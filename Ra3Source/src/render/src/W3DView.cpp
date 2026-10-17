#include <ra3/render/W3DView.h>

#include <algorithm>
#include <cmath>
#include <cstring>

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

int framesFor(int milliseconds, int frameMilliseconds, int minimum) noexcept
{
    const int frames = milliseconds / frameMilliseconds;
    return frames < minimum ? minimum : frames;
}

float clamp01(float value) noexcept
{
    if (value < 0.0F) {
        return 0.0F;
    }
    if (1.0F < value) {
        return 1.0F;
    }
    return value;
}

void setupScalarMovement(
    void* view, std::size_t framesOffset, std::size_t startOffset,
    std::size_t targetOffset, std::size_t easeOffset, std::size_t activeOffset,
    std::size_t currentOffset, float target,
    int milliseconds, int frameMilliseconds, float easeIn, float easeOut) noexcept
{
    const int frames = framesFor(milliseconds, frameMilliseconds, 1);
    store<int>(view, framesOffset, frames);
    store<int>(view, framesOffset + 4, 0);
    store<float>(view, startOffset, load<float>(view, currentOffset));
    store<float>(view, targetOffset, target);
    store<std::uint8_t>(view, activeOffset, 1);
    ParabolicEase ease{};
    configureEase(ease, easeIn, easeOut, static_cast<float>(milliseconds));
    std::memcpy(static_cast<std::byte*>(view) + easeOffset, &ease, sizeof(ease));
}

void updateScalarMovement(
    void* view, std::size_t framesOffset, std::size_t startOffset,
    std::size_t targetOffset, std::size_t easeOffset, std::size_t activeOffset,
    std::size_t currentOffset, float scale) noexcept
{
    const int current = load<int>(view, framesOffset + 4) + 1;
    const int frames = load<int>(view, framesOffset);
    store<int>(view, framesOffset + 4, current);
    if (current <= frames) {
        ParabolicEase ease{};
        std::memcpy(&ease, static_cast<const std::byte*>(view) + easeOffset, sizeof(ease));
        const float amount = evaluateEase(ease, static_cast<float>(current) /
                                                   static_cast<float>(frames));
        const float start = load<float>(view, startOffset);
        const float target = load<float>(view, targetOffset);
        store<float>(view, currentOffset, (target - start) * amount + start);
    }
    if (frames <= current) {
        store<std::uint8_t>(view, activeOffset, 0);
        store<float>(view, currentOffset, load<float>(view, targetOffset) * scale);
    }
}

}  // namespace

// ParabolicEase in Generals GameClient/ParabolicEase; RA3
// ra3_1.0.game 0x005ABC30 and 0x005ABCE0.
void configureEase(ParabolicEase& ease, float easeIn, float easeOut, float duration) noexcept
{
    if (0.0F < duration) {
        easeIn = easeIn <= 0.0F ? 0.0F : easeIn / duration;
        easeOut = easeOut <= 0.0F ? 0.0F : easeOut / duration;
    }
    ease.easeIn = clamp01(easeIn);
    ease.easeOutStart = clamp01(1.0F - easeOut);
    if (ease.easeOutStart < ease.easeIn) {
        ease.easeIn = ease.easeOutStart;
    }
}

float evaluateEase(const ParabolicEase& ease, float position) noexcept
{
    if (position < 0.0F) {
        position = 0.0F;
    } else if (1.0F < position) {
        position = 1.0F;
    }
    const float span = (ease.easeOutStart + 1.0F) - ease.easeIn;
    if (position < ease.easeIn) {
        return (position * position) / (ease.easeIn * span);
    }
    if (ease.easeOutStart < position) {
        const double out = ease.easeOutStart;
        const double p = position;
        return static_cast<float>(((out - ease.easeIn) * 2.0 +
             ((out * out + (p - out) * 2.0) - p * p) /
                 (1.0 - out) + ease.easeIn) / span);
    }
    const double delta = static_cast<double>(position) - ease.easeIn;
    return static_cast<float>((delta + delta + ease.easeIn) / span);
}

// The following offsets and operation order are from ra3_1.0.game. Structure
// names follow Generals W3DView.h/W3DView.cpp, but implementations are original.
float scaledHeight(const void* view) noexcept // 0x00562250
{
    return load<float>(view, 0xA8) * load<float>(view, 0x40);
}

void* guardBandBias(void* view) noexcept // 0x00562260
{
    return static_cast<std::byte*>(view) + 0x150;
}

void setWireframePending(void* view, std::uint8_t enabled) noexcept // 0x00562290
{
    store<std::uint8_t>(view, 0x351, enabled);
}

void clearFilter(void* view, std::uint32_t filter) noexcept // 0x005622A0
{
    store<std::uint32_t>(view, 0x5C, filter);
    store<std::uint32_t>(view, 0x68, 0);
    store<std::uint32_t>(view, 0x64, 0);
    store<std::uint8_t>(view, 0x4B9, 0);
}

void setFilter(void* view, std::uint32_t mode, std::uint32_t filter) noexcept // 0x005622C0
{
    store<std::uint32_t>(view, 0x64, mode);
    store<std::uint32_t>(view, 0x68, filter);
    if (mode != 2) {
        store<std::uint8_t>(view, 0x4B9, 1);
        store<std::uint32_t>(view, 0x2674, 0xBF800000U);
    }
}

bool cameraMovementFinished(const void* view) noexcept // 0x00562570
{
    return load<std::int32_t>(view, 0x2594) == 0 &&
           load<std::uint8_t>(view, 0x418) == 0 &&
           load<std::uint8_t>(view, 0x440) == 0 &&
           load<std::uint8_t>(view, 0x464) == 0 &&
           load<std::uint8_t>(view, 0x490) == 0 &&
           load<std::uint8_t>(view, 0x4B8) == 0 &&
           load<std::int32_t>(view, 0x5C) == 0 &&
           load<std::int32_t>(view, 0x60) == 0;
}

bool cameraLockEquals(const void* view, std::int32_t objectId) noexcept // 0x005625C0
{
    return load<std::int32_t>(view, 0x2610) == objectId;
}

void setCameraLockRaw(void* view, std::int32_t objectId) noexcept // state part of 0x005628C0
{
    store<std::int32_t>(view, 0x2610, objectId);
}

std::int32_t cameraLockRaw(const void* view) noexcept
{
    return load<std::int32_t>(view, 0x2610);
}

float cameraShakeX(const void* view) noexcept { return load<float>(view, 0x2728); }
float cameraShakeY(const void* view) noexcept { return load<float>(view, 0x272C); }
float cameraShakeIntensity(const void* view) noexcept { return load<float>(view, 0x2730); }
std::uint8_t cameraShakeActive(const void* view) noexcept { return load<std::uint8_t>(view, 0x2734); }

void clearCameraShake(void* view) noexcept // 0x00562950
{
    std::memset(static_cast<std::byte*>(view) + 0x2728, 0, 0x11);
}

void setCameraShakeFlag0(void* view, std::uint8_t value) noexcept { store<std::uint8_t>(view, 0x2737, value); }
void setCameraShakeFlag1(void* view, std::uint8_t value) noexcept { store<std::uint8_t>(view, 0x2736, value); }
void setCameraShakeFlag2(void* view, std::uint8_t value) noexcept { store<std::uint8_t>(view, 0x2735, value); }
void setCameraShakeFlag3(void* view, std::uint8_t value) noexcept { store<std::uint8_t>(view, 0x2738, value); }

bool sphereVisible(const void* view, const math::Vector3& center, float radius) noexcept // 0x0056B9E0
{
    const auto* first = load<const ViewPlane*>(view, 0x308);
    const auto* last = load<const ViewPlane*>(view, 0x30C);
    for (; first != last; ++first) {
        if (radius < first->x * center.x + first->y * center.y +
                         first->z * center.z - first->distance) {
            return false;
        }
    }
    return true;
}

void translateCameraSamples(void* view, const math::Vector3& position) noexcept // 0x0056C2B0
{
    if (load<std::uint8_t>(view, 0x418) != 0 || load<int>(view, 0x2594) != 1) {
        return;
    }
    const int current = load<int>(view, 0x252C);
    const math::Vector3 anchor{
        load<float>(view, 0x4E8 + current * 0x14),
        load<float>(view, 0x4EC + current * 0x14),
        load<float>(view, 0x4F0 + current * 0x14)};
    for (int index = 2; index <= current; ++index) {
        const std::size_t offset = 0x4E8 + static_cast<std::size_t>(index) * 0x14;
        store<float>(view, offset, load<float>(view, offset) + position.x - anchor.x);
        store<float>(view, offset + 4, load<float>(view, offset + 4) + position.y - anchor.y);
        store<float>(view, offset + 8, load<float>(view, offset + 8) + position.z - anchor.z);
    }
}

void beginZoom(void* view, float target, int milliseconds, int frameMilliseconds,
               float easeIn, float easeOut) noexcept
{
    setupScalarMovement(view, 0x444, 0x44C, 0x450, 0x454, 0x464, 0x40,
                        target, milliseconds, frameMilliseconds, easeIn, easeOut);
}

void beginPitch(void* view, float target, int milliseconds, int frameMilliseconds,
                float easeIn, float easeOut) noexcept
{
    setupScalarMovement(view, 0x468, 0x478, 0x47C, 0x480, 0x490, 0x74,
                        target, milliseconds, frameMilliseconds, easeIn, easeOut);
}

void beginHeight(void* view, float target, int milliseconds, int frameMilliseconds,
                 float easeIn, float easeOut) noexcept
{
    setupScalarMovement(view, 0x41C, 0x428, 0x42C, 0x430, 0x440, 0x78,
                        target, milliseconds, frameMilliseconds, easeIn, easeOut);
}

void beginAngle(void* view, float rotations, int milliseconds, int frameMilliseconds,
                float easeIn, float easeOut, bool freezeTime) noexcept
{
    const int frames = framesFor(milliseconds, frameMilliseconds, 1);
    store<int>(view, 0x3E8, frames);
    store<int>(view, 0x3EC, 0);
    store<float>(view, 0x408, load<float>(view, 0x28));
    store<float>(view, 0x40C, rotations * 6.28318548F + load<float>(view, 0x28));
    store<std::uint8_t>(view, 0x418, 1);
    store<std::uint8_t>(view, 0x2670, static_cast<std::uint8_t>(freezeTime));
    ParabolicEase ease{};
    configureEase(ease, easeIn, easeOut, static_cast<float>(milliseconds));
    std::memcpy(static_cast<std::byte*>(view) + 0x3F0, &ease, sizeof(ease));
}

void beginAngleAbsolute(void* view, float angle, int milliseconds, int holdMilliseconds,
                        int frameMilliseconds, float easeIn, float easeOut,
                        std::int32_t targetObjectId) noexcept
{
    store<std::uint8_t>(view, 0x404, 1);
    store<int>(view, 0x3F8, std::max(0, holdMilliseconds) / frameMilliseconds);
    const int frames = framesFor(milliseconds, frameMilliseconds, 1);
    store<int>(view, 0x3E8, frames);
    store<int>(view, 0x3EC, 0);
    store<float>(view, 0x408, angle);
    store<std::uint8_t>(view, 0x418, 1);
    store<std::uint32_t>(view, 0x3F0, load<std::uint32_t>(view, 0x2618));
    store<std::uint32_t>(view, 0x3F4, load<std::uint32_t>(view, 0x2618));
    store<std::int32_t>(view, 0x414, targetObjectId);
    ParabolicEase ease{};
    configureEase(ease, easeIn, easeOut, static_cast<float>(milliseconds));
    std::memcpy(static_cast<std::byte*>(view) + 0x3FC, &ease, sizeof(ease));
}

void updateZoom(void* view) noexcept
{
    updateScalarMovement(view, 0x444, 0x44C, 0x450, 0x454, 0x464, 0x40, 1.0F);
}
void updatePitch(void* view) noexcept
{
    updateScalarMovement(view, 0x468, 0x478, 0x47C, 0x480, 0x490, 0x74, 0.0174532924F);
}
void updateHeight(void* view) noexcept
{
    updateScalarMovement(view, 0x41C, 0x428, 0x42C, 0x430, 0x440, 0x78, 1.0F);
}
void updateAngle(void* view) noexcept
{
    updateScalarMovement(view, 0x494, 0x4A0, 0x4A4, 0x4A8, 0x4B8, 0x30, 1.0F);
}

}  // namespace ra3::render
