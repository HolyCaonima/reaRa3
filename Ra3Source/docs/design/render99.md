# Render 0x0099/0x009A SageRender reconstruction

## Scope and ownership

This batch keeps 40 clean ledger entries whose starts lie in `0x00990000-0x009AFFFF`.
Each counted VA is a named-vtable slot, a slot of a secondary vtable stored by an
owned function, or a direct callee on a chain that reaches one of those slots or
one of the three source-path anchors. Class names were read from the binary:
vtable slot 2 is `mov eax, imm32; ret`, and the immediate is a C string.

| Vtable | Slot 2 string | How it is reached |
| --- | --- | --- |
| `00C5D6C0` | PartitionManager | slot 0 is `00990D90` |
| `00C5D6D0` | none (slot 2 is `004D6430`) | stored by `00990F20` |
| `00C5D764` | CollisionManager | slot 0 is `009942F0` |
| `00C5D774` | none (slot 2 is `004D6430`) | stored by `00994300` |
| `00C5DA5C` | visibleobjects - sets objects to be visible | command slots 3 and 4 |
| `00C5DAAC` | renderbin [name] - renders all objects in the named bin | command slots 3 and 4 |

Source-path anchors, each a `push` of the path string:

- `009A2660` pushes `00C5E0E8` (`ShaderFactory.cpp`) at `009A28DB`.
- `009A8FD0` pushes `00C5E270` (`MaterialFactory.cpp`) at `009A9351`.
- `009AF530` pushes `00C5E498` (`RenderObject.cpp`) at `009AF879`.

`009A8AB0`, `009AA210`, and `009AB980` are EASTL vector helpers. They are call-chain
links only and are not counted. `009AE110` is also a link only: it calls `009B9290`
into a stack record before `009ACFE0`, and that helper is outside this range.

No hooks were registered.

## Inventory

| VA | Size | Ownership | Function |
| --- | --- | --- | --- |
| `00990080` | 122 | callee of `00990260` at `009902D9`; `00C5D6D0` slot 5 via PartitionManager | `removePartitionObject` |
| `00990100` | 219 | callee of `009906E0` at `009906E4` | `clearPartitionBuckets` |
| `009901E0` | 124 | callee of `00990620` at `00990651` | `setPartitionWorldBounds` |
| `00990620` | 58 | `00C5D6D0` slot 1, stored by `00990F20` | `resetPartitionBounds` |
| `00992F70` | 61 | callee of `009939D0` at `00993A4D`; `00C5D774` slot 5 | `findWideCollisionNode` |
| `00993120` | 134 | callee of `00993B80` at `00993BA5`; CollisionManager | `unlinkCollisionEntry` |
| `00993360` | 193 | callee of `00994350` (`00C5D774` slot 5) at `00994362` | `sweepIdleCollisionNodes` |
| `00993600` | 127 | callee of `009937E0` at `0099384A` | `retireCollisionHashNode` |
| `009937E0` | 190 | callee of `009938A0` at `009938F8` | `retireCollisionObjectList` |
| `009938A0` | 129 | callee of `00993B80` at `00993B9A` | `updateCollisionRegistration` |
| `00995D00` | 800 | callee of `009A2DF0` at `009A2E01`; RenderObject.cpp | `matrix3x3ToQuaternion` |
| `00997720` | 34 | `00C5DA5C` slot 4 | `runVisibleObjectsCommand` |
| `00997EF0` | 139 | callee of `009A8AB0` at `009A8B4A` | `fillShaderParamRange` |
| `00997FD0` | 156 | callee of `009AB8E0` at `009AB94C` | `copyShaderParamsForward` |
| `009980D0` | 158 | callee of `009A8AB0` at `009A8B3C` | `copyShaderParamsBackward` |
| `009984F0` | 208 | callee of `009AB8E0` at `009AB90F` | `defaultShaderParam` |
| `00998F40` | 117 | callee of `009AA210` at `009AA26D` | `assignShaderParamRelaxed` |
| `0099A790` | 144 | `00C5DA5C` slot 3 | `parseVisibleObjectsCommand` |
| `0099A850` | 132 | `00C5DAAC` slot 4 | `runRenderBinCommand` |
| `0099A8E0` | 60 | callee of `009A2660` at `009A2942`; ShaderFactory.cpp | `readNestedNameWord` |
| `0099B010` | 61 | callee of `009A2660` at `009A29E3` | `assignRefCountedPointer` |
| `0099BD10` | 96 | callee of `009AB980` at `009ABA85` | `copySubObjectRecords` |
| `0099BD70` | 67 | callee of `009AA210` at `009AA2DE` | `copyExtendedRecords` |
| `0099BE10` | 115 | callee of `009A8AB0` at `009A8AEF` | `assignShaderParam` |
| `0099C0A0` | 60 | callee of `009A8AB0` at `009A8B2D` | `constructShaderParamRange` |
| `0099C160` | 174 | callee of `009AA210` at `009AA254` | `copyExtendedRecordsBackward` |
| `0099C290` | 420 | callee of `009A2DF0` at `009A2EF1` | `composeShaderTransform` |
| `0099C440` | 392 | callee of `009A2DF0` at `009A2F03` | `composeShaderTransformInPlace` |
| `0099F1C0` | 217 | callee of `009A8FD0` at `009A8FF1`; MaterialFactory.cpp | `resolveRegisteredShader` |
| `0099F310` | 184 | callee of `009A2660` at `009A2A03` | `bindShaderConstantIds` |
| `009A0500` | 45 | callee of `009A8AB0` at `009A8B65` | `replicateShaderParam` |
| `009A0640` | 150 | callee of `009AB980` at `009AB9DF` | `moveSubObjectRecordsBackward` |
| `009A1990` | 219 | `00C5DAAC` slot 3 | `parseRenderBinCommand` |
| `009A2540` | 281 | callee of `009A2660` at `009A28A7` | `shaderSemanticNames` |
| `009A2DF0` | 308 | callee of `009A82B0` at `009A82C0` | `refreshShaderParamChain` |
| `009A82B0` | 626 | callee of `009AF530` at `009AF8FE` | `skinShaderInstances` |
| `009A8EA0` | 290 | callee of `009A8FD0` at `009A90BF` | `bindShaderAsset` |
| `009AB8E0` | 157 | callee of `009ACB60` at `009ACBA8` | `resizeShaderParamVector` |
| `009ACB60` | 243 | callee of `009AF530` at `009AF659` | `importShaderParamBlock` |
| `009ACFE0` | 85 | callee of `009AE110` at `009AE15C` | `appendSubObjectRecord` |

Boilerplate count is 0. None of the 40 is a slot-2 name getter, a static NAMEKEY
cache, or a return-constant stub. The two smallest bodies still do work:
`00997720` forwards three parsed words through virtual `+0x6C`, and `009A0500`
replicates a 0x48-byte parameter.

## What the bodies do

PartitionManager's secondary table `00C5D6D0` is written by `00990F20`.
`00990620` (slot 1) resets the inner partition. On an empty span, `00990100`
leaves `eax` at `this+0x18`. `009901E0` writes the six world-bound floats and
returns the bounds pointer. `00990080` unlinks one object from its cell.

CollisionManager's secondary table `00C5D774` is written by `00994300`.
`00992F70` hashes `(key[2] << 16) + key[3]` with modulus `0x2B7B` and walks
the chain at `+0x30`. `00993600` uses modulus `0x493` on the same key shape.
`00993120` unlinks an entry through the previous pointer at `+0x0C`.
`00993360` scans `0x493` buckets and leaves `eax` equal to `0x493`.

Shader parameters are 0x48-byte records. `009984F0` fills the default,
`0099BE10` and `00998F40` assign one, and the copy helpers move ranges forward
or backward. `009AB8E0` on the truncate path (`count` not greater than the
current size) adjusts `[vector+4]` by the signed multiple of 72 produced with
magic `0xC71C71C7` and leaves that delta in `eax`. The grow path calls EASTL
`009A8AB0` and is not part of the oracle. `00995D00` converts a 3x3 to a
quaternion. `0099C290` and `0099C440` compose a rigid transform; the in-place
form leaves the written `w` bits in `eax`. `009A2DF0` rewrites a pose chain,
and `009A82B0` skins the instance list after refreshing the chain at `+0x74`.

`0099F1C0` resolves a shader from the registry span at `00E1A588`/`00E1A58C`
when the ready byte at `00E1B318` is set. `009A2540` returns the cached
semantic string at `00E1B31C` when `00E1B320` is ready and the length prefix
is non-zero. `0099F310` writes the two constant ids and returns the inner
pointer. `009A8EA0`'s tested path is the failed lookup, which releases the
asset and returns false.

## Dropped candidates

- `009AE110` calls `009B9290` to fill a stack record, then `009ACFE0`. `009B9290` is outside this range, so the body was not counted. It remains the call-chain link for `009ACFE0`, `0099BD10`, and `009A0640`.
- `009A8AB0`, `009AA210`, `009AB980` are EASTL growth helpers. Counted callees hang off them; the helpers themselves were not ported.
- `00993020` and `009930B0` walk every bucket (`0x2B7B` and `0x493`) and move each node onto a free list. Free-list movers were skipped.
- `00993060` unlinks one node onto the free list at `+0xADEC`. Free path, skipped.
- `0099E400` releases the two pointers at `this+4` and `this+8`. Release-only, skipped.
- `009A0450` releases the ref at `+8` across a `[first, last)` range of 0x14-byte records. Release-only, skipped.

## Oracle and mutation

`tools/analysis/ports/verify_render99.py` runs the Release checker
`ra3_render99check` against Unicorn on the shipped image. Every counted
function has more than one case, including empty ranges, a zero key, a
refcount of 1, and the truncate edges of the parameter vector.

Result after the chain-offset restore:

```
render99 oracle: 40 entries, 85 cases, 0 mismatches
```

Mutation: `findWideCollisionNode` read the chain link at `+0x34` instead of
`+0x30`. The oracle reported

```
FAIL 0x00992F70 hit ret cpp=0x00000000 emu=0x00000030
render99 oracle: 40 entries, 85 cases, 1 mismatches
```

The offset was restored and the oracle returned to 0 mismatches. `00990620`
leaves a stack temporary in `eax`; that return is not compared. The memory
image is compared.

Hooks: none.
