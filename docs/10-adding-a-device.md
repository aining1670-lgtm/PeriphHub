# 完整驱动接入指南（Adding a Device Driver）

如果你希望让 PeriphHub 不仅显示设备电量，还能通过设置窗口配置 **DPI、改键、板载宏、回报率、传感器或灯效**，可以按源码级驱动模块的方式提交 PR 接入。

建议在动手前通读 **YJX 驱动**（[`native/src/yjx_protocol.h`](../native/src/yjx_protocol.h)、[`native/src/yjx_hid.h`](../native/src/yjx_hid.h)、[`native/src/drivers/yjx_driver.cpp`](../native/src/drivers/yjx_driver.cpp)），它是结构最干净的参考样板。

---

## 1. 需要新增与修改的文件清单

得益于驱动注册表（`drivers::Registry`），新驱动**不需要修改 `main.cpp` 的 `BridgeHandler` 分发逻辑**：

| 步骤 | 文件 | 职责 |
|---|---|---|
| 1. 协议纯函数 | `native/src/<vendor>_protocol.{h,cpp}` | 无 Win32 依赖的报文编解码、校验和、参数边界检查 |
| 2. 协议单测 | `tests/<vendor>_protocol_test.cpp` | 覆盖报文拼装、回包解析、越界拒绝（接入 `Makefile` 的 `make test`） |
| 3. HID 通道封装 | `native/src/<vendor>_hid.{h,cpp}` | 设备枚举、Feature/Output 读写、写后回读校验 |
| 4. 设备发现接入 | `native/src/peripherals.{h,cpp}` | 新增 `Scan<Vendor>Devices()` 并在 `ScanAllDevices()` 中调用、传入 `MergeDevices()` |
| 5. 能力声明 | `native/src/drivers/builtin_caps.{h,cpp}` | 实现 `Build<Vendor>Capabilities` 与 `<Vendor>Descriptor()` |
| 6. 桥指令处理 | `native/src/drivers/<vendor>_driver.{h,cpp}` | 实现 `Handle(const bridge::Request&, const std::string& device_key)` |
| 7. 注册驱动 | `native/src/drivers/builtin.cpp` | 在 `RegisterBuiltinDrivers()` 中新增 **1 行** 注册代码 |

注意：
- `device_key` 前缀必须是**全局唯一的小写命名空间**（例如 `mybrand:VID:PID`），不要与现有前缀（`incott:`、`ksnake:`、`royuan:3151:`）重叠。
- 如需持久化简单配置或读取应用数据目录，可通过 [`native/src/drivers/host.h`](../native/src/drivers/host.h) 暴露的宿主服务访问，尽量避免直接耦合 `main.cpp` 全局状态。

---

## 2. `capabilities` JSON Schema 规范

前端页面完全通过 `device.info` 返回的 `capabilities` 与 `driver` 字段决定渲染鼠标视图、键盘视图还是仅电量卡片，禁止再在前端写死设备 key 前缀。

以下是当前内置驱动使用的完整 Schema（空能力设备使用 `BuildEmptyCapabilities()`）：

```json
{
  "dpi": {
    "min": 200,
    "max": 12000,
    "step": 100,
    "maxStages": 6,
    "xySplit": false,
    "stageCount": true
  },
  "rates": [125, 250, 500, 1000],
  "sensor": {
    "lod": ["1.0", "2.0"],
    "modes": [],
    "ripple": true,
    "fixline": true,
    "motion": true
  },
  "misc": {
    "debounce": { "min": 0, "max": 30 },
    "sleep": {
      "min": 10,
      "max": 900,
      "step": 10,
      "presetsSec": [60, 180, 300, 600, 1200, 1800, 3600],
      "discrete": true
    },
    "scrollReverse": true,
    "fire": false
  },
  "lighting": {
    "mouseLed": [
      { "code": 0, "label": "灭灯" },
      { "code": 1, "label": "波浪" }
    ],
    "receiverLed": false
  },
  "keys": true,
  "macros": true,
  "keyboard": {
    "model": "AKOS BC98",
    "layout": "bc98",
    "fnSlot": 65,
    "profiles": [0, 1, 2, 3],
    "keymap": true,
    "fnLayer": true,
    "macros": { "maxSlots": 50, "defaultVisibleSlots": 16, "maxStreamBytes": 250 },
    "brightnessMax": 4,
    "speedMax": 4,
    "debounce": { "min": 1, "max": 10 },
    "sleep": {
      "minSec": 60,
      "maxSec": 3600,
      "shallowPresetsSec": [60, 120, 300, 600, 1800],
      "deepPresetsSec": [600, 1200, 1680, 1800, 3600]
    },
    "options": ["winLock", "macMode", "wasdSwap", "backlightOff", "sidelightOff"],
    "mainModes": [{ "code": 1, "label": "常亮", "supportsColor": true }],
    "sideModes": []
  }
}
```

字段约束说明：
- **通用鼠标字段**：不支持某项时将对应对象设为 `null`（例如 `dpi: null`、`sensor: null`、`misc: null`、`lighting: null`）、`rates: []`、`keys: false`、`macros: false`。
- **`misc.sleep`**：连续调节型（如 Incott）提供 `min`、`max`、`step`、`presetsSec`、`discrete: false`；离散档位型（如 YJX）提供 `presetsSec`、`discrete: true`。
- **`keyboard`**：仅键盘驱动返回该对象；存在 `capabilities.keyboard` 时前端切换到键盘驱动工作区。
- **任意配列（87 / 98 / 104 / 108+ 键）**：
  - `keyboard.layout` 支持内置标准配列 ID：`"bc98"`（98 键）、`"ansi-87"`（87 键 TKL）、`"ansi-104"`（104 键全尺寸）、`"ansi-108"`（108 键全尺寸）。
  - `keyboard.fnSlot` 指定 Fn 键所在的硬件槽位索引（例如 `65`；无 Fn 限制时可省略或设 `null`）。
  - 若键盘为非标准配列或槽位顺序不同，可直接在 `keyboard.keys` 中下发按键数组（每项 `{ "slot": 0, "code": "Escape", "label": "Esc", "x": 1, "y": 1, "w": 40, "h": 40, "base": [0,0,41,0], "fn": [0,0,0,0] }`），前端会自动根据坐标计算画布宽高比并渲染所有键帽。

---

## 3. Bridge 命令族与拒绝契约

`drivers::Registry` 将驱动命令分为 5 个命令族。只有列在 `DriverDescriptor::commands` 中的命令才会路由到你的 `Handle()`；未声明或缺少 `handle` 时，宿主会自动返回标准错误文案：

| 命令族 | 包含命令 | 未支持时的标准错误文案 |
|---|---|---|
| 按键重映射 | `keys.get`、`keys.set` | `key remapping is not supported by this device` |
| 鼠标宏 | `macros.list`、`macros.save`、`macros.upload`、`macro.import`、`macro.export`、`macros.delete`、`macros.rename` | `macros are not supported by this device` |
| DPI | `dpi.get`、`dpi.set`、`dpi.stage` | `DPI configuration is not supported by this device` |
| 硬件杂项与灯效 | `rate.get/set`、`misc.get/set`、`sensor.get/set`、`led.get/set` | `hardware settings are not supported by this device` |
| 键盘全套 | `kbd.get/set`、`kbd.keys.get/set/reset`、`kbd.macro.list/get/set/delete` | `keyboard configuration is not supported by this device` |

详细请求/响应结构请参阅 [`docs/02-bridge-protocol.md`](./02-bridge-protocol.md)。

---

## 4. 固件交互原则（必读）

1. **禁止静默钳位越界参数**：前端与后端必须使用统一边界校验（使用 `bridge::OptIntArg` / `OptBoolArg` 做严格类型检查），非法类型或越界值一律返回 `bridge::Failure(...)`。
2. **写后必须回读校验**：所有写入操作在延时等待固件落盘后必须回读确认；回读失败或值不匹配时返回 `ok: false`，禁止用请求参数伪造回显。
3. **只读保留字节**：对于采用整帧读-改-写（Read-Modify-Write）的设备（参考 YJX `0x0E`/`0x0F` 与 ROYUAN `0x86`/`0x06`），未涉及的保留字节与未知比特位必须原样回填。

---

## 5. 提交 PR 前检查清单

- [ ] `make clean && make` **零编译警告（0 warning）**
- [ ] `make test` 全部纯 C++ 单测通过，且包含新增设备的 `<vendor>_protocol_test`
- [ ] 使用 `node tools/check_bridge_json.js <exe>` 验证 `--bridge` JSON 与退出码（`ok:true` 返回 0，`ok:false` 返回 1）
- [ ] 若改动了 `web-next/`（并重新生成 `web/index.html`），确认无新增硬编码设备 `key` 前缀分支，并通过 `--webview-selftest`
- [ ] 附带真机验证记录（`--list-json` 输出、至少一组参数写入与回读对比）
