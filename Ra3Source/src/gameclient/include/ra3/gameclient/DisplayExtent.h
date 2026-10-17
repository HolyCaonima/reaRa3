#pragma once

#include <cstddef>
#include <cstdint>

namespace ra3::gameclient {

// Non-owning view of the display-extent object.  The original class name is
// not recovered: there is no usable RTTI, and these leaves carry no
// source-path assert.
//
// Ownership is the vtables, not proximity.
// - Base vtable 0x00BE06D0 is installed by 0x00532670, which is slot 0 of
//   that table (`mov [esi], 0x00BE06D0`).
// - Derived vtable 0x00BE4BE0 is installed by 0x0058C700
//   (`mov [esi], 0x00BE4BE0`, then secondary vptrs at +0x24, +0x100 and
//   +0x10C).  Slot 0 of the derived table, 0x00593180, calls 0x0058C700.
// The two tables share the leaf slots implemented here.  Derived slot 24
// is the different function 0x005603B0 and is not this cluster.
//
// work/references/generals/GeneralsMD/Code/GameEngine/Include/GameClient/Display.h
// lines 81-84 and 189 were read only as architecture: Generals Display has
// a width/height pair.  Those fields sit at the start of the Generals
// object.  The RA3 offsets below are the offsets in the shipped
// instructions.  No Generals code is copied.  "Primary" (+0x4C) and
// "secondary" (+0x50) name the pair consumed by the unsigned comparison;
// they are not shipped field names.
//
// 0x005D5B20 and 0x0051FBB0 are identical-code-folded with slots on
// vtables 0x00C8DB70 and 0x00C8E9F8.  The body is the field transfer
// below, counted once.
//
// Byte loads and byte stores are `mov al, ...` and leave EAX[31:8] as it
// was on entry.  Dword transfers define all of EAX.  Null `this` is
// outside the contract: every counted method dereferences ECX.
// storeTriple64 and storeQuad11C dereference their source pointer with
// no null test.  Full object size is unknown; the methods below touch
// bytes in [0x4C, 0x12C].
class DisplayExtentObject {
public:
    static constexpr std::size_t minimumRecoveredSize = 0x12D;

    [[nodiscard]] std::uint32_t loadPrimaryExtent() const noexcept;    // 0x0052AF80 slot 17
    std::uint32_t storePrimaryExtent(std::uint32_t value) noexcept;    // 0x005D5B20 slot 15
    [[nodiscard]] std::uint32_t loadSecondaryExtent() const noexcept;  // 0x00740FA0 slot 18
    std::uint32_t storeSecondaryExtent(std::uint32_t value) noexcept;  // 0x00560DB0 slot 16
    [[nodiscard]] std::uint32_t loadFlag5E() const noexcept;           // 0x0052AF90 slot 19
    // 1 iff uint32(4 * secondary) < uint32(3 * primary).  0x0052AFA0 slot 20.
    [[nodiscard]] std::uint32_t secondaryTimesFourBelowPrimaryTimesThree() const noexcept;
    std::uint32_t storeFlag5F(std::uint32_t value) noexcept;           // 0x0052AFC0 slot 22
    [[nodiscard]] std::uint32_t loadFlag5F() const noexcept;           // 0x0052AFD0 slot 23
    std::uint32_t storeFlag60(std::uint32_t value) noexcept;           // 0x0052AFE0 base slot 24
    [[nodiscard]] std::uint32_t loadFlag60() const noexcept;           // 0x00ADA7C0 slot 25
    // Reads source[0], source[1], source[2] one dword at a time, writing
    // +0x64, +0x68, +0x6C after each read.  0x0052AF60 slot 30.
    std::uint32_t storeTriple64(const std::uint32_t* source) noexcept;
    std::uint32_t storePair74(std::uint32_t first, std::uint32_t second) noexcept;  // 0x0051FB90 slot 50
    [[nodiscard]] std::uint32_t loadField74() const noexcept;          // 0x0051FBB0 slot 51
    std::uint32_t storeFieldCC(std::uint32_t value) noexcept;          // 0x0052B000 slot 57
    [[nodiscard]] std::uint32_t loadFlag12C() const noexcept;          // 0x0058C900 derived slot 34
    std::uint32_t storeFlag12C(std::uint32_t value) noexcept;          // 0x0058C910 derived slot 35
    [[nodiscard]] std::uint32_t loadField110() const noexcept;         // 0x0058C920 derived slot 36
    std::uint32_t storeField110(std::uint32_t value) noexcept;         // 0x0058C930 derived slot 37
    // Copies four dwords to +0x11C..+0x128, then sets byte +0x12C to 1.
    // 0x00560800 derived slot 33.
    std::uint32_t storeQuad11C(const std::uint32_t* source) noexcept;
    [[nodiscard]] std::uint32_t loadField118() const noexcept;         // 0x00560A10 derived slot 61
};

// Deliberate inversion of 0x0052AFA0.  Not a shipped function.
[[nodiscard]] std::uint32_t secondaryTimesFourBelowPrimaryTimesThreeMutant(
    const DisplayExtentObject& object) noexcept;

}  // namespace ra3::gameclient
