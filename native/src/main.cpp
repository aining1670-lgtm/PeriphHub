// PeriphHub — native host shell.
// Tray icon, taskbar battery widget, and WebView2 settings host.
#include <windows.h>
#include <shellapi.h>
#include <shlobj.h>

#include <algorithm>
#include <atomic>
#include <cctype>
#include <climits>
#include <condition_variable>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <deque>
#include <functional>
#include <map>
#include <mutex>
#include <set>
#include <string>
#include <thread>
#include <utility>

#include "bridge.h"
#include "bridge_args.h"
#include "command_json.h"
#include "driver_router.h"
#include "drivers/host.h"
#include "drivers/registry.h"
#include "hid.h"
#include "json.h"
#include "macro_json.h"
#include "macro_store.h"
#include "peripherals.h"
#include "protocol.h"
#include "text.h"
#include "webview.h"
#include "brands.h"
#include "widget.h"
#include "yjx_hid.h"
#include "yjx_protocol.h"

namespace {

constexpr wchar_t kAppName[] = L"PeriphHub";
constexpr wchar_t kMainClass[] = L"PeriphHubWnd";
constexpr wchar_t kSettingsClass[] = L"PeriphHubSettings";

constexpr UINT kTrayMessage = WM_APP + 1;
constexpr UINT kWorkerReplyMessage = WM_APP + 2;
constexpr UINT kRefreshDoneMessage = WM_APP + 3;
constexpr UINT kWidgetClickMessage = WM_APP + 4;
constexpr UINT kTestNotifyMessage = WM_APP + 5;
constexpr UINT kMenuSettings = 1001;
constexpr UINT kMenuQuit = 1002;
constexpr UINT kMenuWidget = 1003;
constexpr UINT kMenuRefresh = 1005;
constexpr UINT kMenuDeviceBase = 2000;
constexpr UINT kMenuDeviceMax = 2063;
constexpr UINT_PTR kSelftestTimer = 1;
constexpr UINT_PTR kPollTimer = 2;
constexpr UINT_PTR kInitTimer = 3;
constexpr UINT_PTR kRepositionTimer = 4;
constexpr UINT_PTR kWebViewReadyTimer = 5;
constexpr int kDefaultLowBatteryThreshold = 15;
constexpr int kDefaultPollIntervalSec = 30;

constexpr wchar_t kRunKey[] = L"Software\\Microsoft\\Windows\\CurrentVersion\\Run";
constexpr wchar_t kRunValue[] = L"PeriphHub";
constexpr wchar_t kHubRegKey[] = L"Software\\PeriphHub";
constexpr wchar_t kAliasesRegKey[] = L"Software\\PeriphHub\\Aliases";

struct HubSettings {
    bool taskbar_widget = true;
    bool widget_stacked = false;
    bool widget_background = false;
    std::string widget_monitor = "all";
    bool brand_accent = false;
    bool low_battery_notify = true;
    int low_battery_threshold = kDefaultLowBatteryThreshold;
    int poll_interval_sec = kDefaultPollIntervalSec;
    std::vector<std::string> hidden_devices;
    std::map<std::string, std::string> device_aliases;
};

HINSTANCE g_instance = nullptr;
HWND g_main = nullptr;
HWND g_settings = nullptr;
webview::Host* g_host = nullptr;
int g_webview_wait_ticks = 0;
enum class SelftestMode { kNone, kRender, kWrite, kF26 };
bool g_selftest = false;
SelftestMode g_selftest_mode = SelftestMode::kNone;

NOTIFYICONDATAW g_tray{};
std::atomic<bool> g_tray_added{false};
std::mutex g_hub_mutex;
HubSettings g_hub;
int g_active_poll_sec = 0;
std::vector<std::string> g_menu_device_keys;
std::string g_pending_nav_key;
std::set<std::string> g_low_notified_keys;
std::string g_last_notified_device_key;
bool g_light_taskbar = false;
HICON g_dynamic_icon = nullptr;

std::wstring Widen(const std::string& value);
std::string Narrow(const std::wstring& value);
std::wstring ExecutableDirectory();
using host::AppDataDir;
std::wstring AliasesFilePath();
std::wstring PortableAliasesFilePath();
using host::ReadAllBytes;
using host::WriteAllBytesAtomic;

HubSettings SnapshotHubSettings() {
    std::lock_guard<std::mutex> lock(g_hub_mutex);
    return g_hub;
}

bool ReadRegDword(const wchar_t* name, DWORD* out) {
    DWORD val = 0, sz = sizeof(val);
    if (RegGetValueW(HKEY_CURRENT_USER, kHubRegKey, name, RRF_RT_REG_DWORD, nullptr, &val, &sz) ==
        ERROR_SUCCESS) {
        *out = val;
        return true;
    }
    return false;
}

bool ReadRegString(const wchar_t* name, std::wstring* out) {
    DWORD sz = 0;
    if (RegGetValueW(HKEY_CURRENT_USER, kHubRegKey, name, RRF_RT_REG_SZ, nullptr, nullptr, &sz) !=
            ERROR_SUCCESS ||
        sz < sizeof(wchar_t)) {
        return false;
    }
    std::wstring buf(sz / sizeof(wchar_t), L'\0');
    if (RegGetValueW(HKEY_CURRENT_USER, kHubRegKey, name, RRF_RT_REG_SZ, nullptr, buf.data(),
                     &sz) != ERROR_SUCCESS) {
        return false;
    }
    while (!buf.empty() && buf.back() == L'\0') buf.pop_back();
    *out = std::move(buf);
    return true;
}

void LoadHubSettings() {
    HubSettings loaded;
    DWORD dw = 0;
    if (ReadRegDword(L"TaskbarWidget", &dw)) loaded.taskbar_widget = (dw != 0);
    if (ReadRegDword(L"WidgetStacked", &dw)) loaded.widget_stacked = (dw != 0);
    if (ReadRegDword(L"WidgetBackground", &dw)) loaded.widget_background = (dw != 0);
    if (ReadRegDword(L"BrandAccent", &dw)) loaded.brand_accent = (dw != 0);
    if (ReadRegDword(L"LowBatteryNotify", &dw)) loaded.low_battery_notify = (dw != 0);
    if (ReadRegDword(L"LowBatteryThreshold", &dw) && dw >= 5 && dw <= 50) {
        loaded.low_battery_threshold = static_cast<int>(dw);
    }
    if (ReadRegDword(L"PollIntervalSec", &dw) && dw >= 10 && dw <= 300) {
        loaded.poll_interval_sec = static_cast<int>(dw);
    }
    std::wstring ws;
    if (ReadRegString(L"WidgetMonitor", &ws) && !ws.empty()) {
        loaded.widget_monitor = Narrow(ws);
    }
    if (ReadRegString(L"HiddenDevices", &ws) && !ws.empty()) {
        const std::string utf8 = Narrow(ws);
        std::size_t start = 0;
        while (start <= utf8.size()) {
            const std::size_t nl = utf8.find('\n', start);
            const std::string item =
                (nl == std::string::npos) ? utf8.substr(start) : utf8.substr(start, nl - start);
            if (!item.empty() &&
                std::find(loaded.hidden_devices.begin(), loaded.hidden_devices.end(), item) ==
                    loaded.hidden_devices.end()) {
                loaded.hidden_devices.push_back(item);
            }
            if (nl == std::string::npos) break;
            start = nl + 1;
        }
    }
    {
        HKEY alias_key = nullptr;
        if (RegOpenKeyExW(HKEY_CURRENT_USER, kAliasesRegKey, 0, KEY_READ, &alias_key) ==
            ERROR_SUCCESS) {
            for (DWORD index = 0;; ++index) {
                wchar_t name_buf[256]{};
                DWORD name_len = ARRAYSIZE(name_buf);
                wchar_t val_buf[256]{};
                DWORD val_bytes = sizeof(val_buf);
                DWORD type = 0;
                const LSTATUS rc =
                    RegEnumValueW(alias_key, index, name_buf, &name_len, nullptr, &type,
                                  reinterpret_cast<BYTE*>(val_buf), &val_bytes);
                if (rc == ERROR_NO_MORE_ITEMS) break;
                if (rc != ERROR_SUCCESS || type != REG_SZ) continue;
                name_buf[ARRAYSIZE(name_buf) - 1] = L'\0';
                const std::size_t wchars =
                    std::min<std::size_t>(val_bytes / sizeof(wchar_t), ARRAYSIZE(val_buf));
                std::wstring val_wide(val_buf, wchars);
                while (!val_wide.empty() && val_wide.back() == L'\0') val_wide.pop_back();
                const std::string dev_key = Narrow(name_buf);
                const std::string alias = Narrow(val_wide);
                if (!dev_key.empty() && !alias.empty()) {
                    loaded.device_aliases[dev_key] = alias;
                }
            }
            RegCloseKey(alias_key);
        }
    }
    auto load_aliases_from_file = [&](const std::wstring& alias_path) {
        if (alias_path.empty()) return;
        std::string raw;
        std::wstring read_err;
        bool missing = false;
        json::Value root;
        std::string parse_err;
        if (ReadAllBytes(alias_path, &raw, &read_err, &missing) &&
            json::Parse(raw, &root, &parse_err) &&
            root.type == json::Value::Type::kObject) {
            for (const auto& entry : root.object) {
                if (!entry.first.empty() && entry.second.type == json::Value::Type::kString &&
                    !entry.second.text.empty()) {
                    loaded.device_aliases[entry.first] = entry.second.text;
                }
            }
        }
    };
    load_aliases_from_file(AliasesFilePath());
    load_aliases_from_file(PortableAliasesFilePath());
    std::lock_guard<std::mutex> lock(g_hub_mutex);
    g_hub = std::move(loaded);
}

bool SaveDeviceAliases(const std::map<std::string, std::string>& aliases,
                       std::string* err_out = nullptr) {
    if (err_out != nullptr) err_out->clear();
    const std::wstring primary_path = AliasesFilePath();
    const std::wstring portable_path = PortableAliasesFilePath();
    if (primary_path.empty() && portable_path.empty()) {
        if (err_out != nullptr) *err_out = "profile directory is unavailable";
        return false;
    }
    std::string body = "{";
    bool first = true;
    for (const auto& entry : aliases) {
        if (entry.first.empty() || entry.second.empty()) continue;
        if (!first) body += ',';
        first = false;
        body += bridge::JsonQuote(entry.first) + ':' + bridge::JsonQuote(entry.second);
    }
    body += "}\n";
    std::wstring primary_err;
    if (!primary_path.empty() && WriteAllBytesAtomic(primary_path, body, &primary_err)) {
        return true;
    }
    std::wstring fallback_err;
    if (!portable_path.empty() && WriteAllBytesAtomic(portable_path, body, &fallback_err)) {
        return true;
    }
    if (err_out != nullptr) {
        *err_out = Narrow(!primary_err.empty() ? primary_err : fallback_err);
    }
    return false;
}

void SaveHubSettings(const HubSettings& s) {
    HKEY k = nullptr;
    if (RegCreateKeyExW(HKEY_CURRENT_USER, kHubRegKey, 0, nullptr, 0, KEY_SET_VALUE, nullptr, &k,
                        nullptr) != ERROR_SUCCESS) {
        return;
    }
    auto set_dword = [&](const wchar_t* name, DWORD v) {
        RegSetValueExW(k, name, 0, REG_DWORD, reinterpret_cast<const BYTE*>(&v), sizeof(v));
    };
    auto set_str = [&](const wchar_t* name, const std::wstring& v) {
        RegSetValueExW(k, name, 0, REG_SZ, reinterpret_cast<const BYTE*>(v.c_str()),
                       static_cast<DWORD>((v.size() + 1) * sizeof(wchar_t)));
    };
    set_dword(L"TaskbarWidget", s.taskbar_widget ? 1u : 0u);
    set_dword(L"WidgetStacked", s.widget_stacked ? 1u : 0u);
    set_dword(L"WidgetBackground", s.widget_background ? 1u : 0u);
    set_dword(L"BrandAccent", s.brand_accent ? 1u : 0u);
    set_dword(L"LowBatteryNotify", s.low_battery_notify ? 1u : 0u);
    set_dword(L"LowBatteryThreshold", static_cast<DWORD>(s.low_battery_threshold));
    set_dword(L"PollIntervalSec", static_cast<DWORD>(s.poll_interval_sec));
    set_str(L"WidgetMonitor", Widen(s.widget_monitor.empty() ? "all" : s.widget_monitor));
    std::string joined;
    for (std::size_t i = 0; i < s.hidden_devices.size(); ++i) {
        if (i > 0) joined += '\n';
        joined += s.hidden_devices[i];
    }
    set_str(L"HiddenDevices", Widen(joined));
    RegCloseKey(k);
}

Accent DetectBrandAccent(const std::string& name) {
    std::string lower;
    lower.reserve(name.size());
    for (unsigned char ch : name) lower.push_back(static_cast<char>(std::tolower(ch)));
    auto has = [&](const char* needle) { return lower.find(needle) != std::string::npos; };
    if (has("razer")) return Accent::Razer;
    if (has("logitech") || has("g pro") || has("mx master")) return Accent::Logitech;
    if (has("steelseries") || has("arctis") || has("aerox")) return Accent::SteelSeries;
    if (has("corsair")) return Accent::Corsair;
    if (has("hyperx")) return Accent::HyperX;
    if (has("rog") || has("asus")) return Accent::Rog;
    if (has("zowie")) return Accent::Zowie;
    if (has("playstation") || has("dualsense") || has("dualshock")) return Accent::PlayStation;
    if (has("xbox")) return Accent::Xbox;
    if (has("galaxy") || has("samsung")) return Accent::Galaxy;
    if (has("apple") || has("airpods")) return Accent::Apple;
    if (has("jabra")) return Accent::Jabra;
    if (has("jbl")) return Accent::Jbl;
    if (has("cooler master")) return Accent::CoolerMaster;
    if (has("steam")) return Accent::Steam;
    if (has("redragon")) return Accent::Redragon;
    return Accent::None;
}

bool ReadLightTaskbar() {
    DWORD v = 0, sz = sizeof(v);
    if (RegGetValueW(HKEY_CURRENT_USER,
                     L"Software\\Microsoft\\Windows\\CurrentVersion\\Themes\\Personalize",
                     L"SystemUsesLightTheme", RRF_RT_REG_DWORD, nullptr, &v, &sz) == ERROR_SUCCESS) {
        return v != 0;
    }
    return false;
}

std::wstring ExePath() {
    wchar_t path[MAX_PATH]{};
    GetModuleFileNameW(nullptr, path, ARRAYSIZE(path));
    return path;
}

bool AutostartEnabled() {
    wchar_t buf[MAX_PATH * 2]{};
    DWORD sz = sizeof(buf);
    if (RegGetValueW(HKEY_CURRENT_USER, kRunKey, kRunValue, RRF_RT_REG_SZ, nullptr, buf, &sz) != ERROR_SUCCESS) {
        return false;
    }
    return _wcsicmp(buf, (L"\"" + ExePath() + L"\"").c_str()) == 0;
}

bool SetAutostart(bool on) {
    HKEY k = nullptr;
    if (RegOpenKeyExW(HKEY_CURRENT_USER, kRunKey, 0, KEY_SET_VALUE, &k) != ERROR_SUCCESS) {
        return false;
    }
    LSTATUS rc = ERROR_SUCCESS;
    if (on) {
        std::wstring v = L"\"" + ExePath() + L"\"";
        rc = RegSetValueExW(k, kRunValue, 0, REG_SZ, reinterpret_cast<const BYTE*>(v.c_str()),
                            static_cast<DWORD>((v.size() + 1) * sizeof(wchar_t)));
    } else {
        rc = RegDeleteValueW(k, kRunValue);
        if (rc == ERROR_FILE_NOT_FOUND) rc = ERROR_SUCCESS;
    }
    RegCloseKey(k);
    return rc == ERROR_SUCCESS && AutostartEnabled() == on;
}

struct Rgb { BYTE r, g, b; };

HICON RenderIcon(const std::wstring& text, int barPct, Rgb textColor, Rgb barColor, bool dim) {
    HWND taskbar = FindWindowW(L"Shell_TrayWnd", nullptr);
    UINT dpi = taskbar ? GetDpiForWindow(taskbar) : 0;
    const int S = GetSystemMetricsForDpi(SM_CXSMICON, dpi ? dpi : GetDpiForSystem());
    const int barH = std::max(2, S / 8);
    const int textH = S - barH - 1;

    HDC dc = CreateCompatibleDC(nullptr);
    if (dc == nullptr) return nullptr;
    BITMAPINFO bi{};
    bi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bi.bmiHeader.biPlanes = 1;
    bi.bmiHeader.biBitCount = 32;
    bi.bmiHeader.biCompression = BI_RGB;

    const int W = S * 3, H = S * 2;
    bi.bmiHeader.biWidth = W;
    bi.bmiHeader.biHeight = -H;
    void* sbits = nullptr;
    HBITMAP scratch = CreateDIBSection(dc, &bi, DIB_RGB_COLORS, &sbits, nullptr, 0);
    if (scratch == nullptr || sbits == nullptr) {
        if (scratch != nullptr) DeleteObject(scratch);
        DeleteDC(dc);
        return nullptr;
    }
    HGDIOBJ oldBmp = SelectObject(dc, scratch);
    SetBkMode(dc, TRANSPARENT);
    SetTextColor(dc, RGB(255, 255, 255));
    auto* sp = static_cast<DWORD*>(sbits);

    int bx0 = 0, by0 = 0, bx1 = -1, by1 = -1;
    for (int fh = textH * 2; fh >= 6; fh--) {
        HFONT f = CreateFontW(-fh, 0, 0, 0, FW_BOLD, 0, 0, 0, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS,
                              CLIP_DEFAULT_PRECIS, ANTIALIASED_QUALITY, DEFAULT_PITCH, L"Segoe UI");
        HGDIOBJ oldF = SelectObject(dc, f);
        SetTextCharacterExtra(dc, text.size() >= 3 ? -1 : 0);
        memset(sbits, 0, static_cast<std::size_t>(W) * H * 4);
        TextOutW(dc, 2, 0, text.c_str(), static_cast<int>(text.size()));
        GdiFlush();
        SelectObject(dc, oldF);
        DeleteObject(f);
        bx0 = W; by0 = H; bx1 = -1; by1 = -1;
        for (int y = 0; y < H; y++) {
            for (int x = 0; x < W; x++) {
                if (sp[y * W + x] & 0xFF) {
                    bx0 = std::min(bx0, x); bx1 = std::max(bx1, x);
                    by0 = std::min(by0, y); by1 = std::max(by1, y);
                }
            }
        }
        if (bx1 < 0) break;
        if (bx1 - bx0 + 1 <= S && by1 - by0 + 1 <= textH) break;
    }

    bi.bmiHeader.biWidth = S;
    bi.bmiHeader.biHeight = -S;
    void* ibits = nullptr;
    HBITMAP color = CreateDIBSection(dc, &bi, DIB_RGB_COLORS, &ibits, nullptr, 0);
    if (color == nullptr || ibits == nullptr) {
        if (color != nullptr) DeleteObject(color);
        SelectObject(dc, oldBmp);
        DeleteObject(scratch);
        DeleteDC(dc);
        return nullptr;
    }
    auto* ip = static_cast<DWORD*>(ibits);
    memset(ip, 0, static_cast<std::size_t>(S) * S * 4);
    const int alphaScale = dim ? 140 : 255;
    auto put = [&](int x, int y, Rgb c, int a) {
        if (x < 0 || y < 0 || x >= S || y >= S || a <= 0) return;
        a = a * alphaScale / 255;
        ip[y * S + x] = (static_cast<DWORD>(a) << 24) | (static_cast<DWORD>(c.r) << 16) |
                        (static_cast<DWORD>(c.g) << 8) | c.b;
    };
    if (bx1 >= 0) {
        int gw = bx1 - bx0 + 1, gh = by1 - by0 + 1;
        int ox = (S - gw) / 2, oy = (textH - gh) / 2;
        for (int y = 0; y < gh; y++) {
            for (int x = 0; x < gw; x++) {
                put(ox + x, oy + y, textColor, sp[(by0 + y) * W + (bx0 + x)] & 0xFF);
            }
        }
    }
    int fill = barPct < 0 ? 0 : std::max(barPct > 0 ? 1 : 0, (barPct * S + 50) / 100);
    for (int y = S - barH; y < S; y++) {
        for (int x = 0; x < S; x++) {
            if (x < fill) {
                put(x, y, barColor, 255);
            } else {
                put(x, y, g_light_taskbar ? Rgb{0, 0, 0} : Rgb{255, 255, 255}, 70);
            }
        }
    }

    SelectObject(dc, oldBmp);
    DeleteObject(scratch);
    DeleteDC(dc);

    HBITMAP mask = CreateBitmap(S, S, 1, 1, nullptr);
    if (mask == nullptr) {
        DeleteObject(color);
        return nullptr;
    }
    if (HDC mdc = CreateCompatibleDC(nullptr)) {
        HGDIOBJ o = SelectObject(mdc, mask);
        PatBlt(mdc, 0, 0, S, S, BLACKNESS);
        SelectObject(mdc, o);
        DeleteDC(mdc);
    }
    ICONINFO ii{TRUE, 0, 0, mask, color};
    HICON icon = CreateIconIndirect(&ii);
    DeleteObject(mask);
    DeleteObject(color);
    return icon;
}

HICON IconFor(int pct, bool charging, bool dim, int low_threshold) {
    Rgb fg = g_light_taskbar ? Rgb{0, 0, 0} : Rgb{255, 255, 255};
    Rgb bar = charging                ? Rgb{0, 170, 255}
            : pct <= low_threshold    ? Rgb{232, 32, 48}
            : pct <= 30               ? Rgb{255, 150, 0}
                                      : Rgb{30, 190, 90};
    Rgb text = (!charging && pct >= 0 && pct <= low_threshold) ? Rgb{240, 50, 60} : fg;
    std::wstring t = pct < 0 ? L"?" : std::to_wstring(pct);
    return RenderIcon(t, pct, text, bar, dim);
}

HICON LoadAppIcon() {
    HICON icon = LoadIconW(g_instance, MAKEINTRESOURCEW(1));
    return icon != nullptr ? icon : LoadIconW(nullptr, IDI_APPLICATION);
}

bool AddTrayIcon(HWND owner) {
    g_tray.cbSize = sizeof(g_tray);
    g_tray.hWnd = owner;
    g_tray.uID = 1;
    g_tray.uFlags = NIF_MESSAGE | NIF_ICON | NIF_TIP;
    g_tray.uCallbackMessage = kTrayMessage;
    g_tray.hIcon = LoadAppIcon();
    lstrcpynW(g_tray.szTip, kAppName, ARRAYSIZE(g_tray.szTip));
    if (Shell_NotifyIconW(NIM_ADD, &g_tray)) return true;
    std::fwprintf(stderr, L"error: tray icon add failed (%lu)\n", GetLastError());
    return false;
}

void RemoveTrayIcon() {
    Shell_NotifyIconW(NIM_DELETE, &g_tray);
    g_tray_added.store(false);
    if (g_dynamic_icon != nullptr) {
        DestroyIcon(g_dynamic_icon);
        g_dynamic_icon = nullptr;
    }
}

void ShowTrayNotification(const std::wstring& title, const std::wstring& body,
                          DWORD info_flags = NIIF_WARNING | NIIF_RESPECT_QUIET_TIME) {
    if (!g_tray_added.load() || g_tray.hWnd == nullptr) return;
    NOTIFYICONDATAW nid = g_tray;
    nid.uFlags = NIF_INFO;
    nid.dwInfoFlags = info_flags;
    lstrcpynW(nid.szInfoTitle, title.c_str(), ARRAYSIZE(nid.szInfoTitle));
    lstrcpynW(nid.szInfo, body.c_str(), ARRAYSIZE(nid.szInfo));
    Shell_NotifyIconW(NIM_MODIFY, &nid);
}

void CheckLowBatteryNotifications(const std::vector<bridge::DeviceSnapshot>& devices,
                                  const HubSettings& hub) {
    std::set<std::string> connected_keys;
    for (const bridge::DeviceSnapshot& d : devices) {
        if (!d.connected) continue;
        connected_keys.insert(d.key);

        const bool charging = (d.charging == 2);
        const int reset_threshold = hub.low_battery_threshold + 5;
        if (charging || (d.battery >= 0 && d.battery > reset_threshold)) {
            g_low_notified_keys.erase(d.key);
            continue;
        }
        if (!hub.low_battery_notify || d.asleep || d.battery < 0 ||
            d.battery > hub.low_battery_threshold) {
            continue;
        }
        if (g_low_notified_keys.count(d.key) != 0) continue;

        g_low_notified_keys.insert(d.key);
        g_last_notified_device_key = d.key;
        const std::wstring title = Widen(d.name) + L" 电量低";
        const std::wstring body =
            L"当前剩余电量 " + std::to_wstring(d.battery) + L"%（阈值 " +
            std::to_wstring(hub.low_battery_threshold) + L"%），请及时充电。";
        ShowTrayNotification(title, body, NIIF_WARNING | NIIF_RESPECT_QUIET_TIME);
    }

    // Clear notified state for devices that disconnected so they can alert again when reconnected.
    for (auto it = g_low_notified_keys.begin(); it != g_low_notified_keys.end();) {
        if (connected_keys.count(*it) == 0) {
            it = g_low_notified_keys.erase(it);
        } else {
            ++it;
        }
    }
}

std::wstring Widen(const std::string& value) { return text::ToWide(value); }

DeviceKind KindFromString(const std::string& kind) {
    if (kind == "mouse") return DeviceKind::Mouse;
    if (kind == "headset") return DeviceKind::Headset;
    if (kind == "keyboard") return DeviceKind::Keyboard;
    if (kind == "gamepad") return DeviceKind::Gamepad;
    if (kind == "phone") return DeviceKind::Phone;
    if (kind == "microphone") return DeviceKind::Microphone;
    return DeviceKind::Other;
}

void UpdateDisplay() {
    const HubSettings hub = SnapshotHubSettings();
    g_light_taskbar = ReadLightTaskbar();
    widget::SetLightTheme(g_light_taskbar);
    widget::SetStacked(hub.widget_stacked);
    widget::SetBackground(hub.widget_background);
    widget::SetMonitorChoice(L"main");

    if (g_main != nullptr && g_active_poll_sec != hub.poll_interval_sec) {
        g_active_poll_sec = hub.poll_interval_sec;
        SetTimer(g_main, kPollTimer, static_cast<UINT>(g_active_poll_sec) * 1000u, nullptr);
    }

    std::wstring tip = kAppName;
    const std::vector<bridge::DeviceSnapshot> devices = bridge::Devices();
    std::vector<WidgetItem> items;
    int tray_battery = -1;
    bool tray_charging = false;
    bool tray_dim = false;
    bool any_connected = false;
    std::wstring summary;

    auto is_hidden = [&](const std::string& key) {
        return std::find(hub.hidden_devices.begin(), hub.hidden_devices.end(), key) !=
               hub.hidden_devices.end();
    };

    for (const bridge::DeviceSnapshot& device : devices) {
        if (!device.connected) continue;
        any_connected = true;

        const bool item_charging = (device.charging == 2);
        const bool wired_no_battery =
            (device.battery < 0 && device.level.empty() && device.mode == "wired");
        const bool bt_no_battery =
            (device.battery < 0 && device.level.empty() && device.mode == "bluetooth");

        if (!wired_no_battery && !bt_no_battery && !is_hidden(device.key)) {
            const bool item_dim =
                (device.asleep || (device.battery < 0 && device.level.empty()));

            WidgetItem item;
            item.kind = KindFromString(device.kind);
            item.percent = device.battery;
            if (device.battery < 0 && !device.level.empty()) {
                item.text = Widen(device.level);
            }
            item.charging = item_charging;
            item.full = (item_charging && device.battery >= 100);
            item.dim = item_dim;
            item.low = (!item_charging && device.battery >= 0 &&
                        device.battery <= hub.low_battery_threshold);
            item.bluetooth = (device.mode == "bluetooth");
            item.accent = hub.brand_accent ? DetectBrandAccent(device.name) : Accent::None;
            items.push_back(item);
        }

        if (!summary.empty()) summary += L" | ";
        summary += Widen(device.name);
        if (device.battery >= 0) {
            summary += L" " + std::to_wstring(device.battery) + L"%";
        } else if (!device.level.empty()) {
            summary += L" " + Widen(device.level);
        } else if (device.mode == "wired") {
            summary += L" 有线";
        } else if (device.mode == "bluetooth") {
            summary += L" 蓝牙";
        }
        if (item_charging) summary += L"（充电中）";
        if (device.asleep) summary += L"（休眠中）";

        if (tray_battery < 0 && device.battery >= 0) {
            tray_battery = device.battery;
            tray_charging = item_charging;
            tray_dim = device.asleep;
        }
    }

    if (devices.empty()) {
        tip = L"PeriphHub — 未发现设备（移动鼠标唤醒 / 插拔接收器 / 开启蓝牙）";
    } else if (!any_connected) {
        tip = L"PeriphHub — 暂无在线外设（移动鼠标唤醒 / 插拔接收器 / 开启蓝牙）";
    } else {
        tip = L"PeriphHub — " + summary;
    }

    if (hub.taskbar_widget) {
        if (!widget::Alive() && g_main != nullptr) {
            widget::Create(g_instance, g_main, kWidgetClickMessage, g_light_taskbar);
        }
        if (widget::Alive()) {
            widget::SetItems(items, tip);
        }
    } else {
        if (widget::Alive()) {
            widget::Destroy();
        }
    }

    if (g_dynamic_icon != nullptr) {
        DestroyIcon(g_dynamic_icon);
        g_dynamic_icon = nullptr;
    }
    if (any_connected && tray_battery >= 0) {
        g_dynamic_icon = IconFor(tray_battery, tray_charging, tray_dim, hub.low_battery_threshold);
        g_tray.hIcon = g_dynamic_icon;
    } else {
        g_tray.hIcon = LoadAppIcon();
    }
    g_tray.uFlags = NIF_MESSAGE | NIF_ICON | NIF_TIP;
    lstrcpynW(g_tray.szTip, tip.c_str(), ARRAYSIZE(g_tray.szTip));
    if (g_tray_added.load() && g_tray.hWnd != nullptr) {
        Shell_NotifyIconW(NIM_MODIFY, &g_tray);
    }
    CheckLowBatteryNotifications(devices, hub);
}

void PostNavigateEvent(const std::string& device_key) {
    if (device_key.empty() || g_host == nullptr || !g_host->Started()) return;
    g_host->PostJson("{\"event\":\"navigate\",\"deviceKey\":" + bridge::JsonQuote(device_key) +
                     "}");
}

void PostWindowStateEvent(HWND hwnd) {
    if (hwnd == nullptr || g_host == nullptr || !g_host->Started()) return;
    const bool maximized = IsZoomed(hwnd) != FALSE;
    g_host->PostJson(std::string("{\"event\":\"windowState\",\"maximized\":") +
                     (maximized ? "true" : "false") + "}");
}

struct DwmMargins {
    int cxLeftWidth;
    int cxRightWidth;
    int cyTopHeight;
    int cyBottomHeight;
};

void ApplyDwmDarkAttribute(HWND hwnd, bool dark) {
    if (hwnd == nullptr) return;
    HMODULE dwm = LoadLibraryExW(L"dwmapi.dll", nullptr, LOAD_LIBRARY_SEARCH_SYSTEM32);
    if (dwm == nullptr) return;
    using SetAttrFn = HRESULT(WINAPI*)(HWND, DWORD, LPCVOID, DWORD);
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wcast-function-type"
    const auto set_attr = reinterpret_cast<SetAttrFn>(GetProcAddress(dwm, "DwmSetWindowAttribute"));
#pragma GCC diagnostic pop
    if (set_attr != nullptr) {
        const DWORD dark_mode = dark ? 1u : 0u;
        set_attr(hwnd, 20 /* DWMWA_USE_IMMERSIVE_DARK_MODE */, &dark_mode, sizeof(dark_mode));
    }
    FreeLibrary(dwm);
}

void ConfigureCustomWindowFrame(HWND hwnd) {
    if (hwnd == nullptr) return;
    HMODULE dwm = LoadLibraryExW(L"dwmapi.dll", nullptr, LOAD_LIBRARY_SEARCH_SYSTEM32);
    if (dwm != nullptr) {
        using ExtendFn = HRESULT(WINAPI*)(HWND, const DwmMargins*);
        using SetAttrFn = HRESULT(WINAPI*)(HWND, DWORD, LPCVOID, DWORD);
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wcast-function-type"
        const auto extend =
            reinterpret_cast<ExtendFn>(GetProcAddress(dwm, "DwmExtendFrameIntoClientArea"));
        const auto set_attr =
            reinterpret_cast<SetAttrFn>(GetProcAddress(dwm, "DwmSetWindowAttribute"));
#pragma GCC diagnostic pop
        if (extend != nullptr) {
            const DwmMargins margins{1, 1, 1, 1};
            extend(hwnd, &margins);
        }
        if (set_attr != nullptr) {
            const DWORD dark_mode = ReadLightTaskbar() ? 0u : 1u;
            set_attr(hwnd, 20 /* DWMWA_USE_IMMERSIVE_DARK_MODE */, &dark_mode, sizeof(dark_mode));
            const DWORD corner_pref = 2u; /* DWMWCP_ROUND */
            set_attr(hwnd, 33 /* DWMWA_WINDOW_CORNER_PREFERENCE */, &corner_pref,
                     sizeof(corner_pref));
        }
        FreeLibrary(dwm);
    }
    SetWindowPos(hwnd, nullptr, 0, 0, 0, 0,
                 SWP_FRAMECHANGED | SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE);
}

bool TryHandleWindowUiRequest(HWND hwnd, const std::string& request_json,
                              const std::function<void(std::string)>& respond) {
    if (request_json.find("\"window.") == std::string::npos) return false;
    bridge::Request req;
    std::string err;
    if (!bridge::ParseRequest(request_json, &req, &err)) return false;
    if (req.cmd.rfind("window.", 0) != 0) return false;

    auto reply_state = [&]() {
        const bool maximized = IsZoomed(hwnd) != FALSE;
        if (respond) {
            respond(bridge::Ok(req, std::string("{\"maximized\":") +
                                        (maximized ? "true" : "false") + "}"));
        }
    };

    if (req.cmd == "window.state") {
        reply_state();
        return true;
    }
    if (req.cmd == "window.minimize") {
        ShowWindow(hwnd, SW_MINIMIZE);
        reply_state();
        return true;
    }
    if (req.cmd == "window.maximize") {
        ShowWindow(hwnd, IsZoomed(hwnd) ? SW_RESTORE : SW_MAXIMIZE);
        reply_state();
        PostWindowStateEvent(hwnd);
        return true;
    }
    if (req.cmd == "window.close") {
        if (respond) respond(bridge::Ok(req, "{}"));
        PostMessageW(hwnd, WM_CLOSE, 0, 0);
        return true;
    }
    if (req.cmd == "window.drag") {
        if (respond) respond(bridge::Ok(req, "{}"));
        ReleaseCapture();
        SendMessageW(hwnd, WM_NCLBUTTONDOWN, HTCAPTION, 0);
        return true;
    }
    if (req.cmd == "window.sysmenu") {
        int x = -1;
        int y = -1;
        if (req.args != nullptr) {
            long long vx = 0, vy = 0;
            if (req.args->GetInt("x", &vx)) x = static_cast<int>(vx);
            if (req.args->GetInt("y", &vy)) y = static_cast<int>(vy);
        }
        POINT pt{};
        if (x >= 0 && y >= 0) {
            pt.x = x;
            pt.y = y;
            ClientToScreen(hwnd, &pt);
        } else {
            GetCursorPos(&pt);
        }
        if (respond) respond(bridge::Ok(req, "{}"));
        HMENU sys_menu = GetSystemMenu(hwnd, FALSE);
        if (sys_menu != nullptr) {
            const bool max_now = IsZoomed(hwnd) != FALSE;
            EnableMenuItem(sys_menu, SC_RESTORE,
                           MF_BYCOMMAND | (max_now ? MF_ENABLED : MF_GRAYED));
            EnableMenuItem(sys_menu, SC_MOVE,
                           MF_BYCOMMAND | (!max_now ? MF_ENABLED : MF_GRAYED));
            EnableMenuItem(sys_menu, SC_SIZE,
                           MF_BYCOMMAND | (!max_now ? MF_ENABLED : MF_GRAYED));
            EnableMenuItem(sys_menu, SC_MAXIMIZE,
                           MF_BYCOMMAND | (!max_now ? MF_ENABLED : MF_GRAYED));
            EnableMenuItem(sys_menu, SC_MINIMIZE, MF_BYCOMMAND | MF_ENABLED);
            const int cmd = TrackPopupMenu(sys_menu, TPM_RETURNCMD | TPM_RIGHTBUTTON, pt.x, pt.y,
                                           0, hwnd, nullptr);
            if (cmd != 0) {
                PostMessageW(hwnd, WM_SYSCOMMAND, static_cast<WPARAM>(cmd), 0);
            }
        }
        return true;
    }
    if (req.cmd == "window.theme") {
        bool dark = !ReadLightTaskbar();
        if (req.args != nullptr) {
            req.args->GetBool("dark", &dark);
        }
        ApplyDwmDarkAttribute(hwnd, dark);
        if (respond) respond(bridge::Ok(req, "{}"));
        return true;
    }
    if (respond) respond(bridge::Failure(req, "unknown window command: " + req.cmd));
    return true;
}

void ShowSettings(const std::string& device_key = "") {
    if (!device_key.empty()) {
        g_pending_nav_key = device_key;
    }
    if (g_settings != nullptr) {
        if (IsIconic(g_settings)) {
            ShowWindow(g_settings, SW_RESTORE);
        }
        ShowWindow(g_settings, SW_SHOW);
        SetForegroundWindow(g_settings);
        if (!g_pending_nav_key.empty() && g_host != nullptr && g_host->Started()) {
            PostNavigateEvent(g_pending_nav_key);
            g_pending_nav_key.clear();
        }
        return;
    }

    RECT work{};
    POINT pt{};
    GetCursorPos(&pt);
    MONITORINFO mi{};
    mi.cbSize = sizeof(mi);
    if (GetMonitorInfoW(MonitorFromPoint(pt, MONITOR_DEFAULTTONEAREST), &mi)) {
        work = mi.rcWork;
    } else if (!SystemParametersInfoW(SPI_GETWORKAREA, 0, &work, 0)) {
        work = {0, 0, GetSystemMetrics(SM_CXSCREEN), GetSystemMetrics(SM_CYSCREEN)};
    }

    UINT dpi = GetDpiForSystem();
    if (dpi == 0) dpi = 96;

    RECT rc{0, 0, MulDiv(1140, static_cast<int>(dpi), 96),
            MulDiv(800, static_cast<int>(dpi), 96)};
    if (!AdjustWindowRectExForDpi(&rc, WS_OVERLAPPEDWINDOW, FALSE, 0, dpi)) {
        AdjustWindowRectEx(&rc, WS_OVERLAPPEDWINDOW, FALSE, 0);
    }

    const int margin = MulDiv(32, static_cast<int>(dpi), 96);
    const int work_w = work.right - work.left;
    const int work_h = work.bottom - work.top;
    const int max_w = (work_w > margin + 320) ? (work_w - margin) : work_w;
    const int max_h = (work_h > margin + 240) ? (work_h - margin) : work_h;
    int win_w = rc.right - rc.left;
    int win_h = rc.bottom - rc.top;
    if (win_w > max_w) win_w = max_w;
    if (win_h > max_h) win_h = max_h;

    const int x = work.left + (work_w - win_w) / 2;
    const int y = work.top + (work_h - win_h) / 2;

    g_settings = CreateWindowExW(0, kSettingsClass, L"PeriphHub — 设备设置", WS_OVERLAPPEDWINDOW,
                                 x, y, win_w, win_h, nullptr, nullptr,
                                 g_instance, nullptr);
    if (g_settings != nullptr) {
        ShowWindow(g_settings, SW_SHOW);
        UpdateWindow(g_settings);
    }
}

std::wstring ExecutableDirectory() {
    wchar_t path[MAX_PATH]{};
    GetModuleFileNameW(nullptr, path, ARRAYSIZE(path));
    std::wstring directory(path);
    const std::size_t slash = directory.find_last_of(L"\\/");
    return slash == std::wstring::npos ? std::wstring(L".") : directory.substr(0, slash);
}

// Every page request goes through here. The UI thread's only job is to enqueue
// the request: the worker thread computes the reply and posts it back, so the
// WebView2 UI thread never blocks on HID round-trips or file writes.
bool RefreshDeviceCache(int attempts = 3);
bool RefreshKeyCache();
int WriteKey(incott::Device& device, int index, std::uint32_t code, std::uint32_t* readback,
             bool dump = true);

incott::MacroStore g_macros;  // loaded once at startup, saved on every edit
bool SaveMacroLibrary(const incott::MacroStore& store, std::wstring* error);

std::string HandleDpiRequest(const bridge::Request& request);
std::string HandleHardwareExtendedRequest(const bridge::Request& request);
std::string HandleDeviceInfoRequest(const bridge::Request& request);
std::string HandleDeviceRenameRequest(const bridge::Request& request);
std::string HandleHubRequest(const bridge::Request& request);
std::string MacroListJson();
std::string BridgeHandler(const std::string& request, bool* selftest_report = nullptr);

enum class WorkerJobKind { kRequest, kRefresh };

// One queued request. `respond` is the callback webview.cpp handed to the UI
// thread; it must be called on the UI thread because the WebView2 host posts
// replies from the UI thread.
struct WorkerJob {
    WorkerJobKind kind = WorkerJobKind::kRequest;
    unsigned long long generation = 0;
    HWND window = nullptr;
    std::string request;
    std::function<void(std::string)> respond;
};

// One computed reply, carried to the UI thread in a posted message.
struct WorkerReply {
    unsigned long long generation = 0;
    HWND window = nullptr;
    std::string json;
    std::function<void(std::string)> respond;
    bool selftest_report = false;
    bool devices_loaded = false;
};

std::mutex g_job_mutex;
std::condition_variable g_job_ready;
std::deque<WorkerJob> g_jobs;
std::thread g_worker;
bool g_worker_running = false;
bool g_worker_stop = false;
// Bumped whenever the hosting window (re)starts or goes away: a reply computed
// for an older generation is dropped instead of being delivered to a dead host.
std::atomic<unsigned long long> g_view_generation{0};

void WorkerMain() {
    for (;;) {
        WorkerJob job;
        {
            std::unique_lock<std::mutex> lock(g_job_mutex);
            g_job_ready.wait(lock, [] { return g_worker_stop || !g_jobs.empty(); });
            if (g_worker_stop) return;
            job = std::move(g_jobs.front());
            g_jobs.pop_front();
        }
        if (job.kind == WorkerJobKind::kRefresh) {
            // Tray polling: HID open + 0x8F/0x89 reads block for a few hundred
            // milliseconds, so they run here and never in the UI message loop.
            RefreshDeviceCache();
            PostMessageW(job.window, kRefreshDoneMessage, 0, 0);
            continue;
        }
        bool selftest_report = false;
        std::string reply = BridgeHandler(job.request, &selftest_report);
        const bool devices_loaded =
            job.request.find("\"devices.get\"") != std::string::npos ||
            job.request.find("\"device.probe\"") != std::string::npos;
        // The view may have been destroyed while this request was running.
        if (g_view_generation.load() != job.generation || !IsWindow(job.window)) continue;
        auto* payload = new WorkerReply{job.generation, job.window, std::move(reply),
                                        std::move(job.respond), selftest_report, devices_loaded};
        if (!PostMessageW(job.window, kWorkerReplyMessage, 0,
                          reinterpret_cast<LPARAM>(payload))) {
            delete payload;
        }
    }
}

void StartWorker() {
    std::lock_guard<std::mutex> lock(g_job_mutex);
    if (g_worker_running) return;
    g_worker_stop = false;
    g_jobs.clear();
    g_worker = std::thread(WorkerMain);
    g_worker_running = true;
}

// Joins the worker and drops replies that were posted but never handled.
void StopWorker(HWND window) {
    {
        std::lock_guard<std::mutex> lock(g_job_mutex);
        g_worker_stop = true;
    }
    g_job_ready.notify_all();
    if (g_worker.joinable()) g_worker.join();
    {
        std::lock_guard<std::mutex> lock(g_job_mutex);
        g_worker_running = false;
        g_worker_stop = false;
        g_jobs.clear();
    }
    MSG message{};
    // Drain by message type only: the settings window is being destroyed, and a
    // posted payload would otherwise leak.
    while (PeekMessageW(&message, nullptr, kWorkerReplyMessage, kWorkerReplyMessage, PM_REMOVE)) {
        delete reinterpret_cast<WorkerReply*>(message.lParam);
    }
}

// Drops replies already posted for a window that is going away, without
// stopping the worker: tray polling keeps using it after the settings window
// closes.
void DrainWorkerReplies(HWND window) {
    MSG message{};
    while (PeekMessageW(&message, nullptr, kWorkerReplyMessage, kWorkerReplyMessage, PM_REMOVE)) {
        auto* reply = reinterpret_cast<WorkerReply*>(message.lParam);
        if (reply == nullptr) continue;
        if (reply->window == window) {
            delete reply;
        } else if (!PostMessageW(reply->window, kWorkerReplyMessage, message.wParam,
                                 message.lParam)) {
            // The target window is gone; the re-post never entered a queue.
            delete reply;
        }
    }
}

void EnqueueRequest(HWND window, const std::string& request,
                    std::function<void(std::string)> respond) {
    {
        std::lock_guard<std::mutex> lock(g_job_mutex);
        g_jobs.push_back(WorkerJob{WorkerJobKind::kRequest, g_view_generation.load(), window, request,
                                   std::move(respond)});
    }
    g_job_ready.notify_one();
}

void EnqueueRefresh(HWND window) {
    {
        std::lock_guard<std::mutex> lock(g_job_mutex);
        g_jobs.push_back(WorkerJob{WorkerJobKind::kRefresh, 0, window, {}, {}});
    }
    g_job_ready.notify_one();
}

// Requests are routed by the parsed `cmd` only, never by substring search: a
// macro name or a binding could contain a command name. Runs on the worker.
std::string BridgeHandler(const std::string& request_json, bool* selftest_report) {
    bridge::Request request;
    std::string error;
    if (!bridge::ParseRequest(request_json, &request, &error)) {
        return bridge::Failure(request, "invalid JSON request: " + error);
    }
    // A page refresh should hit the hardware, not just the cache. The tray tip is
    // refreshed by the poll timer on the UI thread (Shell_NotifyIcon is not for
    // the worker).
    if (request.cmd == "device.probe") {
        RefreshDeviceCache();
        return bridge::Ok(request, bridge::DevicesJson());
    }
    if (request.cmd == "devices.get") {
        return bridge::Ok(request, bridge::DevicesJson());
    }
    if (request.cmd == "device.info") {
        return HandleDeviceInfoRequest(request);
    }
    if (request.cmd == "device.rename") {
        return HandleDeviceRenameRequest(request);
    }

    // Multi-driver dispatch: route hardware commands by optional args.deviceKey.
    std::string device_key;
    if (request.args != nullptr) {
        if (const json::Value* key_val = request.args->Find("deviceKey")) {
            if (key_val->type != json::Value::Type::kString) {
                return bridge::Failure(request, "deviceKey must be a string");
            }
            device_key = key_val->text;
        }
    }
    if (drivers::IsDriverCommand(request.cmd)) {
        const drivers::DriverDescriptor* d = drivers::Global().FindByKey(device_key);
        if (d == nullptr || !d->handle ||
            std::find(d->commands.begin(), d->commands.end(), request.cmd) == d->commands.end()) {
            return bridge::Failure(request, drivers::UnsupportedMessage(request.cmd));
        }
        return d->handle(request, device_key);
    }
    if (request.cmd == "hub.get" || request.cmd == "hub.set" ||
        request.cmd == "hub.testNotify") {
        return HandleHubRequest(request);
    }
    if (request.cmd == "selftest.report") {
        if (selftest_report != nullptr) *selftest_report = true;
        if (g_selftest) {
            std::printf("selftest: %s\n", request_json.c_str());
            std::fflush(stdout);
        }
        return bridge::Ok(request, "{}");
    }
    return bridge::Failure(request, "not implemented: " + request.cmd);
}

LRESULT CALLBACK SettingsProc(HWND hwnd, UINT message, WPARAM wparam, LPARAM lparam) {
    switch (message) {
        case WM_CREATE: {
            ConfigureCustomWindowFrame(hwnd);
            RECT client{};
            GetClientRect(hwnd, &client);
            ++g_view_generation;
            StartWorker();
            std::wstring page = ExecutableDirectory() + L"\\web\\index.html";
            // web/index.html runs its one-shot report when the fragment says so.
            if (g_selftest_mode == SelftestMode::kF26) {
                page += L"#selftest-f26";
            } else if (g_selftest_mode == SelftestMode::kWrite) {
                page += L"#selftest-write";
            } else if (g_selftest) {
                page += L"#selftest";
            }
            std::wstring error;
            g_host = new webview::Host();
            const auto handler = [hwnd](const std::string& request,
                                        std::function<void(std::string)> respond) {
                if (TryHandleWindowUiRequest(hwnd, request, respond)) return;
                EnqueueRequest(hwnd, request, std::move(respond));
            };
            if (!g_host->Start(hwnd, page, handler, error)) {
                if (g_selftest) {
                    std::fwprintf(stderr, L"error: webview did not start: %ls\n", error.c_str());
                    PostQuitMessage(1);
                } else {
                    std::wstring hint = L"请确认系统已安装 Microsoft Edge WebView2 Runtime。";
                    if (error.find(L"web\\index.html") != std::wstring::npos) {
                        hint = L"请确保 web\\index.html 文件夹与 PeriphHub.exe 位于同一目录下，不要单独移动或直接从压缩包内运行 exe。";
                    } else if (error.find(L"WebView2Loader.dll") != std::wstring::npos) {
                        hint = L"请确保 WebView2Loader.dll 与 PeriphHub.exe 位于同一目录下，不要单独移动或直接从压缩包内运行 exe。";
                    }
                    const std::wstring msg = error + L"\n\n" + hint;
                    MessageBoxW(hwnd, msg.c_str(), L"PeriphHub", MB_ICONERROR);
                    PostMessageW(hwnd, WM_CLOSE, 0, 0);
                }
                delete g_host;
                g_host = nullptr;
            }
            if (g_host != nullptr) {
                g_host->Resize(client.right, client.bottom);
                if (!g_selftest) {
                    g_webview_wait_ticks = 0;
                    SetTimer(hwnd, kWebViewReadyTimer, 500, nullptr);
                }
            }
            return 0;
        }
        case WM_NCCALCSIZE: {
            if (wparam == TRUE && lparam != 0) {
                auto* params = reinterpret_cast<NCCALCSIZE_PARAMS*>(lparam);
                if (IsZoomed(hwnd)) {
                    HMONITOR mon = MonitorFromWindow(hwnd, MONITOR_DEFAULTTONEAREST);
                    MONITORINFO mi{};
                    mi.cbSize = sizeof(mi);
                    if (GetMonitorInfoW(mon, &mi)) {
                        params->rgrc[0] = mi.rcWork;
                    }
                }
                return 0;
            }
            break;
        }
        case WM_NCHITTEST: {
            LRESULT hit = DefWindowProcW(hwnd, message, wparam, lparam);
            if (hit != HTCLIENT || IsZoomed(hwnd)) return hit;
            POINT pt{static_cast<short>(LOWORD(lparam)), static_cast<short>(HIWORD(lparam))};
            ScreenToClient(hwnd, &pt);
            RECT rc{};
            GetClientRect(hwnd, &rc);
            UINT dpi = GetDpiForWindow(hwnd);
            if (dpi == 0) dpi = GetDpiForSystem();
            if (dpi == 0) dpi = 96;
            const int border = MulDiv(6, static_cast<int>(dpi), 96);
            const bool left = pt.x < border;
            const bool right = pt.x >= rc.right - border;
            const bool top = pt.y < border;
            const bool bottom = pt.y >= rc.bottom - border;
            if (top && left) return HTTOPLEFT;
            if (top && right) return HTTOPRIGHT;
            if (bottom && left) return HTBOTTOMLEFT;
            if (bottom && right) return HTBOTTOMRIGHT;
            if (left) return HTLEFT;
            if (right) return HTRIGHT;
            if (top) return HTTOP;
            if (bottom) return HTBOTTOM;
            return HTCLIENT;
        }
        case WM_SIZE: {
            if (g_host != nullptr) {
                g_host->Resize(LOWORD(lparam), HIWORD(lparam));
                PostWindowStateEvent(hwnd);
            }
            return 0;
        }
        case WM_GETMINMAXINFO: {
            auto* mmi = reinterpret_cast<MINMAXINFO*>(lparam);
            UINT dpi = GetDpiForWindow(hwnd);
            if (dpi == 0) dpi = GetDpiForSystem();
            if (dpi == 0) dpi = 96;
            int min_w = MulDiv(880, static_cast<int>(dpi), 96);
            int min_h = MulDiv(600, static_cast<int>(dpi), 96);
            MONITORINFO mi{};
            mi.cbSize = sizeof(mi);
            if (GetMonitorInfoW(MonitorFromWindow(hwnd, MONITOR_DEFAULTTONEAREST), &mi)) {
                const int work_w = mi.rcWork.right - mi.rcWork.left;
                const int work_h = mi.rcWork.bottom - mi.rcWork.top;
                if (min_w > work_w) min_w = work_w;
                if (min_h > work_h) min_h = work_h;
            }
            mmi->ptMinTrackSize.x = min_w;
            mmi->ptMinTrackSize.y = min_h;
            return 0;
        }
        case kWorkerReplyMessage: {
            auto* reply = reinterpret_cast<WorkerReply*>(lparam);
            if (reply != nullptr && reply->window == hwnd &&
                reply->generation == g_view_generation.load() && IsWindow(reply->window) &&
                g_host != nullptr) {
                if (reply->respond) reply->respond(reply->json);
                if (reply->devices_loaded && !g_pending_nav_key.empty()) {
                    PostNavigateEvent(g_pending_nav_key);
                    g_pending_nav_key.clear();
                }
                if (reply->selftest_report && g_selftest) PostQuitMessage(0);
            }
            delete reply;
            return 0;
        }
        case WM_TIMER: {
            if (wparam == kWebViewReadyTimer) {
                if (g_host == nullptr || g_host->Started()) {
                    KillTimer(hwnd, kWebViewReadyTimer);
                    return 0;
                }
                if (!g_host->Error().empty()) {
                    KillTimer(hwnd, kWebViewReadyTimer);
                    const std::wstring msg =
                        L"设置界面启动失败：" + g_host->Error() +
                        L"\n\n请安装或修复 Microsoft Edge WebView2 Runtime 后重试。";
                    MessageBoxW(hwnd, msg.c_str(), L"PeriphHub", MB_ICONERROR);
                    PostMessageW(hwnd, WM_CLOSE, 0, 0);
                    return 0;
                }
                if (++g_webview_wait_ticks >= 20) {
                    KillTimer(hwnd, kWebViewReadyTimer);
                    MessageBoxW(hwnd,
                                L"WebView2 初始化超时（10 秒内未就绪）。\n\n"
                                L"请确认已安装 Microsoft Edge WebView2 Runtime 或重新启动程序。",
                                L"PeriphHub", MB_ICONERROR);
                    PostMessageW(hwnd, WM_CLOSE, 0, 0);
                    return 0;
                }
                return 0;
            }
            if (wparam != kSelftestTimer) break;
            if (g_host == nullptr || !g_host->Started()) {
                std::fwprintf(stderr, L"error: webview did not start: %ls\n",
                              g_host != nullptr ? g_host->Error().c_str() : L"no host");
                PostQuitMessage(2);
            } else {
                std::fwprintf(stderr, L"error: no selftest report within %u s\n",
                              g_selftest_mode == SelftestMode::kF26 ? 30u : 20u);
                PostQuitMessage(3);
            }
            return 0;
        }
        case WM_CLOSE:
            if (!g_tray_added && !widget::Alive() && g_main != nullptr) {
                // Without a tray icon or taskbar widget the settings window is the only UI:
                // closing it shuts the process down instead of leaving it invisible.
                DestroyWindow(g_main);
                return 0;
            }
            // Destroy the settings window immediately on close so WebView2 subprocesses
            // tear down and background tray residency returns to pure Win32 (~5-12 MB).
            DestroyWindow(hwnd);
            if (g_main != nullptr) EnqueueRefresh(g_main);
            return 0;
        case WM_DESTROY:
            KillTimer(hwnd, kWebViewReadyTimer);
            ++g_view_generation;
            DrainWorkerReplies(hwnd);
            if (g_host != nullptr) {
                g_host->Close();
                delete g_host;
                g_host = nullptr;
            }
            g_settings = nullptr;
            return 0;
        default:
            break;
    }
    return DefWindowProcW(hwnd, message, wparam, lparam);
}

void ShowTrayMenu(HWND owner) {
    POINT cursor{};
    GetCursorPos(&cursor);
    HMENU menu = CreatePopupMenu();

    g_menu_device_keys.clear();
    const std::vector<bridge::DeviceSnapshot> devices = bridge::Devices();
    for (const bridge::DeviceSnapshot& d : devices) {
        if (!d.connected) continue;
        if (kMenuDeviceBase + g_menu_device_keys.size() > kMenuDeviceMax) break;
        std::wstring label = Widen(d.name);
        if (d.battery >= 0) {
            label += L" — " + std::to_wstring(d.battery) + L"%";
        } else if (!d.level.empty()) {
            label += L" — " + Widen(d.level);
        } else if (d.mode == "wired") {
            label += L" — 有线";
        } else if (d.mode == "bluetooth") {
            label += L" — 蓝牙已连接";
        }
        if (d.charging == 2) {
            label += (d.battery >= 100) ? L"（已充满）" : L"（充电中）";
        }
        if (d.asleep) label += L"（休眠中）";
        const UINT id = static_cast<UINT>(kMenuDeviceBase + g_menu_device_keys.size());
        g_menu_device_keys.push_back(d.key);
        AppendMenuW(menu, MF_STRING, id, label.c_str());
    }
    if (g_menu_device_keys.empty()) {
        AppendMenuW(menu, MF_STRING | MF_GRAYED, 0, L"暂无已连接设备");
    }

    const HubSettings hub = SnapshotHubSettings();
    AppendMenuW(menu, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(menu, MF_STRING, kMenuSettings, L"设备设置...");
    AppendMenuW(menu, MF_STRING, kMenuRefresh, L"立即刷新");
    AppendMenuW(menu, MF_STRING | (hub.taskbar_widget ? MF_CHECKED : MF_UNCHECKED),
                kMenuWidget, L"显示任务栏电量条");
    AppendMenuW(menu, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(menu, MF_STRING, kMenuQuit, L"退出");
    SetForegroundWindow(owner);
    TrackPopupMenu(menu, TPM_RIGHTBUTTON, cursor.x, cursor.y, 0, owner, nullptr);
    DestroyMenu(menu);
}

LRESULT CALLBACK MainProc(HWND hwnd, UINT message, WPARAM wparam, LPARAM lparam) {
    switch (message) {
        case kWidgetClickMessage: {
            if (wparam == 1) {
                ShowTrayMenu(hwnd);
            } else {
                ShowSettings();
            }
            return 0;
        }
        case WM_TIMER: {
            if (wparam == kInitTimer) {
                KillTimer(hwnd, kInitTimer);
                const HubSettings hub = SnapshotHubSettings();
                g_light_taskbar = ReadLightTaskbar();
                g_tray_added = AddTrayIcon(hwnd);
                widget::SetStacked(hub.widget_stacked);
                widget::SetBackground(hub.widget_background);
                widget::SetMonitorChoice(L"main");
                if (hub.taskbar_widget) {
                    widget::Create(g_instance, hwnd, kWidgetClickMessage, g_light_taskbar);
                }
                if (!g_tray_added && !widget::Alive()) {
                    // Restricted desktops (remote sessions, low-integrity launches)
                    // cannot own a tray icon or taskbar widget; open settings as entry point.
                    std::fwprintf(stderr,
                                  L"warning: no tray icon or taskbar widget; opening the settings window\n");
                    ShowSettings();
                }
                EnqueueRefresh(hwnd);
                g_active_poll_sec = hub.poll_interval_sec;
                SetTimer(hwnd, kPollTimer, static_cast<UINT>(g_active_poll_sec) * 1000u, nullptr);
                SetTimer(hwnd, kRepositionTimer, 1000, nullptr);
                return 0;
            }
            if (wparam == kRepositionTimer) {
                if (SnapshotHubSettings().taskbar_widget && widget::Alive()) {
                    widget::Reposition();
                }
                return 0;
            }
            if (wparam == kPollTimer) {
                if (!g_tray_added) g_tray_added = AddTrayIcon(hwnd);
                // WP7: pause background battery polling while the settings window
                // is visible to avoid HID contention and unnecessary mouse wakeups.
                if (g_settings == nullptr || !IsWindowVisible(g_settings)) {
                    EnqueueRefresh(hwnd);
                }
                return 0;
            }
            return 0;
        }
        case kRefreshDoneMessage:
            UpdateDisplay();
            return 0;
        case kTestNotifyMessage: {
            const HubSettings hub = SnapshotHubSettings();
            const std::wstring body =
                L"通知通道正常。当无线外设电量 ≤ " +
                std::to_wstring(hub.low_battery_threshold) +
                L"% 且未充电时，将自动弹出此提醒（免打扰期间静默）。";
            ShowTrayNotification(L"PeriphHub 低电量提醒测试", body,
                                 NIIF_INFO | NIIF_RESPECT_QUIET_TIME);
            return 0;
        }
        case kTrayMessage: {
            if (LOWORD(lparam) == WM_RBUTTONUP) ShowTrayMenu(hwnd);
            if (LOWORD(lparam) == WM_LBUTTONDBLCLK) ShowSettings();
            if (LOWORD(lparam) == NIN_BALLOONUSERCLICK) {
                ShowSettings(g_last_notified_device_key);
            }
            return 0;
        }
        case WM_COMMAND: {
            const UINT cmd = LOWORD(wparam);
            if (cmd >= kMenuDeviceBase && cmd <= kMenuDeviceMax) {
                const std::size_t idx = static_cast<std::size_t>(cmd - kMenuDeviceBase);
                if (idx < g_menu_device_keys.size()) {
                    ShowSettings(g_menu_device_keys[idx]);
                } else {
                    ShowSettings();
                }
                return 0;
            }
            switch (cmd) {
                case kMenuSettings:
                    ShowSettings();
                    return 0;
                case kMenuRefresh:
                    EnqueueRefresh(hwnd);
                    return 0;
                case kMenuWidget: {
                    HubSettings next;
                    {
                        std::lock_guard<std::mutex> lock(g_hub_mutex);
                        g_hub.taskbar_widget = !g_hub.taskbar_widget;
                        next = g_hub;
                    }
                    SaveHubSettings(next);
                    UpdateDisplay();
                    return 0;
                }
                case kMenuQuit:
                    DestroyWindow(hwnd);
                    return 0;
                default:
                    break;
            }
            return 0;
        }
        case WM_DESTROY:
            widget::Destroy();
            if (g_settings != nullptr) {
                DestroyWindow(g_settings);
                g_settings = nullptr;
            }
            RemoveTrayIcon();
            PostQuitMessage(0);
            return 0;
        default:
            break;
    }
    return DefWindowProcW(hwnd, message, wparam, lparam);
}

bool RegisterClasses() {
    HICON app_icon = LoadAppIcon();
    WNDCLASSEXW main_class{};
    main_class.cbSize = sizeof(main_class);
    main_class.lpfnWndProc = MainProc;
    main_class.hInstance = g_instance;
    main_class.hIcon = app_icon;
    main_class.hIconSm = app_icon;
    main_class.lpszClassName = kMainClass;
    if (RegisterClassExW(&main_class) == 0) return false;

    WNDCLASSEXW settings_class{};
    settings_class.cbSize = sizeof(settings_class);
    settings_class.lpfnWndProc = SettingsProc;
    settings_class.hInstance = g_instance;
    settings_class.hIcon = app_icon;
    settings_class.hIconSm = app_icon;
    settings_class.lpszClassName = kSettingsClass;
    settings_class.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    settings_class.hbrBackground = reinterpret_cast<HBRUSH>(COLOR_WINDOW + 1);
    return RegisterClassExW(&settings_class) != 0;
}

// 0 = success, 1 = device found but IO failed, 2 = device not found, 3 = bad argument.
int OpenCliDevice(incott::Device& device) {
    std::wstring error;
    if (!device.Open(error)) {
        std::fwprintf(stderr, L"error: %ls\n", error.c_str());
        return 2;
    }
    return 0;
}

void DumpFrame(const wchar_t* label, const incott::Frame& frame) {
    std::wprintf(L"%ls:", label);
    for (std::size_t i = 0; i < incott::kReportLen; ++i) {
        std::wprintf(L" %02X", static_cast<unsigned>(frame.bytes[i]));
    }
    std::wprintf(L"\n");
}

// Sends one feature request and receives one reply; prints why when it fails.
int Exchange(incott::Device& device, const incott::Frame& request, incott::Frame& reply,
             const wchar_t* what) {
    std::wstring error;
    if (!device.Feature(request.bytes, reply.bytes, error)) {
        std::fwprintf(stderr, L"error: %ls: %ls\n", what, error.c_str());
        return 1;
    }
    return 0;
}

// WP5 真机验收：浅休眠下 0x8F/0x89/0x81 仍被接收器应答（0x81 还回真实 rate 码），
// 只有深命令（0x83/0x85/0x84/0x82/0x86）回空报告/全零。rate.* 走 0x81，无法从
// 自身回包识别睡态，先读 0x83 探活：无回显或 count<1 视为睡着（docs/05 §7/§10）。
bool DeviceAwake(incott::Device& device) {
    incott::Frame reply{};
    for (int attempt = 0; attempt < 3; ++attempt) {
        if (Exchange(device, incott::MakeRead(incott::kCmdDpiTable, nullptr, 0), reply,
                     L"0x83 wake probe") == 0) {
            if (incott::ParseDpiTable(reply).count >= 1) return true;
        }
        Sleep(40);
    }
    return false;
}

std::string Narrow(const std::wstring& value) { return text::ToUtf8(value); }

// Probes one live Incott G23/G24 device without touching other peripherals.
bool TryReadIncottSnapshotOnce(bridge::DeviceSnapshot* out, bool* not_found) {
    if (not_found != nullptr) *not_found = false;
    incott::Device device;
    std::wstring error;
    if (!device.Open(error)) {
        if (not_found != nullptr && error.rfind(L"device not found", 0) == 0) {
            *not_found = true;
        }
        return false;
    }

    incott::Frame model_reply{};
    const incott::Frame model_request = incott::MakeRead(incott::kCmdModel, nullptr, 0);
    if (!device.Feature(model_request.bytes, model_reply.bytes, error)) {
        std::fwprintf(stderr, L"error: %ls\n", error.c_str());
        return false;
    }
    const incott::DeviceInfo info = incott::ParseModel(model_reply, device.IsWireless());

    int battery = -1;
    int charging = -1;
    incott::Frame status_reply{};
    const incott::Frame status_request = incott::MakeRead(incott::kCmdStatus, nullptr, 0);
    if (device.Feature(status_request.bytes, status_reply.bytes, error)) {
        const incott::StatusInfo status = incott::ParseStatus(status_reply);
        battery = status.battery;
        charging = status.charging;
    }

    wchar_t key[32]{};
    swprintf(key, ARRAYSIZE(key), L"incott:%04X", static_cast<unsigned>(device.ProductId()));

    bridge::DeviceSnapshot snapshot;
    snapshot.key = Narrow(key);
    snapshot.name = "Incott " + Narrow(incott::ModelName(info.model));
    snapshot.kind = "mouse";
    snapshot.via = device.IsWireless() ? "2.4 GHz receiver" : "USB";
    snapshot.driver_level = "full";
    snapshot.mode = device.IsWireless() ? "wireless" : "wired";
    snapshot.receiver = Narrow(incott::ReceiverName(info.receiver));
    snapshot.sensor = Narrow(incott::SensorName(info.sensor));
    snapshot.psd = info.psd;
    snapshot.battery = battery;
    snapshot.charging = charging;
    snapshot.connected = true;
    *out = std::move(snapshot);
    return true;
}

bool TryReadIncottSnapshot(bridge::DeviceSnapshot* out, int attempts) {
    if (attempts < 1) attempts = 1;
    for (int i = 0; i < attempts; ++i) {
        bool not_found = false;
        if (TryReadIncottSnapshotOnce(out, &not_found)) return true;
        if (not_found) break;
        if (i + 1 < attempts) Sleep(100);
    }
    return false;
}

// Fills the bridge cache across all supported peripheral backends (upstream probe,
// native Incott driver, Windows Bluetooth PnP, Generic HID, XInput, HeadsetControl)
// and overlays any saved per-device user aliases.
bool RefreshDeviceCache(int attempts) {
    if (attempts < 1) attempts = 1;
    std::vector<bridge::DeviceSnapshot> devices = peripherals::ScanAllDevices(
        [attempts](bridge::DeviceSnapshot* out) {
            return TryReadIncottSnapshot(out, attempts);
        });
    const HubSettings hub = SnapshotHubSettings();
    for (bridge::DeviceSnapshot& d : devices) {
        const auto it = hub.device_aliases.find(d.key);
        if (it != hub.device_aliases.end() && !it->second.empty()) {
            d.name = it->second;
        }
        const drivers::DriverDescriptor* drv =
            drivers::ResolveDriver(drivers::Global(), d.key, &d);
        d.driver = (drv != nullptr) ? drv->name : "none";
    }
    const bool any = !devices.empty();
    bridge::SetDevices(std::move(devices));
    return any;
}

// Full read-only snapshot: 0x89 status, 0x83 DPI table, 0x85 sleep/debounce, 0x82 active DPI.
int RunInfo() {
    incott::Device device;
    if (const int rc = OpenCliDevice(device)) return rc;

    incott::Frame status_frame{};
    if (const int rc = Exchange(device, incott::MakeRead(incott::kCmdStatus, nullptr, 0),
                                status_frame, L"0x89 status"))
        return rc;
    const incott::StatusInfo status = incott::ParseStatus(status_frame);
    DumpFrame(L"raw89", status_frame);

    incott::Frame table_frame{};
    if (const int rc = Exchange(device, incott::MakeRead(incott::kCmdDpiTable, nullptr, 0),
                                table_frame, L"0x83 dpi table"))
        return rc;
    const incott::DpiTable table = incott::ParseDpiTable(table_frame);
    DumpFrame(L"raw83", table_frame);

    const std::uint8_t sub_sleep[1] = {0x03};
    const std::uint8_t sub_debounce[1] = {0x01};
    incott::Frame sleep_frame{};
    if (const int rc = Exchange(device, incott::MakeRead(incott::kCmdMisc, sub_sleep, 1),
                                sleep_frame, L"0x85 sleep"))
        return rc;
    DumpFrame(L"raw85/3", sleep_frame);
    incott::Frame debounce_frame{};
    if (const int rc = Exchange(device, incott::MakeRead(incott::kCmdMisc, sub_debounce, 1),
                                debounce_frame, L"0x85 debounce"))
        return rc;
    DumpFrame(L"raw85/1", debounce_frame);

    // Read every stored level so the active value can be cross-checked against 0x83.
    int levels[incott::kDpiCountMax] = {-1, -1, -1, -1, -1, -1};
    const int read_count = (table.count >= 1 && table.count <= incott::kDpiCountMax)
                               ? table.count
                               : incott::kDpiCountMax;
    for (int i = 0; i < read_count; ++i) {
        const std::uint8_t slot[1] = {static_cast<std::uint8_t>(i)};
        incott::Frame dpi_frame{};
        if (Exchange(device, incott::MakeRead(incott::kCmdDpi, slot, 1), dpi_frame, L"0x82 dpi")
            != 0) {
            break;
        }
        DumpFrame(L"raw82", dpi_frame);
        levels[i] = incott::DpiFromReply(dpi_frame, i);
    }
    const int active_dpi =
        (table.active >= 0 && table.active < read_count) ? levels[table.active] : -1;

    std::wprintf(L"info: battery=%d%% charging=%d rate=%dHz levels=%d active=%d dpi=%d sleep=%ds debounce=%d\n",
                 status.battery, status.charging, incott::RateFromCode(status.rate_code), table.count,
                 table.active, active_dpi, incott::SleepSeconds(sleep_frame, 0x03),
                 incott::DebounceValue(debounce_frame, 0x01));
    std::wprintf(L"dpi-levels:");
    for (int i = 0; i < read_count; ++i) {
        std::wprintf(L" [%d]=%d", i, levels[i]);
    }
    std::wprintf(L"\n");
    return 0;
}

// Writes one DPI level (both axes) to the active slot, then reads it back.
int RunDpi(int dpi) {    if (!incott::DpiValid(dpi)) {
        std::fwprintf(stderr, L"error: invalid DPI %d: allowed 50..32000 in steps of 50\n", dpi);
        return 3;
    }

    incott::Device device;
    if (const int rc = OpenCliDevice(device)) return rc;

    // The firmware is only told the slot; the active one comes from 0x83 (0x89 is a fallback).
    int slot = -1;
    int count = -1;
    incott::Frame table_frame{};
    if (Exchange(device, incott::MakeRead(incott::kCmdDpiTable, nullptr, 0), table_frame,
                 L"0x83 dpi table")
        == 0) {
        const incott::DpiTable table = incott::ParseDpiTable(table_frame);
        if (table.count >= 1 && table.count <= incott::kDpiCountMax && table.active >= 0 &&
            table.active < table.count) {
            slot = table.active;
            count = table.count;
        }
    }
    if (slot < 0) {
        incott::Frame status_frame{};
        if (Exchange(device, incott::MakeRead(incott::kCmdStatus, nullptr, 0), status_frame,
                     L"0x89 status")
            == 0) {
            const incott::StatusInfo status = incott::ParseStatus(status_frame);
            if (status.dpi_index >= 0 && status.dpi_index < incott::kDpiCountMax) {
                slot = status.dpi_index;
            }
        }
    }
    if (slot < 0) {
        std::fwprintf(stderr, L"error: cannot determine the active DPI slot; nothing was written\n");
        return 1;
    }

    const incott::DpiWrite write = incott::MakeDpiWrite(slot, incott::DpiToHw(dpi), incott::DpiToHw(dpi));
    for (int i = 0; i < write.count; ++i) {
        incott::Frame echo{};
        std::wstring error;
        if (!device.Feature(write.frames[i].bytes, echo.bytes, error)) {
            std::fwprintf(stderr, L"error: 0x02 write failed (slot=%d): %ls\n", slot + 1,
                          error.c_str());
            return 1;
        }
    }
    // The firmware applies the level asynchronously; the official caller waits 60 ms.
    // A read issued too early comes back as an all-zero report, so retry a few times.
    Sleep(60);

    const std::uint8_t read_slot[1] = {static_cast<std::uint8_t>(slot)};
    int readback = -1;
    for (int attempt = 0; attempt < 3; ++attempt) {
        incott::Frame reply{};
        if (const int rc = Exchange(device, incott::MakeRead(incott::kCmdDpi, read_slot, 1), reply,
                                    L"0x82 readback")
            != 0)
            return rc;
        DumpFrame(L"raw82", reply);
        if (incott::CheckEcho(reply, incott::kCmdDpi)) {
            readback = incott::DpiFromReply(reply, slot);
            break;
        }
        Sleep(50);
    }
    const bool ok = readback == dpi;
    if (count > 0) {
        std::wprintf(L"dpi: slot=%d/%d wrote=%d readback=%d %ls\n", slot + 1, count, dpi, readback,
                     ok ? L"ok" : L"MISMATCH");
    } else {
        std::wprintf(L"dpi: slot=%d wrote=%d readback=%d %ls\n", slot + 1, dpi, readback,
                     ok ? L"ok" : L"MISMATCH");
    }
    return ok ? 0 : 1;
}

// Reads all six bindings into the bridge cache. Any unanswered slot clears the
// cache so the page reports the device instead of showing stale values.
bool RefreshKeyCache() {
    incott::Device device;
    std::wstring error;
    if (!device.Open(error)) {
        bridge::ClearKeys();
        return false;
    }
    std::vector<bridge::KeySnapshot> keys;
    for (int i = 0; i < incott::kKeyCount; ++i) {
        const int matrix = incott::kKeyMatrix[i];
        std::uint32_t code = 0;
        bool answered = false;
        for (int attempt = 0; attempt < 3; ++attempt) {
            incott::Frame reply{};
            if (!device.Feature(incott::MakeKeyRead(matrix).bytes, reply.bytes, error)) break;
            if (incott::ParseKeyReply(reply, matrix, &code)) {
                answered = true;
                break;
            }
            Sleep(50);
        }
        if (!answered) {
            bridge::ClearKeys();
            return false;
        }
        bridge::KeySnapshot snapshot;
        snapshot.index = i;
        snapshot.matrix = matrix;
        snapshot.code = code;
        snapshot.binding = incott::DescribeBinding(code);
        snapshot.kind = incott::KeyKindName(incott::DecodeBinding(code).kind);
        keys.push_back(std::move(snapshot));
    }
    bridge::SetKeys(std::move(keys));
    return true;
}

// 0 = written and read back, 1 = IO error, 2 = readback mismatch. The firmware
// applies a binding asynchronously, so the first read after the write can be zero.
// `readback` is required: silently skipping it would look like a successful write.
int WriteKey(incott::Device& device, int index, std::uint32_t code, std::uint32_t* readback,
             bool dump) {
    if (readback == nullptr) {
        std::fwprintf(stderr, L"error: WriteKey needs a readback buffer\n");
        return 1;
    }
    const int matrix = incott::kKeyMatrix[index];
    const incott::Frame request = incott::MakeKeyWrite(matrix, code);
    if (dump) DumpFrame(L"req06", request);
    std::wstring error;
    incott::Frame echo{};
    if (!device.Feature(request.bytes, echo.bytes, error)) {
        std::fwprintf(stderr, L"error: 0x06 write failed (matrix=%d): %ls\n", matrix,
                      error.c_str());
        return 1;
    }
    *readback = 0xFFFFFFFFu;
    // The firmware applies writes asynchronously; measured up to ~300 ms, and the
    // first read after a write is always all zero.
    for (int attempt = 0; attempt < 6; ++attempt) {
        Sleep(attempt == 0 ? 60 : 100);
        incott::Frame reply{};
        if (!device.Feature(incott::MakeKeyRead(matrix).bytes, reply.bytes, error)) {
            std::fwprintf(stderr, L"error: 0x86 readback failed: %ls\n", error.c_str());
            return 1;
        }
        if (dump) DumpFrame(L"raw86", reply);
        std::uint32_t value = 0;
        if (incott::ParseKeyReply(reply, matrix, &value)) {
            *readback = value;
            break;
        }
    }
    return *readback == code ? 0 : 2;
}

// ---- Macro library file (%LOCALAPPDATA%\PeriphHub\macros.ini, UTF-16) -----

std::wstring MacroLibraryPath() {
    const std::wstring dir = AppDataDir();
    return dir.empty() ? std::wstring() : (dir + L"\\macros.ini");
}

std::wstring PortableMacroLibraryPath() {
    wchar_t override_dir[MAX_PATH] = {};
    const DWORD override_len =
        GetEnvironmentVariableW(L"PERIPHHUB_DATA_DIR", override_dir, MAX_PATH);
    if (override_len > 0 && override_len < MAX_PATH) {
        return {};
    }
    return ExecutableDirectory() + L"\\macros.ini";
}

std::wstring AliasesFilePath() {
    const std::wstring dir = AppDataDir();
    return dir.empty() ? std::wstring() : (dir + L"\\aliases.json");
}

std::wstring PortableAliasesFilePath() {
    wchar_t override_dir[MAX_PATH] = {};
    const DWORD override_len =
        GetEnvironmentVariableW(L"PERIPHHUB_DATA_DIR", override_dir, MAX_PATH);
    if (override_len > 0 && override_len < MAX_PATH) {
        return {};
    }
    return ExecutableDirectory() + L"\\aliases.json";
}

std::wstring WidenUtf8(const std::string& value) { return text::ToWide(value); }

bool LoadMacroLibrary(incott::MacroStore& store, std::wstring* error) {
    error->clear();
    const std::wstring primary = MacroLibraryPath();
    const std::wstring portable = PortableMacroLibraryPath();
    if (primary.empty() && portable.empty()) return true;

    std::wstring chosen;
    WIN32_FILE_ATTRIBUTE_DATA primary_info{};
    WIN32_FILE_ATTRIBUTE_DATA portable_info{};
    const bool has_primary =
        !primary.empty() && GetFileAttributesExW(primary.c_str(), GetFileExInfoStandard, &primary_info);
    const bool has_portable =
        !portable.empty() && GetFileAttributesExW(portable.c_str(), GetFileExInfoStandard, &portable_info);

    if (has_primary && has_portable) {
        chosen = (CompareFileTime(&portable_info.ftLastWriteTime, &primary_info.ftLastWriteTime) > 0)
                     ? portable
                     : primary;
    } else if (has_portable) {
        chosen = portable;
    } else if (has_primary) {
        chosen = primary;
    } else {
        return true;  // first run: nothing stored yet
    }

    std::string raw;
    bool missing = false;
    if (!ReadAllBytes(chosen, &raw, error, &missing)) {
        if (missing) {
            error->clear();
            return true;
        }
        return false;
    }
    std::wstring text;
    if (raw.size() >= 2 && static_cast<unsigned char>(raw[0]) == 0xFF &&
        static_cast<unsigned char>(raw[1]) == 0xFE) {
        text.assign(reinterpret_cast<const wchar_t*>(raw.data() + 2), (raw.size() - 2) / 2);
    } else {
        text = WidenUtf8(raw);
    }
    // Deserialize is atomic: it either commits the whole file or leaves the store
    // untouched. Only copy it into the live store after a successful parse.
    incott::MacroStore loaded;
    std::wstring parse_error;
    if (!loaded.Deserialize(text, &parse_error)) {
        *error = parse_error.empty() ? L"macro library is corrupt" : parse_error;
        return false;
    }
    store = loaded;
    return true;
}

bool SaveMacroLibrary(const incott::MacroStore& store, std::wstring* error) {
    const std::wstring primary = MacroLibraryPath();
    const std::wstring portable = PortableMacroLibraryPath();
    if (primary.empty() && portable.empty()) {
        *error = L"LOCALAPPDATA is not set";
        return false;
    }
    const std::wstring text = store.Serialize();
    std::string bytes;
    bytes.reserve(text.size() * 2 + 2);
    bytes.push_back(static_cast<char>(0xFF));
    bytes.push_back(static_cast<char>(0xFE));
    bytes.append(reinterpret_cast<const char*>(text.data()), text.size() * 2);
    if (!primary.empty() && WriteAllBytesAtomic(primary, bytes, error)) {
        return true;
    }
    std::wstring fallback_err;
    if (!portable.empty() && WriteAllBytesAtomic(portable, bytes, &fallback_err)) {
        error->clear();
        return true;
    }
    if (error->empty()) *error = fallback_err;
    return false;
}

// 0 = uploaded, 1 = IO error, 2 = encoding error (see code). The vendor order is
// one feature header, 40 ms, then the 32-byte interrupt OUT chunk, ten times.
int UploadMacro(incott::Device& device, int buffer, const incott::Macro& macro,
                incott::MacroError* code, bool dump = true) {
    std::uint8_t payload[incott::kMacroPayload] = {};
    *code = incott::EncodeMacro(buffer, macro.loopType, macro.cycles, macro.uid, macro.actions,
                                payload);
    if (*code != incott::MacroError::kOk) return 2;

    std::wstring error;
    for (int chunk = 0; chunk < incott::kMacroChunks; ++chunk) {
        const incott::Frame header = incott::MakeMacroChunkHeader(chunk, buffer);
        if (dump) DumpFrame(L"req07", header);
        incott::Frame echo{};
        if (!device.Feature(header.bytes, echo.bytes, error)) {
            std::fwprintf(stderr, L"error: 0x07 header chunk %d failed: %ls\n", chunk,
                          error.c_str());
            return 1;
        }
        Sleep(40);
        std::uint8_t report[incott::kMacroChunkBytes + 1] = {};
        report[0] = static_cast<std::uint8_t>(incott::kReportId);
        std::memcpy(report + 1, payload + chunk * incott::kMacroChunkBytes,
                    incott::kMacroChunkBytes);
        if (!device.Output(report, sizeof(report), error)) {
            std::fwprintf(stderr, L"error: macro chunk %d write failed: %ls\n", chunk,
                          error.c_str());
            return 1;
        }
    }
    return 0;
}

// Chinese label for a key slot; the wire order lives in incott::kKeyMatrix.
const wchar_t* KeySlotLabel(int index) {
    static const wchar_t* const kLabels[incott::kKeyCount] = {L"左键", L"右键", L"滚轮",
                                                              L"前侧键", L"后侧键", L"DPI键"};
    if (index < 0 || index >= incott::kKeyCount) return L"?";
    return kLabels[index];
}

// Reads all six bindings: raw 0x86 reply + decoded value for each slot (retrying up to 3x per key).
int RunKeys() {
    incott::Device device;
    if (const int rc = OpenCliDevice(device)) return rc;

    int failures = 0;
    for (int i = 0; i < incott::kKeyCount; ++i) {
        const int matrix = incott::kKeyMatrix[i];
        incott::Frame reply{};
        std::uint32_t code = 0;
        int rc = 1;
        bool parsed = false;
        for (int attempt = 0; attempt < 3; ++attempt) {
            if (attempt > 0) Sleep(60);
            rc = Exchange(device, incott::MakeKeyRead(matrix), reply, L"0x86 key");
            if (rc == 0 && incott::ParseKeyReply(reply, matrix, &code)) {
                parsed = true;
                break;
            }
        }
        if (rc != 0) return rc;
        DumpFrame(L"raw86", reply);
        if (!parsed) {
            std::wprintf(L"key[%d] %ls matrix=%d ERROR: bad reply\n", i, KeySlotLabel(i), matrix);
            ++failures;
            continue;
        }
        const std::string text = incott::DescribeBinding(code);
        std::wprintf(L"key[%d] %ls matrix=%d code=0x%08X %hs\n", i, KeySlotLabel(i), matrix,
                     static_cast<unsigned>(code), text.c_str());
    }
    return failures == 0 ? 0 : 1;
}

// Writes one binding, waits 60 ms like the vendor adapter, then reads it back.
int RunSetKey(int index, const std::wstring& name) {
    if (index < 0 || index >= incott::kKeyCount) {
        std::fwprintf(stderr,
                      L"error: key index must be 0..%d (0 left, 1 right, 2 middle, 3 forward, "
                      L"4 back, 5 dpi)\n",
                      incott::kKeyCount - 1);
        return 3;
    }
    const std::string narrow_name = Narrow(name);
    std::uint32_t code = 0;
    if (!incott::EncodeBinding(narrow_name.c_str(), &code)) {
        std::fwprintf(stderr,
                      L"error: unknown key \"%ls\" (KeyA, F1, Digit1, Ctrl+KeyC, LButton, "
                      L"Mouse:Back, Dpi:Cycle, Media:Mute, Off)\n",
                      name.c_str());
        return 3;
    }

    incott::Device device;
    if (const int rc = OpenCliDevice(device)) return rc;

    std::uint32_t readback = 0xFFFFFFFFu;
    const int rc = WriteKey(device, index, code, &readback);
    const bool ok = rc == 0;
    std::wprintf(L"set-key: index=%d %ls matrix=%d wrote=0x%08X readback=0x%08X %ls\n", index,
                 KeySlotLabel(index), incott::kKeyMatrix[index], static_cast<unsigned>(code),
                 static_cast<unsigned>(readback), ok ? L"ok" : L"MISMATCH");
    return ok ? 0 : 1;
}

// Uploads a Studio macro file to firmware buffer `key_index`, then (optionally)
// binds the key to it with 0x06. The firmware only plays the buffer whose id
// equals the key index, so CLI users pass a key index rather than a library slot.
int RunMacroUpload(int key_index, const std::wstring& path, bool apply) {
    if (key_index < 0 || key_index >= incott::kKeyCount) {
        std::fwprintf(stderr, L"error: key index must be 0..%d (macro buffer = key index)\n",
                      incott::kKeyCount - 1);
        return 3;
    }
    std::string text;
    std::wstring error;
    if (!ReadAllBytes(path, &text, &error)) {
        std::fwprintf(stderr, L"error: %ls\n", error.c_str());
        return 3;
    }
    if (text.size() >= 3 && static_cast<unsigned char>(text[0]) == 0xEF &&
        static_cast<unsigned char>(text[1]) == 0xBB &&
        static_cast<unsigned char>(text[2]) == 0xBF) {
        text.erase(0, 3);  // UTF-8 BOM from a text editor
    }
    incott::Macro macro;
    std::string parse_error;
    if (!incott::ParseMacroJson(text, &macro, &parse_error)) {
        std::fwprintf(stderr, L"error: %hs\n", parse_error.c_str());
        return 3;
    }

    incott::Device device;
    if (const int rc = OpenCliDevice(device)) return rc;
    incott::MacroError code = incott::MacroError::kOk;
    const int rc = UploadMacro(device, key_index, macro, &code);
    if (rc == 2) {
        std::fwprintf(stderr, L"error: %ls\n",
                      WidenUtf8(incott::MacroErrorText(code)).c_str());
        return 3;
    }
    if (rc != 0) return 1;
    std::wprintf(L"macro-upload: buffer=%d actions=%d loop=%d cycles=%d chunks=%d ok\n",
                 key_index, static_cast<int>(macro.actions.size()), macro.loopType,
                 macro.cycles, incott::kMacroChunks);
    if (!apply) return 0;

    const std::uint32_t binding =
        (static_cast<std::uint32_t>(key_index) << 16) | 0x0009u;
    std::uint32_t readback = 0xFFFFFFFFu;
    const int write_rc = WriteKey(device, key_index, binding, &readback);
    std::wprintf(L"macro-apply: index=%d %ls matrix=%d binding=0x%08X readback=0x%08X %ls\n",
                 key_index, KeySlotLabel(key_index), incott::kKeyMatrix[key_index],
                 static_cast<unsigned>(binding), static_cast<unsigned>(readback),
                 write_rc == 0 ? L"ok" : L"MISMATCH");
    return write_rc == 0 ? 0 : 1;
}

std::string MacroListJson() {
    std::string out = "[";
    bool first = true;
    for (int i = 0; i < incott::MacroStore::kSlotCount; ++i) {
        const incott::MacroSlot& slot = g_macros.At(i);
        if (!slot.defined) continue;
        if (!first) out += ",";
        first = false;
        out += "{\"slot\":" + std::to_string(i);
        out += ",\"name\":" + bridge::JsonQuote(Narrow(slot.macro.name));
        out += ",\"loop\":" + std::to_string(slot.macro.loopType);
        out += ",\"cycles\":" + std::to_string(slot.macro.cycles);
        out += ",\"actions\":" + std::to_string(slot.macro.actions.size());
        // Key indices (0..5) whose firmware binding points at this library slot,
        // so the UI can show which button plays a macro without a keys round-trip.
        out += ",\"bind\":[";
        bool firstBind = true;
        for (int key = 0; key < incott::kKeyCount; ++key) {
            if (g_macros.Binding(key) != i) continue;
            if (!firstBind) out += ",";
            firstBind = false;
            out += std::to_string(key);
        }
        out += "]";
        out += "}";
    }
    out += "]";
    return out;
}

using bridge::ArgState;
using bridge::OptBoolArg;
using bridge::OptIntArg;

std::string HandleDpiRequest(const bridge::Request& request) {
    const json::Value* args = request.args;
    const std::string& command = request.cmd;

    auto opt_int = [&](const char* name, int* out) { return OptIntArg(args, name, out); };

    // 0x83 is a deep command: a light-sleeping device returns an empty report
    // (docs/05 §7), and the first read after a 0x03 write can be stale/empty
    // too — retry a few times instead of trusting one frame. count 1..6 is the
    // validity window; active is passed through for the callers to compare.
    auto read_dpi_table = [&](incott::Device& device, incott::DpiTable* out) {
        for (int attempt = 0; attempt < 3; ++attempt) {
            incott::Frame reply{};
            if (Exchange(device, incott::MakeRead(incott::kCmdDpiTable, nullptr, 0), reply,
                         L"0x83 dpi table") == 0) {
                const incott::DpiTable table = incott::ParseDpiTable(reply);
                if (table.count >= 1 && table.count <= incott::kDpiCountMax) {
                    *out = table;
                    return true;
                }
            }
            Sleep(50);
        }
        return false;
    };

    if (command == "dpi.get") {
        incott::Device device;
        std::wstring error;
        if (!device.Open(error)) return bridge::Failure(request, "device not found");

        incott::DpiTable table;
        if (!read_dpi_table(device, &table)) {
            return bridge::Failure(request, "failed to read dpi table (move mouse to wake it)");
        }

        // WP3 实测形状（docs/05 §11）：0x82 每档只有一个值——最后一帧 0x02 写入的
        // 值。官方/本桥写序 X 先 Y 后，读回即 Y（X=Y 档它就是真值）；X 无法独立
        // 回读，恒为 null 并标注 unverified，绝不把单值同时当作 X 展示。单档读
        // 失败该档回 null，不再回填 800；active 超出 0..count-1 时回 null。
        const bool active_valid = (table.active >= 0 && table.active < table.count);
        std::string json = "{\"count\":" + std::to_string(table.count) +
                           ",\"active\":" + (active_valid ? std::to_string(table.active) : "null") +
                           ",\"levels\":[";
        for (int i = 0; i < table.count; ++i) {
            if (i != 0) json += ",";
            const std::uint8_t slot[1] = {static_cast<std::uint8_t>(i)};
            int dpi_val = -1;
            for (int attempt = 0; attempt < 3 && dpi_val < 0; ++attempt) {
                incott::Frame reply{};
                if (Exchange(device, incott::MakeRead(incott::kCmdDpi, slot, 1), reply, L"0x82 read") == 0) {
                    dpi_val = incott::DpiFromReply(reply, i);
                }
                if (dpi_val < 0) Sleep(40);
            }
            if (dpi_val < 0) {
                json += "null";
            } else {
                json += "{\"dpi\":" + std::to_string(dpi_val) +
                        ",\"x\":null,\"y\":" + std::to_string(dpi_val) +
                        ",\"unverified\":[\"x\"]}";
            }
        }
        json += "],\"axisNote\":\"0x82 returns one value per level: the last written axis "
                "(Y under the official X-then-Y order); x cannot be read back and stays null\"}";
        return bridge::Ok(request, json);
    }

    if (command == "dpi.set") {
        int legacy_dpi = 0;
        int dpi_x = -1;
        int dpi_y = -1;
        int slot = -1;
        const ArgState dpi_state = opt_int("dpi", &legacy_dpi);
        const ArgState x_state = opt_int("dpiX", &dpi_x);
        const ArgState y_state = opt_int("dpiY", &dpi_y);
        const ArgState slot_state = opt_int("slot", &slot);
        if (dpi_state == ArgState::kBad) return bridge::Failure(request, "dpi must be an integer");
        if (x_state == ArgState::kBad) return bridge::Failure(request, "dpiX must be an integer");
        if (y_state == ArgState::kBad) return bridge::Failure(request, "dpiY must be an integer");
        if (slot_state == ArgState::kBad) return bridge::Failure(request, "slot must be an integer");
        const bool has_dpi = (dpi_state == ArgState::kOk);
        const bool has_x = (x_state == ArgState::kOk);
        const bool has_y = (y_state == ArgState::kOk);
        if (!has_dpi && !has_x && !has_y) {
            return bridge::Failure(request, "dpi.set needs args.dpi, or args.dpiX + args.dpiY");
        }
        // F2-3：禁止把单值 dpi 与分轴 dpiX/dpiY 混传，避免静默忽略其中一方。
        if (has_dpi && (has_x || has_y)) {
            return bridge::Failure(request, "dpi.set cannot mix args.dpi with args.dpiX/args.dpiY");
        }
        // WP3：分轴写入必须两轴齐备。0x82 读不到「另一轴」的当前值，缺一半
        // 不能靠读硬件补齐，直接拒绝而不是猜。
        if ((has_x || has_y) && (!has_x || !has_y)) {
            return bridge::Failure(request, "dpi.set needs both dpiX and dpiY for a split-axis write");
        }
        const int x_dpi = has_x ? dpi_x : legacy_dpi;
        const int y_dpi = has_y ? dpi_y : legacy_dpi;
        if (!incott::DpiValid(x_dpi) || !incott::DpiValid(y_dpi)) {
            return bridge::Failure(request, "dpi must be 50..32000 in steps of 50");
        }
        if (slot_state != ArgState::kOk || slot < 0) {
            // WP3-4：缺失/负数 slot 直接报错，不再回退当前档/0。
            return bridge::Failure(request, "dpi.set needs args.slot (0..5)");
        }

        incott::Device device;
        std::wstring error;
        if (!device.Open(error)) return bridge::Failure(request, "device not found");

        // 写前探活兼取真实档数：slot 超出设备实际档数明确报错（WP3-4）。
        incott::DpiTable table;
        if (!read_dpi_table(device, &table)) {
            return bridge::Failure(request, "failed to read dpi table (move mouse to wake it)");
        }
        if (slot >= table.count) {
            return bridge::Failure(request, "slot " + std::to_string(slot) + " is out of range: device stores " +
                                                std::to_string(table.count) + " dpi levels");
        }

        const incott::DpiWrite write = incott::MakeDpiWrite(slot, incott::DpiToHw(x_dpi), incott::DpiToHw(y_dpi));
        for (int i = 0; i < write.count; ++i) {
            if (i != 0) Sleep(100);  // docs/05 §6：X 帧(axis=1) 与 Y 帧(axis=2) 之间 100ms
            incott::Frame echo{};
            std::wstring write_error;
            if (!device.Feature(write.frames[i].bytes, echo.bytes, write_error)) {
                return bridge::Failure(request, "0x02 write failed");
            }
        }
        Sleep(60);

        // WP3 实测：0x82 只暴露最后一帧的值——单帧(axis=0)校验同值，两帧写序
        // (X 后 Y)只能校验 Y；X 读不到，data 里回 null 并标注 unverified。
        const int expected = incott::DpiExpectedReadback(write, x_dpi, y_dpi);
        const std::uint8_t read_slot[1] = {static_cast<std::uint8_t>(slot)};
        int readback = -1;
        for (int attempt = 0; attempt < 4 && readback != expected; ++attempt) {
            incott::Frame reply{};
            if (Exchange(device, incott::MakeRead(incott::kCmdDpi, read_slot, 1), reply, L"0x82 readback") == 0) {
                readback = incott::DpiFromReply(reply, slot);
            }
            if (readback != expected) Sleep(50);
        }
        if (readback < 0) {
            return bridge::Failure(request, "0x82 readback failed (move the mouse to wake it)");
        }
        if (readback != expected) {
            // 分轴写序下期望值是最后写入的 Y 帧：文案标 dpiY=，避免读成两轴共值。
            const std::string wrote = (write.count == 2)
                                          ? "dpiY=" + std::to_string(expected)
                                          : std::to_string(expected);
            return bridge::Failure(request, "readback mismatch: wrote " + wrote +
                                                " got " + std::to_string(readback));
        }
        return bridge::Ok(request, command_json::BuildDpiSetData(slot, readback, write.count == 2));
    }

    if (command == "dpi.stage") {
        int active = -1;
        int count = -1;
        const ArgState active_state = opt_int("active", &active);
        const ArgState count_state = opt_int("count", &count);
        if (active_state == ArgState::kBad) {
            return bridge::Failure(request, "active must be an integer");
        }
        if (count_state == ArgState::kBad) {
            return bridge::Failure(request, "count must be an integer");
        }
        if (active_state != ArgState::kOk) {
            return bridge::Failure(request, "dpi.stage needs args.active");
        }
        if (count_state != ArgState::kOk) {
            return bridge::Failure(request, "dpi.stage needs args.count (1..6)");
        }
        // WP3-3：count/active 非法明确报错，不再静默改成 6/0。
        if (!incott::DpiStageValid(count, active)) {
            return bridge::Failure(request, "invalid dpi.stage args: count must be 1..6 and active 0..count-1");
        }

        incott::Device device;
        std::wstring error;
        if (!device.Open(error)) return bridge::Failure(request, "device not found");

        // 写前探活 + 留基线（失败即设备睡着/拔出，不写 0x03）。
        incott::DpiTable before;
        if (!read_dpi_table(device, &before)) {
            return bridge::Failure(request, "failed to read dpi table (move mouse to wake it)");
        }

        const std::uint8_t payload[2] = {static_cast<std::uint8_t>(count),
                                         static_cast<std::uint8_t>(active)};
        const incott::Frame frame = incott::MakeWrite(incott::kCmdDpiTable, payload, 2);
        incott::Frame echo{};
        std::wstring write_error;
        if (!device.Feature(frame.bytes, echo.bytes, write_error)) {
            return bridge::Failure(request, "0x03 write failed");
        }
        Sleep(60);

        // WP3-3：写后读 0x83 回读比对 count/active。写后首读可能拿到旧表（合法
        // 回显但仍是写前值）或空报告——与 0x06/0x02 写后回读同类（docs/05 §6/§11，
        // 实测最长 ~300ms），不等于期望就重试，最终不一致才 ok:false。
        incott::DpiTable after;
        bool read_ok = true;
        bool matched = false;
        for (int attempt = 0; attempt < 4; ++attempt) {
            if (!read_dpi_table(device, &after)) {
                read_ok = false;
                break;
            }
            if (after.count == count && after.active == active) {
                matched = true;
                break;
            }
            Sleep(50);
        }
        if (!read_ok) {
            return bridge::Failure(request, "0x83 readback failed (move the mouse to wake it)");
        }
        if (!matched) {
            return bridge::Failure(request, "readback mismatch: wrote count=" + std::to_string(count) +
                                                " active=" + std::to_string(active) + ", device kept count=" +
                                                std::to_string(after.count) + " active=" +
                                                std::to_string(after.active));
        }
        return bridge::Ok(request, command_json::BuildDpiStageData(after.active, after.count));
    }

    return bridge::Failure(request, "unknown dpi command");
}

std::string HandleHardwareExtendedRequest(const bridge::Request& request) {
    const json::Value* args = request.args;
    const std::string& command = request.cmd;

    auto opt_int = [&](const char* name, int* out) { return OptIntArg(args, name, out); };
    auto opt_bool = [&](const char* name, bool* out) { return OptBoolArg(args, name, out); };
    auto open_device = [](incott::Device* dev) {
        std::wstring error;
        return dev->Open(error);
    };

    if (command == "rate.get") {
        incott::Device device;
        if (!open_device(&device)) return bridge::Failure(request, "device not found");

        // WP5 验收修复：浅休眠下 0x81 仍被应答并返回真实 rate 码，睡态必须靠 0x83
        // 探活识别；睡着时同 misc.get/sensor.get 形状回 null，不假显 1000 Hz。
        if (!DeviceAwake(device)) {
            return bridge::Ok(request, "{\"code\":null,\"hz\":null}");
        }
        incott::Frame reply{};
        int rate_code = -1;
        for (int attempt = 0; attempt < 3; ++attempt) {
            if (Exchange(device, incott::MakeRead(0x01, nullptr, 0), reply, L"0x81 rate") == 0) {
                if (reply.bytes[0] == incott::kReportId && reply.bytes[1] == 0x81) {
                    rate_code = reply.bytes[2];
                    break;
                }
            }
            Sleep(40);
        }
        // WP5-5 / F1-4: an unread or unmapped rate code reports null for both code and hz.
        const int hz = incott::RateFromCode(rate_code);
        if (hz <= 0) {
            return bridge::Ok(request, "{\"code\":null,\"hz\":null}");
        }
        return bridge::Ok(request, "{\"code\":" + std::to_string(rate_code) +
                                       ",\"hz\":" + std::to_string(hz) + "}");
    }

    if (command == "rate.set") {
        int hz = 0;
        int code = -1;
        const ArgState hz_state = opt_int("hz", &hz);
        const ArgState code_state = opt_int("code", &code);
        if (hz_state == ArgState::kBad) {
            return bridge::Failure(request, "hz must be an integer");
        }
        if (code_state == ArgState::kBad) {
            return bridge::Failure(request, "code must be an integer");
        }
        if (hz_state == ArgState::kOk) {
            switch (hz) {
                case 1000: code = 0; break;
                case 500:  code = 1; break;
                case 250:  code = 2; break;
                case 125:  code = 3; break;
                case 8000: code = 4; break;
                case 4000: code = 5; break;
                case 2000: code = 6; break;
                default:   return bridge::Failure(request, "invalid hz");
            }
        } else if (code_state != ArgState::kOk || code < 0 || code > 6) {
            return bridge::Failure(request, "rate.set needs args.hz or args.code");
        }

        incott::Device device;
        if (!open_device(&device)) return bridge::Failure(request, "device not found");

        // WP5 验收修复：浅休眠下 0x01 写/0x81 读仍被应答（回读必然「成功」，却证明
        // 不了鼠标侧状态），写前先 0x83 探活，睡着直接拒绝、不写 0x01。
        if (!DeviceAwake(device)) {
            return bridge::Failure(request, "device sleeping (move the mouse to wake it)");
        }

        // WP5-4: >1000 Hz requires the 8K receiver on the 2.4G dongle. A 1K
        // receiver asking for 2000/4000/8000 gets an explicit error, not the old
        // silent clamp to 1000 Hz.
        if (code > 3) {
            incott::Frame model_reply{};
            std::wstring model_error;
            if (!device.Feature(incott::MakeRead(incott::kCmdModel, nullptr, 0).bytes,
                                model_reply.bytes, model_error)) {
                return bridge::Failure(request, "0x8F read failed; cannot verify the receiver");
            }
            const incott::DeviceInfo info = incott::ParseModel(model_reply, device.IsWireless());
            if (!incott::HighRateAllowed(info.receiver, device.IsWireless())) {
                return bridge::Failure(request,
                                       "2000 Hz and above need the 8K receiver on 2.4G wireless");
            }
        }

        const std::uint8_t payload[1] = {static_cast<std::uint8_t>(code)};
        const incott::Frame frame = incott::MakeWrite(0x01, payload, 1);
        incott::Frame echo{};
        std::wstring write_error;
        if (!device.Feature(frame.bytes, echo.bytes, write_error)) {
            return bridge::Failure(request, "0x01 write failed");
        }
        Sleep(60);

        incott::Frame reply{};
        int readback_code = -1;
        for (int attempt = 0; attempt < 4; ++attempt) {
            if (Exchange(device, incott::MakeRead(0x01, nullptr, 0), reply, L"0x81 readback") == 0) {
                if (reply.bytes[0] == incott::kReportId && reply.bytes[1] == 0x81) {
                    readback_code = reply.bytes[2];
                    if (readback_code == code) break;
                }
            }
            Sleep(50);
        }
        // WP5-4: no fallback to the requested value — a failed or mismatching
        // readback is an error, and data only ever carries the verified value.
        if (readback_code < 0) {
            return bridge::Failure(request, "0x81 readback failed (move the mouse to wake it)");
        }
        const int readback_hz = incott::RateFromCode(readback_code);
        const int target_hz = incott::RateFromCode(code);
        if (readback_code != code) {
            return bridge::Failure(request, "readback mismatch: wrote code " + std::to_string(code) +
                                                " (" + std::to_string(target_hz) +
                                                " Hz), device kept code " +
                                                std::to_string(readback_code) + " (" +
                                                (readback_hz > 0 ? std::to_string(readback_hz) + " Hz"
                                                                 : "unmapped rate") +
                                                ")");
        }
        return bridge::Ok(request, "{\"code\":" + std::to_string(readback_code) +
                                       ",\"hz\":" + std::to_string(readback_hz) + "}");
    }

    if (command == "misc.get") {
        incott::Device device;
        if (!open_device(&device)) return bridge::Failure(request, "device not found");

        // WP5-5: an unread field reports null, never the old 8/60 defaults. WP5-6:
        // every reply must echo the requested sub (bytes[2]) or it is discarded.
        int debounce = -1;
        int sleep_sec = -1;
        const std::uint8_t sub_debounce[1] = {0x01};
        for (int attempt = 0; attempt < 3 && debounce < 0; ++attempt) {
            incott::Frame deb_reply{};
            if (Exchange(device, incott::MakeRead(incott::kCmdMisc, sub_debounce, 1), deb_reply,
                         L"0x85 debounce") == 0) {
                debounce = incott::DebounceValue(deb_reply, 0x01);
            }
            if (debounce < 0) Sleep(40);
        }
        const std::uint8_t sub_sleep[1] = {0x03};
        for (int attempt = 0; attempt < 3 && sleep_sec < 0; ++attempt) {
            incott::Frame slp_reply{};
            if (Exchange(device, incott::MakeRead(incott::kCmdMisc, sub_sleep, 1), slp_reply,
                         L"0x85 sleep") == 0) {
                sleep_sec = incott::SleepSeconds(slp_reply, 0x03);
            }
            if (sleep_sec < 0) Sleep(40);
        }
        // WP2-1: fire params come from sub2; null means "not read", never a default.
        const std::uint8_t sub_fire[1] = {0x02};
        incott::Frame fire_reply{};
        int fire_loops = -1;
        int fire_interval = -1;
        for (int attempt = 0; attempt < 3; ++attempt) {
            if (Exchange(device, incott::MakeRead(incott::kCmdMisc, sub_fire, 1), fire_reply, L"0x85 fire") == 0 &&
                fire_reply.bytes[2] == 0x02) {
                fire_loops = incott::FireLoops(fire_reply);
                fire_interval = incott::FireInterval(fire_reply);
                break;
            }
            Sleep(40);
        }
        auto int_or_null = [](int value) { return value >= 0 ? std::to_string(value) : "null"; };
        return bridge::Ok(request, "{\"debounce\":" + int_or_null(debounce) +
                                       ",\"sleep\":" + int_or_null(sleep_sec) +
                                       ",\"fireLoops\":" + int_or_null(fire_loops) +
                                       ",\"fireInterval\":" + int_or_null(fire_interval) + "}");
    }

    if (command == "misc.set") {
        int debounce = -1;
        int sleep_sec = -1;
        int fire_loops = -1;
        int fire_interval = -1;
        const ArgState deb_state = opt_int("debounce", &debounce);
        const ArgState slp_state = opt_int("sleep", &sleep_sec);
        const ArgState loops_state = opt_int("fireLoops", &fire_loops);
        const ArgState interval_state = opt_int("fireInterval", &fire_interval);
        if (deb_state == ArgState::kBad) {
            return bridge::Failure(request, "debounce must be an integer");
        }
        if (slp_state == ArgState::kBad) {
            return bridge::Failure(request, "sleep must be an integer");
        }
        if (loops_state == ArgState::kBad) {
            return bridge::Failure(request, "fireLoops must be an integer");
        }
        if (interval_state == ArgState::kBad) {
            return bridge::Failure(request, "fireInterval must be an integer");
        }
        const bool have_debounce = (deb_state == ArgState::kOk);
        const bool have_sleep = (slp_state == ArgState::kOk);
        const bool have_fire_loops = (loops_state == ArgState::kOk);
        const bool have_fire_interval = (interval_state == ArgState::kOk);

        if (!have_debounce && !have_sleep && !have_fire_loops && !have_fire_interval) {
            return bridge::Failure(request, "misc.set needs debounce, sleep, fireLoops or fireInterval");
        }
        // WP5-3: bounds are checked up front, so an invalid request writes nothing.
        // Rejected, never clamped. The sleep cap is the measured firmware limit
        // (900 s kept, 950/1000/1800 refused; docs/05 §10).
        if (have_debounce && (debounce < 0 || debounce > incott::kDebounceMsMax)) {
            return bridge::Failure(request, "debounce must be 0..30 ms");
        }
        if (have_sleep && (sleep_sec < incott::kSleepSecondsMin || sleep_sec > incott::kSleepSecondsMax)) {
            return bridge::Failure(request, "sleep must be 10..900 s (the firmware refuses longer values)");
        }
        // WP2-1: fire-key params go through 0x05 02 (official setFireKeyParam), which
        // always carries BOTH bytes, so a single-field update reads the other one
        // first (docs/02: 缺省字段先读设备补齐 — the missing sentinel stays -1 here
        // and must NOT trip the bound check). Out-of-range values are rejected,
        // never clamped (docs/05 §3.3), per provided field only.
        if (have_fire_loops && (fire_loops < 0 || fire_loops > 3)) {
            return bridge::Failure(request, "fireLoops must be 0..3 and fireInterval 0..255");
        }
        if (have_fire_interval && (fire_interval < 0 || fire_interval > 255)) {
            return bridge::Failure(request, "fireLoops must be 0..3 and fireInterval 0..255");
        }

        incott::Device device;
        if (!open_device(&device)) return bridge::Failure(request, "device not found");
        // F1-2: probe via 0x83 before writing 0x05 so a sleeping mouse rejects cleanly.
        if (!DeviceAwake(device)) {
            return bridge::Failure(request, "device sleeping (move the mouse to wake it)");
        }

        int rb_debounce = -1;
        int rb_sleep = -1;
        if (have_debounce) {
            const std::uint8_t payload[2] = {0x01, static_cast<std::uint8_t>(debounce)};
            const incott::Frame frame = incott::MakeWrite(incott::kCmdMisc, payload, 2);
            incott::Frame echo{};
            std::wstring write_error;
            if (!device.Feature(frame.bytes, echo.bytes, write_error)) {
                return bridge::Failure(request, "debounce write failed");
            }
            Sleep(60);
            const std::uint8_t sub_debounce[1] = {0x01};
            for (int attempt = 0; attempt < 4 && rb_debounce != debounce; ++attempt) {
                incott::Frame reply{};
                if (Exchange(device, incott::MakeRead(incott::kCmdMisc, sub_debounce, 1), reply,
                             L"0x85 debounce readback") == 0) {
                    rb_debounce = incott::DebounceValue(reply, 0x01);
                }
                if (rb_debounce != debounce) Sleep(50);
            }
            if (rb_debounce != debounce) {
                return bridge::Failure(request,
                                       rb_debounce < 0
                                           ? "debounce readback failed (move the mouse to wake it)"
                                           : "readback mismatch: wrote debounce " +
                                                 std::to_string(debounce) + " ms, got " +
                                                 std::to_string(rb_debounce) + " ms");
            }
        }
        if (have_sleep) {
            const std::uint8_t payload[3] = {0x03, static_cast<std::uint8_t>(sleep_sec & 0xFF),
                                             static_cast<std::uint8_t>((sleep_sec >> 8) & 0xFF)};
            const incott::Frame frame = incott::MakeWrite(incott::kCmdMisc, payload, 3);
            incott::Frame echo{};
            std::wstring write_error;
            if (!device.Feature(frame.bytes, echo.bytes, write_error)) {
                return bridge::Failure(request, "sleep write failed");
            }
            Sleep(60);
            const std::uint8_t sub_sleep[1] = {0x03};
            for (int attempt = 0; attempt < 4 && rb_sleep != sleep_sec; ++attempt) {
                incott::Frame reply{};
                if (Exchange(device, incott::MakeRead(incott::kCmdMisc, sub_sleep, 1), reply,
                             L"0x85 sleep readback") == 0) {
                    rb_sleep = incott::SleepSeconds(reply, 0x03);
                }
                if (rb_sleep != sleep_sec) Sleep(50);
            }
            if (rb_sleep != sleep_sec) {
                return bridge::Failure(request,
                                       rb_sleep < 0
                                           ? "sleep readback failed (move the mouse to wake it)"
                                           : "readback mismatch: wrote sleep " +
                                                 std::to_string(sleep_sec) + " s, got " +
                                                 std::to_string(rb_sleep) + " s");
            }
        }
        int rb_loops = -1;
        int rb_interval = -1;
        if (have_fire_loops || have_fire_interval) {
            const std::uint8_t sub_fire[1] = {0x02};
            incott::Frame fire_reply{};
            bool fire_read = false;
            for (int attempt = 0; attempt < 3 && !fire_read; ++attempt) {
                if (Exchange(device, incott::MakeRead(incott::kCmdMisc, sub_fire, 1), fire_reply,
                             L"0x85 fire read") == 0 &&
                    fire_reply.bytes[2] == 0x02) {
                    fire_read = true;
                    break;
                }
                Sleep(40);
            }
            if (!fire_read) {
                return bridge::Failure(request, "fire param read failed (move the mouse to wake it)");
            }
            // Fill the field the caller left out from the live device value.
            if (!have_fire_loops) fire_loops = incott::FireLoops(fire_reply);
            if (!have_fire_interval) fire_interval = incott::FireInterval(fire_reply);
            if (fire_loops < 0 || fire_interval < 0) {
                return bridge::Failure(request, "fire param read failed (invalid device value)");
            }

            const std::uint8_t payload[3] = {0x02, static_cast<std::uint8_t>(fire_loops),
                                             static_cast<std::uint8_t>(fire_interval)};
            const incott::Frame frame = incott::MakeWrite(incott::kCmdMisc, payload, 3);
            incott::Frame echo{};
            std::wstring write_error;
            if (!device.Feature(frame.bytes, echo.bytes, write_error)) {
                return bridge::Failure(request, "fire param write failed");
            }
            Sleep(60);

            for (int attempt = 0;
                 attempt < 4 && (rb_loops != fire_loops || rb_interval != fire_interval); ++attempt) {
                incott::Frame reply{};
                if (Exchange(device, incott::MakeRead(incott::kCmdMisc, sub_fire, 1), reply,
                             L"0x85 fire readback") == 0 &&
                    reply.bytes[2] == 0x02) {
                    rb_loops = incott::FireLoops(reply);
                    rb_interval = incott::FireInterval(reply);
                }
                if (rb_loops != fire_loops || rb_interval != fire_interval) Sleep(50);
            }
            if (rb_loops != fire_loops || rb_interval != fire_interval) {
                return bridge::Failure(request, "readback mismatch: wrote fire " +
                                                    std::to_string(fire_loops) + "/" +
                                                    std::to_string(fire_interval) + " got " +
                                                    std::to_string(rb_loops) + "/" +
                                                    std::to_string(rb_interval));
            }
        }

        // WP5-3: the reply only carries verified readback values, for the fields
        // that were actually written (misc.set 一次只改一个参数也要回读).
        command_json::MiscSetFields fields;
        fields.have_debounce = have_debounce;
        fields.debounce = rb_debounce;
        fields.have_sleep = have_sleep;
        fields.sleep = rb_sleep;
        fields.have_fire = (have_fire_loops || have_fire_interval);
        fields.fire_loops = rb_loops;
        fields.fire_interval = rb_interval;
        return bridge::Ok(request, command_json::BuildMiscSetData(fields));
    }

    if (command == "sensor.get") {
        incott::Device device;
        if (!open_device(&device)) return bridge::Failure(request, "device not found");

        // WP5-5: an unread sub reports null, never a default. WP5-6: the reply
        // must echo the requested sub (bytes[2]) or the value is discarded —
        // a stale frame from another sub must not leak across.
        auto read_sub = [&](int selector) -> int {
            const std::uint8_t sub[1] = {static_cast<std::uint8_t>(selector)};
            for (int attempt = 0; attempt < 3; ++attempt) {
                incott::Frame reply{};
                if (Exchange(device, incott::MakeRead(incott::kCmdSensor, sub, 1), reply, L"0x84 read") == 0) {
                    const int value = incott::SensorSubValue(reply, selector);
                    if (value >= 0) return value;
                }
                Sleep(30);
            }
            return -1;
        };

        const int lod = read_sub(1);
        const int ripple = read_sub(2);
        const int fixline = read_sub(3);
        const int motion = read_sub(4);
        const int mode = read_sub(5);

        auto int_or_null = [](int value) { return value >= 0 ? std::to_string(value) : "null"; };
        auto bool_or_null = [](int value) {
            if (value == 1) return "true";
            if (value == 0) return "false";
            return "null";
        };
        return bridge::Ok(request, "{\"lod\":" + int_or_null(lod) +
                                       ",\"ripple\":" + bool_or_null(ripple) +
                                       ",\"fixline\":" + bool_or_null(fixline) +
                                       ",\"motion\":" + bool_or_null(motion) +
                                       ",\"mode\":" + int_or_null(mode) + "}");
    }

    if (command == "sensor.set") {
        int lod = -1;
        int mode = -1;
        bool ripple = false;
        bool fixline = false;
        bool motion = false;
        const ArgState lod_state = opt_int("lod", &lod);
        const ArgState ripple_state = opt_bool("ripple", &ripple);
        const ArgState fixline_state = opt_bool("fixline", &fixline);
        const ArgState motion_state = opt_bool("motion", &motion);
        const ArgState mode_state = opt_int("mode", &mode);
        if (lod_state == ArgState::kBad) {
            return bridge::Failure(request, "lod must be an integer");
        }
        if (ripple_state == ArgState::kBad) {
            return bridge::Failure(request, "ripple must be a boolean");
        }
        if (fixline_state == ArgState::kBad) {
            return bridge::Failure(request, "fixline must be a boolean");
        }
        if (motion_state == ArgState::kBad) {
            return bridge::Failure(request, "motion must be a boolean");
        }
        if (mode_state == ArgState::kBad) {
            return bridge::Failure(request, "mode must be an integer");
        }
        const bool have_lod = (lod_state == ArgState::kOk);
        const bool have_ripple = (ripple_state == ArgState::kOk);
        const bool have_fixline = (fixline_state == ArgState::kOk);
        const bool have_motion = (motion_state == ArgState::kOk);
        const bool have_mode = (mode_state == ArgState::kOk);

        if (!have_lod && !have_ripple && !have_fixline && !have_motion && !have_mode) {
            return bridge::Failure(request, "sensor.set needs lod, ripple, fixline, motion or mode");
        }
        // WP5-1: LOD/mode are 0..2 on the wire; out-of-range values are rejected,
        // never clamped and never silently skipped (the firmware keeps such writes
        // quiet, so an unchecked write would fake success).
        if (have_lod && !incott::SensorValueValid(1, lod)) {
            return bridge::Failure(request, "lod must be 0..2");
        }
        if (have_mode && !incott::SensorValueValid(5, mode)) {
            return bridge::Failure(request, "mode must be 0..2");
        }

        incott::Device device;
        if (!open_device(&device)) return bridge::Failure(request, "device not found");
        // F1-2: probe via 0x83 before writing 0x04 so a sleeping mouse rejects cleanly.
        if (!DeviceAwake(device)) {
            return bridge::Failure(request, "device sleeping (move the mouse to wake it)");
        }

        // WP5-1: each sub checks the 0x04 write result, then verifies with a 0x84
        // readback of the same sub (the reply must echo it — bytes[2]); any write
        // failure or mismatch fails the whole command instead of a fake ok:true.
        // The firmware applies asynchronously: first reads come back stale/empty.
        auto apply_sub = [&](int selector, int value, const char* name,
                             int* readback) -> std::string {
            const std::uint8_t payload[2] = {static_cast<std::uint8_t>(selector),
                                             static_cast<std::uint8_t>(value)};
            const incott::Frame frame = incott::MakeWrite(incott::kCmdSensor, payload, 2);
            incott::Frame echo{};
            std::wstring write_error;
            if (!device.Feature(frame.bytes, echo.bytes, write_error)) {
                return std::string(name) + " write failed";
            }
            const std::uint8_t sub[1] = {static_cast<std::uint8_t>(selector)};
            Sleep(50);
            int got = -1;
            for (int attempt = 0; attempt < 4 && got != value; ++attempt) {
                incott::Frame reply{};
                if (Exchange(device, incott::MakeRead(incott::kCmdSensor, sub, 1), reply,
                             L"0x84 readback") == 0) {
                    got = incott::SensorSubValue(reply, selector);
                }
                if (got != value) Sleep(50);
            }
            if (got < 0) {
                return std::string(name) + " readback failed (move the mouse to wake it)";
            }
            if (got != value) {
                return std::string(name) + " readback mismatch: wrote " + std::to_string(value) +
                       ", got " + std::to_string(got);
            }
            if (readback != nullptr) *readback = got;
            return "";
        };

        command_json::SensorSetFields fields;
        int got_val = 0;
        std::string error;
        if (have_lod) {
            error = apply_sub(1, lod, "lod", &got_val);
            if (error.empty()) {
                fields.have_lod = true;
                fields.lod = got_val;
            }
        }
        if (error.empty() && have_ripple) {
            error = apply_sub(2, ripple ? 1 : 0, "ripple", &got_val);
            if (error.empty()) {
                fields.have_ripple = true;
                fields.ripple = (got_val > 0);
            }
        }
        if (error.empty() && have_fixline) {
            error = apply_sub(3, fixline ? 1 : 0, "fixline", &got_val);
            if (error.empty()) {
                fields.have_fixline = true;
                fields.fixline = (got_val > 0);
            }
        }
        if (error.empty() && have_motion) {
            error = apply_sub(4, motion ? 1 : 0, "motion", &got_val);
            if (error.empty()) {
                fields.have_motion = true;
                fields.motion = (got_val > 0);
            }
        }
        if (error.empty() && have_mode) {
            error = apply_sub(5, mode, "mode", &got_val);
            if (error.empty()) {
                fields.have_mode = true;
                fields.mode = got_val;
            }
        }
        if (!error.empty()) return bridge::Failure(request, error);
        return bridge::Ok(request, command_json::BuildSensorSetData(fields));
    }

    if (command == "led.get") {
        incott::Device device;
        if (!open_device(&device)) return bridge::Failure(request, "device not found");

        incott::Frame model_reply{};
        std::wstring model_error;
        if (!device.Feature(incott::MakeRead(incott::kCmdModel, nullptr, 0).bytes,
                            model_reply.bytes, model_error)) {
            return bridge::Failure(request, "0x8F read failed");
        }
        const incott::DeviceInfo info = incott::ParseModel(model_reply, device.IsWireless());
        if (info.receiver != 2 || !device.IsWireless()) {
            return bridge::Ok(request, command_json::BuildReceiverLedData(false, -1));
        }

        int mode = -1;
        for (int attempt = 0; attempt < 3 && mode < 0; ++attempt) {
            incott::Frame reply{};
            if (Exchange(device, incott::MakeRead(incott::kCmdReceiverLed, nullptr, 0), reply,
                         L"0x88 receiver led") == 0) {
                mode = incott::ParseReceiverLed(reply);
            }
            if (mode < 0) Sleep(40);
        }
        return bridge::Ok(request, command_json::BuildReceiverLedData(true, mode));
    }

    if (command == "led.set") {
        int mode = -1;
        const ArgState mode_state = opt_int("mode", &mode);
        if (mode_state == ArgState::kAbsent) {
            return bridge::Failure(request, "led.set needs args.mode (0..2)");
        }
        if (mode_state == ArgState::kBad) {
            return bridge::Failure(request, "mode must be an integer");
        }
        if (!incott::ReceiverLedValid(mode)) {
            return bridge::Failure(request, "mode must be 0..2");
        }

        incott::Device device;
        if (!open_device(&device)) return bridge::Failure(request, "device not found");

        incott::Frame model_reply{};
        std::wstring model_error;
        if (!device.Feature(incott::MakeRead(incott::kCmdModel, nullptr, 0).bytes,
                            model_reply.bytes, model_error)) {
            return bridge::Failure(request, "0x8F read failed; cannot verify the receiver");
        }
        const incott::DeviceInfo info = incott::ParseModel(model_reply, device.IsWireless());
        if (info.receiver != 2 || !device.IsWireless()) {
            return bridge::Failure(request, "receiver LED requires 8K wireless receiver");
        }

        const std::uint8_t payload[1] = {static_cast<std::uint8_t>(mode)};
        const incott::Frame frame = incott::MakeWrite(incott::kCmdReceiverLed, payload, 1);
        incott::Frame echo{};
        std::wstring write_error;
        if (!device.Feature(frame.bytes, echo.bytes, write_error)) {
            return bridge::Failure(request, "0x08 write failed");
        }
        Sleep(60);

        int readback = -1;
        for (int attempt = 0; attempt < 4 && readback != mode; ++attempt) {
            incott::Frame reply{};
            if (Exchange(device, incott::MakeRead(incott::kCmdReceiverLed, nullptr, 0), reply,
                         L"0x88 receiver led readback") == 0) {
                readback = incott::ParseReceiverLed(reply);
            }
            if (readback != mode) Sleep(50);
        }
        if (readback < 0) {
            return bridge::Failure(request, "0x88 readback failed");
        }
        if (readback != mode) {
            return bridge::Failure(request, "readback mismatch: wrote mode " +
                                                std::to_string(mode) + ", got " +
                                                std::to_string(readback));
        }
        return bridge::Ok(request, command_json::BuildReceiverLedData(true, readback));
    }

    return bridge::Failure(request, "unknown hardware command");
}

std::string HandleDeviceInfoRequest(const bridge::Request& request) {
    std::string device_key;
    if (request.args != nullptr) {
        if (const json::Value* key_val = request.args->Find("deviceKey")) {
            if (key_val->type != json::Value::Type::kString) {
                return bridge::Failure(request, "deviceKey must be a string");
            }
            device_key = key_val->text;
        }
    }

    const std::vector<bridge::DeviceSnapshot> devices = bridge::Devices();
    const bridge::DeviceSnapshot* matched = nullptr;
    if (!device_key.empty()) {
        for (const bridge::DeviceSnapshot& d : devices) {
            if (d.key == device_key) {
                matched = &d;
                break;
            }
        }
    } else {
        for (const bridge::DeviceSnapshot& d : devices) {
            if (d.driver_level == "full") {
                matched = &d;
                device_key = d.key;
                break;
            }
        }
        if (device_key.empty()) {
            device_key = "incott:522C";
        }
    }

    return bridge::Ok(request, drivers::BuildDeviceInfoJson(drivers::Global(), device_key, matched));
}

std::string HandleDeviceRenameRequest(const bridge::Request& request) {
    if (request.args == nullptr) {
        return bridge::Failure(request, "device.rename needs args.deviceKey and args.alias");
    }
    std::string device_key;
    if (!request.args->GetString("deviceKey", &device_key) || device_key.empty()) {
        return bridge::Failure(request, "device.rename needs non-empty args.deviceKey");
    }
    if (device_key.size() > 128) {
        return bridge::Failure(request, "deviceKey is too long");
    }

    const json::Value* alias_val = request.args->Find("alias");
    if (alias_val == nullptr || alias_val->type != json::Value::Type::kString) {
        return bridge::Failure(request, "device.rename needs string args.alias");
    }
    std::string alias = alias_val->text;
    while (!alias.empty() && (alias.front() == ' ' || alias.front() == '\t')) alias.erase(alias.begin());
    while (!alias.empty() && (alias.back() == ' ' || alias.back() == '\t')) alias.pop_back();

    const std::wstring wide = Widen(alias);
    if (wide.size() > 48u) {
        return bridge::Failure(request, "alias must be at most 48 characters");
    }
    for (wchar_t ch : wide) {
        if (ch < 0x20 || ch == 0x7F) {
            return bridge::Failure(request, "alias contains control characters");
        }
    }

    std::map<std::string, std::string> next_aliases;
    {
        std::lock_guard<std::mutex> lock(g_hub_mutex);
        next_aliases = g_hub.device_aliases;
    }
    if (alias.empty()) {
        next_aliases.erase(device_key);
    } else {
        next_aliases[device_key] = alias;
    }
    std::string save_err;
    if (!SaveDeviceAliases(next_aliases, &save_err)) {
        return bridge::Failure(request, "failed to persist device alias: " + save_err);
    }

    {
        std::lock_guard<std::mutex> lock(g_hub_mutex);
        g_hub.device_aliases = std::move(next_aliases);
    }

    std::string resolved_name = alias;
    if (alias.empty()) {
        RefreshDeviceCache(1);
        for (const bridge::DeviceSnapshot& d : bridge::Devices()) {
            if (d.key == device_key) {
                resolved_name = d.name;
                break;
            }
        }
    } else {
        std::vector<bridge::DeviceSnapshot> devices = bridge::Devices();
        bool changed = false;
        for (bridge::DeviceSnapshot& d : devices) {
            if (d.key == device_key) {
                d.name = alias;
                changed = true;
            }
        }
        if (changed) {
            bridge::SetDevices(std::move(devices));
        }
    }

    if (g_main != nullptr) {
        PostMessageW(g_main, kRefreshDoneMessage, 0, 0);
    }

    return bridge::Ok(request, "{\"key\":" + bridge::JsonQuote(device_key) +
                                   ",\"name\":" + bridge::JsonQuote(resolved_name) +
                                   ",\"alias\":" + bridge::JsonQuote(alias) + "}");
}

command_json::HubSettingsFields BuildHubSettingsReplyFields() {
    const HubSettings hub = SnapshotHubSettings();
    command_json::HubSettingsFields out;
    out.taskbar_widget = hub.taskbar_widget;
    out.auto_start = AutostartEnabled();
    out.widget_stacked = hub.widget_stacked;
    out.widget_background = hub.widget_background;
    out.widget_monitor = hub.widget_monitor.empty() ? "all" : hub.widget_monitor;
    for (const widget::MonitorOption& mon : widget::OtherMonitors()) {
        out.monitors.push_back({Narrow(mon.id), Narrow(mon.label)});
    }
    out.brand_accent = hub.brand_accent;
    out.low_battery_notify = hub.low_battery_notify;
    out.low_battery_threshold = hub.low_battery_threshold;
    out.poll_interval_sec = hub.poll_interval_sec;
    out.hidden_devices = hub.hidden_devices;
    out.device_aliases = hub.device_aliases;
    return out;
}

std::string HandleHubRequest(const bridge::Request& request) {
    if (request.cmd == "hub.get") {
        return bridge::Ok(request, command_json::BuildHubSettingsData(BuildHubSettingsReplyFields()));
    }
    if (request.cmd == "hub.testNotify") {
        if (g_main == nullptr || !g_tray_added.load()) {
            return bridge::Failure(request, "tray icon is not active (notifications require tray mode)");
        }
        PostMessageW(g_main, kTestNotifyMessage, 0, 0);
        return bridge::Ok(request, "{\"sent\":true}");
    }
    if (request.cmd == "hub.set") {
        bool want_widget = false;
        bool want_autostart = false;
        bool want_stacked = false;
        bool want_background = false;
        bool want_accent = false;
        bool want_low_notify = false;
        int want_low_threshold = 0;
        int want_poll_sec = 0;

        const ArgState widget_state = OptBoolArg(request.args, "taskbarWidget", &want_widget);
        const ArgState autostart_state = OptBoolArg(request.args, "autoStart", &want_autostart);
        const ArgState stacked_state = OptBoolArg(request.args, "widgetStacked", &want_stacked);
        const ArgState background_state =
            OptBoolArg(request.args, "widgetBackground", &want_background);
        const ArgState accent_state = OptBoolArg(request.args, "brandAccent", &want_accent);
        const ArgState notify_state =
            OptBoolArg(request.args, "lowBatteryNotify", &want_low_notify);
        const ArgState threshold_state =
            OptIntArg(request.args, "lowBatteryThreshold", &want_low_threshold);
        const ArgState poll_state = OptIntArg(request.args, "pollIntervalSec", &want_poll_sec);

        if (widget_state == ArgState::kBad) {
            return bridge::Failure(request, "taskbarWidget must be a boolean");
        }
        if (autostart_state == ArgState::kBad) {
            return bridge::Failure(request, "autoStart must be a boolean");
        }
        if (stacked_state == ArgState::kBad) {
            return bridge::Failure(request, "widgetStacked must be a boolean");
        }
        if (background_state == ArgState::kBad) {
            return bridge::Failure(request, "widgetBackground must be a boolean");
        }
        if (accent_state == ArgState::kBad) {
            return bridge::Failure(request, "brandAccent must be a boolean");
        }
        if (notify_state == ArgState::kBad) {
            return bridge::Failure(request, "lowBatteryNotify must be a boolean");
        }
        if (threshold_state == ArgState::kBad ||
            (threshold_state == ArgState::kOk && (want_low_threshold < 5 || want_low_threshold > 50))) {
            return bridge::Failure(request, "lowBatteryThreshold must be an integer 5..50");
        }
        if (poll_state == ArgState::kBad ||
            (poll_state == ArgState::kOk && (want_poll_sec < 10 || want_poll_sec > 300))) {
            return bridge::Failure(request, "pollIntervalSec must be an integer 10..300");
        }

        bool have_monitor = false;
        std::string want_monitor;
        if (const json::Value* mon_val =
                request.args != nullptr ? request.args->Find("widgetMonitor") : nullptr) {
            if (mon_val->type != json::Value::Type::kString || mon_val->text.empty()) {
                return bridge::Failure(request, "widgetMonitor must be a non-empty string");
            }
            want_monitor = mon_val->text;
            have_monitor = true;
        }

        bool have_hidden = false;
        std::vector<std::string> want_hidden;
        if (const json::Value* hid_val =
                request.args != nullptr ? request.args->Find("hiddenDevices") : nullptr) {
            if (hid_val->type != json::Value::Type::kArray) {
                return bridge::Failure(request, "hiddenDevices must be an array of strings");
            }
            for (const json::Value& entry : hid_val->array) {
                if (entry.type != json::Value::Type::kString) {
                    return bridge::Failure(request, "hiddenDevices entries must be strings");
                }
                if (!entry.text.empty() &&
                    std::find(want_hidden.begin(), want_hidden.end(), entry.text) ==
                        want_hidden.end()) {
                    want_hidden.push_back(entry.text);
                }
            }
            have_hidden = true;
        }

        const bool any_hub_pref = (widget_state == ArgState::kOk) ||
                                  (stacked_state == ArgState::kOk) ||
                                  (background_state == ArgState::kOk) ||
                                  (accent_state == ArgState::kOk) ||
                                  (notify_state == ArgState::kOk) ||
                                  (threshold_state == ArgState::kOk) ||
                                  (poll_state == ArgState::kOk) || have_monitor || have_hidden;
        if (!any_hub_pref && autostart_state == ArgState::kAbsent) {
            return bridge::Failure(request, "hub.set needs at least one setting field");
        }

        if (autostart_state == ArgState::kOk) {
            if (!SetAutostart(want_autostart)) {
                return bridge::Failure(request, "failed to update Windows autostart registry key");
            }
        }
        if (any_hub_pref) {
            HubSettings next;
            {
                std::lock_guard<std::mutex> lock(g_hub_mutex);
                if (widget_state == ArgState::kOk) g_hub.taskbar_widget = want_widget;
                if (stacked_state == ArgState::kOk) g_hub.widget_stacked = want_stacked;
                if (background_state == ArgState::kOk) g_hub.widget_background = want_background;
                if (accent_state == ArgState::kOk) g_hub.brand_accent = want_accent;
                if (notify_state == ArgState::kOk) g_hub.low_battery_notify = want_low_notify;
                if (threshold_state == ArgState::kOk) {
                    g_hub.low_battery_threshold = want_low_threshold;
                }
                if (poll_state == ArgState::kOk) g_hub.poll_interval_sec = want_poll_sec;
                if (have_monitor) g_hub.widget_monitor = want_monitor;
                if (have_hidden) g_hub.hidden_devices = std::move(want_hidden);
                next = g_hub;
            }
            SaveHubSettings(next);
            if (g_main != nullptr) {
                PostMessageW(g_main, kRefreshDoneMessage, 0, 0);
            }
        }
        return bridge::Ok(request, command_json::BuildHubSettingsData(BuildHubSettingsReplyFields()));
    }
    return bridge::Failure(request, "unknown hub command");
}

// macros.* and macro.import/macro.export need both the library and the device, so
// they live in the host; the bridge module stays free of business logic.
std::string HandleMacroRequest(const bridge::Request& request,
                               const std::string& driver_name,
                               const std::string& device_key) {
    const json::Value* args = request.args;
    const std::string& command = request.cmd;

    auto arg_int = [&](const char* name, int* out) {
        long long value = 0;
        if (args == nullptr || !args->GetInt(name, &value)) return false;
        *out = static_cast<int>(value);
        return true;
    };
    auto save_library = [&](std::string* message) {
        std::wstring error;
        if (SaveMacroLibrary(g_macros, &error)) return true;
        *message = Narrow(error);
        return false;
    };

    if (command == "macros.list") {
        return bridge::Ok(request, MacroListJson());
    }
    if (command == "macros.save") {
        int slot = -1;
        if (!arg_int("slot", &slot) || slot < 0 || slot >= incott::MacroStore::kSlotCount) {
            return bridge::Failure(request, "slot must be 0..9");
        }
        const json::Value* value = args != nullptr ? args->Find("macro") : nullptr;
        if (value == nullptr) return bridge::Failure(request, "macros.save needs args.macro");
        incott::Macro macro;
        std::string error;
        if (!incott::ParseMacroValue(*value, &macro, &error)) {
            return bridge::Failure(request, error);
        }
        // ParseMacroValue already closes the action tail (macro_json.cpp); both the
        // library and the firmware need that normalized form.
        const incott::MacroSlot backup = g_macros.At(slot);
        g_macros.At(slot).macro = macro;
        g_macros.At(slot).defined = true;
        g_macros.At(slot).dirty = true;
        std::string message;
        if (!save_library(&message)) {
            g_macros.At(slot) = backup;  // memory must not drift from the file
            return bridge::Failure(request, message);
        }
        return bridge::Ok(request, "{\"slot\":" + std::to_string(slot) + "}");
    }
    if (command == "macro.import") {
        std::string text;
        if (args == nullptr || !args->GetString("json", &text)) {
            return bridge::Failure(request, "macro.import needs args.json");
        }
        if (text.size() > 131072) {
            return bridge::Failure(request, "file exceeds 128 KB limit");
        }
        // Optional explicit slot (overwrite on purpose); otherwise the first
        // free slot, preferring 0..5 because those map 1:1 onto key buffers.
        // Validated before the body: a bad slot must not be masked by parse errors.
        int slot = -1;
        long long requested = -1;
        const bool has_explicit = args != nullptr && args->GetInt("slot", &requested);
        if (has_explicit) {
            if (requested < 0 || requested >= incott::MacroStore::kSlotCount) {
                return bridge::Failure(request, "slot must be 0..9");
            }
            slot = static_cast<int>(requested);
        }
        incott::Macro macro;
        std::string error;
        if (!incott::ParseMacroJson(text, &macro, &error)) {
            return bridge::Failure(request, error);
        }
        if (!has_explicit) {
            for (int pass = 0; pass < 2 && slot < 0; ++pass) {
                const int lo = pass == 0 ? 0 : 6;
                const int hi = pass == 0 ? 6 : incott::MacroStore::kSlotCount;
                for (int i = lo; i < hi; ++i) {
                    if (!g_macros.At(i).defined) {
                        slot = i;
                        break;
                    }
                }
            }
        }
        if (slot < 0) return bridge::Failure(request, "library is full (10 macros)");
        const incott::MacroSlot backup = g_macros.At(slot);
        g_macros.At(slot).macro = macro;
        g_macros.At(slot).defined = true;
        g_macros.At(slot).dirty = true;
        std::string message;
        if (!save_library(&message)) {
            g_macros.At(slot) = backup;
            return bridge::Failure(request, message);
        }
        return bridge::Ok(request, "{\"slot\":" + std::to_string(slot) +
                                       ",\"summary\":" + incott::MacroSummaryJson(macro) + "}");
    }
    if (command == "macro.export") {
        int slot = -1;
        if (!arg_int("slot", &slot) || slot < 0 || slot >= incott::MacroStore::kSlotCount) {
            return bridge::Failure(request, "slot must be 0..9");
        }
        if (!g_macros.At(slot).defined) return bridge::Failure(request, "slot is empty");
        return bridge::Ok(request, "{\"json\":" +
                                        bridge::JsonQuote(incott::MacroToJson(g_macros.At(slot).macro)) +
                                        "}");
    }
    if (command == "macros.upload") {
        int slot = -1;
        int button = -1;
        if (!arg_int("slot", &slot) || slot < 0 || slot >= incott::MacroStore::kSlotCount) {
            return bridge::Failure(request, "slot must be 0..9");
        }
        if (!arg_int("buttonIndex", &button) || button < 0 || button >= incott::kKeyCount) {
            return bridge::Failure(request, "buttonIndex must be 0..5");
        }
        // WP1-2: library slot and firmware buffer are independent. The firmware
        // plays the macro buffer whose id equals the key index, so `button` is
        // the buffer number; the library slot only decides WHAT is uploaded.
        if (!g_macros.At(slot).defined) {
            return bridge::Failure(request, "slot " + std::to_string(slot) +
                                                " is empty (save the macro first)");
        }
        std::string binding_str;
        if (driver_name == "yjx") {
            yjx::MacroSlotData yjx_slots[yjx::kMacroSlotCount]{};
            for (int s = 0; s < yjx::kMacroSlotCount; ++s) {
                if (!g_macros.At(s).defined) continue;
                for (const incott::MacroAction& a : g_macros.At(s).macro.actions) {
                    yjx::MacroStep st{};
                    st.down = a.press;
                    if (a.usage >= 0xF0 && a.usage <= 0xF4) {
                        st.is_mouse = true;
                        switch (a.usage) {
                            case 0xF0: st.code = 1; break;
                            case 0xF1: st.code = 2; break;
                            case 0xF2: st.code = 4; break;
                            case 0xF3: st.code = 8; break;
                            case 0xF4: st.code = 16; break;
                            default: st.code = 1; break;
                        }
                    } else {
                        st.is_mouse = false;
                        st.code = static_cast<std::uint8_t>(a.usage & 0xFF);
                    }
                    int delay = a.delayMs;
                    if (delay < 0) delay = 0;
                    if (delay > 65535) delay = 65535;
                    st.delay_ms = static_cast<std::uint16_t>(delay);
                    yjx_slots[s].steps.push_back(st);
                }
            }
            if (yjx_slots[slot].steps.empty()) {
                return bridge::Failure(request, "macro has no actions to upload");
            }
            std::vector<std::uint8_t> table;
            std::string enc_err;
            if (!yjx::EncodeMacroTable(yjx_slots, &table, &enc_err)) {
                return bridge::Failure(request, enc_err);
            }
            yjx::Device yjx_dev;
            std::string hid_err;
            std::uint16_t preferred_vid = 0;
            driver_router::ParseYjxVidFromKey(device_key, &preferred_vid);
            if (!yjx_dev.Open(preferred_vid, &hid_err)) {
                return bridge::Failure(request, "device not found (plug the receiver back in)");
            }
            if (!yjx_dev.UploadMacroTable(table, &hid_err)) {
                return bridge::Failure(request, hid_err);
            }
            const incott::Macro& target_macro = g_macros.At(slot).macro;
            const yjx::KeyCode key_val =
                yjx::MakeMacroKeyCode(slot, target_macro.loopType, target_macro.cycles);
            yjx::KeyCode rb{};
            if (!yjx_dev.WriteKeyVerified(button, key_val, &rb, &hid_err)) {
                return bridge::Failure(request, hid_err);
            }
            binding_str = "Macro:" + std::to_string(slot);
        } else {
            incott::Device device;
            std::wstring device_error;
            if (!device.Open(device_error)) {
                return bridge::Failure(request, "device not found (plug the receiver back in)");
            }
            incott::MacroError code = incott::MacroError::kOk;
            const int upload_rc = UploadMacro(device, button, g_macros.At(slot).macro, &code, false);
            if (upload_rc == 2) return bridge::Failure(request, incott::MacroErrorText(code));
            if (upload_rc != 0) return bridge::Failure(request, "macro upload failed (0x07)");

            const std::uint32_t binding =
                (static_cast<std::uint32_t>(button) << 16) | 0x0009u;
            std::uint32_t readback = 0xFFFFFFFFu;
            const int write_rc = WriteKey(device, button, binding, &readback, false);
            if (write_rc == 1) return bridge::Failure(request, "0x06 write failed after upload");
            if (write_rc == 2) {
                return bridge::Failure(request, "readback mismatch: wrote 0x" +
                                                    bridge::HexCode(binding) + " got 0x" +
                                                    bridge::HexCode(readback));
            }
            binding_str = "Macro:" + std::to_string(button);
        }
        g_macros.At(slot).dirty = false;
        g_macros.SetBinding(button, slot);
        std::string message;
        // The key write already succeeded; a save hiccup is a warning, not a
        // failure, so the caller does not retry the HID write.
        if (!save_library(&message)) {
            return bridge::Ok(request, "{\"slot\":" + std::to_string(slot) +
                                           ",\"buttonIndex\":" + std::to_string(button) +
                                           ",\"binding\":" + bridge::JsonQuote(binding_str) +
                                           ",\"warning\":" + bridge::JsonQuote(message) + "}");
        }
        return bridge::Ok(request, "{\"slot\":" + std::to_string(slot) +
                                       ",\"buttonIndex\":" + std::to_string(button) +
                                       ",\"binding\":" + bridge::JsonQuote(binding_str) + "}");
    }
    if (command == "macros.delete") {
        int slot = -1;
        if (!arg_int("slot", &slot) || slot < 0 || slot >= incott::MacroStore::kSlotCount) {
            return bridge::Failure(request, "slot must be 0..9");
        }
        if (!g_macros.At(slot).defined) {
            return bridge::Failure(request, "slot " + std::to_string(slot) + " is empty");
        }
        // PC-library only: the firmware keeps playing the uploaded buffer until
        // the key is re-bound, so the reply stays silent about hardware state.
        const incott::MacroSlot backup = g_macros.At(slot);
        const int bindings[incott::kKeyCount] = {g_macros.Binding(0), g_macros.Binding(1),
                                                 g_macros.Binding(2), g_macros.Binding(3),
                                                 g_macros.Binding(4), g_macros.Binding(5)};
        g_macros.At(slot) = incott::MacroSlot{};
        for (int key = 0; key < incott::kKeyCount; ++key) {
            if (bindings[key] == slot) g_macros.SetBinding(key, -1);
        }
        std::string message;
        if (!save_library(&message)) {
            g_macros.At(slot) = backup;
            for (int key = 0; key < incott::kKeyCount; ++key) {
                if (bindings[key] == slot) g_macros.SetBinding(key, slot);
            }
            return bridge::Failure(request, message);
        }
        return bridge::Ok(request, "{\"slot\":" + std::to_string(slot) + "}");
    }
    if (command == "macros.rename") {
        int slot = -1;
        if (!arg_int("slot", &slot) || slot < 0 || slot >= incott::MacroStore::kSlotCount) {
            return bridge::Failure(request, "slot must be 0..9");
        }
        std::string name;
        if (args == nullptr || !args->GetString("name", &name) || name.empty()) {
            return bridge::Failure(request, "macros.rename needs args.name");
        }
        const std::wstring wide = WidenUtf8(name);
        if (wide.size() > static_cast<std::size_t>(incott::kMacroNameLimit)) {
            return bridge::Failure(request, "name must be at most 32 characters");
        }
        for (wchar_t ch : wide) {
            if (ch < 0x20 || ch == 0x7F) {
                return bridge::Failure(request, "name contains control characters");
            }
        }
        if (!g_macros.At(slot).defined) {
            return bridge::Failure(request, "slot " + std::to_string(slot) + " is empty");
        }
        const std::wstring backup = g_macros.At(slot).macro.name;
        g_macros.At(slot).macro.name = wide;
        std::string message;
        if (!save_library(&message)) {
            g_macros.At(slot).macro.name = backup;
            return bridge::Failure(request, message);
        }
        return bridge::Ok(request, "{\"slot\":" + std::to_string(slot) +
                                       ",\"name\":" + bridge::JsonQuote(name) + "}");
    }
    return bridge::Failure(request, "not implemented: " + command);
}

std::string HandleIncottRequest(const bridge::Request& request, const std::string& device_key) {
    // keys.get re-reads all six bindings; keys.set writes one, then re-reads.
    if (request.cmd == "keys.get") {
        if (!RefreshKeyCache()) {
            return bridge::Failure(request, "key read failed (move the mouse to wake it)");
        }
        return bridge::Ok(request, "{\"bindings\":" + bridge::KeysJson() + "}");
    }
    if (request.cmd == "keys.set") {
        int index = -1;
        std::string name;
        if (!bridge::ParseKeySetArgs(request, &index, &name)) {
            return bridge::Failure(request, "keys.set needs args.index and args.binding");
        }
        if (index < 0 || index >= incott::kKeyCount) {
            return bridge::Failure(request, "index must be 0..5");
        }
        // WP2-2: macros are bound through macros.upload (it writes the 320-byte
        // firmware buffer first); keys.set can only store the 4-byte binding code,
        // so a bare Macro:N here would point at a buffer that was never uploaded.
        if (name.rfind("Macro:", 0) == 0 || name.rfind("macro:", 0) == 0) {
            return bridge::Failure(request,
                                   "Macro:N is set via macros.upload (slot + buttonIndex), "
                                   "not keys.set");
        }
        std::uint32_t code = 0;
        if (!incott::EncodeBinding(name.c_str(), &code)) {
            return bridge::Failure(request, "unknown binding: " + name);
        }
        incott::Device device;
        std::wstring device_error;
        if (!device.Open(device_error)) return bridge::Failure(request, "device not found");
        std::uint32_t readback = 0xFFFFFFFFu;
        const int rc = WriteKey(device, index, code, &readback, false);
        if (rc == 1) return bridge::Failure(request, "0x06 write failed");
        if (rc == 2) {
            return bridge::Failure(request, "readback mismatch: wrote 0x" +
                                                bridge::HexCode(code) + " got 0x" +
                                                bridge::HexCode(readback));
        }
        // F1-6: once the hardware key no longer points to a macro (Key / Fire / Off),
        // clear any macro library bind targeting this button and persist macros.ini.
        host::ClearButtonMacroBind(index);
        // WP2-4: a verified write is a success, but the reply must never present an
        // empty bindings table as the device state. If the full cache refresh fails
        // (e.g. the mouse dozed off right after the write), report the one binding
        // we just verified instead of an empty list.
        if (RefreshKeyCache()) {
            return bridge::Ok(request, "{\"bindings\":" + bridge::KeysJson() + "}");
        }
        bridge::KeySnapshot written;
        written.index = index;
        written.matrix = incott::kKeyMatrix[index];
        written.code = readback;
        written.binding = incott::DescribeBinding(readback);
        written.kind = incott::KeyKindName(incott::DecodeBinding(readback).kind);
        std::string one = "{\"bindings\":[{\"index\":" + std::to_string(written.index) +
                          ",\"matrix\":" + std::to_string(written.matrix) +
                          ",\"code\":" + std::to_string(written.code) +
                          ",\"binding\":" + bridge::JsonQuote(written.binding) +
                          ",\"kind\":" + bridge::JsonQuote(written.kind) + "}]}";
        return bridge::Ok(request, one);
    }
    if (request.cmd == "macros.list" || request.cmd == "macros.save" ||
        request.cmd == "macros.upload" || request.cmd == "macro.import" ||
        request.cmd == "macro.export" || request.cmd == "macros.delete" ||
        request.cmd == "macros.rename") {
        return HandleMacroRequest(request, "incott", device_key);
    }
    if (request.cmd == "dpi.get" || request.cmd == "dpi.set" || request.cmd == "dpi.stage") {
        return HandleDpiRequest(request);
    }
    if (request.cmd == "rate.get" || request.cmd == "rate.set" ||
        request.cmd == "misc.get" || request.cmd == "misc.set" ||
        request.cmd == "sensor.get" || request.cmd == "sensor.set" ||
        request.cmd == "led.get" || request.cmd == "led.set") {
        return HandleHardwareExtendedRequest(request);
    }
    return bridge::Failure(request, "not implemented: " + request.cmd);
}

void WriteStdoutUtf8(const std::string& utf8) {
    if (utf8.empty()) return;
    HANDLE h = GetStdHandle(STD_OUTPUT_HANDLE);
    DWORD mode = 0;
    if (h != nullptr && h != INVALID_HANDLE_VALUE && GetConsoleMode(h, &mode)) {
        const std::wstring wide = Widen(utf8);
        if (!wide.empty()) {
            DWORD written = 0;
            WriteConsoleW(h, wide.c_str(), static_cast<DWORD>(wide.size()), &written, nullptr);
            return;
        }
    }
    std::fwrite(utf8.data(), 1, utf8.size(), stdout);
    std::fflush(stdout);
}

int RunList(bool as_json) {
    RefreshDeviceCache();
    if (as_json) {
        WriteStdoutUtf8(bridge::DevicesJson() + "\n");
        return 0;
    }
    const std::vector<bridge::DeviceSnapshot> devices = bridge::Devices();
    if (devices.empty()) {
        WriteStdoutUtf8("no peripherals found\n");
        return 0;
    }
    std::string out;
    for (const bridge::DeviceSnapshot& d : devices) {
        std::string status;
        if (!d.connected) {
            status = "offline";
        } else if (d.battery >= 0) {
            status = std::to_string(d.battery) + "%";
            if (d.charging == 2) status += " (charging)";
        } else if (!d.level.empty()) {
            status = d.level;
        } else if (d.mode == "wired") {
            status = "wired (no battery report)";
        } else {
            status = "connected";
        }
        const std::string& via = d.via.empty() ? d.mode : d.via;
        out += d.name + "  [" + via + "]  (" + d.kind + ")  " + status + "  (" + d.key + ")\n";
    }
    WriteStdoutUtf8(out);
    return 0;
}

int RunCli(bool probe) {
    incott::Device device;
    if (const int rc = OpenCliDevice(device)) return rc;
    std::wstring error;

    if (probe) {
        incott::Frame request = incott::MakeRead(incott::kCmdModel, nullptr, 0);
        incott::Frame reply{};
        if (!device.Feature(request.bytes, reply.bytes, error)) {
            std::fwprintf(stderr, L"error: %ls\n", error.c_str());
            return 1;
        }
        const incott::DeviceInfo info = incott::ParseModel(reply, device.IsWireless());
        std::wprintf(L"probe: model=%ls receiver=%ls psd=0x%02X sensor=%ls\n",
                     incott::ModelName(info.model), incott::ReceiverName(info.receiver),
                     static_cast<unsigned>(info.psd), incott::SensorName(info.sensor));
        return (info.model == 2 || info.model == 3) ? 0 : 1;
    }

    incott::Frame request = incott::MakeRead(incott::kCmdStatus, nullptr, 0);
    incott::Frame reply{};
    if (!device.Feature(request.bytes, reply.bytes, error)) {
        std::fwprintf(stderr, L"error: device sleeping or not responding (%ls)\n", error.c_str());
        return 1;
    }
    const int percent = incott::BatteryPercent(reply);
    if (percent < 0) {
        std::fwprintf(stderr, L"error: invalid 0x89 status frame\n");
        return 1;
    }
    std::wprintf(L"status: battery=%d%%\n", percent);
    return 0;
}

// Headless check for the WebView2 host: opens the real page off-screen, waits for it
// to render, prints the report the page posts, then exits. No port is opened.
int RunWebViewSelftest(SelftestMode mode) {
    g_instance = GetModuleHandleW(nullptr);
    if (FAILED(CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED))) {
        std::fwprintf(stderr, L"error: CoInitializeEx failed\n");
        return 1;
    }
    if (!RegisterClasses()) {
        std::fwprintf(stderr, L"error: could not register the window classes\n");
        CoUninitialize();
        return 1;
    }
    g_selftest = (mode != SelftestMode::kNone);
    g_selftest_mode = mode;

    HWND window = CreateWindowExW(0, kSettingsClass, L"PeriphHub selftest", WS_OVERLAPPEDWINDOW,
                                  CW_USEDEFAULT, CW_USEDEFAULT, 900, 650, nullptr, nullptr,
                                  g_instance, nullptr);
    if (window == nullptr) {
        std::fwprintf(stderr, L"error: could not create the selftest window\n");
        CoUninitialize();
        return 1;
    }
    const UINT timeout_ms = (mode == SelftestMode::kF26) ? 30000u : 20000u;
    SetTimer(window, kSelftestTimer, timeout_ms, nullptr);

    // Keep pumping: WebView2 delivers the page's message on this thread. A reply
    // that arrives calls respond() and then PostQuitMessage(0) from the window
    // procedure; the timer is only the bounded safety net.
    int exit_code = 1;
    MSG message{};
    for (;;) {
        const BOOL got = GetMessageW(&message, nullptr, 0, 0);
        if (got <= 0) {
            if (got == 0 && message.message == WM_QUIT) {
                exit_code = static_cast<int>(message.wParam);
            }
            break;
        }
        TranslateMessage(&message);
        DispatchMessageW(&message);
    }
    KillTimer(window, kSelftestTimer);
    // The settings window's WM_CREATE starts the shared worker; join it here
    // because this path returns before wWinMain's final StopWorker().
    DestroyWindow(window);
    StopWorker(nullptr);
    CoUninitialize();
    return exit_code;
}

}  // namespace

namespace host {

std::string HandleIncottRequest(const bridge::Request& request, const std::string& key) {
    return ::HandleIncottRequest(request, key);
}

std::string HandleMacroRequest(const bridge::Request& request,
                               const std::string& driver_name,
                               const std::string& key) {
    return ::HandleMacroRequest(request, driver_name, key);
}

void ClearButtonMacroBind(int button_index) {
    if (g_macros.Binding(button_index) >= 0) {
        g_macros.SetBinding(button_index, -1);
        std::wstring save_error;
        if (!SaveMacroLibrary(g_macros, &save_error)) {
            std::fwprintf(stderr, L"warning: macro library bind clear not saved: %ls\n",
                          save_error.c_str());
        }
    }
}

std::wstring AppDataDir() {
    // Test hook: the sandboxed CLI cannot write %LOCALAPPDATA%, so acceptance runs
    // point this at the workspace.
    wchar_t override_dir[MAX_PATH] = {};
    const DWORD override_len =
        GetEnvironmentVariableW(L"PERIPHHUB_DATA_DIR", override_dir, MAX_PATH);
    if (override_len > 0 && override_len < MAX_PATH) {
        return std::wstring(override_dir);
    }
    PWSTR known_path = nullptr;
    if (SUCCEEDED(SHGetKnownFolderPath(FOLDERID_LocalAppData, 0, nullptr, &known_path)) &&
        known_path != nullptr && *known_path != L'\0') {
        std::wstring result = std::wstring(known_path) + L"\\PeriphHub";
        CoTaskMemFree(known_path);
        return result;
    }
    if (known_path != nullptr) CoTaskMemFree(known_path);
    wchar_t buffer[MAX_PATH] = {};
    const DWORD length = GetEnvironmentVariableW(L"LOCALAPPDATA", buffer, MAX_PATH);
    if (length == 0 || length >= MAX_PATH) return {};
    return std::wstring(buffer) + L"\\PeriphHub";
}

bool ReadAllBytes(const std::wstring& path, std::string* out, std::wstring* error,
                  bool* not_found) {
    if (not_found != nullptr) *not_found = false;
    HANDLE file = CreateFileW(path.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING,
                              FILE_ATTRIBUTE_NORMAL, nullptr);
    if (file == INVALID_HANDLE_VALUE) {
        const DWORD code = GetLastError();
        // Only "nothing stored yet" is not an error; a locked/broken file is.
        if (code == ERROR_FILE_NOT_FOUND || code == ERROR_PATH_NOT_FOUND) {
            if (not_found != nullptr) *not_found = true;
        }
        *error = L"cannot open " + path + L" (error " + std::to_wstring(code) + L")";
        return false;
    }
    LARGE_INTEGER size{};
    if (!GetFileSizeEx(file, &size) ||
        size.QuadPart > static_cast<LONGLONG>(incott::kMacroFileMaxBytes)) {
        CloseHandle(file);
        *error = L"file is missing or larger than 128 KB";
        return false;
    }
    out->assign(static_cast<std::size_t>(size.QuadPart), '\0');
    DWORD read = 0;
    const bool ok = out->empty() || ReadFile(file, out->data(), static_cast<DWORD>(out->size()),
                                             &read, nullptr) != 0;
    CloseHandle(file);
    if (!ok) {
        *error = L"cannot read " + path;
        return false;
    }
    out->resize(read);
    return true;
}

void EnsureParentDirectories(const std::wstring& path) {
    const std::size_t slash = path.find_last_of(L"\\/");
    if (slash == std::wstring::npos || slash == 0) return;
    std::wstring dir = path.substr(0, slash);
    std::size_t start = 0;
    if (dir.size() >= 2 && dir[1] == L':') {
        start = 3;
    } else if (dir.rfind(L"\\\\", 0) == 0) {
        const std::size_t server_end = dir.find_first_of(L"\\/", 2);
        if (server_end == std::wstring::npos) return;
        const std::size_t share_end = dir.find_first_of(L"\\/", server_end + 1);
        if (share_end == std::wstring::npos) return;
        start = share_end + 1;
    }
    for (std::size_t pos = start; pos <= dir.size(); ++pos) {
        if (pos == dir.size() || dir[pos] == L'\\' || dir[pos] == L'/') {
            if (pos > start) {
                CreateDirectoryW(dir.substr(0, pos).c_str(), nullptr);
            }
        }
    }
}

// Write to `path.tmp`, flush it, then atomically replace `path`. A crash between
// the two steps leaves the old library intact instead of a half-written file.
bool WriteAllBytesAtomic(const std::wstring& path, const std::string& bytes,
                         std::wstring* error) {
    EnsureParentDirectories(path);
    const std::wstring temp = path + L".tmp";
    HANDLE file = CreateFileW(temp.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS,
                              FILE_ATTRIBUTE_NORMAL, nullptr);
    if (file == INVALID_HANDLE_VALUE) {
        *error = L"cannot open for write " + temp + L" (error " +
                 std::to_wstring(GetLastError()) + L")";
        return false;
    }
    bool ok = true;
    DWORD failure = 0;
    std::size_t offset = 0;
    while (offset < bytes.size()) {
        const std::size_t remaining = bytes.size() - offset;
        const DWORD chunk = static_cast<DWORD>(remaining > 0x1000000u ? 0x1000000u : remaining);
        DWORD written = 0;
        if (!WriteFile(file, bytes.data() + offset, chunk, &written, nullptr) || written == 0) {
            failure = GetLastError();
            ok = false;
            break;
        }
        offset += written;
    }
    if (ok && !FlushFileBuffers(file)) {
        failure = GetLastError();
        ok = false;
    }
    CloseHandle(file);
    if (!ok) {
        DeleteFileW(temp.c_str());
        *error = L"cannot write " + temp + L" (error " + std::to_wstring(failure) + L")";
        return false;
    }
    if (!MoveFileExW(temp.c_str(), path.c_str(),
                     MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH)) {
        failure = GetLastError();
        DeleteFileW(temp.c_str());
        *error = L"cannot replace " + path + L" (error " + std::to_wstring(failure) + L")";
        return false;
    }
    return true;
}

}  // namespace host

int WINAPI wWinMain(HINSTANCE instance, HINSTANCE, LPWSTR, int) {
    static bool s_drivers_registered = false;
    if (!s_drivers_registered) {
        drivers::RegisterBuiltinDrivers(drivers::Global());
        s_drivers_registered = true;
    }
    const HANDLE out_h = GetStdHandle(STD_OUTPUT_HANDLE);
    if (out_h == nullptr || out_h == INVALID_HANDLE_VALUE || GetFileType(out_h) == FILE_TYPE_UNKNOWN) {
        if (AttachConsole(ATTACH_PARENT_PROCESS)) {
            std::FILE* fp = nullptr;
            freopen_s(&fp, "CONOUT$", "w", stdout);
            freopen_s(&fp, "CONOUT$", "w", stderr);
        }
    }
    bool probe = false;
    bool status = false;
    bool info = false;
    bool list_devices = false;
    bool list_json = false;
    bool webview_selftest = false;
    bool webview_selftest_write = false;
    bool webview_selftest_f26 = false;
    bool open_settings = false;
    bool have_dpi = false;
    int dpi_value = 0;
    bool keys = false;
    bool have_set_key = false;
    int set_key_index = 0;
    std::wstring set_key_name;
    bool have_macro = false;
    bool macro_apply = false;
    int macro_key_index = 0;
    std::wstring macro_file;
    bool have_bridge = false;
    std::string bridge_request;
    int argc = 0;
    LPWSTR* argv = CommandLineToArgvW(GetCommandLineW(), &argc);
    if (argv != nullptr) {
        for (int i = 1; i < argc; ++i) {
            if (lstrcmpiW(argv[i], L"--help") == 0 || lstrcmpiW(argv[i], L"-h") == 0 ||
                lstrcmpiW(argv[i], L"/?") == 0) {
                WriteStdoutUtf8(
                    "PeriphHub - Wireless peripheral battery monitor & driver utility\n\n"
                    "Usage:\n"
                    "  PeriphHub.exe                    Start tray icon and taskbar battery widget\n"
                    "  PeriphHub.exe --settings         Start and open the settings window\n"
                    "  PeriphHub.exe --list             List detected peripherals and battery levels\n"
                    "  PeriphHub.exe --list-json        Output detected peripherals as JSON\n"
                    "  PeriphHub.exe --bridge <json>    Run a single JSON bridge request\n"
                    "  PeriphHub.exe --help             Show this help message\n");
                LocalFree(argv);
                return 0;
            } else if (lstrcmpiW(argv[i], L"--probe") == 0) {
                probe = true;
            } else if (lstrcmpiW(argv[i], L"--status") == 0) {
                status = true;
            } else if (lstrcmpiW(argv[i], L"--info") == 0) {
                info = true;
            } else if (lstrcmpiW(argv[i], L"--list") == 0) {
                list_devices = true;
            } else if (lstrcmpiW(argv[i], L"--list-json") == 0) {
                list_json = true;
            } else if (lstrcmpiW(argv[i], L"--dpi") == 0) {
                wchar_t* end = nullptr;
                if (i + 1 >= argc) {
                    std::fwprintf(stderr, L"error: --dpi needs a value\n");
                    LocalFree(argv);
                    return 3;
                }
                const long parsed = std::wcstol(argv[++i], &end, 10);
                if (end == argv[i] || *end != L'\0') {
                    std::fwprintf(stderr, L"error: --dpi expects an integer\n");
                    LocalFree(argv);
                    return 3;
                }
                dpi_value = static_cast<int>(parsed);
                have_dpi = true;
            } else if (lstrcmpiW(argv[i], L"--keys") == 0) {
                keys = true;
            } else if (lstrcmpiW(argv[i], L"--set-key") == 0) {
                if (i + 2 >= argc) {
                    std::fwprintf(stderr, L"error: --set-key needs <index> <key>\n");
                    LocalFree(argv);
                    return 3;
                }
                wchar_t* end = nullptr;
                const long parsed = std::wcstol(argv[++i], &end, 10);
                if (end == argv[i] || *end != L'\0') {
                    std::fwprintf(stderr, L"error: --set-key index must be an integer\n");
                    LocalFree(argv);
                    return 3;
                }
                set_key_index = static_cast<int>(parsed);
                set_key_name = argv[++i];
                have_set_key = true;
            } else if (lstrcmpiW(argv[i], L"--macro-upload") == 0 ||
                       lstrcmpiW(argv[i], L"--macro-apply") == 0) {
                const bool apply = lstrcmpiW(argv[i], L"--macro-apply") == 0;
                if (i + 2 >= argc) {
                    std::fwprintf(stderr, L"error: %ls needs <key-index> <file.json>\n",
                                  apply ? L"--macro-apply" : L"--macro-upload");
                    LocalFree(argv);
                    return 3;
                }
                wchar_t* end = nullptr;
                const long parsed = std::wcstol(argv[++i], &end, 10);
                if (end == argv[i] || *end != L'\0') {
                    std::fwprintf(stderr, L"error: macro key index must be an integer\n");
                    LocalFree(argv);
                    return 3;
                }
                macro_key_index = static_cast<int>(parsed);
                macro_file = argv[++i];
                macro_apply = apply;
                have_macro = true;
            } else if (lstrcmpiW(argv[i], L"--webview-selftest") == 0) {
                webview_selftest = true;
            } else if (lstrcmpiW(argv[i], L"--webview-selftest-write") == 0) {
                webview_selftest_write = true;
            } else if (lstrcmpiW(argv[i], L"--webview-selftest-f26") == 0) {
                webview_selftest_f26 = true;
            } else if (lstrcmpiW(argv[i], L"--settings") == 0) {
                open_settings = true;
            } else if (lstrcmpiW(argv[i], L"--bridge") == 0) {
                if (i + 1 >= argc) {
                    std::fwprintf(stderr, L"error: --bridge needs a JSON request\n");
                    LocalFree(argv);
                    return 3;
                }
                bridge_request = Narrow(argv[++i]);
                have_bridge = true;
            } else if (argv[i][0] == L'-' || argv[i][0] == L'/') {
                std::fwprintf(stderr, L"error: unknown option: %ls (use --help)\n", argv[i]);
                LocalFree(argv);
                return 3;
            }
        }
        LocalFree(argv);
    }
    LoadHubSettings();
    {
        std::wstring macro_error;
        if (!LoadMacroLibrary(g_macros, &macro_error)) {
            std::fwprintf(stderr, L"error: macro library not loaded: %ls\n", macro_error.c_str());
        } else if (!macro_error.empty()) {
            std::fwprintf(stderr, L"warning: macro library: %ls\n", macro_error.c_str());
        }
    }
    if (webview_selftest_f26) return RunWebViewSelftest(SelftestMode::kF26);
    if (webview_selftest_write) return RunWebViewSelftest(SelftestMode::kWrite);
    if (webview_selftest) return RunWebViewSelftest(SelftestMode::kRender);
    if (have_bridge) {
        // WP8: only pre-warm the live device cache when `devices.get` asks for it;
        // invalid requests and cache-free commands (hub.*, macros.*, led.*) skip HID
        // setup completely, and exit code mirrors `ok` (0 on ok:true, 1 on ok:false).
        bridge::Request parsed_req;
        std::string parse_err;
        if (bridge::ParseRequest(bridge_request, &parsed_req, &parse_err) &&
            (parsed_req.cmd == "devices.get" || parsed_req.cmd == "device.info" ||
             parsed_req.cmd == "device.rename")) {
            RefreshDeviceCache();
        }
        const std::string reply = BridgeHandler(bridge_request);
        std::printf("%s\n", reply.c_str());
        json::Value doc;
        std::string reply_err;
        bool ok = false;
        return (json::Parse(reply, &doc, &reply_err) && doc.GetBool("ok", &ok) && ok) ? 0 : 1;
    }
    if (list_devices || list_json) return RunList(list_json);
    if (info) return RunInfo();
    if (have_set_key) return RunSetKey(set_key_index, set_key_name);
    if (have_macro) return RunMacroUpload(macro_key_index, macro_file, macro_apply);
    if (keys) return RunKeys();
    if (have_dpi) return RunDpi(dpi_value);
    if (probe || status) return RunCli(probe);

    g_instance = instance;

    if (FAILED(CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED))) return 1;

    HANDLE mutex = CreateMutexW(nullptr, TRUE, L"Local\\PeriphHub");
    const DWORD mutex_error = GetLastError();
    if (mutex == nullptr) {
        std::fwprintf(stderr, L"error: CreateMutexW failed (error %lu)\n", mutex_error);
        CoUninitialize();
        return 1;
    }
    if (mutex_error == ERROR_ALREADY_EXISTS) {
        CloseHandle(mutex);
        if (HWND existing = FindWindowW(kMainClass, nullptr)) {
            PostMessageW(existing, WM_COMMAND, kMenuSettings, 0);
        } else {
            MessageBoxW(nullptr, L"PeriphHub 已在后台运行（请查看右下角系统托盘）。", kAppName,
                        MB_OK | MB_ICONINFORMATION);
        }
        CoUninitialize();
        return 0;
    }

    if (!RegisterClasses()) {
        CloseHandle(mutex);
        CoUninitialize();
        return 1;
    }

    g_main = CreateWindowExW(WS_EX_TOOLWINDOW, kMainClass, kAppName, WS_POPUP, 0, 0, 0, 0,
                             nullptr, nullptr, instance, nullptr);
    if (g_main == nullptr) {
        CloseHandle(mutex);
        CoUninitialize();
        return 1;
    }

    // The poll timer and page requests need the worker even in tray-only mode,
    // where the settings window is never created.
    StartWorker();

    // The tray icon is added once the message loop is running (kInitTimer): adding it
    // straight after CreateWindow fails with ERROR_ACCESS_DENIED on this box.
    SetTimer(g_main, kInitTimer, 300, nullptr);

    // --settings opens the page straight away, so the app is usable even where
    // the tray icon cannot be added (remote sessions, restricted desktops).
    if (open_settings) ShowSettings();

    MSG message{};
    while (GetMessageW(&message, nullptr, 0, 0) > 0) {
        TranslateMessage(&message);
        DispatchMessageW(&message);
    }

    if (mutex != nullptr) CloseHandle(mutex);
    // Safety net for exits that skipped a window's WM_DESTROY.
    StopWorker(nullptr);
    CoUninitialize();
    return 0;
}
