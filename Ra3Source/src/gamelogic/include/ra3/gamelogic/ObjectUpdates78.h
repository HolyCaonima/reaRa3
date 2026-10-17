#pragma once

#include <cstdint>

#include "ra3/common/Thing78.h"

namespace ra3::gamelogic {

// Update, behavior and draw bodies in 0x00780000-0x0078FFFF.
bool runObjectUpdates78(
    std::uint32_t va, ra3::common::Mem& mem, std::uint32_t ecx, const std::uint32_t* args, int argc,
    ra3::common::RunResult& out);

} // namespace ra3::gamelogic
