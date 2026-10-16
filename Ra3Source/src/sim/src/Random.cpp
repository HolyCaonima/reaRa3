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

namespace {

// The game computes 1 / (2^32 - 1) at startup (0x00BA7EC0) and stores it as
// a float, which rounds to exactly 2^-32.
const float randomScale = 0x1p-32F;
const float randomUnsignedBias = 0x1p32F;

#if defined(_MSC_VER) && defined(_M_IX86)
using StepFunction = std::uint32_t (*)(RandomState&) noexcept;
const StepFunction randomStep = &nextRandom;
#endif

}  // namespace

float randomReal(RandomState& state, float low, float high) noexcept
{
    const float delta = high - low;
    if (delta <= 0.0F) {
        return high;
    }
    return static_cast<float>(nextRandom(state)) * randomScale * delta + low;
}

#if defined(_MSC_VER) && defined(_M_IX86)
__declspec(naked) float __cdecl randomRealExtended(RandomState*, float, float) noexcept
{
    __asm {
        push ecx
        movss xmm0, dword ptr [esp + 10h]
        subss xmm0, dword ptr [esp + 0Ch]
        xorps xmm1, xmm1
        comiss xmm1, xmm0
        movss dword ptr [esp], xmm0
        jb advance
        fld dword ptr [esp + 10h]
        pop ecx
        ret

    advance:
        push dword ptr [esp + 8]
        call dword ptr [randomStep]
        add esp, 4
        test eax, eax
        mov dword ptr [esp + 10h], eax
        fild dword ptr [esp + 10h]
        jge unsignedReady
        fadd dword ptr [randomUnsignedBias]

    unsignedReady:
        fmul dword ptr [randomScale]
        fmul dword ptr [esp]
        fadd dword ptr [esp + 0Ch]
        pop ecx
        ret
    }
}
#endif

}  // namespace ra3::sim
