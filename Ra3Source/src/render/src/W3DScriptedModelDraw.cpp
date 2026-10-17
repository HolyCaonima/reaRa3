#include <ra3/render/W3DScriptedModelDraw.h>

#include <ra3/render/W3DAnimation.h>
#include <ra3/render/W3DDrawable.h>
#include <ra3/render/W3DMaterial.h>

#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <xmmintrin.h>

#if defined(_WIN32)
#include <intrin.h>
#endif

namespace ra3::render {
namespace {

constexpr std::uintptr_t kSentinelParticleHandleVa = 0x00DF88F8U;
constexpr std::uint32_t kAnimEntryBaseVtableVa = 0x00C54284U;
constexpr std::uint32_t kAnimEntryVtableVa = 0x00C54BDCU;

template <class T>
T load(const void* object, std::ptrdiff_t offset = 0) noexcept
{
    T value{};
    std::memcpy(&value, static_cast<const std::byte*>(object) + offset, sizeof(value));
    return value;
}

template <class T>
void store(void* object, std::ptrdiff_t offset, T value) noexcept
{
    std::memcpy(static_cast<std::byte*>(object) + offset, &value, sizeof(value));
}

std::int32_t truncateFloatToInt(float value) noexcept
{
    return _mm_cvtt_ss2si(_mm_set_ss(value));
}

void copyFloatX87(void* destination, const void* source) noexcept
{
#if defined(_MSC_VER) && defined(_M_IX86)
    __asm {
        mov eax, source
        mov edx, destination
        fld dword ptr [eax]
        fstp dword ptr [edx]
    }
#else
    std::memcpy(destination, source, sizeof(float));
#endif
}

std::int32_t atomicFetchAddRef(void* refCountObject, std::int32_t delta) noexcept
{
    auto* refPtr = reinterpret_cast<long*>(static_cast<std::byte*>(refCountObject) + 4);
#if defined(_WIN32)
    return static_cast<std::int32_t>(_InterlockedExchangeAdd(refPtr, delta));
#else
    const auto previous = *refPtr;
    *refPtr += delta;
    return static_cast<std::int32_t>(previous);
#endif
}

void retainRefCountObject(void* refCountObject) noexcept
{
    if (refCountObject != nullptr) {
        atomicFetchAddRef(refCountObject, 1);
    }
}

void releaseRefCountObject(void* refCountObject) noexcept
{
    if (refCountObject == nullptr) {
        return;
    }
    if (atomicFetchAddRef(refCountObject, -1) == 1) {
        auto* refPtr = reinterpret_cast<long*>(static_cast<std::byte*>(refCountObject) + 4);
#if defined(_WIN32)
        _InterlockedExchange(refPtr, 1);
#else
        *refPtr = 1;
#endif
        const auto vtable = load<std::uintptr_t>(refCountObject, 0);
        const auto deletingDtor =
            load<void(__thiscall*)(void*, std::uint32_t)>(reinterpret_cast<const void*>(vtable), 0);
        deletingDtor(refCountObject, 1U);
    }
}

void assignRefCountSlot(void* slotAddress, void* newObject) noexcept
{
    auto* currentObject = load<void*>(slotAddress, 0);
    if (newObject == currentObject) {
        return;
    }
    retainRefCountObject(newObject);
    store(slotAddress, 0, newObject);
    releaseRefCountObject(currentObject);
}

void releaseParticleSystemRef(void* particleSystem) noexcept
{
    if (particleSystem == nullptr) {
        return;
    }
    const auto updated = load<std::int32_t>(particleSystem, 0x24) - 1;
    store<std::int32_t>(particleSystem, 0x24, updated);
    if (updated <= 0) {
        auto* subObj = static_cast<std::byte*>(particleSystem) + 0x20;
        const auto vtable = load<std::uintptr_t>(subObj, 0);
        const auto dtor =
            load<void(__thiscall*)(void*, std::uint32_t)>(reinterpret_cast<const void*>(vtable), 0);
        dtor(subObj, 1U);
    }
}

void releaseSimpleShadowObject(void* shadowObj) noexcept
{
    if (shadowObj == nullptr) {
        return;
    }
    const auto updated = load<std::int32_t>(shadowObj, 4) - 1;
    store<std::int32_t>(shadowObj, 4, updated);
    if (updated == 0) {
        const auto vtable = load<std::uintptr_t>(shadowObj, 0);
        const auto dtor = load<void(__thiscall*)(void*)>(reinterpret_cast<const void*>(vtable), 0);
        dtor(shadowObj);
    }
}

// Helper for 0x00417070 (zero-initialize 8-byte animation handle pair).
void initEmptyAsciiString(void* asciiString) noexcept
{
    store<std::uint32_t>(asciiString, 0, 0U);
    store<std::uint32_t>(asciiString, 4, 0U);
}

// Helper for 0x00AAFE10 (EASTL rbtree::find on integer key at node + 0x10).
void* rbTreeFindIntKey(void* tree, std::int32_t key) noexcept
{
    auto* sentinel = static_cast<std::byte*>(tree) + 4;
    auto* current = load<std::byte*>(tree, 0x0C);
    auto* candidate = sentinel;
    while (current != nullptr) {
        if (load<std::int32_t>(current, 0x10) >= key) {
            candidate = current;
            current = load<std::byte*>(current, 4);
        } else {
            current = load<std::byte*>(current, 0);
        }
    }
    if (candidate == sentinel || key < load<std::int32_t>(candidate, 0x10)) {
        return sentinel;
    }
    return candidate;
}

// Helper for 0x009C9980 (EASTL rbtree_increment).
void* rbTreeNextNode(void* node) noexcept
{
    auto* cur = static_cast<std::byte*>(node);
    auto* right = load<std::byte*>(cur, 0);
    if (right != nullptr) {
        auto* left = load<std::byte*>(right, 4);
        while (left != nullptr) {
            right = left;
            left = load<std::byte*>(right, 4);
        }
        return right;
    }
    auto* parent = load<std::byte*>(cur, 8);
    while (cur == load<std::byte*>(parent, 0)) {
        cur = parent;
        parent = load<std::byte*>(parent, 8);
    }
    if (load<std::byte*>(cur, 0) != parent) {
        return parent;
    }
    return cur;
}

// Helper for 0x00999850 (HLodClass::getSubObjectIndexByName).
std::int32_t findRenderObjSubObjectIndex(const void* compoundRenderObj, std::uint32_t nameKey) noexcept
{
    const auto* first = load<const std::byte*>(compoundRenderObj, 8);
    const auto* last = load<const std::byte*>(compoundRenderObj, 0x0C);
    for (const auto* cur = first; cur != last; cur += 0x14) {
        if (load<std::uint32_t>(cur, 0) == nameKey) {
            return static_cast<std::int32_t>((cur - first) / 20);
        }
    }
    return -1;
}

// Helper for 0x00522110 (Shadow::setColor).
void applyShadowColor(void* shadowDecal, std::uint32_t color) noexcept
{
    const auto mode = load<std::uint32_t>(shadowDecal, 0x34);
    const auto rgb = color & 0x00FFFFFFU;
    store<std::uint32_t>(shadowDecal, 0x24, rgb);
    if (mode == 2U || mode == 4U || mode == 6U) {
        const auto alpha = load<std::uint32_t>(shadowDecal, 0x2C);
        store<std::uint32_t>(shadowDecal, 0x28, (alpha << 24) | rgb);
        return;
    }
    if (mode != 3U && mode != 5U) {
        return;
    }
    float scale = 0.0F;
#if defined(_MSC_VER) && defined(_M_IX86)
    const std::uint32_t rawAlpha = load<std::uint32_t>(shadowDecal, 0x2C);
    const std::uint32_t twoPow32Bits = 0x4F800000U;
    const std::uint32_t inv255Bits = 0x3B808081U;
    __asm {
        mov edx, rawAlpha
        fild dword ptr [rawAlpha]
        test edx, edx
        jge short alpha_non_neg
        fadd dword ptr [twoPow32Bits]
    alpha_non_neg:
        fmul dword ptr [inv255Bits]
        fstp dword ptr [scale]
    }
#else
    const auto rawAlpha = load<std::uint32_t>(shadowDecal, 0x2C);
    scale = static_cast<float>(rawAlpha) * (1.0F / 255.0F);
#endif
    const __m128 scaleVec = _mm_set_ss(scale);
    const auto r = static_cast<std::int32_t>((color >> 16) & 0xFFU);
    const auto g = static_cast<std::int32_t>((color >> 8) & 0xFFU);
    const auto b = static_cast<std::int32_t>(color & 0xFFU);
    const auto rOut = _mm_cvtt_ss2si(_mm_mul_ss(_mm_cvtsi32_ss(_mm_setzero_ps(), r), scaleVec));
    const auto gOut = _mm_cvtt_ss2si(_mm_mul_ss(_mm_cvtsi32_ss(_mm_setzero_ps(), g), scaleVec));
    const auto bOut = _mm_cvtt_ss2si(_mm_mul_ss(_mm_cvtsi32_ss(_mm_setzero_ps(), b), scaleVec));
    const auto packed = static_cast<std::uint32_t>(((rOut << 8) | gOut) << 8 | bOut);
    store<std::uint32_t>(shadowDecal, 0x28, packed);
}

}  // namespace

// Original implementation; ra3_1.0.game 0x008E52A0.
void setDynamicsDrawWord2FC(void* draw, std::uint32_t value) noexcept
{
    store<std::uint32_t>(draw, 0x2FC, value);
}

// Original implementation; ra3_1.0.game 0x008E5690.
void releaseScriptedModelTerrainDecal(void* draw) noexcept
{
    if (auto* decal = load<void*>(draw, 0x74); decal != nullptr) {
        const auto vtable = load<std::uintptr_t>(decal, 0);
        const auto releaseFn = load<void(__thiscall*)(void*)>(reinterpret_cast<const void*>(vtable), 8);
        releaseFn(decal);
    }
    store<void*>(draw, 0x74, nullptr);
}

// Original implementation; ra3_1.0.game 0x008E57E0.
bool animTriggerCrossedFrame(const void* animState, std::int32_t triggerFrame) noexcept
{
    const auto curFrame = truncateFloatToInt(load<float>(animState, 4));
    const auto prevFrame = truncateFloatToInt(load<float>(animState, 8));
    if (prevFrame == curFrame) {
        return false;
    }
    if (curFrame == triggerFrame) {
        return true;
    }
    const auto modeMinusOne = load<std::uint32_t>(animState, 0x10) - 1U;
    if (modeMinusOne > 6U) {
        return false;
    }
    switch (modeMinusOne) {
    case 0U:  // Mode 1 (LOOP)
        if (prevFrame < curFrame) {
            return triggerFrame > prevFrame && triggerFrame < curFrame;
        }
        if (prevFrame <= curFrame) {
            return false;
        }
        return triggerFrame > prevFrame || triggerFrame < curFrame;
    case 1U:  // Mode 2 (ONCE)
    case 3U:  // Mode 4 (ONCE_BACKWARDS)
    case 5U:  // Mode 6 (MANUAL)
    case 6U:  // Mode 7
        if (load<std::int32_t>(animState, 0x14) == 1) {
            return triggerFrame > prevFrame && triggerFrame < curFrame;
        }
        return triggerFrame < prevFrame && triggerFrame > curFrame;
    case 2U:  // Mode 3 (LOOP_BACKWARDS)
        if (curFrame < prevFrame) {
            return triggerFrame < prevFrame && triggerFrame > curFrame;
        }
        if (curFrame <= prevFrame) {
            return false;
        }
        return triggerFrame < prevFrame || triggerFrame > curFrame;
    case 4U:  // Mode 5 (PING_PONG)
        if (load<std::int32_t>(animState, 0x14) == 1) {
            if (prevFrame < curFrame) {
                return triggerFrame > prevFrame && triggerFrame < curFrame;
            }
            if (prevFrame <= curFrame) {
                return false;
            }
            return triggerFrame < prevFrame;
        }
        if (curFrame < prevFrame) {
            return triggerFrame < prevFrame && triggerFrame > curFrame;
        }
        if (curFrame <= prevFrame) {
            return false;
        }
        return triggerFrame > prevFrame;
    default:
        return false;
    }
}

// Original implementation; ra3_1.0.game 0x008E5A60.
std::uint8_t notifySimpleDrawBoundToObject(void* draw, void* drawable) noexcept
{
    for (int i = 0; i < 4; ++i) {
        std::uint8_t flags[4] = {1U, 1U, 0U, 0U};
        const auto drawableVtable = load<std::uintptr_t>(drawable, 0);
        const auto onBoundFn =
            load<void(__thiscall*)(void*, void*)>(reinterpret_cast<const void*>(drawableVtable), 0x18);
        onBoundFn(drawable, flags);
    }

    const auto drawableVtable = load<std::uintptr_t>(drawable, 0);
    const auto subFn = load<std::uint8_t(__thiscall*)(void*, void*)>(
        reinterpret_cast<const void*>(drawableVtable), 0x84);
    return subFn(drawable, static_cast<std::byte*>(draw) + 0x10);
}

// Original implementation; ra3_1.0.game 0x008E5E80.
void* assignParticleSysRef(void* dstSlot, const void* srcSlot) noexcept
{
    if (dstSlot != srcSlot) {
        auto* newSys = load<void*>(srcSlot, 0);
        if (newSys != nullptr) {
            store<std::int32_t>(newSys, 0x24, load<std::int32_t>(newSys, 0x24) + 1);
        }
        releaseParticleSystemRef(load<void*>(dstSlot, 0));
        store<void*>(dstSlot, 0, load<void*>(srcSlot, 0));
    }
    return dstSlot;
}

// Original implementation; ra3_1.0.game 0x008E6060.
void* assignParticleAttachRecord8(void* dstRecord, const void* srcRecord) noexcept
{
    store<std::uint32_t>(dstRecord, 0, load<std::uint32_t>(srcRecord, 0));
    auto* dstSysSlot = static_cast<std::byte*>(dstRecord) + 4;
    const auto* srcSysSlot = static_cast<const std::byte*>(srcRecord) + 4;
    assignParticleSysRef(dstSysSlot, srcSysSlot);
    return dstRecord;
}

// Original implementation; ra3_1.0.game 0x008E6180.
void* copyParticleAttachRecords8Forward(const void* first, const void* last, void* dst) noexcept
{
    const auto* srcCur = static_cast<const std::byte*>(first);
    const auto* srcEnd = static_cast<const std::byte*>(last);
    auto* dstCur = static_cast<std::byte*>(dst);
    while (srcCur != srcEnd) {
        assignParticleAttachRecord8(dstCur, srcCur);
        srcCur += 8;
        dstCur += 8;
    }
    return dstCur;
}

// Original implementation; ra3_1.0.game 0x008E6260.
void* copyParticleAttachRecords8Backward(const void* first, const void* last, void* dstLast) noexcept
{
    const auto* srcFirst = static_cast<const std::byte*>(first);
    const auto* srcCur = static_cast<const std::byte*>(last);
    auto* dstCur = static_cast<std::byte*>(dstLast);
    while (srcCur != srcFirst) {
        srcCur -= 8;
        dstCur -= 8;
        assignParticleAttachRecord8(dstCur, srcCur);
    }
    return dstCur;
}

// Original implementation; ra3_1.0.game 0x008E8550.
void resetTruckExtraRotationIfNonZero(void* truckDraw) noexcept
{
    auto* vecPtr = static_cast<std::byte*>(truckDraw) + 0x2C8;
    const float x = load<float>(vecPtr, 0);
    const float y = load<float>(vecPtr, 4);
    const float z = load<float>(vecPtr, 8);
    if (!std::isnan(x) && x == 0.0F && !std::isnan(y) && y == 0.0F && !std::isnan(z) && z == 0.0F) {
        return;
    }
    auto* subInterface = static_cast<std::byte*>(truckDraw) + 0x290;
    const auto vtable = load<std::uintptr_t>(subInterface, 0);
    const auto applyFn =
        load<void(__thiscall*)(void*, void*)>(reinterpret_cast<const void*>(vtable), 0);
    applyFn(subInterface, vecPtr);
    store<float>(vecPtr, 0, 0.0F);
    store<float>(vecPtr, 4, 0.0F);
    store<float>(vecPtr, 8, 0.0F);
}

// Original implementation; ra3_1.0.game 0x008E8900.
void* getFirstAnimationName(const void* drawStateSlot, void* outAsciiString) noexcept
{
    const auto* state = load<const void*>(drawStateSlot, 0);
    if (load<std::uint32_t>(state, 0x68) == 0U) {
        initEmptyAsciiString(outAsciiString);
        return outAsciiString;
    }
    const auto* table = load<const void*>(state, 0x6C);
    const auto* animEntry = load<const void*>(table, 0);
    std::uint32_t temp[2]{};
    if (animEntry != nullptr) {
        temp[0] = load<std::uint32_t>(animEntry, 4);
        temp[1] = load<std::uint32_t>(animEntry, 8);
    } else {
        initEmptyAsciiString(temp);
    }
    store<std::uint32_t>(outAsciiString, 0, temp[0]);
    store<std::uint32_t>(outAsciiString, 4, temp[1]);
    return outAsciiString;
}

// Original implementation; ra3_1.0.game 0x008E8960.
void* getIndexedAnimationName(
    const void* drawStateSlot, void* outAsciiString, std::uint32_t index) noexcept
{
    const auto* state = load<const void*>(drawStateSlot, 0);
    const auto count = load<std::uint32_t>(state, 0x68);
    if (count == 0U) {
        initEmptyAsciiString(outAsciiString);
        return outAsciiString;
    }
    const auto rem = index % count;
    const auto* table = load<const std::byte*>(state, 0x6C);
    const auto* animEntry = load<const void*>(table, static_cast<std::ptrdiff_t>(rem * 8U));
    std::uint32_t temp[2]{};
    if (animEntry != nullptr) {
        temp[0] = load<std::uint32_t>(animEntry, 4);
        temp[1] = load<std::uint32_t>(animEntry, 8);
    } else {
        initEmptyAsciiString(temp);
    }
    store<std::uint32_t>(outAsciiString, 0, temp[0]);
    store<std::uint32_t>(outAsciiString, 4, temp[1]);
    return outAsciiString;
}

// Original implementation; ra3_1.0.game 0x008E8D50.
bool isAnimationLayerComplete(
    const void* drawStateSlot, const void* animLayer, std::uint8_t checkCompleteFlag) noexcept
{
    if (load<std::uint32_t>(drawStateSlot, 0) == 0U) {
        return true;
    }
    const auto* animObj = load<const void*>(animLayer, 0);
    if (animObj == nullptr) {
        return true;
    }
    const auto mode = load<std::uint32_t>(animLayer, 0x10);
    if (mode > 7U) {
        return true;
    }
    switch (mode) {
    case 0U:
    case 1U:
    case 3U:
    case 5U:
    case 7U:
        if (checkCompleteFlag == 0U) {
            return false;
        }
        return load<std::uint8_t>(animLayer, 0x18) != 0U;
    case 2U: {
        const auto totalFrames = static_cast<float>(load<std::int32_t>(animObj, 0x14)) - 1.0F;
        const auto curFrame = load<float>(animLayer, 4);
        return !std::isnan(curFrame) && !std::isnan(totalFrames) && curFrame >= totalFrames;
    }
    case 4U: {
        const auto curFrame = load<float>(animLayer, 4);
        return !std::isnan(curFrame) && 0.0F >= curFrame;
    }
    case 6U:
        return true;
    default:
        return true;
    }
}

// Original implementation; ra3_1.0.game 0x008E8DC0.
void* getScriptedModelModuleKeyOrOverride(const void* draw, std::uint32_t* outKey) noexcept
{
    const auto overrideKey = load<std::uint32_t>(draw, 0x25C);
    if (overrideKey != 0U) {
        *outKey = overrideKey;
        return outKey;
    }
    const auto* moduleData = load<const void*>(draw, 4);
    *outKey = load<std::uint32_t>(moduleData, 4);
    return outKey;
}

// Original implementation; ra3_1.0.game 0x008E8E90.
void updatePrimaryShadowTransform(
    void* draw, std::uint32_t firstArg, std::uint32_t secondArg) noexcept
{
    if (auto* shadow = load<void*>(draw, 0x1F0); shadow != nullptr) {
        const auto vtable1 = load<std::uintptr_t>(shadow, 0);
        const auto fn1 =
            load<void(__thiscall*)(void*, std::uint32_t)>(reinterpret_cast<const void*>(vtable1), 8);
        fn1(shadow, firstArg);

        auto* shadowAfter = load<void*>(draw, 0x1F0);
        const auto vtable2 = load<std::uintptr_t>(shadowAfter, 0);
        const auto fn2 =
            load<void(__thiscall*)(void*, std::uint32_t)>(reinterpret_cast<const void*>(vtable2), 0x0C);
        fn2(shadowAfter, secondArg);
    }
}

// Original implementation; ra3_1.0.game 0x008E8EC0.
void updatePrimaryShadowVisibility(void* draw, std::uint32_t visibleArg) noexcept
{
    if (auto* shadow = load<void*>(draw, 0x1F0); shadow != nullptr) {
        const auto vtable = load<std::uintptr_t>(shadow, 0);
        const auto fn =
            load<void(__thiscall*)(void*, std::uint32_t)>(reinterpret_cast<const void*>(vtable), 0x14);
        fn(shadow, visibleArg);
    }
}

// Original implementation; ra3_1.0.game 0x008E8FD0.
void releasePrimaryShadow(void* draw) noexcept
{
    if (auto* shadow = load<void*>(draw, 0x1F0); shadow != nullptr) {
        releaseSimpleShadowObject(shadow);
        store<void*>(draw, 0x1F0, nullptr);
    }
}

// Original implementation; ra3_1.0.game 0x008E9000.
void updateAuxShadowModule200(void* draw) noexcept
{
    if (auto* aux = load<void*>(draw, 0x200); aux != nullptr) {
        const auto vtable = load<std::uintptr_t>(aux, 0);
        const auto fn = load<void(__thiscall*)(void*)>(reinterpret_cast<const void*>(vtable), 4);
        fn(aux);
    }
}

// Original implementation; ra3_1.0.game 0x008E9020.
void updateSecondaryShadowTransform(
    void* draw, std::uint32_t firstArg, std::uint32_t secondArg) noexcept
{
    if (auto* shadow = load<void*>(draw, 0x1F4); shadow != nullptr) {
        const auto vtable1 = load<std::uintptr_t>(shadow, 0);
        const auto fn1 =
            load<void(__thiscall*)(void*, std::uint32_t)>(reinterpret_cast<const void*>(vtable1), 8);
        fn1(shadow, firstArg);

        auto* shadowAfter = load<void*>(draw, 0x1F4);
        const auto vtable2 = load<std::uintptr_t>(shadowAfter, 0);
        const auto fn2 =
            load<void(__thiscall*)(void*, std::uint32_t)>(reinterpret_cast<const void*>(vtable2), 0x0C);
        fn2(shadowAfter, secondArg);
    }
}

// Original implementation; ra3_1.0.game 0x008E9050.
void updateSecondaryShadowVisibility(void* draw, std::uint32_t visibleArg) noexcept
{
    if (auto* shadow = load<void*>(draw, 0x1F4); shadow != nullptr) {
        const auto vtable = load<std::uintptr_t>(shadow, 0);
        const auto fn =
            load<void(__thiscall*)(void*, std::uint32_t)>(reinterpret_cast<const void*>(vtable), 0x14);
        fn(shadow, visibleArg);
    }
}

// Original implementation; ra3_1.0.game 0x008E9240.
void releaseSecondaryShadow(void* draw) noexcept
{
    if (auto* shadow = load<void*>(draw, 0x1F4); shadow != nullptr) {
        releaseSimpleShadowObject(shadow);
        store<void*>(draw, 0x1F4, nullptr);
    }
}

// Original implementation; ra3_1.0.game 0x008E9270.
void releaseAuxShadowModule200(void* draw) noexcept
{
    if (auto* aux = load<void*>(draw, 0x200); aux != nullptr) {
        releaseSimpleShadowObject(aux);
        store<void*>(draw, 0x200, nullptr);
    }
}

// Original implementation; ra3_1.0.game 0x008E93B0.
void applyScriptedModelHouseColor(void* drawInterface, const PassMask* mask) noexcept
{
    const auto* moduleData = load<const void*>(drawInterface, -0x10);
    if (load<std::uint8_t>(moduleData, 0xC4) == 0U) {
        return;
    }
    auto* selfMask = reinterpret_cast<PassMask*>(static_cast<std::byte*>(drawInterface) + 0x30);
    if (equalPassMask(*selfMask, *mask)) {
        return;
    }
    assignPassMask(*selfMask, *mask);
    if (auto* renderObj = load<void*>(drawInterface, -8); renderObj != nullptr) {
        assignDrawablePassMask(renderObj, *selfMask);
    }
    auto* decal = load<void*>(drawInterface, 0x60);
    if (decal == nullptr) {
        return;
    }
    const auto* curState = load<const void*>(drawInterface, 0x28);
    if (curState == nullptr) {
        return;
    }
    const auto* stateDef = load<const void*>(curState, 0);
    const auto* decalCfg = load<const void*>(stateDef, 0xAC);
    if (decalCfg == nullptr || load<std::uint8_t>(decalCfg, 0x39) == 0U) {
        return;
    }
    const std::uint32_t color = (selfMask->countAndFlags & 7U) != 0U ? selfMask->words[0] : 0U;
    applyShadowColor(decal, color);
}

// Original implementation; ra3_1.0.game 0x008EB120.
void uninitializedCopyParticleAttachRecords8(
    void** outEnd, const void* first, const void* last, void* dst) noexcept
{
    const auto* srcCur = static_cast<const std::byte*>(first);
    const auto* srcEnd = static_cast<const std::byte*>(last);
    auto* dstCur = static_cast<std::byte*>(dst);
    *outEnd = dstCur;
    if (srcCur == srcEnd) {
        return;
    }
    while (srcCur != srcEnd) {
        if (dstCur != nullptr) {
            store<std::uint32_t>(dstCur, 0, load<std::uint32_t>(srcCur, 0));
            auto* sys = load<void*>(srcCur, 4);
            store<void*>(dstCur, 4, sys);
            if (sys != nullptr) {
                store<std::int32_t>(sys, 0x24, load<std::int32_t>(sys, 0x24) + 1);
            }
        }
        srcCur += 8;
        dstCur += 8;
    }
    *outEnd = dstCur;
}

// Original implementation; ra3_1.0.game 0x008EB360.
void uninitializedCopySubObjectRecords28(
    void** outEnd, const void* first, const void* last, void* dst) noexcept
{
    const auto* srcCur = static_cast<const std::byte*>(first);
    const auto* srcEnd = static_cast<const std::byte*>(last);
    auto* dstCur = static_cast<std::byte*>(dst);
    *outEnd = dstCur;
    if (srcCur == srcEnd) {
        return;
    }
    while (srcCur != srcEnd) {
        if (dstCur != nullptr) {
            std::memcpy(dstCur, srcCur, 0x1CU);
        }
        srcCur += 0x1C;
        dstCur += 0x1C;
    }
    *outEnd = dstCur;
}

// Original implementation; ra3_1.0.game 0x008EB490.
void** uninitializedCopySplineRecords80(
    void** outEnd, const void* first, const void* last, void* dst) noexcept
{
    const auto* srcCur = static_cast<const std::byte*>(first);
    const auto* srcEnd = static_cast<const std::byte*>(last);
    *outEnd = dst;
    while (srcCur != srcEnd) {
        if (auto* dstCur = static_cast<std::byte*>(*outEnd); dstCur != nullptr) {
            copyFloatWords18(dstCur, srcCur);
            copyFloatX87(dstCur + 0x48, srcCur + 0x48);
            copyFloatX87(dstCur + 0x4C, srcCur + 0x4C);
        }
        *outEnd = static_cast<std::byte*>(*outEnd) + 0x50;
        srcCur += 0x50;
    }
    return outEnd;
}

// Original implementation; ra3_1.0.game 0x008EB4E0.
void copyBoneTransformRecord56(void* dstRecord, const void* srcRecord) noexcept
{
    auto* dst = static_cast<std::byte*>(dstRecord);
    const auto* src = static_cast<const std::byte*>(srcRecord);
    store<std::uint32_t>(dst, 0, load<std::uint32_t>(src, 0));
    for (std::ptrdiff_t offset = 4; offset <= 0x30; offset += 4) {
        copyFloatX87(dst + offset, src + offset);
    }
    store<std::uint32_t>(dst, 0x34, load<std::uint32_t>(src, 0x34));
}

// Original implementation; ra3_1.0.game 0x008EB540.
void** uninitializedCopyRenderRecords68(
    void** outEnd, const void* first, const void* last, void* dst) noexcept
{
    const auto* srcCur = static_cast<const std::byte*>(first);
    const auto* srcEnd = static_cast<const std::byte*>(last);
    *outEnd = dst;
    while (srcCur != srcEnd) {
        if (auto* dstCur = static_cast<std::byte*>(*outEnd); dstCur != nullptr) {
            copyRenderRecord68(dstCur, srcCur);
        }
        *outEnd = static_cast<std::byte*>(*outEnd) + 0x44;
        srcCur += 0x44;
    }
    return outEnd;
}

// Original implementation; ra3_1.0.game 0x008EB5E0.
void* copyRenderStates204Backward(const void* first, const void* last, void* dstLast) noexcept
{
    const auto* srcFirst = static_cast<const std::byte*>(first);
    const auto* srcCur = static_cast<const std::byte*>(last);
    auto* dstCur = static_cast<std::byte*>(dstLast);
    while (srcCur != srcFirst) {
        srcCur -= 0xCC;
        dstCur -= 0xCC;
        copyRenderState201(dstCur, srcCur);
    }
    return dstCur;
}

// Original implementation; ra3_1.0.game 0x008EC560.
void retainSubRenderObjectAt4(const void* holder, void** outRenderObj) noexcept
{
    auto* obj = load<void*>(holder, 4);
    *outRenderObj = obj;
    retainRefCountObject(obj);
}

// Original implementation; ra3_1.0.game 0x008ED690.
void* findAnimationBoneInfoByKey(
    void* conditionState, std::int32_t key, std::uint32_t* outCount) noexcept
{
    if ((load<std::uint8_t>(conditionState, 0x84) & 1U) != 0U && key != 0) {
        auto* tree = static_cast<std::byte*>(conditionState) + 0x4C;
        auto* sentinel = static_cast<std::byte*>(conditionState) + 0x50;
        auto* node = static_cast<std::byte*>(rbTreeFindIntKey(tree, key));
        if (node != sentinel) {
            if (outCount != nullptr) {
                *outCount = load<std::uint32_t>(node, 0x44);
            }
            return node + 0x14;
        }
    }
    if (outCount != nullptr) {
        *outCount = 0U;
    }
    return nullptr;
}

// Original implementation; ra3_1.0.game 0x008EDA20.
void propagateSubObjectBoneVisibility(
    std::uint32_t hideFlag, std::int32_t parentBoneIndex, void* compoundRenderObj) noexcept
{
    const auto subObjStart = load<std::uintptr_t>(compoundRenderObj, 8);
    const auto subObjEnd = load<std::uintptr_t>(compoundRenderObj, 0x0C);
    const auto initialCount = static_cast<std::int32_t>(subObjEnd - subObjStart) / 20;
    if (initialCount > 0) {
        const float targetOpacity = (static_cast<std::uint8_t>(hideFlag) != 0U) ? 0.0F : 1.0F;
        auto* entryPtr = reinterpret_cast<std::byte*>(subObjStart) + 8;
        std::int32_t index = 0;
        do {
            auto boneIdx = load<std::int32_t>(entryPtr, -4);
            if (boneIdx > 0) {
                const auto boneArrayStart = load<std::uintptr_t>(compoundRenderObj, 0x7C);
                const auto boneArrayEnd = load<std::uintptr_t>(compoundRenderObj, 0x80);
                const auto numBones = static_cast<std::int32_t>(boneArrayEnd - boneArrayStart) / 72;
                while (boneIdx < numBones) {
                    const auto* boneEntry =
                        reinterpret_cast<const std::byte*>(boneArrayStart) + boneIdx * 72;
                    const auto parentPtr = load<std::uintptr_t>(boneEntry, 4);
                    if (parentPtr != 0U) {
                        boneIdx = static_cast<std::int32_t>(parentPtr - boneArrayStart) / 72;
                    } else {
                        boneIdx = -1;
                    }
                    if (boneIdx == parentBoneIndex) {
                        auto* subRenderObj = load<void*>(entryPtr, 0);
                        store<float>(subRenderObj, 0x50, targetOpacity);
                        break;
                    }
                    if (boneIdx <= 0) {
                        break;
                    }
                }
            }
            const auto curStart = load<std::uintptr_t>(compoundRenderObj, 8);
            const auto curEnd = load<std::uintptr_t>(compoundRenderObj, 0x0C);
            const auto curCount = static_cast<std::int32_t>(curEnd - curStart) / 20;
            ++index;
            entryPtr += 0x14;
            if (index >= curCount) {
                break;
            }
        } while (true);
    }
    releaseRefCountObject(compoundRenderObj);
}

// Original implementation; ra3_1.0.game 0x008EDB30.
void applyNamedSubObjectVisibility(
    const void* subObjEntry, void* compoundRenderObj, std::uint32_t hideFlag) noexcept
{
    if (compoundRenderObj == nullptr) {
        return;
    }
    const auto subObjStart = load<std::uintptr_t>(compoundRenderObj, 8);
    const auto subObjEnd = load<std::uintptr_t>(compoundRenderObj, 0x0C);
    const auto initialCount = static_cast<std::int32_t>(subObjEnd - subObjStart) / 20;
    if (initialCount > 0) {
        auto* basePtr = reinterpret_cast<std::byte*>(subObjStart);
        auto* boneSlot = basePtr + 4;
        std::int32_t index = 0;
        do {
            const auto targetBone = load<std::int32_t>(subObjEntry, 8);
            if (load<std::int32_t>(boneSlot, 0) == targetBone) {
                const float opacity = (static_cast<std::uint8_t>(hideFlag) != 0U) ? 0.0F : 1.0F;
                auto* subRenderObj = load<void*>(basePtr, index * 20 + 8);
                store<float>(subRenderObj, 0x50, opacity);
                retainRefCountObject(compoundRenderObj);
                propagateSubObjectBoneVisibility(
                    hideFlag, load<std::int32_t>(subObjEntry, 8), compoundRenderObj);
                releaseRefCountObject(compoundRenderObj);
                return;
            }
            const auto curEnd = load<std::uintptr_t>(compoundRenderObj, 0x0C);
            const auto curCount =
                static_cast<std::int32_t>(curEnd - reinterpret_cast<std::uintptr_t>(basePtr)) / 20;
            ++index;
            boneSlot += 0x14;
            if (index >= curCount) {
                break;
            }
        } while (true);
    }
    releaseRefCountObject(compoundRenderObj);
}

// Original implementation; ra3_1.0.game 0x008EE860.
void applySubObjectOpacityAndRelease(
    void* draw, void* subObjectRenderObj, float opacity) noexcept
{
    if (!std::isnan(opacity) && opacity <= 0.0F) {
        store<float>(subObjectRenderObj, 0x50, 0.0F);
        releaseRefCountObject(subObjectRenderObj);
        return;
    }
    if (!std::isnan(opacity) && opacity >= 1.0F) {
        store<float>(subObjectRenderObj, 0x50, 1.0F);
        releaseRefCountObject(subObjectRenderObj);
        return;
    }
    store<float>(subObjectRenderObj, 0x50, opacity);
    store<std::uint8_t>(draw, 0x1E9, 1U);
    releaseRefCountObject(subObjectRenderObj);
}

// Original implementation; ra3_1.0.game 0x008EE8E0.
void applySubObjectHideState(
    void* draw, const void* hideEntry, void* subObjectRenderObj, std::int32_t subObjIndex) noexcept
{
    const float opacity = (load<std::uint8_t>(hideEntry, 8) != 0U) ? 0.0F : 1.0F;
    auto* compound = load<void*>(draw, 0x0C);
    const auto* subObjArray = load<const std::byte*>(compound, 8);
    store<float>(subObjectRenderObj, 0x50, opacity);
    const auto boneIdx = load<std::int32_t>(subObjArray, subObjIndex * 20 + 4);
    if (boneIdx > 0) {
        const auto boneStart = load<std::uintptr_t>(compound, 0x7C);
        const auto boneEnd = load<std::uintptr_t>(compound, 0x80);
        const auto numBones = static_cast<std::int32_t>(boneEnd - boneStart) / 72;
        if (boneIdx < numBones) {
            auto* compoundAgain = load<void*>(draw, 0x0C);
            retainRefCountObject(compoundAgain);
            propagateSubObjectBoneVisibility(
                load<std::uint8_t>(hideEntry, 8), boneIdx, compoundAgain);
        }
    }
    releaseRefCountObject(subObjectRenderObj);
}

// Original implementation; ra3_1.0.game 0x008EE990.
void updateSubObjectFadeAndVisibility(
    void* draw, void* fadeEntry, void* subObjectRenderObj, std::int32_t subObjIndex) noexcept
{
    const float fadeStep = load<float>(fadeEntry, 0x0C);
    if (std::isnan(fadeStep) || fadeStep != 0.0F) {
        const auto* drawable = load<const void*>(draw, 8);
        if ((load<std::uint8_t>(drawable, 0x13C) & 0x20U) == 0U) {
            store<float>(fadeEntry, 0x14, 0.0F);
            store<float>(fadeEntry, 0x18, 0.0F);
            const float target = (!std::isnan(fadeStep) && fadeStep > 0.0F) ? 1.0F : 0.0F;
            store<float>(fadeEntry, 0x10, target);
            store<float>(fadeEntry, 0x0C, 0.0F);
            retainRefCountObject(subObjectRenderObj);
            applySubObjectOpacityAndRelease(draw, subObjectRenderObj, target);
        } else {
            const float delayStep = load<float>(fadeEntry, 0x14);
            const float delayRem = load<float>(fadeEntry, 0x18);
            if (!std::isnan(delayStep) && delayStep > 0.0F && !std::isnan(delayRem) && delayRem > 0.0F) {
                const float updatedRem = delayRem - delayStep;
                store<float>(fadeEntry, 0x18, (!std::isnan(updatedRem) && updatedRem > 0.0F) ? updatedRem : 0.0F);
                store<std::uint8_t>(draw, 0x1E9, 1U);
            } else {
                const float updatedOp = load<float>(fadeEntry, 0x10) + fadeStep;
                store<float>(fadeEntry, 0x10, updatedOp);
                if (std::isnan(updatedOp) || updatedOp > 1.0F) {
                    store<float>(fadeEntry, 0x10, 1.0F);
                } else if (0.0F > updatedOp) {
                    store<float>(fadeEntry, 0x10, 0.0F);
                }
                float passOp = 0.0F;
                copyFloatX87(&passOp, static_cast<const std::byte*>(fadeEntry) + 0x10);
                retainRefCountObject(subObjectRenderObj);
                applySubObjectOpacityAndRelease(draw, subObjectRenderObj, passOp);
            }
        }
    } else {
        const float opacity = (load<std::uint8_t>(fadeEntry, 8) == 0U) ? 1.0F : 0.0F;
        store<float>(subObjectRenderObj, 0x50, opacity);
    }

    auto* compound = load<void*>(draw, 0x0C);
    const auto* subObjArray = load<const std::byte*>(compound, 8);
    const auto boneIdx = load<std::int32_t>(subObjArray, subObjIndex * 20 + 4);
    if (boneIdx > 0) {
        const auto boneStart = load<std::uintptr_t>(compound, 0x7C);
        const auto boneEnd = load<std::uintptr_t>(compound, 0x80);
        const auto numBones = static_cast<std::int32_t>(boneEnd - boneStart) / 72;
        if (boneIdx < numBones) {
            retainRefCountObject(compound);
            propagateSubObjectBoneVisibility(
                load<std::uint8_t>(fadeEntry, 8), boneIdx, compound);
        }
    }
    releaseRefCountObject(subObjectRenderObj);
}

// Original implementation; ra3_1.0.game 0x008EEED0.
void resetAnimationLayerSlot(void* draw, std::int32_t layerIndex) noexcept
{
    auto* base = static_cast<std::byte*>(draw);
    auto* slot = base + layerIndex * 28;
    if (auto* animObj = load<void*>(slot, 0x11C); animObj != nullptr) {
        releaseRefCountObject(animObj);
        store<void*>(slot, 0x11C, nullptr);
    }
    store<std::uint8_t>(base, (layerIndex + 11) * 28, 0U);
    store<std::uint8_t>(slot, 0x135, 0U);
    store<std::uint32_t>(slot, 0x12C, 0U);
    store<float>(slot, 0x128, 0.0F);
    store<float>(slot, 0x120, 0.0F);
    store<float>(slot, 0x124, 0.0F);
    store<std::uint32_t>(slot, 0x130, 1U);
}

// Original implementation; ra3_1.0.game 0x008EF1A0.
void* initializeHermiteSplineSegment80(
    void* segment, const float* startPoint, const float* endPoint,
    float durationDivisor, float midpointZScale) noexcept
{
    store<float>(segment, 0x4C, 0.0F);
    store<std::uint32_t>(segment, 0x00, load<std::uint32_t>(startPoint, 0));
    store<std::uint32_t>(segment, 0x04, load<std::uint32_t>(startPoint, 4));
    store<std::uint32_t>(segment, 0x08, load<std::uint32_t>(startPoint, 8));
    store<std::uint32_t>(segment, 0x18, load<std::uint32_t>(endPoint, 0));
    store<std::uint32_t>(segment, 0x1C, load<std::uint32_t>(endPoint, 4));
    store<std::uint32_t>(segment, 0x20, load<std::uint32_t>(endPoint, 8));

#if defined(_MSC_VER) && defined(_M_IX86)
    const float sx = startPoint[0];
    const float sy = startPoint[1];
    const float sz = startPoint[2];
    const float ex = endPoint[0];
    const float ey = endPoint[1];
    const float ez = endPoint[2];
    float deltaX = 0.0F;
    float deltaY = 0.0F;
    float deltaZ = 0.0F;
    float mx = 0.0F;
    float my = 0.0F;
    float mz = 0.0F;
    _mm_store_ss(&deltaX, _mm_sub_ss(_mm_set_ss(ex), _mm_set_ss(sx)));
    _mm_store_ss(&deltaZ, _mm_sub_ss(_mm_set_ss(ez), _mm_set_ss(sz)));
    _mm_store_ss(&deltaY, _mm_sub_ss(_mm_set_ss(ey), _mm_set_ss(sy)));
    const __m128 half = _mm_set_ss(0.5F);
    _mm_store_ss(&mx, _mm_add_ss(_mm_set_ss(sx), _mm_mul_ss(_mm_set_ss(deltaX), half)));
    _mm_store_ss(&my, _mm_add_ss(_mm_set_ss(sy), _mm_mul_ss(_mm_set_ss(deltaY), half)));
    _mm_store_ss(&mz, _mm_add_ss(_mm_set_ss(sz), _mm_mul_ss(_mm_set_ss(deltaZ), half)));
    store<float>(segment, 0x0C, mx);
    store<float>(segment, 0x10, my);
    store<float>(segment, 0x14, mz);
    auto* segBytes = static_cast<std::byte*>(segment);
    __asm {
        mov ecx, segBytes
        fld dword ptr [deltaX]
        fmul st(0), st(0)
        fld dword ptr [deltaZ]
        fmul st(0), st(0)
        faddp st(1), st(0)
        fld dword ptr [deltaY]
        fmul st(0), st(0)
        faddp st(1), st(0)
        fsqrt
        fld dword ptr [midpointZScale]
        fmul st(0), st(1)
        fadd dword ptr [ecx + 0x14]
        fstp dword ptr [ecx + 0x14]
    }
    buildHermiteTangents(reinterpret_cast<float*>(segment));
    __asm {
        mov ecx, segBytes
        fdiv dword ptr [durationDivisor]
        fstp dword ptr [ecx + 0x48]
    }
#else
    const float dx = endPoint[0] - startPoint[0];
    const float dy = endPoint[1] - startPoint[1];
    const float dz = endPoint[2] - startPoint[2];
    const float dist = std::sqrt(dx * dx + dz * dz + dy * dy);
    store<float>(segment, 0x0C, startPoint[0] + dx * 0.5F);
    store<float>(segment, 0x10, startPoint[1] + dy * 0.5F);
    store<float>(segment, 0x14, startPoint[2] + dz * 0.5F + midpointZScale * dist);
    buildHermiteTangents(reinterpret_cast<float*>(segment));
    store<float>(segment, 0x48, dist / durationDivisor);
#endif
    return segment;
}

// Original implementation; ra3_1.0.game 0x008F0A90.
void destroyParticleAttachRecords8(void* first, void* last) noexcept
{
    auto* cur = static_cast<std::byte*>(first);
    const auto* end = static_cast<const std::byte*>(last);
    while (cur < end) {
        releaseParticleSystemRef(load<void*>(cur, 4));
        cur += 8;
    }
}

// Original implementation; ra3_1.0.game 0x008F0B30.
void* constructBoneTransformRecord56(
    void* dstRecord, const std::uint32_t* keyPtr, const void* matrixAndFlags) noexcept
{
    auto* dst = static_cast<std::byte*>(dstRecord);
    const auto* src = static_cast<const std::byte*>(matrixAndFlags);
    store<std::uint32_t>(dst, 0, *keyPtr);
    for (std::ptrdiff_t offset = 0; offset <= 0x2C; offset += 4) {
        copyFloatX87(dst + offset + 4, src + offset);
    }
    store<std::uint32_t>(dst, 0x34, load<std::uint32_t>(src, 0x30));
    return dstRecord;
}

// Original implementation; ra3_1.0.game 0x008F0B90.
void** rbTreeEqualRangeIntKey(void* tree, void** outPair, const std::int32_t* keyPtr) noexcept
{
    auto* root = load<std::byte*>(tree, 0x0C);
    auto* sentinel = static_cast<std::byte*>(tree) + 4;
    auto* upper = sentinel;
    auto* lower = sentinel;
    const auto key = *keyPtr;

    for (auto* cur = root; cur != nullptr;) {
        if (key < load<std::int32_t>(cur, 0x10)) {
            upper = cur;
            cur = load<std::byte*>(cur, 4);
        } else {
            cur = load<std::byte*>(cur, 0);
        }
    }
    for (auto* cur = root; cur != nullptr;) {
        if (load<std::int32_t>(cur, 0x10) >= key) {
            lower = cur;
            cur = load<std::byte*>(cur, 4);
        } else {
            cur = load<std::byte*>(cur, 0);
        }
    }
    outPair[0] = lower;
    outPair[1] = upper;
    return outPair;
}

// Original implementation; ra3_1.0.game 0x008F0DF0.
void uninitializedCopyAnimHandleRecords12(
    void** outEnd, const void* first, const void* last, void* dst) noexcept
{
    const auto* srcCur = static_cast<const std::byte*>(first);
    const auto* srcEnd = static_cast<const std::byte*>(last);
    *outEnd = dst;
    while (srcCur != srcEnd) {
        if (auto* dstCur = static_cast<std::byte*>(*outEnd); dstCur != nullptr) {
            store<std::uint32_t>(dstCur, 0, load<std::uint32_t>(srcCur, 0));
            auto* refObj = load<void*>(srcCur, 4);
            store<void*>(dstCur, 4, refObj);
            retainRefCountObject(refObj);
            copyFloatX87(dstCur + 8, srcCur + 8);
        }
        *outEnd = static_cast<std::byte*>(*outEnd) + 0x0C;
        srcCur += 0x0C;
    }
}

// Original implementation; ra3_1.0.game 0x008F0F30.
void uninitializedFillRenderRecords68(
    void* dst, std::uint32_t count, const void* value) noexcept
{
    auto* dstCur = static_cast<std::byte*>(dst);
    for (std::uint32_t rem = count; rem > 0U; --rem) {
        if (dstCur != nullptr) {
            copyRenderRecord68(dstCur, value);
        }
        dstCur += 0x44;
    }
}

// Original implementation; ra3_1.0.game 0x008F1010.
void uninitializedFillRefCountPairRecords8(
    void* dst, std::uint32_t count, const void* value) noexcept
{
    auto* dstCur = static_cast<std::byte*>(dst);
    for (std::uint32_t rem = count; rem > 0U; --rem) {
        if (dstCur != nullptr) {
            auto* firstObj = load<void*>(value, 0);
            store<void*>(dstCur, 0, firstObj);
            retainRefCountObject(firstObj);

            auto* secondObj = load<void*>(value, 4);
            store<void*>(dstCur, 4, secondObj);
            retainRefCountObject(secondObj);
        }
        dstCur += 8;
    }
}

// Original implementation; ra3_1.0.game 0x008F1120.
void fillRenderRecords68Range(void* first, void* last, const void* value) noexcept
{
    auto* cur = static_cast<std::byte*>(first);
    const auto* end = static_cast<const std::byte*>(last);
    const auto* src = static_cast<const std::byte*>(value);
    while (cur != end) {
        store<std::uint8_t>(cur, 0, load<std::uint8_t>(src, 0));
        store<std::uint32_t>(cur, 4, load<std::uint32_t>(src, 4));
        store<std::uint32_t>(cur, 8, load<std::uint32_t>(src, 8));
        store<std::uint32_t>(cur, 0x0C, load<std::uint32_t>(src, 0x0C));
        store<std::uint32_t>(cur, 0x10, load<std::uint32_t>(src, 0x10));
        for (std::ptrdiff_t offset = 0x14; offset <= 0x40; offset += 4) {
            copyFloatX87(cur + offset, src + offset);
        }
        cur += 0x44;
    }
}

// Original implementation; ra3_1.0.game 0x008F2700.
void stopAttachedParticleSystemsInVector(const void* pairVector) noexcept
{
    const auto* cur = load<const std::byte*>(pairVector, 0);
    const auto* end = load<const std::byte*>(pairVector, 4);
    while (cur != end) {
        constexpr std::ptrdiff_t kSlots[] = {0, 4};
        for (const std::ptrdiff_t slotOffset : kSlots) {
            const auto handleAddr = load<std::uintptr_t>(cur, slotOffset);
            if (handleAddr != kSentinelParticleHandleVa) {
                auto* sys = load<void*>(reinterpret_cast<const void*>(handleAddr), 0);
                if (sys != nullptr) {
                    store<std::uint8_t>(sys, 0x40, static_cast<std::uint8_t>(load<std::uint8_t>(sys, 0x40) | 0x10U));
                }
            }
        }
        cur += 8;
    }
}

// Original implementation; ra3_1.0.game 0x008F2C10.
void updateAttachedParticleSystemsVisibility(void* draw) noexcept
{
    const auto* drawable = load<const void*>(draw, 8);
    const bool drawableHidden =
        (load<std::uint8_t>(drawable, 0x13C) & 6U) != 0U &&
        load<std::uint32_t>(drawable, 0x1C8) == 0U;
    const bool shouldStop = drawableHidden || load<std::uint8_t>(draw, 0x69) != 0U;

    const auto* head = static_cast<const std::byte*>(draw) + 0x7C;
    for (const auto* node = load<const std::byte*>(head, 0); node != head;
         node = load<const std::byte*>(node, 0)) {
        const auto handleAddr = load<std::uintptr_t>(node, 0x0C);
        if (handleAddr != kSentinelParticleHandleVa) {
            auto* sys = load<void*>(reinterpret_cast<const void*>(handleAddr), 0);
            if (sys != nullptr) {
                const auto curFlags = load<std::uint8_t>(sys, 0x40);
                store<std::uint8_t>(
                    sys, 0x40,
                    shouldStop ? static_cast<std::uint8_t>(curFlags | 0x10U)
                               : static_cast<std::uint8_t>(curFlags & 0xEFU));
            }
        }
    }
}

// Original implementation; ra3_1.0.game 0x008F3D60.
void updateScriptedModelSubObjects(void* drawInterface) noexcept
{
    store<std::uint8_t>(drawInterface, 0x1D4, 0U);
    store<std::uint8_t>(drawInterface, 0x1D5, 0U);
    if (load<void*>(drawInterface, -8) == nullptr) {
        return;
    }
    auto* drawRoot = static_cast<std::byte*>(drawInterface) - 0x14;
    for (auto* cur = load<std::byte*>(drawInterface, 0x74);
         cur != load<std::byte*>(drawInterface, 0x78); cur += 0x1C) {
        auto* compound = load<void*>(drawInterface, -8);
        const auto subIdx = findRenderObjSubObjectIndex(compound, load<std::uint32_t>(cur, 4));
        if (subIdx >= 0) {
            auto* compoundAfter = load<void*>(drawInterface, -8);
            const auto* subObjArr = load<const std::byte*>(compoundAfter, 8);
            auto* subObj = load<void*>(subObjArr, subIdx * 20 + 8);
            retainRefCountObject(subObj);
            applySubObjectHideState(drawRoot, cur, subObj, subIdx);
        }
    }
    for (auto* cur = load<std::byte*>(drawInterface, 0x84);
         cur != load<std::byte*>(drawInterface, 0x88); cur += 0x1C) {
        auto* compound = load<void*>(drawInterface, -8);
        const auto subIdx = findRenderObjSubObjectIndex(compound, load<std::uint32_t>(cur, 4));
        if (subIdx >= 0) {
            auto* compoundAfter = load<void*>(drawInterface, -8);
            const auto* subObjArr = load<const std::byte*>(compoundAfter, 8);
            auto* subObj = load<void*>(subObjArr, subIdx * 20 + 8);
            retainRefCountObject(subObj);
            updateSubObjectFadeAndVisibility(drawRoot, cur, subObj, subIdx);
        }
    }
}

// Original implementation; ra3_1.0.game 0x008F3EA0.
void assignAnimationLayerSlot(
    void* draw, std::int32_t layerIndex, void* animObj, std::uint8_t loopFlag,
    std::uint32_t animMode, float frameStep, float currentFrame, float blendFactor,
    std::uint8_t activeFlag, std::uint32_t direction) noexcept
{
    auto* base = static_cast<std::byte*>(draw);
    auto* slot = base + layerIndex * 28;
    auto* animSlot = slot + 0x11C;
    if (auto* existingAnim = load<void*>(animSlot, 0); existingAnim != nullptr) {
        releaseRefCountObject(existingAnim);
        store<void*>(animSlot, 0, nullptr);
    }
    if (animObj != nullptr) {
        assignRefCountSlot(animSlot, animObj);
        store<std::uint8_t>(base, (layerIndex + 11) * 28, activeFlag);
        store<std::uint32_t>(slot, 0x12C, animMode);
        store<float>(slot, 0x128, frameStep);
        store<float>(slot, 0x120, currentFrame);
        store<float>(slot, 0x130, 0.0F);
        store<std::uint32_t>(slot, 0x130, direction);
        store<std::uint8_t>(slot, 0x135, loopFlag);
        store<float>(slot, 0x124, blendFactor);
        releaseRefCountObject(animObj);
    }
}

// Original implementation; ra3_1.0.game 0x008F3FA0.
void moveAnimationLayerSlot(
    void* draw, std::int32_t dstLayerIndex, std::int32_t srcLayerIndex) noexcept
{
    auto* base = static_cast<std::byte*>(draw);
    auto* srcSlot = base + srcLayerIndex * 28;
    const auto direction = load<std::uint32_t>(srcSlot, 0x130);
    float blendFactor = 0.0F;
    copyFloatX87(&blendFactor, srcSlot + 0x124);
    const auto activeFlag = load<std::uint8_t>(base, (srcLayerIndex + 11) * 28);
    const auto animMode = load<std::uint32_t>(srcSlot, 0x12C);
    const auto loopFlag = load<std::uint8_t>(srcSlot, 0x135);
    float currentFrame = 0.0F;
    copyFloatX87(&currentFrame, srcSlot + 0x120);
    float frameStep = 0.0F;
    copyFloatX87(&frameStep, srcSlot + 0x128);
    auto* animObj = load<void*>(srcSlot, 0x11C);
    retainRefCountObject(animObj);

    assignAnimationLayerSlot(
        draw, dstLayerIndex, animObj, loopFlag, animMode, frameStep, currentFrame, blendFactor,
        activeFlag, direction);
    resetAnimationLayerSlot(draw, srcLayerIndex);
}

// Original implementation; ra3_1.0.game 0x008F5CB0.
std::uint32_t rbTreeCountIntKey(void* tree, const std::int32_t* keyPtr) noexcept
{
    void* range[2]{};
    rbTreeEqualRangeIntKey(tree, range, keyPtr);
    std::uint32_t count = 0U;
    for (auto* cur = range[0]; cur != range[1]; cur = rbTreeNextNode(cur)) {
        ++count;
    }
    return count;
}

// Original implementation; ra3_1.0.game 0x008F6180.
void* copySubObjectRecords28Forward(const void* first, const void* last, void* dst) noexcept
{
    const auto* srcCur = static_cast<const std::byte*>(first);
    const auto* srcEnd = static_cast<const std::byte*>(last);
    auto* dstCur = static_cast<std::byte*>(dst);
    while (srcCur != srcEnd) {
        std::memcpy(dstCur, srcCur, 0x1CU);
        srcCur += 0x1C;
        dstCur += 0x1C;
    }
    return dstCur;
}

// Original implementation; ra3_1.0.game 0x008F61B0.
void destroyAnimHandleRecords12(void* first, void* last) noexcept
{
    auto* cur = static_cast<std::byte*>(first);
    const auto* end = static_cast<const std::byte*>(last);
    while (cur < end) {
        releaseRefCountObject(load<void*>(cur, 4));
        cur += 0x0C;
    }
}

// Original implementation; ra3_1.0.game 0x008F6690.
void* copySubObjectRecords28Backward(
    const void* first, const void* last, void* dstLast) noexcept
{
    const auto* srcFirst = static_cast<const std::byte*>(first);
    const auto* srcCur = static_cast<const std::byte*>(last);
    auto* dstCur = static_cast<std::byte*>(dstLast);
    while (srcCur != srcFirst) {
        srcCur -= 0x1C;
        dstCur -= 0x1C;
        std::memcpy(dstCur, srcCur, 0x1CU);
    }
    return dstCur;
}

// Original implementation; ra3_1.0.game 0x008F6900.
void* copyAnimHandleRecords12Forward(const void* first, const void* last, void* dst) noexcept
{
    const auto* srcCur = static_cast<const std::byte*>(first);
    const auto* srcEnd = static_cast<const std::byte*>(last);
    auto* dstCur = static_cast<std::byte*>(dst);
    while (srcCur != srcEnd) {
        if (dstCur != srcCur) {
            store<std::uint32_t>(dstCur, 0, load<std::uint32_t>(srcCur, 0));
            assignRefCountSlot(dstCur + 4, load<void*>(srcCur, 4));
            copyFloatX87(dstCur + 8, srcCur + 8);
        }
        srcCur += 0x0C;
        dstCur += 0x0C;
    }
    return dstCur;
}

// Original implementation; ra3_1.0.game 0x008F6C00.
void* copyAnimHandleRecords12Backward(
    const void* first, const void* last, void* dstLast) noexcept
{
    const auto* srcFirst = static_cast<const std::byte*>(first);
    const auto* srcCur = static_cast<const std::byte*>(last);
    auto* dstCur = static_cast<std::byte*>(dstLast);
    while (srcCur != srcFirst) {
        srcCur -= 0x0C;
        dstCur -= 0x0C;
        if (dstCur != srcCur) {
            store<std::uint32_t>(dstCur, 0, load<std::uint32_t>(srcCur, 0));
            assignRefCountSlot(dstCur + 4, load<void*>(srcCur, 4));
            copyFloatX87(dstCur + 8, srcCur + 8);
        }
    }
    return dstCur;
}

// Original implementation; ra3_1.0.game 0x008F6ED0.
std::uint32_t getSubObjectBoneCountForState(
    void* drawInterface, std::int32_t innerKey, std::int32_t outerKey) noexcept
{
    auto* state = (load<std::uint8_t>(drawInterface, 0x1D7) != 0U)
                      ? load<std::byte*>(drawInterface, 0x1D8)
                      : load<std::byte*>(drawInterface, 0x2C);
    if (state == nullptr) {
        return 0U;
    }
    auto* outerTree = state + 0x68;
    auto* outerSentinel = state + 0x6C;
    auto* node = static_cast<std::byte*>(rbTreeFindIntKey(outerTree, outerKey));
    if (node == outerSentinel || (load<std::uint8_t>(state, 0x84) & 8U) == 0U) {
        return 0U;
    }
    return rbTreeCountIntKey(node + 0x14, &innerKey);
}

// Original implementation; ra3_1.0.game 0x008F95D0.
void* eraseSubObjectRecords28Range(
    void* vectorThis, void* eraseFirst, void* eraseLast) noexcept
{
    auto* endPtr = load<std::byte*>(vectorThis, 4);
    auto* srcCur = static_cast<std::byte*>(eraseLast);
    auto* dstCur = static_cast<std::byte*>(eraseFirst);
    while (srcCur != endPtr) {
        std::memcpy(dstCur, srcCur, 0x1CU);
        srcCur += 0x1C;
        dstCur += 0x1C;
    }
    const auto byteDiff =
        static_cast<std::intptr_t>(static_cast<std::byte*>(eraseLast) - static_cast<std::byte*>(eraseFirst));
    const auto elemDiff = byteDiff / 28;
    store<std::byte*>(vectorThis, 4, load<std::byte*>(vectorThis, 4) - elemDiff * 28);
    return eraseFirst;
}

// Original implementation; ra3_1.0.game 0x008FA6D0.
void fillRefCountPairRecords8Range(void* first, void* last, const void* value) noexcept
{
    auto* cur = static_cast<std::byte*>(first);
    const auto* end = static_cast<const std::byte*>(last);
    const auto* valBytes = static_cast<const std::byte*>(value);
    while (cur != end) {
        if (valBytes != cur) {
            assignRefCountSlot(cur, load<void*>(valBytes, 0));
        }
        if (valBytes + 4 != cur + 4) {
            assignRefCountSlot(cur + 4, load<void*>(valBytes, 4));
        }
        cur += 8;
    }
}

// Original implementation; ra3_1.0.game 0x008FA790.
void* copyRefCountPairRecords8Forward(const void* first, const void* last, void* dst) noexcept
{
    const auto* srcCur = static_cast<const std::byte*>(first);
    const auto* srcEnd = static_cast<const std::byte*>(last);
    auto* dstCur = static_cast<std::byte*>(dst);
    while (srcCur != srcEnd) {
        if (srcCur != dstCur) {
            assignRefCountSlot(dstCur, load<void*>(srcCur, 0));
        }
        if (srcCur + 4 != dstCur + 4) {
            assignRefCountSlot(dstCur + 4, load<void*>(srcCur, 4));
        }
        srcCur += 8;
        dstCur += 8;
    }
    return dstCur;
}

// Original implementation; ra3_1.0.game 0x008FAB20.
void* initializeModelConditionAnimationEntry(void* entry) noexcept
{
    store<std::uint32_t>(entry, 0x00, kAnimEntryBaseVtableVa);
    store<std::uint32_t>(entry, 0x04, 0U);
    store<std::uint32_t>(entry, 0x00, kAnimEntryVtableVa);
    store<std::uint32_t>(entry, 0x08, 0U);
    store<std::uint32_t>(entry, 0x0C, 0U);
    store<std::uint32_t>(entry, 0x10, 0U);
    store<std::uint32_t>(entry, 0x14, 0U);
    store<std::uint32_t>(entry, 0x18, 0U);
    store<std::uint32_t>(entry, 0x20, 0U);
    store<std::uint32_t>(entry, 0x24, 0U);
    store<std::uint32_t>(entry, 0x28, 0U);
    store<std::uint32_t>(entry, 0x30, 0U);
    store<std::uint32_t>(entry, 0x34, 0U);
    store<std::uint32_t>(entry, 0x38, 0U);
    return entry;
}

// Original implementation; ra3_1.0.game 0x00907AB0.
void* initializeW3DModelDrawConditionState(void* state) noexcept
{
    auto* base = static_cast<std::byte*>(state);
    store<std::uint32_t>(base, 0x00, 0U);
    store<std::uint32_t>(base, 0x04, 0U);
    store<std::uint32_t>(base, 0x08, 0U);
    store<std::uint32_t>(base, 0x0C, 0U);
    store<std::uint32_t>(base, 0x10, 0U);
    store<std::uint32_t>(base, 0x18, 0U);
    store<std::uint32_t>(base, 0x1C, 0U);
    store<std::uint32_t>(base, 0x20, 0U);
    store<std::uint32_t>(base, 0x24, 0U);
    store<std::uint32_t>(base, 0x2C, 0U);
    store<std::uint32_t>(base, 0x30, 0U);
    store<std::uint32_t>(base, 0x34, 0U);
    store<std::uint32_t>(base, 0x3C, 0U);
    store<std::uint32_t>(base, 0x40, 0U);
    store<std::uint32_t>(base, 0x44, 0U);

    auto* tree1 = base + 0x50;
    store<void*>(base, 0x50, tree1);
    store<void*>(base, 0x54, tree1);
    store<std::uint32_t>(base, 0x58, 0U);
    store<std::uint8_t>(base, 0x5C, 0U);
    store<std::uint32_t>(base, 0x60, 0U);

    auto* tree2 = base + 0x6C;
    store<void*>(base, 0x6C, tree2);
    store<void*>(base, 0x70, tree2);
    store<std::uint32_t>(base, 0x74, 0U);
    store<std::uint8_t>(base, 0x78, 0U);
    store<std::uint32_t>(base, 0x7C, 0U);
    store<std::uint8_t>(base, 0x84, 0U);
    return state;
}

// Original implementation; ra3_1.0.game 0x00910E50.
void setScriptedModelFlag6D(void* draw, std::uint8_t value) noexcept
{
    store<std::uint8_t>(draw, 0x6D, value);
}

// Original implementation; ra3_1.0.game 0x00910E60.
std::uint8_t getScriptedModelDataFlagD3(const void* draw) noexcept
{
    const auto* moduleData = load<const void*>(draw, 4);
    return load<std::uint8_t>(moduleData, 0xD3);
}

// Original implementation; ra3_1.0.game 0x00910E70.
bool isScriptedModelActive6DOrDataD4(const void* draw) noexcept
{
    if (load<std::uint8_t>(draw, 0x6D) != 0U) {
        return true;
    }
    const auto* moduleData = load<const void*>(draw, 4);
    return load<std::uint8_t>(moduleData, 0xD4) != 0U;
}

// Original implementation; ra3_1.0.game 0x00910E90.
void setScriptedModelFlag6C(void* draw, std::uint8_t value) noexcept
{
    store<std::uint8_t>(draw, 0x6C, value);
}

// Original implementation; ra3_1.0.game 0x00910EB0.
void setScriptedModelFlag216(void* draw, std::uint8_t value) noexcept
{
    store<std::uint8_t>(draw, 0x216, value);
}

// Original implementation; ra3_1.0.game 0x00910EC0.
void* getScriptedModelDrawInterface(void* draw) noexcept
{
    return draw != nullptr ? static_cast<std::byte*>(draw) + 0x14 : nullptr;
}

// Original implementation; ra3_1.0.game 0x00910ED0.
void* getScriptedModelSubInterface18(void* draw) noexcept
{
    return draw != nullptr ? static_cast<std::byte*>(draw) + 0x18 : nullptr;
}

// Original implementation; ra3_1.0.game 0x00910EE0.
void setScriptedModelFlag215(void* draw, std::uint8_t value) noexcept
{
    store<std::uint8_t>(draw, 0x215, value);
}

// Original implementation; ra3_1.0.game 0x00911250.
void* getDynamicsDrawSubInterface290(void* draw) noexcept
{
    return draw != nullptr ? static_cast<std::byte*>(draw) + 0x290 : nullptr;
}

}  // namespace ra3::render
