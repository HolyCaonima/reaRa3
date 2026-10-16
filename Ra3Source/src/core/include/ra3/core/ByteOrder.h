#pragma once

#include <cstdint>

namespace ra3 {

inline std::uint32_t readU32LE(const std::uint8_t* p)
{
    return std::uint32_t(p[0]) | (std::uint32_t(p[1]) << 8) | (std::uint32_t(p[2]) << 16) |
           (std::uint32_t(p[3]) << 24);
}

inline std::uint32_t readU32BE(const std::uint8_t* p)
{
    return (std::uint32_t(p[0]) << 24) | (std::uint32_t(p[1]) << 16) | (std::uint32_t(p[2]) << 8) |
           std::uint32_t(p[3]);
}

inline void writeU32LE(std::uint8_t* p, std::uint32_t value)
{
    p[0] = std::uint8_t(value);
    p[1] = std::uint8_t(value >> 8);
    p[2] = std::uint8_t(value >> 16);
    p[3] = std::uint8_t(value >> 24);
}

inline void writeU32BE(std::uint8_t* p, std::uint32_t value)
{
    p[0] = std::uint8_t(value >> 24);
    p[1] = std::uint8_t(value >> 16);
    p[2] = std::uint8_t(value >> 8);
    p[3] = std::uint8_t(value);
}

}  // namespace ra3
