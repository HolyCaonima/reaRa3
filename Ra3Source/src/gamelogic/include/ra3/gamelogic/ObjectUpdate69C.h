#pragma once

#include <cstdint>

namespace ra3::gamelogic {

// Third pass over clean update/collide methods in 0x00690000-0x0069FFFF.
// Direct callees that are not reconstructed here go through directCall.
// A few bodies read a process singleton; imageDword is the offline stand-in
// for that absolute load (0x00DEBA64, 0x00DFBD74).

struct DirectCall {
    std::uint32_t target = 0;
    void* ecx = nullptr;
    std::uint32_t arg0 = 0;
    std::uint32_t arg1 = 0;
    std::uint32_t arg2 = 0;
    std::uint32_t arg3 = 0;
    std::uint32_t arg4 = 0;
    std::uint32_t arg5 = 0;
    int argc = 0;
};

using DirectHandler = std::uint32_t (*)(const DirectCall&);
using AbsoluteReader = std::uint32_t (*)(std::uint32_t address);

void setObjectUpdate69CDirect(DirectHandler handler) noexcept;
void setObjectUpdate69CAbsolute(AbsoluteReader reader) noexcept;

// AIUpdateInterface 00C22118. Slot 2 returns AIUpdateInterface.
void aiUpdateIssue61(void* self, std::uint32_t goal, std::uint32_t packed, std::uint32_t stamp) noexcept;
void aiUpdateIssue51(void* self, std::uint32_t goal, std::uint32_t stamp) noexcept;
void aiUpdateIssue3A(void* self, std::uint32_t goal, std::uint32_t stamp) noexcept;
void aiUpdateIssue32(void* self, std::uint32_t goal, std::uint32_t stamp) noexcept;
void aiUpdateIssue40(void* self, std::uint32_t goal, std::uint32_t packed, std::uint32_t stamp) noexcept;
void aiUpdateIssue42(void* self, std::uint32_t goal, std::uint32_t packed, std::uint32_t stamp) noexcept;
void aiUpdateIssue3F(void* self, std::uint32_t goal, std::uint32_t packed, std::uint32_t stamp) noexcept;
void aiUpdateIssue43(void* self, std::uint32_t goal, std::uint32_t packed, std::uint32_t stamp) noexcept;
void aiUpdateIssue49(void* self, std::uint32_t goal, std::uint32_t packed, std::uint32_t stamp) noexcept;
void aiUpdateIssue2A(void* self, std::uint32_t target, std::uint32_t stamp) noexcept;
void aiUpdateTouchLocomotor(void* self) noexcept;

// SpecialPowerModule 00C01108. Slot 2 returns SpecialPowerModule.
void specialPowerModuleNoteReady(void* self) noexcept;
void specialPowerModuleTrigger(void* self, std::uint32_t command, std::uint32_t target) noexcept;

// ManagerBody 00C124C8 slot 25. Slot 2 returns ManagerBody.
void managerBodyCacheTemplate(void* self) noexcept;

// MoneyCrateCollide 00C19E10 slot 23. Slot 2 returns MoneyCrateCollide.
std::uint8_t moneyCrateCollideGrant(void* self, void* other) noexcept;

// TunnelContain 00C23A98 slot 48. Slot 2 returns TunnelContain.
void* tunnelContainOpenIterator(void* self, void* out) noexcept;

// UnitVisionBehavior 00BFDB88. Slot 2 returns UnitVisionBehavior.
void unitVisionBehaviorStoreRange(void* self, std::uint32_t packed, std::uint32_t key) noexcept;
void unitVisionBehaviorXfer(void* self, void* xfer) noexcept;

// DetonateState 00BF79C0 slot 5. Slot 2 returns DetonateState.
std::uint32_t detonateStateRequest(void* self) noexcept;

// DozerAIUpdate 00C24A50 slot 58. Slot 2 returns DozerAIUpdate.
void dozerAiUpdateAcceptTask(void* self, void* task, std::uint32_t extra) noexcept;

// CrateCollide 00C0A598 slot 22. Slot 2 returns CrateCollide.
std::uint8_t crateCollideAllows(void* self, void* other) noexcept;

// StealthDetectorUpdate 00C0DFC0 slot 3. Slot 2 returns StealthDetectorUpdate.
void stealthDetectorUpdateXfer(void* self, void* xfer) noexcept;

// SpecialPowerUpdateModule 00C0B860 slot 3. Slot 2 returns SpecialPowerUpdateModule.
void specialPowerUpdateModuleXfer(void* self, void* xfer) noexcept;

// SecondaryDamageTracker 00C05858 slot 3. Slot 2 returns SecondaryDamageTracker.
void secondaryDamageTrackerXfer(void* self, void* xfer) noexcept;

// SpecialPowerUpdateModule secondary 00BF7C68 slots 3 and 5.
// 006B4770 stores 00C0B860 at [esi] and 00BF7C68 at [edi].
float specialPowerUpdateReadiness(void* self) noexcept;
std::uint32_t specialPowerUpdateNotifyOwner(void* self) noexcept;

// DozerAIUpdate secondary 00C24878 slot 3.
// 00751D20 stores 00C24A50 at [esi] and 00C24878 at [edi].
float dozerAiUpdateTerrainScale(void* self) noexcept;

// GarrisonContain secondary 00C21BF0.
// 0074DFC0 stores 00C21E90 at [esi] and 00C21BF0 at [esi+0x24].
std::uint8_t garrisonContainAccepts(void* self, void* other, std::uint32_t mode, std::uint32_t extra) noexcept;
void* garrisonContainAnchorOrSlot(void* self) noexcept;
void* garrisonContainLocatePoint(void* self) noexcept;

// ProductionUpdate secondary 00C19580 slot 10.
// 006E8ED0 stores 00C19788 at [esi] and 00C19580 at [esi+0x24].
std::uint8_t productionUpdateCancelOne(void* self, std::uint32_t key) noexcept;

// SpecialAbilityUpdate secondary 00C14184 slot 6.
// 00701F40 stores 00C142D8 at [esi] and 00C14184 at [esi+0x24].
std::uint8_t specialAbilityUpdateRelay(void* self, std::uint32_t command, std::uint32_t extra) noexcept;

// SpecialPowerDispatchSpecialPower secondary 00C19058.
// 006E2AB0 stores 00C19230 at [esi] and 00C19058 at [esi+0x10].
void* specialPowerDispatchCopyKind(void* self, std::uint32_t* outKind) noexcept;
void specialPowerDispatchAdjustCount(void* self, std::uint32_t engage) noexcept;
void specialPowerDispatchApplyKind4(void* self, std::uint32_t value) noexcept;
void specialPowerDispatchApplyKind23(void* self, std::uint32_t left, std::uint32_t right) noexcept;
void specialPowerDispatchApplyKind03(void* self, std::uint32_t left, std::uint32_t right) noexcept;
void specialPowerDispatchApplyKind1(void* self, std::uint32_t left, std::uint32_t floatBits, std::uint32_t right) noexcept;

// DockUpdate secondary 00C0BBB0 slot 23.
// 00740520 stores 00C0BD58 at [esi] and 00C0BBB0 at [esi+0x24].
float dockUpdateOccupiedScale(void* self) noexcept;

// HordeContain secondary 00C21520.
// 0073CB30 stores 00C21A98 at [esi] and 00C21520 at [esi+0x16C].
std::uint8_t hordeContainSharesSide(void* self, void* other, std::uint32_t gate, void* counter, std::uint32_t allow) noexcept;
void hordeContainPushTimer(void* self, std::uint32_t floatBits) noexcept;

} // namespace ra3::gamelogic
