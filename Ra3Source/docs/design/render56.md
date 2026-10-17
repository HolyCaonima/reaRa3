# W3D Terrain & Environment Render Cluster (`0x005612F0–0x0058D1B0`)

## Scope and Anchors

This batch (`recon/render56`) reconstructs **56** clean, non-ICF functions in the W3D terrain and environment rendering subsystem inside `0x00560000–0x0059FFFF` of `ra3_1.0.game`. Every function has `securom=clean` in `function_ledger.tsv`, zero `.data`/`.bss` global dependencies, zero IAT imports, and a direct ownership proof anchored to vtable slots in `vtables_split.tsv` and/or call-sites traced from vtable methods and source-file assertion anchors (`d:\projects\ra3\code\gameengine\source\gameclient\water.cpp`, `worldheightmap.cpp`, `w3droadbuffer.cpp`, `w3dterrainvisual.cpp`).

### Primary Vtable & Source Anchors

| Subsystem Class | Vtable Start (`vtables_split.tsv`) | Key Anchors |
| --- | --- | --- |
| `W3DTerrainVisual` | `0x00be38e8` (`len=41`) | Slots 8 (`0x0056b290`), 9 (`0x00561d70`), 11 (`0x005882c0`), 18 (`0x00561e80`), 19 (`0x0056b2f0`), 20 (`0x00561db0`), 21 (`0x00561e10`), 22 (`0x0056b380`), 23 (`0x00561e70`) |
| `W3DTerrainLogic` | `0x00be39f8` (`len=35`) | Slots 10 (`0x0056ec10`), 11 (`0x0056ecc0`), 14 (`0x0056edb0`), plus direct callees of slots 18 and 21–27 |
| `BaseHeightMapRenderObjClass` | `0x00be4290` (`len=58`) | Slot 14 (`0x00565250`), plus direct callees of slot 1 (`0x00908ed0`), slot 2 (`0x008f17e0`), slot 9 (`0x00591e00`), and slot 15 (`0x00570e60`) |
| `WorldHeightMap` | `0x00be4e2c` (`len=18`) | Direct callees of slot 2 (`0x005a1700`), `W3DTerrainVisual` slots, `W3DTerrainLogic` slots, and `BaseHeightMapRenderObjClass` vertex/index update (`0x00580a90`, `0x00580fb0`) |
| `W3DScorchBuffer` | `0x00c54618` (caller) | `0x005615a0` (direct callee of `0x008fd1e0`, `0x00c54618` slot 11) and `0x0056ab00` (direct callee of `0x008e51a0`, called by `W3DTerrainVisual` slot 17 `0x00561f00`) |
| `W3DRoadBuffer` / `RoadSegment` | `0x00be4290` slot 1 callees | `0x005612f0`, `0x00561340`, `0x00568ea0`, `0x00569480`, `0x00569570`, `0x005695e0`, `0x00569750`, `0x0056a8a0` |
| `W3DPropBuffer` | `0x00be4d60` (`len=1`) | `0x0058d1b0` sets vtable `0x00be4d60` at `[this+0x00]` (direct callee of `0x008ec630` at `0x008ec795`) |

Generals / Zero Hour reference files used for structural comparison only (all C++ implementations are original and reconstructed from `ra3_1.0.game` machine code):
- `work/references/Generals/Code/GameEngineDevice/Source/W3DDevice/GameClient/W3DTerrainVisual.cpp`
- `work/references/Generals/Code/GameEngineDevice/Source/W3DDevice/GameClient/WorldHeightMap.cpp`
- `work/references/Generals/Code/GameEngineDevice/Source/W3DDevice/GameClient/BaseHeightMap.cpp`
- `work/references/Generals/Code/GameEngineDevice/Source/W3DDevice/GameClient/W3DRoadBuffer.cpp`
- `work/references/Generals/Code/GameEngineDevice/Source/W3DDevice/GameClient/W3DPropBuffer.cpp`

## Reconstructed Functions (56 Accepted)

| VA | C++ Function (`ra3::render`) | Ownership Proof |
| --- | --- | --- |
| `0x005612f0` | `clearRoadTypeLinks` | `direct callee of 0x00569570 at 0x00569571` |
| `0x00561340` | `adjustRoadStacking` | `direct callee of 0x00585900 at 0x00585e6b (direct callee of 0x005a7a60 at 0x005a7a93, direct callee of 0x00908ed0 at 0x00908f1b, 0x00be4290 slot 1)` |
| `0x005615a0` | `clearAllScorches` | `direct callee of 0x008fd1e0 at 0x008fd206 (0x00c54618 slot 11)` |
| `0x00561d70` | `queryTerrainVisualSubobjectSlot6` | `0x00be38e8 slot 9` |
| `0x00561db0` | `getRawMapHeight` | `0x00be38e8 slot 20` |
| `0x00561e10` | `partialMapUpdate` | `0x00be38e8 slot 21` |
| `0x00561e70` | `getTerrainVisualBorderSize` | `0x00be38e8 slot 23` |
| `0x00561e80` | `removeAllBibs` | `0x00be38e8 slot 18` |
| `0x00562bb0` | `setFlipState` | `direct callee of 0x00580a90 at 0x00580c72 (callee of 0x0058bbc0 at 0x0058bc13, direct callee of 0x00591e00 at 0x00591f6d, 0x00be4290 slot 9)` |
| `0x00562d30` | `countTilesFromTgaHeader` | `direct callee of 0x0058a460 at 0x0058a613 (direct callee of 0x005a1700 at 0x005a1e1e, 0x00be4e2c slot 2)` |
| `0x00562df0` | `getTerrainColorAt` | `direct callee of 0x0056b290 at 0x0056b2ae (0x00be38e8 slot 8)` |
| `0x00562f10` | `evaluateBlendTileCoeffs` | `direct callee of 0x0056e0a0 at 0x0056e1ea` |
| `0x00563520` | `initStreamLockHelper` | `direct callee of 0x00580a90 at 0x00580bbd (callee of 0x0058bbc0 at 0x0058bc13, direct callee of 0x00591e00 at 0x00591f6d, 0x00be4290 slot 9)` |
| `0x005635f0` | `vec3SignMaskMin` | `direct callee of 0x00564f80 at 0x00564ff6` |
| `0x005636e0` | `vec3SignMaskMax` | `direct callee of 0x00564f80 at 0x00565048` |
| `0x00563980` | `orderCoord3DPair` | `direct callee of 0x0056ef10 at 0x0056ef57 (0x00be39f8 slot 18)` |
| `0x005642b0` | `getCellFlagBit78` | `direct callee of 0x00580a90 at 0x00580c9b (callee of 0x0058bbc0 at 0x0058bc13, direct callee of 0x00591e00 at 0x00591f6d, 0x00be4290 slot 9)` |
| `0x00564300` | `getCellFlagBit88` | `direct callee of 0x0056f180 at 0x0056f209 (0x00be39f8 slot 27)` |
| `0x00564e70` | `aaboxFromMinMax` | `direct callee of 0x00580a90 at 0x00580f12 (callee of 0x0058bbc0 at 0x0058bc13, direct callee of 0x00591e00 at 0x00591f6d, 0x00be4290 slot 9)` |
| `0x00564f20` | `minMaxAABoxMergePoint` | `direct callee of 0x00580a90 at 0x00580d23 (callee of 0x0058bbc0 at 0x0058bc13, direct callee of 0x00591e00 at 0x00591f6d, 0x00be4290 slot 9)` |
| `0x00564f80` | `minMaxAABoxMergeCenterExtentBox` | `direct callee of 0x00570e60 at 0x00570f20 (0x00be4290 slot 15)` |
| `0x00565130` | `packCompressedTerrainVertex` | `direct callee of 0x00580a90 at 0x00580d12 (callee of 0x0058bbc0 at 0x0058bc13, direct callee of 0x00591e00 at 0x00591f6d, 0x00be4290 slot 9)` |
| `0x00565250` | `clearPatchVisibilityAndBounds` | `0x00be4290 slot 14` |
| `0x00565700` | `copyTerrainRenderState52` | `direct callee of 0x008f17e0 at 0x008f18f3 (0x00be4290 slot 2)` |
| `0x00568ea0` | `initRoadSegment` | `direct callee of 0x00578150 at 0x00578219 (direct callee of 0x005a7a60 at 0x005a7a77, 0x00be4290 slot 1)` |
| `0x00569480` | `copyRoadSegment` | `direct callee of 0x00578150 at 0x0057834c (direct callee of 0x005a7a60 at 0x005a7a77, 0x00be4290 slot 1)` |
| `0x00569570` | `linkRoadsToRoadTypes` | `direct callee of 0x005a7a60 at 0x005a7aa1 (direct callee of 0x00908ed0 at 0x00908f1b, 0x00be4290 slot 1)` |
| `0x005695e0` | `updateRoadSegmentCounts` | `direct callee of 0x00577cc0 at 0x00577e6d (direct callee of 0x00578150 at 0x00578315, direct callee of 0x005a7a60 at 0x005a7a77, 0x00be4290 slot 1)` |
| `0x00569750` | `updateAllRoadCountsAndFlags` | `direct callee of 0x005a7a60 at 0x005a7a7e (direct callee of 0x00908ed0 at 0x00908f1b, 0x00be4290 slot 1)` |
| `0x0056a8a0` | `rotateRoadPointAbout` | `direct callee of 0x005861e0 at 0x00586a19 (direct callee of 0x0058d810 at 0x0058d888, direct callee of 0x005a7a60 at 0x005a7a8c, 0x00be4290 slot 1)` |
| `0x0056ab00` | `invalidateScorchesInRadius` | `direct callee of 0x008e51a0 at 0x008e51b4 (direct callee of 0x00561f00 at 0x00561f14, 0x00be38e8 slot 17)` |
| `0x0056b290` | `getTerrainVisualColorAt` | `0x00be38e8 slot 8` |
| `0x0056b2f0` | `setRawMapHeightAndFlags` | `0x00be38e8 slot 19` |
| `0x0056b380` | `getCellFlags` | `0x00be38e8 slot 22` |
| `0x0056d970` | `getCliffState` | `direct callee of 0x008e4c30 at 0x008e4c9f (direct callee of 0x00563400 at 0x00563418, 0x00be39f8 slot 21)` |
| `0x0056d9d0` | `getCellFlagBit98` | `direct callee of 0x008e4cb0 at 0x008e4d1f (direct callee of 0x00563420 at 0x00563438, 0x00be39f8 slot 24)` |
| `0x0056da40` | `getCellFlagBitB8` | `direct callee of 0x008e4d30 at 0x008e4d9f (direct callee of 0x00563440 at 0x00563458, 0x00be39f8 slot 25)` |
| `0x0056dab0` | `getCellFlagBitA8` | `direct callee of 0x008e4db0 at 0x008e4e1f (direct callee of 0x00563460 at 0x00563478, 0x00be39f8 slot 26)` |
| `0x0056db20` | `getCellFlagBitD8` | `direct callee of 0x008e4e30 at 0x008e4e98 (direct callee of 0x00563480 at 0x00563498, 0x00be39f8 slot 22)` |
| `0x0056db80` | `getCellFlagBitC8` | `direct callee of 0x008e4eb0 at 0x008e4f18 (direct callee of 0x005634a0 at 0x005634b8, 0x00be39f8 slot 23)` |
| `0x0056dc30` | `setCliffState` | `direct callee of 0x0056dfb0 at 0x0056e086` |
| `0x0056dd70` | `isCliffMappedTextureCheck` | `direct callee of 0x0057db70 at 0x0057dbb1` |
| `0x0056de70` | `getBlendVertexAlphaMask` | `direct callee of 0x00580fb0 at 0x00581286 (callee of 0x0058bbc0 at 0x0058bc1c, direct callee of 0x00591e00 at 0x00591f6d, 0x00be4290 slot 9)` |
| `0x0056dfb0` | `setCellCliffFlagFromHeights` | `direct callee of 0x0057e4e0 at 0x0057e504` |
| `0x0056e0a0` | `getCellBlendClassesAndCoeffs` | `direct callee of 0x00580a90 at 0x00580c55 (callee of 0x0058bbc0 at 0x0058bc13, direct callee of 0x00591e00 at 0x00591f6d, 0x00be4290 slot 9)` |
| `0x0056ec10` | `getTerrainLogicExtent` | `0x00be39f8 slot 10` |
| `0x0056ecc0` | `getMaximumPathfindExtent` | `0x00be39f8 slot 11` |
| `0x0056edb0` | `getSmallestTerrainExtent` | `0x00be39f8 slot 14` |
| `0x00570d50` | `recalculateLineSeg` | `direct callee of 0x005882c0 at 0x00588364 (0x00be38e8 slot 11)` |
| `0x00572070` | `initRayCollisionTest` | `direct callee of 0x005882c0 at 0x0058837d (0x00be38e8 slot 11)` |
| `0x0057a140` | `initWaterWaveGridState` | `direct callee of 0x00587ff0 at 0x00588039 (direct callee of 0x005883c0 at 0x00588408, 0x00be38e8 slot 12)` |
| `0x0057db70` | `shouldFlipCellQuad` | `direct callee of 0x00580a90 at 0x00580c5f (callee of 0x0058bbc0 at 0x0058bc13, direct callee of 0x00591e00 at 0x00591f6d, 0x00be4290 slot 9)` |
| `0x0057e4e0` | `initCliffFlagsFromHeights` | `direct callee of 0x005a1700 at 0x005a1c15 (0x00be4e2c slot 2)` |
| `0x00580690` | `setLineSegPoints` | `direct callee of 0x005861e0 at 0x00586653; calls 0x00570d50 at 0x005806bc` |
| `0x005882c0` | `intersectTerrain` | `0x00be38e8 slot 11` |
| `0x0058d1b0` | `initPropBuffer` | `direct callee of 0x008ec630 at 0x008ec795 (direct callee of 0x005806d0 at 0x005806d9, direct callee of 0x00588410 at 0x00588464, 0x00be38e8 slot 6)` |

## Rejected & Deferred Candidates

- **Rejected (`0x00562b70`, 1 candidate)**: `0x00562b70` (`33 C0 C3`, `xor eax, eax; ret`) is an ICF-folded vtable slot body shared across `0x00be5cbc` slot 3 (`W3DWaterRenderer`), `0x00c2bcd8` slot 3 (`AIGuardGoingIdleState`), and `0x00c2d168` slot 3 (`AIGoingIdleState`). Excluded from the counted set per the ICF/identity-stub policy.
- **Deferred (`0x00561f00`, `0x00563400`, `0x00563420`, `0x00563440`, `0x00563460`, `0x00563480`, `0x005634a0`, 7 candidates)**: Thin forwarding thunks on `W3DTerrainVisual` (`0x00be38e8` slot 17) and `W3DTerrainLogic` (`0x00be39f8` slots 21–26) that jump through `0x008e4c30–0x008e51a0` into the underlying `WorldHeightMap` / `W3DScorchBuffer` methods already reconstructed in this batch.

## Optional Hook Targets (`Render56Targets.cpp`)

All 11 hooks registered in `Ra3Source/src/hook/dll/targets/Render56Targets.cpp` are pure, non-lifecycle, non-x87 scalar/predicate queries with whole-instruction position-independent stolen prefixes (`>= 5` bytes) and `explicitOnly = true`.

| Key | VA | RVA | Expected 16 Bytes | Stolen | explicitOnly |
| --- | --- | --- | --- | --- | --- |
| `tvbordersize` | `0x00561E70` | `0x00161E70` | `8B 41 30 85 C0 74 04 8B 40 2C C3 33 C0 C3 CC CC` | 5 | `true` |
| `hmcellflag78` | `0x005642B0` | `0x001642B0` | `56 8B F1 8B 4C 24 08 32 C0 3B 4E 24 73 3A 57 8B` | 7 | `true` |
| `hmcellflag88` | `0x00564300` | `0x00164300` | `56 8B F1 8B 4C 24 08 32 C0 3B 4E 24 73 43 57 8B` | 7 | `true` |
| `tvcellflags` | `0x0056B380` | `0x0016B380` | `8B 49 30 8B 51 2C 8B 44 24 08 03 C2 0F AF 41 24` | 6 | `true` |
| `hmcliffstate` | `0x0056D970` | `0x0016D970` | `56 8B F1 8B 4C 24 08 85 C9 7C 47 8B 54 24 0C 85` | 7 | `true` |
| `hmcellflag98` | `0x0056D9D0` | `0x0016D9D0` | `56 8B F1 8B 4C 24 08 85 C9 7C 50 8B 54 24 0C 85` | 7 | `true` |
| `hmcellflagb8` | `0x0056DA40` | `0x0016DA40` | `56 8B F1 8B 4C 24 08 85 C9 7C 50 8B 54 24 0C 85` | 7 | `true` |
| `hmcellflaga8` | `0x0056DAB0` | `0x0016DAB0` | `56 8B F1 8B 4C 24 08 85 C9 7C 50 8B 54 24 0C 85` | 7 | `true` |
| `hmcellflagd8` | `0x0056DB20` | `0x0016DB20` | `56 8B F1 8B 4C 24 08 85 C9 7C 49 8B 44 24 0C 85` | 7 | `true` |
| `hmcellflagc8` | `0x0056DB80` | `0x0016DB80` | `56 8B F1 8B 4C 24 08 85 C9 7C 49 8B 44 24 0C 85` | 7 | `true` |
| `hmshouldflipquad` | `0x0057DB70` | `0x0017DB70` | `53 55 8B 6C 24 10 56 8B F1 8B 86 4C 71 01 00 8B` | 6 | `true` |

## Verification Summary

- **Differential Oracle**: `python Ra3Source/tools/analysis/ports/verify_render56.py` runs `ra3_render56check.exe` (Release) against Unicorn emulation of `ra3_1.0.game` across **290 cases** covering all **56/56** functions with **0 mismatches**, plus **2/2** deliberate semantic mutations caught.
- **Hook Integration Test**: `ctest -C Release -R hook_integration` passes.
