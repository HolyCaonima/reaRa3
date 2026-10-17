#include "ra3/gamelogic/AIStateViews.h"

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

using ra3::gamelogic::DozerAIView;
using ra3::gamelogic::GameDispatch;
using ra3::gamelogic::LogicTiming;
using ra3::gamelogic::StateView;
using ra3::gamelogic::SupplyEvents;
using ra3::gamelogic::SupplyTruckAIView;
using ra3::gamelogic::UnitAIView;
using ra3::gamelogic::WeaponEvents;
using ra3::gamelogic::WeaponStateView;

static_assert(sizeof(void*) == 4, "aistatecheck models 32-bit game pointers");

template <typename T>
T load(const std::byte* data, std::size_t offset) noexcept
{
    T value{};
    std::memcpy(&value, data + offset, sizeof(value));
    return value;
}

template <typename T>
void store(std::byte* data, std::size_t offset, T value) noexcept
{
    std::memcpy(data + offset, &value, sizeof(value));
}

std::vector<std::byte> decode(const std::string& text)
{
    if ((text.size() & 1U) != 0) {
        throw std::runtime_error("odd hex length");
    }
    const auto nibble = [](char value) -> unsigned {
        if (value >= '0' && value <= '9') return unsigned(value - '0');
        if (value >= 'a' && value <= 'f') return unsigned(value - 'a' + 10);
        if (value >= 'A' && value <= 'F') return unsigned(value - 'A' + 10);
        throw std::runtime_error("invalid hex");
    };
    std::vector<std::byte> result(text.size() / 2);
    for (std::size_t index = 0; index < result.size(); ++index) {
        result[index] = std::byte((nibble(text[index * 2]) << 4U) |
                                  nibble(text[index * 2 + 1]));
    }
    return result;
}

std::string encode(const std::vector<std::byte>& data)
{
    std::ostringstream output;
    output << std::hex << std::setfill('0');
    for (const auto value : data) {
        output << std::setw(2) << std::to_integer<unsigned>(value);
    }
    return output.str();
}

std::uint32_t number(const std::string& value)
{
    return static_cast<std::uint32_t>(std::stoul(value, nullptr, 16));
}

float asFloat(std::uint32_t bits) noexcept
{
    float value{};
    std::memcpy(&value, &bits, sizeof(value));
    return value;
}

std::uint32_t floatBits(float value) noexcept
{
    std::uint32_t bits{};
    std::memcpy(&bits, &value, sizeof(bits));
    return bits;
}

std::vector<std::size_t> parseRelocations(const std::string& text)
{
    std::vector<std::size_t> result;
    if (text == "-") return result;
    std::istringstream input(text);
    for (std::string item; std::getline(input, item, ',');) {
        result.push_back(number(item));
    }
    return result;
}

struct EventLog {
    std::uint32_t value{};
};

struct DispatchLog {
    std::byte* base{};
    std::size_t size{};
    std::uint32_t interfaceOffset{};
    std::uint32_t workerResult{};
    std::uint32_t hash{};
};

std::uint32_t dispatched(
    void* context, void* target, std::uint32_t slot,
    const std::uint32_t* arguments, std::size_t argumentCount) noexcept
{
    auto& log = *static_cast<DispatchLog*>(context);
    const auto address = reinterpret_cast<std::uintptr_t>(target);
    const auto first = reinterpret_cast<std::uintptr_t>(log.base);
    const auto targetOffset = address >= first && address < first + log.size
        ? static_cast<std::uint32_t>(address - first)
        : static_cast<std::uint32_t>(address);
    const auto argument = argumentCount == 0 ? 0U : arguments[0];
    log.hash = (log.hash * 16777619U) ^ slot ^ (targetOffset << 8U) ^ argument;
    if (slot == 0x1bc || slot == 0x1c4) {
        return log.interfaceOffset == 0
            ? 0U
            : static_cast<std::uint32_t>(first + log.interfaceOffset);
    }
    if (slot == 0x288) {
        return log.workerResult;
    }
    if (slot == 0x18 && argumentCount == 1 && argument < 3) {
        const auto* targetBytes = static_cast<const std::byte*>(target);
        const auto value = load<std::uint32_t>(
            targetBytes, 0x04 + std::size_t(argument) * 8U);
        return 0xa5000000U | std::uint32_t(value != 0);
    }
    return 0;
}

std::uint32_t supplyChanged(
    void* context, void*, std::int32_t maximum, std::int32_t current) noexcept
{
    auto& log = *static_cast<EventLog*>(context);
    log.value = 0x51000000U ^ static_cast<std::uint32_t>(maximum) ^
                (static_cast<std::uint32_t>(current) * 0x10001U);
    return 0;
}

void readyNotified(void* context, std::uint32_t value) noexcept
{
    auto& log = *static_cast<EventLog*>(context);
    log.value = 0x57000000U ^ value;
}

std::uint32_t pointerResult(const void* value, const std::byte* base, std::size_t size) noexcept
{
    const auto address = reinterpret_cast<std::uintptr_t>(value);
    const auto first = reinterpret_cast<std::uintptr_t>(base);
    if (address >= first && address < first + size) {
        return static_cast<std::uint32_t>(address - first);
    }
    return static_cast<std::uint32_t>(address);
}

void run(const std::vector<std::string>& tokens)
{
    if (tokens.size() != 13) {
        throw std::runtime_error("wrong token count");
    }
    const auto& op = tokens[0];
    auto arena = decode(tokens[1]);
    const auto relocations = parseRelocations(tokens[2]);
    const auto thisOffset = number(tokens[3]);
    const std::uint32_t args[] = {
        number(tokens[4]), number(tokens[5]), number(tokens[6]), number(tokens[7])};
    LogicTiming timing{};
    timing.gameLogic = number(tokens[8]) == 0 ? nullptr : reinterpret_cast<void*>(1);
    timing.frame = number(tokens[9]);
    timing.timeScale = asFloat(number(tokens[10]));
    timing.framesPerSecond = asFloat(number(tokens[11]));
    timing.integerFramesPerSecond = number(tokens[12]);

    auto* base = arena.data();
    for (const auto relocation : relocations) {
        const auto target = load<std::uint32_t>(base, relocation);
        const auto value = target == 0 ? std::uintptr_t{0}
                                       : reinterpret_cast<std::uintptr_t>(base + target);
        store(base, relocation, value);
    }

    auto* self = base + thisOffset;
    StateView state(self);
    DozerAIView dozer(self);
    SupplyTruckAIView supply(self);
    UnitAIView unit(self);
    WeaponStateView weaponState(self);
    EventLog event{};
    DispatchLog dispatchLog{base, arena.size(), args[0], args[1], 0};
    const GameDispatch dispatch{&dispatchLog, &dispatched};
    std::uint32_t result = 0;

    if (op == "dozeractionenter") {
        result = static_cast<std::uint32_t>(state.enterDozerAction(timing.frame, dispatch));
    } else if (op == "dozeractionexit") state.exitDozerAction(dispatch);
    else if (op == "dozernestedenter") {
        result = static_cast<std::uint32_t>(state.enterNestedDozerAction(dispatch));
    } else if (op == "dozernestedexit") state.exitNestedDozerAction(dispatch);
    else if (op == "supplybusyenter") {
        result = static_cast<std::uint32_t>(state.enterSupplyBusy(dispatch));
    } else if (op == "supplyidleenter") {
        result = static_cast<std::uint32_t>(state.enterSupplyIdle(dispatch));
    } else if (op == "supplydockenter") {
        result = static_cast<std::uint32_t>(state.enterSupplyDocking(dispatch));
    } else if (op == "stateenter") {
        result = static_cast<std::uint32_t>(state.enterDozerPrimaryIdle(timing.frame));
    } else if (op == "stateexit") state.exitDozerPrimaryIdle();
    else if (op == "dozerhastask") result = dozer.hasTask(args[0]);
    else if (op == "dozeranytask") result = dozer.hasAnyTask(dispatch);
    else if (op == "dozertarget") result = dozer.taskTarget(args[0]);
    else if (op == "dozerrecent") {
        result = static_cast<std::uint32_t>(dozer.mostRecentTask(dispatch));
    }
    else if (op == "dozerdock") {
        result = pointerResult(dozer.dockPoint(args[0], args[1]), base, arena.size());
    } else if (op == "dozerrepair") result = floatBits(dozer.repairHealthPerSecond());
    else if (op == "dozerbored") result = floatBits(dozer.boredTime());
    else if (op == "supplyferry") result = supply.isCurrentlyFerryingSupplies();
    else if (op == "supplylose") {
        result = supply.loseOneBox(
            SupplyEvents{&event, &supplyChanged});
    } else if (op == "supplywant") result = supply.forceWanting();
    else if (op == "supplybusy") result = supply.forceBusy();
    else if (op == "supplyposition") {
        supply.copyPosition24(base + args[0]);
        result = load<std::uint32_t>(base, args[0]);
    } else if (op == "supplyscan") result = floatBits(supply.warehouseScanDistance());
    else if (op == "supplyflag6c") result = supply.dataFlag6c();
    else if (op == "supplyflag6d") result = supply.dataFlag6d();
    else if (op == "supplyflag6e") result = supply.dataFlag6e();
    else if (op == "supplyfloat60") result = floatBits(supply.dataFloat60());
    else if (op == "supplycenter") result = static_cast<std::uint32_t>(supply.centerDelay(timing));
    else if (op == "supplywarehouse") result = static_cast<std::uint32_t>(supply.warehouseDelay(timing));
    else if (op == "unitset144") unit.setValue0144(args[0]);
    else if (op == "unitreset") unit.resetStateFlags();
    else if (op == "unitnestedflag") result = unit.nestedFlag1e();
    else if (op == "unitsetgoal") {
        unit.setGoalPosition(base + args[0]);
    } else if (op == "unitcleargoal") unit.clearGoal();
    else if (op == "unitweaponusable") result = unit.selectedWeaponUsable();
    else if (op == "unitclearids") unit.clearPendingObjectIds();
    else if (op == "unitmatchids") result = unit.objectMatchesAttackIds(base + args[0]);
    else if (op == "unitstate69") result = unit.currentStateIs69Or6a();
    else if (op == "unitvalue250") result = unit.value0250();
    else if (op == "unitstateid") result = unit.currentOrQueuedStateId();
    else if (op == "unitqueued") result = unit.hasQueuedState();
    else if (op == "unitcurrentid") result = unit.currentStateId();
    else if (op == "unitconditions") {
        result = pointerResult(unit.currentStateConditionBlock(), base, arena.size());
    } else if (op == "unittransitions") {
        result = pointerResult(unit.currentStateTransitionBlock(), base, arena.size());
    } else if (op == "unitdesired") unit.setDesiredSpeedBits(args[0]);
    else if (op == "unitscaled") result = static_cast<std::uint32_t>(unit.scaledModuleTime(timing));
    else if (op == "unitclearcommand") unit.clearCommandLatch(dispatch);
    else if (op == "unitdelay") result = static_cast<std::uint32_t>(unit.randomizedDelay(timing));
    else if (op == "weaponready") {
        result = static_cast<std::uint32_t>(weaponState.updateReady(
            WeaponEvents{&event, &readyNotified}));
    } else if (op == "weaponcooldown") {
        result = static_cast<std::uint32_t>(weaponState.updateCoolDown(timing));
    } else if (op == "weaponreload") {
        result = static_cast<std::uint32_t>(weaponState.updateReload(timing));
    } else if (op == "onesecond") {
        result = static_cast<std::uint32_t>(ra3::gamelogic::ai_port::oneSecondFrames(timing));
    } else {
        throw std::runtime_error("unknown operation");
    }

    const auto baseValue = reinterpret_cast<std::uintptr_t>(base);
    for (const auto relocation : relocations) {
        const auto value = load<std::uintptr_t>(base, relocation);
        if (value == 0) {
            store(base, relocation, std::uint32_t{0});
        } else if (value >= baseValue && value < baseValue + arena.size()) {
            store(base, relocation, static_cast<std::uint32_t>(value - baseValue));
        }
    }

    event.value ^= dispatchLog.hash;
    std::cout << std::hex << result << ' ' << event.value << ' '
              << static_cast<std::uint32_t>(baseValue) << ' ' << encode(arena) << '\n';
}

}  // namespace

int main()
{
    std::string line;
    while (std::getline(std::cin, line)) {
        try {
            std::istringstream input(line);
            std::vector<std::string> tokens;
            for (std::string token; input >> token;) tokens.push_back(token);
            if (!tokens.empty()) run(tokens);
        } catch (const std::exception&) {
            std::cout << "error 0 0 .\n";
        }
    }
}
