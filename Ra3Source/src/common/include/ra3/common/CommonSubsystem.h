#pragma once

#include <cstdint>

namespace ra3::common {

// The common RTS subsystem object itself.  Methods read ECX the way the
// shipped slots do.  Ownership of every method is the vtable slot named
// in the comment, not address adjacency.
//
// Slot 2 of each small vtable is `mov eax, imm32; ret`.  The immediate is
// the ASCII name stored for that record (Money, Energy, Player, Team, ...).
// The function does not read ECX and replaces all of EAX.
//
// work/references/generals/GeneralsMD/Code/GameEngine/Include/Common/PlayerList.h
// and Source/Common/RTS/PlayerList.cpp were read only as architecture: a
// player list owns player, team, money and score records.  No Generals
// field offset or function body is copied.  The RA3 names are the bytes
// the slot returns.
//
// Null `this` is outside the contract of every method that dereferences
// ECX.  The name slots do not dereference ECX.
class CommonSubsystem {
public:
    [[nodiscard]] std::uint32_t disguiseGuardName() const noexcept;          // 0x007e0ba0
    [[nodiscard]] std::uint32_t engagementTrackerName() const noexcept;      // 0x007e1470
    [[nodiscard]] std::uint32_t redAlertName() const noexcept;               // 0x007e24b0
    [[nodiscard]] std::uint32_t scoredKillTrackerName() const noexcept;      // 0x007e25f0
    [[nodiscard]] std::uint32_t moneyName() const noexcept;                  // 0x007e72b0
    [[nodiscard]] std::uint32_t commandName() const noexcept;                // 0x007e72c0
    [[nodiscard]] std::uint32_t playerListName() const noexcept;             // 0x007e7c00
    [[nodiscard]] std::uint32_t energyName() const noexcept;                 // 0x007e9bc0
    [[nodiscard]] std::uint32_t playerRelationMapName() const noexcept;      // 0x007e9ff0
    [[nodiscard]] std::uint32_t playerTechManagerName() const noexcept;      // 0x007eae70
    [[nodiscard]] std::uint32_t resourceGatheringManagerName() const noexcept; // 0x007eb200
    [[nodiscard]] std::uint32_t teamRelationMapName() const noexcept;        // 0x007eb480
    [[nodiscard]] std::uint32_t tunnelTrackerName() const noexcept;          // 0x007ebfc0
    [[nodiscard]] std::uint32_t playerTeamListName() const noexcept;         // 0x007ee9b0
    [[nodiscard]] std::uint32_t teamFactoryName() const noexcept;            // 0x007f0f90
    [[nodiscard]] std::uint32_t playerName() const noexcept;                 // 0x007f5fa0
    [[nodiscard]] std::uint32_t teamTemplateInfoName() const noexcept;       // 0x007f74a0
    [[nodiscard]] std::uint32_t teamName() const noexcept;                   // 0x007f7850
    [[nodiscard]] std::uint32_t teamPrototypeName() const noexcept;          // 0x007f9f70
    [[nodiscard]] std::uint32_t scoreKeeperName() const noexcept;            // 0x007fc3a0
    [[nodiscard]] std::uint32_t logicEvaTrackerName() const noexcept;        // 0x008028f0
    [[nodiscard]] std::uint32_t logicPlayingAudioTrackerName() const noexcept; // 0x00802900
    [[nodiscard]] std::uint32_t logicViewTrackerName() const noexcept;       // 0x00802910

    // uint16 length stored four bytes before the buffer.  0x007e96c0.
    [[nodiscard]] std::uint32_t linkedTextLength() const noexcept;
    // That length plus the dword at node+4.  A null buffer returns the dword.  0x007e9710.
    [[nodiscard]] std::uint32_t linkedTextLengthPlus() const noexcept;

    // +0x10 += 1 when the argument byte is nonzero, else += -1.  EAX is preserved.  0x007e2dd0.
    void adjustTunnelCount(std::uint8_t nonzero) noexcept;

    // Copies three dwords and returns the following byte in AL.  0x007e1710.
    std::uint32_t copyNestedTriple(std::uint32_t* destination) const noexcept;

    // Forwards to nested vtable +0x34, or clears AL.  0x007e16f0.  ret 8.
    std::uint32_t forwardNested34(std::uint32_t first, std::uint32_t second) const noexcept;
    // Forwards to nested vtable +0x38, or leaves EAX unchanged.  0x007e1740.  ret 4.
    std::uint32_t forwardNested38(std::uint32_t argument) const noexcept;

    // Writes the argument's low byte at +0xCF and returns it in AL.  0x008052d0.
    std::uint32_t storeFlagCf(std::uint32_t value) noexcept;
    [[nodiscard]] std::uint32_t byteAt40() const noexcept;   // 0x00805850
    [[nodiscard]] std::uint32_t byteAtD0() const noexcept;   // 0x00805840
    // AL is byte +0xB8 when byte +0xBA is 0, otherwise 0.  0x00805760.
    [[nodiscard]] std::uint32_t byteB8UnlessBa() const noexcept;
    // Same bytes with the +0xBA test inverted.  Not a shipped function.
    [[nodiscard]] std::uint32_t byteB8UnlessBaMutant() const noexcept;

    // 0x00805720.  See CommonSubsystem.cpp for the branch order.
    std::uint32_t availabilityByte() const noexcept;
};

}  // namespace ra3::common
