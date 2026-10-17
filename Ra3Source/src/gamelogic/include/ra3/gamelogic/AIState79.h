#pragma once

#include <cstddef>
#include <cstdint>

namespace ra3::gamelogic {

// Stand-in for the absolutes and out-of-range calls used by the 0x00790000
// AI State methods. `call` is thiscall when the original callee pops its stack
// arguments, and cdecl when the original does `add esp`. The oracle stub
// returns EAX and leaves ECX unchanged, which is the contract these bodies use
// when they do not reload ECX between two calls.
struct AIState79Env {
    std::uint32_t gameLogic = 0;     // dword at 0x00DEBA64
    std::uint32_t logicTable = 0;    // dword at 0x00DFC4B4
    std::uint32_t limitBits = 0x43C80000u;   // 0x00BD95B8, 400.0f
    std::uint32_t maxFloatBits = 0x7F7FFFFFu; // 0x00BE700C
    std::uint32_t oneBits = 0x3F800000u;      // 0x00BD90A4
    std::uint32_t (*call)(void* ctx, std::uint32_t target, std::uint32_t ecx,
                          const std::uint32_t* args, int argc) = nullptr;
    void* callCtx = nullptr;
    // Stack temporary the body is about to pass. The checker logs it as `ptr`
    // or, when captureBlob is set, as the eight bytes at that address.
    std::uint8_t* blob = nullptr;
    std::uint32_t blobBytes = 0;
    bool captureBlob = false;
};

std::uint32_t callExternal(AIState79Env& env, std::uint32_t target, std::uint32_t ecx,
                           const std::uint32_t* args, int argc);

// FollowWaypointPathStateMachine 0x00C29A48.
std::uint32_t followWaypointPathStoreTolerance(std::uint8_t* self, std::uint32_t eax,
                                               const std::uint32_t* stack, int argc, AIState79Env& env);
std::uint32_t followWaypointPathStoreFlag(std::uint8_t* self, std::uint32_t eax,
                                          const std::uint32_t* stack, int argc, AIState79Env& env);

// UnitAIPArkedState 0x00C2DD88 (the image spells the class with that capital P).
std::uint32_t unitAIParkedOnExit(std::uint8_t* self, std::uint32_t eax,
                                 const std::uint32_t* stack, int argc, AIState79Env& env);
std::uint32_t unitAIParkedOnEnter(std::uint8_t* self, std::uint32_t eax,
                                  const std::uint32_t* stack, int argc, AIState79Env& env);

std::uint32_t unitAIGetBestBoneOnExit(std::uint8_t* self, std::uint32_t eax,
                                      const std::uint32_t* stack, int argc, AIState79Env& env);
std::uint32_t unitAIUpdateNodeOnEnter(std::uint8_t* self, std::uint32_t eax,
                                      const std::uint32_t* stack, int argc, AIState79Env& env);
std::uint32_t unitAIWaitForDeliveryUpdate(std::uint8_t* self, std::uint32_t eax,
                                          const std::uint32_t* stack, int argc, AIState79Env& env);
std::uint32_t unitAIWaitForDeliveryOnExit(std::uint8_t* self, std::uint32_t eax,
                                          const std::uint32_t* stack, int argc, AIState79Env& env);
std::uint32_t unitAIWaitForMinerUpdate(std::uint8_t* self, std::uint32_t eax,
                                       const std::uint32_t* stack, int argc, AIState79Env& env);

// TurretAIIdleState 0x00C29B20. onEnter pushes TurretStates\Idle.cpp.
std::uint32_t turretAIIdleOnEnter(std::uint8_t* self, std::uint32_t eax,
                                  const std::uint32_t* stack, int argc, AIState79Env& env);
std::uint32_t turretAIIdleUpdate(std::uint8_t* self, std::uint32_t eax,
                                 const std::uint32_t* stack, int argc, AIState79Env& env);

std::uint32_t unitAIAimFireAtPositionOnEnter(std::uint8_t* self, std::uint32_t eax,
                                             const std::uint32_t* stack, int argc, AIState79Env& env);
std::uint32_t unitAIAimFireAtPositionOnExit(std::uint8_t* self, std::uint32_t eax,
                                            const std::uint32_t* stack, int argc, AIState79Env& env);
std::uint32_t unitAIAttackObjectSlot19(std::uint8_t* self, std::uint32_t eax,
                                       const std::uint32_t* stack, int argc, AIState79Env& env);
std::uint32_t unitAIAttackPositionSlot19(std::uint8_t* self, std::uint32_t eax,
                                         const std::uint32_t* stack, int argc, AIState79Env& env);
std::uint32_t unitAICreatePathOnExit(std::uint8_t* self, std::uint32_t eax,
                                     const std::uint32_t* stack, int argc, AIState79Env& env);
std::uint32_t unitAIDeleteOnEnter(std::uint8_t* self, std::uint32_t eax,
                                  const std::uint32_t* stack, int argc, AIState79Env& env);
std::uint32_t unitAIForceAttackObjectSlot19(std::uint8_t* self, std::uint32_t eax,
                                            const std::uint32_t* stack, int argc, AIState79Env& env);
std::uint32_t unitAIHoldFireOnEnter(std::uint8_t* self, std::uint32_t eax,
                                    const std::uint32_t* stack, int argc, AIState79Env& env);
std::uint32_t unitAIIdleOnEnter(std::uint8_t* self, std::uint32_t eax,
                                const std::uint32_t* stack, int argc, AIState79Env& env);
std::uint32_t unitAIRotateToAngleOnExit(std::uint8_t* self, std::uint32_t eax,
                                        const std::uint32_t* stack, int argc, AIState79Env& env);
std::uint32_t unitAISetLocomotorOnEnter(std::uint8_t* self, std::uint32_t eax,
                                        const std::uint32_t* stack, int argc, AIState79Env& env);
std::uint32_t unitAISuperStateSlot15(std::uint8_t* self, std::uint32_t eax,
                                     const std::uint32_t* stack, int argc, AIState79Env& env);
std::uint32_t unitAITimedIdleOnEnter(std::uint8_t* self, std::uint32_t eax,
                                     const std::uint32_t* stack, int argc, AIState79Env& env);
std::uint32_t unitAITimedIdleUpdate(std::uint8_t* self, std::uint32_t eax,
                                    const std::uint32_t* stack, int argc, AIState79Env& env);
std::uint32_t unitAIWaitForOccupantsOnExit(std::uint8_t* self, std::uint32_t eax,
                                           const std::uint32_t* stack, int argc, AIState79Env& env);
std::uint32_t aiDockWaitForClearanceOnEnter(std::uint8_t* self, std::uint32_t eax,
                                            const std::uint32_t* stack, int argc, AIState79Env& env);
std::uint32_t aiDockProcessDockOnExit(std::uint8_t* self, std::uint32_t eax,
                                      const std::uint32_t* stack, int argc, AIState79Env& env);
std::uint32_t aiGuardRetaliateMachineSlot18(std::uint8_t* self, std::uint32_t eax,
                                            const std::uint32_t* stack, int argc, AIState79Env& env);

// AIIdleState 0x00C2C520. onEnter pushes AIStates.cpp.
std::uint32_t aiIdleOnEnter(std::uint8_t* self, std::uint32_t eax,
                            const std::uint32_t* stack, int argc, AIState79Env& env);
std::uint32_t aiInternalMoveToSlot19(std::uint8_t* self, std::uint32_t eax,
                                     const std::uint32_t* stack, int argc, AIState79Env& env);
std::uint32_t aiAimFireOnExit(std::uint8_t* self, std::uint32_t eax,
                              const std::uint32_t* stack, int argc, AIState79Env& env);
std::uint32_t aiAimFireSlot19(std::uint8_t* self, std::uint32_t eax,
                              const std::uint32_t* stack, int argc, AIState79Env& env);
std::uint32_t aiAttackStateSlot17(std::uint8_t* self, std::uint32_t eax,
                                  const std::uint32_t* stack, int argc, AIState79Env& env);
std::uint32_t aiAttackSquadStateSlot17(std::uint8_t* self, std::uint32_t eax,
                                       const std::uint32_t* stack, int argc, AIState79Env& env);
std::uint32_t turretAIHoldTurretUpdate(std::uint8_t* self, std::uint32_t eax,
                                       const std::uint32_t* stack, int argc, AIState79Env& env);
std::uint32_t aiFaceStateSlot8(std::uint8_t* self, std::uint32_t eax,
                               const std::uint32_t* stack, int argc, AIState79Env& env);
std::uint32_t turretStateMachineSlot13(std::uint8_t* self, std::uint32_t eax,
                                       const std::uint32_t* stack, int argc, AIState79Env& env);
std::uint32_t turretStateMachineOnExit(std::uint8_t* self, std::uint32_t eax,
                                       const std::uint32_t* stack, int argc, AIState79Env& env);
std::uint32_t turretStateMachineSlot7(std::uint8_t* self, std::uint32_t eax,
                                      const std::uint32_t* stack, int argc, AIState79Env& env);
std::uint32_t unitAICheckFarEnoughOnEnter(std::uint8_t* self, std::uint32_t eax,
                                          const std::uint32_t* stack, int argc, AIState79Env& env);
std::uint32_t unitAIParkOrientUpdate(std::uint8_t* self, std::uint32_t eax,
                                     const std::uint32_t* stack, int argc, AIState79Env& env);
std::uint32_t unitAIWaitForClearanceOnEnter(std::uint8_t* self, std::uint32_t eax,
                                            const std::uint32_t* stack, int argc, AIState79Env& env);
std::uint32_t unitAIAttemptSkipMoveOnEnter(std::uint8_t* self, std::uint32_t eax,
                                           const std::uint32_t* stack, int argc, AIState79Env& env);
std::uint32_t unitAICanEnterUpdate(std::uint8_t* self, std::uint32_t eax,
                                   const std::uint32_t* stack, int argc, AIState79Env& env);
std::uint32_t unitAICreatePathExactOnEnter(std::uint8_t* self, std::uint32_t eax,
                                           const std::uint32_t* stack, int argc, AIState79Env& env);
std::uint32_t unitAIEnterCoverOnEnter(std::uint8_t* self, std::uint32_t eax,
                                      const std::uint32_t* stack, int argc, AIState79Env& env);
std::uint32_t unitAIGarrisonedIdleUpdate(std::uint8_t* self, std::uint32_t eax,
                                         const std::uint32_t* stack, int argc, AIState79Env& env);

bool runAIState79(std::uint32_t va, std::uint8_t* self, std::uint32_t eaxIn,
                  const std::uint32_t* stack, int argc, AIState79Env& env, std::uint32_t& eaxOut);

}  // namespace ra3::gamelogic
