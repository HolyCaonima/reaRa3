#pragma once

#include <cstdint>

namespace ra3::gameclient {

// View camera cluster from ra3_1.0.game, image range 0x00520000-0x0053FFFF.
// Primary vtable 0x00BE0440. Slot 2 (0x00529C20) is `mov eax, 0x00BE0658; ret`
// and 0x00BE0658 is the C string "View". Counted methods are non-getter slots
// of that table, plus 0x00529C60 which slot 5 tail-calls.
//
// Field notes (offsets are the ones the instructions use):
//   +0x0C/+0x10/+0x14 position, +0x18/+0x1C viewport size, +0x20/+0x24 origin,
//   +0x28/+0x2C/+0x30 angle triple, +0x44 fov, +0x6C/+0x70 zoom history,
//   +0x8C/+0x90/+0x94 guard point, +0x98 shake, +0x9C/+0xA4 clip planes.

void __fastcall View_resetCameraDefaults(void* self, void* edx) noexcept;          // 0x00529C60
void __fastcall View_initDefaultViewport(void* self, void* edx) noexcept;          // 0x00531330 slot 5

std::uint32_t __fastcall View_setLockByte(void* self, void* edx, std::uint32_t value) noexcept; // 0x00529820 slot 8
std::uint32_t __fastcall View_setViewportWidth(void* self, void* edx, std::uint32_t width) noexcept;   // 0x00529830 slot 10
std::uint32_t __fastcall View_setViewportHeight(void* self, void* edx, std::uint32_t height) noexcept; // 0x00529840 slot 12
std::uint32_t __fastcall View_setViewportOrigin(void* self, void* edx, std::uint32_t x, std::uint32_t y) noexcept; // 0x00529850 slot 14
std::uint32_t __fastcall View_getViewportOrigin(void* self, void* edx, std::uint32_t* x, std::uint32_t* y) noexcept; // 0x00529870 slot 15

std::uint32_t __fastcall View_setPositionFromScreen(void* self, void* edx, const void* point) noexcept; // 0x00529D40 slot 18
std::uint32_t __fastcall View_addPositionXY(void* self, void* edx, const void* point) noexcept;         // 0x00529DB0 slot 20
std::uint32_t __fastcall View_setPositionFromScreenIf(void* self, void* edx, const void* point,
    std::uint32_t, std::uint32_t, std::uint32_t, std::uint32_t, std::uint32_t) noexcept;               // 0x00529890 slot 21

void __fastcall View_setAngleClamped(void* self, void* edx, float angle) noexcept;  // 0x005225F0 slot 50
float __fastcall View_getAngle(void* self, void* edx) noexcept;                     // 0x005298E0 slot 51
float __fastcall View_getPitch(void* self, void* edx) noexcept;                     // 0x005298F0 slot 53
void __fastcall View_copyAnglePitchDefaults(void* self, void* edx) noexcept;        // 0x00522620 slot 54
std::uint8_t* __fastcall View_copyPosition(void* self, void* edx, std::uint8_t* dest) noexcept; // 0x00529900 slot 55

float __fastcall View_getZoom(void* self, void* edx) noexcept;                      // 0x00529920 slot 59
float __fastcall View_getFov(void* self, void* edx) noexcept;                       // 0x00529930 slot 61
void __fastcall View_setFov(void* self, void* edx, float fov) noexcept;             // 0x00529940 slot 62
void __fastcall View_scaleFovDown(void* self, void* edx) noexcept;                  // 0x00522580 slot 63
void __fastcall View_scaleFovUp(void* self, void* edx) noexcept;                    // 0x005225B0 slot 64

std::uint32_t __fastcall View_setByte7D(void* self, void* edx, std::uint32_t value) noexcept; // 0x00529950 slot 67
float __fastcall View_getHeight(void* self, void* edx) noexcept;                    // 0x00529960 slot 68
void __fastcall View_setHeight(void* self, void* edx, float height) noexcept;       // 0x00529970 slot 69
float __fastcall View_getMaxHeight(void* self, void* edx) noexcept;                 // 0x00529980 slot 70
void __fastcall View_setMaxHeight(void* self, void* edx, float height) noexcept;    // 0x00529990 slot 71
void __fastcall View_setTerrainHeight(void* self, void* edx, float height) noexcept; // 0x005299A0 slot 72
float __fastcall View_getTerrainHeight(void* self, void* edx) noexcept;             // 0x005299B0 slot 73

std::uint8_t* __fastcall View_captureCamera(void* self, void* edx, std::uint8_t* dest) noexcept; // 0x00529DE0 slot 74
void __fastcall View_applyCamera(void* self, void* edx, const std::uint8_t* state) noexcept;     // 0x00522630 slot 75

std::uint32_t __fastcall View_clearFilter(void* self, void* edx, std::uint32_t id) noexcept; // 0x005299C0 slot 78
void __fastcall View_setFilterReady(void* self, void* edx) noexcept;                    // 0x005299E0 slot 79
std::uint32_t __fastcall View_setFilter(void* self, void* edx, std::uint32_t id, float value) noexcept; // 0x005299F0 slot 80

void __fastcall View_pushZoom(void* self, void* edx, float zoom) noexcept;          // 0x00522530 slot 81
float __fastcall View_getZoomCurrent(void* self, void* edx) noexcept;               // 0x00529A10 slot 82
std::uint32_t __fastcall View_getZoomMode(void* self, void* edx) noexcept;          // 0x00529A20 slot 83
std::uint32_t __fastcall View_setZoomMode(void* self, void* edx, std::uint32_t mode) noexcept; // 0x00529A30 slot 84
std::uint32_t __fastcall View_clearZoomFilter(void* self, void* edx) noexcept;      // 0x00529A40 slot 85
std::uint32_t __fastcall View_setCameraByte(void* self, void* edx, std::uint32_t value) noexcept; // 0x00529A60 slot 86

float __fastcall View_getExtraPitch(void* self, void* edx) noexcept;                // 0x00529A70 slot 89
void __fastcall View_setExtraPitch(void* self, void* edx, float pitch) noexcept;    // 0x00529A80 slot 90
std::uint32_t __fastcall View_getGuardFlag(void* self, void* edx) noexcept;         // 0x00529A90 slot 94
std::uint32_t __fastcall View_getGuardId(void* self, void* edx) noexcept;           // 0x00529AA0 slot 96
std::uint32_t __fastcall View_setGuardId(void* self, void* edx, std::uint32_t id) noexcept; // 0x00529AB0 slot 97
std::uint32_t __fastcall View_setGuardPosition(void* self, void* edx, std::uint32_t x, std::uint32_t y) noexcept; // 0x00529AC0 slot 99
std::uint32_t __fastcall View_clearGuardPosition(void* self, void* edx) noexcept;   // 0x00529AE0 slot 100
std::uint32_t __fastcall View_hasGuardPosition(void* self, void* edx) noexcept;     // 0x00529AF0 slot 101
std::uint8_t* __fastcall View_copyGuardPosition(void* self, void* edx, std::uint8_t* dest) noexcept; // 0x00529B10 slot 102

void __fastcall View_addShakeMagnitude(void* self, void* edx, float delta) noexcept; // 0x00529B40 slot 106
std::uint32_t __fastcall View_clearShake(void* self, void* edx) noexcept;           // 0x00529B80 slot 107
float __fastcall View_getShake(void* self, void* edx) noexcept;                     // 0x00529B90 slot 108
void __fastcall View_setShakeRange(void* self, void* edx, float low, float high) noexcept; // 0x00529BA0 slot 111

// st0 receives the 10-byte image of ST0. Both reload the rounded float32
// after fptan, so the image is the float32 extended back to 80 bits.
void __fastcall View_distanceFromFov(void* self, void* edx, float angle, std::uint8_t st0[10]) noexcept;       // 0x00529E90 slot 112
void __fastcall View_distanceFromHalfAngle(void* self, void* edx, float angle, std::uint8_t st0[10]) noexcept; // 0x00522750 slot 113

float __fastcall View_getNearClip(void* self, void* edx) noexcept;                  // 0x00529BC0 slot 118
float __fastcall View_getFarClip(void* self, void* edx) noexcept;                   // 0x00529BD0 slot 119
void __fastcall View_setNearClip(void* self, void* edx, float clip) noexcept;       // 0x00529BE0 slot 120
void __fastcall View_setFarClip(void* self, void* edx, float clip) noexcept;        // 0x00529C00 slot 121
std::uint8_t* __fastcall View_setViewId(void* self, void* edx, std::uint32_t id) noexcept; // 0x00522570 slot 131

}  // namespace ra3::gameclient
