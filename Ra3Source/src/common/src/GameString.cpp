#include <ra3/common/GameString.h>

#include <ra3/common/ByteString.h>

#include <algorithm>
#include <array>
#include <cstring>
#include <limits>

// Structure follows the reference-counted AsciiString of the GPL Generals
// source (GeneralsMD/Code/GameEngine/Source/Common/System/AsciiString.cpp);
// the RA3 rewrite moved the header in front of the characters, added copy
// source objects and a class-wide lock, so every detail here comes from the
// ra3_1.0.game disassembly.

namespace ra3::common {
namespace {

using CopyTo = std::uint32_t(__thiscall*)(const void* source, void* destination);
using SourceLength = int(__thiscall*)(const void* source);

std::uint32_t copyFromSource(const void* source, void* destination) noexcept
{
    const auto* vtable = *static_cast<const GameStringSourceVtable* const*>(source);
    return reinterpret_cast<CopyTo>(vtable->copyTo)(source, destination);
}

int lengthOfSource(const void* source) noexcept
{
    const auto* vtable = *static_cast<const GameStringSourceVtable* const*>(source);
    return reinterpret_cast<SourceLength>(vtable->length)(source);
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

template <typename Char>
int terminatedLength(const Char* text) noexcept
{
    if (text == nullptr) {
        return 0;
    }
    const Char* cursor = text;
    while (*cursor != 0) {
        ++cursor;
    }
    return static_cast<int>(cursor - text);
}

template <typename Char>
const Char* dataOrEmpty(const Char* data, const Char* empty) noexcept
{
    return data == nullptr ? empty : data;
}

template <typename String, typename Char>
bool setString(
    String* self, const Char* text, int length, const GameStringOps& ops) noexcept
{
    if (self->data != nullptr && text == self->data) {
        return true;
    }
    if (length == 0) {
        releaseString(self, ops);
        return true;
    }
    GameStringSource<Char> source{ops.textSource, text, length};
    return ensureUnique(&self->data, length, false, &source, nullptr, ops);
}

template <typename String>
bool removeLastCharacter(String* self, const GameStringOps& ops) noexcept
{
    if (self->data == nullptr) {
        return true;
    }
    if (!ensureUnique(
            &self->data, gameStringHeader(self->data)->length, true, nullptr, nullptr, ops)) {
        return false;
    }
    const int length = static_cast<int>(gameStringHeader(self->data)->length) - 1;
    if (length > 0) {
        self->data[length] = 0;
        gameStringHeader(self->data)->length = static_cast<std::uint16_t>(length);
    } else {
        releaseString(self, ops);
    }
    return true;
}

template <typename String, typename Char>
Char* prepareBuffer(String* self, int length, const GameStringOps& ops) noexcept
{
    if (length <= 0) {
        return nullptr;
    }
    if (!ensureUnique(&self->data, length, false, nullptr, nullptr, ops)) {
        return nullptr;
    }
    GameStringHeader* header = gameStringHeader(self->data);
    header->length = static_cast<std::uint16_t>(length);
    self->data[length] = 0;
    return self->data;
}

template <typename Char>
bool containsCharacter(const Char* text, Char character) noexcept
{
    while (*text != 0) {
        if (*text++ == character) {
            return true;
        }
    }
    return false;
}

template <typename Char>
const Char* skipSeparatorsImpl(const Char* text, const Char* separators) noexcept
{
    while (*text != 0 && containsCharacter(separators, *text)) {
        ++text;
    }
    return text;
}

template <typename Char>
const Char* skipNonSeparatorsImpl(const Char* text, const Char* separators) noexcept
{
    while (*text != 0 && !containsCharacter(separators, *text)) {
        ++text;
    }
    return text;
}

template <typename String, typename Char>
bool trimString(
    String* self, GameCharacterPredicate isSpace, const GameStringOps& ops) noexcept
{
    if (self->data == nullptr) {
        return true;
    }

    Char* leading = self->data;
    while (*leading != 0
        && isSpace(static_cast<int>(*leading)) != 0) {
        ++leading;
    }
    if (leading != self->data) {
        const int remaining = static_cast<int>(gameStringHeader(self->data)->length)
            - static_cast<int>(leading - self->data);
        if (!setString<String, Char>(self, leading, remaining, ops)) {
            return false;
        }
    }
    if (self->data == nullptr) {
        return true;
    }

    int index = static_cast<int>(gameStringHeader(self->data)->length);
    while (index > 0) {
        --index;
        if (isSpace(static_cast<int>(self->data[index])) == 0) {
            break;
        }
        if (!removeLastCharacter(self, ops)) {
            return false;
        }
        if (self->data == nullptr) {
            break;
        }
    }
    return true;
}

template <typename String, typename Char>
bool nextToken(
    String* self,
    String* token,
    const Char* separators,
    const Char* defaultSeparators,
    const GameStringOps& ops) noexcept
{
    if (self->data == nullptr || gameStringHeader(self->data)->length == 0 || self == token) {
        return false;
    }
    const Char* const activeSeparators =
        separators == nullptr ? defaultSeparators : separators;
    const Char* const start = skipSeparatorsImpl(self->data, activeSeparators);
    const Char* const end = skipNonSeparatorsImpl(start, activeSeparators);
    if (end <= start) {
        releaseString(self, ops);
        releaseString(token, ops);
        return false;
    }

    const int tokenLength = static_cast<int>(end - start);
    Char* const destination = prepareBuffer<String, Char>(token, tokenLength, ops);
    if (destination == nullptr) {
        return false;
    }
    std::memcpy(
        destination, start, static_cast<std::size_t>(tokenLength) * sizeof(Char));
    destination[tokenLength] = 0;

    const int remaining = static_cast<int>(gameStringHeader(self->data)->length)
        - static_cast<int>(end - self->data);
    return setString<String, Char>(self, end, remaining, ops);
}

template <typename String, typename Char>
bool substring(
    String* self,
    const Char* source,
    int sourceLength,
    int start,
    int count,
    const GameStringOps& ops) noexcept
{
    // The shipped code performs these additions in signed 32-bit arithmetic.
    const std::uint32_t sumBits =
        static_cast<std::uint32_t>(start) + static_cast<std::uint32_t>(count);
    const int end = static_cast<std::int32_t>(sumBits);
    if (end < 0 || start >= sourceLength) {
        releaseString(self, ops);
        return true;
    }
    if (start < 0) {
        count = end;
        start = 0;
    }
    const std::uint32_t adjustedBits =
        static_cast<std::uint32_t>(start) + static_cast<std::uint32_t>(count);
    if (static_cast<std::int32_t>(adjustedBits) >= sourceLength) {
        count = sourceLength - start;
    }
    return setString<String, Char>(self, source + start, count, ops);
}

template <typename Char>
const Char* findSubsequence(const Char* text, const Char* needle) noexcept
{
    if (*needle == 0) {
        return text;
    }
    for (; *text != 0; ++text) {
        const Char* left = text;
        const Char* right = needle;
        while (*right != 0 && *left == *right) {
            ++left;
            ++right;
        }
        if (*right == 0) {
            return text;
        }
    }
    return nullptr;
}

template <typename String, typename Char>
int replaceAll(
    String* self,
    const Char* search,
    const Char* replacement,
    const Char* empty,
    const GameStringOps& ops) noexcept
{
    const int searchLength = terminatedLength(search);
    const int replacementLength = terminatedLength(replacement);
    int replacements = 0;
    int offset = 0;
    String prefix{nullptr};
    String suffix{nullptr};

    while (true) {
        const Char* const current = dataOrEmpty(self->data, empty);
        const Char* const found = findSubsequence(current + offset, search);
        if (found == nullptr) {
            break;
        }
        const int foundOffset = static_cast<int>(found - current);
        const int currentLength = terminatedLength(current);
        if (!substring(
                &prefix, current, currentLength, 0, foundOffset, ops)
            || !substring(
                &suffix,
                current,
                currentLength,
                foundOffset + searchLength,
                currentLength - foundOffset - searchLength,
                ops)) {
            break;
        }
        assignString(self, &prefix, ops);
        if (replacementLength != 0) {
            const bool appended = self->data == nullptr
                ? setString<String, Char>(self, replacement, replacementLength, ops)
                : concatString(&self->data, replacement, replacementLength, ops);
            if (!appended) {
                break;
            }
        }
        if (suffix.data != nullptr) {
            const int suffixLength =
                static_cast<int>(gameStringHeader(suffix.data)->length);
            const bool appended = self->data == nullptr
                ? setString<String, Char>(self, suffix.data, suffixLength, ops)
                : concatString(&self->data, suffix.data, suffixLength, ops);
            if (!appended) {
                break;
            }
        }
        ++replacements;
        offset = foundOffset + replacementLength;
    }

    releaseString(&suffix, ops);
    releaseString(&prefix, ops);
    return replacements;
}

}  // namespace

GameStringHeader* gameStringHeader(const void* data) noexcept
{
    return reinterpret_cast<GameStringHeader*>(
        const_cast<std::uint8_t*>(static_cast<const std::uint8_t*>(data))) - 1;
}

std::uint16_t gameStringLength(const void* data) noexcept
{
    return data == nullptr ? 0 : gameStringHeader(data)->length;
}

bool isGameStringEmpty(const GameAsciiString* self) noexcept
{
    return gameStringLength(self->data) == 0;
}

const char* findGameCharacter(const GameAsciiString* self, char character) noexcept
{
    const int length = static_cast<int>(gameStringLength(self->data));
    for (int index = 0; index < length; ++index) {
        if (self->data[index] == character) {
            return self->data + index;
        }
    }
    return nullptr;
}

const char16_t* findGameCharacter(
    const GameUnicodeString* self, char16_t character) noexcept
{
    const int length = static_cast<int>(gameStringLength(self->data));
    for (int index = 0; index < length; ++index) {
        if (self->data[index] == character) {
            return self->data + index;
        }
    }
    return nullptr;
}

const char16_t* gameUnicodeStringData(
    const GameUnicodeString* self, const char16_t* empty) noexcept
{
    return dataOrEmpty(self->data, empty);
}

int compareWideNoCase(
    const char16_t* left,
    const char16_t* right,
    int length,
    GameCharacterMap fold) noexcept
{
    for (int index = 0; index < length; ++index) {
        const int leftUnit = fold(static_cast<int>(left[index]));
        const int rightUnit = fold(static_cast<int>(right[index]));
        if (leftUnit != rightUnit) {
            return leftUnit - rightUnit;
        }
    }
    return 0;
}

int compareWide(
    const char16_t* left,
    int leftLength,
    const char16_t* right,
    int rightLength) noexcept
{
    const int count = (std::min)(leftLength, rightLength);
    for (int index = 0; index < count; ++index) {
        if (left[index] != right[index]) {
            return static_cast<int>(left[index]) - static_cast<int>(right[index]);
        }
    }
    return leftLength - rightLength;
}

bool startsWithGameString(
    const GameAsciiString* self, const char* text, int length) noexcept
{
    if (*text == 0) {
        return true;
    }
    const int selfLength = static_cast<int>(gameStringLength(self->data));
    return selfLength >= length
        && compareBytes(
            reinterpret_cast<const std::uint8_t*>(self->data),
            reinterpret_cast<const std::uint8_t*>(text),
            static_cast<std::uint32_t>(length)) == 0;
}

bool endsWithGameString(
    const GameAsciiString* self, const char* text, int length) noexcept
{
    if (*text == 0) {
        return true;
    }
    const int selfLength = static_cast<int>(gameStringLength(self->data));
    return selfLength >= length
        && compareBytes(
            reinterpret_cast<const std::uint8_t*>(self->data + selfLength - length),
            reinterpret_cast<const std::uint8_t*>(text),
            static_cast<std::uint32_t>(length)) == 0;
}

bool endsWithNoCaseGameString(
    const GameAsciiString* self, const char* text, int length) noexcept
{
    if (*text == 0) {
        return true;
    }
    const int selfLength = static_cast<int>(gameStringLength(self->data));
    return selfLength >= length
        && compareStringsCaseInsensitiveLimited(
            self->data + selfLength - length,
            text,
            static_cast<std::uint32_t>(length)) == 0;
}

bool startsWithNoCaseGameString(
    const GameUnicodeString* self,
    const char16_t* text,
    int length,
    GameCharacterMap fold) noexcept
{
    if (*text == 0) {
        return true;
    }
    const int selfLength = static_cast<int>(gameStringLength(self->data));
    return selfLength >= length
        && compareWideNoCase(self->data, text, length, fold) == 0;
}

bool endsWithGameString(
    const GameUnicodeString* self, const char16_t* text, int length) noexcept
{
    if (*text == 0) {
        return true;
    }
    const int selfLength = static_cast<int>(gameStringLength(self->data));
    return selfLength >= length
        && compareWide(
            self->data + selfLength - length, length, text, length) == 0;
}

const char16_t* skipGameWhitespace(
    const char16_t* text, GameCharacterPredicate isSpace) noexcept
{
    while (isSpace(static_cast<int>(*text)) != 0) {
        ++text;
    }
    return text;
}

void* destroyGameStringSource(
    void* source,
    const GameStringSourceVtable* baseVtable,
    std::uint32_t flags,
    const GameStringOps& ops) noexcept
{
    *static_cast<const GameStringSourceVtable**>(source) = baseVtable;
    if ((flags & 1u) != 0) {
        ops.free(ops.context, reinterpret_cast<GameStringHeader*>(source));
    }
    return source;
}

void* copyGameStringSourceRange(
    const GameStringSource<char>* source,
    void* destination,
    int offset,
    int count) noexcept
{
    return std::memcpy(
        destination, source->text + offset, static_cast<std::size_t>(count));
}

void* copyGameStringSourceRange(
    const GameStringSource<char16_t>* source,
    void* destination,
    int offset,
    int count) noexcept
{
    return std::memcpy(
        destination,
        source->text + offset,
        static_cast<std::size_t>(count) * sizeof(char16_t));
}

const char* skipGameSeparators(const char* text, const char* separators) noexcept
{
    return skipSeparatorsImpl(text, separators);
}

const char* skipGameNonSeparators(const char* text, const char* separators) noexcept
{
    return skipNonSeparatorsImpl(text, separators);
}

const char16_t* skipGameSeparators(
    const char16_t* text, const char16_t* separators) noexcept
{
    return skipSeparatorsImpl(text, separators);
}

const char16_t* skipGameNonSeparators(
    const char16_t* text, const char16_t* separators) noexcept
{
    return skipNonSeparatorsImpl(text, separators);
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

bool removeLastGameCharacter(GameAsciiString* self, const GameStringOps& ops) noexcept
{
    return removeLastCharacter(self, ops);
}

bool removeLastGameCharacter(GameUnicodeString* self, const GameStringOps& ops) noexcept
{
    return removeLastCharacter(self, ops);
}

bool removeFirstGameCharacter(GameAsciiString* self, const GameStringOps& ops) noexcept
{
    if (self->data == nullptr) {
        return true;
    }
    if (!ensureUnique(
            &self->data, gameStringHeader(self->data)->length, true, nullptr, nullptr, ops)) {
        return false;
    }
    const int length = static_cast<int>(gameStringHeader(self->data)->length) - 1;
    if (length <= 0) {
        releaseString(self, ops);
        return true;
    }
    std::memmove(
        self->data, self->data + 1, static_cast<std::size_t>(length + 1));
    gameStringHeader(self->data)->length = static_cast<std::uint16_t>(length);
    return true;
}

char* prepareGameStringBuffer(
    GameAsciiString* self, int length, const GameStringOps& ops) noexcept
{
    return prepareBuffer<GameAsciiString, char>(self, length, ops);
}

char16_t* prepareGameStringBuffer(
    GameUnicodeString* self, int length, const GameStringOps& ops) noexcept
{
    return prepareBuffer<GameUnicodeString, char16_t>(self, length, ops);
}

bool setGameString(
    GameAsciiString* self, const char* text, int length, const GameStringOps& ops) noexcept
{
    return setString<GameAsciiString, char>(self, text, length, ops);
}

bool setGameString(
    GameUnicodeString* self, const char16_t* text, int length, const GameStringOps& ops) noexcept
{
    return setString<GameUnicodeString, char16_t>(self, text, length, ops);
}

bool setGameStringFromSource(
    GameUnicodeString* self, const void* source, const GameStringOps& ops) noexcept
{
    const int length = lengthOfSource(source);
    if (length == 0) {
        releaseString(self, ops);
        return true;
    }
    return ensureUnique(&self->data, length, false, source, nullptr, ops);
}

bool concatGameStringFromSource(
    GameUnicodeString* self, const void* source, const GameStringOps& ops) noexcept
{
    const int length = lengthOfSource(source);
    if (length == 0) {
        return true;
    }
    if (self->data == nullptr) {
        return setGameStringFromSource(self, source, ops);
    }
    const int oldLength = static_cast<int>(gameStringHeader(self->data)->length);
    return ensureUnique(&self->data, oldLength + length, true, nullptr, source, ops);
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

bool translateGameString(
    GameAsciiString* self,
    const GameUnicodeString* source,
    GameWideToAsciiConverter convert,
    const char16_t* empty,
    const GameStringOps& ops) noexcept
{
    const char16_t* const text = dataOrEmpty(source->data, empty);
    const int length = convert(text, -1, nullptr, 0);
    if (!ensureUnique(&self->data, length, false, nullptr, nullptr, ops)) {
        return false;
    }
    convert(text, -1, self->data, length);
    gameStringHeader(self->data)->length =
        static_cast<std::uint16_t>(length - 1);
    return true;
}

bool translateGameString(
    GameUnicodeString* self,
    const char* source,
    GameAsciiToWideConverter convert,
    const GameStringOps& ops) noexcept
{
    const int length = convert(source, -1, nullptr, 0);
    if (!ensureUnique(&self->data, length, false, nullptr, nullptr, ops)) {
        return false;
    }
    convert(source, -1, self->data, length);
    gameStringHeader(self->data)->length =
        static_cast<std::uint16_t>(length - 1);
    return true;
}

bool translateGameString(
    GameUnicodeString* self,
    const GameAsciiString* source,
    GameAsciiToWideConverter convert,
    const char* empty,
    const GameStringOps& ops) noexcept
{
    return translateGameString(
        self, dataOrEmpty(source->data, empty), convert, ops);
}

GameUnicodeString* constructTranslatedGameString(
    GameUnicodeString* self,
    const GameAsciiString* source,
    GameAsciiToWideConverter convert,
    const char* empty,
    const GameStringOps& ops) noexcept
{
    self->data = nullptr;
    translateGameString(self, source, convert, empty, ops);
    return self;
}

int compareNoCaseGameString(const GameAsciiString* self, const char* other) noexcept
{
    const int leftLength = static_cast<int>(gameStringLength(self->data));
    const int rightLength = terminatedLength(other);
    static constexpr char empty = '\0';
    const char* const left = dataOrEmpty(self->data, &empty);
    static constexpr char nullText = '\0';
    const char* const right = dataOrEmpty(other, &nullText);
    const int compared = compareStringsCaseInsensitiveLimited(
        left, right, static_cast<std::uint32_t>((std::min)(leftLength, rightLength)));
    return compared != 0 ? compared : leftLength - rightLength;
}

int compareNoCaseGameString(
    const GameAsciiString* self, const GameAsciiString* other) noexcept
{
    const int leftLength = static_cast<int>(gameStringLength(self->data));
    const int rightLength = static_cast<int>(gameStringLength(other->data));
    static constexpr char empty = '\0';
    const int compared = compareStringsCaseInsensitiveLimited(
        dataOrEmpty(self->data, &empty),
        dataOrEmpty(other->data, &empty),
        static_cast<std::uint32_t>((std::min)(leftLength, rightLength)));
    return compared != 0 ? compared : leftLength - rightLength;
}

int compareGameString(const GameUnicodeString* self, const char16_t* other) noexcept
{
    const int leftLength = static_cast<int>(gameStringLength(self->data));
    const int rightLength = terminatedLength(other);
    static constexpr char16_t empty = u'\0';
    return compareWide(
        dataOrEmpty(self->data, &empty),
        leftLength,
        dataOrEmpty(other, &empty),
        rightLength);
}

int compareNoCaseGameString(
    const GameUnicodeString* self,
    const GameUnicodeString* other,
    GameCharacterMap fold) noexcept
{
    const int leftLength = static_cast<int>(gameStringLength(self->data));
    const int rightLength = static_cast<int>(gameStringLength(other->data));
    const int count = (std::min)(leftLength, rightLength);
    static constexpr char16_t empty = u'\0';
    const int compared = compareWideNoCase(
        dataOrEmpty(self->data, &empty),
        dataOrEmpty(other->data, &empty),
        count,
        fold);
    return compared != 0 ? compared : leftLength - rightLength;
}

bool trimGameString(
    GameAsciiString* self, GameCharacterPredicate isSpace, const GameStringOps& ops) noexcept
{
    return trimString<GameAsciiString, char>(self, isSpace, ops);
}

bool trimGameString(
    GameUnicodeString* self, GameCharacterPredicate isSpace, const GameStringOps& ops) noexcept
{
    return trimString<GameUnicodeString, char16_t>(self, isSpace, ops);
}

bool nextGameStringToken(
    GameAsciiString* self,
    GameAsciiString* token,
    const char* separators,
    const char* defaultSeparators,
    const GameStringOps& ops) noexcept
{
    return nextToken<GameAsciiString, char>(
        self, token, separators, defaultSeparators, ops);
}

bool nextGameStringToken(
    GameUnicodeString* self,
    GameUnicodeString* token,
    const char16_t* separators,
    const char16_t* defaultSeparators,
    const GameStringOps& ops) noexcept
{
    return nextToken<GameUnicodeString, char16_t>(
        self, token, separators, defaultSeparators, ops);
}

bool isNoneGameString(const GameAsciiString* self) noexcept
{
    return compareNoCaseGameString(self, "None") == 0;
}

bool formatGameStringVa(
    GameUnicodeString* self,
    const char16_t* format,
    const void* arguments,
    GameUnicodeFormatter formatter,
    void* formatterContext,
    const GameStringOps& ops) noexcept
{
    std::array<char16_t, 0x2000> buffer{};
    formatter(
        formatterContext,
        buffer.data(),
        0x1FFF,
        format,
        arguments);
    buffer.back() = 0;
    const int length = terminatedLength(buffer.data());
    return setString<GameUnicodeString, char16_t>(
        self, buffer.data(), length, ops);
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

bool concatGameString(
    GameUnicodeString* self, const char16_t* text, const GameStringOps& ops) noexcept
{
    const int length = terminatedLength(text);
    if (length == 0) {
        return true;
    }
    if (self->data == nullptr) {
        return setString<GameUnicodeString, char16_t>(self, text, length, ops);
    }
    return concatString(&self->data, text, length, ops);
}

bool substringGameString(
    GameAsciiString* self,
    const char* source,
    int start,
    int count,
    const GameStringOps& ops) noexcept
{
    static constexpr char empty = '\0';
    const char* const text = dataOrEmpty(source, &empty);
    return substring(
        self, text, terminatedLength(source), start, count, ops);
}

bool substringGameString(
    GameAsciiString* self,
    const GameAsciiString* source,
    int start,
    int count,
    const char* empty,
    const GameStringOps& ops) noexcept
{
    const char* const text = dataOrEmpty(source->data, empty);
    return substring(
        self,
        text,
        static_cast<int>(gameStringLength(source->data)),
        start,
        count,
        ops);
}

bool substringGameString(
    GameUnicodeString* self,
    const char16_t* source,
    int start,
    int count,
    const GameStringOps& ops) noexcept
{
    static constexpr char16_t empty = u'\0';
    const char16_t* const text = dataOrEmpty(source, &empty);
    return substring(
        self, text, terminatedLength(source), start, count, ops);
}

GameUnicodeString* constructGameString(
    GameUnicodeString* self, char16_t character, const GameStringOps& ops) noexcept
{
    self->data = nullptr;
    setString<GameUnicodeString, char16_t>(self, &character, 1, ops);
    return self;
}

GameUnicodeString* constructGameString(
    GameUnicodeString* self,
    const char16_t* source,
    int length,
    const GameStringOps& ops) noexcept
{
    self->data = nullptr;
    setString<GameUnicodeString, char16_t>(self, source, length, ops);
    return self;
}

int replaceAllGameString(
    GameAsciiString* self,
    const char* search,
    const char* replacement,
    const char* empty,
    const GameStringOps& ops) noexcept
{
    return replaceAll(self, search, replacement, empty, ops);
}

int replaceAllGameString(
    GameUnicodeString* self,
    const char16_t* search,
    const char16_t* replacement,
    const char16_t* empty,
    const GameStringOps& ops) noexcept
{
    return replaceAll(self, search, replacement, empty, ops);
}

int replaceAllGameString(
    GameUnicodeString* self,
    const GameUnicodeString* search,
    const GameUnicodeString* replacement,
    const char16_t* empty,
    const GameStringOps& ops) noexcept
{
    return replaceAll(
        self,
        dataOrEmpty(search->data, empty),
        dataOrEmpty(replacement->data, empty),
        empty,
        ops);
}

}  // namespace ra3::common
