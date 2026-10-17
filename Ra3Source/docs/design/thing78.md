# Thing and update bodies (`recon/thing78`)

## Scope

Forty clean functions whose entries lie in `0x00780000`–`0x0078FFFF` of `ra3_1.0.game`. Thing, matrix and scale bodies are `ra3::common` in `Thing78.h` / `Thing78.cpp`. Update, behavior and draw bodies are `ra3::gamelogic` in `ObjectUpdates78.h` / `ObjectUpdates78.cpp`. The oracle is `tools/analysis/ports/verify_thing78.py` against `ra3_thing78check`. There are no hooks.

Class names come from vtable slot 2 (`mov eax, imm32; ret` and the C string at that immediate). Several secondary tables in this window have a slot 2 that is not a name getter; those rows say so and do not invent a class. `0078b7a0` writes record vtable `00C284C8`. `00788650` writes tagged vtable `00C28498`. No other counted function is a constructor.

Ownership for every counted function is in `work/thing78_claims.txt`. `verify_ownership.py` reports `fails=0`.

## Counted functions

| VA | C++ name | Ownership |
| --- | --- | --- |
| `007806f0` | `ra3::gamelogic::applyTimedBonus` | `00c0dc34` slot 3; slot 2 `006ad8a0` is not a name getter |
| `00781ed0` | `ra3::gamelogic::stepBalloon` | `00c0b578` slot 5 `BalloonMoveToGroundState` |
| `00782e20` | `ra3::gamelogic::releaseAttached` | `00c01180` slot 98; slot 2 `0065f220` is not a name getter |
| `00782e70` | `ra3::gamelogic::childHasMarker` | `00c13440` slot 40; slot 2 `006cafd0` is not a name getter |
| `00783940` | `ra3::gamelogic::forwardPair` | `00bf9a40` slot 3; slot 2 `00735a90` is not a name getter |
| `00783e70` | `ra3::gamelogic::forwardIfPositive` | `00c07b48` slot 3; slot 2 `00735a90` is not a name getter |
| `007843d0` | `ra3::gamelogic::classifyPair` | `00c15200` slot 29; slot 2 `006b4190` is not a name getter |
| `00784490` | `ra3::gamelogic::probeWithMaxFloat` | `00c183f0` slot 30; slot 2 `006b4190` is not a name getter |
| `00784910` | `ra3::gamelogic::gateAndForward` | `00c00f4c` slot 20; slot 2 `0067e580` is not a name getter |
| `00786da0` | `ra3::gamelogic::gateAndForwardAlt` | `00c15200` slot 27; slot 2 `006b4190` is not a name getter |
| `007874a0` | `ra3::gamelogic::xferGatherEject` | `00c20848` slot 3 `GatherEjectUpdate` |
| `007877d0` | `ra3::gamelogic::xferSqueezeFling` | `00c225f0` slot 3 `SqueezeFlingUpdate` |
| `007879f0` | `ra3::gamelogic::xferLineSegment` | `00c28380` slot 3 `LineSegmentDamageUpdate` |
| `00788470` | `ra3::gamelogic::lineSegmentNameKey` | `00c28380` slot 4 `LineSegmentDamageUpdate` |
| `007885e0` | `ra3::gamelogic::writeModuleDataWord` | `00bdbc08` slot 5 `BehaviorModule` |
| `00788650` | `ra3::common::assignTagged` | called from `00911260` at `00911271`; `00911260` called by `W3DScriptedModelDraw` `00911910` at `0091193c` |
| `007886c0` | `ra3::common::cachedScalar` | called by `AIEnterState` `007db740` at `007db76a` |
| `007886e0` | `ra3::common::firstSlot` | called by `HighlanderBody` `006906e0` at `0069070f` |
| `00788730` | `ra3::common::templateField130` | called by `AIBuildableStructure` `008c4de0` at `008c4e5f` |
| `00788750` | `ra3::common::templatePlus230` | called from `004f5780` at `004f5b17`; `004f5780` called by `UnitAIExitGoalState` `007ab310` at `007ab3bf` |
| `00788760` | `ra3::common::field2C4` | called by `AIBuildableStructure` `008d2130` at `008d247a` |
| `00788770` | `ra3::common::scaleCount` | called by `SlaughterHordeContain` `0077caa0` at `0077caf3` |
| `00788800` | `ra3::common::scaleToFloat` | called by `0078bda0` at `0078bdb4`; `0078bda0` called by `DozerActionDoActionState` `006fe4a0` at `006fe91f` |
| `00787bd0` | `ra3::gamelogic::tickSqueeze` | called by `00787ce0` at `00787d42`; `00787ce0` is `00c22648` slot 0 |
| `0078b690` | `ra3::common::resolveDisplayName` | called by `0078de50` at `0078df81`; `0078de50` pushes `00c29210` |
| `0078b730` | `ra3::gamelogic::xferDrawModule` | `00c54498` slot 3 `DrawModule` |
| `0078b7a0` | `ra3::common::initMatrixRecord` | called from `0076b240` at `0076b269`; that caller is reached from `GameLogic` `004ede90` |
| `0078b870` | `ra3::common::directionCache` | called by `AttachUpdate` `006e6830` at `006e69ac` |
| `0078b8e0` | `ra3::common::directionWrite` | called by `AIUpdateInterface` `00785b90` at `00785ec2` |
| `0078b990` | `ra3::common::cachedHeight` | called from `006b3200` at `006b3246`; `006b3200` called by `MoveToGroundState` `0077ce90` at `0077ce94` |
| `0078ba00` | `ra3::common::transformPoint` | called by `OpenContain` `0075e110` at `0075e2b2` |
| `0078bb60` | `ra3::common::findSpanRecord` | called from `008ba680` at `008ba888`; that caller is reached from `AIBuildableStructure` `008d2130` |
| `0078bda0` | `ra3::common::scaleByClock` | called by `DozerActionDoActionState` `006fe4a0` at `006fe91f` |
| `0078bdf0` | `ra3::common::objectFlagPair` | called from `00879050` at `00879559`; `00879050` called by `SpecialAbilityUpdate` `00731e00` at `00732110` |
| `0078bf60` | `ra3::common::dispatchKeyed` | called from `0076b240` at `0076b8fc`, on the `GameLogic` chain |
| `0078bfb0` | `ra3::common::normalizeColumn` | called by `MoveInStraightLineState` `006b3270` at `006b327e` |
| `0078cb90` | `ra3::common::ensureAudioIds` | called by `0078d4f0` at `0078d4fa` |
| `0078d4f0` | `ra3::common::resolveAudioChain` | called from `005cd2b0` at `005cd3a5`; that caller is reached from `GameStateMap` `00550620` |
| `0078d520` | `ra3::common::lookupMappedValue` | called from `00740fb0` at `00740fc4`; that caller is reached from `AIUpdateInterface` `00752ab0` |
| `0078d560` | `ra3::common::selectDensestMask` | called from `00795cf0` at `00795d26`; `00795cf0` called by `HighlanderBody` `006906e0` at `006906fa` |

`00788470` is the one boilerplate row: a static NAMEKEY cache of the string at `00C283D8` (`LineSegmentDamageUpdate`). 1 of 40 is 2.5%, under the 30% cap. The slot-2 name getter `00788460` is not counted.

## What the bodies do

`firstSlot`, `templateField130`, `templatePlus230` and `field2C4` are field loads used by Highlander, buildable-structure and unit-exit callers. `assignTagged` stores id and vtable `00C28498`, and calls slot `+0x10` when the source is non-null. `cachedScalar` and `cachedHeight` fill bits 1 and 2 of the flag byte at `+0x48`. `directionCache` and `directionWrite` fill bit 0 with `fsincos` of the angle at `+0x44`. `initMatrixRecord` writes `00C284C8`, increments `[00DF9D24]`, and builds an identity when its argument is non-null. `transformPoint` applies the 3x4 at `this+8`. `normalizeColumn` normalizes the column at `+8/+18/+28` and returns the Y bits. `scaleToFloat` and `scaleByClock` are the dozer clock scale; `scaleCount` is the horde count scale. `objectFlagPair` requires bit 13 of `[[obj+4]+0xD0]` and bit 3 of `[obj+0x98]`. `findSpanRecord` walks four stride-`0x14` spans. `selectDensestMask` keeps the cursor with the greatest masked popcount; the complement tie starts at `0x3E7`. `dispatchKeyed` and `lookupMappedValue` are map lookups. `resolveDisplayName` looks up a name through `00499640`. `ensureAudioIds` and `resolveAudioChain` fill four audio id slots through `004A62E0` and `00528920`.

The gamelogic rows are module xfers (`DrawModule`, `GatherEjectUpdate`, `SqueezeFlingUpdate`, `LineSegmentDamageUpdate`), the balloon move-to-ground step, the squeeze-fling timer tick, and small predicates on secondary vtables. Xfer version locals are two bytes; the oracle compares the whole dword that sits in that stack slot. The line-segment xfer is tested on the empty circular list. The squeeze tick is tested with an empty rules range. The tail jump inside `releaseAttached` (`jmp 0077E1D0`) is not taken by the oracle.

## Dropped

- `0078e980`: `00c354f0` slot 3 holds `00800820`, not this function.
- `00785b90`: `AIUpdateInterface` slot 168, 2715 bytes. Kept only as the caller root of `directionWrite`.
- `007859e0`: `WanderAIUpdate` slot 0 of `00c25aa8`. Slot 2 is not a name getter and the body is 423 bytes.
- EASTL map and tree bodies `0078bea0`, `0078bee0`, `0078cc50`, `0078d650`, `0078dba0`, `0078e350`.
- Thunks `007886b0` and `0078b960`.
- Destructor-shaped bodies `007884c0`, `007885c0`, `00788160`.
- Thing transform bodies `0078c080`, `0078c500`, `0078c7c0`, `0078cee0`, `0078d1e0`. The matrix helpers underneath them are the counted rows.
- `0078de50`: ThingTemplate.cpp anchor, used only as the caller of `resolveDisplayName`.
- Source-path anchors `00788ab0`, `007895b0`, `007897f0`, `00789a70`, `0078a310`, `0078b4d0`: not counted.
- Large callees of the xfers (`00786a20`, `00786ba0`, `00787520`) are stubbed by the oracle rather than ported.

## Oracle and mutation

`verify_thing78.py` runs 93 cases. Release `ra3_thing78check` matches the shipped bytes: `cases=93 mismatches=0`.

Mutation: `objectFlagPair` compared bit 12 instead of bit 13. The oracle reported `mismatches=1`, case `0078bdf0` EAX `00000000` against the shipped `00000001`. The shift was restored and the oracle returned to `mismatches=0`.

## Hooks

None.
