#include "text.h"

#include <windows.h>

#include <climits>
#include <cstddef>

namespace text {

std::string ToUtf8(const std::wstring& wide) {
    if (wide.empty()) return std::string();
    if (wide.size() > static_cast<std::size_t>(INT_MAX)) return std::string();

    const int length = static_cast<int>(wide.size());
    const int size = WideCharToMultiByte(CP_UTF8, 0, wide.data(), length, nullptr, 0, nullptr,
                                         nullptr);
    if (size <= 0) return std::string();

    std::string utf8(static_cast<std::size_t>(size), '\0');
    WideCharToMultiByte(CP_UTF8, 0, wide.data(), length, utf8.data(), size, nullptr, nullptr);
    return utf8;
}

std::wstring ToWide(const std::string& utf8) {
    if (utf8.empty()) return std::wstring();
    if (utf8.size() > static_cast<std::size_t>(INT_MAX)) return std::wstring();

    const int length = static_cast<int>(utf8.size());
    const int size = MultiByteToWideChar(CP_UTF8, 0, utf8.data(), length, nullptr, 0);
    if (size <= 0) return std::wstring();

    std::wstring wide(static_cast<std::size_t>(size), L'\0');
    MultiByteToWideChar(CP_UTF8, 0, utf8.data(), length, wide.data(), size);
    return wide;
}

}  // namespace text
