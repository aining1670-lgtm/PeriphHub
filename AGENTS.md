# AGENTS.md

Instructions for AI coding agents working in the **PeriphHub** repository.

## Project Overview

PeriphHub is a Windows system-tray / taskbar peripheral battery monitor and gaming mouse/keyboard driver utility built with a **"native C++17 host + offline WebView2 UI"** architecture:

- **Native host (`native/src/`)**: C++17 Win32 application (MinGW-w64) handling single-instance mutex, tray icon with dynamic battery badge, translucent taskbar battery strip, background polling, multi-source peripheral discovery (Incott / YJX / ROYUAN / Bluetooth / HID Battery / XInput / external `battery-probe`), and all HID communication.
- **Web UI (`web-next/` → `web/index.html`)**: React 19 + TypeScript + Tailwind CSS v4 + Zustand source under `web-next/`, bundled via Vite + `vite-plugin-singlefile` into the single-file offline `web/index.html` hosted in WebView2 via `chrome.webview.postMessage` JSON IPC. No local HTTP server or TCP port is ever opened.
- **Packaging**: Web assets and `WebView2Loader.dll` are embedded as `RCDATA` resources via `native/src/resource.rc`; `make dist` produces a standalone `dist/PeriphHub/PeriphHub.exe`.

## Architecture & Repository Map

Maintain strict separation across four layers:

| Layer | Location | Responsibility |
|---|---|---|
| Protocol pure functions | `native/src/protocol.*`, `yjx_protocol.*`, `royuan_protocol.*`, `command_json.*`, `json.*`, `macro_json.*`, `peripheral_merge.*` | Zero Win32 dependencies; hardware-free unit-testable; single source of truth for wire bytes and JSON schemas |
| Device IO | `native/src/hid.*`, `yjx_hid.*`, `royuan_hid.*`, `peripherals.*` | HID/Bluetooth/XInput enumeration, Feature/Output reports, retries, timeouts, and locking |
| Driver registry & routing | `native/src/drivers/` (`registry.*`, `builtin_caps.*`, `builtin.cpp`, `yjx_driver.*`, `royuan_driver.*`), `driver_router.*` | Routes `deviceKey` to backend drivers and declares device `capabilities` |
| Bridge & UI | `native/src/main.cpp`, `bridge.*`, `bridge_args.*`, `webview.*`, `widget.*`, `brands.*`, `web-next/src/`, `web/index.html` | JSON IPC (`{"id":1,"cmd":"...","args":{...}}` ↔ `{"id":1,"ok":true,"data":{...}}`); UI renders strictly from bridge `driver` and `capabilities` and never manipulates HID handles or raw bytes |

Other directories:

- `tests/`: 9 hardware-free C++ unit test suites wired into `make test`.
- `tools/`: `gen_golden.js` (generates `native/src/keytable.inc`), `check_bridge_json.js` (`--bridge` JSON validity & exit-code contract test).
- `examples/battery-probe/`: Minimal external battery probe example (`make probe-example`).
- `docs/`: Architecture, bridge protocol, firmware constraints, external probe spec, and new-device guide.

## Build, Test & Verification

Environment: **MSYS2 / MinGW-w64** (`g++` C++17, static runtime).

```bash
# Frontend (when editing web-next/): (cd web-next && npm run build && cp dist/index.html ../web/index.html)
export PATH="/c/msys64/usr/bin:/c/msys64/mingw64/bin:$PATH"

# Kill any running instance before relinking
MSYS2_ARG_CONV_EXCL='*' taskkill /IM PeriphHub.exe /F

# Build (must compile with 0 warnings)
make

# Run all 9 hardware-free C++ unit test suites
make test

# Build standalone single-file executable (dist/PeriphHub/PeriphHub.exe)
make dist

# Build external probe example
make probe-example

# Run --bridge JSON & exit-code contract check
node tools/check_bridge_json.js build/PeriphHub.exe
```

Hardware & WebView2 verification commands (require a desktop session and awake device):

```powershell
.\PeriphHub.exe --list
.\PeriphHub.exe --list-json
.\PeriphHub.exe --probe
.\PeriphHub.exe --status
.\PeriphHub.exe --info
.\PeriphHub.exe --keys
.\PeriphHub.exe --bridge '{"id":1,"cmd":"device.probe"}'
.\PeriphHub.exe --webview-selftest
.\PeriphHub.exe --webview-selftest-write
.\PeriphHub.exe --webview-selftest-f26
.\PeriphHub.exe --settings
```

## Engineering Rules

1. **Reject out-of-range inputs — never clamp or truncate.** Enforce limits strictly and return `ok:false`: DPI `50–32000` (step `50`), polling rate `125–8000`, macro `≤ 71` items / cycle `1–65535` / delay `0–65535` / name `≤ 32`, rapid-fire loops `0–3` / interval `0–255`, sleep `10–900`, debounce `0–30`.
2. **Verify every write with readback; never fabricate defaults.** If a value cannot be read back, return `null` or an explicit error (or `unverified` where firmware cannot report it). Never substitute hardcoded defaults (e.g. 800 DPI, 1000 Hz, 8 ms, 60 s) as real hardware state.
3. **Check device liveness before writing.** Wireless mice enter shallow sleep after ~60–90 s; confirm awake state before issuing writes and report sleeping devices explicitly.
4. **All bridge JSON responses must be valid JSON.** Build responses via `command_json.*` / `bridge.*` helpers rather than ad-hoc string concatenation, ensure they pass `json::Parse`, and keep `--bridge` exit codes consistent (`0` for `ok:true`, `1` for `ok:false`).
5. **Macro safety.** Incott G23 firmware cannot read back macro contents: treat loaded slots as dirty, re-upload before binding, and never write a key binding (`0x06`) if macro upload fails. Keep macro payloads within the 128 KB import / 256 KB IPC guards.
6. **Zero warnings & test coverage.** Code must build cleanly under `-Wall -Wextra` with static linking (`-static -static-libgcc -static-libstdc++`). Any new protocol, parser, or routing logic must include hardware-free C++ unit tests registered in `Makefile`'s `test` target.
7. **Do not patch `Makefile` for local environment workarounds.** Pass temporary overrides on the command line (see below).

## Known Environment & Firmware Pitfalls

- **MSYS2 `make` strips `TMP`/`TEMP`**: `g++` may fail writing temp files to `C:\WINDOWS\`. Work around on the command line without editing `Makefile`:
  ```bash
  make all CXX='TMP="C:\\Temp" g++'
  make test CXX='TMP="C:\\Temp" g++'
  ```
- **Low-IL workspace ACL on Windows**: Launching `PeriphHub.exe` directly from a sandboxed Low-IL directory can cause `Shell_NotifyIconW` `ERROR_ACCESS_DENIED` or `0xC0000142`. Copy `build/` or `dist/PeriphHub/PeriphHub.exe` to a normal folder outside the sandbox for GUI/tray testing.
- **Incott G23 firmware quirks** (full details in [`docs/05-firmware-constraints.md`](docs/05-firmware-constraints.md)):
  - Active DPI stage is read from `0x83` — do **not** read `0x89[3]` (charging flag) or `0x89[7]` (sleep threshold).
  - First readback after a write frequently returns `09 00 ...` or zeros; keep 3× retry with delay.
  - `0x82` cannot read back X-axis DPI on G23; acceptance is Y readback verified + X marked `unverified`.
  - `0x06` key write does not echo; verify via `0x86` readback.
- **CLI flags (`--help` vs unknown flags)**: `--help` / `-h` / `/?` print CLI usage to stdout and exit `0`; unrecognized `-` or `/` flags exit with code `3`.
- **Binary inspection & UI checks**: Do not use `grep -a` to search UTF-16 wide strings in `.exe` binaries; use `--webview-selftest*` for headless UI verification rather than manual tray clicking.

## Documentation Index

Read the relevant reference doc before modifying a subsystem:

- Architecture: [`docs/01-architecture.md`](docs/01-architecture.md)
- Web console architecture (`web-next/`): [`docs/11-web-console-architecture.md`](docs/11-web-console-architecture.md)
- Bridge JSON protocol & commands: [`docs/02-bridge-protocol.md`](docs/02-bridge-protocol.md)
- Hardware & firmware constraints: [`docs/05-firmware-constraints.md`](docs/05-firmware-constraints.md)
- External battery probe protocol: [`docs/09-external-probe.md`](docs/09-external-probe.md)
- Adding a new device driver: [`docs/10-adding-a-device.md`](docs/10-adding-a-device.md) (and workspace skill [`.agents/skills/add-device-driver/SKILL.md`](.agents/skills/add-device-driver/SKILL.md))
- Contribution checklist: [`CONTRIBUTING.md`](CONTRIBUTING.md)

## Workflow & Reporting

- Verify changes with `make`, `make test`, and (when touching bridge/CLI/UI) `node tools/check_bridge_json.js` or `--webview-selftest`.
- Report concrete evidence: exact commands run, test results, and file/line references. If hardware or GUI tests were not run, state that explicitly.
- Use Conventional Commits (`feat(driver): ...`, `fix(native): ...`, `refactor(web): ...`, `docs: ...`).
