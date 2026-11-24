// Unit tests for pure driver descriptor registry and device.info capabilities.
// Pure C++17, no Win32 dependencies, no hardware required.
#include <cstdio>
#include <string>

#include "../native/src/bridge.h"
#include "../native/src/drivers/builtin_caps.h"
#include "../native/src/drivers/registry.h"
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

std::string DemoCapabilities(const std::string&, const bridge::DeviceSnapshot*) {
    return "{\"dpi\":{\"min\":100,\"max\":1600,\"step\":100,\"maxStages\":4,\"xySplit\":false,"
           "\"stageCount\":false},\"rates\":[1000],\"sensor\":null,\"misc\":null,"
           "\"lighting\":null,\"keys\":false,\"macros\":false}";
}

}  // namespace

int main() {
    drivers::Registry reg;
    reg.Add(drivers::IncottDescriptor());
    reg.Add(drivers::YjxDescriptor());
    reg.Add(drivers::RoyuanDescriptor());
    reg.SetDefault("incott");

    // 1. Key prefix matching (case-insensitive), default fallback, unknown -> nullptr.
    {
        const drivers::DriverDescriptor* d_empty = reg.FindByKey("");
        Expect(d_empty != nullptr && d_empty->name == "incott", "empty key falls back to incott");

        const drivers::DriverDescriptor* d_incott = reg.FindByKey("INCOTT:522c");
        Expect(d_incott != nullptr && d_incott->name == "incott",
               "case-insensitive incott prefix");

        const drivers::DriverDescriptor* d_yjx = reg.FindByKey("KSNAKE:a8a5:2255");
        Expect(d_yjx != nullptr && d_yjx->name == "yjx", "case-insensitive KSNAKE prefix -> yjx");

        const drivers::DriverDescriptor* d_royuan = reg.FindByKey("ROYUAN:3151:4015");
        Expect(d_royuan != nullptr && d_royuan->name == "royuan",
               "case-insensitive royuan prefix -> royuan");

        Expect(reg.FindByKey("hid:1234:5678") == nullptr, "unknown key -> nullptr");
        Expect(reg.FindByKey("bt:1122") == nullptr, "bt key -> nullptr");
        Expect(reg.All().size() == 3, "3 built-in descriptors registered");
    }

    // 2. ROYUAN battery-only snapshot disables driver -> "driver":"none".
    {
        bridge::DeviceSnapshot bat;
        bat.key = "royuan:3151:4099";
        bat.kind = "keyboard";
        bat.driver_level = "battery";
        Expect(drivers::ResolveDriver(reg, bat.key, &bat) == nullptr,
               "ResolveDriver returns nullptr for royuan battery snapshot");

        const std::string info_json = drivers::BuildDeviceInfoJson(reg, bat.key, &bat);
        json::Value doc;
        std::string err;
        Expect(json::Parse(info_json, &doc, &err) && doc.type == json::Value::Type::kObject,
               "royuan battery device.info parses");
        std::string drv;
        Expect(doc.GetString("driver", &drv) && drv == "none",
               "royuan battery snapshot outputs driver == none");
    }

    // 3. IsDriverCommand & UnsupportedMessage across all 5 command families.
    {
        Expect(drivers::IsDriverCommand("keys.get") && drivers::IsDriverCommand("keys.set"),
               "keys.* are driver commands");
        Expect(drivers::IsDriverCommand("macros.list") && drivers::IsDriverCommand("macro.import"),
               "macros.* / macro.* are driver commands");
        Expect(drivers::IsDriverCommand("dpi.get") && drivers::IsDriverCommand("dpi.stage"),
               "dpi.* are driver commands");
        Expect(drivers::IsDriverCommand("rate.get") && drivers::IsDriverCommand("misc.set") &&
                   drivers::IsDriverCommand("sensor.get") && drivers::IsDriverCommand("led.set"),
               "rate/misc/sensor/led.* are driver commands");
        Expect(drivers::IsDriverCommand("kbd.get") && drivers::IsDriverCommand("kbd.macro.delete"),
               "kbd.* are driver commands");
        Expect(!drivers::IsDriverCommand("hub.get") && !drivers::IsDriverCommand("devices.get") &&
                   !drivers::IsDriverCommand("foo.bar"),
               "hub.get / devices.get / foo.bar are not driver commands");

        ExpectEqual(drivers::UnsupportedMessage("keys.get"),
                    "key remapping is not supported by this device", "keys unsupported msg");
        ExpectEqual(drivers::UnsupportedMessage("macros.list"),
                    "macros are not supported by this device", "macros unsupported msg");
        ExpectEqual(drivers::UnsupportedMessage("dpi.get"),
                    "DPI configuration is not supported by this device", "dpi unsupported msg");
        ExpectEqual(drivers::UnsupportedMessage("rate.get"),
                    "hardware settings are not supported by this device", "hw unsupported msg");
        ExpectEqual(drivers::UnsupportedMessage("kbd.get"),
                    "keyboard configuration is not supported by this device", "kbd unsupported msg");
    }

    // 4. Community extensibility: registering a custom descriptor exposes it in BuildDeviceInfoJson.
    {
        drivers::DriverDescriptor demo;
        demo.name = "demo";
        demo.key_prefixes = {"demo:"};
        demo.default_kind = "mouse";
        demo.commands = {"dpi.get"};
        demo.capabilities = DemoCapabilities;
        reg.Add(std::move(demo));

        const drivers::DriverDescriptor* found = reg.FindByKey("demo:1234");
        Expect(found != nullptr && found->name == "demo", "custom demo driver matched");

        const std::string info_json = drivers::BuildDeviceInfoJson(reg, "demo:1234", nullptr);
        json::Value doc;
        std::string err;
        Expect(json::Parse(info_json, &doc, &err) && doc.type == json::Value::Type::kObject,
               "demo device.info JSON parses");
        std::string drv, kind, lvl;
        Expect(doc.GetString("driver", &drv) && drv == "demo", "demo driver == demo");
        Expect(doc.GetString("kind", &kind) && kind == "mouse", "demo kind == mouse");
        Expect(doc.GetString("driverLevel", &lvl) && lvl == "full", "demo driverLevel == full");
        const json::Value* caps = doc.Find("capabilities");
        const json::Value* dpi = caps != nullptr ? caps->Find("dpi") : nullptr;
        long long max_dpi = 0;
        Expect(dpi != nullptr && dpi->GetInt("max", &max_dpi) && max_dpi == 1600,
               "demo capabilities.dpi.max == 1600");
    }

    if (g_failures == 0) {
        std::printf("driver_registry_test: OK (prefix, fallback, royuan-battery, families, custom)\n");
        return 0;
    }
    std::printf("driver_registry_test: %d failure(s)\n", g_failures);
    return 1;
}
