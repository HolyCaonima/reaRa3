#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

namespace ra3::gamelogic {

struct Vector3 {
    float x;
    float y;
    float z;
};

struct TimedCondition {
    std::uint32_t condition;
    std::uint32_t expirationFrame;
};

struct ContainedObject {
    std::uint32_t id;
    float x;
    float y;
    std::uint32_t relation;
    std::uint8_t slotKind;
    std::uint8_t valid;
};

struct GarrisonStatusInput {
    std::array<std::uint32_t, 9> healthy;
    std::array<std::uint32_t, 9> damaged;
    std::array<std::uint32_t, 9> reallyDamaged;
    bool healthyMatches;
    bool damagedMatches;
    bool reallyDamagedMatches;
};

enum class GarrisonDestination : std::uint32_t {
    unchanged,
    object,
    position,
    randomPoint
};

struct DisabledStatusResult {
    bool active;
    bool pending;
    bool statusSet;
    bool dirty;
    std::uint32_t sleepFrames;
};

struct FireWeaponStateInput {
    std::array<std::uint32_t, 12> weaponState;
    Vector3 position;
    bool specialStatus;
    bool statusAlreadySet;
};

struct FireWeaponState {
    std::array<std::uint32_t, 12> weaponState;
    Vector3 origin;
    std::uint32_t phase;
    std::uint32_t reserved;
    bool statusSet;
    bool dirty;
};

struct PhysicsRuntime {
    std::array<std::uint32_t, 28> words;
};
static_assert(sizeof(PhysicsRuntime) == 0x70);

struct PhysicsContact {
    std::uint32_t id;
    std::uint32_t next;
    std::uint32_t previous;
    bool disabled;
};

struct SpawnCrateState {
    std::array<std::uint32_t, 15> words;
};

struct PhysicsStatusResult {
    std::uint32_t statusA8;
    std::uint32_t statusB8;
    bool conditionWasSet;
};

// These implementations are original RA3 reconstructions. Their structure and
// naming were cross-checked against the exact Generals references listed in
// docs/design/gamelogic_behavior.md; layouts and arithmetic come from
// ra3_1.0.game, not from the reference implementation.

std::int32_t findOpenGarrisonPoint(
    std::int32_t start, const std::uint32_t* occupantIds,
    std::int32_t pointCount) noexcept;                                                // 0x006B1540
GarrisonDestination chooseGarrisonDestination(
    bool hasTargetObject, bool hasTargetPosition,
    std::int32_t currentPoint) noexcept;                                              // 0x006B1670
std::uint32_t garrisonReassignmentActions(std::int32_t firstPoint,
                                           std::int32_t secondPoint) noexcept;         // 0x006B16E0
bool preferGarrisonPoint(
    std::int32_t currentPoint, std::int32_t candidatePoint,
    float currentDistance, float candidateDistance) noexcept;                        // 0x006B1900
bool copyExitWaypointIfAvailable(
    bool hasObject, bool hasTeam, std::uint32_t relation,
    std::uint32_t waypoint, std::uint32_t& output) noexcept;                          // 0x006B1E90
std::uint32_t countMatchingOccupants(
    bool useLiveList, const std::uint8_t* matches, std::size_t count,
    std::uint32_t cachedCount) noexcept;                                              // 0x006B1F00
bool canTransferContainedObject(
    std::uint32_t relation, bool allowNeutral, bool allowAllied,
    bool samePlayer, bool allowSamePlayerEnemy,
    bool allowOtherEnemy) noexcept;                                                   // 0x006B20E0
bool containsObjectId(
    const std::uint32_t* ids, std::size_t count, std::uint32_t id) noexcept;           // 0x006B21C0
bool hasOpenRiderSlotZero(
    bool baseCheck, const std::uint8_t* slotKinds,
    std::size_t count) noexcept;                                                      // 0x006B2220
bool hasOpenRiderSlotOne(
    bool baseCheck, const std::uint8_t* slotKinds,
    std::size_t count) noexcept;                                                      // 0x006B2260
std::int32_t nearestContainedObject2D(
    const ContainedObject* objects, std::size_t count,
    float x, float y) noexcept;                                                       // 0x006B2320
std::int32_t consumeScaledSpawnCount(
    std::int32_t configuredCount, float& oneShotScale) noexcept;                      // 0x006B23B0
std::uint32_t garrisonDamageStatusToSet(
    const GarrisonStatusInput& input) noexcept;                                       // 0x006B2690
std::uint32_t clearGarrisonDamageStatusBits(std::uint32_t status) noexcept;            // 0x006B27B0

std::uint32_t remainingConditionFrames(
    const TimedCondition* conditions, std::size_t count,
    std::uint32_t condition, std::uint32_t currentFrame) noexcept;                    // 0x006B3120
std::uint32_t nextConditionWakeFrame(
    const TimedCondition* conditions, std::size_t count,
    std::uint32_t currentFrame) noexcept;                                             // 0x006B3160
Vector3 scaleBalloonVelocity(Vector3 direction, float scale) noexcept;                 // 0x006B3270
std::uint32_t specialAbilityDamageBucket(float ratio) noexcept;                       // 0x006B3660
std::uint32_t specialAbilityActivationState(std::uint32_t flags) noexcept;             // 0x006B3980
std::uint32_t specialAbilityUnpackState(std::uint32_t flags) noexcept;                 // 0x006B3DC0
DisabledStatusResult updateDisabledStatus(
    bool active, bool pending, bool statusSet,
    float currentSpeed, float disableAboveSpeed) noexcept;                            // 0x006B3820
Vector3 approachPosition(
    const Vector3& from, const Vector3& to,
    float distance, float adjustedZ) noexcept;                                        // 0x006B39A0
bool crossedTriggerRadius(
    const Vector3& owner, const Vector3& target, float radius,
    bool enabled, bool alreadyTriggered) noexcept;                                   // 0x006B3F10
float minimumSpecialProgress(
    const float* childProgress, std::size_t count,
    float baseProgress) noexcept;                                                     // 0x006B3FC0
void broadcastSpecialProgress(
    float* childProgress, std::size_t count,
    float& baseProgress, float value) noexcept;                                       // 0x006B4070
void resetSpecialProgress(
    float* childProgress, std::size_t count,
    float& baseProgress) noexcept;                                                    // 0x006B40F0
bool withinAnySpecialPosition(
    const Vector3* positions, std::size_t count,
    const Vector3& target, float radius) noexcept;                                    // 0x006B44E0
bool requiredConditionSet(
    std::uint32_t requiredMask, std::uint32_t objectMask) noexcept;                   // 0x006B4720
bool requiredConditionSetUpdate(
    std::uint32_t requiredMask, std::uint32_t objectMask) noexcept;                   // 0x006B48D0
bool specialPowerWithinRange(
    bool hasTarget, float maximumRange,
    const Vector3& owner, const Vector3& target,
    bool baseResult) noexcept;                                                        // 0x006B4A40
bool weaponFireTargetAllowed(
    std::uint32_t objectStatus, bool baseResult) noexcept;                            // 0x006B4D40
void applyWeaponFireRadius(
    Vector3& target, float angle, float radius,
    bool hasWeapon, float weaponRadius) noexcept;                                     // 0x006B4EB0
std::uint32_t clearDockObjectStatus(std::uint32_t status) noexcept;                   // 0x006B51C0
Vector3 selectSourceDockPosition(
    const Vector3& configured, const Vector3& source,
    const Vector3& docker, bool useSourceWhenAircraft,
    bool dockerIsAircraft, const Vector3& transformed) noexcept;                      // 0x006B5AF0
Vector3 selectTargetDockPosition(
    const Vector3& configured, const Vector3& docker,
    const Vector3& transformed) noexcept;                                             // 0x006B5BA0
Vector3 selectApproachDockPosition(
    const Vector3& configured, const Vector3& docker,
    const Vector3& transformed) noexcept;                                             // 0x006B5C20
void markDockPairOccupied(
    std::uint32_t& sourceStatus, std::uint32_t& dockerStatus) noexcept;                // 0x006B5CA0
void setDockEnabled(std::uint8_t& field, bool enabled) noexcept;                       // 0x006B5AE0

void setSpecialDestination(
    Vector3& destination, std::uint32_t& modeFlags,
    std::uint32_t& targetObject, const Vector3& value) noexcept;                      // 0x006B6EC0
bool canSelectAlternateTarget(
    bool alternateEnabled, bool hasContainer,
    bool firstPassengerEligible, bool fallbackPassengerEligible) noexcept;            // 0x006B7340
float bombardRadius(
    float configuredRadius, bool hasModifier,
    float modifier) noexcept;                                                         // 0x006B7850
FireWeaponState initializeFireWeaponState(
    const FireWeaponStateInput& input) noexcept;                                      // 0x006B7B00
void invalidTargetsDeathFlags(std::uint32_t& flags) noexcept;                         // 0x006B7D70
void invalidTargetsDeathDefaultFlags(std::uint32_t& flags) noexcept;                  // 0x006B7D80
void linearDamageFlags(std::uint32_t& flags) noexcept;                                // 0x006B8480
void physicsBehaviorFlags(std::uint32_t& flags) noexcept;                             // 0x006B8C70
Vector3 physicsAnglesToVector(
    float pitch, float roll, float yaw) noexcept;                                     // 0x006B8CF0
void initializePhysicsRuntime(
    PhysicsRuntime& state, std::uint32_t owner,
    std::uint32_t moduleData, std::uint32_t currentFrame) noexcept;                   // 0x006B8EC0
bool hasMotiveTarget(bool hasTarget, std::uint32_t targetStatus) noexcept;              // 0x006B91E0
void appendPhysicsContact(
    PhysicsContact*& first, PhysicsContact*& last,
    PhysicsContact& contact, std::uint32_t& count) noexcept;                          // 0x006B9510
std::uint32_t countMatchingContacts(
    const PhysicsContact* first, const std::uint32_t* ids,
    std::size_t idCount) noexcept;                                                    // 0x006B9580
bool enablePhysicsContact(
    std::array<std::uint32_t, 5>& startFrames,
    std::array<std::uint8_t, 5>& enabled,
    std::size_t index, bool setting, std::uint32_t currentFrame) noexcept;             // 0x006B95D0
bool expireDelayedWeapon(
    std::uint32_t& expirationFrame, std::uint8_t& state,
    std::uint32_t currentFrame) noexcept;                                             // 0x006B96C0
bool removePhysicsOverlap(
    std::uint32_t id, std::uint32_t* currentIds, std::uint32_t* currentValues,
    std::uint32_t& currentCount, std::uint32_t* previousIds,
    std::uint32_t* previousValues, std::uint32_t& previousCount) noexcept;             // 0x006B99C0
bool needsAirborneStatus(float threshold, std::uint32_t objectStatus) noexcept;         // 0x006B9B70
PhysicsStatusResult clearPhysicsStatusesAndSet(
    std::uint32_t statusA8, std::uint32_t statusB8,
    std::uint32_t condition) noexcept;                                                // 0x006B9BB0
void initializeSpawnCrateState(
    SpawnCrateState& state, std::uint32_t owner, std::uint32_t moduleData,
    std::uint32_t currentFrame, float delayFrames) noexcept;                          // 0x006BA5D0

}  // namespace ra3::gamelogic
