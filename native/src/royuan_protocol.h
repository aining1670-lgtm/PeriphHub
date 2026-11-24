#pragma once
// ROYUAN (yc500 family) keyboard wire protocol for AKOS BC98
// (VID 0x3151, PID 0x4015 USB / 2.4G dongles, Device ID 0x03AE, UsagePage 0xFFFF / Usage 0x0002,
// 65-byte HID Feature Reports: Report ID 0x00 + 64-byte payload).
// Pure C++17 functions with no Win32 dependencies so they can be unit-tested standalone.
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace royuan {

constexpr std::size_t kPacketLen = 65;   // Report ID (0x00) + 64-byte payload
constexpr std::size_t kPayloadLen = 64;

constexpr std::uint16_t kVid = 0x3151;
constexpr std::uint16_t kPidWiredBc98 = 0x4015;
constexpr std::uint16_t kDevIdAkosBc98 = 0x03AE;  // 942 decimal

constexpr std::uint16_t kUsagePage = 0xFFFF;
constexpr std::uint16_t kUsage = 0x0002;

constexpr std::uint8_t kRelayTargetKeyboard = 10;

constexpr int kProfileMin = 0;
constexpr int kProfileMax = 3;

constexpr int kBrightnessMin = 0;
constexpr int kBrightnessMax = 4;
constexpr int kSpeedMin = 0;
constexpr int kSpeedMax = 4;
constexpr int kDirectionMin = 0;
constexpr int kDirectionMax = 3;

constexpr int kSideModeMax = 5;

constexpr int kDebounceMin = 1;
constexpr int kDebounceMax = 10;

constexpr int kSleepSecMin = 60;
constexpr int kSleepSecMax = 3600;

constexpr int kKeymapSlotCount = 128;
constexpr int kKeymapWriteMaxSlot = 125;  // Bulk 0x09/0x10 sends 9x56 = 504B (slots 0..125)
constexpr std::size_t kKeymapBytes = 512;
constexpr int kKeymapReadPages = 8;
constexpr int kKeymapBulkPages = 9;
constexpr int kKeymapFnPhysicalSlot = 65;

constexpr int kMacroSlotCount = 50;
constexpr std::size_t kMacroBytes = 256;
constexpr int kMacroReadPages = 4;
constexpr int kMacroWritePageData = 56;
constexpr std::size_t kMacroMaxStreamBytes = 250;  // 2B repeat + <=250B events + >=4B zero tail
constexpr int kMacroRepeatMin = 1;
constexpr int kMacroRepeatMax = 65535;
constexpr int kMacroMoveMinDelta = -127;
constexpr int kMacroMoveMaxDelta = 127;
constexpr int kMacroMoveMaxDelayMs = 255;

// 0x06 / 0x86 payload[2] bitmasks. Bit 7 (0x80 = keyboard lock) is never set by this driver.
constexpr std::uint8_t kOptWinLock = 0x01;
constexpr std::uint8_t kOptMacMode = 0x02;
constexpr std::uint8_t kOptWasdSwap = 0x08;
constexpr std::uint8_t kOptBacklightOff = 0x10;
constexpr std::uint8_t kOptSidelightOff = 0x20;

struct DongleStatus {
    bool paired = false;
    bool online = false;
    int battery = -1;
    bool can_accept_relay = false;
    bool reply_ready = false;
};

struct RawKbOptionsFrame {
    std::uint8_t payload[kPayloadLen]{};
};

struct KbOptions {
    bool win_lock = false;
    bool mac_mode = false;
    bool wasd_swap = false;
    bool backlight_off = false;
    bool sidelight_off = false;
    std::uint8_t raw_flags = 0;
};

struct KbOptionsPatch {
    bool have_win_lock = false;
    bool win_lock = false;
    bool have_mac_mode = false;
    bool mac_mode = false;
    bool have_wasd_swap = false;
    bool wasd_swap = false;
    bool have_backlight_off = false;
    bool backlight_off = false;
    bool have_sidelight_off = false;
    bool sidelight_off = false;

    bool Any() const {
        return have_win_lock || have_mac_mode || have_wasd_swap ||
               have_backlight_off || have_sidelight_off;
    }
};

struct LedParams {
    int mode = 1;
    int speed = 2;       // UI scale 0..4
    int brightness = 3;  // 0..4
    bool colorful = true;
    int r = 255;
    int g = 255;
    int b = 255;
    int direction = 0;   // 0..3 (upper nibble of payload[4])
};

struct LedPatch {
    bool have_mode = false;
    int mode = 1;
    bool have_speed = false;
    int speed = 2;
    bool have_brightness = false;
    int brightness = 3;
    bool have_colorful = false;
    bool colorful = true;
    bool have_color = false;
    int r = 255;
    int g = 255;
    int b = 255;
    bool have_direction = false;
    int direction = 0;

    bool Any() const {
        return have_mode || have_speed || have_brightness ||
               have_colorful || have_color || have_direction;
    }
};

struct SleepTimers {
    int sleep_bt = -1;
    int sleep_24g = -1;
    int deep_bt = -1;
    int deep_24g = -1;
};

struct SleepPatch {
    bool have_sleep_bt = false;
    int sleep_bt = 120;
    bool have_sleep_24g = false;
    int sleep_24g = 120;
    bool have_deep_bt = false;
    int deep_bt = 1680;
    bool have_deep_24g = false;
    int deep_24g = 1680;

    bool Any() const {
        return have_sleep_bt || have_sleep_24g || have_deep_bt || have_deep_24g;
    }
};

bool IsDonglePid(std::uint16_t pid);
bool IsSupportedYc500Board(std::uint16_t dev_id);
std::string ResolveModelName(std::uint16_t dev_id, const std::string& raw_usb_name);

// Checksum helpers: operating on payload = out + 1 (64 bytes).
void ApplyChecksum7(std::uint8_t out[kPacketLen]);
void ApplyChecksum8(std::uint8_t out[kPacketLen]);

// Device ID (0x8F) & Firmware Revision (0x80)
void BuildGetDevIdPacket(std::uint8_t out[kPacketLen]);
bool ParseDevIdReply(const std::uint8_t reply[kPacketLen], std::uint16_t* dev_id_out);

void BuildGetFirmwarePacket(std::uint8_t out[kPacketLen]);
bool ParseFirmwareReply(const std::uint8_t reply[kPacketLen], std::string* version_out);

// 2.4G Dongle Status (0xF7) & Relay Control (0xF6 / 0xFC)
void BuildDongleStatusPacket(std::uint8_t out[kPacketLen]);
bool ParseDongleStatus(const std::uint8_t reply[kPacketLen], DongleStatus* out);
void BuildRelaySelectPacket(std::uint8_t target, std::uint8_t out[kPacketLen]);
void BuildRelayReleasePacket(std::uint8_t out[kPacketLen]);

// Profile (0x85 / 0x05)
bool ProfileValid(int profile);
void BuildGetProfilePacket(std::uint8_t out[kPacketLen]);
bool ParseProfileReply(const std::uint8_t reply[kPacketLen], int* profile_out);
void BuildSetProfilePacket(int profile, std::uint8_t out[kPacketLen]);

// Keyboard Options Bitfield (0x86 / 0x06)
void BuildGetKbOptionsPacket(std::uint8_t out[kPacketLen]);
bool ParseKbOptionsReply(const std::uint8_t reply[kPacketLen],
                         KbOptions* out,
                         RawKbOptionsFrame* raw_out = nullptr);
void BuildSetKbOptionsPacket(const RawKbOptionsFrame& base,
                             const KbOptionsPatch& patch,
                             std::uint8_t out[kPacketLen]);
bool KbOptionsMatchPatch(const KbOptions& actual, const KbOptionsPatch& patch);

// Main Backlight (0x87 / 0x07) & Side Edge Light (0x88 / 0x08)
bool MainLedModeValid(int mode);
bool MainLedSupportsColor(int mode);
bool SideLedModeValid(int mode);
bool SideLedSupportsColor(int mode);
bool LedBrightnessValid(int brightness);
bool LedSpeedValid(int speed);
bool LedDirectionValid(int direction);
bool RgbChannelValid(int value);

void BuildGetLedPacket(bool side, std::uint8_t out[kPacketLen]);
bool ParseLedReply(bool side, const std::uint8_t reply[kPacketLen], LedParams* out);
LedParams MergeLedPatch(const LedParams& base, const LedPatch& patch, bool side);
void BuildSetLedPacket(bool side, const LedParams& params, std::uint8_t out[kPacketLen]);
bool LedMatchesTarget(const LedParams& actual, const LedParams& target, bool side);

// Debounce (0x91 / 0x11)
bool DebounceValid(int level);
void BuildGetDebouncePacket(std::uint8_t out[kPacketLen]);
bool ParseDebounceReply(const std::uint8_t reply[kPacketLen], int* level_out);
void BuildSetDebouncePacket(int level, std::uint8_t out[kPacketLen]);

// Sleep Timers (0x92 / 0x12)
bool SleepSecValid(int seconds);
void BuildGetSleepPacket(std::uint8_t out[kPacketLen]);
bool ParseSleepReply(const std::uint8_t reply[kPacketLen], SleepTimers* out);
SleepTimers MergeSleepPatch(const SleepTimers& base, const SleepPatch& patch);
void BuildSetSleepPacket(const SleepTimers& timers,
                         std::uint8_t out[kPacketLen],
                         bool header_only_ck7 = true);
bool SleepMatchesTarget(const SleepTimers& actual, const SleepTimers& target);

// Keymap (0x89 / 0x90 / 0x13 / 0x15 / 0x09 / 0x10)
struct KeyEntry {
    std::uint8_t b[4]{};

    bool operator==(const KeyEntry& o) const {
        return b[0] == o.b[0] && b[1] == o.b[1] && b[2] == o.b[2] && b[3] == o.b[3];
    }
    bool operator!=(const KeyEntry& o) const { return !(*this == o); }
};

struct KeyMatrix {
    std::uint8_t bytes[kKeymapBytes]{};

    KeyEntry Slot(int slot) const;
    void SetSlot(int slot, const KeyEntry& entry);
};

bool KeySlotValid(int slot);
bool KeyEntryValid(const KeyEntry& entry, bool fn_layer, std::string* reason = nullptr);
bool IsBc98PhysicalSlot(int slot);
void GetBc98FactoryKeymap(bool fn_layer, KeyMatrix* out);

void BuildGetKeymapPagePacket(int profile, int page, bool fn_layer, std::uint8_t out[kPacketLen]);
void BuildSetKeyOnePacket(int profile,
                          int slot,
                          bool fn_layer,
                          const KeyEntry& entry,
                          std::uint8_t out[kPacketLen]);
void BuildBulkKeymapPagePacket(int profile,
                               int page,
                               bool fn_layer,
                               const KeyMatrix& matrix,
                               std::uint8_t out[kPacketLen]);

// Onboard Macros (0x8B / 0x16)
enum class MacroEventKind {
    kKey,
    kMouseButton,
    kMouseMove,
};

struct MacroEvent {
    MacroEventKind kind = MacroEventKind::kKey;
    std::uint8_t code = 0;  // kKey: HID usage 0x04..0xEF; kMouseButton: 0..4
    bool pressed = false;
    int dx = 0;             // kMouseMove: -127..127
    int dy = 0;             // kMouseMove: -127..127
    int delay_ms = 0;       // kKey/kMouseButton: 0..65535; kMouseMove: 0..255
};

struct MacroData {
    int repeat = 1;
    std::vector<MacroEvent> events;
};

bool MacroSlotValid(int slot);
bool EncodeMacroBlob(const MacroData& macro,
                     std::uint8_t blob[kMacroBytes],
                     std::string* error,
                     int* stream_bytes_out = nullptr);
MacroData DecodeMacroBlob(const std::uint8_t blob[kMacroBytes], bool* is_empty_out = nullptr);
int CountMacroWritePages(const std::uint8_t blob[kMacroBytes]);
void BuildGetMacroPagePacket(int slot, int page, std::uint8_t out[kPacketLen]);
void BuildSetMacroPagePacket(int slot,
                             int page,
                             bool is_last,
                             const std::uint8_t blob[kMacroBytes],
                             std::uint8_t out[kPacketLen]);

}  // namespace royuan
