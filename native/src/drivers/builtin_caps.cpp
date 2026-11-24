#include "drivers/builtin_caps.h"

#include <cctype>
#include <string>

#include "driver_router.h"

namespace drivers {
namespace {

std::string ToLowerAscii(const std::string& text) {
    std::string out = text;
    for (char& ch : out) {
        ch = static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));
    }
    return out;
}

bool ContainsAsciiInsensitive(const std::string& haystack, const char* needle) {
    const std::string h = ToLowerAscii(haystack);
    const std::string n = ToLowerAscii(needle);
    return h.find(n) != std::string::npos;
}

std::string BuildIncottCapabilities(const std::string&,
                                    const bridge::DeviceSnapshot* snapshot) {
    const bool is_wireless =
        (snapshot != nullptr) ? (snapshot->mode != "wired") : true;
    const bool is_8k_receiver =
        (snapshot != nullptr) &&
        ContainsAsciiInsensitive(snapshot->receiver, "8k");
    const bool allow_high_rate = is_wireless && is_8k_receiver;

    std::string rates = "[125,250,500,1000";
    if (allow_high_rate) rates += ",2000,4000,8000";
    rates += "]";

    std::string json = "{";
    json += "\"dpi\":{\"min\":50,\"max\":32000,\"step\":50,\"maxStages\":6,\"xySplit\":true,\"stageCount\":true},";
    json += "\"rates\":" + rates + ",";
    json += "\"sensor\":{\"lod\":[\"0.7\",\"1.0\",\"2.0\"],\"modes\":[\"eco\",\"standard\",\"game\"],\"ripple\":true,\"fixline\":true,\"motion\":true},";
    json += "\"misc\":{\"debounce\":{\"min\":0,\"max\":30},\"sleep\":{\"min\":10,\"max\":900,\"step\":10,\"presetsSec\":[60,180,300,900],\"discrete\":false},\"scrollReverse\":false,\"fire\":true},";
    json += "\"lighting\":{\"mouseLed\":[],\"receiverLed\":";
    json += allow_high_rate ? "true" : "false";
    json += "},";
    json += "\"keys\":true,";
    json += "\"macros\":true";
    json += "}";
    return json;
}

std::string BuildYjxCapabilities(const std::string& device_key,
                                 const bridge::DeviceSnapshot* snapshot) {
    std::uint16_t vid = 0;
    const bool has_vid = driver_router::ParseYjxVidFromKey(device_key, &vid);
    const bool is_wired =
        (snapshot != nullptr) ? (snapshot->mode == "wired")
                              : (has_vid && vid == 0xA8A4u);

    std::string json = "{";
    json += "\"dpi\":{\"min\":200,\"max\":12000,\"step\":100,\"maxStages\":6,\"xySplit\":false,\"stageCount\":true},";
    json += "\"rates\":";
    json += is_wired ? "[1000]" : "[125,250,500,1000]";
    json += ",";
    json += "\"sensor\":{\"lod\":[\"1.0\",\"2.0\"],\"modes\":[],\"ripple\":true,\"fixline\":true,\"motion\":true},";
    json += "\"misc\":{\"debounce\":{\"min\":0,\"max\":30},\"sleep\":{\"presetsSec\":[60,180,300,600,1200,1800,3600],\"discrete\":true},\"scrollReverse\":true,\"fire\":false},";
    json += "\"lighting\":{\"mouseLed\":["
            "{\"code\":0,\"label\":\"\xE7\x81\xAD\xE7\x81\xAF\"},"
            "{\"code\":1,\"label\":\"\xE6\xB3\xA2\xE6\xB5\xAA\"},"
            "{\"code\":2,\"label\":\"\xE9\x9C\x93\xE8\x99\xB9\"},"
            "{\"code\":3,\"label\":\"\xE5\xB7\xA1\xE5\x9B\x9E\xE9\x97\xAA\"},"
            "{\"code\":4,\"label\":\"YOYO\xE7\x90\x83\"},"
            "{\"code\":5,\"label\":\"\xE5\x8D\x95\xE5\x90\x91\xE9\x97\xAA\"},"
            "{\"code\":6,\"label\":\"\xE5\xBE\xAA\xE7\x8E\xAF\xE5\x91\xBC\xE5\x90\xB8\"}"
            "],\"receiverLed\":false},";
    json += "\"keys\":true,";
    json += "\"macros\":true";
    json += "}";
    return json;
}

std::string BuildRoyuanCapabilities(const std::string&,
                                    const bridge::DeviceSnapshot*) {
    std::string json = "{";
    json += "\"dpi\":null,\"rates\":[],\"sensor\":null,\"misc\":null,"
            "\"lighting\":null,\"keys\":false,\"macros\":false,";
    json += "\"keyboard\":{";
    json += "\"model\":\"AKOS BC98\",";
    json += "\"layout\":\"bc98\",";
    json += "\"fnSlot\":65,";
    json += "\"profiles\":[0,1,2,3],";
    json += "\"keymap\":true,";
    json += "\"fnLayer\":true,";
    json += "\"macros\":{\"maxSlots\":50,\"defaultVisibleSlots\":16,\"maxStreamBytes\":250},";
    json += "\"brightnessMax\":4,";
    json += "\"speedMax\":4,";
    json += "\"debounce\":{\"min\":1,\"max\":10},";
    json += "\"sleep\":{\"minSec\":60,\"maxSec\":3600,"
            "\"shallowPresetsSec\":[60,120,300,600,1800],"
            "\"deepPresetsSec\":[600,1200,1680,1800,3600]},";
    json += "\"options\":[\"winLock\",\"macMode\",\"wasdSwap\",\"backlightOff\",\"sidelightOff\"],";
    json += "\"mainModes\":["
            "{\"code\":1,\"label\":\"\xE5\xB8\xB8\xE4\xBA\xAE\",\"supportsColor\":true},"
            "{\"code\":2,\"label\":\"\xE5\x91\xBC\xE5\x90\xB8\",\"supportsColor\":true},"
            "{\"code\":3,\"label\":\"\xE5\x85\x89\xE8\xB0\xB1\xE5\xBE\xAA\xE7\x8E\xAF\",\"supportsColor\":false},"
            "{\"code\":4,\"label\":\"\xE6\xB3\xA2\xE6\xB5\xAA\",\"supportsColor\":true},"
            "{\"code\":5,\"label\":\"\xE6\xB6\x9F\xE6\xBC\xAA\xE6\x89\xA9\xE6\x95\xA3\",\"supportsColor\":true},"
            "{\"code\":6,\"label\":\"\xE7\xB9\x81\xE6\x98\x9F\xE7\x82\xB9\xE7\x82\xB9\",\"supportsColor\":true},"
            "{\"code\":7,\"label\":\"\xE6\xB5\x81\xE5\x85\x89\xE6\xBA\xA2\xE5\xBD\xA9\",\"supportsColor\":true},"
            "{\"code\":8,\"label\":\"\xE8\xB8\x8F\xE9\x9B\xAA\xE6\x97\xA0\xE7\x97\x95\",\"supportsColor\":true},"
            "{\"code\":9,\"label\":\"\xE5\xB1\x82\xE5\xB3\xA6\xE5\x8F\xA0\xE5\xB6\x82\",\"supportsColor\":true},"
            "{\"code\":10,\"label\":\"\xE6\xAD\xA3\xE5\xBC\xA6\xE6\xB3\xA2\",\"supportsColor\":true},"
            "{\"code\":11,\"label\":\"\xE5\xBC\xB9\xE7\xB0\xA7\xE6\xB6\x8C\xE5\x8A\xA8\",\"supportsColor\":true},"
            "{\"code\":12,\"label\":\"\xE9\x9C\x93\xE8\x99\xB9\",\"supportsColor\":false},"
            "{\"code\":14,\"label\":\"\xE6\x94\xBE\xE5\xB0\x84\xE6\x89\xA9\xE6\x95\xA3\",\"supportsColor\":true},"
            "{\"code\":15,\"label\":\"\xE7\x8E\xAF\xE5\xBD\xA2\xE6\xB5\x81\xE5\x85\x89\",\"supportsColor\":true},"
            "{\"code\":16,\"label\":\"\xE5\xA4\x9A\xE5\xBD\xA9\xE7\x9F\xA9\xE9\x98\xB5\",\"supportsColor\":false},"
            "{\"code\":17,\"label\":\"\xE9\xA3\x98\xE9\x9B\xAA\",\"supportsColor\":true},"
            "{\"code\":18,\"label\":\"\xE6\xB5\x81\xE6\x98\x9F\",\"supportsColor\":true},"
            "{\"code\":19,\"label\":\"\xE9\x9D\x99\xE8\xB0\xA7\xE8\x90\xBD\xE9\x9B\xAA\",\"supportsColor\":true}"
            "],";
    json += "\"sideModes\":[]";
    json += "}";
    json += "}";
    return json;
}

std::vector<std::string> MouseCommands() {
    return {
        "keys.get",    "keys.set",     "macros.list",  "macros.save", "macros.upload",
        "macro.import","macro.export", "macros.delete","macros.rename","dpi.get",
        "dpi.set",     "dpi.stage",    "rate.get",     "rate.set",    "misc.get",
        "misc.set",    "sensor.get",   "sensor.set",   "led.get",     "led.set",
    };
}

}  // namespace

std::string BuildEmptyCapabilities() {
    return "{\"dpi\":null,\"rates\":[],\"sensor\":null,\"misc\":null,"
           "\"lighting\":null,\"keys\":false,\"macros\":false}";
}

DriverDescriptor IncottDescriptor() {
    DriverDescriptor d;
    d.name = "incott";
    d.key_prefixes = {"incott:"};
    d.default_kind = "mouse";
    d.commands = MouseCommands();
    d.capabilities = BuildIncottCapabilities;
    return d;
}

DriverDescriptor YjxDescriptor() {
    DriverDescriptor d;
    d.name = "yjx";
    d.key_prefixes = {"ksnake:"};
    d.default_kind = "mouse";
    d.commands = MouseCommands();
    d.capabilities = BuildYjxCapabilities;
    return d;
}

DriverDescriptor RoyuanDescriptor() {
    DriverDescriptor d;
    d.name = "royuan";
    d.key_prefixes = {"royuan:3151:"};
    d.default_kind = "keyboard";
    d.commands = {
        "kbd.get",        "kbd.set",         "kbd.keys.get",   "kbd.keys.set",
        "kbd.keys.reset", "kbd.macro.list",  "kbd.macro.get",  "kbd.macro.set",
        "kbd.macro.delete",
    };
    d.capabilities = BuildRoyuanCapabilities;
    d.battery_snapshot_disables = true;
    return d;
}

}  // namespace drivers
