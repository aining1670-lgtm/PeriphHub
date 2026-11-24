#include "webview.h"

#include <objbase.h>
#include <shlobj.h>

#include <cstdio>
#include <cstring>
#include <cwchar>
#include <memory>
#include <utility>

#include "text.h"

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunknown-pragmas"
#include "WebView2.h"
#pragma GCC diagnostic pop

// WebView2.h does not give MinGW a __declspec(uuid()) on its interfaces, so __uuidof
// resolves through these specialisations. GUIDs copied verbatim from WebView2.h.
__CRT_UUID_DECL(ICoreWebView2CreateCoreWebView2EnvironmentCompletedHandler, 0x4e8a3389, 0xc9d8,
                0x4bd2, 0xb6, 0xb5, 0x12, 0x4f, 0xee, 0x6c, 0xc1, 0x4d)
__CRT_UUID_DECL(ICoreWebView2CreateCoreWebView2ControllerCompletedHandler, 0x6c4819f3, 0xc9b7,
                0x4260, 0x81, 0x27, 0xc9, 0xf5, 0xbd, 0xe7, 0xf6, 0x8c)
__CRT_UUID_DECL(ICoreWebView2WebMessageReceivedEventHandler, 0x57213f19, 0x00e6, 0x49fa, 0x8e, 0x07,
                0x89, 0x8e, 0xa0, 0x1e, 0xcb, 0xd2)
__CRT_UUID_DECL(ICoreWebView2NavigationStartingEventHandler, 0x9adbe429, 0xf36d, 0x432b,
                0x9d, 0xdc, 0xf8, 0x88, 0x1f, 0xbd, 0x76, 0xe3)
__CRT_UUID_DECL(ICoreWebView2NewWindowRequestedEventHandler, 0xd4c185fe, 0xc81c, 0x4989,
                0x97, 0xaf, 0x2d, 0x3f, 0xa7, 0xab, 0x56, 0x51)

namespace webview {
namespace {

// Same limit as before: requests above 256 KB are answered with an error instead
// of being dispatched.
constexpr std::size_t kMaxMessageBytes = 256u * 1024u;

// Shared IUnknown implementation for our single-interface callbacks.
#define PERIPHHUB_COM_OBJECT(interface_name)                                          \
public:                                                                               \
    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void** object) override {    \
        if (object == nullptr) return E_POINTER;                                      \
        if (riid == IID_IUnknown || riid == __uuidof(interface_name)) {               \
            *object = static_cast<interface_name*>(this);                             \
            AddRef();                                                                 \
            return S_OK;                                                              \
        }                                                                             \
        *object = nullptr;                                                            \
        return E_NOINTERFACE;                                                         \
    }                                                                                 \
    ULONG STDMETHODCALLTYPE AddRef() override {                                         \
        return static_cast<ULONG>(InterlockedIncrement(&refs_));                       \
    }                                                                                 \
    ULONG STDMETHODCALLTYPE Release() override {                                       \
        const ULONG left = static_cast<ULONG>(InterlockedDecrement(&refs_));          \
        if (left == 0) delete this;                                                   \
        return left;                                                                  \
    }                                                                                 \
                                                                                      \
private:                                                                              \
    LONG refs_ = 1;

class EnvironmentReady final : public ICoreWebView2CreateCoreWebView2EnvironmentCompletedHandler {
public:
    using Fn = std::function<void(HRESULT, ICoreWebView2Environment*)>;

    explicit EnvironmentReady(Fn fn) : fn_(std::move(fn)) {}

    HRESULT STDMETHODCALLTYPE Invoke(HRESULT result,
                                    ICoreWebView2Environment* environment) override {
        if (fn_) fn_(result, environment);
        return S_OK;
    }

    PERIPHHUB_COM_OBJECT(ICoreWebView2CreateCoreWebView2EnvironmentCompletedHandler)

private:
    ~EnvironmentReady() = default;
    Fn fn_;
};

class ControllerReady final : public ICoreWebView2CreateCoreWebView2ControllerCompletedHandler {
public:
    using Fn = std::function<void(HRESULT, ICoreWebView2Controller*)>;

    explicit ControllerReady(Fn fn) : fn_(std::move(fn)) {}

    HRESULT STDMETHODCALLTYPE Invoke(HRESULT result,
                                    ICoreWebView2Controller* controller) override {
        if (fn_) fn_(result, controller);
        return S_OK;
    }

    PERIPHHUB_COM_OBJECT(ICoreWebView2CreateCoreWebView2ControllerCompletedHandler)

private:
    ~ControllerReady() = default;
    Fn fn_;
};

class MessageReceived final : public ICoreWebView2WebMessageReceivedEventHandler {
public:
    using Fn = std::function<void(const std::wstring&, const std::wstring&)>;

    explicit MessageReceived(Fn fn) : fn_(std::move(fn)) {}

    HRESULT STDMETHODCALLTYPE Invoke(ICoreWebView2*,
                                    ICoreWebView2WebMessageReceivedEventArgs* args) override {
        if (args == nullptr) return S_OK;

        LPWSTR json = nullptr;
        if (FAILED(args->get_WebMessageAsJson(&json)) || json == nullptr) return S_OK;

        LPWSTR source = nullptr;
        if (FAILED(args->get_Source(&source)) || source == nullptr) {
            CoTaskMemFree(json);
            return S_OK;
        }
        if (fn_) fn_(json, source);
        CoTaskMemFree(source);
        CoTaskMemFree(json);
        return S_OK;
    }

    PERIPHHUB_COM_OBJECT(ICoreWebView2WebMessageReceivedEventHandler)

private:
    ~MessageReceived() = default;
    Fn fn_;
};

class NavigationStarting final : public ICoreWebView2NavigationStartingEventHandler {
public:
    using Fn = std::function<void(ICoreWebView2*, ICoreWebView2NavigationStartingEventArgs*)>;

    explicit NavigationStarting(Fn fn) : fn_(std::move(fn)) {}

    HRESULT STDMETHODCALLTYPE Invoke(ICoreWebView2* sender,
                                     ICoreWebView2NavigationStartingEventArgs* args) override {
        if (fn_) fn_(sender, args);
        return S_OK;
    }

    PERIPHHUB_COM_OBJECT(ICoreWebView2NavigationStartingEventHandler)

private:
    ~NavigationStarting() = default;
    Fn fn_;
};

class NewWindowRequested final : public ICoreWebView2NewWindowRequestedEventHandler {
public:
    using Fn = std::function<void(ICoreWebView2NewWindowRequestedEventArgs*)>;

    explicit NewWindowRequested(Fn fn) : fn_(std::move(fn)) {}

    HRESULT STDMETHODCALLTYPE Invoke(ICoreWebView2*,
                                    ICoreWebView2NewWindowRequestedEventArgs* args) override {
        if (fn_) fn_(args);
        return S_OK;
    }

    PERIPHHUB_COM_OBJECT(ICoreWebView2NewWindowRequestedEventHandler)

private:
    ~NewWindowRequested() = default;
    Fn fn_;
};

std::wstring FileUrl(const std::wstring& windows_path) {
    const std::string utf8 = text::ToUtf8(windows_path);
    std::string url = "file:///";
    char buffer[8];
    for (const unsigned char byte : utf8) {
        if (byte == '\\') {
            url += '/';
            continue;
        }
        const bool plain = (byte >= 'A' && byte <= 'Z') || (byte >= 'a' && byte <= 'z')
                           || (byte >= '0' && byte <= '9') || byte == '-' || byte == '_'
                           || byte == '.' || byte == '~' || byte == '/' || byte == ':';
        if (plain) {
            url += static_cast<char>(byte);
        } else {
            std::snprintf(buffer, sizeof(buffer), "%%%02X", static_cast<unsigned>(byte));
            url += buffer;
        }
    }
    return text::ToWide(url);
}

// Directory holding the running exe; WebView2Loader.dll and the user data folder
// live next to it.
std::wstring ModuleDirectory() {
    wchar_t module_path[MAX_PATH]{};
    const DWORD length = GetModuleFileNameW(nullptr, module_path, ARRAYSIZE(module_path));
    if (length == 0) return std::wstring(L".");
    const std::wstring path(module_path, length);
    const std::size_t slash = path.find_last_of(L"\\/");
    return slash == std::wstring::npos ? std::wstring(L".") : path.substr(0, slash);
}

// True when `source` is the loaded page, ignoring its query/fragment (the page
// itself may be loaded with #selftest, and the document URL may or may not carry
// it). Case-insensitive because file:// URLs are.
bool IsPageUrl(const std::wstring& page_url, const std::wstring& source) {
    const std::wstring base = page_url.substr(0, page_url.find_first_of(L"#?"));
    if (base.empty() || source.size() < base.size()) return false;
    if (_wcsnicmp(source.c_str(), base.c_str(), base.size()) != 0) return false;
    if (source.size() == base.size()) return true;
    const wchar_t tail = source[base.size()];
    return tail == L'#' || tail == L'?';
}

bool IsLowIntegrityProcess() {
    HANDLE token = nullptr;
    if (!OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &token)) return false;
    DWORD len = 0;
    GetTokenInformation(token, TokenIntegrityLevel, nullptr, 0, &len);
    if (len == 0) {
        CloseHandle(token);
        return false;
    }
    std::string buf(len, '\0');
    auto* til = reinterpret_cast<TOKEN_MANDATORY_LABEL*>(buf.data());
    bool is_low = false;
    if (GetTokenInformation(token, TokenIntegrityLevel, til, len, &len) &&
        til->Label.Sid != nullptr) {
        const UCHAR count = *GetSidSubAuthorityCount(til->Label.Sid);
        if (count > 0) {
            const DWORD rid = *GetSidSubAuthority(til->Label.Sid, count - 1);
            is_low = (rid < SECURITY_MANDATORY_MEDIUM_RID);
        }
    }
    CloseHandle(token);
    return is_low;
}

bool EnsureWritableDirectory(const std::wstring& dir) {
    if (dir.empty()) return false;
    if (CreateDirectoryW(dir.c_str(), nullptr) == 0 &&
        GetLastError() != ERROR_ALREADY_EXISTS) {
        return false;
    }
    const std::wstring probe = dir + L"\\.bh_write_probe";
    HANDLE h = CreateFileW(probe.c_str(), GENERIC_WRITE,
                           FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
                           nullptr, CREATE_ALWAYS,
                           FILE_ATTRIBUTE_TEMPORARY | FILE_FLAG_DELETE_ON_CLOSE, nullptr);
    if (h == INVALID_HANDLE_VALUE) return false;
    CloseHandle(h);
    return true;
}

std::wstring ResolveRuntimeDataRoot() {
    wchar_t override_dir[MAX_PATH]{};
    const DWORD override_len =
        GetEnvironmentVariableW(L"PERIPHHUB_DATA_DIR", override_dir, MAX_PATH);
    if (override_len > 0 && override_len < MAX_PATH) {
        const std::wstring base(override_dir, override_len);
        if (EnsureWritableDirectory(base)) return base;
    }

    if (!IsLowIntegrityProcess()) {
        std::wstring base_data_dir;
        PWSTR known_path = nullptr;
        if (SUCCEEDED(SHGetKnownFolderPath(FOLDERID_LocalAppData, 0, nullptr, &known_path)) &&
            known_path != nullptr && *known_path != L'\0') {
            base_data_dir = std::wstring(known_path) + L"\\PeriphHub";
        }
        if (known_path != nullptr) CoTaskMemFree(known_path);
        if (base_data_dir.empty()) {
            wchar_t local_buf[MAX_PATH]{};
            const DWORD local_len = GetEnvironmentVariableW(L"LOCALAPPDATA", local_buf, MAX_PATH);
            if (local_len > 0 && local_len < MAX_PATH) {
                base_data_dir = std::wstring(local_buf, local_len) + L"\\PeriphHub";
            }
        }
        if (!base_data_dir.empty() && EnsureWritableDirectory(base_data_dir)) {
            return base_data_dir;
        }
    }

    const std::wstring local_candidate = ModuleDirectory() + L"\\periphhub-webview2";
    if (EnsureWritableDirectory(local_candidate)) {
        return local_candidate;
    }

    wchar_t temp_buf[MAX_PATH]{};
    if (GetTempPathW(MAX_PATH, temp_buf) > 0) {
        const std::wstring temp_candidate = std::wstring(temp_buf) + L"PeriphHub-runtime";
        if (EnsureWritableDirectory(temp_candidate)) return temp_candidate;
    }
    return ModuleDirectory();
}

bool ExtractRcDataToFile(WORD resource_id, const std::wstring& dest_path) {
    HRSRC res = FindResourceW(nullptr, MAKEINTRESOURCEW(resource_id), RT_RCDATA);
    if (res == nullptr) return false;
    const DWORD size = SizeofResource(nullptr, res);
    if (size == 0) return false;
    HGLOBAL loaded = LoadResource(nullptr, res);
    if (loaded == nullptr) return false;
    const void* data = LockResource(loaded);
    if (data == nullptr) return false;

    // Skip disk write when the file already exists with the exact byte content.
    HANDLE existing = CreateFileW(dest_path.c_str(), GENERIC_READ,
                                  FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr,
                                  OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (existing != INVALID_HANDLE_VALUE) {
        LARGE_INTEGER file_sz{};
        bool same = false;
        if (GetFileSizeEx(existing, &file_sz) &&
            file_sz.QuadPart == static_cast<LONGLONG>(size)) {
            std::string buf(size, '\0');
            DWORD got = 0;
            if (ReadFile(existing, buf.data(), size, &got, nullptr) && got == size &&
                std::memcmp(buf.data(), data, size) == 0) {
                same = true;
            }
        }
        CloseHandle(existing);
        if (same) return true;
    }

    const std::wstring tmp_path = dest_path + L".tmp";
    HANDLE out = CreateFileW(tmp_path.c_str(), GENERIC_WRITE, 0, nullptr,
                             CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (out == INVALID_HANDLE_VALUE) {
        // Another process may already be using the target file (e.g. loaded DLL).
        const DWORD attr = GetFileAttributesW(dest_path.c_str());
        return attr != INVALID_FILE_ATTRIBUTES && (attr & FILE_ATTRIBUTE_DIRECTORY) == 0;
    }
    DWORD written = 0;
    const BOOL ok = WriteFile(out, data, size, &written, nullptr);
    CloseHandle(out);
    if (!ok || written != size) {
        DeleteFileW(tmp_path.c_str());
        return false;
    }
    if (!MoveFileExW(tmp_path.c_str(), dest_path.c_str(),
                     MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH)) {
        DeleteFileW(tmp_path.c_str());
        const DWORD attr = GetFileAttributesW(dest_path.c_str());
        return attr != INVALID_FILE_ATTRIBUTES && (attr & FILE_ATTRIBUTE_DIRECTORY) == 0;
    }
    return true;
}

bool EnsureEmbeddedWebAssets(const std::wstring& root_dir, std::wstring* index_out) {
    const std::wstring web_dir = root_dir + L"\\web";
    CreateDirectoryW(root_dir.c_str(), nullptr);
    CreateDirectoryW(web_dir.c_str(), nullptr);

    const std::wstring idx = web_dir + L"\\index.html";
    const std::wstring tw = web_dir + L"\\tailwind.js";

    if (!ExtractRcDataToFile(101, idx) || !ExtractRcDataToFile(102, tw)) {
        return false;
    }
    if (index_out != nullptr) *index_out = idx;
    return true;
}

bool EnsureEmbeddedLoaderDll(const std::wstring& root_dir, std::wstring* dll_out) {
    CreateDirectoryW(root_dir.c_str(), nullptr);
    const std::wstring dll_path = root_dir + L"\\WebView2Loader.dll";
    if (!ExtractRcDataToFile(105, dll_path)) return false;
    if (dll_out != nullptr) *dll_out = dll_path;
    return true;
}

}  // namespace

struct Host::Impl final : public std::enable_shared_from_this<Host::Impl> {
    HWND parent = nullptr;
    Handler handler;
    std::wstring page_path;
    std::wstring page_url;
    std::wstring error;
    int width = 0;
    int height = 0;
    bool started = false;
    // Bumped by Close(): callbacks that were scheduled by an older generation are
    // ignored, so a late WebView2 completion can never touch a closed host.
    bool closed = false;
    unsigned long long generation = 0;
    ICoreWebView2Controller* controller = nullptr;
    ICoreWebView2* page = nullptr;
    EventRegistrationToken message_token{};
    EventRegistrationToken navigation_token{};
    EventRegistrationToken new_window_token{};
    bool message_registered = false;
    bool navigation_registered = false;
    bool new_window_registered = false;

    bool Live(unsigned long long gen) const { return !closed && generation == gen; }

    void OnEnvironment(unsigned long long gen, HRESULT result,
                       ICoreWebView2Environment* environment);
    void OnController(unsigned long long gen, HRESULT result, ICoreWebView2Controller* created);
    void OnMessage(const std::wstring& json, const std::wstring& source);
    void PostJson(const std::string& json);
    void ApplyBounds();
    void Shutdown();
};

void Host::Impl::OnEnvironment(unsigned long long gen, HRESULT result,
                               ICoreWebView2Environment* environment) {
    if (FAILED(result) || environment == nullptr) {
        error = L"could not create the WebView2 environment (is the runtime installed?)";
        return;
    }

    std::weak_ptr<Impl> weak = weak_from_this();
    auto* ready = new ControllerReady([weak, gen](HRESULT rc, ICoreWebView2Controller* created) {
        if (auto self = weak.lock(); self != nullptr && self->Live(gen)) {
            self->OnController(gen, rc, created);
        } else if (created != nullptr) {
            // Host is gone or restarted: close the orphaned controller window.
            created->Close();
        }
    });
    environment->CreateCoreWebView2Controller(parent, ready);
    ready->Release();
}

void Host::Impl::OnController(unsigned long long gen, HRESULT result,
                              ICoreWebView2Controller* created) {
    if (created != nullptr && !Live(gen)) {
        created->Close();
        return;
    }
    if (FAILED(result) || created == nullptr) {
        wchar_t hr_buf[96]{};
        std::swprintf(hr_buf, ARRAYSIZE(hr_buf),
                      L"could not create the WebView2 controller (hr=0x%08lX)",
                      static_cast<unsigned long>(result));
        error = hr_buf;
        return;
    }

    controller = created;
    controller->AddRef();
    ApplyBounds();

    if (FAILED(controller->get_CoreWebView2(&page)) || page == nullptr) {
        page = nullptr;
        error = L"could not get ICoreWebView2";
        return;
    }

    ICoreWebView2Settings* settings = nullptr;
    if (SUCCEEDED(page->get_Settings(&settings)) && settings != nullptr) {
        settings->put_AreDefaultContextMenusEnabled(FALSE);
        settings->put_IsStatusBarEnabled(FALSE);
        settings->put_AreDevToolsEnabled(FALSE);
        settings->put_AreDefaultScriptDialogsEnabled(FALSE);
        settings->put_AreHostObjectsAllowed(FALSE);
        settings->Release();
    }

    std::weak_ptr<Impl> weak = weak_from_this();
    auto* receiver = new MessageReceived(
        [weak, gen](const std::wstring& json, const std::wstring& source) {
            if (auto self = weak.lock(); self != nullptr && self->Live(gen)) {
                self->OnMessage(json, source);
            }
        });
    if (SUCCEEDED(page->add_WebMessageReceived(receiver, &message_token))) {
        message_registered = true;
    }
    receiver->Release();

    const std::wstring url = page_url;
    const std::wstring url_base = url.substr(0, url.find_first_of(L"#?"));
    auto* nav_handler = new NavigationStarting(
        [url_base](ICoreWebView2*, ICoreWebView2NavigationStartingEventArgs* args) {
            if (args == nullptr) return;
            LPWSTR uri_raw = nullptr;
            if (SUCCEEDED(args->get_Uri(&uri_raw)) && uri_raw != nullptr) {
                const std::wstring target_uri(uri_raw);
                CoTaskMemFree(uri_raw);
                // Same-document fragment/query navigation is allowed; anything
                // that leaves the loaded page is cancelled.
                const std::wstring target_base =
                    target_uri.substr(0, target_uri.find_first_of(L"#?"));
                if (_wcsicmp(target_base.c_str(), url_base.c_str()) != 0) {
                    args->put_Cancel(TRUE);
                }
            }
        });
    if (SUCCEEDED(page->add_NavigationStarting(nav_handler, &navigation_token))) {
        navigation_registered = true;
    }
    nav_handler->Release();

    auto* popup_handler = new NewWindowRequested(
        [](ICoreWebView2NewWindowRequestedEventArgs* args) {
            if (args != nullptr) args->put_Handled(TRUE);
        });
    if (SUCCEEDED(page->add_NewWindowRequested(popup_handler, &new_window_token))) {
        new_window_registered = true;
    }
    popup_handler->Release();

    page->Navigate(url.c_str());
    started = true;
}

void Host::Impl::OnMessage(const std::wstring& json, const std::wstring& source) {
    if (closed || page == nullptr) return;
    if (!IsPageUrl(page_url, source)) return;

    const std::string request = text::ToUtf8(json);
    if (request.empty()) return;
    if (request.size() > kMaxMessageBytes) {
        PostJson("{\"ok\":false,\"error\":\"IPC message exceeds 256 KB limit\"}");
        return;
    }
    if (!handler) {
        PostJson("{\"ok\":false}");
        return;
    }

    // The handler owns the reply timing; the weak_ptr drops replies once we close.
    std::weak_ptr<Impl> weak = weak_from_this();
    handler(request, [weak](std::string reply) {
        if (auto self = weak.lock(); self != nullptr) {
            self->PostJson(reply);
        }
    });
}

void Host::Impl::PostJson(const std::string& json) {
    if (closed || page == nullptr || json.empty()) return;
    const std::wstring wide = text::ToWide(json);
    if (wide.empty()) {
        std::fprintf(stderr, "error: PostJson failed to convert %zu-byte payload to UTF-16\n",
                     json.size());
        std::fflush(stderr);
        return;
    }
    const HRESULT hr = page->PostWebMessageAsJson(wide.c_str());
    if (FAILED(hr)) {
        const int preview_len = static_cast<int>(json.size() > 256 ? 256 : json.size());
        std::fprintf(stderr, "error: PostWebMessageAsJson failed (hr=0x%08lX): %.*s%s\n",
                     static_cast<unsigned long>(hr), preview_len, json.c_str(),
                     json.size() > 256 ? "..." : "");
        std::fflush(stderr);
    }
}

void Host::Impl::ApplyBounds() {
    if (controller == nullptr) return;
    const RECT bounds{0, 0, width, height};
    controller->put_Bounds(bounds);
}

void Host::Impl::Shutdown() {
    if (closed) return;
    closed = true;
    started = false;
    ++generation;

    if (page != nullptr) {
        if (message_registered) page->remove_WebMessageReceived(message_token);
        if (navigation_registered) page->remove_NavigationStarting(navigation_token);
        if (new_window_registered) page->remove_NewWindowRequested(new_window_token);
        message_registered = false;
        navigation_registered = false;
        new_window_registered = false;
        page->Release();
        page = nullptr;
    }
    message_token = EventRegistrationToken{};
    navigation_token = EventRegistrationToken{};
    new_window_token = EventRegistrationToken{};

    if (controller != nullptr) {
        controller->Close();
        controller->Release();
        controller = nullptr;
    }
    handler = nullptr;
}

Host::Host() : impl_(std::make_shared<Impl>()) {}

Host::~Host() {
    if (impl_ != nullptr) impl_->Shutdown();
}

bool Host::Start(HWND parent, const std::wstring& page_path, Handler handler,
                 std::wstring& error) {
    if (impl_->page != nullptr || impl_->controller != nullptr || impl_->started) {
        error = L"webview host already started";
        return false;
    }

    impl_->parent = parent;
    impl_->page_path = page_path;
    const std::wstring runtime_root = ResolveRuntimeDataRoot();

    // The page path may carry a fragment/query (for example index.html#selftest).
    // FileUrl() escapes '#' and '?', so convert the path part only and append the
    // suffix verbatim.
    std::wstring page_file = page_path;
    std::wstring page_suffix;
    const std::size_t split = page_path.find_first_of(L"#?");
    if (split != std::wstring::npos) {
        page_file = page_path.substr(0, split);
        page_suffix = page_path.substr(split);
    }
    DWORD page_attr = GetFileAttributesW(page_file.c_str());
    if (page_attr == INVALID_FILE_ATTRIBUTES || (page_attr & FILE_ATTRIBUTE_DIRECTORY) != 0) {
        std::wstring embedded_index;
        if (EnsureEmbeddedWebAssets(runtime_root, &embedded_index)) {
            page_file = embedded_index;
            page_attr = GetFileAttributesW(page_file.c_str());
        }
    }
    if (page_attr == INVALID_FILE_ATTRIBUTES || (page_attr & FILE_ATTRIBUTE_DIRECTORY) != 0) {
        impl_->error = L"web\\index.html is missing and embedded UI assets could not be extracted";
        error = impl_->error;
        return false;
    }
    impl_->page_url = FileUrl(page_file) + page_suffix;
    impl_->handler = std::move(handler);
    impl_->error.clear();
    impl_->closed = false;
    const unsigned long long generation = ++impl_->generation;

    static HMODULE s_loader = nullptr;
    if (s_loader == nullptr) {
        std::wstring loader_path = ModuleDirectory() + L"\\WebView2Loader.dll";
        const DWORD loader_attr = GetFileAttributesW(loader_path.c_str());
        if (loader_attr == INVALID_FILE_ATTRIBUTES || (loader_attr & FILE_ATTRIBUTE_DIRECTORY) != 0) {
            std::wstring embedded_dll;
            if (EnsureEmbeddedLoaderDll(runtime_root, &embedded_dll)) {
                loader_path = embedded_dll;
            }
        }
        // Only the resolved loader directory and System32 may provide the loader: no
        // current directory / PATH hijacking.
        s_loader = LoadLibraryExW(loader_path.c_str(), nullptr,
                                  LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR
                                      | LOAD_LIBRARY_SEARCH_SYSTEM32);
    }
    HMODULE loader = s_loader;
    if (loader == nullptr) {
        impl_->error = L"WebView2Loader.dll is missing and embedded loader could not be extracted";
        error = impl_->error;
        return false;
    }
    using CreateEnvironment = HRESULT(STDMETHODCALLTYPE*)(
        PCWSTR, PCWSTR, ICoreWebView2EnvironmentOptions*,
        ICoreWebView2CreateCoreWebView2EnvironmentCompletedHandler*);
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wcast-function-type"
    const auto create = reinterpret_cast<CreateEnvironment>(
        GetProcAddress(loader, "CreateCoreWebView2EnvironmentWithOptions"));
#pragma GCC diagnostic pop
    if (create == nullptr) {
        impl_->error = L"WebView2Loader.dll has no CreateCoreWebView2EnvironmentWithOptions";
        error = impl_->error;
        return false;
    }

    std::wstring user_data = runtime_root + L"\\webview2";
    if (!EnsureWritableDirectory(user_data)) {
        user_data = runtime_root;
    }

    std::weak_ptr<Impl> weak = impl_;
    auto* ready = new EnvironmentReady(
        [weak, generation](HRESULT rc, ICoreWebView2Environment* environment) {
            if (auto self = weak.lock(); self != nullptr && self->Live(generation)) {
                self->OnEnvironment(generation, rc, environment);
            }
        });
    const HRESULT hr = create(nullptr, user_data.c_str(), nullptr, ready);
    ready->Release();
    if (FAILED(hr)) {
        impl_->error = L"CreateCoreWebView2EnvironmentWithOptions failed";
        error = impl_->error;
        return false;
    }
    // The loader stays loaded for the lifetime of the process on purpose: one
    // load per run and no dangling entry points.
    return true;
}

void Host::Resize(int width, int height) {
    impl_->width = width;
    impl_->height = height;
    impl_->ApplyBounds();
}

void Host::Close() {
    if (impl_ != nullptr) impl_->Shutdown();
}

void Host::PostJson(const std::string& json) {
    if (impl_ != nullptr) impl_->PostJson(json);
}

bool Host::Started() const { return impl_->started; }

const std::wstring& Host::Error() const { return impl_->error; }

}  // namespace webview
