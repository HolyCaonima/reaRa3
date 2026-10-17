#pragma once

#include "ra3/gamelogic/AIState79.h"

#include <cstdint>

namespace ra3::gamelogic {

// Third pass over 0x00790000. Same thiscall/stub contract as AIState79Env.
// `maxFloatBits` is the dword at 0x00BE700C. The SA registry ecx is the dword
// at 0x00CC569C (0x00CC79E0 on disk). The aim threshold is the dword at
// 0x00BD95F8 (360.0f).

std::uint32_t unitAIAttackObjectOnEnter(std::uint8_t* self, std::uint32_t eax,
                                        const std::uint32_t* stack, int argc, AIState79Env& env);
std::uint32_t aiGuardAreaOnEnter(std::uint8_t* self, std::uint32_t eax,
                                 const std::uint32_t* stack, int argc, AIState79Env& env);
std::uint32_t aiAttackAimAtTargetOnExit(std::uint8_t* self, std::uint32_t eax,
                                        const std::uint32_t* stack, int argc, AIState79Env& env);
std::uint32_t aiAttackPositionAimAtTargetOnExit(std::uint8_t* self, std::uint32_t eax,
                                                const std::uint32_t* stack, int argc, AIState79Env& env);
std::uint32_t unitAIWaitForDeliveryOnEnter(std::uint8_t* self, std::uint32_t eax,
                                           const std::uint32_t* stack, int argc, AIState79Env& env);
std::uint32_t unitAIAttackObjectOnExit(std::uint8_t* self, std::uint32_t eax,
                                       const std::uint32_t* stack, int argc, AIState79Env& env);
std::uint32_t aiAttackMeleeHordeWaitPathXfer(std::uint8_t* self, std::uint32_t eax,
                                             const std::uint32_t* stack, int argc, AIState79Env& env);
std::uint32_t unitAISuperXfer(std::uint8_t* self, std::uint32_t eax,
                              const std::uint32_t* stack, int argc, AIState79Env& env);
std::uint32_t unitAISetGoalPositionToTargetOnEnter(std::uint8_t* self, std::uint32_t eax,
                                                   const std::uint32_t* stack, int argc, AIState79Env& env);
std::uint32_t unitAIForceAttackObjectOnEnter(std::uint8_t* self, std::uint32_t eax,
                                             const std::uint32_t* stack, int argc, AIState79Env& env);
std::uint32_t aiHarvestOnExit(std::uint8_t* self, std::uint32_t eax,
                              const std::uint32_t* stack, int argc, AIState79Env& env);
std::uint32_t aiAttackSquadOnExit(std::uint8_t* self, std::uint32_t eax,
                                  const std::uint32_t* stack, int argc, AIState79Env& env);
std::uint32_t saAIPackUpdate(std::uint8_t* self, std::uint32_t eax,
                             const std::uint32_t* stack, int argc, AIState79Env& env);
std::uint32_t aiGuardRetaliateUpdate(std::uint8_t* self, std::uint32_t eax,
                                     const std::uint32_t* stack, int argc, AIState79Env& env);
std::uint32_t aiHarvestPrepareSiteOnEnter(std::uint8_t* self, std::uint32_t eax,
                                          const std::uint32_t* stack, int argc, AIState79Env& env);
std::uint32_t unitAIHordeExitGoalOnEnter(std::uint8_t* self, std::uint32_t eax,
                                         const std::uint32_t* stack, int argc, AIState79Env& env);
std::uint32_t unitAIAIEvacuateUpdate(std::uint8_t* self, std::uint32_t eax,
                                     const std::uint32_t* stack, int argc, AIState79Env& env);
std::uint32_t unitAIKillMinerOnEnter(std::uint8_t* self, std::uint32_t eax,
                                     const std::uint32_t* stack, int argc, AIState79Env& env);
std::uint32_t unitAIDoSpecialPowerUpdate(std::uint8_t* self, std::uint32_t eax,
                                         const std::uint32_t* stack, int argc, AIState79Env& env);
std::uint32_t aiAttackMeleeHordeWaitXfer(std::uint8_t* self, std::uint32_t eax,
                                         const std::uint32_t* stack, int argc, AIState79Env& env);
std::uint32_t saAIPrepareUpdate(std::uint8_t* self, std::uint32_t eax,
                                const std::uint32_t* stack, int argc, AIState79Env& env);
std::uint32_t aiRotateFiringarcOnExit(std::uint8_t* self, std::uint32_t eax,
                                      const std::uint32_t* stack, int argc, AIState79Env& env);
std::uint32_t unitAIHordeForceMembersToExitGoalOnExit(std::uint8_t* self, std::uint32_t eax,
                                                      const std::uint32_t* stack, int argc, AIState79Env& env);
std::uint32_t unitAICombinedUnitOnExit(std::uint8_t* self, std::uint32_t eax,
                                       const std::uint32_t* stack, int argc, AIState79Env& env);
std::uint32_t aiMoveOntoWallUpdate(std::uint8_t* self, std::uint32_t eax,
                                   const std::uint32_t* stack, int argc, AIState79Env& env);
std::uint32_t aiPickUpCrateOnEnter(std::uint8_t* self, std::uint32_t eax,
                                   const std::uint32_t* stack, int argc, AIState79Env& env);
std::uint32_t unitAIFollowWaypointPathOnExit(std::uint8_t* self, std::uint32_t eax,
                                             const std::uint32_t* stack, int argc, AIState79Env& env);
std::uint32_t aiGuardAreaUpdate(std::uint8_t* self, std::uint32_t eax,
                                const std::uint32_t* stack, int argc, AIState79Env& env);
std::uint32_t aiGuardRetaliateOnExit(std::uint8_t* self, std::uint32_t eax,
                                     const std::uint32_t* stack, int argc, AIState79Env& env);
std::uint32_t unitAIAimForceFireAtObjectXfer(std::uint8_t* self, std::uint32_t eax,
                                             const std::uint32_t* stack, int argc, AIState79Env& env);
std::uint32_t saAIFinishOnEnter(std::uint8_t* self, std::uint32_t eax,
                                const std::uint32_t* stack, int argc, AIState79Env& env);
std::uint32_t aiAttackFireWeaponSlot17(std::uint8_t* self, std::uint32_t eax,
                                       const std::uint32_t* stack, int argc, AIState79Env& env);
std::uint32_t aiGuardOnExit(std::uint8_t* self, std::uint32_t eax,
                            const std::uint32_t* stack, int argc, AIState79Env& env);
std::uint32_t saAIPrepareOnExit(std::uint8_t* self, std::uint32_t eax,
                                const std::uint32_t* stack, int argc, AIState79Env& env);
std::uint32_t aiBackAwayOnEnter(std::uint8_t* self, std::uint32_t eax,
                                const std::uint32_t* stack, int argc, AIState79Env& env);
std::uint32_t aiTNGuardInnerOnExit(std::uint8_t* self, std::uint32_t eax,
                                   const std::uint32_t* stack, int argc, AIState79Env& env);
std::uint32_t aiTNGuardOuterOnExit(std::uint8_t* self, std::uint32_t eax,
                                   const std::uint32_t* stack, int argc, AIState79Env& env);
std::uint32_t unitAIAttackObjectUpdate(std::uint8_t* self, std::uint32_t eax,
                                       const std::uint32_t* stack, int argc, AIState79Env& env);
std::uint32_t unitAIHeliPrepareForBoardingOnExit(std::uint8_t* self, std::uint32_t eax,
                                                 const std::uint32_t* stack, int argc, AIState79Env& env);
std::uint32_t aiBackAwayAndCowerUpdate(std::uint8_t* self, std::uint32_t eax,
                                       const std::uint32_t* stack, int argc, AIState79Env& env);
std::uint32_t aiMoveToGoalDiveBombOnExit(std::uint8_t* self, std::uint32_t eax,
                                         const std::uint32_t* stack, int argc, AIState79Env& env);
std::uint32_t aiStateMachineSlot10(std::uint8_t* self, std::uint32_t eax,
                                   const std::uint32_t* stack, int argc, AIState79Env& env);
std::uint32_t unitAIParachuteToGroundSlot19(std::uint8_t* self, std::uint32_t eax,
                                            const std::uint32_t* stack, int argc, AIState79Env& env);
std::uint32_t unitAIDeadUpdate(std::uint8_t* self, std::uint32_t eax,
                               const std::uint32_t* stack, int argc, AIState79Env& env);
std::uint32_t unitAIAttackSquadOnEnter(std::uint8_t* self, std::uint32_t eax,
                                       const std::uint32_t* stack, int argc, AIState79Env& env);
std::uint32_t unitAIMoveToGoalCopyOnEnter(std::uint8_t* self, std::uint32_t eax,
                                          const std::uint32_t* stack, int argc, AIState79Env& env);
std::uint32_t unitAIMoveToGoalOnEnter(std::uint8_t* self, std::uint32_t eax,
                                      const std::uint32_t* stack, int argc, AIState79Env& env);

bool runAIState79C(std::uint32_t va, std::uint8_t* self, std::uint32_t eaxIn,
                   const std::uint32_t* stack, int argc, AIState79Env& env, std::uint32_t& eaxOut);

}  // namespace ra3::gamelogic
