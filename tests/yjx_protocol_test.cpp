// Unit tests for YJX generic mouse protocol (0x0E / 0x0F / 0x21) and multi-driver router.
// Pure C++17, no Win32 dependencies, no hardware required.
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

#include "../native/src/bridge.h"
#include "../native/src/driver_router.h"
#include "../native/src/drivers/builtin_caps.h"
#include "../native/src/drivers/registry.h"
#include "../native/src/json.h"
#include "../native/src/yjx_protocol.h"

namespace {

int g_failures = 0;

void Expect(bool condition, const char* what) {
    if (!condition) {
        std::printf("FAIL: %s\n", what);
        ++g_failures;
    }
}

void BuildSample0EReply(std::uint8_t out[yjx::kPacketLen]) {
    std::memset(out, 0, yjx::kPacketLen);
    out[0] = 0x00;  // Windows Report ID
    std::uint8_t* p = out + 1;
    p[0] = 0xAA;
    p[1] = 0x0E;
    p[2] = 0xA5;
    p[3] = 0x0B;
    p[4] = 0x2F;
    p[5] = 0x01;
    p[6] = 0x01;
    p[7] = 0x00;
    p[8] = 0x77;  // preserved pre-config byte
    p[9] = 2;     // light_mode: neon
    p[10] = 4;    // report_rate: 1000 Hz (3 + 1)
    p[11] = 5;    // dpi_count: 5
    p[12] = 3;    // dpi_active: slot 2 (2 + 1)

    const std::uint16_t dpis[6] = {800, 1200, 1600, 3200, 5000, 12000};
    for (int i = 0; i < 6; ++i) {
        p[13 + i * 2] = static_cast<std::uint8_t>(dpis[i] & 0xFFu);
        p[14 + i * 2] = static_cast<std::uint8_t>((dpis[i] >> 8) & 0xFFu);
    }

    // Populate distinct sentinel values in unknown/reserved ranges payload[25..47] & payload[53..63].
    for (std::size_t i = 25; i <= 47; ++i) {
        p[i] = static_cast<std::uint8_t>(0x30u + i);
    }
    p[48] = 1;     // scroll_flag: reverse
    p[49] = 1;     // lod_value: 1.0 mm
    p[50] = 0x35;  // sensor_flag: ripple(0x01) + unknown bit2(0x04) + fixline(0x10) + motion(0x20)
    p[51] = 4;     // key_respond: 4 ms
    p[52] = 10;    // sleep_light: 10 min (600 s)
    for (std::size_t i = 53; i < yjx::kPayloadLen; ++i) {
        p[i] = static_cast<std::uint8_t>(0x80u + i);
    }
}

}  // namespace

int main() {
    // 1. Validators & wire mappings
    Expect(yjx::IsYjxDevice(0xA8A4, 0x2255), "0xA8A4:0x2255 matches YJX USB");
    Expect(yjx::IsYjxDevice(0xA8A5, 0x2255), "0xA8A5:0x2255 matches YJX 2.4G");
    Expect(!yjx::IsYjxDevice(0x093A, 0x522C), "Incott VID/PID rejected by YJX");
    Expect(yjx::IsDongleVid(0xA8A5) && !yjx::IsDongleVid(0xA8A4), "dongle VID check");

    Expect(yjx::DpiValid(200) && yjx::DpiValid(1600) && yjx::DpiValid(12000),
           "valid YJX DPI values");
    Expect(!yjx::DpiValid(100) && !yjx::DpiValid(12100) && !yjx::DpiValid(850),
           "invalid YJX DPI values (bounds & step 100)");
    Expect(yjx::DpiStageValid(5, 4) && !yjx::DpiStageValid(5, 5) && !yjx::DpiStageValid(0, 0),
           "DpiStageValid bounds");

    Expect(yjx::RateToWire(125) == 1 && yjx::RateToWire(250) == 2 &&
               yjx::RateToWire(500) == 3 && yjx::RateToWire(1000) == 4 &&
               yjx::RateToWire(2000) == -1,
           "RateToWire");
    Expect(yjx::RateFromWire(1) == 125 && yjx::RateFromWire(4) == 1000 &&
               yjx::RateFromWire(5) == -1,
           "RateFromWire");
    Expect(yjx::LodBridgeToWire(0) == 1 && yjx::LodBridgeToWire(1) == 2 &&
               yjx::LodBridgeToWire(2) == -1,
           "LodBridgeToWire");
    Expect(yjx::LodWireToBridge(1) == 0 && yjx::LodWireToBridge(2) == 1 &&
               yjx::LodWireToBridge(0) == -1,
           "LodWireToBridge");
    Expect(yjx::SleepSecToMin(60) == 1 && yjx::SleepSecToMin(600) == 10 &&
               yjx::SleepSecToMin(3600) == 60 && yjx::SleepSecToMin(900) == -1,
           "SleepSecToMin");
    Expect(yjx::SleepMinToSec(10) == 600 && yjx::SleepMinToSec(0) == -1 &&
               yjx::SleepMinToSec(255) == -1 && yjx::SleepMinToSec(15) == -1,
           "SleepMinToSec");

    // 2. 0x0E packet build & reply parse
    std::uint8_t read_req[yjx::kPacketLen]{};
    yjx::BuildReadConfigPacket(read_req);
    Expect(read_req[0] == 0x00 && read_req[1] == 0x55 && read_req[2] == 0x0E &&
               read_req[3] == 0xA5 && read_req[4] == 0x0B && read_req[5] == 0x2F &&
               read_req[6] == 0x01 && read_req[7] == 0x01 && read_req[8] == 0x00,
           "BuildReadConfigPacket header layout");

    std::uint8_t reply[yjx::kPacketLen]{};
    BuildSample0EReply(reply);
    yjx::Config cfg;
    yjx::RawConfigFrame raw;
    Expect(yjx::ParseConfigReply(reply, &cfg, &raw), "ParseConfigReply valid sample");
    Expect(cfg.light_mode == 2 && cfg.rate_hz == 1000 && cfg.dpi_count == 5 &&
               cfg.dpi_active == 2 && cfg.dpi[0] == 800 && cfg.dpi[5] == 12000 &&
               cfg.scroll_reverse && cfg.lod_wire == 1 && cfg.sensor_flag == 0x35 &&
               cfg.ripple && cfg.fixline && cfg.motion && cfg.debounce_ms == 4 &&
               cfg.sleep_min == 10,
           "ParseConfigReply decoded fields match");

    // Bad header / zeroed / 0xFF / bad dpi_count rejected
    {
        std::uint8_t bad[yjx::kPacketLen]{};
        BuildSample0EReply(bad);
        bad[2] = 0x30;
        Expect(!yjx::ParseConfigReply(bad, &cfg, nullptr), "wrong opcode rejected");

        BuildSample0EReply(bad);
        bad[14] = bad[15] = bad[16] = 0x00;
        Expect(!yjx::ParseConfigReply(bad, &cfg, nullptr), "all-0x00 DPI bytes rejected");

        BuildSample0EReply(bad);
        bad[14] = bad[15] = bad[16] = 0xFF;
        Expect(!yjx::ParseConfigReply(bad, &cfg, nullptr), "all-0xFF DPI bytes rejected");

        BuildSample0EReply(bad);
        bad[12] = 0;  // payload[11] = dpi_count
        Expect(!yjx::ParseConfigReply(bad, &cfg, nullptr), "dpi_count=0 rejected");
    }

    // 3. 0x0F read-modify-write preserves all unknown/reserved bytes & unknown sensor bits
    {
        yjx::ConfigPatch patch;
        patch.have_dpi[1] = true;
        patch.dpi[1] = 2400;
        patch.have_rate_hz = true;
        patch.rate_hz = 500;
        patch.have_fixline = true;
        patch.fixline = false;  // clear bit4 (0x10) while preserving bit2 (0x04), bit0, bit5
        patch.have_scroll_reverse = true;
        patch.scroll_reverse = false;
        patch.have_sleep_min = true;
        patch.sleep_min = 30;

        std::uint8_t write_pkt[yjx::kPacketLen]{};
        yjx::BuildWriteConfigPacket(raw, patch, write_pkt);

        Expect(write_pkt[0] == 0x00 && write_pkt[1] == 0x55 && write_pkt[2] == 0x0F &&
                   write_pkt[3] == 0xAE && write_pkt[4] == 0x0A && write_pkt[5] == 0x2F &&
                   write_pkt[6] == 0x01 && write_pkt[7] == 0x01 && write_pkt[8] == 0x00,
               "0x0F header layout");
        Expect(write_pkt[9] == 0x77, "payload[8] preserved in out[9]");
        Expect(write_pkt[10] == 2, "unpatched light_mode preserved");
        Expect(write_pkt[11] == 3, "patched rate 500 Hz -> wire 3");
        Expect(write_pkt[12] == 5 && write_pkt[13] == 3, "unpatched dpi_count/active preserved");
        Expect(write_pkt[14] == (800 & 0xFF) && write_pkt[15] == (800 >> 8),
               "unpatched slot 0 DPI preserved");
        Expect(write_pkt[16] == (2400 & 0xFF) && write_pkt[17] == (2400 >> 8),
               "patched slot 1 DPI updated");
        Expect(write_pkt[49] == 0, "patched scroll_reverse=false");
        Expect(write_pkt[50] == 1, "unpatched lod_wire preserved");
        Expect(write_pkt[51] == 0x25,
               "sensor_flag bit4 cleared while preserving bit2(0x04), bit0(0x01), bit5(0x20)");
        Expect(write_pkt[52] == 4, "unpatched debounce preserved");
        Expect(write_pkt[53] == 30, "patched sleep_min=30");

        bool reserved_ok = true;
        for (std::size_t i = 25; i <= 47; ++i) {
            if (write_pkt[i + 1] != raw.payload[i]) reserved_ok = false;
        }
        for (std::size_t i = 53; i < yjx::kPayloadLen; ++i) {
            if (write_pkt[i + 1] != raw.payload[i]) reserved_ok = false;
        }
        Expect(reserved_ok, "all reserved payload[25..47] & payload[53..63] bytes preserved verbatim");

        // Simulated readback round-trip through ParseConfigReply & ConfigMatchesPatch
        std::uint8_t echo_0e[yjx::kPacketLen]{};
        echo_0e[0] = 0x00;
        std::memcpy(echo_0e + 1, write_pkt + 1, yjx::kPayloadLen);
        echo_0e[1] = 0xAA;
        echo_0e[2] = 0x0E;
        yjx::Config after;
        Expect(yjx::ParseConfigReply(echo_0e, &after, nullptr), "simulated 0x0E readback parses");
        Expect(yjx::ConfigMatchesPatch(after, patch), "ConfigMatchesPatch succeeds on matching reply");

        yjx::ConfigPatch mismatch_patch = patch;
        mismatch_patch.rate_hz = 1000;
        Expect(!yjx::ConfigMatchesPatch(after, mismatch_patch),
               "ConfigMatchesPatch detects mismatch");
    }

    // 4. 0x21 light mode packet
    {
        std::uint8_t light_pkt[yjx::kPacketLen]{};
        yjx::BuildLightModePacket(6, light_pkt);
        Expect(light_pkt[0] == 0x00 && light_pkt[1] == 0x55 && light_pkt[2] == 0x21 &&
                   light_pkt[5] == 0x03 && light_pkt[11] == 6,
               "BuildLightModePacket layout");
    }

    // 5. driver registry routing and capability JSON
    {
        drivers::Registry reg;
        reg.Add(drivers::IncottDescriptor());
        reg.Add(drivers::YjxDescriptor());
        reg.Add(drivers::RoyuanDescriptor());
        reg.SetDefault("incott");

        Expect(reg.FindByKey("") != nullptr && reg.FindByKey("")->name == "incott",
               "empty key -> incott");
        Expect(reg.FindByKey("incott:522C") != nullptr &&
                   reg.FindByKey("incott:522C")->name == "incott",
               "incott key -> incott");
        Expect(reg.FindByKey("ksnake:A8A5:2255") != nullptr &&
                   reg.FindByKey("ksnake:A8A5:2255")->name == "yjx",
               "ksnake key -> yjx");
        Expect(reg.FindByKey("royuan:3151:4015") != nullptr &&
                   reg.FindByKey("royuan:3151:4015")->name == "royuan",
               "royuan key -> royuan");
        Expect(reg.FindByKey("bt:1122") == nullptr, "bt key -> nullptr");

        std::uint16_t vid = 0;
        Expect(driver_router::ParseYjxVidFromKey("ksnake:A8A5:2255", &vid) && vid == 0xA8A5u,
               "ParseYjxVidFromKey 0xA8A5");
        Expect(driver_router::ParseYjxVidFromKey("ksnake:a8a4:2255", &vid) && vid == 0xA8A4u,
               "ParseYjxVidFromKey lowercase 0xA8A4");

        bridge::DeviceSnapshot yjx_snap;
        yjx_snap.key = "ksnake:A8A5:2255";
        yjx_snap.name = "Wireless Mouse";
        yjx_snap.kind = "mouse";
        yjx_snap.mode = "wireless";
        yjx_snap.driver_level = "full";

        const std::string yjx_info =
            drivers::BuildDeviceInfoJson(reg, "ksnake:A8A5:2255", &yjx_snap);
        json::Value doc;
        std::string err;
        Expect(json::Parse(yjx_info, &doc, &err) && doc.type == json::Value::Type::kObject,
               "YJX device.info JSON parses");
        std::string drv;
        Expect(doc.GetString("driver", &drv) && drv == "yjx", "YJX driver == yjx");
        const json::Value* caps = doc.Find("capabilities");
        Expect(caps != nullptr && caps->type == json::Value::Type::kObject, "YJX has capabilities");
        if (caps != nullptr) {
            bool keys_cap = false;
            bool macros_cap = false;
            Expect(caps->GetBool("keys", &keys_cap) && keys_cap, "YJX keys == true");
            Expect(caps->GetBool("macros", &macros_cap) && macros_cap, "YJX macros == true");
            const json::Value* dpi = caps->Find("dpi");
            long long dpi_min = 0, dpi_max = 0, dpi_step = 0;
            bool xy_split = true;
            Expect(dpi != nullptr && dpi->GetInt("min", &dpi_min) && dpi_min == 200 &&
                       dpi->GetInt("max", &dpi_max) && dpi_max == 12000 &&
                       dpi->GetInt("step", &dpi_step) && dpi_step == 100 &&
                       dpi->GetBool("xySplit", &xy_split) && !xy_split,
                   "YJX DPI capability bounds");
            const json::Value* rates = caps->Find("rates");
            Expect(rates != nullptr && rates->type == json::Value::Type::kArray &&
                       rates->array.size() == 4,
                   "YJX 2.4G rates has 4 entries (125..1000)");
            const json::Value* misc = caps->Find("misc");
            bool scroll_rev = false;
            Expect(misc != nullptr && misc->GetBool("scrollReverse", &scroll_rev) && scroll_rev,
                   "YJX scrollReverse supported");
        }

        bridge::DeviceSnapshot incott_snap;
        incott_snap.key = "incott:522C";
        incott_snap.kind = "mouse";
        incott_snap.mode = "wireless";
        incott_snap.receiver = "8K";
        incott_snap.driver_level = "full";
        const std::string incott_info =
            drivers::BuildDeviceInfoJson(reg, "incott:522C", &incott_snap);
        json::Value incott_doc;
        Expect(json::Parse(incott_info, &incott_doc, &err), "Incott device.info parses");
        const json::Value* incott_caps = incott_doc.Find("capabilities");
        const json::Value* incott_rates =
            incott_caps != nullptr ? incott_caps->Find("rates") : nullptr;
        Expect(incott_rates != nullptr && incott_rates->array.size() == 7,
               "Incott 8K wireless rates has 7 entries");
    }

    // 6. Phase 2: 0x08 / 0x09 keys read-modify-write & binding codecs
    {
        std::uint8_t rk[yjx::kPacketLen]{};
        yjx::BuildReadKeysPacket(rk);
        Expect(rk[0] == 0x00 && rk[1] == 0x55 && rk[2] == 0x08 && rk[3] == 0xA5 &&
                   rk[4] == 0x0B && rk[5] == 0x20,
               "BuildReadKeysPacket layout");

        std::uint8_t rep[yjx::kPacketLen]{};
        rep[0] = 0x00;
        rep[1] = 0xAA;
        rep[2] = 0x08;
        rep[3] = 0xA5;
        rep[4] = 0x0B;
        rep[5] = 0x20;
        // Wire slot 0..5 -> payload[8..31], slots 6..7 (scroll up/down) -> payload[32..39]
        std::uint8_t* p = rep + 1;
        p[8] = 32;  p[9] = 1;    // wire 0 = Left
        p[12] = 32; p[13] = 2;   // wire 1 = Right
        p[16] = 32; p[17] = 4;   // wire 2 = Middle
        p[20] = 32; p[21] = 8;   // wire 3 = Back (bridge index 4)
        p[24] = 32; p[25] = 16;  // wire 4 = Forward (bridge index 3)
        p[28] = 33; p[29] = 85;  // wire 5 = DPI Cycle
        p[32] = 33; p[33] = 56; p[34] = 1;    // scroll up (wire 6)
        p[36] = 33; p[37] = 56; p[38] = 255;  // scroll down (wire 7)
        for (std::size_t i = 40; i < yjx::kPayloadLen; ++i) {
            p[i] = static_cast<std::uint8_t>(0xA0u + (i & 0x3Fu));
        }

        yjx::KeyCode parsed[yjx::kKeyCount]{};
        yjx::RawKeysFrame raw_keys;
        Expect(yjx::ParseKeysReply(rep, parsed, &raw_keys), "ParseKeysReply valid 0x08");
        Expect(parsed[0].type == 32 && parsed[0].code1 == 1, "index 0 Left");
        Expect(parsed[3].type == 32 && parsed[3].code1 == 16, "index 3 Forward from wire slot 4");
        Expect(parsed[4].type == 32 && parsed[4].code1 == 8, "index 4 Back from wire slot 3");
        Expect(parsed[5].type == 33 && parsed[5].code1 == 85, "index 5 DPI Cycle");

        // Patch bridge index 4 (Back -> wire slot 3 -> out[9 + 12..15] = out[21..24])
        yjx::KeyCode ctrl_s{};
        Expect(yjx::EncodeBinding("Ctrl+Shift+KeyS", &ctrl_s), "encode Ctrl+Shift+KeyS");
        Expect(ctrl_s.type == 16 && ctrl_s.code1 == 0x03 && ctrl_s.code2 == 0x16 && ctrl_s.code3 == 0,
               "Ctrl+Shift+KeyS wire bytes");

        std::uint8_t wk[yjx::kPacketLen]{};
        yjx::BuildWriteKeysPacket(raw_keys, 4, ctrl_s, wk);
        Expect(wk[0] == 0x00 && wk[1] == 0x55 && wk[2] == 0x09 && wk[3] == 0xA5 &&
                   wk[4] == 0x22 && wk[5] == 0x20,
               "0x09 header layout");
        Expect(wk[21] == 16 && wk[22] == 0x03 && wk[23] == 0x16 && wk[24] == 0,
               "patched wire slot 3 (bridge index 4) in out[21..24]");
        Expect(wk[25] == 32 && wk[26] == 16, "unpatched wire slot 4 (Forward) preserved");
        Expect(wk[33] == 33 && wk[34] == 56 && wk[35] == 1 &&
                   wk[37] == 33 && wk[38] == 56 && wk[39] == 255,
               "scroll up/down slots preserved");
        bool keys_tail_ok = true;
        for (std::size_t i = 40; i < yjx::kPayloadLen; ++i) {
            if (wk[i + 1] != raw_keys.payload[i]) keys_tail_ok = false;
        }
        Expect(keys_tail_ok, "0x09 trailing reserved bytes preserved");

        // Binding encode/decode round-trips & rejections
        const char* kRoundTrips[] = {
            "Off",
            "Mouse:Left",
            "Mouse:Right",
            "Mouse:Middle",
            "Mouse:Back",
            "Mouse:Forward",
            "Dpi:Cycle",
            "Media:VolUp",
            "Media:Search",
            "Media:PlayPause",
            "Ctrl+Shift+KeyS",
            "Alt+F4",
            "Escape",
        };
        for (const char* name : kRoundTrips) {
            yjx::KeyCode kc{};
            std::string b, k;
            Expect(yjx::EncodeBinding(name, &kc), "EncodeBinding succeeds");
            yjx::DecodeBinding(kc, &b, &k);
            Expect(b == name, "DecodeBinding round-trips exact name");
        }
        {
            std::string b, k;
            yjx::DecodeBinding(yjx::KeyCode{112, 3, 1, 2}, &b, &k);
            Expect(b == "Macro:3" && k == "macro", "DecodeBinding type 112 -> Macro:3");
            yjx::DecodeBindingForSlot(4, yjx::KeyCode{0, 0, 0, 0}, &b, &k);
            Expect(b == "Mouse:Back" && k == "mouse", "type 0 slot 4 defaults to Mouse:Back");
            yjx::DecodeBindingForSlot(5, yjx::KeyCode{255, 255, 255, 255}, &b, &k);
            Expect(b == "Dpi:Cycle" && k == "dpi", "type 255 slot 5 defaults to Dpi:Cycle");
        }
        yjx::KeyCode dummy{};
        Expect(!yjx::EncodeBinding("Fire", &dummy), "Fire rejected by YJX");
        Expect(!yjx::EncodeBinding("Profile", &dummy), "Profile rejected by YJX");
        Expect(!yjx::EncodeBinding("Dpi:Up", &dummy), "Dpi:Up rejected by YJX");
        Expect(!yjx::EncodeBinding("Dpi:Down", &dummy), "Dpi:Down rejected by YJX");
        Expect(!yjx::EncodeBinding("Macro:0", &dummy), "Macro:0 rejected in keys.set");
    }

    // 7. Phase 2: 4096B onboard macro table, 0x0D/0x10 packets, MakeMacroKeyCode
    {
        yjx::MacroSlotData slots[yjx::kMacroSlotCount]{};
        slots[2].steps.push_back(yjx::MacroStep{false, true, 0x04, 15});    // KeyA down, 15 ms
        slots[2].steps.push_back(yjx::MacroStep{true, false, 1, 300});     // Mouse Left up, 300 ms

        std::vector<std::uint8_t> table;
        std::string err;
        Expect(yjx::EncodeMacroTable(slots, &table, &err), "EncodeMacroTable valid");
        Expect(table.size() == 64u + 8u, "table exact size 64 header + 8 bytes");
        Expect(table[0] == 0 && table[1] == 0, "empty slot 0 offset == 0");
        Expect(table[4] == 64 && table[5] == 0, "slot 2 starts at offset 64");
        // Step 0: delay=15 (0x0F,0x00), flags=0x40|0x02=0x42, code=0x04
        Expect(table[64] == 15 && table[65] == 0 && table[66] == 0x42 && table[67] == 0x04,
               "step 0 encoded bytes");
        // Step 1: delay=300 (0x2C,0x01), flags=0x80|0x03=0x83, code=1
        Expect(table[68] == 0x2C && table[69] == 0x01 && table[70] == 0x83 && table[71] == 1,
               "step 1 last-item + mouse flags");

        // Overflow > 4096 bytes rejected
        yjx::MacroSlotData too_big[yjx::kMacroSlotCount]{};
        too_big[0].steps.resize(1010, yjx::MacroStep{false, true, 0x04, 10});  // 64 + 4040 > 4096
        Expect(!yjx::EncodeMacroTable(too_big, &table, &err), "table > 4096 B rejected");

        std::uint8_t chunk[yjx::kPacketLen]{};
        const std::uint8_t sample[4] = {1, 2, 3, 4};
        yjx::BuildMacroChunkPacket(112, sample, 4, chunk);
        Expect(chunk[0] == 0x00 && chunk[1] == 0x55 && chunk[2] == 0x0D && chunk[5] == 4 &&
                   chunk[6] == 112 && chunk[7] == 0 && chunk[9] == 1 && chunk[12] == 4,
               "BuildMacroChunkPacket layout");

        std::uint8_t commit[yjx::kPacketLen]{};
        yjx::BuildMacroCommitPacket(commit);
        Expect(commit[0] == 0x00 && commit[1] == 0x55 && commit[2] == 0x10 &&
                   commit[3] == 0xA5 && commit[4] == 0x22 && commit[8] == 0x05,
               "BuildMacroCommitPacket layout");

        const yjx::KeyCode m1 = yjx::MakeMacroKeyCode(2, 1, 5);
        const yjx::KeyCode m2 = yjx::MakeMacroKeyCode(2, 2, 5);
        const yjx::KeyCode m3 = yjx::MakeMacroKeyCode(2, 3, 7);
        Expect(m1.type == 112 && m1.code1 == 2 && m1.code2 == 1 && m1.code3 == 2,
               "loopType 1 (hold) -> code3=2");
        Expect(m2.type == 112 && m2.code1 == 2 && m2.code2 == 1 && m2.code3 == 3,
               "loopType 2 (until any key) -> code3=3");
        Expect(m3.type == 112 && m3.code1 == 2 && m3.code2 == 7 && m3.code3 == 0,
               "loopType 3 (cycles=7) -> code3=0, code2=7");
    }

    if (g_failures == 0) {
        std::printf(
            "yjx_protocol_test: OK (0x0E/0x0F/0x21, 0x08/0x09 keys, 0x0D/0x10 macros, "
            "driver_router)\n");
        return 0;
    }
    std::printf("yjx_protocol_test: %d failure(s)\n", g_failures);
    return 1;
}
