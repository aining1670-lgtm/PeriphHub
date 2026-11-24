#include "yjx_protocol.h"

#include <cstdio>
#include <cstring>

namespace yjx {
namespace {

struct KeyNameEntry {
    const char* code;
    const char* label;
    std::uint8_t usage;
};

#include "keytable.inc"

struct MouseEntry {
    const char* name;
    std::uint8_t code1;
};

const MouseEntry kMouseNames[] = {
    {"Left", 1},
    {"Right", 2},
    {"Middle", 4},
    {"Back", 8},
    {"Forward", 16},
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

const MouseEntry* FindMouseByCode1(std::uint8_t code1) {
    for (const MouseEntry& e : kMouseNames) {
        if (e.code1 == code1) return &e;
    }
    return nullptr;
}

const MouseEntry* FindMouseByName(const std::string& name) {
    for (const MouseEntry& e : kMouseNames) {
        if (IEquals(name, e.name)) return &e;
    }
    return nullptr;
}

const MediaEntry* FindMediaByUsage(int usage) {
    for (const MediaEntry& e : kMediaNames) {
        if (e.usage == usage) return &e;
    }
    return nullptr;
}

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

const char* KeyCodeFromUsage(int usage) {
    for (int i = 0; i < kKeyNameCount; ++i) {
        if (kKeyNames[i].usage == usage) return kKeyNames[i].code;
    }
    return nullptr;
}

std::string HexUnknown(const KeyCode& kc) {
    char buf[20];
    std::snprintf(buf, sizeof(buf), "unknown:0x%08X", static_cast<unsigned>(PackKeyCode(kc)));
    return buf;
}

std::uint16_t ReadLe16(const std::uint8_t* p) {
    return static_cast<std::uint16_t>(p[0]) | (static_cast<std::uint16_t>(p[1]) << 8);
}

void WriteLe16(std::uint8_t* p, std::uint16_t value) {
    p[0] = static_cast<std::uint8_t>(value & 0xFFu);
    p[1] = static_cast<std::uint8_t>((value >> 8) & 0xFFu);
}

void SetBit(std::uint8_t* flags, std::uint8_t mask, bool on) {
    if (on) {
        *flags = static_cast<std::uint8_t>(*flags | mask);
    } else {
        *flags = static_cast<std::uint8_t>(*flags & static_cast<std::uint8_t>(~mask));
    }
}

}  // namespace

bool IsYjxDevice(std::uint16_t vid, std::uint16_t pid) {
    return (vid == kVidUsb || vid == kVidDongle) && pid == kPidX11;
}

bool IsDongleVid(std::uint16_t vid) {
    return vid == kVidDongle;
}

bool DpiValid(int dpi) {
    return dpi >= kDpiMin && dpi <= kDpiMax && (dpi % kDpiStep) == 0;
}

bool DpiStageValid(int count, int active) {
    return count >= 1 && count <= kDpiStageMax && active >= 0 && active < count;
}

int RateToWire(int hz) {
    switch (hz) {
        case 125: return 1;
        case 250: return 2;
        case 500: return 3;
        case 1000: return 4;
        default: return -1;
    }
}

int RateFromWire(std::uint8_t wire) {
    switch (wire) {
        case 1: return 125;
        case 2: return 250;
        case 3: return 500;
        case 4: return 1000;
        default: return -1;
    }
}

int LodBridgeToWire(int bridge_lod) {
    switch (bridge_lod) {
        case 0: return 1;  // 1.0 mm
        case 1: return 2;  // 2.0 mm
        default: return -1;
    }
}

int LodWireToBridge(int wire_lod) {
    switch (wire_lod) {
        case 1: return 0;  // 1.0 mm
        case 2: return 1;  // 2.0 mm
        default: return -1;
    }
}

int SleepSecToMin(int seconds) {
    switch (seconds) {
        case 60: return 1;
        case 180: return 3;
        case 300: return 5;
        case 600: return 10;
        case 1200: return 20;
        case 1800: return 30;
        case 3600: return 60;
        default: return -1;
    }
}

int SleepMinToSec(int minutes) {
    switch (minutes) {
        case 1:
        case 3:
        case 5:
        case 10:
        case 20:
        case 30:
        case 60:
            return minutes * 60;
        default:
            return -1;
    }
}

bool LightModeValid(int mode) {
    return mode >= 0 && mode <= kLightModeMax;
}

bool DebounceValid(int ms) {
    return ms >= kDebounceMin && ms <= kDebounceMax;
}

void BuildReadConfigPacket(std::uint8_t out[kPacketLen]) {
    std::memset(out, 0, kPacketLen);
    out[0] = 0x00;
    out[1] = 0x55;
    out[2] = 0x0E;
    out[3] = 0xA5;
    out[4] = 0x0B;
    out[5] = 0x2F;
    out[6] = 0x01;
    out[7] = 0x01;
    out[8] = 0x00;
}

bool ParseConfigReply(const std::uint8_t reply[kPacketLen],
                      Config* out,
                      RawConfigFrame* raw_out) {
    if (reply == nullptr) return false;
    const std::uint8_t* payload = reply + 1;
    if (payload[0] != 0xAAu || payload[1] != 0x0Eu) return false;

    // Matches the web driver guard against sleeping/uninitialized replies.
    if ((payload[13] == 0x00u && payload[14] == 0x00u && payload[15] == 0x00u) ||
        (payload[13] == 0xFFu && payload[14] == 0xFFu && payload[15] == 0xFFu)) {
        return false;
    }

    const int dpi_count = static_cast<int>(payload[11]);
    if (dpi_count < 1 || dpi_count > kDpiStageMax) return false;

    Config cfg;
    cfg.light_mode = static_cast<int>(payload[9]);
    cfg.rate_hz = RateFromWire(payload[10]);
    cfg.dpi_count = dpi_count;
    cfg.dpi_active = (payload[12] >= 1u) ? (static_cast<int>(payload[12]) - 1) : -1;
    for (int i = 0; i < kDpiStageMax; ++i) {
        cfg.dpi[i] = static_cast<int>(ReadLe16(payload + 13 + (i * 2)));
    }
    cfg.scroll_reverse = (payload[48] != 0u);
    cfg.lod_wire = static_cast<int>(payload[49]);
    cfg.sensor_flag = payload[50];
    cfg.ripple = (cfg.sensor_flag & kSensorBitRipple) != 0u;
    cfg.fixline = (cfg.sensor_flag & kSensorBitFixline) != 0u;
    cfg.motion = (cfg.sensor_flag & kSensorBitMotion) != 0u;
    cfg.debounce_ms = static_cast<int>(payload[51]);
    cfg.sleep_min = static_cast<int>(payload[52]);

    if (raw_out != nullptr) {
        std::memcpy(raw_out->payload, payload, kPayloadLen);
    }
    if (out != nullptr) {
        *out = cfg;
    }
    return true;
}

void BuildWriteConfigPacket(const RawConfigFrame& base_raw,
                            const ConfigPatch& patch,
                            std::uint8_t out[kPacketLen]) {
    std::memset(out, 0, kPacketLen);

    // Copy payload[8..63] into out[9..64] first so every reserved/unknown byte
    // (including payload[25..47], payload[53..63], and sensor_flag unknown bits)
    // is preserved verbatim.
    std::memcpy(out + 9, base_raw.payload + 8, kPayloadLen - 8);

    out[0] = 0x00;
    out[1] = 0x55;
    out[2] = 0x0F;
    out[3] = 0xAE;
    out[4] = 0x0A;
    out[5] = 0x2F;
    out[6] = 0x01;
    out[7] = 0x01;
    out[8] = 0x00;

    if (patch.have_light_mode) {
        out[10] = static_cast<std::uint8_t>(patch.light_mode);
    }
    if (patch.have_rate_hz) {
        const int wire = RateToWire(patch.rate_hz);
        if (wire > 0) out[11] = static_cast<std::uint8_t>(wire);
    }
    if (patch.have_dpi_count) {
        out[12] = static_cast<std::uint8_t>(patch.dpi_count);
    }
    if (patch.have_dpi_active) {
        out[13] = static_cast<std::uint8_t>(patch.dpi_active + 1);
    }
    for (int i = 0; i < kDpiStageMax; ++i) {
        if (patch.have_dpi[i]) {
            WriteLe16(out + 14 + (i * 2), static_cast<std::uint16_t>(patch.dpi[i]));
        }
    }
    if (patch.have_scroll_reverse) {
        out[49] = patch.scroll_reverse ? 1u : 0u;
    }
    if (patch.have_lod_wire) {
        out[50] = static_cast<std::uint8_t>(patch.lod_wire);
    }
    if (patch.have_ripple) {
        SetBit(&out[51], kSensorBitRipple, patch.ripple);
    }
    if (patch.have_fixline) {
        SetBit(&out[51], kSensorBitFixline, patch.fixline);
    }
    if (patch.have_motion) {
        SetBit(&out[51], kSensorBitMotion, patch.motion);
    }
    if (patch.have_debounce_ms) {
        out[52] = static_cast<std::uint8_t>(patch.debounce_ms);
    }
    if (patch.have_sleep_min) {
        out[53] = static_cast<std::uint8_t>(patch.sleep_min);
    }
}

void BuildLightModePacket(std::uint8_t mode, std::uint8_t out[kPacketLen]) {
    std::memset(out, 0, kPacketLen);
    out[0] = 0x00;
    out[1] = 0x55;
    out[2] = 0x21;
    out[3] = 0x00;
    out[4] = 0x00;
    out[5] = 0x03;
    out[6] = 0x00;
    out[7] = 0x00;
    out[8] = 0x00;
    out[9] = 0x00;
    out[10] = 0x00;
    out[11] = mode;
}

bool ConfigMatchesPatch(const Config& actual, const ConfigPatch& patch) {
    if (patch.have_light_mode && actual.light_mode != patch.light_mode) return false;
    if (patch.have_rate_hz && actual.rate_hz != patch.rate_hz) return false;
    if (patch.have_dpi_count && actual.dpi_count != patch.dpi_count) return false;
    if (patch.have_dpi_active && actual.dpi_active != patch.dpi_active) return false;
    for (int i = 0; i < kDpiStageMax; ++i) {
        if (patch.have_dpi[i] && actual.dpi[i] != patch.dpi[i]) return false;
    }
    if (patch.have_scroll_reverse && actual.scroll_reverse != patch.scroll_reverse) return false;
    if (patch.have_lod_wire && actual.lod_wire != patch.lod_wire) return false;
    if (patch.have_ripple && actual.ripple != patch.ripple) return false;
    if (patch.have_fixline && actual.fixline != patch.fixline) return false;
    if (patch.have_motion && actual.motion != patch.motion) return false;
    if (patch.have_debounce_ms && actual.debounce_ms != patch.debounce_ms) return false;
    if (patch.have_sleep_min && actual.sleep_min != patch.sleep_min) return false;
    return true;
}

const char* KeySlotName(int index) {
    static const char* const kNames[kKeyCount] = {
        "left", "right", "middle", "forward", "back", "dpi",
    };
    if (index < 0 || index >= kKeyCount) return nullptr;
    return kNames[index];
}

std::uint32_t PackKeyCode(const KeyCode& kc) {
    return static_cast<std::uint32_t>(kc.type) |
           (static_cast<std::uint32_t>(kc.code1) << 8) |
           (static_cast<std::uint32_t>(kc.code2) << 16) |
           (static_cast<std::uint32_t>(kc.code3) << 24);
}

void BuildReadKeysPacket(std::uint8_t out[kPacketLen]) {
    std::memset(out, 0, kPacketLen);
    out[0] = 0x00;
    out[1] = 0x55;
    out[2] = 0x08;
    out[3] = 0xA5;
    out[4] = 0x0B;
    out[5] = 0x20;
}

bool ParseKeysReply(const std::uint8_t reply[kPacketLen],
                    KeyCode out_keys[kKeyCount],
                    RawKeysFrame* raw_out) {
    if (reply == nullptr || out_keys == nullptr) return false;
    const std::uint8_t* payload = reply + 1;
    if (payload[0] != 0xAAu || payload[1] != 0x08u) return false;

    for (int i = 0; i < kKeyCount; ++i) {
        const std::size_t off = 8u + static_cast<std::size_t>(kKeyWireSlot[i]) * 4u;
        out_keys[i] = KeyCode{payload[off], payload[off + 1], payload[off + 2], payload[off + 3]};
    }
    if (raw_out != nullptr) {
        std::memcpy(raw_out->payload, payload, kPayloadLen);
    }
    return true;
}

void BuildWriteKeysPacket(const RawKeysFrame& base,
                          int index,
                          const KeyCode& value,
                          std::uint8_t out[kPacketLen]) {
    std::memset(out, 0, kPacketLen);
    // Preserve payload[8..63] (all slots + scroll up/down + trailing bytes) into out[9..64].
    std::memcpy(out + 9, base.payload + 8, kPayloadLen - 8);

    out[0] = 0x00;
    out[1] = 0x55;
    out[2] = 0x09;
    out[3] = 0xA5;
    out[4] = 0x22;
    out[5] = 0x20;
    out[6] = 0x00;
    out[7] = 0x00;
    out[8] = 0x00;

    if (index >= 0 && index < kKeyCount) {
        const std::size_t off = 9u + static_cast<std::size_t>(kKeyWireSlot[index]) * 4u;
        out[off] = value.type;
        out[off + 1] = value.code1;
        out[off + 2] = value.code2;
        out[off + 3] = value.code3;
    }
}

bool EncodeBinding(const char* name, KeyCode* out) {
    if (name == nullptr || out == nullptr) return false;
    const std::string text(name);
    if (text.empty()) return false;

    if (IEquals(text, "Off") || IEquals(text, "None") || IEquals(text, "Disabled")) {
        *out = KeyCode{32, 0, 0, 0};
        return true;
    }

    const std::size_t colon = text.find(':');
    if (colon != std::string::npos) {
        const std::string head = text.substr(0, colon);
        const std::string tail = text.substr(colon + 1);
        if (IEquals(head, "Mouse")) {
            const MouseEntry* entry = FindMouseByName(tail);
            if (entry == nullptr) return false;
            *out = KeyCode{32, entry->code1, 0, 0};
            return true;
        }
        if (IEquals(head, "Dpi")) {
            if (!IEquals(tail, "Cycle")) return false;
            *out = KeyCode{33, 85, 0, 0};
            return true;
        }
        if (IEquals(head, "Media")) {
            for (const MediaEntry& entry : kMediaNames) {
                if (IEquals(tail, entry.name)) {
                    *out = KeyCode{48,
                                   static_cast<std::uint8_t>(entry.usage & 0xFF),
                                   static_cast<std::uint8_t>((entry.usage >> 8) & 0xFF),
                                   0};
                    return true;
                }
            }
            return false;
        }
        return false;
    }

    if (IEquals(text, "Fire") || IEquals(text, "Profile")) {
        return false;
    }

    std::uint8_t modifiers = 0;
    std::string key = text;
    std::size_t plus = key.find('+');
    while (plus != std::string::npos) {
        const std::string part = key.substr(0, plus);
        if (IEquals(part, "Ctrl") || IEquals(part, "Control")) {
            modifiers = static_cast<std::uint8_t>(modifiers | 0x01u);
        } else if (IEquals(part, "Shift")) {
            modifiers = static_cast<std::uint8_t>(modifiers | 0x02u);
        } else if (IEquals(part, "Alt")) {
            modifiers = static_cast<std::uint8_t>(modifiers | 0x04u);
        } else if (IEquals(part, "Win") || IEquals(part, "Meta")) {
            modifiers = static_cast<std::uint8_t>(modifiers | 0x08u);
        } else {
            return false;
        }
        key = key.substr(plus + 1);
        plus = key.find('+');
    }

    int usage = 0;
    if (!KeyUsageFromCode(key.c_str(), &usage)) return false;
    if (usage >= 0xF0) {
        if (modifiers != 0) return false;
        const char* mouse_name = usage == 0xF0 ? "Left" : usage == 0xF1 ? "Right" : "Middle";
        const MouseEntry* entry = FindMouseByName(mouse_name);
        if (entry == nullptr) return false;
        *out = KeyCode{32, entry->code1, 0, 0};
        return true;
    }

    *out = KeyCode{16, modifiers, static_cast<std::uint8_t>(usage & 0xFF), 0};
    return true;
}

void DecodeBinding(const KeyCode& kc, std::string* binding_out, std::string* kind_out) {
    auto set = [&](const std::string& binding, const char* kind) {
        if (binding_out != nullptr) *binding_out = binding;
        if (kind_out != nullptr) *kind_out = kind;
    };

    if (kc.type == 32u) {
        if (kc.code1 == 0u && kc.code2 == 0u && kc.code3 == 0u) {
            set("Off", "off");
            return;
        }
        if (kc.code2 == 0u && kc.code3 == 0u) {
            if (const MouseEntry* entry = FindMouseByCode1(kc.code1)) {
                set(std::string("Mouse:") + entry->name, "mouse");
                return;
            }
        }
        set(HexUnknown(kc), "unknown");
        return;
    }

    if ((kc.type == 33u && kc.code1 == 85u && kc.code2 == 0u && kc.code3 == 0u) ||
        (kc.type == 240u && kc.code1 == 1u && kc.code2 == 1u && kc.code3 == 0u)) {
        set("Dpi:Cycle", "dpi");
        return;
    }

    if (kc.type == 48u && kc.code3 == 0u) {
        const int usage = static_cast<int>(kc.code1) | (static_cast<int>(kc.code2) << 8);
        if (const MediaEntry* entry = FindMediaByUsage(usage)) {
            set(std::string("Media:") + entry->name, "media");
            return;
        }
        set(HexUnknown(kc), "unknown");
        return;
    }

    if (kc.type == 16u && kc.code3 == 0u) {
        std::string out;
        if ((kc.code1 & 0x11u) != 0u) out += "Ctrl+";
        if ((kc.code1 & 0x22u) != 0u) out += "Shift+";
        if ((kc.code1 & 0x44u) != 0u) out += "Alt+";
        if ((kc.code1 & 0x88u) != 0u) out += "Win+";
        const char* code_name = KeyCodeFromUsage(kc.code2);
        if (code_name != nullptr) {
            out += code_name;
        } else {
            char buf[16];
            std::snprintf(buf, sizeof(buf), "usage:0x%02X", kc.code2);
            out += buf;
        }
        set(out, "keyboard");
        return;
    }

    if (kc.type == 112u && kc.code1 < kMacroSlotCount) {
        set("Macro:" + std::to_string(kc.code1), "macro");
        return;
    }

    set(HexUnknown(kc), "unknown");
}

void DecodeBindingForSlot(int index,
                          const KeyCode& kc,
                          std::string* binding_out,
                          std::string* kind_out) {
    if ((kc.type == 0u || kc.type == 255u) && index >= 0 && index < kKeyCount) {
        static const char* const kDefaults[kKeyCount] = {
            "Mouse:Left", "Mouse:Right", "Mouse:Middle", "Mouse:Forward", "Mouse:Back", "Dpi:Cycle",
        };
        static const char* const kKinds[kKeyCount] = {
            "mouse", "mouse", "mouse", "mouse", "mouse", "dpi",
        };
        if (binding_out != nullptr) *binding_out = kDefaults[index];
        if (kind_out != nullptr) *kind_out = kKinds[index];
        return;
    }
    DecodeBinding(kc, binding_out, kind_out);
}

KeyCode MakeMacroKeyCode(int macro_slot, int loop_type, int cycles) {
    const std::uint8_t slot =
        static_cast<std::uint8_t>((macro_slot < 0) ? 0 : (macro_slot >= kMacroSlotCount ? kMacroSlotCount - 1 : macro_slot));
    if (loop_type == 1) {
        // Repeat while key held -> web driver radio value "2" ("循环直到按键松开")
        return KeyCode{112, slot, 1, 2};
    }
    if (loop_type == 2) {
        // Repeat until any key pressed -> web driver radio value "3" ("循环直到任意键按下")
        return KeyCode{112, slot, 1, 3};
    }
    int n = cycles;
    if (n < 1) n = 1;
    if (n > 255) n = 255;
    return KeyCode{112, slot, static_cast<std::uint8_t>(n), 0};
}

bool EncodeMacroTable(const MacroSlotData slots[kMacroSlotCount],
                      std::vector<std::uint8_t>* out,
                      std::string* error) {
    if (slots == nullptr || out == nullptr) return false;
    std::vector<std::uint8_t> buf(kMacroHeaderBytes, 0);

    for (int s = 0; s < kMacroSlotCount; ++s) {
        const auto& steps = slots[s].steps;
        if (steps.empty()) continue;
        const std::size_t slot_off = buf.size();
        const std::size_t need = slot_off + steps.size() * 4u;
        if (need > kMacroMaxBytes) {
            if (error != nullptr) {
                *error = "onboard macro table exceeds 4096 bytes (" + std::to_string(need) + " B)";
            }
            return false;
        }
        WriteLe16(buf.data() + (s * 2), static_cast<std::uint16_t>(slot_off));
        for (std::size_t i = 0; i < steps.size(); ++i) {
            const MacroStep& st = steps[i];
            std::uint8_t flags = st.is_mouse ? 0x03u : 0x02u;
            if (st.down) flags = static_cast<std::uint8_t>(flags | 0x40u);
            if (i + 1 == steps.size()) flags = static_cast<std::uint8_t>(flags | 0x80u);
            buf.push_back(static_cast<std::uint8_t>(st.delay_ms & 0xFFu));
            buf.push_back(static_cast<std::uint8_t>((st.delay_ms >> 8) & 0xFFu));
            buf.push_back(flags);
            buf.push_back(st.code);
        }
    }

    *out = std::move(buf);
    return true;
}

void BuildMacroChunkPacket(std::uint16_t offset,
                           const std::uint8_t* data,
                           std::uint8_t len,
                           std::uint8_t out[kPacketLen]) {
    std::memset(out, 0, kPacketLen);
    out[0] = 0x00;
    out[1] = 0x55;
    out[2] = 0x0D;
    out[3] = 0x00;
    out[4] = 0x00;
    const std::uint8_t clamped = (len > kMacroChunkPayload)
                                     ? static_cast<std::uint8_t>(kMacroChunkPayload)
                                     : len;
    out[5] = clamped;
    WriteLe16(out + 6, offset);
    out[8] = 0x00;
    if (data != nullptr && clamped > 0) {
        std::memcpy(out + 9, data, clamped);
    }
}

void BuildMacroCommitPacket(std::uint8_t out[kPacketLen]) {
    std::memset(out, 0, kPacketLen);
    out[0] = 0x00;
    out[1] = 0x55;
    out[2] = 0x10;
    out[3] = 0xA5;
    out[4] = 0x22;
    out[5] = 0x00;
    out[6] = 0x00;
    out[7] = 0x00;
    out[8] = 0x05;
}

}  // namespace yjx
