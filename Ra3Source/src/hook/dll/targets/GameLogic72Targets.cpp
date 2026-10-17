#include "../Targets.h"

#include <ra3/gamelogic/ModuleIdentity72.h>

#include <cstdint>

namespace ra3hook {
namespace {

TargetState g_opencontainname;
TargetState g_hordecontainname;
TargetState g_aiupdateinterfacename;
TargetState g_dozeraiupdatename;

std::uint32_t __fastcall hook_opencontainname(void* self, void*)
{
    ++g_opencontainname.calls;
    const std::uint32_t ours = ra3::gamelogic::openContainGetModuleName_00727950();
    if (g_opencontainname.mode == Mode::replace) {
        return ours;
    }
    using Original = std::uint32_t(__thiscall*)(void*);
    const std::uint32_t game = reinterpret_cast<Original>(g_opencontainname.original)(self);
    recordScalar("opencontainname", g_opencontainname, game, ours);
    return game;
}

std::uint32_t __fastcall hook_hordecontainname(void* self, void*)
{
    ++g_hordecontainname.calls;
    const std::uint32_t ours = ra3::gamelogic::hordeContainGetModuleName_0073CC60();
    if (g_hordecontainname.mode == Mode::replace) {
        return ours;
    }
    using Original = std::uint32_t(__thiscall*)(void*);
    const std::uint32_t game = reinterpret_cast<Original>(g_hordecontainname.original)(self);
    recordScalar("hordecontainname", g_hordecontainname, game, ours);
    return game;
}

std::uint32_t __fastcall hook_aiupdateinterfacename(void* self, void*)
{
    ++g_aiupdateinterfacename.calls;
    const std::uint32_t ours = ra3::gamelogic::aiUpdateInterfaceGetModuleName_00740F10();
    if (g_aiupdateinterfacename.mode == Mode::replace) {
        return ours;
    }
    using Original = std::uint32_t(__thiscall*)(void*);
    const std::uint32_t game = reinterpret_cast<Original>(g_aiupdateinterfacename.original)(self);
    recordScalar("aiupdateinterfacename", g_aiupdateinterfacename, game, ours);
    return game;
}

std::uint32_t __fastcall hook_dozeraiupdatename(void* self, void*)
{
    ++g_dozeraiupdatename.calls;
    const std::uint32_t ours = ra3::gamelogic::dozerAIUpdateGetModuleName_00751DD0();
    if (g_dozeraiupdatename.mode == Mode::replace) {
        return ours;
    }
    using Original = std::uint32_t(__thiscall*)(void*);
    const std::uint32_t game = reinterpret_cast<Original>(g_dozeraiupdatename.original)(self);
    recordScalar("dozeraiupdatename", g_dozeraiupdatename, game, ours);
    return game;
}

}  // namespace

void appendGameLogic72Targets(std::vector<Target>& table)
{
    table.insert(table.end(), {
        {"opencontainname", 0x00327950,
         {0xB8, 0x30, 0xFC, 0xC1, 0x00, 0xC3, 0xCC, 0xCC, 0xCC, 0xCC, 0xCC, 0xCC, 0xCC, 0xCC, 0xCC, 0xCC},
         5, reinterpret_cast<void*>(&hook_opencontainname), &g_opencontainname, true},
        {"hordecontainname", 0x0033CC60,
         {0xB8, 0x64, 0x1B, 0xC2, 0x00, 0xC3, 0xCC, 0xCC, 0xCC, 0xCC, 0xCC, 0xCC, 0xCC, 0xCC, 0xCC, 0xCC},
         5, reinterpret_cast<void*>(&hook_hordecontainname), &g_hordecontainname, true},
        {"aiupdateinterfacename", 0x00340F10,
         {0xB8, 0xCC, 0x23, 0xC2, 0x00, 0xC3, 0xCC, 0xCC, 0xCC, 0xCC, 0xCC, 0xCC, 0xCC, 0xCC, 0xCC, 0xCC},
         5, reinterpret_cast<void*>(&hook_aiupdateinterfacename), &g_aiupdateinterfacename, true},
        {"dozeraiupdatename", 0x00351DD0,
         {0xB8, 0x04, 0x4D, 0xC2, 0x00, 0xC3, 0xCC, 0xCC, 0xCC, 0xCC, 0xCC, 0xCC, 0xCC, 0xCC, 0xCC, 0xCC},
         5, reinterpret_cast<void*>(&hook_dozeraiupdatename), &g_dozeraiupdatename, true},
    });
}

}  // namespace ra3hook
