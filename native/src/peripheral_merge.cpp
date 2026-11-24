#include "peripheral_merge.h"

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <unordered_set>

#include "json.h"

namespace peripherals {
namespace {

std::string ToLowerAscii(const std::string& text) {
    std::string out = text;
    for (char& ch : out) {
        ch = static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));
    }
    return out;
}

bool ContainsTokenWord(const std::string& lower, const std::string& token) {
    std::size_t pos = lower.find(token);
    while (pos != std::string::npos) {
        const bool left_ok = (pos == 0) ||
                             !std::isalnum(static_cast<unsigned char>(lower[pos - 1]));
        const std::size_t end = pos + token.size();
        const bool right_ok = (end == lower.size()) ||
                              !std::isalnum(static_cast<unsigned char>(lower[end]));
        if (left_ok && right_ok) return true;
        pos = lower.find(token, pos + 1);
    }
    return false;
}

bool IsValidKind(const std::string& kind) {
    return kind == "mouse" || kind == "keyboard" || kind == "headset" ||
           kind == "gamepad" || kind == "phone" || kind == "microphone" ||
           kind == "other";
}

std::string InferKindFromName(const std::string& lower_name) {
    if (lower_name.find("keyboard") != std::string::npos ||
        ContainsTokenWord(lower_name, "kbd") ||
        lower_name.find("\xE9\x94\xAE\xE7\x9B\x98") != std::string::npos) {
        return "keyboard";
    }
    if (lower_name.find("headset") != std::string::npos ||
        lower_name.find("headphone") != std::string::npos ||
        lower_name.find("earbud") != std::string::npos ||
        lower_name.find("earphone") != std::string::npos ||
        lower_name.find("buds") != std::string::npos ||
        lower_name.find("airpods") != std::string::npos ||
        lower_name.find("hands-free") != std::string::npos ||
        lower_name.find("\xE8\x80\xB3\xE6\x9C\xBA") != std::string::npos) {
        return "headset";
    }
    if (lower_name.find("mouse") != std::string::npos ||
        lower_name.find("\xE9\xBC\xA0\xE6\xA0\x87") != std::string::npos) {
        return "mouse";
    }
    if (lower_name.find("gamepad") != std::string::npos ||
        lower_name.find("controller") != std::string::npos ||
        lower_name.find("dualsense") != std::string::npos ||
        lower_name.find("dualshock") != std::string::npos ||
        lower_name.find("xbox") != std::string::npos ||
        lower_name.find("\xE6\x89\x8B\xE6\x9F\x84") != std::string::npos) {
        return "gamepad";
    }
    if (lower_name.find("microphone") != std::string::npos ||
        ContainsTokenWord(lower_name, "mic") ||
        lower_name.find("\xE9\xBA\xA6\xE5\x85\x8B\xE9\xA3\x8E") != std::string::npos) {
        return "microphone";
    }
    if (lower_name.find("phone") != std::string::npos ||
        lower_name.find("iphone") != std::string::npos ||
        lower_name.find("\xE6\x89\x8B\xE6\x9C\xBA") != std::string::npos) {
        return "phone";
    }
    return "other";
}

bool IsHexDigit(char ch) {
    return (ch >= '0' && ch <= '9') || (ch >= 'a' && ch <= 'f');
}

// Extracts lowercase "vid:pid" when present in a key (e.g. "ajazzaj:3151:4015" -> "3151:4015",
// "incott:522C" -> "093a:522c"). Returns empty string when none applies.
std::string ExtractVidPid(const std::string& lower_key) {
    if (lower_key.rfind("incott:", 0) == 0 && lower_key.size() == 11) {
        const std::string pid = lower_key.substr(7);
        if (std::all_of(pid.begin(), pid.end(), IsHexDigit)) {
            return "093a:" + pid;
        }
    }
    for (std::size_t i = 0; i + 9 <= lower_key.size(); ++i) {
        if (i > 0 && lower_key[i - 1] != ':') continue;
        if (lower_key[i + 4] != ':') continue;
        if (i + 9 < lower_key.size() && lower_key[i + 9] != ':') continue;
        bool ok = true;
        for (int j = 0; j < 9; ++j) {
            if (j == 4) continue;
            if (!IsHexDigit(lower_key[i + j])) {
                ok = false;
                break;
            }
        }
        if (ok) return lower_key.substr(i, 9);
    }
    return {};
}

bool ParseHexIdValue(const json::Value* obj, const char* key, unsigned* out) {
    if (obj == nullptr || out == nullptr) return false;
    const json::Value* val = obj->Find(key);
    if (val == nullptr) return false;
    if (val->type == json::Value::Type::kNumber && val->number >= 0 && val->number <= 0xFFFF) {
        *out = static_cast<unsigned>(val->number);
        return true;
    }
    if (val->type == json::Value::Type::kString && !val->text.empty()) {
        char* end = nullptr;
        const unsigned long parsed = std::strtoul(val->text.c_str(), &end, 16);
        if (end != val->text.c_str() && *end == '\0' && parsed <= 0xFFFFul) {
            *out = static_cast<unsigned>(parsed);
            return true;
        }
    }
    return false;
}

std::string Hex4(unsigned value) {
    char buf[8]{};
    std::snprintf(buf, sizeof(buf), "%04X", value & 0xFFFFu);
    return buf;
}

int BucketOf(const bridge::DeviceSnapshot& d) {
    if (d.driver_level == "full") return 0;
    if (d.connected) return 1;
    return 2;
}

}  // namespace

bool IsRoyuanDonglePid(std::uint16_t pid) {
    switch (pid) {
        case 0x4011:
        case 0x4014:
        case 0x4017:
        case 0x4021:
        case 0x4027:
        case 0x402D:
        case 0x4038:
            return true;
        default:
            return false;
    }
}

void BuildRoyuanGetRevPacket(std::uint8_t out[kRoyuanPacketLen]) {
    std::memset(out, 0, kRoyuanPacketLen);
    out[0] = 0x00;
    out[1] = 0x80;
    out[8] = 0x7F;
}

bool ParseRoyuanFirmwareReply(const std::uint8_t reply[kRoyuanPacketLen],
                              std::string* version_out) {
    if (reply[1] != 0x80u || (reply[2] == 0u && reply[3] == 0u)) return false;
    if (version_out != nullptr) {
        *version_out = "v" + std::to_string(static_cast<unsigned>(reply[3])) + "." +
                       std::to_string(static_cast<unsigned>(reply[2]));
    }
    return true;
}

void BuildRoyuanDongleStatusPacket(std::uint8_t out[kRoyuanPacketLen]) {
    std::memset(out, 0, kRoyuanPacketLen);
    out[0] = 0x00;
    out[1] = 0xF7;
}

bool ParseRoyuanDongleStatus(const std::uint8_t reply[kRoyuanPacketLen],
                             bool* online,
                             int* battery) {
    const std::uint8_t* payload = reply + 1;
    if ((payload[6] & 0x01u) == 0u) return false;
    const bool is_online = (payload[3] == 0u);
    if (online != nullptr) *online = is_online;
    if (battery != nullptr) {
        *battery = (is_online && payload[1] <= 100u) ? static_cast<int>(payload[1]) : -1;
    }
    return true;
}

void BuildRoyuanGetDevIdPacket(std::uint8_t out[kRoyuanPacketLen]) {
    std::memset(out, 0, kRoyuanPacketLen);
    out[0] = 0x00;
    out[1] = 0x8F;
    out[8] = 0x70;
}

bool ParseRoyuanDevIdReply(const std::uint8_t reply[kRoyuanPacketLen],
                           std::uint16_t* dev_id_out) {
    if (reply == nullptr) return false;
    const std::uint8_t* p = reply + 1;
    if (p[0] != 0x8Fu) return false;
    const std::uint16_t dev_id =
        static_cast<std::uint16_t>(p[1]) | (static_cast<std::uint16_t>(p[2]) << 8);
    if (dev_id == 0u || dev_id == 0xFFFFu) return false;
    if (dev_id_out != nullptr) *dev_id_out = dev_id;
    return true;
}

bool IsAkosBc98Board(std::uint16_t dev_id) {
    return dev_id == 0x03AEu;
}

std::string ResolveRoyuanModelName(std::uint16_t dev_id, const std::string& raw_name) {
    const std::string lower = ToLowerAscii(raw_name);
    const bool generic = raw_name.empty() || lower == "gaming keyboard" ||
                         lower == "royuan keyboard" || lower == "usb keyboard" ||
                         lower == "2.4g wireless receiver" ||
                         lower.find("receiver") != std::string::npos;
    if (IsAkosBc98Board(dev_id) && generic) {
        return "AKOS BC98";
    }
    if (!raw_name.empty()) return raw_name;
    return "ROYUAN Keyboard";
}

bool IsRoyuanBtHint(const std::string& inst_or_id, const std::string& name) {
    const std::string lower_inst = ToLowerAscii(inst_or_id);
    const std::string lower_name = ToLowerAscii(name);
    return lower_inst.find("vid&023151") != std::string::npos ||
           lower_inst.find("vid&013151") != std::string::npos ||
           lower_inst.find("vid_3151") != std::string::npos ||
           lower_name.find("bc98") != std::string::npos ||
           lower_name.find("akos") != std::string::npos;
}

std::string NormalizeRoyuanBtName(const std::string& name) {
    if (name.empty()) return "AKOS BC98";
    const std::string lower = ToLowerAscii(name);
    if (lower.find("bc98") != std::string::npos && lower.find("akos") == std::string::npos) {
        return "AKOS " + name;
    }
    return name;
}

bool IsKsnakeX11Device(std::uint16_t vid, std::uint16_t pid) {
    return (vid == kKsnakeVidUsb || vid == kKsnakeVidDongle) && pid == kKsnakePidX11;
}

bool IsKsnakeDongleVid(std::uint16_t vid) {
    return vid == kKsnakeVidDongle;
}

void BuildKsnakeBatteryPacket(std::uint8_t out[kKsnakePacketLen]) {
    std::memset(out, 0, kKsnakePacketLen);
    out[0] = 0x00;
    out[1] = 0x55;
    out[2] = 0x30;
    out[3] = 0xA5;
    out[4] = 0x0B;
    out[5] = 0x2E;
    out[6] = 0x01;
    out[7] = 0x01;
}

bool ParseKsnakeBatteryReply(const std::uint8_t reply[kKsnakePacketLen],
                             int* battery,
                             int* charging) {
    // Windows ReadFile prepends Report ID 0x00 at reply[0]; WebHID payload starts at reply[1].
    const std::uint8_t* payload = reply + 1;
    if (payload[0] != 0xAAu) return false;

    int raw_battery = -1;
    int raw_charge = -1;
    if (payload[1] == 0x30u) {
        raw_battery = static_cast<int>(payload[8]);
        raw_charge = static_cast<int>(payload[9]);
    } else if (payload[1] == 0xFAu && payload[8] == 0xD0u) {
        raw_battery = static_cast<int>(payload[9]);
        raw_charge = static_cast<int>(payload[10]);
    } else {
        return false;
    }

    if (raw_battery < 0 || raw_battery > 100) return false;
    if (raw_battery == 0 && raw_charge == 0) return false;

    if (battery != nullptr) *battery = raw_battery;
    if (charging != nullptr) *charging = (raw_charge != 0) ? 2 : 0;
    return true;
}

void BuildKsnakeOnlinePacket(std::uint8_t out[kKsnakePacketLen]) {
    std::memset(out, 0, kKsnakePacketLen);
    out[0] = 0x00;
    out[1] = 0x55;
    out[2] = 0xED;
    out[3] = 0x00;
    out[4] = 0x01;
    out[5] = 0x2E;
}

bool ParseKsnakeOnlineReply(const std::uint8_t reply[kKsnakePacketLen], bool* online) {
    const std::uint8_t* payload = reply + 1;
    if (payload[0] != 0xAAu || payload[1] != 0xEDu) return false;
    if (online != nullptr) *online = (payload[8] == 2u);
    return true;
}

std::string NormalizeKind(const std::string& raw_kind, const std::string& name) {
    std::string kind = ToLowerAscii(raw_kind);
    if (!IsValidKind(kind)) kind = "other";

    const std::string lower_name = ToLowerAscii(name);
    if (kind == "mouse" || kind == "other") {
        if (lower_name.find("keyboard") != std::string::npos ||
            ContainsTokenWord(lower_name, "kbd") ||
            lower_name.find("\xE9\x94\xAE\xE7\x9B\x98") != std::string::npos) {
            return "keyboard";
        }
    }
    if (kind == "other") {
        return InferKindFromName(lower_name);
    }
    return kind;
}

std::string ModeFromVia(const std::string& via) {
    const std::string lower = ToLowerAscii(via);
    if (lower.find("bluetooth") != std::string::npos || ContainsTokenWord(lower, "ble")) {
        return "bluetooth";
    }
    if (lower.find("usb") != std::string::npos || lower.find("wired") != std::string::npos) {
        return "wired";
    }
    return "wireless";
}

bool ParseUpstreamJson(const std::string& json_text,
                       std::vector<bridge::DeviceSnapshot>* out,
                       std::string* error) {
    if (out == nullptr) return false;
    out->clear();

    json::Value doc;
    std::string parse_err;
    if (!json::ParseAllowFloat(json_text, &doc, &parse_err)) {
        if (error != nullptr) *error = "invalid upstream JSON: " + parse_err;
        return false;
    }
    if (doc.type != json::Value::Type::kArray) {
        if (error != nullptr) *error = "upstream JSON must be an array";
        return false;
    }

    for (const json::Value& item : doc.array) {
        if (item.type != json::Value::Type::kObject) continue;

        std::string key;
        std::string name;
        if (!item.GetString("key", &key) || key.empty()) continue;
        if (!item.GetString("name", &name) || name.empty()) continue;

        bridge::DeviceSnapshot snap;
        snap.key = std::move(key);
        snap.name = std::move(name);
        item.GetString("via", &snap.via);

        std::string raw_kind;
        item.GetString("kind", &raw_kind);
        snap.kind = NormalizeKind(raw_kind, snap.name);
        snap.mode = ModeFromVia(snap.via);
        snap.driver_level = "battery";

        long long pct = -1;
        if (item.GetInt("percent", &pct)) {
            if (pct < 0) snap.battery = -1;
            else if (pct > 100) snap.battery = 100;
            else snap.battery = static_cast<int>(pct);
        }

        bool connected = false;
        if (item.GetBool("connected", &connected)) snap.connected = connected;
        bool asleep = false;
        if (item.GetBool("asleep", &asleep)) snap.asleep = asleep;

        bool is_charging = false;
        item.GetBool("charging", &is_charging);
        if (is_charging) {
            snap.charging = 2;
        } else if (snap.connected && snap.battery >= 0) {
            snap.charging = 0;
        } else {
            snap.charging = -1;
        }

        item.GetString("level", &snap.level);
        item.GetString("detail", &snap.detail);
        out->push_back(std::move(snap));
    }
    return true;
}

bool ParseHeadsetControlJson(const std::string& json_text,
                             std::vector<bridge::DeviceSnapshot>* out,
                             std::string* error) {
    if (out == nullptr) return false;
    out->clear();

    json::Value doc;
    std::string parse_err;
    if (!json::ParseAllowFloat(json_text, &doc, &parse_err)) {
        if (error != nullptr) *error = "invalid HeadsetControl JSON: " + parse_err;
        return false;
    }
    if (doc.type != json::Value::Type::kObject) {
        if (error != nullptr) *error = "HeadsetControl JSON must be an object";
        return false;
    }
    const json::Value* devices = doc.Find("devices");
    if (devices == nullptr || devices->type != json::Value::Type::kArray) {
        if (error != nullptr) *error = "HeadsetControl JSON missing devices array";
        return false;
    }

    for (const json::Value& item : devices->array) {
        if (item.type != json::Value::Type::kObject) continue;

        std::string name;
        if (!item.GetString("device", &name) || name.empty()) continue;

        unsigned vid = 0;
        unsigned pid = 0;
        const bool have_ids = ParseHexIdValue(&item, "id_vendor", &vid) &&
                              ParseHexIdValue(&item, "id_product", &pid);

        std::string key = have_ids ? ("hsc:" + Hex4(vid) + ":" + Hex4(pid))
                                   : ("hsc:" + ToLowerAscii(name));

        std::string status_str;
        long long level = -1;
        if (const json::Value* bat = item.Find("battery")) {
            if (bat->type == json::Value::Type::kObject) {
                bat->GetString("status", &status_str);
                bat->GetInt("level", &level);
            }
        }

        bridge::DeviceSnapshot snap;
        snap.key = std::move(key);
        snap.name = std::move(name);
        snap.kind = "headset";
        snap.via = "2.4 GHz receiver";
        snap.mode = "wireless";
        snap.driver_level = "battery";
        snap.connected = (status_str == "BATTERY_AVAILABLE" || status_str == "BATTERY_CHARGING");
        if (snap.connected && level >= 0 && level <= 100) {
            snap.battery = static_cast<int>(level);
        } else {
            snap.battery = -1;
        }
        if (status_str == "BATTERY_CHARGING") {
            snap.charging = 2;
        } else if (snap.connected && snap.battery >= 0) {
            snap.charging = 0;
        } else {
            snap.charging = -1;
        }
        out->push_back(std::move(snap));
    }
    return true;
}

std::vector<bridge::DeviceSnapshot> MergeDevices(
    const std::vector<bridge::DeviceSnapshot>& native_incott,
    const std::vector<bridge::DeviceSnapshot>& native_vendor,
    const std::vector<bridge::DeviceSnapshot>& upstream_devices,
    const std::vector<bridge::DeviceSnapshot>& native_bt,
    const std::vector<bridge::DeviceSnapshot>& headsetcontrol_devices,
    const std::vector<bridge::DeviceSnapshot>& native_hid,
    const std::vector<bridge::DeviceSnapshot>& native_xinput) {
    std::vector<bridge::DeviceSnapshot> merged;
    std::unordered_set<std::string> seen_keys;
    std::unordered_set<std::string> seen_vid_pids;
    std::unordered_set<std::string> seen_bt_names;

    const bool has_native_incott = !native_incott.empty();

    auto record_seen = [&](const bridge::DeviceSnapshot& d) {
        const std::string lower_key = ToLowerAscii(d.key);
        seen_keys.insert(lower_key);
        const std::string vp = ExtractVidPid(lower_key);
        if (!vp.empty()) seen_vid_pids.insert(vp);
        if (d.mode == "bluetooth" || lower_key.rfind("bt:", 0) == 0) {
            seen_bt_names.insert(ToLowerAscii(d.name));
        }
    };

    for (bridge::DeviceSnapshot d : native_incott) {
        d.driver_level = "full";
        if (d.kind.empty() || d.kind == "other") d.kind = "mouse";
        const std::string lower_key = ToLowerAscii(d.key);
        if (seen_keys.count(lower_key)) continue;
        record_seen(d);
        merged.push_back(std::move(d));
    }

    auto append_vid_pid_checked = [&](const std::vector<bridge::DeviceSnapshot>& source) {
        for (bridge::DeviceSnapshot d : source) {
            const std::string lower_key = ToLowerAscii(d.key);
            if (seen_keys.count(lower_key)) continue;
            const std::string vp = ExtractVidPid(lower_key);
            if (!vp.empty() && seen_vid_pids.count(vp)) continue;
            record_seen(d);
            merged.push_back(std::move(d));
        }
    };

    append_vid_pid_checked(native_vendor);

    for (bridge::DeviceSnapshot d : upstream_devices) {
        const std::string lower_key = ToLowerAscii(d.key);
        if (has_native_incott && lower_key.rfind("incott:", 0) == 0) continue;
        if (seen_keys.count(lower_key)) continue;
        const std::string vp = ExtractVidPid(lower_key);
        if (!vp.empty() && seen_vid_pids.count(vp)) continue;
        record_seen(d);
        merged.push_back(std::move(d));
    }

    for (bridge::DeviceSnapshot d : native_bt) {
        const std::string lower_key = ToLowerAscii(d.key);
        const std::string lower_name = ToLowerAscii(d.name);
        if (seen_keys.count(lower_key) || seen_bt_names.count(lower_name)) continue;
        record_seen(d);
        merged.push_back(std::move(d));
    }

    append_vid_pid_checked(headsetcontrol_devices);
    append_vid_pid_checked(native_hid);
    append_vid_pid_checked(native_xinput);

    std::stable_sort(merged.begin(), merged.end(),
                     [](const bridge::DeviceSnapshot& a, const bridge::DeviceSnapshot& b) {
                         return BucketOf(a) < BucketOf(b);
                     });
    return merged;
}

}  // namespace peripherals
