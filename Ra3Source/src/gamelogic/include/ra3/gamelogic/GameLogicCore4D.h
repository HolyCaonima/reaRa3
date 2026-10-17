#pragma once

#include <cstddef>
#include <cstdint>

namespace ra3::gamelogic {

// 0x004D2AD0 - GhostObjectManager circular doubly-linked list node relink-before
void relinkGhostObjectNodeBefore4D(void* node, void* targetNode) noexcept;

// 0x004D3750 - 8-byte ObjectID pair equality check
std::uint32_t equalsObjectIDPair4D(const void* self, const void* other) noexcept;

// 0x004D3770 - Normalize angle (radians) into (-pi, pi]
float normalizeAngleRad4D(float angleRad) noexcept;

// 0x004D3CD0 - BehaviorModule[2] class-name literal VA ("BehaviorModule")
std::uint32_t getBehaviorModuleClassNameVa4D() noexcept;

// 0x004D4020 - Convert duration at (*holder + 0x08) to integer frames using
// global frame rate (0x00DEEEE8) and optional GameLogic time scale (+0x1C4)
std::int32_t convertSecondsToFrames4D(
    const void* holder, const void* theGameLogic, float logicFramesPerSecond) noexcept;

// 0x004D4260 - CampaignSystem[2] subsystem-name literal VA ("CampaignSystem")
std::uint32_t getCampaignSystemNameVa4D() noexcept;

// 0x004D4320 - DelayedExperienceLevelGrantSystem[1] reset (+0x1C = 0)
void resetDelayedExperienceLevelGrantSystem4D(void* self) noexcept;

// 0x004D4330 - DelayedExperienceLevelGrantSystem[2] subsystem-name literal VA
std::uint32_t getDelayedExperienceLevelGrantSystemNameVa4D() noexcept;

// 0x004D4400 - DamageInfoInput payload assignment (+0x04..+0x82, preserving vtable at +0x00)
void* assignDamageInfoInputPayload4D(void* dst, const void* src) noexcept;

// 0x004D4500 - Compare scaled remaining cooldown frames between two entries (+0xA0 frame, +0x38 scale)
std::uint32_t compareCooldownProgress4D(
    const void* lhs, const void* rhs, std::uint32_t currentFrame) noexcept;

// 0x004D4910 - ThingTemplateRefWrapper constructor (vtable 0x00BDBE5C, +0x04 = ref)
void* initThingTemplateRefWrapper4D(void* self, std::uint32_t templateRef) noexcept;

// 0x004D4C50 - GameLogic post-increment sequence allocator at +0x144
std::uint32_t allocateNextSequenceId4D(void* gameLogic) noexcept;

// 0x004D4CD0 - Count singly-linked list nodes starting at *(owner + 0xBC) via next at +0x7C
std::uint32_t countLinkedNodesAtBC4D(const void* owner) noexcept;

// 0x004D4EA0 - GameLogic CRC buffer high-water mark update at +0x1C
void updateCrcBufferMaxSize4D(void* crcBuffer, std::uint32_t candidateSize) noexcept;

// 0x004D5530 - Update float high-water mark at +0x20 when candidate > current
void updateMaxFloatAt204D(void* self, float candidate) noexcept;

// 0x004D57A0 - Snap angle (radians) in-place to nearest cardinal multiple of pi/2
void snapAngleToCardinalRad4D(float* inOutAngleRad) noexcept;

// 0x004D5900 - 288-bit (9-word) KindOf / bitset non-zero test
bool anyBitFlags288_4D(const void* bits) noexcept;

// 0x004D5920 - 288-bit bitset test: all `required` bits set and no `forbidden` bits set
bool testRequiredAndForbiddenBitFlags288_4D(
    const void* self, const void* requiredBits, const void* forbiddenBits) noexcept;

// 0x004D5A40 - 480-bit (15-word) ModelCondition / bitset intersection test
bool intersectsBitFlags480_4D(const void* self, const void* other) noexcept;

// 0x004D5AB0 - 480-bit (15-word) bitset non-zero test
bool anyBitFlags480_4D(const void* self) noexcept;

// 0x004D5B10 - 480-bit (60-byte) bitset equality test
bool equalsBitFlags480_4D(const void* lhs, const void* rhs) noexcept;

// 0x004D5BB0 - 224-bit (7-word) ObjectStatusMask non-zero test
bool anyObjectStatusMask224_4D(const void* self) noexcept;

// 0x004D5BD0 - 224-bit (7-word) ObjectStatusMask intersection test
bool intersectsObjectStatusMask224_4D(const void* self, const void* other) noexcept;

// 0x004D5DF0 - DamageInfoInput copy constructor (vtable 0x00BDC178, copies +0x04..+0x82)
void* copyConstructDamageInfoInput4D(void* self, const void* other) noexcept;

// 0x004D6170 - Region2D strict interior point test (lo.x < p.x < hi.x && lo.y < p.y < hi.y)
std::uint32_t containsPointStrictRegion2D4D(const void* region, const void* point) noexcept;

// 0x004D6500 - AIInternalMoveToState goal record default initializer (0x28 bytes)
void* resetMoveGoalResult4D(void* self) noexcept;

// 0x004D6550 - DamageInfoInput default constructor (vtable 0x00BDC178)
void* defaultConstructDamageInfoInput4D(void* self) noexcept;

// 0x004D6680 - Single-word embedded bitset at +0x26C non-zero test
bool anyBitAt26C4D(const void* self) noexcept;

// 0x004D66A0 - Bit test in embedded bitset starting at +0x26C
std::uint32_t testBitAt26C4D(const void* self, std::uint32_t bitIndex) noexcept;

// 0x004D6710 - Safe dereference of *(*(self + 0x04) + 0x0C), returning 0 when null
std::uint32_t getSpecialAbilityField0C4D(const void* self) noexcept;

// 0x004D6730 - Safe dereference of *(*(self + 0x04) + 0x10), returning 0 when null
std::uint32_t getSpecialAbilityField104D(const void* self) noexcept;

// 0x004D6750 - Test whether any of bits 0..2 are set in *(*(self + 0x04) + 0x08)
std::uint32_t hasAnyLowThreeFlagsAt08_4D(const void* self) noexcept;

// 0x004D67F0 - UpdateModule[2] class-name literal VA ("UpdateModule")
std::uint32_t getUpdateModuleClassNameVa4D() noexcept;

// 0x004D6960 - Stamp `value` into self + 0x38 + i*4 for each set bit i in 0..14 of maskWords
void stampPassiveAreaEffectMaskedSlots4D(
    void* self, const std::uint32_t* maskWords, std::uint32_t value) noexcept;

// 0x004D69A0 - Test whether targetEntry's nested key matches any entry in listHeader
bool isEntryInBeamExcludeList4D(const void* targetEntry, const void* listHeader) noexcept;

// 0x004D6AB0 - Linear search of 32-byte campaign records in [self+0x0C, self+0x10) by nested key
void* findCampaignRecordByNestedKey4D(const void* self, std::uint32_t key) noexcept;

// 0x004D6FF0 - Copy DamageInfoShort (+0x04 DamageInfoInput payload plus +0x8C..+0x98)
void* assignDamageInfoShort4D(void* self, const void* other) noexcept;

// 0x004D7040 - Remove *(obj + 0xDC) from vector [self+0x24, self+0x28) via swap-with-last
bool removeObjectIdSwapLastAt24_4D(void* self, const void* obj) noexcept;

// 0x004D8390 - Resolve AIUpgradeScience target template ref by kind (1..13), fallback 0x00CA5724
std::uint32_t resolveAIUpgradeTargetTemplateRef4D(const void* self) noexcept;

// 0x004D86D0 - Bounds-checked query of flag byte +0x90 in 0x9C-byte table [tableBeginVa, tableEndVa)
bool isDamageAndSpawnTableEntryFlag90Set4D(
    const void* self, std::uint32_t tableBeginVa, std::uint32_t tableEndVa) noexcept;

// 0x004D8780 - ObjectStatus filter check against object's status mask at +0x80
bool matchesObjectStatusFilter4D(const void* filter, const void* object) noexcept;

// 0x004D8810 - Relationship filter bitmask check (ENEMIES=0, NEUTRAL=1, ALLIES=2 + same-player bit 3)
bool matchesRelationshipFilter4D(
    const void* maskPtr, const void* objA, const void* objB, std::int32_t relationship) noexcept;

// 0x004D8930 - Find contact entry with tag 0x9063FB6C in [holder+0x1A8, holder+0x1AC) and populate 40-byte goal
bool findTaggedContactMidpoint4D(const void* holder, void* outGoal40) noexcept;

// 0x004D9230 - Initialize 3x3 row-major Z-rotation matrix (36 bytes) from angle (radians)
void buildZRotationMatrix4D(void* outMatrix36, float angleRad) noexcept;

// 0x004D9280 - SWAR popcount across 9-word (288-bit) bitset
std::uint32_t countSetBits288_4D(const void* bits) noexcept;

// 0x004DA690 - Copy DOTInfoRecord (+0x04 DamageInfoInput payload plus +0x8C..+0xA8)
void* assignDOTInfoRecord4D(void* self, const void* other) noexcept;

// 0x004DA780 - LuaEventParameters default constructor (vtable 0x00BDC6E8 and 3 EventParameter slots)
void* defaultConstructLuaEventParameters4D(void* self) noexcept;

// 0x004DAD10 - Search circular doubly-linked list at gameLogic+0x1B0 for 64-bit key, returning node+8
void* findRegisteredPairByKey4D(void* gameLogic, const std::uint32_t* keyPair) noexcept;

// 0x004DB8C0 - GlobalWeatherSystem weather data default reset (0x9C bytes)
void* resetGlobalWeatherData4D(void* self) noexcept;

// 0x004DBB30 - UnitAIExitGoalState 0x24-byte entry initializer
void* initExitGoalEntry4D(
    void* self,
    std::uint32_t field00,
    std::uint32_t field04,
    std::uint32_t field08,
    std::uint32_t field0C,
    std::uint8_t flag20) noexcept;

// 0x004DBBF0 - Convert grid cell (gridX, gridY) to world position through matrix at +0xA0..+0xC0
void gridToWorldPlacement4D(
    const void* gridTransform,
    void* outWorldPos,
    std::int32_t gridX,
    std::int32_t gridY,
    std::uint8_t centerInCell) noexcept;

// 0x004DBD20 - Snap world position in-place to nearest 30-unit placement grid cell corner
void snapWorldPointToGrid4D(const void* gridTransform, void* inOutWorldPos) noexcept;

// 0x004DBEC0 - Offset world position by (deltaCellsX, deltaCellsY) in grid space, preserving world Z
void offsetWorldPointByGridCells4D(
    const void* gridTransform,
    void* outWorldPos,
    const void* inWorldPos,
    std::int32_t deltaCellsX,
    std::int32_t deltaCellsY) noexcept;

// 0x004DC080 - Convert world position to grid cell coordinates (outCellX, outCellY)
void worldToGridCell4D(
    const void* gridTransform,
    const void* worldPos,
    std::int32_t* outCellX,
    std::int32_t* outCellY,
    std::uint8_t roundFlag) noexcept;

// 0x004DD390 - MapMissionObjectives default constructor (vtable 0x00BDC9B8, +0x14 = -1)
void* defaultConstructMapMissionObjectives4D(void* self) noexcept;

// 0x004DD3D0 - CampaignSystem sub-table default constructor (vtable 0x00BDC9E0, byte +0x1C = 1)
void* defaultConstructCampaignRecordTable4D(void* self) noexcept;

// 0x004DFC30 - Snap rotated structure footprint center to placement grid
void snapRotatedFootprintToGrid4D(
    const void* gridTransform,
    const void* footprint,
    void* inOutWorldPos,
    float angleRad) noexcept;

}  // namespace ra3::gamelogic
