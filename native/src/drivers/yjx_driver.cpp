#include "drivers/yjx_driver.h"

#include <cstdint>
#include <string>

#include "bridge_args.h"
#include "command_json.h"
#include "driver_router.h"
#include "drivers/host.h"
#include "yjx_hid.h"
#include "yjx_protocol.h"

namespace yjx_driver {
namespace {

using bridge::ArgState;
using bridge::OptBoolArg;
using bridge::OptIntArg;

bool OpenYjxDeviceForKey(const std::string& device_key, yjx::Device* dev, std::string* error) {
    std::uint16_t preferred_vid = 0;
    driver_router::ParseYjxVidFromKey(device_key, &preferred_vid);
    return dev->Open(preferred_vid, error);
}

std::string HandleYjxKeysRequest(const bridge::Request& request, const std::string& device_key) {
    auto format_key = [](int index, const yjx::KeyCode& kc) {
        std::string binding;
        std::string kind;
        yjx::DecodeBindingForSlot(index, kc, &binding, &kind);
        const char* slot_name = yjx::KeySlotName(index);
        return "{\"index\":" + std::to_string(index) +
               ",\"slot\":" + bridge::JsonQuote(slot_name != nullptr ? slot_name : "") +
               ",\"matrix\":" + std::to_string(yjx::kKeyWireSlot[index]) +
               ",\"code\":" + std::to_string(yjx::PackKeyCode(kc)) +
               ",\"binding\":" + bridge::JsonQuote(binding) +
               ",\"kind\":" + bridge::JsonQuote(kind) + "}";
    };
    auto format_all = [&](const yjx::KeyCode keys[yjx::kKeyCount]) {
        std::string out = "{\"bindings\":[";
        for (int i = 0; i < yjx::kKeyCount; ++i) {
            if (i != 0) out += ',';
            out += format_key(i, keys[i]);
        }
        out += "]}";
        return out;
    };

    if (request.cmd == "keys.get") {
        yjx::Device device;
        std::string err;
        if (!OpenYjxDeviceForKey(device_key, &device, &err)) {
            return bridge::Failure(request, "device not found");
        }
        yjx::KeyCode keys[yjx::kKeyCount]{};
        if (!device.ReadKeys(keys, nullptr, &err)) {
            return bridge::Failure(request, err);
        }
        return bridge::Ok(request, format_all(keys));
    }

    if (request.cmd == "keys.set") {
        int index = -1;
        std::string name;
        if (!bridge::ParseKeySetArgs(request, &index, &name)) {
            return bridge::Failure(request, "keys.set needs args.index and args.binding");
        }
        if (index < 0 || index >= yjx::kKeyCount) {
            return bridge::Failure(request, "index must be 0..5");
        }
        if (name.rfind("Macro:", 0) == 0 || name.rfind("macro:", 0) == 0) {
            return bridge::Failure(request,
                                   "Macro:N is set via macros.upload (slot + buttonIndex), "
                                   "not keys.set");
        }
        yjx::KeyCode code{};
        if (!yjx::EncodeBinding(name.c_str(), &code)) {
            return bridge::Failure(request, "unsupported or unknown binding: " + name);
        }
        yjx::Device device;
        std::string err;
        if (!OpenYjxDeviceForKey(device_key, &device, &err)) {
            return bridge::Failure(request, "device not found");
        }
        yjx::KeyCode readback{};
        if (!device.WriteKeyVerified(index, code, &readback, &err)) {
            return bridge::Failure(request, err);
        }
        host::ClearButtonMacroBind(index);
        yjx::KeyCode all[yjx::kKeyCount]{};
        std::string read_err;
        if (device.ReadKeys(all, nullptr, &read_err)) {
            return bridge::Ok(request, format_all(all));
        }
        return bridge::Ok(request, "{\"bindings\":[" + format_key(index, readback) + "]}");
    }

    return bridge::Failure(request, "unknown key command");
}

std::string HandleYjxDpiRequest(const bridge::Request& request, const std::string& device_key) {
    const json::Value* args = request.args;
    const std::string& command = request.cmd;
    auto opt_int = [&](const char* name, int* out) { return OptIntArg(args, name, out); };

    if (command == "dpi.get") {
        yjx::Device device;
        std::string err;
        if (!OpenYjxDeviceForKey(device_key, &device, &err)) {
            return bridge::Failure(request, "device not found");
        }
        yjx::Config cfg;
        if (!device.ReadConfig(&cfg, nullptr, &err)) {
            return bridge::Failure(request, err);
        }
        const bool active_valid = (cfg.dpi_active >= 0 && cfg.dpi_active < cfg.dpi_count);
        std::string json = "{\"count\":" + std::to_string(cfg.dpi_count) +
                           ",\"active\":" + (active_valid ? std::to_string(cfg.dpi_active) : "null") +
                           ",\"levels\":[";
        for (int i = 0; i < yjx::kDpiStageMax; ++i) {
            if (i != 0) json += ",";
            const int v = cfg.dpi[i];
            if (v < yjx::kDpiMin || v > yjx::kDpiMax) {
                json += "null";
            } else {
                json += "{\"dpi\":" + std::to_string(v) +
                        ",\"x\":" + std::to_string(v) +
                        ",\"y\":" + std::to_string(v) + "}";
            }
        }
        json += "]}";
        return bridge::Ok(request, json);
    }

    if (command == "dpi.set") {
        int legacy_dpi = 0;
        int dpi_x = -1;
        int dpi_y = -1;
        int slot = -1;
        const ArgState dpi_state = opt_int("dpi", &legacy_dpi);
        const ArgState x_state = opt_int("dpiX", &dpi_x);
        const ArgState y_state = opt_int("dpiY", &dpi_y);
        const ArgState slot_state = opt_int("slot", &slot);
        if (dpi_state == ArgState::kBad) return bridge::Failure(request, "dpi must be an integer");
        if (x_state == ArgState::kBad) return bridge::Failure(request, "dpiX must be an integer");
        if (y_state == ArgState::kBad) return bridge::Failure(request, "dpiY must be an integer");
        if (slot_state == ArgState::kBad) return bridge::Failure(request, "slot must be an integer");

        const bool has_dpi = (dpi_state == ArgState::kOk);
        const bool has_x = (x_state == ArgState::kOk);
        const bool has_y = (y_state == ArgState::kOk);
        if (!has_dpi && !has_x && !has_y) {
            return bridge::Failure(request, "dpi.set needs args.dpi");
        }
        if (has_dpi && (has_x || has_y)) {
            return bridge::Failure(request, "dpi.set cannot mix args.dpi with args.dpiX/args.dpiY");
        }
        if ((has_x || has_y) && (!has_x || !has_y)) {
            return bridge::Failure(request, "dpi.set needs both dpiX and dpiY");
        }
        if (has_x && dpi_x != dpi_y) {
            return bridge::Failure(request, "this device does not support independent X/Y DPI");
        }
        const int target_dpi = has_x ? dpi_x : legacy_dpi;
        if (!yjx::DpiValid(target_dpi)) {
            return bridge::Failure(request, "dpi must be 200..12000 in steps of 100");
        }
        if (slot_state != ArgState::kOk || slot < 0 || slot >= yjx::kDpiStageMax) {
            return bridge::Failure(request, "dpi.set needs args.slot (0..5)");
        }

        yjx::Device device;
        std::string err;
        if (!OpenYjxDeviceForKey(device_key, &device, &err)) {
            return bridge::Failure(request, "device not found");
        }

        yjx::ConfigPatch patch;
        patch.have_dpi[slot] = true;
        patch.dpi[slot] = target_dpi;
        yjx::Config after;
        if (!device.WriteConfigVerified(patch, &after, &err)) {
            return bridge::Failure(request, err);
        }
        return bridge::Ok(request, command_json::BuildDpiSetData(slot, after.dpi[slot], false));
    }

    if (command == "dpi.stage") {
        int active = -1;
        int count = -1;
        const ArgState active_state = opt_int("active", &active);
        const ArgState count_state = opt_int("count", &count);
        if (active_state == ArgState::kBad) return bridge::Failure(request, "active must be an integer");
        if (count_state == ArgState::kBad) return bridge::Failure(request, "count must be an integer");
        if (active_state != ArgState::kOk) return bridge::Failure(request, "dpi.stage needs args.active");
        if (count_state != ArgState::kOk) return bridge::Failure(request, "dpi.stage needs args.count (1..6)");
        if (!yjx::DpiStageValid(count, active)) {
            return bridge::Failure(request, "invalid dpi.stage args: count must be 1..6 and active 0..count-1");
        }

        yjx::Device device;
        std::string err;
        if (!OpenYjxDeviceForKey(device_key, &device, &err)) {
            return bridge::Failure(request, "device not found");
        }

        yjx::ConfigPatch patch;
        patch.have_dpi_count = true;
        patch.dpi_count = count;
        patch.have_dpi_active = true;
        patch.dpi_active = active;
        yjx::Config after;
        if (!device.WriteConfigVerified(patch, &after, &err)) {
            return bridge::Failure(request, err);
        }
        return bridge::Ok(request, command_json::BuildDpiStageData(after.dpi_active, after.dpi_count));
    }

    return bridge::Failure(request, "unknown dpi command");
}

std::string HandleYjxHardwareRequest(const bridge::Request& request,
                                     const std::string& device_key) {
    const json::Value* args = request.args;
    const std::string& command = request.cmd;
    auto opt_int = [&](const char* name, int* out) { return OptIntArg(args, name, out); };
    auto opt_bool = [&](const char* name, bool* out) { return OptBoolArg(args, name, out); };

    if (command == "rate.get") {
        yjx::Device device;
        std::string err;
        if (!OpenYjxDeviceForKey(device_key, &device, &err)) {
            return bridge::Failure(request, "device not found");
        }
        yjx::Config cfg;
        if (!device.ReadConfig(&cfg, nullptr, &err) || cfg.rate_hz <= 0) {
            return bridge::Ok(request, "{\"code\":null,\"hz\":null}");
        }
        const int code = yjx::RateToWire(cfg.rate_hz) - 1;
        return bridge::Ok(request, "{\"code\":" + std::to_string(code) +
                                       ",\"hz\":" + std::to_string(cfg.rate_hz) + "}");
    }

    if (command == "rate.set") {
        int hz = 0;
        int code = -1;
        const ArgState hz_state = opt_int("hz", &hz);
        const ArgState code_state = opt_int("code", &code);
        if (hz_state == ArgState::kBad) return bridge::Failure(request, "hz must be an integer");
        if (code_state == ArgState::kBad) return bridge::Failure(request, "code must be an integer");

        if (hz_state == ArgState::kOk) {
            if (yjx::RateToWire(hz) < 0) {
                return bridge::Failure(request, "hz must be 125, 250, 500 or 1000");
            }
        } else if (code_state == ArgState::kOk) {
            hz = yjx::RateFromWire(static_cast<std::uint8_t>(code + 1));
            if (hz < 0) return bridge::Failure(request, "code must be 0..3");
        } else {
            return bridge::Failure(request, "rate.set needs args.hz or args.code");
        }

        yjx::Device device;
        std::string err;
        if (!OpenYjxDeviceForKey(device_key, &device, &err)) {
            return bridge::Failure(request, "device not found");
        }
        if (!device.IsWireless() && hz != 1000) {
            return bridge::Failure(request, "USB wired mode only supports 1000 Hz");
        }

        yjx::ConfigPatch patch;
        patch.have_rate_hz = true;
        patch.rate_hz = hz;
        yjx::Config after;
        if (!device.WriteConfigVerified(patch, &after, &err)) {
            return bridge::Failure(request, err);
        }
        const int rb_code = yjx::RateToWire(after.rate_hz) - 1;
        return bridge::Ok(request, "{\"code\":" + std::to_string(rb_code) +
                                       ",\"hz\":" + std::to_string(after.rate_hz) + "}");
    }

    if (command == "sensor.get") {
        yjx::Device device;
        std::string err;
        if (!OpenYjxDeviceForKey(device_key, &device, &err)) {
            return bridge::Failure(request, "device not found");
        }
        yjx::Config cfg;
        if (!device.ReadConfig(&cfg, nullptr, &err)) {
            return bridge::Ok(request,
                              "{\"lod\":null,\"ripple\":null,\"fixline\":null,"
                              "\"motion\":null,\"mode\":null}");
        }
        const int lod = yjx::LodWireToBridge(cfg.lod_wire);
        auto bool_str = [](bool v) { return v ? "true" : "false"; };
        return bridge::Ok(request, "{\"lod\":" + (lod >= 0 ? std::to_string(lod) : "null") +
                                       ",\"ripple\":" + bool_str(cfg.ripple) +
                                       ",\"fixline\":" + bool_str(cfg.fixline) +
                                       ",\"motion\":" + bool_str(cfg.motion) +
                                       ",\"mode\":null}");
    }

    if (command == "sensor.set") {
        int lod = -1;
        int mode = -1;
        bool ripple = false;
        bool fixline = false;
        bool motion = false;
        const ArgState lod_state = opt_int("lod", &lod);
        const ArgState ripple_state = opt_bool("ripple", &ripple);
        const ArgState fixline_state = opt_bool("fixline", &fixline);
        const ArgState motion_state = opt_bool("motion", &motion);
        const ArgState mode_state = opt_int("mode", &mode);
        if (lod_state == ArgState::kBad) return bridge::Failure(request, "lod must be an integer");
        if (ripple_state == ArgState::kBad) return bridge::Failure(request, "ripple must be a boolean");
        if (fixline_state == ArgState::kBad) return bridge::Failure(request, "fixline must be a boolean");
        if (motion_state == ArgState::kBad) return bridge::Failure(request, "motion must be a boolean");
        if (mode_state != ArgState::kAbsent) {
            return bridge::Failure(request, "sensor mode is not supported by this device");
        }

        const bool have_lod = (lod_state == ArgState::kOk);
        const bool have_ripple = (ripple_state == ArgState::kOk);
        const bool have_fixline = (fixline_state == ArgState::kOk);
        const bool have_motion = (motion_state == ArgState::kOk);
        if (!have_lod && !have_ripple && !have_fixline && !have_motion) {
            return bridge::Failure(request, "sensor.set needs lod, ripple, fixline or motion");
        }

        int wire_lod = -1;
        if (have_lod) {
            wire_lod = yjx::LodBridgeToWire(lod);
            if (wire_lod < 0) {
                return bridge::Failure(request, "lod must be 0 (1.0 mm) or 1 (2.0 mm)");
            }
        }

        yjx::Device device;
        std::string err;
        if (!OpenYjxDeviceForKey(device_key, &device, &err)) {
            return bridge::Failure(request, "device not found");
        }

        yjx::ConfigPatch patch;
        patch.have_lod_wire = have_lod;
        patch.lod_wire = wire_lod;
        patch.have_ripple = have_ripple;
        patch.ripple = ripple;
        patch.have_fixline = have_fixline;
        patch.fixline = fixline;
        patch.have_motion = have_motion;
        patch.motion = motion;

        yjx::Config after;
        if (!device.WriteConfigVerified(patch, &after, &err)) {
            return bridge::Failure(request, err);
        }

        command_json::SensorSetFields fields;
        if (have_lod) {
            fields.have_lod = true;
            fields.lod = yjx::LodWireToBridge(after.lod_wire);
        }
        if (have_ripple) {
            fields.have_ripple = true;
            fields.ripple = after.ripple;
        }
        if (have_fixline) {
            fields.have_fixline = true;
            fields.fixline = after.fixline;
        }
        if (have_motion) {
            fields.have_motion = true;
            fields.motion = after.motion;
        }
        return bridge::Ok(request, command_json::BuildSensorSetData(fields));
    }

    if (command == "misc.get") {
        yjx::Device device;
        std::string err;
        if (!OpenYjxDeviceForKey(device_key, &device, &err)) {
            return bridge::Failure(request, "device not found");
        }
        yjx::Config cfg;
        if (!device.ReadConfig(&cfg, nullptr, &err)) {
            return bridge::Ok(request,
                              "{\"debounce\":null,\"sleep\":null,"
                              "\"scrollReverse\":null,\"fireLoops\":null,\"fireInterval\":null}");
        }
        const int sleep_sec = yjx::SleepMinToSec(cfg.sleep_min);
        auto int_or_null = [](int v) { return v >= 0 ? std::to_string(v) : "null"; };
        return bridge::Ok(request, "{\"debounce\":" + int_or_null(cfg.debounce_ms) +
                                       ",\"sleep\":" + int_or_null(sleep_sec) +
                                       ",\"scrollReverse\":" + (cfg.scroll_reverse ? "true" : "false") +
                                       ",\"fireLoops\":null,\"fireInterval\":null}");
    }

    if (command == "misc.set") {
        int debounce = -1;
        int sleep_sec = -1;
        bool scroll_reverse = false;
        int dummy = -1;
        const ArgState deb_state = opt_int("debounce", &debounce);
        const ArgState slp_state = opt_int("sleep", &sleep_sec);
        const ArgState scroll_state = opt_bool("scrollReverse", &scroll_reverse);
        const ArgState loops_state = opt_int("fireLoops", &dummy);
        const ArgState interval_state = opt_int("fireInterval", &dummy);
        if (deb_state == ArgState::kBad) return bridge::Failure(request, "debounce must be an integer");
        if (slp_state == ArgState::kBad) return bridge::Failure(request, "sleep must be an integer");
        if (scroll_state == ArgState::kBad) {
            return bridge::Failure(request, "scrollReverse must be a boolean");
        }
        if (loops_state != ArgState::kAbsent || interval_state != ArgState::kAbsent) {
            return bridge::Failure(request, "fire key parameters are not supported by this device");
        }

        const bool have_deb = (deb_state == ArgState::kOk);
        const bool have_slp = (slp_state == ArgState::kOk);
        const bool have_scroll = (scroll_state == ArgState::kOk);
        if (!have_deb && !have_slp && !have_scroll) {
            return bridge::Failure(request, "misc.set needs debounce, sleep or scrollReverse");
        }
        if (have_deb && !yjx::DebounceValid(debounce)) {
            return bridge::Failure(request, "debounce must be 0..30 ms");
        }
        int sleep_min = -1;
        if (have_slp) {
            sleep_min = yjx::SleepSecToMin(sleep_sec);
            if (sleep_min < 0) {
                return bridge::Failure(
                    request, "sleep must be 60, 180, 300, 600, 1200, 1800 or 3600 s");
            }
        }

        yjx::Device device;
        std::string err;
        if (!OpenYjxDeviceForKey(device_key, &device, &err)) {
            return bridge::Failure(request, "device not found");
        }

        yjx::ConfigPatch patch;
        patch.have_debounce_ms = have_deb;
        patch.debounce_ms = debounce;
        patch.have_sleep_min = have_slp;
        patch.sleep_min = sleep_min;
        patch.have_scroll_reverse = have_scroll;
        patch.scroll_reverse = scroll_reverse;

        yjx::Config after;
        if (!device.WriteConfigVerified(patch, &after, &err)) {
            return bridge::Failure(request, err);
        }

        command_json::MiscSetFields fields;
        if (have_deb) {
            fields.have_debounce = true;
            fields.debounce = after.debounce_ms;
        }
        if (have_slp) {
            fields.have_sleep = true;
            fields.sleep = yjx::SleepMinToSec(after.sleep_min);
        }
        if (have_scroll) {
            fields.have_scroll_reverse = true;
            fields.scroll_reverse = after.scroll_reverse;
        }
        return bridge::Ok(request, command_json::BuildMiscSetData(fields));
    }

    if (command == "led.get") {
        yjx::Device device;
        std::string err;
        if (!OpenYjxDeviceForKey(device_key, &device, &err)) {
            return bridge::Failure(request, "device not found");
        }
        yjx::Config cfg;
        if (!device.ReadConfig(&cfg, nullptr, &err) || !yjx::LightModeValid(cfg.light_mode)) {
            return bridge::Ok(request, command_json::BuildLedData(true, "mouse", -1));
        }
        return bridge::Ok(request, command_json::BuildLedData(true, "mouse", cfg.light_mode));
    }

    if (command == "led.set") {
        int mode = -1;
        const ArgState mode_state = opt_int("mode", &mode);
        if (mode_state == ArgState::kAbsent) return bridge::Failure(request, "led.set needs args.mode (0..6)");
        if (mode_state == ArgState::kBad) return bridge::Failure(request, "mode must be an integer");
        if (!yjx::LightModeValid(mode)) return bridge::Failure(request, "mode must be 0..6");

        yjx::Device device;
        std::string err;
        if (!OpenYjxDeviceForKey(device_key, &device, &err)) {
            return bridge::Failure(request, "device not found");
        }
        int readback = -1;
        if (!device.SetLightModeVerified(mode, &readback, &err)) {
            return bridge::Failure(request, err);
        }
        return bridge::Ok(request, command_json::BuildLedData(true, "mouse", readback));
    }

    return bridge::Failure(request, "unknown hardware command");
}

}  // namespace

std::string Handle(const bridge::Request& request, const std::string& device_key) {
    if (request.cmd == "keys.get" || request.cmd == "keys.set") {
        return HandleYjxKeysRequest(request, device_key);
    }
    if (request.cmd == "macros.list" || request.cmd == "macros.save" ||
        request.cmd == "macros.upload" || request.cmd == "macro.import" ||
        request.cmd == "macro.export" || request.cmd == "macros.delete" ||
        request.cmd == "macros.rename") {
        return host::HandleMacroRequest(request, "yjx", device_key);
    }
    if (request.cmd == "dpi.get" || request.cmd == "dpi.set" || request.cmd == "dpi.stage") {
        return HandleYjxDpiRequest(request, device_key);
    }
    if (request.cmd == "rate.get" || request.cmd == "rate.set" ||
        request.cmd == "misc.get" || request.cmd == "misc.set" ||
        request.cmd == "sensor.get" || request.cmd == "sensor.set" ||
        request.cmd == "led.get" || request.cmd == "led.set") {
        return HandleYjxHardwareRequest(request, device_key);
    }
    return bridge::Failure(request, "not implemented: " + request.cmd);
}

}  // namespace yjx_driver
