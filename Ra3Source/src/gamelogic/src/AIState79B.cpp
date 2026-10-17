#include "ra3/gamelogic/AIState79B.h"

#include <cstring>

namespace ra3::gamelogic {
namespace {

constexpr std::uint32_t kDone = 0xFFFFFFFFu;
constexpr std::uint32_t kFail = 0xFFFFFFFEu;
constexpr std::uint32_t kStateSentinel = 0x000F423Fu;

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

std::uint8_t* asPtr(std::uint32_t address)
{
    return reinterpret_cast<std::uint8_t*>(static_cast<std::uintptr_t>(address));
}

std::uint32_t addressOf(const void* pointer)
{
    return static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(pointer));
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

std::uint32_t lowByteSet(std::uint32_t value, std::uint8_t byte)
{
    return (value & 0xFFFFFF00u) | byte;
}

std::uint32_t call0(AIState79BEnv& env, std::uint32_t target, std::uint32_t ecx)
{
    return callExternal(env.core, target, ecx, nullptr, 0);
}

std::uint32_t call1(AIState79BEnv& env, std::uint32_t target, std::uint32_t ecx, std::uint32_t arg)
{
    return callExternal(env.core, target, ecx, &arg, 1);
}

std::uint32_t callN(AIState79BEnv& env, std::uint32_t target, std::uint32_t ecx,
                    const std::uint32_t* args, int argc)
{
    return callExternal(env.core, target, ecx, args, argc);
}

std::uint32_t callVirtual(AIState79BEnv& env, std::uint32_t obj, std::uint32_t slot,
                          const std::uint32_t* args, int argc)
{
    const auto function = load32(asPtr(load32(asPtr(obj), 0)), slot);
    return callExternal(env.core, function, obj, args, argc);
}

void publish(AIState79BEnv& env, void* data, std::uint32_t bytes)
{
    env.core.blob = static_cast<std::uint8_t*>(data);
    env.core.blobBytes = bytes;
    env.core.captureBlob = false;
}

void clearBlob(AIState79BEnv& env)
{
    env.core.blob = nullptr;
    env.core.blobBytes = 0;
    env.core.captureBlob = false;
}

std::uint32_t xferVersion(AIState79BEnv& env, std::uint32_t xfer, std::uint8_t first, std::uint8_t second,
                          std::uint8_t* secondOut)
{
    std::uint8_t version[2] = {first, second};
    publish(env, version, 2);
    const auto pointer = addressOf(version);
    const auto result = callVirtual(env, xfer, 0x18, &pointer, 1);
    if (secondOut != nullptr) {
        *secondOut = version[1];
    }
    clearBlob(env);
    return result;
}

void copy12(std::uint8_t* destination, const std::uint8_t* source)
{
    store32(destination, 0, load32(source, 0));
    store32(destination, 4, load32(source, 4));
    store32(destination, 8, load32(source, 8));
}

// Shared guard/attack xfer: version byte, present-flag, optional create, optional +0x24 tail.
std::uint32_t xferLinkedState(std::uint8_t* self, std::uint32_t eaxIn, std::uint32_t xfer, AIState79BEnv& env,
                              bool stampAl, std::uint32_t createSlot, std::uint32_t createArg, bool argFromByte,
                              bool xferTail)
{
    const auto mode = load32(asPtr(xfer), 4);
    if (((mode >> 3) & 1u) != 0) {
        // AIGuardObjectState shifts the mode in EAX and returns it. The others set AL.
        return stampAl ? lowByteSet(eaxIn, 1) : (mode >> 3);
    }
    xferVersion(env, xfer, 1, 1, nullptr);
    std::uint8_t flag = load32(self, 0x20) != 0 ? 1 : 0;
    publish(env, &flag, 1);
    const auto flagPtr = addressOf(&flag);
    auto result = callVirtual(env, xfer, 0x84, &flagPtr, 1);
    clearBlob(env);
    if (flag != 0) {
        if (load32(self, 0x20) == 0) {
            const auto arg = argFromByte ? static_cast<std::uint32_t>(self[0x25]) : createArg;
            store32(self, 0x20, callVirtual(env, machineOf(self), createSlot, &arg, 1));
        }
        if (flag != 0) {
            const auto id = load32(self, 0x20);
            result = callVirtual(env, xfer, 0x20, &id, 1);
        }
    }
    if (xferTail) {
        const auto field = addressOf(self) + 0x24u;
        result = callVirtual(env, xfer, 0x6c, &field, 1);
    }
    return result;
}

std::uint32_t aiHordeExitUpdate(std::uint8_t* self, std::uint32_t, const std::uint32_t*, int, AIState79BEnv& env)
{
    const auto machine = machineOf(self);
    const auto unit = unitOf(machine);
    const auto goal = call0(env, 0x005B9F30u, machine);
    if (unit == 0 || goal == 0) {
        return kFail;
    }
    const auto loco = locoOf(goal);
    if (loco != 0 && callVirtual(env, loco, 0x1F8, &unit, 1) == 2) {
        return 0;
    }
    const auto contain = call0(env, 0x006A7600u, unit);
    if (contain == 0) {
        return kFail;
    }
    const auto horde = callVirtual(env, contain, 0x84, nullptr, 0);
    if (horde == 0 || call0(env, 0x006A7600u, goal) == 0) {
        return kFail;
    }
    const auto member = callVirtual(env, call0(env, 0x006A7600u, goal), 0x7C, nullptr, 0);
    if (member == 0) {
        return kFail;
    }
    if ((callVirtual(env, member, 0, nullptr, 0) & 0xFFu) != 0) {
        return 0;
    }
    const std::uint32_t idArgs[2] = {load32(asPtr(unit), 4), unit};
    const auto id = callVirtual(env, member, 4, idArgs, 2);
    if (id == kDone) {
        return kFail;
    }
    const std::uint32_t exitArgs[2] = {unit, id};
    env.hordeGate = self[0x20];
    callVirtual(env, member, 8, exitArgs, 2);
    env.hordeGate = 0;
    if ((callVirtual(env, horde, 0xFC, nullptr, 0) & 0xFFu) == 0 && call0(env, 0x006EFE40u, unit) != 0) {
        return 0;
    }
    if (call0(env, 0x006EFE40u, unit) != 0) {
        env.hordeGate = self[0x20];
        callVirtual(env, member, 8, exitArgs, 2);
        env.hordeGate = 0;
    }
    return kDone;
}

std::uint32_t unitAIHordeEnterGoalOnEnter(std::uint8_t* self, std::uint32_t, const std::uint32_t*, int,
                                          AIState79BEnv& env)
{
    const auto machine = machineOf(self);
    const auto unit = unitOf(machine);
    const auto goal = call0(env, 0x005B9F30u, machine);
    if (unit == 0 || goal == 0 || (asPtr(goal)[0x471] & 1u) != 0) {
        return kFail;
    }
    const auto contain = call0(env, 0x006A7600u, unit);
    if (contain == 0) {
        return kFail;
    }
    const auto horde = callVirtual(env, contain, 0x84, nullptr, 0);
    if (horde == 0) {
        return kFail;
    }
    const std::uint32_t enterArgs[2] = {unit, 1u};
    const auto member = callN(env, 0x006A7620u, goal, enterArgs, 2);
    if (member == 0 || callVirtual(env, member, 0x84, nullptr, 0) != 0) {
        return kFail;
    }
    const std::uint32_t wide[3] = {unit, 1u, 0u};
    if ((callVirtual(env, member, 0x94, wide, 3) & 0xFFu) == 0) {
        return kFail;
    }
    const std::uint32_t atGoal[3] = {addressOf(asPtr(goal)) + 0x38u, 2u, goal};
    callVirtual(env, horde, 0x7C, atGoal, 3);
    if (((load32(asPtr(load32(asPtr(goal), 4)), 0xC0) >> 4) & 1u) == 0) {
        call1(env, 0x0077B1F0u, goal, 8u);
    }
    callVirtual(env, member, 0x98, &unit, 1);
    const std::uint32_t kick[2] = {unit, 0u};
    callVirtual(env, member, 0x40, kick, 2);
    return 0;
}

std::uint32_t aiExitUpdate(std::uint8_t* self, std::uint32_t, const std::uint32_t*, int, AIState79BEnv& env)
{
    const auto machine = machineOf(self);
    const auto unit = unitOf(machine);
    const auto goal = call0(env, 0x005B9F30u, machine);
    if (goal == 0) {
        return kFail;
    }
    const auto loco = locoOf(goal);
    if (loco != 0 && callVirtual(env, loco, 0x1F8, &unit, 1) == 2) {
        return 0;
    }
    if (call0(env, 0x006A7600u, goal) == 0) {
        return kFail;
    }
    const auto member = callVirtual(env, call0(env, 0x006A7600u, goal), 0x7C, nullptr, 0);
    if (member == 0) {
        return kFail;
    }
    if ((callVirtual(env, member, 0, nullptr, 0) & 0xFFu) != 0) {
        return 0;
    }
    const std::uint32_t idArgs[2] = {load32(asPtr(unit), 4), unit};
    const auto id = callVirtual(env, member, 4, idArgs, 2);
    if (id == kDone) {
        return kFail;
    }
    env.hordeGate = self[0x24];
    const std::uint32_t exitArgs[2] = {unit, id};
    callVirtual(env, member, 8, exitArgs, 2);
    env.hordeGate = 0;
    const auto head = load32(asPtr(machine), 4);
    const auto current = head == 0 ? kStateSentinel : load32(asPtr(head), 4);
    return current == load32(self, 4) ? kDone : 0u;
}

std::uint32_t unitAIReturnForLandingOnExit(std::uint8_t* self, std::uint32_t, const std::uint32_t*, int,
                                           AIState79BEnv& env)
{
    const auto machine = machineOf(self);
    const auto unit = unitOf(machine);
    const std::uint32_t mood[2] = {0xABu, 0u};
    auto result = callN(env, 0x006D2020u, unit, mood, 2);
    const auto goal = call0(env, 0x005B9F30u, machine);
    if (goal != 0) {
        const std::uint32_t query[2] = {load32(asPtr(goal), 0xDC), 0u};
        const auto producer = callN(env, 0x006E4760u, machine, query, 2);
        result = producer;
        if (producer != 0) {
            result = callVirtual(env, producer, 0x54, &unit, 1);
            const auto produced = load32(asPtr(unit), 0xDC);
            result = callVirtual(env, producer, 0x18, &produced, 1);
        }
    }
    if ((env.nameKeyGuard & 1u) == 0) {
        env.nameKeyGuard = static_cast<std::uint8_t>(env.nameKeyGuard | 1u);
        env.nameKey = call1(env, 0x00986150u, env.nameKeyPool, 0x00C18D98u);
    }
    const auto module = call1(env, 0x00687460u, unit, env.nameKey);
    result = module;
    if (module != 0) {
        const auto argument = load32(asPtr(load32(asPtr(module), 4)), 0x10);
        result = call1(env, 0x006C1D60u, unit, argument);
    }
    const auto child = load32(self, 0x20);
    if (child != 0) {
        const std::uint32_t one = 1u;
        result = callVirtual(env, child, 0, &one, 1);
    }
    store32(self, 0x20, 0);
    return result;
}

std::uint32_t unitAIEndWaypointPathMoveOnEnter(std::uint8_t* self, std::uint32_t, const std::uint32_t*, int,
                                               AIState79BEnv& env)
{
    const auto path = load32(self, 0x20);
    const auto table = load32(asPtr(path), 0x80);
    std::uint32_t chosen = 0;
    if (table != 0) {
        const auto count = static_cast<std::int32_t>(load32(asPtr(table), 0x4C));
        if (count > 0) {
            const std::uint32_t args[4] = {0u, static_cast<std::uint32_t>(count - 1), 0x00C2B140u, 0x75u};
            const auto index = callN(env, 0x005A94C0u, addressOf(self), args, 4);
            chosen = index <= 7u ? load32(asPtr(table), 0x20u + index * 4u) : 0u;
        }
    }
    store32(asPtr(path), 0x80, chosen);
    if ((callVirtual(env, path, 0x4C, nullptr, 0) & 0xFFu) != 0) {
        const auto node = call0(env, 0x00687C90u, unitOf(machineOf(self)));
        store32(asPtr(node), 0x30, chosen);
    }
    if (chosen == 0) {
        call1(env, 0x006B6E30u, locoOf(unitOf(machineOf(self))), table);
        return kDone;
    }
    return kFail;
}

std::uint32_t aiAttackAreaXfer(std::uint8_t* self, std::uint32_t eaxIn, const std::uint32_t* stack, int,
                               AIState79BEnv& env)
{
    return xferLinkedState(self, eaxIn, stack[0], env, true, 0x20, 0u, false, true);
}

std::uint32_t followWaypointPathMachineXfer(std::uint8_t* self, std::uint32_t, const std::uint32_t* stack, int,
                                            AIState79BEnv& env)
{
    const auto xfer = stack[0];
    std::uint8_t second = 0;
    xferVersion(env, xfer, 1, 2, &second);
    auto result = call1(env, 0x005C0A40u, addressOf(self), xfer);
    const std::uint32_t primary[2] = {xfer, addressOf(self) + 0x80u};
    result = callN(env, 0x00798150u, addressOf(self), primary, 2);
    const auto at88 = addressOf(self) + 0x88u;
    result = callVirtual(env, xfer, 0x44, &at88, 1);
    const auto at90 = addressOf(self) + 0x90u;
    result = callVirtual(env, xfer, 0x64, &at90, 1);
    const auto at94 = addressOf(self) + 0x94u;
    result = callVirtual(env, xfer, 0x84, &at94, 1);
    if (second >= 2) {
        const std::uint32_t extra[2] = {xfer, addressOf(self) + 0x84u};
        result = callN(env, 0x00798150u, addressOf(self), extra, 2);
    }
    return result;
}

std::uint32_t aiGuardObjectOnEnter(std::uint8_t* self, std::uint32_t, const std::uint32_t*, int, AIState79BEnv& env)
{
    const auto machine = machineOf(self);
    const auto loco = locoOf(unitOf(machine));
    std::uint32_t which = 1u;
    if (callVirtual(env, loco, 0x22C, nullptr, 0) == 0) {
        if (callVirtual(env, loco, 0x230, nullptr, 0) == 0) {
            return kFail;
        }
        which = 2u;
    }
    const auto created = callVirtual(env, machine, 0x24, &which, 1);
    store32(self, 0x20, created);
    if (callVirtual(env, loco, 0x23C, nullptr, 0) == 4) {
        call1(env, 0x00682F20u, loco, 0u);
        return kFail;
    }
    if (callVirtual(env, created, 0x14, nullptr, 0) == kFail) {
        return kFail;
    }
    const auto child = load32(self, 0x20);
    const auto argument = load32(asPtr(child), 0x68);
    return callVirtual(env, child, 0x1C, &argument, 1);
}

std::uint32_t unitAIFollowWaypointPathOnEnter(std::uint8_t* self, std::uint32_t, const std::uint32_t*, int,
                                              AIState79BEnv& env)
{
    const auto path = callVirtual(env, addressOf(self), 0x4C, nullptr, 0);
    store32(self, 0x20, path);
    const auto waypoint = call0(env, 0x0079EF90u, addressOf(self));
    if (waypoint == 0) {
        return kFail;
    }
    store32(asPtr(path), 0x80, waypoint);
    std::uint32_t local[2] = {};
    publish(env, local, sizeof(local));
    const auto localPtr = addressOf(local);
    const auto got = callVirtual(env, addressOf(self), 0x50, &localPtr, 1);
    clearBlob(env);
    store32(asPtr(path), 0x88, load32(asPtr(got), 0));
    store32(asPtr(path), 0x8C, load32(asPtr(got), 4));
    if ((callVirtual(env, path, 0x4C, nullptr, 0) & 0xFFu) != 0) {
        const auto node = call0(env, 0x00687C90u, unitOf(machineOf(self)));
        store32(asPtr(node), 0x30, waypoint);
    }
    const auto argument = load32(asPtr(path), 0x68);
    return callVirtual(env, path, 0x1C, &argument, 1);
}

std::uint32_t aiAttackSquadXfer(std::uint8_t* self, std::uint32_t eaxIn, const std::uint32_t* stack, int,
                                AIState79BEnv& env)
{
    return xferLinkedState(self, eaxIn, stack[0], env, true, 0x20, 0u, true, false);
}

std::uint32_t aiGuardXfer(std::uint8_t* self, std::uint32_t eaxIn, const std::uint32_t* stack, int, AIState79BEnv& env)
{
    return xferLinkedState(self, eaxIn, stack[0], env, true, 0x24, 0u, false, false);
}

std::uint32_t aiGuardAreaXfer(std::uint8_t* self, std::uint32_t eaxIn, const std::uint32_t* stack, int,
                              AIState79BEnv& env)
{
    return xferLinkedState(self, eaxIn, stack[0], env, true, 0x24, 3u, false, false);
}

std::uint32_t aiGuardObjectXfer(std::uint8_t* self, std::uint32_t eaxIn, const std::uint32_t* stack, int,
                                AIState79BEnv& env)
{
    return xferLinkedState(self, eaxIn, stack[0], env, false, 0x24, 1u, false, false);
}

std::uint32_t aiWaitUntilFinishedFiringUpdate(std::uint8_t* self, std::uint32_t, const std::uint32_t*, int,
                                              AIState79BEnv& env)
{
    const auto weapon = call1(env, 0x006A51A0u, unitOf(machineOf(self)), kDone);
    if (weapon == 0) {
        return kFail;
    }
    const auto owner = load32(asPtr(weapon), 4);
    if (static_cast<std::int32_t>(call0(env, 0x00686460u, owner)) >= 0) {
        const auto again = call0(env, 0x00686460u, owner);
        const auto extra = call0(env, 0x00685C20u, call0(env, 0x00688830u, weapon));
        const auto frame = load32(asPtr(env.core.gameLogic), 0x58);
        if (again + extra > frame) {
            return 0;
        }
    }
    const auto flag = call0(env, 0x00688810u, weapon);
    if (static_cast<std::int32_t>(call0(env, 0x00686460u, owner)) >= 0) {
        return kFail;
    }
    return flag == 0 ? kDone : 0u;
}

std::uint32_t unitAICombinedUnitOnEnter(std::uint8_t* self, std::uint32_t, const std::uint32_t*, int,
                                        AIState79BEnv& env)
{
    const auto machine = machineOf(self);
    const auto unit = unitOf(machine);
    const auto goal = call0(env, 0x005B9F30u, machine);
    auto result = kFail;
    if (goal != 0) {
        const auto state = callVirtual(env, addressOf(self), 0x4C, nullptr, 0);
        store32(self, 0x20, state);
        callVirtual(env, state, 0x38, &goal, 1);
        asPtr(state)[0x7C] = 0;
        const auto argument = load32(asPtr(state), 0x68);
        result = callVirtual(env, state, 0x1C, &argument, 1);
        if (result != kFail) {
            const auto link = load32(asPtr(locoOf(unit)), 0x214);
            if (link != 0) {
                store32(asPtr(link), 0x40, load32(asPtr(link), 0x40) | 8u);
            }
            return result;
        }
    }
    call0(env, 0x007000E0u, locoOf(unit));
    return result;
}

std::uint32_t unitAIDockWithNodeOnExit(std::uint8_t* self, std::uint32_t, const std::uint32_t* stack, int,
                                       AIState79BEnv& env)
{
    const auto machine = machineOf(self);
    const auto unit = unitOf(machine);
    const auto goal = call0(env, 0x005B9F30u, machine);
    auto result = machine;
    const auto loco = locoOf(unit);
    if (loco != 0) {
        result = call1(env, 0x006E6360u, loco, 0u);
    }
    if (goal != 0) {
        const std::uint32_t args[2] = {0x7D0u, load32(asPtr(unit), 0xDC)};
        result = callN(env, 0x0067E860u, load32(asPtr(goal), 0x394), args, 2);
        const auto miner = load32(asPtr(goal), 0x398);
        if (stack[0] == 1 && miner != 0) {
            result = call1(env, 0x0067E410u, miner, 0u);
        }
    }
    const auto child = load32(self, 0x20);
    if (child != 0) {
        const std::uint32_t one = 1u;
        result = callVirtual(env, child, 0, &one, 1);
    }
    store32(self, 0x20, 0);
    return result;
}

std::uint32_t unitAIAttackObjectXfer(std::uint8_t* self, std::uint32_t, const std::uint32_t* stack, int,
                                     AIState79BEnv& env)
{
    const auto xfer = stack[0];
    std::uint8_t second = 0;
    xferVersion(env, xfer, 1, 2, &second);
    if (second > 1) {
        const std::uint32_t args[2] = {xfer, addressOf(self) + 0x28u};
        callN(env, 0x00549C90u, xfer, args, 2);
    }
    xferVersion(env, xfer, 1, 1, nullptr);
    if (((load32(asPtr(xfer), 4) >> 1) & 1u) != 0) {
        store32(self, 0x20, callVirtual(env, addressOf(self), 0x4C, nullptr, 0));
    }
    const auto id = load32(self, 0x20);
    return callVirtual(env, xfer, 0x20, &id, 1);
}

std::uint32_t unitAIAimForceFireAtObjectOnEnter(std::uint8_t* self, std::uint32_t, const std::uint32_t*, int,
                                                AIState79BEnv& env)
{
    const auto machine = machineOf(self);
    const auto unit = unitOf(machine);
    const std::uint32_t mood[2] = {0x8Eu, 1u};
    callN(env, 0x006D2020u, unit, mood, 2);
    const auto loco = locoOf(unit);
    if (callVirtual(env, loco, 0x288, nullptr, 0) != 2) {
        const auto goal = call0(env, 0x005B9F30u, machine);
        call1(env, 0x006C0F40u, unit, goal);
    }
    store32(self, 0x20, load32(asPtr(loco), 0x218));
    if ((call1(env, 0x007527C0u, loco, 0xCu) & 0xFFu) == 0) {
        store32(self, 0x20, kDone);
    }
    call0(env, 0x006A4EA0u, unit);
    return 0;
}

std::uint32_t saAIPrepareOnEnter(std::uint8_t* self, std::uint32_t, const std::uint32_t*, int, AIState79BEnv& env)
{
    const auto machine = machineOf(self);
    const auto key = load32(asPtr(machine), 0x80);
    const auto unit = unitOf(machine);
    const auto found = call1(env, 0x007E2950u, env.registry, key);
    if (found == 0) {
        return kFail;
    }
    const auto module = call1(env, 0x006878B0u, unit, found);
    if (module == 0) {
        return kFail;
    }
    callVirtual(env, module, 0x60, nullptr, 0);
    const auto foundAgain = call1(env, 0x007E2950u, env.registry, load32(asPtr(machineOf(self)), 0x80));
    if (foundAgain == 0) {
        return kFail;
    }
    const auto moduleAgain = call1(env, 0x006878B0u, unitOf(machineOf(self)), foundAgain);
    if (moduleAgain == 0) {
        return kFail;
    }
    const auto byte = callVirtual(env, moduleAgain, 0x70, nullptr, 0);
    return (byte & 0xFFu) != 0 ? kDone : 0u;
}

std::uint32_t unitAIMoveToGoalOnExit(std::uint8_t* self, std::uint32_t, const std::uint32_t*, int, AIState79BEnv& env)
{
    const auto child = load32(self, 0x20);
    if (child != 0) {
        const std::uint32_t one = 1u;
        callVirtual(env, child, 0, &one, 1);
    }
    store32(self, 0x20, 0);
    const auto machine = machineOf(self);
    const auto unit = unitOf(machine);
    if (((load32(asPtr(unit), 0x90) >> 29) & 1u) != 0) {
        const std::uint32_t args[2] = {0x9Du, 0u};
        return callN(env, 0x006D2020u, unit, args, 2);
    }
    const auto loco = locoOf(unit);
    if (loco == 0) {
        return machine;
    }
    const std::uint32_t args[2] = {0x00BF21D8u, 0u};
    callN(env, 0x006B7040u, loco, args, 2);
    return call0(env, 0x00682D70u, loco);
}

std::uint32_t aiGuardUpdate(std::uint8_t* self, std::uint32_t, const std::uint32_t*, int, AIState79BEnv& env)
{
    if (load32(self, 0x20) == 0) {
        return kFail;
    }
    const auto machine = machineOf(self);
    const auto held = call0(env, 0x00687CB0u, unitOf(machine));
    if (held != 0 && call0(env, 0x00A780D0u, held) == 1) {
        const auto child = load32(self, 0x20);
        const auto link = load32(asPtr(child), 4);
        if (link == 0 || (callVirtual(env, link, 0x20, nullptr, 0) & 0xFFu) != 0) {
            return kFail;
        }
    }
    call1(env, 0x005AA240u, machine, 0x00C2D08Cu);
    const auto child = load32(self, 0x20);
    const auto result = callVirtual(env, child, 0x10, nullptr, 0);
    call0(env, 0x005AA250u, machine);
    return result;
}

std::uint32_t aiDockUpdate(std::uint8_t* self, std::uint32_t, const std::uint32_t*, int, AIState79BEnv& env)
{
    const auto child = load32(self, 0x20);
    const auto link = load32(asPtr(child), 4);
    const auto id = link == 0 ? kStateSentinel : load32(asPtr(link), 4);
    if (id != 5 && id != 6 && id != 7) {
        const auto goal = call0(env, 0x005B9F30u, child);
        if (goal != 0) {
            const auto left = call0(env, 0x00686FA0u, unitOf(machineOf(self)));
            if (call0(env, 0x00686FA0u, goal) != left) {
                return kFail;
            }
        }
    }
    const auto result = callVirtual(env, load32(self, 0x20), 0x10, nullptr, 0);
    return static_cast<std::int32_t>(result) > 0 ? 0u : result;
}

std::uint32_t aiHordeExitOnEnter(std::uint8_t* self, std::uint32_t, const std::uint32_t*, int, AIState79BEnv& env)
{
    const auto machine = machineOf(self);
    const auto unit = unitOf(machine);
    const auto goal = call0(env, 0x005B9F30u, machine);
    if (unit == 0 || goal == 0) {
        return kFail;
    }
    const auto contain = call0(env, 0x006A7600u, unit);
    if (contain == 0 || callVirtual(env, contain, 0x84, nullptr, 0) == 0) {
        return kFail;
    }
    const auto other = call0(env, 0x006A7600u, goal);
    if (other != 0) {
        const std::uint32_t args[2] = {unit, 1u};
        callVirtual(env, other, 0x40, args, 2);
    }
    const auto loco = locoOf(unit);
    if (loco != 0) {
        call1(env, 0x006E6360u, loco, goal);
    }
    return 0;
}

std::uint32_t aiMoveToStateSASlot19(std::uint8_t* self, std::uint32_t eax, const std::uint32_t*, int,
                                    AIState79BEnv& env)
{
    if (self[0x4D] != 0) {
        return lowByteSet(eax, 1);
    }
    const auto machine = machineOf(self);
    if (call0(env, 0x005B9F30u, machine) != 0) {
        const auto goal = call0(env, 0x005B9F30u, machine);
        copy12(self + 0x20, asPtr(goal) + 0x38);
        call1(env, 0x006A72A0u, goal, addressOf(self) + 0x20u);
    } else {
        copy12(self + 0x20, asPtr(machine) + 0x2C);
    }
    return call0(env, 0x00799920u, addressOf(self));
}

std::uint32_t unitAIHordeEnterGoalUpdate(std::uint8_t* self, std::uint32_t, const std::uint32_t*, int,
                                         AIState79BEnv& env)
{
    const auto machine = machineOf(self);
    const auto unit = unitOf(machine);
    const auto goal = call0(env, 0x005B9F30u, machine);
    if (unit == 0 || goal == 0) {
        return kFail;
    }
    const auto contain = call0(env, 0x006A7600u, unit);
    if (contain == 0) {
        return kFail;
    }
    const auto horde = callVirtual(env, contain, 0x84, nullptr, 0);
    if (horde == 0) {
        return kFail;
    }
    if ((callVirtual(env, horde, 0xF8, nullptr, 0) & 0xFFu) != 0) {
        return kDone;
    }
    callVirtual(env, horde, 0x100, &goal, 1);
    return 0;
}

std::uint32_t unitAIFollowWaypointPathExactOnEnter(std::uint8_t* self, std::uint32_t, const std::uint32_t*, int,
                                                   AIState79BEnv& env)
{
    const auto inner = call0(env, 0x0079F030u, addressOf(self));
    if (inner != kFail) {
        const std::uint32_t one = 1u;
        callVirtual(env, load32(self, 0x20), 0x48, &one, 1);
    }
    const auto loco = locoOf(unitOf(machineOf(self)));
    const auto link = load32(asPtr(loco), 0x214);
    if (link == 0) {
        return kFail;
    }
    asPtr(loco)[0x434] = static_cast<std::uint8_t>(1);
    store32(asPtr(link), 0x40, load32(asPtr(link), 0x40) | 2u);
    return inner;
}

std::uint32_t aiHordeExitOnExit(std::uint8_t* self, std::uint32_t, const std::uint32_t*, int, AIState79BEnv& env)
{
    const auto machine = machineOf(self);
    const auto goal = call0(env, 0x005B9F30u, machine);
    if (goal != 0 && call0(env, 0x006A7600u, goal) != 0) {
        const auto contain = call0(env, 0x006A7600u, goal);
        const auto unit = unitOf(machine);
        const std::uint32_t args[2] = {unit, 2u};
        callVirtual(env, contain, 0x40, args, 2);
    }
    const auto loco = locoOf(unitOf(machine));
    if (loco != 0) {
        call1(env, 0x006E6360u, loco, 0u);
    }
    const std::uint32_t zero = 0u;
    return callVirtual(env, machine, 0x38, &zero, 1);
}

std::uint32_t unitAIHeliEvacuateXfer(std::uint8_t* self, std::uint32_t, const std::uint32_t* stack, int,
                                     AIState79BEnv& env)
{
    const auto xfer = stack[0];
    xferVersion(env, xfer, 1, 1, nullptr);
    xferVersion(env, xfer, 1, 1, nullptr);
    if (((load32(asPtr(xfer), 4) >> 1) & 1u) != 0) {
        store32(self, 0x20, callVirtual(env, addressOf(self), 0x4C, nullptr, 0));
    }
    const auto id = load32(self, 0x20);
    return callVirtual(env, xfer, 0x20, &id, 1);
}

std::uint32_t unitAIDeliverOreOnExit(std::uint8_t* self, std::uint32_t, const std::uint32_t*, int, AIState79BEnv& env)
{
    const auto machine = machineOf(self);
    const auto unit = unitOf(machine);
    const auto goal = call0(env, 0x005B9F30u, machine);
    auto result = machine;
    const auto loco = locoOf(unit);
    if (loco != 0) {
        result = call1(env, 0x006E6360u, loco, 0u);
    }
    if (goal != 0) {
        const std::uint32_t args[2] = {0x7D0u, load32(asPtr(unit), 0xDC)};
        result = callN(env, 0x0067E860u, load32(asPtr(goal), 0x394), args, 2);
    }
    const auto child = load32(self, 0x20);
    if (child != 0) {
        const std::uint32_t one = 1u;
        result = callVirtual(env, child, 0, &one, 1);
    }
    store32(self, 0x20, 0);
    return result;
}

std::uint32_t unitAIForceAttackObjectOnExit(std::uint8_t* self, std::uint32_t, const std::uint32_t*, int,
                                            AIState79BEnv& env)
{
    const auto unit = unitOf(machineOf(self));
    const std::uint32_t mood[2] = {0x8Eu, 0u};
    callN(env, 0x006D2020u, unit, mood, 2);
    call0(env, 0x006F0440u, unit);
    call1(env, 0x006C0F40u, unit, 0u);
    auto result = call1(env, 0x0067D870u, unit, unit);
    if (result != 0) {
        const std::uint32_t zero = 0u;
        result = callVirtual(env, result, 4, &zero, 1);
    }
    const auto child = load32(self, 0x20);
    if (child != 0) {
        const std::uint32_t one = 1u;
        result = callVirtual(env, child, 0, &one, 1);
    }
    store32(self, 0x20, 0);
    return result;
}

std::uint32_t unitAIResetEnterOnEnter(std::uint8_t* self, std::uint32_t, const std::uint32_t*, int, AIState79BEnv& env)
{
    const auto machine = machineOf(self);
    const auto goal = call0(env, 0x005B9F30u, machine);
    if (goal == 0) {
        return kDone;
    }
    const auto unit = unitOf(machine);
    const std::uint32_t args[2] = {unit, 1u};
    const auto member = callN(env, 0x006A7620u, goal, args, 2);
    if (member != 0) {
        const std::uint32_t kick[2] = {unit, 2u};
        callVirtual(env, member, 0x40, kick, 2);
        const auto loco = locoOf(unit);
        if (loco != 0) {
            call1(env, 0x00683140u, loco, 0u);
        }
        asPtr(machine)[0x7C] = static_cast<std::uint8_t>(1);
    }
    return kDone;
}

std::uint32_t aiPickUpCrateOnEnter(std::uint8_t* self, std::uint32_t, const std::uint32_t*, int, AIState79BEnv& env)
{
    const auto machine = machineOf(self);
    const auto found = call0(env, 0x006E5CE0u, locoOf(unitOf(machine)));
    if (found == 0) {
        return kDone;
    }
    callVirtual(env, machine, 0x38, &found, 1);
    const auto goal = call0(env, 0x005B9F30u, machine);
    if (goal == 0) {
        return kFail;
    }
    copy12(self + 0x20, asPtr(goal) + 0x38);
    self[0x4C] = static_cast<std::uint8_t>(1);
    store32(self, 0x50, 3u);
    return 0;
}

std::uint32_t saAIValidateApproachTargetOnEnter(std::uint8_t* self, std::uint32_t, const std::uint32_t*, int,
                                                AIState79BEnv& env)
{
    const auto machine = machineOf(self);
    const auto found = call1(env, 0x007E2950u, env.registry, load32(asPtr(machine), 0x80));
    if (found == 0) {
        return kDone;
    }
    const auto module = call1(env, 0x006878B0u, unitOf(machine), found);
    if (module == 0) {
        return kDone;
    }
    const auto bits = load32(asPtr(load32(asPtr(module), 4)), 0x2C);
    if (((bits >> 30) & 1u) == 0) {
        return kDone;
    }
    const auto point = unitOf(machineOf(self)) + 0x38u;
    if ((call1(env, 0x00718FA0u, module, point) & 0xFFu) == 0) {
        return kFail;
    }
    return kDone;
}

std::uint32_t aiCowerOnExit(std::uint8_t* self, std::uint32_t, const std::uint32_t* stack, int, AIState79BEnv& env)
{
    auto result = call1(env, 0x009CCDA0u, addressOf(self), stack[0]);
    const auto unit = unitOf(machineOf(self));
    if (unit == 0) {
        return result;
    }
    call1(env, 0x006F17F0u, unit, 4u);
    if (call0(env, 0x006A7600u, unit) == 0) {
        return 0;
    }
    const auto contain = call0(env, 0x006A7600u, unit);
    const auto member = callVirtual(env, contain, 0x84, nullptr, 0);
    if (member == 0) {
        return 0;
    }
    const std::uint32_t zero = 0u;
    return callVirtual(env, member, 0x1A0, &zero, 1);
}

std::uint32_t unitAIMoveOnPathXfer(std::uint8_t* self, std::uint32_t, const std::uint32_t* stack, int,
                                   AIState79BEnv& env)
{
    const auto xfer = stack[0];
    xferVersion(env, xfer, 1, 1, nullptr);
    auto result = xferVersion(env, xfer, 1, 1, nullptr);
    const auto root = env.core.logicTable;
    if (root != 0) {
        const std::uint32_t args[2] = {xfer, addressOf(self) + 0x20u};
        result = callVirtual(env, root, 0x158, args, 2);
    }
    return result;
}

std::uint32_t aiGuardRetaliateMachineXfer(std::uint8_t* self, std::uint32_t eaxIn, const std::uint32_t* stack, int,
                                          AIState79BEnv& env)
{
    const auto xfer = stack[0];
    if (((load32(asPtr(xfer), 4) >> 3) & 1u) != 0) {
        return lowByteSet(eaxIn, 1);
    }
    xferVersion(env, xfer, 1, 1, nullptr);
    call1(env, 0x005C0A40u, addressOf(self), xfer);
    const std::uint32_t args[2] = {xfer, addressOf(self) + 0x8Cu};
    callN(env, 0x0054A090u, addressOf(self), args, 2);
    const auto field = addressOf(self) + 0x80u;
    return callVirtual(env, xfer, 0x58, &field, 1);
}

std::uint32_t unitAIHordeForceMembersToExitGoalOnEnter(std::uint8_t* self, std::uint32_t, const std::uint32_t*, int,
                                                       AIState79BEnv& env)
{
    const auto machine = machineOf(self);
    const auto unit = unitOf(machine);
    const auto goal = call0(env, 0x005B9F30u, machine);
    if (unit == 0 || goal == 0) {
        return kFail;
    }
    const auto contain = call0(env, 0x006A7600u, unit);
    if (contain == 0 || callVirtual(env, contain, 0x84, nullptr, 0) == 0) {
        return kFail;
    }
    const auto other = call0(env, 0x006A7600u, goal);
    if (other != 0) {
        const std::uint32_t args[2] = {unit, 1u};
        callVirtual(env, other, 0x40, args, 2);
    }
    return 0;
}

std::uint32_t unitAIHordeEnterGoalOnExit(std::uint8_t* self, std::uint32_t, const std::uint32_t*, int,
                                         AIState79BEnv& env)
{
    const auto machine = machineOf(self);
    const auto goal = call0(env, 0x005B9F30u, machine);
    if (goal == 0) {
        return 0;
    }
    if (((load32(asPtr(load32(asPtr(goal), 4)), 0xC0) >> 4) & 1u) == 0) {
        call1(env, 0x00706E00u, goal, 8u);
    }
    if (call0(env, 0x006A7600u, goal) == 0) {
        return 0;
    }
    const auto contain = call0(env, 0x006A7600u, goal);
    const auto unit = unitOf(machine);
    const std::uint32_t args[2] = {unit, 2u};
    return callVirtual(env, contain, 0x40, args, 2);
}

std::uint32_t unitAIDoSpecialPowerOnEnter(std::uint8_t* self, std::uint32_t, const std::uint32_t*, int,
                                          AIState79BEnv& env)
{
    const auto machine = machineOf(self);
    const auto key = load32(asPtr(machine), 0x70);
    store32(self, 0x20, key);
    const auto found = call1(env, 0x007E2950u, env.registry, key);
    std::uint32_t goal = 0;
    if (found != 0) {
        const auto bits = load32(asPtr(load32(asPtr(found), 8)), 0x1C);
        if (((bits >> 26) & 1u) == 0) {
            goal = call0(env, 0x005B9F30u, machine);
        }
    }
    const auto unit = unitOf(machine);
    call1(env, 0x006876D0u, unit, 0u);
    call1(env, 0x006C0F40u, unit, goal);
    return 0;
}

std::uint32_t aiExitOnEnter(std::uint8_t* self, std::uint32_t, const std::uint32_t*, int, AIState79BEnv& env)
{
    store32(self, 0x20, 0);
    const auto machine = machineOf(self);
    const auto unit = unitOf(machine);
    const auto goal = call0(env, 0x005B9F30u, machine);
    if (goal == 0) {
        return kFail;
    }
    const auto contain = call0(env, 0x006A7600u, goal);
    if (contain != 0) {
        const std::uint32_t args[2] = {unit, 1u};
        callVirtual(env, contain, 0x40, args, 2);
        store32(self, 0x20, load32(asPtr(goal), 0xDC));
    }
    return 0;
}

std::uint32_t unitAIAimForceFireAtObjectOnExit(std::uint8_t* self, std::uint32_t, const std::uint32_t*, int,
                                               AIState79BEnv& env)
{
    const auto unit = unitOf(machineOf(self));
    const std::uint32_t mood[2] = {0x8Eu, 0u};
    callN(env, 0x006D2020u, unit, mood, 2);
    const auto loco = locoOf(unit);
    const auto saved = load32(self, 0x20);
    if (loco != 0 && saved != kDone) {
        call1(env, 0x007527C0u, loco, saved);
    }
    const std::uint32_t clear[2] = {0x1Bu, 0u};
    return callN(env, 0x006D2020u, unit, clear, 2);
}

std::uint32_t unitAIEndPathMoveOnEnter(std::uint8_t* self, std::uint32_t, const std::uint32_t*, int, AIState79BEnv& env)
{
    const auto path = load32(self, 0x20);
    store32(asPtr(path), 0x80, load32(asPtr(path), 0x80) + 1u);
    const auto unit = unitOf(machineOf(self));
    if ((callVirtual(env, path, 0x48, nullptr, 0) & 0xFFu) != 0) {
        call1(env, 0x00683140u, locoOf(unit), 0u);
    }
    if ((callVirtual(env, path, 0x4C, nullptr, 0) & 0xFFu) != 0) {
        call0(env, 0x006F19F0u, unit);
    }
    return kDone;
}

std::uint32_t unitAIEnterGoalOnExit(std::uint8_t* self, std::uint32_t, const std::uint32_t*, int, AIState79BEnv& env)
{
    const auto unit = unitOf(machineOf(self));
    const auto loco = locoOf(unit);
    if (loco != 0) {
        call1(env, 0x006E6360u, loco, 0u);
        const auto link = load32(asPtr(loco), 0x214);
        if (link != 0) {
            store32(asPtr(link), 0x40, load32(asPtr(link), 0x40) & ~2u);
        }
    }
    const std::uint32_t mood[2] = {0x61u, 0u};
    auto result = callN(env, 0x006D2020u, unit, mood, 2);
    const auto child = load32(self, 0x20);
    if (child != 0) {
        const std::uint32_t one = 1u;
        result = callVirtual(env, child, 0, &one, 1);
    }
    store32(self, 0x20, 0);
    return result;
}

std::uint32_t aiDockOnExit(std::uint8_t* self, std::uint32_t, const std::uint32_t*, int, AIState79BEnv& env)
{
    if (load32(self, 0x20) != 0) {
        callVirtual(env, load32(self, 0x20), 0x30, nullptr, 0);
        if (load32(self, 0x20) != 0) {
            const std::uint32_t one = 1u;
            callVirtual(env, load32(self, 0x20), 0, &one, 1);
        }
        store32(self, 0x20, 0);
    }
    const auto machine = machineOf(self);
    const auto loco = locoOf(unitOf(machine));
    if (loco == 0) {
        return machine;
    }
    asPtr(loco)[0x434] = self[0x25];
    return call1(env, 0x006E6360u, loco, 0u);
}

std::uint32_t unitAIExitGoalOnExit(std::uint8_t* self, std::uint32_t, const std::uint32_t*, int, AIState79BEnv& env)
{
    const auto machine = machineOf(self);
    const auto goal = call0(env, 0x005B9F30u, machine);
    if (goal != 0) {
        const auto contain = call0(env, 0x006A7600u, goal);
        if (contain != 0) {
            const auto unit = unitOf(machine);
            const std::uint32_t args[2] = {unit, 2u};
            callVirtual(env, contain, 0x40, args, 2);
        }
    }
    const std::uint32_t zero = 0u;
    auto result = callVirtual(env, machine, 0x38, &zero, 1);
    const auto child = load32(self, 0x20);
    if (child != 0) {
        const std::uint32_t one = 1u;
        result = callVirtual(env, child, 0, &one, 1);
    }
    store32(self, 0x20, 0);
    return result;
}

std::uint32_t saAIPackOnEnter(std::uint8_t* self, std::uint32_t, const std::uint32_t*, int, AIState79BEnv& env)
{
    const auto machine = machineOf(self);
    const auto found = call1(env, 0x007E2950u, env.registry, load32(asPtr(machine), 0x80));
    if (found == 0) {
        return kFail;
    }
    const auto module = call1(env, 0x006878B0u, unitOf(machine), found);
    if (module == 0) {
        return kFail;
    }
    if ((callVirtual(env, module, 0x64, nullptr, 0) & 0xFFu) != 0) {
        const std::uint32_t one = 1u;
        callVirtual(env, module, 0x68, &one, 1);
        return 0;
    }
    return kDone;
}

std::uint32_t saAITriggerOnEnter(std::uint8_t* self, std::uint32_t, const std::uint32_t*, int, AIState79BEnv& env)
{
    const auto machine = machineOf(self);
    const auto found = call1(env, 0x007E2950u, env.registry, load32(asPtr(machine), 0x80));
    if (found == 0) {
        return kDone;
    }
    const auto module = call1(env, 0x006878B0u, unitOf(machine), found);
    if (module == 0) {
        return kDone;
    }
    callVirtual(env, module, 0x6C, nullptr, 0);
    if ((call0(env, 0x00684BD0u, module) & 0xFFu) == 0) {
        return kDone;
    }
    call0(env, 0x00684C00u, module);
    return kFail;
}

std::uint32_t saAIUnpackOnEnter(std::uint8_t* self, std::uint32_t, const std::uint32_t*, int, AIState79BEnv& env)
{
    const auto machine = machineOf(self);
    const auto found = call1(env, 0x007E2950u, env.registry, load32(asPtr(machine), 0x80));
    if (found == 0) {
        return kFail;
    }
    const auto module = call1(env, 0x006878B0u, unitOf(machine), found);
    if (module == 0) {
        return kFail;
    }
    if ((call0(env, 0x0069FC10u, module) & 0xFFu) == 0) {
        return kDone;
    }
    callVirtual(env, module, 0x58, nullptr, 0);
    return 0;
}

}  // namespace

bool runAIState79B(std::uint32_t va, std::uint8_t* self, std::uint32_t eaxIn, const std::uint32_t* stack, int argc,
                   AIState79BEnv& env, std::uint32_t& eaxOut)
{
    switch (va) {
    case 0x00797B80u: eaxOut = unitAIAimForceFireAtObjectOnEnter(self, eaxIn, stack, argc, env); return true;
    case 0x00797C00u: eaxOut = unitAIAimForceFireAtObjectOnExit(self, eaxIn, stack, argc, env); return true;
    case 0x00797660u: eaxOut = saAIPackOnEnter(self, eaxIn, stack, argc, env); return true;
    case 0x00797760u: eaxOut = saAITriggerOnEnter(self, eaxIn, stack, argc, env); return true;
    case 0x007977C0u: eaxOut = saAIUnpackOnEnter(self, eaxIn, stack, argc, env); return true;
    case 0x00797FA0u: eaxOut = unitAIEndPathMoveOnEnter(self, eaxIn, stack, argc, env); return true;
    case 0x00798310u: eaxOut = unitAIHordeForceMembersToExitGoalOnEnter(self, eaxIn, stack, argc, env); return true;
    case 0x00798560u: eaxOut = unitAIResetEnterOnEnter(self, eaxIn, stack, argc, env); return true;
    case 0x00798F80u: eaxOut = aiGuardRetaliateMachineXfer(self, eaxIn, stack, argc, env); return true;
    case 0x00799A20u: eaxOut = aiMoveToStateSASlot19(self, eaxIn, stack, argc, env); return true;
    case 0x00799C30u: eaxOut = aiCowerOnExit(self, eaxIn, stack, argc, env); return true;
    case 0x0079A310u: eaxOut = aiWaitUntilFinishedFiringUpdate(self, eaxIn, stack, argc, env); return true;
    case 0x0079A520u: eaxOut = aiAttackSquadXfer(self, eaxIn, stack, argc, env); return true;
    case 0x0079A620u: eaxOut = aiDockOnExit(self, eaxIn, stack, argc, env); return true;
    case 0x0079A680u: eaxOut = aiDockUpdate(self, eaxIn, stack, argc, env); return true;
    case 0x0079A780u: eaxOut = unitAIHordeEnterGoalUpdate(self, eaxIn, stack, argc, env); return true;
    case 0x0079A800u: eaxOut = aiHordeExitOnEnter(self, eaxIn, stack, argc, env); return true;
    case 0x0079A870u: eaxOut = aiHordeExitUpdate(self, eaxIn, stack, argc, env); return true;
    case 0x0079A9B0u: eaxOut = aiHordeExitOnExit(self, eaxIn, stack, argc, env); return true;
    case 0x0079AAC0u: eaxOut = aiExitOnEnter(self, eaxIn, stack, argc, env); return true;
    case 0x0079AB10u: eaxOut = aiExitUpdate(self, eaxIn, stack, argc, env); return true;
    case 0x0079AC50u: eaxOut = aiGuardXfer(self, eaxIn, stack, argc, env); return true;
    case 0x0079AD70u: eaxOut = aiGuardUpdate(self, eaxIn, stack, argc, env); return true;
    case 0x0079ADF0u: eaxOut = aiGuardAreaXfer(self, eaxIn, stack, argc, env); return true;
    case 0x0079AF50u: eaxOut = aiGuardObjectXfer(self, eaxIn, stack, argc, env); return true;
    case 0x0079AFD0u: eaxOut = aiGuardObjectOnEnter(self, eaxIn, stack, argc, env); return true;
    case 0x0079B130u: eaxOut = aiAttackAreaXfer(self, eaxIn, stack, argc, env); return true;
    case 0x0079B720u: eaxOut = aiPickUpCrateOnEnter(self, eaxIn, stack, argc, env); return true;
    case 0x0079C820u: eaxOut = unitAIHeliEvacuateXfer(self, eaxIn, stack, argc, env); return true;
    case 0x0079CDA0u: eaxOut = unitAIReturnForLandingOnExit(self, eaxIn, stack, argc, env); return true;
    case 0x0079CFD0u: eaxOut = unitAIDeliverOreOnExit(self, eaxIn, stack, argc, env); return true;
    case 0x0079D2A0u: eaxOut = unitAIDockWithNodeOnExit(self, eaxIn, stack, argc, env); return true;
    case 0x0079D930u: eaxOut = saAIPrepareOnEnter(self, eaxIn, stack, argc, env); return true;
    case 0x0079D9B0u: eaxOut = saAIValidateApproachTargetOnEnter(self, eaxIn, stack, argc, env); return true;
    case 0x0079DEB0u: eaxOut = unitAIAttackObjectXfer(self, eaxIn, stack, argc, env); return true;
    case 0x0079E190u: eaxOut = unitAICombinedUnitOnEnter(self, eaxIn, stack, argc, env); return true;
    case 0x0079E960u: eaxOut = unitAIDoSpecialPowerOnEnter(self, eaxIn, stack, argc, env); return true;
    case 0x0079E9C0u: eaxOut = unitAIEndWaypointPathMoveOnEnter(self, eaxIn, stack, argc, env); return true;
    case 0x0079EBA0u: eaxOut = unitAIEnterGoalOnExit(self, eaxIn, stack, argc, env); return true;
    case 0x0079EC10u: eaxOut = unitAIExitGoalOnExit(self, eaxIn, stack, argc, env); return true;
    case 0x0079EEF0u: eaxOut = followWaypointPathMachineXfer(self, eaxIn, stack, argc, env); return true;
    case 0x0079F030u: eaxOut = unitAIFollowWaypointPathOnEnter(self, eaxIn, stack, argc, env); return true;
    case 0x0079F150u: eaxOut = unitAIFollowWaypointPathExactOnEnter(self, eaxIn, stack, argc, env); return true;
    case 0x0079F290u: eaxOut = unitAIForceAttackObjectOnExit(self, eaxIn, stack, argc, env); return true;
    case 0x0079F610u: eaxOut = unitAIHordeEnterGoalOnEnter(self, eaxIn, stack, argc, env); return true;
    case 0x0079F6F0u: eaxOut = unitAIHordeEnterGoalOnExit(self, eaxIn, stack, argc, env); return true;
    case 0x0079FC40u: eaxOut = unitAIMoveOnPathXfer(self, eaxIn, stack, argc, env); return true;
    case 0x0079FF10u: eaxOut = unitAIMoveToGoalOnExit(self, eaxIn, stack, argc, env); return true;
    default: return false;
    }
}

}  // namespace ra3::gamelogic
