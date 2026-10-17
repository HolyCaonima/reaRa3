#pragma once

#include <cstddef>
#include <cstdint>

namespace ra3::gamelogic {

// 0x00695550 (inner-patched, cave 0x004037D6 -> 0x0069556D)
// Base constructor of SpecialPowerModule (vtable 0x00C01108, size 0x4C).
struct BehaviorModule00C01108Layout {
    std::uint32_t words[19];
};

std::uint32_t constructBehaviorModule00C01108(
    std::uint32_t selfPtr,
    std::uint32_t thingPtr,
    std::uint32_t resolvedOwnerPtr,
    std::uint32_t moduleDataPtr,
    BehaviorModule00C01108Layout& out);

// 0x00699780 (entry-recoverable, cave 0x00449506 -> 0x00699786)
// Checks AI sub-controller state ([ecx+0x21c] == 1 or 4) and delegates to 0x00631C20([ecx+0x150]).
bool hasActiveSubController(
    std::uint32_t state21C,
    bool hasSubController,
    std::uint32_t ptr10,
    std::uint32_t ptr24,
    std::int32_t& cachedState38,
    std::uint32_t firstNodeTag20);

// 0x0069E780 (inner-patched, cave 0x00451D81 -> 0x0069E794)
// Evaluates partition / modifier count from spec mode ([arg+0x0C] == 1 or 2) and optional divisor.
std::int32_t computePartitionModifierCount(
    bool hasOwner,
    bool hasControllingPlayer,
    bool hasSpec,
    std::uint32_t specMode,
    std::int32_t mode1Count,
    std::int32_t mode2Count,
    bool hasModifierList,
    bool modifierApplied,
    float modifierDivisor);

// 0x006A51A0 (entry-recoverable, cave 0x0043A336 -> 0x006A51A6)
// Forwards sub-module query to [ecx+0x37c] via 0x006A1720(0, targetTag).
struct SubModuleEntry {
    std::uint32_t modulePtr;
    std::int32_t tag;
    std::uint32_t wildcardPredicateResult;
    std::uint32_t interfaceResult;
};

std::uint32_t queryObjectSubModule(
    bool hasContainer37C,
    const SubModuleEntry* entries,
    std::size_t entryCount,
    std::int32_t targetTag);

// 0x006A5FA0 (inner-patched, cave 0x0043A766 -> 0x006A5FAA)
// Evaluates object status/template/filter eligibility flags and status bits 2/6 via 0x004D66A0.
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
    std::uint8_t modeFlag);

// 0x006A6460 (inner-patched, cave 0x0043A117 -> 0x006A646A)
// Unregisters an Object from partition/radar/player/audio subsystems using flags at +0x4D8 and +0x4D7.
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
    std::uint32_t& outCallMask);

// 0x006A6DC0 (inner-patched, cave 0x00450629 -> 0x006A6DC8)
// Computes 3D per-frame position delta from flags +0x236/+0x234/+0x235 and frame timestamps.
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
    float outDelta[3]);

// 0x006A7600 (entry-recoverable, cave 0x004236A0 -> 0x006A7606)
// Returns [ecx+0x328] if non-null, else *[ecx+0x32c] if [ecx+0x32c] != [ecx+0x330], else 0.
std::uint32_t getObjectControllingPlayer(
    std::uint32_t directPtr328,
    std::uint32_t listBegin32C,
    std::uint32_t listEnd330,
    std::uint32_t firstListElem);

// 0x006A8400 (entry-recoverable, cave 0x004480D6 -> 0x006A8406)
// Selects event [ecx+0xBC] when state1C < threshold70 else [ecx+0xC0] and dispatches audio/EVA event.
void notifyActiveBodyMaxHealthChanged(
    std::uint32_t state1C,
    std::uint32_t threshold70,
    std::uint32_t eventBC,
    std::uint32_t eventC0,
    std::uint32_t ownerId0C,
    std::uint32_t& outSelectedEvent,
    std::uint32_t& outBoundOwnerId,
    bool& outDispatched);

// 0x006B7040 (inner-patched, cave 0x00417391 -> 0x006B704B)
// Releases old target ref at [edi+0x150], clears +0x428/+0x432, binds new target ref, and invokes 0x006B6900.
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
    bool& helper6B6900Called);

// 0x006BB360 (inner-patched, cave 0x0044F3B9 -> 0x006BB36F)
// Builds target surface/kind classification bitmask from ThingTemplate kindOf and Object status bits.
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
    std::uint32_t& outMask);

// 0x006BD510 (inner-patched, cave 0x00454226 -> 0x006BD526)
// Classifies target via 0x006BB360 and tests against forbidden masks at [moduleData+0xD0] and [moduleData+0xD4].
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
    std::uint32_t forbiddenMaskD4);

// 0x006C0620 (inner-patched, cave 0x00416306 -> 0x006C0629)
// Resolves Object indicator ARGB color ([esi+0x424] override, SpecialDisguiseUpdate color, or team player color).
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
    std::uint32_t teamPlayerColor80);

// 0x006C0B60 (inner-patched, cave 0x004435E8 -> 0x006C0B69)
// Sets bit 0 of [esi+0xC0] when argPlayer == [esi+0x418], marks object dirty, and notifies module +0x100 listeners.
void setObjectBit31AndNotifyModules(
    std::uint32_t& flagsC0,
    std::uint8_t flagsB3,
    std::uint32_t argPlayer,
    std::uint32_t ownerPlayer418,
    const std::uint32_t* moduleListenerFlags,
    std::size_t moduleCount,
    bool& markedDirty,
    std::uint32_t& notifiedCount);

// 0x006C1500 (inner-patched, cave 0x00402716 -> 0x006C150B)
// Looks up a behavior module in the RB-tree at [esi+0x3AC] by key (or returns sentinel head for key == -1).
struct ModuleTreeEntry {
    std::int32_t key;
    std::uint32_t modulePtr;
};

std::uint32_t findObjectModuleByKey(
    bool hasModuleArray310,
    std::uint32_t treeCount3C0,
    std::uint32_t defaultModulePtr,
    const ModuleTreeEntry* entries,
    std::size_t entryCount,
    std::int32_t searchKey);

// 0x006C1D60 (inner-patched, cave 0x00445A23 -> 0x006C1D69)
// Dispatches player reassignment through container/transport AI ([+0x84]->[+0x1D8]) or 0x00687B80.
void reassignObjectControllingPlayer(
    std::uint32_t directPtr328,
    std::uint32_t listBegin32C,
    std::uint32_t listEnd330,
    std::uint32_t firstListElem,
    bool subObjHasAI84,
    std::uint32_t& outActionMask);

// 0x006C2C30 (inner-patched, cave 0x0040AEDF -> 0x006C2C39)
// Synchronizes Object shroud/vision registration bit ([eax+0x471] & 0x20) and calls 0x007F55B0 / 0x007F5620.
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
    std::uint8_t& flags471);

// 0x006C4F10 (inner-patched, cave 0x004128BB -> 0x006C4F5B)
// Constructs command message 0 with 3D target position, dispatches to vtable[0], and runs cave teardown.
void dispatchModuleCommandMessage16(
    const float targetPos[3],
    std::int32_t& refCount50,
    bool& refDestroyed,
    bool hasBuffer2C,
    bool hasBuffer1C,
    float outDispatchedPos[3],
    std::uint32_t& outFinalVtable,
    std::uint32_t& outFreedMask);

// 0x006C5480 (inner-patched, cave 0x0040B54B -> 0x006C54BB)
// Constructs command message 0x3D with 32-bit payload, dispatches to vtable[0], and runs cave teardown.
void dispatchModuleCommandMessage3D(
    std::uint32_t commandArg,
    std::int32_t& refCount50,
    bool& refDestroyed,
    bool hasBuffer2C,
    bool hasBuffer1C,
    std::uint32_t& outDispatchedType,
    std::uint32_t& outDispatchedArg,
    std::uint32_t& outFinalVtable,
    std::uint32_t& outFreedMask);

// 0x006C6310 (inner-patched, cave 0x00447521 -> 0x006C631A)
// Clears bit 15 (0x8000) of [obj+0xB0] and calls 0x006A4FB0 if it was set; returns false.
bool clearObjectStatusBit15AndMarkDirty(
    std::uint32_t& flagsB0,
    bool& markedDirty);

// 0x006C8380 (inner-patched, cave 0x0043AE66 -> 0x006C838A) & 0x0073C740 (inner-patched, cave 0x00429A56 -> 0x0073C75C)
// Constructor and destructor for UpdateModule 0x00C11AA0 (size 0x124, 73 uint32_t words).
struct UpdateModule00C11AA0Layout {
    std::uint32_t words[73];
};

std::uint32_t constructUpdateModule00C11AA0(
    std::uint32_t selfPtr,
    std::uint32_t thingPtr,
    std::uint32_t resolvedOwnerPtr,
    std::uint32_t moduleDataPtr,
    float moduleDataFloat08,
    float moduleDataFloat14,
    UpdateModule00C11AA0Layout& out,
    bool& helper6906E0Called,
    bool& helper7221C0Called);

void destroyUpdateModule00C11AA0(
    std::uint32_t selfPtr,
    UpdateModule00C11AA0Layout& out,
    std::uint32_t& freedMask,
    bool& unlinkedIntrusiveNode);

// 0x006CACE0 (inner-patched, cave 0x00438E56 -> 0x006CACEC)
// Virtual eligibility check at vtable 0x00C00F30+0x50 (0x00C00F80).
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
    bool subModVtable18Ok);

// 0x006D0330 (entry-recoverable, cave 0x00402576 -> 0x006D0336)
// Updates sub-object [eax+0x1C] from limit [[[eax+4]+4]+0x70] and dispatches 0x006A8400 or vtable[+0x1C](5).
std::uint32_t dispatchActiveBodyMaxHealthUpdate(
    std::int32_t limit70,
    std::int32_t& val1C);

// 0x006D1DD0 (inner-patched, cave 0x004180A0 -> 0x006D1DDA)
// Broadcasts visibility changes across [ebx+0x310] modules, shroud 0x006C2C30, drawable lists, and 0x006A4D20.
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
    bool& outCalledTail6A4D20);

// 0x006E4760 (inner-patched, cave 0x00442ACF -> 0x006E4774)
// Resolves Object by ID under lock, validates alive/kindOf/status flags, and queries module +0x44 interface.
std::uint32_t lookupObjectModuleInterface44ById(
    std::uint32_t objectId,
    std::uint32_t foundObjectPtr,
    std::uint8_t flags471,
    std::uint32_t tmplFlagsC4,
    std::uint32_t flags80,
    const std::uint32_t* moduleInterface44Results,
    std::size_t moduleCount,
    std::uint32_t& outObjectPtr);

// 0x006E5CE0 (inner-patched, cave 0x00456E77 -> 0x006E5CE8)
// Clears [ecx+0x26c] if non-zero.
void clearObjectStatusWord0(std::uint32_t& statusWord26C);

// 0x006EE7B0 (inner-patched, cave 0x0043AF76 -> 0x006EE7B9)
// Resolves template handle via [0x00DEBA5C], updates float at +0x0C and cached pointer at +0x24.
void refreshObjectTemplateRegistration(
    std::uint32_t handle4E20E0,
    std::uint32_t obj4F13B0,
    std::int32_t intVal8D6970,
    std::uint32_t newValA7C770,
    float& outFloat0C,
    std::uint32_t& ioField24,
    bool& outVtable10Called,
    bool& outHelper6D17F0Called);

// 0x006EFE40 (inner-patched, cave 0x00401F36 -> 0x006EFE49)
// Looks up producer Object by ID [ecx+0x3EC] in the global object hash table.
std::uint32_t lookupObjectProducerById(
    std::uint32_t producerId3EC,
    std::uint32_t hashLookupResultPtr);

// 0x006F1350 (inner-patched, cave 0x00456F6B -> 0x006F135C)
// Evaluates whether an Object is selectable based on producer team, status/template flags, and modules.
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
    bool mod687880Vtable1C);

// 0x0072CF30 (inner-patched, cave 0x0040A771 -> 0x0072CF3D)
// Dispatches command 0x1B with 3-float tint vector to owner's Drawable ([owner+0xF4]).
bool applyBehaviorDrawableTintEffect(
    bool hasOwner,
    bool hasDrawableF4,
    const float tintVec[3],
    float outAppliedTint[3],
    std::uint32_t& outCommandId);

// 0x00731530 (inner-patched, cave 0x0043A596 -> 0x0073153B)
// Registers active module in 0x00DF8A50 if [esi+0x138] == 0, appends listener, and updates player counter +0x1318.
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
    std::int32_t& playerCounter1318);

// 0x00734490 (inner-patched, cave 0x00457E56 -> 0x0073449A)
// Appends up to 6 non-null weapon slot pointers at [ebp+0x94] when signed byte [ebp+0xA4] > 0.
bool gatherActiveWeaponSlots(
    std::int8_t activeCountA4,
    const std::uint32_t slotPtrs[6],
    std::uint32_t* outVector,
    std::uint32_t& outCount,
    std::uint32_t maxCapacity);

// 0x00741140 (inner-patched, cave 0x0043A4B6 -> 0x0074114A)
// Populates weapon modifier cache vector at [ebx+0x230] across 18 slots when initially empty.
struct WeaponModifierPair {
    std::uint32_t itemPtr;
    float value;
};

std::uint32_t ensureObjectWeaponModifierCache(
    std::uint32_t selfPtr,
    const WeaponModifierPair* incomingPairs,
    std::size_t incomingCount,
    WeaponModifierPair* cacheBuffer,
    std::uint32_t& cacheCount,
    std::uint32_t cacheCapacity);

// 0x007451E0 (inner-patched, cave 0x0044AFD1 -> 0x007451FD)
// Calls 0x00734380 and iterates 6 pointer-range buckets at [edi+0x34] notifying 0x006A8490 and vtable[+0x48].
void notifySixBucketObjectListeners(
    const std::uint32_t* const bucketArrays[6],
    const std::size_t bucketCounts[6],
    std::uint32_t* outNotifiedObjects,
    std::uint32_t& outNotifiedCount,
    std::uint32_t maxOutput);

// 0x007461C0 (inner-patched, cave 0x0040E35C -> 0x007461D4)
// Copies 12 transform floats from [src+0x08..0x34] to [esi+0x70..0x9C] and invokes 0x006A4540 + 0x00735DB0.
bool updateBehaviorTransformMatrix(
    std::uint32_t flags40,
    std::uint8_t flagA2,
    float float38,
    const float srcMatrix[12],
    float dstMatrix[12]);

// 0x00747920 (inner-patched, cave 0x00454BB1 -> 0x0074792E)
// Updates old/new player counter +0x1318 and team lists when [esi+0x414] changes and template bit +0xE0:10 is set.
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
    bool& outCalled736890);

// 0x00747F10 (inner-patched, cave 0x00401F76 -> 0x00747F19)
// Checks AI idle eligibility ([esi+0x374]->vtable[+0x214], +0x471, +0x80:30, +0x378, 0x00638B70) and calls 0x00747C10.
bool checkAndHandleObjectAIIdleState(
    bool hasAI374,
    bool aiVtable214Busy,
    std::uint8_t flags471,
    std::uint32_t flags80,
    std::uint8_t allowBit30,
    bool hasSub378,
    std::uint8_t sub378FlagE0,
    bool helper638B70Ok);

}  // namespace ra3::gamelogic
