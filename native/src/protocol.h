#pragma once
// Incott G23 wire protocol — pure functions, no Win32 dependency.
// Facts: docs/05-firmware-constraints.md
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace incott {

constexpr std::size_t kReportLen = 9;      // Report ID + 8 payload bytes, strictly
constexpr std::uint8_t kReportId = 0x09;
constexpr std::uint8_t kReadFlag = 0x80;   // read command is cmd | 0x80
constexpr std::size_t kMacroPayload = 320; // 10 chunks x 32 bytes
constexpr int kMacroChunkBytes = 32;
constexpr int kMacroChunks = 10;
constexpr int kMacroActionLimit = 71;      // official web cap; last item bit0=0 is the stop marker
constexpr int kMacroCycleLimit = 65535;    // 16-bit; macro_to_hw writes both bytes
constexpr int kMacroNameLimit = 32;
constexpr std::size_t kMacroFileMaxBytes = 128 * 1024;

enum Cmd : std::uint8_t {
    kCmdModel = 0x8F,
    kCmdStatus = 0x09,
    kCmdDpi = 0x02,
    kCmdDpiTable = 0x03,
    kCmdSensor = 0x04,
    kCmdMisc = 0x05,
    kCmdKeys = 0x06,
    kCmdMacro = 0x07,
    kCmdReceiverLed = 0x08,
};

struct Frame {
    std::uint8_t bytes[kReportLen];
};

// Build [09, cmd, p1..p7]; unused payload bytes are zero.
Frame MakeWrite(std::uint8_t cmd, const std::uint8_t* payload, std::size_t count);

// Build [09, cmd|0x80, p1..p7].
Frame MakeRead(std::uint8_t cmd, const std::uint8_t* payload, std::size_t count);

// True when the response echoes cmd (read replies echo cmd|0x80).
bool CheckEcho(const Frame& frame, std::uint8_t cmd);

// Battery percentage from a 0x89 status frame, or -1 when out of range.
int BatteryPercent(const Frame& status);

struct DeviceInfo {
    int psd = 0;
    int model = 0;      // 2 = G23, 3 = G24
    int receiver = 0;   // 2 = 8K receiver
    int sensor = 0;     // 3395 / 3950, 0 = unknown
};

// Parse a 0x8F reply. `wireless` picks payload[5] over payload[4] for sensor.
DeviceInfo ParseModel(const Frame& frame, bool wireless);
const wchar_t* ModelName(int model);
const wchar_t* ReceiverName(int receiver);
const wchar_t* SensorName(int sensor);

// Macro chunk header: [07, 10, chunk, 32, slot, 0, 0, 0].
Frame MakeMacroChunkHeader(int chunk, int slot);

// One event in a macro. The vendor recorder stores key-down and key-up separately.
struct MacroAction {
    bool press = false;
    int usage = 0;    // HID usage (0x04 = KeyA), 0..0xFF
    int delayMs = 0;  // pause after the event, 0..65535
};

// loopType mirrors the web UI: 1 = until key release, 2 = until any key, 3 = cycles.
enum class MacroError {
    kOk,
    kBufferRange,   // buffer must be 0..9
    kLoopType,      // 1..3
    kCyclesRange,   // 1..65535
    kActionCount,   // 1..71
    kActionUsage,   // 0..255
    kActionDelay,   // 0..65535
};

const char* MacroErrorText(MacroError error);

// Validates the parameters without touching memory (so callers can report the
// exact limit that was crossed, e.g. cycles 65536 rejected, never clamped).
MacroError ValidateMacro(int buffer, int loopType, int cycles,
                         const std::vector<MacroAction>& actions);

// Builds the 320-byte buffer the vendor macro_to_hw() produces:
//   [0]=buffer id, [1]=loop type (0/1/2), [2..3]=cycles LE16, then 4-byte
//   actions from offset 4 (bit0 set = more items follow, bit7 set = key up),
//   and the trailer at 0x130 (total size, uid, item count).
MacroError EncodeMacro(int buffer, int loopType, int cycles, std::uint64_t uid,
                       const std::vector<MacroAction>& actions,
                       std::uint8_t out[kMacroPayload]);

// DPI: the wire value is dpi/50 - 1 (little-endian), so 50 -> 0 and 32000 -> 639.
constexpr int kDpiMin = 50;
constexpr int kDpiMax = 32000;
constexpr int kDpiStep = 50;
constexpr int kDpiCountMax = 6;

// 50..32000 in 50 steps. Anything else is rejected, never rounded.
bool DpiValid(int dpi);
int DpiToHw(int dpi);
int DpiFromHw(int hw);

// Value from a 0x82 reply, or -1 on echo mismatch / slot mismatch (WP3: [2]
// echoes the requested slot, same stale-frame guard as the 0x84/0x85 sub reads)
// / unmapped wire value.
int DpiFromReply(const Frame& reply, int expect_slot);

struct DpiWrite {
    Frame frames[2];
    int count = 1;
};

// [02, slot, hwLo, hwHi, 0, 0, 0, axis], axis 0 = both, 1 = X, 2 = Y.
// Equal axes need one frame; different axes need 1 then 2 (100 ms apart).
DpiWrite MakeDpiWrite(int slot, int hw_x, int hw_y);

// WP3 measured shape (docs/05 §11): a 0x82 read returns the value of the LAST
// written 0x02 frame, whatever axis byte that frame carried, and read requests
// are axis-agnostic. MakeDpiWrite ends with the Y frame (axis=2) when the axes
// differ, so a split write can only verify Y; a single axis=0 frame stores the
// shared value. Returns the dpi value the post-write readback must equal.
int DpiExpectedReadback(const DpiWrite& write, int dpi_x, int dpi_y);

// WP3: dpi.stage args are rejected outright when invalid — never silently
// rewritten to 6/0. count 1..6, active a 0-based index below count.
bool DpiStageValid(int count, int active);

struct DpiTable {
    int count = -1;   // stored DPI levels, 1..6
    int active = -1;  // active level index
};

// 0x83 reply: [2] = level count, [3] = active level.
DpiTable ParseDpiTable(const Frame& reply);

struct StatusInfo {
    int battery = -1;     // percent, -1 out of range
    int charging = -1;
    int rate_code = -1;   // 0..6, see RateFromCode
    int dpi_index = -1;
};

// 0x89 reply: [2] battery, [3] charging, [4] rate code, [5] DPI index.
StatusInfo ParseStatus(const Frame& reply);

// 0 -> 1000, 1 -> 500, 2 -> 250, 3 -> 125, 4 -> 8000, 5 -> 4000, 6 -> 2000 Hz.
int RateFromCode(int code);

// WP5: >1000 Hz rates (2000/4000/8000) need the 8K receiver (receiverType==2)
// on the 2.4G dongle; anything else gets an explicit error, not a clamp.
bool HighRateAllowed(int receiver_type, bool wireless);

// WP5: misc.set bounds. Values outside are rejected, never clamped. The sleep
// cap is the measured firmware limit: 900 s is kept, 950/1000/1800 are refused
// with the old value intact (docs/05 §10); the official preset list also tops
// out at 900 s.
constexpr int kDebounceMsMax = 30;
constexpr int kSleepSecondsMin = 10;
constexpr int kSleepSecondsMax = 900;

// 0x85 sub-replies: sleep is LE16 seconds, debounce a single byte. Both return
// -1 unless [2] echoes the requested sub (WP5: a stale frame from another sub
// must never be parsed as this sub's value).
int SleepSeconds(const Frame& reply, int expect_sub);
int DebounceValue(const Frame& reply, int expect_sub);

// 0x84 sensor sub-replies: [2] echoes the requested sub, [3] carries the value.
// -1 on echo/sub mismatch (same stale-frame guard as 0x85).
int SensorSubValue(const Frame& reply, int expect_sub);

// Official sensor value ranges (vendor adapter.js): LOD and sensor mode are
// 0..2, the ripple/fixline/motion flags are 0..1. sensor.set rejects anything
// else — the firmware keeps out-of-range writes silent, so we must not send
// them and pretend they worked.
bool SensorValueValid(int sub, int value);

// 0x85 sub2 fire-key reply: [3] = loops (0..3), [4] = interval (0..255 ms).
int FireLoops(const Frame& reply);
int FireInterval(const Frame& reply);

// 8K wireless receiver LED (`0x08` write / `0x88` read): 0 = connection/rate,
// 1 = battery status, 2 = low battery warning only.
bool ReceiverLedValid(int mode);
int ParseReceiverLed(const Frame& reply);

// ---- Key bindings: 0x06 write, 0x86 read, one matrix index per frame --------
constexpr int kKeyCount = 6;

// UI order -> firmware matrix index. The two side buttons are swapped on the
// wire: "前侧键" is matrix 4 and "后侧键" is matrix 3.
extern const int kKeyMatrix[kKeyCount];

// ASCII slot name for JSON and logs: "left", "right", "middle", "forward",
// "back", "dpi" (index 0..5), nullptr out of range.
const char* KeySlotName(int index);

// [09, 86, matrix, 0, 0, 0, 0, 0, 0]
Frame MakeKeyRead(int matrix);

// [09, 06, matrix, code0, code1, code2, code3, 0, 0] (code little endian).
Frame MakeKeyWrite(int matrix, std::uint32_t code);

// True when the reply echoes 0x86 for `expect_matrix`; code = [3..6] LE32.
bool ParseKeyReply(const Frame& reply, int expect_matrix, std::uint32_t* code);

// The 108 names the vendor web driver accepts in macro files (golden/keys.json).
int KeyNameCount();
bool KeyUsageFromCode(const char* code, int* usage);
const char* KeyLabelFromCode(const char* code);  // display label, nullptr if unknown
const char* KeyCodeFromUsage(int usage);         // vkPOS code, nullptr if unknown

enum class KeyKind {
    kUnknown,
    kOff,
    kKeyboard,
    kMouse,
    kMedia,
    kMacro,
    kDpi,
    kProfile,
    kFire,
};

struct Binding {
    KeyKind kind = KeyKind::kUnknown;
    std::uint32_t code = 0;
    int usage = -1;       // keyboard HID usage (kKeyboard)
    int modifiers = 0;    // 1 Ctrl, 2 Shift, 4 Alt, 8 Win
    int mouse = -1;       // 1 left, 2 right, 3 middle, 4 forward, 5 back
    int media = -1;       // HID consumer-page usage
    int dpi = -1;         // 1 up, 2 down, 3 cycle
    int macro_slot = -1;
};

// Splits a 32-bit firmware key code into its fields (never guesses).
Binding DecodeBinding(std::uint32_t code);

// Accepts the 108 vkPOS codes, "LButton"/"RButton"/"MButton", "Mouse:*",
// "Dpi:Up|Down|Cycle", "Media:*", "Profile", "Fire", "Off", plus optional
// "Ctrl+"/"Shift+"/"Alt+"/"Win+" prefixes on keyboard keys. False when unknown.
bool EncodeBinding(const char* name, std::uint32_t* code);

// Short human label, e.g. "KeyA", "Ctrl+KeyC", "Mouse:Back", "Dpi:Cycle", "Off".
std::string DescribeBinding(std::uint32_t code);

// Stable lowercase tag for the bridge: "off", "keyboard", "mouse", "media",
// "macro", "dpi", "profile", "fire", "unknown".
const char* KeyKindName(KeyKind kind);

}  // namespace incott
