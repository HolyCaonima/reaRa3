#pragma once

#include <cstdint>

namespace ra3::gamelogic {

// Direct calls out of the 0x007A0000 second-pass states. `args` holds the
// stack arguments in left-to-right order (arg0 was pushed last). `nargs` is
// the count. The checker and the offline oracle install the same side effects.
struct AIState7A2Calls {
    std::uint32_t(__cdecl* direct)(std::uint32_t address, void* ecx, const std::uint32_t* args,
                                   int nargs) = nullptr;
    // Absolute dwords the bodies load (0x00DEBA64, 0x00BE700C, 0x00CC561C,
    // 0x00DF03FC, 0x00DFC190, 0x00BD9628, and the dword at address 0).
    std::uint32_t (*absDword)(std::uint32_t address) = nullptr;
};

void setAIState7A2Calls(const AIState7A2Calls& calls) noexcept;

// AIAttackPositionFireWeaponState vtable 0x00C2CA48 slot 4.
std::uint32_t AIAttackPositionFireWeaponState_onEnter(std::uint8_t* self) noexcept;
// Same vtable slot 5.
std::uint32_t AIAttackPositionFireWeaponState_update(std::uint8_t* self) noexcept;

// AIGuardIdleState vtable 0x00C2BC78 slot 5.
std::uint32_t AIGuardIdleState_update(std::uint8_t* self) noexcept;

// AIHarvestWorkSiteState vtable 0x00C2BEC8 slot 4.
std::uint32_t AIHarvestWorkSiteState_onEnter(std::uint8_t* self) noexcept;

// AIHordeEnterState vtable 0x00C2CEA0 slot 4.
std::uint32_t AIHordeEnterState_onEnter(std::uint8_t* self) noexcept;

// AIGoingIdleState vtable 0x00C2D168 slot 4.
std::uint32_t AIGoingIdleState_onEnter(std::uint8_t* self) noexcept;

// AIWaitUntilFinishedFiringState vtable 0x00C2CB88 slot 4.
std::uint32_t AIWaitUntilFinishedFiringState_onEnter(std::uint8_t* self) noexcept;

// AIPrepareForBoarding vtable 0x00C2D300 slot 4.
std::uint32_t AIPrepareForBoarding_onEnter(std::uint8_t* self) noexcept;

// AIFollowExitProductionPathState vtable 0x00C31210 slot 4.
std::uint32_t AIFollowExitProductionPathState_onEnter(std::uint8_t* self) noexcept;

// UnitAIReloadAmmoState vtable 0x00C29ED8 slot 4.
std::uint32_t UnitAIReloadAmmoState_onEnter(std::uint8_t* self) noexcept;

// UnitAIMoveOutOfTheWayState vtable 0x00C2ED88 slot 4.
std::uint32_t UnitAIMoveOutOfTheWayState_onEnter(std::uint8_t* self) noexcept;

// JetAILandingApproachState vtable 0x00C2DC40 slot 4.
std::uint32_t JetAILandingApproachState_onEnter(std::uint8_t* self) noexcept;

// AIPlayer vtable 0x00C336A0 slot 25 (also AISkirmishPlayer 0x00C33718).
std::uint32_t AIPlayer_offerTeam(std::uint8_t* self, std::uint32_t team) noexcept;

// UnitAICombinedUnitIdleState vtable 0x00C2ABA8 slot 4.
std::uint32_t UnitAICombinedUnitIdleState_onEnter(std::uint8_t* self) noexcept;

// UnitAIEnterGoalState vtable 0x00C2E718 slot 4.
std::uint32_t UnitAIEnterGoalState_onEnter(std::uint8_t* self) noexcept;

// UnitAIParkOrientState vtable 0x00C2DDE8 slots 4 and 6.
std::uint32_t UnitAIParkOrientState_onEnter(std::uint8_t* self) noexcept;
std::uint32_t UnitAIParkOrientState_onExit(std::uint8_t* self, std::uint32_t status) noexcept;

// UnitAILandState vtable 0x00C30B70 slot 5.
std::uint32_t UnitAILandState_update(std::uint8_t* self) noexcept;

// UnitAITakeOffState vtable 0x00C30BD0 slot 5.
std::uint32_t UnitAITakeOffState_update(std::uint8_t* self) noexcept;

// AITunnelNetworkGuardState vtable 0x00C2D230 slot 5.
std::uint32_t AITunnelNetworkGuardState_update(std::uint8_t* self) noexcept;

// UnitAIMoveOnPathState vtable 0x00C2ED20 slot 6.
std::uint32_t UnitAIMoveOnPathState_onExit(std::uint8_t* self, std::uint32_t status) noexcept;

// UnitAIParachuteToGroundState vtable 0x00C2F018 slot 4.
std::uint32_t UnitAIParachuteToGroundState_onEnter(std::uint8_t* self) noexcept;

// UnitAIAttackMoveToGoalState vtable 0x00C2E2F8 slot 4.
std::uint32_t UnitAIAttackMoveToGoalState_onEnter(std::uint8_t* self) noexcept;

// UnitAIAttackPositionState vtable 0x00C2E3D0 slot 4.
std::uint32_t UnitAIAttackPositionState_onEnter(std::uint8_t* self) noexcept;

// JetAIAttackMoveToGoalState vtable 0x00C2DBD0 slot 4.
std::uint32_t JetAIAttackMoveToGoalState_onEnter(std::uint8_t* self) noexcept;

// AIInternalMoveToState vtable 0x00C2D6D8 slot 3 (shared by nine other state tables).
std::uint32_t AIInternalMoveToState_xfer(std::uint8_t* self, std::uint8_t* xfer) noexcept;

// AIFollowWaypointPathState vtable 0x00C2FCB0 slot 3 (also evacuate 0x00C324D0).
std::uint32_t AIFollowWaypointPathState_xfer(std::uint8_t* self, std::uint8_t* xfer) noexcept;

std::uint32_t AIAttackApproachTargetState_xfer(std::uint8_t* self, std::uint8_t* xfer) noexcept;
std::uint32_t AIAttackPositionApproachTargetState_xfer(std::uint8_t* self, std::uint8_t* xfer) noexcept;
std::uint32_t AIAttackPursueTargetState_xfer(std::uint8_t* self, std::uint8_t* xfer) noexcept;
std::uint32_t AIGarrisonedState_xfer(std::uint8_t* self, std::uint8_t* xfer) noexcept;
std::uint32_t AIAttackMoveToState_xfer(std::uint8_t* self, std::uint8_t* xfer) noexcept;
std::uint32_t AIAttackFireDuringApproachState_xfer(std::uint8_t* self, std::uint8_t* xfer) noexcept;
std::uint32_t AIFollowWaypointPathExactState_xfer(std::uint8_t* self, std::uint8_t* xfer) noexcept;
std::uint32_t AITNGuardReturnState_xfer(std::uint8_t* self, std::uint8_t* xfer) noexcept;
std::uint32_t AIBackAwayState_xfer(std::uint8_t* self, std::uint8_t* xfer) noexcept;
std::uint32_t AIFollowPathState_xfer(std::uint8_t* self, std::uint8_t* xfer) noexcept;
std::uint32_t AIAttackMeleeHordeApproachTargetState_xfer(std::uint8_t* self, std::uint8_t* xfer) noexcept;
std::uint32_t AIAttackMeleeSquishState_xfer(std::uint8_t* self, std::uint8_t* xfer) noexcept;
std::uint32_t AIChargeTargetState_xfer(std::uint8_t* self, std::uint8_t* xfer) noexcept;
std::uint32_t AIFollowPathAsTeamState_xfer(std::uint8_t* self, std::uint8_t* xfer) noexcept;
std::uint32_t AIAttackMeleeEngageState_xfer(std::uint8_t* self, std::uint8_t* xfer) noexcept;

// AI vtable 0x00C31A50 slot 3. Slot 2 returns the string "AI".
std::uint32_t AI_xfer(std::uint8_t* self, std::uint8_t* xfer) noexcept;

}  // namespace ra3::gamelogic
