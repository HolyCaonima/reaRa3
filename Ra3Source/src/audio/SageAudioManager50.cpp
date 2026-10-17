#include "ra3/audio/SageAudioManager50.h"

#include <xmmintrin.h>

#include <cstdint>
#include <cstdio>
#include <cstdlib>

namespace ra3::audio {
namespace {

[[noreturn]] void untestedPath(const char* what) noexcept {
    std::fputs(what, stderr);
    ::_exit(2);
}

float loadFloat(std::uintptr_t address) noexcept {
    return *reinterpret_cast<volatile float*>(address);
}

std::uint32_t loadU32(std::uintptr_t address) noexcept {
    return *reinterpret_cast<volatile std::uint32_t*>(address);
}

float mulss(float left, float right) noexcept {
    float out = 0.f;
    const __m128 product = _mm_mul_ss(_mm_load_ss(&left), _mm_load_ss(&right));
    _mm_store_ss(&out, product);
    return out;
}

// Matches ucomiss / lahf / test ah,44h / jnp: true unless the values are ordered and equal.
bool notOrderedEqual(float left, float right) noexcept {
    return _mm_ucomieq_ss(_mm_load_ss(&left), _mm_load_ss(&right)) == 0;
}

float clampToUnit(float value) noexcept {
    __m128 current = _mm_load_ss(&value);
    const __m128 zero = _mm_setzero_ps();
    if (_mm_comigt_ss(zero, current)) {
        current = zero;
    } else {
        const float one = loadFloat(kOneFloatAddress);
        const __m128 limit = _mm_load_ss(&one);
        if (_mm_comigt_ss(current, limit)) {
            current = limit;
        }
    }
    float out = 0.f;
    _mm_store_ss(&out, current);
    return out;
}

std::uint32_t audioSettings() noexcept {
    return loadU32(kAudioSettingsAddress);
}

void decrementIfPositive(std::uint8_t* byte) noexcept {
    const auto value = static_cast<std::int8_t>(*byte);
    if (value > 0) {
        *byte = static_cast<std::uint8_t>(value - 1);
    }
}

void orCategoryBit(std::uint8_t* bitset, std::uint32_t bitIndex) noexcept {
    const auto word = bitIndex >> 5;
    const auto shift = bitIndex & 0x1Fu;
    auto* cell = reinterpret_cast<std::uint32_t*>(bitset + word * 4u);
    *cell |= 1u << shift;
}

// Shipped mask at 00504AC9. The mutation check changes this constant.
constexpr std::uint32_t kCategoryBitMask = 0x1Fu;

}  // namespace

std::uint32_t resolveSpeakerCenterChannel() noexcept {
    const auto mode = loadU32(kSpeakerModeAddress);
    if (mode == 0xFFFFFFFFu) {
        // 00502288: eax is the system pointer; only AL is replaced by the channel byte.
        const auto system = loadU32(kAudioSystemAddress);
        const auto index = *reinterpret_cast<const std::uint32_t*>(system + 0x2E4u);
        const auto channel = *reinterpret_cast<const std::uint32_t*>(system + index * 4u + 0x54u);
        const auto enabled = *reinterpret_cast<const std::uint8_t*>(channel + 0x29u);
        return (system & 0xFFFFFF00u) | enabled;
    }
    if (mode == 0u) {
        return 0u;
    }
    return (mode & 0xFFFFFF00u) | (mode == 1u ? 1u : 0u);
}

std::uint32_t sageAudioManagerEmptyEventAddress() noexcept {
    return kEmptyAudioEventAddress;
}

std::uint32_t sageAudioManagerStopAllImmediately(void* manager) noexcept {
    const auto* vtable = *reinterpret_cast<std::uintptr_t* const*>(manager);
    using Stop = std::uint32_t(__thiscall*)(void*, int);
    return reinterpret_cast<Stop>(vtable[14])(manager, 1);
}

std::uint32_t sageAudioManagerListenerOrientationCount() noexcept {
    return loadU32(kListenerCountAddress);
}

std::uint32_t sageAudioManagerSubsystemNameAddress() noexcept {
    return kSageAudioManagerNameAddress;
}

std::uint32_t sagePathMusicManagerSubsystemNameAddress() noexcept {
    return kSagePathMusicManagerNameAddress;
}

void* initAudioEventHandle(void* handle, void* eventSlot) noexcept {
    // 005044F2 reads the pointer stored at the argument, then writes that pointer.
    auto* eventObject = *reinterpret_cast<void**>(eventSlot);
    *reinterpret_cast<void**>(handle) = eventObject;
    if (eventObject != nullptr) {
        *reinterpret_cast<std::uint32_t*>(static_cast<std::uint8_t*>(eventObject) + 0x98u) += 1u;
    }
    return handle;
}

std::uint32_t releaseAudioEventHandle(void* handle) noexcept {
    auto* object = *reinterpret_cast<std::uint8_t**>(handle);
    if (object == nullptr) {
        return 0u;
    }
    auto& refs = *reinterpret_cast<std::int32_t*>(object + 0x98u);
    refs -= 1;
    if (refs > 0) {
        return reinterpret_cast<std::uint32_t>(object);
    }
    auto* sub = object + 0x94u;
    const auto* vtable = *reinterpret_cast<std::uintptr_t* const*>(sub);
    using Release = std::uint32_t(__thiscall*)(void*, int);
    return reinterpret_cast<Release>(vtable[0])(sub, 1);
}

std::uint32_t releasePlayingAudioHandle(void* handle, std::uint32_t incomingEax) noexcept {
    auto* object = *reinterpret_cast<std::uint8_t**>(handle);
    if (object == nullptr) {
        return incomingEax;
    }
    auto& refs = *reinterpret_cast<std::int32_t*>(object + 4u);
    refs -= 1;
    if (refs > 0) {
        return incomingEax;
    }
    const auto* vtable = *reinterpret_cast<std::uintptr_t* const*>(object);
    using Release = std::uint32_t(__thiscall*)(void*, int);
    return reinterpret_cast<Release>(vtable[0])(object, 1);
}

std::uint32_t unlinkPlayingAudioNode(void* node) noexcept {
    auto* self = static_cast<std::uint8_t*>(node);
    auto* prev = *reinterpret_cast<std::uint8_t**>(self + 4u);
    auto* next = *reinterpret_cast<std::uint8_t**>(self + 8u);
    if (prev != nullptr) {
        *reinterpret_cast<std::uint8_t**>(prev + 8u) = next;
    } else {
        auto* owner = *reinterpret_cast<std::uint8_t**>(self);
        *reinterpret_cast<std::uint8_t**>(owner + 0xC0u) = next;
    }
    if (next != nullptr) {
        *reinterpret_cast<std::uint8_t**>(next + 4u) = prev;
        *reinterpret_cast<std::uint8_t**>(self + 8u) = nullptr;
        *reinterpret_cast<std::uint8_t**>(self + 4u) = nullptr;
        return reinterpret_cast<std::uint32_t>(next);
    }
    auto* owner = *reinterpret_cast<std::uint8_t**>(self);
    *reinterpret_cast<std::uint8_t**>(owner + 0xC4u) = prev;
    *reinterpret_cast<std::uint8_t**>(self + 8u) = nullptr;
    *reinterpret_cast<std::uint8_t**>(self + 4u) = nullptr;
    return reinterpret_cast<std::uint32_t>(owner);
}

std::uint32_t testAudioEventCategoryBit(const void* eventInfo, std::uint32_t bitIndex) noexcept {
    const auto shift = bitIndex & kCategoryBitMask;
    const auto word = bitIndex >> 5;
    const auto mask = 1u << shift;
    const auto bits = *reinterpret_cast<const std::uint32_t*>(
        static_cast<const std::uint8_t*>(eventInfo) + word * 4u + 0x78u);
    return (mask & bits) == 0u ? 0u : 1u;
}

std::uint32_t intersectsAudioEventCategoryMask(const void* eventInfo,
                                               const std::uint32_t* mask) noexcept {
    const auto bits = *reinterpret_cast<const std::uint32_t*>(
        static_cast<const std::uint8_t*>(eventInfo) + 0x78u);
    return (bits & mask[0]) != 0u ? 1u : 0u;
}

void recomputePlayerCategoryVolume(void* player, std::uint32_t category,
                                   std::uint32_t subchannel) noexcept {
    auto* self = static_cast<std::uint8_t*>(player);
    const auto pair = subchannel + category * 2u;
    auto* block = reinterpret_cast<float*>(self + ((pair + 0x0Fu) << 4));
    const float saved[4] = {block[0], block[1], block[2], block[3]};

    if (category == 5u) {
        const float one = loadFloat(kOneFloatAddress);
        block[0] = one;
        block[1] = one;
        block[2] = one;
        block[3] = one;
    } else {
        const float scale = loadFloat(kCategoryScaleTableAddress + category * 4u);
        float value = *reinterpret_cast<float*>(self + pair * 4u + 4u);
        value = mulss(value, scale);
        value = mulss(value, *reinterpret_cast<float*>(self + 0xB0u));
        block[0] = value;

        const auto settings = audioSettings();
        if (subchannel == 1u) {
            value = mulss(*reinterpret_cast<float*>(self + 0xACu), block[0]);
            block[0] = value;
            if (settings != 0u) {
                block[0] = mulss(*reinterpret_cast<float*>(settings + 0x78u), value);
            }
        } else if (settings != 0u) {
            block[0] = mulss(*reinterpret_cast<float*>(settings + 0x74u), block[0]);
        }

        block[1] = mulss(block[0], *reinterpret_cast<float*>(self + 0xB4u));
        const float third =
            mulss(*reinterpret_cast<float*>(self + category * 4u + 0x34u), block[0]);
        block[2] = third;
        block[3] = mulss(third, *reinterpret_cast<float*>(self + 0xB4u));
    }

    const float written[4] = {block[0], block[1], block[2], block[3]};
    for (std::uint32_t component = 0; component < 4u; ++component) {
        if (notOrderedEqual(written[component], saved[component])) {
            self[component + (pair + 0x6Cu) * 4u] = 2u;
        }
    }
}

std::uint32_t recomputePlayerCategoryProduct(void* player, std::uint32_t category) noexcept {
    auto* self = static_cast<std::uint8_t*>(player);
    *reinterpret_cast<float*>(self + category * 4u + 0x34u) = loadFloat(kOneFloatAddress);

    auto* cursor = *reinterpret_cast<std::uint8_t**>(self + category * 16u + 0x4Cu);
    auto* end = *reinterpret_cast<std::uint8_t**>(self + (category + 5u) * 16u);
    while (cursor != end) {
        float product = *reinterpret_cast<float*>(self + category * 4u + 0x34u);
        product = mulss(product, *reinterpret_cast<float*>(cursor));
        cursor += 8;
        *reinterpret_cast<float*>(self + category * 4u + 0x34u) = product;
    }
    std::uint32_t result = reinterpret_cast<std::uint32_t>(end);

    const auto lock = static_cast<std::int32_t>(loadU32(kVolumeLockAddress));
    if (lock > 0) {
        const auto settings = audioSettings();
        result = settings;
        if (settings != 0u) {
            auto* row = *reinterpret_cast<std::uint8_t**>(settings + 0xD4u);
            for (;;) {
                const auto count = *reinterpret_cast<std::uint32_t*>(settings + 0xD0u);
                const auto* base = *reinterpret_cast<std::uint8_t**>(settings + 0xD4u);
                if (row == base + count * 8u) {
                    break;
                }
                if (*reinterpret_cast<std::uint32_t*>(row) == category) {
                    float product = *reinterpret_cast<float*>(self + category * 4u + 0x34u);
                    product = mulss(*reinterpret_cast<float*>(row + 4u), product);
                    *reinterpret_cast<float*>(self + category * 4u + 0x34u) = product;
                }
                row += 8;
            }
        }
    }

    const float clamped = clampToUnit(*reinterpret_cast<float*>(self + category * 4u + 0x34u));
    *reinterpret_cast<float*>(self + category * 4u + 0x34u) = clamped;
    return result;
}

std::uint32_t decrementPlayerVolumeCounters(void* player) noexcept {
    auto* cursor = static_cast<std::uint8_t*>(player) + 0x1B2u;
    for (int group = 0; group < 6; ++group) {
        for (int step = 0; step < 2; ++step) {
            decrementIfPositive(cursor - 2);
            decrementIfPositive(cursor - 1);
            decrementIfPositive(cursor);
            decrementIfPositive(cursor + 1);
            cursor += 4;
        }
    }
    return reinterpret_cast<std::uint32_t>(cursor);
}

std::uint32_t acquireVolumeModifierLock(const void* modifierGuard) noexcept {
    const auto* guard = static_cast<const std::uint8_t*>(modifierGuard);
    if (guard != nullptr && *reinterpret_cast<const std::uint32_t*>(guard + 0x18u) != 0u) {
        return reinterpret_cast<std::uint32_t>(guard);
    }
    auto& lock = *reinterpret_cast<std::uint32_t*>(kVolumeLockAddress);
    lock += 1u;
    if (lock != 1u) {
        return lock;
    }
    for (int player = 0; player < 3; ++player) {
        auto* self = reinterpret_cast<std::uint8_t*>(kPlayerAudioBaseAddress +
                                                     static_cast<std::uint32_t>(player) *
                                                         kPlayerAudioStride);
        for (std::uint32_t category = 0; category < 6u; ++category) {
            recomputePlayerCategoryProduct(self, category);
            recomputePlayerCategoryVolume(self, category, 0u);
            recomputePlayerCategoryVolume(self, category, 1u);
        }
    }
    return 0u;
}

std::uint32_t releaseVolumeModifierLock(const void* modifierGuard) noexcept {
    const auto* guard = static_cast<const std::uint8_t*>(modifierGuard);
    if (guard != nullptr && *reinterpret_cast<const std::uint32_t*>(guard + 0x18u) != 0u) {
        return reinterpret_cast<std::uint32_t>(guard);
    }
    auto& lock = *reinterpret_cast<std::int32_t*>(kVolumeLockAddress);
    lock -= 1;
    if (lock != 0) {
        return reinterpret_cast<std::uint32_t>(guard);
    }
    for (int player = 0; player < 3; ++player) {
        auto* self = reinterpret_cast<std::uint8_t*>(kPlayerAudioBaseAddress +
                                                     static_cast<std::uint32_t>(player) *
                                                         kPlayerAudioStride);
        for (std::uint32_t category = 0; category < 6u; ++category) {
            recomputePlayerCategoryProduct(self, category);
            recomputePlayerCategoryVolume(self, category, 0u);
            recomputePlayerCategoryVolume(self, category, 1u);
        }
    }
    return 0u;
}

std::uint32_t sageAudioManagerOnAcquireVolumeModifierLock(const void* modifierGuard) noexcept {
    return acquireVolumeModifierLock(modifierGuard);
}

std::uint32_t sageAudioManagerOnReleaseVolumeModifierLock(const void* modifierGuard) noexcept {
    return releaseVolumeModifierLock(modifierGuard);
}

std::uint32_t sageAudioManagerQuerySuppressed(void* request, std::uint32_t incomingEax) noexcept {
    auto* req = static_cast<std::uint8_t*>(request);
    if (*reinterpret_cast<std::uint32_t*>(req + 0x30u) != 0u) {
        return incomingEax & 0xFFFFFF00u;
    }
    const auto inner = *reinterpret_cast<std::uint32_t*>(req + 0x0Cu);
    const auto bits = *reinterpret_cast<std::uint32_t*>(inner + 0x74u);
    if (((bits >> 2) & 1u) == 0u) {
        return inner & 0xFFFFFF00u;
    }
    untestedPath("00506430 continued into 0055C680\n");
}

void* initCategoryBitsFour(void* bitset, std::uint32_t bit0, std::uint32_t bit1,
                           std::uint32_t bit2, std::uint32_t bit3) noexcept {
    auto* bytes = static_cast<std::uint8_t*>(bitset);
    *reinterpret_cast<std::uint32_t*>(bytes) = 0u;
    orCategoryBit(bytes, bit0);
    orCategoryBit(bytes, bit1);
    orCategoryBit(bytes, bit2);
    orCategoryBit(bytes, bit3);
    return bitset;
}

void* initCategoryBitsTwo(void* bitset, std::uint32_t bit0, std::uint32_t bit1) noexcept {
    auto* bytes = static_cast<std::uint8_t*>(bitset);
    *reinterpret_cast<std::uint32_t*>(bytes) = 0u;
    orCategoryBit(bytes, bit0);
    orCategoryBit(bytes, bit1);
    return bitset;
}

std::uint32_t bindSoundProvider(void* provider, const void* name) noexcept {
    auto* self = static_cast<std::uint8_t*>(provider);
    for (int slot = 0; slot < 2; ++slot) {
        if (*reinterpret_cast<std::uint32_t*>(self + 4u + static_cast<std::uint32_t>(slot) * 8u) ==
            0u) {
            return 1u;
        }
    }
    if (*reinterpret_cast<std::uint32_t*>(self) == 0u || name == nullptr) {
        return 1u;
    }
    untestedPath("00502540 continued into 00B76D80\n");
}

std::uint32_t hasActiveRenderCursor(const void* playingAudio) noexcept {
    const auto object = *reinterpret_cast<const std::uint32_t*>(playingAudio);
    if (object == 0u) {
        return 0u;
    }
    const auto inner = *reinterpret_cast<const std::uint32_t*>(object + 0x10u);
    const auto cursor = *reinterpret_cast<const std::uint32_t*>(inner + 0x20u);
    if (cursor != 0u) {
        return (cursor & 0xFFFFFF00u) | 1u;
    }
    const auto fallback = *reinterpret_cast<const std::uint32_t*>(object + 0x1Cu);
    return (fallback & 0xFFFFFF00u) | (fallback != 0u ? 1u : 0u);
}

void* appendPlayingAudioNode(void* node, void* owner) noexcept {
    auto* self = static_cast<std::uint8_t*>(node);
    *reinterpret_cast<void**>(self) = owner;
    if (owner == nullptr) {
        *reinterpret_cast<void**>(self + 8u) = nullptr;
        *reinterpret_cast<void**>(self + 4u) = nullptr;
        return node;
    }
    auto* prev = *reinterpret_cast<std::uint8_t**>(static_cast<std::uint8_t*>(owner) + 0xC4u);
    *reinterpret_cast<std::uint8_t**>(self + 4u) = prev;
    *reinterpret_cast<std::uint8_t**>(self + 8u) = nullptr;
    *reinterpret_cast<void**>(static_cast<std::uint8_t*>(owner) + 0xC4u) = node;
    if (prev != nullptr) {
        *reinterpret_cast<void**>(prev + 8u) = node;
    } else {
        *reinterpret_cast<void**>(static_cast<std::uint8_t*>(owner) + 0xC0u) = node;
    }
    return node;
}

std::uint32_t sagePathMusicManagerTransferState(void* manager, void* stream) noexcept {
    const auto field = *reinterpret_cast<std::uint32_t*>(static_cast<std::uint8_t*>(stream) + 4u);
    const auto shifted = field >> 2;
    if ((shifted & 1u) != 0u) {
        return shifted;
    }
    std::uint32_t slot = reinterpret_cast<std::uint32_t>(stream);
    auto* slotBytes = reinterpret_cast<std::uint8_t*>(&slot);
    slotBytes[0] = 1u;
    slotBytes[1] = 2u;
    const auto* vtable = *reinterpret_cast<std::uintptr_t* const*>(stream);
    using Virtual = std::uint32_t(__thiscall*)(void*, void*);
    reinterpret_cast<Virtual>(vtable[6])(stream, &slot);
    const auto result = reinterpret_cast<Virtual>(vtable[27])(
        stream, static_cast<std::uint8_t*>(manager) + 0x4Cu);
    if (slotBytes[1] <= 1u) {
        untestedPath("0050B090 continued into 00509710\n");
    }
    return result;
}

std::uint32_t eraseListenerRecord(void* cache, std::uint32_t key) noexcept {
    auto* self = static_cast<std::uint8_t*>(cache);
    auto* begin = *reinterpret_cast<std::uint8_t**>(self + 0x60u);
    auto* end = *reinterpret_cast<std::uint8_t**>(self + 0x64u);
    if (begin == end) {
        return reinterpret_cast<std::uint32_t>(begin);
    }
    for (auto* cursor = begin; cursor != end; cursor += 8) {
        if (*reinterpret_cast<std::uint32_t*>(cursor) != key) {
            continue;
        }
        auto* source = cursor + 8;
        auto* dest = cursor;
        while (source != end) {
            *reinterpret_cast<std::uint32_t*>(dest) = *reinterpret_cast<std::uint32_t*>(source);
            *reinterpret_cast<std::uint32_t*>(dest + 4u) =
                *reinterpret_cast<std::uint32_t*>(source + 4u);
            source += 8;
            dest += 8;
        }
        *reinterpret_cast<std::uint8_t**>(self + 0x64u) = end - 8;
        self[0x81u] = 1u;
        return reinterpret_cast<std::uint32_t>(end);
    }
    return reinterpret_cast<std::uint32_t>(end);
}

std::uint32_t sageAudioManagerRemoveListenerEntry(void* manager, std::uint32_t key,
                                                  std::uint32_t incomingEax) noexcept {
    auto* cache = *reinterpret_cast<std::uint8_t**>(static_cast<std::uint8_t*>(manager) + 0x40u);
    if (cache == nullptr) {
        return incomingEax;
    }
    return eraseListenerRecord(cache, key);
}

std::uint32_t applyListedVolumeModifiers(void* player, void* listHolder,
                                         std::uint32_t playerIndex) noexcept {
    auto* object = *reinterpret_cast<std::uint8_t**>(listHolder);
    auto* begin = *reinterpret_cast<std::uint8_t**>(object + 0xF0u);
    auto* end = *reinterpret_cast<std::uint8_t**>(object + 0xF4u);
    if (begin == end) {
        return reinterpret_cast<std::uint32_t>(end);
    }
    for (auto* cursor = begin; cursor != end; cursor += 8) {
        const auto category = *reinterpret_cast<std::uint32_t*>(cursor);
        if (category == 0xFFFFFFFFu) {
            continue;
        }
        const auto sample = *reinterpret_cast<std::uint32_t*>(cursor + 4u);
        auto* vector = static_cast<std::uint8_t*>(player) + 0x4Cu + category * 16u;
        auto* destEnd = *reinterpret_cast<std::uint8_t**>(vector + 4u);
        auto* destCap = *reinterpret_cast<std::uint8_t**>(vector + 8u);
        if (destEnd >= destCap) {
            untestedPath("0050FA80 continued into 007F17C0\n");
        }
        *reinterpret_cast<std::uint8_t**>(vector + 4u) = destEnd + 8;
        if (destEnd != nullptr) {
            *reinterpret_cast<std::uint32_t*>(destEnd) = sample;
            *reinterpret_cast<std::uint32_t*>(destEnd + 4u) = playerIndex;
        }
        recomputePlayerCategoryProduct(player, category);
        recomputePlayerCategoryVolume(player, category, 0u);
        recomputePlayerCategoryVolume(player, category, 1u);
    }
    return reinterpret_cast<std::uint32_t>(end);
}

std::uint32_t refreshRenderingPlayerVolumes(void* rendering) noexcept {
    auto* self = static_cast<std::uint8_t*>(rendering);
    auto* sub = *reinterpret_cast<std::uint8_t**>(self + 4u);
    const auto index = *reinterpret_cast<std::uint32_t*>(sub + 0x30u);
    auto* player = reinterpret_cast<std::uint8_t*>(kPlayerAudioBaseAddress + index * kPlayerAudioStride);
    auto result = applyListedVolumeModifiers(player, sub + 0x0Cu, index);
    if (index == 2u) {
        for (std::uint32_t slot = kPlayerAudioBaseAddress, which = 0; slot < 0x00DEE884u;
             slot += kPlayerAudioStride, ++which) {
            if (which == 2u) {
                continue;
            }
            sub = *reinterpret_cast<std::uint8_t**>(self + 4u);
            result = applyListedVolumeModifiers(reinterpret_cast<void*>(slot), sub + 0x0Cu, 2u);
        }
    }
    return result;
}

}  // namespace ra3::audio
