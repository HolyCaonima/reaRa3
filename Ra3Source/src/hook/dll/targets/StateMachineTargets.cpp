#include "../Targets.h"

#include <ra3/common/StateMachine.h>

#include <cstdint>

namespace ra3hook {
namespace {

constexpr auto relaxed = std::memory_order_relaxed;

TargetState g_getId;
TargetState g_goalDword;
TargetState g_pathAt;

std::uint32_t __fastcall hookGetId(void* self, void*)
{
    auto& state = g_getId;
    state.calls.fetch_add(1, relaxed);
    const auto ours = ra3::common::currentStateId(self);
    if (state.mode == Mode::replace) {
        state.matches.fetch_add(1, relaxed);
        return ours;
    }
    const auto game = reinterpret_cast<std::uint32_t(__thiscall*)(void*)>(state.original)(self);
    recordScalar("smgetid", state, game, ours);
    return game;
}

std::uint32_t __fastcall hookGoalDword(void* self, void*, std::uint32_t value)
{
    auto& state = g_goalDword;
    state.calls.fetch_add(1, relaxed);
    const auto ours = ra3::common::storeGoalDword(self, value);
    if (state.mode == Mode::replace) {
        state.matches.fetch_add(1, relaxed);
        return ours;
    }
    const auto game =
        reinterpret_cast<std::uint32_t(__thiscall*)(void*, std::uint32_t)>(state.original)(self, value);
    recordScalar("smgoaldword", state, game, ours);
    return game;
}

std::uint32_t __fastcall hookPathAt(const void* self, void*, std::int32_t index)
{
    auto& state = g_pathAt;
    state.calls.fetch_add(1, relaxed);
    const auto ours = ra3::common::pathPointAt(self, index);
    if (state.mode == Mode::replace) {
        state.matches.fetch_add(1, relaxed);
        return ours;
    }
    const auto game =
        reinterpret_cast<std::uint32_t(__thiscall*)(const void*, std::int32_t)>(state.original)(self, index);
    recordScalar("smpathat", state, game, ours);
    return game;
}

}  // namespace

void appendStateMachineTargets(std::vector<Target>& table)
{
    table.insert(table.end(), {
        {"smgetid", 0x001A82E0,
         {0x8B, 0x41, 0x04, 0x85, 0xC0, 0x74, 0x04, 0x8B,
          0x40, 0x04, 0xC3, 0xB8, 0x3F, 0x42, 0x0F, 0x00},
         5, reinterpret_cast<void*>(&hookGetId), &g_getId, true},
        {"smgoaldword", 0x001AA2A0,
         {0x8B, 0x44, 0x24, 0x04, 0x89, 0x41, 0x70, 0xC2,
          0x04, 0x00, 0xCC, 0xCC, 0xCC, 0xCC, 0xCC, 0xCC},
         7, reinterpret_cast<void*>(&hookGoalDword), &g_goalDword, true},
        {"smpathat", 0x001AC540,
         {0x56, 0x8B, 0x74, 0x24, 0x08, 0x85, 0xF6, 0x7C,
          0x27, 0x8B, 0x51, 0x5C, 0x2B, 0x51, 0x58, 0xB8},
         5, reinterpret_cast<void*>(&hookPathAt), &g_pathAt, true},
    });
}

}  // namespace ra3hook
