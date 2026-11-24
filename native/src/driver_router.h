#pragma once
// Driver-specific key parsing helpers. Pure C++17, free of Win32 dependencies.
#include <cstdint>
#include <string>

namespace driver_router {

// Extracts the 16-bit vendor ID from a `ksnake:XXXX:YYYY` key, returning false
// when the key does not carry a valid hex VID.
bool ParseYjxVidFromKey(const std::string& device_key, std::uint16_t* vid_out);

// Extracts the 16-bit product ID from a `royuan:3151:XXXX` key, returning false
// when the key does not carry a valid hex PID under VID 0x3151.
bool ParseRoyuanPidFromKey(const std::string& device_key, std::uint16_t* pid_out);

}  // namespace driver_router
