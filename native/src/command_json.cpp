#include "command_json.h"

#include <cstdio>

#include "bridge.h"

namespace command_json {
namespace {

void AppendField(std::string* out, const std::string& field) {
    if (!out->empty()) *out += ',';
    *out += field;
}

std::string FormatHexRgb(int r, int g, int b) {
    auto clamp = [](int v) { return v < 0 ? 0 : (v > 255 ? 255 : v); };
    char buf[16]{};
    std::snprintf(buf, sizeof(buf), "#%02X%02X%02X", clamp(r), clamp(g), clamp(b));
    return buf;
}

std::string BuildKbdLedObject(const KbdLedFields& led) {
    std::string body;
    if (led.has_on) {
        AppendField(&body, BuildBoolField("on", led.on));
    } else {
        AppendField(&body, bridge::JsonQuote("on") + ":null");
    }
    AppendField(&body, BuildIntField("mode", led.mode));
    AppendField(&body, BuildIntField("brightness", led.brightness));
    AppendField(&body, BuildIntField("speed", led.speed));
    AppendField(&body, BuildBoolField("colorful", led.colorful));
    AppendField(&body, bridge::JsonQuote("color") + ":" +
                           bridge::JsonQuote(FormatHexRgb(led.r, led.g, led.b)));
    AppendField(&body, BuildIntField("direction", led.direction));
    return "{" + body + "}";
}

}  // namespace

std::string BuildIntField(const std::string& name, int value) {
    return bridge::JsonQuote(name) + ":" + std::to_string(value);
}

std::string BuildBoolField(const std::string& name, bool value) {
    return bridge::JsonQuote(name) + ":" + (value ? "true" : "false");
}

std::string BuildMiscSetData(const MiscSetFields& fields) {
    std::string body;
    if (fields.have_debounce) AppendField(&body, BuildIntField("debounce", fields.debounce));
    if (fields.have_sleep) AppendField(&body, BuildIntField("sleep", fields.sleep));
    if (fields.have_fire) {
        AppendField(&body, BuildIntField("fireLoops", fields.fire_loops));
        AppendField(&body, BuildIntField("fireInterval", fields.fire_interval));
    }
    if (fields.have_scroll_reverse) {
        AppendField(&body, BuildBoolField("scrollReverse", fields.scroll_reverse));
    }
    return "{" + body + "}";
}

std::string BuildSensorSetData(const SensorSetFields& fields) {
    std::string body;
    if (fields.have_lod) AppendField(&body, BuildIntField("lod", fields.lod));
    if (fields.have_ripple) AppendField(&body, BuildBoolField("ripple", fields.ripple));
    if (fields.have_fixline) AppendField(&body, BuildBoolField("fixline", fields.fixline));
    if (fields.have_motion) AppendField(&body, BuildBoolField("motion", fields.motion));
    if (fields.have_mode) AppendField(&body, BuildIntField("mode", fields.mode));
    return "{" + body + "}";
}

std::string BuildDpiSetData(int slot, int readback, bool split_axis) {
    std::string body = BuildIntField("slot", slot);
    if (split_axis) {
        AppendField(&body, "\"dpiX\":null");
        AppendField(&body, BuildIntField("dpiY", readback));
        AppendField(&body, "\"unverified\":[\"dpiX\"]");
    } else {
        AppendField(&body, BuildIntField("dpi", readback));
        AppendField(&body, BuildIntField("dpiX", readback));
        AppendField(&body, BuildIntField("dpiY", readback));
    }
    return "{" + body + "}";
}

std::string BuildDpiStageData(int active, int count) {
    return "{" + BuildIntField("active", active) + "," + BuildIntField("count", count) + "}";
}

std::string BuildHubSettingsData(const HubSettingsFields& fields) {
    std::string body;
    AppendField(&body, BuildBoolField("taskbarWidget", fields.taskbar_widget));
    AppendField(&body, BuildBoolField("autoStart", fields.auto_start));
    AppendField(&body, BuildBoolField("widgetStacked", fields.widget_stacked));
    AppendField(&body, BuildBoolField("widgetBackground", fields.widget_background));
    AppendField(&body, "\"widgetMonitor\":" + bridge::JsonQuote(fields.widget_monitor));

    std::string monitors_json = "[";
    for (std::size_t i = 0; i < fields.monitors.size(); ++i) {
        if (i > 0) monitors_json += ',';
        monitors_json += "{\"id\":" + bridge::JsonQuote(fields.monitors[i].id) +
                         ",\"label\":" + bridge::JsonQuote(fields.monitors[i].label) + "}";
    }
    monitors_json += "]";
    AppendField(&body, "\"monitors\":" + monitors_json);

    AppendField(&body, BuildBoolField("brandAccent", fields.brand_accent));
    AppendField(&body, BuildBoolField("lowBatteryNotify", fields.low_battery_notify));
    AppendField(&body, BuildIntField("lowBatteryThreshold", fields.low_battery_threshold));
    AppendField(&body, BuildIntField("pollIntervalSec", fields.poll_interval_sec));

    std::string hidden_json = "[";
    for (std::size_t i = 0; i < fields.hidden_devices.size(); ++i) {
        if (i > 0) hidden_json += ',';
        hidden_json += bridge::JsonQuote(fields.hidden_devices[i]);
    }
    hidden_json += "]";
    AppendField(&body, "\"hiddenDevices\":" + hidden_json);

    std::string aliases_json = "{";
    bool first_alias = true;
    for (const auto& kv : fields.device_aliases) {
        if (kv.first.empty() || kv.second.empty()) continue;
        if (!first_alias) aliases_json += ',';
        first_alias = false;
        aliases_json += bridge::JsonQuote(kv.first) + ":" + bridge::JsonQuote(kv.second);
    }
    aliases_json += "}";
    AppendField(&body, "\"deviceAliases\":" + aliases_json);

    return "{" + body + "}";
}

std::string BuildHubSettingsData(bool taskbar_widget, bool auto_start) {
    HubSettingsFields fields;
    fields.taskbar_widget = taskbar_widget;
    fields.auto_start = auto_start;
    return BuildHubSettingsData(fields);
}

std::string BuildReceiverLedData(bool supported, int mode) {
    std::string body = BuildBoolField("supported", supported);
    AppendField(&body, (supported && mode >= 0) ? BuildIntField("mode", mode) : "\"mode\":null");
    return "{" + body + "}";
}

std::string BuildLedData(bool supported, const std::string& kind, int mode) {
    std::string body = BuildBoolField("supported", supported);
    if (!kind.empty()) {
        AppendField(&body, "\"kind\":" + bridge::JsonQuote(kind));
    }
    AppendField(&body, (supported && mode >= 0) ? BuildIntField("mode", mode) : "\"mode\":null");
    return "{" + body + "}";
}

std::string BuildKbdStateData(const KbdStateFields& fields) {
    std::string body;
    AppendField(&body, BuildIntField("devId", fields.dev_id));
    AppendField(&body, bridge::JsonQuote("model") + ":" + bridge::JsonQuote(fields.model));
    AppendField(&body, bridge::JsonQuote("firmware") + ":" +
                           (fields.firmware.empty() ? std::string("null")
                                                    : bridge::JsonQuote(fields.firmware)));
    AppendField(&body, bridge::JsonQuote("profile") + ":" +
                           (fields.have_profile ? std::to_string(fields.profile)
                                                : std::string("null")));
    if (fields.have_options) {
        std::string opt;
        AppendField(&opt, BuildBoolField("winLock", fields.options.win_lock));
        AppendField(&opt, BuildBoolField("macMode", fields.options.mac_mode));
        AppendField(&opt, BuildBoolField("wasdSwap", fields.options.wasd_swap));
        AppendField(&opt, BuildBoolField("backlightOff", fields.options.backlight_off));
        AppendField(&opt, BuildBoolField("sidelightOff", fields.options.sidelight_off));
        AppendField(&body, bridge::JsonQuote("options") + ":{" + opt + "}");
    } else {
        AppendField(&body, bridge::JsonQuote("options") + ":null");
    }

    AppendField(&body, bridge::JsonQuote("mainLed") + ":" +
                           (fields.have_main_led ? BuildKbdLedObject(fields.main_led)
                                                 : std::string("null")));
    AppendField(&body, bridge::JsonQuote("sideLed") + ":" +
                           (fields.have_side_led ? BuildKbdLedObject(fields.side_led)
                                                 : std::string("null")));
    AppendField(&body, bridge::JsonQuote("debounce") + ":" +
                           (fields.have_debounce ? std::to_string(fields.debounce)
                                                 : std::string("null")));
    if (fields.have_sleep) {
        std::string slp;
        AppendField(&slp, BuildIntField("sleepBt", fields.sleep.sleep_bt));
        AppendField(&slp, BuildIntField("sleep24g", fields.sleep.sleep_24g));
        AppendField(&slp, BuildIntField("deepBt", fields.sleep.deep_bt));
        AppendField(&slp, BuildIntField("deep24g", fields.sleep.deep_24g));
        AppendField(&body, bridge::JsonQuote("sleep") + ":{" + slp + "}");
    } else {
        AppendField(&body, bridge::JsonQuote("sleep") + ":null");
    }
    return "{" + body + "}";
}

namespace {

std::string FormatKeyEntryTuple(const std::array<int, 4>& e) {
    return "[" + std::to_string(e[0]) + "," + std::to_string(e[1]) + "," +
           std::to_string(e[2]) + "," + std::to_string(e[3]) + "]";
}

std::string FormatKeyEntryArray(const std::vector<std::array<int, 4>>& entries) {
    std::string out = "[";
    for (std::size_t i = 0; i < entries.size(); ++i) {
        if (i > 0) out += ',';
        out += FormatKeyEntryTuple(entries[i]);
    }
    out += "]";
    return out;
}

}  // namespace

std::string BuildKbdKeymapData(int profile,
                               const std::vector<std::array<int, 4>>& base_entries,
                               const std::vector<std::array<int, 4>>& fn_entries) {
    std::string body;
    AppendField(&body, BuildIntField("profile", profile));
    AppendField(&body, bridge::JsonQuote("base") + ":" + FormatKeyEntryArray(base_entries));
    AppendField(&body, bridge::JsonQuote("fn") + ":" + FormatKeyEntryArray(fn_entries));
    return "{" + body + "}";
}

std::string BuildKbdKeySetData(int profile,
                               bool fn_layer,
                               int slot,
                               const std::array<int, 4>& entry,
                               bool bulk_used) {
    std::string body;
    AppendField(&body, BuildIntField("profile", profile));
    AppendField(&body, BuildBoolField("fnLayer", fn_layer));
    AppendField(&body, BuildIntField("slot", slot));
    AppendField(&body, bridge::JsonQuote("entry") + ":" + FormatKeyEntryTuple(entry));
    AppendField(&body, BuildBoolField("bulkUsed", bulk_used));
    return "{" + body + "}";
}

std::string BuildKbdMacroListData(int max_slots,
                                  const std::vector<KbdMacroSummaryItem>& items) {
    std::string body;
    AppendField(&body, BuildIntField("maxSlots", max_slots));
    std::string arr = "[";
    for (std::size_t i = 0; i < items.size(); ++i) {
        if (i > 0) arr += ',';
        const KbdMacroSummaryItem& it = items[i];
        std::string obj;
        AppendField(&obj, BuildIntField("slot", it.slot));
        AppendField(&obj, bridge::JsonQuote("name") + ":" + bridge::JsonQuote(it.name));
        AppendField(&obj, BuildBoolField("empty", it.empty));
        AppendField(&obj, BuildIntField("repeat", it.repeat));
        AppendField(&obj, BuildIntField("eventCount", it.event_count));
        AppendField(&obj, BuildIntField("byteLength", it.byte_length));
        arr += "{" + obj + "}";
    }
    arr += "]";
    AppendField(&body, bridge::JsonQuote("slots") + ":" + arr);
    return "{" + body + "}";
}

std::string BuildKbdMacroData(int slot,
                              const std::string& name,
                              bool empty,
                              int repeat,
                              int byte_length,
                              const std::vector<KbdMacroEventItem>& events) {
    std::string body;
    AppendField(&body, BuildIntField("slot", slot));
    AppendField(&body, bridge::JsonQuote("name") + ":" + bridge::JsonQuote(name));
    AppendField(&body, BuildBoolField("empty", empty));
    AppendField(&body, BuildIntField("repeat", repeat));
    AppendField(&body, BuildIntField("byteLength", byte_length));
    std::string arr = "[";
    for (std::size_t i = 0; i < events.size(); ++i) {
        if (i > 0) arr += ',';
        const KbdMacroEventItem& ev = events[i];
        std::string obj;
        AppendField(&obj, bridge::JsonQuote("kind") + ":" + bridge::JsonQuote(ev.kind));
        if (ev.kind == "key") {
            AppendField(&obj, BuildIntField("usage", ev.usage));
            AppendField(&obj, BuildBoolField("pressed", ev.pressed));
        } else if (ev.kind == "mouseButton") {
            AppendField(&obj, BuildIntField("button", ev.button));
            AppendField(&obj, BuildBoolField("pressed", ev.pressed));
        } else if (ev.kind == "mouseMove") {
            AppendField(&obj, BuildIntField("dx", ev.dx));
            AppendField(&obj, BuildIntField("dy", ev.dy));
        }
        AppendField(&obj, BuildIntField("delayMs", ev.delay_ms));
        arr += "{" + obj + "}";
    }
    arr += "]";
    AppendField(&body, bridge::JsonQuote("events") + ":" + arr);
    return "{" + body + "}";
}

std::string BuildKbdMacroDeleteData(int slot) {
    std::string body;
    AppendField(&body, BuildIntField("slot", slot));
    AppendField(&body, BuildBoolField("deleted", true));
    return "{" + body + "}";
}

}  // namespace command_json
