#include "protocol.h"

#include <cstdio>
#include <cstring>

namespace incott {
namespace {

Frame Build(std::uint8_t cmd, const std::uint8_t* payload, std::size_t count) {
    Frame f{};
    f.bytes[0] = kReportId;
    f.bytes[1] = cmd;
    if (payload != nullptr) {
        const std::size_t room = kReportLen - 2;
        if (count > room) count = room;
        std::memcpy(f.bytes + 2, payload, count);
    }
    return f;
}

}  // namespace

Frame MakeWrite(std::uint8_t cmd, const std::uint8_t* payload, std::size_t count) {
    return Build(static_cast<std::uint8_t>(cmd & ~kReadFlag), payload, count);
}

Frame MakeRead(std::uint8_t cmd, const std::uint8_t* payload, std::size_t count) {
    return Build(static_cast<std::uint8_t>(cmd | kReadFlag), payload, count);
}

bool CheckEcho(const Frame& frame, std::uint8_t cmd) {
    return frame.bytes[0] == kReportId && frame.bytes[1] == static_cast<std::uint8_t>(cmd | kReadFlag);
}

int BatteryPercent(const Frame& status) {
    if (!CheckEcho(status, kCmdStatus)) return -1;
    const int pct = status.bytes[2];
    return pct <= 100 ? pct : -1;
}

DeviceInfo ParseModel(const Frame& frame, bool wireless) {
    DeviceInfo info;
    if (!CheckEcho(frame, kCmdModel)) return info;
    info.psd = frame.bytes[2];
    info.model = frame.bytes[3];
    info.receiver = frame.bytes[4];
    // Payload [4] is the wired sensor byte, [5] the wireless one.
    const int sensor_code = wireless ? frame.bytes[6] : frame.bytes[5];
    if (sensor_code == 0xF0) {
        info.sensor = 3395;
    } else if (sensor_code == 0xF1) {
        info.sensor = 3950;
    }
    return info;
}

const wchar_t* ModelName(int model) {
    switch (model) {
        case 2: return L"G23";
        case 3: return L"G24";
        default: return L"unknown";
    }
}

const wchar_t* ReceiverName(int receiver) {
    switch (receiver) {
        case 2: return L"8K";
        case 1: return L"1K";
        default: return L"unknown";
    }
}

const wchar_t* SensorName(int sensor) {
    switch (sensor) {
        case 3395: return L"PAW3395";
        case 3950: return L"PAW3950";
        default: return L"unknown";
    }
}

bool DpiValid(int dpi) {
    return dpi >= kDpiMin && dpi <= kDpiMax && dpi % kDpiStep == 0;
}

int DpiToHw(int dpi) { return dpi / kDpiStep - 1; }
int DpiFromHw(int hw) { return (hw + 1) * kDpiStep; }

namespace {

// [09, 02, slot, hwLo, hwHi, 0, 0, 0, axis]
Frame DpiFrame(int slot, int hw, int axis) {
    Frame f{};
    f.bytes[0] = kReportId;
    f.bytes[1] = kCmdDpi;
    f.bytes[2] = static_cast<std::uint8_t>(slot);
    f.bytes[3] = static_cast<std::uint8_t>(hw & 0xFF);
    f.bytes[4] = static_cast<std::uint8_t>((hw >> 8) & 0xFF);
    f.bytes[8] = static_cast<std::uint8_t>(axis);
    return f;
}

}  // namespace

DpiWrite MakeDpiWrite(int slot, int hw_x, int hw_y) {
    DpiWrite write;
    if (hw_x == hw_y) {
        write.frames[0] = DpiFrame(slot, hw_x, 0);
        write.count = 1;
    } else {
        write.frames[0] = DpiFrame(slot, hw_x, 1);
        write.frames[1] = DpiFrame(slot, hw_y, 2);
        write.count = 2;
    }
    return write;
}

int DpiFromReply(const Frame& reply, int expect_slot) {
    if (!CheckEcho(reply, kCmdDpi)) return -1;
    // WP3 hardening (same stale-frame guard as SleepSeconds/SensorSubValue):
    // bytes[2] must echo the slot we asked for, or the frame belongs elsewhere.
    if (reply.bytes[2] != expect_slot) return -1;
    const int hw = (static_cast<int>(reply.bytes[4]) << 8) | reply.bytes[3];
    if (hw < 0 || hw > kDpiMax / kDpiStep - 1) return -1;
    return DpiFromHw(hw);
}

int DpiExpectedReadback(const DpiWrite& write, int dpi_x, int dpi_y) {
    // Live-measured (docs/05 §11): [3..4] of every 0x82 reply holds the value of
    // the last written frame — X=800→Y=1600 reads 1600, X=1600→Y=800 reads 800,
    // Y=800→X=1600 (axis=1 last) reads 1600. MakeDpiWrite's two-frame order ends
    // with Y, so the Y value is the only axis the readback can verify.
    return write.count == 2 ? dpi_y : dpi_x;
}

bool DpiStageValid(int count, int active) {
    return count >= 1 && count <= kDpiCountMax && active >= 0 && active < count;
}

DpiTable ParseDpiTable(const Frame& reply) {
    DpiTable table;
    if (!CheckEcho(reply, kCmdDpiTable)) return table;
    // Live G23 2.4G reply: 09 83 06 02 ... -> 6 levels, active level 2 (0-based).
    // Same layout setDpiInfo writes: [03, count, active, 0, 0, 0, 0, 0].
    table.count = reply.bytes[2];
    table.active = reply.bytes[3];
    return table;
}

StatusInfo ParseStatus(const Frame& reply) {
    StatusInfo info;
    if (!CheckEcho(reply, kCmdStatus)) return info;
    if (reply.bytes[2] <= 100) info.battery = reply.bytes[2];
    info.charging = reply.bytes[3];
    info.rate_code = reply.bytes[4];
    info.dpi_index = reply.bytes[5];
    return info;
}

int RateFromCode(int code) {
    static const int kRates[] = {1000, 500, 250, 125, 8000, 4000, 2000};
    if (code < 0 || code >= static_cast<int>(sizeof(kRates) / sizeof(kRates[0]))) return -1;
    return kRates[code];
}

bool HighRateAllowed(int receiver_type, bool wireless) {
    return receiver_type == 2 && wireless;
}

int SleepSeconds(const Frame& reply, int expect_sub) {
    if (!CheckEcho(reply, kCmdMisc)) return -1;
    if (reply.bytes[2] != static_cast<std::uint8_t>(expect_sub)) return -1;
    const int value = (static_cast<int>(reply.bytes[4]) << 8) | reply.bytes[3];
    if (value < kSleepSecondsMin || value > kSleepSecondsMax) return -1;
    return value;
}

int DebounceValue(const Frame& reply, int expect_sub) {
    if (!CheckEcho(reply, kCmdMisc)) return -1;
    if (reply.bytes[2] != static_cast<std::uint8_t>(expect_sub)) return -1;
    const int value = reply.bytes[3];
    if (value > kDebounceMsMax) return -1;
    return value;
}

int SensorSubValue(const Frame& reply, int expect_sub) {
    if (!CheckEcho(reply, kCmdSensor)) return -1;
    if (reply.bytes[2] != static_cast<std::uint8_t>(expect_sub)) return -1;
    const int value = reply.bytes[3];
    if (!SensorValueValid(expect_sub, value)) return -1;
    return value;
}

bool SensorValueValid(int sub, int value) {
    switch (sub) {
        case 1:  // LOD
        case 5:  // sensor mode (official data model: 0 power save / 1 standard / 2 game)
            return value >= 0 && value <= 2;
        case 2:  // ripple
        case 3:  // fixline
        case 4:  // motion sync
            return value == 0 || value == 1;
        default:
            return false;
    }
}

// 0x85 sub2 (fire key): reply is [09, 85, 02, loops, interval, ...]. The caller
// checks bytes[2] == 0x02 itself (the same frame carries both fields).
int FireLoops(const Frame& reply) {
    if (!CheckEcho(reply, kCmdMisc)) return -1;
    const int loops = reply.bytes[3];
    if (loops > 3) return -1;
    return loops;
}

int FireInterval(const Frame& reply) {
    if (!CheckEcho(reply, kCmdMisc)) return -1;
    return reply.bytes[4];
}

Frame MakeMacroChunkHeader(int chunk, int slot) {
    Frame f{};
    f.bytes[0] = kReportId;
    f.bytes[1] = kCmdMacro;
    f.bytes[2] = kMacroChunks;
    f.bytes[3] = static_cast<std::uint8_t>(chunk);
    f.bytes[4] = static_cast<std::uint8_t>(kMacroChunkBytes);
    f.bytes[5] = static_cast<std::uint8_t>(slot);
    return f;
}

// ---- Macros --------------------------------------------------------------

const char* MacroErrorText(MacroError error) {
    switch (error) {
        case MacroError::kOk: return "ok";
        case MacroError::kBufferRange: return "buffer must be 0..9";
        case MacroError::kLoopType: return "loop type must be 1..3";
        case MacroError::kCyclesRange: return "cycles must be 1..65535";
        case MacroError::kActionCount: return "macro must have 1..71 actions";
        case MacroError::kActionUsage: return "key usage must be 0..255";
        case MacroError::kActionDelay: return "delay must be 0..65535";
    }
    return "unknown macro error";
}

MacroError ValidateMacro(int buffer, int loopType, int cycles,
                         const std::vector<MacroAction>& actions) {
    if (buffer < 0 || buffer > 9) return MacroError::kBufferRange;
    if (loopType < 1 || loopType > 3) return MacroError::kLoopType;
    if (cycles < 1 || cycles > kMacroCycleLimit) return MacroError::kCyclesRange;
    if (actions.empty() || static_cast<int>(actions.size()) > kMacroActionLimit) {
        return MacroError::kActionCount;
    }
    for (const MacroAction& action : actions) {
        if (action.usage < 0 || action.usage > 0xFF) return MacroError::kActionUsage;
        if (action.delayMs < 0 || action.delayMs > 0xFFFF) return MacroError::kActionDelay;
    }
    return MacroError::kOk;
}

MacroError EncodeMacro(int buffer, int loopType, int cycles, std::uint64_t uid,
                       const std::vector<MacroAction>& actions,
                       std::uint8_t out[kMacroPayload]) {
    const MacroError error = ValidateMacro(buffer, loopType, cycles, actions);
    if (error != MacroError::kOk) return error;
    if (out == nullptr) return MacroError::kActionCount;

    for (std::size_t i = 0; i < kMacroPayload; ++i) out[i] = 0;

    // buffer id doubles as the firmware slot; the firmware only plays the buffer
    // that matches the key index, hence the "macro slot == key index" rule.
    out[0] = static_cast<std::uint8_t>(buffer);
    out[1] = static_cast<std::uint8_t>(loopType == 3 ? 2 : (loopType == 2 ? 1 : 0));
    out[2] = static_cast<std::uint8_t>(cycles & 0xFF);
    out[3] = static_cast<std::uint8_t>((cycles >> 8) & 0xFF);

    std::size_t offset = 4;
    for (std::size_t i = 0; i < actions.size(); ++i) {
        std::uint8_t flags = actions[i].press ? 0x00 : 0x80;
        if (i + 1 != actions.size()) flags |= 0x01;  // bit0 = another item follows
        out[offset] = flags;
        out[offset + 1] = static_cast<std::uint8_t>(actions[i].usage);
        out[offset + 2] = static_cast<std::uint8_t>(actions[i].delayMs & 0xFF);
        out[offset + 3] = static_cast<std::uint8_t>((actions[i].delayMs >> 8) & 0xFF);
        offset += 4;
    }

    // Trailer, byte-for-byte from the vendor macro_to_hw().
    const int total = static_cast<int>((actions.size() + 1) * 4 + 0x80);
    out[0x130] = static_cast<std::uint8_t>(total & 0xFF);
    out[0x131] = static_cast<std::uint8_t>((total >> 8) & 0xFF);
    out[0x132] = static_cast<std::uint8_t>((total >> 16) & 0xFF);
    out[0x133] = static_cast<std::uint8_t>((total >> 24) & 0xFF);
    out[0x134] = 0x10;
    out[0x135] = 0x00;
    out[0x136] = 0xE8;
    out[0x137] = 0xE8;
    out[0x138] = static_cast<std::uint8_t>(uid & 0xFF);
    out[0x139] = static_cast<std::uint8_t>((uid >> 8) & 0xFF);
    out[0x13A] = static_cast<std::uint8_t>((uid >> 16) & 0xFF);
    out[0x13B] = static_cast<std::uint8_t>((uid >> 24) & 0xFF);
    const int item_bytes = static_cast<int>(actions.size() * 2);
    out[0x13C] = static_cast<std::uint8_t>(item_bytes & 0xFF);
    out[0x13D] = static_cast<std::uint8_t>((item_bytes >> 8) & 0xFF);
    out[0x13E] = static_cast<std::uint8_t>(actions.size());
    out[0x13F] = 0x00;
    return MacroError::kOk;
}

// ---- Key bindings ---------------------------------------------------------

namespace {

struct KeyNameEntry {
    const char* code;
    const char* label;
    std::uint8_t usage;
};

#include "keytable.inc"

struct MouseEntry {
    const char* name;    // user-facing suffix after "Mouse:"
    int action;          // 1 left, 2 right, 3 middle, 4 forward, 5 back
    std::uint32_t code;
};

const MouseEntry kMouseNames[] = {
    {"Left", 1, 0x00F00001},
    {"Right", 2, 0x00F10001},
    {"Middle", 3, 0x00F20001},
    {"Forward", 4, 0x00F40001},
    {"Back", 5, 0x00F30001},
};

struct MediaEntry {
    const char* name;
    int usage;
};

const MediaEntry kMediaNames[] = {
    {"Player", 0x183},     {"PlayPause", 0x0CD}, {"Stop", 0x0B7},     {"Prev", 0x0B6},
    {"Next", 0x0B5},       {"VolUp", 0x0E9},     {"VolDown", 0x0EA},  {"Mute", 0x0E2},
    {"Email", 0x18A},      {"Calculator", 0x192}, {"Explorer", 0x194}, {"Home", 0x223},
    {"Refresh", 0x227},    {"Forward", 0x225},   {"Back", 0x224},     {"Search", 0x221},
};

bool IEquals(const std::string& a, const char* b) {
    if (b == nullptr) return false;
    std::size_t i = 0;
    for (; i < a.size() && b[i] != '\0'; ++i) {
        char x = a[i];
        char y = b[i];
        if (x >= 'a' && x <= 'z') x = static_cast<char>(x - 'a' + 'A');
        if (y >= 'a' && y <= 'z') y = static_cast<char>(y - 'a' + 'A');
        if (x != y) return false;
    }
    return i == a.size() && b[i] == '\0';
}

const MouseEntry* FindMouseByAction(int action) {
    for (const MouseEntry& entry : kMouseNames) {
        if (entry.action == action) return &entry;
    }
    return nullptr;
}

const MouseEntry* FindMouseByName(const std::string& name) {
    for (const MouseEntry& entry : kMouseNames) {
        if (IEquals(name, entry.name)) return &entry;
    }
    return nullptr;
}

const MediaEntry* FindMediaByUsage(int usage) {
    for (const MediaEntry& entry : kMediaNames) {
        if (entry.usage == usage) return &entry;
    }
    return nullptr;
}

std::uint32_t EncodeKeyboard(int usage, int modifiers) {
    if (modifiers != 0) {
        return (static_cast<std::uint32_t>(usage) << 16)
               | (static_cast<std::uint32_t>(modifiers & 0x0F) << 8);
    }
    return (static_cast<std::uint32_t>(usage) << 8) | 0x80;
}

std::string HexUnknown(std::uint32_t code) {
    char buffer[16];
    std::snprintf(buffer, sizeof(buffer), "unknown:0x%08X", static_cast<unsigned>(code));
    return buffer;
}

}  // namespace

const int kKeyMatrix[kKeyCount] = {0, 1, 2, 4, 3, 5};

const char* KeySlotName(int index) {
    static const char* const kNames[kKeyCount] = {"left", "right", "middle", "forward", "back",
                                                  "dpi"};
    if (index < 0 || index >= kKeyCount) return nullptr;
    return kNames[index];
}

Frame MakeKeyRead(int matrix) {
    const std::uint8_t payload[1] = {static_cast<std::uint8_t>(matrix)};
    return MakeRead(kCmdKeys, payload, 1);
}

Frame MakeKeyWrite(int matrix, std::uint32_t code) {
    const std::uint8_t payload[5] = {
        static_cast<std::uint8_t>(matrix),
        static_cast<std::uint8_t>(code & 0xFF),
        static_cast<std::uint8_t>((code >> 8) & 0xFF),
        static_cast<std::uint8_t>((code >> 16) & 0xFF),
        static_cast<std::uint8_t>((code >> 24) & 0xFF),
    };
    return MakeWrite(kCmdKeys, payload, sizeof(payload));
}

bool ParseKeyReply(const Frame& reply, int expect_matrix, std::uint32_t* code) {
    if (!CheckEcho(reply, kCmdKeys)) return false;
    if (reply.bytes[2] != static_cast<std::uint8_t>(expect_matrix)) return false;
    if (code != nullptr) {
        *code = static_cast<std::uint32_t>(reply.bytes[3])
                | (static_cast<std::uint32_t>(reply.bytes[4]) << 8)
                | (static_cast<std::uint32_t>(reply.bytes[5]) << 16)
                | (static_cast<std::uint32_t>(reply.bytes[6]) << 24);
    }
    return true;
}

int KeyNameCount() { return kKeyNameCount; }

bool KeyUsageFromCode(const char* code, int* usage) {
    if (code == nullptr) return false;
    for (int i = 0; i < kKeyNameCount; ++i) {
        if (std::strcmp(code, kKeyNames[i].code) == 0) {
            if (usage != nullptr) *usage = kKeyNames[i].usage;
            return true;
        }
    }
    return false;
}

const char* KeyLabelFromCode(const char* code) {
    if (code == nullptr) return nullptr;
    for (int i = 0; i < kKeyNameCount; ++i) {
        if (std::strcmp(code, kKeyNames[i].code) == 0) return kKeyNames[i].label;
    }
    return nullptr;
}

const char* KeyCodeFromUsage(int usage) {
    for (int i = 0; i < kKeyNameCount; ++i) {
        if (kKeyNames[i].usage == usage) return kKeyNames[i].code;
    }
    return nullptr;
}

Binding DecodeBinding(std::uint32_t code) {
    Binding binding;
    binding.code = code;
    if (code == 0) {
        binding.kind = KeyKind::kOff;
        return binding;
    }
    const int low = static_cast<int>(code & 0xFF);
    switch (low) {
        case 0x01: {  // mouse button: [action][0][0xF0..0xF4][0]
            const int wire = static_cast<int>((code >> 16) & 0xFF);
            const MouseEntry* entry = nullptr;
            switch (wire) {
                case 0xF0: entry = FindMouseByAction(1); break;
                case 0xF1: entry = FindMouseByAction(2); break;
                case 0xF2: entry = FindMouseByAction(3); break;
                case 0xF3: entry = FindMouseByAction(5); break;
                case 0xF4: entry = FindMouseByAction(4); break;
                default: break;
            }
            if (entry != nullptr) {
                binding.kind = KeyKind::kMouse;
                binding.mouse = entry->action;
            }
            return binding;
        }
        case 0x03: {  // media: [usage][usage][][0]
            const int media = static_cast<int>(code >> 16);
            if (FindMediaByUsage(media) != nullptr) {
                binding.kind = KeyKind::kMedia;
                binding.media = media;
            }
            return binding;
        }
        case 0x07: {  // dpi: [action][0][][0]
            const int action = static_cast<int>(code >> 16);
            if (action >= 1 && action <= 3) {
                binding.kind = KeyKind::kDpi;
                binding.dpi = action;
            }
            return binding;
        }
        case 0x09:  // macro slot
            binding.kind = KeyKind::kMacro;
            binding.macro_slot = static_cast<int>(code >> 16);
            return binding;
        case 0x0A: {  // profile / fire
            const int sub = static_cast<int>((code >> 8) & 0xFF);
            if (sub == 0xF1 && (code >> 16) == 0) {
                binding.kind = KeyKind::kProfile;
            } else if (sub == 0xF0) {
                binding.kind = KeyKind::kFire;
            }
            return binding;
        }
        case 0x80:  // plain keyboard: [0][usage][0x80][0]
            binding.kind = KeyKind::kKeyboard;
            binding.usage = static_cast<int>((code >> 8) & 0xFF);
            return binding;
        case 0x00: {  // keyboard with modifiers: [0][mod][usage][0]
            const int usage = static_cast<int>((code >> 16) & 0xFF);
            const int modifiers = static_cast<int>((code >> 8) & 0x0F);
            if (usage != 0) {
                binding.kind = KeyKind::kKeyboard;
                binding.usage = usage;
                binding.modifiers = modifiers;
            }
            return binding;
        }
        default:
            return binding;
    }
}

bool EncodeBinding(const char* name, std::uint32_t* code) {
    if (name == nullptr || code == nullptr) return false;
    const std::string text(name);
    if (text.empty()) return false;

    if (IEquals(text, "Off") || IEquals(text, "None") || IEquals(text, "Disabled")) {
        *code = 0;
        return true;
    }
    if (IEquals(text, "Profile")) {
        *code = 0x0000F10A;
        return true;
    }
    if (IEquals(text, "Fire")) {
        *code = 0x0218F00A;
        return true;
    }

    const std::size_t colon = text.find(':');
    if (colon != std::string::npos) {
        const std::string head = text.substr(0, colon);
        const std::string tail = text.substr(colon + 1);
        if (IEquals(head, "Mouse")) {
            const MouseEntry* entry = FindMouseByName(tail);
            if (entry == nullptr) return false;
            *code = entry->code;
            return true;
        }
        if (IEquals(head, "Dpi")) {
            if (IEquals(tail, "Up")) {
                *code = 0x00010007;
            } else if (IEquals(tail, "Down")) {
                *code = 0x00020007;
            } else if (IEquals(tail, "Cycle")) {
                *code = 0x00030007;
            } else {
                return false;
            }
            return true;
        }
        if (IEquals(head, "Media")) {
            for (const MediaEntry& entry : kMediaNames) {
                if (IEquals(tail, entry.name)) {
                    *code = (static_cast<std::uint32_t>(entry.usage) << 16) | 0x03;
                    return true;
                }
            }
            return false;
        }
    }

    // "Ctrl+Shift+KeyS": every segment but the last is a modifier.
    int modifiers = 0;
    std::string key = text;
    std::size_t plus = key.find('+');
    while (plus != std::string::npos) {
        const std::string part = key.substr(0, plus);
        if (IEquals(part, "Ctrl") || IEquals(part, "Control")) {
            modifiers |= 1;
        } else if (IEquals(part, "Shift")) {
            modifiers |= 2;
        } else if (IEquals(part, "Alt")) {
            modifiers |= 4;
        } else if (IEquals(part, "Win") || IEquals(part, "Meta")) {
            modifiers |= 8;
        } else {
            return false;
        }
        key = key.substr(plus + 1);
        plus = key.find('+');
    }

    int usage = 0;
    if (!KeyUsageFromCode(key.c_str(), &usage)) return false;
    if (usage >= 0xF0) {
        // Mouse usages 0xF0..0xF2 in vkPOS, encoded as the mouse-button form.
        const char* mouse_name = usage == 0xF0 ? "Left" : usage == 0xF1 ? "Right" : "Middle";
        const MouseEntry* entry = FindMouseByName(mouse_name);
        if (entry == nullptr || modifiers != 0) return false;
        *code = entry->code;
        return true;
    }
    *code = EncodeKeyboard(usage, modifiers);
    return true;
}

std::string DescribeBinding(std::uint32_t code) {    const Binding binding = DecodeBinding(code);
    switch (binding.kind) {
        case KeyKind::kOff:
            return "Off";
        case KeyKind::kKeyboard: {
            std::string out;
            if ((binding.modifiers & 1) != 0) out += "Ctrl+";
            if ((binding.modifiers & 2) != 0) out += "Shift+";
            if ((binding.modifiers & 4) != 0) out += "Alt+";
            if ((binding.modifiers & 8) != 0) out += "Win+";
            const char* code_name = KeyCodeFromUsage(binding.usage);
            if (code_name != nullptr) {
                out += code_name;
            } else {
                char buffer[16];
                std::snprintf(buffer, sizeof(buffer), "usage:0x%02X", binding.usage);
                out += buffer;
            }
            return out;
        }
        case KeyKind::kMouse: {
            const MouseEntry* entry = FindMouseByAction(binding.mouse);
            return std::string("Mouse:") + (entry != nullptr ? entry->name : "?");
        }
        case KeyKind::kMedia: {
            const MediaEntry* entry = FindMediaByUsage(binding.media);
            return std::string("Media:") + (entry != nullptr ? entry->name : "?");
        }
        case KeyKind::kMacro:
            return "Macro:" + std::to_string(binding.macro_slot);
        case KeyKind::kDpi:
            if (binding.dpi == 1) return "Dpi:Up";
            if (binding.dpi == 2) return "Dpi:Down";
            return "Dpi:Cycle";
        case KeyKind::kProfile:
            return "Profile";
        case KeyKind::kFire:
            return "Fire";
        case KeyKind::kUnknown:
            break;
    }
    return HexUnknown(code);
}

const char* KeyKindName(KeyKind kind) {
    switch (kind) {
        case KeyKind::kOff: return "off";
        case KeyKind::kKeyboard: return "keyboard";
        case KeyKind::kMouse: return "mouse";
        case KeyKind::kMedia: return "media";
        case KeyKind::kMacro: return "macro";
        case KeyKind::kDpi: return "dpi";
        case KeyKind::kProfile: return "profile";
        case KeyKind::kFire: return "fire";
        case KeyKind::kUnknown: break;
    }
    return "unknown";
}

bool ReceiverLedValid(int mode) { return mode >= 0 && mode <= 2; }

int ParseReceiverLed(const Frame& reply) {
    if (!CheckEcho(reply, kCmdReceiverLed)) return -1;
    const int mode = reply.bytes[2];
    return ReceiverLedValid(mode) ? mode : -1;
}

}  // namespace incott
