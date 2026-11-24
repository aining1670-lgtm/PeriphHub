# 桥协议（网页 ⇄ C++）

两种传输，**同一套消息体**：

1. **WebView2 主通道**：`window.chrome.webview.postMessage` / `WebMessageReceived`
2. **HTTP 兜底**（仅开发期，**尚未实现**）：`POST http://127.0.0.1:6123/api`，同 JSON；P3 验收要求不开监听，故当前只有通道 1

## 消息格式

请求：

```json
{ "id": 7, "cmd": "dpi.set", "args": { "slot": 2, "dpiX": 800, "dpiY": 1600 } }
```

响应：

```json
{ "id": 7, "ok": true, "data": { "slot": 2, "dpiX": null, "dpiY": 1600, "unverified": ["dpiX"] } }
{ "id": 7, "ok": false, "error": "device sleeping" }
```

`id` 由前端自增，用于配对；`error` 是**给用户看的一句话**，不是堆栈。

`id` 必须是 JSON 整数（省略时按 `0` 处理）；字符串 `id`（如 `"id":"7"`）会被拒绝，返回 `{"id":0,"ok":false,"error":"id must be an integer"}`。`cmd` 必须是非空字符串，缺失或为空返回 `{"id":N,"ok":false,"error":"missing cmd"}`（`N` 是已解析到的 id）；`args` 可省略，出现时必须是对象，否则报 `args must be an object`。JSON 允许任意空白，`{ "id": 3, "cmd": "devices.get" }` 与紧凑写法等价。

请求由 `bridge::ParseRequest` **只解析一次**：`cmd`/`args` 都取自顶层对象，`args.cmd` 之类的嵌套字段不会被当作指令；`Request::args` 指向复用的解析树，宿主若要留到下一次解析之后使用，必须先复制所需字段。宿主中的回复**可以是异步的**：`id` 只负责配对，响应顺序不保证与请求顺序一致。

## 指令表

| cmd | args | data | 说明 |
|---|---|---|---|
| `devices.get` | — | `[{key,name,kind,via,driverLevel,battery,charging,connected,asleep,mode,receiver,psd,sensor,level,detail}]` | 轮询缓存，不碰硬件；汇聚原生 Incott G23/G24（`driverLevel:"full"`）、Windows 蓝牙 PnP、通用 HID 电量、XInput 与可选上游探针（`--json`）/`headsetcontrol`；`charging`：`2` = 充电中、`0+` = 放电中、`-1` = 未知 |
| `device.probe` | `{key}` | 同 `devices.get` | 先重读全部外设后端（Incott 带 3×100ms 重试）再返回合并去重后的列表；网页「刷新 / 扫描」用它 |
| `hub.get` | — | `{taskbarWidget,autoStart}` | 读取当前任务栏半透明电量条开关状态（`g_want_widget`）与 Windows 注册表 `HKCU\...\Run` 开机自启状态 |
| `hub.set` | `{taskbarWidget?,autoStart?}` | `{taskbarWidget,autoStart}` | 校验布尔类型（错型/全缺省 `ok:false`）；`taskbarWidget` 即时创建/销毁任务栏电量条；`autoStart` 读写 `HKCU\...\Run` 并回读校验 |
| `led.get` | — | `{supported,mode}` | 读 `0x8F` 判断是否 `8K` 无线接收器：非 8K 或有线模式返回 `{supported:false,mode:null}`；8K 无线读 `0x88`（3×40ms 重试，值 `0..2`，读失败或越界为 `null`） |
| `led.set` | `{mode}` | `{supported:true,mode}` | `mode` 必须为整数 `0..2`（0=连接/回报率、1=电池、2=仅低电警告）；先读 `0x8F` 校验 8K 无线接收器（非 8K 直接拒绝）；写 `0x08` 后 Sleep(60) 回读 `0x88` 比对 |
| `dpi.get` | `{key}` | `{count,active,levels,axisNote}`；levels 每档为 `{dpi,x:null,y,unverified:["x"]}` 或 `null`（该档读失败，不回填默认值）；`active` 超出 `0..count-1` 时为 `null` | 先读 `0x83` 探活取档数，再逐档读 `0x82`（每帧校验回包 slot 回显）；`0x82` 每档只有一个值 = 最后一帧写入的轴（官方写序下为 Y，X=Y 档即真值），X 无法独立回读故恒 `null`（WP3 实测，`docs/05` §11）；`axisNote` 是给消费方的形状说明 |
| `dpi.set` | `{key,slot,dpi}`（X=Y 单值）或 `{key,slot,dpiX,dpiY}`（分轴，两轴必须齐备）；`dpi` 与 `dpiX/dpiY` 不可混用（混用或类型错误直接拒绝） | X=Y：`{slot,dpi,dpiX,dpiY}`（回读值）；X≠Y：`{slot,dpiX:null,dpiY,unverified:["dpiX"]}` | 写入后**回读比对**（0x82 只能读到最后一帧：单帧 axis=0 校验同值，两帧写序只校验 Y；回包校验 slot 回显）；X≠Y 两帧间 100ms（axis=1 → 100ms → axis=2）；`slot` 缺失/负数/超出设备真实档数（先读 `0x83`）直接报错，不回退当前档/0；`dpi/dpiX/dpiY` 非 50..32000 步进 50 拒绝 |
| `dpi.stage` | `{key,active,count}` | `{active,count}`（`0x83` 回读值） | 写前 `0x83` 探活留基线；写 `0x03` 后 Sleep(60) 读 `0x83` 比对 count/active（写后首读可能拿到旧表/空报告，不匹配按 4×50ms 重试，最终不一致 `ok:false`）；`count` 非 1..6、`active` 越界（非 0..count-1）直接报错，不再静默改成 6/0 |
| `rate.get` | `{key}` | `{code,hz}` | 先读 `0x83` 探活（浅休眠下 `0x81` 仍被应答并回真实 rate 码，睡态只能靠深命令识别）；睡着回 `{code:null,hz:null}`；醒着读 `0x81`（3×40ms 重试），读失败或遇未映射 code 均回 `{code:null,hz:null}`，不用默认值兜底 |
| `rate.set` | `{key,hz}` 或 `{key,code}` | `{code,hz}`（回读值） | 参数严格校验类型（非整数直接拒绝）；写前先 `0x83` 探活，睡着 `ok:false device sleeping (move the mouse to wake it)` 且不写 `0x01`（浅休眠下 `0x01` 写/`0x81` 读仍被应答，回读证明不了鼠标侧状态）；125/250/500/1000 直接写；2000/4000/8000 仅当 0x8F `receiverType==2` 且 2.4G 无线，否则 `ok:false`（明确报错，不钳位；0x8F 读不到也拒绝）；写后回读 `0x81` 比对，读失败或不一致 `ok:false`，不用请求值兜底 |
| `sensor.get` | `{key}` | `{lod,ripple,fixline,motion,mode}` | 读 `0x84` sub1..sub5（每帧校验 `bytes[2]` 回显请求的 sub）；任一 sub 读失败或超出官方范围（`lod`/`mode` 0..2、flag 0..1）该项为 `null`（不把 `0xFF` 当 `true`）；`mode` 是原始码（0=节能/1=标准/2=竞技，官方语义），LOD 线上编码 0=1.0mm/1=2.0mm/2=0.7mm（官方错位，`docs/05` §3.4） |
| `sensor.set` | `{key,lod?,ripple?,fixline?,motion?,mode?}` | `{lod?,mode?}`（原始码）+ `{ripple?,fixline?,motion?}`（true/false），仅写入字段的回读值，类型与 `sensor.get` 对应字段一致 | 出现即校验类型（`lod`/`mode` 整数、`ripple`/`fixline`/`motion` 布尔，类型错误拒绝整条请求）；写前 `0x83` 探活（睡态报 `device sleeping (move the mouse to wake it)`，不写 `0x04`）；按字段顺序写 `0x04` 对应 sub 并逐个回读比对（多字段不回滚：中途失败可能已部分生效，UI 保持单字段调用）；`lod`/`mode` 越界（非 0..2）拒绝不钳位；一个字段都不给报错 |
| `misc.get` | `{key}` | `{debounce,sleep,fireLoops,fireInterval}` | 读 `0x85` sub1/sub3/sub2（校验 `bytes[2]` 回显及范围）；读失败或超限该项为 `null` |
| `misc.set` | `{key,debounce?,sleep?,fireLoops?,fireInterval?}` | `{debounce?,sleep?,fireLoops?,fireInterval?}`（仅写入字段的回读值） | 出现即校验整数类型（错误类型拒绝整条请求）；写前 `0x83` 探活（睡态报 `device sleeping (move the mouse to wake it)`，不写 `0x05`）；sub: 1 去抖ms / 2 连发 / 3 休眠秒；`fireLoops`(0..3)/`fireInterval`(0..255) 写 `0x05 02`（缺省字段先读设备补齐）；去抖 0..30、休眠 10..900（900 = 实测固件保留上限，`docs/05` §10）、fire 越界一律拒绝不钳位；多字段按顺序写入、不回滚（中途失败可能已部分生效，UI 保持单字段调用）；逐参数写后回读对应 sub 比对；一个参数都不给报错 |
| `keys.get` | `{key}`（可省略） | `{bindings:[...]}` | 读 `0x86` ×6；失败 `ok:false`，提示移动鼠标唤醒 |
| `keys.set` | `{key,index,binding}` | `{bindings:[...]}` | 单键写入 + 回读校验；不一致 `ok:false`；`binding` 语法见 `docs/05` 第 7 节；**不接受 `Macro:N`**（宏绑定走 `macros.upload`，否则按键会指向从未上传过的固件缓冲）；写入成功后自动清理该按键在 `g_macros` 中的旧 `bind` 标记并保存 `macros.ini`；写入成功但全表刷新失败时返回仅含该键的 `bindings`，不回空表 |
| `macros.list` | — | `[{slot,name,loop,cycles,actions,bind}]` | 本地宏库，不碰硬件；`bind` 是当前绑定到该槽的按键 index（0..5）列表 |
| `macros.save` | `{slot,macro}` | `{slot}` | 落盘 `%LOCALAPPDATA%\\PeriphHub\\macros.ini`；末项若是「按下」自动补一条松开 |
| `macros.upload` | `{slot,buttonIndex}` | `{slot,buttonIndex,binding,warning?}` | **slot 与 buttonIndex 解耦**：读库槽 `slot` 的宏，写固件缓冲 `buttonIndex`（固件宏缓冲号 = 按键 index 0..5，与库槽无关）；先传宏（320B = 10×32B，块间 40ms）再写按键；库槽为空报 `slot N is empty (save the macro first)`；任一步失败即中止；按键已写入但本地库保存失败时仍返回 `ok:true` 并带 `warning` |
| `macro.import` | `{json,slot?}` | `{slot,summary}` | Studio 格式（`docs/05` 第 4 节），同样自动收尾；`slot` 可选（0..9，显式给出即覆盖该槽），缺省按空槽 0–5 → 6–9 顺序挑选；旧 UI 导出格式 `incott-g23-macro-v1` 自动识别并转换成 Studio schema |
| `macro.export` | `{slot}` | `{json}` | 同上；空槽返回 `slot is empty` |
| `macros.delete` | `{slot}` | `{slot}` | 删除库槽并清掉指向它的 bind 记录；只删本地库，不影响固件里已上传的宏与按键绑定；空槽报 `slot N is empty` |
| `macros.rename` | `{slot,name}` | `{slot,name}` | 重命名库槽（≤32 字符）；空槽报错 |
| `device.info` | `{deviceKey?}` | `{key,kind,driver,driverLevel,capabilities}` | 查询设备后端驱动（`incott`/`yjx`/`royuan`/`none`）与能力声明；`royuan:3151:4015`（AKOS BC98 `0x03AE`）返回 `driver:"royuan"` 及 `capabilities.keyboard` |
| `kbd.get` | `{deviceKey}` | `{devId,model,firmware,profile,options,mainLed,sideLed,debounce,sleep}` | 仅 `driver:"royuan"`（需 `0x8F` 校验 `devId==0x03AE`）；USB 直连或 2.4G 接收器中继（`0xF7`→`0xF6`→目标命令→`0xFC`）读取配置层/选项位/主背光/侧翼灯/去抖/休眠时间 |
| `kbd.set` | `{deviceKey,profile?,options?/winLock/macMode/wasdSwap/backlightOff/sidelightOff,mainLed?,sideLed?,debounce?,sleep?}` | 同 `kbd.get`（回读验证后全量返回） | 严格校验所有字段类型与边界（`profile` 0..3、`mainLed.mode` 1..19 排除 13、`sideLed.mode` 0..5、亮度/速度 0..4、去抖 1..10、休眠 60..3600s）；`0x06`/`0x07`/`0x08`/`0x12` 先读后改保未知位，写后 `Sleep(60)` 回读校验不一致报错 |
| `kbd.keys.get` | `{deviceKey,profile?}` | `{profile,base,fn}`（128 × 4 字节数组） | 读取指定（或当前激活）配置层的 Base(`0x89`) 与 Fn(`0x90`) 全部 8 页（128 槽位×4 字节）映射 |
| `kbd.keys.set` | `{deviceKey,profile?,fnLayer?,slot,entry}` | `{profile,fnLayer,slot,entry,bulkUsed}` | 单槽位改键（先尝试 `0x13`/`0x15` 读回校验，未生效自动切 `0x09`/`0x10` 整层刷写并回读一致）；拒绝槽位 `>125`、Fn 层改 `slot 65` 及禁用最后一颗 `Fn` 键，硬屏蔽 `KB Lock [10,7,0,0]` |
| `kbd.keys.reset` | `{deviceKey,profile?,fnLayer}` | `{profile,base,fn}` | 将 AKOS BC98 指定层通过 `0x09`/`0x10` 批量恢复为出厂默认 98 键数组并整层 126 槽位逐字节回读校验 |
| `kbd.macro.list` | `{deviceKey,count?}` | `{maxSlots,slots:[{slot,name,empty,repeat,eventCount,byteLength}]}` | 扫描 `count`（默认 16，最大 50）个板载宏槽位 `0x8B` 状态并与本地别名 `%LOCALAPPDATA%\PeriphHub\royuan_macros.ini` 合并 |
| `kbd.macro.get` | `{deviceKey,slot}` | `{slot,name,empty,repeat,byteLength,events}` | 读取指定槽位 `0..49` 全部 4 页（256B）`0x8B` 宏数据与本地名称 |
| `kbd.macro.set` | `{deviceKey,slot,name?,repeat,events}` | 同 `kbd.macro.get` | 严格校验动作顺序（禁止悬空按下、总事件字节 `<=250B`、`repeat` 1..65535），编码 2B/4B 事件流通过 `0x16` 写入并多轮回读 `0x8B` 比对 |
| `kbd.macro.delete` | `{deviceKey,slot}` | `{slot,deleted:true}` | 将全零帧覆盖写入原宏占用的所有页，回读全零校验一致并移除本地别名 |

`devices.get` 返回的是**缓存**，不是当场读硬件：`battery = -1` 表示这一轮 `0x89` 没读到（未知 ≠ 0%），`connected = false` 表示轮询时设备不在。缓存由 native 工作线程轮询填充（启动一次 + 30s 定时器投递刷新任务，UI 线程不碰 HID），调试时可 `PeriphHub.exe --bridge '{"id":1,"cmd":"devices.get"}'` 直接看返回。

`receiver`/`psd`/`sensor` 来自 `0x8F`（如 `"receiver":"1K","psd":1,"sensor":"PAW3395"`）；`device.probe` 与 `devices.get` 返回同一形状，区别是前者先触发一次带重试的硬件重读（P3「刷新」按钮）。

事件（C++ → 网页，单向）：

```json
{ "event": "battery", "key": "incott:522C", "value": 32 }
{ "event": "device",  "key": "incott:522C", "connected": true }
{ "event": "error",   "message": "device sleeping" }
```

`keys.get` 的 `bindings` 每项是 `{index,matrix,code,binding,kind}`：`index` 是 UI 槽位 0..5（左/右/滚轮/前侧/后侧/DPI），`matrix` 是固件矩阵号（`[0,1,2,4,3,5]`），`binding` 可直接回填编辑框（`KeyA`、`Ctrl+KeyC`、`Mouse:Back`、`Media:Mute`、`Macro:2`、`Off`），`kind` 用于分组显示。`keys.set` 每次只改一个槽位（单键写入，与 `docs/05` 的 `0x06` 一致）；写后 60ms 回读，不一致返回 `ok:false`（`readback mismatch: wrote 0x… got 0x…`）。P4 只支持单设备，多设备寻址留到 P7。

`macros.*` 只碰本地库，`macros.upload` 才碰硬件，且顺序固定：先传宏（10 个 `09 07 0A <chunk> 20 <buttonIndex>` 头 + 32B Output，块间 40ms），成功后才写 `0x06` 把按键绑成 `Macro:<buttonIndex>`。**固件宏缓冲号 = 按键 index（0..5），不是库槽位（0..9）**；`0x06` 只存缓冲号、不存宏内容，所以重连/重启必须 `macros.upload` 重传（`macros.ini` 的 `bind` 行只记录「哪个库槽原本绑在哪个键」，不会自动重放）。宏内容不可从 G23 回读（`0x87` 仅 G24 可用），库是唯一副本。末项是「按下」时保存/导入会自动补一条松开（固件会丢弃悬空按下）；已满 71 项则拒绝并提示。库文件路径可用 `PERIPHHUB_DATA_DIR` 覆盖（测试/便携模式）。

## 硬规则

- 每个 `set` 必须回读校验，不一致返回 `ok:false` 并在 `error` 中给出期望值与实测回读值
- `macros.upload` 失败后**不得**再写任何按键（上一版 bug）
- 设备浅休眠时返回 `device sleeping`，前端提示「移动鼠标唤醒」，不要重试超过 2 次
- 桥里不做业务计算；DPI/宏编码全部在 `protocol.*`
