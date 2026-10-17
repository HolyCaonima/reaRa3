#pragma once

#include "ra3/gamelogic/AIState79.h"

#include <cstdint>

namespace ra3::gamelogic {

// Second pass over 0x00790000 AI states. `core` is the same absolute/stub
// contract as the first pass. The extra fields mirror .data the bodies touch:
// the on-disk dword at 0x00CC569C, the special-power name-key guard, and the
// horde-exit byte at 0x00DF8906. `logicTable` is also written to 0x00DF03FC
// for UnitAIMoveOnPathState's xfer.
struct AIState79BEnv {
    AIState79Env core{};
    std::uint32_t registry = 0x00CC79E0u;
    std::uint8_t nameKeyGuard = 0;
    std::uint32_t nameKey = 0;
    std::uint32_t nameKeyPool = 0;
    std::uint8_t hordeGate = 0;
};

bool runAIState79B(std::uint32_t va, std::uint8_t* self, std::uint32_t eaxIn,
                   const std::uint32_t* stack, int argc, AIState79BEnv& env, std::uint32_t& eaxOut);

}  // namespace ra3::gamelogic
