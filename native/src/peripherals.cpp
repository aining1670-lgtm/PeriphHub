#include "peripherals.h"

#include <windows.h>
#include <setupapi.h>
#include <cfgmgr32.h>
#include <devpropdef.h>
#include <hidsdi.h>
#include <hidpi.h>
#include <objbase.h>

#include <algorithm>
#include <cctype>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <map>
#include <string>
#include <thread>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "peripheral_merge.h"
#include "text.h"

namespace peripherals {
namespace {

constexpr ULONG kDnHasProblem = 0x00000400;
constexpr DWORD kUpstreamTimeoutMs = 7000;
constexpr DWORD kHeadsetControlTimeoutMs = 4000;
constexpr std::size_t kMaxProbeStdoutBytes = 262144;

const GUID kHidClassGuid = {0x4D1E55B2, 0xF16F, 0x11CF,
                            {0x88, 0xCB, 0x00, 0x11, 0x11, 0x00, 0x00, 0x30}};

constexpr DEVPROPKEY kPropDeviceDesc = {
    {0xa45c254e, 0xdf1c, 0x4efd, {0x80, 0x20, 0x67, 0xd1, 0x46, 0xa8, 0x50, 0xe0}}, 2};
constexpr DEVPROPKEY kPropFriendlyName = {
    {0xa45c254e, 0xdf1c, 0x4efd, {0x80, 0x20, 0x67, 0xd1, 0x46, 0xa8, 0x50, 0xe0}}, 14};
constexpr DEVPROPKEY kPropContainerId = {
    {0x8c7ed206, 0x3f8a, 0x4827, {0xb3, 0xab, 0xae, 0x9e, 0x1f, 0xae, 0xfc, 0x6c}}, 2};
constexpr DEVPROPKEY kPropContainerCategory = {
    {0x78c34fc8, 0x104a, 0x4aca, {0x9e, 0xa4, 0x52, 0x4d, 0x52, 0x99, 0x6e, 0x57}}, 90};
constexpr DEVPROPKEY kPropBtBattery = {
    {0x104EA319, 0x6EE2, 0x4701, {0xBD, 0x47, 0x8D, 0xDB, 0xF4, 0x25, 0xBB, 0xE5}}, 2};
constexpr DEVPROPKEY kPropIsConnected = {
    {0x83DA6326, 0x97A6, 0x4088, {0x94, 0x53, 0xA1, 0x92, 0x3F, 0x57, 0x3B, 0x29}}, 15};
constexpr DEVPROPKEY kPropClassOfDevice = {
    {0x2BD67D8B, 0x8BEB, 0x48D5, {0x87, 0xE0, 0x6C, 0xDA, 0x34, 0x28, 0x04, 0x0A}}, 10};

std::string Hex4(std::uint16_t value) {
    char buf[8]{};
    std::snprintf(buf, sizeof(buf), "%04X", static_cast<unsigned>(value));
    return buf;
}

std::wstring SelfExePath() {
    wchar_t path[MAX_PATH]{};
    const DWORD n = GetModuleFileNameW(nullptr, path, ARRAYSIZE(path));
    return n > 0 ? std::wstring(path, n) : std::wstring();
}

std::wstring ParentDir(const std::wstring& path) {
    const std::size_t slash = path.find_last_of(L"\\/");
    return slash == std::wstring::npos ? std::wstring() : path.substr(0, slash);
}

std::wstring FullPath(const std::wstring& path) {
    if (path.empty()) return {};
    wchar_t buf[MAX_PATH]{};
    const DWORD n = GetFullPathNameW(path.c_str(), ARRAYSIZE(buf), buf, nullptr);
    if (n > 0 && n < ARRAYSIZE(buf)) return std::wstring(buf, n);
    return path;
}

bool FileExists(const std::wstring& path) {
    if (path.empty()) return false;
    const DWORD attr = GetFileAttributesW(path.c_str());
    return attr != INVALID_FILE_ATTRIBUTES && (attr & FILE_ATTRIBUTE_DIRECTORY) == 0;
}

bool SamePath(const std::wstring& a, const std::wstring& b) {
    const std::wstring fa = FullPath(a);
    const std::wstring fb = FullPath(b);
    return !fa.empty() && _wcsicmp(fa.c_str(), fb.c_str()) == 0;
}

bool GetDevPropRaw(HDEVINFO set, SP_DEVINFO_DATA* dev, const DEVPROPKEY& key,
                   DEVPROPTYPE* prop_type, std::vector<BYTE>* out) {
    out->assign(512, 0);
    DWORD required = 0;
    if (SetupDiGetDevicePropertyW(set, dev, &key, prop_type, out->data(),
                                  static_cast<DWORD>(out->size()), &required, 0)) {
        if (required <= out->size()) out->resize(required);
        return true;
    }
    if (GetLastError() == ERROR_INSUFFICIENT_BUFFER && required > out->size() &&
        required <= 65536) {
        out->assign(required, 0);
        if (SetupDiGetDevicePropertyW(set, dev, &key, prop_type, out->data(),
                                      static_cast<DWORD>(out->size()), &required, 0)) {
            if (required <= out->size()) out->resize(required);
            return true;
        }
    }
    return false;
}

bool GetDevPropString(HDEVINFO set, SP_DEVINFO_DATA* dev, const DEVPROPKEY& key,
                      std::string* out) {
    DEVPROPTYPE type = 0;
    std::vector<BYTE> buf;
    if (!GetDevPropRaw(set, dev, key, &type, &buf)) return false;
    if (type != DEVPROP_TYPE_STRING && type != DEVPROP_TYPE_STRING_LIST) return false;
    const std::size_t wchar_count = buf.size() / sizeof(wchar_t);
    if (wchar_count == 0) return false;
    std::wstring wide(wchar_count, L'\0');
    std::memcpy(wide.data(), buf.data(), wchar_count * sizeof(wchar_t));
    const std::size_t nul = wide.find(L'\0');
    if (nul != std::wstring::npos) wide.resize(nul);
    if (wide.empty()) return false;
    *out = text::ToUtf8(wide);
    return !out->empty();
}

bool GetDevPropGuidString(HDEVINFO set, SP_DEVINFO_DATA* dev, const DEVPROPKEY& key,
                          std::string* out) {
    DEVPROPTYPE type = 0;
    std::vector<BYTE> buf;
    if (!GetDevPropRaw(set, dev, key, &type, &buf)) return false;
    if (type != DEVPROP_TYPE_GUID || buf.size() < sizeof(GUID)) return false;
    GUID guid{};
    std::memcpy(&guid, buf.data(), sizeof(GUID));
    wchar_t wbuf[64]{};
    if (StringFromGUID2(guid, wbuf, ARRAYSIZE(wbuf)) <= 0) return false;
    *out = text::ToUtf8(wbuf);
    return !out->empty();
}

bool GetDevPropBool(HDEVINFO set, SP_DEVINFO_DATA* dev, const DEVPROPKEY& key, bool* out) {
    DEVPROPTYPE type = 0;
    std::vector<BYTE> buf;
    if (!GetDevPropRaw(set, dev, key, &type, &buf)) return false;
    if (type != DEVPROP_TYPE_BOOLEAN || buf.empty()) return false;
    *out = (buf[0] != 0);
    return true;
}

bool GetDevPropByte(HDEVINFO set, SP_DEVINFO_DATA* dev, const DEVPROPKEY& key,
                    std::uint8_t* out) {
    DEVPROPTYPE type = 0;
    std::vector<BYTE> buf;
    if (!GetDevPropRaw(set, dev, key, &type, &buf)) return false;
    if (type != DEVPROP_TYPE_BYTE || buf.empty()) return false;
    *out = buf[0];
    return true;
}

bool GetDevPropUint32(HDEVINFO set, SP_DEVINFO_DATA* dev, const DEVPROPKEY& key,
                      std::uint32_t* out) {
    DEVPROPTYPE type = 0;
    std::vector<BYTE> buf;
    if (!GetDevPropRaw(set, dev, key, &type, &buf)) return false;
    if (type != DEVPROP_TYPE_UINT32 || buf.size() < sizeof(std::uint32_t)) return false;
    std::memcpy(out, buf.data(), sizeof(std::uint32_t));
    return true;
}

std::string StripBtServiceSuffix(std::string name) {
    static constexpr const char* kSuffixes[] = {
        " Hands-Free AG", " Hands-Free", " Avrcp Transport", " Stereo", " Audio Gateway"};
    for (const char* suf : kSuffixes) {
        const std::size_t slen = std::strlen(suf);
        if (name.size() > slen &&
            name.compare(name.size() - slen, slen, suf) == 0) {
            name.resize(name.size() - slen);
            break;
        }
    }
    return name;
}

std::string KindFromClassOfDevice(std::uint32_t cod) {
    const std::uint32_t major = (cod >> 8) & 0x1Fu;
    const std::uint32_t minor = (cod >> 2) & 0x3Fu;
    if (major == 0x02u) return "phone";
    if (major == 0x04u) return "headset";
    if (major == 0x05u) {
        if ((minor & 0x0Fu) == 0x01u || (minor & 0x0Fu) == 0x02u) return "gamepad";
        if ((minor >> 4) == 1u) return "keyboard";
        if ((minor >> 4) == 2u) return "mouse";
    }
    return "other";
}

std::string KindFromCategory(const std::string& cat) {
    if (cat == "Communication.Phone" || cat.rfind("Communication.Phone.", 0) == 0) {
        return "phone";
    }
    if (cat == "Input.Keyboard") return "keyboard";
    if (cat == "Input.Mouse") return "mouse";
    if (cat == "Input.Gaming") return "gamepad";
    if (cat.rfind("Communication.Headset", 0) == 0 || cat == "Audio.Headphones") {
        return "headset";
    }
    return "other";
}

bool ContainerOfHidInterface(const std::wstring& interface_path, std::string* container_id) {
    HDEVINFO set = SetupDiCreateDeviceInfoList(nullptr, nullptr);
    if (set == INVALID_HANDLE_VALUE) return false;

    bool found = false;
    SP_DEVICE_INTERFACE_DATA ifdata{};
    ifdata.cbSize = sizeof(ifdata);
    if (SetupDiOpenDeviceInterfaceW(set, interface_path.c_str(), 0, &ifdata)) {
        DWORD required = 0;
        SetupDiGetDeviceInterfaceDetailW(set, &ifdata, nullptr, 0, &required, nullptr);
        if (required > 0) {
            std::vector<BYTE> buffer(required);
            auto* detail = reinterpret_cast<PSP_DEVICE_INTERFACE_DETAIL_DATA_W>(buffer.data());
            detail->cbSize = sizeof(SP_DEVICE_INTERFACE_DETAIL_DATA_W);
            SP_DEVINFO_DATA devdata{};
            devdata.cbSize = sizeof(devdata);
            if (SetupDiGetDeviceInterfaceDetailW(set, &ifdata, detail, required, nullptr,
                                                 &devdata)) {
                found = GetDevPropGuidString(set, &devdata, kPropContainerId, container_id);
            }
        }
    }
    SetupDiDestroyDeviceInfoList(set);
    return found;
}

struct HidCollectionInfo {
    std::wstring path;
    std::uint16_t vid = 0;
    std::uint16_t pid = 0;
    std::uint16_t usage_page = 0;
    std::uint16_t usage = 0;
    std::uint16_t input_len = 0;
    std::uint16_t feature_len = 0;
    std::uint16_t input_value_caps = 0;
    std::uint16_t feature_value_caps = 0;
};

bool IsBatteryUsage(USAGE page, USAGE usage) {
    return (page == 0x06u && usage == 0x20u) || (page == 0x85u && usage == 0x65u);
}

int ClampPct(long val) {
    if (val < 0) return 0;
    if (val > 100) return 100;
    return static_cast<int>(val);
}

bool ReadHidBattery(const HidCollectionInfo& c, int* percent_out, std::string* name_out) {
    HANDLE handle = CreateFileW(c.path.c_str(), 0, FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr,
                                OPEN_EXISTING, 0, nullptr);
    if (handle == INVALID_HANDLE_VALUE) return false;

    PHIDP_PREPARSED_DATA pp = nullptr;
    if (!HidD_GetPreparsedData(handle, &pp)) {
        CloseHandle(handle);
        return false;
    }

    bool found = false;
    int pct = -1;
    struct ReportPass {
        HIDP_REPORT_TYPE type;
        USHORT cap_count;
        USHORT report_len;
    };
    const ReportPass passes[2] = {
        {HidP_Feature, c.feature_value_caps, c.feature_len},
        {HidP_Input, c.input_value_caps, c.input_len},
    };

    for (const ReportPass& pass : passes) {
        if (found) break;
        if (pass.cap_count == 0 || pass.report_len == 0) continue;

        USHORT n = pass.cap_count;
        std::vector<HIDP_VALUE_CAPS> caps(n);
        if (HidP_GetValueCaps(pass.type, caps.data(), &n, pp) != HIDP_STATUS_SUCCESS) {
            continue;
        }
        for (USHORT i = 0; i < n; ++i) {
            const HIDP_VALUE_CAPS& v = caps[i];
            const USAGE u = v.IsRange ? v.Range.UsageMin : v.NotRange.Usage;
            if (!IsBatteryUsage(v.UsagePage, u)) continue;

            std::vector<std::uint8_t> buf(pass.report_len, 0);
            buf[0] = v.ReportID;
            const BOOLEAN got =
                (pass.type == HidP_Feature)
                    ? HidD_GetFeature(handle, buf.data(), static_cast<ULONG>(buf.size()))
                    : HidD_GetInputReport(handle, buf.data(), static_cast<ULONG>(buf.size()));
            if (!got) continue;

            ULONG raw_val = 0;
            if (HidP_GetUsageValue(pass.type, v.UsagePage, v.LinkCollection, u, &raw_val, pp,
                                   reinterpret_cast<PCHAR>(buf.data()),
                                   static_cast<ULONG>(buf.size())) != HIDP_STATUS_SUCCESS) {
                continue;
            }
            if (c.vid == 0x05ACu && raw_val <= 100u) {
                pct = static_cast<int>(raw_val);
            } else if (v.LogicalMax > v.LogicalMin) {
                const long long num =
                    (static_cast<long long>(raw_val) - v.LogicalMin) * 100LL;
                const long long den = static_cast<long long>(v.LogicalMax) - v.LogicalMin;
                pct = ClampPct(static_cast<long>(num / den));
            } else if (raw_val <= 100u) {
                pct = static_cast<int>(raw_val);
            } else {
                continue;
            }
            found = true;
            break;
        }
    }

    if (found && name_out != nullptr) {
        wchar_t prod[128]{};
        if (HidD_GetProductString(handle, prod, sizeof(prod))) {
            prod[ARRAYSIZE(prod) - 1] = L'\0';
            *name_out = text::ToUtf8(prod);
        }
    }

    HidD_FreePreparsedData(pp);
    CloseHandle(handle);
    if (found && percent_out != nullptr) *percent_out = pct;
    return found;
}

bool QueryRoyuanFeature(HANDLE handle, const std::uint8_t request[kRoyuanPacketLen],
                        std::uint8_t reply[kRoyuanPacketLen]) {
    std::uint8_t out_buf[kRoyuanPacketLen]{};
    std::memcpy(out_buf, request, kRoyuanPacketLen);
    if (!HidD_SetFeature(handle, out_buf, static_cast<ULONG>(kRoyuanPacketLen))) {
        return false;
    }
    Sleep(35);
    std::memset(reply, 0, kRoyuanPacketLen);
    return HidD_GetFeature(handle, reply, static_cast<ULONG>(kRoyuanPacketLen)) != 0;
}

bool QueryRoyuanRelayedFeature(HANDLE handle, const std::uint8_t request[kRoyuanPacketLen],
                               std::uint8_t reply[kRoyuanPacketLen]) {
    std::uint8_t f7_req[kRoyuanPacketLen]{};
    BuildRoyuanDongleStatusPacket(f7_req);

    bool relay_ready = false;
    for (int attempt = 0; attempt < 4; ++attempt) {
        std::uint8_t f7_rep[kRoyuanPacketLen]{};
        if (QueryRoyuanFeature(handle, f7_req, f7_rep)) {
            const std::uint8_t* p = f7_rep + 1;
            if ((p[6] & 0x01u) == 0u || p[3] != 0u) return false;
            if (p[5] == 1u) {
                relay_ready = true;
                break;
            }
        }
        Sleep(40);
    }
    if (!relay_ready) return false;

    std::uint8_t sel_req[kRoyuanPacketLen]{};
    sel_req[0] = 0x00;
    sel_req[1] = 0xF6;
    sel_req[2] = 10;  // keyboard relay target
    if (!HidD_SetFeature(handle, sel_req, static_cast<ULONG>(kRoyuanPacketLen))) {
        return false;
    }
    Sleep(20);

    std::uint8_t out_buf[kRoyuanPacketLen]{};
    std::memcpy(out_buf, request, kRoyuanPacketLen);
    if (!HidD_SetFeature(handle, out_buf, static_cast<ULONG>(kRoyuanPacketLen))) {
        return false;
    }
    Sleep(35);

    bool reply_ready = false;
    for (int attempt = 0; attempt < 5; ++attempt) {
        std::uint8_t f7_rep[kRoyuanPacketLen]{};
        if (QueryRoyuanFeature(handle, f7_req, f7_rep) && f7_rep[1] == 1u) {
            reply_ready = true;
            break;
        }
        Sleep(45);
    }
    if (!reply_ready) return false;

    std::uint8_t rel_req[kRoyuanPacketLen]{};
    rel_req[0] = 0x00;
    rel_req[1] = 0xFC;
    if (!HidD_SetFeature(handle, rel_req, static_cast<ULONG>(kRoyuanPacketLen))) {
        return false;
    }
    Sleep(20);
    std::memset(reply, 0, kRoyuanPacketLen);
    return HidD_GetFeature(handle, reply, static_cast<ULONG>(kRoyuanPacketLen)) != 0;
}

bool WriteOverlappedReport(HANDLE handle, const std::uint8_t* data, DWORD len, DWORD timeout_ms) {
    HANDLE ev = CreateEventW(nullptr, TRUE, FALSE, nullptr);
    if (ev == nullptr) return false;
    OVERLAPPED ov{};
    ov.hEvent = ev;
    DWORD written = 0;
    BOOL ok = WriteFile(handle, data, len, &written, &ov);
    if (!ok) {
        if (GetLastError() == ERROR_IO_PENDING) {
            if (WaitForSingleObject(ev, timeout_ms) == WAIT_OBJECT_0) {
                ok = GetOverlappedResult(handle, &ov, &written, FALSE);
            } else {
                CancelIo(handle);
                GetOverlappedResult(handle, &ov, &written, TRUE);
                ok = FALSE;
            }
        }
    }
    CloseHandle(ev);
    return ok != FALSE && written == len;
}

bool ReadOverlappedReport(HANDLE handle, std::uint8_t* data, DWORD len, DWORD timeout_ms) {
    HANDLE ev = CreateEventW(nullptr, TRUE, FALSE, nullptr);
    if (ev == nullptr) return false;
    OVERLAPPED ov{};
    ov.hEvent = ev;
    DWORD got = 0;
    BOOL ok = ReadFile(handle, data, len, &got, &ov);
    if (!ok) {
        if (GetLastError() == ERROR_IO_PENDING) {
            if (WaitForSingleObject(ev, timeout_ms) == WAIT_OBJECT_0) {
                ok = GetOverlappedResult(handle, &ov, &got, FALSE);
            } else {
                CancelIo(handle);
                GetOverlappedResult(handle, &ov, &got, TRUE);
                ok = FALSE;
            }
        }
    }
    CloseHandle(ev);
    return ok != FALSE && got == len;
}

bool QueryKsnakeReport(HANDLE handle, const std::uint8_t request[kKsnakePacketLen],
                       const std::function<bool(const std::uint8_t[kKsnakePacketLen])>& accept) {
    HidD_FlushQueue(handle);
    if (!WriteOverlappedReport(handle, request, static_cast<DWORD>(kKsnakePacketLen), 250)) {
        return false;
    }
    const ULONGLONG deadline = GetTickCount64() + 400;
    for (int i = 0; i < 4; ++i) {
        const ULONGLONG now = GetTickCount64();
        if (now >= deadline) break;
        const DWORD wait_ms = static_cast<DWORD>(deadline - now);
        std::uint8_t reply[kKsnakePacketLen]{};
        if (!ReadOverlappedReport(handle, reply, static_cast<DWORD>(kKsnakePacketLen), wait_ms)) {
            break;
        }
        if (accept(reply)) return true;
    }
    return false;
}

std::wstring FindUpstreamProbeExe() {
    const std::wstring self = SelfExePath();

    wchar_t env_buf[MAX_PATH]{};
    const DWORD env_len = GetEnvironmentVariableW(L"PERIPHHUB_PROBE_EXE", env_buf, MAX_PATH);
    if (env_len > 0 && env_len < MAX_PATH) {
        const std::wstring env_val(env_buf, env_len);
        if (_wcsicmp(env_val.c_str(), L"0") == 0 ||
            _wcsicmp(env_val.c_str(), L"none") == 0 ||
            _wcsicmp(env_val.c_str(), L"off") == 0) {
            return {};
        }
        if (FileExists(env_val) && !SamePath(env_val, self)) {
            return FullPath(env_val);
        }
        return {};
    }

    const std::wstring dir = ParentDir(self);
    if (dir.empty()) return {};

    const std::wstring cand = dir + L"\\battery-probe.exe";
    if (FileExists(cand) && !SamePath(cand, self)) {
        return FullPath(cand);
    }
    return {};
}

std::wstring FindHeadsetControlExe() {
    const std::wstring dir = ParentDir(SelfExePath());
    if (dir.empty()) return {};
    const std::wstring cand = dir + L"\\headsetcontrol.exe";
    if (FileExists(cand)) return FullPath(cand);
    return {};
}

bool RunProcessCaptureStdout(const std::wstring& exe_path, const std::wstring& args,
                             DWORD timeout_ms, std::string* stdout_out) {
    if (stdout_out == nullptr || exe_path.empty()) return false;
    stdout_out->clear();

    SECURITY_ATTRIBUTES sa{};
    sa.nLength = sizeof(sa);
    sa.bInheritHandle = TRUE;

    HANDLE read_pipe = nullptr;
    HANDLE write_pipe = nullptr;
    if (!CreatePipe(&read_pipe, &write_pipe, &sa, 0)) return false;
    if (!SetHandleInformation(read_pipe, HANDLE_FLAG_INHERIT, 0)) {
        CloseHandle(read_pipe);
        CloseHandle(write_pipe);
        return false;
    }

    HANDLE nul_in = CreateFileW(L"NUL", GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, &sa,
                                OPEN_EXISTING, 0, nullptr);
    HANDLE nul_err = CreateFileW(L"NUL", GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, &sa,
                                 OPEN_EXISTING, 0, nullptr);

    HANDLE job = CreateJobObjectW(nullptr, nullptr);
    if (job != nullptr) {
        JOBOBJECT_EXTENDED_LIMIT_INFORMATION jeli{};
        jeli.BasicLimitInformation.LimitFlags = JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE;
        if (!SetInformationJobObject(job, JobObjectExtendedLimitInformation, &jeli, sizeof(jeli))) {
            CloseHandle(job);
            job = nullptr;
        }
    }

    HANDLE inherit_handles[3]{};
    DWORD inherit_count = 0;
    inherit_handles[inherit_count++] = write_pipe;
    if (nul_in != INVALID_HANDLE_VALUE) inherit_handles[inherit_count++] = nul_in;
    if (nul_err != INVALID_HANDLE_VALUE) inherit_handles[inherit_count++] = nul_err;

    std::vector<BYTE> attr_buf;
    LPPROC_THREAD_ATTRIBUTE_LIST attr_list = nullptr;
    SIZE_T attr_size = 0;
    InitializeProcThreadAttributeList(nullptr, 1, 0, &attr_size);
    if (attr_size > 0) {
        attr_buf.resize(attr_size);
        attr_list = reinterpret_cast<LPPROC_THREAD_ATTRIBUTE_LIST>(attr_buf.data());
        if (!InitializeProcThreadAttributeList(attr_list, 1, 0, &attr_size)) {
            attr_list = nullptr;
        } else if (!UpdateProcThreadAttribute(attr_list, 0, PROC_THREAD_ATTRIBUTE_HANDLE_LIST,
                                              inherit_handles,
                                              inherit_count * sizeof(HANDLE), nullptr, nullptr)) {
            DeleteProcThreadAttributeList(attr_list);
            attr_list = nullptr;
        }
    }

    STARTUPINFOEXW siex{};
    siex.StartupInfo.cb = (attr_list != nullptr) ? sizeof(STARTUPINFOEXW) : sizeof(STARTUPINFOW);
    siex.StartupInfo.dwFlags = STARTF_USESTDHANDLES;
    siex.StartupInfo.hStdInput = (nul_in != INVALID_HANDLE_VALUE) ? nul_in : GetStdHandle(STD_INPUT_HANDLE);
    siex.StartupInfo.hStdOutput = write_pipe;
    siex.StartupInfo.hStdError = (nul_err != INVALID_HANDLE_VALUE) ? nul_err : write_pipe;
    siex.lpAttributeList = attr_list;

    std::wstring cmdline = L"\"" + exe_path + L"\"";
    if (!args.empty()) {
        cmdline += L" ";
        cmdline += args;
    }
    std::wstring cwd = ParentDir(exe_path);

    DWORD creation_flags = CREATE_NO_WINDOW | CREATE_SUSPENDED;
    if (attr_list != nullptr) creation_flags |= EXTENDED_STARTUPINFO_PRESENT;

    PROCESS_INFORMATION pi{};
    const BOOL ok = CreateProcessW(
        exe_path.c_str(), cmdline.data(), nullptr, nullptr, TRUE, creation_flags, nullptr,
        cwd.empty() ? nullptr : cwd.c_str(), &siex.StartupInfo, &pi);

    if (attr_list != nullptr) DeleteProcThreadAttributeList(attr_list);
    CloseHandle(write_pipe);
    if (nul_in != INVALID_HANDLE_VALUE) CloseHandle(nul_in);
    if (nul_err != INVALID_HANDLE_VALUE) CloseHandle(nul_err);

    if (!ok) {
        if (job != nullptr) CloseHandle(job);
        CloseHandle(read_pipe);
        return false;
    }

    if (job != nullptr && !AssignProcessToJobObject(job, pi.hProcess)) {
        CloseHandle(job);
        job = nullptr;
    }
    ResumeThread(pi.hThread);

    std::string captured;
    std::thread reader([read_pipe, &captured]() {
        char buf[4096];
        DWORD got = 0;
        while (ReadFile(read_pipe, buf, sizeof(buf), &got, nullptr) && got > 0) {
            if (captured.size() < kMaxProbeStdoutBytes) {
                const std::size_t take =
                    std::min<std::size_t>(got, kMaxProbeStdoutBytes - captured.size());
                captured.append(buf, take);
            }
        }
    });

    const DWORD wait_rc = WaitForSingleObject(pi.hProcess, timeout_ms);
    const bool finished = (wait_rc == WAIT_OBJECT_0);
    if (!finished) {
        if (job != nullptr) {
            TerminateJobObject(job, 1);
            CloseHandle(job);
            job = nullptr;
        }
        TerminateProcess(pi.hProcess, 1);
        WaitForSingleObject(pi.hProcess, 1000);
    }

    reader.join();
    if (job != nullptr) CloseHandle(job);
    CloseHandle(read_pipe);
    CloseHandle(pi.hThread);
    CloseHandle(pi.hProcess);

    if (!finished) return false;
    *stdout_out = std::move(captured);
    return !stdout_out->empty();
}

}  // namespace

std::vector<bridge::DeviceSnapshot> ScanRoyuanKeyboards() {
    HDEVINFO set = SetupDiGetClassDevsW(&kHidClassGuid, nullptr, nullptr,
                                        DIGCF_PRESENT | DIGCF_DEVICEINTERFACE);
    if (set == INVALID_HANDLE_VALUE) return {};

    std::vector<bridge::DeviceSnapshot> out;
    std::unordered_set<std::uint16_t> seen_pids;

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

        HANDLE handle = CreateFileW(detail->DevicePath, GENERIC_READ | GENERIC_WRITE,
                                    FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr, OPEN_EXISTING, 0,
                                    nullptr);
        if (handle == INVALID_HANDLE_VALUE) continue;

        HIDD_ATTRIBUTES attr{};
        attr.Size = sizeof(attr);
        PHIDP_PREPARSED_DATA pp = nullptr;
        HIDP_CAPS caps{};
        const bool ok = HidD_GetAttributes(handle, &attr) && attr.VendorID == 0x3151u &&
                        HidD_GetPreparsedData(handle, &pp) &&
                        HidP_GetCaps(pp, &caps) == HIDP_STATUS_SUCCESS;
        if (pp != nullptr) HidD_FreePreparsedData(pp);

        if (!ok || caps.UsagePage != 0xFFFFu || caps.Usage != 0x0002u ||
            caps.FeatureReportByteLength != kRoyuanPacketLen ||
            seen_pids.count(attr.ProductID)) {
            CloseHandle(handle);
            continue;
        }

        std::string name;
        wchar_t prod[128]{};
        if (HidD_GetProductString(handle, prod, sizeof(prod))) {
            prod[ARRAYSIZE(prod) - 1] = L'\0';
            name = text::ToUtf8(prod);
        }
        if (name.empty()) name = "ROYUAN Keyboard";

        bridge::DeviceSnapshot snap;
        snap.key = "royuan:3151:" + Hex4(attr.ProductID);
        snap.name = std::move(name);
        snap.kind = "keyboard";
        snap.driver_level = "battery";

        if (IsRoyuanDonglePid(attr.ProductID)) {
            std::uint8_t req[kRoyuanPacketLen]{};
            BuildRoyuanDongleStatusPacket(req);
            bool online = false;
            int battery = -1;
            bool parsed = false;
            for (int attempt = 0; attempt < 2 && !parsed; ++attempt) {
                std::uint8_t reply[kRoyuanPacketLen]{};
                if (QueryRoyuanFeature(handle, req, reply) &&
                    ParseRoyuanDongleStatus(reply, &online, &battery)) {
                    parsed = true;
                } else if (attempt == 0) {
                    Sleep(25);
                }
            }
            if (!parsed) {
                CloseHandle(handle);
                continue;
            }

            std::uint16_t dev_id = 0;
            std::string fw_ver;
            if (online) {
                std::uint8_t id_req[kRoyuanPacketLen]{};
                BuildRoyuanGetDevIdPacket(id_req);
                std::uint8_t id_rep[kRoyuanPacketLen]{};
                if (QueryRoyuanRelayedFeature(handle, id_req, id_rep)) {
                    ParseRoyuanDevIdReply(id_rep, &dev_id);
                }

                std::uint8_t rev_req[kRoyuanPacketLen]{};
                BuildRoyuanGetRevPacket(rev_req);
                std::uint8_t rev_rep[kRoyuanPacketLen]{};
                if (QueryRoyuanRelayedFeature(handle, rev_req, rev_rep)) {
                    ParseRoyuanFirmwareReply(rev_rep, &fw_ver);
                }
            }
            CloseHandle(handle);

            const bool is_bc98 =
                IsAkosBc98Board(dev_id) ||
                (!online && (attr.ProductID == 0x4014u || IsRoyuanBtHint("", snap.name)));
            if (is_bc98) {
                snap.name = ResolveRoyuanModelName(0x03AEu, snap.name);
                snap.driver_level = "full";
                snap.sensor = fw_ver.empty()
                                  ? "ROYUAN yc500"
                                  : ("\xE5\x9B\xBA\xE4\xBB\xB6 " + fw_ver);
            }

            snap.via = "2.4 GHz receiver";
            snap.mode = "wireless";
            snap.connected = online;
            snap.asleep = !online;
            snap.battery = battery;
            snap.charging = (online && battery >= 0) ? 0 : -1;
            if (!online) {
                snap.detail = "\x32.4G \xE9\x94\xAE\xE7\x9B\x98\xE6\x9C\xAA\xE5\x94\xA4\xE9\x86\x92\xE6\x88\x96\xE5\xB7\xB2\xE5\x85\xB3\xE9\x97\xAD";
            }
        } else {
            std::uint8_t id_req[kRoyuanPacketLen]{};
            BuildRoyuanGetDevIdPacket(id_req);
            std::uint16_t dev_id = 0;
            for (int attempt = 0; attempt < 2 && dev_id == 0u; ++attempt) {
                std::uint8_t reply[kRoyuanPacketLen]{};
                if (QueryRoyuanFeature(handle, id_req, reply)) {
                    ParseRoyuanDevIdReply(reply, &dev_id);
                }
                if (dev_id == 0u && attempt == 0) Sleep(25);
            }

            std::uint8_t req[kRoyuanPacketLen]{};
            BuildRoyuanGetRevPacket(req);
            std::string fw_ver;
            for (int attempt = 0; attempt < 2 && fw_ver.empty(); ++attempt) {
                std::uint8_t reply[kRoyuanPacketLen]{};
                if (QueryRoyuanFeature(handle, req, reply)) {
                    ParseRoyuanFirmwareReply(reply, &fw_ver);
                }
                if (fw_ver.empty() && attempt == 0) Sleep(25);
            }
            CloseHandle(handle);

            if (IsAkosBc98Board(dev_id)) {
                snap.name = ResolveRoyuanModelName(dev_id, snap.name);
                snap.driver_level = "full";
                snap.sensor = fw_ver.empty()
                                  ? "ROYUAN yc500"
                                  : ("\xE5\x9B\xBA\xE4\xBB\xB6 " + fw_ver);
            }

            snap.via = "USB";
            snap.mode = "wired";
            snap.connected = true;
            snap.battery = -1;
            snap.charging = -1;
            snap.detail = fw_ver.empty()
                              ? "USB \xE6\x9C\x89\xE7\xBA\xBF\xE7\x9B\xB4\xE8\xBF\x9E\xEF\xBC\x88\xE5\x9B\xBA\xE4\xBB\xB6\xE4\xB8\x8D\xE4\xB8\x8A\xE6\x8A\xA5\xE7\x94\xB5\xE9\x87\x8F\xEF\xBC\x89"
                              : ("\xE5\x9B\xBA\xE4\xBB\xB6 " + fw_ver +
                                 " \xC2\xB7 \xE6\x9C\x89\xE7\xBA\xBF\xE6\xA8\xA1\xE5\xBC\x8F\xE4\xB8\x8D\xE4\xB8\x8A\xE6\x8A\xA5\xE7\x94\xB5\xE9\x87\x8F");
        }

        seen_pids.insert(attr.ProductID);
        out.push_back(std::move(snap));
    }
    SetupDiDestroyDeviceInfoList(set);
    return out;
}

std::vector<bridge::DeviceSnapshot> ScanKsnakeMice() {
    HDEVINFO set = SetupDiGetClassDevsW(&kHidClassGuid, nullptr, nullptr,
                                        DIGCF_PRESENT | DIGCF_DEVICEINTERFACE);
    if (set == INVALID_HANDLE_VALUE) return {};

    std::vector<bridge::DeviceSnapshot> out;
    std::unordered_set<std::uint32_t> seen_vp;

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

        HANDLE handle = CreateFileW(detail->DevicePath, GENERIC_READ | GENERIC_WRITE,
                                    FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr, OPEN_EXISTING,
                                    FILE_FLAG_OVERLAPPED, nullptr);
        if (handle == INVALID_HANDLE_VALUE) continue;

        HIDD_ATTRIBUTES attr{};
        attr.Size = sizeof(attr);
        PHIDP_PREPARSED_DATA pp = nullptr;
        HIDP_CAPS caps{};
        const bool ok = HidD_GetAttributes(handle, &attr) &&
                        IsKsnakeX11Device(attr.VendorID, attr.ProductID) &&
                        HidD_GetPreparsedData(handle, &pp) &&
                        HidP_GetCaps(pp, &caps) == HIDP_STATUS_SUCCESS;
        if (pp != nullptr) HidD_FreePreparsedData(pp);

        const std::uint32_t vp = (static_cast<std::uint32_t>(attr.VendorID) << 16) | attr.ProductID;
        if (!ok || caps.UsagePage != 0xFF01u || caps.Usage != 0x0010u ||
            caps.OutputReportByteLength != kKsnakePacketLen ||
            caps.InputReportByteLength != kKsnakePacketLen || seen_vp.count(vp)) {
            CloseHandle(handle);
            continue;
        }

        std::string name;
        wchar_t prod[128]{};
        if (HidD_GetProductString(handle, prod, sizeof(prod))) {
            prod[ARRAYSIZE(prod) - 1] = L'\0';
            name = text::ToUtf8(prod);
        }
        std::string lower_name = name;
        for (char& ch : lower_name) {
            ch = static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));
        }
        if (name.empty() || lower_name.find("receiver") != std::string::npos ||
            lower_name == "usb mouse" || lower_name == "gaming mouse") {
            name = "Wireless Mouse";
        }

        bridge::DeviceSnapshot snap;
        snap.key = "ksnake:" + Hex4(attr.VendorID) + ":" + Hex4(attr.ProductID);
        snap.name = std::move(name);
        snap.kind = "mouse";
        snap.driver_level = "full";

        std::uint8_t bat_req[kKsnakePacketLen]{};
        BuildKsnakeBatteryPacket(bat_req);

        int battery = -1;
        int charging = -1;
        bool bat_ok = false;

        if (IsKsnakeDongleVid(attr.VendorID)) {
            std::uint8_t online_req[kKsnakePacketLen]{};
            BuildKsnakeOnlinePacket(online_req);

            bool online = false;
            bool online_ok = false;
            for (int attempt = 0; attempt < 2 && !online_ok; ++attempt) {
                online_ok = QueryKsnakeReport(handle, online_req, [&](const std::uint8_t r[kKsnakePacketLen]) {
                    return ParseKsnakeOnlineReply(r, &online);
                });
                if (!online_ok && attempt == 0) Sleep(25);
            }

            for (int attempt = 0; attempt < 2 && !bat_ok; ++attempt) {
                bat_ok = QueryKsnakeReport(handle, bat_req, [&](const std::uint8_t r[kKsnakePacketLen]) {
                    return ParseKsnakeBatteryReply(r, &battery, &charging);
                });
                if (!bat_ok && attempt == 0) Sleep(25);
            }
            CloseHandle(handle);

            if (!online_ok && !bat_ok) continue;

            snap.via = "2.4 GHz receiver";
            snap.mode = "wireless";
            const bool is_charging = (bat_ok && charging == 2);
            const bool is_connected = is_charging || (online_ok ? online : bat_ok);
            snap.connected = is_connected;
            if (is_connected && bat_ok) {
                snap.battery = battery;
                snap.charging = charging;
            } else {
                snap.battery = -1;
                snap.charging = -1;
                if (!is_connected) {
                    snap.detail = "\x32.4G \xE9\xBC\xA0\xE6\xA0\x87\xE6\x9C\xAA\xE5\x94\xA4\xE9\x86\x92\xE6\x88\x96\xE5\xB7\xB2\xE5\x85\xB3\xE9\x97\xAD";
                }
            }
        } else {
            for (int attempt = 0; attempt < 2 && !bat_ok; ++attempt) {
                bat_ok = QueryKsnakeReport(handle, bat_req, [&](const std::uint8_t r[kKsnakePacketLen]) {
                    return ParseKsnakeBatteryReply(r, &battery, &charging);
                });
                if (!bat_ok && attempt == 0) Sleep(25);
            }
            CloseHandle(handle);

            snap.via = "USB";
            snap.mode = "wired";
            snap.connected = true;
            snap.battery = bat_ok ? battery : -1;
            snap.charging = bat_ok ? charging : -1;
        }

        seen_vp.insert(vp);
        out.push_back(std::move(snap));
    }
    SetupDiDestroyDeviceInfoList(set);
    return out;
}

std::vector<bridge::DeviceSnapshot> ScanBluetoothDevices(
    std::unordered_set<std::string>* bt_containers) {
    struct BtBatteryEntry {
        int percent = -1;
        std::string name;
        bool connected = false;
    };

    std::unordered_map<std::string, std::string> container_name;
    std::unordered_map<std::string, std::uint32_t> container_cod;
    std::unordered_map<std::string, std::string> container_cat;
    std::unordered_map<std::string, bool> container_conn;
    std::unordered_map<std::string, bool> container_has_ble_hid;
    std::unordered_map<std::string, bool> container_is_yjx_bt;
    std::unordered_map<std::string, bool> container_is_royuan_bt;
    std::unordered_map<std::string, BtBatteryEntry> batteries;
    std::vector<std::string> order;
    std::unordered_set<std::string> seen_order;

    auto add_to_order = [&](const std::string& cid) {
        if (!cid.empty() && seen_order.insert(cid).second) {
            order.push_back(cid);
        }
    };

    auto contains_icase = [](const std::string& hay, const char* needle) {
        std::string lower;
        lower.reserve(hay.size());
        for (unsigned char ch : hay) {
            lower.push_back(static_cast<char>(std::tolower(ch)));
        }
        return lower.find(needle) != std::string::npos;
    };

    static constexpr const wchar_t* kEnumerators[] = {L"BTHENUM", L"BTHLE", L"BTHLEDEVICE"};
    for (const wchar_t* enumerator : kEnumerators) {
        HDEVINFO set = SetupDiGetClassDevsW(nullptr, enumerator, nullptr,
                                            DIGCF_ALLCLASSES | DIGCF_PRESENT);
        if (set == INVALID_HANDLE_VALUE) continue;

        for (DWORD i = 0;; ++i) {
            SP_DEVINFO_DATA dev{};
            dev.cbSize = sizeof(dev);
            if (!SetupDiEnumDeviceInfo(set, i, &dev)) break;

            wchar_t inst_buf[512]{};
            if (!SetupDiGetDeviceInstanceIdW(set, &dev, inst_buf, ARRAYSIZE(inst_buf), nullptr)) {
                continue;
            }
            const std::string inst = text::ToUtf8(inst_buf);
            if (inst.rfind("BTHENUM\\", 0) != 0 && inst.rfind("BTHLE\\", 0) != 0 &&
                inst.rfind("BTHLEDEVICE\\", 0) != 0) {
                continue;
            }

            std::string cid;
            if (!GetDevPropGuidString(set, &dev, kPropContainerId, &cid)) {
                cid = inst;
            }

            std::string name;
            if (!GetDevPropString(set, &dev, kPropFriendlyName, &name)) {
                GetDevPropString(set, &dev, kPropDeviceDesc, &name);
            }

            if (contains_icase(inst, "{00001812-0000-1000-8000-00805f9b34fb}")) {
                container_has_ble_hid[cid] = true;
            }
            if (contains_icase(inst, "vid&012717_pid&32b0") ||
                contains_icase(inst, "vid&022717_pid&32b0") ||
                contains_icase(inst, "vid_2717&pid_32b0") ||
                contains_icase(name, "x11bt")) {
                container_is_yjx_bt[cid] = true;
            }
            if (IsRoyuanBtHint(inst, name)) {
                container_is_royuan_bt[cid] = true;
            }

            ULONG status = 0;
            ULONG problem = 0;
            const bool has_problem =
                (CM_Get_DevNode_Status(&status, &problem, dev.DevInst, 0) == CR_SUCCESS &&
                 (status & kDnHasProblem) != 0);

            if (inst.rfind("BTHENUM\\DEV_", 0) == 0 || inst.rfind("BTHLE\\DEV_", 0) == 0) {
                if (!name.empty()) container_name[cid] = name;
                std::uint32_t cod = 0;
                if (GetDevPropUint32(set, &dev, kPropClassOfDevice, &cod)) {
                    container_cod[cid] = cod;
                }
                std::string cat;
                if (GetDevPropString(set, &dev, kPropContainerCategory, &cat)) {
                    container_cat[cid] = cat;
                }
                bool conn = false;
                if (GetDevPropBool(set, &dev, kPropIsConnected, &conn)) {
                    container_conn[cid] = container_conn[cid] || conn;
                }
                if (!has_problem) {
                    add_to_order(cid);
                }
            }

            std::uint8_t pct = 0;
            if (!GetDevPropByte(set, &dev, kPropBtBattery, &pct) || pct > 100) {
                continue;
            }
            if (has_problem) {
                continue;
            }

            bool node_conn = true;
            GetDevPropBool(set, &dev, kPropIsConnected, &node_conn);

            auto existing = batteries.find(cid);
            if (existing != batteries.end() && existing->second.connected && !node_conn) {
                continue;
            }
            add_to_order(cid);
            batteries[cid] = BtBatteryEntry{static_cast<int>(pct),
                                            StripBtServiceSuffix(name), node_conn};
        }
        SetupDiDestroyDeviceInfoList(set);
    }

    std::vector<bridge::DeviceSnapshot> out;
    for (const std::string& cid : order) {
        const auto bat_it = batteries.find(cid);
        const bool has_bat = (bat_it != batteries.end());
        const auto conn_it = container_conn.find(cid);
        const bool connected = (conn_it != container_conn.end())
                                   ? conn_it->second
                                   : (has_bat ? bat_it->second.connected : false);
        if (!connected) continue;

        std::string dev_name;
        const auto name_it = container_name.find(cid);
        if (name_it != container_name.end() && !name_it->second.empty()) {
            dev_name = name_it->second;
        } else if (has_bat && !bat_it->second.name.empty()) {
            dev_name = bat_it->second.name;
        } else {
            dev_name = "Bluetooth device";
        }

        const bool is_yjx_bt = container_is_yjx_bt[cid] || contains_icase(dev_name, "x11bt");
        const bool is_royuan_bt = container_is_royuan_bt[cid] || IsRoyuanBtHint("", dev_name);
        if (is_royuan_bt) {
            dev_name = NormalizeRoyuanBtName(dev_name);
        }
        std::string kind = is_yjx_bt ? "mouse" : (is_royuan_bt ? "keyboard" : "other");
        if (kind == "other") {
            const auto cod_it = container_cod.find(cid);
            if (cod_it != container_cod.end()) {
                kind = KindFromClassOfDevice(cod_it->second);
            }
        }
        if (kind == "other") {
            const auto cat_it = container_cat.find(cid);
            if (cat_it != container_cat.end()) {
                kind = KindFromCategory(cat_it->second);
            }
        }
        kind = NormalizeKind(kind, dev_name);

        // If the Bluetooth device has no 0x180F Battery Service (kPropBtBattery),
        // only include it when it is a recognized input/audio peripheral or BLE HID device.
        if (!has_bat && kind == "other" && !container_has_ble_hid[cid] && !is_yjx_bt &&
            !is_royuan_bt) {
            continue;
        }

        bridge::DeviceSnapshot snap;
        snap.key = "bt:" + cid;
        snap.name = dev_name;
        snap.kind = kind;
        snap.via = "Bluetooth";
        snap.mode = "bluetooth";
        snap.driver_level = "battery";
        snap.battery = has_bat ? bat_it->second.percent : -1;
        snap.charging = -1;
        snap.connected = true;
        if (is_royuan_bt) {
            snap.sensor = has_bat
                              ? "ROYUAN \xC2\xB7 \xE8\x93\x9D\xE7\x89\x99 BLE"
                              : "ROYUAN \xC2\xB7 \xE8\x93\x9D\xE7\x89\x99 HID";
            snap.detail =
                has_bat
                    ? "\xE8\x93\x9D\xE7\x89\x99\xE6\xA8\xA1\xE5\xBC\x8F\xE5\xB7\xB2\xE8\xBF\x9E\xE6\x8E\xA5"
                      "\xEF\xBC\x88\xE9\x85\x8D\xE7\xBD\xAE RGB \xE7\x81\xAF\xE6\x95\x88\xE4\xB8\x8E\xE6\x9D\xBF"
                      "\xE8\xBD\xBD\xE5\x8F\x82\xE6\x95\xB0\xE8\xAF\xB7\xE5\x88\x87\xE6\x8D\xA2\xE8\x87\xB3 "
                      "USB \xE6\x9C\x89\xE7\xBA\xBF\xE6\x88\x96 2.4G \xE6\x8E\xA5\xE6\x94\xB6\xE5\x99\xA8\xEF\xBC\x89"
                    : "\xE8\x93\x9D\xE7\x89\x99\xE6\xA8\xA1\xE5\xBC\x8F\xE5\xB7\xB2\xE8\xBF\x9E\xE6\x8E\xA5"
                      "\xEF\xBC\x88\xE5\x9B\xBA\xE4\xBB\xB6\xE6\x9C\xAA\xE6\x8F\x90\xE4\xBE\x9B\xE8\x93\x9D\xE7\x89\x99"
                      "\xE7\x94\xB5\xE9\x87\x8F\xE6\x9C\x8D\xE5\x8A\xA1\xEF\xBC\x8C\xE9\x85\x8D\xE7\xBD\xAE\xE7\x81\xAF"
                      "\xE5\x85\x89\xE4\xB8\x8E\xE5\x8F\x82\xE6\x95\xB0\xE8\xAF\xB7\xE5\x88\x87\xE6\x8D\xA2\xE8\x87\xB3 "
                      "USB \xE6\x9C\x89\xE7\xBA\xBF\xE6\x88\x96 2.4G \xE6\x8E\xA5\xE6\x94\xB6\xE5\x99\xA8\xEF\xBC\x89";
        } else if (!has_bat) {
            if (is_yjx_bt) {
                snap.sensor = "PAW3311 \xC2\xB7 \xE8\x93\x9D\xE7\x89\x99 HID";
                snap.detail =
                    "\xE8\x93\x9D\xE7\x89\x99\xE6\xA8\xA1\xE5\xBC\x8F\xE5\xB7\xB2\xE8\xBF\x9E\xE6\x8E\xA5"
                    "\xEF\xBC\x88\xE5\x9B\xBA\xE4\xBB\xB6\xE8\x93\x9D\xE7\x89\x99\xE4\xB8\x8D\xE4\xB8\x8A"
                    "\xE6\x8A\xA5\xE7\x94\xB5\xE9\x87\x8F\xEF\xBC\x8C\xE6\x9F\xA5\xE7\x9C\x8B\xE7\x94\xB5"
                    "\xE9\x87\x8F\xE6\x88\x96\xE9\x85\x8D\xE7\xBD\xAE\xE9\xA9\xB1\xE5\x8A\xA8\xE8\xAF\xB7"
                    "\xE7\x94\xA8 2.4G \xE6\x8E\xA5\xE6\x94\xB6\xE5\x99\xA8\xE6\x88\x96 USB \xE6\x9C\x89\xE7\xBA\xBF\xEF\xBC\x89";
            } else {
                snap.detail =
                    "\xE8\x93\x9D\xE7\x89\x99\xE5\xB7\xB2\xE8\xBF\x9E\xE6\x8E\xA5"
                    "\xEF\xBC\x88\xE8\xAF\xA5\xE8\xAE\xBE\xE5\xA4\x87\xE5\x9B\xBA\xE4\xBB\xB6\xE6\x9C\xAA"
                    "\xE6\x8F\x90\xE4\xBE\x9B\xE8\x93\x9D\xE7\x89\x99\xE7\x94\xB5\xE9\x87\x8F\xE6\x9C\x8D\xE5\x8A\xA1\xEF\xBC\x89";
            }
        }

        if (bt_containers != nullptr) {
            bt_containers->insert(cid);
        }
        out.push_back(std::move(snap));
    }
    return out;
}

std::vector<bridge::DeviceSnapshot> ScanGenericHidDevices(
    const std::unordered_set<std::string>& skip_containers) {
    HDEVINFO set = SetupDiGetClassDevsW(&kHidClassGuid, nullptr, nullptr,
                                        DIGCF_PRESENT | DIGCF_DEVICEINTERFACE);
    if (set == INVALID_HANDLE_VALUE) return {};

    std::vector<HidCollectionInfo> colls;
    std::unordered_set<std::uint32_t> digitizer_vid_pids;
    std::map<std::uint32_t, std::string> vid_pid_kinds;

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

        const std::wstring path = detail->DevicePath;
        HANDLE handle = CreateFileW(path.c_str(), 0, FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr,
                                    OPEN_EXISTING, 0, nullptr);
        if (handle == INVALID_HANDLE_VALUE) continue;

        HIDD_ATTRIBUTES attr{};
        attr.Size = sizeof(attr);
        PHIDP_PREPARSED_DATA pp = nullptr;
        HIDP_CAPS caps{};
        const bool ok = HidD_GetAttributes(handle, &attr) &&
                        HidD_GetPreparsedData(handle, &pp) &&
                        HidP_GetCaps(pp, &caps) == HIDP_STATUS_SUCCESS;
        if (pp != nullptr) HidD_FreePreparsedData(pp);
        CloseHandle(handle);
        if (!ok) continue;

        const std::uint32_t vp =
            (static_cast<std::uint32_t>(attr.VendorID) << 16) | attr.ProductID;
        if (caps.UsagePage == 0x0Du) {
            digitizer_vid_pids.insert(vp);
        }
        if (caps.UsagePage == 0x01u) {
            if (caps.Usage == 0x02u) vid_pid_kinds[vp] = "mouse";
            else if (caps.Usage == 0x06u) vid_pid_kinds[vp] = "keyboard";
            else if (caps.Usage == 0x04u || caps.Usage == 0x05u) vid_pid_kinds[vp] = "gamepad";
        }

        HidCollectionInfo info;
        info.path = path;
        info.vid = attr.VendorID;
        info.pid = attr.ProductID;
        info.usage_page = caps.UsagePage;
        info.usage = caps.Usage;
        info.input_len = caps.InputReportByteLength;
        info.feature_len = caps.FeatureReportByteLength;
        info.input_value_caps = caps.NumberInputValueCaps;
        info.feature_value_caps = caps.NumberFeatureValueCaps;
        colls.push_back(std::move(info));
    }
    SetupDiDestroyDeviceInfoList(set);

    std::vector<bridge::DeviceSnapshot> out;
    std::unordered_set<std::uint32_t> seen_vp;

    for (const HidCollectionInfo& c : colls) {
        if (c.vid == 0x093Au && (c.pid == 0x522Cu || c.pid == 0x622Cu)) continue;
        if (c.vid == 0x3151u) continue;
        if (IsKsnakeX11Device(c.vid, c.pid)) continue;
        if (c.usage_page == 0x84u || c.usage_page == 0x85u) continue;
        if (c.input_value_caps == 0 && c.feature_value_caps == 0) continue;

        const std::uint32_t vp = (static_cast<std::uint32_t>(c.vid) << 16) | c.pid;
        if (digitizer_vid_pids.count(vp) || seen_vp.count(vp)) continue;

        if (!skip_containers.empty()) {
            std::string container;
            if (ContainerOfHidInterface(c.path, &container) && skip_containers.count(container)) {
                continue;
            }
        }

        int pct = -1;
        std::string name;
        if (!ReadHidBattery(c, &pct, &name)) continue;
        if (name.empty()) name = "Wireless device";

        std::string kind = "other";
        const auto kind_it = vid_pid_kinds.find(vp);
        if (kind_it != vid_pid_kinds.end()) kind = kind_it->second;

        bridge::DeviceSnapshot snap;
        snap.key = "hid:" + Hex4(c.vid) + ":" + Hex4(c.pid);
        snap.name = std::move(name);
        snap.kind = NormalizeKind(kind, snap.name);
        snap.via = "HID";
        snap.mode = "wireless";
        snap.driver_level = "battery";
        snap.battery = pct;
        snap.charging = -1;
        snap.connected = true;

        seen_vp.insert(vp);
        out.push_back(std::move(snap));
    }
    return out;
}

std::vector<bridge::DeviceSnapshot> ScanXInputDevices() {
    struct XInputBatteryInfo {
        BYTE BatteryType;
        BYTE BatteryLevel;
    };
    using GetBatteryFn = DWORD(WINAPI*)(DWORD, BYTE, XInputBatteryInfo*);

    HMODULE mod = LoadLibraryExW(L"xinput1_4.dll", nullptr, LOAD_LIBRARY_SEARCH_SYSTEM32);
    if (mod == nullptr) mod = LoadLibraryExW(L"xinput1_3.dll", nullptr, LOAD_LIBRARY_SEARCH_SYSTEM32);
    if (mod == nullptr) mod = LoadLibraryExW(L"xinput9_1_0.dll", nullptr, LOAD_LIBRARY_SEARCH_SYSTEM32);
    if (mod == nullptr) return {};

    FARPROC proc = GetProcAddress(mod, "XInputGetBatteryInformation");
    auto fn = reinterpret_cast<GetBatteryFn>(reinterpret_cast<void*>(proc));
    if (fn == nullptr) {
        FreeLibrary(mod);
        return {};
    }

    std::vector<bridge::DeviceSnapshot> out;
    constexpr BYTE kBatteryDevTypeGamepad = 0x00;
    constexpr BYTE kBatteryTypeAlkaline = 0x02;
    constexpr BYTE kBatteryTypeNimh = 0x03;

    for (DWORD idx = 0; idx < 4; ++idx) {
        XInputBatteryInfo info{};
        if (fn(idx, kBatteryDevTypeGamepad, &info) != ERROR_SUCCESS) continue;
        if (info.BatteryType != kBatteryTypeAlkaline && info.BatteryType != kBatteryTypeNimh) {
            continue;
        }

        int pct = 5;
        std::string level = "Empty";
        switch (info.BatteryLevel) {
            case 0: pct = 5; level = "Empty"; break;
            case 1: pct = 25; level = "Low"; break;
            case 2: pct = 60; level = "Medium"; break;
            case 3: pct = 100; level = "Full"; break;
            default: pct = -1; level.clear(); break;
        }

        bridge::DeviceSnapshot snap;
        snap.key = "xinput:" + std::to_string(idx);
        snap.name = "Xbox Controller #" + std::to_string(idx + 1);
        snap.kind = "gamepad";
        snap.via = "XInput";
        snap.mode = "wireless";
        snap.driver_level = "battery";
        snap.battery = pct;
        snap.level = std::move(level);
        snap.charging = -1;
        snap.connected = true;
        out.push_back(std::move(snap));
    }

    FreeLibrary(mod);
    return out;
}

bool RunUpstreamProbe(std::vector<bridge::DeviceSnapshot>* out) {
    if (out == nullptr) return false;
    out->clear();
    const std::wstring exe = FindUpstreamProbeExe();
    if (exe.empty()) return false;

    std::string stdout_utf8;
    if (!RunProcessCaptureStdout(exe, L"--json", kUpstreamTimeoutMs, &stdout_utf8)) {
        return false;
    }
    std::string error;
    if (!ParseUpstreamJson(stdout_utf8, out, &error)) {
        out->clear();
        return false;
    }
    return true;
}

bool RunHeadsetControl(std::vector<bridge::DeviceSnapshot>* out) {
    if (out == nullptr) return false;
    out->clear();
    const std::wstring exe = FindHeadsetControlExe();
    if (exe.empty()) return false;

    std::string stdout_utf8;
    if (!RunProcessCaptureStdout(exe, L"-o json", kHeadsetControlTimeoutMs, &stdout_utf8)) {
        return false;
    }
    std::string error;
    if (!ParseHeadsetControlJson(stdout_utf8, out, &error)) {
        out->clear();
        return false;
    }
    return true;
}

std::vector<bridge::DeviceSnapshot> ScanAllDevices(
    const std::function<bool(bridge::DeviceSnapshot*)>& probe_incott) {
    std::vector<bridge::DeviceSnapshot> upstream;
    const bool upstream_ok = RunUpstreamProbe(&upstream);

    std::vector<bridge::DeviceSnapshot> native_incott;
    if (probe_incott) {
        bridge::DeviceSnapshot incott_snap;
        if (probe_incott(&incott_snap)) {
            native_incott.push_back(std::move(incott_snap));
        }
    }

    std::unordered_set<std::string> bt_containers;
    std::vector<bridge::DeviceSnapshot> native_vendor = ScanRoyuanKeyboards();
    for (auto& mouse : ScanKsnakeMice()) {
        native_vendor.push_back(std::move(mouse));
    }
    const std::vector<bridge::DeviceSnapshot> native_bt = ScanBluetoothDevices(&bt_containers);
    const std::vector<bridge::DeviceSnapshot> native_hid = ScanGenericHidDevices(bt_containers);
    const std::vector<bridge::DeviceSnapshot> native_xinput = ScanXInputDevices();

    std::vector<bridge::DeviceSnapshot> hsc;
    if (!upstream_ok) {
        RunHeadsetControl(&hsc);
    }

    return MergeDevices(native_incott, native_vendor, upstream, native_bt, hsc, native_hid,
                        native_xinput);
}

}  // namespace peripherals
