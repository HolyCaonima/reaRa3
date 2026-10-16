#pragma once

#include <array>
#include <cstdint>

namespace ra3::sim {

struct RandomState {
    std::array<std::uint32_t, 6> words;
};

static_assert(sizeof(RandomState) == 24, "RA3 random state layout changed");

std::uint32_t nextRandom(RandomState& state) noexcept;
void seedRandom(RandomState& state, std::uint32_t seed) noexcept;

int logicRandomInteger(RandomState& state, int low, int high) noexcept;
int clientRandomInteger(RandomState& state, int low, int high) noexcept;

// ra3_1.0.game 0x005A9550 / 0x005A95A0 / 0x005A95F0 (logic / client / audio).
// Returns `high` without advancing unless the single-precision high - low is
// positive or NaN; otherwise unsigned(next) * 2^-32 * delta + low. Matches
// the original under the game's 24-bit x87 precision (control word 0x007F)
// while every intermediate stays in single precision's normal range; x87
// keeps its wider exponent there, so denormal or overflowing operands differ.
float randomReal(RandomState& state, float low, float high) noexcept;
#if defined(_MSC_VER) && defined(_M_IX86)
// Same x87 instruction order, leaving ST0 rounded only by the caller's control word.
float __cdecl randomRealExtended(RandomState* state, float low, float high) noexcept;
#endif

}  // namespace ra3::sim
