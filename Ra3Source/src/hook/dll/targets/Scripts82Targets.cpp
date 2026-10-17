#include "../Targets.h"

#include <ra3/gamelogic/ScriptsCluster.h>

#include <cstdint>
#include <cstring>

namespace ra3hook {
namespace {

using ra3::gamelogic::DualHashKey;
using ra3::gamelogic::ScriptCoord3D;
using ra3::gamelogic::ScriptIndexedEntry;
using ra3::gamelogic::ScriptParameterView;
using ra3::gamelogic::SightingKey;

TargetState g_unitAction;
TargetState g_teamAction;
TargetState g_teamOrUnitAction;
TargetState g_paramCoord3D;
TargetState g_intVecSum;
TargetState g_sightingLess;
TargetState g_sightingFind;
TargetState g_sightingLowerBound;
TargetState g_indexedLowerBound;
TargetState g_dualHashFind;

template <typename T>
T readAt(const void* base, std::size_t offset) noexcept
{
    T value{};
    std::memcpy(&value, static_cast<const std::uint8_t*>(base) + offset, sizeof(T));
    return value;
}

std::uint32_t __fastcall hookIsUnitAction(const void* action, void*)
{
    TargetState& state = g_unitAction;
    state.calls.fetch_add(1, std::memory_order_relaxed);
    const auto actionType = readAt<std::int32_t>(action, 0x04);
    const std::uint32_t ours = ra3::gamelogic::isUnitActionEax(actionType);
    if (state.mode == Mode::replace) {
        state.matches.fetch_add(1, std::memory_order_relaxed);
        return ours;
    }
    const std::uint32_t game =
        reinterpret_cast<std::uint32_t(__thiscall*)(const void*)>(state.original)(action);
    recordScalar("scripts82unitaction", state, game, ours);
    return game;
}

std::uint32_t __fastcall hookIsTeamAction(const void* action, void*)
{
    TargetState& state = g_teamAction;
    state.calls.fetch_add(1, std::memory_order_relaxed);
    const auto actionType = readAt<std::int32_t>(action, 0x04);
    const std::uint32_t ours = ra3::gamelogic::isTeamActionEax(actionType);
    if (state.mode == Mode::replace) {
        state.matches.fetch_add(1, std::memory_order_relaxed);
        return ours;
    }
    const std::uint32_t game =
        reinterpret_cast<std::uint32_t(__thiscall*)(const void*)>(state.original)(action);
    recordScalar("scripts82teamaction", state, game, ours);
    return game;
}

std::uint32_t __fastcall hookIsTeamOrUnitAction(const void* action, void*)
{
    TargetState& state = g_teamOrUnitAction;
    state.calls.fetch_add(1, std::memory_order_relaxed);
    const auto actionType = readAt<std::int32_t>(action, 0x04);
    const std::uint32_t ours = ra3::gamelogic::isTeamOrUnitActionEax(actionType);
    if (state.mode == Mode::replace) {
        state.matches.fetch_add(1, std::memory_order_relaxed);
        return ours;
    }
    const std::uint32_t game =
        reinterpret_cast<std::uint32_t(__thiscall*)(const void*)>(state.original)(action);
    recordScalar("scripts82teamorunitaction", state, game, ours);
    return game;
}

ScriptCoord3D* __fastcall hookParamGetCoord3D(
    const void* param, void*, ScriptCoord3D* outCoord)
{
    TargetState& state = g_paramCoord3D;
    state.calls.fetch_add(1, std::memory_order_relaxed);
    ScriptParameterView view{};
    view.paramType = readAt<std::int32_t>(param, 0x00);
    view.coord = readAt<ScriptCoord3D>(param, 0x14);
    ScriptCoord3D ours{};
    ra3::gamelogic::getParameterCoord3D(view, ours);
    if (state.mode == Mode::replace) {
        *outCoord = ours;
        state.matches.fetch_add(1, std::memory_order_relaxed);
        return outCoord;
    }
    ScriptCoord3D* const gameRet =
        reinterpret_cast<ScriptCoord3D*(__thiscall*)(const void*, ScriptCoord3D*)>(
            state.original)(param, outCoord);
    recordOutput("scripts82paramcoord3d", state, outCoord, &ours, sizeof(ScriptCoord3D));
    return gameRet;
}

std::uint32_t __cdecl hookIntVecSum(const void* vecRange)
{
    TargetState& state = g_intVecSum;
    state.calls.fetch_add(1, std::memory_order_relaxed);
    const auto* const begin = readAt<const std::int32_t*>(vecRange, 0x00);
    const auto* const end = readAt<const std::int32_t*>(vecRange, 0x04);
    const std::uint32_t ours = ra3::gamelogic::sumIntVectorElements(begin, end);
    if (state.mode == Mode::replace) {
        state.matches.fetch_add(1, std::memory_order_relaxed);
        return ours;
    }
    const std::uint32_t game =
        reinterpret_cast<std::uint32_t(__cdecl*)(const void*)>(state.original)(vecRange);
    recordScalar("scripts82intvecsum", state, game, ours);
    return game;
}

std::uint32_t __stdcall hookSightingKeyLess(
    const SightingKey* lhs, const SightingKey* rhs)
{
    TargetState& state = g_sightingLess;
    state.calls.fetch_add(1, std::memory_order_relaxed);
    const std::uint32_t ours = ra3::gamelogic::sightingKeyLess(*lhs, *rhs) ? 1U : 0U;
    if (state.mode == Mode::replace) {
        state.matches.fetch_add(1, std::memory_order_relaxed);
        return ours;
    }
    const std::uint32_t game =
        reinterpret_cast<std::uint32_t(__stdcall*)(const SightingKey*, const SightingKey*)>(
            state.original)(lhs, rhs);
    recordScalar("scripts82sightingless", state, game, ours);
    return game;
}

void** __fastcall hookSightingTreeFind(
    void* tree, void*, void** outIter, const SightingKey* key)
{
    TargetState& state = g_sightingFind;
    state.calls.fetch_add(1, std::memory_order_relaxed);
    void* const ours = ra3::gamelogic::sightingTreeFindRaw(tree, *key);
    if (state.mode == Mode::replace) {
        *outIter = ours;
        state.matches.fetch_add(1, std::memory_order_relaxed);
        return outIter;
    }
    void** const gameRet =
        reinterpret_cast<void**(__thiscall*)(void*, void**, const SightingKey*)>(
            state.original)(tree, outIter, key);
    recordScalar(
        "scripts82sightingfind", state,
        static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(*outIter)),
        static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(ours)));
    return gameRet;
}

void** __fastcall hookSightingTreeLowerBound(
    void* tree, void*, void** outIter, const SightingKey* key)
{
    TargetState& state = g_sightingLowerBound;
    state.calls.fetch_add(1, std::memory_order_relaxed);
    void* const ours = ra3::gamelogic::sightingTreeLowerBoundRaw(tree, *key);
    if (state.mode == Mode::replace) {
        *outIter = ours;
        state.matches.fetch_add(1, std::memory_order_relaxed);
        return outIter;
    }
    void** const gameRet =
        reinterpret_cast<void**(__thiscall*)(void*, void**, const SightingKey*)>(
            state.original)(tree, outIter, key);
    recordScalar(
        "scripts82sightinglowerbound", state,
        static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(*outIter)),
        static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(ours)));
    return gameRet;
}

const ScriptIndexedEntry* __cdecl hookIndexedLowerBound(
    const ScriptIndexedEntry* first,
    const ScriptIndexedEntry* last,
    const std::int32_t* key)
{
    TargetState& state = g_indexedLowerBound;
    state.calls.fetch_add(1, std::memory_order_relaxed);
    const ScriptIndexedEntry* const ours =
        ra3::gamelogic::lowerBoundScriptIndexedEntry(first, last, *key);
    if (state.mode == Mode::replace) {
        state.matches.fetch_add(1, std::memory_order_relaxed);
        return ours;
    }
    const ScriptIndexedEntry* const game =
        reinterpret_cast<const ScriptIndexedEntry*(__cdecl*)(
            const ScriptIndexedEntry*, const ScriptIndexedEntry*, const std::int32_t*)>(
                state.original)(first, last, key);
    recordScalar(
        "scripts82indexedlowerbound", state,
        static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(game)),
        static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(ours)));
    return game;
}

void** __fastcall hookDualHashTreeFind(
    void* tree, void*, void** outIter, const DualHashKey* key)
{
    TargetState& state = g_dualHashFind;
    state.calls.fetch_add(1, std::memory_order_relaxed);
    void* const ours = ra3::gamelogic::dualHashTreeFindRaw(tree, *key);
    if (state.mode == Mode::replace) {
        *outIter = ours;
        state.matches.fetch_add(1, std::memory_order_relaxed);
        return outIter;
    }
    void** const gameRet =
        reinterpret_cast<void**(__thiscall*)(void*, void**, const DualHashKey*)>(
            state.original)(tree, outIter, key);
    recordScalar(
        "scripts82dualhashfind", state,
        static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(*outIter)),
        static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(ours)));
    return gameRet;
}

}  // namespace

void appendScripts82Targets(std::vector<Target>& table)
{
    table.insert(table.end(), {
        {"scripts82unitaction", 0x00404590,
         {0x8B, 0x41, 0x04, 0x3D, 0x29, 0x01, 0x00, 0x00,
          0x7F, 0x1A, 0x74, 0x32, 0x83, 0xE8, 0x21, 0x3D},
         8, reinterpret_cast<void*>(&hookIsUnitAction), &g_unitAction, true},
        {"scripts82teamaction", 0x00404780,
         {0x8B, 0x41, 0x04, 0x3D, 0x1A, 0x01, 0x00, 0x00,
          0x7F, 0x3E, 0x74, 0x56, 0x3D, 0x93, 0x00, 0x00},
         8, reinterpret_cast<void*>(&hookIsTeamAction), &g_teamAction, true},
        {"scripts82teamorunitaction", 0x004049A0,
         {0x8B, 0x41, 0x04, 0x3D, 0x22, 0x01, 0x00, 0x00,
          0x7F, 0x1F, 0x3D, 0x1F, 0x01, 0x00, 0x00, 0x7D},
         8, reinterpret_cast<void*>(&hookIsTeamOrUnitAction), &g_teamOrUnitAction, true},
        {"scripts82paramcoord3d", 0x0041A750,
         {0x8B, 0x44, 0x24, 0x04, 0x0F, 0x57, 0xC0, 0xF3,
          0x0F, 0x11, 0x00, 0xF3, 0x0F, 0x11, 0x40, 0x04},
         7, reinterpret_cast<void*>(&hookParamGetCoord3D), &g_paramCoord3D, true},
        {"scripts82intvecsum", 0x0041AA40,
         {0x8B, 0x54, 0x24, 0x04, 0x8B, 0x0A, 0x8B, 0x52,
          0x04, 0x33, 0xC0, 0x3B, 0xCA, 0x74, 0x0A, 0x90},
         6, reinterpret_cast<void*>(&hookIntVecSum), &g_intVecSum, true},
        {"scripts82sightingless", 0x0041AA60,
         {0x8B, 0x44, 0x24, 0x04, 0x8B, 0x4C, 0x24, 0x08,
          0x8B, 0x50, 0x0C, 0x56, 0x8B, 0x71, 0x0C, 0x85},
         8, reinterpret_cast<void*>(&hookSightingKeyLess), &g_sightingLess, true},
        {"scripts82sightingfind", 0x0041AAA0,
         {0x8B, 0x41, 0x0C, 0x85, 0xC0, 0x55, 0x56, 0x8B,
          0x74, 0x24, 0x10, 0x8D, 0x69, 0x04, 0x57, 0x8B},
         5, reinterpret_cast<void*>(&hookSightingTreeFind), &g_sightingFind, true},
        {"scripts82sightinglowerbound", 0x0041AB60,
         {0x8B, 0x51, 0x0C, 0x85, 0xD2, 0x53, 0x8D, 0x59,
          0x04, 0x74, 0x52, 0x56, 0x8B, 0x74, 0x24, 0x10},
         5, reinterpret_cast<void*>(&hookSightingTreeLowerBound), &g_sightingLowerBound, true},
        {"scripts82indexedlowerbound", 0x0041DE20,
         {0x8B, 0x4C, 0x24, 0x08, 0x56, 0x8B, 0x74, 0x24,
          0x08, 0x2B, 0xCE, 0xB8, 0xAB, 0xAA, 0xAA, 0x2A},
         5, reinterpret_cast<void*>(&hookIndexedLowerBound), &g_indexedLowerBound, true},
        {"scripts82dualhashfind", 0x0041DF50,
         {0x8B, 0x41, 0x0C, 0x85, 0xC0, 0x53, 0x8D, 0x59,
          0x04, 0x57, 0x8B, 0x7C, 0x24, 0x10, 0x8B, 0xD3},
         5, reinterpret_cast<void*>(&hookDualHashTreeFind), &g_dualHashFind, true},
    });
}

}  // namespace ra3hook
