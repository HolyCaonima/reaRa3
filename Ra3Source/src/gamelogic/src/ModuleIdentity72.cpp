#include "ra3/gamelogic/ModuleIdentity72.h"

namespace ra3::gamelogic {
namespace {

std::uint32_t g_stubReturn = 0;
int g_called = 0;
std::uint32_t g_seenManager = 0;
std::uint32_t g_seenArg = 0;

std::uint32_t stubNameToKey(std::uint32_t manager, std::uint32_t nameAddress) noexcept
{
    ++g_called;
    g_seenManager = manager;
    g_seenArg = nameAddress;
    return g_stubReturn;
}

// Shipped body: test byte [guard] & 1; on miss, ecx = [0x00E1A078], or dword [guard], 1,
// thiscall 0x00986150(name), store eax to [cache]. The ready bit is the comparison
// the oracle mutant flips.
std::uint32_t moduleDataKey(ModuleDataCache& cache, std::uint32_t nameAddress) noexcept
{
#if defined(RA3_GAMELOGIC72_ORACLE_MUTANT)
    constexpr std::uint32_t kReadyMask = 2u;
#else
    constexpr std::uint32_t kReadyMask = 1u;
#endif
    if ((cache.guard & kReadyMask) != 0) {
        return cache.value;
    }
    const std::uint32_t manager = g_moduleIdentityServices.manager;
    cache.guard |= 1u;
    const std::uint32_t key = g_moduleIdentityServices.nameToKey(manager, nameAddress);
    cache.value = key;
    return key;
}

}  // namespace

ModuleIdentityServices g_moduleIdentityServices{0, &stubNameToKey};

// OpenContain::getModuleName
// 00C1FB90 slot 2; slot2 name "OpenContain" at 00C1FC30.
std::uint32_t openContainGetModuleName_00727950() noexcept
{
    return 0x00C1FC30u;
}

// OpenContain::getModuleNameKey
// 00C1FB90 slot 4; slot2 name "OpenContain" at 00C1FC30.
// guard dword 00DF9948, cache dword 00DF9944, manager dword 00E1A078.
std::uint32_t openContainGetModuleNameKey_00727960(ModuleDataCache& cache) noexcept
{
    return moduleDataKey(cache, 0x00C1FC30u);
}

// TransportContain::getModuleName
// 00C200A0 slot 2; slot2 name "TransportContain" at 00C20150.
std::uint32_t transportContainGetModuleName_00727E20() noexcept
{
    return 0x00C20150u;
}

// TransportContain::getModuleNameKey
// 00C200A0 slot 4; slot2 name "TransportContain" at 00C20150.
// guard dword 00DF9950, cache dword 00DF994C, manager dword 00E1A078.
std::uint32_t transportContainGetModuleNameKey_00727E30(ModuleDataCache& cache) noexcept
{
    return moduleDataKey(cache, 0x00C20150u);
}

// DelayedWeaponFireSpecialAbilityUpdate::getModuleName
// 00C202F0 slot 2; slot2 name "DelayedWeaponFireSpecialAbilityUpdate" at 00C2039C.
std::uint32_t delayedWeaponFireSpecialAbilityUpdateGetModuleName_007293F0() noexcept
{
    return 0x00C2039Cu;
}

// DelayedWeaponFireSpecialAbilityUpdate::getModuleNameKey
// 00C202F0 slot 4; slot2 name "DelayedWeaponFireSpecialAbilityUpdate" at 00C2039C.
// guard dword 00DF9958, cache dword 00DF9954, manager dword 00E1A078.
std::uint32_t delayedWeaponFireSpecialAbilityUpdateGetModuleNameKey_00729400(ModuleDataCache& cache) noexcept
{
    return moduleDataKey(cache, 0x00C2039Cu);
}

// LuaSpyData::getModuleName
// 00C20460 slot 2; slot2 name "LuaSpyData" at 00C20470.
std::uint32_t luaSpyDataGetModuleName_0072AFB0() noexcept
{
    return 0x00C20470u;
}

// BoobyTrapUpdate::getModuleName
// 00C20660 slot 2; slot2 name "BoobyTrapUpdate" at 00C206E4.
std::uint32_t boobyTrapUpdateGetModuleName_0072F820() noexcept
{
    return 0x00C206E4u;
}

// BoobyTrapUpdate::getModuleNameKey
// 00C20660 slot 4; slot2 name "BoobyTrapUpdate" at 00C206E4.
// guard dword 00DF9984, cache dword 00DF9980, manager dword 00E1A078.
std::uint32_t boobyTrapUpdateGetModuleNameKey_0072F830(ModuleDataCache& cache) noexcept
{
    return moduleDataKey(cache, 0x00C206E4u);
}

// GatherEjectUpdate::getModuleName
// 00C20848 slot 2; slot2 name "GatherEjectUpdate" at 00BF8904.
std::uint32_t gatherEjectUpdateGetModuleName_007304F0() noexcept
{
    return 0x00BF8904u;
}

// GatherEjectUpdate::getModuleNameKey
// 00C20848 slot 4; slot2 name "GatherEjectUpdate" at 00BF8904.
// guard dword 00DF998C, cache dword 00DF9988, manager dword 00E1A078.
std::uint32_t gatherEjectUpdateGetModuleNameKey_00730500(ModuleDataCache& cache) noexcept
{
    return moduleDataKey(cache, 0x00BF8904u);
}

// LeechTargetingAttachUpdate::getModuleName
// 00C209E8 slot 2; slot2 name "LeechTargetingAttachUpdate" at 00C20A5C.
std::uint32_t leechTargetingAttachUpdateGetModuleName_00730660() noexcept
{
    return 0x00C20A5Cu;
}

// LeechTargetingAttachUpdate::getModuleNameKey
// 00C209E8 slot 4; slot2 name "LeechTargetingAttachUpdate" at 00C20A5C.
// guard dword 00DF9994, cache dword 00DF9990, manager dword 00E1A078.
std::uint32_t leechTargetingAttachUpdateGetModuleNameKey_00730670(ModuleDataCache& cache) noexcept
{
    return moduleDataKey(cache, 0x00C20A5Cu);
}

// MoneyGainAttachUpdate::getModuleName
// 00C20BC0 slot 2; slot2 name "MoneyGainAttachUpdate" at 00C20C34.
std::uint32_t moneyGainAttachUpdateGetModuleName_00730AF0() noexcept
{
    return 0x00C20C34u;
}

// MoneyGainAttachUpdate::getModuleNameKey
// 00C20BC0 slot 4; slot2 name "MoneyGainAttachUpdate" at 00C20C34.
// guard dword 00DF999C, cache dword 00DF9998, manager dword 00E1A078.
std::uint32_t moneyGainAttachUpdateGetModuleNameKey_00730B00(ModuleDataCache& cache) noexcept
{
    return moduleDataKey(cache, 0x00C20C34u);
}

// StealUnitUpdate::getModuleName
// 00C20D90 slot 2; slot2 name "StealUnitUpdate" at 00C20E14.
std::uint32_t stealUnitUpdateGetModuleName_00732E30() noexcept
{
    return 0x00C20E14u;
}

// StealUnitUpdate::getModuleNameKey
// 00C20D90 slot 4; slot2 name "StealUnitUpdate" at 00C20E14.
// guard dword 00DF99BC, cache dword 00DF99B8, manager dword 00E1A078.
std::uint32_t stealUnitUpdateGetModuleNameKey_00732E40(ModuleDataCache& cache) noexcept
{
    return moduleDataKey(cache, 0x00C20E14u);
}

// HordeContain::getModuleName
// 00C21A98 slot 2; slot2 name "HordeContain" at 00C21B64.
std::uint32_t hordeContainGetModuleName_0073CC60() noexcept
{
    return 0x00C21B64u;
}

// HordeContain::getModuleNameKey
// 00C21A98 slot 4; slot2 name "HordeContain" at 00C21B64.
// guard dword 00DF9ABC, cache dword 00DF9AB8, manager dword 00E1A078.
std::uint32_t hordeContainGetModuleNameKey_0073CC70(ModuleDataCache& cache) noexcept
{
    return moduleDataKey(cache, 0x00C21B64u);
}

// GarrisonContain::getModuleName
// 00C21E90 slot 2; slot2 name "GarrisonContain" at 00C21F3C.
std::uint32_t garrisonContainGetModuleName_0073DD70() noexcept
{
    return 0x00C21F3Cu;
}

// GarrisonContain::getModuleNameKey
// 00C21E90 slot 4; slot2 name "GarrisonContain" at 00C21F3C.
// guard dword 00DF9AC4, cache dword 00DF9AC0, manager dword 00E1A078.
std::uint32_t garrisonContainGetModuleNameKey_0073DD80(ModuleDataCache& cache) noexcept
{
    return moduleDataKey(cache, 0x00C21F3Cu);
}

// AIUpdateInterface::getModuleName
// 00C22118 slot 2; slot2 name "AIUpdateInterface" at 00C223CC.
std::uint32_t aiUpdateInterfaceGetModuleName_00740F10() noexcept
{
    return 0x00C223CCu;
}

// AIUpdateInterface::getModuleNameKey
// 00C22118 slot 4; slot2 name "AIUpdateInterface" at 00C223CC.
// guard dword 00DF9ACC, cache dword 00DF9AC8, manager dword 00E1A078.
std::uint32_t aiUpdateInterfaceGetModuleNameKey_00740F20(ModuleDataCache& cache) noexcept
{
    return moduleDataKey(cache, 0x00C223CCu);
}

// SqueezeFlingUpdate::getModuleName
// 00C225F0 slot 2; slot2 name "SqueezeFlingUpdate" at 00C22658.
std::uint32_t squeezeFlingUpdateGetModuleName_00743760() noexcept
{
    return 0x00C22658u;
}

// SqueezeFlingUpdate::getModuleNameKey
// 00C225F0 slot 4; slot2 name "SqueezeFlingUpdate" at 00C22658.
// guard dword 00DF9AE4, cache dword 00DF9AE0, manager dword 00E1A078.
std::uint32_t squeezeFlingUpdateGetModuleNameKey_00743770(ModuleDataCache& cache) noexcept
{
    return moduleDataKey(cache, 0x00C22658u);
}

// SlaughterHordeContain::getModuleName
// 00C22E28 slot 2; slot2 name "SlaughterHordeContain" at 00C22EEC.
std::uint32_t slaughterHordeContainGetModuleName_0074E7D0() noexcept
{
    return 0x00C22EECu;
}

// SlaughterHordeContain::getModuleNameKey
// 00C22E28 slot 4; slot2 name "SlaughterHordeContain" at 00C22EEC.
// guard dword 00DF9B68, cache dword 00DF9B64, manager dword 00E1A078.
std::uint32_t slaughterHordeContainGetModuleNameKey_0074E7E0(ModuleDataCache& cache) noexcept
{
    return moduleDataKey(cache, 0x00C22EECu);
}

// GarrisonStealthContain::getModuleName
// 00C23218 slot 2; slot2 name "GarrisonStealthContain" at 00C232C4.
std::uint32_t garrisonStealthContainGetModuleName_0074EA60() noexcept
{
    return 0x00C232C4u;
}

// GarrisonStealthContain::getModuleNameKey
// 00C23218 slot 4; slot2 name "GarrisonStealthContain" at 00C232C4.
// guard dword 00DF9B70, cache dword 00DF9B6C, manager dword 00E1A078.
std::uint32_t garrisonStealthContainGetModuleNameKey_0074EA70(ModuleDataCache& cache) noexcept
{
    return moduleDataKey(cache, 0x00C232C4u);
}

// HealContain::getModuleName
// 00C235F0 slot 2; slot2 name "HealContain" at 00C236AC.
std::uint32_t healContainGetModuleName_0074EAA0() noexcept
{
    return 0x00C236ACu;
}

// HealContain::getModuleNameKey
// 00C235F0 slot 4; slot2 name "HealContain" at 00C236AC.
// guard dword 00DF9B78, cache dword 00DF9B74, manager dword 00E1A078.
std::uint32_t healContainGetModuleNameKey_0074EAB0(ModuleDataCache& cache) noexcept
{
    return moduleDataKey(cache, 0x00C236ACu);
}

// TunnelContain::getModuleName
// 00C23A98 slot 2; slot2 name "TunnelContain" at 00C23B5C.
std::uint32_t tunnelContainGetModuleName_0074FBE0() noexcept
{
    return 0x00C23B5Cu;
}

// TunnelContain::getModuleNameKey
// 00C23A98 slot 4; slot2 name "TunnelContain" at 00C23B5C.
// guard dword 00DF9B80, cache dword 00DF9B7C, manager dword 00E1A078.
std::uint32_t tunnelContainGetModuleNameKey_0074FBF0(ModuleDataCache& cache) noexcept
{
    return moduleDataKey(cache, 0x00C23B5Cu);
}

// AnimalAIUpdate::getModuleName
// 00C23D58 slot 2; slot2 name "AnimalAIUpdate" at 00C2400C.
std::uint32_t animalAIUpdateGetModuleName_00751C00() noexcept
{
    return 0x00C2400Cu;
}

// AnimalAIUpdate::getModuleNameKey
// 00C23D58 slot 4; slot2 name "AnimalAIUpdate" at 00C2400C.
// guard dword 00DF9B88, cache dword 00DF9B84, manager dword 00E1A078.
std::uint32_t animalAIUpdateGetModuleNameKey_00751C10(ModuleDataCache& cache) noexcept
{
    return moduleDataKey(cache, 0x00C2400Cu);
}

// AssaultTransportAIUpdate::getModuleName
// 00C24180 slot 2; slot2 name "AssaultTransportAIUpdate" at 00C24434.
std::uint32_t assaultTransportAIUpdateGetModuleName_00751C70() noexcept
{
    return 0x00C24434u;
}

// AssaultTransportAIUpdate::getModuleNameKey
// 00C24180 slot 4; slot2 name "AssaultTransportAIUpdate" at 00C24434.
// guard dword 00DF9B90, cache dword 00DF9B8C, manager dword 00E1A078.
std::uint32_t assaultTransportAIUpdateGetModuleNameKey_00751C80(ModuleDataCache& cache) noexcept
{
    return moduleDataKey(cache, 0x00C24434u);
}

// DeployStyleAIUpdate::getModuleName
// 00C245B0 slot 2; slot2 name "DeployStyleAIUpdate" at 00C24864.
std::uint32_t deployStyleAIUpdateGetModuleName_00751CC0() noexcept
{
    return 0x00C24864u;
}

// DeployStyleAIUpdate::getModuleNameKey
// 00C245B0 slot 4; slot2 name "DeployStyleAIUpdate" at 00C24864.
// guard dword 00DF9B98, cache dword 00DF9B94, manager dword 00E1A078.
std::uint32_t deployStyleAIUpdateGetModuleNameKey_00751CD0(ModuleDataCache& cache) noexcept
{
    return moduleDataKey(cache, 0x00C24864u);
}

// DozerAIUpdate::getModuleName
// 00C24A50 slot 2; slot2 name "DozerAIUpdate" at 00C24D04.
std::uint32_t dozerAIUpdateGetModuleName_00751DD0() noexcept
{
    return 0x00C24D04u;
}

// DozerAIUpdate::getModuleNameKey
// 00C24A50 slot 4; slot2 name "DozerAIUpdate" at 00C24D04.
// guard dword 00DF9BA0, cache dword 00DF9B9C, manager dword 00E1A078.
std::uint32_t dozerAIUpdateGetModuleNameKey_00751DE0(ModuleDataCache& cache) noexcept
{
    return moduleDataKey(cache, 0x00C24D04u);
}

// HordeAIUpdate::getModuleName
// 00C24EE0 slot 2; slot2 name "HordeAIUpdate" at 00C25194.
std::uint32_t hordeAIUpdateGetModuleName_00751EA0() noexcept
{
    return 0x00C25194u;
}

// HordeAIUpdate::getModuleNameKey
// 00C24EE0 slot 4; slot2 name "HordeAIUpdate" at 00C25194.
// guard dword 00DF9BA8, cache dword 00DF9BA4, manager dword 00E1A078.
std::uint32_t hordeAIUpdateGetModuleNameKey_00751EB0(ModuleDataCache& cache) noexcept
{
    return moduleDataKey(cache, 0x00C25194u);
}

// SupplyTruckAIUpdate::getModuleName
// 00C25398 slot 2; slot2 name "SupplyTruckAIUpdate" at 00C2564C.
std::uint32_t supplyTruckAIUpdateGetModuleName_00752330() noexcept
{
    return 0x00C2564Cu;
}

// SupplyTruckAIUpdate::getModuleNameKey
// 00C25398 slot 4; slot2 name "SupplyTruckAIUpdate" at 00C2564C.
// guard dword 00DF9BB0, cache dword 00DF9BAC, manager dword 00E1A078.
std::uint32_t supplyTruckAIUpdateGetModuleNameKey_00752340(ModuleDataCache& cache) noexcept
{
    return moduleDataKey(cache, 0x00C2564Cu);
}

// TransportAIUpdate::getModuleName
// 00C257C0 slot 2; slot2 name "TransportAIUpdate" at 00C25A74.
std::uint32_t transportAIUpdateGetModuleName_00752400() noexcept
{
    return 0x00C25A74u;
}

// TransportAIUpdate::getModuleNameKey
// 00C257C0 slot 4; slot2 name "TransportAIUpdate" at 00C25A74.
// guard dword 00DF9BB8, cache dword 00DF9BB4, manager dword 00E1A078.
std::uint32_t transportAIUpdateGetModuleNameKey_00752410(ModuleDataCache& cache) noexcept
{
    return moduleDataKey(cache, 0x00C25A74u);
}

// WanderAIUpdate::getModuleName
// 00C25BE8 slot 2; slot2 name "WanderAIUpdate" at 00C25E9C.
std::uint32_t wanderAIUpdateGetModuleName_00752440() noexcept
{
    return 0x00C25E9Cu;
}

// WanderAIUpdate::getModuleNameKey
// 00C25BE8 slot 4; slot2 name "WanderAIUpdate" at 00C25E9C.
// guard dword 00DF9BC0, cache dword 00DF9BBC, manager dword 00E1A078.
std::uint32_t wanderAIUpdateGetModuleNameKey_00752450(ModuleDataCache& cache) noexcept
{
    return moduleDataKey(cache, 0x00C25E9Cu);
}

// HordeGarrisonContain::getModuleName
// 00C26980 slot 2; slot2 name "HordeGarrisonContain" at 00C26A3C.
std::uint32_t hordeGarrisonContainGetModuleName_0075C870() noexcept
{
    return 0x00C26A3Cu;
}

// HordeGarrisonContain::getModuleNameKey
// 00C26980 slot 4; slot2 name "HordeGarrisonContain" at 00C26A3C.
// guard dword 00DF9BF0, cache dword 00DF9BEC, manager dword 00E1A078.
std::uint32_t hordeGarrisonContainGetModuleNameKey_0075C880(ModuleDataCache& cache) noexcept
{
    return moduleDataKey(cache, 0x00C26A3Cu);
}

namespace {

struct NameEntry {
    std::uint32_t va;
    std::uint32_t (*fn)();
};

struct KeyEntry {
    std::uint32_t va;
    std::uint32_t (*fn)(ModuleDataCache&);
};

const NameEntry kNames[] = {
    {0x00727950u, &openContainGetModuleName_00727950},
    {0x00727E20u, &transportContainGetModuleName_00727E20},
    {0x007293F0u, &delayedWeaponFireSpecialAbilityUpdateGetModuleName_007293F0},
    {0x0072AFB0u, &luaSpyDataGetModuleName_0072AFB0},
    {0x0072F820u, &boobyTrapUpdateGetModuleName_0072F820},
    {0x007304F0u, &gatherEjectUpdateGetModuleName_007304F0},
    {0x00730660u, &leechTargetingAttachUpdateGetModuleName_00730660},
    {0x00730AF0u, &moneyGainAttachUpdateGetModuleName_00730AF0},
    {0x00732E30u, &stealUnitUpdateGetModuleName_00732E30},
    {0x0073CC60u, &hordeContainGetModuleName_0073CC60},
    {0x0073DD70u, &garrisonContainGetModuleName_0073DD70},
    {0x00740F10u, &aiUpdateInterfaceGetModuleName_00740F10},
    {0x00743760u, &squeezeFlingUpdateGetModuleName_00743760},
    {0x0074E7D0u, &slaughterHordeContainGetModuleName_0074E7D0},
    {0x0074EA60u, &garrisonStealthContainGetModuleName_0074EA60},
    {0x0074EAA0u, &healContainGetModuleName_0074EAA0},
    {0x0074FBE0u, &tunnelContainGetModuleName_0074FBE0},
    {0x00751C00u, &animalAIUpdateGetModuleName_00751C00},
    {0x00751C70u, &assaultTransportAIUpdateGetModuleName_00751C70},
    {0x00751CC0u, &deployStyleAIUpdateGetModuleName_00751CC0},
    {0x00751DD0u, &dozerAIUpdateGetModuleName_00751DD0},
    {0x00751EA0u, &hordeAIUpdateGetModuleName_00751EA0},
    {0x00752330u, &supplyTruckAIUpdateGetModuleName_00752330},
    {0x00752400u, &transportAIUpdateGetModuleName_00752400},
    {0x00752440u, &wanderAIUpdateGetModuleName_00752440},
    {0x0075C870u, &hordeGarrisonContainGetModuleName_0075C870},
};

const KeyEntry kKeys[] = {
    {0x00727960u, &openContainGetModuleNameKey_00727960},
    {0x00727E30u, &transportContainGetModuleNameKey_00727E30},
    {0x00729400u, &delayedWeaponFireSpecialAbilityUpdateGetModuleNameKey_00729400},
    {0x0072F830u, &boobyTrapUpdateGetModuleNameKey_0072F830},
    {0x00730500u, &gatherEjectUpdateGetModuleNameKey_00730500},
    {0x00730670u, &leechTargetingAttachUpdateGetModuleNameKey_00730670},
    {0x00730B00u, &moneyGainAttachUpdateGetModuleNameKey_00730B00},
    {0x00732E40u, &stealUnitUpdateGetModuleNameKey_00732E40},
    {0x0073CC70u, &hordeContainGetModuleNameKey_0073CC70},
    {0x0073DD80u, &garrisonContainGetModuleNameKey_0073DD80},
    {0x00740F20u, &aiUpdateInterfaceGetModuleNameKey_00740F20},
    {0x00743770u, &squeezeFlingUpdateGetModuleNameKey_00743770},
    {0x0074E7E0u, &slaughterHordeContainGetModuleNameKey_0074E7E0},
    {0x0074EA70u, &garrisonStealthContainGetModuleNameKey_0074EA70},
    {0x0074EAB0u, &healContainGetModuleNameKey_0074EAB0},
    {0x0074FBF0u, &tunnelContainGetModuleNameKey_0074FBF0},
    {0x00751C10u, &animalAIUpdateGetModuleNameKey_00751C10},
    {0x00751C80u, &assaultTransportAIUpdateGetModuleNameKey_00751C80},
    {0x00751CD0u, &deployStyleAIUpdateGetModuleNameKey_00751CD0},
    {0x00751DE0u, &dozerAIUpdateGetModuleNameKey_00751DE0},
    {0x00751EB0u, &hordeAIUpdateGetModuleNameKey_00751EB0},
    {0x00752340u, &supplyTruckAIUpdateGetModuleNameKey_00752340},
    {0x00752410u, &transportAIUpdateGetModuleNameKey_00752410},
    {0x00752450u, &wanderAIUpdateGetModuleNameKey_00752450},
    {0x0075C880u, &hordeGarrisonContainGetModuleNameKey_0075C880},
};

}  // namespace

bool moduleIdentityRunName(std::uint32_t va, std::uint32_t* eax) noexcept
{
    for (const NameEntry& entry : kNames) {
        if (entry.va == va) {
            *eax = entry.fn();
            return true;
        }
    }
    return false;
}

bool moduleIdentityRunKey(std::uint32_t va, ModuleDataCache& cache, std::uint32_t manager,
                           std::uint32_t stubReturn, std::uint32_t* eax, int* called,
                           std::uint32_t* seenManager, std::uint32_t* seenArg) noexcept
{
    for (const KeyEntry& entry : kKeys) {
        if (entry.va != va) {
            continue;
        }
        g_called = 0;
        g_seenManager = 0;
        g_seenArg = 0;
        g_stubReturn = stubReturn;
        g_moduleIdentityServices.manager = manager;
        g_moduleIdentityServices.nameToKey = &stubNameToKey;
        *eax = entry.fn(cache);
        *called = g_called;
        *seenManager = g_seenManager;
        *seenArg = g_seenArg;
        return true;
    }
    return false;
}

}  // namespace ra3::gamelogic
