# GameLogic AI state-machine reconstruction

## Scope and ownership rule

This module reconstructs 51 clean RA3 functions around the AI state-machine
cluster. An address is accepted only when ownership closes through a state
vtable/name pair, an AI update interface, or a direct call path to a
source-anchored state-machine routine. Address proximity alone is not evidence.

The implementation is deliberately a set of non-owning layout views. It does
not construct, destroy, allocate, retain, or release game objects. External
virtual calls and singleton values are passed through `GameDispatch`,
`SupplyEvents`, `WeaponEvents`, and `LogicTiming`; adjusted secondary-interface
`this` pointers remain explicit.

Accepted address bands:

| Band | Ownership closure | Accepted |
|---|---|---:|
| `006819B0-006823C0` | Dozer/Supply state names, state vtables, and update-interface layouts | 28 |
| `00682890-00683670` | direct AI-state callers and shared `AIUpdateInterface`/`AIGateUpdate` layouts | 20 |
| `00685CC0-00685DA0` | `WeaponReadyState`, `WeaponCoolDownState`, and `WeaponReloadState` vtables | 3 |
| **Total** | | **51** |

The exact address/name ledger is
`docs/analysis/ra3_1.0/aistates_annotations.tsv`.

## Class and vtable map

| Recovered class/family | Vtable or interface evidence | Code |
|---|---|---|
| Dozer action substates | `00BF7D60`, `00BF7DD0`, `00BF7E40` | `006819B0-00681A00` |
| `DozerActionStateMachine` | `00BF7EA8`, constructor at `00681A30` | state methods above |
| `DozerPrimaryIdleState` / action state | `00BF7F54` family, `00BF7FBC` | `00681BB0-00681CC0` |
| Dozer secondary AI interface | `00C24874`, adjusted `this = module + 518h` | `00681E80-00681FB0` |
| Supply state family | `00BF8218-00BF8480` | `00682310-006823C0` |
| Supply secondary AI interface | `00C251A8`, adjusted `this = module + 518h` | `00682090-00682280` |
| shared `AIUpdateInterface` / gate views | secondary interface family including `00BDBC08`; `AIGateUpdate` `00BF86F8` | `00682D30-00683670` |
| `WeaponStateMachine` | `00BF9650` | support closure from `00685BC0` onward |
| `WeaponReadyState` | `00BF9710`, update slot 3 | `00685CC0` |
| `WeaponCoolDownState` | `00BF9840`, update slot 3 | `00685D20` |
| `WeaponReloadState` | `00BF9910`, update slot 3 | `00685DA0` |

The Dozer/Supply class identities are independently tied to the update module
vtables `00C24A50` and `00C25398` by their constructors and module-name
methods. Those out-of-band methods are evidence anchors, not accepted function
count.

## Recovered layouts

`StateView` covers offsets through `+28h`: machine `+18h`, state values
`+20h/+24h`, and the idle latch `+28h`. `DozerAIView` and
`SupplyTruckAIView` receive the secondary-interface address; module and owner
are intentionally reached at `this-518h` and `this-514h`.

`UnitAIView` is a non-owning view through `+503h`. Important recovered fields
include the state machine `+3Ch`, command latch `+30h`, desired speed `+188h`,
goal discriminant/vector `+21Ch/+220h`, current value `+250h`, flags
`+430h/+431h`, nested state `+47Ch`, and pending object IDs `+49Ch/+4FCh`.
No whole-object size or ownership claim follows from the minimum view size.

`WeaponStateView` uses machine `+18h`, weapon `+20h`, start/duration
`+24h/+28h`, and the context state word `+08h`.

## Evidence

The original side of the differential oracle executes clean bytes from the
read-only `D:/Game/Ra3/Game/Data/ra3_1.0.game` image through the offline
`Emulator`; no game process is launched. The run-35 capture is used only as an
ownership/protection cross-check, because every protected or entry-lost
candidate in this cluster is deferred rather than inferred. Function
boundaries, first bytes, ABIs, source strings, and vtable references are
cross-checked against:

- `docs/analysis/ra3_1.0/hook_candidates.tsv`
- `docs/analysis/ra3_1.0/vtables_split.tsv`
- `docs/analysis/ra3_1.0/function_ledger.tsv`

Generals GPL code guides class vocabulary and state-machine relationships, not
copied implementation:

- `work/references/generals/GeneralsMD/Code/GameEngine/Include/Common/StateMachine.h`
- `work/references/generals/GeneralsMD/Code/GameEngine/Source/Common/StateMachine.cpp`
- `work/references/generals/GeneralsMD/Code/GameEngine/Source/GameLogic/AI/AIStates.cpp`
- `work/references/generals/GeneralsMD/Code/GameEngine/Source/GameLogic/Object/Update/AIUpdate/DozerAIUpdate.cpp`
- `work/references/generals/GeneralsMD/Code/GameEngine/Source/GameLogic/Object/Update/AIUpdate/SupplyTruckAIUpdate.cpp`

RA3 behavior and layout come from the captured native bytes. In particular,
the RA3-only gate layouts are behavior-matched, not claimed as Generals source
equivalents.

## Differential oracle

`tools/analysis/ports/verify_aistates.py` runs 64 deterministic edge/random
cases for every accepted address (3,264 clean comparisons per configuration).
Each case compares:

- the complete `1800h` synthetic object arena, including exact write order
  under overlapping aliases;
- defined EAX/AL/pointer provenance or the x87 float result;
- callee stack cleanup;
- virtual/external callback order, target, slot, and argument;
- relocated pointers and null/non-null branches;
- `+0`, `-0`, subnormal, finite-limit, infinity, and NaN float inputs.

The checker receives equivalent relocated layouts but uses independently
allocated C++ objects. Pointer and byte-register results are normalized only
when their provenance is inside the synthetic arena. Undefined EAX after a
`void` ABI is not treated as a result; all defined return registers are
compared.

`RA3_AISTATE_ORACLE_MUTANT` changes the `00682D70` write at `+431h` from zero to one.
The mutant executable must be detected in every one of 16 cases before the
clean oracle is accepted.

## Hook policy

`dll/targets/AIStateTargets.cpp` contains 16 opt-in targets. Every entry is
`explicitOnly=true`. Writers run against a copied minimum view in shadow mode
and compare only the native write set; replacement copies only that write set
back. Read-only wrappers compare scalar/pointer results. All member wrappers
translate `__thiscall` to `__fastcall`.

No constructor, destructor, allocator, state-machine step, shared-thread
singleton mutation, or 80-bit x87-returning function is hooked. Functions that
depend on overridable virtual task queries are also excluded from replacement.

## Deferred boundaries

- `00682860`, `00682D20`, and `00683140` remain unresolved protected entries.
  No SecuROM VM, decryptor, or neighboring-byte inference is used.
- `00683630` is an entry-lost thunk; the runtime cave body is not promoted as
  the original entry.
- Constructors, destructors, factories, xfer/snapshot methods, allocators,
  ref-counted swaps, pathfinding, and full update loops are deferred.
- `00685B80-00685BA0` is rejected as neighboring weapon/slot logic.
  The nearby `00685DD0-00686540` weapon/config timing helpers are also
  excluded because their ownership does not close to a state vtable or a
  proven direct path from the source anchor. `00685EB0` is protected and
  unresolved. `00686580+` crosses into the FX/object-creation cluster.
- Name-only six-byte returns and trivial constant thunks are evidence anchors,
  never accepted functions.
