#include "ra3/gamelogic/AIState79.h"

#include <cstring>

namespace ra3::gamelogic {
namespace {

constexpr std::uint32_t kDone = 0xFFFFFFFFu;
constexpr std::uint32_t kFail = 0xFFFFFFFEu;

std::uint32_t load32(const std::uint8_t* base, std::uint32_t offset)
{
    std::uint32_t value = 0;
    std::memcpy(&value, base + offset, 4);
    return value;
}

void store32(std::uint8_t* base, std::uint32_t offset, std::uint32_t value)
{
    std::memcpy(base + offset, &value, 4);
}

void store16(std::uint8_t* base, std::uint32_t offset, std::uint16_t value)
{
    std::memcpy(base + offset, &value, 2);
}

std::uint8_t* asPtr(std::uint32_t address)
{
    return reinterpret_cast<std::uint8_t*>(static_cast<std::uintptr_t>(address));
}

std::uint32_t addressOf(const void* pointer)
{
    return static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(pointer));
}

float bitsToFloat(std::uint32_t bits)
{
    float value = 0.f;
    std::memcpy(&value, &bits, 4);
    return value;
}

std::uint32_t machineOf(const std::uint8_t* self)
{
    return load32(self, 0x18);
}

std::uint32_t unitOf(std::uint32_t machine)
{
    return load32(asPtr(machine), 0x24);
}

std::uint32_t locoOf(std::uint32_t unit)
{
    return load32(asPtr(unit), 0x374);
}

std::uint32_t frameOf(const AIState79Env& env)
{
    return load32(asPtr(env.gameLogic), 0x58);
}

// comiss left, right; jbe  — taken when left <= right or either side is NaN.
bool comissJbe(float left, float right)
{
    return !(left > right);
}

std::uint32_t call1(AIState79Env& env, std::uint32_t target, std::uint32_t ecx, std::uint32_t arg)
{
    return callExternal(env, target, ecx, &arg, 1);
}

std::uint32_t lowByteSet(std::uint32_t value, std::uint8_t byte)
{
    return (value & 0xFFFFFF00u) | byte;
}

}  // namespace

std::uint32_t callExternal(AIState79Env& env, std::uint32_t target, std::uint32_t ecx,
                           const std::uint32_t* args, int argc)
{
    return env.call(env.callCtx, target, ecx, args, argc);
}

// 0x00796F20 FollowWaypointPathStateMachine slot 16. Stores the stack float at +0x90.
std::uint32_t followWaypointPathStoreTolerance(std::uint8_t* self, std::uint32_t eax,
                                               const std::uint32_t* stack, int, AIState79Env&)
{
    store32(self, 0x90, stack[0]);
    return eax;
}

// 0x00796F50 slot 18. Stores the argument byte at +0x94 and returns it in AL.
std::uint32_t followWaypointPathStoreFlag(std::uint8_t* self, std::uint32_t eax,
                                          const std::uint32_t* stack, int, AIState79Env&)
{
    const auto byte = static_cast<std::uint8_t>(stack[0]);
    self[0x94] = byte;
    return lowByteSet(eax, byte);
}

// 0x007970A0 UnitAIPArkedState slot 6.
std::uint32_t unitAIParkedOnExit(std::uint8_t* self, std::uint32_t, const std::uint32_t* stack, int,
                                 AIState79Env& env)
{
    callExternal(env, 0x006F1990u, unitOf(machineOf(self)), nullptr, 0);
    return call1(env, 0x009CCDA0u, addressOf(self), stack[0]);
}

// 0x0079CC20 UnitAIPArkedState slot 4.
std::uint32_t unitAIParkedOnEnter(std::uint8_t* self, std::uint32_t, const std::uint32_t*, int,
                                  AIState79Env& env)
{
    callExternal(env, 0x006D2840u, unitOf(machineOf(self)), nullptr, 0);
    callExternal(env, 0x006A4EA0u, unitOf(machineOf(self)), nullptr, 0);
    return 0;
}

// 0x007971E0 UnitAIGetBestBoneState slot 6.
std::uint32_t unitAIGetBestBoneOnExit(std::uint8_t* self, std::uint32_t, const std::uint32_t*, int,
                                      AIState79Env& env)
{
    const std::uint32_t args[2] = {0x9Du, 1u};
    return callExternal(env, 0x006D2020u, unitOf(machineOf(self)), args, 2);
}

// 0x00797380 UnitAIUpdateNodeState slot 4.
std::uint32_t unitAIUpdateNodeOnEnter(std::uint8_t* self, std::uint32_t, const std::uint32_t*, int,
                                      AIState79Env& env)
{
    const std::uint32_t machine = machineOf(self);
    const std::uint32_t goal = callExternal(env, 0x005B9F30u, machine, nullptr, 0);
    if (goal == 0) {
        return kFail;
    }
    const std::uint32_t unit = unitOf(machine);
    const std::uint32_t node = load32(asPtr(unit), 0x39C);
    const std::uint32_t args[2] = {load32(asPtr(node), 0x1C), load32(asPtr(unit), 0xDC)};
    callExternal(env, 0x0067E860u, load32(asPtr(goal), 0x394), args, 2);
    return kDone;
}

// 0x00797440 UnitAIWaitForDeliveryState slot 5.
std::uint32_t unitAIWaitForDeliveryUpdate(std::uint8_t* self, std::uint32_t, const std::uint32_t*, int,
                                          AIState79Env& env)
{
    const std::uint32_t node = load32(asPtr(unitOf(machineOf(self))), 0x39C);
    if ((call1(env, 0x0067E350u, node, 0) & 0xFFu) != 0) {
        return kDone;
    }
    callExternal(env, 0x006C6D40u, node, nullptr, 0);
    return 0;
}

// 0x00797470 UnitAIWaitForDeliveryState slot 6.
std::uint32_t unitAIWaitForDeliveryOnExit(std::uint8_t* self, std::uint32_t, const std::uint32_t*, int,
                                          AIState79Env& env)
{
    std::uint32_t node = load32(asPtr(unitOf(machineOf(self))), 0x39C);
    if (node != 0) {
        const std::uint32_t args[2] = {0, 0};
        callExternal(env, 0x0067E340u, node, args, 2);
    }
    node = load32(asPtr(unitOf(machineOf(self))), 0x39C);
    return callExternal(env, 0x006C6D40u, node, nullptr, 0);
}

// 0x007974D0 UnitAIWaitForMinerState slot 5.
std::uint32_t unitAIWaitForMinerUpdate(std::uint8_t* self, std::uint32_t, const std::uint32_t*, int,
                                       AIState79Env& env)
{
    const std::uint32_t node = load32(asPtr(unitOf(machineOf(self))), 0x398);
    if ((call1(env, 0x0067E3E0u, node, 0) & 0xFFu) == 0) {
        return 0;
    }
    const std::uint32_t args[2] = {0, 0};
    callExternal(env, 0x0067E3D0u, node, args, 2);
    return kDone;
}

// 0x00797960 TurretAIIdleState slot 4. Pushes 0x00C2A8C8 (TurretStates\Idle.cpp).
std::uint32_t turretAIIdleOnEnter(std::uint8_t* self, std::uint32_t, const std::uint32_t*, int,
                                  AIState79Env& env)
{
    const std::uint32_t now = frameOf(env);
    const std::uint32_t info = load32(asPtr(machineOf(self)), 0x80);
    const std::uint32_t lo = load32(asPtr(info), 0x4C);
    const std::uint32_t hi = load32(asPtr(info), 0x50);
    const std::uint32_t args[4] = {lo, hi, 0x00C2A8C8u, 0x30u};
    const std::uint32_t rolled = callExternal(env, 0x005A94C0u, hi, args, 4);
    store32(self, 0x20, rolled + now);
    return 0;
}

// 0x007979A0 TurretAIIdleState slot 5.
std::uint32_t turretAIIdleUpdate(std::uint8_t* self, std::uint32_t, const std::uint32_t*, int,
                                 AIState79Env& env)
{
    const std::uint32_t info = load32(asPtr(machineOf(self)), 0x80);
    if (load32(asPtr(info), 0x4C) == 0 || load32(asPtr(info), 0x50) == 0) {
        return 0;
    }
    if (frameOf(env) < load32(self, 0x20)) {
        return 0;
    }
    return kDone;
}

// 0x00797AC0 UnitAIAimFireAtPositionState slot 4. Also on UnitAIAimFireAtObjectState.
std::uint32_t unitAIAimFireAtPositionOnEnter(std::uint8_t* self, std::uint32_t, const std::uint32_t*, int,
                                             AIState79Env& env)
{
    const std::uint32_t unit = unitOf(machineOf(self));
    const std::uint32_t loco = locoOf(unit);
    store32(self, 0x20, load32(asPtr(loco), 0x218));
    if ((call1(env, 0x007527C0u, loco, 0xCu) & 0xFFu) == 0) {
        store32(self, 0x20, kDone);
    }
    callExternal(env, 0x006A4EA0u, unit, nullptr, 0);
    return 0;
}

// 0x00797B00 UnitAIAimFireAtPositionState slot 6.
std::uint32_t unitAIAimFireAtPositionOnExit(std::uint8_t* self, std::uint32_t, const std::uint32_t*, int,
                                            AIState79Env& env)
{
    const std::uint32_t unit = unitOf(machineOf(self));
    const std::uint32_t loco = locoOf(unit);
    if (loco != 0) {
        const std::uint32_t mode = load32(self, 0x20);
        if (mode != kDone) {
            call1(env, 0x007527C0u, loco, mode);
        }
    }
    const std::uint32_t args[2] = {0x1Bu, 0u};
    return callExternal(env, 0x006D2020u, unit, args, 2);
}

// 0x00797C50 UnitAIAttackObjectState slot 19.
std::uint32_t unitAIAttackObjectSlot19(std::uint8_t* self, std::uint32_t, const std::uint32_t*, int,
                                       AIState79Env& env)
{
    const std::uint32_t goal = callExternal(env, 0x005B9F30u, machineOf(self), nullptr, 0);
    const std::uint32_t unit = unitOf(machineOf(self));
    const std::uint32_t args[3] = {unit, load32(self, 0x24), goal};
    return callExternal(env, 0x00682C20u, locoOf(unit), args, 3);
}

// 0x00797C80 UnitAIAttackPositionState slot 19.
std::uint32_t unitAIAttackPositionSlot19(std::uint8_t* self, std::uint32_t, const std::uint32_t*, int,
                                         AIState79Env& env)
{
    const std::uint32_t unit = unitOf(machineOf(self));
    return call1(env, 0x00682C40u, locoOf(unit), unit);
}

// 0x00797D60 UnitAICreatePathState slot 6. Also on UnitAICreateAttackPathState.
// EAX is the locomotor pointer (0 when the unit has none); the incoming value is overwritten.
std::uint32_t unitAICreatePathOnExit(std::uint8_t* self, std::uint32_t, const std::uint32_t*, int,
                                     AIState79Env& env)
{
    const std::uint32_t loco = locoOf(unitOf(machineOf(self)));
    if (loco != 0) {
        store32(asPtr(loco), 0x18C, env.oneBits);
    }
    return loco;
}

// 0x00797DD0 UnitAIDeleteState slot 4.
std::uint32_t unitAIDeleteOnEnter(std::uint8_t* self, std::uint32_t, const std::uint32_t*, int,
                                  AIState79Env& env)
{
    call1(env, 0x004EDD70u, env.gameLogic, unitOf(machineOf(self)));
    return kDone;
}

// 0x007981F0 UnitAIForceAttackObjectState slot 19.
std::uint32_t unitAIForceAttackObjectSlot19(std::uint8_t* self, std::uint32_t, const std::uint32_t*, int,
                                            AIState79Env& env)
{
    const std::uint32_t goal = callExternal(env, 0x005B9F30u, machineOf(self), nullptr, 0);
    const std::uint32_t unit = unitOf(machineOf(self));
    const std::uint32_t args[2] = {unit, goal};
    return callExternal(env, 0x00682C30u, locoOf(unit), args, 2);
}

// 0x007982D0 UnitAIHoldFireState slot 4.
std::uint32_t unitAIHoldFireOnEnter(std::uint8_t* self, std::uint32_t, const std::uint32_t*, int,
                                    AIState79Env& env)
{
    const std::uint32_t unit = unitOf(machineOf(self));
    const std::uint32_t args[3] = {unit, 0u, 1u};
    callExternal(env, 0x006AEFE0u, unit, args, 3);
    return 0;
}

// 0x007983D0 UnitAIIdleState slot 4. Also on UnitAIInCoverState.
std::uint32_t unitAIIdleOnEnter(std::uint8_t* self, std::uint32_t, const std::uint32_t*, int,
                                AIState79Env& env)
{
    callExternal(env, 0x006A4EA0u, unitOf(machineOf(self)), nullptr, 0);
    return 0;
}

// 0x00798690 UnitAIRotateToAngleState slot 6. Also on SA_AIFaceTargetState.
std::uint32_t unitAIRotateToAngleOnExit(std::uint8_t* self, std::uint32_t, const std::uint32_t*, int,
                                        AIState79Env& env)
{
    const std::uint32_t unit = unitOf(machineOf(self));
    const std::uint32_t loco = locoOf(unit);
    if (loco == 0) {
        return unit;
    }
    store32(asPtr(loco), 0x1F0, env.maxFloatBits);
    if (load32(asPtr(loco), 0x21C) == 3) {
        store32(asPtr(loco), 0x21C, 0);
    }
    return loco;
}

// 0x007987C0 UnitAISetLocomotorState slot 4.
std::uint32_t unitAISetLocomotorOnEnter(std::uint8_t* self, std::uint32_t, const std::uint32_t*, int,
                                        AIState79Env& env)
{
    call1(env, 0x007527C0u, locoOf(unitOf(machineOf(self))), load32(self, 0x20));
    return kDone;
}

// 0x00798880 UnitAISuperState slot 15. Null nested machine returns the incoming EAX.
std::uint32_t unitAISuperStateSlot15(std::uint8_t* self, std::uint32_t eax, const std::uint32_t*, int,
                                     AIState79Env& env)
{
    const std::uint32_t nested = load32(self, 0x20);
    if (nested == 0) {
        return eax;
    }
    return callExternal(env, 0x005AA1E0u, nested, nullptr, 0);
}

// 0x007988F0 UnitAITimedIdleState slot 4.
std::uint32_t unitAITimedIdleOnEnter(std::uint8_t* self, std::uint32_t, const std::uint32_t*, int,
                                     AIState79Env&)
{
    store32(self, 0x24, load32(self, 0x20));
    return 0;
}

// 0x00798900 UnitAITimedIdleState slot 5. Also the move-to deliver/extract updates.
// Decrement +0x24. EAX is 0 while the counter is still nonzero, else 0xFFFFFFFF.
std::uint32_t unitAITimedIdleUpdate(std::uint8_t* self, std::uint32_t, const std::uint32_t*, int,
                                    AIState79Env&)
{
    const std::uint32_t value = load32(self, 0x24) + kDone;
    store32(self, 0x24, value);
    return value == 0 ? kDone : 0u;
}

// 0x007989E0 UnitAIWaitForOccupantsState slot 6.
std::uint32_t unitAIWaitForOccupantsOnExit(std::uint8_t* self, std::uint32_t, const std::uint32_t* stack,
                                           int, AIState79Env& env)
{
    const std::uint32_t args[2] = {0xA8u, 0u};
    callExternal(env, 0x006D2020u, unitOf(machineOf(self)), args, 2);
    return call1(env, 0x009CCDA0u, addressOf(self), stack[0]);
}

// 0x00798CF0 AIDockWaitForClearanceState slot 4. Stamps TheGameLogic frame at +0x20.
std::uint32_t aiDockWaitForClearanceOnEnter(std::uint8_t* self, std::uint32_t, const std::uint32_t*, int,
                                            AIState79Env& env)
{
    store32(self, 0x20, frameOf(env));
    return 0;
}

// 0x00798DC0 AIDockProcessDockState slot 6.
std::uint32_t aiDockProcessDockOnExit(std::uint8_t* self, std::uint32_t, const std::uint32_t*, int,
                                      AIState79Env& env)
{
    return callExternal(env, 0x005AA250u, machineOf(self), nullptr, 0);
}

// 0x00798F00 AIGuardRetaliateMachine slot 18.
// AL is the only byte written: 1 when the child record exists and its +4 dword is 0.
std::uint32_t aiGuardRetaliateMachineSlot18(std::uint8_t* self, std::uint32_t, const std::uint32_t*, int,
                                            AIState79Env&)
{
    const std::uint32_t pointer = load32(self, 4);
    if (pointer == 0 || load32(asPtr(pointer), 4) != 0) {
        return pointer & 0xFFFFFF00u;
    }
    return lowByteSet(pointer, 1);
}

// 0x00799890 AIIdleState slot 4. Pushes 0x00C2C1A8 (AIStates.cpp).
std::uint32_t aiIdleOnEnter(std::uint8_t* self, std::uint32_t, const std::uint32_t*, int, AIState79Env& env)
{
    store16(self, 0x22, 0x000F);
    self[0x25] = 1;
    const std::uint32_t loco = locoOf(unitOf(machineOf(self)));
    if (loco == 0) {
        store16(self, 0x20, 0);
        return 0;
    }
    const std::uint32_t nameArgs[2] = {0x00C2C1A8u, 0x9F8u};
    const std::uint32_t keyed = callExternal(env, 0x006835B0u, loco, nameArgs, 2);
    const std::uint32_t rollArgs[2] = {0u, keyed};
    const std::uint32_t rolled = callExternal(env, 0x005A94C0u, loco, rollArgs, 2);
    store16(self, 0x20, static_cast<std::uint16_t>(rolled));
    return 0;
}

// 0x00799920 AIInternalMoveToState slot 19.
std::uint32_t aiInternalMoveToSlot19(std::uint8_t* self, std::uint32_t, const std::uint32_t*, int,
                                     AIState79Env& env)
{
    const std::uint32_t machine = machineOf(self);
    const std::uint32_t loco = locoOf(unitOf(machine));
    const std::uint32_t source = asPtr(machine)[0x44] != 0 ? machine + 0x48u : addressOf(self) + 0x20u;
    const std::uint32_t dest = addressOf(self) + 0x20u;
    const std::uint32_t args[2] = {dest, source};
    callExternal(env, 0x007525F0u, loco, args, 2);
    const auto flag = asPtr(loco)[0x428];
    self[0x4D] = flag;
    if (flag == 0) {
        store32(self, 0x48, frameOf(env));
        store32(self, 0x34, load32(self, 0x20));
        store32(self, 0x38, load32(self, 0x24));
        store32(self, 0x3C, load32(self, 0x28));
        self[0x40] = 1;
    }
    const std::uint32_t result = callExternal(env, 0x00713FD0u, loco, nullptr, 0);
    return lowByteSet(result, 1);
}

// 0x0079A0A0 AIAimFireState slot 6.
std::uint32_t aiAimFireOnExit(std::uint8_t* self, std::uint32_t, const std::uint32_t*, int, AIState79Env& env)
{
    const std::uint32_t args[2] = {0x1Bu, 0u};
    return callExternal(env, 0x006D2020u, unitOf(machineOf(self)), args, 2);
}

// 0x0079A0C0 AIAimFireState slot 19. Byte +0x21 selects a tail call to getGoalObject.
std::uint32_t aiAimFireSlot19(std::uint8_t* self, std::uint32_t, const std::uint32_t*, int, AIState79Env& env)
{
    if (self[0x21] == 0) {
        return 0;
    }
    return callExternal(env, 0x005B9F30u, machineOf(self), nullptr, 0);
}

// 0x0079A3D0 AIAttackState slot 17.
std::uint32_t aiAttackStateSlot17(std::uint8_t* self, std::uint32_t eax, const std::uint32_t*, int,
                                  AIState79Env& env)
{
    const std::uint32_t nested = load32(self, 0x24);
    if (nested == 0) {
        return eax & 0xFFFFFF00u;
    }
    return callExternal(env, 0x005AA200u, nested, nullptr, 0);
}

// 0x0079A4F0 AIAttackSquadState slot 17.
std::uint32_t aiAttackSquadStateSlot17(std::uint8_t* self, std::uint32_t eax, const std::uint32_t*, int,
                                       AIState79Env& env)
{
    const std::uint32_t nested = load32(self, 0x20);
    if (nested == 0) {
        return eax & 0xFFFFFF00u;
    }
    return callExternal(env, 0x005AA200u, nested, nullptr, 0);
}

// 0x0079B470 TurretAIHoldTurretState slot 5. Also AIDeployState and AIUnDeployState.
// 0 while TheGameLogic frame is still below the deadline at +0x20, else 0xFFFFFFFF.
std::uint32_t turretAIHoldTurretUpdate(std::uint8_t* self, std::uint32_t, const std::uint32_t*, int,
                                       AIState79Env& env)
{
    constexpr std::uint32_t kDeadlineOffset = 0x20;
    const std::uint32_t deadline = load32(self, kDeadlineOffset);
    return frameOf(env) < deadline ? 0u : kDone;
}

// 0x0079C490 AIFaceState slot 8.
std::uint32_t aiFaceStateSlot8(std::uint8_t* self, std::uint32_t, const std::uint32_t*, int, AIState79Env&)
{
    return load32(self, 0x28) == 2 ? 1u : 0u;
}

// 0x0079C520 TurretStateMachine slot 13. A live link makes EAX the game-logic pointer.
std::uint32_t turretStateMachineSlot13(std::uint8_t* self, std::uint32_t, const std::uint32_t*, int,
                                       AIState79Env& env)
{
    const std::uint32_t result = callExternal(env, 0x005C0840u, addressOf(self), nullptr, 0);
    const std::uint32_t link = load32(self, 0x80);
    if (link == 0) {
        return result;
    }
    store32(asPtr(link), 0x80, frameOf(env));
    return env.gameLogic;
}

// 0x0079C550 TurretStateMachine slot 6. The helper's EAX is kept on both paths.
std::uint32_t turretStateMachineOnExit(std::uint8_t* self, std::uint32_t, const std::uint32_t*, int,
                                       AIState79Env& env)
{
    const std::uint32_t result = callExternal(env, 0x005C08C0u, addressOf(self), nullptr, 0);
    const std::uint32_t link = load32(self, 0x80);
    if (link != 0) {
        store32(asPtr(link), 0x80, frameOf(env));
    }
    return result;
}

// 0x0079C580 TurretStateMachine slot 7. Missing current-state record uses id 999999.
std::uint32_t turretStateMachineSlot7(std::uint8_t* self, std::uint32_t, const std::uint32_t* stack, int,
                                      AIState79Env& env)
{
    const std::uint32_t current = load32(self, 4);
    const std::uint32_t previous = current == 0 ? 0xF423Fu : load32(asPtr(current), 4);
    const std::uint32_t argument = stack[0];
    const std::uint32_t result = call1(env, 0x005B6230u, addressOf(self), argument);
    const std::uint32_t link = load32(self, 0x80);
    if (link != 0 && previous != argument) {
        store32(asPtr(link), 0x80, frameOf(env));
    }
    return result;
}

// 0x0079C8D0 UnitAICheckFarEnoughState slot 4.
std::uint32_t unitAICheckFarEnoughOnEnter(std::uint8_t* self, std::uint32_t, const std::uint32_t*, int,
                                          AIState79Env& env)
{
    const std::uint32_t machine = machineOf(self);
    const std::uint32_t unit = unitOf(machine);
    const std::uint32_t probe = callExternal(env, 0x00686CB0u, unit, nullptr, 0);
    if (probe == 0) {
        return kFail;
    }
    const float dx = bitsToFloat(load32(asPtr(machine), 0x2C)) - bitsToFloat(load32(asPtr(unit), 0x38));
    const float dy = bitsToFloat(load32(asPtr(machine), 0x30)) - bitsToFloat(load32(asPtr(unit), 0x3C));
    const std::uint32_t radiusObj = load32(asPtr(load32(asPtr(load32(asPtr(probe), 4)), 4)), 0x16C);
    const float radius = radiusObj == 0 ? 0.f : bitsToFloat(load32(asPtr(radiusObj), 4));
    const float dist = dx * dx + dy * dy;
    if (comissJbe(dist, radius * radius)) {
        return kFail;
    }
    return kDone;
}

// 0x0079CCF0 UnitAIParkOrientState slot 5.
std::uint32_t unitAIParkOrientUpdate(std::uint8_t* self, std::uint32_t, const std::uint32_t*, int,
                                     AIState79Env& env)
{
    const std::uint32_t unit = unitOf(machineOf(self));
    if ((asPtr(unit)[0x471] & 1u) != 0) {
        return kFail;
    }
    if ((callExternal(env, 0x0079CC50u, addressOf(self), nullptr, 0) & 0xFFu) == 0) {
        return kFail;
    }
    return callExternal(env, 0x007985D0u, addressOf(self), nullptr, 0);
}

// 0x0079CFA0 UnitAIWaitForClearanceState slot 4. Bit 11 of the unit dword at +0x94.
std::uint32_t unitAIWaitForClearanceOnEnter(std::uint8_t* self, std::uint32_t, const std::uint32_t*, int,
                                            AIState79Env&)
{
    const std::uint32_t flags = load32(asPtr(unitOf(machineOf(self))), 0x94);
    return ((flags >> 11) & 1u) != 0 ? kDone : 0u;
}

// 0x0079E0C0 UnitAIAttemptSkipMoveState slot 4.
std::uint32_t unitAIAttemptSkipMoveOnEnter(std::uint8_t* self, std::uint32_t, const std::uint32_t*, int,
                                           AIState79Env& env)
{
    const std::uint32_t machine = machineOf(self);
    const std::uint32_t unit = unitOf(machine);
    std::uint32_t local[3] = {};
    env.blob = reinterpret_cast<std::uint8_t*>(local);
    env.blobBytes = sizeof(local);
    env.captureBlob = false;
    const std::uint32_t point = call1(env, 0x005BA110u, machine, addressOf(local));
    env.blob = nullptr;
    const float dx = bitsToFloat(load32(asPtr(point), 0)) - bitsToFloat(load32(asPtr(unit), 0x38));
    const float dy = bitsToFloat(load32(asPtr(point), 4)) - bitsToFloat(load32(asPtr(unit), 0x3C));
    const float radius = bitsToFloat(load32(asPtr(env.logicTable), 0x80));
    const float dist = dx * dx + dy * dy;
    if (comissJbe(radius * radius, dist)) {
        return kFail;
    }
    const std::uint32_t args[2] = {0x32u, 0u};
    callExternal(env, 0x006D2020u, unit, args, 2);
    return kDone;
}

// 0x0079E150 UnitAICanEnterState slot 5.
std::uint32_t unitAICanEnterUpdate(std::uint8_t* self, std::uint32_t, const std::uint32_t*, int,
                                   AIState79Env& env)
{
    const std::uint32_t machine = machineOf(self);
    const std::uint32_t unit = unitOf(machine);
    const std::uint32_t goal = callExternal(env, 0x005B9F30u, machine, nullptr, 0);
    if (unit == 0 || goal == 0 || callExternal(env, 0x006EFEB0u, unit, nullptr, 0) != goal) {
        return kFail;
    }
    return kDone;
}

// 0x0079E4D0 UnitAICreatePathExactState slot 4.
std::uint32_t unitAICreatePathExactOnEnter(std::uint8_t* self, std::uint32_t, const std::uint32_t*, int,
                                           AIState79Env& env)
{
    const std::uint32_t record = load32(self, 0x20);
    const std::uint32_t id = load32(asPtr(record), 0x80);
    if (id == 0) {
        return kFail;
    }
    std::uint32_t local[2] = {load32(asPtr(record), 0x88), load32(asPtr(record), 0x8C)};
    env.blob = reinterpret_cast<std::uint8_t*>(local);
    env.blobBytes = sizeof(local);
    env.captureBlob = true;
    const std::uint32_t unit = unitOf(machineOf(self));
    const std::uint32_t args[2] = {id, addressOf(local)};
    callExternal(env, 0x006B69E0u, locoOf(unit), args, 2);
    env.blob = nullptr;
    env.captureBlob = false;
    return kDone;
}

// 0x0079EA60 UnitAIEnterCoverState slot 4. The limit at 0x00BD95B8 is compared unsquared.
std::uint32_t unitAIEnterCoverOnEnter(std::uint8_t* self, std::uint32_t, const std::uint32_t*, int,
                                      AIState79Env& env)
{
    const std::uint32_t machine = machineOf(self);
    const std::uint32_t unit = unitOf(machine);
    const float dx = bitsToFloat(load32(asPtr(machine), 0x2C)) - bitsToFloat(load32(asPtr(unit), 0x38));
    const float dy = bitsToFloat(load32(asPtr(machine), 0x30)) - bitsToFloat(load32(asPtr(unit), 0x3C));
    if (comissJbe(bitsToFloat(env.limitBits), dx * dx + dy * dy)) {
        return kFail;
    }
    const std::uint32_t probe = callExternal(env, 0x00687C30u, unit, nullptr, 0);
    if (probe == 0) {
        return kFail;
    }
    callExternal(env, 0x006F6A20u, probe, nullptr, 0);
    return kDone;
}

// 0x0079F3D0 UnitAIGarrisonedIdleState slot 5.
std::uint32_t unitAIGarrisonedIdleUpdate(std::uint8_t* self, std::uint32_t, const std::uint32_t*, int,
                                         AIState79Env& env)
{
    const std::uint32_t found = callExternal(env, 0x006EFEB0u, unitOf(machineOf(self)), nullptr, 0);
    if (found != 0) {
        call1(env, 0x0079F330u, addressOf(self), found);
    }
    return 0;
}

bool runAIState79(std::uint32_t va, std::uint8_t* self, std::uint32_t eaxIn, const std::uint32_t* stack,
                  int argc, AIState79Env& env, std::uint32_t& eaxOut)
{
    switch (va) {
    case 0x00796F20u: eaxOut = followWaypointPathStoreTolerance(self, eaxIn, stack, argc, env); return true;
    case 0x00796F50u: eaxOut = followWaypointPathStoreFlag(self, eaxIn, stack, argc, env); return true;
    case 0x007970A0u: eaxOut = unitAIParkedOnExit(self, eaxIn, stack, argc, env); return true;
    case 0x007971E0u: eaxOut = unitAIGetBestBoneOnExit(self, eaxIn, stack, argc, env); return true;
    case 0x00797380u: eaxOut = unitAIUpdateNodeOnEnter(self, eaxIn, stack, argc, env); return true;
    case 0x00797440u: eaxOut = unitAIWaitForDeliveryUpdate(self, eaxIn, stack, argc, env); return true;
    case 0x00797470u: eaxOut = unitAIWaitForDeliveryOnExit(self, eaxIn, stack, argc, env); return true;
    case 0x007974D0u: eaxOut = unitAIWaitForMinerUpdate(self, eaxIn, stack, argc, env); return true;
    case 0x00797960u: eaxOut = turretAIIdleOnEnter(self, eaxIn, stack, argc, env); return true;
    case 0x007979A0u: eaxOut = turretAIIdleUpdate(self, eaxIn, stack, argc, env); return true;
    case 0x00797AC0u: eaxOut = unitAIAimFireAtPositionOnEnter(self, eaxIn, stack, argc, env); return true;
    case 0x00797B00u: eaxOut = unitAIAimFireAtPositionOnExit(self, eaxIn, stack, argc, env); return true;
    case 0x00797C50u: eaxOut = unitAIAttackObjectSlot19(self, eaxIn, stack, argc, env); return true;
    case 0x00797C80u: eaxOut = unitAIAttackPositionSlot19(self, eaxIn, stack, argc, env); return true;
    case 0x00797D60u: eaxOut = unitAICreatePathOnExit(self, eaxIn, stack, argc, env); return true;
    case 0x00797DD0u: eaxOut = unitAIDeleteOnEnter(self, eaxIn, stack, argc, env); return true;
    case 0x007981F0u: eaxOut = unitAIForceAttackObjectSlot19(self, eaxIn, stack, argc, env); return true;
    case 0x007982D0u: eaxOut = unitAIHoldFireOnEnter(self, eaxIn, stack, argc, env); return true;
    case 0x007983D0u: eaxOut = unitAIIdleOnEnter(self, eaxIn, stack, argc, env); return true;
    case 0x00798690u: eaxOut = unitAIRotateToAngleOnExit(self, eaxIn, stack, argc, env); return true;
    case 0x007987C0u: eaxOut = unitAISetLocomotorOnEnter(self, eaxIn, stack, argc, env); return true;
    case 0x00798880u: eaxOut = unitAISuperStateSlot15(self, eaxIn, stack, argc, env); return true;
    case 0x007988F0u: eaxOut = unitAITimedIdleOnEnter(self, eaxIn, stack, argc, env); return true;
    case 0x00798900u: eaxOut = unitAITimedIdleUpdate(self, eaxIn, stack, argc, env); return true;
    case 0x007989E0u: eaxOut = unitAIWaitForOccupantsOnExit(self, eaxIn, stack, argc, env); return true;
    case 0x00798CF0u: eaxOut = aiDockWaitForClearanceOnEnter(self, eaxIn, stack, argc, env); return true;
    case 0x00798DC0u: eaxOut = aiDockProcessDockOnExit(self, eaxIn, stack, argc, env); return true;
    case 0x00798F00u: eaxOut = aiGuardRetaliateMachineSlot18(self, eaxIn, stack, argc, env); return true;
    case 0x00799890u: eaxOut = aiIdleOnEnter(self, eaxIn, stack, argc, env); return true;
    case 0x00799920u: eaxOut = aiInternalMoveToSlot19(self, eaxIn, stack, argc, env); return true;
    case 0x0079A0A0u: eaxOut = aiAimFireOnExit(self, eaxIn, stack, argc, env); return true;
    case 0x0079A0C0u: eaxOut = aiAimFireSlot19(self, eaxIn, stack, argc, env); return true;
    case 0x0079A3D0u: eaxOut = aiAttackStateSlot17(self, eaxIn, stack, argc, env); return true;
    case 0x0079A4F0u: eaxOut = aiAttackSquadStateSlot17(self, eaxIn, stack, argc, env); return true;
    case 0x0079B470u: eaxOut = turretAIHoldTurretUpdate(self, eaxIn, stack, argc, env); return true;
    case 0x0079C490u: eaxOut = aiFaceStateSlot8(self, eaxIn, stack, argc, env); return true;
    case 0x0079C520u: eaxOut = turretStateMachineSlot13(self, eaxIn, stack, argc, env); return true;
    case 0x0079C550u: eaxOut = turretStateMachineOnExit(self, eaxIn, stack, argc, env); return true;
    case 0x0079C580u: eaxOut = turretStateMachineSlot7(self, eaxIn, stack, argc, env); return true;
    case 0x0079C8D0u: eaxOut = unitAICheckFarEnoughOnEnter(self, eaxIn, stack, argc, env); return true;
    case 0x0079CC20u: eaxOut = unitAIParkedOnEnter(self, eaxIn, stack, argc, env); return true;
    case 0x0079CCF0u: eaxOut = unitAIParkOrientUpdate(self, eaxIn, stack, argc, env); return true;
    case 0x0079CFA0u: eaxOut = unitAIWaitForClearanceOnEnter(self, eaxIn, stack, argc, env); return true;
    case 0x0079E0C0u: eaxOut = unitAIAttemptSkipMoveOnEnter(self, eaxIn, stack, argc, env); return true;
    case 0x0079E150u: eaxOut = unitAICanEnterUpdate(self, eaxIn, stack, argc, env); return true;
    case 0x0079E4D0u: eaxOut = unitAICreatePathExactOnEnter(self, eaxIn, stack, argc, env); return true;
    case 0x0079EA60u: eaxOut = unitAIEnterCoverOnEnter(self, eaxIn, stack, argc, env); return true;
    case 0x0079F3D0u: eaxOut = unitAIGarrisonedIdleUpdate(self, eaxIn, stack, argc, env); return true;
    default: return false;
    }
}

}  // namespace ra3::gamelogic
