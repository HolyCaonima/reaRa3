#include "../Targets.h"

#include <ra3/ui/UiControl.h>

#include <array>
#include <cstdint>
#include <cstring>

namespace ra3hook {
namespace {

constexpr auto relaxed = std::memory_order_relaxed;

TargetState g_updateHitRegionBounds;
TargetState g_controllerUiObject;
TargetState g_controllerInputTable;
TargetState g_countControlFlags;
TargetState g_initializeHitRegion;
TargetState g_hitRegionContains;
TargetState g_moveTextCaretLeft;
TargetState g_makeTextSlice;
TargetState g_makeTextPrefix;
TargetState g_makeTextSuffix;

void recordComparison(
    const char* const key, TargetState& state, const bool equal,
    const char* const detail)
{
    if (equal) {
        state.matches.fetch_add(1, relaxed);
    } else {
        reportMismatch(key, state, detail);
    }
}

std::uint32_t __fastcall controllerUiObject(void* const controller, void*)
{
    TargetState& state = g_controllerUiObject;
    state.calls.fetch_add(1, relaxed);
    const std::uint32_t ours =
        ra3::ui::controllerUiObject(ra3::ui::LayoutView(controller));
    if (state.mode == Mode::replace) {
        state.matches.fetch_add(1, relaxed);
        return ours;
    }
    const std::uint32_t game =
        reinterpret_cast<std::uint32_t(__thiscall*)(void*)>(
            state.original)(controller);
    recordScalar("uicontrollerobject", state, game, ours);
    return game;
}

std::uint32_t __fastcall controllerInputTable(void* const controller, void*)
{
    TargetState& state = g_controllerInputTable;
    state.calls.fetch_add(1, relaxed);
    const std::uint32_t ours =
        ra3::ui::controllerInputTable(ra3::ui::LayoutView(controller));
    if (state.mode == Mode::replace) {
        state.matches.fetch_add(1, relaxed);
        return ours;
    }
    const std::uint32_t game =
        reinterpret_cast<std::uint32_t(__thiscall*)(void*)>(
            state.original)(controller);
    recordScalar("uicontrollerinputtable", state, game, ours);
    return game;
}

std::uint32_t __fastcall countControlFlags(
    const std::uint32_t* const words, void*)
{
    TargetState& state = g_countControlFlags;
    state.calls.fetch_add(1, relaxed);
    const std::uint32_t ours = ra3::ui::countControlFlags(words);
    if (state.mode == Mode::replace) {
        state.matches.fetch_add(1, relaxed);
        return ours;
    }
    const std::uint32_t game =
        reinterpret_cast<std::uint32_t(__thiscall*)(const std::uint32_t*)>(
            state.original)(words);
    recordScalar("uicountcontrolflags", state, game, ours);
    return game;
}

void __fastcall updateHitRegionBounds(void* const region, void*)
{
    TargetState& state = g_updateHitRegionBounds;
    state.calls.fetch_add(1, relaxed);
    if (state.mode == Mode::replace) {
        ra3::ui::updateHitRegionBounds(ra3::ui::HitRegionView(region));
        state.matches.fetch_add(1, relaxed);
        return;
    }
    std::array<std::uint8_t, 0x24> ours{};
    std::memcpy(ours.data(), region, ours.size());
    ra3::ui::updateHitRegionBounds(ra3::ui::HitRegionView(ours.data()));
    reinterpret_cast<void(__thiscall*)(void*)>(state.original)(region);
    recordComparison(
        "uiupdatehitregionbounds", state,
        std::memcmp(region, ours.data(), ours.size()) == 0,
        "hit-region-layout");
}

void* __fastcall initializeHitRegion(void* const region, void*)
{
    TargetState& state = g_initializeHitRegion;
    state.calls.fetch_add(1, relaxed);
    if (state.mode == Mode::replace) {
        ra3::ui::initializeHitRegion(ra3::ui::HitRegionView(region));
        state.matches.fetch_add(1, relaxed);
        return region;
    }
    std::array<std::uint8_t, 0x24> ours{};
    std::memcpy(ours.data(), region, ours.size());
    ra3::ui::initializeHitRegion(ra3::ui::HitRegionView(ours.data()));
    void* const result =
        reinterpret_cast<void*(__thiscall*)(void*)>(state.original)(region);
    recordComparison(
        "uiinitializehitregion", state,
        result == region && std::memcmp(region, ours.data(), ours.size()) == 0,
        "return-or-layout");
    return result;
}

bool __cdecl hitRegionContains(void* const region, const ra3::ui::Point* const point)
{
    TargetState& state = g_hitRegionContains;
    state.calls.fetch_add(1, relaxed);
    if (state.mode == Mode::replace) {
        const bool ours = ra3::ui::hitRegionContains(
            ra3::ui::HitRegionView(region), *point);
        state.matches.fetch_add(1, relaxed);
        return ours;
    }
    std::array<std::uint8_t, 0x24> oursRegion{};
    std::memcpy(oursRegion.data(), region, oursRegion.size());
    const bool ours = ra3::ui::hitRegionContains(
        ra3::ui::HitRegionView(oursRegion.data()), *point);
    const bool game =
        reinterpret_cast<bool(__cdecl*)(void*, const ra3::ui::Point*)>(
            state.original)(region, point);
    recordComparison(
        "uihitregioncontains", state,
        game == ours
            && std::memcmp(region, oursRegion.data(), oursRegion.size()) == 0,
        "return-or-cache-layout");
    return game;
}

void __fastcall moveTextCaretLeft(
    void* const edit, void*, const std::uint8_t* const key)
{
    TargetState& state = g_moveTextCaretLeft;
    state.calls.fetch_add(1, relaxed);
    std::uint32_t modifiers = 0;
    std::memcpy(&modifiers, key + 4, sizeof(modifiers));
    if (state.mode == Mode::replace) {
        ra3::ui::moveTextCaretLeft(ra3::ui::TextEditView(edit), modifiers);
        state.matches.fetch_add(1, relaxed);
        return;
    }
    std::array<std::uint8_t, 0x38> ours{};
    std::memcpy(ours.data(), edit, ours.size());
    ra3::ui::moveTextCaretLeft(ra3::ui::TextEditView(ours.data()), modifiers);
    reinterpret_cast<void(__thiscall*)(void*, const void*)>(
        state.original)(edit, key);
    recordComparison(
        "uimovetextcaretleft", state,
        std::memcmp(edit, ours.data(), ours.size()) == 0, "edit-layout");
}

using SliceImplementation =
    ra3::ui::TextSlice (*)(const void*, std::int32_t) noexcept;

ra3::ui::TextSlice* shadowShortSlice(
    const char* const key, TargetState& state, ra3::ui::TextSlice* const output,
    const void* const sourceHandle, const std::int32_t value,
    const SliceImplementation implementation)
{
    state.calls.fetch_add(1, relaxed);
    const ra3::ui::TextSlice ours = implementation(sourceHandle, value);
    if (state.mode == Mode::replace) {
        *output = ours;
        state.matches.fetch_add(1, relaxed);
        return output;
    }
    reinterpret_cast<ra3::ui::TextSlice*(__cdecl*)(
        ra3::ui::TextSlice*, const void*, std::int32_t)>(
            state.original)(output, sourceHandle, value);
    recordOutput(key, state, output, &ours, sizeof(ours));
    return output;
}

ra3::ui::TextSlice* __cdecl makeTextSlice(
    ra3::ui::TextSlice* const output, const void* const sourceHandle,
    const std::int32_t start, const std::int32_t count)
{
    TargetState& state = g_makeTextSlice;
    state.calls.fetch_add(1, relaxed);
    const ra3::ui::TextSlice ours =
        ra3::ui::makeTextSlice(sourceHandle, start, count);
    if (state.mode == Mode::replace) {
        *output = ours;
        state.matches.fetch_add(1, relaxed);
        return output;
    }
    reinterpret_cast<ra3::ui::TextSlice*(__cdecl*)(
        ra3::ui::TextSlice*, const void*, std::int32_t, std::int32_t)>(
            state.original)(output, sourceHandle, start, count);
    recordOutput("uimaketextslice", state, output, &ours, sizeof(ours));
    return output;
}

ra3::ui::TextSlice* __cdecl makeTextPrefix(
    ra3::ui::TextSlice* const output, const void* const sourceHandle,
    const std::int32_t count)
{
    return shadowShortSlice(
        "uimaketextprefix", g_makeTextPrefix, output, sourceHandle, count,
        &ra3::ui::makeTextPrefix);
}

ra3::ui::TextSlice* __cdecl makeTextSuffix(
    ra3::ui::TextSlice* const output, const void* const sourceHandle,
    const std::int32_t count)
{
    return shadowShortSlice(
        "uimaketextsuffix", g_makeTextSuffix, output, sourceHandle, count,
        &ra3::ui::makeTextSuffix);
}

}  // namespace

void appendUiTargets(std::vector<Target>& table)
{
    table.insert(table.end(), {
        {"uiupdatehitregionbounds", 0x0066FFE0,
         {0x8B, 0x01, 0x8B, 0x10, 0x89, 0x51, 0x10, 0x8B,
          0x50, 0x04, 0x89, 0x51, 0x14, 0x8B, 0x10, 0x89},
         7, reinterpret_cast<void*>(&updateHitRegionBounds),
         &g_updateHitRegionBounds, true},
        {"uicontrollerobject", 0x006705A0,
         {0x8B, 0x41, 0x04, 0x8B, 0x80, 0x1C, 0x02, 0x00,
          0x00, 0xC3, 0xCC, 0xCC, 0xCC, 0xCC, 0xCC, 0xCC},
         9, reinterpret_cast<void*>(&controllerUiObject), &g_controllerUiObject, true},
        {"uicontrollerinputtable", 0x00670670,
         {0x8B, 0x41, 0x04, 0x8B, 0x40, 0x04, 0x05, 0xD0,
          0x02, 0x00, 0x00, 0xC3, 0xCC, 0xCC, 0xCC, 0xCC},
         6, reinterpret_cast<void*>(&controllerInputTable),
         &g_controllerInputTable, true},
        {"uicountcontrolflags", 0x00670700,
         {0x8B, 0x01, 0x8B, 0x49, 0x04, 0x8B, 0xD0, 0xD1,
          0xEA, 0x81, 0xE2, 0x55, 0x55, 0x55, 0x55, 0x2B},
         5, reinterpret_cast<void*>(&countControlFlags), &g_countControlFlags, true},
        {"uiinitializehitregion", 0x00671B20,
         {0x8B, 0xC1, 0x33, 0xC9, 0x33, 0xD2, 0x89, 0x08,
          0x89, 0x48, 0x04, 0x89, 0x48, 0x08, 0xC6, 0x40},
         6, reinterpret_cast<void*>(&initializeHitRegion),
         &g_initializeHitRegion, true},
        {"uihitregioncontains", 0x00671B70,
         {0x55, 0x8B, 0xEC, 0x83, 0xE4, 0xF8, 0x8B, 0x4D,
          0x08, 0x8B, 0x41, 0x04, 0x2B, 0x01, 0x83, 0xEC},
         6, reinterpret_cast<void*>(&hitRegionContains), &g_hitRegionContains, true},
        {"uimovetextcaretleft", 0x00671E80,
         {0x8B, 0x54, 0x24, 0x04, 0x8B, 0x41, 0x10, 0x57,
          0x33, 0xFF, 0xF7, 0x42, 0x04, 0x30, 0x04, 0x00},
         7, reinterpret_cast<void*>(&moveTextCaretLeft), &g_moveTextCaretLeft, true},
        {"uimaketextslice", 0x00673590,
         {0x56, 0x8B, 0x74, 0x24, 0x0C, 0x8B, 0x06, 0x85,
          0xC0, 0x74, 0x06, 0x0F, 0xB7, 0x50, 0xFC, 0xEB},
         5, reinterpret_cast<void*>(&makeTextSlice), &g_makeTextSlice, true},
        {"uimaketextprefix", 0x006735F0,
         {0x56, 0x8B, 0x74, 0x24, 0x0C, 0x8B, 0x06, 0x33,
          0xC9, 0x85, 0xC0, 0x74, 0x06, 0x0F, 0xB7, 0x50},
         5, reinterpret_cast<void*>(&makeTextPrefix), &g_makeTextPrefix, true},
        {"uimaketextsuffix", 0x00673640,
         {0x57, 0x8B, 0x7C, 0x24, 0x0C, 0x8B, 0x07, 0x85,
          0xC0, 0x74, 0x06, 0x0F, 0xB7, 0x48, 0xFC, 0xEB},
         5, reinterpret_cast<void*>(&makeTextSuffix), &g_makeTextSuffix, true},
    });
}

}  // namespace ra3hook
