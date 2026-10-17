#pragma once

#include <cstdint>

namespace ra3::gamelogic {

// Absolute callees of this cluster. The checker and the offline oracle install
// the same side effects; the addresses are the shipped call targets.
struct Behavior68Calls {
    void(__fastcall* xferBase)(void* self, void* edx, void* xfer);       // 0x008D82F0 ret 4
    void(__fastcall* xferAt10)(void* self, void* edx, void* xfer);       // 0x00685080 ret 4
    void(__fastcall* xferModule)(void* self, void* edx, void* xfer);     // 0x00680850 ret 4
    void(__fastcall* dockDispatch)(void* dock, void* edx, std::uint32_t first,
                                   std::uint32_t second);                // 0x0052DDE0 ret 8
    void(__fastcall* notifyEnergy)(void* target, void* edx, std::uint32_t id); // 0x007F8BC0 ret 4
    void(__cdecl* xferFieldA)(void* xfer, void* field);                  // 0x0054A090
    void(__cdecl* xferFieldB)(void* xfer, void* field);                  // 0x00986340
};

void setBehavior68Calls(const Behavior68Calls& calls) noexcept;

// GarrisonContain vtable 0x00C21E90 slot 10. ret 8. EAX stays the body pointer.
std::uint32_t GarrisonContain_clearReservedIfOccupied(std::uint8_t* self, std::uint32_t,
                                                       std::uint32_t) noexcept;

// OpenContain vtable 0x00C1FB90 slot 10. ret 8.
void OpenContain_clearClosingByte(std::uint8_t* self, std::uint32_t, std::uint32_t) noexcept;

// TransportContain vtable 0x00C200A0 slot 24. ret 4. EAX keeps the high 24 bits of [self+4].
std::uint32_t TransportContain_passengerFlag(std::uint8_t* self, std::uint32_t) noexcept;

// IdleState vtable 0x00BF7A20 slot 5.
std::uint32_t IdleState_busyMask(std::uint8_t* self) noexcept;

// MoveInStraightLineState vtable 0x00C00CF0 slot 3. ret 4.
void MoveInStraightLineState_xfer(std::uint8_t* self, std::uint8_t* xfer) noexcept;

// MoveToGroundState vtable 0x00C00D58 slot 3. ret 4.
void MoveToGroundState_xfer(std::uint8_t* self, std::uint8_t* xfer) noexcept;

// BeamSpecialAbilityUpdate vtable 0x00C1BA58 slot 41.
std::uint32_t BeamSpecialAbilityUpdate_templateFlag(std::uint8_t* self) noexcept;

// DominateEnemySpecialPower vtable 0x00C1C0E8 slot 33. fld m32; ret.
void DominateEnemySpecialPower_storeRadius80(const std::uint8_t* self, std::uint8_t encoded[10]) noexcept;

// ToggleStatusSpecialAbilityUpdate vtable 0x00C1E6F0 slots 30 and 31.
std::uint32_t ToggleStatusSpecialAbilityUpdate_entryPointer(std::uint8_t* self) noexcept;
std::uint32_t ToggleStatusSpecialAbilityUpdate_nextEntryPointer(std::uint8_t* self) noexcept;

// DozerPrimaryIdleState vtable 0x00BF7F08 slot 3. ret 4.
void DozerPrimaryIdleState_xfer(std::uint8_t* self, std::uint8_t* xfer) noexcept;

// SupplyWarehouseDockUpdate vtable 0x00C0C5A8 slot 6.
void SupplyWarehouseDockUpdate_dispatch(std::uint8_t* self) noexcept;

// AIUpdateInterface vtable 0x00C22118.
std::uint32_t AIUpdateInterface_ensureLocomotor(std::uint8_t* self) noexcept;
std::uint32_t AIUpdateInterface_setGoal(std::uint8_t* self, std::uint32_t value) noexcept;
std::uint32_t AIUpdateInterface_setGoalAux(std::uint8_t* self, std::uint32_t value) noexcept;
std::uint32_t AIUpdateInterface_query34(std::uint8_t* self) noexcept;
std::uint32_t AIUpdateInterface_wantsAdvance(std::uint8_t* self) noexcept;
std::uint32_t AIUpdateInterface_query24(std::uint8_t* self) noexcept;
std::uint32_t AIUpdateInterface_query2C(std::uint8_t* self) noexcept;
std::uint32_t AIUpdateInterface_query30(std::uint8_t* self) noexcept;
std::uint32_t AIUpdateInterface_enterState4D(std::uint8_t* self, std::uint32_t arg) noexcept;
std::uint32_t AIUpdateInterface_forwardOrZero20(std::uint8_t* self, std::uint32_t arg) noexcept;
std::uint32_t AIUpdateInterface_forwardOrZero24(std::uint8_t* self, std::uint32_t arg) noexcept;

// LeechTargetingAttachUpdate vtable 0x00C209E8 slot 27. ret 4.
std::uint32_t LeechTargetingAttachUpdate_notify(std::uint8_t* self, std::uint8_t* object) noexcept;

// MissileUpdate vtable 0x00C13F50 slot 9.
void MissileUpdate_clearTimer(std::uint8_t* self) noexcept;

// ProjectileUpdate vtable 0x00BF8CD8 slot 23. ret 4.
// clock is the object published at 0x00DEBA64; secondsPerFrame is the f32 at 0x00DEEEE8.
std::uint32_t ProjectileUpdate_addFrames(std::uint8_t* self, float frames, const std::uint8_t* clock,
                                          float secondsPerFrame) noexcept;

// RepairUpdate vtable 0x00C056A0 slot 9. player is the object at 0x00DF03FC.
std::uint32_t RepairUpdate_notify(std::uint8_t* self, std::uint8_t* player) noexcept;

// SpecialAbilityUpdate vtable 0x00C142D8 slot 28. Shared by many special-ability vtables.
std::uint32_t SpecialAbilityUpdate_countdown(std::uint8_t* self) noexcept;

// EncloseBehavior vtable 0x00C08A48 slots 27 and 23. `self` is the interface at object+0x10.
std::uint32_t EncloseBehavior_relayIfArmed(std::uint8_t* self) noexcept;
std::uint32_t EncloseBehavior_tryOpen(std::uint8_t* self, std::uint32_t arg) noexcept;

// Called from ReconstituteStateSpecialAbility (0x00C1D2B8 slot 32). ret 4.
std::uint32_t findModuleByInterfaceKey(std::uint8_t* self, std::uint32_t key) noexcept;

// Called from StrafeAreaUpdate (0x00BF90F0 slot 23). ret 4.
// Equal-path sign is the batch mutation site.
std::uint32_t storeCoordAndSentinelSign(std::uint8_t* self, const std::uint8_t* coord) noexcept;

// AreaRestrictSpecialPowerBehavior vtable 0x00BF64E0 slot 3. ret 4.
void AreaRestrictSpecialPowerBehavior_xfer(std::uint8_t* self, std::uint8_t* xfer) noexcept;

// SupplyCenterCreate vtable 0x00BF7938 slot 3. ret 4.
void SupplyCenterCreate_xfer(std::uint8_t* self, std::uint8_t* xfer) noexcept;

// AutoAbilityBehavior vtable 0x00C08080 slot 3. ret 4.
void AutoAbilityBehavior_xfer(std::uint8_t* self, std::uint8_t* xfer) noexcept;

// Called from BridgeBuilderBehavior (0x00C1AAB8 slot 3).
void BridgeBuilderBehavior_countSpanSegments(std::uint8_t* self) noexcept;

// DistributedMoneyBehavior vtable 0x00C08870 slot 3. ret 4.
void DistributedMoneyBehavior_xfer(std::uint8_t* self, std::uint8_t* xfer) noexcept;

// EnergyDisablingBehavior vtable 0x00BFAEC0 slot 6.
void EnergyDisablingBehavior_notifyOwner(std::uint8_t* self) noexcept;

// Called from 0x0074AA60, which pushes FireWeaponWhenDeadBehavior.cpp. ret 4. Returns dst.
std::uint8_t* copyPackedDamageBlock(std::uint8_t* dst, const std::uint8_t* src) noexcept;

// Called from UnitAIDeliverToRefineryState (0x00C29FA8 slot 5).
std::uint32_t sumStride8Keys(const std::uint8_t* self) noexcept;

// PlayerPowerManagerBehavior vtable 0x00C11420 slot 3. ret 4.
void PlayerPowerManagerBehavior_xfer(std::uint8_t* self, std::uint8_t* xfer) noexcept;

// SlaveWatcherBehavior vtable 0x00C14738 slot 3. ret 4.
void SlaveWatcherBehavior_xfer(std::uint8_t* self, std::uint8_t* xfer) noexcept;

// StancesBehavior vtable 0x00C11840 slot 3. ret 4.
void StancesBehavior_xfer(std::uint8_t* self, std::uint8_t* xfer) noexcept;

}  // namespace ra3::gamelogic
