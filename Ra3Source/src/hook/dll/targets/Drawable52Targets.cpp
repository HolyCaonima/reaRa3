#include "../Targets.h"

#include <ra3/gameclient/DrawableAccess.h>

#include <cstdint>
#include <cstring>

namespace ra3hook {
namespace {

TargetState g_tintStatic;
TargetState g_copyTwelve;
TargetState g_position;
TargetState g_transform;

constexpr std::uint32_t kPositionGlobals = 0x00DEED68;
constexpr std::uint32_t kTransformGlobals = 0x00DEEDC0;

std::uint32_t __fastcall hookTintStatic(void*, void*)
{
    TargetState& state = g_tintStatic;
    state.calls.fetch_add(1, std::memory_order_relaxed);
    const std::uint32_t ours = ra3::gameclient::tintEnvelopeStatic();
    if (state.mode == Mode::replace) {
        state.matches.fetch_add(1, std::memory_order_relaxed);
        return ours;
    }
    const std::uint32_t game =
        reinterpret_cast<std::uint32_t(__fastcall*)(void*, void*)>(state.original)(nullptr, nullptr);
    recordScalar("drawable52tintstatic", state, game, ours);
    return game;
}

std::uint32_t __fastcall hookCopyTwelve(void* destination, void*, const void* source)
{
    TargetState& state = g_copyTwelve;
    state.calls.fetch_add(1, std::memory_order_relaxed);
    std::uint8_t saved[48];
    std::uint8_t oursBytes[48];
    std::memcpy(saved, destination, 48);
    const std::uint32_t ours = ra3::gameclient::copyTwelveDwords(destination, source);
    std::memcpy(oursBytes, destination, 48);
    std::memcpy(destination, saved, 48);
    if (state.mode == Mode::replace) {
        std::memcpy(destination, oursBytes, 48);
        state.matches.fetch_add(1, std::memory_order_relaxed);
        return ours;
    }
    const std::uint32_t game =
        reinterpret_cast<std::uint32_t(__thiscall*)(void*, const void*)>(state.original)(
            destination, source);
    recordScalar("drawable52copytwelve", state, game, ours);
    recordOutput("drawable52copytwelve", state, destination, oursBytes, 48);
    return game;
}

std::uint32_t __fastcall hookPosition(void* self, void*)
{
    TargetState& state = g_position;
    state.calls.fetch_add(1, std::memory_order_relaxed);
    auto* globals = reinterpret_cast<ra3::gameclient::PositionGlobals*>(kPositionGlobals);
    const ra3::gameclient::PositionGlobals saved = *globals;
    const std::uint32_t ours = ra3::gameclient::positionPointer(self, globals);
    const ra3::gameclient::PositionGlobals oursBytes = *globals;
    *globals = saved;
    if (state.mode == Mode::replace) {
        *globals = oursBytes;
        state.matches.fetch_add(1, std::memory_order_relaxed);
        return ours;
    }
    const std::uint32_t game =
        reinterpret_cast<std::uint32_t(__thiscall*)(void*)>(state.original)(self);
    recordScalar("drawable52position", state, game, ours);
    recordOutput("drawable52position", state, globals, &oursBytes, sizeof(oursBytes));
    return game;
}

std::uint32_t __fastcall hookTransform(void* self, void*)
{
    TargetState& state = g_transform;
    state.calls.fetch_add(1, std::memory_order_relaxed);
    auto* globals = reinterpret_cast<ra3::gameclient::TransformGlobals*>(kTransformGlobals);
    const ra3::gameclient::TransformGlobals saved = *globals;
    const std::uint32_t ours = ra3::gameclient::transformPointer(self, globals);
    const ra3::gameclient::TransformGlobals oursBytes = *globals;
    *globals = saved;
    if (state.mode == Mode::replace) {
        *globals = oursBytes;
        state.matches.fetch_add(1, std::memory_order_relaxed);
        return ours;
    }
    const std::uint32_t game =
        reinterpret_cast<std::uint32_t(__thiscall*)(void*)>(state.original)(self);
    recordScalar("drawable52transform", state, game, ours);
    recordOutput("drawable52transform", state, globals, &oursBytes, sizeof(oursBytes));
    return game;
}

}  // namespace

void appendDrawable52Targets(std::vector<Target>& table)
{
    table.insert(table.end(), {
        {"drawable52tintstatic", 0x001261A0,
         {0xB8, 0xE4, 0xFF, 0xBD, 0x00, 0xC3, 0xCC, 0xCC,
          0xCC, 0xCC, 0xCC, 0xCC, 0xCC, 0xCC, 0xCC, 0xCC},
         5, reinterpret_cast<void*>(&hookTintStatic), &g_tintStatic, true},
        {"drawable52copytwelve", 0x0012A800,
         {0x8B, 0xC1, 0x8B, 0x4C, 0x24, 0x04, 0xD9, 0x01,
          0xD9, 0x18, 0xD9, 0x41, 0x04, 0xD9, 0x58, 0x04},
         6, reinterpret_cast<void*>(&hookCopyTwelve), &g_copyTwelve, true},
        {"drawable52position", 0x00125CF0,
         {0x83, 0xB9, 0x30, 0x01, 0x00, 0x00, 0xFF, 0x75,
          0x34, 0xB8, 0x01, 0x00, 0x00, 0x00, 0x84, 0x05},
         7, reinterpret_cast<void*>(&hookPosition), &g_position, true},
        {"drawable52transform", 0x0012D790,
         {0x83, 0xB9, 0x30, 0x01, 0x00, 0x00, 0xFF, 0x0F,
          0x85, 0x84, 0x00, 0x00, 0x00, 0xB8, 0x01, 0x00},
         7, reinterpret_cast<void*>(&hookTransform), &g_transform, true},
    });
}

}  // namespace ra3hook
