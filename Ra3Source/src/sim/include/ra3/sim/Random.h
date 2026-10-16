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

// ra3_1.0.game 0x005A9390, Generals GetGameLogicRandomSeedCRC: the rotating
// byte CRC of the six state words, starting from zero.
std::uint32_t randomStateCrc(const RandomState& state) noexcept;

// Generals RandomVariable::DistributionType.
enum class RandomDistribution : std::int32_t {
    constant = 0,
    uniform = 1,
    gaussian = 2,
    triangular = 3,
    lowBias = 4,
    highBias = 5,
};

// Layout shared by GameClientRandomVariable and GameLogicRandomVariable.
struct RandomVariable {
    std::int32_t distribution;
    float low;
    float high;
};

static_assert(sizeof(RandomVariable) == 12, "RA3 random variable layout changed");

// ra3_1.0.game 0x005A9640.
void setRandomVariable(
    RandomVariable& variable, float low, float high, std::int32_t distribution) noexcept;

// ra3_1.0.game 0x005A9660 and 0x005A9680, the two compiler-emitted copies.
// Both move low and high through x87, which quiets signaling NaNs.
void copyRandomVariable(RandomVariable& destination, const RandomVariable& source) noexcept;

// ra3_1.0.game 0x005A96A0 (client stream) and 0x005A9730 (logic stream).
// constant returns low when low == high and otherwise behaves as uniform,
// which is randomReal(low, high). The logic variable also implements
// highBias, low + (high - low) * sqrt(r) with r and its root stored as
// floats. Every other distribution returns 0. The portable forms match the
// originals under the same conditions as randomReal.
float clientRandomVariableValue(const RandomVariable& variable, RandomState& state) noexcept;
float logicRandomVariableValue(const RandomVariable& variable, RandomState& state) noexcept;
#if defined(_MSC_VER) && defined(_M_IX86)
float __cdecl clientRandomVariableExtended(
    const RandomVariable* variable, RandomState* state) noexcept;
float __cdecl logicRandomVariableExtended(
    const RandomVariable* variable, RandomState* state) noexcept;
#endif

}  // namespace ra3::sim
