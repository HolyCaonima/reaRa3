#include "../Targets.h"

#include <ra3/audio/AudioEvent.h>

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>

namespace ra3hook {
namespace {

constexpr auto relaxed = std::memory_order_relaxed;

TargetState setPlayerState;
TargetState decrementDelayState;
TargetState setPositionState;
TargetState setObjectState;
TargetState setDrawableState;
TargetState setFadeState;
TargetState shouldFadeState;
TargetState setMinimumState;
TargetState setMaximumState;
TargetState setVolumeState;
TargetState serialState;
TargetState isOnState;
TargetState setOnState;
TargetState requestEventState;
TargetState playingHandleState;

template <std::size_t Offset, std::size_t Size, typename Apply, typename Original>
void shadowEventWrite(const char* key, TargetState& state, void* object,
                      Apply apply, Original original)
{
    state.calls.fetch_add(1, relaxed);
    std::array<std::byte, ra3::audio::AudioEventView::minimumSize> copy{};
    std::memcpy(copy.data(), object, copy.size());
    apply(copy.data());
    if (state.mode == Mode::replace) {
        std::memcpy(static_cast<std::byte*>(object) + Offset, copy.data() + Offset, Size);
        return;
    }
    original();
    recordOutput(key, state, static_cast<const std::byte*>(object) + Offset,
                 copy.data() + Offset, Size);
}

void __fastcall setPlayerHook(void* object, void*, std::uint32_t value)
{
    shadowEventWrite<0x30, 4>(
        "audioeventsetplayer", setPlayerState, object,
        [value](void* copy) { ra3::audio::AudioEventView(copy).setPlayerIndex(value); },
        [&] {
            reinterpret_cast<void(__thiscall*)(void*, std::uint32_t)>(
                setPlayerState.original)(object, value);
        });
}

void __fastcall decrementDelayHook(void* object, void*, float amount)
{
    shadowEventWrite<0x68, 4>(
        "audioeventdecrementdelay", decrementDelayState, object,
        [amount](void* copy) { ra3::audio::AudioEventView(copy).decrementDelay(amount); },
        [&] {
            reinterpret_cast<void(__thiscall*)(void*, float)>(
                decrementDelayState.original)(object, amount);
        });
}

void __fastcall setPositionHook(void* object, void*, const ra3::audio::Vector3* position)
{
    shadowEventWrite<0x38, 0x11>(
        "audioeventsetposition", setPositionState, object,
        [position](void* copy) { ra3::audio::AudioEventView(copy).setPosition(position); },
        [&] {
            reinterpret_cast<void(__thiscall*)(void*, const ra3::audio::Vector3*)>(
                setPositionState.original)(object, position);
        });
}

void __fastcall setObjectHook(void* object, void*, std::uint32_t value)
{
    shadowEventWrite<0x34, 8>(
        "audioeventsetobject", setObjectState, object,
        [value](void* copy) { ra3::audio::AudioEventView(copy).setObjectId(value); },
        [&] {
            reinterpret_cast<void(__thiscall*)(void*, std::uint32_t)>(
                setObjectState.original)(object, value);
        });
}

void __fastcall setDrawableHook(void* object, void*, std::uint32_t value)
{
    shadowEventWrite<0x34, 8>(
        "audioeventsetdrawable", setDrawableState, object,
        [value](void* copy) { ra3::audio::AudioEventView(copy).setDrawableId(value); },
        [&] {
            reinterpret_cast<void(__thiscall*)(void*, std::uint32_t)>(
                setDrawableState.original)(object, value);
        });
}

void __fastcall setFadeHook(void* object, void*, bool value)
{
    shadowEventWrite<0x49, 1>(
        "audioeventsetfade", setFadeState, object,
        [value](void* copy) { ra3::audio::AudioEventView(copy).setShouldFade(value); },
        [&] {
            reinterpret_cast<void(__thiscall*)(void*, bool)>(
                setFadeState.original)(object, value);
        });
}

bool __fastcall shouldFadeHook(const void* object, void*)
{
    shouldFadeState.calls.fetch_add(1, relaxed);
    const bool ours = ra3::audio::AudioEventView(object).shouldFade();
    if (shouldFadeState.mode == Mode::replace) return ours;
    const bool game = reinterpret_cast<bool(__thiscall*)(const void*)>(
        shouldFadeState.original)(object);
    recordScalar("audioeventshouldfade", shouldFadeState,
                 static_cast<std::uint32_t>(game), static_cast<std::uint32_t>(ours));
    return game;
}

template <std::size_t Offset>
void shadowEventFloat(const char* key, TargetState& state, void* object, float value,
                      void (ra3::audio::AudioEventView::*method)(float) noexcept)
{
    shadowEventWrite<Offset, 4>(
        key, state, object,
        [value, method](void* copy) { (ra3::audio::AudioEventView(copy).*method)(value); },
        [&] {
            reinterpret_cast<void(__thiscall*)(void*, float)>(state.original)(object, value);
        });
}

void __fastcall setMinimumHook(void* object, void*, float value)
{
    shadowEventFloat<0x24>(
        "audioeventsetminimum", setMinimumState, object, value,
        &ra3::audio::AudioEventView::setMinimumRangeOverride);
}

void __fastcall setMaximumHook(void* object, void*, float value)
{
    shadowEventFloat<0x28>(
        "audioeventsetmaximum", setMaximumState, object, value,
        &ra3::audio::AudioEventView::setMaximumRangeOverride);
}

void __fastcall setVolumeHook(void* object, void*, float value)
{
    shadowEventFloat<0x2C>(
        "audioeventsetvolume", setVolumeState, object, value,
        &ra3::audio::AudioEventView::setVolume);
}

void __fastcall serialHook(void* object, void*)
{
    serialState.calls.fetch_add(1, relaxed);
    std::uint32_t ours{};
    std::memcpy(&ours, static_cast<const std::byte*>(object) + 0x140, sizeof(ours));
    ++ours;
    if (serialState.mode == Mode::replace) {
        std::memcpy(static_cast<std::byte*>(object) + 0x140, &ours, sizeof(ours));
        return;
    }
    reinterpret_cast<void(__thiscall*)(void*)>(serialState.original)(object);
    recordOutput("audiomanagerserial", serialState,
                 static_cast<const std::byte*>(object) + 0x140, &ours, sizeof(ours));
}

bool __fastcall isOnHook(const void* object, void*, std::uint8_t affect)
{
    isOnState.calls.fetch_add(1, relaxed);
    const bool ours = ra3::audio::SageAudioManagerView(object).isOn(affect);
    if (isOnState.mode == Mode::replace) return ours;
    const bool game = reinterpret_cast<bool(__thiscall*)(const void*, std::uint8_t)>(
        isOnState.original)(object, affect);
    recordScalar("audiomanagerison", isOnState,
                 static_cast<std::uint32_t>(game), static_cast<std::uint32_t>(ours));
    return game;
}

void __fastcall setOnHook(void* object, void*, bool enabled, std::uint8_t affect)
{
    setOnState.calls.fetch_add(1, relaxed);
    std::array<std::byte, ra3::audio::SageAudioManagerView::minimumSize> copy{};
    std::memcpy(copy.data(), object, copy.size());
    ra3::audio::SageAudioManagerView(copy.data()).setOn(enabled, affect);
    if (setOnState.mode == Mode::replace) {
        std::memcpy(static_cast<std::byte*>(object) + 0x136, copy.data() + 0x136, 5);
        return;
    }
    reinterpret_cast<void(__thiscall*)(void*, bool, std::uint8_t)>(
        setOnState.original)(object, enabled, affect);
    recordOutput("audiomanagerseton", setOnState,
                 static_cast<const std::byte*>(object) + 0x136, copy.data() + 0x136, 5);
}

bool __fastcall requestEventHook(const void* self, void*, const void* request)
{
    requestEventState.calls.fetch_add(1, relaxed);
    const bool ours = ra3::audio::requestUsesAudioEvent(request);
    if (requestEventState.mode == Mode::replace) return ours;
    const bool game = reinterpret_cast<bool(__thiscall*)(const void*, const void*)>(
        requestEventState.original)(self, request);
    recordScalar("audiorequestusesevent", requestEventState,
                 static_cast<std::uint32_t>(game), static_cast<std::uint32_t>(ours));
    return game;
}

std::uint32_t __fastcall playingHandleHook(void* object, void*)
{
    playingHandleState.calls.fetch_add(1, relaxed);
    std::uint32_t ours{};
    std::memcpy(&ours, static_cast<const std::byte*>(object) + 0xB4, sizeof(ours));
    if (playingHandleState.mode == Mode::replace) {
        const auto next = ours + 1u;
        std::memcpy(static_cast<std::byte*>(object) + 0xB4, &next, sizeof(next));
        return ours;
    }
    const auto game = reinterpret_cast<std::uint32_t(__thiscall*)(void*)>(
        playingHandleState.original)(object);
    const auto expected = ours + 1u;
    if (game == ours) {
        recordOutput("audiomanagerplayinghandle", playingHandleState,
                     static_cast<const std::byte*>(object) + 0xB4,
                     &expected, sizeof(expected));
    } else {
        recordScalar("audiomanagerplayinghandle", playingHandleState, game, ours);
    }
    return game;
}

}  // namespace

void appendAudioTargets(std::vector<Target>& table)
{
    table.push_back({"audioeventsetplayer", 0x00159C10,
        {0x8B, 0x44, 0x24, 0x04, 0x3B, 0x41, 0x30, 0x74, 0x03, 0x89, 0x41, 0x30, 0xC2, 0x04, 0x00, 0xCC},
        7, reinterpret_cast<void*>(&setPlayerHook), &setPlayerState, true});
    table.push_back({"audioeventdecrementdelay", 0x00159C30,
        {0xF3, 0x0F, 0x10, 0x41, 0x68, 0xF3, 0x0F, 0x5C, 0x44, 0x24, 0x04, 0xF3, 0x0F, 0x11, 0x41, 0x68},
        5, reinterpret_cast<void*>(&decrementDelayHook), &decrementDelayState, true});
    table.push_back({"audioeventsetposition", 0x00159C50,
        {0x8B, 0x44, 0x24, 0x04, 0x85, 0xC0, 0x74, 0x28, 0x8B, 0x51, 0x38, 0x85, 0xD2, 0x74, 0x05, 0x83},
        6, reinterpret_cast<void*>(&setPositionHook), &setPositionState, true});
    table.push_back({"audioeventsetobject", 0x00159C90,
        {0x8B, 0x41, 0x38, 0x83, 0xF8, 0x02, 0x74, 0x05, 0x83, 0xF8, 0x03, 0x75, 0x0E, 0x8B, 0x44, 0x24},
        6, reinterpret_cast<void*>(&setObjectHook), &setObjectState, true});
    table.push_back({"audioeventsetdrawable", 0x00159CC0,
        {0x8B, 0x41, 0x38, 0x83, 0xF8, 0x01, 0x74, 0x05, 0x83, 0xF8, 0x03, 0x75, 0x0E, 0x8B, 0x44, 0x24},
        6, reinterpret_cast<void*>(&setDrawableHook), &setDrawableState, true});
    table.push_back({"audioeventsetfade", 0x00159CF0,
        {0x8A, 0x44, 0x24, 0x04, 0x88, 0x41, 0x49, 0xC2, 0x04, 0x00, 0xCC, 0xCC, 0xCC, 0xCC, 0xCC, 0xCC},
        7, reinterpret_cast<void*>(&setFadeHook), &setFadeState, true});
    table.push_back({"audioeventshouldfade", 0x00159D00,
        {0x8A, 0x41, 0x49, 0xC3, 0xCC, 0xCC, 0xCC, 0xCC, 0xCC, 0xCC, 0xCC, 0xCC, 0xCC, 0xCC, 0xCC, 0xCC},
        5, reinterpret_cast<void*>(&shouldFadeHook), &shouldFadeState, true});
    table.push_back({"audioeventsetminimum", 0x00159D10,
        {0xF3, 0x0F, 0x10, 0x44, 0x24, 0x04, 0xF3, 0x0F, 0x11, 0x41, 0x24, 0xC2, 0x04, 0x00, 0xCC, 0xCC},
        6, reinterpret_cast<void*>(&setMinimumHook), &setMinimumState, true});
    table.push_back({"audioeventsetmaximum", 0x00159D20,
        {0xF3, 0x0F, 0x10, 0x44, 0x24, 0x04, 0xF3, 0x0F, 0x11, 0x41, 0x28, 0xC2, 0x04, 0x00, 0xCC, 0xCC},
        6, reinterpret_cast<void*>(&setMaximumHook), &setMaximumState, true});
    table.push_back({"audioeventsetvolume", 0x00159D30,
        {0xF3, 0x0F, 0x10, 0x44, 0x24, 0x04, 0xF3, 0x0F, 0x11, 0x41, 0x2C, 0xC2, 0x04, 0x00, 0xCC, 0xCC},
        6, reinterpret_cast<void*>(&setVolumeHook), &setVolumeState, true});
    table.push_back({"audiomanagerserial", 0x00103170,
        {0x83, 0x81, 0x40, 0x01, 0x00, 0x00, 0x01, 0xC3, 0xCC, 0xCC, 0xCC, 0xCC, 0xCC, 0xCC, 0xCC, 0xCC},
        7, reinterpret_cast<void*>(&serialHook), &serialState, true});
    table.push_back({"audiomanagerison", 0x00103180,
        {0x8A, 0x44, 0x24, 0x04, 0xA8, 0x01, 0x74, 0x09, 0x8A, 0x81, 0x39, 0x01, 0x00, 0x00, 0xC2, 0x04},
        6, reinterpret_cast<void*>(&isOnHook), &isOnState, true});
    table.push_back({"audiomanagerseton", 0x001031D0,
        {0x8A, 0x54, 0x24, 0x08, 0xF6, 0xC2, 0x01, 0x8A, 0x44, 0x24, 0x04, 0x74, 0x06, 0x88, 0x81, 0x39},
        7, reinterpret_cast<void*>(&setOnHook), &setOnState, true});
    table.push_back({"audiorequestusesevent", 0x00106410,
        {0x8B, 0x44, 0x24, 0x04, 0x85, 0xC0, 0x75, 0x05, 0x32, 0xC0, 0xC2, 0x04, 0x00, 0x33, 0xC9, 0x83},
        6, reinterpret_cast<void*>(&requestEventHook), &requestEventState, true});
    table.push_back({"audiomanagerplayinghandle", 0x00106490,
        {0x8B, 0x81, 0xB4, 0x00, 0x00, 0x00, 0x8D, 0x50, 0x01, 0x89, 0x91, 0xB4, 0x00, 0x00, 0x00, 0xC3},
        6, reinterpret_cast<void*>(&playingHandleHook), &playingHandleState, true});
}

}  // namespace ra3hook
