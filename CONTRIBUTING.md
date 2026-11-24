# Contributing to PeriphHub

感谢考虑为 PeriphHub 贡献代码或接入新设备！

## 两条设备接入路径

1. **只想显示设备电量 / 充电状态**（无需编译 PeriphHub）：
   请阅读 [`docs/09-external-probe.md`](docs/09-external-probe.md)，编写独立探针程序并输出约定 JSON 数组。仓库内提供示例 [`examples/battery-probe/battery_probe.cpp`](examples/battery-probe/battery_probe.cpp)。
2. **接入完整硬件驱动**（DPI / 改键 / 宏 / 灯效等）：
   请阅读 [`docs/10-adding-a-device.md`](docs/10-adding-a-device.md)，新增纯函数协议模块、HID 通道、驱动描述符与单测，并在 [`native/src/drivers/builtin.cpp`](native/src/drivers/builtin.cpp) 注册。若使用 AI 编码助手，仓库内置 [`AGENTS.md`](AGENTS.md) 与 [`.agents/skills/add-device-driver/SKILL.md`](.agents/skills/add-device-driver/SKILL.md) 可直接按标准流程引导适配。

## 本地构建与测试环境

在 **MSYS2 / MinGW-w64** 环境中执行：

```bash
export PATH="/c/msys64/usr/bin:/c/msys64/mingw64/bin:$PATH"
make clean && make     # 编译 build/PeriphHub.exe（必须 0 warning）
make test              # 运行全部无硬件纯 C++ 单元测试
make probe-example     # 编译外部探针示例 build/battery-probe-example.exe
```

> **Windows 沙箱提示**：若工作区带有 Low-IL ACL 权限，直接从工作区内启动 `PeriphHub.exe` 可能遇到托盘注册被拒或 `0xC0000142`。可将 `build/PeriphHub.exe` 与 `build/WebView2Loader.dll` 复制到工作区外目录（例如 `C:\Temp\bh_test`）后再运行 `--bridge`、`--list-json` 或 `--webview-selftest`。

## 提交前自检

- 代码遵守 C++17 风格，保持零编译警告（`-Wall -Wextra`）
- 新增协议或路由逻辑必须附带无硬件纯 C++ 单测并接入 `Makefile` 的 `test` 目标
- 运行 `node tools/check_bridge_json.js <path/to/PeriphHub.exe>` 确保全通过
- 提交信息建议使用 Conventional Commits（如 `feat(driver): ...`、`refactor(native): ...`、`docs: ...`）

## 许可证

本项目采用 **GNU General Public License v3.0 (GPL-3.0)** 开源协议，详见 [`LICENSE`](LICENSE)。向本仓库提交贡献即表示你同意贡献内容以相同协议发布。
