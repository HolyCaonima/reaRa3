#include "ra3/render/W3DTerrainCluster56.h"

#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstring>

namespace ra3::render {

namespace {

inline std::uint32_t readU32(const void* base, std::ptrdiff_t offset) {
    std::uint32_t v = 0;
    std::memcpy(&v, static_cast<const std::uint8_t*>(base) + offset, sizeof(v));
    return v;
}

inline std::int32_t readI32(const void* base, std::ptrdiff_t offset) {
    std::int32_t v = 0;
    std::memcpy(&v, static_cast<const std::uint8_t*>(base) + offset, sizeof(v));
    return v;
}

inline std::uint16_t readU16(const void* base, std::ptrdiff_t offset) {
    std::uint16_t v = 0;
    std::memcpy(&v, static_cast<const std::uint8_t*>(base) + offset, sizeof(v));
    return v;
}

inline std::int16_t readI16(const void* base, std::ptrdiff_t offset) {
    std::int16_t v = 0;
    std::memcpy(&v, static_cast<const std::uint8_t*>(base) + offset, sizeof(v));
    return v;
}

inline std::uint8_t readU8(const void* base, std::ptrdiff_t offset) {
    return *(static_cast<const std::uint8_t*>(base) + offset);
}

inline float readF32(const void* base, std::ptrdiff_t offset) {
    float v = 0.0f;
    std::memcpy(&v, static_cast<const std::uint8_t*>(base) + offset, sizeof(v));
    return v;
}

inline void writeU32(void* base, std::ptrdiff_t offset, std::uint32_t v) {
    std::memcpy(static_cast<std::uint8_t*>(base) + offset, &v, sizeof(v));
}

inline void writeI32(void* base, std::ptrdiff_t offset, std::int32_t v) {
    std::memcpy(static_cast<std::uint8_t*>(base) + offset, &v, sizeof(v));
}

inline void writeU16(void* base, std::ptrdiff_t offset, std::uint16_t v) {
    std::memcpy(static_cast<std::uint8_t*>(base) + offset, &v, sizeof(v));
}

inline void writeU8(void* base, std::ptrdiff_t offset, std::uint8_t v) {
    *(static_cast<std::uint8_t*>(base) + offset) = v;
}

inline void writeF32(void* base, std::ptrdiff_t offset, float v) {
    std::memcpy(static_cast<std::uint8_t*>(base) + offset, &v, sizeof(v));
}

inline const void* readPtr(const void* base, std::ptrdiff_t offset) {
    const void* p = nullptr;
    std::memcpy(&p, static_cast<const std::uint8_t*>(base) + offset, sizeof(p));
    return p;
}

inline void* readMutPtr(void* base, std::ptrdiff_t offset) {
    void* p = nullptr;
    std::memcpy(&p, static_cast<std::uint8_t*>(base) + offset, sizeof(p));
    return p;
}

// Emulates x86 `fld dword ptr [src]; fstp dword ptr [dst]`:
// SNaN (exponent 0xFF, bit 22 == 0, mantissa != 0) is quieted (bit 22 set to 1) by x87 `fld m32fp`.
inline std::uint32_t x87CopyBits(std::uint32_t bits) {
    if ((bits & 0x7F800000u) == 0x7F800000u && (bits & 0x007FFFFFu) != 0u) {
        bits |= 0x00400000u;
    }
    return bits;
}

inline void x87CopyDword(void* dst, std::ptrdiff_t dstOff, const void* src, std::ptrdiff_t srcOff) {
    writeU32(dst, dstOff, x87CopyBits(readU32(src, srcOff)));
}

inline std::uint32_t floatToBits(float f) {
    std::uint32_t u = 0;
    std::memcpy(&u, &f, sizeof(u));
    return u;
}

inline float bitsToFloat(std::uint32_t u) {
    float f = 0.0f;
    std::memcpy(&f, &u, sizeof(f));
    return f;
}

} // namespace

// ============================================================================
// 1. W3DTerrainVisual & ray/water helpers
// ============================================================================

// 0x00561d70 — 0x00be38e8 slot 9
std::uint32_t queryTerrainVisualSubobjectSlot6(void* terrainVisual, float x, float y) {
    const void* vtable = readPtr(terrainVisual, 0x00);
    using Slot10Fn = void* (__thiscall*)(void*, float, float);
    const auto fn = reinterpret_cast<Slot10Fn>(const_cast<void*>(readPtr(vtable, 0x28)));
    void* subobj = fn(terrainVisual, x, y);
    if (subobj != nullptr) {
        // Calls 0x00917a50: mov eax, [ecx]; mov eax, [eax+0x18]; ret
        const void* subVt = readPtr(subobj, 0x00);
        return readU32(subVt, 0x18);
    }
    return 0u;
}

// 0x00561db0 — 0x00be38e8 slot 20
std::int32_t getRawMapHeight(const void* terrainVisual, std::int32_t x, std::int32_t y) {
    const void* hm = readPtr(terrainVisual, 0x30);
    if (hm == nullptr) {
        return 0;
    }
    const std::int32_t border = readI32(hm, 0x2c);
    const std::int32_t width = readI32(hm, 0x24);
    const std::uint32_t idx = static_cast<std::uint32_t>((y + border) * width + border + x);
    if (idx >= readU32(hm, 0x40)) {
        return 0;
    }
    const void* data = readPtr(hm, 0x44);
    if (data == nullptr) {
        return 0;
    }
    const std::uint16_t rawSample = readU16(data, static_cast<std::ptrdiff_t>(idx) * 2);
    // cvtsi2ss xmm0, ecx; mulss xmm0, [0x00be1550] (0.0390625f); addss xmm0, [0x00bd9528] (0.5f); cvttss2si eax, xmm0
    const float scaled = static_cast<float>(static_cast<std::int32_t>(rawSample)) * 0.0390625f + 0.5f;
    return static_cast<std::int32_t>(scaled);
}

// 0x00561e10 — 0x00be38e8 slot 21
void partialMapUpdate(void* terrainVisual, const void* region2D) {
    void* renderObj = readMutPtr(terrainVisual, 0x28);
    if (renderObj == nullptr) {
        return;
    }
    const void* vtable = readPtr(renderObj, 0x00);
    using UpdateFn = void (__thiscall*)(void*, const std::int32_t*);
    const auto fn = reinterpret_cast<UpdateFn>(const_cast<void*>(readPtr(vtable, 0x28)));
    if (region2D != nullptr) {
        std::int32_t expanded[4];
        expanded[0] = readI32(region2D, 0x00) - 1;
        expanded[1] = readI32(region2D, 0x04) - 1;
        expanded[2] = readI32(region2D, 0x08) + 1;
        expanded[3] = readI32(region2D, 0x0c) + 1;
        fn(renderObj, expanded);
    } else {
        fn(renderObj, nullptr);
    }
}

// 0x00561e70 — 0x00be38e8 slot 23
std::int32_t getTerrainVisualBorderSize(const void* terrainVisual) {
    const void* hm = readPtr(terrainVisual, 0x30);
    if (hm != nullptr) {
        return readI32(hm, 0x2c);
    }
    return 0;
}

// 0x00561e80 — 0x00be38e8 slot 18
void removeAllBibs(void* terrainVisual) {
    void* renderObj = readMutPtr(terrainVisual, 0x28);
    if (renderObj != nullptr) {
        const void* vtable = readPtr(renderObj, 0x00);
        using RemoveBibsFn = void (__thiscall*)(void*);
        const auto fn = reinterpret_cast<RemoveBibsFn>(const_cast<void*>(readPtr(vtable, 0x4c)));
        fn(renderObj);
    }
}

// 0x0056b290 — 0x00be38e8 slot 8
void getTerrainVisualColorAt(const void* terrainVisual, float x, float y, void* outColor) {
    const void* hm = readPtr(terrainVisual, 0x30);
    if (hm != nullptr) {
        getTerrainColorAt(hm, x, y, outColor);
    }
}

// 0x0056b2f0 — 0x00be38e8 slot 19
std::uint32_t setRawMapHeightAndFlags(
    void* terrainVisual,
    std::int32_t x,
    std::int32_t y,
    std::int32_t heightValue,
    std::uint32_t flagWord) {
    void* hm = readMutPtr(terrainVisual, 0x30);
    if (hm == nullptr) {
        return 0u;
    }
    const std::int32_t border = readI32(hm, 0x2c);
    const std::int32_t adjX = x + border;
    const std::int32_t adjY = y + border;
    const std::uint8_t flagByte = static_cast<std::uint8_t>(flagWord & 0xFFu);
    const std::uint32_t dataSize = readU32(hm, 0x40);
    const std::int32_t width = readI32(hm, 0x24);
    const std::uint32_t idx = static_cast<std::uint32_t>(adjY * width + adjX);

    if (flagByte == 0u) {
        std::uint16_t curSample = 0u;
        if (idx < dataSize) {
            const void* heights = readPtr(hm, 0x44);
            curSample = readU16(heights, static_cast<std::ptrdiff_t>(idx) * 2);
        }
        if (static_cast<std::int32_t>(curSample) <= heightValue) {
            return static_cast<std::uint32_t>(curSample) & 0xFFFFFF00u;
        }
    }

    if (idx < dataSize) {
        void* heights = readMutPtr(hm, 0x44);
        if (heights != nullptr) {
            writeU16(heights, static_cast<std::ptrdiff_t>(idx) * 2, static_cast<std::uint16_t>(heightValue & 0xFFFF));
        }
    }

    void* hmReload = readMutPtr(terrainVisual, 0x30);
    const std::uint32_t idx2 = static_cast<std::uint32_t>(adjY * readI32(hmReload, 0x24) + adjX);
    void* flags = readMutPtr(hmReload, 0x48);
    if (flags != nullptr && idx2 < readU32(hmReload, 0x40)) {
        writeU8(flags, static_cast<std::ptrdiff_t>(idx2), flagByte);
    }
    return (idx2 & 0xFFFFFF00u) | 1u;
}

// 0x0056b380 — 0x00be38e8 slot 22
std::uint32_t getCellFlags(const void* terrainVisual, std::int32_t x, std::int32_t y) {
    const void* hm = readPtr(terrainVisual, 0x30);
    const std::int32_t border = readI32(hm, 0x2c);
    const std::int32_t width = readI32(hm, 0x24);
    const std::uint32_t idx = static_cast<std::uint32_t>((y + border) * width + border + x);
    const std::uint32_t eaxUpper = idx & 0xFFFFFF00u;
    const void* flags = readPtr(hm, 0x48);
    if (flags == nullptr || idx >= readU32(hm, 0x40)) {
        return eaxUpper;
    }
    return eaxUpper | readU8(flags, static_cast<std::ptrdiff_t>(idx));
}

// 0x00570d50 — LineSegClass::recalculate()
void recalculateLineSeg(void* lineSeg) {
    const float p0x = readF32(lineSeg, 0x00);
    const float p0y = readF32(lineSeg, 0x04);
    const float p0z = readF32(lineSeg, 0x08);
    const float p1x = readF32(lineSeg, 0x0c);
    const float p1y = readF32(lineSeg, 0x10);
    const float p1z = readF32(lineSeg, 0x14);

    const float dpx = p1x - p0x;
    const float dpy = p1y - p0y;
    const float dpz = p1z - p0z;

    writeF32(lineSeg, 0x18, dpx);
    writeF32(lineSeg, 0x1c, dpy);
    writeF32(lineSeg, 0x20, dpz);

    // Copies DP (+0x18..+0x20) to Dir (+0x24..+0x2c) via x87 fld/fstp
    x87CopyDword(lineSeg, 0x24, lineSeg, 0x18);
    x87CopyDword(lineSeg, 0x28, lineSeg, 0x1c);
    x87CopyDword(lineSeg, 0x2c, lineSeg, 0x20);

    // Length = float(sqrt(double(dpx*dpx + dpy*dpy + dpz*dpz)))
    const float lenSq = dpx * dpx + dpy * dpy + dpz * dpz;
    const float length = static_cast<float>(std::sqrt(static_cast<double>(lenSq)));
    writeF32(lineSeg, 0x30, length);

    // comiss xmm0, [0x00be2d70] (0x38d1b717 = 9.999999747378752e-05f)
    const float kEps = bitsToFloat(0x38d1b717u);
    if (length > kEps) {
        const float invLen = 1.0f / length;
        writeF32(lineSeg, 0x24, readF32(lineSeg, 0x24) * invLen);
        writeF32(lineSeg, 0x28, readF32(lineSeg, 0x28) * invLen);
        writeF32(lineSeg, 0x2c, readF32(lineSeg, 0x2c) * invLen);
    }
}

// 0x00572070 — RayCollisionTestClass::init
void* initRayCollisionTest(
    void* rayCollisionTest,
    const void* lineSeg,
    void* castResult,
    std::uint32_t collisionType,
    std::uint8_t flag3c,
    std::uint8_t flag3d) {
    writeU32(rayCollisionTest, 0x00, reinterpret_cast<std::uintptr_t>(castResult));
    writeU32(rayCollisionTest, 0x04, collisionType);
    for (std::ptrdiff_t off = 0; off <= 0x30; off += 4) {
        x87CopyDword(rayCollisionTest, 0x08 + off, lineSeg, off);
    }
    writeU8(rayCollisionTest, 0x3c, flag3c);
    writeU8(rayCollisionTest, 0x3d, flag3d);
    writeU8(rayCollisionTest, 0x3e, 0u);
    writeU8(rayCollisionTest, 0x3f, 0u);
    return rayCollisionTest;
}

// 0x00580690 — LineSegClass::set
void* setLineSegPoints(void* lineSeg, const void* p0, const void* p1) {
    x87CopyDword(lineSeg, 0x00, p0, 0x00);
    x87CopyDword(lineSeg, 0x04, p0, 0x04);
    x87CopyDword(lineSeg, 0x08, p0, 0x08);
    x87CopyDword(lineSeg, 0x0c, p1, 0x00);
    x87CopyDword(lineSeg, 0x10, p1, 0x04);
    x87CopyDword(lineSeg, 0x14, p1, 0x08);
    recalculateLineSeg(lineSeg);
    return lineSeg;
}

// 0x005882c0 — 0x00be38e8 slot 11: W3DTerrainVisual::intersectTerrain
std::uint32_t intersectTerrain(
    void* terrainVisual,
    const void* rayStart,
    const void* rayEnd,
    void* outHitPoint) {
    void* renderObj = readMutPtr(terrainVisual, 0x28);
    if (renderObj == nullptr) {
        return 0u;
    }

    // CastResultStruct on stack (0x24 bytes)
    std::uint8_t castResult[0x24];
    std::memset(castResult, 0, sizeof(castResult));
    writeF32(castResult, 0x04, 1.0f);          // Fraction = 1.0f

    // LineSegClass on stack (0x34 bytes)
    std::uint8_t lineSeg[0x34];
    writeU32(lineSeg, 0x00, readU32(rayStart, 0x00));
    writeU32(lineSeg, 0x04, readU32(rayStart, 0x04));
    writeU32(lineSeg, 0x08, readU32(rayStart, 0x08));
    writeU32(lineSeg, 0x0c, readU32(rayEnd, 0x00));
    writeU32(lineSeg, 0x10, readU32(rayEnd, 0x04));
    writeU32(lineSeg, 0x14, readU32(rayEnd, 0x08));
    recalculateLineSeg(lineSeg);

    // RayCollisionTestClass on stack (0x40 bytes)
    std::uint8_t rayTest[0x40];
    initRayCollisionTest(rayTest, lineSeg, castResult, 2u, 0u, 0u);

    // Call renderObj->vtable[2] (+0x08): castRay(rayTest)
    const void* vtable = readPtr(renderObj, 0x00);
    using CastRayFn = std::uint32_t (__thiscall*)(void*, void*);
    const auto castRay = reinterpret_cast<CastRayFn>(const_cast<void*>(readPtr(vtable, 0x08)));
    const std::uint32_t res = castRay(renderObj, rayTest);
    const std::uint8_t hit = static_cast<std::uint8_t>(res & 0xFFu);

    if (hit != 0u && outHitPoint != nullptr) {
        writeU32(outHitPoint, 0x00, readU32(castResult, 0x18));
        writeU32(outHitPoint, 0x04, readU32(castResult, 0x1c));
        writeU32(outHitPoint, 0x08, readU32(castResult, 0x20));
    }
    return res;
}

// 0x0057a140 — direct callee of 0x00587ff0 at 0x00588039
void initWaterWaveGridState(void* waterState) {
    writeU32(waterState, 0x24, 0u);
    writeF32(waterState, 0x12f4, 0.0f);
    writeF32(waterState, 0x12f8, 1.0f);        // 0x00bd90a4
    writeF32(waterState, 0x12fc, 2.25f);       // 0x00be40f8
    writeU8(waterState, 0x1308, 0u);
    writeU8(waterState, 0x1315, 1u);
    writeU32(waterState, 0x1304, 0u);
    writeU32(waterState, 0x1300, 0u);
    writeU32(waterState, 0x28, 0u);
    writeU32(waterState, 0x2c, 0u);
    writeU8(waterState, 0x1309, 0u);
    writeU32(waterState, 0x30, 0u);

    for (int i = 0; i < 100; ++i) {
        const std::ptrdiff_t base = 0x34 + i * 0x30;
        for (std::ptrdiff_t off = 0x00; off <= 0x2c; off += 4) {
            writeU32(waterState, base + off, 0u);
        }
    }

    writeF32(waterState, 0x130c, 0.0f);
    writeF32(waterState, 0x1310, 0.0f);
    writeU8(waterState, 0x1314, 0u);
    writeU32(waterState, 0x1318, 0u);
    writeU32(waterState, 0x131c, 0u);
}

// ============================================================================
// 2. W3DTerrainLogic & direct helpers
// ============================================================================

// 0x0056ec10 — 0x00be39f8 slot 10: W3DTerrainLogic::getExtent
void getTerrainLogicExtent(const void* terrainLogic, void* extent) {
    const auto* begin = static_cast<const std::uint8_t*>(readPtr(terrainLogic, 0x54));
    const auto* end = static_cast<const std::uint8_t*>(readPtr(terrainLogic, 0x58));
    if (begin != end) {
        const std::int32_t idx = readI32(terrainLogic, 0x64);
        const auto* entry = begin + static_cast<std::ptrdiff_t>(idx) * 16;
        writeF32(extent, 0x00, static_cast<float>(readI32(entry, 0x00)) * 10.0f);
        writeF32(extent, 0x04, static_cast<float>(readI32(entry, 0x04)) * 10.0f);
        writeF32(extent, 0x0c, static_cast<float>(readI32(entry, 0x08)) * 10.0f);
        writeF32(extent, 0x10, static_cast<float>(readI32(entry, 0x0c)) * 10.0f);
    } else {
        writeF32(extent, 0x00, 0.0f);
        writeF32(extent, 0x04, 0.0f);
        writeF32(extent, 0x0c, 0.0f);
        writeF32(extent, 0x10, 0.0f);
    }
    x87CopyDword(extent, 0x08, terrainLogic, 0x1438);
    x87CopyDword(extent, 0x14, terrainLogic, 0x143c);
}

// 0x0056ecc0 — 0x00be39f8 slot 11: W3DTerrainLogic::getMaximumPathfindExtent
void getMaximumPathfindExtent(const void* terrainLogic, void* extent) {
    const auto* begin = static_cast<const std::uint8_t*>(readPtr(terrainLogic, 0x54));
    const auto* end = static_cast<const std::uint8_t*>(readPtr(terrainLogic, 0x58));
    if (begin != end) {
        float minX = static_cast<float>(readI32(begin, 0x00));
        float minY = static_cast<float>(readI32(begin, 0x04));
        float maxX = static_cast<float>(readI32(begin, 0x08));
        float maxY = static_cast<float>(readI32(begin, 0x0c));

        const std::uint32_t count = static_cast<std::uint32_t>((end - begin) >> 4);
        for (std::uint32_t i = 1; i < count; ++i) {
            const auto* entry = begin + static_cast<std::ptrdiff_t>(i) * 16;
            const float x0 = static_cast<float>(readI32(entry, 0x00));
            if (minX > x0) minX = x0;
            const float y0 = static_cast<float>(readI32(entry, 0x04));
            if (minY > y0) minY = y0;
            const float x1 = static_cast<float>(readI32(entry, 0x08));
            if (x1 > maxX) maxX = x1;
            const float y1 = static_cast<float>(readI32(entry, 0x0c));
            if (y1 > maxY) maxY = y1;
        }
        writeF32(extent, 0x00, minX * 10.0f);
        writeF32(extent, 0x04, minY * 10.0f);
        writeF32(extent, 0x0c, maxX * 10.0f);
        writeF32(extent, 0x10, maxY * 10.0f);
    } else {
        writeF32(extent, 0x00, 0.0f);
        writeF32(extent, 0x04, 0.0f);
        writeF32(extent, 0x0c, 0.0f);
        writeF32(extent, 0x10, 0.0f);
    }
    x87CopyDword(extent, 0x08, terrainLogic, 0x1438);
    x87CopyDword(extent, 0x14, terrainLogic, 0x143c);
}

// 0x0056edb0 — 0x00be39f8 slot 14: W3DTerrainLogic::getSmallestExtent
void getSmallestTerrainExtent(const void* terrainLogic, void* extent) {
    writeF32(extent, 0x00, 0.0f);
    writeF32(extent, 0x04, 0.0f);
    writeF32(extent, 0x0c, 0.0f);
    writeF32(extent, 0x10, 0.0f);

    const auto* begin = static_cast<const std::uint8_t*>(readPtr(terrainLogic, 0x54));
    const auto* end = static_cast<const std::uint8_t*>(readPtr(terrainLogic, 0x58));
    const std::uint32_t count = static_cast<std::uint32_t>((end - begin) >> 4);
    for (std::uint32_t i = 0; i < count; ++i) {
        const auto* entry = begin + static_cast<std::ptrdiff_t>(i) * 16;
        const float x0 = static_cast<float>(readI32(entry, 0x00)) * 10.0f;
        if (readF32(extent, 0x00) > x0) writeF32(extent, 0x00, x0);
        const float y0 = static_cast<float>(readI32(entry, 0x04)) * 10.0f;
        if (readF32(extent, 0x04) > y0) writeF32(extent, 0x04, y0);
        const float x1 = static_cast<float>(readI32(entry, 0x08)) * 10.0f;
        if (x1 > readF32(extent, 0x0c)) writeF32(extent, 0x0c, x1);
        const float y1 = static_cast<float>(readI32(entry, 0x0c)) * 10.0f;
        if (y1 > readF32(extent, 0x10)) writeF32(extent, 0x10, y1);
    }
    x87CopyDword(extent, 0x08, terrainLogic, 0x1438);
    x87CopyDword(extent, 0x14, terrainLogic, 0x143c);
}

// 0x00563980 — direct callee of 0x0056ef10 at 0x0056ef57
void orderCoord3DPair(
    float ax, float ay, float az,
    float bx, float by, float bz,
    void* outFirst,
    void* outSecond) {
    const std::uint32_t axBits = floatToBits(ax);
    const std::uint32_t ayBits = floatToBits(ay);
    const std::uint32_t azBits = floatToBits(az);
    const std::uint32_t bxBits = floatToBits(bx);
    const std::uint32_t byBits = floatToBits(by);
    const std::uint32_t bzBits = floatToBits(bz);

    bool aFirst = false;
    if (bx > ax) {
        aFirst = true;
    } else if (ax == bx && by > ay) {
        aFirst = true;
    }

    if (aFirst) {
        writeU32(outFirst, 0x00, axBits);
        writeU32(outFirst, 0x04, ayBits);
        writeU32(outFirst, 0x08, azBits);
        writeU32(outSecond, 0x00, bxBits);
        writeU32(outSecond, 0x04, byBits);
        writeU32(outSecond, 0x08, bzBits);
    } else {
        writeU32(outFirst, 0x00, bxBits);
        writeU32(outFirst, 0x04, byBits);
        writeU32(outFirst, 0x08, bzBits);
        writeU32(outSecond, 0x00, axBits);
        writeU32(outSecond, 0x04, ayBits);
        writeU32(outSecond, 0x08, azBits);
    }
}

// ============================================================================
// 3. WorldHeightMap
// ============================================================================

// 0x00562bb0 — WorldHeightMap::setFlipState
void setFlipState(void* heightMap, std::int32_t xIndex, std::int32_t yIndex, std::uint8_t value) {
    if (xIndex < 0 || yIndex < 0) {
        return;
    }
    if (yIndex >= readI32(heightMap, 0x28) || xIndex >= readI32(heightMap, 0x24)) {
        return;
    }
    const std::uint32_t byteIndex = static_cast<std::uint32_t>(yIndex * readI32(heightMap, 0x54) + (xIndex >> 3));
    if (byteIndex >= readU32(heightMap, 0x50)) {
        return;
    }
    auto* data = static_cast<std::uint8_t*>(readMutPtr(heightMap, 0x4c));
    const std::uint8_t mask = static_cast<std::uint8_t>(1u << (xIndex & 7));
    if (value != 0u) {
        data[byteIndex] |= mask;
    } else {
        data[byteIndex] &= static_cast<std::uint8_t>(~mask);
    }
}

// 0x00562d30 — WorldHeightMap::countTiles
std::int32_t countTilesFromTgaHeader(void* inputStream, std::uint8_t* halfTileOut) {
    if (halfTileOut != nullptr) {
        *halfTileOut = 0u;
    }
    std::uint8_t hdr[0x12];
    std::memset(hdr, 0, sizeof(hdr));

    const void* vtable = readPtr(inputStream, 0x00);
    using ReadFn = std::int32_t (__thiscall*)(void*, void*, std::int32_t);
    const auto readMethod = reinterpret_cast<ReadFn>(const_cast<void*>(readPtr(vtable, 0x00)));
    if (readMethod(inputStream, hdr, 0x12) != 0x12) {
        return 0;
    }

    const std::int16_t w = readI16(hdr, 0x0c);
    const std::int16_t h = readI16(hdr, 0x0e);
    const std::int32_t tileHeight = static_cast<std::int32_t>(h) / 64;
    const std::int32_t tileWidth = static_cast<std::int32_t>(w) / 64;

    if (hdr[0x01] != 0u) {
        return 0;
    }
    const std::uint8_t imgType = hdr[0x02];
    if (imgType != 2u && imgType != 10u) {
        return 0;
    }
    const std::uint8_t bpp = hdr[0x10];
    if (bpp < 24u || bpp > 32u) {
        return 0;
    }
    if (tileHeight > 16 || tileWidth > 16) {
        return 0;
    }

    for (std::int32_t row = 16; row > 0; --row) {
        if (tileHeight >= row && tileWidth >= row) {
            return row * row;
        }
    }

    if (halfTileOut != nullptr && w == 32 && h == 32) {
        *halfTileOut = 1u;
        return 1;
    }
    return 0;
}

// 0x00562df0 — WorldHeightMap::getTerrainColorAt
void getTerrainColorAt(const void* heightMap, float x, float y, void* outColor) {
    const std::int32_t border = readI32(heightMap, 0x2c);
    writeU32(outColor, 0x08, 0u);
    writeU32(outColor, 0x04, 0u);
    writeU32(outColor, 0x00, 0u);

    constexpr float kNegTenth = -0.10000000149011612f; // 0x00be39b8 (0xbdcccccd)
    const std::int32_t cellX = border - static_cast<std::int32_t>(x * kNegTenth);
    const std::int32_t cellY = border - static_cast<std::int32_t>(y * kNegTenth);

    const std::int32_t width = readI32(heightMap, 0x24);
    const std::int32_t height = readI32(heightMap, 0x28);
    const std::int32_t clampedX = cellX < 0 ? 0 : (cellX >= width ? width - 1 : cellX);
    const std::int32_t clampedY = cellY < 0 ? 0 : (cellY >= height ? height - 1 : cellY);

    const std::int32_t idx = clampedY * width + clampedX;
    if (idx < 0 || idx >= readI32(heightMap, 0x40)) {
        return;
    }
    const void* tileNdxes = readPtr(heightMap, 0xf8);
    const std::int32_t tileGroup = static_cast<std::int32_t>(readI16(tileNdxes, static_cast<std::ptrdiff_t>(idx) * 2)) >> 2;
    if (static_cast<std::uint32_t>(tileGroup) >= 0x1000u) {
        return;
    }
    const void* tilePtr = readPtr(heightMap, 0x110 + static_cast<std::ptrdiff_t>(tileGroup) * 4);
    if (tilePtr == nullptr) {
        return;
    }
    const std::uint32_t w = readU16(tilePtr, 0x2ab0);
    std::uint32_t ebx = w << 3;
    std::uint32_t eax = (w >> 10) << 19;
    const std::uint8_t bh = static_cast<std::uint8_t>(((ebx >> 8) & 0xFFu) << 3);
    ebx = (ebx & 0xFFFF00FFu) | (static_cast<std::uint32_t>(bh) << 8);
    eax = (eax & 0xFFFF0000u) | (ebx & 0xFFFFu);
    const std::uint32_t rgb24 = eax + ((eax >> 5) & 0x003F3F3Fu);

    constexpr double kInv255 = 0.00392156862745098; // 0x00be39b0 (1.0 / 255.0)
    const float c0 = static_cast<float>(static_cast<double>(rgb24 & 0xFFu) * kInv255);
    const float c1 = static_cast<float>(static_cast<double>((rgb24 >> 8) & 0xFFu) * kInv255);
    const float c2 = static_cast<float>(static_cast<double>((rgb24 >> 16) & 0xFFu) * kInv255);
    writeF32(outColor, 0x00, c0);
    writeF32(outColor, 0x04, c1);
    writeF32(outColor, 0x08, c2);
}

// 0x00562f10 — WorldHeightMap::evaluateBlendTileCoeffs
void evaluateBlendTileCoeffs(const void* heightMap, const void* blendInfo, float* outCoeffs4) {
    (void)heightMap;
    outCoeffs4[3] = 0.0f;
    outCoeffs4[2] = 0.0f;
    outCoeffs4[1] = 0.0f;
    outCoeffs4[0] = 0.0f;
    if (blendInfo == nullptr) {
        return;
    }

    const std::uint8_t b4 = readU8(blendInfo, 0x04);
    const std::uint8_t b5 = readU8(blendInfo, 0x05);
    const std::uint8_t b6 = readU8(blendInfo, 0x06);
    const std::uint8_t b7 = readU8(blendInfo, 0x07);
    const std::uint8_t b8 = readU8(blendInfo, 0x08);
    const std::uint8_t b9 = readU8(blendInfo, 0x09);

    if (b4 != 0u) {
        if ((b8 & 1u) != 0u) {
            outCoeffs4[0] = 1.0f;
            outCoeffs4[3] = 1.0f;
        } else {
            outCoeffs4[2] = 1.0f;
            outCoeffs4[1] = 1.0f;
        }
        return;
    }
    if (b5 != 0u) {
        if ((b8 & 1u) != 0u) {
            outCoeffs4[1] = 1.0f;
            outCoeffs4[0] = 1.0f;
        } else {
            outCoeffs4[2] = 1.0f;
            outCoeffs4[3] = 1.0f;
        }
        return;
    }
    if (b6 != 0u) {
        if ((b8 & 1u) != 0u) {
            if (b9 != 0u) {
                outCoeffs4[0] = 1.0f;
                outCoeffs4[1] = 2.0f;
                outCoeffs4[2] = 1.0f;
            } else {
                outCoeffs4[1] = 1.0f;
                outCoeffs4[3] = -1.0f;
            }
        } else {
            if (b9 != 0u) {
                outCoeffs4[1] = 1.0f;
                outCoeffs4[2] = 2.0f;
                outCoeffs4[3] = 1.0f;
            } else {
                outCoeffs4[0] = -1.0f;
                outCoeffs4[2] = 1.0f;
            }
        }
        return;
    }
    if (b7 != 0u) {
        if ((b8 & 1u) != 0u) {
            if (b9 != 0u) {
                outCoeffs4[0] = 2.0f;
                outCoeffs4[1] = 1.0f;
                outCoeffs4[3] = 1.0f;
            } else {
                outCoeffs4[0] = 1.0f;
                outCoeffs4[2] = -1.0f;
            }
        } else {
            if (b9 != 0u) {
                outCoeffs4[0] = 1.0f;
                outCoeffs4[2] = 1.0f;
                outCoeffs4[3] = 2.0f;
            } else {
                outCoeffs4[1] = -1.0f;
                outCoeffs4[3] = 1.0f;
            }
        }
    }
}

namespace {

inline std::uint32_t queryBitSpanUnsignedBounds(
    const void* heightMap,
    std::ptrdiff_t beginOffset,
    std::ptrdiff_t endOffset,
    std::int32_t xIndex,
    std::int32_t yIndex) {
    if (static_cast<std::uint32_t>(xIndex) >= readU32(heightMap, 0x24) ||
        static_cast<std::uint32_t>(yIndex) >= readU32(heightMap, 0x28)) {
        return 0u;
    }
    const std::uint32_t byteIndex = static_cast<std::uint32_t>(yIndex * readI32(heightMap, 0x54) + (xIndex >> 3));
    const auto* begin = static_cast<const std::uint8_t*>(readPtr(heightMap, beginOffset));
    const auto* end = static_cast<const std::uint8_t*>(readPtr(heightMap, endOffset));
    const std::uint32_t spanSize = static_cast<std::uint32_t>(end - begin);
    if (byteIndex >= spanSize) {
        return 0u;
    }
    const std::uint8_t mask = static_cast<std::uint8_t>(1u << (xIndex & 7));
    return (begin[byteIndex] & mask) != 0u ? 1u : 0u;
}

inline std::uint32_t queryBitSpanSignedBoundsNormalized(
    const void* heightMap,
    std::ptrdiff_t beginOffset,
    std::ptrdiff_t endOffset,
    std::int32_t xIndex,
    std::int32_t yIndex) {
    if (xIndex < 0 || yIndex < 0 || yIndex >= readI32(heightMap, 0x28) || xIndex >= readI32(heightMap, 0x24)) {
        return 0u;
    }
    const std::uint32_t byteIndex = static_cast<std::uint32_t>(yIndex * readI32(heightMap, 0x54) + (xIndex >> 3));
    const std::uint32_t eaxUpper = byteIndex & 0xFFFFFF00u;
    const auto* begin = static_cast<const std::uint8_t*>(readPtr(heightMap, beginOffset));
    const auto* end = static_cast<const std::uint8_t*>(readPtr(heightMap, endOffset));
    const std::uint32_t spanSize = static_cast<std::uint32_t>(end - begin);
    if (byteIndex >= spanSize) {
        return eaxUpper;
    }
    const std::uint8_t mask = static_cast<std::uint8_t>(1u << (xIndex & 7));
    const std::uint8_t al = (begin[byteIndex] & mask) != 0u ? 1u : 0u;
    return eaxUpper | al;
}

inline std::uint32_t queryBitSpanSignedBoundsRawMask(
    const void* heightMap,
    std::ptrdiff_t beginOffset,
    std::ptrdiff_t endOffset,
    std::int32_t xIndex,
    std::int32_t yIndex) {
    if (xIndex < 0) {
        return 0u;
    }
    std::uint32_t eaxUpper = static_cast<std::uint32_t>(yIndex) & 0xFFFFFF00u;
    if (yIndex < 0 || yIndex >= readI32(heightMap, 0x28) || xIndex >= readI32(heightMap, 0x24)) {
        return eaxUpper;
    }
    const std::uint32_t byteIndex = static_cast<std::uint32_t>(yIndex * readI32(heightMap, 0x54) + (xIndex >> 3));
    const auto* begin = static_cast<const std::uint8_t*>(readPtr(heightMap, beginOffset));
    const auto* end = static_cast<const std::uint8_t*>(readPtr(heightMap, endOffset));
    const std::uint32_t spanSize = static_cast<std::uint32_t>(end - begin);
    eaxUpper = spanSize & 0xFFFFFF00u;
    if (byteIndex >= spanSize) {
        return eaxUpper;
    }
    const std::uint32_t mask = 1u << (xIndex & 7);
    const std::uint32_t byteVal = begin[byteIndex];
    return byteVal & mask;
}

} // namespace

// 0x005642b0 — WorldHeightMap::getCellFlagBit78
std::uint32_t getCellFlagBit78(const void* heightMap, std::int32_t xIndex, std::int32_t yIndex) {
    return queryBitSpanUnsignedBounds(heightMap, 0x78, 0x7c, xIndex, yIndex);
}

// 0x00564300 — WorldHeightMap::getCellFlagBit88
std::uint32_t getCellFlagBit88(const void* heightMap, std::int32_t xIndex, std::int32_t yIndex) {
    return queryBitSpanUnsignedBounds(heightMap, 0x88, 0x8c, xIndex, yIndex);
}

// 0x0056d970 — WorldHeightMap::getCliffState
std::uint32_t getCliffState(const void* heightMap, std::int32_t xIndex, std::int32_t yIndex) {
    return queryBitSpanSignedBoundsNormalized(heightMap, 0x58, 0x5c, xIndex, yIndex);
}

// 0x0056d9d0 — WorldHeightMap::getCellFlagBit98
std::uint32_t getCellFlagBit98(const void* heightMap, std::int32_t xIndex, std::int32_t yIndex) {
    return queryBitSpanSignedBoundsNormalized(heightMap, 0x98, 0x9c, xIndex, yIndex);
}

// 0x0056da40 — WorldHeightMap::getCellFlagBitB8
std::uint32_t getCellFlagBitB8(const void* heightMap, std::int32_t xIndex, std::int32_t yIndex) {
    return queryBitSpanSignedBoundsNormalized(heightMap, 0xb8, 0xbc, xIndex, yIndex);
}

// 0x0056dab0 — WorldHeightMap::getCellFlagBitA8
std::uint32_t getCellFlagBitA8(const void* heightMap, std::int32_t xIndex, std::int32_t yIndex) {
    return queryBitSpanSignedBoundsNormalized(heightMap, 0xa8, 0xac, xIndex, yIndex);
}

// 0x0056db20 — WorldHeightMap::getCellFlagBitD8
std::uint32_t getCellFlagBitD8(const void* heightMap, std::int32_t xIndex, std::int32_t yIndex) {
    return queryBitSpanSignedBoundsRawMask(heightMap, 0xd8, 0xdc, xIndex, yIndex);
}

// 0x0056db80 — WorldHeightMap::getCellFlagBitC8
std::uint32_t getCellFlagBitC8(const void* heightMap, std::int32_t xIndex, std::int32_t yIndex) {
    return queryBitSpanSignedBoundsRawMask(heightMap, 0xc8, 0xcc, xIndex, yIndex);
}

// 0x0056dc30 — WorldHeightMap::setCliffState
void setCliffState(void* heightMap, std::int32_t xIndex, std::int32_t yIndex, std::uint8_t state) {
    if (xIndex < 0 || yIndex < 0 || yIndex >= readI32(heightMap, 0x28) || xIndex >= readI32(heightMap, 0x24)) {
        return;
    }
    const std::uint32_t byteIndex = static_cast<std::uint32_t>(yIndex * readI32(heightMap, 0x54) + (xIndex >> 3));
    auto* begin = static_cast<std::uint8_t*>(readMutPtr(heightMap, 0x58));
    const auto* end = static_cast<const std::uint8_t*>(readPtr(heightMap, 0x5c));
    const std::uint32_t spanSize = static_cast<std::uint32_t>(end - begin);
    if (byteIndex >= spanSize) {
        return;
    }
    const std::uint8_t mask = static_cast<std::uint8_t>(1u << (xIndex & 7));
    if (state != 0u) {
        begin[byteIndex] |= mask;
    } else {
        begin[byteIndex] &= static_cast<std::uint8_t>(~mask);
    }
}

// 0x0056dfb0 — WorldHeightMap::setCellCliffFlagFromHeights
void setCellCliffFlagFromHeights(void* heightMap, std::int32_t xIndex, std::int32_t yIndex) {
    const std::uint32_t dataSize = readU32(heightMap, 0x40);
    const std::int32_t width = readI32(heightMap, 0x24);
    const void* heights = readPtr(heightMap, 0x44);

    const std::uint32_t idx0 = static_cast<std::uint32_t>(yIndex * width + xIndex);
    const float h0 = static_cast<float>(idx0 < dataSize ? readU16(heights, static_cast<std::ptrdiff_t>(idx0) * 2) : 0u);

    const std::uint32_t idx1 = idx0 + 1u;
    const float h1 = static_cast<float>(idx1 < dataSize ? readU16(heights, static_cast<std::ptrdiff_t>(idx1) * 2) : 0u);

    const std::uint32_t idx2 = static_cast<std::uint32_t>((yIndex + 1) * width + xIndex);
    const float h2 = static_cast<float>(idx2 < dataSize ? readU16(heights, static_cast<std::ptrdiff_t>(idx2) * 2) : 0u);

    const std::uint32_t idx3 = idx2 + 1u;
    const float h3 = static_cast<float>(idx3 < dataSize ? readU16(heights, static_cast<std::ptrdiff_t>(idx3) * 2) : 0u);

    float minH = h0;
    float maxH = h0;
    if (minH > h1) minH = h1;
    else if (h1 > maxH) maxH = h1;

    if (minH > h2) minH = h2;
    else if (h2 > maxH) maxH = h2;

    if (minH > h3) minH = h3;
    else if (h3 > maxH) maxH = h3;

    // comiss xmm0, dword ptr [0xbe3d48] (250.8800048828125f = 0x437ae148)
    const std::uint8_t isCliff = (maxH - minH > bitsToFloat(0x437ae148u)) ? 1u : 0u;
    setCliffState(heightMap, xIndex, yIndex, isCliff);
}

// 0x0057e4e0 — WorldHeightMap::initCliffFlagsFromHeights
void initCliffFlagsFromHeights(void* heightMap) {
    for (std::int32_t x = 0; x < readI32(heightMap, 0x24) - 1; ++x) {
        for (std::int32_t y = 0; y < readI32(heightMap, 0x28) - 1; ++y) {
            setCellCliffFlagFromHeights(heightMap, x, y);
        }
    }
}

// 0x0056e0a0 — WorldHeightMap::getCellBlendClassesAndCoeffs
void getCellBlendClassesAndCoeffs(
    const void* heightMap,
    std::int32_t xIndex,
    std::int32_t yIndex,
    std::int32_t* outTexClasses3,
    float* outBlendCoeffs8) {
    outTexClasses3[2] = -1;
    outTexClasses3[1] = -1;
    outTexClasses3[0] = -1;

    const std::int32_t idx = yIndex * readI32(heightMap, 0x24) + xIndex;
    if (idx < 0 || idx >= readI32(heightMap, 0x40)) {
        return;
    }

    const void* tileNdxes = readPtr(heightMap, 0xf8);
    const std::int32_t baseTileGroup = static_cast<std::int32_t>(readI16(tileNdxes, static_cast<std::ptrdiff_t>(idx) * 2)) >> 2;

    const void* blendTileNdxes = readPtr(heightMap, 0xfc);
    const std::int32_t blendIdx1 = static_cast<std::int32_t>(readU16(blendTileNdxes, static_cast<std::ptrdiff_t>(idx) * 2));

    const std::uint8_t* blendBase = static_cast<const std::uint8_t*>(readPtr(heightMap, 0x8110));
    const std::uint8_t* blendEnd = static_cast<const std::uint8_t*>(readPtr(heightMap, 0x8114));
    const std::int32_t numBlendDescs = static_cast<std::int32_t>((blendEnd - blendBase) >> 4);

    std::int32_t blendGroup1 = -1;
    const void* blendPtr1 = nullptr;
    if (blendIdx1 > 0 && blendIdx1 < numBlendDescs) {
        blendPtr1 = blendBase + blendIdx1 * 16;
        blendGroup1 = readI32(blendPtr1, 0x00) >> 2;
    }

    const void* extraBlendTileNdxes = readPtr(heightMap, 0x104);
    const std::int32_t blendIdx2 = static_cast<std::int32_t>(readU16(extraBlendTileNdxes, static_cast<std::ptrdiff_t>(idx) * 2));

    std::int32_t blendGroup2 = -1;
    const void* blendPtr2 = nullptr;
    if (blendIdx2 > 0 && blendIdx2 < numBlendDescs) {
        blendPtr2 = blendBase + blendIdx2 * 16;
        blendGroup2 = readI32(blendPtr2, 0x00) >> 2;
    }

    const std::int32_t numTexClasses = readI32(heightMap, 0x8130);
    for (std::int32_t cls = 0; cls < numTexClasses; ++cls) {
        const std::ptrdiff_t clsOff = 0x8138 + static_cast<std::ptrdiff_t>(cls) * 0x3c;
        const std::int32_t firstTile = readI32(heightMap, clsOff + 0x00);
        if (firstTile >= 0) {
            const std::int32_t numTiles = readI32(heightMap, clsOff + 0x04);
            if (baseTileGroup >= firstTile && baseTileGroup < firstTile + numTiles) {
                outTexClasses3[0] = cls;
            }
            if (blendPtr1 != nullptr && blendGroup1 >= firstTile && blendGroup1 < firstTile + numTiles) {
                outTexClasses3[1] = cls;
            }
            if (blendPtr2 != nullptr && blendGroup2 >= firstTile && blendGroup2 < firstTile + numTiles) {
                outTexClasses3[2] = cls;
            }
        }
    }

    const void* argPtr1 = (outTexClasses3[1] != -1) ? blendPtr1 : nullptr;
    evaluateBlendTileCoeffs(heightMap, argPtr1, outBlendCoeffs8);

    const void* argPtr2 = (outTexClasses3[2] != -1) ? blendPtr2 : nullptr;
    evaluateBlendTileCoeffs(heightMap, argPtr2, outBlendCoeffs8 + 4);
}

// 0x0056dd70 — WorldHeightMap::isCliffMappedTextureCheck
std::uint32_t isCliffMappedTextureCheck(const void* heightMap, std::int32_t xOffset, std::int32_t yOffset) {
    const std::int32_t idx = (readI32(heightMap, 0x1714c) + yOffset) * readI32(heightMap, 0x24)
        + readI32(heightMap, 0x17148) + xOffset;
    if (idx < 0 || idx >= readI32(heightMap, 0x40)) {
        return 1u;
    }

    const void* cliffInfoNdxes = readPtr(heightMap, 0x100);
    const std::uint16_t cliffNdx = readU16(cliffInfoNdxes, static_cast<std::ptrdiff_t>(idx) * 2);
    const void* cliffTable = readPtr(heightMap, 0x8120);
    const std::uint8_t cliffEntryByte3 = readU8(cliffTable, static_cast<std::ptrdiff_t>(cliffNdx) * 36 + 3);
    const std::int32_t cliffGroup = static_cast<std::int32_t>(readI16(cliffTable, static_cast<std::ptrdiff_t>(cliffNdx) * 36 + 0x22)) >> 2;

    const void* tileNdxes = readPtr(heightMap, 0xf8);
    const std::int32_t baseTileGroup = static_cast<std::int32_t>(readI16(tileNdxes, static_cast<std::ptrdiff_t>(idx) * 2)) >> 2;

    std::uint8_t bl = 0u;
    const std::int32_t numTexClasses = readI32(heightMap, 0x8130);
    for (std::int32_t cls = 0; cls < numTexClasses; ++cls) {
        const std::ptrdiff_t clsOff = 0x8138 + static_cast<std::ptrdiff_t>(cls) * 0x3c;
        const std::int32_t firstTile = readI32(heightMap, clsOff + 0x00);
        if (baseTileGroup >= firstTile) {
            const std::int32_t endTile = firstTile + readI32(heightMap, clsOff + 0x04);
            if (baseTileGroup < endTile) {
                if (cliffGroup >= firstTile && cliffGroup < endTile) {
                    bl = 1u;
                } else {
                    bl = 0u;
                }
                return bl == 0u ? 1u : 0u;
            }
            bl = cliffEntryByte3;
        }
    }
    return bl == 0u ? 1u : 0u;
}

// 0x0057db70 — WorldHeightMap::shouldFlipCellQuad
std::uint32_t shouldFlipCellQuad(const void* heightMap, std::int32_t xIndex, std::int32_t yIndex) {
    const std::int32_t width = readI32(heightMap, 0x24);
    const std::int32_t dataSize = readI32(heightMap, 0x40);
    const std::uint32_t drawIdx = static_cast<std::uint32_t>(
        (readI32(heightMap, 0x1714c) + yIndex) * width + readI32(heightMap, 0x17148) + xIndex);

    std::uint32_t eaxReg = drawIdx;
    if (drawIdx < static_cast<std::uint32_t>(dataSize)) {
        const void* cliffInfoNdxes = readPtr(heightMap, 0x100);
        const std::uint16_t cliffNdx = readU16(cliffInfoNdxes, static_cast<std::ptrdiff_t>(drawIdx) * 2);
        const std::uint8_t hasCliff = cliffNdx != 0u ? 1u : 0u;
        eaxReg = (eaxReg & 0xFFFFFF00u) | hasCliff;
        if (hasCliff != 0u) {
            const std::uint32_t checkRes = isCliffMappedTextureCheck(heightMap, xIndex, yIndex);
            if ((checkRes & 0xFFu) != 0u) {
                return static_cast<std::uint32_t>(static_cast<std::uint8_t>(xIndex & 0xFF));
            }
        }
    }

    const std::int32_t cellIdx = yIndex * width + xIndex;
    eaxReg = static_cast<std::uint32_t>(cellIdx);
    if (cellIdx < 0 || cellIdx >= dataSize) {
        return eaxReg & 0xFFFFFF00u;
    }
    const void* heights = readPtr(heightMap, 0x44);
    if (heights == nullptr) {
        return eaxReg & 0xFFFFFF00u;
    }
    if (cellIdx >= dataSize - width - 1) {
        return eaxReg & 0xFFFFFF00u;
    }

    const std::int32_t h00 = readU16(heights, static_cast<std::ptrdiff_t>(cellIdx) * 2);
    const std::int32_t h10 = readU16(heights, static_cast<std::ptrdiff_t>(cellIdx) * 2 + 2);
    const std::int32_t rowNext = cellIdx + width;
    const std::int32_t h01 = readU16(heights, static_cast<std::ptrdiff_t>(rowNext) * 2);
    const std::int32_t h11 = readU16(heights, static_cast<std::ptrdiff_t>(rowNext) * 2 + 2);

    std::int32_t minH = h00 < h10 ? h00 : h10;
    std::int32_t maxH = h00 > h10 ? h00 : h10;
    if (minH > h11) minH = h11;
    if (maxH < h11) maxH = h11;
    if (minH > h01) minH = h01;
    if (maxH < h01) maxH = h01;

    const std::int32_t diff = maxH - minH;
    eaxReg = static_cast<std::uint32_t>(diff);
    // mulss xmm0, [0x00cb4fd4] (0.00390625f); comiss xmm0, [0x00cb4fd0] (0.5f)
    if (static_cast<float>(diff) * 0.00390625f <= 0.5f) {
        return eaxReg & 0xFFFFFF00u;
    }

    std::int32_t diag1 = h00 - h11;
    if (diag1 < 0) diag1 = -diag1;
    std::int32_t diag2 = h10 - h01;
    if (diag2 < 0) diag2 = -diag2;
    return diag1 > diag2 ? 1u : 0u;
}

// 0x0056de70 — WorldHeightMap::getBlendVertexAlphaMask
void getBlendVertexAlphaMask(
    const void* heightMap,
    std::int32_t xOffset,
    std::int32_t yOffset,
    std::uint8_t* outAlphaBytes4,
    std::uint32_t unusedArg) {
    (void)unusedArg;
    const std::int32_t idx = (readI32(heightMap, 0x1714c) + yOffset) * readI32(heightMap, 0x24)
        + readI32(heightMap, 0x17148) + xOffset;
    if (idx >= readI32(heightMap, 0x40)) {
        return;
    }
    if (readPtr(heightMap, 0xf8) == nullptr) {
        return;
    }
    const void* blendTileNdxes = readPtr(heightMap, 0xfc);
    const std::uint16_t blendNdx = readU16(blendTileNdxes, static_cast<std::ptrdiff_t>(idx) * 2);

    outAlphaBytes4[3] = 0u;
    outAlphaBytes4[2] = 0u;
    outAlphaBytes4[1] = 0u;
    outAlphaBytes4[0] = 0u;
    if (blendNdx == 0u) {
        return;
    }

    const void* blendBase = readPtr(heightMap, 0x8110);
    const std::ptrdiff_t descOff = static_cast<std::ptrdiff_t>(blendNdx) * 16;
    const std::uint8_t b4 = readU8(blendBase, descOff + 0x04);
    const std::uint8_t b5 = readU8(blendBase, descOff + 0x05);
    const std::uint8_t b6 = readU8(blendBase, descOff + 0x06);
    const std::uint8_t b7 = readU8(blendBase, descOff + 0x07);
    const std::uint8_t b8 = readU8(blendBase, descOff + 0x08);
    const std::uint8_t b9 = readU8(blendBase, descOff + 0x09);

    if (b4 != 0u) {
        if ((b8 & 1u) != 0u) {
            outAlphaBytes4[3] = 0xFFu;
            outAlphaBytes4[0] = 0xFFu;
        } else {
            outAlphaBytes4[2] = 0xFFu;
            outAlphaBytes4[1] = 0xFFu;
        }
    }
    if (b5 != 0u) {
        if ((b8 & 1u) != 0u) {
            outAlphaBytes4[1] = 0xFFu;
            outAlphaBytes4[0] = 0xFFu;
        } else {
            outAlphaBytes4[3] = 0xFFu;
            outAlphaBytes4[2] = 0xFFu;
        }
    }
    if (b6 != 0u) {
        if ((b8 & 1u) != 0u) {
            outAlphaBytes4[1] = 0xFFu;
            if (b9 != 0u) {
                outAlphaBytes4[0] = 0xFFu;
                outAlphaBytes4[2] = 0xFFu;
            }
        } else {
            outAlphaBytes4[2] = 0xFFu;
            if (b9 != 0u) {
                outAlphaBytes4[1] = 0xFFu;
                outAlphaBytes4[3] = 0xFFu;
            }
        }
    }
    if (b7 != 0u) {
        if ((b8 & 1u) != 0u) {
            outAlphaBytes4[0] = 0xFFu;
            if (b9 != 0u) {
                outAlphaBytes4[1] = 0xFFu;
                outAlphaBytes4[3] = 0xFFu;
            }
        } else {
            outAlphaBytes4[3] = 0xFFu;
            if (b9 != 0u) {
                outAlphaBytes4[0] = 0xFFu;
                outAlphaBytes4[2] = 0xFFu;
            }
        }
    }

    if (readI32(blendBase, descOff + 0x0c) >= 0) {
        outAlphaBytes4[3] = 0u;
        outAlphaBytes4[2] = 0u;
        outAlphaBytes4[1] = 0u;
        outAlphaBytes4[0] = 0u;
    }
}

// ============================================================================
// 4. BaseHeightMapRenderObjClass, Scorch, Road & Prop Buffers
// ============================================================================

// 0x00565250 — 0x00be4290 slot 14: BaseHeightMapRenderObjClass::clearPatchVisibilityAndBounds
void clearPatchVisibilityAndBounds(void* heightMapRenderObj) {
    auto* patchPtr = static_cast<std::uint8_t*>(readMutPtr(heightMapRenderObj, 0xb4));
    const std::int32_t countY = readI32(heightMapRenderObj, 0xc4);
    const std::int32_t countX = readI32(heightMapRenderObj, 0xc0);
    for (std::int32_t y = 0; y < countY; ++y) {
        for (std::int32_t x = 0; x < countX; ++x) {
            *patchPtr = 0u;
            patchPtr += 0x40;
        }
    }
    for (std::ptrdiff_t off = 0xc8; off <= 0xec; off += 4) {
        writeU32(heightMapRenderObj, off, 0u);
    }
}

// 0x00563520 — BaseHeightMapRenderObjClass::initStreamLockHelper
void* initStreamLockHelper(void* helperThis, void* streamObj, std::uint32_t lockArg) {
    writeU32(helperThis, 0x00, reinterpret_cast<std::uintptr_t>(streamObj));
    const void* vtable = readPtr(streamObj, 0x00);
    using Slot3Fn = std::uint32_t (__thiscall*)(void*, std::uint32_t);
    const auto fn = reinterpret_cast<Slot3Fn>(const_cast<void*>(readPtr(vtable, 0x0c)));
    const std::uint32_t elemPtr = fn(streamObj, lockArg);
    writeU32(helperThis, 0x04, elemPtr);
    return helperThis;
}

// 0x005635f0 — MinMaxAABox::vec3SignMaskMin
void* vec3SignMaskMin(void* outVec3, const void* aVec3, const void* bVec3) {
    const float ax = readF32(aVec3, 0x00);
    const float ay = readF32(aVec3, 0x04);
    const float az = readF32(aVec3, 0x08);
    const float bx = readF32(bVec3, 0x00);
    const float by = readF32(bVec3, 0x04);
    const float bz = readF32(bVec3, 0x08);

    const float dx = ax - bx;
    const float dy = ay - by;
    const float dz = az - bz;

    const std::uint32_t mx = static_cast<std::uint32_t>(static_cast<std::int32_t>(floatToBits(dx)) >> 31);
    const std::uint32_t my = static_cast<std::uint32_t>(static_cast<std::int32_t>(floatToBits(dy)) >> 31);
    const std::uint32_t mz = static_cast<std::uint32_t>(static_cast<std::int32_t>(floatToBits(dz)) >> 31);

    const std::uint32_t rx = (floatToBits(ax) & mx) | (floatToBits(bx) & ~mx);
    const std::uint32_t ry = (floatToBits(ay) & my) | (floatToBits(by) & ~my);
    const std::uint32_t rz = (floatToBits(az) & mz) | (floatToBits(bz) & ~mz);

    writeU32(outVec3, 0x00, rx);
    writeU32(outVec3, 0x04, ry);
    writeU32(outVec3, 0x08, rz);
    return outVec3;
}

// 0x005636e0 — MinMaxAABox::vec3SignMaskMax
void* vec3SignMaskMax(void* outVec3, const void* aVec3, const void* bVec3) {
    const float ax = readF32(aVec3, 0x00);
    const float ay = readF32(aVec3, 0x04);
    const float az = readF32(aVec3, 0x08);
    const float bx = readF32(bVec3, 0x00);
    const float by = readF32(bVec3, 0x04);
    const float bz = readF32(bVec3, 0x08);

    const float dx = ax - bx;
    const float dy = ay - by;
    const float dz = az - bz;

    const std::uint32_t mx = static_cast<std::uint32_t>(static_cast<std::int32_t>(floatToBits(dx)) >> 31);
    const std::uint32_t my = static_cast<std::uint32_t>(static_cast<std::int32_t>(floatToBits(dy)) >> 31);
    const std::uint32_t mz = static_cast<std::uint32_t>(static_cast<std::int32_t>(floatToBits(dz)) >> 31);

    const std::uint32_t rx = (floatToBits(bx) & mx) | (floatToBits(ax) & ~mx);
    const std::uint32_t ry = (floatToBits(by) & my) | (floatToBits(ay) & ~my);
    const std::uint32_t rz = (floatToBits(bz) & mz) | (floatToBits(az) & ~mz);

    writeU32(outVec3, 0x00, rx);
    writeU32(outVec3, 0x04, ry);
    writeU32(outVec3, 0x08, rz);
    return outVec3;
}

// 0x00564e70 — AABoxClass::initFromMinMax
void aaboxFromMinMax(void* outCenterExtentBox, const void* inMinMaxBox) {
    const float minX = readF32(inMinMaxBox, 0x00);
    const float minY = readF32(inMinMaxBox, 0x04);
    const float minZ = readF32(inMinMaxBox, 0x08);
    const float maxX = readF32(inMinMaxBox, 0x0c);
    const float maxY = readF32(inMinMaxBox, 0x10);
    const float maxZ = readF32(inMinMaxBox, 0x14);

    const float cx = (maxX + minX) * 0.5f;
    const float cy = (maxY + minY) * 0.5f;
    const float cz = (maxZ + minZ) * 0.5f;
    writeF32(outCenterExtentBox, 0x00, cx);
    writeF32(outCenterExtentBox, 0x04, cy);
    writeF32(outCenterExtentBox, 0x08, cz);

    const float ex = (maxX - minX) * 0.5f;
    const float ey = (maxY - minY) * 0.5f;
    const float ez = (maxZ - minZ) * 0.5f;
    writeF32(outCenterExtentBox, 0x0c, ex);
    writeF32(outCenterExtentBox, 0x10, ey);
    writeF32(outCenterExtentBox, 0x14, ez);
}

// 0x00564f20 — MinMaxAABox::mergePoint
void minMaxAABoxMergePoint(void* minMaxBox, const void* point3D) {
    std::uint8_t tmp[12];
    vec3SignMaskMin(tmp, minMaxBox, point3D);
    writeU32(minMaxBox, 0x00, readU32(tmp, 0x00));
    writeU32(minMaxBox, 0x04, readU32(tmp, 0x04));
    writeU32(minMaxBox, 0x08, readU32(tmp, 0x08));

    auto* maxPart = static_cast<std::uint8_t*>(minMaxBox) + 0x0c;
    vec3SignMaskMax(tmp, maxPart, point3D);
    writeU32(maxPart, 0x00, readU32(tmp, 0x00));
    writeU32(maxPart, 0x04, readU32(tmp, 0x04));
    writeU32(maxPart, 0x08, readU32(tmp, 0x08));
}

// 0x00564f80 — MinMaxAABox::mergeCenterExtentBox
void minMaxAABoxMergeCenterExtentBox(void* minMaxBox, const void* centerExtentBox) {
    const float ex = readF32(centerExtentBox, 0x0c);
    const float ey = readF32(centerExtentBox, 0x10);
    const float ez = readF32(centerExtentBox, 0x14);
    if (ex == 0.0f && ey == 0.0f && ez == 0.0f) {
        return;
    }

    const float cx = readF32(centerExtentBox, 0x00);
    const float cy = readF32(centerExtentBox, 0x04);
    const float cz = readF32(centerExtentBox, 0x08);

    float boxMin[3] = { cx - ex, cy - ey, cz - ez };
    float tmp[3];
    vec3SignMaskMin(tmp, minMaxBox, boxMin);
    writeU32(minMaxBox, 0x00, floatToBits(tmp[0]));
    writeU32(minMaxBox, 0x04, floatToBits(tmp[1]));
    writeU32(minMaxBox, 0x08, floatToBits(tmp[2]));

    float boxMax[3] = { cx + ex, cy + ey, cz + ez };
    auto* maxPart = static_cast<std::uint8_t*>(minMaxBox) + 0x0c;
    vec3SignMaskMax(tmp, maxPart, boxMax);
    writeU32(maxPart, 0x00, floatToBits(tmp[0]));
    writeU32(maxPart, 0x04, floatToBits(tmp[1]));
    writeU32(maxPart, 0x08, floatToBits(tmp[2]));
}

// 0x00565130 — BaseHeightMapRenderObjClass::packCompressedTerrainVertex
void packCompressedTerrainVertex(
    const void* renderObj,
    void* outVertex24,
    const void* posVec3,
    const void* normalVec3,
    const void* texParams6,
    const void* blendParams2) {
    (void)renderObj;

    const float px = readF32(posVec3, 0x00);
    const float py = readF32(posVec3, 0x04);
    const float pz = readF32(posVec3, 0x08);
    writeU16(outVertex24, 0x00, static_cast<std::uint16_t>(static_cast<std::int32_t>(px)));
    writeU16(outVertex24, 0x02, static_cast<std::uint16_t>(static_cast<std::int32_t>(py)));
    writeU16(outVertex24, 0x04, static_cast<std::uint16_t>(static_cast<std::int32_t>(pz)));

    const float bp0 = readF32(blendParams2, 0x00);
    const float bp1 = readF32(blendParams2, 0x04);

    const std::uint8_t b6 = static_cast<std::uint8_t>(static_cast<std::int32_t>(bp0 + 1.0f));
    writeU16(outVertex24, 0x06, static_cast<std::uint16_t>(b6));

    const std::uint8_t nzByte = static_cast<std::uint8_t>(static_cast<std::int32_t>((readF32(normalVec3, 0x08) + 1.0f) * 100.0f));
    const std::uint8_t bp1Byte = static_cast<std::uint8_t>(static_cast<std::int32_t>(bp1 + 1.0f));
    const std::uint8_t nyByte = static_cast<std::uint8_t>(static_cast<std::int32_t>((readF32(normalVec3, 0x04) + 1.0f) * 100.0f));
    const std::uint8_t nxByte = static_cast<std::uint8_t>(static_cast<std::int32_t>((readF32(normalVec3, 0x00) + 1.0f) * 100.0f));

    const std::uint32_t packedNormalWord =
        (static_cast<std::uint32_t>(bp1Byte) << 24) |
        (static_cast<std::uint32_t>(nzByte) << 16) |
        (static_cast<std::uint32_t>(nyByte) << 8) |
        static_cast<std::uint32_t>(nxByte);
    writeU32(outVertex24, 0x08, packedNormalWord);

    constexpr float kScale30000 = 30000.0f; // 0x00be3b14
    writeU16(outVertex24, 0x0c, static_cast<std::uint16_t>(static_cast<std::int32_t>(readF32(texParams6, 0x00) * kScale30000)));
    writeU16(outVertex24, 0x0e, static_cast<std::uint16_t>(static_cast<std::int32_t>(readF32(texParams6, 0x04) * kScale30000)));
    writeU16(outVertex24, 0x10, static_cast<std::uint16_t>(static_cast<std::int32_t>(readF32(texParams6, 0x08) * kScale30000)));
    writeU16(outVertex24, 0x12, static_cast<std::uint16_t>(static_cast<std::int32_t>(readF32(texParams6, 0x0c) * kScale30000)));
    writeU16(outVertex24, 0x14, static_cast<std::uint16_t>(static_cast<std::int32_t>(readF32(texParams6, 0x10) * kScale30000)));
    writeU16(outVertex24, 0x16, static_cast<std::uint16_t>(static_cast<std::int32_t>(readF32(texParams6, 0x14) * kScale30000)));
}

// 0x00565700 — BaseHeightMapRenderObjClass::copyTerrainRenderState52
void copyTerrainRenderState52(void* dst52, const void* src52) {
    for (std::ptrdiff_t off = 0; off < 0x34; off += 4) {
        x87CopyDword(dst52, off, src52, off);
    }
}

// 0x005615a0 — W3DScorchBuffer::clearAllScorches
void clearAllScorches(void* scorchBuffer) {
    writeU32(scorchBuffer, 0x4660, 0u);
    writeU32(scorchBuffer, 0x4664, 0u);
    writeU32(scorchBuffer, 0x4668, 0u);
}

// 0x0056ab00 — W3DScorchBuffer::invalidateScorchesInRadius
void invalidateScorchesInRadius(void* scorchBuffer, const void* centerPos, float radius) {
    const std::int32_t count = readI32(scorchBuffer, 0x4660);
    if (count > 0) {
        const float cx = readF32(centerPos, 0x00);
        const float cy = readF32(centerPos, 0x04);
        const float cz = readF32(centerPos, 0x08);
        for (std::int32_t i = 0; i < count; ++i) {
            const std::ptrdiff_t entryOff = 0x14 + static_cast<std::ptrdiff_t>(i) * 0x24;
            const double dx = static_cast<double>(cx) - static_cast<double>(readF32(scorchBuffer, entryOff + 0x00));
            const double dy = static_cast<double>(cy) - static_cast<double>(readF32(scorchBuffer, entryOff + 0x04));
            const double dz = static_cast<double>(cz) - static_cast<double>(readF32(scorchBuffer, entryOff + 0x08));
            const double dist = std::sqrt(dx * dx + dy * dy + dz * dz);
            const double maxDist = static_cast<double>(readF32(scorchBuffer, entryOff + 0x0c)) + static_cast<double>(radius);
            if (maxDist > dist) {
                writeU8(scorchBuffer, entryOff + 0x1c, 1u);
            }
        }
    }
    writeU32(scorchBuffer, 0x4664, 0u);
}

// 0x005612f0 — W3DRoadBuffer::clearRoadTypeLinks
void clearRoadTypeLinks(void* roadBuffer) {
    const std::int32_t numRoads = readI32(roadBuffer, 0x08);
    if (numRoads > 0) {
        for (std::int32_t i = 0; i < numRoads; ++i) {
            auto* roads = static_cast<std::uint8_t*>(readMutPtr(roadBuffer, 0x04));
            writeU32(roads, static_cast<std::ptrdiff_t>(i) * 0x7c + 0x44, 0u);
        }
    }
    const std::int32_t numTypes = readI32(roadBuffer, 0x34);
    if (numTypes > 0) {
        for (std::int32_t j = 0; j < numTypes; ++j) {
            auto* types = static_cast<std::uint8_t*>(readMutPtr(roadBuffer, 0x00));
            writeU32(types, static_cast<std::ptrdiff_t>(j) * 0xa0 + 0x04, 0u);
        }
    }
}

// 0x00561340 — W3DRoadBuffer::adjustRoadStacking
void adjustRoadStacking(void* roadBuffer, std::int32_t topUniqueID, std::int32_t bottomUniqueID) {
    if (readU8(roadBuffer, 0x40) == 0u) {
        return;
    }
    const std::int32_t numTypes = readI32(roadBuffer, 0x34);
    auto* types = static_cast<std::uint8_t*>(readMutPtr(roadBuffer, 0x00));

    std::int32_t topIdx = 0;
    for (; topIdx < numTypes; ++topIdx) {
        if (readI32(types, static_cast<std::ptrdiff_t>(topIdx) * 0xa0 + 0x14) == topUniqueID) {
            break;
        }
    }
    if (topIdx >= numTypes) {
        return;
    }

    std::int32_t bottomIdx = 0;
    for (; bottomIdx < numTypes; ++bottomIdx) {
        if (readI32(types, static_cast<std::ptrdiff_t>(bottomIdx) * 0xa0 + 0x14) == bottomUniqueID) {
            break;
        }
    }
    if (bottomIdx >= numTypes) {
        return;
    }

    const std::int32_t bottomStacking = readI32(types, static_cast<std::ptrdiff_t>(bottomIdx) * 0xa0 + 0x1c);
    if (readI32(types, static_cast<std::ptrdiff_t>(topIdx) * 0xa0 + 0x1c) > bottomStacking) {
        return;
    }

    for (std::int32_t j = 0; j < readI32(roadBuffer, 0x34); ++j) {
        auto* curTypes = static_cast<std::uint8_t*>(readMutPtr(roadBuffer, 0x00));
        const std::ptrdiff_t off = static_cast<std::ptrdiff_t>(j) * 0xa0 + 0x1c;
        const std::int32_t cur = readI32(curTypes, off);
        if (cur > bottomStacking) {
            writeI32(curTypes, off, cur + 1);
        }
    }
    auto* finalTypes = static_cast<std::uint8_t*>(readMutPtr(roadBuffer, 0x00));
    writeI32(finalTypes, static_cast<std::ptrdiff_t>(topIdx) * 0xa0 + 0x1c, bottomStacking + 1);
}

// 0x00569570 — W3DRoadBuffer::linkRoadsToRoadTypes
void linkRoadsToRoadTypes(void* roadBuffer) {
    clearRoadTypeLinks(roadBuffer);
    for (std::int32_t i = 0; i < readI32(roadBuffer, 0x08); ++i) {
        const std::ptrdiff_t roadOff = static_cast<std::ptrdiff_t>(i) * 0x7c;
        for (std::int32_t j = 0; j < readI32(roadBuffer, 0x34); ++j) {
            const std::ptrdiff_t typeOff = static_cast<std::ptrdiff_t>(j) * 0xa0;
            auto* types = static_cast<std::uint8_t*>(readMutPtr(roadBuffer, 0x00));
            auto* roads = static_cast<std::uint8_t*>(readMutPtr(roadBuffer, 0x04));
            if (readI32(roads, roadOff + 0x54) == readI32(types, typeOff + 0x14)) {
                writeU32(roads, roadOff + 0x44, readU32(types, typeOff + 0x04));
                const auto roadPtrVal = reinterpret_cast<std::uintptr_t>(roads + roadOff);
                writeU32(types, typeOff + 0x04, roadPtrVal);
            }
        }
    }
}

// 0x005695e0 — W3DRoadBuffer::updateRoadSegmentCounts
void updateRoadSegmentCounts(void* roadBuffer, void* roadSegment) {
    if (readU8(roadBuffer, 0x40) == 0u) {
        return;
    }
    const float p0x = readF32(roadSegment, 0x00);
    const float p0y = readF32(roadSegment, 0x04);
    writeU8(roadSegment, 0x1c, 1u);
    writeU8(roadSegment, 0x3c, 1u);
    writeU8(roadSegment, 0x1d, 0u);
    writeU8(roadSegment, 0x3d, 0u);
    writeI32(roadSegment, 0x18, 0);
    writeI32(roadSegment, 0x38, 0);
    const float p1x = readF32(roadSegment, 0x20);
    const float p1y = readF32(roadSegment, 0x24);

    for (std::int32_t i = 0; i < readI32(roadBuffer, 0x08); ++i) {
        auto* other = static_cast<std::uint8_t*>(readMutPtr(roadBuffer, 0x04)) + static_cast<std::ptrdiff_t>(i) * 0x7c;
        if (readI32(other, 0x54) != readI32(roadSegment, 0x54)) {
            continue;
        }
        if (readF32(other, 0x00) == p0x && readF32(other, 0x04) == p0y) {
            writeI32(other, 0x18, readI32(other, 0x18) + 1);
            writeI32(roadSegment, 0x18, readI32(roadSegment, 0x18) + 1);
        }
        if (readF32(other, 0x00) == p1x && readF32(other, 0x04) == p1y) {
            writeI32(other, 0x18, readI32(other, 0x18) + 1);
            writeI32(roadSegment, 0x38, readI32(roadSegment, 0x38) + 1);
        }
        if (readF32(other, 0x20) == p0x && readF32(other, 0x24) == p0y) {
            writeI32(other, 0x38, readI32(other, 0x38) + 1);
            writeI32(roadSegment, 0x18, readI32(roadSegment, 0x18) + 1);
        }
        if (readF32(other, 0x20) == p1x && readF32(other, 0x24) == p1y) {
            writeI32(other, 0x38, readI32(other, 0x38) + 1);
            writeI32(roadSegment, 0x38, readI32(roadSegment, 0x38) + 1);
        }
        writeU8(other, 0x1d, readI32(other, 0x18) > 1 ? 1u : 0u);
        writeU8(other, 0x3d, readI32(other, 0x38) > 1 ? 1u : 0u);
    }
    writeU8(roadSegment, 0x1d, readI32(roadSegment, 0x18) > 1 ? 1u : 0u);
    writeU8(roadSegment, 0x3d, readI32(roadSegment, 0x38) > 1 ? 1u : 0u);
}

// 0x00569750 — W3DRoadBuffer::updateAllRoadCountsAndFlags
void updateAllRoadCountsAndFlags(void* roadBuffer) {
    if (readU8(roadBuffer, 0x40) == 0u) {
        return;
    }
    const std::int32_t numRoads = readI32(roadBuffer, 0x08);
    for (std::int32_t i = 0; i < numRoads; ++i) {
        auto* seg = static_cast<std::uint8_t*>(readMutPtr(roadBuffer, 0x04)) + static_cast<std::ptrdiff_t>(i) * 0x7c;
        writeU8(seg, 0x1c, 1u);
        writeU8(seg, 0x3c, 1u);
        writeI32(seg, 0x18, 0);
        writeI32(seg, 0x38, 0);
    }

    for (std::int32_t ebp = numRoads - 1; ebp > 0; --ebp) {
        auto* baseRoads = static_cast<std::uint8_t*>(readMutPtr(roadBuffer, 0x04));
        auto* segA = baseRoads + static_cast<std::ptrdiff_t>(ebp) * 0x7c;
        const float p0x = readF32(segA, 0x00);
        const float p0y = readF32(segA, 0x04);
        const float p1x = readF32(segA, 0x20);
        const float p1y = readF32(segA, 0x24);

        for (std::int32_t j = 0; j < ebp; ++j) {
            auto* segB = baseRoads + static_cast<std::ptrdiff_t>(j) * 0x7c;
            if (readI32(segB, 0x54) != readI32(segA, 0x54)) {
                continue;
            }
            if (readF32(segB, 0x00) == p0x && readF32(segB, 0x04) == p0y) {
                writeU8(segB, 0x1c, 0u);
                writeI32(segB, 0x18, readI32(segB, 0x18) + 1);
                writeI32(segA, 0x18, readI32(segA, 0x18) + 1);
            }
            if (readF32(segB, 0x00) == p1x && readF32(segB, 0x04) == p1y) {
                writeU8(segB, 0x1c, 0u);
                writeI32(segB, 0x18, readI32(segB, 0x18) + 1);
                writeI32(segA, 0x38, readI32(segA, 0x38) + 1);
            }
            if (readF32(segB, 0x20) == p0x && readF32(segB, 0x24) == p0y) {
                writeU8(segB, 0x3c, 0u);
                writeI32(segB, 0x38, readI32(segB, 0x38) + 1);
                writeI32(segA, 0x18, readI32(segA, 0x18) + 1);
            }
            if (readF32(segB, 0x20) == p1x && readF32(segB, 0x24) == p1y) {
                writeU8(segB, 0x3c, 0u);
                writeI32(segB, 0x38, readI32(segB, 0x38) + 1);
                writeI32(segA, 0x38, readI32(segA, 0x38) + 1);
            }
        }
    }
}

// 0x0056a8a0 — W3DRoadBuffer::rotateRoadPointAbout
void rotateRoadPointAbout(void* roadBuffer, void* ptVec2, float centerX, float centerY, float angle) {
    (void)roadBuffer;
    const float px = readF32(ptVec2, 0x00);
    const float py = readF32(ptVec2, 0x04);
    const float dx = px - centerX;
    const float dy = py - centerY;
    const double rad = static_cast<double>(angle);
    const float s = static_cast<float>(std::sin(rad));
    const float c = static_cast<float>(std::cos(rad));
    const float rotX = dx * c - dy * s;
    const float rotY = dx * s + dy * c;
    const float deltaX = rotX - dx;
    const float deltaY = rotY - dy;
    writeF32(ptVec2, 0x00, px + deltaX);
    writeF32(ptVec2, 0x04, py + deltaY);
}

// 0x00568ea0 — RoadSegment::RoadSegment()
void* initRoadSegment(void* roadSegment) {
    for (std::ptrdiff_t off = 0x00; off <= 0x40; off += 4) {
        writeU32(roadSegment, off, 0u);
    }
    // Note: +0x44, +0x59..+0x5b are left untouched by 0x00568ea0 in ra3_1.0.game
    writeU32(roadSegment, 0x54, 0u);
    writeU8(roadSegment, 0x58, 0u);
    writeU32(roadSegment, 0x5c, 0u);
    writeU32(roadSegment, 0x60, 0u);
    writeU32(roadSegment, 0x64, 0u);
    writeU32(roadSegment, 0x68, 0u);
    writeU32(roadSegment, 0x48, 0u);
    writeU32(roadSegment, 0x4c, 0u);
    writeU32(roadSegment, 0x50, 0u);
    writeU32(roadSegment, 0x6c, 0u);
    writeU32(roadSegment, 0x70, 0u);
    writeU32(roadSegment, 0x74, 0u);
    writeU32(roadSegment, 0x78, 0u);
    return roadSegment;
}

// 0x00569480 — RoadSegment::operator=
void* copyRoadSegment(void* dstSegment, const void* srcSegment) {
    for (std::ptrdiff_t off = 0x00; off <= 0x14; off += 4) {
        x87CopyDword(dstSegment, off, srcSegment, off);
    }
    writeU32(dstSegment, 0x18, readU32(srcSegment, 0x18));
    writeU8(dstSegment, 0x1c, readU8(srcSegment, 0x1c));
    writeU8(dstSegment, 0x1d, readU8(srcSegment, 0x1d));
    writeU8(dstSegment, 0x1e, readU8(srcSegment, 0x1e));
    writeU8(dstSegment, 0x1f, readU8(srcSegment, 0x1f));
    for (std::ptrdiff_t off = 0x20; off <= 0x34; off += 4) {
        x87CopyDword(dstSegment, off, srcSegment, off);
    }
    writeU32(dstSegment, 0x38, readU32(srcSegment, 0x38));
    writeU8(dstSegment, 0x3c, readU8(srcSegment, 0x3c));
    writeU8(dstSegment, 0x3d, readU8(srcSegment, 0x3d));
    writeU8(dstSegment, 0x3e, readU8(srcSegment, 0x3e));
    writeU8(dstSegment, 0x3f, readU8(srcSegment, 0x3f));
    writeU32(dstSegment, 0x40, readU32(srcSegment, 0x40));
    writeU32(dstSegment, 0x44, readU32(srcSegment, 0x44));
    x87CopyDword(dstSegment, 0x48, srcSegment, 0x48);
    x87CopyDword(dstSegment, 0x4c, srcSegment, 0x4c);
    x87CopyDword(dstSegment, 0x50, srcSegment, 0x50);
    writeU32(dstSegment, 0x54, readU32(srcSegment, 0x54));
    writeU8(dstSegment, 0x58, readU8(srcSegment, 0x58));
    // Note: +0x59..+0x5b are left untouched by 0x00569480 in ra3_1.0.game
    writeU32(dstSegment, 0x5c, readU32(srcSegment, 0x5c));
    writeU32(dstSegment, 0x60, readU32(srcSegment, 0x60));
    writeU32(dstSegment, 0x64, readU32(srcSegment, 0x64));
    writeU32(dstSegment, 0x68, readU32(srcSegment, 0x68));
    x87CopyDword(dstSegment, 0x6c, srcSegment, 0x6c);
    x87CopyDword(dstSegment, 0x70, srcSegment, 0x70);
    x87CopyDword(dstSegment, 0x74, srcSegment, 0x74);
    x87CopyDword(dstSegment, 0x78, srcSegment, 0x78);
    return dstSegment;
}

// 0x0058d1b0 — W3DPropBuffer::W3DPropBuffer()
void* initPropBuffer(void* propBuffer) {
    writeU32(propBuffer, 0x00, 0x00be4d60u);
    writeU32(propBuffer, 0x04, 0u);
    writeU32(propBuffer, 0x08, 0u);
    writeU32(propBuffer, 0x0c, 0u);
    writeU32(propBuffer, 0x14, 0u);
    writeU32(propBuffer, 0x18, 0u);
    writeU32(propBuffer, 0x1c, 0u);
    writeU8(propBuffer, 0x24, 0u);
    writeU8(propBuffer, 0x25, 0u);
    writeU32(propBuffer, 0x28, 0u);
    writeU32(propBuffer, 0x2c, 0u);
    writeU32(propBuffer, 0x30, 0u);
    writeU32(propBuffer, 0x34, 0u);
    writeU32(propBuffer, 0x38, 0u);
    return propBuffer;
}

} // namespace ra3::render
