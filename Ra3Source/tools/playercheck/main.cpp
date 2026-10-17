#include "ra3/common/Player.h"

#include <cstdint>
#include <cstring>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

std::vector<std::byte> decode(const std::string& text)
{
    if ((text.size() & 1U) != 0) throw std::runtime_error("odd hex length");
    std::vector<std::byte> result(text.size() / 2);
    const auto nibble = [](char value) -> unsigned {
        if (value >= '0' && value <= '9') return unsigned(value - '0');
        if (value >= 'a' && value <= 'f') return unsigned(value - 'a' + 10);
        if (value >= 'A' && value <= 'F') return unsigned(value - 'A' + 10);
        throw std::runtime_error("invalid hex");
    };
    for (std::size_t index = 0; index < result.size(); ++index) {
        result[index] = std::byte((nibble(text[index * 2]) << 4)
                                | nibble(text[index * 2 + 1]));
    }
    return result;
}

std::string encode(const std::vector<std::byte>& data)
{
    std::ostringstream out;
    out << std::hex << std::setfill('0');
    for (const auto value : data) out << std::setw(2) << std::to_integer<unsigned>(value);
    return out.str();
}

std::uint32_t number(const std::string& value) { return std::stoul(value, nullptr, 16); }

void run(const std::vector<std::string>& tokens)
{
    using ra3::common::Player;
    using ra3::common::ScoreKeeper;
    namespace port = ra3::common::player_port;

    auto data = decode(tokens.at(1));
    const auto arg = tokens.size() > 2 ? number(tokens[2]) : 0U;
    const auto arg2 = tokens.size() > 3 ? number(tokens[3]) : 0U;
    std::uint32_t result = 0;
    Player player(data.data());
    ScoreKeeper score(data.data());
    const auto& op = tokens[0];

    if (op == "adjust125c") player.adjustCounter125c(static_cast<std::int32_t>(arg));
    else if (op == "set1258") player.setValue1258(arg);
    else if (op == "setlistscore") player.setListInScoreScreen(arg != 0);
    else if (op == "setf8") player.setValueF8(arg);
    else if (op == "value1f8") std::memcpy(&result, &data[0x1f8], 4);
    else if (op == "slot19e0") result = player.slotForIndex(arg) == nullptr ? 0U : 0x248U + arg * 0x30U;
    else if (op == "flag10d") result = player.indexedFlag10d(arg);
    else if (op == "pointer128") {
        result = static_cast<std::uint32_t>(
            reinterpret_cast<std::uintptr_t>(player.indexedPointer128(static_cast<std::int32_t>(arg))));
    }
    else if (op == "value1c8") result = player.value1c8();
    else if (op == "observer") result = player.isPlayerObserver();
    else if (op == "dead") result = player.isPlayerDead();
    else if (op == "active") result = player.isPlayerActive();
    else if (op == "score04") score.setValue04IfUnlocked(arg);
    else if (op == "score0c") score.setValue0cIfUnlocked(arg);
    else if (op == "addd0") score.addD0(static_cast<std::int32_t>(arg), arg2 != 0);
    else if (op == "addf11c") {
        float value{};
        std::memcpy(&value, &arg, 4);
        score.addFloat11c(value, arg2 != 0);
    } else if (op == "inc100") score.increment100(arg != 0);
    else if (op == "ince8") score.incrementE8(arg != 0);
    else if (op == "incec") score.incrementEc(arg != 0);
    else if (op == "incf0") score.incrementF0(arg != 0);
    else if (op == "incd8") score.incrementD8(arg != 0);
    else if (op == "adddc") score.addDc(static_cast<std::int32_t>(arg), arg2 != 0);
    else if (op == "stoptimer") score.stopTimer(arg, arg2 != 0);
    else if (op == "elapsed") result = score.elapsedSeconds(arg, arg2);
    else if (op == "timed") result = score.timedSeconds(arg, arg2);
    else if (op == "sum68") result = static_cast<std::uint32_t>(score.sumGroup68());
    else if (op == "sum10") result = static_cast<std::uint32_t>(score.sumGroup10());
    else if (op == "flag134") score.setFlag134(arg != 0);
    else if (op == "flag135") score.setFlag135(arg != 0);
    else if (op == "mark134") score.markFlag134();
    else if (op == "addmoney") score.addMoney(static_cast<std::int32_t>(arg), arg2 != 0);
    else if (op == "divide") {
        float left{}, right{}, value{};
        std::memcpy(&left, &arg, 4);
        std::memcpy(&right, &arg2, 4);
        value = port::divideUnlessOne(left, right);
        std::memcpy(&result, &value, 4);
    } else if (op == "capacity") result = port::hasCapacity(data.data(), arg);
    else if (op == "delay") port::setBoundedDelay(data.data(), static_cast<std::int32_t>(arg));
    else if (op == "outside2") result = port::indexIsOutsideBinaryRange(static_cast<std::int32_t>(arg));
    else if (op == "flagd7") port::setFlagD7(data.data(), arg != 0);
    else if (op == "add290") port::addValue290(data.data(), static_cast<std::int32_t>(arg));
    else if (op == "sub290") port::subtractValue290(data.data(), static_cast<std::int32_t>(arg));
    else throw std::runtime_error("unknown operation");

    std::cout << std::hex << result << ' ' << encode(data) << '\n';
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
        } catch (...) {
            std::cout << "error .\n";
        }
    }
}
