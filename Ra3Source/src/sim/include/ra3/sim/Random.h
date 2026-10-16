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

}  // namespace ra3::sim
