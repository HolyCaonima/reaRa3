#include "../Targets.h"

#include <ra3/render/W3DTerrainCluster56.h>

#include <atomic>
#include <cstdint>

namespace ra3hook {
namespace {

constexpr auto relaxed = std::memory_order_relaxed;

TargetState tvBorderSizeState;
TargetState tvCellFlagsState;
TargetState hmCellFlag78State;
TargetState hmCellFlag88State;
TargetState hmCliffStateState;
TargetState hmCellFlag98State;
TargetState hmCellFlagB8State;
TargetState hmCellFlagA8State;
TargetState hmCellFlagD8State;
TargetState hmCellFlagC8State;
TargetState hmShouldFlipQuadState;

std::int32_t __fastcall tvBorderSizeWrapper(const void* self, void*)
{
    tvBorderSizeState.calls.fetch_add(1, relaxed);
    const auto ours = ra3::render::getTerrainVisualBorderSize(self);
    if (tvBorderSizeState.mode == Mode::replace) {
        return ours;
    }
    using Original = std::int32_t(__thiscall*)(const void*);
    const auto game = reinterpret_cast<Original>(tvBorderSizeState.original)(self);
    recordScalar("tvbordersize", tvBorderSizeState, game, ours);
    return game;
}

std::uint32_t __fastcall tvCellFlagsWrapper(
    const void* self, void*, std::int32_t x, std::int32_t y)
{
    tvCellFlagsState.calls.fetch_add(1, relaxed);
    const auto ours = ra3::render::getCellFlags(self, x, y);
    if (tvCellFlagsState.mode == Mode::replace) {
        return ours;
    }
    using Original = std::uint32_t(__thiscall*)(const void*, std::int32_t, std::int32_t);
    const auto game = reinterpret_cast<Original>(tvCellFlagsState.original)(self, x, y);
    recordScalar("tvcellflags", tvCellFlagsState, game, ours);
    return game;
}

template <std::uint32_t (*PortFn)(const void*, std::int32_t, std::int32_t)>
std::uint32_t runCellQuery(
    const char* key, TargetState& state,
    const void* self, std::int32_t x, std::int32_t y)
{
    state.calls.fetch_add(1, relaxed);
    const auto ours = PortFn(self, x, y);
    if (state.mode == Mode::replace) {
        return ours;
    }
    using Original = std::uint32_t(__thiscall*)(const void*, std::int32_t, std::int32_t);
    const auto game = reinterpret_cast<Original>(state.original)(self, x, y);
    recordScalar(key, state, game, ours);
    return game;
}

std::uint32_t __fastcall hmCellFlag78Wrapper(
    const void* self, void*, std::int32_t x, std::int32_t y)
{
    return runCellQuery<&ra3::render::getCellFlagBit78>(
        "hmcellflag78", hmCellFlag78State, self, x, y);
}

std::uint32_t __fastcall hmCellFlag88Wrapper(
    const void* self, void*, std::int32_t x, std::int32_t y)
{
    return runCellQuery<&ra3::render::getCellFlagBit88>(
        "hmcellflag88", hmCellFlag88State, self, x, y);
}

std::uint32_t __fastcall hmCliffStateWrapper(
    const void* self, void*, std::int32_t x, std::int32_t y)
{
    return runCellQuery<&ra3::render::getCliffState>(
        "hmcliffstate", hmCliffStateState, self, x, y);
}

std::uint32_t __fastcall hmCellFlag98Wrapper(
    const void* self, void*, std::int32_t x, std::int32_t y)
{
    return runCellQuery<&ra3::render::getCellFlagBit98>(
        "hmcellflag98", hmCellFlag98State, self, x, y);
}

std::uint32_t __fastcall hmCellFlagB8Wrapper(
    const void* self, void*, std::int32_t x, std::int32_t y)
{
    return runCellQuery<&ra3::render::getCellFlagBitB8>(
        "hmcellflagb8", hmCellFlagB8State, self, x, y);
}

std::uint32_t __fastcall hmCellFlagA8Wrapper(
    const void* self, void*, std::int32_t x, std::int32_t y)
{
    return runCellQuery<&ra3::render::getCellFlagBitA8>(
        "hmcellflaga8", hmCellFlagA8State, self, x, y);
}

std::uint32_t __fastcall hmCellFlagD8Wrapper(
    const void* self, void*, std::int32_t x, std::int32_t y)
{
    return runCellQuery<&ra3::render::getCellFlagBitD8>(
        "hmcellflagd8", hmCellFlagD8State, self, x, y);
}

std::uint32_t __fastcall hmCellFlagC8Wrapper(
    const void* self, void*, std::int32_t x, std::int32_t y)
{
    return runCellQuery<&ra3::render::getCellFlagBitC8>(
        "hmcellflagc8", hmCellFlagC8State, self, x, y);
}

std::uint32_t __fastcall hmShouldFlipQuadWrapper(
    const void* self, void*, std::int32_t x, std::int32_t y)
{
    return runCellQuery<&ra3::render::shouldFlipCellQuad>(
        "hmshouldflipquad", hmShouldFlipQuadState, self, x, y);
}

}  // namespace

void appendRender56Targets(std::vector<Target>& table)
{
    table.push_back({"tvbordersize", 0x00161E70,
        {0x8B,0x41,0x30,0x85,0xC0,0x74,0x04,0x8B,0x40,0x2C,0xC3,0x33,0xC0,0xC3,0xCC,0xCC},
        5, reinterpret_cast<void*>(&tvBorderSizeWrapper), &tvBorderSizeState, true});
    table.push_back({"hmcellflag78", 0x001642B0,
        {0x56,0x8B,0xF1,0x8B,0x4C,0x24,0x08,0x32,0xC0,0x3B,0x4E,0x24,0x73,0x3A,0x57,0x8B},
        7, reinterpret_cast<void*>(&hmCellFlag78Wrapper), &hmCellFlag78State, true});
    table.push_back({"hmcellflag88", 0x00164300,
        {0x56,0x8B,0xF1,0x8B,0x4C,0x24,0x08,0x32,0xC0,0x3B,0x4E,0x24,0x73,0x43,0x57,0x8B},
        7, reinterpret_cast<void*>(&hmCellFlag88Wrapper), &hmCellFlag88State, true});
    table.push_back({"tvcellflags", 0x0016B380,
        {0x8B,0x49,0x30,0x8B,0x51,0x2C,0x8B,0x44,0x24,0x08,0x03,0xC2,0x0F,0xAF,0x41,0x24},
        6, reinterpret_cast<void*>(&tvCellFlagsWrapper), &tvCellFlagsState, true});
    table.push_back({"hmcliffstate", 0x0016D970,
        {0x56,0x8B,0xF1,0x8B,0x4C,0x24,0x08,0x85,0xC9,0x7C,0x47,0x8B,0x54,0x24,0x0C,0x85},
        7, reinterpret_cast<void*>(&hmCliffStateWrapper), &hmCliffStateState, true});
    table.push_back({"hmcellflag98", 0x0016D9D0,
        {0x56,0x8B,0xF1,0x8B,0x4C,0x24,0x08,0x85,0xC9,0x7C,0x50,0x8B,0x54,0x24,0x0C,0x85},
        7, reinterpret_cast<void*>(&hmCellFlag98Wrapper), &hmCellFlag98State, true});
    table.push_back({"hmcellflagb8", 0x0016DA40,
        {0x56,0x8B,0xF1,0x8B,0x4C,0x24,0x08,0x85,0xC9,0x7C,0x50,0x8B,0x54,0x24,0x0C,0x85},
        7, reinterpret_cast<void*>(&hmCellFlagB8Wrapper), &hmCellFlagB8State, true});
    table.push_back({"hmcellflaga8", 0x0016DAB0,
        {0x56,0x8B,0xF1,0x8B,0x4C,0x24,0x08,0x85,0xC9,0x7C,0x50,0x8B,0x54,0x24,0x0C,0x85},
        7, reinterpret_cast<void*>(&hmCellFlagA8Wrapper), &hmCellFlagA8State, true});
    table.push_back({"hmcellflagd8", 0x0016DB20,
        {0x56,0x8B,0xF1,0x8B,0x4C,0x24,0x08,0x85,0xC9,0x7C,0x49,0x8B,0x44,0x24,0x0C,0x85},
        7, reinterpret_cast<void*>(&hmCellFlagD8Wrapper), &hmCellFlagD8State, true});
    table.push_back({"hmcellflagc8", 0x0016DB80,
        {0x56,0x8B,0xF1,0x8B,0x4C,0x24,0x08,0x85,0xC9,0x7C,0x49,0x8B,0x44,0x24,0x0C,0x85},
        7, reinterpret_cast<void*>(&hmCellFlagC8Wrapper), &hmCellFlagC8State, true});
    table.push_back({"hmshouldflipquad", 0x0017DB70,
        {0x53,0x55,0x8B,0x6C,0x24,0x10,0x56,0x8B,0xF1,0x8B,0x86,0x4C,0x71,0x01,0x00,0x8B},
        6, reinterpret_cast<void*>(&hmShouldFlipQuadWrapper), &hmShouldFlipQuadState, true});
}

}  // namespace ra3hook
