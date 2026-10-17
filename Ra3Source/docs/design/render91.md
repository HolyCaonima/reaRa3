# Render 0x008E/0x0091 W3D draw-module reconstruction

## Scope and ownership

This batch keeps 36 clean ledger entries from the W3D draw-module cluster.
Each counted VA is either a vtable slot whose pointer was read from `.rdata`,
or a direct callee of such a slot (or of `0x0090CE30`, which references the
assertion string at `0x00C54190`). Class names below were read from the
binary: vtable slot 2 is `mov eax, imm32; ret` and the immediate is the
class-name string.

| Vtable | Slot 2 string |
| --- | --- |
| `00C54698` | W3DLaserDraw |
| `00C54790` | W3DPropDraw |
| `00C54890` | W3DStreamDraw |
| `00C54988` | W3DTracerDraw |
| `00C55668` | W3DScriptedModelDraw |
| `00C557E8` | W3DTankDraw |
| `00C55968` | W3DTruckDraw |
| `00C55B00` | W3DDynamicsDraw |
| `00C55CA8` | W3DSpotlightDraw |

`0x009108F0` stores `00C55668` at `[this]` and `00C555F0` at `[this+0x14]`.
The 29-slot table `00C555F0` is the secondary vtable of W3DScriptedModelDraw.
Its own slot 2 (`0x008E9540`) does not return a name. The same pairing is
present for the tank, truck, dynamics, and spotlight primary tables.
`0x00C54190` is the assertion path
`W3DScriptedModelDraw.cpp`.

Thirty-four drafted VAs were not counted. `0x00910EC0` is one body at
W3DScriptedModelDraw `00C55668` slots 44 and 45 and at
LargeGroupAudioClientUpdate `00C503E0` slots 13 and 14 (slot 2 string read
from that table). The other 33 are not vtable slots and are not direct
callees of a vtable method or of `0x0090CE30`.

## Inventory

| VA | Size | Class and slot | Function | Cases |
| --- | --- | --- | --- | --- |
| `008E52A0` | 13 | W3DDynamicsDraw `00C55B00` slot 59 | `setDynamicsDrawWord2FC` | 2 |
| `008E5690` | 26 | W3DScriptedModelDraw `00C55668` slot 17 | `releaseScriptedModelTerrainDecal` | 2 |
| `008E5A60` | 47 | W3DLaserDraw `00C54698` slot 3 (also prop/stream/tracer) | `notifySimpleDrawBoundToObject` | 2 |
| `008E8900` | 89 | direct callee of `008ED6F0` (`00C55668` slot 64) at `008ED706` | `getFirstAnimationName` | 3 |
| `008E8D50` | 78 | direct callee of `0090CE30` at `0090D097` | `isAnimationLayerComplete` | 12 |
| `008E8DC0` | 36 | `00C55668` slot 5 | `getScriptedModelModuleKeyOrOverride` | 2 |
| `008E8E90` | 47 | `00C55668` slot 23 | `updatePrimaryShadowTransform` | 2 |
| `008E8EC0` | 20 | `00C55668` slot 24 | `updatePrimaryShadowVisibility` | 2 |
| `008E8FD0` | 37 | `00C55668` slot 26 | `releasePrimaryShadow` | 3 |
| `008E9000` | 18 | `00C55668` slot 29 | `updateAuxShadowModule200` | 2 |
| `008E9020` | 47 | `00C55668` slot 30 | `updateSecondaryShadowTransform` | 2 |
| `008E9050` | 20 | `00C55668` slot 31 | `updateSecondaryShadowVisibility` | 2 |
| `008E9240` | 37 | `00C55668` slot 34 | `releaseSecondaryShadow` | 2 |
| `008E9270` | 37 | `00C55668` slot 35 | `releaseAuxShadowModule200` | 2 |
| `008E93B0` | 126 | secondary `00C555F0` slot 15 | `applyScriptedModelHouseColor` | 3 |
| `008EDA20` | 260 | direct callee of `008EE8E0` at `008EE954` | `propagateSubObjectBoneVisibility` | 4 |
| `008EDB30` | 227 | direct callee of `008F70E0` (`00C555F0` slot 16) at `008F72B1` | `applyNamedSubObjectVisibility` | 2 |
| `008EE860` | 120 | direct callee of `008EE990` at `008EEA0B` | `applySubObjectOpacityAndRelease` | 5 |
| `008EE8E0` | 162 | direct callee of `008F3D60` at `008F3DCD` | `applySubObjectHideState` | 2 |
| `008EE990` | 409 | direct callee of `008F3D60` at `008F3E2F` | `updateSubObjectFadeAndVisibility` | 2 |
| `008EEED0` | 148 | direct callee of `0090CE30` at `0090CED3` | `resetAnimationLayerSlot` | 2 |
| `008F0B90` | 82 | direct callee of `008F5CB0` at `008F5CBF` | `rbTreeEqualRangeIntKey` | 4 |
| `008F2700` | 69 | direct callee of `008F4290` (`00C557E8` slot 19) at `008F42AC` | `stopAttachedParticleSystemsInVector` | 3 |
| `008F2C10` | 79 | direct callee of `008F2C60` (`00C55668` slot 19) at `008F2CE2` | `updateAttachedParticleSystemsVisibility` | 4 |
| `008F3D60` | 223 | secondary `00C555F0` slot 23 | `updateScriptedModelSubObjects` | 3 |
| `008F5CB0` | 60 | direct callee of `008F6ED0` at `008F6F18` | `rbTreeCountIntKey` | 4 |
| `008F6ED0` | 89 | secondary `00C555F0` slot 18 | `getSubObjectBoneCountForState` | 5 |
| `008F95D0` | 108 | direct callee of `0090D780` (`00C55668` slot 3) at `0090D9B9` | `eraseSubObjectRecords28Range` | 3 |
| `00910E50` | 10 | `00C55668` slot 12 | `setScriptedModelFlag6D` | 3 |
| `00910E60` | 10 | `00C55668` slot 57 | `getScriptedModelDataFlagD3` | 2 |
| `00910E70` | 27 | `00C55668` slot 22 | `isScriptedModelActive6DOrDataD4` | 4 |
| `00910E90` | 10 | `00C55668` slot 20 | `setScriptedModelFlag6C` | 3 |
| `00910EB0` | 13 | `00C55668` slot 56 | `setScriptedModelFlag216` | 3 |
| `00910ED0` | 11 | `00C55668` slot 10 (also slot 11) | `getScriptedModelSubInterface18` | 2 |
| `00910EE0` | 13 | `00C55668` slot 55 | `setScriptedModelFlag215` | 3 |
| `00911250` | 14 | W3DDynamicsDraw `00C55B00` slot 48 (also slot 49) | `getDynamicsDrawSubInterface290` | 2 |

The same `00C55668` slot is also installed on `00C557E8`, `00C55968`,
`00C55B00`, and `00C55CA8` for the shared scripted-model methods. Each body
is counted once.

## Layouts

- Dynamics word written by `008E52A0` is the dword at `+0x2FC`.
- Terrain decal pointer is `+0x74`. Primary shadow is `+0x1F0`, secondary
  shadow `+0x1F4`, aux shadow module `+0x200`. Simple-object refcount is
  `+4`. A release that reaches 0 calls vtable slot 0 with no extra argument.
- House-color compare runs on the secondary interface (`this` is
  object`+0x14`). The module flag is `module+0xC4`. The pass mask is 16
  bytes at interface`+0x30`.
- Sub-object opacity is the float at `subObject+0x50`. Values `<= 0` store
  0, values `>= 1` store 1, and the open interval stores the value and sets
  draw`+0x1E9`.
- Particle stop walks an 8-byte pair vector and skips the sentinel handle
  `0x00DF88F8`. The visibility list head is embedded at draw`+0x7C`.
- Integer tree: root at `header+0x0C`, sentinel identity `header+4`, key at
  `node+0x10`.
- `008F95D0` erases a half-open range of 28-byte records and slides the tail
  with `memcpy`. The same body is also called from `00BE431C` slot 1; it is
  counted once via the scripted-model call.

## Oracle and mutation

`ra3_render91check` and `verify_render91.py` compare the clean game bytes
with the reconstruction on one relocated blob. Release result:

`render91 oracle: 36 entries, 108 cases, 0 mismatches`

Mutation: `setDynamicsDrawWord2FC` stored at `+0x2F8` instead of `+0x2FC`.
The oracle failed on `0x008E52A0` (`blob mismatch at +0x2F8`, game `0xA5`,
cpp `0x44`). The offset was restored, the checker rebuilt, and the oracle
returned to 0 mismatches.

## Hooks

No hook keys. The counted virtuals include releases and a refcount path, and
the remaining bodies are not a clearly safe explicit-only detour.

| Key | VA | Expected bytes | explicitOnly |
| --- | --- | --- | --- |
| (none) | | | |

## Deferred

Not counted, because ownership is not a vtable slot or a direct callee of an
anchored VA:

`008E57E0 008E5E80 008E6060 008E6180 008E6260 008E8550 008E8960 008EB120
008EB360 008EB490 008EB4E0 008EB540 008EB5E0 008EC560 008ED690 008EF1A0
008F0A90 008F0B30 008F0DF0 008F0F30 008F1010 008F1120 008F3EA0 008F3FA0
008F6180 008F61B0 008F6690 008F6900 008F6C00 008FA6D0 008FA790 008FAB20
00907AB0`

`00910EC0` is deferred as a shared body with LargeGroupAudioClientUpdate.
`00C555F0` slot 2 does not return a class name; the class is taken from the
primary table stored beside it. `008F95D0` is also reached from `00BE431C`
slot 1, whose slot 2 is not a class-name return.
