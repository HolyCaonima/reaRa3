# Drawable accessor cluster (`recon/drawable52`)

## Scope

Thirty clean functions in `0x00520000`–`0x0052FFFF` from `ra3_1.0.game`. Each one is a getter, a flag predicate, a setter with a fixed write set, or a small lookup helper. The implementation is `ra3::gameclient` in `DrawableAccess.h` / `DrawableAccess.cpp`. The oracle is `tools/analysis/ports/verify_drawable52.py` against `ra3_drawable52check`. Four explicit-only hooks cover pure leaves.

The source anchor is `SageEngine\source\GameEngine\gameclient\Drawable.cpp`, ledger range `0x0051FCB0`–`0x0052C1C0`. The helper constructor `0x0051FCB0` is outside this window and is not counted. `Drawable::xfer` at `0x0053EE30` is outside the window; its string is `Drawable::xfer`.

`work/references/generals/GeneralsMD/Code/GameEngine/Include/GameClient/Drawable.h` and `Source/GameClient/Drawable.cpp` were used for architecture only (module lists, tint, opacity). Offsets and control flow come from the RA3 bytes.

## Ownership

Primary vtable `0x00BE1058` is installed by `0x0053CAF0` and `0x00542A10`. The class name `Drawable` is at `0x00BE1080`. Secondary vtable `0x00BE1048` sits at `this+0x60`. TintEnvelope vtable `0x00BDFFD4` is installed by `Drawable::xfer`.

| VA | Function | Proof |
| --- | --- | --- |
| `0x005209B0` | `visitModules20` | primary slot 8 at `0x00BE1078` |
| `0x00520F50` | `queryModulesB0` | primary slot 9 at `0x00BE107C` |
| `0x00525BD0` | `publishScalar` | primary slot 5 at `0x00BE106C` |
| `0x0052DEF0` | `syncSecondary` | secondary slot 1 at `0x00BE104C` |
| `0x005261A0` | `tintEnvelopeStatic` | TintEnvelope slot 2 at `0x00BDFFDC` |
| `0x00520CB0` | `readTintEnvelope` | TintEnvelope slot 3 at `0x00BDFFE0` |
| `0x00523140` | `readCountedBlock` | direct call from xfer at `0x0053EEFF` |
| `0x0052D790` | `transformPointer` | direct call from xfer at `0x0053EF09` |
| `0x0052DC90` | `refreshModuleGate` | direct call from xfer at `0x0053F90E` |
| `0x00520380` | `ensureAuxRecord` | direct call from xfer at `0x0053FB0D` |
| `0x00525D40` | `releaseTracked` | direct call from xfer at `0x0053FEB7` |
| `0x00524310` | `updateSwayState` | direct call to constructor `0x0051FCB0` |
| `0x00520170` | `addSwayOffset` | called from constructor callers `0x0052BAD0` and `0x0052C3D0` |
| `0x00520220` | `integrateSwayAngle` | called from those same constructor callers |
| `0x00525CF0` | `positionPointer` | called from Drawable.cpp-asserting `0x0052C1C0` at `0x0052C273` |
| `0x00524050` | `publishCondition` | called by secondary slot `0x0052DEF0` |
| `0x0052A800` | `copyTwelveDwords` | called by secondary slot `0x0052DEF0` |
| `0x00524010` | `publishArgument` | caller `0x005371D0` also calls secondary-vtable `publishCondition` at `0x0053721E` |
| `0x0052B960` | `assignByte25A` | direct call to `publishCondition` |
| `0x0052B980` | `assignByte25B` | direct call to `publishCondition` |
| `0x00525DA0` | `clearAttachedBit` | direct call to xfer callee `releaseTracked` |
| `0x0052DDB0` | `assignFlag13CBit1` | direct call to xfer callee `refreshModuleGate` |
| `0x0052D8F0` | `installResolved` | direct call to xfer callee `releaseTracked` |
| `0x0052D990` | `refreshIfVisible` | direct call to `releaseTracked` |
| `0x0052DAE0` | `assignFlag13DBit1` | direct call to `refreshIfVisible` |
| `0x0052D9E0` | `refreshSupplied` | direct call to `installResolved` |
| `0x0052DA30` | `applyAlternate` | direct call to `resolveRecord`, which `installResolved`'s neighbor and `setAttached` also call; `setAttached` calls `releaseTracked` |
| `0x005268E0` | `resolveRecord` | direct callee of `applyAlternate` and `selectRecordKey` |
| `0x0052CDD0` | `selectRecordKey` | direct call to `resolveRecord` |
| `0x0052DBD0` | `setAttachedAndRefresh` | direct call to xfer callee `releaseTracked` |

Every counted row is a vtable slot or a direct call to or from a function that is a vtable slot, a `Drawable::xfer` callee, or the `Drawable.cpp` constructor.

## Behavior the oracle checks

Straight-line leaves use two to four cases (value, boundary, incoming `0xA5A5A500` residue, sentinel bytes in the write set). Loops use six to twelve branch cases. `ucomiss`/`lahf` results keep defined AH bits. `0x0052A800` copies with `fld`/`fstp` so a QNaN stays quiet. `0x00520170` and `0x00520220` use `fsin`/`fcos`/`fcomip` and match Unicorn on the tested samples. `0x00524310` reads the logic frame from `[[0x00DEBA64]+0x58]`.

## Hooks

All four are `explicitOnly`. Stolen lengths come from `tools/analysis/disasm.py --hook-only`. The short branch after each compare is outside the stolen bytes.

| Key | RVA | Stolen | Shadow records |
| --- | --- | --- | --- |
| `drawable52tintstatic` | `0x001261A0` | 5 | `recordScalar` |
| `drawable52copytwelve` | `0x0012A800` | 6 | `recordScalar`, `recordOutput` of the 48 destination bytes. The destination is snapshotted and restored before the original runs, so an overlapping source is intact. |
| `drawable52position` | `0x00125CF0` | 7 | `recordScalar`, `recordOutput` of `PositionGlobals` at `0x00DEED68`. Globals are restored before the original runs. |
| `drawable52transform` | `0x0012D790` | 7 | `recordScalar`, `recordOutput` of `TransformGlobals` at `0x00DEEDC0`. Same restore. |

## Rejected

Thirty-seven candidates in the window were not counted:

- Twenty-three module-list walks have the `+0x178` shape and no vtable or caller edge to an anchored Drawable function: `0x00520010`, `0x00520990`, `0x00520B40`, `0x00520BD0`, `0x00520C10`, `0x00520DB0`, `0x00521090`, `0x00523BB0`, `0x00523BF0`, `0x00523EC0`, `0x00523EF0`, `0x00523F20`, `0x00523F40`, `0x00523F90`, `0x00523FC0`, `0x00523FE0`, `0x005240C0`, `0x00524110`, `0x00524500`, `0x00524530`, `0x0052BA80`, `0x0052D670`, `0x0052D730`.
- `0x0052B060` is a six-byte thunk. `0x0052B930` and `0x0052CD70` are entry-recoverable thunks, not clean bodies.
- `0x00523990` and `0x0052B0E0` are deleting destructors.
- `0x00523E60`, `0x005242B0`, `0x0052D6B0`, and `0x0052DB30` are ledger `inner-patched`.
- `0x0052BAD0`, `0x0052C3D0`, and `0x0052B2F0` are large update bodies, not accessors. They are ownership anchors only.
- `0x0052D830` calls the entry-recoverable thunk and is not an accessor.
- `0x0052DF80` has no vtable or anchor edge.
- `0x005230C0` is the middle of `0x00522D90`, not an entry.

## Deferred

The heavy player-name path inside `refreshModuleGate` is implemented and not exercised. The success path of `applyAlternate` past the virtual reject is implemented and not exercised. `0x0052D830` stays a stub. SecuROM bodies were not analyzed.
