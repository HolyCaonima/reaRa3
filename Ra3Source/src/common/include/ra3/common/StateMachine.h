#pragma once

#include <cstdint>

namespace ra3::common {

// RA3 StateMachine at vtable 0x00BE8178. Slot 2 (0x005B2440) returns the
// ASCII name at 0x00BE81C0. FailureState is the sibling vtable 0x00BE6CB0
// whose slot 2 (0x005AA2E0) returns 0x00BE6CFC.
//
// Generals Common/StateMachine.h and StateMachine.cpp supplied the role
// names (update, setState, conditions, goal object). Every body below is
// from the RA3 instruction stream. Constants are the immediates in those
// instructions: 0 continue, 0xFFFFFFFF success, 0xFFFFFFFE failure,
// 0x000F423F invalid, 0x000F423E machine-done, 0x270E / 0x270F exit ids,
// 0x000F423D the machine virtual at +0x18, recursion cap 0x14.
//
// work/references/generals/GeneralsMD/Code/GameEngine/Include/Common/StateMachine.h
// work/references/generals/GeneralsMD/Code/GameEngine/Source/Common/StateMachine.cpp

constexpr std::uint32_t kStateSuccess = 0xFFFFFFFFu;
constexpr std::uint32_t kStateFailure = 0xFFFFFFFEu;
constexpr std::uint32_t kInvalidStateId = 0x000F423Fu;
constexpr std::uint32_t kMachineDoneStateId = 0x000F423Eu;
constexpr std::uint32_t kExitMachineSuccess = 0x0000270Eu;
constexpr std::uint32_t kExitMachineFailure = 0x0000270Fu;
constexpr std::uint32_t kSpecialStateId = 0x000F423Du;
constexpr std::uint32_t kStateRecursionLimit = 0x14u;
// Dword at 0x00BE700C, loaded by the goal-position reset and the goal copy.
constexpr std::uint32_t kDefaultGoalScalarBits = 0x7F7FFFFFu;

// Callbacks stand in for calls that leave this cluster (map find, state
// virtuals, and the out-of-range goal/xfer helpers). `context` is opaque.
struct StateMachineHooks {
    void* context = nullptr;
    std::uint32_t (*findState)(void* context, std::uint32_t machine, std::uint32_t key) = nullptr;
    std::uint32_t (*callState)(void* context, std::uint32_t state, std::uint32_t slot,
                               std::uint32_t argument) = nullptr;
    std::uint32_t (*callMachine)(void* context, std::uint32_t machine, std::uint32_t slot,
                                 std::uint32_t argument) = nullptr;
    std::uint32_t (*callCondition)(void* context, std::uint32_t function, std::uint32_t state,
                                   std::uint32_t userData) = nullptr;
    std::uint32_t (*callExternal)(void* context, std::uint32_t id, std::uint32_t a,
                                  std::uint32_t b) = nullptr;
    std::uint32_t frame = 0;
    std::uint32_t* transitionDepth = nullptr;
    std::uint32_t* sleepDepth = nullptr;
};

// External ids shared by the oracle stubs and the checker.
constexpr std::uint32_t kExtOwnerProbe = 1;
constexpr std::uint32_t kExtOwnerTail = 2;
constexpr std::uint32_t kExtAssert = 3;
constexpr std::uint32_t kExtDebugGate = 4;
constexpr std::uint32_t kExtOwnerQuery = 5;
constexpr std::uint32_t kExtGoalLookup = 6;
constexpr std::uint32_t kExtGoalLookupBegin = 14;
constexpr std::uint32_t kExtGoalLookupEnd = 15;
constexpr std::uint32_t kExtDebugChain = 16;
constexpr std::uint32_t kExtMoving = 7;
constexpr std::uint32_t kExtUnitGoal = 8;
constexpr std::uint32_t kExtUnitKind = 9;
constexpr std::uint32_t kExtMoveProbe = 10;
constexpr std::uint32_t kExtCanPublish = 11;
constexpr std::uint32_t kExtAlreadyThere = 12;
constexpr std::uint32_t kExtPublish = 13;
constexpr std::uint32_t kExtPathAssign = 17;
constexpr std::uint32_t kExtXferOpen = 18;
constexpr std::uint32_t kExtXferName = 19;
constexpr std::uint32_t kExtXferCount = 20;
constexpr std::uint32_t kExtXferElement = 21;
constexpr std::uint32_t kExtXferClear = 22;
constexpr std::uint32_t kExtXferReserve = 23;
constexpr std::uint32_t kExtXferGrow = 24;

// Field writers and getters. `incoming` is the EAX the instruction stream
// does not write. Pointer-sized results are Win32 addresses.
std::uint32_t lockMachine(void* self, std::uint32_t incoming) noexcept;
std::uint32_t unlockMachine(void* self, std::uint32_t incoming) noexcept;
std::uint32_t storeGoalScalar(void* self, std::uint32_t incoming, std::uint32_t bits) noexcept;
std::uint32_t storeGoalScalarMutant(void* self, std::uint32_t incoming, std::uint32_t bits) noexcept;
std::uint32_t storeGoalDword(void* self, std::uint32_t value) noexcept;
std::uint32_t setGoalObject(void* self, std::uint32_t incoming, std::uint32_t object) noexcept;
std::uint32_t setGoalPosition(void* self, std::uint32_t incoming, std::uint32_t position,
                              std::uint32_t scalarBits) noexcept;
std::uint32_t resetGoalPosition(void* self, std::uint32_t incoming, std::uint32_t position) noexcept;
std::uint32_t copyGoalFields(void* self, std::uint32_t source) noexcept;
std::uint32_t pathPointAt(const void* self, std::int32_t index) noexcept;
std::uint32_t currentStateId(const void* self) noexcept;

std::uint32_t isInIdleState(void* self, std::uint32_t incoming, StateMachineHooks& hooks) noexcept;
std::uint32_t forwardStateSlot48(void* self, std::uint32_t incoming, StateMachineHooks& hooks) noexcept;
std::uint32_t callWhileUnlocked(void* self, StateMachineHooks& hooks) noexcept;
std::uint32_t notifyStateThenMachine(void* self, StateMachineHooks& hooks) noexcept;
std::uint32_t stateMapContains(void* self, std::uint32_t incoming, std::uint32_t key,
                               StateMachineHooks& hooks) noexcept;
std::uint32_t internalGetState(void* self, std::uint32_t key, StateMachineHooks& hooks) noexcept;

std::uint32_t scanConditions(void* state, std::uint32_t* outStatus, std::uint32_t incoming,
                             StateMachineHooks& hooks) noexcept;
std::uint32_t applyTransitionId(void* state, std::uint32_t transitionId,
                                StateMachineHooks& hooks) noexcept;
std::uint32_t checkTransitions(void* state, std::uint32_t status, StateMachineHooks& hooks) noexcept;
std::uint32_t checkSleepTransitions(void* state, std::uint32_t frames,
                                    StateMachineHooks& hooks) noexcept;
std::uint32_t internalSetState(void* self, std::uint32_t stateId, StateMachineHooks& hooks) noexcept;
std::uint32_t setState(void* self, std::uint32_t stateId, StateMachineHooks& hooks) noexcept;
std::uint32_t initDefaultState(void* self, StateMachineHooks& hooks) noexcept;
std::uint32_t updateStateMachine(void* self, StateMachineHooks& hooks) noexcept;

std::uint32_t failureStateExit(void* state, std::uint32_t argument, StateMachineHooks& hooks) noexcept;
std::uint32_t goalQuery(void* self, StateMachineHooks& hooks) noexcept;
std::uint32_t goalObjectLive(void* self, std::uint32_t incoming, StateMachineHooks& hooks) noexcept;
std::uint32_t publishGoalPosition(void* self, std::uint32_t out, StateMachineHooks& hooks) noexcept;
std::uint32_t assignGoalPath(void* self, std::uint32_t source, StateMachineHooks& hooks) noexcept;
std::uint32_t xferGoalPath(void* self, std::uint32_t xfer, std::uint32_t vector,
                           StateMachineHooks& hooks) noexcept;

}  // namespace ra3::common
