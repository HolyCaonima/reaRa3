#pragma once

#include <cstddef>
#include <cstdint>

#include <ra3/math/Math.h>

namespace ra3::render {

struct RainVertex {
    math::Vector3 position;
    std::uint32_t color;
    float u;
    float v;
};

struct RainDropQuad {
    RainVertex vertices[4];
};

struct RainVisibilityPlane {
    float x;
    float y;
    float z;
    float distance;
};

struct RainSegment {
    std::byte prefix[0x14];
    std::uint32_t enabled;
    std::uint32_t visibility;
    math::Vector3 center;
    float radius;
    std::byte suffix[0x24];
};

static_assert(sizeof(RainVertex) == 0x18);
static_assert(sizeof(RainDropQuad) == 0x60);
static_assert(sizeof(RainVisibilityPlane) == 0x10);
static_assert(sizeof(RainSegment) == 0x50);

RainDropQuad makeRainDrop(float x, float y, float z, std::uint32_t color) noexcept;
void fillRainDropVertices(RainDropQuad* output, std::size_t count,
                          const float* randomXyz, const std::uint32_t* colors) noexcept;
void fillRainIndices(std::uint16_t* output, std::size_t count) noexcept;
void setRainEnabled(RainSegment* first, RainSegment* last, bool enabled) noexcept;
bool sphereInsidePlanes(const math::Vector3& center, float radius,
                        const RainVisibilityPlane* first,
                        const RainVisibilityPlane* last) noexcept;
void updateRainVisibility(RainSegment* first, RainSegment* last,
                          const RainVisibilityPlane* primaryFirst,
                          const RainVisibilityPlane* primaryLast,
                          const RainVisibilityPlane* secondaryFirst,
                          const RainVisibilityPlane* secondaryLast,
                          bool useSecondary) noexcept;

}  // namespace ra3::render
