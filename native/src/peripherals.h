#pragma once
// Multi-peripheral discovery (Bluetooth PnP, Generic HID Battery, XInput,
// HeadsetControl, and optional upstream PeriphHub --json adapter).
#include <functional>
#include <string>
#include <unordered_set>
#include <vector>

#include "bridge.h"

namespace peripherals {

// Scans ROYUAN/Ajazz/Epomaker keyboards (VID 0x3151, UsagePage 0xFFFF Usage 0x0002):
// reads firmware info over USB wired or live battery via 0xF7 on 2.4G dongles.
std::vector<bridge::DeviceSnapshot> ScanRoyuanKeyboards();

// Scans K-snake X11 mice (VID 0xA8A4 USB / 0xA8A5 2.4G, PID 0x2255,
// UsagePage 0xFF01 Usage 0x0010): reads live battery and charging status.
std::vector<bridge::DeviceSnapshot> ScanKsnakeMice();

// Scans Windows Bluetooth PnP properties for connected devices with battery levels.
// Populates `bt_containers` (when non-null) with container GUID strings so generic
// HID battery scanning skips devices already reported via Bluetooth.
std::vector<bridge::DeviceSnapshot> ScanBluetoothDevices(
    std::unordered_set<std::string>* bt_containers = nullptr);

// Scans HID devices exposing standard Battery Strength usages (0x06:0x20 / 0x85:0x65),
// skipping Incott (093A:522C/622C), digitizers (0x0D), UPS (0x84/0x85), and `skip_containers`.
std::vector<bridge::DeviceSnapshot> ScanGenericHidDevices(
    const std::unordered_set<std::string>& skip_containers);

// Scans XInput gamepads (0..3) dynamically via xinput1_4.dll / xinput1_3.dll / xinput9_1_0.dll.
std::vector<bridge::DeviceSnapshot> ScanXInputDevices();

// Runs upstream `PeriphHub.exe --json` / `battery-probe.exe --json` if present
// and not disabled (`PERIPHHUB_PROBE_EXE=0/none/off`). Returns true when executed
// and parsed cleanly.
bool RunUpstreamProbe(std::vector<bridge::DeviceSnapshot>* out);

// Runs `headsetcontrol.exe -o json` if present beside the binary.
bool RunHeadsetControl(std::vector<bridge::DeviceSnapshot>* out);

// Unified multi-peripheral scan: runs upstream probe FIRST (before Incott HID),
// then invokes `probe_incott`, then native Bluetooth / Generic HID / XInput (and
// HeadsetControl fallback when upstream probe did not succeed), and merges/dedups.
std::vector<bridge::DeviceSnapshot> ScanAllDevices(
    const std::function<bool(bridge::DeviceSnapshot*)>& probe_incott);

}  // namespace peripherals
