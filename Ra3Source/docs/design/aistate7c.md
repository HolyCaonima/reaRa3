# 0x007C0000–0x007CFFFF 尚未归入 AIStateMachine 的 AI 状态

## 范围

`gamelogic7c` 已经合入 `AIStateMachine` 核心和一部分 `0x007C` 状态。本批只取账本里仍是 `unanalyzed` 且 `clean`、入口落在 `0x007C0000–0x007CFFFF` 的函数，一共 200 个。计入 41 个。每个都是 `vtables_split.tsv` 里一张拆分表的槽，槽位 dword 等于函数入口。类名是该表 slot 2 的 `mov eax, imm32; ret`，立即数是 C 字符串。没有 MSVC RTTI。

状态对象用 `this+0x18` 找状态机，状态机 `+0x24` 是单位，单位 `+0x374` 是 locomotor。`dword [0x00DEBA64]` 是 TheGameLogic。继续返回 `0`，完成返回 `0xFFFFFFFF`，失败返回 `0xFFFFFFFE`。`AIHarvestWorkSiteState::onExit`、`AIEnterAndAttackState::onExit`、`AIMoveToPositionAndEnterState::onExit` 是 `ret 4`。

`AIInternalMoveToState` 的 slot 2 字符串被四张表共用，四份 onEnter 不是同一个函数：

- `007c9ea0` 在 `00c31b10` slot 4
- `007c9f40` 在 `00c31b60` slot 4
- `007c9ff0` 在 `00c31bb0` slot 4
- `007ca180` 在 `00c31c00` slot 4

C++ 名字在类名后加上入口，避免撞名。归属按入口计一次。`verify_ownership.py` 对 `work/aistate7c_claims.txt` 的每一条 `vt` 行给出 `fails=0`。

## 函数

| VA | 归属 | C++ | 槽 |
| --- | --- | --- | --- |
| `007c2610` | AIAttackFireDuringApproachState `00c2f8e8` slot 4 | `AIAttackFireDuringApproachState_onEnter` | onEnter |
| `007c29e0` | AIAttackMeleeSquishState `00c2f9c8` slot 4 | `AIAttackMeleeSquishState_onEnter` | onEnter |
| `007c35a0` | AIAttackMoveToState `00c32180` slot 4 | `AIAttackMoveToState_onEnter` | onEnter |
| `007c39e0` | AIFollowWaypointPathExactState `00c2fd20` slot 4 | `AIFollowWaypointPathExactState_onEnter` | onEnter |
| `007c41c0` | AIAimFireState `00c2c910` slot 4 | `AIAimFireState_onEnter` | onEnter |
| `007c4220` | AIAimFireState `00c2c910` slot 5 | `AIAimFireState_update` | update |
| `007c45c0` | AIEnterState `00c2fe00` slot 4 | `AIEnterState_onEnter` | onEnter |
| `007c4700` | AIEnterAndAttackState `00c2fec0` slot 4 | `AIEnterAndAttackState_onEnter` | onEnter |
| `007c4a90` | AIChargeTargetState `00c2ff50` slot 5 | `AIChargeTargetState_update` | update |
| `007c4c30` | AIMoveToPositionAndEnterState `00c301b8` slot 4 | `AIMoveToPositionAndEnterState_onEnter` | onEnter |
| `007c4e00` | AIBurningDeathState `00c30098` slot 4 | `AIBurningDeathState_onEnter` | onEnter |
| `007c4ee0` | AIBurningDeathState `00c30098` slot 5 | `AIBurningDeathState_update` | update |
| `007c77d0` | UnitAIMoveToRallyPointState `00c2dd18` slot 4 | `UnitAIMoveToRallyPointState_onEnter` | onEnter |
| `007c7980` | UnitAIReturnForLandingState `00c2de50` slot 4 | `UnitAIReturnForLandingState_onEnter` | onEnter |
| `007c8450` | SA_AIApproachTargetState `00c30e48` slot 4 | `SA_AIApproachTargetState_onEnter` | onEnter |
| `007c85c0` | UnitAIAttackBlockingObjectState `00c30ee0` slot 4 | `UnitAIAttackBlockingObjectState_onEnter` | onEnter |
| `007c8a10` | UnitAICombinedUnitState `00c2e508` slot 5 | `UnitAICombinedUnitState_update` | update |
| `007c92b0` | UnitAIIsBlockedByWall `00c2b560` slot 4 | `UnitAIIsBlockedByWall_onEnter` | onEnter |
| `007c9370` | AIMoveToAndCollectState `00c31488` slot 4 | `AIMoveToAndCollectState_onEnter` | onEnter |
| `007c9400` | UnitAIMoveToGoalState `00c31668` slot 5 | `UnitAIMoveToGoalState_update` | update |
| `007c9630` | UnitAIReturnToSourceAndEnterState `00c2f1d0` slot 4 | `UnitAIReturnToSourceAndEnterState_onEnter` | onEnter |
| `007c9830` | UnitAIStartMoveState `00c2b8b8` slot 4 | `UnitAIStartMoveState_onEnter` | onEnter |
| `007c9df0` | AIDockApproachState `00c31aa8` slot 4 | `AIDockApproachState_onEnter` | onEnter |
| `007c9ea0` | AIInternalMoveToState `00c31b10` slot 4 | `AIInternalMoveToState_onEnter` | onEnter |
| `007c9f40` | AIInternalMoveToState `00c31b60` slot 4 | `AIInternalMoveToState_onEnter_007c9f40` | onEnter |
| `007c9ff0` | AIInternalMoveToState `00c31bb0` slot 4 | `AIInternalMoveToState_onEnter_007c9ff0` | onEnter |
| `007ca180` | AIInternalMoveToState `00c31c00` slot 4 | `AIInternalMoveToState_onEnter_007ca180` | onEnter |
| `007ca460` | AIGuardMoveToAttack `00c31ca0` slot 4 | `AIGuardMoveToAttack_onEnter` | onEnter |
| `007ca680` | AIGuardReturnState `00c31d70` slot 4 | `AIGuardReturnState_onEnter` | onEnter |
| `007ca7c0` | AIGuardReturnState `00c31d70` slot 5 | `AIGuardReturnState_update` | update |
| `007cb070` | AIHarvestApproachSiteState `00c31e38` slot 4 | `AIHarvestApproachSiteState_onEnter` | onEnter |
| `007cb350` | AIHarvestWorkSiteState `00c2bec8` slot 6 | `AIHarvestWorkSiteState_onExit` | onExit |
| `007cd3b0` | AIMoveAndDeleteState `00c2fc48` slot 5 | `AIMoveAndDeleteState_update` | update |
| `007ce340` | AIEnterAndAttackState `00c2fec0` slot 6 | `AIEnterAndAttackState_onExit` | onExit |
| `007ce520` | AIMoveToPositionAndDieState `00c2ffb8` slot 5 | `AIMoveToPositionAndDieState_update` | update |
| `007ce660` | AIMoveToPositionAndEnterState `00c301b8` slot 6 | `AIMoveToPositionAndEnterState_onExit` | onExit |
| `007cee00` | AITNGuardInnerState `00c2d4e8` slot 4 | `AITNGuardInnerState_onEnter` | onEnter |
| `007cf090` | AITNGuardOuterState `00c2d548` slot 4 | `AITNGuardOuterState_onEnter` | onEnter |
| `007cf190` | AITNGuardOuterState `00c2d548` slot 5 | `AITNGuardOuterState_update` | update |
| `007cf530` | AITNGuardAttackAggressorState `00c2d610` slot 4 | `AITNGuardAttackAggressorState_onEnter` | onEnter |
| `007cf670` | AITNGuardAttackAggressorState `00c2d610` slot 5 | `AITNGuardAttackAggressorState_update` | update |

移植是逐条 thiscall 指令的 C++。`fld` / `fstp` / `fldz` 按 float32 位搬运，`addss` / `subss` / `mulss` 用 SSE 内建。直接调用和虚调用都记进同一条调用记录；虚槽用双方相同的桩地址，这样记录里的目标地址一致。栈上临时指针在两边都写成相对入口 ESP 的 `s+offset`。

`007ca460`（`AIGuardMoveToAttack::onEnter`）在单位标志的某一位为 1 时执行 `mov eax, dword ptr [0]`。oracle 的两条用例都走标志为 0 的分支。

## 未计入

200 个里剩下 159 个。样板（slot 2 名字 getter、slot 0 析构、只转发的 xfer）没有计入，计入集合里的样板比例是 0/41。

slot 2 名字 getter（7）：`007c70a0`、`007c7200`、`007c7360`、`007c88f0`、`007c8ef0`、`007c9110`、`007cc860`。

vtable slot 0 析构或释放路径（18）：`007c3580`、`007c3b30`、`007c5280`、`007c5a30`、`007c5e90`、`007c5fe0`、`007c6830`、`007c70b0`、`007c7210`、`007c7370`、`007c8900`、`007c89e0`、`007c8f00`、`007c9120`、`007ca220`、`007cb420`、`007cc330`、`007cc870`。

没有拆分表槽位，归属立不住（44）：`007c4f60`、`007c5170`、`007c5210`、`007c54e0`、`007c5540`、`007c55b0`、`007c5670`、`007c5770`、`007c5800`、`007c5860`、`007c5a70`、`007c5d30`、`007c5e10`、`007c5f60`、`007c6f80`、`007c7240`、`007c78a0`、`007c7ef0`、`007c8530`、`007c8780`、`007c8e40`、`007c9070`、`007c9180`、`007c91e0`、`007c9c60`、`007c9cc0`、`007c9cf0`、`007c9d60`、`007ca0d0`、`007ca240`、`007cab40`、`007cb440`、`007cb5a0`、`007cc2b0`、`007cc460`、`007cc6c0`、`007cdc70`、`007ce950`、`007cf730`、`007cf7a0`、`007cf850`、`007cfaf0`、`007cfc10`、`007cff00`。

x87 控制字、整数转换或 `comiss`，不在本批能逐位对齐的指令集里（49）。源码路径锚点也在这里，没有拿来充数：`007c3b80` / `007c3cb0` / `007c3dd0` / `007c3f10`（AIStates.cpp），`007ca8c0`（AIGuardRetaliate.cpp），`007cf280`（AITNGuard.cpp）。其余地址：`007c06f0`、`007c08a0`、`007c0f40`、`007c12e0`、`007c1ac0`、`007c1d20`、`007c2250`、`007c2440`、`007c26e0`、`007c2ca0`、`007c2dd0`、`007c2f80`、`007c3740`、`007c4040`、`007c4940`、`007c73a0`、`007c7530`、`007c7a20`、`007c7c30`、`007c7f80`、`007c80e0`、`007c82c0`、`007c8f30`、`007c9730`、`007c98e0`、`007c99a0`、`007ca300`、`007ca530`、`007ca9e0`、`007cac00`、`007caec0`、`007cb110`、`007cb770`、`007ccc30`、`007cce40`、`007cd0d0`、`007cd460`、`007cd890`、`007cdf70`、`007ce860`、`007ceb30`、`007cf360`、`007cfa30`。

有名字或有槽、但留给下一批的真实方法（41）：`007c0550`、`007c1960`、`007c2ac0`、`007c58d0`、`007c5960`、`007c5b90`、`007c5c10`、`007c5ca0`、`007c5e80`、`007c5ea0`、`007c5fd0`、`007c5ff0`、`007c6100`、`007c6190`、`007c6600`、`007c66c0`、`007c6880`、`007c6930`、`007c69b0`、`007c6a40`、`007c6ce0`、`007c6d00`、`007c8ad0`、`007c9520`、`007c9bc0`、`007cb5d0`、`007cb6b0`、`007cba30`、`007cbcd0`、`007cbe80`、`007cc360`、`007cc370`、`007cc380`、`007cc390`、`007cc3a0`、`007cc900`、`007cd0c0`、`007cd730`、`007cde90`、`007ceef0`、`007cf350`。其中 `007c58d0` 到 `007c6930` 以及 `007c9bc0` 的 slot 2 不是 `mov eax, imm32; ret`，类名没有从槽位上读出来。

没有 EASTL 实例，也没有编译器生成的逐成员拷贝被算进这 41 个。

## Oracle

`python Ra3Source/tools/analysis/ports/verify_aistate7c.py` 对 Release 的 `ra3_aistate7ccheck`：

```
aistate7c oracle Release cases=82 functions=41 mismatches=0
```

每个函数两条用例。一条让直接调用和虚调用返回 0，另一条改返回值、栈参数或对象字段，使 EAX、调用记录或对象字节不同。`ret 4` 的三条 onExit 带一个栈参数。Debug 配置也编过 `ra3_aistate7ccheck`，没有新警告。

变异：把 `AIAimFireState::onEnter`（`007c41c0`）在 `007c41df` 的 `je` 取反。原字节在 locomotor 槽返回 0 时继续调用 `007a4890` 和 `007c4040`，EAX 为 0。变异后这条路径直接返回 `0xfffffffe`，调用记录只剩第一次虚调用。oracle 报 `eax 00000000 != fffffffe`，并且调用记录对不上。还原该分支后重新得到 `mismatches=0`。

## Hooks

没有。本批没有 `AIState7CTargets.cpp`，也没有 `record` 键。
