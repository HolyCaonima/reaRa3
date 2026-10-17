#include "ra3/audio/SageAudioManager50.h"

#include "ra3/audio/AudioEvent.h"

#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstring>

namespace ra3::audio {

namespace {

constexpr std::uint32_t kEmptyAudioEventRtsVa = 0x00CAA11Cu;
constexpr std::uint32_t kSageAudioManagerNameVa = 0x00BDEDB4u;
constexpr std::uint32_t kSagePathMusicManagerNameVa = 0x00BDEDC8u;
constexpr std::size_t kPerPlayerAudioSettingsStride = 0x1FCu;

std::byte* bytes(void* ptr) noexcept {
    return static_cast<std::byte*>(ptr);
}

const std::byte* bytes(const void* ptr) noexcept {
    return static_cast<const std::byte*>(ptr);
}

std::uint32_t& u32(void* base, std::size_t offset) noexcept {
    return *reinterpret_cast<std::uint32_t*>(bytes(base) + offset);
}

const std::uint32_t& u32(const void* base, std::size_t offset) noexcept {
    return *reinterpret_cast<const std::uint32_t*>(bytes(base) + offset);
}

std::int32_t& i32(void* base, std::size_t offset) noexcept {
    return *reinterpret_cast<std::int32_t*>(bytes(base) + offset);
}

const std::int32_t& i32(const void* base, std::size_t offset) noexcept {
    return *reinterpret_cast<const std::int32_t*>(bytes(base) + offset);
}

float& f32(void* base, std::size_t offset) noexcept {
    return *reinterpret_cast<float*>(bytes(base) + offset);
}

const float& f32(const void* base, std::size_t offset) noexcept {
    return *reinterpret_cast<const float*>(bytes(base) + offset);
}

std::uint8_t& u8(void* base, std::size_t offset) noexcept {
    return *reinterpret_cast<std::uint8_t*>(bytes(base) + offset);
}

const std::uint8_t& u8(const void* base, std::size_t offset) noexcept {
    return *reinterpret_cast<const std::uint8_t*>(bytes(base) + offset);
}

std::int8_t& i8(void* base, std::size_t offset) noexcept {
    return *reinterpret_cast<std::int8_t*>(bytes(base) + offset);
}

const std::int8_t& i8(const void* base, std::size_t offset) noexcept {
    return *reinterpret_cast<const std::int8_t*>(bytes(base) + offset);
}

bool floatNotEqualOrUnordered(float a, float b) noexcept {
    return std::isunordered(a, b) || (a != b);
}

using ThiscallIntFn = std::uint32_t(__thiscall*)(void*, int);
using ThiscallNoArgFn = std::uint32_t(__thiscall*)(void*);
using ThiscallLookupFn = void*(__thiscall*)(void*, std::uint32_t);

std::uint32_t invokeVirtualSlot(void* object, std::size_t byteOffset, int arg) noexcept {
    auto* vtable = *reinterpret_cast<std::uintptr_t* const*>(object);
    auto fn = reinterpret_cast<ThiscallIntFn>(vtable[byteOffset / sizeof(std::uintptr_t)]);
    return fn(object, arg);
}

std::uint32_t invokeVirtualSlotNoArg(void* object, std::size_t byteOffset) noexcept {
    auto* vtable = *reinterpret_cast<std::uintptr_t* const*>(object);
    auto fn = reinterpret_cast<ThiscallNoArgFn>(vtable[byteOffset / sizeof(std::uintptr_t)]);
    return fn(object);
}

void* invokeVirtualSlotLookup(void* object, std::size_t byteOffset,
                              std::uint32_t id) noexcept {
    auto* vtable = *reinterpret_cast<std::uintptr_t* const*>(object);
    auto fn =
        reinterpret_cast<ThiscallLookupFn>(vtable[byteOffset / sizeof(std::uintptr_t)]);
    return fn(object, id);
}

}  // namespace

// 0x00502270: RenderingSound speaker-mode helper.
std::uint32_t resolveSpeakerCenterChannelEnabled(std::int32_t modeOverride,
                                                 const void* speakerSystem,
                                                 std::uint32_t initialEax) noexcept {
    (void)initialEax;
    std::uint32_t eax = static_cast<std::uint32_t>(modeOverride);
    if (modeOverride != -1) {
        if (modeOverride == 0) {
            return 0u;
        }
        const std::uint8_t low = (modeOverride == 1) ? 1u : 0u;
        return (eax & 0xFFFFFF00u) | low;
    }
    const std::uint32_t index = u32(speakerSystem, 0x2E4);
    const void* config =
        *reinterpret_cast<const void* const*>(bytes(speakerSystem) + index * 4u + 0x54u);
    const std::uint8_t low = u8(config, 0x29);
    return (index & 0xFFFFFF00u) | low;
}

// 0x00502F00: RenderingSound::pushSampleDuration.
std::uint32_t pushRenderingSoundSampleDuration(void* renderingSound, float duration,
                                               std::uint32_t initialEax) noexcept {
    const std::int8_t count = i8(renderingSound, 0x55);
    const std::int8_t capacity = i8(renderingSound, 0x56);
    std::uint32_t eax =
        (initialEax & 0xFFFFFF00u) | static_cast<std::uint8_t>(count);
    if (count < capacity) {
        const std::int32_t head = static_cast<std::int32_t>(i8(renderingSound, 0x54));
        std::int32_t index = head + static_cast<std::int32_t>(count);
        if (index >= 10) {
            index -= 10;
        }
        f32(renderingSound, static_cast<std::size_t>(index) * 4u + 0x28u) = duration;
        u8(renderingSound, 0x55) = static_cast<std::uint8_t>(count + 1);
        eax = static_cast<std::uint32_t>(index);
    }
    return eax;
}

// 0x00502FB0: 00BDF5A8 slot 73 (SageAudioManager::emptyAudioEventRTS).
std::uint32_t sageAudioManagerEmptyEventAddress() noexcept {
    return kEmptyAudioEventRtsVa;
}

// 0x00502FF0: 00BDF5A8 slot 4 (SageAudioManager::stopAllAudioImmediately).
std::uint32_t sageAudioManagerStopAllImmediately(void* manager) noexcept {
    return invokeVirtualSlot(manager, 0x38, 1);
}

// 0x00503000: 00BDF5A8 slot 102 (SageAudioManager::updateFrameDeltaSeconds).
float sageAudioManagerUpdateFrameDeltaSeconds(void* manager, std::uint32_t listenerCount,
                                              std::uint8_t pausedFlag, void* timerSource,
                                              float defaultStep, std::uint32_t* outEax,
                                              std::uint32_t initialEax) noexcept {
    std::uint32_t eax = initialEax;
    if (listenerCount == 0u && pausedFlag == 0u) {
        if (timerSource == nullptr) {
            if (outEax != nullptr) {
                *outEax = eax;
            }
            return defaultStep;
        }
        const std::uint32_t now = invokeVirtualSlotNoArg(timerSource, 0x74);
        const std::uint32_t prev = u32(manager, 0x48);
        float delta = 0.0f;
#if defined(_MSC_VER) && defined(_M_IX86)
        const float twoPow32 = 4294967296.0f;
        __asm {
            fild dword ptr [now]
            mov eax, dword ptr [now]
            test eax, eax
            jge now_nonneg
            fadd dword ptr [twoPow32]
        now_nonneg:
            fild dword ptr [prev]
            mov eax, dword ptr [prev]
            test eax, eax
            jge prev_nonneg
            fadd dword ptr [twoPow32]
        prev_nonneg:
            fsubp st(1), st(0)
            fmul dword ptr [defaultStep]
            fstp dword ptr [delta]
        }
#else
        const double diff = static_cast<double>(now) - static_cast<double>(prev);
        delta = static_cast<float>(diff * static_cast<double>(defaultStep));
#endif
        if (0.0f > delta || std::isunordered(0.0f, delta)) {
            delta = 0.0f;
        }
        const std::uint32_t nextTick = invokeVirtualSlotNoArg(timerSource, 0x74);
        u32(manager, 0x48) = nextTick;
        if (outEax != nullptr) {
            *outEax = nextTick;
        }
        return delta;
    }

    if (timerSource != nullptr) {
        eax = invokeVirtualSlotNoArg(timerSource, 0x74);
        u32(manager, 0x48) = eax;
    }
    if (outEax != nullptr) {
        *outEax = eax;
    }
    return defaultStep;
}

// 0x00503290: 00BDF5A8 slot 20 (SageAudioManager::listenerOrientationCount).
std::uint32_t sageAudioManagerListenerOrientationCount(
    const std::uint32_t* globalListenerCount) noexcept {
    return *globalListenerCount;
}

// 0x00503300: 00BDF594 slot 2 (SageAudioManager::subsystemName).
const char* sageAudioManagerSubsystemNameLiteral() noexcept {
    return "SageAudioManager";
}

std::uint32_t sageAudioManagerSubsystemNameAddress() noexcept {
    return kSageAudioManagerNameVa;
}

// 0x00503400: 00BDF3A4 slot 2 (SagePathMusicManager::subsystemName).
const char* sagePathMusicManagerSubsystemNameLiteral() noexcept {
    return "SagePathMusicManager";
}

std::uint32_t sagePathMusicManagerSubsystemNameAddress() noexcept {
    return kSagePathMusicManagerNameVa;
}

// 0x005044F0: AudioEventRTSHandle::initFromPointer.
void* initAudioEventRtsHandle(void* handleStorage, void* eventRts) noexcept {
    *reinterpret_cast<void**>(handleStorage) = eventRts;
    if (eventRts != nullptr) {
        i32(eventRts, 0x98) += 1;
    }
    return handleStorage;
}

// 0x00504560: AudioEventRTSHandle::release.
std::uint32_t releaseAudioEventRtsHandle(void* handleStorage) noexcept {
    void* eventRts = *reinterpret_cast<void**>(handleStorage);
    std::uint32_t eax = static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(eventRts));
    if (eventRts != nullptr) {
        i32(eventRts, 0x98) -= 1;
        if (i32(eventRts, 0x98) <= 0) {
            void* subObject = bytes(eventRts) + 0x94;
            eax = invokeVirtualSlot(subObject, 0x00, 1);
        }
    }
    return eax;
}

// 0x00504590: PlayingAudioHandle::releaseAndClear.
std::uint32_t releaseAndClearPlayingAudioHandle(void* handleStorage,
                                                std::uint32_t initialEax) noexcept {
    void* playingAudio = *reinterpret_cast<void**>(handleStorage);
    std::uint32_t eax = initialEax;
    if (playingAudio != nullptr) {
        i32(playingAudio, 0x04) -= 1;
        if (i32(playingAudio, 0x04) <= 0) {
            eax = invokeVirtualSlot(playingAudio, 0x00, 1);
        }
        *reinterpret_cast<void**>(handleStorage) = nullptr;
    }
    return eax;
}

// 0x005045C0: PlayingAudioHandle::release.
std::uint32_t releasePlayingAudioHandle(void* handleStorage,
                                        std::uint32_t initialEax) noexcept {
    void* playingAudio = *reinterpret_cast<void**>(handleStorage);
    std::uint32_t eax = initialEax;
    if (playingAudio != nullptr) {
        i32(playingAudio, 0x04) -= 1;
        if (i32(playingAudio, 0x04) <= 0) {
            eax = invokeVirtualSlot(playingAudio, 0x00, 1);
        }
    }
    return eax;
}

// 0x005045E0: PlayingAudioNode::unlinkFromList.
std::uint32_t unlinkPlayingAudioNode(void* node) noexcept {
    void* prev = *reinterpret_cast<void**>(bytes(node) + 0x04);
    void* next = *reinterpret_cast<void**>(bytes(node) + 0x08);
    if (prev != nullptr) {
        *reinterpret_cast<void**>(bytes(prev) + 0x08) = next;
    } else {
        void* owner = *reinterpret_cast<void**>(node);
        *reinterpret_cast<void**>(bytes(owner) + 0xC0) = next;
    }

    next = *reinterpret_cast<void**>(bytes(node) + 0x08);
    prev = *reinterpret_cast<void**>(bytes(node) + 0x04);
    std::uint32_t eax = static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(next));
    if (next != nullptr) {
        *reinterpret_cast<void**>(bytes(next) + 0x04) = prev;
    } else {
        void* owner = *reinterpret_cast<void**>(node);
        *reinterpret_cast<void**>(bytes(owner) + 0xC4) = prev;
    }
    u32(node, 0x08) = 0u;
    u32(node, 0x04) = 0u;
    return eax;
}

// 0x00504AC0: AudioEventRTS::testCategoryBit.
std::uint32_t testAudioEventCategoryBit(const void* eventInfo,
                                        std::uint32_t bitIndex) noexcept {
    const std::uint32_t mask = 1u << (bitIndex & 0x1Fu);
    const std::uint32_t wordIndex = bitIndex >> 5u;
    const std::uint32_t word = u32(eventInfo, wordIndex * 4u + 0x78u);
    return (word & mask) != 0u ? 1u : 0u;
}

// 0x00504AF0: AudioEventRTS::intersectsCategoryMask.
std::uint32_t intersectsAudioEventCategoryMask(const void* eventInfo,
                                               const std::uint32_t* mask) noexcept {
    const std::uint32_t word = u32(eventInfo, 0x78);
    return (word & mask[0]) != 0u ? 1u : 0u;
}

// 0x00504B20: PlayingAudioPoolNode::initEntry.
void* initPlayingAudioPoolNode(void* node, void* nextNode, void* prevNode,
                               void* refCountedPayload) noexcept {
    *reinterpret_cast<void**>(node) = nextNode;
    *reinterpret_cast<void**>(bytes(node) + 0x04) = prevNode;
    *reinterpret_cast<void**>(bytes(node) + 0x08) = refCountedPayload;
    if (refCountedPayload != nullptr) {
        i32(refCountedPayload, 0x04) += 1;
        i32(refCountedPayload, 0x04) -= 1;
        if (i32(refCountedPayload, 0x04) <= 0) {
            invokeVirtualSlot(refCountedPayload, 0x00, 1);
        }
    }
    return node;
}

// 0x00504C80: PerPlayerAudioSettings::recomputeFinalVolume.
std::uint32_t recomputePlayerCategoryFinalVolume(
    void* playerSettings, std::uint32_t category, std::uint32_t subchannel,
    const void* globalAudioSettings, const float* globalCategoryScaleTable) noexcept {
    const std::uint32_t pairIndex = subchannel + category * 2u;
    const std::size_t blockOffset = static_cast<std::size_t>(pairIndex + 0x0Fu) << 4u;
    std::byte* block = bytes(playerSettings) + blockOffset;

    float saved[4];
    for (std::size_t i = 0; i < 4; ++i) {
        saved[i] = f32(block, i * 4u);
    }

    if (category == 5u) {
        f32(block, 0x00) = 1.0f;
        f32(block, 0x04) = 1.0f;
        f32(block, 0x08) = 1.0f;
        f32(block, 0x0C) = 1.0f;
    } else {
        float baseVol = f32(playerSettings, pairIndex * 4u + 0x04u);
        baseVol *= globalCategoryScaleTable[category];
        baseVol *= f32(playerSettings, 0xB0);
        f32(block, 0x00) = baseVol;

        if (subchannel == 1u) {
            float ducked = f32(playerSettings, 0xAC) * f32(block, 0x00);
            f32(block, 0x00) = ducked;
            if (globalAudioSettings != nullptr) {
                f32(block, 0x00) = f32(globalAudioSettings, 0x78) * ducked;
            }
        } else if (globalAudioSettings != nullptr) {
            f32(block, 0x00) = f32(globalAudioSettings, 0x74) * f32(block, 0x00);
        }

        const float v0 = f32(block, 0x00);
        f32(block, 0x04) = v0 * f32(playerSettings, 0xB4);
        const float v2 = f32(playerSettings, category * 4u + 0x34u) * v0;
        f32(block, 0x08) = v2;
        f32(block, 0x0C) = v2 * f32(playerSettings, 0xB4);
    }

    const std::size_t dirtyBase = static_cast<std::size_t>(pairIndex + 0x6Cu) * 4u;
    std::uint32_t eax = 0u;
    for (std::size_t i = 0; i < 4; ++i) {
        if (floatNotEqualOrUnordered(f32(block, i * 4u), saved[i])) {
            eax = pairIndex + 0x6Cu;
            u8(playerSettings, dirtyBase + i) = 2u;
        }
    }
    return eax;
}

// 0x00504DF0: PerPlayerAudioSettings::recomputeCategoryModifierProduct.
std::uint32_t recomputePlayerCategoryModifierProduct(
    void* playerSettings, std::uint32_t category,
    std::int32_t globalModifierLockCount, const void* globalAudioSettings) noexcept {
    const std::size_t productOffset = static_cast<std::size_t>(category) * 4u + 0x34u;
    f32(playerSettings, productOffset) = 1.0f;

    const std::size_t beginPtrOffset = (static_cast<std::size_t>(category) << 4u) + 0x4Cu;
    const std::size_t endPtrOffset = static_cast<std::size_t>(category + 5u) << 4u;
    const std::byte* node = *reinterpret_cast<const std::byte* const*>(
        bytes(playerSettings) + beginPtrOffset);
    const std::byte* endNode = *reinterpret_cast<const std::byte* const*>(
        bytes(playerSettings) + endPtrOffset);
    while (node != endNode) {
        f32(playerSettings, productOffset) *=
            *reinterpret_cast<const float*>(node);
        node += 8;
    }

    std::uint32_t eax =
        static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(endNode));
    if (globalModifierLockCount > 0) {
        eax = static_cast<std::uint32_t>(
            reinterpret_cast<std::uintptr_t>(globalAudioSettings));
        if (globalAudioSettings != nullptr) {
            const std::byte* entry =
                *reinterpret_cast<const std::byte* const*>(bytes(globalAudioSettings) + 0xD4);
            const std::uint32_t count = u32(globalAudioSettings, 0xD0);
            const std::byte* entryEnd = entry + static_cast<std::size_t>(count) * 8u;
            while (entry != entryEnd) {
                if (*reinterpret_cast<const std::uint32_t*>(entry) == category) {
                    const float factor = *reinterpret_cast<const float*>(entry + 4);
                    f32(playerSettings, productOffset) *= factor;
                }
                entry += 8;
            }
        }
    }

    float clamped = f32(playerSettings, productOffset);
    if (0.0f > clamped) {
        clamped = 0.0f;
    } else if (clamped > 1.0f) {
        clamped = 1.0f;
    }
    f32(playerSettings, productOffset) = clamped;
    return eax;
}

// 0x00504EC0: PerPlayerAudioSettings::decrementVolumeChangeCounters.
std::uint32_t decrementPlayerVolumeChangeCounters(void* playerSettings) noexcept {
    std::byte* cursor = bytes(playerSettings) + 0x1B0;
    for (int i = 0; i < 12; ++i) {
        for (int j = 0; j < 4; ++j) {
            auto& counter = *reinterpret_cast<std::int8_t*>(cursor + j);
            if (counter > 0) {
                counter = static_cast<std::int8_t>(counter - 1);
            }
        }
        cursor += 4;
    }
    return static_cast<std::uint32_t>(
        reinterpret_cast<std::uintptr_t>(bytes(playerSettings) + 0x1E2));
}

// 0x00504F20: PerPlayerAudioSettings::hasPendingVolumeChangeForEvent.
std::uint32_t hasPendingVolumeChangeForEvent(const void* playerSettings,
                                             void* const* eventInfoPtr,
                                             std::uint32_t subchannel) noexcept {
    const void* eventInfo = *eventInfoPtr;
    const std::uint32_t fadeOffset =
        (u32(eventInfo, 0xF0) == u32(eventInfo, 0xF4)) ? 2u : 0u;
    const std::uint32_t category = AudioEventInfoView(eventInfo).resolvedPriority();
    const std::uint32_t baseOffset = (subchannel + category * 2u + 0x6Cu) * 4u;
    if (u8(playerSettings, baseOffset + fadeOffset) != 0u) {
        return (baseOffset & 0xFFFFFF00u) | 1u;
    }
    const std::uint8_t secondByte = u8(playerSettings, baseOffset + (fadeOffset | 1u));
    return (baseOffset & 0xFFFFFF00u) | (secondByte != 0u ? 1u : 0u);
}

// 0x00504F80: PerPlayerAudioSettings::updateDuckingMultiplierFromDistance.
std::uint32_t updatePlayerDuckingMultiplierFromDistance(
    void* playerSettings, const void* duckingParams, const float* deltaVector3,
    const void* globalAudioSettings, const float* globalCategoryScaleTable) noexcept {
    const float maxRadius = f32(duckingParams, 0x1C);
    const float maxDuck = f32(duckingParams, 0x20);
    const float maxRadiusSq = maxRadius * maxRadius;
    if (maxDuck > 0.0f && !std::isunordered(maxDuck, 0.0f)) {
        const float dx = deltaVector3[0];
        const float dy = deltaVector3[1];
        const float dz = deltaVector3[2];
        const float distSq = (dx * dx) + (dy * dy) + (dz * dz);
        const float minRadius = f32(duckingParams, 0x18);
        const float minRadiusSq = minRadius * minRadius;
        float multiplier = 1.0f;
        if (minRadiusSq > distSq) {
            multiplier = 1.0f;
        } else if (maxRadiusSq > distSq) {
            float dist = 0.0f;
#if defined(_MSC_VER) && defined(_M_IX86)
            __asm {
                fld dword ptr [distSq]
                fsqrt
                fstp dword ptr [dist]
            }
#else
            dist = std::sqrt(distSq);
#endif
            const float t = ((dist - minRadius) / (maxRadius - minRadius)) * maxDuck;
            multiplier = 1.0f - t;
        } else {
            multiplier = 1.0f - maxDuck;
        }
        f32(playerSettings, 0xAC) = multiplier;
    }

    std::uint32_t eax = 0u;
    for (std::uint32_t category = 0; category < 6u; ++category) {
        eax = recomputePlayerCategoryFinalVolume(playerSettings, category, 1u,
                                                 globalAudioSettings,
                                                 globalCategoryScaleTable);
    }
    return eax;
}

// 0x00505150: PerPlayerAudioSettings::applyVolumeSettingsByFlags.
std::uint32_t applyPlayerVolumeSettingsByFlags(
    void* playerSettings, float value, std::uint8_t flags,
    const void* globalAudioSettings, const float* globalCategoryScaleTable) noexcept {
    std::uint32_t eax = 0u;
    if ((flags & 0x01u) != 0u) {
        f32(playerSettings, 0x14) = value;
        f32(playerSettings, 0x18) = value;
        for (std::uint32_t sub = 0; sub < 2u; ++sub) {
            eax = recomputePlayerCategoryFinalVolume(playerSettings, 2u, sub,
                                                     globalAudioSettings,
                                                     globalCategoryScaleTable);
        }
    }

    if ((flags & 0x02u) != 0u) {
        f32(playerSettings, 0x04) = value;
        float saved[4];
        for (std::size_t i = 0; i < 4; ++i) {
            saved[i] = f32(playerSettings, 0xF0u + i * 4u);
        }

        float v0 = f32(playerSettings, 0xB0) * globalCategoryScaleTable[0] *
                   f32(playerSettings, 0x04);
        f32(playerSettings, 0xF0) = v0;
        eax = static_cast<std::uint32_t>(
            reinterpret_cast<std::uintptr_t>(globalAudioSettings));
        if (globalAudioSettings != nullptr) {
            v0 = f32(globalAudioSettings, 0x74) * f32(playerSettings, 0xF0);
            f32(playerSettings, 0xF0) = v0;
        }
        const float base0 = f32(playerSettings, 0xF0);
        f32(playerSettings, 0xF4) = f32(playerSettings, 0xB4) * base0;
        const float v2 = f32(playerSettings, 0x34) * base0;
        f32(playerSettings, 0xF8) = v2;
        f32(playerSettings, 0xFC) = f32(playerSettings, 0xB4) * v2;

        for (std::size_t i = 0; i < 4; ++i) {
            if (floatNotEqualOrUnordered(f32(playerSettings, 0xF0u + i * 4u), saved[i])) {
                u8(playerSettings, 0x1B0u + i) = 2u;
            }
        }
    }

    if ((flags & 0x04u) != 0u) {
        f32(playerSettings, 0x08) = value;
        eax = recomputePlayerCategoryFinalVolume(playerSettings, 0u, 1u,
                                                 globalAudioSettings,
                                                 globalCategoryScaleTable);
    }

    if ((flags & 0x08u) != 0u) {
        f32(playerSettings, 0x0C) = value;
        f32(playerSettings, 0x24) = value;
        f32(playerSettings, 0x10) = value;
        f32(playerSettings, 0x28) = value;
        for (std::uint32_t sub = 0; sub < 2u; ++sub) {
            eax = recomputePlayerCategoryFinalVolume(playerSettings, 1u, sub,
                                                     globalAudioSettings,
                                                     globalCategoryScaleTable);
        }
        for (std::uint32_t sub = 0; sub < 2u; ++sub) {
            eax = recomputePlayerCategoryFinalVolume(playerSettings, 4u, sub,
                                                     globalAudioSettings,
                                                     globalCategoryScaleTable);
        }
    }

    if ((flags & 0x10u) != 0u) {
        f32(playerSettings, 0x1C) = value;
        f32(playerSettings, 0x20) = value;
        for (std::uint32_t sub = 0; sub < 2u; ++sub) {
            eax = recomputePlayerCategoryFinalVolume(playerSettings, 3u, sub,
                                                     globalAudioSettings,
                                                     globalCategoryScaleTable);
        }
    }
    return eax;
}

// 0x00505340: SageAudioManager::acquireGlobalVolumeModifierLock.
std::uint32_t acquireGlobalVolumeModifierLock(
    const void* modifierGuard, std::int32_t& globalModifierLockCount,
    void* playerSettingsArray3, const void* globalAudioSettings,
    const float* globalCategoryScaleTable) noexcept {
    std::uint32_t eax =
        static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(modifierGuard));
    if (modifierGuard != nullptr && i32(modifierGuard, 0x18) != 0) {
        return eax;
    }
    globalModifierLockCount += 1;
    eax = static_cast<std::uint32_t>(globalModifierLockCount);
    if (globalModifierLockCount != 1) {
        return eax;
    }
    for (std::size_t player = 0; player < 3u; ++player) {
        void* playerSettings =
            bytes(playerSettingsArray3) + player * kPerPlayerAudioSettingsStride;
        for (std::uint32_t category = 0; category < 6u; ++category) {
            recomputePlayerCategoryModifierProduct(
                playerSettings, category, globalModifierLockCount, globalAudioSettings);
            for (std::uint32_t sub = 0; sub < 2u; ++sub) {
                eax = recomputePlayerCategoryFinalVolume(playerSettings, category, sub,
                                                         globalAudioSettings,
                                                         globalCategoryScaleTable);
            }
        }
    }
    return eax;
}

// 0x005053B0: SageAudioManager::releaseGlobalVolumeModifierLock.
std::uint32_t releaseGlobalVolumeModifierLock(
    const void* modifierGuard, std::int32_t& globalModifierLockCount,
    void* playerSettingsArray3, const void* globalAudioSettings,
    const float* globalCategoryScaleTable) noexcept {
    std::uint32_t eax =
        static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(modifierGuard));
    if (modifierGuard != nullptr && i32(modifierGuard, 0x18) != 0) {
        return eax;
    }
    globalModifierLockCount -= 1;
    if (globalModifierLockCount != 0) {
        return eax;
    }
    for (std::size_t player = 0; player < 3u; ++player) {
        void* playerSettings =
            bytes(playerSettingsArray3) + player * kPerPlayerAudioSettingsStride;
        for (std::uint32_t category = 0; category < 6u; ++category) {
            recomputePlayerCategoryModifierProduct(
                playerSettings, category, globalModifierLockCount, globalAudioSettings);
            for (std::uint32_t sub = 0; sub < 2u; ++sub) {
                eax = recomputePlayerCategoryFinalVolume(playerSettings, category, sub,
                                                         globalAudioSettings,
                                                         globalCategoryScaleTable);
            }
        }
    }
    return eax;
}

// 0x00506670: 00BDF5A8 slot 71 (SageAudioManager::onAcquireVolumeModifierLock).
std::uint32_t sageAudioManagerOnAcquireVolumeModifierLock(
    const void* modifierGuard, std::int32_t& globalModifierLockCount,
    void* playerSettingsArray3, const void* globalAudioSettings,
    const float* globalCategoryScaleTable) noexcept {
    return acquireGlobalVolumeModifierLock(modifierGuard, globalModifierLockCount,
                                           playerSettingsArray3, globalAudioSettings,
                                           globalCategoryScaleTable);
}

// 0x00506680: 00BDF5A8 slot 72 (SageAudioManager::onReleaseVolumeModifierLock).
std::uint32_t sageAudioManagerOnReleaseVolumeModifierLock(
    const void* modifierGuard, std::int32_t& globalModifierLockCount,
    void* playerSettingsArray3, const void* globalAudioSettings,
    const float* globalCategoryScaleTable) noexcept {
    return releaseGlobalVolumeModifierLock(modifierGuard, globalModifierLockCount,
                                           playerSettingsArray3, globalAudioSettings,
                                           globalCategoryScaleTable);
}

// 0x00506C20: AudioCategoryBitset::initFromFourBits.
void* initCategoryBitsetFromFourBits(void* bitset, std::uint32_t bit0,
                                     std::uint32_t bit1, std::uint32_t bit2,
                                     std::uint32_t bit3) noexcept {
    u32(bitset, 0x00) = 0u;
    u32(bitset, (bit0 >> 5u) * 4u) |= (1u << (bit0 & 0x1Fu));
    u32(bitset, (bit1 >> 5u) * 4u) |= (1u << (bit1 & 0x1Fu));
    u32(bitset, (bit2 >> 5u) * 4u) |= (1u << (bit2 & 0x1Fu));
    u32(bitset, (bit3 >> 5u) * 4u) |= (1u << (bit3 & 0x1Fu));
    return bitset;
}

// 0x00506C90: AudioCategoryBitset::initFromTwoBits.
void* initCategoryBitsetFromTwoBits(void* bitset, std::uint32_t bit0,
                                    std::uint32_t bit1) noexcept {
    u32(bitset, 0x00) = 0u;
    u32(bitset, (bit0 >> 5u) * 4u) |= (1u << (bit0 & 0x1Fu));
    u32(bitset, (bit1 >> 5u) * 4u) |= (1u << (bit1 & 0x1Fu));
    return bitset;
}

// 0x00506CD0: PlayingAudioNode::unlinkIfOwned.
std::uint32_t unlinkPlayingAudioNodeIfOwned(void* node,
                                            std::uint32_t initialEax) noexcept {
    void* owner = *reinterpret_cast<void**>(bytes(node) + 0x0C);
    std::uint32_t eax = initialEax;
    if (owner != nullptr) {
        void* prev = *reinterpret_cast<void**>(bytes(node) + 0x04);
        void* next = *reinterpret_cast<void**>(bytes(node) + 0x08);
        if (prev != nullptr) {
            *reinterpret_cast<void**>(bytes(prev) + 0x08) = next;
        } else {
            *reinterpret_cast<void**>(bytes(owner) + 0x200) = next;
        }
        next = *reinterpret_cast<void**>(bytes(node) + 0x08);
        prev = *reinterpret_cast<void**>(bytes(node) + 0x04);
        if (next != nullptr) {
            *reinterpret_cast<void**>(bytes(next) + 0x04) = prev;
            eax = static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(prev));
        } else {
            void* ownerAgain = *reinterpret_cast<void**>(bytes(node) + 0x0C);
            *reinterpret_cast<void**>(bytes(ownerAgain) + 0x204) = prev;
            eax = static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(ownerAgain));
        }
        u32(node, 0x08) = 0u;
        u32(node, 0x04) = 0u;
        u32(node, 0x0C) = 0u;
    }
    return eax;
}

// 0x00507720: PlayingAudio::hasActiveRenderCursor.
std::uint32_t hasActiveRenderCursor(const void* playingAudioSlot) noexcept {
    const void* playingAudio = *reinterpret_cast<const void* const*>(playingAudioSlot);
    if (playingAudio == nullptr) {
        return 0u;
    }
    const void* renderChannel =
        *reinterpret_cast<const void* const*>(bytes(playingAudio) + 0x10);
    const std::uint32_t cursorA = u32(renderChannel, 0x20);
    if (cursorA != 0u) {
        return (cursorA & 0xFFFFFF00u) | 1u;
    }
    const std::uint32_t cursorB = u32(playingAudio, 0x1C);
    return (cursorB & 0xFFFFFF00u) | (cursorB != 0u ? 1u : 0u);
}

// 0x00508330: RenderingSound::sumWindowSamplePlayheadRatios.
double sumWindowSamplePlayheadRatios(const void* renderingSound,
                                     std::uint32_t* outEax) noexcept {
    double total = 0.0;
    const void* cursorContainer =
        *reinterpret_cast<const void* const*>(bytes(renderingSound) + 0x20);
    std::uint32_t eax =
        static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(cursorContainer));
    if (cursorContainer != nullptr) {
        const std::byte* cursor =
            *reinterpret_cast<const std::byte* const*>(bytes(cursorContainer) + 0x08);
        eax = static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(cursor));
        for (; cursor != nullptr;
             cursor = *reinterpret_cast<const std::byte* const*>(cursor + 0x08)) {
            const void* sampleBlock =
                *reinterpret_cast<const void* const*>(cursor + 0x0C);
            const std::uint32_t count = u32(sampleBlock, 0x0C);
            eax = 0u;
            if (count != 0u) {
                const std::byte* entry = bytes(sampleBlock) + 0x14;
                for (; eax < count; ++eax, entry += 0x10) {
                    const std::uint32_t totalSamples = u32(entry, 0x00);
                    if (totalSamples != 0u) {
                        const std::uint32_t currentSample = u32(entry, 0x04);
                        total += static_cast<double>(currentSample) /
                                 static_cast<double>(totalSamples);
                    }
                }
            }
        }
    }

    const void* window =
        *reinterpret_cast<const void* const*>(bytes(renderingSound) + 0x1C);
    eax = static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(window));
    if (window != nullptr) {
        const std::byte* begin =
            *reinterpret_cast<const std::byte* const*>(bytes(window) + 0x20);
        const std::byte* end =
            *reinterpret_cast<const std::byte* const*>(bytes(window) + 0x24);
        for (const std::byte* it = begin; it != end; it += 4) {
            const void* item = *reinterpret_cast<const void* const*>(it);
            for (const std::byte* sub = bytes(item) + 0x20;
                 sub != bytes(item) + 0x40; sub += 4) {
                const void* subBlock = *reinterpret_cast<const void* const*>(sub);
                eax = static_cast<std::uint32_t>(
                    reinterpret_cast<std::uintptr_t>(subBlock));
                if (subBlock != nullptr) {
                    const std::uint32_t count = u32(subBlock, 0x0C);
                    eax = 0u;
                    if (count != 0u) {
                        const std::byte* entry = bytes(subBlock) + 0x14;
                        for (; eax < count; ++eax, entry += 0x10) {
                            const std::uint32_t totalSamples = u32(entry, 0x00);
                            if (totalSamples != 0u) {
                                const std::uint32_t currentSample = u32(entry, 0x04);
                                total += static_cast<double>(currentSample) /
                                         static_cast<double>(totalSamples);
                            }
                        }
                    }
                }
            }
        }
    }

    if (outEax != nullptr) {
        *outEax = eax;
    }
    return total;
}

// 0x005084B0: RenderingSound::sumAllChannelSamplePlayheadRatios.
double sumAllChannelSamplePlayheadRatios(const void* renderingSound,
                                         std::uint32_t* outEax) noexcept {
    double total = 0.0;
    std::uint32_t eax = 0u;
    for (std::size_t offset = 0x20; offset != 0x40; offset += 4) {
        const void* block =
            *reinterpret_cast<const void* const*>(bytes(renderingSound) + offset);
        eax = static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(block));
        if (block != nullptr) {
            const std::uint32_t count = u32(block, 0x0C);
            eax = 0u;
            if (count != 0u) {
                const std::byte* entry = bytes(block) + 0x14;
                for (; eax < count; ++eax, entry += 0x10) {
                    const std::uint32_t totalSamples = u32(entry, 0x00);
                    if (totalSamples != 0u) {
                        const std::uint32_t currentSample = u32(entry, 0x04);
                        total += static_cast<double>(currentSample) /
                                 static_cast<double>(totalSamples);
                    }
                }
            }
        }
    }
    if (outEax != nullptr) {
        *outEax = eax;
    }
    return total;
}

// 0x00508640: PlayingAudioNode::appendToOwnerList.
void* appendPlayingAudioNodeToOwnerList(void* node, void* owner) noexcept {
    u32(node, 0x08) = 0u;
    *reinterpret_cast<void**>(bytes(node) + 0x0C) = owner;
    void* tail = *reinterpret_cast<void**>(bytes(owner) + 0x204);
    *reinterpret_cast<void**>(bytes(node) + 0x04) = tail;
    if (tail != nullptr) {
        void* prev = *reinterpret_cast<void**>(bytes(node) + 0x04);
        *reinterpret_cast<void**>(bytes(prev) + 0x08) = node;
        void* nodeOwner = *reinterpret_cast<void**>(bytes(node) + 0x0C);
        *reinterpret_cast<void**>(bytes(nodeOwner) + 0x204) = node;
        return nodeOwner;
    }
    void* nodeOwner = *reinterpret_cast<void**>(bytes(node) + 0x0C);
    *reinterpret_cast<void**>(bytes(nodeOwner) + 0x200) = node;
    nodeOwner = *reinterpret_cast<void**>(bytes(node) + 0x0C);
    *reinterpret_cast<void**>(bytes(nodeOwner) + 0x204) = node;
    return nodeOwner;
}

// 0x00508AF0: SageAudioManager::findPlayingAudioByHandle.
void* findPlayingAudioByHandle(void* manager, void** outHandle,
                               const std::uint32_t* targetIdPtr) noexcept {
    void* found = nullptr;

    auto retainIfNonNull = [](void* obj) noexcept {
        if (obj != nullptr) {
            i32(obj, 0x04) += 1;
        }
    };
    auto releaseTemp = [&]() noexcept {
        if (found != nullptr) {
            i32(found, 0x04) -= 1;
            if (i32(found, 0x04) <= 0) {
                invokeVirtualSlot(found, 0x00, 1);
            }
        }
    };

    const std::byte* sentinelA = bytes(manager) + 0x1BC;
    for (const std::byte* node =
             *reinterpret_cast<const std::byte* const*>(bytes(manager) + 0x1BC);
         node != sentinelA;
         node = *reinterpret_cast<const std::byte* const*>(node)) {
        void* candidate = *reinterpret_cast<void* const*>(node + 0x08);
        retainIfNonNull(candidate);
        releaseTemp();
        found = candidate;
        const void* eventRts = *reinterpret_cast<const void* const*>(bytes(found) + 0x0C);
        if (eventRts != nullptr && u32(eventRts, 0x70) == *targetIdPtr) {
            *outHandle = found;
            retainIfNonNull(found);
            releaseTemp();
            return outHandle;
        }
    }

    const std::byte* sentinelB = bytes(manager) + 0x1C8;
    for (const std::byte* node =
             *reinterpret_cast<const std::byte* const*>(bytes(manager) + 0x1C8);
         node != sentinelB;
         node = *reinterpret_cast<const std::byte* const*>(node)) {
        void* candidate = *reinterpret_cast<void* const*>(node + 0x08);
        retainIfNonNull(candidate);
        releaseTemp();
        found = candidate;
        const void* eventRts = *reinterpret_cast<const void* const*>(bytes(found) + 0x0C);
        if (eventRts != nullptr && u32(eventRts, 0x70) == *targetIdPtr) {
            *outHandle = found;
            retainIfNonNull(found);
            releaseTemp();
            return outHandle;
        }
    }

    const std::byte* sentinelC = bytes(manager) + 0x1D4;
    for (const std::byte* node =
             *reinterpret_cast<const std::byte* const*>(bytes(manager) + 0x1D4);
         node != sentinelC;
         node = *reinterpret_cast<const std::byte* const*>(node)) {
        void* candidate = *reinterpret_cast<void* const*>(node + 0x08);
        retainIfNonNull(candidate);
        releaseTemp();
        found = candidate;
        if (u32(found, 0x08) == *targetIdPtr) {
            *outHandle = found;
            retainIfNonNull(found);
            releaseTemp();
            return outHandle;
        }
        const void* eventRts = *reinterpret_cast<const void* const*>(bytes(found) + 0x0C);
        if (eventRts != nullptr && u32(eventRts, 0x70) == *targetIdPtr) {
            *outHandle = found;
            retainIfNonNull(found);
            releaseTemp();
            return outHandle;
        }
    }

    *outHandle = nullptr;
    releaseTemp();
    return outHandle;
}

// 0x00508D50: SageAudioManager::hasActiveEventForObjectOrDrawable.
std::uint32_t hasActiveEventForObjectOrDrawable(const void* manager,
                                                std::uint32_t objectId,
                                                std::uint32_t drawableId,
                                                std::uint32_t initialEax) noexcept {
    std::uint32_t eax = initialEax;

    const auto scanList = [&](std::size_t offset, bool& matched) noexcept {
        const std::byte* sentinel = bytes(manager) + offset;
        for (const std::byte* node =
                 *reinterpret_cast<const std::byte* const*>(bytes(manager) + offset);
             node != sentinel;
             node = *reinterpret_cast<const std::byte* const*>(node)) {
            const void* playing = *reinterpret_cast<const void* const*>(node + 0x08);
            const void* eventRts =
                *reinterpret_cast<const void* const*>(bytes(playing) + 0x0C);
            if (objectId != 0u) {
                eax = AudioEventView(eventRts).objectId();
                if (eax == objectId) {
                    matched = true;
                    return;
                }
            }
            if (drawableId != 0u) {
                eax = AudioEventView(eventRts).drawableId();
                if (eax == drawableId) {
                    matched = true;
                    return;
                }
            }
        }
    };

    bool matched = false;
    scanList(0x1BC, matched);
    if (matched) {
        return (eax & 0xFFFFFF00u) | 1u;
    }
    scanList(0x1C8, matched);
    if (matched) {
        return (eax & 0xFFFFFF00u) | 1u;
    }
    return eax & 0xFFFFFF00u;
}

// 0x0050A220: PerPlayerAudioSettings::initializeDefaults.
void* initializePlayerAudioSettingsDefaults(
    void* playerSettings, void* playerSettingsArrayBase,
    const void* globalAudioSettings, const float* globalCategoryScaleTable) noexcept {
    const std::ptrdiff_t byteDiff =
        bytes(playerSettings) - bytes(playerSettingsArrayBase);
    const std::int32_t playerIndex =
        static_cast<std::int32_t>(byteDiff / static_cast<std::ptrdiff_t>(kPerPlayerAudioSettingsStride));
    i32(playerSettings, 0x00) = playerIndex;
    for (std::size_t offset = 0x04; offset <= 0x30; offset += 4) {
        f32(playerSettings, offset) = 1.0f;
    }
    for (std::size_t i = 0; i < 6u; ++i) {
        const std::size_t nodeBase = (i << 4u) + 0x4Cu;
        void* sentinel = bytes(playerSettings) + nodeBase;
        *reinterpret_cast<void**>(bytes(playerSettings) + nodeBase) = sentinel;
        *reinterpret_cast<void**>(bytes(playerSettings) + nodeBase + 4u) = sentinel;
        u32(playerSettings, nodeBase + 8u) = 0u;
    }
    f32(playerSettings, 0xAC) = 1.0f;
    f32(playerSettings, 0xB0) = 1.0f;
    f32(playerSettings, 0xB4) = 1.0f;
    f32(playerSettings, 0xB8) = 1.0f;
    f32(playerSettings, 0xBC) = 1.0f;
    f32(playerSettings, 0xC0) = 12.266923904418945f;
    f32(playerSettings, 0xC4) = 0.0f;
    f32(playerSettings, 0xC8) = 0.0f;
    f32(playerSettings, 0xCC) = 0.0f;
    u8(playerSettings, 0xD0) = 0u;
    f32(playerSettings, 0xD4) = 1.0f;
    f32(playerSettings, 0xD8) = 1.0f;
    f32(playerSettings, 0xDC) = 12.266923904418945f;
    f32(playerSettings, 0xE0) = 0.0f;
    f32(playerSettings, 0xE4) = 0.0f;
    f32(playerSettings, 0xE8) = 0.0f;
    u8(playerSettings, 0xEC) = 0u;

    for (std::size_t i = 0; i < 6u; ++i) {
        f32(playerSettings, 0x34u + i * 4u) = 1.0f;
    }
    std::memset(bytes(playerSettings) + 0x1B0, 0, 0x30);
    for (std::uint32_t category = 0; category < 6u; ++category) {
        for (std::uint32_t sub = 0; sub < 2u; ++sub) {
            recomputePlayerCategoryFinalVolume(playerSettings, category, sub,
                                               globalAudioSettings,
                                               globalCategoryScaleTable);
        }
    }
    std::memset(bytes(playerSettings) + 0x1B0, 0, 0x30);
    u8(playerSettings, 0x1E0) = 0u;
    f32(playerSettings, 0x1EC) = 0.0f;
    f32(playerSettings, 0x1E8) = 0.0f;
    f32(playerSettings, 0x1E4) = 0.0f;
    f32(playerSettings, 0x1F8) = 0.0f;
    f32(playerSettings, 0x1F4) = 0.0f;
    f32(playerSettings, 0x1F0) = 0.0f;
    return playerSettings;
}

// 0x0050AD80: 00BDF5A8 slot 30 (SageAudioManager::isObjectOrDrawablePlayingAudio).
std::uint32_t isObjectOrDrawablePlayingAudio(const void* manager,
                                             std::uint32_t drawableId,
                                             void* gameClientLookup,
                                             std::uint32_t initialEax) noexcept {
    if (drawableId == 0u) {
        return initialEax & 0xFFFFFF00u;
    }
    const void* drawable = invokeVirtualSlotLookup(gameClientLookup, 0x20, drawableId);
    std::uint32_t objectId = 0u;
    std::uint32_t callEax = 0u;
    if (drawable != nullptr) {
        const void* object =
            *reinterpret_cast<const void* const*>(bytes(drawable) + 0x9C);
        callEax = static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(object));
        if (object != nullptr) {
            objectId = u32(object, 0x10);
        }
    }
    return hasActiveEventForObjectOrDrawable(manager, objectId, drawableId, callEax);
}

// 0x0050BE70: SageAudioCache::totalEntryCount.
std::uint32_t sageAudioCacheTotalEntryCount(const void* audioCache) noexcept {
    const std::uint32_t buckets = u32(audioCache, 0x9C) + 1u;
    const std::uint32_t itemsA = u32(audioCache, 0xAC);
    const std::uint32_t itemsB = u32(audioCache, 0xC8);
    return buckets * (itemsA + itemsB);
}

// 0x0050DE20: PerPlayerAudioSettings::removeEventVolumeModifiers.
std::uint32_t removePlayerEventVolumeModifiers(
    void* playerSettings, void* const* eventInfoPtr, std::uint32_t sourcePlayerIndex,
    std::int32_t globalModifierLockCount, const void* globalAudioSettings,
    const float* globalCategoryScaleTable) noexcept {
    const void* eventInfo = *eventInfoPtr;
    std::uint32_t eax =
        static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(eventInfo));
    const std::uint32_t modifierCount = u32(eventInfo, 0xD0);
    if (modifierCount == 0u) {
        return eax;
    }

    const std::uint32_t targetPlayer = u32(playerSettings, 0x00);
    for (std::uint32_t idx = 0; idx < modifierCount; ++idx) {
        const void* curEventInfo = *eventInfoPtr;
        const std::byte* table =
            *reinterpret_cast<const std::byte* const*>(bytes(curEventInfo) + 0xD4);
        const std::byte* entry = table + idx * 8u;
        const float modifierVal = *reinterpret_cast<const float*>(entry + 4);
        const std::uint32_t category = *reinterpret_cast<const std::uint32_t*>(entry);
        eax = category;
        if (category == 4u && sourcePlayerIndex != targetPlayer) {
            eax = idx + 1u;
            continue;
        }

        const std::size_t listOffset = (static_cast<std::size_t>(category) << 4u) + 0x4Cu;
        std::byte* sentinel = bytes(playerSettings) + listOffset;
        std::byte* cur = *reinterpret_cast<std::byte**>(sentinel);
        while (cur != sentinel) {
            const float curVal = *reinterpret_cast<const float*>(cur + 8);
            if (!floatNotEqualOrUnordered(curVal, modifierVal)) {
                break;
            }
            cur = *reinterpret_cast<std::byte**>(cur);
        }
        if (cur != sentinel) {
            std::byte* next = *reinterpret_cast<std::byte**>(cur);
            std::byte* prev = *reinterpret_cast<std::byte**>(cur + 4);
            *reinterpret_cast<std::byte**>(prev) = next;
            *reinterpret_cast<std::byte**>(next + 4) = prev;
            u32(playerSettings, (static_cast<std::size_t>(category) << 4u) + 0x54u) -= 1u;
        }
        recomputePlayerCategoryModifierProduct(
            playerSettings, category, globalModifierLockCount, globalAudioSettings);
        recomputePlayerCategoryFinalVolume(playerSettings, category, 0u,
                                           globalAudioSettings,
                                           globalCategoryScaleTable);
        recomputePlayerCategoryFinalVolume(playerSettings, category, 1u,
                                           globalAudioSettings,
                                           globalCategoryScaleTable);
        eax = idx + 1u;
    }
    return eax;
}

// 0x0050DF80: SageAudioCache::removeListenerEntry.
std::uint32_t removeAudioCacheListenerEntry(void* audioCache,
                                            std::uint32_t listenerKey) noexcept {
    std::byte* sentinel = bytes(audioCache) + 0x208;
    std::byte* head = *reinterpret_cast<std::byte**>(sentinel);
    std::byte* cur = head;
    while (cur != sentinel) {
        if (*reinterpret_cast<const std::uint32_t*>(cur + 8) == listenerKey) {
            break;
        }
        cur = *reinterpret_cast<std::byte**>(cur);
    }
    if (cur == sentinel) {
        return static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(head));
    }
    std::byte* next = *reinterpret_cast<std::byte**>(cur);
    std::byte* prev = *reinterpret_cast<std::byte**>(cur + 4);
    *reinterpret_cast<std::byte**>(prev) = next;
    *reinterpret_cast<std::byte**>(next + 4) = prev;
    u32(audioCache, 0x210) -= 1u;
    return static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(prev));
}

// 0x0050E040: PlayingAudio::detachPlayerVolumeModifiers.
std::uint32_t detachPlayingAudioVolumeModifiers(
    void* playingAudio, void* playerSettingsArray3,
    std::int32_t globalModifierLockCount, const void* globalAudioSettings,
    const float* globalCategoryScaleTable) noexcept {
    void* eventRts = *reinterpret_cast<void**>(bytes(playingAudio) + 0x0C);
    std::uint32_t eax =
        static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(eventRts));
    if (eventRts == nullptr || u8(playingAudio, 0x22) == 0u) {
        return eax;
    }
    for (std::size_t i = 0; i < 3u; ++i) {
        void* curEventRts = *reinterpret_cast<void**>(bytes(playingAudio) + 0x0C);
        const std::uint32_t sourcePlayer = u32(curEventRts, 0x6C);
        void* const* eventInfoPtr =
            reinterpret_cast<void* const*>(bytes(curEventRts) + 0x58);
        void* playerSettings =
            bytes(playerSettingsArray3) + i * kPerPlayerAudioSettingsStride;
        eax = removePlayerEventVolumeModifiers(playerSettings, eventInfoPtr,
                                               sourcePlayer, globalModifierLockCount,
                                               globalAudioSettings,
                                               globalCategoryScaleTable);
    }
    u8(playingAudio, 0x22) = 0u;
    return eax;
}

// 0x0050E510: 00BDF5A8 slot 92 & 94 (SageAudioManager::removeAudioListenerEntry).
std::uint32_t sageAudioManagerRemoveListenerEntry(void* manager,
                                                  std::uint32_t listenerKey,
                                                  std::uint32_t initialEax) noexcept {
    void* audioCache = *reinterpret_cast<void**>(bytes(manager) + 0x350);
    if (audioCache != nullptr) {
        return removeAudioCacheListenerEntry(audioCache, listenerKey);
    }
    return initialEax;
}

}  // namespace ra3::audio
