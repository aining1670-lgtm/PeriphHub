# Web Console Architecture (`web-next`)

## 1. 产品定位与空间架构：2.5D 硬件陈列架 + 全画布棚拍工作台 + 悬浮 Dock

旧版 `web/index.html`（9,122 行单文件，447 KB + 运行时 398 KB `tailwind.js`）采用“左侧设备树 + 右侧纵向表单流”的后台设置页结构。新版 **PeriphHub Console** 基于 React 19 + TypeScript + Tailwind v4 构建，采用统一工作台架构与共享元素 View Transitions：

| 区域 | 界面形态 | 核心职责 |
|---|---|---|
| **首页舞台：2.5D CNC 陈列架 (`HardwareShelf`)** | 双层阳极铝陈列架（可调参主力外设 / 伴随电量外设） | 一览全部外设真机轮廓与电量条，点击通过 View Transitions 平滑过渡进入设备工作台 |
| **设备工作台 (`DeviceConsoleShell`)** | 100% 全宽连贯暗室/陶瓷棚拍柔光画布 | 鼠标走 **50/50 舒展双栏**（GPW 俯视渲染图 + SVG 交互热区 + 精密仪器控制）；键盘走 **3D PBT 倒角 98 键物理配列**与实时轴体底光；伴随设备展示 **24 段能量刻度条**与任务栏开关 |
| **顶栏：集成无边框标题栏 (`AppShell`)** | 56px 透明顶栏直接融入画布 | 左：陈列架面包屑 + 任意设备行内重命名/恢复原名；中：模式切换 Tab；右：同步状态、扫描、主题切换、全局设置入口及 Win32 无边框最小化/最大化/关闭按钮（支持 `window.drag` / 双击最大化 / 右键系统菜单） |
| **底栏：悬浮磨砂多设备 Dock (`InnerDeviceDock`)** | 底部居中毛玻璃条 | 设备视图内快速零跳转切换其他已连接外设 |
| **全局设置：右侧滑出抽屉 (`SettingsDrawer`)** | 顶栏齿轮唤出 | 主题模式、Windows 任务栏电量条开关/双行/底板/品牌色/显示器选择、开机自启、低电阈值/轮询间隔与逐设备任务栏显隐控制 |

### 视觉、配色与核心控件规范

- **钛灰工业 / 冰银（Titanium Mono）双主题**：默认跟随 Windows 系统主题（`prefers-color-scheme`），支持 `跟随系统 / 浅色 / 深色` 三态切换，持久化至 `localStorage`（`periphhub.theme`），并通过 `window.theme` 同步 Windows DWM 窗口暗色属性；内联预渲染脚本消除首屏闪烁。
- **统一精密控件家族**：
  - **非线性分段滑轨 (`PrecisionSlider`)**：支持 DPI 分段映射（常用低 DPI 区间占主要行程）与键盘方向键微调，松手后严格执行硬件写入回读校验。
  - **可点击 + 可拖拽分段滑块 (`SegmentedTrack`)**：统一复用于无线回报率、传感器模式、LOD、自动休眠、连发次数与设定阈值。
  - **硬件拨动开关 (`hw-switch`)** 与 **原生模态 `<dialog>` 体系**（`ConfirmDialog` 与 `SettingsDrawer` 均采用 `<dialog>` + `showModal()`、`closedby="any"` 及 `overscroll-behavior: contain` 防止穿透滚动；Toast 通知采用 `popover="manual"` 确保始终位于 Top Layer 最上方）。
- **严格真值展示**：未读到/固件不支持回读项展示 `—` 或明确标注 `无法回读`（如 G23 `0x82` X 轴），绝不伪造默认值。

---

## 2. 技术栈与单文件离线打包方案

- **前端栈**：`Vite 6` + `React 19` + `TypeScript` (strict) + `Tailwind CSS v4` (`@tailwindcss/vite`) + `Zustand 5` + `lucide-react` + `vite-plugin-singlefile`
- **WebView2 `file:///` 单文件打包链路**：
  1. `web-next/` 开发期支持 `npm run dev` 热更新（仅在 `import.meta.env.DEV` 下动态加载 `mock-host.ts` 有状态模拟宿主，生产构建由 Vite/Rollup 自动 Tree-Shake 剔除）。
  2. `npm run build` 执行 `tsc --noEmit && vite build`，在 `vite.config.ts` 中通过 `rollupOptions.output.format: 'iife'` 与后处理插件将所有 JS / CSS / WebP 素材内联为**单个经典 `<script>` 离线包** `web-next/dist/index.html`（避免 WebView2 在 `file:///` 下对 `<script type="module" crossorigin>` 的同源策略拦截）。
  3. 产物替换根目录 `web/index.html`；`native/src/resource.rc` 与 `native/src/webview.cpp` 仅内嵌并提取 `101` 单文件资源（已移除 `tailwind.js`），最终产出完全独立的 `dist/PeriphHub/PeriphHub.exe`。

---

## 3. 前端四层架构

与 C++ 侧四层职责（协议纯函数 / 设备 IO / 驱动注册 / Bridge）一一对齐：

```text
web-next/src/
├── main.tsx                     # 入口
├── App.tsx                      # 根组件（主题监听、宿主事件订阅、bootstrap 与 #selftest*）
├── assets/                      # 内联 WebP 真机素材（mouse-gpw / headset-generic / gamepad-generic）
├── styles/tokens.css            # Tailwind v4 + Titanium Mono 双主题、2.5D 陈列架与 PBT 键帽样式
├── bridge/                      # 【Layer 1】WebView2 IPC、强类型指令、Mock 与自动化自测
│   ├── types.ts                 # 对齐 docs/02-bridge-protocol.md 与 command_json.cpp
│   ├── transport.ts             # postMessage + id 配对 + 超时 + 写操作串行队列
│   ├── commands.ts              # 全量硬件与无边框窗口控制 IPC API
│   ├── mock-host.ts             # 浏览器预览期有状态外设模拟宿主
│   └── selftest.ts              # 兼容 --webview-selftest / -write / -f26 自动化验收
├── state/                       # 【Layer 2】Zustand 状态管理
│   ├── theme-store.ts           # system / light / dark 同步 + DWM 窗口主题通知
│   └── hub-store.ts             # 设备列表、能力缓存、别名/全局设置、窗口状态与事件处理
├── devices/registry.ts          # 【Layer 3】按 driver & capabilities 匹配工作台模块与顶栏模式 Tab
├── modules/                     # 【Layer 4】硬件工作台模块与协议编解码
│   ├── shelf/HardwareShelf.tsx
│   ├── device-stage/{DeviceConsoleShell,GenericDeviceBench}.tsx
│   ├── mouse/{MouseWorkbench,mouse-codec}.ts
│   ├── keyboard/{KeyboardWorkbench,bc98-layout}.ts
│   └── macros/MacroStudio.tsx
└── ui/                          # 统一外壳、精密控件与确认弹窗
    ├── AppShell.tsx
    ├── SettingsDrawer.tsx
    ├── ConfirmDialog.tsx
    ├── PrecisionControls.tsx
    ├── DeviceGlyph.tsx
    └── view-transition.ts
```
