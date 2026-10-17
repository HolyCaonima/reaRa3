# TerrainLogic Cluster Design and Evidence (`0x0085C5E0`–`0x00868840`)

## 1. Scope and Ownership

This document records the reverse-engineering evidence, object layouts, calling conventions, floating-point semantics, reconstructed C++ API, hook descriptors, and differential verification for the Red Alert 3 (`ra3_1.0.game`) `TerrainLogic` cluster centered on `0x008678E0`–`0x00868840` and its directly coupled geometry, bridge, water, and waypoint helpers in `0x0085C5E0`–`0x0086869F`.

### Primary Source Anchor

- **`0x008684C0` (`TerrainLogic::findPositionAround` / `PartitionManager` ring search)**:
  - Pushes source file literal `0x00C4F858` (`"d:\p4\sage_main\sageengine\source\gameengine\gamelogic\map\TerrainLogic.cpp"`) and line `0x0C56` (`3158`) into `GameLogicRandomValueReal` (`0x005A9550`) when `options->startAngle == -99999.8984375f` (`0xC7C34FF3` at `0x00C4F238`).
- **`0x008662B0` (`TerrainLogic::getName`)**:
  - Returns `0x00C4F820` (`"TerrainLogic"`).

### Generals GPL Reference Files Used (Semantic / Naming Reference Only)

All implementations in `Ra3Source/src/sim/include/ra3/sim/TerrainLogic.h` and `Ra3Source/src/sim/src/TerrainLogic.cpp` are derived directly from `ra3_1.0.game` machine code and verified against Unicorn execution of the shipped bytes. The following Command & Conquer Generals / Zero Hour GPL source files (`D:\Game\Ra3\CnC_Generals_Zero_Hour-main`) were consulted strictly for architectural context and method/type naming:

- `GeneralsMD/Code/GameEngine/Include/GameLogic/TerrainLogic.h` (`TerrainLogic`, `Bridge`, `BridgeInfo`, `Waypoint`)
- `GeneralsMD/Code/GameEngine/Source/GameLogic/Map/TerrainLogic.cpp` (`Bridge::isPointOnBridge`, `Bridge::pickBridge`, `Bridge::getBridgeHeight`, `TerrainLogic::getGroundHeight`, `TerrainLogic::getLayerHeight`, `TerrainLogic::findClosestEdgePoint`, `TerrainLogic::findFarthestEdgePoint`, `TerrainLogic::isClearLineOfSight`, `TerrainLogic::getLayerForDestination`, `TerrainLogic::getHighestLayerForDestination`, `TerrainLogic::alignOnTerrain`, `Waypoint` link resolution)
- `GeneralsMD/Code/GameEngine/Include/GameLogic/PartitionManager.h` (`FindPositionOptions`)
- `GeneralsMD/Code/GameEngine/Source/GameLogic/Object/PartitionManager.cpp` (`PartitionManager::findPositionAround`)
- `GeneralsMD/Code/GameEngine/Source/Common/System/Geometry.cpp` (`PointInTriangle2D`, `ClipLine2D`)
- `GeneralsMD/Code/Libraries/Source/WWVegas/WWMath/plane.h` (`PlaneClass::Set`)

---

## 2. Reconstructed Data Structures, Layouts, and Globals

### 2.1 Global Pointers and Constants

| Address | Symbol / Role | Value / Type | Evidence |
|---|---|---|---|
| `0x00DFC190` | `TheTerrainLogic` | `TerrainLogic*` | Read by `0x0085E18A`, `0x0085E840`, `0x0086840D`, `0x008684C0` |
| `0x00DFC194` | `g_waypointListHead` | `WaypointData*` | Read by `0x0085DD30`, `0x008663B0`, `0x00868470` |
| `0x00E07D94` | `ThePartitionManager` | `PartitionManager*` | Read by `0x00866760`, `0x008678E0` |
| `0x00CB7420` | `g_logicRandom` | `RandomState` (24 bytes) | Used by `0x005A9550` called from `0x008684C0` |
| `0x00C4F820` | `"TerrainLogic"` | `const char[]` | Returned by `0x008662B0` |
| `0x00C4F858` | `"d:\p4\sage_main\...\TerrainLogic.cpp"` | `const char[]` | Pushed at `0x00868544` (`line 0x0C56 = 3158`) |
| `0x00BBA4DC` | `_imp__ceil` | `double (__cdecl*)(double)` | Called at `0x008685E0` followed by `_ftol2_sse` (`0x004D2EE0`) |

### 2.2 `TerrainLogic` Layout (`0x00A83EC0` vtable, `0x00862FD0` ctor)

| Offset | Type | Field | Evidence |
|---|---|---|---|
| `+0x00` | `void**` | `vptr` (`0x00A83EC0`) | `0x00862FD0` ctor |
| `+0x2C` | `std::int32_t` | `mapDX` | Map width in cells (`0x00862FD0`) |
| `+0x30` | `WaterTableContainer*` | `waterTable` | Dereferenced by `0x008627F0`, `0x00864190` (`+0x18` begin, `+0x1C` end of 8-byte pairs `{id, target*}` where `target + 0x70` is `int32_t height`) |
| `+0x3C` | `float` | `cachedWaterGridHeight` | Written by `0x00864190` |
| `+0x50` | `Coord2D*` | `boundariesBegin` | Read by `0x0085DA00`, `0x0085DA10` |
| `+0x54` | `Coord2D*` / `IRegion2D*` | `boundariesEnd` / `boundaryRegions` | Read by `0x0085DA00`, `0x0085DA10`, `0x0085E7F0` |
| `+0x60` | `std::uint8_t[16 * N]` | `boundaryVectors` | Indexed by `0x0085C5E0` (`this + index * 16 + 0x60`) |
| `+0x68` | `BridgeData*` | `bridgeListHead` | Walked by `0x00860630`, `0x00860660`, `0x00861CB0`, `0x00861E30`, `0x00861F40` |
| `+0x88` | `std::int32_t` | `activeBoundary` | Checked and updated by `0x0085C650` |

Relevant `TerrainLogic` vtable slots (`0x00A83EC0`):
- `+0x18`: `getGroundHeight(float x, float y, Coord3D* normal)` (`0x0085DDF0` base, `ret 0x0C`, returns `ST0`)
- `+0x1C`: `getOriginGroundHeight(int unused, Coord3D* normal)` (`0x0085CA80`, `ret 0x08`, returns `ST0`)
- `+0x20`: `getGroundOrWaterHeight(const Coord3D* pos, Coord3D* normal)` (`ret 0x08`, returns `ST0`)
- `+0x24`: `getLayerHeight(float x, float y, int layer, Coord3D* normal, bool clip)` (`0x0085DE60` base, `ret 0x14`, returns `ST0`)
- `+0x28`: `getExtent(Region3D* extent)` (`ret 0x04`, returns `extent` in `EAX`)
- `+0x2C`: `getExtentIncludingBorder(Region3D* extent)` (`ret 0x04`, returns `extent` in `EAX`)
- `+0x38`: `getMaximumPathfindExtent(Region3D* extent)` (`ret 0x04`, returns `extent` in `EAX`)
- `+0x3C`: `findClosestEdgePoint(Coord3D* out, const Coord3D* closestTo)` (`0x0085DFA0`, `ret 0x08`, returns `out` in `EAX`)
- `+0x40`: `findFarthestEdgePoint(Coord3D* out, const Coord3D* farthestFrom)` (`0x0085E3A0`, `ret 0x08`, returns `out` in `EAX`)
- `+0x44`: `clipLineToMapExtent(Coord3D* out, const Coord3D* p1, const Coord3D* p2)` (`0x0085E0B0`, `ret 0x0C`, returns `bool` in `AL`)

### 2.3 `BridgeInfo` (`0x34` bytes) and `BridgeData` (`0x60` bytes)

`BridgeInfo` (`0x0085D840`):
- `+0x00`: `Coord3D fromLeft` (12 bytes)
- `+0x0C`: `Coord3D fromRight` (12 bytes)
- `+0x18`: `Coord3D toLeft` (12 bytes)
- `+0x24`: `Coord3D toRight` (12 bytes)
- `+0x30`: `std::int32_t bridgeIndex` (initialized to `0`, `from/to` fields zeroed)

`BridgeData` (`0x0085D890`, `0x0085FA50`, `0x0085FAD0`, `0x0085FD60`, `0x00860020`, `0x00861450`, `0x008614B0`, `0x00861680`):
- `+0x00`: `BridgeData* next`
- `+0x04`: `BridgeInfo info` (`0x34` bytes: `fromLeft` at `+0x04`, `fromRight` at `+0x10`, `toLeft` at `+0x1C`, `toRight` at `+0x28`, `bridgeIndex` at `+0x34`)
- `+0x38`: `Region2D bounds` (`0x10` bytes)
- `+0x48`: `std::int32_t layer`
- `+0x4C`: `void* bridgePolygon` (`PolygonTrigger*`)
- `+0x50`: `BridgeSpanInterval* spanBegin` (`{float lo, float hi}` pairs representing damaged/impassable span intervals)
- `+0x54`: `BridgeSpanInterval* spanEnd`
- `+0x58`: `BridgeSpanInterval* spanCap`

### 2.4 `WaypointData` (`0x60` bytes)

Verified from `0x0085DD30`, `0x00860A20`, `0x00860A30`, `0x008683F0`, `0x00868470`:
- `+0x04`: `std::int32_t id`
- `+0x0C`: `Coord3D location`
- `+0x1C`: `WaypointData* next`
- `+0x20`: `std::uintptr_t links[8]` (stores waypoint IDs before post-load resolution, or `0x7FFFFFFF` for none; resolved in-place to `WaypointData*` by `0x008683F0`)
- `+0x40`: `std::uintptr_t secondaryLink`
- `+0x44`: `std::uintptr_t prevLink`
- `+0x48`: `std::int32_t numLinks` (max 8)
- `+0x5C`: `std::uint8_t biDirectional`

### 2.5 `FindPositionOptions` (`0x44` bytes)

Verified from `0x0085D7E0` and `0x008684C0`:
- `+0x00`: `void* vptr` (`0x00A66C38`, base `0x00A517D0`)
- `+0x04`: `std::uint32_t flags` (`0`)
- `+0x08..+0x23`: `std::uint32_t reserved[7]` (`0`)
- `+0x24`: `std::int32_t relationship` (`2`)
- `+0x28`: `float minRadius` (`0.0f`)
- `+0x2C`: `float maxRadius` (`0.0f`)
- `+0x30`: `float startAngle` (`-99999.8984375f`, `0xC7C34FF3`)
- `+0x34`: `float maxZDelta` (`1.0e10f`, `0x501502F9`)
- `+0x38`: `std::uint8_t ignoreObject[4]` (`0, 0, 0, 1`)
- `+0x3C`: `void* sourceToPathTo` (`0`)
- `+0x40`: `void* relationshipObject` (`0`)

---

## 3. Accepted Reconstructed Functions (43 Functions)

All 43 functions have closed control flow in `ra3_1.0.game` and pass differential verification against Unicorn execution of the shipped binary.

| # | Address | Size | ABI / Cleanup | Reconstructed Symbol | Summary |
|---|---|---|---|---|---|
| 1 | `0x0085C5E0` | 12 | `__thiscall` (`ret 4`) | `ra3::sim::getTerrainBoundaryVector` | Returns `reinterpret_cast<uint8_t*>(this) + index * 8 + 0x50` |
| 2 | `0x0085C650` | 29 | `__thiscall` (`ret 4`) | `ra3::sim::setTerrainActiveBoundary` | If `newBoundary != this->activeBoundary (+0x88)`, stores it and tail-jumps to `vtable[+0x28]` |
| 3 | `0x0085CA80` | 28 | `__thiscall` (`ret 8`, `ST0`) | `ra3::sim::getOriginGroundHeight` | Calls `this->vtable[+0x18](0.0f, 0.0f, normal)` |
| 4 | `0x0085CC70` | 45 | `__thiscall` (`ret 4`, `ST0`) | `ra3::sim::getWaterHeightFromHandle` (`Extended`) | If `handle == nullptr` returns `0.0f`, else dispatches through vbtable at `handle+0x1C` (`vtable[+0x08]`) and `fild`s the returned `int32_t` |
| 5 | `0x0085D110` | 532 | `__cdecl` (`ret`) | `ra3::sim::pointInTriangle2D` | 2D arbitrary-axis triangle containment with colinear segment fallback and boundary bit 2 in `*inOutFlags` |
| 6 | `0x0085D7E0` | 88 | `__thiscall` (`ret`) | `ra3::sim::initializeFindPositionOptions` | Initializes `0x44`-byte `FindPositionOptions` |
| 7 | `0x0085D840` | 73 | `__thiscall` (`ret`) | `ra3::sim::initializeBridgeInfo` | Zeroes `0x34`-byte `BridgeInfo` |
| 8 | `0x0085D890` | 356 | `__thiscall` (`ret 4`) | `ra3::sim::isPointOnBridge` | Tests 2D `bounds`, then either `PolygonTrigger::pointInTrigger(ICoord3D)` (`0x0085CF60`) or two `pointInTriangle2D` calls |
| 9 | `0x0085DA00` | 15 | `__thiscall` (`ret`) | `ra3::sim::hasTerrainBoundaries` | Returns `*(this+0x50) != *(this+0x54)` |
| 10 | `0x0085DA10` | 57 | `__thiscall` (`ret 0x0C`) | `ra3::sim::getTerrainBoundary2D` | Bounds-checked read of `Coord2D` from vector at `+0x50..+0x54` via x87 `fld/fstp` |
| 11 | `0x0085DA50` | 645 | `__cdecl` (`ret`) | `ra3::sim::lineInRegion2D` | 2D Cohen-Sutherland segment-box intersection test |
| 12 | `0x0085DD30` | 179 | `__thiscall` (`ret 8`) | `ra3::sim::addWaypointLink` | Links `waypointID1 -> waypointID2` in `g_waypointListHead` (`0x00DFC194`), plus reverse link when `wp1->biDirectional` |
| 13 | `0x0085DDF0` | 31 | `__thiscall` (`ret 0x0C`, `ST0`) | `ra3::sim::getBaseGroundHeight` (`Extended`) | Base `TerrainLogic::getGroundHeight`: zeroes `*normal` if non-null, returns `0.0f` |
| 14 | `0x0085DE60` | 31 | `__thiscall` (`ret 0x14`, `ST0`) | `ra3::sim::getBaseLayerHeight` (`Extended`) | Base `TerrainLogic::getLayerHeight`: zeroes `*normal` if non-null, returns `0.0f` |
| 15 | `0x0085DFA0` | 269 | `__thiscall` (`ret 8`) | `ra3::sim::findClosestEdgePoint` | Clamps point to nearest edge of `getExtent` (`vtable[+0x28]`) and queries `getGroundHeight` (`vtable[+0x18]`) |
| 16 | `0x0085E0B0` | 743 | `__thiscall` (`ret 0x0C`) | `ra3::sim::clipLineToMapExtent` | Ray-casts from `p2` (inside `getExtentIncludingBorder`) along `p1 - p2` to map border and queries `getLayerHeight(..., 1, nullptr, true)` |
| 17 | `0x0085E3A0` | 178 | `__thiscall` (`ret 8`) | `ra3::sim::findFarthestEdgePoint` | Picks farthest corner of `getExtent` and queries `getGroundHeight` |
| 18 | `0x0085E7F0` | 68 | `__thiscall` (`ret 8`) | `ra3::sim::getTerrainBoundaryRegion` | Scales `IRegion2D` at `(*(this+0x54))[index]` by `10.0f` (`MAP_XY_FACTOR`) into `Region2D` |
| 19 | `0x0085E840` | 123 | `__cdecl` (`ret`) | `ra3::sim::clampPositionToMapExtent` | Clamps `Coord3D*` to `TheTerrainLogic->getExtent()` via `findClosestEdgePoint` if outside |
| 20 | `0x0085EC40` | 431 | `__thiscall` (`ret 0x0C`) | `ra3::sim::setPlaneFromPoints` | `Plane3D::set(v1, v2, v3)` using `(v2 - v1) x (v3 - v1)` and x87 reciprocal sqrt (`fld1; fdivrp`), defaulting to `(0, 0, 1, 0)` on zero cross product |
| 21 | `0x0085FA50` | 127 | `__thiscall` (`ret 4`, `ST0`) | `ra3::sim::projectPointAlongBridgeSpan` (`Extended`) | Projects `loc` onto 2D bridge span `toLeft - fromLeft` using x87 `fsqrt; fld1; fdivrp` |
| 22 | `0x0085FAD0` | 657 | `__thiscall` (`ret 4`) | `ra3::sim::isCellOnBridgeEnd` | Tests whether `cell` intersects inset `fromLeft..fromRight` or `toLeft..toRight` end segments |
| 23 | `0x0085FD60` | 691 | `__thiscall` (`ret 4`) | `ra3::sim::isCellOnBridgeSide` | Tests whether `cell` intersects 1x or 2x `10.2f`-offset side segments along `fromLeft..toLeft` and `fromRight..toRight` |
| 24 | `0x00860020` | 718 | `__thiscall` (`ret 8`) | `ra3::sim::isCellBridgeEntryPoint` | Tests whether `cell` intersects `10.0f`-outset / `20.0f`-widened entry segments and writes `fromLeft.z` or `toLeft.z` |
| 25 | `0x008602F0` | 117 | `__thiscall` (`ret 4`) | `ra3::sim::getMaximumPathfindExtentCenter` | Midpoint of `getMaximumPathfindExtent` (`vtable[+0x38]`) |
| 26 | `0x008603B0` | 529 | `__cdecl` (`ret`) | `ra3::sim::makeAlignToNormalMatrix` | Builds `Matrix3x4` from yaw `angle` (`fcos`/`fsin`), `pos`, and terrain `normal` |
| 27 | `0x00860630` | 48 | `__thiscall` (`ret 4`) | `ra3::sim::findBridgeAt` | Walks `this->bridgeListHead (+0x68)` and returns first `BridgeData*` where `isPointOnBridge(loc)` is true |
| 28 | `0x00860660` | 79 | `__thiscall` (`ret 0x0C`) | `ra3::sim::findBridgeLayerAt` | Walks `this->bridgeListHead (+0x68)` for matching `layer` and tests `isPointOnIntactBridge` (if `clip`) or `isPointOnBridge` |
| 29 | `0x00860A20` | 8 | `__thiscall` (`ret 4`) | `ra3::sim::setWaypointBiDirectional` | Stores `biDirectional` at `waypoint + 0x5C` |
| 30 | `0x00860A30` | 5 | `__thiscall` (`ret`) | `ra3::sim::getWaypointBiDirectional` | Returns the raw `biDirectional` byte at `waypoint + 0x5C` in AL (not normalized; upper EAX is caller residue) |
| 31 | `0x00861450` | 89 | `__thiscall` (`ret 4`) | `ra3::sim::isPointOnIntactBridge` | Tests `isPointOnBridge(loc)` and verifies `projectPointAlongBridgeSpan(loc)` is outside all `[lo, hi]` intervals in `spanBegin..spanEnd` |
| 32 | `0x008614B0` | 449 | `__thiscall` (`ret 0x0C`) | `ra3::sim::pickBridge` | Intersects segment `from -> to` with bridge `Plane3D` and checks `isPointOnIntactBridge` |
| 33 | `0x00861680` | 971 | `__thiscall` (`ret 8`, `ST0`) | `ra3::sim::getBridgeHeight` (`Extended`) | Computes bridge height and optional normal via either quadratic ramp interpolation (`bridgePolygon != nullptr`) or plane intersection (`dz = 1000.0f`) |
| 34 | `0x00861CB0` | 380 | `__thiscall` (`ret 8`) | `ra3::sim::getLayerForDestination` | Finds closest intact bridge layer or wall layer (`17`) when `pos->z > groundOrWaterHeight - 1.0f` |
| 35 | `0x00861E30` | 254 | `__thiscall` (`ret 8`) | `ra3::sim::getHighestLayerForDestination` | Finds layer minimizing `|pos->z - bridgeHeight + 0.0002f|` among bridges at or below `pos->z + 0.0002f` |
| 36 | `0x00861F40` | 200 | `__thiscall` (`ret 8`) | `ra3::sim::getHighestBridgeLayerForDestination` | Finds bridge layer with maximum `bridgeHeight >= groundHeight` |
| 37 | `0x008627F0` | 33 | `__thiscall` (`ret`, `ST0`) | `ra3::sim::getFirstWaterTableHeight` (`Extended`) | Reads first water table entry's integer height (`+0x70`) via `fild` or `fldz` |
| 38 | `0x00863F00` | 139 | `__thiscall` (`ret 0x10`) | `ra3::sim::alignOnTerrain` | Resolves layer via `getLayerForDestination`, queries `getLayerHeight` (+`2.5f` bias if `layer > 1`), builds `Matrix3x4` via `makeAlignToNormalMatrix`, and sets `mtx.m[2][3]` if `stickToGround` |
| 39 | `0x00864190` | 46 | `__thiscall` (`ret`) | `ra3::sim::updateCachedWaterGridHeight` | Converts first water table entry's integer height via `cvtsi2ss` into `*(float*)(this + 0x3C)` |
| 40 | `0x008662B0` | 6 | `__thiscall` (`ret`) | `ra3::sim::getTerrainLogicSubsystemName` | Returns `"TerrainLogic"` (`0x00C4F820`) |
| 41 | `0x008683F0` | 118 | `__thiscall` (`ret`) | `ra3::sim::resolveWaypointLinks` | Resolves `links[0..7]`, `prevLink`, and `secondaryLink` IDs (`0x7FFFFFFF -> nullptr`, else `TheTerrainLogic->findWaypointByID`) |
| 42 | `0x00868470` | 68 | `__thiscall` (`ret 4`) | `ra3::sim::adjustNewMapWaypoints` | Adds `this->getGroundHeight(wp->x, wp->y, nullptr)` to every waypoint's `z` and calls `0x00866BB0` |
| 43 | `0x008684C0` | 480 | `__cdecl` (`ret`) | `ra3::sim::findPositionAround` | Certain source anchor (`TerrainLogic.cpp:3158`): returns `*center` if outside pathfind extent, else searches concentric rings (`dr = 5.0f`) around random/fixed start angle via `0x00866760` |

*(Note: the 43 entries are 30 self-contained functions plus 13 callback-parameterized bridge/layer/waypoint/search helpers; all are declared in `Ra3Source/src/sim/include/ra3/sim/TerrainLogic.h` and exercised by `verify_terrainlogic.py`.)*

---

## 4. Hook Descriptors (20 Safe Hooks)

Registered in `Ra3Source/src/hook/dll/targets/TerrainLogicTargets.cpp` (`appendTerrainLogicTargets`):

| Hook Name | Address | Stolen | `expected16` | ABI / Return | `explicitOnly` | Notes |
|---|---|---|---|---|---|---|
| `terrainboundaryvec` | `0x0085C5E0` | 8 | `8B4424048D44C150C20400CCCCCCCCCC` | `__fastcall(this, edx, idx) -> void*` | `true` | Pointer arithmetic on `this` |
| `pointintriangle2d` | `0x0085D110` | 7 | `83EC248B54243C8B4C2438538B5C2430` | `__cdecl` (`bool`, 1-byte `*inOutFlags`) | `false` | Pure 2D geometry |
| `findposoptionsinit` | `0x0085D7E0` | 5 | `0F57C08BC133C9890889480489480889` | `__fastcall(this, edx) -> FindPositionOptions*` | `true` | Mutates `0x44`-byte struct |
| `bridgeinfoinit` | `0x0085D840` | 5 | `0F57C08BC1F30F1100F30F114004F30F` | `__fastcall(this, edx) -> BridgeInfo*` | `true` | Mutates `0x34`-byte struct |
| `terrainhasboundaries` | `0x0085DA00` | 5 | `8B415033D23B41540F95C28AC2C3CCCC` | `__fastcall(this, edx) -> bool` | `true` | State accessor |
| `terrainboundary2d` | `0x0085DA10` | 6 | `8B51542B51508B442404C1FA033BC272` | `__fastcall(this, edx, idx, outX, outY) -> bool` | `true` | Compares `*outX`/`*outY` only when `true` |
| `lineinregion2d` | `0x0085DA50` | 7 | `83EC088B442414F30F1030F30F107808` | `__cdecl` (`bool`) | `false` | Pure 2D segment-box clip |
| `terraingroundheightbase` | `0x0085DDF0` | 6 | `8B44240CD9EE85C074110F57C0F30F11` | `__fastcall` + naked x87 `tbyte` wrapper | `true` | Preserves 80-bit `ST0` + optional `Coord3D*` |
| `terrainlayerheightbase` | `0x0085DE60` | 6 | `8B442410D9EE85C074110F57C0F30F11` | `__fastcall` + naked x87 `tbyte` wrapper | `true` | Preserves 80-bit `ST0` + optional `Coord3D*` |
| `terrainboundaryregion` | `0x0085E7F0` | 12 | `8B442408F30F10058495BD00C1E00403` | `__fastcall(this, edx, outRegion, idx)` | `true` | Writes 16-byte `Region2D` |
| `planesetfrompoints` | `0x0085EC40` | 7 | `83EC0C8B442418F30F1020F30F106804` | `__fastcall(this, edx, v1, v2, v3)` | `true` | Writes 16-byte `Plane3D` |
| `bridgeprojectalongspan` | `0x0085FA50` | 8 | `83EC0CF30F10411CF30F5C4104F30F10` | `__fastcall` + naked x87 `tbyte` wrapper | `true` | Preserves 80-bit `ST0` |
| `bridgecellonend` | `0x0085FAD0` | 8 | `83EC48F30F104914D9E8F30F105118F3` | `__fastcall(this, edx, cell) -> bool` | `true` | Bridge cell geometry |
| `bridgecellonside` | `0x0085FD60` | 8 | `83EC4CF30F105910F30F5C5904F30F10` | `__fastcall(this, edx, cell) -> bool` | `true` | Bridge cell geometry |
| `bridgecellentrypoint` | `0x00860020` | 8 | `83EC44F30F104114D9E8F30F5C4108F3` | `__fastcall(this, edx, cell, outH) -> bool` | `true` | Compares `*outH` only when `true` |
| `aligntonormalmatrix` | `0x008603B0` | 7 | `83EC24D94424288B442430D9C08B08D9` | `__cdecl` (`Matrix3x4` output) | `false` | Pure 3D basis construction |
| `waypointsetbidir` | `0x00860A20` | 7 | `8A44240488415CC20400CCCCCCCCCCCC` | `__fastcall(this, edx, bi)` | `true` | Mutates `waypoint + 0x5C` |
| `waypointgetbidir` | `0x00860A30` | 5 | `8A415CC3CCCCCCCCCCCCCCCCCCCCCCCC` | `__fastcall(this, edx) -> std::uint8_t` (raw byte in AL) | `true` | Reads `waypoint + 0x5C`; stolen prefix is `mov al,[ecx+5Ch]; ret; int3`, same shape as main's `audioeventshouldfade` |
| `terrainfirstwaterheight` | `0x008627F0` | 6 | `8B49308B411C2B4118C1F8033BC97504` | `__fastcall` + naked x87 `tbyte` wrapper | `true` | Preserves 80-bit `ST0` |
| `terrainupdatewatergrid` | `0x00864190` | 6 | `8B41308B501C2B5018C1FA033BC07504` | `__fastcall(this, edx)` | `true` | Mutates `*(float*)(this + 0x3C)` |

17 targets are `explicitOnly=true`; only the three pure geometry functions
(`pointintriangle2d`, `lineinregion2d`, `aligntonormalmatrix`) follow the
configuration's default mode.

Functions that invoke virtual methods on `TheTerrainLogic` (`0x0085CA80`, `0x0085DFA0`, `0x0085E0B0`, `0x0085E3A0`, `0x0085E840`, `0x008602F0`, `0x00861CB0`, `0x00861E30`, `0x00861F40`, `0x00863F00`), call `PolygonTrigger::pointInTrigger` (`0x0085D890`, `0x00860630`, `0x00860660`, `0x00861450`, `0x008614B0`, `0x00861680`), mutate waypoint/global state (`0x0085C650`, `0x0085DD30`, `0x008683F0`, `0x00868470`), or advance `g_logicRandom` (`0x008684C0`) are intentionally not hooked to avoid double-execution or global side effects in shadow mode. `0x008662B0` (`mov eax, 0x00C4F820; ret`) only returns the constant subsystem-name literal; it is reconstructed and oracle-checked but deliberately not hooked.

---

## 5. Deferred and Rejected Neighbor Functions in `0x0085C000`–`0x0086A500`

| Address Range | Symbol / Role | Reason Deferred / Not Hooked |
|---|---|---|
| `0x0085CF60` | `PolygonTrigger::pointInTrigger` | Separate `PolygonTrigger` class (`0x0085C760`–`0x0085D0E0`) with internal vertex array cache |
| `0x00860810`–`0x00861440` | `Bridge` object/tower/damage/scaffold management | Spawns/destroys `Object` instances, FX lists, and audio events |
| `0x00862010`–`0x008627E0` | `TerrainLogic` bridge list creation / deletion / damage states | Allocates `Bridge` objects and mutates `ThePartitionManager` / radar |
| `0x00862820`–`0x00863EF0` | `TerrainLogic` ctor/dtor/init/reset/update/xfer and water table queries | Heavy subsystem lifecycle, STL map/vector allocations, and `Xfer` serialization |
| `0x008662C0`–`0x00866750` | `TerrainLogic::loadMap` / waypoint & trigger post-load | File/asset loading and `Object` creation |
| `0x00866760` | `tryPositionAtPolarOffset` (helper called by `0x008684C0`) | Computes `(x, y)` via `cos`/`sin`, clips to map extent, and delegates to `0x008678E0` |
| `0x008678E0` | `PartitionManager::tryPosition` (`size = 2231` bytes) | Deeply coupled to `PartitionManager`, `Object`, `AIPathfind`, `WaveGuide`, and water/cliff/building collision queries (`0x00866E00`, `0x00867520`) |
| `0x008686A0` | `PartitionManager::findPositionAround7` (`size = 445` bytes) | Variant 7-argument ring search calling `0x008678E0` and `0x00867430` (`PartitionManager` cell queries) |

---

## 6. Verification Summary

- **Standalone Checker (`ra3_terrainlogiccheck --selftest`)**: Passes in both Win32 `Debug` and `Release`.
- **Unit & Integration Tests (`ctest -C Debug` / `ctest -C Release`)**: `3/3` pass (`unit`, `game_data`, `hook_integration`) in both configurations.
- **Differential Oracle (`Ra3Source/tools/analysis/ports/verify_terrainlogic.py`)**:
  - Executes all 43 functions (the 30 self-contained functions + 13 callback-parameterized `TerrainLogic`/`Bridge`/`Waypoint` functions) in `ra3_1.0.game` via Unicorn against `ra3_terrainlogiccheck.exe` (`Debug` and `Release`).
  - **2,868 scenarios** in Release (the initial delivery ran 2,865 in both Debug and Release; the follow-up added three non-boolean `waypointgetbidir` bytes) covering normal, boundary, colinear, degenerate zero-length, `NaN`, and pseudo-random inputs across x87 FPU control words `0x007F` (24-bit), `0x027F` (53-bit), and `0x037F` (64-bit).
  - **0 mismatches**.
  - **Harness check**: `mutate 1` offsets the `terrainboundaryvec` result by 4 and the oracle must report it; `mutate 0` restores parity.
  - **Semantic mutation**: temporarily setting `kSideOffset` to `10.0f` makes the oracle fail in the `isCellOnBridgeSide` comparison; restoring the shipped `10.2f` (`0x41233333`) returns to zero mismatches.
