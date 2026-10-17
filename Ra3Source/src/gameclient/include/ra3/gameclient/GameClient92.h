#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

namespace ra3::gameclient {

enum class MusicConditionKind : std::uint8_t {
    factory,
    andCondition,
    anyTrackPlaying,
    engagementCount,
    evaEventPlayedRecently,
    hitBySuperWeapon,
    localPlayerIsObserver,
    localPlayerKillRatio,
    notCondition,
    objectsNearEvaEvent,
    objectsOfTypeExist,
    orCondition,
    pathMusicStateCurrentlyActive,
    pathMusicStateLastActive,
    scoredKillCount,
    specificTrackTypePlaying,
    timeFromStartOfLevel,
    trackPlayedCount,
    unitsFarFromBase,
    system
};

const char* musicConditionFactoryTypeName() noexcept;                    // 0x00923740
const char* andConditionTypeName() noexcept;                             // 0x00923750
const char* anyTrackPlayingTypeName() noexcept;                          // 0x00923760
const char* engagementCountTypeName() noexcept;                          // 0x00923890
const char* evaEventPlayedRecentlyTypeName() noexcept;                   // 0x00923950
const char* hitBySuperWeaponTypeName() noexcept;                         // 0x00923960
const char* localPlayerIsObserverTypeName() noexcept;                    // 0x009239C0
const char* localPlayerKillRatioTypeName() noexcept;                     // 0x009239D0
const char* notConditionTypeName() noexcept;                             // 0x009239E0
const char* objectsNearEvaEventTypeName() noexcept;                      // 0x009239F0
const char* objectsOfTypeExistTypeName() noexcept;                       // 0x00923A00
const char* orConditionTypeName() noexcept;                              // 0x00923A10
const char* pathMusicStateCurrentlyActiveTypeName() noexcept;            // 0x00923A70
const char* pathMusicStateLastActiveTypeName() noexcept;                 // 0x00923B70
const char* scoredKillCountTypeName() noexcept;                          // 0x00923BE0
const char* specificTrackTypePlayingTypeName() noexcept;                 // 0x00923BF0
const char* timeFromStartOfLevelTypeName() noexcept;                     // 0x00923C50
const char* trackPlayedCountTypeName() noexcept;                         // 0x00923C60
const char* unitsFarFromBaseTypeName() noexcept;                         // 0x00923C70
const char* musicScriptingSystemTypeName() noexcept;                     // 0x00923D30
const char* musicConditionTypeName(MusicConditionKind kind) noexcept;

// The largest reconstructed condition runtime is 0x68 bytes. A rounded view
// keeps untouched bytes observable so constructor write sets can be checked.
struct MusicConditionObject {
    std::array<std::uint8_t, 0x70> bytes;
};

static_assert(sizeof(MusicConditionObject) == 0x70);

using MusicConditionAction = void (*)(void* context) noexcept;

struct MusicConditionConstructionContext {
    std::uint32_t objectAddress;
    std::uint32_t resourceAddress;
    std::uint32_t localPlayer;
    std::uint32_t ticksPerSecondBits;
    std::uint32_t gameSpeedBits;
    bool hasGameClock;
    bool indexedTrackedState;
    bool objectFilterPresent;
    MusicConditionAction clear;
    MusicConditionAction refresh;
    void* actionContext;
};

void constructAnyTrackPlaying(
    MusicConditionObject& object,
    const MusicConditionConstructionContext& context) noexcept;         // 0x00924BD0
void constructEvaEventPlayedRecently(
    MusicConditionObject& object,
    const MusicConditionConstructionContext& context) noexcept;         // 0x00924C00
void constructLocalPlayerIsObserver(
    MusicConditionObject& object,
    const MusicConditionConstructionContext& context) noexcept;         // 0x00924C30
void constructPathMusicStateCurrentlyActive(
    MusicConditionObject& object,
    const MusicConditionConstructionContext& context) noexcept;         // 0x00924D10
void constructPathMusicStateLastActive(
    MusicConditionObject& object,
    const MusicConditionConstructionContext& context) noexcept;         // 0x00924D90
void constructSpecificTrackTypePlaying(
    MusicConditionObject& object,
    const MusicConditionConstructionContext& context) noexcept;         // 0x00924E20
void constructTimeFromStartOfLevel(
    MusicConditionObject& object,
    const MusicConditionConstructionContext& context) noexcept;         // 0x00924E50
void constructTrackPlayedCount(
    MusicConditionObject& object,
    const MusicConditionConstructionContext& context) noexcept;         // 0x00924E90
void constructEngagementCount(
    MusicConditionObject& object,
    const MusicConditionConstructionContext& context) noexcept;         // 0x00925290
void constructHitBySuperWeapon(
    MusicConditionObject& object,
    const MusicConditionConstructionContext& context,
    std::uint32_t durationBits) noexcept;                               // 0x00925360
void constructObjectsNearEvaEvent(
    MusicConditionObject& object,
    const MusicConditionConstructionContext& context) noexcept;         // 0x009256D0
void constructObjectsOfTypeExist(
    MusicConditionObject& object,
    const MusicConditionConstructionContext& context) noexcept;         // 0x00925700
void constructUnitsFarFromBase(
    MusicConditionObject& object,
    const MusicConditionConstructionContext& context) noexcept;         // 0x00925810
void constructLocalPlayerKillRatio(
    MusicConditionObject& object,
    const MusicConditionConstructionContext& context) noexcept;         // 0x00926130
void constructScoredKillCount(
    MusicConditionObject& object,
    const MusicConditionConstructionContext& context) noexcept;         // 0x00926380

using MusicConditionPredicate = bool (*)(void* context, std::size_t index) noexcept;
using MusicFrameProvider = std::uint32_t (*)(void* context) noexcept;
using MusicCountProvider = std::int32_t (*)(void* context) noexcept;

struct MusicScoreRange {
    const std::int32_t* scores;
    std::size_t count;
};

using MusicScoreRangeProvider = MusicScoreRange (*)(void* context) noexcept;

enum class MusicComparison : std::uint32_t {
    less = 0,
    lessOrEqual = 1,
    approximatelyEqual = 2,
    greaterOrEqual = 3,
    greater = 4
};

bool compareMusicValue(
    MusicComparison comparison, float value, float reference) noexcept; // 0x00923C80
bool musicStateIsActive(bool hasState, bool active) noexcept;            // 0x00923A20
void updateTrackedMusicState(
    bool enabled, std::uint32_t trackedState, std::uint32_t eventState,
    bool active, bool& state) noexcept;                                  // 0x00923A30/0x00923A50
void updateTimedTrackedMusicState(
    bool enabled, std::uint32_t trackedState, std::uint32_t eventState,
    std::uint32_t currentFrame, std::uint32_t& changedAt,
    bool& state) noexcept;                                               // 0x00923AD0/0x00923B20

bool allMusicConditions(
    std::size_t count, MusicConditionPredicate predicate,
    void* context) noexcept;                                             // 0x00923E20
bool anyMusicCondition(
    std::size_t count, MusicConditionPredicate predicate,
    void* context) noexcept;                                             // 0x00923F50
bool negateMusicCondition(
    bool hasCondition, MusicConditionPredicate predicate,
    void* context) noexcept;                                             // 0x00923F00
bool engagementCountSatisfied(
    std::uint32_t count, std::uint32_t required) noexcept;               // 0x00923EF0
bool timeFromStartSatisfied(
    std::uint32_t currentFrame, std::uint32_t framesPerSecond,
    float seconds) noexcept;                                             // 0x00923C00
bool lastMusicStateSatisfied(
    bool hasState, bool active, std::uint32_t currentFrame,
    std::uint32_t changedAt, std::uint32_t framesPerSecond,
    float time, MusicComparison comparison) noexcept;                    // 0x00923F80
bool scoredKillCountSatisfied(
    std::uint32_t& cachedPlayer, std::uint32_t& cachedVersion,
    std::uint32_t localPlayer, std::uint32_t currentVersion,
    std::int32_t required, MusicScoreRangeProvider scores,
    MusicConditionAction clear, MusicConditionAction refresh,
    void* context) noexcept;                                            // 0x00926470

bool cachedMusicCondition(
    std::uint32_t& nextCheck, bool& cached,
    float interval, float checksPerSecond,
    MusicConditionPredicate predicate, void* predicateContext,
    MusicFrameProvider frameProvider, void* frameContext) noexcept;      // 0x009236C0
bool localPlayerObserverCondition(
    bool hasLocalPlayer, bool firstQuery, bool playerFlag,
    bool secondQuery, bool configuredFallback) noexcept;                 // 0x00923970
bool trackPlayedCountSatisfied(
    std::uint32_t& cachedPlayer, std::uint32_t localPlayer,
    std::int32_t required, MusicCountProvider played,
    MusicConditionAction refresh, void* context) noexcept;              // 0x009241F0

enum class MusicEntryFloatPolicy : std::uint8_t {
    none,
    float32,
    x87Intermediate
};

struct MusicReconstructionEntry {
    std::uint32_t address;
    const char* name;
    std::uint8_t stackCleanup;
    const char* registers;
    const char* writes;
    const char* aliases;
    MusicEntryFloatPolicy floats;
};

const MusicReconstructionEntry* musicReconstructionEntries(
    std::size_t& count) noexcept;

}  // namespace ra3::gameclient
