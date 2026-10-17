#include "../Targets.h"

#include <ra3/render/W3DAnimation.h>
#include <ra3/render/W3DDrawable.h>
#include <ra3/render/W3DMaterial.h>

#include <array>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <cstring>

namespace ra3hook {
namespace {

constexpr auto relaxed = std::memory_order_relaxed;

TargetState word104State;
TargetState word108State;
TargetState extentState;
TargetState disableModeState;
TargetState enableMode2State;
TargetState assignPassMaskState;
TargetState equalPassMaskState;
TargetState drawablePassMaskState;
TargetState addColorState;
TargetState compatibleFlagsState;
TargetState textureSlotsState;
TargetState condition260State;
TargetState condition2cState;
TargetState blendActiveState;
TargetState extendedFlagsState;

template <std::size_t Offset>
void runWordSetter(
    const char* key, TargetState& state, void* self, std::uint32_t value,
    void (*port)(void*, std::uint32_t) noexcept)
{
    state.calls.fetch_add(1, relaxed);
    if (state.mode == Mode::replace) {
        port(self, value);
        return;
    }
    using Original = void(__thiscall*)(void*, std::uint32_t);
    reinterpret_cast<Original>(state.original)(self, value);
    std::array<std::byte, Offset + sizeof(value)> copy{};
    port(copy.data(), value);
    std::uint32_t game = 0;
    std::uint32_t ours = 0;
    std::memcpy(&game, static_cast<const std::byte*>(self) + Offset, sizeof(game));
    std::memcpy(&ours, copy.data() + Offset, sizeof(ours));
    recordScalar(key, state, game, ours);
}

void __fastcall word104Wrapper(void* self, void*, std::uint32_t value)
{
    runWordSetter<0x104>(
        "render8eword104", word104State, self, value,
        ra3::render::setDrawableWord104);
}

void __fastcall word108Wrapper(void* self, void*, std::uint32_t value)
{
    runWordSetter<0x108>(
        "render8eword108", word108State, self, value,
        ra3::render::setDrawableWord108);
}

void __fastcall extentWrapper(
    void* self, void*, std::uint32_t first, std::uint32_t second)
{
    extentState.calls.fetch_add(1, relaxed);
    if (extentState.mode == Mode::replace) {
        ra3::render::setDrawableExtent(self, first, second);
        return;
    }
    using Original = void(__thiscall*)(void*, std::uint32_t, std::uint32_t);
    reinterpret_cast<Original>(extentState.original)(self, first, second);
    std::array<std::byte, 0x784> copy{};
    ra3::render::setDrawableExtent(copy.data(), first, second);
    recordOutput(
        "render8eextent", extentState,
        static_cast<const std::byte*>(self) + 0x77C,
        copy.data() + 0x77C, 8);
}

void runModeSetter(
    const char* key, TargetState& state, void* self,
    void (*port)(void*) noexcept)
{
    state.calls.fetch_add(1, relaxed);
    if (state.mode == Mode::replace) {
        port(self);
        return;
    }
    using Original = void(__thiscall*)(void*);
    reinterpret_cast<Original>(state.original)(self);
    std::array<std::byte, 6> game{};
    std::array<std::byte, 6> ours{};
    std::array<std::byte, 0x77B> copy{};
    port(copy.data());
    std::memcpy(game.data(), static_cast<const std::byte*>(self) + 0x768, 4);
    game[4] = *(static_cast<const std::byte*>(self) + 0x778);
    game[5] = *(static_cast<const std::byte*>(self) + 0x77A);
    std::memcpy(ours.data(), copy.data() + 0x768, 4);
    ours[4] = copy[0x778];
    ours[5] = copy[0x77A];
    recordOutput(key, state, game.data(), ours.data(), game.size());
}

void __fastcall disableModeWrapper(void* self, void*)
{
    runModeSetter(
        "render8edisablemode", disableModeState, self,
        ra3::render::disableDrawableMode);
}

void __fastcall enableMode2Wrapper(void* self, void*)
{
    runModeSetter(
        "render8eenablemode2", enableMode2State, self,
        ra3::render::enableDrawableMode2);
}

void __fastcall assignPassMaskWrapper(
    ra3::render::PassMask* self, void*, const ra3::render::PassMask* source)
{
    assignPassMaskState.calls.fetch_add(1, relaxed);
    if (assignPassMaskState.mode == Mode::replace) {
        ra3::render::assignPassMask(*self, *source);
        return;
    }
    const auto before = *self;
    const auto sourceBefore = *source;
    using Original = void(__thiscall*)(
        ra3::render::PassMask*, const ra3::render::PassMask*);
    reinterpret_cast<Original>(assignPassMaskState.original)(self, source);
    auto ours = before;
    ra3::render::assignPassMask(ours, sourceBefore);
    recordOutput(
        "render8eassignpassmask", assignPassMaskState,
        self, &ours, sizeof(ours));
}

std::uint32_t __fastcall equalPassMaskWrapper(
    const ra3::render::PassMask* self, void*,
    const ra3::render::PassMask* other)
{
    equalPassMaskState.calls.fetch_add(1, relaxed);
    const auto ours =
        static_cast<std::uint32_t>(ra3::render::equalPassMask(*self, *other));
    if (equalPassMaskState.mode == Mode::replace) {
        return ours;
    }
    using Original = std::uint32_t(__thiscall*)(
        const ra3::render::PassMask*, const ra3::render::PassMask*);
    const auto game =
        reinterpret_cast<Original>(equalPassMaskState.original)(self, other);
    recordScalar("render8eequalpassmask", equalPassMaskState, game, ours);
    return game;
}

void __fastcall drawablePassMaskWrapper(
    void* self, void*, const ra3::render::PassMask* source)
{
    drawablePassMaskState.calls.fetch_add(1, relaxed);
    if (drawablePassMaskState.mode == Mode::replace) {
        ra3::render::assignDrawablePassMask(self, *source);
        return;
    }
    ra3::render::PassMask before{};
    std::memcpy(
        &before, static_cast<const std::byte*>(self) + 0xD8,
        sizeof(before));
    const auto sourceBefore = *source;
    using Original = void(__thiscall*)(
        void*, const ra3::render::PassMask*);
    reinterpret_cast<Original>(drawablePassMaskState.original)(self, source);
    std::array<std::byte, 0xE8> copy{};
    std::memcpy(copy.data() + 0xD8, &before, sizeof(before));
    ra3::render::assignDrawablePassMask(copy.data(), sourceBefore);
    recordOutput(
        "render8edrawablepassmask", drawablePassMaskState,
        static_cast<const std::byte*>(self) + 0xD8,
        copy.data() + 0xD8, sizeof(before));
}

std::uint32_t __cdecl addColorWrapper(
    std::uint32_t left, std::uint32_t right)
{
    addColorState.calls.fetch_add(1, relaxed);
    const auto ours = ra3::render::addPackedColor(left, right);
    if (addColorState.mode == Mode::replace) {
        return ours;
    }
    using Original = std::uint32_t(__cdecl*)(std::uint32_t, std::uint32_t);
    const auto game =
        reinterpret_cast<Original>(addColorState.original)(left, right);
    recordScalar("render8eaddcolor", addColorState, game, ours);
    return game;
}

std::uint32_t __cdecl compatibleFlagsWrapper(
    const std::uint32_t* left, const std::uint32_t* right)
{
    compatibleFlagsState.calls.fetch_add(1, relaxed);
    const auto ours = static_cast<std::uint32_t>(
        ra3::render::compatibleRenderFlags(*left, *right));
    if (compatibleFlagsState.mode == Mode::replace) {
        return ours;
    }
    using Original = std::uint32_t(__cdecl*)(
        const std::uint32_t*, const std::uint32_t*);
    const auto game =
        reinterpret_cast<Original>(compatibleFlagsState.original)(left, right);
    recordScalar("render8ecompatibleflags", compatibleFlagsState, game, ours);
    return game;
}

std::uint32_t __fastcall textureSlotsWrapper(const void* self, void*)
{
    textureSlotsState.calls.fetch_add(1, relaxed);
    const auto ours =
        static_cast<std::uint32_t>(ra3::render::modelHasTextureSlots(self));
    if (textureSlotsState.mode == Mode::replace) {
        return ours;
    }
    using Original = std::uint32_t(__thiscall*)(const void*);
    const auto game =
        reinterpret_cast<Original>(textureSlotsState.original)(self);
    recordScalar("render8etextureslots", textureSlotsState, game, ours);
    return game;
}

void* __fastcall condition260Wrapper(
    void* self, void*, const std::uint32_t* conditions)
{
    condition260State.calls.fetch_add(1, relaxed);
    auto* const ours =
        ra3::render::findConditionSubset260(self, conditions);
    if (condition260State.mode == Mode::replace) {
        return ours;
    }
    using Original = void*(__thiscall*)(void*, const std::uint32_t*);
    auto* const game =
        reinterpret_cast<Original>(condition260State.original)(self, conditions);
    recordScalar(
        "render8econdition260", condition260State,
        reinterpret_cast<std::uintptr_t>(game),
        reinterpret_cast<std::uintptr_t>(ours));
    return game;
}

void* __fastcall condition2cWrapper(
    void* self, void*, const std::uint32_t* conditions)
{
    condition2cState.calls.fetch_add(1, relaxed);
    auto* const ours =
        ra3::render::findConditionSubset2c(self, conditions);
    if (condition2cState.mode == Mode::replace) {
        return ours;
    }
    using Original = void*(__thiscall*)(void*, const std::uint32_t*);
    auto* const game =
        reinterpret_cast<Original>(condition2cState.original)(self, conditions);
    recordScalar(
        "render8econdition2c", condition2cState,
        reinterpret_cast<std::uintptr_t>(game),
        reinterpret_cast<std::uintptr_t>(ours));
    return game;
}

std::uint32_t __fastcall blendActiveWrapper(const void* self, void*)
{
    blendActiveState.calls.fetch_add(1, relaxed);
    const auto ours =
        static_cast<std::uint32_t>(ra3::render::animationBlendActive(self));
    if (blendActiveState.mode == Mode::replace) {
        return ours;
    }
    using Original = std::uint32_t(__thiscall*)(const void*);
    const auto game =
        reinterpret_cast<Original>(blendActiveState.original)(self);
    recordScalar("render8eblendactive", blendActiveState, game, ours);
    return game;
}

std::uint32_t __cdecl extendedFlagsWrapper(const std::uint32_t* flags)
{
    extendedFlagsState.calls.fetch_add(1, relaxed);
    const auto ours =
        static_cast<std::uint32_t>(ra3::render::hasExtendedRenderFlags(*flags));
    if (extendedFlagsState.mode == Mode::replace) {
        return ours;
    }
    using Original = std::uint32_t(__cdecl*)(const std::uint32_t*);
    const auto game =
        reinterpret_cast<Original>(extendedFlagsState.original)(flags);
    recordScalar("render8eextendedflags", extendedFlagsState, game, ours);
    return game;
}

}  // namespace

void appendRender8eTargets(std::vector<Target>& table)
{
    table.push_back({"render8eword104", 0x004E4610,
        {0x8B,0x44,0x24,0x04,0x89,0x81,0x04,0x01,0x00,0x00,0xC2,0x04,0x00,0xCC,0xCC,0xCC},
        10, reinterpret_cast<void*>(&word104Wrapper), &word104State, true});
    table.push_back({"render8eword108", 0x004E4620,
        {0x8B,0x44,0x24,0x04,0x89,0x81,0x08,0x01,0x00,0x00,0xC2,0x04,0x00,0xCC,0xCC,0xCC},
        10, reinterpret_cast<void*>(&word108Wrapper), &word108State, true});
    table.push_back({"render8eextent", 0x004E4680,
        {0xF3,0x0F,0x10,0x44,0x24,0x04,0xF3,0x0F,0x11,0x81,0x7C,0x07,0x00,0x00,0xF3,0x0F},
        6, reinterpret_cast<void*>(&extentWrapper), &extentState, true});
    table.push_back({"render8edisablemode", 0x004E4820,
        {0xB0,0x01,0xC7,0x81,0x68,0x07,0x00,0x00,0x00,0x00,0x00,0x00,0x88,0x81,0x78,0x07},
        12, reinterpret_cast<void*>(&disableModeWrapper), &disableModeState, true});
    table.push_back({"render8eenablemode2", 0x004E4910,
        {0xB0,0x01,0xC7,0x81,0x68,0x07,0x00,0x00,0x02,0x00,0x00,0x00,0x88,0x81,0x78,0x07},
        12, reinterpret_cast<void*>(&enableMode2Wrapper), &enableMode2State, true});
    table.push_back({"render8eassignpassmask", 0x004E4AA0,
        {0x8B,0xC1,0x8B,0x4C,0x24,0x04,0x3B,0xC8,0x74,0x2C,0x8B,0x11,0x33,0x10,0x56,0x83},
        6, reinterpret_cast<void*>(&assignPassMaskWrapper), &assignPassMaskState, true});
    table.push_back({"render8eequalpassmask", 0x004E4AE0,
        {0x8B,0x11,0x57,0x8B,0x7C,0x24,0x08,0x8B,0x07,0x83,0xE2,0x07,0x83,0xE0,0x07,0x3B},
        7, reinterpret_cast<void*>(&equalPassMaskWrapper), &equalPassMaskState, true});
    table.push_back({"render8edrawablepassmask", 0x004E4B30,
        {0x8D,0x81,0xD8,0x00,0x00,0x00,0x8B,0x4C,0x24,0x04,0x3B,0xC8,0x74,0x2E,0x8B,0x10},
        6, reinterpret_cast<void*>(&drawablePassMaskWrapper), &drawablePassMaskState, true});
    table.push_back({"render8eaddcolor", 0x004E5B80,
        {0x8B,0x54,0x24,0x04,0x53,0x8B,0x5C,0x24,0x0C,0x8B,0xC2,0x8B,0xCB,0xC1,0xE8,0x18},
        5, reinterpret_cast<void*>(&addColorWrapper), &addColorState, true});
    table.push_back({"render8ecompatibleflags", 0x004E87D0,
        {0x8B,0x44,0x24,0x04,0x8B,0x00,0x8B,0xC8,0xC1,0xE9,0x05,0xF6,0xC1,0x01,0x8B,0x4C},
        6, reinterpret_cast<void*>(&compatibleFlagsWrapper), &compatibleFlagsState, true});
    table.push_back({"render8etextureslots", 0x004E89D0,
        {0x8B,0x11,0x33,0xC0,0x8D,0x4A,0x0C,0x83,0x39,0x00,0x75,0x0E,0x83,0xC0,0x01,0x83},
        7, reinterpret_cast<void*>(&textureSlotsWrapper), &textureSlotsState, true});
    table.push_back({"render8econdition260", 0x004E8B20,
        {0x83,0xEC,0x0C,0x55,0x8B,0x6C,0x24,0x14,0x89,0x4C,0x24,0x08,0xC7,0x44,0x24,0x04},
        8, reinterpret_cast<void*>(&condition260Wrapper), &condition260State, true});
    table.push_back({"render8econdition2c", 0x004E8C10,
        {0x83,0xEC,0x0C,0x8B,0x51,0x30,0x8B,0x41,0x2C,0x53,0x55,0x33,0xDB,0x3B,0xC2,0x56},
        6, reinterpret_cast<void*>(&condition2cWrapper), &condition2cState, true});
    table.push_back({"render8eblendactive", 0x004E96B0,
        {0x83,0xB9,0x1C,0x01,0x00,0x00,0x00,0x74,0x0C,0x83,0xB9,0x38,0x01,0x00,0x00,0x00},
        7, reinterpret_cast<void*>(&blendActiveWrapper), &blendActiveState, true});
    table.push_back({"render8eextendedflags", 0x004ED5B0,
        {0x51,0x8B,0x4C,0x24,0x08,0x33,0xC0,0x0D,0xE0,0x01,0x00,0x00,0x89,0x04,0x24,0x8D},
        5, reinterpret_cast<void*>(&extendedFlagsWrapper), &extendedFlagsState, true});
}

}  // namespace ra3hook
