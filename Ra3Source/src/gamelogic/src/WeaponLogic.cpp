#include <ra3/gamelogic/WeaponLogic.h>

#include <algorithm>
#include <cmath>
#include <cstring>
#include <limits>

namespace ra3::gamelogic {
namespace {

std::uint32_t floatBits(float value) noexcept
{
    std::uint32_t result = 0;
    std::memcpy(&result, &value, sizeof(result));
    return result;
}

float boundedUnit(float value) noexcept
{
    return std::max(0.0F, std::min(1.0F, value));
}

ProbeObservation integerResult(
    std::uint32_t value, std::uint32_t cleanup = 0) noexcept
{
    ProbeObservation result{};
    result.eax = value;
    result.stackCleanup = cleanup;
    return result;
}

ProbeObservation floatResult(float value, std::uint32_t cleanup = 0) noexcept
{
    ProbeObservation result{};
    result.stackCleanup = cleanup;
    result.st0Bits = floatBits(value);
    return result;
}

std::uint32_t stateCode(const ProbeInput& input) noexcept
{
    return input.words[0] == 0 ? static_cast<std::uint32_t>(WeaponStateType::invalid)
                               : input.words[1];
}

}  // namespace

int secondsToFrames(float seconds, float logicRate, float timeScale) noexcept
{
    if (!std::isfinite(seconds) || !std::isfinite(logicRate)
        || !std::isfinite(timeScale)) {
        return std::numeric_limits<int>::min();
    }
    const double frames = std::ceil(
        static_cast<double>(seconds * logicRate * timeScale));
    if (frames > static_cast<double>(std::numeric_limits<int>::max())
        || frames < static_cast<double>(std::numeric_limits<int>::min())) {
        return std::numeric_limits<int>::min();
    }
    return static_cast<int>(frames);
}

int rangeEndpointToFrames(
    const DurationRange* range, bool high, float logicRate,
    float timeScale) noexcept
{
    if (range == nullptr) {
        return 0;
    }
    return secondsToFrames(
        high ? range->highSeconds : range->lowSeconds, logicRate, timeScale);
}

float weaponAttackRange(
    const WeaponTemplateData& data, float pitchRadians) noexcept
{
    float result = data.attackRange - 5.0F;
    if (-pitchRadians >= data.requestAssistRange) {
        result += data.approachFactor
            - (data.requestAssistRange + pitchRadians) * data.pitchTolerance;
    }
    if ((data.pitchTolerance > 0.0F
         && std::fabs(pitchRadians) > data.pitchTolerance)
        || result < 0.0F) {
        return 0.0F;
    }
    return result;
}

float weaponMinimumAttackRange(const WeaponTemplateData& data) noexcept
{
    return std::max(0.0F, data.minimumAttackRange - 5.0F);
}

bool isStateType(const WeaponRuntimeData& weapon, WeaponStateType type) noexcept
{
    return weapon.state != 0
        && weapon.stateFlags == static_cast<std::uint32_t>(type);
}

bool isFiring(const WeaponRuntimeData& weapon) noexcept
{
    if (weapon.weaponTemplate == nullptr
        || !weapon.weaponTemplate->autoReloads) {
        return false;
    }
    const auto state = static_cast<WeaponStateType>(weapon.stateFlags);
    return state == WeaponStateType::init || state == WeaponStateType::ready
        || state == WeaponStateType::preFire || weapon.firing != 0;
}

bool cachedTargetPosition(
    const WeaponRuntimeData& weapon, std::uint32_t targetObjectId,
    const float* transform3x4, Vector3& output) noexcept
{
    if (weapon.hasCachedTarget == 0) {
        return false;
    }
    if (targetObjectId != 0 && targetObjectId != weapon.cachedTargetObjectId) {
        return false;
    }
    output = weapon.cachedTargetPosition;
    if (weapon.transformCachedTarget == 0 || transform3x4 == nullptr) {
        return true;
    }
    const Vector3 input = output;
    output.x = transform3x4[0] * input.x + transform3x4[1] * input.y
        + transform3x4[2] * input.z + transform3x4[3];
    output.y = transform3x4[4] * input.x + transform3x4[5] * input.y
        + transform3x4[6] * input.z + transform3x4[7];
    output.z = transform3x4[8] * input.x + transform3x4[9] * input.y
        + transform3x4[10] * input.z + transform3x4[11];
    return true;
}

ProbeObservation evaluateWeaponEntry(
    WeaponEntry entry, const ProbeInput& input) noexcept
{
    const std::uint32_t now = input.words[0];
    const std::uint32_t start = input.words[1];
    const std::uint32_t duration = input.words[2];
    const std::uint32_t deadline = start + duration;
    const float logicRate = input.values[6];
    const float timeScale = input.values[7];

    switch (entry) {
    case WeaponEntry::initUpdate:
        return integerResult(input.words[0] != 0 ? 0xFFFFFFFFU : input.words[1]);
    case WeaponEntry::fireUpdate: {
        ProbeObservation result = integerResult(
            now > deadline ? input.words[4] : 0xFFFFFFFFU);
        if (now > deadline && input.words[3] == 0) {
            result.writes[0] = 0;
            result.writeMask = 1;
        }
        return result;
    }
    case WeaponEntry::stateDeadlineUpdate:
        return integerResult(now > deadline ? 0U : 0xFFFFFFFFU);
    case WeaponEntry::zeroAimValue:
        return floatResult(0.0F, 8);
    case WeaponEntry::attackRangeForPitch: {
        WeaponTemplateData data{};
        data.attackRange = input.values[0];
        data.requestAssistRange = input.values[1];
        data.approachFactor = input.values[2];
        data.pitchTolerance = input.values[3];
        return floatResult(weaponAttackRange(data, input.values[4]), 4);
    }
    case WeaponEntry::minimumAttackRange:
        return floatResult(std::max(0.0F, input.values[0] - 5.0F));
    case WeaponEntry::attackRange:
        return floatResult(input.values[0]);
    case WeaponEntry::preAttackLowFrames:
    case WeaponEntry::firingLowFrames:
    case WeaponEntry::coolDownLowFrames:
    case WeaponEntry::reloadLowFrames:
        return integerResult(static_cast<std::uint32_t>(
            secondsToFrames(input.values[0], logicRate, timeScale)));
    case WeaponEntry::preAttackHighFrames:
    case WeaponEntry::firingHighFrames:
    case WeaponEntry::coolDownHighFrames:
    case WeaponEntry::reloadHighFrames:
        return integerResult(static_cast<std::uint32_t>(
            secondsToFrames(input.values[1], logicRate, timeScale)));
    case WeaponEntry::idleAfterFiringFrames:
    case WeaponEntry::holdAfterFiringFrames:
    case WeaponEntry::suspendFxFrames:
        return integerResult(static_cast<std::uint32_t>(
            secondsToFrames(input.values[0], logicRate, timeScale)));
    case WeaponEntry::firstWeaponAndSlot: {
        ProbeObservation result = integerResult(input.words[0], 4);
        if (input.words[0] != 0 && input.words[1] != 0) {
            result.writes[0] = std::min(input.words[2], 5U);
            result.writeMask = 1;
        }
        return result;
    }
    case WeaponEntry::weaponAtSlot:
        return integerResult(input.words[input.words[7] % 6U], 4);
    case WeaponEntry::currentWeaponInactive:
        return integerResult(
            input.words[0] < 6U && input.words[1] == 4U
                    && input.words[2] != 0 && input.words[3] == 0
                ? 1U
                : 0U);
    case WeaponEntry::ownerWeaponSetToken:
        return integerResult(input.words[0] != 0 ? input.words[1] : 0U);
    case WeaponEntry::rebuildWeaponMasks: {
        ProbeObservation result{};
        for (std::size_t index = 0; index != 6; ++index) {
            result.writes[0] |= input.words[index];
            result.writes[1] |= (input.words[index] << 1U);
        }
        result.eax = input.words[5] << 1U;
        result.writeMask = 3;
        return result;
    }
    case WeaponEntry::longestRangeWeaponPosition: {
        std::size_t best = 0;
        for (std::size_t index = 1; index != 6; ++index) {
            if (input.words[index] > input.words[best]) {
                best = index;
            }
        }
        ProbeObservation result = integerResult(input.words[7], 4);
        result.writes = {
            floatBits(input.values[best]),
            floatBits(input.values[(best + 1U) % input.values.size()]),
            floatBits(input.values[(best + 2U) % input.values.size()]), 0};
        result.writeMask = 7;
        return result;
    }
    case WeaponEntry::matchingAntiMaskCount: {
        std::uint32_t count = 0;
        for (std::size_t index = 0; index != 6; ++index) {
            if ((input.words[index] & input.words[6]) != 0) {
                count += input.words[index] & 0xFFU;
            }
        }
        return integerResult(count, 8);
    }
    case WeaponEntry::findWeaponTemplate:
        return integerResult(
            input.words[0] == input.words[6] ? input.words[7] : 0U, 8);
    case WeaponEntry::effectiveMinimumRange:
        return floatResult(
            input.words[0] != 0 ? input.values[1]
                                : std::max(0.0F, input.values[0] - 5.0F),
            12);
    case WeaponEntry::stateDeadline:
        return integerResult(deadline);
    case WeaponEntry::releaseInitState:
        return integerResult(input.words[0], 4);
    case WeaponEntry::chooseStateDuration: {
        const auto span = input.words[2] - input.words[1] + 1U;
        const auto random = input.words[3] % span + input.words[1];
        const float divisor = input.values[0] == 0.0F ? 1.0F : input.values[0];
        return integerResult(static_cast<std::uint32_t>(
            static_cast<float>(random) / divisor), 8);
    }
    case WeaponEntry::refreshTargetId: {
        ProbeObservation result{};
        if (input.words[0] == 0 && input.words[1] != 0) {
            result.writes[0] = input.words[2];
            result.writeMask = 1;
        }
        return result;
    }
    case WeaponEntry::updateReadyFraction: {
        const float denominator =
            static_cast<float>(std::max(input.words[2], 1U));
        const float value = boundedUnit(
            static_cast<float>(now - start) / denominator);
        ProbeObservation result = integerResult(
            now > deadline ? input.words[4] : 0xFFFFFFFFU);
        result.writes[0] = floatBits(value);
        result.writeMask = 1;
        return result;
    }
    case WeaponEntry::weaponSpeedMultiplier:
        return floatResult(
            input.words[0] != 0 ? input.values[0] * input.values[1] : 1.0F,
            4);
    case WeaponEntry::attackRangeForTarget: {
        WeaponTemplateData data{};
        data.attackRange = input.values[0];
        data.requestAssistRange = input.values[1];
        data.approachFactor = input.values[2];
        data.pitchTolerance = input.values[3];
        return floatResult(weaponAttackRange(data, -input.values[4]), 4);
    }
    case WeaponEntry::acceptsTargetKind: {
        if ((input.words[2] & 1U) != 0) {
            return integerResult(1, 12);
        }
        const std::uint32_t kind =
            input.words[1] == 2 ? 1U : (input.words[1] != 0 ? 3U : 2U);
        return integerResult(
            (input.words[0] & (1U << (kind & 31U))) != 0 ? 1U : 0U, 12);
    }
    case WeaponEntry::targetEffectValue:
        return integerResult(
            input.words[0] != 0 && input.words[1] != 0
                    && input.words[2] == 4
                ? input.words[3]
                : input.words[4],
            4);
    case WeaponEntry::initializeWeapon: {
        ProbeObservation result = integerResult(input.words[7], 8);
        result.writes = {input.words[0], input.words[1], 0x7FFFFFFFU, 0xFFFFFFFFU};
        result.writeMask = 15;
        return result;
    }
    case WeaponEntry::targetWithinMinimumRange: {
        const float range = std::max(0.0F, input.values[0] - 5.0F);
        return integerResult(
            range > 0.0F && input.values[1] * input.values[1] <= range * range,
            8);
    }
    case WeaponEntry::readinessFraction:
        return floatResult(
            input.words[0] == 0 || input.words[1] > 5U ? 0.0F
                                : boundedUnit(static_cast<float>(now - start)
                                              / std::max(input.values[0], 1.0F)));
    case WeaponEntry::acceptsObjectFilter:
        return integerResult(
            (input.words[0] | input.words[1]) == 0
                    || (input.words[0] & input.words[2]) != 0,
            4);
    case WeaponEntry::isPreFireState:
        return integerResult(stateCode(input) == 2U ? 1U : 0U);
    case WeaponEntry::targetWithinPitch:
        return integerResult(
            input.words[0] != 0 || input.words[1] == 0
                    || std::fabs(input.values[0]) < 10.0F,
            8);
    case WeaponEntry::suppressesAttack:
        return integerResult(
            input.words[0] != 0 && stateCode(input) == 5U ? 0U : input.words[3],
            4);
    case WeaponEntry::canFireNow: {
        const auto state = stateCode(input);
        return integerResult(
            input.words[2] != 0
                && (state == 0U || state == 1U || state == 2U
                    || input.words[3] != 0));
    }
    case WeaponEntry::getCachedTargetPosition: {
        const bool valid = input.words[0] != 0
            && (input.words[1] == 0 || input.words[1] == input.words[2]);
        ProbeObservation result = integerResult(valid ? 1U : 0U, 8);
        if (valid) {
            result.writes = {floatBits(input.values[0]), floatBits(input.values[1]),
                             floatBits(input.values[2]), 0};
            result.writeMask = 7;
        }
        return result;
    }
    case WeaponEntry::coolDownDeadline:
        return integerResult(
            deadline
            + (input.words[3] > 0 ? (input.words[4] + input.words[5] + 1U) / 2U
                                  : 0U));
    case WeaponEntry::enterCoolDownState: {
        ProbeObservation result = integerResult(input.words[7]);
        result.writes = {now, input.words[1], 3U, 0};
        result.writeMask = 7;
        return result;
    }
    case WeaponEntry::coolDownUpdate:
        return integerResult(input.words[0] > 0 ? 0xFFFFFFFEU : 0U);
    case WeaponEntry::reloadUpdate: {
        ProbeObservation result =
            integerResult(input.words[0] > input.words[1] ? 0xFFFFFFFFU : 0U);
        result.writes[0] = input.words[2] == 0 ? 0x7FFFFFFFU : input.words[2];
        result.writeMask = 1;
        return result;
    }
    case WeaponEntry::enterReloadState: {
        ProbeObservation result = integerResult(0, 4);
        result.writes = {0U, input.words[0] & ~0x4000U, 0U, 0U};
        result.writeMask = 3;
        return result;
    }
    default:
        return {};
    }
}

const char* weaponEntryName(WeaponEntry entry) noexcept
{
#define RA3_WEAPON_NAME(value) case WeaponEntry::value: return #value
    switch (entry) {
    RA3_WEAPON_NAME(initUpdate);
    RA3_WEAPON_NAME(fireUpdate);
    RA3_WEAPON_NAME(stateDeadlineUpdate);
    RA3_WEAPON_NAME(zeroAimValue);
    RA3_WEAPON_NAME(attackRangeForPitch);
    RA3_WEAPON_NAME(minimumAttackRange);
    RA3_WEAPON_NAME(attackRange);
    RA3_WEAPON_NAME(preAttackLowFrames);
    RA3_WEAPON_NAME(preAttackHighFrames);
    RA3_WEAPON_NAME(firingLowFrames);
    RA3_WEAPON_NAME(firingHighFrames);
    RA3_WEAPON_NAME(coolDownLowFrames);
    RA3_WEAPON_NAME(coolDownHighFrames);
    RA3_WEAPON_NAME(reloadLowFrames);
    RA3_WEAPON_NAME(reloadHighFrames);
    RA3_WEAPON_NAME(idleAfterFiringFrames);
    RA3_WEAPON_NAME(holdAfterFiringFrames);
    RA3_WEAPON_NAME(suspendFxFrames);
    RA3_WEAPON_NAME(firstWeaponAndSlot);
    RA3_WEAPON_NAME(weaponAtSlot);
    RA3_WEAPON_NAME(currentWeaponInactive);
    RA3_WEAPON_NAME(ownerWeaponSetToken);
    RA3_WEAPON_NAME(rebuildWeaponMasks);
    RA3_WEAPON_NAME(longestRangeWeaponPosition);
    RA3_WEAPON_NAME(matchingAntiMaskCount);
    RA3_WEAPON_NAME(findWeaponTemplate);
    RA3_WEAPON_NAME(effectiveMinimumRange);
    RA3_WEAPON_NAME(stateDeadline);
    RA3_WEAPON_NAME(releaseInitState);
    RA3_WEAPON_NAME(chooseStateDuration);
    RA3_WEAPON_NAME(refreshTargetId);
    RA3_WEAPON_NAME(updateReadyFraction);
    RA3_WEAPON_NAME(weaponSpeedMultiplier);
    RA3_WEAPON_NAME(attackRangeForTarget);
    RA3_WEAPON_NAME(acceptsTargetKind);
    RA3_WEAPON_NAME(targetEffectValue);
    RA3_WEAPON_NAME(initializeWeapon);
    RA3_WEAPON_NAME(targetWithinMinimumRange);
    RA3_WEAPON_NAME(readinessFraction);
    RA3_WEAPON_NAME(acceptsObjectFilter);
    RA3_WEAPON_NAME(isPreFireState);
    RA3_WEAPON_NAME(targetWithinPitch);
    RA3_WEAPON_NAME(suppressesAttack);
    RA3_WEAPON_NAME(canFireNow);
    RA3_WEAPON_NAME(getCachedTargetPosition);
    RA3_WEAPON_NAME(coolDownDeadline);
    RA3_WEAPON_NAME(enterCoolDownState);
    RA3_WEAPON_NAME(coolDownUpdate);
    RA3_WEAPON_NAME(reloadUpdate);
    RA3_WEAPON_NAME(enterReloadState);
    }
#undef RA3_WEAPON_NAME
    return "unknown";
}

}  // namespace ra3::gamelogic
