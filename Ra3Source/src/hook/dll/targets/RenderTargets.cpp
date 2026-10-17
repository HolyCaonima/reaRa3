#include "../Targets.h"

#include <ra3/render/W3DView.h>

#include <array>
#include <atomic>
#include <cstdint>
#include <cstring>

namespace ra3hook {
namespace {

constexpr auto relaxed = std::memory_order_relaxed;

TargetState wireState;
TargetState clearFilterState;
TargetState filterState;
TargetState finishedState;
TargetState lockEqualsState;
TargetState clearShakeState;
TargetState shakeActiveState;
TargetState shakeFlag0State;
TargetState shakeFlag1State;
TargetState shakeFlag2State;
TargetState shakeFlag3State;

template <std::size_t Offset>
void runByteSetter(const char* key, TargetState& state, void* self, std::uint8_t value,
                   void (*port)(void*, std::uint8_t) noexcept)
{
    state.calls.fetch_add(1, relaxed);
    if (state.mode == Mode::replace) {
        port(self, value);
        return;
    }
    using Original = void(__thiscall*)(void*, std::uint8_t);
    reinterpret_cast<Original>(state.original)(self, value);
    std::array<std::byte, Offset + 1> copy{};
    port(copy.data(), value);
    const auto game = *(static_cast<const std::uint8_t*>(self) + Offset);
    const auto ours = static_cast<std::uint8_t>(copy[Offset]);
    recordScalar(key, state, game, ours);
}

void __fastcall wireWrapper(void* self, void*, std::uint8_t value)
{
    runByteSetter<0x351>("w3dviewwirepending", wireState, self, value,
                         ra3::render::setWireframePending);
}
void __fastcall shakeFlag0Wrapper(void* self, void*, std::uint8_t value)
{
    runByteSetter<0x2737>("w3dviewshakeflag0", shakeFlag0State, self, value,
                          ra3::render::setCameraShakeFlag0);
}
void __fastcall shakeFlag1Wrapper(void* self, void*, std::uint8_t value)
{
    runByteSetter<0x2736>("w3dviewshakeflag1", shakeFlag1State, self, value,
                          ra3::render::setCameraShakeFlag1);
}
void __fastcall shakeFlag2Wrapper(void* self, void*, std::uint8_t value)
{
    runByteSetter<0x2735>("w3dviewshakeflag2", shakeFlag2State, self, value,
                          ra3::render::setCameraShakeFlag2);
}
void __fastcall shakeFlag3Wrapper(void* self, void*, std::uint8_t value)
{
    runByteSetter<0x2738>("w3dviewshakeflag3", shakeFlag3State, self, value,
                          ra3::render::setCameraShakeFlag3);
}

void __fastcall clearFilterWrapper(void* self, void*, std::uint32_t filter)
{
    clearFilterState.calls.fetch_add(1, relaxed);
    if (clearFilterState.mode == Mode::replace) {
        ra3::render::clearFilter(self, filter);
        return;
    }
    using Original = void(__thiscall*)(void*, std::uint32_t);
    reinterpret_cast<Original>(clearFilterState.original)(self, filter);
    std::array<std::byte, 0x4BA> copy{};
    ra3::render::clearFilter(copy.data(), filter);
    std::array<std::byte, 13> game{};
    std::array<std::byte, 13> ours{};
    const auto* bytes = static_cast<const std::byte*>(self);
    std::memcpy(game.data(), bytes + 0x5C, 4);
    std::memcpy(game.data() + 4, bytes + 0x64, 4);
    std::memcpy(game.data() + 8, bytes + 0x68, 4);
    game[12] = bytes[0x4B9];
    std::memcpy(ours.data(), copy.data() + 0x5C, 4);
    std::memcpy(ours.data() + 4, copy.data() + 0x64, 4);
    std::memcpy(ours.data() + 8, copy.data() + 0x68, 4);
    ours[12] = copy[0x4B9];
    recordOutput("w3dviewclearfilter", clearFilterState, game.data(), ours.data(), sizeof(game));
}

void __fastcall filterWrapper(void* self, void*, std::uint32_t mode, std::uint32_t filter)
{
    filterState.calls.fetch_add(1, relaxed);
    if (filterState.mode == Mode::replace) {
        ra3::render::setFilter(self, mode, filter);
        return;
    }
    using Original = void(__thiscall*)(void*, std::uint32_t, std::uint32_t);
    reinterpret_cast<Original>(filterState.original)(self, mode, filter);
    std::array<std::byte, 0x2678> copy{};
    ra3::render::setFilter(copy.data(), mode, filter);
    std::array<std::byte, 13> game{};
    std::array<std::byte, 13> ours{};
    const auto* bytes = static_cast<const std::byte*>(self);
    const auto outputSize = mode == 2 ? std::size_t{8} : game.size();
    std::memcpy(game.data(), bytes + 0x64, 8);
    std::memcpy(ours.data(), copy.data() + 0x64, 8);
    if (mode != 2) {
        game[8] = bytes[0x4B9];
        ours[8] = copy[0x4B9];
        std::memcpy(game.data() + 9, bytes + 0x2674, 4);
        std::memcpy(ours.data() + 9, copy.data() + 0x2674, 4);
    }
    recordOutput("w3dviewfilter", filterState, game.data(), ours.data(), outputSize);
}

std::uint8_t __fastcall finishedWrapper(void* self, void*)
{
    finishedState.calls.fetch_add(1, relaxed);
    const auto ours = static_cast<std::uint8_t>(ra3::render::cameraMovementFinished(self));
    if (finishedState.mode == Mode::replace) {
        return ours;
    }
    using Original = std::uint8_t(__thiscall*)(void*);
    const auto game = reinterpret_cast<Original>(finishedState.original)(self);
    recordScalar("w3dviewmovementfinished", finishedState, game, ours);
    return game;
}

std::uint8_t __fastcall lockEqualsWrapper(void* self, void*, std::int32_t objectId)
{
    lockEqualsState.calls.fetch_add(1, relaxed);
    const auto ours = static_cast<std::uint8_t>(ra3::render::cameraLockEquals(self, objectId));
    if (lockEqualsState.mode == Mode::replace) {
        return ours;
    }
    using Original = std::uint8_t(__thiscall*)(void*, std::int32_t);
    const auto game = reinterpret_cast<Original>(lockEqualsState.original)(self, objectId);
    recordScalar("w3dviewlockequals", lockEqualsState, game, ours);
    return game;
}

void __fastcall clearShakeWrapper(void* self, void*)
{
    clearShakeState.calls.fetch_add(1, relaxed);
    if (clearShakeState.mode == Mode::replace) {
        ra3::render::clearCameraShake(self);
        return;
    }
    using Original = void(__thiscall*)(void*);
    reinterpret_cast<Original>(clearShakeState.original)(self);
    std::array<std::byte, 0x2739> copy{};
    ra3::render::clearCameraShake(copy.data());
    recordOutput("w3dviewclearshake", clearShakeState,
                 static_cast<const std::byte*>(self) + 0x2728,
                 copy.data() + 0x2728, 0x11);
}

std::uint8_t __fastcall shakeActiveWrapper(void* self, void*)
{
    shakeActiveState.calls.fetch_add(1, relaxed);
    const auto ours = ra3::render::cameraShakeActive(self);
    if (shakeActiveState.mode == Mode::replace) {
        return ours;
    }
    using Original = std::uint8_t(__thiscall*)(void*);
    const auto game = reinterpret_cast<Original>(shakeActiveState.original)(self);
    recordScalar("w3dviewshakeactive", shakeActiveState, game, ours);
    return game;
}

}  // namespace

void appendRenderTargets(std::vector<Target>& table)
{
    table.push_back({"w3dviewwirepending", 0x00162290,
        {0x8A,0x44,0x24,0x04,0x88,0x81,0x51,0x03,0x00,0x00,0xC2,0x04,0x00,0xCC,0xCC,0xCC},
        10, reinterpret_cast<void*>(&wireWrapper), &wireState, true});
    table.push_back({"w3dviewclearfilter", 0x001622A0,
        {0x8B,0x44,0x24,0x04,0x0F,0x57,0xC0,0x89,0x41,0x5C,0x33,0xC0,0xF3,0x0F,0x11,0x41},
        7, reinterpret_cast<void*>(&clearFilterWrapper), &clearFilterState, true});
    table.push_back({"w3dviewfilter", 0x001622C0,
        {0x8B,0x44,0x24,0x04,0x83,0xF8,0x02,0xF3,0x0F,0x10,0x44,0x24,0x08,0x89,0x41,0x64},
        7, reinterpret_cast<void*>(&filterWrapper), &filterState, true});
    table.push_back({"w3dviewmovementfinished", 0x00162570,
        {0x83,0xB9,0x94,0x25,0x00,0x00,0x00,0x75,0x3F,0x80,0xB9,0x18,0x04,0x00,0x00,0x00},
        7, reinterpret_cast<void*>(&finishedWrapper), &finishedState, true});
    table.push_back({"w3dviewlockequals", 0x001625C0,
        {0x8B,0x89,0x10,0x26,0x00,0x00,0x33,0xC0,0x3B,0x4C,0x24,0x04,0x0F,0x94,0xC0,0xC2},
        6, reinterpret_cast<void*>(&lockEqualsWrapper), &lockEqualsState, true});
    table.push_back({"w3dviewclearshake", 0x00162950,
        {0x0F,0x57,0xC0,0x32,0xC0,0xF3,0x0F,0x11,0x81,0x28,0x27,0x00,0x00,0xF3,0x0F,0x11},
        5, reinterpret_cast<void*>(&clearShakeWrapper), &clearShakeState, true});
    table.push_back({"w3dviewshakeactive", 0x00162940,
        {0x8A,0x81,0x34,0x27,0x00,0x00,0xC3,0xCC,0xCC,0xCC,0xCC,0xCC,0xCC,0xCC,0xCC,0xCC},
        6, reinterpret_cast<void*>(&shakeActiveWrapper), &shakeActiveState, true});
    table.push_back({"w3dviewshakeflag0", 0x00162990,
        {0x8A,0x44,0x24,0x04,0x88,0x81,0x37,0x27,0x00,0x00,0xC2,0x04,0x00,0xCC,0xCC,0xCC},
        10, reinterpret_cast<void*>(&shakeFlag0Wrapper), &shakeFlag0State, true});
    table.push_back({"w3dviewshakeflag1", 0x001629A0,
        {0x8A,0x44,0x24,0x04,0x88,0x81,0x36,0x27,0x00,0x00,0xC2,0x04,0x00,0xCC,0xCC,0xCC},
        10, reinterpret_cast<void*>(&shakeFlag1Wrapper), &shakeFlag1State, true});
    table.push_back({"w3dviewshakeflag2", 0x001629B0,
        {0x8A,0x44,0x24,0x04,0x88,0x81,0x35,0x27,0x00,0x00,0xC2,0x04,0x00,0xCC,0xCC,0xCC},
        10, reinterpret_cast<void*>(&shakeFlag2Wrapper), &shakeFlag2State, true});
    table.push_back({"w3dviewshakeflag3", 0x001629C0,
        {0x8A,0x44,0x24,0x04,0x88,0x81,0x38,0x27,0x00,0x00,0xC2,0x04,0x00,0xCC,0xCC,0xCC},
        10, reinterpret_cast<void*>(&shakeFlag3Wrapper), &shakeFlag3State, true});
}

}  // namespace ra3hook
