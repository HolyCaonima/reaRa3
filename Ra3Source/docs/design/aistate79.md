# 0x00790000–0x0079FFFF AI 状态

## 范围

账本在 `0x00790000–0x0079FFFF` 有 600 个函数，其中 591 个是 clean 且原先 `unanalyzed`。本批计入 48 个。每个都是 `vtables_split.tsv` 里一张拆分表的槽：槽位 dword 等于函数入口。类名是该表 slot 2 的 `mov eax, imm32; ret`，立即数是 C 字符串。没有 MSVC RTTI。`UnitAIPArkedState` 的拼写以 slot 2 字符串为准。

状态对象用 `this+0x18` 找状态机，状态机 `+0x24` 是单位，单位 `+0x374` 是 locomotor。`dword [0x00DEBA64]` 是 TheGameLogic，帧号在它的 `+0x58`。`dword [0x00DFC4B4]` 是逻辑表，半径 float 在 `+0x80`。比较用到的立即数：`0x00BD95B8` 是 `0x43C80000`，`0x00BE700C` 是 `0x7F7FFFFF`，`0x00BD90A4` 是 `0x3F800000`。返回立即数是继续 `0`、完成 `0xFFFFFFFF`、失败 `0xFFFFFFFE`。

同一函数体会装进多张表。计数按入口一次，归属取 `work/aistate79_claims.txt` 里的那一条：

- `00797AC0` / `00797B00` 也在 `UnitAIAimFireAtObjectState` `00C2E208` 的 slot 4 / slot 6。
- `007983D0` 也在 `UnitAIInCoverState` `00C2B500` slot 4。
- `00798690` 也在 `SA_AIFaceTargetState` `00C2E138` slot 6。
- `00797D60` 也在 `UnitAICreateAttackPathState` `00C2E570` slot 6。
- `00798900` 也在 `UnitAIMoveToDeliverState` `00C2E068`、`UnitAIMoveToExtractState` `00C2E0D0`、`UnitAIExtractState` `00C30DE8` 的 slot 5。
- `0079B470` 也在 `AIDeployState` `00C2D3C8` 和 `AIUnDeployState` `00C2D428` 的 slot 5。
- `0079C490` 也在另一张 `AIFaceState` `00C2F248` slot 8。
- `0079CFA0` 同时是 `00C2DF30` 的 slot 4 和 slot 5。
- `00798880` 是多张状态表的 slot 15。计入的归属是 `UnitAISuperState` `00C2B9F8` slot 15。

读过的源码路径只有两处。`0079797D` 推 `0x00C2A8C8`，字符串以 `\Ra3\code\SageEngine\source\GameEngine\gamelogic\ai\States\TurretStates\Idle.cpp` 结尾。`007998B2` 推 `0x00C2C1A8`，字符串以 `...\gamelogic\ai\AIStates.cpp` 结尾。`00797468` 的字节是 `5E C3`，所以 `00797440` 的 `pop esi; ret` 落在账本长度之后的那个字节上，下一条入口是 `00797470`。

## 函数

| VA | 归属 | C++ | 字节在做什么 |
| --- | --- | --- | --- |
| `00796F20` | FollowWaypointPathStateMachine `00C29A48` slot 16 | `followWaypointPathStoreTolerance` | 把栈上的 dword 写到 `+0x90`，EAX 原样返回 |
| `00796F50` | 同表 slot 18 | `followWaypointPathStoreFlag` | 把参数低字节写到 `+0x94`，只改 AL |
| `007970A0` | UnitAIPArkedState `00C2DD88` slot 6 | `unitAIParkedOnExit` | `006F1990(unit)` 之后 `009CCDA0(this, arg)` |
| `0079CC20` | 同表 slot 4 | `unitAIParkedOnEnter` | `006D2840(unit)` 然后 `006A4EA0(unit)`，返回 0 |
| `007971E0` | UnitAIGetBestBoneState `00C2A080` slot 6 | `unitAIGetBestBoneOnExit` | `006D2020(unit, {0x9D, 1})` |
| `00797380` | UnitAIUpdateNodeState `00C2A2E0` slot 4 | `unitAIUpdateNodeOnEnter` | 目标对象为空返回失败，否则 `0067E860` 后返回完成 |
| `00797440` | UnitAIWaitForDeliveryState `00C2A348` slot 5 | `unitAIWaitForDeliveryUpdate` | `0067E350` 低字节非 0 返回完成，否则 `006C6D40` 后返回 0 |
| `00797470` | 同表 slot 6 | `unitAIWaitForDeliveryOnExit` | 节点非空时 `0067E340(node, {0,0})`，再 `006C6D40` |
| `007974D0` | UnitAIWaitForMinerState `00C2A410` slot 5 | `unitAIWaitForMinerUpdate` | 节点在 `unit+0x398`；`0067E3E0` 低字节为 0 返回 0，否则 `0067E3D0` 后返回完成 |
| `00797960` | TurretAIIdleState `00C29B20` slot 4 | `turretAIIdleOnEnter` | 帧号加 `005A94C0`，参数含 `00C2A8C8` 和 `0x30`，结果写入 `+0x20` |
| `007979A0` | 同表 slot 5 | `turretAIIdleUpdate` | `info+0x4C` 或 `+0x50` 为 0，或帧号仍小于 `+0x20`，返回 0，否则完成 |
| `00797AC0` | UnitAIAimFireAtPositionState `00C2AA10` slot 4 | `unitAIAimFireAtPositionOnEnter` | 保存 `loco+0x218`，`007527C0(loco, 0xC)` 失败则写成 `0xFFFFFFFF`，再 `006A4EA0` |
| `00797B00` | 同表 slot 6 | `unitAIAimFireAtPositionOnExit` | locomotor 存在且 `+0x20` 不是完成值时 `007527C0`，然后 `006D2020(unit, {0x1B, 0})` |
| `00797C50` | UnitAIAttackObjectState `00C2E368` slot 19 | `unitAIAttackObjectSlot19` | `005B9F30` 取目标，再 `00682C20(loco, {unit, +0x24, goal})` |
| `00797C80` | UnitAIAttackPositionState `00C2E3D0` slot 19 | `unitAIAttackPositionSlot19` | `00682C40(loco, unit)` |
| `007981F0` | UnitAIForceAttackObjectState `00C2E9B8` slot 19 | `unitAIForceAttackObjectSlot19` | `00682C30(loco, {unit, goal})` |
| `00797D60` | UnitAICreatePathState `00C2AC10` slot 6 | `unitAICreatePathOnExit` | locomotor 非空时把 `0x3F800000` 写到 `loco+0x18C`；EAX 是 locomotor 指针 |
| `00797DD0` | UnitAIDeleteState `00C2AD40` slot 4 | `unitAIDeleteOnEnter` | `004EDD70(gameLogic, unit)`，返回完成 |
| `007987C0` | UnitAISetLocomotorState `00C2B850` slot 4 | `unitAISetLocomotorOnEnter` | `007527C0(loco, +0x20)`，返回完成 |
| `007982D0` | UnitAIHoldFireState `00C2B2F8` slot 4 | `unitAIHoldFireOnEnter` | `006AEFE0(unit, {unit, 0, 1})`，返回 0 |
| `007983D0` | UnitAIIdleState `00C2B4A0` slot 4 | `unitAIIdleOnEnter` | `006A4EA0(unit)`，返回 0 |
| `00798690` | UnitAIRotateToAngleState `00C2B708` slot 6 | `unitAIRotateToAngleOnExit` | 无 locomotor 时返回单位指针；否则写 `0x7F7FFFFF` 到 `+0x1F0`，模式 3 时再写 0 |
| `00798880` | UnitAISuperState `00C2B9F8` slot 15 | `unitAISuperStateSlot15` | `+0x20` 为空返回进入时的 EAX，否则尾跳 `005AA1E0` |
| `007988F0` | UnitAITimedIdleState `00C2BA60` slot 4 | `unitAITimedIdleOnEnter` | 把 `+0x20` 复制到 `+0x24` |
| `00798900` | 同表 slot 5 | `unitAITimedIdleUpdate` | `+0x24` 减 1，减到 0 返回完成 |
| `007989E0` | UnitAIWaitForOccupantsState `00C2BAC8` slot 6 | `unitAIWaitForOccupantsOnExit` | `006D2020(unit, {0xA8, 0})` 后 `009CCDA0(this, arg)` |
| `00798CF0` | AIDockWaitForClearanceState `00C2BBA8` slot 4 | `aiDockWaitForClearanceOnEnter` | 把当前帧写到 `+0x20` |
| `00798DC0` | AIDockProcessDockState `00C2BC10` slot 6 | `aiDockProcessDockOnExit` | `005AA250(machine)` |
| `00798F00` | AIGuardRetaliateMachine `00C2BD98` slot 18 | `aiGuardRetaliateMachineSlot18` | 子记录为空或其 `+4` 非 0 时清 AL，否则 AL=1 |
| `00799890` | AIIdleState `00C2C520` slot 4 | `aiIdleOnEnter` | `+0x22=0x000F`，`+0x25=1`；无 locomotor 时 `+0x20=0`，否则用 `00C2C1A8` 和 `0x9F8` 走 `006835B0`、`005A94C0` |
| `00799920` | AIInternalMoveToState `00C2D6D8` slot 19 | `aiInternalMoveToSlot19` | `007525F0` 拷贝向量，记下 `loco+0x428`；标志为 0 时抄帧和三个 dword |
| `0079A0A0` | AIAimFireState `00C2C910` slot 6 | `aiAimFireOnExit` | `006D2020(unit, {0x1B, 0})` |
| `0079A0C0` | 同表 slot 19 | `aiAimFireSlot19` | `+0x21` 为 0 返回 0，否则尾跳 `005B9F30` |
| `0079A3D0` | AIAttackState `00C2CCE0` slot 17 | `aiAttackStateSlot17` | `+0x24` 为空则清 AL，否则尾跳 `005AA200` |
| `0079A4F0` | AIAttackSquadState `00C2CD88` slot 17 | `aiAttackSquadStateSlot17` | 同上，嵌套指针在 `+0x20` |
| `0079B470` | TurretAIHoldTurretState `00C29AB8` slot 5 | `turretAIHoldTurretUpdate` | 帧号仍小于 `+0x20` 返回 0，否则 `0xFFFFFFFF` |
| `0079C490` | AIFaceState `00C2D950` slot 8 | `aiFaceStateSlot8` | `+0x28==2` 返回 1，否则 0 |
| `0079C520` | TurretStateMachine `00C29C50` slot 13 | `turretStateMachineSlot13` | `005C0840`；`+0x80` 非空时把帧写到链接 `+0x80` 并返回 game-logic 指针 |
| `0079C550` | 同表 slot 6 | `turretStateMachineOnExit` | `005C08C0`，链接非空时同样盖帧，EAX 保持 helper 的结果 |
| `0079C580` | 同表 slot 7 | `turretStateMachineSlot7` | 当前状态为空时旧 id 用 `0x000F423F`；id 变化才盖帧 |
| `0079C8D0` | UnitAICheckFarEnoughState `00C29D20` slot 4 | `unitAICheckFarEnoughOnEnter` | 距离平方不大于半径平方时失败 |
| `0079CCF0` | UnitAIParkOrientState `00C2DDE8` slot 5 | `unitAIParkOrientUpdate` | `unit+0x471` bit 0 或 `0079CC50` 低字节为 0 则失败，否则尾跳 `007985D0` |
| `0079CFA0` | UnitAIWaitForClearanceState `00C2DF30` slot 4 | `unitAIWaitForClearanceOnEnter` | `unit+0x94` 的 bit 11 决定完成或 0 |
| `0079E0C0` | UnitAIAttemptSkipMoveState `00C2AA80` slot 4 | `unitAIAttemptSkipMoveOnEnter` | `005BA110` 取点；半径平方不大于距离则失败，否则 `006D2020(unit, {0x32, 0})` |
| `0079E150` | UnitAICanEnterState `00C2AB48` slot 5 | `unitAICanEnterUpdate` | 单位或目标为空，或 `006EFEB0(unit)` 不等于目标，返回失败 |
| `0079E4D0` | UnitAICreatePathExactState `00C2AC78` slot 4 | `unitAICreatePathExactOnEnter` | 记录 `+0x80` 为 0 返回失败，否则 `006B69E0(loco, {id, &xy})` |
| `0079EA60` | UnitAIEnterCoverState `00C2AFA0` slot 4 | `unitAIEnterCoverOnEnter` | `0x43C80000` 与距离平方做 `comiss`/`jbe`（上限不平方）；然后 `00687C30`、`006F6A20` |
| `0079F3D0` | UnitAIGarrisonedIdleState `00C2B1B8` slot 5 | `unitAIGarrisonedIdleUpdate` | `006EFEB0` 非空时 `0079F330(this, found)`，返回 0 |

## 未计入

- slot 2 类名 getter，例如 `00796EB0`（`mov eax, 0x00C29900; ret`）。
- 返回常量的共享桩：`00797DB0`（`or eax, 0xffffffff; ret`）、`00799B60`（`mov al, 1; ret`）。
- 析构：`00796EC0`、`007984C0`、`0079C4F0`、`0079E010`、`0079E8E0`、`0079F810`。它们调用 `005B2370`，再按条件调用 `004166B0`。
- `operator new` 工厂：`0079E050`、`0079E920`、`0079F850`，先 `00416820` 再调用范围内的构造。
- 纯尾跳：`0079A1C0` 是 `mov ecx, [ecx+0x18]; jmp 005B9F30`。
- x87：`00796F40`（`fld dword ptr [ecx+0x90]; ret`）、`00797570`、`007978D0`（`fnstcw`/`fld`）、`0079DB10`（`fld`/`fstp` 拷贝目标 float）。整数 oracle 不携带 ST0。

样板占计入集合的 0/48。

## Hook

无。

## Oracle

`tools/aistate79check` 与 `tools/analysis/ports/verify_aistate79.py` 对 48 个入口共 119 组输入比较 EAX、对象字节和调用记录。

```
aistate79 oracle Release cases=119 functions=48 mismatches=0
```

把 `turretAIHoldTurretUpdate` 的截止偏移从 `0x20` 改成 `0x24` 后，oracle 报：

```
case 79 0x0079b470 eax 00000000 != ffffffff
```

改回 `0x20` 并重新编译后，再次是 `mismatches=0`。
