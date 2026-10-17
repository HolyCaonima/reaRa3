#include "../Targets.h"

#include <ra3/common/Player.h>

#include <array>
#include <cstdint>
#include <cstring>

namespace ra3hook {
namespace {

constexpr auto relaxed = std::memory_order_relaxed;

TargetState g_adjustCounter;
TargetState g_playerActive;
TargetState g_scoreAddD0;
TargetState g_scoreIncrement100;
TargetState g_scoreIncrementE8;
TargetState g_scoreIncrementEc;
TargetState g_scoreIncrementF0;
TargetState g_scoreIncrementD8;
TargetState g_scoreAddDc;
TargetState g_scoreStopTimer;
TargetState g_scoreFlag134;
TargetState g_scoreFlag135;
TargetState g_scoreMark134;
TargetState g_scoreAddMoney;

bool scoringEnabled() noexcept
{
    const auto global = *reinterpret_cast<const std::uint8_t**>(0x00DEBA64);
    return global != nullptr && global[0xAB] != 0;
}

std::uint32_t gameFrame() noexcept
{
    const auto global = *reinterpret_cast<const std::uint8_t**>(0x00DEBA64);
    return global == nullptr ? 0U : *reinterpret_cast<const std::uint32_t*>(global + 0x58);
}

template <std::size_t Offset, typename Apply>
void shadowNoArg(const char* key, TargetState& state, void* object, Apply apply)
{
    state.calls.fetch_add(1, relaxed);
    std::array<std::uint8_t, ra3::common::ScoreKeeper::minimumRecoveredSize> copy{};
    std::memcpy(copy.data(), object, copy.size());
    apply(copy.data());
    if (state.mode == Mode::replace) {
        std::memcpy(static_cast<std::uint8_t*>(object) + Offset, copy.data() + Offset, 4);
        return;
    }
    reinterpret_cast<void(__thiscall*)(void*)>(state.original)(object);
    recordOutput(key, state, static_cast<const std::uint8_t*>(object) + Offset,
                 copy.data() + Offset, 4);
}

void __fastcall adjustCounter(void* object, void*, std::int32_t delta)
{
    auto& state = g_adjustCounter;
    state.calls.fetch_add(1, relaxed);
    std::int32_t expected{};
    std::memcpy(&expected, static_cast<const std::uint8_t*>(object) + 0x125c, 4);
    expected += delta;
    if (expected < 0) expected = 0;
    if (state.mode == Mode::replace) {
        std::memcpy(static_cast<std::uint8_t*>(object) + 0x125c, &expected, 4);
        return;
    }
    reinterpret_cast<void(__thiscall*)(void*, std::int32_t)>(state.original)(object, delta);
    recordOutput("playeradjustcounter", state,
                 static_cast<const std::uint8_t*>(object) + 0x125c, &expected, 4);
}

std::uint32_t __fastcall playerActive(const void* object, void*)
{
    auto& state = g_playerActive;
    state.calls.fetch_add(1, relaxed);
    const auto ours = std::uint32_t(ra3::common::Player(object).isPlayerActive());
    if (state.mode == Mode::replace) return ours;
    const auto game = reinterpret_cast<std::uint32_t(__thiscall*)(const void*)>(
        state.original)(object);
    recordScalar("playeractive", state, game, ours);
    return game;
}

#define RA3_SCORE_NOARG(Name, State, Key, Offset, Method)                              \
    void __fastcall Name(void* object, void*)                                          \
    {                                                                                   \
        shadowNoArg<Offset>(Key, State, object, [](void* copy) {                        \
            ra3::common::ScoreKeeper(copy).Method(scoringEnabled());                    \
        });                                                                             \
    }

RA3_SCORE_NOARG(scoreIncrement100, g_scoreIncrement100, "scoreincrement100", 0x100, increment100)
RA3_SCORE_NOARG(scoreIncrementE8, g_scoreIncrementE8, "scoreincremente8", 0xe8, incrementE8)
RA3_SCORE_NOARG(scoreIncrementEc, g_scoreIncrementEc, "scoreincrementec", 0xec, incrementEc)
RA3_SCORE_NOARG(scoreIncrementF0, g_scoreIncrementF0, "scoreincrementf0", 0xf0, incrementF0)
RA3_SCORE_NOARG(scoreIncrementD8, g_scoreIncrementD8, "scoreincrementd8", 0xd8, incrementD8)

#undef RA3_SCORE_NOARG

void shadowScoreArg(const char* key, TargetState& state, void* object, std::size_t offset,
                    std::int32_t delta, void (ra3::common::ScoreKeeper::*method)(std::int32_t, bool) noexcept)
{
    state.calls.fetch_add(1, relaxed);
    std::int32_t expected{};
    std::memcpy(&expected, static_cast<const std::uint8_t*>(object) + offset, 4);
    std::array<std::uint8_t, ra3::common::ScoreKeeper::minimumRecoveredSize> copy{};
    std::memcpy(copy.data(), object, copy.size());
    (ra3::common::ScoreKeeper(copy.data()).*method)(delta, scoringEnabled());
    std::memcpy(&expected, copy.data() + offset, 4);
    if (state.mode == Mode::replace) {
        std::memcpy(static_cast<std::uint8_t*>(object) + offset, &expected, 4);
        return;
    }
    reinterpret_cast<void(__thiscall*)(void*, std::int32_t)>(state.original)(object, delta);
    recordOutput(key, state, static_cast<const std::uint8_t*>(object) + offset, &expected, 4);
}

void __fastcall scoreAddD0(void* object, void*, std::int32_t value)
{
    shadowScoreArg("scoreaddd0", g_scoreAddD0, object, 0xd0, value,
                   &ra3::common::ScoreKeeper::addD0);
}
void __fastcall scoreAddDc(void* object, void*, std::int32_t value)
{
    shadowScoreArg("scoreadddc", g_scoreAddDc, object, 0xdc, value,
                   &ra3::common::ScoreKeeper::addDc);
}
void __fastcall scoreAddMoney(void* object, void*, std::int32_t value)
{
    auto& state = g_scoreAddMoney;
    state.calls.fetch_add(1, relaxed);
    std::array<std::uint8_t, ra3::common::ScoreKeeper::minimumRecoveredSize> copy{};
    std::memcpy(copy.data(), object, copy.size());
    ra3::common::ScoreKeeper(copy.data()).addMoney(value, scoringEnabled());
    if (state.mode == Mode::replace) {
        std::memcpy(static_cast<std::uint8_t*>(object) + 4, copy.data() + 4, 4);
        std::memcpy(static_cast<std::uint8_t*>(object) + 0x144, copy.data() + 0x144, 4);
        return;
    }
    reinterpret_cast<void(__thiscall*)(void*, std::int32_t)>(state.original)(object, value);
    std::array<std::uint32_t, 2> game{};
    std::array<std::uint32_t, 2> ours{};
    std::memcpy(&game[0], static_cast<const std::uint8_t*>(object) + 4, 4);
    std::memcpy(&game[1], static_cast<const std::uint8_t*>(object) + 0x144, 4);
    std::memcpy(&ours[0], copy.data() + 4, 4);
    std::memcpy(&ours[1], copy.data() + 0x144, 4);
    recordOutput("scoreaddmoney", state, game.data(), ours.data(), sizeof(game));
}

void __fastcall scoreStopTimer(void* object, void*)
{
    auto& state = g_scoreStopTimer;
    state.calls.fetch_add(1, relaxed);
    std::array<std::uint8_t, ra3::common::ScoreKeeper::minimumRecoveredSize> copy{};
    std::memcpy(copy.data(), object, copy.size());
    ra3::common::ScoreKeeper(copy.data()).stopTimer(gameFrame(), scoringEnabled());
    if (state.mode == Mode::replace) {
        std::memcpy(static_cast<std::uint8_t*>(object) + 0xf4, copy.data() + 0xf4, 8);
        return;
    }
    reinterpret_cast<void(__thiscall*)(void*)>(state.original)(object);
    recordOutput("scorestoptimer", state, static_cast<const std::uint8_t*>(object) + 0xf4,
                 copy.data() + 0xf4, 8);
}

template <std::size_t Offset, void (ra3::common::ScoreKeeper::*Method)(bool) noexcept>
void scoreFlag(const char* key, TargetState& state, void* object, bool value)
{
    state.calls.fetch_add(1, relaxed);
    std::uint8_t expected = *(static_cast<const std::uint8_t*>(object) + Offset);
    std::array<std::uint8_t, ra3::common::ScoreKeeper::minimumRecoveredSize> copy{};
    std::memcpy(copy.data(), object, copy.size());
    (ra3::common::ScoreKeeper(copy.data()).*Method)(value);
    expected = copy[Offset];
    if (state.mode == Mode::replace) {
        *(static_cast<std::uint8_t*>(object) + Offset) = expected;
        return;
    }
    reinterpret_cast<void(__thiscall*)(void*, bool)>(state.original)(object, value);
    recordOutput(key, state, static_cast<const std::uint8_t*>(object) + Offset, &expected, 1);
}

void __fastcall scoreFlag134(void* object, void*, bool value)
{
    scoreFlag<0x134, &ra3::common::ScoreKeeper::setFlag134>(
        "scoreflag134", g_scoreFlag134, object, value);
}
void __fastcall scoreFlag135(void* object, void*, bool value)
{
    scoreFlag<0x135, &ra3::common::ScoreKeeper::setFlag135>(
        "scoreflag135", g_scoreFlag135, object, value);
}
void __fastcall scoreMark134(void* object, void*)
{
    auto& state = g_scoreMark134;
    state.calls.fetch_add(1, relaxed);
    const std::uint8_t expected = 1;
    if (state.mode == Mode::replace) {
        *(static_cast<std::uint8_t*>(object) + 0x134) = expected;
        return;
    }
    reinterpret_cast<void(__thiscall*)(void*)>(state.original)(object);
    recordOutput("scoremark134", state, static_cast<const std::uint8_t*>(object) + 0x134,
                 &expected, 1);
}

}  // namespace

void appendPlayerTargets(std::vector<Target>& table)
{
    table.insert(table.end(), {
        {"playeradjustcounter", 0x003E1530, {0x8B,0x44,0x24,0x04,0x01,0x81,0x5C,0x12,0x00,0x00,0x79,0x0A,0xC7,0x81,0x5C,0x12}, 10, reinterpret_cast<void*>(&adjustCounter), &g_adjustCounter, true},
        {"playeractive", 0x003E1BA0, {0x80,0xB9,0x06,0x01,0x00,0x00,0x00,0x75,0x0F,0x80,0xB9,0x3C,0x12,0x00,0x00,0x00}, 7, reinterpret_cast<void*>(&playerActive), &g_playerActive, true},
        {"scoreaddd0", 0x003E2600, {0xA1,0x64,0xBA,0xDE,0x00,0x80,0xB8,0xAB,0x00,0x00,0x00,0x00,0x74,0x0A,0x8B,0x54}, 5, reinterpret_cast<void*>(&scoreAddD0), &g_scoreAddD0, true},
        {"scoreincrement100", 0x003E2650, {0xA1,0x64,0xBA,0xDE,0x00,0x80,0xB8,0xAB,0x00,0x00,0x00,0x00,0x74,0x07,0x83,0x81}, 5, reinterpret_cast<void*>(&scoreIncrement100), &g_scoreIncrement100, true},
        {"scoreincremente8", 0x003E2670, {0xA1,0x64,0xBA,0xDE,0x00,0x80,0xB8,0xAB,0x00,0x00,0x00,0x00,0x74,0x07,0x83,0x81}, 5, reinterpret_cast<void*>(&scoreIncrementE8), &g_scoreIncrementE8, true},
        {"scoreincrementec", 0x003E2690, {0xA1,0x64,0xBA,0xDE,0x00,0x80,0xB8,0xAB,0x00,0x00,0x00,0x00,0x74,0x07,0x83,0x81}, 5, reinterpret_cast<void*>(&scoreIncrementEc), &g_scoreIncrementEc, true},
        {"scoreincrementf0", 0x003E26B0, {0xA1,0x64,0xBA,0xDE,0x00,0x80,0xB8,0xAB,0x00,0x00,0x00,0x00,0x74,0x07,0x83,0x81}, 5, reinterpret_cast<void*>(&scoreIncrementF0), &g_scoreIncrementF0, true},
        {"scoreincrementd8", 0x003E26D0, {0xA1,0x64,0xBA,0xDE,0x00,0x80,0xB8,0xAB,0x00,0x00,0x00,0x00,0x74,0x07,0x83,0x81}, 5, reinterpret_cast<void*>(&scoreIncrementD8), &g_scoreIncrementD8, true},
        {"scoreadddc", 0x003E26F0, {0xA1,0x64,0xBA,0xDE,0x00,0x80,0xB8,0xAB,0x00,0x00,0x00,0x00,0x74,0x0A,0x8B,0x54}, 5, reinterpret_cast<void*>(&scoreAddDc), &g_scoreAddDc, true},
        {"scorestoptimer", 0x003E2710, {0x8B,0x15,0x64,0xBA,0xDE,0x00,0x80,0xBA,0xAB,0x00,0x00,0x00,0x74,0x1F,0x8B}, 6, reinterpret_cast<void*>(&scoreStopTimer), &g_scoreStopTimer, true},
        {"scoreflag134", 0x003E2800, {0x8A,0x44,0x24,0x04,0x88,0x81,0x34,0x01,0x00,0x00,0xC2,0x04,0x00,0xCC,0xCC,0xCC}, 10, reinterpret_cast<void*>(&scoreFlag134), &g_scoreFlag134, true},
        {"scoreflag135", 0x003E2810, {0x8A,0x44,0x24,0x04,0x88,0x81,0x35,0x01,0x00,0x00,0xC2,0x04,0x00,0xCC,0xCC,0xCC}, 10, reinterpret_cast<void*>(&scoreFlag135), &g_scoreFlag135, true},
        {"scoremark134", 0x003E2820, {0xC6,0x81,0x34,0x01,0x00,0x00,0x01,0xC3,0xCC,0xCC,0xCC,0xCC,0xCC,0xCC,0xCC,0xCC}, 7, reinterpret_cast<void*>(&scoreMark134), &g_scoreMark134, true},
        {"scoreaddmoney", 0x003E2830, {0xA1,0x64,0xBA,0xDE,0x00,0x80,0xB8,0xAB,0x00,0x00,0x00,0x00,0x74,0x0D,0x8B,0x44}, 5, reinterpret_cast<void*>(&scoreAddMoney), &g_scoreAddMoney, true},
    });
}

}  // namespace ra3hook
