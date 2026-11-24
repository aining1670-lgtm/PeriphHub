// Pure-function tests: no hardware, no Win32 UI.
#include <cstdio>
#include <cstring>

#include "../native/src/protocol.h"

namespace {

int g_failures = 0;

void Expect(bool condition, const char* what) {
    if (!condition) {
        std::printf("FAIL: %s\n", what);
        ++g_failures;
    }
}

}  // namespace

int main() {
    using namespace incott;

    const Frame read_status = MakeRead(kCmdStatus, nullptr, 0);
    Expect(read_status.bytes[0] == 0x09, "report id");
    Expect(read_status.bytes[1] == 0x89, "read flag applied");
    Expect(read_status.bytes[2] == 0x00, "payload zeroed");

    const std::uint8_t dpi[4] = {0x01, 0x40, 0x06, 0x00};
    const Frame write_dpi = MakeWrite(kCmdDpi, dpi, 4);
    Expect(write_dpi.bytes[1] == 0x02, "write keeps cmd");
    Expect(write_dpi.bytes[2] == 0x01 && write_dpi.bytes[3] == 0x40, "payload copied");

    Frame reply = MakeRead(kCmdStatus, nullptr, 0);
    reply.bytes[2] = 32;
    Expect(CheckEcho(reply, kCmdStatus), "echo matches");
    Expect(BatteryPercent(reply) == 32, "battery percent");
    reply.bytes[2] = 200;
    Expect(BatteryPercent(reply) == -1, "percent out of range rejected");

    Expect(kMacroActionLimit == 71 && kMacroCycleLimit == 65535, "official macro caps");

    // Real G23 2.4G reply: 09 8F 01 02 01 00 F0 00 00.
    Frame model{};
    model.bytes[0] = kReportId;
    model.bytes[1] = kCmdModel;
    model.bytes[2] = 0x01;  // psd
    model.bytes[3] = 0x02;  // G23
    model.bytes[4] = 0x01;  // 1K receiver
    model.bytes[5] = 0x00;  // wired sensor byte
    model.bytes[6] = 0xF0;  // wireless sensor byte: PAW3395
    const DeviceInfo wireless = ParseModel(model, true);
    Expect(wireless.model == 2 && wireless.psd == 1 && wireless.receiver == 1, "0x8F fields");
    Expect(wireless.sensor == 3395, "wireless sensor byte");
    const DeviceInfo wired = ParseModel(model, false);
    Expect(wired.sensor == 0, "wired sensor byte is empty");

    const Frame header = MakeMacroChunkHeader(3, 6);
    Expect(header.bytes[1] == 0x07, "macro cmd");
    Expect(header.bytes[2] == 10, "macro chunk count");
    Expect(header.bytes[3] == 3, "macro chunk index");
    Expect(header.bytes[4] == 32, "macro chunk bytes");
    Expect(header.bytes[5] == 6, "macro slot");

    const std::uint8_t too_long[16] = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16};
    const Frame clipped = MakeWrite(kCmdDpi, too_long, sizeof(too_long));
    Expect(clipped.bytes[2] == 1 && clipped.bytes[8] == 7, "payload clipped to 9-byte report");

    // DPI encoding: wire value = dpi/50 - 1.
    Expect(DpiValid(50) && DpiValid(1600) && DpiValid(32000), "valid dpi values");
    Expect(!DpiValid(0) && !DpiValid(49) && !DpiValid(51) && !DpiValid(32050), "invalid dpi values");
    Expect(DpiToHw(1600) == 31, "dpi to hardware units");
    Expect(DpiFromHw(0) == 50 && DpiFromHw(31) == 1600 && DpiFromHw(639) == 32000, "hardware to dpi");

    const DpiWrite same = MakeDpiWrite(2, 31, 31);
    Expect(same.count == 1, "equal axes need one frame");
    Expect(same.frames[0].bytes[1] == 0x02 && same.frames[0].bytes[2] == 2, "dpi frame slot");
    Expect(same.frames[0].bytes[3] == 31 && same.frames[0].bytes[4] == 0, "dpi frame little endian");
    Expect(same.frames[0].bytes[8] == 0, "dpi axis both");

    const DpiWrite split = MakeDpiWrite(1, 31, 63);
    Expect(split.count == 2, "different axes need two frames");
    Expect(split.frames[0].bytes[8] == 1 && split.frames[0].bytes[3] == 31, "dpi axis x");
    Expect(split.frames[1].bytes[8] == 2 && split.frames[1].bytes[3] == 63, "dpi axis y");

    // WP3 measured shape (docs/05 §11): 0x82 exposes only the LAST written
    // frame — a split write (X then Y) can only verify Y; a single axis=0
    // frame verifies the shared value.
    Expect(DpiExpectedReadback(split, 1600, 3200) == 3200, "split write readback target is Y");
    Expect(DpiExpectedReadback(same, 1600, 1600) == 1600, "single-frame write readback target is the shared value");

    // WP3: dpi.stage args are rejected, never silently rewritten to 6/0.
    Expect(DpiStageValid(6, 2) && DpiStageValid(1, 0) && DpiStageValid(6, 5), "valid dpi.stage args");
    Expect(!DpiStageValid(0, 0) && !DpiStageValid(7, 0) && !DpiStageValid(-1, 0), "count outside 1..6 rejected");
    Expect(!DpiStageValid(6, 6) && !DpiStageValid(3, 3) && !DpiStageValid(6, -1), "active outside 0..count-1 rejected");

    Frame dpi_reply{};
    dpi_reply.bytes[0] = kReportId;
    dpi_reply.bytes[1] = 0x82;
    dpi_reply.bytes[2] = 2;
    dpi_reply.bytes[3] = 0x1F;
    Expect(DpiFromReply(dpi_reply, 2) == 1600, "0x82 reply decodes");
    dpi_reply.bytes[1] = 0x02;
    Expect(DpiFromReply(dpi_reply, 2) == -1, "0x82 echo mismatch rejected");
    dpi_reply.bytes[1] = 0x82;
    // WP3 hardening: bytes[2] must echo the requested slot (stale-frame guard,
    // same rule as the 0x84/0x85 sub reads).
    Expect(DpiFromReply(dpi_reply, 5) == -1, "0x82 slot mismatch rejected");
    Expect(DpiFromReply(dpi_reply, 2) == 1600, "0x82 slot echo accepted");
    dpi_reply.bytes[3] = 0x80;
    dpi_reply.bytes[4] = 0x02;
    Expect(DpiFromReply(dpi_reply, 2) == -1, "unmapped hardware value rejected");

    // Live G23 2.4G reply: 09 83 06 02 00 00 00 00 00 -> 6 levels, active level 2.
    Frame table_frame{};
    table_frame.bytes[0] = kReportId;
    table_frame.bytes[1] = 0x83;
    table_frame.bytes[2] = 6;
    table_frame.bytes[3] = 2;
    const DpiTable table = ParseDpiTable(table_frame);
    Expect(table.count == 6 && table.active == 2, "0x83 dpi table");
    // WP5 acceptance: a light-sleep 0x83 read comes back empty/all-zero; the
    // rate.* wake probe (main.cpp DeviceAwake) treats count < 1 as asleep.
    Frame asleep_table{};
    Expect(ParseDpiTable(asleep_table).count == -1, "empty 0x83 (no echo) parses as asleep");
    asleep_table.bytes[0] = kReportId;
    asleep_table.bytes[1] = 0x83;
    Expect(ParseDpiTable(asleep_table).count == 0, "echoed empty 0x83 (count 0) parses as asleep");

    // Real G23 2.4G status reply: 09 89 20 01 00 08 00 00 5A.
    Frame status{};
    status.bytes[0] = kReportId;
    status.bytes[1] = 0x89;
    status.bytes[2] = 0x20;
    status.bytes[3] = 0x01;
    status.bytes[4] = 0x00;
    status.bytes[5] = 0x08;
    status.bytes[8] = 0x5A;
    const StatusInfo info = ParseStatus(status);
    Expect(info.battery == 32 && info.charging == 1 && info.rate_code == 0 && info.dpi_index == 8,
           "0x89 status fields");
    Expect(RateFromCode(0) == 1000 && RateFromCode(4) == 8000 && RateFromCode(6) == 2000,
           "rate codes");
    Expect(RateFromCode(-1) == -1 && RateFromCode(7) == -1, "rate code out of range");

    Frame misc{};
    misc.bytes[0] = kReportId;
    misc.bytes[1] = 0x85;
    misc.bytes[2] = 0x03;
    misc.bytes[3] = 0x3C;
    Expect(SleepSeconds(misc, 0x03) == 60, "sleep seconds little endian");
    Expect(SleepSeconds(misc, 0x01) == -1, "sleep reply rejected on sub mismatch");
    misc.bytes[2] = 0x01;
    misc.bytes[3] = 8;
    Expect(DebounceValue(misc, 0x01) == 8, "debounce value");
    Expect(DebounceValue(misc, 0x03) == -1, "debounce reply rejected on sub mismatch");
    misc.bytes[1] = 0x00;
    Expect(DebounceValue(misc, 0x01) == -1 && SleepSeconds(misc, 0x03) == -1,
           "misc replies rejected without echo");
    misc.bytes[1] = kCmdMisc | kReadFlag;
    misc.bytes[2] = 0x01;
    misc.bytes[3] = 31;
    Expect(DebounceValue(misc, 0x01) == -1, "debounce >30 rejected");
    misc.bytes[2] = 0x03;
    misc.bytes[3] = 5;
    misc.bytes[4] = 0;
    Expect(SleepSeconds(misc, 0x03) == -1, "sleep <10 rejected");
    misc.bytes[3] = 0xB6;  // 950 s = 0x03B6
    misc.bytes[4] = 0x03;
    Expect(SleepSeconds(misc, 0x03) == -1, "sleep >900 rejected");

    // ---- WP5: sensor sub replies, official ranges, misc/rate bounds ----
    Expect(RateFromCode(7) == -1 && RateFromCode(-1) == -1, "unmapped rate code rejected");
    Expect(kDebounceMsMax == 30 && kSleepSecondsMin == 10 && kSleepSecondsMax == 900,
           "WP5 misc bounds (sleep 900 = measured firmware limit, docs/05 §10)");
    Expect(!HighRateAllowed(1, true) && !HighRateAllowed(0, true),
           "1K/unknown receiver rejects >1000 Hz");
    Expect(!HighRateAllowed(2, false), "8K receiver on wired USB rejects >1000 Hz");
    Expect(HighRateAllowed(2, true), "8K receiver + 2.4G wireless allows >1000 Hz");

    // Real G23 2.4G 0x84 sub5 reply: 09 84 05 01 00 00 00 00 00 -> mode = 1.
    Frame sensor_reply{};
    sensor_reply.bytes[0] = kReportId;
    sensor_reply.bytes[1] = kCmdSensor | kReadFlag;
    sensor_reply.bytes[2] = 5;
    sensor_reply.bytes[3] = 1;
    Expect(SensorSubValue(sensor_reply, 5) == 1, "0x84 sub5 value");
    Expect(SensorSubValue(sensor_reply, 4) == -1, "0x84 stale sub rejected");
    sensor_reply.bytes[1] = 0x00;
    Expect(SensorSubValue(sensor_reply, 5) == -1, "0x84 reply rejected without echo");
    sensor_reply.bytes[1] = kCmdSensor | kReadFlag;
    sensor_reply.bytes[2] = 1;
    sensor_reply.bytes[3] = 2;
    Expect(SensorSubValue(sensor_reply, 1) == 2, "0x84 sub1 lod value");
    sensor_reply.bytes[3] = 3;
    Expect(SensorSubValue(sensor_reply, 1) == -1, "0x84 sub1 lod=3 rejected");
    sensor_reply.bytes[2] = 5;
    sensor_reply.bytes[3] = 3;
    Expect(SensorSubValue(sensor_reply, 5) == -1, "0x84 sub5 mode=3 rejected");
    sensor_reply.bytes[2] = 2;
    sensor_reply.bytes[3] = 255;
    Expect(SensorSubValue(sensor_reply, 2) == -1, "0x84 sub2 ripple=255 rejected");
    sensor_reply.bytes[2] = 3;
    sensor_reply.bytes[3] = 2;
    Expect(SensorSubValue(sensor_reply, 3) == -1, "0x84 sub3 fixline=2 rejected");
    sensor_reply.bytes[2] = 4;
    sensor_reply.bytes[3] = 255;
    Expect(SensorSubValue(sensor_reply, 4) == -1, "0x84 sub4 motion=255 rejected");

    Expect(SensorValueValid(1, 0) && SensorValueValid(1, 1) && SensorValueValid(1, 2) &&
               !SensorValueValid(1, 3) && !SensorValueValid(1, -1),
           "lod range 0..2");
    Expect(SensorValueValid(5, 0) && SensorValueValid(5, 1) && SensorValueValid(5, 2) &&
               !SensorValueValid(5, 3) && !SensorValueValid(5, -1),
           "sensor mode range 0..2");
    Expect(SensorValueValid(2, 0) && SensorValueValid(2, 1) && !SensorValueValid(2, 2) &&
               !SensorValueValid(3, 2) && !SensorValueValid(4, 2),
           "sensor flags range 0..1");
    Expect(!SensorValueValid(6, 0), "unknown sensor sub rejected");

    // ---- P4: key bindings (0x06 write / 0x86 read) ----
    Expect(KeyNameCount() == 108, "108 vendor key names");
    int usage = 0;
    Expect(KeyUsageFromCode("KeyA", &usage) && usage == 0x04, "KeyA usage");
    Expect(KeyCodeFromUsage(0x04) != nullptr && std::strcmp(KeyCodeFromUsage(0x04), "KeyA") == 0,
           "usage maps back to KeyA");
    Expect(!KeyUsageFromCode("Nope", &usage), "unknown key name rejected");
    Expect(KeyLabelFromCode("KeyA") != nullptr && std::strcmp(KeyLabelFromCode("KeyA"), "A") == 0,
           "KeyA label");

    Expect(kKeyMatrix[0] == 0 && kKeyMatrix[4] == 3 && kKeyMatrix[3] == 4,
           "side buttons are swapped on the wire");
    Expect(std::strcmp(KeySlotName(4), "back") == 0 && KeySlotName(6) == nullptr, "slot names");

    const Frame key_read = MakeKeyRead(3);
    Expect(key_read.bytes[0] == kReportId && key_read.bytes[1] == 0x86 && key_read.bytes[2] == 3,
           "0x86 read frame");

    const Frame key_write = MakeKeyWrite(3, 0x00F40001);
    Expect(key_write.bytes[1] == 0x06 && key_write.bytes[2] == 3, "0x06 write frame");
    Expect(key_write.bytes[3] == 0x01 && key_write.bytes[4] == 0x00 && key_write.bytes[5] == 0xF4 &&
               key_write.bytes[6] == 0x00,
           "key code little endian");

    Frame key_reply{};
    key_reply.bytes[0] = kReportId;
    key_reply.bytes[1] = 0x86;
    key_reply.bytes[2] = 3;
    key_reply.bytes[3] = 0x01;
    key_reply.bytes[4] = 0x00;
    key_reply.bytes[5] = 0xF4;
    std::uint32_t key_code = 0;
    Expect(ParseKeyReply(key_reply, 3, &key_code) && key_code == 0x00F40001, "key reply decodes");
    Expect(!ParseKeyReply(key_reply, 4, &key_code), "key reply matrix mismatch rejected");

    struct BindingCase {
        const char* name;
        std::uint32_t code;
    };
    const BindingCase binding_cases[] = {
        {"KeyA", 0x00000480},        {"Ctrl+KeyC", 0x00060100},
        {"Shift+Alt+KeyS", 0x00160600},
        {"LButton", 0x00F00001},     {"RButton", 0x00F10001},
        {"MButton", 0x00F20001},     {"Mouse:Back", 0x00F30001},
        {"Mouse:Forward", 0x00F40001},
        {"Dpi:Up", 0x00010007},      {"Dpi:Down", 0x00020007},
        {"Dpi:Cycle", 0x00030007},   {"Profile", 0x0000F10A},
        {"Fire", 0x0218F00A},        {"Media:PlayPause", 0x00CD0003},
        {"Media:VolUp", 0x00E90003}, {"Media:Search", 0x02210003},
        {"Off", 0x00000000},
    };
    for (const BindingCase& item : binding_cases) {
        std::uint32_t encoded = 1;
        const bool ok = EncodeBinding(item.name, &encoded) && encoded == item.code;
        if (!ok) {
            std::printf("FAIL: EncodeBinding(%s) = 0x%08X, want 0x%08X\n", item.name,
                        static_cast<unsigned>(encoded), static_cast<unsigned>(item.code));
            ++g_failures;
        }
    }
    std::uint32_t rejected = 1;
    Expect(!EncodeBinding("Nope", &rejected), "unknown binding rejected");
    Expect(!EncodeBinding("Ctrl+Nope", &rejected), "unknown modified key rejected");
    Expect(!EncodeBinding("Mouse:Side", &rejected), "unknown mouse action rejected");
    Expect(!EncodeBinding("Media:Rewind", &rejected), "unknown media action rejected");
    Expect(!EncodeBinding("", &rejected), "empty binding rejected");
    // WP2-2: keys.set must not accept Macro:N — macros go through macros.upload,
    // which writes the 320-byte firmware buffer before binding the key.
    Expect(!EncodeBinding("Macro:2", &rejected), "Macro:N rejected (use macros.upload)");
    Expect(!EncodeBinding("Mouse:WheelUp", &rejected), "Mouse:WheelUp rejected (no official code)");
    Expect(!EncodeBinding("Mouse:WheelDown", &rejected), "Mouse:WheelDown rejected (no official code)");
    Expect(!EncodeBinding("Mouse:DoubleLeft", &rejected), "Mouse:DoubleLeft rejected (no official code)");

    Expect(DecodeBinding(0x00000480).kind == KeyKind::kKeyboard &&
               DecodeBinding(0x00000480).usage == 0x04,
           "decode plain keyboard");
    Expect(DecodeBinding(0x00060100).modifiers == 1 && DecodeBinding(0x00060100).usage == 6,
           "decode modified keyboard");
    Expect(DecodeBinding(0x00F30001).kind == KeyKind::kMouse &&
               DecodeBinding(0x00F30001).mouse == 5,
           "decode mouse back");
    Expect(DecodeBinding(0x00030007).kind == KeyKind::kDpi &&
               DecodeBinding(0x00030007).dpi == 3,
           "decode dpi cycle");
    Expect(DecodeBinding(0x00020009).kind == KeyKind::kMacro &&
               DecodeBinding(0x00020009).macro_slot == 2,
           "decode macro slot");
    Expect(DecodeBinding(0x00CD0003).kind == KeyKind::kMedia, "decode media");
    Expect(DecodeBinding(0x0218F00A).kind == KeyKind::kFire, "decode fire");
    Expect(DecodeBinding(0).kind == KeyKind::kOff, "decode off");

    Expect(DescribeBinding(0x00000480) == "KeyA", "describe plain key");
    Expect(DescribeBinding(0x00160600) == "Shift+Alt+KeyS", "describe modified key");
    Expect(DescribeBinding(0x00F30001) == "Mouse:Back", "describe mouse back");
    Expect(DescribeBinding(0x00030007) == "Dpi:Cycle", "describe dpi cycle");
    Expect(DescribeBinding(0x00020009) == "Macro:2", "describe macro");
    Expect(DescribeBinding(0x0218F00A) == "Fire", "describe fire");
    Expect(DescribeBinding(0).find("Off") == 0, "describe off");

    // ---- WP2-1: 0x85 sub2 fire-key reply parsing ----
    {
        incott::Frame fire{};
        fire.bytes[0] = incott::kReportId;
        fire.bytes[1] = incott::kCmdMisc | incott::kReadFlag;
        fire.bytes[2] = 0x02;  // sub echo
        fire.bytes[3] = 2;     // loops
        fire.bytes[4] = 50;    // interval ms
        Expect(incott::FireLoops(fire) == 2 && incott::FireInterval(fire) == 50,
               "fire reply parses loops/interval");
        fire.bytes[1] = 0x00;  // echo broken
        Expect(incott::FireLoops(fire) == -1 && incott::FireInterval(fire) == -1,
               "fire reply rejected without echo");
        fire.bytes[1] = incott::kCmdMisc | incott::kReadFlag;
        fire.bytes[3] = 4;
        Expect(incott::FireLoops(fire) == -1, "fire loops >3 rejected");
    }

    // ---- P5: macro buffer encoding (vendor macro_to_hw) ----
    {
        std::uint8_t buffer[incott::kMacroPayload]{};
        std::vector<incott::MacroAction> actions = {
            {true, 0x04, 30},   // KeyA down
            {false, 0x04, 30},  // KeyA up
            {true, 0x05, 0},    // KeyB down, last item
        };
        Expect(incott::EncodeMacro(4, 3, 1, 0x11223344, actions, buffer) ==
                   incott::MacroError::kOk,
               "encode 3-action macro");
        Expect(buffer[0] == 4, "macro buffer id = key index");
        Expect(buffer[1] == 2, "loop type cycle -> 2");
        Expect(buffer[2] == 1 && buffer[3] == 0, "cycles LE16");
        Expect(buffer[4] == 0x01 && buffer[5] == 0x04 && buffer[6] == 30,
               "first action: down + more");
        Expect(buffer[8] == 0x81 && buffer[9] == 0x04 && buffer[10] == 30,
               "second action: up + more");
        Expect(buffer[12] == 0x00 && buffer[13] == 0x05, "last action: down, no more bit");
        Expect(buffer[0x130] == 0x90, "trailer total = (3+1)*4+0x80");
        Expect(buffer[0x134] == 0x10 && buffer[0x136] == 0xE8 && buffer[0x137] == 0xE8,
               "trailer fixed bytes");
        Expect(buffer[0x138] == 0x44 && buffer[0x13B] == 0x11, "trailer uid LE32");
        Expect(buffer[0x13C] == 6 && buffer[0x13E] == 3, "trailer item count");

        buffer[0] = 0xFF;
        Expect(incott::EncodeMacro(4, 3, 65535, 1, actions, buffer) ==
                   incott::MacroError::kOk,
               "cycles 65535 accepted");
        Expect(buffer[2] == 0xFF && buffer[3] == 0xFF, "cycles 65535 bytes");
        Expect(incott::EncodeMacro(4, 3, 65536, 1, actions, buffer) ==
                   incott::MacroError::kCyclesRange,
               "cycles 65536 rejected");
        Expect(incott::EncodeMacro(10, 3, 1, 1, actions, buffer) ==
                   incott::MacroError::kBufferRange,
               "buffer 10 rejected");
        Expect(incott::EncodeMacro(4, 4, 1, 1, actions, buffer) ==
                   incott::MacroError::kLoopType,
               "loop type 4 rejected");
        Expect(incott::EncodeMacro(4, 1, 1, 1, {{true, 0x04, 0}}, buffer) ==
                   incott::MacroError::kOk,
               "single action ok");
        Expect(incott::EncodeMacro(4, 1, 1, 1, {}, buffer) ==
                   incott::MacroError::kActionCount,
               "empty macro rejected");
        Expect(incott::EncodeMacro(4, 1, 1, 1, {{true, 0x100, 0}}, buffer) ==
                   incott::MacroError::kActionUsage,
               "usage 256 rejected");
        Expect(incott::EncodeMacro(4, 1, 1, 1, {{true, 0x04, 65536}}, buffer) ==
                   incott::MacroError::kActionDelay,
               "delay 65536 rejected");

        std::vector<incott::MacroAction> full;
        for (int i = 0; i < incott::kMacroActionLimit; ++i) full.push_back({true, 0x04, 10});
        Expect(incott::EncodeMacro(5, 1, 1, 7, full, buffer) == incott::MacroError::kOk,
               "71 actions accepted");
        Expect(buffer[4 + 70 * 4] == 0x00, "last of 71 has no more-bit");
        std::vector<incott::MacroAction> tooMany = full;
        tooMany.push_back({true, 0x04, 10});
        Expect(incott::EncodeMacro(5, 1, 1, 7, tooMany, buffer) ==
                   incott::MacroError::kActionCount,
               "72 actions rejected");
    }

    // ---- WP4: 8K receiver LED (0x08/0x88) ----
    {
        Expect(incott::ReceiverLedValid(0) && incott::ReceiverLedValid(1) &&
                   incott::ReceiverLedValid(2),
               "receiver LED modes 0..2 valid");
        Expect(!incott::ReceiverLedValid(-1) && !incott::ReceiverLedValid(3),
               "receiver LED modes outside 0..2 rejected");

        incott::Frame led{};
        led.bytes[0] = incott::kReportId;
        led.bytes[1] = incott::kCmdReceiverLed | incott::kReadFlag;
        led.bytes[2] = 1;
        Expect(incott::ParseReceiverLed(led) == 1, "0x88 receiver LED reply parses mode 1");
        led.bytes[2] = 3;
        Expect(incott::ParseReceiverLed(led) == -1, "0x88 receiver LED rejects mode 3");
        led.bytes[1] = 0x89;
        led.bytes[2] = 1;
        Expect(incott::ParseReceiverLed(led) == -1, "0x88 receiver LED requires echo");
    }

    if (g_failures == 0) {
        std::printf("protocol_test: OK (frames, echo, battery, macro, keys, clipping, sensor/misc/rate bounds, led)\n");
        return 0;
    }
    std::printf("protocol_test: %d failure(s)\n", g_failures);
    return 1;
}
