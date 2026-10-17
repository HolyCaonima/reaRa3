#include "../Targets.h"

#include <ra3/sim/TerrainLogic.h>

#include <array>
#include <cstdint>
#include <cstring>

namespace ra3hook {
namespace {

constexpr auto relaxed = std::memory_order_relaxed;

using BoundaryVectorFn = void*(__thiscall*)(void* self, std::int32_t index);
using PointInTriangle2DFn = std::uint8_t(__cdecl*)(
    const ra3::sim::Coord3D* p0, const ra3::sim::Coord3D* p1,
    const ra3::sim::Coord3D* p2, const ra3::sim::Coord3D* pt,
    int axis1, int axis2, std::uint8_t* flags);
using FindPositionOptionsInitFn =
    ra3::sim::FindPositionOptions*(__thiscall*)(ra3::sim::FindPositionOptions* self);
using BridgeInfoInitFn = ra3::sim::BridgeInfo*(__thiscall*)(ra3::sim::BridgeInfo* self);
using TerrainHasBoundariesFn = std::uint8_t(__thiscall*)(const void* self);
using TerrainBoundary2DFn = std::uint8_t(__thiscall*)(
    const void* self, std::uint32_t index, float* outX, float* outY);
using LineInRegion2DFn = std::uint8_t(__cdecl*)(
    const ra3::sim::Coord2D* p1, const ra3::sim::Coord2D* p2,
    const ra3::sim::Region2D* clipRegion);
using BaseGroundHeightFn = float(__thiscall*)(
    const void* self, float x, float y, ra3::sim::Coord3D* normal);
using BaseLayerHeightFn = float(__thiscall*)(
    const void* self, float x, float y, std::int32_t layer,
    ra3::sim::Coord3D* normal, bool clip);
using TerrainBoundaryRegionFn = void(__thiscall*)(
    const void* self, ra3::sim::Region2D* outRegion, std::int32_t index);
using PlaneSetFromPointsFn = void(__thiscall*)(
    ra3::sim::Plane3D* self, const ra3::sim::Coord3D* v1,
    const ra3::sim::Coord3D* v2, const ra3::sim::Coord3D* v3);
using BridgeProjectAlongSpanFn = float(__thiscall*)(
    const ra3::sim::BridgeData* self, const ra3::sim::Coord3D* loc);
using BridgeCellRegionFn = std::uint8_t(__thiscall*)(
    const ra3::sim::BridgeData* self, const ra3::sim::Region2D* cell);
using BridgeCellEntryPointFn = std::uint8_t(__thiscall*)(
    const ra3::sim::BridgeData* self, const ra3::sim::Region2D* cell,
    float* outEntryHeight);
using MakeAlignToNormalMatrixFn = void(__cdecl*)(
    float angle, const ra3::sim::Coord3D* pos, const ra3::sim::Coord3D* normal,
    ra3::sim::Matrix3x4* mtx);
using WaypointSetBiDirectionalFn = void(__thiscall*)(
    ra3::sim::WaypointData* self, bool biDirectional);
using WaypointGetBiDirectionalFn = std::uint8_t(__thiscall*)(
    const ra3::sim::WaypointData* self);
using TerrainFirstWaterHeightFn = float(__thiscall*)(const void* self);
using TerrainUpdateWaterGridHeightFn = void(__thiscall*)(void* self);

TargetState g_terrainBoundaryVectorState;
TargetState g_pointInTriangle2DState;
TargetState g_findPositionOptionsInitState;
TargetState g_bridgeInfoInitState;
TargetState g_terrainHasBoundariesState;
TargetState g_terrainBoundary2DState;
TargetState g_lineInRegion2DState;
TargetState g_baseGroundHeightState;
TargetState g_baseLayerHeightState;
TargetState g_terrainBoundaryRegionState;
TargetState g_planeSetFromPointsState;
TargetState g_bridgeProjectAlongSpanState;
TargetState g_bridgeIsCellOnEndState;
TargetState g_bridgeIsCellOnSideState;
TargetState g_bridgeIsCellEntryPointState;
TargetState g_makeAlignToNormalMatrixState;
TargetState g_waypointSetBiDirectionalState;
TargetState g_waypointGetBiDirectionalState;
TargetState g_terrainFirstWaterHeightState;
TargetState g_terrainUpdateWaterGridHeightState;

void* __fastcall hookedTerrainBoundaryVector(
    void* self, void* /*edx*/, std::int32_t index) noexcept {
    auto& state = g_terrainBoundaryVectorState;
    state.calls.fetch_add(1, relaxed);
    void* const ours = ra3::sim::getTerrainBoundaryVector(self, index);
    if (state.mode == Mode::replace) {
        return ours;
    }
    void* const game = reinterpret_cast<BoundaryVectorFn>(state.original)(self, index);
    recordScalar(
        "terrainboundaryvec", state,
        static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(game)),
        static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(ours)));
    return game;
}

std::uint8_t __cdecl hookedPointInTriangle2D(
    const ra3::sim::Coord3D* p0, const ra3::sim::Coord3D* p1,
    const ra3::sim::Coord3D* p2, const ra3::sim::Coord3D* pt,
    int axis1, int axis2, std::uint8_t* flags) noexcept {
    auto& state = g_pointInTriangle2DState;
    state.calls.fetch_add(1, relaxed);
    std::uint8_t ourFlags = *flags;
    const auto ours = static_cast<std::uint8_t>(
        ra3::sim::pointInTriangle2D(*p0, *p1, *p2, *pt, axis1, axis2, ourFlags));
    if (state.mode == Mode::replace) {
        *flags = ourFlags;
        return ours;
    }
    std::uint8_t gameFlags = *flags;
    const auto game = reinterpret_cast<PointInTriangle2DFn>(state.original)(
        p0, p1, p2, pt, axis1, axis2, &gameFlags);
    *flags = gameFlags;
    const std::array<std::uint8_t, 2> gameOut{game, gameFlags};
    const std::array<std::uint8_t, 2> ourOut{ours, ourFlags};
    recordOutput("pointintriangle2d", state, gameOut.data(), ourOut.data(), gameOut.size());
    return game;
}

ra3::sim::FindPositionOptions* __fastcall hookedFindPositionOptionsInit(
    ra3::sim::FindPositionOptions* self, void* /*edx*/) noexcept {
    auto& state = g_findPositionOptionsInitState;
    state.calls.fetch_add(1, relaxed);
    if (state.mode == Mode::replace) {
        return ra3::sim::initializeFindPositionOptions(*self);
    }
    ra3::sim::FindPositionOptions ourCopy = *self;
    ra3::sim::initializeFindPositionOptions(ourCopy);
    auto* const gameRet = reinterpret_cast<FindPositionOptionsInitFn>(state.original)(self);
    recordOutput("findposoptionsinit", state, self, &ourCopy, sizeof(ourCopy));
    return gameRet;
}

ra3::sim::BridgeInfo* __fastcall hookedBridgeInfoInit(
    ra3::sim::BridgeInfo* self, void* /*edx*/) noexcept {
    auto& state = g_bridgeInfoInitState;
    state.calls.fetch_add(1, relaxed);
    if (state.mode == Mode::replace) {
        return ra3::sim::initializeBridgeInfo(*self);
    }
    ra3::sim::BridgeInfo ourCopy = *self;
    ra3::sim::initializeBridgeInfo(ourCopy);
    auto* const gameRet = reinterpret_cast<BridgeInfoInitFn>(state.original)(self);
    recordOutput("bridgeinfoinit", state, self, &ourCopy, sizeof(ourCopy));
    return gameRet;
}

std::uint8_t __fastcall hookedTerrainHasBoundaries(
    const void* self, void* /*edx*/) noexcept {
    auto& state = g_terrainHasBoundariesState;
    state.calls.fetch_add(1, relaxed);
    const auto ours = static_cast<std::uint8_t>(ra3::sim::hasTerrainBoundaries(self));
    if (state.mode == Mode::replace) {
        return ours;
    }
    const auto game = reinterpret_cast<TerrainHasBoundariesFn>(state.original)(self);
    recordScalar("terrainhasboundaries", state, game, ours);
    return game;
}

std::uint8_t __fastcall hookedTerrainBoundary2D(
    const void* self, void* /*edx*/, std::uint32_t index, float* outX, float* outY) noexcept {
    auto& state = g_terrainBoundary2DState;
    state.calls.fetch_add(1, relaxed);
    if (state.mode == Mode::replace) {
        return static_cast<std::uint8_t>(ra3::sim::getTerrainBoundary2D(self, index, outX, outY));
    }
    float ourX = *outX;
    float ourY = *outY;
    const auto ours = static_cast<std::uint8_t>(
        ra3::sim::getTerrainBoundary2D(self, index, &ourX, &ourY));
    const auto game = reinterpret_cast<TerrainBoundary2DFn>(state.original)(
        self, index, outX, outY);
    if (game != ours) {
        recordScalar("terrainboundary2d", state, game, ours);
    } else if (game != 0U) {
        const std::array<float, 2> gameXY{*outX, *outY};
        const std::array<float, 2> ourXY{ourX, ourY};
        recordOutput("terrainboundary2d", state, gameXY.data(), ourXY.data(), sizeof(gameXY));
    } else {
        recordScalar("terrainboundary2d", state, game, ours);
    }
    return game;
}

std::uint8_t __cdecl hookedLineInRegion2D(
    const ra3::sim::Coord2D* p1, const ra3::sim::Coord2D* p2,
    const ra3::sim::Region2D* clipRegion) noexcept {
    auto& state = g_lineInRegion2DState;
    state.calls.fetch_add(1, relaxed);
    const auto ours = static_cast<std::uint8_t>(ra3::sim::lineInRegion2D(p1, p2, clipRegion));
    if (state.mode == Mode::replace) {
        return ours;
    }
    const auto game = reinterpret_cast<LineInRegion2DFn>(state.original)(p1, p2, clipRegion);
    recordScalar("lineinregion2d", state, game, ours);
    return game;
}

#if defined(_MSC_VER) && defined(_M_IX86)
void __fastcall shadowBaseGroundHeight(
    const void* self, void* /*edx*/, float x, float y,
    ra3::sim::Coord3D* normal, std::uint8_t* outGameFpu) noexcept {
    auto& state = g_baseGroundHeightState;
    state.calls.fetch_add(1, relaxed);
    const auto origFn = reinterpret_cast<BaseGroundHeightFn>(state.original);
    std::uint8_t ourFpu[10]{};
    ra3::sim::Coord3D ourNormal{};
    ra3::sim::Coord3D* const ourNormalPtr = (normal != nullptr) ? &ourNormal : nullptr;
    if (normal != nullptr) {
        ourNormal = *normal;
    }
    __asm {
        push normal
        push dword ptr [y]
        push dword ptr [x]
        mov ecx, self
        call origFn
        mov eax, outGameFpu
        fstp tbyte ptr [eax]

        push ourNormalPtr
        push dword ptr [y]
        push dword ptr [x]
        mov ecx, self
        call ra3::sim::getBaseGroundHeightExtended
        fstp tbyte ptr [ourFpu]
    }
    if (normal != nullptr) {
        std::array<std::uint8_t, 22> gameBuf{};
        std::array<std::uint8_t, 22> ourBuf{};
        std::memcpy(gameBuf.data(), outGameFpu, 10);
        std::memcpy(gameBuf.data() + 10, normal, 12);
        std::memcpy(ourBuf.data(), ourFpu, 10);
        std::memcpy(ourBuf.data() + 10, &ourNormal, 12);
        recordOutput("terraingroundheightbase", state, gameBuf.data(), ourBuf.data(), gameBuf.size());
    } else {
        recordOutput("terraingroundheightbase", state, outGameFpu, ourFpu, 10);
    }
}

__declspec(naked) float __fastcall hookedBaseGroundHeight(
    const void* /*self*/, void* /*edx*/, float /*x*/, float /*y*/,
    ra3::sim::Coord3D* /*normal*/) noexcept {
    __asm {
        cmp dword ptr [g_baseGroundHeightState], 2
        je replace_mode
        sub esp, 0Ch
        lea eax, [esp]
        push eax
        push dword ptr [esp + 1Ch]
        push dword ptr [esp + 1Ch]
        push dword ptr [esp + 1Ch]
        call shadowBaseGroundHeight
        fld tbyte ptr [esp]
        add esp, 0Ch
        ret 0Ch
    replace_mode:
        lock add dword ptr [g_baseGroundHeightState + 08h], 1
        adc dword ptr [g_baseGroundHeightState + 0Ch], 0
        jmp ra3::sim::getBaseGroundHeightExtended
    }
}

void __fastcall shadowBaseLayerHeight(
    const void* self, void* /*edx*/, float x, float y, std::int32_t layer,
    ra3::sim::Coord3D* normal, bool clip, std::uint8_t* outGameFpu) noexcept {
    auto& state = g_baseLayerHeightState;
    state.calls.fetch_add(1, relaxed);
    const auto origFn = reinterpret_cast<BaseLayerHeightFn>(state.original);
    std::uint8_t ourFpu[10]{};
    ra3::sim::Coord3D ourNormal{};
    ra3::sim::Coord3D* const ourNormalPtr = (normal != nullptr) ? &ourNormal : nullptr;
    if (normal != nullptr) {
        ourNormal = *normal;
    }
    const std::uint32_t clipWord = clip ? 1U : 0U;
    __asm {
        push clipWord
        push normal
        push dword ptr [layer]
        push dword ptr [y]
        push dword ptr [x]
        mov ecx, self
        call origFn
        mov eax, outGameFpu
        fstp tbyte ptr [eax]

        push clipWord
        push ourNormalPtr
        push dword ptr [layer]
        push dword ptr [y]
        push dword ptr [x]
        mov ecx, self
        call ra3::sim::getBaseLayerHeightExtended
        fstp tbyte ptr [ourFpu]
    }
    if (normal != nullptr) {
        std::array<std::uint8_t, 22> gameBuf{};
        std::array<std::uint8_t, 22> ourBuf{};
        std::memcpy(gameBuf.data(), outGameFpu, 10);
        std::memcpy(gameBuf.data() + 10, normal, 12);
        std::memcpy(ourBuf.data(), ourFpu, 10);
        std::memcpy(ourBuf.data() + 10, &ourNormal, 12);
        recordOutput("terrainlayerheightbase", state, gameBuf.data(), ourBuf.data(), gameBuf.size());
    } else {
        recordOutput("terrainlayerheightbase", state, outGameFpu, ourFpu, 10);
    }
}

__declspec(naked) float __fastcall hookedBaseLayerHeight(
    const void* /*self*/, void* /*edx*/, float /*x*/, float /*y*/, std::int32_t /*layer*/,
    ra3::sim::Coord3D* /*normal*/, bool /*clip*/) noexcept {
    __asm {
        cmp dword ptr [g_baseLayerHeightState], 2
        je replace_mode
        sub esp, 0Ch
        lea eax, [esp]
        push eax
        push dword ptr [esp + 24h]
        push dword ptr [esp + 24h]
        push dword ptr [esp + 24h]
        push dword ptr [esp + 24h]
        push dword ptr [esp + 24h]
        call shadowBaseLayerHeight
        fld tbyte ptr [esp]
        add esp, 0Ch
        ret 14h
    replace_mode:
        lock add dword ptr [g_baseLayerHeightState + 08h], 1
        adc dword ptr [g_baseLayerHeightState + 0Ch], 0
        jmp ra3::sim::getBaseLayerHeightExtended
    }
}
#endif

void __fastcall hookedTerrainBoundaryRegion(
    const void* self, void* /*edx*/, ra3::sim::Region2D* outRegion,
    std::int32_t index) noexcept {
    auto& state = g_terrainBoundaryRegionState;
    state.calls.fetch_add(1, relaxed);
    if (state.mode == Mode::replace) {
        ra3::sim::getTerrainBoundaryRegion(self, outRegion, index);
        return;
    }
    ra3::sim::Region2D ourRegion = *outRegion;
    ra3::sim::getTerrainBoundaryRegion(self, &ourRegion, index);
    reinterpret_cast<TerrainBoundaryRegionFn>(state.original)(self, outRegion, index);
    recordOutput("terrainboundaryregion", state, outRegion, &ourRegion, sizeof(ourRegion));
}

void __fastcall hookedPlaneSetFromPoints(
    ra3::sim::Plane3D* self, void* /*edx*/, const ra3::sim::Coord3D* v1,
    const ra3::sim::Coord3D* v2, const ra3::sim::Coord3D* v3) noexcept {
    auto& state = g_planeSetFromPointsState;
    state.calls.fetch_add(1, relaxed);
    if (state.mode == Mode::replace) {
        ra3::sim::setPlaneFromPoints(*self, *v1, *v2, *v3);
        return;
    }
    ra3::sim::Plane3D ourPlane = *self;
    ra3::sim::setPlaneFromPoints(ourPlane, *v1, *v2, *v3);
    reinterpret_cast<PlaneSetFromPointsFn>(state.original)(self, v1, v2, v3);
    recordOutput("planesetfrompoints", state, self, &ourPlane, sizeof(ourPlane));
}

#if defined(_MSC_VER) && defined(_M_IX86)
void __fastcall shadowBridgeProjectAlongSpan(
    const ra3::sim::BridgeData* self, void* /*edx*/, const ra3::sim::Coord3D* loc,
    std::uint8_t* outGameFpu) noexcept {
    auto& state = g_bridgeProjectAlongSpanState;
    state.calls.fetch_add(1, relaxed);
    const auto origFn = reinterpret_cast<BridgeProjectAlongSpanFn>(state.original);
    std::uint8_t ourFpu[10]{};
    __asm {
        push loc
        mov ecx, self
        call origFn
        mov eax, outGameFpu
        fstp tbyte ptr [eax]

        push loc
        mov ecx, self
        call ra3::sim::projectPointAlongBridgeSpanExtended
        fstp tbyte ptr [ourFpu]
    }
    recordOutput("bridgeprojectalongspan", state, outGameFpu, ourFpu, 10);
}

__declspec(naked) float __fastcall hookedBridgeProjectAlongSpan(
    const ra3::sim::BridgeData* /*self*/, void* /*edx*/,
    const ra3::sim::Coord3D* /*loc*/) noexcept {
    __asm {
        cmp dword ptr [g_bridgeProjectAlongSpanState], 2
        je replace_mode
        sub esp, 0Ch
        lea eax, [esp]
        push eax
        push dword ptr [esp + 14h]
        call shadowBridgeProjectAlongSpan
        fld tbyte ptr [esp]
        add esp, 0Ch
        ret 4
    replace_mode:
        lock add dword ptr [g_bridgeProjectAlongSpanState + 08h], 1
        adc dword ptr [g_bridgeProjectAlongSpanState + 0Ch], 0
        jmp ra3::sim::projectPointAlongBridgeSpanExtended
    }
}
#endif

std::uint8_t __fastcall hookedBridgeIsCellOnEnd(
    const ra3::sim::BridgeData* self, void* /*edx*/,
    const ra3::sim::Region2D* cell) noexcept {
    auto& state = g_bridgeIsCellOnEndState;
    state.calls.fetch_add(1, relaxed);
    const auto ours = static_cast<std::uint8_t>(ra3::sim::isCellOnBridgeEnd(*self, cell));
    if (state.mode == Mode::replace) {
        return ours;
    }
    const auto game = reinterpret_cast<BridgeCellRegionFn>(state.original)(self, cell);
    recordScalar("bridgecellonend", state, game, ours);
    return game;
}

std::uint8_t __fastcall hookedBridgeIsCellOnSide(
    const ra3::sim::BridgeData* self, void* /*edx*/,
    const ra3::sim::Region2D* cell) noexcept {
    auto& state = g_bridgeIsCellOnSideState;
    state.calls.fetch_add(1, relaxed);
    const auto ours = static_cast<std::uint8_t>(ra3::sim::isCellOnBridgeSide(*self, cell));
    if (state.mode == Mode::replace) {
        return ours;
    }
    const auto game = reinterpret_cast<BridgeCellRegionFn>(state.original)(self, cell);
    recordScalar("bridgecellonside", state, game, ours);
    return game;
}

std::uint8_t __fastcall hookedBridgeIsCellEntryPoint(
    const ra3::sim::BridgeData* self, void* /*edx*/, const ra3::sim::Region2D* cell,
    float* outEntryHeight) noexcept {
    auto& state = g_bridgeIsCellEntryPointState;
    state.calls.fetch_add(1, relaxed);
    if (state.mode == Mode::replace) {
        return static_cast<std::uint8_t>(
            ra3::sim::isCellBridgeEntryPoint(*self, cell, outEntryHeight));
    }
    float ourHeight = *outEntryHeight;
    const auto ours = static_cast<std::uint8_t>(
        ra3::sim::isCellBridgeEntryPoint(*self, cell, &ourHeight));
    const auto game = reinterpret_cast<BridgeCellEntryPointFn>(state.original)(
        self, cell, outEntryHeight);
    if (game != ours) {
        recordScalar("bridgecellentrypoint", state, game, ours);
    } else if (game != 0U) {
        recordOutput("bridgecellentrypoint", state, outEntryHeight, &ourHeight, sizeof(float));
    } else {
        recordScalar("bridgecellentrypoint", state, game, ours);
    }
    return game;
}

void __cdecl hookedMakeAlignToNormalMatrix(
    float angle, const ra3::sim::Coord3D* pos, const ra3::sim::Coord3D* normal,
    ra3::sim::Matrix3x4* mtx) noexcept {
    auto& state = g_makeAlignToNormalMatrixState;
    state.calls.fetch_add(1, relaxed);
    if (state.mode == Mode::replace) {
        ra3::sim::makeAlignToNormalMatrix(angle, *pos, *normal, *mtx);
        return;
    }
    ra3::sim::Matrix3x4 ourMtx = *mtx;
    ra3::sim::makeAlignToNormalMatrix(angle, *pos, *normal, ourMtx);
    reinterpret_cast<MakeAlignToNormalMatrixFn>(state.original)(angle, pos, normal, mtx);
    recordOutput("aligntonormalmatrix", state, mtx, &ourMtx, sizeof(ourMtx));
}

void __fastcall hookedWaypointSetBiDirectional(
    ra3::sim::WaypointData* self, void* /*edx*/, bool biDirectional) noexcept {
    auto& state = g_waypointSetBiDirectionalState;
    state.calls.fetch_add(1, relaxed);
    if (state.mode == Mode::replace) {
        ra3::sim::setWaypointBiDirectional(*self, biDirectional);
        return;
    }
    ra3::sim::WaypointData ourWp = *self;
    ra3::sim::setWaypointBiDirectional(ourWp, biDirectional);
    reinterpret_cast<WaypointSetBiDirectionalFn>(state.original)(self, biDirectional);
    recordScalar("waypointsetbidir", state, self->biDirectional, ourWp.biDirectional);
}

// The native body is `mov al, [ecx+5Ch]; ret`: AL is the raw stored byte and the
// upper EAX bits are caller residue, which no caller can consume.
std::uint8_t __fastcall hookedWaypointGetBiDirectional(
    const ra3::sim::WaypointData* self, void* /*edx*/) noexcept {
    auto& state = g_waypointGetBiDirectionalState;
    state.calls.fetch_add(1, relaxed);
    const std::uint8_t ours = ra3::sim::getWaypointBiDirectional(*self);
    if (state.mode == Mode::replace) {
        return ours;
    }
    const std::uint8_t game =
        reinterpret_cast<WaypointGetBiDirectionalFn>(state.original)(self);
    recordScalar("waypointgetbidir", state, game, ours);
    return game;
}

#if defined(_MSC_VER) && defined(_M_IX86)
void __fastcall shadowTerrainFirstWaterHeight(
    const void* self, void* /*edx*/, std::uint8_t* outGameFpu) noexcept {
    auto& state = g_terrainFirstWaterHeightState;
    state.calls.fetch_add(1, relaxed);
    const auto origFn = reinterpret_cast<TerrainFirstWaterHeightFn>(state.original);
    std::uint8_t ourFpu[10]{};
    __asm {
        mov ecx, self
        call origFn
        mov eax, outGameFpu
        fstp tbyte ptr [eax]

        mov ecx, self
        call ra3::sim::getFirstWaterTableHeightExtended
        fstp tbyte ptr [ourFpu]
    }
    recordOutput("terrainfirstwaterheight", state, outGameFpu, ourFpu, 10);
}

__declspec(naked) float __fastcall hookedTerrainFirstWaterHeight(
    const void* /*self*/, void* /*edx*/) noexcept {
    __asm {
        cmp dword ptr [g_terrainFirstWaterHeightState], 2
        je replace_mode
        sub esp, 0Ch
        lea eax, [esp]
        push eax
        call shadowTerrainFirstWaterHeight
        fld tbyte ptr [esp]
        add esp, 0Ch
        ret
    replace_mode:
        lock add dword ptr [g_terrainFirstWaterHeightState + 08h], 1
        adc dword ptr [g_terrainFirstWaterHeightState + 0Ch], 0
        jmp ra3::sim::getFirstWaterTableHeightExtended
    }
}
#endif

void __fastcall hookedTerrainUpdateWaterGridHeight(void* self, void* /*edx*/) noexcept {
    auto& state = g_terrainUpdateWaterGridHeightState;
    state.calls.fetch_add(1, relaxed);
    if (state.mode == Mode::replace) {
        ra3::sim::updateCachedWaterGridHeight(self);
        return;
    }
    auto* const bytes = static_cast<std::uint8_t*>(self);
    auto* const slot = reinterpret_cast<float*>(bytes + 0x3C);
    const float saved = *slot;
    ra3::sim::updateCachedWaterGridHeight(self);
    const float ourVal = *slot;
    *slot = saved;
    reinterpret_cast<TerrainUpdateWaterGridHeightFn>(state.original)(self);
    recordOutput("terrainupdatewatergrid", state, slot, &ourVal, sizeof(float));
}

}  // namespace

void appendTerrainLogicTargets(std::vector<Target>& table) {
    table.insert(table.end(), {
        {
            "terrainboundaryvec",
            0x0045C5E0U,
            {0x8B, 0x44, 0x24, 0x04, 0x8D, 0x44, 0xC1, 0x50,
             0xC2, 0x04, 0x00, 0xCC, 0xCC, 0xCC, 0xCC, 0xCC},
            8U,
            reinterpret_cast<void*>(&hookedTerrainBoundaryVector),
            &g_terrainBoundaryVectorState,
            true,
        },
        {
            "pointintriangle2d",
            0x0045D110U,
            {0x83, 0xEC, 0x24, 0x8B, 0x54, 0x24, 0x3C, 0x8B,
             0x4C, 0x24, 0x38, 0x53, 0x8B, 0x5C, 0x24, 0x30},
            7U,
            reinterpret_cast<void*>(&hookedPointInTriangle2D),
            &g_pointInTriangle2DState,
            false,
        },
        {
            "findposoptionsinit",
            0x0045D7E0U,
            {0x0F, 0x57, 0xC0, 0x8B, 0xC1, 0x33, 0xC9, 0x89,
             0x08, 0x89, 0x48, 0x04, 0x89, 0x48, 0x08, 0x89},
            5U,
            reinterpret_cast<void*>(&hookedFindPositionOptionsInit),
            &g_findPositionOptionsInitState,
            true,
        },
        {
            "bridgeinfoinit",
            0x0045D840U,
            {0x0F, 0x57, 0xC0, 0x8B, 0xC1, 0xF3, 0x0F, 0x11,
             0x00, 0xF3, 0x0F, 0x11, 0x40, 0x04, 0xF3, 0x0F},
            5U,
            reinterpret_cast<void*>(&hookedBridgeInfoInit),
            &g_bridgeInfoInitState,
            true,
        },
        {
            "terrainhasboundaries",
            0x0045DA00U,
            {0x8B, 0x41, 0x50, 0x33, 0xD2, 0x3B, 0x41, 0x54,
             0x0F, 0x95, 0xC2, 0x8A, 0xC2, 0xC3, 0xCC, 0xCC},
            5U,
            reinterpret_cast<void*>(&hookedTerrainHasBoundaries),
            &g_terrainHasBoundariesState,
            true,
        },
        {
            "terrainboundary2d",
            0x0045DA10U,
            {0x8B, 0x51, 0x54, 0x2B, 0x51, 0x50, 0x8B, 0x44,
             0x24, 0x04, 0xC1, 0xFA, 0x03, 0x3B, 0xC2, 0x72},
            6U,
            reinterpret_cast<void*>(&hookedTerrainBoundary2D),
            &g_terrainBoundary2DState,
            true,
        },
        {
            "lineinregion2d",
            0x0045DA50U,
            {0x83, 0xEC, 0x08, 0x8B, 0x44, 0x24, 0x14, 0xF3,
             0x0F, 0x10, 0x30, 0xF3, 0x0F, 0x10, 0x78, 0x08},
            7U,
            reinterpret_cast<void*>(&hookedLineInRegion2D),
            &g_lineInRegion2DState,
            false,
        },
        {
            "terraingroundheightbase",
            0x0045DDF0U,
            {0x8B, 0x44, 0x24, 0x0C, 0xD9, 0xEE, 0x85, 0xC0,
             0x74, 0x11, 0x0F, 0x57, 0xC0, 0xF3, 0x0F, 0x11},
            6U,
            reinterpret_cast<void*>(&hookedBaseGroundHeight),
            &g_baseGroundHeightState,
            true,
        },
        {
            "terrainlayerheightbase",
            0x0045DE60U,
            {0x8B, 0x44, 0x24, 0x10, 0xD9, 0xEE, 0x85, 0xC0,
             0x74, 0x11, 0x0F, 0x57, 0xC0, 0xF3, 0x0F, 0x11},
            6U,
            reinterpret_cast<void*>(&hookedBaseLayerHeight),
            &g_baseLayerHeightState,
            true,
        },
        {
            "terrainboundaryregion",
            0x0045E7F0U,
            {0x8B, 0x44, 0x24, 0x08, 0xF3, 0x0F, 0x10, 0x05,
             0x84, 0x95, 0xBD, 0x00, 0xC1, 0xE0, 0x04, 0x03},
            12U,
            reinterpret_cast<void*>(&hookedTerrainBoundaryRegion),
            &g_terrainBoundaryRegionState,
            true,
        },
        {
            "planesetfrompoints",
            0x0045EC40U,
            {0x83, 0xEC, 0x0C, 0x8B, 0x44, 0x24, 0x18, 0xF3,
             0x0F, 0x10, 0x20, 0xF3, 0x0F, 0x10, 0x68, 0x04},
            7U,
            reinterpret_cast<void*>(&hookedPlaneSetFromPoints),
            &g_planeSetFromPointsState,
            true,
        },
        {
            "bridgeprojectalongspan",
            0x0045FA50U,
            {0x83, 0xEC, 0x0C, 0xF3, 0x0F, 0x10, 0x41, 0x1C,
             0xF3, 0x0F, 0x5C, 0x41, 0x04, 0xF3, 0x0F, 0x10},
            8U,
            reinterpret_cast<void*>(&hookedBridgeProjectAlongSpan),
            &g_bridgeProjectAlongSpanState,
            true,
        },
        {
            "bridgecellonend",
            0x0045FAD0U,
            {0x83, 0xEC, 0x48, 0xF3, 0x0F, 0x10, 0x49, 0x14,
             0xD9, 0xE8, 0xF3, 0x0F, 0x10, 0x51, 0x18, 0xF3},
            8U,
            reinterpret_cast<void*>(&hookedBridgeIsCellOnEnd),
            &g_bridgeIsCellOnEndState,
            true,
        },
        {
            "bridgecellonside",
            0x0045FD60U,
            {0x83, 0xEC, 0x4C, 0xF3, 0x0F, 0x10, 0x59, 0x10,
             0xF3, 0x0F, 0x5C, 0x59, 0x04, 0xF3, 0x0F, 0x10},
            8U,
            reinterpret_cast<void*>(&hookedBridgeIsCellOnSide),
            &g_bridgeIsCellOnSideState,
            true,
        },
        {
            "bridgecellentrypoint",
            0x00460020U,
            {0x83, 0xEC, 0x44, 0xF3, 0x0F, 0x10, 0x41, 0x14,
             0xD9, 0xE8, 0xF3, 0x0F, 0x5C, 0x41, 0x08, 0xF3},
            8U,
            reinterpret_cast<void*>(&hookedBridgeIsCellEntryPoint),
            &g_bridgeIsCellEntryPointState,
            true,
        },
        {
            "aligntonormalmatrix",
            0x004603B0U,
            {0x83, 0xEC, 0x24, 0xD9, 0x44, 0x24, 0x28, 0x8B,
             0x44, 0x24, 0x30, 0xD9, 0xC0, 0x8B, 0x08, 0xD9},
            7U,
            reinterpret_cast<void*>(&hookedMakeAlignToNormalMatrix),
            &g_makeAlignToNormalMatrixState,
            false,
        },
        {
            "waypointsetbidir",
            0x00460A20U,
            {0x8A, 0x44, 0x24, 0x04, 0x88, 0x41, 0x5C, 0xC2,
             0x04, 0x00, 0xCC, 0xCC, 0xCC, 0xCC, 0xCC, 0xCC},
            7U,
            reinterpret_cast<void*>(&hookedWaypointSetBiDirectional),
            &g_waypointSetBiDirectionalState,
            true,
        },
        {
            "waypointgetbidir",
            0x00460A30U,
            {0x8A, 0x41, 0x5C, 0xC3, 0xCC, 0xCC, 0xCC, 0xCC,
             0xCC, 0xCC, 0xCC, 0xCC, 0xCC, 0xCC, 0xCC, 0xCC},
            5U,
            reinterpret_cast<void*>(&hookedWaypointGetBiDirectional),
            &g_waypointGetBiDirectionalState,
            true,
        },
        {
            "terrainfirstwaterheight",
            0x004627F0U,
            {0x8B, 0x49, 0x30, 0x8B, 0x41, 0x1C, 0x2B, 0x41,
             0x18, 0xC1, 0xF8, 0x03, 0x3B, 0xC9, 0x75, 0x04},
            6U,
            reinterpret_cast<void*>(&hookedTerrainFirstWaterHeight),
            &g_terrainFirstWaterHeightState,
            true,
        },
        {
            "terrainupdatewatergrid",
            0x00464190U,
            {0x8B, 0x41, 0x30, 0x8B, 0x50, 0x1C, 0x2B, 0x50,
             0x18, 0xC1, 0xFA, 0x03, 0x3B, 0xC0, 0x75, 0x04},
            6U,
            reinterpret_cast<void*>(&hookedTerrainUpdateWaterGridHeight),
            &g_terrainUpdateWaterGridHeightState,
            true,
        },
    });
}

}  // namespace ra3hook
