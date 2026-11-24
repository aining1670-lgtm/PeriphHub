#include "royuan_protocol.h"

#include <algorithm>
#include <cctype>
#include <cstring>

namespace royuan {
namespace {

std::string ToLowerAscii(const std::string& text) {
    std::string out = text;
    for (char& ch : out) {
        ch = static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));
    }
    return out;
}

bool IsGenericKeyboardName(const std::string& raw_name) {
    if (raw_name.empty()) return true;
    const std::string lower = ToLowerAscii(raw_name);
    return lower == "gaming keyboard" || lower == "royuan keyboard" ||
           lower == "usb keyboard" || lower == "2.4g wireless receiver" ||
           lower == "wireless receiver" || lower.find("receiver") != std::string::npos;
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

bool PresetColorRgb(std::uint8_t index, int* r, int* g, int* b) {
    switch (index) {
        case 0: *r = 255; *g = 0; *b = 0; return true;      // Red
        case 1: *r = 255; *g = 128; *b = 0; return true;    // Orange
        case 2: *r = 255; *g = 255; *b = 0; return true;    // Yellow
        case 3: *r = 0; *g = 255; *b = 0; return true;      // Green
        case 4: *r = 0; *g = 255; *b = 255; return true;    // Cyan
        case 5: *r = 0; *g = 0; *b = 255; return true;      // Blue
        case 6: *r = 255; *g = 0; *b = 255; return true;    // Magenta
        default: return false;
    }
}

void ApplyChecksumAt(std::uint8_t out[kPacketLen], std::size_t ck_index) {
    std::uint8_t* p = out + 1;
    p[ck_index] = 0;
    unsigned sum = 0;
    for (std::size_t i = 0; i < kPayloadLen; ++i) {
        sum += p[i];
    }
    p[ck_index] = static_cast<std::uint8_t>(0xFFu - (sum & 0xFFu));
}

}  // namespace

bool IsDonglePid(std::uint16_t pid) {
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

bool IsSupportedYc500Board(std::uint16_t dev_id) {
    return dev_id == kDevIdAkosBc98;
}

std::string ResolveModelName(std::uint16_t dev_id, const std::string& raw_usb_name) {
    if (dev_id == kDevIdAkosBc98 && IsGenericKeyboardName(raw_usb_name)) {
        return "AKOS BC98";
    }
    if (!raw_usb_name.empty()) return raw_usb_name;
    return "ROYUAN Keyboard";
}

void ApplyChecksum7(std::uint8_t out[kPacketLen]) {
    ApplyChecksumAt(out, 7);
}

void ApplyChecksum8(std::uint8_t out[kPacketLen]) {
    ApplyChecksumAt(out, 8);
}

void BuildGetDevIdPacket(std::uint8_t out[kPacketLen]) {
    std::memset(out, 0, kPacketLen);
    out[0] = 0x00;
    out[1] = 0x8F;
    ApplyChecksum7(out);
}

bool ParseDevIdReply(const std::uint8_t reply[kPacketLen], std::uint16_t* dev_id_out) {
    if (reply == nullptr) return false;
    const std::uint8_t* p = reply + 1;
    if (p[0] != 0x8Fu) return false;
    const std::uint16_t dev_id = ReadLe16(p + 1);
    if (dev_id == 0u || dev_id == 0xFFFFu) return false;
    if (dev_id_out != nullptr) *dev_id_out = dev_id;
    return true;
}

void BuildGetFirmwarePacket(std::uint8_t out[kPacketLen]) {
    std::memset(out, 0, kPacketLen);
    out[0] = 0x00;
    out[1] = 0x80;
    ApplyChecksum7(out);
}

bool ParseFirmwareReply(const std::uint8_t reply[kPacketLen], std::string* version_out) {
    if (reply == nullptr) return false;
    const std::uint8_t* p = reply + 1;
    if (p[0] != 0x80u || (p[1] == 0u && p[2] == 0u)) return false;
    if (version_out != nullptr) {
        *version_out = "v" + std::to_string(static_cast<unsigned>(p[2])) + "." +
                       std::to_string(static_cast<unsigned>(p[1]));
    }
    return true;
}

void BuildDongleStatusPacket(std::uint8_t out[kPacketLen]) {
    std::memset(out, 0, kPacketLen);
    out[0] = 0x00;
    out[1] = 0xF7;
}

bool ParseDongleStatus(const std::uint8_t reply[kPacketLen], DongleStatus* out) {
    if (reply == nullptr) return false;
    const std::uint8_t* p = reply + 1;
    const bool paired = (p[6] & 0x01u) != 0u;
    if (!paired) return false;

    const bool online = (p[3] == 0u);
    if (out != nullptr) {
        out->paired = paired;
        out->online = online;
        out->battery = (online && p[1] <= 100u) ? static_cast<int>(p[1]) : -1;
        out->can_accept_relay = (p[5] == 1u);
        out->reply_ready = (p[0] == 1u);
    }
    return true;
}

void BuildRelaySelectPacket(std::uint8_t target, std::uint8_t out[kPacketLen]) {
    std::memset(out, 0, kPacketLen);
    out[0] = 0x00;
    out[1] = 0xF6;
    out[2] = target;
}

void BuildRelayReleasePacket(std::uint8_t out[kPacketLen]) {
    std::memset(out, 0, kPacketLen);
    out[0] = 0x00;
    out[1] = 0xFC;
}

bool ProfileValid(int profile) {
    return profile >= kProfileMin && profile <= kProfileMax;
}

void BuildGetProfilePacket(std::uint8_t out[kPacketLen]) {
    std::memset(out, 0, kPacketLen);
    out[0] = 0x00;
    out[1] = 0x85;
    ApplyChecksum7(out);
}

bool ParseProfileReply(const std::uint8_t reply[kPacketLen], int* profile_out) {
    if (reply == nullptr) return false;
    const std::uint8_t* p = reply + 1;
    if (p[0] != 0x85u) return false;
    const int prof = static_cast<int>(p[1]);
    if (!ProfileValid(prof)) return false;
    if (profile_out != nullptr) *profile_out = prof;
    return true;
}

void BuildSetProfilePacket(int profile, std::uint8_t out[kPacketLen]) {
    std::memset(out, 0, kPacketLen);
    out[0] = 0x00;
    out[1] = 0x05;
    out[2] = static_cast<std::uint8_t>(profile);
    ApplyChecksum7(out);
}

void BuildGetKbOptionsPacket(std::uint8_t out[kPacketLen]) {
    std::memset(out, 0, kPacketLen);
    out[0] = 0x00;
    out[1] = 0x86;
    ApplyChecksum7(out);
}

bool ParseKbOptionsReply(const std::uint8_t reply[kPacketLen],
                         KbOptions* out,
                         RawKbOptionsFrame* raw_out) {
    if (reply == nullptr) return false;
    const std::uint8_t* p = reply + 1;
    if (p[0] != 0x86u) return false;

    if (out != nullptr) {
        const std::uint8_t flags = p[2];
        out->raw_flags = flags;
        out->win_lock = (flags & kOptWinLock) != 0u;
        out->mac_mode = (flags & kOptMacMode) != 0u;
        out->wasd_swap = (flags & kOptWasdSwap) != 0u;
        out->backlight_off = (flags & kOptBacklightOff) != 0u;
        out->sidelight_off = (flags & kOptSidelightOff) != 0u;
    }
    if (raw_out != nullptr) {
        std::memcpy(raw_out->payload, p, kPayloadLen);
    }
    return true;
}

void BuildSetKbOptionsPacket(const RawKbOptionsFrame& base,
                             const KbOptionsPatch& patch,
                             std::uint8_t out[kPacketLen]) {
    std::memset(out, 0, kPacketLen);
    out[0] = 0x00;
    std::uint8_t* p = out + 1;
    std::memcpy(p + 1, base.payload + 1, 6);
    p[0] = 0x06;

    if (patch.have_win_lock) SetBit(&p[2], kOptWinLock, patch.win_lock);
    if (patch.have_mac_mode) SetBit(&p[2], kOptMacMode, patch.mac_mode);
    if (patch.have_wasd_swap) SetBit(&p[2], kOptWasdSwap, patch.wasd_swap);
    if (patch.have_backlight_off) SetBit(&p[2], kOptBacklightOff, patch.backlight_off);
    if (patch.have_sidelight_off) SetBit(&p[2], kOptSidelightOff, patch.sidelight_off);

    ApplyChecksum7(out);
}

bool KbOptionsMatchPatch(const KbOptions& actual, const KbOptionsPatch& patch) {
    if (patch.have_win_lock && actual.win_lock != patch.win_lock) return false;
    if (patch.have_mac_mode && actual.mac_mode != patch.mac_mode) return false;
    if (patch.have_wasd_swap && actual.wasd_swap != patch.wasd_swap) return false;
    if (patch.have_backlight_off && actual.backlight_off != patch.backlight_off) return false;
    if (patch.have_sidelight_off && actual.sidelight_off != patch.sidelight_off) return false;
    return true;
}

bool MainLedModeValid(int mode) {
    return mode >= 1 && mode <= 19 && mode != 13;
}

bool MainLedSupportsColor(int mode) {
    return mode != 3 && mode != 12 && mode != 16;
}

bool SideLedModeValid(int mode) {
    return mode >= 0 && mode <= kSideModeMax;
}

bool SideLedSupportsColor(int mode) {
    return mode == 1 || mode == 2 || mode == 4 || mode == 5;
}

bool LedBrightnessValid(int brightness) {
    return brightness >= kBrightnessMin && brightness <= kBrightnessMax;
}

bool LedSpeedValid(int speed) {
    return speed >= kSpeedMin && speed <= kSpeedMax;
}

bool LedDirectionValid(int direction) {
    return direction >= kDirectionMin && direction <= kDirectionMax;
}

bool RgbChannelValid(int value) {
    return value >= 0 && value <= 255;
}

void BuildGetLedPacket(bool side, std::uint8_t out[kPacketLen]) {
    std::memset(out, 0, kPacketLen);
    out[0] = 0x00;
    out[1] = side ? 0x88u : 0x87u;
    ApplyChecksum7(out);
}

bool ParseLedReply(bool side, const std::uint8_t reply[kPacketLen], LedParams* out) {
    if (reply == nullptr) return false;
    const std::uint8_t* p = reply + 1;
    const std::uint8_t expected_op = side ? 0x88u : 0x87u;
    if (p[0] != expected_op) return false;

    const int mode = static_cast<int>(p[1]);
    const int wire_speed = static_cast<int>(p[2]);
    const int speed = side ? ((wire_speed >= 0 && wire_speed <= kSpeedMax) ? wire_speed : -1)
                           : ((wire_speed >= 1 && wire_speed <= 5) ? (5 - wire_speed) : -1);
    const int brightness = (p[3] <= kBrightnessMax) ? static_cast<int>(p[3]) : -1;
    if (speed < 0 || brightness < 0) return false;

    const std::uint8_t opt_flags = p[4];
    const int direction = static_cast<int>((opt_flags >> 4) & 0x03u);
    const std::uint8_t flags = static_cast<std::uint8_t>(opt_flags & 0x0Fu);

    bool colorful = true;
    int r = static_cast<int>(p[5]);
    int g = static_cast<int>(p[6]);
    int b = static_cast<int>(p[7]);
    if (flags <= 6u) {
        PresetColorRgb(flags, &r, &g, &b);
        colorful = false;
    } else {
        colorful = (flags == 0x08u);
        if (r == 0xFA && g == 0xFA && b == 0xFA) {
            r = 255;
            g = 255;
            b = 255;
        }
    }

    if (out != nullptr) {
        out->mode = mode;
        out->speed = speed;
        out->brightness = brightness;
        out->colorful = colorful;
        out->r = r;
        out->g = g;
        out->b = b;
        out->direction = direction;
    }
    return true;
}

LedParams MergeLedPatch(const LedParams& base, const LedPatch& patch, bool side) {
    LedParams next = base;
    if (patch.have_mode) next.mode = patch.mode;
    if (patch.have_speed) next.speed = patch.speed;
    if (patch.have_brightness) next.brightness = patch.brightness;
    if (patch.have_colorful) next.colorful = patch.colorful;
    if (patch.have_color) {
        next.r = patch.r;
        next.g = patch.g;
        next.b = patch.b;
        if (!patch.have_colorful) {
            next.colorful = false;
        }
    }
    if (patch.have_direction) next.direction = patch.direction;

    const bool supports_color =
        side ? SideLedSupportsColor(next.mode) : MainLedSupportsColor(next.mode);
    if (!supports_color) {
        next.colorful = true;
    }
    return next;
}

void BuildSetLedPacket(bool side, const LedParams& params, std::uint8_t out[kPacketLen]) {
    std::memset(out, 0, kPacketLen);
    out[0] = 0x00;
    std::uint8_t* p = out + 1;
    p[0] = side ? 0x08u : 0x07u;
    p[1] = static_cast<std::uint8_t>(params.mode);
    p[2] = static_cast<std::uint8_t>(side ? params.speed : (5 - params.speed));
    p[3] = static_cast<std::uint8_t>(params.brightness);

    const bool supports_color =
        side ? SideLedSupportsColor(params.mode) : MainLedSupportsColor(params.mode);
    const std::uint8_t flags = (params.colorful || !supports_color) ? 0x08u : 0x07u;
    const std::uint8_t opt = static_cast<std::uint8_t>(params.direction & 0x03u);
    p[4] = static_cast<std::uint8_t>((opt << 4) | flags);

    int r = params.r;
    int g = params.g;
    int b = params.b;
    if (r == 255 && g == 255 && b == 255) {
        r = 0xFA;
        g = 0xFA;
        b = 0xFA;
    }
    p[5] = static_cast<std::uint8_t>(r);
    p[6] = static_cast<std::uint8_t>(g);
    p[7] = static_cast<std::uint8_t>(b);
    ApplyChecksum8(out);
}

bool LedMatchesTarget(const LedParams& actual, const LedParams& target, bool side) {
    if (actual.mode != target.mode) return false;
    if (actual.speed != target.speed) return false;
    if (actual.brightness != target.brightness) return false;
    if (actual.direction != target.direction) return false;

    const bool supports_color =
        side ? SideLedSupportsColor(target.mode) : MainLedSupportsColor(target.mode);
    if (!supports_color) {
        return true;
    }
    if (actual.colorful != target.colorful) return false;
    if (!target.colorful) {
        if (actual.r != target.r || actual.g != target.g || actual.b != target.b) {
            return false;
        }
    }
    return true;
}

bool DebounceValid(int level) {
    return level >= kDebounceMin && level <= kDebounceMax;
}

void BuildGetDebouncePacket(std::uint8_t out[kPacketLen]) {
    std::memset(out, 0, kPacketLen);
    out[0] = 0x00;
    out[1] = 0x91;
    ApplyChecksum7(out);
}

bool ParseDebounceReply(const std::uint8_t reply[kPacketLen], int* level_out) {
    if (reply == nullptr) return false;
    const std::uint8_t* p = reply + 1;
    if (p[0] != 0x91u || p[1] != 0x00u) return false;
    const int level = static_cast<int>(p[2]);
    if (!DebounceValid(level)) return false;
    if (level_out != nullptr) *level_out = level;
    return true;
}

void BuildSetDebouncePacket(int level, std::uint8_t out[kPacketLen]) {
    std::memset(out, 0, kPacketLen);
    out[0] = 0x00;
    out[1] = 0x11;
    out[2] = 0x00;
    out[3] = static_cast<std::uint8_t>(level);
    ApplyChecksum7(out);
}

bool SleepSecValid(int seconds) {
    return seconds >= kSleepSecMin && seconds <= kSleepSecMax;
}

void BuildGetSleepPacket(std::uint8_t out[kPacketLen]) {
    std::memset(out, 0, kPacketLen);
    out[0] = 0x00;
    out[1] = 0x92;
    ApplyChecksum7(out);
}

bool ParseSleepReply(const std::uint8_t reply[kPacketLen], SleepTimers* out) {
    if (reply == nullptr) return false;
    const std::uint8_t* p = reply + 1;
    if (p[0] != 0x92u) return false;
    const int s_bt = static_cast<int>(ReadLe16(p + 1));
    const int s_24g = static_cast<int>(ReadLe16(p + 3));
    const int d_bt = static_cast<int>(ReadLe16(p + 5));
    const int d_24g = static_cast<int>(ReadLe16(p + 7));
    if (s_bt == 0 && s_24g == 0 && d_bt == 0 && d_24g == 0) return false;
    if (out != nullptr) {
        out->sleep_bt = s_bt;
        out->sleep_24g = s_24g;
        out->deep_bt = d_bt;
        out->deep_24g = d_24g;
    }
    return true;
}

SleepTimers MergeSleepPatch(const SleepTimers& base, const SleepPatch& patch) {
    SleepTimers next = base;
    if (patch.have_sleep_bt) next.sleep_bt = patch.sleep_bt;
    if (patch.have_sleep_24g) next.sleep_24g = patch.sleep_24g;
    if (patch.have_deep_bt) next.deep_bt = patch.deep_bt;
    if (patch.have_deep_24g) next.deep_24g = patch.deep_24g;
    return next;
}

void BuildSetSleepPacket(const SleepTimers& timers,
                         std::uint8_t out[kPacketLen],
                         bool header_only_ck7) {
    std::memset(out, 0, kPacketLen);
    out[0] = 0x00;
    std::uint8_t* p = out + 1;
    p[0] = 0x12;
    WriteLe16(p + 8, static_cast<std::uint16_t>(timers.sleep_bt));
    WriteLe16(p + 10, static_cast<std::uint16_t>(timers.sleep_24g));
    WriteLe16(p + 12, static_cast<std::uint16_t>(timers.deep_bt));
    WriteLe16(p + 14, static_cast<std::uint16_t>(timers.deep_24g));
    if (header_only_ck7) {
        p[7] = static_cast<std::uint8_t>(0xFFu - 0x12u);
    } else {
        ApplyChecksum7(out);
    }
}

bool SleepMatchesTarget(const SleepTimers& actual, const SleepTimers& target) {
    return actual.sleep_bt == target.sleep_bt &&
           actual.sleep_24g == target.sleep_24g &&
           actual.deep_bt == target.deep_bt &&
           actual.deep_24g == target.deep_24g;
}

KeyEntry KeyMatrix::Slot(int slot) const {
    KeyEntry e;
    if (slot < 0 || slot >= kKeymapSlotCount) return e;
    const std::size_t off = static_cast<std::size_t>(slot) * 4u;
    std::memcpy(e.b, bytes + off, 4);
    return e;
}

void KeyMatrix::SetSlot(int slot, const KeyEntry& entry) {
    if (slot < 0 || slot >= kKeymapSlotCount) return;
    const std::size_t off = static_cast<std::size_t>(slot) * 4u;
    std::memcpy(bytes + off, entry.b, 4);
}

bool KeySlotValid(int slot) {
    return slot >= 0 && slot <= kKeymapWriteMaxSlot;
}

bool KeyEntryValid(const KeyEntry& entry, bool /*fn_layer*/, std::string* reason) {
    auto fail = [&](const std::string& msg) {
        if (reason != nullptr) *reason = msg;
        return false;
    };

    const std::uint8_t b0 = entry.b[0];
    const std::uint8_t b1 = entry.b[1];
    const std::uint8_t b2 = entry.b[2];
    const std::uint8_t b3 = entry.b[3];

    if (b0 == 0x00u) {
        if (b1 == 0u && b2 == 0u && b3 == 0u) return true;
        auto valid_hid = [](std::uint8_t u) { return u >= 0x04u && u <= 0xEFu; };
        if (!valid_hid(b2)) {
            return fail("HID key usage (byte 2) must be 0x04..0xEF");
        }
        if ((b1 != 0u && !valid_hid(b1)) || (b3 != 0u && !valid_hid(b3))) {
            return fail("combo modifier usages (bytes 1 and 3) must be 0 or 0x04..0xEF");
        }
        return true;
    }

    if (b0 == 0x01u) {
        if (b1 != 0u) return fail("mouse entry byte 1 must be 0");
        if (b2 >= 240u && b2 <= 244u && b3 == 0u) return true;
        if (b2 == 245u && (b3 == 1u || b3 == 255u)) return true;
        if ((b2 == 246u || b2 == 247u) && (b3 == 5u || b3 == 251u)) return true;
        return fail("invalid mouse key entry");
    }

    if (b0 == 0x02u) {
        if (b1 >= 129u && b1 <= 131u && b2 == 0u && b3 == 0u) return true;
        return fail("invalid system power key entry");
    }

    if (b0 == 0x03u) {
        const unsigned usage = static_cast<unsigned>(b2) | (static_cast<unsigned>(b3) << 8);
        if (b1 == 0u && usage > 0u && usage <= 0x03FFu) return true;
        return fail("invalid consumer/media key entry");
    }

    if (b0 == 0x04u) {
        if (b1 >= 1u && b1 <= 4u && b2 == 0u && b3 == 0u) return true;
        return fail("invalid wheel scroll/tilt key entry");
    }

    if (b0 == 0x05u) {
        if (b1 == 0u && b2 >= 1u && b2 <= 3u && b3 == 0u) return true;
        return fail("invalid report rate key entry");
    }

    if (b0 == 0x08u) {
        if (b1 == 0u && b2 >= 1u && b2 <= 3u && b3 == 0u) return true;
        return fail("invalid profile switch key entry");
    }

    if (b0 == 0x09u) {
        if (b1 <= 2u && b2 < static_cast<std::uint8_t>(kMacroSlotCount) && b3 == 0u) return true;
        return fail("macro key entry requires mode 0..2 and slot 0..49");
    }

    if (b0 == 0x0Au) {
        if (b1 == 7u) {
            return fail("KB Lock ([10,7,0,0]) is blocked for safety");
        }
        if (b1 >= 1u && b1 <= 23u && b2 == 0u && b3 == 0u) return true;
        return fail("invalid keyboard special function entry");
    }

    if (b0 == 0x0Du) {
        if (b1 <= 8u) return true;
        return fail("invalid lighting control key entry");
    }

    if (b0 == 0x0Eu) {
        if (b1 == 0u && (b2 <= 6u || b2 == 255u) && b3 == 0u) return true;
        if (b1 == 1u && b2 <= 1u && b3 == 0u) return true;
        return fail("invalid wireless mode key entry");
    }

    return fail("unsupported key entry tag " + std::to_string(static_cast<unsigned>(b0)));
}

bool IsBc98PhysicalSlot(int slot) {
    static const int kPhysicalSlots[] = {
        0,   1,   2,   3,   4,   5,   6,   7,   8,   9,   12,  13,  14,  15,
        16,  17,  18,  19,  20,  21,  22,  23,  24,  25,  26,  27,  28,  30,
        31,  32,  33,  34,  36,  37,  38,  39,  40,  41,  42,  43,  44,  45,
        46,  48,  49,  50,  51,  52,  54,  55,  56,  57,  58,  59,  60,  61,
        62,  63,  64,  65,  66,  67,  68,  69,  70,  71,  72,  73,  74,  76,
        77,  79,  80,  81,  82,  83,  84,  85,  86,  87,  88,  89,  90,  91,
        92,  93,  94,  95,  96,  97,  98,  99,  100, 101, 102, 103, 104, 105
    };
    for (int s : kPhysicalSlots) {
        if (s == slot) return true;
    }
    return false;
}

void GetBc98FactoryKeymap(bool fn_layer, KeyMatrix* out) {
    if (out == nullptr) return;
    std::memset(out->bytes, 0, sizeof(out->bytes));

    struct SlotInit {
        int slot;
        std::uint8_t b0;
        std::uint8_t b1;
        std::uint8_t b2;
        std::uint8_t b3;
    };

    if (!fn_layer) {
        static const SlotInit kFactoryBase[] = {
            {0, 0, 0, 41, 0},     {1, 0, 0, 53, 0},     {2, 0, 0, 43, 0},     {3, 0, 0, 57, 0},
            {4, 0, 0, 225, 0},    {5, 0, 0, 224, 0},    {6, 0, 0, 58, 0},     {7, 0, 0, 30, 0},
            {8, 0, 0, 20, 0},     {9, 0, 0, 4, 0},      {10, 0, 0, 100, 0},   {12, 0, 0, 59, 0},
            {13, 0, 0, 31, 0},    {14, 0, 0, 26, 0},    {15, 0, 0, 22, 0},    {16, 0, 0, 29, 0},
            {17, 0, 0, 227, 0},   {18, 0, 0, 60, 0},    {19, 0, 0, 32, 0},    {20, 0, 0, 8, 0},
            {21, 0, 0, 7, 0},     {22, 0, 0, 27, 0},    {23, 0, 0, 226, 0},   {24, 0, 0, 61, 0},
            {25, 0, 0, 33, 0},    {26, 0, 0, 21, 0},    {27, 0, 0, 9, 0},     {28, 0, 0, 6, 0},
            {30, 0, 0, 62, 0},    {31, 0, 0, 34, 0},    {32, 0, 0, 23, 0},    {33, 0, 0, 10, 0},
            {34, 0, 0, 25, 0},    {36, 0, 0, 63, 0},    {37, 0, 0, 35, 0},    {38, 0, 0, 28, 0},
            {39, 0, 0, 11, 0},    {40, 0, 0, 5, 0},     {41, 0, 0, 44, 0},    {42, 0, 0, 64, 0},
            {43, 0, 0, 36, 0},    {44, 0, 0, 24, 0},    {45, 0, 0, 13, 0},    {46, 0, 0, 17, 0},
            {48, 0, 0, 65, 0},    {49, 0, 0, 37, 0},    {50, 0, 0, 12, 0},    {51, 0, 0, 14, 0},
            {52, 0, 0, 16, 0},    {54, 0, 0, 66, 0},    {55, 0, 0, 38, 0},    {56, 0, 0, 18, 0},
            {57, 0, 0, 15, 0},    {58, 0, 0, 54, 0},    {59, 0, 0, 230, 0},   {60, 0, 0, 67, 0},
            {61, 0, 0, 39, 0},    {62, 0, 0, 19, 0},    {63, 0, 0, 51, 0},    {64, 0, 0, 55, 0},
            {65, 10, 1, 0, 0},    {66, 0, 0, 68, 0},    {67, 0, 0, 45, 0},    {68, 0, 0, 47, 0},
            {69, 0, 0, 52, 0},    {70, 0, 0, 56, 0},    {71, 0, 0, 228, 0},   {72, 0, 0, 69, 0},
            {73, 0, 0, 46, 0},    {74, 0, 0, 48, 0},    {75, 0, 0, 50, 0},    {76, 0, 0, 229, 0},
            {77, 0, 0, 80, 0},    {79, 0, 0, 42, 0},    {80, 0, 0, 49, 0},    {81, 0, 0, 40, 0},
            {82, 0, 0, 82, 0},    {83, 0, 0, 81, 0},    {84, 0, 0, 76, 0},    {85, 0, 0, 83, 0},
            {86, 0, 0, 95, 0},    {87, 0, 0, 92, 0},    {88, 0, 0, 89, 0},    {89, 0, 0, 79, 0},
            {90, 0, 0, 70, 0},    {91, 0, 0, 84, 0},    {92, 0, 0, 96, 0},    {93, 0, 0, 93, 0},
            {94, 0, 0, 90, 0},    {95, 0, 0, 98, 0},    {96, 0, 0, 75, 0},    {97, 0, 0, 85, 0},
            {98, 0, 0, 97, 0},    {99, 0, 0, 94, 0},    {100, 0, 0, 91, 0},   {101, 0, 0, 99, 0},
            {102, 0, 0, 78, 0},   {103, 0, 0, 86, 0},   {104, 0, 0, 87, 0},   {105, 0, 0, 88, 0},
        };
        for (const auto& item : kFactoryBase) {
            out->SetSlot(item.slot, KeyEntry{{item.b0, item.b1, item.b2, item.b3}});
        }
    } else {
        static const SlotInit kFactoryFn[] = {
            {0, 10, 2, 0, 0},     {6, 3, 0, 112, 0},    {7, 14, 0, 0, 0},    {12, 3, 0, 111, 0},
            {13, 14, 0, 1, 0},    {17, 10, 3, 0, 0},    {18, 3, 0, 138, 1},   {19, 14, 0, 2, 0},
            {24, 3, 0, 146, 1},   {25, 14, 0, 5, 0},    {30, 3, 0, 148, 1},   {36, 3, 0, 131, 1},
            {42, 3, 0, 182, 0},   {48, 3, 0, 205, 0},   {54, 3, 0, 181, 0},   {56, 0, 0, 71, 0},
            {60, 3, 0, 226, 0},   {62, 0, 0, 72, 0},    {66, 3, 0, 234, 0},   {72, 3, 0, 233, 0},
            {77, 13, 3, 1, 0},    {79, 10, 8, 0, 0},    {82, 13, 2, 1, 0},    {83, 13, 2, 2, 0},
            {84, 13, 1, 0, 0},    {89, 13, 3, 2, 0},    {90, 13, 5, 1, 0},    {96, 10, 12, 0, 0},
        };
        for (const auto& item : kFactoryFn) {
            out->SetSlot(item.slot, KeyEntry{{item.b0, item.b1, item.b2, item.b3}});
        }
    }
}

void BuildGetKeymapPagePacket(int profile, int page, bool fn_layer, std::uint8_t out[kPacketLen]) {
    std::memset(out, 0, kPacketLen);
    out[0] = 0x00;
    out[1] = fn_layer ? 0x90u : 0x89u;
    out[2] = static_cast<std::uint8_t>(profile);
    out[3] = static_cast<std::uint8_t>(page);
    ApplyChecksum7(out);
}

void BuildSetKeyOnePacket(int profile,
                          int slot,
                          bool fn_layer,
                          const KeyEntry& entry,
                          std::uint8_t out[kPacketLen]) {
    std::memset(out, 0, kPacketLen);
    out[0] = 0x00;
    std::uint8_t* p = out + 1;
    p[0] = fn_layer ? 0x15u : 0x13u;
    p[1] = static_cast<std::uint8_t>(profile);
    p[2] = static_cast<std::uint8_t>(slot);
    ApplyChecksum7(out);
    std::memcpy(p + 8, entry.b, 4);
}

void BuildBulkKeymapPagePacket(int profile,
                               int page,
                               bool fn_layer,
                               const KeyMatrix& matrix,
                               std::uint8_t out[kPacketLen]) {
    std::memset(out, 0, kPacketLen);
    out[0] = 0x00;
    std::uint8_t* p = out + 1;
    p[0] = fn_layer ? 0x10u : 0x09u;
    p[1] = static_cast<std::uint8_t>(profile);
    p[2] = 0xF8u;
    p[3] = 1u;
    p[4] = static_cast<std::uint8_t>(page);
    ApplyChecksum7(out);

    const std::size_t start = static_cast<std::size_t>(page) * 56u;
    if (start < kKeymapBytes) {
        const std::size_t avail = kKeymapBytes - start;
        const std::size_t copy_len = avail < 56u ? avail : 56u;
        std::memcpy(p + 8, matrix.bytes + start, copy_len);
    }
}

bool MacroSlotValid(int slot) {
    return slot >= 0 && slot < kMacroSlotCount;
}

bool EncodeMacroBlob(const MacroData& macro,
                     std::uint8_t blob[kMacroBytes],
                     std::string* error,
                     int* stream_bytes_out) {
    auto fail = [&](const std::string& msg) {
        if (error != nullptr) *error = msg;
        return false;
    };

    if (macro.repeat < kMacroRepeatMin || macro.repeat > kMacroRepeatMax) {
        return fail("macro repeat must be 1..65535");
    }
    if (macro.events.empty()) {
        return fail("macro events must not be empty");
    }

    bool key_down[256]{};
    bool btn_down[5]{};
    std::vector<std::uint8_t> stream;
    stream.reserve(kMacroMaxStreamBytes + 4);

    auto append_press = [&](std::uint8_t wire_code, bool pressed, int delay_ms) {
        const std::uint8_t hi = pressed ? 0x80u : 0x00u;
        if (delay_ms >= 1 && delay_ms <= 127) {
            stream.push_back(wire_code);
            stream.push_back(static_cast<std::uint8_t>(hi | static_cast<std::uint8_t>(delay_ms)));
        } else {
            stream.push_back(wire_code);
            stream.push_back(hi);
            stream.push_back(static_cast<std::uint8_t>(delay_ms & 0xFF));
            stream.push_back(static_cast<std::uint8_t>((delay_ms >> 8) & 0xFF));
        }
    };

    for (std::size_t i = 0; i < macro.events.size(); ++i) {
        const MacroEvent& ev = macro.events[i];
        if (ev.kind == MacroEventKind::kKey) {
            if (ev.code < 0x04u || ev.code > 0xEFu) {
                return fail("macro key usage out of range (0x04..0xEF)");
            }
            if (ev.delay_ms < 0 || ev.delay_ms > 65535) {
                return fail("macro key delay must be 0..65535 ms");
            }
            key_down[ev.code] = ev.pressed;
            append_press(ev.code, ev.pressed, ev.delay_ms);
        } else if (ev.kind == MacroEventKind::kMouseButton) {
            if (ev.code > 4u) {
                return fail("macro mouse button out of range (0..4)");
            }
            if (ev.delay_ms < 0 || ev.delay_ms > 65535) {
                return fail("macro mouse button delay must be 0..65535 ms");
            }
            btn_down[ev.code] = ev.pressed;
            append_press(static_cast<std::uint8_t>(0xF0u + ev.code), ev.pressed, ev.delay_ms);
        } else if (ev.kind == MacroEventKind::kMouseMove) {
            if (ev.dx < kMacroMoveMinDelta || ev.dx > kMacroMoveMaxDelta ||
                ev.dy < kMacroMoveMinDelta || ev.dy > kMacroMoveMaxDelta) {
                return fail("macro mouse move dx/dy must be -127..127");
            }
            if (ev.delay_ms < 0 || ev.delay_ms > kMacroMoveMaxDelayMs) {
                return fail("macro mouse move delay must be 0..255 ms");
            }
            stream.push_back(0xF9u);
            stream.push_back(static_cast<std::uint8_t>(ev.delay_ms));
            stream.push_back(static_cast<std::uint8_t>(static_cast<std::int8_t>(ev.dx)));
            stream.push_back(static_cast<std::uint8_t>(static_cast<std::int8_t>(ev.dy)));
        } else {
            return fail("unknown macro event kind");
        }
    }

    for (int u = 0x04; u <= 0xEF; ++u) {
        if (key_down[u]) {
            return fail("macro leaves key usage " + std::to_string(u) + " pressed without release");
        }
    }
    for (int b = 0; b < 5; ++b) {
        if (btn_down[b]) {
            return fail("macro leaves mouse button " + std::to_string(b) + " pressed without release");
        }
    }

    if (stream.size() > kMacroMaxStreamBytes) {
        return fail("macro too long: " + std::to_string(stream.size()) +
                    " bytes exceeds 250-byte hardware limit");
    }

    std::memset(blob, 0, kMacroBytes);
    WriteLe16(blob, static_cast<std::uint16_t>(macro.repeat));
    std::memcpy(blob + 2, stream.data(), stream.size());
    if (stream_bytes_out != nullptr) *stream_bytes_out = static_cast<int>(stream.size());
    return true;
}

MacroData DecodeMacroBlob(const std::uint8_t blob[kMacroBytes], bool* is_empty_out) {
    MacroData out;
    out.repeat = static_cast<int>(ReadLe16(blob));
    std::size_t i = 2;
    while (i + 1u < kMacroBytes) {
        const std::uint8_t code = blob[i];
        if (code == 0u) break;
        if (code == 0xF9u) {
            if (i + 3u >= kMacroBytes) break;
            MacroEvent ev;
            ev.kind = MacroEventKind::kMouseMove;
            ev.delay_ms = static_cast<int>(blob[i + 1]);
            ev.dx = static_cast<int>(static_cast<std::int8_t>(blob[i + 2]));
            ev.dy = static_cast<int>(static_cast<std::int8_t>(blob[i + 3]));
            out.events.push_back(ev);
            i += 4u;
            continue;
        }
        const bool pressed = (blob[i + 1] & 0x80u) != 0u;
        const std::uint8_t low = static_cast<std::uint8_t>(blob[i + 1] & 0x7Fu);
        int delay_ms = 0;
        if (low != 0u) {
            delay_ms = static_cast<int>(low);
            i += 2u;
        } else {
            if (i + 3u >= kMacroBytes) break;
            delay_ms = static_cast<int>(ReadLe16(blob + i + 2));
            i += 4u;
        }
        if (code >= 0x04u && code <= 0xEFu) {
            MacroEvent ev;
            ev.kind = MacroEventKind::kKey;
            ev.code = code;
            ev.pressed = pressed;
            ev.delay_ms = delay_ms;
            out.events.push_back(ev);
        } else if (code >= 0xF0u && code <= 0xF4u) {
            MacroEvent ev;
            ev.kind = MacroEventKind::kMouseButton;
            ev.code = static_cast<std::uint8_t>(code - 0xF0u);
            ev.pressed = pressed;
            ev.delay_ms = delay_ms;
            out.events.push_back(ev);
        } else {
            break;
        }
    }
    if (is_empty_out != nullptr) *is_empty_out = out.events.empty();
    return out;
}

int CountMacroWritePages(const std::uint8_t blob[kMacroBytes]) {
    int last_nonzero_page = 0;
    for (int page = 0; page * kMacroWritePageData < static_cast<int>(kMacroBytes); ++page) {
        const std::size_t start = static_cast<std::size_t>(page) * kMacroWritePageData;
        const std::size_t end =
            std::min<std::size_t>(kMacroBytes, start + static_cast<std::size_t>(kMacroWritePageData));
        for (std::size_t i = start; i < end; ++i) {
            if (blob[i] != 0u) {
                last_nonzero_page = page;
                break;
            }
        }
    }
    return last_nonzero_page + 1;
}

void BuildGetMacroPagePacket(int slot, int page, std::uint8_t out[kPacketLen]) {
    std::memset(out, 0, kPacketLen);
    out[0] = 0x00;
    out[1] = 0x8Bu;
    out[2] = static_cast<std::uint8_t>(slot);
    out[3] = static_cast<std::uint8_t>(page);
    ApplyChecksum7(out);
}

void BuildSetMacroPagePacket(int slot,
                             int page,
                             bool is_last,
                             const std::uint8_t blob[kMacroBytes],
                             std::uint8_t out[kPacketLen]) {
    std::memset(out, 0, kPacketLen);
    out[0] = 0x00;
    std::uint8_t* p = out + 1;
    p[0] = 0x16u;
    p[1] = static_cast<std::uint8_t>(slot);
    p[2] = static_cast<std::uint8_t>(page);
    p[3] = static_cast<std::uint8_t>(kMacroWritePageData);
    p[4] = is_last ? 1u : 0u;
    ApplyChecksum7(out);

    const std::size_t start = static_cast<std::size_t>(page) * kMacroWritePageData;
    if (start < kMacroBytes) {
        const std::size_t avail = kMacroBytes - start;
        const std::size_t copy_len =
            avail < static_cast<std::size_t>(kMacroWritePageData) ? avail : static_cast<std::size_t>(kMacroWritePageData);
        std::memcpy(p + 8, blob + start, copy_len);
    }
}

}  // namespace royuan
