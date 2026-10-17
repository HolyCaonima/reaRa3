#include <ra3/gameclient/GameClient52.h>

#include <cstring>
#include <xmmintrin.h>

namespace ra3::gameclient {
namespace {

std::uint32_t load32(const void* base, std::uint32_t offset) noexcept
{
    std::uint32_t value = 0;
    std::memcpy(&value, static_cast<const std::uint8_t*>(base) + offset, 4);
    return value;
}

void store32(void* base, std::uint32_t offset, std::uint32_t value) noexcept
{
    std::memcpy(static_cast<std::uint8_t*>(base) + offset, &value, 4);
}

float loadFloat(const void* base, std::uint32_t offset) noexcept
{
    float value = 0.f;
    std::memcpy(&value, static_cast<const std::uint8_t*>(base) + offset, 4);
    return value;
}

void storeFloat(void* base, std::uint32_t offset, float value) noexcept
{
    std::memcpy(static_cast<std::uint8_t*>(base) + offset, &value, 4);
}

void storeSs(void* base, std::uint32_t offset, __m128 value) noexcept
{
    _mm_store_ss(reinterpret_cast<float*>(static_cast<std::uint8_t*>(base) + offset), value);
}

std::uint8_t* raw(void* base) noexcept
{
    return static_cast<std::uint8_t*>(base);
}

void setControlWord() noexcept
{
    unsigned short control = 0x037F;
    __asm { fldcw control }
}

const std::uint32_t kDeg2RadBits = 0x3C8EFA35u;
const std::uint32_t kHalfBits = 0x3F000000u;
const std::uint32_t kTwelveBits = 0x41400000u;
const std::uint32_t kHugeBits = 0x501502F9u;
const std::uint32_t kScaleDownBits = 0x3F75C28Fu;
const std::uint32_t kScaleUpBits = 0x3F866666u;
const std::uint32_t kOneBits = 0x3F800000u;
const std::uint32_t kNegOneBits = 0xBF800000u;
const std::uint32_t kAngleMinBits = 0xBF20D97Cu;
const std::uint32_t kAngleMaxBits = 0x3F20D97Cu;
const std::uint32_t kFifthBits = 0x3E4CCCCDu;
const std::uint32_t kDefaultPitchBits = 0x3F5F66F3u;

using VoidFn = void(__fastcall*)(void*, void*);
using SetFloatFn = void(__fastcall*)(void*, void*, float);
using SetPointFn = void(__fastcall*)(void*, void*, void*);
using GetFloatFn = float(__fastcall*)(void*, void*);

void* slot(void* self, std::uint32_t offset) noexcept
{
    auto** table = *reinterpret_cast<void***>(self);
    return table[offset / 4];
}

void scaleFov(void* self, const std::uint32_t* factor, bool add) noexcept
{
    setControlWord();
    const std::uint32_t* one = &kOneBits;
    void* stable = self;
    if (add) {
        __asm {
            mov esi, stable
            mov eax, dword ptr [esi]
            mov eax, dword ptr [eax + 0F4h]
            call eax
            mov eax, factor
            fmul dword ptr [eax]
            mov eax, one
            fadd dword ptr [eax]
            push ecx
            mov ecx, esi
            fstp dword ptr [esp]
            mov eax, dword ptr [esi]
            mov eax, dword ptr [eax + 0F8h]
            call eax
        }
    } else {
        __asm {
            mov esi, stable
            mov eax, dword ptr [esi]
            mov eax, dword ptr [eax + 0F4h]
            call eax
            mov eax, factor
            fmul dword ptr [eax]
            mov eax, one
            fsub dword ptr [eax]
            push ecx
            mov ecx, esi
            fstp dword ptr [esp]
            mov eax, dword ptr [esi]
            mov eax, dword ptr [eax + 0F8h]
            call eax
        }
    }
}

void storeTanDistance(float angle, bool degrees, std::uint8_t* encoded) noexcept
{
    setControlWord();
    std::uint32_t slotBits = 0;
    std::memcpy(&slotBits, &angle, 4);
    const std::uint32_t* deg = &kDeg2RadBits;
    const std::uint32_t* half = &kHalfBits;
    const std::uint32_t* twelve = &kTwelveBits;
    const std::uint32_t* huge = &kHugeBits;
    if (degrees) {
        __asm {
            lea eax, slotBits
            fld dword ptr [eax]
            mov eax, deg
            fmul dword ptr [eax]
            mov eax, half
            fmul dword ptr [eax]
            fptan
            fstp st(0)
            lea eax, slotBits
            fst dword ptr [eax]
            fldz
            fxch st(1)
            fucomip st(0), st(1)
            fstp st(0)
            lahf
            test ah, 44h
            jnp tan_huge_deg
            mov eax, twelve
            movss xmm0, dword ptr [eax]
            lea eax, slotBits
            divss xmm0, dword ptr [eax]
            movss dword ptr [eax], xmm0
            fld dword ptr [eax]
            jmp tan_store_deg
        tan_huge_deg:
            mov eax, huge
            movss xmm0, dword ptr [eax]
            lea eax, slotBits
            movss dword ptr [eax], xmm0
            fld dword ptr [eax]
        tan_store_deg:
            mov eax, encoded
            fstp tbyte ptr [eax]
        }
    } else {
        __asm {
            lea eax, slotBits
            fld dword ptr [eax]
            mov eax, half
            fmul dword ptr [eax]
            fptan
            fstp st(0)
            lea eax, slotBits
            fst dword ptr [eax]
            fldz
            fxch st(1)
            fucomip st(0), st(1)
            fstp st(0)
            lahf
            test ah, 44h
            jnp tan_huge_rad
            mov eax, twelve
            movss xmm0, dword ptr [eax]
            lea eax, slotBits
            divss xmm0, dword ptr [eax]
            movss dword ptr [eax], xmm0
            fld dword ptr [eax]
            jmp tan_store_rad
        tan_huge_rad:
            mov eax, huge
            movss xmm0, dword ptr [eax]
            lea eax, slotBits
            movss dword ptr [eax], xmm0
            fld dword ptr [eax]
        tan_store_rad:
            mov eax, encoded
            fstp tbyte ptr [eax]
        }
    }
}

}  // namespace

void __fastcall View_resetCameraDefaults(void* self, void*) noexcept
{
    store32(self, 0x20, 0);
    store32(self, 0x24, 0);
    store32(self, 0x0C, 0);
    store32(self, 0x10, 0);
    store32(self, 0x14, 0);
    store32(self, 0x3C, kFifthBits);
    store32(self, 0x28, 0);
    store32(self, 0x5C, 0);
    store32(self, 0x60, 0);
    store32(self, 0x38, kOneBits);
    store32(self, 0x40, kOneBits);
    raw(self)[0x7D] = 0;
    store32(self, 0x4C, 0);
    store32(self, 0x50, 0);
    store32(self, 0x64, 0);
    store32(self, 0x68, 0);
    store32(self, 0x6C, 0);
    store32(self, 0x70, 0);
    store32(self, 0x74, kDefaultPitchBits);
    store32(self, 0x78, 0);
    raw(self)[0x7C] = 0;
    raw(self)[0x7E] = 0;
    raw(self)[0x84] = 0;
    store32(self, 0x88, 0);
    store32(self, 0x8C, 0);
    store32(self, 0x90, 0);
    store32(self, 0x94, 0);
    store32(self, 0x98, 0);
    store32(self, 0x9C, kOneBits);
    store32(self, 0xA0, kOneBits);
    store32(self, 0xA4, kOneBits);
    store32(self, 0xA8, kOneBits);
    store32(self, 0xAC, 0);
    store32(self, 0xB0, 0);
}

void __fastcall View_initDefaultViewport(void* self, void*) noexcept
{
    store32(self, 0x18, 0x280);
    store32(self, 0x1C, 0x1E0);
    View_resetCameraDefaults(self, nullptr);
}

std::uint32_t __fastcall View_setLockByte(void* self, void*, std::uint32_t value) noexcept
{
    raw(self)[0x48] = static_cast<std::uint8_t>(value);
    return value & 0xFFu;
}

std::uint32_t __fastcall View_setViewportWidth(void* self, void*, std::uint32_t width) noexcept
{
    store32(self, 0x18, width);
    return width;
}

std::uint32_t __fastcall View_setViewportHeight(void* self, void*, std::uint32_t height) noexcept
{
    store32(self, 0x1C, height);
    return height;
}

std::uint32_t __fastcall View_setViewportOrigin(void* self, void*, std::uint32_t x, std::uint32_t y) noexcept
{
    store32(self, 0x20, x);
    store32(self, 0x24, y);
    return x;
}

std::uint32_t __fastcall View_getViewportOrigin(void* self, void*, std::uint32_t* x, std::uint32_t* y) noexcept
{
    const std::uint32_t first = load32(self, 0x20);
    const std::uint32_t second = load32(self, 0x24);
    *x = first;
    *y = second;
    return second;
}

std::uint32_t __fastcall View_setPositionFromScreen(void* self, void*, const void* point) noexcept
{
    const auto* bytes = static_cast<const std::uint8_t*>(point);
    const std::uint32_t zBits = load32(self, 0x14);
    const float half = loadFloat(&kHalfBits, 0);
    __m128 x = _mm_load_ss(reinterpret_cast<const float*>(bytes));
    __m128 width = _mm_cvtsi32_ss(_mm_setzero_ps(), static_cast<int>(load32(self, 0x18)));
    __m128 halfReg = _mm_load_ss(&half);
    width = _mm_mul_ss(width, halfReg);
    x = _mm_sub_ss(x, width);
    const int heightBits = static_cast<int>(load32(self, 0x1C));
    __m128 y = _mm_load_ss(reinterpret_cast<const float*>(bytes + 4));
    __m128 height = _mm_cvtsi32_ss(_mm_setzero_ps(), heightBits);
    height = _mm_mul_ss(height, halfReg);
    y = _mm_sub_ss(y, height);
    storeSs(self, 0x0C, x);
    storeSs(self, 0x10, y);
    store32(self, 0x14, zBits);
    return zBits;
}

std::uint32_t __fastcall View_addPositionXY(void* self, void*, const void* point) noexcept
{
    const auto* bytes = static_cast<const std::uint8_t*>(point);
    __m128 x = _mm_load_ss(reinterpret_cast<const float*>(bytes));
    __m128 px = _mm_load_ss(reinterpret_cast<const float*>(raw(self) + 0x0C));
    x = _mm_add_ss(x, px);
    storeSs(self, 0x0C, x);
    __m128 y = _mm_load_ss(reinterpret_cast<const float*>(bytes + 4));
    __m128 py = _mm_load_ss(reinterpret_cast<const float*>(raw(self) + 0x10));
    y = _mm_add_ss(y, py);
    storeSs(self, 0x10, y);
    return 0;
}

std::uint32_t __fastcall View_setPositionFromScreenIf(void* self, void*, const void* point,
    std::uint32_t, std::uint32_t, std::uint32_t, std::uint32_t, std::uint32_t) noexcept
{
    if (point == nullptr) {
        return 0;
    }
    using Fn = std::uint32_t(__fastcall*)(void*, void*, const void*);
    return reinterpret_cast<Fn>(slot(self, 0x48))(self, nullptr, point);
}

void __fastcall View_setAngleClamped(void* self, void*, float angle) noexcept
{
    const float low = loadFloat(&kAngleMinBits, 0);
    const float high = loadFloat(&kAngleMaxBits, 0);
    __m128 value = _mm_load_ss(&angle);
    __m128 bound = _mm_load_ss(&low);
    storeFloat(self, 0x2C, angle);
    if (_mm_comigt_ss(bound, value)) {
        storeFloat(self, 0x2C, low);
        return;
    }
    bound = _mm_load_ss(&high);
    if (_mm_comigt_ss(value, bound)) {
        storeFloat(self, 0x2C, high);
    }
}

float __fastcall View_getAngle(void* self, void*) noexcept
{
    return loadFloat(self, 0x2C);
}

float __fastcall View_getPitch(void* self, void*) noexcept
{
    return loadFloat(self, 0x30);
}

void __fastcall View_copyAnglePitchDefaults(void* self, void*) noexcept
{
    storeFloat(self, 0x28, loadFloat(self, 0x4C));
    storeFloat(self, 0x2C, loadFloat(self, 0x50));
}

std::uint8_t* __fastcall View_copyPosition(void* self, void*, std::uint8_t* dest) noexcept
{
    store32(dest, 0, load32(self, 0x0C));
    store32(dest, 4, load32(self, 0x10));
    store32(dest, 8, load32(self, 0x14));
    return dest;
}

float __fastcall View_getZoom(void* self, void*) noexcept
{
    return loadFloat(self, 0x40);
}

float __fastcall View_getFov(void* self, void*) noexcept
{
    return loadFloat(self, 0x44);
}

void __fastcall View_setFov(void* self, void*, float fov) noexcept
{
    storeFloat(self, 0x44, fov);
}

void __fastcall View_scaleFovDown(void* self, void*) noexcept
{
    scaleFov(self, &kScaleDownBits, false);
}

void __fastcall View_scaleFovUp(void* self, void*) noexcept
{
    scaleFov(self, &kScaleUpBits, true);
}

std::uint32_t __fastcall View_setByte7D(void* self, void*, std::uint32_t value) noexcept
{
    raw(self)[0x7D] = static_cast<std::uint8_t>(value);
    return value & 0xFFu;
}

float __fastcall View_getHeight(void* self, void*) noexcept
{
    return loadFloat(self, 0x58);
}

void __fastcall View_setHeight(void* self, void*, float height) noexcept
{
    storeFloat(self, 0x58, height);
}

float __fastcall View_getMaxHeight(void* self, void*) noexcept
{
    return loadFloat(self, 0x54);
}

void __fastcall View_setMaxHeight(void* self, void*, float height) noexcept
{
    storeFloat(self, 0x54, height);
}

void __fastcall View_setTerrainHeight(void* self, void*, float height) noexcept
{
    storeFloat(self, 0x74, height);
}

float __fastcall View_getTerrainHeight(void* self, void*) noexcept
{
    return loadFloat(self, 0x74);
}

std::uint8_t* __fastcall View_captureCamera(void* self, void*, std::uint8_t* dest) noexcept
{
    const std::uint32_t x = load32(self, 0x0C);
    const std::uint32_t y = load32(self, 0x10);
    const std::uint32_t z = load32(self, 0x14);
    const float angle = reinterpret_cast<GetFloatFn>(slot(self, 0xC4))(self, nullptr);
    const float pitch = reinterpret_cast<GetFloatFn>(slot(self, 0xCC))(self, nullptr);
    const float roll = reinterpret_cast<GetFloatFn>(slot(self, 0xD4))(self, nullptr);
    const float fov = reinterpret_cast<GetFloatFn>(slot(self, 0xF4))(self, nullptr);
    dest[0] = 1;
    store32(dest, 4, x);
    store32(dest, 8, y);
    store32(dest, 12, z);
    storeFloat(dest, 0x10, angle);
    storeFloat(dest, 0x14, pitch);
    storeFloat(dest, 0x18, roll);
    storeFloat(dest, 0x1C, fov);
    return dest;
}

void __fastcall View_applyCamera(void* self, void*, const std::uint8_t* state) noexcept
{
    if (state[0] == 0) {
        return;
    }
    reinterpret_cast<SetPointFn>(slot(self, 0x48))(self, nullptr, const_cast<std::uint8_t*>(state + 4));
    reinterpret_cast<SetFloatFn>(slot(self, 0xC0))(self, nullptr, loadFloat(state, 0x10));
    reinterpret_cast<SetFloatFn>(slot(self, 0xC8))(self, nullptr, loadFloat(state, 0x14));
    reinterpret_cast<SetFloatFn>(slot(self, 0xD0))(self, nullptr, loadFloat(state, 0x18));
    reinterpret_cast<SetFloatFn>(slot(self, 0xF8))(self, nullptr, loadFloat(state, 0x1C));
    reinterpret_cast<VoidFn>(slot(self, 0x40))(self, nullptr);
}

std::uint32_t __fastcall View_clearFilter(void* self, void*, std::uint32_t id) noexcept
{
    store32(self, 0x5C, id);
    store32(self, 0x68, 0);
    store32(self, 0x64, 0);
    return id;
}

void __fastcall View_setFilterReady(void* self, void*) noexcept
{
    raw(self)[0x7E] = 1;
}

std::uint32_t __fastcall View_setFilter(void* self, void*, std::uint32_t id, float value) noexcept
{
    store32(self, 0x64, id);
    storeFloat(self, 0x68, value);
    return id;
}

void __fastcall View_pushZoom(void* self, void*, float zoom) noexcept
{
    const float current = loadFloat(self, 0x6C);
    if (current == 0.f) {
        storeFloat(self, 0x70, loadFloat(self, 0x44));
    } else {
        storeFloat(self, 0x70, current);
    }
    storeFloat(self, 0x6C, zoom);
}

float __fastcall View_getZoomCurrent(void* self, void*) noexcept
{
    return loadFloat(self, 0x6C);
}

std::uint32_t __fastcall View_getZoomMode(void* self, void*) noexcept
{
    return load32(self, 0x60);
}

std::uint32_t __fastcall View_setZoomMode(void* self, void*, std::uint32_t mode) noexcept
{
    store32(self, 0x60, mode);
    return mode;
}

std::uint32_t __fastcall View_clearZoomFilter(void* self, void*) noexcept
{
    store32(self, 0x5C, 0);
    store32(self, 0x60, 0);
    store32(self, 0x6C, 0);
    store32(self, 0x70, 0);
    return 0;
}

std::uint32_t __fastcall View_setCameraByte(void* self, void*, std::uint32_t value) noexcept
{
    raw(self)[0x7C] = static_cast<std::uint8_t>(value);
    return value & 0xFFu;
}

float __fastcall View_getExtraPitch(void* self, void*) noexcept
{
    return loadFloat(self, 0x78);
}

void __fastcall View_setExtraPitch(void* self, void*, float pitch) noexcept
{
    storeFloat(self, 0x78, pitch);
}

std::uint32_t __fastcall View_getGuardFlag(void* self, void*) noexcept
{
    return raw(self)[0x84];
}

std::uint32_t __fastcall View_getGuardId(void* self, void*) noexcept
{
    return load32(self, 0x88);
}

std::uint32_t __fastcall View_setGuardId(void* self, void*, std::uint32_t id) noexcept
{
    store32(self, 0x88, id);
    return id;
}

std::uint32_t __fastcall View_setGuardPosition(void* self, void*, std::uint32_t x, std::uint32_t y) noexcept
{
    store32(self, 0x8C, x);
    store32(self, 0x90, y);
    return x;
}

std::uint32_t __fastcall View_clearGuardPosition(void* self, void*) noexcept
{
    store32(self, 0x8C, 0xFFFFFFFFu);
    store32(self, 0x90, 0xFFFFFFFFu);
    return 0xFFFFFFFFu;
}

std::uint32_t __fastcall View_hasGuardPosition(void* self, void*) noexcept
{
    if (load32(self, 0x8C) == 0xFFFFFFFFu || load32(self, 0x90) == 0xFFFFFFFFu) {
        return 0;
    }
    return 1;
}

std::uint8_t* __fastcall View_copyGuardPosition(void* self, void*, std::uint8_t* dest) noexcept
{
    store32(dest, 0, load32(self, 0x8C));
    store32(dest, 4, load32(self, 0x90));
    store32(dest, 8, load32(self, 0x94));
    return dest;
}

void __fastcall View_addShakeMagnitude(void* self, void*, float delta) noexcept
{
    const float neg = loadFloat(&kNegOneBits, 0);
    __m128 zero = _mm_setzero_ps();
    __m128 value = _mm_load_ss(&delta);
    __m128 current = _mm_load_ss(reinterpret_cast<const float*>(raw(self) + 0x98));
    if (_mm_comigt_ss(zero, value)) {
        __m128 sign = _mm_load_ss(&neg);
        value = _mm_mul_ss(value, sign);
        value = _mm_add_ss(value, current);
    } else {
        value = _mm_add_ss(current, value);
    }
    storeSs(self, 0x98, value);
}

std::uint32_t __fastcall View_clearShake(void* self, void*) noexcept
{
    store32(self, 0x98, 0);
    return 0;
}

float __fastcall View_getShake(void* self, void*) noexcept
{
    return loadFloat(self, 0x98);
}

void __fastcall View_setShakeRange(void* self, void*, float low, float high) noexcept
{
    storeFloat(self, 0xAC, low);
    storeFloat(self, 0xB0, high);
}

void __fastcall View_distanceFromFov(void*, void*, float angle, std::uint8_t st0[10]) noexcept
{
    storeTanDistance(angle, true, st0);
}

void __fastcall View_distanceFromHalfAngle(void*, void*, float angle, std::uint8_t st0[10]) noexcept
{
    storeTanDistance(angle, false, st0);
}

float __fastcall View_getNearClip(void* self, void*) noexcept
{
    return loadFloat(self, 0x9C);
}

float __fastcall View_getFarClip(void* self, void*) noexcept
{
    return loadFloat(self, 0xA4);
}

void __fastcall View_setNearClip(void* self, void*, float clip) noexcept
{
    storeFloat(self, 0x9C, clip);
}

void __fastcall View_setFarClip(void* self, void*, float clip) noexcept
{
    storeFloat(self, 0xA4, clip);
}

std::uint8_t* __fastcall View_setViewId(void* self, void*, std::uint32_t id) noexcept
{
    store32(self, 4, id);
    return raw(self);
}

}  // namespace ra3::gameclient
