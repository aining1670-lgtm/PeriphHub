#pragma once
// JSON bridge between the web UI and native code. No business logic lives here.
#include <cstdint>
#include <string>
#include <vector>

#include "json.h"

namespace bridge {

// One row of `devices.get` (docs/02-bridge-protocol.md). The host's poller fills
// this cache; a request only reads it, so `devices.get` never touches hardware.
struct DeviceSnapshot {
    std::string key;                      // "incott:522C" or "bt:{...}" or "hid:VID:PID"
    std::string name;                     // display name ("Incott G23", "VIVALDI", ...)
    std::string kind = "other";           // "mouse", "keyboard", "headset", "gamepad", ...
    std::string via;                      // "2.4 GHz receiver", "Bluetooth", "USB", "HID", "XInput"
    std::string driver_level = "battery"; // "full" or "battery"
    std::string driver = "none";          // "incott", "yjx", "royuan", or "none"
    std::string mode;                     // "wireless", "wired", or "bluetooth"
    std::string receiver;                 // "1K", "4K", "8K"
    std::string sensor;                   // "PAW3395", "PAW3950", or "unknown"
    int psd = -1;                         // raw psd byte from 0x8F
    int battery = -1;                     // percent, -1 when unknown
    int charging = -1;                    // 2 = charging, 0+ = discharging, -1 = unknown
    bool connected = false;
    bool asleep = false;
    std::string level;                    // coarse level like "Low", "Medium", "Full"
    std::string detail;                   // extra info like "L 80%, R 75%, case 90%"
};

// Replaces the cached list served by the next `devices.get`.
void SetDevices(std::vector<DeviceSnapshot> devices);
void ClearDevices();

// Snapshot of the cache, for the tray tip and future widgets.
std::vector<DeviceSnapshot> Devices();

// Serialises the cached device list as a JSON array (`devices.get` / `device.probe`).
std::string DevicesJson();

// Parsed request envelope. `args` points into the parse tree owned by
// ParseRequest (see below); it is nullptr when the request has no `args`.
struct Request {
    long long id = 0;
    std::string cmd;
    const json::Value* args = nullptr;
};

// Parses one request object. `id` must be an integer (missing -> 0), `cmd` must
// be a non-empty string, `args` is optional and must be an object when present.
// On failure returns false and writes a user-facing message to `error`;
// `out->id` keeps the id parsed so far (0 when it was missing/invalid) so the
// host can still reply with the right id.
//
// The parsed document lives in thread-local storage: `out->args` stays valid
// until the next ParseRequest call on the same thread. Copy values you need
// before parsing another request.
bool ParseRequest(const std::string& text, Request* out, std::string* error);

// Handles one request line and returns one response line:
//   {"id":1,"cmd":"devices.get"} -> {"id":1,"ok":true,"data":[{"key":...}]}
std::string Handle(const std::string& requestJson);

// Serialises an event for the web side (see docs/02-bridge-protocol.md).
std::string BatteryEvent(const std::string& key, int percent);

// One row of `keys.get`: UI index 0..5, the firmware matrix behind it, and the
// decoded 32-bit code. The host reads the hardware; the bridge only formats.
struct KeySnapshot {
    int index = 0;
    int matrix = 0;
    std::uint32_t code = 0;
    std::string binding;  // "KeyA", "Mouse:Back", ...
    std::string kind;     // "keyboard", "mouse", "media", ...
};

void SetKeys(std::vector<KeySnapshot> keys);
void ClearKeys();
std::vector<KeySnapshot> Keys();

// `{"bindings":[...]}` body used by both keys.get and keys.set.
std::string KeysJson();

// Pulls `index` (integer) and `binding` (non-empty string) out of a request's
// parsed args object.
bool ParseKeySetArgs(const Request& request, int* index, std::string* binding);

// `{"id":N,"ok":false,"error":"..."}` keeping the request id.
std::string Failure(const Request& request, const std::string& message);

// `{"id":N,"ok":true,"data":<dataJson>}` keeping the request id. For handlers
// implemented in the host (macros.*) that need the device.
std::string Ok(const Request& request, const std::string& dataJson);

// JSON string literal (with quotes) for building data payloads in the host.
std::string JsonQuote(const std::string& text);

// 0x00F40001 -> "00F40001" (8 uppercase hex digits, no prefix).
std::string HexCode(std::uint32_t code);

}  // namespace bridge
