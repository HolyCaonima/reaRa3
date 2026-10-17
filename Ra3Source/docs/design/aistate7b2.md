# 0x007B0000 第二批 AI 状态

## 范围

这一批补第一批没带走的 `0x007B0000–0x007BFFFF` 行为。计入 46 个函数。类名仍是拆分虚表 slot 2 的 `mov eax, imm32; ret`，立即数是 C 字符串。没有 MSVC RTTI。

状态布局和第一批相同：`this+0x18` 是状态机，状态机 `+0x24` 是单位，单位 `+0x374` 是 locomotor。炮塔状态读状态机 `+0x80`。返回值是继续 `0`、完成 `0xFFFFFFFF`、失败 `0xFFFFFFFE`。`007B1190` 被本批的退出函数直接调用，C++ 走已移植的函数体，不再打桩。

`007B0E20` 失败路径推 `0x00C32110`，字符串是 `...\gamelogic\ai\AIStates.cpp:1690`，下一条消息 `0x00C320B0` 是 `m_moveOntoMachine is NULL in AIMoveOntoWallState::DoXfer, Game crash or DSYNC is likely!!`。

## 共享的类名 getter

`00C30480`、`00C30530`、`00C30638`、`00C32808` 的 slot 2 都是 `00799570`，字符串是 `AIStateMachine`。`007B98D0` 同时是 `00C30638` 和 `00C32808` 的 slot 18。`007B8E70` 只在 `00C30480`，`007B9710` 只在 `00C30530`，注释里写明它们用的是同一个 getter。

`007B1190` 装在两张表上，getter 不是同一个：`00C2D6D8` slot 6 的 getter `0079C430` 返回 `AIInternalMoveToState`，`00C2F4B0` slot 6 的 getter `007A2510` 返回 `AIMoveAndTightenState`。

## 函数

| VA | 归属 | C++ | 字节在做什么 |
| --- | --- | --- | --- |
| `007B0D50` | `007B0E20` @ `007B0E89`，也从 `007B0F30` @ `007B0F52` 调入 | `moveOntoWallMachineCtor` | 写 `00C2C400` MoveOntoWallStateMachine，再 `new` 子状态并 `005BDB90` 装进机器 |
| `007B0E20` | AIMoveOntoWallState `00C2C468` slot 3 | `aiMoveOntoWallXfer` | 版本字节 `1,1`；标志非 0 且 `+0x20` 为空时 `new 0x80` 再调 `007B0D50(unit, 0x3D424C5A)`；否则走 `0040EBF0` 断言 |
| `007B0F30` | 同表 slot 4 | `aiMoveOntoWallOnEnter` | `new 0x80` 后 `007B0D50(unit, 0xB4CC9815)`，虚调用机器 slot `0x38` 和 slot `0x14`，中间 `005AC4F0` |
| `007B1090` | AIDeadState `00C2C578` slot 4 | `aiDeadOnEnter` | 单位为空返回 0；否则两块 0x3C 缓冲、六次 `006C3500`/`006888A0`，再 `006A5060` 和 `00804300` |
| `007B1190` | 见上，两张表的 slot 6 | `aiMoveAndTightenOnExit` | 按单位标志清 bit，`[0x00DF03FC]` 非空且 `+0x44 >= 5` 时虚调用 slot `0x6C` 并把字段写成 1；locomotor 非空则清节点字节 |
| `007B36B0` | AIAttackMeleeHordeApproachTargetState `00C2F800` slot 6 | `aiAttackMeleeHordeApproachOnExit` | 先 `007B1190`；`unit+0x80` bit 0 置位则返回；否则 `006D5570`，locomotor 上 `comiss` 50.0 与距离平方，`jbe` 不成立才 `0078D1E0` |
| `007B47B0` | AIAttackMeleeEngageState `00C2FA38` slot 6 | `aiAttackMeleeEngageOnExit` | `unit+0x80` bit 0 置位时 EAX 已是状态机（入口 `mov eax, [this+0x18]`）；否则可能 `00682CE0`、`006D5570`、`006D2020`，最后 `007B1190` |
| `007B4820` | AIFollowPathState `00C2FB10` slot 6 | `aiFollowPathOnExit` | `007B1190` 后清 locomotor `+0x434`、路径 bit 3、`+0x1E4=-1`；模板 `+0xCC` bit 0xA 清时写状态 `0x95`；第二次 `00686CB0` 留下的 float 由 `006A4500` 弹出 |
| `007B4BD0` | AIFollowPathAsTeamState `00C2FB78` slot 6 | `aiFollowPathAsTeamOnExit` | `+0x60` 非空则虚调用 slot `0x1C`；`007B1190`；locomotor 非空时 EAX 改成 `00682CE0` 的返回值 |
| `007B4E30` | AIMoveAndEvacuateState `00C2FBE0` slot 6 | `aiMoveAndEvacuateOnExit` | `005AA250`，再 `005AC4F0(machine, this+0x50, 0x7F7FFFFF)`，返回 `007B1190` |
| `007B5D20` | AIAttackState `00C2CCE0` slot 6 | `aiAttackOnExit` | `+0x24` 非空则虚调用 slot 0 参数 1 并清掉；清若干状态和 `unit+0xA8` 的低 3 位；locomotor 非空再 `006C0F40`、`00682D90` |
| `007B6380` | AIMoveForBoarding `00C30310` slot 5 | `aiMoveForBoardingUpdate` | 先 `007B1810`；`loco+0x150` 为 0 返回它的 EAX；否则 `0063BF20` 后 cdecl `00639FB0`，AL 为 0 返回 `0xFFFFFFFF` |
| `007B6840` | `007BC050` @ `007BC0C8` | `squadCtor` | 写 `00C32318` Squad 并清一块字段，返回 this |
| `007B8E70` | AIStateMachine `00C30480` slot 18 | `aiStateMachine30480Slot18` | 按固定 id 链 `new`、构造、`005BDB90` 装子状态 |
| `007B9710` | AIStateMachine `00C30530` slot 18 | `aiStateMachine30530Slot18` | 同上，另一张表的安装序列 |
| `007B98D0` | AIStateMachine `00C30638` 与 `00C32808` slot 18 | `aiStateMachine30638Slot18` | 账本把函数切在 `007BA000`，`ret` 在 `007BA17B`/`007BA18E`；C++ 覆盖到 `007BA18F` |
| `007BA190` | `007BA230` @ `007BA24D` | `disguiseAggressiveMachineCtor` | 写 `00C32B20` 并安装子状态 |
| `007BA230` | UnitAIDisguiseAggressiveState `00C30690` slot 19 | `unitAIDisguiseAggressiveSlot19` | `new 0x80`，空则 0，否则把单位交给 `007BA190` |
| `007BA260` | `007BA3E0` @ `007BA3FD` | `heliEvacuateMachineCtor` | 写 `00C32B88` 并安装子状态 |
| `007BA3E0` | UnitAIHeliEvacuateState `00C2D9A8` slot 19 | `unitAIHeliEvacuateSlot19` | `new 0x80` 后调 `007BA260` |
| `007BA7F0` | `007BA940` @ `007BA95D` | `jetAttackMoveFollowPathMachineCtor` | 写 `00C32D30` 并安装子状态 |
| `007BA940` | JetAIAttackMoveFollowPathAsTeamState `00C30A10` slot 19 | `jetAIAttackMoveFollowPathSlot19` | `new 0x84` 后调 `007BA7F0` |
| `007BA970` | `007BAAF0` @ `007BAB0D` | `jetAttackMoveFollowWaypointMachineCtor` | 写 `00C32DE0` 并安装子状态 |
| `007BAAF0` | JetAIAttackMoveFollowWaypointPathState `00C30A88` slot 19 | `jetAIAttackMoveFollowWaypointSlot19` | `new 0x98` 后调 `007BA970` |
| `007BAB20` | `007BAC70` @ `007BAC8D` | `jetAttackMoveToGoalMachineCtor` | 写 `00C32E80` 并安装子状态 |
| `007BAC70` | JetAIAttackMoveToGoalState `00C2DBD0` slot 19 | `jetAIAttackMoveToGoalSlot19` | `new 0x80` 后调 `007BAB20` |
| `007BB2B0` | `007BB470` @ `007BB48D` | `jetMoveToGoalMachineCtor` | 写 `00C33208` 并安装子状态 |
| `007BB470` | JetAIMoveToGoalState `00C2DCB0` slot 19 | `jetAIMoveToGoalSlot19` | `new 0x80` 后调 `007BB2B0` |
| `007BB500` | `007BB610` @ `007BB62D` | `moveToRallyPointMachineCtor` | 写 `00C33270` 并安装子状态 |
| `007BB610` | UnitAIMoveToRallyPointState `00C2DD18` slot 19 | `unitAIMoveToRallyPointSlot19` | `new 0x80` 后调 `007BB500` |
| `007BBBD0` | TurretAIIdleScanState `00C29B80` slot 5 | `turretAIIdleScanUpdate` | 炮塔为空时 `lea eax, [esi-2]` 得到 `0xFFFFFFFE`；bit 9 置位返回 `0xFFFFFFFF`；否则 `007B6C60` 和 `0079BB20` 的低字节都非 0 才返回完成 |
| `007BBC80` | TurretAIRecenterTurretState `00C29BE8` slot 5 | `turretAIRecenterUpdate` | `+0x20` 为 0 时角度取 `0x00BD90A4`（1.0），否则取炮塔信息 `+0x30`；两个调用的低字节都非 0 返回完成 |
| `007BBD20` | `007BBE10` @ `007BBE2D` | `attackMoveFollowPathMachineCtor` | 写 `00C333D0` 并安装子状态 |
| `007BBE10` | UnitAIAttackMoveFollowPathAsTeamState `00C30F50` slot 19 | `unitAIAttackMoveFollowPathSlot19` | `new 0x84` 后调 `007BBD20` |
| `007BBE40` | `007BBF00` @ `007BBF1D` | `attackMoveFollowWaypointMachineCtor` | 写 `00C33478` 并安装子状态 |
| `007BBF00` | UnitAIAttackMoveFollowWaypointPathState `00C30FC8` slot 19 | `unitAIAttackMoveFollowWaypointSlot19` | `new 0x98` 后调 `007BBE40` |
| `007BBF30` | `007BC020` @ `007BC03D` | `attackMoveToGoalMachineCtor` | 写 `00C33518` 并安装子状态 |
| `007BC020` | UnitAIAttackMoveToGoalState `00C2E2F8` slot 19 | `unitAIAttackMoveToGoalSlot19` | `new 0x80` 后调 `007BBF30` |
| `007BC050` | AttackSquadStateMachine `00C2E440` slot 3 | `attackSquadMachineXfer` | 版本字节 `1,2`；`005C0A40` 之后版本低于 2 就返回；需要时 `new 0x30` 调 `007B6840` |
| `007BC100` | `007BC1C0` @ `007BC1DD` | `combinedUnitMachineCtor` | 写 `00C33580` 并安装子状态 |
| `007BC1C0` | UnitAICombinedUnitState `00C2E508` slot 19 | `unitAICombinedUnitSlot19` | `new 0x80` 后调 `007BC100` |
| `007BD6B0` | AIDockApproachState `00C31AA8` slot 5 | `aiDockApproachUpdate` | 目标为空或两个死亡位任一置位返回失败；`007B12D0` 不是 `0xFFFFFFFF` 则原样返回；否则再取一次目标和对接器，`fsqrt` 后 `fcompi`/`ja`，距离大于 50.0 返回状态减 1 |
| `007BD980` | AIInternalMoveToState `00C31BB0` slot 6 | `aiInternalMoveTo31BB0OnExit` | 目标活着且对接器在时，按退出原因和 slot `0x38` 的低字节选 slot `0x28` 或 `0x34`；然后 `005AA250`、`006F0A20(unit, 3)`，返回 `007B1190` |
| `007BDDE0` | AIGroup `00C33664` slot 3 | `aiGroupXfer` | 先虚调用 slot `0x18`；`[xfer+4]>>2` 的 bit 0 为 0 时 EAX 就是这个移位结果。置位则沿 `this+4` 的环（哨兵是 `this+4` 自己）走 `0054A090`，再按序虚调用 |
| `007BDEC0` | AIGuardMoveToAttack `00C31CA0` slot 6 | `aiGuardMoveToAttackOnExit` | `007B1190` 之后 EAX 改成 `005AC4F0(machine, this+0x50, [this+0x5C])` |
| `007BEE80` | AISkirmishPlayer `00C33718` slot 12 | `aiSkirmishPlayerSlot12` | `[0x00DEBA64]+0x58` 的帧号小于 `this+0xC0` 就跳过；否则把帧号加上 `fistp` 截断的乘积（乘 `[0x00DEEEE8]`，盘上是 0，再乘 `0x00BD9534` 的 5.0）并调 `007BEBD0`。返回值始终是 `this+0xC4` |

## 样板

11 个 slot 19 都是 `new`、判空、从 `this+0x18` 的状态机取 `+0x24`、再把单位交给对应构造。前 5 个算进行为，后 6 个（`007BB470`、`007BB610`、`007BBE10`、`007BBF00`、`007BC020`、`007BC1C0`）算样板。6/46，约 13%。分配大小不是都一样：`007BA940` 和 `007BBE10` 推 `0x84`，`007BAAF0` 和 `007BBF00` 推 `0x98`，其余推 `0x80`。

## 没计入的

- 30 个 `mov eax, imm32; ret` 类名 getter。
- 7 个 5 字节尾跳：`007B1AE0`、`007B2D40`、`007B36A0`、`007B3940`、`007B3B30`、`007B3EE0`、`007BDF20`。
- 含 `fpatan`（`D9 F3`）的 `007B12D0`、`007B2130`、`007B2E10`、`007B3270`、`007B6B30`、`007BF4A0`。`007B6C60` 也有这条指令，只作为炮塔更新的被调用者打桩，不计入。`007B12D0` 和 `007B1810`、`007BEBD0` 同样只打桩。
- 本批选择的函数里没有 EASTL 实例化，也没有编译器生成的拷贝。

## 预言机

`aistate7b2 oracle Release cases=97 functions=46 mismatches=0`

变异：把 `007B1190` 里 `+0x44 >= 5` 改成 `> 5`。`+0x44` 为 5 的用例失败：原生仍虚调用并写回 1，移植侧跳过调用，EAX 停在 5，`obj0+0x44` 也不一致。改回后重新构建，`mismatches=0`。

没有钩子。
