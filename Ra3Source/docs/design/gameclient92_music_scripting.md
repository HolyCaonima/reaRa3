# GameClient 0x0092 MusicScripting 重建

本批重建 `ra3_1.0.game` 的 `MusicScriptingSystem`、
`MusicScriptConditionFactory` 和 18 种 `MusicScriptConditionNugget_*`
运行时条件。共 53 个原生入口：20 个运行时类型名、17 个构造入口和 16
个比较、缓存、组合、事件及计数入口。

## 归属证据

归属不是按地址邻近推断：

- `00923740..00923D30` 返回完整的
  `MusicScriptConditionFactory`、`MusicScriptingSystem` 和 18 个
  `MusicScriptConditionNugget_*` 名称。
- `00924BD0..00926380` 构造函数写入连续虚表族
  `00C569F8..00C56C40`；这些虚表反向包含上述类型名、条件求值、
  序列化和析构入口。
- 静态字符串 `00BBEF70..00BC65F0` 是 18 个完整 nugget 运行时类名，
  `00C5692C/00C56968` 分别是 factory/system 名称。程序未保留可用的
  MSVC 标准 RTTI，因此不把地址邻近或猜测的 RTTI 当作证据。
- `00C55FE1/00C55FFD` 的断言源路径明确为
  `GameEngine\GameClient\PathMusicDynamicStateSystem\PathMusicDynamicStateSystem.cpp`；
  PathMusic 条件还通过 `00417080` 解码 typed state ID。
- `00923780` 直接调用 APT 文本/显示链函数 `00523630`、
  `00529830`、`00529840`，证明该族属于 GameClient 的 UI/音乐条件层。
- 资产布局给出 And/Or/Not、EngagementCount、HitBySuperWeapon、
  LocalPlayerKillRatio、PathMusic state 和 ScoredKillCount 的精确字段。
- 相邻的 `00928F80..009293E0` Win32 文件对象及
  `0092A110..0092E440` CRT/系统信息函数没有被纳入；单个
  `AptCIH::GetDisplayListPrevious` 符号不足以改变该结论。

Generals 仅有基础 `GameMusic` 播放代码：
`GeneralsMD/Code/GameEngine/Include/Common/GameMusic.h:69-115` 定义
`MusicTrack`/`MusicManager`，而
`GeneralsMD/Code/GameEngine/Source/Common/Audio/GameMusic.cpp:69-128`
只实现轨道配置和 play/stop 请求；没有本条件系统的等价类或算法。因此这些
精确引用只用于排除错误归属，没有从中复制行为。名称、布局、比较顺序和写集
均来自 RA3 原程序；不存在为凑数量添加的 padding。

## 入口

类型名（20）：

`00923740, 00923750, 00923760, 00923890, 00923950, 00923960,
009239C0, 009239D0, 009239E0, 009239F0, 00923A00, 00923A10,
00923A70, 00923B70, 00923BE0, 00923BF0, 00923C50, 00923C60,
00923C70, 00923D30`。

构造入口（17）：

`00924BD0, 00924C00, 00924C30, 00924D10, 00924D90,
00924E20, 00924E50, 00924E90, 00925290, 00925360, 009256D0,
00925700, 00925810, 00925990, 00925D70, 00926130, 00926380`。

行为与框架辅助（16）：

`009236C0, 00923970, 00923A20, 00923A30, 00923A50, 00923AD0,
00923B20, 00923C00, 00923C80, 00923E20, 00923EF0, 00923F00,
00923F50, 00923F80, 009241F0, 00926470`。

`GameClient92.h` 暴露条件种类、构造上下文、条件组合、时间比较、事件状态、
玩家缓存和加权得分接口。`MusicConditionObject` 保留 0x70 字节原始视图，
构造函数只改原程序实际写入的字节；checker 用随机非零种子验证未写区域。
PathMusic 和 HitBySuperWeapon 的 secondary listener 自锚位于
`+0x14/+0x18`，LocalPlayerKillRatio 的内嵌 0x34 记录位于 `+0x20`。

## ABI 与差分

`verify_gameclient92.py` 用磁盘映像的 clean native 指令执行每个入口，并与
`ra3_gameclient92check` 比较：

- 每入口的 `ret` cleanup、EAX/AL 返回及 EBX/ESI/EDI 保持；
- 完整构造写集和随机 untouched 字节；
- 空/混合 child 列表、null child 和短路调用次数；
- 五种比较操作、精确 epsilon `0x3A83126F` 和浮点位输入；
- 玩家缓存、事件状态、阈值和 weighted-score 结果。

`musicReconstructionEntries()` 为 53 个入口逐项记录 registers、cleanup、
writes、aliases 和 float policy。`--mutation-probe` 翻转一个预期字节并要求
oracle 拒绝，防止只跑通而未实际比较。
Ghidra 批量导出漏掉带内联 jump table 的 `00923C80`；它的精确范围为
`00923C80..00923D27`（168 字节），由 `function_additions.tsv` 补入 ledger，
因此 canonical ledger 与批次 annotation 均为 53 行。

Debug 和 Release 各执行 3,454 个差分用例，均为零差异；两配置 checker
也均报告 `53 entries, 0 mismatches`，CTest 均为 3/3。三项 mutation probe
分别破坏类型名、构造写集和比较结果，三项均被 oracle 拒绝。

## Hooks

`GameClient92Targets.cpp` 只挂 20 个无参数、只读的运行时类型名函数，全部
`explicitOnly=true`。shadow 比较字符串内容并返回原指针；replace 返回模块
静态字符串。没有构造/析构、renderer/backend、系统生命周期、全局音乐状态
或游戏对象写入 hook，也没有 x87 hook，因此不需要 naked ST0 桥。

## 延后

- `00923780` APT/UI 标记创建涉及显示链和全局 GameClient 状态。
- `00924C60` Not 构造的非空 child 路径带 intrusive refcount
  获取/释放；只验证空 child 不足以把整个入口标为完成。
- `00923D40/00923D80` 0x34 字节记录复制依赖其元素构造函数，未用只计算
  返回地址的近似实现冒充完整重建。
- `009244F0` factory 资产序列化、`00924EC0` 系统分配和生命周期。
- `009247A0` EVA 过期清理会调用 `0052A370` 释放关联状态，延后整体移植。
- `00924220` UnitsFarFromBase 和 `009247F0/009248A0` EVA 附近对象查询的
  成功对象图尚未验证；`00925510` superweapon 事件过滤也延后。
- `00925AF0/00925E40/00926240` 的跨玩家缓存填充及全局音乐状态更新。
- And/Or 非空资产解析和 allocator/refcount 生命周期保留为框架输入，不挂钩。
- 已合并的 `0092A0A0/0092A0D0` CRC、FXPS 和 W3DView 均未触碰。
