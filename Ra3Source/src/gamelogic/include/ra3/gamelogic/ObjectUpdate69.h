#pragma once

#include <cstdint>

namespace ra3::gamelogic {

// Object-update and collide-module methods whose entries lie in
// 0x00690000-0x0069FFFF. Offsets are the ones the RA3 bytes use.
// A secondary vtable's own slot 2 is not a class-name getter; the class
// name below is slot 2 of the primary vtable installed by the same
// constructor (read from the binary).

// 0x00690740 StancesBehavior 00C11840 slot 31. Slot 2 returns StancesBehavior.
std::uint32_t stancesBehaviorFirstNodeValue(const void* self) noexcept;

// 0x006911D0 HordeContain secondary 00C21520 slot 138.
// 0073CB3B stores primary 00C21A98 (slot 2 returns HordeContain) and
// 0073CB8A stores 00C21520 at this+0x16C.
float hordeContainDecayRange(void* self, float argument) noexcept;

// 0x00691350 HordeContain secondary 00C21784 slot 0 (0073CB72 stores it
// at this+0x38).
void hordeContainInvertSlotF4(void* self) noexcept;

// 0x006914B0 HordeContain secondary 00C21520 slot 8.
void hordeContainFirstPassengerId(const void* self, std::uint32_t* outId) noexcept;

// 0x006914E0 HordeContain secondary 00C21520 slot 62.
std::uint8_t hordeContainField5cClear(const void* self) noexcept;

// 0x006914F0 / 0x00691550 HordeContain secondary 00C21520 slots 121 and 122.
void hordeContainNotifyPassengerList(void* self) noexcept;
void hordeContainNotifyPassengerListShort(void* self) noexcept;

// 0x006919E0 / 0x00691A30 HordeContain record list 00C0A7D0 slots 12 and 13.
// 0074D2C6 stores HordeContain 00C21A98 and 0074D555 stores 00C0A7D0 into
// the subobject published at this+0x294.
void hordeContainMarkRecord(void* self, std::uint32_t ignored, std::int32_t index) noexcept;
std::uint8_t hordeContainCopyRecordVector(
    const void* self, std::int32_t index, void* outVector) noexcept;

// 0x00691990 SlaughterHordeContain 00C22E28 slot 43.
// Slot 2 returns SlaughterHordeContain.
std::uint8_t slaughterHordeContainPasses(void* self, const void* other) noexcept;

// 0x006923A0 InfiltratorContain secondary 00BFE530 slot 53.
// 006922F3 stores primary 00BFE7E8 (slot 2 returns InfiltratorContain) and
// 0069230E stores 00BFE530 at this+0x18.
void infiltratorContainZeroSevenDwords(void* self, void* destination, std::uint32_t ignored) noexcept;

// 0x00692500 OpenContain secondary 00C1F8F0 slot 42.
// 0072782B stores primary 00C1FB90 (slot 2 returns OpenContain) and
// 0072783F stores 00C1F8F0 at this+0x24.
void openContainStorePairIds(
    void* self, const void* first, std::int32_t count, const void* second) noexcept;

// 0x00692580 / 0x006925F0 OpenContain secondary 00C1F8F0 slots 45 and 46.
std::uint8_t openContainPlaneAccepts(const void* self, const void* candidate) noexcept;
std::uint8_t openContainPairPlaneAccepts(
    const void* self, const void* first, const void* second) noexcept;

// 0x006926D0 OpenContain secondary 00C1F8F0 slot 17.
std::uint8_t openContainHasField78(const void* self) noexcept;

// 0x00692850 / 0x00692890 OpenContain secondary 00C1F8F0 slots 4 and 43.
std::uint32_t openContainBitClear(const void* self, const std::uint32_t* bitIndex) noexcept;
void openContainCopySevenDwords(const void* self, void* destination, std::uint32_t ignored) noexcept;

// 0x00694670 BeamSpecialAbilityUpdate 00C1BA58 slot 43.
// Slot 2 returns BeamSpecialAbilityUpdate.
std::uint8_t beamSpecialAbilityAllowsTarget(const void* self, const void* target) noexcept;

// 0x00694710 SpecialPowerUpdateModule secondary 00BF7D10 slot 5.
// 006B4770 stores primary 00C0B860 (slot 2 returns SpecialPowerUpdateModule),
// 006B47B4 stores 00BF7D10 at this+0x24, and 006B47BB stores 00BF7C68 at this+0x28.
std::uint8_t specialPowerUpdateIdMatches(const void* self, std::uint32_t id) noexcept;

// 0x00694730 / 0x006966B0 / 0x00696870 secondary 00BF7C68 slots 38, 0 and 6.
void specialPowerUpdateReadFieldC(const void* self, std::uint32_t* outValue) noexcept;
std::uint8_t specialPowerUpdateIdEquals(const void* self, std::uint32_t id) noexcept;
void specialPowerUpdateReadSlot1(const void* self, std::uint32_t* outValue) noexcept;

// 0x00695670 / 0x00695690 sit on 00C00F4C slots 18 and 31.
// Slot 2 is 0067E580 and is not a name getter. The image contains no
// store of 00C00F4C; the only immediate is call dword ptr [0x00C00F4C]
// at 006CAC8B. These names do not claim a class.
void table00C00F4CStoreByte39(void* self, std::uint32_t value) noexcept;
void table00C00F4CReadField54(const void* self, std::uint32_t* outValue) noexcept;

// 0x00697100 UncombineSpecialPower secondary 00C13440 slot 30.
// 006CB444 stores primary 00C13640 (slot 2 returns UncombineSpecialPower);
// the same table's slot 130 also returns that name. 006CB46D stores
// 00C13440 at this+0x28.
std::uint32_t uncombineSpecialPowerHasEntries(const void* self) noexcept;

// 0x00697130 WeaponFireSpecialAbilityUpdate 00C1E918 slot 42.
// Slot 2 returns WeaponFireSpecialAbilityUpdate.
std::uint8_t weaponFireSpecialAbilityFlagBit8(const void* self) noexcept;

// 0x00697720 / 0x006978D0 SupplyTruckAIUpdate secondary 00C251A8 slots 2 and 19.
// 007522BB stores primary 00C25398 (slot 2 returns SupplyTruckAIUpdate) and
// 007522EB stores 00C251A8 at this+0x51C.
std::uint32_t supplyTruckSumDwords(const void* self) noexcept;
float supplyTruckRangeScale(const void* self) noexcept;

// 0x006979E0 / 0x00697A40 / 0x00697AA0 DockUpdate secondary 00C0BBB0 slots 0, 4 and 8.
// 00740541 stores primary 00C0BD58 (slot 2 returns DockUpdate) and
// 00740555 stores 00C0BBB0 at this+0x24.
std::uint8_t dockUpdateIdPresentOrOpen(const void* self, const void* visitor) noexcept;
std::uint32_t dockUpdateSlotReady(const void* self, const void* visitor, std::int32_t index) noexcept;
void dockUpdateMarkId(void* self, const void* visitor) noexcept;

// 0x006981B0 DefaultProductionExitUpdate 00C01460 slot 98.
// Slot 78 of that table returns DefaultProductionExitUpdate.
std::uint8_t defaultProductionExitTransformPoint(const void* self, float* outPoint) noexcept;

// 0x00699710 / 0x006997B0 / 0x006997E0 / 0x00699840 / 0x00699860 / 0x00699CA0
// AIUpdateInterface 00C22118 slots 158, 151, 153, 155, 157 and 66.
// Slot 2 returns AIUpdateInterface.
std::uint8_t aiUpdatePathGoalActive(const void* self) noexcept;
void aiUpdateClearGoalFloats(void* self) noexcept;
void aiUpdateSetGoalPosition(void* self, const void* position) noexcept;
void aiUpdateSetGoalFloat(void* self, std::uint32_t floatBits) noexcept;
std::uint8_t aiUpdateLocomotorAllowsMove(const void* self) noexcept;
void aiUpdatePulseModule3c(void* self, std::uint32_t ignored, std::uint32_t stored) noexcept;

// 0x0069B5A0 FireWeaponUpdate secondary 00C0CE84 slot 2.
// 006CD898 stores primary 00C0CFC8 (slot 2 returns FireWeaponUpdate) and
// 006CD8A5 stores 00C0CE84 at this+0x10.
void fireWeaponUpdateReadFieldC(const void* self, std::uint32_t* outValue) noexcept;

// 0x0069C1B0 InvisibilityBroadcastUpdate 00C03DC0 slot 92.
// Slot 78 of that table returns InvisibilityBroadcastUpdate.
void* invisibilityBroadcastAdjustor24(void* self) noexcept;

// 0x0069E5B0 ProductionUpdate secondary 00C19580 slot 37.
// 006E8ED7 stores primary 00C19788 (slot 2 returns ProductionUpdate) and
// 006E8EEB stores 00C19580 at this+0x24.
std::uint8_t productionUpdateConsumeMatchingLink(void* self, const void* key) noexcept;

} // namespace ra3::gamelogic
