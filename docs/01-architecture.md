# 架构

## 进程模型

单进程、单窗口族：

```
wWinMain
 ├─ CreateMutexW("Local\\PeriphHub")         单实例
 ├─ 隐藏主窗口 PeriphHubWnd                   接托盘消息
 ├─ 托盘图标 + 任务栏电量条                     native 绘制
 ├─ 线程：HID 轮询（每 N 秒）                   电量、插拔
 └─ 窗口：DeviceSettingsWnd
       └─ WebView2 子窗口                      加载 web/index.html
```

配置写入与电量轮询**互斥**：配置窗口打开期间暂停轮询，避免同一 HID Collection
上 Feature Report 与轮询交错导致固件崩（上一版用 `DialogOpen()` 门控）。

## 四层职责

| 层 | 位置 | 规则 |
|---|---|---|
| 协议纯函数 | `native/src/*_protocol.*` | 无 Win32 依赖，可单测；字节级事实唯一来源（Incott / YJX / ROYUAN） |
| 设备 IO | `native/src/*_hid.*` & `peripherals.*` | 打开/枚举/Feature/Output；互斥锁；超时 ≤ 4s；多源外设扫描合并 |
| 驱动注册与分发 | `native/src/drivers/` | `registry.*` 按 `deviceKey` 前缀路由到对应后端并构建 `capabilities`；`builtin.cpp` 统一注册 |
| 桥 | `native/src/bridge.*` & `web/index.html` | JSON 指令 ↔ C++ 调用；前端依据 `driver` 与 `capabilities` 渲染 |

UI 只通过桥说话，不直接持有句柄、不直接算字节。

## 为什么 UI 用网页

| 需求 | 原生自绘 | 网页 |
|---|---|---|
| 布局/排版 | 手写坐标（上一版 2568 行） | CSS |
| 图表 | GDI+ 自绘 | 一个 canvas 库 |
| 深浅色 | 自己判断注册表 + 重绘 | `prefers-color-scheme` |
| 改动成本 | 重编译 | 改文件 |

代价：多一个 WebView2 运行时依赖（Win10/11 自带 Edge 一般已装，需检测）。
不采用 WebHID：HID 在 C++ 侧，网页只是界面，**这一点让 WebView2 的限制与我们无关**。

## 离线与安全

- 页面资源内嵌 exe（`RCDATA`）或 `SetVirtualHostNameToFolderMapping` 指向本地目录
- 不监听任何 TCP 端口；调试期如需要 HTTP 兜底，只绑 `127.0.0.1` + 一次性 token + Origin 校验
- 发布版关 DevTools
