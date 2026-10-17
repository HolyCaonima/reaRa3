#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

namespace ra3::gamelogic {

// Reconstructed clean GameLogic Object Behavior module family in 0x00687460 and
// 0x00689E80-0x0068FE60 (70 functions across Object::findBehaviorModule and 26
// vtable-anchored Behavior module classes).
//
// Reference architecture and naming informed by EA Generals / Zero Hour GPL:
// - GeneralsMD/Code/GameEngine/Include/GameLogic/Module/BehaviorModule.h
// - GeneralsMD/Code/GameEngine/Source/GameLogic/Object/Behavior/AutoHealBehavior.cpp
// - GeneralsMD/Code/GameEngine/Source/GameLogic/Object/Behavior/FireWeaponWhenDamagedBehavior.cpp
// - GeneralsMD/Code/GameEngine/Source/GameLogic/Object/Behavior/ParkingPlaceBehavior.cpp
// - GeneralsMD/Code/GameEngine/Source/GameLogic/Object/Behavior/PoisonedBehavior.cpp
// - GeneralsMD/Code/GameEngine/Source/GameLogic/Object/Behavior/SlowDeathBehavior.cpp
// - GeneralsMD/Code/GameEngine/Source/GameLogic/Object/Behavior/SpawnBehavior.cpp
// - GeneralsMD/Code/GameEngine/Source/GameLogic/Object/Behavior/SupplyWarehouseCripplingBehavior.cpp

struct Coord3D68 {
    float x;
    float y;
    float z;
};

struct StaticModuleKeyState {
    std::uint32_t guard;
    std::uint32_t cachedKey;
};

struct StaticModuleFinderResult {
    std::uint32_t moduleAddress;
    std::uint32_t guard;
    std::uint32_t cachedKey;
    bool resolverCalled;
    const char* resolvedName;
};

struct WakeScheduleCall {
    bool called;
    std::uint32_t objectAddress;
    std::uint32_t updateSubobjectAddress;
    std::uint32_t wakeFrame;
};

// 1. 0x00687460: Object::findBehaviorModule(NameKeyType)
std::uint32_t findBehaviorModuleByKey(
    const std::uint32_t* moduleAddresses,
    const std::uint32_t* moduleKeys,
    std::size_t count,
    std::uint32_t targetKey) noexcept;

// 2. 0x00689E80: AimWeaponBehavior target position update & sentinel check
void updateAimWeaponTargetPosition(
    Coord3D68& storedTarget,
    std::int32_t& outMode,
    const Coord3D68& inputTarget) noexcept;

// 3. 0x00689FC0: AimWeaponBehavior weapon slot mask remapper
void remapAimWeaponSlotMask(
    std::uint32_t (&outPair)[2],
    std::uint32_t sourceMask) noexcept;

// 4. 0x0068A080: AimWeaponBehavior module-data weapon slot mask remapper
void getAimWeaponModuleSlotMask(
    std::uint32_t (&outPair)[2],
    std::uint32_t moduleDataMask) noexcept;

// 5. 0x0068A0A0: BehaviorModule subobject (+0x0C) cast thunk to interface (+0x10)
std::uint32_t castBehaviorModuleToSubobjectPlus10(
    std::uint32_t behaviorSubobjectAddress) noexcept;

// 6. 0x0068A190: AssistedPlacementBehavior::getFromObject
StaticModuleFinderResult getAssistedPlacementBehaviorFromObject(
    StaticModuleKeyState state,
    std::uint32_t resolverKey,
    const std::uint32_t* moduleAddresses,
    const std::uint32_t* moduleKeys,
    std::size_t count) noexcept;

// 7. 0x0068A270: AssistedPlacementBehavior placement entry initializer
struct AssistedPlacementEntry68 {
    std::uint32_t vtable;
    std::uint32_t field04;
    std::array<std::uint32_t, 9> block9;
    std::array<std::uint32_t, 15> block15;
    std::array<std::uint32_t, 7> block7;
    std::uint8_t hasBlock9;
    std::uint8_t hasBlock15;
    std::uint8_t hasBlock7;
    std::uint8_t pad87;
};
static_assert(sizeof(AssistedPlacementEntry68) == 0x88, "AssistedPlacementEntry68 layout");

void initAssistedPlacementEntry(
    AssistedPlacementEntry68& entry,
    const std::uint32_t (&block9)[9],
    const std::uint32_t (&block15)[15],
    const std::uint32_t (&block7)[7]) noexcept;

// 8. 0x0068A320: AssistedPlacementInfoBehavior::getFromObject
StaticModuleFinderResult getAssistedPlacementInfoBehaviorFromObject(
    StaticModuleKeyState state,
    std::uint32_t resolverKey,
    const std::uint32_t* moduleAddresses,
    const std::uint32_t* moduleKeys,
    std::size_t count) noexcept;

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
    float fpsGlobal) noexcept;

// 10. 0x0068A810: AutoHealBehavior 9-dword state reset
void resetAutoHealStateFields(std::uint32_t (&fields)[9]) noexcept;

// 11. 0x0068A890: AutoHealBehavior flag 0x1000 setter/clearer at +0x40
void setAutoHealFlag1000(std::uint32_t& flagsAt40, bool enable) noexcept;

// 12. 0x0068A8B0: AutoHealBehavior enable/disable wake scheduling
WakeScheduleCall setAutoHealEnabledAndSchedule(
    std::uint32_t subobjectAddress,
    std::uint32_t ownerAddress,
    std::uint32_t currentFrame,
    bool enable,
    std::uint32_t& fieldAtPlus18) noexcept;

// 13. 0x0068AC10: BezierProjectileBehavior::advanceAlongPath
std::uint32_t advanceBezierProjectileAlongPath(
    const Coord3D68* waypoints,
    std::size_t waypointCount,
    std::uint32_t currentSegmentIndex,
    float field7C,
    float& fieldC8,
    float& fieldCC,
    float moduleDataStepEC,
    Coord3D68& outPosition) noexcept;

// 14. 0x0068ADF0: BezierProjectileBehavior::updateSegmentDistance
bool updateBezierProjectileSegmentDistance(
    const Coord3D68* waypoints,
    std::size_t waypointCount,
    std::int32_t currentSegmentIndex,
    std::uint32_t moduleDataFlags08,
    float moduleDataThresholdE4,
    float& accumulatedDistanceC0) noexcept;

// 15. 0x0068AEF0: BezierProjectileBehavior::getPreviousAndCurrentWaypoints
void getBezierProjectilePreviousAndCurrentWaypoints(
    const Coord3D68* waypoints,
    std::size_t waypointCount,
    std::int32_t currentIndex60,
    Coord3D68& outPrevious,
    Coord3D68& outCurrent) noexcept;

// 16. 0x0068AF80: BridgeBuilderBehavior::hasNullTowerSlot
bool hasNullBridgeTowerSlot(
    const std::uint32_t* slots,
    std::size_t count) noexcept;

// 17. 0x0068AFB0: BridgeBuilderBehavior::getTowerSlotByIndex
std::uint32_t getBridgeTowerSlotByIndex(
    const std::uint32_t* slots,
    std::size_t count,
    std::uint32_t index) noexcept;

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
    Coord3D68& outPosition) noexcept;

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
    std::uint32_t& outCount38) noexcept;

// 20. 0x0068B230: BuildingBehavior::getFromObject
StaticModuleFinderResult getBuildingBehaviorFromObject(
    StaticModuleKeyState state,
    std::uint32_t resolverKey,
    const std::uint32_t* moduleAddresses,
    const std::uint32_t* moduleKeys,
    std::size_t count) noexcept;

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
    std::uint8_t& state2E) noexcept;

// 22. 0x0068B840: BuildingDestructionBehavior::resetBoneDamage
struct BoneDamageResetCall {
    bool called;
    std::uint32_t modelAddress;
    std::int32_t boneIndex;
    std::uint32_t valueBits;
};

BoneDamageResetCall resetBuildingDestructionBoneDamage(
    bool hasSubObject,
    std::uint32_t renderObjectAddress,
    std::int32_t boneIndex,
    std::int32_t modelBoneCount) noexcept;

// 23. 0x0068B960: BuildingDestructionBehavior::findClosestBone
struct BuildingBoneCandidate68 {
    float activeMetric04;
    Coord3D68 position0C;
};

void findClosestBuildingDestructionBone(
    const BuildingBoneCandidate68* const* groups,
    const std::uint32_t* groupCounts,
    std::uint32_t numGroups,
    const Coord3D68& queryPoint,
    std::uint32_t& outGroupIndex,
    std::uint32_t& outBoneIndex) noexcept;

// 24. 0x0068C210: EnergyDisablingBehavior::notifyOwnerPlayerEnergyDisabling
struct EnergyDisablingNotifyCall {
    bool called;
    std::uint32_t energyManagerAddress;
    std::uint32_t ownerObjectId;
};

EnergyDisablingNotifyCall notifyOwnerPlayerEnergyDisabling(
    std::uint32_t ownerAddress,
    std::uint32_t playerAddress,
    std::uint32_t energyManagerAddress,
    std::uint32_t ownerObjectId) noexcept;

// 25. 0x0068C520: FireWeaponWhenDamagedBehavior::copyStateBlock
struct FireWeaponDamagedState68 {
    std::uint32_t field00;
    std::uint32_t field04;
    std::uint32_t field08;
    std::uint32_t field0C;
    std::uint32_t floatBits10;
    std::uint32_t floatBits14;
    std::uint32_t floatBits18;
    std::uint32_t field1C;
    std::uint32_t field20;
    std::uint32_t field24;
    std::uint32_t field28;
};
static_assert(sizeof(FireWeaponDamagedState68) == 0x2C, "FireWeaponDamagedState68 layout");

void copyFireWeaponWhenDamagedState(
    FireWeaponDamagedState68& dst,
    const FireWeaponDamagedState68& src) noexcept;

// 26. 0x0068C5E0: FXListBehavior::getFromObject
StaticModuleFinderResult getFXListBehaviorFromObject(
    StaticModuleKeyState state,
    std::uint32_t resolverKey,
    const std::uint32_t* moduleAddresses,
    const std::uint32_t* moduleKeys,
    std::size_t count) noexcept;

// 27. 0x0068C6D0: BehaviorModule subobject (+0x0C) cast thunk to interface (+0x1C)
std::uint32_t castBehaviorModuleToSubobjectPlus1C(
    std::uint32_t behaviorSubobjectAddress) noexcept;

// 28. 0x0068C7A0: FXListBehavior::findEntryByTrigger
std::uint32_t findFXListBehaviorEntryByTrigger(
    std::uint32_t entriesBaseAddress,
    const std::uint32_t* entryKeys,
    std::size_t count,
    std::uint32_t triggerKey) noexcept;

// 29. 0x0068CEF0: GateOpenAndCloseBehavior::canObjectPassThrough
bool canObjectPassThroughGate(
    std::uint32_t gateState30,
    bool hasObject,
    std::uint32_t templateFlagsC0,
    std::uint32_t objectGateState454) noexcept;

// 30. 0x0068CF20: GateOpenAndCloseBehavior::stopGateAudioAndMarkClosed
struct GateAudioStopResult {
    std::uint32_t audioHandlePassed;
    std::uint8_t closedFlag4C;
};

GateAudioStopResult stopGateAudioAndMarkClosed(
    std::uint32_t audioHandle48) noexcept;

// 31. 0x0068D3B0: SweepingLaserStateModule::computeCurrentSweepPosition
void computeSweepingLaserPosition(
    std::uint32_t endFrame58,
    std::uint32_t currentFrame,
    std::uint32_t durationFramesB0,
    bool reverseBC,
    const Coord3D68& startPos98,
    const Coord3D68& endPosA4,
    Coord3D68& outPos) noexcept;

// 32. 0x0068D650: LUAEventListBehavior::sumEntryValues
std::uint32_t sumLUAEventListEntries(
    const std::uint32_t* entryFirstWords,
    std::size_t count) noexcept;

// 33. 0x0068D790: ParkingPlaceBehavior::hasReservedSpace
bool hasParkingPlaceReservedSpace(
    bool gotInfo48,
    std::uint32_t objectId,
    const std::uint32_t* spaceObjectIds,
    std::size_t spaceCount) noexcept;

// 34. 0x0068D860: ParkingPlaceBehavior::canReserveSpaceForObject
struct ParkingCanReserveResult {
    bool allowed;
    bool virtualCalled;
    std::uint32_t templatePassed;
    std::uint32_t playerPassed;
};

ParkingCanReserveResult canParkingPlaceReserveSpaceForObject(
    std::uint32_t objectStatus88,
    std::uint32_t templateAddress,
    std::uint32_t playerAddress,
    bool virtualHasSpaceResult) noexcept;

// 35. 0x0068D890: ParkingPlaceBehavior::onObjectExit
struct ParkingExitResult {
    std::uint32_t releasedObjectId;
    std::uint32_t updatedPlayerCounter1314;
};

ParkingExitResult onParkingPlaceObjectExit(
    std::uint32_t exitingObjectId,
    std::uint32_t initialPlayerCounter1314,
    std::uint32_t moduleDataDecrement20) noexcept;

// 36. 0x0068D8C0: ParkingPlaceBehavior::releaseDoorForExit
struct ParkingSpaceSlot68 {
    std::int32_t door30;
    std::uint32_t objectInSpace34;
    std::uint8_t reservedForExit38;
};

void releaseParkingPlaceDoorForExit(
    ParkingSpaceSlot68* spaces,
    std::size_t count,
    std::int32_t exitDoor) noexcept;

// 37. 0x0068DA00: ParkingPlaceBehavior::matchesObjectFilter
struct ObjectFilterCallResult {
    std::uint32_t result;
    std::uint32_t filterAddress;
    std::uint32_t objectAddress;
    std::uint32_t playerAddress;
};

ObjectFilterCallResult matchesParkingPlaceObjectFilter(
    std::uint32_t filterAddress,
    std::uint32_t candidateObjectAddress,
    std::uint32_t ownerPlayerAddress,
    std::uint32_t filterReturnVal) noexcept;

// 38. 0x0068DA20: PassiveAreaEffectBehavior::getEffectValue
std::uint32_t getPassiveAreaEffectValue(
    std::uint32_t moduleDataValue2C,
    std::uint32_t& outValue) noexcept;

// 39. 0x0068DBF0: PlayerPowerManagerBehavior::getOwnerObject
std::uint32_t getPlayerPowerManagerOwnerObject(
    std::uint32_t ownerFieldAtBasePlus08) noexcept;

// 40. 0x0068DC00: PlayerPowerManagerBehavior::isForObject
bool isPlayerPowerManagerForObject(
    std::uint32_t ownerFieldAtBasePlus08,
    std::uint32_t candidateObjectAddress) noexcept;

// 41. 0x0068DC90: PlayerPowerManagerBehavior::getEntryCount
std::int32_t getPlayerPowerManagerEntryCount(
    std::uint32_t beginAddress,
    std::uint32_t endAddress) noexcept;

// 42. 0x0068DCA0: PlayerPowerManagerBehavior::getEntryByIndex
std::uint32_t getPlayerPowerManagerEntryByIndex(
    const std::uint32_t* entries,
    std::int32_t count,
    std::int32_t index) noexcept;

// 43. 0x0068DCD0: PlayerPowerManagerBehavior::findActiveEntry
struct PowerManagerEntryProbe68 {
    std::uint32_t entryAddress;
    float metricValue;
    bool acceptsFlag0;
    bool acceptsFlag1;
};

bool findActivePlayerPowerManagerEntry(
    const PowerManagerEntryProbe68* entries,
    std::size_t count,
    std::uint32_t mode,
    bool hasOutPointer,
    std::uint32_t& outEntryAddress) noexcept;

// 44. 0x0068DE80: PoisonedBehavior::onHealing
struct PoisonedHealingResult {
    std::uint32_t poisonDamageFrame04;
    std::uint32_t poisonStopFrame08;
    std::uint32_t poisonDamageAmountBits0C;
    std::uint8_t drawableTintByte14C;
    WakeScheduleCall wake;
};

PoisonedHealingResult onPoisonedBehaviorHealing(
    std::uint32_t damageSubobjectAddress,
    std::uint32_t ownerAddress,
    bool hasDrawable,
    std::uint8_t initialDrawableTintByte14C,
    std::uint32_t currentFrame) noexcept;

// 45. 0x0068E410: RadarBehavior::enableObjectRadarOnPlayer
struct RadarEnableResult {
    bool playerRadarAdded;
    std::uint32_t playerAddress;
    std::uint32_t objectIdPassed;
    std::uint8_t flag04;
};

RadarEnableResult enableObjectRadarOnPlayer(
    bool virtualCanEnable,
    std::uint32_t playerAddress,
    std::uint32_t ownerObjectId,
    std::uint8_t initialFlag04) noexcept;

// 46. 0x0068E440: RadarBehavior::disableObjectRadarOnPlayer
struct RadarDisableResult {
    bool playerRadarRemoved;
    std::uint32_t playerAddress;
    std::uint32_t objectIdPassed;
};

RadarDisableResult disableObjectRadarOnPlayer(
    std::uint32_t playerAddress,
    std::uint32_t ownerObjectId) noexcept;

// 47. 0x0068E550: RestrictSpecialPowerBehavior::getRestrictionCenter
void getRestrictSpecialPowerPosition(
    bool hasOwner,
    const Coord3D68& ownerPosition,
    Coord3D68& outPosition) noexcept;

// 48. 0x0068E5F0: RestrictSpecialPowerBehavior::sumFirstNWeights
float sumRestrictSpecialPowerWeights(
    const float* weights,
    std::size_t weightCount,
    std::int32_t limit20) noexcept;

// 49. 0x0068E820: SecondaryObjectListenerModule::findMatchingEntry
std::uint32_t findSecondaryObjectListenerEntry(
    std::uint32_t entriesBaseAddress,
    const std::uint8_t* filterMatches,
    std::size_t count) noexcept;

// 50. 0x0068E880: SecondaryObjectListenerModule::hasMatchingEntry
std::uint32_t hasSecondaryObjectListenerEntry(
    std::uint32_t entriesBaseAddress,
    const std::uint8_t* filterMatches,
    std::size_t count) noexcept;

// 51. 0x0068EA70: ShareExperienceBehavior::calculateShareWeight
float calculateShareExperienceWeight(
    float radius08,
    float falloffFlag0C,
    const Coord3D68& sourcePosition,
    const Coord3D68& targetPosition) noexcept;

// 52. 0x0068EC50: SlowDeathBehavior::getProbabilityModifier
std::int32_t getSlowDeathProbabilityModifier(
    float maxHealth,
    std::int32_t baseProbabilityModifier0C,
    float bonusPerOverkillPercent10,
    float actualDamageDealt8C,
    float actualDamageClipped90) noexcept;

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
    float fpsGlobal) noexcept;

// 54. 0x0068F050: SpawnBehavior::scheduleReplacement
WakeScheduleCall scheduleSpawnBehaviorReplacement(
    std::uint32_t subobjectAddress,
    std::uint32_t ownerAddress,
    std::uint8_t moduleFlag59,
    float delaySeconds3C,
    bool hasGameLogic,
    std::uint32_t currentFrame,
    float gameLogicFrameScale,
    float fpsGlobal) noexcept;

// 55. 0x0068F0F0: SpawnBehavior::shouldTryToSpawn
struct SpawnTryResult {
    bool shouldSpawn;
    bool stopSpawningCalled;
};

SpawnTryResult shouldSpawnBehaviorTryToSpawn(
    std::uint8_t active68,
    std::uint8_t isOneShot4C,
    std::uint32_t objectStatus80,
    std::uint32_t objectDisabledMaskC0,
    std::uint8_t isNeutralControlled471) noexcept;

// 56. 0x0068F1E0: StancesBehavior::canAddEntry
std::uint32_t canStancesBehaviorAddEntry(
    std::uint32_t beginAddress14,
    std::uint32_t endAddress18,
    std::uint32_t maxEntries08) noexcept;

// 57. 0x0068F200: StancesBehavior::getOrPopLastEntry
void getOrPopLastStanceEntry(
    std::uint32_t* entries,
    std::size_t& count,
    bool pop,
    std::uint32_t& outValue) noexcept;

// 58. 0x0068F240: StancesBehavior::selectNextEntryByUsage
struct StanceUsageEntry68 {
    std::uint32_t value00;
    std::uint8_t useCount04;
};

std::uint32_t selectNextStanceEntryByUsage(
    StanceUsageEntry68* entries,
    std::size_t count,
    std::int32_t maxUseCount0C) noexcept;

// 59. 0x0068F2C0: StancesBehavior::findObjectBehaviorByTemplateId
struct BehaviorTemplateCandidate68 {
    std::uint32_t subobjectAddress;
    std::uint32_t templateId08;
};

std::uint32_t findObjectBehaviorByTemplateId(
    const BehaviorTemplateCandidate68* candidates,
    std::size_t count,
    std::uint32_t targetTemplateId) noexcept;

// 60. 0x0068F420: StreamStateModule::deactivateAndSleep
WakeScheduleCall deactivateStreamStateAndSleep(
    std::uint32_t subobjectAddress,
    std::uint32_t ownerAddress,
    std::uint32_t currentFrame,
    std::uint8_t& activeFlag40) noexcept;

// 61. 0x0068F520: StructurePlacementBehavior::getFromObject
StaticModuleFinderResult getStructurePlacementBehaviorFromObject(
    StaticModuleKeyState state,
    std::uint32_t resolverKey,
    const std::uint32_t* moduleAddresses,
    const std::uint32_t* moduleKeys,
    std::size_t count) noexcept;

// 62. 0x0068F790: SupplyWarehouseCripplingBehavior::onDamage
WakeScheduleCall onSupplyWarehouseCripplingDamage(
    std::uint32_t damageSubobjectAddress,
    std::uint32_t ownerAddress,
    std::uint32_t frameBeforeReset,
    std::uint32_t frameAfterReset,
    std::uint32_t healingSuppressedUntilFrame04) noexcept;

// 63 & 64. 0x0068F7D0 / 0x0068F810: SupplyWarehouseCripplingBehavior::start/stopCrippledEffects
struct DockCrippledCallResult {
    std::uint32_t dockInterfaceAddress;
    std::uint32_t crippledArg;
};

DockCrippledCallResult startSupplyWarehouseCrippledEffects(
    const std::uint32_t* dockInterfaces,
    std::size_t moduleCount) noexcept;

DockCrippledCallResult stopSupplyWarehouseCrippledEffects(
    const std::uint32_t* dockInterfaces,
    std::size_t moduleCount) noexcept;

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
    std::uint32_t& timer9C) noexcept;

// 66. 0x0068FCE0: TiberiumFieldBehavior::setFieldActiveState
struct TiberiumCrystalSubmodule68 {
    std::uint8_t active28;
    std::uint32_t activatedFrame2C;
    std::uint32_t crystalState40;
};

struct TiberiumCrystalSlot68 {
    std::uint32_t state00;
    std::uint32_t hasCrystal04;
    TiberiumCrystalSubmodule68* submodule08;
};

void setTiberiumFieldActiveState(
    std::uint8_t& fieldActiveA0,
    std::uint8_t newActive,
    std::uint32_t currentFrame,
    TiberiumCrystalSlot68* slots,
    std::size_t slotCount) noexcept;

// 67. 0x0068FD40: TiberiumFieldBehavior::bindCrystalSlotModule
struct BindCrystalSlotResult {
    StaticModuleFinderResult finder;
    std::uint32_t boundModuleAddress;
    std::uint8_t submoduleActive28;
    std::uint32_t submoduleCrystalState40;
};

BindCrystalSlotResult bindTiberiumFieldCrystalSlot(
    std::uint8_t fieldActiveA0,
    std::uint32_t slotState00,
    StaticModuleKeyState keyState,
    std::uint32_t resolverKey,
    const std::uint32_t* moduleAddresses,
    const std::uint32_t* moduleKeys,
    std::size_t moduleCount,
    std::uint8_t initialSubmoduleActive28,
    std::uint32_t initialSubmoduleCrystalState40) noexcept;

// 68. 0x0068FDC0: TiberiumFieldBehavior::hasNoSlotInStateOne
bool hasNoTiberiumFieldSlotInStateOne(
    const std::uint32_t* slotStates,
    std::int32_t cols38,
    std::int32_t rows3C) noexcept;

// 69. 0x0068FE10: TiberiumFieldBehavior::hasActiveSlotInStateEight
bool hasActiveTiberiumFieldSlotInStateEight(
    const std::uint32_t* slotStates,
    const std::uint32_t* slotActiveFlags,
    std::int32_t cols38,
    std::int32_t rows3C) noexcept;

// 70. 0x0068FE60: TiberiumFieldBehavior::pickRandomEntry
std::uint32_t pickRandomTiberiumFieldEntry(
    const std::uint32_t* entries,
    std::size_t count,
    std::uint32_t sampledIndex) noexcept;

}  // namespace ra3::gamelogic
