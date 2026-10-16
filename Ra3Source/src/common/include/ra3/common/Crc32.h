#pragma once

#include <cstdint>

namespace ra3::common {

std::uint32_t crc32CString(const char* text) noexcept;

// ra3_1.0.game 0x0092A0A0, the Generals CRC::computeCRC byte loop: each byte
// rotates the running value left by one bit and adds the byte. A null buffer
// returns `crc` unchanged.
std::uint32_t addRotatingCrc(const void* data, std::uint32_t length, std::uint32_t crc) noexcept;

}  // namespace ra3::common
