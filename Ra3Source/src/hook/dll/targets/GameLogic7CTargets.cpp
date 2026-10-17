#include "../Targets.h"

#include <ra3/gamelogic/AIStateMachine7C.h>

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>

namespace ra3hook {
namespace {

using ra3::gamelogic::AIStateMachine7CMemoryView;
using ra3::gamelogic::GameDispatch;
using ra3::gamelogic::noOpDispatch;

constexpr auto kRelaxed = std::memory_order_relaxed;
constexpr std::size_t kMoveStateShadowSize = 0x60;

TargetState g_moveOutOfTheWayOnEnter;
TargetState g_moveAndTightenOnEnter;
TargetState g_moveAwayFromRepulsorsOnEnter;
TargetState g_attackApproachTargetOnEnter;
TargetState g_moveToTargetObjectOnEnter;
TargetState g_moveAndEvacuateOnEnter;
TargetState g_moveWithStatusFlagsOnEnter;
TargetState g_moveValidatedOnEnter;

AIStateMachine7CMemoryView liveMemoryView() noexcept
{
    AIStateMachine7CMemoryView view{};
    view.theGameLogic = *reinterpret_cast<std::uint32_t*>(0x00DEBA64);
    view.theWeaponStore = *reinterpret_cast<std::uint32_t*>(0x00DEBA88);
    view.thePartitionMgr = *reinterpret_cast<std::uint32_t*>(0x00DFB9D8);
    view.halfScale = *reinterpret_cast<const float*>(0x00BD9528);
    view.minMoveDistance = *reinterpret_cast<const float*>(0x00BD9520);
    view.defaultGoalRadius = *reinterpret_cast<const float*>(0x00BE700C);
    return view;
}

void* liveGetGoalObject_005B9F30(void* machinePtr, AIStateMachine7CMemoryView&)
{
    using Fn = void*(__thiscall*)(void*);
    return reinterpret_cast<Fn>(0x005B9F30)(machinePtr);
}

std::uint32_t liveGetAIUpdateTargetId_00683630(void* aiUpdatePtr, AIStateMachine7CMemoryView&)
{
    using Fn = std::uint32_t(__thiscall*)(void*);
    return reinterpret_cast<Fn>(0x00683630)(aiUpdatePtr);
}

std::uint32_t liveStartMove_007C08A0(void* statePtr, AIStateMachine7CMemoryView&)
{
    using Fn = std::uint32_t(__thiscall*)(void*);
    return reinterpret_cast<Fn>(0x007C08A0)(statePtr);
}

bool liveCheckMoveOut_006FFB00(void* aiUpdatePtr, std::int32_t arg, AIStateMachine7CMemoryView&)
{
    using Fn = bool(__thiscall*)(void*, std::int32_t);
    return reinterpret_cast<Fn>(0x006FFB00)(aiUpdatePtr, arg);
}

void liveSetStatusHelper_006A4F80(void* ownerPtr, std::int32_t bitIndex, std::int32_t val,
                                  AIStateMachine7CMemoryView&)
{
    using Fn = void(__thiscall*)(void*, std::int32_t, std::int32_t);
    reinterpret_cast<Fn>(0x006A4F80)(ownerPtr, bitIndex, val);
}

void livePostStatusHelper_006A4FB0(void* ownerPtr, AIStateMachine7CMemoryView&)
{
    using Fn = void(__thiscall*)(void*);
    reinterpret_cast<Fn>(0x006A4FB0)(ownerPtr);
}

void liveOwnerHelper_00686E00(void* ownerPtr, AIStateMachine7CMemoryView&)
{
    using Fn = void(__thiscall*)(void*);
    reinterpret_cast<Fn>(0x00686E00)(ownerPtr);
}

void liveRepulsorHelper_007526C0(void* aiUpdatePtr, void* ioPos, void* refPos,
                                 AIStateMachine7CMemoryView&)
{
    using Fn = void(__thiscall*)(void*, void*, void*);
    reinterpret_cast<Fn>(0x007526C0)(aiUpdatePtr, ioPos, refPos);
}

bool liveAiUpdateMode_007527C0(void* aiUpdatePtr, std::int32_t mode, AIStateMachine7CMemoryView&)
{
    using Fn = bool(__thiscall*)(void*, std::int32_t);
    return reinterpret_cast<Fn>(0x007527C0)(aiUpdatePtr, mode);
}

void liveOwnerAction_004D9AF0(void* ownerPtr, std::int32_t bit, AIStateMachine7CMemoryView&)
{
    using Fn = void(__thiscall*)(void*, std::int32_t);
    reinterpret_cast<Fn>(0x004D9AF0)(ownerPtr, bit);
}

void liveAiUpdateSetTargetId_00752720(void* aiUpdatePtr, std::uint32_t objId,
                                      AIStateMachine7CMemoryView&)
{
    using Fn = void(__thiscall*)(void*, std::uint32_t);
    reinterpret_cast<Fn>(0x00752720)(aiUpdatePtr, objId);
}

void liveLockMachine_005AA240(void* machinePtr, std::uint32_t tagAddr, AIStateMachine7CMemoryView&)
{
    using Fn = void(__thiscall*)(void*, std::uint32_t);
    reinterpret_cast<Fn>(0x005AA240)(machinePtr, tagAddr);
}

const GameDispatch& liveReplaceDispatch() noexcept
{
    static const GameDispatch kDispatch = [] {
        GameDispatch d = noOpDispatch();
        d.getGoalObject_005B9F30 = &liveGetGoalObject_005B9F30;
        d.getAIUpdateTargetId_00683630 = &liveGetAIUpdateTargetId_00683630;
        d.startMove_007C08A0 = &liveStartMove_007C08A0;
        d.checkMoveOut_006FFB00 = &liveCheckMoveOut_006FFB00;
        d.setStatusHelper_006A4F80 = &liveSetStatusHelper_006A4F80;
        d.postStatusHelper_006A4FB0 = &livePostStatusHelper_006A4FB0;
        d.ownerHelper_00686E00 = &liveOwnerHelper_00686E00;
        d.repulsorHelper_007526C0 = &liveRepulsorHelper_007526C0;
        d.aiUpdateMode_007527C0 = &liveAiUpdateMode_007527C0;
        d.ownerAction_004D9AF0 = &liveOwnerAction_004D9AF0;
        d.aiUpdateSetTargetId_00752720 = &liveAiUpdateSetTargetId_00752720;
        d.lockMachine_005AA240 = &liveLockMachine_005AA240;
        return d;
    }();
    return kDispatch;
}

const GameDispatch& liveShadowReadDispatch() noexcept
{
    static const GameDispatch kDispatch = [] {
        GameDispatch d = noOpDispatch();
        d.getGoalObject_005B9F30 = &liveGetGoalObject_005B9F30;
        d.getAIUpdateTargetId_00683630 = &liveGetAIUpdateTargetId_00683630;
        return d;
    }();
    return kDispatch;
}

void* resolveStateMachine(void* statePtr) noexcept
{
    if (statePtr == nullptr) {
        return nullptr;
    }
    std::uint32_t machineAddr = 0;
    std::memcpy(&machineAddr, static_cast<const std::uint8_t*>(statePtr) + 0x18, 4);
    return reinterpret_cast<void*>(static_cast<std::uintptr_t>(machineAddr));
}

void* resolveStateOwner(void* statePtr) noexcept
{
    void* const machine = resolveStateMachine(statePtr);
    if (machine == nullptr) {
        return nullptr;
    }
    std::uint32_t ownerAddr = 0;
    std::memcpy(&ownerAddr, static_cast<const std::uint8_t*>(machine) + 0x24, 4);
    return reinterpret_cast<void*>(static_cast<std::uintptr_t>(ownerAddr));
}

std::uint32_t __fastcall moveOutOfTheWayOnEnter_007C0BD0(void* statePtr, void*)
{
    constexpr const char* kKey =
        "gamelogic.aistatemachine7c.move_out_of_the_way_on_enter_007c0bd0";
    TargetState& state = g_moveOutOfTheWayOnEnter;
    state.calls.fetch_add(1, kRelaxed);

    AIStateMachine7CMemoryView view = liveMemoryView();
    if (state.mode == Mode::replace) {
        const std::uint32_t ours = ra3::gamelogic::moveOutOfTheWayStateOnEnter_007C0BD0(
            statePtr, view, liveReplaceDispatch());
        state.matches.fetch_add(1, kRelaxed);
        return ours;
    }

    std::array<std::uint8_t, kMoveStateShadowSize> copy{};
    std::memcpy(copy.data(), statePtr, copy.size());
    copy[0x54] = 0;  // suppress post-startMove checkMoveOut side effect in shadow dry-run
    ra3::gamelogic::moveOutOfTheWayStateOnEnter_007C0BD0(
        copy.data(), view, liveShadowReadDispatch());

    const std::uint32_t game =
        reinterpret_cast<std::uint32_t(__thiscall*)(void*)>(state.original)(statePtr);
    const auto* const gameBytes = static_cast<const std::uint8_t*>(statePtr);
    recordOutput(kKey, state, gameBytes + 0x20, copy.data() + 0x20, 12);
    recordOutput(kKey, state, gameBytes + 0x4C, copy.data() + 0x4C, 1);
    return game;
}

std::uint32_t __fastcall moveAndTightenOnEnter_007C0C80(void* statePtr, void*)
{
    constexpr const char* kKey =
        "gamelogic.aistatemachine7c.move_and_tighten_on_enter_007c0c80";
    TargetState& state = g_moveAndTightenOnEnter;
    state.calls.fetch_add(1, kRelaxed);

    AIStateMachine7CMemoryView view = liveMemoryView();
    if (state.mode == Mode::replace) {
        const std::uint32_t ours = ra3::gamelogic::moveAndTightenStateOnEnter_007C0C80(
            statePtr, view, liveReplaceDispatch());
        state.matches.fetch_add(1, kRelaxed);
        return ours;
    }

    std::array<std::uint8_t, kMoveStateShadowSize> copy{};
    std::memcpy(copy.data(), statePtr, copy.size());
    void* const owner = resolveStateOwner(statePtr);
    std::uint32_t savedOwnerFlags = 0;
    std::uint32_t targetSubObj = 0;
    if (owner != nullptr) {
        std::memcpy(&savedOwnerFlags, static_cast<const std::uint8_t*>(owner) + 0xB0, 4);
        std::uint32_t aiUpdateAddr = 0;
        std::memcpy(&aiUpdateAddr, static_cast<const std::uint8_t*>(owner) + 0x374, 4);
        if (aiUpdateAddr != 0) {
            std::memcpy(
                &targetSubObj,
                reinterpret_cast<const std::uint8_t*>(static_cast<std::uintptr_t>(aiUpdateAddr))
                    + 0x150,
                4);
        }
    }

    const std::uint32_t ours = ra3::gamelogic::moveAndTightenStateOnEnter_007C0C80(
        copy.data(), view, liveShadowReadDispatch());
    if (owner != nullptr) {
        std::memcpy(static_cast<std::uint8_t*>(owner) + 0xB0, &savedOwnerFlags, 4);
    }

    const std::uint32_t game =
        reinterpret_cast<std::uint32_t(__thiscall*)(void*)>(state.original)(statePtr);
    const auto* const gameBytes = static_cast<const std::uint8_t*>(statePtr);
    recordOutput(kKey, state, gameBytes + 0x4C, copy.data() + 0x4C, 1);
    if (targetSubObj != 0) {
        recordOutput(kKey, state, gameBytes + 0x20, copy.data() + 0x20, 12);
    } else {
        recordScalar(kKey, state, game, ours);
    }
    return game;
}

std::uint32_t __fastcall moveAwayFromRepulsorsOnEnter_007C0D00(void* statePtr, void*)
{
    constexpr const char* kKey =
        "gamelogic.aistatemachine7c.move_away_from_repulsors_on_enter_007c0d00";
    TargetState& state = g_moveAwayFromRepulsorsOnEnter;
    state.calls.fetch_add(1, kRelaxed);

    AIStateMachine7CMemoryView view = liveMemoryView();
    if (state.mode == Mode::replace) {
        const std::uint32_t ours = ra3::gamelogic::moveAwayFromRepulsorsStateOnEnter_007C0D00(
            statePtr, view, liveReplaceDispatch());
        state.matches.fetch_add(1, kRelaxed);
        return ours;
    }

    std::array<std::uint8_t, kMoveStateShadowSize> copy{};
    std::memcpy(copy.data(), statePtr, copy.size());
    ra3::gamelogic::moveAwayFromRepulsorsStateOnEnter_007C0D00(
        copy.data(), view, liveShadowReadDispatch());

    const std::uint32_t game =
        reinterpret_cast<std::uint32_t(__thiscall*)(void*)>(state.original)(statePtr);
    const auto* const gameBytes = static_cast<const std::uint8_t*>(statePtr);
    recordOutput(kKey, state, gameBytes + 0x4C, copy.data() + 0x4C, 1);
    recordOutput(kKey, state, gameBytes + 0x50, copy.data() + 0x50, 4);
    recordOutput(kKey, state, gameBytes + 0x54, copy.data() + 0x54, 1);
    return game;
}

std::uint32_t __fastcall attackApproachTargetOnEnter_007C0D70(void* statePtr, void*)
{
    constexpr const char* kKey =
        "gamelogic.aistatemachine7c.attack_approach_target_on_enter_007c0d70";
    TargetState& state = g_attackApproachTargetOnEnter;
    state.calls.fetch_add(1, kRelaxed);

    AIStateMachine7CMemoryView view = liveMemoryView();
    if (state.mode == Mode::replace) {
        const std::uint32_t ours = ra3::gamelogic::attackApproachTargetStateOnEnter_007C0D70(
            statePtr, view, liveReplaceDispatch());
        state.matches.fetch_add(1, kRelaxed);
        return ours;
    }

    std::array<std::uint8_t, kMoveStateShadowSize> copy{};
    std::memcpy(copy.data(), statePtr, copy.size());
    const std::uint32_t ours = ra3::gamelogic::attackApproachTargetStateOnEnter_007C0D70(
        copy.data(), view, liveShadowReadDispatch());

    const std::uint32_t game =
        reinterpret_cast<std::uint32_t(__thiscall*)(void*)>(state.original)(statePtr);
    const auto* const gameBytes = static_cast<const std::uint8_t*>(statePtr);
    recordOutput(kKey, state, gameBytes + 0x4C, copy.data() + 0x4C, 1);
    if (ours != 0xFFFFFFFEu) {
        recordOutput(kKey, state, gameBytes + 0x50, copy.data() + 0x50, 4);
        recordOutput(kKey, state, gameBytes + 0x54, copy.data() + 0x54, 1);
    } else {
        recordScalar(kKey, state, game, ours);
    }
    return game;
}

std::uint32_t __fastcall moveToTargetObjectOnEnter_007C0EE0(void* statePtr, void*)
{
    constexpr const char* kKey =
        "gamelogic.aistatemachine7c.move_to_target_object_on_enter_007c0ee0";
    TargetState& state = g_moveToTargetObjectOnEnter;
    state.calls.fetch_add(1, kRelaxed);

    AIStateMachine7CMemoryView view = liveMemoryView();
    if (state.mode == Mode::replace) {
        const std::uint32_t ours = ra3::gamelogic::moveToTargetObjectStateOnEnter_007C0EE0(
            statePtr, view, liveReplaceDispatch());
        state.matches.fetch_add(1, kRelaxed);
        return ours;
    }

    std::array<std::uint8_t, kMoveStateShadowSize> copy{};
    std::memcpy(copy.data(), statePtr, copy.size());
    const std::uint32_t ours = ra3::gamelogic::moveToTargetObjectStateOnEnter_007C0EE0(
        copy.data(), view, liveShadowReadDispatch());

    const std::uint32_t game =
        reinterpret_cast<std::uint32_t(__thiscall*)(void*)>(state.original)(statePtr);
    const auto* const gameBytes = static_cast<const std::uint8_t*>(statePtr);
    if (ours != 0xFFFFFFFEu) {
        recordOutput(kKey, state, gameBytes + 0x50, copy.data() + 0x50, 4);
        recordOutput(kKey, state, gameBytes + 0x54, copy.data() + 0x54, 1);
    } else {
        recordScalar(kKey, state, game, ours);
    }
    return game;
}

std::uint32_t __fastcall moveAndEvacuateOnEnter_007C3520(void* statePtr, void*)
{
    constexpr const char* kKey =
        "gamelogic.aistatemachine7c.move_and_evacuate_on_enter_007c3520";
    TargetState& state = g_moveAndEvacuateOnEnter;
    state.calls.fetch_add(1, kRelaxed);

    AIStateMachine7CMemoryView view = liveMemoryView();
    if (state.mode == Mode::replace) {
        const std::uint32_t ours = ra3::gamelogic::moveAndEvacuateStateOnEnter_007C3520(
            statePtr, view, liveReplaceDispatch());
        state.matches.fetch_add(1, kRelaxed);
        return ours;
    }

    std::array<std::uint8_t, kMoveStateShadowSize> copy{};
    std::memcpy(copy.data(), statePtr, copy.size());
    ra3::gamelogic::moveAndEvacuateStateOnEnter_007C3520(
        copy.data(), view, liveShadowReadDispatch());

    const std::uint32_t game =
        reinterpret_cast<std::uint32_t(__thiscall*)(void*)>(state.original)(statePtr);
    const auto* const gameBytes = static_cast<const std::uint8_t*>(statePtr);
    recordOutput(kKey, state, gameBytes + 0x20, copy.data() + 0x20, 12);
    recordOutput(kKey, state, gameBytes + 0x4C, copy.data() + 0x4C, 1);
    recordOutput(kKey, state, gameBytes + 0x50, copy.data() + 0x50, 12);
    return game;
}

std::uint32_t __fastcall moveWithStatusFlagsOnEnter_007C3680(void* statePtr, void*)
{
    constexpr const char* kKey =
        "gamelogic.aistatemachine7c.move_with_status_flags_on_enter_007c3680";
    TargetState& state = g_moveWithStatusFlagsOnEnter;
    state.calls.fetch_add(1, kRelaxed);

    AIStateMachine7CMemoryView view = liveMemoryView();
    if (state.mode == Mode::replace) {
        const std::uint32_t ours = ra3::gamelogic::moveWithStatusFlagsStateOnEnter_007C3680(
            statePtr, view, liveReplaceDispatch());
        state.matches.fetch_add(1, kRelaxed);
        return ours;
    }

    std::array<std::uint8_t, kMoveStateShadowSize> copy{};
    std::memcpy(copy.data(), statePtr, copy.size());
    void* const machine = resolveStateMachine(statePtr);
    void* const owner = resolveStateOwner(statePtr);
    std::uint8_t savedLock = 0;
    std::uint32_t savedOwnerFlags = 0;
    if (machine != nullptr) {
        savedLock = *(static_cast<const std::uint8_t*>(machine) + 0x78);
    }
    if (owner != nullptr) {
        std::memcpy(&savedOwnerFlags, static_cast<const std::uint8_t*>(owner) + 0xB0, 4);
    }
    ra3::gamelogic::moveWithStatusFlagsStateOnEnter_007C3680(
        copy.data(), view, liveShadowReadDispatch());
    if (machine != nullptr) {
        *(static_cast<std::uint8_t*>(machine) + 0x78) = savedLock;
    }
    if (owner != nullptr) {
        std::memcpy(static_cast<std::uint8_t*>(owner) + 0xB0, &savedOwnerFlags, 4);
    }

    const std::uint32_t game =
        reinterpret_cast<std::uint32_t(__thiscall*)(void*)>(state.original)(statePtr);
    const auto* const gameBytes = static_cast<const std::uint8_t*>(statePtr);
    recordOutput(kKey, state, gameBytes + 0x20, copy.data() + 0x20, 12);
    recordOutput(kKey, state, gameBytes + 0x4C, copy.data() + 0x4C, 1);
    recordOutput(kKey, state, gameBytes + 0x50, copy.data() + 0x50, 1);
    return game;
}

std::uint32_t __fastcall moveValidatedOnEnter_007C4BE0(void* statePtr, void*)
{
    constexpr const char* kKey =
        "gamelogic.aistatemachine7c.move_validated_on_enter_007c4be0";
    TargetState& state = g_moveValidatedOnEnter;
    state.calls.fetch_add(1, kRelaxed);

    AIStateMachine7CMemoryView view = liveMemoryView();
    if (state.mode == Mode::replace) {
        const std::uint32_t ours = ra3::gamelogic::moveValidatedStateOnEnter_007C4BE0(
            statePtr, view, liveReplaceDispatch());
        state.matches.fetch_add(1, kRelaxed);
        return ours;
    }

    std::array<std::uint8_t, kMoveStateShadowSize> copy{};
    std::memcpy(copy.data(), statePtr, copy.size());
    void* const machine = resolveStateMachine(statePtr);
    std::uint8_t savedLock = 0;
    if (machine != nullptr) {
        savedLock = *(static_cast<const std::uint8_t*>(machine) + 0x78);
    }
    ra3::gamelogic::moveValidatedStateOnEnter_007C4BE0(
        copy.data(), view, liveShadowReadDispatch());
    if (machine != nullptr) {
        *(static_cast<std::uint8_t*>(machine) + 0x78) = savedLock;
    }

    const std::uint32_t game =
        reinterpret_cast<std::uint32_t(__thiscall*)(void*)>(state.original)(statePtr);
    const auto* const gameBytes = static_cast<const std::uint8_t*>(statePtr);
    recordOutput(kKey, state, gameBytes + 0x20, copy.data() + 0x20, 12);
    recordOutput(kKey, state, gameBytes + 0x4C, copy.data() + 0x4C, 1);
    recordOutput(kKey, state, gameBytes + 0x50, copy.data() + 0x50, 1);
    return game;
}

}  // namespace

void appendGameLogic7CTargets(std::vector<Target>& table)
{
    table.insert(table.end(), {
        {"gamelogic.aistatemachine7c.move_out_of_the_way_on_enter_007c0bd0", 0x003C0BD0,
         {0x53, 0x56, 0x8B, 0xF1, 0x8B, 0x4E, 0x18, 0xC6,
          0x46, 0x4C, 0x01, 0x8B, 0x41, 0x24, 0x57, 0x8B},
         7, reinterpret_cast<void*>(&moveOutOfTheWayOnEnter_007C0BD0),
         &g_moveOutOfTheWayOnEnter, true},
        {"gamelogic.aistatemachine7c.move_and_tighten_on_enter_007c0c80", 0x003C0C80,
         {0x53, 0x56, 0x57, 0x8B, 0xF9, 0x8B, 0x47, 0x18,
          0xC6, 0x47, 0x4C, 0x01, 0x8B, 0x70, 0x24, 0x8B},
         5, reinterpret_cast<void*>(&moveAndTightenOnEnter_007C0C80),
         &g_moveAndTightenOnEnter, true},
        {"gamelogic.aistatemachine7c.move_away_from_repulsors_on_enter_007c0d00", 0x003C0D00,
         {0x56, 0x8B, 0xF1, 0x8B, 0x46, 0x18, 0xC6, 0x46,
          0x4C, 0x00, 0x8B, 0x48, 0x24, 0xB8, 0x01, 0x00},
         6, reinterpret_cast<void*>(&moveAwayFromRepulsorsOnEnter_007C0D00),
         &g_moveAwayFromRepulsorsOnEnter, true},
        {"gamelogic.aistatemachine7c.attack_approach_target_on_enter_007c0d70", 0x003C0D70,
         {0x53, 0x55, 0x56, 0x8B, 0xF1, 0x8B, 0x4E, 0x18,
          0x57, 0xC6, 0x46, 0x4C, 0x00, 0x8B, 0x79, 0x24},
         5, reinterpret_cast<void*>(&attackApproachTargetOnEnter_007C0D70),
         &g_attackApproachTargetOnEnter, true},
        {"gamelogic.aistatemachine7c.move_to_target_object_on_enter_007c0ee0", 0x003C0EE0,
         {0x53, 0x56, 0x8B, 0xF1, 0x8B, 0x4E, 0x18, 0x57,
          0xE8, 0x43, 0x90, 0xDF, 0xFF, 0x8B, 0xD8, 0x85},
         7, reinterpret_cast<void*>(&moveToTargetObjectOnEnter_007C0EE0),
         &g_moveToTargetObjectOnEnter, true},
        {"gamelogic.aistatemachine7c.move_and_evacuate_on_enter_007c3520", 0x003C3520,
         {0x56, 0x8B, 0xF1, 0x8B, 0x4E, 0x18, 0x8B, 0x41,
          0x24, 0x8B, 0x50, 0x38, 0x83, 0xC0, 0x38, 0x89},
         6, reinterpret_cast<void*>(&moveAndEvacuateOnEnter_007C3520),
         &g_moveAndEvacuateOnEnter, true},
        {"gamelogic.aistatemachine7c.move_with_status_flags_on_enter_007c3680", 0x003C3680,
         {0x56, 0x8B, 0xF1, 0x8B, 0x4E, 0x18, 0x57, 0x68,
          0xD8, 0x38, 0xC3, 0x00, 0xC6, 0x46, 0x4C, 0x00},
         6, reinterpret_cast<void*>(&moveWithStatusFlagsOnEnter_007C3680),
         &g_moveWithStatusFlagsOnEnter, true},
        {"gamelogic.aistatemachine7c.move_validated_on_enter_007c4be0", 0x003C4BE0,
         {0x56, 0x8B, 0xF1, 0x8B, 0x4E, 0x18, 0x68, 0xD8,
          0x38, 0xC3, 0x00, 0xC6, 0x46, 0x4C, 0x00, 0xE8},
         6, reinterpret_cast<void*>(&moveValidatedOnEnter_007C4BE0),
         &g_moveValidatedOnEnter, true},
    });
}

}  // namespace ra3hook
