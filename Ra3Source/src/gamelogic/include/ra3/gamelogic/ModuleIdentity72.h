#pragma once

#include <cstdint>

namespace ra3::gamelogic {

// Module identity leaves for primary vtables whose slot 2 returns the
// class-name string and whose slot 4 is the MSVC static NAMEKEY cache.
// Generals reference: work/references/generals/GeneralsMD/Code/GameEngine/
// Include/Common/Module.h MAKE_STANDARD_MODULE_MACRO getModuleNameKey.
// The name-key callee 0x00986150 is thiscall and returns 0 for a null name;
// it is a boundary, not part of this batch. The manager pointer is the
// dword at 0x00E1A078.

struct ModuleDataCache {
    std::uint32_t guard = 0;
    std::uint32_t value = 0;
};

struct ModuleIdentityServices {
    std::uint32_t manager = 0;
    std::uint32_t (*nameToKey)(std::uint32_t manager, std::uint32_t nameAddress) = nullptr;
};

extern ModuleIdentityServices g_moduleIdentityServices;

std::uint32_t openContainGetModuleName_00727950() noexcept;
std::uint32_t openContainGetModuleNameKey_00727960(ModuleDataCache& cache) noexcept;
std::uint32_t transportContainGetModuleName_00727E20() noexcept;
std::uint32_t transportContainGetModuleNameKey_00727E30(ModuleDataCache& cache) noexcept;
std::uint32_t delayedWeaponFireSpecialAbilityUpdateGetModuleName_007293F0() noexcept;
std::uint32_t delayedWeaponFireSpecialAbilityUpdateGetModuleNameKey_00729400(ModuleDataCache& cache) noexcept;
std::uint32_t luaSpyDataGetModuleName_0072AFB0() noexcept;
std::uint32_t boobyTrapUpdateGetModuleName_0072F820() noexcept;
std::uint32_t boobyTrapUpdateGetModuleNameKey_0072F830(ModuleDataCache& cache) noexcept;
std::uint32_t gatherEjectUpdateGetModuleName_007304F0() noexcept;
std::uint32_t gatherEjectUpdateGetModuleNameKey_00730500(ModuleDataCache& cache) noexcept;
std::uint32_t leechTargetingAttachUpdateGetModuleName_00730660() noexcept;
std::uint32_t leechTargetingAttachUpdateGetModuleNameKey_00730670(ModuleDataCache& cache) noexcept;
std::uint32_t moneyGainAttachUpdateGetModuleName_00730AF0() noexcept;
std::uint32_t moneyGainAttachUpdateGetModuleNameKey_00730B00(ModuleDataCache& cache) noexcept;
std::uint32_t stealUnitUpdateGetModuleName_00732E30() noexcept;
std::uint32_t stealUnitUpdateGetModuleNameKey_00732E40(ModuleDataCache& cache) noexcept;
std::uint32_t hordeContainGetModuleName_0073CC60() noexcept;
std::uint32_t hordeContainGetModuleNameKey_0073CC70(ModuleDataCache& cache) noexcept;
std::uint32_t garrisonContainGetModuleName_0073DD70() noexcept;
std::uint32_t garrisonContainGetModuleNameKey_0073DD80(ModuleDataCache& cache) noexcept;
std::uint32_t aiUpdateInterfaceGetModuleName_00740F10() noexcept;
std::uint32_t aiUpdateInterfaceGetModuleNameKey_00740F20(ModuleDataCache& cache) noexcept;
std::uint32_t squeezeFlingUpdateGetModuleName_00743760() noexcept;
std::uint32_t squeezeFlingUpdateGetModuleNameKey_00743770(ModuleDataCache& cache) noexcept;
std::uint32_t slaughterHordeContainGetModuleName_0074E7D0() noexcept;
std::uint32_t slaughterHordeContainGetModuleNameKey_0074E7E0(ModuleDataCache& cache) noexcept;
std::uint32_t garrisonStealthContainGetModuleName_0074EA60() noexcept;
std::uint32_t garrisonStealthContainGetModuleNameKey_0074EA70(ModuleDataCache& cache) noexcept;
std::uint32_t healContainGetModuleName_0074EAA0() noexcept;
std::uint32_t healContainGetModuleNameKey_0074EAB0(ModuleDataCache& cache) noexcept;
std::uint32_t tunnelContainGetModuleName_0074FBE0() noexcept;
std::uint32_t tunnelContainGetModuleNameKey_0074FBF0(ModuleDataCache& cache) noexcept;
std::uint32_t animalAIUpdateGetModuleName_00751C00() noexcept;
std::uint32_t animalAIUpdateGetModuleNameKey_00751C10(ModuleDataCache& cache) noexcept;
std::uint32_t assaultTransportAIUpdateGetModuleName_00751C70() noexcept;
std::uint32_t assaultTransportAIUpdateGetModuleNameKey_00751C80(ModuleDataCache& cache) noexcept;
std::uint32_t deployStyleAIUpdateGetModuleName_00751CC0() noexcept;
std::uint32_t deployStyleAIUpdateGetModuleNameKey_00751CD0(ModuleDataCache& cache) noexcept;
std::uint32_t dozerAIUpdateGetModuleName_00751DD0() noexcept;
std::uint32_t dozerAIUpdateGetModuleNameKey_00751DE0(ModuleDataCache& cache) noexcept;
std::uint32_t hordeAIUpdateGetModuleName_00751EA0() noexcept;
std::uint32_t hordeAIUpdateGetModuleNameKey_00751EB0(ModuleDataCache& cache) noexcept;
std::uint32_t supplyTruckAIUpdateGetModuleName_00752330() noexcept;
std::uint32_t supplyTruckAIUpdateGetModuleNameKey_00752340(ModuleDataCache& cache) noexcept;
std::uint32_t transportAIUpdateGetModuleName_00752400() noexcept;
std::uint32_t transportAIUpdateGetModuleNameKey_00752410(ModuleDataCache& cache) noexcept;
std::uint32_t wanderAIUpdateGetModuleName_00752440() noexcept;
std::uint32_t wanderAIUpdateGetModuleNameKey_00752450(ModuleDataCache& cache) noexcept;
std::uint32_t hordeGarrisonContainGetModuleName_0075C870() noexcept;
std::uint32_t hordeGarrisonContainGetModuleNameKey_0075C880(ModuleDataCache& cache) noexcept;

bool moduleIdentityRunName(std::uint32_t va, std::uint32_t* eax) noexcept;
bool moduleIdentityRunKey(std::uint32_t va, ModuleDataCache& cache, std::uint32_t manager,
                           std::uint32_t stubReturn, std::uint32_t* eax, int* called,
                           std::uint32_t* seenManager, std::uint32_t* seenArg) noexcept;

}  // namespace ra3::gamelogic
