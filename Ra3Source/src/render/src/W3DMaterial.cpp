#include <ra3/render/W3DMaterial.h>

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstring>
#include <xmmintrin.h>

namespace ra3::render {
namespace {

template <class T>
T load(const void* object, std::size_t offset) noexcept
{
    T value{};
    std::memcpy(&value, static_cast<const std::byte*>(object) + offset, sizeof(value));
    return value;
}

template <class T>
void store(void* object, std::size_t offset, T value) noexcept
{
    std::memcpy(static_cast<std::byte*>(object) + offset, &value, sizeof(value));
}

void copyFloatX87(void* destination, const void* source) noexcept
{
#if defined(_MSC_VER) && defined(_M_IX86)
    __asm {
        mov eax, source
        mov edx, destination
        fld dword ptr [eax]
        fstp dword ptr [edx]
    }
#else
    std::memcpy(destination, source, sizeof(float));
#endif
}

void copyFloatSequence(void* destination, const void* source, std::size_t count) noexcept
{
    auto* out = static_cast<std::byte*>(destination);
    const auto* in = static_cast<const std::byte*>(source);
    for (std::size_t index = 0; index != count; ++index) {
        copyFloatX87(out + index * 4, in + index * 4);
    }
}

std::uint32_t channelProduct(std::uint32_t left, std::uint32_t right) noexcept
{
    return (left * right + 0xFFU) >> 8;
}

std::uint32_t channelSum(std::uint32_t left, std::uint32_t right) noexcept
{
    const auto sum = left + right;
    return sum < 0x100U ? sum : 0xFFU;
}

}  // namespace

// Original implementation; ra3_1.0.game 0x008E5550.
void approachWrappedAngle(float& value, float target) noexcept
{
    constexpr float step = 0.1F;
    constexpr float pi = 3.1415927410125732421875F;
    constexpr float twoPi = 6.283185482025146484375F;
    if (value < target) {
        const auto distance = target - value;
        if (pi < distance) {
            value -= step;
        } else if (step < distance) {
            value += step;
        } else {
            value = target;
        }
    } else if (target < value) {
        const auto distance = value - target;
        if (pi < distance) {
            value += step;
        } else if (step < distance) {
            value -= step;
        } else {
            value = target;
        }
    } else {
        value = target;
    }
    if (pi < value) {
        value -= twoPi;
    }
    if (value < -pi) {
        value += twoPi;
    }
}

// Original implementation; ra3_1.0.game 0x008E5B00.
std::uint32_t modulatePackedColor(std::uint32_t left, std::uint32_t right) noexcept
{
    const auto a = channelProduct(left >> 24, right >> 24);
    const auto r = channelProduct(left >> 16 & 0xFFU, right >> 16 & 0xFFU);
    const auto g = channelProduct(left >> 8 & 0xFFU, right >> 8 & 0xFFU);
    const auto b = channelProduct(left & 0xFFU, right & 0xFFU);
    return a << 24 | r << 16 | g << 8 | b;
}

// Original implementation; ra3_1.0.game 0x008E5B80.
std::uint32_t addPackedColor(std::uint32_t left, std::uint32_t right) noexcept
{
    const auto a = channelSum(left >> 24, right >> 24);
    const auto r = channelSum(left >> 16 & 0xFFU, right >> 16 & 0xFFU);
    const auto g = channelSum(left >> 8 & 0xFFU, right >> 8 & 0xFFU);
    const auto b = channelSum(left & 0xFFU, right & 0xFFU);
    return a << 24 | r << 16 | g << 8 | b;
}

// Original implementation; ra3_1.0.game 0x008E5F70.
void copyFloatWords18(void* destination, const void* source) noexcept
{
    copyFloatSequence(destination, source, 18);
}

// Original implementation; ra3_1.0.game 0x008E62E0.
void copyRecords24Backward(
    const void* first, const void* last, void* destinationLast) noexcept
{
    const auto* begin = static_cast<const std::byte*>(first);
    auto* current = static_cast<const std::byte*>(last);
    auto* destination = static_cast<std::byte*>(destinationLast);
    while (current != begin) {
        current -= 24;
        destination -= 24;
        for (std::size_t offset = 0; offset != 24; offset += 4) {
            store(destination, offset, load<std::uint32_t>(current, offset));
        }
    }
}

// Original implementation; ra3_1.0.game 0x008E6330.
bool equalMatrix3x4(const float* left, const float* right) noexcept
{
    for (std::size_t index = 0; index != 12; ++index) {
        if (left[index] != right[index]) {
            return false;
        }
    }
    return true;
}

// Original implementation; ra3_1.0.game 0x008E6620.
// Generals reference: WWVegas/WWMath/matrix3d.cpp, quaternion-to-Matrix3D construction.
void quaternionToMatrix3x4(const float* value, float* matrix) noexcept
{
    const float x = value[0];
    const float y = value[1];
    const float z = value[2];
    const float w = value[3];
    const float xx2 = x * x * 2.0F;
    const float xy2 = x * y * 2.0F;
    const float xw2 = x * w * 2.0F;
    const float xz2 = x * z * 2.0F;
    const float norm = w * w + x * x + y * y + z * z;
    const float zz2 = z * z * 2.0F;
    const float yz2 = y * z * 2.0F;
    const float zw2 = z * w * 2.0F;
    const float yw2 = y * w * 2.0F;
    const float yy2 = y * y * 2.0F;

    matrix[0] = (norm - yy2) - zz2;
    matrix[1] = xy2 - zw2;
    matrix[2] = yw2 + xz2;
    matrix[3] = value[4];
    matrix[4] = zw2 + xy2;
    matrix[5] = (norm - zz2) - xx2;
    matrix[6] = yz2 - xw2;
    matrix[7] = value[5];
    matrix[8] = xz2 - yw2;
    matrix[9] = yz2 + xw2;
    matrix[10] = (norm - yy2) - xx2;
    matrix[11] = value[6];
}

// Original implementation; ra3_1.0.game 0x008E6770.
void copyDrawableDescriptor(void* destination, const void* source) noexcept
{
    for (std::size_t offset = 4; offset != 0x38; offset += 4) {
        store(destination, offset, load<std::uint32_t>(source, offset));
    }
    store(destination, 0x38, load<std::uint8_t>(source, 0x38));
    store(destination, 0x39, load<std::uint8_t>(source, 0x39));
    copyFloatX87(
        static_cast<std::byte*>(destination) + 0x3C,
        static_cast<const std::byte*>(source) + 0x3C);
    copyFloatX87(
        static_cast<std::byte*>(destination) + 0x40,
        static_cast<const std::byte*>(source) + 0x40);
    for (std::size_t offset = 0x44; offset != 0x50; offset += 4) {
        store(destination, offset, load<std::uint32_t>(source, offset));
    }
}

// Original implementation; ra3_1.0.game 0x008E6800.
void initializeDrawableDescriptor(void* descriptor) noexcept
{
    store<std::uint8_t>(descriptor, 4, 1);
    store<std::uint8_t>(descriptor, 5, 0);
    for (std::size_t offset = 8; offset != 0x1C; offset += 4) {
        store<std::uint32_t>(descriptor, offset, 0);
    }
    store(descriptor, 0x1C, 1.0F);
    store<std::uint32_t>(descriptor, 0x20, 0);
    store<std::uint32_t>(descriptor, 0x24, 0xFFFFFFFFU);
    store<std::uint32_t>(descriptor, 0x28, 0xFFFFFFFFU);
    store<std::uint32_t>(descriptor, 0x2C, 0xFFU);
    store<std::uint8_t>(descriptor, 0x30, 1);
    store<std::uint32_t>(descriptor, 0x34, 0);
    store<std::uint32_t>(descriptor, 0x38, 0);
    store<std::uint32_t>(descriptor, 0x3C, 0xFFFFFFFFU);
    for (std::size_t offset = 0x40; offset != 0x5C; offset += 4) {
        store<std::uint32_t>(descriptor, offset, 0);
    }
}

// Original implementation; ra3_1.0.game 0x008E87D0.
bool compatibleRenderFlags(std::uint32_t left, std::uint32_t right) noexcept
{
    constexpr std::uint32_t shared = (1U << 5) | (1U << 6) | (1U << 7) | (1U << 8);
    return (left & right & shared) != 0;
}

// Original implementation; ra3_1.0.game 0x008E88A0.
void copyFloatWords13(void* destination, const void* source) noexcept
{
    copyFloatSequence(destination, source, 12);
    store(destination, 0x30, load<std::uint32_t>(source, 0x30));
}

// Original implementation; ra3_1.0.game 0x008E8AB0.
void copyFloatWords15(void* destination, const void* source) noexcept
{
    for (std::size_t offset = 0; offset != 0xC; offset += 4) {
        store(destination, offset, load<std::uint32_t>(source, offset));
    }
    copyFloatSequence(
        static_cast<std::byte*>(destination) + 0xC,
        static_cast<const std::byte*>(source) + 0xC, 12);
}

// Original implementation; ra3_1.0.game 0x008EADD0.
void* renderTreeLowerBound(const void* tree, std::int32_t key) noexcept
{
    auto* candidate = const_cast<std::byte*>(static_cast<const std::byte*>(tree)) + 4;
    auto* node = reinterpret_cast<std::byte*>(static_cast<std::uintptr_t>(
        load<std::uint32_t>(tree, 0xC)));
    while (node != nullptr) {
        if (load<std::int32_t>(node, 0x10) < key) {
            node = reinterpret_cast<std::byte*>(static_cast<std::uintptr_t>(
                load<std::uint32_t>(node, 0)));
        } else {
            candidate = node;
            node = reinterpret_cast<std::byte*>(static_cast<std::uintptr_t>(
                load<std::uint32_t>(node, 4)));
        }
    }
    return candidate;
}

// Original implementation; ra3_1.0.game 0x008EAE00.
void* renderTreeUpperBound(const void* tree, std::int32_t key) noexcept
{
    auto* candidate = const_cast<std::byte*>(static_cast<const std::byte*>(tree)) + 4;
    auto* node = reinterpret_cast<std::byte*>(static_cast<std::uintptr_t>(
        load<std::uint32_t>(tree, 0xC)));
    while (node != nullptr) {
        if (key < load<std::int32_t>(node, 0x10)) {
            candidate = node;
            node = reinterpret_cast<std::byte*>(static_cast<std::uintptr_t>(
                load<std::uint32_t>(node, 4)));
        } else {
            node = reinterpret_cast<std::byte*>(static_cast<std::uintptr_t>(
                load<std::uint32_t>(node, 0)));
        }
    }
    return candidate;
}

// Original implementation; ra3_1.0.game 0x008EAF70.
void copyRenderState201(void* destination, const void* source) noexcept
{
    copyFloatSequence(destination, source, 24);
    store(destination, 0x60, load<std::uint32_t>(source, 0x60));
    store(destination, 0x64, load<std::uint32_t>(source, 0x64));
    copyFloatSequence(
        static_cast<std::byte*>(destination) + 0x68,
        static_cast<const std::byte*>(source) + 0x68, 24);
    store(destination, 0xC8, load<std::uint8_t>(source, 0xC8));
}

// Original implementation; ra3_1.0.game 0x008EB160.
// Generals reference: WWVegas/WWMath/quat.cpp, shortest-path quaternion interpolation.
void interpolateQuaternion(
    float* output, const float* first, const float* second, float fraction) noexcept
{
    const auto dot = first[0] * second[0] + first[1] * second[1]
        + first[2] * second[2] + first[3] * second[3];
    if (0.0F < dot) {
        for (std::size_t index = 0; index != 4; ++index) {
            output[index] = first[index] + (second[index] - first[index]) * fraction;
        }
    } else {
        const auto reverseFraction = -fraction;
        for (std::size_t index = 0; index != 4; ++index) {
            output[index] = first[index]
                + (first[index] + second[index]) * reverseFraction;
        }
    }
}

// Original implementation; ra3_1.0.game 0x008EB2C0.
void copyRenderRecord68(void* destination, const void* source) noexcept
{
    store(destination, 0, load<std::uint8_t>(source, 0));
    for (std::size_t offset = 4; offset != 0x14; offset += 4) {
        store(destination, offset, load<std::uint32_t>(source, offset));
    }
    copyFloatSequence(
        static_cast<std::byte*>(destination) + 0x14,
        static_cast<const std::byte*>(source) + 0x14, 12);
}

// Original implementation; ra3_1.0.game 0x008EB620.
void* copyRenderRecords68(
    const void* first, const void* last, void* destination) noexcept
{
    auto* output = static_cast<std::byte*>(destination);
    auto* input = static_cast<const std::byte*>(first);
    const auto* end = static_cast<const std::byte*>(last);
    while (input != end) {
        copyRenderRecord68(output, input);
        input += 0x44;
        output += 0x44;
    }
    return output;
}

// Original implementation; ra3_1.0.game 0x008EB6D0.
void* copyRenderRecords68Backward(
    const void* first, const void* last, void* destinationLast) noexcept
{
    const auto* begin = static_cast<const std::byte*>(first);
    auto* input = static_cast<const std::byte*>(last);
    auto* output = static_cast<std::byte*>(destinationLast);
    while (input != begin) {
        input -= 0x44;
        output -= 0x44;
        copyRenderRecord68(output, input);
    }
    return output;
}

// Original implementation; ra3_1.0.game 0x008ED5B0.
// Call ownership closes through W3DScriptedModelDraw.cpp function 0x0090CE30.
bool hasExtendedRenderFlags(std::uint32_t flags) noexcept
{
    return (flags & 0x1E0U) != 0;
}

namespace {

void sineCosine(float angle, float& sine, float& cosine) noexcept
{
#if defined(_MSC_VER) && defined(_M_IX86)
    __asm {
        fld angle
        fsincos
        mov eax, cosine
        fstp dword ptr [eax]
        mov eax, sine
        fstp dword ptr [eax]
    }
#else
    sine = std::sin(angle);
    cosine = std::cos(angle);
#endif
}

float inverseSquareRoot(float value) noexcept
{
#if defined(_MSC_VER) && defined(_M_IX86)
    float result = 0.0F;
    __asm {
        fld value
        fsqrt
        fld1
        fdivrp st(1), st
        fstp result
    }
    return result;
#else
    return 1.0F / std::sqrt(value);
#endif
}

}  // namespace

// Original implementation; ra3_1.0.game 0x008EE0B0.
// Generals reference: WWVegas/WWMath/matrix3d.cpp, affine rotation and translation.
void buildDrawableTransform(
    float* matrix, const float* origin, const void* drawableData,
    const float* target, const float* offset) noexcept
{
    const auto dx = target[0] - origin[0];
    const auto dy = target[1] - origin[1];
    const auto dz = target[2] - origin[2];
    matrix[0] = 1.0F; matrix[1] = 0.0F; matrix[2] = 0.0F; matrix[3] = origin[0] + offset[0];
    matrix[4] = 0.0F; matrix[5] = 1.0F; matrix[6] = 0.0F; matrix[7] = origin[1] + offset[1];
    matrix[8] = 0.0F; matrix[9] = 0.0F; matrix[10] = 1.0F; matrix[11] = origin[2] + offset[2];

    float sine = 0.0F;
    float cosine = 0.0F;
    sineCosine(load<float>(drawableData, 0x64), sine, cosine);
    for (const auto base : {std::size_t{0}, std::size_t{4}, std::size_t{8}}) {
        const auto x = matrix[base];
        const auto y = matrix[base + 1];
        matrix[base] = cosine * x + sine * y;
        matrix[base + 1] = cosine * y - sine * x;
    }

    sineCosine(-load<float>(drawableData, 0x68), sine, cosine);
    for (const auto base : {std::size_t{0}, std::size_t{4}, std::size_t{8}}) {
        const auto x = matrix[base];
        const auto z = matrix[base + 2];
        matrix[base] = cosine * x - sine * z;
        matrix[base + 2] = cosine * z + sine * x;
    }

    const auto* parent = reinterpret_cast<const void*>(static_cast<std::uintptr_t>(
        load<std::uint32_t>(drawableData, 0x98)));
    if (parent != nullptr) {
        sineCosine(load<float>(parent, 0x64), sine, cosine);
        for (const auto base : {std::size_t{0}, std::size_t{4}, std::size_t{8}}) {
            const auto x = matrix[base];
            const auto y = matrix[base + 1];
            matrix[base] = sine * y + cosine * x;
            matrix[base + 1] = cosine * y - sine * x;
        }
    }
    matrix[3] += matrix[0] * dx + matrix[1] * dy + matrix[2] * dz;
    matrix[7] += matrix[4] * dx + matrix[5] * dy + matrix[6] * dz;
    matrix[11] += matrix[8] * dx + matrix[9] * dy + matrix[10] * dz;
}

// Original implementation; ra3_1.0.game 0x008EFE00.
void emitSegmentQuad(
    void*& cursor, const float* first, const float* second,
    float width, std::uint32_t attribute) noexcept
{
    const auto add = [](float left, float right) noexcept {
        return _mm_cvtss_f32(_mm_add_ss(_mm_set_ss(left), _mm_set_ss(right)));
    };
    const auto subtract = [](float left, float right) noexcept {
        return _mm_cvtss_f32(_mm_sub_ss(_mm_set_ss(left), _mm_set_ss(right)));
    };
    const auto multiply = [](float left, float right) noexcept {
        return _mm_cvtss_f32(_mm_mul_ss(_mm_set_ss(left), _mm_set_ss(right)));
    };

    const auto dx = subtract(first[0], second[0]);
    const auto dy = subtract(first[1], second[1]);
    const auto dz = subtract(first[2], second[2]);
    const auto lengthSquared =
        add(add(multiply(dz, dz), multiply(dy, dy)), multiply(dx, dx));
    if (lengthSquared < 9.999999747378752E-5F) {
        return;
    }
    const auto inverseLength =
        lengthSquared == 0.0F ? 0.0F : inverseSquareRoot(lengthSquared);
    const auto scaledWidth = multiply(width, 0.5F);
    const auto halfWidth = 0.5F < scaledWidth ? scaledWidth : 0.5F;
    const auto normalizedX = multiply(inverseLength, dx);
    const auto normalizedY = multiply(inverseLength, dy);
    const auto normalizedZ = multiply(inverseLength, dz);
    const auto a = multiply(normalizedY, halfWidth);
    const auto aSecond = multiply(normalizedY, halfWidth);
    const auto b = multiply(subtract(-0.0F, normalizedX), halfWidth);
    const auto c = multiply(normalizedX, halfWidth);
    const auto zero = multiply(halfWidth, 0.0F);
    const auto d = multiply(normalizedZ, halfWidth);
    const float vertices[6][3] = {
        {subtract(add(second[0], a), c),
         subtract(add(second[1], b), aSecond),
         subtract(add(second[2], zero), d)},
        {add(subtract(first[0], a), c),
         add(subtract(first[1], b), aSecond),
         add(subtract(first[2], zero), d)},
        {subtract(subtract(second[0], a), c),
         subtract(subtract(second[1], b), aSecond),
         subtract(subtract(second[2], zero), d)},
        {subtract(add(second[0], a), c),
         subtract(add(b, second[1]), aSecond),
         subtract(add(second[2], zero), d)},
        {add(add(first[0], a), c),
         add(add(first[1], b), aSecond),
         add(add(zero, first[2]), d)},
        {add(subtract(first[0], a), c),
         add(subtract(first[1], b), aSecond),
         add(subtract(first[2], zero), d)},
    };
    auto* output = static_cast<std::byte*>(cursor);
    for (const auto& vertex : vertices) {
        store(output, 0, vertex[0]);
        store(output, 4, vertex[1]);
        store(output, 8, vertex[2]);
        store(output, 0xC, attribute);
        output += 0x18;
    }
    cursor = output;
}

}  // namespace ra3::render
