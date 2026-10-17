# 0x00790000–0x0079FFFF AI 状态，第三遍

## 范围

前两遍已经计入 `AIState79` 和 `AIState79B` 各 48 个方法。这一遍只取同一窗口里仍是 clean、`unanalyzed`、并且不在那两份注释表上的函数。计入 47 个。归属全部是 `vtables_split.tsv` 的拆分表槽位，槽位 dword 等于入口。类名来自该表 slot 2 的 `mov eax, imm32; ret`。没有 MSVC RTTI。`work/aistate79c_claims.txt` 经 `verify_ownership.py` 复核，`fails=0`。

布局沿用前两遍：`this+0x18` 是状态机，状态机 `+0x24` 是单位，单位 `+0x374` 是 locomotor。`dword [0x00DEBA64]` 是 TheGameLogic，帧号在 `+0x58`。`dword [0x00BE700C]` 是最大有限浮点，盘上是 `0x7F7FFFFF`。`dword [0x00BD95F8]` 是瞄准阈值，盘上是 `0x43B40000`（360.0f）。SA 查找的注册表指针在 `0x00CC569C`，盘上是 `0x00CC79E0`。返回立即数是继续 `0`、完成 `0xFFFFFFFF`、失败 `0xFFFFFFFE`。`AIBackAwayAndCowerState` 把链接记录 `+4` 和 `0x000F423E` 比较。

读到的源码字符串有两处。`00799530` 在 `00799541` 推 `0x00C2C4B4`，文本是 `AIMoveOntoWallState`。`0079AEE0` 在 `0079AEF4` 推 `0x00C2D08C`，文本是 `AIGuardState::update`。

同一个函数体出现在多张表里时只计一次，名字取下面引用的那张表。`00797810` 也在 `SA_AIUnpackState` `00C2A6B8` slot 5。`0079F0C0` 也在 `00C2E8C8`、`00C30A88`、`00C30FC8` 的 slot 6。`0079AEE0` 也在 `AIGuardObjectState` `00C2D108` slot 5。`0079B080` 也在 `AITunnelNetworkGuardState` `00C2D230` slot 6。`0079AD30` 也在 `00C2D0A8` 和 `00C2D108` 的 slot 6。`0079A210` 也在 `AIAttackPositionFireWeaponState` `00C2CA48` slot 17。`0079DE30` 也在 `00C2E9B8` 和 `00C30EE0` 的 slot 5。`007998F0` 也在 `AIDeadState` `00C2C578` slot 5。`00799780` 还在另外几张 `AIStateMachine` 表的 slot 10。

没有 hook。

## 函数

| VA | 归属 | C++ | 行为 |
| --- | --- | --- | --- |
| `00797210` | UnitAIKillMinerState `00C2A0E8` slot 4 | `unitAIKillMinerOnEnter` | 目标或 `+0x33C` 为空则直接完成；否则 slot `0x64` 非空时 `007483A0(unit, {+0x1C,+0x34,+8})`，返回值被完成值覆盖 |
| `007973D0` | UnitAIWaitForDeliveryState `00C2A348` slot 4 | `unitAIWaitForDeliveryOnEnter` | `0068E1F0` 低字节为 0 则失败；否则唤醒矿工、把 `goal+0xDC` 写入节点 `+0x34`，并把模板 `+0x14` 的位交给 `0070B000` |
| `00797610` | SA_AIFinishState `00C2A538` slot 4 | `saAIFinishOnEnter` | 注册表或模块为空也返回完成；否则先调模块 slot `0x74` |
| `007976D0` | SA_AIPrepareState `00C2A5F8` slot 5 | `saAIPrepareUpdate` | 查找失败返回失败；模块 slot `0x70` 低字节为 0 返回 0，否则完成 |
| `00797710` | 同表 slot 6 | `saAIPrepareOnExit` | 查找失败返回 0；否则返回 `006EB6C0` |
| `00797810` | SA_AIPackState `00C2A598` slot 5 | `saAIPackUpdate` | 查找失败返回失败；slot `0x5C` 低字节为 0 返回完成，非 0 返回 0 |
| `00797F30` | UnitAIDoSpecialPowerState `00C2E6B0` slot 5 | `unitAIDoSpecialPowerUpdate` | 键在 `this+0x20`；查找失败返回 0；模块 slot 4 低字节非 0 返回 0，否则失败 |
| `00798060` | UnitAIAIEvacuateState `00C2B008` slot 5 | `unitAIAIEvacuateUpdate` | 容器为空则失败；loco slot `0x288` 的结果传给容器 slot `0x88`，返回完成 |
| `00798370` | UnitAIHordeForceMembersToExitGoalState `00C2B3C0` slot 6 | `unitAIHordeForceMembersToExitGoalOnExit` | 目标或两次 `006A7600` 任一为空则返回 0；否则容器 slot `0x40` 参数 `{unit, 2}` |
| `007984F0` | UnitAIParachuteToGroundState `00C2F018` slot 19 | `unitAIParachuteToGroundSlot19` | `00416820(this, 0x80)` 为空返回 0；否则 `00798400(分配结果, unit)` |
| `00798760` | UnitAISetGoalPositionToTargetState `00C2B7E0` slot 4 | `unitAISetGoalPositionToTargetOnEnter` | 目标为空则失败；`005AC4F0` 后把目标 `+0x38` 的 12 字节拷到状态机 `+0x38`，返回完成 |
| `00798890` | UnitAISuperState `00C2B9F8` slot 3 | `unitAISuperXfer` | 版本字节 `{1,1}`；mode bit 1 时 slot `0x4C` 写入 `+0x20`；再把该 dword 交给 xfer slot `0x20` |
| `007990A0` | AIHarvestPrepareSiteState `00C2BE60` slot 4 | `aiHarvestPrepareSiteOnEnter` | loco slot `0x1C4` 为空则失败；否则 `+0x20` 写成 slot `0x44` 的结果加上当前帧 |
| `00799530` | AIMoveOntoWallState `00C2C468` slot 5 | `aiMoveOntoWallUpdate` | 子状态为空返回 0；推类名后 slot `0x10`，有符号大于 0 的结果改成 0 |
| `00799780` | AIStateMachine `00C2C4C8` slot 10 | `aiStateMachineSlot10` | `+0x88` 的 slot `0x28` 低字节非 0 则把 AL 置 1；否则尾调 `005AA1A0` |
| `007998F0` | UnitAIDeadState `00C2ACE0` slot 5 | `unitAIDeadUpdate` | 有 loco 则 slot `0x270`；然后 `006D23F0(unit, 1)`，返回 0 |
| `00799AB0` | AIBackAwayAndCowerState `00C2C638` slot 5 | `aiBackAwayAndCowerUpdate` | 子状态为空则失败；链接 `+4` 等于 `0xF423E` 则完成；否则尾调 slot `0x10` |
| `00799CF0` | AIBackAwayState `00C2F6B8` slot 4 | `aiBackAwayOnEnter` | 先清 `+0x4C`；目标和 loco 都在时 `007527C0(loco, 9)`；总是返回失败 |
| `00799D90` | AIAttackMeleeHordeWaitState `00C2C7D0` slot 3 | `aiAttackMeleeHordeWaitXfer` | 版本字节后，mode bit 3 清空时才传 `this+0x20` 给 slot `0x6C` |
| `00799E20` | AIAttackMeleeHordeWaitPathState `00C2C838` slot 3 | `aiAttackMeleeHordeWaitPathXfer` | 同上，并继续把 `this+0x24` 交给 slot `0x70` |
| `00799E70` | AIPickUpCrateState `00C2FAA8` slot 4 | `aiPickUpCrateOnEnter` | 目标为空则失败；否则 `+0x4C=1`，拷目标 `+0x38` 的 12 字节，`+0x50=3` |
| `00799FE0` | AIAttackAimAtTargetState `00C2C8A8` slot 6 | `aiAttackAimAtTargetOnExit` | `+0x21/+0x22` 和 loco 决定是否 slot `0x270`；`0x00BD95F8` 大于模板链 `+0x120` 时 `007527C0`；最后 `006D2020(unit, {0x1B, 0})` |
| `0079A140` | AIAttackPositionAimAtTargetState `00C2C970` slot 6 | `aiAttackPositionAimAtTargetOnExit` | 与上一行相同，字节在 `+0x20/+0x21` |
| `0079A210` | AIAttackFireWeaponState `00C2C9E0` slot 17 | `aiAttackFireWeaponSlot17` | `006A51A0(unit, -1)` 为空则 AL 为 0；`00685EC0` 低字节非 0，或 `00686460` 有符号不小于 0，则把 AL 置 1，高 24 位保留 |
| `0079A5C0` | AIAttackSquadState `00C2CD88` slot 6 | `aiAttackSquadOnExit` | 有 loco 则 `006C0F40(unit, 0)`；清 `unit+0x490`；子状态 slot 0 参数 1 后清 `+0x20` |
| `0079A700` | AIHarvestState `00C2CE40` slot 6 | `aiHarvestOnExit` | 子状态先 slot `0x30` 再 slot 0；把 `+0x25` 写入 `loco+0x434`；返回 loco 指针或 0 |
| `0079AD30` | AIGuardState `00C2D030` slot 6 | `aiGuardOnExit` | 清 `loco+0x444`；子状态 slot 0 后清 `+0x20` |
| `0079AE70` | AIGuardAreaState `00C2D0A8` slot 4 | `aiGuardAreaOnEnter` | 状态机 slot `0x24` 参数 3；loco slot `0x234` 非 0 时用 slot `0x224` 调 `005AC4F0`；子状态 slot `0x14` 为失败则返回，否则 slot `0x1C` |
| `0079AEE0` | 同表 slot 5 | `aiGuardAreaUpdate` | 子状态为空则失败；推 `AIGuardState::update` 后返回 slot `0x10` |
| `0079B080` | AIGuardRetaliateState `00C2D1C8` slot 6 | `aiGuardRetaliateOnExit` | 先拆子状态；无 loco 时返回单位指针，否则 loco slot `0x240` |
| `0079B0C0` | 同表 slot 5 | `aiGuardRetaliateUpdate` | 子状态为空则失败；两个 `005B9F30` 不等时 slot `0x38`；尾调 slot `0x10` |
| `0079B360` | AIRotateFiringarc `00C2D368` slot 6 | `aiRotateFiringarcOnExit` | 无 loco 时返回状态机；否则 `007527C0`、slot `0x270`，并把 `unit+0x44` 的 dword 抄到 `+0x264`，返回单位指针 |
| `0079B5D0` | AITNGuardInnerState `00C2D4E8` slot 6 | `aiTNGuardInnerOnExit` | `+0x2C` 为空则原样返回 EAX；否则把退出参数交给 slot `0x18`，再 slot 0，然后清字段 |
| `0079B610` | AITNGuardOuterState `00C2D548` slot 6 | `aiTNGuardOuterOnExit` | 与上一行相同，字段是 `+0x28` |
| `0079C8A0` | UnitAIHeliPrepareForBoardingState `00C2DA10` slot 6 | `unitAIHeliPrepareForBoardingOnExit` | `006D2020(unit, {0x29, 0})` 后拆子状态 |
| `0079C9F0` | AIMoveToGoalDiveBombState `00C2DAF8` slot 6 | `aiMoveToGoalDiveBombOnExit` | `007576C0(unit, 0)` 后拆子状态 |
| `0079DD50` | UnitAIAimForceFireAtObjectState `00C2E280` slot 3 | `unitAIAimForceFireAtObjectXfer` | 版本字节初值 `{1,2}`；第二字节不小于 2 时再调 `0079DAC0` |
| `0079DDB0` | UnitAIAttackObjectState `00C2E368` slot 4 | `unitAIAttackObjectOnEnter` | slot `0x4C` 写入 `+0x20`；`+0x24==1` 时子状态 slot `0x38`；随后 `005AC4F0`、`006D21F0`、节点 slot `0x28`、`006A5370` 写入 `+0x28`，尾部 slot `0x1C` |
| `0079DE30` | 同表 slot 5 | `unitAIAttackObjectUpdate` | `006A5370` 与 `+0x28` 不等则完成；否则尾调子状态 slot `0x10` |
| `0079DE60` | 同表 slot 6 | `unitAIAttackObjectOnExit` | `006F0440`、`006C0F40`、cdecl `0067D870`；返回值非 0 时其 slot 4；再拆 `+0x20` |
| `0079E080` | UnitAIAttackSquadState `00C2E4A0` slot 4 | `unitAIAttackSquadOnEnter` | slot `0x4C` 得到子状态，状态机 slot `0x44` 写入子状态 `+0x80`，再 slot `0x1C` |
| `0079E210` | UnitAICombinedUnitState `00C2E508` slot 6 | `unitAICombinedUnitOnExit` | 清链接 `+0x40` 的 bit 3；拆子状态 |
| `0079F0C0` | UnitAIFollowWaypointPathState `00C2E850` slot 6 | `unitAIFollowWaypointPathOnExit` | 有 loco 则 `00683590(loco, 0.0f)`；再拆子状态 |
| `0079F240` | UnitAIForceAttackObjectState `00C2E9B8` slot 4 | `unitAIForceAttackObjectOnEnter` | `006D2020(unit, {0x8E, 1})`；loco slot `0x288` 不是 2 时再 `005B9F30` 和 `006C0F40`；然后跳进 `0079DDB0` |
| `0079F880` | UnitAIHordeExitGoalState `00C2EC38` slot 4 | `unitAIHordeExitGoalOnEnter` | 目标为空则失败；slot `0x4C`、slot `0x38`、slot `0x1C` |
| `0079FD20` | UnitAIMoveToGoalState `00C31668` slot 4 | `unitAIMoveToGoalOnEnter` | 目标为空返回失败。模板位 `goal+4` 的 `+0xE0` bit 1、单位模板 `+0xC4` bit 15 与 bit 25 都置上且 `+0xDC` bit 28 清空时走落点分支（slot `0x38` 参数 0、两次 `006A76A0`）；否则 slot `0x38` 直接收目标。`unit+4` 的 `+0xE2` bit 0 清子状态 `+0x7C`。武器 `006A51C0` 后 `006A88C0` 低字节非 0 再 `00683140` |
| `0079FEE0` | UnitAIMoveToGoalState `00C2EF40` slot 4 | `unitAIMoveToGoalCopyOnEnter` | slot `0x4C` 后 `005B2450(child, machine)`，再 slot `0x1C` |

样板 0/47。没有 slot 2 名字 getter、NAMEKEY 缓存、只返回常量的桩、只转发基类的 xfer、同形工厂，也没有单字段访问器。xfer 都带自己的字段或版本字节分支。`00797610` 和 `00797210` 最后固定返回完成，但前面有查找和调用，不算返回常量桩。

## 放弃

- `0079D8F0`（`SA_AIFaceTargetState` `00C2E138` slot 5）在 `0079D928` 跳进 `007985D0`。后者是 163 字节的 SSE/x87 体，这一遍不单独重建，所以整段不计入。
- 同窗口里更大的浮点体留给后面：`0079D030`、`0079C6E0`、`0079C5D0`、`0079D5C0`、`0079D320`、`0079F4C0`、`0079F400`、`0079D6A0`、`00797A10`、`0079B690`、`00799410`、`0079FFA0`、`0079B1C0`、`007970D0`。它们的 x87 或 SSE 指令明显多于这一遍选中的体。
- slot 2 类名 getter、不到 16 字节的返回桩、纯析构和 EASTL 容器实例不计入。`0079DAC0` 只作为 `0079DD50` 的被调函数打桩，没有单独认领。

## Oracle 与变异

Release：`aistate79c oracle Release cases=117 functions=47 mismatches=0`。

语义变异：把 `aiMoveOntoWallUpdate` 的 `value > 0` 改成 `value > 6`。Oracle 在 case 67 `00799530` 报 `eax 00000000 != 00000005`（原生把 slot `0x10` 的 5 夹成 0，变异后的移植保留 5）。恢复比较后重新跑，仍是 117 例 0 差异。

Debug 只要求编译。没有 hook，不跑 `hook_integration`。
