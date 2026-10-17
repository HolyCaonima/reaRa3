# GameLogic behavior, update, and contain reconstruction

## Scope

This batch reconstructs 57 clean `ra3_1.0.game` entries in the GameLogic
behavior cluster between `0x006B1540` and `0x006BA5D0`.  Fifty entries expose
stateful decisions, list operations, vector/float calculations, status
transitions, or constructor write footprints.  Seven small ABI leaves are kept
because they close the same vtables and provide safe explicit hooks.

The public C++ API exposes the independently testable semantic decision made by
each entry.  A full game method can additionally traverse engine-owned lists or
invoke virtual callbacks; the differential oracle executes that shipped method
and compares the callback arguments and object writes that correspond to the
public result.  This is an intentional framework boundary, not a claim that
`Behavior.cpp` recreates unrelated object ownership or callback dispatch.

The implementation is original code.  Names and high-level responsibilities
were cross-checked against these exact Generals references:

- `GameLogic/Object/Contain/GarrisonContain.cpp`
- `GameLogic/Object/Update/SpecialAbilityUpdate.cpp`
- `GameLogic/Object/Update/SpecialPowerUpdateModule.cpp`
- `GameLogic/Object/Update/DockUpdate/DockUpdate.cpp`
- `GameLogic/Object/Update/OCLUpdate.cpp`
- `GameLogic/Object/Update/PhysicsUpdate.cpp`

RA3 layouts, constants, control flow, ABIs, and write footprints come from
`ra3_1.0.game`; no reference implementation was copied.

## Ownership evidence

Ownership was closed by calls, vtables, assertion paths, and module type names,
not by address proximity:

- `0x006B1540` carries the exact `GarrisonContain.cpp` assertion-source path.
  Its callers at `0x006B1670`, `0x006B16E0`, and `0x006B1740`, plus the shared
  station-assignment callbacks, close the contain family through `0x006B27B0`.
- `0x006B34B0` carries the
  `ActivateModuleSpecialPower.cpp` path and assertion text.  Its module-data
  calls and special-power vtables close the ability/progress entries through
  `0x006B4EB0`.
- `0x006B4D80` and `0x006B4EB0` carry exact
  `ToggleStatusSpecialAbilityUpdate.cpp` and
  `WeaponFireSpecialAbilityUpdate.cpp` paths.  The `SpecialPowerUpdateModule`
  type-name/vtable family owns `0x006B4720`–`0x006B51C0`.
- Dock module data, the common transform callback at `0x0078BA00`, and source,
  target, and approach field offsets close `0x006B5AE0`–`0x006B5CA0`.
- The FireWeapon and related update vtables close the destination, targeting,
  radius, and initialization entries through `0x006B8480`.
- `0x006B8930` carries the exact `OCLUpdate.cpp` path and marks the boundary to
  the Physics family.  It is ownership evidence only; its random spawning
  orchestration is not counted as a reconstructed entry in this batch.
- `0x006B8CF0` carries the exact `PhysicsUpdate.cpp` path.  The
  `PhysicsBehavior`, contact-list, delayed-weapon, overlap, status, and
  `SpawnCrateUpdate` vtables close the family through `0x006BA5D0`.

The range also contains unrelated helper types and protected sites.  They were
not reclassified merely because they are nearby, and no `0x006A` ObjectLogic or
`0x0068` AI-state implementation is duplicated.

## Differential checker

`ra3_behaviorcheck` accepts a fixed 1,024-byte per-entry scenario and emits the
same-size result.  `verify_gamelogic_behavior.py` runs every catalog entry in
two ways:

1. execute the shipped clean body in `Emulator`, with only external
   engine-owned callbacks replaced by recording stubs;
2. evaluate the corresponding C++ semantic contract and compare the native
   decision, callback arguments, EAX/ST0 result, stack cleanup, aliases, and
   exact write footprint.

Float entries compare caller-rounded float32 results or explicit vectors.
Constructors start from nonzero sentinels where padding preservation matters.
List tests preserve native aliasing, and status tests verify both selected bits
and dirty callbacks.  Every entry also runs through a deliberate output-bit
mutation; equality would fail the oracle.

The oracle maps only the shipped `.text`, `.rdata`, and load-time `.data`
images.  It does not launch or inject into the game.  No SecuROM VM is
executed.

## Hook boundary

Eight hooks are registered `explicitOnly`:

`0x006B3980`, `0x006B3DC0`, `0x006B7D70`, `0x006B7D80`, `0x006B8480`,
`0x006B8C70`, `0x006B8EC0`, and `0x006B91E0`.

They are leaf state selectors, flag writers, a fixed-layout constructor, or a
read-only target predicate.  None allocates, destroys, starts threads, or owns
global side effects.  The wrappers preserve the original fastcall/thiscall or
stdcall cleanup, EAX output-pointer rules, and the constructor's exact byte
writes.  No x87-returning entry is hooked, so a naked ST0 bridge is unnecessary.

## Deferred sites

Protected bodies are not inferred from nearby clean code.  Entry-lost or
inner-patched sites are deferred unless a captured run35 slot resolves to a
native body; unresolved sites remain unknown.  This batch neither analyzes a
SecuROM VM nor adds hooks for protected constructors, destructors, allocators,
threading paths, or global lifecycle operations.

The exact protected sites deferred in `0x006B0000`–`0x006BFFFF` are:

- inner-patched: `0x006B08C0`, `0x006B1B50`, `0x006B2A30`, `0x006B4F80`,
  `0x006B5A30`, `0x006B6BF0`, `0x006B7040`, `0x006B8130`, `0x006B8F50`,
  `0x006B9A60`, `0x006BB360`, and `0x006BD510`;
- entry-recoverable thunks not needed by this cluster:
  `0x006BA210` and `0x006BCF30`.
