#pragma once
// HID transport. Everything that touches the device goes through here.
#include <cstddef>
#include <cstdint>
#include <mutex>
#include <string>

namespace incott {

class Device {
public:
    Device() = default;
    ~Device();
    Device(const Device&) = delete;
    Device& operator=(const Device&) = delete;

    // Enumerates collections in reverse order (the first FF05 collection on some
    // receivers rejects feature reports) and validates the model with 0x8F.
    bool Open(std::wstring& error);
    void Close();
    bool IsOpen() const { return handle_ != nullptr; }
    int ProductId() const { return product_id_; }
    bool IsWireless() const { return product_id_ == 0x522C; }

    // 9-byte feature report round trip.
    bool Feature(const std::uint8_t* in, std::uint8_t* out, std::wstring& error);

    // Macro data goes out as an interrupt OUT report; HidD_SetOutputReport is a
    // silent no-op on this firmware. `count` must be the report length the
    // device announced at Open time (33 = report ID + 32-byte macro chunk).
    bool Output(const std::uint8_t* bytes, std::size_t count, std::wstring& error);

private:
    void* handle_ = nullptr;
    std::wstring path_;
    int product_id_ = 0;
    // HIDP_CAPS.OutputReportByteLength captured in Open; 0 while closed.
    std::size_t output_report_len_ = 0;
    std::mutex lock_;
};

}  // namespace incott
