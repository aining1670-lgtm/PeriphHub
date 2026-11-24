#include "drivers/royuan_driver.h"

#include <array>
#include <cctype>
#include <cstdint>
#include <cstdlib>
#include <map>
#include <string>
#include <utility>
#include <vector>

#include "bridge_args.h"
#include "command_json.h"
#include "driver_router.h"
#include "drivers/host.h"
#include "royuan_hid.h"
#include "royuan_protocol.h"

namespace royuan_driver {
namespace {

using bridge::ArgState;
using bridge::OptBoolArg;
using bridge::OptIntArg;

bool OpenRoyuanDeviceForKey(const std::string& device_key, royuan::Device* dev, std::string* error) {
    std::uint16_t preferred_pid = 0;
    driver_router::ParseRoyuanPidFromKey(device_key, &preferred_pid);
    return dev->Open(preferred_pid, error);
}

command_json::KbdStateFields ToKbdStateFields(const royuan::FullState& st) {
    command_json::KbdStateFields f;
    f.dev_id = static_cast<int>(st.dev_id);
    f.model = royuan::ResolveModelName(st.dev_id, "");
    f.firmware = st.firmware;
    f.have_profile = st.have_profile;
    f.profile = st.profile;
    f.have_options = st.have_options;
    if (st.have_options) {
        f.options.win_lock = st.options.win_lock;
        f.options.mac_mode = st.options.mac_mode;
        f.options.wasd_swap = st.options.wasd_swap;
        f.options.backlight_off = st.options.backlight_off;
        f.options.sidelight_off = st.options.sidelight_off;
    }
    f.have_main_led = st.have_main_led;
    if (st.have_main_led) {
        f.main_led.has_on = st.have_options;
        f.main_led.on = st.have_options ? !st.options.backlight_off : true;
        f.main_led.mode = st.main_led.mode;
        f.main_led.brightness = st.main_led.brightness;
        f.main_led.speed = st.main_led.speed;
        f.main_led.colorful = st.main_led.colorful;
        f.main_led.r = st.main_led.r;
        f.main_led.g = st.main_led.g;
        f.main_led.b = st.main_led.b;
        f.main_led.direction = st.main_led.direction;
    }
    f.have_side_led = st.have_side_led;
    if (st.have_side_led) {
        f.side_led.has_on = st.have_options;
        f.side_led.on = st.have_options ? (!st.options.sidelight_off && st.side_led.mode != 0)
                                        : (st.side_led.mode != 0);
        f.side_led.mode = st.side_led.mode;
        f.side_led.brightness = st.side_led.brightness;
        f.side_led.speed = st.side_led.speed;
        f.side_led.colorful = st.side_led.colorful;
        f.side_led.r = st.side_led.r;
        f.side_led.g = st.side_led.g;
        f.side_led.b = st.side_led.b;
        f.side_led.direction = st.side_led.direction;
    }
    f.have_debounce = st.have_debounce;
    f.debounce = st.debounce;
    f.have_sleep = st.have_sleep;
    if (st.have_sleep) {
        f.sleep.sleep_bt = st.sleep.sleep_bt;
        f.sleep.sleep_24g = st.sleep.sleep_24g;
        f.sleep.deep_bt = st.sleep.deep_bt;
        f.sleep.deep_24g = st.sleep.deep_24g;
    }
    return f;
}

bool ParseHexColorString(const std::string& text, int* r, int* g, int* b) {
    if (text.size() != 7 || text[0] != '#') return false;
    for (std::size_t i = 1; i < 7; ++i) {
        if (!std::isxdigit(static_cast<unsigned char>(text[i]))) return false;
    }
    auto byte_at = [&](std::size_t off) {
        return static_cast<int>(std::strtoul(text.substr(off, 2).c_str(), nullptr, 16));
    };
    *r = byte_at(1);
    *g = byte_at(3);
    *b = byte_at(5);
    return true;
}

std::string HandleRoyuanKeyboardRequest(const bridge::Request& request,
                                        const std::string& device_key) {
    const json::Value* args = request.args;
    if (request.cmd == "kbd.get") {
        royuan::Device device;
        std::string err;
        if (!OpenRoyuanDeviceForKey(device_key, &device, &err)) {
            return bridge::Failure(request, "device not found");
        }
        royuan::FullState st;
        if (!device.ReadFullState(&st, &err)) {
            return bridge::Failure(request, err);
        }
        return bridge::Ok(request, command_json::BuildKbdStateData(ToKbdStateFields(st)));
    }

    if (request.cmd == "kbd.set") {
        if (args == nullptr) {
            return bridge::Failure(request, "kbd.set needs at least one setting field");
        }

        royuan::StatePatch patch;

        int profile = -1;
        const ArgState prof_state = OptIntArg(args, "profile", &profile);
        if (prof_state == ArgState::kBad) return bridge::Failure(request, "profile must be an integer");
        if (prof_state == ArgState::kOk) {
            if (!royuan::ProfileValid(profile)) {
                return bridge::Failure(request, "profile must be 0..3");
            }
            patch.have_profile = true;
            patch.profile = profile;
        }

        int debounce = -1;
        const ArgState deb_state = OptIntArg(args, "debounce", &debounce);
        if (deb_state == ArgState::kBad) return bridge::Failure(request, "debounce must be an integer");
        if (deb_state == ArgState::kOk) {
            if (!royuan::DebounceValid(debounce)) {
                return bridge::Failure(request, "debounce must be 1..10");
            }
            patch.have_debounce = true;
            patch.debounce = debounce;
        }

        auto parse_bool_option = [&](const json::Value* obj, const char* name, bool* flag,
                                     bool* val) -> std::string {
            bool b = false;
            const ArgState st = OptBoolArg(obj, name, &b);
            if (st == ArgState::kBad) return std::string(name) + " must be a boolean";
            if (st == ArgState::kOk) {
                *flag = true;
                *val = b;
            }
            return {};
        };

        auto parse_options_obj = [&](const json::Value* obj) -> std::string {
            std::string err;
            if (!(err = parse_bool_option(obj, "winLock", &patch.options.have_win_lock,
                                          &patch.options.win_lock))
                     .empty())
                return err;
            if (!(err = parse_bool_option(obj, "macMode", &patch.options.have_mac_mode,
                                          &patch.options.mac_mode))
                     .empty())
                return err;
            if (!(err = parse_bool_option(obj, "wasdSwap", &patch.options.have_wasd_swap,
                                          &patch.options.wasd_swap))
                     .empty())
                return err;
            if (!(err = parse_bool_option(obj, "backlightOff", &patch.options.have_backlight_off,
                                          &patch.options.backlight_off))
                     .empty())
                return err;
            if (!(err = parse_bool_option(obj, "sidelightOff", &patch.options.have_sidelight_off,
                                          &patch.options.sidelight_off))
                     .empty())
                return err;
            return {};
        };

        if (const json::Value* opt_val = args->Find("options")) {
            if (opt_val->type != json::Value::Type::kObject) {
                return bridge::Failure(request, "options must be an object");
            }
            const std::string opt_err = parse_options_obj(opt_val);
            if (!opt_err.empty()) return bridge::Failure(request, opt_err);
        }
        {
            const std::string top_err = parse_options_obj(args);
            if (!top_err.empty()) return bridge::Failure(request, top_err);
        }

        auto parse_led_obj = [&](const char* key, bool side, royuan::LedPatch* out,
                                 bool* have_led, bool* have_off_opt, bool* off_opt) -> std::string {
            const json::Value* val = args->Find(key);
            if (val == nullptr) return {};
            if (val->type != json::Value::Type::kObject) {
                return std::string(key) + " must be an object";
            }

            bool led_on = true;
            const ArgState on_st = OptBoolArg(val, "on", &led_on);
            if (on_st == ArgState::kBad) return std::string(key) + ".on must be a boolean";
            if (on_st == ArgState::kOk) {
                if (*have_off_opt && *off_opt != !led_on) {
                    return std::string(key) + ".on conflicts with options off flag";
                }
                *have_off_opt = true;
                *off_opt = !led_on;
                patch.have_options = true;
            }

            int mode = 0;
            const ArgState mode_st = OptIntArg(val, "mode", &mode);
            if (mode_st == ArgState::kBad) return std::string(key) + ".mode must be an integer";
            if (mode_st == ArgState::kOk) {
                const bool ok =
                    side ? royuan::SideLedModeValid(mode) : royuan::MainLedModeValid(mode);
                if (!ok) {
                    return side ? "sideLed.mode must be 0..5"
                                : "mainLed.mode must be 1..19 (excluding 13)";
                }
                out->have_mode = true;
                out->mode = mode;
            }

            int bri = 0;
            const ArgState bri_st = OptIntArg(val, "brightness", &bri);
            if (bri_st == ArgState::kBad) return std::string(key) + ".brightness must be an integer";
            if (bri_st == ArgState::kOk) {
                if (!royuan::LedBrightnessValid(bri)) {
                    return std::string(key) + ".brightness must be 0..4";
                }
                out->have_brightness = true;
                out->brightness = bri;
            }

            int spd = 0;
            const ArgState spd_st = OptIntArg(val, "speed", &spd);
            if (spd_st == ArgState::kBad) return std::string(key) + ".speed must be an integer";
            if (spd_st == ArgState::kOk) {
                if (!royuan::LedSpeedValid(spd)) {
                    return std::string(key) + ".speed must be 0..4";
                }
                out->have_speed = true;
                out->speed = spd;
            }

            bool colorful = true;
            const ArgState colf_st = OptBoolArg(val, "colorful", &colorful);
            if (colf_st == ArgState::kBad) {
                return std::string(key) + ".colorful must be a boolean";
            }
            if (colf_st == ArgState::kOk) {
                out->have_colorful = true;
                out->colorful = colorful;
            }

            if (const json::Value* color_val = val->Find("color")) {
                if (color_val->type != json::Value::Type::kString ||
                    !ParseHexColorString(color_val->text, &out->r, &out->g, &out->b)) {
                    return std::string(key) + ".color must be #RRGGBB";
                }
                out->have_color = true;
            }

            int dir = 0;
            const ArgState dir_st = OptIntArg(val, "direction", &dir);
            if (dir_st == ArgState::kBad) return std::string(key) + ".direction must be an integer";
            if (dir_st == ArgState::kOk) {
                if (!royuan::LedDirectionValid(dir)) {
                    return std::string(key) + ".direction must be 0..3";
                }
                out->have_direction = true;
                out->direction = dir;
            }

            if (out->Any()) {
                *have_led = true;
            } else if (on_st != ArgState::kOk) {
                return std::string(key) + " must contain at least one LED field";
            }
            return {};
        };

        {
            const std::string err =
                parse_led_obj("mainLed", false, &patch.main_led, &patch.have_main_led,
                              &patch.options.have_backlight_off, &patch.options.backlight_off);
            if (!err.empty()) return bridge::Failure(request, err);
        }
        {
            const std::string err =
                parse_led_obj("sideLed", true, &patch.side_led, &patch.have_side_led,
                              &patch.options.have_sidelight_off, &patch.options.sidelight_off);
            if (!err.empty()) return bridge::Failure(request, err);
        }

        if (patch.options.Any()) {
            patch.have_options = true;
        }

        if (const json::Value* slp_val = args->Find("sleep")) {
            if (slp_val->type != json::Value::Type::kObject) {
                return bridge::Failure(request, "sleep must be an object");
            }
            auto parse_sleep_field = [&](const char* fname, bool* flag, int* out) -> std::string {
                int v = 0;
                const ArgState st = OptIntArg(slp_val, fname, &v);
                if (st == ArgState::kBad) return std::string("sleep.") + fname + " must be an integer";
                if (st == ArgState::kOk) {
                    if (!royuan::SleepSecValid(v)) {
                        return std::string("sleep.") + fname + " must be 60..3600 seconds";
                    }
                    *flag = true;
                    *out = v;
                }
                return {};
            };
            std::string serr;
            if (!(serr = parse_sleep_field("sleepBt", &patch.sleep.have_sleep_bt,
                                           &patch.sleep.sleep_bt))
                     .empty())
                return bridge::Failure(request, serr);
            if (!(serr = parse_sleep_field("sleep24g", &patch.sleep.have_sleep_24g,
                                           &patch.sleep.sleep_24g))
                     .empty())
                return bridge::Failure(request, serr);
            if (!(serr = parse_sleep_field("deepBt", &patch.sleep.have_deep_bt,
                                           &patch.sleep.deep_bt))
                     .empty())
                return bridge::Failure(request, serr);
            if (!(serr = parse_sleep_field("deep24g", &patch.sleep.have_deep_24g,
                                           &patch.sleep.deep_24g))
                     .empty())
                return bridge::Failure(request, serr);
            if (!patch.sleep.Any()) {
                return bridge::Failure(request, "sleep must contain at least one timer field");
            }
            patch.have_sleep = true;
        }

        if (!patch.Any()) {
            return bridge::Failure(request, "kbd.set needs at least one setting field");
        }

        royuan::Device device;
        std::string err;
        if (!OpenRoyuanDeviceForKey(device_key, &device, &err)) {
            return bridge::Failure(request, "device not found");
        }
        royuan::FullState after;
        if (!device.ApplyPatchVerified(patch, &after, &err)) {
            return bridge::Failure(request, err);
        }
        return bridge::Ok(request, command_json::BuildKbdStateData(ToKbdStateFields(after)));
    }

    auto utf8_codepoints = [](const std::string& s) -> int {
        int count = 0;
        for (unsigned char c : s) {
            if ((c & 0xC0u) != 0x80u) ++count;
        }
        return count;
    };

    auto royuan_macro_labels_path = [&]() -> std::wstring {
        const std::wstring dir = host::AppDataDir();
        return dir.empty() ? std::wstring() : (dir + L"\\royuan_macros.ini");
    };

    auto load_royuan_macro_labels = [&]() -> std::map<int, std::string> {
        std::map<int, std::string> labels;
        const std::wstring path = royuan_macro_labels_path();
        if (path.empty()) return labels;
        std::string raw;
        std::wstring rerr;
        bool not_found = false;
        if (!host::ReadAllBytes(path, &raw, &rerr, &not_found) || raw.empty()) return labels;
        std::size_t start = 0;
        while (start < raw.size()) {
            std::size_t end = raw.find('\n', start);
            if (end == std::string::npos) end = raw.size();
            std::string line = raw.substr(start, end - start);
            if (!line.empty() && line.back() == '\r') line.pop_back();
            const std::size_t eq = line.find('=');
            if (eq != std::string::npos && eq > 0) {
                char* endp = nullptr;
                const long slot_val = std::strtol(line.substr(0, eq).c_str(), &endp, 10);
                if (endp != line.data() && *endp == '\0' &&
                    royuan::MacroSlotValid(static_cast<int>(slot_val))) {
                    labels[static_cast<int>(slot_val)] = line.substr(eq + 1);
                }
            }
            start = end + 1;
        }
        return labels;
    };

    auto save_royuan_macro_label = [&](int slot, const std::string& name) {
        const std::wstring path = royuan_macro_labels_path();
        if (path.empty()) return;
        std::map<int, std::string> labels = load_royuan_macro_labels();
        std::string clean;
        clean.reserve(name.size());
        for (char ch : name) {
            if (ch != '\r' && ch != '\n') clean.push_back(ch);
        }
        if (clean.empty()) {
            labels.erase(slot);
        } else {
            labels[slot] = clean;
        }
        std::string out;
        for (const auto& kv : labels) {
            out += std::to_string(kv.first) + "=" + kv.second + "\n";
        }
        std::wstring werr;
        host::WriteAllBytesAtomic(path, out, &werr);
    };

    auto resolve_macro_name = [&](const std::map<int, std::string>& labels, int slot) {
        auto it = labels.find(slot);
        if (it != labels.end() && !it->second.empty()) return it->second;
        return std::string("M") + std::to_string(slot + 1);
    };

    auto matrix_to_entries = [](const royuan::KeyMatrix& mat) {
        std::vector<std::array<int, 4>> list(royuan::kKeymapSlotCount);
        for (int s = 0; s < royuan::kKeymapSlotCount; ++s) {
            const royuan::KeyEntry e = mat.Slot(s);
            list[static_cast<std::size_t>(s)] = {e.b[0], e.b[1], e.b[2], e.b[3]};
        }
        return list;
    };

    auto macro_to_json_events = [](const royuan::MacroData& m) {
        std::vector<command_json::KbdMacroEventItem> out;
        out.reserve(m.events.size());
        for (const royuan::MacroEvent& ev : m.events) {
            command_json::KbdMacroEventItem item;
            if (ev.kind == royuan::MacroEventKind::kKey) {
                item.kind = "key";
                item.usage = static_cast<int>(ev.code);
                item.pressed = ev.pressed;
            } else if (ev.kind == royuan::MacroEventKind::kMouseButton) {
                item.kind = "mouseButton";
                item.button = static_cast<int>(ev.code);
                item.pressed = ev.pressed;
            } else {
                item.kind = "mouseMove";
                item.dx = ev.dx;
                item.dy = ev.dy;
            }
            item.delay_ms = ev.delay_ms;
            out.push_back(std::move(item));
        }
        return out;
    };

    if (request.cmd == "kbd.keys.get") {
        int profile = -1;
        if (args != nullptr) {
            const ArgState p_st = OptIntArg(args, "profile", &profile);
            if (p_st == ArgState::kBad) return bridge::Failure(request, "profile must be an integer");
            if (p_st == ArgState::kOk && !royuan::ProfileValid(profile)) {
                return bridge::Failure(request, "profile must be 0..3");
            }
        }
        royuan::Device device;
        std::string err;
        if (!OpenRoyuanDeviceForKey(device_key, &device, &err)) {
            return bridge::Failure(request, "device not found");
        }
        royuan::KeyMatrix base_mat, fn_mat;
        int resolved_profile = 0;
        if (!device.ReadKeymap(profile, &base_mat, &fn_mat, &resolved_profile, &err)) {
            return bridge::Failure(request, err);
        }
        return bridge::Ok(request, command_json::BuildKbdKeymapData(
                                       resolved_profile, matrix_to_entries(base_mat),
                                       matrix_to_entries(fn_mat)));
    }

    if (request.cmd == "kbd.keys.set") {
        if (args == nullptr) {
            return bridge::Failure(request, "kbd.keys.set requires args");
        }
        int profile = -1;
        const ArgState p_st = OptIntArg(args, "profile", &profile);
        if (p_st == ArgState::kBad) return bridge::Failure(request, "profile must be an integer");
        if (p_st == ArgState::kOk && !royuan::ProfileValid(profile)) {
            return bridge::Failure(request, "profile must be 0..3");
        }

        bool fn_layer = false;
        const ArgState fn_st = OptBoolArg(args, "fnLayer", &fn_layer);
        if (fn_st == ArgState::kBad) return bridge::Failure(request, "fnLayer must be a boolean");

        int slot = -1;
        const ArgState slot_st = OptIntArg(args, "slot", &slot);
        if (slot_st == ArgState::kAbsent) return bridge::Failure(request, "kbd.keys.set requires args.slot");
        if (slot_st == ArgState::kBad) return bridge::Failure(request, "slot must be an integer");
        if (!royuan::KeySlotValid(slot)) {
            return bridge::Failure(request, "slot must be 0..125");
        }

        const json::Value* entry_val = args->Find("entry");
        if (entry_val == nullptr || entry_val->type != json::Value::Type::kArray ||
            entry_val->array.size() != 4u) {
            return bridge::Failure(request, "entry must be a 4-integer array");
        }
        royuan::KeyEntry entry;
        for (std::size_t i = 0; i < 4u; ++i) {
            const json::Value& elem = entry_val->array[i];
            if (elem.type != json::Value::Type::kNumber || elem.number < 0 || elem.number > 255) {
                return bridge::Failure(request, "entry bytes must be integers 0..255");
            }
            entry.b[i] = static_cast<std::uint8_t>(elem.number);
        }
        std::string valid_err;
        if (!royuan::KeyEntryValid(entry, fn_layer, &valid_err)) {
            return bridge::Failure(request, valid_err);
        }
        if (fn_layer && slot == royuan::kKeymapFnPhysicalSlot) {
            return bridge::Failure(request, "cannot remap the Fn key (slot 65) on the Fn layer");
        }

        royuan::Device device;
        std::string err;
        if (!OpenRoyuanDeviceForKey(device_key, &device, &err)) {
            return bridge::Failure(request, "device not found");
        }
        royuan::KeyEntry readback;
        bool bulk_used = false;
        if (!device.WriteKeySlotVerified(profile, fn_layer, slot, entry, &readback, &bulk_used,
                                         &err)) {
            return bridge::Failure(request, err);
        }
        int resolved_profile = profile;
        if (resolved_profile < 0) {
            royuan::FullState st;
            if (device.ReadFullState(&st, nullptr) && st.have_profile) {
                resolved_profile = st.profile;
            } else {
                resolved_profile = 0;
            }
        }
        const std::array<int, 4> rb_arr{readback.b[0], readback.b[1], readback.b[2], readback.b[3]};
        return bridge::Ok(request, command_json::BuildKbdKeySetData(resolved_profile, fn_layer,
                                                                   slot, rb_arr, bulk_used));
    }

    if (request.cmd == "kbd.keys.reset") {
        if (args == nullptr) {
            return bridge::Failure(request, "kbd.keys.reset requires args");
        }
        int profile = -1;
        const ArgState p_st = OptIntArg(args, "profile", &profile);
        if (p_st == ArgState::kBad) return bridge::Failure(request, "profile must be an integer");
        if (p_st == ArgState::kOk && !royuan::ProfileValid(profile)) {
            return bridge::Failure(request, "profile must be 0..3");
        }
        bool fn_layer = false;
        const ArgState fn_st = OptBoolArg(args, "fnLayer", &fn_layer);
        if (fn_st != ArgState::kOk) {
            return bridge::Failure(request, "kbd.keys.reset requires boolean args.fnLayer");
        }

        royuan::Device device;
        std::string err;
        if (!OpenRoyuanDeviceForKey(device_key, &device, &err)) {
            return bridge::Failure(request, "device not found");
        }
        royuan::KeyMatrix target;
        royuan::GetBc98FactoryKeymap(fn_layer, &target);
        if (!device.WriteKeymapLayerBulkVerified(profile, fn_layer, target, nullptr, &err)) {
            return bridge::Failure(request, err);
        }
        royuan::KeyMatrix base_mat, fn_mat;
        int resolved_profile = 0;
        if (!device.ReadKeymap(profile, &base_mat, &fn_mat, &resolved_profile, &err)) {
            return bridge::Failure(request, err);
        }
        return bridge::Ok(request, command_json::BuildKbdKeymapData(
                                       resolved_profile, matrix_to_entries(base_mat),
                                       matrix_to_entries(fn_mat)));
    }

    if (request.cmd == "kbd.macro.list") {
        int count = 16;
        if (args != nullptr) {
            const ArgState c_st = OptIntArg(args, "count", &count);
            if (c_st == ArgState::kBad) return bridge::Failure(request, "count must be an integer");
            if (c_st == ArgState::kOk && (count < 1 || count > royuan::kMacroSlotCount)) {
                return bridge::Failure(request, "count must be 1..50");
            }
        }
        royuan::Device device;
        std::string err;
        if (!OpenRoyuanDeviceForKey(device_key, &device, &err)) {
            return bridge::Failure(request, "device not found");
        }
        std::vector<royuan::MacroSlotSummary> summaries;
        if (!device.ListMacroSummaries(count, &summaries, &err)) {
            return bridge::Failure(request, err);
        }
        const std::map<int, std::string> labels = load_royuan_macro_labels();
        std::vector<command_json::KbdMacroSummaryItem> items;
        items.reserve(summaries.size());
        for (const auto& s : summaries) {
            command_json::KbdMacroSummaryItem item;
            item.slot = s.slot;
            item.name = resolve_macro_name(labels, s.slot);
            item.empty = s.empty;
            item.repeat = s.repeat;
            item.event_count = s.event_count;
            item.byte_length = s.byte_length;
            items.push_back(std::move(item));
        }
        return bridge::Ok(request,
                          command_json::BuildKbdMacroListData(royuan::kMacroSlotCount, items));
    }

    if (request.cmd == "kbd.macro.get") {
        if (args == nullptr) {
            return bridge::Failure(request, "kbd.macro.get requires args.slot");
        }
        int slot = -1;
        const ArgState s_st = OptIntArg(args, "slot", &slot);
        if (s_st == ArgState::kAbsent) return bridge::Failure(request, "kbd.macro.get requires args.slot");
        if (s_st == ArgState::kBad) return bridge::Failure(request, "slot must be an integer");
        if (!royuan::MacroSlotValid(slot)) {
            return bridge::Failure(request, "macro slot must be 0..49");
        }
        royuan::Device device;
        std::string err;
        if (!OpenRoyuanDeviceForKey(device_key, &device, &err)) {
            return bridge::Failure(request, "device not found");
        }
        royuan::MacroData macro;
        bool empty = true;
        int byte_len = 0;
        if (!device.ReadMacroSlot(slot, &macro, &empty, nullptr, &byte_len, &err)) {
            return bridge::Failure(request, err);
        }
        const std::map<int, std::string> labels = load_royuan_macro_labels();
        return bridge::Ok(request, command_json::BuildKbdMacroData(
                                       slot, resolve_macro_name(labels, slot), empty, macro.repeat,
                                       byte_len, macro_to_json_events(macro)));
    }

    if (request.cmd == "kbd.macro.set") {
        if (args == nullptr) {
            return bridge::Failure(request, "kbd.macro.set requires args");
        }
        int slot = -1;
        const ArgState s_st = OptIntArg(args, "slot", &slot);
        if (s_st == ArgState::kAbsent) return bridge::Failure(request, "kbd.macro.set requires args.slot");
        if (s_st == ArgState::kBad) return bridge::Failure(request, "slot must be an integer");
        if (!royuan::MacroSlotValid(slot)) {
            return bridge::Failure(request, "macro slot must be 0..49");
        }

        bool have_name = false;
        std::string name;
        if (const json::Value* nv = args->Find("name")) {
            if (nv->type != json::Value::Type::kString) {
                return bridge::Failure(request, "name must be a string");
            }
            if (utf8_codepoints(nv->text) > 32) {
                return bridge::Failure(request, "macro name must be <= 32 characters");
            }
            have_name = true;
            name = nv->text;
        }

        int repeat = 1;
        const ArgState r_st = OptIntArg(args, "repeat", &repeat);
        if (r_st == ArgState::kBad) return bridge::Failure(request, "repeat must be an integer");
        if (repeat < royuan::kMacroRepeatMin || repeat > royuan::kMacroRepeatMax) {
            return bridge::Failure(request, "repeat must be 1..65535");
        }

        const json::Value* ev_arr = args->Find("events");
        if (ev_arr == nullptr || ev_arr->type != json::Value::Type::kArray) {
            return bridge::Failure(request, "events must be an array");
        }

        royuan::MacroData macro;
        macro.repeat = repeat;
        macro.events.reserve(ev_arr->array.size());
        for (const json::Value& item : ev_arr->array) {
            if (item.type != json::Value::Type::kObject) {
                return bridge::Failure(request, "each macro event must be an object");
            }
            const json::Value* kv = item.Find("kind");
            if (kv == nullptr || kv->type != json::Value::Type::kString) {
                return bridge::Failure(request, "macro event missing string kind");
            }
            royuan::MacroEvent ev;
            if (kv->text == "key") {
                int usage = 0;
                bool pressed = false;
                int delay_ms = 0;
                if (OptIntArg(&item, "usage", &usage) != ArgState::kOk) {
                    return bridge::Failure(request, "key event requires integer usage");
                }
                if (OptBoolArg(&item, "pressed", &pressed) != ArgState::kOk) {
                    return bridge::Failure(request, "key event requires boolean pressed");
                }
                if (OptIntArg(&item, "delayMs", &delay_ms) != ArgState::kOk) {
                    return bridge::Failure(request, "key event requires integer delayMs");
                }
                if (usage < 0x04 || usage > 0xEF) {
                    return bridge::Failure(request, "key usage must be 0x04..0xEF");
                }
                ev.kind = royuan::MacroEventKind::kKey;
                ev.code = static_cast<std::uint8_t>(usage);
                ev.pressed = pressed;
                ev.delay_ms = delay_ms;
            } else if (kv->text == "mouseButton") {
                int button = 0;
                bool pressed = false;
                int delay_ms = 0;
                if (OptIntArg(&item, "button", &button) != ArgState::kOk) {
                    return bridge::Failure(request, "mouseButton event requires integer button");
                }
                if (OptBoolArg(&item, "pressed", &pressed) != ArgState::kOk) {
                    return bridge::Failure(request, "mouseButton event requires boolean pressed");
                }
                if (OptIntArg(&item, "delayMs", &delay_ms) != ArgState::kOk) {
                    return bridge::Failure(request, "mouseButton event requires integer delayMs");
                }
                if (button < 0 || button > 4) {
                    return bridge::Failure(request, "mouse button must be 0..4");
                }
                ev.kind = royuan::MacroEventKind::kMouseButton;
                ev.code = static_cast<std::uint8_t>(button);
                ev.pressed = pressed;
                ev.delay_ms = delay_ms;
            } else if (kv->text == "mouseMove") {
                int dx = 0;
                int dy = 0;
                int delay_ms = 0;
                if (OptIntArg(&item, "dx", &dx) != ArgState::kOk ||
                    OptIntArg(&item, "dy", &dy) != ArgState::kOk ||
                    OptIntArg(&item, "delayMs", &delay_ms) != ArgState::kOk) {
                    return bridge::Failure(request, "mouseMove event requires integer dx, dy, delayMs");
                }
                ev.kind = royuan::MacroEventKind::kMouseMove;
                ev.dx = dx;
                ev.dy = dy;
                ev.delay_ms = delay_ms;
            } else {
                return bridge::Failure(request, "unknown macro event kind: " + kv->text);
            }
            macro.events.push_back(ev);
        }

        std::uint8_t target_blob[royuan::kMacroBytes]{};
        std::string encode_err;
        if (!royuan::EncodeMacroBlob(macro, target_blob, &encode_err, nullptr)) {
            return bridge::Failure(request, encode_err);
        }

        royuan::Device device;
        std::string err;
        if (!OpenRoyuanDeviceForKey(device_key, &device, &err)) {
            return bridge::Failure(request, "device not found");
        }
        royuan::MacroData after;
        bool empty = true;
        int byte_len = 0;
        if (!device.WriteMacroSlotVerified(slot, target_blob, &after, &empty, &byte_len, &err)) {
            return bridge::Failure(request, err);
        }
        if (have_name) {
            save_royuan_macro_label(slot, name);
        }
        const std::map<int, std::string> labels = load_royuan_macro_labels();
        return bridge::Ok(request, command_json::BuildKbdMacroData(
                                       slot, resolve_macro_name(labels, slot), empty, after.repeat,
                                       byte_len, macro_to_json_events(after)));
    }

    if (request.cmd == "kbd.macro.delete") {
        if (args == nullptr) {
            return bridge::Failure(request, "kbd.macro.delete requires args.slot");
        }
        int slot = -1;
        const ArgState s_st = OptIntArg(args, "slot", &slot);
        if (s_st == ArgState::kAbsent) {
            return bridge::Failure(request, "kbd.macro.delete requires args.slot");
        }
        if (s_st == ArgState::kBad) return bridge::Failure(request, "slot must be an integer");
        if (!royuan::MacroSlotValid(slot)) {
            return bridge::Failure(request, "macro slot must be 0..49");
        }

        royuan::Device device;
        std::string err;
        if (!OpenRoyuanDeviceForKey(device_key, &device, &err)) {
            return bridge::Failure(request, "device not found");
        }
        const std::uint8_t zero_blob[royuan::kMacroBytes]{};
        if (!device.WriteMacroSlotVerified(slot, zero_blob, nullptr, nullptr, nullptr, &err)) {
            return bridge::Failure(request, err);
        }
        save_royuan_macro_label(slot, "");
        return bridge::Ok(request, command_json::BuildKbdMacroDeleteData(slot));
    }

    return bridge::Failure(request, "unknown keyboard command");
}

}  // namespace

std::string Handle(const bridge::Request& request, const std::string& device_key) {
    return HandleRoyuanKeyboardRequest(request, device_key);
}

}  // namespace royuan_driver
