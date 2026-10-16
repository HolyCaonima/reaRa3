#include <ra3/sim/Random.h>

namespace ra3::sim {

// ra3_1.0.game 0x005A9310. State is six contiguous little-endian dwords.
std::uint32_t nextRandom(RandomState& state) noexcept
{
    std::uint32_t value = state.words[5];
    bool carry = false;
    for (int index = 4; index >= 0; --index) {
        const std::uint32_t addend = state.words[static_cast<std::size_t>(index)];
        const std::uint32_t withoutCarry = value + addend;
        const std::uint32_t sum = withoutCarry + static_cast<std::uint32_t>(carry);
        carry = withoutCarry < value || sum < withoutCarry;
        value = sum;
        state.words[static_cast<std::size_t>(index)] = value;
    }

    for (int index = 5; index >= 0; --index) {
        ++state.words[static_cast<std::size_t>(index)];
        if (state.words[static_cast<std::size_t>(index)] != 0) {
            break;
        }
    }
    return state.words[0];
}

// ra3_1.0.game 0x005A93B0 uses this transition for all three streams.
void seedRandom(RandomState& state, std::uint32_t seed) noexcept
{
    constexpr std::array<std::uint32_t, 6> initial{
        0xF22D0E56U, 0x883126E9U, 0xC624DD2FU,
        0x0702C49CU, 0x9E353F7DU, 0x6FDF3B64U};
    for (std::size_t index = 0; index < state.words.size(); ++index) {
        state.words[index] = seed + initial[index];
    }
}

// ra3_1.0.game 0x005A94C0. Unlike the client/audio helpers, logic does not
// advance the stream when low >= high.
int logicRandomInteger(RandomState& state, int low, int high) noexcept
{
    if (low >= high) {
        return low;
    }
    const std::uint32_t width =
        static_cast<std::uint32_t>(high) - static_cast<std::uint32_t>(low) + 1U;
    if (width == 0) {
        return high;
    }
    return static_cast<int>(
        nextRandom(state) % width + static_cast<std::uint32_t>(low));
}

// ra3_1.0.game 0x005A94F0 and 0x005A9520 differ only in the selected stream.
int clientRandomInteger(RandomState& state, int low, int high) noexcept
{
    const std::uint32_t width =
        static_cast<std::uint32_t>(high) - static_cast<std::uint32_t>(low) + 1U;
    if (width == 0) {
        return high;
    }
    return static_cast<int>(
        nextRandom(state) % width + static_cast<std::uint32_t>(low));
}

}  // namespace ra3::sim
