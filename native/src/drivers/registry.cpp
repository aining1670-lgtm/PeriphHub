#include "drivers/registry.h"

#include <cctype>
#include <string>

#include "drivers/builtin_caps.h"

namespace drivers {
namespace {

std::string ToLowerAscii(const std::string& text) {
    std::string out = text;
    for (char& ch : out) {
        ch = static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));
    }
    return out;
}

enum class CommandFamily {
    kNone,
    kKeys,
    kMacros,
    kDpi,
    kHardware,
    kKeyboard,
};

CommandFamily ClassifyCommand(const std::string& cmd) {
    if (cmd == "keys.get" || cmd == "keys.set") return CommandFamily::kKeys;
    if (cmd == "macros.list" || cmd == "macros.save" || cmd == "macros.upload" ||
        cmd == "macro.import" || cmd == "macro.export" || cmd == "macros.delete" ||
        cmd == "macros.rename") {
        return CommandFamily::kMacros;
    }
    if (cmd == "dpi.get" || cmd == "dpi.set" || cmd == "dpi.stage") {
        return CommandFamily::kDpi;
    }
    if (cmd == "rate.get" || cmd == "rate.set" || cmd == "misc.get" || cmd == "misc.set" ||
        cmd == "sensor.get" || cmd == "sensor.set" || cmd == "led.get" || cmd == "led.set") {
        return CommandFamily::kHardware;
    }
    if (cmd == "kbd.get" || cmd == "kbd.set" || cmd == "kbd.keys.get" ||
        cmd == "kbd.keys.set" || cmd == "kbd.keys.reset" || cmd == "kbd.macro.list" ||
        cmd == "kbd.macro.get" || cmd == "kbd.macro.set" || cmd == "kbd.macro.delete") {
        return CommandFamily::kKeyboard;
    }
    return CommandFamily::kNone;
}

}  // namespace

void Registry::Add(DriverDescriptor d) {
    for (std::string& prefix : d.key_prefixes) {
        prefix = ToLowerAscii(prefix);
    }
    for (DriverDescriptor& existing : drivers_) {
        if (existing.name == d.name) {
            existing = std::move(d);
            return;
        }
    }
    drivers_.push_back(std::move(d));
}

void Registry::SetDefault(const std::string& name) {
    default_name_ = name;
}

const DriverDescriptor* Registry::FindByName(const std::string& name) const {
    if (name.empty()) return nullptr;
    for (const DriverDescriptor& d : drivers_) {
        if (d.name == name) return &d;
    }
    return nullptr;
}

const DriverDescriptor* Registry::FindByKey(const std::string& key) const {
    if (key.empty()) {
        return FindByName(default_name_);
    }
    const std::string lower = ToLowerAscii(key);
    for (const DriverDescriptor& d : drivers_) {
        for (const std::string& prefix : d.key_prefixes) {
            if (!prefix.empty() && lower.rfind(prefix, 0) == 0) {
                return &d;
            }
        }
    }
    return nullptr;
}

const std::vector<DriverDescriptor>& Registry::All() const {
    return drivers_;
}

Registry& Global() {
    static Registry instance;
    return instance;
}

bool IsDriverCommand(const std::string& cmd) {
    return ClassifyCommand(cmd) != CommandFamily::kNone;
}

std::string UnsupportedMessage(const std::string& cmd) {
    switch (ClassifyCommand(cmd)) {
        case CommandFamily::kKeys:
            return "key remapping is not supported by this device";
        case CommandFamily::kMacros:
            return "macros are not supported by this device";
        case CommandFamily::kDpi:
            return "DPI configuration is not supported by this device";
        case CommandFamily::kHardware:
            return "hardware settings are not supported by this device";
        case CommandFamily::kKeyboard:
            return "keyboard configuration is not supported by this device";
        case CommandFamily::kNone:
        default:
            return "not implemented: " + cmd;
    }
}

const DriverDescriptor* ResolveDriver(const Registry& reg,
                                      const std::string& key,
                                      const bridge::DeviceSnapshot* snapshot) {
    const DriverDescriptor* raw = reg.FindByKey(key);
    if (raw != nullptr && raw->battery_snapshot_disables &&
        snapshot != nullptr && snapshot->driver_level == "battery") {
        return nullptr;
    }
    return raw;
}

std::string BuildDeviceInfoJson(const Registry& reg,
                                const std::string& key,
                                const bridge::DeviceSnapshot* snapshot) {
    const std::string effective_key =
        !key.empty() ? key : (snapshot != nullptr ? snapshot->key : std::string());
    const DriverDescriptor* drv = ResolveDriver(reg, effective_key, snapshot);
    const std::string driver_name = (drv != nullptr) ? drv->name : "none";
    const std::string kind =
        (snapshot != nullptr && !snapshot->kind.empty())
            ? snapshot->kind
            : ((drv != nullptr && !drv->default_kind.empty()) ? drv->default_kind : "other");
    const std::string driver_level =
        (snapshot != nullptr && !snapshot->driver_level.empty())
            ? snapshot->driver_level
            : ((drv == nullptr) ? "battery" : "full");
    const std::string caps =
        (drv != nullptr && drv->capabilities) ? drv->capabilities(effective_key, snapshot)
                                              : BuildEmptyCapabilities();

    std::string json = "{";
    json += "\"key\":" + bridge::JsonQuote(effective_key);
    json += ",\"kind\":" + bridge::JsonQuote(kind);
    json += ",\"driver\":" + bridge::JsonQuote(driver_name);
    json += ",\"driverLevel\":" + bridge::JsonQuote(driver_level);
    json += ",\"capabilities\":" + caps;
    json += "}";
    return json;
}

}  // namespace drivers
