# 0x007B0000–0x007BFFFF AI 状态

## 范围

账本在 `0x007B0000–0x007BFFFF` 有 290 个函数，全部是 clean 且原先 `unanalyzed`。本批计入 55 个。每个都是 `vtables_split.tsv` 里一张拆分表的槽：槽位 dword 等于函数入口。类名是该表 slot 2 的 `mov eax, imm32; ret`，立即数是 C 字符串。没有 MSVC RTTI。

状态对象用 `this+0x18` 找状态机，状态机 `+0x24` 是单位，单位 `+0x374` 是 locomotor。`007B64A0` 和 `007B65B0` 从 `dword [0x00DEBA64]` 取逻辑对象，帧号在它的 `+0x58`。距离比较读到的立即数：`0x00BD95B8` 是 `0x43C80000`（400.0），`0x00BD952C` 是 `0x42480000`（50.0）。`comiss` 后的 `jbe` 在左值小于等于右值或无序时跳走。返回立即数是继续 `0`、完成 `0xFFFFFFFF`、失败 `0xFFFFFFFE`。槽位约定与相邻批次一致：slot 2 类名，slot 4 `onEnter`，slot 5 `update`，slot 6 `onExit`。

同一函数体会装进多张表。计数按入口一次，归属取核对过的那一条：

- `007B1150` 也在 `AIDeadState` `00C2C578` slot 6。
- `007B1C10` 也在 `AIMoveAwayFromRepulsorsState` `00C2F580` slot 5。
- `007B62D0` 也在 `AIMoveToPositionAndDieState` `00C2FFB8` slot 6。
- `007B62F0` 也在 `AIAttackMoveToAndEvacuateState` `00C30298` slot 6 和 `AIMoveForBoarding` `00C30310` slot 6。
- `007BD7A0` 也在 `AIInternalMoveToState` `00C31B10` slot 6。四张 `AIInternalMoveToState` 表 `00C31B10`、`00C31B60`、`00C31BB0`、`00C31C00` 的 slot 2 字符串相同，C++ 名用表地址后缀区分。

读过的源码路径：`007B0E20` 推的字符串以 `...\gamelogic\ai\AIStates.cpp:1690` 结尾。`007B1CB0`、`007B54B0`、`007B56F0` 推 `0x00C2C1A8`，字符串以 `...\gamelogic\ai\AIStates.cpp` 结尾。`007BEEF0` 推的字符串以 `...\gamelogic\ai\AISkirmishPlayer.cpp` 结尾。`007B1F30` 推 `0x00C2C7B8`，字符串是 `AIUncontrollableCower`。

## 函数

| VA | 归属 | C++ | 字节在做什么 |
| --- | --- | --- | --- |
| `007B0880` | AIGarrisonedState `00C2C268` slot 4 | `aiGarrisonedOnEnter` | `00416820(0x80)` 成功则 `007A1BD0(unit)`，指针写入 `+0x20`；置 `unit+0xA0` bit 0x15，再虚调用占用者 slot `0x1C` |
| `007B08F0` | 同表 slot 6 | `aiGarrisonedOnExit` | 占用者非空时虚调用 slot 0 参数 1；`EAX` 改成状态机，`+0x20=0`，清同一标志 |
| `007B0940` | 同表 slot 5 | `aiGarrisonedUpdate` | 占用者虚调用 slot `0x10`，否则记下失败；`006EFE40` 沿链表走完后返回先前记下的状态 |
| `007B1150` | UnitAIDeadState `00C2ACE0` slot 6 | `unitAIDeadOnExit` | `EAX` 先是状态机；`unit+0xB2` bit 0 置位才清 `unit+0xB0` bit 0x10 |
| `007B17D0` | AIMoveToState `00C2F388` slot 6 | `aiMoveToOnExit` | `007B1190` 后，locomotor 非空则 `006B7040(loco, {0x00BF21D8, 0})` |
| `007B19F0` | AIMoveToCoverState `00C32468` slot 6 | `aiMoveToCoverOnExit` | 同上，再把 `EAX` 改成状态机；`0x43C80000` 对距离平方 `comiss`/`jbe` 时返回状态机，否则 `00687C30`、`006F6A20` |
| `007B1A80` | AIMoveToStateSA `00C2F3E8` slot 5 | `aiMoveToStateSAUpdate` | `+0x54` 非 0 时帧号大于 `+0x50` 返回失败，相等则 `004D9AB0(unit, 0x8F)` 并返回 0；否则拷贝目标三个 dword 后尾跳 `007B12D0` |
| `007B1AF0` | AIMoveOutOfTheWayState `00C2F448` slot 6 | `aiMoveOutOfTheWayOnExit` | `007B1190`；locomotor 非空时 `006996C0` 并清 `+0x434`、`+0x1E8`、`+0x1EC`；`EAX` 改成状态机后清 `unit+0xB0` bit 0x16 |
| `007B1B70` | AIMoveAndTightenState `00C2F4B0` slot 5 | `aiMoveAndTightenUpdate` | `+0x54`、路径和 `loco+0x428` 都允许时写 `+0x4C=1`、`+0x54=0`，然后尾跳 `007B12D0` |
| `007B1BB0` | AIMoveAwayPanicState `00C2F518` slot 6 | `aiMoveAwayPanicOnExit` | `007B1190`；单位为空返回该结果；locomotor 非空则 `007527C0(loco, 0)`，再清 `unit+0xB4` bit 0x0E |
| `007B1C10` | 同表 slot 5 | `aiMoveAwayPanicUpdate` | 条件成立时从路径节点 `+0x0C` 拷三个 dword，清 `+0x4C` 和 `+0x54`，尾跳 `007B12D0` |
| `007B1C60` | AIMoveAwayFromRepulsorsState `00C2F580` slot 6 | `aiMoveAwayFromRepulsorsOnExit` | `007B1190` 后清 `unit+0xB4` bit 0x0E；单位为空时返回 `007B1190` 的 EAX |
| `007B1E20` | AIFearState `00C2F5F0` slot 6 | `aiFearOnExit` | `007B1190`，清 `unit+0xCC` bit 0x0F；locomotor 非空则 `007527C0(loco, 0)` 并把 `loco+0x43D` 写成 0 |
| `007B1F30` | AIUncontrollableCower `00C2C768` slot 4 | `aiUncontrollableCowerOnEnter` | `006A5F50` 的低字节写入 `+0x20`；`00687500(unit, 0)`、`006D2020(unit, {0x48, 1})`，置 `unit+0xCC` bit 0x14，`005AA240(machine, 0x00C2C7B8)`，返回 `00799B70` |
| `007B1FA0` | 同表 slot 6 | `aiUncontrollableCowerOnExit` | 用 `+0x20` 调 `00687500`，状态 `0x48` 清掉，清 bit 0x14，`005AA250`，返回 `00799C30` |
| `007B2010` | AIMoveAwayAndCowerState `00C2F650` slot 6 | `aiMoveAwayAndCowerOnExit` | `007B1190` 之后 `EAX` 被改成单位再改成 locomotor；任一为空返回 0，否则尾跳 `007527C0(loco, 0)` |
| `007B2050` | 同表 slot 5 | `aiMoveAwayAndCowerUpdate` | 目标或 locomotor 为空返回失败；`+0x54` 为 0 尾跳 `007B12D0`；路径可用时拷三个 dword 并清两个字节，返回 0 |
| `007B20D0` | AIBackAwayState `00C2F6B8` slot 6 | `aiBackAwayOnExit` | `007B1190`；`unit+0xCE` bit 0 置位时掩掉 `unit+0xCC` 的 `0x00010000` 并走标志对 `0x170`；locomotor 非空尾跳 `007527C0` |
| `007B28D0` | AIAttackApproachTargetState `00C2F718` slot 6 | `aiAttackApproachTargetOnExit` | `007B1190` 后 `006D2020(unit, {0x73, 0})`；locomotor 为空则返回该 EAX。否则 `006E6360`、按模板 bit 0x1A 清节点、虚调用 slot `0x274`，近距时 `0078D1E0`，最后虚调用 slot `0x270`，`+0x73=0` |
| `007B2D50` | AIAttackPositionApproachTargetState `00C2F788` slot 6 | `aiAttackPositionApproachTargetOnExit` | 与上一条相同，但没有 `0x73` 状态调用；locomotor 为空时 EAX 保持 `007B1190`，字节写在 `+0x65` |
| `007B3950` | AIAttackPursueTargetState `00C2F878` slot 6 | `aiAttackPursueTargetOnExit` | `007B1190` 后把 `+0x63` 写成 0，EAX 不变 |
| `007B3EF0` | AIAttackMeleeSquishState `00C2F9C8` slot 6 | `aiAttackMeleeSquishOnExit` | `007B1190` 后 `006D2020(unit, {0x1E, 0})` |
| `007B4780` | AIAttackMeleeEngageState `00C2FA38` slot 5 | `aiAttackMeleeEngageUpdate` | `007B42E0(this)` 为 `0xFFFFFFFF` 时把 `this` 低字节改成 1 再 `006D2020(unit, {0x1E, flag})`，返回 `007B42E0` 的 EAX |
| `007B4F90` | AIAttackMoveToState `00C32180` slot 6 | `aiAttackMoveToOnExit` | 清 `unit+0xB0` bit 0x19，虚调用 `+0x58` 的 slot `0x1C`，`007B1190` 后 `EAX` 改成单位；locomotor 非空则喷洒；`unit+0x90` bit 0x0F 置位时再 `006D2020(unit, {0x8F, 0})` |
| `007B52A0` | AIFollowWaypointPathState `00C2FCB0` slot 6 | `aiFollowWaypointPathOnExit` | `007B1190` 后 EAX 变成 locomotor，再变成 `loco+0x214`；节点非空则掩掉 `+0x40` 的 bit 3。空指针返回 0 |
| `007B52D0` | AIFollowWaypointPathStateAndEvacuate `00C324D0` slot 6 | `aiFollowWaypointPathEvacuateOnExit` | 同样清节点，然后无论 locomotor 是否为空都 `007527C0(loco, 0)` |
| `007B5320` | AIFollowWaypointPathExactState `00C2FD20` slot 6 | `aiFollowWaypointPathExactOnExit` | 节点非空时 `006B6E30(loco, +0x50)`，清 `loco+0x434`，掩掉节点 `+0x40` 的 bit 1 |
| `007B5370` | 同表 slot 5 | `aiFollowWaypointPathExactUpdate` | locomotor 非空时把 `+0x434` 写成 1，尾跳 `007B12D0` |
| `007B5460` | AIAttackFollowWaypointPathState `00C321E8` slot 6 | `aiAttackFollowWaypointPathOnExit` | 先虚调用 `+0x6C` 的 slot `0x1C`，再走与 `007B52A0` 相同的节点清除，EAX 是节点或 0 |
| `007B5630` | AIWanderState `00C32258` slot 6 | `aiWanderOnExit` | 清节点后 `EAX` 改成状态机；清 `unit+0xB0` bit 0x13；locomotor 非空尾跳 `007527C0` |
| `007B56B0` | AIWanderInPlaceState `00C2FD90` slot 6 | `aiWanderInPlaceOnExit` | locomotor 为空返回 `007B1190`，否则 `007527C0(loco, 0)` |
| `007B5830` | AIPanicState `00C322B8` slot 6 | `aiPanicOnExit` | 先清 `unit+0xB4` bit 0x0E，再 `007B1190`，返回后者 |
| `007B5E30` | AICombineState `00C2FE60` slot 5 | `aiCombineUpdate` | `007A5B20` 为空则尾跳 `007B12D0`，否则返回那个指针 |
| `007B60D0` | AIChargeTargetState `00C2FF50` slot 6 | `aiChargeTargetOnExit` | 单位或 locomotor 为空返回状态机；否则 `004D9AB0(unit, 0x95)`、`007527C0(loco, 0)`、`machine+0x7B=0`、`007B1190` |
| `007B62D0` | AIMoveAndDeleteState `00C2FC48` slot 6 | `aiMoveAndDeleteOnExit` | `005AA250(machine)` 后 `007B1190` |
| `007B62F0` | AIMoveToAndEvacuateState `00C30228` slot 6 | `aiMoveToAndEvacuateOnExit` | `007B1190`，locomotor 非空则喷洒，然后总是 `007527C0(loco, 0)` |
| `007B64A0` | AIDeployState `00C2D3C8` slot 4 | `aiDeployOnEnter` | locomotor 为空或虚调用 slot `0x1B0` 为 0 返回失败；`unit+0x84` bit 0x1E 置位返回完成；否则帧号加上 `00681840` 写入 `+0x20`，`004D9AF0(unit, 0xC9)`、`006D2020(unit, {0x43, 1})`，返回 0 |
| `007B6530` | 同表 slot 6 | `aiDeployOnExit` | `unit+0xB8` bit 9 置位则清除；`unit+0xC0` bit 9 未置位则置上；然后 `006D2020` 两次，参数是 `{0x43, 0}` 和 `{0x3E, 1}` |
| `007B65B0` | AIUnDeployState `00C2D428` slot 4 | `aiUnDeployOnEnter` | 前两个空检查同部署；bit 0x1E 未置位时 `unit+0x88` bit 3 决定返回 0 或失败；否则两次状态调用，`006818C0` 加帧写入 `+0x20`，`004D9AF0(unit, 0xC7)`，`00757B40(unit, {8, ready+2})`，返回 0 |
| `007B6670` | 同表 slot 6 | `aiUnDeployOnExit` | 两块 `0x3C` 缓冲区 `004D2F98` 清零，第一块 `+0x18` 或上 `0x80`、`+0x20` 或上 `0x200`；`006A5060(unit, buf1, buf2)`，两次 `006D2020`，返回 `006F0A20(unit, 8)` |
| `007B66E0` | AITNGuardMachine `00C2D488` slot 3 | `aiTNGuardMachineXfer` | `[xfer+4]` bit 3 置位时只把 AL 改成 1。否则把参数槽低 16 位改成 `0x0101` 再虚调用 slot `0x18`、`005C0A40`、`0054A090`、slot `0x58`、slot `0x14` |
| `007B9820` | AIStateMachine `00C305E0` slot 18 | `aiStateMachineSlot18` | `007B7B50` 后 `00416820(0x24)`，非空则 `005A9F60(state, machine, 0xA232715B)` 并写入虚表 `00C2DA10`；`005BDB90` 五个参数。第二块是 `0x3C`、id `0x061D6D91`、虚表 `00C30BD0` |
| `007BB4A0` | UnitAILandState `00C30B70` slot 6 | `unitAILandOnExit` | locomotor 为空返回状态机；`007A9AB0`、`007527C0(loco, 5)`，清 `unit+0xB4` bit 6 |
| `007BB8E0` | UnitAIReturnForLandingState `00C2DE50` slot 19 | `unitAIReturnForLandingSlot19` | `ECX` 是单位模板；`template+0xC8` bit 8 选择 `007BB2B0` 或 `007BB640`。`00416820(0x80)` 为空返回 0 |
| `007BB930` | UnitAITakeOffState `00C30BD0` slot 6 | `unitAITakeOffOnExit` | locomotor 为空返回状态机；`006D2020(unit, {0x19, 0})`、`007A9AB0`、`006E4760`。查询结果非空且 `info+0x62` 为 0 时虚调用 slot `0x18`，再清 `unit+0xB4` bit 5 |
| `007BC320` | AIFollowExitProductionPathState `00C31210` slot 19 | `aiFollowExitProductionPathSlot19` | `00416820(0x84)` 为空返回 0；`007AB5B0(created, unit)` 后把虚表写成 `00C335E8`（slot 2 是 `FollowPathStateMachine`），返回新对象 |
| `007BC4C0` | AIMoveToAndCollectState `00C31488` slot 6 | `aiMoveToAndCollectOnExit` | `005B9F30` 非空时 `00748590` 填向量，每个元素虚调用 slot `0x64`，`begin` 非空则 `004166B0`。`007B1190` 后 `EAX` 是单位，locomotor 非空则改成喷洒结果 |
| `007BD770` | AIInternalMoveToState `00C31B10` slot 5 | `aiInternalMoveTo31B10Update` | 目标为空，或 `+0x80` / `+0x471` 的 bit 0 任一置位，返回失败；两 bit 都清才尾跳 `007B12D0` |
| `007BD7A0` | AIDockApproachState `00C31AA8` slot 6 | `aiDockApproachOnExit` | 目标被挡或 `00687810` 为空则 `007B1190`。`reason!=1` 且 slot `0x38` 低字节非 0 时走 slot `0x20`，否则 slot `0x34`；两条都再 `007B1190` |
| `007BD830` | AIInternalMoveToState `00C31B60` slot 5 | `aiInternalMoveTo31B60Update` | 被挡或码头为空返回失败；否则虚调用 slot `0x14`，参数是单位和 `this+0x20`，尾跳 `007B12D0` |
| `007BD880` | 同表 slot 6 | `aiInternalMoveTo31B60OnExit` | 与 `007BD7A0` 相反：先看 slot `0x38`，再看 `reason!=1`。成功走 slot `0x24`，否则 slot `0x34`，最后都是 `007B1190` |
| `007BD910` | AIInternalMoveToState `00C31BB0` slot 5 | `aiInternalMoveTo31BB0Update` | 被挡、码头为空或 slot `0x38` 低字节为 0 返回失败；否则 slot `0x18` 两个参数，尾跳 `007B12D0` |
| `007BDA40` | AIInternalMoveToState `00C31C00` slot 5 | `aiInternalMoveTo31C00Update` | 被挡或码头为空返回失败；否则 slot `0x1C` 两个参数，尾跳 `007B12D0` |
| `007BDA90` | 同表 slot 6 | `aiInternalMoveTo31C00OnExit` | 码头可用时虚调用 slot `0x2C`；`005AA250`；locomotor 非空则 `006E6360(loco, 0)`；返回 `007B1190` |
| `007BDEF0` | AIGuardReturnState `00C31D08` slot 5 | `aiGuardReturnUpdate` | `006A6210(unit)` 非 0 返回失败，否则尾跳 `007B12D0` |

## 未计入

- slot 2 类名 getter，例如 `007B4F30`（`mov eax, 0x00C321D0; ret`，`AIAttackMoveToState`）。账本里这一类有 31 个只出现在 slot 2 的入口，另有一批 6 字节 `mov eax, imm32; ret` 桩。
- 纯尾跳：`007B1AE0`（`jmp 007B12D0`）、`007B2D40`、`007B36A0`、`007B3940`、`007B3B30`、`007B3EE0`、`007BDF20`（`jmp 007B1190`）。
- 删除析构。`007BA200`、`007BA3B0`、`007BA580`、`007BA7C0`、`007BAAC0`、`007BAC40`、`007BAD90`、`007BAF30`、`007BB040`、`007BB150`、`007BB280`、`007BB440`、`007BB5E0`、`007BB8B0`、`007BBDE0`、`007BBFF0` 都是写回基类虚表、`005B2370`，再按标志 `004166B0`。`007B3F10` 先 `007A3D00`，`007B4BB0` 先 `007A3F30`，`007BC5D0` 先 `007AD260`，然后按标志 `004166B0`。`007B5030` 写虚表 `00C2D6D8` 后 `00788640`，再按标志删除。`007B6880`（Squad）释放 `+0x1C`，`007B7410` 释放 `+0xA4`，`007BE360`（WorkOrder）释放 `+0x20`，用的都是 `004166B0`。
- `operator new` 工厂：`007BA5B0` 先 `00416820(0x80)` 再构造。
- x87：54 个。其中包括 `007B1CB0`（AIFearState slot 5）、`007B54B0`（AIWanderState slot 5）、`007B56F0`（AIPanicState slot 5）。整数 oracle 不携带 ST0。
- 重函数：38 个。`007B0E20`（AIMoveOntoWallState slot 3，257 字节，推 `AIStates.cpp:1690`）和 `007BEEF0`（AISkirmishPlayer slot 4，611 字节，推 `AISkirmishPlayer.cpp`）在其中。slot 2 不是类名 getter 的虚表没有计入。

样板占计入集合的 0/55。没有 slot 2 getter、NAMEKEY 缓存或返回常量桩。`007B66E0` 会改 xfer 指针并调用多条虚函数，不是转发给基类。

## Hook

无。

## Oracle

`tools/aistate7bcheck` 与 `tools/analysis/ports/verify_aistate7b.py` 对 55 个入口共 116 组输入比较 EAX、对象字节和调用记录。

```
aistate7b oracle Release cases=116 functions=55 mismatches=0
```

把 `aiMoveToStateSAUpdate` 的比较从 `frame > [this+0x50]` 改成 `>=` 后，相等的那一组（帧号 20，截止 20）被 oracle 抓住：

```
case 13 0x007b1a80 eax 00000000 != fffffffe; calls 004d9ab0:@2+0:0000008f != .
```

改回 `>` 并重新编译后，再次是 `mismatches=0`。
