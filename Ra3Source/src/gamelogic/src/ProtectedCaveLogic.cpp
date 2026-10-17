#include "ra3/gamelogic/ProtectedCaveLogic.h"

#include <cstring>

namespace ra3::gamelogic {

namespace {

std::uint32_t floatToBits(float v) {
    std::uint32_t bits = 0U;
    std::memcpy(&bits, &v, sizeof(bits));
    return bits;
}

}  // namespace

// 0x00695550
std::uint32_t constructBehaviorModule00C01108(
    std::uint32_t selfPtr,
    std::uint32_t thingPtr,
    std::uint32_t resolvedOwnerPtr,
    std::uint32_t moduleDataPtr,
    BehaviorModule00C01108Layout& out) {
    out.words[0] = 0x00C01108U;
    out.words[1] = moduleDataPtr;
    out.words[2] = (thingPtr != 0U) ? resolvedOwnerPtr : 0U;
    // Cave 0x004037D6 writes [esi+0x0C] = 0x00BDB9A8, then 0x006955A4 overwrites with 0x00C00FD8.
    out.words[3] = 0x00C00FD8U;
    out.words[4] = 0x00C00F30U;
    out.words[5] = 0x00C00F18U;
    out.words[6] = 0x00C00F10U;
    out.words[7] = 0x00C01168U;
    out.words[8] = selfPtr + 0x20U;
    out.words[9] = selfPtr + 0x20U;
    out.words[10] = 0x00C00F0CU;
    out.words[11] = 0U;
    out.words[12] = 0x7FFFFFFFU;
    out.words[13] = 0U;
    out.words[14] = 0U;
    out.words[15] = 0U;
    out.words[16] &= 0xFFFFFF00U;
    out.words[17] = 0U;
    out.words[18] &= 0xFFFF0000U;
    return selfPtr;
}

// 0x00699780
bool hasActiveSubController(
    std::uint32_t state21C,
    bool hasSubController,
    std::uint32_t ptr10,
    std::uint32_t ptr24,
    std::int32_t& cachedState38,
    std::uint32_t firstNodeTag20) {
    if (state21C != 1U && state21C != 4U) {
        return false;
    }
    if (!hasSubController || ptr10 == 0U || ptr24 == 0U) {
        return false;
    }
    if (cachedState38 >= 0) {
        return cachedState38 > 0;
    }
    cachedState38 = (firstNodeTag20 != 0x7FFFFFFFU) ? 1 : 0;
    return cachedState38 > 0;
}

// 0x0069E780
std::int32_t computePartitionModifierCount(
    bool hasOwner,
    bool hasControllingPlayer,
    bool hasSpec,
    std::uint32_t specMode,
    std::int32_t mode1Count,
    std::int32_t mode2Count,
    bool hasModifierList,
    bool modifierApplied,
    float modifierDivisor) {
    if (!hasOwner || !hasControllingPlayer || !hasSpec) {
        return 1;
    }
    std::int32_t result = 1;
    if (specMode == 1U) {
        result = mode1Count;
    } else if (specMode == 2U) {
        result = mode2Count;
    }
    if (hasModifierList && modifierApplied) {
        result = static_cast<std::int32_t>(static_cast<float>(result) / modifierDivisor);
    }
    return result;
}

// 0x006A51A0
std::uint32_t queryObjectSubModule(
    bool hasContainer37C,
    const SubModuleEntry* entries,
    std::size_t entryCount,
    std::int32_t targetTag) {
    if (!hasContainer37C || entries == nullptr) {
        return 0U;
    }
    for (std::size_t i = 0; i < entryCount; ++i) {
        const SubModuleEntry& e = entries[i];
        bool matched = (e.tag == targetTag);
        if (!matched && targetTag == -1) {
            matched = (e.wildcardPredicateResult != 0U);
        }
        if (matched && e.interfaceResult != 0U) {
            return e.interfaceResult;
        }
    }
    return 0U;
}

// 0x006A5FA0
bool isObjectEligibleForStatusQuery(
    std::uint8_t flags471,
    std::uint32_t flags80,
    std::uint8_t tmplFlagsC7,
    std::uint32_t tmplFlagsC0,
    std::uint32_t flagsB4,
    std::uint32_t statusWord26C,
    std::uint32_t flags84,
    std::uint8_t filterByte0,
    std::uint8_t filterByte1,
    std::uint8_t filterByte2,
    std::uint8_t modeFlag) {
    if ((flags471 & 0x01U) != 0U) {
        return false;
    }
    if ((flags80 & 0x01U) != 0U) {
        return false;
    }
    if ((tmplFlagsC7 & 0x01U) != 0U) {
        return false;
    }
    if (filterByte0 != 0U && ((tmplFlagsC0 >> 9U) & 1U) != 0U) {
        if (((flagsB4 >> 7U) & 1U) != 0U || ((flagsB4 >> 8U) & 1U) != 0U) {
            return false;
        }
    }
    if (modeFlag != 0U) {
        if (filterByte1 != 0U && ((statusWord26C >> 6U) & 1U) != 0U) {
            return false;
        }
    } else {
        if (filterByte1 != 0U &&
            (((statusWord26C >> 2U) & 1U) != 0U || ((statusWord26C >> 6U) & 1U) != 0U)) {
            return false;
        }
    }
    if (filterByte2 != 0U && ((flags84 >> 23U) & 1U) != 0U) {
        return false;
    }
    return true;
}

// 0x006A6460
void unregisterObjectFromSubsystems(
    std::uint8_t& flag4D8,
    std::uint8_t& flag4D7,
    std::uint8_t skip4D8Arg,
    bool hasSub9C,
    std::uint32_t tmplFlagsC8,
    bool hasSub4E4,
    bool hasSub4E8,
    bool hasGlobalDFC044,
    bool hasControllingPlayer418,
    std::uint32_t& outCallMask) {
    outCallMask = 0U;
    if (flag4D8 != 0U && skip4D8Arg != 1U) {
        outCallMask |= 0x01U;
        flag4D8 = 0U;
    }
    if (flag4D7 != 0U) {
        if (hasSub9C) {
            outCallMask |= 0x02U;
        }
        if (((tmplFlagsC8 >> 25U) & 1U) == 0U) {
            if (hasSub4E4) {
                outCallMask |= 0x04U;
            }
            if (hasSub4E8) {
                outCallMask |= 0x08U;
            }
            if (hasGlobalDFC044) {
                outCallMask |= 0x10U;
            }
        }
        if (hasControllingPlayer418) {
            outCallMask |= 0x20U;
        }
        flag4D7 = 0U;
    }
}

// 0x006A6DC0
void updateObjectPositionDelta(
    std::uint8_t flag236,
    std::uint8_t flag234,
    std::uint8_t flag235,
    std::uint32_t lastFrame218,
    std::uint32_t currentFrame,
    const float pos38[3],
    const float vel1F4_204_214[3],
    const float prevPos21C[3],
    const float targetPos228[3],
    float outDelta[3]) {
    if (flag236 != 0U) {
        outDelta[0] = targetPos228[0] - pos38[0];
        outDelta[1] = targetPos228[1] - pos38[1];
        outDelta[2] = targetPos228[2] - pos38[2];
        return;
    }
    if (flag234 != 0U && lastFrame218 < currentFrame) {
        const std::uint32_t dt = currentFrame - lastFrame218;
        const float invDt = static_cast<float>(1.0 / static_cast<double>(dt));
        outDelta[0] = (pos38[0] + vel1F4_204_214[0] * (-1.0f)) * invDt;
        outDelta[1] = (pos38[1] + vel1F4_204_214[1] * (-1.0f)) * invDt;
        outDelta[2] = (pos38[2] + vel1F4_204_214[2] * (-1.0f)) * invDt;
        return;
    }
    if (flag235 != 0U && lastFrame218 <= currentFrame) {
        const std::uint32_t dt = (currentFrame - lastFrame218) + 1U;
        const float invDt = static_cast<float>(1.0 / static_cast<double>(dt));
        outDelta[0] = (pos38[0] - prevPos21C[0]) * invDt;
        outDelta[1] = (pos38[1] - prevPos21C[1]) * invDt;
        outDelta[2] = (pos38[2] - prevPos21C[2]) * invDt;
        return;
    }
    outDelta[0] = 0.0f;
    outDelta[1] = 0.0f;
    outDelta[2] = 0.0f;
}

// 0x006A7600
std::uint32_t getObjectControllingPlayer(
    std::uint32_t directPtr328,
    std::uint32_t listBegin32C,
    std::uint32_t listEnd330,
    std::uint32_t firstListElem) {
    if (directPtr328 != 0U) {
        return directPtr328;
    }
    return (listBegin32C != listEnd330) ? firstListElem : 0U;
}

// 0x006A8400
void notifyActiveBodyMaxHealthChanged(
    std::uint32_t state1C,
    std::uint32_t threshold70,
    std::uint32_t eventBC,
    std::uint32_t eventC0,
    std::uint32_t ownerId0C,
    std::uint32_t& outSelectedEvent,
    std::uint32_t& outBoundOwnerId,
    bool& outDispatched) {
    outSelectedEvent = (state1C < threshold70) ? eventBC : eventC0;
    outBoundOwnerId = ownerId0C;
    outDispatched = true;
}

// 0x006B7040
void assignAIUpdateTargetRef(
    std::uint32_t& currentTargetPtr,
    std::int32_t& oldTargetRefCount,
    bool& oldTargetDestroyed,
    std::uint8_t& flag428,
    std::uint8_t& flag432,
    std::uint32_t newTargetPtr,
    std::int32_t& newTargetRefCount,
    bool& newTargetDestroyed,
    bool& vtable270Called,
    bool& helper6B6900Called) {
    oldTargetDestroyed = false;
    newTargetDestroyed = false;
    if (currentTargetPtr != 0U) {
        --oldTargetRefCount;
        if (oldTargetRefCount <= 0) {
            oldTargetDestroyed = true;
        }
        currentTargetPtr = 0U;
    }
    flag428 = 0U;
    flag432 = 0U;
    vtable270Called = true;
    currentTargetPtr = newTargetPtr;
    if (newTargetPtr != 0U) {
        ++newTargetRefCount;
        ++newTargetRefCount;
    }
    helper6B6900Called = true;
    if (newTargetPtr != 0U) {
        --newTargetRefCount;
        if (newTargetRefCount <= 0) {
            newTargetDestroyed = true;
        }
    }
}

// 0x006BB360
void classifyObjectSurfaceAndKindMask(
    std::uint32_t kindOfC0,
    std::uint32_t kindOfC4,
    std::uint32_t kindOfC8,
    std::uint32_t kindOfD8,
    std::uint32_t status80,
    std::uint32_t status94,
    std::uint32_t status98,
    bool aiAirborne6875D0,
    bool isOverWater85CBC0,
    std::uint32_t& outMask) {
    outMask = 0U;
    if (((kindOfC4 >> 24U) & 1U) != 0U) {
        outMask |= 0x0201U;
        return;
    }
    if (((kindOfC4 >> 21U) & 1U) != 0U) {
        outMask |= 0x0020U;
        return;
    }
    if (((kindOfC8 >> 11U) & 1U) != 0U) {
        outMask |= 0x0100U;
        return;
    }
    if (((kindOfC0 >> 26U) & 1U) != 0U) {
        outMask |= 0x0010U;
        return;
    }
    if (((status80 >> 7U) & 1U) != 0U) {
        if (((status98 >> 19U) & 1U) != 0U) {
            outMask |= 0x0800U;
        }
        if (((kindOfD8 >> 12U) & 1U) != 0U ||
            ((kindOfC0 >> 11U) & 1U) != 0U ||
            ((kindOfC0 >> 14U) & 1U) != 0U) {
            outMask |= 0x2040U;
            return;
        }
        if (((kindOfC0 >> 10U) & 1U) != 0U) {
            outMask |= 0x1080U;
            return;
        }
        if (((kindOfC8 >> 14U) & 1U) != 0U) {
            outMask |= 0x0400U;
            return;
        }
        return;
    }
    if ((status98 & 1U) != 0U) {
        outMask |= 0x0004U;
    } else if (((status94 >> 31U) & 1U) != 0U ||
               ((status98 >> 21U) & 1U) != 0U ||
               aiAirborne6875D0) {
        outMask |= 0x0002U;
    } else if (((kindOfC0 >> 9U) & 1U) != 0U) {
        outMask |= 0x0008U;
        outMask |= isOverWater85CBC0 ? 0x0002U : 0x0001U;
        return;
    } else {
        outMask |= 0x0001U;
    }
    if (((kindOfC0 >> 10U) & 1U) != 0U) {
        outMask |= 0x1000U;
        return;
    }
    if (((kindOfD8 >> 12U) & 1U) != 0U) {
        outMask |= 0x2000U;
        return;
    }
}

// 0x006BD510
bool matchesObjectClassificationFilter(
    std::uint32_t kindOfC0,
    std::uint32_t kindOfC4,
    std::uint32_t kindOfC8,
    std::uint32_t kindOfD8,
    std::uint32_t status80,
    std::uint32_t status94,
    std::uint32_t status98,
    bool aiAirborne6875D0,
    bool isOverWater85CBC0,
    std::uint32_t forbiddenMaskD0,
    std::uint32_t forbiddenMaskD4) {
    std::uint32_t mask = 0U;
    classifyObjectSurfaceAndKindMask(
        kindOfC0, kindOfC4, kindOfC8, kindOfD8,
        status80, status94, status98,
        aiAirborne6875D0, isOverWater85CBC0, mask);
    if ((forbiddenMaskD0 & mask) != 0U) {
        return false;
    }
    if ((forbiddenMaskD4 & mask) != 0U) {
        return false;
    }
    return true;
}

// 0x006C0620
std::uint32_t getObjectIndicatorColor(
    std::uint32_t overrideColor424,
    std::uint32_t team414,
    std::uint32_t teamField1F8,
    std::uint32_t fallbackTeamAC,
    std::uint8_t tmplFlagsCB,
    std::uint32_t flagsC8,
    std::uint32_t shroudState,
    std::uint32_t disguisePlayerPtr,
    std::uint32_t disguisePlayerColor80,
    std::uint32_t teamPlayerPtr,
    std::uint32_t teamPlayerColor80) {
    if (overrideColor424 != 0U) {
        return overrideColor424;
    }
    const std::uint32_t resolvedTeam =
        (team414 != 0U && teamField1F8 == 0U) ? fallbackTeamAC : team414;
    if ((tmplFlagsCB & 0x01U) != 0U &&
        ((flagsC8 >> 9U) & 1U) != 0U &&
        shroudState != 2U) {
        return (disguisePlayerPtr != 0U) ? disguisePlayerColor80 : 0xFF000000U;
    }
    return (resolvedTeam != 0U && teamPlayerPtr != 0U) ? teamPlayerColor80 : 0xFF000000U;
}

// 0x006C0B60
void setObjectBit31AndNotifyModules(
    std::uint32_t& flagsC0,
    std::uint8_t flagsB3,
    std::uint32_t argPlayer,
    std::uint32_t ownerPlayer418,
    const std::uint32_t* moduleListenerFlags,
    std::size_t moduleCount,
    bool& markedDirty,
    std::uint32_t& notifiedCount) {
    markedDirty = false;
    notifiedCount = 0U;
    if (((flagsC0 >> 10U) & 1U) != 0U || (flagsB3 & 0x01U) != 0U) {
        return;
    }
    if (argPlayer == ownerPlayer418 && (flagsC0 & 0x01U) == 0U) {
        flagsC0 |= 0x01U;
        markedDirty = true;
    }
    if (moduleListenerFlags != nullptr) {
        for (std::size_t i = 0; i < moduleCount; ++i) {
            if (moduleListenerFlags[i] != 0U) {
                ++notifiedCount;
            }
        }
    }
}

// 0x006C1500
std::uint32_t findObjectModuleByKey(
    bool hasModuleArray310,
    std::uint32_t treeCount3C0,
    std::uint32_t defaultModulePtr,
    const ModuleTreeEntry* entries,
    std::size_t entryCount,
    std::int32_t searchKey) {
    if (!hasModuleArray310 || treeCount3C0 == 0U) {
        return 0U;
    }
    if (searchKey == -1) {
        return defaultModulePtr;
    }
    if (entries == nullptr) {
        return 0U;
    }
    std::size_t bestIdx = entryCount;
    for (std::size_t i = 0; i < entryCount; ++i) {
        if (entries[i].key >= searchKey) {
            if (bestIdx == entryCount || entries[i].key < entries[bestIdx].key) {
                bestIdx = i;
            }
        }
    }
    if (bestIdx == entryCount || searchKey < entries[bestIdx].key) {
        return 0U;
    }
    return entries[bestIdx].modulePtr;
}

// 0x006C1D60
void reassignObjectControllingPlayer(
    std::uint32_t directPtr328,
    std::uint32_t listBegin32C,
    std::uint32_t listEnd330,
    std::uint32_t firstListElem,
    bool subObjHasAI84,
    std::uint32_t& outActionMask) {
    const std::uint32_t subObj =
        (directPtr328 != 0U) ? directPtr328 : ((listBegin32C != listEnd330) ? firstListElem : 0U);
    if (subObj != 0U) {
        outActionMask = subObjHasAI84 ? 1U : 0U;
    } else {
        outActionMask = 2U;
    }
}

// 0x006C2C30
std::uint32_t updateObjectVisionShroudRegistration(
    bool hasPlayer418,
    bool hasShroud74,
    std::uint8_t checkActiveArg,
    std::uint32_t status26C,
    std::int32_t sightRange8C,
    std::uint32_t flags80,
    std::uint32_t flags88,
    std::uint32_t flags8C,
    std::uint32_t flags94,
    std::uint32_t tmplFlagsD0,
    std::uint8_t flag301,
    std::uint8_t& flags471) {
    if (!hasPlayer418 || !hasShroud74) {
        return 0U;
    }
    const bool statusMaskedNonZero = ((status26C & 0xFFFFFEBFU) != 0U);
    bool shouldRegister = false;
    if (checkActiveArg != 0U &&
        sightRange8C != 0 &&
        ((flags80 >> 2U) & 1U) == 0U &&
        ((flags88 >> 26U) & 1U) == 0U &&
        ((flags8C >> 11U) & 1U) == 0U &&
        ((flags94 >> 2U) & 1U) == 0U &&
        (((tmplFlagsD0 >> 21U) & 1U) == 0U || flag301 != 0U) &&
        (flags471 & 0x01U) == 0U &&
        (!statusMaskedNonZero || sightRange8C <= 0)) {
        shouldRegister = true;
    }
    const bool wasRegistered = ((flags471 & 0x20U) != 0U);
    if (wasRegistered == shouldRegister) {
        return 0U;
    }
    if (shouldRegister) {
        flags471 = static_cast<std::uint8_t>(flags471 | 0x20U);
        return 1U;
    }
    flags471 = static_cast<std::uint8_t>(flags471 & ~0x20U);
    return 2U;
}

// 0x006C4F10
void dispatchModuleCommandMessage16(
    const float targetPos[3],
    std::int32_t& refCount50,
    bool& refDestroyed,
    bool hasBuffer2C,
    bool hasBuffer1C,
    float outDispatchedPos[3],
    std::uint32_t& outFinalVtable,
    std::uint32_t& outFreedMask) {
    outDispatchedPos[0] = targetPos[0];
    outDispatchedPos[1] = targetPos[1];
    outDispatchedPos[2] = targetPos[2];
    outFinalVtable = 0x00BDB8E8U;
    refDestroyed = false;
    if (refCount50 > 0) {
        --refCount50;
        if (refCount50 <= 0) {
            refDestroyed = true;
        }
    }
    outFreedMask = 0U;
    if (hasBuffer2C) {
        outFreedMask |= 0x01U;
    }
    if (hasBuffer1C) {
        outFreedMask |= 0x02U;
    }
}

// 0x006C5480
void dispatchModuleCommandMessage3D(
    std::uint32_t commandArg,
    std::int32_t& refCount50,
    bool& refDestroyed,
    bool hasBuffer2C,
    bool hasBuffer1C,
    std::uint32_t& outDispatchedType,
    std::uint32_t& outDispatchedArg,
    std::uint32_t& outFinalVtable,
    std::uint32_t& outFreedMask) {
    outDispatchedType = 0x3DU;
    outDispatchedArg = commandArg;
    outFinalVtable = 0x00BDB8E8U;
    refDestroyed = false;
    if (refCount50 > 0) {
        --refCount50;
        if (refCount50 <= 0) {
            refDestroyed = true;
        }
    }
    outFreedMask = 0U;
    if (hasBuffer2C) {
        outFreedMask |= 0x01U;
    }
    if (hasBuffer1C) {
        outFreedMask |= 0x02U;
    }
}

// 0x006C6310
bool clearObjectStatusBit15AndMarkDirty(
    std::uint32_t& flagsB0,
    bool& markedDirty) {
    if ((flagsB0 & 0x00008000U) != 0U) {
        flagsB0 &= ~0x00008000U;
        markedDirty = true;
    } else {
        markedDirty = false;
    }
    return false;
}

// 0x0073C740
std::uint32_t constructUpdateModule00C11AA0(
    std::uint32_t selfPtr,
    std::uint32_t thingPtr,
    std::uint32_t resolvedOwnerPtr,
    std::uint32_t moduleDataPtr,
    float moduleDataFloat08,
    float moduleDataFloat14,
    UpdateModule00C11AA0Layout& out,
    bool& helper6906E0Called,
    bool& helper7221C0Called) {
    out.words[0] = 0x00C11AA0U;
    out.words[1] = moduleDataPtr;
    out.words[2] = (thingPtr != 0U) ? resolvedOwnerPtr : 0U;
    out.words[3] = 0x00C11970U;
    out.words[4] = 0x00C11898U;

    const float chosen = (moduleDataFloat14 == -1.0f) ? moduleDataFloat08 : moduleDataFloat14;
    out.words[5] = floatToBits(chosen);
    out.words[6] = floatToBits(moduleDataFloat14);
    out.words[7] = floatToBits(moduleDataFloat08);
    out.words[8] = floatToBits(chosen);
    out.words[9] = 0U;
    out.words[10] = 0U;
    out.words[11] = 0xFFFFFFFFU;
    out.words[12] = 0xFFFFFFFFU;
    out.words[13] = 0U;
    out.words[14] = 0U;
    out.words[15] = 0U;
    out.words[16] = 0x00000070U;
    out.words[17] = 0U;
    out.words[18] = 0x00BDC368U;
    out.words[19] = 0x00BDC178U;
    for (int i = 20; i <= 24; ++i) {
        out.words[i] = 0U;
    }
    out.words[25] = 0x0000000EU;
    out.words[26] = 0x00000070U;
    for (int i = 27; i <= 30; ++i) {
        out.words[i] = 0U;
    }
    out.words[31] = 0x00000001U;
    for (int i = 32; i <= 42; ++i) {
        out.words[i] = 0U;
    }
    out.words[43] = 0x3F800000U;
    out.words[44] = 0x3F800000U;
    out.words[45] = 0x42C80000U;
    for (int i = 46; i <= 50; ++i) {
        out.words[i] = 0U;
    }
    out.words[51] &= 0xFF000000U;
    out.words[52] = 0x00BDB99CU;
    out.words[53] = 0U;
    out.words[54] = 0U;
    out.words[55] &= 0xFFFFFF00U;
    out.words[56] = 0U;
    out.words[57] = selfPtr + 0xE4U;
    out.words[58] = selfPtr + 0xE4U;
    out.words[60] = 0U;
    out.words[61] = 0U;
    out.words[62] = 0U;
    out.words[64] = 0U;
    out.words[65] = 0U;
    out.words[66] = 0U;
    out.words[68] = 0U;
    out.words[69] = 0U;
    out.words[70] = 0U;
    out.words[71] = 0U;
    out.words[72] &= 0xFF000000U;
    helper6906E0Called = true;
    helper7221C0Called = true;
    return selfPtr;
}

// 0x006C8380
void destroyUpdateModule00C11AA0(
    std::uint32_t selfPtr,
    UpdateModule00C11AA0Layout& out,
    std::uint32_t& freedMask,
    bool& unlinkedIntrusiveNode) {
    freedMask = 0U;
    if (out.words[64] != 0U) {
        freedMask |= 0x01U;
    }
    if (out.words[60] != 0U) {
        freedMask |= 0x02U;
    }
    const std::uint32_t listSelf = selfPtr + 0xE4U;
    unlinkedIntrusiveNode = (out.words[57] != listSelf);
    out.words[57] = listSelf;
    out.words[58] = listSelf;
    out.words[4] = 0x00BF62D0U;
    out.words[3] = 0x00BDBAD8U;
    out.words[0] = 0x00BDB8E8U;
}

// 0x006CACE0
bool evaluateBehaviorEligibility00C00F4C(
    bool vtable9COk,
    bool helper6B4220Ok,
    std::uint32_t targetPtr,
    std::uint8_t flag1BB,
    bool helper6B44E0Ok,
    std::uint32_t vtable1CVal,
    bool disallow7F03B0,
    std::uint32_t ownerFlags88,
    std::uint32_t ownerFlags84,
    bool disallow6A4F20,
    bool hasSubMod687780,
    bool subModVtable18Ok) {
    if (!vtable9COk || !helper6B4220Ok) {
        return false;
    }
    if (targetPtr != 0U && flag1BB != 0U && !helper6B44E0Ok) {
        return false;
    }
    if (vtable1CVal != 0U && targetPtr != 0U && disallow7F03B0) {
        return false;
    }
    if (((ownerFlags88 >> 12U) & 1U) != 0U || ((ownerFlags84 >> 17U) & 1U) != 0U) {
        return false;
    }
    if (disallow6A4F20) {
        return false;
    }
    if (hasSubMod687780) {
        return subModVtable18Ok;
    }
    return true;
}

// 0x006D0330
std::uint32_t dispatchActiveBodyMaxHealthUpdate(
    std::int32_t limit70,
    std::int32_t& val1C) {
    if (limit70 <= 0) {
        const std::uint32_t oldVal = static_cast<std::uint32_t>(val1C);
        const std::uint32_t newVal =
            (limit70 != 0) ? static_cast<std::uint32_t>(limit70) : 0x7FFFFFFFU;
        val1C = static_cast<std::int32_t>(newVal);
        return (newVal > oldVal) ? 1U : 0U;
    }
    return (val1C < limit70) ? 2U : 0U;
}

// 0x006D1DD0
void broadcastObjectVisibilityState(
    std::uint32_t module310Count,
    bool skipShroud9C,
    bool hasDrawableF4,
    std::uint32_t drawable180Count,
    std::uint32_t drawable17CCount,
    std::uint32_t flags90,
    std::uint32_t tmplFlagsC0,
    std::uint32_t flags94,
    std::uint32_t& outNotifiedModules,
    bool& outUpdatedShroud,
    std::uint32_t& outNotifiedDrawable180,
    std::uint32_t& outNotifiedDrawable17C,
    bool& outCalledTail6A4D20) {
    outNotifiedModules = module310Count;
    outUpdatedShroud = !skipShroud9C;
    if (hasDrawableF4) {
        outNotifiedDrawable180 = drawable180Count;
        outNotifiedDrawable17C = drawable17CCount;
    } else {
        outNotifiedDrawable180 = 0U;
        outNotifiedDrawable17C = 0U;
    }
    if (((flags90 >> 6U) & 1U) == 0U &&
        ((tmplFlagsC0 >> 9U) & 1U) != 0U &&
        ((flags94 >> 5U) & 1U) != 0U) {
        outCalledTail6A4D20 = false;
    } else {
        outCalledTail6A4D20 = true;
    }
}

// 0x006E4760
std::uint32_t lookupObjectModuleInterface44ById(
    std::uint32_t objectId,
    std::uint32_t foundObjectPtr,
    std::uint8_t flags471,
    std::uint32_t tmplFlagsC4,
    std::uint32_t flags80,
    const std::uint32_t* moduleInterface44Results,
    std::size_t moduleCount,
    std::uint32_t& outObjectPtr) {
    outObjectPtr = 0U;
    if (objectId == 0U || foundObjectPtr == 0U) {
        return 0U;
    }
    if ((flags471 & 0x01U) != 0U ||
        ((tmplFlagsC4 >> 4U) & 1U) == 0U ||
        ((flags80 >> 21U) & 1U) != 0U) {
        return 0U;
    }
    outObjectPtr = foundObjectPtr;
    if (moduleInterface44Results != nullptr) {
        for (std::size_t i = 0; i < moduleCount; ++i) {
            if (moduleInterface44Results[i] != 0U) {
                return moduleInterface44Results[i];
            }
        }
    }
    return 0U;
}

// 0x006E5CE0
void clearObjectStatusWord0(std::uint32_t& statusWord26C) {
    if (statusWord26C != 0U) {
        statusWord26C = 0U;
    }
}

// 0x006EE7B0
void refreshObjectTemplateRegistration(
    std::uint32_t handle4E20E0,
    std::uint32_t obj4F13B0,
    std::int32_t intVal8D6970,
    std::uint32_t newValA7C770,
    float& outFloat0C,
    std::uint32_t& ioField24,
    bool& outVtable10Called,
    bool& outHelper6D17F0Called) {
    outVtable10Called = false;
    outHelper6D17F0Called = false;
    if (handle4E20E0 == 0U || obj4F13B0 == 0U) {
        return;
    }
    outVtable10Called = true;
    outFloat0C = static_cast<float>(intVal8D6970);
    if (newValA7C770 != 0U && ioField24 != newValA7C770) {
        ioField24 = newValA7C770;
        outHelper6D17F0Called = true;
    }
}

// 0x006EFE40
std::uint32_t lookupObjectProducerById(
    std::uint32_t producerId3EC,
    std::uint32_t hashLookupResultPtr) {
    if (producerId3EC == 0U) {
        return 0U;
    }
    return hashLookupResultPtr;
}

// 0x006F1350
bool isObjectSelectableByLocalPlayer(
    std::uint32_t flags80,
    std::uint32_t flags84,
    std::uint32_t tmplFlagsC0,
    std::uint8_t globalFlagAC,
    bool hasProducer,
    bool hasProducerTeam,
    std::uint32_t producerTeamFlagsAC,
    bool hasAI374,
    bool hasHelper6A6210,
    bool producerTeamB8Disallows,
    bool hasControllingPlayer,
    bool ctrlPlayerB0,
    std::int32_t ctrlPlayer10CCount,
    bool hasSub37C,
    bool sub37CHelper6A19E0,
    bool hasMod687880,
    bool mod687880Vtable1C) {
    if (((flags80 >> 5U) & 1U) != 0U) {
        return false;
    }
    if (hasProducer && hasProducerTeam && ((flags84 >> 7U) & 1U) == 0U) {
        if (((producerTeamFlagsAC >> 1U) & 1U) == 0U) {
            return false;
        }
        if (hasAI374 && hasHelper6A6210 && producerTeamB8Disallows) {
            return false;
        }
    }
    if (((flags80 >> 2U) & 1U) != 0U || ((flags80 >> 21U) & 1U) != 0U) {
        return false;
    }
    if (((tmplFlagsC0 >> 14U) & 1U) != 0U &&
        ((tmplFlagsC0 >> 1U) & 1U) != 0U &&
        globalFlagAC == 0U) {
        return false;
    }
    if (((tmplFlagsC0 >> 5U) & 1U) != 0U || ((flags80 >> 1U) & 1U) != 0U) {
        return true;
    }
    if (hasControllingPlayer && ctrlPlayerB0 && ctrlPlayer10CCount > 0) {
        return true;
    }
    if (hasSub37C && hasAI374 && sub37CHelper6A19E0) {
        return true;
    }
    return hasMod687880 && mod687880Vtable1C;
}

// 0x0072CF30
bool applyBehaviorDrawableTintEffect(
    bool hasOwner,
    bool hasDrawableF4,
    const float tintVec[3],
    float outAppliedTint[3],
    std::uint32_t& outCommandId) {
    if (!hasOwner || !hasDrawableF4) {
        outAppliedTint[0] = 0.0f;
        outAppliedTint[1] = 0.0f;
        outAppliedTint[2] = 0.0f;
        outCommandId = 0U;
        return false;
    }
    outAppliedTint[0] = tintVec[0];
    outAppliedTint[1] = tintVec[1];
    outAppliedTint[2] = tintVec[2];
    outCommandId = 0x1BU;
    return true;
}

// 0x00731530
void registerActiveModuleAndAppendListener(
    std::uint8_t& isRegistered138,
    std::uint32_t& globalActiveCount,
    std::uint32_t* listenerArray,
    std::uint32_t& listenerCount,
    std::uint32_t listenerCapacity,
    std::uint32_t listenerPtr,
    std::uint8_t& listenerFlag65,
    std::uint32_t listenerType0C,
    std::uint32_t listenerSubFlagsE0,
    std::int32_t& playerCounter1318) {
    if (isRegistered138 == 0U) {
        ++globalActiveCount;
        isRegistered138 = 1U;
    }
    if (listenerArray != nullptr && listenerCount < listenerCapacity) {
        listenerArray[listenerCount] = listenerPtr;
        ++listenerCount;
    }
    listenerFlag65 = 1U;
    if (listenerType0C == 1U && ((listenerSubFlagsE0 >> 10U) & 1U) != 0U) {
        --playerCounter1318;
    }
}

// 0x00734490
bool gatherActiveWeaponSlots(
    std::int8_t activeCountA4,
    const std::uint32_t slotPtrs[6],
    std::uint32_t* outVector,
    std::uint32_t& outCount,
    std::uint32_t maxCapacity) {
    if (activeCountA4 <= 0) {
        return false;
    }
    for (std::size_t i = 0; i < 6; ++i) {
        if (slotPtrs[i] != 0U && outVector != nullptr && outCount < maxCapacity) {
            outVector[outCount] = slotPtrs[i];
            ++outCount;
        }
    }
    return true;
}

// 0x00741140
std::uint32_t ensureObjectWeaponModifierCache(
    std::uint32_t selfPtr,
    const WeaponModifierPair* incomingPairs,
    std::size_t incomingCount,
    WeaponModifierPair* cacheBuffer,
    std::uint32_t& cacheCount,
    std::uint32_t cacheCapacity) {
    if (cacheCount == 0U && incomingPairs != nullptr && cacheBuffer != nullptr) {
        for (std::size_t i = 0; i < incomingCount; ++i) {
            if (incomingPairs[i].itemPtr != 0U && cacheCount < cacheCapacity) {
                cacheBuffer[cacheCount] = incomingPairs[i];
                ++cacheCount;
            }
        }
    }
    return selfPtr + 0x230U;
}

// 0x007451E0
void notifySixBucketObjectListeners(
    const std::uint32_t* const bucketArrays[6],
    const std::size_t bucketCounts[6],
    std::uint32_t* outNotifiedObjects,
    std::uint32_t& outNotifiedCount,
    std::uint32_t maxOutput) {
    outNotifiedCount = 0U;
    for (std::size_t b = 0; b < 6; ++b) {
        const std::uint32_t* arr = bucketArrays[b];
        const std::size_t cnt = bucketCounts[b];
        if (arr == nullptr) {
            continue;
        }
        for (std::size_t i = 0; i < cnt; ++i) {
            if (outNotifiedObjects != nullptr && outNotifiedCount < maxOutput) {
                outNotifiedObjects[outNotifiedCount] = arr[i];
            }
            ++outNotifiedCount;
        }
    }
}

// 0x007461C0
bool updateBehaviorTransformMatrix(
    std::uint32_t flags40,
    std::uint8_t flagA2,
    float float38,
    const float srcMatrix[12],
    float dstMatrix[12]) {
    if (((flags40 >> 7U) & 1U) == 0U && flagA2 != 0U && float38 != 0.0f) {
        return false;
    }
    for (std::size_t i = 0; i < 12; ++i) {
        dstMatrix[i] = srcMatrix[i];
    }
    return true;
}

// 0x00747920
void transferObjectTeamListMembership(
    std::uint32_t oldTeamPtr,
    std::uint32_t newTeamPtr,
    std::uint32_t tmplFlagsE0,
    std::uint32_t oldPlayerPtr,
    std::uint32_t newPlayerPtr,
    bool oldPlayerHasList20,
    bool oldPlayerContainsObj,
    bool newPlayerHasList20,
    bool newPlayerContainsObj,
    std::int32_t& oldPlayerCounter1318,
    std::int32_t& newPlayerCounter1318,
    bool& outRemovedFromOld,
    bool& outAddedToNew,
    bool& outCalled736890) {
    outRemovedFromOld = false;
    outAddedToNew = false;
    if (oldTeamPtr != newTeamPtr && ((tmplFlagsE0 >> 10U) & 1U) != 0U) {
        const bool samePlayer =
            (oldTeamPtr != 0U && newTeamPtr != 0U && oldPlayerPtr == newPlayerPtr);
        if (!samePlayer) {
            if (oldTeamPtr != 0U && oldPlayerHasList20 && oldPlayerContainsObj) {
                --oldPlayerCounter1318;
                outRemovedFromOld = true;
            }
            if (newTeamPtr != 0U && newPlayerHasList20 && !newPlayerContainsObj) {
                ++newPlayerCounter1318;
                outAddedToNew = true;
            }
        }
    }
    outCalled736890 = true;
}

// 0x00747F10
bool checkAndHandleObjectAIIdleState(
    bool hasAI374,
    bool aiVtable214Busy,
    std::uint8_t flags471,
    std::uint32_t flags80,
    std::uint8_t allowBit30,
    bool hasSub378,
    std::uint8_t sub378FlagE0,
    bool helper638B70Ok) {
    if (hasAI374 && aiVtable214Busy) {
        return false;
    }
    if ((flags471 & 0x01U) != 0U) {
        return false;
    }
    if (((flags80 >> 30U) & 1U) != 0U && allowBit30 == 0U) {
        return false;
    }
    if (hasSub378 && sub378FlagE0 != 0U) {
        return false;
    }
    return helper638B70Ok;
}

}  // namespace ra3::gamelogic
