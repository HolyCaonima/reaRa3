#include <ra3/gamelogic/ObjectCore6A.h>

#include <cmath>
#include <cstring>

namespace ra3::gamelogic {

// ============================================================================
// Part A: WeaponSetUpdate (Object + 0x37C, 0x006A15C0 - 0x006A1A50) - 15 functions
// ============================================================================

// 0x006A15C0 (128B)
bool weaponSetHasEntryKey(
    const WeaponSetEntryKey* begin, const WeaponSetEntryKey* end,
    const WeaponSetEntryKey& key) noexcept
{
    if (begin == nullptr || end == nullptr || begin >= end) {
        return false;
    }
    const std::ptrdiff_t count = end - begin;
    for (std::ptrdiff_t i = 0; i < count; ++i) {
        const WeaponSetEntryKey& entry = begin[i];
        if (entry.kindOrTag != key.kindOrTag) {
            continue;
        }
        // ucomiss + lahf + test ah, 0x44 + jp (not equal or unordered)
        if (!(entry.x == key.x)) {
            continue;
        }
        if (!(entry.y == key.y)) {
            continue;
        }
        if (entry.z == key.z) {
            return true;
        }
    }
    return false;
}

// 0x006A1640 (92B)
int weaponSetRebuildMaskAndNotify(
    std::uint32_t& outMask34, const std::uint32_t* elemFlags4, std::size_t elemCount,
    bool hasOwnerSub64, int& outElemCalls, int& outSub64Calls) noexcept
{
    outMask34 = 0u;
    outElemCalls = 0;
    outSub64Calls = 0;
    for (std::size_t i = 0; i < elemCount; ++i) {
        outMask34 |= elemFlags4[i];
        ++outElemCalls;
    }
    if (hasOwnerSub64) {
        ++outSub64Calls;
    }
    return 1;
}

// 0x006A16A0 (35B)
int weaponSetNotifyAllSlots1C(std::size_t slotCount) noexcept
{
    return static_cast<int>(slotCount);
}

// 0x006A16D0 (67B)
bool weaponSetInvokeSlot30ByTag(
    const std::int32_t* slotTags, const std::uint8_t* slotResults, std::size_t slotCount,
    std::int32_t tag, int& outCalledIndex) noexcept
{
    outCalledIndex = -1;
    for (std::size_t i = 0; i < slotCount; ++i) {
        if (slotTags[i] == tag) {
            outCalledIndex = static_cast<int>(i);
            return slotResults[i] != 0u;
        }
    }
    return false;
}

// 0x006A1720 (80B)
std::uint32_t weaponSetFindWeapon20ByTag(
    const std::int32_t* slotTags, const std::uint32_t* slotWeapons20, std::size_t slotCount,
    std::int32_t /*arg0*/, std::int32_t tag, int& outCalls) noexcept
{
    outCalls = 0;
    for (std::size_t i = 0; i < slotCount; ++i) {
        if (slotTags[i] == tag) {
            ++outCalls;
            return slotWeapons20[i];
        }
        if (tag == -1) {
            ++outCalls;
            if (slotWeapons20[i] != 0u) {
                return slotWeapons20[i];
            }
        }
    }
    return 0u;
}

// 0x006A1770 (71B)
std::uint32_t weaponSetFindWeapon24ByTag(
    const std::int32_t* slotTags, const std::uint32_t* slotWeapons24, std::size_t slotCount,
    std::int32_t tag, int& outCalls) noexcept
{
    outCalls = 0;
    for (std::size_t i = 0; i < slotCount; ++i) {
        if (slotTags[i] == tag) {
            ++outCalls;
            return slotWeapons24[i];
        }
        if (tag == -1) {
            ++outCalls;
            if (slotWeapons24[i] != 0u) {
                return slotWeapons24[i];
            }
        }
    }
    return 0u;
}

// 0x006A17C0 (47B)
std::uint32_t weaponSetFindActiveWeapon24(
    const std::uint32_t* slotWeapons24, const std::int32_t* weaponField5C,
    std::size_t slotCount) noexcept
{
    for (std::size_t i = 0; i < slotCount; ++i) {
        if (slotWeapons24[i] != 0u && weaponField5C[i] != -1) {
            return slotWeapons24[i];
        }
    }
    return 0u;
}

// 0x006A17F0 (48B)
std::uint32_t weaponSetFindSlotByPositiveTag(
    const std::uint32_t* slotAddrs, const std::int32_t* slotTags, std::size_t slotCount,
    std::int32_t tag) noexcept
{
    if (tag <= 0) {
        return 0u;
    }
    for (std::size_t i = 0; i < slotCount; ++i) {
        if (slotTags[i] == tag) {
            return slotAddrs[i];
        }
    }
    return 0u;
}

// 0x006A1820 (45B)
std::uint32_t weaponSetFindSlotByTag(
    const std::uint32_t* slotAddrs, const std::int32_t* slotTags, std::size_t slotCount,
    std::int32_t tag) noexcept
{
    for (std::size_t i = 0; i < slotCount; ++i) {
        if (slotTags[i] == tag) {
            return slotAddrs[i];
        }
    }
    return 0u;
}

// 0x006A18A0 (62B)
std::int32_t weaponSetQueryFirstReloadState(
    const std::uint32_t* slotWeapons10, const std::uint32_t* weaponSub90,
    const std::int32_t* sub90Result, std::size_t slotCount) noexcept
{
    for (std::size_t i = 0; i < slotCount; ++i) {
        if (slotWeapons10[i] != 0u && weaponSub90[i] != 0u) {
            if (sub90Result[i] != 0) {
                return sub90Result[i];
            }
        }
    }
    return 0;
}

// 0x006A18E0 (73B)
bool weaponSetHasTargetObjectId(
    const std::uint32_t* slotWeapons10, const std::uint32_t* weaponSub90,
    const std::uint32_t* sub90Field24, const std::uint32_t* sub90Field28,
    std::size_t slotCount, std::uint32_t targetId) noexcept
{
    for (std::size_t i = 0; i < slotCount; ++i) {
        if (slotWeapons10[i] != 0u && weaponSub90[i] != 0u) {
            if (sub90Field28[i] == targetId || sub90Field24[i] == targetId) {
                return true;
            }
        }
    }
    return false;
}

// 0x006A1930 (172B)
bool weaponSetDispatchSlot34ByTag(
    const std::int32_t* slotTags, const std::uint8_t* slotResults34, std::size_t slotCount,
    std::int32_t tag, int& outCalledSlotIndex) noexcept
{
    outCalledSlotIndex = -1;
    if (tag == -1) {
        for (std::size_t i = 0; i < slotCount; ++i) {
            outCalledSlotIndex = static_cast<int>(i);
            if (slotResults34[i] != 0u) {
                return true;
            }
        }
        return false;
    }
    int matchedIndex = -1;
    for (std::size_t i = 0; i < slotCount; ++i) {
        if (matchedIndex != -1) {
            break;
        }
        if (slotTags[i] == tag) {
            matchedIndex = static_cast<int>(i);
        }
    }
    if (matchedIndex == -1) {
        return false;
    }
    outCalledSlotIndex = matchedIndex;
    return slotResults34[matchedIndex] != 0u;
}

// 0x006A19E0 (46B)
bool weaponSetHasAnySlotReady2C(
    const std::uint8_t* slotReady2C, std::size_t slotCount) noexcept
{
    for (std::size_t i = 0; i < slotCount; ++i) {
        if (slotReady2C[i] != 0u) {
            return true;
        }
    }
    return false;
}

// 0x006A1A10 (62B)
void weaponSetUpdateAllSlotSub38Targets(
    const std::uint32_t* slotSub38, std::size_t slotCount, std::uint32_t argAddr,
    std::uint32_t argObjectId, std::uint32_t* subField24, std::uint32_t* subField28,
    std::uint32_t* subField2C, std::uint32_t* subField30, std::uint32_t* subField38,
    std::int32_t* subField70) noexcept
{
    for (std::size_t i = 0; i < slotCount; ++i) {
        if (slotSub38[i] == 0u) {
            continue;
        }
        if (argAddr != 0u) {
            // 0x007DCAA0(sub, arg)
            if (subField28[i] != argObjectId) {
                subField70[i] = 0;
                subField28[i] = argObjectId;
                subField30[i] = argObjectId;
                subField24[i] = 0u;
                subField38[i] = 0u;
            }
        } else {
            // 0x007DF660(sub)
            subField70[i] = -1;
            subField28[i] = 0u;
            subField24[i] = 0u;
            subField2C[i] = 0u;
        }
    }
}

// 0x006A1A50 (46B)
void weaponSetClearAllSlotSub38Field30(
    const std::uint32_t* slotSub38, std::size_t slotCount, std::uint32_t* subField30) noexcept
{
    for (std::size_t i = 0; i < slotCount; ++i) {
        if (slotSub38[i] != 0u) {
            subField30[i] = 0u;
        }
    }
}

// ============================================================================
// Part B: ExperienceTracker / ExperienceLevelTemplate (0x006A3B80 - 0x006A4180) - 6 functions
// ============================================================================

// 0x006A3B80 (69B)
bool experienceVectorFindObjectId(
    bool hasVector, const std::uint32_t* ids, std::size_t count,
    std::uint32_t objectId, std::uint32_t& outIndex) noexcept
{
    if (!hasVector) {
        return false;
    }
    outIndex = 0u;
    for (std::size_t i = 0; i < count; ++i) {
        if (ids[i] == objectId) {
            return true;
        }
        ++outIndex;
    }
    return false;
}

// 0x006A3BD0 (24B)
std::uint32_t experienceVectorFirstId(
    bool hasVector, const std::uint32_t* ids, std::size_t count) noexcept
{
    if (!hasVector || count == 0u || ids == nullptr) {
        return 0u;
    }
    return ids[0];
}

// 0x006A3BF0 (57B)
float experienceTemplateLookupScale(
    float defaultScale, const ExperienceScaleEntry* entries, std::size_t count,
    std::int32_t key) noexcept
{
    for (std::size_t i = 0; i < count; ++i) {
        if (entries[i].key == key) {
            return entries[i].scale;
        }
    }
    return defaultScale;
}

// 0x006A3C30 (22B)
float experienceTrackerLookupScaleOrOne(
    bool hasTracker, float defaultScale, const ExperienceScaleEntry* entries,
    std::size_t count, std::int32_t key) noexcept
{
    if (!hasTracker) {
        return 1.0f;
    }
    return experienceTemplateLookupScale(defaultScale, entries, count, key);
}

// 0x006A4130 (68B)
bool experienceShouldSyncContainedStateBit10(
    std::uint32_t templateFlags14, bool hasObjA, bool hasObjB,
    std::uint32_t containerA, std::uint32_t containerB,
    std::uint32_t& outSrcAddr244, std::uint32_t& outDstContainer) noexcept
{
    outSrcAddr244 = 0u;
    outDstContainer = 0u;
    if (((templateFlags14 >> 10u) & 1u) != 0u) {
        return false;
    }
    if (!hasObjA || !hasObjB || containerA == 0u || containerB == 0u) {
        return false;
    }
    outSrcAddr244 = containerB + 0x244u;
    outDstContainer = containerA;
    return true;
}

// 0x006A4180 (65B)
void experienceSyncContainedVisionRangeBit11(
    std::uint32_t templateFlags14, bool hasObjA, bool hasObjB,
    bool hasContainerA, bool hasContainerB,
    float& containerAVision25C, float containerBVision25C) noexcept
{
    if (((templateFlags14 >> 11u) & 1u) != 0u) {
        return;
    }
    if (!hasObjA || !hasObjB || !hasContainerA || !hasContainerB) {
        return;
    }
    containerAVision25C = containerBVision25C;
}

// ============================================================================
// Part C: StructureUnpackUpdate + Object Core (0x006A0BF0 - 0x006ABCE0) - 47 functions
// ============================================================================

// 0x006A0BF0 (129B)
void structureUnpackStart(
    std::uint8_t& unpackingFlag45, std::uint32_t& startFrame40,
    std::uint32_t& outScheduleFrame, std::uint32_t currentFrame) noexcept
{
    unpackingFlag45 = 1u;
    startFrame40 = currentFrame;
    outScheduleFrame = currentFrame + 0x3FFFFFFFu;
}

// 0x006A0C80 (86B)
void structureUnpackPauseAndAccumulate(
    std::uint8_t& unpackingFlag45, std::uint32_t& startFrame40,
    std::uint32_t& accumulatedFrames3C, std::uint32_t unpackDuration38,
    std::uint32_t currentFrame, std::uint32_t& outScheduleFrame) noexcept
{
    unpackingFlag45 = 0u;
    if (startFrame40 > 0u && startFrame40 < currentFrame) {
        accumulatedFrames3C += (currentFrame - startFrame40);
        startFrame40 = 0u;
    }
    const std::uint32_t remaining =
        (accumulatedFrames3C - currentFrame) + unpackDuration38;
    outScheduleFrame = currentFrame + remaining;
}

// 0x006A4CF0 (44B)
std::uint32_t objectFindSubObjectByMask(
    const std::uint32_t* subObjAddrs, const std::uint32_t* subObjMasks,
    std::size_t count, std::uint32_t mask) noexcept
{
    for (std::size_t i = 0; i < count; ++i) {
        if (subObjAddrs[i] != 0u && (subObjMasks[i] & mask) != 0u) {
            return subObjAddrs[i];
        }
    }
    return 0u;
}

// 0x006A4E70 (43B)
bool objectIsMobileOrHasWaypoints(
    std::uint32_t status84, std::int32_t aiField190) noexcept
{
    if (((status84 >> 21u) & 1u) != 0u) {
        return true;
    }
    if (((status84 >> 19u) & 1u) != 0u) {
        return true;
    }
    return aiField190 > 0;
}

// 0x006A4ED0 (77B)
bool objectCanCrushOrSquishTarget(
    std::uint8_t selfCrusherByte30B, std::uint32_t targetTmplD4,
    std::uint32_t targetTmplDC, bool hasAiUpdate, std::uint32_t aiFlags208) noexcept
{
    if (selfCrusherByte30B == 0u) {
        return false;
    }
    if (((targetTmplD4 >> 20u) & 1u) == 0u) {
        return false;
    }
    if (((targetTmplDC >> 7u) & 1u) == 0u) {
        return false;
    }
    if (hasAiUpdate && (aiFlags208 & 0x200u) == 0u) {
        return false;
    }
    return true;
}

// 0x006A4F20 (55B)
bool objectIsIgnoredByWeaponOrKind(
    std::uint32_t status80, std::uint32_t status98, std::uint32_t weaponFlags1C) noexcept
{
    if (((status80 >> 6u) & 1u) != 0u) {
        return true;
    }
    if (((status98 >> 23u) & 1u) != 0u) {
        if (((weaponFlags1C >> 28u) & 1u) == 0u) {
            return true;
        }
    }
    return false;
}

// 0x006A4F60 (18B)
bool objectHasAnyWeaponReady(
    bool hasWeaponSetUpdate, const std::uint8_t* slotReady2C, std::size_t slotCount) noexcept
{
    if (!hasWeaponSetUpdate) {
        return false;
    }
    return weaponSetHasAnySlotReady2C(slotReady2C, slotCount);
}

// 0x006A4FB0 (117B)
void objectMarkStatusDirtyAndWakeAI(
    std::uint32_t selfAddr, std::uint32_t& globalHeadAddr,
    std::uint32_t& selfPrevLink4EC, std::uint32_t& selfNextLink4F0,
    std::uint32_t& oldHeadPrevLink4EC, bool hasAiUpdate,
    std::uint32_t aiNextWakeFrame14, std::uint8_t aiFlag43B,
    std::uint32_t currentFrame, bool& outScheduledWake,
    std::uint32_t& outWakeFrame) noexcept
{
    outScheduledWake = false;
    outWakeFrame = 0u;
    if (selfPrevLink4EC == 0u) {
        const std::uint32_t oldHead = globalHeadAddr;
        selfNextLink4F0 = oldHead;
        selfPrevLink4EC = 0x00DF8928u;
        if (oldHead != 0u) {
            oldHeadPrevLink4EC = selfAddr + 0x4F0u;
        }
        globalHeadAddr = selfAddr;
    }
    if (hasAiUpdate) {
        if (aiNextWakeFrame14 > currentFrame) {
            const std::int32_t diff = static_cast<std::int32_t>(aiNextWakeFrame14 - currentFrame);
            if (diff > 1 && aiFlag43B == 0u) {
                outScheduledWake = true;
                outWakeFrame = currentFrame + 1u;
            }
        }
    }
}

// 0x006A5030 (36B)
bool objectTestStatusBit(
    const std::uint32_t statusWords[15], std::uint32_t bitIndex) noexcept
{
#ifdef RA3_OBJ6A_ORACLE_MUTANT
    const std::uint32_t mask = 1u << ((bitIndex ^ 1u) & 0x1Fu);
#else
    const std::uint32_t mask = 1u << (bitIndex & 0x1Fu);
#endif
    const std::uint32_t wordIndex = bitIndex >> 5u;
    return (statusWords[wordIndex] & mask) != 0u;
}

// 0x006A5060 (132B)
void objectUpdateStatusBits5060(
    std::uint32_t statusWords[15], const std::uint32_t newWordsAfter522880[15],
    bool hasTeam418, std::int32_t& ioTeamCount125C,
    bool& outCalledMarkDirty) noexcept
{
    outCalledMarkDirty = false;
    std::uint32_t oldWords[15];
    std::memcpy(oldWords, statusWords, sizeof(oldWords));
    std::memcpy(statusWords, newWordsAfter522880, sizeof(oldWords));

    if (std::memcmp(oldWords, statusWords, sizeof(oldWords)) == 0) {
        return;
    }
    outCalledMarkDirty = true;
    const std::uint8_t newBit = static_cast<std::uint8_t>((statusWords[11] >> 10u) & 1u);
    const std::uint8_t oldBit = static_cast<std::uint8_t>((oldWords[11] >> 10u) & 1u);
    if (newBit != oldBit && hasTeam418) {
        const std::int32_t delta = (newBit != 0u) ? 1 : -1;
        ioTeamCount125C += delta;
        if (ioTeamCount125C < 0) {
            ioTeamCount125C = 0;
        }
    }
}

// 0x006A50F0 (161B)
void objectAssignStatusMask50F0(
    std::uint32_t statusWords[15], const std::uint32_t srcWords[15],
    std::uint8_t forceNotify, bool hasTeam418, std::int32_t& ioTeamCount125C,
    bool hasContainerF4, bool& outNotifiedContainer) noexcept
{
    outNotifiedContainer = false;
    std::uint32_t oldWords[15];
    std::memcpy(oldWords, statusWords, sizeof(oldWords));
    std::memcpy(statusWords, srcWords, sizeof(oldWords));

    const bool changed = (std::memcmp(oldWords, statusWords, sizeof(oldWords)) != 0);
    if (!changed) {
        if (forceNotify == 0u) {
            return;
        }
    } else {
        const std::uint8_t newBit = static_cast<std::uint8_t>((statusWords[11] >> 10u) & 1u);
        const std::uint8_t oldBit = static_cast<std::uint8_t>((oldWords[11] >> 10u) & 1u);
        if (newBit != oldBit && hasTeam418) {
            const std::int32_t delta = (newBit != 0u) ? 1 : -1;
            ioTeamCount125C += delta;
            if (ioTeamCount125C < 0) {
                ioTeamCount125C = 0;
            }
        }
    }
    if (hasContainerF4) {
        outNotifiedContainer = true;
    }
}

// 0x006A51C0 (44B)
std::uint32_t objectFindWeaponBySlotOrTag(
    bool hasWeaponSetUpdate, const std::int32_t* slotTags,
    const std::uint32_t* slotWeapons24, const std::uint32_t* slotWeapons20,
    std::size_t slotCount, std::int32_t tag) noexcept
{
    if (!hasWeaponSetUpdate) {
        return 0u;
    }
    int dummyCalls = 0;
    const std::uint32_t w24 = weaponSetFindWeapon24ByTag(
        slotTags, slotWeapons24, slotCount, tag, dummyCalls);
    if (w24 != 0u) {
        return w24;
    }
    return weaponSetFindWeapon20ByTag(
        slotTags, slotWeapons20, slotCount, 0, tag, dummyCalls);
}

// 0x006A51F0 (18B)
std::uint32_t objectGetActiveWeapon(
    bool hasWeaponSetUpdate, const std::uint32_t* slotWeapons24,
    const std::int32_t* weaponField5C, std::size_t slotCount) noexcept
{
    if (!hasWeaponSetUpdate) {
        return 0u;
    }
    return weaponSetFindActiveWeapon24(slotWeapons24, weaponField5C, slotCount);
}

// 0x006A5240 (41B)
bool objectCanInteractWithTarget(
    std::uint32_t status88, bool hasTarget, bool targetVtable4Result) noexcept
{
    if (((status88 >> 9u) & 1u) == 0u) {
        return true;
    }
    if (!hasTarget) {
        return false;
    }
    return targetVtable4Result;
}

// 0x006A5270 (44B)
std::uint32_t objectIsAirborneOrLocomotorActive(
    std::uint32_t status90, bool hasAiUpdate, bool hasLocomotorSub4,
    bool locomotorVtable38Result) noexcept
{
    if (((status90 >> 14u) & 1u) != 0u) {
        return 1u;
    }
    if (hasAiUpdate && hasLocomotorSub4 && locomotorVtable38Result) {
        return 1u;
    }
    return 0u;
}

// 0x006A52A0 (31B)
bool objectIsPhysicsFallingWithKindBit(
    bool hasPhysics, std::int32_t physicsState38, std::uint32_t status98) noexcept
{
    if (!hasPhysics || physicsState38 != 1) {
        return false;
    }
    return ((status98 >> 4u) & 1u) != 0u;
}

// 0x006A52C0 (91B)
bool objectIsCrushableOrPhysicsActive(
    std::uint32_t status84, std::uint32_t status80, std::uint32_t status90,
    std::uint32_t statusB0, std::uint32_t tmplDC,
    bool hasPhysics, std::int32_t physicsState38) noexcept
{
    if (((status84 >> 29u) & 1u) != 0u) {
        return true;
    }
    if (((status80 >> 29u) & 1u) != 0u) {
        return true;
    }
    if (((status90 >> 28u) & 1u) != 0u) {
        return true;
    }
    if (((statusB0 >> 25u) & 1u) != 0u) {
        return true;
    }
    if (((tmplDC >> 3u) & 1u) != 0u) {
        return true;
    }
    if (hasPhysics && physicsState38 == 1) {
        return true;
    }
    return false;
}

// 0x006A5320 (69B)
bool objectHasObjectInList4B8(
    std::uint32_t otherTmplE0, std::uint32_t otherIdDC,
    const std::uint32_t* listBegin, const std::uint32_t* listEnd) noexcept
{
    if (((otherTmplE0 >> 19u) & 1u) == 0u) {
        return false;
    }
    if (listBegin == nullptr || listEnd == nullptr) {
        return false;
    }
    for (const std::uint32_t* it = listBegin; it != listEnd; ++it) {
        if (*it == otherIdDC) {
            return true;
        }
    }
    return false;
}

// 0x006A5450 (114B)
void objectSetProducerId(
    std::uint32_t tmplC8, bool hasTeam418, std::uint8_t flag4D7,
    bool hasProducer, std::uint32_t producerIdDC,
    std::uint32_t& ioFieldE4, bool& outUnregisteredBefore, bool& outRegisteredAfter) noexcept
{
    outUnregisteredBefore = false;
    outRegisteredAfter = false;
    const bool notifyTeam = (((tmplC8 >> 25u) & 1u) == 0u) && hasTeam418 && (flag4D7 != 0u);
    if (notifyTeam) {
        outUnregisteredBefore = true;
    }
    ioFieldE4 = hasProducer ? producerIdDC : 0u;
    if (notifyTeam) {
        outRegisteredAfter = true;
    }
}

// 0x006A5530 (93B)
std::uint32_t objectGetDrawableVisibility5530(
    bool hasDrawable9C, bool playerFlag7E79F0, std::uint32_t tmplC4,
    std::uint32_t playerIndex, const std::uint32_t drawableTable24[20]) noexcept
{
    if (!hasDrawable9C) {
        return 1u;
    }
    if (!playerFlag7E79F0 || ((tmplC4 >> 22u) & 1u) != 0u) {
        return 1u;
    }
    if (playerIndex > 19u) {
        return 4u;
    }
    return drawableTable24[playerIndex];
}

// 0x006A5590 (93B)
std::uint32_t objectGetDrawableVisibility5590(
    bool hasDrawable9C, bool playerFlag7E79F0, std::uint32_t tmplC4,
    std::uint32_t playerIndex, const std::uint32_t drawableTable74[20]) noexcept
{
    if (!hasDrawable9C) {
        return 1u;
    }
    if (!playerFlag7E79F0 || ((tmplC4 >> 22u) & 1u) != 0u) {
        return 1u;
    }
    if (playerIndex > 19u) {
        return 4u;
    }
    return drawableTable74[playerIndex];
}

// 0x006A56F0 (96B)
void objectPropagateDisabledTypeToContainer(
    bool hasContainerF4, std::uint32_t disabledMask26C, std::int32_t disabledType,
    std::uint8_t& ioContainerByte14C) noexcept
{
    if (!hasContainerF4 || disabledMask26C == 0u) {
        return;
    }
    if (disabledType == 3 || disabledType == 9 || disabledType == 4
        || disabledType == 1 || disabledType == 8 || disabledType == 7) {
        return;
    }
    if (disabledType == 5) {
        ioContainerByte14C = static_cast<std::uint8_t>(ioContainerByte14C | 0x40u);
    } else {
        ioContainerByte14C = static_cast<std::uint8_t>(ioContainerByte14C | 0x01u);
    }
}

// 0x006A5750 (66B)
std::uint32_t objectGetDisabledUntilFrame(
    std::uint32_t disabledType, std::uint32_t disabledMask26C,
    const std::uint32_t disabledFrames2D0[12], std::uint32_t currentFrame) noexcept
{
    if (disabledType > 11u) {
        return 0u;
    }
    const std::uint32_t mask = 1u << (disabledType & 0x1Fu);
    if ((disabledMask26C & mask) != 0u) {
        return currentFrame;
    }
    return disabledFrames2D0[disabledType];
}

// 0x006A5810 (124B)
void objectCompactSlotTable450(ObjectSlotTable450* table) noexcept
{
    if (table == nullptr) {
        return;
    }
    const int count = static_cast<int>(table->count);
    int writeIdx = 0;
    for (int readIdx = 0; readIdx < count; ++readIdx) {
        if (table->entries[writeIdx].activeFlag6 != 0u) {
            table->entries[writeIdx].flag4 = 0u;
            table->entries[writeIdx].recentFlag5 = 0u;
            table->entries[writeIdx].activeFlag6 = table->entries[readIdx].activeFlag6;
            table->entries[writeIdx].objectId = table->entries[readIdx].objectId;
            ++writeIdx;
        }
    }
    table->count = static_cast<std::int8_t>(writeIdx);
}

// 0x006A5890 (110B)
void objectRemoveFromSlotTable450(
    ObjectSlotTable450* table, std::uint32_t objectId) noexcept
{
    if (table == nullptr) {
        return;
    }
    const int count = static_cast<int>(table->count);
    for (int i = 0; i < count; ++i) {
        if (table->entries[i].objectId == objectId) {
            table->entries[i].flag4 = 0u;
            table->entries[i].recentFlag5 = 0u;
            table->entries[i].activeFlag6 = 0u;
            table->entries[i].objectId = 0u;
        }
    }
    objectCompactSlotTable450(table);
}

// 0x006A5A60 (66B)
std::uint8_t objectCanExecuteCommand(
    std::uint32_t status80, std::uint32_t status88, bool hasAiUpdate,
    bool aiVtable1FCResult, std::uint8_t commandByte7C) noexcept
{
    if (((status80 >> 8u) & 1u) != 0u) {
        return 0u;
    }
    if (((status88 >> 29u) & 1u) != 0u) {
        return 0u;
    }
    if (hasAiUpdate && !aiVtable1FCResult) {
        return 0u;
    }
    return commandByte7C;
}

// 0x006A5B10 (88B)
bool objectHasRecentFlag5InSlotTable450(
    const ObjectSlotTable450* table, std::uint32_t currentFrame,
    std::uint32_t objectId) noexcept
{
    if (table == nullptr) {
        return false;
    }
    if (table->lastUpdateFrame != currentFrame
        && table->lastUpdateFrame != (currentFrame - 1u)) {
        return false;
    }
    const int count = static_cast<int>(table->count);
    for (int i = 0; i < count; ++i) {
        if (table->entries[i].recentFlag5 != 0u && table->entries[i].objectId == objectId) {
            return true;
        }
    }
    return false;
}

// 0x006A5B70 (66B)
bool objectHasActiveEntryInSlotTable450(
    const ObjectSlotTable450* table, std::uint32_t objectId) noexcept
{
    if (table == nullptr) {
        return false;
    }
    const int count = static_cast<int>(table->count);
    for (int i = 0; i < count; ++i) {
        if (table->entries[i].activeFlag6 != 0u && table->entries[i].objectId == objectId) {
            return true;
        }
    }
    return false;
}

// 0x006A5BC0 (48B)
float objectGetHeightAboveTerrain(
    float /*posX*/, float /*posY*/, float posZ, float terrainLayerHeight) noexcept
{
#if defined(_MSC_VER) && defined(_M_IX86)
    float result = 0.0f;
    __asm {
        fld terrainLayerHeight
        fsubr posZ
        fstp result
    }
    return result;
#else
    return posZ - terrainLayerHeight;
#endif
}

// 0x006A5F50 (78B)
bool objectIsSelectable(
    std::uint32_t tmplC4, std::uint32_t tmplC8, std::uint8_t tmplSub14Byte24B,
    std::uint32_t status80, std::uint8_t objFlags471, std::uint8_t objSelectable4D2) noexcept
{
    if (((tmplC4 >> 27u) & 1u) != 0u) {
        return true;
    }
    if (((status80 >> 3u) & 1u) != 0u) {
        return false;
    }
    if (tmplSub14Byte24B == 0u && (objFlags471 & 1u) != 0u) {
        return false;
    }
    if (((tmplC8 >> 9u) & 1u) != 0u) {
        return false;
    }
    return objSelectable4D2 != 0u;
}

// 0x006A60A0 (163B)
void objectSwapPrimaryBodyModules(
    std::uint32_t selfAddr, std::uint32_t otherAddr,
    std::uint32_t* selfModules310, const std::uint32_t* selfModTags,
    std::uint32_t& selfBody328, std::uint32_t& selfBodySlot32C,
    std::uint32_t* otherModules310, const std::uint32_t* otherModTags,
    std::uint32_t& otherBody328, std::uint32_t& otherBodySlot32C,
    std::uint32_t& outNewSelfBodyOwner, std::uint32_t& outNewOtherBodyOwner) noexcept
{
    std::size_t selfIdx = 0;
    while (selfModules310[selfIdx] != 0u) {
        if (selfModTags[selfIdx] == selfBody328) {
            break;
        }
        ++selfIdx;
    }
    std::size_t otherIdx = 0;
    if (otherModules310[0] == 0u) {
        return;
    }
    while (otherModules310[otherIdx] != 0u) {
        if (otherModTags[otherIdx] == otherBody328) {
            break;
        }
        ++otherIdx;
    }
    const std::uint32_t otherMod = otherModules310[otherIdx];
    if (otherMod == 0u) {
        return;
    }
    const std::uint32_t selfMod = selfModules310[selfIdx];
    selfModules310[selfIdx] = otherMod;
    otherModules310[otherIdx] = selfMod;

    const std::uint32_t oldOtherBody = otherBody328;
    const std::uint32_t oldSelfBody = selfBody328;
    selfBodySlot32C = oldOtherBody;
    selfBody328 = oldOtherBody;
    otherBodySlot32C = oldSelfBody;
    otherBody328 = oldSelfBody;

    outNewSelfBodyOwner = selfAddr;
    outNewOtherBodyOwner = otherAddr;
}

// 0x006A6210 (64B)
std::int32_t objectGetReloadTimeOrWeaponState(
    std::uint8_t flags471, bool hasAiUpdate, bool hasAiSub47C,
    std::int32_t aiSub47CResult, bool hasWeaponSetUpdate,
    const std::uint32_t* slotWeapons10, const std::uint32_t* weaponSub90,
    const std::int32_t* sub90Result, std::size_t slotCount) noexcept
{
    if ((flags471 & 1u) != 0u) {
        return 0;
    }
    if (hasAiUpdate && hasAiSub47C && aiSub47CResult != 0) {
        return aiSub47CResult;
    }
    if (hasWeaponSetUpdate) {
        return weaponSetQueryFirstReloadState(
            slotWeapons10, weaponSub90, sub90Result, slotCount);
    }
    return 0;
}

// 0x006A6250 (47B)
void objectClearWeaponTargetField30(
    bool hasAiUpdate, bool hasAiSub47C, std::uint32_t& aiSub47CField30,
    bool hasWeaponSetUpdate, const std::uint32_t* slotSub38, std::size_t slotCount,
    std::uint32_t* subField30) noexcept
{
    if (hasAiUpdate && hasAiSub47C) {
        aiSub47CField30 = 0u;
    }
    if (hasWeaponSetUpdate) {
        weaponSetClearAllSlotSub38Field30(slotSub38, slotCount, subField30);
    }
}

// 0x006A6280 (52B)
std::uint32_t objectIsUnderConstructionOrUpgrading(
    std::uint32_t tmplD4, std::uint32_t statusB0) noexcept
{
    if (((tmplD4 >> 14u) & 1u) == 0u) {
        return 0u;
    }
    if (((statusB0 >> 30u) & 1u) != 0u || ((statusB0 >> 31u) & 1u) != 0u) {
        return 1u;
    }
    return 0u;
}

// 0x006A6510 (196B)
float objectComputeVisionRange(
    float baseVision25C, float visionScale258, bool hasModifier11, float modifier11Scale,
    float posZ40, float groundZ268, float tmplHeightFactor2D8,
    float tmplMaxBonus2DC, float tmplMinBonus2E0) noexcept
{
    float vision = baseVision25C * visionScale258;
    if (hasModifier11) {
        vision = modifier11Scale * vision;
    }
    if (tmplHeightFactor2D8 > 0.0f) {
        float bonus = (posZ40 - groundZ268) * tmplHeightFactor2D8;
        if (bonus > tmplMaxBonus2DC) {
            bonus = tmplMaxBonus2DC;
        }
        if (tmplMinBonus2E0 > bonus) {
            bonus = tmplMinBonus2E0;
        }
        vision = (bonus + 1.0f) * vision;
    }
    return vision;
}

// 0x006A65E0 (214B)
float objectComputeShroudClearingRange(
    float baseShroud260, bool hasModifier15, float modifier15Scale,
    float posZ40, float groundZ268, float tmplHeightFactor2D8,
    float tmplMaxBonus2DC, float tmplMinBonus2E0,
    std::uint32_t tmplDC, std::uint32_t status94) noexcept
{
    float shroud = baseShroud260;
    if (hasModifier15) {
        shroud = modifier15Scale * shroud;
    }
    if (tmplHeightFactor2D8 > 0.0f) {
        float bonus = (posZ40 - groundZ268) * tmplHeightFactor2D8;
        if (bonus > tmplMaxBonus2DC) {
            bonus = tmplMaxBonus2DC;
        }
        if (tmplMinBonus2E0 > bonus) {
            bonus = tmplMinBonus2E0;
        }
        shroud = (bonus + 1.0f) * shroud;
    }
    if (((tmplDC >> 9u) & 1u) != 0u && ((status94 >> 2u) & 1u) != 0u) {
        return 0.0f;
    }
    return shroud;
}

// 0x006A6770 (54B)
void objectGetFormationOrTemplateValue464(
    std::uint32_t field464, std::uint32_t fallback94, std::uint32_t& outValue) noexcept
{
    outValue = (field464 != 0u) ? field464 : fallback94;
}

// 0x006A68E0 (107B)
std::int32_t objectGetRadarOrVeterancyPriority(
    bool hasSub3AC, std::int32_t sub3ACField20, std::uint32_t tmplD4,
    std::uint8_t flags471, std::uint8_t tmplByteC7, bool hasPlayer,
    bool playerHasOverride1F8, bool playerHasOverride1FC) noexcept
{
    if (!hasSub3AC) {
        return -1;
    }
    if (((tmplD4 >> 20u) & 1u) != 0u) {
        return 20;
    }
    if ((flags471 & 2u) != 0u || (tmplByteC7 & 1u) != 0u) {
        return -1;
    }
    if (!hasPlayer || playerHasOverride1F8 || playerHasOverride1FC) {
        return -1;
    }
    return sub3ACField20;
}

// 0x006A6B10 (219B)
std::uint32_t objectComputePackedStateFlags6B10(
    std::uint32_t statusC0, std::uint32_t statusC4, std::uint32_t statusD0,
    std::uint32_t statusD4, std::uint32_t statusD8) noexcept
{
    std::uint16_t edxVal = 0u;
    std::uint16_t eaxVal = 0u;

    if (((statusD4 >> 20u) & 1u) != 0u) {
        edxVal = 1u;
        eaxVal = 11u;
    } else if (((statusC0 >> 4u) & 1u) != 0u) {
        edxVal = 2u;
        eaxVal = 3u;
        if (((statusD0 >> 3u) & 1u) != 0u || ((statusD0 >> 31u) & 1u) != 0u) {
            edxVal = 0x42u;
            eaxVal = 0x23u;
        } else if (((statusC4 >> 4u) & 1u) != 0u) {
            edxVal = 0x82u;
            eaxVal = 0x13u;
        }
    } else if (((statusC0 >> 10u) & 1u) != 0u) {
        edxVal = 8u;
        eaxVal = 1u;
    } else if (((statusD8 >> 12u) & 1u) != 0u || ((statusC0 >> 26u) & 1u) != 0u) {
        if (((statusC0 >> 17u) & 1u) != 0u) {
            edxVal = 0x24u;
            eaxVal = 0x40u;
        } else {
            edxVal = 4u;
            eaxVal = 0u;
        }
    }

    if (((statusC0 >> 14u) & 1u) != 0u) {
        edxVal = static_cast<std::uint16_t>(edxVal | 0x10u);
        eaxVal = static_cast<std::uint16_t>(eaxVal | 0x80u);
    }

    return (static_cast<std::uint32_t>(eaxVal) << 16u) | static_cast<std::uint32_t>(edxVal);
}

// 0x006A6C10 (99B)
int objectDispatchCollisionModules2A0(
    std::uint32_t targetStatus80, const std::uint32_t* selfFlags0CPerStep,
    std::size_t moduleCount) noexcept
{
    if (((targetStatus80 >> 4u) & 1u) != 0u) {
        return 0;
    }
    int calls = 0;
    for (std::size_t i = 0; i < moduleCount; ++i) {
        if (((selfFlags0CPerStep[i] >> 4u) & 1u) != 0u) {
            break;
        }
        ++calls;
    }
    return calls;
}

// 0x006A6C80 (42B)
void objectGetTransformTranslation(
    std::uint8_t useAltTransform234, const float primaryMatrix3x4[12],
    const float altMatrix3x4[12], Vec3f6A& outPos) noexcept
{
    const float* m = (useAltTransform234 != 0u) ? altMatrix3x4 : primaryMatrix3x4;
    outPos.x = m[3];
    outPos.y = m[7];
    outPos.z = m[11];
}

// 0x006A71A0 (29B)
bool objectHasTemplateBitCBAndStatusC8Bit9(
    std::uint8_t tmplByteCB, std::uint32_t statusC8) noexcept
{
    if ((tmplByteCB & 1u) == 0u) {
        return false;
    }
    return ((statusC8 >> 9u) & 1u) != 0u;
}

// 0x006A72A0 (246B)
void objectComputeTransformedOffsetPosition(
    std::uint8_t useRawPos114, const Vec3f6A& rawPos38,
    const float matrix3x4_08[12], const Vec3f6A& localOffset128,
    Vec3f6A& outPos) noexcept
{
    if (useRawPos114 != 0u) {
        outPos = rawPos38;
        return;
    }
    const float ox = localOffset128.x;
    const float oy = localOffset128.y;
    const float oz = localOffset128.z;

    outPos.x = ((matrix3x4_08[1] * oy + matrix3x4_08[2] * oz) + matrix3x4_08[0] * ox)
        + matrix3x4_08[3];
    outPos.y = ((matrix3x4_08[4] * ox + matrix3x4_08[5] * oy) + matrix3x4_08[6] * oz)
        + matrix3x4_08[7];
    outPos.z = ((matrix3x4_08[8] * ox + matrix3x4_08[9] * oy) + matrix3x4_08[10] * oz)
        + matrix3x4_08[11];
}

// 0x006A73A0 (63B)
std::uint32_t objectFindModuleById310(
    const std::uint32_t* moduleAddrs, const std::uint32_t* moduleIds,
    std::size_t count, std::uint32_t targetId) noexcept
{
    for (std::size_t i = 0; i < count; ++i) {
        if (moduleAddrs[i] == 0u) {
            break;
        }
        if (moduleIds[i] == targetId) {
            return moduleAddrs[i];
        }
    }
    return 0u;
}

// 0x006A7510 (35B)
std::int32_t objectGetStatusTriState7510(std::uint32_t statusC8) noexcept
{
    if (((statusC8 >> 11u) & 1u) != 0u) {
        return 0;
    }
    return (((statusC8 >> 12u) & 1u) != 0u) ? 1 : -1;
}

// 0x006A7540 (29B)
bool objectHasStatus8CBit3Or4(std::uint32_t status8C) noexcept
{
    return (((status8C >> 4u) & 1u) != 0u) || (((status8C >> 3u) & 1u) != 0u);
}

// 0x006A7560 (55B)
bool objectHasValidStatusTriState7560(std::uint32_t statusC8) noexcept
{
    return objectGetStatusTriState7510(statusC8) != -1;
}

// 0x006A75A0 (93B)
std::uint32_t objectIsCrushedOrImmobilized75A0(
    std::uint32_t tmplE0, std::uint32_t tmplDC, std::uint32_t statusC0,
    std::uint32_t status90, std::uint32_t status80) noexcept
{
    const bool pairBit = (((tmplE0 >> 19u) & 1u) != 0u) && (((statusC0 >> 5u) & 1u) != 0u);
    if (((status90 >> 28u) & 1u) != 0u) {
        return 1u;
    }
    if (((status80 >> 29u) & 1u) != 0u) {
        return 1u;
    }
    if (((tmplDC >> 3u) & 1u) != 0u) {
        return 1u;
    }
    return pairBit ? 1u : 0u;
}

// 0x006A7620 (122B)
std::uint32_t objectFindBodyOrWeaponModule7620(
    const std::uint32_t* modules, const std::uint8_t* vtable94Match,
    std::size_t count, std::uint32_t primary328) noexcept
{
    for (std::size_t i = 0; i < count; ++i) {
        if (modules[i] == primary328) {
            continue;
        }
        if (vtable94Match[i] != 0u) {
            return modules[i];
        }
    }
    if (primary328 != 0u) {
        return primary328;
    }
    if (count > 0u) {
        return modules[0];
    }
    return 0u;
}

// 0x006A7780 (108B)
int objectFlushDirtyStatusList7780(
    std::uint32_t& ioGlobalHead, std::uint32_t* nodeAddrs,
    std::uint32_t* nodePrevLink4EC, std::uint32_t* nodeNextLink4F0,
    const std::uint8_t* nodeHasContainerF4, std::size_t nodeCount,
    int& outContainerNotifications) noexcept
{
    outContainerNotifications = 0;
    int processed = 0;
    while (ioGlobalHead != 0u) {
        int idx = -1;
        for (std::size_t i = 0; i < nodeCount; ++i) {
            if (nodeAddrs[i] == ioGlobalHead) {
                idx = static_cast<int>(i);
                break;
            }
        }
        if (idx < 0 || nodePrevLink4EC[idx] == 0u) {
            break;
        }
        const std::uint32_t nextAddr = nodeNextLink4F0[idx];
        const std::uint32_t prevLink = nodePrevLink4EC[idx];
        if (nextAddr != 0u) {
            for (std::size_t i = 0; i < nodeCount; ++i) {
                if (nodeAddrs[i] == nextAddr) {
                    nodePrevLink4EC[i] = prevLink;
                    break;
                }
            }
        }
        if (prevLink == 0x00DF8928u) {
            ioGlobalHead = nextAddr;
        }
        nodePrevLink4EC[idx] = 0u;
        nodeNextLink4F0[idx] = 0u;
        if (nodeHasContainerF4[idx] != 0u) {
            ++outContainerNotifications;
        }
        ++processed;
    }
    return processed;
}

// 0x006A78D0 (32B)
bool objectIsOwnedByLocalPlayerShifted78(
    std::uint32_t shiftedField54, std::uint32_t shiftedField3A0,
    std::uint32_t activePlayerField2C) noexcept
{
    if (((shiftedField54 >> 10u) & 1u) != 0u) {
        return false;
    }
    return shiftedField3A0 == activePlayerField2C;
}

// 0x006A7900 (40B)
std::int32_t objectQueryModuleValue158Shifted78(
    const std::uint32_t* modules, const std::uint8_t* vtable94Match,
    const std::int32_t* vtable158Value, std::size_t count,
    std::uint32_t primary328) noexcept
{
    const std::uint32_t found = objectFindBodyOrWeaponModule7620(
        modules, vtable94Match, count, primary328);
    if (found == 0u) {
        return -1;
    }
    for (std::size_t i = 0; i < count; ++i) {
        if (modules[i] == found) {
            return vtable158Value[i];
        }
    }
    return vtable158Value[0];
}

// 0x006A7930 (61B)
std::uint32_t objectMatchesGlobalStatusMaskOrBitB4(
    const std::uint32_t status80To98[7], const std::uint32_t globalMasks[7],
    std::uint32_t statusB4) noexcept
{
    for (std::size_t i = 0; i < 7u; ++i) {
        if ((status80To98[i] & globalMasks[i]) != 0u) {
            return 1u;
        }
    }
    if (((statusB4 >> 5u) & 1u) != 0u) {
        return 1u;
    }
    return 0u;
}

// 0x006A7970 (102B)
std::uint32_t objectIsTargetBehind2D(
    float selfPosX38, float selfPosY3C, float mat08, float mat18, float mat28,
    float targetX, float targetY) noexcept
{
    const float dx = selfPosX38 - targetX;
    const float dy = selfPosY3C - targetY;

    // 0x0078BFB0 normalizes (mat08, mat18, mat28)
    const float lenSq = (mat08 * mat08 + mat18 * mat18) + mat28 * mat28;
    float invLen = 0.0f;
    if (lenSq != 0.0f) {
#if defined(_MSC_VER) && defined(_M_IX86)
        __asm {
            fld lenSq
            fsqrt
            fld1
            fdivrp st(1), st(0)
            fstp invLen
        }
#else
        invLen = 1.0f / std::sqrt(lenSq);
#endif
    }
    const float nx = mat08 * invLen;
    const float ny = mat18 * invLen;
    const float dot = nx * dx + ny * dy;
    return (0.0f > dot) ? 1u : 0u;
}

// 0x006ABC00 (173B)
void objectTransitionStatusBitBC00(
    std::uint32_t statusWords[15], std::uint32_t clearBit, std::uint32_t setBit,
    bool hasTeam418, std::int32_t& ioTeamCount125C,
    bool& outCalledMarkDirty) noexcept
{
    outCalledMarkDirty = false;
    const std::uint32_t clearMask = 1u << (clearBit & 0x1Fu);
    const std::uint32_t clearWord = clearBit >> 5u;
    const bool hadClearBit = (statusWords[clearWord] & clearMask) != 0u;

    const std::uint32_t setMask = 1u << (setBit & 0x1Fu);
    const std::uint32_t setWord = setBit >> 5u;
    const bool lackedSetBit = (statusWords[setWord] & setMask) == 0u;

    if (!hadClearBit && !lackedSetBit) {
        return;
    }

    statusWords[clearWord] &= ~clearMask;
    statusWords[setWord] |= setMask;

    if (hadClearBit && clearBit == 0x16Au && hasTeam418) {
        ioTeamCount125C -= 1;
        if (ioTeamCount125C < 0) {
            ioTeamCount125C = 0;
        }
    }
    if (lackedSetBit && setBit == 0x16Au && hasTeam418) {
        ioTeamCount125C += 1;
        if (ioTeamCount125C < 0) {
            ioTeamCount125C = 0;
        }
    }
    outCalledMarkDirty = true;
}

// 0x006ABCE0 (37B)
float objectGetDistanceSquaredToObject(
    const Vec3f6A& selfPos38, const Vec3f6A& otherPos38) noexcept
{
#if defined(_MSC_VER) && defined(_M_IX86)
    float result = 0.0f;
    __asm {
        mov ecx, selfPos38
        mov eax, otherPos38
        fld dword ptr [ecx]
        fsub dword ptr [eax]
        fld dword ptr [ecx + 4]
        fsub dword ptr [eax + 4]
        fld dword ptr [ecx + 8]
        fsub dword ptr [eax + 8]
        fld st(2)
        fmulp st(3), st(0)
        fmul st(0), st(0)
        faddp st(2), st(0)
        fmul st(0), st(0)
        faddp st(1), st(0)
        fstp result
    }
    return result;
#else
    const float dx = selfPos38.x - otherPos38.x;
    const float dy = selfPos38.y - otherPos38.y;
    const float dz = selfPos38.z - otherPos38.z;
    return (dx * dx + dz * dz) + dy * dy;
#endif
}

} // namespace ra3::gamelogic
