# GameLogic 0x007C AIStateMachine & AI State Hierarchy (`recon/gamelogic7c`)

## Scope and Ownership Evidence

This batch reconstructs **45 clean (`securom=clean`) functions** in the `0x007C0000..0x007CFFFF` address range belonging to `AIStateMachine` and concrete `State` subclasses (`AIMoveToState`, `AIMoveToStateSA`, `AIMoveOutOfTheWayState`, `AIMoveAndTightenState`, `AIMoveAwayPanicState`, `AIMoveAwayFromRepulsorsState`, `AIBackAwayAndCowerState`, `AIMoveAwayAndCowerState`, `AIPickUpCrateState`, `AIMoveAndEvacuateState`, `AIMoveAndDeleteState`, `AIFollowWaypointPathStateAndEvacuate`, `AIAttackFollowWaypointPathState`, `AIDockState`, `AIHarvestState`, `AIGuardRetaliateState`, `AIMoveToPositionAndDieState`, `AIMoveToAndEvacuateState`, `AIFollowPathAndEvacuateState`, `AIMoveForBoarding`, `UnitAIAttackBlockingObjectState`, `UnitAIEnterAndDieState`, `UnitAIFindReplacementState`, `UnitAIFollowWaypointPathAsTeamState`, `UnitAIGarrisonedState`, `UnitAIHoldGroundState`, `AIEnterState`, `AIExitState`, and `AIExitSiegeEngineState`).

Ownership is proven from the shipped `ra3_1.0.game` binary by:
1. **Vtable slot membership and RTTI/class-name getters (`slot[2]`)**: Every state and state-machine vtable in `.rdata` (`0x00C2AF38..0x00C326D0`) has a class-name getter in `slot[2]` (`+0x08`) returning the exact class name string (e.g., `"AIMoveToState"`, `"AIDockState"`, `"AIHarvestState"`, `"AIGuardRetaliateState"`, `"AIStateMachine"`, `"AIEnterState"`, `"AIExitState"`, `"AIExitSiegeEngineState"`).
2. **Source path assertions**: Embedded debug path strings at `0x00C297D0` (`c:\aur\build\ra3_1.0\sage\run\game\gamelogic\statemachine\ai\aistates.cpp`) and `0x00C338D8` (`AIInternalMoveToState::onEnter`) are passed directly into lock/diagnostic helpers (`0x00A21BE0`, `0x005AA240`) by methods in this cluster.
3. **Direct caller/callee closure**: `0x007CCA30` (`AIStateMachine::exitTemporaryState`) is called directly by `AIStateMachine::clear` (`0x007CCB00`), `AIStateMachine::resetToDefaultState` (`0x007CCB70`), and `AIStateMachine::setState` (`0x007CCBC0`); `0x007C0AD0` (`AIMoveToState::onEnter`) is tail-called by `AIMoveToAndEvacuateState::onEnter` (`0x007C4D30`) and `AIMoveForBoarding::onEnter` (`0x007C4DE0`).

## Reconstructed Function Inventory (45 Functions)

| VA | Size | Class / Vtable Slot | Function | Oracle Cases |
|---|---:|---|---|---:|
| `0x007C0AD0` | 248 | `AIMoveToState` (`0x00C2AF38` slot 4) | `internalMoveToStateOnEnter_007C0AD0` | 12 |
| `0x007C0BD0` | 172 | `AIMoveToStateSA` (`0x00C2AF98` slot 4) | `moveOutOfTheWayStateOnEnter_007C0BD0` | 8 |
| `0x007C0C80` | 115 | `AIMoveOutOfTheWayState` (`0x00C2AFC0` slot 4) | `moveAndTightenStateOnEnter_007C0C80` | 8 |
| `0x007C0D00` | 107 | `AIMoveAndTightenState` (`0x00C2B010` slot 4) | `moveAwayFromRepulsorsStateOnEnter_007C0D00` | 6 |
| `0x007C0D70` | 126 | `AIMoveAwayPanicState` (`0x00C2B060` slot 4) | `attackApproachTargetStateOnEnter_007C0D70` | 8 |
| `0x007C0DF0` | 141 | `AIMoveAwayFromRepulsorsState` (`0x00C2B090` slot 4) | `attackMoveApproachTargetStateOnEnter_007C0DF0` | 8 |
| `0x007C0E80` | 91 | `AIBackAwayAndCowerState` (`0x00C2B0B8` slot 4) | `subMachineOwnerStateOnEnter_007C0E80` | 8 |
| `0x007C0EE0` | 81 | `AIMoveAwayAndCowerState` (`0x00C2B108` slot 4) | `moveToTargetObjectStateOnEnter_007C0EE0` | 8 |
| `0x007C2C80` | 20 | `AIPickUpCrateState` (`0x00C2B740` slot 5) | `delayedMoveStateUpdate_007C2C80` | 6 |
| `0x007C3520` | 86 | `AIMoveAndEvacuateState` (`0x00C2BAE8` slot 4) | `moveAndEvacuateStateOnEnter_007C3520` | 6 |
| `0x007C3680` | 178 | `AIMoveAndDeleteState` (`0x00C2BB10` slot 4) | `moveWithStatusFlagsStateOnEnter_007C3680` | 10 |
| `0x007C39C0` | 28 | `AIFollowWaypointPathStateAndEvacuate` (`0x00C2BB88` slot 4) | `internalMoveWrapperStateOnEnter_007C39C0` | 4 |
| `0x007C3B50` | 41 | `AIAttackFollowWaypointPathState` (`0x00C2BC00` slot 4) | `subMachineResetMoveStateOnEnter_007C3B50` | 4 |
| `0x007C42E0` | 179 | `AIDockState` (`0x00C2BF78` slot 1) | `guardStateXfer_007C42E0` | 10 |
| `0x007C43A0` | 172 | `AIDockState` (`0x00C2BF78` slot 4) | `guardStateOnEnter_007C43A0` | 10 |
| `0x007C4450` | 179 | `AIHarvestState` (`0x00C2BFA8` slot 1) | `guardRetaliateStateXfer_007C4450` | 10 |
| `0x007C4510` | 164 | `AIHarvestState` (`0x00C2BFA8` slot 4) | `guardRetaliateStateOnEnter_007C4510` | 10 |
| `0x007C4820` | 145 | `AIGuardRetaliateState` (`0x00C2C000` slot 1) | `areaGuardSubMachineStateXfer_007C4820` | 10 |
| `0x007C48C0` | 127 | `AIGuardRetaliateState` (`0x00C2C000` slot 4) | `areaGuardSubMachineStateOnEnter_007C48C0` | 8 |
| `0x007C4BE0` | 89 | `AIMoveToPositionAndDieState` (`0x00C2C0C0` slot 4) | `moveValidatedStateOnEnter_007C4BE0` | 6 |
| `0x007C4D30` | 129 | `AIMoveToAndEvacuateState` (`0x00C2C0E8` slot 4) | `moveIfBeyondDistanceStateOnEnter_007C4D30` | 12 |
| `0x007C4DC0` | 28 | `AIFollowPathAndEvacuateState` (`0x00C2C110` slot 4) | `followPathMode5StateOnEnter_007C4DC0` | 4 |
| `0x007C4DE0` | 28 | `AIMoveForBoarding` (`0x00C2C180` slot 4) | `internalMoveMode5StateOnEnter_007C4DE0` | 8 |
| `0x007C6B80` | 41 | `AIStateMachine` derived (`0x00C2CD90` slot 19) | `stateMachineCreateSubMachine19_007C6B80` | 6 |
| `0x007C6BB0` | 41 | `AIStateMachine` derived (`0x00C2CD90` slot 20) | `stateMachineCreateSubMachine20_007C6BB0` | 6 |
| `0x007C6BE0` | 39 | `AIStateMachine` derived (`0x00C2CD90` slot 21) | `stateMachineCreateSubMachine21_007C6BE0` | 6 |
| `0x007C6C10` | 97 | `AIStateMachine` derived (`0x00C2CDE8` slot 19) | `stateMachineCreateSubMachine19_007C6C10` | 8 |
| `0x007C6C80` | 97 | `AIStateMachine` derived (`0x00C2CDE8` slot 20) | `stateMachineCreateSubMachine20_007C6C80` | 8 |
| `0x007C6E90` | 269 | `AIStateMachine` derived (`0x00C2CE88` slot 5) | `stateMachineInitDefaultState_007C6E90` | 12 |
| `0x007C86E0` | 150 | `UnitAIAttackBlockingObjectState` (`0x00C2D9F0` slot 6) | `unitAttackTargetStateOnExit_007C86E0` | 8 |
| `0x007C8C90` | 172 | `UnitAIEnterAndDieState` (`0x00C2DA58` slot 4) | `unitEnterContainerStateOnEnter_007C8C90` | 8 |
| `0x007C8D40` | 157 | `UnitAIFindReplacementState` (`0x00C2DA80` slot 4) | `unitNotifyContainerStateOnEnter_007C8D40` | 8 |
| `0x007C8DE0` | 83 | `UnitAIFollowWaypointPathAsTeamState` (`0x00C2DAB0` slot 11) | `unitQueryWeaponClipOffset_007C8DE0` | 8 |
| `0x007C9150` | 39 | `UnitAIGarrisonedState` (`0x00C2DBE0` slot 15) | `stateCreateSubMachine_007C9150` | 6 |
| `0x007C9280` | 39 | `UnitAIHoldGroundState` (`0x00C2DC38` slot 15) | `stateCreateSubMachine_007C9280` | 6 |
| `0x007CC8A0` | 44 | `AIStateMachine` (`0x00C31FB0` slot 19) | `aiStateMachineCreateSubMachine19_007CC8A0` | 6 |
| `0x007CC8D0` | 37 | `AIStateMachine` (`0x00C31FB0` slot 20) | `aiStateMachineCreateSubMachine20_007CC8D0` | 6 |
| `0x007CC920` | 269 | `AIStateMachine` (`0x00C31FB0` slot 4) | `aiStateMachineUpdate_007CC920` | 14 |
| `0x007CCA30` | 206 | `AIStateMachine` (called by `0x007CCB00`/`B70`/`BC0`) | `aiStateMachineExitTemporaryState_007CCA30` | 10 |
| `0x007CCB00` | 112 | `AIStateMachine` (`0x00C31FB0` slot 13) | `aiStateMachineClear_007CCB00` | 10 |
| `0x007CCB70` | 73 | `AIStateMachine` (`0x00C31FB0` slot 6) | `aiStateMachineResetToDefaultState_007CCB70` | 8 |
| `0x007CCBC0` | 162 | `AIStateMachine` (`0x00C31FB0` slot 7) | `aiStateMachineSetState_007CCBC0` | 10 |
| `0x007CE280` | 184 | `AIEnterState` (`0x00C32530` slot 6) | `enterContainerStateOnExit_007CE280` | 10 |
| `0x007CE460` | 158 | `AIExitState` (`0x00C32680` slot 6) | `exitContainerStateOnExit_007CE460` | 8 |
| `0x007CE500` | 68 | `AIExitSiegeEngineState` (`0x00C326D0` slot 6) | `exitOpenContainerStateOnExit_007CE500` | 8 |

## Recovered Structure Layouts

### `AIStateMachine` (size `0xA0`)
- `+0x00`: `vtable` (`0x00C31FB0` base)
- `+0x04`: `State* currentState` (`+0x04` on `State` holds `stateId`)
- `+0x24`: `Object* owner`
- `+0x28`: `ObjectID goalObjectID`
- `+0x2C..+0x34`: `Coord3D goalPosition` (`x, y, z`)
- `+0x44`: `uint8_t goalPositionDirty`
- `+0x48..+0x50`: `Coord3D cachedGoalPosition`
- `+0x54`: `float goalRadius` (default `0x7F7FFFFF` / `FLT_MAX` at `0x00BE700C`)
- `+0x78`: `uint8_t isLocked`
- `+0x80`: `uint32_t secondaryStateId`
- `+0x84`: `StateMachine* subMachine`
- `+0x88`: `State* temporaryState`
- `+0x8C`: `uint32_t temporaryStateFrameEnd` (`0xFFFFFFFF` = permanent override)
- `+0x90`: `ObjectID savedGoalObjectID`
- `+0x94..+0x9C`: `Coord3D savedGoalPosition`

### `AIInternalMoveToState` / `AIMoveToState` Family (`0x60..0x70` bytes)
- `+0x00`: `vtable`
- `+0x18`: `AIStateMachine* machine`
- `+0x20..+0x28`: `Coord3D goalPosition` (`x, y, z`)
- `+0x4C`: `uint8_t adjustForStructure`
- `+0x50`: `uint32_t targetObjectID` / `uint8_t statusMoveFlag` (or `Coord3D evacOrigin` at `+0x50..+0x58` in `AIMoveAndEvacuateState`)
- `+0x54`: `uint8_t moveOutFlag` / `followTargetFlag`

## Differential Oracle and Semantic Mutation Check

- **Oracle script**: `Ra3Source/tools/analysis/ports/verify_gamelogic7c.py`
- **Checker executable**: `ra3_gamelogic7ccheck` (`Ra3Source/tools/gamelogic7ccheck/main.cpp`)
- **Result**: **45 functions, 364 scenarios, 0 mismatches** (`stack_cleanup`, `EAX`, FNV-1a callback trace hash, and full `0x2000`-byte arena state).
- **Semantic mutation check**: `ra3_gamelogic7ccheck_mutant` compiles `AIStateMachine7C.cpp` with `RA3_GAMELOGIC7C_ORACLE_MUTANT`, flipping the temporary-state expiration comparison in `aiStateMachineUpdate_007CC920` (`tempFrameEnd < currentFrame` -> `tempFrameEnd > currentFrame`). Running `verify_gamelogic7c.py --mutation-checker` detects the mutation immediately (`status=PASS`), and the unmutated checker passes with `0` mismatches.

## Hook Targets (`GameLogic7CTargets.cpp`)

All 8 hooks are registered with `explicitOnly = true` because each target is an `onEnter` method that invokes engine movement/status helpers (`0x007C08A0`, `0x006A4F80`, `0x007526C0`, `0x005AA240`). In shadow mode, each wrapper runs the C++ port on a stack-allocated shadow copy (`kMoveStateShadowSize = 0x60`) using `liveShadowReadDispatch()` (side-effect-free read getters only), restores any transient owner/machine flags before invoking `state.original(statePtr)`, and compares the deterministic state fields via `recordOutput` / `recordScalar`.

| Key | VA | Expected 16 Bytes | Stolen | explicitOnly |
|---|---|---|---:|---|
| `gamelogic.aistatemachine7c.move_out_of_the_way_on_enter_007c0bd0` | `0x007C0BD0` | `53 56 8B F1 8B 4E 18 C6 46 4C 01 8B 41 24 57 8B` | 7 | `true` |
| `gamelogic.aistatemachine7c.move_and_tighten_on_enter_007c0c80` | `0x007C0C80` | `53 56 57 8B F9 8B 47 18 C6 47 4C 01 8B 70 24 8B` | 5 | `true` |
| `gamelogic.aistatemachine7c.move_away_from_repulsors_on_enter_007c0d00` | `0x007C0D00` | `56 8B F1 8B 46 18 C6 46 4C 00 8B 48 24 B8 01 00` | 6 | `true` |
| `gamelogic.aistatemachine7c.attack_approach_target_on_enter_007c0d70` | `0x007C0D70` | `53 55 56 8B F1 8B 4E 18 57 C6 46 4C 00 8B 79 24` | 5 | `true` |
| `gamelogic.aistatemachine7c.move_to_target_object_on_enter_007c0ee0` | `0x007C0EE0` | `53 56 8B F1 8B 4E 18 57 E8 43 90 DF FF 8B D8 85` | 7 | `true` |
| `gamelogic.aistatemachine7c.move_and_evacuate_on_enter_007c3520` | `0x007C3520` | `56 8B F1 8B 4E 18 8B 41 24 8B 50 38 83 C0 38 89` | 6 | `true` |
| `gamelogic.aistatemachine7c.move_with_status_flags_on_enter_007c3680` | `0x007C3680` | `56 8B F1 8B 4E 18 57 68 D8 38 C3 00 C6 46 4C 00` | 6 | `true` |
| `gamelogic.aistatemachine7c.move_validated_on_enter_007c4be0` | `0x007C4BE0` | `56 8B F1 8B 4E 18 68 D8 38 C3 00 C6 46 4C 00 E8` | 6 | `true` |

## Deferred Boundaries

- Heavy pathfinding and waypoint-following update bodies in `0x007C0000..0x007CFFFF` (such as `0x007C08A0` `AIInternalMoveToState::startMove`, `0x007C2CA0` `AIFollowPathState::onEnter`, and `0x007C3740` `AIFollowWaypointPathState::onEnter`) depend on external pathfinder/locomotor state and are modeled through `GameDispatch` boundaries for a future pathfinding batch.
