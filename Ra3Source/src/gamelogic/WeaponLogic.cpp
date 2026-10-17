#include <ra3/gamelogic/WeaponLogic.h>

#include <algorithm>
#include <cmath>
#include <cstring>
#include <limits>

namespace ra3::gamelogic {
namespace {

constexpr ModuleMetadata kMetadata[] = {
    {"AssistedTargetingUpdate", 0x0069A250u, 0x0069A260u, 0x00C01CE0u, 0x00DF8F2Cu, 0x00DF8F28u},
    {"AttachUpdate", 0x0069A3C0u, 0x0069A3D0u, 0x00C01EB4u, 0x00DF8F34u, 0x00DF8F30u},
    {"BoredUpdate", 0x0069AA10u, 0x0069AA20u, 0x00C021B8u, 0x00DF8F3Cu, 0x00DF8F38u},
    {"ChargeWeaponUpdate", 0x0069AB60u, 0x0069AB70u, 0x00C021C4u, 0x00DF8F4Cu, 0x00DF8F48u},
    {"DeletionUpdate", 0x0069AE80u, 0x0069AE90u, 0x00C02580u, 0x00DF8F54u, 0x00DF8F50u},
    {"DemoTrapUpdate", 0x0069AF60u, 0x0069AF70u, 0x00C02728u, 0x00DF8F5Cu, 0x00DF8F58u},
    {"DestroyObjectsUpdate", 0x0069B040u, 0x0069B050u, 0x00C028D0u, 0x00DF8F64u, 0x00DF8F60u},
    {"DestructibleEnvironmentUpdate", 0x0069B0C0u, 0x0069B0D0u, 0x00C02A80u, 0x00DF8F6Cu, 0x00DF8F68u},
    {"DieOnProducerDeathUpdate", 0x0069B2E0u, 0x0069B2F0u, 0x00C02C38u, 0x00DF8F74u, 0x00DF8F70u},
    {"EMPUpdate", 0x0069B480u, 0x0069B490u, 0x00C02E60u, 0x00DF8F7Cu, 0x00DF8F78u},
    {"FireSpreadUpdate", 0x0069B560u, 0x0069B570u, 0x00C03018u, 0x00DF8F8Cu, 0x00DF8F88u},
    {"FlammableUpdate", 0x0069B630u, 0x0069B640u, 0x00C02E6Cu, 0x00DF8F94u, 0x00DF8F90u},
    {"FreefallUpdate", 0x0069B830u, 0x0069B840u, 0x00C03368u, 0x00DF8F9Cu, 0x00DF8F98u},
    {"GatherSlavesUpdate", 0x0069B9E0u, 0x0069B9F0u, 0x00C03510u, 0x00DF8FACu, 0x00DF8FA8u},
    {"GrapplingHookUpdate", 0x0069BAF0u, 0x0069BB00u, 0x00C03524u, 0x00DF8FBCu, 0x00DF8FB8u},
    {"HeightDieUpdate", 0x0069BC80u, 0x0069BC90u, 0x00C03868u, 0x00DF8FC4u, 0x00DF8FC0u},
    {"HijackerUpdate", 0x0069BDC0u, 0x0069BDD0u, 0x00C03A10u, 0x00DF8FCCu, 0x00DF8FC8u},
    {"HordeNotifyTargetsOfImminentProbableCrushingUpdate", 0x0069BEE0u, 0x0069BEF0u, 0x00C03BB8u, 0x00DF8FD4u, 0x00DF8FD0u},
    {"InitiateRepairDie", 0x0069BF90u, 0x0069BFA0u, 0x00C03D90u, 0x00DF8FDCu, 0x00DF8FD8u},
    {"InvisibilityBroadcastUpdate", 0x0069C170u, 0x0069C180u, 0x00C03F48u, 0x00DF8FE4u, 0x00DF8FE0u},
    {"InvisibilityUpdate", 0x0069C4B0u, 0x0069C4C0u, 0x00C04108u, 0x00DF8FECu, 0x00DF8FE8u},
    {"LargeGroupBonusUpdate", 0x0069C7F0u, 0x0069C800u, 0x00C04330u, 0x00DF8FF4u, 0x00DF8FF0u},
    {"LaserScatterUpdate", 0x0069CA10u, 0x0069CA20u, 0x00BF8988u, 0x00DF8FFCu, 0x00DF8FF8u},
    {"LoseHealthUpdate", 0x0069D1D0u, 0x0069D1E0u, 0x00C04768u, 0x00DF9004u, 0x00DF9000u},
    {"PowerUpdate", 0x0069E2D0u, 0x0069E2E0u, 0x00BF8A94u, 0x00DF903Cu, 0x00DF9038u},
    {"ProneUpdate", 0x0069EA50u, 0x0069EA60u, 0x00C053B8u, 0x00DF9044u, 0x00DF9040u},
    {"RadarUpdate", 0x0069EB30u, 0x0069EB40u, 0x00C05560u, 0x00DF904Cu, 0x00DF9048u},
    {"RepairUpdate", 0x0069ECE0u, 0x0069ECF0u, 0x00C05708u, 0x00DF9054u, 0x00DF9050u},
    {"SetLocomotorSpeedUpdate", 0x0069F270u, 0x0069F280u, 0x00C05AE0u, 0x00DF9064u, 0x00DF9060u},
    {"SlavedUpdate", 0x0069F840u, 0x0069F850u, 0x00C05E88u, 0x00DF9074u, 0x00DF9070u},
};

template <typename T>
T load(const void* base, std::size_t offset) noexcept
{
    T result{};
    std::memcpy(&result, static_cast<const std::uint8_t*>(base) + offset, sizeof(result));
    return result;
}

template <typename T>
void store(void* base, std::size_t offset, const T& value) noexcept
{
    std::memcpy(static_cast<std::uint8_t*>(base) + offset, &value, sizeof(value));
}

float roundedAdd(float left, float right) noexcept
{
    volatile float result = left + right;
    return result;
}

float roundedSubtract(float left, float right) noexcept
{
    volatile float result = left - right;
    return result;
}

float roundedMultiply(float left, float right) noexcept
{
    volatile float result = left * right;
    return result;
}

float roundedDivide(float left, float right) noexcept
{
    volatile float result = left / right;
    return result;
}

std::int32_t x87RoundProduct(float first, float second) noexcept
{
#if defined(_MSC_VER) && defined(_M_IX86)
    std::int32_t result;
    std::uint16_t oldControl;
    std::uint16_t truncateControl;
    __asm {
        fnstcw oldControl
        mov ax, oldControl
        or ax, 0C00h
        mov truncateControl, ax
        fld first
        fmul second
        fldcw truncateControl
        fistp result
        fldcw oldControl
    }
    return result;
#else
    return static_cast<std::int32_t>(std::trunc(
        static_cast<double>(first) * static_cast<double>(second)));
#endif
}

std::int32_t x87RoundProduct(float first, float second, float third) noexcept
{
#if defined(_MSC_VER) && defined(_M_IX86)
    std::int32_t result;
    std::uint16_t oldControl;
    std::uint16_t truncateControl;
    __asm {
        fnstcw oldControl
        mov ax, oldControl
        or ax, 0C00h
        mov truncateControl, ax
        fld first
        fmul second
        fmul third
        fldcw truncateControl
        fistp result
        fldcw oldControl
    }
    return result;
#else
    return static_cast<std::int32_t>(std::trunc(
        static_cast<double>(first) * static_cast<double>(second)
        * static_cast<double>(third)));
#endif
}

float x87RoundTrip(float value) noexcept
{
#if defined(_MSC_VER) && defined(_M_IX86)
    float result;
    __asm {
        fld value
        fstp result
    }
    return result;
#else
    return value;
#endif
}

float x87SquareRoot(float value) noexcept
{
#if defined(_MSC_VER) && defined(_M_IX86)
    float result;
    __asm {
        fld value
        fsqrt
        fstp result
    }
    return result;
#else
    return std::sqrt(value);
#endif
}

std::uint32_t floatBits(float value) noexcept
{
    std::uint32_t result = 0;
    std::memcpy(&result, &value, sizeof(result));
    return result;
}

float transformCoordinate(
    const Matrix3x4& transform, std::size_t row,
    float x, float y, float z) noexcept
{
    const std::size_t base = row * 4;
    float result = row == 0
        ? roundedMultiply(transform.value[base + 1], y)
        : roundedMultiply(transform.value[base], x);
    result = roundedAdd(result, row == 0
        ? roundedMultiply(transform.value[base + 2], z)
        : roundedMultiply(transform.value[base + 1], y));
    result = roundedAdd(result, row == 0
        ? roundedMultiply(transform.value[base], x)
        : roundedMultiply(transform.value[base + 2], z));
    return roundedAdd(result, transform.value[base + 3]);
}

}  // namespace

const ModuleMetadata* moduleMetadata(std::size_t& count) noexcept
{
    count = sizeof(kMetadata) / sizeof(kMetadata[0]);
    return kMetadata;
}

const ModuleMetadata* findModuleMetadata(std::uint32_t functionAddress) noexcept
{
    for (const auto& metadata : kMetadata) {
        if (metadata.nameFunction == functionAddress
            || metadata.keyFunction == functionAddress) {
            return &metadata;
        }
    }
    return nullptr;
}

std::uint32_t moduleNameAddress(const ModuleMetadata& metadata) noexcept
{
    return metadata.nameAddress;
}

KeyResolution resolveModuleKey(
    const ModuleMetadata&, ModuleKeyState& state,
    std::uint32_t resolverResult) noexcept
{
    // ra3_1.0.game metadata-key functions: a bit-0 local-static guard surrounds
    // the shared 0x00986150 name resolver and the four-byte cache write.
    if ((state.guard & 1u) == 0u) {
        state.guard |= 1u;
        state.cachedKey = resolverResult;
        return {resolverResult, true};
    }
    return {state.cachedKey, false};
}

// ra3_1.0.game 0x0069A610; direct callers 0x00741720 and 0x00752D30
// establish ownership by the weapon-target selection path.
std::uint32_t rejectDifferentControllingPlayer(
    std::uint32_t templateObjectValue, std::uint32_t templateFlags,
    std::uint32_t sourceObjectValue, std::uint32_t sourcePlayer,
    std::uint32_t candidatePlayer) noexcept
{
    if (((templateFlags >> 2u) & 1u) != 0u) {
        if (candidatePlayer != sourcePlayer) {
            return (sourceObjectValue & 0xFFFFFF00u) | 1u;
        }
        return sourceObjectValue & 0xFFFFFF00u;
    }
    return templateObjectValue & 0xFFFFFF00u;
}

// ra3_1.0.game 0x0069A640; direct callers are the same two weapon-target paths.
std::uint32_t weaponKindMaskMatches(
    const std::uint32_t sourceExclusions[7], const std::uint32_t sourceKinds[7],
    const std::uint32_t candidateRequirements[7],
    const std::uint32_t candidateKinds[7]) noexcept
{
    bool hasExclusions = false;
    for (std::size_t index = 0; index != 7; ++index) {
        hasExclusions = hasExclusions || sourceExclusions[index] != 0;
    }
    if (hasExclusions) {
        for (std::size_t index = 0; index != 7; ++index) {
            if ((sourceExclusions[index] & sourceKinds[index]) != 0) {
                return 0;
            }
        }
    }
    for (std::size_t index = 0; index != 7; ++index) {
        if ((candidateRequirements[index] & candidateKinds[index]) != 0) {
            return 1;
        }
    }
    return 0;
}

// ra3_1.0.game 0x0069A710, called by 0x007C9730 in the weapon logic.
std::uint32_t weaponUsesLineOfSight(std::uint32_t templateFlags) noexcept
{
    return (templateFlags >> 17u) & 1u;
}

// ra3_1.0.game 0x0069ABA0, ChargeWeaponUpdate progress. The class name is
// confirmed by its 0x0069AB60/0x0069AB70 metadata pair and assertion string.
float chargeWeaponProgress(
    std::uint32_t currentFrame, std::uint32_t chargeStartFrame,
    float chargeFrames) noexcept
{
    volatile float elapsed = static_cast<float>(currentFrame - chargeStartFrame);
    volatile float denominator = chargeFrames <= 1.0f ? 1.0f : chargeFrames;
    volatile float progress = elapsed / denominator;
    if (progress < 0.0f) {
        return 0.0f;
    }
    return progress > 1.0f ? 1.0f : progress;
}

// ra3_1.0.game 0x0069AC90. This is the ClusterBombUpdate target exclusion
// predicate reached by the class update path anchored at 0x0069AD10.
std::uint32_t clusterBombAcceptsTarget(
    std::uint32_t targetFlags, const std::uint32_t* usedIds,
    std::size_t usedCount, std::uint32_t usedIdsAddress,
    std::uint32_t targetId, bool hasPosition, bool positionAllowed) noexcept
{
    if (((targetFlags >> 7u) & 1u) != 0u) {
        return (targetFlags >> 7u) & 0xFFFFFF00u;
    }
    for (std::size_t index = 0; index != usedCount; ++index) {
        if (usedIds[index] == targetId) {
            return (usedIdsAddress + static_cast<std::uint32_t>(index * 4u))
                & 0xFFFFFF00u;
        }
    }
    if (hasPosition) {
        return positionAllowed ? 1u : 0u;
    }
    return ((usedIdsAddress + static_cast<std::uint32_t>(usedCount * 4u))
            & 0xFFFFFF00u) | 1u;
}

// ra3_1.0.game 0x0069B100. Vtable 0x00C028E8 owns this transform callback;
// the class metadata pair names DestructibleEnvironmentUpdate.
void transformDestructiblePoints(
    bool enabled, const float localPoints[6], const Matrix3x4& transform,
    float output[6]) noexcept
{
    if (!enabled) {
        return;
    }
    for (std::size_t point = 0; point != 2; ++point) {
        const float x = localPoints[point * 3];
        const float y = localPoints[point * 3 + 1];
        const float z = localPoints[point * 3 + 2];
        output[point * 3] = transformCoordinate(transform, 0, x, y, z);
        output[point * 3 + 1] = transformCoordinate(transform, 1, x, y, z);
        output[point * 3 + 2] = transformCoordinate(transform, 2, x, y, z);
    }
}

// ra3_1.0.game 0x0069B930, direct call at 0x0077E91F.
std::uint32_t freefallContainsTarget(
    const std::uint32_t* targetObjectValues, const std::uint32_t* targetIds,
    std::size_t count, std::uint32_t noMatchValue,
    std::uint32_t candidateId) noexcept
{
    for (std::size_t index = 0; index != count; ++index) {
        if (targetIds[index] == candidateId) {
            return (targetObjectValues[index] & 0xFFFFFF00u) | 1u;
        }
    }
    return noMatchValue & 0xFFFFFF00u;
}

// ra3_1.0.game 0x0069CD30. Naming and state transitions were compared with
// work/references/generals/GeneralsMD/Code/GameEngine/Source/GameLogic/Object/Update/LifetimeUpdate.cpp;
// arithmetic and layout are reconstructed from RA3.
std::uint32_t lifetimeSecondaryDeadline(
    const LifetimeConfig& config, std::uint32_t currentFrame,
    std::uint32_t finalFrame, float framesPerSecond,
    float gameSpeed) noexcept
{
    if (!config.minimumEnabled && !config.maximumEnabled) {
        return 0;
    }
    const std::uint32_t delay = static_cast<std::uint32_t>(
        x87RoundProduct(gameSpeed, framesPerSecond, config.delaySeconds));
    if (delay > finalFrame - currentFrame) {
        return 0;
    }
    std::uint32_t tick = static_cast<std::uint32_t>(
        x87RoundProduct(gameSpeed, framesPerSecond, config.tickSeconds));
    if (tick == 0) {
        tick = 1;
    }
    const std::uint32_t delayedFinal = finalFrame - delay;
    std::uint32_t ticks = (delayedFinal - currentFrame) / tick;
    if (config.hasMaximumTicks && config.maximumTicks < ticks) {
        ticks = config.maximumTicks;
    }
    std::uint32_t deadline = delayedFinal - ticks * tick;
    if (deadline == currentFrame) {
        if (ticks == 0) {
            return 0;
        }
        deadline = delayedFinal - (ticks - 1u) * tick;
    }
    return deadline;
}

// ra3_1.0.game entry 0x0069CE80, native continuation 0x0069CE8A,
// LifetimeUpdate range scheduling.
std::uint32_t lifetimeStart(
    std::uint32_t currentFrame, std::uint32_t sampledLifetime,
    std::uint32_t secondaryDeadline) noexcept
{
    if (sampledLifetime == 0) {
        sampledLifetime = 1;
    }
    const std::uint32_t finalFrame = currentFrame + sampledLifetime;
    std::uint32_t delay = finalFrame - currentFrame;
    if (secondaryDeadline != 0) {
        const std::uint32_t secondaryDelay = secondaryDeadline - currentFrame;
        if (secondaryDelay < delay) {
            delay = secondaryDelay;
        }
    }
    return delay;
}

// ra3_1.0.game 0x0069CEF0, LifetimeUpdate pause.
std::uint32_t lifetimePause(std::uint8_t& paused, std::uint32_t currentFrame) noexcept
{
    paused = 1;
    return currentFrame + 1u;
}

// ra3_1.0.game 0x0069CF20, LifetimeUpdate resume.
std::uint32_t lifetimeResume(
    std::uint8_t& paused, std::uint32_t currentFrame,
    std::uint32_t finalFrame, std::uint32_t secondaryDeadline) noexcept
{
    std::uint32_t remaining = finalFrame - currentFrame;
    if (secondaryDeadline != 0) {
        const std::uint32_t secondaryRemaining = secondaryDeadline - currentFrame;
        if (secondaryRemaining < remaining) {
            remaining = secondaryRemaining;
        }
    }
    paused = 0;
    return currentFrame + remaining;
}

// ra3_1.0.game 0x0069D0F0. Name follows the neighboring
// LoseHealthUpdate metadata; arithmetic is reconstructed from RA3.
std::uint32_t loseHealthPulseCount(
    float accumulatedHealth, float healthPerPulse,
    std::uint32_t requestedPulses) noexcept
{
    if (!(accumulatedHealth > 0.0f) || !(healthPerPulse > 0.0f)) {
        return requestedPulses;
    }
    const auto calculated = static_cast<std::uint32_t>(
        std::ceil(static_cast<double>(accumulatedHealth / healthPerPulse)));
    return std::min(calculated, requestedPulses);
}

// ra3_1.0.game 0x0069E0D0/0x0069E120, PhysicsUpdate collision-mask callbacks.
// Structural names follow
// work/references/generals/GeneralsMD/Code/GameEngine/Source/GameLogic/Object/Update/PhysicsUpdate.cpp.
void physicsSetCollisionFlag(
    std::uint8_t& flag, std::uint32_t objectMask,
    std::uint32_t acceptedMask) noexcept
{
    if ((objectMask & ~acceptedMask) != 0) {
        flag = 1;
    }
}

void physicsClearCollisionFlag(
    std::uint8_t& flag, std::uint32_t objectMask,
    std::uint32_t acceptedMask) noexcept
{
    if ((objectMask & ~acceptedMask) == 0) {
        flag = 0;
    }
}

// ra3_1.0.game 0x0069E190, called from object/physics paths 0x00714E30 and
// 0x00762080. The x87 round trip intentionally preserves RA3's SNaN quieting.
void physicsCaptureTransforms(
    const Matrix3x4& objectTransform, const Matrix3x4& contactTransform,
    Matrix3x4& savedObjectTransform, Matrix3x4& savedContactTransform,
    std::uint8_t& valid) noexcept
{
    for (std::size_t index = 0; index != 12; ++index) {
        savedObjectTransform.value[index] = x87RoundTrip(objectTransform.value[index]);
        savedContactTransform.value[index] = x87RoundTrip(contactTransform.value[index]);
    }
    valid = 1;
}

// ra3_1.0.game 0x0069E3C0, direct calls at 0x007312BC and 0x00763EAC.
void physicsScaleForGroup(
    std::int32_t count, std::int32_t sourceValue, float& inverseMass,
    float& scaledFrames) noexcept
{
    if (count < 1) {
        count = 1;
    }
    inverseMass = roundedDivide(100.0f, static_cast<float>(count));
    scaledFrames = roundedMultiply(
        roundedDivide(static_cast<float>(sourceValue), static_cast<float>(count)),
        30.0f);
}

// ra3_1.0.game 0x0069E400.
std::uint32_t objectStatusBit9(std::uint32_t status) noexcept
{
    return (status >> 9u) & 1u;
}

// ra3_1.0.game 0x0069E420, vtable 0x00C19788 slot 6.
void physicsCopyControllingPlayerId(void* physics) noexcept
{
    const auto object = load<const void*>(physics, 0x08);
    const auto controller = load<const void*>(object, 0x418);
    store(physics, 0x12C, load<std::uint32_t>(controller, 0x20));
}

// ra3_1.0.game 0x0069E820, PhysicsUpdate vtable 0x00C19580 slot 23.
std::int32_t physicsCountOverlaps(
    const PhysicsOverlapNode* head, std::uint32_t ownerValue) noexcept
{
    std::int32_t count = 0;
    for (auto* node = head; node != nullptr; node = node->next) {
        if (node->ignored == 0 && node->ownerValue == ownerValue) {
            ++count;
        }
    }
    return count;
}

// ra3_1.0.game 0x0069E850, called by the PhysicsUpdate setup path 0x00763BC0.
std::uint32_t physicsHasControllerAtHeight(
    std::uint32_t objectValue, bool hasObject, bool hasController,
    std::uint32_t heightObjectValue, float height) noexcept
{
    if (!hasObject || !hasController) {
        return objectValue & 0xFFFFFF00u;
    }
    return (heightObjectValue & 0xFFFFFF00u)
        | (100.0f <= height ? 1u : 0u);
}

// ra3_1.0.game 0x0069E8E0, PowerUpdate wake scheduling.
std::uint32_t powerUpdateWakeFrame(bool powered, std::uint32_t currentFrame) noexcept
{
    return currentFrame + (powered ? 1u : 0x3FFFFFFFu);
}

// ra3_1.0.game 0x0069CA50, LaserScatterUpdate initialization.
std::uint32_t laserScatterInitialize(
    float delaySeconds, float framesPerSecond, float gameSpeed,
    std::uint32_t currentFrame, std::uint8_t& active,
    std::uint8_t& finished) noexcept
{
    active = 1;
    finished = 0;
    std::uint32_t delay = static_cast<std::uint32_t>(
        x87RoundProduct(gameSpeed, framesPerSecond, delaySeconds));
    if (delay == 0) {
        delay = 1;
    }
    return currentFrame + delay;
}

// ra3_1.0.game 0x0069F450. Direct callers 0x007178D0/0x00717D50 and the
// ShieldBodyUpdate metadata pair establish the owner. Arithmetic order matches
// the shipped SSE/x87 body.
std::uint32_t shieldSegmentIntersection(
    const Sphere& sphere, const float segmentStart[3],
    const float segmentEnd[3], float intersection[3],
    std::uint32_t segmentStartAddress,
    std::uint32_t intersectionAddress) noexcept
{
    const float fromX = roundedSubtract(segmentStart[0], sphere.center[0]);
    const float fromY = roundedSubtract(segmentStart[1], sphere.center[1]);
    const float fromZ = roundedSubtract(segmentStart[2], sphere.center[2]);
    const float directionX = roundedSubtract(segmentEnd[0], segmentStart[0]);
    const float directionY = roundedSubtract(segmentEnd[1], segmentStart[1]);
    const float directionZ = roundedSubtract(segmentEnd[2], segmentStart[2]);
    float directionLength = roundedMultiply(directionZ, directionZ);
    directionLength = roundedAdd(directionLength, roundedMultiply(directionY, directionY));
    directionLength = roundedAdd(directionLength, roundedMultiply(directionX, directionX));
    constexpr float epsilon = 9.999999747378752e-05f;
    if (directionLength >= epsilon) {
        float projection = roundedMultiply(directionZ, fromZ);
        projection = roundedAdd(projection, roundedMultiply(directionX, fromX));
        projection = roundedAdd(projection, roundedMultiply(directionY, fromY));
        projection = roundedMultiply(projection, 2.0f);
        float originLength = roundedMultiply(fromZ, fromZ);
        originLength = roundedAdd(originLength, roundedMultiply(fromX, fromX));
        originLength = roundedAdd(originLength, roundedMultiply(fromY, fromY));
        const float radiusSquared = roundedMultiply(sphere.radius, sphere.radius);
        float discriminant = roundedSubtract(originLength, radiusSquared);
        discriminant = roundedMultiply(discriminant, directionLength);
        discriminant = roundedMultiply(discriminant, 4.0f);
        discriminant = roundedSubtract(roundedMultiply(projection, projection), discriminant);
        if (discriminant >= 0.0f) {
            const float root = x87SquareRoot(discriminant);
            const float reciprocal = roundedDivide(1.0f, roundedMultiply(directionLength, 2.0f));
            const float first = roundedMultiply(roundedSubtract(root, projection), reciprocal);
            float selected = first;
            if (first <= epsilon || first > 1.0f) {
                selected = roundedMultiply(
                    roundedSubtract(roundedSubtract(-0.0f, projection), root), reciprocal);
                if (selected <= epsilon || selected > 1.0f) {
                    return segmentStartAddress & 0xFFFFFF00u;
                }
            } else {
                const float second = roundedMultiply(
                    roundedSubtract(roundedSubtract(-0.0f, projection), root), reciprocal);
                if (first < second || second < 0.0f) {
                    selected = first;
                } else {
                    selected = second;
                }
            }
            intersection[0] = roundedAdd(
                segmentStart[0], roundedMultiply(directionX, selected));
            intersection[1] = roundedAdd(
                segmentStart[1], roundedMultiply(directionY, selected));
            intersection[2] = roundedAdd(
                segmentStart[2], roundedMultiply(directionZ, selected));
            return (intersectionAddress & 0xFFFFFF00u) | 1u;
        }
    } else {
        const float endX = roundedSubtract(segmentEnd[0], sphere.center[0]);
        const float endY = roundedSubtract(segmentEnd[1], sphere.center[1]);
        const float endZ = roundedSubtract(segmentEnd[2], sphere.center[2]);
        float distance = roundedMultiply(endZ, endZ);
        distance = roundedAdd(distance, roundedMultiply(endY, endY));
        distance = roundedAdd(distance, roundedMultiply(endX, endX));
        if (distance <= roundedMultiply(sphere.radius, sphere.radius)) {
            std::memmove(intersection, segmentStart, sizeof(float) * 3);
            return (floatBits(segmentStart[2]) & 0xFFFFFF00u) | 1u;
        }
    }
    return segmentStartAddress & 0xFFFFFF00u;
}

// ra3_1.0.game 0x0069F760, object-vtable 0x00C05CB4 slot 4.
void clearObjectSlaveLinks(void* object) noexcept
{
    const std::uint32_t zero = 0;
    store(object, 0x12F4, zero);
    store(object, 0x34C, zero);
}

// ra3_1.0.game 0x0069F770, direct caller 0x006E9B80 in SlavedUpdate support.
// The concept name follows
// work/references/generals/GeneralsMD/Code/GameEngine/Source/GameLogic/Object/Update/SlavedUpdate.cpp.
void slavedPinnedPosition(
    const float masterPosition[3], const float* configuredOffset,
    float output[3]) noexcept
{
    std::memmove(output, masterPosition, sizeof(float) * 3);
    if (configuredOffset != nullptr) {
        const float offset[3] = {
            configuredOffset[0], configuredOffset[1], configuredOffset[2]};
        output[0] = roundedAdd(output[0], offset[0]);
        output[1] = roundedAdd(output[1], offset[1]);
        output[2] = roundedAdd(output[2], offset[2]);
    }
}

// ra3_1.0.game 0x0069F8D0, direct caller 0x00718620.
std::uint32_t slavedPointOutsideGuardArea(
    const float* points, std::size_t pointCount, const float position[3],
    float radius, std::uint32_t pointsAddress) noexcept
{
    const float radiusSquared = roundedMultiply(radius, radius);
    for (std::size_t index = 0; index != pointCount; ++index) {
        const float* point = points + index * 3;
        const float dx = roundedSubtract(point[0], position[0]);
        const float dy = roundedSubtract(point[1], position[1]);
        const float dz = roundedSubtract(point[2], position[2]);
        float distance = roundedMultiply(dz, dz);
        distance = roundedAdd(distance, roundedMultiply(dy, dy));
        distance = roundedAdd(distance, roundedMultiply(dx, dx));
        if (distance < radiusSquared) {
            return (pointsAddress + static_cast<std::uint32_t>(index * 12u))
                & 0xFFFFFF00u;
        }
    }
    return ((pointsAddress + static_cast<std::uint32_t>(pointCount * 12u))
            & 0xFFFFFF00u) | 1u;
}

// ra3_1.0.game 0x0069F9E0, directly called by SlavedUpdate 0x0069FA90.
std::uint32_t slavedStateAllowed(
    std::uint32_t configuration, std::uint32_t statusA,
    std::uint32_t statusB, std::uint32_t statusC) noexcept
{
    const std::uint8_t bit0 = static_cast<std::uint8_t>(configuration) & 1u;
    if (bit0 != 0u && ((statusC >> 3u) & 1u) == 0u) {
        return (statusC >> 11u) << 8u;
    }
    const std::uint32_t shifted = configuration >> 1u;
    std::uint32_t residue = (shifted & 0xFFFFFF00u) | (shifted & 1u);
    if (((configuration >> 1u) & 1u) != 0u
        && ((statusA >> 28u) & 1u) == 0u) {
        return residue & 0xFFFFFF00u;
    }
    if (((configuration >> 2u) & 1u) != 0u
        && ((statusB >> 15u) & 1u) == 0u) {
        return residue & 0xFFFFFF00u;
    }
    if (bit0 != 0u && ((statusC >> 3u) & 1u) != 0u) {
        return residue & 0xFFFFFF00u;
    }
    if (((configuration >> 1u) & 1u) != 0u) {
        residue = statusA >> 28u;
        if ((residue & 1u) != 0u) {
            return residue & 0xFFFFFF00u;
        }
    }
    if (((configuration >> 2u) & 1u) != 0u
        && ((statusB >> 15u) & 1u) != 0u) {
        return residue & 0xFFFFFF00u;
    }
    return (residue & 0xFFFFFF00u) | 1u;
}

// ra3_1.0.game 0x0069FC50, direct caller 0x005252B0.
std::uint32_t slavedConfigurationBit22(std::uint32_t configuration) noexcept
{
    return (configuration >> 22u) & 1u;
}

// ra3_1.0.game 0x0069FC60, direct caller 0x007AA550.
std::uint32_t slavedHasRider(
    std::uint32_t objectValue, bool hasRiderModule,
    std::uint32_t configurationObjectValue, std::uint32_t configuration,
    std::uint8_t configuredValue) noexcept
{
    if (hasRiderModule && ((configuration >> 19u) & 1u) == 0u) {
        return (configurationObjectValue & 0xFFFFFF00u) | configuredValue;
    }
    if (hasRiderModule) {
        return configurationObjectValue & 0xFFFFFF00u;
    }
    return objectValue & 0xFFFFFF00u;
}

// ra3_1.0.game 0x0069FD30, SlavedUpdate template-field copier.
void slavedCopyTemplateFlags(std::uint32_t templateFlags, std::uint32_t& output) noexcept
{
    output = templateFlags;
}

}  // namespace ra3::gamelogic
