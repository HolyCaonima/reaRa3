#pragma once

#include <ra3/sim/Random.h>

#include <cstddef>
#include <cstdint>

namespace ra3::sim {

struct Coord2D {
    float x;
    float y;
};

struct Coord3D {
    float x;
    float y;
    float z;
};

struct ICoord3D {
    std::int32_t x;
    std::int32_t y;
    std::int32_t z;
};

struct Region2D {
    Coord2D lo;
    Coord2D hi;
};

struct IRegion2D {
    std::int32_t loX;
    std::int32_t loY;
    std::int32_t hiX;
    std::int32_t hiY;
};

struct Region3D {
    Coord3D lo;
    Coord3D hi;
};

struct Matrix3x4 {
    float m[3][4];
};

struct Plane3D {
    Coord3D normal;
    float dist;
};

static_assert(sizeof(Coord2D) == 0x08, "Coord2D layout mismatch");
static_assert(sizeof(Coord3D) == 0x0C, "Coord3D layout mismatch");
static_assert(sizeof(ICoord3D) == 0x0C, "ICoord3D layout mismatch");
static_assert(sizeof(Region2D) == 0x10, "Region2D layout mismatch");
static_assert(sizeof(IRegion2D) == 0x10, "IRegion2D layout mismatch");
static_assert(sizeof(Region3D) == 0x18, "Region3D layout mismatch");
static_assert(sizeof(Matrix3x4) == 0x30, "Matrix3x4 layout mismatch");
static_assert(sizeof(Plane3D) == 0x10, "Plane3D layout mismatch");

inline constexpr float kRandomStartAngle = -99999.8984375F;  // 0x00C4F238 (0xC7C34FF3)
inline constexpr float kMaxFindPositionZDelta = 1.0e10F;     // 0x00BDF8F0 (0x501502F9)
inline constexpr float kMapXyFactor = 10.0F;                 // 0x00BD9584 (0x41200000)
inline constexpr float kFindPositionRingSpacing = 5.0F;      // 0x00CCBEA4 (0x40A00000)
inline constexpr float kTwoPi = 6.283185482025146484375F;    // 0x00CCBEB0 (0x40C90FDB)
inline constexpr std::uint32_t kInvalidWaypointId = 0x7FFFFFFFU;

// ra3_1.0.game 0x0085D7E0: FindPositionOptions layout (0x44 bytes) consumed by
// findPositionAround (0x008684C0, TerrainLogic.cpp:3158).
struct FindPositionOptions {
    std::uint32_t words00[10];
    float minRadius;            // +0x28
    float maxRadius;            // +0x2C
    float startAngle;           // +0x30
    float maxZDelta;            // +0x34
    void* ignoreObject;         // +0x38
    void* sourceToPathToDest;   // +0x3C
    void* relationshipObject;   // +0x40
};

static_assert(sizeof(FindPositionOptions) == 0x44, "FindPositionOptions layout mismatch");
static_assert(offsetof(FindPositionOptions, minRadius) == 0x28);
static_assert(offsetof(FindPositionOptions, maxRadius) == 0x2C);
static_assert(offsetof(FindPositionOptions, startAngle) == 0x30);
static_assert(offsetof(FindPositionOptions, maxZDelta) == 0x34);

// ra3_1.0.game 0x0085D840: BridgeInfo layout (0x34 bytes) embedded in Bridge at +0x04.
struct BridgeInfo {
    Coord3D fromLeft;              // +0x00
    Coord3D fromRight;             // +0x0C
    Coord3D toLeft;                // +0x18
    Coord3D toRight;               // +0x24
    std::int32_t bridgeObjectId;   // +0x30
};

static_assert(sizeof(BridgeInfo) == 0x34, "BridgeInfo layout mismatch");
static_assert(offsetof(BridgeInfo, bridgeObjectId) == 0x30);

struct BridgeSpanInterval {
    float lo;
    float hi;
};

static_assert(sizeof(BridgeSpanInterval) == 0x08, "BridgeSpanInterval layout mismatch");

// Recovered RA3 Bridge layout (0x60 bytes, allocated in 0x0074A47B and initialized by 0x0085F760).
struct BridgeData {
    BridgeData* next;                       // +0x00
    BridgeInfo info;                        // +0x04..+0x37
    Region2D bounds;                        // +0x38..+0x47
    std::int32_t layer;                     // +0x48
    void* bridgePolygon;                    // +0x4C
    const BridgeSpanInterval* spanBegin;    // +0x50
    const BridgeSpanInterval* spanEnd;      // +0x54
    const BridgeSpanInterval* spanCapacity; // +0x58
    std::uint32_t reserved5C;               // +0x5C
};

static_assert(sizeof(BridgeData) == 0x60, "BridgeData layout mismatch");
static_assert(offsetof(BridgeData, info) == 0x04);
static_assert(offsetof(BridgeData, bounds) == 0x38);
static_assert(offsetof(BridgeData, layer) == 0x48);
static_assert(offsetof(BridgeData, bridgePolygon) == 0x4C);
static_assert(offsetof(BridgeData, spanBegin) == 0x50);
static_assert(offsetof(BridgeData, spanEnd) == 0x54);

// Recovered RA3 Waypoint layout (0x60 bytes, constructed by 0x00863630).
struct WaypointData {
    void* vtable;                   // +0x00 (0x00C4F72C)
    std::int32_t id;                // +0x04
    const char* name;               // +0x08
    Coord3D location;               // +0x0C..+0x17
    std::int32_t flags18;           // +0x18
    WaypointData* next;             // +0x1C
    std::uintptr_t links[8];        // +0x20..+0x3F
    std::uintptr_t prevLink;        // +0x40
    std::uintptr_t secondaryLink;   // +0x44
    std::int32_t pathLabel1;        // +0x48
    std::int32_t numLinks;          // +0x4C
    std::uint32_t labels[3];        // +0x50..+0x5B
    std::uint8_t biDirectional;     // +0x5C
    std::uint8_t padding5D[3];      // +0x5D..+0x5F
};

static_assert(sizeof(WaypointData) == 0x60, "WaypointData layout mismatch");
static_assert(offsetof(WaypointData, id) == 0x04);
static_assert(offsetof(WaypointData, location) == 0x0C);
static_assert(offsetof(WaypointData, next) == 0x1C);
static_assert(offsetof(WaypointData, links) == 0x20);
static_assert(offsetof(WaypointData, prevLink) == 0x40);
static_assert(offsetof(WaypointData, secondaryLink) == 0x44);
static_assert(offsetof(WaypointData, numLinks) == 0x4C);
static_assert(offsetof(WaypointData, biDirectional) == 0x5C);

// Water table inner entry (+0x70 holds integer water height).
struct WaterTableTarget {
    std::uint8_t reserved00[0x70];
    std::int32_t height;            // +0x70
};

struct WaterTablePair {
    std::uint32_t key;              // +0x00
    const WaterTableTarget* target; // +0x04
};

struct WaterTableContainer {
    std::uint8_t reserved00[0x18];
    const WaterTablePair* begin;    // +0x18
    const WaterTablePair* end;      // +0x1C
};

// ra3_1.0.game 0x0085D7E0.
FindPositionOptions* initializeFindPositionOptions(FindPositionOptions& options) noexcept;

// ra3_1.0.game 0x0085D840.
BridgeInfo* initializeBridgeInfo(BridgeInfo& info) noexcept;

// ra3_1.0.game 0x0085D110: WWMath CollisionMath::Point_In_Triangle_2D.
bool pointInTriangle2D(
    const Coord3D& p0, const Coord3D& p1, const Coord3D& p2, const Coord3D& pt,
    int axis1, int axis2, std::uint8_t& flags) noexcept;
bool pointInTriangle2D(
    const Coord3D& p0, const Coord3D& p1, const Coord3D& p2, const Coord3D& pt) noexcept;

// ra3_1.0.game 0x0085DA50: lineInRegion2D (TerrainLogic.cpp:488).
bool lineInRegion2D(
    const Coord2D* p1, const Coord2D* p2, const Region2D* clipRegion) noexcept;

// ra3_1.0.game 0x0085EC40: PlaneClass::set(v1, v2, v3) 3-point plane builder.
void setPlaneFromPoints(
    Plane3D& plane, const Coord3D& v1, const Coord3D& v2, const Coord3D& v3) noexcept;

// ra3_1.0.game 0x008603B0: makeAlignToNormalMatrix (TerrainLogic.cpp:1470).
void makeAlignToNormalMatrix(
    float angle, const Coord3D& pos, const Coord3D& normal, Matrix3x4& mtx) noexcept;

// ra3_1.0.game 0x0085D890.
using PolygonTriggerTestCallback = bool (*)(void* polygonTrigger, const ICoord3D& pt) noexcept;
bool isPointOnBridge(
    const BridgeData& bridge, const Coord3D* loc,
    PolygonTriggerTestCallback polygonTestFn = nullptr) noexcept;

// ra3_1.0.game 0x0085FA50.
float projectPointAlongBridgeSpan(const BridgeData& bridge, const Coord3D* loc) noexcept;
#if defined(_MSC_VER) && defined(_M_IX86)
float __fastcall projectPointAlongBridgeSpanExtended(
    const BridgeData* bridge, void* unused, const Coord3D* loc) noexcept;
#endif

// ra3_1.0.game 0x0085FAD0.
bool isCellOnBridgeEnd(const BridgeData& bridge, const Region2D* cell) noexcept;

// ra3_1.0.game 0x0085FD60.
bool isCellOnBridgeSide(const BridgeData& bridge, const Region2D* cell) noexcept;

// ra3_1.0.game 0x00860020.
bool isCellBridgeEntryPoint(
    const BridgeData& bridge, const Region2D* cell, float* outEntryHeight) noexcept;

// ra3_1.0.game 0x00861450.
bool isPointOnIntactBridge(
    const BridgeData& bridge, const Coord3D* loc,
    PolygonTriggerTestCallback polygonTestFn = nullptr) noexcept;

// ra3_1.0.game 0x008614B0.
bool pickBridge(
    const BridgeData& bridge, const Coord3D& from, const Coord3D& to, Coord3D* outPos,
    PolygonTriggerTestCallback polygonTestFn = nullptr) noexcept;

// ra3_1.0.game 0x00861680.
float getBridgeHeight(
    const BridgeData& bridge, const Coord3D* loc, Coord3D* normal) noexcept;
#if defined(_MSC_VER) && defined(_M_IX86)
float __fastcall getBridgeHeightExtended(
    const BridgeData* bridge, void* unused, const Coord3D* loc, Coord3D* normal) noexcept;
#endif

// ra3_1.0.game 0x00860A20.
void setWaypointBiDirectional(WaypointData& waypoint, bool biDirectional) noexcept;

// ra3_1.0.game 0x00860A30. Returns the stored byte unnormalized.
std::uint8_t getWaypointBiDirectional(const WaypointData& waypoint) noexcept;

// ra3_1.0.game 0x0085DD30.
void addWaypointLink(WaypointData* waypointListHead, std::int32_t id1, std::int32_t id2) noexcept;

// ra3_1.0.game 0x008683F0.
using WaypointLookupCallback = WaypointData* (*)(void* userData, std::int32_t id) noexcept;
void resolveWaypointLinks(
    WaypointData& waypoint, WaypointLookupCallback lookup, void* userData) noexcept;

// ra3_1.0.game 0x0085C5E0.
void* getTerrainBoundaryVector(void* terrainLogic, std::int32_t index) noexcept;

// ra3_1.0.game 0x0085C650.
using ActiveBoundaryChangedCallback = void (*)(void* terrainLogic) noexcept;
void setTerrainActiveBoundary(
    void* terrainLogic, std::int32_t newActiveBoundary,
    ActiveBoundaryChangedCallback onChanged = nullptr) noexcept;

// ra3_1.0.game 0x0085CA80.
using GroundHeightCallback =
    float (*)(void* terrainLogic, float x, float y, Coord3D* normal) noexcept;
float getOriginGroundHeight(
    void* terrainLogic, std::uint32_t unusedArg, Coord3D* normal,
    GroundHeightCallback getGroundHeightFn) noexcept;

// ra3_1.0.game 0x0085CC70.
using WaterHandleHeightCallback = std::int32_t (*)(const void* subObject) noexcept;
float getWaterHeightFromHandle(
    const void* waterHandle, WaterHandleHeightCallback callback = nullptr) noexcept;
#if defined(_MSC_VER) && defined(_M_IX86)
float __fastcall getWaterHeightFromHandleExtended(
    const void* terrainLogic, void* unused, const void* waterHandle) noexcept;
#endif

// ra3_1.0.game 0x0085DA00.
bool hasTerrainBoundaries(const void* terrainLogic) noexcept;

// ra3_1.0.game 0x0085DA10.
bool getTerrainBoundary2D(
    const void* terrainLogic, std::uint32_t index, float* outX, float* outY) noexcept;

// ra3_1.0.game 0x0085DDF0.
float getBaseGroundHeight(
    const void* terrainLogic, float x, float y, Coord3D* normal) noexcept;
#if defined(_MSC_VER) && defined(_M_IX86)
float __fastcall getBaseGroundHeightExtended(
    const void* terrainLogic, void* unused, float x, float y, Coord3D* normal) noexcept;
#endif

// ra3_1.0.game 0x0085DE60.
float getBaseLayerHeight(
    const void* terrainLogic, float x, float y, std::int32_t layer,
    Coord3D* normal, bool clip) noexcept;
#if defined(_MSC_VER) && defined(_M_IX86)
float __fastcall getBaseLayerHeightExtended(
    const void* terrainLogic, void* unused, float x, float y, std::int32_t layer,
    Coord3D* normal, bool clip) noexcept;
#endif

// ra3_1.0.game 0x0085DFA0.
Coord3D* findClosestEdgePoint(
    void* terrainLogic, Coord3D* out, const Coord3D* closestTo,
    const Region3D& extent, GroundHeightCallback getGroundHeightFn) noexcept;

// ra3_1.0.game 0x0085E0B0.
using LayerHeightCallback = float (*)(
    void* terrainLogic, float x, float y, std::int32_t layer,
    Coord3D* normal, bool clip) noexcept;
bool clipLineToMapExtent(
    void* terrainLogic, Coord3D* out, const Coord3D* p1, const Coord3D* p2,
    const Region3D& extentIncludingBorder, LayerHeightCallback getLayerHeightFn) noexcept;

// ra3_1.0.game 0x0085E3A0.
Coord3D* findFarthestEdgePoint(
    void* terrainLogic, Coord3D* out, const Coord3D* farthestFrom,
    const Region3D& extent, GroundHeightCallback getGroundHeightFn) noexcept;

// ra3_1.0.game 0x0085E7F0.
void getTerrainBoundaryRegion(
    const void* terrainLogic, Region2D* outRegion, std::int32_t index) noexcept;

// ra3_1.0.game 0x0085E840.
bool clampPositionToMapExtent(
    void* terrainLogic, Coord3D* pos, const Region3D& extent,
    GroundHeightCallback getGroundHeightFn) noexcept;

// ra3_1.0.game 0x008602F0.
void getMaximumPathfindExtentCenter(
    const Region3D& extent, Coord3D* outCenter) noexcept;

// ra3_1.0.game 0x00860630.
BridgeData* findBridgeAt(const void* terrainLogic, const Coord3D* loc) noexcept;

// ra3_1.0.game 0x00860660.
BridgeData* findBridgeLayerAt(
    const void* terrainLogic, const Coord3D* loc, std::int32_t layer, bool clip) noexcept;

// ra3_1.0.game 0x00861CB0.
using WallTestCallback = bool (*)(const Coord3D* pos) noexcept;
std::int32_t getLayerForDestination(
    const void* terrainLogic, const Coord3D* pos, float groundOrWaterHeight,
    WallTestCallback wallTestFn = nullptr) noexcept;

// ra3_1.0.game 0x00861E30.
std::int32_t getHighestLayerForDestination(
    const void* terrainLogic, const Coord3D* pos, bool onlyHealthyBridges,
    float groundHeight) noexcept;

// ra3_1.0.game 0x00861F40.
std::int32_t getHighestBridgeLayerForDestination(
    const void* terrainLogic, const Coord3D* pos, bool onlyHealthyBridges,
    float groundHeight) noexcept;

// ra3_1.0.game 0x008627F0.
float getFirstWaterTableHeight(const void* terrainLogic) noexcept;
#if defined(_MSC_VER) && defined(_M_IX86)
float __fastcall getFirstWaterTableHeightExtended(
    const void* terrainLogic, void* unused) noexcept;
#endif

// ra3_1.0.game 0x00863F00.
std::int32_t alignOnTerrain(
    void* terrainLogic, float angle, const Coord3D& pos, bool stickToGround,
    Matrix3x4& mtx, float groundOrWaterHeight,
    LayerHeightCallback getLayerHeightFn,
    WallTestCallback wallTestFn = nullptr) noexcept;

// ra3_1.0.game 0x00864190.
void updateCachedWaterGridHeight(void* terrainLogic) noexcept;

// ra3_1.0.game 0x008662B0.
const char* getTerrainLogicSubsystemName(const void* terrainLogic = nullptr) noexcept;

// ra3_1.0.game 0x00868470.
using PostNewMapWaypointsCallback = void (*)(void* terrainLogic) noexcept;
void adjustNewMapWaypoints(
    void* terrainLogic, WaypointData* waypointListHead, bool isSaveGame,
    GroundHeightCallback getGroundHeightFn,
    PostNewMapWaypointsCallback postCallback = nullptr) noexcept;

// ra3_1.0.game 0x008684C0: findPositionAround (TerrainLogic.cpp:3158).
using TryPositionCallback = bool (*)(
    void* userData, const Coord3D* center, float dist, float angle,
    const FindPositionOptions* options, Coord3D* result) noexcept;
bool findPositionAround(
    const Coord3D* center, const FindPositionOptions* options, Coord3D* result,
    const Region3D& maxPathfindExtent, RandomState& logicRandom,
    TryPositionCallback tryPositionFn, void* userData = nullptr) noexcept;

}  // namespace ra3::sim
