#include "bridge.h"

#include <cstdio>
#include <mutex>
#include <string>
#include <utility>
#include <vector>

namespace bridge {
namespace {

std::mutex g_lock;
std::vector<DeviceSnapshot> g_devices;
std::vector<KeySnapshot> g_keys;

// Owns the most recent parse tree so Request::args can point into it. One
// request is parsed at a time per thread; the host copies args before the next
// ParseRequest call.
thread_local json::Value g_request_doc;

std::string KeysJsonLocked() {
    std::string data = "[";
    for (std::size_t i = 0; i < g_keys.size(); ++i) {
        const KeySnapshot& key = g_keys[i];
        if (i != 0) data += ',';
        data += "{\"index\":" + std::to_string(key.index);
        data += ",\"matrix\":" + std::to_string(key.matrix);
        data += ",\"code\":" + std::to_string(static_cast<unsigned long long>(key.code));
        data += ",\"binding\":" + JsonQuote(key.binding);
        data += ",\"kind\":" + JsonQuote(key.kind);
        data += '}';
    }
    data += ']';
    return data;
}

}  // namespace

bool ParseRequest(const std::string& text, Request* out, std::string* error) {
    if (out == nullptr) {
        if (error != nullptr) *error = "internal error: null request";
        return false;
    }
    *out = Request{};

    std::string parseError;
    if (!json::Parse(text, &g_request_doc, &parseError)) {
        if (error != nullptr) *error = parseError;
        return false;
    }
    if (g_request_doc.type != json::Value::Type::kObject) {
        if (error != nullptr) *error = "request must be a JSON object";
        return false;
    }

    if (const json::Value* id = g_request_doc.Find("id")) {
        if (id->type != json::Value::Type::kNumber) {
            if (error != nullptr) *error = "id must be an integer";
            return false;
        }
        out->id = id->number;
    }

    const json::Value* cmd = g_request_doc.Find("cmd");
    if (cmd == nullptr || cmd->type != json::Value::Type::kString || cmd->text.empty()) {
        if (error != nullptr) *error = "missing cmd";
        return false;
    }
    out->cmd = cmd->text;

    if (const json::Value* args = g_request_doc.Find("args")) {
        if (args->type != json::Value::Type::kObject) {
            if (error != nullptr) *error = "args must be an object";
            return false;
        }
        out->args = args;
    }
    return true;
}

void SetDevices(std::vector<DeviceSnapshot> devices) {
    std::lock_guard<std::mutex> guard(g_lock);
    g_devices = std::move(devices);
}

void ClearDevices() {
    std::lock_guard<std::mutex> guard(g_lock);
    g_devices.clear();
}

std::vector<DeviceSnapshot> Devices() {
    std::lock_guard<std::mutex> guard(g_lock);
    return g_devices;
}

std::string DevicesJson() {
    std::lock_guard<std::mutex> guard(g_lock);
    std::string data = "[";
    for (std::size_t i = 0; i < g_devices.size(); ++i) {
        const DeviceSnapshot& device = g_devices[i];
        if (i != 0) data += ',';
        data += "{\"key\":" + JsonQuote(device.key);
        data += ",\"name\":" + JsonQuote(device.name);
        data += ",\"kind\":" + JsonQuote(device.kind);
        data += ",\"via\":" + JsonQuote(device.via);
        data += ",\"driverLevel\":" + JsonQuote(device.driver_level);
        data += ",\"driver\":" + JsonQuote(device.driver);
        data += ",\"battery\":" + std::to_string(device.battery);
        data += ",\"charging\":" + std::to_string(device.charging);
        data += ",\"connected\":";
        data += device.connected ? "true" : "false";
        data += ",\"asleep\":";
        data += device.asleep ? "true" : "false";
        data += ",\"mode\":" + JsonQuote(device.mode);
        data += ",\"receiver\":" + JsonQuote(device.receiver);
        data += ",\"psd\":" + std::to_string(device.psd);
        data += ",\"sensor\":" + JsonQuote(device.sensor);
        data += ",\"level\":" + JsonQuote(device.level);
        data += ",\"detail\":" + JsonQuote(device.detail);
        data += '}';
    }
    data += ']';
    return data;
}

std::string Handle(const std::string& requestJson) {
    Request request;
    std::string error;
    if (!ParseRequest(requestJson, &request, &error)) {
        return Failure(request, error);
    }

    if (request.cmd == "devices.get" || request.cmd == "device.probe") {
        return Ok(request, DevicesJson());
    }
    if (request.cmd == "keys.get" || request.cmd == "keys.set") {
        std::lock_guard<std::mutex> guard(g_lock);
        return Ok(request, "{\"bindings\":" + KeysJsonLocked() + "}");
    }
    // Full driver and host commands are dispatched by the host's BridgeHandler.
    return Failure(request, "not implemented: " + request.cmd);
}

std::string BatteryEvent(const std::string& key, int percent) {
    return "{\"event\":\"battery\",\"key\":" + JsonQuote(key)
           + ",\"value\":" + std::to_string(percent) + "}";
}

void SetKeys(std::vector<KeySnapshot> keys) {
    std::lock_guard<std::mutex> guard(g_lock);
    g_keys = std::move(keys);
}

void ClearKeys() {
    std::lock_guard<std::mutex> guard(g_lock);
    g_keys.clear();
}

std::vector<KeySnapshot> Keys() {
    std::lock_guard<std::mutex> guard(g_lock);
    return g_keys;
}

std::string KeysJson() {
    std::lock_guard<std::mutex> guard(g_lock);
    return KeysJsonLocked();
}

bool ParseKeySetArgs(const Request& request, int* index, std::string* binding) {
    if (index == nullptr || binding == nullptr) return false;
    if (request.args == nullptr || request.args->type != json::Value::Type::kObject) {
        return false;
    }

    long long parsed = 0;
    if (!request.args->GetInt("index", &parsed)) return false;

    std::string text;
    if (!request.args->GetString("binding", &text) || text.empty()) return false;

    *index = static_cast<int>(parsed);
    *binding = std::move(text);
    return true;
}

std::string JsonQuote(const std::string& text) { return "\"" + json::Escape(text) + "\""; }

std::string Ok(const Request& request, const std::string& dataJson) {
    return "{\"id\":" + std::to_string(request.id) + ",\"ok\":true,\"data\":" + dataJson + "}";
}

std::string Failure(const Request& request, const std::string& message) {
    return "{\"id\":" + std::to_string(request.id) + ",\"ok\":false,\"error\":\""
           + json::Escape(message) + "\"}";
}

std::string HexCode(std::uint32_t code) {
    char buffer[16];
    std::snprintf(buffer, sizeof(buffer), "%08X", static_cast<unsigned>(code));
    return buffer;
}

}  // namespace bridge
