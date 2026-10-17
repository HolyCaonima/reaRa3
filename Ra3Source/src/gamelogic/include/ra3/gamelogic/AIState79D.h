#pragma once

#include "ra3/gamelogic/AIState79.h"

#include <cstdint>

namespace ra3::gamelogic {

// Fourth pass over 0x00790000. Same thiscall/stub contract as AIState79Env.
// `scaleBits` is the dword at 0x00DEEEE8. `terrain` is 0x00DFC190,
// `pathfinder` is 0x00DFC198 and `partition` is 0x00DFB9D8. The on-disk
// image of each of those globals is zero, so the oracle writes the case value
// before the call. `env.logicTable` is 0x00DFC4B4 and `env.maxFloatBits` is
// 0x00BE700C.

struct AIState79DEnv {
    AIState79Env env;
    std::uint32_t scaleBits = 0;
    std::uint32_t terrain = 0;
    std::uint32_t pathfinder = 0;
    std::uint32_t partition = 0;
};

bool runAIState79D(std::uint32_t va, std::uint8_t* self, std::uint32_t eaxIn,
                   const std::uint32_t* stack, int argc, AIState79DEnv& env, std::uint32_t& eaxOut);

}  // namespace ra3::gamelogic
