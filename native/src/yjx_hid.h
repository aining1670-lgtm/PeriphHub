#pragma once
// HID transport for YJX generic mouse protocol (VID 0xA8A4 / 0xA8A5, PID 0x2255,
// UsagePage 0xFF01, Usage 0x0010, 65-byte Interrupt OUT/IN reports).
#include <cstdint>
#include <functional>
#include <mutex>
#include <string>
#include <vector>

#include "yjx_protocol.h"

namespace yjx {

class Device {
public:
    Device() = default;
    ~Device();
    Device(const Device&) = delete;
    Device& operator=(const Device&) = delete;

    // Opens the YJX vendor collection. When `preferred_vid` is non-zero (e.g. 0xA8A4
    // or 0xA8A5), only that VID is matched; 0 matches either wired or 2.4G dongle.
    bool Open(std::uint16_t preferred_vid, std::string* error);
    void Close();

    bool IsOpen() const { return handle_ != nullptr; }
    std::uint16_t VendorId() const { return vendor_id_; }
    std::uint16_t ProductId() const { return product_id_; }
    bool IsWireless() const { return IsDongleVid(vendor_id_); }

    // Reads the 0x0E config frame (up to 3 retries).
    bool ReadConfig(Config* out, RawConfigFrame* raw_out, std::string* error);

    // Safe read-modify-write-verify flow:
    //   1. Reads full 0x0E frame (keeps all unknown/reserved bytes intact).
    //   2. Patches only requested fields into 0x0F and writes it.
    //   3. Waits 60 ms and reads back 0x0E up to 4 times to verify ConfigMatchesPatch.
    bool WriteConfigVerified(const ConfigPatch& patch,
                             Config* readback_out,
                             std::string* error);

    // Sends 0x21 light-mode command and reads back 0x0E to verify light_mode.
    // If the firmware keeps the previous 0x0E[9] byte after 0x21, falls back to
    // patching light_mode via WriteConfigVerified so readback & persistence agree.
    bool SetLightModeVerified(int mode, int* readback_out, std::string* error);

    // Phase 2: 0x08 read / 0x09 read-modify-write-verify for 6 mouse buttons.
    bool ReadKeys(KeyCode out_keys[kKeyCount], RawKeysFrame* raw_out, std::string* error);
    bool WriteKeyVerified(int index,
                          const KeyCode& value,
                          KeyCode* readback_out,
                          std::string* error);

    // Phase 2: 0x0D chunked upload + 0x10 commit for the 64..4096 byte macro table.
    bool UploadMacroTable(const std::vector<std::uint8_t>& table, std::string* error);

private:
    bool WriteReport(const std::uint8_t* data, std::uint32_t len, std::uint32_t timeout_ms);
    bool ReadReport(std::uint8_t* data, std::uint32_t len, std::uint32_t timeout_ms);
    bool QueryReport(const std::uint8_t request[kPacketLen],
                     const std::function<bool(const std::uint8_t[kPacketLen])>& accept);
    bool ReadConfigUnlocked(Config* out, RawConfigFrame* raw_out, std::string* error);
    bool ReadKeysUnlocked(KeyCode out_keys[kKeyCount], RawKeysFrame* raw_out, std::string* error);

    void* handle_ = nullptr;
    std::uint16_t vendor_id_ = 0;
    std::uint16_t product_id_ = 0;
    std::mutex lock_;
};

}  // namespace yjx
