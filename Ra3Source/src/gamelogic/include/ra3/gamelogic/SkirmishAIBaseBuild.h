#pragma once

#include <cstddef>
#include <cstdint>

namespace ra3::gamelogic::skirmishai_base_build {

// ============================================================================
// Shipped global / vtable / constant addresses in ra3_1.0.game
// ============================================================================

inline constexpr std::uintptr_t kTheGameLogicSlotAddr = 0x00DEBA64u;
inline constexpr std::uintptr_t kTheThingFactorySlotAddr = 0x00DF9D28u;
inline constexpr std::uintptr_t kTheTerrainLogicSlotAddr = 0x00DFC190u;
inline constexpr std::uintptr_t kThePowerOverrideMgrSlotAddr = 0x00DFC624u;
inline constexpr std::uintptr_t kFramesPerSecondAddr = 0x00DEEEE8u;

inline constexpr std::uint32_t kVtableAIBuildableEntry = 0x00C51D00u;
inline constexpr std::uint32_t kVtableAIEconomyStatsSubobject = 0x00C51E08u;
inline constexpr std::uint32_t kVtableAIBuildableStructureTracker = 0x00C51EB4u;
inline constexpr std::uint32_t kVtableAIBuildHeuristicBase = 0x00C52188u;
inline constexpr std::uint32_t kVtableAIBuildHeuristicStore = 0x00C52338u;
inline constexpr std::uint32_t kEmptyAsciiStringBufAddr = 0x00D0FEECu;

// ============================================================================
// Common data views
// ============================================================================

struct Coord3DView {
    float x;
    float y;
    float z;
};
static_assert(sizeof(Coord3DView) == 0x0C);

struct AIBudgetStateEntryView {
    void* stateAt0;
    float scoreAt4;
    std::uint32_t fieldAt8;
};
static_assert(sizeof(AIBudgetStateEntryView) == 0x0C);

struct AIBudgetStateManagerView {
    std::uint32_t vftableAt0;
    AIBudgetStateEntryView* beginAt4;
    AIBudgetStateEntryView* endAt8;
    AIBudgetStateEntryView* capAtC;
    void* bankOwnerAt10;
    std::uint8_t hasPositiveScoreAt14;
    std::uint8_t pad15[3];
    std::uint32_t nextEvalFrameAt18;
    float evalIntervalSecondsAt1C;
};
static_assert(sizeof(AIBudgetStateManagerView) == 0x20);

struct AIHeuristicCallNodeView {
    AIHeuristicCallNodeView* nextAt0;
    AIHeuristicCallNodeView* prevAt4;
    void* heuristicAt8;
};
static_assert(sizeof(AIHeuristicCallNodeView) == 0x0C);

struct AIBudgetStateView {
    std::uint32_t fieldAt0;
    AIHeuristicCallNodeView* nextAt4;
    AIHeuristicCallNodeView* prevAt8;
};
static_assert(sizeof(AIBudgetStateView) == 0x0C);

struct AIBankRequestNodeView {
    AIBankRequestNodeView* nextAt0;
    AIBankRequestNodeView* prevAt4;
    std::uint32_t fieldAt8;
    std::uint32_t fieldAt0C;
    std::uint32_t fieldAt10;
    float priorityAt14;
    float costAt18;
};
static_assert(sizeof(AIBankRequestNodeView) == 0x1C);

struct AIBankAccountView {
    std::uint32_t vftableAt0;
    void* bankAt4;
    std::uint32_t accountIndexAt8;
    float budgetWeightAt0C;
    AIBankRequestNodeView* requestListNextAt10;
    AIBankRequestNodeView* requestListPrevAt14;
    std::uint32_t reservedAt18;
    float spentAt1C;
    float availableAt20;
};
static_assert(sizeof(AIBankAccountView) == 0x24);

struct BitmaskFloatEntryView {
    float valueAt0;
    std::uint32_t maskAt4;
};
static_assert(sizeof(BitmaskFloatEntryView) == 0x08);

struct AIBaseView {
    std::uint32_t vftableAt0;
    std::int32_t baseIdAt4;
    void* aiPlayerAt8;
    std::uint32_t centerObjectIdAt0C;
    Coord3DView fallbackPositionAt10;
    Coord3DView directionAt1C;
    float radiusAt28;
    std::uint32_t flagsAt2C;
    std::uint32_t* structuresBeginAt30;
    std::uint32_t* structuresEndAt34;
    std::uint32_t* structuresCapAt38;
    std::uint32_t reservedAt3C;
    std::uint32_t metricAt40;
};
static_assert(sizeof(AIBaseView) == 0x44);

struct AIBaseQueryCriteriaConfigView {
    std::uint32_t requiredMaskAt0;
    std::uint32_t forbiddenMaskAt4;
    std::uint32_t preferredMaskAt8;
    std::uint32_t dislikedMaskAtC;
    std::uint32_t sortModeAt10;
};
static_assert(sizeof(AIBaseQueryCriteriaConfigView) == 0x14);

struct AIBaseQueryCriteriaView {
    std::uint32_t requiredMaskAt0;
    std::uint32_t forbiddenMaskAt4;
    std::uint32_t preferredMaskAt8;
    std::uint32_t dislikedMaskAtC;
    std::uint32_t sortModeAt10;
    const Coord3DView* referencePosAt14;
};
static_assert(sizeof(AIBaseQueryCriteriaView) == 0x18);

struct RbTreeNodeView {
    RbTreeNodeView* rightAt0;
    RbTreeNodeView* leftAt4;
    RbTreeNodeView* parentAt8;
    std::uint32_t flagsOrColorAtC;
    std::uint32_t keyAt10;
    std::uint32_t valueAt14;
    std::uint32_t auxValueAt18;
    RbTreeNodeView* nestedLeftmostAt1C;
    std::uint32_t pad20;
    std::uint32_t pad24;
    std::uint32_t blockedFlagAt28;
    std::uint8_t pad2C;
    std::uint8_t isSentinelAt2D;
    std::uint8_t pad2E[2];
};
static_assert(sizeof(RbTreeNodeView) == 0x30);

struct AIBuildableIteratorView {
    RbTreeNodeView* nodeAt0;
    std::uint32_t keyAt4;
    void* ownerMapAt8;
};
static_assert(sizeof(AIBuildableIteratorView) == 0x0C);

struct AIBuildableQueueView {
    std::uint32_t enabledAt0;
    std::uint32_t* cursorAt4;
    std::uint32_t* baseAt8;
    std::uint8_t exhaustedAt0C;
};

struct EconomySampleView {
    std::uint32_t frameAt0;
    float valueAt4;
};
static_assert(sizeof(EconomySampleView) == 0x08);

// ============================================================================
// 56 Reconstructed SkirmishAI Base, Economy & Build-Planning Functions
// ============================================================================

// 1. 0x00881130 — AIBudgetStateManager::normalizeStateScores
void normalizeBudgetStateScores_881130(AIBudgetStateManagerView* self) noexcept;

// 2. 0x00882190 — AIBuildable::resetBuildPosition
void resetBuildPosition_882190(void* self, Coord3DView* outPos) noexcept;

// 3. 0x00882400 — AIBuildableStructure::setBuildPosition
void setBuildPositionWithTerrainHeight_882400(
    void* self,
    const Coord3DView* pos,
    void* terrainLogicOverride = nullptr) noexcept;

// 4. 0x00882A90 — AIBuildPersonalityConfig::findBitmaskFloatOrZero
float findBitmaskFloatOrZero_882A90(const void* self, std::uint32_t bitIndex) noexcept;

// 5. 0x00882AE0 — AIBuildPersonalityConfig::findEntry20ByBit
const void* findEntry20ByBit_882AE0(const void* self, std::uint32_t bitIndex) noexcept;

// 6. 0x00882C30 — AIBuildPersonalityConfig::findBitmaskFloatOrOne
float findBitmaskFloatOrOne_882C30(const void* self, std::uint32_t bitIndex) noexcept;

// 7. 0x00884550 — AIBaseBuilder::updateBasePriorityFlags
void updateBasePriorityFlags_884550(void* self) noexcept;

// 8. 0x00884C40 — AIBuilder::hasActiveBuilderAtPosition
bool hasActiveBuilderAtPosition_884C40(const void* self, const Coord3DView* pos) noexcept;

// 9. 0x00886320 — AIBudgetState::evaluateHeuristicProduct
float evaluateHeuristicProduct_886320(
    const AIBudgetStateView* self,
    void* contextArg0,
    void* contextArg1 = nullptr) noexcept;

// 10. 0x00887170 — AIBuildableQueue::popNextTemplateId
std::uint32_t popNextTemplateId_887170(AIBuildableQueueView* self) noexcept;

// 11. 0x00887300 — AIBuildableRegistry::findEntryByTemplateId
void* findEntryByTemplateId_887300(const void* self, std::uint32_t templateId) noexcept;

// 12. 0x008878C0 — AIBuildableRegistry::lookupNestedValue
std::uint32_t lookupNestedValue_8878C0(
    const void* self,
    std::int32_t outerKey,
    std::int32_t innerKey) noexcept;

// 13. 0x00887D60 — AIBuildableEntry::init
void* initBuildableEntry_887D60(
    void* self,
    std::uint32_t ownerAt50,
    std::uint32_t templateAt54) noexcept;

// 14. 0x0088A0E0 — AIBankAccount::sumWeightedRequestsAbovePriority
float sumWeightedRequestsAbovePriority_88A0E0(
    const AIBankAccountView* self,
    float minPriority,
    float* inOutMaxPriority) noexcept;

// 15. 0x0088AD80 — AIBuilder::sumQueuedPowerDelta
std::int32_t sumQueuedPowerDelta_88AD80(
    const void* self,
    const void* thingFactoryOverride = nullptr) noexcept;

// 16. 0x0088AE10 — AIBuilder::countQueuedTemplate
std::int32_t countQueuedTemplate_88AE10(const void* self, std::uint32_t templateId) noexcept;

// 17. 0x0088AFC0 — AIBuilder::notifyObjectConstructed
void notifyObjectConstructed_88AFC0(
    void* self,
    std::uint32_t builderId,
    const std::uint32_t* templateIdPtr) noexcept;

// 18. 0x0088BC10 — AIEconomyStatsSubobject::initWithOwner
void* initEconomyStatsWithOwner_88BC10(
    void* self,
    void* ownerAt8,
    void* ownerAt4 = nullptr) noexcept;

// 19. 0x0088BE50 — AIEconomyStats::findStatValueByKey
std::uint32_t findStatValueByKey_88BE50(const void* self, std::uint32_t key) noexcept;

// 20. 0x0088BE90 — AIEconomyStatsSlidingWindow::pruneExpiredSamples
void pruneExpiredSamples_88BE90(
    void* self,
    const void* gameLogicOverride = nullptr) noexcept;

// 21. 0x0088D120 — AIBuildableGroup::containsTemplateId
bool groupContainsTemplateId_88D120(const void* self, std::uint32_t templateId) noexcept;

// 22. 0x0088D1A0 — AIBuildableCategoryMap::containsTemplateId
bool categoryMapContainsTemplateId_88D1A0(const void* self, std::uint32_t templateId) noexcept;

// 23. 0x0088DE50 — AIBuildableIterator::advanceToNextActive
void advanceToNextActive_88DE50(AIBuildableIteratorView* self) noexcept;

// 24. 0x0088DEC0 — AIBuildableCategoryMap::findFirstActiveForKey
AIBuildableIteratorView* findFirstActiveForKey_88DEC0(
    void* self,
    AIBuildableIteratorView* outIter,
    std::uint32_t key) noexcept;

// 25. 0x008900B0 — AIBankAccount::canAffordRequest
bool canAffordRequest_8900B0(
    const AIBankAccountView* self,
    float requestCost,
    float requestPriority,
    float rateScale) noexcept;

// 26. 0x00890400 — AIBase::getPosition
const Coord3DView* getBasePosition_890400(
    const AIBaseView* self,
    const void* gameLogicOverride = nullptr) noexcept;

// 27. 0x00890600 — AIBaseQueryCriteria::isCandidateBetterThanBest
bool isCandidateBetterThanBest_890600(
    const AIBaseQueryCriteriaView* self,
    const AIBaseView* candidateBase,
    const AIBaseView* currentBestBase,
    const void* gameLogicOverride = nullptr) noexcept;

// 28. 0x008908C0 — AIBaseBuilder::findBestMatchingBase
AIBaseView* findBestMatchingBase_8908C0(
    const void* self,
    const AIBaseQueryCriteriaConfigView* config,
    const Coord3DView* referencePos,
    const void* gameLogicOverride = nullptr) noexcept;

// 29. 0x008925B0 — AIBuildState::claimFirstMatchingBuilderSlot
std::uint32_t claimFirstMatchingBuilderSlot_8925B0(
    void* self,
    const void* gameLogicOverride = nullptr) noexcept;

// 30. 0x00892670 — AIBaseBuilder::hasEnoughPowerForTemplate
bool hasEnoughPowerForTemplate_892670(
    const void* self,
    const void* buildRequest,
    const void* thingFactoryOverride = nullptr,
    const void* powerOverrideMgrOverride = nullptr) noexcept;

// 31. 0x00893520 — AIBuildState::shouldEstablishNewBase
bool shouldEstablishNewBase_893520(
    const void* self,
    const void* gameLogicOverride = nullptr) noexcept;

// 32. 0x00893880 — AIBuildState::findActiveObjectBySlotKey
void* findActiveObjectBySlotKey_893880(
    const void* self,
    std::int32_t slotKey,
    const void* gameLogicOverride = nullptr) noexcept;

// 33. 0x00893920 — AIBuildState::getSlotAuxiliaryIdIfObjectAlive
std::uint32_t getSlotAuxiliaryIdIfObjectAlive_893920(
    const void* self,
    std::int32_t slotKey,
    const void* gameLogicOverride = nullptr) noexcept;

// 34. 0x00893A00 — AIBuildState::hasAliveTrackedObjectForCandidate
bool hasAliveTrackedObjectForCandidate_893A00(
    const void* self,
    const void* candidateObject,
    const void* gameLogicOverride = nullptr) noexcept;

// 35. 0x00897BB0 — AIEconomyStatsSubobject::initDefault
void* initEconomyStatsDefault_897BB0(void* self) noexcept;

// 36. 0x00897ED0 — AIEconomyStats::updateSlidingWindow
void updateEconomyStatsSlidingWindow_897ED0(
    void* self,
    const void* gameLogicOverride = nullptr) noexcept;

// 37. 0x0089A6D0 — AIBuildableEntry::resolveStructureObject
void* resolveStructureObject_89A6D0(
    const void* self,
    const void* gameLogicOverride = nullptr) noexcept;

// 38. 0x0089A740 — AIBuildableEntry::getStructureOrFallbackPosition
const Coord3DView* getStructureOrFallbackPosition_89A740(
    const void* self,
    const void* gameLogicOverride = nullptr) noexcept;

// 39. 0x0089AC90 — AIBuildableStructureTracker::init
void* initBuildableStructureTracker_89AC90(
    void* self,
    const void* gameLogicOverride = nullptr,
    const float* framesPerSecondOverride = nullptr) noexcept;

// 40. 0x0089AF50 — AIBuildHeuristic::initBase
void* initBuildHeuristicBase_89AF50(void* self) noexcept;

// 41. 0x0089D070 — AIBuilder::hasActiveBuildableEntryForObjectId
bool hasActiveBuildableEntryForObjectId_89D070(
    const void* self,
    std::uint32_t objectId,
    const void* gameLogicOverride = nullptr) noexcept;

// 42. 0x0089D0D0 — AIBuilder::hasActiveBuildableEntryAtPosition
bool hasActiveBuildableEntryAtPosition_89D0D0(
    const void* self,
    float x,
    float y,
    float z,
    const void* gameLogicOverride = nullptr) noexcept;

// 43. 0x0089DD90 — AIBuildableCandidateVector::eraseRange68
void* eraseCandidateRange68_89DD90(
    void* self,
    void* eraseBegin,
    void* eraseEnd) noexcept;

// 44. 0x0089E700 — AIBaseQueryCriteria::insertionSortBounded
void insertionSortBasesBounded_89E700(
    AIBaseView** first,
    AIBaseView** last,
    AIBaseQueryCriteriaView criteria,
    const void* gameLogicOverride = nullptr) noexcept;

// 45. 0x0089E750 — AIBaseQueryCriteria::insertionSortUnguarded
void insertionSortBasesUnguarded_89E750(
    AIBaseView** first,
    AIBaseView** last,
    AIBaseQueryCriteriaView criteria,
    const void* gameLogicOverride = nullptr) noexcept;

// 46. 0x0089F140 — AIBaseQueryCriteria::selectMedianOfThree
AIBaseView** selectMedianOfThreeBases_89F140(
    AIBaseView** first,
    AIBaseView** mid,
    AIBaseView** last,
    AIBaseQueryCriteriaView criteria,
    const void* gameLogicOverride = nullptr) noexcept;

// 47. 0x0089F1D0 — AIBaseQueryCriteria::partitionByPivot
AIBaseView** partitionBasesByPivot_89F1D0(
    AIBaseView** first,
    AIBaseView** last,
    AIBaseView** pivotPtr,
    AIBaseQueryCriteriaView criteria,
    const void* gameLogicOverride = nullptr) noexcept;

// 48. 0x0089F480 — AIBuildHeuristicStore::init
void* initBuildHeuristicStore_89F480(void* self) noexcept;

// 49. 0x0089FE10 — AIBaseBuilder::pruneAssignedStructures
void pruneAssignedStructures_89FE10(void* self) noexcept;

// 50. 0x0089FEB0 — AIBuildStateManager::removeMatchingOrProducedTemplate
void removeMatchingOrProducedTemplate_89FEB0(
    void* self,
    std::uint32_t templateId,
    const void* thingFactoryOverride = nullptr) noexcept;

// 51. 0x008AAB70 — AIBuildableStructure::getEffectivePriority
float getEffectivePriority_8AAB70(const void* self) noexcept;

// 52. 0x008AABB0 — AIBuildableStructure::getValuePerCost
float getStructureValuePerCost_8AABB0(void* self, void* aiContext) noexcept;

// 53. 0x008AABE0 — AIBuildableUnit::getValuePerCost
float getUnitValuePerCost_8AABE0(void* self, void* aiContext) noexcept;

// 54. 0x008AAC20 — AIRebuildableStructure::getScaledDelayFrames
float getScaledDelayFrames_8AAC20(
    const void* self,
    std::uint32_t unusedArg = 0u,
    const void* gameLogicOverride = nullptr,
    const float* framesPerSecondOverride = nullptr) noexcept;

// 55. 0x008AEEB0 — AIBaseBuilder::moveStructureToUnderConstruction
void moveStructureToUnderConstruction_8AEEB0(
    void* self,
    std::uint32_t objectId) noexcept;

// 56. 0x008AEF30 — AIBaseBuilder::moveStructureToCompleted
void moveStructureToCompleted_8AEF30(
    void* self,
    std::uint32_t objectId) noexcept;

} // namespace ra3::gamelogic::skirmishai_base_build
