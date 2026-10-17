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

using CellQueryFn = std::uint32_t(__thiscall*)(const void*, std::int32_t, std::int32_t);

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
    const auto game = reinterpret_cast<CellQueryFn>(tvCellFlagsState.original)(self, x, y);
    recordScalar("tvcellflags", tvCellFlagsState, game, ours);
    return game;
}

std::uint32_t __fastcall hmCellFlag78Wrapper(
    const void* self, void*, std::int32_t x, std::int32_t y)
{
    hmCellFlag78State.calls.fetch_add(1, relaxed);
    const auto ours = ra3::render::getCellFlagBit78(self, x, y);
    if (hmCellFlag78State.mode == Mode::replace) {
        return ours;
    }
    const auto game = reinterpret_cast<CellQueryFn>(hmCellFlag78State.original)(self, x, y);
    recordScalar("hmcellflag78", hmCellFlag78State, game, ours);
    return game;
}

std::uint32_t __fastcall hmCellFlag88Wrapper(
    const void* self, void*, std::int32_t x, std::int32_t y)
{
    hmCellFlag88State.calls.fetch_add(1, relaxed);
    const auto ours = ra3::render::getCellFlagBit88(self, x, y);
    if (hmCellFlag88State.mode == Mode::replace) {
        return ours;
    }
    const auto game = reinterpret_cast<CellQueryFn>(hmCellFlag88State.original)(self, x, y);
    recordScalar("hmcellflag88", hmCellFlag88State, game, ours);
    return game;
}

std::uint32_t __fastcall hmCliffStateWrapper(
    const void* self, void*, std::int32_t x, std::int32_t y)
{
    hmCliffStateState.calls.fetch_add(1, relaxed);
    const auto ours = ra3::render::getCliffState(self, x, y);
    if (hmCliffStateState.mode == Mode::replace) {
        return ours;
    }
    const auto game = reinterpret_cast<CellQueryFn>(hmCliffStateState.original)(self, x, y);
    recordScalar("hmcliffstate", hmCliffStateState, game, ours);
    return game;
}

std::uint32_t __fastcall hmCellFlag98Wrapper(
    const void* self, void*, std::int32_t x, std::int32_t y)
{
    hmCellFlag98State.calls.fetch_add(1, relaxed);
    const auto ours = ra3::render::getCellFlagBit98(self, x, y);
    if (hmCellFlag98State.mode == Mode::replace) {
        return ours;
    }
    const auto game = reinterpret_cast<CellQueryFn>(hmCellFlag98State.original)(self, x, y);
    recordScalar("hmcellflag98", hmCellFlag98State, game, ours);
    return game;
}

std::uint32_t __fastcall hmCellFlagB8Wrapper(
    const void* self, void*, std::int32_t x, std::int32_t y)
{
    hmCellFlagB8State.calls.fetch_add(1, relaxed);
    const auto ours = ra3::render::getCellFlagBitB8(self, x, y);
    if (hmCellFlagB8State.mode == Mode::replace) {
        return ours;
    }
    const auto game = reinterpret_cast<CellQueryFn>(hmCellFlagB8State.original)(self, x, y);
    recordScalar("hmcellflagb8", hmCellFlagB8State, game, ours);
    return game;
}

std::uint32_t __fastcall hmCellFlagA8Wrapper(
    const void* self, void*, std::int32_t x, std::int32_t y)
{
    hmCellFlagA8State.calls.fetch_add(1, relaxed);
    const auto ours = ra3::render::getCellFlagBitA8(self, x, y);
    if (hmCellFlagA8State.mode == Mode::replace) {
        return ours;
    }
    const auto game = reinterpret_cast<CellQueryFn>(hmCellFlagA8State.original)(self, x, y);
    recordScalar("hmcellflaga8", hmCellFlagA8State, game, ours);
    return game;
}

std::uint32_t __fastcall hmCellFlagD8Wrapper(
    const void* self, void*, std::int32_t x, std::int32_t y)
{
    hmCellFlagD8State.calls.fetch_add(1, relaxed);
    const auto ours = ra3::render::getCellFlagBitD8(self, x, y);
    if (hmCellFlagD8State.mode == Mode::replace) {
        return ours;
    }
    const auto game = reinterpret_cast<CellQueryFn>(hmCellFlagD8State.original)(self, x, y);
    recordScalar("hmcellflagd8", hmCellFlagD8State, game, ours);
    return game;
}

std::uint32_t __fastcall hmCellFlagC8Wrapper(
    const void* self, void*, std::int32_t x, std::int32_t y)
{
    hmCellFlagC8State.calls.fetch_add(1, relaxed);
    const auto ours = ra3::render::getCellFlagBitC8(self, x, y);
    if (hmCellFlagC8State.mode == Mode::replace) {
        return ours;
    }
    const auto game = reinterpret_cast<CellQueryFn>(hmCellFlagC8State.original)(self, x, y);
    recordScalar("hmcellflagc8", hmCellFlagC8State, game, ours);
    return game;
}

std::uint32_t __fastcall hmShouldFlipQuadWrapper(
    const void* self, void*, std::int32_t x, std::int32_t y)
{
    hmShouldFlipQuadState.calls.fetch_add(1, relaxed);
    const auto ours = ra3::render::shouldFlipCellQuad(self, x, y);
    if (hmShouldFlipQuadState.mode == Mode::replace) {
        return ours;
    }
    const auto game = reinterpret_cast<CellQueryFn>(hmShouldFlipQuadState.original)(self, x, y);
    recordScalar("hmshouldflipquad", hmShouldFlipQuadState, game, ours);
    return game;
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
