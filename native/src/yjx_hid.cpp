#include "yjx_hid.h"

#include <windows.h>
#include <setupapi.h>
#include <hidsdi.h>
#include <hidpi.h>

#include <vector>

namespace yjx {
namespace {

const GUID kHidClassGuid = {0x4D1E55B2, 0xF16F, 0x11CF,
                            {0x88, 0xCB, 0x00, 0x11, 0x11, 0x00, 0x00, 0x30}};

bool MatchesTarget(std::uint16_t vid, std::uint16_t pid, std::uint16_t preferred_vid) {
    if (pid != kPidX11) return false;
    if (preferred_vid != 0) return vid == preferred_vid;
    return IsYjxDevice(vid, pid);
}

}  // namespace

Device::~Device() {
    Close();
}

void Device::Close() {
    std::lock_guard<std::mutex> guard(lock_);
    if (handle_ != nullptr) {
        CloseHandle(static_cast<HANDLE>(handle_));
        handle_ = nullptr;
    }
    vendor_id_ = 0;
    product_id_ = 0;
}

bool Device::Open(std::uint16_t preferred_vid, std::string* error) {
    Close();

    HDEVINFO set = SetupDiGetClassDevsW(&kHidClassGuid, nullptr, nullptr,
                                        DIGCF_PRESENT | DIGCF_DEVICEINTERFACE);
    if (set == INVALID_HANDLE_VALUE) {
        if (error != nullptr) *error = "SetupDiGetClassDevs failed";
        return false;
    }

    HANDLE found_handle = nullptr;
    std::uint16_t found_vid = 0;
    std::uint16_t found_pid = 0;

    for (DWORD index = 0;; ++index) {
        SP_DEVICE_INTERFACE_DATA ifdata{};
        ifdata.cbSize = sizeof(ifdata);
        if (!SetupDiEnumDeviceInterfaces(set, nullptr, &kHidClassGuid, index, &ifdata)) {
            break;
        }
        DWORD required = 0;
        SetupDiGetDeviceInterfaceDetailW(set, &ifdata, nullptr, 0, &required, nullptr);
        if (required == 0) continue;

        std::vector<BYTE> buf(required);
        auto* detail = reinterpret_cast<PSP_DEVICE_INTERFACE_DETAIL_DATA_W>(buf.data());
        detail->cbSize = sizeof(SP_DEVICE_INTERFACE_DETAIL_DATA_W);
        if (!SetupDiGetDeviceInterfaceDetailW(set, &ifdata, detail, required, nullptr, nullptr)) {
            continue;
        }

        HANDLE h = CreateFileW(detail->DevicePath, GENERIC_READ | GENERIC_WRITE,
                               FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr, OPEN_EXISTING,
                               FILE_FLAG_OVERLAPPED, nullptr);
        if (h == INVALID_HANDLE_VALUE) continue;

        HIDD_ATTRIBUTES attr{};
        attr.Size = sizeof(attr);
        PHIDP_PREPARSED_DATA pp = nullptr;
        HIDP_CAPS caps{};
        const bool ok = HidD_GetAttributes(h, &attr) &&
                        MatchesTarget(attr.VendorID, attr.ProductID, preferred_vid) &&
                        HidD_GetPreparsedData(h, &pp) &&
                        HidP_GetCaps(pp, &caps) == HIDP_STATUS_SUCCESS;
        if (pp != nullptr) HidD_FreePreparsedData(pp);

        if (ok && caps.UsagePage == kUsagePage && caps.Usage == kUsage &&
            caps.OutputReportByteLength == kPacketLen &&
            caps.InputReportByteLength == kPacketLen) {
            found_handle = h;
            found_vid = attr.VendorID;
            found_pid = attr.ProductID;
            break;
        }
        CloseHandle(h);
    }

    SetupDiDestroyDeviceInfoList(set);

    if (found_handle == nullptr) {
        if (error != nullptr) *error = "device not found";
        return false;
    }

    std::lock_guard<std::mutex> guard(lock_);
    handle_ = found_handle;
    vendor_id_ = found_vid;
    product_id_ = found_pid;
    return true;
}

bool Device::WriteReport(const std::uint8_t* data, std::uint32_t len, std::uint32_t timeout_ms) {
    HANDLE h = static_cast<HANDLE>(handle_);
    if (h == nullptr) return false;
    HANDLE ev = CreateEventW(nullptr, TRUE, FALSE, nullptr);
    if (ev == nullptr) return false;

    OVERLAPPED ov{};
    ov.hEvent = ev;
    DWORD written = 0;
    BOOL ok = WriteFile(h, data, len, &written, &ov);
    if (!ok) {
        if (GetLastError() == ERROR_IO_PENDING) {
            if (WaitForSingleObject(ev, timeout_ms) == WAIT_OBJECT_0) {
                ok = GetOverlappedResult(h, &ov, &written, FALSE);
            } else {
                CancelIo(h);
                GetOverlappedResult(h, &ov, &written, TRUE);
                ok = FALSE;
            }
        }
    }
    CloseHandle(ev);
    return ok != FALSE && written == len;
}

bool Device::ReadReport(std::uint8_t* data, std::uint32_t len, std::uint32_t timeout_ms) {
    HANDLE h = static_cast<HANDLE>(handle_);
    if (h == nullptr) return false;
    HANDLE ev = CreateEventW(nullptr, TRUE, FALSE, nullptr);
    if (ev == nullptr) return false;

    OVERLAPPED ov{};
    ov.hEvent = ev;
    DWORD got = 0;
    BOOL ok = ReadFile(h, data, len, &got, &ov);
    if (!ok) {
        if (GetLastError() == ERROR_IO_PENDING) {
            if (WaitForSingleObject(ev, timeout_ms) == WAIT_OBJECT_0) {
                ok = GetOverlappedResult(h, &ov, &got, FALSE);
            } else {
                CancelIo(h);
                GetOverlappedResult(h, &ov, &got, TRUE);
                ok = FALSE;
            }
        }
    }
    CloseHandle(ev);
    return ok != FALSE && got == len;
}

bool Device::QueryReport(const std::uint8_t request[kPacketLen],
                         const std::function<bool(const std::uint8_t[kPacketLen])>& accept) {
    HANDLE h = static_cast<HANDLE>(handle_);
    if (h == nullptr) return false;
    HidD_FlushQueue(h);
    if (!WriteReport(request, static_cast<std::uint32_t>(kPacketLen), 250)) {
        return false;
    }
    const ULONGLONG deadline = GetTickCount64() + 400;
    for (int i = 0; i < 6; ++i) {
        const ULONGLONG now = GetTickCount64();
        if (now >= deadline) break;
        const DWORD wait_ms = static_cast<DWORD>(deadline - now);
        std::uint8_t reply[kPacketLen]{};
        if (!ReadReport(reply, static_cast<std::uint32_t>(kPacketLen), wait_ms)) {
            break;
        }
        if (accept(reply)) return true;
    }
    return false;
}

bool Device::ReadConfigUnlocked(Config* out, RawConfigFrame* raw_out, std::string* error) {
    if (handle_ == nullptr) {
        if (error != nullptr) *error = "device not open";
        return false;
    }
    std::uint8_t req[kPacketLen]{};
    BuildReadConfigPacket(req);

    for (int attempt = 0; attempt < 3; ++attempt) {
        bool ok = QueryReport(req, [&](const std::uint8_t reply[kPacketLen]) {
            return ParseConfigReply(reply, out, raw_out);
        });
        if (ok) return true;
        if (attempt + 1 < 3) Sleep(40);
    }

    if (error != nullptr) {
        *error = "failed to read 0x0E config (move the mouse to wake it)";
    }
    return false;
}

bool Device::ReadConfig(Config* out, RawConfigFrame* raw_out, std::string* error) {
    std::lock_guard<std::mutex> guard(lock_);
    return ReadConfigUnlocked(out, raw_out, error);
}

bool Device::WriteConfigVerified(const ConfigPatch& patch,
                                 Config* readback_out,
                                 std::string* error) {
    std::lock_guard<std::mutex> guard(lock_);
    if (handle_ == nullptr) {
        if (error != nullptr) *error = "device not open";
        return false;
    }

    RawConfigFrame base_raw;
    Config before;
    if (!ReadConfigUnlocked(&before, &base_raw, error)) {
        return false;
    }

    std::uint8_t write_req[kPacketLen]{};
    BuildWriteConfigPacket(base_raw, patch, write_req);

    HidD_FlushQueue(static_cast<HANDLE>(handle_));
    if (!WriteReport(write_req, static_cast<std::uint32_t>(kPacketLen), 250)) {
        if (error != nullptr) *error = "0x0F config write failed";
        return false;
    }
    // Drain an optional 0x0F ACK report without failing if the firmware stays silent.
    std::uint8_t ack[kPacketLen]{};
    ReadReport(ack, static_cast<std::uint32_t>(kPacketLen), 120);

    Sleep(60);

    Config after;
    bool read_any = false;
    for (int attempt = 0; attempt < 4; ++attempt) {
        std::string read_err;
        if (ReadConfigUnlocked(&after, nullptr, &read_err)) {
            read_any = true;
            if (ConfigMatchesPatch(after, patch)) {
                if (readback_out != nullptr) *readback_out = after;
                return true;
            }
        }
        if (attempt + 1 < 4) Sleep(50);
    }

    if (error != nullptr) {
        *error = read_any ? "readback mismatch after 0x0F config write"
                          : "0x0E readback failed after 0x0F write (move the mouse to wake it)";
    }
    return false;
}

bool Device::SetLightModeVerified(int mode, int* readback_out, std::string* error) {
    if (!LightModeValid(mode)) {
        if (error != nullptr) *error = "light mode must be 0..6";
        return false;
    }

    Config after_21;
    bool read_after_21 = false;
    {
        std::lock_guard<std::mutex> guard(lock_);
        if (handle_ == nullptr) {
            if (error != nullptr) *error = "device not open";
            return false;
        }
        std::uint8_t req[kPacketLen]{};
        BuildLightModePacket(static_cast<std::uint8_t>(mode), req);

        HidD_FlushQueue(static_cast<HANDLE>(handle_));
        if (!WriteReport(req, static_cast<std::uint32_t>(kPacketLen), 250)) {
            if (error != nullptr) *error = "0x21 light write failed";
            return false;
        }
        std::uint8_t ack[kPacketLen]{};
        ReadReport(ack, static_cast<std::uint32_t>(kPacketLen), 120);
        Sleep(60);

        for (int attempt = 0; attempt < 3; ++attempt) {
            if (ReadConfigUnlocked(&after_21, nullptr, nullptr)) {
                read_after_21 = true;
                if (after_21.light_mode == mode) {
                    if (readback_out != nullptr) *readback_out = after_21.light_mode;
                    return true;
                }
            }
            if (attempt + 1 < 3) Sleep(40);
        }
    }

    if (!read_after_21) {
        if (error != nullptr) {
            *error = "0x0E readback failed after 0x21 light write (move the mouse to wake it)";
        }
        return false;
    }

    // If 0x21 did not update 0x0E[9], persist via 0x0F read-modify-write so
    // on-board state and 0x0E readback remain consistent.
    ConfigPatch patch;
    patch.have_light_mode = true;
    patch.light_mode = mode;
    Config after_0f;
    if (!WriteConfigVerified(patch, &after_0f, error)) {
        return false;
    }
    if (readback_out != nullptr) *readback_out = after_0f.light_mode;
    return true;
}

bool Device::ReadKeysUnlocked(KeyCode out_keys[kKeyCount],
                              RawKeysFrame* raw_out,
                              std::string* error) {
    if (handle_ == nullptr) {
        if (error != nullptr) *error = "device not open";
        return false;
    }
    std::uint8_t req[kPacketLen]{};
    BuildReadKeysPacket(req);

    for (int attempt = 0; attempt < 3; ++attempt) {
        const bool ok = QueryReport(req, [&](const std::uint8_t reply[kPacketLen]) {
            return ParseKeysReply(reply, out_keys, raw_out);
        });
        if (ok) return true;
        if (attempt + 1 < 3) Sleep(40);
    }

    if (error != nullptr) {
        *error = "failed to read 0x08 key table (move the mouse to wake it)";
    }
    return false;
}

bool Device::ReadKeys(KeyCode out_keys[kKeyCount], RawKeysFrame* raw_out, std::string* error) {
    std::lock_guard<std::mutex> guard(lock_);
    return ReadKeysUnlocked(out_keys, raw_out, error);
}

bool Device::WriteKeyVerified(int index,
                              const KeyCode& value,
                              KeyCode* readback_out,
                              std::string* error) {
    if (index < 0 || index >= kKeyCount) {
        if (error != nullptr) *error = "key index must be 0..5";
        return false;
    }

    std::lock_guard<std::mutex> guard(lock_);
    if (handle_ == nullptr) {
        if (error != nullptr) *error = "device not open";
        return false;
    }

    KeyCode before[kKeyCount]{};
    RawKeysFrame base_raw;
    if (!ReadKeysUnlocked(before, &base_raw, error)) {
        return false;
    }

    std::uint8_t write_req[kPacketLen]{};
    BuildWriteKeysPacket(base_raw, index, value, write_req);

    HidD_FlushQueue(static_cast<HANDLE>(handle_));
    if (!WriteReport(write_req, static_cast<std::uint32_t>(kPacketLen), 250)) {
        if (error != nullptr) *error = "0x09 key write failed";
        return false;
    }
    std::uint8_t ack[kPacketLen]{};
    ReadReport(ack, static_cast<std::uint32_t>(kPacketLen), 120);
    Sleep(30);

    KeyCode after[kKeyCount]{};
    bool read_any = false;
    for (int attempt = 0; attempt < 4; ++attempt) {
        std::string read_err;
        if (ReadKeysUnlocked(after, nullptr, &read_err)) {
            read_any = true;
            if (after[index] == value) {
                if (readback_out != nullptr) *readback_out = after[index];
                return true;
            }
        }
        if (attempt + 1 < 4) Sleep(50);
    }

    if (error != nullptr) {
        *error = read_any ? "readback mismatch after 0x09 key write"
                          : "0x08 readback failed after 0x09 write (move the mouse to wake it)";
    }
    return false;
}

bool Device::UploadMacroTable(const std::vector<std::uint8_t>& table, std::string* error) {
    if (table.size() < kMacroHeaderBytes || table.size() > kMacroMaxBytes) {
        if (error != nullptr) *error = "macro table size must be 64..4096 bytes";
        return false;
    }

    std::lock_guard<std::mutex> guard(lock_);
    if (handle_ == nullptr) {
        if (error != nullptr) *error = "device not open";
        return false;
    }

    HANDLE h = static_cast<HANDLE>(handle_);
    HidD_FlushQueue(h);

    for (std::size_t off = 0; off < table.size(); off += kMacroChunkPayload) {
        const std::size_t rem = table.size() - off;
        const std::uint8_t len = static_cast<std::uint8_t>(
            rem > kMacroChunkPayload ? kMacroChunkPayload : rem);
        std::uint8_t chunk[kPacketLen]{};
        BuildMacroChunkPacket(static_cast<std::uint16_t>(off), table.data() + off, len, chunk);
        if (!WriteReport(chunk, static_cast<std::uint32_t>(kPacketLen), 250)) {
            if (error != nullptr) {
                *error = "0x0D macro chunk write failed at offset " + std::to_string(off);
            }
            return false;
        }
        std::uint8_t ack[kPacketLen]{};
        if (ReadReport(ack, static_cast<std::uint32_t>(kPacketLen), 60)) {
            Sleep(10);
        }
    }

    std::uint8_t commit[kPacketLen]{};
    BuildMacroCommitPacket(commit);
    if (!WriteReport(commit, static_cast<std::uint32_t>(kPacketLen), 250)) {
        if (error != nullptr) *error = "0x10 macro commit write failed";
        return false;
    }
    std::uint8_t ack[kPacketLen]{};
    ReadReport(ack, static_cast<std::uint32_t>(kPacketLen), 150);
    Sleep(40);
    return true;
}

}  // namespace yjx
