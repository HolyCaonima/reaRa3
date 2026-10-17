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

void initializeWeaponRuntime(
    WeaponRuntimeData& weapon, const WeaponTemplateData* weaponTemplate,
    std::uint32_t slot, std::uint32_t now,
    std::uint32_t suspendFxFrames) noexcept
{
    const std::array<std::uint8_t, 3> reserved{
        weapon.reserved31[0], weapon.reserved31[1], weapon.reserved31[2]};
    weapon = {};
    weapon.reserved31[0] = reserved[0];
    weapon.reserved31[1] = reserved[1];
    weapon.reserved31[2] = reserved[2];
    weapon.vtable = 0x00C07D54U;
    weapon.weaponTemplate = weaponTemplate;
    weapon.target = slot;
    weapon.nextReadyFrame =
        weaponTemplate == nullptr ? 0U : now + suspendFxFrames;
    weapon.lastShotFrame = 0x7FFFFFFFU;
    weapon.maxShots =
        weaponTemplate == nullptr ? 1U : weaponTemplate->maxShots;
    weapon.pitchLimited = static_cast<std::uint8_t>(
        weaponTemplate != nullptr
        && (weaponTemplate->minimumPitch > -3.1415927410125732F
            || weaponTemplate->maximumPitch < 3.1415927410125732F));
    weapon.aimPitch = -1.0F;
    weapon.minimumAimPitch = -1.0F;
    weapon.maximumAimPitch = -1.0F;
    weapon.ammo = -1;
}

ProbeObservation evaluateWeaponEntry(
    WeaponEntry entry, const ProbeInput& input) noexcept
{
    const std::uint32_t now = input.words[0];
    const std::uint32_t start = input.words[1];
    const std::uint32_t duration = input.words[2];
    const std::uint32_t deadline = start + duration;

    switch (entry) {
    case WeaponEntry::zeroAimValue:
        return floatResult(0.0F, 8);
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
        if (static_cast<std::int32_t>(input.words[0]) < 0
            || input.words[0] >= 6U) {
            return integerResult(1);
        }
        if (input.words[1] != 4U || input.words[2] == 0) {
            return integerResult(0);
        }
        return integerResult(input.words[3] == 0 ? 1U : 0U);
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
            input.words[0] != 0 && input.words[0] == input.words[6]
                ? input.words[7] : 0U, 8);
    case WeaponEntry::effectiveMinimumRange:
        if (input.words[0] != 0) {
            return floatResult(input.values[1], 12);
        } else {
            const float minimum = std::max(0.0F, input.values[0] - 5.0F);
            if (minimum > input.values[1]) {
                return floatResult(minimum - input.values[1], 12);
            }
            if (input.values[1] > input.values[2]) {
                return floatResult(input.values[1] - input.values[2], 12);
            }
            return floatResult(0.0F, 12);
        }
    case WeaponEntry::stateDeadline:
        return integerResult(deadline);
    case WeaponEntry::chooseStateDuration: {
        const auto span = input.words[2] - input.words[1] + 1U;
        const auto random = input.words[3] % span + input.words[1];
        const float divisor = input.values[0] == 0.0F ? 1.0F : input.values[0];
        return integerResult(static_cast<std::uint32_t>(
            static_cast<float>(random) / divisor), 8);
    }
    case WeaponEntry::updateReadyFraction: {
        if (input.words[3] == 0 || input.words[4] == 0xFFFFFFFFU) {
            return integerResult(0xFFFFFFFEU);
        }
        const float denominator =
            static_cast<float>(std::max(input.words[2], 1U));
        const float value = boundedUnit(
            static_cast<float>(now - start) / denominator);
        const std::uint32_t transition = deadline <= now
            ? 0xFFFFFFFFU
            : (input.words[5] != 0 ? 0xFFFFFFFFU : 0U);
        ProbeObservation result = integerResult(transition);
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
    case WeaponEntry::targetEffectValue:
        return integerResult(
            input.words[3] != 0 && input.words[2] == 4
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
            range > 0.0F && input.values[1] < range * range,
            8);
    }
    case WeaponEntry::readinessFraction: {
        if (input.words[2] == 0) {
            return floatResult(1.0F);
        }
        const std::uint32_t state = input.words[5] == 0
            ? static_cast<std::uint32_t>(WeaponStateType::invalid)
            : input.words[4];
        if (state == 0U) {
            return floatResult(1.0F);
        }
        if (state == 1U || state == 4U || state > 5U) {
            return floatResult(0.0F);
        }
        const std::uint32_t finish = input.words[1];
        const std::uint32_t stateStart = input.words[3];
        if (now >= finish) {
            return floatResult(1.0F);
        }
        const std::uint32_t stateDuration = finish - stateStart;
        const std::uint32_t elapsed = now - stateStart;
        if (stateDuration == 0U || elapsed >= stateDuration) {
            return floatResult(1.0F);
        }
        return floatResult(
            static_cast<float>(elapsed) / static_cast<float>(stateDuration));
    }
    case WeaponEntry::acceptsObjectFilter:
        if (input.words[0] != 0 && input.words[2] != 0) {
            return integerResult(0, 4);
        }
        return integerResult(
            input.words[1] == 0 || input.words[3] != 0, 4);
    case WeaponEntry::isPreFireState:
        return integerResult(
            input.words[0] != 0 && input.words[2] != 0
                && input.words[1] == 2U,
            0);
    case WeaponEntry::targetWithinPitch: {
        if (input.words[0] != 0 || input.words[1] == 0
            || std::fabs(input.values[1] - input.values[0]) < 10.0F) {
            return integerResult(1, 8);
        }
        const float first = input.values[2];
        const float second = input.values[3];
        const float minimum = input.values[4];
        const float maximum = input.values[5];
        const bool firstInside = first >= minimum && first <= maximum;
        const bool secondInside = second >= minimum && second <= maximum;
        return integerResult(
            firstInside || secondInside
                || (first <= minimum && second >= maximum),
            8);
    }
    case WeaponEntry::suppressesAttack:
        return integerResult(
            input.words[2] != 0 && input.words[0] != 0
                    && input.words[1] == 5U
                ? 0U : input.words[3],
            4);
    case WeaponEntry::canFireNow: {
        if (input.words[0] == 0) {
            return integerResult(0);
        }
        const auto state = input.words[1] == 0
            ? 0U
            : (input.words[2] != 0 ? input.words[3] : 0xF423FU);
        return integerResult(
            state == 1U || state == 2U
                || (state == 0U && input.words[4] != 0));
    }
    case WeaponEntry::getCachedTargetPosition: {
        const bool valid = input.words[0] != 0
            && input.words[2] != 0 && input.words[1] == input.words[3];
        ProbeObservation result = integerResult(valid ? 1U : 0U, 8);
        if (valid) {
            Vector3 value{
                input.values[0], input.values[1], input.values[2]};
            if (input.words[4] != 0) {
                value.x = input.values[3] * value.x + input.values[6];
                value.y = input.values[4] * value.y + input.values[7];
                value.z = input.values[5] * value.z;
            }
            result.writes = {
                floatBits(value.x), floatBits(value.y),
                floatBits(value.z), 0};
            result.writeMask = 7;
        }
        return result;
    }
    case WeaponEntry::coolDownDeadline:
        return integerResult(
            deadline + (input.words[4] + input.words[5] + 1U) / 2U);
    case WeaponEntry::enterCoolDownState: {
        ProbeObservation result = integerResult(input.words[7]);
        result.writes = {now, input.words[1], 3U, 0};
        result.writeMask = 7;
        return result;
    }
    case WeaponEntry::coolDownUpdate:
        return integerResult(input.words[0] > 0 ? 0xFFFFFFFEU : 0U);
    case WeaponEntry::reloadUpdate: {
        ProbeObservation result = integerResult(0xFFFFFFFFU);
        result.writes[0] = input.words[2] == 0 ? 0x7FFFFFFFU : input.words[2];
        result.writeMask = 1;
        return result;
    }
    case WeaponEntry::enterReloadState: {
        ProbeObservation result = integerResult(
            (input.words[0] & 0x4000U) != 0
                ? input.words[7]
                : ((input.words[0] >> 14U) & 0xFFFFFF01U),
            4);
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
    RA3_WEAPON_NAME(zeroAimValue);
    RA3_WEAPON_NAME(firstWeaponAndSlot);
    RA3_WEAPON_NAME(weaponAtSlot);
    RA3_WEAPON_NAME(currentWeaponInactive);
    RA3_WEAPON_NAME(rebuildWeaponMasks);
    RA3_WEAPON_NAME(longestRangeWeaponPosition);
    RA3_WEAPON_NAME(matchingAntiMaskCount);
    RA3_WEAPON_NAME(findWeaponTemplate);
    RA3_WEAPON_NAME(effectiveMinimumRange);
    RA3_WEAPON_NAME(stateDeadline);
    RA3_WEAPON_NAME(chooseStateDuration);
    RA3_WEAPON_NAME(updateReadyFraction);
    RA3_WEAPON_NAME(weaponSpeedMultiplier);
    RA3_WEAPON_NAME(attackRangeForTarget);
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
