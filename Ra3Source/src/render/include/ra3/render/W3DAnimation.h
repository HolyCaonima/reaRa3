#pragma once

#include <cstddef>
#include <cstdint>

namespace ra3::render {

float terrainLineMaximum(const void* terrain, const float* first, const float* second) noexcept;
float terrainCellMaximum(const void* terrain, float x, float y) noexcept;
void terrainExtent(const void* terrain, float* extent) noexcept;
void terrainCorners(const void* terrain, float* corners) noexcept;

bool modelHasTextureSlots(const void* modelPointerSlot) noexcept;
std::int32_t chooseWeightedModel(
    const void* modelVector, std::int32_t excludedIndex, std::int32_t limit,
    std::int32_t randomValue) noexcept;
void* findConditionSubset260(void* state, const std::uint32_t* conditions) noexcept;
void* findConditionSubset2c(void* state, const std::uint32_t* conditions) noexcept;
bool modelConditionStateDifferent(const void* left, const void* right) noexcept;

void setDrawableSelectable(void* subobject, std::uint8_t selectable) noexcept;
void setAnimationFrame(void* state, float fraction) noexcept;
bool animationBlendActive(const void* state) noexcept;

void buildHermiteTangents(float* values) noexcept;
void evaluateHermite(const float* values, float* output, float fraction) noexcept;
void advanceTextureFrames(void* frameVector, float elapsed) noexcept;

}  // namespace ra3::render
