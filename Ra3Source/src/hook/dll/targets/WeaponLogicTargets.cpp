#include "../Targets.h"

#include <ra3/gamelogic/WeaponLogic.h>

#include <array>
#include <cstdint>
#include <cstring>

namespace ra3hook {
namespace {

TargetState targetPlayerState;
TargetState kindMaskState;
TargetState lineOfSightState;
TargetState statusBitState;
TargetState overlapCountState;
TargetState controllerHeightState;
TargetState clearSlaveLinksState;
TargetState guardAreaState;
TargetState slavedState;
TargetState configurationBitState;
TargetState riderState;

template <typename T>
T load(const void* base, std::size_t offset)
{
    T result{};
    std::memcpy(&result, static_cast<const std::uint8_t*>(base) + offset, sizeof(result));
    return result;
}

std::uint32_t addressValue(const void* value)
{
    return static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(value));
}

std::uint32_t __fastcall targetPlayerHook(
    const void* self, void*, const void* candidate)
{
    ++targetPlayerState.calls;
    const auto* moduleData = load<const void*>(self, 4);
    const auto* source = load<const void*>(self, 8);
    const auto ours = ra3::gamelogic::rejectDifferentControllingPlayer(
        addressValue(moduleData), load<std::uint32_t>(moduleData, 0x104),
        addressValue(source), load<std::uint32_t>(source, 0x418),
        load<std::uint32_t>(candidate, 0x418));
    if (targetPlayerState.mode == Mode::replace) {
        return ours;
    }
    using Original = std::uint32_t(__thiscall*)(const void*, const void*);
    const auto game =
        reinterpret_cast<Original>(targetPlayerState.original)(self, candidate);
    recordScalar("weapontargetplayer", targetPlayerState, game, ours);
    return game;
}

std::uint32_t __fastcall kindMaskHook(
    const void* self, void*, const void* candidate)
{
    ++kindMaskState.calls;
    const auto* moduleData = load<const std::uint8_t*>(self, 4);
    const auto* source = load<const std::uint8_t*>(self, 8);
    const auto ours = ra3::gamelogic::weaponKindMaskMatches(
        reinterpret_cast<const std::uint32_t*>(moduleData + 0xB4),
        reinterpret_cast<const std::uint32_t*>(source + 0x80),
        reinterpret_cast<const std::uint32_t*>(moduleData + 0x98),
        reinterpret_cast<const std::uint32_t*>(
            static_cast<const std::uint8_t*>(candidate) + 0x80));
    if (kindMaskState.mode == Mode::replace) {
        return ours;
    }
    using Original = std::uint32_t(__thiscall*)(const void*, const void*);
    const auto game = reinterpret_cast<Original>(kindMaskState.original)(self, candidate);
    recordScalar("weaponkindmask", kindMaskState, game, ours);
    return game;
}

std::uint32_t __fastcall lineOfSightHook(const void* self, void*)
{
    ++lineOfSightState.calls;
    const auto* moduleData = load<const void*>(self, 4);
    const auto ours = ra3::gamelogic::weaponUsesLineOfSight(
        load<std::uint32_t>(moduleData, 0x104));
    if (lineOfSightState.mode == Mode::replace) {
        return ours;
    }
    using Original = std::uint32_t(__thiscall*)(const void*);
    const auto game = reinterpret_cast<Original>(lineOfSightState.original)(self);
    recordScalar("weaponlineofsight", lineOfSightState, game, ours);
    return game;
}

std::uint32_t __cdecl statusBitHook(const void* object)
{
    ++statusBitState.calls;
    const auto ours =
        ra3::gamelogic::objectStatusBit9(load<std::uint32_t>(object, 0xC0));
    if (statusBitState.mode == Mode::replace) {
        return ours;
    }
    using Original = std::uint32_t(__cdecl*)(const void*);
    const auto game = reinterpret_cast<Original>(statusBitState.original)(object);
    recordScalar("objectstatusbit9", statusBitState, game, ours);
    return game;
}

std::int32_t __fastcall overlapCountHook(
    const void* self, void*, std::uint32_t ownerValue)
{
    ++overlapCountState.calls;
    const auto* head =
        load<const ra3::gamelogic::PhysicsOverlapNode*>(self, 0x10);
    const auto ours =
        ra3::gamelogic::physicsCountOverlaps(head, ownerValue);
    if (overlapCountState.mode == Mode::replace) {
        return ours;
    }
    using Original = std::int32_t(__thiscall*)(const void*, std::uint32_t);
    const auto game =
        reinterpret_cast<Original>(overlapCountState.original)(self, ownerValue);
    recordScalar("physicsoverlapcount", overlapCountState,
                 static_cast<std::uint32_t>(game),
                 static_cast<std::uint32_t>(ours));
    return game;
}

std::uint32_t __fastcall controllerHeightHook(
    const void* self, void*, const void* heightObject)
{
    ++controllerHeightState.calls;
    const auto* object = load<const void*>(self, 8);
    const bool hasObject = object != nullptr;
    const bool hasController =
        hasObject && load<const void*>(object, 0x418) != nullptr;
    const auto ours = ra3::gamelogic::physicsHasControllerAtHeight(
        addressValue(object), hasObject, hasController,
        addressValue(heightObject), load<float>(heightObject, 0x1C));
    if (controllerHeightState.mode == Mode::replace) {
        return ours;
    }
    using Original = std::uint32_t(__thiscall*)(const void*, const void*);
    const auto game = reinterpret_cast<Original>(
        controllerHeightState.original)(self, heightObject);
    recordScalar("physicscontrollerheight", controllerHeightState, game, ours);
    return game;
}

std::uint32_t __fastcall clearSlaveLinksHook(void* self, void*)
{
    ++clearSlaveLinksState.calls;
    if (clearSlaveLinksState.mode == Mode::replace) {
        ra3::gamelogic::clearObjectSlaveLinks(self);
        return 0;
    }
    std::array<std::uint32_t, 2> ours{
        load<std::uint32_t>(self, 0x12F4),
        load<std::uint32_t>(self, 0x34C)};
    ours[0] = 0;
    ours[1] = 0;
    using Original = std::uint32_t(__thiscall*)(void*);
    const auto game = reinterpret_cast<Original>(clearSlaveLinksState.original)(self);
    const std::array<std::uint32_t, 2> gameState{
        load<std::uint32_t>(self, 0x12F4),
        load<std::uint32_t>(self, 0x34C)};
    if (game == 0) {
        recordOutput(
            "objectclearslavelinks", clearSlaveLinksState,
            gameState.data(), ours.data(), sizeof(gameState));
    } else {
        reportMismatch(
            "objectclearslavelinks", clearSlaveLinksState,
            "EAX differs from the required zero result");
    }
    return game;
}

std::uint32_t __fastcall guardAreaHook(
    const void* self, void*, const float* position)
{
    ++guardAreaState.calls;
    const auto* begin = load<const float*>(self, 0x24);
    const auto* end = load<const float*>(self, 0x28);
    const auto* moduleData = load<const void*>(self, 4);
    const auto count = static_cast<std::size_t>(end - begin) / 3u;
    const auto ours = ra3::gamelogic::slavedPointOutsideGuardArea(
        begin, count, position, load<float>(moduleData, 0x1C),
        addressValue(begin));
    if (guardAreaState.mode == Mode::replace) {
        return ours;
    }
    using Original = std::uint32_t(__thiscall*)(const void*, const float*);
    const auto game =
        reinterpret_cast<Original>(guardAreaState.original)(self, position);
    recordScalar("slavedguardarea", guardAreaState, game, ours);
    return game;
}

std::uint32_t __fastcall slavedStateHook(
    const void* self, void*, std::uint32_t unused)
{
    ++slavedState.calls;
    (void)unused;
    const auto* moduleData = load<const void*>(self, 4);
    const auto* object = load<const void*>(self, 8);
    const auto ours = ra3::gamelogic::slavedStateAllowed(
        load<std::uint32_t>(moduleData, 0x84),
        load<std::uint32_t>(object, 0xA0),
        load<std::uint32_t>(object, 0xB0),
        load<std::uint32_t>(object, 0xC0));
    if (slavedState.mode == Mode::replace) {
        return ours;
    }
    using Original = std::uint32_t(__thiscall*)(const void*, std::uint32_t);
    const auto game =
        reinterpret_cast<Original>(slavedState.original)(self, unused);
    recordScalar("slavedstateallowed", slavedState, game, ours);
    return game;
}

std::uint32_t __fastcall configurationBitHook(const void* self, void*)
{
    ++configurationBitState.calls;
    const auto* moduleData = load<const void*>(self, 4);
    const auto ours = ra3::gamelogic::slavedConfigurationBit22(
        load<std::uint32_t>(moduleData, 0x2C));
    if (configurationBitState.mode == Mode::replace) {
        return ours;
    }
    using Original = std::uint32_t(__thiscall*)(const void*);
    const auto game =
        reinterpret_cast<Original>(configurationBitState.original)(self);
    recordScalar("slavedconfigurationbit22", configurationBitState, game, ours);
    return game;
}

std::uint32_t __fastcall riderHook(const void* self, void*)
{
    ++riderState.calls;
    const auto* object = load<const void*>(self, 8);
    const auto* moduleData = load<const void*>(self, 4);
    const bool hasRiderModule = load<const void*>(object, 0x374) != nullptr;
    const auto ours = ra3::gamelogic::slavedHasRider(
        addressValue(object), hasRiderModule, addressValue(moduleData),
        load<std::uint32_t>(moduleData, 0x2C),
        load<std::uint8_t>(moduleData, 0xEE));
    if (riderState.mode == Mode::replace) {
        return ours;
    }
    using Original = std::uint32_t(__thiscall*)(const void*);
    const auto game = reinterpret_cast<Original>(riderState.original)(self);
    recordScalar("slavedhasrider", riderState, game, ours);
    return game;
}

}  // namespace

void appendWeaponLogicTargets(std::vector<Target>& table)
{
    table.push_back({"weapontargetplayer", 0x0029A610,
        {0x8B, 0x41, 0x04, 0x8B, 0x90, 0x04, 0x01, 0x00, 0x00, 0xC1, 0xEA, 0x02, 0xF6, 0xC2, 0x01, 0x74},
        9, reinterpret_cast<void*>(&targetPlayerHook), &targetPlayerState, true});
    table.push_back({"weaponkindmask", 0x0029A640,
        {0x56, 0x8B, 0x71, 0x04, 0x57, 0x33, 0xC0, 0x8D, 0x96, 0xB4, 0x00, 0x00, 0x00, 0x8D, 0x49, 0x00},
        5, reinterpret_cast<void*>(&kindMaskHook), &kindMaskState, true});
    table.push_back({"weaponlineofsight", 0x0029A710,
        {0x8B, 0x41, 0x04, 0x8B, 0x80, 0x04, 0x01, 0x00, 0x00, 0xC1, 0xE8, 0x11, 0x83, 0xE0, 0x01, 0xC3},
        9, reinterpret_cast<void*>(&lineOfSightHook), &lineOfSightState, true});
    table.push_back({"objectstatusbit9", 0x0029E400,
        {0x8B, 0x44, 0x24, 0x04, 0x8B, 0x80, 0xC0, 0x00, 0x00, 0x00, 0xC1, 0xE8, 0x09, 0x83, 0xE0, 0x01},
        10, reinterpret_cast<void*>(&statusBitHook), &statusBitState, true});
    table.push_back({"physicsoverlapcount", 0x0029E820,
        {0x8B, 0x49, 0x10, 0x33, 0xC0, 0x85, 0xC9, 0x74, 0x1C, 0x8B, 0x54, 0x24, 0x04, 0x8D, 0x49, 0x00},
        5, reinterpret_cast<void*>(&overlapCountHook), &overlapCountState, true});
    table.push_back({"physicscontrollerheight", 0x0029E850,
        {0x8B, 0x41, 0x08, 0x85, 0xC0, 0x74, 0x20, 0x83, 0xB8, 0x18, 0x04, 0x00, 0x00, 0x00, 0x74, 0x17},
        5, reinterpret_cast<void*>(&controllerHeightHook), &controllerHeightState, true});
    table.push_back({"objectclearslavelinks", 0x0029F760,
        {0x33, 0xC0, 0x89, 0x81, 0xF4, 0x12, 0x00, 0x00, 0x89, 0x81, 0x4C, 0x03, 0x00, 0x00, 0xC3, 0xCC},
        8, reinterpret_cast<void*>(&clearSlaveLinksHook), &clearSlaveLinksState, true});
    table.push_back({"slavedguardarea", 0x0029F8D0,
        {0x8B, 0x41, 0x24, 0x8B, 0x51, 0x28, 0x3B, 0xC2, 0x74, 0x69, 0x8B, 0x49, 0x04, 0xF3, 0x0F, 0x10},
        6, reinterpret_cast<void*>(&guardAreaHook), &guardAreaState, true});
    table.push_back({"slavedstateallowed", 0x0029F9E0,
        {0x51, 0x8B, 0x51, 0x08, 0x8B, 0x49, 0x04, 0x53, 0x8A, 0x99, 0x84, 0x00, 0x00, 0x00, 0x80, 0xE3},
        7, reinterpret_cast<void*>(&slavedStateHook), &slavedState, true});
    table.push_back({"slavedconfigurationbit22", 0x0029FC50,
        {0x8B, 0x41, 0x04, 0x8B, 0x40, 0x2C, 0xC1, 0xE8, 0x16, 0x83, 0xE0, 0x01, 0xC3, 0xCC, 0xCC, 0xCC},
        6, reinterpret_cast<void*>(&configurationBitHook), &configurationBitState, true});
    table.push_back({"slavedhasrider", 0x0029FC60,
        {0x8B, 0x41, 0x08, 0x83, 0xB8, 0x74, 0x03, 0x00, 0x00, 0x00, 0x75, 0x03, 0x32, 0xC0, 0xC3, 0x8B},
        10, reinterpret_cast<void*>(&riderHook), &riderState, true});
}

}  // namespace ra3hook
