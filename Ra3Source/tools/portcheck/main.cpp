#include <ra3/common/ByteString.h>
#include <ra3/sim/Random.h>

#include <algorithm>
#include <cstdint>
#include <cstring>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

std::vector<std::uint8_t> fromHex(const std::string& text)
{
    std::vector<std::uint8_t> bytes;
    if (text == "-") {
        return bytes;
    }
    bytes.reserve(text.size() / 2 + 1);
    for (std::size_t i = 0; i + 1 < text.size(); i += 2) {
        bytes.push_back(static_cast<std::uint8_t>(std::stoul(text.substr(i, 2), nullptr, 16)));
    }
    return bytes;
}

void printHex(const std::uint8_t* bytes, std::size_t size)
{
    const auto flags = std::cout.flags();
    const char fill = std::cout.fill();
    std::cout << std::hex << std::setfill('0');
    for (std::size_t i = 0; i < size; ++i) {
        std::cout << std::setw(2) << static_cast<unsigned>(bytes[i]);
    }
    std::cout.flags(flags);
    std::cout.fill(fill);
    std::cout << '\n';
}

const char* textPointer(const std::vector<std::uint8_t>& bytes, const std::string& encoded)
{
    return encoded == "-" ? nullptr : reinterpret_cast<const char*>(bytes.data());
}

std::uint32_t number(const std::string& text)
{
    return static_cast<std::uint32_t>(std::stoul(text, nullptr, 0));
}

ra3::sim::RandomState randomState(const std::vector<std::uint8_t>& bytes)
{
    ra3::sim::RandomState state{};
    if (bytes.size() != sizeof(state)) {
        throw std::invalid_argument("random state must contain six dwords");
    }
    std::memcpy(&state, bytes.data(), sizeof(state));
    return state;
}

void printRandom(std::uint32_t result, const ra3::sim::RandomState& state)
{
    std::cout << result << ' ';
    printHex(reinterpret_cast<const std::uint8_t*>(&state), sizeof(state));
}

}  // namespace

int main()
{
    std::string line;
    while (std::getline(std::cin, line)) {
        std::istringstream in(line);
        std::string op;
        in >> op;
        if (op == "quit") {
            break;
        }

        std::string firstText;
        std::string secondText;
        std::string thirdText;
        in >> firstText >> secondText >> thirdText;
        const auto first = fromHex(firstText);
        const auto second = fromHex(secondText);
        const char* const firstPtr = textPointer(first, firstText);
        const char* const secondPtr = textPointer(second, secondText);

        if (op == "copylim") {
            const std::uint32_t count = number(secondText);
            std::vector<std::uint8_t> output(static_cast<std::size_t>(count) + 2, 0xA5);
            char* destination = firstText == "-" ? nullptr : reinterpret_cast<char*>(output.data());
            ra3::common::copyStringLimited(destination, firstPtr, count);
            printHex(output.data(), output.size());
        } else if (op == "appendlim") {
            const std::uint32_t count = number(thirdText);
            const std::size_t capacity = first.size() + count + 1;
            std::vector<std::uint8_t> output(capacity, 0xA5);
            std::copy(first.begin(), first.end(), output.begin());
            ra3::common::appendStringLimited(
                reinterpret_cast<char*>(output.data()), secondPtr, count);
            printHex(output.data(), output.size());
        } else if (op == "ncmp" || op == "nicmp") {
            const std::uint32_t count = number(thirdText);
            const int result = op == "ncmp"
                ? ra3::common::compareStringsLimited(firstPtr, secondPtr, count)
                : ra3::common::compareStringsCaseInsensitiveLimited(firstPtr, secondPtr, count);
            std::cout << result << '\n';
        } else if (op == "copy" || op == "lower") {
            std::vector<std::uint8_t> output(first.size() + 2, 0xA5);
            if (op == "copy") {
                ra3::common::copyString(reinterpret_cast<char*>(output.data()), firstPtr);
            } else {
                ra3::common::copyStringLowercase(reinterpret_cast<char*>(output.data()), firstPtr);
            }
            printHex(output.data(), output.size());
        } else if (op == "cmp") {
            std::cout << ra3::common::compareStrings(firstPtr, secondPtr) << '\n';
        } else if (op == "len") {
            std::cout << ra3::common::stringLength(firstPtr) << '\n';
        } else if (op == "rchr") {
            const auto* result = ra3::common::findLastCharacter(
                firstPtr, static_cast<std::uint8_t>(number(secondText)));
            std::cout << (result == nullptr ? -1 : static_cast<int>(result - firstPtr)) << '\n';
        } else if (op == "widen") {
            std::vector<std::uint16_t> output(first.size() + 1, 0xA5A5);
            ra3::common::widenBytes(output.data(), firstPtr);
            printHex(reinterpret_cast<const std::uint8_t*>(output.data()), output.size() * 2);
        } else if (op == "bytes") {
            std::cout << ra3::common::compareBytes(first.data(), second.data(), number(thirdText))
                      << '\n';
        } else if (op == "pcstr") {
            std::cout << ra3::common::compareLengthPrefixedToCString(
                firstPtr, static_cast<std::uint16_t>(number(thirdText)), secondPtr) << '\n';
        } else if (op == "pp") {
            std::string fourthText;
            in >> fourthText;
            std::cout << ra3::common::compareLengthPrefixed(
                firstPtr, static_cast<std::uint16_t>(number(thirdText)), secondPtr,
                static_cast<std::uint16_t>(number(fourthText))) << '\n';
        } else if (op == "fnv") {
            std::cout << ra3::common::hashLowercaseFnv1a(firstPtr) << '\n';
        } else if (op == "decode") {
            std::cout << ra3::common::decodeUtf8(first.data()) << '\n';
        } else if (op == "encode") {
            std::uint8_t output[4]{0xA5, 0xA5, 0xA5, 0xA5};
            ra3::common::encodeUtf8(output, number(firstText));
            printHex(output, sizeof(output));
        } else if (op == "decodeadv") {
            std::uint32_t codePoint = 0;
            const auto* end = ra3::common::decodeUtf8AndAdvance(first.data(), codePoint);
            std::cout << codePoint << ' ' << (end - first.data()) << '\n';
        } else if (op == "rngstep") {
            auto state = randomState(first);
            printRandom(ra3::sim::nextRandom(state), state);
        } else if (op == "rngseed") {
            ra3::sim::RandomState state{};
            ra3::sim::seedRandom(state, number(firstText));
            printHex(reinterpret_cast<const std::uint8_t*>(&state), sizeof(state));
        } else if (op == "rnglogic" || op == "rngclient") {
            auto state = randomState(first);
            const int low = std::stoi(secondText);
            const int high = std::stoi(thirdText);
            const int result = op == "rnglogic"
                ? ra3::sim::logicRandomInteger(state, low, high)
                : ra3::sim::clientRandomInteger(state, low, high);
            printRandom(static_cast<std::uint32_t>(result), state);
        } else {
            std::cout << "error\n";
        }
        std::cout.flush();
    }
    return 0;
}
