#include <ra3/gamelogic/WeaponLogic.h>

#include <array>
#include <cstdint>
#include <cstring>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

std::uint32_t number(const std::string& text)
{
    return static_cast<std::uint32_t>(std::stoul(text, nullptr, 16));
}

float floatValue(const std::string& text)
{
    const std::uint32_t bits = number(text);
    float value = 0.0F;
    std::memcpy(&value, &bits, sizeof(value));
    return value;
}

bool hasFloatReturn(ra3::gamelogic::WeaponEntry entry)
{
    using ra3::gamelogic::WeaponEntry;
    switch (entry) {
    case WeaponEntry::zeroAimValue:
    case WeaponEntry::attackRangeForPitch:
    case WeaponEntry::minimumAttackRange:
    case WeaponEntry::attackRange:
    case WeaponEntry::effectiveMinimumRange:
    case WeaponEntry::weaponSpeedMultiplier:
    case WeaponEntry::attackRangeForTarget:
    case WeaponEntry::readinessFraction:
        return true;
    default:
        return false;
    }
}

std::array<std::uint8_t, 10> extendedValue(std::uint32_t bits)
{
    std::array<std::uint8_t, 10> result{};
#if defined(_MSC_VER) && defined(_M_IX86)
    float value = 0.0F;
    std::memcpy(&value, &bits, sizeof(value));
    std::uint8_t* output = result.data();
    __asm {
        fld value
        mov eax, output
        fstp tbyte ptr [eax]
    }
#else
    (void)bits;
#endif
    return result;
}

void probe(const std::vector<std::string>& words, bool mutate)
{
    if (words.size() != 18U) {
        throw std::runtime_error("probe argument count");
    }
    ra3::gamelogic::ProbeInput input{};
    for (std::size_t index = 0; index != input.words.size(); ++index) {
        input.words[index] = number(words[index + 2U]);
        input.values[index] = floatValue(words[index + 10U]);
    }
    const auto entry = static_cast<ra3::gamelogic::WeaponEntry>(number(words[1]));
    auto result = ra3::gamelogic::evaluateWeaponEntry(entry, input);
    if (mutate) {
        if (hasFloatReturn(entry)) {
            result.st0Bits ^= 0x00800000U;
        } else if (result.writeMask != 0) {
            result.writes[0] ^= 1U;
        } else {
            result.eax ^= 1U;
        }
    }
    const auto flags = std::cout.flags();
    const char fill = std::cout.fill();
    std::cout << std::hex << std::setfill('0')
              << std::setw(8) << result.eax
              << std::setw(8) << result.edx
              << std::setw(8) << result.stackCleanup
              << std::setw(8) << result.st0Bits
              << std::setw(8) << result.writeMask;
    for (const std::uint32_t value : result.writes) {
        std::cout << std::setw(8) << value;
    }
    const auto extended =
        hasFloatReturn(entry) ? extendedValue(result.st0Bits)
                              : std::array<std::uint8_t, 10>{};
    for (const std::uint8_t value : extended) {
        std::cout << std::setw(2) << static_cast<unsigned>(value);
    }
    std::cout.flags(flags);
    std::cout.fill(fill);
    std::cout << '\n';
}

void run(const std::vector<std::string>& words)
{
    if (words.empty()) {
        throw std::runtime_error("empty command");
    }
    if (words[0] == "probe") {
        probe(words, false);
        return;
    }
    if (words[0] == "mutate") {
        probe(words, true);
        return;
    }
    if (words[0] == "name" && words.size() == 2U) {
        const auto entry =
            static_cast<ra3::gamelogic::WeaponEntry>(number(words[1]));
        std::cout << ra3::gamelogic::weaponEntryName(entry) << '\n';
        return;
    }
    if (words[0] == "alias" && words.size() == 1U) {
        ra3::gamelogic::WeaponRuntimeData weapon{};
        weapon.hasCachedTarget = 1;
        weapon.cachedTargetObjectId = 20;
        weapon.cachedTargetPosition = {2.0F, 3.0F, 4.0F};
        const bool valid = ra3::gamelogic::cachedTargetPosition(
            weapon, 20, nullptr, weapon.cachedTargetPosition);
        std::array<std::uint32_t, 3> bits{};
        std::memcpy(bits.data(), &weapon.cachedTargetPosition, sizeof(bits));
        const auto flags = std::cout.flags();
        const char fill = std::cout.fill();
        std::cout << (valid ? '1' : '0') << std::hex << std::setfill('0');
        for (const std::uint32_t value : bits) {
            std::cout << std::setw(8) << value;
        }
        std::cout.flags(flags);
        std::cout.fill(fill);
        std::cout << '\n';
        return;
    }
    throw std::runtime_error("unknown command");
}

}  // namespace

int main()
{
    std::string line;
    while (std::getline(std::cin, line)) {
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
