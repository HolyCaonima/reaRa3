#include <ra3/sim/Random.h>

#include <ra3/common/Crc32.h>

#include <cmath>
#include <cstring>

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

std::uint32_t randomStateCrc(const RandomState& state) noexcept
{
    return ra3::common::addRotatingCrc(&state, sizeof(state), 0);
}

void setRandomVariable(
    RandomVariable& variable, float low, float high, std::int32_t distribution) noexcept
{
    variable.low = low;
    variable.high = high;
    variable.distribution = distribution;
}

namespace {

// x87 loads quiet a signaling NaN by setting the top fraction bit. Bits, not
// a float, are returned: an x86 float return would itself pass through x87.
std::uint32_t quietedThroughX87(const float& value) noexcept
{
    std::uint32_t bits = 0;
    std::memcpy(&bits, &value, sizeof(bits));
    if ((bits & 0x7F800000U) == 0x7F800000U && (bits & 0x007FFFFFU) != 0) {
        bits |= 0x00400000U;
    }
    return bits;
}

float constantOrUniform(const RandomVariable& variable, RandomState& state) noexcept
{
    if (variable.distribution == static_cast<std::int32_t>(RandomDistribution::constant)
        && variable.low == variable.high) {
        return variable.low;
    }
    return randomReal(state, variable.low, variable.high);
}

}  // namespace

void copyRandomVariable(RandomVariable& destination, const RandomVariable& source) noexcept
{
    const std::int32_t distribution = source.distribution;
    const std::uint32_t low = quietedThroughX87(source.low);
    const std::uint32_t high = quietedThroughX87(source.high);
    std::memcpy(&destination.low, &low, sizeof(low));
    std::memcpy(&destination.high, &high, sizeof(high));
    destination.distribution = distribution;
}

float clientRandomVariableValue(const RandomVariable& variable, RandomState& state) noexcept
{
    switch (static_cast<RandomDistribution>(variable.distribution)) {
    case RandomDistribution::constant:
    case RandomDistribution::uniform:
        return constantOrUniform(variable, state);
    default:
        return 0.0F;
    }
}

float logicRandomVariableValue(const RandomVariable& variable, RandomState& state) noexcept
{
    switch (static_cast<RandomDistribution>(variable.distribution)) {
    case RandomDistribution::constant:
    case RandomDistribution::uniform:
        return constantOrUniform(variable, state);
    case RandomDistribution::highBias: {
        const float unit = static_cast<float>(nextRandom(state)) * randomScale;
        const float root = std::sqrt(unit);
        return (variable.high - variable.low) * root + variable.low;
    }
    default:
        return 0.0F;
    }
}

#if defined(_MSC_VER) && defined(_M_IX86)
namespace {

using RandomRealFunction = float(__cdecl*)(RandomState*, float, float) noexcept;
const RandomRealFunction randomRealImplementation = &randomRealExtended;

}  // namespace

__declspec(naked) float __cdecl clientRandomVariableExtended(
    const RandomVariable*, RandomState*) noexcept
{
    __asm {
        mov eax, dword ptr [esp + 4]
        mov ecx, dword ptr [eax]
        test ecx, ecx
        je constantValue
        cmp ecx, 1
        je uniformValue
        fldz
        ret

    constantValue:
        movss xmm0, dword ptr [eax + 4]
        ucomiss xmm0, dword ptr [eax + 8]
        jp uniformValue
        jne uniformValue
        fld dword ptr [eax + 4]
        ret

    uniformValue:
        push dword ptr [eax + 8]
        push dword ptr [eax + 4]
        push dword ptr [esp + 10h]
        call dword ptr [randomRealImplementation]
        add esp, 12
        ret
    }
}

__declspec(naked) float __cdecl logicRandomVariableExtended(
    const RandomVariable*, RandomState*) noexcept
{
    __asm {
        mov eax, dword ptr [esp + 4]
        mov ecx, dword ptr [eax]
        test ecx, ecx
        je constantValue
        cmp ecx, 1
        je uniformValue
        cmp ecx, 5
        je highBiasValue
        fldz
        ret

    constantValue:
        movss xmm0, dword ptr [eax + 4]
        ucomiss xmm0, dword ptr [eax + 8]
        jp uniformValue
        jne uniformValue
        fld dword ptr [eax + 4]
        ret

    uniformValue:
        push dword ptr [eax + 8]
        push dword ptr [eax + 4]
        push dword ptr [esp + 10h]
        call dword ptr [randomRealImplementation]
        add esp, 12
        ret

    highBiasValue:
        sub esp, 8
        push dword ptr [esp + 10h]
        call dword ptr [randomStep]
        add esp, 4
        test eax, eax
        mov dword ptr [esp + 4], eax
        fild dword ptr [esp + 4]
        jge highBiasUnsigned
        fadd dword ptr [randomUnsignedBias]

    highBiasUnsigned:
        fmul dword ptr [randomScale]
        fstp dword ptr [esp + 4]
        fld dword ptr [esp + 4]
        fsqrt
        fstp dword ptr [esp]
        mov eax, dword ptr [esp + 0Ch]
        fld dword ptr [eax + 4]
        fld dword ptr [eax + 8]
        fsub st, st(1)
        fmul dword ptr [esp]
        faddp st(1), st
        add esp, 8
        ret
    }
}
#endif

}  // namespace ra3::sim
