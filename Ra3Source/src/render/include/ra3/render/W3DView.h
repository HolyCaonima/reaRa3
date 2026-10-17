#pragma once

#include <cstddef>
#include <cstdint>

#include <ra3/math/Math.h>

namespace ra3::render {

// Layout-compatible descriptions of the camera-only state touched by this
// reconstruction. The complete RA3 W3DView is 0x2740+ bytes and remains owned
// by the game.
struct ParabolicEase {
    float easeIn;
    float easeOutStart;
};

struct ViewPlane {
    float x;
    float y;
    float z;
    float distance;
};

struct CameraMovementState {
    std::byte bytes[0x4BC];
};

static_assert(sizeof(ParabolicEase) == 8);
static_assert(sizeof(ViewPlane) == 16);
static_assert(sizeof(CameraMovementState) == 0x4BC);

void configureEase(ParabolicEase& ease, float easeIn, float easeOut, float duration) noexcept;
float evaluateEase(const ParabolicEase& ease, float position) noexcept;

float scaledHeight(const void* view) noexcept;
void* guardBandBias(void* view) noexcept;
void setWireframePending(void* view, std::uint8_t enabled) noexcept;
void clearFilter(void* view, std::uint32_t filter) noexcept;
void setFilter(void* view, std::uint32_t mode, std::uint32_t filter) noexcept;
bool cameraMovementFinished(const void* view) noexcept;
bool cameraLockEquals(const void* view, std::int32_t objectId) noexcept;
void setCameraLockRaw(void* view, std::int32_t objectId) noexcept;
std::int32_t cameraLockRaw(const void* view) noexcept;

float cameraShakeX(const void* view) noexcept;
float cameraShakeY(const void* view) noexcept;
float cameraShakeIntensity(const void* view) noexcept;
std::uint8_t cameraShakeActive(const void* view) noexcept;
void clearCameraShake(void* view) noexcept;
void setCameraShakeFlag0(void* view, std::uint8_t value) noexcept;
void setCameraShakeFlag1(void* view, std::uint8_t value) noexcept;
void setCameraShakeFlag2(void* view, std::uint8_t value) noexcept;
void setCameraShakeFlag3(void* view, std::uint8_t value) noexcept;

bool sphereVisible(const void* view, const math::Vector3& center, float radius) noexcept;
void translateCameraSamples(void* view, const math::Vector3& position) noexcept;

void beginZoom(void* view, float target, int milliseconds, int frameMilliseconds,
               float easeIn, float easeOut) noexcept;
void beginPitch(void* view, float target, int milliseconds, int frameMilliseconds,
                float easeIn, float easeOut) noexcept;
void beginHeight(void* view, float target, int milliseconds, int frameMilliseconds,
                 float easeIn, float easeOut) noexcept;
void beginAngle(void* view, float rotations, int milliseconds, int frameMilliseconds,
                float easeIn, float easeOut, bool freezeTime) noexcept;
void beginAngleAbsolute(void* view, float angle, int milliseconds, int holdMilliseconds,
                        int frameMilliseconds, float easeIn, float easeOut,
                        std::int32_t targetObjectId) noexcept;

void updateZoom(void* view) noexcept;
void updatePitch(void* view) noexcept;
void updateHeight(void* view) noexcept;
void updateAngle(void* view) noexcept;

}  // namespace ra3::render
