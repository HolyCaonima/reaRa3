#include <ra3/common/GameString.h>

#include <cstring>

// Structure follows the reference-counted AsciiString of the GPL Generals
// source (GeneralsMD/Code/GameEngine/Source/Common/System/AsciiString.cpp);
// the RA3 rewrite moved the header in front of the characters, added copy
// source objects and a class-wide lock, so every detail here comes from the
// ra3_1.0.game disassembly.

namespace ra3::common {
namespace {

using CopyTo = std::uint32_t(__thiscall*)(const void* source, void* destination);

std::uint32_t copyFromSource(const void* source, void* destination) noexcept
{
    const auto* vtable = *static_cast<const GameStringSourceVtable* const*>(source);
    return reinterpret_cast<CopyTo>(vtable->copyTo)(source, destination);
}

template <typename Char>
Char* charactersOf(GameStringHeader* block) noexcept
{
    return reinterpret_cast<Char*>(block + 1);
}

// The ASCII instantiation requests needed + 9 bytes, the Unicode one
// 2 * needed + 10: header plus characters plus terminator, in 32-bit math.
template <typename Char>
std::uint32_t requestBytes(int needed) noexcept
{
    return static_cast<std::uint32_t>(needed) * sizeof(Char) + 8u + sizeof(Char);
}

template <typename Char>
std::uint16_t capacityOf(std::uint32_t size) noexcept
{
    return static_cast<std::uint16_t>((size - 8u) / sizeof(Char));
}

template <typename Char>
void releaseLocked(Char** data, const GameStringOps& ops) noexcept
{
    if (*data != nullptr) {
        GameStringHeader* block = gameStringHeader(*data);
        if (--block->refCount == 0) {
            ops.free(ops.context, block);
        }
        *data = nullptr;
    }
}

template <typename Char>
void addReference(Char* data) noexcept
{
    if (data != nullptr) {
        ++gameStringHeader(data)->refCount;
    }
}

template <typename String>
String* copyString(String* self, const String* other, const GameStringOps& ops) noexcept
{
    ops.lock(ops.context);
    self->data = other->data;
    addReference(self->data);
    ops.unlock(ops.context);
    return self;
}

template <typename String>
void assignString(String* self, const String* other, const GameStringOps& ops) noexcept
{
    ops.lock(ops.context);
    if (other != self) {
        ops.lock(ops.context);
        releaseLocked(&self->data, ops);
        ops.unlock(ops.context);
        self->data = other->data;
        addReference(self->data);
    }
    ops.unlock(ops.context);
}

template <typename String>
void releaseString(String* self, const GameStringOps& ops) noexcept
{
    ops.lock(ops.context);
    releaseLocked(&self->data, ops);
    ops.unlock(ops.context);
}

template <typename Char>
void appendSources(
    GameStringHeader* block,
    Char* characters,
    const void* copySource,
    const void* catSource) noexcept
{
    if (copySource != nullptr) {
        block->length = static_cast<std::uint16_t>(copyFromSource(copySource, characters));
    }
    if (catSource != nullptr) {
        block->length = static_cast<std::uint16_t>(
            block->length + copyFromSource(catSource, characters + block->length));
    }
    characters[block->length] = 0;
}

template <typename Char>
bool ensureUnique(
    Char** data,
    int needed,
    bool preserve,
    const void* copySource,
    const void* catSource,
    const GameStringOps& ops) noexcept
{
    Char* const old = *data;
    if (old != nullptr) {
        GameStringHeader* block = gameStringHeader(old);
        const int capacity = block->capacity;
        if (capacity > needed) {
            if (block->refCount == 1) {
                appendSources(block, old, copySource, catSource);
                return true;
            }
        } else if (catSource != nullptr) {
            // Concatenation grows by half to amortise repeated appends.
            const int grown = capacity + (capacity >> 1) - 1;
            if (grown > needed) {
                needed = grown;
            }
        }
    }

    const std::uint32_t request = requestBytes<Char>(needed);
    if (request > 0x7FFFu) {
        return false;
    }
    const std::uint32_t size = (request + 3u) >> 2 << 2;
    GameStringHeader* block = ops.allocate(ops.context, size);
    block->refCount = 1;
    block->capacity = capacityOf<Char>(size);
    Char* const characters = charactersOf<Char>(block);
    if (old != nullptr && preserve) {
        const std::uint16_t length = gameStringHeader(old)->length;
        std::memcpy(characters, old, length * sizeof(Char));
        block->length = length;
    } else {
        block->length = 0;
    }
    appendSources(block, characters, copySource, catSource);

    ops.lock(ops.context);
    releaseLocked(data, ops);
    ops.unlock(ops.context);
    *data = characters;
    return true;
}

template <typename Char>
bool mapString(Char** data, GameCharacterMap map, const GameStringOps& ops) noexcept
{
    if (*data == nullptr) {
        return true;
    }
    if (!ensureUnique(data, gameStringHeader(*data)->length, true, nullptr, nullptr, ops)) {
        return false;
    }
    Char* cursor = *data;
    Char* const end = cursor + gameStringHeader(cursor)->length;
    for (; cursor != end; ++cursor) {
        // ASCII passes the sign-extended char, Unicode the zero-extended unit.
        *cursor = static_cast<Char>(map(static_cast<int>(*cursor)));
    }
    return true;
}

template <typename Char>
bool concatString(
    Char** data, const Char* text, int length, const GameStringOps& ops) noexcept
{
    GameStringSource<Char> source{ops.textSource, text, length};
    return ensureUnique(
        data, gameStringHeader(*data)->length + length, true, nullptr, &source, ops);
}

}  // namespace

GameStringHeader* gameStringHeader(const void* data) noexcept
{
    return reinterpret_cast<GameStringHeader*>(
        const_cast<std::uint8_t*>(static_cast<const std::uint8_t*>(data))) - 1;
}

GameAsciiString* copyGameString(
    GameAsciiString* self, const GameAsciiString* other, const GameStringOps& ops) noexcept
{
    return copyString(self, other, ops);
}

GameUnicodeString* copyGameString(
    GameUnicodeString* self, const GameUnicodeString* other, const GameStringOps& ops) noexcept
{
    return copyString(self, other, ops);
}

void assignGameString(
    GameAsciiString* self, const GameAsciiString* other, const GameStringOps& ops) noexcept
{
    assignString(self, other, ops);
}

void assignGameString(
    GameUnicodeString* self, const GameUnicodeString* other, const GameStringOps& ops) noexcept
{
    assignString(self, other, ops);
}

void releaseGameString(GameAsciiString* self, const GameStringOps& ops) noexcept
{
    releaseString(self, ops);
}

void releaseGameString(GameUnicodeString* self, const GameStringOps& ops) noexcept
{
    releaseString(self, ops);
}

bool ensureUniqueGameString(
    GameAsciiString* self,
    int needed,
    bool preserve,
    const void* copySource,
    const void* catSource,
    const GameStringOps& ops) noexcept
{
    return ensureUnique(&self->data, needed, preserve, copySource, catSource, ops);
}

bool ensureUniqueGameString(
    GameUnicodeString* self,
    int needed,
    bool preserve,
    const void* copySource,
    const void* catSource,
    const GameStringOps& ops) noexcept
{
    return ensureUnique(&self->data, needed, preserve, copySource, catSource, ops);
}

bool mapGameString(GameAsciiString* self, GameCharacterMap map, const GameStringOps& ops) noexcept
{
    return mapString(&self->data, map, ops);
}

bool mapGameString(GameUnicodeString* self, GameCharacterMap map, const GameStringOps& ops) noexcept
{
    return mapString(&self->data, map, ops);
}

bool removeLastGameCharacter(GameUnicodeString* self, const GameStringOps& ops) noexcept
{
    if (self->data == nullptr) {
        return true;
    }
    if (!ensureUnique(
            &self->data, gameStringHeader(self->data)->length, true, nullptr, nullptr, ops)) {
        return false;
    }
    const int length = gameStringHeader(self->data)->length - 1;
    if (length > 0) {
        self->data[length] = 0;
        gameStringHeader(self->data)->length = static_cast<std::uint16_t>(length);
    } else {
        releaseString(self, ops);
    }
    return true;
}

bool setGameString(
    GameUnicodeString* self, const char16_t* text, int length, const GameStringOps& ops) noexcept
{
    if (self->data != nullptr && text == self->data) {
        return true;
    }
    if (length == 0) {
        releaseString(self, ops);
        return true;
    }
    GameStringSource<char16_t> source{ops.textSource, text, length};
    return ensureUnique(&self->data, length, false, &source, nullptr, ops);
}

bool concatGameString(
    GameAsciiString* self, const char* text, int length, const GameStringOps& ops) noexcept
{
    if (length == 0) {
        return true;
    }
    if (self->data == nullptr) {
        return false;
    }
    return concatString(&self->data, text, length, ops);
}

bool concatGameString(
    GameUnicodeString* self, const char16_t* text, int length, const GameStringOps& ops) noexcept
{
    if (length == 0) {
        return true;
    }
    if (self->data == nullptr) {
        return setGameString(self, text, length, ops);
    }
    return concatString(&self->data, text, length, ops);
}

}  // namespace ra3::common
