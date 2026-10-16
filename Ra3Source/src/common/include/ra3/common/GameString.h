#pragma once

#include <cstddef>
#include <cstdint>

namespace ra3::common {

// ra3_1.0.game Libraries\Source\string\StringBase.cpp (a CharType template with
// ASCII and Unicode instantiations). A string object is one pointer to its
// characters; this header sits immediately before them.
struct GameStringHeader {
    std::int32_t refCount;
    std::uint16_t length;
    // Characters the block can hold, including the terminator.
    std::uint16_t capacity;
};

struct GameAsciiString {
    char* data;
};

struct GameUnicodeString {
    char16_t* data;
};

// Copy source consumed by ensureUniqueBuffer. The {text, length} sources built
// by concat/set use vtables 0x00BDAB6C (ASCII) and 0x00BDAB7C (Unicode).
struct GameStringSourceVtable {
    const void* destroy;
    const void* length;
    const void* copyRange;
    // thiscall(destination) -> characters written.
    const void* copyTo;
};

template <typename Char>
struct GameStringSource {
    const GameStringSourceVtable* vtable;
    const Char* text;
    std::int32_t length;
};

// Per-class lock (0x004CA440 ASCII / 0x004CA490 Unicode), the global allocator
// 0x004165A0 and free 0x004166B0, bound by the ABI boundary.
struct GameStringOps {
    void* context = nullptr;
    void (*lock)(void* context) = nullptr;
    void (*unlock)(void* context) = nullptr;
    GameStringHeader* (*allocate)(void* context, std::uint32_t size) = nullptr;
    void (*free)(void* context, GameStringHeader* block) = nullptr;
    const GameStringSourceVtable* textSource = nullptr;
};

using GameCharacterMap = int(__cdecl*)(int character);

GameStringHeader* gameStringHeader(const void* data) noexcept;

// 0x004CA8F0 / 0x004CAD90. Shares other's block under the class lock.
GameAsciiString* copyGameString(
    GameAsciiString* self, const GameAsciiString* other, const GameStringOps& ops) noexcept;
GameUnicodeString* copyGameString(
    GameUnicodeString* self, const GameUnicodeString* other, const GameStringOps& ops) noexcept;

// 0x004CA9B0 (the Unicode twin 0x004CAE10 is SecuROM inner-patched).
void assignGameString(
    GameAsciiString* self, const GameAsciiString* other, const GameStringOps& ops) noexcept;
void assignGameString(
    GameUnicodeString* self, const GameUnicodeString* other, const GameStringOps& ops) noexcept;

// 0x004CAB60 (the ASCII twin 0x004CA930 is SecuROM inner-patched).
void releaseGameString(GameAsciiString* self, const GameStringOps& ops) noexcept;
void releaseGameString(GameUnicodeString* self, const GameStringOps& ops) noexcept;

// 0x004CA710 / 0x004CABA0 ensureUniqueBufferOfSize. Returns false, having
// changed nothing, when the request exceeds 0x7FFF bytes: the original then
// reports through inner-patched debug code before allocating.
bool ensureUniqueGameString(
    GameAsciiString* self,
    int needed,
    bool preserve,
    const void* copySource,
    const void* catSource,
    const GameStringOps& ops) noexcept;
bool ensureUniqueGameString(
    GameUnicodeString* self,
    int needed,
    bool preserve,
    const void* copySource,
    const void* catSource,
    const GameStringOps& ops) noexcept;

// 0x004CAA30 / 0x004CAA80 / 0x004CAF40. `map` is the game's CRT
// tolower/toupper/towlower so locale behaviour is the original's.
bool mapGameString(GameAsciiString* self, GameCharacterMap map, const GameStringOps& ops) noexcept;
bool mapGameString(GameUnicodeString* self, GameCharacterMap map, const GameStringOps& ops) noexcept;

// 0x004CAF90. The ASCII twin 0x004CAAD0 releases through 0x004CA930.
bool removeLastGameCharacter(GameUnicodeString* self, const GameStringOps& ops) noexcept;

// 0x004CB9B0.
bool setGameString(
    GameUnicodeString* self, const char16_t* text, int length, const GameStringOps& ops) noexcept;

// 0x004CB6E0 / 0x004CBA00. The ASCII form returns false for an empty target,
// which the original hands to the inner-patched setter 0x004CB690.
bool concatGameString(
    GameAsciiString* self, const char* text, int length, const GameStringOps& ops) noexcept;
bool concatGameString(
    GameUnicodeString* self, const char16_t* text, int length, const GameStringOps& ops) noexcept;

}  // namespace ra3::common
