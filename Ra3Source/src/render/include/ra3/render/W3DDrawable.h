#pragma once

#include <cstdint>

namespace ra3::render {

struct PassMask {
    std::uint32_t countAndFlags;
    std::uint32_t words[3];
};

void setDrawableWord104(void* drawable, std::uint32_t value) noexcept;
void setDrawableWord108(void* drawable, std::uint32_t value) noexcept;
void setDrawableExtent(void* drawable, std::uint32_t first, std::uint32_t second) noexcept;
void setDrawableViewport(void* drawable, float left, float top, float right, float bottom) noexcept;
void disableDrawableMode(void* drawable) noexcept;
void configureDrawableMode(
    void* drawable, std::int32_t width, std::int32_t mode,
    std::int32_t height, std::int32_t secondaryMode) noexcept;
void enableDrawableMode2(void* drawable) noexcept;
void copyDrawableState68(void* destination, const void* source) noexcept;

void normalizedToPixel(
    float x, float y, std::int32_t& pixelX, std::int32_t& pixelY,
    std::int32_t width, std::int32_t height) noexcept;
void pixelToNormalized(
    std::int32_t pixelX, std::int32_t pixelY, float& x, float& y,
    std::int32_t width, std::int32_t height) noexcept;

void assignPassMask(PassMask& destination, const PassMask& source) noexcept;
bool equalPassMask(const PassMask& left, const PassMask& right) noexcept;
void assignDrawablePassMask(void* drawable, const PassMask& source) noexcept;

void setAnimationVisible(void* state, std::uint8_t visible) noexcept;
std::uint32_t consumeCachedProperty50(void* state) noexcept;
std::uint32_t refreshCachedProperty54(void* state) noexcept;

}  // namespace ra3::render
