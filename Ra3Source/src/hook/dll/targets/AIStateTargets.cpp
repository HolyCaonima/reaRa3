#include "../Targets.h"

#include <ra3/gamelogic/AIStateViews.h>

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>

namespace ra3hook {
namespace {

using ra3::gamelogic::DozerAIView;
using ra3::gamelogic::StateView;
using ra3::gamelogic::SupplyTruckAIView;
using ra3::gamelogic::UnitAIView;

constexpr auto relaxed = std::memory_order_relaxed;

TargetState g_primaryIdleEnter;
TargetState g_dozerHasTask;
TargetState g_dozerTaskTarget;
TargetState g_dozerDockPoint;
TargetState g_supplyFerrying;
TargetState g_unitSetValue144;
TargetState g_unitResetFlags;
TargetState g_unitSetGoal;
TargetState g_unitClearGoal;
TargetState g_unitClearIds;
TargetState g_unitValue250;
TargetState g_unitQueued;
TargetState g_unitCurrentId;
TargetState g_unitConditions;
TargetState g_unitTransitions;
TargetState g_unitDesiredSpeed;

std::uint32_t gameFrame() noexcept
{
    const auto* logic = *reinterpret_cast<const std::uint8_t**>(0x00DEBA64);
    return logic == nullptr ? 0U : *reinterpret_cast<const std::uint32_t*>(logic + 0x58);
}

template <typename Apply, typename Original>
void shadowWrite(
    const char* key, TargetState& state, void* object, std::size_t objectSize,
    std::size_t compareOffset, std::size_t compareSize, Apply apply,
    Original original)
{
    state.calls.fetch_add(1, relaxed);
    std::array<std::byte, UnitAIView::minimumRecoveredSize> copy{};
    std::memcpy(copy.data(), object, objectSize);
    apply(copy.data());
    if (state.mode == Mode::replace) {
        std::memcpy(
            static_cast<std::byte*>(object) + compareOffset,
            copy.data() + compareOffset, compareSize);
        return;
    }
    original(object);
    recordOutput(
        key, state, static_cast<const std::byte*>(object) + compareOffset,
        copy.data() + compareOffset, compareSize);
}

std::uint32_t __fastcall primaryIdleEnter(void* object, void*)
{
    auto& state = g_primaryIdleEnter;
    state.calls.fetch_add(1, relaxed);
    std::array<std::byte, StateView::minimumRecoveredSize> copy{};
    std::memcpy(copy.data(), object, copy.size());
    const auto ours = static_cast<std::uint32_t>(
        StateView(copy.data()).enterDozerPrimaryIdle(gameFrame()));
    if (state.mode == Mode::replace) {
        std::memcpy(static_cast<std::byte*>(object) + 0x20, copy.data() + 0x20, 4);
        std::memcpy(static_cast<std::byte*>(object) + 0x28, copy.data() + 0x28, 1);
        return ours;
    }
    const auto game = reinterpret_cast<std::uint32_t(__thiscall*)(void*)>(
        state.original)(object);
    recordOutput("aistateprimaryidleenter", state,
                 static_cast<const std::byte*>(object) + 0x20, copy.data() + 0x20, 4);
    recordOutput("aistateprimaryidleenter", state,
                 static_cast<const std::byte*>(object) + 0x28, copy.data() + 0x28, 1);
    recordScalar("aistateprimaryidleenter", state, game, ours);
    return game;
}

std::uint32_t __fastcall dozerHasTask(const void* object, void*, std::uint32_t task)
{
    auto& state = g_dozerHasTask;
    state.calls.fetch_add(1, relaxed);
    const auto ours = DozerAIView(object).hasTask(task);
    if (state.mode == Mode::replace) return ours;
    const auto game = reinterpret_cast<std::uint32_t(__thiscall*)(
        const void*, std::uint32_t)>(state.original)(object, task);
    recordScalar("aistatedozerhastask", state, game, ours);
    return game;
}

std::uint32_t __fastcall dozerTaskTarget(const void* object, void*, std::uint32_t task)
{
    auto& state = g_dozerTaskTarget;
    state.calls.fetch_add(1, relaxed);
    const auto ours = DozerAIView(object).taskTarget(task);
    if (state.mode == Mode::replace) return ours;
    const auto game = reinterpret_cast<std::uint32_t(__thiscall*)(
        const void*, std::uint32_t)>(state.original)(object, task);
    recordScalar("aistatedozertasktarget", state, game, ours);
    return game;
}

const void* __fastcall dozerDockPoint(
    const void* object, void*, std::uint32_t task, std::uint32_t point)
{
    auto& state = g_dozerDockPoint;
    state.calls.fetch_add(1, relaxed);
    const auto* ours = DozerAIView(object).dockPoint(task, point);
    if (state.mode == Mode::replace) return ours;
    const auto* game = reinterpret_cast<const void*(__thiscall*)(
        const void*, std::uint32_t, std::uint32_t)>(
            state.original)(object, task, point);
    recordScalar("aistatedozerdockpoint", state,
                 reinterpret_cast<std::uint32_t>(game),
                 reinterpret_cast<std::uint32_t>(ours));
    return game;
}

std::uint32_t __fastcall supplyFerrying(const void* object, void*)
{
    auto& state = g_supplyFerrying;
    state.calls.fetch_add(1, relaxed);
    const auto ours = SupplyTruckAIView(object).isCurrentlyFerryingSupplies();
    if (state.mode == Mode::replace) return ours;
    const auto game = reinterpret_cast<std::uint32_t(__thiscall*)(const void*)>(
        state.original)(object);
    recordScalar("aistatesupplyferrying", state, game, ours);
    return game;
}

void __fastcall unitSetValue144(void* object, void*, std::uint32_t value)
{
    auto& state = g_unitSetValue144;
    shadowWrite("aistateunitset144", state, object, UnitAIView::minimumRecoveredSize,
                0x144, 4,
                [value](void* copy) { UnitAIView(copy).setValue0144(value); },
                [statePointer = &state, value](void* target) {
                    reinterpret_cast<void(__thiscall*)(void*, std::uint32_t)>(
                        statePointer->original)(target, value);
                });
}

void __fastcall unitResetFlags(void* object, void*)
{
    auto& state = g_unitResetFlags;
    state.calls.fetch_add(1, relaxed);
    std::array<std::byte, UnitAIView::minimumRecoveredSize> copy{};
    std::memcpy(copy.data(), object, copy.size());
    UnitAIView(copy.data()).resetStateFlags();
    if (state.mode == Mode::replace) {
        std::memcpy(static_cast<std::byte*>(object) + 0x430, copy.data() + 0x430, 2);
        std::memcpy(static_cast<std::byte*>(object) + 0x144, copy.data() + 0x144, 4);
        return;
    }
    reinterpret_cast<void(__thiscall*)(void*)>(state.original)(object);
    recordOutput("aistateunitresetflags", state,
                 static_cast<const std::byte*>(object) + 0x430, copy.data() + 0x430, 2);
    recordOutput("aistateunitresetflags", state,
                 static_cast<const std::byte*>(object) + 0x144, copy.data() + 0x144, 4);
}

void __fastcall unitSetGoal(void* object, void*, const void* value)
{
    auto& state = g_unitSetGoal;
    state.calls.fetch_add(1, relaxed);
    constexpr std::size_t vectorSize = 12;
    std::array<std::byte, vectorSize> captured{};
    std::memcpy(captured.data(), value, captured.size());
    std::array<std::byte, UnitAIView::minimumRecoveredSize> copy{};
    std::memcpy(copy.data(), object, copy.size());
    const auto objectAddress = reinterpret_cast<std::uintptr_t>(object);
    const auto valueAddress = reinterpret_cast<std::uintptr_t>(value);
    const auto inside = valueAddress >= objectAddress &&
        valueAddress - objectAddress <= copy.size() - vectorSize;
    const void* shadowValue = inside
        ? static_cast<const void*>(copy.data() + (valueAddress - objectAddress))
        : static_cast<const void*>(captured.data());
    UnitAIView(copy.data()).setGoalPosition(shadowValue);
    if (state.mode == Mode::replace) {
        std::memcpy(static_cast<std::byte*>(object) + 0x21c, copy.data() + 0x21c, 0x10);
        return;
    }
    reinterpret_cast<void(__thiscall*)(void*, const void*)>(
        state.original)(object, value);
    recordOutput("aistateunitsetgoal", state,
                 static_cast<const std::byte*>(object) + 0x21c, copy.data() + 0x21c, 0x10);
}

#define RA3_UNIT_NOARG_WRITE(Name, State, Key, Offset, Size, Method)                    \
    void __fastcall Name(void* object, void*)                                           \
    {                                                                                    \
        auto& localState = State;                                                        \
        localState.calls.fetch_add(1, relaxed);                                          \
        std::array<std::byte, UnitAIView::minimumRecoveredSize> copy{};                  \
        std::memcpy(copy.data(), object, copy.size());                                   \
        UnitAIView(copy.data()).Method();                                                \
        if (localState.mode == Mode::replace) {                                          \
            std::memcpy(static_cast<std::byte*>(object) + Offset,                        \
                        copy.data() + Offset, Size);                                     \
            return;                                                                      \
        }                                                                                \
        reinterpret_cast<void(__thiscall*)(void*)>(localState.original)(object);         \
        recordOutput(Key, localState, static_cast<const std::byte*>(object) + Offset,    \
                     copy.data() + Offset, Size);                                        \
    }

RA3_UNIT_NOARG_WRITE(unitClearGoal, g_unitClearGoal, "aistateunitcleargoal",
                     0x21c, 4, clearGoal)

#undef RA3_UNIT_NOARG_WRITE

void __fastcall unitClearIds(void* object, void*)
{
    auto& state = g_unitClearIds;
    state.calls.fetch_add(1, relaxed);
    std::array<std::byte, UnitAIView::minimumRecoveredSize> copy{};
    std::memcpy(copy.data(), object, copy.size());
    UnitAIView(copy.data()).clearPendingObjectIds();
    if (state.mode == Mode::replace) {
        std::memcpy(static_cast<std::byte*>(object) + 0x49c, copy.data() + 0x49c, 4);
        std::memcpy(static_cast<std::byte*>(object) + 0x4fc, copy.data() + 0x4fc, 4);
        return;
    }
    reinterpret_cast<void(__thiscall*)(void*)>(state.original)(object);
    recordOutput("aistateunitclearids", state,
                 static_cast<const std::byte*>(object) + 0x49c,
                 copy.data() + 0x49c, 4);
    recordOutput("aistateunitclearids", state,
                 static_cast<const std::byte*>(object) + 0x4fc,
                 copy.data() + 0x4fc, 4);
}

template <typename Method>
std::uint32_t shadowUnitNoArg(
    const char* key, TargetState& state, const void* object, Method method)
{
    state.calls.fetch_add(1, relaxed);
    const auto ours = static_cast<std::uint32_t>((UnitAIView(object).*method)());
    if (state.mode == Mode::replace) return ours;
    const auto game = reinterpret_cast<std::uint32_t(__thiscall*)(const void*)>(
        state.original)(object);
    recordScalar(key, state, game, ours);
    return game;
}

std::uint32_t __fastcall unitValue250(const void* object, void*)
{
    return shadowUnitNoArg(
        "aistateunitvalue250", g_unitValue250, object, &UnitAIView::value0250);
}
std::uint32_t __fastcall unitQueued(const void* object, void*)
{
    return shadowUnitNoArg(
        "aistateunitqueued", g_unitQueued, object, &UnitAIView::hasQueuedState);
}
std::uint32_t __fastcall unitCurrentId(const void* object, void*)
{
    return shadowUnitNoArg(
        "aistateunitcurrentid", g_unitCurrentId, object, &UnitAIView::currentStateId);
}

void* __fastcall unitConditions(const void* object, void*)
{
    auto& state = g_unitConditions;
    state.calls.fetch_add(1, relaxed);
    auto* ours = UnitAIView(object).currentStateConditionBlock();
    if (state.mode == Mode::replace) return ours;
    auto* game = reinterpret_cast<void*(__thiscall*)(const void*)>(
        state.original)(object);
    recordScalar("aistateunitconditions", state,
                 reinterpret_cast<std::uint32_t>(game),
                 reinterpret_cast<std::uint32_t>(ours));
    return game;
}

void* __fastcall unitTransitions(const void* object, void*)
{
    auto& state = g_unitTransitions;
    state.calls.fetch_add(1, relaxed);
    auto* ours = UnitAIView(object).currentStateTransitionBlock();
    if (state.mode == Mode::replace) return ours;
    auto* game = reinterpret_cast<void*(__thiscall*)(const void*)>(
        state.original)(object);
    recordScalar("aistateunittransitions", state,
                 reinterpret_cast<std::uint32_t>(game),
                 reinterpret_cast<std::uint32_t>(ours));
    return game;
}

void __fastcall unitDesiredSpeed(void* object, void*, std::uint32_t bits)
{
    auto& state = g_unitDesiredSpeed;
    state.calls.fetch_add(1, relaxed);
    std::uint32_t ours = bits;
    if (state.mode == Mode::replace) {
        std::memcpy(static_cast<std::byte*>(object) + 0x188, &ours, 4);
        return;
    }
    reinterpret_cast<void(__thiscall*)(void*, std::uint32_t)>(
        state.original)(object, bits);
    recordOutput("aistateunitdesiredspeed", state,
                 static_cast<const std::byte*>(object) + 0x188, &ours, 4);
}

}  // namespace

void appendAIStateTargets(std::vector<Target>& table)
{
    table.insert(table.end(), {
        {"aistateprimaryidleenter", 0x00281BB0, {0xA1,0x64,0xBA,0xDE,0x00,0x8B,0x50,0x58,0x89,0x51,0x20,0xC6,0x41,0x28,0x00,0x33}, 5, reinterpret_cast<void*>(&primaryIdleEnter), &g_primaryIdleEnter, true},
        {"aistatedozerhastask", 0x00281E80, {0x8B,0x44,0x24,0x04,0x83,0x7C,0xC1,0x04,0x00,0x0F,0x95,0xC0,0xC2,0x04,0x00,0xCC}, 9, reinterpret_cast<void*>(&dozerHasTask), &g_dozerHasTask, true},
        {"aistatedozertasktarget", 0x00281EC0, {0x8B,0x44,0x24,0x04,0x8B,0x44,0xC1,0x04,0xC2,0x04,0x00,0xCC,0xCC,0xCC,0xCC,0xCC}, 8, reinterpret_cast<void*>(&dozerTaskTarget), &g_dozerTaskTarget, true},
        {"aistatedozerdockpoint", 0x00281F60, {0x8B,0x44,0x24,0x04,0x83,0xF8,0x02,0x77,0x25,0x8B,0x54,0x24,0x08,0x83,0xFA,0x02}, 7, reinterpret_cast<void*>(&dozerDockPoint), &g_dozerDockPoint, true},
        {"aistatesupplyferrying", 0x00282090, {0x8B,0x41,0x04,0x85,0xC0,0x74,0x19,0x8B,0x40,0x04,0x85,0xC0,0x74,0x12,0x8B,0x40}, 5, reinterpret_cast<void*>(&supplyFerrying), &g_supplyFerrying, true},
        {"aistateunitset144", 0x00282D30, {0x8B,0x44,0x24,0x04,0x89,0x81,0x44,0x01,0x00,0x00,0xC2,0x04,0x00,0xCC,0xCC,0xCC}, 10, reinterpret_cast<void*>(&unitSetValue144), &g_unitSetValue144, true},
        {"aistateunitresetflags", 0x00282D70, {0x33,0xC0,0xC6,0x81,0x30,0x04,0x00,0x00,0x01,0x88,0x81,0x31,0x04,0x00,0x00,0x89}, 9, reinterpret_cast<void*>(&unitResetFlags), &g_unitResetFlags, true},
        {"aistateunitsetgoal", 0x00282E40, {0x8B,0x44,0x24,0x04,0xC7,0x81,0x1C,0x02,0x00,0x00,0x02,0x00,0x00,0x00,0x8B,0x10}, 14, reinterpret_cast<void*>(&unitSetGoal), &g_unitSetGoal, true},
        {"aistateunitcleargoal", 0x00282E70, {0xC7,0x81,0x1C,0x02,0x00,0x00,0x00,0x00,0x00,0x00,0xC3,0xCC,0xCC,0xCC,0xCC,0xCC}, 10, reinterpret_cast<void*>(&unitClearGoal), &g_unitClearGoal, true},
        {"aistateunitclearids", 0x00282F00, {0xC7,0x81,0x9C,0x04,0x00,0x00,0xFF,0xFF,0xFF,0xFF,0xC7,0x81,0xFC,0x04,0x00,0x00}, 10, reinterpret_cast<void*>(&unitClearIds), &g_unitClearIds, true},
        {"aistateunitvalue250", 0x002830F0, {0x8B,0x81,0x50,0x02,0x00,0x00,0xC3,0xCC,0xCC,0xCC,0xCC,0xCC,0xCC,0xCC,0xCC,0xCC}, 6, reinterpret_cast<void*>(&unitValue250), &g_unitValue250, true},
        {"aistateunitqueued", 0x00283320, {0x8B,0x41,0x3C,0x85,0xC0,0x74,0x18,0x83,0xB8,0x88,0x00,0x00,0x00,0x00,0x74,0x0F}, 5, reinterpret_cast<void*>(&unitQueued), &g_unitQueued, true},
        {"aistateunitcurrentid", 0x00283520, {0x8B,0x49,0x3C,0x8B,0x41,0x04,0x85,0xC0,0x74,0x04,0x8B,0x40,0x04,0xC3,0xB8,0x3F}, 6, reinterpret_cast<void*>(&unitCurrentId), &g_unitCurrentId, true},
        {"aistateunitconditions", 0x00283570, {0x8B,0x41,0x3C,0x83,0xC0,0x2C,0xC3,0xCC,0xCC,0xCC,0xCC,0xCC,0xCC,0xCC,0xCC,0xCC}, 6, reinterpret_cast<void*>(&unitConditions), &g_unitConditions, true},
        {"aistateunittransitions", 0x00283580, {0x8B,0x41,0x3C,0x83,0xC0,0x58,0xC3,0xCC,0xCC,0xCC,0xCC,0xCC,0xCC,0xCC,0xCC,0xCC}, 6, reinterpret_cast<void*>(&unitTransitions), &g_unitTransitions, true},
        {"aistateunitdesiredspeed", 0x00283590, {0xF3,0x0F,0x10,0x44,0x24,0x04,0xF3,0x0F,0x11,0x81,0x88,0x01,0x00,0x00,0xC2,0x04}, 6, reinterpret_cast<void*>(&unitDesiredSpeed), &g_unitDesiredSpeed, true},
    });
}

}  // namespace ra3hook
