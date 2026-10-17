#include <ra3/gameclient/GameClient92.h>

#include <algorithm>
#include <cmath>
#include <cstring>
#include <limits>

namespace ra3::gameclient {
namespace {

constexpr std::uint32_t kBaseInterface = 0x00BDB8E8;
constexpr std::uint32_t kOneBits = 0x3F800000;
constexpr std::uint32_t kLargeDurationBits = 0x40000000;
constexpr float kMaximumStateAge = 3600.0F;
constexpr float kUnsignedCorrection = 4294967296.0F;

void write32(MusicConditionObject& object, std::size_t offset, std::uint32_t value) noexcept
{
    std::memcpy(object.bytes.data() + offset, &value, sizeof(value));
}

void write8(MusicConditionObject& object, std::size_t offset, std::uint8_t value) noexcept
{
    object.bytes[offset] = value;
}

float floatFromBits(std::uint32_t bits) noexcept
{
    float value = 0.0F;
    std::memcpy(&value, &bits, sizeof(value));
    return value;
}

std::uint32_t truncateHitDuration(
    float duration, float gameSpeed, float ticksPerSecond) noexcept
{
#if defined(_MSC_VER) && defined(_M_IX86)
    std::uint16_t originalControl = 0;
    std::uint16_t truncateControl = 0;
    __int64 result = 0;
    __asm {
        fnstcw originalControl
        mov ax, originalControl
        or ax, 0C00h
        mov truncateControl, ax
        fld duration
        fld gameSpeed
        fmul ticksPerSecond
        fmulp st(1), st(0)
        fldcw truncateControl
        fistp result
        fldcw originalControl
    }
    return static_cast<std::uint32_t>(result);
#else
    const long double value =
        static_cast<long double>(duration)
        * static_cast<long double>(gameSpeed)
        * static_cast<long double>(ticksPerSecond);
    if (!std::isfinite(static_cast<double>(value))
        || value < static_cast<long double>(std::numeric_limits<std::int64_t>::lowest())
        || value > static_cast<long double>(std::numeric_limits<std::int64_t>::max())) {
        return 0;
    }
    return static_cast<std::uint32_t>(static_cast<std::int64_t>(value));
#endif
}

std::uint32_t truncateNextCheck(
    std::uint32_t now, float interval, float checksPerSecond) noexcept
{
#if defined(_MSC_VER) && defined(_M_IX86)
    std::uint16_t originalControl = 0;
    std::uint16_t truncateControl = 0;
    __int64 result = 0;
    __asm {
        fnstcw originalControl
        mov ax, originalControl
        or ax, 0C00h
        mov truncateControl, ax
        mov eax, now
        push eax
        fild dword ptr [esp]
        add esp, 4
        test eax, eax
        jge now_ready
        fadd kUnsignedCorrection
    now_ready:
        fld interval
        fmul checksPerSecond
        faddp st(1), st(0)
        fldcw truncateControl
        fistp result
        fldcw originalControl
    }
    return static_cast<std::uint32_t>(result);
#else
    const long double value =
        static_cast<long double>(now)
        + static_cast<long double>(interval)
        * static_cast<long double>(checksPerSecond);
    if (!std::isfinite(static_cast<double>(value))
        || value < static_cast<long double>(std::numeric_limits<std::int64_t>::lowest())
        || value > static_cast<long double>(std::numeric_limits<std::int64_t>::max())) {
        return 0;
    }
    return static_cast<std::uint32_t>(static_cast<std::int64_t>(value));
#endif
}

bool extendedTimeFromStart(
    std::uint32_t currentFrame, std::uint32_t framesPerSecond,
    float seconds) noexcept
{
#if defined(_MSC_VER) && defined(_M_IX86)
    bool result = false;
    __asm {
        mov eax, currentFrame
        push eax
        fild dword ptr [esp]
        add esp, 4
        test eax, eax
        jge current_ready
        fadd kUnsignedCorrection
    current_ready:
        mov eax, framesPerSecond
        push eax
        fild dword ptr [esp]
        add esp, 4
        test eax, eax
        jge rate_ready
        fadd kUnsignedCorrection
    rate_ready:
        fmul seconds
        fxch st(1)
        fcomip st(0), st(1)
        fstp st(0)
        jbe finished
        mov result, 1
    finished:
    }
    return result;
#else
    return static_cast<long double>(framesPerSecond)
            * static_cast<long double>(seconds)
        < static_cast<long double>(currentFrame);
#endif
}

float extendedElapsed(
    std::uint32_t frames, std::uint32_t framesPerSecond) noexcept
{
#if defined(_MSC_VER) && defined(_M_IX86)
    float result = 0.0F;
    __asm {
        mov eax, frames
        push eax
        fild dword ptr [esp]
        add esp, 4
        test eax, eax
        jge frames_ready
        fadd kUnsignedCorrection
    frames_ready:
        mov eax, framesPerSecond
        push eax
        fild dword ptr [esp]
        add esp, 4
        test eax, eax
        jge divisor_ready
        fadd kUnsignedCorrection
    divisor_ready:
        fdivp st(1), st(0)
        fstp result
    }
    return result;
#else
    return static_cast<float>(
        static_cast<long double>(frames)
        / static_cast<long double>(framesPerSecond));
#endif
}

void constructSimple(
    MusicConditionObject& object, const MusicConditionConstructionContext& context,
    std::uint32_t secondaryVtable, std::uint32_t primaryVtable) noexcept
{
    write32(object, 0x04, 0);
    write32(object, 0x08, kBaseInterface);
    write32(object, 0x0C, context.resourceAddress);
    write32(object, 0x00, secondaryVtable);
    write32(object, 0x08, primaryVtable);
}

void constructListened(
    MusicConditionObject& object, const MusicConditionConstructionContext& context,
    std::uint32_t secondaryVtable, std::uint32_t primaryVtable,
    std::uint32_t listenerVtable) noexcept
{
    write32(object, 0x04, 0);
    write32(object, 0x08, kBaseInterface);
    write32(object, 0x00, 0x00C56958);
    write32(object, 0x08, 0x00C56948);
    write32(object, 0x0C, context.resourceAddress);
    write32(object, 0x10, 0x00C5724C);
    write32(object, 0x14, context.objectAddress + 0x14);
    write32(object, 0x18, context.objectAddress + 0x14);
    write32(object, 0x00, secondaryVtable);
    write32(object, 0x08, primaryVtable);
    write32(object, 0x10, listenerVtable);
}

constexpr MusicReconstructionEntry kEntries[] = {
    {0x009236C0, "cachedMusicCondition", 0, "AL", "+0x10,+0x14", "this", MusicEntryFloatPolicy::x87Intermediate},
    {0x00923740, "musicConditionFactoryTypeName", 0, "EAX", "none", "none", MusicEntryFloatPolicy::none},
    {0x00923750, "andConditionTypeName", 0, "EAX", "none", "none", MusicEntryFloatPolicy::none},
    {0x00923760, "anyTrackPlayingTypeName", 0, "EAX", "none", "none", MusicEntryFloatPolicy::none},
    {0x00923890, "engagementCountTypeName", 0, "EAX", "none", "none", MusicEntryFloatPolicy::none},
    {0x00923950, "evaEventPlayedRecentlyTypeName", 0, "EAX", "none", "none", MusicEntryFloatPolicy::none},
    {0x00923960, "hitBySuperWeaponTypeName", 0, "EAX", "none", "none", MusicEntryFloatPolicy::none},
    {0x00923970, "localPlayerObserverCondition", 0, "AL", "none", "this/resource", MusicEntryFloatPolicy::none},
    {0x009239C0, "localPlayerIsObserverTypeName", 0, "EAX", "none", "none", MusicEntryFloatPolicy::none},
    {0x009239D0, "localPlayerKillRatioTypeName", 0, "EAX", "none", "none", MusicEntryFloatPolicy::none},
    {0x009239E0, "notConditionTypeName", 0, "EAX", "none", "none", MusicEntryFloatPolicy::none},
    {0x009239F0, "objectsNearEvaEventTypeName", 0, "EAX", "none", "none", MusicEntryFloatPolicy::none},
    {0x00923A00, "objectsOfTypeExistTypeName", 0, "EAX", "none", "none", MusicEntryFloatPolicy::none},
    {0x00923A10, "orConditionTypeName", 0, "EAX", "none", "none", MusicEntryFloatPolicy::none},
    {0x00923A20, "musicStateIsActive", 0, "AL", "none", "this", MusicEntryFloatPolicy::none},
    {0x00923A30, "setTrackedMusicStateActive", 4, "none", "+0x0D conditional", "event may alias", MusicEntryFloatPolicy::none},
    {0x00923A50, "setTrackedMusicStateInactive", 4, "none", "+0x0D conditional", "event may alias", MusicEntryFloatPolicy::none},
    {0x00923A70, "pathMusicStateCurrentlyActiveTypeName", 0, "EAX", "none", "none", MusicEntryFloatPolicy::none},
    {0x00923AD0, "onTrackedMusicStateActivated", 4, "none", "+0x0D,+0x10 conditional", "event may alias", MusicEntryFloatPolicy::none},
    {0x00923B20, "onTrackedMusicStateDeactivated", 4, "none", "+0x0D,+0x10 conditional", "event may alias", MusicEntryFloatPolicy::none},
    {0x00923B70, "pathMusicStateLastActiveTypeName", 0, "EAX", "none", "none", MusicEntryFloatPolicy::none},
    {0x00923BE0, "scoredKillCountTypeName", 0, "EAX", "none", "none", MusicEntryFloatPolicy::none},
    {0x00923BF0, "specificTrackTypePlayingTypeName", 0, "EAX", "none", "none", MusicEntryFloatPolicy::none},
    {0x00923C00, "timeFromStartSatisfied", 0, "EAX", "none", "this/resource", MusicEntryFloatPolicy::x87Intermediate},
    {0x00923C50, "timeFromStartOfLevelTypeName", 0, "EAX", "none", "none", MusicEntryFloatPolicy::none},
    {0x00923C60, "trackPlayedCountTypeName", 0, "EAX", "none", "none", MusicEntryFloatPolicy::none},
    {0x00923C70, "unitsFarFromBaseTypeName", 0, "EAX", "none", "none", MusicEntryFloatPolicy::none},
    {0x00923C80, "compareMusicValue", 0, "AL", "none", "stack values", MusicEntryFloatPolicy::float32},
    {0x00923D30, "musicScriptingSystemTypeName", 0, "EAX", "none", "none", MusicEntryFloatPolicy::none},
    {0x00923E20, "allMusicConditions", 0, "AL", "none", "child list", MusicEntryFloatPolicy::none},
    {0x00923EF0, "engagementCountSatisfied", 0, "EAX", "none", "this/resource", MusicEntryFloatPolicy::none},
    {0x00923F00, "negateMusicCondition", 0, "AL", "none", "child may alias this", MusicEntryFloatPolicy::none},
    {0x00923F50, "anyMusicCondition", 0, "AL", "none", "child list", MusicEntryFloatPolicy::none},
    {0x00923F80, "lastMusicStateSatisfied", 0, "AL", "none", "this/resource", MusicEntryFloatPolicy::x87Intermediate},
    {0x009241F0, "trackPlayedCountSatisfied", 0, "AL", "+0x64 and refresh on player change", "this/resource/callback", MusicEntryFloatPolicy::none},
    {0x00924BD0, "constructAnyTrackPlaying", 4, "EAX", "+0x00..+0x0F selective", "this/resource", MusicEntryFloatPolicy::none},
    {0x00924C00, "constructEvaEventPlayedRecently", 4, "EAX", "+0x00..+0x0F selective", "this/resource", MusicEntryFloatPolicy::none},
    {0x00924C30, "constructLocalPlayerIsObserver", 4, "EAX", "+0x00..+0x0F selective", "this/resource", MusicEntryFloatPolicy::none},
    {0x00924D10, "constructPathMusicStateCurrentlyActive", 4, "EAX", "+0x00..+0x1D selective", "self anchor", MusicEntryFloatPolicy::none},
    {0x00924D90, "constructPathMusicStateLastActive", 4, "EAX", "+0x00..+0x20 selective", "self anchor", MusicEntryFloatPolicy::none},
    {0x00924E20, "constructSpecificTrackTypePlaying", 4, "EAX", "+0x00..+0x0F selective", "this/resource", MusicEntryFloatPolicy::none},
    {0x00924E50, "constructTimeFromStartOfLevel", 4, "EAX", "+0x00..+0x0F selective", "this/resource", MusicEntryFloatPolicy::none},
    {0x00924E90, "constructTrackPlayedCount", 4, "EAX", "+0x00..+0x0F selective", "this/resource", MusicEntryFloatPolicy::none},
    {0x00925290, "constructEngagementCount", 4, "EAX", "+0x00..+0x67 selective; refresh callback", "this/resource/embedded record", MusicEntryFloatPolicy::none},
    {0x00925360, "constructHitBySuperWeapon", 4, "EAX", "+0x00..+0x3F selective", "self anchor", MusicEntryFloatPolicy::x87Intermediate},
    {0x009256D0, "constructObjectsNearEvaEvent", 4, "EAX", "+0x00..+0x14 selective", "this/resource", MusicEntryFloatPolicy::none},
    {0x00925700, "constructObjectsOfTypeExist", 4, "EAX", "+0x00..+0x18 selective", "this/resource", MusicEntryFloatPolicy::none},
    {0x00925810, "constructUnitsFarFromBase", 4, "EAX", "+0x00..+0x14 selective", "this/resource", MusicEntryFloatPolicy::none},
    {0x00926130, "constructLocalPlayerKillRatio", 4, "EAX", "+0x00..+0x58 selective; clear/refresh callbacks", "this/resource/vector/embedded record", MusicEntryFloatPolicy::none},
    {0x00926380, "constructScoredKillCount", 4, "EAX", "+0x00..+0x24 selective; clear/refresh callbacks", "this/resource/vector", MusicEntryFloatPolicy::none},
    {0x00926470, "scoredKillCountSatisfied", 0, "AL", "+0x20,+0x24 and clear/refresh conditional", "this/resource/vector/callback", MusicEntryFloatPolicy::none},
};

static_assert(sizeof(kEntries) / sizeof(kEntries[0]) == 51);

}  // namespace

const char* musicConditionFactoryTypeName() noexcept { return "MusicScriptConditionFactory"; }
const char* andConditionTypeName() noexcept { return "MusicScriptConditionNugget_And"; }
const char* anyTrackPlayingTypeName() noexcept
{
    return "MusicScriptConditionNugget_AnyTrackPlaying";
}
const char* engagementCountTypeName() noexcept
{
    return "MusicScriptConditionNugget_EngagementCount";
}
const char* evaEventPlayedRecentlyTypeName() noexcept
{
    return "MusicScriptConditionNugget_EvaEventPlayedRecently";
}
const char* hitBySuperWeaponTypeName() noexcept
{
    return "MusicScriptConditionNugget_HitBySuperWeapon";
}
const char* localPlayerIsObserverTypeName() noexcept
{
    return "MusicScriptConditionNugget_LocalPlayerIsObserver";
}
const char* localPlayerKillRatioTypeName() noexcept
{
    return "MusicScriptConditionNugget_LocalPlayerKillRatio";
}
const char* notConditionTypeName() noexcept { return "MusicScriptConditionNugget_Not"; }
const char* objectsNearEvaEventTypeName() noexcept
{
    return "MusicScriptConditionNugget_ObjectsNearEvaEvent";
}
const char* objectsOfTypeExistTypeName() noexcept
{
    return "MusicScriptConditionNugget_ObjectsOfTypeExist";
}
const char* orConditionTypeName() noexcept { return "MusicScriptConditionNugget_Or"; }
const char* pathMusicStateCurrentlyActiveTypeName() noexcept
{
    return "MusicScriptConditionNugget_PathMusicStateCurrentlyActive";
}
const char* pathMusicStateLastActiveTypeName() noexcept
{
    return "MusicScriptConditionNugget_PathMusicStateLastActive";
}
const char* scoredKillCountTypeName() noexcept
{
    return "MusicScriptConditionNugget_ScoredKillCount";
}
const char* specificTrackTypePlayingTypeName() noexcept
{
    return "MusicScriptConditionNugget_SpecificTrackTypePlaying";
}
const char* timeFromStartOfLevelTypeName() noexcept
{
    return "MusicScriptConditionNugget_TimeFromStartOfLevel";
}
const char* trackPlayedCountTypeName() noexcept
{
    return "MusicScriptConditionNugget_TrackPlayedCount";
}
const char* unitsFarFromBaseTypeName() noexcept
{
    return "MusicScriptConditionNugget_UnitsFarFromBase";
}
const char* musicScriptingSystemTypeName() noexcept { return "MusicScriptingSystem"; }

const char* musicConditionTypeName(MusicConditionKind kind) noexcept
{
    switch (kind) {
    case MusicConditionKind::factory: return musicConditionFactoryTypeName();
    case MusicConditionKind::andCondition: return andConditionTypeName();
    case MusicConditionKind::anyTrackPlaying: return anyTrackPlayingTypeName();
    case MusicConditionKind::engagementCount: return engagementCountTypeName();
    case MusicConditionKind::evaEventPlayedRecently: return evaEventPlayedRecentlyTypeName();
    case MusicConditionKind::hitBySuperWeapon: return hitBySuperWeaponTypeName();
    case MusicConditionKind::localPlayerIsObserver: return localPlayerIsObserverTypeName();
    case MusicConditionKind::localPlayerKillRatio: return localPlayerKillRatioTypeName();
    case MusicConditionKind::notCondition: return notConditionTypeName();
    case MusicConditionKind::objectsNearEvaEvent: return objectsNearEvaEventTypeName();
    case MusicConditionKind::objectsOfTypeExist: return objectsOfTypeExistTypeName();
    case MusicConditionKind::orCondition: return orConditionTypeName();
    case MusicConditionKind::pathMusicStateCurrentlyActive:
        return pathMusicStateCurrentlyActiveTypeName();
    case MusicConditionKind::pathMusicStateLastActive:
        return pathMusicStateLastActiveTypeName();
    case MusicConditionKind::scoredKillCount: return scoredKillCountTypeName();
    case MusicConditionKind::specificTrackTypePlaying:
        return specificTrackTypePlayingTypeName();
    case MusicConditionKind::timeFromStartOfLevel: return timeFromStartOfLevelTypeName();
    case MusicConditionKind::trackPlayedCount: return trackPlayedCountTypeName();
    case MusicConditionKind::unitsFarFromBase: return unitsFarFromBaseTypeName();
    case MusicConditionKind::system: return musicScriptingSystemTypeName();
    }
    return "";
}

void constructAnyTrackPlaying(
    MusicConditionObject& object,
    const MusicConditionConstructionContext& context) noexcept
{
    constructSimple(object, context, 0x00C56A08, 0x00C569F8);
}

void constructEvaEventPlayedRecently(
    MusicConditionObject& object,
    const MusicConditionConstructionContext& context) noexcept
{
    constructSimple(object, context, 0x00C56A28, 0x00C56A18);
}

void constructLocalPlayerIsObserver(
    MusicConditionObject& object,
    const MusicConditionConstructionContext& context) noexcept
{
    constructSimple(object, context, 0x00C56A48, 0x00C56A38);
}

void constructPathMusicStateCurrentlyActive(
    MusicConditionObject& object,
    const MusicConditionConstructionContext& context) noexcept
{
    constructListened(object, context, 0x00C56A90, 0x00C56A80, 0x00C56A78);
    write8(object, 0x1C, 0);
    write8(object, 0x1D, 0);
    if (context.indexedTrackedState) {
        write8(object, 0x1C, 1);
    }
}

void constructPathMusicStateLastActive(
    MusicConditionObject& object,
    const MusicConditionConstructionContext& context) noexcept
{
    constructListened(object, context, 0x00C56AB8, 0x00C56AA8, 0x00C56AA0);
    write8(object, 0x1C, 0);
    write8(object, 0x1D, 0);
    write32(object, 0x20, 0);
    if (context.indexedTrackedState) {
        write8(object, 0x1C, 1);
    }
}

void constructSpecificTrackTypePlaying(
    MusicConditionObject& object,
    const MusicConditionConstructionContext& context) noexcept
{
    constructSimple(object, context, 0x00C56AD8, 0x00C56AC8);
}

void constructTimeFromStartOfLevel(
    MusicConditionObject& object,
    const MusicConditionConstructionContext& context) noexcept
{
    constructSimple(object, context, 0x00C56AF8, 0x00C56AE8);
}

void constructTrackPlayedCount(
    MusicConditionObject& object,
    const MusicConditionConstructionContext& context) noexcept
{
    constructSimple(object, context, 0x00C56B18, 0x00C56B08);
}

void constructEngagementCount(
    MusicConditionObject& object,
    const MusicConditionConstructionContext& context) noexcept
{
    constructSimple(object, context, 0x00C56B38, 0x00C56B28);
    write32(object, 0x10, 0);
    write8(object, 0x14, 0);
    write32(object, 0x18, 0x00C35008);
    write32(object, 0x1C, 0x00C35000);
    write32(object, 0x20, context.objectAddress + 0x20);
    write32(object, 0x24, context.objectAddress + 0x20);
    write32(object, 0x28, 1);
    write32(object, 0x2C, 0);
    write32(object, 0x30, 0x000FFFFF);
    write32(object, 0x34, 0x000FFFFF);
    write32(object, 0x38, 2);
    write32(object, 0x40, context.objectAddress + 0x40);
    write32(object, 0x44, context.objectAddress + 0x40);
    write32(object, 0x48, 0);
    write32(object, 0x4C, 0);
    write32(object, 0x50, 0);
    write32(object, 0x58, 0);
    write32(object, 0x5C, 0);
    write32(object, 0x60, 0);
    write32(object, 0x64, 0);
    write32(object, 0x64, context.localPlayer);
    if (context.refresh != nullptr) {
        context.refresh(context.actionContext);
    }
}

void constructHitBySuperWeapon(
    MusicConditionObject& object,
    const MusicConditionConstructionContext& context,
    std::uint32_t durationBits) noexcept
{
    constructListened(object, context, 0x00C56B60, 0x00C56B50, 0x00C56B48);
    const float duration = floatFromBits(durationBits);
    const float ticksPerSecond = floatFromBits(context.ticksPerSecondBits);
    const float gameSpeed = floatFromBits(context.gameSpeedBits);
    std::uint32_t ticks = 0;
    if (context.hasGameClock) {
        ticks = truncateHitDuration(duration, gameSpeed, ticksPerSecond);
    } else {
        const double ceiling = std::ceil(
            static_cast<double>(duration) * static_cast<double>(ticksPerSecond));
        constexpr double int64Limit = 9223372036854775808.0;
        if (std::isfinite(ceiling)
            && ceiling >= -int64Limit && ceiling < int64Limit) {
            ticks = static_cast<std::uint32_t>(
                static_cast<std::int64_t>(ceiling));
        }
    }
    write32(object, 0x1C, ticks);
    write32(object, 0x20, 0);
    write32(object, 0x34, kOneBits);
    write32(object, 0x38, kLargeDurationBits);
    write32(object, 0x2C, 1);
    write32(object, 0x28, 0x00D0FEEC);
    write32(object, 0x30, 0);
    write32(object, 0x3C, 0);
}

void constructObjectsNearEvaEvent(
    MusicConditionObject& object,
    const MusicConditionConstructionContext& context) noexcept
{
    constructSimple(object, context, 0x00C56B80, 0x00C56B70);
    write32(object, 0x10, 0);
    write8(object, 0x14, 0);
}

void constructObjectsOfTypeExist(
    MusicConditionObject& object,
    const MusicConditionConstructionContext& context) noexcept
{
    constructSimple(object, context, 0x00C56BA0, 0x00C56B90);
    write32(object, 0x10, 0);
    write8(object, 0x14, 0);
    write8(object, 0x18, context.objectFilterPresent ? 1 : 0);
}

void constructUnitsFarFromBase(
    MusicConditionObject& object,
    const MusicConditionConstructionContext& context) noexcept
{
    constructSimple(object, context, 0x00C56BC0, 0x00C56BB0);
    write32(object, 0x10, 0);
    write8(object, 0x14, 0);
}

void constructLocalPlayerKillRatio(
    MusicConditionObject& object,
    const MusicConditionConstructionContext& context) noexcept
{
    constructSimple(object, context, 0x00C56C20, 0x00C56C10);
    write32(object, 0x10, 0);
    write32(object, 0x14, 0);
    write32(object, 0x18, 0);
    write32(object, 0x20, 0x00C350EC);
    write32(object, 0x24, 1);
    write32(object, 0x28, 0);
    write32(object, 0x2C, 0x000FFFFF);
    write32(object, 0x30, 0);
    write32(object, 0x34, 0xFFFFFFFF);
    write32(object, 0x38, context.objectAddress + 0x38);
    write32(object, 0x3C, context.objectAddress + 0x38);
    write32(object, 0x44, 0);
    write32(object, 0x48, 0);
    write32(object, 0x4C, 0);
    write32(object, 0x50, 0);
    write32(object, 0x54, 0);
    write32(object, 0x58, 0);
    if (context.clear != nullptr) {
        context.clear(context.actionContext);
    }
    write32(object, 0x54, context.localPlayer);
    write32(object, 0x58, 0);
    if (context.refresh != nullptr) {
        context.refresh(context.actionContext);
    }
}

void constructScoredKillCount(
    MusicConditionObject& object,
    const MusicConditionConstructionContext& context) noexcept
{
    constructSimple(object, context, 0x00C56C40, 0x00C56C30);
    write32(object, 0x10, 0);
    write32(object, 0x14, 0);
    write32(object, 0x18, 0);
    write32(object, 0x20, 0);
    write32(object, 0x24, 0);
    if (context.clear != nullptr) {
        context.clear(context.actionContext);
    }
    write32(object, 0x20, context.localPlayer);
    write32(object, 0x24, 0);
    if (context.refresh != nullptr) {
        context.refresh(context.actionContext);
    }
}

bool compareMusicValue(
    MusicComparison comparison, float value, float reference) noexcept
{
    constexpr std::uint32_t epsilonBits = 0x3A83126F;
    const float epsilon = floatFromBits(epsilonBits);
    switch (comparison) {
    case MusicComparison::less: return value < reference;
    case MusicComparison::lessOrEqual: return value <= reference + epsilon;
    case MusicComparison::approximatelyEqual:
        return value <= reference + epsilon && reference - epsilon <= value;
    case MusicComparison::greaterOrEqual: return reference - epsilon <= value;
    case MusicComparison::greater: return reference < value;
    }
    return false;
}

bool musicStateIsActive(bool hasState, bool active) noexcept
{
    return hasState && active;
}

void updateTrackedMusicState(
    bool enabled, std::uint32_t trackedState, std::uint32_t eventState,
    bool active, bool& state) noexcept
{
    if (enabled && trackedState == eventState) {
        state = active;
    }
}

void updateTimedTrackedMusicState(
    bool enabled, std::uint32_t trackedState, std::uint32_t eventState,
    std::uint32_t currentFrame, std::uint32_t& changedAt,
    bool& state) noexcept
{
    if (enabled && trackedState == eventState) {
        changedAt = currentFrame;
        state = true;
    }
}

bool allMusicConditions(
    std::size_t count, MusicConditionPredicate predicate,
    void* context) noexcept
{
    for (std::size_t index = 0; index < count; ++index) {
        if (!predicate(context, index)) {
            return false;
        }
    }
    return true;
}

bool anyMusicCondition(
    std::size_t count, MusicConditionPredicate predicate,
    void* context) noexcept
{
    for (std::size_t index = 0; index < count; ++index) {
        if (predicate(context, index)) {
            return true;
        }
    }
    return false;
}

bool negateMusicCondition(
    bool hasCondition, MusicConditionPredicate predicate,
    void* context) noexcept
{
    return hasCondition && !predicate(context, 0);
}

bool engagementCountSatisfied(std::uint32_t count, std::uint32_t required) noexcept
{
    return required <= count;
}

bool timeFromStartSatisfied(
    std::uint32_t currentFrame, std::uint32_t framesPerSecond,
    float seconds) noexcept
{
    return extendedTimeFromStart(currentFrame, framesPerSecond, seconds);
}

bool lastMusicStateSatisfied(
    bool hasState, bool active, std::uint32_t currentFrame,
    std::uint32_t changedAt, std::uint32_t framesPerSecond,
    float time, MusicComparison comparison) noexcept
{
    if (!hasState) {
        return false;
    }
    float elapsed = kMaximumStateAge;
    if (active) {
        const std::uint32_t frames = currentFrame - changedAt;
        elapsed = extendedElapsed(frames, framesPerSecond);
        if (!(elapsed < kMaximumStateAge)) {
            elapsed = kMaximumStateAge;
        }
    }
    return compareMusicValue(comparison, elapsed, time);
}

bool scoredKillCountSatisfied(
    std::uint32_t& cachedPlayer, std::uint32_t& cachedVersion,
    std::uint32_t localPlayer, std::uint32_t currentVersion,
    std::int32_t required, MusicScoreRangeProvider scores,
    MusicConditionAction clear, MusicConditionAction refresh,
    void* context) noexcept
{
    if (cachedPlayer != localPlayer || currentVersion < cachedVersion) {
        clear(context);
        cachedPlayer = localPlayer;
        cachedVersion = 0;
        refresh(context);
        return false;
    }
    if (cachedVersion < currentVersion) {
        refresh(context);
    }
    const MusicScoreRange range = scores(context);
    std::uint32_t totalBits = 0;
    std::int32_t total = 0;
    for (std::size_t index = 0; index < range.count && total < required; ++index) {
        totalBits += static_cast<std::uint32_t>(range.scores[index]);
        std::memcpy(&total, &totalBits, sizeof(total));
    }
    return required <= total;
}

bool cachedMusicCondition(
    std::uint32_t& nextCheck, bool& cached,
    float interval, float checksPerSecond,
    MusicConditionPredicate predicate, void* predicateContext,
    MusicFrameProvider frameProvider, void* frameContext) noexcept
{
    if (nextCheck <= frameProvider(frameContext)) {
        cached = predicate(predicateContext, 0);
        nextCheck = truncateNextCheck(
            frameProvider(frameContext), interval, checksPerSecond);
    }
    return cached;
}

bool localPlayerObserverCondition(
    bool hasLocalPlayer, bool firstQuery, bool playerFlag,
    bool secondQuery, bool configuredFallback) noexcept
{
    if (!hasLocalPlayer || firstQuery || playerFlag) {
        return true;
    }
    if (secondQuery) {
        return false;
    }
    return configuredFallback;
}

bool trackPlayedCountSatisfied(
    std::uint32_t& cachedPlayer, std::uint32_t localPlayer,
    std::int32_t required, MusicCountProvider played,
    MusicConditionAction refresh, void* context) noexcept
{
    if (cachedPlayer != localPlayer) {
        cachedPlayer = localPlayer;
        refresh(context);
        return false;
    }
    return required <= played(context);
}

const MusicReconstructionEntry* musicReconstructionEntries(
    std::size_t& count) noexcept
{
    count = sizeof(kEntries) / sizeof(kEntries[0]);
    return kEntries;
}

}  // namespace ra3::gameclient
