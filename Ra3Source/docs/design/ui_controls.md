# UI 控件、窗口与消息辅助簇

本模块重建 `ra3_1.0.game` 中以 `0x00A70000–0x00A7FFFF` 为主的 53 个 UI
函数（含其直接 bounds 依赖 `0x00A6FFE0`）。范围不是归属依据：纳入项必须由
Apt/UI vtable、同一控制对象上的直接调用、
连续的输入消息表、共同布局读写或明确的文本/窗口语义闭合。仅因地址相邻但对象归属、
回调语义或副作用边界不清楚的函数不纳入。

实现位于 `src/ui`。游戏对象只通过不拥有内存的 `LayoutView`、
`InputRouterView`、`HitRegionView` 和 `TextEditView` 访问；模块不分配、释放或持有
游戏对象。输入路由、计时器、位状态、文本区间及消息编码均为可离线调用的状态转换，
游戏虚调用以 `Callbacks` 或窄化的 `TextMeasureFunction` 边界表示。

## 布局与消息约束

- `InputRouterView`：父 sink 在 `+0x04`，捕获槽在 `+0x08`；三个槽从
  `+0x0C` 开始、步长 `0x14`，active/alternate 字节在槽内 `+0x10/+0x11`。
- `HitRegionView`：点数组 begin/end/capacity 在 `+0x00/+0x04/+0x08`，
  缓存矩形在 `+0x10..+0x1C`，有效字节在 `+0x20`。
- `TextEditView`：UTF-16 指针 `+0x08`，caret/anchor 为 `+0x10/+0x14`，
  选择区间为 `+0x18/+0x1C`，选择模式 `+0x20`，移动标志 `+0x28`，
  鼠标选择字节 `+0x35`。
- `TextSlice` 是 12 字节 `{sourceHandle,start,count}`；`sourceHandle` 是别名，
  不转移所有权。
- 消息 `0x00E2A107..0x00E2A115` 保留 3-word/5-word payload 的区别以及
  sink 所见的局部 payload 别名。输入转发另行区分调用者 payload 与局部副本。

## 已接受函数

| VA | Ra3Source 名称 | 闭合依据 |
|---|---|---|
| 00A6FFE0 | `updateHitRegionBounds` | A71B40/A71B70 直接调用与 hit-region 布局 |
| 00A70060 | `setControlActive` | 控件状态与 delegate |
| 00A700D0 | `propagateEnabledState` | 控件状态与 delegate |
| 00A70110 | `propagateFocusState` | 控件状态与 delegate |
| 00A701A0 | `fitTextIndex` | 文本测量 vcall 与输出值 |
| 00A70220 | `tickUiTimer` | timer 布局与回调 |
| 00A70290 | `startUiTimer` | timer 布局与回调 |
| 00A704B0 | `clearPendingAction` | owned-action 布局与释放回调 |
| 00A70500 | `setControlPosition` | 控件 delegate 与 dirty 字节 |
| 00A70520 | `setControlExtent` | 控件 delegate 与 dirty 字节 |
| 00A70540 | `setControlClip` | 控件 delegate 与 dirty 字节 |
| 00A705A0 | `controllerUiObject` | controller 布局 getter |
| 00A70670 | `controllerInputTable` | controller 布局 getter |
| 00A70700 | `countControlFlags` | 两字位集 helper |
| 00A71200 | `dispatchInputRelease` | 三槽输入布局与释放消息表 |
| 00A712C0 | `routeIdleInput` | 三槽输入布局与 pointer tag |
| 00A71340 | `cancelInputCapture` | 三槽输入布局与 cancel tag |
| 00A71B20 | `initializeHitRegion` | hit-region 布局 |
| 00A71B40 | `copyHitRegionBounds` | hit-region lazy bounds |
| 00A71B70 | `hitRegionContains` | 同一 hit-region 几何 helper |
| 00A71E80 | `moveTextCaretLeft` | text-edit 布局与 UTF-16 |
| 00A72380 | `synchronizeControllerState` | controller vtable/state |
| 00A72660 | `setNamedControlBit` | 名称解析与位集 |
| 00A72D30 | `deactivateEventGate` | gate 布局与 cancel tag |
| 00A72DC0 | `dispatchActiveInputSlots` | 三槽输入直接调用闭包 |
| 00A72E20 | `routeTypedInputEvent` | 完整 typed-input tag 表 |
| 00A730B0 | `setPrimaryControlBit` | 控件位集与通知对象 |
| 00A73130 | `setSecondaryControlBit` | 控件位集与通知对象 |
| 00A73590 | `makeTextSlice` | UTF-16 handle/slice 布局 |
| 00A735F0 | `makeTextPrefix` | 同一 slice 家族 |
| 00A73640 | `makeTextSuffix` | 同一 slice 家族 |
| 00A740A0 | `dispatchUiMessage107` | 连续 typed-message 表 |
| 00A740E0 | `dispatchUiMessage108` | 连续 typed-message 表 |
| 00A74120 | `dispatchUiMessage109` | 连续 typed-message 表 |
| 00A74170 | `dispatchUiMessage10A` | 连续 typed-message 表 |
| 00A741B0 | `dispatchUiMessage10B` | 连续 typed-message 表 |
| 00A741F0 | `dispatchUiMessage10C` | 连续 typed-message 表 |
| 00A74230 | `dispatchUiMessage10D` | 连续 typed-message 表 |
| 00A74270 | `dispatchUiMessage10E` | 连续 typed-message 表 |
| 00A742C0 | `dispatchUiMessage10F` | 连续 typed-message 表 |
| 00A74300 | `dispatchUiMessage110` | 连续 typed-message 表 |
| 00A74340 | `dispatchUiMessage111` | 连续 typed-message 表 |
| 00A74380 | `dispatchUiMessage112` | 连续 typed-message 表 |
| 00A743C0 | `dispatchUiMessage113` | 连续 typed-message 表 |
| 00A74410 | `dispatchUiMessage114` | 连续 typed-message 表 |
| 00A74450 | `dispatchUiMessage115` | 连续 typed-message 表 |
| 00A74700 | `routeNearPointerEvent` | pointer coalescing 直接调用 |
| 00A748F0 | `activateEventGate` | gate 布局与 activate tag |
| 00A74E70 | `beginTextSelection` | text-edit 布局与 hit-test |
| 00A74EC0 | `updateTextSelectionFromPoint` | text-edit 直接调用闭包 |
| 00A75570 | `forwardUiMessage106` | 嵌套 sink 与 106 tag |
| 00A759F0 | `routePrimaryDoubleClick` | pointer 路由与时间/位置阈值 |
| 00A75A60 | `routeSecondaryDoubleClick` | pointer 路由与时间/位置阈值 |

## Oracle 与 ABI

`tools/analysis/ports/verify_ui_controls.py` 只能从已解析的 run35 capture 创建
`Emulator(capture=...)`，不读取受保护磁盘代码。它驱动 `uicheck`，对 53 个函数执行
随机及边界用例，比较：

- EAX/AL（仅在原函数定义返回值时）和 callee stack cleanup；
- 完整对象字节、辅助缓冲及输出缓冲的精确写入；
- 回调顺序、参数、typed-message tag、payload 指针别名；
- UTF-16 字符串长度、slice 的 source-handle 别名；
- hit-region 的整数到 float 边界行为。

`fitTextIndex` 的测量桩特意让输出值不同于输入 candidate，以验证成功路径取回调写入值。
名称到控制位的 resolver 也将 EAX index 与独立 success out-byte 正交生成；三种槽位的
normal/alternate/captured release tag 均由定向用例覆盖，未知 typed event 比较原始
payload word，而不是只比较空/非空。
变异检查会分别破坏 `countControlFlags` 和 typed-message tag；两种变异都必须被 oracle
检测，随后恢复基线。

## Hook 边界

以下 10 个目标均为 `explicitOnly=true`：

| key | VA | ABI / shadow 比较 |
|---|---|---|
| `uiupdatehitregionbounds` | 00A6FFE0 | thiscall；0x24 布局 |
| `uicontrollerobject` | 00A705A0 | thiscall；EAX |
| `uicontrollerinputtable` | 00A70670 | thiscall；EAX |
| `uicountcontrolflags` | 00A70700 | thiscall；EAX |
| `uiinitializehitregion` | 00A71B20 | thiscall；返回 this 与 0x24 布局 |
| `uihitregioncontains` | 00A71B70 | cdecl；AL 与 lazy-cache 布局 |
| `uimovetextcaretleft` | 00A71E80 | thiscall, ret 4；0x38 布局 |
| `uimaketextslice` | 00A73590 | cdecl hidden output；12 字节输出 |
| `uimaketextprefix` | 00A735F0 | cdecl hidden output；12 字节输出 |
| `uimaketextsuffix` | 00A73640 | cdecl hidden output；12 字节输出 |

这些入口不渲染、不发送 OS 消息、不分配、不改引用计数，也不执行析构。shadow 对会被
本实现修改的对象先做 clone，再调用 original，并比较 original 实际对象与 clone；
replace 保留原调用约定和 hidden-output EAX。此簇没有 x87 返回值。

## Generals 架构参考与原创边界

Generals GPL 源码只用于确认 SAGE GUI 的概念分层与命名，RA3 偏移、控制流、消息值和
实现均来自本项目的 RA3 原生证据，未复制 Generals 实现。具体参考：

- `work/references/generals/GeneralsMD/Code/GameEngine/Source/GameClient/GUI/GameWindow.cpp`
  （窗口状态、region、draw/input/system callback 的分层）。
- `work/references/generals/GeneralsMD/Code/GameEngine/Include/GameClient/GameWindow.h`
  （窗口消息、handled 结果和 status/style 位概念）。
- `work/references/generals/GeneralsMD/Code/GameEngine/Source/GameClient/GUI/GameWindowManager.cpp`
  （focus/capture、父窗口消息传播和延迟销毁边界）。
- `work/references/generals/GeneralsMD/Code/GameEngine/Source/GameClient/GUI/Gadget/GadgetTextEntry.cpp`
  （文本输入、caret 类状态与 owner 消息的架构概念）。
- `work/references/generals/GeneralsMD/Code/GameEngine/Source/GameClient/GUI/Gadget/GadgetListBox.cpp`
  （坐标命中、行列状态和 double-click 概念）。
- `work/references/generals/GeneralsMD/Code/GameEngine/Include/GameClient/GadgetTextEntry.h`
  （gadget 通过 window-manager system message 交互的接口形状）。

## 延后边界

- `00A71B40` 的开头含短条件跳转和相对 call，不能由当前 trampoline 安全搬移，故仅
  离线重建，不挂 hook。
- rendering、OS/window 消息、allocator/refcount/destructor 和全局 UI mutation
  路径全部延后；未证明 clone-shadow 的入口不挂。
- 范围内其余函数若 vtable 所有者、调用约定、全局对象语义或输出布局未闭合，保持
  unknown；不以地址邻近补数。
