#include <ra3/sim/TerrainLogic.h>

#include "support/TestHarness.h"

#include <cmath>
#include <cstring>

namespace {

bool nearEq(float a, float b, float eps = 1e-4F) {
    return std::fabs(a - b) <= eps;
}

float flatGround(void* /*tl*/, float /*x*/, float /*y*/, ra3::sim::Coord3D* normal) noexcept {
    if (normal != nullptr) {
        *normal = {0.0F, 0.0F, 1.0F};
    }
    return 12.0F;
}

float flatLayer(
    void* /*tl*/, float /*x*/, float /*y*/, std::int32_t layer,
    ra3::sim::Coord3D* normal, bool /*clip*/) noexcept {
    if (normal != nullptr) {
        *normal = {0.0F, 0.0F, 1.0F};
    }
    return (layer > 1) ? 30.0F : 12.0F;
}

bool acceptSecondRing(
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

}  // namespace

RA3_TEST(TerrainLogicStructInitializers) {
    using namespace ra3::sim;

    FindPositionOptions opts{};
    std::memset(&opts, 0xFF, sizeof(opts));
    RA3_CHECK_EQ(initializeFindPositionOptions(opts), &opts);
    RA3_CHECK(opts.minRadius == 0.0F);
    RA3_CHECK(opts.maxRadius == 0.0F);
    RA3_CHECK(opts.startAngle == kRandomStartAngle);
    RA3_CHECK(opts.maxZDelta == kMaxFindPositionZDelta);
    RA3_CHECK_EQ(opts.ignoreObject, nullptr);
    RA3_CHECK_EQ(opts.sourceToPathToDest, nullptr);
    RA3_CHECK_EQ(opts.relationshipObject, nullptr);

    BridgeInfo info{};
    std::memset(&info, 0xFF, sizeof(info));
    RA3_CHECK_EQ(initializeBridgeInfo(info), &info);
    RA3_CHECK(info.fromLeft.x == 0.0F);
    RA3_CHECK(info.toRight.z == 0.0F);
    RA3_CHECK_EQ(info.bridgeObjectId, 0);
}

RA3_TEST(TerrainLogicWaypointBiDirectionalAccessors) {
    using namespace ra3::sim;

    WaypointData waypoint{};
    setWaypointBiDirectional(waypoint, true);
    RA3_CHECK_EQ(getWaypointBiDirectional(waypoint), 1U);
    setWaypointBiDirectional(waypoint, false);
    RA3_CHECK_EQ(getWaypointBiDirectional(waypoint), 0U);
    waypoint.biDirectional = 0x80U;
    RA3_CHECK_EQ(getWaypointBiDirectional(waypoint), 0x80U);
}

RA3_TEST(TerrainLogicGeometryAndClipping) {
    using namespace ra3::sim;

    const Coord3D a{0.0F, 0.0F, 0.0F};
    const Coord3D b{20.0F, 0.0F, 0.0F};
    const Coord3D c{0.0F, 20.0F, 0.0F};
    const Coord3D in{4.0F, 4.0F, 0.0F};
    const Coord3D out{15.0F, 15.0F, 0.0F};
    const Coord3D onEdge{10.0F, 0.0F, 0.0F};
    std::uint8_t flags = 0;
    RA3_CHECK(pointInTriangle2D(a, b, c, in, 0, 1, flags));
    RA3_CHECK_EQ(flags, 0U);
    RA3_CHECK(pointInTriangle2D(a, c, b, in, 0, 1, flags));
    RA3_CHECK(!pointInTriangle2D(a, b, c, out, 0, 1, flags));
    RA3_CHECK(pointInTriangle2D(a, b, c, onEdge, 0, 1, flags));
    RA3_CHECK((flags & 1U) != 0U);

    const Coord3D colB{10.0F, 0.0F, 0.0F};
    const Coord3D colC{20.0F, 0.0F, 0.0F};
    flags = 0;
    RA3_CHECK(pointInTriangle2D(a, colB, colC, onEdge, 0, 1, flags));
    RA3_CHECK((flags & 1U) != 0U);

    const Region2D region{{0.0F, 0.0F}, {50.0F, 50.0F}};
    const Coord2D p1{-10.0F, 25.0F};
    const Coord2D p2{60.0F, 25.0F};
    const Coord2D p3{-10.0F, -10.0F};
    const Coord2D p4{-2.0F, -2.0F};
    RA3_CHECK(lineInRegion2D(&p1, &p2, &region));
    RA3_CHECK(!lineInRegion2D(&p3, &p4, &region));

    Plane3D plane{};
    setPlaneFromPoints(plane, {0.0F, 0.0F, 10.0F}, {5.0F, 0.0F, 10.0F}, {0.0F, 5.0F, 10.0F});
    RA3_CHECK(nearEq(plane.normal.x, 0.0F, 1e-5F));
    RA3_CHECK(nearEq(plane.normal.y, 0.0F, 1e-5F));
    RA3_CHECK(nearEq(plane.normal.z, 1.0F, 1e-5F));
    RA3_CHECK(nearEq(plane.dist, 10.0F, 1e-5F));
}

RA3_TEST(TerrainLogicBridgesAndFindPositionAround) {
    using namespace ra3::sim;

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

    const Coord3D ptIntact{30.0F, 20.0F, 20.0F};
    const Coord3D ptHole{50.0F, 20.0F, 20.0F};
    RA3_CHECK(isPointOnBridge(bridge, &ptIntact));
    RA3_CHECK(nearEq(projectPointAlongBridgeSpan(bridge, &ptIntact), 30.0F));
    RA3_CHECK(isPointOnIntactBridge(bridge, &ptIntact));
    RA3_CHECK(!isPointOnIntactBridge(bridge, &ptHole));

    Coord3D hit{};
    RA3_CHECK(pickBridge(bridge, {30.0F, 20.0F, 50.0F}, {30.0F, 20.0F, 0.0F}, &hit));
    RA3_CHECK(nearEq(hit.x, 30.0F));
    RA3_CHECK(nearEq(hit.y, 20.0F));
    RA3_CHECK(nearEq(hit.z, 20.0F));

    Coord3D n{};
    RA3_CHECK(nearEq(getBridgeHeight(bridge, &ptIntact, &n), 20.0F));
    RA3_CHECK(nearEq(std::fabs(n.z), 1.0F));

    std::uint8_t fakeTerrain[0xA0]{};
    const Region3D extent{{0.0F, 0.0F, 0.0F}, {100.0F, 100.0F, 50.0F}};

    Coord3D pos{-20.0F, 40.0F, 0.0F};
    RA3_CHECK(clampPositionToMapExtent(fakeTerrain, &pos, extent, &flatGround));
    RA3_CHECK(nearEq(pos.x, 0.0F));
    RA3_CHECK(nearEq(pos.y, 40.0F));
    RA3_CHECK(nearEq(pos.z, 12.0F));

    Matrix3x4 mtx{};
    const Coord3D unitPos{25.0F, 25.0F, 12.0F};
    RA3_CHECK_EQ(
        alignOnTerrain(fakeTerrain, 0.0F, unitPos, true, mtx, 12.0F, &flatLayer),
        1);
    RA3_CHECK(nearEq(mtx.m[2][3], 12.0F));

    RandomState rng{};
    seedRandom(rng, 42U);
    FindPositionOptions opts{};
    initializeFindPositionOptions(opts);
    opts.minRadius = 0.0F;
    opts.maxRadius = 10.0F;
    Coord3D out{};
    RA3_CHECK(findPositionAround(
        &unitPos, &opts, &out, extent, rng, &acceptSecondRing));
    RA3_CHECK(nearEq(std::hypot(out.x - unitPos.x, out.y - unitPos.y), 5.0F, 1e-3F));
}
