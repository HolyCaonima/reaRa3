#include <ra3/render/W3DRainManager.h>

namespace ra3::render {

// The quad layout and index order are from ra3_1.0.game 0x00568B90 and
// 0x00568E40 (W3DRainManager.cpp).
RainDropQuad makeRainDrop(float x, float y, float z, std::uint32_t color) noexcept
{
    RainDropQuad result{};
    result.vertices[0] = {{x, y, z}, color, 0.0F, 0.0F};
    result.vertices[1] = {{x, y, z}, color, 0.0F, 1.0F};
    result.vertices[2] = {{x, y, z}, color, 1.0F, 0.0F};
    result.vertices[3] = {{x, y, z}, color, 1.0F, 1.0F};
    return result;
}

void fillRainDropVertices(RainDropQuad* output, std::size_t count,
                          const float* randomXyz, const std::uint32_t* colors) noexcept
{
    for (std::size_t index = 0; index < count; ++index) {
        output[index] = makeRainDrop(randomXyz[index * 3], randomXyz[index * 3 + 1],
                                     randomXyz[index * 3 + 2], colors[index]);
    }
}

void fillRainIndices(std::uint16_t* output, std::size_t count) noexcept
{
    for (std::size_t index = 0; index < count; ++index) {
        const auto base = static_cast<std::uint16_t>(index * 4);
        output[index * 6] = base;
        output[index * 6 + 1] = static_cast<std::uint16_t>(base + 1);
        output[index * 6 + 2] = static_cast<std::uint16_t>(base + 2);
        output[index * 6 + 3] = static_cast<std::uint16_t>(base + 2);
        output[index * 6 + 4] = static_cast<std::uint16_t>(base + 1);
        output[index * 6 + 5] = static_cast<std::uint16_t>(base + 3);
    }
}

void setRainEnabled(RainSegment* first, RainSegment* last, bool enabled) noexcept
{
    const std::uint32_t value = enabled ? 1U : 0U;
    for (; first != last; ++first) {
        first->enabled = value;
    }
}

bool sphereInsidePlanes(const math::Vector3& center, float radius,
                        const RainVisibilityPlane* first,
                        const RainVisibilityPlane* last) noexcept
{
    for (; first != last; ++first) {
        if (radius < first->x * center.x + first->y * center.y +
                         first->z * center.z - first->distance) {
            return false;
        }
    }
    return true;
}

void updateRainVisibility(RainSegment* first, RainSegment* last,
                          const RainVisibilityPlane* primaryFirst,
                          const RainVisibilityPlane* primaryLast,
                          const RainVisibilityPlane* secondaryFirst,
                          const RainVisibilityPlane* secondaryLast,
                          bool useSecondary) noexcept
{
    for (; first != last; ++first) {
        std::uint32_t value = sphereInsidePlanes(first->center, first->radius,
                                                 primaryFirst, primaryLast) ? 1U : 0U;
        if (useSecondary &&
            sphereInsidePlanes(first->center, first->radius, secondaryFirst, secondaryLast)) {
            value |= 2U;
        }
        first->visibility = value;
    }
}

}  // namespace ra3::render
