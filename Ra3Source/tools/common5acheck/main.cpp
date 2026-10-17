#include <ra3/common/StateMachine.h>

#include <cstdint>
#include <cstring>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

namespace {

struct Script {
    std::uint32_t machine[32]{};
    std::uint32_t state[8]{};
    std::uint32_t other[32]{};
    std::uint32_t position[4]{};
    std::uint32_t records[16]{};
    std::uint32_t vector[8]{};
    std::uint8_t blob[0x480]{};
    std::uint32_t depthTransition = 0;
    std::uint32_t depthSleep = 0;
    std::uint32_t frame = 100;
    std::uint32_t findHit = 0;
    std::uint32_t findId = 1;
    std::uint32_t stateRet = 0;
    std::uint32_t enterRet = 0;
    std::uint32_t machineRet = 0;
    std::uint32_t clearCurrent = 0;
    std::uint32_t ownerProbe = 0;
    std::uint32_t ownerTail = 0;
    std::uint32_t assertAl = 1;
    std::uint32_t debugGate = 1;
    std::uint32_t ownerQuery = 0;
    std::uint32_t goalValue = 0;
    std::uint32_t moving = 1;
    std::uint32_t unitGoal = 0;
    std::uint32_t unitKind = 0;
    std::uint32_t moveProbe = 0;
    std::uint32_t canPublish = 0;
    std::uint32_t already = 1;
    std::uint32_t pathRet = 0;
    std::uint32_t pathThis = 0;
    std::uint32_t pathArg = 0;
    std::uint32_t xferCount = 0;
    std::vector<std::uint32_t> conditions;
    std::size_t conditionIndex = 0;
    std::string trace;
};

Script g;

void note(const std::string& text)
{
    if (!g.trace.empty()) {
        g.trace += ',';
    }
    g.trace += text;
}

std::uint32_t hexOf(const std::string& text)
{
    return static_cast<std::uint32_t>(std::stoul(text, nullptr, 16));
}

void write32(void* base, std::uint32_t offset, std::uint32_t value)
{
    std::memcpy(static_cast<char*>(base) + offset, &value, 4);
}

std::uint32_t read32(const void* base, std::uint32_t offset)
{
    std::uint32_t value = 0;
    std::memcpy(&value, static_cast<const char*>(base) + offset, 4);
    return value;
}

std::uint32_t addressOf(const void* pointer)
{
    return static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(pointer));
}

ra3::common::StateMachineHooks hooks()
{
    ra3::common::StateMachineHooks result;
    result.context = &g;
    result.frame = g.frame;
    result.transitionDepth = &g.depthTransition;
    result.sleepDepth = &g.depthSleep;
    result.findState = [](void*, std::uint32_t, std::uint32_t key) -> std::uint32_t {
        note("find " + std::to_string(key) + " " + std::to_string(g.findHit));
        if (g.findHit == 0) {
            return 0;
        }
        write32(g.other, 4, g.findId);
        write32(g.other, 0x18, addressOf(g.machine));
        return addressOf(g.other);
    };
    result.callState = [](void*, std::uint32_t, std::uint32_t slot, std::uint32_t argument) -> std::uint32_t {
        note("st " + std::to_string(slot) + " " + std::to_string(argument));
        if (slot == 0x10 && g.clearCurrent != 0) {
            write32(g.machine, 4, 0);
        }
        if (slot == 0x14) {
            return g.stateRet;
        }
        if (slot == 0x10) {
            return g.enterRet;
        }
        return slot == 0x28 || slot == 0x48 || slot == 0x3C ? g.stateRet : 0u;
    };
    result.callMachine = [](void*, std::uint32_t machine, std::uint32_t slot, std::uint32_t) -> std::uint32_t {
        const auto byte = reinterpret_cast<const std::uint8_t*>(
            static_cast<std::uintptr_t>(machine))[0x78];
        note("mc " + std::to_string(slot) + " " + std::to_string(byte));
        return g.machineRet;
    };
    result.callCondition = [](void*, std::uint32_t, std::uint32_t, std::uint32_t) -> std::uint32_t {
        const std::uint32_t value = g.conditionIndex < g.conditions.size()
                                        ? g.conditions[g.conditionIndex++]
                                        : 0u;
        note("cond " + std::to_string(value & 0xFFu));
        return value;
    };
    result.callExternal = [](void*, std::uint32_t id, std::uint32_t a, std::uint32_t b) -> std::uint32_t {
        note("ext " + std::to_string(id));
        using namespace ra3::common;
        switch (id) {
        case kExtOwnerProbe: return g.ownerProbe;
        case kExtOwnerTail: return g.ownerTail;
        case kExtAssert: return g.assertAl;
        case kExtDebugGate: return g.debugGate;
        case kExtOwnerQuery: return g.ownerQuery;
        case kExtGoalLookup: return g.goalValue;
        case kExtMoving: return g.moving;
        case kExtUnitGoal: return g.unitGoal;
        case kExtUnitKind: return g.unitKind;
        case kExtMoveProbe: return g.moveProbe;
        case kExtCanPublish: return g.canPublish;
        case kExtAlreadyThere: return g.already;
        case kExtPathAssign:
            g.pathThis = a;
            g.pathArg = b;
            return g.pathRet;
        case kExtXferCount: return g.xferCount;
        case kExtXferGrow:
            write32(g.vector, 4, b + 12u);
            std::memset(reinterpret_cast<void*>(static_cast<std::uintptr_t>(b)), 0, 12);
            return 0;
        default: return 0;
        }
    };
    return result;
}

void prepare()
{
    std::memset(g.machine, 0, sizeof(g.machine));
    std::memset(g.state, 0, sizeof(g.state));
    std::memset(g.other, 0, sizeof(g.other));
    std::memset(g.blob, 0, sizeof(g.blob));
    std::memset(g.records, 0, sizeof(g.records));
    std::memset(g.vector, 0, sizeof(g.vector));
    g.depthTransition = 0;
    g.depthSleep = 0;
    g.frame = 100;
    g.conditionIndex = 0;
    g.conditions.clear();
    g.trace.clear();
    g.clearCurrent = 0;
    g.findHit = 0;
    g.findId = 0;
    g.stateRet = 0;
    g.enterRet = 0;
    g.machineRet = 0;
    g.goalValue = 0;
    g.xferCount = 0;
    write32(g.state, 0x18, addressOf(g.machine));
    write32(g.machine, 0x68, ra3::common::kInvalidStateId);
}

std::uint32_t read8Word(void* object)
{
    return reinterpret_cast<std::uint8_t*>(object)[0x78];
}

std::string finish(std::uint32_t eax, const std::vector<std::uint32_t>& words)
{
    std::ostringstream line;
    line << std::hex;
    line.width(8);
    line.fill('0');
    line << eax;
    for (const std::uint32_t word : words) {
        line << ' ';
        line.width(8);
        line.fill('0');
        line << word;
    }
    if (!g.trace.empty()) {
        line << ' ' << g.trace;
    }
    return line.str();
}

std::uint32_t currentId()
{
    const std::uint32_t current = read32(g.machine, 4);
    return current == 0 ? 0u : read32(reinterpret_cast<void*>(static_cast<std::uintptr_t>(current)), 4);
}

std::string run(const std::vector<std::string>& arg)
{
    using namespace ra3::common;
    prepare();
    auto linked = hooks();
    const std::string& command = arg[0];
    if (command == "scalar" || command == "mutant") {
        const std::uint32_t locked = hexOf(arg[1]);
        const std::uint32_t incoming = hexOf(arg[2]);
        const std::uint32_t bits = hexOf(arg[3]);
        write32(g.machine, 0x78, locked);
        write32(g.machine, 0x54, 0x11111111u);
        const std::uint32_t eax = command == "mutant"
                                      ? storeGoalScalarMutant(g.machine, incoming, bits)
                                      : storeGoalScalar(g.machine, incoming, bits);
        return finish(eax, {read32(g.machine, 0x54)});
    }
    if (command == "dword") {
        const std::uint32_t eax = storeGoalDword(g.machine, hexOf(arg[1]));
        return finish(eax, {read32(g.machine, 0x70)});
    }
    if (command == "lock") {
        const std::uint32_t eax = lockMachine(g.machine, hexOf(arg[1]));
        return finish(eax, {read8Word(g.machine)});
    }
    if (command == "unlock") {
        write32(g.machine, 0x78, 1);
        const std::uint32_t eax = unlockMachine(g.machine, hexOf(arg[1]));
        return finish(eax, {static_cast<std::uint32_t>(reinterpret_cast<std::uint8_t*>(g.machine)[0x78])});
    }
    if (command == "goalobj") {
        const std::uint32_t locked = hexOf(arg[1]);
        const std::uint32_t incoming = hexOf(arg[2]);
        reinterpret_cast<std::uint8_t*>(g.machine)[0x78] = static_cast<std::uint8_t>(locked);
        write32(g.machine, 0x28, 0x22222222u);
        std::uint32_t object = 0;
        if (hexOf(arg[3]) != 0) {
            write32(g.blob, 0xDC, hexOf(arg[4]));
            object = addressOf(g.blob);
        }
        const std::uint32_t eax = setGoalObject(g.machine, incoming, object);
        return finish(eax, {read32(g.machine, 0x28)});
    }
    if (command == "goalpos" || command == "goalreset") {
        const std::uint32_t locked = hexOf(arg[1]);
        const std::uint32_t incoming = hexOf(arg[2]);
        reinterpret_cast<std::uint8_t*>(g.machine)[0x78] = static_cast<std::uint8_t>(locked);
        write32(g.machine, 0x54, 0x11111111u);
        write32(g.machine, 0x44, 0xAB);
        std::uint32_t position = 0;
        if (hexOf(arg[3]) != 0) {
            write32(g.position, 0, hexOf(arg[4]));
            write32(g.position, 4, hexOf(arg[5]));
            write32(g.position, 8, hexOf(arg[6]));
            position = addressOf(g.position);
        }
        const std::uint32_t eax = command == "goalpos"
                                      ? setGoalPosition(g.machine, incoming, position, hexOf(arg[7]))
                                      : resetGoalPosition(g.machine, incoming, position);
        return finish(eax, {read32(g.machine, 0x2C), read32(g.machine, 0x30), read32(g.machine, 0x34),
                            read32(g.machine, 0x44), read32(g.machine, 0x48), read32(g.machine, 0x4C),
                            read32(g.machine, 0x50), read32(g.machine, 0x54)});
    }
    if (command == "copy") {
        reinterpret_cast<std::uint8_t*>(g.machine)[0x78] = static_cast<std::uint8_t>(hexOf(arg[1]));
        write32(g.machine, 0x2C, 0x01010101u);
        write32(g.machine, 0x30, 0x02020202u);
        write32(g.machine, 0x34, 0x03030303u);
        write32(g.machine, 0x44, 0x44);
        write32(g.machine, 0x48, 0x05050505u);
        write32(g.machine, 0x4C, 0x06060606u);
        write32(g.machine, 0x50, 0x07070707u);
        write32(g.machine, 0x54, 0x11111111u);
        reinterpret_cast<std::uint8_t*>(g.machine)[0x7C] = 0x7C;
        write32(g.other, 0x2C, hexOf(arg[2]));
        write32(g.other, 0x30, hexOf(arg[3]));
        write32(g.other, 0x34, hexOf(arg[4]));
        reinterpret_cast<std::uint8_t*>(g.other)[0x44] = static_cast<std::uint8_t>(hexOf(arg[5]));
        write32(g.other, 0x48, hexOf(arg[6]));
        write32(g.other, 0x4C, hexOf(arg[7]));
        write32(g.other, 0x50, hexOf(arg[8]));
        write32(g.other, 0x54, hexOf(arg[9]));
        reinterpret_cast<std::uint8_t*>(g.other)[0x7C] = static_cast<std::uint8_t>(hexOf(arg[10]));
        const std::uint32_t eax = copyGoalFields(g.machine, addressOf(g.other));
        return finish(eax, {read32(g.machine, 0x2C), read32(g.machine, 0x30), read32(g.machine, 0x34),
                            static_cast<std::uint32_t>(reinterpret_cast<std::uint8_t*>(g.machine)[0x44]),
                            read32(g.machine, 0x48), read32(g.machine, 0x4C), read32(g.machine, 0x50),
                            read32(g.machine, 0x54),
                            static_cast<std::uint32_t>(reinterpret_cast<std::uint8_t*>(g.machine)[0x7C])});
    }
    if (command == "path") {
        const auto index = static_cast<std::int32_t>(hexOf(arg[1]));
        const std::uint32_t count = hexOf(arg[2]);
        const std::uint32_t begin = addressOf(g.records);
        write32(g.machine, 0x58, begin);
        write32(g.machine, 0x5C, begin + count * 12u);
        const std::uint32_t eax = pathPointAt(g.machine, index);
        const std::uint32_t reported = eax == 0 ? 0xFFFFFFFFu : (eax - begin) / 12u;
        return finish(reported, {});
    }
    if (command == "getid") {
        if (hexOf(arg[1]) != 0) {
            write32(g.state, 4, hexOf(arg[2]));
            write32(g.machine, 4, addressOf(g.state));
        }
        return finish(currentStateId(g.machine), {});
    }
    if (command == "idle") {
        g.stateRet = hexOf(arg[3]);
        if (hexOf(arg[1]) != 0) {
            write32(g.machine, 4, addressOf(g.state));
        }
        return finish(isInIdleState(g.machine, hexOf(arg[2]), linked), {});
    }
    if (command == "fwd") {
        g.stateRet = hexOf(arg[3]);
        if (hexOf(arg[1]) != 0) {
            write32(g.machine, 4, addressOf(g.state));
        }
        return finish(forwardStateSlot48(g.machine, hexOf(arg[2]), linked), {});
    }
    if (command == "whileu") {
        g.machineRet = hexOf(arg[1]);
        reinterpret_cast<std::uint8_t*>(g.machine)[0x78] = 0xAB;
        const std::uint32_t eax = callWhileUnlocked(g.machine, linked);
        return finish(eax, {static_cast<std::uint32_t>(reinterpret_cast<std::uint8_t*>(g.machine)[0x78])});
    }
    if (command == "notify") {
        g.stateRet = hexOf(arg[2]);
        g.machineRet = hexOf(arg[3]);
        if (hexOf(arg[1]) != 0) {
            write32(g.machine, 4, addressOf(g.state));
        }
        return finish(notifyStateThenMachine(g.machine, linked), {});
    }
    if (command == "contains") {
        g.findHit = hexOf(arg[2]);
        return finish(stateMapContains(g.machine, hexOf(arg[1]), 7, linked), {});
    }
    if (command == "getst") {
        g.findHit = hexOf(arg[1]);
        g.findId = hexOf(arg[2]);
        const std::uint32_t eax = internalGetState(g.machine, 7, linked);
        const std::uint32_t id = eax == 0 ? 0u : read32(reinterpret_cast<void*>(static_cast<std::uintptr_t>(eax)), 4);
        return finish(id, {});
    }
    if (command == "scan") {
        const std::uint32_t incoming = hexOf(arg[1]);
        const std::uint32_t count = hexOf(arg[2]);
        write32(g.state, 0x10, addressOf(g.records));
        write32(g.state, 0x14, count);
        write32(g.state, 0x18, addressOf(g.machine));
        for (std::uint32_t i = 0; i < count; i += 1u) {
            write32(g.records, i * 16u, 0x20);
            write32(g.records, i * 16u + 4, hexOf(arg[3 + i * 2]));
            g.conditions.push_back(hexOf(arg[4 + i * 2]));
        }
        g.findHit = hexOf(arg[3 + count * 2]);
        g.findId = hexOf(arg[4 + count * 2]);
        std::uint32_t outStatus = 0x11111111u;
        const std::uint32_t eax = scanConditions(g.state, &outStatus, incoming, linked);
        const std::uint32_t shown = eax == addressOf(&outStatus) ? 1u : eax;
        return finish(shown, {outStatus, currentId(), read32(g.machine, 0x74)});
    }
    if (command == "trans") {
        g.depthTransition = hexOf(arg[1]);
        write32(g.state, 8, hexOf(arg[2]));
        write32(g.state, 0x0C, hexOf(arg[3]));
        write32(g.state, 0x10, addressOf(g.records));
        write32(g.state, 0x14, hexOf(arg[4]));
        write32(g.state, 0x18, addressOf(g.machine));
        const std::uint32_t count = hexOf(arg[4]);
        for (std::uint32_t i = 0; i < count; i += 1u) {
            write32(g.records, i * 16u + 4, hexOf(arg[6 + i * 2]));
            g.conditions.push_back(hexOf(arg[7 + i * 2]));
        }
        g.findHit = 0;
        if (arg.size() > 6 + count * 2) {
            g.findHit = hexOf(arg[6 + count * 2]);
            g.findId = hexOf(arg[7 + count * 2]);
        }
        const std::uint32_t eax = checkTransitions(g.state, hexOf(arg[5]), linked);
        return finish(eax, {g.depthTransition, currentId()});
    }
    if (command == "sleep") {
        g.depthSleep = hexOf(arg[1]);
        write32(g.state, 0x14, hexOf(arg[2]));
        write32(g.state, 0x10, addressOf(g.records));
        write32(g.state, 0x18, addressOf(g.machine));
        if (hexOf(arg[2]) != 0) {
            write32(g.records, 4, hexOf(arg[4]));
            g.conditions.push_back(hexOf(arg[5]));
        }
        const std::uint32_t eax = checkSleepTransitions(g.state, hexOf(arg[3]), linked);
        return finish(eax, {g.depthSleep});
    }
    if (command == "apply") {
        write32(g.state, 0x18, addressOf(g.machine));
        g.machineRet = hexOf(arg[2]);
        g.findHit = hexOf(arg[3]);
        g.findId = hexOf(arg[4]);
        g.enterRet = hexOf(arg[5]);
        const std::uint32_t eax = applyTransitionId(g.state, hexOf(arg[1]), linked);
        return finish(eax, {currentId(), read32(g.machine, 0x74)});
    }
    if (command == "set") {
        reinterpret_cast<std::uint8_t*>(g.machine)[0x78] = static_cast<std::uint8_t>(hexOf(arg[1]));
        write32(g.machine, 0x68, hexOf(arg[2]));
        write32(g.machine, 0x74, 0x33333333u);
        if (hexOf(arg[3]) != 0) {
            write32(g.other, 4, 9);
            write32(g.machine, 4, addressOf(g.other));
        }
        g.findHit = hexOf(arg[4]);
        g.findId = hexOf(arg[5]);
        g.enterRet = hexOf(arg[6]);
        g.clearCurrent = hexOf(arg[7]);
        const std::uint32_t eax = setState(g.machine, hexOf(arg[8]), linked);
        return finish(eax, {currentId(), read32(g.machine, 0x74),
                            static_cast<std::uint32_t>(reinterpret_cast<std::uint8_t*>(g.machine)[0x78])});
    }
    if (command == "init") {
        reinterpret_cast<std::uint8_t*>(g.machine)[0x7A] = static_cast<std::uint8_t>(hexOf(arg[1]));
        write32(g.machine, 0x68, hexOf(arg[2]));
        g.findHit = hexOf(arg[3]);
        g.findId = hexOf(arg[4]);
        g.stateRet = 0;
        const std::uint32_t eax = initDefaultState(g.machine, linked);
        return finish(eax, {static_cast<std::uint32_t>(reinterpret_cast<std::uint8_t*>(g.machine)[0x7A]), currentId()});
    }
    if (command == "update") {
        reinterpret_cast<std::uint8_t*>(g.machine)[0x79] = static_cast<std::uint8_t>(hexOf(arg[1]));
        g.frame = hexOf(arg[2]);
        linked.frame = g.frame;
        write32(g.machine, 0x74, hexOf(arg[3]));
        if (hexOf(arg[4]) != 0) {
            write32(g.state, 4, hexOf(arg[5]));
            write32(g.state, 8, hexOf(arg[6]));
            write32(g.state, 0x0C, hexOf(arg[7]));
            write32(g.state, 0x14, hexOf(arg[8]));
            write32(g.state, 0x10, addressOf(g.records));
            write32(g.state, 0x18, addressOf(g.machine));
            write32(g.machine, 4, addressOf(g.state));
            if (hexOf(arg[8]) != 0) {
                write32(g.records, 4, hexOf(arg[10]));
                g.conditions.push_back(hexOf(arg[11]));
            }
        }
        g.stateRet = hexOf(arg[9]);
        g.findHit = arg.size() > 12 ? hexOf(arg[12]) : 0;
        g.findId = arg.size() > 13 ? hexOf(arg[13]) : 0;
        const std::uint32_t eax = updateStateMachine(g.machine, linked);
        return finish(eax, {static_cast<std::uint32_t>(reinterpret_cast<std::uint8_t*>(g.machine)[0x79]),
                            read32(g.machine, 0x74), currentId()});
    }
    if (command == "mode") {
        write32(g.machine, 0x6C, hexOf(arg[1]));
        write32(g.machine, 0x24, hexOf(arg[2]) == 0 ? 0u : addressOf(g.other));
        write32(g.machine, 0x28, hexOf(arg[3]));
        g.ownerQuery = hexOf(arg[4]);
        g.goalValue = hexOf(arg[5]) == 0 ? 0u : addressOf(g.blob);
        g.assertAl = hexOf(arg[6]);
        g.debugGate = hexOf(arg[7]);
        const std::uint32_t eax = goalQuery(g.machine, linked);
        const std::uint32_t shown = eax == addressOf(g.blob) ? 1u : eax;
        return finish(shown, {});
    }
    if (command == "live") {
        const std::uint32_t incoming = hexOf(arg[1]);
        write32(g.machine, 0x28, hexOf(arg[2]));
        write32(g.machine, 0x6C, 1);
        g.goalValue = hexOf(arg[3]) == 0 ? 0u : addressOf(g.blob);
        g.blob[0x471] = static_cast<std::uint8_t>(hexOf(arg[4]));
        const std::uint32_t eax = goalObjectLive(g.machine, incoming, linked);
        const std::uint32_t shown = (g.goalValue != 0 && eax != 1u) ? (eax & 0xFFu) : eax;
        return finish(shown, {});
    }
    if (command == "fail") {
        write32(g.state, 0x18, addressOf(g.machine));
        if (hexOf(arg[1]) != 0) {
            write32(g.machine, 0x24, addressOf(g.other));
        }
        g.ownerProbe = hexOf(arg[3]);
        g.ownerTail = hexOf(arg[4]);
        return finish(failureStateExit(g.state, hexOf(arg[2]), linked), {});
    }
    if (command == "publish") {
        write32(g.machine, 0x24, addressOf(g.other));
        write32(g.other, 0x374, hexOf(arg[1]) == 0 ? 0u : addressOf(g.blob));
        write32(g.machine, 0x6C, hexOf(arg[2]));
        write32(g.machine, 0x28, hexOf(arg[3]));
        write32(g.machine, 0x2C, hexOf(arg[4]));
        write32(g.machine, 0x30, hexOf(arg[5]));
        write32(g.machine, 0x34, hexOf(arg[6]));
        g.goalValue = hexOf(arg[7]) == 0 ? 0u : addressOf(g.records);
        if (g.goalValue != 0) {
            write32(g.records, 0x38, hexOf(arg[8]));
            write32(g.records, 0x3C, hexOf(arg[9]));
            write32(g.records, 0x40, hexOf(arg[10]));
            write32(g.records, 0xDC, hexOf(arg[11]));
        }
        g.moving = hexOf(arg[12]);
        g.unitGoal = hexOf(arg[13]);
        g.unitKind = hexOf(arg[14]);
        g.moveProbe = hexOf(arg[15]);
        g.canPublish = hexOf(arg[16]);
        g.already = hexOf(arg[17]);
        alignas(256) static std::uint32_t outWords[3];
        outWords[0] = 0xAAAAAAAAu;
        outWords[1] = 0xBBBBBBBBu;
        outWords[2] = 0xCCCCCCCCu;
        const std::uint32_t eax = publishGoalPosition(g.machine, addressOf(outWords), linked);
        const std::uint32_t same = eax == addressOf(outWords) ? 1u : eax;
        return finish(same, {outWords[0], outWords[1], outWords[2]});
    }
    if (command == "adjust") {
        g.pathRet = hexOf(arg[1]);
        const std::uint32_t eax = assignGoalPath(g.machine, hexOf(arg[2]), linked);
        const std::uint32_t expectedThis = addressOf(g.machine) + 0x58u;
        return finish(eax, {g.pathThis == expectedThis ? 1u : 0u, g.pathArg});
    }
    if (command == "xfer") {
        const std::uint32_t count = hexOf(arg[1]);
        const std::uint32_t save = hexOf(arg[2]);
        const std::uint32_t byte8 = hexOf(arg[3]);
        g.xferCount = hexOf(arg[4]);
        const std::uint32_t begin = addressOf(g.records);
        write32(g.vector, 0, begin);
        write32(g.vector, 4, begin + count * 12u);
        write32(g.vector, 8, begin + hexOf(arg[5]) * 12u);
        reinterpret_cast<std::uint8_t*>(g.blob)[4] = static_cast<std::uint8_t>(save);
        reinterpret_cast<std::uint8_t*>(g.blob)[8] = static_cast<std::uint8_t>(byte8);
        const std::uint32_t eax = xferGoalPath(g.machine, addressOf(g.blob), addressOf(g.vector), linked);
        const std::uint32_t end = read32(g.vector, 4);
        const std::uint32_t stored = end >= begin ? (end - begin) / 12u : 0xFFFFFFFFu;
        return finish(eax == addressOf(g.blob) ? 1u : 0u, {stored, read32(g.records, 0)});
    }
    return "error";
}

}  // namespace

int main()
{
    std::string line;
    while (std::getline(std::cin, line)) {
        if (line == "quit") {
            return 0;
        }
        std::istringstream split(line);
        std::vector<std::string> arg;
        std::string word;
        while (split >> word) {
            arg.push_back(word);
        }
        if (arg.empty()) {
            std::cout << "error\n";
            continue;
        }
        try {
            std::cout << run(arg) << '\n';
        } catch (const std::exception&) {
            std::cout << "error\n";
        }
        std::cout.flush();
    }
    return 0;
}
