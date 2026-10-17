#include "../Targets.h"

#include <ra3/gamelogic/AITNGuardViews.h>

#include <cstdint>
#include <cstring>

namespace ra3hook {
namespace {

using ra3::gamelogic::Coord2D;

TargetState g_guardPayload;
TargetState g_guardEndpointPair;
TargetState g_guardObjectCooldown;
TargetState g_guardTargetCooldown;
TargetState g_guardTrackedTarget;
TargetState g_guardCandidateNear;
TargetState g_guardPointNear;
TargetState g_guardReturnNear;

std::uint8_t* bytes(void* pointer)
{
    return static_cast<std::uint8_t*>(pointer);
}

template <typename T>
T read(const void* pointer, std::size_t offset)
{
    T value{};
    std::memcpy(&value, static_cast<const std::uint8_t*>(pointer) + offset, sizeof(value));
    return value;
}

void* __fastcall guardPayload(void* object, void*)
{
    TargetState& state = g_guardPayload;
    state.calls.fetch_add(1, std::memory_order_relaxed);
    void* const nested = read<void*>(object, 0x90);
    const std::int32_t mode = nested == nullptr ? 0 : read<std::int32_t>(nested, 0x70);
    const std::int32_t offset =
        ra3::gamelogic::conditionalPayloadOffset(nested != nullptr, mode);
    void* const ours = offset < 0 ? nullptr : bytes(nested) + offset;
    if (state.mode == Mode::replace) {
        state.matches.fetch_add(1, std::memory_order_relaxed);
        return ours;
    }
    void* const game =
        reinterpret_cast<void*(__thiscall*)(void*)>(state.original)(object);
    recordScalar(
        "skirmishguardpayload", state,
        static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(game)),
        static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(ours)));
    return game;
}

std::uint32_t __cdecl guardEndpointPair(
    std::uint32_t first, std::uint32_t second,
    std::uint32_t expectedFirst, std::uint32_t expectedSecond)
{
    TargetState& state = g_guardEndpointPair;
    state.calls.fetch_add(1, std::memory_order_relaxed);
    const std::uint32_t ours = ra3::gamelogic::classifyEndpointPair(
        first, second, expectedFirst, expectedSecond);
    if (state.mode == Mode::replace) {
        state.matches.fetch_add(1, std::memory_order_relaxed);
        return ours;
    }
    const std::uint32_t game =
        reinterpret_cast<std::uint32_t(__cdecl*)(
            std::uint32_t, std::uint32_t, std::uint32_t, std::uint32_t)>(
                state.original)(first, second, expectedFirst, expectedSecond);
    recordScalar("skirmishguardpair", state, game, ours);
    return game;
}

std::uint8_t* gameLogic()
{
    return *reinterpret_cast<std::uint8_t**>(0x00DEBA64);
}

std::uint32_t __fastcall guardObjectCooldown(
    void* table, void*, const void* object, int slot)
{
    TargetState& state = g_guardObjectCooldown;
    state.calls.fetch_add(1, std::memory_order_relaxed);
    const auto currentId = read<std::uint32_t>(object, 0xDC);
    const auto previousId = read<std::uint32_t>(table, 0x34 + slot * 4U);
    const auto frame = read<std::int32_t>(gameLogic(), 0x58);
    const auto previousFrame = read<std::int32_t>(table, 0x40 + slot * 4U);
    const float cooldown = *reinterpret_cast<const float*>(0x00CC3374);
    const std::uint32_t ours = ra3::gamelogic::objectCooldownResult(
        currentId, previousId, frame, previousFrame, cooldown,
        static_cast<std::uint32_t>(slot));
    if (state.mode == Mode::replace) {
        state.matches.fetch_add(1, std::memory_order_relaxed);
        return ours;
    }
    const std::uint32_t game = reinterpret_cast<std::uint32_t(__thiscall*)(
        void*, const void*, int)>(state.original)(table, object, slot);
    recordScalar("skirmishguardobjectcooldown", state, game, ours);
    return game;
}

std::uint32_t __fastcall guardTargetCooldown(
    void* table, void*, const void* object, int slot, int mode)
{
    TargetState& state = g_guardTargetCooldown;
    state.calls.fetch_add(1, std::memory_order_relaxed);
    const auto currentId = read<std::uint32_t>(object, 0xDC);
    const auto previousId = read<std::uint32_t>(table, 0x50 + slot * 4U);
    const auto frame = read<std::int32_t>(gameLogic(), 0x58);
    const auto previousFrame = read<std::int32_t>(table, 0x5C + slot * 4U);
    const float cooldown = *reinterpret_cast<const float*>(0x00CC32F4);
    const std::uint32_t ours = ra3::gamelogic::targetCooldownResult(
        currentId, previousId, frame, previousFrame, cooldown, mode,
        static_cast<std::uint32_t>(slot));
    if (state.mode == Mode::replace) {
        state.matches.fetch_add(1, std::memory_order_relaxed);
        return ours;
    }
    const std::uint32_t game = reinterpret_cast<std::uint32_t(__thiscall*)(
        void*, const void*, int, int)>(state.original)(table, object, slot, mode);
    recordScalar("skirmishguardtargetcooldown", state, game, ours);
    return game;
}

void __fastcall guardTrackedTarget(void* stateObject, void*, void* target)
{
    TargetState& state = g_guardTrackedTarget;
    state.calls.fetch_add(1, std::memory_order_relaxed);
    const std::uint32_t id = target == nullptr
        ? 0U
        : read<std::uint32_t>(read<void*>(target, 0x80), 0xDC);
    struct Snapshot {
        void* pointer;
        std::uint32_t id;
    };
    const Snapshot ours{target, ra3::gamelogic::trackedTargetId(target != nullptr, id)};
    if (state.mode == Mode::replace) {
        std::memcpy(bytes(stateObject) + 0x84, &ours, sizeof(ours));
        state.matches.fetch_add(1, std::memory_order_relaxed);
        return;
    }
    reinterpret_cast<void(__thiscall*)(void*, void*)>(
        state.original)(stateObject, target);
    recordOutput(
        "skirmishguardtrackedtarget", state,
        bytes(stateObject) + 0x84, &ours, sizeof(ours));
}

std::uint32_t __cdecl guardCandidateNear(
    const void* object, const Coord2D* point)
{
    TargetState& state = g_guardCandidateNear;
    state.calls.fetch_add(1, std::memory_order_relaxed);
    bool near = false;
    const bool disabled =
        object != nullptr && (read<std::uint8_t>(object, 0x471) & 1U) != 0U;
    if (object != nullptr && (read<std::uint8_t>(object, 0x471) & 1U) == 0U) {
        const Coord2D center{
            read<float>(object, 0x38), read<float>(object, 0x3C)};
        near = ra3::gamelogic::isNear2D(
            *point, center, *reinterpret_cast<const float*>(0x00BD95B8));
    }
    const auto pointer = static_cast<std::uint32_t>(
        reinterpret_cast<std::uintptr_t>(object));
    const std::uint32_t ours =
        ra3::gamelogic::candidateNearResult(pointer, disabled, near);
    if (state.mode == Mode::replace) {
        state.matches.fetch_add(1, std::memory_order_relaxed);
        return ours;
    }
    const std::uint32_t game = reinterpret_cast<std::uint32_t(__cdecl*)(
        const void*, const Coord2D*)>(state.original)(object, point);
    recordScalar("skirmishguardcandidatenear", state, game, ours);
    return game;
}

int shadowPointNear(
    const char* key, TargetState& state, const void* object,
    const Coord2D* point, std::size_t offset)
{
    state.calls.fetch_add(1, std::memory_order_relaxed);
    const Coord2D center{
        read<float>(object, offset), read<float>(object, offset + 4U)};
    const int ours = ra3::gamelogic::isNear2D(
        *point, center, *reinterpret_cast<const float*>(0x00BD9514)) ? 1 : 0;
    if (state.mode == Mode::replace) {
        state.matches.fetch_add(1, std::memory_order_relaxed);
        return ours;
    }
    const int game = reinterpret_cast<int(__thiscall*)(
        const void*, const Coord2D*)>(state.original)(object, point);
    recordScalar(key, state, game, ours);
    return game;
}

int __fastcall guardPointNear(
    const void* object, void*, const Coord2D* point)
{
    return shadowPointNear(
        "skirmishguardpointnear", g_guardPointNear, object, point, 0x30);
}

int __fastcall guardReturnNear(
    const void* object, void*, const Coord2D* point)
{
    return shadowPointNear(
        "skirmishguardreturnnear", g_guardReturnNear, object, point, 0x2C);
}

}  // namespace

void appendSkirmishTargets(std::vector<Target>& table)
{
    table.insert(table.end(), {
        {"skirmishguardpayload", 0x0039BCE0,
         {0x8B, 0x81, 0x90, 0x00, 0x00, 0x00, 0x85, 0xC0,
          0x74, 0x0A, 0x83, 0x78, 0x70, 0x01, 0x75, 0x04},
         6, reinterpret_cast<void*>(&guardPayload), &g_guardPayload, true},
        {"skirmishguardpair", 0x0039BED0,
         {0x8B, 0x4C, 0x24, 0x04, 0x8B, 0x44, 0x24, 0x0C,
          0x3B, 0xC8, 0x8B, 0x54, 0x24, 0x08, 0x56, 0x8B},
         8, reinterpret_cast<void*>(&guardEndpointPair), &g_guardEndpointPair, true},
        {"skirmishguardobjectcooldown", 0x0039C020,
         {0x8B, 0x44, 0x24, 0x04, 0x8B, 0x90, 0xDC, 0x00,
          0x00, 0x00, 0x8B, 0x44, 0x24, 0x08, 0x3B, 0x54},
         10, reinterpret_cast<void*>(&guardObjectCooldown),
         &g_guardObjectCooldown, true},
        {"skirmishguardtargetcooldown", 0x0039C080,
         {0x8B, 0x44, 0x24, 0x04, 0x8B, 0x90, 0xDC, 0x00,
          0x00, 0x00, 0x8B, 0x44, 0x24, 0x08, 0x3B, 0x54},
         10, reinterpret_cast<void*>(&guardTargetCooldown),
         &g_guardTargetCooldown, true},
        {"skirmishguardtrackedtarget", 0x0039C100,
         {0x8B, 0x44, 0x24, 0x04, 0x85, 0xC0, 0x89, 0x81,
          0x84, 0x00, 0x00, 0x00, 0xC7, 0x81, 0x88, 0x00},
         6, reinterpret_cast<void*>(&guardTrackedTarget),
         &g_guardTrackedTarget, true},
        {"skirmishguardcandidatenear", 0x0039C2A0,
         {0x8B, 0x44, 0x24, 0x04, 0x85, 0xC0, 0x74, 0x48,
          0xF6, 0x80, 0x71, 0x04, 0x00, 0x00, 0x01, 0x75},
         6, reinterpret_cast<void*>(&guardCandidateNear),
         &g_guardCandidateNear, true},
        {"skirmishguardpointnear", 0x0039C340,
         {0x8B, 0x44, 0x24, 0x04, 0xF3, 0x0F, 0x10, 0x00,
          0xF3, 0x0F, 0x5C, 0x41, 0x30, 0xF3, 0x0F, 0x10},
         8, reinterpret_cast<void*>(&guardPointNear), &g_guardPointNear, true},
        {"skirmishguardreturnnear", 0x0039C3D0,
         {0x8B, 0x44, 0x24, 0x04, 0xF3, 0x0F, 0x10, 0x00,
          0xF3, 0x0F, 0x5C, 0x41, 0x2C, 0xF3, 0x0F, 0x10},
         8, reinterpret_cast<void*>(&guardReturnNear), &g_guardReturnNear, true},
    });
}

}  // namespace ra3hook
