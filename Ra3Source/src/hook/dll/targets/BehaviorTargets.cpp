#include "../Targets.h"

#include <ra3/gamelogic/Behavior.h>

#include <cstdint>
#include <cstring>

namespace ra3hook {
namespace {

using ra3::gamelogic::PhysicsRuntime;

TargetState g_specialAbilityActivationState;
TargetState g_specialAbilityUnpackState;
TargetState g_invalidTargetsDeathFlags;
TargetState g_invalidTargetsDeathDefaultFlags;
TargetState g_linearDamageFlags;
TargetState g_physicsBehaviorFlags;
TargetState g_physicsRuntime;
TargetState g_hasMotiveTarget;

std::uint32_t shadowSpecialState(
    const char* key, TargetState& state, const std::uint32_t* module,
    std::size_t offset,
    std::uint32_t (*implementation)(std::uint32_t) noexcept)
{
    state.calls.fetch_add(1, std::memory_order_relaxed);
    const std::uint32_t flags =
        *reinterpret_cast<const std::uint32_t*>(
            reinterpret_cast<const std::uint8_t*>(module[1]) + offset);
    const std::uint32_t ours = implementation(flags);
    if (state.mode == Mode::replace) {
        state.matches.fetch_add(1, std::memory_order_relaxed);
        return ours;
    }
    const std::uint32_t game =
        reinterpret_cast<std::uint32_t(__fastcall*)(const std::uint32_t*)>(
            state.original)(module);
    recordScalar(key, state, game, ours);
    return game;
}

std::uint32_t __fastcall specialAbilityActivationState(
    const std::uint32_t* module, void*)
{
    return shadowSpecialState(
        "behaviorspecialactivationstate", g_specialAbilityActivationState,
        module, 0x28, &ra3::gamelogic::specialAbilityActivationState);
}

std::uint32_t __fastcall specialAbilityUnpackState(
    const std::uint32_t* module, void*)
{
    return shadowSpecialState(
        "behaviorspecialunpackstate", g_specialAbilityUnpackState,
        module, 0xF4, &ra3::gamelogic::specialAbilityUnpackState);
}

using FlagWriter = void (*)(std::uint32_t&) noexcept;

std::uint32_t* shadowFlagWriter(
    const char* key, TargetState& state, std::uint32_t* output,
    FlagWriter implementation)
{
    state.calls.fetch_add(1, std::memory_order_relaxed);
    std::uint32_t ours = *output;
    implementation(ours);
    if (state.mode == Mode::replace) {
        *output = ours;
        state.matches.fetch_add(1, std::memory_order_relaxed);
        return output;
    }
    std::uint32_t* const game =
        reinterpret_cast<std::uint32_t*(__stdcall*)(std::uint32_t*)>(
            state.original)(output);
    if (game != output) {
        reportMismatch(key, state, "EAX output pointer differs");
    } else {
        recordScalar(key, state, *output, ours);
    }
    return game;
}

#define RA3_BEHAVIOR_FLAG_WRAPPER(wrapper, keyText, stateName, implementation) \
    std::uint32_t* __stdcall wrapper(std::uint32_t* output) \
    { \
        return shadowFlagWriter(keyText, stateName, output, implementation); \
    }

RA3_BEHAVIOR_FLAG_WRAPPER(
    invalidTargetsDeathFlags, "behaviorinvalidtargetsflags",
    g_invalidTargetsDeathFlags, &ra3::gamelogic::invalidTargetsDeathFlags)
RA3_BEHAVIOR_FLAG_WRAPPER(
    invalidTargetsDeathDefaultFlags, "behaviorinvalidtargetsdefaultflags",
    g_invalidTargetsDeathDefaultFlags,
    &ra3::gamelogic::invalidTargetsDeathDefaultFlags)
RA3_BEHAVIOR_FLAG_WRAPPER(
    linearDamageFlags, "behaviorlineardamageflags",
    g_linearDamageFlags, &ra3::gamelogic::linearDamageFlags)
RA3_BEHAVIOR_FLAG_WRAPPER(
    physicsBehaviorFlags, "behaviorphysicsflags",
    g_physicsBehaviorFlags, &ra3::gamelogic::physicsBehaviorFlags)

#undef RA3_BEHAVIOR_FLAG_WRAPPER

PhysicsRuntime* __fastcall initializePhysicsRuntime(
    PhysicsRuntime* destination, void*,
    std::uint32_t owner, std::uint32_t moduleData)
{
    TargetState& state = g_physicsRuntime;
    state.calls.fetch_add(1, std::memory_order_relaxed);
    PhysicsRuntime ours = *destination;
    const std::uint32_t currentFrame =
        *reinterpret_cast<const std::uint32_t*>(0x00DF8AB8);
    ra3::gamelogic::initializePhysicsRuntime(
        ours, owner, moduleData, currentFrame);
    if (state.mode == Mode::replace) {
        *destination = ours;
        state.matches.fetch_add(1, std::memory_order_relaxed);
        return destination;
    }
    PhysicsRuntime* const game =
        reinterpret_cast<PhysicsRuntime*(__thiscall*)(
            PhysicsRuntime*, std::uint32_t, std::uint32_t)>(
                state.original)(destination, owner, moduleData);
    recordOutput(
        "behaviorphysicsruntime", state,
        destination, &ours, sizeof(ours));
    if (game != destination) {
        reportMismatch(
            "behaviorphysicsruntime", state, "EAX this pointer differs");
    }
    return game;
}

std::uint32_t __fastcall hasMotiveTarget(
    const std::uint8_t* physics, void*)
{
    TargetState& state = g_hasMotiveTarget;
    state.calls.fetch_add(1, std::memory_order_relaxed);
    const auto* target = *reinterpret_cast<const std::uint8_t* const*>(
        physics + 0x10);
    const std::uint32_t status =
        target == nullptr
        ? 0U
        : *reinterpret_cast<const std::uint32_t*>(target + 0xC0);
    const std::uint32_t ours =
        ra3::gamelogic::hasMotiveTarget(target != nullptr, status) ? 1U : 0U;
    if (state.mode == Mode::replace) {
        state.matches.fetch_add(1, std::memory_order_relaxed);
        return ours;
    }
    const std::uint32_t game =
        reinterpret_cast<std::uint32_t(__fastcall*)(const std::uint8_t*)>(
            state.original)(physics);
    recordScalar("behaviorhasmotivetarget", state, game, ours);
    return game;
}

}  // namespace

void appendBehaviorTargets(std::vector<Target>& table)
{
    table.insert(table.end(), {
        {"behaviorspecialactivationstate", 0x002B3980,
         {0x8B, 0x49, 0x04, 0x8B, 0x51, 0x28, 0xD1, 0xEA,
          0xF6, 0xC2, 0x01, 0xB8, 0x20, 0x1A, 0x0C, 0x00},
         6, reinterpret_cast<void*>(&specialAbilityActivationState),
         &g_specialAbilityActivationState, true},
        {"behaviorspecialunpackstate", 0x002B3DC0,
         {0x8B, 0x49, 0x04, 0x8B, 0x91, 0xF4, 0x00, 0x00,
          0x00, 0xD1, 0xEA, 0xF6, 0xC2, 0x01, 0xB8, 0x20},
         9, reinterpret_cast<void*>(&specialAbilityUnpackState),
         &g_specialAbilityUnpackState, true},
        {"behaviorinvalidtargetsflags", 0x002B7D70,
         {0x8B, 0x44, 0x24, 0x04, 0xC7, 0x00, 0x00, 0x00,
          0x00, 0x00, 0x83, 0x08, 0x42, 0xC2, 0x04, 0x00},
         10, reinterpret_cast<void*>(&invalidTargetsDeathFlags),
         &g_invalidTargetsDeathFlags, true},
        {"behaviorinvalidtargetsdefaultflags", 0x002B7D80,
         {0x8B, 0x44, 0x24, 0x04, 0xC7, 0x00, 0x00, 0x00,
          0x00, 0x00, 0x83, 0x08, 0x40, 0xC2, 0x04, 0x00},
         10, reinterpret_cast<void*>(&invalidTargetsDeathDefaultFlags),
         &g_invalidTargetsDeathDefaultFlags, true},
        {"behaviorlineardamageflags", 0x002B8480,
         {0x8B, 0x44, 0x24, 0x04, 0xC7, 0x00, 0x00, 0x00,
          0x00, 0x00, 0x83, 0x08, 0x1C, 0xC2, 0x04, 0x00},
         10, reinterpret_cast<void*>(&linearDamageFlags),
         &g_linearDamageFlags, true},
        {"behaviorphysicsflags", 0x002B8C70,
         {0x8B, 0x44, 0x24, 0x04, 0xC7, 0x00, 0x00, 0x00,
          0x00, 0x00, 0x81, 0x08, 0x90, 0x00, 0x00, 0x00},
         10, reinterpret_cast<void*>(&physicsBehaviorFlags),
         &g_physicsBehaviorFlags, true},
        {"behaviorphysicsruntime", 0x002B8EC0,
         {0x8B, 0xC1, 0x8B, 0x4C, 0x24, 0x08, 0x0F, 0x57,
          0xC0, 0x8B, 0x54, 0x24, 0x04, 0x89, 0x48, 0x04},
         6, reinterpret_cast<void*>(&initializePhysicsRuntime),
         &g_physicsRuntime, true},
        {"behaviorhasmotivetarget", 0x002B91E0,
         {0x8B, 0x41, 0x10, 0x85, 0xC0, 0x75, 0x03, 0x32,
          0xC0, 0xC3, 0x8B, 0x80, 0xC0, 0x00, 0x00, 0x00},
         5, reinterpret_cast<void*>(&hasMotiveTarget),
         &g_hasMotiveTarget, true},
    });
}

}  // namespace ra3hook
