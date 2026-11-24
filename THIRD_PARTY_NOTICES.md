# Third-Party Notices

PeriphHub incorporates or references the following third-party components and assets.

## 1. Wireless-Device-Battery-Monitor
- **Source**: https://github.com/krisgfx9/Wireless-Device-Battery-Monitor
- **Copyright**: Copyright (C) krisgfx9 and contributors
- **License**: GNU General Public License v3.0 (GPL-3.0)
- **Status**: The upstream repository and its author account are unreachable as of 2026-11-24 (GitHub returns 404). This notice is kept to preserve the attribution required by GPL-3.0.
- **Usage**: The taskbar battery strip (window/rendering portion of `native/src/widget.cpp`) and the brand icon table (`native/src/brands.cpp`) are derived from this project and remain licensed under GPL-3.0. Everything else in this repository — HID protocol layers, device drivers (Incott / YJX / ROYUAN / generic HID / Bluetooth / XInput / external probe), the macro system and the WebView2 UI — is independent work by the PeriphHub contributors.

## 2. Microsoft Edge WebView2 SDK
- **Location**: `third_party/webview2/` (`include/WebView2.h`, `x64/WebView2Loader.dll`, `LICENSE.txt`)
- **Copyright**: Copyright (C) Microsoft Corporation. All rights reserved.
- **License**: BSD 3-Clause License — see [`third_party/webview2/LICENSE.txt`](third_party/webview2/LICENSE.txt).

## 3. Tailwind CSS (Play CDN Runtime)
- **Location**: `web/tailwind.js`
- **Copyright**: Copyright (c) Tailwind Labs, Inc.
- **License**: MIT License
- **Usage**: Bundled offline to style `web/index.html` without external CDN network requests.
