#include <ra3/sim/TerrainLogic.h>

#include <cmath>
#include <cstdint>
#include <cstring>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

int g_failures = 0;

void check(bool condition, const char* expr, int line) {
    if (!condition) {
        std::cerr << "FAIL [line " << line << "]: " << expr << "\n";
        ++g_failures;
    }
}

bool nearEq(float a, float b, float eps = 1e-4F) {
    return std::fabs(a - b) <= eps;
}

std::vector<std::uint8_t> decodeHex(const std::string& text) {
    if (text == ".") {
        return {};
    }
    if ((text.size() & 1U) != 0U) {
        throw std::runtime_error("odd hex length");
    }
    std::vector<std::uint8_t> out(text.size() / 2U);
    for (std::size_t i = 0; i < out.size(); ++i) {
        out[i] = static_cast<std::uint8_t>(std::stoul(text.substr(i * 2U, 2U), nullptr, 16));
    }
    return out;
}

template <typename T>
T decodeStruct(const std::string& text) {
    const auto bytes = decodeHex(text);
    if (bytes.size() != sizeof(T)) {
        throw std::runtime_error("struct size mismatch");
    }
    T out{};
    std::memcpy(&out, bytes.data(), sizeof(T));
    return out;
}

std::string encodeHex(const void* data, std::size_t size) {
    const auto* const bytes = static_cast<const unsigned char*>(data);
    std::ostringstream os;
    os << std::hex << std::setfill('0');
    for (std::size_t i = 0; i < size; ++i) {
        os << std::setw(2) << static_cast<unsigned>(bytes[i]);
    }
    return os.str();
}

template <typename T>
T fromBits(std::uint32_t raw) {
    static_assert(sizeof(T) == sizeof(raw));
    T out{};
    std::memcpy(&out, &raw, sizeof(T));
    return out;
}

std::uint32_t toBits(float value) {
    std::uint32_t raw = 0;
    std::memcpy(&raw, &value, sizeof(raw));
    return raw;
}

void setFpuControlWord(std::uint16_t cw) noexcept {
#if defined(_MSC_VER) && defined(_M_IX86)
    __asm {
        fldcw word ptr [cw]
    }
#else
    (void)cw;
#endif
}

float g_stubGroundHeight = 0.0F;
float g_stubLayerHeight = 0.0F;
ra3::sim::Coord3D g_stubLayerNormal{0.0F, 0.0F, 1.0F};
bool g_stubCallbackCalled = false;
ra3::sim::ICoord3D g_lastPolyICoord{};
bool g_stubPolyResult = false;
int g_tryPosAcceptIndex = -1;
int g_tryPosCallCount = 0;
bool g_mutateMode = false;

void stubOnActiveBoundaryChanged(void* /*tl*/) noexcept {
    g_stubCallbackCalled = true;
}

float stubGetGroundHeight(
    void* /*tl*/, float /*x*/, float /*y*/, ra3::sim::Coord3D* normal) noexcept {
    if (normal != nullptr) {
        *normal = g_stubLayerNormal;
    }
    return g_stubGroundHeight;
}

float stubGetLayerHeight(
    void* /*tl*/, float /*x*/, float /*y*/, std::int32_t /*layer*/,
    ra3::sim::Coord3D* normal, bool /*clip*/) noexcept {
    if (normal != nullptr) {
        *normal = g_stubLayerNormal;
    }
    return g_stubLayerHeight;
}

bool stubPolygonTriggerTest(void* /*poly*/, const ra3::sim::ICoord3D& pt) noexcept {
    g_lastPolyICoord = pt;
    return g_stubPolyResult;
}

bool stubTryPosition(
    void* /*userData*/, const ra3::sim::Coord3D* center, float dist, float angle,
    const ra3::sim::FindPositionOptions* /*options*/, ra3::sim::Coord3D* result) noexcept {
    const int idx = g_tryPosCallCount++;
    if (idx == g_tryPosAcceptIndex) {
        result->x = center->x + dist;
        result->y = center->y + angle;
        result->z = center->z;
        return true;
    }
    return false;
}

ra3::sim::WaypointData* stubLookupWaypoint(void* userData, std::int32_t id) noexcept {
    auto* const base = static_cast<std::uint8_t*>(userData);
    if (id <= 0) {
        return nullptr;
    }
    return reinterpret_cast<ra3::sim::WaypointData*>(base + static_cast<std::size_t>(id) * 0x100U);
}

float flatGroundHeight(
    void* /*tl*/, float /*x*/, float /*y*/, ra3::sim::Coord3D* normal) noexcept {
    if (normal != nullptr) {
        *normal = {0.0F, 0.0F, 1.0F};
    }
    return 15.0F;
}

float layeredHeight(
    void* /*tl*/, float /*x*/, float /*y*/, std::int32_t layer,
    ra3::sim::Coord3D* normal, bool /*clip*/) noexcept {
    if (normal != nullptr) {
        *normal = {0.0F, 0.0F, 1.0F};
    }
    return (layer > 1) ? 40.0F : 15.0F;
}

bool acceptRing2(
    void* /*userData*/, const ra3::sim::Coord3D* center, float dist, float angle,
    const ra3::sim::FindPositionOptions* /*options*/, ra3::sim::Coord3D* result) noexcept {
    if (dist >= 5.0F) {
        result->x = center->x + std::cos(angle) * dist;
        result->y = center->y + std::sin(angle) * dist;
        result->z = center->z;
        return true;
    }
    return false;
}

ra3::sim::WaypointData* lookupWpFromArray(void* userData, std::int32_t id) noexcept {
    auto* const head = static_cast<ra3::sim::WaypointData*>(userData);
    for (ra3::sim::WaypointData* cur = head; cur != nullptr; cur = cur->next) {
        if (cur->id == id) {
            return cur;
        }
    }
    return nullptr;
}

#define CHECK(expr) check((expr), #expr, __LINE__)

int runSelfTest() {
    using namespace ra3::sim;

    FindPositionOptions opts{};
    std::memset(&opts, 0xAB, sizeof(opts));
    CHECK(initializeFindPositionOptions(opts) == &opts);
    CHECK(opts.minRadius == 0.0F);
    CHECK(opts.maxRadius == 0.0F);
    CHECK(opts.startAngle == kRandomStartAngle);
    CHECK(opts.maxZDelta == kMaxFindPositionZDelta);
    CHECK(opts.ignoreObject == nullptr);

    BridgeInfo binfo{};
    std::memset(&binfo, 0xCD, sizeof(binfo));
    CHECK(initializeBridgeInfo(binfo) == &binfo);
    CHECK(binfo.fromLeft.x == 0.0F && binfo.toRight.z == 0.0F);
    CHECK(binfo.bridgeObjectId == 0);

    const Coord3D p0{0.0F, 0.0F, 0.0F};
    const Coord3D p1{10.0F, 0.0F, 0.0F};
    const Coord3D p2{0.0F, 10.0F, 0.0F};
    const Coord3D inside{2.5F, 2.5F, 0.0F};
    const Coord3D outside{8.0F, 8.0F, 0.0F};
    const Coord3D edge{5.0F, 0.0F, 0.0F};
    std::uint8_t triFlags = 0;
    CHECK(pointInTriangle2D(p0, p1, p2, inside, 0, 1, triFlags));
    CHECK(triFlags == 0);
    CHECK(pointInTriangle2D(p0, p2, p1, inside, 0, 1, triFlags));
    CHECK(!pointInTriangle2D(p0, p1, p2, outside, 0, 1, triFlags));
    CHECK(pointInTriangle2D(p0, p1, p2, edge, 0, 1, triFlags));
    CHECK((triFlags & 1U) != 0U);

    const Region2D clip{{0.0F, 0.0F}, {100.0F, 100.0F}};
    const Coord2D l1{-20.0F, 50.0F};
    const Coord2D l2{120.0F, 50.0F};
    const Coord2D l3{-20.0F, -10.0F};
    const Coord2D l4{-5.0F, -2.0F};
    CHECK(lineInRegion2D(&l1, &l2, &clip));
    CHECK(!lineInRegion2D(&l3, &l4, &clip));

    Plane3D plane{};
    setPlaneFromPoints(plane, {0.0F, 0.0F, 25.0F}, {10.0F, 0.0F, 25.0F}, {0.0F, 10.0F, 25.0F});
    CHECK(nearEq(plane.normal.x, 0.0F));
    CHECK(nearEq(plane.normal.y, 0.0F));
    CHECK(nearEq(plane.normal.z, 1.0F));
    CHECK(nearEq(plane.dist, 25.0F));

    Matrix3x4 mtx{};
    makeAlignToNormalMatrix(0.0F, {10.0F, 20.0F, 30.0F}, {0.0F, 0.0F, 1.0F}, mtx);
    CHECK(nearEq(mtx.m[0][0], 1.0F));
    CHECK(nearEq(mtx.m[1][1], 1.0F));
    CHECK(nearEq(mtx.m[2][2], 1.0F));
    CHECK(nearEq(mtx.m[0][3], 10.0F));
    CHECK(nearEq(mtx.m[1][3], 20.0F));
    CHECK(nearEq(mtx.m[2][3], 30.0F));

    BridgeData bridge{};
    bridge.info.fromLeft = {0.0F, 0.0F, 20.0F};
    bridge.info.fromRight = {0.0F, 40.0F, 20.0F};
    bridge.info.toLeft = {100.0F, 0.0F, 20.0F};
    bridge.info.toRight = {100.0F, 40.0F, 20.0F};
    bridge.bounds = {{0.0F, 0.0F}, {100.0F, 40.0F}};
    bridge.layer = 2;
    BridgeSpanInterval hole{40.0F, 60.0F};
    bridge.spanBegin = &hole;
    bridge.spanEnd = &hole + 1;

    const Coord3D onBridgePt{25.0F, 20.0F, 20.0F};
    const Coord3D inHolePt{50.0F, 20.0F, 20.0F};
    const Coord3D offBridgePt{150.0F, 20.0F, 20.0F};
    CHECK(isPointOnBridge(bridge, &onBridgePt));
    CHECK(!isPointOnBridge(bridge, &offBridgePt));
    CHECK(nearEq(projectPointAlongBridgeSpan(bridge, &onBridgePt), 25.0F));
    CHECK(isPointOnIntactBridge(bridge, &onBridgePt));
    CHECK(!isPointOnIntactBridge(bridge, &inHolePt));

    const Region2D endCell{{-5.0F, 15.0F}, {5.0F, 25.0F}};
    const Region2D sideCell{{40.0F, -12.0F}, {60.0F, -8.0F}};
    const Region2D entryCell{{-12.0F, 15.0F}, {-8.0F, 25.0F}};
    CHECK(isCellOnBridgeEnd(bridge, &endCell));
    CHECK(isCellOnBridgeSide(bridge, &sideCell));
    float entryH = -1.0F;
    CHECK(isCellBridgeEntryPoint(bridge, &entryCell, &entryH));
    CHECK(nearEq(entryH, 20.0F));

    Coord3D pickOut{};
    CHECK(pickBridge(bridge, {25.0F, 20.0F, 50.0F}, {25.0F, 20.0F, 0.0F}, &pickOut));
    CHECK(nearEq(pickOut.x, 25.0F) && nearEq(pickOut.y, 20.0F) && nearEq(pickOut.z, 20.0F));

    Coord3D bridgeNormal{};
    CHECK(nearEq(getBridgeHeight(bridge, &onBridgePt, &bridgeNormal), 20.0F));
    CHECK(nearEq(std::fabs(bridgeNormal.z), 1.0F));

    WaypointData wp1{};
    WaypointData wp2{};
    wp1.id = 101;
    wp1.location = {10.0F, 20.0F, 5.0F};
    wp1.next = &wp2;
    wp2.id = 202;
    wp2.location = {30.0F, 40.0F, 7.0F};
    setWaypointBiDirectional(wp1, true);
    CHECK(getWaypointBiDirectional(wp1));
    addWaypointLink(&wp1, 101, 202);
    CHECK(wp1.numLinks == 1 && wp1.links[0] == reinterpret_cast<std::uintptr_t>(&wp2));
    CHECK(wp2.numLinks == 1 && wp2.links[0] == reinterpret_cast<std::uintptr_t>(&wp1));

    WaypointData wpResolve{};
    for (std::uintptr_t& slot : wpResolve.links) {
        slot = kInvalidWaypointId;
    }
    wpResolve.links[0] = 202U;
    wpResolve.prevLink = 101U;
    wpResolve.secondaryLink = kInvalidWaypointId;
    resolveWaypointLinks(wpResolve, &lookupWpFromArray, &wp1);
    CHECK(wpResolve.links[0] == reinterpret_cast<std::uintptr_t>(&wp2));
    CHECK(wpResolve.links[1] == 0U);
    CHECK(wpResolve.prevLink == reinterpret_cast<std::uintptr_t>(&wp1));
    CHECK(wpResolve.secondaryLink == 0U);

    std::uint8_t fakeTerrain[0xA0]{};
    Coord2D boundsVec[2]{{12.0F, 34.0F}, {56.0F, 78.0F}};
    IRegion2D iregions[2]{{1, 2, 30, 40}, {5, 6, 70, 80}};
    *reinterpret_cast<std::uintptr_t*>(fakeTerrain + 0x50) =
        reinterpret_cast<std::uintptr_t>(&boundsVec[0]);
    *reinterpret_cast<std::uintptr_t*>(fakeTerrain + 0x54) =
        reinterpret_cast<std::uintptr_t>(&boundsVec[2]);
    *reinterpret_cast<BridgeData**>(fakeTerrain + 0x68) = &bridge;

    CHECK(getTerrainBoundaryVector(fakeTerrain, 2) == fakeTerrain + 0x60);
    setTerrainActiveBoundary(fakeTerrain, 3);
    CHECK(*reinterpret_cast<std::int32_t*>(fakeTerrain + 0x88) == 3);
    CHECK(hasTerrainBoundaries(fakeTerrain));
    float bx = 0.0F;
    float by = 0.0F;
    CHECK(getTerrainBoundary2D(fakeTerrain, 1U, &bx, &by));
    CHECK(bx == 56.0F && by == 78.0F);
    CHECK(!getTerrainBoundary2D(fakeTerrain, 2U, &bx, &by));

    *reinterpret_cast<const IRegion2D**>(fakeTerrain + 0x54) = iregions;
    Region2D breg{};
    getTerrainBoundaryRegion(fakeTerrain, &breg, 1);
    CHECK(breg.lo.x == 50.0F && breg.lo.y == 60.0F &&
          breg.hi.x == 700.0F && breg.hi.y == 800.0F);

    Coord3D baseNorm{1.0F, 2.0F, 3.0F};
    CHECK(getBaseGroundHeight(fakeTerrain, 5.0F, 6.0F, &baseNorm) == 0.0F);
    CHECK(baseNorm.x == 0.0F && baseNorm.y == 0.0F && baseNorm.z == 0.0F);
    baseNorm = {1.0F, 2.0F, 3.0F};
    CHECK(getBaseLayerHeight(fakeTerrain, 5.0F, 6.0F, 1, &baseNorm, true) == 0.0F);
    CHECK(baseNorm.x == 0.0F && baseNorm.y == 0.0F && baseNorm.z == 0.0F);
    CHECK(getOriginGroundHeight(fakeTerrain, 0, &baseNorm, &flatGroundHeight) == 15.0F);

    const Region3D mapExtent{{0.0F, 0.0F, 0.0F}, {200.0F, 100.0F, 50.0F}};
    Coord3D closest{};
    const Coord3D nearBottom{80.0F, 5.0F, 0.0F};
    CHECK(findClosestEdgePoint(
              fakeTerrain, &closest, &nearBottom, mapExtent, &flatGroundHeight) == &closest);
    CHECK(nearEq(closest.x, 80.0F) && nearEq(closest.y, 0.0F) && nearEq(closest.z, 15.0F));

    Coord3D farthest{};
    CHECK(findFarthestEdgePoint(
              fakeTerrain, &farthest, &nearBottom, mapExtent, &flatGroundHeight) == &farthest);
    CHECK(nearEq(farthest.x, 200.0F) && nearEq(farthest.y, 100.0F) && nearEq(farthest.z, 15.0F));

    Coord3D clipped{};
    const Coord3D pOut{150.0F, 50.0F, 0.0F};
    const Coord3D pIn{50.0F, 50.0F, 0.0F};
    CHECK(clipLineToMapExtent(
        fakeTerrain, &clipped, &pOut, &pIn, mapExtent, &layeredHeight));
    CHECK(nearEq(clipped.x, 0.0F) && nearEq(clipped.y, 50.0F) && nearEq(clipped.z, 15.0F));

    Coord3D toClamp{-10.0F, 40.0F, 0.0F};
    CHECK(clampPositionToMapExtent(fakeTerrain, &toClamp, mapExtent, &flatGroundHeight));
    CHECK(nearEq(toClamp.x, 0.0F) && nearEq(toClamp.y, 40.0F) && nearEq(toClamp.z, 15.0F));

    Coord3D center{};
    getMaximumPathfindExtentCenter(mapExtent, &center);
    CHECK(nearEq(center.x, 100.0F) && nearEq(center.y, 50.0F) && nearEq(center.z, 25.0F));

    CHECK(findBridgeAt(fakeTerrain, &onBridgePt) == &bridge);
    CHECK(findBridgeAt(fakeTerrain, &offBridgePt) == nullptr);
    CHECK(findBridgeLayerAt(fakeTerrain, &onBridgePt, 2, true) == &bridge);
    CHECK(findBridgeLayerAt(fakeTerrain, &onBridgePt, 1, true) == nullptr);

    const Coord3D highOnBridge{25.0F, 20.0F, 20.2F};
    CHECK(getLayerForDestination(fakeTerrain, &highOnBridge, 0.0F) == 2);
    CHECK(getHighestLayerForDestination(fakeTerrain, &highOnBridge, true, 0.0F) == 2);
    CHECK(getHighestBridgeLayerForDestination(fakeTerrain, &highOnBridge, true, 0.0F) == 2);

    WaterTableTarget target{};
    target.height = 42;
    WaterTablePair pair{1U, &target};
    WaterTableContainer table{};
    table.begin = &pair;
    table.end = &pair + 1;
    *reinterpret_cast<const WaterTableContainer**>(fakeTerrain + 0x30) = &table;
    CHECK(getFirstWaterTableHeight(fakeTerrain) == 42.0F);
    updateCachedWaterGridHeight(fakeTerrain);
    CHECK(*reinterpret_cast<float*>(fakeTerrain + 0x3C) == 42.0F);

    Matrix3x4 aligned{};
    CHECK(alignOnTerrain(
              fakeTerrain, 0.0F, highOnBridge, true, aligned, 0.0F, &layeredHeight) == 2);
    CHECK(nearEq(aligned.m[2][3], 42.5F));

    CHECK(std::strcmp(getTerrainLogicSubsystemName(fakeTerrain), "TerrainLogic") == 0);

    adjustNewMapWaypoints(fakeTerrain, &wp1, false, &flatGroundHeight);
    CHECK(nearEq(wp1.location.z, 20.0F));
    CHECK(nearEq(wp2.location.z, 22.0F));

    RandomState rng{};
    seedRandom(rng, 12345U);
    opts.minRadius = 0.0F;
    opts.maxRadius = 15.0F;
    opts.startAngle = kRandomStartAngle;
    Coord3D foundPos{};
    const Coord3D searchCenter{50.0F, 50.0F, 15.0F};
    CHECK(findPositionAround(
        &searchCenter, &opts, &foundPos, mapExtent, rng, &acceptRing2));
    const float foundDist = std::hypot(foundPos.x - searchCenter.x, foundPos.y - searchCenter.y);
    CHECK(nearEq(foundDist, 5.0F, 1e-3F));

    if (g_failures == 0) {
        std::cout << "ra3_terrainlogic_check: ALL CHECKS PASSED\n";
        return 0;
    }
    std::cerr << "ra3_terrainlogic_check: " << g_failures << " failure(s)\n";
    return 1;
}

void dispatchCommand(const std::string& line) {
    using namespace ra3::sim;
    std::istringstream in(line);
    std::string op;
    in >> op;
    if (op.empty()) {
        return;
    }
    if (op == "fpcw") {
        unsigned cw = 0;
        in >> cw;
        setFpuControlWord(static_cast<std::uint16_t>(cw));
        std::cout << "ok\n";
        return;
    }
    if (op == "mutate") {
        int m = 0;
        in >> m;
        g_mutateMode = (m != 0);
        std::cout << "ok\n";
        return;
    }
    if (op == "boundaryvec") {
        std::uintptr_t base = 0;
        std::int32_t idx = 0;
        in >> base >> idx;
        void* const out = getTerrainBoundaryVector(reinterpret_cast<void*>(base), idx);
        const auto val = reinterpret_cast<std::uintptr_t>(out) + (g_mutateMode ? 4U : 0U);
        std::cout << val << "\n";
        return;
    }
    if (op == "setactiveboundary") {
        std::string terrainHex;
        std::int32_t newIdx = 0;
        in >> terrainHex >> newIdx;
        auto bytes = decodeHex(terrainHex);
        g_stubCallbackCalled = false;
        setTerrainActiveBoundary(bytes.data(), newIdx, &stubOnActiveBoundaryChanged);
        std::cout << encodeHex(bytes.data(), bytes.size()) << " "
                  << (g_stubCallbackCalled ? 1 : 0) << "\n";
        return;
    }
    if (op == "origingroundheight") {
        int hasNormal = 0;
        std::string normalHex;
        std::uint32_t retBits = 0;
        std::string normStubHex;
        in >> hasNormal >> normalHex >> retBits >> normStubHex;
        Coord3D normal = decodeStruct<Coord3D>(normalHex);
        g_stubGroundHeight = fromBits<float>(retBits);
        g_stubLayerNormal = decodeStruct<Coord3D>(normStubHex);
        const float res = getOriginGroundHeight(
            nullptr, 0U, hasNormal ? &normal : nullptr, &stubGetGroundHeight);
        std::cout << toBits(res) << " " << encodeHex(&normal, sizeof(normal)) << "\n";
        return;
    }
    if (op == "waterhandleheight") {
        int present = 0;
        std::int32_t rawInt = 0;
        in >> present >> rawInt;
        if (!present) {
            std::uint8_t fpu80[10]{};
#if defined(_MSC_VER) && defined(_M_IX86)
            __asm {
                push 0
                xor ecx, ecx
                call ra3::sim::getWaterHeightFromHandleExtended
                fstp tbyte ptr [fpu80]
            }
#endif
            std::cout << encodeHex(fpu80, sizeof(fpu80)) << "\n";
            return;
        }
        static std::int32_t s_rawWaterInt = 0;
        s_rawWaterInt = rawInt;
        auto cb = [](const void* /*sub*/) noexcept -> std::int32_t { return s_rawWaterInt; };
        const std::int32_t vbtable[2]{0, 0x10};
        std::uint8_t dummyHandle[0x40]{};
        const auto* const vbPtr = &vbtable[0];
        std::memcpy(dummyHandle + 0x1C, &vbPtr, sizeof(vbPtr));
        const float val = getWaterHeightFromHandle(dummyHandle, cb);
        std::uint8_t fpu80[10]{};
#if defined(_MSC_VER) && defined(_M_IX86)
        __asm {
            fild dword ptr [rawInt]
            fstp tbyte ptr [fpu80]
        }
#else
        (void)val;
#endif
        std::cout << encodeHex(fpu80, sizeof(fpu80)) << " " << toBits(val) << "\n";
        return;
    }
    if (op == "pointintri2d") {
        std::string p0H, p1H, p2H, ptH;
        int axis1 = 0, axis2 = 1;
        unsigned flagsIn = 0;
        in >> p0H >> p1H >> p2H >> ptH >> axis1 >> axis2 >> flagsIn;
        const Coord3D p0 = decodeStruct<Coord3D>(p0H);
        const Coord3D p1 = decodeStruct<Coord3D>(p1H);
        const Coord3D p2 = decodeStruct<Coord3D>(p2H);
        const Coord3D pt = decodeStruct<Coord3D>(ptH);
        auto flags = static_cast<std::uint8_t>(flagsIn);
        const bool ok = pointInTriangle2D(p0, p1, p2, pt, axis1, axis2, flags);
        std::cout << (ok ? 1 : 0) << " " << static_cast<unsigned>(flags) << "\n";
        return;
    }
    if (op == "findposopts") {
        std::string hex;
        in >> hex;
        FindPositionOptions opts = decodeStruct<FindPositionOptions>(hex);
        initializeFindPositionOptions(opts);
        std::cout << encodeHex(&opts, sizeof(opts)) << "\n";
        return;
    }
    if (op == "bridgeinfo") {
        std::string hex;
        in >> hex;
        BridgeInfo info = decodeStruct<BridgeInfo>(hex);
        initializeBridgeInfo(info);
        std::cout << encodeHex(&info, sizeof(info)) << "\n";
        return;
    }
    if (op == "pointonbridge") {
        std::string bridgeHex, locHex;
        int polyRet = 0;
        in >> bridgeHex >> locHex >> polyRet;
        const BridgeData bridge = decodeStruct<BridgeData>(bridgeHex);
        const Coord3D loc = decodeStruct<Coord3D>(locHex);
        g_stubPolyResult = (polyRet != 0);
        g_lastPolyICoord = {0, 0, 0};
        const bool ok = isPointOnBridge(bridge, &loc, &stubPolygonTriggerTest);
        std::cout << (ok ? 1 : 0) << " "
                  << encodeHex(&g_lastPolyICoord, sizeof(g_lastPolyICoord)) << "\n";
        return;
    }
    if (op == "hasboundaries") {
        std::uintptr_t b = 0, e = 0;
        in >> b >> e;
        std::uint8_t tl[0x60]{};
        *reinterpret_cast<std::uintptr_t*>(tl + 0x50) = b;
        *reinterpret_cast<std::uintptr_t*>(tl + 0x54) = e;
        std::cout << (hasTerrainBoundaries(tl) ? 1 : 0) << "\n";
        return;
    }
    if (op == "boundary2d") {
        std::string elemsHex;
        std::uint32_t idx = 0, initX = 0, initY = 0;
        in >> elemsHex >> idx >> initX >> initY;
        const auto elems = decodeHex(elemsHex);
        std::uint8_t tl[0x60]{};
        const auto base = reinterpret_cast<std::uintptr_t>(elems.data());
        *reinterpret_cast<std::uintptr_t*>(tl + 0x50) = base;
        *reinterpret_cast<std::uintptr_t*>(tl + 0x54) = base + elems.size();
        float outX = fromBits<float>(initX);
        float outY = fromBits<float>(initY);
        const bool ok = getTerrainBoundary2D(tl, idx, &outX, &outY);
        std::cout << (ok ? 1 : 0) << " " << toBits(outX) << " " << toBits(outY) << "\n";
        return;
    }
    if (op == "lineinregion2d") {
        std::string p1H, p2H, regH;
        in >> p1H >> p2H >> regH;
        const Coord2D p1 = decodeStruct<Coord2D>(p1H);
        const Coord2D p2 = decodeStruct<Coord2D>(p2H);
        const Region2D reg = decodeStruct<Region2D>(regH);
        std::cout << (lineInRegion2D(&p1, &p2, &reg) ? 1 : 0) << "\n";
        return;
    }
    if (op == "addwplink") {
        std::string wp1H, wp2H;
        std::int32_t id1 = 0, id2 = 0;
        std::uintptr_t simWp1Addr = 0, simWp2Addr = 0;
        in >> wp1H >> wp2H >> id1 >> id2 >> simWp1Addr >> simWp2Addr;
        WaypointData wp1 = decodeStruct<WaypointData>(wp1H);
        WaypointData wp2 = decodeStruct<WaypointData>(wp2H);
        // Translate simulated addresses in existing link slots to &wp1 / &wp2
        auto toLocal = [&](std::uintptr_t val) -> std::uintptr_t {
            if (val == simWp1Addr) return reinterpret_cast<std::uintptr_t>(&wp1);
            if (val == simWp2Addr) return reinterpret_cast<std::uintptr_t>(&wp2);
            return val;
        };
        auto toSim = [&](std::uintptr_t val) -> std::uintptr_t {
            if (val == reinterpret_cast<std::uintptr_t>(&wp1)) return simWp1Addr;
            if (val == reinterpret_cast<std::uintptr_t>(&wp2)) return simWp2Addr;
            return val;
        };
        for (auto& s : wp1.links) s = toLocal(s);
        for (auto& s : wp2.links) s = toLocal(s);
        wp1.prevLink = toLocal(wp1.prevLink);
        wp2.prevLink = toLocal(wp2.prevLink);
        wp1.next = &wp2;
        wp2.next = nullptr;
        addWaypointLink(&wp1, id1, id2);
        for (auto& s : wp1.links) s = toSim(s);
        for (auto& s : wp2.links) s = toSim(s);
        wp1.prevLink = toSim(wp1.prevLink);
        wp2.prevLink = toSim(wp2.prevLink);
        wp1.next = reinterpret_cast<WaypointData*>(simWp2Addr);
        wp2.next = nullptr;
        std::cout << encodeHex(&wp1, sizeof(wp1)) << " "
                  << encodeHex(&wp2, sizeof(wp2)) << "\n";
        return;
    }
    if (op == "basegroundheight") {
        std::uint32_t xB = 0, yB = 0;
        int hasNorm = 0;
        std::string normH;
        in >> xB >> yB >> hasNorm >> normH;
        Coord3D norm = decodeStruct<Coord3D>(normH);
        Coord3D* const normPtr = hasNorm ? &norm : nullptr;
        std::uint8_t fpu80[10]{};
#if defined(_MSC_VER) && defined(_M_IX86)
        const float x = fromBits<float>(xB);
        const float y = fromBits<float>(yB);
        __asm {
            push normPtr
            push dword ptr [y]
            push dword ptr [x]
            xor ecx, ecx
            call ra3::sim::getBaseGroundHeightExtended
            fstp tbyte ptr [fpu80]
        }
#endif
        std::cout << encodeHex(fpu80, sizeof(fpu80)) << " "
                  << encodeHex(&norm, sizeof(norm)) << "\n";
        return;
    }
    if (op == "baselayerheight") {
        std::uint32_t xB = 0, yB = 0;
        std::int32_t layer = 0;
        int hasNorm = 0, clip = 0;
        std::string normH;
        in >> xB >> yB >> layer >> hasNorm >> normH >> clip;
        Coord3D norm = decodeStruct<Coord3D>(normH);
        Coord3D* const normPtr = hasNorm ? &norm : nullptr;
        std::uint8_t fpu80[10]{};
#if defined(_MSC_VER) && defined(_M_IX86)
        const float x = fromBits<float>(xB);
        const float y = fromBits<float>(yB);
        const std::uint32_t clipW = clip ? 1U : 0U;
        __asm {
            push clipW
            push normPtr
            push layer
            push dword ptr [y]
            push dword ptr [x]
            xor ecx, ecx
            call ra3::sim::getBaseLayerHeightExtended
            fstp tbyte ptr [fpu80]
        }
#endif
        std::cout << encodeHex(fpu80, sizeof(fpu80)) << " "
                  << encodeHex(&norm, sizeof(norm)) << "\n";
        return;
    }
    if (op == "closestedge") {
        std::string outH, closestH, extH;
        std::uint32_t gzB = 0;
        in >> outH >> closestH >> extH >> gzB;
        Coord3D out = decodeStruct<Coord3D>(outH);
        const Coord3D closest = decodeStruct<Coord3D>(closestH);
        const Region3D ext = decodeStruct<Region3D>(extH);
        g_stubGroundHeight = fromBits<float>(gzB);
        findClosestEdgePoint(reinterpret_cast<void*>(1), &out, &closest, ext, &stubGetGroundHeight);
        std::cout << encodeHex(&out, sizeof(out)) << "\n";
        return;
    }
    if (op == "clipline") {
        std::string outH, p1H, p2H, extH;
        std::uint32_t lzB = 0;
        in >> outH >> p1H >> p2H >> extH >> lzB;
        Coord3D out = decodeStruct<Coord3D>(outH);
        const Coord3D p1 = decodeStruct<Coord3D>(p1H);
        const Coord3D p2 = decodeStruct<Coord3D>(p2H);
        const Region3D ext = decodeStruct<Region3D>(extH);
        g_stubLayerHeight = fromBits<float>(lzB);
        const bool ok = clipLineToMapExtent(
            reinterpret_cast<void*>(1), &out, &p1, &p2, ext, &stubGetLayerHeight);
        std::cout << (ok ? 1 : 0) << " " << encodeHex(&out, sizeof(out)) << "\n";
        return;
    }
    if (op == "farthestedge") {
        std::string outH, farH, extH;
        std::uint32_t gzB = 0;
        in >> outH >> farH >> extH >> gzB;
        Coord3D out = decodeStruct<Coord3D>(outH);
        const Coord3D farPt = decodeStruct<Coord3D>(farH);
        const Region3D ext = decodeStruct<Region3D>(extH);
        g_stubGroundHeight = fromBits<float>(gzB);
        findFarthestEdgePoint(reinterpret_cast<void*>(1), &out, &farPt, ext, &stubGetGroundHeight);
        std::cout << encodeHex(&out, sizeof(out)) << "\n";
        return;
    }
    if (op == "boundaryregion") {
        std::string iregH, outH;
        std::int32_t idx = 0;
        in >> iregH >> idx >> outH;
        const auto iregs = decodeHex(iregH);
        Region2D out = decodeStruct<Region2D>(outH);
        std::uint8_t tl[0x60]{};
        *reinterpret_cast<const std::uint8_t**>(tl + 0x54) = iregs.data();
        getTerrainBoundaryRegion(tl, &out, idx);
        std::cout << encodeHex(&out, sizeof(out)) << "\n";
        return;
    }
    if (op == "clamptoextent") {
        int hasTl = 0;
        std::string posH, extH;
        std::uint32_t gzB = 0;
        in >> hasTl >> posH >> extH >> gzB;
        Coord3D pos = decodeStruct<Coord3D>(posH);
        const Region3D ext = decodeStruct<Region3D>(extH);
        g_stubGroundHeight = fromBits<float>(gzB);
        const bool ok = clampPositionToMapExtent(
            hasTl ? reinterpret_cast<void*>(1) : nullptr, &pos, ext, &stubGetGroundHeight);
        std::cout << (ok ? 1 : 0) << " " << encodeHex(&pos, sizeof(pos)) << "\n";
        return;
    }
    if (op == "planeset") {
        std::string plH, v1H, v2H, v3H;
        in >> plH >> v1H >> v2H >> v3H;
        Plane3D pl = decodeStruct<Plane3D>(plH);
        const Coord3D v1 = decodeStruct<Coord3D>(v1H);
        const Coord3D v2 = decodeStruct<Coord3D>(v2H);
        const Coord3D v3 = decodeStruct<Coord3D>(v3H);
        setPlaneFromPoints(pl, v1, v2, v3);
        std::cout << encodeHex(&pl, sizeof(pl)) << "\n";
        return;
    }
    if (op == "bridgeproject") {
        std::string brH, locH;
        in >> brH >> locH;
        const BridgeData br = decodeStruct<BridgeData>(brH);
        const Coord3D loc = decodeStruct<Coord3D>(locH);
        std::uint8_t fpu80[10]{};
#if defined(_MSC_VER) && defined(_M_IX86)
        const BridgeData* const brPtr = &br;
        const Coord3D* const locPtr = &loc;
        __asm {
            push locPtr
            mov ecx, brPtr
            call ra3::sim::projectPointAlongBridgeSpanExtended
            fstp tbyte ptr [fpu80]
        }
#endif
        std::cout << encodeHex(fpu80, sizeof(fpu80)) << "\n";
        return;
    }
    if (op == "bridgecellend") {
        std::string brH, cellH;
        in >> brH >> cellH;
        const BridgeData br = decodeStruct<BridgeData>(brH);
        const Region2D cell = decodeStruct<Region2D>(cellH);
        std::cout << (isCellOnBridgeEnd(br, &cell) ? 1 : 0) << "\n";
        return;
    }
    if (op == "bridgecellside") {
        std::string brH, cellH;
        in >> brH >> cellH;
        const BridgeData br = decodeStruct<BridgeData>(brH);
        const Region2D cell = decodeStruct<Region2D>(cellH);
        std::cout << (isCellOnBridgeSide(br, &cell) ? 1 : 0) << "\n";
        return;
    }
    if (op == "bridgecellentry") {
        std::string brH, cellH;
        std::uint32_t initH = 0;
        in >> brH >> cellH >> initH;
        const BridgeData br = decodeStruct<BridgeData>(brH);
        const Region2D cell = decodeStruct<Region2D>(cellH);
        float outH = fromBits<float>(initH);
        const bool ok = isCellBridgeEntryPoint(br, &cell, &outH);
        std::cout << (ok ? 1 : 0) << " " << toBits(outH) << "\n";
        return;
    }
    if (op == "pathfindcenter") {
        std::string extH, outH;
        in >> extH >> outH;
        const Region3D ext = decodeStruct<Region3D>(extH);
        Coord3D out = decodeStruct<Coord3D>(outH);
        getMaximumPathfindExtentCenter(ext, &out);
        std::cout << encodeHex(&out, sizeof(out)) << "\n";
        return;
    }
    if (op == "alignmatrix") {
        std::uint32_t angB = 0;
        std::string posH, normH, mtxH;
        in >> angB >> posH >> normH >> mtxH;
        const Coord3D pos = decodeStruct<Coord3D>(posH);
        const Coord3D norm = decodeStruct<Coord3D>(normH);
        Matrix3x4 mtx = decodeStruct<Matrix3x4>(mtxH);
        makeAlignToNormalMatrix(fromBits<float>(angB), pos, norm, mtx);
        std::cout << encodeHex(&mtx, sizeof(mtx)) << "\n";
        return;
    }
    if (op == "findbridgeat") {
        std::string b1H, b2H, locH;
        in >> b1H >> b2H >> locH;
        BridgeData b1 = decodeStruct<BridgeData>(b1H);
        BridgeData b2 = decodeStruct<BridgeData>(b2H);
        const Coord3D loc = decodeStruct<Coord3D>(locH);
        b1.next = &b2;
        b2.next = nullptr;
        std::uint8_t tl[0x70]{};
        *reinterpret_cast<BridgeData**>(tl + 0x68) = &b1;
        const BridgeData* const res = findBridgeAt(tl, &loc);
        int idx = 0;
        if (res == &b1) idx = 1;
        else if (res == &b2) idx = 2;
        std::cout << idx << "\n";
        return;
    }
    if (op == "findbridgelayerat") {
        std::string b1H, b2H, locH;
        std::int32_t layer = 0;
        int clip = 0;
        in >> b1H >> b2H >> locH >> layer >> clip;
        BridgeData b1 = decodeStruct<BridgeData>(b1H);
        BridgeData b2 = decodeStruct<BridgeData>(b2H);
        const Coord3D loc = decodeStruct<Coord3D>(locH);
        b1.next = &b2;
        b2.next = nullptr;
        std::uint8_t tl[0x70]{};
        *reinterpret_cast<BridgeData**>(tl + 0x68) = &b1;
        const BridgeData* const res = findBridgeLayerAt(tl, &loc, layer, clip != 0);
        int idx = 0;
        if (res == &b1) idx = 1;
        else if (res == &b2) idx = 2;
        std::cout << idx << "\n";
        return;
    }
    if (op == "wpsetbi") {
        std::string wpH;
        int bi = 0;
        in >> wpH >> bi;
        WaypointData wp = decodeStruct<WaypointData>(wpH);
        setWaypointBiDirectional(wp, bi != 0);
        std::cout << encodeHex(&wp, sizeof(wp)) << "\n";
        return;
    }
    if (op == "wpgetbi") {
        std::string wpH;
        in >> wpH;
        const WaypointData wp = decodeStruct<WaypointData>(wpH);
        std::cout << (getWaypointBiDirectional(wp) ? 1 : 0) << "\n";
        return;
    }
    if (op == "pointonintactbridge") {
        std::string brH, spansH, locH;
        in >> brH >> spansH >> locH;
        BridgeData br = decodeStruct<BridgeData>(brH);
        const auto spans = decodeHex(spansH);
        const Coord3D loc = decodeStruct<Coord3D>(locH);
        if (spans.empty()) {
            br.spanBegin = nullptr;
            br.spanEnd = nullptr;
        } else {
            br.spanBegin = reinterpret_cast<const BridgeSpanInterval*>(spans.data());
            br.spanEnd = br.spanBegin + (spans.size() / sizeof(BridgeSpanInterval));
        }
        std::cout << (isPointOnIntactBridge(br, &loc) ? 1 : 0) << "\n";
        return;
    }
    if (op == "pickbridge") {
        std::string brH, spansH, fromH, toH, outH;
        in >> brH >> spansH >> fromH >> toH >> outH;
        BridgeData br = decodeStruct<BridgeData>(brH);
        const auto spans = decodeHex(spansH);
        const Coord3D from = decodeStruct<Coord3D>(fromH);
        const Coord3D to = decodeStruct<Coord3D>(toH);
        Coord3D out = decodeStruct<Coord3D>(outH);
        if (spans.empty()) {
            br.spanBegin = nullptr;
            br.spanEnd = nullptr;
        } else {
            br.spanBegin = reinterpret_cast<const BridgeSpanInterval*>(spans.data());
            br.spanEnd = br.spanBegin + (spans.size() / sizeof(BridgeSpanInterval));
        }
        const bool ok = pickBridge(br, from, to, &out);
        std::cout << (ok ? 1 : 0) << " " << encodeHex(&out, sizeof(out)) << "\n";
        return;
    }
    if (op == "bridgeheight") {
        std::string brH, locH, normH;
        int hasNorm = 0;
        in >> brH >> locH >> hasNorm >> normH;
        const BridgeData br = decodeStruct<BridgeData>(brH);
        const Coord3D loc = decodeStruct<Coord3D>(locH);
        Coord3D norm = decodeStruct<Coord3D>(normH);
        Coord3D* const normPtr = hasNorm ? &norm : nullptr;
        std::uint8_t fpu80[10]{};
#if defined(_MSC_VER) && defined(_M_IX86)
        const BridgeData* const brPtr = &br;
        const Coord3D* const locPtr = &loc;
        __asm {
            push normPtr
            push locPtr
            mov ecx, brPtr
            call ra3::sim::getBridgeHeightExtended
            fstp tbyte ptr [fpu80]
        }
#endif
        std::cout << encodeHex(fpu80, sizeof(fpu80)) << " "
                  << encodeHex(&norm, sizeof(norm)) << "\n";
        return;
    }
    if (op == "layerfordest") {
        std::string brH, spansH, posH;
        std::uint32_t gwB = 0;
        in >> brH >> spansH >> posH >> gwB;
        BridgeData br = decodeStruct<BridgeData>(brH);
        const auto spans = decodeHex(spansH);
        const Coord3D pos = decodeStruct<Coord3D>(posH);
        if (spans.empty()) {
            br.spanBegin = nullptr;
            br.spanEnd = nullptr;
        } else {
            br.spanBegin = reinterpret_cast<const BridgeSpanInterval*>(spans.data());
            br.spanEnd = br.spanBegin + (spans.size() / sizeof(BridgeSpanInterval));
        }
        br.next = nullptr;
        std::uint8_t tl[0x70]{};
        *reinterpret_cast<BridgeData**>(tl + 0x68) = &br;
        std::cout << getLayerForDestination(tl, &pos, fromBits<float>(gwB)) << "\n";
        return;
    }
    if (op == "highestlayerfordest") {
        std::string brH, spansH, posH;
        int onlyHealthy = 0;
        std::uint32_t gB = 0;
        in >> brH >> spansH >> posH >> onlyHealthy >> gB;
        BridgeData br = decodeStruct<BridgeData>(brH);
        const auto spans = decodeHex(spansH);
        const Coord3D pos = decodeStruct<Coord3D>(posH);
        if (spans.empty()) {
            br.spanBegin = nullptr;
            br.spanEnd = nullptr;
        } else {
            br.spanBegin = reinterpret_cast<const BridgeSpanInterval*>(spans.data());
            br.spanEnd = br.spanBegin + (spans.size() / sizeof(BridgeSpanInterval));
        }
        br.next = nullptr;
        std::uint8_t tl[0x70]{};
        *reinterpret_cast<BridgeData**>(tl + 0x68) = &br;
        std::cout << getHighestLayerForDestination(
                         tl, &pos, onlyHealthy != 0, fromBits<float>(gB))
                  << "\n";
        return;
    }
    if (op == "highestbridgelayerfordest") {
        std::string brH, spansH, posH;
        int onlyHealthy = 0;
        std::uint32_t gB = 0;
        in >> brH >> spansH >> posH >> onlyHealthy >> gB;
        BridgeData br = decodeStruct<BridgeData>(brH);
        const auto spans = decodeHex(spansH);
        const Coord3D pos = decodeStruct<Coord3D>(posH);
        if (spans.empty()) {
            br.spanBegin = nullptr;
            br.spanEnd = nullptr;
        } else {
            br.spanBegin = reinterpret_cast<const BridgeSpanInterval*>(spans.data());
            br.spanEnd = br.spanBegin + (spans.size() / sizeof(BridgeSpanInterval));
        }
        br.next = nullptr;
        std::uint8_t tl[0x70]{};
        *reinterpret_cast<BridgeData**>(tl + 0x68) = &br;
        std::cout << getHighestBridgeLayerForDestination(
                         tl, &pos, onlyHealthy != 0, fromBits<float>(gB))
                  << "\n";
        return;
    }
    if (op == "firstwaterheight") {
        int hasTable = 0, count = 0;
        std::int32_t rawInt = 0;
        in >> hasTable >> count >> rawInt;
        WaterTableTarget target{};
        target.height = rawInt;
        WaterTablePair pairs[2]{{1U, &target}, {2U, &target}};
        WaterTableContainer table{};
        table.begin = &pairs[0];
        table.end = &pairs[count];
        std::uint8_t tl[0x50]{};
        *reinterpret_cast<const WaterTableContainer**>(tl + 0x30) =
            hasTable ? &table : nullptr;
        std::uint8_t fpu80[10]{};
#if defined(_MSC_VER) && defined(_M_IX86)
        const void* const tlPtr = tl;
        __asm {
            mov ecx, tlPtr
            call ra3::sim::getFirstWaterTableHeightExtended
            fstp tbyte ptr [fpu80]
        }
#endif
        std::cout << encodeHex(fpu80, sizeof(fpu80)) << "\n";
        return;
    }
    if (op == "alignonterrain") {
        std::string brH, spansH, posH, mtxH, normH;
        std::uint32_t angB = 0, gwB = 0, lzB = 0;
        int stick = 0;
        in >> brH >> spansH >> angB >> posH >> stick >> mtxH >> gwB >> lzB >> normH;
        BridgeData br = decodeStruct<BridgeData>(brH);
        const auto spans = decodeHex(spansH);
        const Coord3D pos = decodeStruct<Coord3D>(posH);
        Matrix3x4 mtx = decodeStruct<Matrix3x4>(mtxH);
        g_stubLayerNormal = decodeStruct<Coord3D>(normH);
        g_stubLayerHeight = fromBits<float>(lzB);
        if (spans.empty()) {
            br.spanBegin = nullptr;
            br.spanEnd = nullptr;
        } else {
            br.spanBegin = reinterpret_cast<const BridgeSpanInterval*>(spans.data());
            br.spanEnd = br.spanBegin + (spans.size() / sizeof(BridgeSpanInterval));
        }
        br.next = nullptr;
        std::uint8_t tl[0x70]{};
        *reinterpret_cast<BridgeData**>(tl + 0x68) = &br;
        const std::int32_t layer = alignOnTerrain(
            tl, fromBits<float>(angB), pos, stick != 0, mtx,
            fromBits<float>(gwB), &stubGetLayerHeight);
        std::cout << layer << " " << encodeHex(&mtx, sizeof(mtx)) << "\n";
        return;
    }
    if (op == "updatewatergrid") {
        int hasTable = 0, count = 0;
        std::int32_t rawInt = 0;
        std::uint32_t initBits = 0;
        in >> hasTable >> count >> rawInt >> initBits;
        WaterTableTarget target{};
        target.height = rawInt;
        WaterTablePair pairs[2]{{1U, &target}, {2U, &target}};
        WaterTableContainer table{};
        table.begin = &pairs[0];
        table.end = &pairs[count];
        std::uint8_t tl[0x50]{};
        *reinterpret_cast<const WaterTableContainer**>(tl + 0x30) =
            hasTable ? &table : nullptr;
        *reinterpret_cast<std::uint32_t*>(tl + 0x3C) = initBits;
        updateCachedWaterGridHeight(tl);
        std::cout << *reinterpret_cast<const std::uint32_t*>(tl + 0x3C) << "\n";
        return;
    }
    if (op == "subsystemname") {
        const char* const name = getTerrainLogicSubsystemName(nullptr);
        std::cout << encodeHex(name, 13U) << "\n";
        return;
    }
    if (op == "resolvewplinks") {
        std::string wpH;
        std::uintptr_t baseAddr = 0;
        in >> wpH >> baseAddr;
        WaypointData wp = decodeStruct<WaypointData>(wpH);
        resolveWaypointLinks(wp, &stubLookupWaypoint, reinterpret_cast<void*>(baseAddr));
        std::cout << encodeHex(&wp, sizeof(wp)) << "\n";
        return;
    }
    if (op == "newmapwps") {
        std::string wp1H, wp2H;
        std::uint32_t gzB = 0;
        std::uintptr_t simWp2Addr = 0;
        in >> wp1H >> wp2H >> gzB >> simWp2Addr;
        WaypointData wp1 = decodeStruct<WaypointData>(wp1H);
        WaypointData wp2 = decodeStruct<WaypointData>(wp2H);
        wp1.next = &wp2;
        wp2.next = nullptr;
        g_stubGroundHeight = fromBits<float>(gzB);
        g_stubCallbackCalled = false;
        adjustNewMapWaypoints(
            reinterpret_cast<void*>(1), &wp1, false,
            &stubGetGroundHeight, &stubOnActiveBoundaryChanged);
        wp1.next = reinterpret_cast<WaypointData*>(simWp2Addr);
        wp2.next = nullptr;
        std::cout << encodeHex(&wp1, sizeof(wp1)) << " "
                  << encodeHex(&wp2, sizeof(wp2)) << " "
                  << (g_stubCallbackCalled ? 1 : 0) << "\n";
        return;
    }
    if (op == "findposaround") {
        std::string centerH, optsH, resH, extH, rngH;
        int acceptIdx = -1;
        in >> centerH >> optsH >> resH >> extH >> rngH >> acceptIdx;
        const Coord3D center = decodeStruct<Coord3D>(centerH);
        const FindPositionOptions opts = decodeStruct<FindPositionOptions>(optsH);
        Coord3D res = decodeStruct<Coord3D>(resH);
        const Region3D ext = decodeStruct<Region3D>(extH);
        RandomState rng = decodeStruct<RandomState>(rngH);
        g_tryPosAcceptIndex = acceptIdx;
        g_tryPosCallCount = 0;
        const bool ok = findPositionAround(
            &center, &opts, &res, ext, rng, &stubTryPosition, nullptr);
        std::cout << (ok ? 1 : 0) << " "
                  << encodeHex(&res, sizeof(res)) << " "
                  << encodeHex(&rng, sizeof(rng)) << " "
                  << g_tryPosCallCount << "\n";
        return;
    }
    throw std::runtime_error("unknown command: " + op);
}

}  // namespace

int main(int argc, char** argv) {
    if (argc > 1 && std::strcmp(argv[1], "--selftest") == 0) {
        return runSelfTest();
    }
    try {
        std::string line;
        while (std::getline(std::cin, line)) {
            if (line == "quit" || line == "exit") {
                return 0;
            }
            if (line == "selftest") {
                return runSelfTest();
            }
            dispatchCommand(line);
        }
        return 0;
    } catch (const std::exception&) {
        std::cout << "error\n";
        return 1;
    }
}
