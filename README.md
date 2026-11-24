# PeriphHub

> **这是个人自用项目，不是通用驱动中心。** 驱动只照着作者手上的几台设备逆向适配，目前只在 **Incott G23、Ksnake X11、ROYUAN AKOS BC98** 上实测通过；同品牌其他型号基本没验证过，厂商私有协议随固件变化随时可能不兼容。想接自己的设备，仓库留了驱动骨架、接入文档和 AI Skill，见 [接入你自己的设备](#接入你自己的设备)。

**Windows 外设电量监控与驱动控制中心** —— 托盘电量图标、任务栏半透明电量条、离线配置界面，一个 `PeriphHub.exe` 全部搞定。

[![Platform](https://img.shields.io/badge/platform-Windows%2010%20%7C%2011%20x64-0078D6?logo=windows&logoColor=white)](#系统要求)
[![C++17](https://img.shields.io/badge/C%2B%2B-17-00599C?logo=cplusplus&logoColor=white)](#从源码构建)
[![License](https://img.shields.io/badge/license-GPL--3.0-blue.svg)](LICENSE)

它的出发点很实际：每家外设都配一套厂商软件，电量还得挨个打开看。具体来说：

- **驱动统一管理**：鼠标键盘的驱动都归 PeriphHub 管，装上就能读电量、调 DPI、改键、写宏、调灯效，桌面上少几个常驻的官方后台。
- **电量集中看**：自家驱动、蓝牙 / BLE、HID Battery、XInput、`headsetcontrol` 和自定义探针报的电量，去重后合成一个列表。
- **不联网**：不监听本地端口，也不发网络请求；配置和宏只写在本机。
- **拷贝即用**：网页资源和 `WebView2Loader.dll` 都编进了 exe，拿走一个文件就能跑。

> **关于本项目**：PeriphHub 是非官方、独立的开源项目（GPL-3.0）。任务栏电量条的绘制与品牌图标表（`native/src/widget.cpp`、`native/src/brands.cpp`）派生自 [krisgfx9/Wireless-Device-Battery-Monitor](https://github.com/krisgfx9/Wireless-Device-Battery-Monitor)（GPL-3.0，原仓库已于 2026 年下线，详见 [`THIRD_PARTY_NOTICES.md`](THIRD_PARTY_NOTICES.md)）；其余外设驱动、协议层、宏系统与 WebView2 界面均为本项目独立实现。本项目与 Incott、AKOS、ROYUAN 等厂商及上述原作者无隶属或背书关系。

<!-- 截图建议：主界面 + 任务栏电量条，放 docs/assets/screenshot.png 后启用
![PeriphHub 主界面](docs/assets/screenshot.png)
-->

## 核心特性

| 能力 | 说明 |
|---|---|
| **托盘与任务栏电量条** | 动态托盘电量图标 + 任务栏半透明多设备电量条；支持主/副显示器、堆叠布局、低电量提醒与自定义别名。 |
| **多协议外设驱动** | 直接读写鼠标 DPI（含分轴）、回报率、传感器参数、防抖/休眠、按键映射、板载宏与 RGB 灯效，写后回读校验，浅休眠自动识别。 |
| **多来源电量聚合** | 原生驱动、Windows 蓝牙 / BLE（`0x180F`）、通用 HID Battery、XInput 手柄、[`headsetcontrol`](https://github.com/Sapd/HeadsetControl) 与外部探针去重合并。 |
| **离线 WebView2 界面** | `RCDATA` 内嵌的单文件 UI，通过 `chrome.webview.postMessage` JSON IPC 与宿主通信，不经本地端口。 |
| **可脚本化的桥协议** | `--bridge '<json>'` 单次请求 / 单次响应，`ok:true` 退出码 `0`、`ok:false` 退出码 `1`，便于 CI 与自动化。 |

## 系统要求

- **操作系统**：Windows 10 / Windows 11（x64）
- **运行时**：Microsoft Edge WebView2 Runtime（Windows 11 自带，Windows 10 多数已预装；缺失时程序会提示安装）
- **构建环境（仅源码编译）**：MSYS2 / MinGW-w64（`g++` C++17、`windres`、`make`）与 Node.js（运行契约检查脚本）

## 快速开始

### 开箱即用

1. 从 [Releases](https://github.com/aining1670-lgtm/PeriphHub/releases) 下载 `PeriphHub.exe`（或按下方[从源码构建](#从源码构建)自行编译），放到任意目录。
2. 双击运行：程序最小化驻留系统托盘，并在任务栏显示外设电量条。
3. 双击托盘图标打开配置窗口；鼠标移上托盘图标可看当前电量，右键是常用操作菜单。

> 首次运行若出现 SmartScreen 提示，属未签名单文件程序的正常现象，可选择"仍要运行"或自行编译。

### 平时怎么用

- **托盘图标**：鼠标移上去看当前电量，双击打开设置，右键出菜单（设备列表、立即刷新、任务栏电量条开关、退出）。
- **任务栏电量条**：每台设备一个图标加电量，点一下也进设置；嫌占地方可以在设置里关掉。
- **命令行**：`PeriphHub.exe --settings` 直接开设置窗口；如果程序已经在后台跑，会唤醒原来的窗口，不会再开一个。

### 设置里能改什么

- 开机静默自启（写 `HKCU\Software\Microsoft\Windows\CurrentVersion\Run`）、低电量弹窗提醒、每台设备的别名。
- 任务栏电量条的外观：显隐、图标与电量上下堆叠、圆角背景底板、按品牌配色发光条。
- 每台设备的 DPI、回报率、键位、宏、灯效在各自的设置页里调；所有开关即时生效，不用重启。

常用命令：

```powershell
.\PeriphHub.exe                # 启动托盘 + 任务栏电量条
.\PeriphHub.exe --settings     # 启动并打开设置窗口
.\PeriphHub.exe --list         # 列出已识别外设及电量
.\PeriphHub.exe --list-json    # 以 JSON 输出外设状态
.\PeriphHub.exe --help         # 查看内置帮助
```

完整的命令行参考见[下文](#命令行参考)。

## 支持设备与功能

| 设备 / 来源 | 识别前缀 / 接口 | 支持能力 | 作者实测 |
|---|---|---|---|
| **Incott 鼠标**（G23 / G24） | `incott:522C` / `622C`（VID `0x093A`） | 电量、DPI 分轴、回报率、传感器、6 键映射、320B 宏（兼容 Studio JSON）、灯效 | G23 ✅ 日常使用；G24 仅协议兼容，未实测 |
| **Ksnake 鼠标**（X11） | `ksnake:A8A5:2255` / `A8A4:*` | 电量、充电状态、DPI、回报率、按键映射、板载宏、灯效 | ✅ 日常使用；其他 YJX 型号未验证 |
| **ROYUAN 键盘**（AKOS BC98） | `royuan:3151:*`（87/98/104/108 及自定义配列） | 2.4G / 蓝牙电量、多配置档、全键改键（含 Fn 层）、板载宏、主/侧 RGB 灯效、防抖与休眠 | ✅ 日常使用；代码层只校验过 BC98，其他型号未验证 |
| **蓝牙 / BLE 外设** | Windows 蓝牙 PnP（`BTHENUM` / `BTHLE`） | 电量监控（`0x180F` Battery Service） | 通用系统来源，非驱动 |
| **通用 HID / XInput 手柄** | 标准 HID Battery / `xinput1_4.dll` | 电量监控 | 通用系统来源，非驱动 |
| **外部电量探针** | `battery-probe.exe --json` | 免编译接入任意私有协议的设备电量与在线状态 | 通用机制，附示例 |

说明：

- 表格之外的型号一律视为**未适配**：厂商私有协议随固件版本变化，能读出电量也不代表能改 DPI / 改键 / 写宏。
- 电量类来源（蓝牙 / HID / XInput）**零配置**，系统配对后即出现。
- 驱动类能力需要设备处于**唤醒**状态：无线鼠标约 60–90 秒无操作会进入浅休眠，动一下再刷新即可。
- 外部探针默认查找与 `PeriphHub.exe` 同目录的 `battery-probe.exe`，可用环境变量 `PERIPHHUB_PROBE_EXE` 指定路径（设为 `0` / `none` 可禁用）。

## 命令行参考

| 命令 | 作用 | 退出码 |
|---|---|---|
| `PeriphHub.exe` | 启动托盘图标与任务栏电量条 | `0` |
| `--settings` | 启动并打开设置窗口（已运行则唤起现有窗口） | `0` |
| `--list` / `--list-json` | 列出已识别外设（文本 / JSON） | `0` |
| `--bridge <json>` | 执行单次桥请求并打印响应 | `ok:true` → `0`，`ok:false` → `1` |
| `--help` / `-h` / `/?` | 打印帮助 | `0` |
| `--probe`、`--status`、`--info` | Incott 直连探测 / 状态 / 设备信息（低级调试） | 成功 `0`，失败 `1` |
| `--keys`、`--dpi <n>`、`--set-key <i> <key>` | 读取键表、设置 DPI、改单键（低级调试） | 成功 `0`，失败 `1` |
| `--macro-upload <i> <file>` / `--macro-apply <i> <file>` | 上传 / 上传并绑定宏 | 成功 `0`，失败 `1` |
| `--webview-selftest*` | WebView2 界面渲染 / 写入 / F26 自动化自检 | 成功 `0`，失败 `1` |

> 低级调试命令只针对 Incott 通道，日常使用与脚本集成请优先用 `--bridge`（协议见 [`docs/02-bridge-protocol.md`](docs/02-bridge-protocol.md)）。未知 `-` / `/` 开头参数以退出码 `3` 结束。

## 架构

```text
PeriphHub.exe
├─ 托盘图标 / 任务栏电量条        native  后台轮询 + Win32 自绘组件
├─ 设备设置窗口（WebView2）        web     本地离线 HTML，chrome.webview.postMessage IPC
├─ 驱动注册表与协议后端            native  drivers/registry 统一路由（Incott / YJX / ROYUAN）
└─ 多外设发现与外部探针            native  蓝牙 / HID / XInput / battery-probe --json
```

四层职责严格分离：协议纯函数（无 Win32，可单测）→ 设备 IO（HID / 蓝牙 / XInput）→ 驱动注册表与路由 → 桥与 UI。UI 只渲染桥返回的 `driver` 与 `capabilities`，**不接触任何 HID 句柄或原始字节**。细节见 [`docs/01-architecture.md`](docs/01-architecture.md)。

## 仓库结构

| 路径 | 作用 |
|---|---|
| `native/src/` | C++17 原生宿主：HID 与多品牌协议（Incott / YJX / ROYUAN）、外设扫描与合并（`peripherals.*`, `peripheral_merge.*`）、JSON 与响应构造（`json.*`, `command_json.*`, `bridge_args.*`）、宏库（`macro_json.*`, `macro_store.*`）、WebView2 宿主（`webview.*`）、任务栏电量条（`widget.*`, `brands.*`）与主入口（`main.cpp`） |
| `native/src/drivers/` | 驱动描述符注册表（`registry.*`）、内置能力声明（`builtin_caps.*`）、驱动后端（`yjx_driver.*`, `royuan_driver.*`）与统一注册入口（`builtin.cpp`） |
| `web/` | 离线单文件配置界面 `index.html` + 本地 `tailwind.js` |
| `examples/` | 外部电量探针最小示例（`examples/battery-probe/battery_probe.cpp`） |
| `tests/` | 9 组无硬件纯 C++ 单测：协议、编码、桥、宏、JSON 构造、外设合并、驱动注册表 |
| `tools/` | `gen_golden.js`（生成键表）、`check_bridge_json.js`（`--bridge` JSON 与退出码契约测试） |
| `docs/` | 架构 `01`、桥协议 `02`、固件约束 `05`、外部探针 `09`、新设备指南 `10` |

## 从源码构建

在 MSYS2 / MinGW-w64 环境下：

```bash
export PATH="/c/msys64/usr/bin:/c/msys64/mingw64/bin:$PATH"

# 链接前先结束正在运行的实例
MSYS2_ARG_CONV_EXCL='*' taskkill /IM PeriphHub.exe /F

make        # 编译 build/PeriphHub.exe（零警告、静态链接、内嵌 web 资源与 WebView2Loader.dll）
make test   # 运行 9 组纯 C++ 单元测试
make dist   # 产出单文件绿色版 dist/PeriphHub/PeriphHub.exe
```

验证与调试（Windows 桌面 PowerShell）：

```powershell
# 1. 只读检查
.\PeriphHub.exe --list
.\PeriphHub.exe --list-json
.\PeriphHub.exe --probe
.\PeriphHub.exe --status
.\PeriphHub.exe --info
.\PeriphHub.exe --keys

# 2. 打开设置窗口（或无参启动托盘与任务栏电量条）
.\PeriphHub.exe --settings

# 3. 桥命令与 JSON 契约检查（ok:true → 0，ok:false → 1）
.\PeriphHub.exe --bridge '{"id":1,"cmd":"device.probe"}'
node tools/check_bridge_json.js .\PeriphHub.exe

# 4. WebView2 自动化自检
.\PeriphHub.exe --webview-selftest
.\PeriphHub.exe --webview-selftest-write
.\PeriphHub.exe --webview-selftest-f26
```

常见构建问题：

- **`g++` 报无法写入临时文件**（MSYS2 下 `make` 会清掉 `TMP` / `TEMP`）：
  ```bash
  make all CXX='TMP="C:\\Temp" g++'
  make test CXX='TMP="C:\\Temp" g++'
  ```
- **托盘图标创建失败 / `0xC0000142`**：工作区目录带 Low-IL 沙箱权限时，把 `build/` 或 `dist/PeriphHub/PeriphHub.exe` 复制到沙箱外（如 `C:\Temp\bh_test`）再运行。

## 接入你自己的设备

驱动的扩展点是一张注册表，而不是散落在 `main.cpp` 里的分派分支：

- **骨架**：`native/src/drivers/registry.h` 的 `DriverDescriptor` 声明了 `key_prefixes`（匹配 `deviceKey`）、`commands`（该设备可用的命令）、`capabilities`（能力 JSON）与 `handle`（请求处理）。新驱动 = 协议纯函数 + HID 通道 + 一个 `Handle` 函数，然后在 `native/src/drivers/builtin.cpp` 的 `RegisterBuiltinDrivers()` 里加一行注册；Bridge 分发和 Web UI 都不用改。
- **结构样板**：鼠标照 `native/src/drivers/yjx_driver.*`（YJX），键盘照 `royuan_driver.*`（ROYUAN），配套 `<vendor>_protocol.*` 与 `<vendor>_hid.*` 分层。
- **完整清单**：文件清单、`capabilities` JSON Schema、Bridge 命令族与拒绝契约、固件交互原则、PR 检查清单见 [`docs/10-adding-a-device.md`](docs/10-adding-a-device.md)。
- **只想看电量（免编译）**：写一个输出 JSON 数组的 `battery-probe.exe --json`，见 [`docs/09-external-probe.md`](docs/09-external-probe.md) 与示例 [`examples/battery-probe/battery_probe.cpp`](examples/battery-probe/battery_probe.cpp)（`make probe-example`）。
- **让 AI 帮你接**：仓库内置工作区 Skill [`.agents/skills/add-device-driver/SKILL.md`](.agents/skills/add-device-driver/SKILL.md)，会先分诊（只电量 → 探针；鼠标 → YJX 模板；键盘 → ROYUAN 模板），再要求取得 WebHID 网页驱动 / 开源协议 / USB 抓包证据、确认 VID/PID 与报文校验后才动代码。

## 常见问题

**电量一直显示 `--`，或设备不出现在列表里？**
多为设备浅休眠：动一下鼠标/键盘再刷新。蓝牙设备需已配对且系统蓝牙开启；外部探针需放在 exe 同目录，或用 `PERIPHHUB_PROBE_EXE` 指定。

**双击没反应？**
程序可能已在后台运行（单实例）。从托盘找图标；再次运行会唤起已有窗口。

**支持我这款鼠标 / 键盘吗？**
大概率不支持。驱动是照着作者自用设备的私有协议逆向写的，实测通过的只有 Incott G23、Ksnake X11、ROYUAN AKOS BC98；同品牌其他型号不保证，能读出电量也不代表能改设置。先用 `--list` 看是否被识别，需要完整控制请按 [接入你自己的设备](#接入你自己的设备) 自己接。

**配置和宏保存在哪里？**
设置与别名保存在注册表 `HKCU\Software\PeriphHub`，宏库与别名文件在 `%LOCALAPPDATA%\PeriphHub\`。设 `PERIPHHUB_DATA_DIR` 可指定数据目录；把 `macros.ini` / `aliases.json` 放在 exe 同目录可实现便携使用。

**写入后数值显示 `unverified` 或回读不一致？**
部分固件无法回读（例如 G23 的 `0x82` X 轴 DPI、`0x06` 键位写入不回显）。PeriphHub 会如实标记 `unverified`，而不是伪造默认值；细节与规避方式见 [`docs/05-firmware-constraints.md`](docs/05-firmware-constraints.md)。

**杀软报警 / SmartScreen 拦截？**
发布的是未签名的 MinGW 单文件 exe，可能触发误报。可核对哈希后放行，或直接 `make dist` 自行编译。

## 参与贡献

欢迎提 Issue 和 PR。这是个人自用项目，不承诺响应时效；提交代码前请先看 [`CONTRIBUTING.md`](CONTRIBUTING.md)。

## 致谢与第三方声明

- 任务栏电量条视觉组件参考自 [`krisgfx9/Wireless-Device-Battery-Monitor`](https://github.com/krisgfx9/Wireless-Device-Battery-Monitor)（GPL-3.0）。
- 完整第三方组件与版权声明见 [`THIRD_PARTY_NOTICES.md`](THIRD_PARTY_NOTICES.md)。

## 许可证

本项目采用 **GNU General Public License v3.0 (GPL-3.0)** 开源协议，详见 [`LICENSE`](LICENSE)。
