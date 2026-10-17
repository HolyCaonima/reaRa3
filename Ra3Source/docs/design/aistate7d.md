# AI state bodies `0x007D0000–0x007DFFFF`

## Scope

This batch ports 40 clean, previously unanalyzed functions whose entries lie in
`0x007D0000–0x007DFFFF`. Each counted row has a `vt` or `call` line in
`work/aistate7d_claims.txt`. Class names are the NUL-terminated strings returned
by slot 2 (`mov eax, imm32; ret`) of the cited split vtable. A shared body is
counted once, under the class on the cited line.

Direct and virtual callees are stubbed. The C++ in `ra3::gamelogic` compares
EAX, object bytes, and the normalized call trace with the shipped bytes under
Unicorn. Stack temporaries log as `ptr`.

## Counted functions

| VA | C++ name | Ownership |
| --- | --- | --- |
| `007D0980` | `aiStateMachineNewSlot19` | `00c30378` slot 19, AIStateMachine |
| `007D09B0` | `aiStateMachineNewSlot20` | `00c30378` slot 20, AIStateMachine |
| `007D09E0` | `aiStateMachineNewSlot21` | `00c30378` slot 21, AIStateMachine |
| `007D0A00` | `disguiseHoldGroundConstruct` | call from `007D0AA0` at `007D0ABD`; writes `00c343f8` and `00c2e640` |
| `007D0AA0` | `unitAIDisguiseHoldGroundClone` | `00c30770` slot 19, UnitAIDisguiseHoldGroundState |
| `007D0AD0` | `unitAILocateNodeConsider` | `00c2a1a0` slots 4 and 5, UnitAILocateNodeState |
| `007D0B30` | `aiDockProcessDockUpdate` | `00c2bc10` slot 5, AIDockProcessDockState |
| `007D0D00` | `aiGuardReturnOnEnter` | `00c31d08` slot 4, AIGuardReturnState |
| `007D1860` | `aiAttackAimOnEnter` | `00c2c8a8` slot 4, AIAttackAimAtTargetState |
| `007D19B0` | `aiAttackAimAtTargetUpdate` | `00c2c8a8` slot 5, AIAttackAimAtTargetState |
| `007D19C0` | `aiStateMachineNewSlot9` | `00c2c4c8` slot 9, AIStateMachine |
| `007D1A00` | `aiTunnelNetworkGuardXfer` | `00c2d230` slot 3, AITunnelNetworkGuardState |
| `007D1AA0` | `aiTunnelNetworkGuardOnEnter` | `00c2d230` slot 4, AITunnelNetworkGuardState |
| `007D1D60` | `turretAIXfer` | `00c345a8` slot 3, TurretAI |
| `007D2F90` | `turretAIReadAim` | call from `007D6090` at `007D60B7` |
| `007D6050` | `turretAIAimSlot19` | `00c2a788` slot 19, TurretAIAimTurretState |
| `007D6090` | `turretAIAimUpdate` | `00c2a788` slot 5, TurretAIAimTurretState |
| `007D6230` | `turretAIFireWeaponUpdate` | `00c2a860` slot 5, TurretAIFireWeaponState |
| `007DA4D0` | `aiPlayerSlot16` | `00c336a0` slot 16, AIPlayer |
| `007DA590` | `aiPlayerSlot19` | `00c336a0` slot 19, AIPlayer |
| `007DA660` | `workOrderXfer` | `00c31ea4` slot 3, WorkOrder |
| `007DABC0` | `aiPlayerSlot26` | `00c336a0` slot 26, AIPlayer |
| `007DAD30` | `aiPlayerSlot5` | `00c336a0` slot 5, AIPlayer |
| `007DB090` | `aiIdleUpdate` | `00c2c520` slot 5, AIIdleState |
| `007DB430` | `aiCowerUpdate` | `00c2c708` slot 5, AICowerState |
| `007DB530` | `aiBackAwayUpdate` | `00c2f6b8` slot 5, AIBackAwayState |
| `007DB660` | `aiMoveAndEvacuateUpdate` | `00c2fbe0` slot 5, AIMoveAndEvacuateState |
| `007DB6C0` | `aiFollowWaypointEvacuateUpdate` | `00c324d0` slot 5, AIFollowWaypointPathStateAndEvacuate |
| `007DBB20` | `aiFaceStateUpdate` | `00c2d950` slot 5, AIFaceState |
| `007DBBC0` | `aiChargeTargetOnEnter` | `00c2ff50` slot 4, AIChargeTargetState |
| `007DBF70` | `aiMoveToAndEvacuateUpdate` | `00c30228` slot 5, AIMoveToAndEvacuateState |
| `007DC160` | `aiPrepareForBoardingUpdate` | `00c2d300` slot 5, AIPrepareForBoarding |
| `007DC1B0` | `aiTNGuardReturnUpdate` | `00c33840` slot 5, AITNGuardReturnState |
| `007DC680` | `squadXfer` | `00c32318` slot 3, Squad |
| `007DCAA0` | `aiAttackSquadBindTarget` | call from `007DFE70` at `007DFF30` |
| `007DE580` | `aiFaceStateUpdateWide` | `00c2f248` slot 5, AIFaceState |
| `007DF320` | `unitAIPickTargetApplySpan` | call from `007DFCF0` at `007DFD76` |
| `007DFCF0` | `unitAIPickTargetOnEnter` | `00c2b630` slot 4, UnitAIPickTargetFromSquadState |
| `007DFE70` | `aiAttackSquadOnEnter` | `00c2cd88` slot 4, AIAttackSquadState |
| `007DFF50` | `aiAttackSquadUpdate` | `00c2cd88` slot 5, AIAttackSquadState |

Five of these (`007D0980`, `007D09B0`, `007D09E0`, `007D0AA0`, `007D19C0`) are
same-shape allocate-and-construct factories, which is the cap of five.
`007D0A00` is not one of them: it writes two vtables and registers a nested
state. No counted row is a slot-2 name getter, a constant stub, or an xfer that
only forwards to a base. Boilerplate is 0 of 40.

`007D0A60` (`mov eax, 0xC34440; ret`, string `DisguiseHoldGroundStateMachine`)
and `007D1D50` (`mov eax, 0xC345B8; ret`, string `TurretAI`) are the slot-2
getters next to counted bodies and are not counted.

## What the bodies do

State updates return `0` to continue, `0xFFFFFFFF` for success, and
`0xFFFFFFFE` for failure. The usual object links are state+`0x18` machine,
machine+`0x24` unit, and unit+`0x374` locomotor.

- `007D0AD0` continues only when unit X and Z are both ordered-equal to zero;
  otherwise a probe selects success, failure, or continue.
- `007D6050` replaces AL with whether both probe bytes are set and keeps the
  rest of the callee's EAX. `007D6230` succeeds immediately when the weapon
  field at +`0x78` is zero.
- `007D6090` reads an aim mode through `007D2F90`, then aims or fires.
  `007D2F90` walks the +`0x9C` chain and fills the two out pointers.
- Evacuate updates `007DB660`, `007DB6C0`, and `007DBF70` clear a blocked
  locomotor and, on success, hand the unit to `007DAD60`.
- Face and guard updates `007DBB20`, `007DE580`, and `007D0D00` compare an
  angle or a squared distance with the constants at `00BD9558` and `00BD95B8`.
- Player slots `007DA4D0` and `007DA590` count down and chop a time-scale
  product toward zero. `007DABC0` and `007DAD30` are virtual probes with a tail
  call.
- Xfers `007D1A00`, `007DC680`, `007DA660`, and `007D1D60` return early when
  xfer mode bit 3 is set, and otherwise walk version, bool, and pointer slots.
- `007D0A00` stores `00C343F8` on this and `00C2E640` on the nested state, then
  registers that state.

## Oracle

`python Ra3Source/tools/analysis/ports/verify_aistate7d.py` against the Release
checker:

```
aistate7d oracle Release cases=85 functions=40 mismatches=0
```

Mutation: the ordered-and in `unitAILocateNodeConsider` (`007D0AD0`) was
temporarily changed to an ordered-or. The oracle stopped on case 1 with
`eax ffffffff != 00000000` and a missing call to `007C7EF0`. The comparison
was restored and the same command again printed `mismatches=0`.

Debug and Release builds of `ra3_aistate7dcheck` completed with no new warnings.
There are no hooks.

## Dropped

| VA | Reason |
| --- | --- |
| `007D007B`, `007D007C` | One-byte ledger entries, no body. |
| `007D0A60`, `007D1D50` | Slot-2 name getters (`mov eax, imm32; ret`). Not counted. |
| `007D0A70` | Destructor: writes `00C29980`, calls `005B2370`, optional `004166B0`. |
| `007D0630`, `007D06E0`, `007D2F70` | `ret 4` destroy slots: call a base destructor and optionally `004166B0`. |
| `007D1850`, `007DA750`, `007DA760`, `007DA770` | Five-byte thunks (`jmp` to `007D1390`, `007D8AB0`, `007D9010`, `007D95D0`). |
| `007D1390`, `007D9010`, `007D9CF0`, `007DA780` | Ledger source is `EASTL/allocator.h`. Not counted. |
| `007D08E0` | Container walk, not a named state update. |
| `007D8AB0`, `007D95D0` | Large AIPlayer bodies (674 and 954 bytes) left outside this set. |

The rest of the unanalyzed entries in the page are outside the 40 counted rows.
No address outside `0x007D0000–0x007DFFFF` is claimed.
