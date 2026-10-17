#pragma once

#include <cstdint>

namespace ra3::gamelogic {

// Absolutes and out-of-range calls for the 0x007B0000 AI State methods.
// `call` is thiscall when the original callee pops its arguments, and cdecl
// when the original does `add esp`. Stubs return EAX and leave ECX unchanged.
struct AIState7BEnv {
    std::uint32_t gameLogic = 0;          // dword at 0x00DEBA64
    std::uint32_t limitBits = 0x43C80000u; // 0x00BD95B8, 400.0f
    std::uint32_t nearBits = 0x42480000u;  // 0x00BD952C, 50.0f
    std::uint32_t (*call)(void* ctx, std::uint32_t target, std::uint32_t ecx,
                          const std::uint32_t* args, int argc) = nullptr;
    void* callCtx = nullptr;
    std::uint8_t* scratch = nullptr;
    std::uint32_t scratchBytes = 0;
    std::uint32_t captureBytes = 0;
};

std::uint32_t callExternal(AIState7BEnv& env, std::uint32_t target, std::uint32_t ecx,
                           const std::uint32_t* args, int argc);

std::uint32_t aiGarrisonedOnEnter(std::uint8_t* self, std::uint32_t eax, const std::uint32_t* stack,
                                  int argc, AIState7BEnv& env);
std::uint32_t aiGarrisonedOnExit(std::uint8_t* self, std::uint32_t eax, const std::uint32_t* stack,
                                 int argc, AIState7BEnv& env);
std::uint32_t aiGarrisonedUpdate(std::uint8_t* self, std::uint32_t eax, const std::uint32_t* stack,
                                 int argc, AIState7BEnv& env);
std::uint32_t unitAIDeadOnExit(std::uint8_t* self, std::uint32_t eax, const std::uint32_t* stack,
                               int argc, AIState7BEnv& env);
std::uint32_t aiMoveToOnExit(std::uint8_t* self, std::uint32_t eax, const std::uint32_t* stack,
                             int argc, AIState7BEnv& env);
std::uint32_t aiMoveToCoverOnExit(std::uint8_t* self, std::uint32_t eax, const std::uint32_t* stack,
                                  int argc, AIState7BEnv& env);
std::uint32_t aiMoveToStateSAUpdate(std::uint8_t* self, std::uint32_t eax, const std::uint32_t* stack,
                                    int argc, AIState7BEnv& env);
std::uint32_t aiMoveOutOfTheWayOnExit(std::uint8_t* self, std::uint32_t eax, const std::uint32_t* stack,
                                      int argc, AIState7BEnv& env);
std::uint32_t aiMoveAndTightenUpdate(std::uint8_t* self, std::uint32_t eax, const std::uint32_t* stack,
                                     int argc, AIState7BEnv& env);
std::uint32_t aiMoveAwayPanicOnExit(std::uint8_t* self, std::uint32_t eax, const std::uint32_t* stack,
                                    int argc, AIState7BEnv& env);
std::uint32_t aiMoveAwayPanicUpdate(std::uint8_t* self, std::uint32_t eax, const std::uint32_t* stack,
                                    int argc, AIState7BEnv& env);
std::uint32_t aiMoveAwayFromRepulsorsOnExit(std::uint8_t* self, std::uint32_t eax,
                                            const std::uint32_t* stack, int argc, AIState7BEnv& env);
std::uint32_t aiFearOnExit(std::uint8_t* self, std::uint32_t eax, const std::uint32_t* stack, int argc,
                           AIState7BEnv& env);
std::uint32_t aiUncontrollableCowerOnEnter(std::uint8_t* self, std::uint32_t eax,
                                           const std::uint32_t* stack, int argc, AIState7BEnv& env);
std::uint32_t aiUncontrollableCowerOnExit(std::uint8_t* self, std::uint32_t eax,
                                          const std::uint32_t* stack, int argc, AIState7BEnv& env);
std::uint32_t aiMoveAwayAndCowerOnExit(std::uint8_t* self, std::uint32_t eax, const std::uint32_t* stack,
                                       int argc, AIState7BEnv& env);
std::uint32_t aiMoveAwayAndCowerUpdate(std::uint8_t* self, std::uint32_t eax, const std::uint32_t* stack,
                                       int argc, AIState7BEnv& env);
std::uint32_t aiBackAwayOnExit(std::uint8_t* self, std::uint32_t eax, const std::uint32_t* stack,
                               int argc, AIState7BEnv& env);
std::uint32_t aiAttackApproachTargetOnExit(std::uint8_t* self, std::uint32_t eax,
                                           const std::uint32_t* stack, int argc, AIState7BEnv& env);
std::uint32_t aiAttackPositionApproachTargetOnExit(std::uint8_t* self, std::uint32_t eax,
                                                   const std::uint32_t* stack, int argc,
                                                   AIState7BEnv& env);
std::uint32_t aiAttackPursueTargetOnExit(std::uint8_t* self, std::uint32_t eax,
                                         const std::uint32_t* stack, int argc, AIState7BEnv& env);
std::uint32_t aiAttackMeleeSquishOnExit(std::uint8_t* self, std::uint32_t eax,
                                        const std::uint32_t* stack, int argc, AIState7BEnv& env);
std::uint32_t aiAttackMeleeEngageUpdate(std::uint8_t* self, std::uint32_t eax,
                                        const std::uint32_t* stack, int argc, AIState7BEnv& env);
std::uint32_t aiAttackMoveToOnExit(std::uint8_t* self, std::uint32_t eax, const std::uint32_t* stack,
                                   int argc, AIState7BEnv& env);
std::uint32_t aiFollowWaypointPathOnExit(std::uint8_t* self, std::uint32_t eax,
                                         const std::uint32_t* stack, int argc, AIState7BEnv& env);
std::uint32_t aiFollowWaypointPathEvacuateOnExit(std::uint8_t* self, std::uint32_t eax,
                                                 const std::uint32_t* stack, int argc, AIState7BEnv& env);
std::uint32_t aiFollowWaypointPathExactOnExit(std::uint8_t* self, std::uint32_t eax,
                                              const std::uint32_t* stack, int argc, AIState7BEnv& env);
std::uint32_t aiFollowWaypointPathExactUpdate(std::uint8_t* self, std::uint32_t eax,
                                              const std::uint32_t* stack, int argc, AIState7BEnv& env);
std::uint32_t aiAttackFollowWaypointPathOnExit(std::uint8_t* self, std::uint32_t eax,
                                               const std::uint32_t* stack, int argc, AIState7BEnv& env);
std::uint32_t aiWanderOnExit(std::uint8_t* self, std::uint32_t eax, const std::uint32_t* stack, int argc,
                             AIState7BEnv& env);
std::uint32_t aiWanderInPlaceOnExit(std::uint8_t* self, std::uint32_t eax, const std::uint32_t* stack,
                                    int argc, AIState7BEnv& env);
std::uint32_t aiPanicOnExit(std::uint8_t* self, std::uint32_t eax, const std::uint32_t* stack, int argc,
                            AIState7BEnv& env);
std::uint32_t aiCombineUpdate(std::uint8_t* self, std::uint32_t eax, const std::uint32_t* stack, int argc,
                              AIState7BEnv& env);
std::uint32_t aiChargeTargetOnExit(std::uint8_t* self, std::uint32_t eax, const std::uint32_t* stack,
                                   int argc, AIState7BEnv& env);
std::uint32_t aiMoveAndDeleteOnExit(std::uint8_t* self, std::uint32_t eax, const std::uint32_t* stack,
                                    int argc, AIState7BEnv& env);
std::uint32_t aiMoveToAndEvacuateOnExit(std::uint8_t* self, std::uint32_t eax, const std::uint32_t* stack,
                                        int argc, AIState7BEnv& env);
std::uint32_t aiDeployOnEnter(std::uint8_t* self, std::uint32_t eax, const std::uint32_t* stack, int argc,
                              AIState7BEnv& env);
std::uint32_t aiDeployOnExit(std::uint8_t* self, std::uint32_t eax, const std::uint32_t* stack, int argc,
                             AIState7BEnv& env);
std::uint32_t aiUnDeployOnEnter(std::uint8_t* self, std::uint32_t eax, const std::uint32_t* stack,
                                int argc, AIState7BEnv& env);
std::uint32_t aiUnDeployOnExit(std::uint8_t* self, std::uint32_t eax, const std::uint32_t* stack,
                               int argc, AIState7BEnv& env);
std::uint32_t aiTNGuardMachineXfer(std::uint8_t* self, std::uint32_t eax, const std::uint32_t* stack,
                                   int argc, AIState7BEnv& env);
std::uint32_t aiStateMachineSlot18(std::uint8_t* self, std::uint32_t eax, const std::uint32_t* stack,
                                   int argc, AIState7BEnv& env);
std::uint32_t unitAILandOnExit(std::uint8_t* self, std::uint32_t eax, const std::uint32_t* stack, int argc,
                               AIState7BEnv& env);
std::uint32_t unitAIReturnForLandingSlot19(std::uint8_t* self, std::uint32_t eax,
                                           const std::uint32_t* stack, int argc, AIState7BEnv& env);
std::uint32_t unitAITakeOffOnExit(std::uint8_t* self, std::uint32_t eax, const std::uint32_t* stack,
                                  int argc, AIState7BEnv& env);
std::uint32_t aiFollowExitProductionPathSlot19(std::uint8_t* self, std::uint32_t eax,
                                               const std::uint32_t* stack, int argc, AIState7BEnv& env);
std::uint32_t aiMoveToAndCollectOnExit(std::uint8_t* self, std::uint32_t eax, const std::uint32_t* stack,
                                       int argc, AIState7BEnv& env);
std::uint32_t aiInternalMoveTo31B10Update(std::uint8_t* self, std::uint32_t eax,
                                          const std::uint32_t* stack, int argc, AIState7BEnv& env);
std::uint32_t aiDockApproachOnExit(std::uint8_t* self, std::uint32_t eax, const std::uint32_t* stack,
                                   int argc, AIState7BEnv& env);
std::uint32_t aiInternalMoveTo31B60Update(std::uint8_t* self, std::uint32_t eax,
                                          const std::uint32_t* stack, int argc, AIState7BEnv& env);
std::uint32_t aiInternalMoveTo31B60OnExit(std::uint8_t* self, std::uint32_t eax,
                                          const std::uint32_t* stack, int argc, AIState7BEnv& env);
std::uint32_t aiInternalMoveTo31BB0Update(std::uint8_t* self, std::uint32_t eax, const std::uint32_t* stack,
                                          int argc, AIState7BEnv& env);
std::uint32_t aiInternalMoveTo31C00Update(std::uint8_t* self, std::uint32_t eax, const std::uint32_t* stack,
                                          int argc, AIState7BEnv& env);
std::uint32_t aiInternalMoveTo31C00OnExit(std::uint8_t* self, std::uint32_t eax, const std::uint32_t* stack,
                                          int argc, AIState7BEnv& env);
std::uint32_t aiGuardReturnUpdate(std::uint8_t* self, std::uint32_t eax, const std::uint32_t* stack,
                                  int argc, AIState7BEnv& env);

bool runAIState7B(std::uint32_t va, std::uint8_t* self, std::uint32_t eaxIn, const std::uint32_t* stack,
                  int argc, AIState7BEnv& env, std::uint32_t& eaxOut);

}  // namespace ra3::gamelogic
