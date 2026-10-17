# GameLogic AI states `0x007A0000–0x007AFFFF`

## Scope

The range holds 513 clean unanalyzed ledger functions. This batch reconstructs 47 of them: `onEnter`, `update`, `onExit`, and `xfer` slots of named `State` subclasses, plus `AIStateMachine` slot 8. Slot numbers are the split-table index from `vtables_split.tsv`. The dword at `vtable + 4 * slot` is the counted VA. Slot 2 of each cited vtable is `mov eax, imm32; ret`, and that immediate is the class-name C string. Shared bodies are one C++ function, named for the class used in the annotation, with every extra vtable listed below.

RA3 state slots used here: slot 3 `xfer`, slot 4 `onEnter`, slot 5 `update`, slot 6 `onExit`. `AIStateMachine` slot 8 allocates 0x80 bytes and calls `0x007A57A0`.

`.rdata` dword `0x00BE700C` is `ff ff 7f 7f` (`0x7F7FFFFF`). `0x007A58A0` pushes `0x71E396C1` at `0x007A58BC`.

## Counted functions

| VA | Size | Ownership | Function |
| --- | ---: | --- | --- |
| `007A00A0` | 100 | UnitAIMoveToGoalAndEnterState `00C2EFA8` slot 6 | `UnitAIMoveToGoalAndEnterState_onExit` |
| `007A0120` | 81 | UnitAIParachuteToGroundState `00C2F018` slot 6 | `UnitAIParachuteToGroundState_onExit` |
| `007A03F0` | 67 | UnitAIPrepareForBoardingState `00C2F160` slot 4; UnitAIHeliPrepareForBoardingState `00C2DA10` slot 4 | `UnitAIPrepareForBoardingState_onEnter` |
| `007A0440` | 113 | UnitAIPrepareForBoardingState `00C2F160` slot 6 | `UnitAIPrepareForBoardingState_onExit` |
| `007A04D0` | 30 | UnitAIRotateToAngleState `00C2B708` slot 4 | `UnitAIRotateToAngleState_onEnter` |
| `007A0C50` | 111 | AIDockWaitForClearanceState `00C2BBA8` slot 5 | `AIDockWaitForClearanceState_update` |
| `007A0CC0` | 88 | AIFaceState `00C2F248` slot 6; AIDockWaitForClearanceState `00C2BBA8` slot 6 | `AIFaceState_onExit` |
| `007A0E30` | 91 | AIDockProcessDockState `00C2BC10` slot 4 | `AIDockProcessDockState_onEnter` |
| `007A1090` | 95 | AIGuardGoingIdleState `00C2BCD8` slot 4 | `AIGuardGoingIdleState_onEnter` |
| `007A2340` | 86 | AIMoveToStateSA `00C2F3E8` slot 3 | `AIMoveToStateSA_xfer` |
| `007A2520` | 76 | AIMoveAndTightenState `00C2F4B0` slot 3; AIMoveAwayPanicState `00C2F518` slot 3; AIMoveAwayFromRepulsorsState `00C2F580` slot 3; AIFearState `00C2F5F0` slot 3 | `AIMoveAndTightenState_xfer` |
| `007A32F0` | 82 | AIAttackMeleeHordeWaitState `00C2C7D0` slot 6; AIAttackMeleeHordeWaitPathState `00C2C838` slot 6 | `AIAttackMeleeHordeWaitState_onExit` |
| `007A3DD0` | 73 | AIPickUpCrateState `00C2FAA8` slot 3 and `00C30160` slot 3 | `AIPickUpCrateState_xfer` |
| `007A41C0` | 60 | AIMoveAndEvacuateState `00C2FBE0` slot 3 | `AIMoveAndEvacuateState_xfer` |
| `007A4310` | 63 | AIMoveAndDeleteState `00C2FC48` slot 3; AIMoveToPositionAndDieState `00C2FFB8` slot 3 | `AIMoveAndDeleteState_xfer` |
| `007A46D0` | 70 | AIAttackFollowWaypointPathState `00C321E8` slot 3 | `AIAttackFollowWaypointPathState_xfer` |
| `007A4810` | 73 | AIWanderState `00C32258` slot 3; AIPanicState `00C322B8` slot 3 | `AIWanderState_xfer` |
| `007A5590` | 31 | AIWaitUntilMembersFinishedBeforeExitState `00C2CB10` slot 4 | `AIWaitUntilMembersFinishedBeforeExitState_onEnter` |
| `007A5640` | 76 | AIStartAttackObjState `00C2CBF8` slot 4 | `AIStartAttackObjState_onEnter` |
| `007A58A0` | 51 | AIStateMachine `00C2C4C8` slot 8, also `00C30378` `00C303D0` `00C30428` `00C30480` `00C304D8` `00C30530` `00C30588` `00C305E0` `00C30638` `00C32620` `00C32678` `00C326D0` `00C32808` `00C32940` slot 8 | `AIStateMachine_createLinkedState` |
| `007A5A50` | 73 | AIEnterState `00C2FE00` slot 3 | `AIEnterState_xfer` |
| `007A5CB0` | 70 | AIEnterAndAttackState `00C2FEC0` slot 3 | `AIEnterAndAttackState_xfer` |
| `007A5DE0` | 97 | AIHordeEnterState `00C2CEA0` slot 6 | `AIHordeEnterState_onExit` |
| `007A6480` | 60 | AIMoveToPositionAndEnterState `00C301B8` slot 3 | `AIMoveToPositionAndEnterState_xfer` |
| `007A65B0` | 62 | AIRotateFiringarc `00C2D368` slot 4 | `AIRotateFiringarc_onEnter` |
| `007A85E0` | 49 | UnitAIDisguiseHoldFireState `00C30700` slot 4 | `UnitAIDisguiseHoldFireState_onEnter` |
| `007A92B0` | 82 | JetAIAttackMoveToGoalState `00C2DBD0` slot 6; UnitAIAttackMoveToGoalState `00C2E2F8` slot 6 | `JetAIAttackMoveToGoalState_onExit` |
| `007A9700` | 67 | UnitAIReloadAmmoState `00C29ED8` slot 6 | `UnitAIReloadAmmoState_onExit` |
| `007A9A30` | 127 | UnitAITaxiFromHangerState `00C2DEC0` slot 6 | `UnitAITaxiFromHangerState_onExit` |
| `007AA080` | 75 | UnitAIDeliverToRefineryState `00C29FA8` slot 6 | `UnitAIDeliverToRefineryState_onExit` |
| `007AA4D0` | 53 | UnitAIDriftState `00C2A478` slot 4 | `UnitAIDriftState_onEnter` |
| `007AAB50` | 119 | UnitAIAttackPositionState `00C2E3D0` slot 6 | `UnitAIAttackPositionState_onExit` |
| `007AAE40` | 19 | UnitAICombinedUnitMoveToState `00C31048` slot 5 | `UnitAICombinedUnitMoveToState_update` |
| `007AAE60` | 42 | UnitAICombinedUnitMoveToState `00C31048` slot 6 | `UnitAICombinedUnitMoveToState_onExit` |
| `007AAE90` | 69 | UnitAICreatePathState `00C2AC10` slot 4 | `UnitAICreatePathState_onEnter` |
| `007AAFF0` | 61 | UnitAIDepartBeach `00C2ADA0` slot 4 | `UnitAIDepartBeach_onEnter` |
| `007AB570` | 60 | AIFollowExitProductionPathState `00C31210` slot 6 | `AIFollowExitProductionPathState_onExit` |
| `007ABA00` | 79 | UnitAIGarrisonedState `00C2EA28` slot 6 | `UnitAIGarrisonedState_onExit` |
| `007ABA50` | 75 | UnitAIGarrisonedIdleState `00C2B1B8` slot 4 | `UnitAIGarrisonedIdleState_onEnter` |
| `007ABBF0` | 53 | UnitAIHordeGarrisonedState `00C2B438` slot 4 | `UnitAIHordeGarrisonedState_onEnter` |
| `007ABC30` | 53 | UnitAIHordeGarrisonedState `00C2B438` slot 6 | `UnitAIHordeGarrisonedState_onExit` |
| `007AC260` | 116 | UnitAIMoveOutOfTheWayState `00C2ED88` slot 6 | `UnitAIMoveOutOfTheWayState_onExit` |
| `007AC810` | 69 | UnitAIMoveToGoalState `00C31668` slot 6 | `UnitAIMoveToGoalState_onExit` |
| `007AD1C0` | 72 | UnitAIMoveToDeliverState `00C2E068` slot 4; UnitAIMoveToExtractState `00C2E0D0` slot 4; UnitAIExtractState `00C30DE8` slot 4 | `UnitAIMoveToDeliverState_onEnter` |
| `007AD210` | 80 | same three vtables, slot 6 | `UnitAIMoveToDeliverState_onExit` |
| `007AE9D0` | 63 | AIHarvestApproachSiteState `00C31E38` slot 3 | `AIHarvestApproachSiteState_xfer` |
| `007AEBB0` | 56 | AIHarvestPrepareSiteState `00C2BE60` slot 6 | `AIHarvestPrepareSiteState_onExit` |

## Instruction evidence

`007A04D0` compares `machine+0x54` with `ucomiss` against `0x00BE700C`, then `lahf` / `test ah, 0x44` / `jp`. Ordered equality falls through to `or eax, -1`. NaN is unordered (ZF, PF, and CF) and takes the tail call `0x007985D0`, as does every other encoding.

`007A5590` shifts `template+0xCC` by 10 and tests bit 0 of the shifted byte. The bit set returns 0. The bit clear is `neg` / `sbb` / `and 2` / `add -2`, which is `0xFFFFFFFE`.

`007AD210` calls `0x009CCDA0` first, then overwrites EAX with `1 << (index & 31)` where the index is `self+0x28`. A clear bit returns that mask. A set bit clears it and returns the `0x006A4FB0` result. `007AD1C0` copies `self+0x20` to `self+0x24` before the same bit index.

`007A2340` and `007A46D0` shift `[xfer+4]` by 3 and test the low bit. When that bit is set they return the shifted value and skip the extra field virtuals. `007A46D0` calls `0x007A44D0`; the other field xfers call `0x007A1FC0`. `007A6480` writes the version bytes, calls cdecl `0x0054A090` with ECX still the xfer pointer, then calls `0x007A1FC0`.

Status exits publish through `0x006A4F80` then `0x006A4FB0` and keep the second return. A clear bit leaves the machine pointer or the earlier call's EAX, as each body writes it.

## Dropped

- 88 slot-2 name getters in the range (`mov eax, imm32; ret`). Boilerplate, not counted.
- `007A7A50`, `007A7AF0`, `007A7BF0`: `EASTL/allocator.h`.
- `007A64C0` (47 bytes, slots of AICombineState, AIMoveToAndEvacuateState, AIAttackMoveToAndEvacuateState, AIDockApproachState) writes the version bytes and calls `0x007A1FC0` only.
- `007A6500` (47 bytes, AIFollowPathAndEvacuateState slot 3) writes the version bytes and calls `0x007A3E20` only.
- `007A65F0` (AIRotateFiringarc slot 5, 155 bytes) has `jbe 0x007A668A`. That target is the last byte of the ledger range; the linear listing ends at `ret` `0x007A6689`.
- `007A7940` (28 bytes, vtable `00C32330` slot 3) ends at `test eax, eax` with no `ret` inside the ledger size. Slot 2 is not a name getter.
- `007A0020` (vtable `00BE5208` slot 3), `007A72C0` and `007A72F0` (vtable `00C33914` slots 3 and 5): slot 2 is not `mov eax, imm32; ret`.
- `007A57A0` (208 bytes) is the factory callee of `007A58A0` and stays a stub.
- Larger named bodies left for a later batch: `007A0D20` (261), `007A2E50` (364), `007A6720` (406, `AIStates.cpp`), `007A8640` (426), `007A8F60` (617, `GetStrafePosition.cpp`, UnitAIGetStrafePositionState `00C29E68` slot 4), `007A9760` (333), `007AEA10` (415).

## Oracle and mutation

`ra3_aistate7acheck` against Unicorn:

`cases=131 mismatches=0`

Mutation: `AIWaitUntilMembersFinishedBeforeExitState_onEnter` shifted by 11 instead of 10. The oracle reported `cases=131 mismatches=1`, case `wait 0x400`, checker `fffffffe` against the image `0`. The shift was restored and the oracle returned to `cases=131 mismatches=0`.

## Hooks

None.
