#include <ra3/sim/TerrainLogic.h>

#include <cmath>
#include <cstring>
#include <xmmintrin.h>

namespace ra3::sim {
namespace {

inline constexpr const char* kTerrainLogicSourcePath =
    "d:\\p4\\sage_main\\sageengine\\source\\gameengine\\gamelogic\\map\\TerrainLogic.cpp";
inline constexpr int kFindPositionAroundSourceLine = 0x0C56;  // 3158
inline constexpr const char* kTerrainLogicSubsystemLiteral = "TerrainLogic";

inline constexpr float kZero = 0.0F;
inline constexpr float kNegZero = -0.0F;                        // 0x00BDC008 (0x80000000)
inline constexpr float kOne = 1.0F;
inline constexpr float kNegOne = -1.0F;                         // 0x00BD9628 (0xBF800000)
inline constexpr float kHalf = 0.5F;
inline constexpr float kTen = 10.0F;
inline constexpr float kTwenty = 20.0F;
inline constexpr float kSideOffset = 10.19999980926513671875F;  // 0x00C4F4D4 (0x41233333)
inline constexpr float kBridgeLayerHeightBias = 2.5F;          // 0x00BE0C2C (0x40200000)
inline constexpr float kLayerHeightEpsilon = 0.00019999999494757503F;  // 0x00BE625C (0x3951B717)
inline constexpr float kOneSixth = 0.16666667163372039794921875F;      // 0x00BE6F10 (0x3E2AAAAB)
inline constexpr float kThousand = 1000.0F;                    // 0x00BD95A8 (0x447A0000)
inline constexpr float kClipEpsilon = 9.99999974737875163555e-05F;     // 0x00C4F1FC (0x38D1B717)
inline constexpr float kNegEpsilon = -9.99999997475242707878e-07F;     // 0x00BE34F0 (0xB58637BD)
inline constexpr float kPosEpsilon = 9.99999997475242707878e-07F;      // 0x00BDF8AC (0x358637BD)

// Matches the shipped x87 reciprocal-square-root idiom:
//   fld dword ptr [lenSq]; fsqrt; fld1; fdivrp st(1), st(0); fstp dword ptr [invLen]
float x87ReciprocalSqrt(float lenSq) noexcept {
#if defined(_MSC_VER) && defined(_M_IX86)
    float resultVal = 0.0F;
    __asm {
        fld dword ptr [lenSq]
        fsqrt
        fld1
        fdivrp st(1), st(0)
        fstp dword ptr [resultVal]
    }
    return resultVal;
#else
    return 1.0F / std::sqrt(lenSq);
#endif
}

float x87Sqrt(float val) noexcept {
#if defined(_MSC_VER) && defined(_M_IX86)
    float resultVal = 0.0F;
    __asm {
        fld dword ptr [val]
        fsqrt
        fstp dword ptr [resultVal]
    }
    return resultVal;
#else
    return std::sqrt(val);
#endif
}

void copyFloatViaX87(const float* src, float* dst) noexcept {
#if defined(_MSC_VER) && defined(_M_IX86)
    __asm {
        mov eax, dword ptr [src]
        mov edx, dword ptr [dst]
        fld dword ptr [eax]
        fstp dword ptr [edx]
    }
#else
    *dst = *src;
#endif
}

// Note: _mm_comi* intrinsics match the exact x86 COMISS / UCOMISS flag behavior.
bool sseGt(float a, float b) noexcept {
    return _mm_comigt_ss(_mm_set_ss(a), _mm_set_ss(b)) != 0;
}

bool sseGe(float a, float b) noexcept {
    return _mm_comige_ss(_mm_set_ss(a), _mm_set_ss(b)) != 0;
}

bool sseLt(float a, float b) noexcept {
    // comiss a, b; jb -> CF=1 (true if a < b OR unordered/NaN!)
    return _mm_comige_ss(_mm_set_ss(a), _mm_set_ss(b)) == 0;
}

bool sseLe(float a, float b) noexcept {
    // comiss a, b; jbe -> CF=1 or ZF=1 (true if a <= b OR unordered/NaN!)
    return _mm_comigt_ss(_mm_set_ss(a), _mm_set_ss(b)) == 0;
}

bool sseEqOrdered(float a, float b) noexcept {
    // ucomiss a, b; lahf; test ah, 0x44; jp false (true iffZF=1 and PF=0)
    const __m128 va = _mm_set_ss(a);
    const __m128 vb = _mm_set_ss(b);
    return _mm_ucomieq_ss(va, vb) != 0 && _mm_ucomineq_ss(va, vb) == 0;
}

std::uint32_t computeRegionOutcode(float x, float y, const Region2D& r) noexcept {
    std::uint32_t code = 0;
    if (sseGt(r.lo.x, x)) {
        code = 1;
    } else if (sseGt(x, r.hi.x)) {
        code = 2;
    }
    if (sseGt(r.lo.y, y)) {
        code |= 8;
    } else if (sseGt(y, r.hi.y)) {
        code |= 4;
    }
    return code;
}

}  // namespace

// ra3_1.0.game 0x0085D7E0
FindPositionOptions* initializeFindPositionOptions(FindPositionOptions& options) noexcept {
    for (std::uint32_t& word : options.words00) {
        word = 0;
    }
    options.minRadius = 0.0F;
    options.maxRadius = 0.0F;
    options.startAngle = kRandomStartAngle;
    options.maxZDelta = kMaxFindPositionZDelta;
    options.ignoreObject = nullptr;
    options.sourceToPathToDest = nullptr;
    options.relationshipObject = nullptr;
    return &options;
}

// ra3_1.0.game 0x0085D840
BridgeInfo* initializeBridgeInfo(BridgeInfo& info) noexcept {
    info.fromLeft = {0.0F, 0.0F, 0.0F};
    info.fromRight = {0.0F, 0.0F, 0.0F};
    info.toLeft = {0.0F, 0.0F, 0.0F};
    info.toRight = {0.0F, 0.0F, 0.0F};
    info.bridgeObjectId = 0;
    return &info;
}

// ra3_1.0.game 0x0085D110
bool pointInTriangle2D(
    const Coord3D& p0, const Coord3D& p1, const Coord3D& p2, const Coord3D& pt,
    int axis1, int axis2, std::uint8_t& flags) noexcept {
    const float* const v0 = &p0.x;
    const float* const v1 = &p1.x;
    const float* const v2 = &p2.x;
    const float* const vp = &pt.x;

    const float p0u = v0[axis1];
    const float p0v = v0[axis2];
    const float p1u = v1[axis1];
    const float p1v = v1[axis2];
    const float p2u = v2[axis1];
    const float p2v = v2[axis2];

    const float e01u = p1u - p0u;
    const float e01v = p1v - p0v;
    const float e12u = p2u - p1u;
    const float e12v = p2v - p1v;
    const float e20u = p0u - p2u;
    const float e20v = p0v - p2v;

    const float area2 = (p2u - p0u) * e01v - (p2v - p0v) * e01u;
    if (!sseEqOrdered(area2, kZero)) {
        const float sign = sseLe(area2, kZero) ? -1.0F : kOne;
        const float ptu = vp[axis1];
        const float ptv = vp[axis2];

        const float c0 = (ptu - p0u) * e01v - (ptv - p0v) * e01u;
        if (sseGt(kZero, c0 * sign)) {
            return false;
        }

        const float c1 = e12v * (ptu - p1u) - (ptv - p1v) * e12u;
        if (sseGt(kZero, c1 * sign)) {
            return false;
        }

        const float c2 = e20v * (ptu - p2u) - (ptv - p2v) * e20u;
        if (sseGt(kZero, c2 * sign)) {
            return false;
        }

        if (sseEqOrdered(c0, kZero) || sseEqOrdered(c1, kZero) || sseEqOrdered(c2, kZero)) {
            flags |= 1U;
        }
        return true;
    }

    // Degenerate / colinear triangle fallback (0x0085D2BE..0x0085D40A).
    const float len01Sq = e01u * e01u + e01v * e01v;
    const float len12Sq = e12v * e12v + e12u * e12u;
    const float len20Sq = len12Sq;

    float segDu = 0.0F;
    float segDv = 0.0F;
    float relU = 0.0F;
    float relV = 0.0F;
    float segLenSq = 0.0F;

    if (!sseLe(len01Sq, len12Sq) && !sseLe(len01Sq, len20Sq)) {
        segDu = e01u;
        segDv = e01v;
        relU = vp[axis1] - p0u;
        relV = vp[axis2] - p0v;
        segLenSq = len01Sq;
    } else if (!sseLe(len12Sq, len20Sq)) {
        segDu = e12u;
        segDv = e12v;
        relU = vp[axis1] - p1u;
        relV = vp[axis2] - p1v;
        segLenSq = len12Sq;
    } else {
        segDu = e20u;
        segDv = e20v;
        relU = vp[axis1] - p2u;
        relV = vp[axis2] - p2v;
        segLenSq = len20Sq;
    }

    if (sseEqOrdered(segLenSq, kZero)) {
        const float distSq = relV * relV + relU * relU;
        if (sseEqOrdered(distSq, kZero)) {
            flags |= 1U;
            return true;
        }
        return false;
    }

    const float cross = segDv * relU - relV * segDu;
    if (!sseEqOrdered(cross, kZero)) {
        return false;
    }

    const float distFromStartSq = relV * relV + relU * relU;
    if (sseLt(segLenSq, distFromStartSq)) {
        return false;
    }

    const float diffU = relU - segDu;
    const float diffV = relV - segDv;
    const float distFromEndSq = diffV * diffV + diffU * diffU;
    if (sseLt(segLenSq, distFromEndSq)) {
        return false;
    }

    flags |= 1U;
    return true;
}

bool pointInTriangle2D(
    const Coord3D& p0, const Coord3D& p1, const Coord3D& p2, const Coord3D& pt) noexcept {
    std::uint8_t flags = 0;
    return pointInTriangle2D(p0, p1, p2, pt, 0, 1, flags);
}

// ra3_1.0.game 0x0085DA50
bool lineInRegion2D(
    const Coord2D* p1, const Coord2D* p2, const Region2D* clipRegion) noexcept {
    float x1 = p1->x;
    float y1 = p1->y;
    float x2 = p2->x;
    float y2 = p2->y;

    const std::uint32_t code1 = computeRegionOutcode(x1, y1, *clipRegion);
    const std::uint32_t code2 = computeRegionOutcode(x2, y2, *clipRegion);

    if ((code1 | code2) == 0U) {
        return true;
    }
    if ((code1 & code2) != 0U) {
        return false;
    }

    if (code1 != 0U) {
        if ((code1 & 8U) != 0U) {
            const float dy = y2 - y1;
            if (sseEqOrdered(dy, kZero)) {
                return false;
            }
            x1 = ((clipRegion->lo.y - y1) / dy) * (x2 - x1) + x1;
            y1 = clipRegion->lo.y;
        } else if ((code1 & 4U) != 0U) {
            const float dy = y2 - y1;
            if (sseEqOrdered(dy, kZero)) {
                return false;
            }
            x1 = ((clipRegion->hi.y - y1) / dy) * (x2 - x1) + x1;
            y1 = clipRegion->hi.y;
        }

        if (sseGt(x1, clipRegion->hi.x)) {
            const float dx = x2 - x1;
            if (sseEqOrdered(dx, kZero)) {
                return false;
            }
            y1 = ((clipRegion->hi.x - x1) / dx) * (y2 - y1) + y1;
            x1 = clipRegion->hi.x;
        } else if (sseGt(clipRegion->lo.x, x1)) {
            const float dx = x2 - x1;
            if (sseEqOrdered(dx, kZero)) {
                return false;
            }
            y1 = ((clipRegion->lo.x - x1) / dx) * (y2 - y1) + y1;
            x1 = clipRegion->lo.x;
        }
    }

    if (code2 != 0U) {
        if ((code2 & 8U) != 0U) {
            const float dy = y2 - y1;
            if (sseEqOrdered(dy, kZero)) {
                return false;
            }
            x2 = ((x2 - x1) / dy) * (clipRegion->lo.y - y2) + x2;
            y2 = clipRegion->lo.y;
        } else if ((code2 & 4U) != 0U) {
            const float dy = y2 - y1;
            if (sseEqOrdered(dy, kZero)) {
                return false;
            }
            x2 = ((x2 - x1) / dy) * (clipRegion->hi.y - y2) + x2;
            y2 = clipRegion->hi.y;
        }

        if (sseGt(x2, clipRegion->hi.x)) {
            const float dx = x2 - x1;
            if (sseEqOrdered(dx, kZero)) {
                return false;
            }
            y2 = ((clipRegion->hi.x - x2) / dx) * (y2 - y1) + y2;
            x2 = clipRegion->hi.x;
        } else if (sseGt(clipRegion->lo.x, x2)) {
            const float dx = x2 - x1;
            if (sseEqOrdered(dx, kZero)) {
                return false;
            }
            y2 = ((clipRegion->lo.x - x2) / dx) * (y2 - y1) + y2;
            x2 = clipRegion->lo.x;
        }
    }

    if (sseLt(x1, clipRegion->lo.x) || sseLt(clipRegion->hi.x, x1) ||
        sseLt(y1, clipRegion->lo.y) || sseLt(clipRegion->hi.y, y1) ||
        sseLt(x2, clipRegion->lo.x) || sseLt(clipRegion->hi.x, x2) ||
        sseLt(y2, clipRegion->lo.y) || sseLt(clipRegion->hi.y, y2)) {
        return false;
    }
    return true;
}

// ra3_1.0.game 0x0085EC40
void setPlaneFromPoints(
    Plane3D& plane, const Coord3D& v1, const Coord3D& v2, const Coord3D& v3) noexcept {
    const float ax = v2.x - v1.x;
    const float ay = v2.y - v1.y;
    const float az = v2.z - v1.z;

    const float bx = v3.x - v1.x;
    const float by = v3.y - v1.y;
    const float bz = v3.z - v1.z;

    const float nx = ay * bz - az * by;
    const float ny = az * bx - ax * bz;
    const float nz = ax * by - ay * bx;

    plane.normal.x = nx;
    plane.normal.y = ny;
    plane.normal.z = nz;

    if (sseEqOrdered(nx, kZero) && sseEqOrdered(ny, kZero) && sseEqOrdered(nz, kZero)) {
        plane.normal.x = kZero;
        plane.normal.y = kZero;
        plane.normal.z = kOne;
        plane.dist = kZero;
        return;
    }

    const float lenSq = (nz * nz + ny * ny) + nx * nx;
    const float invLen = sseEqOrdered(lenSq, kZero) ? kZero : x87ReciprocalSqrt(lenSq);
    plane.normal.x = nx * invLen;
    plane.normal.y = ny * invLen;
    plane.normal.z = nz * invLen;

    plane.dist = (v1.x * plane.normal.x + plane.normal.y * v1.y) + v1.z * plane.normal.z;
}

// ra3_1.0.game 0x008603B0
void makeAlignToNormalMatrix(
    float angle, const Coord3D& pos, const Coord3D& normal, Matrix3x4& mtx) noexcept {
    float xx = 0.0F;
    float xy = 0.0F;
    float xz = 0.0F;
#if defined(_MSC_VER) && defined(_M_IX86)
    __asm {
        fld dword ptr [angle]
        fld st(0)
        fcos
        fstp dword ptr [xx]
        fsin
        fstp dword ptr [xy]
    }
#else
    xx = std::cos(angle);
    xy = std::sin(angle);
#endif
    if (!sseEqOrdered(normal.z, kZero)) {
        const float dotXY = normal.y * xy + xx * normal.x;
        xz = kNegZero - (dotXY / normal.z);
        const float lenSq = (xz * xz + xx * xx) + xy * xy;
        const float invLen = sseEqOrdered(lenSq, kZero) ? kZero : x87ReciprocalSqrt(lenSq);
        xx = invLen * xx;
        xy = invLen * xy;
        xz = xz * invLen;
    }

    const float yx = xz * normal.y - normal.z * xy;
    const float yy = normal.z * xx - xz * normal.x;
    const float yz = xy * normal.x - normal.y * xx;
    const float yLenSq = (yz * yz + yy * yy) + yx * yx;
    const float invYLen = sseEqOrdered(yLenSq, kZero) ? kZero : x87ReciprocalSqrt(yLenSq);

    mtx.m[0][0] = xx;
    mtx.m[0][1] = invYLen * yx;
    mtx.m[0][2] = normal.x;
    mtx.m[0][3] = pos.x;

    mtx.m[1][0] = xy;
    mtx.m[1][1] = yy * invYLen;
    mtx.m[1][2] = normal.y;
    mtx.m[1][3] = pos.y;

    mtx.m[2][0] = xz;
    mtx.m[2][1] = yz * invYLen;
    mtx.m[2][2] = normal.z;
    mtx.m[2][3] = pos.z;
}

// ra3_1.0.game 0x0085D890
bool isPointOnBridge(
    const BridgeData& bridge, const Coord3D* loc,
    PolygonTriggerTestCallback polygonTestFn) noexcept {
    if (sseGt(bridge.bounds.lo.x, loc->x) || sseGt(loc->x, bridge.bounds.hi.x) ||
        sseGt(bridge.bounds.lo.y, loc->y) || sseGt(loc->y, bridge.bounds.hi.y)) {
        return false;
    }

    if (bridge.bridgePolygon != nullptr) {
        if (polygonTestFn == nullptr) {
            return false;
        }
        ICoord3D icoord{};
        icoord.x = _mm_cvtt_ss2si(_mm_set_ss(loc->x));
        icoord.y = _mm_cvtt_ss2si(_mm_set_ss(loc->y));
        icoord.z = _mm_cvtt_ss2si(_mm_set_ss(loc->z));
        return polygonTestFn(bridge.bridgePolygon, icoord);
    }

    std::uint8_t flags = 0;
    if (pointInTriangle2D(
            bridge.info.fromLeft, bridge.info.fromRight, bridge.info.toLeft, *loc,
            0, 1, flags)) {
        return true;
    }
    return pointInTriangle2D(
        bridge.info.fromRight, bridge.info.toLeft, bridge.info.toRight, *loc,
        0, 1, flags);
}

// ra3_1.0.game 0x0085FA50
float projectPointAlongBridgeSpan(const BridgeData& bridge, const Coord3D* loc) noexcept {
    float dirX = bridge.info.toLeft.x - bridge.info.fromLeft.x;
    float dirY = bridge.info.toLeft.y - bridge.info.fromLeft.y;
    const float lenSq = dirX * dirX + dirY * dirY;
    if (!sseEqOrdered(lenSq, kZero)) {
        const float invLen = x87ReciprocalSqrt(lenSq);
        dirX *= invLen;
        dirY *= invLen;
    }
    const float relX = loc->x - bridge.info.fromLeft.x;
    const float relY = loc->y - bridge.info.fromLeft.y;
#if defined(_MSC_VER) && defined(_M_IX86)
    float resultVal = 0.0F;
    __asm {
        fld dword ptr [relY]
        fmul dword ptr [dirY]
        fld dword ptr [relX]
        fmul dword ptr [dirX]
        faddp st(1), st(0)
        fstp dword ptr [resultVal]
    }
    return resultVal;
#else
    return relY * dirY + relX * dirX;
#endif
}

#if defined(_MSC_VER) && defined(_M_IX86)
__declspec(naked) float __fastcall projectPointAlongBridgeSpanExtended(
    const BridgeData* /*bridge*/, void* /*unused*/, const Coord3D* /*loc*/) noexcept {
    __asm {
        sub esp, 0Ch
        movss xmm0, dword ptr [ecx + 1Ch]
        subss xmm0, dword ptr [ecx + 04h]
        movss xmm1, dword ptr [ecx + 20h]
        subss xmm1, dword ptr [ecx + 08h]
        movaps xmm2, xmm1
        mulss xmm2, xmm1
        movaps xmm3, xmm0
        mulss xmm3, xmm0
        addss xmm2, xmm3
        xorps xmm3, xmm3
        ucomiss xmm2, xmm3
        movss dword ptr [esp + 00h], xmm0
        movss dword ptr [esp + 04h], xmm1
        movss dword ptr [esp + 08h], xmm2
        lahf
        test ah, 44h
        jnp skip_norm
        fld dword ptr [esp + 08h]
        fsqrt
        fld1
        fdivrp st(1), st(0)
        fstp dword ptr [esp + 08h]
        movss xmm0, dword ptr [esp + 08h]
        movaps xmm1, xmm0
        mulss xmm1, dword ptr [esp + 00h]
        mulss xmm0, dword ptr [esp + 04h]
        movss dword ptr [esp + 00h], xmm1
        movss dword ptr [esp + 04h], xmm0
    skip_norm:
        mov eax, dword ptr [esp + 10h]
        movss xmm0, dword ptr [eax + 00h]
        subss xmm0, dword ptr [ecx + 04h]
        movss xmm1, dword ptr [eax + 04h]
        subss xmm1, dword ptr [ecx + 08h]
        movss dword ptr [esp + 08h], xmm0
        movss dword ptr [esp + 10h], xmm1
        fld dword ptr [esp + 10h]
        fmul dword ptr [esp + 04h]
        fld dword ptr [esp + 08h]
        fmul dword ptr [esp + 00h]
        faddp st(1), st(0)
        add esp, 0Ch
        ret 4
    }
}
#endif

// ra3_1.0.game 0x0085FAD0
bool isCellOnBridgeEnd(const BridgeData& bridge, const Region2D* cell) noexcept {
    float dx = bridge.info.fromRight.x - bridge.info.fromLeft.x;
    float dy = bridge.info.fromRight.y - bridge.info.fromLeft.y;
    float dz = bridge.info.fromRight.z - bridge.info.fromLeft.z;
    const float lenSq1 = dx * dx + dy * dy + dz * dz;
    if (!sseEqOrdered(lenSq1, kZero)) {
        const float invLen = x87ReciprocalSqrt(lenSq1);
        dx *= invLen;
        dy *= invLen;
    }
    const float offX1 = dx * kTwenty;
    const float offY1 = dy * kTwenty;

    Coord2D end1A{bridge.info.fromLeft.x + offX1, bridge.info.fromLeft.y + offY1};
    Coord2D end1B{bridge.info.fromRight.x - offX1, bridge.info.fromRight.y - offY1};
    if (lineInRegion2D(&end1A, &end1B, cell)) {
        return true;
    }

    float offX2 = offX1;
    float offY2 = offY1;
    if (bridge.bridgePolygon != nullptr) {
        float dx2 = bridge.info.toRight.x - bridge.info.toLeft.x;
        float dy2 = bridge.info.toRight.y - bridge.info.toLeft.y;
        float dz2 = bridge.info.toRight.z - bridge.info.toLeft.z;
        const float lenSq2 = dx2 * dx2 + dy2 * dy2 + dz2 * dz2;
        if (!sseEqOrdered(lenSq2, kZero)) {
            const float invLen2 = x87ReciprocalSqrt(lenSq2);
            dx2 *= invLen2;
            dy2 *= invLen2;
        }
        offX2 = dx2 * kTwenty;
        offY2 = dy2 * kTwenty;
    }

    Coord2D end2A{bridge.info.toLeft.x + offX2, bridge.info.toLeft.y + offY2};
    Coord2D end2B{bridge.info.toRight.x - offX2, bridge.info.toRight.y - offY2};
    return lineInRegion2D(&end2A, &end2B, cell);
}

// ra3_1.0.game 0x0085FD60
bool isCellOnBridgeSide(const BridgeData& bridge, const Region2D* cell) noexcept {
    const float wx = bridge.info.fromRight.x - bridge.info.fromLeft.x;
    const float wy = bridge.info.fromRight.y - bridge.info.fromLeft.y;
    const float wz = bridge.info.fromRight.z - bridge.info.fromLeft.z;
    const float lenSq = (wz * wz + wx * wx) + wy * wy;
    const float invLen = sseEqOrdered(lenSq, kZero) ? kZero : x87ReciprocalSqrt(lenSq);
    const float offX = (invLen * wx) * kSideOffset;
    const float offY = (invLen * wy) * kSideOffset;

    const float s1Ax = bridge.info.fromLeft.x - offX;
    const float s1Ay = bridge.info.fromLeft.y - offY;
    const float s1Bx = bridge.info.toLeft.x - offX;
    const float s1By = bridge.info.toLeft.y - offY;
    const float s2Ax = bridge.info.fromRight.x + offX;
    const float s2Ay = bridge.info.fromRight.y + offY;
    const float s2Bx = bridge.info.toRight.x + offX;
    const float s2By = bridge.info.toRight.y + offY;

    Coord2D p1{s1Ax, s1Ay};
    Coord2D p2{s1Bx, s1By};
    if (lineInRegion2D(&p1, &p2, cell)) {
        return true;
    }

    p1 = {s2Ax, s2Ay};
    p2 = {s2Bx, s2By};
    if (lineInRegion2D(&p1, &p2, cell)) {
        return true;
    }

    const float s3Ax = s1Ax - offX;
    const float s3Ay = s1Ay - offY;
    const float s3Bx = s1Bx - offX;
    const float s3By = s1By - offY;
    const float s4Ax = s2Ax + offX;
    const float s4Ay = s2Ay + offY;
    const float s4Bx = s2Bx + offX;
    const float s4By = s2By + offY;

    p1 = {s3Ax, s3Ay};
    p2 = {s3Bx, s3By};
    if (lineInRegion2D(&p1, &p2, cell)) {
        return true;
    }

    p1 = {s4Ax, s4Ay};
    p2 = {s4Bx, s4By};
    return lineInRegion2D(&p1, &p2, cell);
}

// ra3_1.0.game 0x00860020
bool isCellBridgeEntryPoint(
    const BridgeData& bridge, const Region2D* cell, float* outEntryHeight) noexcept {
    const float wx = bridge.info.fromRight.x - bridge.info.fromLeft.x;
    const float wy = bridge.info.fromRight.y - bridge.info.fromLeft.y;
    const float wz = bridge.info.fromRight.z - bridge.info.fromLeft.z;
    const float wLenSq = (wz * wz + wy * wy) + wx * wx;
    const float invWLen = sseEqOrdered(wLenSq, kZero) ? kZero : x87ReciprocalSqrt(wLenSq);
    const float offWX = (invWLen * wx) * kTwenty;
    const float offWY = (wy * invWLen) * kTwenty;

    const float sx = bridge.info.toRight.x - bridge.info.fromRight.x;
    const float sy = bridge.info.toRight.y - bridge.info.fromRight.y;
    const float sz = bridge.info.toRight.z - bridge.info.fromRight.z;
    const float sLenSq = (sz * sz + sy * sy) + sx * sx;
    const float invSLen = sseEqOrdered(sLenSq, kZero) ? kZero : x87ReciprocalSqrt(sLenSq);
    const float offSX = (invSLen * sx) * kTen;
    const float offSY = (sy * invSLen) * kTen;

    const float e1Ax = (bridge.info.fromLeft.x - offSX) + offWX;
    const float e1Ay = (bridge.info.fromLeft.y - offSY) + offWY;
    const float e1Bx = (bridge.info.fromRight.x - offSX) - offWX;
    const float e1By = (bridge.info.fromRight.y - offSY) - offWY;
    const float e2Ax = (bridge.info.toLeft.x + offSX) + offWX;
    const float e2Ay = (bridge.info.toLeft.y + offSY) + offWY;
    const float e2Bx = (bridge.info.toRight.x + offSX) - offWX;
    const float e2By = (bridge.info.toRight.y + offSY) - offWY;

    Coord2D p1{e1Ax, e1Ay};
    Coord2D p2{e1Bx, e1By};
    if (lineInRegion2D(&p1, &p2, cell)) {
        *outEntryHeight = bridge.info.fromLeft.z;
        return true;
    }

    p1 = {e2Ax, e2Ay};
    p2 = {e2Bx, e2By};
    if (lineInRegion2D(&p1, &p2, cell)) {
        *outEntryHeight = bridge.info.toLeft.z;
        return true;
    }
    return false;
}

// ra3_1.0.game 0x00861450
bool isPointOnIntactBridge(
    const BridgeData& bridge, const Coord3D* loc,
    PolygonTriggerTestCallback polygonTestFn) noexcept {
    if (!isPointOnBridge(bridge, loc, polygonTestFn)) {
        return false;
    }
    if (bridge.spanBegin == bridge.spanEnd) {
        return true;
    }
    const float t = projectPointAlongBridgeSpan(bridge, loc);
    for (const BridgeSpanInterval* it = bridge.spanBegin; it != bridge.spanEnd; ++it) {
        if (!sseLt(t, it->lo) && sseGe(it->hi, t)) {
            return false;
        }
    }
    return true;
}

// ra3_1.0.game 0x008614B0
bool pickBridge(
    const BridgeData& bridge, const Coord3D& from, const Coord3D& to, Coord3D* outPos,
    PolygonTriggerTestCallback polygonTestFn) noexcept {
    Plane3D plane{};
    setPlaneFromPoints(plane, bridge.info.fromLeft, bridge.info.fromRight, bridge.info.toLeft);

    const float dx = to.x - from.x;
    const float dy = to.y - from.y;
    const float dz = to.z - from.z;
    const float denom = (plane.normal.z * dz + plane.normal.y * dy) + dx * plane.normal.x;

    float t = from.y;
    if (!sseEqOrdered(denom, kZero)) {
        const float num =
            ((from.z * plane.normal.z + from.y * plane.normal.y) + from.x * plane.normal.x) -
            plane.dist;
        t = (kNegZero - num) / denom;
    }

    Coord3D hit{
        dx * t + from.x,
        (to.y - from.y) * t + from.y,
        (to.z - from.z) * t + from.z};
    if (isPointOnIntactBridge(bridge, &hit, polygonTestFn)) {
        *outPos = hit;
        return true;
    }
    return false;
}

namespace {

void computeBridgeHeightInternal(
    const BridgeData& bridge, const Coord3D* loc, Coord3D* normal,
    bool& outPolyBranch, float& outZOrT) noexcept {
    if (bridge.bridgePolygon != nullptr) {
        outPolyBranch = true;
        const float e1x = bridge.info.fromRight.x - bridge.info.fromLeft.x;
        const float e1y = bridge.info.fromRight.y - bridge.info.fromLeft.y;
        const float e2x = bridge.info.toRight.x - bridge.info.toLeft.x;
        const float e2y = bridge.info.toRight.y - bridge.info.toLeft.y;

        const float vx = bridge.info.toRight.x - bridge.info.fromRight.x;
        const float vy = bridge.info.toRight.y - bridge.info.fromRight.y;

        float n1x = kNegZero - e1y;
        float n1y = e1x;
        float n2x = kNegZero - e2x;
        float n2y = e2y;

        const float dot1 = vy * n1y + n1x * vx;
        if (sseGt(kZero, dot1)) {
            n1x *= kNegOne;
            n1y *= kNegOne;
        }

        const float dot2 = n2x * vy + n2y * vx;
        if (sseGt(dot2, kZero)) {
            n2y *= kNegOne;
            n2x *= kNegOne;
        }

        const float len1Sq = n1y * n1y + n1x * n1x;
        const float invLen1 = sseEqOrdered(len1Sq, kZero) ? kZero : x87ReciprocalSqrt(len1Sq);
        const float u1x = invLen1 * n1x;
        const float u1y = n1y * invLen1;

        const float len2Sq = n2x * n2x + n2y * n2y;
        const float invLen2 = sseEqOrdered(len2Sq, kZero) ? kZero : x87ReciprocalSqrt(len2Sq);
        const float u2x = n2x * invLen2;
        const float u2y = invLen2 * n2y;

        const float m1x = (bridge.info.fromRight.x + bridge.info.fromLeft.x) * kHalf;
        const float m1y = (bridge.info.fromRight.y + bridge.info.fromLeft.y) * kHalf;
        const float m2y = (bridge.info.toLeft.y + bridge.info.toRight.y) * kHalf;

        const float d1 = (loc->y - m1y) * u1y + u1x * (loc->x - m1x);
        const float m2x = (bridge.info.toLeft.x + bridge.info.toRight.x) * kHalf;
        const float d2 = (loc->y - m2y) * u2x + (loc->x - m2x) * u2y;

        const float sumD = d2 + d1;
        const float invSumD = kOne / sumD;
        const float w2 = invSumD * d2;
        const float w1 = invSumD * d1;
        const float z1 = bridge.info.fromRight.z;
        const float z2 = bridge.info.toRight.z;

        const float termA = ((z1 - z2) * w2 + z2) * (kOne - w2);
        const float termB = ((z2 - z1) * w1 + z1) * (kOne - w1);
        outZOrT = termA + termB;

        if (normal != nullptr) {
            normal->x = kZero;
            normal->y = kZero;
            normal->z = kOne;
        }
        return;
    }

    outPolyBranch = false;
    Plane3D plane{};
    setPlaneFromPoints(plane, bridge.info.fromLeft, bridge.info.fromRight, bridge.info.toLeft);

    const float dx = loc->x - loc->x;
    const float dy = loc->y - loc->y;
    const float denom =
        (plane.normal.y * dy + dx * plane.normal.x) + plane.normal.z * kThousand;
    float t = outZOrT;
    if (!sseEqOrdered(denom, kZero)) {
        const float num =
            ((plane.normal.y * loc->y + plane.normal.x * loc->x) + plane.normal.z * kZero) -
            plane.dist;
        t = (kNegZero - num) / denom;
    }
    if (normal != nullptr) {
        *normal = plane.normal;
    }
    outZOrT = t;
}

}  // namespace

// ra3_1.0.game 0x00861680
float getBridgeHeight(
    const BridgeData& bridge, const Coord3D* loc, Coord3D* normal) noexcept {
    bool polyBranch = false;
    float zOrT = 0.0F;
    std::memcpy(&zOrT, &loc, sizeof(float));
    computeBridgeHeightInternal(bridge, loc, normal, polyBranch, zOrT);
    if (polyBranch) {
        return zOrT;
    }
#if defined(_MSC_VER) && defined(_M_IX86)
    float resultVal = 0.0F;
    __asm {
        fld dword ptr [zOrT]
        fmul dword ptr [kThousand]
        fstp dword ptr [resultVal]
    }
    return resultVal;
#else
    return zOrT * kThousand;
#endif
}

#if defined(_MSC_VER) && defined(_M_IX86)
float __fastcall getBridgeHeightExtended(
    const BridgeData* bridge, void* /*unused*/, const Coord3D* loc, Coord3D* normal) noexcept {
    bool polyBranch = false;
    float zOrT = 0.0F;
    std::memcpy(&zOrT, &loc, sizeof(float));
    computeBridgeHeightInternal(*bridge, loc, normal, polyBranch, zOrT);
    if (polyBranch) {
        __asm {
            fld dword ptr [zOrT]
        }
    } else {
        __asm {
            fld dword ptr [zOrT]
            fmul dword ptr [kThousand]
        }
    }
}
#endif

// ra3_1.0.game 0x00860A20
void setWaypointBiDirectional(WaypointData& waypoint, bool biDirectional) noexcept {
    waypoint.biDirectional = biDirectional ? 1U : 0U;
}

// ra3_1.0.game 0x00860A30
std::uint8_t getWaypointBiDirectional(const WaypointData& waypoint) noexcept {
    return waypoint.biDirectional;
}

// ra3_1.0.game 0x0085DD30
void addWaypointLink(WaypointData* waypointListHead, std::int32_t id1, std::int32_t id2) noexcept {
    WaypointData* wp1 = nullptr;
    for (WaypointData* cur = waypointListHead; cur != nullptr; cur = cur->next) {
        if (cur->id == id1) {
            wp1 = cur;
            break;
        }
    }

    WaypointData* wp2 = nullptr;
    for (WaypointData* cur = waypointListHead; cur != nullptr; cur = cur->next) {
        if (cur->id == id2) {
            wp2 = cur;
            break;
        }
    }

    if (wp1 == nullptr || wp2 == nullptr || wp1 == wp2) {
        return;
    }

    const std::uintptr_t wp1Addr = reinterpret_cast<std::uintptr_t>(wp1);
    const std::uintptr_t wp2Addr = reinterpret_cast<std::uintptr_t>(wp2);

    std::int32_t idx1 = 0;
    if (wp1->numLinks > 0) {
        while (wp1->links[idx1] != wp2Addr) {
            ++idx1;
            if (idx1 >= wp1->numLinks) {
                break;
            }
        }
    }
    if (idx1 == wp1->numLinks) {
        if (idx1 < 8) {
            wp1->links[idx1] = wp2Addr;
            ++wp1->numLinks;
        }
        wp2->prevLink = wp1Addr;
    }

    if (wp1->biDirectional != 0U) {
        std::int32_t idx2 = 0;
        if (wp2->numLinks > 0) {
            while (wp2->links[idx2] != wp1Addr) {
                ++idx2;
                if (idx2 >= wp2->numLinks) {
                    break;
                }
            }
        }
        if (idx2 == wp2->numLinks) {
            if (idx2 < 8) {
                wp2->links[idx2] = wp1Addr;
                ++wp2->numLinks;
            }
            wp1->prevLink = wp2Addr;
        }
    }
}

// ra3_1.0.game 0x008683F0
void resolveWaypointLinks(
    WaypointData& waypoint, WaypointLookupCallback lookup, void* userData) noexcept {
    for (std::uintptr_t& slot : waypoint.links) {
        const auto id = static_cast<std::uint32_t>(slot);
        if (id == kInvalidWaypointId) {
            slot = 0U;
        } else {
            slot = reinterpret_cast<std::uintptr_t>(
                lookup(userData, static_cast<std::int32_t>(id)));
        }
    }

    const auto secId = static_cast<std::uint32_t>(waypoint.secondaryLink);
    if (secId == kInvalidWaypointId) {
        waypoint.secondaryLink = 0U;
    } else {
        waypoint.secondaryLink = reinterpret_cast<std::uintptr_t>(
            lookup(userData, static_cast<std::int32_t>(secId)));
    }

    const auto prevId = static_cast<std::uint32_t>(waypoint.prevLink);
    if (prevId == kInvalidWaypointId) {
        waypoint.prevLink = 0U;
    } else {
        waypoint.prevLink = reinterpret_cast<std::uintptr_t>(
            lookup(userData, static_cast<std::int32_t>(prevId)));
    }
}

// ra3_1.0.game 0x0085C5E0
void* getTerrainBoundaryVector(void* terrainLogic, std::int32_t index) noexcept {
    return static_cast<std::uint8_t*>(terrainLogic) + index * 8 + 0x50;
}

// ra3_1.0.game 0x0085C650
void setTerrainActiveBoundary(
    void* terrainLogic, std::int32_t newActiveBoundary,
    ActiveBoundaryChangedCallback onChanged) noexcept {
    auto* const bytes = static_cast<std::uint8_t*>(terrainLogic);
    auto* const activeSlot = reinterpret_cast<std::int32_t*>(bytes + 0x88);
    if (*activeSlot != newActiveBoundary) {
        *activeSlot = newActiveBoundary;
        if (onChanged != nullptr) {
            onChanged(terrainLogic);
        }
    }
}

// ra3_1.0.game 0x0085CA80
float getOriginGroundHeight(
    void* terrainLogic, std::uint32_t /*unusedArg*/, Coord3D* normal,
    GroundHeightCallback getGroundHeightFn) noexcept {
    return getGroundHeightFn(terrainLogic, 0.0F, 0.0F, normal);
}

// ra3_1.0.game 0x0085CC70
float getWaterHeightFromHandle(
    const void* waterHandle, WaterHandleHeightCallback callback) noexcept {
    if (waterHandle == nullptr) {
        return 0.0F;
    }
    const auto* const bytes = static_cast<const std::uint8_t*>(waterHandle);
    if (callback != nullptr) {
        const auto* const vbtable = *reinterpret_cast<const std::int32_t* const*>(bytes + 0x1C);
        const std::int32_t vbaseOffset = (vbtable != nullptr) ? vbtable[1] : 0;
        const std::int32_t rawHeight = callback(bytes + 0x1C + vbaseOffset);
        return static_cast<float>(rawHeight);
    }
    return 0.0F;
}

#if defined(_MSC_VER) && defined(_M_IX86)
__declspec(naked) float __fastcall getWaterHeightFromHandleExtended(
    const void* /*terrainLogic*/, void* /*unused*/, const void* /*waterHandle*/) noexcept {
    __asm {
        mov eax, dword ptr [esp + 04h]
        test eax, eax
        jne valid_handle
        fldz
        ret 4
    valid_handle:
        mov ecx, dword ptr [eax + 1Ch]
        mov edx, dword ptr [ecx + 04h]
        lea ecx, [eax + edx + 1Ch]
        mov eax, dword ptr [ecx]
        mov edx, dword ptr [eax + 08h]
        call edx
        mov dword ptr [esp + 04h], eax
        fild dword ptr [esp + 04h]
        ret 4
    }
}
#endif

// ra3_1.0.game 0x0085DA00
bool hasTerrainBoundaries(const void* terrainLogic) noexcept {
    const auto* const bytes = static_cast<const std::uint8_t*>(terrainLogic);
    const auto begin = *reinterpret_cast<const std::uintptr_t*>(bytes + 0x50);
    const auto end = *reinterpret_cast<const std::uintptr_t*>(bytes + 0x54);
    return begin != end;
}

// ra3_1.0.game 0x0085DA10
bool getTerrainBoundary2D(
    const void* terrainLogic, std::uint32_t index, float* outX, float* outY) noexcept {
    const auto* const bytes = static_cast<const std::uint8_t*>(terrainLogic);
    const auto begin = *reinterpret_cast<const std::uintptr_t*>(bytes + 0x50);
    const auto end = *reinterpret_cast<const std::uintptr_t*>(bytes + 0x54);
    const auto count =
        static_cast<std::uint32_t>(static_cast<std::intptr_t>(end - begin) >> 3);
    if (index < count) {
        const auto* const elem = reinterpret_cast<const Coord2D*>(begin + index * 8U);
        copyFloatViaX87(&elem->x, outX);
        copyFloatViaX87(&elem->y, outY);
        return true;
    }
    return false;
}

// ra3_1.0.game 0x0085DDF0
float getBaseGroundHeight(
    const void* /*terrainLogic*/, float /*x*/, float /*y*/, Coord3D* normal) noexcept {
    if (normal != nullptr) {
        normal->x = 0.0F;
        normal->y = 0.0F;
        normal->z = 0.0F;
    }
    return 0.0F;
}

#if defined(_MSC_VER) && defined(_M_IX86)
__declspec(naked) float __fastcall getBaseGroundHeightExtended(
    const void* /*terrainLogic*/, void* /*unused*/, float /*x*/, float /*y*/,
    Coord3D* /*normal*/) noexcept {
    __asm {
        mov eax, dword ptr [esp + 0Ch]
        fldz
        test eax, eax
        je done
        xorps xmm0, xmm0
        movss dword ptr [eax + 00h], xmm0
        movss dword ptr [eax + 04h], xmm0
        movss dword ptr [eax + 08h], xmm0
    done:
        ret 0Ch
    }
}
#endif

// ra3_1.0.game 0x0085DE60
float getBaseLayerHeight(
    const void* /*terrainLogic*/, float /*x*/, float /*y*/, std::int32_t /*layer*/,
    Coord3D* normal, bool /*clip*/) noexcept {
    if (normal != nullptr) {
        normal->x = 0.0F;
        normal->y = 0.0F;
        normal->z = 0.0F;
    }
    return 0.0F;
}

#if defined(_MSC_VER) && defined(_M_IX86)
__declspec(naked) float __fastcall getBaseLayerHeightExtended(
    const void* /*terrainLogic*/, void* /*unused*/, float /*x*/, float /*y*/,
    std::int32_t /*layer*/, Coord3D* /*normal*/, bool /*clip*/) noexcept {
    __asm {
        mov eax, dword ptr [esp + 10h]
        fldz
        test eax, eax
        je done
        xorps xmm0, xmm0
        movss dword ptr [eax + 00h], xmm0
        movss dword ptr [eax + 04h], xmm0
        movss dword ptr [eax + 08h], xmm0
    done:
        ret 14h
    }
}
#endif

// ra3_1.0.game 0x0085DFA0
Coord3D* findClosestEdgePoint(
    void* terrainLogic, Coord3D* out, const Coord3D* closestTo,
    const Region3D& extent, GroundHeightCallback getGroundHeightFn) noexcept {
#if defined(_MSC_VER) && defined(_M_IX86)
    float dists[4]{};
    const float px = closestTo->x;
    const float py = closestTo->y;
    const float loX = extent.lo.x;
    const float loY = extent.lo.y;
    const float hiX = extent.hi.x;
    const float hiY = extent.hi.y;
    __asm {
        fld dword ptr [py]
        fsub dword ptr [loY]
        fabs
        fstp dword ptr [dists + 00h]

        fld dword ptr [px]
        fsub dword ptr [hiX]
        fabs
        fstp dword ptr [dists + 04h]

        fld dword ptr [py]
        fsub dword ptr [hiY]
        fabs
        fstp dword ptr [dists + 08h]

        fld dword ptr [px]
        fsub dword ptr [loX]
        fabs
        fstp dword ptr [dists + 0Ch]
    }
#else
    float dists[4]{
        std::fabs(closestTo->y - extent.lo.y),
        std::fabs(closestTo->x - extent.hi.x),
        std::fabs(closestTo->y - extent.hi.y),
        std::fabs(closestTo->x - extent.lo.x)};
#endif
    int bestIndex = 0;
    float bestDist = dists[0];
    for (int i = 1; i < 4; ++i) {
        if (sseGt(bestDist, dists[i])) {
            bestDist = dists[i];
            bestIndex = i;
        }
    }

    copyFloatViaX87(&closestTo->z, &out->z);
    if (bestIndex == 0) {
        out->x = closestTo->x;
        out->y = extent.lo.y;
    } else if (bestIndex == 1) {
        out->x = extent.hi.x;
        out->y = closestTo->y;
    } else if (bestIndex == 2) {
        out->x = closestTo->x;
        out->y = extent.hi.y;
    } else {
        out->x = extent.lo.x;
        out->y = closestTo->y;
    }
    out->z = getGroundHeightFn(terrainLogic, out->x, out->y, nullptr);
    return out;
}

// ra3_1.0.game 0x0085E0B0
bool clipLineToMapExtent(
    void* terrainLogic, Coord3D* out, const Coord3D* p1, const Coord3D* p2,
    const Region3D& extentIncludingBorder, LayerHeightCallback getLayerHeightFn) noexcept {
    if (!(sseGt(p2->x, extentIncludingBorder.lo.x) &&
          sseGt(extentIncludingBorder.hi.x, p2->x) &&
          sseGt(p2->y, extentIncludingBorder.lo.y) &&
          sseGt(extentIncludingBorder.hi.y, p2->y))) {
        return false;
    }

    const float deltaX = p2->x - p1->x;
    const float deltaY = p2->y - p1->y;
    float absDx = 0.0F;
    float absDy = 0.0F;
#if defined(_MSC_VER) && defined(_M_IX86)
    __asm {
        fld dword ptr [deltaX]
        fabs
        fstp dword ptr [absDx]
        fld dword ptr [deltaY]
        fabs
        fstp dword ptr [absDy]
    }
#else
    absDx = std::fabs(deltaX);
    absDy = std::fabs(deltaY);
#endif

    if (sseGt(kClipEpsilon, absDx)) {
        if (sseGt(kClipEpsilon, absDy)) {
            return false;
        }
        const float hitX = p1->x;
        const float hitY = sseGt(deltaY, kZero) ? extentIncludingBorder.hi.y : extentIncludingBorder.lo.y;
        const float hitZ = getLayerHeightFn(terrainLogic, hitX, hitY, 1, nullptr, true);
        out->x = hitX;
        out->y = hitY;
        out->z = hitZ;
        return true;
    }

    if (!sseLt(kClipEpsilon, absDy)) {
        if (!sseLt(kClipEpsilon, absDx)) {
            return false;
        }
        const float hitX = sseGt(deltaX, kZero) ? extentIncludingBorder.hi.x : extentIncludingBorder.lo.x;
        const float hitY = p1->y;
        const float hitZ = getLayerHeightFn(terrainLogic, hitX, hitY, 1, nullptr, true);
        out->x = hitX;
        out->y = hitY;
        out->z = hitZ;
        return true;
    }

    const float dxOverDy = deltaX / deltaY;
    const float dyOverDx = deltaY / deltaX;

    if (sseGt(deltaY, kZero)) {
        const float hitX = (extentIncludingBorder.hi.y - p2->y) * dxOverDy + p2->x;
        if (sseGe(hitX, extentIncludingBorder.lo.x) && sseGe(extentIncludingBorder.hi.x, hitX)) {
            const float hitY = extentIncludingBorder.hi.y;
            const float hitZ = getLayerHeightFn(terrainLogic, hitX, hitY, 1, nullptr, true);
            out->x = hitX;
            out->y = hitY;
            out->z = hitZ;
            return true;
        }
    }

    if (sseGt(deltaX, kZero)) {
        const float hitY = (extentIncludingBorder.hi.x - p2->x) * dyOverDx + p2->y;
        if (sseGe(hitY, extentIncludingBorder.lo.y) && sseGe(extentIncludingBorder.hi.y, hitY)) {
            const float hitX = extentIncludingBorder.hi.x;
            const float hitZ = getLayerHeightFn(terrainLogic, hitX, hitY, 1, nullptr, true);
            out->x = hitX;
            out->y = hitY;
            out->z = hitZ;
            return true;
        }
    }

    if (sseGt(kZero, deltaY)) {
        const float hitX = (extentIncludingBorder.lo.y - p2->y) * dxOverDy + p2->x;
        if (sseGe(hitX, extentIncludingBorder.lo.x) && sseGe(extentIncludingBorder.hi.x, hitX)) {
            const float hitY = extentIncludingBorder.lo.y;
            const float hitZ = getLayerHeightFn(terrainLogic, hitX, hitY, 1, nullptr, true);
            out->x = hitX;
            out->y = hitY;
            out->z = hitZ;
            return true;
        }
    }

    if (sseGt(kZero, deltaX)) {
        const float hitY = (extentIncludingBorder.lo.x - p2->x) * dyOverDx + p2->y;
        if (sseGe(hitY, extentIncludingBorder.lo.y) && sseGe(extentIncludingBorder.hi.y, hitY)) {
            const float hitX = extentIncludingBorder.lo.x;
            const float hitZ = getLayerHeightFn(terrainLogic, hitX, hitY, 1, nullptr, true);
            out->x = hitX;
            out->y = hitY;
            out->z = hitZ;
            return true;
        }
    }

    return false;
}

// ra3_1.0.game 0x0085E3A0
Coord3D* findFarthestEdgePoint(
    void* terrainLogic, Coord3D* out, const Coord3D* farthestFrom,
    const Region3D& extent, GroundHeightCallback getGroundHeightFn) noexcept {
    if (sseGt((extent.hi.x - extent.lo.x) * kHalf, farthestFrom->x)) {
        out->x = extent.hi.x;
    } else {
        out->x = extent.lo.x;
    }
    if (sseGt((extent.hi.y - extent.lo.y) * kHalf, farthestFrom->y)) {
        out->y = extent.hi.y;
    } else {
        out->y = extent.lo.y;
    }
    out->z = getGroundHeightFn(terrainLogic, out->x, out->y, nullptr);
    return out;
}

// ra3_1.0.game 0x0085E7F0
void getTerrainBoundaryRegion(
    const void* terrainLogic, Region2D* outRegion, std::int32_t index) noexcept {
    const auto* const bytes = static_cast<const std::uint8_t*>(terrainLogic);
    const auto* const regions = *reinterpret_cast<const IRegion2D* const*>(bytes + 0x54);
    const IRegion2D& src = regions[index];
    outRegion->lo.x = static_cast<float>(src.loX) * kMapXyFactor;
    outRegion->lo.y = static_cast<float>(src.loY) * kMapXyFactor;
    outRegion->hi.x = static_cast<float>(src.hiX) * kMapXyFactor;
    outRegion->hi.y = static_cast<float>(src.hiY) * kMapXyFactor;
}

// ra3_1.0.game 0x0085E840
bool clampPositionToMapExtent(
    void* terrainLogic, Coord3D* pos, const Region3D& extent,
    GroundHeightCallback getGroundHeightFn) noexcept {
    if (terrainLogic == nullptr) {
        return false;
    }
    if (sseGt(pos->x, extent.lo.x) && sseGt(extent.hi.x, pos->x) &&
        sseGt(pos->y, extent.lo.y) && sseGt(extent.hi.y, pos->y)) {
        return true;
    }
    Coord3D clamped{};
    findClosestEdgePoint(terrainLogic, &clamped, pos, extent, getGroundHeightFn);
    *pos = clamped;
    return true;
}

// ra3_1.0.game 0x008602F0
void getMaximumPathfindExtentCenter(
    const Region3D& extent, Coord3D* outCenter) noexcept {
    outCenter->x = (extent.hi.x - extent.lo.x) * kHalf + extent.lo.x;
    outCenter->y = (extent.hi.y - extent.lo.y) * kHalf + extent.lo.y;
    outCenter->z = (extent.hi.z - extent.lo.z) * kHalf + extent.lo.z;
}

// ra3_1.0.game 0x00860630
BridgeData* findBridgeAt(const void* terrainLogic, const Coord3D* loc) noexcept {
    const auto* const bytes = static_cast<const std::uint8_t*>(terrainLogic);
    BridgeData* cur = *reinterpret_cast<BridgeData* const*>(bytes + 0x68);
    while (cur != nullptr) {
        if (isPointOnBridge(*cur, loc)) {
            return cur;
        }
        cur = cur->next;
    }
    return nullptr;
}

// ra3_1.0.game 0x00860660
BridgeData* findBridgeLayerAt(
    const void* terrainLogic, const Coord3D* loc, std::int32_t layer, bool clip) noexcept {
    if (layer == 1 || layer > 15) {
        return nullptr;
    }
    const auto* const bytes = static_cast<const std::uint8_t*>(terrainLogic);
    BridgeData* cur = *reinterpret_cast<BridgeData* const*>(bytes + 0x68);
    while (cur != nullptr) {
        if (cur->layer == layer && (!clip || isPointOnBridge(*cur, loc))) {
            return cur;
        }
        cur = cur->next;
    }
    return nullptr;
}

// ra3_1.0.game 0x00861CB0
std::int32_t getLayerForDestination(
    const void* terrainLogic, const Coord3D* pos, float groundOrWaterHeight,
    WallTestCallback wallTestFn) noexcept {
    if (!sseLt(groundOrWaterHeight - kOne, pos->z)) {
        return 1;
    }

    float bestDiff = std::fabs(pos->z - groundOrWaterHeight);
    std::int32_t bestLayer = 1;

    const auto* const bytes = static_cast<const std::uint8_t*>(terrainLogic);
    for (const BridgeData* cur = *reinterpret_cast<const BridgeData* const*>(bytes + 0x68);
         cur != nullptr;
         cur = cur->next) {
        if (isPointOnBridge(*cur, pos)) {
            bool damaged = false;
            if (cur->spanBegin != cur->spanEnd) {
                const float t = projectPointAlongBridgeSpan(*cur, pos);
                for (const BridgeSpanInterval* it = cur->spanBegin; it != cur->spanEnd; ++it) {
                    if (!sseLt(t, it->lo) && sseGe(it->hi, t)) {
                        damaged = true;
                        break;
                    }
                }
            }
            if (!damaged) {
                const float bh = getBridgeHeight(*cur, pos, nullptr);
                const float diff = std::fabs(pos->z - bh) - kOne;
                if (sseGt(bestDiff, diff)) {
                    bestDiff = diff;
                    bestLayer = cur->layer;
                }
            }
        }
    }

    if (wallTestFn != nullptr) {
        (void)wallTestFn(pos);
    }
    return bestLayer;
}

// ra3_1.0.game 0x00861E30
std::int32_t getHighestLayerForDestination(
    const void* terrainLogic, const Coord3D* pos, bool onlyHealthyBridges,
    float groundHeight) noexcept {
    float bestDelta = (pos->z - groundHeight) + kOne;
    std::int32_t bestLayer = 1;

    const auto* const bytes = static_cast<const std::uint8_t*>(terrainLogic);
    for (const BridgeData* cur = *reinterpret_cast<const BridgeData* const*>(bytes + 0x68);
         cur != nullptr;
         cur = cur->next) {
        const bool onBridge = onlyHealthyBridges
            ? isPointOnIntactBridge(*cur, pos)
            : isPointOnBridge(*cur, pos);
        if (onBridge) {
            const float bh = getBridgeHeight(*cur, pos, nullptr);
            const float delta = (pos->z - bh) + kLayerHeightEpsilon;
            if (!sseLt(delta, kZero) && sseGt(std::fabs(bestDelta), std::fabs(delta))) {
                bestDelta = delta;
                bestLayer = cur->layer;
            }
        }
    }
    return bestLayer;
}

// ra3_1.0.game 0x00861F40
std::int32_t getHighestBridgeLayerForDestination(
    const void* terrainLogic, const Coord3D* pos, bool onlyHealthyBridges,
    float groundHeight) noexcept {
    float bestHeight = groundHeight;
    std::int32_t bestLayer = 1;

    const auto* const bytes = static_cast<const std::uint8_t*>(terrainLogic);
    for (const BridgeData* cur = *reinterpret_cast<const BridgeData* const*>(bytes + 0x68);
         cur != nullptr;
         cur = cur->next) {
        const bool onBridge = onlyHealthyBridges
            ? isPointOnIntactBridge(*cur, pos)
            : isPointOnBridge(*cur, pos);
        if (onBridge) {
            const float bh = getBridgeHeight(*cur, pos, nullptr);
            if (!sseLt(bh, bestHeight)) {
                bestHeight = bh;
                bestLayer = cur->layer;
            }
        }
    }
    return bestLayer;
}

// ra3_1.0.game 0x008627F0
float getFirstWaterTableHeight(const void* terrainLogic) noexcept {
    const auto* const bytes = static_cast<const std::uint8_t*>(terrainLogic);
    const auto* const table =
        *reinterpret_cast<const WaterTableContainer* const*>(bytes + 0x30);
    if (table != nullptr) {
        const auto begin = reinterpret_cast<std::uintptr_t>(table->begin);
        const auto end = reinterpret_cast<std::uintptr_t>(table->end);
        const auto count = static_cast<std::intptr_t>(end - begin) >> 3;
        if (count != 0) {
            return static_cast<float>(table->begin->target->height);
        }
    }
    return 0.0F;
}

#if defined(_MSC_VER) && defined(_M_IX86)
__declspec(naked) float __fastcall getFirstWaterTableHeightExtended(
    const void* /*terrainLogic*/, void* /*unused*/) noexcept {
    __asm {
        mov ecx, dword ptr [ecx + 30h]
        mov eax, dword ptr [ecx + 1Ch]
        sub eax, dword ptr [ecx + 18h]
        sar eax, 3
        cmp ecx, 0
        jne check_count
        xor eax, eax
    check_count:
        test eax, eax
        je zero_ret
        mov eax, dword ptr [ecx + 18h]
        mov ecx, dword ptr [eax + 04h]
        fild dword ptr [ecx + 70h]
        ret
    zero_ret:
        fldz
        ret
    }
}
#endif

// ra3_1.0.game 0x00863F00
std::int32_t alignOnTerrain(
    void* terrainLogic, float angle, const Coord3D& pos, bool stickToGround,
    Matrix3x4& mtx, float groundOrWaterHeight,
    LayerHeightCallback getLayerHeightFn,
    WallTestCallback wallTestFn) noexcept {
    const std::int32_t layer =
        getLayerForDestination(terrainLogic, &pos, groundOrWaterHeight, wallTestFn);
    Coord3D normal{};
    float height = getLayerHeightFn(terrainLogic, pos.x, pos.y, layer, &normal, true);
    if (layer > 1) {
        height += kBridgeLayerHeightBias;
    }
    makeAlignToNormalMatrix(angle, pos, normal, mtx);
    if (stickToGround) {
        mtx.m[2][3] = height;
    }
    return layer;
}

// ra3_1.0.game 0x00864190
void updateCachedWaterGridHeight(void* terrainLogic) noexcept {
    auto* const bytes = static_cast<std::uint8_t*>(terrainLogic);
    const auto* const table =
        *reinterpret_cast<const WaterTableContainer* const*>(bytes + 0x30);
    auto* const outHeight = reinterpret_cast<float*>(bytes + 0x3C);
    if (table != nullptr) {
        const auto begin = reinterpret_cast<std::uintptr_t>(table->begin);
        const auto end = reinterpret_cast<std::uintptr_t>(table->end);
        const auto count = static_cast<std::intptr_t>(end - begin) >> 3;
        if (count != 0) {
            *outHeight = static_cast<float>(table->begin->target->height);
            return;
        }
    }
    *outHeight = 0.0F;
}

// ra3_1.0.game 0x008662B0
const char* getTerrainLogicSubsystemName(const void* /*terrainLogic*/) noexcept {
    return kTerrainLogicSubsystemLiteral;
}

// ra3_1.0.game 0x00868470
void adjustNewMapWaypoints(
    void* terrainLogic, WaypointData* waypointListHead, bool /*isSaveGame*/,
    GroundHeightCallback getGroundHeightFn,
    PostNewMapWaypointsCallback postCallback) noexcept {
    for (WaypointData* wp = waypointListHead; wp != nullptr; wp = wp->next) {
        const float groundZ =
            getGroundHeightFn(terrainLogic, wp->location.x, wp->location.y, nullptr);
#if defined(_MSC_VER) && defined(_M_IX86)
        float updatedZ = 0.0F;
        const float origZ = wp->location.z;
        __asm {
            fld dword ptr [groundZ]
            fadd dword ptr [origZ]
            fstp dword ptr [updatedZ]
        }
        wp->location.z = updatedZ;
#else
        wp->location.z += groundZ;
#endif
    }
    if (postCallback != nullptr) {
        postCallback(terrainLogic);
    }
}

// ra3_1.0.game 0x008684C0
bool findPositionAround(
    const Coord3D* center, const FindPositionOptions* options, Coord3D* result,
    const Region3D& maxPathfindExtent, RandomState& logicRandom,
    TryPositionCallback tryPositionFn, void* userData) noexcept {
    if (!(sseGt(center->x, maxPathfindExtent.lo.x) &&
          sseGt(maxPathfindExtent.hi.x, center->x) &&
          sseGt(center->y, maxPathfindExtent.lo.y) &&
          sseGt(maxPathfindExtent.hi.y, center->y))) {
        *result = *center;
        return true;
    }

    float baseAngle = options->startAngle;
    if (sseEqOrdered(baseAngle, kRandomStartAngle)) {
#if defined(_MSC_VER) && defined(_M_IX86)
        baseAngle = randomRealExtended(&logicRandom, kZero, kTwoPi);
#else
        baseAngle = randomReal(logicRandom, kZero, kTwoPi);
#endif
    }

    float dist = options->minRadius;
    if (sseLt(options->maxRadius, dist)) {
        return false;
    }

    while (sseGe(options->maxRadius, dist)) {
        float angleStep = kFindPositionRingSpacing;
        if (sseEqOrdered(dist, options->minRadius)) {
            angleStep = kTwoPi;
        } else {
            angleStep = (kFindPositionRingSpacing / (dist + kOne)) * (kTwoPi * kOneSixth);
        }

        double halfCountReal = 0.0;
#if defined(_MSC_VER) && defined(_M_IX86)
        __asm {
            fld dword ptr [kTwoPi]
            fdiv dword ptr [angleStep]
            fmul dword ptr [kHalf]
            fstp qword ptr [halfCountReal]
        }
#else
        halfCountReal = static_cast<double>((kTwoPi / angleStep) * kHalf);
#endif
        const std::int32_t halfCount =
            static_cast<std::int32_t>(std::ceil(halfCountReal));

        for (std::int32_t i = 0; i < halfCount; ++i) {
            const float offset = static_cast<float>(i) * angleStep;
            const float anglePlus = offset + baseAngle;
            if (tryPositionFn(userData, center, dist, anglePlus, options, result)) {
                return true;
            }
            if (i != 0) {
                const float angleMinus = baseAngle - offset;
                if (tryPositionFn(userData, center, dist, angleMinus, options, result)) {
                    return true;
                }
            }
        }

        dist = kFindPositionRingSpacing + dist;
    }

    return false;
}

}  // namespace ra3::sim
