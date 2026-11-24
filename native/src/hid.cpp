#include "hid.h"

#include "protocol.h"

#include <windows.h>
#include <setupapi.h>
#include <hidsdi.h>
#include <hidpi.h>

#include <vector>

namespace incott {
namespace {

// GUID_DEVINTERFACE_HID, spelled out so we do not depend on initguid.h.
const GUID kHidClassGuid = {0x4D1E55B2, 0xF16F, 0x11CF, {0x88, 0xCB, 0x00, 0x11, 0x11, 0x00, 0x00, 0x30}};

std::wstring LastErrorText(DWORD code) {
    wchar_t* buffer = nullptr;
    const DWORD length = FormatMessageW(
        FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
        nullptr, code, 0, reinterpret_cast<LPWSTR>(&buffer), 0, nullptr);
    std::wstring text = length != 0 ? std::wstring(buffer, length) : L"error " + std::to_wstring(code);
    if (buffer != nullptr) LocalFree(buffer);
    while (!text.empty() && (text.back() == L'\r' || text.back() == L'\n' || text.back() == L' ')) {
        text.pop_back();
    }
    return text;
}

HANDLE OpenPath(const std::wstring& path) {
    // Feature writes need write access; a read-only handle would make every
    // 0x8F handshake fail with ERROR_ACCESS_DENIED.
    return CreateFileW(path.c_str(), GENERIC_READ | GENERIC_WRITE,
                       FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr, OPEN_EXISTING, 0, nullptr);
}

// True only for the collection that accepts 9-byte feature reports. The G23
// exposes two FF05 collections; the first one has FeatureReportByteLength 0.
bool QueryCaps(HANDLE handle, HIDD_ATTRIBUTES* attr, HIDP_CAPS* caps) {
    attr->Size = sizeof(*attr);
    if (!HidD_GetAttributes(handle, attr)) return false;
    if (attr->VendorID != 0x093A || (attr->ProductID != 0x522C && attr->ProductID != 0x622C)) {
        return false;
    }

    PHIDP_PREPARSED_DATA preparsed = nullptr;
    if (!HidD_GetPreparsedData(handle, &preparsed)) return false;
    const NTSTATUS status = HidP_GetCaps(preparsed, caps);
    HidD_FreePreparsedData(preparsed);
    if (status != HIDP_STATUS_SUCCESS) return false;

    return caps->UsagePage == 0xFF05 && caps->Usage == 0x0001 &&
           caps->FeatureReportByteLength == kReportLen;
}

bool ProbeModel(HANDLE handle, std::wstring& why) {
    Frame request = MakeRead(kCmdModel, nullptr, 0);
    if (!HidD_SetFeature(handle, request.bytes, static_cast<ULONG>(kReportLen))) {
        why = L"HidD_SetFeature failed: " + LastErrorText(GetLastError());
        return false;
    }
    Sleep(30);
    Frame reply{};
    reply.bytes[0] = kReportId;  // HidD_GetFeature reads the report ID from the buffer
    if (!HidD_GetFeature(handle, reply.bytes, static_cast<ULONG>(kReportLen))) {
        why = L"HidD_GetFeature failed: " + LastErrorText(GetLastError());
        return false;
    }
    if (!CheckEcho(reply, kCmdModel)) {
        why = L"echo mismatch";
        return false;
    }
    if (reply.bytes[3] != 2 && reply.bytes[3] != 3) {
        why = L"unexpected model byte " + std::to_wstring(reply.bytes[3]);
        return false;
    }
    return true;
}

struct Candidate {
    std::wstring path;
    HANDLE handle = nullptr;
    int product_id = 0;
    std::size_t output_report_len = 0;
};

}  // namespace

Device::~Device() { Close(); }

bool Device::Open(std::wstring& error) {
    Close();

    HDEVINFO set = SetupDiGetClassDevsW(&kHidClassGuid, nullptr, nullptr,
                                        DIGCF_PRESENT | DIGCF_DEVICEINTERFACE);
    if (set == INVALID_HANDLE_VALUE) {
        error = L"SetupDiGetClassDevs failed: " + LastErrorText(GetLastError());
        return false;
    }

    std::vector<Candidate> candidates;
    for (DWORD index = 0;; ++index) {
        SP_DEVICE_INTERFACE_DATA interface_data{};
        interface_data.cbSize = sizeof(interface_data);
        if (!SetupDiEnumDeviceInterfaces(set, nullptr, &kHidClassGuid, index, &interface_data)) {
            break;  // ERROR_NO_MORE_ITEMS, or an unreadable interface we skip.
        }

        DWORD required = 0;
        SetupDiGetDeviceInterfaceDetailW(set, &interface_data, nullptr, 0, &required, nullptr);
        if (required == 0) continue;

        std::vector<BYTE> buffer(required);
        auto* detail = reinterpret_cast<PSP_DEVICE_INTERFACE_DETAIL_DATA_W>(buffer.data());
        detail->cbSize = sizeof(SP_DEVICE_INTERFACE_DETAIL_DATA_W);
        if (!SetupDiGetDeviceInterfaceDetailW(set, &interface_data, detail, required, nullptr, nullptr)) {
            continue;
        }

        const std::wstring path = detail->DevicePath;
        HANDLE handle = OpenPath(path);
        if (handle == INVALID_HANDLE_VALUE) continue;

        HIDD_ATTRIBUTES attr{};
        HIDP_CAPS caps{};
        if (!QueryCaps(handle, &attr, &caps)) {
            CloseHandle(handle);
            continue;
        }

        Candidate candidate;
        candidate.path = path;
        candidate.handle = handle;
        candidate.product_id = attr.ProductID;
        candidate.output_report_len = caps.OutputReportByteLength;
        candidates.push_back(candidate);
    }
    SetupDiDestroyDeviceInfoList(set);

    if (candidates.empty()) {
        error = L"device not found: VID_093A PID_522C/622C, UsagePage 0xFF05, 9-byte feature report";
        return false;
    }

    // Reverse order: the first FF05 collection rejects feature reports.
    std::wstring why;
    for (auto it = candidates.rbegin(); it != candidates.rend(); ++it) {
        if (ProbeModel(it->handle, why)) {
            handle_ = it->handle;
            it->handle = nullptr;
            path_ = it->path;
            product_id_ = it->product_id;
            output_report_len_ = it->output_report_len;
            for (auto& candidate : candidates) {
                if (candidate.handle != nullptr) {
                    CloseHandle(candidate.handle);
                    candidate.handle = nullptr;
                }
            }
            return true;
        }
    }

    for (auto& candidate : candidates) {
        if (candidate.handle != nullptr) {
            CloseHandle(candidate.handle);
            candidate.handle = nullptr;
        }
    }
    error = L"device found but 0x8F handshake failed: " + why;
    return false;
}

void Device::Close() {
    std::lock_guard<std::mutex> guard(lock_);
    if (handle_ != nullptr) {
        CloseHandle(static_cast<HANDLE>(handle_));
        handle_ = nullptr;
    }
    path_.clear();
    product_id_ = 0;
    output_report_len_ = 0;
}

bool Device::Feature(const std::uint8_t* in, std::uint8_t* out, std::wstring& error) {
    std::lock_guard<std::mutex> guard(lock_);
    if (handle_ == nullptr) {
        error = L"device not open";
        return false;
    }
    if (in == nullptr || out == nullptr) {
        error = L"null report buffer";
        return false;
    }

    HANDLE handle = static_cast<HANDLE>(handle_);
    if (!HidD_SetFeature(handle, const_cast<std::uint8_t*>(in), static_cast<ULONG>(kReportLen))) {
        error = L"HidD_SetFeature failed: " + LastErrorText(GetLastError());
        return false;
    }
    // The firmware swaps the report asynchronously; 25-30 ms is enough in practice.
    Sleep(30);
    out[0] = kReportId;  // HidD_GetFeature takes the report ID from the buffer
    if (!HidD_GetFeature(handle, out, static_cast<ULONG>(kReportLen))) {
        error = L"HidD_GetFeature failed: " + LastErrorText(GetLastError());
        return false;
    }
    return true;
}

bool Device::Output(const std::uint8_t* bytes, std::size_t count, std::wstring& error) {
    std::lock_guard<std::mutex> guard(lock_);
    if (handle_ == nullptr) {
        error = L"device not open";
        return false;
    }
    if (bytes == nullptr || count == 0) {
        error = L"empty output report";
        return false;
    }
    // docs/05-firmware-constraints.md section 1: a macro chunk is one output
    // report of exactly 33 bytes (report id 0x09 + 32 payload bytes), and the
    // device only accepts its own PnP output report length.
    const std::size_t expected = static_cast<std::size_t>(kMacroChunkBytes) + 1;
    if (count != expected || output_report_len_ != count) {
        error = L"unexpected output report size: " + std::to_wstring(count) +
                L" bytes, device output report length is " + std::to_wstring(output_report_len_) +
                L", expected " + std::to_wstring(expected);
        return false;
    }

    DWORD written = 0;
    if (!WriteFile(static_cast<HANDLE>(handle_), bytes, static_cast<DWORD>(count), &written, nullptr)) {
        error = L"WriteFile failed: " + LastErrorText(GetLastError());
        return false;
    }
    if (written != count) {
        error = L"short write: " + std::to_wstring(written) + L" of " + std::to_wstring(count);
        return false;
    }
    return true;
}

}  // namespace incott
