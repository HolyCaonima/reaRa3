#include "../Targets.h"

#include <cstddef>
#include <cstdint>

namespace ra3hook {
namespace {

TargetState g_weaponAtSlot;
TargetState g_stateDeadline;
TargetState g_coolDownUpdate;

std::uint32_t rawDword(const void* object, std::size_t offset)
{
    return *reinterpret_cast<const std::uint32_t*>(
        static_cast<const std::uint8_t*>(object) + offset);
}

const std::uint8_t* rawPointer(const void* object, std::size_t offset)
{
    return *reinterpret_cast<const std::uint8_t* const*>(
        static_cast<const std::uint8_t*>(object) + offset);
}

using ScalarMethod = std::uint32_t (*)(const void*);

std::uint32_t shadowScalar(
    const char* key, TargetState& state, const void* object,
    ScalarMethod implementation)
{
    state.calls.fetch_add(1, std::memory_order_relaxed);
    const std::uint32_t ours = implementation(object);
    if (state.mode == Mode::replace) {
        state.matches.fetch_add(1, std::memory_order_relaxed);
        return ours;
    }
    const std::uint32_t game =
        reinterpret_cast<std::uint32_t(__thiscall*)(const void*)>(
            state.original)(object);
    recordScalar(key, state, game, ours);
    return game;
}

std::uint32_t stateDeadlineImpl(const void* object)
{
    return rawDword(object, 0x24) + rawDword(object, 0x28);
}

std::uint32_t __fastcall stateDeadline(const void* object, void*)
{
    return shadowScalar(
        "objectlogicstatedeadline", g_stateDeadline, object,
        &stateDeadlineImpl);
}

std::uint32_t coolDownUpdateImpl(const void* object)
{
    const auto* weapon = rawPointer(object, 0x20);
    return rawDword(weapon, 0x1C) > 0 ? 0xFFFFFFFEU : 0U;
}

std::uint32_t __fastcall coolDownUpdate(const void* object, void*)
{
    return shadowScalar(
        "objectlogiccooldownupdate", g_coolDownUpdate, object,
        &coolDownUpdateImpl);
}

std::uint32_t __fastcall weaponAtSlot(
    const void* object, void*, std::uint32_t slot)
{
    TargetState& state = g_weaponAtSlot;
    state.calls.fetch_add(1, std::memory_order_relaxed);
    const auto* slots = rawPointer(object, 0x94);
    const std::uint32_t ours = rawDword(slots, slot * 4U);
    if (state.mode == Mode::replace) {
        state.matches.fetch_add(1, std::memory_order_relaxed);
        return ours;
    }
    const std::uint32_t game =
        reinterpret_cast<std::uint32_t(__thiscall*)(
            const void*, std::uint32_t)>(state.original)(object, slot);
    recordScalar("objectlogicweaponatslot", state, game, ours);
    return game;
}

}  // namespace

void appendObjectLogicTargets(std::vector<Target>& table)
{
    table.insert(table.end(), {
        {"objectlogicweaponatslot", 0x002A2430,
         {0x8B, 0x81, 0x94, 0x00, 0x00, 0x00, 0x8B, 0x4C,
          0x24, 0x04, 0x8B, 0x04, 0x88, 0xC2, 0x04, 0x00},
         6, reinterpret_cast<void*>(&weaponAtSlot), &g_weaponAtSlot, true},
        {"objectlogicstatedeadline", 0x002A2840,
         {0x8B, 0x41, 0x28, 0x03, 0x41, 0x24, 0xC3, 0xCC,
          0xCC, 0xCC, 0xCC, 0xCC, 0xCC, 0xCC, 0xCC, 0xCC},
         6, reinterpret_cast<void*>(&stateDeadline), &g_stateDeadline, true},
        {"objectlogiccooldownupdate", 0x002BD310,
         {0x8B, 0x41, 0x20, 0x33, 0xC9, 0x3B, 0x48, 0x1C,
          0x1B, 0xC0, 0x83, 0xE0, 0xFE, 0xC3, 0xCC, 0xCC},
         5, reinterpret_cast<void*>(&coolDownUpdate), &g_coolDownUpdate, true},
    });
}

}  // namespace ra3hook
