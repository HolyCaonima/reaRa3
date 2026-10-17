#include <ra3/common/ThingTemplate.h>

#include <cstdint>
#include <cstring>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

std::uint32_t number(const std::string& text)
{
    return static_cast<std::uint32_t>(std::stoul(text, nullptr, 0));
}

std::uint32_t addressOf(const void* pointer)
{
    return static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(pointer));
}

void storeDword(std::uint8_t* base, std::size_t offset, std::uint32_t value)
{
    std::memcpy(base + offset, &value, sizeof(value));
}

std::string hexDword(std::uint32_t value)
{
    constexpr char digits[] = "0123456789abcdef";
    std::string text(8, '0');
    for (int shift = 28; shift >= 0; shift -= 4) {
        text[static_cast<std::size_t>(7 - shift / 4)] = digits[(value >> shift) & 0xF];
    }
    return text;
}

std::string hexBytes(const std::uint8_t* data, std::size_t size)
{
    constexpr char digits[] = "0123456789abcdef";
    std::string text(size * 2, '0');
    for (std::size_t index = 0; index < size; ++index) {
        text[index * 2] = digits[data[index] >> 4];
        text[index * 2 + 1] = digits[data[index] & 0xF];
    }
    return text;
}

std::vector<std::uint32_t> readWords(std::istream& input, std::size_t count)
{
    std::vector<std::uint32_t> words(count);
    for (std::uint32_t& word : words) {
        std::string text;
        if (!(input >> text)) {
            throw std::runtime_error("missing word");
        }
        word = number(text);
    }
    return words;
}

void writeWords(std::uint8_t* destination, const std::vector<std::uint32_t>& words)
{
    for (std::size_t index = 0; index < words.size(); ++index) {
        storeDword(destination, index * 4, words[index]);
    }
}

constexpr std::size_t kMaskBytes = 0xE8;
constexpr std::size_t kTemplateBytes = 0x310;
constexpr std::uint32_t kGlobalMapAddress = 0x00CC32D0u;

struct ValueBlock {
    std::uint32_t word0;
    std::uint32_t word4;
    std::uint32_t word8;
    std::uint32_t padC;
    std::uint32_t word10;
};

struct MapNode {
    std::uint32_t right;
    std::uint32_t left;
    std::uint32_t pad8;
    std::uint32_t padC;
    std::uint32_t key;
    std::uint32_t value;
};

void plantNode(std::uint8_t* registry, std::size_t mapOffset, MapNode* node)
{
    storeDword(registry, mapOffset + 0x0C, addressOf(node));
}

std::uint32_t runPrimary(std::istream& input)
{
    const std::uint32_t key = number([&] {
        std::string text;
        input >> text;
        return text;
    }());
    std::string mode;
    input >> mode;
    const auto primaryWords = readWords(input, 5);
    const auto secondaryWords = readWords(input, 5);
    alignas(16) std::uint8_t registry[0x80] = {};
    ValueBlock primaryValue{primaryWords[1], primaryWords[2], primaryWords[3], 0, primaryWords[4]};
    ValueBlock secondaryValue{
        secondaryWords[1], secondaryWords[2], secondaryWords[3], 0, secondaryWords[4]};
    MapNode primaryNode{};
    MapNode secondaryNode{};
    primaryNode.key = primaryWords[0];
    secondaryNode.key = secondaryWords[0];
    if (mode == "P" || mode == "B" || mode == "Z" || mode == "N") {
        primaryNode.value = mode == "Z" ? 0u : addressOf(&primaryValue);
        plantNode(registry, 0x40, &primaryNode);
    }
    if (mode == "S" || mode == "B" || mode == "Z" || mode == "N") {
        secondaryNode.value = addressOf(&secondaryValue);
        plantNode(registry, 0x24, &secondaryNode);
    }
    return ra3::common::registryPrimaryValue(registry, key);
}

std::uint32_t runSecondary(std::istream& input)
{
    const auto keyText = [&] {
        std::string text;
        input >> text;
        return text;
    }();
    const std::uint32_t key = number(keyText);
    std::string mode;
    input >> mode;
    const auto primaryWords = readWords(input, 5);
    const auto secondaryWords = readWords(input, 5);
    alignas(16) std::uint8_t registry[0x80] = {};
    ValueBlock primaryValue{primaryWords[1], primaryWords[2], primaryWords[3], 0, primaryWords[4]};
    ValueBlock secondaryValue{
        secondaryWords[1], secondaryWords[2], secondaryWords[3], 0, secondaryWords[4]};
    MapNode primaryNode{};
    MapNode secondaryNode{};
    primaryNode.key = primaryWords[0];
    secondaryNode.key = secondaryWords[0];
    if (mode == "P" || mode == "B" || mode == "Z" || mode == "N") {
        primaryNode.value = mode == "Z" ? 0u : addressOf(&primaryValue);
        plantNode(registry, 0x40, &primaryNode);
    }
    if (mode == "S" || mode == "B" || mode == "Z" || mode == "N") {
        secondaryNode.value = addressOf(&secondaryValue);
        plantNode(registry, 0x24, &secondaryNode);
    }
    return ra3::common::registrySecondaryValue(registry, key);
}

std::uint32_t runPresent(std::istream& input)
{
    const std::uint32_t key = number([&] {
        std::string text;
        input >> text;
        return text;
    }());
    std::string mode;
    input >> mode;
    const std::uint32_t nodeKey = number([&] {
        std::string text;
        input >> text;
        return text;
    }());
    const std::uint32_t residue = number([&] {
        std::string text;
        input >> text;
        return text;
    }());
    alignas(16) std::uint8_t registry[0x80] = {};
    MapNode node{};
    node.key = nodeKey;
    node.value = mode == "Z" ? 0u : residue;
    if (mode != "E") {
        plantNode(registry, 0x24, &node);
    }
    return ra3::common::registryValuePresent(registry, key);
}

std::string runFind(std::istream& input)
{
    const std::uint32_t key = number([&] {
        std::string text;
        input >> text;
        return text;
    }());
    const std::size_t count = static_cast<std::size_t>(number([&] {
        std::string text;
        input >> text;
        return text;
    }()));
    const auto words = readWords(input, count * 5);
    std::vector<std::uint8_t> records(count * 0x14 + 4, 0xA5);
    if (count != 0) {
        writeWords(records.data(), words);
    }
    alignas(16) std::uint8_t container[0x20];
    std::memset(container, 0xA5, sizeof(container));
    const std::uint32_t begin = count == 0 ? 0u : addressOf(records.data());
    const std::uint32_t end = count == 0 ? 0u : begin + static_cast<std::uint32_t>(count * 0x14);
    storeDword(container, 0x14, begin);
    storeDword(container, 0x18, end);
    const std::uint32_t eax = ra3::common::findModuleRecordByKey(container, key);
    return hexDword(eax) + " " + hexDword(begin);
}

std::string runRemove(std::istream& input)
{
    const std::uint32_t incoming = number([&] {
        std::string text;
        input >> text;
        return text;
    }());
    const std::uint32_t mask = number([&] {
        std::string text;
        input >> text;
        return text;
    }());
    const std::size_t firstCount = static_cast<std::size_t>(number([&] {
        std::string text;
        input >> text;
        return text;
    }()));
    const auto firstWords = readWords(input, firstCount * 5);
    const std::size_t secondCount = static_cast<std::size_t>(number([&] {
        std::string text;
        input >> text;
        return text;
    }()));
    auto secondWords = readWords(input, secondCount * 5);
    std::uint32_t dummy = 0x22222222u;
    for (std::size_t index = 0; index < secondCount; ++index) {
        if (secondWords[index * 5 + 1] == 1u) {
            secondWords[index * 5 + 1] = addressOf(&dummy);
        }
    }
    std::vector<std::uint8_t> first(firstCount * 0x14 + 4, 0xA5);
    std::vector<std::uint8_t> second(secondCount * 0x14 + 4, 0xA5);
    if (firstCount != 0) {
        writeWords(first.data(), firstWords);
    }
    if (secondCount != 0) {
        writeWords(second.data(), secondWords);
    }
    alignas(16) std::uint8_t container[0x20];
    std::memset(container, 0xA5, sizeof(container));
    const std::uint32_t firstBegin = firstCount == 0 ? 0u : addressOf(first.data());
    const std::uint32_t firstEnd =
        firstCount == 0 ? 0u : firstBegin + static_cast<std::uint32_t>(firstCount * 0x14);
    const std::uint32_t secondBegin = secondCount == 0 ? 0u : addressOf(second.data());
    const std::uint32_t secondEnd =
        secondCount == 0 ? 0u : secondBegin + static_cast<std::uint32_t>(secondCount * 0x14);
    storeDword(container, 4, firstBegin);
    storeDword(container, 8, firstEnd);
    storeDword(container, 0x14, secondBegin);
    storeDword(container, 0x18, secondEnd);
    alignas(16) std::uint8_t nameMap[0x20] = {};
    const std::uint32_t eax = ra3::common::removeFirstMatchingModuleRecord(
        container, mask, incoming, nameMap, nullptr);
    const std::uint32_t newFirstEnd = [&] {
        std::uint32_t value = 0;
        std::memcpy(&value, container + 8, 4);
        return value;
    }();
    const std::uint32_t newSecondEnd = [&] {
        std::uint32_t value = 0;
        std::memcpy(&value, container + 0x18, 4);
        return value;
    }();
    return hexDword(eax) + " " + hexDword(firstBegin) + " " + hexDword(newFirstEnd) + " "
        + hexDword(secondBegin) + " " + hexDword(newSecondEnd) + " " + hexBytes(first.data(), first.size())
        + " " + hexBytes(second.data(), second.size()) + " " + hexBytes(container, sizeof(container));
}

std::string runMove(std::istream& input)
{
    std::string mode;
    input >> mode;
    if (mode == "null") {
        const std::uint32_t eax = ra3::common::moveModuleRecords(nullptr, nullptr, nullptr, nullptr);
        return hexDword(eax);
    }
    alignas(16) std::uint8_t destination[0x20];
    std::memset(destination, 0xA5, sizeof(destination));
    alignas(16) std::uint8_t records[4] = {0x11, 0x22, 0x33, 0x44};
    const std::uint8_t* begin = records;
    const std::uint32_t eax = ra3::common::moveModuleRecords(begin, begin, destination, nullptr);
    return hexDword(eax) + " " + hexDword(addressOf(destination)) + " "
        + hexBytes(destination, sizeof(destination));
}

std::string runInstall(std::istream& input, bool alternate)
{
    const std::uint32_t key = number([&] {
        std::string text;
        input >> text;
        return text;
    }());
    const std::uint32_t floatBits = number([&] {
        std::string text;
        input >> text;
        return text;
    }());
    const std::uint32_t flag = number([&] {
        std::string text;
        input >> text;
        return text;
    }());
    alignas(16) std::uint8_t object[kTemplateBytes];
    std::memset(object, 0xA5, sizeof(object));
    for (std::size_t base : {std::size_t{0x194}, std::size_t{0x1B8}, std::size_t{0x1DC}, std::size_t{0x200}}) {
        storeDword(object, base + 4, 0);
        storeDword(object, base + 8, 0);
        storeDword(object, base + 0x14, 0);
        storeDword(object, base + 0x18, 0);
    }
    object[0x308] = static_cast<std::uint8_t>(flag);
    alignas(16) std::uint8_t data[0x10] = {};
    storeDword(data, 8, floatBits);
    std::uint32_t pair[3] = {key, addressOf(data), floatBits};
    alignas(16) std::uint8_t registry[0x80] = {};
    alignas(16) std::uint8_t globalMap[0x20] = {};
    const std::uint32_t eax = alternate
        ? ra3::common::installParsedModuleAlternate(
              object, pair, registry, globalMap, kGlobalMapAddress, nullptr)
        : ra3::common::installParsedModule(
              object, pair, registry, globalMap, kGlobalMapAddress, nullptr);
    return hexDword(eax) + " " + hexDword(addressOf(pair)) + " " + hexBytes(object, sizeof(object));
}

std::string runAppend(std::istream& input)
{
    const std::uint32_t flag = number([&] {
        std::string text;
        input >> text;
        return text;
    }());
    const std::uint32_t word = number([&] {
        std::string text;
        input >> text;
        return text;
    }());
    const std::uint32_t filled = number([&] {
        std::string text;
        input >> text;
        return text;
    }());
    const std::string mode = [&] {
        std::string text;
        input >> text;
        return text;
    }();
    alignas(16) std::uint8_t object[kTemplateBytes];
    std::memset(object, 0x5A, sizeof(object));
    alignas(16) std::uint32_t vector[8] = {
        0x11111111u, 0x22222222u, 0x33333333u, 0x44444444u, 0x55555555u, 0x66666666u, 0x77777777u,
        0x88888888u};
    object[0x306] = static_cast<std::uint8_t>(flag);
    if (mode == "nullend") {
        storeDword(object, 0x254, 0);
        storeDword(object, 0x258, 0);
        storeDword(object, 0x25C, 0x100);
    } else {
        storeDword(object, 0x254, addressOf(vector));
        storeDword(object, 0x258, addressOf(vector) + filled * 4);
        storeDword(object, 0x25C, addressOf(vector) + sizeof(vector));
    }
    storeDword(object, 0x270, 0);
    const std::uint32_t eax = ra3::common::appendTemplateWordAndResetMap(object, word);
    return hexDword(eax) + " " + hexDword(addressOf(object)) + " " + hexDword(addressOf(vector)) + " "
        + hexBytes(object, sizeof(object)) + " " + hexBytes(reinterpret_cast<std::uint8_t*>(vector), sizeof(vector));
}

}  // namespace

int main()
{
    try {
        std::string line;
        while (std::getline(std::cin, line)) {
            if (line.empty()) {
                continue;
            }
            std::istringstream input(line);
            std::string command;
            input >> command;
            if (command == "quit") {
                return 0;
            }
            if (command == "allnine" || command == "allc0" || command == "any" || command == "anymutant") {
                if (command == "allc0") {
                    const auto objectWords = readWords(input, 9);
                    const auto query = readWords(input, 9);
                    alignas(16) std::uint8_t object[kMaskBytes];
                    std::memset(object, 0xA5, sizeof(object));
                    writeWords(object + 0xC0, objectWords);
                    const std::uint32_t eax = ra3::common::maskAllNineAtC0(object, query.data());
                    std::cout << hexDword(eax) << " " << hexDword(addressOf(query.data())) << " "
                              << hexBytes(object, sizeof(object)) << "\n";
                } else if (command == "allnine") {
                    const auto objectWords = readWords(input, 9);
                    const auto query = readWords(input, 9);
                    const std::uint32_t eax = ra3::common::maskAllNine(objectWords.data(), query.data());
                    std::cout << hexDword(eax) << " " << hexDword(addressOf(query.data())) << "\n";
                } else {
                    const auto objectWords = readWords(input, 9);
                    const auto query = readWords(input, 9);
                    alignas(16) std::uint8_t object[kMaskBytes];
                    std::memset(object, 0xA5, sizeof(object));
                    writeWords(object + 0xC0, objectWords);
                    const std::uint32_t eax = command == "any"
                        ? ra3::common::maskAnyNineAtC0(object, query.data())
                        : ra3::common::maskAnyNineAtC0Mutant(object, query.data());
                    std::cout << hexDword(eax) << " " << hexBytes(object, sizeof(object)) << "\n";
                }
            } else if (command == "find") {
                std::cout << runFind(input) << "\n";
            } else if (command == "primary") {
                std::cout << hexDword(runPrimary(input)) << "\n";
            } else if (command == "secondary") {
                std::cout << hexDword(runSecondary(input)) << "\n";
            } else if (command == "present") {
                std::cout << hexDword(runPresent(input)) << "\n";
            } else if (command == "remove") {
                std::cout << runRemove(input) << "\n";
            } else if (command == "move") {
                std::cout << runMove(input) << "\n";
            } else if (command == "install" || command == "installb") {
                std::cout << runInstall(input, command == "installb") << "\n";
            } else if (command == "append") {
                std::cout << runAppend(input) << "\n";
            } else {
                throw std::runtime_error("unknown command");
            }
        }
    } catch (const std::exception&) {
        std::cout << "error\n";
        return 1;
    }
    return 0;
}
