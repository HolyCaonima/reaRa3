#include "ra3/gamelogic/BehaviorModules68.h"

#include <algorithm>
#include <cmath>
#include <cstring>

namespace ra3::gamelogic {
namespace {

constexpr std::uint32_t kAssistedPlacementEntryVtable = 0x00BFA120u;
constexpr std::uint32_t kSleepForeverOffset = 0x3FFFFFFFu;
constexpr std::uint32_t kInvalidBoneSentinel = 0x7FFFFFFFu;

float bitsToFloat(std::uint32_t bits) noexcept {
    float value = 0.0f;
    std::memcpy(&value, &bits, sizeof(value));
    return value;
}

std::uint32_t floatToBits(float value) noexcept {
    std::uint32_t bits = 0;
    std::memcpy(&bits, &value, sizeof(bits));
    return bits;
}

std::uint32_t quietX87FloatLoadStoreBits(std::uint32_t bits) noexcept {
    const std::uint32_t exp = bits & 0x7F800000u;
    const std::uint32_t mantissa = bits & 0x007FFFFFu;
    if (exp == 0x7F800000u && mantissa != 0u) {
        return bits | 0x00400000u;
    }
    return bits;
}

bool isOrderedEqual(float a, float b) noexcept {
    return !std::isnan(a) && !std::isnan(b) && (a == b);
}

StaticModuleFinderResult resolveStaticModuleFromObject(
    StaticModuleKeyState state,
    std::uint32_t resolverKey,
    const char* moduleNameLiteral,
    const std::uint32_t* moduleAddresses,
    const std::uint32_t* moduleKeys,
    std::size_t count) noexcept {
    StaticModuleFinderResult out{};
    out.guard = state.guard;
    out.cachedKey = state.cachedKey;
    out.resolverCalled = false;
    out.resolvedName = moduleNameLiteral;

    if ((out.guard & 1u) == 0u) {
        out.guard |= 1u;
        out.cachedKey = resolverKey;
        out.resolverCalled = true;
    }

    out.moduleAddress = findBehaviorModuleByKey(
        moduleAddresses, moduleKeys, count, out.cachedKey);
    return out;
}

std::uint32_t computeSecondsToFramesTruncated(
    float seconds,
    bool hasGameLogic,
    float gameLogicFrameScale,
    float fpsGlobal) noexcept {
    double product = static_cast<double>(fpsGlobal);
    if (hasGameLogic) {
        product *= static_cast<double>(gameLogicFrameScale);
    }
    product *= static_cast<double>(seconds);
    return static_cast<std::uint32_t>(static_cast<std::int32_t>(product));
}

template <std::size_t N>
std::uint8_t hasAnyNonZeroDword(const std::uint32_t (&arr)[N]) noexcept {
    for (std::size_t i = 0; i < N; ++i) {
        if (arr[i] != 0u) {
            return 1u;
        }
    }
    return 0u;
}

}  // namespace

// 1. 0x00687460: Object::findBehaviorModule(NameKeyType)
std::uint32_t findBehaviorModuleByKey(
    const std::uint32_t* moduleAddresses,
    const std::uint32_t* moduleKeys,
    std::size_t count,
    std::uint32_t targetKey) noexcept {
    if (moduleAddresses == nullptr || moduleKeys == nullptr) {
        return 0u;
    }
    for (std::size_t i = 0; i < count; ++i) {
        const std::uint32_t modAddr = moduleAddresses[i];
        if (modAddr == 0u) {
            break;
        }
        if (moduleKeys[i] == targetKey) {
            return modAddr;
        }
    }
    return 0u;
}

// 2. 0x00689E80: AimWeaponBehavior::updateTargetPosition
void updateAimWeaponTargetPosition(
    Coord3D68& storedTarget,
    std::int32_t& outMode,
    const Coord3D68& inputTarget) noexcept {
    storedTarget = inputTarget;
    if (isOrderedEqual(inputTarget.x, -1.0f) &&
        isOrderedEqual(inputTarget.y, -1.0f) &&
        isOrderedEqual(inputTarget.z, -1.0f)) {
        outMode = -1;
    } else {
        outMode = 1;
    }
}

// 3. 0x00689FC0: AimWeaponBehavior::remapWeaponSlotMask
void remapAimWeaponSlotMask(
    std::uint32_t (&outPair)[2],
    std::uint32_t sourceMask) noexcept {
    outPair[0] = sourceMask;

    std::uint32_t remapped = ((sourceMask >> 7u) & 1u) << 7u;
    remapped |= (sourceMask & 1u);
    remapped |= ((sourceMask >> 1u) & 1u) << 1u;
    remapped |= ((sourceMask >> 2u) & 1u) << 7u;
    remapped |= ((sourceMask >> 3u) & 1u) << 3u;
    remapped |= ((sourceMask >> 4u) & 1u) << 4u;
    remapped |= ((sourceMask >> 5u) & 1u) << 5u;
    remapped |= ((sourceMask >> 6u) & 1u) << 6u;
    remapped |= ((sourceMask >> 8u) & 1u) << 8u;
    remapped |= ((sourceMask >> 10u) & 1u) << 9u;

    outPair[1] = remapped;
}

// 4. 0x0068A080: AimWeaponBehavior::getModuleWeaponSlotMask
void getAimWeaponModuleSlotMask(
    std::uint32_t (&outPair)[2],
    std::uint32_t moduleDataMask) noexcept {
    remapAimWeaponSlotMask(outPair, moduleDataMask);
}

// 5. 0x0068A0A0: BehaviorModule (+0x0C) cast thunk to (+0x10)
std::uint32_t castBehaviorModuleToSubobjectPlus10(
    std::uint32_t behaviorSubobjectAddress) noexcept {
    if (behaviorSubobjectAddress == 0x0Cu) {
        return 0u;
    }
    return behaviorSubobjectAddress + 0x04u;
}

// 6. 0x0068A190: AssistedPlacementBehavior::getFromObject
StaticModuleFinderResult getAssistedPlacementBehaviorFromObject(
    StaticModuleKeyState state,
    std::uint32_t resolverKey,
    const std::uint32_t* moduleAddresses,
    const std::uint32_t* moduleKeys,
    std::size_t count) noexcept {
    return resolveStaticModuleFromObject(
        state,
        resolverKey,
        "AssistedPlacementBehavior",
        moduleAddresses,
        moduleKeys,
        count);
}

// 7. 0x0068A270: AssistedPlacementBehavior::initPlacementEntry
void initAssistedPlacementEntry(
    AssistedPlacementEntry68& entry,
    const std::uint32_t (&block9)[9],
    const std::uint32_t (&block15)[15],
    const std::uint32_t (&block7)[7]) noexcept {
    entry.vtable = kAssistedPlacementEntryVtable;
    entry.field04 = 0u;
    std::copy(std::begin(block9), std::end(block9), entry.block9.begin());
    std::copy(std::begin(block15), std::end(block15), entry.block15.begin());
    std::copy(std::begin(block7), std::end(block7), entry.block7.begin());
    entry.hasBlock9 = hasAnyNonZeroDword(block9);
    entry.hasBlock15 = hasAnyNonZeroDword(block15);
    entry.hasBlock7 = hasAnyNonZeroDword(block7);
}

// 8. 0x0068A320: AssistedPlacementInfoBehavior::getFromObject
StaticModuleFinderResult getAssistedPlacementInfoBehaviorFromObject(
    StaticModuleKeyState state,
    std::uint32_t resolverKey,
    const std::uint32_t* moduleAddresses,
    const std::uint32_t* moduleKeys,
    std::size_t count) noexcept {
    return resolveStaticModuleFromObject(
        state,
        resolverKey,
        "AssistedPlacementInfoBehavior",
        moduleAddresses,
        moduleKeys,
        count);
}

// 9. 0x0068A730: AutoHealBehavior::onDamage
WakeScheduleCall onAutoHealDamage(
    std::uint32_t damageSubobjectAddress,
    std::uint32_t ownerAddress,
    bool upgradeActive,
    std::uint32_t radiusBits,
    float startHealingDelaySeconds,
    std::uint32_t damageType,
    std::uint32_t soonestHealFrame,
    bool hasGameLogic,
    std::uint32_t currentFrame,
    float gameLogicFrameScale,
    float fpsGlobal) noexcept {
    WakeScheduleCall out{false, 0u, 0u, 0u};
    if (!upgradeActive || radiusBits != 0u) {
        return out;
    }

    const bool isHealing = (damageType == 2u);
    const std::uint32_t delayFrames = computeSecondsToFramesTruncated(
        startHealingDelaySeconds, hasGameLogic, gameLogicFrameScale, fpsGlobal);

    if (!isHealing && delayFrames > 0u) {
        out.called = true;
        out.objectAddress = ownerAddress;
        out.updateSubobjectAddress = damageSubobjectAddress + 0x04u;
        out.wakeFrame = currentFrame + delayFrames;
        return out;
    }

    if (currentFrame > soonestHealFrame) {
        out.called = true;
        out.objectAddress = ownerAddress;
        out.updateSubobjectAddress = damageSubobjectAddress + 0x04u;
        out.wakeFrame = currentFrame + 1u;
    }
    return out;
}

// 10. 0x0068A810: AutoHealBehavior::resetStateFields
void resetAutoHealStateFields(std::uint32_t (&fields)[9]) noexcept {
    std::fill(std::begin(fields), std::end(fields), 0u);
}

// 11. 0x0068A890: AutoHealBehavior::setFlag1000
void setAutoHealFlag1000(std::uint32_t& flagsAt40, bool enable) noexcept {
    if (enable) {
        flagsAt40 |= 0x1000u;
    } else {
        flagsAt40 &= ~0x1000u;
    }
}

// 12. 0x0068A8B0: AutoHealBehavior::setEnabledAndSchedule
WakeScheduleCall setAutoHealEnabledAndSchedule(
    std::uint32_t subobjectAddress,
    std::uint32_t ownerAddress,
    std::uint32_t currentFrame,
    bool enable,
    std::uint32_t& fieldAtPlus18) noexcept {
    WakeScheduleCall out{};
    out.called = true;
    out.objectAddress = ownerAddress;
    out.updateSubobjectAddress = subobjectAddress - 0x14u;
    if (enable) {
        out.wakeFrame = currentFrame + 1u;
    } else {
        out.wakeFrame = currentFrame + kSleepForeverOffset;
        fieldAtPlus18 = 0u;
    }
    return out;
}

// 13. 0x0068AC10: BezierProjectileBehavior::advanceAlongPath
std::uint32_t advanceBezierProjectileAlongPath(
    const Coord3D68* waypoints,
    std::size_t waypointCount,
    std::uint32_t currentSegmentIndex,
    float field7C,
    float& fieldC8,
    float& fieldCC,
    float moduleDataStepEC,
    Coord3D68& outPosition) noexcept {
    outPosition = waypoints[currentSegmentIndex];
    if (isOrderedEqual(fieldC8, field7C)) {
        return currentSegmentIndex;
    }

    const float prevC8 = fieldC8;
    fieldCC = fieldCC + prevC8;
    fieldC8 = moduleDataStepEC + prevC8;

    const std::int32_t numSegments = static_cast<std::int32_t>(waypointCount) - 1;
    if (numSegments <= 0) {
        return 0u;
    }

    const float targetDist = fieldCC;
    float accumDist = 0.0f;

    for (std::int32_t seg = 0; seg < numSegments; ++seg) {
        const Coord3D68& p0 = waypoints[seg];
        const Coord3D68& p1 = waypoints[seg + 1];

        const float dx = p1.x - p0.x;
        const float dy = p1.y - p0.y;
        const float dz = p1.z - p0.z;

        const double lenSq =
            (static_cast<double>(dz) * static_cast<double>(dz) +
             static_cast<double>(dy) * static_cast<double>(dy)) +
            static_cast<double>(dx) * static_cast<double>(dx);
        const double lenExact = std::sqrt(lenSq);
        const float segLen = static_cast<float>(lenExact);

        if (static_cast<double>(fieldC8) >= lenExact) {
            fieldC8 = field7C;
        }

        const float prevAccum = accumDist;
        accumDist = segLen + accumDist;

        if (accumDist > targetDist) {
            constexpr float kMinSegLen = 1.0e-4f;
            float t = 1.0f;
            if (segLen > kMinSegLen) {
                t = (targetDist - prevAccum) / segLen;
            }
            outPosition.x = dx * t + p0.x;
            outPosition.y = dy * t + p0.y;
            outPosition.z = dz * t + p0.z;
            return static_cast<std::uint32_t>(seg);
        }
    }

    return static_cast<std::uint32_t>(numSegments);
}

// 14. 0x0068ADF0: BezierProjectileBehavior::updateSegmentDistance
bool updateBezierProjectileSegmentDistance(
    const Coord3D68* waypoints,
    std::size_t waypointCount,
    std::int32_t currentSegmentIndex,
    std::uint32_t moduleDataFlags08,
    float moduleDataThresholdE4,
    float& accumulatedDistanceC0) noexcept {
    const std::int32_t count = static_cast<std::int32_t>(waypointCount);
    if (currentSegmentIndex >= count) {
        return false;
    }

    const Coord3D68& cur = waypoints[currentSegmentIndex];
    float dx = 0.0f;
    float dy = 0.0f;
    float dz = 0.0f;

    if (currentSegmentIndex > 0) {
        const Coord3D68& prev = waypoints[currentSegmentIndex - 1];
        dx = cur.x - prev.x;
        dy = cur.y - prev.y;
        dz = cur.z - prev.z;

        const bool hasMidpointFlag = ((moduleDataFlags08 >> 12u) & 1u) != 0u;
        const std::int32_t halfCount = static_cast<std::int32_t>(
            static_cast<std::uint32_t>(count) >> 1u);
        if (hasMidpointFlag && currentSegmentIndex == halfCount) {
            return true;
        }
    } else {
        dx = cur.x - cur.x;
        dy = cur.y - cur.y;
        dz = cur.z - cur.z;
    }

    const double segLen = std::sqrt(
        (static_cast<double>(dz) * static_cast<double>(dz) +
         static_cast<double>(dy) * static_cast<double>(dy)) +
        static_cast<double>(dx) * static_cast<double>(dx));
    const double totalDist = segLen + static_cast<double>(accumulatedDistanceC0);
    accumulatedDistanceC0 = static_cast<float>(totalDist);
    return totalDist > static_cast<double>(moduleDataThresholdE4);
}

// 15. 0x0068AEF0: BezierProjectileBehavior::getPreviousAndCurrentWaypoints
void getBezierProjectilePreviousAndCurrentWaypoints(
    const Coord3D68* waypoints,
    std::size_t waypointCount,
    std::int32_t currentIndex60,
    Coord3D68& outPrevious,
    Coord3D68& outCurrent) noexcept {
    if (waypoints == nullptr || waypointCount == 0u) {
        return;
    }

    const std::uint32_t count = static_cast<std::uint32_t>(waypointCount);
    std::int32_t clampedIdx = currentIndex60;
    if (static_cast<std::uint32_t>(clampedIdx) >= count) {
        clampedIdx = static_cast<std::int32_t>(count - 1u);
    }

    const std::int32_t prevIdx = (clampedIdx > 0) ? (clampedIdx - 1) : 0;
    outPrevious = waypoints[prevIdx];
    outCurrent = waypoints[clampedIdx];
}

// 16. 0x0068AF80: BridgeBuilderBehavior::hasNullTowerSlot
bool hasNullBridgeTowerSlot(
    const std::uint32_t* slots,
    std::size_t count) noexcept {
    if (slots == nullptr || count == 0u) {
        return false;
    }
    for (std::size_t i = 0; i < count; ++i) {
        if (slots[i] == 0u) {
            return true;
        }
    }
    return false;
}

// 17. 0x0068AFB0: BridgeBuilderBehavior::getTowerSlotByIndex
std::uint32_t getBridgeTowerSlotByIndex(
    const std::uint32_t* slots,
    std::size_t count,
    std::uint32_t index) noexcept {
    if (slots == nullptr || index >= static_cast<std::uint32_t>(count)) {
        return 0u;
    }
    return slots[index];
}

// 18. 0x0068AFD0: BridgeBuilderBehavior::computeSectionPosition
void computeBridgeSectionPosition(
    float moduleDataF10,
    float moduleDataF18,
    const float* moduleDataOptF20,
    std::int32_t count34,
    std::int32_t count38,
    const Coord3D68& targetPos28,
    const Coord3D68& ownerPos38,
    std::uint32_t mode,
    Coord3D68& outPosition) noexcept {
    float offset = 0.0f;

    switch (mode) {
        case 0u:
            offset = 0.0f;
            break;
        case 1u:
        case 3u: {
            const float endMul = (mode == 1u) ? (moduleDataF10 * 0.5f) : 0.0f;
            const float base = moduleDataF10 * 1.5f + endMul;
            offset = static_cast<float>(count34) * moduleDataF18 + base;
            if (moduleDataOptF20 != nullptr) {
                offset += static_cast<float>(count38) * (*moduleDataOptF20);
            }
            break;
        }
        case 2u:
            offset = moduleDataF10 * 0.5f;
            break;
        default: {
            const std::int32_t idx = static_cast<std::int32_t>(mode) - 4;
            const float base = moduleDataF18 * 0.5f + moduleDataF10;
            if (moduleDataOptF20 == nullptr) {
                offset = static_cast<float>(idx) * moduleDataF18 + base;
            } else {
                const std::int16_t half = static_cast<std::int16_t>(idx / 2);
                const std::int32_t rem = idx % 2;
                const float pairStep = (*moduleDataOptF20) + moduleDataF18;
                if (rem != 0) {
                    offset = (static_cast<float>(half) + 0.5f) * pairStep + base;
                } else {
                    offset = pairStep * static_cast<float>(half) + base;
                }
            }
            break;
        }
    }

    const float dx = targetPos28.x - ownerPos38.x;
    const float dy = targetPos28.y - ownerPos38.y;
    const float len2D = static_cast<float>(std::sqrt(
        static_cast<double>(dy) * static_cast<double>(dy) +
        static_cast<double>(dx) * static_cast<double>(dx)));
    const float scale = offset / len2D;

    outPosition.x = dx * scale + ownerPos38.x;
    outPosition.y = dy * scale + ownerPos38.y;
    outPosition.z = (targetPos28.z > ownerPos38.z) ? targetPos28.z : ownerPos38.z;
}

// 19. 0x0068B180: BridgeBuilderBehavior::computeSpanSegmentCounts
void computeBridgeSpanSegmentCounts(
    float moduleDataF10,
    float moduleDataF18,
    std::uint32_t moduleDataHasAlt1C,
    const float* moduleDataOptF20,
    float targetX28,
    float targetY2C,
    float ownerX38,
    float ownerY3C,
    std::uint32_t& outCount34,
    std::uint32_t& outCount38) noexcept {
    const float dx = ownerX38 - targetX28;
    const float dy = ownerY3C - targetY2C;
    const float len2D = static_cast<float>(std::sqrt(
        static_cast<double>(dy) * static_cast<double>(dy) +
        static_cast<double>(dx) * static_cast<double>(dx)));
    const float twoEnd = moduleDataF10 + moduleDataF10;
    const float spanLen = len2D - twoEnd;

    outCount34 = 0u;
    outCount38 = 0u;

    if (spanLen >= 0.0f) {
        float remaining = spanLen;
        std::uint32_t which = 0u;
        do {
            if (which == 0u) {
                ++outCount34;
                remaining -= moduleDataF18;
            } else {
                ++outCount38;
                remaining -= *moduleDataOptF20;
            }
            if (moduleDataHasAlt1C != 0u && moduleDataOptF20 != nullptr) {
                which = (which == 0u) ? 1u : 0u;
            }
        } while (remaining >= 0.0f);
    }
}

// 20. 0x0068B230: BuildingBehavior::getFromObject
StaticModuleFinderResult getBuildingBehaviorFromObject(
    StaticModuleKeyState state,
    std::uint32_t resolverKey,
    const std::uint32_t* moduleAddresses,
    const std::uint32_t* moduleKeys,
    std::size_t count) noexcept {
    return resolveStaticModuleFromObject(
        state,
        resolverKey,
        "BuildingBehavior",
        moduleAddresses,
        moduleKeys,
        count);
}

// 21. 0x0068B360: BuildingBehavior::setAlertState
WakeScheduleCall setBuildingBehaviorAlertState(
    std::uint32_t moduleAddress,
    std::uint32_t ownerAddress,
    std::uint32_t alertManagerMode,
    std::uint32_t logicFramesPerSecond,
    std::uint32_t currentFrame,
    std::uint8_t newState,
    std::uint32_t& countdown28,
    std::uint8_t& flag2D,
    std::uint8_t& state2E) noexcept {
    WakeScheduleCall out{false, 0u, 0u, 0u};
    if (alertManagerMode != 4u || state2E == newState) {
        return out;
    }

    state2E = newState;
    countdown28 = 0u;
    if (newState != 0u) {
        countdown28 = logicFramesPerSecond * 2u;
    } else {
        flag2D = 1u;
    }

    out.called = true;
    out.objectAddress = ownerAddress;
    out.updateSubobjectAddress = moduleAddress + 0x10u;
    out.wakeFrame = currentFrame + 1u;
    return out;
}

// 22. 0x0068B840: BuildingDestructionBehavior::resetBoneDamage
BoneDamageResetCall resetBuildingDestructionBoneDamage(
    bool hasSubObject,
    std::uint32_t renderObjectAddress,
    std::int32_t boneIndex,
    std::int32_t modelBoneCount) noexcept {
    BoneDamageResetCall out{false, 0u, 0, 0u};
    if (!hasSubObject || renderObjectAddress == 0u) {
        return out;
    }
    if (boneIndex < modelBoneCount) {
        out.called = true;
        out.modelAddress = renderObjectAddress + 0x74u;
        out.boneIndex = boneIndex;
        out.valueBits = 0u;
    }
    return out;
}

// 23. 0x0068B960: BuildingDestructionBehavior::findClosestBone
void findClosestBuildingDestructionBone(
    const BuildingBoneCandidate68* const* groups,
    const std::uint32_t* groupCounts,
    std::uint32_t numGroups,
    const Coord3D68& queryPoint,
    std::uint32_t& outGroupIndex,
    std::uint32_t& outBoneIndex) noexcept {
    outGroupIndex = kInvalidBoneSentinel;
    outBoneIndex = kInvalidBoneSentinel;

    float bestDistSq = bitsToFloat(0x7F7FFFFFu);

    for (std::uint32_t g = 0u; g < numGroups; ++g) {
        const std::uint32_t count = groupCounts[g];
        const BuildingBoneCandidate68* elems = groups[g];
        for (std::uint32_t b = 0u; b < count; ++b) {
            const BuildingBoneCandidate68& elem = elems[b];
            if (elem.activeMetric04 > 0.0f) {
                const float dx = queryPoint.x - elem.position0C.x;
                const float dy = queryPoint.y - elem.position0C.y;
                const float dz = queryPoint.z - elem.position0C.z;
                const float distSq = (dz * dz + dx * dx) + dy * dy;
                if (bestDistSq > distSq) {
                    outGroupIndex = g;
                    outBoneIndex = b;
                    bestDistSq = distSq;
                }
            }
        }
    }
}

// 24. 0x0068C210: EnergyDisablingBehavior::notifyOwnerPlayerEnergyDisabling
EnergyDisablingNotifyCall notifyOwnerPlayerEnergyDisabling(
    std::uint32_t ownerAddress,
    std::uint32_t playerAddress,
    std::uint32_t energyManagerAddress,
    std::uint32_t ownerObjectId) noexcept {
    EnergyDisablingNotifyCall out{false, 0u, 0u};
    if (ownerAddress != 0u && playerAddress != 0u && energyManagerAddress != 0u) {
        out.called = true;
        out.energyManagerAddress = energyManagerAddress;
        out.ownerObjectId = ownerObjectId;
    }
    return out;
}

// 25. 0x0068C520: FireWeaponWhenDamagedBehavior::copyStateBlock
void copyFireWeaponWhenDamagedState(
    FireWeaponDamagedState68& dst,
    const FireWeaponDamagedState68& src) noexcept {
    dst.field00 = src.field00;
    dst.field04 = src.field04;
    dst.field08 = src.field08;
    dst.field0C = src.field0C;
    dst.floatBits10 = quietX87FloatLoadStoreBits(src.floatBits10);
    dst.floatBits14 = quietX87FloatLoadStoreBits(src.floatBits14);
    dst.floatBits18 = quietX87FloatLoadStoreBits(src.floatBits18);
    dst.field1C = src.field1C;
    dst.field20 = src.field20;
    dst.field24 = src.field24;
    dst.field28 = src.field28;
}

// 26. 0x0068C5E0: FXListBehavior::getFromObject
StaticModuleFinderResult getFXListBehaviorFromObject(
    StaticModuleKeyState state,
    std::uint32_t resolverKey,
    const std::uint32_t* moduleAddresses,
    const std::uint32_t* moduleKeys,
    std::size_t count) noexcept {
    return resolveStaticModuleFromObject(
        state,
        resolverKey,
        "FXListBehavior",
        moduleAddresses,
        moduleKeys,
        count);
}

// 27. 0x0068C6D0: BehaviorModule (+0x0C) cast thunk to (+0x1C)
std::uint32_t castBehaviorModuleToSubobjectPlus1C(
    std::uint32_t behaviorSubobjectAddress) noexcept {
    if (behaviorSubobjectAddress == 0x0Cu) {
        return 0u;
    }
    return behaviorSubobjectAddress + 0x10u;
}

// 28. 0x0068C7A0: FXListBehavior::findEntryByTrigger
std::uint32_t findFXListBehaviorEntryByTrigger(
    std::uint32_t entriesBaseAddress,
    const std::uint32_t* entryKeys,
    std::size_t count,
    std::uint32_t triggerKey) noexcept {
    if (entriesBaseAddress == 0u || entryKeys == nullptr || count == 0u) {
        return 0u;
    }
    for (std::size_t i = 0; i < count; ++i) {
        if (entryKeys[i] == triggerKey) {
            return entriesBaseAddress + static_cast<std::uint32_t>(i * 0x18u);
        }
    }
    return 0u;
}

// 29. 0x0068CEF0: GateOpenAndCloseBehavior::canObjectPassThrough
bool canObjectPassThroughGate(
    std::uint32_t gateState30,
    bool hasObject,
    std::uint32_t templateFlagsC0,
    std::uint32_t objectGateState454) noexcept {
    if (gateState30 != 1u || !hasObject) {
        return false;
    }
    if ((templateFlagsC0 & 0x10u) != 0u) {
        return false;
    }
    return objectGateState454 == 1u;
}

// 30. 0x0068CF20: GateOpenAndCloseBehavior::stopGateAudioAndMarkClosed
GateAudioStopResult stopGateAudioAndMarkClosed(
    std::uint32_t audioHandle48) noexcept {
    return GateAudioStopResult{audioHandle48, 1u};
}

// 31. 0x0068D3B0: SweepingLaserStateModule::computeCurrentSweepPosition
void computeSweepingLaserPosition(
    std::uint32_t endFrame58,
    std::uint32_t currentFrame,
    std::uint32_t durationFramesB0,
    bool reverseBC,
    const Coord3D68& startPos98,
    const Coord3D68& endPosA4,
    Coord3D68& outPos) noexcept {
    const std::uint32_t delta = endFrame58 - currentFrame;
    const float deltaF = static_cast<float>(static_cast<double>(delta));
    float t = static_cast<float>(
        static_cast<double>(deltaF) / static_cast<double>(durationFramesB0));
    if (reverseBC) {
        t = static_cast<float>(1.0 - static_cast<double>(t));
    }

    const double td = static_cast<double>(t);
    outPos.x = static_cast<float>(
        (static_cast<double>(endPosA4.x) - static_cast<double>(startPos98.x)) * td +
        static_cast<double>(startPos98.x));
    outPos.y = static_cast<float>(
        (static_cast<double>(endPosA4.y) - static_cast<double>(startPos98.y)) * td +
        static_cast<double>(startPos98.y));
    outPos.z = static_cast<float>(
        (static_cast<double>(endPosA4.z) - static_cast<double>(startPos98.z)) * td +
        static_cast<double>(startPos98.z));
}

// 32. 0x0068D650: LUAEventListBehavior::sumEntryValues
std::uint32_t sumLUAEventListEntries(
    const std::uint32_t* entryFirstWords,
    std::size_t count) noexcept {
    std::uint32_t total = 0u;
    for (std::size_t i = 0; i < count; ++i) {
        total += entryFirstWords[i];
    }
    return total;
}

// 33. 0x0068D790: ParkingPlaceBehavior::hasReservedSpace
bool hasParkingPlaceReservedSpace(
    bool gotInfo48,
    std::uint32_t objectId,
    const std::uint32_t* spaceObjectIds,
    std::size_t spaceCount) noexcept {
    if (!gotInfo48 || objectId == 0u || spaceObjectIds == nullptr) {
        return false;
    }
    for (std::size_t i = 0; i < spaceCount; ++i) {
        if (spaceObjectIds[i] == objectId) {
            return true;
        }
    }
    return false;
}

// 34. 0x0068D860: ParkingPlaceBehavior::canReserveSpaceForObject
ParkingCanReserveResult canParkingPlaceReserveSpaceForObject(
    std::uint32_t objectStatus88,
    std::uint32_t templateAddress,
    std::uint32_t playerAddress,
    bool virtualHasSpaceResult) noexcept {
    ParkingCanReserveResult out{false, false, 0u, 0u};
    if (((objectStatus88 >> 12u) & 1u) != 0u) {
        return out;
    }
    out.virtualCalled = true;
    out.templatePassed = templateAddress;
    out.playerPassed = playerAddress;
    out.allowed = virtualHasSpaceResult;
    return out;
}

// 35. 0x0068D890: ParkingPlaceBehavior::onObjectExit
ParkingExitResult onParkingPlaceObjectExit(
    std::uint32_t exitingObjectId,
    std::uint32_t initialPlayerCounter1314,
    std::uint32_t moduleDataDecrement20) noexcept {
    return ParkingExitResult{
        exitingObjectId,
        initialPlayerCounter1314 - moduleDataDecrement20};
}

// 36. 0x0068D8C0: ParkingPlaceBehavior::releaseDoorForExit
void releaseParkingPlaceDoorForExit(
    ParkingSpaceSlot68* spaces,
    std::size_t count,
    std::int32_t exitDoor) noexcept {
    if (exitDoor == -2 || spaces == nullptr) {
        return;
    }
    for (std::size_t i = 0; i < count; ++i) {
        if (spaces[i].door30 == exitDoor) {
            spaces[i].objectInSpace34 = 0u;
            spaces[i].reservedForExit38 = 0u;
            return;
        }
    }
}

// 37. 0x0068DA00: ParkingPlaceBehavior::matchesObjectFilter
ObjectFilterCallResult matchesParkingPlaceObjectFilter(
    std::uint32_t filterAddress,
    std::uint32_t candidateObjectAddress,
    std::uint32_t ownerPlayerAddress,
    std::uint32_t filterReturnVal) noexcept {
    return ObjectFilterCallResult{
        filterReturnVal,
        filterAddress,
        candidateObjectAddress,
        ownerPlayerAddress};
}

// 38. 0x0068DA20: PassiveAreaEffectBehavior::getEffectValue
std::uint32_t getPassiveAreaEffectValue(
    std::uint32_t moduleDataValue2C,
    std::uint32_t& outValue) noexcept {
    outValue = moduleDataValue2C;
    return outValue;
}

// 39. 0x0068DBF0: PlayerPowerManagerBehavior::getOwnerObject
std::uint32_t getPlayerPowerManagerOwnerObject(
    std::uint32_t ownerFieldAtBasePlus08) noexcept {
    return ownerFieldAtBasePlus08;
}

// 40. 0x0068DC00: PlayerPowerManagerBehavior::isForObject
bool isPlayerPowerManagerForObject(
    std::uint32_t ownerFieldAtBasePlus08,
    std::uint32_t candidateObjectAddress) noexcept {
    return ownerFieldAtBasePlus08 == candidateObjectAddress;
}

// 41. 0x0068DC90: PlayerPowerManagerBehavior::getEntryCount
std::int32_t getPlayerPowerManagerEntryCount(
    std::uint32_t beginAddress,
    std::uint32_t endAddress) noexcept {
    return static_cast<std::int32_t>(endAddress - beginAddress) >> 2;
}

// 42. 0x0068DCA0: PlayerPowerManagerBehavior::getEntryByIndex
std::uint32_t getPlayerPowerManagerEntryByIndex(
    const std::uint32_t* entries,
    std::int32_t count,
    std::int32_t index) noexcept {
    if (entries == nullptr || index < 0 || index >= count) {
        return 0u;
    }
    return entries[index];
}

// 43. 0x0068DCD0: PlayerPowerManagerBehavior::findActiveEntry
bool findActivePlayerPowerManagerEntry(
    const PowerManagerEntryProbe68* entries,
    std::size_t count,
    std::uint32_t mode,
    bool hasOutPointer,
    std::uint32_t& outEntryAddress) noexcept {
    if (entries == nullptr || count == 0u) {
        return false;
    }
    for (std::size_t i = 0; i < count; ++i) {
        const PowerManagerEntryProbe68& probe = entries[i];
        bool flag = false;
        if (mode == 1u) {
            flag = true;
        } else {
            flag = (1.0f > probe.metricValue);
        }
        const bool accepted = flag ? probe.acceptsFlag1 : probe.acceptsFlag0;
        if (accepted) {
            if (hasOutPointer) {
                outEntryAddress = probe.entryAddress;
            }
            return true;
        }
    }
    return false;
}

// 44. 0x0068DE80: PoisonedBehavior::onHealing
PoisonedHealingResult onPoisonedBehaviorHealing(
    std::uint32_t damageSubobjectAddress,
    std::uint32_t ownerAddress,
    bool hasDrawable,
    std::uint8_t initialDrawableTintByte14C,
    std::uint32_t currentFrame) noexcept {
    PoisonedHealingResult out{};
    out.poisonDamageFrame04 = 0u;
    out.poisonStopFrame08 = 0u;
    out.poisonDamageAmountBits0C = 0u;
    out.drawableTintByte14C = hasDrawable
        ? static_cast<std::uint8_t>(initialDrawableTintByte14C & ~0x04u)
        : initialDrawableTintByte14C;
    out.wake.called = true;
    out.wake.objectAddress = ownerAddress;
    out.wake.updateSubobjectAddress = damageSubobjectAddress - 0x14u;
    out.wake.wakeFrame = currentFrame + kSleepForeverOffset;
    return out;
}

// 45. 0x0068E410: RadarBehavior::enableObjectRadarOnPlayer
RadarEnableResult enableObjectRadarOnPlayer(
    bool virtualCanEnable,
    std::uint32_t playerAddress,
    std::uint32_t ownerObjectId,
    std::uint8_t /*initialFlag04*/) noexcept {
    RadarEnableResult out{false, 0u, 0u, 0u};
    if (virtualCanEnable && playerAddress != 0u) {
        out.playerRadarAdded = true;
        out.playerAddress = playerAddress;
        out.objectIdPassed = ownerObjectId;
    }
    out.flag04 = 0u;
    return out;
}

// 46. 0x0068E440: RadarBehavior::disableObjectRadarOnPlayer
RadarDisableResult disableObjectRadarOnPlayer(
    std::uint32_t playerAddress,
    std::uint32_t ownerObjectId) noexcept {
    RadarDisableResult out{false, 0u, 0u};
    if (playerAddress != 0u) {
        out.playerRadarRemoved = true;
        out.playerAddress = playerAddress;
        out.objectIdPassed = ownerObjectId;
    }
    return out;
}

// 47. 0x0068E550: RestrictSpecialPowerBehavior::getRestrictionCenter
void getRestrictSpecialPowerPosition(
    bool hasOwner,
    const Coord3D68& ownerPosition,
    Coord3D68& outPosition) noexcept {
    if (!hasOwner) {
        outPosition = Coord3D68{0.0f, 0.0f, 0.0f};
        return;
    }
    outPosition.x = bitsToFloat(quietX87FloatLoadStoreBits(floatToBits(ownerPosition.x)));
    outPosition.y = bitsToFloat(quietX87FloatLoadStoreBits(floatToBits(ownerPosition.y)));
    outPosition.z = bitsToFloat(quietX87FloatLoadStoreBits(floatToBits(ownerPosition.z)));
}

// 48. 0x0068E5F0: RestrictSpecialPowerBehavior::sumFirstNWeights
float sumRestrictSpecialPowerWeights(
    const float* weights,
    std::size_t weightCount,
    std::int32_t limit20) noexcept {
    if (weights == nullptr || weightCount == 0u || limit20 <= 0) {
        return 0.0f;
    }

    float sum = 0.0f;
    float current = weights[0];
    const std::uint32_t count = static_cast<std::uint32_t>(weightCount);

    for (std::int32_t i = 0; i < limit20; ++i) {
        sum += current;
        const std::uint32_t nextIdx = static_cast<std::uint32_t>(i + 1);
        if (nextIdx < count) {
            current = weights[nextIdx];
        }
    }
    return sum;
}

// 49. 0x0068E820: SecondaryObjectListenerModule::findMatchingEntry
std::uint32_t findSecondaryObjectListenerEntry(
    std::uint32_t entriesBaseAddress,
    const std::uint8_t* filterMatches,
    std::size_t count) noexcept {
    if (entriesBaseAddress == 0u || filterMatches == nullptr || count == 0u) {
        return 0u;
    }
    for (std::size_t i = 0; i < count; ++i) {
        if (filterMatches[i] != 0u) {
            return entriesBaseAddress + static_cast<std::uint32_t>(i * 0x78u);
        }
    }
    return 0u;
}

// 50. 0x0068E880: SecondaryObjectListenerModule::hasMatchingEntry
std::uint32_t hasSecondaryObjectListenerEntry(
    std::uint32_t entriesBaseAddress,
    const std::uint8_t* filterMatches,
    std::size_t count) noexcept {
    return findSecondaryObjectListenerEntry(entriesBaseAddress, filterMatches, count) != 0u
        ? 1u
        : 0u;
}

// 51. 0x0068EA70: ShareExperienceBehavior::calculateShareWeight
float calculateShareExperienceWeight(
    float radius08,
    float falloffFlag0C,
    const Coord3D68& sourcePosition,
    const Coord3D68& targetPosition) noexcept {
    if (!isOrderedEqual(falloffFlag0C, 1.0f)) {
        return 1.0f;
    }

    const float dx = targetPosition.x - sourcePosition.x;
    const float dy = targetPosition.y - sourcePosition.y;
    const float dz = targetPosition.z - sourcePosition.z;

    const double dist = std::sqrt(
        (static_cast<double>(dz) * static_cast<double>(dz) +
         static_cast<double>(dx) * static_cast<double>(dx)) +
        static_cast<double>(dy) * static_cast<double>(dy));
    const float ratio = static_cast<float>(dist / static_cast<double>(radius08));
    const float rawWeight = 1.0f - ratio;
    return (0.0f > rawWeight) ? 0.0f : rawWeight;
}

// 52. 0x0068EC50: SlowDeathBehavior::getProbabilityModifier
std::int32_t getSlowDeathProbabilityModifier(
    float maxHealth,
    std::int32_t baseProbabilityModifier0C,
    float bonusPerOverkillPercent10,
    float actualDamageDealt8C,
    float actualDamageClipped90) noexcept {
    std::int32_t overkillModifier = 0;
    if (maxHealth > 0.0f && bonusPerOverkillPercent10 != 0.0f) {
        const std::int32_t overkillDamage =
            static_cast<std::int32_t>(actualDamageDealt8C - actualDamageClipped90);
        const float ratio = static_cast<float>(overkillDamage) / maxHealth;
        overkillModifier = static_cast<std::int32_t>(ratio * bonusPerOverkillPercent10);
    }

    const std::int32_t total = baseProbabilityModifier0C + overkillModifier;
    return (total > 1) ? total : 1;
}

// 53. 0x0068EF90: SpawnBehavior::scheduleOnUpgrade
WakeScheduleCall scheduleSpawnBehaviorOnUpgrade(
    std::uint32_t upgradeSubobjectAddress,
    std::uint32_t ownerAddress,
    std::uint8_t moduleFlag59,
    std::uint32_t ownerFlags90,
    float delaySeconds3C,
    bool hasGameLogic,
    std::uint32_t currentFrame,
    float gameLogicFrameScale,
    float fpsGlobal) noexcept {
    WakeScheduleCall out{false, 0u, 0u, 0u};
    if (moduleFlag59 == 0u || ((ownerFlags90 >> 6u) & 1u) == 0u) {
        return out;
    }

    const std::uint32_t delayFrames = computeSecondsToFramesTruncated(
        delaySeconds3C, hasGameLogic, gameLogicFrameScale, fpsGlobal);
    out.called = true;
    out.objectAddress = ownerAddress;
    out.updateSubobjectAddress = upgradeSubobjectAddress + 0x10u;
    out.wakeFrame = currentFrame + delayFrames;
    return out;
}

// 54. 0x0068F050: SpawnBehavior::scheduleReplacement
WakeScheduleCall scheduleSpawnBehaviorReplacement(
    std::uint32_t subobjectAddress,
    std::uint32_t ownerAddress,
    std::uint8_t moduleFlag59,
    float delaySeconds3C,
    bool hasGameLogic,
    std::uint32_t currentFrame,
    float gameLogicFrameScale,
    float fpsGlobal) noexcept {
    WakeScheduleCall out{false, 0u, 0u, 0u};
    if (moduleFlag59 == 0u) {
        return out;
    }

    const std::uint32_t delayFrames = computeSecondsToFramesTruncated(
        delaySeconds3C, hasGameLogic, gameLogicFrameScale, fpsGlobal);
    out.called = true;
    out.objectAddress = ownerAddress;
    out.updateSubobjectAddress = subobjectAddress + 0x10u;
    out.wakeFrame = currentFrame + delayFrames;
    return out;
}

// 55. 0x0068F0F0: SpawnBehavior::shouldTryToSpawn
SpawnTryResult shouldSpawnBehaviorTryToSpawn(
    std::uint8_t active68,
    std::uint8_t isOneShot4C,
    std::uint32_t objectStatus80,
    std::uint32_t objectDisabledMaskC0,
    std::uint8_t isNeutralControlled471) noexcept {
    SpawnTryResult out{false, false};
    if (active68 == 0u) {
        return out;
    }

    if (isOneShot4C != 0u && ((objectStatus80 >> 23u) & 1u) != 0u) {
        out.stopSpawningCalled = true;
        return out;
    }

    if (((objectStatus80 >> 2u) & 1u) != 0u || ((objectStatus80 >> 21u) & 1u) != 0u) {
        return out;
    }

    if (((objectDisabledMaskC0 >> 10u) & 1u) != 0u) {
        return out;
    }

    out.shouldSpawn = ((isNeutralControlled471 ^ 1u) & 1u) != 0u;
    return out;
}

// 56. 0x0068F1E0: StancesBehavior::canAddEntry
std::uint32_t canStancesBehaviorAddEntry(
    std::uint32_t beginAddress14,
    std::uint32_t endAddress18,
    std::uint32_t maxEntries08) noexcept {
    const std::uint32_t count = static_cast<std::uint32_t>(
        static_cast<std::int32_t>(endAddress18 - beginAddress14) >> 2);
    return (count < maxEntries08) ? 1u : 0u;
}

// 57. 0x0068F200: StancesBehavior::getOrPopLastEntry
void getOrPopLastStanceEntry(
    std::uint32_t* entries,
    std::size_t& count,
    bool pop,
    std::uint32_t& outValue) noexcept {
    outValue = 0u;
    if (entries == nullptr || count == 0u) {
        return;
    }
    outValue = entries[count - 1u];
    if (pop) {
        --count;
    }
}

// 58. 0x0068F240: StancesBehavior::selectNextEntryByUsage
std::uint32_t selectNextStanceEntryByUsage(
    StanceUsageEntry68* entries,
    std::size_t count,
    std::int32_t maxUseCount0C) noexcept {
    if (entries == nullptr || count == 0u) {
        return 0u;
    }

    const std::uint8_t firstUse = entries[0].useCount04;
    const std::uint32_t targetUse = (firstUse == 0u) ? 1u : static_cast<std::uint32_t>(firstUse);

    for (std::size_t i = 0; i < count; ++i) {
        if (static_cast<std::uint32_t>(entries[i].useCount04) < targetUse) {
            ++entries[i].useCount04;
            return entries[i].value00;
        }
    }

    if (static_cast<std::int32_t>(static_cast<std::uint32_t>(firstUse)) < maxUseCount0C) {
        ++entries[0].useCount04;
        return entries[0].value00;
    }

    return 0u;
}

// 59. 0x0068F2C0: StancesBehavior::findObjectBehaviorByTemplateId
std::uint32_t findObjectBehaviorByTemplateId(
    const BehaviorTemplateCandidate68* candidates,
    std::size_t count,
    std::uint32_t targetTemplateId) noexcept {
    if (candidates == nullptr) {
        return 0u;
    }
    for (std::size_t i = 0; i < count; ++i) {
        if (candidates[i].subobjectAddress != 0u &&
            candidates[i].templateId08 == targetTemplateId) {
            return candidates[i].subobjectAddress;
        }
    }
    return 0u;
}

// 60. 0x0068F420: StreamStateModule::deactivateAndSleep
WakeScheduleCall deactivateStreamStateAndSleep(
    std::uint32_t subobjectAddress,
    std::uint32_t ownerAddress,
    std::uint32_t currentFrame,
    std::uint8_t& activeFlag40) noexcept {
    WakeScheduleCall out{false, 0u, 0u, 0u};
    if (activeFlag40 != 0u) {
        activeFlag40 = 0u;
        out.called = true;
        out.objectAddress = ownerAddress;
        out.updateSubobjectAddress = subobjectAddress - 0x14u;
        out.wakeFrame = currentFrame + kSleepForeverOffset;
    }
    return out;
}

// 61. 0x0068F520: StructurePlacementBehavior::getFromObject
StaticModuleFinderResult getStructurePlacementBehaviorFromObject(
    StaticModuleKeyState state,
    std::uint32_t resolverKey,
    const std::uint32_t* moduleAddresses,
    const std::uint32_t* moduleKeys,
    std::size_t count) noexcept {
    return resolveStaticModuleFromObject(
        state,
        resolverKey,
        "StructurePlacementBehavior",
        moduleAddresses,
        moduleKeys,
        count);
}

// 62. 0x0068F790: SupplyWarehouseCripplingBehavior::onDamage
WakeScheduleCall onSupplyWarehouseCripplingDamage(
    std::uint32_t damageSubobjectAddress,
    std::uint32_t ownerAddress,
    std::uint32_t frameBeforeReset,
    std::uint32_t frameAfterReset,
    std::uint32_t healingSuppressedUntilFrame04) noexcept {
    WakeScheduleCall out{};
    out.called = true;
    out.objectAddress = ownerAddress;
    out.updateSubobjectAddress = damageSubobjectAddress - 0x14u;
    out.wakeFrame = frameAfterReset + healingSuppressedUntilFrame04 - frameBeforeReset;
    return out;
}

// 63. 0x0068F7D0: SupplyWarehouseCripplingBehavior::startCrippledEffects
DockCrippledCallResult startSupplyWarehouseCrippledEffects(
    const std::uint32_t* dockInterfaces,
    std::size_t moduleCount) noexcept {
    DockCrippledCallResult out{0u, 1u};
    if (dockInterfaces == nullptr) {
        return out;
    }
    for (std::size_t i = 0; i < moduleCount; ++i) {
        if (dockInterfaces[i] != 0u) {
            out.dockInterfaceAddress = dockInterfaces[i];
            break;
        }
    }
    return out;
}

// 64. 0x0068F810: SupplyWarehouseCripplingBehavior::stopCrippledEffects
DockCrippledCallResult stopSupplyWarehouseCrippledEffects(
    const std::uint32_t* dockInterfaces,
    std::size_t moduleCount) noexcept {
    DockCrippledCallResult out{0u, 0u};
    if (dockInterfaces == nullptr) {
        return out;
    }
    for (std::size_t i = 0; i < moduleCount; ++i) {
        if (dockInterfaces[i] != 0u) {
            out.dockInterfaceAddress = dockInterfaces[i];
            break;
        }
    }
    return out;
}

// 65. 0x0068FC00: TiberiumFieldBehavior::updateTimersAndGetSleep
std::uint32_t updateTiberiumFieldTimersAndGetSleep(
    std::uint32_t currentFrame,
    float gameLogicFrameScale,
    std::uint32_t logicFramesPerSecond,
    std::uint32_t baseSeconds70,
    std::uint32_t interval80,
    std::uint32_t interval84,
    float rate8C,
    std::uint32_t& timer94,
    std::uint32_t& timer98,
    std::uint32_t& timer9C) noexcept {
    if (currentFrame >= timer94) {
        const std::int32_t baseTicks =
            static_cast<std::int32_t>(baseSeconds70 * logicFramesPerSecond);
        std::int32_t step = static_cast<std::int32_t>(
            static_cast<double>(baseTicks) * static_cast<double>(gameLogicFrameScale));
        if (rate8C != 0.0f) {
            const float invRate = 1.0f / rate8C;
            step = static_cast<std::int32_t>(invRate * static_cast<float>(step));
        }
        timer94 = currentFrame + static_cast<std::uint32_t>(step);
    }

    if (currentFrame >= timer98) {
        timer98 = currentFrame + interval80;
    }

    if (currentFrame >= timer9C) {
        timer9C = currentFrame + interval84;
    }

    const std::uint32_t earliest = (timer94 < timer98) ? timer94 : timer98;
    const std::uint32_t minNext = (earliest < timer9C) ? earliest : timer9C;
    return minNext - currentFrame;
}

// 66. 0x0068FCE0: TiberiumFieldBehavior::setFieldActiveState
void setTiberiumFieldActiveState(
    std::uint8_t& fieldActiveA0,
    std::uint8_t newActive,
    std::uint32_t currentFrame,
    TiberiumCrystalSlot68* slots,
    std::size_t slotCount) noexcept {
    if (fieldActiveA0 == newActive) {
        return;
    }

    fieldActiveA0 = newActive;
    for (std::size_t i = 0; i < slotCount; ++i) {
        TiberiumCrystalSlot68& slot = slots[i];
        if (slot.state00 == 8u &&
            slot.submodule08 != nullptr &&
            slot.submodule08->active28 != newActive) {
            slot.submodule08->active28 = newActive;
            if (newActive != 0u) {
                slot.submodule08->activatedFrame2C = currentFrame;
            }
            slot.submodule08->crystalState40 = 0u;
        }
    }
}

// 67. 0x0068FD40: TiberiumFieldBehavior::bindCrystalSlotModule
BindCrystalSlotResult bindTiberiumFieldCrystalSlot(
    std::uint8_t fieldActiveA0,
    std::uint32_t slotState00,
    StaticModuleKeyState keyState,
    std::uint32_t resolverKey,
    const std::uint32_t* moduleAddresses,
    const std::uint32_t* moduleKeys,
    std::size_t moduleCount,
    std::uint8_t initialSubmoduleActive28,
    std::uint32_t initialSubmoduleCrystalState40) noexcept {
    BindCrystalSlotResult out{};
    out.finder.guard = keyState.guard;
    out.finder.cachedKey = keyState.cachedKey;
    out.finder.resolverCalled = false;
    out.finder.resolvedName = "TiberiumFieldBehavior";
    out.boundModuleAddress = 0u;
    out.submoduleActive28 = initialSubmoduleActive28;
    out.submoduleCrystalState40 = initialSubmoduleCrystalState40;

    if (slotState00 != 8u) {
        return out;
    }

    out.finder = resolveStaticModuleFromObject(
        keyState,
        resolverKey,
        "TiberiumFieldBehavior",
        moduleAddresses,
        moduleKeys,
        moduleCount);
    out.boundModuleAddress = out.finder.moduleAddress;

    if (fieldActiveA0 == 0u &&
        out.boundModuleAddress != 0u &&
        out.submoduleActive28 != 0u) {
        out.submoduleActive28 = 0u;
        out.submoduleCrystalState40 = 0u;
    }

    return out;
}

// 68. 0x0068FDC0: TiberiumFieldBehavior::hasNoSlotInStateOne
bool hasNoTiberiumFieldSlotInStateOne(
    const std::uint32_t* slotStates,
    std::int32_t cols38,
    std::int32_t rows3C) noexcept {
    if (cols38 <= 0 || rows3C <= 0 || slotStates == nullptr) {
        return true;
    }
    for (std::int32_t c = 0; c < cols38; ++c) {
        for (std::int32_t r = 0; r < rows3C; ++r) {
            const std::size_t idx = static_cast<std::size_t>(c + r * rows3C);
            if (slotStates[idx] == 1u) {
                return false;
            }
        }
    }
    return true;
}

// 69. 0x0068FE10: TiberiumFieldBehavior::hasActiveSlotInStateEight
bool hasActiveTiberiumFieldSlotInStateEight(
    const std::uint32_t* slotStates,
    const std::uint32_t* slotActiveFlags,
    std::int32_t cols38,
    std::int32_t rows3C) noexcept {
    if (cols38 <= 0 || rows3C <= 0 || slotStates == nullptr || slotActiveFlags == nullptr) {
        return false;
    }
    for (std::int32_t c = 0; c < cols38; ++c) {
        for (std::int32_t r = 0; r < rows3C; ++r) {
            const std::size_t idx = static_cast<std::size_t>(c + r * rows3C);
            if (slotStates[idx] == 8u && slotActiveFlags[idx] != 0u) {
                return true;
            }
        }
    }
    return false;
}

// 70. 0x0068FE60: TiberiumFieldBehavior::pickRandomEntry
std::uint32_t pickRandomTiberiumFieldEntry(
    const std::uint32_t* entries,
    std::size_t count,
    std::uint32_t sampledIndex) noexcept {
    if (entries == nullptr || count == 0u) {
        return 0u;
    }
    return entries[sampledIndex];
}

}  // namespace ra3::gamelogic
