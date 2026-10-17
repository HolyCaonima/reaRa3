# 0x00790000–0x0079FFFF AI 状态，第四遍

## 范围

前三遍已经计入 `AIState79`、`AIState79B` 和 `AIState79C`。这一遍只取同一窗口里仍是 clean、`unanalyzed`、并且不在那三份注释表上的函数。计入 48 个。归属写在 `work/aistate79d_claims.txt`，`verify_ownership.py` 的结果是 `fails=0`，`check_cited.py` 的结果是 `suspect=0`。

类名来自拆分虚表 slot 2 的 `mov eax, imm32; ret`。没有 MSVC RTTI。同一个函数体出现在多张表里时只计一次，名字取其中一张表的类。这些表各自有自己的 slot 2 getter，没有两张表共用同一个 getter。`00798860` 被 27 张表的 slot 6 指向，getter 仍然各不相同，名字取 `MoveInStraightLineState`。

`007985D0` 没有虚表。`0079D8F0`（`SA_AIFaceTargetState` `00C2E138` slot 5）在 `0079D928` 用 `E9` 跳进它，调用方本身有虚表归属。

布局沿用前三遍：`this+0x18` 是状态机，状态机 `+0x24` 是单位，单位 `+0x374` 是 locomotor。`dword [0x00DEBA64]` 是 TheGameLogic，帧号在 `+0x58`，时间缩放在 `+0x1C4`。`dword [0x00DEEEE8]` 是本遍 checker 额外传入的缩放浮点。地形、寻路和分区指针在 `0x00DFC190`、`0x00DFC198`、`0x00DFB9D8`，盘上是 0，oracle 在调用前写入。`dword [0x00BE700C]` 是最大有限浮点 `0x7F7FFFFF`。`dword [0x00BD9534]` 是 5.0f（`0x40A00000`），自由落体用它做高度偏置。`dword [0x00CB0910]` 是 15，交货和驻军空闲用它做步长。圆周率常量是 `0x00C297DC`、`0x00CC5520`、`0x00BDB8E4`，小角度是 `0x00C2B770`。返回立即数是继续 `0`、完成 `0xFFFFFFFF`、失败 `0xFFFFFFFE`。

读到的源码字符串有三处。`00797A10` 在 `00797A4C` 和 `00797A6C` 推 `0x00C2A938`，文本是 `TurretStates\IdleScan.cpp`。`00799410` 在 `00799419`、`0079B1C0` 在 `0079B201` 推 `0x00C2C1A8`，文本是 `AIStates.cpp`。`0079B690` 在 `0079B6CC` 推 `0x00C2D5A8`，文本是 `AITNGuard.cpp`。

没有 hook。样板（slot 2 getter、NAMEKEY 缓存、返回常数、单调用转发、单字段存取）是 0/48。最薄的 `0079C960` 仍是生成子状态再进入，`007986E0` 是调用后清航向，xfer 各自搬多个字段。

## 浮点顺序

`007985D0` 用 SSE 把 `machine+0x54 - unit+0x44` 卷进 `(-pi, pi]`，再 `fabs` 后和 `0x00C2B770` 比较。小于小角度时 `0078C080(unit, 原始航向)` 并返回完成；否则 locomotor slot `0x26C` 传入同一份原始航向并返回 0。`0079D8F0` 先 `006A6CB0` 取朝向，加上 `unit+0x44`，`005AA280` 写回状态机，再跳到 `007985D0`。

`0079D5C0` 用 `comiss` 比较高度和 `Z - logicTable+0x6C`。低于界限时 `0078D1E0` 并返回 0；否则可能 `006A7AE0`，然后 `00861E30`、`00687380`，返回完成。`0079D6A0` 的界限是 `Z - 5.0f`，低于界限才移动，随后帧号小于 `this+0x20` 返回 0，否则完成。

`007970D0` 在截止帧之前用无符号浮点（负整数先加 `0x4F800000`）算 `duration` 的比例，再交给 `006EC710`。低字节非 0 返回完成。`00797A10` 两端点都是有序 0 时直接完成；否则采样后按 `005A94C0` 的低字节决定是否用 `-0.0` 取反。`00797890` 在绝对值不大于 0.1 时把返回值的低字节改成 1。

`0079FFA0` 的账本长度在 `007A002A` 结束，实际 `ret` 在 `007A008C`，失败路径在 `007A008D`。本遍不把 `007A` 的地址记成单独的函数。`00861CB0` 是 `ret 8`，调用点推了 4 个 dword，剩下的 8 字节由随后地形 slot `0x24` 以 `ret 0x14` 一起弹出。清理字节配成 12 时外层 `ret` 会落到 0；配成 20 后与移植的 EAX、调用记录和子状态字节一致。

## 函数

| VA | 归属 | C++ | 行为 |
| --- | --- | --- | --- |
| `007970D0` | UnitAIReloadAmmoState slot 5 | `unitAIReloadAmmoUpdate` | 武器为空则失败；过了截止帧传 1.0，否则传无符号比例 |
| `00797510` | UnitAIDriftState slot 6 | `unitAIDriftSlot6` | `logicTable+0x70 * scale` 截断后 `006F04A0(unit, 0x12, ticks)` |
| `00797890` | TurretAIAimTurretStateWhenBusy slot 19 | `turretAIAimTurretWhenBusySlot19` | 绝对值不大于 0.1 时低字节为 1 |
| `00797A10` | TurretAIIdleScanState slot 4 | `turretAIIdleScanOnEnter` | 两端为 0 则完成；否则采样并可取反，写入 `+0x20` |
| `007985D0` | `0079D8F0` 的 `E9` | `saAIFaceTargetApplyHeading` | 卷绕后小角度走 `0078C080`，否则 locomotor slot `0x26C` |
| `007986E0` | UnitAIRotateToPathState slot 6 | `unitAIRotateToPathSlot6` | `005AA280(machine, +0x20)` 后把 `+0x20` 清成 +0.0 |
| `00798700` | 同表 slot 3 | `unitAIRotateToPathXfer` | 版本字节、slot `0x64` 的 `+0x20`、再一次版本字节 |
| `00798860` | MoveInStraightLineState slot 6，另有 26 张表 | `moveInStraightLineSlot6` | 子状态非空则 slot 0 参数 1；总是清 `+0x20` |
| `007989A0` | UnitAIWaitForOccupantsState slot 4 | `unitAIWaitForOccupantsOnEnter` | 两次 `006D2020` 后 `006A4EA0`，返回 0 |
| `00798D70` | AIDockProcessDockState slot 3 | `aiDockProcessDockXfer` | 版本字节、`0054A090`、两个 slot `0x6C` |
| `00799100` | AIHarvestWorkSiteState slot 3 | `aiHarvestWorkSiteXfer` | 版本字节后 `+0x20/+0x34/+0x30/+0x28/+0x2C/+0x24` |
| `00799410` | AIGarrisonedIdleState slot 4 | `aiGarrisonedIdleOnEnter` | `15 * roll`，有逻辑对象时再乘时间缩放，然后 `006D2020(unit, 0x8E, 0)` |
| `00799820` | AIIdleState slot 3 | `aiIdleXfer` | 版本字节和 `+0x20/+0x24/+0x25/+0x22` |
| `00799AE0` | AIBackAwayAndCowerState slot 6 | `aiBackAwayAndCowerSlot6` | 子状态为空则原样返回；否则 slot 0 后清 `+0x20` |
| `00799CA0` | AIUncontrollableCower slot 3 | `aiUncontrollableCowerXfer` | 版本字节、slot `0x84` 的 `+0x20`、再一次版本字节 |
| `00799EC0` | AIFollowPathAsTeamState slot 9 | `aiFollowPathAsTeamSlot9` | 链接为空返回 0；对象为空则低字节为 0；否则反转 slot `0x20` 的低字节 |
| `00799EF0` | AIAttackMoveToState slot 9 | `aiAttackMoveToSlot9` | 与上一行相同，链接在 `+0x58`，没有空指针短路 |
| `00799F80` | AIAttackAimAtTargetState slot 3 | `aiAttackAimAtTargetXfer` | 版本字节后三个 slot `0x84` 字节 |
| `0079A1D0` | AIAttackFireWeaponState slot 6，也在 Position 表 | `aiAttackFireWeaponSlot6` | 三次 `006D2020`，返回最后一次 |
| `0079A480` | AIAttackState slot 8 | `aiAttackSlot8` | 链接为空返回 0；对象为空则低字节为 1；否则尾调 slot `0x20` |
| `0079AF30` | AIGarrisonedState slot 8，另有三张 Guard 表 | `aiGarrisonedSlot8` | 链接为空返回 1；其余与 `0079A480` 相同 |
| `0079B1C0` | AIAttackAreaState slot 4 | `aiAttackAreaOnEnter` | 状态机 slot `0x20`，时间缩放乘 scale 后掷点，尾调子对象 slot `0x14` |
| `0079B280` | AIFaceState slot 4 | `aiFaceOnEnter` | 探针浮点有序等于 0 时把 `+0x2C` 写成 1；模式 2 尾调 `00799890` |
| `0079B690` | AITNGuardIdleState slot 4 | `aiTNGuardIdleOnEnter` | 分区样本乘时间缩放后掷点，`00682D90(loco, 0)` |
| `0079C5D0` | UnitAIGetBoardingLocationState slot 4 | `unitAIGetBoardingLocationOnEnter` | 目标为空则失败；探针失败时目标点是 `goal+0x38`，命中时用栈上替换向量 |
| `0079C6E0` | UnitAIHeliEvacuateState slot 4 | `unitAIHeliEvacuateOnEnter` | 寻路接地且输出不小于 Z 则失败；否则给子状态设目标并进入 |
| `0079C960` | AIMoveToFollowThroughState slot 4，也在伪装空闲表 | `aiMoveToFollowThroughOnEnter` | 生成子状态并进入 |
| `0079C990` | AIMoveToGoalDiveBombState slot 4 | `aiMoveToGoalDiveBombOnEnter` | 设目标、写航向、复制 `+0x7C`、`007576C0(unit, 1)`、进入 |
| `0079CA30` | AIMoveToInitialRunState slot 4，也在 UnitAIMoveToCoverState | `aiMoveToInitialRunOnEnter` | 设目标后进入 |
| `0079CA70` | 两张 FollowPath 状态机 slot 3 | `jetAttackMoveFollowPathXfer` | 两次版本字节、`005C0A40`、slot `0x70` 的 `+0x80` |
| `0079CB00` | JetAIMoveToGoalState slot 4 | `jetAIMoveToGoalOnEnter` | 设目标、写航向、复制 `+0x7C`、进入 |
| `0079CE60` | UnitAILandState 与 UnitAITakeOffState slot 3 | `unitAILandXfer` | 两次版本字节、slot `0x70` 的 `+0x38`、slot `0x58` 的 `+0x20` 和 `+0x2C` |
| `0079D030` | UnitAIDeliverToRefineryState slot 5 | `unitAIDeliverToRefineryUpdate` | 按模板步长折算字段；经纪人拒绝则递增并返回 0；到期则推两次并完成 |
| `0079D260` | UnitAIDeliverOreState slot 4，也在 DockWithNode | `unitAIDeliverOreOnEnter` | 子状态 slot `0x38` 传入目标后进入 |
| `0079D320` | UnitAIGetBestBoneState slot 5 | `unitAIGetBestBoneUpdate` | 索引小于 `0x3E8` 则完成；否则写目标并把状态机 `+0x7C` 写成 1，仍返回失败 |
| `0079D5C0` | UnitAIDriftState slot 5 | `unitAIDriftUpdate` | 低于高度界限则移动并返回 0，否则分层后完成 |
| `0079D6A0` | UnitAIFreeFallState slot 5 | `unitAIFreeFallUpdate` | 不低于 `Z-5` 则失败；移动后按帧号返回 0 或完成 |
| `0079D8F0` | SA_AIFaceTargetState slot 5 | `saAIFaceTargetUpdate` | 朝向加到单位角上，然后进入 `007985D0` |
| `0079DA20` | AIAggressiveState slot 4，也在伪装表 | `aiAggressiveOnEnter` | 拷贝单位坐标设目标，loco slot `0x228`，`006AEFE0` 的 ecx 仍是 loco |
| `0079DAC0` | UnitAIAimFireAtObjectState slot 3 | `unitAIAimFireAtObjectXfer` | 两次版本字节后 cdecl `00549E50` |
| `0079EE10` | UnitAIFollowPathState slot 4，另有两张组队表 | `unitAIFollowPathOnEnter` | `005BDCC0`、`0079ECE0` 后进入 |
| `0079F110` | UnitAIFollowWaypointPathAsTeamState slot 4 | `unitAIFollowWaypointPathAsTeamOnEnter` | `0079F030` 返回失败则原样返回；否则子状态 slot `0x48` 参数 1，返回值仍是 `0079F030` 的 EAX |
| `0079F400` | UnitAIGuardAreaState slot 4 | `unitAIGuardAreaOnEnter` | 坐标从状态机拷到栈上；空中检查失败才写航向 |
| `0079F4C0` | UnitAIGuardPositionState slot 4，也在伪装表 | `unitAIGuardPositionOnEnter` | 坐标来自单位，并先把子状态 `+0x7C` 清 0 |
| `0079F580` | UnitAIHoldGroundState slot 4，也在伪装表 | `unitAIHoldGroundOnEnter` | 没有空中检查；`006AEFE0` 的 ecx 重新装成单位 |
| `0079FCD0` | UnitAIMoveToAndDeleteState slot 4，也在撤离表 | `unitAIMoveToAndDeleteOnEnter` | 设目标并写航向，不复制字节 |
| `0079FE80` | UnitAIMoveToAttackPositionState slot 4 | `unitAIMoveToAttackPositionOnEnter` | 子状态 `+0x7C` 清 0，设目标后进入 |
| `0079FFA0` | UnitAIMoveToGoalAndEnterState slot 4 | `unitAIMoveToGoalAndEnterOnEnter` | 目标为空走 `007A008D` 失败；否则设目标、登记乘客并进入 |

## 丢弃

窗口里其余 clean、未分析的函数没有计入。slot 2 的名字 getter、纯析构、EASTL 容器实例化和没有行为的 thunk 不计入。`0079F030` 已在 aistate79b 记为 `unitAIFollowWaypointPathOnEnter`，这里只当作 `0079F110` 的被调桩。账本上的 `0079614D`（2831 字节，ThingTemplate.cpp）和 `00790072`（23644 字节）没有当作本遍的函数入口，也没有引用。`007A008D` 落在下一窗口，不单独计入。

## Oracle

`aistate79d oracle Release cases=109 functions=48 mismatches=0`

把 `kFreeFallBias` 从 `0x40A00000` 改成 `0` 之后，`0079D6A0` 的高度比较变了，移植比原生多走了移动调用（checker 报 stub queue empty）。改回 `0x40A00000` 后仍是 109 例、0 失配。没有 hook。
