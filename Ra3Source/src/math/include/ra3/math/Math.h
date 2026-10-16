#pragma once

#include <cstddef>
#include <cstdint>

namespace ra3::math {

struct Vector2 {
    float x;
    float y;
};

struct Vector3 {
    float x;
    float y;
    float z;
};

struct Vector4 {
    float x;
    float y;
    float z;
    float w;
};

struct Matrix3x4 {
    float m[3][4];
};

static_assert(sizeof(Vector2) == 0x08);
static_assert(sizeof(Vector3) == 0x0C);
static_assert(sizeof(Vector4) == 0x10);
static_assert(sizeof(Matrix3x4) == 0x30);
static_assert(offsetof(Matrix3x4, m[1][0]) == 0x10);
static_assert(offsetof(Matrix3x4, m[2][0]) == 0x20);

float maximum(float a, float b) noexcept;
float minimum(float a, float b) noexcept;
float clamp(float value, float lower, float upper) noexcept;

bool exactlyEqual(const Vector3& a, const Vector3& b) noexcept;
bool exactlyEqualVector2Range(
    const Vector2* first, const Vector2* last, const Vector2* other) noexcept;

float squaredDistance(const Vector3& a, const Vector3& b) noexcept;
float distance(const Vector3& a, const Vector3& b) noexcept;
float approximateLength(const Vector3& value) noexcept;

void normalize(const Vector2& input, Vector2& output) noexcept;
Vector3 scaled(const Vector3& input, float scale) noexcept;
Vector3 transformDirection(const Matrix3x4& matrix, const Vector3& input) noexcept;
float squaredDistanceFromOffset38(const void* object, const Vector3& point) noexcept;
void clampFloatAtOffset68(void* object, float lower, float upper) noexcept;

Vector4 colorFromArgb(std::uint32_t argb) noexcept;
bool containsInclusive(const Vector2& point, const Vector2& minimum,
                       const Vector2& maximum) noexcept;
void separate(Vector2& a, Vector2& b, float factor) noexcept;

}  // namespace ra3::math
