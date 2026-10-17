#include "../Targets.h"

#include <ra3/gameclient/GameClient92.h>

#include <cstring>
#include <string>

namespace ra3hook {
namespace {

using TypeName = const char* (*)() noexcept;

const char* shadowTypeName(
    const char* key, TargetState& state, TypeName implementation)
{
    state.calls.fetch_add(1, std::memory_order_relaxed);
    const char* const ours = implementation();
    if (state.mode == Mode::replace) {
        state.matches.fetch_add(1, std::memory_order_relaxed);
        return ours;
    }
    const char* const game =
        reinterpret_cast<const char*(__cdecl*)()>(state.original)();
    if (std::strcmp(game, ours) == 0) {
        state.matches.fetch_add(1, std::memory_order_relaxed);
    } else {
        reportMismatch(
            key, state, std::string("game=") + game + " ours=" + ours);
    }
    return game;
}

#define RA3_GC92_TYPE_NAME(wrapper, keyText, implementation) \
    TargetState g_##wrapper; \
    const char* __cdecl wrapper() \
    { \
        return shadowTypeName(keyText, g_##wrapper, implementation); \
    }

RA3_GC92_TYPE_NAME(
    conditionFactoryName, "gc92conditionfactoryname",
    &ra3::gameclient::musicConditionFactoryTypeName)
RA3_GC92_TYPE_NAME(
    andConditionName, "gc92andconditionname",
    &ra3::gameclient::andConditionTypeName)
RA3_GC92_TYPE_NAME(
    anyTrackPlayingName, "gc92anytrackplayingname",
    &ra3::gameclient::anyTrackPlayingTypeName)
RA3_GC92_TYPE_NAME(
    engagementCountName, "gc92engagementcountname",
    &ra3::gameclient::engagementCountTypeName)
RA3_GC92_TYPE_NAME(
    evaEventRecentName, "gc92evaeventrecentname",
    &ra3::gameclient::evaEventPlayedRecentlyTypeName)
RA3_GC92_TYPE_NAME(
    hitBySuperWeaponName, "gc92hitbysuperweaponname",
    &ra3::gameclient::hitBySuperWeaponTypeName)
RA3_GC92_TYPE_NAME(
    localObserverName, "gc92localobservername",
    &ra3::gameclient::localPlayerIsObserverTypeName)
RA3_GC92_TYPE_NAME(
    localKillRatioName, "gc92localkillrationame",
    &ra3::gameclient::localPlayerKillRatioTypeName)
RA3_GC92_TYPE_NAME(
    notConditionName, "gc92notconditionname",
    &ra3::gameclient::notConditionTypeName)
RA3_GC92_TYPE_NAME(
    objectsNearEvaName, "gc92objectsnearevaname",
    &ra3::gameclient::objectsNearEvaEventTypeName)
RA3_GC92_TYPE_NAME(
    objectsOfTypeName, "gc92objectsoftypename",
    &ra3::gameclient::objectsOfTypeExistTypeName)
RA3_GC92_TYPE_NAME(
    orConditionName, "gc92orconditionname",
    &ra3::gameclient::orConditionTypeName)
RA3_GC92_TYPE_NAME(
    currentPathStateName, "gc92currentpathstatename",
    &ra3::gameclient::pathMusicStateCurrentlyActiveTypeName)
RA3_GC92_TYPE_NAME(
    lastPathStateName, "gc92lastpathstatename",
    &ra3::gameclient::pathMusicStateLastActiveTypeName)
RA3_GC92_TYPE_NAME(
    scoredKillCountName, "gc92scoredkillcountname",
    &ra3::gameclient::scoredKillCountTypeName)
RA3_GC92_TYPE_NAME(
    specificTrackTypeName, "gc92specifictracktypename",
    &ra3::gameclient::specificTrackTypePlayingTypeName)
RA3_GC92_TYPE_NAME(
    timeFromStartName, "gc92timefromstartname",
    &ra3::gameclient::timeFromStartOfLevelTypeName)
RA3_GC92_TYPE_NAME(
    trackPlayedCountName, "gc92trackplayedcountname",
    &ra3::gameclient::trackPlayedCountTypeName)
RA3_GC92_TYPE_NAME(
    unitsFarFromBaseName, "gc92unitsfarfrombasename",
    &ra3::gameclient::unitsFarFromBaseTypeName)
RA3_GC92_TYPE_NAME(
    musicScriptingSystemName, "gc92musicscriptingsystemname",
    &ra3::gameclient::musicScriptingSystemTypeName)

#undef RA3_GC92_TYPE_NAME

}  // namespace

void appendGameClient92Targets(std::vector<Target>& table)
{
    table.insert(table.end(), {
        {"gc92conditionfactoryname", 0x00523740,
         {0xB8, 0x2C, 0x69, 0xC5, 0x00, 0xC3, 0xCC, 0xCC,
          0xCC, 0xCC, 0xCC, 0xCC, 0xCC, 0xCC, 0xCC, 0xCC},
         5, reinterpret_cast<void*>(&conditionFactoryName), &g_conditionFactoryName, true},
        {"gc92andconditionname", 0x00523750,
         {0xB8, 0xE8, 0x63, 0xBC, 0x00, 0xC3, 0xCC, 0xCC,
          0xCC, 0xCC, 0xCC, 0xCC, 0xCC, 0xCC, 0xCC, 0xCC},
         5, reinterpret_cast<void*>(&andConditionName), &g_andConditionName, true},
        {"gc92anytrackplayingname", 0x00523760,
         {0xB8, 0x08, 0x65, 0xBC, 0x00, 0xC3, 0xCC, 0xCC,
          0xCC, 0xCC, 0xCC, 0xCC, 0xCC, 0xCC, 0xCC, 0xCC},
         5, reinterpret_cast<void*>(&anyTrackPlayingName), &g_anyTrackPlayingName, true},
        {"gc92engagementcountname", 0x00523890,
         {0xB8, 0x4C, 0xF6, 0xBB, 0x00, 0xC3, 0xCC, 0xCC,
          0xCC, 0xCC, 0xCC, 0xCC, 0xCC, 0xCC, 0xCC, 0xCC},
         5, reinterpret_cast<void*>(&engagementCountName), &g_engagementCountName, true},
        {"gc92evaeventrecentname", 0x00523950,
         {0xB8, 0xA4, 0x64, 0xBC, 0x00, 0xC3, 0xCC, 0xCC,
          0xCC, 0xCC, 0xCC, 0xCC, 0xCC, 0xCC, 0xCC, 0xCC},
         5, reinterpret_cast<void*>(&evaEventRecentName), &g_evaEventRecentName, true},
        {"gc92hitbysuperweaponname", 0x00523960,
         {0xB8, 0x70, 0xEF, 0xBB, 0x00, 0xC3, 0xCC, 0xCC,
          0xCC, 0xCC, 0xCC, 0xCC, 0xCC, 0xCC, 0xCC, 0xCC},
         5, reinterpret_cast<void*>(&hitBySuperWeaponName), &g_hitBySuperWeaponName, true},
        {"gc92localobservername", 0x005239C0,
         {0xB8, 0xF0, 0x65, 0xBC, 0x00, 0xC3, 0xCC, 0xCC,
          0xCC, 0xCC, 0xCC, 0xCC, 0xCC, 0xCC, 0xCC, 0xCC},
         5, reinterpret_cast<void*>(&localObserverName), &g_localObserverName, true},
        {"gc92localkillrationame", 0x005239D0,
         {0xB8, 0x1C, 0xF6, 0xBB, 0x00, 0xC3, 0xCC, 0xCC,
          0xCC, 0xCC, 0xCC, 0xCC, 0xCC, 0xCC, 0xCC, 0xCC},
         5, reinterpret_cast<void*>(&localKillRatioName), &g_localKillRatioName, true},
        {"gc92notconditionname", 0x005239E0,
         {0xB8, 0x28, 0x64, 0xBC, 0x00, 0xC3, 0xCC, 0xCC,
          0xCC, 0xCC, 0xCC, 0xCC, 0xCC, 0xCC, 0xCC, 0xCC},
         5, reinterpret_cast<void*>(&notConditionName), &g_notConditionName, true},
        {"gc92objectsnearevaname", 0x005239F0,
         {0xB8, 0x74, 0x64, 0xBC, 0x00, 0xC3, 0xCC, 0xCC,
          0xCC, 0xCC, 0xCC, 0xCC, 0xCC, 0xCC, 0xCC, 0xCC},
         5, reinterpret_cast<void*>(&objectsNearEvaName), &g_objectsNearEvaName, true},
        {"gc92objectsoftypename", 0x00523A00,
         {0xB8, 0xD8, 0x64, 0xBC, 0x00, 0xC3, 0xCC, 0xCC,
          0xCC, 0xCC, 0xCC, 0xCC, 0xCC, 0xCC, 0xCC, 0xCC},
         5, reinterpret_cast<void*>(&objectsOfTypeName), &g_objectsOfTypeName, true},
        {"gc92orconditionname", 0x00523A10,
         {0xB8, 0x08, 0x64, 0xBC, 0x00, 0xC3, 0xCC, 0xCC,
          0xCC, 0xCC, 0xCC, 0xCC, 0xCC, 0xCC, 0xCC, 0xCC},
         5, reinterpret_cast<void*>(&orConditionName), &g_orConditionName, true},
        {"gc92currentpathstatename", 0x00523A70,
         {0xB8, 0x78, 0xF6, 0xBB, 0x00, 0xC3, 0xCC, 0xCC,
          0xCC, 0xCC, 0xCC, 0xCC, 0xCC, 0xCC, 0xCC, 0xCC},
         5, reinterpret_cast<void*>(&currentPathStateName), &g_currentPathStateName, true},
        {"gc92lastpathstatename", 0x00523B70,
         {0xB8, 0xB4, 0xF6, 0xBB, 0x00, 0xC3, 0xCC, 0xCC,
          0xCC, 0xCC, 0xCC, 0xCC, 0xCC, 0xCC, 0xCC, 0xCC},
         5, reinterpret_cast<void*>(&lastPathStateName), &g_lastPathStateName, true},
        {"gc92scoredkillcountname", 0x00523BE0,
         {0xB8, 0x48, 0x64, 0xBC, 0x00, 0xC3, 0xCC, 0xCC,
          0xCC, 0xCC, 0xCC, 0xCC, 0xCC, 0xCC, 0xCC, 0xCC},
         5, reinterpret_cast<void*>(&scoredKillCountName), &g_scoredKillCountName, true},
        {"gc92specifictracktypename", 0x00523BF0,
         {0xB8, 0x34, 0x65, 0xBC, 0x00, 0xC3, 0xCC, 0xCC,
          0xCC, 0xCC, 0xCC, 0xCC, 0xCC, 0xCC, 0xCC, 0xCC},
         5, reinterpret_cast<void*>(&specificTrackTypeName), &g_specificTrackTypeName, true},
        {"gc92timefromstartname", 0x00523C50,
         {0xB8, 0x94, 0x65, 0xBC, 0x00, 0xC3, 0xCC, 0xCC,
          0xCC, 0xCC, 0xCC, 0xCC, 0xCC, 0xCC, 0xCC, 0xCC},
         5, reinterpret_cast<void*>(&timeFromStartName), &g_timeFromStartName, true},
        {"gc92trackplayedcountname", 0x00523C60,
         {0xB8, 0x68, 0x65, 0xBC, 0x00, 0xC3, 0xCC, 0xCC,
          0xCC, 0xCC, 0xCC, 0xCC, 0xCC, 0xCC, 0xCC, 0xCC},
         5, reinterpret_cast<void*>(&trackPlayedCountName), &g_trackPlayedCountName, true},
        {"gc92unitsfarfrombasename", 0x00523C70,
         {0xB8, 0xC4, 0x65, 0xBC, 0x00, 0xC3, 0xCC, 0xCC,
          0xCC, 0xCC, 0xCC, 0xCC, 0xCC, 0xCC, 0xCC, 0xCC},
         5, reinterpret_cast<void*>(&unitsFarFromBaseName), &g_unitsFarFromBaseName, true},
        {"gc92musicscriptingsystemname", 0x00523D30,
         {0xB8, 0x68, 0x69, 0xC5, 0x00, 0xC3, 0xCC, 0xCC,
          0xCC, 0xCC, 0xCC, 0xCC, 0xCC, 0xCC, 0xCC, 0xCC},
         5, reinterpret_cast<void*>(&musicScriptingSystemName),
         &g_musicScriptingSystemName, true},
    });
}

}  // namespace ra3hook
