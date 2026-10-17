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
using GameCharacterPredicate = int(__cdecl*)(int character);
using GameWideToAsciiConverter =
    int(__cdecl*)(const char16_t* source, int sourceLength, char* destination, int capacity);
using GameAsciiToWideConverter =
    int(__cdecl*)(const char* source, int sourceLength, char16_t* destination, int capacity);
using GameUnicodeFormatter = int(__cdecl*)(
    void* context,
    char16_t* destination,
    int capacity,
    const char16_t* format,
    const void* arguments);

GameStringHeader* gameStringHeader(const void* data) noexcept;
std::uint16_t gameStringLength(const void* data) noexcept;

// 0x004CA030. A non-null zero-length block is empty too.
bool isGameStringEmpty(const GameAsciiString* self) noexcept;

// 0x004CA050 / 0x004CA260. Searches exactly the recorded length.
const char* findGameCharacter(const GameAsciiString* self, char character) noexcept;
const char16_t* findGameCharacter(
    const GameUnicodeString* self, char16_t character) noexcept;

// 0x004CA250. `empty` is the engine's immutable empty-string address.
const char16_t* gameUnicodeStringData(
    const GameUnicodeString* self, const char16_t* empty) noexcept;

// 0x004C9F80 / 0x004CA510.
int compareWideNoCase(
    const char16_t* left,
    const char16_t* right,
    int length,
    GameCharacterMap fold) noexcept;
int compareWide(
    const char16_t* left,
    int leftLength,
    const char16_t* right,
    int rightLength) noexcept;

// 0x004CA0D0 / 0x004CA170 / 0x004CA1D0 and the recovered Unicode
// instantiations 0x004CA330 / 0x004CA380. Names describe observed effects;
// the protected bodies do not retain C++ symbols.
bool startsWithGameString(
    const GameAsciiString* self, const char* text, int length) noexcept;
bool endsWithGameString(
    const GameAsciiString* self, const char* text, int length) noexcept;
bool endsWithNoCaseGameString(
    const GameAsciiString* self, const char* text, int length) noexcept;
bool startsWithNoCaseGameString(
    const GameUnicodeString* self,
    const char16_t* text,
    int length,
    GameCharacterMap fold) noexcept;
bool endsWithGameString(
    const GameUnicodeString* self, const char16_t* text, int length) noexcept;

// 0x004CA4E0.
const char16_t* skipGameWhitespace(
    const char16_t* text, GameCharacterPredicate isSpace) noexcept;

// 0x004CA010 / 0x004CB140 deleting destructors for abstract copy sources.
void* destroyGameStringSource(
    void* source,
    const GameStringSourceVtable* baseVtable,
    std::uint32_t flags,
    const GameStringOps& ops) noexcept;

// 0x004CB020 / 0x004CB0E0 text-source vtable slot 2.
void* copyGameStringSourceRange(
    const GameStringSource<char>* source,
    void* destination,
    int offset,
    int count) noexcept;
void* copyGameStringSourceRange(
    const GameStringSource<char16_t>* source,
    void* destination,
    int offset,
    int count) noexcept;

// 0x004CB060 / 0x004CB0A0 / 0x004CB160 / 0x004CB1B0.
const char* skipGameSeparators(const char* text, const char* separators) noexcept;
const char* skipGameNonSeparators(const char* text, const char* separators) noexcept;
const char16_t* skipGameSeparators(
    const char16_t* text, const char16_t* separators) noexcept;
const char16_t* skipGameNonSeparators(
    const char16_t* text, const char16_t* separators) noexcept;

// 0x004CA8F0 / 0x004CAD90. Shares other's block under the class lock.
GameAsciiString* copyGameString(
    GameAsciiString* self, const GameAsciiString* other, const GameStringOps& ops) noexcept;
GameUnicodeString* copyGameString(
    GameUnicodeString* self, const GameUnicodeString* other, const GameStringOps& ops) noexcept;

// 0x004CA9B0 / 0x004CAE10.
void assignGameString(
    GameAsciiString* self, const GameAsciiString* other, const GameStringOps& ops) noexcept;
void assignGameString(
    GameUnicodeString* self, const GameUnicodeString* other, const GameStringOps& ops) noexcept;

// 0x004CA930 / 0x004CAB60.
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

// 0x004CAAD0 / 0x004CAF90.
bool removeLastGameCharacter(GameAsciiString* self, const GameStringOps& ops) noexcept;
bool removeLastGameCharacter(GameUnicodeString* self, const GameStringOps& ops) noexcept;

// 0x004CAB10.
bool removeFirstGameCharacter(GameAsciiString* self, const GameStringOps& ops) noexcept;

// 0x004CADD0. A non-positive length returns null without changing `self`.
char16_t* prepareGameStringBuffer(
    GameUnicodeString* self, int length, const GameStringOps& ops) noexcept;

// Framework operation used by 0x004CA970 as well as the tokenizers.
char* prepareGameStringBuffer(
    GameAsciiString* self, int length, const GameStringOps& ops) noexcept;

// 0x004CB690 / 0x004CB9B0. The ASCII address has an unresolved protected
// source-construction slot, so it is framework support rather than a claimed
// reconstructed entry point.
bool setGameString(
    GameAsciiString* self, const char* text, int length, const GameStringOps& ops) noexcept;
bool setGameString(
    GameUnicodeString* self, const char16_t* text, int length, const GameStringOps& ops) noexcept;

// 0x004CAE90 / 0x004CAF00 abstract-source overloads.
bool setGameStringFromSource(
    GameUnicodeString* self, const void* source, const GameStringOps& ops) noexcept;
bool concatGameStringFromSource(
    GameUnicodeString* self, const void* source, const GameStringOps& ops) noexcept;

// 0x004CB6E0 / 0x004CBA00. The ASCII form returns false for an empty target,
// which the original hands to the inner-patched setter 0x004CB690.
bool concatGameString(
    GameAsciiString* self, const char* text, int length, const GameStringOps& ops) noexcept;
bool concatGameString(
    GameUnicodeString* self, const char16_t* text, int length, const GameStringOps& ops) noexcept;

// 0x004CB200 / 0x004CB2A0 plus 0x004CBCE0 and 0x004CBD00 wrappers.
bool translateGameString(
    GameAsciiString* self,
    const GameUnicodeString* source,
    GameWideToAsciiConverter convert,
    const char16_t* empty,
    const GameStringOps& ops) noexcept;
bool translateGameString(
    GameUnicodeString* self,
    const char* source,
    GameAsciiToWideConverter convert,
    const GameStringOps& ops) noexcept;
bool translateGameString(
    GameUnicodeString* self,
    const GameAsciiString* source,
    GameAsciiToWideConverter convert,
    const char* empty,
    const GameStringOps& ops) noexcept;
GameUnicodeString* constructTranslatedGameString(
    GameUnicodeString* self,
    const GameAsciiString* source,
    GameAsciiToWideConverter convert,
    const char* empty,
    const GameStringOps& ops) noexcept;

// 0x004CB460 / 0x004CB4C0 / 0x004CB520 / 0x004CB630.
int compareNoCaseGameString(const GameAsciiString* self, const char* other) noexcept;
int compareNoCaseGameString(
    const GameAsciiString* self, const GameAsciiString* other) noexcept;
int compareGameString(const GameUnicodeString* self, const char16_t* other) noexcept;
int compareNoCaseGameString(
    const GameUnicodeString* self,
    const GameUnicodeString* other,
    GameCharacterMap fold) noexcept;

// The control flow of these methods also appears in the GPL reference files
// work/references/generals/GeneralsMD/Code/GameEngine/Source/Common/System/AsciiString.cpp
// work/references/generals/GeneralsMD/Code/GameEngine/Source/Common/System/UnicodeString.cpp
// This implementation is independently derived
// from the RA3 1.0 bytes and preserves RA3's header lengths and allocation API.
bool trimGameString(
    GameAsciiString* self, GameCharacterPredicate isSpace, const GameStringOps& ops) noexcept;
bool trimGameString(
    GameUnicodeString* self, GameCharacterPredicate isSpace, const GameStringOps& ops) noexcept;
bool nextGameStringToken(
    GameAsciiString* self,
    GameAsciiString* token,
    const char* separators,
    const char* defaultSeparators,
    const GameStringOps& ops) noexcept;
bool nextGameStringToken(
    GameUnicodeString* self,
    GameUnicodeString* token,
    const char16_t* separators,
    const char16_t* defaultSeparators,
    const GameStringOps& ops) noexcept;
bool isNoneGameString(const GameAsciiString* self) noexcept;

// 0x004CBB40. `arguments` is the original x86 va_list pointer at the ABI
// boundary; the formatter callback isolates CRT/locale behavior.
bool formatGameStringVa(
    GameUnicodeString* self,
    const char16_t* format,
    const void* arguments,
    GameUnicodeFormatter formatter,
    void* formatterContext,
    const GameStringOps& ops) noexcept;

// 0x004CBE80.
bool concatGameString(
    GameUnicodeString* self, const char16_t* text, const GameStringOps& ops) noexcept;

// 0x004CC010 / 0x004CC0B0 / 0x004CC460.
bool substringGameString(
    GameAsciiString* self,
    const char* source,
    int start,
    int count,
    const GameStringOps& ops) noexcept;
bool substringGameString(
    GameAsciiString* self,
    const GameAsciiString* source,
    int start,
    int count,
    const char* empty,
    const GameStringOps& ops) noexcept;
bool substringGameString(
    GameUnicodeString* self,
    const char16_t* source,
    int start,
    int count,
    const GameStringOps& ops) noexcept;

// 0x004CC3B0 / 0x004CC3F0.
GameUnicodeString* constructGameString(
    GameUnicodeString* self, char16_t character, const GameStringOps& ops) noexcept;
GameUnicodeString* constructGameString(
    GameUnicodeString* self,
    const char16_t* source,
    int length,
    const GameStringOps& ops) noexcept;

// 0x004CC160 / 0x004CC5B0 / 0x004CC820.
int replaceAllGameString(
    GameAsciiString* self,
    const char* search,
    const char* replacement,
    const char* empty,
    const GameStringOps& ops) noexcept;
int replaceAllGameString(
    GameUnicodeString* self,
    const char16_t* search,
    const char16_t* replacement,
    const char16_t* empty,
    const GameStringOps& ops) noexcept;
int replaceAllGameString(
    GameUnicodeString* self,
    const GameUnicodeString* search,
    const GameUnicodeString* replacement,
    const char16_t* empty,
    const GameStringOps& ops) noexcept;

}  // namespace ra3::common
