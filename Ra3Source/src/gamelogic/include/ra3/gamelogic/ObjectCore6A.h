#pragma once

#include <cstddef>
#include <cstdint>

namespace ra3::gamelogic {

// Module bodies in 0x006A0000-0x006BFFFF whose vtable slot-2 function
// returns the class name. Field offsets are the ones the RA3 bytes use.
// External callees are explicit so a test can supply their EAX.

inline constexpr std::size_t kModule6ABytes = 0x100;

struct ModuleResult6A {
    std::uint32_t eax;
    char trace[160];
};

// 0x006A23A0. EncloseBehavior vtable 00C08A48 slot 30.
// EAX is the list pointer with AL replaced by 1 when the needle is present.
std::uint32_t encloseContainsId(
    const std::uint32_t* ids, std::uint32_t count, std::uint32_t needle,
    std::uint32_t listAddress) noexcept;

// 0x006A16A0. WeaponSetUpdate vtable 00C1F320 slot 1.
// EAX is the incoming register when the weapon list is empty, otherwise
// the last slot+0x1C return.
ModuleResult6A weaponSetCallSlot1C(
    std::uint32_t incomingEax, const std::uint32_t* slotReturns, std::size_t count) noexcept;

// 0x006B13F0. SlaughterHordeContain vtable 00C22E28 slot 32.
// EAX is 0 or 1 from the helper's full register.
std::uint32_t slaughterContainedFlag(std::uint32_t helperEax) noexcept;

// 0x006AFCB0. VeterancyCrateCollide vtable 00C1A4E0 slot 22.
ModuleResult6A veterancyCrateCanPass(
    std::uint32_t helper6908, std::uint8_t objectFlag471, std::uint32_t objectStatus80,
    std::uint8_t templateByte74, std::uint8_t templateByte75, bool sameTeam,
    std::uint32_t objectTeam, std::uint32_t helper6875) noexcept;

// Slot-3 xfer bodies. `module` is the 0x100-byte image (pattern fill).
// Scripted values are the full EAX of each call, in execution order.
// The trace names the call and, for a pointer argument, its module offset.

ModuleResult6A clearanceSlowDeathXfer(std::uint32_t versionEax, std::uint32_t helperEax) noexcept;
ModuleResult6A techUpgradeXfer(std::uint32_t versionEax, std::uint32_t helperEax) noexcept;
ModuleResult6A dozerMoveToXfer(std::uint32_t versionEax, std::uint32_t blockEax) noexcept;
ModuleResult6A dozerStateMachineXfer(std::uint32_t versionEax, std::uint32_t blockEax) noexcept;
ModuleResult6A dozerPickXfer(
    std::uint32_t versionEax, std::uint32_t blockEax, std::uint32_t tailEax) noexcept;
ModuleResult6A dozerDoXfer(
    std::uint32_t versionEax, std::uint32_t blockEax, std::uint32_t tailEax) noexcept;
ModuleResult6A dozerActionXfer(
    const std::uint8_t module[kModule6ABytes], std::uint32_t versionEax,
    std::uint32_t blockEax, std::uint32_t valueEax) noexcept;
ModuleResult6A subObjectsUpgradeXfer(
    std::uint32_t versionEax, std::uint32_t blockEax, std::uint32_t localEax,
    std::uint32_t helperEax) noexcept;
ModuleResult6A experienceTrackerObjectXfer(
    std::uint32_t versionEax, std::uint32_t helper3d10, std::uint32_t helper54a090,
    std::uint32_t tailEax) noexcept;
ModuleResult6A audioLoopUpgradeXfer(
    std::uint32_t versionEax, std::uint32_t globalEax, std::uint32_t baseEax,
    std::uint32_t subEax) noexcept;
ModuleResult6A createAndEnterXfer(
    std::uint32_t versionEax0, std::uint32_t versionEax1, std::uint32_t blockEax,
    std::uint32_t helperEax) noexcept;
// xferPlus4 is the dword at xfer+4. Bit 2 selects the short path.
ModuleResult6A techUpgradeReceiverXfer(
    std::uint32_t xferPlus4, std::uint32_t versionEax, std::uint32_t baseEax,
    std::uint32_t helper54a090, std::uint32_t slot64a, std::uint32_t slot64b,
    std::uint32_t slot70) noexcept;

} // namespace ra3::gamelogic
