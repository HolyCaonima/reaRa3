#include <ra3/gamelogic/AITNGuardViews.h>

#include <algorithm>
#include <cstdint>
#include <cstring>
#include <iomanip>
#include <iostream>
#include <limits>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

std::uint32_t number(const std::string& text)
{
    return static_cast<std::uint32_t>(std::stoul(text, nullptr, 0));
}

float floatBits(const std::string& text)
{
    const std::uint32_t bits = number(text);
    float result = 0.0F;
    std::memcpy(&result, &bits, sizeof(result));
    return result;
}

class Random {
public:
    std::uint32_t next() noexcept
    {
        state_ ^= state_ << 13;
        state_ ^= state_ >> 17;
        state_ ^= state_ << 5;
        return state_;
    }

private:
    std::uint32_t state_ = 0x534B3739U;
};

bool referencePair(
    std::uint32_t first, std::uint32_t second,
    std::uint32_t expectedFirst, std::uint32_t expectedSecond,
    std::uint32_t actual)
{
    const bool firstMatches =
        first == expectedFirst || first == expectedSecond;
    const bool secondMatches =
        second == expectedFirst || second == expectedSecond;
    const std::uint32_t expected = firstMatches != secondMatches
        ? (firstMatches ? 1U : 2U)
        : (first == expectedFirst && second == expectedSecond ? 1U
           : (first == expectedSecond && second == expectedFirst ? 2U : 0U));
    return actual == expected;
}

bool runSelfCheck()
{
    using namespace ra3::gamelogic;
    const auto& entries = aitnGuardInventory();
    if (entries.size() != 50U) {
        return false;
    }
    for (std::size_t index = 0; index < entries.size(); ++index) {
        if (entries[index].size <= 8U || entries[index].name == nullptr) {
            return false;
        }
        for (std::size_t previous = 0; previous < index; ++previous) {
            if (entries[index].address == entries[previous].address) {
                return false;
            }
        }
    }
    const auto primary = std::count_if(
        entries.begin(), entries.end(), [](const InventoryEntry& entry) {
            return entry.address >= 0x00790000U && entry.address <= 0x0079FFFFU;
        });
    const auto directPorts = std::count_if(
        entries.begin(), entries.end(), [](const InventoryEntry& entry) {
            return entry.directPort;
        });
    const auto hooks = std::count_if(
        entries.begin(), entries.end(), [](const InventoryEntry& entry) {
            return entry.hookSafe;
        });
    if (primary != 39 || directPorts != 8 || hooks != 8) {
        return false;
    }

    Random random;
    for (unsigned index = 0; index < 10000U; ++index) {
        const std::uint32_t frame = random.next();
        const std::uint32_t deadline = random.next();
        if (attackDeadlineExpired(frame, deadline) != (frame >= deadline)) {
            return false;
        }
        const std::uint32_t pointer = random.next();
        const auto value = static_cast<std::uint8_t>(random.next());
        if (byteWithPointerUpperBits(pointer, value)
            != ((pointer & 0xFFFFFF00U) | value)) {
            return false;
        }
        const std::uint32_t first = random.next() % 5U;
        const std::uint32_t second = random.next() % 5U;
        const std::uint32_t expectedFirst = random.next() % 5U;
        const std::uint32_t expectedSecond = random.next() % 5U;
        if (!referencePair(
                first, second, expectedFirst, expectedSecond,
                classifyEndpointPair(
                    first, second, expectedFirst, expectedSecond))) {
            return false;
        }
    }

    std::vector<GuardCandidate> tunnels{
        {10U, {4.0F, 0.0F}, true, false, false},
        {20U, {1.0F, 1.0F}, false, false, false},
        {30U, {2.0F, 0.0F}, true, false, false}};
    const GuardCandidate* nearest = findNearestTunnel(tunnels, {0.0F, 0.0F});
    if (nearest == nullptr || nearest->id != 30U) {
        return false;
    }

    IdleStateView idle{};
    beginIdle(idle, 100U, 7U);
    IdleUpdateInput sleeping{};
    sleeping.frame = 103U;
    const IdleUpdateResult sleepResult = updateIdle(idle, sleeping);
    if (sleepResult.sleepFrames != 4U
        || sleepResult.state != StateResult::continueState) {
        return false;
    }
    IdleUpdateInput target{};
    target.frame = 107U;
    target.scanPeriod = 15U;
    target.foundInnerTarget = true;
    const IdleUpdateResult targetResult = updateIdle(idle, target);
    if (targetResult.state != StateResult::success
        || targetResult.sleepFrames != 0U) {
        return false;
    }

    // Sensitivity witness for reconstructed 0x0079BED0, not inventory data.
    const std::uint32_t expected = classifyEndpointPair(1U, 2U, 1U, 2U);
    const std::uint32_t deliberateMutant = expected == 1U ? 2U : expected;
    return expected == 1U && deliberateMutant != expected
        && classifyEndpointPair(1U, 2U, 1U, 2U) == expected;
}

void run(const std::vector<std::string>& words)
{
    using namespace ra3::gamelogic;
    if (words.empty() || words[0] == "selftest") {
        if (!runSelfCheck()) {
            throw std::runtime_error("self check failed");
        }
        std::cout << "AITNGuard checker: inventory=50, reconstructed=8, "
                     "0 semantic mismatches; semantic mutant detected\n";
        return;
    }
    if (words[0] == "inventory" || words[0] == "manifest") {
        for (const InventoryEntry& entry : aitnGuardInventory()) {
            std::cout << std::hex << std::setfill('0') << std::setw(8)
                      << entry.address << std::dec << '\t' << entry.size << '\t'
                      << (entry.directPort ? 1 : 0) << '\t'
                      << (entry.hookSafe ? 1 : 0) << '\t' << entry.name << '\n';
        }
        return;
    }
    if (words[0] == "deadline") {
        std::cout << (attackDeadlineExpired(
            number(words.at(1)), number(words.at(2))) ? 1 : 0) << '\n';
        return;
    }
    if (words[0] == "byte") {
        std::cout << byteWithPointerUpperBits(
            number(words.at(1)),
            static_cast<std::uint8_t>(number(words.at(2)))) << '\n';
        return;
    }
    if (words[0] == "payload") {
        std::cout << conditionalPayloadOffset(
            number(words.at(1)) != 0U,
            static_cast<std::int32_t>(number(words.at(2)))) << '\n';
        return;
    }
    if (words[0] == "tracked") {
        std::cout << trackedTargetId(
            number(words.at(1)) != 0U, number(words.at(2))) << '\n';
        return;
    }
    if (words[0] == "pair") {
        std::cout << classifyEndpointPair(
            number(words.at(1)), number(words.at(2)),
            number(words.at(3)), number(words.at(4))) << '\n';
        return;
    }
    if (words[0] == "pairmutant") {
        const std::uint32_t result = classifyEndpointPair(
            number(words.at(1)), number(words.at(2)),
            number(words.at(3)), number(words.at(4)));
        std::cout << (result == 1U ? 2U : result) << '\n';
        return;
    }
    if (words[0] == "cooldown") {
        std::cout << objectCooldownResult(
            number(words.at(1)), number(words.at(2)),
            static_cast<std::int32_t>(number(words.at(3))),
            static_cast<std::int32_t>(number(words.at(4))),
            floatBits(words.at(5)), number(words.at(6))) << '\n';
        return;
    }
    if (words[0] == "targetcooldown") {
        std::cout << targetCooldownResult(
            number(words.at(1)), number(words.at(2)),
            static_cast<std::int32_t>(number(words.at(3))),
            static_cast<std::int32_t>(number(words.at(4))),
            floatBits(words.at(5)),
            static_cast<std::int32_t>(number(words.at(6))),
            number(words.at(7))) << '\n';
        return;
    }
    if (words[0] == "near") {
        const Coord2D value{floatBits(words.at(1)), floatBits(words.at(2))};
        const Coord2D center{floatBits(words.at(3)), floatBits(words.at(4))};
        std::cout << (isNear2D(
            value, center, floatBits(words.at(5))) ? 1 : 0) << '\n';
        return;
    }
    if (words[0] == "candidatenear") {
        std::cout << candidateNearResult(
            number(words.at(1)), number(words.at(2)) != 0U,
            number(words.at(3)) != 0U) << '\n';
        return;
    }
    throw std::runtime_error("unknown command");
}

}  // namespace

int main(int argc, char** argv)
{
    try {
        if (argc == 1) {
            run({"selftest"});
            return 0;
        }
        if (argc == 2 && std::string(argv[1]) == "--stream") {
            for (std::string line; std::getline(std::cin, line);) {
                if (line == "quit") {
                    return 0;
                }
                try {
                    std::istringstream input(line);
                    std::vector<std::string> words;
                    for (std::string word; input >> word;) {
                        words.push_back(std::move(word));
                    }
                    run(words);
                } catch (...) {
                    std::cout << "error\n";
                }
                std::cout.flush();
            }
            return 0;
        }
        std::vector<std::string> words;
        words.reserve(static_cast<std::size_t>(argc - 1));
        for (int index = 1; index < argc; ++index) {
            words.emplace_back(argv[index]);
        }
        run(words);
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "ra3_skirmishcheck: " << error.what() << '\n';
        return 1;
    }
}
