#pragma once
// Thin WebView2 host: one controller filling `parent`, one JSON message channel.
// The page talks to the bridge with chrome.webview.postMessage / addEventListener.
#include <windows.h>

#include <functional>
#include <memory>
#include <string>

namespace webview {

class Host {
public:
    // Receives the JSON text the page posted. The handler decides when to reply:
    // call `respond` with the JSON reply, on the UI thread, possibly later
    // (for example after marshalling back from a worker thread). Omitting the
    // reply is allowed.
    using Handler =
        std::function<void(const std::string& request, std::function<void(std::string)> respond)>;

    Host();
    ~Host();
    Host(const Host&) = delete;
    Host& operator=(const Host&) = delete;

    // Starts WebView2 asynchronously; navigation happens on the message loop, so
    // Started() turns true later. `page_path` is a Windows path to the HTML file.
    bool Start(HWND parent, const std::wstring& page_path, Handler handler, std::wstring& error);

    void Resize(int width, int height);
    void SetBackgroundColor(COLORREF rgb);
    void Close();
    void PostJson(const std::string& json);

    bool Started() const;
    const std::wstring& Error() const;

private:
    struct Impl;
    std::shared_ptr<Impl> impl_;
};

}  // namespace webview
