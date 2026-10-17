#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace ra3::gamelogic {

enum class Scripts82Status : std::uint8_t {
    reversed = 0,
    inventoryOnly = 1
};

struct Scripts82Entry {
    std::uint32_t address;
    std::uint32_t size;
    std::uint32_t crc32;
    Scripts82Status status;
    bool hookSafe;
    const char* symbol;
};

struct ScriptCoord3D {
    float x = 0.0F;
    float y = 0.0F;
    float z = 0.0F;
};

// Shipped Parameter layout (size 0x30 = 48 bytes; 0x0081B130, 0x0081A750, 0x00820630).
struct ScriptParameterView {
    std::int32_t paramType = 0;           // +0x00 (0x0B = SIDE, 0x10 = COORD3D)
    std::uint8_t initialized = 0;         // +0x04
    std::uint8_t pad05[3]{};              // +0x05..+0x07
    std::int32_t intValue = 0;            // +0x08
    float realValue = 0.0F;               // +0x0C
    std::string stringValue{};            // +0x10 (AsciiString in binary)
    ScriptCoord3D coord{};                // +0x14..+0x1C
    std::uint32_t objStatusLo = 0;        // +0x20
    std::uint32_t objStatusHi = 0;        // +0x24
    std::uint32_t templateHandle0 = 0;    // +0x28
    std::uint32_t templateHandle1 = 0;    // +0x2C
};

// Shipped ActionTemplate parameter layout (stride 0x80 = 128 bytes at ScriptEngine + 0x48).
struct ScriptActionTemplateView {
    std::int32_t numParams = 0;                     // +0x48
    std::array<std::int32_t, 12> paramTypes{};      // +0x4C..+0x78
};

// Shipped ScriptAction layout (vtable 0x00C35D44, size 0x48 = 72 bytes).
struct ScriptActionView {
    std::uint32_t vtable = 0x00C35D44U;             // +0x00
    std::int32_t actionType = 0;                    // +0x04
    std::int32_t numParams = 0;                     // +0x08
    std::array<ScriptParameterView, 12> params{};   // +0x0C..+0x38
    std::array<bool, 12> paramPresent{};
    std::uint32_t nextAction = 0;                   // +0x3C
    std::uint8_t hasWarnings = 0;                   // +0x40
    std::uint8_t isEnabled = 1;                     // +0x41
    std::uint32_t customData = 0;                   // +0x44
};

// Shipped Script layout (vtable 0x00C4D57C, size 0x50 = 80 bytes; 0x0081C3E0, 0x0081A420).
struct ScriptView {
    std::uint32_t vtable = 0x00C4D57CU;             // +0x00
    std::uint32_t scriptName = 0;                   // +0x04
    std::uint32_t comment = 0;                      // +0x08
    std::uint32_t conditionComment = 0;             // +0x0C
    std::uint8_t flag10 = 0;                        // +0x10
    std::uint8_t flag11 = 0;                        // +0x11
    std::int32_t delayEvaluationSeconds = 0;        // +0x14
    bool isActive = true;                           // +0x18
    std::uint32_t field1C = 0;                      // +0x1C
    std::uint32_t field20 = 0;                      // +0x20
    bool isOneShot = true;                          // +0x24
    bool isSubroutine = true;                       // +0x25
    bool flag26 = false;                            // +0x26
    bool easy = true;                               // +0x27
    bool normal = true;                             // +0x28
    bool hard = true;                               // +0x29
    std::uint32_t condition = 0;                    // +0x2C
    std::uint32_t action = 0;                       // +0x30
    std::uint32_t actionFalse = 0;                  // +0x34
    std::uint32_t frameToEvaluateAt = 0;            // +0x38
    bool isScriptActiveRuntime = true;              // +0x3C
    std::uint8_t flag3D = 0;                        // +0x3D
    std::uint32_t conditionTeamName = 0;            // +0x40
    float conditionTime = 0.0F;                     // +0x44
    float curTime = 0.0F;                           // +0x48
    std::uint32_t nextScript = 0;                   // +0x4C
};

// Shipped SequentialScript layout (vtable 0x00C4D4F4, size 0x2C = 44 bytes; 0x0081BE00).
struct SequentialScriptView {
    std::uint32_t vtable = 0x00C4D4F4U;             // +0x00
    std::uint32_t teamToExecOn = 0;                 // +0x04
    std::uint32_t objectId = 0;                     // +0x08
    std::uint32_t scriptName = 0;                   // +0x0C
    std::uint32_t field10 = 0;                      // +0x10
    std::uint32_t scriptToExecuteSequentially = 0;  // +0x14
    std::int32_t currentInstruction = -1;           // +0x18
    std::int32_t timesToLoop = 0;                   // +0x1C
    std::int32_t framesToWait = -1;                 // +0x20
    std::uint8_t dontAdvanceInstruction = 0;        // +0x24
    std::uint32_t nextScriptInSequence = 0;         // +0x28
};

// 16-byte SightingKey (0x0081AA60, 0x0081AAA0, 0x0081AB60, 0x0081FD60).
struct SightingKey {
    std::uint32_t word0 = 0;  // +0x00
    std::uint32_t word1 = 0;  // +0x04
    std::uint32_t mask8 = 0;  // +0x08
    std::uint32_t maskC = 0;  // +0x0C
};

// 8-byte DualHashKey (0x0081DF50, 0x0081FA00).
struct DualHashKey {
    std::uint32_t prefixHash = 0;  // +0x00
    std::uint32_t nameHash = 0;    // +0x04
};

// 12-byte ScriptIndexedEntry (0x0081DE20).
struct ScriptIndexedEntry {
    std::int32_t id = 0;       // +0x00
    std::uint32_t name = 0;    // +0x04
    std::uint32_t flags = 0;   // +0x08
};

// 36-byte ScriptWaypointInfo (0x0081DFC0).
struct ScriptWaypointInfo {
    std::uint32_t word00 = 0;       // +0x00
    std::uint32_t word04 = 0;       // +0x04
    std::uint32_t word08 = 0;       // +0x08
    std::uint8_t byte0C = 0;        // +0x0C
    std::uint8_t pad0D[3]{};        // +0x0D..+0x0F
    std::uint32_t word10 = 0;       // +0x10
    float float14 = 0.0F;           // +0x14
    float float18 = 0.0F;           // +0x18
    float float1C = 0.0F;           // +0x1C
    std::uint32_t word20 = 0;       // +0x20
};

// Tracked named-object entry at ScriptEngine + 0x17D74..0x17D78 (0x0081A4A0, 0x0081A5B0, 0x0081BFF0).
struct TrackedNamedObjectEntry {
    std::uint32_t nameHash = 0;     // +0x00
    std::uint32_t objectPtr = 0;    // +0x04
};

// AttackInfo entry at ScriptEngine + 0x16D64 + idx * 0x10 (0x0081FB40).
struct ScriptAttackInfoEntry {
    std::uint32_t value0 = 0;       // +0x00 (entry base + 0x00)
    std::string scriptName{};       // +0x04 (entry base + 0x04)
    std::uint32_t value8 = 0;       // +0x08
    std::uint32_t valueC = 0;       // +0x0C
};

// SpecialPower event node in ScriptEngine player list (0x008201B0).
struct SpecialPowerEventNode {
    std::uint32_t hash0 = 0;            // +0x04 (unused by 0x008201B0)
    std::uint32_t powerHash = 0;        // +0x08
    std::uint32_t sourceObjectId = 0;   // +0x0C
    ScriptCoord3D targetCoord{};        // +0x10..+0x18
    bool hasTargetCoord = false;        // +0x1C
    bool pointInsideTrigger = false;
};

// 1. 0x00804590: ScriptAction::isUnitAction
bool isUnitAction(std::int32_t actionType) noexcept;
std::uint32_t isUnitActionEax(std::int32_t actionType) noexcept;

// 2. 0x00804780: ScriptAction::isTeamAction
bool isTeamAction(std::int32_t actionType) noexcept;
std::uint32_t isTeamActionEax(std::int32_t actionType) noexcept;

// 3. 0x008049A0: ScriptAction::isTeamOrUnitAction
bool isTeamOrUnitAction(std::int32_t actionType) noexcept;
std::uint32_t isTeamOrUnitActionEax(std::int32_t actionType) noexcept;

// 4. 0x0081A420: ScriptEngine::canEvaluateScript
bool canEvaluateScript(
    ScriptView& script,
    std::int32_t engineDifficulty,
    bool hasCurrentPlayer,
    bool currentPlayerHasAi,
    std::int32_t aiDifficulty,
    std::uint32_t currentFrame) noexcept;

// 5. 0x0081A4A0: ScriptEngine::clearTrackedObjectPointer
void clearTrackedObjectPointer(
    std::vector<TrackedNamedObjectEntry>& entries,
    std::uint32_t objectPtr) noexcept;

// 6. 0x0081A4D0: ScriptEngine::setSequentialTimerForObject
void setSequentialTimerForObject(
    std::vector<SequentialScriptView>& seqScripts,
    bool objectPresent,
    std::uint32_t objectId,
    std::int32_t frameCount) noexcept;

// 7. 0x0081A520: ScriptEngine::setSequentialTimerForTeam
void setSequentialTimerForTeam(
    std::vector<SequentialScriptView>& seqScripts,
    std::uint32_t teamPtr,
    std::int32_t frameCount) noexcept;

// 8. 0x0081A5B0: ScriptEngine::notifyOfObjectDestruction
void notifyOfObjectDestruction(
    std::vector<TrackedNamedObjectEntry>& entries,
    std::uint32_t& cachedObject17D70,
    std::uint32_t& cachedObject17D68,
    std::uint32_t destroyedObjectPtr) noexcept;

// 9. 0x0081A700: ScriptList::markAllScriptsAndGroupsActive
struct ScriptChainNode {
    std::int32_t nextIndex = -1;  // +0x00
    std::uint8_t activeFlag = 0;  // +0x0C
};
void markAllScriptsAndGroupsActive(
    std::vector<ScriptChainNode>& scripts,
    std::int32_t firstScriptIndex,
    std::vector<ScriptChainNode>& groups,
    std::int32_t firstGroupIndex) noexcept;

// 10. 0x0081A750: Parameter::getCoord3D
void getParameterCoord3D(
    const ScriptParameterView& param,
    ScriptCoord3D& outCoord) noexcept;

// 11. 0x0081AA40: IntVector::sumElements
std::uint32_t sumIntVectorElements(
    const std::int32_t* begin,
    const std::int32_t* end) noexcept;

// 12. 0x0081AA60: SightingKey::less
bool sightingKeyLess(
    const SightingKey& lhs,
    const SightingKey& rhs) noexcept;

// 13 & 14. 0x0081AAA0 & 0x0081AB60: SightingTree::find & SightingTree::lowerBound
// Operates on raw in-memory RB-tree layout (root at +0x0C, anchor at +0x04, key at node + 0x10).
void* sightingTreeLowerBoundRaw(
    void* treeBase,
    const SightingKey& key) noexcept;
void* sightingTreeFindRaw(
    void* treeBase,
    const SightingKey& key) noexcept;

// 15. 0x0081B130: Parameter::Parameter(ParameterType, int)
ScriptParameterView initParameter(
    std::int32_t paramType,
    std::int32_t intValue) noexcept;

// 16. 0x0081BE00: SequentialScript::SequentialScript()
SequentialScriptView initSequentialScript() noexcept;

// 17. 0x0081BF90: ScriptCounterDefaultMap::lookupOrDefault
double lookupCounterDefaultMap(
    std::int32_t defaultValue,
    bool hasTree,
    const std::vector<std::pair<std::uint32_t, std::int32_t>>& sortedMap,
    std::uint32_t key) noexcept;

// 18. 0x0081BFF0: ScriptEngine::isTrackedObjectDestroyedByName
std::uint32_t fnv1a32(std::string_view text) noexcept;
bool isTrackedObjectDestroyedByName(
    const std::vector<TrackedNamedObjectEntry>& entries,
    std::string_view unitName) noexcept;

// 19. 0x0081C3E0: Script::Script()
ScriptView initScript() noexcept;

// 20. 0x0081DE20: ScriptIndexedEntry::lowerBoundById
const ScriptIndexedEntry* lowerBoundScriptIndexedEntry(
    const ScriptIndexedEntry* first,
    const ScriptIndexedEntry* last,
    std::int32_t key) noexcept;

// 21. 0x0081DE70: ScriptListIndex::lowerBoundByName
int compareAsciiStrings(std::string_view lhs, std::string_view rhs) noexcept;
std::int32_t lowerBoundScriptListByName(
    const std::vector<std::int32_t>& sortedIndices,
    const std::vector<std::string>& entryNames,
    std::string_view targetName) noexcept;

// 22. 0x0081DF50: DualHashTree::find
void* dualHashTreeFindRaw(
    void* treeBase,
    const DualHashKey& key) noexcept;

// 23. 0x0081DFC0: ScriptWaypointInfo::copyFrom
void copyScriptWaypointInfo(
    ScriptWaypointInfo& dst,
    const ScriptWaypointInfo& src) noexcept;

// 24. 0x0081F710: ScriptConditions::evaluateSkirmishCommandButtonIsReady
std::uint32_t crc32Ra3String(std::string_view text) noexcept;
bool evaluateSkirmishCommandButtonIsReady(
    std::int32_t& conditionCustomData,
    std::string_view buttonName,
    bool gameLogicFlagA8,
    const std::vector<std::pair<std::uint32_t, std::uint32_t>>& readyPairs,
    bool& outRegisteredFirstTime) noexcept;

// 25. 0x0081F7B0: ScriptConditions::evaluateBridgeBroken
struct WaypointStub {
    std::int32_t id = 0;
    std::string name{};
};
bool evaluateBridgeBroken(
    std::string_view bridgeWaypointName,
    const std::vector<WaypointStub>& waypoints,
    const std::vector<std::pair<std::int32_t, std::int32_t>>& bridgeDamageStates) noexcept;

// 26. 0x0081F8A0: ScriptConditions::evaluatePlatform
bool evaluatePlatform(std::string_view platformName) noexcept;

// 27 & 28. 0x0081FA00 & 0x0081FAE0: findFlagByName & qualifyWithCurrentSidePrefix
void splitSidePrefix(
    std::string_view currentSidePrefix,
    std::string& inOutName,
    std::string& outPrefix);
std::string qualifyWithCurrentSidePrefix(
    std::string_view currentSidePrefix,
    std::string_view rawName);
DualHashKey computeFlagLookupKey(
    std::string_view currentSidePrefix,
    std::string_view flagName);

// 29. 0x0081FB40: ScriptEngine::findAttackInfo
// Returns 1-based index in [1..count] when found or newly added, or 0 if not found / full.
std::int32_t findAttackInfoIndex(
    std::vector<ScriptAttackInfoEntry>& entries,
    std::int32_t& currentCount,
    std::string_view attackName,
    bool addIfNotFound);

// 30. 0x0081FC40: ScriptEngine::getObjectCount
std::int32_t getScriptObjectCount(
    bool playerValid,
    std::string_view objectTypeName,
    const std::vector<std::pair<std::uint32_t, std::int32_t>>& playerObjectCounts) noexcept;

// 31. 0x0081FD60: ScriptEngine::hasSightingEntry
bool hasSightingEntryRaw(
    void* sightingTreeBase,
    const SightingKey& key) noexcept;

// 32. 0x008201B0: ScriptEngine::isSpecialPowerTriggeredOrComplete
bool isSpecialPowerTriggeredOrComplete(
    std::uint32_t playerIndex,
    std::uint32_t powerFastHash,
    bool eraseIfFound,
    std::uint32_t requiredSourceObjId,
    bool hasTriggerFilter,
    std::vector<SpecialPowerEventNode>& events,
    bool& outErasedNextLink) noexcept;

// 33. 0x00820280: ScriptEngine::isScienceAcquiredOrUpgradeComplete
bool isScienceAcquiredOrUpgradeComplete(
    std::uint32_t playerIndex,
    std::uint32_t itemFastHash,
    bool eraseIfFound,
    std::vector<std::uint32_t>& eventHashes,
    bool& outErasedNextLink) noexcept;

// 34. 0x00820630: Parameter::qualifyPlayerSide
void qualifyPlayerSide(ScriptParameterView& param);

// 35. 0x00820B70: ScriptAction::setActionType
void setScriptActionType(
    ScriptActionView& action,
    std::int32_t actionType,
    const std::vector<ScriptActionTemplateView>& templates);

// 36. 0x00820E00: ScriptAction::getTeamInvolved
// Returns -1 if action does not involve a team, 0 if it uses m_parms[0], or 1 if it uses m_parms[1].
std::int32_t getTeamInvolvedParamIndex(std::int32_t actionType) noexcept;
std::string getTeamInvolvedName(
    const ScriptActionView& action,
    bool& outInvolved);

const std::array<Scripts82Entry, 64>& scripts82Inventory() noexcept;

}  // namespace ra3::gamelogic
