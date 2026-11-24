// Unit tests for ROYUAN (yc500) AKOS BC98 keyboard protocol and driver_router routing.
// Pure C++17, no Win32 dependencies, no hardware required.
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>

#include "../native/src/bridge.h"
#include "../native/src/driver_router.h"
#include "../native/src/drivers/builtin_caps.h"
#include "../native/src/drivers/registry.h"
#include "../native/src/json.h"
#include "../native/src/royuan_protocol.h"

namespace {

int g_failures = 0;

void Expect(bool condition, const char* what) {
    if (!condition) {
        std::printf("FAIL: %s\n", what);
        ++g_failures;
    }
}

void ExpectEqual(const std::string& actual, const std::string& expected, const char* what) {
    if (actual == expected) return;
    std::printf("FAIL: %s\n  expected: %s\n  actual:   %s\n", what, expected.c_str(),
                actual.c_str());
    ++g_failures;
}

}  // namespace

int main() {
    // 1. Model identification & safety guards
    Expect(royuan::IsDonglePid(0x4011) && royuan::IsDonglePid(0x4014) &&
               royuan::IsDonglePid(0x4038),
           "known ROYUAN dongle PIDs");
    Expect(!royuan::IsDonglePid(0x4015), "0x4015 is wired BC98 PID");
    Expect(royuan::IsSupportedYc500Board(0x03AE), "0x03AE is AKOS BC98");
    Expect(!royuan::IsSupportedYc500Board(0x03AF) && !royuan::IsSupportedYc500Board(0),
           "unknown dev_id rejected");
    ExpectEqual(royuan::ResolveModelName(0x03AE, "Gaming Keyboard"), "AKOS BC98",
                "generic USB name maps to AKOS BC98");
    ExpectEqual(royuan::ResolveModelName(0x03AE, ""), "AKOS BC98",
                "empty USB name maps to AKOS BC98");
    ExpectEqual(royuan::ResolveModelName(0x03AE, "Custom BC98"), "Custom BC98",
                "non-generic name preserved");

    // 2. Checksums ck7 / ck8 & probe packets (0x8F / 0x80)
    {
        std::uint8_t dev_req[royuan::kPacketLen]{};
        royuan::BuildGetDevIdPacket(dev_req);
        Expect(dev_req[0] == 0x00 && dev_req[1] == 0x8F && dev_req[8] == 0x70,
               "0x8F packet matches captured ck7=0x70");

        const std::uint8_t dev_rep[royuan::kPacketLen] = {
            0x00, 0x8F, 0xAE, 0x03, 0x00, 0x00, 0x00, 0x00, 0x70};
        std::uint16_t dev_id = 0;
        Expect(royuan::ParseDevIdReply(dev_rep, &dev_id) && dev_id == 0x03AE,
               "ParseDevIdReply decodes 0x03AE");

        std::uint8_t bad_dev[royuan::kPacketLen] = {0x00, 0x80, 0xAE, 0x03};
        Expect(!royuan::ParseDevIdReply(bad_dev, &dev_id), "wrong opcode rejected");

        std::uint8_t fw_req[royuan::kPacketLen]{};
        royuan::BuildGetFirmwarePacket(fw_req);
        Expect(fw_req[1] == 0x80 && fw_req[8] == 0x7F, "0x80 ck7=0x7F");
        const std::uint8_t fw_rep[royuan::kPacketLen] = {
            0x00, 0x80, 0x02, 0x01, 0x00, 0x00, 0x00, 0x00, 0x7F};
        std::string fw;
        Expect(royuan::ParseFirmwareReply(fw_rep, &fw) && fw == "v1.2",
               "ParseFirmwareReply v1.2");
    }

    // 3. 2.4G dongle status (0xF7) & relay control (0xF6 / 0xFC)
    {
        std::uint8_t f7_req[royuan::kPacketLen]{};
        royuan::BuildDongleStatusPacket(f7_req);
        Expect(f7_req[0] == 0x00 && f7_req[1] == 0xF7, "0xF7 layout");

        const std::uint8_t f7_rep[royuan::kPacketLen] = {
            0x00, 0x01, 0x4E, 0x00, 0x00, 0x00, 0x01, 0x01};
        royuan::DongleStatus st;
        Expect(royuan::ParseDongleStatus(f7_rep, &st) && st.paired && st.online &&
                   st.battery == 78 && st.can_accept_relay && st.reply_ready,
               "ParseDongleStatus online + relay flags");

        std::uint8_t sel[royuan::kPacketLen]{};
        royuan::BuildRelaySelectPacket(royuan::kRelayTargetKeyboard, sel);
        Expect(sel[1] == 0xF6 && sel[2] == 10, "0xF6 keyboard target 10");

        std::uint8_t rel[royuan::kPacketLen]{};
        royuan::BuildRelayReleasePacket(rel);
        Expect(rel[1] == 0xFC, "0xFC relay release");
    }

    // 4. Profile (0x85 / 0x05)
    {
        Expect(royuan::ProfileValid(0) && royuan::ProfileValid(3) &&
                   !royuan::ProfileValid(-1) && !royuan::ProfileValid(4),
               "ProfileValid bounds");
        std::uint8_t p_req[royuan::kPacketLen]{};
        royuan::BuildSetProfilePacket(2, p_req);
        Expect(p_req[1] == 0x05 && p_req[2] == 0x02 && p_req[8] == 0xF8,
               "BuildSetProfilePacket ck7");
        const std::uint8_t p_rep[royuan::kPacketLen] = {0x00, 0x85, 0x02, 0, 0, 0, 0, 0, 0x78};
        int prof = -1;
        Expect(royuan::ParseProfileReply(p_rep, &prof) && prof == 2, "ParseProfileReply 2");
    }

    // 5. Options bitfield (0x86 / 0x06) preserves unknown bits & never forces bit 7
    {
        const std::uint8_t opt_rep[royuan::kPacketLen] = {
            0x00, 0x86, 0x44, 0x04, 0x01, 0x02, 0x03, 0x04, 0x00};
        royuan::KbOptions opt;
        royuan::RawKbOptionsFrame raw;
        Expect(royuan::ParseKbOptionsReply(opt_rep, &opt, &raw), "ParseKbOptionsReply");
        Expect(!opt.win_lock && !opt.mac_mode && !opt.wasd_swap && opt.raw_flags == 0x04,
               "unknown bit2 preserved in raw_flags");

        royuan::KbOptionsPatch patch;
        patch.have_win_lock = true;
        patch.win_lock = true;
        patch.have_backlight_off = true;
        patch.backlight_off = true;
        std::uint8_t opt_write[royuan::kPacketLen]{};
        royuan::BuildSetKbOptionsPacket(raw, patch, opt_write);
        Expect(opt_write[1] == 0x06 && opt_write[2] == 0x44 &&
                   opt_write[3] == (0x04 | 0x01 | 0x10) &&
                   opt_write[4] == 0x01 && opt_write[5] == 0x02 && opt_write[6] == 0x03 &&
                   opt_write[7] == 0x04,
               "0x06 read-modify-write preserves unknown bits & adjacent bytes");
        unsigned sum = 0;
        for (int i = 1; i <= 64; ++i) sum += opt_write[i];
        Expect((sum & 0xFFu) == 0xFFu, "0x06 checksum ck7 sums to 0xFF");
    }

    // 6. Main LED (0x87/0x07) & Side LED (0x88/0x08): speed inversion & white 0xFAFAFA
    {
        std::uint8_t get_main[royuan::kPacketLen]{};
        royuan::BuildGetLedPacket(false, get_main);
        Expect(get_main[1] == 0x87 && get_main[8] == 0x78 && get_main[9] == 0x00,
               "0x87 GET uses ck7 at p[7]");
        std::uint8_t get_side[royuan::kPacketLen]{};
        royuan::BuildGetLedPacket(true, get_side);
        Expect(get_side[1] == 0x88 && get_side[8] == 0x77 && get_side[9] == 0x00,
               "0x88 GET uses ck7 at p[7]");

        const std::uint8_t main_rep[royuan::kPacketLen] = {
            0x00, 0x87, 0x0C, 0x03, 0x03, 0x08, 0xFF, 0xFF, 0xFF};
        royuan::LedParams main_led;
        Expect(royuan::ParseLedReply(false, main_rep, &main_led), "ParseLedReply captured 0x87");
        Expect(main_led.mode == 12 && main_led.speed == 2 && main_led.brightness == 3 &&
                   main_led.colorful && main_led.r == 255 && main_led.g == 255 &&
                   main_led.b == 255,
               "0x87 wire speed 3 -> UI speed 2, colorful=true");

        royuan::LedPatch lpatch;
        lpatch.have_mode = true;
        lpatch.mode = 1;
        lpatch.have_speed = true;
        lpatch.speed = 4;  // fastest -> wire 5 - 4 = 1
        lpatch.have_color = true;
        lpatch.r = 255;
        lpatch.g = 255;
        lpatch.b = 255;
        const royuan::LedParams merged = royuan::MergeLedPatch(main_led, lpatch, false);
        std::uint8_t set_led[royuan::kPacketLen]{};
        royuan::BuildSetLedPacket(false, merged, set_led);
        Expect(set_led[1] == 0x07 && set_led[2] == 1 && set_led[3] == 1 &&
                   set_led[4] == 3 && set_led[5] == 0x07 &&
                   set_led[6] == 0xFA && set_led[7] == 0xFA && set_led[8] == 0xFA,
               "0x07 speed inversion 4->1 & white 255->0xFA");
        unsigned sum = 0;
        for (int i = 1; i <= 64; ++i) sum += set_led[i];
        Expect((sum & 0xFFu) == 0xFFu, "0x07 checksum ck8 sums to 0xFF");

        // Side LED keeps speed direct (0..4)
        const std::uint8_t side_rep[royuan::kPacketLen] = {
            0x00, 0x88, 0x01, 0x03, 0x04, 0x08, 0xFF, 0xFF, 0xFF};
        royuan::LedParams side_led;
        Expect(royuan::ParseLedReply(true, side_rep, &side_led) && side_led.mode == 1 &&
                   side_led.speed == 3 && side_led.brightness == 4,
               "0x88 side LED speed direct 3");

        // Preset color flags (0..6) decode to preset RGB and colorful=false
        const std::uint8_t preset_rep[royuan::kPacketLen] = {
            0x00, 0x87, 0x01, 0x03, 0x04, 0x04, 0x00, 0x00, 0x00};  // flags=4 cyan
        royuan::LedParams preset_led;
        Expect(royuan::ParseLedReply(false, preset_rep, &preset_led) &&
                   !preset_led.colorful && preset_led.r == 0 && preset_led.g == 255 &&
                   preset_led.b == 255,
               "0x87 preset flags=4 decodes to cyan");

        Expect(royuan::MainLedModeValid(1) && royuan::MainLedModeValid(19) &&
                   !royuan::MainLedModeValid(13) && !royuan::MainLedModeValid(20),
               "MainLedModeValid excludes host-stream modes 13/20");
        Expect(!royuan::MainLedSupportsColor(3) && !royuan::MainLedSupportsColor(12) &&
                   !royuan::MainLedSupportsColor(16) && royuan::MainLedSupportsColor(1),
               "MainLedSupportsColor");
    }

    // 7. Debounce (0x91 / 0x11) & Sleep Timers (0x92 / 0x12)
    {
        Expect(royuan::DebounceValid(1) && royuan::DebounceValid(10) &&
                   !royuan::DebounceValid(0) && !royuan::DebounceValid(11),
               "DebounceValid 1..10");
        const std::uint8_t deb_rep[royuan::kPacketLen] = {
            0x00, 0x91, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00, 0x6E};
        int deb = -1;
        Expect(royuan::ParseDebounceReply(deb_rep, &deb) && deb == 1,
               "ParseDebounceReply captured 0x91");

        std::uint8_t deb_set[royuan::kPacketLen]{};
        royuan::BuildSetDebouncePacket(5, deb_set);
        Expect(deb_set[1] == 0x11 && deb_set[2] == 0x00 && deb_set[3] == 5,
               "BuildSetDebouncePacket value at p[2]");

        const std::uint8_t slp_rep[royuan::kPacketLen] = {
            0x00, 0x92, 0x78, 0x00, 0x78, 0x00, 0x90, 0x06, 0x90, 0x06};
        royuan::SleepTimers slp;
        Expect(royuan::ParseSleepReply(slp_rep, &slp) && slp.sleep_bt == 120 &&
                   slp.sleep_24g == 120 && slp.deep_bt == 1680 && slp.deep_24g == 1680,
               "ParseSleepReply captured 0x92");

        std::uint8_t slp_set[royuan::kPacketLen]{};
        royuan::BuildSetSleepPacket(slp, slp_set);
        Expect(slp_set[1] == 0x12 && slp_set[8] == 0xED &&
                   slp_set[9] == 0x78 && slp_set[10] == 0x00 &&
                   slp_set[11] == 0x78 && slp_set[12] == 0x00 && slp_set[13] == 0x90 &&
                   slp_set[14] == 0x06 && slp_set[15] == 0x90 && slp_set[16] == 0x06,
               "BuildSetSleepPacket puts timers at p[8..15] with header-only ck7=0xED");
    }

    // 8. driver registry routing for royuan:3151:4015
    {
        drivers::Registry reg;
        reg.Add(drivers::RoyuanDescriptor());
        Expect(reg.FindByKey("royuan:3151:4015") != nullptr &&
                   reg.FindByKey("royuan:3151:4015")->name == "royuan",
               "Registry routes royuan:3151:4015 to royuan");
        std::uint16_t pid = 0;
        Expect(driver_router::ParseRoyuanPidFromKey("royuan:3151:4015", &pid) && pid == 0x4015,
               "ParseRoyuanPidFromKey 0x4015");
        Expect(!driver_router::ParseRoyuanPidFromKey("royuan:1234:4015", &pid),
               "ParseRoyuanPidFromKey rejects non-3151 VID");

        bridge::DeviceSnapshot full_snap;
        full_snap.key = "royuan:3151:4015";
        full_snap.name = "AKOS BC98";
        full_snap.kind = "keyboard";
        full_snap.driver_level = "full";
        const std::string info_json =
            drivers::BuildDeviceInfoJson(reg, "royuan:3151:4015", &full_snap);
        json::Value doc;
        std::string err;
        Expect(json::Parse(info_json, &doc, &err) && doc.type == json::Value::Type::kObject,
               "BuildDeviceInfoJson royuan parses cleanly");
        std::string drv;
        Expect(doc.GetString("driver", &drv) && drv == "royuan",
               "BuildDeviceInfoJson driver == royuan");
        const json::Value* caps = doc.Find("capabilities");
        const json::Value* kbd = caps != nullptr ? caps->Find("keyboard") : nullptr;
        Expect(kbd != nullptr && kbd->type == json::Value::Type::kObject,
               "BuildDeviceInfoJson carries capabilities.keyboard");

        // Unknown ROYUAN board stays battery-only
        bridge::DeviceSnapshot bat_snap = full_snap;
        bat_snap.driver_level = "battery";
        const std::string bat_json =
            drivers::BuildDeviceInfoJson(reg, "royuan:3151:4099", &bat_snap);
        json::Value bat_doc;
        Expect(json::Parse(bat_json, &bat_doc, &err) &&
                   bat_doc.GetString("driver", &drv) && drv == "none",
               "unknown ROYUAN battery-only snapshot keeps driver == none");

        const json::Value* km_cap = kbd->Find("keymap");
        const json::Value* fn_cap = kbd->Find("fnLayer");
        const json::Value* macro_cap = kbd->Find("macros");
        std::string kbd_layout;
        long long kbd_fn_slot = -1;
        Expect(kbd->GetString("layout", &kbd_layout) && kbd_layout == "bc98",
               "capabilities.keyboard.layout == bc98");
        Expect(kbd->GetInt("fnSlot", &kbd_fn_slot) && kbd_fn_slot == 65,
               "capabilities.keyboard.fnSlot == 65");
        Expect(km_cap != nullptr && km_cap->type == json::Value::Type::kBool && km_cap->boolean,
               "capabilities.keyboard.keymap == true");
        Expect(fn_cap != nullptr && fn_cap->type == json::Value::Type::kBool && fn_cap->boolean,
               "capabilities.keyboard.fnLayer == true");
        Expect(macro_cap != nullptr && macro_cap->type == json::Value::Type::kObject,
               "capabilities.keyboard.macros object present");
    }

    // 9. AKOS BC98 Factory Keymap & Key Entry Validation + Packet Builders
    {
        Expect(royuan::IsBc98PhysicalSlot(0) && royuan::IsBc98PhysicalSlot(65) &&
                   royuan::IsBc98PhysicalSlot(90) && royuan::IsBc98PhysicalSlot(104) &&
                   royuan::IsBc98PhysicalSlot(105) && !royuan::IsBc98PhysicalSlot(78) &&
                   !royuan::IsBc98PhysicalSlot(10),
               "IsBc98PhysicalSlot matches exact 98-key BC98 matrix");

        royuan::KeyMatrix base, fn;
        royuan::GetBc98FactoryKeymap(false, &base);
        royuan::GetBc98FactoryKeymap(true, &fn);
        Expect(base.Slot(0) == (royuan::KeyEntry{{0, 0, 41, 0}}), "BC98 factory slot 0 == Esc");
        Expect(base.Slot(65) == (royuan::KeyEntry{{10, 1, 0, 0}}), "BC98 factory slot 65 == Fn");
        Expect(base.Slot(90) == (royuan::KeyEntry{{0, 0, 70, 0}}), "BC98 factory slot 90 == PrtSc");
        Expect(base.Slot(104) == (royuan::KeyEntry{{0, 0, 87, 0}}), "BC98 factory slot 104 == Num+");
        Expect(base.Slot(105) == (royuan::KeyEntry{{0, 0, 88, 0}}),
               "BC98 factory slot 105 == NumEnter");
        Expect(base.Slot(10) == (royuan::KeyEntry{{0, 0, 100, 0}}) &&
                   base.Slot(75) == (royuan::KeyEntry{{0, 0, 50, 0}}),
               "BC98 factory base preserves unfitted ISO slots 10 & 75");
        Expect(fn.Slot(0) == (royuan::KeyEntry{{10, 2, 0, 0}}) &&
                   fn.Slot(7) == (royuan::KeyEntry{{14, 0, 0, 0}}),
               "BC98 factory Fn slots populated");

        std::string reason;
        Expect(royuan::KeyEntryValid(royuan::KeyEntry{{0, 0, 0, 0}}, false, &reason),
               "clear [0,0,0,0] valid");
        Expect(royuan::KeyEntryValid(royuan::KeyEntry{{0, 224, 6, 0}}, false, &reason),
               "combo Ctrl+C valid");
        Expect(royuan::KeyEntryValid(royuan::KeyEntry{{9, 1, 49, 0}}, false, &reason),
               "macro toggle M50 valid");
        Expect(!royuan::KeyEntryValid(royuan::KeyEntry{{10, 7, 0, 0}}, false, &reason),
               "KB Lock [10,7,0,0] blocked");
        Expect(!royuan::KeyEntryValid(royuan::KeyEntry{{9, 0, 50, 0}}, false, &reason),
               "macro slot 50 rejected");
        Expect(!royuan::KeyEntryValid(royuan::KeyEntry{{99, 0, 0, 0}}, false, &reason),
               "unknown key tag 99 rejected");

        std::uint8_t get_pg[royuan::kPacketLen]{};
        royuan::BuildGetKeymapPagePacket(1, 5, true, get_pg);
        Expect(get_pg[1] == 0x90 && get_pg[2] == 1 && get_pg[3] == 5,
               "BuildGetKeymapPagePacket 0x90 page 5");
        unsigned sum_get = 0;
        for (int i = 1; i <= 64; ++i) sum_get += get_pg[i];
        Expect((sum_get & 0xFFu) == 0xFFu, "0x90 ck7 sums to 0xFF");

        std::uint8_t set_one[royuan::kPacketLen]{};
        royuan::BuildSetKeyOnePacket(0, 90, false, royuan::KeyEntry{{0, 0, 104, 0}}, set_one);
        Expect(set_one[1] == 0x13 && set_one[2] == 0 && set_one[3] == 90 &&
                   set_one[8] == static_cast<std::uint8_t>(0xFFu - (0x13u + 90u)) &&
                   set_one[9] == 0 && set_one[10] == 0 && set_one[11] == 104 && set_one[12] == 0,
               "BuildSetKeyOnePacket ck7 covers header only");

        std::uint8_t bulk_pg[royuan::kPacketLen]{};
        royuan::BuildBulkKeymapPagePacket(2, 0, false, base, bulk_pg);
        Expect(bulk_pg[1] == 0x09 && bulk_pg[2] == 2 && bulk_pg[3] == 0xF8 &&
                   bulk_pg[4] == 1 && bulk_pg[5] == 0 &&
                   bulk_pg[8] == static_cast<std::uint8_t>(0xFFu - ((0x09u + 2u + 0xF8u + 1u) & 0xFFu)) &&
                   bulk_pg[9] == 0 && bulk_pg[10] == 0 && bulk_pg[11] == 41 && bulk_pg[12] == 0,
               "BuildBulkKeymapPagePacket header ck7 + 56B payload");
    }

    // 10. Onboard Macro Codec (0x8B / 0x16)
    {
        royuan::MacroData macro;
        macro.repeat = 3;
        macro.events = {
            {royuan::MacroEventKind::kKey, 0x0B, true, 0, 0, 20},    // short 2B
            {royuan::MacroEventKind::kKey, 0x0B, false, 0, 0, 0},    // long 4B (0ms)
            {royuan::MacroEventKind::kKey, 0x0C, true, 0, 0, 260},   // long 4B (>127ms)
            {royuan::MacroEventKind::kKey, 0x0C, false, 0, 0, 50},
            {royuan::MacroEventKind::kMouseButton, 1, true, 0, 0, 15},
            {royuan::MacroEventKind::kMouseButton, 1, false, 0, 0, 25},
            {royuan::MacroEventKind::kMouseMove, 0, false, -12, 34, 10},
        };
        std::uint8_t blob[royuan::kMacroBytes]{};
        std::string err;
        int stream_len = 0;
        Expect(royuan::EncodeMacroBlob(macro, blob, &err, &stream_len) && stream_len == 20,
               "EncodeMacroBlob short + 0ms long + >127ms long + mouse events == 20B");
        Expect(blob[0] == 3 && blob[1] == 0, "macro repeat LE u16");
        // Verify 0ms delay encoded as 4B [0x0B, 0x00, 0x00, 0x00] not 2B!
        Expect(blob[4] == 0x0B && blob[5] == 0x00 && blob[6] == 0x00 && blob[7] == 0x00,
               "0ms key release uses 4-byte long form");
        Expect(royuan::CountMacroWritePages(blob) == 1, "20B stream fits in 1 write page");

        bool empty = true;
        const royuan::MacroData decoded = royuan::DecodeMacroBlob(blob, &empty);
        Expect(!empty && decoded.repeat == 3 && decoded.events.size() == macro.events.size(),
               "DecodeMacroBlob roundtrip size");
        Expect(decoded.events[1].delay_ms == 0 && decoded.events[2].delay_ms == 260 &&
                   decoded.events[4].kind == royuan::MacroEventKind::kMouseButton &&
                   decoded.events[4].code == 1 &&
                   decoded.events[6].kind == royuan::MacroEventKind::kMouseMove &&
                   decoded.events[6].dx == -12 && decoded.events[6].dy == 34,
               "DecodeMacroBlob preserves delays, mouse buttons, and signed move");

        // Unreleased key must fail validation
        royuan::MacroData dangling;
        dangling.repeat = 1;
        dangling.events = {{royuan::MacroEventKind::kKey, 0x04, true, 0, 0, 20}};
        Expect(!royuan::EncodeMacroBlob(dangling, blob, &err),
               "dangling key down without release rejected");

        // Overflow > 250 stream bytes must fail validation
        royuan::MacroData huge;
        huge.repeat = 1;
        for (int i = 0; i < 65; ++i) {
            huge.events.push_back({royuan::MacroEventKind::kKey, 0x04, true, 0, 0, 20});
            huge.events.push_back({royuan::MacroEventKind::kKey, 0x04, false, 0, 0, 20});
        }
        Expect(!royuan::EncodeMacroBlob(huge, blob, &err), "macro > 250B rejected");

        std::uint8_t zero_blob[royuan::kMacroBytes]{};
        std::uint8_t m_pkt[royuan::kPacketLen]{};
        royuan::BuildSetMacroPagePacket(49, 0, true, zero_blob, m_pkt);
        Expect(m_pkt[1] == 0x16 && m_pkt[2] == 49 && m_pkt[3] == 0 && m_pkt[4] == 56 &&
                   m_pkt[5] == 1,
               "BuildSetMacroPagePacket header");
        unsigned m_sum = 0;
        for (int i = 1; i <= 64; ++i) m_sum += m_pkt[i];
        Expect((m_sum & 0xFFu) == 0xFFu, "BuildSetMacroPagePacket ck7 sums to 0xFF");
    }

    if (g_failures == 0) {
        std::printf("royuan_protocol_test: OK (checksums, yc500 LED/options/debounce/sleep, keymap, macros, router)\n");
        return 0;
    }
    std::printf("royuan_protocol_test: %d failure(s)\n", g_failures);
    return 1;
}
