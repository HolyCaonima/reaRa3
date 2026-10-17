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

struct DurationRange {
    float lowSeconds;
    float highSeconds;
};

struct WeaponTemplateData {
    float attackRange;
    float minimumAttackRange;
    float requestAssistRange;
    float approachFactor;
    float pitchTolerance;
    float minimumPitch;
    float maximumPitch;
    float idleAfterFiringSeconds;
    float holdAfterFiringSeconds;
    float suspendFxSeconds;
    DurationRange preAttackDelay;
    DurationRange firingDuration;
    DurationRange coolDownDelay;
    DurationRange clipReloadTime;
    std::uint32_t maxShots;
    bool pitchLimited;
    bool autoReloads;
};

struct WeaponRuntimeData {
    std::uint32_t vtable;
    const WeaponTemplateData* weaponTemplate;
    std::uint32_t owner;
    std::uint32_t stateMachine;
    std::uint32_t slot;
    std::uint32_t target;
    std::uint32_t targetObjectId;
    std::uint32_t stateFlags;
    std::uint32_t nextReadyFrame;
    std::uint32_t lastShotFrame;
    std::uint32_t sentinelFrame;
    std::uint32_t maxShots;
    std::uint8_t pitchLimited;
    std::uint8_t reserved31[3];
    std::uint32_t targetKind;
    std::uint32_t reloadCount;
    std::uint32_t shotCount;
    float aimPitch;
    float minimumAimPitch;
    float maximumAimPitch;
    Vector3 cachedTargetPosition;
    std::uint32_t cachedTargetObjectId;
    std::int32_t ammo;
    std::uint8_t firing;
    std::uint8_t disabled;
    std::uint8_t hasCachedTarget;
    std::uint8_t transformCachedTarget;
    std::uint32_t userData;
    std::uint32_t state;
    Vector3 muzzlePosition;
};
static_assert(sizeof(WeaponRuntimeData) == 0x78);

struct WeaponStateMachineData {
    std::array<std::uint32_t, 9> prefix;
    std::uint32_t stateStartFrame;
    std::uint32_t stateDurationFrames;
};

enum class WeaponStateType : std::uint32_t {
    init = 0,
    ready = 1,
    preFire = 2,
    fire = 3,
    coolDown = 4,
    outOfAmmo = 5,
    reload = 6,
    invalid = 0xF423F,
};

int secondsToFrames(float seconds, float logicRate, float timeScale) noexcept;
int rangeEndpointToFrames(
    const DurationRange* range, bool high, float logicRate,
    float timeScale) noexcept;
float weaponAttackRange(
    const WeaponTemplateData& data, float pitchRadians) noexcept;
float weaponMinimumAttackRange(const WeaponTemplateData& data) noexcept;
bool isStateType(const WeaponRuntimeData& weapon, WeaponStateType type) noexcept;
bool isFiring(const WeaponRuntimeData& weapon) noexcept;
bool cachedTargetPosition(
    const WeaponRuntimeData& weapon, std::uint32_t targetObjectId,
    const float* transform3x4, Vector3& output) noexcept;
void initializeWeaponRuntime(
    WeaponRuntimeData& weapon, const WeaponTemplateData* weaponTemplate,
    std::uint32_t slot, std::uint32_t now,
    std::uint32_t suspendFxFrames) noexcept;

enum class WeaponEntry : std::uint32_t {
    zeroAimValue = 0x00685DC0,
    firstWeaponAndSlot = 0x006A2400,
    weaponAtSlot = 0x006A2430,
    currentWeaponInactive = 0x006A2440,
    rebuildWeaponMasks = 0x006A24A0,
    longestRangeWeaponPosition = 0x006A24F0,
    matchingAntiMaskCount = 0x006A2560,
    findWeaponTemplate = 0x006A25F0,
    effectiveMinimumRange = 0x006A2630,
    stateDeadline = 0x006A2840,
    chooseStateDuration = 0x006A2AB0,
    updateReadyFraction = 0x006A2BD0,
    weaponSpeedMultiplier = 0x006A2D30,
    attackRangeForTarget = 0x006A2DC0,
    targetEffectValue = 0x006A3120,
    initializeWeapon = 0x006A82F0,
    targetWithinMinimumRange = 0x006A8530,
    readinessFraction = 0x006A8580,
    acceptsObjectFilter = 0x006A8650,
    isPreFireState = 0x006A86C0,
    targetWithinPitch = 0x006A8700,
    suppressesAttack = 0x006A8840,
    canFireNow = 0x006A8870,
    getCachedTargetPosition = 0x006A88F0,
    coolDownDeadline = 0x006BD260,
    enterCoolDownState = 0x006BD2C0,
    coolDownUpdate = 0x006BD310,
    reloadUpdate = 0x006ED440,
    enterReloadState = 0x006ED4A0,
};

struct ProbeInput {
    std::array<std::uint32_t, 8> words{};
    std::array<float, 8> values{};
};

struct ProbeObservation {
    std::uint32_t eax{};
    std::uint32_t edx{};
    std::uint32_t stackCleanup{};
    std::uint32_t st0Bits{};
    std::array<std::uint32_t, 4> writes{};
    std::uint32_t writeMask{};
};

ProbeObservation evaluateWeaponEntry(
    WeaponEntry entry, const ProbeInput& input) noexcept;
const char* weaponEntryName(WeaponEntry entry) noexcept;

}  // namespace ra3::gamelogic
