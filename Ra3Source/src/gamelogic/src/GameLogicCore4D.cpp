#include <ra3/gamelogic/GameLogicCore4D.h>

#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <xmmintrin.h>

namespace ra3::gamelogic {
namespace {

constexpr std::uint32_t kPiBits = 0x40490FDBU;       // 0x00BDB84C:  3.14159274f
constexpr std::uint32_t kTwoPiBits = 0x40C90FDBU;    // 0x00CA56D0:  6.28318548f
constexpr std::uint32_t kNegPiBits = 0xC0490FDBU;    // 0x00BDB8E4: -3.14159274f
constexpr std::uint32_t kQuarterPiBits = 0x3F490FDBU;// 0x00CA56D8:  0.78539819f
constexpr std::uint32_t kHalfPiBits = 0x3FC90FDBU;   // 0x00CA56D4:  1.57079637f
constexpr std::uint32_t kNegZeroBits = 0x80000000U;  // 0x00BDC008: -0.0f
constexpr std::uint32_t kTwoPow32Bits = 0x4F800000U; // 0x00BDBDD4:  4294967296.0f
constexpr std::uint32_t kOneBits = 0x3F800000U;      // 0x00BD90A4:  1.0f
constexpr std::uint32_t kHundredBits = 0x42C80000U;  // 0x00BD94EC:  100.0f
constexpr std::uint32_t kHalfBits = 0x3F000000U;     // 0x00BD9528:  0.5f
constexpr std::uint32_t kFifteenBits = 0x41700000U;  // 0x00BD9530:  15.0f
constexpr std::uint32_t kInv30Bits = 0x3D088889U;    // 0x00BDC814:  1.0f / 30.0f
constexpr std::uint32_t kRound51Bits = 0x3F028F5CU;  // 0x00BDC818:  0.50999999f

template <class T>
T load(const void* base, std::ptrdiff_t offset = 0) noexcept {
    T value{};
    std::memcpy(&value, static_cast<const std::byte*>(base) + offset, sizeof(value));
    return value;
}

template <class T>
void store(void* base, std::ptrdiff_t offset, T value) noexcept {
    std::memcpy(static_cast<std::byte*>(base) + offset, &value, sizeof(value));
}

float bitsToFloat(std::uint32_t bits) noexcept {
    float value = 0.0F;
    std::memcpy(&value, &bits, sizeof(value));
    return value;
}

void copyFloatX87(void* dstBase, std::ptrdiff_t dstOff, const void* srcBase, std::ptrdiff_t srcOff) noexcept {
    const void* srcPtr = static_cast<const std::byte*>(srcBase) + srcOff;
    void* dstPtr = static_cast<std::byte*>(dstBase) + dstOff;
#if defined(_MSC_VER) && defined(_M_IX86)
    __asm {
        mov eax, srcPtr
        mov edx, dstPtr
        fld dword ptr [eax]
        fstp dword ptr [edx]
    }
#else
    std::uint32_t raw = 0;
    std::memcpy(&raw, srcPtr, sizeof(raw));
    std::memcpy(dstPtr, &raw, sizeof(raw));
#endif
}

std::uint32_t popcountWord32(std::uint32_t x) noexcept {
    x = x - ((x >> 1U) & 0x55555555U);
    x = (x & 0x33333333U) + ((x >> 2U) & 0x33333333U);
    x = (x + (x >> 4U)) & 0x0F0F0F0FU;
    return (x * 0x01010101U) >> 24U;
}

std::int32_t mulBy30Wrap(std::int32_t value) noexcept {
    const auto u = static_cast<std::uint32_t>(value);
    const auto x15 = (u << 4U) - u;
    return static_cast<std::int32_t>(x15 + x15);
}

float cvtIntToFloatSS(std::int32_t value) noexcept {
    return _mm_cvtss_f32(_mm_cvtsi32_ss(_mm_setzero_ps(), value));
}

std::int32_t cvttFloatToIntSS(float value) noexcept {
    return _mm_cvtt_ss2si(_mm_set_ss(value));
}

float addSS(float a, float b) noexcept {
    return _mm_cvtss_f32(_mm_add_ss(_mm_set_ss(a), _mm_set_ss(b)));
}

float subSS(float a, float b) noexcept {
    return _mm_cvtss_f32(_mm_sub_ss(_mm_set_ss(a), _mm_set_ss(b)));
}

float mulSS(float a, float b) noexcept {
    return _mm_cvtss_f32(_mm_mul_ss(_mm_set_ss(a), _mm_set_ss(b)));
}

}  // namespace

// 0x004D2AD0
void relinkGhostObjectNodeBefore4D(void* node, void* targetNode) noexcept {
    auto* next = load<void*>(node, 0x00);
    if (next != node) {
        auto* prev = load<void*>(node, 0x04);
        store<void*>(next, 0x04, prev);
        auto* prevReload = load<void*>(node, 0x04);
        auto* nextReload = load<void*>(node, 0x00);
        store<void*>(prevReload, 0x00, nextReload);
        store<void*>(node, 0x00, node);
        store<void*>(node, 0x04, node);
    }
    store<void*>(node, 0x00, targetNode);
    auto* targetPrev = load<void*>(targetNode, 0x04);
    store<void*>(node, 0x04, targetPrev);
    store<void*>(targetNode, 0x04, node);
    auto* nodePrev = load<void*>(node, 0x04);
    store<void*>(nodePrev, 0x00, node);
}

// 0x004D3750
std::uint32_t equalsObjectIDPair4D(const void* self, const void* other) noexcept {
    if (load<std::uint32_t>(self, 0x00) != load<std::uint32_t>(other, 0x00)) {
        return 0U;
    }
    if (load<std::uint32_t>(self, 0x04) != load<std::uint32_t>(other, 0x04)) {
        return 0U;
    }
    return 1U;
}

// 0x004D3770
float normalizeAngleRad4D(float angleRad) noexcept {
    __m128 xmm0 = _mm_set_ss(angleRad);
    const __m128 xmmPi = _mm_set_ss(bitsToFloat(kPiBits));
    const __m128 xmmTwoPi = _mm_set_ss(bitsToFloat(kTwoPiBits));
    const __m128 xmmNegPi = _mm_set_ss(bitsToFloat(kNegPiBits));

    if (_mm_comigt_ss(xmm0, xmmPi)) {
        do {
            xmm0 = _mm_sub_ss(xmm0, xmmTwoPi);
        } while (_mm_comigt_ss(xmm0, xmmPi));
    }

    if (_mm_comige_ss(xmmNegPi, xmm0)) {
        do {
            __m128 xmm3 = _mm_add_ss(xmmTwoPi, xmm0);
            xmm0 = xmm3;
        } while (_mm_comige_ss(xmmNegPi, xmm0));
    }

    float outVal = _mm_cvtss_f32(xmm0);
#if defined(_MSC_VER) && defined(_M_IX86)
    __asm {
        fld dword ptr [outVal]
        fstp dword ptr [outVal]
    }
#endif
    return outVal;
}

// 0x004D3CD0
std::uint32_t getBehaviorModuleClassNameVa4D() noexcept {
    return 0x00BDBC60U;
}

// 0x004D4020
std::int32_t convertSecondsToFrames4D(
    const void* holder, const void* theGameLogic, float logicFramesPerSecond) noexcept {
    const auto* inner = load<const void*>(holder, 0x00);
    const float seconds = load<float>(inner, 0x08);
    std::int32_t result = 0;

#if defined(_MSC_VER) && defined(_M_IX86)
    if (theGameLogic == nullptr) {
        std::uint16_t savedCw = 0;
        std::uint32_t truncCw = 0;
        std::int64_t tmp64 = 0;
        __asm {
            fld dword ptr [logicFramesPerSecond]
            fmul dword ptr [seconds]
            fnstcw word ptr [savedCw]
            movzx eax, word ptr [savedCw]
            or eax, 0x0C00
            mov dword ptr [truncCw], eax
            fldcw word ptr [truncCw]
            fistp qword ptr [tmp64]
            mov eax, dword ptr [tmp64]
            mov dword ptr [result], eax
            fldcw word ptr [savedCw]
        }
    } else {
        const float scale = load<float>(theGameLogic, 0x1C4);
        std::uint16_t savedCw = 0;
        std::uint32_t truncCw = 0;
        std::int64_t tmp64 = 0;
        __asm {
            fld dword ptr [scale]
            fmul dword ptr [logicFramesPerSecond]
            fmul dword ptr [seconds]
            fnstcw word ptr [savedCw]
            movzx eax, word ptr [savedCw]
            or eax, 0x0C00
            mov dword ptr [truncCw], eax
            fldcw word ptr [truncCw]
            fistp qword ptr [tmp64]
            mov eax, dword ptr [tmp64]
            mov dword ptr [result], eax
            fldcw word ptr [savedCw]
        }
    }
#else
    double prod = static_cast<double>(logicFramesPerSecond) * static_cast<double>(seconds);
    if (theGameLogic != nullptr) {
        prod = static_cast<double>(load<float>(theGameLogic, 0x1C4)) *
               static_cast<double>(logicFramesPerSecond) * static_cast<double>(seconds);
    }
    result = static_cast<std::int32_t>( static_cast<std::int64_t>(prod) );
#endif
    return result;
}

// 0x004D4260
std::uint32_t getCampaignSystemNameVa4D() noexcept {
    return 0x00BDBD9CU;
}

// 0x004D4320
void resetDelayedExperienceLevelGrantSystem4D(void* self) noexcept {
    store<std::uint8_t>(self, 0x1C, 0U);
}

// 0x004D4330
std::uint32_t getDelayedExperienceLevelGrantSystemNameVa4D() noexcept {
    return 0x00BDBDACU;
}

// 0x004D4400
void* assignDamageInfoInputPayload4D(void* dst, const void* src) noexcept {
    for (std::ptrdiff_t off = 0x04; off <= 0x30; off += 4) {
        store<std::uint32_t>(dst, off, load<std::uint32_t>(src, off));
    }
    copyFloatX87(dst, 0x34, src, 0x34);
    store<std::uint32_t>(dst, 0x38, load<std::uint32_t>(src, 0x38));
    copyFloatX87(dst, 0x3C, src, 0x3C);
    for (std::ptrdiff_t off = 0x40; off <= 0x50; off += 4) {
        store<std::uint32_t>(dst, off, load<std::uint32_t>(src, off));
    }
    for (std::ptrdiff_t off = 0x54; off <= 0x68; off += 4) {
        copyFloatX87(dst, off, src, off);
    }
    for (std::ptrdiff_t off = 0x6C; off <= 0x74; off += 4) {
        store<std::uint32_t>(dst, off, load<std::uint32_t>(src, off));
    }
    copyFloatX87(dst, 0x78, src, 0x78);
    for (std::ptrdiff_t off = 0x7C; off <= 0x82; ++off) {
        store<std::uint8_t>(dst, off, load<std::uint8_t>(src, off));
    }
    return dst;
}

// 0x004D4500
std::uint32_t compareCooldownProgress4D(
    const void* lhs, const void* rhs, std::uint32_t currentFrame) noexcept {
    const std::int32_t diffR =
        static_cast<std::int32_t>(load<std::uint32_t>(rhs, 0xA0) - currentFrame);
    const std::int32_t diffL =
        static_cast<std::int32_t>(load<std::uint32_t>(lhs, 0xA0) - currentFrame);
    const float scaleR = load<float>(rhs, 0x38);
    const float scaleL = load<float>(lhs, 0x38);
    const float twoPow32 = bitsToFloat(kTwoPow32Bits);
    std::uint32_t outVal = 0U;

#if defined(_MSC_VER) && defined(_M_IX86)
    __asm {
        fild dword ptr [diffR]
        cmp dword ptr [diffR], 0
        jge skip_r
        fadd dword ptr [twoPow32]
    skip_r:
        fmul dword ptr [scaleR]
        fild dword ptr [diffL]
        cmp dword ptr [diffL], 0
        jge skip_l
        fadd dword ptr [twoPow32]
    skip_l:
        fmul dword ptr [scaleL]
        fxch st(1)
        _emit 0xDF
        _emit 0xF1
        fstp st(0)
        jbe ret_zero
        mov dword ptr [outVal], 1
        jmp done
    ret_zero:
        mov dword ptr [outVal], 0
    done:
    }
#else
    double valR = static_cast<double>(diffR);
    if (diffR < 0) {
        valR += 4294967296.0;
    }
    valR *= static_cast<double>(scaleR);
    double valL = static_cast<double>(diffL);
    if (diffL < 0) {
        valL += 4294967296.0;
    }
    valL *= static_cast<double>(scaleL);
    outVal = (valR > valL) ? 1U : 0U;
#endif
    return outVal;
}

// 0x004D4910
void* initThingTemplateRefWrapper4D(void* self, std::uint32_t templateRef) noexcept {
    store<std::uint32_t>(self, 0x00, 0x00BDBE5CU);
    store<std::uint32_t>(self, 0x04, templateRef);
    return self;
}

// 0x004D4C50
std::uint32_t allocateNextSequenceId4D(void* gameLogic) noexcept {
    const auto current = load<std::uint32_t>(gameLogic, 0x144);
    store<std::uint32_t>(gameLogic, 0x144, current + 1U);
    return current;
}

// 0x004D4CD0
std::uint32_t countLinkedNodesAtBC4D(const void* owner) noexcept {
    const auto* cur = load<const void*>(owner, 0xBC);
    std::uint32_t count = 0U;
    while (cur != nullptr) {
        cur = load<const void*>(cur, 0x7C);
        ++count;
    }
    return count;
}

// 0x004D4EA0
void updateCrcBufferMaxSize4D(void* crcBuffer, std::uint32_t candidateSize) noexcept {
    if (candidateSize > load<std::uint32_t>(crcBuffer, 0x1C)) {
        store<std::uint32_t>(crcBuffer, 0x1C, candidateSize);
    }
}

// 0x004D5530
void updateMaxFloatAt204D(void* self, float candidate) noexcept {
    const __m128 cand = _mm_set_ss(candidate);
    const __m128 cur = _mm_set_ss(load<float>(self, 0x20));
    if (_mm_comigt_ss(cand, cur)) {
        store<float>(self, 0x20, candidate);
    }
}

// 0x004D57A0
void snapAngleToCardinalRad4D(float* inOutAngleRad) noexcept {
    __m128 xmm0 = _mm_set_ss(load<float>(inOutAngleRad, 0));
    const __m128 xmm2 = _mm_set_ss(bitsToFloat(kQuarterPiBits));
    const __m128 xmm3 = _mm_set_ss(bitsToFloat(kPiBits));
    const __m128 xmm4 = _mm_set_ss(bitsToFloat(kTwoPiBits));
    __m128 xmm5 = _mm_set_ss(bitsToFloat(kNegPiBits));

    xmm0 = _mm_sub_ss(xmm0, xmm2);
    store<float>(inOutAngleRad, 0, _mm_cvtss_f32(xmm0));
    if (_mm_comigt_ss(xmm0, xmm3)) {
        do {
            xmm0 = _mm_sub_ss(xmm0, xmm4);
        } while (_mm_comigt_ss(xmm0, xmm3));
    }

    if (_mm_comige_ss(xmm5, xmm0)) {
        do {
            __m128 xmm1 = _mm_add_ss(xmm4, xmm0);
            xmm0 = xmm1;
        } while (_mm_comige_ss(xmm5, xmm0));
    }

    __m128 xmm6 = _mm_setzero_ps();
    store<float>(inOutAngleRad, 0, _mm_cvtss_f32(xmm0));
    if (!_mm_comige_ss(xmm0, xmm6)) {
        // jb 0x4d5826 (includes negative and NaN)
        __m128 xmm1 = _mm_set_ss(bitsToFloat(kNegZeroBits));
        __m128 xmm7 = _mm_sub_ss(xmm1, xmm2);
        if (_mm_comigt_ss(xmm0, xmm7)) {
            store<float>(inOutAngleRad, 0, _mm_cvtss_f32(xmm6));
        } else {
            __m128 xmmHalfPi = _mm_set_ss(bitsToFloat(kHalfPiBits));
            xmm6 = _mm_add_ss(xmmHalfPi, xmm2);
            xmm7 = _mm_sub_ss(xmm1, xmm6);
            if (_mm_comigt_ss(xmm0, xmm7)) {
                xmm1 = _mm_sub_ss(xmm1, xmmHalfPi);
                xmm5 = _mm_set_ss(bitsToFloat(kNegPiBits));
                store<float>(inOutAngleRad, 0, _mm_cvtss_f32(xmm1));
            } else {
                xmm5 = _mm_set_ss(bitsToFloat(kNegPiBits));
                store<float>(inOutAngleRad, 0, _mm_cvtss_f32(xmm3));
            }
        }
    } else {
        if (_mm_comigt_ss(xmm2, xmm0)) {
            store<float>(inOutAngleRad, 0, _mm_cvtss_f32(xmm6));
        } else {
            __m128 xmm1 = _mm_set_ss(bitsToFloat(kHalfPiBits));
            xmm6 = _mm_add_ss(xmm1, xmm2);
            if (_mm_comigt_ss(xmm6, xmm0)) {
                store<float>(inOutAngleRad, 0, _mm_cvtss_f32(xmm1));
            } else {
                store<float>(inOutAngleRad, 0, _mm_cvtss_f32(xmm3));
            }
        }
    }

    xmm0 = _mm_set_ss(load<float>(inOutAngleRad, 0));
    xmm0 = _mm_add_ss(xmm0, xmm2);
    store<float>(inOutAngleRad, 0, _mm_cvtss_f32(xmm0));
    if (_mm_comigt_ss(xmm0, xmm3)) {
        do {
            xmm0 = _mm_sub_ss(xmm0, xmm4);
        } while (_mm_comigt_ss(xmm0, xmm3));
    }
    if (_mm_comige_ss(xmm5, xmm0)) {
        do {
            __m128 xmm1 = _mm_add_ss(xmm4, xmm0);
            xmm0 = xmm1;
        } while (_mm_comige_ss(xmm5, xmm0));
    }
    store<float>(inOutAngleRad, 0, _mm_cvtss_f32(xmm0));
}

// 0x004D5900
bool anyBitFlags288_4D(const void* bits) noexcept {
    for (std::size_t i = 0; i < 9U; ++i) {
        if (load<std::uint32_t>(bits, static_cast<std::ptrdiff_t>(i * 4U)) != 0U) {
            return true;
        }
    }
    return false;
}

// 0x004D5920
bool testRequiredAndForbiddenBitFlags288_4D(
    const void* self, const void* requiredBits, const void* forbiddenBits) noexcept {
    for (std::size_t i = 0; i < 9U; ++i) {
        const auto off = static_cast<std::ptrdiff_t>(i * 4U);
        const auto word = load<std::uint32_t>(self, off);
        const auto forbid = load<std::uint32_t>(forbiddenBits, off);
        if ((word & forbid) != 0U) {
            return false;
        }
        const auto req = load<std::uint32_t>(requiredBits, off);
        if ((word & req) != req) {
            return false;
        }
    }
    return true;
}

// 0x004D5A40
bool intersectsBitFlags480_4D(const void* self, const void* other) noexcept {
    for (std::size_t i = 0; i < 15U; ++i) {
        const auto off = static_cast<std::ptrdiff_t>(i * 4U);
        if ((load<std::uint32_t>(self, off) & load<std::uint32_t>(other, off)) != 0U) {
            return true;
        }
    }
    return false;
}

// 0x004D5AB0
bool anyBitFlags480_4D(const void* self) noexcept {
    for (std::size_t i = 0; i < 15U; ++i) {
        if (load<std::uint32_t>(self, static_cast<std::ptrdiff_t>(i * 4U)) != 0U) {
            return true;
        }
    }
    return false;
}

// 0x004D5B10
bool equalsBitFlags480_4D(const void* lhs, const void* rhs) noexcept {
    return std::memcmp(lhs, rhs, 60U) == 0;
}

// 0x004D5BB0
bool anyObjectStatusMask224_4D(const void* self) noexcept {
    for (std::size_t i = 0; i < 7U; ++i) {
        if (load<std::uint32_t>(self, static_cast<std::ptrdiff_t>(i * 4U)) != 0U) {
            return true;
        }
    }
    return false;
}

// 0x004D5BD0
bool intersectsObjectStatusMask224_4D(const void* self, const void* other) noexcept {
    for (std::size_t i = 0; i < 7U; ++i) {
        const auto off = static_cast<std::ptrdiff_t>(i * 4U);
        if ((load<std::uint32_t>(self, off) & load<std::uint32_t>(other, off)) != 0U) {
            return true;
        }
    }
    return false;
}

// 0x004D5DF0
void* copyConstructDamageInfoInput4D(void* self, const void* other) noexcept {
    store<std::uint32_t>(self, 0x00, 0x00BDC178U);
    for (std::ptrdiff_t off = 0x04; off <= 0x1C; off += 4) {
        store<std::uint32_t>(self, off, load<std::uint32_t>(other, off));
    }
    for (std::ptrdiff_t off = 0x20; off <= 0x28; off += 4) {
        copyFloatX87(self, off, other, off);
    }
    store<std::uint32_t>(self, 0x2C, load<std::uint32_t>(other, 0x2C));
    store<std::uint32_t>(self, 0x30, load<std::uint32_t>(other, 0x30));
    copyFloatX87(self, 0x34, other, 0x34);
    store<std::uint32_t>(self, 0x38, load<std::uint32_t>(other, 0x38));
    copyFloatX87(self, 0x3C, other, 0x3C);
    store<std::uint32_t>(self, 0x40, load<std::uint32_t>(other, 0x40));
    store<std::uint32_t>(self, 0x44, load<std::uint32_t>(other, 0x44));
    for (std::ptrdiff_t off = 0x48; off <= 0x78; off += 4) {
        copyFloatX87(self, off, other, off);
    }
    for (std::ptrdiff_t off = 0x7C; off <= 0x82; ++off) {
        store<std::uint8_t>(self, off, load<std::uint8_t>(other, off));
    }
    return self;
}

// 0x004D6170
std::uint32_t containsPointStrictRegion2D4D(const void* region, const void* point) noexcept {
    const __m128 px = _mm_set_ss(load<float>(point, 0x00));
    const __m128 loX = _mm_set_ss(load<float>(region, 0x00));
    if (!_mm_comigt_ss(px, loX)) {
        return 0U;
    }
    const __m128 hiX = _mm_set_ss(load<float>(region, 0x0C));
    if (!_mm_comigt_ss(hiX, px)) {
        return 0U;
    }
    const __m128 py = _mm_set_ss(load<float>(point, 0x04));
    const __m128 loY = _mm_set_ss(load<float>(region, 0x04));
    if (!_mm_comigt_ss(py, loY)) {
        return 0U;
    }
    const __m128 hiY = _mm_set_ss(load<float>(region, 0x10));
    if (!_mm_comigt_ss(hiY, py)) {
        return 0U;
    }
    return 1U;
}

// 0x004D6500
void* resetMoveGoalResult4D(void* self) noexcept {
    const float one = bitsToFloat(kOneBits);
    store<std::uint32_t>(self, 0x00, 0U);
    store<float>(self, 0x04, one);
    store<float>(self, 0x08, one);
    store<float>(self, 0x0C, one);
    store<float>(self, 0x10, 0.0F);
    store<float>(self, 0x14, 0.0F);
    store<float>(self, 0x18, 0.0F);
    store<std::uint32_t>(self, 0x1C, 0U);
    store<std::uint8_t>(self, 0x20, 1U);
    store<std::uint8_t>(self, 0x21, 1U);
    store<std::uint8_t>(self, 0x22, 0U);
    store<float>(self, 0x24, one);
    return self;
}

// 0x004D6550
void* defaultConstructDamageInfoInput4D(void* self) noexcept {
    const float one = bitsToFloat(kOneBits);
    const float hundred = bitsToFloat(kHundredBits);
    store<std::uint32_t>(self, 0x00, 0x00BDC178U);
    for (std::ptrdiff_t off = 0x04; off <= 0x14; off += 4) {
        store<std::uint32_t>(self, off, 0U);
    }
    store<std::uint32_t>(self, 0x18, 0x0EU);
    store<std::uint32_t>(self, 0x1C, 0x70U);
    store<float>(self, 0x20, 0.0F);
    store<float>(self, 0x24, 0.0F);
    store<float>(self, 0x28, 0.0F);
    store<std::uint32_t>(self, 0x2C, 0U);
    store<std::uint32_t>(self, 0x30, 1U);
    store<float>(self, 0x34, 0.0F);
    store<std::uint32_t>(self, 0x38, 0U);
    store<float>(self, 0x3C, 0.0F);
    store<std::uint32_t>(self, 0x40, 0U);
    store<std::uint32_t>(self, 0x44, 0U);
    for (std::ptrdiff_t off = 0x48; off <= 0x5C; off += 4) {
        store<float>(self, off, 0.0F);
    }
    store<float>(self, 0x60, one);
    store<float>(self, 0x64, one);
    store<float>(self, 0x68, hundred);
    for (std::ptrdiff_t off = 0x6C; off <= 0x78; off += 4) {
        store<float>(self, off, 0.0F);
    }
    for (std::ptrdiff_t off = 0x7C; off <= 0x82; ++off) {
        store<std::uint8_t>(self, off, 0U);
    }
    return self;
}

// 0x004D6680
bool anyBitAt26C4D(const void* self) noexcept {
    return load<std::uint32_t>(self, 0x26C) != 0U;
}

// 0x004D66A0
std::uint32_t testBitAt26C4D(const void* self, std::uint32_t bitIndex) noexcept {
    const std::uint32_t mask = 1U << (bitIndex & 0x1FU);
    const std::uint32_t wordIdx = bitIndex >> 5U;
    const std::uint32_t word =
        load<std::uint32_t>(self, static_cast<std::ptrdiff_t>(0x26CU + wordIdx * 4U));
    return (word & mask) != 0U ? 1U : 0U;
}

// 0x004D6710
std::uint32_t getSpecialAbilityField0C4D(const void* self) noexcept {
    const auto* inner = load<const void*>(self, 0x04);
    const auto* ptr = load<const std::uint32_t*>(inner, 0x0C);
    return ptr != nullptr ? *ptr : 0U;
}

// 0x004D6730
std::uint32_t getSpecialAbilityField104D(const void* self) noexcept {
    const auto* inner = load<const void*>(self, 0x04);
    const auto* ptr = load<const std::uint32_t*>(inner, 0x10);
    return ptr != nullptr ? *ptr : 0U;
}

// 0x004D6750
std::uint32_t hasAnyLowThreeFlagsAt08_4D(const void* self) noexcept {
    const auto* inner = load<const void*>(self, 0x04);
    const auto flags = load<std::uint32_t>(inner, 0x08);
    if (((flags >> 2U) & 1U) != 0U) {
        return 1U;
    }
    if ((flags & 1U) != 0U) {
        return 1U;
    }
    if (((flags >> 1U) & 1U) != 0U) {
        return 1U;
    }
    return 0U;
}

// 0x004D67F0
std::uint32_t getUpdateModuleClassNameVa4D() noexcept {
    return 0x00BDC318U;
}

// 0x004D6960
void stampPassiveAreaEffectMaskedSlots4D(
    void* self, const std::uint32_t* maskWords, std::uint32_t value) noexcept {
    for (std::uint32_t i = 0; i < 15U; ++i) {
        const std::uint32_t bit = 1U << (i & 0x1FU);
        const std::uint32_t word = maskWords[i >> 5U];
        if ((word & bit) != 0U) {
            store<std::uint32_t>(self, static_cast<std::ptrdiff_t>(0x38U + i * 4U), value);
        }
    }
}

// 0x004D69A0
bool isEntryInBeamExcludeList4D(const void* targetEntry, const void* listHeader) noexcept {
    if (listHeader == nullptr) {
        return false;
    }
    const auto count = load<std::uint32_t>(listHeader, 0x00);
    if (count == 0U) {
        return false;
    }
    const auto* begin = load<const void* const*>(listHeader, 0x04);
    const auto* end = begin + count;
    if (begin == end) {
        return false;
    }

    const auto* t1 = load<const void*>(targetEntry, 0x00);
    const auto* t2 = load<const void*>(t1, 0x00);
    const auto* t3 = load<const void*>(t2, 0x00);
    const auto targetKey = load<std::uint32_t>(t3, 0x04);

    for (const auto* cur = begin; cur != end; ++cur) {
        const auto* e1 = *cur;
        const auto* e2 = load<const void*>(e1, 0x00);
        const auto* e3 = load<const void*>(e2, 0x00);
        if (load<std::uint32_t>(e3, 0x04) == targetKey) {
            return true;
        }
    }
    return false;
}

// 0x004D6AB0
void* findCampaignRecordByNestedKey4D(const void* self, std::uint32_t key) noexcept {
    auto* cur = load<std::byte*>(self, 0x0C);
    auto* end = load<std::byte*>(self, 0x10);
    while (cur != end) {
        const auto* p1 = load<const void*>(cur, 0x04);
        const auto* p2 = load<const void*>(p1, 0x00);
        if (load<std::uint32_t>(p2, 0x04) == key) {
            return cur;
        }
        cur += 0x20;
    }
    return nullptr;
}

// 0x004D6FF0
void* assignDamageInfoShort4D(void* self, const void* other) noexcept {
    assignDamageInfoInputPayload4D(
        static_cast<std::byte*>(self) + 0x04, static_cast<const std::byte*>(other) + 0x04);
    copyFloatX87(self, 0x8C, other, 0x8C);
    copyFloatX87(self, 0x90, other, 0x90);
    store<std::uint8_t>(self, 0x94, load<std::uint8_t>(other, 0x94));
    copyFloatX87(self, 0x98, other, 0x98);
    return self;
}

// 0x004D7040
bool removeObjectIdSwapLastAt24_4D(void* self, const void* obj) noexcept {
    const auto targetId = load<std::uint32_t>(obj, 0xDC);
    auto* begin = load<std::uint32_t*>(self, 0x24);
    auto* end = load<std::uint32_t*>(self, 0x28);
    auto* cur = begin;
    while (cur != end) {
        if (*cur == targetId) {
            break;
        }
        ++cur;
    }
    if (cur == end) {
        return false;
    }
    *cur = *(end - 1);
    store<std::uint32_t*>(self, 0x28, end - 1);
    return true;
}

// 0x004D8390
std::uint32_t resolveAIUpgradeTargetTemplateRef4D(const void* self) noexcept {
    constexpr std::uint32_t kFallbackVa = 0x00CA5724U;
    const auto* inner = load<const void*>(self, 0x04);
    const auto kind = load<std::uint32_t>(inner, 0x04);
    std::uint32_t result = 0U;

    switch (kind) {
    case 1U: {
        const auto* p1 = load<const void*>(inner, 0x0C);
        const auto* p2 = load<const void*>(p1, 0x00);
        result = load<std::uint32_t>(p2, 0x04);
        break;
    }
    case 3U:
    case 4U: {
        const auto* p1 = load<const void*>(inner, 0x10);
        const auto* p2 = load<const void*>(p1, 0x00);
        result = load<std::uint32_t>(p2, 0x28);
        break;
    }
    case 6U:
    case 13U: {
        const auto* p1 = load<const void*>(inner, 0x14);
        const auto* p2 = load<const void*>(p1, 0x00);
        result = load<std::uint32_t>(p2, 0x318);
        break;
    }
    default:
        return kFallbackVa;
    }

    return result != 0U ? result : kFallbackVa;
}

// 0x004D86D0
bool isDamageAndSpawnTableEntryFlag90Set4D(
    const void* self, std::uint32_t tableBeginVa, std::uint32_t tableEndVa) noexcept {
    const auto index = load<std::int32_t>(self, 0x00);
    if (index < 0) {
        return false;
    }
    const auto byteDiff = static_cast<std::int32_t>(tableEndVa - tableBeginVa);
    const auto prod =
        static_cast<std::int64_t>(byteDiff) * static_cast<std::int64_t>(static_cast<std::int32_t>(0xD20D20D3U));
    const auto hi = static_cast<std::int32_t>(static_cast<std::uint64_t>(prod) >> 32U) + byteDiff;
    const auto shifted = hi >> 7;
    const auto count =
        shifted + static_cast<std::int32_t>(static_cast<std::uint32_t>(shifted) >> 31U);

    if (index >= count) {
        return false;
    }
    const auto entryAddr =
        static_cast<std::uintptr_t>(tableBeginVa + static_cast<std::uint32_t>(index) * 0x9CU);
    return load<std::uint8_t>(reinterpret_cast<const void*>(entryAddr), 0x90) != 0U;
}

// 0x004D8780
bool matchesObjectStatusFilter4D(const void* filter, const void* object) noexcept {
    if (object == nullptr) {
        return false;
    }
    const void* objStatus = static_cast<const std::byte*>(object) + 0x80;
    const void* reqMask = load<const void*>(filter, 0x58);
    if (reqMask != nullptr && anyObjectStatusMask224_4D(reqMask)) {
        if (!intersectsObjectStatusMask224_4D(objStatus, reqMask)) {
            return false;
        }
    }
    const void* exclMask = load<const void*>(filter, 0x5C);
    if (exclMask != nullptr && anyObjectStatusMask224_4D(exclMask)) {
        if (intersectsObjectStatusMask224_4D(objStatus, exclMask)) {
            return false;
        }
    }
    return true;
}

// 0x004D8810
bool matchesRelationshipFilter4D(
    const void* maskPtr, const void* objA, const void* objB, std::int32_t relationship) noexcept {
    const auto mask = load<std::uint32_t>(maskPtr, 0x00);
    if (mask == 0U) {
        return true;
    }
    if (relationship == 0) {
        return ((mask >> 1U) & 1U) != 0U;
    }
    if (relationship == 1) {
        return ((mask >> 2U) & 1U) != 0U;
    }
    if (relationship == 2) {
        if ((mask & 1U) != 0U) {
            return true;
        }
        if (load<std::uint32_t>(objB, 0x20) != load<std::uint32_t>(objA, 0x20)) {
            return false;
        }
        return ((mask >> 3U) & 1U) != 0U;
    }
    return false;
}

// 0x004D8930
bool findTaggedContactMidpoint4D(const void* holder, void* outGoal40) noexcept {
    auto* cur = load<std::byte*>(holder, 0x1A8);
    auto* end = load<std::byte*>(holder, 0x1AC);
    if (cur == end) {
        return false;
    }

    const void* matched = nullptr;
    while (cur != end) {
        const auto* candidate = load<const void*>(cur, 0x04);
        if (load<std::uint32_t>(candidate, 0x00) == 0x9063FB6CU) {
            matched = candidate;
            break;
        }
        cur += 0x14;
    }
    if (matched == nullptr) {
        return false;
    }

    const float p1x = load<float>(matched, 0x14);
    const float p1y = load<float>(matched, 0x18);
    const float p1z = load<float>(matched, 0x1C);
    const float p2x = load<float>(matched, 0x20);
    const float p2y = load<float>(matched, 0x24);
    const float p2z = load<float>(matched, 0x28);

    const float dy = subSS(p2y, p1y);
    const float dz = subSS(p2z, p1z);
    const float dx = subSS(p2x, p1x);

    const float sqY = mulSS(dy, dy);
    const float sqZ = mulSS(dz, dz);
    const float sqZY = addSS(sqZ, sqY);
    const float sqX = mulSS(dx, dx);
    const float distSq = addSS(sqZY, sqX);

    float dist = 0.0F;
#if defined(_MSC_VER) && defined(_M_IX86)
    __asm {
        fld dword ptr [distSq]
        fsqrt
        fstp dword ptr [dist]
    }
#else
    dist = std::sqrt(distSq);
#endif

    const float half = bitsToFloat(kHalfBits);
    const float fifteen = bitsToFloat(kFifteenBits);
    const float one = bitsToFloat(kOneBits);

    const float midX = addSS(mulSS(dx, half), p1x);
    const float midY = addSS(p1y, mulSS(dy, half));
    const float midZ = addSS(p1z, mulSS(dz, half));

    store<std::uint32_t>(outGoal40, 0x00, 1U);
    store<float>(outGoal40, 0x04, fifteen);
    store<float>(outGoal40, 0x08, dist);
    store<float>(outGoal40, 0x0C, fifteen);
    store<float>(outGoal40, 0x10, midX);
    store<float>(outGoal40, 0x14, midY);
    store<float>(outGoal40, 0x18, midZ);
    store<std::uint32_t>(outGoal40, 0x1C, 0U);
    store<std::uint8_t>(outGoal40, 0x20, 1U);
    store<std::uint8_t>(outGoal40, 0x21, 1U);
    store<std::uint8_t>(outGoal40, 0x22, 0U);
    store<std::uint8_t>(outGoal40, 0x23, 0U);
    store<float>(outGoal40, 0x24, one);
    return true;
}

// 0x004D9230
void buildZRotationMatrix4D(void* outMatrix36, float angleRad) noexcept {
    const float one = bitsToFloat(kOneBits);
#if defined(_MSC_VER) && defined(_M_IX86)
    __asm {
        fld dword ptr [angleRad]
        mov eax, outMatrix36
        fld st(0)
        xorps xmm0, xmm0
        fsin
        movss dword ptr [eax + 0x08], xmm0
        fxch st(1)
        fcos
        fst dword ptr [eax + 0x00]
        fxch st(1)
        fst dword ptr [eax + 0x04]
        fchs
        fstp dword ptr [eax + 0x0C]
        movss dword ptr [eax + 0x14], xmm0
        fstp dword ptr [eax + 0x10]
        movss dword ptr [eax + 0x18], xmm0
        movss dword ptr [eax + 0x1C], xmm0
    }
    store<float>(outMatrix36, 0x20, one);
#else
    const float s = std::sin(angleRad);
    const float c = std::cos(angleRad);
    store<float>(outMatrix36, 0x00, c);
    store<float>(outMatrix36, 0x04, s);
    store<float>(outMatrix36, 0x08, 0.0F);
    store<float>(outMatrix36, 0x0C, -s);
    store<float>(outMatrix36, 0x10, c);
    store<float>(outMatrix36, 0x14, 0.0F);
    store<float>(outMatrix36, 0x18, 0.0F);
    store<float>(outMatrix36, 0x1C, 0.0F);
    store<float>(outMatrix36, 0x20, one);
#endif
}

// 0x004D9280
std::uint32_t countSetBits288_4D(const void* bits) noexcept {
    std::uint32_t total = 0U;
    for (std::size_t i = 0; i < 9U; ++i) {
        total += popcountWord32(load<std::uint32_t>(bits, static_cast<std::ptrdiff_t>(i * 4U)));
    }
    return total;
}

// 0x004DA690
void* assignDOTInfoRecord4D(void* self, const void* other) noexcept {
    assignDamageInfoInputPayload4D(
        static_cast<std::byte*>(self) + 0x04, static_cast<const std::byte*>(other) + 0x04);
    copyFloatX87(self, 0x8C, other, 0x8C);
    copyFloatX87(self, 0x90, other, 0x90);
    store<std::uint8_t>(self, 0x94, load<std::uint8_t>(other, 0x94));
    copyFloatX87(self, 0x98, other, 0x98);
    store<std::uint32_t>(self, 0x9C, load<std::uint32_t>(other, 0x9C));
    store<std::uint32_t>(self, 0xA0, load<std::uint32_t>(other, 0xA0));
    store<std::uint32_t>(self, 0xA4, load<std::uint32_t>(other, 0xA4));
    store<std::uint8_t>(self, 0xA8, load<std::uint8_t>(other, 0xA8));
    return self;
}

// 0x004DA780
void* defaultConstructLuaEventParameters4D(void* self) noexcept {
    store<std::uint32_t>(self, 0x00, 0x00BDC6E8U);
    constexpr std::ptrdiff_t kSlots[3] = {0x04, 0x1C, 0x34};
    for (const auto base : kSlots) {
        store<std::uint32_t>(self, base + 0x00, 0x00BDC43CU);
        store<float>(self, base + 0x04, 0.0F);
        store<std::uint8_t>(self, base + 0x08, 0U);
        store<std::uint32_t>(self, base + 0x0C, 0U);
        store<std::uint32_t>(self, base + 0x10, 0U);
        store<std::uint32_t>(self, base + 0x14, 0U);
    }
    return self;
}

// 0x004DAD10
void* findRegisteredPairByKey4D(void* gameLogic, const std::uint32_t* keyPair) noexcept {
    auto* sentinel = static_cast<std::byte*>(gameLogic) + 0x1B0;
    auto* cur = load<std::byte*>(sentinel, 0x00);
    if (cur == sentinel) {
        return nullptr;
    }
    const std::uint32_t k0 = keyPair[0];
    const std::uint32_t k1 = keyPair[1];
    while (cur != sentinel) {
        if (load<std::uint32_t>(cur, 0x08) == k0 && load<std::uint32_t>(cur, 0x0C) == k1) {
            return cur + 0x08;
        }
        cur = load<std::byte*>(cur, 0x00);
    }
    return nullptr;
}

// 0x004DB8C0
void* resetGlobalWeatherData4D(void* self) noexcept {
    constexpr std::ptrdiff_t kZeroDwords[] = {
        0x00, 0x04, 0x08,
        0x10, 0x14, 0x18,
        0x20, 0x24, 0x28,
        0x30, 0x34, 0x38,
        0x40, 0x44, 0x48, 0x4C,
        0x50, 0x54, 0x58, 0x5C,
        0x60, 0x64, 0x68, 0x6C,
        0x70, 0x74, 0x78, 0x7C,
        0x80, 0x84,
        0x8C, 0x94, 0x98,
    };
    for (const auto off : kZeroDwords) {
        store<std::uint32_t>(self, off, 0U);
    }
    store<std::uint32_t>(self, 0x88, 1U);
    store<std::uint8_t>(self, 0x90, 1U);
    return self;
}

// 0x004DBB30
void* initExitGoalEntry4D(
    void* self,
    std::uint32_t field00,
    std::uint32_t field04,
    std::uint32_t field08,
    std::uint32_t field0C,
    std::uint8_t flag20) noexcept {
    store<std::uint32_t>(self, 0x00, field00);
    store<std::uint32_t>(self, 0x04, field04);
    store<std::uint32_t>(self, 0x08, field08);
    store<std::uint32_t>(self, 0x0C, field0C);
    store<std::uint32_t>(self, 0x10, 0U);
    store<std::uint32_t>(self, 0x14, 0U);
    store<std::uint32_t>(self, 0x18, 0U);
    store<std::uint8_t>(self, 0x20, flag20);
    return self;
}

// 0x004DBBF0
void gridToWorldPlacement4D(
    const void* gridTransform,
    void* outWorldPos,
    std::int32_t gridX,
    std::int32_t gridY,
    std::uint8_t centerInCell) noexcept {
    const float one = bitsToFloat(kOneBits);
    const float fifteen = bitsToFloat(kFifteenBits);

    store<float>(outWorldPos, 0x08, one);
    float x = cvtIntToFloatSS(mulBy30Wrap(gridX));
    float y = cvtIntToFloatSS(mulBy30Wrap(gridY));
    store<float>(outWorldPos, 0x00, x);
    store<float>(outWorldPos, 0x04, y);

    if (centerInCell != 0U) {
        x = addSS(x, fifteen);
        store<float>(outWorldPos, 0x00, x);
        y = addSS(load<float>(outWorldPos, 0x04), fifteen);
        store<float>(outWorldPos, 0x04, y);
    }

    const float py = load<float>(outWorldPos, 0x04);
    const float pz = load<float>(outWorldPos, 0x08);
    const float px = load<float>(outWorldPos, 0x00);

    const float wx = addSS(
        addSS(mulSS(load<float>(gridTransform, 0xAC), py),
              mulSS(load<float>(gridTransform, 0xB8), pz)),
        mulSS(px, load<float>(gridTransform, 0xA0)));

    const float wy = addSS(
        addSS(mulSS(load<float>(gridTransform, 0xA4), px),
              mulSS(load<float>(gridTransform, 0xB0), py)),
        mulSS(load<float>(gridTransform, 0xBC), pz));

    const float wz = addSS(
        addSS(mulSS(load<float>(gridTransform, 0xA8), px),
              mulSS(load<float>(gridTransform, 0xB4), py)),
        mulSS(load<float>(gridTransform, 0xC0), pz));

    store<float>(outWorldPos, 0x00, wx);
    store<float>(outWorldPos, 0x04, wy);
    store<float>(outWorldPos, 0x08, wz);
}

// 0x004DBD20
void snapWorldPointToGrid4D(const void* gridTransform, void* inOutWorldPos) noexcept {
    const float x = load<float>(inOutWorldPos, 0x00);
    const float y = load<float>(inOutWorldPos, 0x04);
    const float one = bitsToFloat(kOneBits);
    store<float>(inOutWorldPos, 0x08, one);

    const float gx = addSS(
        addSS(mulSS(load<float>(gridTransform, 0x88), y),
              mulSS(x, load<float>(gridTransform, 0x7C))),
        load<float>(gridTransform, 0x94));

    const float gy = addSS(
        addSS(mulSS(load<float>(gridTransform, 0x80), x),
              mulSS(load<float>(gridTransform, 0x8C), y)),
        load<float>(gridTransform, 0x98));

    const float gz = addSS(
        addSS(mulSS(load<float>(gridTransform, 0x84), x),
              mulSS(load<float>(gridTransform, 0x90), y)),
        load<float>(gridTransform, 0x9C));

    store<float>(inOutWorldPos, 0x00, gx);
    store<float>(inOutWorldPos, 0x04, gy);
    store<float>(inOutWorldPos, 0x08, gz);

    const float inv30 = bitsToFloat(kInv30Bits);
    const float half = bitsToFloat(kHalfBits);

    const std::int32_t cellX = cvttFloatToIntSS(addSS(mulSS(gx, inv30), half));
    const float qx = cvtIntToFloatSS(mulBy30Wrap(cellX));
    store<float>(inOutWorldPos, 0x08, one);

    const std::int32_t cellY = cvttFloatToIntSS(addSS(mulSS(gy, inv30), half));
    store<float>(inOutWorldPos, 0x00, qx);
    const float qy = cvtIntToFloatSS(mulBy30Wrap(cellY));
    store<float>(inOutWorldPos, 0x04, qy);

    const float wx = addSS(
        addSS(mulSS(load<float>(gridTransform, 0xAC), qy),
              mulSS(qx, load<float>(gridTransform, 0xA0))),
        load<float>(gridTransform, 0xB8));

    const float wy = addSS(
        addSS(mulSS(load<float>(gridTransform, 0xB0), qy),
              mulSS(load<float>(gridTransform, 0xA4), qx)),
        load<float>(gridTransform, 0xBC));

    const float wz = addSS(
        addSS(mulSS(load<float>(gridTransform, 0xB4), qy),
              mulSS(load<float>(gridTransform, 0xA8), qx)),
        load<float>(gridTransform, 0xC0));

    store<float>(inOutWorldPos, 0x00, wx);
    store<float>(inOutWorldPos, 0x04, wy);
    store<float>(inOutWorldPos, 0x08, wz);
}

// 0x004DBEC0
void offsetWorldPointByGridCells4D(
    const void* gridTransform,
    void* outWorldPos,
    const void* inWorldPos,
    std::int32_t deltaCellsX,
    std::int32_t deltaCellsY) noexcept {
    const float one = bitsToFloat(kOneBits);
    copyFloatX87(outWorldPos, 0x00, inWorldPos, 0x00);
    const float x = load<float>(outWorldPos, 0x00);
    store<float>(outWorldPos, 0x08, one);
    copyFloatX87(outWorldPos, 0x04, inWorldPos, 0x04);
    const float y = load<float>(outWorldPos, 0x04);

    const float gx0 = addSS(
        addSS(mulSS(load<float>(gridTransform, 0x88), y),
              mulSS( x, load<float>(gridTransform, 0x7C))),
        load<float>(gridTransform, 0x94));

    const float gy0 = addSS(
        addSS(mulSS(load<float>(gridTransform, 0x80), x),
              mulSS(load<float>(gridTransform, 0x8C), y)),
        load<float>(gridTransform, 0x98));

    store<float>(outWorldPos, 0x00, gx0);
    store<float>(outWorldPos, 0x04, gy0);
    store<float>(outWorldPos, 0x08, one);

    const float gx = addSS(cvtIntToFloatSS(mulBy30Wrap(deltaCellsX)), load<float>(outWorldPos, 0x00));
    store<float>(outWorldPos, 0x00, gx);

    const float gy = addSS(cvtIntToFloatSS(mulBy30Wrap(deltaCellsY)), load<float>(outWorldPos, 0x04));
    store<float>(outWorldPos, 0x04, gy);

    const float py = load<float>(outWorldPos, 0x04);
    const float pz = load<float>(outWorldPos, 0x08);
    const float px = load<float>(outWorldPos, 0x00);

    const float wx = addSS(
        addSS(mulSS(load<float>(gridTransform, 0xAC), py),
              mulSS(load<float>(gridTransform, 0xB8), pz)),
        mulSS(load<float>(gridTransform, 0xA0), px));

    const float wy = addSS(
        addSS(mulSS(load<float>(gridTransform, 0xA4), px),
              mulSS(load<float>(gridTransform, 0xB0), py)),
        mulSS(load<float>(gridTransform, 0xBC), pz));

    const float wz = addSS(
        addSS(mulSS(load<float>(gridTransform, 0xA8), px),
              mulSS(load<float>(gridTransform, 0xB4), py)),
        mulSS(load<float>(gridTransform, 0xC0), pz));

    store<float>(outWorldPos, 0x00, wx);
    store<float>(outWorldPos, 0x04, wy);
    store<float>(outWorldPos, 0x08, wz);
    copyFloatX87(outWorldPos, 0x08, inWorldPos, 0x08);
}

// 0x004DC080
void worldToGridCell4D(
    const void* gridTransform,
    const void* worldPos,
    std::int32_t* outCellX,
    std::int32_t* outCellY,
    std::uint8_t roundFlag) noexcept {
    const float x = load<float>(worldPos, 0x00);
    const float y = load<float>(worldPos, 0x04);

    const float gx = addSS(
        addSS(mulSS(load<float>(gridTransform, 0x88), y),
              mulSS(load<float>(gridTransform, 0x7C), x)),
        load<float>(gridTransform, 0x94));

    const float gy = addSS(
        addSS(mulSS(load<float>(gridTransform, 0x80), x),
              mulSS(load<float>(gridTransform, 0x8C), y)),
        load<float>(gridTransform, 0x98));

    const float bias = (roundFlag != 0U) ? bitsToFloat(kRound51Bits) : 0.0F;
    const float inv30 = bitsToFloat(kInv30Bits);

    *outCellX = cvttFloatToIntSS(addSS(mulSS(gx, inv30), bias));
    *outCellY = cvttFloatToIntSS(addSS(mulSS(gy, inv30), bias));
}

// 0x004DD390
void* defaultConstructMapMissionObjectives4D(void* self) noexcept {
    store<std::uint32_t>(self, 0x04, 0U);
    store<std::uint32_t>(self, 0x08, 0U);
    store<std::uint32_t>(self, 0x0C, 0U);
    store<std::uint32_t>(self, 0x14, 0xFFFFFFFFU);
    store<std::uint32_t>(self, 0x00, 0x00BDC9B8U);
    store<std::uint32_t>(self, 0x18, 0U);
    store<std::uint32_t>(self, 0x1C, 0U);
    store<std::uint32_t>(self, 0x20, 0U);
    return self;
}

// 0x004DD3D0
void* defaultConstructCampaignRecordTable4D(void* self) noexcept {
    store<std::uint32_t>(self, 0x00, 0x00BDC9E0U);
    store<std::uint32_t>(self, 0x04, 0U);
    store<std::uint32_t>(self, 0x08, 0U);
    store<std::uint32_t>(self, 0x0C, 0U);
    store<std::uint32_t>(self, 0x10, 0U);
    store<std::uint32_t>(self, 0x14, 0U);
    store<std::uint8_t>(self, 0x1C, 1U);
    return self;
}

// 0x004DFC30
void snapRotatedFootprintToGrid4D(
    const void* gridTransform,
    const void* footprint,
    void* inOutWorldPos,
    float angleRad) noexcept {
    if (footprint == nullptr) {
        return;
    }

    float cosVal = 0.0F;
    float sinVal = 0.0F;
#if defined(_MSC_VER) && defined(_M_IX86)
    __asm {
        fld dword ptr [angleRad]
        fsincos
        fstp dword ptr [cosVal]
        fstp dword ptr [sinVal]
    }
#else
    cosVal = std::cos(angleRad);
    sinVal = std::sin(angleRad);
#endif

    float halfX = 0.0F;
    float halfY = 0.0F;
    if (load<std::uint8_t>(footprint, 0x68) != 0U) {
        const float scale = load<float>(footprint, 0x90);
        halfY = mulSS(load<float>(footprint, 0x74), scale);
        halfX = halfY;
        if (load<std::uint32_t>(footprint, 0x6C) == 1U) {
            halfY = mulSS(load<float>(footprint, 0x78), scale);
        }
    } else {
        halfX = load<float>(footprint, 0x50);
        halfY = load<float>(footprint, 0x54);
    }

    const float offsetY = addSS(mulSS(cosVal, halfY), mulSS(sinVal, halfX));
    const float offsetX = subSS(mulSS(cosVal, halfX), mulSS(sinVal, halfY));

    float tempPos[3];
    tempPos[0] = addSS(load<float>(inOutWorldPos, 0x00), offsetX);
    tempPos[1] = addSS(offsetY, load<float>(inOutWorldPos, 0x04));
    tempPos[2] = bitsToFloat(kOneBits);

    snapWorldPointToGrid4D(gridTransform, tempPos);

    store<float>(inOutWorldPos, 0x00, subSS(tempPos[0], offsetX));
    store<float>(inOutWorldPos, 0x04, subSS(tempPos[1], offsetY));
}

}  // namespace ra3::gamelogic
