// Unit tests for the JSON bridge. No hardware: the device cache is fed by hand.
#include <cstdio>
#include <string>
#include <vector>

#include "../native/src/bridge.h"

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

bridge::DeviceSnapshot Wireless() {
    bridge::DeviceSnapshot device;
    device.key = "incott:522C";
    device.name = "G23";
    device.mode = "wireless";
    device.battery = 32;
    device.connected = true;
    return device;
}

}  // namespace

int main() {
    // Empty cache: an empty JSON array, still a successful reply.
    bridge::ClearDevices();
    ExpectEqual(bridge::Handle(R"({"id":1,"cmd":"devices.get"})"),
                R"({"id":1,"ok":true,"data":[]})", "empty device list");
    ExpectEqual(bridge::Handle(R"({"cmd":"devices.get"})"),
                R"({"id":0,"ok":true,"data":[]})", "missing id defaults to 0");

    // One live row: field names, order and types must match docs/02.
    bridge::SetDevices({Wireless()});
    ExpectEqual(bridge::Handle(R"({"id":7,"cmd":"devices.get","args":{}})"),
                R"({"id":7,"ok":true,"data":[{"key":"incott:522C","name":"G23","kind":"other","via":"","driverLevel":"battery","driver":"none","battery":32,"charging":-1,"connected":true,"asleep":false,"mode":"wireless","receiver":"","psd":-1,"sensor":"","level":"","detail":""}]})",
                "one connected device");

    // Charging device exposes raw 0x89[3] byte (2 = charging in Incott firmware).
    bridge::DeviceSnapshot charging = Wireless();
    charging.charging = 2;
    bridge::SetDevices({charging});
    ExpectEqual(bridge::Handle(R"({"id":17,"cmd":"devices.get"})"),
                R"({"id":17,"ok":true,"data":[{"key":"incott:522C","name":"G23","kind":"other","via":"","driverLevel":"battery","driver":"none","battery":32,"charging":2,"connected":true,"asleep":false,"mode":"wireless","receiver":"","psd":-1,"sensor":"","level":"","detail":""}]})",
                "charging field serialised");

    // Unknown battery and disconnected device: battery -1, connected false.
    bridge::DeviceSnapshot wired;
    wired.key = "incott:622C";
    wired.name = "G23 wired";
    wired.mode = "wired";
    bridge::SetDevices({wired});
    ExpectEqual(bridge::Handle(R"({"id":2,"cmd":"devices.get"})"),
                R"({"id":2,"ok":true,"data":[{"key":"incott:622C","name":"G23 wired","kind":"other","via":"","driverLevel":"battery","driver":"none","battery":-1,"charging":-1,"connected":false,"asleep":false,"mode":"wired","receiver":"","psd":-1,"sensor":"","level":"","detail":""}]})",
                "unknown battery and disconnected");

    // Two devices keep their order.
    bridge::SetDevices({Wireless(), wired});
    Expect(bridge::Handle(R"({"id":3,"cmd":"devices.get"})").find(
               R"("name":"G23","kind":"other","via":"","driverLevel":"battery","driver":"none","battery":32,"charging":-1,"connected":true,"asleep":false,"mode":"wireless","receiver":"","psd":-1,"sensor":"","level":"","detail":""},{"key":"incott:622C")")
               != std::string::npos,
           "two devices keep insertion order");

    // Multi-peripheral snapshot: Incott mouse + Bluetooth headset + 2.4G keyboard.
    {
        bridge::DeviceSnapshot mouse = Wireless();
        mouse.name = "Incott G23";
        mouse.kind = "mouse";
        mouse.via = "2.4 GHz receiver";
        mouse.driver_level = "full";
        mouse.receiver = "1K";
        mouse.psd = 1;
        mouse.sensor = "PAW3395";

        bridge::DeviceSnapshot headset;
        headset.key = "bt:{D699CC78-883F-5C73-A890-F8EE81BB179D}";
        headset.name = "VIVALDI";
        headset.kind = "headset";
        headset.via = "Bluetooth";
        headset.driver_level = "battery";
        headset.mode = "bluetooth";
        headset.battery = 40;
        headset.connected = true;
        headset.detail = "L 40%, R 40%";

        bridge::DeviceSnapshot kbd;
        kbd.key = "ajazzaj:3151:4015";
        kbd.name = "Gaming Keyboard";
        kbd.kind = "keyboard";
        kbd.via = "2.4 GHz receiver";
        kbd.driver_level = "battery";
        kbd.mode = "wireless";
        kbd.battery = -1;
        kbd.connected = false;
        kbd.level = "Low";

        bridge::SetDevices({mouse, headset, kbd});
        const std::string reply = bridge::Handle(R"({"id":18,"cmd":"devices.get"})");
        Expect(reply.find(R"("driverLevel":"full")") != std::string::npos,
               "multi-device full driver field");
        Expect(reply.find(R"("kind":"headset","via":"Bluetooth")") != std::string::npos,
               "multi-device headset field");
        Expect(reply.find(R"("kind":"keyboard","via":"2.4 GHz receiver")") != std::string::npos,
               "multi-device keyboard field");
        Expect(reply.find(R"("detail":"L 40%, R 40%")") != std::string::npos,
               "multi-device detail field");
        Expect(reply.find(R"("level":"Low")") != std::string::npos,
               "multi-device level field");
    }

    // Strings must be escaped, control bytes included.
    bridge::DeviceSnapshot quoted = Wireless();
    quoted.name = "G\"23\\x";
    bridge::SetDevices({quoted});
    ExpectEqual(bridge::Handle(R"({"id":4,"cmd":"devices.get"})"),
                "{\"id\":4,\"ok\":true,\"data\":[{\"key\":\"incott:522C\",\"name\":\"G\\\"23\\\\x\",\"kind\":\"other\",\"via\":\"\",\"driverLevel\":\"battery\",\"driver\":\"none\",\"battery\":32,\"charging\":-1,\"connected\":true,\"asleep\":false,\"mode\":\"wireless\",\"receiver\":\"\",\"psd\":-1,\"sensor\":\"\",\"level\":\"\",\"detail\":\"\"}]}",
                "quote and backslash escaped");

    quoted.name = "G\x01";
    bridge::SetDevices({quoted});
    ExpectEqual(bridge::Handle(R"({"id":5,"cmd":"devices.get"})"),
                "{\"id\":5,\"ok\":true,\"data\":[{\"key\":\"incott:522C\",\"name\":\"G\\u0001\",\"kind\":\"other\",\"via\":\"\",\"driverLevel\":\"battery\",\"driver\":\"none\",\"battery\":32,\"charging\":-1,\"connected\":true,\"asleep\":false,\"mode\":\"wireless\",\"receiver\":\"\",\"psd\":-1,\"sensor\":\"\",\"level\":\"\",\"detail\":\"\"}]}",
                "control byte escaped");

    // Errors: unknown command and missing command.
    ExpectEqual(bridge::Handle(R"({"id":9,"cmd":"dpi.set","args":{"dpi":1600}})"),
                R"({"id":9,"ok":false,"error":"not implemented: dpi.set"})",
                "unknown command");
    ExpectEqual(bridge::Handle("{}"), R"({"id":0,"ok":false,"error":"missing cmd"})",
                "missing cmd");

    // ---- T2: one typed parse, no field slicing ----
    {
        bridge::Request request;
        std::string error;

        // Whitespace between tokens is legal JSON and must parse.
        Expect(bridge::ParseRequest("{ \"id\": 3, \"cmd\": \"devices.get\" }", &request,
                                    &error),
               "spaced JSON parses");
        Expect(request.id == 3 && request.cmd == "devices.get", "spaced JSON keeps fields");
        Expect(request.args == nullptr, "absent args is null");
        ExpectEqual(bridge::Ok(request, "[]"), R"({"id":3,"ok":true,"data":[]})",
                    "Ok serialises the parsed id");

        // id must be a JSON integer, not a string.
        Expect(!bridge::ParseRequest(R"({"id":"7","cmd":"devices.get"})", &request, &error),
               "string id rejected");
        Expect(error.find("id must be an integer") != std::string::npos,
               "string id error text");
        Expect(request.id == 0, "string id leaves id 0");

        // Missing cmd is an error but the already-parsed id is kept.
        Expect(!bridge::ParseRequest(R"({"id":4})", &request, &error), "missing cmd rejected");
        ExpectEqual(error, "missing cmd", "missing cmd error text");
        ExpectEqual(bridge::Handle(R"({"id":4})"),
                    R"({"id":4,"ok":false,"error":"missing cmd"})",
                    "missing cmd reply keeps id");

        // args is optional but must be an object when present.
        Expect(!bridge::ParseRequest(R"({"id":5,"cmd":"devices.get","args":[]})", &request,
                                     &error),
               "array args rejected");
        ExpectEqual(error, "args must be an object", "array args error text");
        Expect(!bridge::ParseRequest("[]", &request, &error), "non-object request rejected");
        ExpectEqual(error, "request must be a JSON object", "non-object request error text");

        // Nested args.cmd must never select the command.
        Expect(bridge::ParseRequest(
                   R"({"id":6,"cmd":"devices.get","args":{"cmd":"keys.get"}})", &request,
                   &error),
               "nested cmd request parses");
        Expect(request.cmd == "devices.get", "top-level cmd wins");
        Expect(request.args != nullptr && request.args->Find("cmd") != nullptr &&
                   request.args->Find("cmd")->text == "keys.get",
               "nested cmd stays inside args");
        bridge::ClearDevices();
        ExpectEqual(bridge::Handle(
                        R"({"id":6,"cmd":"devices.get","args":{"cmd":"keys.get"}})"),
                    R"({"id":6,"ok":true,"data":[]})", "nested cmd ignored by Handle");

        // Malformed JSON gets an id-0 error reply instead of a crash.
        const std::string malformed = bridge::Handle("{not json");
        Expect(malformed.find(R"({"id":0,"ok":false,"error":)") == 0,
               "malformed JSON replies with id 0");
        Expect(malformed.find("expected a string") != std::string::npos,
               "malformed JSON carries the parser message");

        // Negative and large ids round-trip as numbers.
        Expect(bridge::ParseRequest(R"({"id":-2,"cmd":"devices.get"})", &request, &error) &&
                   request.id == -2,
               "negative id kept");
        ExpectEqual(bridge::Ok(request, "[]"), R"({"id":-2,"ok":true,"data":[]})",
                    "negative id serialised");

        // Failure escapes its message through the single escaping helper.
        bridge::Request failRequest;
        failRequest.id = 9;
        ExpectEqual(bridge::Failure(failRequest, "quote \" back \\ ctrl \x01"),
                    R"({"id":9,"ok":false,"error":"quote \" back \\ ctrl \u0001"})",
                    "failure escapes message");
        ExpectEqual(bridge::JsonQuote("a\"b\\c"), R"("a\"b\\c")",
                    "JsonQuote escapes quote and backslash");
    }

    // ClearDevices empties the cache again.
    bridge::SetDevices({Wireless()});
    bridge::ClearDevices();
    ExpectEqual(bridge::Handle(R"({"id":6,"cmd":"devices.get"})"),
                R"({"id":6,"ok":true,"data":[]})", "cache cleared");

    // BatteryEvent matches docs/02.
    ExpectEqual(bridge::BatteryEvent("incott:522C", 32),
                R"({"event":"battery","key":"incott:522C","value":32})", "battery event");

    // ---- P4: keys.get / keys.set ----
    bridge::ClearKeys();
    ExpectEqual(bridge::Handle(R"({"id":10,"cmd":"keys.get"})"),
                R"({"id":10,"ok":true,"data":{"bindings":[]}})", "empty key list");

    bridge::KeySnapshot key;
    key.index = 4;
    key.matrix = 3;
    key.code = 0x00000480;
    key.binding = "KeyA";
    key.kind = "keyboard";
    bridge::SetKeys({key});
    ExpectEqual(
        bridge::Handle(R"({"id":11,"cmd":"keys.get"})"),
        R"({"id":11,"ok":true,"data":{"bindings":[{"index":4,"matrix":3,"code":1152,"binding":"KeyA","kind":"keyboard"}]}})",
        "one key row");
    ExpectEqual(
        bridge::Handle(R"({"id":12,"cmd":"keys.set","args":{"index":4,"binding":"KeyB"}})"),
        R"({"id":12,"ok":true,"data":{"bindings":[{"index":4,"matrix":3,"code":1152,"binding":"KeyA","kind":"keyboard"}]}})",
        "keys.set returns the refreshed cache");

    bridge::Request keysRequest;
    std::string parseError;
    int index = -1;
    std::string binding;
    Expect(bridge::ParseRequest(
               R"({"id":1,"cmd":"keys.set","args":{"index":4,"binding":"KeyA"}})", &keysRequest,
               &parseError),
           "keys.set request parses");
    Expect(bridge::ParseKeySetArgs(keysRequest, &index, &binding) && index == 4 &&
               binding == "KeyA",
           "keys.set args parsed");
    Expect(bridge::ParseRequest(R"({"id":1,"cmd":"keys.set","args":{"binding":"KeyA"}})",
                                &keysRequest, &parseError) &&
               !bridge::ParseKeySetArgs(keysRequest, &index, &binding),
           "keys.set without index rejected");
    Expect(bridge::ParseRequest(R"({"id":1,"cmd":"keys.set","args":{"index":4}})", &keysRequest,
                                &parseError) &&
               !bridge::ParseKeySetArgs(keysRequest, &index, &binding),
           "keys.set without binding rejected");
    Expect(bridge::ParseRequest(
               R"({"id":1,"cmd":"keys.set","args":{"index":"x","binding":"KeyA"}})",
               &keysRequest, &parseError) &&
               !bridge::ParseKeySetArgs(keysRequest, &index, &binding),
           "keys.set with non-integer index rejected");
    Expect(bridge::ParseRequest(R"({"id":1,"cmd":"keys.set"})", &keysRequest, &parseError) &&
               !bridge::ParseKeySetArgs(keysRequest, &index, &binding),
           "keys.set without args rejected");

    bridge::Request failRequest;
    failRequest.id = 9;
    ExpectEqual(bridge::Failure(failRequest, "readback mismatch"),
                R"({"id":9,"ok":false,"error":"readback mismatch"})", "failure keeps id");
    ExpectEqual(bridge::HexCode(0x00000480), "00000480", "hex code 8 digits");
    ExpectEqual(bridge::HexCode(0x00F40001), "00F40001", "hex code uppercase");

    bridge::ClearKeys();
    ExpectEqual(bridge::Handle(R"({"id":13,"cmd":"keys.get"})"),
                R"({"id":13,"ok":true,"data":{"bindings":[]}})", "key cache cleared");

    if (g_failures == 0) {
        std::printf("bridge_test: OK (devices.get, keys.get/set, escaping, errors, event)\n");
        return 0;
    }
    std::printf("bridge_test: %d failure(s)\n", g_failures);
    return 1;
}
