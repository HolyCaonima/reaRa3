#include <ra3/render/W3DLayer56.h>
#include <ra3/render/W3DView.h>

#include <cstring>
#include <sstream>
#include <xmmintrin.h>

namespace ra3::render {
namespace {

std::int32_t g_frameMs = 1;
std::uint32_t g_halfPi = 0x3FC90FDBu;
std::uint8_t g_pause = 0;
std::uint32_t g_latch = 0;
std::uint32_t g_listenerFlag = 0;
std::uint32_t g_listener[3] = {};
void* g_follow = nullptr;
void* g_terrain = nullptr;
void* g_sampleOwner = nullptr;
std::uint32_t g_queryResult = 0;
std::uint32_t g_finishResult = 0;
std::uint32_t g_heightBits = 0;
std::string g_log;

// First plane pointer inside View::sphereVisible. Mutate this offset to prove
// the oracle is watching the comparison inputs.
constexpr std::size_t kSphereFirst = 0x308;

void addLog(const std::string& text) {
    if (!g_log.empty()) {
        g_log += ',';
    }
    g_log += text;
}

std::string hex32(std::uint32_t value) {
    std::ostringstream out;
    out << std::hex << std::nouppercase << value;
    return out.str();
}

template <class T>
T loadAt(const void* object, std::size_t offset) noexcept {
    T value{};
    std::memcpy(&value, static_cast<const std::byte*>(object) + offset, sizeof(T));
    return value;
}

template <class T>
void storeAt(void* object, std::size_t offset, T value) noexcept {
    std::memcpy(static_cast<std::byte*>(object) + offset, &value, sizeof(value));
}

float bitsToFloat(std::uint32_t bits) noexcept {
    float value = 0.0F;
    std::memcpy(&value, &bits, 4);
    return value;
}

std::uint32_t floatToBits(float value) noexcept {
    std::uint32_t bits = 0;
    std::memcpy(&bits, &value, 4);
    return bits;
}

std::uint32_t markerOf(const void* object) noexcept {
    return object == nullptr ? 0 : loadAt<std::uint32_t>(object, 0);
}

int dividedFrames(int dividend, int minimumQuotient) noexcept {
    int quotient = dividend / g_frameMs;
    return quotient < minimumQuotient ? minimumQuotient : quotient;
}

void configureAt(void* view, std::size_t easeOffset, std::uint32_t easeInBits, std::uint32_t easeOutBits,
                 int durationMs) noexcept {
    ParabolicEase ease{};
    const float duration = _mm_cvtss_f32(_mm_cvtsi32_ss(_mm_setzero_ps(), durationMs));
    configureEase(ease, bitsToFloat(easeInBits), bitsToFloat(easeOutBits), duration);
    std::memcpy(static_cast<std::byte*>(view) + easeOffset, &ease, sizeof(ease));
}

float easeAmount(const void* ease, std::uint32_t frames, std::uint32_t current) noexcept {
    const __m128 denom = _mm_cvtsi32_ss(_mm_setzero_ps(), static_cast<int>(frames));
    const __m128 numer = _mm_cvtsi32_ss(_mm_setzero_ps(), static_cast<int>(current));
    const float ratio = _mm_cvtss_f32(_mm_div_ss(numer, denom));
    return bitsToFloat(w3d56EvaluateEaseBits(ease, ratio));
}

void blendScalar(void* view, std::size_t startOffset, std::size_t targetOffset, std::size_t destOffset,
                 float amount) noexcept {
    const __m128 start = _mm_set_ss(loadAt<float>(view, startOffset));
    const __m128 target = _mm_set_ss(loadAt<float>(view, targetOffset));
    const __m128 delta = _mm_sub_ss(target, start);
    const __m128 scaled = _mm_mul_ss(delta, _mm_set_ss(amount));
    const __m128 value = _mm_add_ss(scaled, start);
    storeAt<float>(view, destOffset, _mm_cvtss_f32(value));
}

void updateScalar(void* view, std::size_t stepOffset, std::size_t frameOffset, std::size_t easeOffset,
                  std::size_t startOffset, std::size_t targetOffset, std::size_t destOffset,
                  std::size_t activeOffset, bool scalePitch) noexcept {
    const std::uint32_t current = loadAt<std::uint32_t>(view, stepOffset) + 1;
    const std::uint32_t frames = loadAt<std::uint32_t>(view, frameOffset);
    storeAt<std::uint32_t>(view, stepOffset, current);
    if (current <= frames) {
        const float amount = easeAmount(static_cast<const std::byte*>(view) + easeOffset, frames, current);
        blendScalar(view, startOffset, targetOffset, destOffset, amount);
        if (scalePitch) {
            const __m128 value = _mm_set_ss(loadAt<float>(view, destOffset));
            const __m128 scaled = _mm_mul_ss(value, _mm_set_ss(bitsToFloat(0x3C8EFA35u)));
            storeAt<float>(view, destOffset, _mm_cvtss_f32(scaled));
        }
    }
    if (current >= frames) {
        storeAt<std::uint8_t>(view, activeOffset, 0);
        if (scalePitch) {
            const __m128 target = _mm_set_ss(loadAt<float>(view, targetOffset));
            const __m128 scaled = _mm_mul_ss(target, _mm_set_ss(bitsToFloat(0x3C8EFA35u)));
            storeAt<float>(view, destOffset, _mm_cvtss_f32(scaled));
        } else {
            storeAt<std::uint32_t>(view, destOffset, loadAt<std::uint32_t>(view, targetOffset));
        }
    }
}

void markRecordDirty(void* self) noexcept {
    void* target = loadAt<void*>(self, 0x34);
    const std::uint32_t maskA = loadAt<std::uint32_t>(self, 0x40);
    const std::uint32_t maskB = loadAt<std::uint32_t>(self, 0x44);
    storeAt<std::uint8_t>(self, 0x58, 1);
    storeAt<std::uint32_t>(target, 0x108, loadAt<std::uint32_t>(target, 0x108) | maskA);
    storeAt<std::uint32_t>(target, 0x10C, loadAt<std::uint32_t>(target, 0x10C) | maskB);
}

using SlotFn = void(__fastcall*)(void*, void*, void*);

void __fastcall xferSlot18(void*, void*, void* argument) {
    const auto* bytes = static_cast<const std::uint8_t*>(argument);
    std::ostringstream text;
    text << "s18:" << std::hex << std::nouppercase << static_cast<int>(bytes[0]) << ":"
         << static_cast<int>(bytes[1]);
    addLog(text.str());
}

void writeXferDword(void* argument, const char* name) {
    const std::uint32_t marker = 0xA1A1A1A1u;
    std::memcpy(argument, &marker, 4);
    addLog(name);
}

void __fastcall xferSlot58(void*, void*, void* argument) { writeXferDword(argument, "s58"); }
void __fastcall xferSlot64(void*, void*, void* argument) { writeXferDword(argument, "s64"); }
void __fastcall xferSlot70(void*, void*, void* argument) { writeXferDword(argument, "s70"); }
void __fastcall xferSlot84(void*, void*, void* argument) { writeXferDword(argument, "s84"); }

SlotFn g_xferTable[40] = {};

void callXferSlot(void* xfer, int byteOffset, void* argument) noexcept {
    auto** table = *reinterpret_cast<SlotFn**>(xfer);
    table[byteOffset / 4](xfer, nullptr, argument);
}

}  // namespace

void w3d56ResetProbe() noexcept {
    g_frameMs = 1;
    g_halfPi = 0x3FC90FDBu;
    g_pause = 0;
    g_latch = 0;
    g_listenerFlag = 0;
    g_listener[0] = g_listener[1] = g_listener[2] = 0;
    g_follow = nullptr;
    g_terrain = nullptr;
    g_sampleOwner = nullptr;
    g_queryResult = 0;
    g_finishResult = 0;
    g_heightBits = 0;
    g_log.clear();
}

void w3d56SetFrameMillis(std::int32_t frameMillis) noexcept { g_frameMs = frameMillis; }
void w3d56SetHalfPiBits(std::uint32_t bits) noexcept { g_halfPi = bits; }
void w3d56SetPauseByte(std::uint8_t value) noexcept { g_pause = value; }
void w3d56SetLatch(std::uint32_t value) noexcept { g_latch = value; }
void w3d56SetListenerFlag(std::uint32_t value) noexcept { g_listenerFlag = value; }
void w3d56SetFollow(void* object) noexcept { g_follow = object; }
void w3d56SetTerrain(void* object) noexcept { g_terrain = object; }
void w3d56SetSampleOwner(void* object) noexcept { g_sampleOwner = object; }
void w3d56SetQueryResult(std::uint32_t value) noexcept { g_queryResult = value; }
void w3d56SetFinishResult(std::uint32_t value) noexcept { g_finishResult = value; }
void w3d56SetHeightBits(std::uint32_t bits) noexcept { g_heightBits = bits; }

void w3d56PrepareXfer(void* xfer, std::uint32_t version) noexcept {
    g_xferTable[0x18 / 4] = xferSlot18;
    g_xferTable[0x58 / 4] = xferSlot58;
    g_xferTable[0x64 / 4] = xferSlot64;
    g_xferTable[0x70 / 4] = xferSlot70;
    g_xferTable[0x84 / 4] = xferSlot84;
    storeAt<SlotFn*>(xfer, 0, g_xferTable);
    storeAt<std::uint32_t>(xfer, 4, version);
}
std::uint8_t w3d56PauseByte() noexcept { return g_pause; }
std::uint32_t w3d56Latch() noexcept { return g_latch; }
std::uint32_t w3d56ListenerFlag() noexcept { return g_listenerFlag; }
std::uint32_t w3d56ListenerBits(int index) noexcept { return g_listener[index]; }

std::string w3d56TakeLog() {
    std::string out = g_log.empty() ? "." : g_log;
    g_log.clear();
    return out;
}

void w3d56SetControlWord() noexcept {
    unsigned short control = 0x037F;
    __asm { fldcw control }
}

std::uint32_t w3d56EvaluateEaseBits(const void* ease, float position) noexcept {
    const float one = 1.0F;
    const float two = 2.0F;
    float pos = position;
    float easeIn = 0.0F;
    float span = 0.0F;
    std::uint32_t bits = 0;
    w3d56SetControlWord();
    __asm {
        mov ecx, ease
        xorps xmm1, xmm1
        movss xmm0, dword ptr pos
        movss xmm3, dword ptr one
        comiss xmm1, xmm0
        ja ease_lo
        comiss xmm0, xmm3
        jbe ease_ready
        comiss xmm1, xmm0
        jbe ease_hi
    ease_lo:
        movaps xmm0, xmm1
        jmp ease_store
    ease_hi:
        movaps xmm0, xmm3
    ease_store:
        movss dword ptr pos, xmm0
    ease_ready:
        movss xmm2, dword ptr [ecx + 4]
        movss xmm1, dword ptr [ecx]
        movss xmm0, dword ptr pos
        comiss xmm1, xmm0
        movaps xmm4, xmm2
        addss xmm4, xmm3
        subss xmm4, xmm1
        movss dword ptr easeIn, xmm1
        movss dword ptr span, xmm4
        jbe ease_mid
        fld dword ptr pos
        fmul st(0), st(0)
        fld dword ptr easeIn
        fmul dword ptr span
        fdivp st(1), st(0)
        jmp ease_done
    ease_mid:
        comiss xmm2, xmm0
        jb ease_out
        fld dword ptr pos
        fld dword ptr easeIn
        fsub st(1), st(0)
        fxch st(1)
        fadd st(0), st(0)
        faddp st(1), st(0)
        fdiv dword ptr span
        jmp ease_done
    ease_out:
        fld dword ptr [ecx + 4]
        fld dword ptr pos
        fld st(0)
        fsub st(0), st(2)
        fld dword ptr two
        fmul st(1), st(0)
        fld st(3)
        fmul st(0), st(4)
        faddp st(2), st(0)
        fld st(2)
        fmulp st(3), st(0)
        fxch st(1)
        fsubrp st(2), st(0)
        fld1
        fsub st(0), st(3)
        fdivp st(2), st(0)
        fld dword ptr easeIn
        fsub st(3), st(0)
        fxch st(3)
        fmulp st(1), st(0)
        faddp st(1), st(0)
        faddp st(1), st(0)
        fdiv dword ptr span
    ease_done:
        fstp dword ptr bits
    }
    return bits;
}

void view56BeginTrackedMove(void* view, std::uint32_t deltaBits, std::uint32_t milliseconds,
                             std::uint32_t flagWord, std::uint32_t easeInBits, std::uint32_t easeOutBits) noexcept {
    int ms = static_cast<int>(milliseconds);
    if (ms < 1) {
        ms = 1;
    }
    storeAt<std::uint32_t>(view, 0x3F8, 0);
    storeAt<std::uint8_t>(view, 0x404, 0);
    storeAt<std::uint32_t>(view, 0x3E8, static_cast<std::uint32_t>(dividedFrames(ms, 1)));
    const float base = loadAt<float>(view, 0x28);
    const __m128 scaled = _mm_mul_ss(_mm_set_ss(bitsToFloat(deltaBits)), _mm_set_ss(bitsToFloat(g_halfPi)));
    const __m128 sum = _mm_add_ss(scaled, _mm_set_ss(base));
    storeAt<float>(view, 0x408, base);
    storeAt<float>(view, 0x40C, _mm_cvtss_f32(sum));
    const std::uint32_t stamp = loadAt<std::uint32_t>(view, 0x2618);
    storeAt<std::uint32_t>(view, 0x3EC, 0);
    storeAt<std::uint8_t>(view, 0x418, 1);
    storeAt<std::uint32_t>(view, 0x3F0, stamp);
    storeAt<std::uint32_t>(view, 0x3F4, stamp);
    configureAt(view, 0x3FC, easeInBits, easeOutBits, ms);
    storeAt<std::uint8_t>(view, 0x2670, static_cast<std::uint8_t>(flagWord));
}

void view56BeginTrackedMoveHeld(void* view, std::uint32_t anchorBits, std::uint32_t milliseconds,
                                std::uint32_t holdMilliseconds, std::uint32_t easeInBits,
                                std::uint32_t easeOutBits, std::uint32_t extraBits) noexcept {
    storeAt<std::uint8_t>(view, 0x404, 1);
    const int hold = static_cast<int>(holdMilliseconds);
    const int holdDividend = hold < 1 ? 0 : hold;
    const int holdFrames = holdDividend / g_frameMs;
    storeAt<std::uint32_t>(view, 0x3F8, static_cast<std::uint32_t>(holdFrames < 1 ? 0 : holdFrames));
    int ms = static_cast<int>(milliseconds);
    if (ms < 1) {
        ms = 1;
    }
    storeAt<std::uint32_t>(view, 0x3E8, static_cast<std::uint32_t>(dividedFrames(ms, 1)));
    storeAt<std::uint32_t>(view, 0x408, anchorBits);
    const std::uint32_t stamp = loadAt<std::uint32_t>(view, 0x2618);
    storeAt<std::uint32_t>(view, 0x3EC, 0);
    storeAt<std::uint8_t>(view, 0x418, 1);
    storeAt<std::uint32_t>(view, 0x3F0, stamp);
    storeAt<std::uint32_t>(view, 0x3F4, stamp);
    configureAt(view, 0x3FC, easeInBits, easeOutBits, ms);
    storeAt<std::uint32_t>(view, 0x414, extraBits);
}

void view56UpdateZoom(void* view) noexcept {
    updateScalar(view, 0x448, 0x444, 0x45C, 0x44C, 0x450, 0x40, 0x464, false);
}

void view56UpdatePitch(void* view) noexcept {
    updateScalar(view, 0x46C, 0x468, 0x488, 0x478, 0x47C, 0x74, 0x490, true);
}

void view56UpdateHeight(void* view) noexcept {
    updateScalar(view, 0x420, 0x41C, 0x438, 0x428, 0x42C, 0x78, 0x440, false);
}

void view56UpdateAngle(void* view) noexcept {
    updateScalar(view, 0x498, 0x494, 0x4B0, 0x4A0, 0x4A4, 0x30, 0x4B8, false);
}

void view56SetPauseFlag(void* view, std::uint32_t enabled) noexcept {
    (void)view;
    if (static_cast<std::uint8_t>(enabled) != 0) {
        g_pause = 1;
        return;
    }
    if (g_pause != 0) {
        g_pause = 0;
    }
}

std::uint32_t view56SphereVisible(const void* view, const void* center, std::uint32_t radiusBits) noexcept {
    const auto* first = loadAt<const float*>(view, kSphereFirst);
    const auto* last = loadAt<const float*>(view, kSphereFirst + 4);
    const float cx = loadAt<float>(center, 0);
    const float cy = loadAt<float>(center, 4);
    const float cz = loadAt<float>(center, 8);
    const float radius = bitsToFloat(radiusBits);
    for (; first != last; first += 4) {
        const __m128 accY = _mm_mul_ss(_mm_set_ss(first[1]), _mm_set_ss(cy));
        const __m128 accZ = _mm_mul_ss(_mm_set_ss(first[2]), _mm_set_ss(cz));
        __m128 acc = _mm_add_ss(accY, accZ);
        const __m128 accX = _mm_mul_ss(_mm_set_ss(first[0]), _mm_set_ss(cx));
        acc = _mm_add_ss(acc, accX);
        acc = _mm_sub_ss(acc, _mm_set_ss(first[3]));
        if (_mm_comigt_ss(acc, _mm_set_ss(radius))) {
            return 0;
        }
    }
    return 1;
}

std::uint32_t view56PublishListener(void* view) noexcept {
    if ((g_listenerFlag & 1u) == 0) {
        g_listenerFlag |= 1u;
    }
    g_listener[0] = loadAt<std::uint32_t>(view, 0x15C);
    g_listener[1] = loadAt<std::uint32_t>(view, 0x16C);
    g_listener[2] = loadAt<std::uint32_t>(view, 0x17C);
    return 0x00DF4918u;
}

std::uint32_t view56FollowHeightRatio(const void* view, std::uint32_t numeratorBits) noexcept {
    std::uint32_t bits = 0;
    const float one = 1.0F;
    const float numerator = bitsToFloat(numeratorBits);
    void* follow = g_follow;
    w3d56SetControlWord();
    __asm {
        mov eax, follow
        movss xmm0, dword ptr [eax + 8]
        comiss xmm0, dword ptr one
        jbe follow_one
        fld dword ptr numerator
        mov ecx, view
        fdiv dword ptr [ecx + 0x2624]
        fstp dword ptr bits
        jmp follow_done
    follow_one:
        fld1
        fstp dword ptr bits
    follow_done:
    }
    return bits;
}

void view56BeginZoom(void* view, std::uint32_t targetBits, std::uint32_t milliseconds,
                     std::uint32_t easeInBits, std::uint32_t easeOutBits) noexcept {
    int ms = static_cast<int>(milliseconds);
    storeAt<std::uint8_t>(view, 0x464, 1);
    if (ms < 1) {
        ms = 1;
    }
    storeAt<std::uint32_t>(view, 0x44C, loadAt<std::uint32_t>(view, 0x40));
    storeAt<std::uint32_t>(view, 0x450, targetBits);
    storeAt<std::uint32_t>(view, 0x444, static_cast<std::uint32_t>(dividedFrames(ms, 1)));
    storeAt<std::uint32_t>(view, 0x448, 0);
    configureAt(view, 0x45C, easeInBits, easeOutBits, ms);
    if (ms == 1) {
        view56UpdateZoom(view);
    }
}

void view56BeginHeight(void* view, std::uint32_t targetBits, std::uint32_t milliseconds,
                       std::uint32_t easeInBits, std::uint32_t easeOutBits) noexcept {
    int ms = static_cast<int>(milliseconds);
    if (ms < 1) {
        ms = 1;
    }
    storeAt<std::uint32_t>(view, 0x41C, static_cast<std::uint32_t>(dividedFrames(ms, 1)));
    storeAt<std::uint32_t>(view, 0x428, loadAt<std::uint32_t>(view, 0x78));
    storeAt<std::uint32_t>(view, 0x42C, targetBits);
    storeAt<std::uint32_t>(view, 0x420, 0);
    storeAt<std::uint8_t>(view, 0x440, 1);
    configureAt(view, 0x438, easeInBits, easeOutBits, ms);
    if (ms == 1) {
        view56UpdateHeight(view);
    }
}

void view56BeginAngle(void* view, std::uint32_t targetBits, std::uint32_t milliseconds,
                      std::uint32_t easeInBits, std::uint32_t easeOutBits) noexcept {
    int ms = static_cast<int>(milliseconds);
    if (ms < 1) {
        ms = 1;
    }
    storeAt<std::uint32_t>(view, 0x494, static_cast<std::uint32_t>(dividedFrames(ms, 1)));
    storeAt<std::uint32_t>(view, 0x4A0, loadAt<std::uint32_t>(view, 0x30));
    storeAt<std::uint32_t>(view, 0x4A4, targetBits);
    storeAt<std::uint32_t>(view, 0x498, 0);
    storeAt<std::uint8_t>(view, 0x4B8, 1);
    configureAt(view, 0x4B0, easeInBits, easeOutBits, ms);
    if (ms == 1) {
        view56UpdateAngle(view);
    }
}

void view56TranslateSamples(void* view, const void* position) noexcept {
    if (loadAt<std::uint8_t>(view, 0x418) != 0 || loadAt<std::int32_t>(view, 0x2594) != 1) {
        return;
    }
    const int current = loadAt<std::int32_t>(view, 0x252C);
    const std::size_t anchor = 0x4E8 + static_cast<std::size_t>(current) * 0x14;
    const __m128 dx = _mm_sub_ss(_mm_set_ss(loadAt<float>(position, 0)), _mm_set_ss(loadAt<float>(view, anchor)));
    const __m128 dy = _mm_sub_ss(_mm_set_ss(loadAt<float>(position, 4)), _mm_set_ss(loadAt<float>(view, anchor + 4)));
    const __m128 dz = _mm_sub_ss(_mm_set_ss(loadAt<float>(position, 8)), _mm_set_ss(loadAt<float>(view, anchor + 8)));
    if (current < 2) {
        return;
    }
    for (int index = 2; index <= current; ++index) {
        const std::size_t offset = 0x4E8 + static_cast<std::size_t>(index) * 0x14;
        storeAt<float>(view, offset, _mm_cvtss_f32(_mm_add_ss(dx, _mm_set_ss(loadAt<float>(view, offset)))));
        storeAt<float>(view, offset + 4, _mm_cvtss_f32(_mm_add_ss(dy, _mm_set_ss(loadAt<float>(view, offset + 4)))));
        storeAt<float>(view, offset + 8, _mm_cvtss_f32(_mm_add_ss(dz, _mm_set_ss(loadAt<float>(view, offset + 8)))));
    }
}

void view56ClearCameraMotion(void* view) noexcept {
    const float zero = 0.0F;
    storeAt<std::uint32_t>(view, 0x2594, 0);
    storeAt<std::uint8_t>(view, 0x418, 0);
    storeAt<std::uint8_t>(view, 0x440, 0);
    storeAt<std::uint8_t>(view, 0x464, 0);
    storeAt<std::uint8_t>(view, 0x4B9, 0);
    storeAt<std::uint8_t>(view, 0x4B8, 0);
    storeAt<std::uint8_t>(view, 0x260C, 0);
    storeAt<float>(view, 0x267C, zero);
    storeAt<float>(view, 0x2680, zero);
    storeAt<float>(view, 0x2684, zero);
    storeAt<std::uint32_t>(view, 0x5C, 0);
    storeAt<std::uint32_t>(view, 0x60, 0);
    storeAt<float>(view, 0x6C, zero);
    storeAt<float>(view, 0x70, zero);
}

std::uint32_t view56EmbeddedFlagFloat(const void* embedded) noexcept {
    const int value = loadAt<std::uint8_t>(embedded, 0x18);
    return floatToBits(static_cast<float>(value));
}

void view56EmbeddedSet4(void* embedded, std::uint32_t bits) noexcept { storeAt<std::uint32_t>(embedded, 4, bits); }
void view56EmbeddedSet8(void* embedded, std::uint32_t bits) noexcept { storeAt<std::uint32_t>(embedded, 8, bits); }
void view56EmbeddedSet12(void* embedded, std::uint32_t bits) noexcept { storeAt<std::uint32_t>(embedded, 0x0C, bits); }
void view56EmbeddedSet16(void* embedded, std::uint32_t bits) noexcept { storeAt<std::uint32_t>(embedded, 0x10, bits); }

std::uint32_t bounds56Copy(const void* self, void* out) noexcept {
    std::memcpy(out, static_cast<const std::byte*>(self) + 0x0C, 16);
    return loadAt<std::uint8_t>(self, 8);
}

void bounds56Assign(void* self, std::uint32_t enabled, const void* rect) noexcept {
    const auto flag = static_cast<std::uint8_t>(enabled);
    storeAt<std::uint8_t>(self, 8, flag);
    if (flag == 0) {
        return;
    }
    std::memcpy(static_cast<std::byte*>(self) + 0x0C, rect, 16);
}

void bounds56Intersect(void* self, const void* rect) noexcept {
    if (loadAt<std::uint8_t>(self, 8) == 0) {
        storeAt<std::uint8_t>(self, 8, 1);
        std::memcpy(static_cast<std::byte*>(self) + 0x0C, rect, 16);
        return;
    }
    const float xmin = loadAt<float>(self, 0x0C);
    const float ymin = loadAt<float>(self, 0x10);
    const float xmax = loadAt<float>(self, 0x14);
    const float ymax = loadAt<float>(self, 0x18);
    const float ax = loadAt<float>(rect, 0);
    const float ay = loadAt<float>(rect, 4);
    const float bx = loadAt<float>(rect, 8);
    const float by = loadAt<float>(rect, 0x0C);
    if (ax >= xmax || xmin >= bx || ay >= ymax || ymin >= by) {
        const std::uint32_t zero = 0;
        storeAt<std::uint32_t>(self, 0x10, zero);
        storeAt<std::uint32_t>(self, 0x14, zero);
        storeAt<std::uint32_t>(self, 0x0C, zero);
        storeAt<std::uint32_t>(self, 0x18, zero);
        return;
    }
    storeAt<float>(self, 0x0C, xmin > ax ? xmin : ax);
    storeAt<float>(self, 0x10, ymin > ay ? ymin : ay);
    storeAt<float>(self, 0x14, bx > xmax ? xmax : bx);
    storeAt<float>(self, 0x18, by > ymax ? ymax : by);
}

void terrain56FillExtent(const void* self, void* out) noexcept {
    const float negZero = bitsToFloat(0x80000000u);
    const float ten = bitsToFloat(0x41200000u);
    storeAt<std::uint32_t>(out, 0, 0);
    storeAt<std::uint32_t>(out, 4, 0);
    const void* child = loadAt<const void*>(g_terrain, 0x14);
    const __m128 border = _mm_mul_ss(
        _mm_cvtsi32_ss(_mm_setzero_ps(), loadAt<int>(child, 0x2C)), _mm_set_ss(ten));
    const float borderF = _mm_cvtss_f32(border);
    storeAt<float>(out, 0, _mm_cvtss_f32(_mm_sub_ss(_mm_set_ss(negZero), _mm_set_ss(borderF))));
    storeAt<float>(out, 4, _mm_cvtss_f32(_mm_sub_ss(_mm_set_ss(loadAt<float>(out, 4)), _mm_set_ss(borderF))));
    const float x = _mm_cvtss_f32(_mm_mul_ss(
        _mm_cvtsi32_ss(_mm_setzero_ps(), loadAt<int>(self, 0x40)), _mm_set_ss(ten)));
    storeAt<float>(out, 0x0C, _mm_cvtss_f32(_mm_sub_ss(_mm_set_ss(x), _mm_set_ss(borderF))));
    const float y = _mm_cvtss_f32(_mm_mul_ss(
        _mm_cvtsi32_ss(_mm_setzero_ps(), loadAt<int>(self, 0x44)), _mm_set_ss(ten)));
    storeAt<float>(out, 0x10, _mm_cvtss_f32(_mm_sub_ss(_mm_set_ss(y), _mm_set_ss(borderF))));
    storeAt<std::uint32_t>(out, 8, loadAt<std::uint32_t>(self, 0x1438));
    storeAt<std::uint32_t>(out, 0x14, loadAt<std::uint32_t>(self, 0x143C));
}

void terrain56HeightOrZero(void* self, std::uint32_t xBits, std::uint32_t yBits,
                           std::uint32_t* eaxOut, std::uint32_t* stOut) noexcept {
    (void)self;
    if (g_terrain == nullptr) {
        *eaxOut = 0;
        *stOut = 0;
        return;
    }
    addLog("ht:" + hex32(markerOf(g_terrain)) + ":" + hex32(xBits) + ":" + hex32(yBits));
    *eaxOut = 0;
    *stOut = g_heightBits;
}

void terrain56SyncCell(void* self, std::uint32_t cell) noexcept {
    addLog("a:" + hex32(markerOf(g_terrain)) + ":" + hex32(cell));
    addLog("b:" + hex32(markerOf(self)) + ":" + hex32(cell));
}

void terrain56XferHeader(void* self, void* xfer) noexcept {
    std::uint8_t mark[2] = {1, 1};
    callXferSlot(xfer, 0x18, mark);
    addLog("body:" + hex32(markerOf(self)));
}

void terrain56ForwardA(void* self, std::uint32_t a0, std::uint32_t a1Bits, std::uint32_t a2) noexcept {
    void* child = loadAt<void*>(self, 0x28);
    if (child == nullptr) {
        return;
    }
    addLog("fa:" + hex32(markerOf(child)) + ":" + hex32(a0) + ":" + hex32(a1Bits) + ":" + hex32(a2));
}

void terrain56ForwardB(void* self, std::uint32_t a0, std::uint32_t a1, std::uint32_t a2Bits) noexcept {
    void* child = loadAt<void*>(self, 0x28);
    if (child == nullptr) {
        return;
    }
    addLog("fb:" + hex32(markerOf(child)) + ":" + hex32(a0) + ":" + hex32(a1) + ":" + hex32(a2Bits));
}

void terrain56ForwardScorch(void* self, std::uint32_t a0, std::uint32_t a1Bits) noexcept {
    void* child = loadAt<void*>(self, 0x28);
    if (child == nullptr) {
        return;
    }
    addLog("fc:" + hex32(markerOf(child)) + ":" + hex32(a0) + ":" + hex32(a1Bits));
}

std::uint32_t terrain56SampleIndex(void* self, std::uint32_t xBits, std::uint32_t yBits) noexcept {
    void* map = loadAt<void*>(self, 0x30);
    if (map == nullptr) {
        return 0;
    }
    addLog("q:" + hex32(markerOf(map)) + ":" + hex32(xBits) + ":" + hex32(yBits));
    addLog("fin:" + hex32(markerOf(g_sampleOwner)) + ":" + hex32(g_queryResult));
    return g_finishResult;
}

std::uint32_t client56Latch(void* self, std::uint32_t value, std::uint32_t ignored) noexcept {
    (void)self;
    (void)ignored;
    std::uint32_t eax = static_cast<std::int32_t>(g_latch) > 0 ? 1u : 0u;
    eax = (eax - 1u) & value;
    g_latch = eax;
    return (eax & 0xFFFFFF00u) | 1u;
}

void client56SetFlag(void* self, std::uint32_t value) noexcept {
    (void)self;
    if (g_terrain == nullptr) {
        return;
    }
    void* flag = loadAt<void*>(g_terrain, 0xA8);
    if (flag == nullptr) {
        return;
    }
    storeAt<std::uint8_t>(flag, 0x3D, static_cast<std::uint8_t>(value));
    storeAt<std::uint8_t>(flag, 0x3C, 1);
}

void record56WriteVec3(void* self, const void* src, std::uint32_t index) noexcept {
    auto* base = loadAt<std::byte*>(self, 0x50);
    storeAt<void*>(self, 0x4C, base);
    std::memcpy(base + static_cast<std::size_t>(index) * 12, src, 12);
    markRecordDirty(self);
}

void record56WriteFloatUnlessEqual(void* self, const void* src, std::uint32_t index) noexcept {
    const auto slot = reinterpret_cast<std::uintptr_t>(loadAt<void*>(self, 0x4C))
                      + static_cast<std::uintptr_t>(index) * 4;
    if (slot != 0) {
        const float have = loadAt<float>(reinterpret_cast<const void*>(slot), 0);
        const float want = loadAt<float>(src, 0);
        if (have == want) {
            return;
        }
    }
    auto* base = loadAt<std::byte*>(self, 0x50);
    storeAt<void*>(self, 0x4C, base);
    std::memcpy(base + static_cast<std::size_t>(index) * 4, src, 4);
    markRecordDirty(self);
}

void record56WriteVec4(void* self, const void* src, std::uint32_t index) noexcept {
    auto* base = loadAt<std::byte*>(self, 0x50);
    storeAt<void*>(self, 0x4C, base);
    std::memcpy(base + (static_cast<std::size_t>(index) << 4), src, 16);
    markRecordDirty(self);
}

void record56WritePair(void* self, const void* src, std::uint32_t index) noexcept {
    auto* base = loadAt<std::byte*>(self, 0x50);
    storeAt<void*>(self, 0x4C, base);
    std::memcpy(base + static_cast<std::size_t>(index) * 8, src, 8);
    markRecordDirty(self);
}

void mat56Transpose(void* dst, const void* src) noexcept {
    const auto* in = static_cast<const std::uint32_t*>(src);
    auto* out = static_cast<std::uint32_t*>(dst);
    for (int row = 0; row < 4; ++row) {
        out[row * 4 + 0] = in[row];
        out[row * 4 + 1] = in[4 + row];
        out[row * 4 + 2] = in[8 + row];
        out[row * 4 + 3] = in[12 + row];
    }
}

void pose56CopyTranslation(const void* self, void* out) noexcept {
    storeAt<std::uint32_t>(out, 0, loadAt<std::uint32_t>(self, 0x50));
    storeAt<std::uint32_t>(out, 4, loadAt<std::uint32_t>(self, 0x54));
    storeAt<std::uint32_t>(out, 8, loadAt<std::uint32_t>(self, 0x58));
}

void scorch56Xfer(void* self, void* xfer) noexcept {
    const std::uint32_t version = loadAt<std::uint32_t>(xfer, 4);
    if (((version >> 3) & 1u) != 0) {
        return;
    }
    std::uint8_t mark[2] = {1, 1};
    callXferSlot(xfer, 0x18, mark);
    callXferSlot(xfer, 0x58, static_cast<std::byte*>(self) + 4);
    callXferSlot(xfer, 0x64, static_cast<std::byte*>(self) + 0x10);
    callXferSlot(xfer, 0x70, static_cast<std::byte*>(self) + 0x1C);
    callXferSlot(xfer, 0x84, static_cast<std::byte*>(self) + 0x20);
    callXferSlot(xfer, 0x64, static_cast<std::byte*>(self) + 0x14);
    callXferSlot(xfer, 0x64, static_cast<std::byte*>(self) + 0x18);
}

void ease56ResetTrack(void* self, std::uint32_t fill, std::uint32_t frames, std::uint32_t easeInBits,
                      std::uint32_t easeOutBits, std::uint32_t tag, std::uint32_t ignored) noexcept {
    (void)ignored;
    std::memset(static_cast<std::byte*>(self) + 0x1864, 0, 0x404);
    std::memset(static_cast<std::byte*>(self) + 0x1C74, 0, 0x3FC);
    const int count = loadAt<int>(self, 0x2070);
    if (count > 0) {
        for (int index = 0; index < count; ++index) {
            storeAt<std::uint32_t>(self, 0x1C74 + static_cast<std::size_t>(index) * 4, fill);
        }
    }
    const int storedFrames = static_cast<int>(frames) > 1 ? static_cast<int>(frames) : 1;
    storeAt<std::uint32_t>(self, 0x18, 0);
    storeAt<std::uint32_t>(self, 0x1C, 0);
    storeAt<std::uint32_t>(self, 4, static_cast<std::uint32_t>(storedFrames));
    storeAt<std::uint32_t>(self, 8, 0);
    storeAt<std::uint32_t>(self, 0x20, 0);
    storeAt<std::uint32_t>(self, 0x28, 1);
    configureAt(self, 0x10, easeInBits, easeOutBits, static_cast<int>(frames));
    storeAt<std::uint32_t>(self, 0x0C, tag);
    storeAt<std::uint8_t>(self, 0x24, 1);
}

}  // namespace ra3::render
