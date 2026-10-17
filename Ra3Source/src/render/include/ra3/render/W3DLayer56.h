#pragma once

#include <cstdint>
#include <string>

namespace ra3::render {

// Globals stand in for ra3_1.0.game .data cells the reconstructed bodies read.
// The checker and the Unicorn oracle plant the same values before each call.
void w3d56ResetProbe() noexcept;
void w3d56SetFrameMillis(std::int32_t frameMillis) noexcept;
void w3d56SetHalfPiBits(std::uint32_t bits) noexcept;
void w3d56SetPauseByte(std::uint8_t value) noexcept;
void w3d56SetLatch(std::uint32_t value) noexcept;
void w3d56SetListenerFlag(std::uint32_t value) noexcept;
void w3d56SetFollow(void* object) noexcept;
void w3d56SetTerrain(void* object) noexcept;
void w3d56SetSampleOwner(void* object) noexcept;
void w3d56SetQueryResult(std::uint32_t value) noexcept;
void w3d56SetFinishResult(std::uint32_t value) noexcept;
void w3d56SetHeightBits(std::uint32_t bits) noexcept;
void w3d56PrepareXfer(void* xfer, std::uint32_t version) noexcept;
std::uint8_t w3d56PauseByte() noexcept;
std::uint32_t w3d56Latch() noexcept;
std::uint32_t w3d56ListenerFlag() noexcept;
std::uint32_t w3d56ListenerBits(int index) noexcept;
std::string w3d56TakeLog();
void w3d56SetControlWord() noexcept;

// 0x005ABCE0 bit-exact fstp m32, used by the camera updates in this batch.
std::uint32_t w3d56EvaluateEaseBits(const void* ease, float position) noexcept;

// View vtable 0x00BE55C8 (slot 2 returns "View").
void view56BeginTrackedMove(void* view, std::uint32_t deltaBits, std::uint32_t milliseconds,
                             std::uint32_t flagWord, std::uint32_t easeInBits, std::uint32_t easeOutBits) noexcept;
void view56BeginTrackedMoveHeld(void* view, std::uint32_t anchorBits, std::uint32_t milliseconds,
                                std::uint32_t holdMilliseconds, std::uint32_t easeInBits,
                                std::uint32_t easeOutBits, std::uint32_t extraBits) noexcept;
void view56UpdateZoom(void* view) noexcept;
void view56UpdatePitch(void* view) noexcept;
void view56UpdateHeight(void* view) noexcept;
void view56UpdateAngle(void* view) noexcept;
void view56SetPauseFlag(void* view, std::uint32_t enabled) noexcept;
std::uint32_t view56SphereVisible(const void* view, const void* center, std::uint32_t radiusBits) noexcept;
std::uint32_t view56PublishListener(void* view) noexcept;
std::uint32_t view56FollowHeightRatio(const void* view, std::uint32_t numeratorBits) noexcept;
void view56BeginZoom(void* view, std::uint32_t targetBits, std::uint32_t milliseconds,
                     std::uint32_t easeInBits, std::uint32_t easeOutBits) noexcept;
void view56BeginHeight(void* view, std::uint32_t targetBits, std::uint32_t milliseconds,
                       std::uint32_t easeInBits, std::uint32_t easeOutBits) noexcept;
void view56BeginAngle(void* view, std::uint32_t targetBits, std::uint32_t milliseconds,
                      std::uint32_t easeInBits, std::uint32_t easeOutBits) noexcept;
void view56TranslateSamples(void* view, const void* position) noexcept;
void view56ClearCameraMotion(void* view) noexcept;

// Embedded camera at View+0x26FC, secondary vtable 0x00BE3D98.
// 0x00594A80 stores 0x00BE55C8 at +0 and 0x00BE3D98 at +0x26FC.
std::uint32_t view56EmbeddedFlagFloat(const void* embedded) noexcept;
void view56EmbeddedSet4(void* embedded, std::uint32_t bits) noexcept;
void view56EmbeddedSet8(void* embedded, std::uint32_t bits) noexcept;
void view56EmbeddedSet12(void* embedded, std::uint32_t bits) noexcept;
void view56EmbeddedSet16(void* embedded, std::uint32_t bits) noexcept;

// 0x00BE3ED8. Slot 2 is not a name getter. 0x005833E0 stores this vtable.
std::uint32_t bounds56Copy(const void* self, void* out) noexcept;
void bounds56Assign(void* self, std::uint32_t enabled, const void* rect) noexcept;
void bounds56Intersect(void* self, const void* rect) noexcept;

// W3DTerrainLogic vtable 0x00BE39F8 (slot 2 returns "W3DTerrainLogic").
void terrain56FillExtent(const void* self, void* out) noexcept;
void terrain56HeightOrZero(void* self, std::uint32_t xBits, std::uint32_t yBits,
                           std::uint32_t* eaxOut, std::uint32_t* stOut) noexcept;
void terrain56SyncCell(void* self, std::uint32_t cell) noexcept;
void terrain56XferHeader(void* self, void* xfer) noexcept;

// W3DTerrainVisual vtable 0x00BE38E8 (slot 2 returns "W3DTerrainVisual").
void terrain56ForwardA(void* self, std::uint32_t a0, std::uint32_t a1Bits, std::uint32_t a2) noexcept;
void terrain56ForwardB(void* self, std::uint32_t a0, std::uint32_t a1, std::uint32_t a2Bits) noexcept;
void terrain56ForwardScorch(void* self, std::uint32_t a0, std::uint32_t a1Bits) noexcept;
std::uint32_t terrain56SampleIndex(void* self, std::uint32_t xBits, std::uint32_t yBits) noexcept;

// 0x00BE4BE0. Slot 2 is not a name getter. 0x0058C700 stores this vtable.
std::uint32_t client56Latch(void* self, std::uint32_t value, std::uint32_t ignored) noexcept;
void client56SetFlag(void* self, std::uint32_t value) noexcept;

// Callees of 0x009BA010 (0x00C61534 slot 4).
void record56WriteVec3(void* self, const void* src, std::uint32_t index) noexcept;
void record56WriteFloatUnlessEqual(void* self, const void* src, std::uint32_t index) noexcept;
void record56WriteVec4(void* self, const void* src, std::uint32_t index) noexcept;
void record56WritePair(void* self, const void* src, std::uint32_t index) noexcept;

// Callee of 0x00589390 (0x00BE4220 slot 0). cdecl, two pointers.
void mat56Transpose(void* dst, const void* src) noexcept;

// 0x00BE3B38 slot 17. 0x00565CA0 stores this vtable. Slot 2 is not a name getter.
void pose56CopyTranslation(const void* self, void* out) noexcept;

// TScorch vtable 0x00BE3CAC (slot 2 returns "TScorch").
void scorch56Xfer(void* self, void* xfer) noexcept;

// 0x00BE4214 slot 2. That slot is not a name getter.
void ease56ResetTrack(void* self, std::uint32_t fill, std::uint32_t frames, std::uint32_t easeInBits,
                      std::uint32_t easeOutBits, std::uint32_t tag, std::uint32_t ignored) noexcept;

}  // namespace ra3::render
