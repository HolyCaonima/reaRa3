#pragma once

#include <cstdint>

namespace ra3::gamelogic {

// Direct callees of the 0x007A0000 state cluster. `a0` is the first stack
// argument (the last value pushed). `nargs` is 0..3. The checker and the
// offline oracle install the same side effects for these addresses.
struct AIState7ACalls {
    std::uint32_t(__cdecl* direct)(std::uint32_t address, void* ecx, std::uint32_t a0,
                                   std::uint32_t a1, std::uint32_t a2, int nargs) = nullptr;
    // Dword the binary reads from 0x00DEBA64 (TheGameLogic).
    std::uint32_t gameLogic = 0;
};

void setAIState7ACalls(const AIState7ACalls& calls) noexcept;

// UnitAIRotateToAngleState vtable 0x00C2B708 slot 4. Returns -1 when
// machine+0x54 equals the dword at 0x00BE700C (0x7F7FFFFF). NaN and every
// other encoding tail-call 0x007985D0.
std::uint32_t UnitAIRotateToAngleState_onEnter(std::uint8_t* self) noexcept;

// AIWaitUntilMembersFinishedBeforeExitState vtable 0x00C2CB10 slot 4.
// Bit 10 of [template+0xCC] selects 0 or STATE_FAILURE.
std::uint32_t AIWaitUntilMembersFinishedBeforeExitState_onEnter(std::uint8_t* self) noexcept;

// UnitAICombinedUnitMoveToState vtable 0x00C31048 slots 5 and 6.
std::uint32_t UnitAICombinedUnitMoveToState_update(std::uint8_t* self) noexcept;
std::uint32_t UnitAICombinedUnitMoveToState_onExit(std::uint8_t* self, std::uint32_t status) noexcept;

// AIRotateFiringarc vtable 0x00C2D368 slot 4.
std::uint32_t AIRotateFiringarc_onEnter(std::uint8_t* self) noexcept;

// UnitAIDepartBeach vtable 0x00C2ADA0 slot 4.
std::uint32_t UnitAIDepartBeach_onEnter(std::uint8_t* self) noexcept;

// UnitAIDriftState vtable 0x00C2A478 slot 4. Sets owner status bit 0x11.
std::uint32_t UnitAIDriftState_onEnter(std::uint8_t* self) noexcept;

// UnitAIHordeGarrisonedState vtable 0x00C2B438 slots 4 and 6.
std::uint32_t UnitAIHordeGarrisonedState_onEnter(std::uint8_t* self) noexcept;
std::uint32_t UnitAIHordeGarrisonedState_onExit(std::uint8_t* self, std::uint32_t) noexcept;

// UnitAIDisguiseHoldFireState vtable 0x00C30700 slot 4.
std::uint32_t UnitAIDisguiseHoldFireState_onEnter(std::uint8_t* self) noexcept;

// AIStateMachine vtable 0x00C2C4C8 slot 8. Allocates 0x80 and calls 0x007A57A0.
std::uint32_t AIStateMachine_createLinkedState(std::uint8_t* self, std::uint32_t argument) noexcept;

// UnitAIPrepareForBoardingState vtable 0x00C2F160 slot 4 (also 0x00C2DA10).
std::uint32_t UnitAIPrepareForBoardingState_onEnter(std::uint8_t* self) noexcept;

// UnitAIMoveToGoalAndEnterState vtable 0x00C2EFA8 slot 6.
std::uint32_t UnitAIMoveToGoalAndEnterState_onExit(std::uint8_t* self, std::uint32_t) noexcept;

// UnitAIParachuteToGroundState vtable 0x00C2F018 slot 6.
std::uint32_t UnitAIParachuteToGroundState_onExit(std::uint8_t* self, std::uint32_t) noexcept;

// UnitAIPrepareForBoardingState vtable 0x00C2F160 slot 6.
std::uint32_t UnitAIPrepareForBoardingState_onExit(std::uint8_t* self, std::uint32_t) noexcept;

// AIFaceState vtable 0x00C2F248 slot 6 (also AIDockWaitForClearanceState 0x00C2BBA8).
std::uint32_t AIFaceState_onExit(std::uint8_t* self, std::uint32_t status) noexcept;

// AIAttackMeleeHordeWaitState vtable 0x00C2C7D0 slot 6 (also 0x00C2C838).
std::uint32_t AIAttackMeleeHordeWaitState_onExit(std::uint8_t* self, std::uint32_t) noexcept;

// AIHordeEnterState vtable 0x00C2CEA0 slot 6.
std::uint32_t AIHordeEnterState_onExit(std::uint8_t* self, std::uint32_t status) noexcept;

// UnitAIReloadAmmoState vtable 0x00C29ED8 slot 6.
std::uint32_t UnitAIReloadAmmoState_onExit(std::uint8_t* self, std::uint32_t) noexcept;

// UnitAIDeliverToRefineryState vtable 0x00C29FA8 slot 6.
std::uint32_t UnitAIDeliverToRefineryState_onExit(std::uint8_t* self, std::uint32_t) noexcept;

// UnitAICreatePathState vtable 0x00C2AC10 slot 4. Tail-calls 0x0079E460.
std::uint32_t UnitAICreatePathState_onEnter(std::uint8_t* self) noexcept;

// AIFollowExitProductionPathState vtable 0x00C31210 slot 6.
std::uint32_t AIFollowExitProductionPathState_onExit(std::uint8_t* self, std::uint32_t status) noexcept;

// UnitAIGarrisonedState vtable 0x00C2EA28 slot 6.
std::uint32_t UnitAIGarrisonedState_onExit(std::uint8_t* self, std::uint32_t) noexcept;

// UnitAIMoveOutOfTheWayState vtable 0x00C2ED88 slot 6.
std::uint32_t UnitAIMoveOutOfTheWayState_onExit(std::uint8_t* self, std::uint32_t) noexcept;

// UnitAIMoveToGoalState vtable 0x00C31668 slot 6.
std::uint32_t UnitAIMoveToGoalState_onExit(std::uint8_t* self, std::uint32_t status) noexcept;

// UnitAIMoveToDeliverState vtable 0x00C2E068 slot 6 (also 0x00C2E0D0 and 0x00C30DE8).
std::uint32_t UnitAIMoveToDeliverState_onExit(std::uint8_t* self, std::uint32_t status) noexcept;

// AIHarvestPrepareSiteState vtable 0x00C2BE60 slot 6.
std::uint32_t AIHarvestPrepareSiteState_onExit(std::uint8_t* self, std::uint32_t) noexcept;

// JetAIAttackMoveToGoalState vtable 0x00C2DBD0 slot 6 (also 0x00C2E2F8).
std::uint32_t JetAIAttackMoveToGoalState_onExit(std::uint8_t* self, std::uint32_t) noexcept;

// UnitAIAttackPositionState vtable 0x00C2E3D0 slot 6.
std::uint32_t UnitAIAttackPositionState_onExit(std::uint8_t* self, std::uint32_t) noexcept;

// AIDockWaitForClearanceState vtable 0x00C2BBA8 slot 5.
std::uint32_t AIDockWaitForClearanceState_update(std::uint8_t* self) noexcept;

// AIDockProcessDockState vtable 0x00C2BC10 slot 4.
std::uint32_t AIDockProcessDockState_onEnter(std::uint8_t* self) noexcept;

// AIGuardGoingIdleState vtable 0x00C2BCD8 slot 4.
std::uint32_t AIGuardGoingIdleState_onEnter(std::uint8_t* self) noexcept;

// AIStartAttackObjState vtable 0x00C2CBF8 slot 4.
std::uint32_t AIStartAttackObjState_onEnter(std::uint8_t* self) noexcept;

// UnitAIMoveToDeliverState vtable 0x00C2E068 slot 4 (also 0x00C2E0D0 and 0x00C30DE8).
std::uint32_t UnitAIMoveToDeliverState_onEnter(std::uint8_t* self) noexcept;

// UnitAITaxiFromHangerState vtable 0x00C2DEC0 slot 6.
std::uint32_t UnitAITaxiFromHangerState_onExit(std::uint8_t* self, std::uint32_t) noexcept;

// UnitAIGarrisonedIdleState vtable 0x00C2B1B8 slot 4.
std::uint32_t UnitAIGarrisonedIdleState_onEnter(std::uint8_t* self) noexcept;

// AIMoveToStateSA vtable 0x00C2F3E8 slot 3.
std::uint32_t AIMoveToStateSA_xfer(std::uint8_t* self, std::uint8_t* xfer) noexcept;

// AIMoveAndTightenState vtable 0x00C2F4B0 slot 3 (also panic, repulsor, and fear).
std::uint32_t AIMoveAndTightenState_xfer(std::uint8_t* self, std::uint8_t* xfer) noexcept;

// AIPickUpCrateState vtable 0x00C2FAA8 slot 3 (also 0x00C30160).
std::uint32_t AIPickUpCrateState_xfer(std::uint8_t* self, std::uint8_t* xfer) noexcept;

// AIMoveAndEvacuateState vtable 0x00C2FBE0 slot 3.
std::uint32_t AIMoveAndEvacuateState_xfer(std::uint8_t* self, std::uint8_t* xfer) noexcept;

// AIMoveAndDeleteState vtable 0x00C2FC48 slot 3 (also 0x00C2FFB8).
std::uint32_t AIMoveAndDeleteState_xfer(std::uint8_t* self, std::uint8_t* xfer) noexcept;

// AIAttackFollowWaypointPathState vtable 0x00C321E8 slot 3.
std::uint32_t AIAttackFollowWaypointPathState_xfer(std::uint8_t* self, std::uint8_t* xfer) noexcept;

// AIWanderState vtable 0x00C32258 slot 3 (also AIPanicState 0x00C322B8).
std::uint32_t AIWanderState_xfer(std::uint8_t* self, std::uint8_t* xfer) noexcept;

// AIEnterState vtable 0x00C2FE00 slot 3.
std::uint32_t AIEnterState_xfer(std::uint8_t* self, std::uint8_t* xfer) noexcept;

// AIEnterAndAttackState vtable 0x00C2FEC0 slot 3.
std::uint32_t AIEnterAndAttackState_xfer(std::uint8_t* self, std::uint8_t* xfer) noexcept;

// AIMoveToPositionAndEnterState vtable 0x00C301B8 slot 3.
std::uint32_t AIMoveToPositionAndEnterState_xfer(std::uint8_t* self, std::uint8_t* xfer) noexcept;

// AIHarvestApproachSiteState vtable 0x00C31E38 slot 3.
std::uint32_t AIHarvestApproachSiteState_xfer(std::uint8_t* self, std::uint8_t* xfer) noexcept;

}  // namespace ra3::gamelogic
