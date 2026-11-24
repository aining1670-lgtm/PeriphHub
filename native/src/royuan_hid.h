#pragma once
// HID transport for ROYUAN (yc500) keyboard protocol (AKOS BC98: VID 0x3151,
// PID 0x4015 USB / 2.4G dongles, Device ID 0x03AE, UsagePage 0xFFFF / Usage 0x0002,
// 65-byte HID Feature Reports).
#include <cstdint>
#include <mutex>
#include <string>
#include <vector>

#include "royuan_protocol.h"

namespace royuan {

struct FullState {
    std::uint16_t dev_id = 0;
    std::string firmware;

    bool have_profile = false;
    int profile = 0;

    bool have_options = false;
    KbOptions options{};
    RawKbOptionsFrame raw_options{};

    bool have_main_led = false;
    LedParams main_led{};

    bool have_side_led = false;
    LedParams side_led{};

    bool have_debounce = false;
    int debounce = -1;

    bool have_sleep = false;
    SleepTimers sleep{};
};

struct StatePatch {
    bool have_profile = false;
    int profile = 0;

    bool have_options = false;
    KbOptionsPatch options{};

    bool have_main_led = false;
    LedPatch main_led{};

    bool have_side_led = false;
    LedPatch side_led{};

    bool have_debounce = false;
    int debounce = 1;

    bool have_sleep = false;
    SleepPatch sleep{};

    bool Any() const {
        return have_profile || (have_options && options.Any()) ||
               (have_main_led && main_led.Any()) || (have_side_led && side_led.Any()) ||
               have_debounce || (have_sleep && sleep.Any());
    }
};

struct MacroSlotSummary {
    int slot = 0;
    bool empty = true;
    int repeat = 0;
    int event_count = 0;
    int byte_length = 0;
};

class Device {
public:
    Device() = default;
    ~Device();
    Device(const Device&) = delete;
    Device& operator=(const Device&) = delete;

    // Opens the ROYUAN vendor collection (VID 0x3151, UsagePage 0xFFFF, Usage 0x0002).
    // When `preferred_pid` is non-zero, only that PID is matched.
    bool Open(std::uint16_t preferred_pid, std::string* error);
    void Close();

    bool IsOpen() const { return handle_ != nullptr; }
    std::uint16_t VendorId() const { return vendor_id_; }
    std::uint16_t ProductId() const { return product_id_; }
    bool IsWireless() const { return is_wireless_; }

    // Verifies via 0x8F that the connected keyboard is a supported AKOS BC98 (0x03AE).
    bool EnsureSupportedModel(std::uint16_t* dev_id_out, std::string* error);

    // Reads firmware revision + all keyboard state blocks (missing sub-reads stay marked false).
    bool ReadFullState(FullState* out, std::string* error);

    // Validates `patch`, verifies model 0x03AE, writes each patched block with
    // read-modify-write where required, and verifies every write via readback.
    bool ApplyPatchVerified(const StatePatch& patch, FullState* after_out, std::string* error);

    // Keymap (0x89 / 0x90 / 0x13 / 0x15 / 0x09 / 0x10)
    bool ReadKeymap(int profile,
                    KeyMatrix* base_out,
                    KeyMatrix* fn_out,
                    int* active_profile_out,
                    std::string* error);
    bool WriteKeySlotVerified(int profile,
                              bool fn_layer,
                              int slot,
                              const KeyEntry& entry,
                              KeyEntry* readback_out,
                              bool* bulk_used_out,
                              std::string* error);
    bool WriteKeymapLayerBulkVerified(int profile,
                                      bool fn_layer,
                                      const KeyMatrix& target,
                                      KeyMatrix* after_out,
                                      std::string* error);

    // Onboard Macros (0x8B / 0x16)
    bool ReadMacroSlot(int slot,
                       MacroData* out,
                       bool* empty_out,
                       std::uint8_t raw_out[kMacroBytes],
                       int* stream_bytes_out,
                       std::string* error);
    bool ListMacroSummaries(int count,
                            std::vector<MacroSlotSummary>* out,
                            std::string* error);
    bool WriteMacroSlotVerified(int slot,
                                const std::uint8_t target_blob[kMacroBytes],
                                MacroData* after_out,
                                bool* empty_out,
                                int* stream_bytes_out,
                                std::string* error);

private:
    bool RawSetFeature(const std::uint8_t data[kPacketLen]);
    bool RawGetFeature(std::uint8_t reply[kPacketLen]);
    bool ExchangeFeature(const std::uint8_t request[kPacketLen],
                         std::uint8_t reply[kPacketLen],
                         bool want_reply,
                         std::string* error);
    bool ExchangeRawPage(const std::uint8_t request[kPacketLen],
                         std::uint8_t page_out[kPayloadLen],
                         std::string* error);
    bool EnsureSupportedModelUnlocked(std::uint16_t* dev_id_out, std::string* error);
    bool ReadFirmwareUnlocked(std::string* version_out);
    bool ReadProfileUnlocked(int* profile_out, std::string* error);
    bool SwitchProfileUnlocked(int profile, std::string* error);
    bool ReadKeymapPageUnlocked(int profile,
                                int page,
                                bool fn_layer,
                                std::uint8_t page_out[kPayloadLen],
                                std::string* error);
    bool ReadKeymapLayerUnlocked(int profile,
                                 bool fn_layer,
                                 KeyMatrix* out,
                                 std::string* error);
    bool WriteKeymapLayerBulkUnlocked(int profile,
                                      bool fn_layer,
                                      const KeyMatrix& target,
                                      std::string* error);
    bool ReadMacroSlotRawUnlocked(int slot, std::uint8_t blob_out[kMacroBytes], std::string* error);
    bool ReadOptionsUnlocked(KbOptions* out, RawKbOptionsFrame* raw_out, std::string* error);
    bool ReadLedUnlocked(bool side, LedParams* out, std::string* error);
    bool ReadDebounceUnlocked(int* level_out, std::string* error);
    bool ReadSleepUnlocked(SleepTimers* out, std::string* error);
    bool ReadFullStateUnlocked(FullState* out, std::string* error);

    void* handle_ = nullptr;
    std::uint16_t vendor_id_ = 0;
    std::uint16_t product_id_ = 0;
    bool is_wireless_ = false;
    std::mutex lock_;
};

}  // namespace royuan
