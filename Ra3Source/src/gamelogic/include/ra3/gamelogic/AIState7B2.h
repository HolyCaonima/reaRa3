#pragma once

#include <ra3/gamelogic/AIState7B.h>

#include <cstdint>

namespace ra3::gamelogic {

// Second-pass AI state methods in 0x007B0000. Globals that the bodies load from
// absolute .data addresses are supplied here so the offline checker can plant them.
struct AIState7B2Env : AIState7BEnv {
    std::uint32_t globalDf03fc = 0;
    std::uint32_t globalDe9b1c = 0;
    std::uint32_t globalDfc058 = 0;
    std::uint32_t globalDf6ec0 = 0;
};

bool runAIState7B2(std::uint32_t va, std::uint8_t* self, std::uint32_t eaxIn, const std::uint32_t* stack,
                   int argc, AIState7B2Env& env, std::uint32_t& eaxOut);

}  // namespace ra3::gamelogic
