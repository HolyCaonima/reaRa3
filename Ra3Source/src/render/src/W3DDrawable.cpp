#include <ra3/render/W3DDrawable.h>

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

std::int32_t truncate(float value) noexcept
{
    return _mm_cvtt_ss2si(_mm_set_ss(value));
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

}  // namespace

// Original implementation; ra3_1.0.game 0x008E4610.
void setDrawableWord104(void* drawable, std::uint32_t value) noexcept
{
    store(drawable, 0x104, value);
}

// Original implementation; ra3_1.0.game 0x008E4620.
void setDrawableWord108(void* drawable, std::uint32_t value) noexcept
{
    store(drawable, 0x108, value);
}

// Original implementation; ra3_1.0.game 0x008E4680.
void setDrawableExtent(void* drawable, std::uint32_t first, std::uint32_t second) noexcept
{
    store(drawable, 0x77C, first);
    store(drawable, 0x780, second);
}

// Original implementation; ra3_1.0.game 0x008E46A0.
void setDrawableViewport(void* drawable, float left, float top, float right, float bottom) noexcept
{
    store(drawable, 0x60, left);
    store(drawable, 0xE0, left);
    store(drawable, 0x64, top);
    store(drawable, 0xE4, top);
    store(drawable, 0x68, right);
    store(drawable, 0xE8, right);
    store(drawable, 0x6C, bottom);
    store(drawable, 0xEC, bottom);
    if (load<std::uint32_t>(drawable, 0x76C) == 3U && right != 100000.0F) {
        store(drawable, 0xE0, left - 2.0F);
        store(drawable, 0xE8, right + 2.0F);
        store(drawable, 0xE4, top - 2.0F);
        store(drawable, 0xEC, bottom + 2.0F);
    }
    store<std::uint8_t>(drawable, 0x778, 1);
    store<std::uint8_t>(drawable, 0x779, 1);
    store<std::uint8_t>(drawable, 0x77A, 1);
}

// Original implementation; ra3_1.0.game 0x008E4820.
void disableDrawableMode(void* drawable) noexcept
{
    store<std::uint32_t>(drawable, 0x768, 0);
    store<std::uint8_t>(drawable, 0x778, 1);
    store<std::uint8_t>(drawable, 0x77A, 1);
}

// Original implementation; ra3_1.0.game 0x008E4840.
void configureDrawableMode(
    void* drawable, std::int32_t width, std::int32_t mode,
    std::int32_t height, std::int32_t secondaryMode) noexcept
{
    store(drawable, 0x770, static_cast<float>(width));
    store<std::uint32_t>(drawable, 0x768, 1);
    store(drawable, 0x774, static_cast<float>(height));
    const auto primary = mode >= 1 && mode <= 3 ? mode : 0;
    const auto secondary = secondaryMode >= 1 && secondaryMode <= 2 ? secondaryMode : 0;
    store(drawable, 0x358, primary);
    store(drawable, 0x684, primary);
    store<std::uint8_t>(drawable, 0x778, 1);
    store<std::uint8_t>(drawable, 0x77A, 1);
    store(drawable, 0x35C, secondary);
    store(drawable, 0x688, secondary);
}

// Original implementation; ra3_1.0.game 0x008E4910.
void enableDrawableMode2(void* drawable) noexcept
{
    store<std::uint32_t>(drawable, 0x768, 2);
    store<std::uint8_t>(drawable, 0x778, 1);
    store<std::uint8_t>(drawable, 0x77A, 1);
}

// Original implementation; ra3_1.0.game 0x008E4950.
void copyDrawableState68(void* destination, const void* source) noexcept
{
    copyFloatX87(destination, source);
    store(
        destination, 4,
        static_cast<std::uint32_t>(load<std::uint32_t>(source, 4) != 0));
    for (std::size_t offset = 8; offset != 0x44; offset += 4) {
        copyFloatX87(
            static_cast<std::byte*>(destination) + offset,
            static_cast<const std::byte*>(source) + offset);
    }
}

// Original implementation; ra3_1.0.game 0x008E49E0.
void normalizedToPixel(
    float x, float y, std::int32_t& pixelX, std::int32_t& pixelY,
    std::int32_t width, std::int32_t height) noexcept
{
    pixelX = truncate((x + 1.0F) * static_cast<float>(width) * 0.5F);
    pixelY = truncate((1.0F - y) * static_cast<float>(height) * 0.5F);
}

// Original implementation; ra3_1.0.game 0x008E4A40.
void pixelToNormalized(
    std::int32_t pixelX, std::int32_t pixelY, float& x, float& y,
    std::int32_t width, std::int32_t height) noexcept
{
    x = static_cast<float>(pixelX) * 2.0F / static_cast<float>(width) - 1.0F;
    y = -(static_cast<float>(pixelY) * 2.0F / static_cast<float>(height) - 1.0F);
}

// Original implementation; ra3_1.0.game 0x008E4AA0.
void assignPassMask(PassMask& destination, const PassMask& source) noexcept
{
    if (&destination == &source) {
        return;
    }
    destination.countAndFlags ^= (source.countAndFlags ^ destination.countAndFlags) & 7U;
    destination.countAndFlags =
        ((source.countAndFlags ^ destination.countAndFlags) & 7U) ^ source.countAndFlags;
    for (std::size_t index = 0; index != 3; ++index) {
        destination.words[index] = source.words[index];
    }
}

// Original implementation; ra3_1.0.game 0x008E4AE0.
bool equalPassMask(const PassMask& left, const PassMask& right) noexcept
{
    const auto count = left.countAndFlags & 7U;
    if (count != (right.countAndFlags & 7U)) {
        return false;
    }
    for (std::uint32_t index = 0; index != count; ++index) {
        if (left.words[index] != right.words[index]) {
            return false;
        }
    }
    return true;
}

// Original implementation; ra3_1.0.game 0x008E4B30.
void assignDrawablePassMask(void* drawable, const PassMask& source) noexcept
{
    assignPassMask(*reinterpret_cast<PassMask*>(static_cast<std::byte*>(drawable) + 0xD8), source);
}

// Original implementation; ra3_1.0.game 0x008E56B0.
void setAnimationVisible(void* state, std::uint8_t visible) noexcept
{
    const auto animation = load<std::uint32_t>(state, 0x74);
    if (animation != 0) {
        const auto suppressed = load<std::uint8_t>(state, 0x6B) != 0;
        store<std::uint8_t>(
            reinterpret_cast<void*>(static_cast<std::uintptr_t>(animation)), 4,
            static_cast<std::uint8_t>(visible != 0 && !suppressed));
    }
    store(state, 0x6A, visible);
}

// Original implementation; ra3_1.0.game 0x008E5710.
std::uint32_t consumeCachedProperty50(void* state) noexcept
{
    if (load<std::uint8_t>(state, 0x251) != 0) {
        store<std::uint8_t>(state, 0x251, 0);
        const auto object = load<std::uint32_t>(state, 0x3C);
        if (object != 0) {
            const auto table = load<std::uint32_t>(
                reinterpret_cast<const void*>(static_cast<std::uintptr_t>(object)), 0);
            const auto value = load<std::uint32_t>(
                reinterpret_cast<const void*>(static_cast<std::uintptr_t>(table)), 0x50);
            store(state, 0x254, value);
            return value;
        }
    }
    return load<std::uint32_t>(state, 0x254);
}

// Original implementation; ra3_1.0.game 0x008E5740.
std::uint32_t refreshCachedProperty54(void* state) noexcept
{
    const auto object = load<std::uint32_t>(state, 0x3C);
    if (object != 0) {
        const auto table = load<std::uint32_t>(
            reinterpret_cast<const void*>(static_cast<std::uintptr_t>(object)), 0);
        const auto value = load<std::uint32_t>(
            reinterpret_cast<const void*>(static_cast<std::uintptr_t>(table)), 0x54);
        store(state, 0x258, value);
        return value;
    }
    return load<std::uint32_t>(state, 0x258);
}

}  // namespace ra3::render
