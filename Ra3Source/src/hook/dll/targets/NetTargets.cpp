#include "../Targets.h"

#include <ra3/net/Transport.h>

#include <array>
#include <cstdint>
#include <cstring>

namespace ra3hook {
namespace {

TargetState rowConnectedState;
TargetState connectionValueState;
TargetState packetHeaderState;
TargetState activePendingState;
TargetState copyRecordState;
TargetState connectionCodeState;
TargetState opaqueValueState;

bool __fastcall rowConnectedHook(void* self, void*, std::uint16_t row)
{
    ++rowConnectedState.calls;
    const bool ours = ra3::net::rowHasConnectedPeer(self, row);
    if (rowConnectedState.mode == Mode::replace) {
        return ours;
    }
    using Original = bool(__thiscall*)(void*, std::uint16_t);
    const auto game = reinterpret_cast<Original>(rowConnectedState.original)(self, row);
    recordScalar("netrowconnected", rowConnectedState,
                 static_cast<std::uint32_t>(game), static_cast<std::uint32_t>(ours));
    return game;
}

std::uint32_t __fastcall connectionValueHook(
    void* self, void*, std::uint16_t first, std::uint16_t second)
{
    ++connectionValueState.calls;
    const auto ours = ra3::net::connectionValue(self, first, second);
    if (connectionValueState.mode == Mode::replace) {
        return ours;
    }
    using Original = std::uint32_t(__thiscall*)(void*, std::uint16_t, std::uint16_t);
    const auto game =
        reinterpret_cast<Original>(connectionValueState.original)(self, first, second);
    recordScalar("netconnectionvalue", connectionValueState, game, ours);
    return game;
}

std::uint32_t __cdecl packetHeaderHook(const void* packet)
{
    ++packetHeaderState.calls;
    const auto ours = ra3::net::packetHeaderSize(packet);
    if (packetHeaderState.mode == Mode::replace) {
        return ours;
    }
    using Original = std::uint32_t(__cdecl*)(const void*);
    const auto game = reinterpret_cast<Original>(packetHeaderState.original)(packet);
    recordScalar("netpacketheadersize", packetHeaderState, game, ours);
    return game;
}

std::int32_t __fastcall activePendingHook(const void* self, void*)
{
    ++activePendingState.calls;
    const auto ours = ra3::net::activePeerPendingTotal(self);
    if (activePendingState.mode == Mode::replace) {
        return ours;
    }
    using Original = std::int32_t(__thiscall*)(const void*);
    const auto game = reinterpret_cast<Original>(activePendingState.original)(self);
    recordScalar("netactivepending", activePendingState,
                 static_cast<std::uint32_t>(game), static_cast<std::uint32_t>(ours));
    return game;
}

void* __fastcall copyRecordHook(void* self, void*, std::uint32_t tag, const void* source)
{
    ++copyRecordState.calls;
    if (copyRecordState.mode == Mode::replace) {
        return ra3::net::copyTransportRecord(self, tag, source);
    }
    std::array<std::uint8_t, 60> ours{};
    std::memcpy(ours.data(), self, ours.size());
    ra3::net::copyTransportRecord(ours.data(), tag, source);
    using Original = void*(__thiscall*)(void*, std::uint32_t, const void*);
    void* game = reinterpret_cast<Original>(copyRecordState.original)(self, tag, source);
    recordOutput("netcopyrecord", copyRecordState, self, ours.data(), ours.size());
    return game;
}

std::uint32_t __cdecl connectionCodeHook(int family, int index, int side)
{
    ++connectionCodeState.calls;
    const auto ours = ra3::net::makeConnectionCode(family, index, side);
    if (connectionCodeState.mode == Mode::replace) {
        return ours;
    }
    using Original = std::uint32_t(__cdecl*)(int, int, int);
    const auto game =
        reinterpret_cast<Original>(connectionCodeState.original)(family, index, side);
    recordScalar("netconnectioncode", connectionCodeState, game, ours);
    return game;
}

std::uint32_t __fastcall opaqueValueHook(void* self, void*, std::uint32_t value)
{
    ++opaqueValueState.calls;
    if (opaqueValueState.mode == Mode::replace) {
        ra3::net::setOpaqueValue(self, value);
        return value;
    }
    std::uint32_t ours = 0;
    std::memcpy(&ours, self, sizeof(ours));
    ra3::net::setOpaqueValue(&ours, value);
    using Original = std::uint32_t(__thiscall*)(void*, std::uint32_t);
    const auto game = reinterpret_cast<Original>(opaqueValueState.original)(self, value);
    if (game == value && std::memcmp(self, &ours, sizeof(ours)) == 0) {
        ++opaqueValueState.matches;
    } else {
        reportMismatch("netopaquevalue", opaqueValueState, "return or four-byte output differs");
    }
    return game;
}

}  // namespace

void appendNetTargets(std::vector<Target>& table)
{
    table.push_back({"netrowconnected", 0x001D6A70,
        {0x66, 0x8B, 0x54, 0x24, 0x04, 0x66, 0x83, 0xFA, 0x06, 0x73, 0x3A, 0x0F, 0xB7, 0xD2, 0x56, 0x33},
        5, reinterpret_cast<void*>(&rowConnectedHook), &rowConnectedState, true});
    table.push_back({"netconnectionvalue", 0x001D6C00,
        {0x66, 0x8B, 0x44, 0x24, 0x04, 0x66, 0x3D, 0x06, 0x00, 0x73, 0x21, 0x66, 0x8B, 0x54, 0x24, 0x08},
        5, reinterpret_cast<void*>(&connectionValueHook), &connectionValueState, true});
    table.push_back({"netpacketheadersize", 0x001D72A0,
        {0x56, 0x8B, 0x74, 0x24, 0x08, 0x0F, 0xB7, 0x4E, 0x08, 0xC1, 0xE9, 0x0C, 0x8D, 0x46, 0x0A, 0x74},
        5, reinterpret_cast<void*>(&packetHeaderHook), &packetHeaderState, true});
    table.push_back({"netactivepending", 0x001D7500,
        {0x33, 0xC0, 0x38, 0x81, 0xF0, 0x04, 0x00, 0x00, 0x74, 0x0E, 0x38, 0x81, 0xF1, 0x04, 0x00, 0x00},
        8, reinterpret_cast<void*>(&activePendingHook), &activePendingState, true});
    table.push_back({"netcopyrecord", 0x001D75E0,
        {0x8B, 0xC1, 0x8B, 0x4C, 0x24, 0x04, 0x56, 0x8B, 0x74, 0x24, 0x0C, 0x57, 0x89, 0x08, 0x8D, 0x78},
        6, reinterpret_cast<void*>(&copyRecordHook), &copyRecordState, true});
    table.push_back({"netconnectioncode", 0x001D7640,
        {0x53, 0x8B, 0x5C, 0x24, 0x08, 0x85, 0xDB, 0x75, 0x04, 0xB3, 0x41, 0xEB, 0x0D, 0x83, 0xEB, 0x01},
        5, reinterpret_cast<void*>(&connectionCodeHook), &connectionCodeState, false});
    table.push_back({"netopaquevalue", 0x001D77D0,
        {0x8B, 0x44, 0x24, 0x04, 0x89, 0x01, 0xC2, 0x04, 0x00, 0xCC, 0xCC, 0xCC, 0xCC, 0xCC, 0xCC, 0xCC},
        6, reinterpret_cast<void*>(&opaqueValueHook), &opaqueValueState, true});
}

}  // namespace ra3hook
