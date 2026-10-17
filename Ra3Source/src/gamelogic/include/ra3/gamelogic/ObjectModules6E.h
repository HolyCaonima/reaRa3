#pragma once

#include <cstdint>

namespace ra3::gamelogic {

// Absolute callees of the 0x006E0000–0x006FFFFF object-module cluster.
// The checker and the offline oracle install the same side effects.
struct Object6ECalls {
    void(__fastcall* lock)(void*, void*);
    void(__fastcall* unlock)(void*, void*);
    std::uint32_t(__fastcall* find)(void*, void*, std::uint32_t key);
    std::uint32_t(__fastcall* d4d3b50)(void*, void*, std::uint32_t);
    void(__fastcall* d6a4fb0)(void*, void*);
    std::uint32_t(__fastcall* d687720)(void*, void*, std::uint32_t);
    std::uint32_t(__fastcall* d6d1ed0)(void*, void*, std::uint32_t, std::uint32_t);
    std::uint32_t(__fastcall* d687560)(void*, void*);
    void(__fastcall* d54f490)(void*, void*, std::uint32_t);
    std::uint32_t(__fastcall* d6c0ed0)(void*, void*);
    std::uint32_t(__fastcall* d5ac4f0)(void*, void*, std::uint32_t, std::uint32_t);
    std::uint32_t(__fastcall* d6cc2e0)(void*, void*, std::uint32_t);
    std::uint32_t(__fastcall* d6cace0)(void*, void*, std::uint32_t, std::uint32_t, std::uint32_t);
    std::uint32_t(__fastcall* d6a7b60)(void*, void*, std::uint32_t, std::uint32_t);
    std::uint32_t(__fastcall* d6cab20)(void*, void*);
    std::uint32_t(__fastcall* d6ead80)(void*, void*);
    void(__fastcall* d6e6830)(void*, void*, std::uint32_t);
    void(__fastcall* d8d82f0)(void*, void*, std::uint32_t);
    void(__fastcall* d685080)(void*, void*, std::uint32_t);
    std::uint32_t(__fastcall* d6e2470)(void*, void*, std::uint32_t);
    std::uint32_t(__fastcall* d6d3a00)(void*, void*, std::uint32_t);
    std::uint32_t(__fastcall* d6d3870)(void*, void*, std::uint32_t);
    void(__fastcall* d6a8400)(void*, void*);
    void(__fastcall* d6bcf30)(void*, void*, std::uint32_t);
    void(__fastcall* d6d0330)(void*, void*);
    std::uint32_t(__fastcall* d6a88c0)(void*, void*, std::uint32_t);
    std::uint32_t(__fastcall* d6d6ec0)(void*, void*);
    std::uint32_t(__fastcall* d6d9560)(void*, void*, std::uint32_t);
    std::uint32_t(__fastcall* d6efe40)(void*, void*);
    std::uint32_t(__fastcall* daafe10)(void*, void*);
    std::uint32_t(__fastcall* d6f2140)(void*, void*, std::uint32_t, std::uint32_t, std::uint32_t,
                                       std::uint32_t);
    std::uint32_t(__fastcall* d6fa190)(void*, void*, std::uint32_t);
    std::uint32_t(__fastcall* d6e0450)(void*, void*, std::uint32_t);
    std::uint32_t(__fastcall* d6e1630)(void*, void*);
    std::uint32_t(__fastcall* d6eaef0)(void*, void*);
    void(__fastcall* d6ff750)(void*, void*);
    void(__fastcall* d52dde0)(void*, void*, std::uint32_t, std::uint32_t);
    std::uint32_t(__cdecl* d6e3a60)(std::uint32_t, std::uint32_t);
    std::uint32_t(__fastcall* d6c0e50)(void*, void*, std::uint32_t);
    std::uint32_t(__fastcall* d6e90b0)(void*, void*, std::uint32_t, std::uint32_t);
    std::uint32_t(__fastcall* d69d570)(void*, void*, std::uint32_t);
    std::uint32_t(__fastcall* d6f1350)(void*, void*);
};

void setObject6ECalls(const Object6ECalls& calls) noexcept;
void setObject6EClock(std::uint8_t* clock) noexcept;
void setObject6ESt0(std::uint32_t bits) noexcept;

// 0x006E1C80 MovementSpecialPower secondary 00C16930 slot 11. ret.
std::uint32_t __fastcall MovementSpecialPower_adjustedField(std::uint8_t* self, void*) noexcept;

// 0x006E1D30 MoveToPositionAndEvacuateSpecialPower secondary 00C16B10 slot 20. ret 0xC.
std::uint32_t __fastcall MoveToPositionAndEvacuateSpecialPower_dispatch(std::uint8_t* self, void*,
                                                                        std::uint32_t arg0, std::uint32_t arg1,
                                                                        std::uint32_t arg2) noexcept;

// 0x006E1E40 MoveToPositionAndReplaceSelfSpecialPower secondary 00C16D98 slot 20. ret 0xC.
std::uint32_t __fastcall MoveToPositionAndReplaceSelfSpecialPower_dispatch(std::uint8_t* self, void*,
                                                                           std::uint32_t arg0, std::uint32_t arg1,
                                                                           std::uint32_t arg2) noexcept;

// 0x006E1F70 OCLSpecialPower secondary 00C17028 slot 20. ret 0xC.
std::uint32_t __fastcall OCLSpecialPower_dispatch(std::uint8_t* self, void*, std::uint32_t arg0,
                                                  std::uint32_t arg1, std::uint32_t arg2) noexcept;

// 0x006E2F30 SpecialPowerModule 00C01108 slot 6. ret.
std::uint32_t __fastcall SpecialPowerModule_dispatchIfBit25(std::uint8_t* self, void*) noexcept;

// 0x006E2F70 SpecialPowerModule secondary 00C00F18 slot 3. ret.
std::uint32_t __fastcall SpecialPowerModule_dispatchUnlessBit6(std::uint8_t* self, void*) noexcept;

// 0x006E3220 ToggleHiddenSpecialAbilityUpdate 00C1E258 slot 43. ret.
std::uint32_t __fastcall ToggleHiddenSpecialAbilityUpdate_clearHidden(std::uint8_t* self, void*) noexcept;

// 0x006E3290 ToggleHiddenSpecialAbilityUpdate 00C1E258 slot 44. ret.
std::uint32_t __fastcall ToggleHiddenSpecialAbilityUpdate_applyHidden(std::uint8_t* self, void*) noexcept;

// 0x006E3E10 DozerAIUpdate secondary 00C24878 slot 20. ret 4.
std::uint32_t __fastcall DozerAIUpdate_acceptsObject(std::uint8_t* self, void*, std::uint32_t object) noexcept;

// 0x006E4220 DozerAIUpdate secondary 00C24878 slot 13. ret 4.
std::uint32_t __fastcall DozerAIUpdate_resetAction(std::uint8_t* self, void*, std::uint32_t arg) noexcept;

// 0x006E44D0 DozerAIUpdate secondary 00C24878 slot 26. ret.
std::uint32_t __fastcall DozerAIUpdate_workerFromId(std::uint8_t* self, void*) noexcept;

// 0x006E46E0 HordeAIUpdate secondary 00C24D18 slot 12. ret 8.
std::uint32_t __fastcall HordeAIUpdate_flagPassenger(std::uint8_t* self, void*, std::uint32_t object,
                                                    std::uint32_t extra) noexcept;

// 0x006E48F0 SupplyTruckAIUpdate 00C25398 slot 35. ret 4.
std::uint32_t __fastcall SupplyTruckAIUpdate_acceptTarget(std::uint8_t* self, void*, std::uint32_t target) noexcept;

// 0x006E4920 SupplyTruckAIUpdate 00C25398 slot 67. ret 8.
std::uint32_t __fastcall SupplyTruckAIUpdate_deliverBoxes(std::uint8_t* self, void*, std::uint32_t object,
                                                          std::uint32_t value) noexcept;

// 0x006E4980 SupplyTruckAIUpdate 00C25398 slot 69. ret 8.
std::uint32_t __fastcall SupplyTruckAIUpdate_deliverPosition(std::uint8_t* self, void*, std::uint32_t position,
                                                             std::uint32_t value) noexcept;

// 0x006E4A10 SupplyTruckAIUpdate 00C25398 slot 68. ret 8.
std::uint32_t __fastcall SupplyTruckAIUpdate_recordCargo(std::uint8_t* self, void*, std::uint32_t object,
                                                         std::uint32_t value) noexcept;

// 0x006E8750 MoneyGainAttachUpdate 00C20BC0 slot 26. ret 4.
std::uint32_t __fastcall MoneyGainAttachUpdate_maybeGain(std::uint8_t* self, void*, std::uint32_t blocked) noexcept;

// 0x006E8FD0 ProductionUpdate secondary 00C19580 slot 27. ret 4.
std::uint32_t __fastcall ProductionUpdate_producerField(std::uint8_t* self, void*, std::uint32_t object) noexcept;

// 0x006E9010 ProductionUpdate 00C19788 slot 22. ret.
std::uint32_t __fastcall ProductionUpdate_incrementQueued(std::uint8_t* self, void*) noexcept;

// 0x006E9040 ProductionUpdate secondary 00C19580 slot 47. ret.
std::uint32_t __fastcall ProductionUpdate_objectFromId(std::uint8_t* self, void*) noexcept;

// 0x006E91F0 ProductionUpdate secondary 00C19580 slot 36. ret 4.
std::uint32_t __fastcall ProductionUpdate_clearLink(std::uint8_t* self, void*, std::uint32_t arg) noexcept;

// 0x006E9390 ProductionUpdate secondary 00C19580 slot 20. ret.
std::uint32_t __fastcall ProductionUpdate_bindProducer(std::uint8_t* self, void*) noexcept;

// 0x006E9EE0 BehaviorModule secondary 00BF8DA8 slot 7. ret.
std::uint32_t __fastcall BehaviorModule_objectOrigin(std::uint8_t* self, void*) noexcept;

// 0x006EB4E0 SpecialAbilityUpdate 00C142D8 slot 39. ret.
std::uint32_t __fastcall SpecialAbilityUpdate_isReady(std::uint8_t* self, void*) noexcept;

// 0x006EBAC0 StealUnitUpdate 00C20D90 slot 25. ret 4.
std::uint32_t __fastcall StealUnitUpdate_syncTarget(std::uint8_t* self, void*, std::uint32_t object) noexcept;

// 0x006EBBD0 StoreCurrentStateUpdate 00BF8F40 slot 3. ret 4.
std::uint32_t __fastcall StoreCurrentStateUpdate_xfer(std::uint8_t* self, void*, std::uint32_t xfer) noexcept;

// 0x006ECE70 EncloseBehavior 00C08A48 slot 24. ret 4.
std::uint32_t __fastcall EncloseBehavior_offerToContainer(std::uint8_t* self, void*, std::uint32_t container) noexcept;

// 0x006ED440 WeaponInitState 00BF96A8 slot 4. ret.
std::uint32_t __fastcall WeaponInitState_enter(std::uint8_t* self, void*) noexcept;

// 0x006ED4A0 WeaponPreFireState 00BF9770 slot 6. ret 4.
std::uint32_t __fastcall WeaponPreFireState_clearFlag(std::uint8_t* self, void*, std::uint32_t) noexcept;

// 0x006ED4E0 WeaponFireState 00BF97D8 slot 5. ret. The frame compare is the batch mutation site.
std::uint32_t __fastcall WeaponFireState_advance(std::uint8_t* self, void*) noexcept;

// 0x006F6C10 / 6C30 / 6C50 / 6C70 / 6C90 GateProxyBehavior 00BFB418 slots 5, 6, 7, 4, 8. ret.
std::uint32_t __fastcall GateProxyBehavior_call14(std::uint8_t* self, void*) noexcept;
std::uint32_t __fastcall GateProxyBehavior_call18(std::uint8_t* self, void*) noexcept;
std::uint32_t __fastcall GateProxyBehavior_call1C(std::uint8_t* self, void*) noexcept;
std::uint32_t __fastcall GateProxyBehavior_call10(std::uint8_t* self, void*) noexcept;
std::uint32_t __fastcall GateProxyBehavior_call20(std::uint8_t* self, void*) noexcept;

// 0x006F8A60 UnifiedControlBehavior 00BFD820 slot 23. ret 4.
std::uint32_t __fastcall UnifiedControlBehavior_onRadius(std::uint8_t* self, void*, std::uint32_t object) noexcept;

// 0x006F94B0 HordeContain secondary 00C217F8 slot 44. ret.
std::uint32_t __fastcall HordeContain_passengerSlotB0(std::uint8_t* self, void*) noexcept;

// 0x006F94F0 HordeContain secondary 00C217F8 slot 10. ret.
std::uint32_t __fastcall HordeContain_passengerSlot28(std::uint8_t* self, void*) noexcept;

// 0x006F9710 HordeContain secondary 00C21520 slot 5. ret 4.
std::uint32_t __fastcall HordeContain_knowsObject(std::uint8_t* self, void*, std::uint32_t object) noexcept;

// 0x006F9B00 HordeContain secondary 00C217F8 slot 73. ret 0xC.
std::uint32_t __fastcall HordeContain_testTriple(std::uint8_t* self, void*, std::uint32_t arg0, std::uint32_t arg1,
                                                 std::uint32_t arg2) noexcept;

// 0x006F9B40 HordeContain secondary 00C217F8 slot 28. ret 0xC.
std::uint32_t __fastcall HordeContain_passengerSlot70(std::uint8_t* self, void*, std::uint32_t, std::uint32_t,
                                                      std::uint32_t) noexcept;

// 0x006FA9E0 HealContain secondary 00C23350 slot 38. ret 4.
std::uint32_t __fastcall HealContain_noteFrame(std::uint8_t* self, void*, std::uint32_t arg) noexcept;

// 0x006FBB50 OpenContain secondary 00C1F8F0 slot 44. ret.
std::uint32_t __fastcall OpenContain_passengerFlag(std::uint8_t* self, void*) noexcept;

// 0x006FCAD0 ActivateModuleSpecialAbilityUpdate 00C1B5D8 slot 6. ret.
std::uint32_t __fastcall ActivateModuleSpecialAbilityUpdate_enable(std::uint8_t* self, void*) noexcept;

// 0x006FDA30 GiveUpgradeUpdate 00C1CC18 slot 38. ret.
std::uint32_t __fastcall GiveUpgradeUpdate_runGrant(std::uint8_t* self, void*) noexcept;

// 0x006FE070 ToggleMountedSpecialAbilityUpdate 00C1E498 slot 39. ret.
std::uint32_t __fastcall ToggleMountedSpecialAbilityUpdate_mountedReady(std::uint8_t* self, void*) noexcept;

// 0x006FF060 SupplyTruckAIUpdate secondary 00C251A8 slot 7. ret.
std::uint32_t __fastcall SupplyTruckAIUpdate_popPending(std::uint8_t* self, void*) noexcept;

// 0x006FF9F0 AIUpdateInterface 00C22118 slot 167. ret 4.
std::uint32_t __fastcall AIUpdateInterface_allowsMove(std::uint8_t* self, void*, std::uint32_t query) noexcept;

// 0x006FFAD0 AIUpdateInterface 00C22118 slot 130. ret.
std::uint32_t __fastcall AIUpdateInterface_locomotorReady(std::uint8_t* self, void*) noexcept;

// 0x006FFD50 AIUpdateInterface 00C22118 slot 74. ret 8.
std::uint32_t __fastcall AIUpdateInterface_orderState33(std::uint8_t* self, void*, std::uint32_t object,
                                                        std::uint32_t value) noexcept;

// 0x006F7DB0 SpawnBehavior secondary 00C114E8 slot 7. ret.
std::uint32_t __fastcall SpawnBehavior_anyLiving(std::uint8_t* self, void*) noexcept;

}  // namespace ra3::gamelogic
