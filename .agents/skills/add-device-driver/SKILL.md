---
name: add-device-driver
description: >-
  Guided end-to-end workflow for adding a new mouse, keyboard, or peripheral
  driver (or external battery probe) to PeriphHub. Use when the user asks to
  add device support, adapt a new mouse/keyboard/peripheral, implement a new HID
  vendor driver, or add a keyboard layout (适配新设备、接入新驱动、添加鼠标/键盘支持).
---

# Add Device Driver to PeriphHub

Follow this runbook whenever integrating a new peripheral into PeriphHub. Keep
strict layer separation (protocol pure functions → HID IO → driver registry →
bridge/UI) and rely on existing reference docs rather than duplicating schemas.

## Step 0: Triage Integration Mode

Choose the lightest integration that meets the user's goal:

1. **Battery level only** (no DPI / keymap / macro / lighting control)
   - Do **not** add a built-in native driver unless the protocol must live in-process.
   - Implement an external `battery-probe` executable following
     [docs/09-external-probe.md](../../../docs/09-external-probe.md) and
     [examples/battery-probe/battery_probe.cpp](../../../examples/battery-probe/battery_probe.cpp).
2. **Full mouse driver**
   - Use the **YJX** driver as the structural template:
     `native/src/yjx_protocol.{h,cpp}`, `native/src/yjx_hid.{h,cpp}`,
     `native/src/drivers/yjx_driver.{h,cpp}`.
3. **Full keyboard driver**
   - Use the **ROYUAN** driver as the structural template:
     `native/src/royuan_protocol.{h,cpp}`, `native/src/royuan_hid.{h,cpp}`,
     `native/src/drivers/royuan_driver.{h,cpp}`.

## Step 1: Collect & Confirm Protocol Inputs Before Writing Code

Do **not** guess vendor HID protocols from a device name or VID/PID alone.
First establish the protocol source from the user (ask if none was provided):

- **Official WebHID web driver (preferred)**: URL or saved frontend JS bundle.
  Extract VID/PID, `usagePage`/`usage`, report IDs, packet builders/parsers,
  checksums, parameter ranges, LED mode tables, and key/slot maps directly from
  the JS source before writing any C++.
- **Existing open-source spec / driver**: e.g., QMK/VIA, OpenRGB, libratbag, or
  community reverse-engineering notes.
- **Official desktop app + USB capture**: Wireshark + USBPcap traces (`.pcapng`
  or exported hex dumps) covering read + write operations for each feature to
  be supported.

Before implementing wire bytes, confirm every required protocol fact (if any
critical detail is still unverified, stop and ask the user):

- **USB / 2.4G identity**: VID, PID(s), HID `UsagePage` / `Usage`, report IDs
  and packet length, Feature vs Output/Input report transport.
- **Namespace**: Unique lowercase `deviceKey` prefix (e.g., `<vendor>:VID:PID`),
  disjoint from `incott:`, `ksnake:`, and `royuan:3151:`.
- **Wire format**: Command bytes, checksum formula, response layout, and whether
  writes use Read-Modify-Write (RMW) frames with reserved bytes that must be
  preserved verbatim.
- **Capabilities & limits**: Exact supported ranges/presets (DPI, polling rates,
  debounce, sleep, LED modes, keys, macros).
- **Keyboards only**: Built-in layout ID (`bc98`, `ansi-87`, `ansi-104`,
  `ansi-108`) or custom `keyboard.keys` coordinate table, plus `fnSlot` if any.

## Step 2: Read Authoritative References

Read these before writing code so schemas and bridge contracts stay exact:

- [docs/10-adding-a-device.md](../../../docs/10-adding-a-device.md) — `capabilities` JSON schema, command families, and rejection messages.
- [docs/02-bridge-protocol.md](../../../docs/02-bridge-protocol.md) — exact JSON request/response structures for every command.
- [native/src/drivers/host.h](../../../native/src/drivers/host.h) — host services (avoid coupling directly to `main.cpp` globals).
- The closest reference driver (`yjx_*` for mice, `royuan_*` for keyboards).

## Step 3: Implement in Strict Layer Order

Do **not** modify `main.cpp`'s `BridgeHandler` dispatch or add hardcoded
`deviceKey` prefix checks in `web-next/src/` (bundled into `web/index.html`).

1. **Protocol pure functions** — `native/src/<vendor>_protocol.{h,cpp}`
   - Zero Win32 headers (`<windows.h>` forbidden here) so tests run hardware-free.
   - Packet encoding/decoding, checksum calculation/verification, and strict range checks.
2. **Protocol unit tests** — `tests/<vendor>_protocol_test.cpp`
   - Cover packet construction, response parsing, RMW reserved-byte preservation,
     and out-of-range / malformed-packet rejection.
   - Add the new test build & run lines to the `test:` target in `Makefile`.
   - Note: `Makefile` already globs `native/src/*.cpp` and `native/src/drivers/*.cpp`,
     so new driver `.cpp` files are compiled into `PeriphHub.exe` automatically.
3. **HID IO layer** — `native/src/<vendor>_hid.{h,cpp}`
   - Device enumeration, Feature/Output report IO, retry logic for transient
     post-write readbacks, and write-then-readback verification.
   - In RMW frames, copy all untouched/reserved bytes and unknown bits verbatim.
4. **Device discovery** — `native/src/peripherals.{h,cpp}`
   - Add `Scan<Vendor>Devices()`, call it from `ScanAllDevices()`, and pass
     results into `MergeDevices()`.
5. **Capabilities & descriptor** — `native/src/drivers/builtin_caps.{h,cpp}`
   - Implement `Build<Vendor>Capabilities()` and `<Vendor>Descriptor()`.
   - Keep `builtin_caps.*` free of Win32 dependencies so `driver_registry_test`
     stays hardware-free.
   - Set unsupported mouse sections to `null` / `[]` / `false`; only keyboards
     populate `capabilities.keyboard`.
6. **Driver bridge handler** — `native/src/drivers/<vendor>_driver.{h,cpp}`
   - Implement `Handle(const bridge::Request&, const std::string& device_key)`.
   - Parse arguments strictly with `bridge::OptIntArg` / `bridge::OptBoolArg`
     and build responses via `bridge::Success` / `bridge::Failure` or `command_json.*`.
7. **Register & test routing** — `native/src/drivers/builtin.cpp` & `tests/driver_registry_test.cpp`
   - Register `<Vendor>Descriptor()` + handler in `RegisterBuiltinDrivers()`.
   - Add routing, capability JSON validity, and unsupported-command rejection
     assertions to `tests/driver_registry_test.cpp`.

## Step 4: Non-Negotiable Safety Checks

Before running verification, confirm every item:

- [ ] **No silent clamping**: Out-of-range or wrong-typed inputs return `ok: false`.
- [ ] **Readback verification**: Every write waits for firmware commit and reads
      back real state; never echo request arguments or fabricate defaults on failure.
- [ ] **Liveness check**: Wireless devices check awake state before writes and
      report sleeping devices explicitly.
- [ ] **RMW preservation**: Untouched bytes/bits in whole-frame writes are preserved.
- [ ] **Zero UI hardcoding**: `web-next/src/` (`web/index.html`) renders purely from `capabilities`
      and `driver`; no new `deviceKey` prefix branches.

## Step 5: Verify & Hand Off

Run all automated checks (in MSYS2/MinGW-w64; if `make` fails writing temp files,
pass `CXX='TMP="C:\\Temp" g++'` on the command line without editing `Makefile`):

```bash
export PATH="/c/msys64/usr/bin:/c/msys64/mingw64/bin:$PATH"
make clean && make
make test
node tools/check_bridge_json.js build/PeriphHub.exe
```

Verify:
- `make` finishes with **0 warnings** under `-Wall -Wextra`.
- `make test` runs and passes the new `<vendor>_protocol_test` and `driver_registry_test`.
- `check_bridge_json.js` passes all JSON & exit-code checks.

Finally, report the exact commands/results and give the user a short hardware
verification checklist to run with the physical device awake:

```powershell
.\PeriphHub.exe --list-json
.\PeriphHub.exe --bridge '{"id":1,"cmd":"device.info","args":{"deviceKey":"<vendor>:..."}}'
# Plus at least one write command followed by its corresponding readback command
```
