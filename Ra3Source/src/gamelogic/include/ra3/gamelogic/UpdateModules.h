#pragma once

#include <cstddef>
#include <cstdint>

namespace ra3::gamelogic {

struct ModuleMetadata {
    const char* name;
    std::uint32_t nameFunction;
    std::uint32_t keyFunction;
    std::uint32_t nameAddress;
    std::uint32_t guardAddress;
    std::uint32_t cacheAddress;
};

struct ModuleKeyState {
    std::uint32_t guard;
    std::uint32_t cachedKey;
};

struct KeyResolution {
    std::uint32_t value;
    bool resolverCalled;
};

const ModuleMetadata* moduleMetadata(std::size_t& count) noexcept;
const ModuleMetadata* findModuleMetadata(std::uint32_t functionAddress) noexcept;
std::uint32_t moduleNameAddress(const ModuleMetadata& metadata) noexcept;
KeyResolution resolveModuleKey(
    const ModuleMetadata& metadata, ModuleKeyState& state,
    std::uint32_t resolverResult) noexcept;

struct Matrix3x4 {
    float value[12];
};

struct Sphere {
    float center[3];
    float radius;
};

struct PhysicsOverlapNode {
    std::uint8_t reserved00[0x0C];
    std::uint32_t kind;
    std::uint8_t reserved10[0x28];
    std::uint32_t ownerValue;
    std::uint8_t reserved3C[0x18];
    PhysicsOverlapNode* next;
    std::uint8_t reserved58[0x0C];
    std::uint8_t active;
    std::uint8_t ignored;
};

// Shared weapon target filters. Names follow the corresponding concepts in
// work/references/generals/GeneralsMD/Code/GameEngine/Source/GameLogic/Object/Weapon.cpp;
// the layouts and implementations are independently reconstructed from RA3.
std::uint32_t rejectDifferentControllingPlayer(
    std::uint32_t templateObjectValue, std::uint32_t templateFlags,
    std::uint32_t sourceObjectValue, std::uint32_t sourcePlayer,
    std::uint32_t candidatePlayer) noexcept;
std::uint32_t weaponKindMaskMatches(
    const std::uint32_t sourceExclusions[7], const std::uint32_t sourceKinds[7],
    const std::uint32_t candidateRequirements[7],
    const std::uint32_t candidateKinds[7]) noexcept;
std::uint32_t weaponUsesLineOfSight(std::uint32_t templateFlags) noexcept;
float chargeWeaponProgress(
    std::uint32_t currentFrame, std::uint32_t chargeStartFrame,
    float chargeFrames) noexcept;
std::uint32_t clusterBombAcceptsTarget(
    std::uint32_t targetFlags, const std::uint32_t* usedIds,
    std::size_t usedCount, std::uint32_t usedIdsAddress,
    std::uint32_t targetId, bool hasPosition, bool positionAllowed) noexcept;

void transformDestructiblePoints(
    bool enabled, const float localPoints[6], const Matrix3x4& transform,
    float output[6]) noexcept;
std::uint32_t freefallContainsTarget(
    const std::uint32_t* targetObjectValues, const std::uint32_t* targetIds,
    std::size_t count, std::uint32_t noMatchValue,
    std::uint32_t candidateId) noexcept;

struct LifetimeConfig {
    bool minimumEnabled;
    bool maximumEnabled;
    float delaySeconds;
    float tickSeconds;
    bool hasMaximumTicks;
    std::uint32_t maximumTicks;
};

std::uint32_t lifetimeSecondaryDeadline(
    const LifetimeConfig& config, std::uint32_t currentFrame,
    std::uint32_t finalFrame, float framesPerSecond,
    float gameSpeed) noexcept;
std::uint32_t lifetimeStart(
    std::uint32_t currentFrame, std::uint32_t sampledLifetime,
    std::uint32_t secondaryDeadline) noexcept;
std::uint32_t lifetimePause(std::uint8_t& paused, std::uint32_t currentFrame) noexcept;
std::uint32_t lifetimeResume(
    std::uint8_t& paused, std::uint32_t currentFrame,
    std::uint32_t finalFrame, std::uint32_t secondaryDeadline) noexcept;
std::uint32_t loseHealthPulseCount(
    float accumulatedHealth, float healthPerPulse,
    std::uint32_t requestedPulses) noexcept;

void physicsSetCollisionFlag(
    std::uint8_t& flag, std::uint32_t objectMask,
    std::uint32_t acceptedMask) noexcept;
void physicsClearCollisionFlag(
    std::uint8_t& flag, std::uint32_t objectMask,
    std::uint32_t acceptedMask) noexcept;
void physicsCaptureTransforms(
    const Matrix3x4& objectTransform, const Matrix3x4& contactTransform,
    Matrix3x4& savedObjectTransform, Matrix3x4& savedContactTransform,
    std::uint8_t& valid) noexcept;
void physicsScaleForGroup(
    std::int32_t count, std::int32_t sourceValue, float& inverseMass,
    float& scaledFrames) noexcept;
std::uint32_t objectStatusBit9(std::uint32_t status) noexcept;
void physicsCopyControllingPlayerId(void* physics) noexcept;
std::int32_t physicsCountOverlaps(
    const PhysicsOverlapNode* head, std::uint32_t ownerValue) noexcept;
std::uint32_t physicsHasControllerAtHeight(
    std::uint32_t objectValue, bool hasObject, bool hasController,
    std::uint32_t heightObjectValue, float height) noexcept;
std::uint32_t powerUpdateWakeFrame(bool powered, std::uint32_t currentFrame) noexcept;
std::uint32_t laserScatterInitialize(
    float delaySeconds, float framesPerSecond, float gameSpeed,
    std::uint32_t currentFrame, std::uint8_t& active,
    std::uint8_t& finished) noexcept;

std::uint32_t shieldSegmentIntersection(
    const Sphere& sphere, const float segmentStart[3],
    const float segmentEnd[3], float intersection[3],
    std::uint32_t segmentStartAddress,
    std::uint32_t intersectionAddress) noexcept;

void clearObjectSlaveLinks(void* object) noexcept;
void slavedPinnedPosition(
    const float masterPosition[3], const float* configuredOffset,
    float output[3]) noexcept;
std::uint32_t slavedPointOutsideGuardArea(
    const float* points, std::size_t pointCount, const float position[3],
    float radius, std::uint32_t pointsAddress) noexcept;
std::uint32_t slavedStateAllowed(
    std::uint32_t configuration, std::uint32_t statusA,
    std::uint32_t statusB, std::uint32_t statusC) noexcept;
std::uint32_t slavedConfigurationBit22(std::uint32_t configuration) noexcept;
std::uint32_t slavedHasRider(
    std::uint32_t objectValue, bool hasRiderModule,
    std::uint32_t configurationObjectValue, std::uint32_t configuration,
    std::uint8_t configuredValue) noexcept;
void slavedCopyTemplateFlags(std::uint32_t templateFlags, std::uint32_t& output) noexcept;

}  // namespace ra3::gamelogic
