// Unit tests for pure JSON response builders (F0-1). No Win32 UI, no hardware.
#include <cstdio>
#include <string>

#include "../native/src/bridge.h"
#include "../native/src/command_json.h"
#include "../native/src/json.h"

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

const json::Value* ParseObject(const std::string& text, json::Value* out, const char* what) {
    std::string error;
    if (!json::Parse(text, out, &error)) {
        std::printf("FAIL: %s (json::Parse failed: %s)\n  raw: %s\n", what, error.c_str(),
                    text.c_str());
        ++g_failures;
        return nullptr;
    }
    if (out->type != json::Value::Type::kObject) {
        std::printf("FAIL: %s (not a JSON object)\n", what);
        ++g_failures;
        return nullptr;
    }
    return out;
}

const json::Value* ParseBridgeOk(long long id, const std::string& data_json, json::Value* root,
                                 const char* what) {
    bridge::Request req;
    req.id = id;
    req.cmd = "test";
    const std::string wrapped = bridge::Ok(req, data_json);
    if (ParseObject(wrapped, root, what) == nullptr) return nullptr;

    long long got_id = -1;
    bool ok = false;
    Expect(root->GetInt("id", &got_id) && got_id == id, what);
    Expect(root->GetBool("ok", &ok) && ok, what);

    const json::Value* data = root->Find("data");
    Expect(data != nullptr && data->type == json::Value::Type::kObject, what);
    return (data != nullptr && data->type == json::Value::Type::kObject) ? data : nullptr;
}

void ExpectIntField(const json::Value* obj, const char* key, long long expected, const char* what) {
    if (obj == nullptr) return;
    long long actual = 0;
    const json::Value* field = obj->Find(key);
    Expect(field != nullptr && field->type == json::Value::Type::kNumber, what);
    Expect(obj->GetInt(key, &actual) && actual == expected, what);
}

void ExpectBoolField(const json::Value* obj, const char* key, bool expected, const char* what) {
    if (obj == nullptr) return;
    bool actual = !expected;
    const json::Value* field = obj->Find(key);
    Expect(field != nullptr && field->type == json::Value::Type::kBool, what);
    Expect(obj->GetBool(key, &actual) && actual == expected, what);
}

}  // namespace

int main() {
    // Field primitives quote keys properly, including escapes and UTF-8.
    ExpectEqual(command_json::BuildIntField("debounce", 8), "\"debounce\":8", "BuildIntField");
    ExpectEqual(command_json::BuildBoolField("ripple", true), "\"ripple\":true",
                "BuildBoolField true");
    ExpectEqual(command_json::BuildBoolField("motion", false), "\"motion\":false",
                "BuildBoolField false");
    {
        json::Value doc;
        const std::string obj = "{" + command_json::BuildIntField("a\"b\\c", -1) + "," +
                                command_json::BuildBoolField("\xE5\x8E\xBB\xE6\x8A\x96", true) +
                                "}";
        if (ParseObject(obj, &doc, "escaped and UTF-8 field names") != nullptr) {
            ExpectIntField(&doc, "a\"b\\c", -1, "escaped key value");
            ExpectBoolField(&doc, "\xE5\x8E\xBB\xE6\x8A\x96", true, "UTF-8 key value");
        }
    }

    // misc.set: empty, single-field, fire pair, and all fields.
    {
        command_json::MiscSetFields empty;
        const std::string data = command_json::BuildMiscSetData(empty);
        ExpectEqual(data, "{}", "BuildMiscSetData empty");
        json::Value root;
        const json::Value* parsed = ParseBridgeOk(1, data, &root, "misc.set empty wrapper");
        Expect(parsed != nullptr && parsed->object.empty(), "misc.set empty object");
    }
    {
        command_json::MiscSetFields f;
        f.have_debounce = true;
        f.debounce = 8;
        const std::string data = command_json::BuildMiscSetData(f);
        ExpectEqual(data, "{\"debounce\":8}", "BuildMiscSetData single debounce");
        json::Value root;
        const json::Value* parsed = ParseBridgeOk(2, data, &root, "misc.set debounce wrapper");
        Expect(parsed != nullptr && parsed->object.size() == 1, "misc.set debounce key count");
        ExpectIntField(parsed, "debounce", 8, "misc.set debounce value");
    }
    {
        command_json::MiscSetFields f;
        f.have_fire = true;
        f.fire_loops = 2;
        f.fire_interval = 50;
        const std::string data = command_json::BuildMiscSetData(f);
        ExpectEqual(data, "{\"fireLoops\":2,\"fireInterval\":50}", "BuildMiscSetData fire pair");
        json::Value root;
        const json::Value* parsed = ParseBridgeOk(3, data, &root, "misc.set fire wrapper");
        Expect(parsed != nullptr && parsed->object.size() == 2, "misc.set fire key count");
        ExpectIntField(parsed, "fireLoops", 2, "misc.set fireLoops");
        ExpectIntField(parsed, "fireInterval", 50, "misc.set fireInterval");
    }
    {
        command_json::MiscSetFields f;
        f.have_debounce = true;
        f.debounce = 4;
        f.have_sleep = true;
        f.sleep = 120;
        f.have_fire = true;
        f.fire_loops = 0;
        f.fire_interval = 255;
        f.have_scroll_reverse = true;
        f.scroll_reverse = true;
        const std::string data = command_json::BuildMiscSetData(f);
        ExpectEqual(
            data,
            "{\"debounce\":4,\"sleep\":120,\"fireLoops\":0,\"fireInterval\":255,\"scrollReverse\":true}",
            "BuildMiscSetData all fields");
        json::Value root;
        const json::Value* parsed = ParseBridgeOk(4, data, &root, "misc.set all wrapper");
        Expect(parsed != nullptr && parsed->object.size() == 5, "misc.set all key count");
        ExpectIntField(parsed, "debounce", 4, "misc.set all debounce");
        ExpectIntField(parsed, "sleep", 120, "misc.set all sleep");
        ExpectIntField(parsed, "fireLoops", 0, "misc.set all fireLoops");
        ExpectIntField(parsed, "fireInterval", 255, "misc.set all fireInterval");
        ExpectBoolField(parsed, "scrollReverse", true, "misc.set all scrollReverse");
    }

    // sensor.set: empty, single int/bool fields, and all fields.
    {
        command_json::SensorSetFields empty;
        const std::string data = command_json::BuildSensorSetData(empty);
        ExpectEqual(data, "{}", "BuildSensorSetData empty");
        json::Value root;
        const json::Value* parsed = ParseBridgeOk(5, data, &root, "sensor.set empty wrapper");
        Expect(parsed != nullptr && parsed->object.empty(), "sensor.set empty object");
    }
    {
        command_json::SensorSetFields f;
        f.have_lod = true;
        f.lod = 1;
        const std::string data = command_json::BuildSensorSetData(f);
        ExpectEqual(data, "{\"lod\":1}", "BuildSensorSetData single lod");
        json::Value root;
        const json::Value* parsed = ParseBridgeOk(6, data, &root, "sensor.set lod wrapper");
        Expect(parsed != nullptr && parsed->object.size() == 1, "sensor.set lod key count");
        ExpectIntField(parsed, "lod", 1, "sensor.set lod value");
    }
    {
        command_json::SensorSetFields f;
        f.have_ripple = true;
        f.ripple = true;
        const std::string data = command_json::BuildSensorSetData(f);
        ExpectEqual(data, "{\"ripple\":true}", "BuildSensorSetData single bool");
        json::Value root;
        const json::Value* parsed = ParseBridgeOk(7, data, &root, "sensor.set ripple wrapper");
        Expect(parsed != nullptr && parsed->object.size() == 1, "sensor.set ripple key count");
        ExpectBoolField(parsed, "ripple", true, "sensor.set ripple value");
    }
    {
        command_json::SensorSetFields f;
        f.have_lod = true;
        f.lod = 0;
        f.have_ripple = true;
        f.ripple = false;
        f.have_fixline = true;
        f.fixline = true;
        f.have_motion = true;
        f.motion = false;
        f.have_mode = true;
        f.mode = 2;
        const std::string data = command_json::BuildSensorSetData(f);
        ExpectEqual(data,
                    "{\"lod\":0,\"ripple\":false,\"fixline\":true,\"motion\":false,\"mode\":2}",
                    "BuildSensorSetData all fields");
        json::Value root;
        const json::Value* parsed = ParseBridgeOk(8, data, &root, "sensor.set all wrapper");
        Expect(parsed != nullptr && parsed->object.size() == 5, "sensor.set all key count");
        ExpectIntField(parsed, "lod", 0, "sensor.set all lod");
        ExpectBoolField(parsed, "ripple", false, "sensor.set all ripple");
        ExpectBoolField(parsed, "fixline", true, "sensor.set all fixline");
        ExpectBoolField(parsed, "motion", false, "sensor.set all motion");
        ExpectIntField(parsed, "mode", 2, "sensor.set all mode");
    }

    // dpi.set: equal-axis and split-axis, plus dpi.stage.
    {
        const std::string eq = command_json::BuildDpiSetData(2, 1600, false);
        ExpectEqual(eq, "{\"slot\":2,\"dpi\":1600,\"dpiX\":1600,\"dpiY\":1600}",
                    "BuildDpiSetData equal-axis");
        json::Value root;
        const json::Value* parsed = ParseBridgeOk(9, eq, &root, "dpi.set equal-axis wrapper");
        Expect(parsed != nullptr && parsed->object.size() == 4, "dpi.set equal-axis key count");
        ExpectIntField(parsed, "slot", 2, "dpi.set equal-axis slot");
        ExpectIntField(parsed, "dpi", 1600, "dpi.set equal-axis dpi");
        ExpectIntField(parsed, "dpiX", 1600, "dpi.set equal-axis dpiX");
        ExpectIntField(parsed, "dpiY", 1600, "dpi.set equal-axis dpiY");

        const std::string split = command_json::BuildDpiSetData(2, 1600, true);
        ExpectEqual(split, "{\"slot\":2,\"dpiX\":null,\"dpiY\":1600,\"unverified\":[\"dpiX\"]}",
                    "BuildDpiSetData split-axis");
        const json::Value* split_parsed =
            ParseBridgeOk(10, split, &root, "dpi.set split-axis wrapper");
        Expect(split_parsed != nullptr && split_parsed->object.size() == 4,
               "dpi.set split-axis key count");
        ExpectIntField(split_parsed, "slot", 2, "dpi.set split-axis slot");
        const json::Value* x_val = split_parsed != nullptr ? split_parsed->Find("dpiX") : nullptr;
        Expect(x_val != nullptr && x_val->type == json::Value::Type::kNull,
               "dpi.set split-axis dpiX is null");
        ExpectIntField(split_parsed, "dpiY", 1600, "dpi.set split-axis dpiY");
        const json::Value* unv =
            split_parsed != nullptr ? split_parsed->Find("unverified") : nullptr;
        Expect(unv != nullptr && unv->type == json::Value::Type::kArray &&
                   unv->array.size() == 1 && unv->array[0].type == json::Value::Type::kString &&
                   unv->array[0].text == "dpiX",
               "dpi.set split-axis unverified array");
    }
    {
        const std::string stage = command_json::BuildDpiStageData(3, 6);
        ExpectEqual(stage, "{\"active\":3,\"count\":6}", "BuildDpiStageData");
        json::Value root;
        const json::Value* parsed = ParseBridgeOk(11, stage, &root, "dpi.stage wrapper");
        Expect(parsed != nullptr && parsed->object.size() == 2, "dpi.stage key count");
        ExpectIntField(parsed, "active", 3, "dpi.stage active");
        ExpectIntField(parsed, "count", 6, "dpi.stage count");
    }

    // WP4 + Hub preferences: hub.get/hub.set and led.get/led.set builders.
    {
        const std::string hub = command_json::BuildHubSettingsData(true, false);
        json::Value root;
        const json::Value* parsed = ParseBridgeOk(12, hub, &root, "hub settings wrapper");
        Expect(parsed != nullptr && parsed->object.size() == 12, "hub settings key count");
        ExpectBoolField(parsed, "taskbarWidget", true, "hub taskbarWidget");
        ExpectBoolField(parsed, "autoStart", false, "hub autoStart");
        ExpectBoolField(parsed, "widgetStacked", false, "hub widgetStacked");
        ExpectBoolField(parsed, "widgetBackground", false, "hub widgetBackground");
        ExpectBoolField(parsed, "brandAccent", false, "hub brandAccent");
        ExpectBoolField(parsed, "lowBatteryNotify", true, "hub lowBatteryNotify default");
        ExpectIntField(parsed, "lowBatteryThreshold", 15, "hub lowBatteryThreshold");
        ExpectIntField(parsed, "pollIntervalSec", 30, "hub pollIntervalSec");
        std::string mon;
        Expect(parsed != nullptr && parsed->GetString("widgetMonitor", &mon) && mon == "all",
               "hub widgetMonitor default");

        command_json::HubSettingsFields custom;
        custom.taskbar_widget = true;
        custom.auto_start = true;
        custom.widget_stacked = true;
        custom.widget_background = true;
        custom.widget_monitor = "main";
        custom.monitors.push_back({"mon-2", "Display 2 (right)"});
        custom.brand_accent = true;
        custom.low_battery_notify = false;
        custom.low_battery_threshold = 20;
        custom.poll_interval_sec = 60;
        custom.hidden_devices.push_back("bt:1122");
        custom.device_aliases["ksnake:A8A5:2255"] = "My Mouse";
        const std::string custom_json = command_json::BuildHubSettingsData(custom);
        const json::Value* parsed_custom =
            ParseBridgeOk(120, custom_json, &root, "hub custom settings wrapper");
        ExpectBoolField(parsed_custom, "widgetStacked", true, "hub custom widgetStacked");
        ExpectBoolField(parsed_custom, "widgetBackground", true, "hub custom widgetBackground");
        ExpectBoolField(parsed_custom, "brandAccent", true, "hub custom brandAccent");
        ExpectBoolField(parsed_custom, "lowBatteryNotify", false, "hub custom lowBatteryNotify");
        ExpectIntField(parsed_custom, "lowBatteryThreshold", 20, "hub custom threshold");
        ExpectIntField(parsed_custom, "pollIntervalSec", 60, "hub custom pollIntervalSec");
        const json::Value* hidden =
            parsed_custom != nullptr ? parsed_custom->Find("hiddenDevices") : nullptr;
        Expect(hidden != nullptr && hidden->type == json::Value::Type::kArray &&
                   hidden->array.size() == 1 && hidden->array[0].text == "bt:1122",
               "hub custom hiddenDevices");
        const json::Value* aliases =
            parsed_custom != nullptr ? parsed_custom->Find("deviceAliases") : nullptr;
        std::string alias_name;
        Expect(aliases != nullptr && aliases->type == json::Value::Type::kObject &&
                   aliases->GetString("ksnake:A8A5:2255", &alias_name) && alias_name == "My Mouse",
               "hub custom deviceAliases");
    }
    {
        const std::string unsup = command_json::BuildReceiverLedData(false, 1);
        ExpectEqual(unsup, "{\"supported\":false,\"mode\":null}",
                    "BuildReceiverLedData unsupported keeps mode null");
        json::Value root;
        const json::Value* parsed = ParseBridgeOk(13, unsup, &root, "led unsupported wrapper");
        ExpectBoolField(parsed, "supported", false, "led unsupported flag");
        const json::Value* mode_val = parsed != nullptr ? parsed->Find("mode") : nullptr;
        Expect(mode_val != nullptr && mode_val->type == json::Value::Type::kNull,
               "led unsupported mode null");

        const std::string sup_unread = command_json::BuildReceiverLedData(true, -1);
        ExpectEqual(sup_unread, "{\"supported\":true,\"mode\":null}",
                    "BuildReceiverLedData supported unread");
        const std::string sup_ok = command_json::BuildReceiverLedData(true, 2);
        ExpectEqual(sup_ok, "{\"supported\":true,\"mode\":2}", "BuildReceiverLedData supported ok");
        const json::Value* parsed_ok = ParseBridgeOk(14, sup_ok, &root, "led supported wrapper");
        ExpectBoolField(parsed_ok, "supported", true, "led supported flag");
        ExpectIntField(parsed_ok, "mode", 2, "led supported mode");

        const std::string mouse_led = command_json::BuildLedData(true, "mouse", 4);
        ExpectEqual(mouse_led, "{\"supported\":true,\"kind\":\"mouse\",\"mode\":4}",
                    "BuildLedData mouse led");
    }

    // ROYUAN / AKOS BC98 kbd.get / kbd.set data builder (full + null fields)
    {
        command_json::KbdStateFields full;
        full.dev_id = 942;
        full.model = "AKOS BC98";
        full.firmware = "v1.2";
        full.have_profile = true;
        full.profile = 0;
        full.have_options = true;
        full.options.win_lock = true;
        full.have_main_led = true;
        full.main_led.has_on = true;
        full.main_led.on = true;
        full.main_led.mode = 12;
        full.main_led.brightness = 3;
        full.main_led.speed = 2;
        full.main_led.colorful = true;
        full.main_led.r = 255;
        full.main_led.g = 255;
        full.main_led.b = 255;
        full.have_side_led = true;
        full.side_led.has_on = true;
        full.side_led.on = true;
        full.side_led.mode = 1;
        full.side_led.brightness = 4;
        full.side_led.speed = 3;
        full.side_led.colorful = false;
        full.side_led.r = 0;
        full.side_led.g = 255;
        full.side_led.b = 255;
        full.have_debounce = true;
        full.debounce = 1;
        full.have_sleep = true;
        full.sleep.sleep_bt = 120;
        full.sleep.sleep_24g = 120;
        full.sleep.deep_bt = 1680;
        full.sleep.deep_24g = 1680;

        const std::string kbd_json = command_json::BuildKbdStateData(full);
        json::Value root;
        const json::Value* parsed = ParseBridgeOk(15, kbd_json, &root, "kbd.get full wrapper");
        ExpectIntField(parsed, "devId", 942, "kbd.get devId");
        ExpectIntField(parsed, "profile", 0, "kbd.get profile");
        ExpectIntField(parsed, "debounce", 1, "kbd.get debounce");
        const json::Value* opt = parsed != nullptr ? parsed->Find("options") : nullptr;
        ExpectBoolField(opt, "winLock", true, "kbd.get options.winLock");
        const json::Value* ml = parsed != nullptr ? parsed->Find("mainLed") : nullptr;
        ExpectIntField(ml, "mode", 12, "kbd.get mainLed.mode");
        const json::Value* sl = parsed != nullptr ? parsed->Find("sideLed") : nullptr;
        std::string side_color;
        Expect(sl != nullptr && sl->GetString("color", &side_color) && side_color == "#00FFFF",
               "kbd.get sideLed.color #00FFFF");
        const json::Value* slp = parsed != nullptr ? parsed->Find("sleep") : nullptr;
        ExpectIntField(slp, "deep24g", 1680, "kbd.get sleep.deep24g");

        command_json::KbdStateFields partial;
        partial.dev_id = 942;
        partial.model = "AKOS BC98";
        const std::string partial_json = command_json::BuildKbdStateData(partial);
        const json::Value* p_parsed =
            ParseBridgeOk(16, partial_json, &root, "kbd.get partial null wrapper");
        const json::Value* p_opt = p_parsed != nullptr ? p_parsed->Find("options") : nullptr;
        Expect(p_opt != nullptr && p_opt->type == json::Value::Type::kNull,
               "kbd.get unread options is null");
        const json::Value* p_deb = p_parsed != nullptr ? p_parsed->Find("debounce") : nullptr;
        Expect(p_deb != nullptr && p_deb->type == json::Value::Type::kNull,
               "kbd.get unread debounce is null");
    }

    // ROYUAN / AKOS BC98 keymap & macro JSON builders
    {
        std::vector<std::array<int, 4>> base(128, {0, 0, 0, 0});
        std::vector<std::array<int, 4>> fn(128, {0, 0, 0, 0});
        base[0] = {0, 0, 41, 0};
        fn[0] = {10, 2, 0, 0};
        const std::string km_json = command_json::BuildKbdKeymapData(1, base, fn);
        json::Value root;
        const json::Value* km_parsed = ParseBridgeOk(17, km_json, &root, "kbd.keys.get wrapper");
        ExpectIntField(km_parsed, "profile", 1, "kbd.keys.get profile");
        const json::Value* base_arr = km_parsed != nullptr ? km_parsed->Find("base") : nullptr;
        Expect(base_arr != nullptr && base_arr->type == json::Value::Type::kArray &&
                   base_arr->array.size() == 128,
               "kbd.keys.get base length 128");

        const std::string ks_json =
            command_json::BuildKbdKeySetData(0, true, 90, {13, 5, 1, 0}, false);
        const json::Value* ks_parsed = ParseBridgeOk(18, ks_json, &root, "kbd.keys.set wrapper");
        ExpectBoolField(ks_parsed, "fnLayer", true, "kbd.keys.set fnLayer");
        ExpectIntField(ks_parsed, "slot", 90, "kbd.keys.set slot");
        ExpectBoolField(ks_parsed, "bulkUsed", false, "kbd.keys.set bulkUsed");

        std::vector<command_json::KbdMacroSummaryItem> summaries = {
            {0, "测试宏", false, 2, 4, 8},
            {1, "M2", true, 0, 0, 0},
        };
        const std::string ml_json = command_json::BuildKbdMacroListData(50, summaries);
        const json::Value* ml_parsed = ParseBridgeOk(19, ml_json, &root, "kbd.macro.list wrapper");
        ExpectIntField(ml_parsed, "maxSlots", 50, "kbd.macro.list maxSlots");

        std::vector<command_json::KbdMacroEventItem> evs = {
            {"key", 4, 0, true, 0, 0, 20},
            {"key", 4, 0, false, 0, 0, 20},
            {"mouseButton", 0, 1, true, 0, 0, 10},
            {"mouseMove", 0, 0, false, -5, 12, 15},
        };
        const std::string m_json = command_json::BuildKbdMacroData(0, "Alpha\"1", false, 2, 12, evs);
        const json::Value* m_parsed = ParseBridgeOk(20, m_json, &root, "kbd.macro.get wrapper");
        ExpectIntField(m_parsed, "byteLength", 12, "kbd.macro.get byteLength");

        const std::string md_json = command_json::BuildKbdMacroDeleteData(49);
        const json::Value* md_parsed = ParseBridgeOk(21, md_json, &root, "kbd.macro.delete wrapper");
        ExpectIntField(md_parsed, "slot", 49, "kbd.macro.delete slot");
        ExpectBoolField(md_parsed, "deleted", true, "kbd.macro.delete deleted");
    }

    if (g_failures == 0) {
        std::printf(
            "command_json_test: OK (primitives, misc.set, sensor.set, dpi.set/stage, hub/led, kbd, json::Parse)\n");
        return 0;
    }
    std::printf("command_json_test: %d failure(s)\n", g_failures);
    return 1;
}
