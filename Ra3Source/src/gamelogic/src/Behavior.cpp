#include <ra3/gamelogic/Behavior.h>

#include <algorithm>
#include <cmath>
#include <cstring>
#include <limits>

namespace ra3::gamelogic {
namespace {

constexpr std::uint32_t kSleepForever = 0x3FFFFFFFU;
constexpr std::uint32_t kGarrisonDamageMask = 0x0001C000U;
constexpr float kHalfPi = 1.57079637050628662109375F;

float squaredDistance2D(
    float firstX, float firstY, float secondX, float secondY) noexcept
{
    const float x = firstX - secondX;
    const float y = firstY - secondY;
    return x * x + y * y;
}

float squaredDistance(const Vector3& first, const Vector3& second) noexcept
{
    const float x = first.x - second.x;
    const float y = first.y - second.y;
    const float z = first.z - second.z;
    return x * x + y * y + z * z;
}

bool isZero(const Vector3& value) noexcept
{
    return value.x == 0.0F && value.y == 0.0F && value.z == 0.0F;
}

Vector3 selectDockPosition(
    const Vector3& configured, const Vector3& fallback,
    const Vector3& transformed) noexcept
{
    return isZero(configured) ? fallback : transformed;
}

bool removeOverlapFromArray(
    std::uint32_t id, std::uint32_t* ids, std::uint32_t* values,
    std::uint32_t& count) noexcept
{
    for (std::uint32_t index = 0; index < count; ++index) {
        if (ids[index] != id) {
            continue;
        }
        --count;
        if (index != count) {
            ids[index] = ids[count];
            values[index] = values[count];
        }
        return true;
    }
    return false;
}

}  // namespace

// ra3_1.0.game 0x006B1540.
std::int32_t findOpenGarrisonPoint(
    std::int32_t start, const std::uint32_t* occupantIds,
    std::int32_t pointCount) noexcept
{
    if (pointCount <= 0) {
        return -1;
    }
    std::int32_t point = start;
    for (std::int32_t visited = 0; visited < pointCount; ++visited) {
        if (occupantIds[point] == 0) {
            return point;
        }
        point = (point + 1) % pointCount;
    }
    return -1;
}

// ra3_1.0.game 0x006B1670.
GarrisonDestination chooseGarrisonDestination(
    bool hasTargetObject, bool hasTargetPosition,
    std::int32_t currentPoint) noexcept
{
    if (hasTargetObject) {
        return GarrisonDestination::object;
    }
    if (hasTargetPosition) {
        return GarrisonDestination::position;
    }
    return currentPoint == -1
        ? GarrisonDestination::randomPoint
        : GarrisonDestination::unchanged;
}

// ra3_1.0.game 0x006B16E0. Bit 0 removes the old station; bit 1 assigns a new one.
std::uint32_t garrisonReassignmentActions(
    std::int32_t firstPoint, std::int32_t secondPoint) noexcept
{
    std::uint32_t actions = firstPoint != -1 ? 1U : 0U;
    if (secondPoint == -1) {
        actions |= 2U;
    }
    return actions;
}

// ra3_1.0.game 0x006B1900.
bool preferGarrisonPoint(
    std::int32_t currentPoint, std::int32_t candidatePoint,
    float currentDistance, float candidateDistance) noexcept
{
    return candidatePoint != -1
        && candidatePoint != currentPoint
        && candidateDistance < currentDistance;
}

// ra3_1.0.game 0x006B1E90.
bool copyExitWaypointIfAvailable(
    bool hasObject, bool hasTeam, std::uint32_t relation,
    std::uint32_t waypoint, std::uint32_t& output) noexcept
{
    if (!hasObject || !hasTeam || relation != 0) {
        return false;
    }
    output = waypoint;
    return true;
}

// ra3_1.0.game 0x006B1F00.
std::uint32_t countMatchingOccupants(
    bool useLiveList, const std::uint8_t* matches, std::size_t count,
    std::uint32_t cachedCount) noexcept
{
    if (!useLiveList) {
        return cachedCount;
    }
    std::uint32_t result = 0;
    for (std::size_t index = 0; index < count; ++index) {
        result += matches[index] != 0 ? 1U : 0U;
    }
    return result;
}

// ra3_1.0.game 0x006B20E0.
bool canTransferContainedObject(
    std::uint32_t relation, bool allowNeutral, bool allowAllied,
    bool samePlayer, bool allowSamePlayerEnemy,
    bool allowOtherEnemy) noexcept
{
    if (relation == 0) {
        return allowNeutral;
    }
    if (relation == 1) {
        return allowAllied;
    }
    if (relation != 2) {
        return false;
    }
    return samePlayer ? allowSamePlayerEnemy : allowOtherEnemy;
}

// ra3_1.0.game 0x006B21C0.
bool containsObjectId(
    const std::uint32_t* ids, std::size_t count, std::uint32_t id) noexcept
{
    for (std::size_t index = 0; index < count; ++index) {
        if (ids[index] == id) {
            return true;
        }
    }
    return false;
}

bool hasOpenRiderSlotKind(
    bool baseCheck, const std::uint8_t* slotKinds, std::size_t count,
    std::uint8_t wantedKind) noexcept
{
    if (!baseCheck) {
        return false;
    }
    for (std::size_t index = 0; index < count; ++index) {
        if (slotKinds[index] == wantedKind) {
            return true;
        }
    }
    return false;
}

// ra3_1.0.game 0x006B2220.
bool hasOpenRiderSlotZero(
    bool baseCheck, const std::uint8_t* slotKinds, std::size_t count) noexcept
{
    return hasOpenRiderSlotKind(baseCheck, slotKinds, count, 0);
}

// ra3_1.0.game 0x006B2260.
bool hasOpenRiderSlotOne(
    bool baseCheck, const std::uint8_t* slotKinds, std::size_t count) noexcept
{
    return hasOpenRiderSlotKind(baseCheck, slotKinds, count, 1);
}

// ra3_1.0.game 0x006B2320.
std::int32_t nearestContainedObject2D(
    const ContainedObject* objects, std::size_t count,
    float x, float y) noexcept
{
    float bestDistance = std::numeric_limits<float>::max();
    std::int32_t best = -1;
    for (std::size_t index = 0; index < count; ++index) {
        if (objects[index].valid == 0) {
            continue;
        }
        const float distance =
            squaredDistance2D(x, y, objects[index].x, objects[index].y);
        if (best == -1 || distance < bestDistance) {
            best = static_cast<std::int32_t>(index);
            bestDistance = distance;
        }
    }
    return best;
}

// ra3_1.0.game 0x006B23B0.
std::int32_t consumeScaledSpawnCount(
    std::int32_t configuredCount, float& oneShotScale) noexcept
{
    if (oneShotScale == 1.0F) {
        return configuredCount;
    }
    const std::int32_t result =
        static_cast<std::int32_t>(static_cast<float>(configuredCount) * oneShotScale);
    oneShotScale = 1.0F;
    return result;
}

// ra3_1.0.game 0x006B2690.
std::uint32_t garrisonDamageStatusToSet(
    const GarrisonStatusInput& input) noexcept
{
    const auto hasEntry = [](const std::array<std::uint32_t, 9>& values) {
        return std::any_of(values.begin(), values.end(),
                           [](std::uint32_t value) { return value != 0; });
    };
    if (hasEntry(input.healthy) && input.healthyMatches) {
        return 0x00004000U;
    }
    if (hasEntry(input.damaged) && input.damagedMatches) {
        return 0x00008000U;
    }
    if (hasEntry(input.reallyDamaged) && input.reallyDamagedMatches) {
        return 0x00010000U;
    }
    return 0;
}

// ra3_1.0.game 0x006B27B0.
std::uint32_t clearGarrisonDamageStatusBits(std::uint32_t status) noexcept
{
    return status & ~kGarrisonDamageMask;
}

// ra3_1.0.game 0x006B3120.
std::uint32_t remainingConditionFrames(
    const TimedCondition* conditions, std::size_t count,
    std::uint32_t condition, std::uint32_t currentFrame) noexcept
{
    if (condition >= 456) {
        return 0;
    }
    for (std::size_t index = 0; index < count; ++index) {
        if (conditions[index].condition != condition) {
            continue;
        }
        return conditions[index].expirationFrame <= currentFrame
            ? 0U
            : conditions[index].expirationFrame - currentFrame;
    }
    return 0;
}

// ra3_1.0.game 0x006B3160.
std::uint32_t nextConditionWakeFrame(
    const TimedCondition* conditions, std::size_t count,
    std::uint32_t currentFrame) noexcept
{
    if (count == 0) {
        return kSleepForever;
    }
    std::uint32_t earliest = 99999999U;
    for (std::size_t index = 0; index < count; ++index) {
        earliest = std::min(earliest, conditions[index].expirationFrame);
    }
    const std::int32_t remaining =
        static_cast<std::int32_t>(earliest - currentFrame);
    return remaining < 1 ? 1U : static_cast<std::uint32_t>(remaining);
}

// ra3_1.0.game 0x006B3270.
Vector3 scaleBalloonVelocity(Vector3 direction, float scale) noexcept
{
    direction.x *= scale;
    direction.y *= scale;
    direction.z *= scale;
    return direction;
}

// ra3_1.0.game 0x006B3660. The return value identifies the five damage bands
// selected by the original before it constructs the corresponding event mask.
std::uint32_t specialAbilityDamageBucket(float ratio) noexcept
{
    if (ratio >= 1.0F) {
        return 4;
    }
    if (ratio > 0.75F) {
        return 3;
    }
    if (ratio > 0.5F) {
        return 2;
    }
    if (ratio > 0.25F) {
        return 1;
    }
    return 0;
}

// ra3_1.0.game 0x006B3980.
std::uint32_t specialAbilityActivationState(std::uint32_t flags) noexcept
{
    return (flags & 2U) == 0 ? 0x000C1A21U : 0x000C1A20U;
}

// ra3_1.0.game 0x006B3DC0.
std::uint32_t specialAbilityUnpackState(std::uint32_t flags) noexcept
{
    return (flags & 2U) == 0 ? 0x000C1A21U : 0x000C1A20U;
}

// ra3_1.0.game 0x006B3820.
DisabledStatusResult updateDisabledStatus(
    bool active, bool pending, bool statusSet,
    float currentSpeed, float disableAboveSpeed) noexcept
{
    DisabledStatusResult result{active, pending, statusSet, false, 1};
    if (!active && !pending) {
        result.sleepFrames = kSleepForever;
        return result;
    }
    if (currentSpeed <= 0.0F || currentSpeed <= disableAboveSpeed) {
        if (pending && !active) {
            result.pending = false;
            result.active = true;
            if (!result.statusSet) {
                result.statusSet = true;
                result.dirty = true;
            }
        }
        return result;
    }
    if (active) {
        result.active = false;
        if (result.statusSet) {
            result.statusSet = false;
            result.dirty = true;
        }
        result.sleepFrames = kSleepForever;
    }
    return result;
}

// ra3_1.0.game 0x006B39A0.
Vector3 approachPosition(
    const Vector3& from, const Vector3& to,
    float distance, float adjustedZ) noexcept
{
    const float x = to.x - from.x;
    const float y = to.y - from.y;
    const float z = to.z - from.z;
    const float lengthSquared = x * x + y * y + z * z;
    const float inverseLength =
        lengthSquared == 0.0F ? 0.0F : 1.0F / std::sqrt(lengthSquared);
    return {
        from.x + distance * x * inverseLength,
        from.y + distance * y * inverseLength,
        adjustedZ};
}

// ra3_1.0.game 0x006B3F10.
bool crossedTriggerRadius(
    const Vector3& owner, const Vector3& target, float radius,
    bool enabled, bool alreadyTriggered) noexcept
{
    return enabled && !alreadyTriggered
        && squaredDistance(owner, target) <= radius * radius;
}

// ra3_1.0.game 0x006B3FC0.
float minimumSpecialProgress(
    const float* childProgress, std::size_t count,
    float baseProgress) noexcept
{
    float result = 1.0F;
    for (std::size_t index = 0; index < count; ++index) {
        result = std::min(result, childProgress[index]);
    }
    return std::min(result, baseProgress);
}

// ra3_1.0.game 0x006B4070.
void broadcastSpecialProgress(
    float* childProgress, std::size_t count,
    float& baseProgress, float value) noexcept
{
    for (std::size_t index = 0; index < count; ++index) {
        childProgress[index] = value;
    }
    baseProgress = value;
}

// ra3_1.0.game 0x006B40F0.
void resetSpecialProgress(
    float* childProgress, std::size_t count,
    float& baseProgress) noexcept
{
    for (std::size_t index = 0; index < count; ++index) {
        childProgress[index] = 0.0F;
    }
    baseProgress = 0.0F;
}

// ra3_1.0.game 0x006B44E0.
bool withinAnySpecialPosition(
    const Vector3* positions, std::size_t count,
    const Vector3& target, float radius) noexcept
{
    const float radiusSquared = radius * radius;
    for (std::size_t index = 0; index < count; ++index) {
        if (squaredDistance(positions[index], target) <= radiusSquared) {
            return true;
        }
    }
    return false;
}

// ra3_1.0.game 0x006B4720.
bool requiredConditionSet(
    std::uint32_t requiredMask, std::uint32_t objectMask) noexcept
{
    return requiredMask == 0 || (requiredMask & objectMask) != 0;
}

// ra3_1.0.game 0x006B48D0.
bool requiredConditionSetUpdate(
    std::uint32_t requiredMask, std::uint32_t objectMask) noexcept
{
    return requiredMask != 0 && (requiredMask & objectMask) != 0;
}

// ra3_1.0.game 0x006B4A40.
bool specialPowerWithinRange(
    bool hasTarget, float maximumRange,
    const Vector3& owner, const Vector3& target,
    bool baseResult) noexcept
{
    if (hasTarget && maximumRange > 0.0F
        && maximumRange * maximumRange < squaredDistance(owner, target)) {
        return false;
    }
    return baseResult;
}

// ra3_1.0.game 0x006B4D40.
bool weaponFireTargetAllowed(
    std::uint32_t objectStatus, bool baseResult) noexcept
{
    constexpr std::uint32_t forbidden = 0x00000280U;
    return (objectStatus & forbidden) == 0 && baseResult;
}

// ra3_1.0.game 0x006B4EB0.
void applyWeaponFireRadius(
    Vector3& target, float angle, float radius,
    bool hasWeapon, float weaponRadius) noexcept
{
    if (!hasWeapon || weaponRadius <= 0.0001F) {
        return;
    }
    target.x += std::cos(angle) * radius;
    target.y += std::sin(angle) * radius;
}

// ra3_1.0.game 0x006B51C0.
std::uint32_t clearDockObjectStatus(std::uint32_t status) noexcept
{
    return status & ~0x00000800U;
}

// ra3_1.0.game 0x006B5AF0.
Vector3 selectSourceDockPosition(
    const Vector3& configured, const Vector3& source,
    const Vector3& docker, bool useSourceWhenAircraft,
    bool dockerIsAircraft, const Vector3& transformed) noexcept
{
    if (!isZero(configured)) {
        return transformed;
    }
    return useSourceWhenAircraft && dockerIsAircraft ? source : docker;
}

// ra3_1.0.game 0x006B5BA0.
Vector3 selectTargetDockPosition(
    const Vector3& configured, const Vector3& docker,
    const Vector3& transformed) noexcept
{
    return selectDockPosition(configured, docker, transformed);
}

// ra3_1.0.game 0x006B5C20.
Vector3 selectApproachDockPosition(
    const Vector3& configured, const Vector3& docker,
    const Vector3& transformed) noexcept
{
    return selectDockPosition(configured, docker, transformed);
}

// ra3_1.0.game 0x006B5CA0.
void markDockPairOccupied(
    std::uint32_t& sourceStatus, std::uint32_t& dockerStatus) noexcept
{
    const auto mark = [](std::uint32_t status) {
        if ((status & 0x00400000U) != 0 || (status & 0x00800000U) == 0) {
            status &= ~0x00400000U;
            status |= 0x00800000U;
        }
        return status;
    };
    sourceStatus = mark(sourceStatus);
    dockerStatus = mark(dockerStatus);
}

// ra3_1.0.game 0x006B5AE0.
void setDockEnabled(std::uint8_t& field, bool enabled) noexcept
{
    field = enabled ? 1U : 0U;
}

// ra3_1.0.game 0x006B6EC0.
void setSpecialDestination(
    Vector3& destination, std::uint32_t& modeFlags,
    std::uint32_t& targetObject, const Vector3& value) noexcept
{
    destination = value;
    modeFlags = (modeFlags & 0xFFFFDFFFU) | 4U;
    targetObject = 0;
}

// ra3_1.0.game 0x006B7340.
bool canSelectAlternateTarget(
    bool alternateEnabled, bool hasContainer,
    bool firstPassengerEligible, bool fallbackPassengerEligible) noexcept
{
    if (!alternateEnabled || !hasContainer) {
        return false;
    }
    return firstPassengerEligible || fallbackPassengerEligible;
}

// ra3_1.0.game 0x006B7850.
float bombardRadius(
    float configuredRadius, bool hasModifier,
    float modifier) noexcept
{
    float result = configuredRadius;
    if (hasModifier) {
        result *= modifier;
    }
    return result < 0.0F ? 0.0F : result;
}

// ra3_1.0.game 0x006B7B00.
FireWeaponState initializeFireWeaponState(
    const FireWeaponStateInput& input) noexcept
{
    FireWeaponState result{};
    result.weaponState = input.weaponState;
    result.origin = input.position;
    result.phase = 1;
    result.reserved = 0;
    result.statusSet = input.specialStatus && !input.statusAlreadySet;
    result.dirty = result.statusSet;
    return result;
}

// ra3_1.0.game 0x006B7D70.
void invalidTargetsDeathFlags(std::uint32_t& flags) noexcept
{
    flags = 0x42U;
}

// ra3_1.0.game 0x006B7D80.
void invalidTargetsDeathDefaultFlags(std::uint32_t& flags) noexcept
{
    flags = 0x40U;
}

// ra3_1.0.game 0x006B8480.
void linearDamageFlags(std::uint32_t& flags) noexcept
{
    flags = 0x1CU;
}

// ra3_1.0.game 0x006B8C70.
void physicsBehaviorFlags(std::uint32_t& flags) noexcept
{
    flags = 0x90U;
}

// ra3_1.0.game 0x006B8CF0.
Vector3 physicsAnglesToVector(float pitch, float roll, float yaw) noexcept
{
    const float cosPitch = std::cos(pitch);
    const float sinPitch = std::sin(pitch);
    const float cosRoll = std::cos(kHalfPi - roll);
    const float sinRoll = std::sin(kHalfPi - roll);
    const float zeroYaw = yaw * 0.0F;
    Vector3 result{
        cosRoll * (zeroYaw * sinPitch + cosPitch * yaw) - sinRoll * zeroYaw,
        (zeroYaw * cosPitch + yaw * sinPitch) * cosRoll - zeroYaw * sinRoll,
        (zeroYaw * cosPitch + zeroYaw * sinPitch) * cosRoll - yaw * sinRoll};
    if (result.x == 0.0F) {
        result.x = 0.0F;
    }
    if (result.y == 0.0F) {
        result.y = 0.0F;
    }
    if (result.z == 0.0F) {
        result.z = 0.0F;
    }
    return result;
}

// ra3_1.0.game 0x006B8EC0.
void initializePhysicsRuntime(
    PhysicsRuntime& state, std::uint32_t owner,
    std::uint32_t moduleData, std::uint32_t currentFrame) noexcept
{
    state.words[0] = 0x00C0D8B0U;
    state.words[1] = moduleData;
    state.words[2] = owner;
    state.words[3] = 0;
    state.words[4] = 0;
    state.words[5] = 0;
    state.words[6] = 1;
    for (std::size_t index = 7; index <= 13; ++index) {
        state.words[index] = 0;
    }
    state.words[14] = currentFrame;
    state.words[15] = 0;
    auto* const bytes = reinterpret_cast<std::uint8_t*>(&state);
    bytes[0x40] = 0;
    state.words[17] = 0;
    state.words[18] = 0;
    for (std::size_t offset = 0x4C; offset <= 0x50; ++offset) {
        bytes[offset] = 0;
    }
    state.words[21] = 0;
    state.words[22] = 0;
    state.words[23] = 0xFFFFFFFFU;
    state.words[24] = 0;
    bytes[0x64] = 0;
    bytes[0x65] = 0;
    state.words[26] = 0;
    state.words[27] = 0;
}

// ra3_1.0.game 0x006B91E0.
bool hasMotiveTarget(bool hasTarget, std::uint32_t targetStatus) noexcept
{
    return hasTarget && ((targetStatus >> 9U) & 1U) != 0;
}

// ra3_1.0.game 0x006B9510.
void appendPhysicsContact(
    PhysicsContact*& first, PhysicsContact*& last,
    PhysicsContact& contact, std::uint32_t& count) noexcept
{
    contact.next = 0;
    contact.previous = last == nullptr ? 0U : last->id;
    if (first == nullptr) {
        first = &contact;
    }
    last = &contact;
    ++count;
}

// ra3_1.0.game 0x006B9580.
std::uint32_t countMatchingContacts(
    const PhysicsContact* first, const std::uint32_t* ids,
    std::size_t idCount) noexcept
{
    std::uint32_t result = 0;
    for (const PhysicsContact* contact = first;
         contact != nullptr;
         contact = reinterpret_cast<const PhysicsContact*>(
             static_cast<std::uintptr_t>(contact->next))) {
        if (contact->disabled) {
            continue;
        }
        for (std::size_t index = 0; index < idCount; ++index) {
            if (ids[index] == contact->id) {
                ++result;
                break;
            }
        }
    }
    return result;
}

// ra3_1.0.game 0x006B95D0.
bool enablePhysicsContact(
    std::array<std::uint32_t, 5>& startFrames,
    std::array<std::uint8_t, 5>& enabled,
    std::size_t index, bool setting, std::uint32_t currentFrame) noexcept
{
    if (index >= enabled.size()) {
        return false;
    }
    enabled[index] = setting ? 1U : 0U;
    if (setting && startFrames[index] == 0) {
        startFrames[index] = currentFrame;
        return true;
    }
    return false;
}

// ra3_1.0.game 0x006B96C0.
bool expireDelayedWeapon(
    std::uint32_t& expirationFrame, std::uint8_t& state,
    std::uint32_t currentFrame) noexcept
{
    if (expirationFrame == 0 || state == 1 || expirationFrame >= currentFrame) {
        return false;
    }
    state = 1;
    expirationFrame = 0;
    return true;
}

// ra3_1.0.game 0x006B99C0.
bool removePhysicsOverlap(
    std::uint32_t id, std::uint32_t* currentIds, std::uint32_t* currentValues,
    std::uint32_t& currentCount, std::uint32_t* previousIds,
    std::uint32_t* previousValues, std::uint32_t& previousCount) noexcept
{
    const bool removedCurrent =
        removeOverlapFromArray(id, currentIds, currentValues, currentCount);
    const bool removedPrevious =
        removeOverlapFromArray(id, previousIds, previousValues, previousCount);
    return removedCurrent || removedPrevious;
}

// ra3_1.0.game 0x006B9B70.
bool needsAirborneStatus(float threshold, std::uint32_t objectStatus) noexcept
{
    return 0.0F < threshold && (objectStatus & 0x80U) == 0;
}

// ra3_1.0.game 0x006B9BB0.
PhysicsStatusResult clearPhysicsStatusesAndSet(
    std::uint32_t statusA8, std::uint32_t statusB8,
    std::uint32_t condition) noexcept
{
    statusB8 &= ~0x00000280U;
    statusA8 &= ~0xC6018000U;
    return {statusA8, statusB8, condition != 0};
}

// ra3_1.0.game 0x006BA5D0.
void initializeSpawnCrateState(
    SpawnCrateState& state, std::uint32_t owner, std::uint32_t moduleData,
    std::uint32_t currentFrame, float delayFrames) noexcept
{
    state.words[0] = 0x00C0DD78U;
    state.words[1] = moduleData;
    state.words[2] = owner;
    state.words[3] = 0x00C0DC48U;
    state.words[4] = 0x00C0DC34U;
    state.words[5] = 0;
    state.words[6] = 0xFFFFFFFFU;
    state.words[7] = 0xFFFFFFFFU;
    state.words[8] = owner;
    state.words[9] = 0;
    state.words[10] = 0;
    state.words[11] = 0;
    state.words[13] = 0;
    state.words[14] = static_cast<std::uint32_t>(
        std::ceil(static_cast<float>(currentFrame) * delayFrames));
}

}  // namespace ra3::gamelogic
