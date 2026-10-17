#pragma once

#include <cstddef>
#include <cstdint>

namespace ra3::gamelogic {

// ============================================================================
// Object Core (0x006A4CF0 - 0x006ABCE0) + Directly Owned Subsystems:
// - WeaponSetUpdate (Object + 0x37C, 0x006A15C0 - 0x006A1A50)
// - ExperienceTracker / ExperienceLevelTemplate (Object + 0x36C, 0x006A3B80 - 0x006A4180)
// - StructureUnpackUpdate (Object::findUpdateModule("StructureUnpackUpdate"), 0x006A0BF0 - 0x006A0C80)
// ============================================================================

struct Vec3f6A {
    float x;
    float y;
    float z;
};
static_assert(sizeof(Vec3f6A) == 0x0C);

// 16-byte entry stored in WeaponSetUpdate vector at [*(this+0x3C) .. *(this+0x40))
struct WeaponSetEntryKey {
    std::int32_t kindOrTag;
    float x;
    float y;
    float z;
};
static_assert(sizeof(WeaponSetEntryKey) == 0x10);

// Pair stored in ExperienceLevelTemplate table at *(tmpl + 0x1C), count at *(tmpl + 0x18)
struct ExperienceScaleEntry {
    std::int32_t key;
    float scale;
};
static_assert(sizeof(ExperienceScaleEntry) == 0x08);

// 8-byte entry stored in Object slot table *(obj->m_slotTable450)
struct ObjectSlotTableEntry450 {
    std::uint32_t objectId;
    std::uint8_t flag4;
    std::uint8_t recentFlag5;
    std::uint8_t activeFlag6;
    std::uint8_t pad7;
};
static_assert(sizeof(ObjectSlotTableEntry450) == 0x08);

// Object slot table header at *(this + 0x450)
struct ObjectSlotTable450 {
    ObjectSlotTableEntry450* entries; // +0x00
    std::uint32_t reserved04[3];      // +0x04..+0x0F
    std::uint32_t lastUpdateFrame;    // +0x10
    std::uint8_t reserved14[0x18];    // +0x14..+0x2B
    std::int8_t count;                // +0x2C
    std::uint8_t pad2D[3];
};
static_assert(offsetof(ObjectSlotTable450, entries) == 0x00);
static_assert(offsetof(ObjectSlotTable450, lastUpdateFrame) == 0x10);
static_assert(offsetof(ObjectSlotTable450, count) == 0x2C);

// Callbacks for virtual dispatch or external engine helpers during standalone testing
struct WeaponSetSlotCallbacks {
    void (*onVtable08)(std::uint32_t elemAddr, void* user) noexcept;
    void (*onSub64Vtable10)(std::uint32_t sub64Addr, void* user) noexcept;
    std::uint32_t (*onVtable10)(std::uint32_t slotAddr, void* user) noexcept;
    void (*onVtable1C)(std::uint32_t slotAddr, void* user) noexcept;
    std::uint32_t (*onVtable20)(std::uint32_t slotAddr, std::int32_t arg0, void* user) noexcept;
    std::uint32_t (*onVtable24)(std::uint32_t slotAddr, void* user) noexcept;
    bool (*onVtable2C)(std::uint32_t slotAddr, void* user) noexcept;
    bool (*onVtable30)(
        std::uint32_t slotAddr, std::int32_t a2, std::int32_t a3,
        std::int32_t a4, void* user) noexcept;
    bool (*onVtable34)(
        std::uint32_t slotAddr, std::int32_t a1, std::int32_t a2,
        std::int32_t a3, std::int32_t a5, void* user) noexcept;
    std::uint32_t (*onVtable38)(std::uint32_t slotAddr, void* user) noexcept;
    std::int32_t (*onQuerySub90Reload)(std::uint32_t sub90Addr, void* user) noexcept;
    void (*onSub38SetTarget)(std::uint32_t sub38Addr, std::uint32_t argAddr, void* user) noexcept;
    void (*onSub38ClearTarget)(std::uint32_t sub38Addr, void* user) noexcept;
    void (*onSub38ClearField30)(std::uint32_t sub38Addr, void* user) noexcept;
    void* user;
};

// ============================================================================
// Part A: WeaponSetUpdate (Object + 0x37C, 0x006A15C0 - 0x006A1A50) - 15 functions
// ============================================================================

// 0x006A15C0 (128B): scans 16-byte entries in [begin, end) for exact int + 3-float match
bool weaponSetHasEntryKey(
    const WeaponSetEntryKey* begin, const WeaponSetEntryKey* end,
    const WeaponSetEntryKey& key) noexcept;

// 0x006A1640 (92B): rebuilds mask at +0x34 from [*(this+0x14)..*(this+0x18)), invokes vtable+0x08
// on each element, and if owner Object *(this-8) -> +0x444 -> +0x64 is non-null invokes vtable+0x10
int weaponSetRebuildMaskAndNotify(
    std::uint32_t& outMask34, const std::uint32_t* elemFlags4, std::size_t elemCount,
    bool hasOwnerSub64, int& outElemCalls, int& outSub64Calls) noexcept;

// 0x006A16A0 (35B): invokes slot->vtable[+0x1C](slot) for each slot in [begin, end)
int weaponSetNotifyAllSlots1C(std::size_t slotCount) noexcept;

// 0x006A16D0 (67B): finds first slot with *(*(slot+0x0C)+4) == tag and calls slot->vtable[+0x30](a2,a3,a4)
bool weaponSetInvokeSlot30ByTag(
    const std::int32_t* slotTags, const std::uint8_t* slotResults, std::size_t slotCount,
    std::int32_t tag, int& outCalledIndex) noexcept;

// 0x006A1720 (80B): finds slot by tag (or tries all when tag == -1) via slot->vtable[+0x20](arg0)
std::uint32_t weaponSetFindWeapon20ByTag(
    const std::int32_t* slotTags, const std::uint32_t* slotWeapons20, std::size_t slotCount,
    std::int32_t arg0, std::int32_t tag, int& outCalls) noexcept;

// 0x006A1770 (71B): finds slot by tag (or tries all when tag == -1) via slot->vtable[+0x24]()
std::uint32_t weaponSetFindWeapon24ByTag(
    const std::int32_t* slotTags, const std::uint32_t* slotWeapons24, std::size_t slotCount,
    std::int32_t tag, int& outCalls) noexcept;

// 0x006A17C0 (47B): returns first weapon from slot->vtable[+0x24]() whose *(int32_t*)(w + 0x5C) != -1
std::uint32_t weaponSetFindActiveWeapon24(
    const std::uint32_t* slotWeapons24, const std::int32_t* weaponField5C,
    std::size_t slotCount) noexcept;

// 0x006A17F0 (48B): if tag <= 0 returns 0; else returns first slot with *(*(slot+0x0C)+4) == tag
std::uint32_t weaponSetFindSlotByPositiveTag(
    const std::uint32_t* slotAddrs, const std::int32_t* slotTags, std::size_t slotCount,
    std::int32_t tag) noexcept;

// 0x006A1820 (45B): returns first slot with *(*(slot+0x0C)+4) == tag (allows any tag)
std::uint32_t weaponSetFindSlotByTag(
    const std::uint32_t* slotAddrs, const std::int32_t* slotTags, std::size_t slotCount,
    std::int32_t tag) noexcept;

// 0x006A18A0 (62B): iterates slots calling vtable[+0x10](), if w && *(w+0x90) calls 0x007D2120(*(w+0x90))
std::int32_t weaponSetQueryFirstReloadState(
    const std::uint32_t* slotWeapons10, const std::uint32_t* weaponSub90,
    const std::int32_t* sub90Result, std::size_t slotCount) noexcept;

// 0x006A18E0 (73B): iterates slots calling vtable[+0x10](), checks *(sub90+0x28)==id || *(sub90+0x24)==id
bool weaponSetHasTargetObjectId(
    const std::uint32_t* slotWeapons10, const std::uint32_t* weaponSub90,
    const std::uint32_t* sub90Field24, const std::uint32_t* sub90Field28,
    std::size_t slotCount, std::uint32_t targetId) noexcept;

// 0x006A1930 (172B): if tag == -1 calls vtable[+0x34] on slots until true; else scans all slots
// keeping first matching tag and calls vtable[+0x34] on it
bool weaponSetDispatchSlot34ByTag(
    const std::int32_t* slotTags, const std::uint8_t* slotResults34, std::size_t slotCount,
    std::int32_t tag, int& outCalledSlotIndex) noexcept;

// 0x006A19E0 (46B): calls vtable[+0x2C]() on each slot until one returns true
bool weaponSetHasAnySlotReady2C(
    const std::uint8_t* slotReady2C, std::size_t slotCount) noexcept;

// 0x006A1A10 (62B): for each slot calls sub = vtable[+0x38](); if sub != 0, calls 0x007DCAA0(sub, arg)
// when arg != 0 else 0x007DF660(sub)
void weaponSetUpdateAllSlotSub38Targets(
    const std::uint32_t* slotSub38, std::size_t slotCount, std::uint32_t argAddr,
    std::uint32_t argObjectId, std::uint32_t* subField24, std::uint32_t* subField28,
    std::uint32_t* subField2C, std::uint32_t* subField30, std::uint32_t* subField38,
    std::int32_t* subField70) noexcept;

// 0x006A1A50 (46B): for each slot calls sub = vtable[+0x38](); if sub != 0 zeroes *(sub + 0x30)
void weaponSetClearAllSlotSub38Field30(
    const std::uint32_t* slotSub38, std::size_t slotCount, std::uint32_t* subField30) noexcept;

// ============================================================================
// Part B: ExperienceTracker / ExperienceLevelTemplate (0x006A3B80 - 0x006A4180) - 6 functions
// ============================================================================

// 0x006A3B80 (69B): searches *(this) vector for obj->m_id (+0xDC), writing index to *outIndex
bool experienceVectorFindObjectId(
    bool hasVector, const std::uint32_t* ids, std::size_t count,
    std::uint32_t objectId, std::uint32_t& outIndex) noexcept;

// 0x006A3BD0 (24B): returns first element of *(this) vector if non-null and non-empty, else 0
std::uint32_t experienceVectorFirstId(
    bool hasVector, const std::uint32_t* ids, std::size_t count) noexcept;

// 0x006A3BF0 (57B): scans (key, scale) pairs in ExperienceLevelTemplate; returns scale or defaultScale (+0x04)
float experienceTemplateLookupScale(
    float defaultScale, const ExperienceScaleEntry* entries, std::size_t count,
    std::int32_t key) noexcept;

// 0x006A3C30 (22B): __cdecl(tracker, key) -> 1.0f if tracker == nullptr else 0x006A3BF0(tracker, key)
float experienceTrackerLookupScaleOrOne(
    bool hasTracker, float defaultScale, const ExperienceScaleEntry* entries,
    std::size_t count, std::int32_t key) noexcept;

// 0x006A4130 (68B): checks !((flags14 >> 10) & 1) and non-null objects/containers before syncing container state
bool experienceShouldSyncContainedStateBit10(
    std::uint32_t templateFlags14, bool hasObjA, bool hasObjB,
    std::uint32_t containerA, std::uint32_t containerB,
    std::uint32_t& outSrcAddr244, std::uint32_t& outDstContainer) noexcept;

// 0x006A4180 (65B): if !((flags14 >> 11) & 1) and both objects and their +0xF4 containers exist,
// copies *(float*)(containerB + 0x25C) -> *(float*)(containerA + 0x25C)
void experienceSyncContainedVisionRangeBit11(
    std::uint32_t templateFlags14, bool hasObjA, bool hasObjB,
    bool hasContainerA, bool hasContainerB,
    float& containerAVision25C, float containerBVision25C) noexcept;

// ============================================================================
// Part C: StructureUnpackUpdate + Object Core (0x006A0BF0 - 0x006ABCE0) - 47 functions
// ============================================================================

// 0x006A0BF0 (129B): StructureUnpackUpdate::startUnpack: sets byte +0x45 = 1, +0x40 = currentFrame,
// and computes sleep target frame = currentFrame + 0x3FFFFFFF
void structureUnpackStart(
    std::uint8_t& unpackingFlag45, std::uint32_t& startFrame40,
    std::uint32_t& outScheduleFrame, std::uint32_t currentFrame) noexcept;

// 0x006A0C80 (86B): StructureUnpackUpdate::resetOrPauseUnpack: clears byte +0x45 = 0, accumulates
// elapsed frames into +0x3C if 0 < startFrame40 < currentFrame, and computes next wake frame
void structureUnpackPauseAndAccumulate(
    std::uint8_t& unpackingFlag45, std::uint32_t& startFrame40,
    std::uint32_t& accumulatedFrames3C, std::uint32_t unpackDuration38,
    std::uint32_t currentFrame, std::uint32_t& outScheduleFrame) noexcept;

// 0x006A4CF0 (44B): scans [*(this+4)..*(this+8)) for non-null sub-object with (*(*(sub+4)+8) & mask) != 0
std::uint32_t objectFindSubObjectByMask(
    const std::uint32_t* subObjAddrs, const std::uint32_t* subObjMasks,
    std::size_t count, std::uint32_t mask) noexcept;

// 0x006A4E70 (43B): returns true if (status84 & (1<<21)) || (status84 & (1<<19)) || *(int32_t*)(aiUpdate + 0x190) > 0
bool objectIsMobileOrHasWaypoints(
    std::uint32_t status84, std::int32_t aiField190) noexcept;

// 0x006A4ED0 (77B): checks crusher byte (*(tmpl+0x30B) != 0), target template bits (+0xD4 bit 20, +0xDC bit 7),
// and if this->m_aiUpdate (+0x374) != nullptr requires (*(ai+0x208) & 0x200) != 0
bool objectCanCrushOrSquishTarget(
    std::uint8_t selfCrusherByte30B, std::uint32_t targetTmplD4,
    std::uint32_t targetTmplDC, bool hasAiUpdate, std::uint32_t aiFlags208) noexcept;

// 0x006A4F20 (55B): returns true if (status80 & (1<<6)), or if (status98 & (1<<23)) and
// (*(*(arg+8)+0x1C) & (1<<28)) == 0
bool objectIsIgnoredByWeaponOrKind(
    std::uint32_t status80, std::uint32_t status98, std::uint32_t weaponFlags1C) noexcept;

// 0x006A4F60 (18B): if *(this+0x37C) != nullptr tail-calls 0x006A19E0, else returns false
bool objectHasAnyWeaponReady(
    bool hasWeaponSetUpdate, const std::uint8_t* slotReady2C, std::size_t slotCount) noexcept;

// 0x006A4FB0 (117B): links Object into dirty status list at 0x00DF8928 if *(this+0x4EC) == 0,
// and if m_aiUpdate (+0x374) has nextWakeFrame (+0x14) > currentFrame + 1 and byte +0x43B == 0
// schedules wake at currentFrame + 1
void objectMarkStatusDirtyAndWakeAI(
    std::uint32_t selfAddr, std::uint32_t& globalHeadAddr,
    std::uint32_t& selfPrevLink4EC, std::uint32_t& selfNextLink4F0,
    std::uint32_t& oldHeadPrevLink4EC, bool hasAiUpdate,
    std::uint32_t aiNextWakeFrame14, std::uint8_t aiFlag43B,
    std::uint32_t currentFrame, bool& outScheduledWake,
    std::uint32_t& outWakeFrame) noexcept;

// 0x006A5030 (36B): tests bit `bitIndex` in 15-dword bitset at Object + 0xA0
bool objectTestStatusBit(
    const std::uint32_t statusWords[15], std::uint32_t bitIndex) noexcept;

// 0x006A5060 (132B): updates Object status bitset at +0xA0 via 0x00522880, compares with previous
// 15-dword snapshot via 0x004D5B10; if changed, calls 0x006A4FB0 and if bit 10 of word 11 (+0xCC,
// global bit 362 = 0x16A) changed and m_team (+0x418) != nullptr adjusts team count (+0x125C) by +1/-1
void objectUpdateStatusBits5060(
    std::uint32_t statusWords[15], const std::uint32_t newWordsAfter522880[15],
    bool hasTeam418, std::int32_t& ioTeamCount125C,
    bool& outCalledMarkDirty) noexcept;

// 0x006A50F0 (161B): copies 15-dword status bitset `srcWords` into Object + 0xA0, compares with previous
// snapshot; if changed or forceNotify != 0, updates team count (+0x125C) when bit 10 of +0xCC changed
// and notifies container (+0xF4) when non-null
void objectAssignStatusMask50F0(
    std::uint32_t statusWords[15], const std::uint32_t srcWords[15],
    std::uint8_t forceNotify, bool hasTeam418, std::int32_t& ioTeamCount125C,
    bool hasContainerF4, bool& outNotifiedContainer) noexcept;

// 0x006A51C0 (44B): if *(this+0x37C) != nullptr tries 0x006A1770(tag) then 0x006A1720(0, tag)
std::uint32_t objectFindWeaponBySlotOrTag(
    bool hasWeaponSetUpdate, const std::int32_t* slotTags,
    const std::uint32_t* slotWeapons24, const std::uint32_t* slotWeapons20,
    std::size_t slotCount, std::int32_t tag) noexcept;

// 0x006A51F0 (18B): if *(this+0x37C) != nullptr tail-calls 0x006A17C0, else nullptr
std::uint32_t objectGetActiveWeapon(
    bool hasWeaponSetUpdate, const std::uint32_t* slotWeapons24,
    const std::int32_t* weaponField5C, std::size_t slotCount) noexcept;

// 0x006A5240 (41B): if (status88 & (1<<9)) == 0 returns true; else if target != nullptr calls target->vtable[+4]()
bool objectCanInteractWithTarget(
    std::uint32_t status88, bool hasTarget, bool targetVtable4Result) noexcept;

// 0x006A5270 (44B): returns 1 if (status90 & (1<<14)) != 0 or (m_aiUpdate && 0x005AA1C0(*(ai+0x3C)))
std::uint32_t objectIsAirborneOrLocomotorActive(
    std::uint32_t status90, bool hasAiUpdate, bool hasLocomotorSub4,
    bool locomotorVtable38Result) noexcept;

// 0x006A52A0 (31B): returns ((status98 >> 4) & 1) if m_physics (+0x380) != nullptr and *(physics+0x38) == 1
bool objectIsPhysicsFallingWithKindBit(
    bool hasPhysics, std::int32_t physicsState38, std::uint32_t status98) noexcept;

// 0x006A52C0 (91B): checks status bits (+0x84 bit 29, +0x80 bit 29, +0x90 bit 28, +0xB0 bit 25,
// *(tmpl+0xDC) bit 3) or (m_physics != nullptr && *(physics+0x38) == 1)
bool objectIsCrushableOrPhysicsActive(
    std::uint32_t status84, std::uint32_t status80, std::uint32_t status90,
    std::uint32_t statusB0, std::uint32_t tmplDC,
    bool hasPhysics, std::int32_t physicsState38) noexcept;

// 0x006A5320 (69B): if (*(*(other+4)+0xE0) & (1<<19)) != 0 scans [*(this+0x4B8)..*(this+0x4BC)) for other->m_id (+0xDC)
bool objectHasObjectInList4B8(
    std::uint32_t otherTmplE0, std::uint32_t otherIdDC,
    const std::uint32_t* listBegin, const std::uint32_t* listEnd) noexcept;

// 0x006A5450 (114B): sets m_producerId (+0xE4) = producer ? producer->m_id (+0xDC) : 0, and if
// ((*(*(this+4)+0xC8) >> 25) & 1) == 0 && m_team (+0x418) != nullptr && byte +0x4D7 != 0 notifies team before/after
void objectSetProducerId(
    std::uint32_t tmplC8, bool hasTeam418, std::uint8_t flag4D7,
    bool hasProducer, std::uint32_t producerIdDC,
    std::uint32_t& ioFieldE4, bool& outUnregisteredBefore, bool& outRegisteredAfter) noexcept;

// 0x006A5530 (93B): queries drawable visibility slot (+0x24 + playerIdx*4) when m_drawable (+0x9C) != nullptr
std::uint32_t objectGetDrawableVisibility5530(
    bool hasDrawable9C, bool playerFlag7E79F0, std::uint32_t tmplC4,
    std::uint32_t playerIndex, const std::uint32_t drawableTable24[20]) noexcept;

// 0x006A5590 (93B): queries drawable visibility slot (+0x74 + playerIdx*4) when m_drawable (+0x9C) != nullptr
std::uint32_t objectGetDrawableVisibility5590(
    bool hasDrawable9C, bool playerFlag7E79F0, std::uint32_t tmplC4,
    std::uint32_t playerIndex, const std::uint32_t drawableTable74[20]) noexcept;

// 0x006A56F0 (96B): propagates disabled type `disabledType` to container *(this+0xF4)+0x14C when *(this+0x26C) != 0
void objectPropagateDisabledTypeToContainer(
    bool hasContainerF4, std::uint32_t disabledMask26C, std::int32_t disabledType,
    std::uint8_t& ioContainerByte14C) noexcept;

// 0x006A5750 (66B): returns disabled until frame for `disabledType` (0..11)
std::uint32_t objectGetDisabledUntilFrame(
    std::uint32_t disabledType, std::uint32_t disabledMask26C,
    const std::uint32_t disabledFrames2D0[12], std::uint32_t currentFrame) noexcept;

// 0x006A5810 (124B): compacts active entries (activeFlag6 != 0) in *(this+0x450) slot table, zeroing flag4 and recentFlag5
void objectCompactSlotTable450(ObjectSlotTable450* table) noexcept;

// 0x006A5890 (110B): clears entries matching `objectId` in *(this+0x450) slot table and calls 0x006A5810
void objectRemoveFromSlotTable450(
    ObjectSlotTable450* table, std::uint32_t objectId) noexcept;

// 0x006A5A60 (66B): checks !(status80 & (1<<8)), !(status88 & (1<<29)), optional ai->vtable[+0x1FC](), returns *(uint8_t*)(cmd + 0x7C)
std::uint8_t objectCanExecuteCommand(
    std::uint32_t status80, std::uint32_t status88, bool hasAiUpdate,
    bool aiVtable1FCResult, std::uint8_t commandByte7C) noexcept;

// 0x006A5B10 (88B): checks if table->lastUpdateFrame is currentFrame or currentFrame - 1 and an entry has recentFlag5 != 0 and objectId == id
bool objectHasRecentFlag5InSlotTable450(
    const ObjectSlotTable450* table, std::uint32_t currentFrame,
    std::uint32_t objectId) noexcept;

// 0x006A5B70 (66B): checks if an entry in table has activeFlag6 != 0 and objectId == id
bool objectHasActiveEntryInSlotTable450(
    const ObjectSlotTable450* table, std::uint32_t objectId) noexcept;

// 0x006A5BC0 (48B): returns posZ (+0x40) - terrain->getLayerHeight(posX, posY, layer454, 0, 1)
float objectGetHeightAboveTerrain(
    float posX, float posY, float posZ, float terrainLayerHeight) noexcept;

// 0x006A5F50 (78B): evaluates selectability from template flags (+0xC4 bit 27, +0xC8 bit 9), status80 bit 3,
// template sub14 byte +0x24B, object byte +0x471 bit 0, and fallback byte +0x4D2
bool objectIsSelectable(
    std::uint32_t tmplC4, std::uint32_t tmplC8, std::uint8_t tmplSub14Byte24B,
    std::uint32_t status80, std::uint8_t objFlags471, std::uint8_t objSelectable4D2) noexcept;

// 0x006A60A0 (163B): locates primary body modules in null-terminated module arrays +0x310 of `this` and `other`,
// swaps their slots, updates +0x328 and *(*(obj+0x32C)), and updates module owner pointers (+0x08)
void objectSwapPrimaryBodyModules(
    std::uint32_t selfAddr, std::uint32_t otherAddr,
    std::uint32_t* selfModules310, const std::uint32_t* selfModTags,
    std::uint32_t& selfBody328, std::uint32_t& selfBodySlot32C,
    std::uint32_t* otherModules310, const std::uint32_t* otherModTags,
    std::uint32_t& otherBody328, std::uint32_t& otherBodySlot32C,
    std::uint32_t& outNewSelfBodyOwner, std::uint32_t& outNewOtherBodyOwner) noexcept;

// 0x006A6210 (64B): queries reload/weapon state from m_aiUpdate->+0x47C or m_weaponSetUpdate (0x006A18A0) when !(flags471 & 1)
std::int32_t objectGetReloadTimeOrWeaponState(
    std::uint8_t flags471, bool hasAiUpdate, bool hasAiSub47C,
    std::int32_t aiSub47CResult, bool hasWeaponSetUpdate,
    const std::uint32_t* slotWeapons10, const std::uint32_t* weaponSub90,
    const std::int32_t* sub90Result, std::size_t slotCount) noexcept;

// 0x006A6250 (47B): clears +0x30 on m_aiUpdate->+0x47C (via 0x0079C0F0) and on all WeaponSetUpdate slots (via 0x006A1A50)
void objectClearWeaponTargetField30(
    bool hasAiUpdate, bool hasAiSub47C, std::uint32_t& aiSub47CField30,
    bool hasWeaponSetUpdate, const std::uint32_t* slotSub38, std::size_t slotCount,
    std::uint32_t* subField30) noexcept;

// 0x006A6280 (52B): returns true if ((*(*(this+4)+0xD4) >> 14) & 1) != 0 and ((statusB0 >> 30) & 3) != 0
std::uint32_t objectIsUnderConstructionOrUpgrading(
    std::uint32_t tmplD4, std::uint32_t statusB0) noexcept;

// 0x006A6510 (196B): computes effective vision range from +0x25C * +0x258, optional modifier 0x11 at +0x36C,
// and altitude bonus when *(tmpl+0x2D8) > 0.0f
float objectComputeVisionRange(
    float baseVision25C, float visionScale258, bool hasModifier11, float modifier11Scale,
    float posZ40, float groundZ268, float tmplHeightFactor2D8,
    float tmplMaxBonus2DC, float tmplMinBonus2E0) noexcept;

// 0x006A65E0 (214B): computes effective shroud clearing range from +0x260, optional modifier 0x15 at +0x36C,
// altitude bonus when *(tmpl+0x2D8) > 0.0f, and zeroes if (*(tmpl+0xDC) & (1<<9)) && (*(this+0x94) & (1<<2))
float objectComputeShroudClearingRange(
    float baseShroud260, bool hasModifier15, float modifier15Scale,
    float posZ40, float groundZ268, float tmplHeightFactor2D8,
    float tmplMaxBonus2DC, float tmplMinBonus2E0,
    std::uint32_t tmplDC, std::uint32_t status94) noexcept;

// 0x006A6770 (54B): writes *(this+0x464) to *outVal if non-zero, else queries (this+0x78)->vtable[+0x2C]() -> +0x14 -> +0x94
void objectGetFormationOrTemplateValue464(
    std::uint32_t field464, std::uint32_t fallback94, std::uint32_t& outValue) noexcept;

// 0x006A68E0 (107B): returns veterancy/radar priority or -1 based on *(this+0x3AC), template flags, and player state
std::int32_t objectGetRadarOrVeterancyPriority(
    bool hasSub3AC, std::int32_t sub3ACField20, std::uint32_t tmplD4,
    std::uint8_t flags471, std::uint8_t tmplByteC7, bool hasPlayer,
    bool playerHasOverride1F8, bool playerHasOverride1FC) noexcept;

// 0x006A6B10 (219B): packs two 16-bit state masks into EAX ((high16 << 16) | low16) from Object status words
// at `esi = *(ecx - 0x70)` (+0xC0, +0xC4, +0xD0, +0xD4, +0xD8)
std::uint32_t objectComputePackedStateFlags6B10(
    std::uint32_t statusC0, std::uint32_t statusC4, std::uint32_t statusD0,
    std::uint32_t statusD4, std::uint32_t statusD8) noexcept;

// 0x006A6C10 (99B): if !(target->status80 & (1<<4)), iterates modules in [*(this+0x2A0)..*(this+0x2A4))
// while !(*(this+0x0C) & (1<<4)) and calls mod->vtable[0](target, a2, a3)
int objectDispatchCollisionModules2A0(
    std::uint32_t targetStatus80, const std::uint32_t* selfFlags0CPerStep,
    std::size_t moduleCount) noexcept;

// 0x006A6C80 (42B): copies 3 floats from (+0x1E8 + 0x0C/+0x1C/+0x2C) if byte +0x234 != 0 else from (+0x08 + 0x0C/+0x1C/+0x2C)
void objectGetTransformTranslation(
    std::uint8_t useAltTransform234, const float primaryMatrix3x4[12],
    const float altMatrix3x4[12], Vec3f6A& outPos) noexcept;

// 0x006A71A0 (29B): returns ((*(*(this+4)+0xCB) & 1) != 0) && (((*(this+0xC8) >> 9) & 1) != 0)
bool objectHasTemplateBitCBAndStatusC8Bit9(
    std::uint8_t tmplByteCB, std::uint32_t statusC8) noexcept;

// 0x006A72A0 (246B): if byte +0x114 != 0 copies position (+0x38..+0x40) to *outPos; else transforms offset
// (+0x110 + 0x18..0x20) by 3x4 matrix at +0x08..+0x34 into *outPos
void objectComputeTransformedOffsetPosition(
    std::uint8_t useRawPos114, const Vec3f6A& rawPos38,
    const float matrix3x4_08[12], const Vec3f6A& localOffset128,
    Vec3f6A& outPos) noexcept;

// 0x006A73A0 (63B): scans null-terminated array at *(this+0x310), calling mod->vtable[+0x14](&scratch)
// and returning first module whose returned id equals `targetId`
std::uint32_t objectFindModuleById310(
    const std::uint32_t* moduleAddrs, const std::uint32_t* moduleIds,
    std::size_t count, std::uint32_t targetId) noexcept;

// 0x006A7510 (35B): if (statusC8 & (1<<11)) returns 0; else returns (statusC8 & (1<<12)) ? 1 : -1
std::int32_t objectGetStatusTriState7510(std::uint32_t statusC8) noexcept;

// 0x006A7540 (29B): returns ((status8C >> 4) & 1) || ((status8C >> 3) & 1)
bool objectHasStatus8CBit3Or4(std::uint32_t status8C) noexcept;

// 0x006A7560 (55B): returns true if tri-state from statusC8 (bit 11 -> 0, bit 12 -> 1, else -1) != -1
bool objectHasValidStatusTriState7560(std::uint32_t statusC8) noexcept;

// 0x006A75A0 (93B): evaluates composite status/template predicate across tmpl+0xE0 bit 19, statusC0 bit 5,
// status90 bit 28, status80 bit 29, and tmpl+0xDC bit 3
std::uint32_t objectIsCrushedOrImmobilized75A0(
    std::uint32_t tmplE0, std::uint32_t tmplDC, std::uint32_t statusC0,
    std::uint32_t status90, std::uint32_t status80) noexcept;

// 0x006A7620 (122B): searches [*(this+0x32C)..*(this+0x330)) skipping *(this+0x328) for module where
// mod->vtable[+0x94](a1, 0, a2) returns true; falls back to *(this+0x328) or *(*(this+0x32C))
std::uint32_t objectFindBodyOrWeaponModule7620(
    const std::uint32_t* modules, const std::uint8_t* vtable94Match,
    std::size_t count, std::uint32_t primary328) noexcept;

// 0x006A7780 (108B): drains the global dirty Object status intrusive list at 0x00DF8928;
// for each node unlinks (+0x4EC/+0x4F0) and if node->m_containedBy (+0xF4) != nullptr calls 0x00533CB0(node+0xA0, 1, 0)
int objectFlushDirtyStatusList7780(
    std::uint32_t& ioGlobalHead, std::uint32_t* nodeAddrs,
    std::uint32_t* nodePrevLink4EC, std::uint32_t* nodeNextLink4F0,
    const std::uint8_t* nodeHasContainerF4, std::size_t nodeCount,
    int& outContainerNotifications) noexcept;

// 0x006A78D0 (32B): shifted-this (+0x78) predicate: if (*(ecx+0x54) & (1<<10)) returns false;
// else returns *(ecx+0x3A0) == *(*(0x00DFBD74) + 0x2C)
bool objectIsOwnedByLocalPlayerShifted78(
    std::uint32_t shiftedField54, std::uint32_t shiftedField3A0,
    std::uint32_t activePlayerField2C) noexcept;

// 0x006A7900 (40B): shifted-this (+0x78) method: calls 0x006A7620(ecx - 0x78, a1, 1); if non-null calls
// mod->vtable[+0x158](), else returns -1
std::int32_t objectQueryModuleValue158Shifted78(
    const std::uint32_t* modules, const std::uint8_t* vtable94Match,
    const std::int32_t* vtable158Value, std::size_t count,
    std::uint32_t primary328) noexcept;

// 0x006A7930 (61B): tests 7 status dwords at Object+0x80..+0x98 against masks at 0x00DF8A9C..0x00DF8AB4,
// or ((*(this+0xB4) >> 5) & 1) != 0
std::uint32_t objectMatchesGlobalStatusMaskOrBitB4(
    const std::uint32_t status80To98[7], const std::uint32_t globalMasks[7],
    std::uint32_t statusB4) noexcept;

// 0x006A7970 (102B): computes 2D delta (pos38 - target.x, pos3C - target.y), normalizes forward vector
// (+0x08, +0x18, +0x28) via 0x0078BFB0, and returns 1 if dot2D < 0.0f else 0
std::uint32_t objectIsTargetBehind2D(
    float selfPosX38, float selfPosY3C, float mat08, float mat18, float mat28,
    float targetX, float targetY) noexcept;

// 0x006ABC00 (173B): transitions Object status bit `clearBit` off and `setBit` on in 15-dword bitset (+0xA0);
// if either changed, updates team count (+0x125C) when bit 0x16A (362) was cleared (-1) or set (+1) and calls 0x006A4FB0
void objectTransitionStatusBitBC00(
    std::uint32_t statusWords[15], std::uint32_t clearBit, std::uint32_t setBit,
    bool hasTeam418, std::int32_t& ioTeamCount125C,
    bool& outCalledMarkDirty) noexcept;

// 0x006ABCE0 (37B): computes 3D squared distance between `this` (+0x38..+0x40) and `other` (+0x38..+0x40) on x87
float objectGetDistanceSquaredToObject(
    const Vec3f6A& selfPos38, const Vec3f6A& otherPos38) noexcept;

} // namespace ra3::gamelogic
