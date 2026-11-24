#pragma once
// Pure driver descriptor registry and `device.info` capability builder.
// Kept free of Win32 dependencies so it can be unit-tested standalone.
#include <functional>
#include <string>
#include <vector>

#include "bridge.h"

namespace drivers {

using CapsFn = std::function<std::string(const std::string& key,
                                         const bridge::DeviceSnapshot* snapshot)>;
using HandleFn = std::function<std::string(const bridge::Request& request,
                                           const std::string& key)>;

struct DriverDescriptor {
    std::string name;
    std::vector<std::string> key_prefixes;  // lowercase prefixes (e.g. "incott:", "royuan:3151:")
    std::string default_kind;               // "mouse", "keyboard", ...
    std::vector<std::string> commands;      // bridge commands supported by this driver
    CapsFn capabilities;
    HandleFn handle;                        // optional in pure unit tests
    bool battery_snapshot_disables = false;
};

class Registry {
 public:
    void Add(DriverDescriptor d);
    void SetDefault(const std::string& name);
    const DriverDescriptor* FindByKey(const std::string& key) const;
    const DriverDescriptor* FindByName(const std::string& name) const;
    const std::vector<DriverDescriptor>& All() const;

 private:
    std::vector<DriverDescriptor> drivers_;
    std::string default_name_;
};

Registry& Global();
void RegisterBuiltinDrivers(Registry& r);

bool IsDriverCommand(const std::string& cmd);
std::string UnsupportedMessage(const std::string& cmd);

const DriverDescriptor* ResolveDriver(const Registry& reg,
                                      const std::string& key,
                                      const bridge::DeviceSnapshot* snapshot);

std::string BuildDeviceInfoJson(const Registry& reg,
                                const std::string& key,
                                const bridge::DeviceSnapshot* snapshot);

}  // namespace drivers
