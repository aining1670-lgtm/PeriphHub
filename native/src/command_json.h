#pragma once
// Pure JSON response builders for hardware bridge commands.
// Keeps host handlers free of hand-assembled JSON formatting bugs.
#include <array>
#include <map>
#include <string>
#include <vector>

namespace command_json {

// Serializes one `"name":value` integer JSON object field (key quoted via bridge::JsonQuote).
std::string BuildIntField(const std::string& name, int value);

// Serializes one `"name":true|false` boolean JSON object field.
std::string BuildBoolField(const std::string& name, bool value);

struct MiscSetFields {
    bool have_debounce = false;
    int debounce = 0;
    bool have_sleep = false;
    int sleep = 0;
    bool have_fire = false;
    int fire_loops = 0;
    int fire_interval = 0;
    bool have_scroll_reverse = false;
    bool scroll_reverse = false;
};

// Builds the JSON `data` object for a verified `misc.set` response.
std::string BuildMiscSetData(const MiscSetFields& fields);

struct SensorSetFields {
    bool have_lod = false;
    int lod = 0;
    bool have_ripple = false;
    bool ripple = false;
    bool have_fixline = false;
    bool fixline = false;
    bool have_motion = false;
    bool motion = false;
    bool have_mode = false;
    int mode = 0;
};

// Builds the JSON `data` object for a verified `sensor.set` response.
std::string BuildSensorSetData(const SensorSetFields& fields);

// Builds the JSON `data` object for a verified `dpi.set` response.
// `split_axis` emits `{"slot":N,"dpiX":null,"dpiY":M,"unverified":["dpiX"]}`;
// otherwise emits `{"slot":N,"dpi":M,"dpiX":M,"dpiY":M}`.
std::string BuildDpiSetData(int slot, int readback, bool split_axis);

// Builds the JSON `data` object for a verified `dpi.stage` response.
std::string BuildDpiStageData(int active, int count);

struct HubMonitorInfo {
    std::string id;
    std::string label;
};

struct HubSettingsFields {
    bool taskbar_widget = true;
    bool auto_start = false;
    bool widget_stacked = false;
    bool widget_background = false;
    std::string widget_monitor = "all";
    std::vector<HubMonitorInfo> monitors;
    bool brand_accent = false;
    bool low_battery_notify = true;
    int low_battery_threshold = 15;
    int poll_interval_sec = 30;
    std::vector<std::string> hidden_devices;
    std::map<std::string, std::string> device_aliases;
};

// Builds the JSON `data` object for `hub.get` / `hub.set`.
std::string BuildHubSettingsData(const HubSettingsFields& fields);
std::string BuildHubSettingsData(bool taskbar_widget, bool auto_start);

// Builds the JSON `data` object for `led.get` / `led.set`.
// When `supported` is false, emits `{"supported":false,"mode":null}`.
std::string BuildReceiverLedData(bool supported, int mode);

// Builds the JSON `data` object for `led.get` / `led.set` with an explicit kind
// ("receiver" or "mouse"): `{"supported":true,"kind":"mouse","mode":N}`.
std::string BuildLedData(bool supported, const std::string& kind, int mode);

struct KbdOptionsFields {
    bool win_lock = false;
    bool mac_mode = false;
    bool wasd_swap = false;
    bool backlight_off = false;
    bool sidelight_off = false;
};

struct KbdLedFields {
    bool has_on = false;
    bool on = true;
    int mode = 1;
    int brightness = 3;
    int speed = 2;
    bool colorful = true;
    int r = 255;
    int g = 255;
    int b = 255;
    int direction = 0;
};

struct KbdSleepFields {
    int sleep_bt = 120;
    int sleep_24g = 120;
    int deep_bt = 1680;
    int deep_24g = 1680;
};

struct KbdStateFields {
    int dev_id = 0;
    std::string model;
    std::string firmware;
    bool have_profile = false;
    int profile = 0;
    bool have_options = false;
    KbdOptionsFields options{};
    bool have_main_led = false;
    KbdLedFields main_led{};
    bool have_side_led = false;
    KbdLedFields side_led{};
    bool have_debounce = false;
    int debounce = -1;
    bool have_sleep = false;
    KbdSleepFields sleep{};
};

// Builds the JSON `data` object for `kbd.get` / `kbd.set`.
std::string BuildKbdStateData(const KbdStateFields& fields);

// Builds the JSON `data` object for `kbd.keys.get` / `kbd.keys.reset`.
std::string BuildKbdKeymapData(int profile,
                               const std::vector<std::array<int, 4>>& base_entries,
                               const std::vector<std::array<int, 4>>& fn_entries);

// Builds the JSON `data` object for `kbd.keys.set`.
std::string BuildKbdKeySetData(int profile,
                               bool fn_layer,
                               int slot,
                               const std::array<int, 4>& entry,
                               bool bulk_used);

struct KbdMacroSummaryItem {
    int slot = 0;
    std::string name;
    bool empty = true;
    int repeat = 0;
    int event_count = 0;
    int byte_length = 0;
};

// Builds the JSON `data` object for `kbd.macro.list`.
std::string BuildKbdMacroListData(int max_slots,
                                  const std::vector<KbdMacroSummaryItem>& items);

struct KbdMacroEventItem {
    std::string kind;  // "key", "mouseButton", "mouseMove"
    int usage = 0;
    int button = 0;
    bool pressed = false;
    int dx = 0;
    int dy = 0;
    int delay_ms = 0;
};

// Builds the JSON `data` object for `kbd.macro.get` / `kbd.macro.set`.
std::string BuildKbdMacroData(int slot,
                              const std::string& name,
                              bool empty,
                              int repeat,
                              int byte_length,
                              const std::vector<KbdMacroEventItem>& events);

// Builds the JSON `data` object for `kbd.macro.delete`.
std::string BuildKbdMacroDeleteData(int slot);

}  // namespace command_json
