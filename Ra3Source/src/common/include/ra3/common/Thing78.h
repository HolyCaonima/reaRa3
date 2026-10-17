#pragma once

#include <cstdint>
#include <cstring>

namespace ra3::common {

// Shared blob for the 0x00780000 ports. Pointers are absolute and use the
// same base the Unicorn heap uses, so stored pointers compare byte for byte.
inline constexpr std::uint32_t kArenaBase = 0x60000000u;

struct Tape {
    const std::uint32_t* eax = nullptr;
    const std::uint32_t* st0 = nullptr;
    std::uint32_t count = 0;
    std::uint32_t index = 0;
    std::uint32_t trace[768]{};
    std::uint32_t traceCount = 0;
    std::uint32_t gVa[32]{};
    std::uint32_t gVal[32]{};
    int gCount = 0;
};

struct CallArg {
    std::uint32_t value = 0;
    // 0: raw immediate or arena pointer. 1: first dword of a stack slot.
    std::uint8_t kind = 0;
};

struct Mem {
    std::uint8_t* bytes = nullptr;
    std::uint32_t base = kArenaBase;
    std::uint32_t size = 0;
    Tape* tape = nullptr;

    std::uint32_t ld(std::uint32_t addr) const;
    void st(std::uint32_t addr, std::uint32_t value);
    std::uint8_t ld8(std::uint32_t addr) const;
    void st8(std::uint32_t addr, std::uint8_t value);
    std::uint16_t ld16(std::uint32_t addr) const;
    float ldf(std::uint32_t addr) const;
    void stf(std::uint32_t addr, float value);
    std::uint32_t call(std::uint32_t ecx, const CallArg* args, int n, std::uint32_t* st0 = nullptr);
};

struct RunResult {
    std::uint32_t eax = 0;
    std::uint32_t st0 = 0;
};

inline CallArg rawArg(std::uint32_t value) noexcept {
    return CallArg{value, 0};
}

inline CallArg stackArg(std::uint32_t content) noexcept {
    return CallArg{content, 1};
}

inline float bitsToFloat(std::uint32_t bits) noexcept {
    float value = 0.0F;
    std::memcpy(&value, &bits, sizeof(value));
    return value;
}

inline std::uint32_t floatToBits(float value) noexcept {
    std::uint32_t bits = 0;
    std::memcpy(&bits, &value, sizeof(bits));
    return bits;
}

// True when va is one of the common Thing/matrix/scale bodies.
bool runThing78(std::uint32_t va, Mem& mem, std::uint32_t ecx, const std::uint32_t* args, int argc, RunResult& out);

} // namespace ra3::common
