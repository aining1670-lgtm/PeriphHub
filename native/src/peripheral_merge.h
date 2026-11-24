#pragma once
// Pure JSON parsing, kind normalization, and multi-source device merging.
// Kept free of Win32 dependencies so it can be unit-tested standalone.
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

#include "bridge.h"

namespace peripherals {

constexpr std::size_t kRoyuanPacketLen = 65;

// Returns true when `pid` under VID 0x3151 is a ROYUAN 2.4 GHz wireless dongle
// rather than the USB-wired keyboard itself.
bool IsRoyuanDonglePid(std::uint16_t pid);

// Builds a 65-byte ROYUAN GET_REV (0x80) feature packet.
void BuildRoyuanGetRevPacket(std::uint8_t out[kRoyuanPacketLen]);

// Parses a 65-byte ROYUAN 0x80 reply; returns true when opcode echoes 0x80 and
// version bytes are non-zero (e.g. "v1.2").
bool ParseRoyuanFirmwareReply(const std::uint8_t reply[kRoyuanPacketLen],
                              std::string* version_out);

// Builds a 65-byte ROYUAN 2.4G dongle status query (0xF7).
void BuildRoyuanDongleStatusPacket(std::uint8_t out[kRoyuanPacketLen]);

// Parses a 65-byte ROYUAN 0xF7 reply; returns false if no keyboard is paired.
bool ParseRoyuanDongleStatus(const std::uint8_t reply[kRoyuanPacketLen],
                             bool* online,
                             int* battery);

// Builds a 65-byte ROYUAN GET_DEVID (0x8F) feature packet and parses reply.
void BuildRoyuanGetDevIdPacket(std::uint8_t out[kRoyuanPacketLen]);
bool ParseRoyuanDevIdReply(const std::uint8_t reply[kRoyuanPacketLen],
                           std::uint16_t* dev_id_out);
bool IsAkosBc98Board(std::uint16_t dev_id);
std::string ResolveRoyuanModelName(std::uint16_t dev_id, const std::string& raw_name);

// Bluetooth recognition helpers for AKOS BC98 / ROYUAN BT keyboards.
bool IsRoyuanBtHint(const std::string& inst_or_id, const std::string& name);
std::string NormalizeRoyuanBtName(const std::string& name);

constexpr std::size_t kKsnakePacketLen = 65;
constexpr std::uint16_t kKsnakeVidUsb = 0xA8A4;
constexpr std::uint16_t kKsnakeVidDongle = 0xA8A5;
constexpr std::uint16_t kKsnakePidX11 = 0x2255;

// Returns true when (vid, pid) matches K-snake X11 (USB 0xA8A4 or 2.4G 0xA8A5, PID 0x2255).
bool IsKsnakeX11Device(std::uint16_t vid, std::uint16_t pid);

// Returns true when `vid` is the K-snake X11 2.4 GHz receiver (0xA8A5).
bool IsKsnakeDongleVid(std::uint16_t vid);

// Builds a 65-byte K-snake X11 battery query output report (Report ID 0x00 + 0x55 0x30 ...).
void BuildKsnakeBatteryPacket(std::uint8_t out[kKsnakePacketLen]);

// Parses a 65-byte K-snake X11 input report (Report ID 0x00 + 64B payload) for battery and
// charging status. Accepts both direct 0xAA 0x30 replies and unsolicited 0xAA 0xFA (0xD0)
// battery pushes. Maps charging to PeriphHub convention: 2 = charging, 0 = discharging.
bool ParseKsnakeBatteryReply(const std::uint8_t reply[kKsnakePacketLen],
                             int* battery,
                             int* charging);

// Builds a 65-byte K-snake X11 online status query output report (Report ID 0x00 + 0x55 0xED ...).
void BuildKsnakeOnlinePacket(std::uint8_t out[kKsnakePacketLen]);

// Parses a 65-byte K-snake X11 0xAA 0xED reply; sets *online = (payload[8] == 2).
bool ParseKsnakeOnlineReply(const std::uint8_t reply[kKsnakePacketLen], bool* online);

// Normalizes raw kind strings ("mouse", "keyboard", "headset", ...) and fixes
// vendor misclassifications from the device name (e.g. "Gaming Keyboard" -> "keyboard").
std::string NormalizeKind(const std::string& raw_kind, const std::string& name);

// Maps a transport/via label ("Bluetooth", "2.4 GHz receiver", "USB", ...) to
// a normalized bridge mode ("bluetooth", "wired", "wireless").
std::string ModeFromVia(const std::string& via);

// Parses the JSON array printed by `<probe> --json`.
bool ParseUpstreamJson(const std::string& json_text,
                       std::vector<bridge::DeviceSnapshot>* out,
                       std::string* error);

// Parses the JSON object printed by `headsetcontrol -o json`.
bool ParseHeadsetControlJson(const std::string& json_text,
                             std::vector<bridge::DeviceSnapshot>* out,
                             std::string* error);

// Merges and deduplicates snapshots across all discovery layers, preserving
// native vendor / full-driver data and sorting full -> connected -> offline.
std::vector<bridge::DeviceSnapshot> MergeDevices(
    const std::vector<bridge::DeviceSnapshot>& native_incott,
    const std::vector<bridge::DeviceSnapshot>& native_vendor,
    const std::vector<bridge::DeviceSnapshot>& upstream_devices,
    const std::vector<bridge::DeviceSnapshot>& native_bt,
    const std::vector<bridge::DeviceSnapshot>& headsetcontrol_devices,
    const std::vector<bridge::DeviceSnapshot>& native_hid,
    const std::vector<bridge::DeviceSnapshot>& native_xinput);

}  // namespace peripherals
