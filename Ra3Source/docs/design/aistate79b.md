# 0x00790000–0x0079FFFF AI 状态，第二遍

## 范围

第一遍 `recon/aistate79` 已经计入 48 个方法，并留下 17 条空状态记录。这一遍只补同一地址段里仍未分析、且不在那份注释表上的 GameLogic AI 状态。计入 48 个。每个都是 `vtables_split.tsv` 里一张拆分表的槽，槽位 dword 等于函数入口。类名取该表 slot 2 的 `mov eax, imm32; ret`。没有 MSVC RTTI。

布局沿用第一遍：`this+0x18` 是状态机，状态机 `+0x24` 是单位，单位 `+0x374` 是 locomotor。`dword [0x00DEBA64]` 是 TheGameLogic，帧号在 `+0x58`。`0079FC40` 读的全局是 `dword [0x00DF03FC]`，checker 用既有的 table 记号同时写它和 `0x00DFC4B4`。`0079CDA0` 的名字键守卫在 `0x00DFBA7C`，键在 `0x00DFBA78`，池在 `0x00E1A078`。SA 查找的注册表指针在 `0x00CC569C`，盘上的值是 `0x00CC79E0`。返回立即数是继续 `0`、完成 `0xFFFFFFFF`、失败 `0xFFFFFFFE`。嵌套状态 id 缺失时用 `0x000F423F`。

读到的源码字符串有三处。`0079E9C0` 推 `0x00C2B140`，以 `FollowWaypointPath.cpp` 结尾。`0079AD70` 推 `0x00C2D08C`，字符串是 `AIGuardState::update`。`0079CDA0` 推 `0x00C18D98`，字符串是 `ReturnToProducerSpecialPower`。

## 函数

| VA | 归属 | C++ | 字节在做什么 |
| --- | --- | --- | --- |
| `00797B80` | UnitAIAimForceFireAtObjectState `00C2E280` slot 4 | `unitAIAimForceFireAtObjectOnEnter` | `006D2020(unit, {0x8E, 1})`；loco slot `0x288` 不是 2 时再 `005B9F30` 和 `006C0F40`；`007527C0(loco, 0xC)` 低字节为 0 则把 `+0x20` 写成完成 |
| `00797C00` | 同表 slot 6 | `unitAIAimForceFireAtObjectOnExit` | `006D2020(unit, {0x8E, 0})`；loco 存在且 `+0x20` 不是完成值时 `007527C0`，再 `006D2020(unit, {0x1B, 0})` |
| `00797660` | SA_AIPackState `00C2A598` slot 4 | `saAIPackOnEnter` | 注册表查找失败返回失败；模块 slot `0x64` 低字节非 0 则 slot `0x68` 并返回 0，否则完成 |
| `00797760` | SA_AITriggerState `00C2A658` slot 4 | `saAITriggerOnEnter` | 查找失败返回完成；`00684BD0` 低字节为 0 返回完成，否则 `00684C00` 后失败 |
| `007977C0` | SA_AIUnpackState `00C2A6B8` slot 4 | `saAIUnpackOnEnter` | 查找失败返回失败；`0069FC10` 低字节为 0 返回完成，否则模块 slot `0x58` 后返回 0 |
| `00797FA0` | UnitAIEndPathMoveState `00C2AE60` slot 4 | `unitAIEndPathMoveOnEnter` | `path+0x80` 加 1；slot `0x48` 低字节非 0 则 `00683140`，slot `0x4C` 低字节非 0 则 `006F19F0`；返回完成 |
| `00798310` | UnitAIHordeForceMembersToExitGoalState `00C2B3C0` slot 4 | `unitAIHordeForceMembersToExitGoalOnEnter` | 单位、目标或容器 slot `0x84` 为空则失败；目标容器非空时 slot `0x40` 参数 `{unit, 1}` |
| `00798560` | UnitAIResetEnterState `00C2B6A0` slot 4 | `unitAIResetEnterOnEnter` | 目标为空直接完成；`006A7620` 得到成员后 slot `0x40` `{unit, 2}`，有 loco 则 `00683140`，并写 `machine+0x7C` |
| `00798F80` | AIGuardRetaliateMachine `00C2BD98` slot 3 | `aiGuardRetaliateMachineXfer` | mode bit 3 时只把 AL 置 1；否则版本字节、`005C0A40`、`0054A090(this, {xfer, this+0x8C})`、slot `0x58` |
| `00799A20` | AIMoveToStateSA `00C2F3E8` slot 19 | `aiMoveToStateSASlot19` | `+0x4D` 非 0 时只改 AL；有目标则拷 12 字节并 `006A72A0`，否则从状态机 `+0x2C` 拷；尾调 `00799920` |
| `00799C30` | AICowerState `00C2C708` slot 6 | `aiCowerOnExit` | `009CCDA0(this, arg)`；单位为空返回该结果；容器或成员为空返回 0，否则尾调成员 slot `0x1A0` |
| `0079A310` | AIWaitUntilFinishedFiringState `00C2CB88` slot 5 | `aiWaitUntilFinishedFiringUpdate` | `006A51A0(unit, -1)` 为空则失败；时间戳为负或帧已到时看 `00688810`，否则返回 0 |
| `0079A520` | AIAttackSquadState `00C2CD88` slot 3 | `aiAttackSquadXfer` | 版本字节和存在标志；标志为 1 且 `+0x20` 仍为 0 时用 `+0x25` 创建；再传 id |
| `0079A620` | AIDockState `00C2CDE8` slot 6 | `aiDockOnExit` | 子状态先 slot `0x30` 再 slot 0；无 loco 时返回状态机指针，否则写 `loco+0x434` 并尾调 `006E6360` |
| `0079A680` | 同表 slot 5 | `aiDockUpdate` | 嵌套 id 不是 5/6/7 时 `005B9F30` 的 ecx 是子状态；两个 `00686FA0` 不等则失败；slot `0x10` 有符号大于 0 则返回 0 |
| `0079A780` | UnitAIHordeEnterGoalState `00C2B358` slot 5 | `unitAIHordeEnterGoalUpdate` | 容器链为空则失败；horde slot `0xF8` 低字节非 0 返回完成，否则 slot `0x100` 后返回 0 |
| `0079A800` | AIHordeExitState `00C2CF00` slot 4 | `aiHordeExitOnEnter` | 单位、目标或容器 slot `0x84` 为空则失败；目标容器 slot `0x40` `{unit, 1}`，有 loco 则 `006E6360(loco, goal)` |
| `0079A870` | 同表 slot 5 | `aiHordeExitUpdate` | loco slot `0x1F8` 返回 2 则返回 0；成员 slot 0 低字节非 0 返回 0，id 为完成则失败；horde slot `0xFC` 与 `006EFE40` 决定是否再走 slot 8 |
| `0079A9B0` | 同表 slot 6 | `aiHordeExitOnExit` | 目标容器非空时 slot `0x40` `{unit, 2}`；有 loco 则 `006E6360(loco, 0)`；尾调状态机 slot `0x38` |
| `0079AAC0` | AIExitState `00C2CF60` slot 4 | `aiExitOnEnter` | 先清 `+0x20`；目标为空则失败；容器非空时 slot `0x40` `{unit, 1}` 并把 `goal+0xDC` 写入 `+0x20` |
| `0079AB10` | 同表 slot 5 | `aiExitUpdate` | 门字节是 `this+0x24`，loco 在目标上；成员 slot 8 之后比较嵌套 id，相等返回完成 |
| `0079AC50` | AIGuardState `00C2D030` slot 3 | `aiGuardXfer` | 同 `0079A520` 的版本/标志/id 序列，创建槽是状态机 slot `0x24`，参数 0 |
| `0079AD70` | 同表 slot 5 | `aiGuardUpdate` | `+0x20` 为空则失败；`00687CB0` 与 `00A780D0` 命中且链接空或 slot `0x20` 低字节非 0 则失败；否则 `005AA240` 后走子状态 slot `0x10` |
| `0079ADF0` | AIGuardAreaState `00C2D0A8` slot 3 | `aiGuardAreaXfer` | 同守卫 xfer，创建参数是 3 |
| `0079AF50` | AIGuardObjectState `00C2D108` slot 3 | `aiGuardObjectXfer` | 早退不改 AL，EAX 是 mode 右移 3；创建参数是 1 |
| `0079AFD0` | 同表 slot 4 | `aiGuardObjectOnEnter` | loco slot `0x22C`/`0x230` 决定创建参数 1 或 2；slot `0x23C` 为 4 则 `00682F20` 并失败 |
| `0079B130` | AIAttackAreaState `00C2D298` slot 3 | `aiAttackAreaXfer` | 同守卫 xfer，创建参数 0，末尾再 xfer `this+0x24`（slot `0x6C`） |
| `0079B720` | AIPickUpCrateState `00C30160` slot 4 | `aiPickUpCrateOnEnter` | `006E5CE0` 为空返回完成；状态机 slot `0x38` 后无目标则失败，否则拷 12 字节并写 `+0x4C=1`、`+0x50=3` |
| `0079C820` | UnitAIHeliEvacuateState `00C2D9A8` slot 3 | `unitAIHeliEvacuateXfer` | 两次版本字节；mode bit 1 时 `this` slot `0x4C` 写入 `+0x20`，再 xfer 该值 |
| `0079CDA0` | UnitAIReturnForLandingState `00C2DE50` slot 6 | `unitAIReturnForLandingOnExit` | `006D2020(unit, {0xAB, 0})`；`006E4760` 的 ecx 是状态机；名字键未初始化时 `00986150`；子状态 slot 0 后清 `+0x20` |
| `0079CFD0` | UnitAIDeliverOreState `00C2DF98` slot 6 | `unitAIDeliverOreOnExit` | 无后续调用时 EAX 是重新装入的状态机；有目标则 `0067E860(node, {0x7D0, unit+0xDC})` |
| `0079D2A0` | UnitAIDockWithNodeState `00C2E000` slot 6 | `unitAIDockWithNodeOnExit` | 同 `0079CFD0`；参数为 1 且 `goal+0x398` 非空时再 `0067E410` |
| `0079D930` | SA_AIPrepareState `00C2A5F8` slot 4 | `saAIPrepareOnEnter` | 两次注册表查找；第二次模块 slot `0x70` 低字节非 0 返回完成，否则 0；任一步失败返回失败 |
| `0079D9B0` | SA_AIValidateApproachTargetState `00C2A718` slot 4 | `saAIValidateApproachTargetOnEnter` | 查找失败返回完成；记录 `+0x2C` bit 30 清则完成，否则 `00718FA0` 低字节为 0 才失败 |
| `0079DEB0` | UnitAIAttackObjectState `00C2E368` slot 3 | `unitAIAttackObjectXfer` | 版本第二字节大于 1 时 `00549C90`；mode bit 1 时 slot `0x4C` 填 `+0x20`，再 xfer 该值 |
| `0079E190` | UnitAICombinedUnitState `00C2E508` slot 4 | `unitAICombinedUnitOnEnter` | 目标为空则 `007000E0` 并失败；否则 slot `0x4C`/`0x38`/`0x1C`，结果不是失败且 `loco+0x214` 非空时把链接 `+0x40` 或上 8 |
| `0079E960` | UnitAIDoSpecialPowerState `00C2E6B0` slot 4 | `unitAIDoSpecialPowerOnEnter` | 保存 `machine+0x70`；记录 `+0x1C` bit 26 清时才 `005B9F30`；始终 `006876D0` 和 `006C0F40`，返回 0 |
| `0079E9C0` | UnitAIEndWaypointPathMoveState `00C2AEC8` slot 4 | `unitAIEndWaypointPathMoveOnEnter` | 表计数大于 0 时 `005A94C0`，参数含 `00C2B140` 和 `0x75`；选中值为 0 则 `006B6E30` 并完成，否则失败 |
| `0079EBA0` | UnitAIEnterGoalState `00C2E718` slot 6 | `unitAIEnterGoalOnExit` | 有 loco 时 `006E6360` 并清 `loco+0x214` 链接的 bit 1；`006D2020(unit, {0x61, 0})`；子状态 slot 0 后清 `+0x20` |
| `0079EC10` | UnitAIExitGoalState `00C2E780` slot 6 | `unitAIExitGoalOnExit` | 目标容器 slot `0x40` `{unit, 2}`，状态机 slot `0x38` 参数 0，子状态 slot 0 后清 `+0x20` |
| `0079EEF0` | FollowWaypointPathStateMachine `00C29A48` slot 3 | `followWaypointPathMachineXfer` | 版本 `{1, 2}`，然后 `005C0A40`、`00798150`、slot `0x44`/`0x64`/`0x84`；第二字节仍不小于 2 时再 `00798150` |
| `0079F030` | UnitAIFollowWaypointPathState `00C2E850` slot 4 | `unitAIFollowWaypointPathOnEnter` | `this` slot `0x4C` 得到路径；`0079EF90` 为 0 则失败；slot `0x50` 返回的 8 字节写入路径 `+0x88` |
| `0079F150` | UnitAIFollowWaypointPathExactState `00C2E940` slot 4 | `unitAIFollowWaypointPathExactOnEnter` | 直接调用 `0079F030`；不是失败则路径 slot `0x48`；`loco+0x214` 为空则失败，否则写 `loco+0x434` 并把链接 `+0x40` 或上 2 |
| `0079F290` | UnitAIForceAttackObjectState `00C2E9B8` slot 6 | `unitAIForceAttackObjectOnExit` | `006D2020`、`006F0440`、`006C0F40`、`0067D870`；后者非 0 时其 slot 4；子状态 slot 0 后清 `+0x20` |
| `0079F610` | UnitAIHordeEnterGoalState `00C2B358` slot 4 | `unitAIHordeEnterGoalOnEnter` | `goal+0x471` bit 0 则失败；成员 slot `0x94` 低字节为 0 则失败；模板 `+0xC0` bit 4 清时 `0077B1F0(goal, 8)` |
| `0079F6F0` | 同表 slot 6 | `unitAIHordeEnterGoalOnExit` | 目标为空返回 0；同一模板位清时 `00706E00(goal, 8)`；容器 slot `0x40` `{unit, 2}` |
| `0079FC40` | UnitAIMoveOnPathState `00C2ED20` slot 3 | `unitAIMoveOnPathXfer` | 两次版本字节；`0x00DF03FC` 非空时对该对象 slot `0x158` 传 `{xfer, this+0x20}` |
| `0079FF10` | UnitAIMoveToGoalState `00C2EF40` slot 6 | `unitAIMoveToGoalOnExit` | 子状态 slot 0 后清 `+0x20`；`unit+0x90` bit 29 则 `006D2020`；无 loco 返回状态机，否则 `006B7040` 再 `00682D70` |

`0079A520`、`0079AC50`、`0079ADF0`、`0079AF50`、`0079B130` 共用同一段 xfer 形状。创建分支只有在 xfer 回调把标志字节改成 1、同时 `+0x20` 仍为 0 时才走到；桩不回写该字节，所以 oracle 覆盖的是 bit 3 早退、标志为 0、以及已有 id 这三条。

## 未计入

- `00797F70`：`UnitAIDoSpecialPowerState` slot 6 只有装载 `+0x20` 然后 `jmp 0x006D25A0`。账本长度把后面的 `int3` 和别的入口算了进去。
- `00797ED0`：`UnitAIDisguiseState` slot 6 先缓存 `SpecialDisguiseUpdate`（`0x00DFBA6C` / `0x00DFBA68`，字符串 `0x00C07C88`），模块非空时 `jmp 0x00729D50`。后者是 `jmp dword ptr [0x018ED5A4]`，栈清理看不见。
- `00799B70`：`AICowerState` onEnter，反汇编记了 2 条 x87。整数 oracle 不携带 ST0。
- `0079B790`、`00799020`：两个 slot 6 都把 `00687C90` 先以 0 个参数调用，非空后再以 1 个参数调用。一个地址只能绑一种清理量。
- x87，整数 oracle 不携带 ST0：`00798230`（`fld dword ptr [0x00BE700C]`）、`0079EE50`（`fldz`）、以及 dump 里 fp=2 的 `0079ACD0`、`0079CBC0`、`0079CD20`、`0079EC70`、`0079F1C0`、`0079F8E0`。fp 更多的 update/onEnter 留在这一段，不计入。
- 第一遍已经 reversed 或标成空状态的入口不再碰。slot 2 类名 getter、纯 `ret` 常量、析构和 `e9` 跳转桩也不计入。

样板占计入集合的 0/48。字段 xfer 会传版本字节和 id，不是只转给基类。

## Hook

无。

## Oracle

`tools/aistate79bcheck` 与 `tools/analysis/ports/verify_aistate79b.py` 对 48 个入口共 124 组输入比较 EAX、对象字节和调用记录。每组开始前恢复 `.data`，避免 `0079CDA0` 的名字键守卫留到下一组。

```
aistate79b oracle Release cases=124 functions=48 mismatches=0
```

把 `unitAIHordeEnterGoalOnEnter` 的目标字节从 `0x471` 改成 `0x472` 后，oracle 报：

```
case 30 0x0079f610 checker: error stub queue empty
```

改回 `0x471` 并重新编译后，再次是 `mismatches=0`。
