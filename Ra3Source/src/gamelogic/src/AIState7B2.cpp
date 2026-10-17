#include "ra3/gamelogic/AIState7B2.h"

#include <cmath>
#include <cstdint>
#include <cstring>

namespace ra3::gamelogic {
namespace {

constexpr std::uint32_t kFail = 0xFFFFFFFEu;
constexpr std::uint32_t kOperatorNew = 0x00416820u;
constexpr std::uint32_t kFlagPair = 0x006A4F80u;
constexpr std::uint32_t kFlagRefresh = 0x006A4FB0u;
constexpr std::uint32_t kStatus = 0x006D2020u;
constexpr std::uint32_t kFltMax = 0x7F7FFFFFu;
constexpr std::uint32_t kNear = 0x42480000u;
constexpr std::uint32_t kNegOne = 0xBF800000u;
constexpr std::uint32_t kHordeSpeed = 0x497423F0u;

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

std::uint32_t floatToBits(float value)
{
    std::uint32_t bits = 0;
    std::memcpy(&bits, &value, 4);
    return bits;
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

std::uint32_t vcall(AIState7BEnv& env, std::uint32_t obj, std::uint32_t slot, const std::uint32_t* args, int argc)
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

std::uint32_t vcall2(AIState7BEnv& env, std::uint32_t obj, std::uint32_t slot, std::uint32_t arg0,
                     std::uint32_t arg1)
{
    const std::uint32_t args[2] = {arg0, arg1};
    return vcall(env, obj, slot, args, 2);
}

bool bit(std::uint32_t value, unsigned shift)
{
    return ((value >> shift) & 1u) != 0;
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

#include "AIState7B2Lifted.inc"

// 0x007B1190 AIMoveAndTightenState 00C2F4B0 slot 6. Also AIInternalMoveToState 00C2D6D8 slot 6.
std::uint32_t aiMoveAndTightenOnExit(std::uint8_t* self, std::uint32_t, const std::uint32_t*, int,
                                     AIState7B2Env& env)
{
    const auto machine = machineOf(self);
    const auto unit = unitOf(machine);
    auto* unitBytes = asPtr(unit);
    const auto loco = locoOf(unit);
    std::uint32_t eax = machine;
    if (!bit(load32(unitBytes, 0x88), 0x0E) && bit(load32(unitBytes, 0xB0), 0x0F)) {
        store32(unitBytes, 0xB0, load32(unitBytes, 0xB0) & 0xFFFF7FFFu);
        call2(env, kFlagPair, unit, 0x8F, 0);
        eax = call0(env, kFlagRefresh, unit);
    }
    eax = load32(unitBytes, 0xB0) >> 0x1D;
    if ((eax & 1u) != 0) {
        store32(unitBytes, 0xB0, load32(unitBytes, 0xB0) & 0xDFFFFFFFu);
        call2(env, kFlagPair, unit, 0x9D, 0);
        eax = call0(env, kFlagRefresh, unit);
    }
    if (bit(load32(unitBytes, 0xB0), 0x1E)) {
        store32(unitBytes, 0xB0, load32(unitBytes, 0xB0) & 0xBFFFFFFFu);
        call2(env, kFlagPair, unit, 0x9E, 0);
        eax = call0(env, kFlagRefresh, unit);
    }
    if (bit(load32(unitBytes, 0xB0), 0x1F)) {
        store32(unitBytes, 0xB0, load32(unitBytes, 0xB0) & 0x7FFFFFFFu);
        call2(env, kFlagPair, unit, 0x9F, 0);
        eax = call0(env, kFlagRefresh, unit);
    }
    if (env.globalDf03fc != 0) {
        eax = load32(self, 0x44);
        if (eax >= 5) {
            eax = vcall1(env, env.globalDf03fc, 0x6C, eax);
            store32(self, 0x44, 1);
        }
    }
    if (loco != 0) {
        eax = call0(env, 0x00682D70u, loco);
        if (self[0x4F] != 0) {
            store32(asPtr(loco), 0x1F0, kFltMax);
        }
        eax = load32(asPtr(loco), 0x214);
        if (eax != 0 && asPtr(eax)[0xA4] != 0) {
            asPtr(eax)[0xA4] = 0;
        }
    }
    self[0x4F] = 0;
    return eax;
}

// 0x007B5D20 AIAttackState 00C2CCE0 slot 6.
std::uint32_t aiAttackOnExit(std::uint8_t* self, std::uint32_t, const std::uint32_t*, int, AIState7BEnv& env)
{
    const auto held = load32(self, 0x24);
    if (held != 0) {
        vcall1(env, held, 0, 1);
        store32(self, 0x24, 0);
    }
    const auto unit = unitOf(machineOf(self));
    call2(env, kStatus, unit, 0x0E, 0);
    call2(env, kStatus, unit, 0x1B, 0);
    call2(env, kStatus, unit, 0x18, 0);
    call2(env, kStatus, unit, 0x1D, 0);
    call2(env, kStatus, unit, 0x1E, 0);
    auto* unitBytes = asPtr(unit);
    if ((load32(unitBytes, 0xA8) & 1u) != 0) {
        store32(unitBytes, 0xA8, load32(unitBytes, 0xA8) & 0xFFFFFFFEu);
        call2(env, kFlagPair, unit, 0x40, 0);
        call0(env, kFlagRefresh, unit);
    }
    if (bit(load32(unitBytes, 0xA8), 1)) {
        store32(unitBytes, 0xA8, load32(unitBytes, 0xA8) & 0xFFFFFFFDu);
        call2(env, kFlagPair, unit, 0x41, 0);
        call0(env, kFlagRefresh, unit);
    }
    if (bit(load32(unitBytes, 0xA8), 2)) {
        store32(unitBytes, 0xA8, load32(unitBytes, 0xA8) & 0xFFFFFFFBu);
        call2(env, kFlagPair, unit, 0x42, 0);
        call0(env, kFlagRefresh, unit);
    }
    call2(env, kStatus, unit, 0x87, 0);
    auto eax = call2(env, kStatus, unit, 0x8E, 0);
    const auto loco = locoOf(unit);
    if (loco != 0) {
        call1(env, 0x006C0F40u, unit, 0);
        eax = call1(env, 0x00682D90u, loco, 0);
    }
    return eax;
}

// 0x007B0E20 AIMoveOntoWallState 00C2C468 slot 3.
std::uint32_t aiMoveOntoWallXfer(std::uint8_t* self, std::uint32_t, const std::uint32_t* stack, int,
                                 AIState7B2Env& env)
{
    const auto xfer = stack[0];
    env.scratch[0] = 1;
    env.scratch[1] = 1;
    vcall1(env, xfer, 0x18, addressOf(env.scratch));
    env.scratch[4] = load32(self, 0x20) != 0 ? 1 : 0;
    auto eax = vcall1(env, xfer, 0x84, addressOf(env.scratch + 4));
    if (env.scratch[4] != 0) {
        if (load32(self, 0x20) == 0) {
            auto created = call1(env, kOperatorNew, addressOf(self), 0x80);
            if (created != 0) {
                const auto unit = unitOf(machineOf(self));
                const std::uint32_t args[2] = {unit, 0x3D424C5Au};
                created = moveOntoWallMachineCtor(asPtr(created), 0, args, 2, env);
            }
            store32(self, 0x20, created);
        }
        if (env.scratch[4] != 0) {
            const auto machine = load32(self, 0x20);
            if (machine != 0) {
                return vcall1(env, xfer, 0x20, machine);
            }
        }
    }
    const std::uint32_t probe[2] = {0x00C32110u, 1};
    eax = callExternal(env, 0x0040EBF0u, xfer, probe, 2);
    if ((eax & 0xFFu) != 0) {
        return eax;
    }
    eax = vcall0(env, env.globalDe9b1c, 0x68);
    if ((eax & 0xFFu) != 0) {
        return eax;
    }
    const std::uint32_t opened[4] = {0, 0, 0, 0x00BD5550u};
    eax = vcall(env, env.globalDe9b1c, 0x78, opened, 4);
    eax = vcall1(env, eax, 0x3C, 0x00BD61D0u);
    eax = vcall1(env, eax, 0x3C, 0x00C320B0u);
    return vcall2(env, eax, 0x50, 1, 0x00BD61BCu);
}

// 0x007BDDE0 AIGroup 00C33664 slot 3.
std::uint32_t aiGroupXfer(std::uint8_t* self, std::uint32_t, const std::uint32_t* stack, int, AIState7BEnv& env)
{
    const auto xfer = stack[0];
    env.scratch[0] = 1;
    env.scratch[1] = 1;
    vcall1(env, xfer, 0x18, addressOf(env.scratch));
    auto eax = load32(asPtr(xfer), 4) >> 2;
    if ((eax & 1u) == 0) {
        return eax;
    }
    const auto sentinel = addressOf(self) + 4;
    for (auto node = load32(self, 4); node != sentinel; node = load32(asPtr(node), 0)) {
        const auto payload = load32(asPtr(node), 8);
        store32(env.scratch, 8, payload == 0 ? 0 : load32(asPtr(payload), 0xDC));
        const std::uint32_t pair[2] = {xfer, addressOf(env.scratch + 8)};
        callExternal(env, 0x0054A090u, addressOf(self), pair, 2);
    }
    std::uint32_t count = 0;
    for (auto node = load32(self, 4); node != sentinel; node = load32(asPtr(node), 0)) {
        count += 1;
    }
    store32(env.scratch, 12, count);
    vcall1(env, xfer, 0x6C, addressOf(env.scratch + 12));
    vcall1(env, xfer, 0x64, addressOf(self) + 0x10);
    vcall1(env, xfer, 0x84, addressOf(self) + 0x14);
    vcall1(env, xfer, 0x6C, addressOf(self) + 0x18);
    vcall1(env, xfer, 0x58, addressOf(self) + 0x1C);
    return vcall1(env, xfer, 0x58, addressOf(self) + 0x28);
}

// 0x007BD980 AIInternalMoveToState 00C31BB0 slot 6.
std::uint32_t aiInternalMoveTo31BB0OnExit(std::uint8_t* self, std::uint32_t, const std::uint32_t* stack, int,
                                          AIState7BEnv& env)
{
    const auto reason = stack[0];
    const auto goal = call0(env, 0x005B9F30u, machineOf(self));
    if (goal != 0 && (asPtr(goal)[0x80] & 1u) == 0 && (asPtr(goal)[0x471] & 1u) == 0) {
        const auto dock = call0(env, 0x00687810u, goal);
        if (dock != 0) {
            if (reason != 1 && (vcall0(env, dock, 0x38) & 0xFFu) != 0) {
                vcall1(env, dock, 0x28, unitOf(machineOf(self)));
            } else {
                vcall1(env, dock, 0x34, unitOf(machineOf(self)));
            }
        }
    }
    call0(env, 0x005AA250u, machineOf(self));
    call1(env, 0x006F0A20u, unitOf(machineOf(self)), 3);
    const auto node = call0(env, 0x00686CB0u, unitOf(machineOf(self)));
    if (node != 0) {
        asPtr(node)[0xA4] = 0;
    }
    const auto loco = locoOf(unitOf(machineOf(self)));
    if (loco != 0) {
        call1(env, 0x006E6360u, loco, 0);
    }
    return aiMoveAndTightenOnExit(self, 0, stack, 1, static_cast<AIState7B2Env&>(env));
}

// 0x007B1090 AIDeadState 00C2C578 slot 4.
std::uint32_t aiDeadOnEnter(std::uint8_t* self, std::uint32_t, const std::uint32_t*, int, AIState7B2Env& env)
{
    const auto unit = unitOf(machineOf(self));
    if (unit == 0) {
        return 0;
    }
    const auto first = addressOf(env.scratch);
    const std::uint32_t clearFirst[3] = {first, 0, 0x3C};
    callExternal(env, 0x004D2F98u, first, clearFirst, 3);
    for (std::uint32_t index = 0; index < 6; ++index) {
        const std::uint32_t query[2] = {addressOf(env.scratch + 0x40), index};
        const auto found = callExternal(env, 0x006C3500u, first, query, 2);
        call1(env, 0x006888A0u, first, found);
    }
    store32(env.scratch, 0x10, load32(env.scratch, 0x10) | 0x8000u);
    store32(env.scratch, 8, load32(env.scratch, 8) | 8u);
    const std::uint32_t clearSecond[3] = {addressOf(env.scratch + 0x40), 0, 0x3C};
    callExternal(env, 0x004D2F98u, first, clearSecond, 3);
    store32(env.scratch, 0x58, load32(env.scratch, 0x58) | 0x10000u);
    call2(env, 0x006A5060u, unit, addressOf(env.scratch), addressOf(env.scratch + 0x40));
    call0(env, 0x00804300u, env.globalDfc058);
    const auto templ = load32(asPtr(unit), 4);
    if (bit(load32(asPtr(templ), 0xC0), 0x0A)) {
        const auto found = call1(env, 0x00B7AF40u, env.globalDf6ec0, unit);
        call0(env, 0x00654A20u, found);
    }
    return 0;
}

// 0x007BC050 AttackSquadStateMachine 00C2E440 slot 3.
std::uint32_t attackSquadMachineXfer(std::uint8_t* self, std::uint32_t, const std::uint32_t* stack, int,
                                     AIState7BEnv& env)
{
    const auto xfer = stack[0];
    env.scratch[0] = 1;
    env.scratch[1] = 2;
    vcall1(env, xfer, 0x18, addressOf(env.scratch));
    auto eax = call1(env, 0x005C0A40u, addressOf(self), xfer);
    if (env.scratch[1] < 2) {
        return eax;
    }
    env.scratch[4] = load32(self, 0x80) != 0 ? 1 : 0;
    eax = vcall1(env, xfer, 0x84, addressOf(env.scratch + 4));
    if (bit(load32(asPtr(xfer), 4), 1)) {
        if (env.scratch[4] == 0) {
            return eax;
        }
        if (load32(self, 0x80) == 0) {
            auto created = call1(env, kOperatorNew, addressOf(self), 0x30);
            if (created != 0) {
                created = squadCtor(asPtr(created), 0, nullptr, 0, env);
            }
            store32(self, 0x80, created);
        }
    }
    if (env.scratch[4] == 0) {
        return eax;
    }
    const auto squad = load32(self, 0x80);
    if (squad == 0) {
        return eax;
    }
    return vcall1(env, xfer, 0x20, squad);
}

// 0x007B36B0 AIAttackMeleeHordeApproachTargetState 00C2F800 slot 6.
std::uint32_t aiAttackMeleeHordeApproachOnExit(std::uint8_t* self, std::uint32_t eax, const std::uint32_t* stack,
                                               int argc, AIState7B2Env& env)
{
    eax = aiMoveAndTightenOnExit(self, eax, stack, argc, env);
    self[0x61] = 0;
    const auto unit = unitOf(machineOf(self));
    const auto loco = locoOf(unit);
    if ((asPtr(unit)[0x80] & 1u) != 0) {
        return eax;
    }
    eax = call2(env, 0x006D5570u, unit, 0x4E, 0);
    if (loco == 0) {
        return eax;
    }
    eax = call1(env, 0x00682CE0u, loco, kHordeSpeed);
    eax = call1(env, 0x006E6360u, loco, 0);
    eax = vcall0(env, loco, 0x274);
    if ((eax & 0xFFu) == 0) {
        return eax;
    }
    const float dx = bitsToFloat(load32(self, 0x20)) - bitsToFloat(load32(asPtr(unit), 0x38));
    const float dy = bitsToFloat(load32(self, 0x24)) - bitsToFloat(load32(asPtr(unit), 0x3C));
    if (!(bitsToFloat(kNear) > dx * dx + dy * dy)) {
        return eax;
    }
    return call1(env, 0x0078D1E0u, unit, addressOf(self) + 0x20);
}

// 0x007B4820 AIFollowPathState 00C2FB10 slot 6.
std::uint32_t aiFollowPathOnExit(std::uint8_t* self, std::uint32_t eax, const std::uint32_t* stack, int argc,
                                 AIState7B2Env& env)
{
    eax = aiMoveAndTightenOnExit(self, eax, stack, argc, env);
    const auto unit = unitOf(machineOf(self));
    const auto loco = locoOf(unit);
    if (loco == 0) {
        return eax;
    }
    asPtr(loco)[0x434] = 0;
    eax = call1(env, 0x00683140u, loco, 0);
    const auto path = load32(asPtr(loco), 0x214);
    if (path != 0) {
        store32(asPtr(path), 0x40, load32(asPtr(path), 0x40) & 0xFFFFFFF7u);
    }
    store32(asPtr(loco), 0x1E4, 0xFFFFFFFFu);
    if (!bit(load32(asPtr(load32(asPtr(unit), 4)), 0xCC), 0x0A)) {
        eax = call2(env, kStatus, unit, 0x95, 0);
    }
    const auto node = call0(env, 0x00686CB0u, unit);
    if (node == 0) {
        return node;
    }
    const auto again = call1(env, 0x00686CB0u, unit, kNegOne);
    return call1(env, 0x006A4500u, again, kNegOne);
}

// 0x007B47B0 AIAttackMeleeEngageState 00C2FA38 slot 6.
std::uint32_t aiAttackMeleeEngageOnExit(std::uint8_t* self, std::uint32_t eax, const std::uint32_t* stack,
                                        int argc, AIState7B2Env& env)
{
    const auto machine = machineOf(self);
    const auto unit = unitOf(machine);
    if ((asPtr(unit)[0x80] & 1u) != 0) {
        return machine;
    }
    const auto loco = locoOf(unit);
    if (loco != 0) {
        call1(env, 0x00682CE0u, loco, kHordeSpeed);
    }
    if (!bit(load32(asPtr(unit), 0x84), 8)) {
        call2(env, 0x006D5570u, unit, 0x4E, 0);
        call2(env, kStatus, unit, 0x4E, 0);
    }
    return aiMoveAndTightenOnExit(self, eax, stack, argc, env);
}

// 0x007B4BD0 AIFollowPathAsTeamState 00C2FB78 slot 6.
std::uint32_t aiFollowPathAsTeamOnExit(std::uint8_t* self, std::uint32_t eax, const std::uint32_t* stack,
                                       int argc, AIState7B2Env& env)
{
    const auto held = load32(self, 0x60);
    if (held != 0) {
        vcall1(env, held, 0x1C, 0);
    }
    eax = aiMoveAndTightenOnExit(self, eax, stack, argc, env);
    const auto loco = locoOf(unitOf(machineOf(self)));
    if (loco == 0) {
        return eax;
    }
    asPtr(loco)[0x434] = 0;
    const auto path = load32(asPtr(loco), 0x214);
    if (path != 0) {
        store32(asPtr(path), 0x40, load32(asPtr(path), 0x40) & 0xFFFFFFF7u);
    }
    eax = call1(env, 0x00682CE0u, loco, kHordeSpeed);
    store32(asPtr(loco), 0x1E4, 0xFFFFFFFFu);
    return eax;
}

// 0x007B4E30 AIMoveAndEvacuateState 00C2FBE0 slot 6.
std::uint32_t aiMoveAndEvacuateOnExit(std::uint8_t* self, std::uint32_t, const std::uint32_t* stack, int,
                                      AIState7B2Env& env)
{
    call0(env, 0x005AA250u, machineOf(self));
    call2(env, 0x005AC4F0u, machineOf(self), addressOf(self) + 0x50, kFltMax);
    return aiMoveAndTightenOnExit(self, 0, stack, 1, env);
}

// 0x007BDEC0 AIGuardMoveToAttack 00C31CA0 slot 6.
std::uint32_t aiGuardMoveToAttackOnExit(std::uint8_t* self, std::uint32_t, const std::uint32_t* stack, int,
                                        AIState7B2Env& env)
{
    aiMoveAndTightenOnExit(self, 0, stack, 1, env);
    return call2(env, 0x005AC4F0u, machineOf(self), addressOf(self) + 0x50, load32(self, 0x5C));
}

// 0x007B0F30 AIMoveOntoWallState 00C2C468 slot 4.
std::uint32_t aiMoveOntoWallOnEnter(std::uint8_t* self, std::uint32_t, const std::uint32_t*, int, AIState7BEnv& env)
{
    auto created = call1(env, kOperatorNew, addressOf(self), 0x80);
    if (created != 0) {
        const auto unit = unitOf(machineOf(self));
        const std::uint32_t args[2] = {unit, 0xB4CC9815u};
        created = moveOntoWallMachineCtor(asPtr(created), 0, args, 2, env);
    }
    store32(self, 0x20, created);
    const auto goal = call0(env, 0x005B9F30u, machineOf(self));
    // MoveOntoWallStateMachine 00C2C400 slot 0x38 and slot 0x14. The table lives in the
    // game image, so the offline checker calls the slots directly.
    call1(env, 0x005AC4C0u, created, goal);
    call2(env, 0x005AC4F0u, created, machineOf(self) + 0x2C, kFltMax);
    call0(env, 0x005B25C0u, created);
    return 0;
}

// 0x007B6380 AIMoveForBoarding 00C30310 slot 5.
std::uint32_t aiMoveForBoardingUpdate(std::uint8_t* self, std::uint32_t, const std::uint32_t*, int, AIState7BEnv& env)
{
    const auto prior = call0(env, 0x007B1810u, addressOf(self));
    const auto unit = unitOf(machineOf(self));
    const auto loco = locoOf(unit);
    if (load32(asPtr(loco), 0x150) == 0) {
        return prior;
    }
    const auto body = load32(asPtr(loco), 0x150);
    const std::uint32_t sample[2] = {addressOf(env.scratch), load32(asPtr(unit), 0x120)};
    callExternal(env, 0x0063BF20u, body, sample, 2);
    const std::uint32_t query[5] = {addressOf(env.scratch), 0, 0, 1, 1};
    if ((callExternal(env, 0x00639FB0u, body, query, 5) & 0xFFu) == 0) {
        return 0xFFFFFFFFu;
    }
    return prior;
}

// 0x007BD6B0 AIDockApproachState 00C31AA8 slot 5.
std::uint32_t aiDockApproachUpdate(std::uint8_t* self, std::uint32_t, const std::uint32_t*, int, AIState7BEnv& env)
{
    const auto goal = call0(env, 0x005B9F30u, machineOf(self));
    if (goal == 0 || (asPtr(goal)[0x80] & 1u) != 0 || (asPtr(goal)[0x471] & 1u) != 0) {
        return kFail;
    }
    const auto status = call0(env, 0x007B12D0u, addressOf(self));
    if (status != 0xFFFFFFFFu) {
        return status;
    }
    const auto unit = unitOf(machineOf(self));
    if (unit == 0) {
        return status;
    }
    const auto again = call0(env, 0x005B9F30u, machineOf(self));
    const auto dock = call0(env, 0x00687810u, again);
    if (dock == 0) {
        return status;
    }
    const std::uint32_t args[2] = {unit, addressOf(env.scratch)};
    vcall(env, dock, 0x14, args, 2);
    const float dx = bitsToFloat(load32(asPtr(unit), 0x38)) - bitsToFloat(load32(env.scratch, 0));
    const float dy = bitsToFloat(load32(asPtr(unit), 0x3C)) - bitsToFloat(load32(env.scratch, 4));
    const float distance = std::sqrt(dx * dx + dy * dy);
    if (distance > bitsToFloat(kNear)) {
        return status - 1;
    }
    return status;
}

std::uint32_t addedFloat(std::uint32_t left, std::uint32_t right)
{
    return floatToBits(bitsToFloat(left) + bitsToFloat(right));
}

// 0x007BBBD0 TurretAIIdleScanState 00C29B80 slot 5.
std::uint32_t turretAIIdleScanUpdate(std::uint8_t* self, std::uint32_t, const std::uint32_t*, int, AIState7BEnv& env)
{
    const auto turret = load32(asPtr(machineOf(self)), 0x80);
    if (turret == 0) {
        return kFail;
    }
    if (bit(load32(asPtr(load32(asPtr(turret), 0x5C)), 0x88), 9)) {
        return 0xFFFFFFFFu;
    }
    const auto info = load32(asPtr(turret), 0x58);
    const auto angle = load32(asPtr(info), 0x30);
    const auto extra = load32(asPtr(turret), 0x98);
    const auto height = extra == 0 ? load32(asPtr(info), 0x4C)
                                   : addedFloat(load32(asPtr(info), 0x4C), load32(asPtr(extra), 0x64));
    const auto aim = addedFloat(load32(self, 0x20), height);
    const std::uint32_t first[3] = {aim, angle, 0};
    const auto primary = callExternal(env, 0x007B6C60u, turret, first, 3);
    const std::uint32_t second[2] = {0, angle};
    const auto secondary = callExternal(env, 0x0079BB20u, turret, second, 2);
    if ((primary & 0xFFu) != 0 && (secondary & 0xFFu) != 0) {
        return 0xFFFFFFFFu;
    }
    return 0;
}

// 0x007BBC80 TurretAIRecenterTurretState 00C29BE8 slot 5.
std::uint32_t turretAIRecenterUpdate(std::uint8_t* self, std::uint32_t, const std::uint32_t*, int, AIState7BEnv& env)
{
    const auto turret = load32(asPtr(machineOf(self)), 0x80);
    const auto angle = self[0x20] != 0 ? load32(asPtr(load32(asPtr(turret), 0x58)), 0x30) : 0x3F800000u;
    const auto info = load32(asPtr(turret), 0x58);
    const auto extra = load32(asPtr(turret), 0x98);
    const auto height = extra == 0 ? load32(asPtr(info), 0x4C)
                                   : addedFloat(load32(asPtr(info), 0x4C), load32(asPtr(extra), 0x64));
    const std::uint32_t first[3] = {height, angle, 0};
    const auto primary = callExternal(env, 0x007B6C60u, turret, first, 3);
    const std::uint32_t second[2] = {0, angle};
    const auto secondary = callExternal(env, 0x0079BB20u, turret, second, 2);
    if ((primary & 0xFFu) != 0 && (secondary & 0xFFu) != 0) {
        return 0xFFFFFFFFu;
    }
    return 0;
}

// 0x007BEE80 AISkirmishPlayer 00C33718 slot 12.
std::uint32_t aiSkirmishPlayerSlot12(std::uint8_t* self, std::uint32_t, const std::uint32_t*, int, AIState7B2Env& env)
{
    const auto logic = env.gameLogic;
    const auto frame = load32(asPtr(logic), 0x58);
    if (frame >= load32(self, 0xC0)) {
        const float scaled = bitsToFloat(load32(asPtr(logic), 0x1C4)) * bitsToFloat(0x00000000u) *
                             bitsToFloat(0x40A00000u);
        const auto truncated = static_cast<std::int32_t>(scaled);
        store32(self, 0xC0, static_cast<std::uint32_t>(truncated) + frame);
        call0(env, 0x007BEBD0u, addressOf(self));
    }
    return load32(self, 0xC4);
}

bool dispatch(std::uint32_t va, std::uint8_t* self, std::uint32_t eaxIn, const std::uint32_t* stack, int argc,
              AIState7B2Env& env, std::uint32_t& eaxOut)
{
    switch (va) {
    case 0x007B1190u: eaxOut = aiMoveAndTightenOnExit(self, eaxIn, stack, argc, env); return true;
    case 0x007B5D20u: eaxOut = aiAttackOnExit(self, eaxIn, stack, argc, env); return true;
    case 0x007B0E20u: eaxOut = aiMoveOntoWallXfer(self, eaxIn, stack, argc, env); return true;
    case 0x007BDDE0u: eaxOut = aiGroupXfer(self, eaxIn, stack, argc, env); return true;
    case 0x007BD980u: eaxOut = aiInternalMoveTo31BB0OnExit(self, eaxIn, stack, argc, env); return true;
    case 0x007B1090u: eaxOut = aiDeadOnEnter(self, eaxIn, stack, argc, env); return true;
    case 0x007BC050u: eaxOut = attackSquadMachineXfer(self, eaxIn, stack, argc, env); return true;
    case 0x007B36B0u: eaxOut = aiAttackMeleeHordeApproachOnExit(self, eaxIn, stack, argc, env); return true;
    case 0x007B4820u: eaxOut = aiFollowPathOnExit(self, eaxIn, stack, argc, env); return true;
    case 0x007B47B0u: eaxOut = aiAttackMeleeEngageOnExit(self, eaxIn, stack, argc, env); return true;
    case 0x007B4BD0u: eaxOut = aiFollowPathAsTeamOnExit(self, eaxIn, stack, argc, env); return true;
    case 0x007B4E30u: eaxOut = aiMoveAndEvacuateOnExit(self, eaxIn, stack, argc, env); return true;
    case 0x007BDEC0u: eaxOut = aiGuardMoveToAttackOnExit(self, eaxIn, stack, argc, env); return true;
    case 0x007B0F30u: eaxOut = aiMoveOntoWallOnEnter(self, eaxIn, stack, argc, env); return true;
    case 0x007B6380u: eaxOut = aiMoveForBoardingUpdate(self, eaxIn, stack, argc, env); return true;
    case 0x007BD6B0u: eaxOut = aiDockApproachUpdate(self, eaxIn, stack, argc, env); return true;
    case 0x007BBBD0u: eaxOut = turretAIIdleScanUpdate(self, eaxIn, stack, argc, env); return true;
    case 0x007BBC80u: eaxOut = turretAIRecenterUpdate(self, eaxIn, stack, argc, env); return true;
    case 0x007BEE80u: eaxOut = aiSkirmishPlayerSlot12(self, eaxIn, stack, argc, env); return true;
    case 0x007B0D50u: eaxOut = moveOntoWallMachineCtor(self, eaxIn, stack, argc, env); return true;
    case 0x007B6840u: eaxOut = squadCtor(self, eaxIn, stack, argc, env); return true;
    case 0x007B9710u: eaxOut = aiStateMachine30530Slot18(self, eaxIn, stack, argc, env); return true;
    case 0x007B8E70u: eaxOut = aiStateMachine30480Slot18(self, eaxIn, stack, argc, env); return true;
    case 0x007B98D0u: eaxOut = aiStateMachine30638Slot18(self, eaxIn, stack, argc, env); return true;
    case 0x007BA190u: eaxOut = disguiseAggressiveMachineCtor(self, eaxIn, stack, argc, env); return true;
    case 0x007BA260u: eaxOut = heliEvacuateMachineCtor(self, eaxIn, stack, argc, env); return true;
    case 0x007BA7F0u: eaxOut = jetAttackMoveFollowPathMachineCtor(self, eaxIn, stack, argc, env); return true;
    case 0x007BA970u: eaxOut = jetAttackMoveFollowWaypointMachineCtor(self, eaxIn, stack, argc, env); return true;
    case 0x007BAB20u: eaxOut = jetAttackMoveToGoalMachineCtor(self, eaxIn, stack, argc, env); return true;
    case 0x007BB2B0u: eaxOut = jetMoveToGoalMachineCtor(self, eaxIn, stack, argc, env); return true;
    case 0x007BB500u: eaxOut = moveToRallyPointMachineCtor(self, eaxIn, stack, argc, env); return true;
    case 0x007BBD20u: eaxOut = attackMoveFollowPathMachineCtor(self, eaxIn, stack, argc, env); return true;
    case 0x007BBE40u: eaxOut = attackMoveFollowWaypointMachineCtor(self, eaxIn, stack, argc, env); return true;
    case 0x007BBF30u: eaxOut = attackMoveToGoalMachineCtor(self, eaxIn, stack, argc, env); return true;
    case 0x007BC100u: eaxOut = combinedUnitMachineCtor(self, eaxIn, stack, argc, env); return true;
    case 0x007BA230u: eaxOut = unitAIDisguiseAggressiveSlot19(self, eaxIn, stack, argc, env); return true;
    case 0x007BA3E0u: eaxOut = unitAIHeliEvacuateSlot19(self, eaxIn, stack, argc, env); return true;
    case 0x007BA940u: eaxOut = jetAIAttackMoveFollowPathSlot19(self, eaxIn, stack, argc, env); return true;
    case 0x007BAAF0u: eaxOut = jetAIAttackMoveFollowWaypointSlot19(self, eaxIn, stack, argc, env); return true;
    case 0x007BAC70u: eaxOut = jetAIAttackMoveToGoalSlot19(self, eaxIn, stack, argc, env); return true;
    case 0x007BB470u: eaxOut = jetAIMoveToGoalSlot19(self, eaxIn, stack, argc, env); return true;
    case 0x007BB610u: eaxOut = unitAIMoveToRallyPointSlot19(self, eaxIn, stack, argc, env); return true;
    case 0x007BBE10u: eaxOut = unitAIAttackMoveFollowPathSlot19(self, eaxIn, stack, argc, env); return true;
    case 0x007BBF00u: eaxOut = unitAIAttackMoveFollowWaypointSlot19(self, eaxIn, stack, argc, env); return true;
    case 0x007BC020u: eaxOut = unitAIAttackMoveToGoalSlot19(self, eaxIn, stack, argc, env); return true;
    case 0x007BC1C0u: eaxOut = unitAICombinedUnitSlot19(self, eaxIn, stack, argc, env); return true;
    default: return false;
    }
}

}  // namespace

bool runAIState7B2(std::uint32_t va, std::uint8_t* self, std::uint32_t eaxIn, const std::uint32_t* stack, int argc,
                   AIState7B2Env& env, std::uint32_t& eaxOut)
{
    return dispatch(va, self, eaxIn, stack, argc, env, eaxOut);
}

}  // namespace ra3::gamelogic
