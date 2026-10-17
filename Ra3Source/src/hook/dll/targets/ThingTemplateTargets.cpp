#include "../Targets.h"

#include <ra3/common/ThingTemplate.h>

#include <cstdint>

namespace ra3hook {
namespace {

TargetState g_maskAll;
TargetState g_maskAllAtC0;
TargetState g_maskAny;
TargetState g_findModule;
TargetState g_registryPresent;

std::uint32_t __fastcall hookMaskAll(
    const std::uint32_t* objectWords, void*, const std::uint32_t* query)
{
    ++g_maskAll.calls;
    const std::uint32_t ours = ra3::common::maskAllNine(objectWords, query);
    if (g_maskAll.mode == Mode::replace) {
        return ours;
    }
    using Original = std::uint32_t(__thiscall*)(const std::uint32_t*, const std::uint32_t*);
    const std::uint32_t game =
        reinterpret_cast<Original>(g_maskAll.original)(objectWords, query);
    recordScalar("thingtemplatemaskall", g_maskAll, game, ours);
    return game;
}

std::uint32_t __fastcall hookMaskAllAtC0(
    const std::uint8_t* object, void*, const std::uint32_t* query)
{
    ++g_maskAllAtC0.calls;
    const std::uint32_t ours = ra3::common::maskAllNineAtC0(object, query);
    if (g_maskAllAtC0.mode == Mode::replace) {
        return ours;
    }
    using Original = std::uint32_t(__thiscall*)(const std::uint8_t*, const std::uint32_t*);
    const std::uint32_t game =
        reinterpret_cast<Original>(g_maskAllAtC0.original)(object, query);
    recordScalar("thingtemplatemaskallatc0", g_maskAllAtC0, game, ours);
    return game;
}

std::uint32_t __fastcall hookMaskAny(
    const std::uint8_t* object, void*, const std::uint32_t* query)
{
    ++g_maskAny.calls;
    const std::uint32_t ours = ra3::common::maskAnyNineAtC0(object, query);
    if (g_maskAny.mode == Mode::replace) {
        return ours;
    }
    using Original = std::uint32_t(__thiscall*)(const std::uint8_t*, const std::uint32_t*);
    const std::uint32_t game = reinterpret_cast<Original>(g_maskAny.original)(object, query);
    recordScalar("thingtemplatemaskany", g_maskAny, game, ours);
    return game;
}

std::uint32_t __fastcall hookFindModule(const std::uint8_t* container, void*, std::uint32_t key)
{
    ++g_findModule.calls;
    const std::uint32_t ours = ra3::common::findModuleRecordByKey(container, key);
    if (g_findModule.mode == Mode::replace) {
        return ours;
    }
    using Original = std::uint32_t(__thiscall*)(const std::uint8_t*, std::uint32_t);
    const std::uint32_t game =
        reinterpret_cast<Original>(g_findModule.original)(container, key);
    recordScalar("thingtemplatefindmodule", g_findModule, game, ours);
    return game;
}

std::uint32_t __fastcall hookRegistryPresent(const std::uint8_t* registry, void*, std::uint32_t key)
{
    ++g_registryPresent.calls;
    const std::uint32_t ours = ra3::common::registryValuePresent(registry, key);
    if (g_registryPresent.mode == Mode::replace) {
        return ours;
    }
    using Original = std::uint32_t(__thiscall*)(const std::uint8_t*, std::uint32_t);
    const std::uint32_t game =
        reinterpret_cast<Original>(g_registryPresent.original)(registry, key);
    recordScalar("thingtemplateregistrypresent", g_registryPresent, game, ours);
    return game;
}

}  // namespace

void appendThingTemplateTargets(std::vector<Target>& table)
{
    table.insert(table.end(), {
        {"thingtemplatemaskall", 0x0038E7C0,
         {0x8B, 0x44, 0x24, 0x04, 0x56, 0x8B, 0xF1, 0x33,
          0xD2, 0x57, 0x2B, 0xF0, 0x8D, 0x64, 0x24, 0x00},
         5, reinterpret_cast<void*>(&hookMaskAll), &g_maskAll, true},
        {"thingtemplatemaskallatc0", 0x0038E800,
         {0x81, 0xC1, 0xC0, 0x00, 0x00, 0x00, 0xE9, 0xB5,
          0xFF, 0xFF, 0xFF, 0xCC, 0xCC, 0xCC, 0xCC, 0xCC},
         6, reinterpret_cast<void*>(&hookMaskAllAtC0), &g_maskAllAtC0, true},
        {"thingtemplatemaskany", 0x0038E810,
         {0x8B, 0x54, 0x24, 0x04, 0x33, 0xC0, 0x81, 0xC1,
          0xC0, 0x00, 0x00, 0x00, 0x56, 0x8D, 0x49, 0x00},
         6, reinterpret_cast<void*>(&hookMaskAny), &g_maskAny, true},
        {"thingtemplatefindmodule", 0x0038BB10,
         {0x8B, 0x41, 0x14, 0x8B, 0x49, 0x18, 0x3B, 0xC1,
          0x74, 0x11, 0x8B, 0x54, 0x24, 0x04, 0x8B, 0xFF},
         6, reinterpret_cast<void*>(&hookFindModule), &g_findModule, true},
        {"thingtemplateregistrypresent", 0x0038CE90,
         {0x51, 0x8B, 0x44, 0x24, 0x08, 0x56, 0x8B, 0xF1,
          0x8D, 0x4C, 0x24, 0x0C, 0x51, 0x8D, 0x54, 0x24},
         5, reinterpret_cast<void*>(&hookRegistryPresent), &g_registryPresent, true},
    });
}

}  // namespace ra3hook
