#pragma once

#include <cstddef>
#include <cstdint>

namespace ra3::text {

// ra3_1.0.game 0x00539080: EASTL-like sorted lookup table used by GameText.
struct GameAsciiString {
    const char* data;
};

struct GameUnicodeString {
    const char16_t* data;
};

struct GameTextInfo {
    GameAsciiString label;
    GameUnicodeString text;
    GameAsciiString extra;
};

struct GameTextLookupEntry {
    const GameAsciiString* label;
    const GameTextInfo* info;
};

struct GameStringTable {
    std::uint32_t count;
    GameTextInfo* values;
    GameTextLookupEntry* lookup;
};

// ra3_1.0.game 0x0053B4A0 initializes the known fields through +0x68.
struct GameTextManager {
    std::uint8_t base[0x38];
    std::uint8_t initialized;
    std::uint8_t reserved39[0x0F];
    const GameStringTable* primary;
    const GameStringTable* map;
    const GameStringTable* overrideTable;
    std::uint8_t prefixResults[0x10];
    std::uint32_t fetchMode;
    std::uint8_t reloadPending;
    std::uint8_t reserved69[3];
};

static_assert(sizeof(GameAsciiString) == 0x04);
static_assert(sizeof(GameUnicodeString) == 0x04);
static_assert(sizeof(GameTextInfo) == 0x0C);
static_assert(offsetof(GameTextInfo, text) == 0x04);
static_assert(offsetof(GameTextInfo, extra) == 0x08);
static_assert(sizeof(GameTextLookupEntry) == 0x08);
static_assert(offsetof(GameTextLookupEntry, info) == 0x04);
static_assert(sizeof(GameStringTable) == 0x0C);
static_assert(offsetof(GameStringTable, lookup) == 0x08);
static_assert(sizeof(GameTextManager) == 0x6C);
static_assert(offsetof(GameTextManager, initialized) == 0x38);
static_assert(offsetof(GameTextManager, primary) == 0x48);
static_assert(offsetof(GameTextManager, map) == 0x4C);
static_assert(offsetof(GameTextManager, overrideTable) == 0x50);
static_assert(offsetof(GameTextManager, fetchMode) == 0x64);
static_assert(offsetof(GameTextManager, reloadPending) == 0x68);

// Returns the GameUnicodeString member inside a game-owned GameTextInfo.
const GameUnicodeString* findGameText(
    const GameStringTable* table, const char* label) noexcept;

// Lookup precedence from ra3_1.0.game 0x0053B500:
// overrideTable, primary, then map.
const GameUnicodeString* findGameText(
    const GameTextManager* manager, const char* label) noexcept;

}  // namespace ra3::text
