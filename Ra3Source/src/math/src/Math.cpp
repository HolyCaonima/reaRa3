#include <ra3/math/Math.h>

#include <xmmintrin.h>

#include <algorithm>
#include <cmath>
#include <cstring>

namespace ra3::math {
namespace {

constexpr float quarter = 0.25F;
constexpr float elevenThirtySeconds = 0.34375F;

float reciprocalSquareRoot(float value) noexcept
{
#if defined(_MSC_VER) && defined(_M_IX86)
    float result;
    // ra3_1.0.game 0x0056F370: x87 sqrt/divide follows the caller's control word.
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

float scalar(__m128 value) noexcept
{
    return _mm_cvtss_f32(value);
}

}  // namespace

// ra3_1.0.game 0x00504620.
float maximum(float a, float b) noexcept
{
    return a > b ? a : b;
}

// ra3_1.0.game 0x00522A30.
float minimum(float a, float b) noexcept
{
    return b > a ? a : b;
}

// ra3_1.0.game 0x00504650.
float clamp(float value, float lower, float upper) noexcept
{
    if (lower > value) {
        return lower;
    }
    if (value > upper) {
        return upper;
    }
    return value;
}

// ra3_1.0.game 0x00504680.
bool exactlyEqual(const Vector3& a, const Vector3& b) noexcept
{
    return a.x == b.x && a.y == b.y && a.z == b.z;
}

// ra3_1.0.game 0x005046C0.
bool exactlyEqualVector2Range(
    const Vector2* first, const Vector2* last, const Vector2* other) noexcept
{
    while (first != last) {
        if (first->x != other->x || first->y != other->y) {
            return false;
        }
        ++first;
        ++other;
    }
    return true;
}

#if defined(_MSC_VER) && defined(_M_IX86)

// ra3_1.0.game 0x005049E0: x87 stack order is dx, dy, dz, then dx²+dy²+dz².
__declspec(naked) float squaredDistance(const Vector3&, const Vector3&) noexcept
{
    __asm {
        mov eax, dword ptr [esp + 4]
        mov ecx, dword ptr [esp + 8]
        fld dword ptr [eax]
        fsub dword ptr [ecx]
        fld dword ptr [eax + 4]
        fsub dword ptr [ecx + 4]
        fld dword ptr [eax + 8]
        fsub dword ptr [ecx + 8]
        fld st(2)
        fmulp st(3), st
        fmul st, st
        faddp st(2), st
        fmul st, st
        faddp st(1), st
        ret
    }
}

// ra3_1.0.game 0x00564150: squaredDistance order followed by x87 fsqrt.
__declspec(naked) float distance(const Vector3&, const Vector3&) noexcept
{
    __asm {
        mov eax, dword ptr [esp + 4]
        mov ecx, dword ptr [esp + 8]
        fld dword ptr [eax]
        fsub dword ptr [ecx]
        fld dword ptr [eax + 4]
        fsub dword ptr [ecx + 4]
        fld dword ptr [eax + 8]
        fsub dword ptr [ecx + 8]
        fld st(2)
        fmulp st(3), st
        fmul st, st
        faddp st(2), st
        fmul st, st
        faddp st(1), st
        fsqrt
        ret
    }
}

#else

float squaredDistance(const Vector3& a, const Vector3& b) noexcept
{
    const float x = a.x - b.x;
    const float y = a.y - b.y;
    const float z = a.z - b.z;
    return x * x + y * y + z * z;
}

float distance(const Vector3& a, const Vector3& b) noexcept
{
    return std::sqrt(squaredDistance(a, b));
}

#endif

// ra3_1.0.game 0x00523240.
float approximateLength(const Vector3& value) noexcept
{
    float largest = std::fabs(value.x);
    float middle = std::fabs(value.y);
    float smallest = std::fabs(value.z);
    if (middle > largest) {
        std::swap(middle, largest);
    }
    if (smallest > largest) {
        std::swap(smallest, largest);
    }
    if (smallest > middle) {
        std::swap(smallest, middle);
    }
#if defined(_MSC_VER) && defined(_M_IX86)
    float result;
    // x87 order: smallest*0.25, middle*0.34375, add, then add largest.
    __asm {
        fld smallest
        fmul dword ptr [quarter]
        fld middle
        fmul dword ptr [elevenThirtySeconds]
        faddp st(1), st
        fadd largest
        fstp result
    }
    return result;
#else
    return smallest * 0.25F + middle * 0.34375F + largest;
#endif
}

// ra3_1.0.game 0x0056F370.
void normalize(const Vector2& input, Vector2& output) noexcept
{
    const __m128 x = _mm_load_ss(&input.x);
    const __m128 y = _mm_load_ss(&input.y);
    const __m128 sum = _mm_add_ss(_mm_mul_ss(x, x), _mm_mul_ss(y, y));
    const float squared = scalar(sum);
    const float inverse = squared == 0.0F ? 0.0F : reciprocalSquareRoot(squared);
    const __m128 scale = _mm_set_ss(inverse);
    output.x = scalar(_mm_mul_ss(x, scale));
    output.y = scalar(_mm_mul_ss(y, scale));
}

// ra3_1.0.game 0x009814E0.
Vector3 scaled(const Vector3& input, float scale) noexcept
{
    const __m128 factor = _mm_set_ss(scale);
    Vector3 output{};
    output.x = scalar(_mm_mul_ss(_mm_load_ss(&input.x), factor));
    output.y = scalar(_mm_mul_ss(_mm_load_ss(&input.y), factor));
    output.z = scalar(_mm_mul_ss(_mm_load_ss(&input.z), factor));
    return output;
}

// ra3_1.0.game 0x00994A10.
Vector3 transformDirection(const Matrix3x4& matrix, const Vector3& input) noexcept
{
#if defined(_MSC_VER) && defined(_M_IX86)
    Vector3 output{};
    const Matrix3x4* matrixPointer = &matrix;
    const Vector3* inputPointer = &input;
    Vector3* outputPointer = &output;
    // SSE operand order is explicit because it determines NaN payload propagation.
    __asm {
        mov eax, inputPointer
        mov ecx, matrixPointer
        mov edx, outputPointer
        movss xmm1, dword ptr [eax + 4]
        movss xmm0, dword ptr [eax + 8]
        movss xmm2, dword ptr [eax]
        movss xmm3, dword ptr [ecx + 4]
        movss xmm4, dword ptr [ecx + 8]
        mulss xmm3, xmm1
        mulss xmm4, xmm0
        addss xmm3, xmm4
        movaps xmm4, xmm2
        mulss xmm4, dword ptr [ecx]
        addss xmm3, xmm4
        movss dword ptr [edx], xmm3
        movss xmm4, dword ptr [ecx + 14h]
        movss xmm3, dword ptr [ecx + 10h]
        mulss xmm3, xmm2
        mulss xmm4, xmm1
        addss xmm3, xmm4
        movss xmm4, dword ptr [ecx + 18h]
        mulss xmm4, xmm0
        addss xmm3, xmm4
        movss dword ptr [edx + 4], xmm3
        movss xmm3, dword ptr [ecx + 20h]
        mulss xmm3, xmm2
        movss xmm2, dword ptr [ecx + 24h]
        mulss xmm2, xmm1
        movss xmm1, dword ptr [ecx + 28h]
        addss xmm3, xmm2
        mulss xmm1, xmm0
        addss xmm3, xmm1
        movss dword ptr [edx + 8], xmm3
    }
    return output;
#else
    const __m128 x = _mm_load_ss(&input.x);
    const __m128 y = _mm_load_ss(&input.y);
    const __m128 z = _mm_load_ss(&input.z);
    __m128 first = _mm_mul_ss(_mm_load_ss(&matrix.m[0][1]), y);
    first = _mm_add_ss(first, _mm_mul_ss(_mm_load_ss(&matrix.m[0][2]), z));
    first = _mm_add_ss(first, _mm_mul_ss(_mm_load_ss(&matrix.m[0][0]), x));
    __m128 second = _mm_mul_ss(_mm_load_ss(&matrix.m[1][0]), x);
    second = _mm_add_ss(second, _mm_mul_ss(_mm_load_ss(&matrix.m[1][1]), y));
    second = _mm_add_ss(second, _mm_mul_ss(_mm_load_ss(&matrix.m[1][2]), z));
    __m128 third = _mm_mul_ss(_mm_load_ss(&matrix.m[2][0]), x);
    third = _mm_add_ss(third, _mm_mul_ss(_mm_load_ss(&matrix.m[2][1]), y));
    third = _mm_add_ss(third, _mm_mul_ss(_mm_load_ss(&matrix.m[2][2]), z));
    return {scalar(first), scalar(second), scalar(third)};
#endif
}

// ra3_1.0.game 0x006ABCB0.
float squaredDistanceFromOffset38(const void* object, const Vector3& point) noexcept
{
    Vector3 origin{};
    std::memcpy(&origin, static_cast<const std::uint8_t*>(object) + 0x38, sizeof(origin));
    return squaredDistance(origin, point);
}

// ra3_1.0.game 0x0055A9B0.
void clampFloatAtOffset68(void* object, float lower, float upper) noexcept
{
    float& value = *reinterpret_cast<float*>(static_cast<std::uint8_t*>(object) + 0x68);
    if (lower > value) {
        value = lower;
    } else if (value > upper) {
        value = upper;
    }
}

// ra3_1.0.game 0x005235B0.
Vector4 colorFromArgb(std::uint32_t argb) noexcept
{
    constexpr float unit = 0.0039215688593685627F;
    const __m128 scale = _mm_set_ss(unit);
    const auto component = [scale](std::uint32_t value) noexcept {
        return scalar(_mm_mul_ss(_mm_cvtsi32_ss(_mm_setzero_ps(), static_cast<int>(value)), scale));
    };
    return {
        component((argb >> 16) & 0xFF),
        component((argb >> 8) & 0xFF),
        component(argb & 0xFF),
        component(argb >> 24),
    };
}

// ra3_1.0.game 0x00528BC0.
bool containsInclusive(
    const Vector2& point, const Vector2& minimumValue, const Vector2& maximumValue) noexcept
{
    return minimumValue.x <= point.x && point.x <= maximumValue.x
        && minimumValue.y <= point.y && point.y <= maximumValue.y;
}

// ra3_1.0.game 0x00528C00.
void separate(Vector2& a, Vector2& b, float factor) noexcept
{
    __m128 amount = _mm_load_ss(&factor);
    const float one = 1.0F;
    const float half = 0.5F;
    amount = _mm_sub_ss(amount, _mm_load_ss(&one));
    amount = _mm_mul_ss(amount, _mm_load_ss(&half));

    const __m128 dx = _mm_mul_ss(_mm_sub_ss(_mm_load_ss(&b.x), _mm_load_ss(&a.x)), amount);
    const __m128 dy = _mm_mul_ss(_mm_sub_ss(_mm_load_ss(&b.y), _mm_load_ss(&a.y)), amount);
    a.x = scalar(_mm_sub_ss(_mm_load_ss(&a.x), dx));
    a.y = scalar(_mm_sub_ss(_mm_load_ss(&a.y), dy));
    b.x = scalar(_mm_add_ss(_mm_load_ss(&b.x), dx));
    b.y = scalar(_mm_add_ss(_mm_load_ss(&b.y), dy));
}

}  // namespace ra3::math
