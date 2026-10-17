#pragma once

#include <cstddef>
#include <cstdint>

namespace ra3::render {

void approachWrappedAngle(float& value, float target) noexcept;
std::uint32_t modulatePackedColor(std::uint32_t left, std::uint32_t right) noexcept;
std::uint32_t addPackedColor(std::uint32_t left, std::uint32_t right) noexcept;

void copyFloatWords18(void* destination, const void* source) noexcept;
void copyRecords24Backward(
    const void* first, const void* last, void* destinationLast) noexcept;
bool equalMatrix3x4(const float* left, const float* right) noexcept;
void quaternionToMatrix3x4(const float* quaternionAndTranslation, float* matrix) noexcept;

void copyDrawableDescriptor(void* destination, const void* source) noexcept;
void initializeDrawableDescriptor(void* descriptor) noexcept;
bool compatibleRenderFlags(std::uint32_t left, std::uint32_t right) noexcept;
void copyFloatWords13(void* destination, const void* source) noexcept;
void copyFloatWords15(void* destination, const void* source) noexcept;

void* renderTreeLowerBound(const void* tree, std::int32_t key) noexcept;
void* renderTreeUpperBound(const void* tree, std::int32_t key) noexcept;
void copyRenderState201(void* destination, const void* source) noexcept;
void interpolateQuaternion(
    float* output, const float* first, const float* second, float fraction) noexcept;
void copyRenderRecord68(void* destination, const void* source) noexcept;
void* copyRenderRecords68(
    const void* first, const void* last, void* destination) noexcept;
void* copyRenderRecords68Backward(
    const void* first, const void* last, void* destinationLast) noexcept;
bool hasExtendedRenderFlags(std::uint32_t flags) noexcept;

void buildDrawableTransform(
    float* matrix, const float* origin, const void* drawableData,
    const float* target, const float* offset) noexcept;
void emitSegmentQuad(
    void*& cursor, const float* first, const float* second,
    float width, std::uint32_t attribute) noexcept;

}  // namespace ra3::render
