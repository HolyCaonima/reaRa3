#include "ra3/gamelogic/AIState79C.h"

#include <cstring>

namespace ra3::gamelogic {
namespace {

constexpr std::uint32_t kDone = 0xFFFFFFFFu;
constexpr std::uint32_t kFail = 0xFFFFFFFEu;
constexpr std::uint32_t kRegistry = 0x00CC79E0u;
constexpr std::uint32_t kAimLimit = 0x43B40000u;
constexpr std::uint32_t kWallName = 0x00C2C4B4u;
constexpr std::uint32_t kGuardUpdateName = 0x00C2D08Cu;

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

std::uint32_t call0(AIState79Env& env, std::uint32_t target, std::uint32_t ecx)
{
    return callExternal(env, target, ecx, nullptr, 0);
}

std::uint32_t call1(AIState79Env& env, std::uint32_t target, std::uint32_t ecx, std::uint32_t arg)
{
    return callExternal(env, target, ecx, &arg, 1);
}

std::uint32_t callN(AIState79Env& env, std::uint32_t target, std::uint32_t ecx,
                    const std::uint32_t* args, int argc)
{
    return callExternal(env, target, ecx, args, argc);
}

std::uint32_t callVirtual(AIState79Env& env, std::uint32_t obj, std::uint32_t slot,
                          const std::uint32_t* args, int argc)
{
    const auto function = load32(asPtr(load32(asPtr(obj), 0)), slot);
    return callExternal(env, function, obj, args, argc);
}

void publish(AIState79Env& env, void* data, std::uint32_t bytes)
{
    env.blob = static_cast<std::uint8_t*>(data);
    env.blobBytes = bytes;
    env.captureBlob = false;
}

void clearBlob(AIState79Env& env)
{
    env.blob = nullptr;
    env.blobBytes = 0;
    env.captureBlob = false;
}

void copy12(std::uint8_t* destination, const std::uint8_t* source)
{
    store32(destination, 0, load32(source, 0));
    store32(destination, 4, load32(source, 4));
    store32(destination, 8, load32(source, 8));
}

std::uint32_t xferVersion(AIState79Env& env, std::uint32_t xfer, std::uint8_t second, std::uint8_t* live)
{
    std::uint8_t version[2] = {1, second};
    publish(env, version, 2);
    const auto pointer = addressOf(version);
    const auto result = callVirtual(env, xfer, 0x18, &pointer, 1);
    if (live != nullptr) {
        *live = version[1];
    }
    clearBlob(env);
    return result;
}

std::uint32_t packFlag(std::uint32_t value)
{
    const auto eax = (value & 0xFFu) != 0 ? 0xFFFFFFFFu : 0u;
    return static_cast<std::uint32_t>(-static_cast<std::int32_t>(eax)) - 1u;
}

std::uint32_t prepareFlag(std::uint32_t value)
{
    return (value & 0xFFu) != 0 ? kDone : 0u;
}

std::uint32_t withLowBit(std::uint32_t value)
{
    return (value & 0xFFFFFF00u) | 1u;
}

std::uint32_t destroyChild(AIState79Env& env, std::uint8_t* self, std::uint32_t result)
{
    const auto child = load32(self, 0x20);
    if (child != 0) {
        const std::uint32_t one = 1u;
        result = callVirtual(env, child, 0, &one, 1);
    }
    store32(self, 0x20, 0);
    return result;
}

std::uint32_t nestedUpdateArg(AIState79Env& env, std::uint8_t* self)
{
    const auto child = load32(self, 0x20);
    const auto argument = load32(asPtr(child), 0x68);
    return callVirtual(env, child, 0x1C, &argument, 1);
}

std::uint32_t aimExit(std::uint8_t* self, AIState79Env& env, std::uint32_t arm, std::uint32_t hold)
{
    const auto unit = unitOf(machineOf(self));
    const auto loco = locoOf(unit);
    if (self[arm] != 0 && loco == 0) {
        const std::uint32_t mood[2] = {0x1Bu, 0u};
        return callN(env, 0x006D2020u, unitOf(machineOf(self)), mood, 2);
    }
    if (self[arm] != 0 && self[hold] != 0) {
        callVirtual(env, loco, 0x270, nullptr, 0);
    }
    if (loco != 0) {
        const auto row = load32(asPtr(load32(asPtr(unitOf(machineOf(self))), 4)), 0x14);
        if (bitsToFloat(kAimLimit) > bitsToFloat(load32(asPtr(row), 0x120))) {
            call1(env, 0x007527C0u, loco, 0u);
        }
    }
    const std::uint32_t mood[2] = {0x1Bu, 0u};
    return callN(env, 0x006D2020u, unitOf(machineOf(self)), mood, 2);
}

std::uint32_t lookupModule(AIState79Env& env, std::uint32_t machine, std::uint32_t key, std::uint32_t finder)
{
    const auto found = call1(env, 0x007E2950u, kRegistry, key);
    if (found == 0) {
        return 0;
    }
    return call1(env, finder, unitOf(machine), found);
}

void copyGoalPosition(std::uint32_t destination, std::uint32_t source)
{
    copy12(asPtr(destination + 0x38u), asPtr(source + 0x38u));
}

std::uint32_t releaseLinked(AIState79Env& env, std::uint8_t* self, std::uint32_t field, std::uint32_t reason)
{
    const auto child = load32(self, field);
    if (child == 0) {
        return 0;
    }
    auto result = callVirtual(env, child, 0x18, &reason, 1);
    const auto again = load32(self, field);
    if (again != 0) {
        const std::uint32_t one = 1u;
        result = callVirtual(env, again, 0, &one, 1);
    }
    store32(self, field, 0);
    return result;
}

}  // namespace

std::uint32_t unitAIAttackObjectOnEnter(std::uint8_t* self, std::uint32_t, const std::uint32_t*, int,
                                        AIState79Env& env)
{
    const auto machine = machineOf(self);
    const auto unit = unitOf(machine);
    const auto goal = call0(env, 0x005B9F30u, machine);
    const auto child = callVirtual(env, addressOf(self), 0x4C, nullptr, 0);
    store32(self, 0x20, child);
    if (load32(self, 0x24) == 1u) {
        callVirtual(env, child, 0x38, &goal, 1);
    }
    const auto live = load32(self, 0x20);
    const std::uint32_t placed[2] = {goal + 0x38u, env.maxFloatBits};
    callN(env, 0x005AC4F0u, live, placed, 2);
    call1(env, 0x006D21F0u, unit, goal);
    const auto marker = load32(asPtr(unit), 0xDC);
    callVirtual(env, load32(asPtr(goal), 0x33C), 0x28, &marker, 1);
    store32(self, 0x28, call1(env, 0x006A5370u, unit, goal));
    return nestedUpdateArg(env, self);
}

std::uint32_t aiGuardAreaOnEnter(std::uint8_t* self, std::uint32_t, const std::uint32_t*, int, AIState79Env& env)
{
    const auto machine = machineOf(self);
    const auto loco = locoOf(unitOf(machine));
    const std::uint32_t three = 3u;
    const auto child = callVirtual(env, machine, 0x24, &three, 1);
    store32(self, 0x20, child);
    if (callVirtual(env, loco, 0x234, nullptr, 0) != 0) {
        const auto where = callVirtual(env, loco, 0x224, nullptr, 0);
        const std::uint32_t placed[2] = {where, env.maxFloatBits};
        callN(env, 0x005AC4F0u, load32(self, 0x20), placed, 2);
    }
    const auto live = load32(self, 0x20);
    if (callVirtual(env, live, 0x14, nullptr, 0) == kFail) {
        return kFail;
    }
    const auto argument = load32(asPtr(load32(self, 0x20)), 0x68);
    return callVirtual(env, load32(self, 0x20), 0x1C, &argument, 1);
}

std::uint32_t aiAttackAimAtTargetOnExit(std::uint8_t* self, std::uint32_t, const std::uint32_t*, int,
                                        AIState79Env& env)
{
    return aimExit(self, env, 0x21, 0x22);
}

std::uint32_t aiAttackPositionAimAtTargetOnExit(std::uint8_t* self, std::uint32_t, const std::uint32_t*, int,
                                                AIState79Env& env)
{
    return aimExit(self, env, 0x20, 0x21);
}

std::uint32_t unitAIWaitForDeliveryOnEnter(std::uint8_t* self, std::uint32_t, const std::uint32_t*, int,
                                           AIState79Env& env)
{
    const auto machine = machineOf(self);
    const auto goal = call0(env, 0x005B9F30u, machine);
    const auto unit = unitOf(machine);
    if ((call1(env, 0x0068E1F0u, load32(asPtr(goal), 0x394), unit) & 0xFFu) == 0) {
        return kFail;
    }
    const auto miner = load32(asPtr(goal), 0x398);
    const std::uint32_t wake[2] = {0u, 1u};
    callN(env, 0x0067E3D0u, miner, wake, 2);
    const auto node = load32(asPtr(unitOf(machineOf(self))), 0x39C);
    store32(asPtr(node), 0x34, load32(asPtr(goal), 0xDC));
    const auto bits = load32(asPtr(load32(asPtr(miner), 4)), 0x14);
    call1(env, 0x0070B000u, node, bits);
    return 0;
}

std::uint32_t unitAIAttackObjectOnExit(std::uint8_t* self, std::uint32_t, const std::uint32_t*, int,
                                       AIState79Env& env)
{
    const auto unit = unitOf(machineOf(self));
    call0(env, 0x006F0440u, unit);
    call1(env, 0x006C0F40u, unit, 0u);
    auto result = call1(env, 0x0067D870u, unit, unit);
    if (result != 0) {
        const std::uint32_t zero = 0u;
        result = callVirtual(env, result, 4, &zero, 1);
    }
    return destroyChild(env, self, result);
}

std::uint32_t aiAttackMeleeHordeWaitPathXfer(std::uint8_t* self, std::uint32_t, const std::uint32_t* stack, int,
                                             AIState79Env& env)
{
    const auto xfer = stack[0];
    auto result = xferVersion(env, xfer, 1, nullptr);
    if (((load32(asPtr(xfer), 4) >> 3) & 1u) == 0) {
        const auto first = addressOf(self) + 0x20u;
        result = callVirtual(env, xfer, 0x6C, &first, 1);
        const auto second = addressOf(self) + 0x24u;
        result = callVirtual(env, xfer, 0x70, &second, 1);
    }
    return result;
}

std::uint32_t unitAISuperXfer(std::uint8_t* self, std::uint32_t, const std::uint32_t* stack, int, AIState79Env& env)
{
    const auto xfer = stack[0];
    xferVersion(env, xfer, 1, nullptr);
    if (((load32(asPtr(xfer), 4) >> 1) & 1u) != 0) {
        store32(self, 0x20, callVirtual(env, addressOf(self), 0x4C, nullptr, 0));
    }
    const auto id = load32(self, 0x20);
    return callVirtual(env, xfer, 0x20, &id, 1);
}

std::uint32_t unitAISetGoalPositionToTargetOnEnter(std::uint8_t* self, std::uint32_t, const std::uint32_t*, int,
                                                   AIState79Env& env)
{
    const auto goal = call0(env, 0x005B9F30u, machineOf(self));
    if (goal == 0) {
        return kFail;
    }
    const auto machine = machineOf(self);
    const std::uint32_t placed[2] = {goal + 0x38u, load32(asPtr(machine), 0x54)};
    callN(env, 0x005AC4F0u, machine, placed, 2);
    copyGoalPosition(machineOf(self), goal);
    return kDone;
}

std::uint32_t unitAIForceAttackObjectOnEnter(std::uint8_t* self, std::uint32_t eax, const std::uint32_t* stack, int argc,
                                             AIState79Env& env)
{
    const auto machine = machineOf(self);
    const auto unit = unitOf(machine);
    const std::uint32_t mood[2] = {0x8Eu, 1u};
    callN(env, 0x006D2020u, unit, mood, 2);
    if (callVirtual(env, locoOf(unit), 0x288, nullptr, 0) != 2u) {
        const auto goal = call0(env, 0x005B9F30u, machineOf(self));
        call1(env, 0x006C0F40u, unit, goal);
    }
    return unitAIAttackObjectOnEnter(self, eax, stack, argc, env);
}

std::uint32_t aiHarvestOnExit(std::uint8_t* self, std::uint32_t, const std::uint32_t*, int, AIState79Env& env)
{
    const auto child = load32(self, 0x20);
    if (child != 0) {
        callVirtual(env, child, 0x30, nullptr, 0);
        if (load32(self, 0x20) != 0) {
            const std::uint32_t one = 1u;
            callVirtual(env, load32(self, 0x20), 0, &one, 1);
        }
        store32(self, 0x20, 0);
    }
    const auto loco = locoOf(unitOf(machineOf(self)));
    if (loco != 0) {
        asPtr(loco)[0x434] = self[0x25];
    }
    return loco;
}

std::uint32_t aiAttackSquadOnExit(std::uint8_t* self, std::uint32_t, const std::uint32_t*, int, AIState79Env& env)
{
    const auto machine = machineOf(self);
    const auto unit = unitOf(machine);
    auto result = machine;
    if (locoOf(unit) != 0) {
        result = call1(env, 0x006C0F40u, unit, 0u);
    }
    store32(asPtr(unitOf(machineOf(self))), 0x490, 0);
    return destroyChild(env, self, result);
}

std::uint32_t saAIPackUpdate(std::uint8_t* self, std::uint32_t, const std::uint32_t*, int, AIState79Env& env)
{
    const auto machine = machineOf(self);
    const auto module = lookupModule(env, machine, load32(asPtr(machine), 0x80), 0x006878B0u);
    if (module == 0) {
        return kFail;
    }
    return packFlag(callVirtual(env, module, 0x5C, nullptr, 0));
}

std::uint32_t aiGuardRetaliateUpdate(std::uint8_t* self, std::uint32_t, const std::uint32_t*, int, AIState79Env& env)
{
    const auto child = load32(self, 0x20);
    if (child == 0) {
        return kFail;
    }
    const auto goal = call0(env, 0x005B9F30u, machineOf(self));
    if (goal != 0 && goal != call0(env, 0x005B9F30u, child)) {
        callVirtual(env, load32(self, 0x20), 0x38, &goal, 1);
    }
    return callVirtual(env, load32(self, 0x20), 0x10, nullptr, 0);
}

std::uint32_t aiHarvestPrepareSiteOnEnter(std::uint8_t* self, std::uint32_t, const std::uint32_t*, int,
                                          AIState79Env& env)
{
    const auto loco = locoOf(unitOf(machineOf(self)));
    const auto found = callVirtual(env, loco, 0x1C4, nullptr, 0);
    if (found == 0) {
        return kFail;
    }
    const auto frame = load32(asPtr(env.gameLogic), 0x58);
    store32(self, 0x20, callVirtual(env, found, 0x44, nullptr, 0) + frame);
    return 0;
}

std::uint32_t unitAIHordeExitGoalOnEnter(std::uint8_t* self, std::uint32_t, const std::uint32_t*, int,
                                         AIState79Env& env)
{
    const auto goal = call0(env, 0x005B9F30u, machineOf(self));
    if (goal == 0) {
        return kFail;
    }
    const auto child = callVirtual(env, addressOf(self), 0x4C, nullptr, 0);
    store32(self, 0x20, child);
    callVirtual(env, child, 0x38, &goal, 1);
    return nestedUpdateArg(env, self);
}

std::uint32_t unitAIAIEvacuateUpdate(std::uint8_t* self, std::uint32_t, const std::uint32_t*, int, AIState79Env& env)
{
    const auto unit = unitOf(machineOf(self));
    const auto contain = call0(env, 0x006A7600u, unit);
    if (contain == 0) {
        return kFail;
    }
    const auto mode = callVirtual(env, locoOf(unit), 0x288, nullptr, 0);
    callVirtual(env, contain, 0x88, &mode, 1);
    return kDone;
}

std::uint32_t unitAIKillMinerOnEnter(std::uint8_t* self, std::uint32_t, const std::uint32_t*, int, AIState79Env& env)
{
    const auto machine = machineOf(self);
    const auto unit = unitOf(machine);
    const auto goal = call0(env, 0x005B9F30u, machine);
    if (goal != 0) {
        const auto node = load32(asPtr(goal), 0x33C);
        if (node != 0) {
            const auto info = callVirtual(env, node, 0x64, nullptr, 0);
            if (info != 0) {
                const std::uint32_t args[3] = {
                    load32(asPtr(info), 0x1C), load32(asPtr(info), 0x34), load32(asPtr(info), 8)};
                callN(env, 0x007483A0u, unit, args, 3);
            }
        }
    }
    return kDone;
}

std::uint32_t unitAIDoSpecialPowerUpdate(std::uint8_t* self, std::uint32_t, const std::uint32_t*, int,
                                         AIState79Env& env)
{
    const auto module = lookupModule(env, machineOf(self), load32(self, 0x20), 0x00687720u);
    if (module == 0) {
        return 0;
    }
    if ((callVirtual(env, module, 4, nullptr, 0) & 0xFFu) != 0) {
        return 0;
    }
    return kFail;
}

std::uint32_t aiAttackMeleeHordeWaitXfer(std::uint8_t* self, std::uint32_t, const std::uint32_t* stack, int,
                                         AIState79Env& env)
{
    const auto xfer = stack[0];
    auto result = xferVersion(env, xfer, 1, nullptr);
    if (((load32(asPtr(xfer), 4) >> 3) & 1u) == 0) {
        const auto field = addressOf(self) + 0x20u;
        result = callVirtual(env, xfer, 0x6C, &field, 1);
    }
    return result;
}

std::uint32_t saAIPrepareUpdate(std::uint8_t* self, std::uint32_t, const std::uint32_t*, int, AIState79Env& env)
{
    const auto machine = machineOf(self);
    const auto module = lookupModule(env, machine, load32(asPtr(machine), 0x80), 0x006878B0u);
    if (module == 0) {
        return kFail;
    }
    return prepareFlag(callVirtual(env, module, 0x70, nullptr, 0));
}

std::uint32_t aiRotateFiringarcOnExit(std::uint8_t* self, std::uint32_t, const std::uint32_t*, int, AIState79Env& env)
{
    const auto machine = machineOf(self);
    const auto unit = unitOf(machine);
    const auto loco = locoOf(unit);
    if (loco == 0) {
        return machine;
    }
    call1(env, 0x007527C0u, loco, 0u);
    callVirtual(env, loco, 0x270, nullptr, 0);
    store32(asPtr(unit), 0x264, load32(asPtr(unit), 0x44));
    return unit;
}

std::uint32_t unitAIHordeForceMembersToExitGoalOnExit(std::uint8_t* self, std::uint32_t, const std::uint32_t*, int,
                                                      AIState79Env& env)
{
    const auto goal = call0(env, 0x005B9F30u, machineOf(self));
    if (goal == 0 || call0(env, 0x006A7600u, goal) == 0) {
        return 0;
    }
    const auto contain = call0(env, 0x006A7600u, goal);
    if (contain == 0) {
        return 0;
    }
    const auto unit = unitOf(machineOf(self));
    const std::uint32_t args[2] = {unit, 2u};
    return callVirtual(env, contain, 0x40, args, 2);
}

std::uint32_t unitAICombinedUnitOnExit(std::uint8_t* self, std::uint32_t, const std::uint32_t*, int, AIState79Env& env)
{
    const auto loco = locoOf(unitOf(machineOf(self)));
    std::uint32_t result = 0;
    if (loco != 0) {
        const auto link = load32(asPtr(loco), 0x214);
        if (link != 0) {
            store32(asPtr(link), 0x40, load32(asPtr(link), 0x40) & ~8u);
            result = link;
        }
    }
    return destroyChild(env, self, result);
}

std::uint32_t aiMoveOntoWallUpdate(std::uint8_t* self, std::uint32_t, const std::uint32_t*, int, AIState79Env& env)
{
    if (load32(self, 0x20) == 0) {
        return 0;
    }
    const auto machine = machineOf(self);
    call1(env, 0x005AA240u, machine, kWallName);
    auto value = static_cast<std::int32_t>(callVirtual(env, load32(self, 0x20), 0x10, nullptr, 0));
    if (value > 0) {
        value = 0;
    }
    call0(env, 0x005AA250u, machineOf(self));
    return static_cast<std::uint32_t>(value);
}

std::uint32_t aiPickUpCrateOnEnter(std::uint8_t* self, std::uint32_t, const std::uint32_t*, int, AIState79Env& env)
{
    const auto goal = call0(env, 0x005B9F30u, machineOf(self));
    if (goal == 0) {
        return kFail;
    }
    self[0x4C] = 1;
    copy12(self + 0x20, asPtr(goal + 0x38u));
    store32(self, 0x50, 3u);
    return 0;
}

std::uint32_t unitAIFollowWaypointPathOnExit(std::uint8_t* self, std::uint32_t, const std::uint32_t*, int,
                                             AIState79Env& env)
{
    const auto machine = machineOf(self);
    const auto loco = locoOf(unitOf(machine));
    auto result = machine;
    if (loco != 0) {
        result = call1(env, 0x00683590u, loco, 0u);
    }
    return destroyChild(env, self, result);
}

std::uint32_t aiGuardAreaUpdate(std::uint8_t* self, std::uint32_t, const std::uint32_t*, int, AIState79Env& env)
{
    if (load32(self, 0x20) == 0) {
        return kFail;
    }
    const auto machine = machineOf(self);
    call1(env, 0x005AA240u, machine, kGuardUpdateName);
    const auto value = callVirtual(env, load32(self, 0x20), 0x10, nullptr, 0);
    call0(env, 0x005AA250u, machineOf(self));
    return value;
}

std::uint32_t aiGuardRetaliateOnExit(std::uint8_t* self, std::uint32_t, const std::uint32_t*, int, AIState79Env& env)
{
    const auto child = load32(self, 0x20);
    if (child != 0) {
        const std::uint32_t one = 1u;
        callVirtual(env, child, 0, &one, 1);
    }
    const auto machine = machineOf(self);
    store32(self, 0x20, 0);
    const auto unit = unitOf(machine);
    const auto loco = locoOf(unit);
    if (loco == 0) {
        return unit;
    }
    return callVirtual(env, loco, 0x240, nullptr, 0);
}

std::uint32_t unitAIAimForceFireAtObjectXfer(std::uint8_t* self, std::uint32_t, const std::uint32_t* stack, int,
                                             AIState79Env& env)
{
    const auto xfer = stack[0];
    std::uint8_t second = 0;
    auto result = xferVersion(env, xfer, 2, &second);
    if (second >= 2) {
        result = call1(env, 0x0079DAC0u, addressOf(self), xfer);
    }
    return result;
}

std::uint32_t saAIFinishOnEnter(std::uint8_t* self, std::uint32_t, const std::uint32_t*, int, AIState79Env& env)
{
    const auto machine = machineOf(self);
    const auto module = lookupModule(env, machine, load32(asPtr(machine), 0x80), 0x006878B0u);
    if (module != 0) {
        callVirtual(env, module, 0x74, nullptr, 0);
    }
    return kDone;
}

std::uint32_t aiAttackFireWeaponSlot17(std::uint8_t* self, std::uint32_t, const std::uint32_t*, int, AIState79Env& env)
{
    const auto weapon = call1(env, 0x006A51A0u, unitOf(machineOf(self)), kDone);
    if (weapon == 0) {
        return 0;
    }
    const auto owner = load32(asPtr(weapon), 4);
    const auto ready = call0(env, 0x00685EC0u, owner);
    if ((ready & 0xFFu) != 0) {
        return withLowBit(ready);
    }
    const auto frame = call0(env, 0x00686460u, owner);
    if (static_cast<std::int32_t>(frame) < 0) {
        return frame & 0xFFFFFF00u;
    }
    return withLowBit(frame);
}

std::uint32_t aiGuardOnExit(std::uint8_t* self, std::uint32_t, const std::uint32_t*, int, AIState79Env& env)
{
    const auto loco = locoOf(unitOf(machineOf(self)));
    if (loco != 0) {
        asPtr(loco)[0x444] = 0;
    }
    return destroyChild(env, self, loco);
}

std::uint32_t saAIPrepareOnExit(std::uint8_t* self, std::uint32_t, const std::uint32_t*, int, AIState79Env& env)
{
    const auto machine = machineOf(self);
    const auto module = lookupModule(env, machine, load32(asPtr(machine), 0x80), 0x006878B0u);
    if (module == 0) {
        return 0;
    }
    return call0(env, 0x006EB6C0u, module);
}

std::uint32_t aiBackAwayOnEnter(std::uint8_t* self, std::uint32_t, const std::uint32_t*, int, AIState79Env& env)
{
    self[0x4C] = 0;
    const auto goal = call0(env, 0x005B9F30u, machineOf(self));
    const auto loco = locoOf(unitOf(machineOf(self)));
    if (goal != 0 && loco != 0) {
        call1(env, 0x007527C0u, loco, 9u);
    }
    return kFail;
}

std::uint32_t aiTNGuardInnerOnExit(std::uint8_t* self, std::uint32_t eaxIn, const std::uint32_t* stack, int,
                                   AIState79Env& env)
{
    if (load32(self, 0x2C) == 0) {
        return eaxIn;
    }
    return releaseLinked(env, self, 0x2C, stack[0]);
}

std::uint32_t aiTNGuardOuterOnExit(std::uint8_t* self, std::uint32_t eaxIn, const std::uint32_t* stack, int,
                                   AIState79Env& env)
{
    if (load32(self, 0x28) == 0) {
        return eaxIn;
    }
    return releaseLinked(env, self, 0x28, stack[0]);
}

std::uint32_t unitAIAttackObjectUpdate(std::uint8_t* self, std::uint32_t, const std::uint32_t*, int, AIState79Env& env)
{
    const auto machine = machineOf(self);
    const auto goal = call0(env, 0x005B9F30u, machine);
    if (goal != 0) {
        const auto id = call1(env, 0x006A5370u, unitOf(machine), goal);
        if (id != load32(self, 0x28)) {
            return kDone;
        }
    }
    return callVirtual(env, load32(self, 0x20), 0x10, nullptr, 0);
}

std::uint32_t unitAIHeliPrepareForBoardingOnExit(std::uint8_t* self, std::uint32_t, const std::uint32_t*, int,
                                                 AIState79Env& env)
{
    const std::uint32_t mood[2] = {0x29u, 0u};
    const auto result = callN(env, 0x006D2020u, unitOf(machineOf(self)), mood, 2);
    return destroyChild(env, self, result);
}

std::uint32_t aiBackAwayAndCowerUpdate(std::uint8_t* self, std::uint32_t, const std::uint32_t*, int, AIState79Env& env)
{
    const auto child = load32(self, 0x20);
    if (child == 0) {
        return kFail;
    }
    const auto link = load32(asPtr(child), 4);
    if (link != 0 && load32(asPtr(link), 4) == 0x000F423Eu) {
        return kDone;
    }
    return callVirtual(env, load32(self, 0x20), 0x10, nullptr, 0);
}

std::uint32_t aiMoveToGoalDiveBombOnExit(std::uint8_t* self, std::uint32_t, const std::uint32_t*, int,
                                         AIState79Env& env)
{
    const auto result = call1(env, 0x007576C0u, unitOf(machineOf(self)), 0u);
    return destroyChild(env, self, result);
}

std::uint32_t aiStateMachineSlot10(std::uint8_t* self, std::uint32_t, const std::uint32_t*, int, AIState79Env& env)
{
    if (load32(self, 0x88) != 0) {
        const auto flag = callVirtual(env, load32(self, 0x88), 0x28, nullptr, 0);
        if ((flag & 0xFFu) != 0) {
            return withLowBit(flag);
        }
    }
    return call0(env, 0x005AA1A0u, addressOf(self));
}

std::uint32_t unitAIParachuteToGroundSlot19(std::uint8_t* self, std::uint32_t, const std::uint32_t*, int,
                                            AIState79Env& env)
{
    const auto made = call1(env, 0x00416820u, addressOf(self), 0x80u);
    if (made == 0) {
        return 0;
    }
    return call1(env, 0x00798400u, made, unitOf(machineOf(self)));
}

std::uint32_t unitAIDeadUpdate(std::uint8_t* self, std::uint32_t, const std::uint32_t*, int, AIState79Env& env)
{
    const auto unit = unitOf(machineOf(self));
    const auto loco = locoOf(unit);
    if (loco != 0) {
        callVirtual(env, loco, 0x270, nullptr, 0);
    }
    call1(env, 0x006D23F0u, unit, 1u);
    return 0;
}

std::uint32_t unitAIAttackSquadOnEnter(std::uint8_t* self, std::uint32_t, const std::uint32_t*, int, AIState79Env& env)
{
    const auto child = callVirtual(env, addressOf(self), 0x4C, nullptr, 0);
    store32(self, 0x20, child);
    store32(asPtr(child), 0x80, callVirtual(env, machineOf(self), 0x44, nullptr, 0));
    return nestedUpdateArg(env, self);
}

std::uint32_t unitAIMoveToGoalCopyOnEnter(std::uint8_t* self, std::uint32_t, const std::uint32_t*, int,
                                          AIState79Env& env)
{
    const auto child = callVirtual(env, addressOf(self), 0x4C, nullptr, 0);
    store32(self, 0x20, child);
    call1(env, 0x005B2450u, child, machineOf(self));
    return nestedUpdateArg(env, self);
}

std::uint32_t unitAIMoveToGoalOnEnter(std::uint8_t* self, std::uint32_t, const std::uint32_t*, int, AIState79Env& env)
{
    const auto machine = machineOf(self);
    const auto goal = call0(env, 0x005B9F30u, machine);
    const auto unit = unitOf(machine);
    if (goal == 0) {
        return kFail;
    }
    const auto child = callVirtual(env, addressOf(self), 0x4C, nullptr, 0);
    store32(self, 0x20, child);
    const auto goalFlags = load32(asPtr(load32(asPtr(goal), 4)), 0xE0);
    bool special = false;
    if (((goalFlags >> 1) & 1u) != 0) {
        const auto unitTemplate = load32(asPtr(unit), 4);
        const auto unitFlags = load32(asPtr(unitTemplate), 0xC4);
        if (((unitFlags >> 15) & 1u) != 0 && ((unitFlags >> 0x19) & 1u) != 0) {
            special = ((load32(asPtr(unitTemplate), 0xDC) >> 0x1C) & 1u) == 0;
        }
    }
    if (special) {
        const std::uint32_t zero = 0u;
        callVirtual(env, child, 0x38, &zero, 1);
        const auto marker = load32(asPtr(goal), 0xDC);
        call1(env, 0x00683140u, locoOf(unit), marker);
        std::uint8_t local[12] = {};
        publish(env, local, sizeof(local));
        const auto temporary = addressOf(local);
        const auto first = call1(env, 0x006A76A0u, goal, temporary);
        const std::uint32_t placed[2] = {first, env.maxFloatBits};
        callN(env, 0x005AC4F0u, load32(self, 0x20), placed, 2);
        const auto second = call1(env, 0x006A76A0u, goal, temporary);
        clearBlob(env);
        copy12(asPtr(load32(self, 0x20) + 0x38u), asPtr(second));
    } else {
        callVirtual(env, child, 0x38, &goal, 1);
        const std::uint32_t placed[2] = {goal + 0x38u, env.maxFloatBits};
        callN(env, 0x005AC4F0u, load32(self, 0x20), placed, 2);
        copyGoalPosition(load32(self, 0x20), goal);
    }
    if ((asPtr(load32(asPtr(unit), 4))[0xE2] & 1u) != 0) {
        asPtr(load32(self, 0x20))[0x7C] = 0;
    }
    const auto weapon = call1(env, 0x006A51C0u, unit, kDone);
    if (weapon != 0 && (call1(env, 0x006A88C0u, weapon, 6u) & 0xFFu) != 0) {
        call1(env, 0x00683140u, locoOf(unit), load32(asPtr(goal), 0xDC));
    }
    return nestedUpdateArg(env, self);
}

namespace {

std::uint32_t dispatch(std::uint32_t va, std::uint8_t* self, std::uint32_t eax, const std::uint32_t* stack, int argc,
                       AIState79Env& env)
{
    switch (va) {
    case 0x0079DDB0u: return unitAIAttackObjectOnEnter(self, eax, stack, argc, env);
    case 0x0079AE70u: return aiGuardAreaOnEnter(self, eax, stack, argc, env);
    case 0x00799FE0u: return aiAttackAimAtTargetOnExit(self, eax, stack, argc, env);
    case 0x0079A140u: return aiAttackPositionAimAtTargetOnExit(self, eax, stack, argc, env);
    case 0x007973D0u: return unitAIWaitForDeliveryOnEnter(self, eax, stack, argc, env);
    case 0x0079DE60u: return unitAIAttackObjectOnExit(self, eax, stack, argc, env);
    case 0x00799E20u: return aiAttackMeleeHordeWaitPathXfer(self, eax, stack, argc, env);
    case 0x00798890u: return unitAISuperXfer(self, eax, stack, argc, env);
    case 0x00798760u: return unitAISetGoalPositionToTargetOnEnter(self, eax, stack, argc, env);
    case 0x0079F240u: return unitAIForceAttackObjectOnEnter(self, eax, stack, argc, env);
    case 0x0079A700u: return aiHarvestOnExit(self, eax, stack, argc, env);
    case 0x0079A5C0u: return aiAttackSquadOnExit(self, eax, stack, argc, env);
    case 0x00797810u: return saAIPackUpdate(self, eax, stack, argc, env);
    case 0x0079B0C0u: return aiGuardRetaliateUpdate(self, eax, stack, argc, env);
    case 0x007990A0u: return aiHarvestPrepareSiteOnEnter(self, eax, stack, argc, env);
    case 0x0079F880u: return unitAIHordeExitGoalOnEnter(self, eax, stack, argc, env);
    case 0x00798060u: return unitAIAIEvacuateUpdate(self, eax, stack, argc, env);
    case 0x00797210u: return unitAIKillMinerOnEnter(self, eax, stack, argc, env);
    case 0x00797F30u: return unitAIDoSpecialPowerUpdate(self, eax, stack, argc, env);
    case 0x00799D90u: return aiAttackMeleeHordeWaitXfer(self, eax, stack, argc, env);
    case 0x007976D0u: return saAIPrepareUpdate(self, eax, stack, argc, env);
    case 0x0079B360u: return aiRotateFiringarcOnExit(self, eax, stack, argc, env);
    case 0x00798370u: return unitAIHordeForceMembersToExitGoalOnExit(self, eax, stack, argc, env);
    case 0x0079E210u: return unitAICombinedUnitOnExit(self, eax, stack, argc, env);
    case 0x00799530u: return aiMoveOntoWallUpdate(self, eax, stack, argc, env);
    case 0x00799E70u: return aiPickUpCrateOnEnter(self, eax, stack, argc, env);
    case 0x0079F0C0u: return unitAIFollowWaypointPathOnExit(self, eax, stack, argc, env);
    case 0x0079AEE0u: return aiGuardAreaUpdate(self, eax, stack, argc, env);
    case 0x0079B080u: return aiGuardRetaliateOnExit(self, eax, stack, argc, env);
    case 0x0079DD50u: return unitAIAimForceFireAtObjectXfer(self, eax, stack, argc, env);
    case 0x00797610u: return saAIFinishOnEnter(self, eax, stack, argc, env);
    case 0x0079A210u: return aiAttackFireWeaponSlot17(self, eax, stack, argc, env);
    case 0x0079AD30u: return aiGuardOnExit(self, eax, stack, argc, env);
    case 0x00797710u: return saAIPrepareOnExit(self, eax, stack, argc, env);
    case 0x00799CF0u: return aiBackAwayOnEnter(self, eax, stack, argc, env);
    case 0x0079B5D0u: return aiTNGuardInnerOnExit(self, eax, stack, argc, env);
    case 0x0079B610u: return aiTNGuardOuterOnExit(self, eax, stack, argc, env);
    case 0x0079DE30u: return unitAIAttackObjectUpdate(self, eax, stack, argc, env);
    case 0x0079C8A0u: return unitAIHeliPrepareForBoardingOnExit(self, eax, stack, argc, env);
    case 0x00799AB0u: return aiBackAwayAndCowerUpdate(self, eax, stack, argc, env);
    case 0x0079C9F0u: return aiMoveToGoalDiveBombOnExit(self, eax, stack, argc, env);
    case 0x00799780u: return aiStateMachineSlot10(self, eax, stack, argc, env);
    case 0x007984F0u: return unitAIParachuteToGroundSlot19(self, eax, stack, argc, env);
    case 0x007998F0u: return unitAIDeadUpdate(self, eax, stack, argc, env);
    case 0x0079E080u: return unitAIAttackSquadOnEnter(self, eax, stack, argc, env);
    case 0x0079FEE0u: return unitAIMoveToGoalCopyOnEnter(self, eax, stack, argc, env);
    case 0x0079FD20u: return unitAIMoveToGoalOnEnter(self, eax, stack, argc, env);
    default: return 0;
    }
}

}  // namespace

bool runAIState79C(std::uint32_t va, std::uint8_t* self, std::uint32_t eaxIn, const std::uint32_t* stack, int argc,
                   AIState79Env& env, std::uint32_t& eaxOut)
{
    switch (va) {
    case 0x0079DDB0u: case 0x0079AE70u: case 0x00799FE0u: case 0x0079A140u: case 0x007973D0u:
    case 0x0079DE60u: case 0x00799E20u: case 0x00798890u: case 0x00798760u: case 0x0079F240u:
    case 0x0079A700u: case 0x0079A5C0u: case 0x00797810u: case 0x0079B0C0u: case 0x007990A0u:
    case 0x0079F880u: case 0x00798060u: case 0x00797210u: case 0x00797F30u: case 0x00799D90u:
    case 0x007976D0u: case 0x0079B360u: case 0x00798370u: case 0x0079E210u: case 0x00799530u:
    case 0x00799E70u: case 0x0079F0C0u: case 0x0079AEE0u: case 0x0079B080u: case 0x0079DD50u:
    case 0x00797610u: case 0x0079A210u: case 0x0079AD30u: case 0x00797710u: case 0x00799CF0u:
    case 0x0079B5D0u: case 0x0079B610u: case 0x0079DE30u: case 0x0079C8A0u: case 0x00799AB0u:
    case 0x0079C9F0u: case 0x00799780u: case 0x007984F0u: case 0x007998F0u: case 0x0079E080u:
    case 0x0079FEE0u: case 0x0079FD20u:
        eaxOut = dispatch(va, self, eaxIn, stack, argc, env);
        return true;
    default:
        return false;
    }
}

}  // namespace ra3::gamelogic
