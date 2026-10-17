#include "ra3/gamelogic/AIState79D.h"

#include <cmath>
#include <cstring>

namespace ra3::gamelogic {
namespace {

constexpr std::uint32_t kDone = 0xFFFFFFFFu;
constexpr std::uint32_t kFail = 0xFFFFFFFEu;
constexpr std::uint32_t kTwo32 = 0x4F800000u;
constexpr std::uint32_t kLogicStep = 15u;
constexpr std::uint32_t kIdleScan = 0x00C2A938u;
constexpr std::uint32_t kAiStates = 0x00C2C1A8u;
constexpr std::uint32_t kTnGuard = 0x00C2D5A8u;
constexpr std::uint32_t kPi = 0x40490FDBu;
constexpr std::uint32_t kTwoPi = 0x40C90FDBu;
constexpr std::uint32_t kNegPi = 0xC0490FDBu;
constexpr std::uint32_t kSmallAngle = 0x3E20D97Cu;
constexpr std::uint32_t kTenth = 0x3DCCCCCDu;
constexpr std::uint32_t kNegZero = 0x80000000u;
constexpr std::uint32_t kFreeFallBias = 0x40A00000u;
constexpr std::uint32_t kCallback = 0x00797040u;

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

std::int32_t truncTowardZero(float value)
{
    return static_cast<std::int32_t>(value);
}

std::uint32_t withLow(std::uint32_t value, std::uint32_t low)
{
    return (value & 0xFFFFFF00u) | (low & 0xFFu);
}

std::uint32_t machineOf(const std::uint8_t* self)
{
    return load32(self, 0x18);
}

std::uint32_t unitOf(std::uint32_t machine)
{
    return load32(asPtr(machine), 0x24);
}

std::uint32_t frameOf(const AIState79DEnv& box)
{
    return load32(asPtr(box.env.gameLogic), 0x58);
}

std::uint32_t call0(AIState79DEnv& box, std::uint32_t target, std::uint32_t ecx)
{
    return callExternal(box.env, target, ecx, nullptr, 0);
}

std::uint32_t call1(AIState79DEnv& box, std::uint32_t target, std::uint32_t ecx, std::uint32_t arg)
{
    return callExternal(box.env, target, ecx, &arg, 1);
}

std::uint32_t callN(AIState79DEnv& box, std::uint32_t target, std::uint32_t ecx,
                    const std::uint32_t* args, int argc)
{
    return callExternal(box.env, target, ecx, args, argc);
}

std::uint32_t callVirtual(AIState79DEnv& box, std::uint32_t obj, std::uint32_t slot,
                          const std::uint32_t* args, int argc)
{
    const auto function = load32(asPtr(load32(asPtr(obj), 0)), slot);
    return callExternal(box.env, function, obj, args, argc);
}

void publish(AIState79DEnv& box, void* data, std::uint32_t bytes)
{
    box.env.blob = static_cast<std::uint8_t*>(data);
    box.env.blobBytes = bytes;
}

void clearBlob(AIState79DEnv& box)
{
    box.env.blob = nullptr;
    box.env.blobBytes = 0;
}

std::uint32_t selfAddr(const std::uint8_t* self)
{
    return addressOf(self);
}

std::uint32_t setGoal(AIState79DEnv& box, std::uint32_t child, std::uint32_t position)
{
    const std::uint32_t args[] = {position, box.env.maxFloatBits};
    return callN(box, 0x005AC4F0u, child, args, 2);
}

std::uint32_t enterChild(AIState79DEnv& box, std::uint32_t child)
{
    const auto id = load32(asPtr(child), 0x68);
    return callVirtual(box, child, 0x1C, &id, 1);
}

std::uint32_t spawnChild(AIState79DEnv& box, std::uint8_t* self)
{
    const auto child = callVirtual(box, selfAddr(self), 0x4C, nullptr, 0);
    store32(self, 0x20, child);
    return child;
}

std::uint32_t xferVersion(AIState79DEnv& box, std::uint32_t xfer)
{
    std::uint8_t version[2] = {1, 1};
    publish(box, version, 2);
    const auto pointer = addressOf(version);
    const auto result = callVirtual(box, xfer, 0x18, &pointer, 1);
    clearBlob(box);
    return result;
}

std::uint32_t xferField(AIState79DEnv& box, std::uint32_t xfer, std::uint32_t slot, std::uint32_t field)
{
    return callVirtual(box, xfer, slot, &field, 1);
}

// 0x007970D0 UnitAIReloadAmmoState slot 5. Unsigned ratio, then 0x006EC710.
std::uint32_t unitAIReloadAmmoUpdate(std::uint8_t* self, std::uint32_t, const std::uint32_t*, int, AIState79DEnv& box)
{
    const auto weapon = load32(asPtr(unitOf(machineOf(self))), 0x37C);
    if (weapon == 0) {
        return kFail;
    }
    const auto frame = frameOf(box);
    const auto deadline = load32(self, 0x24);
    const auto duration = load32(self, 0x20);
    std::uint32_t sample = 0x3F800000u;
    if (frame < deadline) {
        const auto elapsed = duration - deadline + frame;
        const auto numerator = static_cast<std::int32_t>(elapsed);
        const auto denominator = static_cast<std::int32_t>(duration);
        float top = static_cast<float>(numerator);
        float bottom = static_cast<float>(denominator);
        if (numerator < 0) {
            top += bitsToFloat(kTwo32);
        }
        if (denominator < 0) {
            bottom += bitsToFloat(kTwo32);
        }
        sample = floatToBits(top / bottom);
    }
    const auto result = call1(box, 0x006EC710u, weapon, sample);
    return (result & 0xFFu) != 0 ? kDone : 0u;
}

// 0x00797A10 TurretAIIdleScanState slot 4. Pushes TurretStates\IdleScan.cpp.
std::uint32_t turretAIIdleScanOnEnter(std::uint8_t* self, std::uint32_t, const std::uint32_t*, int, AIState79DEnv& box)
{
    const auto node = load32(asPtr(machineOf(self)), 0x80);
    const auto start = load32(asPtr(node), 0x44);
    const auto end = load32(asPtr(node), 0x48);
    if (bitsToFloat(start) == 0.f && bitsToFloat(end) == 0.f) {
        return kDone;
    }
    const auto delta = floatToBits(bitsToFloat(end) - bitsToFloat(start));
    const std::uint32_t span[] = {0u, delta, kIdleScan, 0x37u};
    const auto randomBits = callN(box, 0x005A9550u, selfAddr(self), span, 4);
    auto heading = floatToBits(bitsToFloat(randomBits) + bitsToFloat(start));
    const std::uint32_t coin[] = {0u, 1u, kIdleScan, 0x38u};
    const auto side = callN(box, 0x005A94C0u, selfAddr(self), coin, 4);
    if (side == 0) {
        heading = floatToBits(bitsToFloat(kNegZero) - bitsToFloat(heading));
    }
    store32(self, 0x20, heading);
    return 0;
}

// 0x007985D0. Tail of SA_AIFaceTargetState::update. Wraps the heading delta into (-pi, pi].
std::uint32_t saAIFaceTargetApplyHeading(std::uint8_t* self, std::uint32_t, const std::uint32_t*, int, AIState79DEnv& box)
{
    const auto machine = machineOf(self);
    const auto original = load32(asPtr(machine), 0x54);
    const auto unit = unitOf(machine);
    const auto pi = bitsToFloat(kPi);
    const auto twoPi = bitsToFloat(kTwoPi);
    float delta = bitsToFloat(original) - bitsToFloat(load32(asPtr(unit), 0x44));
    while (delta > pi) {
        delta -= twoPi;
    }
    const auto negPi = bitsToFloat(kNegPi);
    while (delta <= negPi) {
        delta += twoPi;
    }
    const auto magnitude = std::fabs(delta);
    const auto limit = bitsToFloat(kSmallAngle);
    if (!(magnitude < limit)) {
        const auto loco = load32(asPtr(unit), 0x374);
        callVirtual(box, loco, 0x26C, &original, 1);
        return 0;
    }
    call1(box, 0x0078C080u, unit, original);
    return kDone;
}

// 0x00799410 AIGarrisonedIdleState slot 4. Pushes AIStates.cpp.
std::uint32_t aiGarrisonedIdleOnEnter(std::uint8_t* self, std::uint32_t, const std::uint32_t*, int, AIState79DEnv& box)
{
    const std::uint32_t rollArgs[] = {2u, 4u, kAiStates, 0x3C3u};
    const auto rolled = static_cast<std::int32_t>(callN(box, 0x005A94C0u, selfAddr(self), rollArgs, 4));
    auto scaled = kLogicStep * static_cast<std::uint32_t>(rolled);
    if (box.env.gameLogic != 0) {
        const auto product = static_cast<float>(static_cast<std::int32_t>(scaled))
            * bitsToFloat(load32(asPtr(box.env.gameLogic), 0x1C4));
        scaled = static_cast<std::uint32_t>(truncTowardZero(product));
    }
    store32(self, 0x20, scaled);
    const std::uint32_t notice[] = {0x8Eu, 0u};
    callN(box, 0x006D2020u, unitOf(machineOf(self)), notice, 2);
    return 0;
}

// 0x0079B1C0 AIAttackAreaState slot 4. Pushes AIStates.cpp and tail-calls slot 0x14.
std::uint32_t aiAttackAreaOnEnter(std::uint8_t* self, std::uint32_t, const std::uint32_t*, int, AIState79DEnv& box)
{
    const auto machine = machineOf(self);
    const std::uint32_t zero = 0;
    const auto sub = callVirtual(box, machine, 0x20, &zero, 1);
    store32(self, 0x20, sub);
    const auto product = bitsToFloat(load32(asPtr(box.env.gameLogic), 0x1C4)) * bitsToFloat(box.scaleBits);
    const auto extra = static_cast<std::uint32_t>(truncTowardZero(product));
    const std::uint32_t rollArgs[] = {0u, extra, kAiStates, 0x3F7Cu};
    const auto rolled = callN(box, 0x005A94C0u, machine, rollArgs, 4);
    store32(self, 0x24, rolled + frameOf(box));
    return callVirtual(box, sub, 0x14, nullptr, 0);
}

// 0x0079B690 AITNGuardIdleState slot 4. Pushes AITNGuard.cpp.
std::uint32_t aiTNGuardIdleOnEnter(std::uint8_t* self, std::uint32_t, const std::uint32_t*, int, AIState79DEnv& box)
{
    const auto outer = load32(asPtr(box.partition), 0x38);
    const auto record = load32(asPtr(outer), 0);
    const auto sample = record == 0 ? 0 : static_cast<std::int32_t>(load32(asPtr(record), 0x2C));
    const auto product = static_cast<float>(sample) * bitsToFloat(load32(asPtr(box.env.gameLogic), 0x1C4));
    const auto ticks = static_cast<std::uint32_t>(truncTowardZero(product));
    const std::uint32_t rollArgs[] = {0u, ticks, kTnGuard, 0x25Cu};
    const auto rolled = callN(box, 0x005A94C0u, box.env.gameLogic, rollArgs, 4);
    store32(self, 0x20, rolled + frameOf(box));
    const auto loco = load32(asPtr(unitOf(machineOf(self))), 0x374);
    call1(box, 0x00682D90u, loco, 0);
    return 0;
}

// 0x0079C5D0 UnitAIGetBoardingLocationState slot 4.
std::uint32_t unitAIGetBoardingLocationOnEnter(std::uint8_t* self, std::uint32_t, const std::uint32_t*, int,
                                               AIState79DEnv& box)
{
    const auto machine = machineOf(self);
    const auto goal = call0(box, 0x005B9F30u, machine);
    if (goal == 0) {
        return kFail;
    }
    const auto unit = unitOf(machine);
    std::uint32_t local[3] = {
        load32(asPtr(unit), 0x38), load32(asPtr(unit), 0x3C), load32(asPtr(unit), 0x40)};
    publish(box, local, sizeof(local));
    const auto loco = load32(asPtr(goal), 0x374);
    const std::uint32_t probe[] = {addressOf(local), 0u};
    const auto placed = callN(box, 0x00741480u, loco, probe, 2);
    std::uint32_t position = goal + 0x38u;
    if ((placed & 0xFFu) != 0) {
        std::uint32_t replaced[3] = {};
        publish(box, replaced, sizeof(replaced));
        const std::uint32_t query[] = {goal, loco + 0x1F4u, addressOf(replaced)};
        const auto hit = callN(box, 0x0063B510u, addressOf(replaced), query, 3);
        if ((hit & 0xFFu) != 0) {
            local[0] = replaced[0];
            local[1] = replaced[1];
            local[2] = replaced[2];
        }
        position = addressOf(local);
        publish(box, local, sizeof(local));
    }
    setGoal(box, machine, position);
    clearBlob(box);
    return kDone;
}

// 0x0079C6E0 UnitAIHeliEvacuateState slot 4.
std::uint32_t unitAIHeliEvacuateOnEnter(std::uint8_t* self, std::uint32_t, const std::uint32_t*, int, AIState79DEnv& box)
{
    const auto child = spawnChild(box, self);
    const auto machine = machineOf(self);
    std::uint32_t goal[3] = {
        load32(asPtr(machine), 0x2C), load32(asPtr(machine), 0x30), load32(asPtr(machine), 0x34)};
    const auto unit = unitOf(machine);
    const auto carrier = call0(box, 0x006A7600u, unit);
    std::uint32_t record = 0;
    if (carrier != 0) {
        publish(box, &record, 4);
        const std::uint32_t args[] = {kCallback, addressOf(&record), 1u};
        callN(box, load32(asPtr(load32(asPtr(carrier), 0)), 0x108), carrier, args, 3);
        clearBlob(box);
    }
    if (record != 0) {
        std::uint32_t adjusted[3] = {goal[0], goal[1], goal[2]};
        publish(box, adjusted, sizeof(adjusted));
        const auto park = load32(asPtr(record), 0x374) + 0x1F4u;
        const std::uint32_t query[] = {record, park, addressOf(adjusted)};
        const auto hit = callN(box, 0x0063B510u, addressOf(adjusted), query, 3);
        if ((hit & 0xFFu) != 0) {
            goal[0] = adjusted[0];
            goal[1] = adjusted[1];
            goal[2] = adjusted[2];
        }
        clearBlob(box);
    }
    std::uint32_t out = 0;
    publish(box, &out, 4);
    const std::uint32_t height[] = {goal[0], goal[1], addressOf(&out), 0u};
    const auto grounded = callN(box, 0x0085CBC0u, box.pathfinder, height, 4);
    clearBlob(box);
    if ((grounded & 0xFFu) != 0 && !(bitsToFloat(out) < bitsToFloat(goal[2]))) {
        return kFail;
    }
    publish(box, goal, sizeof(goal));
    setGoal(box, child, addressOf(goal));
    clearBlob(box);
    return enterChild(box, child);
}

std::uint32_t deliverRate(AIState79DEnv& box, std::uint32_t templ)
{
    const auto raw = truncTowardZero(bitsToFloat(load32(asPtr(templ), 0xC)));
    auto scaled = static_cast<std::int32_t>(kLogicStep) * raw;
    if (box.env.gameLogic != 0) {
        const auto product = static_cast<float>(scaled) * bitsToFloat(load32(asPtr(box.env.gameLogic), 0x1C4));
        scaled = truncTowardZero(product);
    }
    return static_cast<std::uint32_t>(scaled);
}

// 0x0079D030 UnitAIDeliverToRefineryState slot 5.
std::uint32_t unitAIDeliverToRefineryUpdate(std::uint8_t* self, std::uint32_t, const std::uint32_t*, int,
                                            AIState79DEnv& box)
{
    const auto machine = machineOf(self);
    const auto unit = unitOf(machine);
    const auto module = load32(asPtr(unit), 0x39C);
    const auto holder = call0(box, 0x00686FA0u, unit);
    const auto broker = call1(box, 0x007E4AB0u, holder, 0);
    const auto templ = load32(asPtr(module), 4);
    const auto rate = deliverRate(box, templ);
    auto field = load32(asPtr(templ), 8);
    if (rate != 0) {
        field /= rate;
    }
    if (broker != 0 && (call1(box, 0x007E1480u, broker, field) & 0xFFu) == 0) {
        store32(self, 0x20, load32(self, 0x20) + 1u);
        return 0;
    }
    const auto goal = call0(box, 0x005B9F30u, machine);
    std::uint32_t target = 0;
    if (goal != 0) {
        target = load32(asPtr(goal), 0x394);
        if (load32(asPtr(goal), 0x26C) != 0) {
            store32(self, 0x20, load32(self, 0x20) + 1u);
            return 0;
        }
        const std::uint32_t args[] = {unit, rate, load32(self, 0x20)};
        callN(box, 0x0075AD90u, target, args, 3);
    }
    auto pushBroker = [&](std::uint32_t amount) {
        const auto bits = call1(box, 0x006F6EB0u, module, amount);
        const auto truncated = static_cast<std::uint32_t>(truncTowardZero(bitsToFloat(bits)));
        std::uint32_t stored = load32(asPtr(unit), 0xDC);
        publish(box, &stored, 4);
        const std::uint32_t args[] = {
            2u, truncated, addressOf(&stored), load32(asPtr(holder), 0x1F0), 1u, 0u};
        callN(box, 0x007F58B0u, broker, args, 6);
        clearBlob(box);
    };
    if (broker != 0) {
        pushBroker(field);
    }
    if (frameOf(box) >= load32(self, 0x20)) {
        if (broker != 0) {
            const auto amount = call0(box, 0x0068D650u, module);
            pushBroker(amount);
        }
        call0(box, 0x0070B050u, module);
        call0(box, 0x006C6D40u, module);
        if (target != 0) {
            call0(box, 0x0067EBE0u, target);
        }
        return kDone;
    }
    call0(box, 0x006C6D40u, module);
    return 0;
}

// 0x0079D320 UnitAIGetBestBoneState slot 5.
std::uint32_t unitAIGetBestBoneUpdate(std::uint8_t* self, std::uint32_t, const std::uint32_t*, int, AIState79DEnv& box)
{
    const auto machine = machineOf(self);
    const auto goal = call0(box, 0x005B9F30u, machine);
    if (goal == 0) {
        return kFail;
    }
    const auto record = load32(asPtr(goal), 0x394);
    if (record == 0) {
        return kFail;
    }
    const auto unit = unitOf(machine);
    const auto module = load32(asPtr(unit), 0x39C);
    const std::uint32_t query[] = {load32(asPtr(module), 0x1C), load32(asPtr(unit), 0xDC)};
    const auto index = callN(box, 0x0067E970u, record, query, 2);
    store32(asPtr(module), 0x1C, index);
    if (static_cast<std::int32_t>(index) < 0x3E8) {
        return kDone;
    }
    std::uint32_t point[3] = {};
    publish(box, point, sizeof(point));
    const std::uint32_t fetch[] = {index, addressOf(point)};
    callN(box, 0x0067E7C0u, record, fetch, 2);
    setGoal(box, machine, addressOf(point));
    clearBlob(box);
    asPtr(machine)[0x7C] = 1;
    return kFail;
}

// 0x0079D5C0 UnitAIDriftState slot 5.
std::uint32_t unitAIDriftUpdate(std::uint8_t* self, std::uint32_t, const std::uint32_t*, int, AIState79DEnv& box)
{
    const auto machine = machineOf(self);
    const auto unit = unitOf(machine);
    std::uint32_t local[3] = {
        load32(asPtr(unit), 0x38), load32(asPtr(unit), 0x3C), load32(asPtr(unit), 0x40)};
    std::uint32_t pack[4] = {local[0], local[1], local[2], 0};
    publish(box, pack, sizeof(pack));
    const std::uint32_t locate[] = {unit, addressOf(pack)};
    const auto found = callN(box, 0x00861CB0u, box.terrain, locate, 2);
    pack[3] = found;
    const std::uint32_t measure[] = {addressOf(pack), addressOf(pack + 3)};
    const auto heightBits = callN(box, 0x0079D520u, selfAddr(self), measure, 2);
    clearBlob(box);
    local[0] = pack[0];
    local[1] = pack[1];
    local[2] = pack[2];
    const auto limit = bitsToFloat(local[2]) - bitsToFloat(load32(asPtr(box.env.logicTable), 0x6C));
    if (!(bitsToFloat(heightBits) < limit)) {
        const auto holder = call0(box, 0x00686FA0u, unit);
        if (holder != 0) {
            const auto row = load32(asPtr(holder), 0x10);
            if (row != 0) {
                const auto method = load32(asPtr(load32(asPtr(row), 0)), 0x5C);
                if (method != 0) {
                    const std::uint32_t args[] = {unit, 0u, 0u};
                    callN(box, 0x006A7AE0u, method, args, 3);
                }
            }
        }
        const std::uint32_t layer[] = {unit + 0x38u, 0u};
        const auto picked = callN(box, 0x00861E30u, box.terrain, layer, 2);
        call1(box, 0x00687380u, unit, picked);
        return kDone;
    }
    publish(box, local, sizeof(local));
    call1(box, 0x0078D1E0u, unit, addressOf(local));
    clearBlob(box);
    return 0;
}

// 0x0079D6A0 UnitAIFreeFallState slot 5.
std::uint32_t unitAIFreeFallUpdate(std::uint8_t* self, std::uint32_t, const std::uint32_t*, int, AIState79DEnv& box)
{
    const auto unit = unitOf(machineOf(self));
    std::uint32_t local[3] = {
        load32(asPtr(unit), 0x38), load32(asPtr(unit), 0x3C), load32(asPtr(unit), 0x40)};
    publish(box, local, sizeof(local));
    const std::uint32_t args[] = {addressOf(local), 0u};
    const auto heightBits = callVirtual(box, box.terrain, 0x1C, args, 2);
    clearBlob(box);
    const auto limit = bitsToFloat(local[2]) - bitsToFloat(kFreeFallBias);
    if (!(bitsToFloat(heightBits) < limit)) {
        return kFail;
    }
    publish(box, local, sizeof(local));
    call1(box, 0x0078D1E0u, unit, addressOf(local));
    clearBlob(box);
    const auto frame = frameOf(box);
    return frame < load32(self, 0x20) ? 0u : kDone;
}

// 0x0079D8F0 SA_AIFaceTargetState slot 5. Adds the facing, then jumps to 0x007985D0.
std::uint32_t saAIFaceTargetUpdate(std::uint8_t* self, std::uint32_t eax, const std::uint32_t* stack, int argc,
                                   AIState79DEnv& box)
{
    const auto machine = machineOf(self);
    const auto goal = call0(box, 0x005B9F30u, machine);
    const auto position = goal != 0 ? goal + 0x38u : machine + 0x2Cu;
    const auto unit = unitOf(machine);
    const auto facing = call1(box, 0x006A6CB0u, unit, position);
    const auto sum = floatToBits(bitsToFloat(facing) + bitsToFloat(load32(asPtr(unit), 0x44)));
    call1(box, 0x005AA280u, machine, sum);
    return saAIFaceTargetApplyHeading(self, eax, stack, argc, box);
}

// 0x0079F400 UnitAIGuardAreaState slot 4. Goal comes from the machine.
std::uint32_t unitAIGuardAreaOnEnter(std::uint8_t* self, std::uint32_t, const std::uint32_t*, int, AIState79DEnv& box)
{
    const auto machine = machineOf(self);
    const auto unit = unitOf(machine);
    const auto child = spawnChild(box, self);
    std::uint32_t local[3] = {
        load32(asPtr(machine), 0x2C), load32(asPtr(machine), 0x30), load32(asPtr(machine), 0x34)};
    publish(box, local, sizeof(local));
    const auto point = addressOf(local);
    setGoal(box, child, point);
    const auto loco = load32(asPtr(unit), 0x374);
    const auto airborne = callVirtual(box, loco, 0x250, nullptr, 0);
    if ((airborne & 0xFFu) == 0) {
        call1(box, 0x005AA280u, child, load32(asPtr(unit), 0x44));
    }
    callVirtual(box, loco, 0x228, &point, 1);
    clearBlob(box);
    const std::uint32_t notice[] = {unit, 1u, 0u};
    callN(box, 0x006AEFE0u, loco, notice, 3);
    return enterChild(box, child);
}

// 0x0079F4C0 UnitAIGuardPositionState slot 4. Also UnitAIDisguiseGuardPositionState slot 4.
std::uint32_t unitAIGuardPositionOnEnter(std::uint8_t* self, std::uint32_t, const std::uint32_t*, int, AIState79DEnv& box)
{
    const auto unit = unitOf(machineOf(self));
    const auto child = spawnChild(box, self);
    std::uint32_t local[3] = {
        load32(asPtr(unit), 0x38), load32(asPtr(unit), 0x3C), load32(asPtr(unit), 0x40)};
    publish(box, local, sizeof(local));
    const auto point = addressOf(local);
    setGoal(box, child, point);
    asPtr(child)[0x7C] = 0;
    const auto loco = load32(asPtr(unit), 0x374);
    const auto airborne = callVirtual(box, loco, 0x250, nullptr, 0);
    if ((airborne & 0xFFu) == 0) {
        call1(box, 0x005AA280u, child, load32(asPtr(unit), 0x44));
    }
    callVirtual(box, loco, 0x228, &point, 1);
    clearBlob(box);
    const std::uint32_t notice[] = {unit, 1u, 0u};
    callN(box, 0x006AEFE0u, loco, notice, 3);
    return enterChild(box, child);
}

// 0x0079FFA0 UnitAIMoveToGoalAndEnterState slot 4. The body continues through 0x007A008C.
std::uint32_t unitAIMoveToGoalAndEnterOnEnter(std::uint8_t* self, std::uint32_t, const std::uint32_t*, int,
                                              AIState79DEnv& box)
{
    const auto child = spawnChild(box, self);
    const auto machine = machineOf(self);
    const auto goal = call0(box, 0x005B9F30u, machine);
    if (goal == 0) {
        return kFail;
    }
    std::uint32_t local[3] = {};
    publish(box, local, sizeof(local));
    call1(box, 0x006A76A0u, goal, addressOf(local));
    const std::uint32_t locate[] = {goal, addressOf(local), 0u, 1u};
    const auto found = callN(box, 0x00861CB0u, box.terrain, locate, 4);
    const std::uint32_t sample[] = {0u, 0u, found};
    callVirtual(box, box.terrain, 0x24, sample, 3);
    setGoal(box, child, addressOf(local));
    store32(asPtr(child), 0x38, local[0]);
    store32(asPtr(child), 0x3C, local[1]);
    store32(asPtr(child), 0x40, local[2]);
    callVirtual(box, child, 0x38, &goal, 1);
    const auto passenger = load32(asPtr(goal), 0xDC);
    const auto unit = unitOf(machine);
    const auto loco = load32(asPtr(unit), 0x374);
    call1(box, 0x00683140u, loco, passenger);
    const std::uint32_t approach[] = {unit, 1u};
    const auto dock = callN(box, 0x006A7620u, goal, approach, 2);
    if (dock != 0) {
        const std::uint32_t args[] = {unit, 0u};
        callVirtual(box, dock, 0x40, args, 2);
    }
    clearBlob(box);
    return enterChild(box, child);
}

// 0x0079F580 UnitAIHoldGroundState slot 4.
std::uint32_t unitAIHoldGroundOnEnter(std::uint8_t* self, std::uint32_t, const std::uint32_t*, int, AIState79DEnv& box)
{
    const auto machine = machineOf(self);
    const auto unit = unitOf(machine);
    const auto child = spawnChild(box, self);
    std::uint32_t local[3] = {
        load32(asPtr(unit), 0x38), load32(asPtr(unit), 0x3C), load32(asPtr(unit), 0x40)};
    publish(box, local, sizeof(local));
    const auto point = addressOf(local);
    setGoal(box, child, point);
    const auto loco = load32(asPtr(unit), 0x374);
    callVirtual(box, loco, 0x228, &point, 1);
    clearBlob(box);
    const std::uint32_t notice[] = {unit, 1u, 0u};
    callN(box, 0x006AEFE0u, unit, notice, 3);
    return enterChild(box, child);
}

// 0x0079DA20 AIAggressiveState slot 4.
std::uint32_t aiAggressiveOnEnter(std::uint8_t* self, std::uint32_t, const std::uint32_t*, int, AIState79DEnv& box)
{
    const auto unit = unitOf(machineOf(self));
    const auto child = spawnChild(box, self);
    std::uint32_t local[3] = {
        load32(asPtr(unit), 0x38), load32(asPtr(unit), 0x3C), load32(asPtr(unit), 0x40)};
    publish(box, local, sizeof(local));
    const auto point = addressOf(local);
    setGoal(box, child, point);
    const auto loco = load32(asPtr(unit), 0x374);
    callVirtual(box, loco, 0x228, &point, 1);
    clearBlob(box);
    const std::uint32_t notice[] = {unit, 1u, 0u};
    callN(box, 0x006AEFE0u, loco, notice, 3);
    return enterChild(box, child);
}

// 0x0079B280 AIFaceState slot 4. cmp 2 tail-calls 0x00799890.
std::uint32_t aiFaceOnEnter(std::uint8_t* self, std::uint32_t eax, const std::uint32_t*, int, AIState79DEnv& box)
{
    const auto machine = machineOf(self);
    const auto unit = unitOf(machine);
    const auto probe = load32(asPtr(load32(asPtr(unit), 0x374)), 0x214);
    std::uint32_t flag = 0;
    if (probe != 0) {
        const auto bits = call1(box, 0x00706820u, probe, unit);
        flag = bitsToFloat(bits) == 0.f ? 1u : 0u;
    }
    self[0x2C] = static_cast<std::uint8_t>(flag);
    const auto goal = call0(box, 0x005B9F30u, machine);
    const auto mode = load32(self, 0x28);
    if (mode != 0 && goal == 0) {
        return kFail;
    }
    if (mode == 2) {
        return call0(box, 0x00799890u, selfAddr(self));
    }
    (void)eax;
    return 0;
}

std::uint32_t moveToGoalCommon(std::uint8_t* self, AIState79DEnv& box, bool angle, bool copyByte, bool dive)
{
    const auto child = spawnChild(box, self);
    const auto machine = machineOf(self);
    publish(box, asPtr(machine + 0x2C), 12);
    setGoal(box, child, machine + 0x2Cu);
    clearBlob(box);
    if (angle) {
        call1(box, 0x005AA280u, child, load32(asPtr(machine), 0x54));
    }
    if (copyByte) {
        asPtr(child)[0x7C] = asPtr(machine)[0x7C];
    }
    if (dive) {
        call1(box, 0x007576C0u, unitOf(machine), 1);
    }
    return enterChild(box, child);
}

// 0x0079C990 AIMoveToGoalDiveBombState slot 4.
std::uint32_t aiMoveToGoalDiveBombOnEnter(std::uint8_t* self, std::uint32_t, const std::uint32_t*, int, AIState79DEnv& box)
{
    return moveToGoalCommon(self, box, true, true, true);
}

// 0x0079CB00 JetAIMoveToGoalState slot 4.
std::uint32_t jetAIMoveToGoalOnEnter(std::uint8_t* self, std::uint32_t, const std::uint32_t*, int, AIState79DEnv& box)
{
    return moveToGoalCommon(self, box, true, true, false);
}

// 0x0079FCD0 UnitAIMoveToAndDeleteState slot 4.
std::uint32_t unitAIMoveToAndDeleteOnEnter(std::uint8_t* self, std::uint32_t, const std::uint32_t*, int, AIState79DEnv& box)
{
    return moveToGoalCommon(self, box, true, false, false);
}

// 0x00797510 UnitAIDriftState slot 6.
std::uint32_t unitAIDriftSlot6(std::uint8_t* self, std::uint32_t, const std::uint32_t*, int, AIState79DEnv& box)
{
    const auto product = bitsToFloat(load32(asPtr(box.env.logicTable), 0x70)) * bitsToFloat(box.scaleBits);
    const auto ticks = static_cast<std::uint32_t>(truncTowardZero(product));
    const std::uint32_t args[] = {0x12u, ticks};
    return callN(box, 0x006F04A0u, unitOf(machineOf(self)), args, 2);
}

// 0x0079FE80 UnitAIMoveToAttackPositionState slot 4.
std::uint32_t unitAIMoveToAttackPositionOnEnter(std::uint8_t* self, std::uint32_t, const std::uint32_t*, int,
                                                AIState79DEnv& box)
{
    const auto child = spawnChild(box, self);
    asPtr(child)[0x7C] = 0;
    const auto machine = machineOf(self);
    publish(box, asPtr(machine + 0x2C), 12);
    setGoal(box, child, machine + 0x2Cu);
    clearBlob(box);
    return enterChild(box, child);
}

// 0x0079CA30 AIMoveToInitialRunState slot 4.
std::uint32_t aiMoveToInitialRunOnEnter(std::uint8_t* self, std::uint32_t, const std::uint32_t*, int, AIState79DEnv& box)
{
    return moveToGoalCommon(self, box, false, false, false);
}

// 0x0079EE10 UnitAIFollowPathState slot 4.
std::uint32_t unitAIFollowPathOnEnter(std::uint8_t* self, std::uint32_t, const std::uint32_t*, int, AIState79DEnv& box)
{
    const auto child = spawnChild(box, self);
    const auto machine = machineOf(self);
    call1(box, 0x005BDCC0u, child, machine + 0x58u);
    call1(box, 0x0079ECE0u, selfAddr(self), child);
    return enterChild(box, child);
}

// 0x0079D260 UnitAIDeliverOreState slot 4.
std::uint32_t unitAIDeliverOreOnEnter(std::uint8_t* self, std::uint32_t, const std::uint32_t*, int, AIState79DEnv& box)
{
    const auto child = spawnChild(box, self);
    const auto goal = call0(box, 0x005B9F30u, machineOf(self));
    callVirtual(box, child, 0x38, &goal, 1);
    return enterChild(box, child);
}

// 0x007989A0 UnitAIWaitForOccupantsState slot 4.
std::uint32_t unitAIWaitForOccupantsOnEnter(std::uint8_t* self, std::uint32_t, const std::uint32_t*, int, AIState79DEnv& box)
{
    const auto unit = unitOf(machineOf(self));
    const std::uint32_t first[] = {0xA8u, 1u};
    const std::uint32_t second[] = {0x4Du, 1u};
    callN(box, 0x006D2020u, unit, first, 2);
    callN(box, 0x006D2020u, unit, second, 2);
    call0(box, 0x006A4EA0u, unit);
    return 0;
}

// 0x00797890 TurretAIAimTurretStateWhenBusy slot 19. True when |angle| <= 0.1.
std::uint32_t turretAIAimTurretWhenBusySlot19(std::uint8_t* self, std::uint32_t, const std::uint32_t* stack, int,
                                              AIState79DEnv& box)
{
    const auto bits = call1(box, 0x006A6CB0u, unitOf(machineOf(self)), stack[0]);
    const auto magnitude = std::fabs(bitsToFloat(bits));
    const auto low = (magnitude < bitsToFloat(kTenth) || magnitude == bitsToFloat(kTenth)) ? 1u : 0u;
    return withLow(bits, low);
}

// 0x007986E0 UnitAIRotateToPathState slot 6.
std::uint32_t unitAIRotateToPathSlot6(std::uint8_t* self, std::uint32_t, const std::uint32_t*, int, AIState79DEnv& box)
{
    const auto result = call1(box, 0x005AA280u, machineOf(self), load32(self, 0x20));
    store32(self, 0x20, 0);
    return result;
}

// 0x0079A1D0 AIAttackFireWeaponState slot 6.
std::uint32_t aiAttackFireWeaponSlot6(std::uint8_t* self, std::uint32_t, const std::uint32_t*, int, AIState79DEnv& box)
{
    const auto unit = unitOf(machineOf(self));
    const std::uint32_t first[] = {0x0Eu, 0u};
    const std::uint32_t second[] = {0x1Du, 0u};
    const std::uint32_t third[] = {0x55u, 0u};
    callN(box, 0x006D2020u, unit, first, 2);
    callN(box, 0x006D2020u, unit, second, 2);
    return callN(box, 0x006D2020u, unit, third, 2);
}

std::uint32_t linkSlot20(AIState79DEnv& box, std::uint32_t link, std::uint32_t emptyLow, bool nullIsOne)
{
    if (link == 0) {
        return nullIsOne ? 1u : 0u;
    }
    const auto obj = load32(asPtr(link), 4);
    if (obj == 0) {
        return withLow(link, emptyLow);
    }
    return callVirtual(box, obj, 0x20, nullptr, 0);
}

// 0x00799EC0 AIFollowPathAsTeamState slot 9. Inverts the linked slot-0x20 byte.
std::uint32_t aiFollowPathAsTeamSlot9(std::uint8_t* self, std::uint32_t, const std::uint32_t*, int, AIState79DEnv& box)
{
    const auto link = load32(self, 0x60);
    if (link == 0 || load32(asPtr(link), 4) == 0) {
        return link == 0 ? 0u : withLow(link, 0);
    }
    const auto result = callVirtual(box, load32(asPtr(link), 4), 0x20, nullptr, 0);
    return withLow(result, (result & 0xFFu) != 0 ? 0u : 1u);
}

// 0x0079F110 UnitAIFollowWaypointPathAsTeamState slot 4.
std::uint32_t unitAIFollowWaypointPathAsTeamOnEnter(std::uint8_t* self, std::uint32_t, const std::uint32_t*, int,
                                                    AIState79DEnv& box)
{
    const auto status = call0(box, 0x0079F030u, selfAddr(self));
    if (status == kFail) {
        return status;
    }
    const auto child = load32(self, 0x20);
    const std::uint32_t one = 1;
    callVirtual(box, child, 0x48, &one, 1);
    return status;
}

// 0x0079A480 AIAttackState slot 8.
std::uint32_t aiAttackSlot8(std::uint8_t* self, std::uint32_t, const std::uint32_t*, int, AIState79DEnv& box)
{
    return linkSlot20(box, load32(self, 0x24), 1, false);
}

// 0x0079C960 AIMoveToFollowThroughState slot 4.
std::uint32_t aiMoveToFollowThroughOnEnter(std::uint8_t* self, std::uint32_t, const std::uint32_t*, int, AIState79DEnv& box)
{
    const auto child = spawnChild(box, self);
    return enterChild(box, child);
}

// 0x00799EF0 AIAttackMoveToState slot 9.
std::uint32_t aiAttackMoveToSlot9(std::uint8_t* self, std::uint32_t, const std::uint32_t*, int, AIState79DEnv& box)
{
    const auto link = load32(self, 0x58);
    const auto obj = load32(asPtr(link), 4);
    if (obj == 0) {
        return withLow(link, 0);
    }
    const auto result = callVirtual(box, obj, 0x20, nullptr, 0);
    return withLow(result, (result & 0xFFu) != 0 ? 0u : 1u);
}

// 0x0079AF30 AIGarrisonedState slot 8.
std::uint32_t aiGarrisonedSlot8(std::uint8_t* self, std::uint32_t, const std::uint32_t*, int, AIState79DEnv& box)
{
    return linkSlot20(box, load32(self, 0x20), 1, true);
}

// 0x00798860 MoveInStraightLineState slot 6. The same body is slot 6 of the other tables listed in the design note; each of those tables has its own slot-2 getter.
std::uint32_t moveInStraightLineSlot6(std::uint8_t* self, std::uint32_t eax, const std::uint32_t*, int, AIState79DEnv& box)
{
    const auto child = load32(self, 0x20);
    if (child != 0) {
        const std::uint32_t one = 1;
        eax = callVirtual(box, child, 0, &one, 1);
    }
    store32(self, 0x20, 0);
    return eax;
}

// 0x00799AE0 AIBackAwayAndCowerState slot 6.
std::uint32_t aiBackAwayAndCowerSlot6(std::uint8_t* self, std::uint32_t eax, const std::uint32_t*, int, AIState79DEnv& box)
{
    const auto child = load32(self, 0x20);
    if (child == 0) {
        return eax;
    }
    const std::uint32_t one = 1;
    eax = callVirtual(box, child, 0, &one, 1);
    store32(self, 0x20, 0);
    return eax;
}

// 0x00799100 AIHarvestWorkSiteState slot 3.
std::uint32_t aiHarvestWorkSiteXfer(std::uint8_t* self, std::uint32_t, const std::uint32_t* stack, int, AIState79DEnv& box)
{
    const auto xfer = stack[0];
    xferVersion(box, xfer);
    xferField(box, xfer, 0x6C, selfAddr(self) + 0x20u);
    const std::uint32_t pair[] = {xfer, selfAddr(self) + 0x34u};
    callN(box, 0x0054A090u, xfer, pair, 2);
    xferField(box, xfer, 0x64, selfAddr(self) + 0x30u);
    xferField(box, xfer, 0x6C, selfAddr(self) + 0x28u);
    xferField(box, xfer, 0x6C, selfAddr(self) + 0x2Cu);
    return xferField(box, xfer, 0x6C, selfAddr(self) + 0x24u);
}

// 0x0079CE60 UnitAILandState slot 3.
std::uint32_t unitAILandXfer(std::uint8_t* self, std::uint32_t, const std::uint32_t* stack, int, AIState79DEnv& box)
{
    const auto xfer = stack[0];
    xferVersion(box, xfer);
    xferVersion(box, xfer);
    xferField(box, xfer, 0x70, selfAddr(self) + 0x38u);
    xferField(box, xfer, 0x58, selfAddr(self) + 0x20u);
    return xferField(box, xfer, 0x58, selfAddr(self) + 0x2Cu);
}

// 0x00799820 AIIdleState slot 3.
std::uint32_t aiIdleXfer(std::uint8_t* self, std::uint32_t, const std::uint32_t* stack, int, AIState79DEnv& box)
{
    const auto xfer = stack[0];
    xferVersion(box, xfer);
    xferField(box, xfer, 0x74, selfAddr(self) + 0x20u);
    xferField(box, xfer, 0x84, selfAddr(self) + 0x24u);
    xferField(box, xfer, 0x84, selfAddr(self) + 0x25u);
    return xferField(box, xfer, 0x74, selfAddr(self) + 0x22u);
}

// 0x00799F80 AIAttackAimAtTargetState slot 3.
std::uint32_t aiAttackAimAtTargetXfer(std::uint8_t* self, std::uint32_t, const std::uint32_t* stack, int, AIState79DEnv& box)
{
    const auto xfer = stack[0];
    xferVersion(box, xfer);
    xferField(box, xfer, 0x84, selfAddr(self) + 0x21u);
    xferField(box, xfer, 0x84, selfAddr(self) + 0x22u);
    return xferField(box, xfer, 0x84, selfAddr(self) + 0x23u);
}

// 0x0079CA70 JetAttackMoveFollowPathAsTeamStateMachine slot 3.
std::uint32_t jetAttackMoveFollowPathXfer(std::uint8_t* self, std::uint32_t, const std::uint32_t* stack, int,
                                          AIState79DEnv& box)
{
    const auto xfer = stack[0];
    xferVersion(box, xfer);
    xferVersion(box, xfer);
    call1(box, 0x005C0A40u, selfAddr(self), xfer);
    return xferField(box, xfer, 0x70, selfAddr(self) + 0x80u);
}

// 0x00799CA0 AIUncontrollableCower slot 3.
std::uint32_t aiUncontrollableCowerXfer(std::uint8_t* self, std::uint32_t, const std::uint32_t* stack, int,
                                        AIState79DEnv& box)
{
    const auto xfer = stack[0];
    xferVersion(box, xfer);
    xferField(box, xfer, 0x84, selfAddr(self) + 0x20u);
    return xferVersion(box, xfer);
}

// 0x00798D70 AIDockProcessDockState slot 3.
std::uint32_t aiDockProcessDockXfer(std::uint8_t* self, std::uint32_t, const std::uint32_t* stack, int, AIState79DEnv& box)
{
    const auto xfer = stack[0];
    xferVersion(box, xfer);
    const std::uint32_t pair[] = {xfer, selfAddr(self) + 0x24u};
    callN(box, 0x0054A090u, xfer, pair, 2);
    xferField(box, xfer, 0x6C, selfAddr(self) + 0x20u);
    return xferField(box, xfer, 0x6C, selfAddr(self) + 0x28u);
}

// 0x00798700 UnitAIRotateToPathState slot 3.
std::uint32_t unitAIRotateToPathXfer(std::uint8_t* self, std::uint32_t, const std::uint32_t* stack, int, AIState79DEnv& box)
{
    const auto xfer = stack[0];
    xferVersion(box, xfer);
    xferField(box, xfer, 0x64, selfAddr(self) + 0x20u);
    return xferVersion(box, xfer);
}

// 0x0079DAC0 UnitAIAimFireAtObjectState slot 3.
std::uint32_t unitAIAimFireAtObjectXfer(std::uint8_t* self, std::uint32_t, const std::uint32_t* stack, int,
                                        AIState79DEnv& box)
{
    const auto xfer = stack[0];
    xferVersion(box, xfer);
    xferVersion(box, xfer);
    const std::uint32_t pair[] = {xfer, selfAddr(self) + 0x20u};
    return callN(box, 0x00549E50u, xfer, pair, 2);
}

std::uint32_t dispatch(std::uint32_t va, std::uint8_t* self, std::uint32_t eax, const std::uint32_t* stack, int argc,
                       AIState79DEnv& box)
{
    switch (va) {
    case 0x007970D0u: return unitAIReloadAmmoUpdate(self, eax, stack, argc, box);
    case 0x00797A10u: return turretAIIdleScanOnEnter(self, eax, stack, argc, box);
    case 0x007985D0u: return saAIFaceTargetApplyHeading(self, eax, stack, argc, box);
    case 0x00799410u: return aiGarrisonedIdleOnEnter(self, eax, stack, argc, box);
    case 0x0079B1C0u: return aiAttackAreaOnEnter(self, eax, stack, argc, box);
    case 0x0079B690u: return aiTNGuardIdleOnEnter(self, eax, stack, argc, box);
    case 0x0079C5D0u: return unitAIGetBoardingLocationOnEnter(self, eax, stack, argc, box);
    case 0x0079C6E0u: return unitAIHeliEvacuateOnEnter(self, eax, stack, argc, box);
    case 0x0079D030u: return unitAIDeliverToRefineryUpdate(self, eax, stack, argc, box);
    case 0x0079D320u: return unitAIGetBestBoneUpdate(self, eax, stack, argc, box);
    case 0x0079D5C0u: return unitAIDriftUpdate(self, eax, stack, argc, box);
    case 0x0079D6A0u: return unitAIFreeFallUpdate(self, eax, stack, argc, box);
    case 0x0079D8F0u: return saAIFaceTargetUpdate(self, eax, stack, argc, box);
    case 0x0079F400u: return unitAIGuardAreaOnEnter(self, eax, stack, argc, box);
    case 0x0079F4C0u: return unitAIGuardPositionOnEnter(self, eax, stack, argc, box);
    case 0x0079FFA0u: return unitAIMoveToGoalAndEnterOnEnter(self, eax, stack, argc, box);
    case 0x0079F580u: return unitAIHoldGroundOnEnter(self, eax, stack, argc, box);
    case 0x0079DA20u: return aiAggressiveOnEnter(self, eax, stack, argc, box);
    case 0x0079B280u: return aiFaceOnEnter(self, eax, stack, argc, box);
    case 0x0079C990u: return aiMoveToGoalDiveBombOnEnter(self, eax, stack, argc, box);
    case 0x0079CB00u: return jetAIMoveToGoalOnEnter(self, eax, stack, argc, box);
    case 0x0079FCD0u: return unitAIMoveToAndDeleteOnEnter(self, eax, stack, argc, box);
    case 0x00797510u: return unitAIDriftSlot6(self, eax, stack, argc, box);
    case 0x0079FE80u: return unitAIMoveToAttackPositionOnEnter(self, eax, stack, argc, box);
    case 0x0079CA30u: return aiMoveToInitialRunOnEnter(self, eax, stack, argc, box);
    case 0x0079EE10u: return unitAIFollowPathOnEnter(self, eax, stack, argc, box);
    case 0x0079D260u: return unitAIDeliverOreOnEnter(self, eax, stack, argc, box);
    case 0x007989A0u: return unitAIWaitForOccupantsOnEnter(self, eax, stack, argc, box);
    case 0x00797890u: return turretAIAimTurretWhenBusySlot19(self, eax, stack, argc, box);
    case 0x007986E0u: return unitAIRotateToPathSlot6(self, eax, stack, argc, box);
    case 0x0079A1D0u: return aiAttackFireWeaponSlot6(self, eax, stack, argc, box);
    case 0x00799EC0u: return aiFollowPathAsTeamSlot9(self, eax, stack, argc, box);
    case 0x0079F110u: return unitAIFollowWaypointPathAsTeamOnEnter(self, eax, stack, argc, box);
    case 0x0079A480u: return aiAttackSlot8(self, eax, stack, argc, box);
    case 0x0079C960u: return aiMoveToFollowThroughOnEnter(self, eax, stack, argc, box);
    case 0x00799EF0u: return aiAttackMoveToSlot9(self, eax, stack, argc, box);
    case 0x0079AF30u: return aiGarrisonedSlot8(self, eax, stack, argc, box);
    case 0x00798860u: return moveInStraightLineSlot6(self, eax, stack, argc, box);
    case 0x00799AE0u: return aiBackAwayAndCowerSlot6(self, eax, stack, argc, box);
    case 0x00799100u: return aiHarvestWorkSiteXfer(self, eax, stack, argc, box);
    case 0x0079CE60u: return unitAILandXfer(self, eax, stack, argc, box);
    case 0x00799820u: return aiIdleXfer(self, eax, stack, argc, box);
    case 0x00799F80u: return aiAttackAimAtTargetXfer(self, eax, stack, argc, box);
    case 0x0079CA70u: return jetAttackMoveFollowPathXfer(self, eax, stack, argc, box);
    case 0x00799CA0u: return aiUncontrollableCowerXfer(self, eax, stack, argc, box);
    case 0x00798D70u: return aiDockProcessDockXfer(self, eax, stack, argc, box);
    case 0x00798700u: return unitAIRotateToPathXfer(self, eax, stack, argc, box);
    case 0x0079DAC0u: return unitAIAimFireAtObjectXfer(self, eax, stack, argc, box);
    default: return 0;
    }
}

}  // namespace

bool runAIState79D(std::uint32_t va, std::uint8_t* self, std::uint32_t eaxIn, const std::uint32_t* stack, int argc,
                   AIState79DEnv& env, std::uint32_t& eaxOut)
{
    switch (va) {
    case 0x007970D0u: case 0x00797A10u: case 0x007985D0u: case 0x00799410u: case 0x0079B1C0u:
    case 0x0079B690u: case 0x0079C5D0u: case 0x0079C6E0u: case 0x0079D030u: case 0x0079D320u:
    case 0x0079D5C0u: case 0x0079D6A0u: case 0x0079D8F0u: case 0x0079F400u: case 0x0079F4C0u:
    case 0x0079FFA0u: case 0x0079F580u: case 0x0079DA20u: case 0x0079B280u: case 0x0079C990u:
    case 0x0079CB00u: case 0x0079FCD0u: case 0x00797510u: case 0x0079FE80u: case 0x0079CA30u:
    case 0x0079EE10u: case 0x0079D260u: case 0x007989A0u: case 0x00797890u: case 0x007986E0u:
    case 0x0079A1D0u: case 0x00799EC0u: case 0x0079F110u: case 0x0079A480u: case 0x0079C960u:
    case 0x00799EF0u: case 0x0079AF30u: case 0x00798860u: case 0x00799AE0u: case 0x00799100u:
    case 0x0079CE60u: case 0x00799820u: case 0x00799F80u: case 0x0079CA70u: case 0x00799CA0u:
    case 0x00798D70u: case 0x00798700u: case 0x0079DAC0u:
        eaxOut = dispatch(va, self, eaxIn, stack, argc, env);
        return true;
    default:
        return false;
    }
}

}  // namespace ra3::gamelogic
