#pragma once

#include <cstddef>
#include <cstdint>

namespace ra3::common {

// Callee results for the 0x00780000 ports. The shipped bodies call other
// functions; the tape stands in for those returns so the port and the
// Unicorn run see the same EAX / pointer / float sequence. Pointer words are
// blob offsets (0 is null). Trace records are 8 bytes: tag, payload.
struct CallTape {
    const std::uint32_t* words = nullptr;
    std::uint32_t count = 0;
    std::uint32_t index = 0;
    std::uint8_t* base = nullptr;
    std::uint8_t* trace = nullptr;
    std::uint32_t traceUsed = 0;
    // Stand-in for dword [0x00DF9D24], incremented by initMatrixRecord.
    std::uint32_t* matrixCounter = nullptr;
    // Stand-in for the object at [0x00DEBA64], read by scaleToFloat.
    std::uint8_t* rules = nullptr;
    // Stand-in for dword [0x00CB0910], consumed by scaleByClock.
    std::uint32_t clock = 0;
    // Float bits left in ST0 by 0x00788800 / 0x00994A00.
    std::uint32_t st0Bits = 0x3F800000u;

    std::uint32_t take() noexcept;
    std::uint8_t* takePtr() noexcept;
    void mark(std::uint32_t tag, std::uint32_t payload) noexcept;
};

CallTape& thing78Tape() noexcept;

inline constexpr std::uint32_t kRecordVtable = 0x00C284C8u;
inline constexpr std::uint32_t kTaggedVtable = 0x00C28498u;
inline constexpr std::uint32_t kOneBits = 0x3F800000u;
inline constexpr std::uint32_t kBoundBits = 0x461C4000u;
inline constexpr std::uint32_t kPiBits = 0x40490FDBu;
inline constexpr std::uint32_t kTauBits = 0x40C90FDBu;
inline constexpr std::uint32_t kNegPiBits = 0xC0490FDBu;
inline constexpr std::uint32_t kTwo32Bits = 0x4F800000u;

// 0x0078BEA0. One-dword key order. EAX is 0 when equal, else 1 when left < right.
std::uint32_t __stdcall keyLess(const std::uint32_t* left, const std::uint32_t* right) noexcept;

// 0x0078BEE0. Tree lower bound. Header is this+4, root is [this+0x0C], key at node+0x10.
void __thiscall treeLowerBound(std::uint8_t* self, std::uint8_t** out, const std::uint32_t* key) noexcept;

// 0x0078CC50. Same tree. Writes the header when the key is absent.
void __thiscall treeSeek(std::uint8_t* self, std::uint8_t** out, const std::uint32_t* key) noexcept;

// 0x0078E350. Returns node+0x14 when the key is present.
std::uint8_t* __thiscall mapValue(std::uint8_t* self, const std::uint32_t* key) noexcept;

// 0x0078D650. Found path writes the node and a 0 flag. Insert path is the tape's node.
std::uint8_t* __thiscall mapLower(
    std::uint8_t* self, std::uint8_t* outPair, const std::uint32_t* key, std::uint32_t flag) noexcept;

// 0x0078DBA0. Hint insert. The predecessor comes from the tape (0x009C9980).
std::uint8_t* __thiscall mapHint(
    std::uint8_t* self, std::uint8_t* hint, const std::uint32_t* key, std::uint8_t** out) noexcept;

// 0x0078BFB0. Copies the matrix column at +8/+0x18/+0x28 and normalizes it.
void __thiscall normalizeColumn(std::uint8_t* self, float* out) noexcept;

// 0x0078BA00. Multiplies a point by the 3x4 at this+8. The optional callee is skipped when null.
void __thiscall transformPoint(
    std::uint8_t* self, const float* point, std::uint32_t ignored, float* dest, std::uint8_t* adjust) noexcept;

// 0x0078B7A0. Writes vtable 0x00C284C8, increments the counter, and builds an identity when arg != 0.
std::uint8_t* __thiscall initMatrixRecord(std::uint8_t* self, std::uint32_t arg) noexcept;

// 0x0078B870. Caches fsincos(angle at +0x44) at +0x4C and returns that pointer.
float* __thiscall directionCache(std::uint8_t* self) noexcept;

// 0x0078B8E0. Same cache, then copies 12 bytes to out.
void __thiscall directionWrite(std::uint8_t* self, float* out) noexcept;

// 0x0078C500. Thing::setTransform. Copies 12 floats, refreshes the cached translation, stores the
// taped heading, and rejects |component| >= 10000 or a NaN before the slot +0x14 notify.
void __thiscall setTransform(std::uint8_t* self, const float* basis) noexcept;

// 0x0078C7C0. Replaces the basis. Slot +0x18 runs when any cached component is unchanged.
void __thiscall setBasis(std::uint8_t* self, const float* basis) noexcept;

// 0x0078CEE0. Writes the height into the Z translation and applies the bit-1 / bit-2 deltas.
void __thiscall setHeight(std::uint8_t* self, float height) noexcept;

// 0x0078D1E0. Writes a world position into the translation column and the cache.
void __thiscall setPosition(std::uint8_t* self, const float* position) noexcept;

// 0x0078C080. Bit 6 of [[this+4]+0xC0] selects the taped anchor; otherwise this is not taken.
// The angle is wrapped into (-pi, pi] and stored at +0x44.
void __thiscall setAnchor(std::uint8_t* self, float angle) noexcept;

// 0x00788800. Integer/float scale used by the dozer clock. ST0 is the scaled value.
// EAX is the rules mode word, which is 0 on the path these tests take.
std::uint32_t __thiscall scaleToFloat(
    std::uint8_t* self, std::uint8_t* divisor, std::uint8_t* query, std::uint32_t count,
    std::uint32_t flags) noexcept;

// 0x0078BDA0. Multiplies scaleToFloat's ST0 by the clock dword and converts with the shipped fistp.
std::int32_t __thiscall scaleByClock(
    std::uint8_t* self, std::uint32_t a, std::uint32_t b, std::uint32_t c, std::uint32_t d) noexcept;

// 0x0078B990. Height cache at +0x5C. ST0 is the cached height.
void __thiscall cachedHeight(std::uint8_t* self) noexcept;

// 0x007886C0. Scalar cache at +0x58. ST0 is that scalar.
void __thiscall cachedScalar(std::uint8_t* self) noexcept;

} // namespace ra3::common
