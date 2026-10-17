// Batch driver for tools/analysis/ports/verify_game_string2.py.
//
// P scenarios exercise read-only helpers:
//   P <operation> <operation-specific arguments>
// S scenarios exercise an object graph:
//   S <width> <operation> <blockCount>
//     {<refCount> <length> <capacity> <areaHex>}...
//     <slotCount> {<blockIndex|-1>}... <operation-specific arguments>

#include <ra3/common/GameString.h>

#include <algorithm>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

namespace {

using ra3::common::GameAsciiString;
using ra3::common::GameStringHeader;
using ra3::common::GameStringOps;
using ra3::common::GameStringSource;
using ra3::common::GameStringSourceVtable;
using ra3::common::GameUnicodeString;

bool g_deliberateMutation = false;

std::vector<std::uint8_t> fromHex(const std::string& text)
{
    std::vector<std::uint8_t> bytes;
    if (text == "-" || text == ".") {
        return bytes;
    }
    for (std::size_t index = 0; index + 1 < text.size(); index += 2) {
        bytes.push_back(static_cast<std::uint8_t>(
            std::stoul(text.substr(index, 2), nullptr, 16)));
    }
    return bytes;
}

std::string toHex(const std::uint8_t* bytes, std::size_t size)
{
    std::ostringstream out;
    out << std::hex << std::setfill('0');
    for (std::size_t index = 0; index < size; ++index) {
        out << std::setw(2) << static_cast<unsigned>(bytes[index]);
    }
    return out.str();
}

template <typename Char>
struct Text {
    std::vector<std::uint8_t> bytes;
    bool present = false;

    const Char* pointer() const
    {
        return present ? reinterpret_cast<const Char*>(bytes.data()) : nullptr;
    }

    int length() const
    {
        return static_cast<int>(bytes.size() / sizeof(Char)) - 1;
    }
};

template <typename Char>
Text<Char> readText(std::istream& input)
{
    std::string encoded;
    input >> encoded;
    Text<Char> text;
    text.present = encoded != "-";
    text.bytes = fromHex(encoded);
    text.bytes.resize(text.bytes.size() + sizeof(Char));
    return text;
}

template <typename String, typename Char>
struct LocalString {
    std::vector<std::uint8_t> bytes;
    String string{nullptr};

    LocalString(const Text<Char>& text, int recordedLength)
    {
        if (!text.present) {
            return;
        }
        const std::size_t area = text.bytes.size();
        bytes.resize(sizeof(GameStringHeader) + area);
        auto* header = reinterpret_cast<GameStringHeader*>(bytes.data());
        header->refCount = 1;
        header->length = static_cast<std::uint16_t>(recordedLength);
        header->capacity = static_cast<std::uint16_t>(area / sizeof(Char));
        std::memcpy(bytes.data() + sizeof(GameStringHeader), text.bytes.data(), area);
        string.data = reinterpret_cast<Char*>(bytes.data() + sizeof(GameStringHeader));
    }
};

int lowerCharacter(int character)
{
    if (character >= 'A' && character <= 'Z') {
        return character + ('a' - 'A');
    }
    return character;
}

int spaceCharacter(int character)
{
    return character == ' ' || (character >= '\t' && character <= '\r');
}

template <typename Char>
std::string pointerOffset(const Char* pointer, const Char* base)
{
    if (pointer == nullptr) {
        return "-1";
    }
    if (base == nullptr) {
        return "empty";
    }
    return std::to_string(pointer - base);
}

struct FreeProbe {
    int calls = 0;
    void* pointer = nullptr;
};

void probeFree(void* context, GameStringHeader* pointer) noexcept
{
    auto* probe = static_cast<FreeProbe*>(context);
    ++probe->calls;
    probe->pointer = pointer;
}

void pureScenario(std::istringstream& input, const std::string& operation)
{
    if (operation == "widefold") {
        const Text<char16_t> left = readText<char16_t>(input);
        const Text<char16_t> right = readText<char16_t>(input);
        int count = 0;
        input >> count;
        const int result = ra3::common::compareWideNoCase(
            left.pointer(), right.pointer(), count, &lowerCharacter);
        std::cout << result + (g_deliberateMutation ? 1 : 0);
    } else if (operation == "comparewide") {
        const Text<char16_t> left = readText<char16_t>(input);
        int leftLength = 0;
        input >> leftLength;
        const Text<char16_t> right = readText<char16_t>(input);
        int rightLength = 0;
        input >> rightLength;
        std::cout << ra3::common::compareWide(
            left.pointer(), leftLength, right.pointer(), rightLength);
    } else if (operation == "empty1" || operation == "find1"
               || operation == "starts1" || operation == "ends1"
               || operation == "endsnocase1" || operation == "cmpnocasec1"
               || operation == "cmpnocaseobj1" || operation == "isnone1") {
        const Text<char> left = readText<char>(input);
        int leftLength = 0;
        input >> leftLength;
        LocalString<GameAsciiString, char> leftString(left, leftLength);
        if (operation == "empty1") {
            std::cout << (ra3::common::isGameStringEmpty(&leftString.string) ? 1 : 0);
        } else if (operation == "find1") {
            unsigned character = 0;
            input >> character;
            std::cout << pointerOffset(
                ra3::common::findGameCharacter(
                    &leftString.string, static_cast<char>(character)),
                leftString.string.data);
        } else if (operation == "isnone1") {
            std::cout << (ra3::common::isNoneGameString(&leftString.string) ? 1 : 0);
        } else {
            const Text<char> right = readText<char>(input);
            int rightLength = 0;
            input >> rightLength;
            if (operation == "starts1") {
                std::cout << (ra3::common::startsWithGameString(
                    &leftString.string, right.pointer(), rightLength) ? 1 : 0);
            } else if (operation == "ends1") {
                std::cout << (ra3::common::endsWithGameString(
                    &leftString.string, right.pointer(), rightLength) ? 1 : 0);
            } else if (operation == "endsnocase1") {
                std::cout << (ra3::common::endsWithNoCaseGameString(
                    &leftString.string, right.pointer(), rightLength) ? 1 : 0);
            } else if (operation == "cmpnocasec1") {
                std::cout << ra3::common::compareNoCaseGameString(
                    &leftString.string, right.pointer());
            } else {
                LocalString<GameAsciiString, char> rightString(right, rightLength);
                std::cout << ra3::common::compareNoCaseGameString(
                    &leftString.string, &rightString.string);
            }
        }
    } else if (operation == "data2" || operation == "find2"
               || operation == "startsnocase2" || operation == "ends2"
               || operation == "cmpc2" || operation == "cmpnocaseobj2") {
        const Text<char16_t> left = readText<char16_t>(input);
        int leftLength = 0;
        input >> leftLength;
        LocalString<GameUnicodeString, char16_t> leftString(left, leftLength);
        static constexpr char16_t empty[] = u"";
        if (operation == "data2") {
            const char16_t* result =
                ra3::common::gameUnicodeStringData(&leftString.string, empty);
            std::cout << (result == empty ? "empty" : pointerOffset(result, leftString.string.data));
        } else if (operation == "find2") {
            unsigned character = 0;
            input >> character;
            std::cout << pointerOffset(
                ra3::common::findGameCharacter(
                    &leftString.string, static_cast<char16_t>(character)),
                leftString.string.data);
        } else {
            const Text<char16_t> right = readText<char16_t>(input);
            int rightLength = 0;
            input >> rightLength;
            if (operation == "startsnocase2") {
                std::cout << (ra3::common::startsWithNoCaseGameString(
                    &leftString.string, right.pointer(), rightLength, &lowerCharacter) ? 1 : 0);
            } else if (operation == "ends2") {
                std::cout << (ra3::common::endsWithGameString(
                    &leftString.string, right.pointer(), rightLength) ? 1 : 0);
            } else if (operation == "cmpc2") {
                std::cout << ra3::common::compareGameString(
                    &leftString.string, right.pointer());
            } else {
                LocalString<GameUnicodeString, char16_t> rightString(right, rightLength);
                std::cout << ra3::common::compareNoCaseGameString(
                    &leftString.string, &rightString.string, &lowerCharacter);
            }
        }
    } else if (operation == "skipspace2" || operation == "skipsep1"
               || operation == "skipnonsep1" || operation == "skipsep2"
               || operation == "skipnonsep2") {
        if (operation == "skipspace2" || operation == "skipsep2"
            || operation == "skipnonsep2") {
            const Text<char16_t> text = readText<char16_t>(input);
            const char16_t* result = nullptr;
            if (operation == "skipspace2") {
                result = ra3::common::skipGameWhitespace(text.pointer(), &spaceCharacter);
            } else {
                const Text<char16_t> separators = readText<char16_t>(input);
                result = operation == "skipsep2"
                    ? ra3::common::skipGameSeparators(text.pointer(), separators.pointer())
                    : ra3::common::skipGameNonSeparators(text.pointer(), separators.pointer());
            }
            std::cout << pointerOffset(result, text.pointer());
        } else {
            const Text<char> text = readText<char>(input);
            const Text<char> separators = readText<char>(input);
            const char* result = operation == "skipsep1"
                ? ra3::common::skipGameSeparators(text.pointer(), separators.pointer())
                : ra3::common::skipGameNonSeparators(text.pointer(), separators.pointer());
            std::cout << pointerOffset(result, text.pointer());
        }
    } else if (operation == "copyrange1" || operation == "copyrange2") {
        const unsigned width = operation == "copyrange1" ? 1u : 2u;
        const Text<char16_t> wide = width == 2 ? readText<char16_t>(input) : Text<char16_t>{};
        const Text<char> narrow = width == 1 ? readText<char>(input) : Text<char>{};
        int offset = 0;
        int count = 0;
        int size = 0;
        input >> offset >> count >> size;
        std::vector<std::uint8_t> output(static_cast<std::size_t>(size), 0xA5);
        void* returned = nullptr;
        static const GameStringSourceVtable vtable{};
        if (width == 1) {
            const GameStringSource<char> source{&vtable, narrow.pointer(), narrow.length()};
            returned = ra3::common::copyGameStringSourceRange(
                &source, output.data(), offset, count);
        } else {
            const GameStringSource<char16_t> source{&vtable, wide.pointer(), wide.length()};
            returned = ra3::common::copyGameStringSourceRange(
                &source, output.data(), offset, count);
        }
        std::cout << (returned == output.data() ? 1 : 0) << ':'
                  << toHex(output.data(), output.size());
    } else if (operation == "sourcedtor1" || operation == "sourcedtor2") {
        unsigned flags = 0;
        input >> flags;
        const GameStringSourceVtable base{};
        GameStringSourceVtable derived{};
        struct Source {
            const GameStringSourceVtable* vtable;
            std::uint32_t marker;
        } source{&derived, 0x12345678};
        FreeProbe probe;
        GameStringOps ops{};
        ops.context = &probe;
        ops.free = &probeFree;
        void* returned =
            ra3::common::destroyGameStringSource(&source, &base, flags, ops);
        std::cout << (returned == &source ? 1 : 0) << ':'
                  << (source.vtable == &base ? 1 : 0) << ':'
                  << probe.calls << ':' << (probe.pointer == &source ? 1 : 0);
    } else {
        std::cout << "unsupported";
    }
    std::cout << '\n';
}

struct Block {
    std::uint8_t* memory;
    std::size_t areaBytes;
    std::string id;
};

struct Arena {
    std::vector<Block> blocks;
    std::vector<std::string> frees;
    std::vector<std::string> trace;
    int allocations = 0;
};

std::string blockId(const Arena& arena, const void* pointer, std::size_t bias)
{
    if (pointer == nullptr) {
        return "0";
    }
    for (const Block& block : arena.blocks) {
        if (static_cast<const std::uint8_t*>(pointer) == block.memory + bias) {
            return block.id;
        }
    }
    return "?";
}

void lockArena(void* context) noexcept
{
    static_cast<Arena*>(context)->trace.emplace_back("L");
}

void unlockArena(void* context) noexcept
{
    static_cast<Arena*>(context)->trace.emplace_back("U");
}

GameStringHeader* allocateBlock(void* context, std::uint32_t size) noexcept
{
    auto* arena = static_cast<Arena*>(context);
    auto* memory = static_cast<std::uint8_t*>(std::malloc(size));
    if (memory == nullptr) {
        return nullptr;
    }
    std::memset(memory, 0xCD, size);
    arena->trace.push_back("A" + std::to_string(size));
    arena->blocks.push_back(
        {memory, size - sizeof(GameStringHeader), "n" + std::to_string(arena->allocations++)});
    return reinterpret_cast<GameStringHeader*>(memory);
}

void freeBlock(void* context, GameStringHeader* header) noexcept
{
    auto* arena = static_cast<Arena*>(context);
    const std::string id = blockId(*arena, header, 0);
    arena->frees.push_back(id);
    arena->trace.push_back("F" + id);
}

template <typename Char>
int __fastcall sourceLength(const GameStringSource<Char>* self, void*)
{
    return self->length;
}

template <typename Char>
void* __fastcall sourceCopyRange(
    const GameStringSource<Char>* self, void*, void* destination, int offset, int count)
{
    return ra3::common::copyGameStringSourceRange(
        self, destination, offset, count);
}

template <typename Char>
std::uint32_t __fastcall sourceCopyTo(
    const GameStringSource<Char>* self, void*, void* destination)
{
    std::memcpy(
        destination,
        self->text,
        static_cast<std::size_t>(self->length) * sizeof(Char));
    return static_cast<std::uint32_t>(self->length);
}

template <typename Char>
const GameStringSourceVtable sourceVtable{
    nullptr,
    reinterpret_cast<const void*>(&sourceLength<Char>),
    reinterpret_cast<const void*>(&sourceCopyRange<Char>),
    reinterpret_cast<const void*>(&sourceCopyTo<Char>)};

int __cdecl wideToAscii(
    const char16_t* source, int, char* destination, int capacity)
{
    int length = 0;
    while (source[length] != 0) {
        ++length;
    }
    if (destination != nullptr && capacity != 0) {
        for (int index = 0; index < length; ++index) {
            destination[index] = static_cast<char>(source[index]);
        }
        destination[length] = 0;
    }
    return length + 1;
}

int __cdecl asciiToWide(
    const char* source, int, char16_t* destination, int capacity)
{
    int length = 0;
    while (source[length] != 0) {
        ++length;
    }
    if (destination != nullptr && capacity != 0) {
        for (int index = 0; index < length; ++index) {
            destination[index] =
                static_cast<unsigned char>(source[index]);
        }
        destination[length] = 0;
    }
    return length + 1;
}

int __cdecl copyFormatted(
    void* context, char16_t* destination, int capacity, const char16_t*, const void*)
{
    const auto* text = static_cast<const Text<char16_t>*>(context);
    const int copied = (std::min)(text->length(), capacity);
    std::memcpy(
        destination, text->pointer(), static_cast<std::size_t>(copied) * sizeof(char16_t));
    destination[copied] = 0;
    return copied;
}

template <typename String, typename Char>
std::string runState(
    std::istringstream& input,
    Arena& arena,
    std::vector<String>& slots,
    const std::string& operation)
{
    GameStringOps ops{};
    ops.context = &arena;
    ops.lock = &lockArena;
    ops.unlock = &unlockArena;
    ops.allocate = &allocateBlock;
    ops.free = &freeBlock;
    ops.textSource = &sourceVtable<Char>;

    std::size_t targetIndex = 0;
    input >> targetIndex;
    String* const self = &slots.at(targetIndex);
    std::string result = "1";

    if (operation == "release") {
        ra3::common::releaseGameString(self, ops);
    } else if (operation == "removelast") {
        result = ra3::common::removeLastGameCharacter(self, ops) ? "1" : "0";
    } else if (operation == "removefirst") {
        if constexpr (sizeof(Char) == 1) {
            result = ra3::common::removeFirstGameCharacter(
                reinterpret_cast<GameAsciiString*>(self), ops) ? "1" : "0";
        } else {
            result = "unsupported";
        }
    } else if (operation == "prepare") {
        int length = 0;
        input >> length;
        Char* buffer = nullptr;
        if constexpr (sizeof(Char) == 1) {
            buffer = reinterpret_cast<Char*>(ra3::common::prepareGameStringBuffer(
                reinterpret_cast<GameAsciiString*>(self), length, ops));
        } else {
            buffer = reinterpret_cast<Char*>(ra3::common::prepareGameStringBuffer(
                reinterpret_cast<GameUnicodeString*>(self), length, ops));
        }
        result = buffer == nullptr ? "0" : blockId(arena, buffer, sizeof(GameStringHeader));
    } else if (operation == "assign") {
        std::size_t sourceIndex = 0;
        input >> sourceIndex;
        ra3::common::assignGameString(self, &slots.at(sourceIndex), ops);
    } else if (operation == "setsource" || operation == "concatsource") {
        const Text<Char> text = readText<Char>(input);
        const GameStringSource<Char> source{
            &sourceVtable<Char>, text.pointer(), text.length()};
        if constexpr (sizeof(Char) == 2) {
            const bool okay = operation == "setsource"
                ? ra3::common::setGameStringFromSource(
                    reinterpret_cast<GameUnicodeString*>(self), &source, ops)
                : ra3::common::concatGameStringFromSource(
                    reinterpret_cast<GameUnicodeString*>(self), &source, ops);
            result = okay ? "1" : "0";
        } else {
            result = "unsupported";
        }
    } else if (operation == "trim") {
        result = ra3::common::trimGameString(self, &spaceCharacter, ops) ? "1" : "0";
    } else if (operation == "token") {
        std::size_t tokenIndex = 0;
        input >> tokenIndex;
        const Text<Char> separators = readText<Char>(input);
        static constexpr char defaultAscii[] = " \n\r\t";
        static constexpr char16_t defaultWide[] = u" \t\n\r";
        bool found = false;
        if constexpr (sizeof(Char) == 1) {
            found = ra3::common::nextGameStringToken(
                reinterpret_cast<GameAsciiString*>(self),
                reinterpret_cast<GameAsciiString*>(&slots.at(tokenIndex)),
                reinterpret_cast<const char*>(separators.pointer()),
                defaultAscii,
                ops);
        } else {
            found = ra3::common::nextGameStringToken(
                reinterpret_cast<GameUnicodeString*>(self),
                reinterpret_cast<GameUnicodeString*>(&slots.at(tokenIndex)),
                reinterpret_cast<const char16_t*>(separators.pointer()),
                defaultWide,
                ops);
        }
        result = found ? "1" : "0";
    } else if (operation == "translate1") {
        const Text<char16_t> source = readText<char16_t>(input);
        LocalString<GameUnicodeString, char16_t> sourceString(
            source, source.present ? source.length() : 0);
        static constexpr char16_t empty[] = u"";
        if constexpr (sizeof(Char) == 1) {
            result = ra3::common::translateGameString(
                reinterpret_cast<GameAsciiString*>(self),
                &sourceString.string,
                &wideToAscii,
                empty,
                ops) ? "1" : "0";
        } else {
            result = "unsupported";
        }
    } else if (operation == "translate2") {
        const Text<char> source = readText<char>(input);
        if constexpr (sizeof(Char) == 2) {
            result = ra3::common::translateGameString(
                reinterpret_cast<GameUnicodeString*>(self),
                source.pointer(),
                &asciiToWide,
                ops) ? "1" : "0";
        } else {
            result = "unsupported";
        }
    } else if (operation == "translateobj2" || operation == "constructtranslated2") {
        const Text<char> sourceText = readText<char>(input);
        LocalString<GameAsciiString, char> sourceString(
            sourceText, sourceText.present ? sourceText.length() : 0);
        static constexpr char empty[] = "";
        if constexpr (sizeof(Char) == 2) {
            auto* target = reinterpret_cast<GameUnicodeString*>(self);
            if (operation == "constructtranslated2") {
                ra3::common::constructTranslatedGameString(
                    target, &sourceString.string, &asciiToWide, empty, ops);
                result = "self";
            } else {
                result = ra3::common::translateGameString(
                    target, &sourceString.string, &asciiToWide, empty, ops) ? "1" : "0";
            }
        } else {
            result = "unsupported";
        }
    } else if (operation == "format2") {
        const Text<char16_t> formatted = readText<char16_t>(input);
        static constexpr char16_t format[] = u"%s";
        if constexpr (sizeof(Char) == 2) {
            result = ra3::common::formatGameStringVa(
                reinterpret_cast<GameUnicodeString*>(self),
                format,
                nullptr,
                &copyFormatted,
                const_cast<Text<char16_t>*>(&formatted),
                ops) ? "1" : "0";
        } else {
            result = "unsupported";
        }
    } else if (operation == "concatc2") {
        const Text<char16_t> text = readText<char16_t>(input);
        if constexpr (sizeof(Char) == 2) {
            result = ra3::common::concatGameString(
                reinterpret_cast<GameUnicodeString*>(self), text.pointer(), ops) ? "1" : "0";
        } else {
            result = "unsupported";
        }
    } else if (operation == "substringc") {
        const Text<Char> source = readText<Char>(input);
        int start = 0;
        int count = 0;
        input >> start >> count;
        if constexpr (sizeof(Char) == 1) {
            result = ra3::common::substringGameString(
                reinterpret_cast<GameAsciiString*>(self),
                reinterpret_cast<const char*>(source.pointer()),
                start,
                count,
                ops) ? "1" : "0";
        } else {
            result = ra3::common::substringGameString(
                reinterpret_cast<GameUnicodeString*>(self),
                reinterpret_cast<const char16_t*>(source.pointer()),
                start,
                count,
                ops) ? "1" : "0";
        }
    } else if (operation == "substringobj") {
        std::size_t sourceIndex = 0;
        int start = 0;
        int count = 0;
        input >> sourceIndex >> start >> count;
        static constexpr char empty[] = "";
        if constexpr (sizeof(Char) == 1) {
            result = ra3::common::substringGameString(
                reinterpret_cast<GameAsciiString*>(self),
                reinterpret_cast<const GameAsciiString*>(&slots.at(sourceIndex)),
                start,
                count,
                empty,
                ops) ? "1" : "0";
        } else {
            result = "unsupported";
        }
    } else if (operation == "constructchar2") {
        unsigned character = 0;
        input >> character;
        if constexpr (sizeof(Char) == 2) {
            ra3::common::constructGameString(
                reinterpret_cast<GameUnicodeString*>(self),
                static_cast<char16_t>(character),
                ops);
            result = "self";
        } else {
            result = "unsupported";
        }
    } else if (operation == "constructrange2") {
        const Text<char16_t> source = readText<char16_t>(input);
        int length = 0;
        input >> length;
        if constexpr (sizeof(Char) == 2) {
            ra3::common::constructGameString(
                reinterpret_cast<GameUnicodeString*>(self), source.pointer(), length, ops);
            result = "self";
        } else {
            result = "unsupported";
        }
    } else if (operation == "replace") {
        const Text<Char> search = readText<Char>(input);
        const Text<Char> replacement = readText<Char>(input);
        if constexpr (sizeof(Char) == 1) {
            static constexpr char empty[] = "";
            result = std::to_string(ra3::common::replaceAllGameString(
                reinterpret_cast<GameAsciiString*>(self),
                reinterpret_cast<const char*>(search.pointer()),
                reinterpret_cast<const char*>(replacement.pointer()),
                empty,
                ops));
        } else {
            static constexpr char16_t empty[] = u"";
            result = std::to_string(ra3::common::replaceAllGameString(
                reinterpret_cast<GameUnicodeString*>(self),
                reinterpret_cast<const char16_t*>(search.pointer()),
                reinterpret_cast<const char16_t*>(replacement.pointer()),
                empty,
                ops));
        }
    } else if (operation == "replaceobj2") {
        std::size_t searchIndex = 0;
        std::size_t replacementIndex = 0;
        input >> searchIndex >> replacementIndex;
        if constexpr (sizeof(Char) == 2) {
            static constexpr char16_t empty[] = u"";
            result = std::to_string(ra3::common::replaceAllGameString(
                reinterpret_cast<GameUnicodeString*>(self),
                reinterpret_cast<const GameUnicodeString*>(&slots.at(searchIndex)),
                reinterpret_cast<const GameUnicodeString*>(&slots.at(replacementIndex)),
                empty,
                ops));
        } else {
            result = "unsupported";
        }
    } else {
        result = "unsupported";
    }
    return result;
}

template <typename String, typename Char>
void stateScenario(std::istringstream& input, const std::string& operation)
{
    Arena arena;
    std::size_t blockCount = 0;
    input >> blockCount;
    for (std::size_t index = 0; index < blockCount; ++index) {
        int refCount = 0;
        unsigned length = 0;
        unsigned capacity = 0;
        std::string encoded;
        input >> refCount >> length >> capacity >> encoded;
        const std::vector<std::uint8_t> area = fromHex(encoded);
        auto* memory = static_cast<std::uint8_t*>(
            std::malloc(sizeof(GameStringHeader) + area.size()));
        auto* header = reinterpret_cast<GameStringHeader*>(memory);
        header->refCount = refCount;
        header->length = static_cast<std::uint16_t>(length);
        header->capacity = static_cast<std::uint16_t>(capacity);
        std::memcpy(memory + sizeof(GameStringHeader), area.data(), area.size());
        arena.blocks.push_back(
            {memory, area.size(), "b" + std::to_string(index)});
    }

    std::size_t slotCount = 0;
    input >> slotCount;
    std::vector<String> slots(slotCount);
    for (String& slot : slots) {
        int blockIndex = -1;
        input >> blockIndex;
        slot.data = blockIndex < 0 ? nullptr
            : reinterpret_cast<Char*>(
                arena.blocks.at(static_cast<std::size_t>(blockIndex)).memory
                + sizeof(GameStringHeader));
    }

    const std::string result = runState<String, Char>(
        input, arena, slots, operation);
    std::cout << "ret=" << result << " trace=";
    if (arena.trace.empty()) {
        std::cout << '-';
    }
    for (std::size_t index = 0; index < arena.trace.size(); ++index) {
        std::cout << (index == 0 ? "" : ",") << arena.trace[index];
    }
    std::cout << " slots=";
    for (std::size_t index = 0; index < slots.size(); ++index) {
        std::cout << (index == 0 ? "" : ",")
                  << blockId(arena, slots[index].data, sizeof(GameStringHeader));
    }
    std::cout << " blocks=";
    bool first = true;
    for (const Block& block : arena.blocks) {
        if ((std::find)(arena.frees.begin(), arena.frees.end(), block.id)
            != arena.frees.end()) {
            continue;
        }
        const auto* header =
            reinterpret_cast<const GameStringHeader*>(block.memory);
        std::cout << (first ? "" : ";") << block.id << ':'
                  << header->refCount << ':' << header->length << ':'
                  << header->capacity << ':'
                  << toHex(
                      block.memory + sizeof(GameStringHeader),
                      block.areaBytes);
        first = false;
    }
    std::cout << '\n';

    for (const Block& block : arena.blocks) {
        std::free(block.memory);
    }
}

}  // namespace

int main()
{
    g_deliberateMutation = std::getenv("RA3_STRINGBASE2_MUTATE") != nullptr;
    std::string line;
    while (std::getline(std::cin, line)) {
        if (line.empty()) {
            continue;
        }
        std::istringstream input(line);
        char scenarioType = 0;
        input >> scenarioType;
        if (scenarioType == 'P') {
            std::string operation;
            input >> operation;
            pureScenario(input, operation);
        } else {
            int width = 0;
            std::string operation;
            input >> width >> operation;
            if (width == 1) {
                stateScenario<GameAsciiString, char>(input, operation);
            } else {
                stateScenario<GameUnicodeString, char16_t>(input, operation);
            }
        }
        std::cout.flush();
    }
    return 0;
}
