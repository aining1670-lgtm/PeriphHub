#include "driver_router.h"

#include <cctype>
#include <cstdlib>
#include <string>

namespace driver_router {
namespace {

std::string ToLowerAscii(const std::string& text) {
    std::string out = text;
    for (char& ch : out) {
        ch = static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));
    }
    return out;
}

bool ParseHex4(const std::string& text, std::uint16_t* out) {
    if (text.size() != 4) return false;
    for (char ch : text) {
        if (!std::isxdigit(static_cast<unsigned char>(ch))) return false;
    }
    char* end = nullptr;
    const unsigned long val = std::strtoul(text.c_str(), &end, 16);
    if (end == text.c_str() || *end != '\0' || val > 0xFFFFul) return false;
    if (out != nullptr) *out = static_cast<std::uint16_t>(val);
    return true;
}

}  // namespace

bool ParseYjxVidFromKey(const std::string& device_key, std::uint16_t* vid_out) {
    const std::string lower = ToLowerAscii(device_key);
    if (lower.rfind("ksnake:", 0) != 0) return false;
    const std::size_t vid_start = 7;
    const std::size_t colon = lower.find(':', vid_start);
    const std::string vid_hex = (colon == std::string::npos)
                                    ? lower.substr(vid_start)
                                    : lower.substr(vid_start, colon - vid_start);
    return ParseHex4(vid_hex, vid_out);
}

bool ParseRoyuanPidFromKey(const std::string& device_key, std::uint16_t* pid_out) {
    const std::string lower = ToLowerAscii(device_key);
    if (lower.rfind("royuan:3151:", 0) != 0) return false;
    const std::string pid_hex = lower.substr(12);
    return ParseHex4(pid_hex, pid_out);
}

}  // namespace driver_router
