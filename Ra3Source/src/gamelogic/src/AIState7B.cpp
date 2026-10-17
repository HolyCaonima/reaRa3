#include "ra3/gamelogic/AIState7B.h"

#include <cstring>

namespace ra3::gamelogic {
namespace {

constexpr std::uint32_t kDone = 0xFFFFFFFFu;
constexpr std::uint32_t kFail = 0xFFFFFFFEu;
constexpr std::uint32_t kContinue = 0x007B12D0u;
constexpr std::uint32_t kBaseExit = 0x007B1190u;
constexpr std::uint32_t kFlagPair = 0x006A4F80u;
constexpr std::uint32_t kFlagRefresh = 0x006A4FB0u;
constexpr std::uint32_t kStatus = 0x006D2020u;
constexpr std::uint32_t kLocoMode = 0x007527C0u;
constexpr std::uint32_t kSpray = 0x006B7040u;
constexpr std::uint32_t kGoal = 0x005B9F30u;
constexpr std::uint32_t kOperatorNew = 0x00416820u;

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

std::uint32_t frameOf(const AIState7BEnv& env)
{
    return load32(asPtr(env.gameLogic), 0x58);
}

std::uint32_t call0(AIState7BEnv& env, std::uint32_t target, std::uint32_t ecx)
{
    return callExternal(env, target, ecx, nullptr, 0);
}

std::uint32_t call1(AIState7BEnv& env, std::uint32_t target, std::uint32_t ecx, std::uint32_t arg)
{
    return callExternal(env, target, ecx, &arg, 1);
}

std::uint32_t call2(AIState7BEnv& env, std::uint32_t target, std::uint32_t ecx, std::uint32_t arg0,
                    std::uint32_t arg1)
{
    const std::uint32_t args[2] = {arg0, arg1};
    return callExternal(env, target, ecx, args, 2);
}

std::uint32_t vcall(AIState7BEnv& env, std::uint32_t obj, std::uint32_t slot, const std::uint32_t* args,
                    int argc)
{
    const auto fn = load32(asPtr(load32(asPtr(obj), 0)), slot);
    return callExternal(env, fn, obj, args, argc);
}

std::uint32_t vcall0(AIState7BEnv& env, std::uint32_t obj, std::uint32_t slot)
{
    return vcall(env, obj, slot, nullptr, 0);
}

std::uint32_t vcall1(AIState7BEnv& env, std::uint32_t obj, std::uint32_t slot, std::uint32_t arg)
{
    return vcall(env, obj, slot, &arg, 1);
}

bool low(std::uint32_t value)
{
    return (value & 0xFFu) != 0;
}

bool bit(std::uint32_t value, unsigned shift)
{
    return ((value >> shift) & 1u) != 0;
}

std::uint32_t clearFlag(std::uint8_t* obj, std::uint32_t offset, unsigned shift, std::uint32_t mask,
                        std::uint32_t status, std::uint32_t eax, AIState7BEnv& env)
{
    const auto self = addressOf(obj);
    if (!bit(load32(obj, offset), shift)) {
        return eax;
    }
    store32(obj, offset, load32(obj, offset) & mask);
    call2(env, kFlagPair, self, status, 0);
    return call0(env, kFlagRefresh, self);
}

std::uint32_t setFlag(std::uint8_t* obj, std::uint32_t offset, unsigned shift, std::uint32_t mask,
                      std::uint32_t status, std::uint32_t eax, AIState7BEnv& env)
{
    const auto self = addressOf(obj);
    if (bit(load32(obj, offset), shift)) {
        return eax;
    }
    store32(obj, offset, load32(obj, offset) | mask);
    call2(env, kFlagPair, self, status, 1);
    return call0(env, kFlagRefresh, self);
}

std::uint32_t clearLoco214(std::uint32_t loco, std::uint32_t mask)
{
    if (loco == 0) {
        return 0;
    }
    const auto node = load32(asPtr(loco), 0x214);
    if (node == 0) {
        return 0;
    }
    store32(asPtr(node), 0x40, load32(asPtr(node), 0x40) & mask);
    return node;
}

std::uint32_t spray(AIState7BEnv& env, std::uint32_t loco)
{
    const std::uint32_t args[2] = {0x00BF21D8u, 0};
    return callExternal(env, kSpray, loco, args, 2);
}

std::uint32_t exitThenSpray(std::uint8_t* self, std::uint32_t arg, AIState7BEnv& env)
{
    auto eax = call1(env, kBaseExit, addressOf(self), arg);
    const auto loco = locoOf(unitOf(machineOf(self)));
    if (loco != 0) {
        eax = spray(env, loco);
    }
    return eax;
}

bool fartherThan(std::uint32_t leftBits, std::uint32_t rightBits)
{
    return !(bitsToFloat(leftBits) > bitsToFloat(rightBits));
}

float distSq(std::uint32_t ax, std::uint32_t ay, std::uint32_t bx, std::uint32_t by)
{
    const float dx = bitsToFloat(ax) - bitsToFloat(bx);
    const float dy = bitsToFloat(ay) - bitsToFloat(by);
    return dx * dx + dy * dy;
}

void copy3(std::uint8_t* dst, std::uint32_t src)
{
    store32(dst, 0x20, load32(asPtr(src), 0));
    store32(dst, 0x24, load32(asPtr(src), 4));
    store32(dst, 0x28, load32(asPtr(src), 8));
}

std::uint32_t goalObject(std::uint8_t* self, AIState7BEnv& env)
{
    return call0(env, kGoal, machineOf(self));
}

bool goalBlocked(std::uint32_t goal)
{
    return goal == 0 || (asPtr(goal)[0x80] & 1u) != 0 || (asPtr(goal)[0x471] & 1u) != 0;
}

void clearTemplateApproach(std::uint32_t unit, std::uint32_t loco)
{
    const auto templ = load32(asPtr(unit), 4);
    if (!bit(load32(asPtr(templ), 0xC0), 0x1A)) {
        return;
    }
    clearLoco214(loco, 0xFFFFFFF7u);
}

std::uint32_t approachOnExit(std::uint8_t* self, std::uint32_t arg, bool pushedStatus,
                             AIState7BEnv& env)
{
    const auto state = addressOf(self);
    auto eax = call1(env, kBaseExit, state, arg);
    const auto unit = unitOf(machineOf(self));
    const auto loco = locoOf(unit);
    if (pushedStatus) {
        eax = call2(env, kStatus, unit, 0x73u, 0);
    }
    if (loco != 0) {
        call1(env, 0x006E6360u, loco, 0);
        clearTemplateApproach(unit, loco);
        if (low(vcall0(env, loco, 0x274))) {
            const float squared = distSq(load32(self, 0x20), load32(self, 0x24), load32(asPtr(unit), 0x38),
                                         load32(asPtr(unit), 0x3C));
            std::uint32_t leftBits = 0;
            std::memcpy(&leftBits, &squared, 4);
            if (!fartherThan(env.nearBits, leftBits)) {
                call1(env, 0x0078D1E0u, unit, state + 0x20);
            }
        }
        eax = vcall0(env, loco, 0x270);
    }
    self[pushedStatus ? 0x73 : 0x65] = 0;
    return eax;
}

}  // namespace

std::uint32_t callExternal(AIState7BEnv& env, std::uint32_t target, std::uint32_t ecx,
                           const std::uint32_t* args, int argc)
{
    return env.call(env.callCtx, target, ecx, args, argc);
}

// 0x007B0880 AIGarrisonedState 00C2C268 slot 4.
std::uint32_t aiGarrisonedOnEnter(std::uint8_t* self, std::uint32_t, const std::uint32_t*, int,
                                  AIState7BEnv& env)
{
    const auto state = addressOf(self);
    const auto unit = unitOf(machineOf(self));
    auto created = call1(env, kOperatorNew, state, 0x80);
    if (created != 0) {
        created = call1(env, 0x007A1BD0u, created, unit);
    }
    store32(self, 0x20, created);
    auto* unitBytes = asPtr(unit);
    setFlag(unitBytes, 0xA0, 0x15, 0x200000u, 0x15, 0, env);
    const auto occupant = load32(self, 0x20);
    return vcall1(env, occupant, 0x1C, load32(asPtr(occupant), 0x68));
}

// 0x007B08F0 AIGarrisonedState slot 6.
std::uint32_t aiGarrisonedOnExit(std::uint8_t* self, std::uint32_t, const std::uint32_t*, int,
                                 AIState7BEnv& env)
{
    const auto occupant = load32(self, 0x20);
    if (occupant != 0) {
        vcall1(env, occupant, 0, 1);
    }
    const auto machine = machineOf(self);
    store32(self, 0x20, 0);
    auto* unit = asPtr(unitOf(machine));
    return clearFlag(unit, 0xA0, 0x15, 0xFFDFFFFFu, 0x15, machine, env);
}

// 0x007B0940 AIGarrisonedState slot 5.
std::uint32_t aiGarrisonedUpdate(std::uint8_t* self, std::uint32_t, const std::uint32_t*, int,
                                 AIState7BEnv& env)
{
    std::uint32_t status = kFail;
    const auto occupant = load32(self, 0x20);
    if (occupant != 0) {
        status = vcall0(env, occupant, 0x10);
    }
    auto cursor = call0(env, 0x006EFE40u, unitOf(machineOf(self)));
    if (cursor != 0) {
        for (;;) {
            if (call0(env, 0x006EFE40u, cursor) == 0) {
                break;
            }
            cursor = call0(env, 0x006EFE40u, cursor);
            if (cursor == 0) {
                break;
            }
        }
    }
    return status;
}

// 0x007B1150 UnitAIDeadState 00C2ACE0 slot 6.
std::uint32_t unitAIDeadOnExit(std::uint8_t* self, std::uint32_t, const std::uint32_t*, int,
                               AIState7BEnv& env)
{
    const auto machine = machineOf(self);
    auto* unit = asPtr(unitOf(machine));
    if ((unit[0xB2] & 1u) == 0) {
        return machine;
    }
    store32(unit, 0xB0, load32(unit, 0xB0) & 0xFFFEFFFFu);
    call2(env, kFlagPair, addressOf(unit), 0x90, 0);
    return call0(env, kFlagRefresh, addressOf(unit));
}

// 0x007B17D0 AIMoveToState 00C2F388 slot 6.
std::uint32_t aiMoveToOnExit(std::uint8_t* self, std::uint32_t, const std::uint32_t* stack, int,
                             AIState7BEnv& env)
{
    return exitThenSpray(self, stack[0], env);
}

// 0x007B19F0 AIMoveToCoverState 00C32468 slot 6.
std::uint32_t aiMoveToCoverOnExit(std::uint8_t* self, std::uint32_t, const std::uint32_t* stack, int,
                                  AIState7BEnv& env)
{
    exitThenSpray(self, stack[0], env);
    const auto machine = machineOf(self);
    const auto unit = unitOf(machine);
    std::uint32_t left = 0;
    const float squared = distSq(load32(self, 0x20), load32(self, 0x24), load32(asPtr(unit), 0x38),
                                 load32(asPtr(unit), 0x3C));
    std::memcpy(&left, &squared, 4);
    if (fartherThan(env.limitBits, left)) {
        return machine;
    }
    auto eax = call0(env, 0x00687C30u, unit);
    if (eax == 0) {
        return eax;
    }
    return call0(env, 0x006F6A20u, eax);
}

// 0x007B1A80 AIMoveToStateSA 00C2F3E8 slot 5.
std::uint32_t aiMoveToStateSAUpdate(std::uint8_t* self, std::uint32_t, const std::uint32_t*, int,
                                    AIState7BEnv& env)
{
    if (self[0x54] != 0) {
        if (frameOf(env) > load32(self, 0x50)) {
            return kFail;
        }
        call1(env, 0x004D9AB0u, unitOf(machineOf(self)), 0x8F);
        return 0;
    }
    const auto goal = goalObject(self, env);
    if (goal != 0) {
        copy3(self, goal + 0x38);
    }
    return call0(env, kContinue, addressOf(self));
}

// 0x007B1AF0 AIMoveOutOfTheWayState 00C2F448 slot 6.
std::uint32_t aiMoveOutOfTheWayOnExit(std::uint8_t* self, std::uint32_t, const std::uint32_t* stack, int,
                                      AIState7BEnv& env)
{
    call1(env, kBaseExit, addressOf(self), stack[0]);
    const auto machine = machineOf(self);
    const auto unit = unitOf(machine);
    const auto loco = locoOf(unit);
    if (loco != 0) {
        call0(env, 0x006996C0u, loco);
        asPtr(loco)[0x434] = 0;
        store32(asPtr(loco), 0x1E8, 0);
        store32(asPtr(loco), 0x1EC, 0);
    }
    return clearFlag(asPtr(unit), 0xB0, 0x16, 0xFFBFFFFFu, 0x96, machine, env);
}

// 0x007B1B70 AIMoveAndTightenState 00C2F4B0 slot 5.
std::uint32_t aiMoveAndTightenUpdate(std::uint8_t* self, std::uint32_t, const std::uint32_t*, int,
                                     AIState7BEnv& env)
{
    if (self[0x54] != 0) {
        const auto loco = locoOf(unitOf(machineOf(self)));
        if (load32(asPtr(loco), 0x150) != 0 && asPtr(loco)[0x428] == 0) {
            self[0x4C] = 1;
            self[0x54] = 0;
        }
    }
    return call0(env, kContinue, addressOf(self));
}

// 0x007B1BB0 AIMoveAwayPanicState 00C2F518 slot 6.
std::uint32_t aiMoveAwayPanicOnExit(std::uint8_t* self, std::uint32_t, const std::uint32_t* stack, int,
                                    AIState7BEnv& env)
{
    auto eax = call1(env, kBaseExit, addressOf(self), stack[0]);
    const auto unit = unitOf(machineOf(self));
    if (unit == 0) {
        return eax;
    }
    const auto loco = locoOf(unit);
    if (loco != 0) {
        eax = call1(env, kLocoMode, loco, 0);
    }
    return clearFlag(asPtr(unit), 0xB4, 0x0E, 0xFFFFBFFFu, 0xAE, eax, env);
}

// 0x007B1C10 AIMoveAwayPanicState slot 5.
std::uint32_t aiMoveAwayPanicUpdate(std::uint8_t* self, std::uint32_t, const std::uint32_t*, int,
                                    AIState7BEnv& env)
{
    if (self[0x54] != 0) {
        const auto loco = locoOf(unitOf(machineOf(self)));
        const auto path = load32(asPtr(loco), 0x150);
        if (path != 0 && asPtr(loco)[0x428] == 0) {
            copy3(self, load32(asPtr(path), 0x14) + 0x0C);
            self[0x4C] = 0;
            self[0x54] = 0;
        }
    }
    return call0(env, kContinue, addressOf(self));
}

// 0x007B1C60 AIMoveAwayFromRepulsorsState 00C2F580 slot 6.
std::uint32_t aiMoveAwayFromRepulsorsOnExit(std::uint8_t* self, std::uint32_t, const std::uint32_t* stack,
                                            int, AIState7BEnv& env)
{
    auto eax = call1(env, kBaseExit, addressOf(self), stack[0]);
    const auto unit = unitOf(machineOf(self));
    if (unit == 0) {
        return eax;
    }
    return clearFlag(asPtr(unit), 0xB4, 0x0E, 0xFFFFBFFFu, 0xAE, eax, env);
}

// 0x007B1E20 AIFearState 00C2F5F0 slot 6.
std::uint32_t aiFearOnExit(std::uint8_t* self, std::uint32_t, const std::uint32_t* stack, int,
                           AIState7BEnv& env)
{
    auto eax = call1(env, kBaseExit, addressOf(self), stack[0]);
    const auto unit = unitOf(machineOf(self));
    if (unit == 0) {
        return eax;
    }
    eax = clearFlag(asPtr(unit), 0xCC, 0x0F, 0xFFFF7FFFu, 0x16F, eax, env);
    const auto loco = locoOf(unit);
    if (loco == 0) {
        return eax;
    }
    eax = call1(env, kLocoMode, loco, 0);
    asPtr(loco)[0x43D] = 0;
    return eax;
}

// 0x007B1F30 AIUncontrollableCower 00C2C768 slot 4.
std::uint32_t aiUncontrollableCowerOnEnter(std::uint8_t* self, std::uint32_t, const std::uint32_t*, int,
                                           AIState7BEnv& env)
{
    const auto unit = unitOf(machineOf(self));
    self[0x20] = static_cast<std::uint8_t>(call0(env, 0x006A5F50u, unit));
    call1(env, 0x00687500u, unit, 0);
    call2(env, kStatus, unit, 0x48, 1);
    setFlag(asPtr(unit), 0xCC, 0x14, 0x100000u, 0x174, 0, env);
    call1(env, 0x005AA240u, machineOf(self), 0x00C2C7B8u);
    return call0(env, 0x00799B70u, addressOf(self));
}

// 0x007B1FA0 AIUncontrollableCower slot 6.
std::uint32_t aiUncontrollableCowerOnExit(std::uint8_t* self, std::uint32_t, const std::uint32_t* stack,
                                          int, AIState7BEnv& env)
{
    const auto unit = unitOf(machineOf(self));
    call1(env, 0x00687500u, unit, self[0x20]);
    call2(env, kStatus, unit, 0x48, 0);
    clearFlag(asPtr(unit), 0xCC, 0x14, 0xFFEFFFFFu, 0x174, 0, env);
    call0(env, 0x005AA250u, machineOf(self));
    return call1(env, 0x00799C30u, addressOf(self), stack[0]);
}

// 0x007B2010 AIMoveAwayAndCowerState 00C2F650 slot 6.
std::uint32_t aiMoveAwayAndCowerOnExit(std::uint8_t* self, std::uint32_t, const std::uint32_t* stack, int,
                                       AIState7BEnv& env)
{
    call1(env, kBaseExit, addressOf(self), stack[0]);
    const auto unit = unitOf(machineOf(self));
    if (unit == 0) {
        return 0;
    }
    const auto loco = locoOf(unit);
    if (loco == 0) {
        return 0;
    }
    return call1(env, kLocoMode, loco, 0);
}

// 0x007B2050 AIMoveAwayAndCowerState slot 5.
std::uint32_t aiMoveAwayAndCowerUpdate(std::uint8_t* self, std::uint32_t, const std::uint32_t*, int,
                                       AIState7BEnv& env)
{
    const auto unit = unitOf(machineOf(self));
    if (goalObject(self, env) == 0 || locoOf(unit) == 0) {
        return kFail;
    }
    if (self[0x54] == 0) {
        return call0(env, kContinue, addressOf(self));
    }
    const auto loco = locoOf(unit);
    const auto path = load32(asPtr(loco), 0x150);
    if (path != 0 && asPtr(loco)[0x428] == 0) {
        copy3(self, load32(asPtr(path), 0x14) + 0x0C);
        self[0x4C] = 0;
        self[0x54] = 0;
    }
    return 0;
}

// 0x007B20D0 AIBackAwayState 00C2F6B8 slot 6.
std::uint32_t aiBackAwayOnExit(std::uint8_t* self, std::uint32_t, const std::uint32_t* stack, int,
                               AIState7BEnv& env)
{
    auto eax = call1(env, kBaseExit, addressOf(self), stack[0]);
    const auto unit = unitOf(machineOf(self));
    if (unit == 0) {
        return eax;
    }
    auto* unitBytes = asPtr(unit);
    if ((unitBytes[0xCE] & 1u) != 0) {
        store32(unitBytes, 0xCC, load32(unitBytes, 0xCC) & 0xFFFEFFFFu);
        call2(env, kFlagPair, unit, 0x170, 0);
        eax = call0(env, kFlagRefresh, unit);
    }
    const auto loco = locoOf(unit);
    if (loco == 0) {
        return eax;
    }
    return call1(env, kLocoMode, loco, 0);
}

// 0x007B28D0 AIAttackApproachTargetState 00C2F718 slot 6.
std::uint32_t aiAttackApproachTargetOnExit(std::uint8_t* self, std::uint32_t, const std::uint32_t* stack,
                                           int, AIState7BEnv& env)
{
    return approachOnExit(self, stack[0], true, env);
}

// 0x007B2D50 AIAttackPositionApproachTargetState 00C2F788 slot 6.
std::uint32_t aiAttackPositionApproachTargetOnExit(std::uint8_t* self, std::uint32_t,
                                                   const std::uint32_t* stack, int, AIState7BEnv& env)
{
    return approachOnExit(self, stack[0], false, env);
}

// 0x007B3950 AIAttackPursueTargetState 00C2F878 slot 6.
std::uint32_t aiAttackPursueTargetOnExit(std::uint8_t* self, std::uint32_t, const std::uint32_t* stack,
                                         int, AIState7BEnv& env)
{
    const auto eax = call1(env, kBaseExit, addressOf(self), stack[0]);
    self[0x63] = 0;
    return eax;
}

// 0x007B3EF0 AIAttackMeleeSquishState 00C2F9C8 slot 6.
std::uint32_t aiAttackMeleeSquishOnExit(std::uint8_t* self, std::uint32_t, const std::uint32_t* stack,
                                        int, AIState7BEnv& env)
{
    call1(env, kBaseExit, addressOf(self), stack[0]);
    return call2(env, kStatus, unitOf(machineOf(self)), 0x1E, 0);
}

// 0x007B4780 AIAttackMeleeEngageState 00C2FA38 slot 5.
std::uint32_t aiAttackMeleeEngageUpdate(std::uint8_t* self, std::uint32_t, const std::uint32_t*, int,
                                        AIState7BEnv& env)
{
    const auto state = addressOf(self);
    const auto unit = unitOf(machineOf(self));
    const auto status = call0(env, 0x007B42E0u, state);
    const auto flag = (state & 0xFFFFFF00u) | (status == kDone ? 1u : 0u);
    call2(env, kStatus, unit, 0x1E, flag);
    return status;
}

// 0x007B4F90 AIAttackMoveToState 00C32180 slot 6.
std::uint32_t aiAttackMoveToOnExit(std::uint8_t* self, std::uint32_t, const std::uint32_t* stack, int,
                                   AIState7BEnv& env)
{
    const auto unit = unitOf(machineOf(self));
    auto* unitBytes = asPtr(unit);
    clearFlag(unitBytes, 0xB0, 0x19, 0xFDFFFFFFu, 0x99, 0, env);
    vcall1(env, load32(self, 0x58), 0x1C, 0);
    call1(env, kBaseExit, addressOf(self), stack[0]);
    const auto loco = locoOf(unit);
    auto eax = unit;
    if (loco != 0) {
        eax = spray(env, loco);
    }
    if (bit(load32(unitBytes, 0x90), 0x0F)) {
        eax = call2(env, kStatus, unit, 0x8F, 0);
    }
    return eax;
}

// 0x007B52A0 AIFollowWaypointPathState 00C2FCB0 slot 6.
std::uint32_t aiFollowWaypointPathOnExit(std::uint8_t* self, std::uint32_t, const std::uint32_t* stack,
                                         int, AIState7BEnv& env)
{
    call1(env, kBaseExit, addressOf(self), stack[0]);
    return clearLoco214(locoOf(unitOf(machineOf(self))), 0xFFFFFFF7u);
}

// 0x007B52D0 AIFollowWaypointPathStateAndEvacuate 00C324D0 slot 6.
std::uint32_t aiFollowWaypointPathEvacuateOnExit(std::uint8_t* self, std::uint32_t,
                                                 const std::uint32_t* stack, int, AIState7BEnv& env)
{
    call1(env, kBaseExit, addressOf(self), stack[0]);
    const auto unit = unitOf(machineOf(self));
    clearLoco214(locoOf(unit), 0xFFFFFFF7u);
    return call1(env, kLocoMode, locoOf(unit), 0);
}

// 0x007B5320 AIFollowWaypointPathExactState 00C2FD20 slot 6.
std::uint32_t aiFollowWaypointPathExactOnExit(std::uint8_t* self, std::uint32_t,
                                              const std::uint32_t* stack, int, AIState7BEnv& env)
{
    auto eax = call1(env, kBaseExit, addressOf(self), stack[0]);
    const auto loco = locoOf(unitOf(machineOf(self)));
    if (loco != 0 && load32(asPtr(loco), 0x214) != 0) {
        eax = call1(env, 0x006B6E30u, loco, load32(self, 0x50));
        asPtr(loco)[0x434] = 0;
        const auto node = load32(asPtr(loco), 0x214);
        store32(asPtr(node), 0x40, load32(asPtr(node), 0x40) & 0xFFFFFFFDu);
    }
    return eax;
}

// 0x007B5370 AIFollowWaypointPathExactState slot 5.
std::uint32_t aiFollowWaypointPathExactUpdate(std::uint8_t* self, std::uint32_t, const std::uint32_t*,
                                              int, AIState7BEnv& env)
{
    const auto loco = locoOf(unitOf(machineOf(self)));
    if (loco != 0) {
        asPtr(loco)[0x434] = 1;
    }
    return call0(env, kContinue, addressOf(self));
}

// 0x007B5460 AIAttackFollowWaypointPathState 00C321E8 slot 6.
std::uint32_t aiAttackFollowWaypointPathOnExit(std::uint8_t* self, std::uint32_t,
                                               const std::uint32_t* stack, int, AIState7BEnv& env)
{
    vcall1(env, load32(self, 0x6C), 0x1C, 0);
    call1(env, kBaseExit, addressOf(self), stack[0]);
    return clearLoco214(locoOf(unitOf(machineOf(self))), 0xFFFFFFF7u);
}

// 0x007B5630 AIWanderState 00C32258 slot 6.
std::uint32_t aiWanderOnExit(std::uint8_t* self, std::uint32_t, const std::uint32_t* stack, int,
                             AIState7BEnv& env)
{
    call1(env, kBaseExit, addressOf(self), stack[0]);
    const auto machine = machineOf(self);
    const auto unit = unitOf(machine);
    clearLoco214(unit == 0 ? 0u : locoOf(unit), 0xFFFFFFF7u);
    if (unit == 0) {
        return machine;
    }
    auto eax = clearFlag(asPtr(unit), 0xB0, 0x13, 0xFFF7FFFFu, 0x93, machine, env);
    const auto loco = locoOf(unit);
    if (loco == 0) {
        return eax;
    }
    return call1(env, kLocoMode, loco, 0);
}

// 0x007B56B0 AIWanderInPlaceState 00C2FD90 slot 6.
std::uint32_t aiWanderInPlaceOnExit(std::uint8_t* self, std::uint32_t, const std::uint32_t* stack, int,
                                    AIState7BEnv& env)
{
    const auto eax = call1(env, kBaseExit, addressOf(self), stack[0]);
    const auto loco = locoOf(unitOf(machineOf(self)));
    if (loco == 0) {
        return eax;
    }
    return call1(env, kLocoMode, loco, 0);
}

// 0x007B5830 AIPanicState 00C322B8 slot 6.
std::uint32_t aiPanicOnExit(std::uint8_t* self, std::uint32_t, const std::uint32_t* stack, int,
                            AIState7BEnv& env)
{
    const auto unit = unitOf(machineOf(self));
    clearFlag(asPtr(unit), 0xB4, 0x0E, 0xFFFFBFFFu, 0xAE, 0, env);
    return call1(env, kBaseExit, addressOf(self), stack[0]);
}

// 0x007B5E30 AICombineState 00C2FE60 slot 5.
std::uint32_t aiCombineUpdate(std::uint8_t* self, std::uint32_t, const std::uint32_t*, int,
                              AIState7BEnv& env)
{
    const auto found = call0(env, 0x007A5B20u, addressOf(self));
    if (found == 0) {
        return call0(env, kContinue, addressOf(self));
    }
    return found;
}

// 0x007B60D0 AIChargeTargetState 00C2FF50 slot 6.
std::uint32_t aiChargeTargetOnExit(std::uint8_t* self, std::uint32_t, const std::uint32_t* stack,
                                   int, AIState7BEnv& env)
{
    const auto machine = machineOf(self);
    const auto unit = unitOf(machine);
    const auto loco = unit == 0 ? 0u : locoOf(unit);
    if (unit == 0 || loco == 0) {
        return machine;
    }
    call1(env, 0x004D9AB0u, unit, 0x95);
    call1(env, kLocoMode, loco, 0);
    asPtr(machine)[0x7B] = 0;
    return call1(env, kBaseExit, addressOf(self), stack[0]);
}

// 0x007B62D0 AIMoveAndDeleteState 00C2FC48 slot 6.
std::uint32_t aiMoveAndDeleteOnExit(std::uint8_t* self, std::uint32_t, const std::uint32_t* stack, int,
                                    AIState7BEnv& env)
{
    call0(env, 0x005AA250u, machineOf(self));
    return call1(env, kBaseExit, addressOf(self), stack[0]);
}

// 0x007B62F0 AIMoveToAndEvacuateState 00C30228 slot 6.
std::uint32_t aiMoveToAndEvacuateOnExit(std::uint8_t* self, std::uint32_t, const std::uint32_t* stack,
                                        int, AIState7BEnv& env)
{
    call1(env, kBaseExit, addressOf(self), stack[0]);
    const auto loco = locoOf(unitOf(machineOf(self)));
    if (loco != 0) {
        spray(env, loco);
    }
    return call1(env, kLocoMode, loco, 0);
}

// 0x007B64A0 AIDeployState 00C2D3C8 slot 4.
std::uint32_t aiDeployOnEnter(std::uint8_t* self, std::uint32_t, const std::uint32_t*, int,
                              AIState7BEnv& env)
{
    const auto unit = unitOf(machineOf(self));
    const auto loco = locoOf(unit);
    if (loco == 0) {
        return kFail;
    }
    auto mode = vcall0(env, loco, 0x1B0);
    if (mode == 0) {
        return kFail;
    }
    if (bit(load32(asPtr(unit), 0x84), 0x1E)) {
        return kDone;
    }
    mode = vcall0(env, loco, 0x1B0);
    store32(self, 0x20, call0(env, 0x00681840u, mode) + frameOf(env));
    call1(env, 0x004D9AF0u, unit, 0xC9);
    call2(env, kStatus, unit, 0x43, 1);
    return 0;
}

// 0x007B6530 AIDeployState slot 6.
std::uint32_t aiDeployOnExit(std::uint8_t* self, std::uint32_t, const std::uint32_t*, int,
                             AIState7BEnv& env)
{
    auto* unit = asPtr(unitOf(machineOf(self)));
    const auto unitBits = addressOf(unit);
    clearFlag(unit, 0xB8, 9, 0xFFFFFDFFu, 0xC9, 0, env);
    setFlag(unit, 0xC0, 9, 0x200u, 0x109, 0, env);
    call2(env, kStatus, unitBits, 0x43, 0);
    return call2(env, kStatus, unitBits, 0x3E, 1);
}

// 0x007B65B0 AIUnDeployState 00C2D428 slot 4.
std::uint32_t aiUnDeployOnEnter(std::uint8_t* self, std::uint32_t, const std::uint32_t*, int,
                                AIState7BEnv& env)
{
    const auto unit = unitOf(machineOf(self));
    const auto loco = locoOf(unit);
    if (loco == 0) {
        return kFail;
    }
    if (vcall0(env, loco, 0x1B0) == 0) {
        return kFail;
    }
    if (!bit(load32(asPtr(unit), 0x84), 0x1E)) {
        return bit(load32(asPtr(unit), 0x88), 3) ? 0u : kFail;
    }
    call2(env, kStatus, unit, 0x3E, 0);
    call2(env, kStatus, unit, 0x43, 1);
    const auto mode = vcall0(env, loco, 0x1B0);
    const auto ready = call0(env, 0x006818C0u, mode) + frameOf(env);
    store32(self, 0x20, ready);
    call1(env, 0x004D9AF0u, unit, 0xC7);
    call2(env, 0x00757B40u, unit, 8, ready + 2);
    return 0;
}

// 0x007B6670 AIUnDeployState slot 6.
std::uint32_t aiUnDeployOnExit(std::uint8_t* self, std::uint32_t, const std::uint32_t*, int,
                               AIState7BEnv& env)
{
    auto* first = env.scratch;
    auto* second = env.scratch + 0x3C;
    const auto firstBits = addressOf(first);
    const auto secondBits = addressOf(second);
    const std::uint32_t clearFirst[3] = {firstBits, 0, 0x3C};
    callExternal(env, 0x004D2F98u, firstBits, clearFirst, 3);
    store32(first, 0x18, load32(first, 0x18) | 0x80u);
    store32(first, 0x20, load32(first, 0x20) | 0x200u);
    const std::uint32_t clearSecond[3] = {secondBits, 0, 0x3C};
    callExternal(env, 0x004D2F98u, firstBits, clearSecond, 3);
    const auto unit = unitOf(machineOf(self));
    call2(env, 0x006A5060u, unit, firstBits, secondBits);
    call2(env, kStatus, unit, 0x3E, 0);
    call2(env, kStatus, unit, 0x43, 0);
    return call1(env, 0x006F0A20u, unit, 8);
}

// 0x007B66E0 AITNGuardMachine 00C2D488 slot 3.
std::uint32_t aiTNGuardMachineXfer(std::uint8_t* self, std::uint32_t eax, const std::uint32_t* stack,
                                   int, AIState7BEnv& env)
{
    const auto xfer = stack[0];
    if (bit(load32(asPtr(xfer), 4), 3)) {
        return (eax & 0xFFFFFF00u) | 1u;
    }
    store32(env.scratch, 0, (xfer & 0xFFFF0000u) | 0x0101u);
    vcall1(env, xfer, 0x18, addressOf(env.scratch));
    call1(env, 0x005C0A40u, addressOf(self), xfer);
    const std::uint32_t pair[2] = {xfer, addressOf(self) + 0x8C};
    callExternal(env, 0x0054A090u, addressOf(self), pair, 2);
    vcall1(env, xfer, 0x58, addressOf(self) + 0x80);
    const std::uint32_t tail[2] = {addressOf(self) + 0x90, 4};
    return vcall(env, xfer, 0x14, tail, 2);
}

// 0x007B9820 AIStateMachine 00C305E0 slot 18.
std::uint32_t aiStateMachineSlot18(std::uint8_t* self, std::uint32_t, const std::uint32_t*, int,
                                   AIState7BEnv& env)
{
    const auto machine = addressOf(self);
    call0(env, 0x007B7B50u, machine);
    auto state = call1(env, kOperatorNew, machine, 0x24);
    if (state != 0) {
        call2(env, 0x005A9F60u, state, machine, 0xA232715Bu);
        store32(asPtr(state), 0x20, 0);
        store32(asPtr(state), 0, 0x00C2DA10u);
    }
    const std::uint32_t first[5] = {0x44, state, 0x7D0, 0, 0};
    callExternal(env, 0x005BDB90u, machine, first, 5);
    state = call1(env, kOperatorNew, machine, 0x3C);
    if (state != 0) {
        call2(env, 0x005A9F60u, state, machine, 0x061D6D91u);
        store32(asPtr(state), 0x38, 0);
        for (std::uint32_t offset = 0x20; offset <= 0x34; offset += 4) {
            store32(asPtr(state), offset, 0);
        }
        store32(asPtr(state), 0, 0x00C30BD0u);
    }
    const std::uint32_t second[5] = {0x7D0, state, 0, 0, 0};
    return callExternal(env, 0x005BDB90u, machine, second, 5);
}

// 0x007BB4A0 UnitAILandState 00C30B70 slot 6.
std::uint32_t unitAILandOnExit(std::uint8_t* self, std::uint32_t, const std::uint32_t* stack, int,
                               AIState7BEnv& env)
{
    const auto machine = machineOf(self);
    const auto unit = unitOf(machine);
    const auto loco = locoOf(unit);
    if (loco == 0) {
        return machine;
    }
    call1(env, 0x007A9AB0u, addressOf(self), stack[0]);
    auto result = call1(env, kLocoMode, loco, 5);
    result = clearFlag(asPtr(unit), 0xB4, 6, 0xFFFFFFBFu, 0xA6, result, env);
    return result;
}

// 0x007BB8E0 UnitAIReturnForLandingState 00C2DE50 slot 19.
std::uint32_t unitAIReturnForLandingSlot19(std::uint8_t* self, std::uint32_t, const std::uint32_t*, int,
                                           AIState7BEnv& env)
{
    const auto unit = unitOf(machineOf(self));
    const auto templ = load32(asPtr(unit), 4);
    const auto created = call1(env, kOperatorNew, templ, 0x80);
    if (created == 0) {
        return 0;
    }
    const auto ctor = bit(load32(asPtr(templ), 0xC8), 8) ? 0x007BB2B0u : 0x007BB640u;
    return call1(env, ctor, created, unit);
}

// 0x007BB930 UnitAITakeOffState 00C30BD0 slot 6.
std::uint32_t unitAITakeOffOnExit(std::uint8_t* self, std::uint32_t, const std::uint32_t* stack, int,
                                  AIState7BEnv& env)
{
    const auto machine = machineOf(self);
    const auto unit = unitOf(machine);
    const auto loco = locoOf(unit);
    if (loco == 0) {
        return machine;
    }
    call2(env, kStatus, unit, 0x19, 0);
    call1(env, 0x007A9AB0u, addressOf(self), stack[0]);
    const std::uint32_t query[2] = {load32(asPtr(unit), 0xE4), 0};
    auto result = callExternal(env, 0x006E4760u, addressOf(self), query, 2);
    if (result != 0 && asPtr(load32(asPtr(loco), 4))[0x62] == 0) {
        result = vcall1(env, result, 0x18, load32(asPtr(unit), 0xDC));
    }
    return clearFlag(asPtr(unit), 0xB4, 5, 0xFFFFFFDFu, 0xA5, result, env);
}

// 0x007BC320 AIFollowExitProductionPathState 00C31210 slot 19.
std::uint32_t aiFollowExitProductionPathSlot19(std::uint8_t* self, std::uint32_t,
                                               const std::uint32_t*, int, AIState7BEnv& env)
{
    const auto state = addressOf(self);
    const auto created = call1(env, kOperatorNew, state, 0x84);
    if (created == 0) {
        return 0;
    }
    call1(env, 0x007AB5B0u, created, unitOf(machineOf(self)));
    store32(asPtr(created), 0, 0x00C335E8u);
    return created;
}

// 0x007BC4C0 AIMoveToAndCollectState 00C31488 slot 6.
std::uint32_t aiMoveToAndCollectOnExit(std::uint8_t* self, std::uint32_t, const std::uint32_t* stack,
                                       int, AIState7BEnv& env)
{
    const auto goal = goalObject(self, env);
    if (goal != 0) {
        const auto unit = unitOf(machineOf(self));
        auto* vector = env.scratch;
        store32(vector, 0, 0);
        store32(vector, 4, 0);
        store32(vector, 8, 0);
        call1(env, 0x00748590u, goal, addressOf(vector));
        const auto begin = load32(vector, 0);
        const auto end = load32(vector, 4);
        auto ecxBits = goal;
        for (auto cursor = begin; cursor != end; cursor += 4) {
            ecxBits = load32(asPtr(cursor), 0);
            vcall1(env, ecxBits, 0x64, load32(asPtr(unit), 0xDC));
        }
        if (begin != 0) {
            call1(env, 0x004166B0u, ecxBits, begin);
        }
    }
    call1(env, kBaseExit, addressOf(self), stack[0]);
    const auto unit = unitOf(machineOf(self));
    const auto loco = locoOf(unit);
    if (loco != 0) {
        return spray(env, loco);
    }
    return unit;
}

// 0x007BD770 AIInternalMoveToState 00C31B10 slot 5.
std::uint32_t aiInternalMoveTo31B10Update(std::uint8_t* self, std::uint32_t, const std::uint32_t*, int,
                                          AIState7BEnv& env)
{
    if (goalBlocked(goalObject(self, env))) {
        return kFail;
    }
    return call0(env, kContinue, addressOf(self));
}

// 0x007BD7A0 AIDockApproachState 00C31AA8 slot 6.
std::uint32_t aiDockApproachOnExit(std::uint8_t* self, std::uint32_t, const std::uint32_t* stack, int,
                                   AIState7BEnv& env)
{
    const auto reason = stack[0];
    const auto goal = goalObject(self, env);
    if (!goalBlocked(goal)) {
        const auto dock = call0(env, 0x00687810u, goal);
        if (dock != 0) {
            const auto unit = unitOf(machineOf(self));
            if (reason != 1 && low(vcall0(env, dock, 0x38))) {
                vcall1(env, dock, 0x20, unit);
                return call1(env, kBaseExit, addressOf(self), reason);
            }
            vcall1(env, dock, 0x34, unit);
        }
    }
    return call1(env, kBaseExit, addressOf(self), reason);
}

// 0x007BD830 AIInternalMoveToState 00C31B60 slot 5.
std::uint32_t aiInternalMoveTo31B60Update(std::uint8_t* self, std::uint32_t, const std::uint32_t*, int,
                                          AIState7BEnv& env)
{
    const auto goal = goalObject(self, env);
    if (goalBlocked(goal)) {
        return kFail;
    }
    const auto dock = call0(env, 0x00687810u, goal);
    if (dock == 0) {
        return kFail;
    }
    call2(env, load32(asPtr(load32(asPtr(dock), 0)), 0x14), dock, unitOf(machineOf(self)),
          addressOf(self) + 0x20);
    return call0(env, kContinue, addressOf(self));
}

// 0x007BD880 AIInternalMoveToState 00C31B60 slot 6.
std::uint32_t aiInternalMoveTo31B60OnExit(std::uint8_t* self, std::uint32_t, const std::uint32_t* stack,
                                          int, AIState7BEnv& env)
{
    const auto reason = stack[0];
    const auto goal = goalObject(self, env);
    if (!goalBlocked(goal)) {
        const auto dock = call0(env, 0x00687810u, goal);
        if (dock != 0) {
            const auto unit = unitOf(machineOf(self));
            if (low(vcall0(env, dock, 0x38)) && reason != 1) {
                vcall1(env, dock, 0x24, unit);
                return call1(env, kBaseExit, addressOf(self), reason);
            }
            vcall1(env, dock, 0x34, unit);
        }
    }
    return call1(env, kBaseExit, addressOf(self), reason);
}

// 0x007BD910 AIInternalMoveToState 00C31BB0 slot 5.
std::uint32_t aiInternalMoveTo31BB0Update(std::uint8_t* self, std::uint32_t, const std::uint32_t*, int,
                                          AIState7BEnv& env)
{
    const auto goal = goalObject(self, env);
    if (goalBlocked(goal)) {
        return kFail;
    }
    const auto dock = call0(env, 0x00687810u, goal);
    if (dock == 0 || !low(vcall0(env, dock, 0x38))) {
        return kFail;
    }
    call2(env, load32(asPtr(load32(asPtr(dock), 0)), 0x18), dock, unitOf(machineOf(self)),
          addressOf(self) + 0x20);
    return call0(env, kContinue, addressOf(self));
}

// 0x007BDA40 AIInternalMoveToState 00C31C00 slot 5.
std::uint32_t aiInternalMoveTo31C00Update(std::uint8_t* self, std::uint32_t, const std::uint32_t*, int,
                                          AIState7BEnv& env)
{
    const auto goal = goalObject(self, env);
    if (goalBlocked(goal)) {
        return kFail;
    }
    const auto dock = call0(env, 0x00687810u, goal);
    if (dock == 0) {
        return kFail;
    }
    call2(env, load32(asPtr(load32(asPtr(dock), 0)), 0x1C), dock, unitOf(machineOf(self)),
          addressOf(self) + 0x20);
    return call0(env, kContinue, addressOf(self));
}

// 0x007BDA90 AIInternalMoveToState 00C31C00 slot 6.
std::uint32_t aiInternalMoveTo31C00OnExit(std::uint8_t* self, std::uint32_t, const std::uint32_t* stack,
                                          int, AIState7BEnv& env)
{
    const auto goal = goalObject(self, env);
    if (!goalBlocked(goal)) {
        const auto dock = call0(env, 0x00687810u, goal);
        if (dock != 0) {
            vcall1(env, dock, 0x2C, unitOf(machineOf(self)));
        }
    }
    const auto machine = machineOf(self);
    call0(env, 0x005AA250u, machine);
    const auto loco = locoOf(unitOf(machine));
    if (loco != 0) {
        call1(env, 0x006E6360u, loco, 0);
    }
    return call1(env, kBaseExit, addressOf(self), stack[0]);
}

// 0x007BDEF0 AIGuardReturnState 00C31D08 slot 5.
std::uint32_t aiGuardReturnUpdate(std::uint8_t* self, std::uint32_t, const std::uint32_t*, int,
                                  AIState7BEnv& env)
{
    if (call0(env, 0x006A6210u, unitOf(machineOf(self))) != 0) {
        return kFail;
    }
    return call0(env, kContinue, addressOf(self));
}

bool runAIState7B(std::uint32_t va, std::uint8_t* self, std::uint32_t eaxIn, const std::uint32_t* stack,
                  int argc, AIState7BEnv& env, std::uint32_t& eaxOut)
{
    switch (va) {
    case 0x007B0880u: eaxOut = aiGarrisonedOnEnter(self, eaxIn, stack, argc, env); return true;
    case 0x007B08F0u: eaxOut = aiGarrisonedOnExit(self, eaxIn, stack, argc, env); return true;
    case 0x007B0940u: eaxOut = aiGarrisonedUpdate(self, eaxIn, stack, argc, env); return true;
    case 0x007B1150u: eaxOut = unitAIDeadOnExit(self, eaxIn, stack, argc, env); return true;
    case 0x007B17D0u: eaxOut = aiMoveToOnExit(self, eaxIn, stack, argc, env); return true;
    case 0x007B19F0u: eaxOut = aiMoveToCoverOnExit(self, eaxIn, stack, argc, env); return true;
    case 0x007B1A80u: eaxOut = aiMoveToStateSAUpdate(self, eaxIn, stack, argc, env); return true;
    case 0x007B1AF0u: eaxOut = aiMoveOutOfTheWayOnExit(self, eaxIn, stack, argc, env); return true;
    case 0x007B1B70u: eaxOut = aiMoveAndTightenUpdate(self, eaxIn, stack, argc, env); return true;
    case 0x007B1BB0u: eaxOut = aiMoveAwayPanicOnExit(self, eaxIn, stack, argc, env); return true;
    case 0x007B1C10u: eaxOut = aiMoveAwayPanicUpdate(self, eaxIn, stack, argc, env); return true;
    case 0x007B1C60u: eaxOut = aiMoveAwayFromRepulsorsOnExit(self, eaxIn, stack, argc, env); return true;
    case 0x007B1E20u: eaxOut = aiFearOnExit(self, eaxIn, stack, argc, env); return true;
    case 0x007B1F30u: eaxOut = aiUncontrollableCowerOnEnter(self, eaxIn, stack, argc, env); return true;
    case 0x007B1FA0u: eaxOut = aiUncontrollableCowerOnExit(self, eaxIn, stack, argc, env); return true;
    case 0x007B2010u: eaxOut = aiMoveAwayAndCowerOnExit(self, eaxIn, stack, argc, env); return true;
    case 0x007B2050u: eaxOut = aiMoveAwayAndCowerUpdate(self, eaxIn, stack, argc, env); return true;
    case 0x007B20D0u: eaxOut = aiBackAwayOnExit(self, eaxIn, stack, argc, env); return true;
    case 0x007B28D0u: eaxOut = aiAttackApproachTargetOnExit(self, eaxIn, stack, argc, env); return true;
    case 0x007B2D50u: eaxOut = aiAttackPositionApproachTargetOnExit(self, eaxIn, stack, argc, env); return true;
    case 0x007B3950u: eaxOut = aiAttackPursueTargetOnExit(self, eaxIn, stack, argc, env); return true;
    case 0x007B3EF0u: eaxOut = aiAttackMeleeSquishOnExit(self, eaxIn, stack, argc, env); return true;
    case 0x007B4780u: eaxOut = aiAttackMeleeEngageUpdate(self, eaxIn, stack, argc, env); return true;
    case 0x007B4F90u: eaxOut = aiAttackMoveToOnExit(self, eaxIn, stack, argc, env); return true;
    case 0x007B52A0u: eaxOut = aiFollowWaypointPathOnExit(self, eaxIn, stack, argc, env); return true;
    case 0x007B52D0u: eaxOut = aiFollowWaypointPathEvacuateOnExit(self, eaxIn, stack, argc, env); return true;
    case 0x007B5320u: eaxOut = aiFollowWaypointPathExactOnExit(self, eaxIn, stack, argc, env); return true;
    case 0x007B5370u: eaxOut = aiFollowWaypointPathExactUpdate(self, eaxIn, stack, argc, env); return true;
    case 0x007B5460u: eaxOut = aiAttackFollowWaypointPathOnExit(self, eaxIn, stack, argc, env); return true;
    case 0x007B5630u: eaxOut = aiWanderOnExit(self, eaxIn, stack, argc, env); return true;
    case 0x007B56B0u: eaxOut = aiWanderInPlaceOnExit(self, eaxIn, stack, argc, env); return true;
    case 0x007B5830u: eaxOut = aiPanicOnExit(self, eaxIn, stack, argc, env); return true;
    case 0x007B5E30u: eaxOut = aiCombineUpdate(self, eaxIn, stack, argc, env); return true;
    case 0x007B60D0u: eaxOut = aiChargeTargetOnExit(self, eaxIn, stack, argc, env); return true;
    case 0x007B62D0u: eaxOut = aiMoveAndDeleteOnExit(self, eaxIn, stack, argc, env); return true;
    case 0x007B62F0u: eaxOut = aiMoveToAndEvacuateOnExit(self, eaxIn, stack, argc, env); return true;
    case 0x007B64A0u: eaxOut = aiDeployOnEnter(self, eaxIn, stack, argc, env); return true;
    case 0x007B6530u: eaxOut = aiDeployOnExit(self, eaxIn, stack, argc, env); return true;
    case 0x007B65B0u: eaxOut = aiUnDeployOnEnter(self, eaxIn, stack, argc, env); return true;
    case 0x007B6670u: eaxOut = aiUnDeployOnExit(self, eaxIn, stack, argc, env); return true;
    case 0x007B66E0u: eaxOut = aiTNGuardMachineXfer(self, eaxIn, stack, argc, env); return true;
    case 0x007B9820u: eaxOut = aiStateMachineSlot18(self, eaxIn, stack, argc, env); return true;
    case 0x007BB4A0u: eaxOut = unitAILandOnExit(self, eaxIn, stack, argc, env); return true;
    case 0x007BB8E0u: eaxOut = unitAIReturnForLandingSlot19(self, eaxIn, stack, argc, env); return true;
    case 0x007BB930u: eaxOut = unitAITakeOffOnExit(self, eaxIn, stack, argc, env); return true;
    case 0x007BC320u: eaxOut = aiFollowExitProductionPathSlot19(self, eaxIn, stack, argc, env); return true;
    case 0x007BC4C0u: eaxOut = aiMoveToAndCollectOnExit(self, eaxIn, stack, argc, env); return true;
    case 0x007BD770u: eaxOut = aiInternalMoveTo31B10Update(self, eaxIn, stack, argc, env); return true;
    case 0x007BD7A0u: eaxOut = aiDockApproachOnExit(self, eaxIn, stack, argc, env); return true;
    case 0x007BD830u: eaxOut = aiInternalMoveTo31B60Update(self, eaxIn, stack, argc, env); return true;
    case 0x007BD880u: eaxOut = aiInternalMoveTo31B60OnExit(self, eaxIn, stack, argc, env); return true;
    case 0x007BD910u: eaxOut = aiInternalMoveTo31BB0Update(self, eaxIn, stack, argc, env); return true;
    case 0x007BDA40u: eaxOut = aiInternalMoveTo31C00Update(self, eaxIn, stack, argc, env); return true;
    case 0x007BDA90u: eaxOut = aiInternalMoveTo31C00OnExit(self, eaxIn, stack, argc, env); return true;
    case 0x007BDEF0u: eaxOut = aiGuardReturnUpdate(self, eaxIn, stack, argc, env); return true;
    default: return false;
    }
}

}  // namespace ra3::gamelogic
