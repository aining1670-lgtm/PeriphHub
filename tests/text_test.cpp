// Pure-function tests for the UTF-8 <-> UTF-16 helpers: no Win32 UI, no hardware.
#include <cstdio>
#include <string>

#include "../native/src/text.h"

namespace {

int g_failures = 0;

void Expect(bool condition, const char* what) {
    if (!condition) {
        std::printf("FAIL: %s\n", what);
        ++g_failures;
    }
}

}  // namespace

int main() {
    using text::ToUtf8;
    using text::ToWide;

    // Empty strings are empty, not a lone terminator.
    Expect(ToUtf8(std::wstring()).empty(), "empty wide -> empty utf8");
    Expect(ToWide(std::string()).empty(), "empty utf8 -> empty wide");
    Expect(ToUtf8(L"").empty(), "empty wide literal -> empty utf8");
    Expect(ToWide("").empty(), "empty utf8 literal -> empty wide");

    // ASCII survives byte for byte.
    const std::wstring ascii = L"PeriphHub 09 -_/:.~";
    const std::string ascii_utf8 = "PeriphHub 09 -_/:.~";
    Expect(ToUtf8(ascii) == ascii_utf8, "ASCII wide -> utf8");
    Expect(ToUtf8(ascii).size() == ascii.size(), "ASCII utf8 length");
    Expect(ToWide(ascii_utf8) == ascii, "ASCII utf8 -> wide");
    Expect(ToWide(ascii_utf8).size() == ascii_utf8.size(), "ASCII wide length");

    // Chinese text "中文测试" is three UTF-8 bytes per character.
    const std::wstring chinese = L"\u4E2D\u6587\u6D4B\u8BD5";
    const std::string chinese_utf8("\xE4\xB8\xAD\xE6\x96\x87\xE6\xB5\x8B\xE8\xAF\x95", 12);
    Expect(ToUtf8(chinese) == chinese_utf8, "Chinese -> utf8 bytes");
    Expect(ToUtf8(chinese).size() == 12, "Chinese utf8 length");
    Expect(ToWide(chinese_utf8) == chinese, "Chinese utf8 -> wide");
    Expect(ToWide(chinese_utf8).size() == 4, "Chinese wide length");

    // U+1F600 GRINNING FACE: a surrogate pair in UTF-16, four bytes in UTF-8.
    const std::wstring emoji = L"\U0001F600";
    const std::string emoji_utf8("\xF0\x9F\x98\x80", 4);
    Expect(ToUtf8(emoji) == emoji_utf8, "emoji -> utf8 bytes");
    Expect(ToUtf8(emoji).size() == 4, "emoji utf8 length");
    Expect(ToWide(emoji_utf8) == emoji, "emoji utf8 -> wide");
    Expect(ToWide(emoji_utf8).size() == 2, "emoji wide length (surrogate pair)");

    // Mixed ASCII / BMP / astral roundtrips both ways.
    const std::wstring mixed = L"a\u4E2D\U0001F600z";
    const std::string mixed_utf8("a\xE4\xB8\xAD\xF0\x9F\x98\x80z", 9);
    Expect(ToUtf8(mixed) == mixed_utf8, "mixed wide -> utf8 bytes");
    Expect(ToUtf8(mixed).size() == 9, "mixed utf8 length");
    Expect(ToWide(ToUtf8(mixed)) == mixed, "mixed wide -> utf8 -> wide");
    Expect(ToUtf8(ToWide(mixed_utf8)) == mixed_utf8, "mixed utf8 -> wide -> utf8");

    // Embedded NULs stay in the middle of the string: lengths are explicit and
    // the terminator is never part of the result.
    const std::wstring wide_nul(L"a\0b", 3);
    const std::string utf8_nul = ToUtf8(wide_nul);
    Expect(utf8_nul.size() == 3, "embedded NUL wide length");
    Expect(utf8_nul == std::string("a\0b", 3), "embedded NUL wide bytes");
    Expect(ToWide(utf8_nul) == wide_nul, "embedded NUL wide roundtrip");
    Expect(ToWide(utf8_nul).size() == 3, "embedded NUL wide roundtrip length");

    const std::string bytes_nul("x\0y", 3);
    const std::wstring wide_back = ToWide(bytes_nul);
    Expect(wide_back.size() == 3, "embedded NUL utf8 length");
    Expect(wide_back[1] == L'\0', "embedded NUL utf8 byte kept");
    Expect(ToUtf8(wide_back) == bytes_nul, "embedded NUL utf8 roundtrip");

    // No trailing NUL is appended in either direction.
    Expect(ToUtf8(L"A").size() == 1 && ToUtf8(L"A")[0] == 'A', "utf8 has no trailing NUL");
    Expect(ToWide("A").size() == 1 && ToWide("A")[0] == L'A', "wide has no trailing NUL");

    if (g_failures == 0) {
        std::printf("text_test: OK (empty, ascii, chinese, emoji, mixed, embedded NUL)\n");
        return 0;
    }
    std::printf("text_test: %d failure(s)\n", g_failures);
    return 1;
}
