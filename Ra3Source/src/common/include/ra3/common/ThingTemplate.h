#pragma once

#include <ra3/common/GameString.h>

#include <cstdint>

namespace ra3::common {

// ThingTemplate accessors reconstructed from ra3_1.0.game.
//
// Shipped observations (image base 0x00400000):
// - Constructor 0x00795F70 contains the ThingTemplate.cpp assertions and
//   stores vtable 0x00C2927C at offset 0. That table's slot 2 is 0x0078E810
//   and slot 3 is 0x0078E800, which adds 0xC0 and jumps to 0x0078E7C0.
//   Ghidra did not create a function at 0x0078E7C0; the bytes are a real
//   entry reached only by that jump.
// - 0x0078DE50 is the ThingTemplate.cpp function that calls 0x0078BB10.
//   0x0078E0D0 and 0x0078E1C0 are direct callees of the constructor; they
//   call 0x0078CD90, 0x0078CE10, 0x0078CE90 and 0x0078DCA0. 0x0078DCA0 calls
//   0x0078B6E0. 0x0078E2A0 is the constructor's other in-range callee.
//
// Names below are inferences from those edges and from the nine-dword tests.
// Generals ThingTemplate.h / ThingTemplate.cpp / Thing.h were used only as
// architecture orientation (module lists, a kind mask). RA3's nine dwords,
// offsets, and return residues are not the Generals layout.
//
// Null this is outside the contract: the shipped bodies dereference ecx.
// A null registry, query, or record pointer is likewise outside the contract
// except where a tested path explicitly loads a zero key or a zero child.

// 0x0078E7C0. this points at nine dwords. Compares each query dword as a
// required bit subset. On success EAX is (query + 0x24) with the low byte
// replaced by 1. On failure at index i, EAX is (query + 4*i) with the low
// byte cleared. thiscall, one stack argument, ret 4.
std::uint32_t maskAllNine(
    const std::uint32_t* objectWords, const std::uint32_t* query) noexcept;

// 0x0078E800. Adds 0xC0 to this, then the 0x0078E7C0 test. The add is why
// this entry is not an alias of 0x0078E7C0.
std::uint32_t maskAllNineAtC0(
    const std::uint8_t* object, const std::uint32_t* query) noexcept;

// 0x0078E810. True (EAX 1) when any of the nine dwords at this+0xC0 shares
// a bit with the query dword at the same index. A miss returns EAX 0.
// Index values stay below 256, so the low-byte stores define the full register.
std::uint32_t maskAnyNineAtC0(
    const std::uint8_t* object, const std::uint32_t* query) noexcept;

// Same walk as maskAnyNineAtC0 with the overlap test inverted (je instead of
// jne). Used only by the oracle's semantic mutant.
std::uint32_t maskAnyNineAtC0Mutant(
    const std::uint8_t* object, const std::uint32_t* query) noexcept;

// 0x0078BB10. Scans records [container+0x14, container+0x18) with stride 0x14
// for a leading dword equal to key. Returns that record's address, or 0.
// An empty range returns 0 without reading key.
std::uint32_t findModuleRecordByKey(
    const std::uint8_t* container, std::uint32_t key) noexcept;

// Map nodes used by the three registry lookups, matching 0x006C3A10:
// +0 child taken when node.key < key (unsigned), +4 the other child,
// +0x10 key, +0x14 value address. A null child ends the walk. The end
// sentinel is the map object + 4. The registry holds one map at +0x24 and
// another at +0x40.

// 0x0078CD90. Key 0 returns 0 without touching the maps. Otherwise the +0x40
// map wins when its value address is non-null, and the result is the dword
// at that address. A miss or a null value uses the +0x24 map and returns the
// dword at value+8. Otherwise 0.
std::uint32_t registryPrimaryValue(
    const std::uint8_t* registry, std::uint32_t key) noexcept;

// 0x0078CE10. Same control flow as registryPrimaryValue, returning value+4
// from the +0x40 map and value+0x10 from the +0x24 map.
std::uint32_t registrySecondaryValue(
    const std::uint8_t* registry, std::uint32_t key) noexcept;

// 0x0078CE90. Searches only the +0x24 map, including key 0. A miss or a null
// value returns 0. A non-null value returns that address with the low byte
// replaced by 1.
std::uint32_t registryValuePresent(
    const std::uint8_t* registry, std::uint32_t key) noexcept;

// 0x0078B6E0. cdecl (begin, end, destination). Equal pointers return
// destination and write nothing. Otherwise each 0x14-byte record is assigned
// as a GameAsciiString at +0, then dwords +4/+8/+0xC and bytes +0x10/+0x11
// are copied. stringOps supplies the 0x004CA9B0 lock and allocator; it is
// not read when begin == end. Returns the destination address just past the
// last copied record.
std::uint32_t moveModuleRecords(
    const std::uint8_t* begin,
    const std::uint8_t* end,
    std::uint8_t* destination,
    const GameStringOps* stringOps) noexcept;

// 0x0078DCA0. Two record ranges on one container: [+4, +8) and [+0x14, +0x18),
// stride 0x14. A first-range hit requires (dword +0xC & mask) != 0, byte
// +0x10 != 0 and byte +0x11 == 0. A second-range hit requires (dword +8 &
// mask) != 0, byte +0xC != 0 and byte +0xD == 0. Hits slide the tail onto
// the removed slot and subtract 0x14 from that range's end. The first range
// assigns strings through moveModuleRecords and then releases the vacated
// slot (0x004CA930). The second range copies five dwords and does not release.
// A second-range hit also reads the dword at record+4 and the dword it points
// at. nameMap is the 0x00CC32D0 lookup; an empty map misses and skips the
// virtual call. With no hit, EAX is incomingEax with the low byte cleared.
// A second-range hit replaces EAX with that range's old end pointer, low byte 1.
std::uint32_t removeFirstMatchingModuleRecord(
    std::uint8_t* container,
    std::uint32_t mask,
    std::uint32_t incomingEax,
    const std::uint8_t* nameMap,
    const GameStringOps* stringOps) noexcept;

// 0x0078E0D0. this is the template. pair[0] is the key, pair[1] is a data
// address, and pair[2] is the dword stored at template+0x2F8 when the key is
// 0x977F8391 (the shipped fld/fstp reads pair+8, not the data object). Key 0
// returns the pair's address and does not read the registry, the maps, or
// the template. Otherwise looks up the registry and removes from the four
// containers at +0x194/+0x1B8/+0x1DC/+0x200 using the primary value as the
// mask. Returns globalMapAddress+4 when that map misses. The second stack
// argument is popped (ret 8) and is not read.
std::uint32_t installParsedModule(
    std::uint8_t* object,
    const std::uint32_t* pair,
    const std::uint8_t* registry,
    const std::uint8_t* globalMap,
    std::uint32_t globalMapAddress,
    const GameStringOps* stringOps) noexcept;

// 0x0078E1C0. Same as installParsedModule without the 0x977F8391 store.
// The registry lookups run primary, secondary, then present.
std::uint32_t installParsedModuleAlternate(
    std::uint8_t* object,
    const std::uint32_t* pair,
    const std::uint8_t* registry,
    const std::uint8_t* globalMap,
    std::uint32_t globalMapAddress,
    const GameStringOps* stringOps) noexcept;

// 0x0078E2A0. If byte +0x306 is 1, clears it and sets the dword-vector end
// at +0x258 back to the begin pointer at +0x254 (the copy loop is dead:
// the shipped body compares the end pointer with itself). When end < capacity
// (+0x25C), appends word and adds 4 to end, except a null end stores 4 and
// writes no dword. Then resets the map header at +0x264: count and root 0,
// header self-linked at +0x268. Returns that header address. The tree pointer
// previously at +0x270 is freed only when non-null; the oracle keeps it null.
std::uint32_t appendTemplateWordAndResetMap(
    std::uint8_t* object, std::uint32_t word) noexcept;

}  // namespace ra3::common
