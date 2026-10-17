# AITNGuard game-logic reconstruction

## Accepted boundary

This batch reconstructs the tunnel-network guard state family, not every
function near `0x00790000`. The class identity is supported by independent
evidence:

- vtable `0x00C2D488` names `AITNGuardMachine` at `0x0079B4B0`;
- vtable `0x00C2D4E8` names `AITNGuardInnerState` at `0x0079B4C0`;
- vtable `0x00C2D544` has the `AITNGuardOuterState` secondary name at
  `0x0079B600`;
- vtable `0x00C30100` names `AITNGuardIdleState` at `0x007A6970` and points
  directly to `0x0079B640`, `0x0079B690`, and `0x007CF360`;
- vtable `0x00C2D610` names `AITNGuardAttackAggressorState` at `0x0079B780`;
- the literal source paths in `0x0079B690` and `0x007CF280` are
  `SageEngine\source\GameEngine\gamelogic\ai\AITNGuard.cpp`;
- the seven out-of-line state callbacks at `0x007CEE00`–`0x007CF670` are
  direct members of those vtables.

The local translation-unit interval starts after `AIUnDeployState`
(`0x0079B430`–`0x0079B470`) and ends before `AIInternalMoveToState`
(`0x0079C420`–`0x0079C430`). Only 39 nontrivial bodies from
`0x0079B4D0`–`0x0079C3D0` are accepted. Eleven connected bodies are outside
that interval: the machine deleting destructor at `0x007A68C0`, two
AITNGuard helpers at `0x007CE860`/`0x007CEB30`, and eight state callbacks
through `0x007CF670`. This gives 50 nontrivial clean native functions; the
short type-name returns are evidence but are not count padding.

## Source correspondence and RA3 differences

Names and state intent were cross-checked against the released Generals code:

- `GeneralsMD/Code/GameEngine/Include/GameLogic/AITNGuard.h:54-208`
  defines the machine, exit condition, and six guard states.
- `GeneralsMD/Code/GameEngine/Source/GameLogic/AI/AITNGuard.cpp:65-128`
  defines the aggressor test and nearest-tunnel selection.
- the same source at `:130-293` defines deadline handling, machine setup,
  target scanning, and snapshot methods;
- `:295-638` defines Inner, Outer, and Return behavior;
- `:640-773` defines Idle and crate-pickup behavior;
- `:775-877` defines AttackAggressor behavior.

The Generals source establishes original names and intent only. RA3's object
offsets, unsigned frame comparisons, state return values, extra helper
classes, and callback order come from the clean RA3 bodies. The port uses
pointer-free views for deterministic testing and leaves game-owned lookup,
allocation, virtual dispatch, tunnel-list mutation, and serialization behind
explicit inputs/effects.

## Oracle

`tools/analysis/ports/verify_skirmish_ai.py` has two independent layers:

1. Every accepted entry is compared with the shipped image by address, ledger
   size, clean status, and CRC32 over the complete native body.
2. Eight snapshot-safe leaf entries are executed from the shipped bytes with
   Unicorn and compared against `ra3_gamelogic`, including return values,
   changed bytes, and stack cleanup. Random cases cover conditional payloads,
   endpoint orientation, both cooldown tables, tracked targets, and three
   distance predicates.

The checker independently exercises the state policies and verifies a
deliberate `>=` to `>` deadline mutation. The Python oracle separately mutates
a body fingerprint and an endpoint result, requires both mismatches, and then
requires restored matches.

## Hook policy

`SkirmishTargets.cpp` exposes eight `explicitOnly` targets. They are pure
predicates or bounded snapshot reads/writes and contain no allocator,
lifecycle, thread, virtual-dispatch, or side-effectful global calls. The two
cooldown hooks read the current frame and immutable tuning constants, matching
the original. The hooks preserve cdecl/thiscall cleanup through MSVC fastcall
adapters; none returns an x87 value.

The remaining 42 functions are deliberately not hookable: their original
bodies allocate states, release owned children, call game-owned virtual
interfaces, mutate tunnel/team/global state, or serialize through external
objects.

## Deferred and unknown

- `AIUnDeployState` (`0x0079B430`–`0x0079B470`) is a preceding class and is
  excluded.
- `AIInternalMoveToState` beginning at `0x0079C420` is a following class and
  is excluded.
- TurretAI, FollowWaypointPath, generic UnitAI states, and all other nearby
  vtables are excluded even where they share generic State methods.
- Address-qualified helper names remain address-qualified where no symbol,
  assertion, vtable, or Generals method gives an exact RA3 name.
- No protected or unresolved body is accepted, and no game process was
  launched or injected during reconstruction.
