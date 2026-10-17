#include <ra3/gamelogic/ObjectCore6A.h>

#include <cstdint>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

namespace {

using ra3::gamelogic::kModule6ABytes;
using ra3::gamelogic::ModuleResult6A;

void fillModule(std::uint8_t module[kModule6ABytes], unsigned seed) noexcept
{
    for (std::size_t i = 0; i < kModule6ABytes; ++i) {
        module[i] = static_cast<std::uint8_t>(seed + i);
    }
}

void emit(std::uint32_t eax, const char* trace, const std::uint8_t* mem, std::size_t memBytes)
{
    std::ostringstream line;
    line << std::hex << std::setfill('0') << std::setw(8) << eax << ' '
         << ((trace == nullptr || trace[0] == '\0') ? "-" : trace) << ' ';
    if (mem == nullptr) {
        line << '-';
    } else {
        for (std::size_t i = 0; i < memBytes; ++i) {
            line << std::setw(2) << static_cast<unsigned>(mem[i]);
        }
    }
    std::cout << line.str() << '\n';
}

void emitResult(const ModuleResult6A& result)
{
    emit(result.eax, result.trace, nullptr, 0);
}

bool selfTest() noexcept
{
    const std::uint32_t ids[] = {3u, 9u, 9u};
    if (ra3::gamelogic::encloseContainsId(ids, 0, 9u, 0xA5A5A500u) != 0xA5A5A500u) {
        return false;
    }
    if (ra3::gamelogic::encloseContainsId(ids, 3, 9u, 0xA5A5A500u) != 0xA5A5A501u) {
        return false;
    }
    if (ra3::gamelogic::slaughterContainedFlag(0) != 0u ||
        ra3::gamelogic::slaughterContainedFlag(0x80000000u) != 1u) {
        return false;
    }
    const std::uint32_t rets[] = {2u, 5u};
    const ModuleResult6A notified = ra3::gamelogic::weaponSetCallSlot1C(0x11111111u, rets, 2);
    if (notified.eax != 5u || std::string(notified.trace) != "s1c:0;s1c:1;") {
        return false;
    }
    return true;
}

} // namespace

int main(int argc, char** argv)
{
    for (int i = 1; i < argc; ++i) {
        if (std::string(argv[i]) == "--self-test") {
            return selfTest() ? 0 : 1;
        }
    }
    if (!selfTest()) {
        std::cerr << "obj6acheck selfTest failed\n";
        return 1;
    }

    std::string line;
    while (std::getline(std::cin, line)) {
        if (line.empty()) {
            continue;
        }
        std::istringstream input(line);
        std::string command;
        input >> command;
        if (command == "quit") {
            break;
        }
        if (command == "enclose") {
            std::uint32_t listAddress = 0;
            std::uint32_t count = 0;
            std::uint32_t needle = 0;
            input >> listAddress >> count >> needle;
            std::vector<std::uint32_t> ids(count);
            for (std::uint32_t i = 0; i < count; ++i) {
                input >> ids[i];
            }
            const std::uint32_t* data = ids.empty() ? nullptr : ids.data();
            emit(ra3::gamelogic::encloseContainsId(data, count, needle, listAddress), "-", nullptr, 0);
        } else if (command == "notify") {
            std::uint32_t incoming = 0;
            std::uint32_t count = 0;
            input >> incoming >> count;
            std::vector<std::uint32_t> rets(count);
            for (std::uint32_t i = 0; i < count; ++i) {
                input >> rets[i];
            }
            emitResult(ra3::gamelogic::weaponSetCallSlot1C(
                incoming, rets.empty() ? nullptr : rets.data(), count));
        } else if (command == "slaughter") {
            std::uint32_t helper = 0;
            input >> helper;
            emit(ra3::gamelogic::slaughterContainedFlag(helper), "h6b0bd0;", nullptr, 0);
        } else if (command == "vet") {
            std::uint32_t helper1 = 0;
            std::uint32_t flag = 0;
            std::uint32_t status = 0;
            std::uint32_t byte74 = 0;
            std::uint32_t byte75 = 0;
            int same = 0;
            std::uint32_t helper2 = 0;
            std::uint32_t team = 0;
            input >> helper1 >> flag >> status >> byte74 >> byte75 >> same >> helper2 >> team;
            emitResult(ra3::gamelogic::veterancyCrateCanPass(
                helper1, static_cast<std::uint8_t>(flag), status,
                static_cast<std::uint8_t>(byte74), static_cast<std::uint8_t>(byte75),
                same != 0, team, helper2));
        } else if (command == "x_clear") {
            std::uint32_t version = 0;
            std::uint32_t helper = 0;
            input >> version >> helper;
            emitResult(ra3::gamelogic::clearanceSlowDeathXfer(version, helper));
        } else if (command == "x_tech") {
            std::uint32_t version = 0;
            std::uint32_t helper = 0;
            input >> version >> helper;
            emitResult(ra3::gamelogic::techUpgradeXfer(version, helper));
        } else if (command == "x_dmove") {
            std::uint32_t version = 0;
            std::uint32_t block = 0;
            input >> version >> block;
            emitResult(ra3::gamelogic::dozerMoveToXfer(version, block));
        } else if (command == "x_dsm") {
            std::uint32_t version = 0;
            std::uint32_t block = 0;
            input >> version >> block;
            emitResult(ra3::gamelogic::dozerStateMachineXfer(version, block));
        } else if (command == "x_dpick") {
            std::uint32_t version = 0;
            std::uint32_t block = 0;
            std::uint32_t tail = 0;
            input >> version >> block >> tail;
            emitResult(ra3::gamelogic::dozerPickXfer(version, block, tail));
        } else if (command == "x_ddo") {
            std::uint32_t version = 0;
            std::uint32_t block = 0;
            std::uint32_t tail = 0;
            input >> version >> block >> tail;
            emitResult(ra3::gamelogic::dozerDoXfer(version, block, tail));
        } else if (command == "x_dact") {
            unsigned seed = 0;
            std::uint32_t version = 0;
            std::uint32_t block = 0;
            std::uint32_t value = 0;
            input >> seed >> version >> block >> value;
            std::uint8_t module[kModule6ABytes];
            fillModule(module, seed);
            emitResult(ra3::gamelogic::dozerActionXfer(module, version, block, value));
        } else if (command == "x_sub") {
            std::uint32_t version = 0;
            std::uint32_t block = 0;
            std::uint32_t local = 0;
            std::uint32_t helper = 0;
            input >> version >> block >> local >> helper;
            emitResult(ra3::gamelogic::subObjectsUpgradeXfer(version, block, local, helper));
        } else if (command == "x_exp") {
            std::uint32_t version = 0;
            std::uint32_t first = 0;
            std::uint32_t second = 0;
            std::uint32_t tail = 0;
            input >> version >> first >> second >> tail;
            emitResult(ra3::gamelogic::experienceTrackerObjectXfer(version, first, second, tail));
        } else if (command == "x_audio") {
            std::uint32_t version = 0;
            std::uint32_t global = 0;
            std::uint32_t base = 0;
            std::uint32_t sub = 0;
            input >> version >> global >> base >> sub;
            emitResult(ra3::gamelogic::audioLoopUpgradeXfer(version, global, base, sub));
        } else if (command == "x_enter") {
            std::uint32_t version0 = 0;
            std::uint32_t version1 = 0;
            std::uint32_t block = 0;
            std::uint32_t helper = 0;
            input >> version0 >> version1 >> block >> helper;
            emitResult(ra3::gamelogic::createAndEnterXfer(version0, version1, block, helper));
        } else if (command == "x_recv") {
            std::uint32_t plus4 = 0;
            std::uint32_t version = 0;
            std::uint32_t base = 0;
            std::uint32_t helper = 0;
            std::uint32_t slotA = 0;
            std::uint32_t slotB = 0;
            std::uint32_t slot70 = 0;
            input >> plus4 >> version >> base >> helper >> slotA >> slotB >> slot70;
            emitResult(ra3::gamelogic::techUpgradeReceiverXfer(
                plus4, version, base, helper, slotA, slotB, slot70));
        } else {
            std::cout << "error unknown\n";
        }
    }
    return 0;
}
