#pragma once
// UTF-8 <-> UTF-16 conversion helpers.
//
// Both functions take an explicit string length (std::string / std::wstring) so
// embedded NUL characters are preserved and no terminator is required in the
// input. The result never contains a trailing NUL, so its length is the number
// of code units actually written.
#include <string>

namespace text {

// Converts UTF-16 (std::wstring) to UTF-8. Invalid surrogates are replaced.
std::string ToUtf8(const std::wstring& wide);

// Converts UTF-8 to UTF-16 (std::wstring). Invalid bytes are replaced.
std::wstring ToWide(const std::string& utf8);

}  // namespace text
