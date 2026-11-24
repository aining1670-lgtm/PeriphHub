#pragma once
// YJX generic mouse wire protocol (VID 0xA8A4 USB / 0xA8A5 2.4G, PID 0x2255,
// UsagePage 0xFF01 / Usage 0x0010, 65-byte Interrupt OUT/IN reports).
// Pure C++17 functions with no Win32 dependencies so they can be unit-tested standalone.
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace yjx {

constexpr std::size_t kPacketLen = 65;   // Report ID (0x00) + 64-byte payload
constexpr std::size_t kPayloadLen = 64;

constexpr std::uint16_t kVidUsb = 0xA8A4;
constexpr std::uint16_t kVidDongle = 0xA8A5;
constexpr std::uint16_t kPidX11 = 0x2255;

constexpr std::uint16_t kUsagePage = 0xFF01;
constexpr std::uint16_t kUsage = 0x0010;

constexpr int kDpiMin = 200;
constexpr int kDpiMax = 12000;
constexpr int kDpiStep = 100;
constexpr int kDpiStageMax = 6;

constexpr int kDebounceMin = 0;
constexpr int kDebounceMax = 30;
constexpr int kLightModeMax = 6;

// sensor_flag (payload[50]) bitmask flags; unknown bits (e.g. bit2) are preserved.
constexpr std::uint8_t kSensorBitRipple = 0x01;
constexpr std::uint8_t kSensorBitFixline = 0x10;
constexpr std::uint8_t kSensorBitMotion = 0x20;

// Phase 2: 6 programmable buttons & 4096B onboard macro storage.
constexpr int kKeyCount = 6;
// Bridge index (left, right, middle, forward, back, dpi) -> YJX wire slot 0..5.
constexpr int kKeyWireSlot[kKeyCount] = {0, 1, 2, 4, 3, 5};

constexpr int kMacroSlotCount = 10;
constexpr std::size_t kMacroHeaderBytes = 64;
constexpr std::size_t kMacroMaxBytes = 4096;
constexpr std::size_t kMacroChunkPayload = 56;

struct KeyCode {
    std::uint8_t type = 0;
    std::uint8_t code1 = 0;
    std::uint8_t code2 = 0;
    std::uint8_t code3 = 0;

    bool operator==(const KeyCode& o) const {
        return type == o.type && code1 == o.code1 && code2 == o.code2 && code3 == o.code3;
    }
    bool operator!=(const KeyCode& o) const { return !(*this == o); }
};

struct RawKeysFrame {
    std::uint8_t payload[kPayloadLen]{};
};

struct MacroStep {
    bool is_mouse = false;
    bool down = false;
    std::uint8_t code = 0;
    std::uint16_t delay_ms = 0;
};

struct MacroSlotData {
    std::vector<MacroStep> steps;
};

// Complete 64-byte payload captured from a 0x0E reply (reply[1..64]).
// Preserved as the base for 0x0F writes so unknown/vendor-specific bytes are untouched.
struct RawConfigFrame {
    std::uint8_t payload[kPayloadLen]{};
};

struct Config {
    int light_mode = 0;         // 0..6 (0 = off, 1..6 = modes)
    int rate_hz = -1;           // 125, 250, 500, 1000
    int dpi_count = 0;          // 1..6
    int dpi_active = -1;        // 0-based active stage index
    int dpi[kDpiStageMax]{};    // 200..12000 in steps of 100
    bool scroll_reverse = false;
    int lod_wire = -1;          // 1 = 1.0mm, 2 = 2.0mm
    std::uint8_t sensor_flag = 0;
    bool ripple = false;
    bool fixline = false;
    bool motion = false;
    int debounce_ms = -1;       // payload[51], 0..30 ms
    int sleep_min = -1;         // payload[52], minutes (presets: 1,3,5,10,20,30,60)
};

struct ConfigPatch {
    bool have_light_mode = false;
    int light_mode = 0;

    bool have_rate_hz = false;
    int rate_hz = 0;

    bool have_dpi_count = false;
    int dpi_count = 0;

    bool have_dpi_active = false;
    int dpi_active = 0;  // 0-based

    bool have_dpi[kDpiStageMax]{};
    int dpi[kDpiStageMax]{};

    bool have_scroll_reverse = false;
    bool scroll_reverse = false;

    bool have_lod_wire = false;
    int lod_wire = 1;  // 1 = 1.0mm, 2 = 2.0mm

    bool have_ripple = false;
    bool ripple = false;

    bool have_fixline = false;
    bool fixline = false;

    bool have_motion = false;
    bool motion = false;

    bool have_debounce_ms = false;
    int debounce_ms = 0;

    bool have_sleep_min = false;
    int sleep_min = 0;
};

bool IsYjxDevice(std::uint16_t vid, std::uint16_t pid);
bool IsDongleVid(std::uint16_t vid);

bool DpiValid(int dpi);
bool DpiStageValid(int count, int active);

// Wire report rate code in payload[10]: 1->125, 2->250, 3->500, 4->1000 Hz.
int RateToWire(int hz);
int RateFromWire(std::uint8_t wire);

// Bridge LOD convention: 0 = 1.0mm (wire 1), 1 = 2.0mm (wire 2).
int LodBridgeToWire(int bridge_lod);
int LodWireToBridge(int wire_lod);

// Discrete sleep presets in seconds {60, 180, 300, 600, 1200, 1800, 3600} <-> minutes.
int SleepSecToMin(int seconds);
int SleepMinToSec(int minutes);

bool LightModeValid(int mode);
bool DebounceValid(int ms);

// Builds a 65-byte 0x0E read-config output report (Report ID 0x00 + 64B payload).
void BuildReadConfigPacket(std::uint8_t out[kPacketLen]);

// Parses a 65-byte 0x0E input report (reply[0] = Report ID 0x00, reply[1..64] = payload).
// Rejects non-0xAA/0x0E packets, uninitialized all-0x00/all-0xFF DPI bytes, and bad dpi_count.
bool ParseConfigReply(const std::uint8_t reply[kPacketLen],
                      Config* out,
                      RawConfigFrame* raw_out = nullptr);

// Builds a 65-byte 0x0F write-config output report from `base_raw`, overwriting only
// fields marked present in `patch` and preserving all unknown/reserved bytes intact.
void BuildWriteConfigPacket(const RawConfigFrame& base_raw,
                            const ConfigPatch& patch,
                            std::uint8_t out[kPacketLen]);

// Builds a 65-byte 0x21 light-mode output report.
void BuildLightModePacket(std::uint8_t mode, std::uint8_t out[kPacketLen]);

// Returns true when every field enabled in `patch` matches `actual`.
bool ConfigMatchesPatch(const Config& actual, const ConfigPatch& patch);

// Phase 2: Key remapping (0x08 read / 0x09 write).
const char* KeySlotName(int index);
std::uint32_t PackKeyCode(const KeyCode& kc);

void BuildReadKeysPacket(std::uint8_t out[kPacketLen]);
bool ParseKeysReply(const std::uint8_t reply[kPacketLen],
                    KeyCode out_keys[kKeyCount],
                    RawKeysFrame* raw_out = nullptr);
void BuildWriteKeysPacket(const RawKeysFrame& base,
                          int index,
                          const KeyCode& value,
                          std::uint8_t out[kPacketLen]);

bool EncodeBinding(const char* name, KeyCode* out);
void DecodeBinding(const KeyCode& kc, std::string* binding_out, std::string* kind_out);
void DecodeBindingForSlot(int index,
                          const KeyCode& kc,
                          std::string* binding_out,
                          std::string* kind_out);

// Phase 2: Onboard macros (0x0D chunk write + 0x10 commit, type 112 key binding).
KeyCode MakeMacroKeyCode(int macro_slot, int loop_type, int cycles);
bool EncodeMacroTable(const MacroSlotData slots[kMacroSlotCount],
                      std::vector<std::uint8_t>* out,
                      std::string* error);
void BuildMacroChunkPacket(std::uint16_t offset,
                           const std::uint8_t* data,
                           std::uint8_t len,
                           std::uint8_t out[kPacketLen]);
void BuildMacroCommitPacket(std::uint8_t out[kPacketLen]);

}  // namespace yjx
