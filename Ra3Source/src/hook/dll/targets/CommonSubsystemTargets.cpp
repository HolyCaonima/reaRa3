#include "../Targets.h"

#include <ra3/common/CommonSubsystem.h>

#include <cstdint>

namespace ra3hook {
namespace {

constexpr auto relaxed = std::memory_order_relaxed;

TargetState g_subsysMoney;
TargetState g_subsysEnergy;
TargetState g_subsysCommand;
TargetState g_subsysPlayer;
TargetState g_subsysScore;
TargetState g_subsysTeamProto;
TargetState g_subsysGather;
TargetState g_subsysDisguise;

std::uint32_t __fastcall subsysMoney(void* object, void*)
{
    auto& state = g_subsysMoney;
    state.calls.fetch_add(1, relaxed);
    const auto ours = reinterpret_cast<const ra3::common::CommonSubsystem*>(object)->moneyName();
    if (state.mode == Mode::replace) {
        state.matches.fetch_add(1, relaxed);
        return ours;
    }
    const auto game = reinterpret_cast<std::uint32_t(__thiscall*)(void*)>(state.original)(object);
    recordScalar("subsysmoney", state, game, ours);
    return game;
}

std::uint32_t __fastcall subsysEnergy(void* object, void*)
{
    auto& state = g_subsysEnergy;
    state.calls.fetch_add(1, relaxed);
    const auto ours = reinterpret_cast<const ra3::common::CommonSubsystem*>(object)->energyName();
    if (state.mode == Mode::replace) {
        state.matches.fetch_add(1, relaxed);
        return ours;
    }
    const auto game = reinterpret_cast<std::uint32_t(__thiscall*)(void*)>(state.original)(object);
    recordScalar("subsysenergy", state, game, ours);
    return game;
}

std::uint32_t __fastcall subsysCommand(void* object, void*)
{
    auto& state = g_subsysCommand;
    state.calls.fetch_add(1, relaxed);
    const auto ours = reinterpret_cast<const ra3::common::CommonSubsystem*>(object)->commandName();
    if (state.mode == Mode::replace) {
        state.matches.fetch_add(1, relaxed);
        return ours;
    }
    const auto game = reinterpret_cast<std::uint32_t(__thiscall*)(void*)>(state.original)(object);
    recordScalar("subsyscommand", state, game, ours);
    return game;
}

std::uint32_t __fastcall subsysPlayer(void* object, void*)
{
    auto& state = g_subsysPlayer;
    state.calls.fetch_add(1, relaxed);
    const auto ours = reinterpret_cast<const ra3::common::CommonSubsystem*>(object)->playerName();
    if (state.mode == Mode::replace) {
        state.matches.fetch_add(1, relaxed);
        return ours;
    }
    const auto game = reinterpret_cast<std::uint32_t(__thiscall*)(void*)>(state.original)(object);
    recordScalar("subsysplayer", state, game, ours);
    return game;
}

std::uint32_t __fastcall subsysScore(void* object, void*)
{
    auto& state = g_subsysScore;
    state.calls.fetch_add(1, relaxed);
    const auto ours = reinterpret_cast<const ra3::common::CommonSubsystem*>(object)->scoreKeeperName();
    if (state.mode == Mode::replace) {
        state.matches.fetch_add(1, relaxed);
        return ours;
    }
    const auto game = reinterpret_cast<std::uint32_t(__thiscall*)(void*)>(state.original)(object);
    recordScalar("subsysscore", state, game, ours);
    return game;
}

std::uint32_t __fastcall subsysTeamProto(void* object, void*)
{
    auto& state = g_subsysTeamProto;
    state.calls.fetch_add(1, relaxed);
    const auto ours = reinterpret_cast<const ra3::common::CommonSubsystem*>(object)->teamPrototypeName();
    if (state.mode == Mode::replace) {
        state.matches.fetch_add(1, relaxed);
        return ours;
    }
    const auto game = reinterpret_cast<std::uint32_t(__thiscall*)(void*)>(state.original)(object);
    recordScalar("subsysteamproto", state, game, ours);
    return game;
}

std::uint32_t __fastcall subsysGather(void* object, void*)
{
    auto& state = g_subsysGather;
    state.calls.fetch_add(1, relaxed);
    const auto ours = reinterpret_cast<const ra3::common::CommonSubsystem*>(object)->resourceGatheringManagerName();
    if (state.mode == Mode::replace) {
        state.matches.fetch_add(1, relaxed);
        return ours;
    }
    const auto game = reinterpret_cast<std::uint32_t(__thiscall*)(void*)>(state.original)(object);
    recordScalar("subsysgather", state, game, ours);
    return game;
}

std::uint32_t __fastcall subsysDisguise(void* object, void*)
{
    auto& state = g_subsysDisguise;
    state.calls.fetch_add(1, relaxed);
    const auto ours = reinterpret_cast<const ra3::common::CommonSubsystem*>(object)->disguiseGuardName();
    if (state.mode == Mode::replace) {
        state.matches.fetch_add(1, relaxed);
        return ours;
    }
    const auto game = reinterpret_cast<std::uint32_t(__thiscall*)(void*)>(state.original)(object);
    recordScalar("subsysdisguise", state, game, ours);
    return game;
}

}  // namespace

void appendCommonSubsystemTargets(std::vector<Target>& table)
{
    table.insert(table.end(), {
        {"subsysdisguise", 0x003E0BA0,
         {0xB8, 0x10, 0x49, 0xC3, 0x00, 0xC3, 0xCC, 0xCC,
          0xCC, 0xCC, 0xCC, 0xCC, 0xCC, 0xCC, 0xCC, 0xCC},
         5, reinterpret_cast<void*>(&subsysDisguise), &g_subsysDisguise, true},
        {"subsysgather", 0x003EB200,
         {0xB8, 0xD0, 0x50, 0xC3, 0x00, 0xC3, 0xCC, 0xCC,
          0xCC, 0xCC, 0xCC, 0xCC, 0xCC, 0xCC, 0xCC, 0xCC},
         5, reinterpret_cast<void*>(&subsysGather), &g_subsysGather, true},
        {"subsyscommand", 0x003E72C0,
         {0xB8, 0xF8, 0x4E, 0xC3, 0x00, 0xC3, 0xCC, 0xCC,
          0xCC, 0xCC, 0xCC, 0xCC, 0xCC, 0xCC, 0xCC, 0xCC},
         5, reinterpret_cast<void*>(&subsysCommand), &g_subsysCommand, true},
        {"subsysmoney", 0x003E72B0,
         {0xB8, 0xE0, 0x4E, 0xC3, 0x00, 0xC3, 0xCC, 0xCC,
          0xCC, 0xCC, 0xCC, 0xCC, 0xCC, 0xCC, 0xCC, 0xCC},
         5, reinterpret_cast<void*>(&subsysMoney), &g_subsysMoney, true},
        {"subsysenergy", 0x003E9BC0,
         {0xB8, 0xF8, 0x4F, 0xC3, 0x00, 0xC3, 0xCC, 0xCC,
          0xCC, 0xCC, 0xCC, 0xCC, 0xCC, 0xCC, 0xCC, 0xCC},
         5, reinterpret_cast<void*>(&subsysEnergy), &g_subsysEnergy, true},
        {"subsysplayer", 0x003F5FA0,
         {0xB8, 0x40, 0x53, 0xC3, 0x00, 0xC3, 0xCC, 0xCC,
          0xCC, 0xCC, 0xCC, 0xCC, 0xCC, 0xCC, 0xCC, 0xCC},
         5, reinterpret_cast<void*>(&subsysPlayer), &g_subsysPlayer, true},
        {"subsysteamproto", 0x003F9F70,
         {0xB8, 0xEC, 0x53, 0xC3, 0x00, 0xC3, 0xCC, 0xCC,
          0xCC, 0xCC, 0xCC, 0xCC, 0xCC, 0xCC, 0xCC, 0xCC},
         5, reinterpret_cast<void*>(&subsysTeamProto), &g_subsysTeamProto, true},
        {"subsysscore", 0x003FC3A0,
         {0xB8, 0x00, 0x55, 0xC3, 0x00, 0xC3, 0xCC, 0xCC,
          0xCC, 0xCC, 0xCC, 0xCC, 0xCC, 0xCC, 0xCC, 0xCC},
         5, reinterpret_cast<void*>(&subsysScore), &g_subsysScore, true},
    });
}

}  // namespace ra3hook
