#include "../Targets.h"

#include <ra3/gamelogic/WeaponLogic.h>

#include <cstddef>
#include <cstdint>

namespace ra3hook {
namespace {

TargetState g_stateDeadlineUpdate;
TargetState g_preAttackLowFrames;
TargetState g_preAttackHighFrames;
TargetState g_firingLowFrames;
TargetState g_firingHighFrames;
TargetState g_coolDownLowFrames;
TargetState g_coolDownHighFrames;
TargetState g_reloadLowFrames;
TargetState g_reloadHighFrames;
TargetState g_idleAfterFiringFrames;
TargetState g_holdAfterFiringFrames;
TargetState g_suspendFxFrames;
TargetState g_weaponAtSlot;
TargetState g_ownerWeaponSetToken;
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

int durationFrames(
    const void* object, std::size_t rangeOffset, bool high,
    bool allowMissingLogic)
{
    const auto* data = rawPointer(object, 4);
    const auto* range =
        *reinterpret_cast<const ra3::gamelogic::DurationRange* const*>(
            data + rangeOffset);
    if (range == nullptr) {
        return 0;
    }
    const auto* logic =
        *reinterpret_cast<const std::uint8_t* const*>(0x00DEBA64);
    if (logic == nullptr && !allowMissingLogic) {
        return 0;
    }
    const float logicRate =
        logic == nullptr ? 1.0F
                         : *reinterpret_cast<const float*>(logic + 0x1C4);
    const float timeScale = *reinterpret_cast<const float*>(0x00DEEEE8);
    return ra3::gamelogic::rangeEndpointToFrames(
        range, high, logicRate, timeScale);
}

int scalarFrames(const void* object, std::size_t secondsOffset)
{
    const auto* logic =
        *reinterpret_cast<const std::uint8_t* const*>(0x00DEBA64);
    if (logic == nullptr) {
        return 0;
    }
    const auto* data = rawPointer(object, 4);
    return ra3::gamelogic::secondsToFrames(
        *reinterpret_cast<const float*>(data + secondsOffset),
        *reinterpret_cast<const float*>(logic + 0x1C4),
        *reinterpret_cast<const float*>(0x00DEEEE8));
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

std::uint32_t deadlineUpdateImpl(const void* object)
{
    const std::uint32_t deadline =
        rawDword(object, 0x24) + rawDword(object, 0x28);
    const auto* logic =
        *reinterpret_cast<const std::uint8_t* const*>(0x00DEBA64);
    return rawDword(logic, 0x58) < deadline ? 0xFFFFFFFFU : 0U;
}

#define RA3_DURATION_IMPL(name, offset, high, missing) \
    std::uint32_t name##Impl(const void* object) \
    { \
        return static_cast<std::uint32_t>( \
            durationFrames(object, offset, high, missing)); \
    } \
    std::uint32_t __fastcall name(const void* object, void*) \
    { \
        return shadowScalar( \
            "objectlogic" #name, g_##name, object, &name##Impl); \
    }

RA3_DURATION_IMPL(preAttackLowFrames, 0xEC, false, false)
RA3_DURATION_IMPL(preAttackHighFrames, 0xEC, true, false)
RA3_DURATION_IMPL(firingLowFrames, 0xF0, false, false)
RA3_DURATION_IMPL(firingHighFrames, 0xF0, true, true)
RA3_DURATION_IMPL(coolDownLowFrames, 0xF4, false, false)
RA3_DURATION_IMPL(coolDownHighFrames, 0xF4, true, false)
RA3_DURATION_IMPL(reloadLowFrames, 0xF8, false, false)
RA3_DURATION_IMPL(reloadHighFrames, 0xF8, true, false)

#undef RA3_DURATION_IMPL

#define RA3_SCALAR_FRAMES_IMPL(name, offset) \
    std::uint32_t name##Impl(const void* object) \
    { \
        return static_cast<std::uint32_t>(scalarFrames(object, offset)); \
    } \
    std::uint32_t __fastcall name(const void* object, void*) \
    { \
        return shadowScalar( \
            "objectlogic" #name, g_##name, object, &name##Impl); \
    }

RA3_SCALAR_FRAMES_IMPL(idleAfterFiringFrames, 0x7C)
RA3_SCALAR_FRAMES_IMPL(holdAfterFiringFrames, 0x80)
RA3_SCALAR_FRAMES_IMPL(suspendFxFrames, 0x94)

#undef RA3_SCALAR_FRAMES_IMPL

std::uint32_t __fastcall stateDeadlineUpdate(const void* object, void*)
{
    return shadowScalar(
        "objectlogicstatedeadlineupdate", g_stateDeadlineUpdate, object,
        &deadlineUpdateImpl);
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

std::uint32_t ownerWeaponSetTokenImpl(const void* object)
{
    const auto* owner = rawPointer(object, 0x10);
    if (owner == nullptr) {
        return 0;
    }
    const auto* set = rawPointer(owner, 0x374);
    return set == nullptr ? 0U : rawDword(set, 0x47C);
}

std::uint32_t __fastcall ownerWeaponSetToken(const void* object, void*)
{
    return shadowScalar(
        "objectlogicownerweaponsettoken", g_ownerWeaponSetToken, object,
        &ownerWeaponSetTokenImpl);
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
        {"objectlogicstatedeadlineupdate", 0x00285DA0,
         {0x8B, 0x41, 0x28, 0x03, 0x41, 0x24, 0x8B, 0x0D,
          0x64, 0xBA, 0xDE, 0x00, 0x39, 0x41, 0x58, 0x1B},
         6, reinterpret_cast<void*>(&stateDeadlineUpdate),
         &g_stateDeadlineUpdate, true},
        {"objectlogicpreattacklowframes", 0x00285EF0,
         {0x8B, 0x15, 0x64, 0xBA, 0xDE, 0x00, 0x83, 0xEC,
          0x0C, 0x85, 0xD2, 0x74, 0x60, 0x8B, 0x41, 0x04},
         6, reinterpret_cast<void*>(&preAttackLowFrames),
         &g_preAttackLowFrames, true},
        {"objectlogicpreattackhighframes", 0x00285F70,
         {0x8B, 0x15, 0x64, 0xBA, 0xDE, 0x00, 0x83, 0xEC,
          0x0C, 0x85, 0xD2, 0x74, 0x61, 0x8B, 0x41, 0x04},
         6, reinterpret_cast<void*>(&preAttackHighFrames),
         &g_preAttackHighFrames, true},
        {"objectlogicfiringlowframes", 0x00285FF0,
         {0x8B, 0x15, 0x64, 0xBA, 0xDE, 0x00, 0x83, 0xEC,
          0x0C, 0x85, 0xD2, 0x74, 0x60, 0x8B, 0x41, 0x04},
         6, reinterpret_cast<void*>(&firingLowFrames),
         &g_firingLowFrames, true},
        {"objectlogicfiringhighframes", 0x00286070,
         {0x8B, 0x41, 0x04, 0x8B, 0x80, 0xF0, 0x00, 0x00,
          0x00, 0x83, 0xEC, 0x0C, 0x85, 0xC0, 0x0F, 0x84},
         9, reinterpret_cast<void*>(&firingHighFrames),
         &g_firingHighFrames, true},
        {"objectlogiccooldownlowframes", 0x00286110,
         {0x8B, 0x15, 0x64, 0xBA, 0xDE, 0x00, 0x83, 0xEC,
          0x0C, 0x85, 0xD2, 0x74, 0x60, 0x8B, 0x41, 0x04},
         6, reinterpret_cast<void*>(&coolDownLowFrames),
         &g_coolDownLowFrames, true},
        {"objectlogiccooldownhighframes", 0x00286190,
         {0x8B, 0x15, 0x64, 0xBA, 0xDE, 0x00, 0x83, 0xEC,
          0x0C, 0x85, 0xD2, 0x74, 0x61, 0x8B, 0x41, 0x04},
         6, reinterpret_cast<void*>(&coolDownHighFrames),
         &g_coolDownHighFrames, true},
        {"objectlogicreloadlowframes", 0x00286210,
         {0x8B, 0x15, 0x64, 0xBA, 0xDE, 0x00, 0x83, 0xEC,
          0x0C, 0x85, 0xD2, 0x74, 0x60, 0x8B, 0x41, 0x04},
         6, reinterpret_cast<void*>(&reloadLowFrames),
         &g_reloadLowFrames, true},
        {"objectlogicreloadhighframes", 0x00286290,
         {0x8B, 0x15, 0x64, 0xBA, 0xDE, 0x00, 0x83, 0xEC,
          0x0C, 0x85, 0xD2, 0x74, 0x61, 0x8B, 0x41, 0x04},
         6, reinterpret_cast<void*>(&reloadHighFrames),
         &g_reloadHighFrames, true},
        {"objectlogicidleafterfiringframes", 0x00286310,
         {0xA1, 0x64, 0xBA, 0xDE, 0x00, 0x83, 0xEC, 0x0C,
          0x85, 0xC0, 0x74, 0x57, 0x8B, 0x49, 0x04, 0xF3},
         8, reinterpret_cast<void*>(&idleAfterFiringFrames),
         &g_idleAfterFiringFrames, true},
        {"objectlogicholdafterfiringframes", 0x00286380,
         {0xA1, 0x64, 0xBA, 0xDE, 0x00, 0x83, 0xEC, 0x0C,
          0x85, 0xC0, 0x74, 0x5A, 0x8B, 0x49, 0x04, 0xF3},
         8, reinterpret_cast<void*>(&holdAfterFiringFrames),
         &g_holdAfterFiringFrames, true},
        {"objectlogicsuspendfxframes", 0x002863F0,
         {0xA1, 0x64, 0xBA, 0xDE, 0x00, 0x83, 0xEC, 0x0C,
          0x85, 0xC0, 0x74, 0x5A, 0x8B, 0x49, 0x04, 0xF3},
         8, reinterpret_cast<void*>(&suspendFxFrames),
         &g_suspendFxFrames, true},
        {"objectlogicweaponatslot", 0x002A2430,
         {0x8B, 0x81, 0x94, 0x00, 0x00, 0x00, 0x8B, 0x4C,
          0x24, 0x04, 0x8B, 0x04, 0x88, 0xC2, 0x04, 0x00},
         6, reinterpret_cast<void*>(&weaponAtSlot), &g_weaponAtSlot, true},
        {"objectlogicownerweaponsettoken", 0x002A2480,
         {0x8B, 0x41, 0x10, 0x85, 0xC0, 0x74, 0x11, 0x8B,
          0x80, 0x74, 0x03, 0x00, 0x00, 0x85, 0xC0, 0x74},
         5, reinterpret_cast<void*>(&ownerWeaponSetToken),
         &g_ownerWeaponSetToken, true},
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
