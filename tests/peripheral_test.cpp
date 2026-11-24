// Unit tests for peripheral JSON parsing, kind normalization, and device merging.
#include <cstdio>
#include <string>
#include <vector>

#include "../native/src/peripheral_merge.h"

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
    // 1. Kind normalization & Gaming Keyboard fix.
    ExpectEqual(peripherals::NormalizeKind("mouse", "Gaming Keyboard"), "keyboard",
                "Gaming Keyboard overrides upstream mouse kind");
    ExpectEqual(peripherals::NormalizeKind("other", "MY KBD 75"), "keyboard",
                "kbd token resolves to keyboard");
    ExpectEqual(peripherals::NormalizeKind("other", "VIVALDI Hands-Free"), "headset",
                "hands-free resolves to headset");
    ExpectEqual(peripherals::NormalizeKind("other", "Logitech Mouse"), "mouse",
                "mouse resolves to mouse");
    ExpectEqual(peripherals::NormalizeKind("other", "DualSense Wireless Controller"), "gamepad",
                "controller resolves to gamepad");
    ExpectEqual(peripherals::NormalizeKind("other", "Studio Mic"), "microphone",
                "mic word resolves to microphone");
    ExpectEqual(peripherals::NormalizeKind("other", "iPhone 16"), "phone",
                "iphone resolves to phone");

    // 2. Mode mapping from via label.
    ExpectEqual(peripherals::ModeFromVia("Bluetooth"), "bluetooth", "Bluetooth mode");
    ExpectEqual(peripherals::ModeFromVia("BLE 5.3"), "bluetooth", "BLE mode");
    ExpectEqual(peripherals::ModeFromVia("USB"), "wired", "USB wired mode");
    ExpectEqual(peripherals::ModeFromVia("2.4 GHz receiver"), "wireless", "2.4G wireless mode");

    // 3. Parse real captured upstream JSON.
    const std::string real_upstream =
        R"([{"key":"ajazzaj:3151:4015","name":"Gaming Keyboard","via":"2.4 GHz receiver","kind":"mouse","percent":-1,"level":"","detail":"","charging":false,"asleep":false,"connected":false,"millivolts":0,"graceSec":0},)"
        R"({"key":"incott:522C","name":"Incott mouse","via":"2.4 GHz receiver","kind":"mouse","percent":32,"level":"","detail":"","charging":false,"asleep":false,"connected":true,"millivolts":0,"graceSec":1800},)"
        R"({"key":"bt:{D699CC78-883F-5C73-A890-F8EE81BB179D}","name":"VIVALDI","via":"Bluetooth","kind":"headset","percent":40,"level":"","detail":"","charging":false,"asleep":false,"connected":true,"millivolts":0,"graceSec":0}])";
    std::vector<bridge::DeviceSnapshot> upstream;
    std::string error;
    Expect(peripherals::ParseUpstreamJson(real_upstream, &upstream, &error),
           "ParseUpstreamJson succeeds on captured output");
    Expect(upstream.size() == 3, "ParseUpstreamJson parses 3 items");
    if (upstream.size() == 3) {
        ExpectEqual(upstream[0].key, "ajazzaj:3151:4015", "item 0 key");
        ExpectEqual(upstream[0].kind, "keyboard", "item 0 normalized to keyboard");
        Expect(!upstream[0].connected && upstream[0].battery == -1, "item 0 offline -1");

        ExpectEqual(upstream[1].key, "incott:522C", "item 1 key");
        Expect(upstream[1].connected && upstream[1].battery == 32 && upstream[1].charging == 0,
               "item 1 discharging 32%");

        ExpectEqual(upstream[2].kind, "headset", "item 2 headset");
        ExpectEqual(upstream[2].mode, "bluetooth", "item 2 bluetooth mode");
        Expect(upstream[2].connected && upstream[2].battery == 40, "item 2 connected 40%");
    }

    // Invalid upstream JSON fails cleanly.
    std::vector<bridge::DeviceSnapshot> bad_out;
    Expect(!peripherals::ParseUpstreamJson("{}", &bad_out, &error),
           "ParseUpstreamJson rejects object root");
    Expect(!peripherals::ParseUpstreamJson("{broken", &bad_out, &error),
           "ParseUpstreamJson rejects malformed JSON");

    // 4. Parse HeadsetControl JSON including float voltage.
    const std::string hsc_json = R"({
      "name": "HeadsetControl",
      "version": "4.3.0",
      "device_count": 2,
      "devices": [
        {
          "device": "SteelSeries Arctis Nova 7",
          "id_vendor": "0x1038",
          "id_product": "0x2202",
          "battery": { "status": "BATTERY_CHARGING", "level": 68, "voltage_mv": 3912.75 }
        },
        {
          "device": "Corsair HS80",
          "id_vendor": "0x1b1c",
          "id_product": "0x0a6b",
          "battery": { "status": "BATTERY_UNAVAILABLE", "level": -1 }
        }
      ]
    })";
    std::vector<bridge::DeviceSnapshot> hsc;
    Expect(peripherals::ParseHeadsetControlJson(hsc_json, &hsc, &error),
           "ParseHeadsetControlJson handles float fields");
    Expect(hsc.size() == 2, "HeadsetControl parses 2 devices");
    if (hsc.size() == 2) {
        ExpectEqual(hsc[0].key, "hsc:1038:2202", "hsc hex key");
        ExpectEqual(hsc[0].kind, "headset", "hsc kind");
        Expect(hsc[0].connected && hsc[0].battery == 68 && hsc[0].charging == 2,
               "hsc charging maps to 2");
        Expect(!hsc[1].connected && hsc[1].battery == -1 && hsc[1].charging == -1,
               "hsc unavailable maps to disconnected");
    }

    // 5. Merge & deduplication across native Incott, upstream, native BT, HSC, HID, XInput.
    bridge::DeviceSnapshot native_g23;
    native_g23.key = "incott:522C";
    native_g23.name = "Incott G23";
    native_g23.kind = "mouse";
    native_g23.via = "2.4 GHz receiver";
    native_g23.driver_level = "full";
    native_g23.mode = "wireless";
    native_g23.receiver = "1K";
    native_g23.sensor = "PAW3395";
    native_g23.psd = 1;
    native_g23.battery = 32;
    native_g23.charging = 1;
    native_g23.connected = true;

    bridge::DeviceSnapshot bt_dup;
    bt_dup.key = "bt:{d699cc78-883f-5c73-a890-f8ee81bb179d}";  // lowercase GUID duplicate
    bt_dup.name = "VIVALDI";
    bt_dup.kind = "headset";
    bt_dup.via = "Bluetooth";
    bt_dup.mode = "bluetooth";
    bt_dup.battery = 40;
    bt_dup.connected = true;

    bridge::DeviceSnapshot bt_new;
    bt_new.key = "bt:{162B5710-6B20-543B-9AD4-5E1BB892725E}";
    bt_new.name = "EDI-820";
    bt_new.kind = "headset";
    bt_new.via = "Bluetooth";
    bt_new.mode = "bluetooth";
    bt_new.battery = 50;
    bt_new.connected = true;

    bridge::DeviceSnapshot hid_dup_incott;
    hid_dup_incott.key = "hid:093A:522C";
    hid_dup_incott.name = "Wireless Gaming Mouse";
    hid_dup_incott.kind = "mouse";
    hid_dup_incott.via = "HID";
    hid_dup_incott.battery = 32;
    hid_dup_incott.connected = true;

    bridge::DeviceSnapshot hid_dup_hsc;
    hid_dup_hsc.key = "hid:1038:2202";
    hid_dup_hsc.name = "Arctis Nova 7";
    hid_dup_hsc.kind = "headset";
    hid_dup_hsc.via = "HID";
    hid_dup_hsc.battery = 68;
    hid_dup_hsc.connected = true;

    bridge::DeviceSnapshot hid_unique;
    hid_unique.key = "hid:046D:C547";
    hid_unique.name = "G PRO X";
    hid_unique.kind = "mouse";
    hid_unique.via = "HID";
    hid_unique.battery = 85;
    hid_unique.connected = true;

    bridge::DeviceSnapshot xinput_pad;
    xinput_pad.key = "xinput:0";
    xinput_pad.name = "Xbox Controller #1";
    xinput_pad.kind = "gamepad";
    xinput_pad.via = "XInput";
    xinput_pad.battery = 60;
    xinput_pad.level = "Medium";
    xinput_pad.connected = true;

    const auto merged = peripherals::MergeDevices(
        {native_g23}, {}, upstream, {bt_dup, bt_new}, hsc,
        {hid_dup_incott, hid_dup_hsc, hid_unique}, {xinput_pad});

    // Expected order:
    // 0: native_g23 (full, connected)
    // 1: VIVALDI (connected from upstream, native_bt dup skipped)
    // 2: EDI-820 (connected from native_bt)
    // 3: SteelSeries Arctis Nova 7 (connected from hsc; hid:1038:2202 skipped)
    // 4: G PRO X (connected from hid)
    // 5: Xbox Controller #1 (connected from xinput)
    // 6: Gaming Keyboard (offline from upstream)
    // 7: Corsair HS80 (offline from hsc)
    Expect(merged.size() == 8, "merged count is 8 after dedup");
    if (merged.size() == 8) {
        ExpectEqual(merged[0].name, "Incott G23", "slot 0 is native Incott G23");
        ExpectEqual(merged[0].driver_level, "full", "slot 0 keeps full driverLevel");
        ExpectEqual(merged[0].sensor, "PAW3395", "slot 0 keeps PAW3395");
        ExpectEqual(merged[1].name, "VIVALDI", "slot 1 is VIVALDI");
        ExpectEqual(merged[2].name, "EDI-820", "slot 2 is EDI-820");
        ExpectEqual(merged[3].key, "hsc:1038:2202", "slot 3 is hsc Nova 7");
        ExpectEqual(merged[4].key, "hid:046D:C547", "slot 4 is unique HID mouse");
        ExpectEqual(merged[5].key, "xinput:0", "slot 5 is Xbox pad");
        Expect(!merged[6].connected && merged[6].key == "ajazzaj:3151:4015",
               "slot 6 is offline keyboard");
        Expect(!merged[7].connected && merged[7].key == "hsc:1B1C:0A6B",
               "slot 7 is offline HS80");
    }

    // 6. ROYUAN (VID 0x3151) packet helpers & overriding upstream ajazzaj:3151:4015.
    {
        Expect(peripherals::IsRoyuanDonglePid(0x4011), "0x4011 is ROYUAN 2.4G dongle");
        Expect(!peripherals::IsRoyuanDonglePid(0x4015), "0x4015 is ROYUAN USB wired keyboard");

        std::uint8_t get_rev[peripherals::kRoyuanPacketLen]{};
        peripherals::BuildRoyuanGetRevPacket(get_rev);
        Expect(get_rev[0] == 0x00 && get_rev[1] == 0x80 && get_rev[8] == 0x7F,
               "BuildRoyuanGetRevPacket layout");

        std::uint8_t rev_reply[peripherals::kRoyuanPacketLen] = {
            0x00, 0x80, 0x02, 0x01, 0x00, 0x00, 0x00, 0x00, 0x7F};
        std::string fw_ver;
        Expect(peripherals::ParseRoyuanFirmwareReply(rev_reply, &fw_ver),
               "ParseRoyuanFirmwareReply succeeds on captured 3151:4015 frame");
        ExpectEqual(fw_ver, "v1.2", "ParseRoyuanFirmwareReply decodes v1.2");

        std::uint8_t status_pkt[peripherals::kRoyuanPacketLen]{};
        peripherals::BuildRoyuanDongleStatusPacket(status_pkt);
        Expect(status_pkt[0] == 0x00 && status_pkt[1] == 0xF7,
               "BuildRoyuanDongleStatusPacket layout");

        std::uint8_t dongle_online[peripherals::kRoyuanPacketLen] = {
            0x00, 0x00, 0x42, 0x00, 0x00, 0x01, 0x01, 0x01, 0x00};
        bool online = false;
        int bat = -1;
        Expect(peripherals::ParseRoyuanDongleStatus(dongle_online, &online, &bat) &&
                   online && bat == 66,
               "ParseRoyuanDongleStatus online keyboard 66%");

        std::uint8_t dongle_offline[peripherals::kRoyuanPacketLen] = {
            0x00, 0x00, 0x42, 0x00, 0x01, 0x00, 0x01, 0x01, 0x00};
        Expect(peripherals::ParseRoyuanDongleStatus(dongle_offline, &online, &bat) &&
                   !online && bat == -1,
               "ParseRoyuanDongleStatus offline keyboard clears battery");

        std::uint8_t dongle_mouse_only[peripherals::kRoyuanPacketLen] = {
            0x00, 0x00, 0x00, 0x50, 0x01, 0x00, 0x01, 0x02, 0x00};
        Expect(!peripherals::ParseRoyuanDongleStatus(dongle_mouse_only, &online, &bat),
               "ParseRoyuanDongleStatus rejects mouse-only dongle");

        bridge::DeviceSnapshot royuan_wired;
        royuan_wired.key = "royuan:3151:4015";
        royuan_wired.name = "Gaming Keyboard";
        royuan_wired.kind = "keyboard";
        royuan_wired.via = "USB";
        royuan_wired.driver_level = "battery";
        royuan_wired.mode = "wired";
        royuan_wired.battery = -1;
        royuan_wired.charging = -1;
        royuan_wired.connected = true;
        royuan_wired.detail = "固件 v1.2 · 有线模式不上报电量";

        const auto wired_merged = peripherals::MergeDevices(
            {native_g23}, {royuan_wired}, upstream, {}, {}, {}, {});
        Expect(wired_merged.size() == 3,
               "native royuan:3151:4015 overrides upstream ajazzaj:3151:4015");
        if (wired_merged.size() == 3) {
            ExpectEqual(wired_merged[1].key, "royuan:3151:4015",
                        "royuan wired keyboard takes connected slot");
            Expect(wired_merged[1].connected && wired_merged[1].mode == "wired",
                   "royuan wired keyboard is connected and wired");
            ExpectEqual(wired_merged[2].key, "bt:{D699CC78-883F-5C73-A890-F8EE81BB179D}",
                        "bt headset remains alongside wired keyboard");
        }
    }

    // 7. K-snake X11 (VID 0xA8A4 USB / 0xA8A5 2.4G, PID 0x2255) battery & online protocol.
    {
        Expect(peripherals::IsKsnakeX11Device(0xA8A4, 0x2255), "0xA8A4:0x2255 is K-snake X11 USB");
        Expect(peripherals::IsKsnakeX11Device(0xA8A5, 0x2255), "0xA8A5:0x2255 is K-snake X11 2.4G");
        Expect(!peripherals::IsKsnakeX11Device(0xA8A5, 0x1234), "wrong PID rejected");
        Expect(peripherals::IsKsnakeDongleVid(0xA8A5), "0xA8A5 is 2.4G dongle");
        Expect(!peripherals::IsKsnakeDongleVid(0xA8A4), "0xA8A4 is USB wired");

        std::uint8_t bat_req[peripherals::kKsnakePacketLen]{};
        peripherals::BuildKsnakeBatteryPacket(bat_req);
        Expect(bat_req[0] == 0x00 && bat_req[1] == 0x55 && bat_req[2] == 0x30 &&
                   bat_req[3] == 0xA5 && bat_req[4] == 0x0B && bat_req[5] == 0x2E &&
                   bat_req[6] == 0x01 && bat_req[7] == 0x01,
               "BuildKsnakeBatteryPacket layout");

        std::uint8_t bat_reply[peripherals::kKsnakePacketLen]{};
        bat_reply[0] = 0x00;
        bat_reply[1] = 0xAA;
        bat_reply[2] = 0x30;
        bat_reply[9] = 85;  // WebHID t[8] = battery_value
        bat_reply[10] = 1;  // WebHID t[9] = charge_flag
        int bat = -1;
        int chg = -1;
        Expect(peripherals::ParseKsnakeBatteryReply(bat_reply, &bat, &chg) &&
                   bat == 85 && chg == 2,
               "ParseKsnakeBatteryReply 0x30 charging 85% -> charging=2");

        bat_reply[9] = 64;
        bat_reply[10] = 0;
        Expect(peripherals::ParseKsnakeBatteryReply(bat_reply, &bat, &chg) &&
                   bat == 64 && chg == 0,
               "ParseKsnakeBatteryReply 0x30 discharging 64% -> charging=0");

        bat_reply[9] = 0;
        bat_reply[10] = 0;
        Expect(!peripherals::ParseKsnakeBatteryReply(bat_reply, &bat, &chg),
               "ParseKsnakeBatteryReply rejects 0% when not charging");

        std::uint8_t bat_push[peripherals::kKsnakePacketLen]{};
        bat_push[0] = 0x00;
        bat_push[1] = 0xAA;
        bat_push[2] = 0xFA;
        bat_push[9] = 0xD0;   // WebHID n[8] == 208
        bat_push[10] = 92;    // WebHID n[9] = battery
        bat_push[11] = 1;     // WebHID n[10] = charge_flag
        Expect(peripherals::ParseKsnakeBatteryReply(bat_push, &bat, &chg) &&
                   bat == 92 && chg == 2,
               "ParseKsnakeBatteryReply 0xFA/0xD0 push 92% charging");

        std::uint8_t dpi_push[peripherals::kKsnakePacketLen]{};
        dpi_push[1] = 0xAA;
        dpi_push[2] = 0xFA;
        dpi_push[9] = 0x01;  // not 0xD0
        Expect(!peripherals::ParseKsnakeBatteryReply(dpi_push, &bat, &chg),
               "ParseKsnakeBatteryReply ignores non-battery 0xFA push");

        std::uint8_t online_req[peripherals::kKsnakePacketLen]{};
        peripherals::BuildKsnakeOnlinePacket(online_req);
        Expect(online_req[0] == 0x00 && online_req[1] == 0x55 && online_req[2] == 0xED &&
                   online_req[4] == 0x01 && online_req[5] == 0x2E,
               "BuildKsnakeOnlinePacket layout");

        std::uint8_t online_reply[peripherals::kKsnakePacketLen]{};
        online_reply[1] = 0xAA;
        online_reply[2] = 0xED;
        online_reply[9] = 2;
        bool online = false;
        Expect(peripherals::ParseKsnakeOnlineReply(online_reply, &online) && online,
               "ParseKsnakeOnlineReply byte 2 -> online");

        online_reply[9] = 0;
        Expect(peripherals::ParseKsnakeOnlineReply(online_reply, &online) && !online,
               "ParseKsnakeOnlineReply byte 0 -> offline");

        bridge::DeviceSnapshot x11;
        x11.key = "ksnake:A8A5:2255";
        x11.name = "Wireless Mouse";
        x11.kind = "mouse";
        x11.via = "2.4 GHz receiver";
        x11.driver_level = "full";
        x11.mode = "wireless";
        x11.battery = 85;
        x11.charging = 2;
        x11.connected = true;

        bridge::DeviceSnapshot x11_hid_dup;
        x11_hid_dup.key = "hid:A8A5:2255";
        x11_hid_dup.name = "USB Mouse";
        x11_hid_dup.kind = "mouse";
        x11_hid_dup.via = "HID";
        x11_hid_dup.battery = 85;
        x11_hid_dup.connected = true;

        const auto x11_merged =
            peripherals::MergeDevices({}, {x11}, {}, {}, {}, {x11_hid_dup}, {});
        Expect(x11_merged.size() == 1 && x11_merged[0].key == "ksnake:A8A5:2255" &&
                   x11_merged[0].charging == 2 && x11_merged[0].driver_level == "full",
               "native ksnake:A8A5:2255 deduplicates hid:A8A5:2255 and keeps full/charging=2");
    }

    // 8. AKOS BC98 DevId (0x8F) & Bluetooth hint normalization + full keyboard merge
    {
        std::uint8_t devid_req[peripherals::kRoyuanPacketLen]{};
        peripherals::BuildRoyuanGetDevIdPacket(devid_req);
        Expect(devid_req[0] == 0x00 && devid_req[1] == 0x8F && devid_req[8] == 0x70,
               "BuildRoyuanGetDevIdPacket layout");

        const std::uint8_t devid_rep[peripherals::kRoyuanPacketLen] = {
            0x00, 0x8F, 0xAE, 0x03, 0x00, 0x00, 0x00, 0x00, 0x70};
        std::uint16_t dev_id = 0;
        Expect(peripherals::ParseRoyuanDevIdReply(devid_rep, &dev_id) &&
                   peripherals::IsAkosBc98Board(dev_id),
               "ParseRoyuanDevIdReply identifies AKOS BC98 (0x03AE)");
        ExpectEqual(peripherals::ResolveRoyuanModelName(dev_id, "Gaming Keyboard"), "AKOS BC98",
                    "ResolveRoyuanModelName replaces generic USB string");

        Expect(peripherals::IsRoyuanBtHint("BTHLE\\DEV_001\\VID&023151", "BT Keyboard"),
               "IsRoyuanBtHint matches VID 3151");
        Expect(peripherals::IsRoyuanBtHint("", "BC98 5.0"),
               "IsRoyuanBtHint matches BC98 name");
        ExpectEqual(peripherals::NormalizeRoyuanBtName("BC98 5.0"), "AKOS BC98 5.0",
                    "NormalizeRoyuanBtName adds AKOS prefix");
        ExpectEqual(peripherals::NormalizeRoyuanBtName("AKOS BC98"), "AKOS BC98",
                    "NormalizeRoyuanBtName keeps existing AKOS prefix");

        bridge::DeviceSnapshot bc98;
        bc98.key = "royuan:3151:4015";
        bc98.name = "AKOS BC98";
        bc98.kind = "keyboard";
        bc98.via = "USB";
        bc98.driver_level = "full";
        bc98.mode = "wired";
        bc98.connected = true;

        const auto merged_kbd = peripherals::MergeDevices(
            {}, {bc98}, upstream, {}, {}, {}, {});
        Expect(merged_kbd.size() >= 2 && merged_kbd[0].key == "royuan:3151:4015",
               "full ROYUAN keyboard sorts into bucket 0");
    }

    // 9. Documented external probe JSON example fixture (docs/09-external-probe.md).
    {
        const std::string example_probe_json =
            R"([{"key":"ext:example:0001","name":"Example Device","via":"2.4 GHz receiver","kind":"mouse","percent":78,"connected":true,"asleep":false,"charging":false,"level":"","detail":"2.4G dongle"}])";
        std::vector<bridge::DeviceSnapshot> ex;
        std::string ex_err;
        Expect(peripherals::ParseUpstreamJson(example_probe_json, &ex, &ex_err),
               "documented external probe example JSON parses");
        Expect(ex.size() == 1, "documented external probe example has 1 item");
        if (ex.size() == 1) {
            ExpectEqual(ex[0].key, "ext:example:0001", "example key");
            ExpectEqual(ex[0].name, "Example Device", "example name");
            ExpectEqual(ex[0].via, "2.4 GHz receiver", "example via");
            ExpectEqual(ex[0].kind, "mouse", "example kind");
            ExpectEqual(ex[0].mode, "wireless", "example mode");
            ExpectEqual(ex[0].driver_level, "battery", "example driverLevel");
            Expect(ex[0].battery == 78, "example battery 78");
            Expect(ex[0].connected && !ex[0].asleep, "example connected and awake");
            Expect(ex[0].charging == 0, "example charging=false -> 0");
            ExpectEqual(ex[0].level, "", "example level empty");
            ExpectEqual(ex[0].detail, "2.4G dongle", "example detail");
        }
    }

    if (g_failures == 0) {
        std::printf("peripheral_test: OK (kind, mode, upstream, headsetcontrol, merge, dedup)\n");
        return 0;
    }
    std::printf("peripheral_test: %d failure(s)\n", g_failures);
    return 1;
}
