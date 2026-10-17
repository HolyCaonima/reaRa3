#include "ra3/gamelogic/ProtectedCaveLogic3.h"

#include <cstring>

namespace ra3::gamelogic {
namespace {

inline std::uint8_t readU8(const void* base, std::ptrdiff_t offset) noexcept {
    return *reinterpret_cast<const std::uint8_t*>(
        static_cast<const std::uint8_t*>(base) + offset);
}

inline void writeU8(void* base, std::ptrdiff_t offset, std::uint8_t value) noexcept {
    *reinterpret_cast<std::uint8_t*>(
        static_cast<std::uint8_t*>(base) + offset) = value;
}

inline void writeU16(void* base, std::ptrdiff_t offset, std::uint16_t value) noexcept {
    std::memcpy(static_cast<std::uint8_t*>(base) + offset, &value, sizeof(value));
}

inline std::uint32_t readU32(const void* base, std::ptrdiff_t offset) noexcept {
    std::uint32_t v = 0;
    std::memcpy(&v, static_cast<const std::uint8_t*>(base) + offset, sizeof(v));
    return v;
}

inline std::int32_t readI32(const void* base, std::ptrdiff_t offset) noexcept {
    std::int32_t v = 0;
    std::memcpy(&v, static_cast<const std::uint8_t*>(base) + offset, sizeof(v));
    return v;
}

inline void writeU32(void* base, std::ptrdiff_t offset, std::uint32_t value) noexcept {
    std::memcpy(static_cast<std::uint8_t*>(base) + offset, &value, sizeof(value));
}

inline float readF32(const void* base, std::ptrdiff_t offset) noexcept {
    float v = 0.0f;
    std::memcpy(&v, static_cast<const std::uint8_t*>(base) + offset, sizeof(v));
    return v;
}

inline std::uint32_t floatToBits(float v) noexcept {
    std::uint32_t bits = 0;
    std::memcpy(&bits, &v, sizeof(bits));
    return bits;
}

inline void recordCall(
    CaveCallTrace* trace,
    std::uint32_t targetVa,
    std::uint32_t ecxVal,
    std::uint32_t argCount,
    std::uint32_t a0 = 0,
    std::uint32_t a1 = 0,
    std::uint32_t a2 = 0) noexcept {
    if (trace == nullptr || trace->count >= CaveCallTrace::kMaxEntries) {
        return;
    }
    CaveCallEntry& e = trace->entries[trace->count++];
    e.targetVa = targetVa;
    e.ecx = ecxVal;
    e.argCount = argCount;
    e.arg0 = a0;
    e.arg1 = a1;
    e.arg2 = a2;
}

}  // namespace

// 1. 0x004CB260
std::uint32_t runInitZeroAndCall4CB200(
    void* self,
    std::uint32_t selfVa,
    std::uint32_t arg0,
    CaveCallTrace* trace) noexcept {
    // Cave @ 0x0044DD7F (site 0x004CB268, slot 0x018F2E38 -> 0x004CB26E):
    // mov dword ptr [esi], 0
    writeU32(self, 0x00, 0u);
    recordCall(trace, 0x004CB200u, selfVa, 1u, arg0);
    return selfVa;
}

// 2. 0x004D66D0
void runDestroyObjectBDB8E8(
    void* self,
    std::uint32_t childVa,
    void* child,
    std::uint32_t childDtorTargetVa,
    CaveCallTrace* trace) noexcept {
    // Cave @ 0x00459C86 (site 0x004D66D0, slot 0x018F25C8 -> 0x004D66D6):
    // mov dword ptr [ecx], 0xBDB8E8
    writeU32(self, 0x00, 0x00BDB8E8u);
    if (childVa == 0u || child == nullptr) {
        return;
    }
    const std::int32_t newRef = readI32(child, 0x04) - 1;
    writeU32(child, 0x04, static_cast<std::uint32_t>(newRef));
    if (newRef <= 0) {
        recordCall(trace, childDtorTargetVa, childVa, 1u, 1u);
    }
}

// 3. 0x004D99E0
std::uint32_t runAllocateFromPowerOfTwoPool(
    std::uint32_t selfVa,
    std::uint32_t requestedSize,
    std::uint32_t maxPoolSize,
    std::uint32_t largeAllocReturnVa,
    std::uint32_t poolEntryVa,
    void* poolEntry,
    std::uint32_t growPoolReturnByte,
    std::uint32_t grownHeadNodeVa,
    const void* headNode,
    CaveCallTrace* trace) noexcept {
    // Cave @ 0x0042055A (site 0x004D99E5, slot 0x018ED488 -> 0x004D99EB):
    // cmp esi, dword ptr [0x018F9C38]
    if (requestedSize > maxPoolSize) {
        recordCall(trace, 0x004165A0u, 0u, 3u, requestedSize, 0u, 0u);
        return largeAllocReturnVa;
    }
    recordCall(trace, 0x004D61B0u, selfVa, 0u);

    std::uint32_t rounded = requestedSize;
    if ((requestedSize & (requestedSize - 1u)) != 0u) {
        std::uint32_t v = (requestedSize >> 1u) | requestedSize;
        v |= (v >> 2u);
        v |= (v >> 4u);
        v |= (v >> 8u);
        v |= (v >> 16u);
        rounded = v + 1u;
    }

    recordCall(trace, 0x004D36C0u, selfVa, 1u, rounded);

    std::uint32_t freeHeadVa = readU32(poolEntry, 0x10);
    if (freeHeadVa == 0u) {
        recordCall(trace, 0x00A662A0u, poolEntryVa, 2u, 0u, 0u);
        if ((growPoolReturnByte & 0xFFu) == 0u) {
            return 0u;
        }
        writeU32(poolEntry, 0x10, grownHeadNodeVa);
        freeHeadVa = grownHeadNodeVa;
    }

    const std::uint32_t nextFreeVa = readU32(headNode, 0x00);
    writeU32(poolEntry, 0x10, nextFreeVa);
    return freeHeadVa;
}

// 4. 0x004DA710
std::uint32_t runConstructObjectBDC368(
    void* self,
    std::uint32_t selfVa,
    CaveCallTrace* trace) noexcept {
    // Cave @ 0x0044F45B (site 0x004DA716, slot 0x018F2570 -> 0x004DA71C):
    // mov dword ptr [esi], 0xBDC368
    writeU32(self, 0x00, 0x00BDC368u);
    recordCall(trace, 0x004D6550u, selfVa + 0x04u, 0u);
    writeU32(self, 0x88, 0x00BDB99Cu);
    writeU32(self, 0x8C, 0u);
    writeU32(self, 0x90, 0u);
    writeU8(self, 0x94, 0u);
    writeU32(self, 0x98, 0u);
    return selfVa;
}

// 5. 0x004E5C90
void runClearLists8CAnd94(
    void* self,
    std::uint32_t selfVa,
    std::uint32_t localOutStackVa,
    const ListNode8C94Entry* nodes,
    std::uint32_t nodeCount,
    CaveCallTrace* trace) noexcept {
    auto findNode = [&](std::uint32_t va) noexcept -> const ListNode8C94Entry* {
        for (std::uint32_t i = 0; i < nodeCount; ++i) {
            if (nodes[i].nodeVa == va) {
                return &nodes[i];
            }
        }
        return nullptr;
    };

    // Cave @ 0x004440D2 (site 0x004E5C95, slot 0x018F2BB4 -> 0x004E5C9C):
    // cmp dword ptr [esi + 0x8C], 0
    std::uint32_t cur8C = readU32(self, 0x8C);
    while (cur8C != 0u) {
        const ListNode8C94Entry* entry = findNode(cur8C);
        const std::uint32_t next8C = entry ? readU32(entry->node, 0x14) : 0u;
        recordCall(trace, 0x004166B0u, 0u, 1u, cur8C);
        writeU32(self, 0x8C, next8C);
        cur8C = next8C;
    }

    const std::uint32_t sentinel94Va = selfVa + 0x94u;
    std::uint32_t cur94 = readU32(self, 0x94);
    writeU32(self, 0x90, cur94);
    if (cur94 != sentinel94Va) {
        do {
            const ListNode8C94Entry* entry = findNode(cur94);
            const std::uint32_t payload8 = entry ? readU32(entry->node, 0x08) : 0u;
            if (payload8 != 0u && entry != nullptr) {
                recordCall(trace, 0x004CE3D0u, payload8, 1u, localOutStackVa);
                recordCall(trace, 0x004D4EA0u, 0x00CA572Cu, 1u, entry->localOutVal);
                recordCall(trace, 0x004E29F0u, 0x00CA572Cu, 1u, entry->helper4CE3D0Ret);
            }
            const std::uint32_t next94 = entry ? readU32(entry->node, 0x00) : sentinel94Va;
            writeU32(self, 0x90, next94);
            cur94 = next94;
        } while (cur94 != sentinel94Va);
    }

    std::uint32_t freeCur = readU32(self, 0x94);
    while (freeCur != sentinel94Va) {
        const ListNode8C94Entry* entry = findNode(freeCur);
        const std::uint32_t nextFree = entry ? readU32(entry->node, 0x00) : sentinel94Va;
        recordCall(trace, 0x004166B0u, 0u, 1u, freeCur);
        freeCur = nextFree;
    }

    writeU32(self, 0x94, sentinel94Va);
    writeU32(self, 0x98, sentinel94Va);
    writeU32(self, 0x8C, 0u);
    writeU8(self, 0xAA, 0u);
}

// 6. 0x004EDD70
void runDeactivateAndEnqueueObject138(
    void* self,
    std::uint32_t selfVa,
    std::uint32_t targetVa,
    const void* target,
    const Module310DeactivateEntry* modules,
    std::uint32_t moduleCount,
    std::uint32_t allocatedNodeVa,
    void* allocatedNode,
    void* prevTailNode,
    CaveCallTrace* trace) noexcept {
    if (targetVa == 0u || target == nullptr) {
        return;
    }
    // Cave @ 0x004404A6 (site 0x004EDD7C, slot 0x018EDA00 -> 0x004EDD83):
    // test byte ptr [edi + 0x80], 1
    if ((readU8(target, 0x80) & 0x01u) != 0u) {
        return;
    }

    for (std::uint32_t i = 0; i < moduleCount; ++i) {
        const auto& m = modules[i];
        if (m.moduleVa == 0u) {
            break;
        }
        recordCall(trace, m.vt18TargetVa, m.moduleVa + 0x0Cu, 0u);
        if (m.subObjVa != 0u) {
            recordCall(trace, m.subVt00TargetVa, m.subObjVa, 0u);
        }
    }

    recordCall(trace, 0x006D2020u, targetVa, 2u, 0u, 1u);
    recordCall(trace, 0x006D23F0u, targetVa, 1u, 1u);

    const std::uint32_t listHeadVa = selfVa + 0x138u;
    recordCall(trace, 0x004D99E0u, listHeadVa + 0x08u, 2u, 0x0Cu, 0u);

    if (allocatedNode != nullptr) {
        writeU32(allocatedNode, 0x08, targetVa);
        writeU32(allocatedNode, 0x00, listHeadVa);
        const std::uint32_t prevVa = readU32(self, 0x13C);
        writeU32(allocatedNode, 0x04, prevVa);
        if (prevTailNode != nullptr) {
            writeU32(prevTailNode, 0x00, allocatedNodeVa);
        } else if (prevVa == listHeadVa) {
            writeU32(self, 0x138, allocatedNodeVa);
        }
        writeU32(self, 0x13C, allocatedNodeVa);
    }

    recordCall(trace, 0x006F01A0u, targetVa, 0u);
}

// 7. 0x00504510
std::uint32_t runAssignRefObjectAtOffset94(
    void* self,
    std::uint32_t selfVa,
    const void* other,
    std::uint32_t otherVa,
    std::uint32_t newObjVa,
    void* newObj,
    std::uint32_t oldObjVa,
    void* oldObj,
    std::uint32_t oldObjDtorTargetVa,
    CaveCallTrace* trace) noexcept {
    if (selfVa == otherVa) {
        return selfVa;
    }
    // Cave @ 0x0040B62E (site 0x00504522, slot 0x018ED658 -> 0x00504529):
    // add dword ptr [eax + 0x98], 1
    if (newObjVa != 0u && newObj != nullptr) {
        writeU32(newObj, 0x98, readU32(newObj, 0x98) + 1u);
    }
    if (oldObjVa != 0u && oldObj != nullptr) {
        const std::int32_t newRef = readI32(oldObj, 0x98) - 1;
        writeU32(oldObj, 0x98, static_cast<std::uint32_t>(newRef));
        if (newRef <= 0) {
            recordCall(trace, oldObjDtorTargetVa, oldObjVa + 0x94u, 1u, 1u);
        }
    }
    writeU32(self, 0x00, readU32(other, 0x00));
    return selfVa;
}

// 8. 0x0050A9C0
void runApplyIndexedTableScaleToAudioObject(
    std::uint32_t targetVa,
    const void* target,
    const void* subObj0C,
    std::uint32_t subObj0CVt08TargetVa,
    std::uint32_t keyReturnVa,
    std::uint32_t outIterStackVa,
    std::uint32_t foundNodeVa,
    float foundScale18,
    CaveCallTrace* trace) noexcept {
    const std::uint32_t subObj0CVa = readU32(target, 0x0C);
    const std::uint32_t index30 = readU32(target, 0x30);
    recordCall(trace, subObj0CVt08TargetVa, subObj0CVa, 0u);

    // Cave @ 0x0044026E (site 0x0050A9D3, slot 0x018ED9F8 -> 0x0050A9D9):
    // imul esi, esi, 0x1FC
    const std::uint32_t entryBase = 0x00DEE290u + index30 * 0x1FCu;
    recordCall(trace, 0x00506DF0u, entryBase + 0x1E0u, 2u, outIterStackVa, keyReturnVa);

    const std::uint32_t endSentinelVa = entryBase + 0x1E4u;
    const float denom10 = readF32(subObj0C, 0x10);
    const float factor1C = readF32(subObj0C, 0x1C);

    if (foundNodeVa != endSentinelVa && (foundNodeVa + 0x18u) != 0u) {
        recordCall(trace, 0x00559D10u, targetVa, 1u, floatToBits(foundScale18));
        if (denom10 > 0.0f) {
            const float prod = (foundScale18 / denom10) * factor1C;
            recordCall(trace, 0x00559D20u, targetVa, 1u, floatToBits(prod));
        }
        return;
    }

    recordCall(trace, 0x00559D10u, targetVa, 1u, floatToBits(denom10));
    recordCall(trace, 0x00559D20u, targetVa, 1u, floatToBits(factor1C));
}

// 9. 0x00520120
void runSetDrawableBit13CAndNotify394(
    void* self,
    std::uint32_t selfVa,
    std::uint32_t arg0,
    std::uint32_t listenerVt04TargetVa,
    CaveCallTrace* trace) noexcept {
    // Cave @ 0x00401B26 (site 0x00520120, slot 0x018F285C -> 0x00520126):
    // mov dl, byte ptr [ecx + 0x13C]
    const std::uint8_t dl = readU8(self, 0x13C);
    const std::uint8_t al = static_cast<std::uint8_t>(arg0 & 0xFFu);
    const std::uint8_t bl = static_cast<std::uint8_t>((dl >> 4u) & 0x01u);
    if (al == bl) {
        return;
    }
    const std::uint8_t newByte = static_cast<std::uint8_t>((((al << 4u) ^ dl) & 0x10u) ^ dl);
    writeU8(self, 0x13C, newByte);
    const std::uint32_t listenerVa = readU32(self, 0x394);
    if (listenerVa != 0u) {
        recordCall(trace, listenerVt04TargetVa, listenerVa, 1u, selfVa);
    }
}

// 10. 0x00523E60
std::uint8_t runQueryDrawableModules178(
    const void* self,
    std::uint32_t arg0,
    std::uint32_t arg1,
    const DrawableModule178QueryEntry* entries,
    std::uint32_t entryCount,
    CaveCallTrace* trace) noexcept {
    // Cave @ 0x0040D226 (site 0x00523E62, slot 0x018ED3AC -> 0x00523E68):
    // mov esi, dword ptr [ecx + 0x178]
    (void)readU32(self, 0x178);
    for (std::uint32_t i = 0; i < entryCount; ++i) {
        const auto& e = entries[i];
        if (e.moduleVa == 0u) {
            break;
        }
        recordCall(trace, e.vtB0TargetVa, e.moduleVa, 0u);
        if (e.subObjVa != 0u) {
            recordCall(trace, e.subVt0CTargetVa, e.subObjVa, 2u, arg0, arg1);
            if (e.subVt0CReturn != 0u) {
                return 1u;
            }
        }
    }
    return 0u;
}

// 11. 0x005242B0
void runMarkDrawableSlotFlags224And234(
    const void* self,
    std::uint32_t selfVa,
    std::uint32_t arg0,
    std::int32_t indexReturn,
    void* array224,
    void* array234,
    CaveCallTrace* trace) noexcept {
    (void)self;
    recordCall(trace, 0x00524230u, selfVa, 2u, arg0, 2u);
    if (indexReturn < 0) {
        return;
    }
    // Cave @ 0x004128B0 (site 0x005242C3, slot 0x018EDA54 -> 0x005242C9):
    // mov ecx, dword ptr [esi + 0x224]
    writeU8(array224, indexReturn, 1u);
    writeU8(array234, indexReturn, 1u);
}

// 12. 0x0052D6B0
void runSetDrawableField384AndSyncVisibility(
    void* self,
    std::uint32_t selfVa,
    std::uint32_t arg0,
    std::uint8_t globalDEEA50Byte13F,
    std::uint32_t localTempStackVa,
    std::uint32_t helper529FF0ReturnVa,
    std::uint8_t helper7886B0Return,
    CaveCallTrace* trace) noexcept {
    // Cave @ 0x0040A37E (site 0x0052D6BA, slot 0x018ED3A0 -> 0x0052D6C0):
    // mov dword ptr [esi + 0x384], eax
    writeU32(self, 0x384, arg0);
    const std::uint32_t obj138Va = readU32(self, 0x138);
    bool setOne = (globalDEEA50Byte13F != 0u);
    if (!setOne && obj138Va != 0u) {
        recordCall(trace, 0x00529FF0u, localTempStackVa, 2u, 0x76u, 0xABu);
        recordCall(trace, 0x007886B0u, obj138Va, 1u, helper529FF0ReturnVa);
        if (helper7886B0Return != 0u) {
            setOne = true;
        }
    }
    const std::uint32_t mergedArg = (arg0 & 0xFFFFFF00u) | (setOne ? 1u : 0u);
    recordCall(trace, 0x005208B0u, selfVa, 1u, mergedArg);
}

// 13. 0x00535410
std::uint32_t runCreateRefObjectBE03E4(
    void* outPtr,
    std::uint32_t outPtrVa,
    std::uint32_t arg1,
    std::uint32_t allocatedVa,
    void* allocatedObj,
    CaveCallTrace* trace) noexcept {
    recordCall(trace, 0x00416820u, 0u, 1u, 0x14u);
    if (allocatedVa != 0u && allocatedObj != nullptr) {
        writeU32(allocatedObj, 0x08, arg1);
        // Cave @ 0x0042DBC6 (site 0x00535428, slot 0x018ED604 -> 0x0053542F):
        // mov dword ptr [eax + 4], 0
        writeU32(allocatedObj, 0x04, 0u);
        writeU32(allocatedObj, 0x00, 0x00BE03E4u);
        const std::uint32_t listVa = allocatedVa + 0x0Cu;
        writeU32(allocatedObj, 0x0C, listVa);
        writeU32(allocatedObj, 0x10, listVa);
        writeU32(outPtr, 0x00, allocatedVa);
        writeU32(allocatedObj, 0x04, readU32(allocatedObj, 0x04) + 1u);
        return outPtrVa;
    }
    writeU32(outPtr, 0x00, 0u);
    return outPtrVa;
}

// 14. 0x00537A40
void runRemoveFlaggedDrawableFromList50(
    void* self,
    std::uint32_t selfVa,
    void* target,
    std::uint32_t targetVa,
    std::uint32_t globalDEBA64Field58,
    void* globalDEEA50,
    void* removedNode,
    std::uint32_t removedNodeVa,
    void* prevNode,
    void* nextNode,
    void* poolEntry,
    std::uint32_t localEventStackVa,
    void* localEventBuf,
    CaveCallTrace* trace) noexcept {
    // Cave @ 0x00420536 (site 0x00537A48, slot 0x018F29B8 -> 0x00537A4F):
    // test byte ptr [ebx + 0x13D], 0x40
    const std::uint8_t flags13D = readU8(target, 0x13D);
    if ((flags13D & 0x40u) == 0u) {
        return;
    }

    writeU32(self, 0x60, globalDEBA64Field58);
    writeU8(target, 0x13D, static_cast<std::uint8_t>(flags13D & 0xBFu));
    if (readU32(target, 0x138) != 0u) {
        writeU32(globalDEEA50, 0xBC, 0u);
        recordCall(trace, 0x005240C0u, targetVa, 0u);
    }
    recordCall(trace, 0x00524050u, targetVa, 0u);

    const std::uint32_t nextVa = readU32(removedNode, 0x00);
    const std::uint32_t prevVa = readU32(removedNode, 0x04);
    writeU32(prevNode, 0x00, nextVa);
    writeU32(nextNode, 0x04, prevVa);

    recordCall(trace, 0x004D36C0u, selfVa + 0x58u, 1u, 0x10u);
    const std::uint32_t oldFreeHead = readU32(poolEntry, 0x10);
    writeU32(removedNode, 0x00, oldFreeHead);
    writeU32(poolEntry, 0x10, removedNodeVa);

    writeU32(self, 0x5C, readU32(self, 0x5C) - 1u);
    writeU32(localEventBuf, 0x00, 0x005AABD0u);
    writeU32(localEventBuf, 0x04, selfVa);
    writeU32(localEventBuf, 0x08, targetVa);
    recordCall(trace, 0x0085E950u, selfVa + 0x3Cu, 1u, localEventStackVa);
}

// 15. 0x00538440
std::uint32_t runConstructObjectBE082C(
    void* self,
    std::uint32_t selfVa,
    std::uint32_t sourceVa,
    const FactoryItem538440Entry* items,
    std::uint32_t itemCount,
    CaveCallTrace* trace) noexcept {
    // Cave @ 0x00456F76 (site 0x0053844A, slot 0x018F2A04 -> 0x00538450):
    // mov dword ptr [edi], 0xBE082C
    writeU32(self, 0x00, 0x00BE082Cu);
    const std::uint32_t list4Va = selfVa + 0x04u;
    writeU32(self, 0x04, list4Va);
    writeU32(self, 0x08, list4Va);
    writeU32(self, 0x10, sourceVa);
    const std::uint32_t list14Va = selfVa + 0x14u;
    writeU32(self, 0x14, list14Va);
    writeU32(self, 0x18, list14Va);

    void* prevTailNode = nullptr;
    for (std::uint32_t i = 0; i < itemCount; ++i) {
        const auto& it = items[i];
        if (it.factoryObjVa != 0u && it.itemKey >= it.matchedTreeKey) {
            recordCall(trace, it.factoryVt00TargetVa, it.factoryObjVa, 1u, it.itemDescVa);
            recordCall(trace, 0x00416860u, 0u, 3u, 0x0Cu, 0x00BC8E74u, 0u);
            if (it.allocatedNode != nullptr) {
                writeU32(it.allocatedNode, 0x08, it.createdObjVa);
                writeU32(it.allocatedNode, 0x00, list4Va);
                const std::uint32_t prevVa = readU32(self, 0x08);
                writeU32(it.allocatedNode, 0x04, prevVa);
                if (prevTailNode != nullptr) {
                    writeU32(prevTailNode, 0x00, it.allocatedNodeVa);
                } else {
                    writeU32(self, 0x04, it.allocatedNodeVa);
                }
                writeU32(self, 0x08, it.allocatedNodeVa);
                prevTailNode = it.allocatedNode;
            }
        }
    }
    return selfVa;
}

// 16. 0x0055B530
void runDestroyAudioEventBDF2C4(
    void* self,
    std::uint32_t selfVa,
    const RefSubObjectDesc& sub20,
    const RefSubObjectDesc& sub1C,
    const RefSubObjectDesc& sub14,
    const RefSubObjectDesc& sub0C,
    const RefSubObjectDesc& sub04,
    CaveCallTrace* trace) noexcept {
    // Cave @ 0x004036AA (site 0x0055B534, slot 0x018ED650 -> 0x0055B53A):
    // lea ecx, [esi + 0x90]
    writeU32(self, 0x00, 0x00BDF2C4u);
    recordCall(trace, 0x004CA930u, selfVa + 0x90u, 0u);

    auto decRefAt04 = [&](const RefSubObjectDesc& d) noexcept {
        if (d.objVa != 0u && d.obj != nullptr) {
            const std::int32_t r = readI32(d.obj, 0x04) - 1;
            writeU32(d.obj, 0x04, static_cast<std::uint32_t>(r));
            if (r <= 0) {
                recordCall(trace, d.dtorTargetVa, d.objVa, 1u, 1u);
            }
        }
    };

    decRefAt04(sub20);
    decRefAt04(sub1C);

    if (sub14.objVa != 0u && sub14.obj != nullptr) {
        const std::int32_t r = readI32(sub14.obj, 0x98) - 1;
        writeU32(sub14.obj, 0x98, static_cast<std::uint32_t>(r));
        if (r <= 0) {
            recordCall(trace, sub14.dtorTargetVa, sub14.objVa + 0x94u, 1u, 1u);
        }
    }

    decRefAt04(sub0C);
    decRefAt04(sub04);
}

// 17. 0x0055CC60
std::uint32_t runConstructObjectBE2CD8(
    void* self,
    std::uint32_t selfVa,
    CaveCallTrace* trace) noexcept {
    // Cave @ 0x004025EC (site 0x0055CC63, slot 0x018ED450 -> 0x0055CC69):
    // mov dword ptr [esi], 0xBE2A48
    writeU32(self, 0x00, 0x00BE2A48u);
    writeU32(self, 0x04, 0u);
    writeU32(self, 0x00, 0x00BE2CD8u);
    recordCall(trace, 0x00417070u, selfVa + 0x08u, 0u);

    constexpr std::uint32_t kFloatOne = 0x3F800000u;     // [0x00BD90A4] = 1.0f
    constexpr std::uint32_t kConst94EC = 0x42C80000u;    // [0x00BD94EC] = 100.0f
    constexpr std::uint32_t kConst9644 = 0x447A0000u;    // [0x00BD9644] = 1000.0f

    writeU32(self, 0x10, kFloatOne);
    writeU32(self, 0x14, 0u);
    writeU32(self, 0x18, 0u);
    writeU32(self, 0x1C, 0u);
    writeU32(self, 0x28, 0u);
    writeU32(self, 0x2C, 0u);
    writeU32(self, 0x30, 0u);
    writeU32(self, 0x34, 0u);
    writeU32(self, 0x38, kFloatOne);
    for (std::ptrdiff_t off = 0x3C; off <= 0x50; off += 4) {
        writeU32(self, off, 0u);
    }
    writeU32(self, 0x54, 0xFFFFFFFFu);
    writeU32(self, 0x58, 0u);
    writeU32(self, 0x5C, 0u);
    writeU32(self, 0x60, 0u);
    writeU32(self, 0x64, 0u);
    writeU32(self, 0x70, 2u);
    for (std::ptrdiff_t off = 0x74; off <= 0x84; off += 4) {
        writeU32(self, off, 0u);
    }
    for (std::ptrdiff_t off = 0x8C; off <= 0x98; off += 4) {
        writeU32(self, off, 0u);
    }
    for (std::ptrdiff_t off = 0xA0; off <= 0xAC; off += 4) {
        writeU32(self, off, 0u);
    }
    for (std::ptrdiff_t off = 0xB4; off <= 0xC0; off += 4) {
        writeU32(self, off, 0u);
    }
    writeU32(self, 0xCC, kConst94EC);
    writeU32(self, 0xC8, 0u);
    writeU32(self, 0xD0, kConst9644);
    writeU32(self, 0xD4, kFloatOne);
    writeU32(self, 0xD8, kFloatOne);
    writeU32(self, 0xDC, 0u);
    writeU32(self, 0xE0, kFloatOne);
    writeU32(self, 0xE4, kFloatOne);
    writeU32(self, 0xE8, 2u);
    writeU32(self, 0xEC, 0xFFFFFFFFu);
    writeU32(self, 0xF0, 0u);
    writeU32(self, 0xF4, 0u);
    writeU32(self, 0xF8, 0u);
    writeU32(self, 0x100, 0u);
    writeU32(self, 0x104, 0u);
    writeU32(self, 0x108, 0u);
    writeU32(self, 0x10C, 0u);
    writeU32(self, 0x114, 0u);
    return selfVa;
}

// 18. 0x005838F0
void runReleaseLockedObjects6C68And6C6C(
    void* self,
    std::uint32_t lockTempStackVa,
    const RefSubObjectDesc& sub6C68,
    const RefSubObjectDesc& sub6C6C,
    std::uint32_t sub6C6CVt14TargetVa,
    CaveCallTrace* trace) noexcept {
    recordCall(trace, 0x00997410u, lockTempStackVa, 0u);

    // Cave @ 0x0043A04F (site 0x00583900, slot 0x018EDBC4 -> 0x00583906):
    // mov ecx, dword ptr [esi + 0x6C68]
    if (sub6C68.objVa != 0u && sub6C68.obj != nullptr) {
        writeU32(self, 0x6C68, 0u);
        const std::int32_t r = readI32(sub6C68.obj, 0x04) - 1;
        writeU32(sub6C68.obj, 0x04, static_cast<std::uint32_t>(r));
        if (r == 0) {
            writeU32(sub6C68.obj, 0x04, 1u);
            recordCall(trace, sub6C68.dtorTargetVa, sub6C68.objVa, 1u, 1u);
        }
    }

    if (sub6C6C.objVa != 0u && sub6C6C.obj != nullptr) {
        recordCall(trace, sub6C6CVt14TargetVa, sub6C6C.objVa, 0u);
        writeU32(self, 0x6C6C, 0u);
        const std::int32_t r = readI32(sub6C6C.obj, 0x04) - 1;
        writeU32(sub6C6C.obj, 0x04, static_cast<std::uint32_t>(r));
        if (r == 0) {
            writeU32(sub6C6C.obj, 0x04, 1u);
            recordCall(trace, sub6C6C.dtorTargetVa, sub6C6C.objVa, 1u, 1u);
        }
    }

    writeU8(self, 0x2C, 0u);
    recordCall(trace, 0x00997460u, lockTempStackVa, 0u);
}

// 19. 0x005A4ED0
void runReleaseAndClearHandleVector524(
    void* self,
    std::uint32_t selfVa,
    std::uint32_t globalDF6F94,
    const std::uint32_t* handleTargets,
    std::uint32_t handleCount,
    std::uint32_t helper591C30ReturnVa,
    CaveCallTrace* trace) noexcept {
    // Cave @ 0x0040BA01 (site 0x005A4ED0, slot 0x018F27D4 -> 0x005A4ED7):
    // cmp dword ptr [0x00DF6F94], 0
    if (globalDF6F94 == 0u) {
        return;
    }

    for (std::uint32_t i = 0; i < handleCount; ++i) {
        const std::uint32_t target = handleTargets[i];
        if (target != 0u) {
            recordCall(trace, 0x0067ACA0u, target, 0u);
        }
    }

    const std::uint32_t beginVa = readU32(self, 0x524);
    const std::uint32_t endVa = readU32(self, 0x528);
    recordCall(trace, 0x00591C30u, 0u, 3u, endVa, endVa, beginVa);
    recordCall(trace, 0x005901C0u, selfVa + 0x524u, 2u, helper591C30ReturnVa, endVa);

    const std::int32_t count = static_cast<std::int32_t>(endVa - beginVa) >> 2;
    const std::uint32_t newEndVa = endVa + static_cast<std::uint32_t>((-count) * 4);
    writeU32(self, 0x528, newEndVa);
}

// 20. 0x005C5D70
void runDestroyObjectBE97D4(
    void* self,
    std::uint32_t selfVa,
    CaveCallTrace* trace) noexcept {
    // Cave @ 0x00450B64 (site 0x005C5D76, slot 0x018ED958 -> 0x005C5D7C):
    // mov dword ptr [esi], 0xBE97D4
    writeU32(self, 0x00, 0x00BE97D4u);
    recordCall(trace, 0x004CAB60u, selfVa + 0x20u, 0u);
    const std::uint32_t arg10 = readU32(self, 0x10);
    recordCall(trace, 0x005BE310u, selfVa + 0x04u, 1u, arg10);
}

// 21. 0x005EF110
std::uint32_t runConstructObjectBE2030(
    void* self,
    std::uint32_t selfVa,
    CaveCallTrace* trace) noexcept {
    // Cave @ 0x0043A77D (site 0x005EF115, slot 0x018F2B10 -> 0x005EF11B):
    // mov dword ptr [esi], 0xBE2030
    writeU32(self, 0x00, 0x00BE2030u);
    writeU32(self, 0x30, 0u);
    writeU32(self, 0x34, 0u);
    writeU32(self, 0x38, 0u);
    writeU16(self, 0x3C, 0u);
    writeU32(self, 0x58, 0u);
    recordCall(trace, 0x005E6B30u, selfVa, 0u);
    return selfVa;
}

// 22. 0x006232F0
void runShutdownGlobalsDF6BC8AndDF6B34(
    std::uint32_t* globalDF6BC8,
    std::uint32_t globalDF6B34,
    CaveCallTrace* trace) noexcept {
    // Cave @ 0x00455491 (site 0x006232F0, slot 0x018F2E58 -> 0x006232F7):
    // cmp dword ptr [0x00DF6BC8], 0
    if (*globalDF6BC8 == 0u) {
        return;
    }
    if (globalDF6B34 != 0u) {
        recordCall(trace, 0x005F13A0u, globalDF6B34, 0u);
        recordCall(trace, 0x004166B0u, 0u, 1u, globalDF6B34);
    }
    const std::uint32_t oldBC8 = *globalDF6BC8;
    *globalDF6BC8 = 0u;
    if (oldBC8 != 0u) {
        recordCall(trace, 0x005EACE0u, oldBC8 + 0x0Cu, 0u);
        recordCall(trace, 0x004166B0u, 0u, 1u, oldBC8);
    }
    recordCall(trace, 0x00622D30u, 0u, 0u);
}

// 23. 0x0064BD80
void runUpdateSubObject3CAndReleaseB0(
    const void* self,
    void* subObj3C,
    std::uint32_t arg0,
    std::uint32_t securomOffset18F9E50,
    const RefSubObjectDesc& refB0,
    CaveCallTrace* trace) noexcept {
    const std::uint32_t sub3CVa = readU32(self, 0x3C);
    recordCall(trace, 0x00648310u, sub3CVa + 0x04u, 1u, arg0 + 0x04u);

    // Cave @ 0x00445F07 (site 0x0064BD9A, slot 0x018F26AC -> 0x0064BDA0):
    // add edi, dword ptr [0x018F9E50]
    const std::uint32_t adjustedArg = arg0 + securomOffset18F9E50;
    recordCall(trace, 0x00646A70u, sub3CVa + 0xA0u, 1u, adjustedArg);

    if (refB0.objVa != 0u && refB0.obj != nullptr) {
        const std::int32_t r = readI32(refB0.obj, 0x04) - 1;
        writeU32(refB0.obj, 0x04, static_cast<std::uint32_t>(r));
        if (r <= 0) {
            recordCall(trace, refB0.dtorTargetVa, refB0.objVa, 1u, 1u);
        }
        writeU32(subObj3C, 0xB0, 0u);
    }
}

// 24. 0x00683140
void runSetObjectField180(void* self, std::uint32_t value) noexcept {
    // Cave @ 0x00459661 (site 0x00683144, slot 0x018F24C4 -> 0x0068314A):
    // mov dword ptr [ecx + 0x180], eax
    writeU32(self, 0x180, value);
}

// 25. 0x00685EB0
std::uint8_t runReadSubObject4Byte144(const void* subObj4) noexcept {
    // Cave @ 0x00459EF6 (site 0x00685EB3, slot 0x018ED43C -> 0x00685EB9):
    // mov al, byte ptr [eax + 0x144]
    return readU8(subObj4, 0x144);
}

// 26. 0x006879B0
void runDetachSubObject238(
    void* self,
    std::uint32_t selfVa,
    CaveCallTrace* trace) noexcept {
    // Cave @ 0x00459DE0 (site 0x006879B2, slot 0x018ED738 -> 0x006879B8):
    // mov ecx, dword ptr [eax + 0x238]
    const std::uint32_t sub238Va = readU32(self, 0x238);
    if (sub238Va != 0u) {
        writeU32(self, 0x238, 0u);
        recordCall(trace, 0x007BDC20u, sub238Va, 1u, selfVa);
    }
}

// 27. 0x00688190
void runConstructObjectBF9CBC(
    void* self,
    const void* ownerObj,
    std::uint8_t flagByte) noexcept {
    // Cave @ 0x004399A6 (site 0x00688198, slot 0x018ED554 -> 0x0068819F):
    // mov dword ptr [eax + 4], 0
    writeU32(self, 0x04, 0u);
    writeU32(self, 0x00, 0x00BF9CBCu);
    if (ownerObj != nullptr) {
        writeU32(self, 0x08, readU32(ownerObj, 0x418));
    } else {
        writeU32(self, 0x08, 0u);
    }
    writeU8(self, 0x0C, flagByte);
}

// 28. 0x0071C460
void runPropagateObjectMap418ToModules310(
    const void* self,
    std::uint32_t selfVa,
    std::uint32_t localMapStackVa,
    void* localMapBuf,
    const Module310MapPropagateEntry* entries,
    std::uint32_t entryCount,
    CaveCallTrace* trace) noexcept {
    // Cave @ 0x00456E6C (site 0x0071C467, slot 0x018ED4FC -> 0x0071C46D):
    // mov eax, dword ptr [ebp + 0x418]
    const std::uint32_t obj418Va = readU32(self, 0x418);
    if (obj418Va == 0u) {
        return;
    }

    const std::uint32_t srcMapVa = obj418Va + 0x58u;
    const std::uint32_t localTreeVa = localMapStackVa + 0x04u;

    for (std::uint32_t i = 0; i < entryCount; ++i) {
        const auto& e = entries[i];
        if (e.moduleVa == 0u) {
            break;
        }
        recordCall(trace, e.vt30TargetVa, e.moduleVa + 0x0Cu, 0u);
        if (e.subObjVa != 0u) {
            recordCall(trace, e.subVt00TargetVa, e.subObjVa, 0u);
            if (e.subVt00Return == 0u) {
                if (localMapBuf != nullptr) {
                    writeU32(localMapBuf, 0x04, localTreeVa);
                    writeU32(localMapBuf, 0x08, localTreeVa);
                    writeU32(localMapBuf, 0x0C, 0u);
                    writeU8(localMapBuf, 0x10, 0u);
                    writeU32(localMapBuf, 0x14, 0u);
                }
                recordCall(trace, 0x006F2DA0u, localMapStackVa, 1u, srcMapVa);
                recordCall(trace, 0x004E3D00u, localMapStackVa, 1u, selfVa + 0x3F8u);
                recordCall(trace, e.subVt04TargetVa, e.subObjVa, 1u, localMapStackVa);
                const std::uint32_t rootNodeVa = localMapBuf ? readU32(localMapBuf, 0x0C) : 0u;
                recordCall(trace, 0x00636FA0u, localMapStackVa, 1u, rootNodeVa);
            }
            recordCall(trace, e.subVt18TargetVa, e.subObjVa, 0u);
        }
    }
}

}  // namespace ra3::gamelogic
