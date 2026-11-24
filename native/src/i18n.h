#pragma once
#include <string>
#include <utility>
#include <initializer_list>

inline const wchar_t* T(const wchar_t* en) { return en; }
inline std::wstring T(const std::wstring& en) { return en; }
inline std::wstring TF(const wchar_t* en, std::initializer_list<std::pair<const wchar_t*, std::wstring>> args) {
    std::wstring res = en;
    for (const auto& [key, val] : args) {
        std::wstring placeholder = L"{" + std::wstring(key) + L"}";
        size_t pos = res.find(placeholder);
        if (pos != std::wstring::npos) {
            res.replace(pos, placeholder.length(), val);
        }
    }
    return res;
}
