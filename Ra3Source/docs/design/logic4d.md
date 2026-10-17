# GameLogic & Core 0x004D Cluster (`recon/logic4d`)

## Scope and Ownership Evidence

This batch reconstructs **58 clean (`securom=clean`) functions** in `0x004D0000..0x004DFFFF` (`1` in `ra3::core` and `57` in `ra3::gamelogic`). Every function was verified against `D:\Game\Ra3\Game\Data\ra3_1.0.game` by `work/probe/verify_ownership.py` (`fails=0`) and by the Unicorn differential oracle `Ra3Source/tools/analysis/ports/verify_logic4d.py` (`58 entries, 258 cases, 0 mismatches`). Boilerplate getters/initializers account for only 5 of 58 functions (`8.6%`).

### Subsystems Reconstructed

1. **Indexed Text File Line Reader (`ra3::core`, `0x004D05A0`)**:
   - `getTextFileLine4D` (`0x004D05A0`): Indexes into the 12-byte line entry vector `[this+0x04..+0x08)` using signed magic-multiply division by 12 (`0x2AAAAAAB`) and returns `entry[index].text` when `index < count`, else `nullptr`.
2. **GhostObject Intrusive Circular List & ObjectID Pair Helpers (`ra3::gamelogic`, `0x004D2AD0..0x004D3750`, `0x004DAD10`)**:
   - `relinkGhostObjectNodeBefore4D` (`0x004D2AD0`): Unlinks a circular doubly-linked list node if not self-linked and inserts it before `targetNode`.
   - `equalsObjectIDPair4D` (`0x004D3750`): 8-byte ObjectID pair equality comparison.
   - `findRegisteredPairByKey4D` (`0x004DAD10`): Searches the circular doubly-linked list at `gameLogic + 0x1B0` for a 64-bit key via `0x004D3750`, returning `node + 8`.
3. **Angle Math, Frame Conversion, Cooldown Comparison & Z-Rotation Matrix (`ra3::gamelogic`, `0x004D3770`, `0x004D4020`, `0x004D4500`, `0x004D57A0`, `0x004D9230`)**:
   - `normalizeAngleRad4D` (`0x004D3770`): Wraps radian angles into `(-\pi, \pi]` using `2\pi` steps (`0x40C90FDB`).
   - `convertSecondsToFrames4D` (`0x004D4020`): Converts seconds at `*(*holder + 0x08)` to integer logic frames using global frame rate `0x00DEEEE8` and optional `GameLogic` time scale at `+0x1C4`, truncating via x87 `fistp` (`cw | 0x0C00`).
   - `compareCooldownProgress4D` (`0x004D4500`): Compares scaled remaining cooldown frames (`+0xA0` target frame, `+0x38` scale) between two records.
   - `snapAngleToCardinalRad4D` (`0x004D57A0`): Snaps radian angle in-place to the nearest cardinal multiple of `\pi/2` (`0`, `\pi/2`, `\pi`, `-\pi/2`).
   - `buildZRotationMatrix4D` (`0x004D9230`): Populates a `3x3` row-major Z-rotation matrix (`36` bytes) using `fsincos`.
4. **BitFlags (288-bit KindOf, 480-bit ModelCondition, 224-bit ObjectStatusMask, Embedded Bitsets) & Filters (`ra3::gamelogic`, `0x004D5900..0x004D5BD0`, `0x004D6680..0x004D66A0`, `0x004D8780..0x004D8810`, `0x004D9280`)**:
   - `anyBitFlags288_4D` (`0x004D5900`), `testRequiredAndForbiddenBitFlags288_4D` (`0x004D5920`), `countSetBits288_4D` (`0x004D9280`): 9-word (`288`-bit) bitset queries and SWAR popcount.
   - `intersectsBitFlags480_4D` (`0x004D5A40`), `anyBitFlags480_4D` (`0x004D5AB0`), `equalsBitFlags480_4D` (`0x004D5B10`): 15-word (`480`-bit) bitset operations.
   - `anyObjectStatusMask224_4D` (`0x004D5BB0`), `intersectsObjectStatusMask224_4D` (`0x004D5BD0`), `matchesObjectStatusFilter4D` (`0x004D8780`), `matchesRelationshipFilter4D` (`0x004D8810`): 7-word (`224`-bit) `ObjectStatusMask` and relationship filter checks.
   - `anyBitAt26C4D` (`0x004D6680`), `testBitAt26C4D` (`0x004D66A0`): Embedded bitset queries at `+0x26C`.
5. **DamageInfoInput / DamageInfoShort / DOTInfoRecord & SpecialAbility / Campaign / AI Helpers (`ra3::gamelogic`)**:
   - `assignDamageInfoInputPayload4D` (`0x004D4400`), `copyConstructDamageInfoInput4D` (`0x004D5DF0`), `defaultConstructDamageInfoInput4D` (`0x004D6550`), `assignDamageInfoShort4D` (`0x004D6FF0`), `assignDOTInfoRecord4D` (`0x004DA690`).
   - `getBehaviorModuleClassNameVa4D` (`0x004D3CD0`), `getCampaignSystemNameVa4D` (`0x004D4260`), `resetDelayedExperienceLevelGrantSystem4D` (`0x004D4320`), `getDelayedExperienceLevelGrantSystemNameVa4D` (`0x004D4330`), `getUpdateModuleClassNameVa4D` (`0x004D67F0`).
   - `initThingTemplateRefWrapper4D` (`0x004D4910`), `allocateNextSequenceId4D` (`0x004D4C50`), `countLinkedNodesAtBC4D` (`0x004D4CD0`), `updateCrcBufferMaxSize4D` (`0x004D4EA0`), `updateMaxFloatAt204D` (`0x004D5530`), `containsPointStrictRegion2D4D` (`0x004D6170`), `resetMoveGoalResult4D` (`0x004D6500`), `getSpecialAbilityField0C4D` (`0x004D6710`), `getSpecialAbilityField104D` (`0x004D6730`), `hasAnyLowThreeFlagsAt08_4D` (`0x004D6750`), `stampPassiveAreaEffectMaskedSlots4D` (`0x004D6960`), `isEntryInBeamExcludeList4D` (`0x004D69A0`), `findCampaignRecordByNestedKey4D` (`0x004D6AB0`), `removeObjectIdSwapLastAt24_4D` (`0x004D7040`), `resolveAIUpgradeTargetTemplateRef4D` (`0x004D8390`), `isDamageAndSpawnTableEntryFlag90Set4D` (`0x004D86D0`), `findTaggedContactMidpoint4D` (`0x004D8930`), `defaultConstructLuaEventParameters4D` (`0x004DA780`), `resetGlobalWeatherData4D` (`0x004DB8C0`), `initExitGoalEntry4D` (`0x004DBB30`), `defaultConstructMapMissionObjectives4D` (`0x004DD390`), `defaultConstructCampaignRecordTable4D` (`0x004DD3D0`).
6. **Skirmish AI Structure Placement Grid Transforms (`ra3::gamelogic`, `0x004DBBF0..0x004DC080`, `0x004DFC30`)**:
   - `gridToWorldPlacement4D` (`0x004DBBF0`), `snapWorldPointToGrid4D` (`0x004DBD20`), `offsetWorldPointByGridCells4D` (`0x004DBEC0`), `worldToGridCell4D` (`0x004DC080`), and `snapRotatedFootprintToGrid4D` (`0x004DFC30`): Affine `2x3` grid-to-world and world-to-grid transforms, `30.0f` cell rounding, and rotated structure footprint snapping.

### Ownership Table

| VA | Symbol | Module | Proof |
|---|---|---|---|
| `0x004D05A0` | `ra3::core::getTextFileLine4D` | `core` | `call 004d05a0 <- 00987fc0 <- ... <- 00550620 GameStateMap[3]` |
| `0x004D2AD0` | `ra3::gamelogic::relinkGhostObjectNodeBefore4D` | `gamelogic` | `call 004d2ad0 <- 0071ac40 GhostObjectManager[3]` |
| `0x004D3750` | `ra3::gamelogic::equalsObjectIDPair4D` | `gamelogic` | `call 004d3750 <- 0065add0 <- 0077d580 ReconstituteStateSpecialAbility[27]` |
| `0x004D3770` | `ra3::gamelogic::normalizeAngleRad4D` | `gamelogic` | `call 004d3770 <- 007c2f80 AIFollowPathAsTeamState[5]` |
| `0x004D3CD0` | `ra3::gamelogic::getBehaviorModuleClassNameVa4D` | `gamelogic` | `vt 004d3cd0 00bdbc08[2] BehaviorModule` |
| `0x004D4020` | `ra3::gamelogic::convertSecondsToFrames4D` | `gamelogic` | `call 004d4020 <- 004ec480 <- ... <- 00712730 SpecialPowerModule[23]` |
| `0x004D4260` | `ra3::gamelogic::getCampaignSystemNameVa4D` | `gamelogic` | `vt 004d4260 00bdbd54[2] CampaignSystem` |
| `0x004D4320` | `ra3::gamelogic::resetDelayedExperienceLevelGrantSystem4D` | `gamelogic` | `vt 004d4320 00bdc6a0[1] DelayedExperienceLevelGrantSystem` |
| `0x004D4330` | `ra3::gamelogic::getDelayedExperienceLevelGrantSystemNameVa4D` | `gamelogic` | `vt 004d4330 00bdc6a0[2] DelayedExperienceLevelGrantSystem` |
| `0x004D4400` | `ra3::gamelogic::assignDamageInfoInputPayload4D` | `gamelogic` | `call 004d4400 <- 004da690 <- 004f6720 DOTManager[3]` |
| `0x004D4500` | `ra3::gamelogic::compareCooldownProgress4D` | `gamelogic` | `call 004d4500 <- 004f6630 <- ... <- 00550620 GameStateMap[3]` |
| `0x004D4910` | `ra3::gamelogic::initThingTemplateRefWrapper4D` | `gamelogic` | `call 004d4910 <- 0079614d ThingTemplate.cpp:770` |
| `0x004D4C50` | `ra3::gamelogic::allocateNextSequenceId4D` | `gamelogic` | `call 004d4c50 <- 0076b240 <- 004d7970 <- 004ede90 GameLogic[3]` |
| `0x004D4CD0` | `ra3::gamelogic::countLinkedNodesAtBC4D` | `gamelogic` | `call 004d4cd0 <- 0059d680 W3DTracerManager.cpp` |
| `0x004D4EA0` | `ra3::gamelogic::updateCrcBufferMaxSize4D` | `gamelogic` | `wr 004d4ea0 00bdbec8 GameLogic.cpp:4254` |
| `0x004D5530` | `ra3::gamelogic::updateMaxFloatAt204D` | `gamelogic` | `call 004d5530 <- 0076b240 <- 004d7970 <- 004ede90 GameLogic[3]` |
| `0x004D57A0` | `ra3::gamelogic::snapAngleToCardinalRad4D` | `gamelogic` | `call 004d57a0 <- 008bb220 <- 008d2130 AIBuildableStructure[23]` |
| `0x004D5900` | `ra3::gamelogic::anyBitFlags288_4D` | `gamelogic` | `call 004d5900 <- 006908e0 CrateCollide[22]` |
| `0x004D5920` | `ra3::gamelogic::testRequiredAndForbiddenBitFlags288_4D` | `gamelogic` | `call 004d5920 <- 0078b960 <- 006908e0 CrateCollide[22]` |
| `0x004D5A40` | `ra3::gamelogic::intersectsBitFlags480_4D` | `gamelogic` | `call 004d5a40 <- 0076e8c0 StancesBehavior[26]` |
| `0x004D5AB0` | `ra3::gamelogic::anyBitFlags480_4D` | `gamelogic` | `call 004d5ab0 <- 009091c0 W3DScriptedModelDraw.cpp:851` |
| `0x004D5B10` | `ra3::gamelogic::equalsBitFlags480_4D` | `gamelogic` | `call 004d5b10 <- 00692670 OpenContain[29]` |
| `0x004D5BB0` | `ra3::gamelogic::anyObjectStatusMask224_4D` | `gamelogic` | `call 004d5bb0 <- 007712e0 BeamSpecialAbilityUpdate[39]` |
| `0x004D5BD0` | `ra3::gamelogic::intersectsObjectStatusMask224_4D` | `gamelogic` | `call 004d5bd0 <- 00690e60 FireWeaponCollide[22]` |
| `0x004D5DF0` | `ra3::gamelogic::copyConstructDamageInfoInput4D` | `gamelogic` | `call 004d5df0 <- 00738140 Object[3]` |
| `0x004D6170` | `ra3::gamelogic::containsPointStrictRegion2D4D` | `gamelogic` | `call 004d6170 <- 0072bad0 AIUpdate.cpp` |
| `0x004D6500` | `ra3::gamelogic::resetMoveGoalResult4D` | `gamelogic` | `call 004d6500 <- 006471f0 <- 0064b470 <- 007c08a0 AIInternalMoveToState[4]` |
| `0x004D6550` | `ra3::gamelogic::defaultConstructDamageInfoInput4D` | `gamelogic` | `call 004d6550 <- 004f6720 DOTManager[3]` |
| `0x004D6680` | `ra3::gamelogic::anyBitAt26C4D` | `gamelogic` | `call 004d6680 <- 008c9430 AIEngineerTactic[11]` |
| `0x004D66A0` | `ra3::gamelogic::testBitAt26C4D` | `gamelogic` | `call 004d66a0 <- 007db090 AIIdleState[5]` |
| `0x004D6710` | `ra3::gamelogic::getSpecialAbilityField0C4D` | `gamelogic` | `call 004d6710 <- 00739344 <- ... <- 007432e0 SpecialAbilityUpdate[29]` |
| `0x004D6730` | `ra3::gamelogic::getSpecialAbilityField104D` | `gamelogic` | `call 004d6730 <- 00739344 <- ... <- 007432e0 SpecialAbilityUpdate[29]` |
| `0x004D6750` | `ra3::gamelogic::hasAnyLowThreeFlagsAt08_4D` | `gamelogic` | `call 004d6750 <- 007432e0 SpecialAbilityUpdate[29]` |
| `0x004D67F0` | `ra3::gamelogic::getUpdateModuleClassNameVa4D` | `gamelogic` | `vt 004d67f0 00bdc2c0[2] UpdateModule` |
| `0x004D6960` | `ra3::gamelogic::stampPassiveAreaEffectMaskedSlots4D` | `gamelogic` | `call 004d6960 <- 006c72e0 PassiveAreaEffectBehavior[25]` |
| `0x004D69A0` | `ra3::gamelogic::isEntryInBeamExcludeList4D` | `gamelogic` | `call 004d69a0 <- 004d9ce0 <- 007712e0 BeamSpecialAbilityUpdate[39]` |
| `0x004D6AB0` | `ra3::gamelogic::findCampaignRecordByNestedKey4D` | `gamelogic` | `call 004d6ab0 <- 004ecf10 CampaignSystem[3]` |
| `0x004D6FF0` | `ra3::gamelogic::assignDamageInfoShort4D` | `gamelogic` | `call 004d6ff0 <- 0076e8c0 StancesBehavior[26]` |
| `0x004D7040` | `ra3::gamelogic::removeObjectIdSwapLastAt24_4D` | `gamelogic` | `call 004d7040 <- 00748600 <- 0076e8c0 StancesBehavior[26]` |
| `0x004D8390` | `ra3::gamelogic::resolveAIUpgradeTargetTemplateRef4D` | `gamelogic` | `call 004d8390 <- 008b8e10 <- 008c4990 <- 008c4b10 AIUpgradeScience[3]` |
| `0x004D86D0` | `ra3::gamelogic::isDamageAndSpawnTableEntryFlag90Set4D` | `gamelogic` | `call 004d86d0 <- 00704eb0 <- 00756960 <- 00767810 DamageAndSpawnNugget.cpp` |
| `0x004D8780` | `ra3::gamelogic::matchesObjectStatusFilter4D` | `gamelogic` | `call 004d8780 <- 004e30b0 <- 00771a00 CaptureSpecialAbilityUpdate[27]` |
| `0x004D8810` | `ra3::gamelogic::matchesRelationshipFilter4D` | `gamelogic` | `call 004d8810 <- 004e30b0 <- 00771a00 CaptureSpecialAbilityUpdate[27]` |
| `0x004D8930` | `ra3::gamelogic::findTaggedContactMidpoint4D` | `gamelogic` | `call 004d8930 <- 004f5780 <- 007ab310 UnitAIExitGoalState[4]` |
| `0x004D9230` | `ra3::gamelogic::buildZRotationMatrix4D` | `gamelogic` | `call 004d9230 <- 0066ca50 WaveParticleStorageModule[4]` |
| `0x004D9280` | `ra3::gamelogic::countSetBits288_4D` | `gamelogic` | `call 004d9280 <- 004e0b1c <- 00421704 <- 004e0b00 <- 005a3000 W3DPropBuffer[3]` |
| `0x004DA690` | `ra3::gamelogic::assignDOTInfoRecord4D` | `gamelogic` | `call 004da690 <- 004f6720 DOTManager[3]` |
| `0x004DA780` | `ra3::gamelogic::defaultConstructLuaEventParameters4D` | `gamelogic` | `call 004da780 <- 0077caa0 SlaughterHordeContain[45]` |
| `0x004DAD10` | `ra3::gamelogic::findRegisteredPairByKey4D` | `gamelogic` | `call 004dad10 <- 004ede90 GameLogic[3]` |
| `0x004DB8C0` | `ra3::gamelogic::resetGlobalWeatherData4D` | `gamelogic` | `call 004db8c0 <- 004f9760 <- 004f9a70 GlobalWeatherSystem[3]` |
| `0x004DBB30` | `ra3::gamelogic::initExitGoalEntry4D` | `gamelogic` | `call 004dbb30 <- 004efd20 <- 004f5780 <- 007ab310 UnitAIExitGoalState[4]` |
| `0x004DBBF0` | `ra3::gamelogic::gridToWorldPlacement4D` | `gamelogic` | `call 004dbbf0 <- 008bb220 <- 008d2130 AIBuildableStructure[23]` |
| `0x004DBD20` | `ra3::gamelogic::snapWorldPointToGrid4D` | `gamelogic` | `call 004dbd20 <- 004dfc30 <- 00549300 <- 008ba680 <- 008bade0 GetAIStructurePlacementLocation.cpp` |
| `0x004DBEC0` | `ra3::gamelogic::offsetWorldPointByGridCells4D` | `gamelogic` | `call 004dbec0 <- 005492c0 <- 008abd40 <- 008ba280 <- 008ba450 <- 008ba680` |
| `0x004DC080` | `ra3::gamelogic::worldToGridCell4D` | `gamelogic` | `call 004dc080 <- 008830e0 <- 008bb220 <- 008d2130 AIBuildableStructure[23]` |
| `0x004DD390` | `ra3::gamelogic::defaultConstructMapMissionObjectives4D` | `gamelogic` | `call 004dd390 <- 004df330 MissionObjectiveTracker[3]` |
| `0x004DD3D0` | `ra3::gamelogic::defaultConstructCampaignRecordTable4D` | `gamelogic` | `call 004dd3d0 <- 004ecf10 CampaignSystem[3]` |
| `0x004DFC30` | `ra3::gamelogic::snapRotatedFootprintToGrid4D` | `gamelogic` | `call 004dfc30 <- 00549300 <- 008ba680 <- 008bade0 GetAIStructurePlacementLocation.cpp` |

## Differential Oracle and Semantic Mutation Check

- **Oracle script**: `Ra3Source/tools/analysis/ports/verify_logic4d.py`
- **Checker executable**: `ra3_logic4dcheck` (`Ra3Source/tools/logic4dcheck/main.cpp`)
- **Result**: `logic4d oracle: 58 entries, 258 cases, 0 mismatches`
- Floating-point functions (`0x004D3770`, `0x004D4020`, `0x004D4500`, `0x004D57A0`, `0x004D9230`, `0x004DBBF0`, `0x004DBD20`, `0x004DBEC0`, `0x004DC080`, `0x004DFC30`) are tested across all three x87 precision control words (`0x007F`, `0x027F`, `0x037F`).
- **Semantic mutation check**: Flipped `!=` to `==` on the first dword comparison in `equalsObjectIDPair4D` (`0x004D3750`), rebuilt `ra3_logic4dcheck`, and verified `verify_logic4d.py` failed with `AssertionError: 0x004D3750: return mismatch (u32): game=0x00000001 cpp=0x00000000`. Restored the original operator, rebuilt `ra3_logic4dcheck` (both `Release` and `Debug`), and re-verified `58 entries, 258 cases, 0 mismatches`.

## Rejected Candidates and Deferred Boundaries

- **Controller review — implicit special members**: `0x004D4400` (DamageInfoInput copy assignment, member-wise from `+0x04`, vptr untouched), `0x004D5DF0` (DamageInfoInput copy constructor: vptr `0x00BDC178` then member-wise copy), `0x004D6FF0` and `0x004DA690` (DamageInfoShort / DOTInfoRecord copy assignments: call `0x004D4400`, then copy the tail) are compiler-generated and carry no written logic, so they are annotated as rejected and the batch counts **54**. Their ports stay because the oracle exercises them. The four default constructors (`0x004D6550`, `0x004DA780`, `0x004DD390`, `0x004DD3D0`) set non-trivial defaults and remain counted. Ownership was re-derived by the controller with `work/probe/verify_ownership.py` (`fails=0`); `0x004D4EA0` has no call/vt line but pushes its own `GameLogic.cpp:4254` assertion path.

- **Mid-function loop tail**: `0x004D5AE3` (inner loop continuation of `0x004D5AB0`) was excluded as a non-entry address.
- **EASTL / STL vector & red-black tree helpers**: `0x004D5C20`, `0x004D5F00`, `0x004D6050`, and `0x004DF940` (`eastl::rbtree` erase fixup) were excluded per batch rules.
- **Static `NAMEKEY` initializer leaf**: `0x004D6800` (`UpdateModule::getModuleNameKey`) was excluded to keep the batch focused on behavioral GameLogic and Core functions.
- **Destructor / free paths**: Deleting and non-deleting destructors in `0x004D0000..0x004DFFFF` were excluded per batch rules.
