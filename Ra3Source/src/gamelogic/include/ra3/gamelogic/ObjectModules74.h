#pragma once

#include <cstdint>

namespace ra3::gamelogic {

// Direct callees of the 0x0074–0x0077 contain, dock, and special-ability
// cluster. The checker and the offline oracle install the same side effects.
// Tags in comments are the shipped targets.
struct Object74Calls {
    std::uint32_t(__fastcall* dockXfer)(void*, void*, std::uint32_t);          // 0x00740690
    std::uint32_t(__fastcall* openContainXfer)(void*, void*, std::uint32_t);   // 0x0074F0E0
    std::uint32_t(__fastcall* garrisonXfer)(void*, void*, std::uint32_t);      // 0x0075D630
    std::uint32_t(__fastcall* behaviorXfer)(void*, void*, std::uint32_t);      // 0x008D82F0
    std::uint32_t(__fastcall* xferFrom10)(void*, void*, std::uint32_t);        // 0x00685080
    std::uint32_t(__fastcall* upgradeXfer)(void*, void*, std::uint32_t);       // 0x006858E0
    std::uint32_t(__fastcall* updateXfer)(void*, void*, std::uint32_t);        // 0x007321F0
    std::uint32_t(__fastcall* damageXfer)(void*, void*, std::uint32_t);        // 0x007302E0
    std::uint32_t(__fastcall* delayedXfer)(void*, void*, std::uint32_t);       // 0x0073F600
    std::uint32_t(__fastcall* supplyXfer)(void*, void*, std::uint32_t);        // 0x0072E810
    std::uint32_t(__fastcall* resizeCount)(void*, void*, std::uint32_t);       // 0x0071D5A0
    std::uint32_t(__fastcall* resizeVector)(void*, void*, std::uint32_t);      // 0x0073A190
    std::uint32_t(__fastcall* missilePrep)(void*, void*, std::uint32_t);       // 0x0073AA60
    std::uint32_t(__fastcall* attachBase)(void*, void*);                       // 0x00741D00
    std::uint32_t(__fastcall* currentTarget)(void*, void*);                    // 0x006EBB60
    std::uint32_t(__fastcall* stealForward)(void*, void*, std::uint32_t);      // 0x00752C00
    std::uint32_t(__fastcall* containAdd)(void*, void*, std::uint32_t);        // 0x0075DF80
    std::uint32_t(__fastcall* recordAdd)(void*, void*, std::uint32_t);         // 0x006DE680
    std::uint32_t(__fastcall* placePassenger)(void*, void*, std::uint32_t);    // 0x0078D1E0
    std::uint32_t(__fastcall* slaughterNote)(void*, void*, std::uint32_t);     // 0x00691AB0
    std::uint32_t(__fastcall* canRemove)(void*, void*, std::uint32_t, std::uint32_t);     // 0x006B0DB0
    std::uint32_t(__fastcall* canRemoveAlt)(void*, void*, std::uint32_t, std::uint32_t);  // 0x006B0BD0
    std::uint32_t(__fastcall* doRemove)(void*, void*, std::uint32_t, std::uint32_t);      // 0x007704B0
    std::uint32_t(__fastcall* spawnOne)(void*, void*, std::uint32_t);          // 0x0074C7A0
    std::uint32_t(__fastcall* clearStatus)(void*, void*, std::uint32_t, std::uint32_t);   // 0x006D1ED0
    std::uint32_t(__fastcall* queryState)(void*, void*);                       // 0x005B9F30
    std::uint32_t(__fastcall* pushBuild)(void*, void*, std::uint32_t, std::uint32_t, std::uint32_t); // 0x007483A0
    std::uint32_t(__fastcall* techApply)(void*, void*, std::uint32_t);         // 0x00755FA0
    std::uint32_t(__fastcall* techRestore)(void*, void*, std::uint32_t);       // 0x00755E80
    std::uint32_t(__fastcall* laserPair)(void*, void*, std::uint32_t, std::uint32_t);     // 0x0075A8F0
    std::uint32_t(__fastcall* laserFinish)(void*, void*);                      // 0x0073B550
    std::uint32_t(__fastcall* prepareSpecial)(void*, void*);                   // 0x007653F0
    std::uint32_t(__fastcall* mountedPrep)(void*, void*);                      // 0x007432E0
    std::uint32_t(__fastcall* hasUpgrade)(void*, void*, std::uint32_t);        // 0x00707050
    std::uint32_t(__fastcall* makeUpgrade)(void*, void*, std::uint32_t, std::uint32_t);   // 0x006FD820
    std::uint32_t(__fastcall* grantUpgrade)(void*, void*, std::uint32_t, std::uint32_t);  // 0x0071C890
    std::uint32_t(__fastcall* locoPrelude)(void*, void*, std::uint32_t, std::uint32_t);   // 0x00718B80
    std::uint32_t(__fastcall* applyMask)(void*, void*, std::uint32_t, std::uint32_t);     // 0x006D2090
    std::uint32_t(__fastcall* rewindStatus)(void*, void*);                     // 0x00775EA0
    std::uint32_t(__fastcall* bindPower)(void*, void*);                        // 0x006C74D0
    std::uint32_t(__fastcall* linkPower)(void*, void*, std::uint32_t);         // 0x0073A270
    std::uint32_t(__fastcall* markObject)(void*, void*);                       // 0x006A4FB0
    std::uint32_t(__fastcall* flingAlt)(void*, void*);                         // 0x00701770
    std::uint32_t(__fastcall* groundBusy)(void*, void*);                       // 0x006B3200
    std::uint32_t(__fastcall* groundAim)(void*, void*, std::uint32_t, std::uint32_t);     // 0x006A5900
    std::uint32_t(__fastcall* groundCheck)(void*, void*);                      // 0x00694000
    std::uint32_t(__fastcall* groundNear)(void*, void*);                       // 0x0077CE10
    std::uint32_t(__fastcall* groundClear)(void*, void*);                      // 0x006941F0
    std::uint32_t(__fastcall* groundStep)(void*, void*);                       // 0x0075EB00
    std::uint32_t(__fastcall* scatterOn)(void*, void*, std::uint32_t);         // 0x004D9AB0
    std::uint32_t(__fastcall* scatterOff)(void*, void*, std::uint32_t);        // 0x004D9AF0
    std::uint32_t(__fastcall* projectileExtra)(void*, void*, std::uint32_t);   // 0x00757200
    std::uint32_t(__fastcall* relay45)(void*, void*, std::uint32_t, std::uint32_t, std::uint32_t); // 0x0072DC90
    std::uint32_t(__fastcall* relay49)(void*, void*, std::uint32_t, std::uint32_t, std::uint32_t); // 0x0072DE40
    std::uint32_t(__fastcall* relay52)(void*, void*, std::uint32_t, std::uint32_t, std::uint32_t); // 0x0072E080
    std::uint32_t(__fastcall* inner45)(void*, void*, std::uint32_t, std::uint32_t, std::uint32_t); // 0x006C5680
    std::uint32_t(__fastcall* inner49)(void*, void*, std::uint32_t, std::uint32_t, std::uint32_t); // 0x006C5710
    std::uint32_t(__fastcall* inner52)(void*, void*, std::uint32_t, std::uint32_t, std::uint32_t); // 0x006C57A0
    std::uint32_t(__cdecl* xferField)(std::uint32_t, std::uint32_t);           // 0x0054A090
    std::uint32_t(__cdecl* xferBlock)(std::uint32_t, std::uint32_t);           // 0x0071D140
    std::uint32_t(__cdecl* xferSpan)(std::uint32_t, std::uint32_t);            // 0x0073A470
    std::uint32_t(__cdecl* rawMove)(std::uint32_t, std::uint32_t, std::uint32_t);
    std::uint32_t(__cdecl* stamp)(std::uint32_t, std::uint32_t, std::uint32_t); // 0x004ED9C0
};

// gameLogic is the object published at 0x00DEBA64. scatterIds is the table at 0x00CC0D58.
struct Object74Env {
    std::uint8_t* gameLogic;
    const std::uint32_t* scatterIds;
};

void setObject74Calls(const Object74Calls& calls) noexcept;
void setObject74Env(const Object74Env& env) noexcept;

// TransportContain vtable 0x00C200A0 slot 3. ret 4.
std::uint32_t TransportContain_xfer(std::uint8_t* self, std::uint8_t* xfer) noexcept;

// HealContain vtable 0x00C235F0 slot 44. ret 4. Also TunnelContain and HordeGarrisonContain.
std::uint32_t HealContain_receivePassenger(std::uint8_t* self, std::uint8_t* passenger) noexcept;

// HordeGarrisonContain vtable 0x00C26980 slot 3. ret 4.
std::uint32_t HordeGarrisonContain_xfer(std::uint8_t* self, std::uint8_t* xfer) noexcept;

// SlaughterHordeContain vtable 0x00C22E28 slot 44. ret 4.
std::uint32_t SlaughterHordeContain_receivePassenger(std::uint8_t* self, std::uint8_t* passenger) noexcept;

// GarrisonStealthContain vtable 0x00C23218 slot 3. ret 4.
std::uint32_t GarrisonStealthContain_xfer(std::uint8_t* self, std::uint8_t* xfer) noexcept;

// HealContain vtable 0x00C235F0 slot 3. ret 4.
std::uint32_t HealContain_xfer(std::uint8_t* self, std::uint8_t* xfer) noexcept;

// TunnelContain vtable 0x00C23A98 slot 3. ret 4.
std::uint32_t TunnelContain_xfer(std::uint8_t* self, std::uint8_t* xfer) noexcept;

// HealContain vtable 0x00C235F0 slot 45. ret 4.
std::uint32_t HealContain_removePassenger(std::uint8_t* self, std::uint8_t* passenger) noexcept;

// SlaughterHordeContain vtable 0x00C22E28 slot 46. ret 4.
std::uint32_t SlaughterHordeContain_removePassenger(std::uint8_t* self, std::uint8_t* passenger) noexcept;

// RailedTransportDockUpdate vtable 0x00C0BF70 slot 3. ret 4.
std::uint32_t RailedTransportDockUpdate_xfer(std::uint8_t* self, std::uint8_t* xfer) noexcept;

// RepairDockUpdate vtable 0x00C0C190 slot 3. ret 4.
std::uint32_t RepairDockUpdate_xfer(std::uint8_t* self, std::uint8_t* xfer) noexcept;

// SupplyCenterDockUpdate vtable 0x00C0C3A8 slot 3. ret 4.
std::uint32_t SupplyCenterDockUpdate_xfer(std::uint8_t* self, std::uint8_t* xfer) noexcept;

// SupplyWarehouseDockUpdate vtable 0x00C0C5A8 slot 3. ret 4.
std::uint32_t SupplyWarehouseDockUpdate_xfer(std::uint8_t* self, std::uint8_t* xfer) noexcept;

// AutoDepositUpdate vtable 0x00C0C920 slot 3. ret 4.
std::uint32_t AutoDepositUpdate_xfer(std::uint8_t* self, std::uint8_t* xfer) noexcept;

// ClusterBombUpdate vtable 0x00C0CE18 slot 3. ret 4.
std::uint32_t ClusterBombUpdate_xfer(std::uint8_t* self, std::uint8_t* xfer) noexcept;

// DamageFieldUpdate vtable 0x00C13A00 slot 3. ret 4.
std::uint32_t DamageFieldUpdate_xfer(std::uint8_t* self, std::uint8_t* xfer) noexcept;

// SupplyTruckAIUpdate vtable 0x00C25398 slot 3. ret 4.
std::uint32_t SupplyTruckAIUpdate_xfer(std::uint8_t* self, std::uint8_t* xfer) noexcept;

// ObjectStatusSpecialAbilityUpdate vtable 0x00C1EBC8 slot 3. ret 4.
std::uint32_t ObjectStatusSpecialAbilityUpdate_xfer(std::uint8_t* self, std::uint8_t* xfer) noexcept;

// OCLMonitorSpecialAbilityUpdate vtable 0x00C1EE08 slot 3. ret 4.
std::uint32_t OCLMonitorSpecialAbilityUpdate_xfer(std::uint8_t* self, std::uint8_t* xfer) noexcept;

// ReplaceObjectUpdate vtable 0x00C1F0A8 slot 3. ret 4.
std::uint32_t ReplaceObjectUpdate_xfer(std::uint8_t* self, std::uint8_t* xfer) noexcept;

// SubStructureBuildUpgrade vtable 0x00C108B0 slot 3. ret 4.
std::uint32_t SubStructureBuildUpgrade_xfer(std::uint8_t* self, std::uint8_t* xfer) noexcept;

// DelayedWeaponFireSpecialAbilityUpdate vtable 0x00C202F0 slot 3. ret 4.
std::uint32_t DelayedWeaponFireSpecialAbilityUpdate_xfer(std::uint8_t* self, std::uint8_t* xfer) noexcept;

// AIUpdateInterface vtable 0x00C22118 slot 138. ret 4.
std::uint32_t AIUpdateInterface_storeGoalPosition(std::uint8_t* self, const std::uint8_t* coord) noexcept;

// MissileUpdate vtable 0x00C13F50 slot 22. ret 4.
std::uint32_t MissileUpdate_arm(std::uint8_t* self, std::uint32_t arg) noexcept;

// StealUnitUpdate vtable 0x00C20D90 slot 24.
std::uint32_t StealUnitUpdate_latch(std::uint8_t* self) noexcept;

// SpawnBehavior vtable 0x00C11688 slot 10. ret 8. The two arguments are unused.
std::uint32_t SpawnBehavior_prime(std::uint8_t* self, std::uint32_t, std::uint32_t) noexcept;

// ToggleMountedSpecialAbilityUpdate vtable 0x00C1E498 slot 29.
std::uint32_t ToggleMountedSpecialAbilityUpdate_clear(std::uint8_t* self) noexcept;

// DozerAIUpdate vtable 0x00C24A50 slot 166.
std::uint32_t DozerAIUpdate_routeBuild(std::uint8_t* self) noexcept;

// StealUnitUpdate vtable 0x00C20D90 slot 23. ret 4.
std::uint32_t StealUnitUpdate_relay(std::uint8_t* self, std::uint32_t arg) noexcept;

// TechUpgrade vtable 0x00C10AA0 slot 9. ecx is the interface at object+4.
std::uint32_t TechUpgrade_finish(std::uint8_t* self) noexcept;

// TechUpgrade vtable 0x00C10AA0 slot 10. ret 8.
std::uint32_t TechUpgrade_grant(std::uint8_t* self, std::uint32_t first, std::uint32_t second) noexcept;

// LaserStateModule vtable 0x00C1AE58 slot 22. ret 8. The second argument is unused.
std::uint32_t LaserStateModule_schedule(std::uint8_t* self, std::uint32_t extra, std::uint32_t) noexcept;

// GiveOrRestoreUpgradeSpecialPower vtable 0x00C1C9D8 slot 27.
std::uint32_t GiveOrRestoreUpgradeSpecialPower_apply(std::uint8_t* self) noexcept;

// SwitchLocomotorsSpecialAbilityUpdate vtable 0x00C1D960 slot 36. ret 8.
std::uint32_t SwitchLocomotorsSpecialAbilityUpdate_commit(std::uint8_t* self, std::uint32_t arg,
                                                          std::uint32_t flag) noexcept;

// ToggleDeploySpecialAbilityUpdate vtable 0x00C1E010 slot 27.
std::uint32_t ToggleDeploySpecialAbilityUpdate_flip(std::uint8_t* self) noexcept;

// ToggleHiddenSpecialAbilityUpdate vtable 0x00C1E258 slot 27.
std::uint32_t ToggleHiddenSpecialAbilityUpdate_flip(std::uint8_t* self) noexcept;

// ToggleStatusSpecialAbilityUpdate vtable 0x00C1E6F0 slot 43.
std::uint32_t ToggleStatusSpecialAbilityUpdate_rewind(std::uint8_t* self) noexcept;

// ObjectStatusSpecialAbilityUpdate vtable 0x00C1EBC8 slot 27.
std::uint32_t ObjectStatusSpecialAbilityUpdate_swap(std::uint8_t* self) noexcept;

// MoveToGroundState vtable 0x00C00D58 slot 5.
std::uint32_t MoveToGroundState_update(std::uint8_t* self) noexcept;

// TransportAIUpdate vtable 0x00C257C0 slots 45, 49, and 52. ret 0xC.
std::uint32_t TransportAIUpdate_notifyPassengers45(std::uint8_t* self, std::uint32_t a0, std::uint32_t a1,
                                                   std::uint32_t a2) noexcept;
std::uint32_t TransportAIUpdate_notifyPassengers49(std::uint8_t* self, std::uint32_t a0, std::uint32_t a1,
                                                   std::uint32_t a2) noexcept;
std::uint32_t TransportAIUpdate_notifyPassengers52(std::uint8_t* self, std::uint32_t a0, std::uint32_t a1,
                                                   std::uint32_t a2) noexcept;

// PlayerPowerManagerBehavior vtable 0x00C11420 slot 1. The same body is also slot 6.
std::uint32_t PlayerPowerManagerBehavior_refresh(std::uint8_t* self) noexcept;

// FlingPassengerSpecialAbilityUpdate vtable 0x00C1C798 slot 27.
std::uint32_t FlingPassengerSpecialAbilityUpdate_fling(std::uint8_t* self) noexcept;

// WeaponScatterEffectUpdate vtable 0x00C0E5D0 slot 31. ecx is the interface.
std::uint32_t WeaponScatterEffectUpdate_apply(std::uint8_t* self) noexcept;

// ProjectileUpdate vtable 0x00BF8CD8 slot 22. ret 0xC.
std::uint32_t ProjectileUpdate_launch(std::uint8_t* self, std::uint8_t* source, const std::uint8_t* coord,
                                      std::uint32_t extra) noexcept;

}  // namespace ra3::gamelogic
