#include <ra3/gamelogic/WeaponLogic.h>

#include <algorithm>
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

std::uint32_t floatBits(float value)
{
    std::uint32_t bits = 0;
    std::memcpy(&bits, &value, sizeof(bits));
    return bits;
}

bool hasFloatReturn(ra3::gamelogic::WeaponEntry entry)
{
    using ra3::gamelogic::WeaponEntry;
    switch (entry) {
    case WeaponEntry::zeroAimValue:
    case WeaponEntry::effectiveMinimumRange:
    case WeaponEntry::weaponSpeedMultiplier:
    case WeaponEntry::attackRangeForTarget:
    case WeaponEntry::readinessFraction:
        return true;
    default:
        return false;
    }
}

ra3::gamelogic::ProbeObservation mutatedProbe(
    ra3::gamelogic::WeaponEntry entry,
    const ra3::gamelogic::ProbeInput& input)
{
    using ra3::gamelogic::WeaponEntry;
    using ra3::gamelogic::evaluateWeaponEntry;
    auto result = evaluateWeaponEntry(entry, input);
    switch (entry) {
    case WeaponEntry::zeroAimValue:
        result.st0Bits = floatBits(1.0F);  // wrong neutral aim
        break;
    case WeaponEntry::firstWeaponAndSlot:
        result.eax = input.words[5];  // scans from the wrong end
        break;
    case WeaponEntry::weaponAtSlot:
        result.eax = input.words[(input.words[7] + 1U) % 6U];
        break;
    case WeaponEntry::currentWeaponInactive:
        result.eax = 0;  // omits invalid-slot handling
        break;
    case WeaponEntry::rebuildWeaponMasks:
        result.writes[0] &= ~input.words[5];
        result.writes[1] &= ~(input.words[5] << 1U);
        break;
    case WeaponEntry::longestRangeWeaponPosition:
        result.writes = {
            floatBits(input.values[0]), floatBits(input.values[1]),
            floatBits(input.values[2]), 0};
        break;
    case WeaponEntry::matchingAntiMaskCount:
        result.eax += 1;  // counts a rejected mask as one match
        break;
    case WeaponEntry::findWeaponTemplate:
        result.eax = input.words[7];  // accepts a non-matching template
        break;
    case WeaponEntry::effectiveMinimumRange:
        result.st0Bits = floatBits(input.values[0]);  // omits range inset
        break;
    case WeaponEntry::stateDeadline:
        result.eax = input.words[1];  // omits duration
        break;
    case WeaponEntry::chooseStateDuration:
        result.eax += 1;  // inclusive random-range off-by-one
        break;
    case WeaponEntry::updateReadyFraction:
        result.eax = 0;  // omits transition to the next state
        break;
    case WeaponEntry::weaponSpeedMultiplier:
        result.st0Bits = floatBits(input.values[0]);  // omits settings multiplier
        break;
    case WeaponEntry::attackRangeForTarget:
        result.st0Bits = floatBits(
            std::max(0.0F, input.values[0] - 5.0F));  // omits pitch branch
        break;
    case WeaponEntry::targetEffectValue:
        result.eax = input.words[3];  // ignores fallback effect
        break;
    case WeaponEntry::initializeWeapon:
        break;  // layout_mutate changes the constructor's frame sentinel
    case WeaponEntry::targetWithinMinimumRange:
        result.eax = input.values[1] < input.values[0] ? 1U : 0U;
        break;
    case WeaponEntry::readinessFraction: {
        auto changed = input;
        changed.words[1] += 1U;  // wrong progress denominator
        result = evaluateWeaponEntry(entry, changed);
        break;
    }
    case WeaponEntry::acceptsObjectFilter:
        result.eax = input.words[0] == 0 ? 1U : 0U;
        break;
    case WeaponEntry::isPreFireState:
        result.eax = input.words[1] == 3U ? 1U : 0U;
        break;
    case WeaponEntry::targetWithinPitch:
        result.eax = input.values[2] >= input.values[4]
                && input.values[2] <= input.values[5]
            ? 1U : 0U;
        break;
    case WeaponEntry::suppressesAttack:
        result.eax = input.words[0] != 0 && input.words[1] == 4U
            ? 0U : input.words[3];
        break;
    case WeaponEntry::canFireNow:
        result.eax = input.words[3] == 1U ? 1U : 0U;
        break;
    case WeaponEntry::getCachedTargetPosition:
        if (result.writeMask != 0) {
            result.writes[0] = floatBits(input.values[3] * input.values[0]);
        } else {
            result.eax = 1;  // accepts a stale cache
        }
        break;
    case WeaponEntry::coolDownDeadline:
        result.eax -= (input.words[4] + input.words[5] + 1U) / 2U;
        break;
    case WeaponEntry::enterCoolDownState:
        result.writes[2] = 4;  // enters reload instead of cooldown
        break;
    case WeaponEntry::coolDownUpdate:
        result.eax = result.eax == 0 ? 0xFFFFFFFEU : 0U;
        break;
    case WeaponEntry::reloadUpdate:
        result.writes[0] = input.words[2];  // omits infinite-ammo sentinel
        break;
    case WeaponEntry::enterReloadState:
        result.writes[1] = input.words[0];  // fails to clear reload flag
        break;
    }
    return result;
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

std::array<std::uint8_t, 10> readinessExtended(
    const ra3::gamelogic::ProbeInput& input, std::uint32_t bits)
{
    const std::uint32_t now = input.words[0];
    const std::uint32_t finish = input.words[1];
    const std::uint32_t stateStart = input.words[3];
    const std::uint32_t state = input.words[5] != 0
        ? input.words[4] : 0xF423FU;
    if (input.words[2] == 0 || state == 0U || state == 1U
        || state == 4U || state > 5U || now >= finish) {
        return extendedValue(bits);
    }
    const std::uint32_t duration = finish - stateStart;
    const std::uint32_t elapsed = now - stateStart;
    if (duration == 0U || elapsed >= duration) {
        return extendedValue(bits);
    }
    std::array<std::uint8_t, 10> result{};
#if defined(_MSC_VER) && defined(_M_IX86)
    const std::uint32_t* numerator = &elapsed;
    const std::uint32_t* denominator = &duration;
    std::uint8_t* output = result.data();
    __asm {
        mov eax, numerator
        fild dword ptr [eax]
        mov eax, denominator
        fild dword ptr [eax]
        fdivp st(1), st(0)
        mov eax, output
        fstp tbyte ptr [eax]
    }
#else
    (void)input;
#endif
    return result;
}

void setFpuControlWord(std::uint16_t value)
{
#if defined(_MSC_VER) && defined(_M_IX86)
    __asm {
        fldcw value
    }
#else
    (void)value;
#endif
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
    auto result = mutate
        ? mutatedProbe(entry, input)
        : ra3::gamelogic::evaluateWeaponEntry(entry, input);
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
    auto extendedInput = input;
    if (mutate
        && entry == ra3::gamelogic::WeaponEntry::readinessFraction) {
        extendedInput.words[1] += 1U;
    }
    const auto extended = entry == ra3::gamelogic::WeaponEntry::readinessFraction
        ? readinessExtended(extendedInput, result.st0Bits)
        : (hasFloatReturn(entry) ? extendedValue(result.st0Bits)
                                 : std::array<std::uint8_t, 10>{});
    for (const std::uint8_t value : extended) {
        std::cout << std::setw(2) << static_cast<unsigned>(value);
    }
    std::cout.flags(flags);
    std::cout.fill(fill);
    std::cout << '\n';
}

void layout(const std::vector<std::string>& words, bool mutate)
{
    if (words.size() != 18U) {
        throw std::runtime_error("layout argument count");
    }
    ra3::gamelogic::ProbeInput input{};
    for (std::size_t index = 0; index != input.words.size(); ++index) {
        input.words[index] = number(words[index + 2U]);
        input.values[index] = floatValue(words[index + 10U]);
    }
    ra3::gamelogic::WeaponTemplateData weaponTemplate{};
    weaponTemplate.minimumPitch = input.values[0];
    weaponTemplate.maximumPitch = input.values[1];
    weaponTemplate.maxShots = input.words[4];
    ra3::gamelogic::WeaponRuntimeData weapon;
    std::memset(&weapon, 0xA5, sizeof(weapon));
    const auto* templatePointer =
        input.words[0] != 0 ? &weaponTemplate : nullptr;
    ra3::gamelogic::initializeWeaponRuntime(
        weapon, templatePointer, input.words[1], input.words[2],
        input.words[3]);
    if (mutate) {
        weapon.lastShotFrame = 0;
    }
    weapon.weaponTemplate = reinterpret_cast<
        const ra3::gamelogic::WeaponTemplateData*>(
            static_cast<std::uintptr_t>(input.words[0] != 0 ? 1U : 0U));
    const auto flags = std::cout.flags();
    const char fill = std::cout.fill();
    std::cout << "L00000008" << std::hex << std::setfill('0');
    const auto* bytes = reinterpret_cast<const std::uint8_t*>(&weapon);
    for (std::size_t index = 0; index != sizeof(weapon); ++index) {
        std::cout << std::setw(2) << static_cast<unsigned>(bytes[index]);
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
    if (words[0] == "layout") {
        layout(words, false);
        return;
    }
    if (words[0] == "layout_mutate") {
        layout(words, true);
        return;
    }
    if (words[0] == "fpcw" && words.size() == 2U) {
        setFpuControlWord(static_cast<std::uint16_t>(number(words[1])));
        std::cout << "ok\n";
        return;
    }
    if (words[0] == "name" && words.size() == 2U) {
        const auto entry =
            static_cast<ra3::gamelogic::WeaponEntry>(number(words[1]));
        std::cout << ra3::gamelogic::weaponEntryName(entry) << '\n';
        return;
    }
    if ((words[0] == "alias" || words[0] == "alias_mutate")
        && words.size() == 1U) {
        ra3::gamelogic::WeaponRuntimeData weapon{};
        weapon.hasCachedTarget = 1;
        weapon.transformCachedTarget = 1;
        weapon.cachedTargetObjectId = 20;
        weapon.cachedTargetPosition = {2.0F, 3.0F, 4.0F};
        const std::array<float, 12> transform{
            2.0F, 0.0F, 0.0F, 10.0F,
            0.0F, 3.0F, 0.0F, 20.0F,
            0.0F, 0.0F, 4.0F, 30.0F};
        bool valid = false;
        if (words[0] == "alias_mutate") {
            valid = true;
            weapon.cachedTargetPosition.x =
                2.0F * weapon.cachedTargetPosition.x + 10.0F;
            weapon.cachedTargetPosition.y =
                3.0F * weapon.cachedTargetPosition.x + 20.0F;
            weapon.cachedTargetPosition.z =
                4.0F * weapon.cachedTargetPosition.z + 30.0F;
        } else {
            valid = ra3::gamelogic::cachedTargetPosition(
                weapon, 20, transform.data(), weapon.cachedTargetPosition);
        }
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
