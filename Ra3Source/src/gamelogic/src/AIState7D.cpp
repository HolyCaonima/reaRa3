#include "ra3/gamelogic/AIState7D.h"

#include <cmath>
#include <cstring>
#include <initializer_list>

namespace ra3::gamelogic {
namespace {

constexpr std::uint32_t kSuccess = 0xFFFFFFFFu;
constexpr std::uint32_t kFailure = 0xFFFFFFFEu;
constexpr std::uint32_t kFltMax = 0x7F7FFFFFu;
constexpr std::uint32_t kLimit400 = 0x43C80000u;
constexpr std::uint32_t kAngleLimit = 0x3D0F5C29u;
constexpr std::uint32_t kNineTenths = 0x3F666666u;
constexpr std::uint32_t kFive = 0x40A00000u;

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

std::uint16_t load16(const std::uint8_t* base, std::uint32_t offset)
{
    std::uint16_t value = 0;
    std::memcpy(&value, base + offset, 2);
    return value;
}

void store16(std::uint8_t* base, std::uint32_t offset, std::uint16_t value)
{
    std::memcpy(base + offset, &value, 2);
}

std::uint8_t load8(const std::uint8_t* base, std::uint32_t offset)
{
    return base[offset];
}

void store8(std::uint8_t* base, std::uint32_t offset, std::uint8_t value)
{
    base[offset] = value;
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

// ucomiss x, 0 is ordered-equal, including -0. NaN is not equal.
bool sseEqualZero(std::uint32_t bits)
{
    return bitsToFloat(bits) == 0.f;
}

std::uint32_t invoke(AIState7DEnv& env, std::uint32_t target, std::uint32_t ecx,
                     std::initializer_list<std::uint32_t> args)
{
    std::uint32_t buffer[8] = {};
    int count = 0;
    for (std::uint32_t arg : args) {
        buffer[count++] = arg;
    }
    return env.call(env.callCtx, target, ecx, buffer, count);
}

std::uint32_t virtualCall(AIState7DEnv& env, std::uint32_t object, std::uint32_t slot,
                          std::initializer_list<std::uint32_t> args)
{
    const std::uint32_t table = load32(asPtr(object), 0);
    const std::uint32_t function = load32(asPtr(table), slot);
    return invoke(env, function, object, args);
}

void copy3(std::uint8_t* dst, std::uint32_t dstOff, std::uint32_t src)
{
    store32(dst, dstOff, load32(asPtr(src), 0));
    store32(dst, dstOff + 4, load32(asPtr(src), 4));
    store32(dst, dstOff + 8, load32(asPtr(src), 8));
}

std::uint32_t chopTowardZero(long double value)
{
    if (!(value < 2147483648.0L) || value < -2147483648.0L) {
        return 0x80000000u;
    }
    return static_cast<std::uint32_t>(static_cast<std::int32_t>(value));
}

std::uint32_t doubledChop(long double value)
{
    return chopTowardZero(value + value);
}

// 007D0AD0 UnitAILocateNodeState slots 4 and 5. Continue when X and Z are both 0.
std::uint32_t unitAILocateNodeConsider(std::uint8_t* self, AIState7DEnv& env)
{
    const std::uint32_t unit = unitOf(machineOf(self));
    const std::uint32_t x = load32(asPtr(unit), 0x38);
    const std::uint32_t z = load32(asPtr(unit), 0x40);
    if (sseEqualZero(x) && sseEqualZero(z)) {
        return 0;
    }
    if ((invoke(env, 0x007C7EF0u, addressOf(self), {}) & 0xFFu) != 0) {
        return kSuccess;
    }
    const std::uint32_t status = invoke(env, 0x007BB9C0u, addressOf(self), {});
    if (status == 0) {
        return kSuccess;
    }
    if (status == 2) {
        return kFailure;
    }
    return 0;
}

// 007D6050 TurretAIAimTurretState slot 19. AL is replaced; the call's high bits stay.
std::uint32_t turretAIAimSlot19(std::uint8_t* self, AIState7DEnv& env)
{
    env.scratch[0] = 0;
    env.scratch[1] = 0;
    const std::uint32_t turret = load32(asPtr(machineOf(self)), 0x80);
    const std::uint32_t result = invoke(env, 0x007D3070u, turret,
                                        {addressOf(&env.scratch[0]), addressOf(&env.scratch[1])});
    const std::uint8_t bit = (env.scratch[0] != 0 && env.scratch[1] != 0) ? 1u : 0u;
    return (result & 0xFFFFFF00u) | bit;
}

// 007D6230 TurretAIFireWeaponState slot 5. A nonzero probe byte continues; zero fails.
std::uint32_t turretAIFireWeaponUpdate(std::uint8_t* self, AIState7DEnv& env)
{
    const std::uint32_t weapon = load32(self, 0x20);
    if (load32(asPtr(weapon), 0x78) == 0) {
        return kSuccess;
    }
    env.scratch[0] = 0;
    env.scratch[1] = 0;
    const std::uint32_t probed = invoke(env, 0x007D3070u, weapon,
                                        {addressOf(&env.scratch[0]), addressOf(&env.scratch[1])});
    return (probed & 0xFFu) != 0 ? 0u : kFailure;
}

// 007DC160 AIPrepareForBoarding slot 5.
std::uint32_t aiPrepareForBoardingUpdate(std::uint8_t* self, AIState7DEnv& env)
{
    const std::uint32_t unit = unitOf(machineOf(self));
    const std::uint32_t ready = invoke(env, 0x0063EB60u, addressOf(&env.scratch[0]),
                                       {unit + 0x38, unit, addressOf(&env.scratch[0])});
    if ((ready & 0xFFu) == 0) {
        return kSuccess;
    }
    invoke(env, 0x0054F490u, locoOf(unit) + 0x24, {2u});
    return kFailure;
}

// 007DB660 AIMoveAndEvacuateState slot 5.
std::uint32_t aiMoveAndEvacuateUpdate(std::uint8_t* self, AIState7DEnv& env)
{
    const std::uint32_t unit = unitOf(machineOf(self));
    if ((load8(asPtr(unit), 0x471) & 1u) != 0) {
        return kFailure;
    }
    const std::uint32_t status = invoke(env, 0x007B12D0u, addressOf(self), {});
    if (status == 0) {
        return 0;
    }
    if ((load8(asPtr(unit), 0x471) & 1u) != 0) {
        return kFailure;
    }
    invoke(env, 0x006C5BE0u, locoOf(unit) + 0x24, {0u, 2u});
    const std::uint32_t contain = invoke(env, 0x00687C90u, unit, {});
    invoke(env, 0x007E57C0u, contain, {});
    return status;
}

// 007D19B0 AIAttackAimAtTargetState slot 5. Forwards with a zero argument.
std::uint32_t aiAttackAimAtTargetUpdate(std::uint8_t* self, AIState7DEnv& env)
{
    return invoke(env, 0x007CD890u, addressOf(self), {0u});
}

// 007DABC0 AIPlayer / AISkirmishPlayer slot 26.
std::uint32_t aiPlayerSlot26(std::uint8_t* self, std::uint32_t eax, AIState7DEnv& env)
{
    (void)eax;
    const std::uint32_t ready = virtualCall(env, addressOf(self), 0x58, {});
    if ((ready & 0xFFu) == 0) {
        return ready;
    }
    return invoke(env, 0x007DA110u, addressOf(self), {});
}

// 007DAD30 AIPlayer / AISkirmishPlayer slot 5. Four virtuals, then the build tail.
std::uint32_t aiPlayerSlot5(std::uint8_t* self, AIState7DEnv& env)
{
    const std::uint32_t selfAddr = addressOf(self);
    virtualCall(env, selfAddr, 0x40, {});
    virtualCall(env, selfAddr, 0x44, {});
    virtualCall(env, selfAddr, 0x48, {});
    virtualCall(env, selfAddr, 0x4C, {});
    return invoke(env, 0x007D99A0u, selfAddr, {});
}

// 007DC1B0 AITNGuardReturnState slot 5.
std::uint32_t aiTNGuardReturnUpdate(std::uint8_t* self, AIState7DEnv& env)
{
    const std::uint32_t machine = machineOf(self);
    const std::uint32_t unit = unitOf(machine);
    const std::uint32_t guard = invoke(env, 0x00686FA0u, unit, {});
    std::uint32_t contain = invoke(env, 0x00687C90u, unit, {});
    if (contain != 0) {
        contain = invoke(env, 0x00687C90u, unit, {});
        const std::uint32_t tunnel = invoke(env, 0x007F0710u, contain, {});
        if (tunnel != 0) {
            store32(asPtr(machine), 0x8C, load32(asPtr(tunnel), 0xDC));
            return kFailure;
        }
    }
    if (guard != 0) {
        const std::uint32_t link = load32(asPtr(guard), 0xA4);
        if (link != 0 && invoke(env, 0x007F1050u, link, {}) != 0) {
            store32(asPtr(machine), 0x8C, load32(asPtr(guard), 0xDC));
            return kFailure;
        }
    }
    const std::uint32_t entered = invoke(env, 0x007DB740u, addressOf(self), {});
    return entered == 0 ? 0u : kSuccess;
}

// 007D0B30 AIDockProcessDockState slot 5.
std::uint32_t aiDockProcessDockUpdate(std::uint8_t* self, AIState7DEnv& env)
{
    const std::uint32_t machine = machineOf(self);
    const std::uint32_t goal = invoke(env, 0x005B9F30u, machine, {});
    if (goal == 0 || (load8(asPtr(goal), 0x80) & 1u) != 0 || (load8(asPtr(goal), 0x471) & 1u) != 0) {
        return kFailure;
    }
    const std::uint32_t dock = invoke(env, 0x00687810u, goal, {});
    if (dock == 0) {
        return kFailure;
    }
    if ((load8(asPtr(goal), 0x471) & 1u) == 0 || ((load32(asPtr(goal), 0x80) >> 21) & 1u) != 0) {
        const std::uint32_t unit = unitOf(machine);
        const std::uint32_t left = invoke(env, 0x00686FA0u, unit, {});
        const std::uint32_t right = invoke(env, 0x00686FA0u, goal, {});
        if (left != right) {
            return kFailure;
        }
        const std::uint32_t flags = load32(asPtr(goal), 0x26C);
        if (((flags >> 2) & 1u) != 0 || ((flags >> 6) & 1u) != 0) {
            return kFailure;
        }
        const std::uint32_t frame = load32(asPtr(env.gameLogic), 0x58);
        if (frame < load32(self, 0x20)) {
            return 0;
        }
        invoke(env, 0x00798D10u, addressOf(self), {});
        const std::uint32_t produced = invoke(env, 0x007CA0D0u, addressOf(self), {});
        if ((virtualCall(env, dock, 0x38, {}) & 0xFFu) != 0 &&
            (virtualCall(env, dock, 0x30, {unit, produced}) & 0xFFu) != 0) {
            return 0;
        }
        if (load32(asPtr(env.gameLogic), 0x58) < load32(self, 0x28)) {
            return 0;
        }
        return kSuccess;
    }
    invoke(env, 0x007483A0u, unitOf(machine), {6u, 1u, 0u});
    return kFailure;
}

// 007DB6C0 AIFollowWaypointPathStateAndEvacuate slot 5.
std::uint32_t aiFollowWaypointEvacuateUpdate(std::uint8_t* self, AIState7DEnv& env)
{
    std::uint32_t status = invoke(env, 0x007CD460u, addressOf(self), {});
    const std::uint32_t unit = unitOf(machineOf(self));
    const std::uint32_t loco = locoOf(unit);
    if (load32(asPtr(loco), 0x150) != 0) {
        const std::uint32_t radius = load32(asPtr(unit), 0x120);
        const std::uint32_t blocked = invoke(env, 0x0063BF20u, load32(asPtr(loco), 0x150),
                                             {addressOf(&env.scratch[0]), radius});
        (void)blocked;
        const std::uint32_t holder = load32(asPtr(loco), 0x150);
        const std::uint32_t clear = invoke(env, 0x00639FB0u, holder,
                                           {addressOf(&env.scratch[0]), 0u, 0u, 1u, 1u});
        if ((clear & 0xFFu) == 0) {
            status = kSuccess;
        }
    }
    if (status == kSuccess) {
        const std::uint32_t ecx = load32(asPtr(loco), 0x150) != 0 ? load32(asPtr(loco), 0x150)
                                                                  : addressOf(self);
        invoke(env, 0x007DAD60u, ecx, {unit, machineOf(self), 0u});
    }
    return status;
}

// 007DBF70 AIMoveToAndEvacuateState / AIAttackMoveToAndEvacuateState slot 5.
std::uint32_t aiMoveToAndEvacuateUpdate(std::uint8_t* self, AIState7DEnv& env)
{
    std::uint32_t status = invoke(env, 0x007B1810u, addressOf(self), {});
    const std::uint32_t unit = unitOf(machineOf(self));
    const std::uint32_t loco = locoOf(unit);
    if (load32(asPtr(loco), 0x150) != 0) {
        const float scaled = bitsToFloat(load32(asPtr(unit), 0x120)) * bitsToFloat(kNineTenths);
        const std::uint32_t radius = floatToBits(scaled);
        invoke(env, 0x0063BF20u, load32(asPtr(loco), 0x150), {addressOf(&env.scratch[0]), radius});
        const std::uint32_t holder = load32(asPtr(loco), 0x150);
        const std::uint32_t clear = invoke(env, 0x00639FB0u, holder,
                                           {addressOf(&env.scratch[0]), 0u, 0u, 1u, 1u});
        if ((clear & 0xFFu) == 0) {
            status = kSuccess;
        }
    }
    if (status == kSuccess) {
        const std::uint32_t code = virtualCall(env, addressOf(self), 0x50, {});
        invoke(env, 0x007DAD60u, addressOf(self), {unit, machineOf(self), code});
    }
    return status;
}

// 007D1A00 AITunnelNetworkGuardState slot 3 (xfer).
std::uint32_t aiTunnelNetworkGuardXfer(std::uint8_t* self, std::uint32_t eaxIn,
                                       std::uint32_t xfer, AIState7DEnv& env)
{
    std::uint32_t eax = (eaxIn & 0xFFFFFF00u) | 1u;
    if (((load32(asPtr(xfer), 4) >> 3) & 1u) != 0) {
        return eax;
    }
    env.scratch[0] = 1;
    env.scratch[1] = 1;
    virtualCall(env, xfer, 0x18, {addressOf(&env.scratch[0])});
    env.scratch[4] = load32(self, 0x20) != 0 ? 1u : 0u;
    eax = virtualCall(env, xfer, 0x84, {addressOf(&env.scratch[4])});
    if (env.scratch[4] == 0) {
        return eax;
    }
    if (load32(self, 0x20) == 0) {
        const std::uint32_t created = invoke(env, 0x00416820u, xfer, {0x94u});
        std::uint32_t installed = 0;
        if (created != 0) {
            installed = invoke(env, 0x007CE950u, created, {unitOf(machineOf(self))});
        }
        store32(self, 0x20, installed);
        eax = installed;
    }
    if (env.scratch[4] == 0) {
        return eax;
    }
    return virtualCall(env, xfer, 0x20, {load32(self, 0x20)});
}

// 007D1AA0 AITunnelNetworkGuardState slot 4.
std::uint32_t aiTunnelNetworkGuardOnEnter(std::uint8_t* self, AIState7DEnv& env)
{
    const std::uint32_t unit = unitOf(machineOf(self));
    const std::uint32_t loco = locoOf(unit);
    const std::uint32_t created = invoke(env, 0x00416820u, unit, {0x94u});
    std::uint32_t object = 0;
    if (created != 0) {
        object = invoke(env, 0x007CE950u, created, {unit});
    }
    store32(self, 0x20, object);
    const std::uint32_t position = virtualCall(env, loco, 0x224, {});
    copy3(asPtr(object), 0x80, position);
    const std::uint32_t extra = virtualCall(env, loco, 0x244, {});
    store32(asPtr(object), 0x90, extra);
    const std::uint32_t probe = virtualCall(env, object, 0x14, {});
    if (probe == kFailure) {
        return probe;
    }
    return virtualCall(env, object, 0x1C, {4u});
}

// 007DC680 Squad slot 3.
std::uint32_t squadXfer(std::uint8_t* self, std::uint32_t xfer, AIState7DEnv& env)
{
    const std::uint32_t mode = load32(asPtr(xfer), 4);
    if (((mode >> 3) & 1u) != 0) {
        return mode >> 3;
    }
    env.scratch[0] = 1;
    env.scratch[1] = 1;
    std::uint32_t eax = virtualCall(env, xfer, 0x18, {addressOf(&env.scratch[0])});
    eax = invoke(env, 0x0071D020u, xfer, {xfer, addressOf(self) + 8});
    eax = virtualCall(env, xfer, 0x84, {addressOf(self) + 4});
    if (((mode >> 1) & 1u) != 0) {
        store8(self, 0x18, 1);
    }
    return virtualCall(env, xfer, 0x84, {addressOf(self) + 0x2C});
}

// 007DA660 WorkOrder slot 3.
std::uint32_t workOrderXfer(std::uint8_t* self, std::uint32_t eaxIn, std::uint32_t xfer, AIState7DEnv& env)
{
    if (((load32(asPtr(xfer), 4) >> 3) & 1u) != 0) {
        return (eaxIn & 0xFFFFFF00u) | 1u;
    }
    env.scratch[0] = 1;
    env.scratch[1] = 1;
    virtualCall(env, xfer, 0x18, {addressOf(&env.scratch[0])});
    invoke(env, 0x0054BB10u, xfer, {xfer, addressOf(self) + 4});
    invoke(env, 0x0054A090u, addressOf(self) + 8, {xfer, addressOf(self) + 8});
    virtualCall(env, xfer, 0x70, {addressOf(self) + 0x10});
    virtualCall(env, xfer, 0x70, {addressOf(self) + 0x14});
    virtualCall(env, xfer, 0x84, {addressOf(self) + 0x18});
    virtualCall(env, xfer, 0x84, {addressOf(self) + 0x19});
    virtualCall(env, xfer, 0x70, {addressOf(self) + 0x1C});
    invoke(env, 0x00708900u, xfer, {xfer, addressOf(self) + 0x20});
    virtualCall(env, xfer, 0x6C, {addressOf(self) + 0x30});
    virtualCall(env, xfer, 0x84, {addressOf(self) + 0x34});
    virtualCall(env, xfer, 0x84, {addressOf(self) + 0x35});
    store32(env.scratch, 32, load32(self, 0x38));
    return virtualCall(env, xfer, 0x70, {addressOf(&env.scratch[32])});
}

// 007DBBC0 AIChargeTargetState slot 4.
std::uint32_t aiChargeTargetOnEnter(std::uint8_t* self, AIState7DEnv& env)
{
    const std::uint32_t machine = machineOf(self);
    const std::uint32_t unit = unitOf(machine);
    const std::uint32_t loco = locoOf(unit);
    const std::uint32_t goal = invoke(env, 0x005B9F30u, machine, {});
    if (loco == 0 || goal == 0) {
        return kFailure;
    }
    store8(self, 0x4C, 0);
    store32(self, 0x20, load32(asPtr(goal), 0x38));
    store32(self, 0x24, load32(asPtr(goal), 0x3C));
    store32(self, 0x28, load32(asPtr(goal), 0x40));
    const std::uint32_t audio = load32(asPtr(unit), 0xF4);
    if (audio != 0) {
        store32(env.scratch, 0, audio);
        invoke(env, 0x005559F0u, addressOf(&env.scratch[16]), {addressOf(&env.scratch[0])});
        invoke(env, 0x00879050u, addressOf(&env.scratch[32]), {addressOf(&env.scratch[16]), 0x1Du, 0u});
        invoke(env, 0x00550290u, addressOf(&env.scratch[16]), {});
    }
    return invoke(env, 0x007C08A0u, addressOf(self), {});
}

// 007D19C0 AIStateMachine slot 9. Allocates 0x80 and constructs with the machine unit.
std::uint32_t aiStateMachineNewSlot9(std::uint8_t* self, std::uint32_t argument, AIState7DEnv& env)
{
    const std::uint32_t created = invoke(env, 0x00416820u, addressOf(self), {0x80u});
    if (created == 0) {
        return 0;
    }
    return invoke(env, 0x007D0EE0u, created, {unitOf(addressOf(self)), 0x5E655E53u, argument});
}

// 007D0980 AIStateMachine slot 19.
std::uint32_t aiStateMachineNewSlot19(std::uint8_t* self, std::uint32_t arg0, std::uint32_t arg1,
                                      AIState7DEnv& env)
{
    const std::uint32_t created = invoke(env, 0x00416820u, addressOf(self), {0x80u});
    if (created == 0) {
        return 0;
    }
    return invoke(env, 0x007C6F80u, created, {arg0, arg1});
}

// 007D09B0 AIStateMachine slot 20.
std::uint32_t aiStateMachineNewSlot20(std::uint8_t* self, std::uint32_t arg0, AIState7DEnv& env)
{
    const std::uint32_t created = invoke(env, 0x00416820u, addressOf(self), {0x80u});
    if (created == 0) {
        return 0;
    }
    return invoke(env, 0x007C7240u, created, {arg0});
}

// 007D0AA0 UnitAIDisguiseHoldGroundState slot 19.
std::uint32_t unitAIDisguiseHoldGroundClone(std::uint8_t* self, AIState7DEnv& env)
{
    const std::uint32_t created = invoke(env, 0x00416820u, addressOf(self), {0x80u});
    if (created == 0) {
        return 0;
    }
    return invoke(env, 0x007D0A00u, created, {unitOf(machineOf(self))});
}

// 007D09E0 AIStateMachine slot 21. Tail-calls the constructor with the caller's argument.
std::uint32_t aiStateMachineNewSlot21(std::uint8_t* self, std::uint32_t argument, AIState7DEnv& env)
{
    const std::uint32_t created = invoke(env, 0x00416820u, addressOf(self), {0x80u});
    if (created == 0) {
        return 0;
    }
    return invoke(env, 0x007C70E0u, created, {argument});
}

// 007D0A00 DisguiseHoldGroundStateMachine constructor. Writes 0x00C343F8 on this
// and 0x00C2E640 on the nested state.
std::uint32_t disguiseHoldGroundConstruct(std::uint8_t* self, std::uint32_t unit, AIState7DEnv& env)
{
    invoke(env, 0x007C91E0u, addressOf(self), {unit});
    store32(self, 0, 0x00C343F8u);
    const std::uint32_t created = invoke(env, 0x00416820u, addressOf(self), {0x24u});
    std::uint32_t state = 0;
    if (created != 0) {
        invoke(env, 0x005A9F60u, created, {addressOf(self), 0xECD646A3u});
        store32(asPtr(created), 0x20, 0);
        store32(asPtr(created), 0, 0x00C2E640u);
        state = created;
    }
    invoke(env, 0x005BDC20u, addressOf(self), {0u, state, 0u, 0u, 0x00C343D4u});
    return addressOf(self);
}

// 007DB530 AIBackAwayState slot 5.
std::uint32_t aiBackAwayUpdate(std::uint8_t* self, AIState7DEnv& env)
{
    const std::uint32_t machine = machineOf(self);
    const std::uint32_t unit = unitOf(machine);
    if ((load8(asPtr(unit), 0xCE) & 1u) == 0) {
        store32(asPtr(unit), 0xCC, load32(asPtr(unit), 0xCC) | 0x10000u);
        invoke(env, 0x006A4F80u, unit, {0x170u, 1u});
        invoke(env, 0x006A4FB0u, unit, {});
    }
    const std::uint32_t loco = locoOf(unit);
    const std::uint32_t goal = invoke(env, 0x005B9F30u, machine, {});
    if (goal == 0 || (load8(asPtr(goal), 0x471) & 1u) != 0 || load8(self, 0x58) != 0) {
        return kSuccess;
    }
    if (load8(self, 0x54) != 0 && loco != 0 && load32(asPtr(loco), 0x150) != 0 &&
        load8(asPtr(loco), 0x428) == 0) {
        const std::uint32_t node = load32(asPtr(loco), 0x150);
        const std::uint32_t position = load32(asPtr(node), 0x14) + 0x0C;
        store32(self, 0x20, load32(asPtr(position), 0));
        store32(self, 0x24, load32(asPtr(position), 4));
        store32(self, 0x28, load32(asPtr(position), 8));
        store8(self, 0x4C, 0);
        store8(self, 0x54, 0);
    }
    const std::uint32_t moving = invoke(env, 0x007B12D0u, addressOf(self), {});
    if (moving != 0) {
        store8(self, 0x58, 1);
        if (loco != 0) {
            invoke(env, 0x00682D70u, loco, {});
        }
    }
    const std::uint32_t kind = invoke(env, 0x006C2300u, unit, {addressOf(&env.scratch[0]), 4u});
    if ((kind & 0xFFu) != 1u) {
        return 0;
    }
    const std::uint32_t audio = load32(asPtr(unit), 0xF4);
    if (audio == 0) {
        return kSuccess;
    }
    store32(env.scratch, 16, audio);
    invoke(env, 0x005559F0u, addressOf(&env.scratch[32]), {addressOf(&env.scratch[16])});
    invoke(env, 0x00879050u, addressOf(&env.scratch[48]), {addressOf(&env.scratch[32]), 0x0Eu, 0u});
    invoke(env, 0x00550290u, addressOf(&env.scratch[32]), {});
    return kSuccess;
}

// 007DB430 AICowerState / AIUncontrollableCower slot 5.
std::uint32_t aiCowerUpdate(std::uint8_t* self, AIState7DEnv& env)
{
    const std::uint32_t goal = invoke(env, 0x005B9F30u, machineOf(self), {});
    if (goal == 0) {
        return kSuccess;
    }
    if ((load8(asPtr(goal), 0x471) & 1u) != 0) {
        const std::uint32_t record = load32(asPtr(goal), 4);
        if (((load32(asPtr(record), 0xCC) >> 25) & 1u) == 0) {
            return kSuccess;
        }
    }
    const std::uint32_t unit = unitOf(machineOf(self));
    if (unit == 0) {
        return kFailure;
    }
    if ((virtualCall(env, addressOf(self), 0x4C, {}) & 0xFFu) == 0) {
        return 0;
    }
    store32(env.scratch, 0, 0);
    const std::uint32_t hit = invoke(env, 0x006C2300u, unit, {addressOf(&env.scratch[0]), 4u});
    std::uint8_t accepted = 0;
    if ((hit & 0xFFu) != 0) {
        accepted = static_cast<std::uint8_t>(
            invoke(env, 0x006F1880u, unit, {load32(env.scratch, 0)}) & 0xFFu);
    }
    const std::uint32_t loco = locoOf(unit);
    if (loco == 0) {
        return kFailure;
    }
    if (accepted == 0) {
        return 0;
    }
    if ((invoke(env, 0x00683320u, loco, {}) & 0xFFu) != 0) {
        return 0;
    }
    store32(env.scratch, 16, load32(asPtr(unit), 0xF4));
    invoke(env, 0x005559F0u, addressOf(&env.scratch[32]), {addressOf(&env.scratch[16])});
    invoke(env, 0x00879050u, addressOf(&env.scratch[48]), {addressOf(&env.scratch[32]), 0x0Eu, 0u});
    invoke(env, 0x00550290u, addressOf(&env.scratch[32]), {});
    return kSuccess;
}

// 007DE580 AIFaceState (vtable 0x00C2F248) slot 5.
std::uint32_t aiFaceStateUpdateWide(std::uint8_t* self, AIState7DEnv& env)
{
    const std::uint32_t machine = machineOf(self);
    const std::uint32_t goal = invoke(env, 0x005B9F30u, machine, {});
    if (goal == 0 || (load8(asPtr(goal), 0x80) & 1u) != 0 || (load8(asPtr(goal), 0x471) & 1u) != 0) {
        return kFailure;
    }
    const std::uint32_t face = invoke(env, 0x00687810u, goal, {});
    if (face == 0 || (virtualCall(env, face, 0x38, {}) & 0xFFu) == 0) {
        return kFailure;
    }
    const std::uint32_t unit = unitOf(machine);
    store32(env.scratch, 0, 0);
    store32(env.scratch, 4, 0);
    store32(env.scratch, 8, 0);
    virtualCall(env, face, 0x18, {addressOf(&env.scratch[0]), unit});
    if ((virtualCall(env, face, 0x50, {}) & 0xFFu) != 0) {
        const float dx = bitsToFloat(load32(asPtr(unit), 0x38)) - bitsToFloat(load32(env.scratch, 0));
        const float dy = bitsToFloat(load32(asPtr(unit), 0x3C)) - bitsToFloat(load32(env.scratch, 4));
        const float dz = bitsToFloat(load32(asPtr(unit), 0x40)) - bitsToFloat(load32(env.scratch, 8));
        const float nx = bitsToFloat(load32(asPtr(unit), 0x38)) + dx;
        const float ny = bitsToFloat(load32(asPtr(unit), 0x3C)) + dy;
        const float nz = bitsToFloat(load32(asPtr(unit), 0x40)) + dz;
        store32(env.scratch, 0, floatToBits(nx));
        store32(env.scratch, 4, floatToBits(ny));
        store32(env.scratch, 8, floatToBits(nz));
    }
    invoke(env, 0x005AC4F0u, machine, {addressOf(&env.scratch[0]), kFltMax});
    return invoke(env, 0x007DBB20u, addressOf(self), {});
}

// 007DBB20 AIFaceState (vtable 0x00C2D950) slot 5.
std::uint32_t aiFaceStateUpdate(std::uint8_t* self, AIState7DEnv& env)
{
    const std::uint32_t machine = machineOf(self);
    const std::uint32_t unit = unitOf(machine);
    const std::uint32_t loco = locoOf(unit);
    std::uint32_t aim = machine + 0x2C;
    if (load32(self, 0x28) != 0) {
        const std::uint32_t goal = invoke(env, 0x005B9F30u, machine, {});
        if (goal == 0) {
            return kFailure;
        }
        aim = goal + 0x38;
    }
    invoke(env, 0x006A6CB0u, unit, {aim});
    const float angle = bitsToFloat(env.lastSt0);
    if (std::fabs(angle) < bitsToFloat(kAngleLimit)) {
        return kSuccess;
    }
    if (load8(self, 0x2C) != 0) {
        const float turned = angle + bitsToFloat(load32(asPtr(unit), 0x44));
        virtualCall(env, loco, 0x26C, {floatToBits(turned)});
    } else {
        virtualCall(env, loco, 0x260, {aim});
    }
    if (load32(self, 0x28) == 2) {
        return invoke(env, 0x007DB090u, addressOf(self), {});
    }
    return 0;
}

// 007DFE70 AIAttackSquadState slot 4.
std::uint32_t aiAttackSquadOnEnter(std::uint8_t* self, AIState7DEnv& env)
{
    const std::uint32_t machine = machineOf(self);
    const std::uint32_t unit = unitOf(machine);
    if (unit == 0) {
        return kFailure;
    }
    const std::uint32_t chosen = virtualCall(env, machine, 0x20, {load8(self, 0x25)});
    store32(self, 0x20, chosen);
    const std::uint32_t weapon = invoke(env, 0x007DEE00u, addressOf(self), {});
    const std::uint32_t locked = invoke(env, 0x006A51A0u, unit, {kSuccess});
    if (locked != 0 && weapon != 0) {
        bool done = false;
        if (locoOf(unit) != 0 && load32(asPtr(unit), 0x37C) != 0) {
            const std::uint32_t member = load32(asPtr(unit), 0x37C);
            if ((invoke(env, 0x006A1930u, member, {unit + 0x38, weapon, 0u, kSuccess, 1u}) & 0xFFu) != 0 &&
                (invoke(env, 0x007AD2D0u, load32(asPtr(unit), 0x37C),
                        {unit, unit + 0x38, weapon, 0u, 0u, 1u}) &
                 0xFFu) != 0) {
                done = true;
            }
        }
        if (!done) {
            const std::uint32_t loco = locoOf(unit);
            if ((invoke(env, 0x006F04C0u, unit, {}) & 0xFFu) == 0 || load8(asPtr(loco), 0x443) != 0) {
                return kFailure;
            }
        }
    }
    const std::uint32_t again = unitOf(machine);
    if (again != 0) {
        const std::uint32_t slot = invoke(env, 0x006876F0u, again, {});
        if (slot != 0) {
            const std::uint32_t slot2 = invoke(env, 0x006876F0u, unitOf(machine), {weapon});
            invoke(env, 0x007DCAA0u, slot2, {weapon});
        }
    }
    store8(self, 0x24, 0);
    return virtualCall(env, load32(self, 0x20), 0x14, {});
}

// 007D0D00 AIGuardReturnState slot 4.
std::uint32_t aiGuardReturnOnEnter(std::uint8_t* self, AIState7DEnv& env)
{
    const std::uint32_t unit = unitOf(machineOf(self));
    const std::uint32_t loco = locoOf(unit);
    if (loco != 0) {
        const std::uint32_t position = virtualCall(env, loco, 0x224, {});
        const float dx = bitsToFloat(load32(asPtr(position), 0)) - bitsToFloat(load32(asPtr(unit), 0x38));
        const float dy = bitsToFloat(load32(asPtr(position), 4)) - bitsToFloat(load32(asPtr(unit), 0x3C));
        const float dz = bitsToFloat(load32(asPtr(position), 8)) - bitsToFloat(load32(asPtr(unit), 0x40));
        float sum = dy * dy;
        sum = sum + dz * dz;
        sum = sum + dx * dx;
        if (bitsToFloat(kLimit400) > sum) {
            return kSuccess;
        }
        store32(self, 0x20, load32(asPtr(position), 0));
        store32(self, 0x24, load32(asPtr(position), 4));
        store32(self, 0x28, load32(asPtr(position), 8));
        if ((virtualCall(env, loco, 0x274, {}) & 0xFFu) != 0) {
            invoke(env, 0x0063B510u, addressOf(&env.scratch[0]), {unit, loco + 0x1F4, addressOf(self) + 0x20});
        }
        invoke(env, 0x006996C0u, loco, {});
        std::uint32_t radius = load32(asPtr(loco), 0x78);
        if ((virtualCall(env, loco, 0x250, {}) & 0xFFu) != 0) {
            const std::uint32_t slot = invoke(env, 0x006876F0u, unit, {1u});
            const std::uint32_t held = invoke(env, 0x007CFC10u, slot, {});
            if (held != 0) {
                return kFailure;
            }
            radius = kFltMax;
        }
        invoke(env, 0x005AC4F0u, machineOf(self), {addressOf(self) + 0x20, radius});
    }
    store8(self, 0x4C, 1);
    return invoke(env, 0x007C08A0u, addressOf(self), {});
}

// 007DA4D0 AIPlayer / AISkirmishPlayer slot 16. Countdown, then a chopped time scale.
std::uint32_t aiPlayerSlot16(std::uint8_t* self, AIState7DEnv& env)
{
    const std::uint32_t record = load32(self, 4);
    if (load8(asPtr(record), 0x105) == 0) {
        return record;
    }
    if (load8(self, 0x21) == 0) {
        const std::uint32_t next = load32(self, 0x28) + kSuccess;
        store32(self, 0x28, next);
        if (static_cast<std::int32_t>(next) <= 0) {
            store8(self, 0x21, 1);
            store32(self, 0x30, 0);
        }
    }
    const std::uint32_t timer = load32(self, 0x30) + kSuccess;
    store32(self, 0x30, timer);
    if (static_cast<std::int32_t>(timer) >= 1) {
        return kSuccess;
    }
    std::uint32_t eax = kSuccess;
    if (load8(self, 0x21) != 0) {
        eax = invoke(env, 0x007D8D60u, addressOf(self), {});
    }
    if (static_cast<std::int32_t>(load32(self, 0x30)) >= 1) {
        return eax;
    }
    long double value = static_cast<long double>(bitsToFloat(env.timeScaleBits));
    if (env.gameLogic != 0) {
        value = static_cast<long double>(bitsToFloat(load32(asPtr(env.gameLogic), 0x1C4))) * value;
    }
    const std::uint32_t chopped = doubledChop(value);
    store32(self, 0x30, chopped);
    return chopped;
}

// 007DA590 AIPlayer slot 19.
std::uint32_t aiPlayerSlot19(std::uint8_t* self, AIState7DEnv& env)
{
    const std::uint32_t record = load32(self, 4);
    if (load8(asPtr(record), 0x104) == 0) {
        return record;
    }
    if (load8(self, 0x20) == 0) {
        const std::uint32_t next = load32(self, 0x24) + kSuccess;
        store32(self, 0x24, next);
        if (static_cast<std::int32_t>(next) <= 0) {
            store8(self, 0x20, 1);
            store32(self, 0x34, 0);
        }
    }
    const std::uint32_t timer = load32(self, 0x34) + kSuccess;
    store32(self, 0x34, timer);
    if (static_cast<std::int32_t>(timer) >= 1) {
        return kSuccess;
    }
    invoke(env, 0x007DA110u, addressOf(self), {});
    if (load8(self, 0x20) != 0) {
        virtualCall(env, addressOf(self), 0x68, {});
    }
    long double value = static_cast<long double>(bitsToFloat(env.timeScaleBits));
    if (env.gameLogic != 0) {
        value = static_cast<long double>(bitsToFloat(load32(asPtr(env.gameLogic), 0x1C4))) * value;
    }
    value = value * static_cast<long double>(bitsToFloat(kFive));
    const std::uint32_t chopped = chopTowardZero(value);
    store32(self, 0x34, chopped);
    return chopped;
}

// 007D2F90 Turret aim probe. Walks the +0x9C chain and reports the leaf mode.
std::uint32_t turretAIReadAim(std::uint8_t* self, std::uint32_t outId, std::uint32_t outVec, AIState7DEnv& env)
{
    while (load32(self, 0x9C) != 0) {
        self = asPtr(load32(self, 0x9C));
    }
    store32(asPtr(outId), 0, 0);
    store32(asPtr(outVec), 0, 0);
    store32(asPtr(outVec), 4, 0);
    store32(asPtr(outVec), 8, 0);
    const std::uint32_t mode = load32(self, 0x78);
    if (mode == 1) {
        const std::uint32_t linked = load32(self, 0x90);
        if (linked != 0) {
            const std::uint32_t found = invoke(env, 0x007D2120u, linked, {});
            store32(asPtr(outId), 0, found);
            if (found == 0 || (load8(asPtr(found), 0x471) & 1u) != 0) {
                if ((invoke(env, 0x0067D700u, env.gameLogic, {}) & 0xFFu) != 0) {
                    store32(self, 0x78, 0);
                    store8(self, 0x8E, 0);
                    return 0;
                }
                return 0;
            }
        }
    } else if (mode == 2) {
        store32(asPtr(outId), 0, 0);
        const std::uint32_t source = load32(self, 0x60) + 0x2C;
        store32(asPtr(outVec), 0, load32(asPtr(source), 0));
        store32(asPtr(outVec), 4, load32(asPtr(source), 4));
        store32(asPtr(outVec), 8, load32(asPtr(source), 8));
    }
    return load32(self, 0x78);
}

// 007D6090 TurretAIAimTurretState / TurretAIAimTurretStateWhenBusy slot 5.
std::uint32_t turretAIAimUpdate(std::uint8_t* self, AIState7DEnv& env)
{
    const std::uint32_t turret = load32(asPtr(machineOf(self)), 0x80);
    const std::uint32_t unit = load32(asPtr(turret), 0x5C);
    store32(env.scratch, 0, 0);
    store32(env.scratch, 16, 0);
    store32(env.scratch, 20, 0);
    store32(env.scratch, 24, 0);
    const std::uint32_t mode = invoke(env, 0x007D2F90u, turret,
                                      {addressOf(&env.scratch[0]), addressOf(&env.scratch[16])});
    std::uint32_t object = load32(env.scratch, 0);
    if (mode == 0) {
        return kFailure;
    }
    if (mode == 1) {
        if (object == 0) {
            return kFailure;
        }
        if (((load32(asPtr(load32(asPtr(object), 4)), 0xC0) >> 23) & 1u) == 0) {
            store32(env.scratch, 16, load32(asPtr(object), 0x38));
            store32(env.scratch, 20, load32(asPtr(object), 0x3C));
            store32(env.scratch, 24, load32(asPtr(object), 0x40));
        }
        store32(env.scratch, 0, 0);
        object = 0;
    }
    const std::uint32_t weapon = invoke(env, 0x006A51C0u, unit, {load32(asPtr(turret), 0x94)});
    const std::uint32_t tune = load32(asPtr(turret), 0x58);
    if (!sseEqualZero(load32(asPtr(tune), 0x50)) && weapon != 0 &&
        (invoke(env, 0x006F1350u, unit, {}) & 0xFFu) != 0) {
        const float radius = bitsToFloat(load32(asPtr(load32(asPtr(turret), 0x58)), 0x50));
        const float dy = bitsToFloat(load32(asPtr(unit), 0x3C)) - bitsToFloat(load32(env.scratch, 20));
        const float dx = bitsToFloat(load32(asPtr(unit), 0x38)) - bitsToFloat(load32(env.scratch, 16));
        const float limit = radius * radius;
        float dist = dy * dy;
        dist = dist + dx * dx;
        if (limit >= dist) {
            store32(asPtr(weapon), 0x24, 0x7FFFFFFFu);
            store8(asPtr(weapon), 0x60, 1);
        }
    }
    const float yawSteps = static_cast<float>(load8(asPtr(load32(asPtr(turret), 0x58)), 0x5E));
    if (!sseEqualZero(floatToBits(yawSteps))) {
        invoke(env, 0x006A6CB0u, unit, {addressOf(&env.scratch[16])});
        invoke(env, 0x007AA5B0u, addressOf(self), {unit, env.lastSt0});
    }
    const std::uint32_t aimed = virtualCall(env, addressOf(self), 0x4C, {addressOf(&env.scratch[16])});
    if ((aimed & 0xFFu) == 0) {
        return 0;
    }
    object = load32(env.scratch, 0);
    if (object != 0) {
        if (weapon == 0) {
            return kSuccess;
        }
        if ((invoke(env, 0x006F2140u, weapon, {unit, object, 0u, 1u}) & 0xFFu) != 0) {
            return kSuccess;
        }
        return 0;
    }
    if (weapon == 0) {
        return kSuccess;
    }
    if ((invoke(env, 0x006F2100u, weapon, {unit, addressOf(&env.scratch[16]), 0u, 1u}) & 0xFFu) == 0) {
        return 0;
    }
    return kSuccess;
}

// 007DF320 helper reached from UnitAIPickTargetFromSquadState. Forwards four
// dwords of context as ECX plus the two dwords at the argument.
std::uint32_t unitAIPickTargetApplySpan(std::uint8_t* self, std::uint32_t argument, AIState7DEnv& env)
{
    const std::uint32_t first = load32(asPtr(argument), 0);
    const std::uint32_t second = load32(asPtr(argument), 4);
    return invoke(env, 0x007D34C0u, load32(self, 0x0C), {first, second});
}

// 007DFCF0 UnitAIPickTargetFromSquadState slot 4.
std::uint32_t unitAIPickTargetOnEnter(std::uint8_t* self, AIState7DEnv& env)
{
    const std::uint32_t machine = machineOf(self);
    const std::uint32_t squad = virtualCall(env, machine, 0x44, {});
    if (squad == 0) {
        return kFailure;
    }
    store32(env.scratch, 0, 0);
    store32(env.scratch, 4, 0);
    store32(env.scratch, 8, 0);
    invoke(env, 0x007DC420u, squad, {addressOf(&env.scratch[0]), 1u});
    std::uint32_t cursor = load32(env.scratch, 0);
    const std::uint32_t end = load32(env.scratch, 4);
    std::uint32_t chosen = 0;
    if (cursor != end) {
        const std::uint32_t unit = unitOf(machine);
        const std::uint32_t filter = load32(asPtr(locoOf(unit)), 0x47C);
        if (filter != 0) {
            store32(env.scratch, 32, unit);
            store32(env.scratch, 36, load32(asPtr(env.logicTable), 0x84) + 4);
            store32(env.scratch, 40, 0);
            invoke(env, 0x007DF320u, addressOf(&env.scratch[16]), {addressOf(&env.scratch[32])});
            while (cursor != end) {
                const std::uint32_t member = load32(asPtr(cursor), 0);
                if (member != 0 && (invoke(env, 0x007A7730u, filter, {member}) & 0xFFu) != 0) {
                    chosen = member;
                    break;
                }
                cursor += 4;
            }
        }
    }
    const std::uint32_t begin = load32(env.scratch, 0);
    const std::uint32_t unit = unitOf(machine);
    invoke(env, 0x006C0F40u, unit, {chosen});
    virtualCall(env, machine, 0x38, {chosen});
    if (chosen == 0) {
        if (begin != 0) {
            invoke(env, 0x004166B0u, addressOf(&env.scratch[48]), {begin});
        }
        return kFailure;
    }
    if (begin != 0) {
        invoke(env, 0x004166B0u, addressOf(&env.scratch[48]), {begin});
    }
    return kSuccess;
}

// 007D1860 AIAttackAimAtTargetState slot 4.
std::uint32_t aiAttackAimOnEnter(std::uint8_t* self, AIState7DEnv& env)
{
    const std::uint32_t machine = machineOf(self);
    const std::uint32_t unit = unitOf(machine);
    const std::uint32_t weapon = invoke(env, 0x006A51A0u, unit, {kSuccess});
    const std::uint32_t goal = invoke(env, 0x005B9F30u, machine, {});
    const std::uint32_t source = load32(asPtr(locoOf(unit)), 0x214);
    store8(self, 0x22, 0);
    std::uint32_t flag = 0;
    if (source != 0) {
        invoke(env, 0x00706820u, source, {unit});
        if (bitsToFloat(env.lastSt0) == 0.f) {
            flag = 1;
        }
    }
    store8(self, 0x21, static_cast<std::uint8_t>(flag));
    const std::uint32_t pose = invoke(env, 0x006873C0u, unit, {});
    invoke(env, 0x00686D90u, unit, {unit + 0x38, pose, 0u});
    const std::uint32_t slot = invoke(env, 0x006EFE40u, unit, {});
    std::uint32_t fired = 0;
    if (slot != 0 && weapon != 0) {
        const std::uint32_t turret = invoke(env, 0x006A7620u, slot, {unit, 1u});
        if (goal != 0) {
            fired = virtualCall(env, turret, 0x124, {unit, weapon, goal});
        } else {
            fired = virtualCall(env, turret, 0x120, {unit, weapon, machine + 0x2C});
        }
    } else if (goal != 0) {
        if (weapon == 0) {
            return kFailure;
        }
        fired = invoke(env, 0x006F2140u, weapon, {unit, goal, 0u, 1u});
    } else {
        if (weapon == 0) {
            return kFailure;
        }
        fired = invoke(env, 0x006F2100u, weapon, {unit, machine + 0x2C, 0u, 1u});
    }
    if (weapon == 0) {
        return kFailure;
    }
    const std::uint8_t firedByte = static_cast<std::uint8_t>(fired & 0xFFu);
    if (load8(asPtr(weapon), 0x61) == 0) {
        if ((invoke(env, 0x00685EB0u, load32(asPtr(weapon), 4), {}) & 0xFFu) != 0 && firedByte != 0) {
            return kSuccess;
        }
        if ((invoke(env, 0x00685EC0u, load32(asPtr(weapon), 4), {}) & 0xFFu) != 0 && firedByte == 0) {
            return kFailure;
        }
    }
    if (((load32(asPtr(unit), 0x88) >> 14) & 1u) != 0) {
        return kSuccess;
    }
    invoke(env, 0x006D2020u, unit, {0x1Bu, 1u});
    return invoke(env, 0x007CD890u, addressOf(self), {1u});
}

// 007DFF50 AIAttackSquadState slot 5.
std::uint32_t aiAttackSquadUpdate(std::uint8_t* self, AIState7DEnv& env)
{
    if (load32(self, 0x20) == 0) {
        return kFailure;
    }
    const std::uint32_t machine = machineOf(self);
    const std::uint32_t unit = unitOf(machine);
    if (((load32(asPtr(unit), 0x80) >> 30) & 1u) != 0) {
        store8(self, 0x24, 1);
    }
    const std::uint32_t loco = locoOf(unit);
    const std::uint32_t slot = invoke(env, 0x006876F0u, unit, {});
    if (slot == 0) {
        return kFailure;
    }
    std::uint32_t target = invoke(env, 0x007CFC10u, slot, {1u});
    if (target == 0 || (load8(asPtr(target), 0x471) & 1u) != 0 || (load8(asPtr(target), 0x80) & 1u) != 0) {
        target = invoke(env, 0x007DEE00u, addressOf(self), {});
        if (target == 0) {
            return kSuccess;
        }
        invoke(env, 0x007DCAA0u, slot, {target});
    }
    const std::uint32_t weapon = invoke(env, 0x006A51A0u, unit, {kSuccess});
    if (weapon == 0) {
        return kFailure;
    }
    std::uint32_t remain = virtualCall(env, load32(self, 0x20), 0x10, {});
    if (static_cast<std::int32_t>(remain) > 0) {
        remain = 0;
    }
    const std::uint32_t order = load32(self, 0x20);
    if (order == 0) {
        return 0;
    }
    const std::uint32_t orderLink = load32(asPtr(order), 4);
    if (orderLink == 0 || load32(asPtr(orderLink), 4) != 0) {
        return remain;
    }
    if ((load8(asPtr(target), 0x471) & 1u) == 0) {
        if ((invoke(env, 0x0065BF00u, addressOf(&env.scratch[0]), {unit, target + 0x38, weapon, 0u}) & 0xFFu) == 0) {
            return kFailure;
        }
    }
    if (((load32(asPtr(unit), 0x84) >> 8) & 1u) != 0) {
        if (invoke(env, 0x006EFE40u, unit, {}) != 0) {
            return kSuccess;
        }
    }
    const std::uint32_t contact = invoke(env, 0x006E5CE0u, loco, {});
    if (contact != 0) {
        virtualCall(env, order, 0x38, {contact});
        virtualCall(env, order, 0x1C, {0x27u});
        return 0;
    }
    if ((load8(asPtr(load32(asPtr(target), 4)), 0xC7) & 1u) == 0) {
        const std::uint32_t left = invoke(env, 0x00686FA0u, unit, {});
        const std::uint32_t right = invoke(env, 0x00686FA0u, target, {});
        if (left == right) {
            return kSuccess;
        }
    }
    if ((invoke(env, 0x007BC640u, addressOf(&env.scratch[8]), {unit, target, 0u, 0u, 1u}) & 0xFFu) == 0) {
        if ((invoke(env, 0x006F04C0u, unit, {}) & 0xFFu) == 0 || load8(asPtr(loco), 0x443) != 0) {
            return kFailure;
        }
    }
    virtualCall(env, order, 0x1C, {0x0Au});
    return 0;
}

// 007D1D60 TurretAI slot 3.
std::uint32_t turretAIXfer(std::uint8_t* self, std::uint32_t xfer, AIState7DEnv& env)
{
    const std::uint32_t mode = load32(asPtr(xfer), 4);
    if (((mode >> 3) & 1u) != 0) {
        return mode >> 3;
    }
    env.scratch[0] = 1;
    env.scratch[1] = 2;
    virtualCall(env, xfer, 0x18, {addressOf(&env.scratch[0])});
    virtualCall(env, xfer, 0x20, {load32(self, 0x60)});
    virtualCall(env, xfer, 0x64, {addressOf(self) + 0x64});
    virtualCall(env, xfer, 0x64, {addressOf(self) + 0x68});
    virtualCall(env, xfer, 0x6C, {addressOf(self) + 0x70});
    invoke(env, 0x00549E10u, xfer, {xfer, addressOf(self) + 0x78});
    virtualCall(env, xfer, 0x6C, {addressOf(self) + 0x7C});
    virtualCall(env, xfer, 0x84, {addressOf(self) + 0x88});
    virtualCall(env, xfer, 0x84, {addressOf(self) + 0x89});
    virtualCall(env, xfer, 0x84, {addressOf(self) + 0x8A});
    virtualCall(env, xfer, 0x84, {addressOf(self) + 0x8B});
    virtualCall(env, xfer, 0x84, {addressOf(self) + 0x8C});
    virtualCall(env, xfer, 0x84, {addressOf(self) + 0x8D});
    virtualCall(env, xfer, 0x84, {addressOf(self) + 0x8E});
    virtualCall(env, xfer, 0x6C, {addressOf(self) + 0x80});
    if (env.subsystem != 0) {
        const std::uint32_t table = load32(asPtr(env.subsystem), 0);
        const std::uint32_t function = load32(asPtr(table), 0x158);
        invoke(env, function, env.subsystem, {xfer, addressOf(self) + 0x6C});
    }
    std::uint32_t current = 0;
    if (load32(self, 0x74) != 0) {
        current = load32(asPtr(load32(self, 0x74)), 8);
    }
    store32(env.scratch, 16, current);
    virtualCall(env, xfer, 0x6C, {addressOf(&env.scratch[16])});
    const std::uint32_t resolved = invoke(env, 0x007EBDE0u, env.registry, {load32(env.scratch, 16)});
    store32(self, 0x74, resolved);
    virtualCall(env, xfer, 0x70, {addressOf(self) + 0x94});
    if (env.scratch[1] < 2) {
        env.scratch[4] = 0;
        virtualCall(env, xfer, 0x84, {addressOf(&env.scratch[4])});
    }
    env.scratch[8] = load32(self, 0x90) != 0 ? 1u : 0u;
    std::uint32_t eax = virtualCall(env, xfer, 0x84, {addressOf(&env.scratch[8])});
    if (env.scratch[8] == 0) {
        return eax;
    }
    if (((mode >> 1) & 1u) != 0 && load32(self, 0x90) == 0) {
        invoke(env, 0x007CF730u, addressOf(self), {});
    }
    return virtualCall(env, load32(self, 0x90), 0x00, {xfer});
}

// 007DCAA0 reached from AIAttackSquadState. Rebinds when the target id changes.
std::uint32_t aiAttackSquadBindTarget(std::uint8_t* self, std::uint32_t target, AIState7DEnv& env)
{
    if (target == 0) {
        store32(self, 0x70, kSuccess);
        store32(self, 0x28, 0);
        return 0;
    }
    if (load32(asPtr(target), 0xDC) == load32(self, 0x28)) {
        return target;
    }
    store32(self, 0x70, 0);
    const std::uint32_t id = load32(asPtr(target), 0xDC);
    store32(self, 0x28, id);
    store32(self, 0x30, id);
    const std::uint32_t result = invoke(env, 0x007DC910u, addressOf(self), {target, 1u});
    store32(self, 0x24, 0);
    store32(self, 0x38, 0);
    return result;
}

// 007DB090 AIIdleState slot 5.
std::uint32_t aiIdleUpdate(std::uint8_t* self, AIState7DEnv& env)
{
    const std::uint32_t unit = unitOf(machineOf(self));
    const std::uint32_t list = load32(asPtr(unit), 0x37C);
    if (list != 0) {
        store32(env.scratch, 0, 0);
        store32(env.scratch, 4, 0);
        store32(env.scratch, 8, 0);
        const std::uint32_t filled = invoke(env, 0x007445C0u, list, {addressOf(&env.scratch[0])});
        std::uint32_t cursor = load32(env.scratch, 0);
        const std::uint32_t end = load32(env.scratch, 4);
        if ((filled & 0xFFu) != 0 && cursor != end) {
            while (cursor != end) {
                if ((invoke(env, 0x006A86C0u, load32(asPtr(cursor), 0), {}) & 0xFFu) != 0) {
                    if (load32(env.scratch, 0) != 0) {
                        invoke(env, 0x004166B0u, addressOf(&env.scratch[16]), {load32(env.scratch, 0)});
                    }
                    return 0;
                }
                cursor += 4;
            }
        }
        if (load32(env.scratch, 0) != 0) {
            invoke(env, 0x004166B0u, addressOf(&env.scratch[16]), {load32(env.scratch, 0)});
        }
    }
    invoke(env, 0x007A1EC0u, addressOf(self), {});
    const std::uint32_t loco = locoOf(unit);
    if (((load32(asPtr(unit), 0x84) >> 29) & 1u) == 0 && load32(asPtr(loco), 0x40) == 0) {
        const std::uint16_t left = static_cast<std::uint16_t>(load16(self, 0x22) + 0xFFFFu);
        store16(self, 0x22, left);
        if (left <= 0) {
            store16(self, 0x22, 0x000F);
            if (invoke(env, 0x00687CB0u, unit, {}) != 0 &&
                (invoke(env, 0x005B6240u, machineOf(self), {0x4Cu}) & 0xFFu) != 0) {
                return kFailure;
            }
        }
    }
    const std::uint32_t base = invoke(env, 0x006835B0u, loco, {});
    const std::uint32_t sum = base + load16(self, 0x20);
    store16(self, 0x20, 0);
    if (load8(self, 0x24) != 0 && load8(asPtr(machineOf(self)), 0x78) == 0) {
        const std::uint32_t record = load32(asPtr(unit), 4);
        if (((load32(asPtr(record), 0xC4) >> 14) & 1u) != 0) {
            if ((virtualCall(env, loco, 0x208, {}) & 0xFFu) != 0) {
                invoke(env, 0x006A6510u, unit, {});
                const std::uint32_t range = env.lastSt0;
                if (invoke(env, 0x00798BE0u, env.partition, {unit, range}) != 0) {
                    virtualCall(env, machineOf(self), 0x1C, {0x28u});
                    return 0;
                }
            }
        }
        const std::uint32_t contact = invoke(env, 0x006E5CE0u, loco, {});
        if (contact != 0) {
            invoke(env, 0x006C50C0u, loco + 0x24, {contact, 2u});
            return 0;
        }
        const std::uint32_t bits = load32(asPtr(unit), 0x26C);
        if (((bits >> 4) & 1u) == 0 && ((bits >> 8) & 1u) == 0 &&
            (invoke(env, 0x004D66A0u, unit, {2u}) & 0xFFu) == 0 &&
            ((load32(asPtr(record), 0xC0) >> 17) & 1u) == 0) {
            const std::uint32_t other = invoke(env, 0x006A6210u, unit, {});
            if (other != 0) {
                const std::uint32_t kind = invoke(env, 0x006A5270u, unit, {});
                if (kind == 1) {
                    invoke(env, 0x006C5710u, loco + 0x24, {other, 0x7FFFFFFFu, 2u});
                } else {
                    invoke(env, 0x006C5680u, loco + 0x24, {other, 0x7FFFFFFFu, 2u});
                }
                return 0;
            }
        }
    }
    return sum;
}

}  // namespace

bool runAIState7D(std::uint32_t va, std::uint8_t* self, std::uint32_t eaxIn,
                  const std::uint32_t* stack, int argc, AIState7DEnv& env, std::uint32_t& eaxOut)
{
    (void)argc;
    const std::uint32_t arg0 = stack != nullptr && argc > 0 ? stack[0] : 0u;
    const std::uint32_t arg1 = stack != nullptr && argc > 1 ? stack[1] : 0u;
    switch (va) {
    case 0x007D0AD0u: eaxOut = unitAILocateNodeConsider(self, env); return true;
    case 0x007D6050u: eaxOut = turretAIAimSlot19(self, env); return true;
    case 0x007D6230u: eaxOut = turretAIFireWeaponUpdate(self, env); return true;
    case 0x007DC160u: eaxOut = aiPrepareForBoardingUpdate(self, env); return true;
    case 0x007DB660u: eaxOut = aiMoveAndEvacuateUpdate(self, env); return true;
    case 0x007D19B0u: eaxOut = aiAttackAimAtTargetUpdate(self, env); return true;
    case 0x007DABC0u: eaxOut = aiPlayerSlot26(self, eaxIn, env); return true;
    case 0x007DAD30u: eaxOut = aiPlayerSlot5(self, env); return true;
    case 0x007DC1B0u: eaxOut = aiTNGuardReturnUpdate(self, env); return true;
    case 0x007D0B30u: eaxOut = aiDockProcessDockUpdate(self, env); return true;
    case 0x007DB6C0u: eaxOut = aiFollowWaypointEvacuateUpdate(self, env); return true;
    case 0x007DBF70u: eaxOut = aiMoveToAndEvacuateUpdate(self, env); return true;
    case 0x007D1A00u: eaxOut = aiTunnelNetworkGuardXfer(self, eaxIn, arg0, env); return true;
    case 0x007D1AA0u: eaxOut = aiTunnelNetworkGuardOnEnter(self, env); return true;
    case 0x007DC680u: eaxOut = squadXfer(self, arg0, env); return true;
    case 0x007DA660u: eaxOut = workOrderXfer(self, eaxIn, arg0, env); return true;
    case 0x007DBBC0u: eaxOut = aiChargeTargetOnEnter(self, env); return true;
    case 0x007D19C0u: eaxOut = aiStateMachineNewSlot9(self, arg0, env); return true;
    case 0x007D0980u: eaxOut = aiStateMachineNewSlot19(self, arg0, arg1, env); return true;
    case 0x007D09B0u: eaxOut = aiStateMachineNewSlot20(self, arg0, env); return true;
    case 0x007D0AA0u: eaxOut = unitAIDisguiseHoldGroundClone(self, env); return true;
    case 0x007D09E0u: eaxOut = aiStateMachineNewSlot21(self, arg0, env); return true;
    case 0x007D0A00u: eaxOut = disguiseHoldGroundConstruct(self, arg0, env); return true;
    case 0x007DB530u: eaxOut = aiBackAwayUpdate(self, env); return true;
    case 0x007DB430u: eaxOut = aiCowerUpdate(self, env); return true;
    case 0x007DE580u: eaxOut = aiFaceStateUpdateWide(self, env); return true;
    case 0x007DFE70u: eaxOut = aiAttackSquadOnEnter(self, env); return true;
    case 0x007D0D00u: eaxOut = aiGuardReturnOnEnter(self, env); return true;
    case 0x007DBB20u: eaxOut = aiFaceStateUpdate(self, env); return true;
    case 0x007DA4D0u: eaxOut = aiPlayerSlot16(self, env); return true;
    case 0x007DA590u: eaxOut = aiPlayerSlot19(self, env); return true;
    case 0x007D6090u: eaxOut = turretAIAimUpdate(self, env); return true;
    case 0x007DF320u: eaxOut = unitAIPickTargetApplySpan(self, arg0, env); return true;
    case 0x007DFCF0u: eaxOut = unitAIPickTargetOnEnter(self, env); return true;
    case 0x007D1860u: eaxOut = aiAttackAimOnEnter(self, env); return true;
    case 0x007DFF50u: eaxOut = aiAttackSquadUpdate(self, env); return true;
    case 0x007D1D60u: eaxOut = turretAIXfer(self, arg0, env); return true;
    case 0x007DCAA0u: eaxOut = aiAttackSquadBindTarget(self, arg0, env); return true;
    case 0x007D2F90u: eaxOut = turretAIReadAim(self, arg0, arg1, env); return true;
    case 0x007DB090u: eaxOut = aiIdleUpdate(self, env); return true;
    default: return false;
    }
}

}  // namespace ra3::gamelogic
