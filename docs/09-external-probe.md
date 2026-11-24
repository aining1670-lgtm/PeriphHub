# 外部电量探针协议规范（`battery-probe --json`）

如果只想把一款私有协议外设的**电量、充电状态和在线状态**接入 PeriphHub 托盘图标、任务栏电量条与设备总览页，**不需要修改 PeriphHub C++ 源码或重新编译主程序**——你可以用任意语言（C++ / Rust / Go / Python / C# 等）编写一个独立可执行文件作为外部电量探针。

---

## 1. 探针发现机制

PeriphHub 在每次设备扫描时，按以下优先级查找外部探针（实现见 [`native/src/peripherals.cpp`](../native/src/peripherals.cpp)）：

1. **环境变量 `PERIPHHUB_PROBE_EXE`**
   - 若值为 `0`、`none` 或 `off`（大小写不敏感），**禁用**外部探针，不再回退到同目录查找。
   - 否则若该路径指向一个存在的文件（且不是 `PeriphHub.exe` 自身），则使用该可执行文件。
2. **默认同目录探针**
   - 未设置 `PERIPHHUB_PROBE_EXE` 时，自动查找与 `PeriphHub.exe` 同目录下的 `battery-probe.exe`。

---

## 2. 进程调用约定

- **命令行参数**：`<probe_exe> --json`
- **工作目录**：`<probe_exe>` 所在目录
- **标准输入 / 标准错误**：均重定向到 `NUL`（不可依赖交互输入，日志也请勿依赖 stderr 展示）
- **窗口标志**：`CREATE_NO_WINDOW`（无控制台窗口静默执行）
- **超时限制**：`kUpstreamTimeoutMs = 7000`（7000 ms）；超时后 PeriphHub 会强制终止探针进程并丢弃本次输出
- **输出上限**：`kMaxProbeStdoutBytes = 262144`（256 KB）；超过部分会被截断，从而导致 JSON 解析失败
- **退出码处理**：PeriphHub **不检查进程退出码**，只要求探针在 7000 ms 内退出、标准输出非空且为合法的 UTF-8 JSON 数组

---

## 3. 输出 JSON 规范

标准输出必须是 UTF-8 编码的 JSON **数组**（`[...]`），数组每个元素代表一台设备。缺少非空字符串 `key` 或 `name` 的对象会被忽略。

### 字段说明

| 字段 | 类型 | 必填 | 说明与归一化规则 |
|---|---|---|---|
| `key` | string | 是 | 全局唯一设备标识。建议使用 `ext:<vendor>:<id>`（例如 `ext:example:0001`）。**禁止使用内置驱动前缀** `incott:`、`ksnake:`、`royuan:3151:`，否则会与内置驱动路由冲突 |
| `name` | string | 是 | 设备显示名称（如 `"Example Device"`） |
| `via` | string | 否 | 连接介质标签（如 `"2.4 GHz receiver"`、`"Bluetooth"`、`"BLE"`、`"USB"`）。由 `ModeFromVia()` 映射为 `mode`：含 `"bluetooth"` 或词 `"ble"` → `"bluetooth"`；含 `"usb"` 或 `"wired"` → `"wired"`；其余 → `"wireless"` |
| `kind` | string | 否 | 设备类型（`"mouse"`、`"keyboard"`、`"headset"`、`"gamepad"`、`"microphone"`、`"phone"`、`"other"`）。会经 `NormalizeKind(kind, name)` 结合设备名二次校准 |
| `percent` | number | 否 | 电量百分比。负数统一归一化为 `-1`（未知），大于 `100` 夹紧为 `100` |
| `connected` | boolean | 否 | 当前是否在线连接（默认 `false`） |
| `asleep` | boolean | 否 | 是否处于浅休眠状态（默认 `false`） |
| `charging` | boolean | 否 | 是否正在充电（默认 `false`）。内部映射：`true` → `charging = 2`；`false` 且 `connected && percent >= 0` → `charging = 0`；其余 → `-1` |
| `level` | string | 否 | 无精确百分比时的粗粒度等级文本（如 `"Low"`、`"Medium"`、`"Full"`） |
| `detail` | string | 否 | 补充状态文本（如 `"L 80%, R 75%"`、`"2.4G dongle"`） |

外部探针接入的设备，`driverLevel` 固定为 `"battery"`（只读监控），不参与驱动写入。

---

## 4. 合并与去重规则

`MergeDevices()` 按以下优先级合并所有设备来源（先出现者保留）：

1. 原生完整驱动（`incott`、原生多厂商后端 `royuan` / `yjx`）
2. 外部电量探针（`--json`）——若内置驱动已扫描到同一 `key` 或同一 `VID:PID`，原生结果优先并跳过探针条目
3. Windows 蓝牙 PnP
4. `headsetcontrol.exe`（注意：**当外部探针执行成功并返回有效 JSON 数组时，PeriphHub 会跳过 `headsetcontrol.exe`**）
5. 通用 HID Battery Strength 与 XInput 手柄

排序顺序：`full` 完整驱动设备 → 已连接设备 → 离线设备。

---

## 5. 完整示例 JSON 与参考实现

下面这段 JSON 与 [`tests/peripheral_test.cpp`](../tests/peripheral_test.cpp) 中的契约测试保持同步：

```json
[{"key":"ext:example:0001","name":"Example Device","via":"2.4 GHz receiver","kind":"mouse","percent":78,"connected":true,"asleep":false,"charging":false,"level":"","detail":"2.4G dongle"}]
```

仓库自带最小 C++ 示例 [`examples/battery-probe/battery_probe.cpp`](../examples/battery-probe/battery_probe.cpp)，可通过以下命令编译并快速验证：

```bash
make probe-example
cp build/battery-probe-example.exe /path/to/PeriphHub/battery-probe.exe
```

也可以直接用 `PERIPHHUB_PROBE_EXE` 指定自定义探针或脚本包装器进行测试。
