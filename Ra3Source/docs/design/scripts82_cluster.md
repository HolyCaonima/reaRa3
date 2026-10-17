# RA3 1.0 `Scripts.cpp` / `ScriptEngine` Cluster (`recon/scripts82`)

## 1. Scope & Anchor Provenance

This branch reconstructs an evidence-closed cluster of 36 functions in the RA3 1.0 script subsystem (`ScriptAction`, `Parameter`, `ActionTemplate`, `Script`, `SequentialScript`, `ScriptList`, `ScriptConditions`, and `ScriptEngine`), centered on `0x0081F710`–`0x00821080` and its tightly coupled script engine/action helpers (`0x00804590`–`0x008049A0`, `0x0081A420`–`0x0081F700`).

### Primary Source Anchor
- **`0x00820780` (`Parameter::readParameter`)**: Anchored by shipped assert path `C:\BF_QABuild_RA3_EALA-BUILD108\Ra3\code\SageEngine\source\GameEngine\gamelogic\scriptengine\Scripts.cpp` (`function_ledger.tsv` row `00820780`, `securom = clean`, size `992`).

### Neighbor & Branch Non-Overlap
- Does **not** expand into the neighboring `LuaScriptEngine.cpp` cluster (`0x00823740+`).
- Zero VA or file overlap with active branches (`recon/aistates`, `audioevent`, `objectlogic`, `uicontrols`, `weaponlogic`, `behavior6b`, `render8e`, `gameclient92`, `terrainlogic`).

### Generals / Zero Hour GPL Reference Files (Architecture & Naming Only)
All C++ implementations and struct offsets in this branch are derived independently from RA3 1.0 shipped x86 bytes and verified against Unicorn execution. Reference files used for class/method naming:
- `GeneralsMD/Code/GameEngine/Include/GameLogic/Scripts.h` (`Script`, `ScriptGroup`, `OrCondition`, `Condition`, `ScriptAction`, `Parameter`, `ScriptList`)
- `GeneralsMD/Code/GameEngine/Include/GameLogic/ScriptEngine.h` (`ScriptEngine`, `SequentialScript`, `AttackPriorityInfo`, `TPairedAsciiString`)
- `GeneralsMD/Code/GameEngine/Source/GameEngine/ScriptEngine/Scripts.cpp` (`Parameter::qualify`, `ScriptAction`, `ScriptList`, `Script`)
- `GeneralsMD/Code/GameEngine/Source/GameEngine/ScriptEngine/ScriptEngine.cpp` (`ScriptEngine` flags, counters, attack priorities, sequential scripts, sightings, object counts)
- `GeneralsMD/Code/GameEngine/Source/GameEngine/ScriptEngine/ScriptConditions.cpp` (`ScriptConditions` evaluation helpers)

---

## 2. Reconstructed Class Layouts, Vtables & Global Dependencies

### Vtables (`vtables_split.tsv` / `classes.tsv`)
| Vtable VA | Class | Primary Constructors / Users |
| --- | --- | --- |
| `0x00C35D44` | `ScriptAction` | `0x00820C60`, `0x00821080`, `0x0082CA70` |
| `0x00C4D4F4` | `Script` (secondary / `Xfer` interface at `+0x0C`) | `0x0081BE00`, `0x0081BE40` (primary `0x00C4D4C0`) |
| `0x00C4D57C` | `SequentialScript` (secondary interface at `+0x08`) | `0x0081C300` (primary `0x00C4D574`), `0x0081C3E0`, `0x0081C450` |

### Key Object Layouts
- **`Parameter` (`0x28` bytes)**:
  - `+0x00`: `int32_t m_paramType` (`0x0B` = `SIDE`, `0x10` = `TEAM`, `0x1C` = `COORD3D`, `0x25` = `OBJECT_FLAG`, `0x30` = `EMOTICON`, etc.)
  - `+0x04`: `uint8_t m_initialized`
  - `+0x08`: `int32_t m_int`
  - `+0x0C`: `float m_real`
  - `+0x10`: `AsciiString m_string` (`char*` buffer pointer; length stored at `*(uint16_t*)(ptr - 4)`)
  - `+0x14`..`+0x1C`: `Coord3D m_coord` (`float x, y, z`)
  - `+0x20`: `uint32_t m_objectStatusMask[2]` (`1 << (index & 0x1F)` in word `index >> 5` via `0x008205C0`)
- **`ScriptAction` (`0x44` bytes)**:
  - `+0x00`: `vptr` (`0x00C35D44`)
  - `+0x04`: `int32_t m_actionType` (`0..0x23C`)
  - `+0x08`: `int32_t m_numParams` (`<= 12`)
  - `+0x0C`..`+0x38`: `Parameter* m_params[12]`
  - `+0x3C`: `ScriptAction* m_nextAction`
  - `+0x40`: `uint8_t m_hasWarnings`
- **`ActionTemplate` (`0x80` bytes per entry in `ScriptEngine`)**:
  - Action table at `ScriptEngine + 0x4C` (`573` entries, `0x23D * 0x80`); condition table at `ScriptEngine + 0x11ECC` (`156` entries, `0x9C * 0x80`).
  - `+0x00`: `AsciiString m_uiName`
  - `+0x04`: `AsciiString m_internalName`
  - `+0x08`: `uint32_t m_nameKey`
  - `+0x0C`: `int32_t m_numUiStrings`
  - `+0x10`..`+0x3C`: `AsciiString m_uiStrings[12]`
  - `+0x40`: `int32_t m_numParameters`
  - `+0x44`..`+0x70`: `int32_t m_parameterTypes[12]`
  - `+0x74`: `AsciiString m_helpString`
- **`Script`**:
  - `+0x27`: `bool m_easy`, `+0x28`: `bool m_normal`, `+0x29`: `bool m_hard`
  - `+0x34`: `int32_t m_delayEvaluationSeconds`
  - `+0x38`: `uint32_t m_frameToEvaluateAt`
  - `+0x3C`: `bool m_isActive`, `+0x3D`: `bool m_isOneShot`, `+0x3E`: `bool m_isSubroutine`
  - `+0x40`: `OrCondition* m_condition`
  - `+0x44`: `ScriptAction* m_action`, `+0x48`: `ScriptAction* m_actionFalse` / evaluation metric
- **`SequentialScript` (`0x2C` bytes)**:
  - `+0x00`: `vptr` (`0x00C4D574`), `+0x04`: `Team* m_teamToExecOn`, `+0x08`: `uint32_t m_objectID`
  - `+0x0C`: `Script* m_scriptToExecute`, `+0x10`: `int32_t m_currentInstruction`, `+0x14`: `int32_t m_timesToLoop`
  - `+0x20`: `int32_t m_framesToWait`, `+0x24`: `bool m_dontAdvanceInstruction`, `+0x28`: `SequentialScript* m_nextScriptInSequence`
- **`ScriptEngine` Key Offsets & Globals**:
  - `0x00DFC058`: `TheScriptEngine`
  - `0x00DEBA64`: `TheGameLogic` (`+0x2C` = `m_frame`, `+0xA8` = skirmish/evaluation gate byte)
  - `0x00DFBD74`: `ThePlayerList` (`+0x34 + idx * 4` = `Player*`; active check at `Player + 0x106 == 0 && Player + 0x123C == 0`)
  - `0x00DFC190`: `TheTerrainLogic` (waypoint lookup `0x00861A70`)
  - `0x00DFC040`: `TheBridgeManager` (RB-tree at `+0x38`, root at `+0x44`, damage state at `node + 0x14 == 2`)
  - `0x00DF4C18`: Skirmish command-button readiness manager (`0x005BA210` / `0x005BA2F0`)
  - `ScriptEngine + 0x34..0x38`: active `SequentialScript*` vector `[begin, end)` (`0x0081A4D0`, `0x0081A520`)
  - `ScriptEngine + 0x16CE4`: flag RB-tree (`DualHashKey` at `+0x10..+0x14`, value at `+0x18`, queried by `0x0081DF50` / `0x0081FA00`)
  - `ScriptEngine + 0x16D64`: `AttackPriorityInfo` array (`256` entries of `16` bytes, count at `+0x17D54`, queried by `0x0081FB40`)
  - `ScriptEngine + 0x17D58`: current evaluating `Player*` (`+0x54` = `SkirmishAI*`, `+0x3C` = difficulty); `+0x17D60`: current player side prefix `AsciiString`; `+0x17D64`: engine difficulty (`0x0081A420`)
  - `ScriptEngine + 0x17D74..0x17D78`: tracked named-object vector `(nameHash, objectPtr)` (`0x0081A4A0`, `0x0081A5B0`, `0x0081BFF0`)
  - `ScriptEngine + (playerIdx * 3 + 0x5FFA) * 4`: per-player `SpecialPower` event list (`0x008201B0`)
  - `ScriptEngine + (playerIdx * 3 + 0x6036) * 4`: per-player `Science/Upgrade` event list (`0x00820280`)
  - `ScriptEngine + (playerIdx + 0xDA2) * 28`: per-player object-type count RB-tree (`0x0081FC40`)
  - `ScriptEngine + (playerIdx + 0xDB6) * 28`: per-player `SightingKey` RB-tree (`0x0081FD60`)

---

## 3. Reconstructed Functions (`36` Total)

All 36 functions are implemented in `Ra3Source/src/gamelogic/include/ra3/gamelogic/ScriptsCluster.h` and `Ra3Source/src/gamelogic/src/ScriptsCluster.cpp`, recorded in `Ra3Source/docs/analysis/ra3_1.0/scripts82_annotations.tsv`, and verified against Unicorn execution of the shipped RA3 1.0 binary bytes in `Ra3Source/tools/analysis/ports/verify_scripts82.py`:

| # | VA | Size | ABI / Return | Symbol | Summary |
| --- | --- | --- | --- | --- | --- |
| 1 | `0x00804590` | 485 | `__thiscall` / `ret` (`AL`) | `ScriptAction::isUnitAction` | 3-stage jump-table classifier for unit-targeted script actions (`12..386`). Hook-safe. |
| 2 | `0x00804780` | 536 | `__thiscall` / `ret` (`AL`) | `ScriptAction::isTeamAction` | 3-stage jump-table classifier for team-targeted script actions (`12..385`). Hook-safe. |
| 3 | `0x008049A0` | 290 | `__thiscall` / `ret` (`AL`) | `ScriptAction::isTeamOrUnitAction` | Combined classifier for actions taking a team (`param[0]`) or unit (`param[0]`/`param[1]`). Hook-safe. |
| 4 | `0x0081A420` | 122 | `__thiscall` / `ret 4` (`EAX` preserving upper 24 bits on `isActive==0`) | `ScriptEngine::canEvaluateScript` | Checks `m_isActive`, difficulty flags (`+0x27..+0x29`) against `Player->m_ai->m_difficulty` or engine default (`+0x17D64`), and `TheGameLogic->m_frame >= m_frameToEvaluateAt`. |
| 5 | `0x0081A4A0` | 46 | `__thiscall` / `ret 4` | `ScriptEngine::clearTrackedObjectPointer` | Clears first matching `objectPtr` in `[+0x17D74, +0x17D78)`. |
| 6 | `0x0081A4D0` | 65 | `__thiscall` / `ret 8` | `ScriptEngine::setSequentialTimerForObject` | Reads `object->m_id` (`+0xDC`) and sets `m_framesToWait` (`+0x20`) on matching active `SequentialScript`. |
| 7 | `0x0081A520` | 52 | `__thiscall` / `ret 8` | `ScriptEngine::setSequentialTimerForTeam` | Sets `m_framesToWait` (`+0x20`) on active `SequentialScript` matching `m_teamToExecOn` (`+0x04`). |
| 8 | `0x0081A5B0` | 88 | `__thiscall` / `ret 4` | `ScriptEngine::notifyOfObjectDestruction` | Clears matching `objectPtr` in tracked vector and cached slots `+0x17D70` / `+0x17D68`. |
| 9 | `0x0081A700` | 73 | `__thiscall` / `ret` | `ScriptList::markAllScriptsAndGroupsActive` | Walks intrusive script list (`+0x00`) and group list (`+0x0C`) setting active flag (`+0x0C = 1`). |
| 10 | `0x0081A750` | 64 | `__thiscall` / `ret` (`EAX`) | `ScriptList::findFirstActiveScriptIndex` | Returns index of first active script in intrusive list or `-1`. Hook-safe. |
| 11 | `0x0081AA40` | 30 | `__thiscall` / `ret 4` (`EAX=this`) | `SightingKey::copyConstruct` | Copies 16-byte `SightingKey` (`word0, word1, mask8, maskC`). Hook-safe. |
| 12 | `0x0081AA60` | 62 | `__thiscall` / `ret 4` (`AL`) | `SightingKey::lessThan` | Lexicographical unsigned 4-tuple `<` comparison on `SightingKey`. Hook-safe. |
| 13 | `0x0081AAA0` | 184 | `__thiscall` / `ret 8` (`EAX=outIter`) | `SightingTree::find` | RB-tree `lower_bound` + equality check on 16-byte `SightingKey` at `node + 0x10..0x1C`. Hook-safe. |
| 14 | `0x0081AB60` | 87 | `__thiscall` / `ret 8` (`EAX`) | `SightingTree::lowerBoundNode` | Walks RB-tree from `root` (`+0x0C`), returning `best` (`EDX`) and parent (`ECX`). Hook-safe. |
| 15 | `0x0081BE00` | 53 | `__thiscall` / `ret` (`EAX=this`) | `Script::initDefaultFields` | Initializes `Script` fields (`+0x0C = 0x00C4D4F4`, difficulty/active flags, pointers). |
| 16 | `0x0081BF20` | 109 | `__thiscall` / `ret` | `Script::updateFrameToEvaluate` | Computes `m_frameToEvaluateAt` from `m_delayEvaluationSeconds * 15` relative to `TheGameLogic->m_frame`. |
| 17 | `0x0081BF90` | 82 | `__thiscall` / `ret 0xC` (`ST0`) | `ScriptCounterDefaultMap::lookupOrDefault` | Looks up `keyHolder->hash` in RB-tree at `this+0x0C` (`0x006C3A10`) or falls back to `this+0x08`, returning `double` in `ST0`. |
| 18 | `0x0081BFF0` | 127 | `__thiscall` / `ret 4` (`AL`) | `ScriptEngine::isTrackedObjectDestroyedByName` | Hashes `AsciiString` with FNV-1a (`0x811C9DC5` / `0x01000193`) and checks if tracked entry has `objectPtr == 0`. |
| 19 | `0x0081C3E0` | 102 | `__thiscall` / `ret` (`EAX=this`) | `SequentialScript::resetFields` | Initializes `SequentialScript` fields (`+0x08 = 0x00C4D57C`, `m_timesToLoop = -1`, `m_framesToWait = -1`, etc.). |
| 20 | `0x0081DE20` | 73 | `__cdecl` / `ret` (`EAX`) | `ScriptIndexedEntry::lowerBoundById` | Binary search over 12-byte `ScriptIndexedEntry` range `[first, last)` by `int32_t id`. Hook-safe. |
| 21 | `0x0081DE70` | 212 | `__thiscall` / `ret 4` (`EAX`) | `ScriptListIndex::lowerBoundByName` | Binary search over index array (`+0x00..+0x04`) into 20-byte entry array (`+0x10`, string at `+0x08`). |
| 22 | `0x0081DF50` | 97 | `__thiscall` / `ret 8` (`EAX=outIter`) | `DualHashTree::find` | RB-tree lookup on 8-byte `DualHashKey` (`prefixHash, nameHash` at `node + 0x10..0x14`). Hook-safe. |
| 23 | `0x0081DFC0` | 61 | `__thiscall` / `ret 4` (`EAX=this`) | `ScriptWaypointInfo::copyFrom` | Copies 36-byte `ScriptWaypointInfo` (`+0x00..+0x20`, skipping padding `+0x0D..+0x0F`). |
| 24 | `0x0081F710` | 156 | `__stdcall` / `ret 8` (`AL`) | `ScriptConditions::evaluateSkirmishCommandButtonIsReady` | Registers `(0x01D2592F, crc32(buttonName))` on first call when `condition->m_customData == 0` and queries readiness (`0x005BA2F0`). |
| 25 | `0x0081F7B0` | 111 | `__stdcall` / `ret 4` (`AL`) | `ScriptConditions::evaluateBridgeBroken` | Resolves waypoint via `TheTerrainLogic` (`0x00861A70`) and checks `TheBridgeManager` RB-tree (`0x00AAFE10`) for `damageState == 2`. |
| 26 | `0x0081F8A0` | 91 | `__stdcall` / `ret 4` (`AL`) | `ScriptConditions::evaluateSupplySourceSafe` | Compares parameter string against `"Any"` (`0x00BF0A98`), `"Safe"` (`0x00C4DD3C`), and `"Unsafe"` (`0x00C4DD38`), returning `1` iff `"Any"`. |
| 27 | `0x0081FA00` | 212 | `__thiscall` / `ret 4` (`EAX`) | `ScriptEngine::findFlagByName` | Splits `flagName` on `'/'` (or uses `this + 0x17D60`), hashes both parts with FNV-1a, and looks up node in `this + 0x16CE4` (`0x0081DF50`). |
| 28 | `0x0081FAE0` | 90 | `__thiscall` / `ret 8` (`EAX=outStr`) | `ScriptEngine::qualifyWithCurrentSidePrefix` | Splits `flagName` on `'/'` (defaulting prefix to `this + 0x17D60`) and constructs `"<prefix>/<name>"`. |
| 29 | `0x0081FB40` | 242 | `__thiscall` / `ret 8` (`EAX`) | `ScriptEngine::findAttackInfo` | Searches `AttackPriorityInfo[1..count-1]` at `this + 0x16D64`; appends new entry if `addIfNotFound && count < 0x100`. |
| 30 | `0x0081FC40` | 107 | `__thiscall` / `ret 8` (`EAX`) | `ScriptEngine::getObjectCount` | Checks player active (`0x007E1C70`/`0x007E1BA0`), computes `crc32(typeName)`, and queries RB-tree at `this + (playerIdx + 0xDA2) * 28`. |
| 31 | `0x0081FD60` | 241 | `__thiscall` / `ret 0x10` (`AL`) | `ScriptEngine::hasRecordedSighting` | Queries `SightingTree::find` (`0x0081AAA0`) at `this + (playerIdx + 0xDB6) * 28`; optionally erases/frees node (`0x0081A990`). |
| 32 | `0x008201B0` | 197 | `__thiscall` / `ret 0x14` (`AL`) | `ScriptEngine::checkAndConsumeSpecialPowerEvent` | Scans per-player list at `(playerIdx * 3 + 0x5FFA) * 4` for matching power hash, optional `sourceObjectId`, and optional `TriggerArea::isPointInside` (`0x0085CFA0`); unlinks and frees node if `eraseWhenFound`. |
| 33 | `0x00820280` | 140 | `__thiscall` / `ret 0xC` (`AL`) | `ScriptEngine::checkAndConsumeScienceOrUpgradeEvent` | Scans per-player list at `(playerIdx * 3 + 0x6036) * 4` for matching hash; unlinks and frees node if `eraseWhenFound`. |
| 34 | `0x00820630` | 317 | `__thiscall` / `ret` | `Parameter::qualifyPlayerSide` | If `m_paramType == 0x0B` (`SIDE`), maps `"Player_1".."Player_4"` and `"PlyrCivilian"` to `"player0".."player3"` and `"neutral"`. |
| 35 | `0x00820B70` | 239 | `__thiscall` / `ret 0xC` (`EAX=outStr`) | `ActionTemplate::getUiStrings` | Concatenates `m_uiStrings[0..m_numUiStrings-1]` into `outStr` and returns `m_numUiStrings`. |
| 36 | `0x00820E00` | 175 | `__thiscall` / `ret` (`EAX`) | `ScriptAction::getTeamInvolved` | Dispatches via 3 jump tables to look up `param[0]` or `param[1]` team name in `TheScriptEngine->getTeamNamed` (`0x0081FE80`). |

---

## 4. Excluded / Inventory-Only Boundaries (`28` Total)

All 28 non-reconstructed functions in the scanned neighborhood are cataloged in `Ra3Source/docs/analysis/ra3_1.0/scripts82_inventory.tsv` with `status = inventory-only` and excluded from `scripts82_annotations.tsv`:

1. **SecuROM-Protected Callee Dependency**:
   - `0x0081F820` (`ScriptConditions::evaluateTechTreeStatus`, 115 bytes): Calls `0x00401E90` (`TechTree::getBuildStatus`), which is an `entry_lost` SecuROM thunk jumping through unresolved slot `0x018FB2A0`. Excluded per hard constraint against inferring unresolved protected bodies.
2. **Trivial Getters, Setters, Destructors, and Forwarding Thunks (No Count Padding)**:
   - `0x0081FCB0` (`Script::xferThunk`, 10 bytes), `0x008204D0` (`OrCondition::xferThunk`, 91 bytes), `0x00820530` (`SequentialScript::xferThunk`, 30 bytes): `this`-adjusting `Xfer` forwarding thunks.
   - `0x00820550` (`Condition::dtor`, 102 bytes), `0x00820C60` (`ScriptAction::dtor`, 102 bytes): Destructors freeing pooled parameter slots.
   - `0x008205C0` (`Parameter::setStatusBit`, 111 bytes): Simple bit-setter helper.
3. **Heavy Engine / Lifecycle / Serialization / Allocator Paths**:
   - `0x0081A560` (`notifyOfTeamDestruction`), `0x0081A610` (`clearTeamReferences`), `0x0081BE40` (`Script::ctor`), `0x0081BEC0` (`Script::dtor`), `0x0081C070` (`getSkirmishAIDifficulty`), `0x0081C0E0` (`resolvePlayerIndexFromSide`), `0x0081C280` (`SequentialScript::operatorNew`), `0x0081C300` (`SequentialScript::ctor`), `0x0081C380` (`SequentialScript::dtor`), `0x0081C450` (`SequentialScript::xfer`), `0x0081C5D0` (`Parameter::readParameterEntry`), `0x0081DE00` (`ScriptEngine::registerLuaSubsystems`), `0x0081F6C0` (`ScriptEngine::onRadarEvent`), `0x0081F900` (`ScriptEngine::initActionAndConditionTemplates`), `0x0081FCC0` (`ScriptEngine::recordSighting`), `0x0081FE80` (`ScriptEngine::getTeamNamed`), `0x0081FFE0` (`ScriptEngine::notifyOfObjectCreationOrDestruction`), `0x008200F0` (`ScriptEngine::transferObjectScriptState`), `0x00820370` (`OrCondition::xfer`), `0x00820780` (`Parameter::readParameter`), `0x00820CD0` (`ScriptAction::qualifyParameters`), `0x00821080` (`ScriptAction::readScriptAction`).

---

## 5. Runtime Hook Targets (`10` Total, All `explicitOnly = true`)

Defined in `Ra3Source/src/hook/dll/targets/Scripts82Targets.cpp` and registered in `ModuleTargets.cpp`:

| VA | Name | Stolen Bytes | Wrapper / ABI Notes |
| --- | --- | --- | --- |
| `0x00804590` | `ScriptAction::isUnitAction` | 8 (`8B 41 04 3D 1A 01 00 00`) | Naked wrapper preserves upper 24 bits of caller `EAX` and writes only `AL`. |
| `0x00804780` | `ScriptAction::isTeamAction` | 8 (`8B 41 04 3D 1D 01 00 00`) | Naked wrapper preserves upper 24 bits of caller `EAX` and writes only `AL`. |
| `0x008049A0` | `ScriptAction::isTeamOrUnitAction` | 5 (`56 8B F1 8B C6`) | Naked wrapper preserves upper 24 bits of caller `EAX` and writes only `AL`. |
| `0x0081A750` | `ScriptList::findFirstActiveScriptIndex` | 6 (`8B 01 83 F8 FF 56`) | `__fastcall(const void* self, void* edx)` -> `int32_t`. |
| `0x0081AA40` | `SightingKey::copyConstruct` | 6 (`8B C1 8B 4C 24 04`) | `__fastcall(SightingKey* self, void* edx, const SightingKey* src)` returning `self` in `EAX` (`ret 4`). |
| `0x0081AA60` | `SightingKey::lessThan` | 5 (`8B 44 24 04 56`) | `__fastcall(const SightingKey* self, void* edx, const SightingKey* rhs)` (`ret 4`). |
| `0x0081AAA0` | `SightingTree::find` | 7 (`83 EC 08 56 8B 74 24`) | `__fastcall(void* self, void* edx, void** outIter, const SightingKey* key)` returning `outIter` in `EAX` (`ret 8`). |
| `0x0081AB60` | `SightingTree::lowerBoundNode` | 5 (`8B 41 0C 53 8D`) | Naked wrapper sets both `EAX` (candidate node) and `ECX` (last parent node) (`ret 8`). |
| `0x0081DE20` | `ScriptIndexedEntry::lowerBoundById` | 6 (`8B 4C 24 04 8B 44`) | `__cdecl(const ScriptIndexedEntry* first, const ScriptIndexedEntry* last, const int32_t* keyId)`. |
| `0x0081DF50` | `DualHashTree::find` | 5 (`8B 41 0C 85 C0`) | `__fastcall(void* self, void* edx, void** outIter, const DualHashKey* key)` returning `outIter` in `EAX` (`ret 8`). |

All 10 hooks have `explicitOnly = true`, position-independent stolen prefixes (`5..8` bytes), exact `expected16` headers verified against `ra3_1.0.game`, and pass the `hook_integration` CTest suite.

---

## 6. Verification & Differential Oracle Summary

- **Unicorn Shipped-Binary Differential Verifier**:
  - Script: `Ra3Source/tools/analysis/ports/verify_scripts82.py`
  - Checker CLI: `Ra3Source/tools/scripts82check/main.cpp` (`ra3_scripts82check.exe`)
  - Executes all 36 reconstructed functions directly in Unicorn (`emulate.py`) on `ra3_1.0.game` across **1,693 scenarios** (covering all action jump-table branches, difficulty/frame combinations, sequential timer updates, RB-tree hits/misses, FNV-1a/CRC32 hashes, side qualification, event list unlinking/deallocation, and UI string concatenation).
  - **Debug & Release Results**: `inventory=64, reversed=36, hooks=10, scenarios=1693, 0 semantic mismatches; deliberate mutant detected and restored`.
- **Unit & Integration Tests**:
  - `Ra3Source/tests/gamelogic/Scripts82Tests.cpp`
  - Win32 `Debug` and `Release` CTest: **3/3 passed (`unit`, `game_data`, `hook_integration`)** with zero build warnings.
