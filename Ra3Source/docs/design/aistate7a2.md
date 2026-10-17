# GameLogic AI 状态第二批 `0x007A0000–0x007AFFFF`

## 范围

这一批接在已合并的 `recon/aistate7a` 之后，只重建该区间里仍然 clean、unanalyzed、且不在 `aistate7a_annotations.tsv` 中的函数。共 43 个有行为的函数：命名状态的 `onEnter` / `update` / `onExit` / `xfer`，以及 `AIPlayer` 的 slot 25。共享体只计一次，名字取自被引用类的 slot 2 字符串。没有 hook。

槽位沿用第一批：slot 3 `xfer`，slot 4 `onEnter`，slot 5 `update`，slot 6 `onExit`。`AIPlayer` / `AISkirmishPlayer` 的 slot 25 是 `offerTeam`。每个 vtable 的 slot 2 都是 `mov eax, imm32; ret`，立即数是类名 C 字符串。`verify_ownership.py` 对 `work/aistate7a2_claims.txt` 的 54 条 `vt` 记录打印 `fails=0`。

对象布局沿用 `AIState7A`：`self+0x18` 是 machine，`machine+0x24` 是 owner，`machine+0x54` 是目标标量，`owner+4` 是 template，`owner+0x374` 是 loco。镜像里的绝对量：`0x00BE700C` 为 `0x7F7FFFFF`，`0x00BD9628` 为 `0xBF800000`，`0x00CC561C` 为 `0x00CC785C`。`007A5400` 在 template `+0xCC` 的 bit 10 置位时执行 `mov edx, [0]`，再 `call [edx]`，this 为 0，两个参数是 `machine+0x2C` 和 0。失败码是 `0xFFFFFFFE`，`-1` 是 `0xFFFFFFFF`。

## 计数函数

| VA | 大小 | 归属 | C++ |
| --- | ---: | --- | --- |
| `007A0F30` | 235 | AIGuardIdleState `00C2BC78` slot 5 | `AIGuardIdleState_update` |
| `007A1CB0` | 160 | AIGarrisonedState `00C2C268` slot 3 | `AIGarrisonedState_xfer` |
| `007A1FC0` | 233 | AIInternalMoveToState `00C2D6D8` slot 3；同体还有 `00C2F388` `00C2F448` `00C30310` `00C31488` `00C31B10` `00C31B60` `00C31BB0` `00C31C00` `00C32468` slot 3 | `AIInternalMoveToState_xfer` |
| `007A28D0` | 116 | AIBackAwayState `00C2F6B8` slot 3 | `AIBackAwayState_xfer` |
| `007A2A20` | 209 | AIAttackApproachTargetState `00C2F718` slot 3 | `AIAttackApproachTargetState_xfer` |
| `007A2BB0` | 167 | AIAttackPositionApproachTargetState `00C2F788` slot 3 | `AIAttackPositionApproachTargetState_xfer` |
| `007A2CF0` | 115 | AIAttackMeleeHordeApproachTargetState `00C2F800` slot 3 | `AIAttackMeleeHordeApproachTargetState_xfer` |
| `007A34D0` | 163 | AIAttackPursueTargetState `00C2F878` slot 3 | `AIAttackPursueTargetState_xfer` |
| `007A3620` | 125 | AIAttackFireDuringApproachState `00C2F8E8` slot 3 | `AIAttackFireDuringApproachState_xfer` |
| `007A3AD0` | 112 | AIAttackMeleeSquishState `00C2F9C8` slot 3 | `AIAttackMeleeSquishState_xfer` |
| `007A3C00` | 251 | AIAttackMeleeEngageState `00C2FA38` slot 3 | `AIAttackMeleeEngageState_xfer` |
| `007A3E20` | 115 | AIFollowPathState `00C2FB10` slot 3 | `AIFollowPathState_xfer` |
| `007A3F90` | 210 | AIFollowPathAsTeamState `00C2FB78` slot 3 | `AIFollowPathAsTeamState_xfer` |
| `007A4200` | 132 | AIAttackMoveToState `00C32180` slot 3 | `AIAttackMoveToState_xfer` |
| `007A44D0` | 244 | AIFollowWaypointPathState `00C2FCB0` slot 3；AIFollowWaypointPathStateAndEvacuate `00C324D0` slot 3 | `AIFollowWaypointPathState_xfer` |
| `007A4650` | 121 | AIFollowWaypointPathExactState `00C2FD20` slot 3 | `AIFollowWaypointPathExactState_xfer` |
| `007A5400` | 257 | AIAttackPositionFireWeaponState `00C2CA48` slot 4 | `AIAttackPositionFireWeaponState_onEnter` |
| `007A5510` | 121 | 同上 slot 5 | `AIAttackPositionFireWeaponState_update` |
| `007A55B0` | 133 | AIWaitUntilFinishedFiringState `00C2CB88` slot 4 | `AIWaitUntilFinishedFiringState_onEnter` |
| `007A5D00` | 215 | AIHordeEnterState `00C2CEA0` slot 4 | `AIHordeEnterState_onEnter` |
| `007A6020` | 118 | AIGoingIdleState `00C2D168` slot 4 | `AIGoingIdleState_onEnter` |
| `007A6140` | 97 | AITunnelNetworkGuardState `00C2D230` slot 5 | `AITunnelNetworkGuardState_update` |
| `007A6390` | 108 | AIChargeTargetState `00C2FF50` slot 3 | `AIChargeTargetState_xfer` |
| `007A6530` | 114 | AIPrepareForBoarding `00C2D300` slot 4 | `AIPrepareForBoarding_onEnter` |
| `007A68F0` | 120 | AITNGuardReturnState `00C33840` slot 3 | `AITNGuardReturnState_xfer` |
| `007A9220` | 136 | JetAIAttackMoveToGoalState `00C2DBD0` slot 4 | `JetAIAttackMoveToGoalState_onEnter` |
| `007A93F0` | 213 | JetAILandingApproachState `00C2DC40` slot 4 | `JetAILandingApproachState_onEnter` |
| `007A94E0` | 151 | UnitAILandState `00C30B70` slot 5 | `UnitAILandState_update` |
| `007A9580` | 137 | UnitAIParkOrientState `00C2DDE8` slot 4 | `UnitAIParkOrientState_onEnter` |
| `007A9610` | 124 | 同上 slot 6 | `UnitAIParkOrientState_onExit` |
| `007A9690` | 103 | UnitAIReloadAmmoState `00C29ED8` slot 4 | `UnitAIReloadAmmoState_onEnter` |
| `007A98B0` | 92 | UnitAITakeOffState `00C30BD0` slot 5 | `UnitAITakeOffState_update` |
| `007AAA40` | 123 | UnitAIAttackMoveToGoalState `00C2E2F8` slot 4 | `UnitAIAttackMoveToGoalState_onEnter` |
| `007AAAC0` | 134 | UnitAIAttackPositionState `00C2E3D0` slot 4 | `UnitAIAttackPositionState_onEnter` |
| `007AABD0` | 184 | UnitAICombinedUnitIdleState `00C2ABA8` slot 4 | `UnitAICombinedUnitIdleState_onEnter` |
| `007AB170` | 182 | UnitAIEnterGoalState `00C2E718` slot 4 | `UnitAIEnterGoalState_onEnter` |
| `007AB4F0` | 114 | AIFollowExitProductionPathState `00C31210` slot 4 | `AIFollowExitProductionPathState_onEnter` |
| `007ABEC0` | 323 | UnitAIMoveOnPathState `00C2ED20` slot 6 | `UnitAIMoveOnPathState_onExit` |
| `007AC1F0` | 103 | UnitAIMoveOutOfTheWayState `00C2ED88` slot 4 | `UnitAIMoveOutOfTheWayState_onEnter` |
| `007ACF90` | 123 | UnitAIParachuteToGroundState `00C2F018` slot 4 | `UnitAIParachuteToGroundState_onEnter` |
| `007AD4C0` | 163 | AI `00C31A50` slot 3 | `AI_xfer` |
| `007AEBF0` | 265 | AIHarvestWorkSiteState `00C2BEC8` slot 4 | `AIHarvestWorkSiteState_onEnter` |
| `007AF6F0` | 119 | AIPlayer `00C336A0` slot 25；AISkirmishPlayer `00C33718` slot 25 | `AIPlayer_offerTeam` |

样板（slot 2 类名 getter、NAMEKEY 缓存、返回常数、只转发到基类的 xfer、第 6 个起的同形工厂）为 0/43。每个 xfer 都在版本字节之后还有自己的字段、门控或分配；状态函数都有分支。

## 行为要点

- `007A5400` 先发 code `0x55`。目标为空，或 template `+0xC0` bit 4 清且两次探测都失败且 `006FFB00` 非零，返回 `0xFFFFFFFE`。bit 10 走地址 0 上的间接调用。`007A0B30` 的 AL 为 0 同样失败。`00688810` 非零返回 `0xFFFFFFFF`。owner `+0x84` bit 8 且游戏字节 `+0x58` 的 bit 0 置位时把 `self[0x24]` 写成 1。
- `007A5510` 在目标丢失、owner `+0x471` bit 0、或 owner `+0x88` bit `0x15` 时失败。`self[0x24]` 非零则清掉并发 code `0x0E`。忙碌值 1 保持，其他非零失败，0 则发 code `0x1D` 并把子状态 slot 0 的返回值与 `0xFFFFFFFF` 按位或。
- `007A0F30` 找到目标且 `006A4F60` 的 AL 非零时，动画顶精灵为 1 或 loco slot `0x244` 不是 1 就返回 `0xFFFFFFFF`。loco slot `0x288` 是 cdecl，压入的 0 留作 `006906A0` 的第 4 个参数；该调用返回 3 则失败。`self[0x20]` 为 0 时写成 1，并可能把机器转到一个栈上的位置。
- `007A5D00` 要求 owner、goal、phys 都存在，且 goal `+0x471` bit 0 清、phys slot `0x84` 非零。另一具身体必须存在、它的 slot `0x84` 为 0、slot `0x94`（参数 owner, 1, 0）的 AL 非零。然后武器 slot `0x7C` 的参数是 `goal+0x38, 2, goal`。
- `007A6530` 先放位置再取 goal。未放下返回 `0xFFFFFFFF`，没有 goal 返回 `0xFFFFFFFE`，适配失败返回 `0xFFFFFFFF`，适配成功返回 `0xFFFFFFFE`。
- `007A6140`：`self+0x20` 为空返回 `0xFFFFFFFE`。`006D21B0` 的 AL 非零且 template `+0xC0` bit `0x1A` 清也失败。否则调用 `005AA240(machine, 0x00C2FF28)`，返回子状态 slot `0x10`，再调用 `005AA250`。
- `007A9610` 在收尾时把 EAX 改成 loco；loco 为空则 EAX 是重新取出的 owner。`movss` 把 `0x00BE700C` 写入 `loco+0x1F0`，`loco+0x21C == 3` 时清零。
- `007ABEC0` 的返回值是最后一次调用的 EAX。宽单位（owner `+0x90` bit `0x1D`）跳过 `004D9AB0` 和 `006B7040`。`0x00DF03FC` 非空且 `self+0x20 >= 5` 时调用其 slot `0x6C` 并把相位写成 1。
- `007AEBF0` 的跨度比较是有符号的 `jge`。site 为空，或记录 `+0x38` 非零，返回 `0xFFFFFFFE`。
- `007AF6F0` 扫 `self+0x88` 的链表。命中时返回节点指针并清 AL。接受则 AL 为 1。
- `007AD4C0` 在 xfer `+4` bit 2 置位时走链表：节点载荷非零才构造文本（`004CB690` 参数 `0x00C31A98, 0x0E`）并 xfer。循环后总是调用 `004CA930`。另一条路径是版本 `{1,1}`、slot `0x6C` 写 `self+0x18`、slot `0x14` 参数为 `self+0x1C, 4`。
- 带 `007A1FC0` 的 xfer 在 `call 007A1FC0` 之后用 `mov eax, [xfer+4]; shr eax, 3; test al, 1` 做门。门打开时函数返回的是移位后的 EAX，不是 `007A1FC0` 的返回值。`007A4650` 在 bit 1 清时返回 `xfer+4 >> 1`。`007A2BB0` 的版本字节是 `{1, 2}`，高字节小于 2 时不再写 `self+0x67`，返回值是前一个 slot `0x84`。`007A68F0` 在 bit 3 置位时返回 `xfer+4 >> 3`，否则再做一次版本调用并写 `self+0x58`。`007A1CB0` 把标志字节交给 slot `0x84`，桩可以改写它；标志非零且 `self+0x20` 仍为空时分配 `0x80` 并调用 `007A1BD0`，存的是该调用的返回值。`007A3C00` 在 `self+0x50` 为空时分配 `0x28`，成功则调用 `005A9F60(block, machine, 0xAAEC3BFD)` 并把块的虚表写成 `0x00C2C9E0`。

调用 `007A1FC0` 的 xfer 在测试调用方时把 `007A1FC0` 当作桩，测试 `007A1FC0` 自己时跑真实函数体。

## 放下的候选

- `007A8F60`、`007A87F0`、`007A8A40`：位置计算，x87 操作过多。
- `007A4FD0`、`007A4DD0`：体很大，调用和浮点混在一起。
- `007AFDB0`：TeamInQueue xfer，带分配循环。
- `007A8400`、`007AED50`、`007AAEF0`、`007A18C0`：工厂、字符串/INI、bitset、玩家 slot 20，不是这条状态机路径。
- `007A3730`、`007A8640`、`007AB310`、`007AEA10`、`007A2E50`、`007A2FC0`、`007A9760`、`007A31F0`、`007A33F0`、`007A3110`、`007A0D20`：浮点密集的进入/更新。
- `007A7A00`、`007A7A30`：留给 `recon/vclass`。
- EASTL、纯析构和没有行为的 thunk 不计数。

## Oracle 与变异

Release `ra3_aistate7a2check` 对 160 个用例（每个计数函数至少两条，含空目标、门控、分配失败和版本字节）与 Unicorn 比较 kind、log、mem。

```
cases=160 mismatches=0
```

语义变异：把 `AITunnelNetworkGuardState_update` 的空指针判断从 `self+0x20` 改成 `self+0x24`。Oracle 报 `mismatches=3`（`tng 0 1 0` 的 log，`tng 0 1 1` 和 `tng 0 0 0` 的 kind）。改回之后再次 `cases=160 mismatches=0`。

Debug 配置也编译了 `ra3_aistate7a2check`，没有新警告。没有 hook。
