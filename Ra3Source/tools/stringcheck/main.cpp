// Drives the GameString port with scenarios from tools/analysis/ports/
// verify_game_string.py, one per input line:
//
//   <width 1|2> <op> <blockCount> {<refCount> <length> <capacity> <hex>}...
//   <slotCount> {<blockIndex|-1>}... <op arguments>
//
// and prints the normalized outcome the oracle also derives from the
// original functions: return value, the ordered lock/allocate/free trace,
// where every slot points and the header and bytes of every live block.

#include <ra3/common/GameString.h>

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

struct Block {
    std::uint8_t* memory;
    std::size_t areaBytes;
    std::string id;
};

struct Arena {
    std::vector<Block> blocks;
    std::vector<std::string> frees;
    // L lock, U unlock, A<size> allocate, F<block> free, in call order.
    std::vector<std::string> trace;
    int allocations = 0;
};

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
    std::memset(memory, 0xCD, size);
    arena->trace.push_back("A" + std::to_string(size));
    arena->blocks.push_back({memory, size - 8u, "n" + std::to_string(arena->allocations++)});
    return reinterpret_cast<GameStringHeader*>(memory);
}

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

void freeBlock(void* context, GameStringHeader* header) noexcept
{
    auto* arena = static_cast<Arena*>(context);
    arena->frees.push_back(blockId(*arena, header, 0));
    arena->trace.push_back("F" + arena->frees.back());
}

template <typename Char>
std::uint32_t __fastcall copyTo(const GameStringSource<Char>* self, void*, void* destination)
{
    std::memcpy(destination, self->text, static_cast<std::size_t>(self->length) * sizeof(Char));
    return static_cast<std::uint32_t>(self->length);
}

template <typename Char>
const GameStringSourceVtable sourceVtable{
    nullptr, nullptr, nullptr, reinterpret_cast<const void*>(&copyTo<Char>)};

int lowerC(int c)
{
    return c >= 'A' && c <= 'Z' ? c + 32 : c;
}

int upperC(int c)
{
    return c >= 'a' && c <= 'z' ? c - 32 : c;
}

// "-" is an absent text, "." a present empty one.
std::vector<std::uint8_t> fromHex(const std::string& text)
{
    std::vector<std::uint8_t> bytes;
    if (text == "-" || text == ".") {
        return bytes;
    }
    for (std::size_t i = 0; i + 1 < text.size(); i += 2) {
        bytes.push_back(static_cast<std::uint8_t>(std::stoul(text.substr(i, 2), nullptr, 16)));
    }
    return bytes;
}

std::string toHex(const std::uint8_t* bytes, std::size_t size)
{
    std::ostringstream out;
    out << std::hex << std::setfill('0');
    for (std::size_t i = 0; i < size; ++i) {
        out << std::setw(2) << static_cast<unsigned>(bytes[i]);
    }
    return out.str();
}

template <typename Char>
struct Text {
    std::vector<std::uint8_t> bytes;
    int length = 0;
    bool present = false;

    const Char* pointer() const
    {
        return present ? reinterpret_cast<const Char*>(bytes.data()) : nullptr;
    }
};

template <typename Char>
Text<Char> parseText(const std::string& hex)
{
    Text<Char> text;
    text.present = hex != "-";
    text.bytes = fromHex(hex);
    text.length = static_cast<int>(text.bytes.size() / sizeof(Char));
    // Keeps a present empty text non-null.
    text.bytes.resize(text.bytes.size() + sizeof(Char));
    return text;
}

template <typename Char>
Text<Char> readText(std::istream& in)
{
    std::string hex;
    in >> hex;
    return parseText<Char>(hex);
}

template <typename String, typename Char>
std::string run(std::istream& in, Arena& arena, std::vector<String>& slots, const std::string& op)
{
    GameStringOps ops{};
    ops.context = &arena;
    ops.lock = &lockArena;
    ops.unlock = &unlockArena;
    ops.allocate = &allocateBlock;
    ops.free = &freeBlock;
    ops.textSource = &sourceVtable<Char>;

    std::size_t slot = 0;
    in >> slot;
    String* self = &slots.at(slot);
    bool result = true;

    if (op == "copy" || op == "assign") {
        std::size_t source = 0;
        in >> source;
        if (op == "copy") {
            return ra3::common::copyGameString(self, &slots.at(source), ops) == self ? "self" : "other";
        }
        ra3::common::assignGameString(self, &slots.at(source), ops);
    } else if (op == "release") {
        ra3::common::releaseGameString(self, ops);
    } else if (op == "ensure") {
        int needed = 0;
        int preserve = 0;
        in >> needed >> preserve;
        const Text<Char> copyText = readText<Char>(in);
        const Text<Char> catText = readText<Char>(in);
        GameStringSource<Char> copySource{&sourceVtable<Char>, copyText.pointer(), copyText.length};
        GameStringSource<Char> catSource{&sourceVtable<Char>, catText.pointer(), catText.length};
        result = ra3::common::ensureUniqueGameString(self, needed, preserve != 0,
            copyText.present ? &copySource : nullptr, catText.present ? &catSource : nullptr, ops);
    } else if (op == "lower" || op == "upper") {
        result = ra3::common::mapGameString(self, op == "lower" ? &lowerC : &upperC, ops);
    } else if (op == "set" || op == "concat") {
        std::string hex;
        int length = 0;
        in >> hex >> length;
        const Text<Char> parsed = parseText<Char>(hex == "self" ? "-" : hex);
        const Char* text = hex == "self" ? self->data : parsed.pointer();
        if constexpr (sizeof(Char) == 2) {
            result = op == "set" ? ra3::common::setGameString(self, text, length, ops)
                                 : ra3::common::concatGameString(self, text, length, ops);
        } else {
            if (op == "set") {
                return "unsupported";
            }
            result = ra3::common::concatGameString(self, text, length, ops);
        }
    } else if (op == "removelast") {
        if constexpr (sizeof(Char) == 2) {
            result = ra3::common::removeLastGameCharacter(self, ops);
        } else {
            return "unsupported";
        }
    } else {
        return "unsupported";
    }
    return result ? "1" : "0";
}

template <typename String, typename Char>
void scenario(std::istringstream& in, const std::string& op)
{
    Arena arena;
    std::size_t blockCount = 0;
    in >> blockCount;
    for (std::size_t i = 0; i < blockCount; ++i) {
        int refCount = 0;
        unsigned length = 0;
        unsigned capacity = 0;
        std::string hex;
        in >> refCount >> length >> capacity >> hex;
        const std::vector<std::uint8_t> area = fromHex(hex);
        auto* memory = static_cast<std::uint8_t*>(std::malloc(8 + area.size()));
        auto* header = reinterpret_cast<GameStringHeader*>(memory);
        header->refCount = refCount;
        header->length = static_cast<std::uint16_t>(length);
        header->capacity = static_cast<std::uint16_t>(capacity);
        std::memcpy(memory + 8, area.data(), area.size());
        arena.blocks.push_back({memory, area.size(), "b" + std::to_string(i)});
    }

    std::size_t slotCount = 0;
    in >> slotCount;
    std::vector<String> slots(slotCount);
    for (String& slot : slots) {
        int index = -1;
        in >> index;
        slot.data = index < 0 ? nullptr
                              : reinterpret_cast<Char*>(arena.blocks.at(static_cast<std::size_t>(index)).memory + 8);
    }

    const std::string result = run<String, Char>(in, arena, slots, op);

    std::cout << "ret=" << result << " trace=";
    if (arena.trace.empty()) {
        std::cout << '-';
    }
    for (std::size_t i = 0; i < arena.trace.size(); ++i) {
        std::cout << (i ? "," : "") << arena.trace[i];
    }
    std::cout << " slots=";
    for (std::size_t i = 0; i < slots.size(); ++i) {
        std::cout << (i ? "," : "") << blockId(arena, slots[i].data, 8);
    }
    std::cout << " blocks=";
    bool first = true;
    for (const Block& block : arena.blocks) {
        bool freed = false;
        for (const std::string& id : arena.frees) {
            freed = freed || id == block.id;
        }
        if (freed) {
            continue;
        }
        const auto* header = reinterpret_cast<const GameStringHeader*>(block.memory);
        std::cout << (first ? "" : ";") << block.id << ':' << header->refCount << ':' << header->length
                  << ':' << header->capacity << ':' << toHex(block.memory + 8, block.areaBytes);
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
    std::string line;
    while (std::getline(std::cin, line)) {
        if (line.empty()) {
            continue;
        }
        std::istringstream in(line);
        int width = 0;
        std::string op;
        in >> width >> op;
        if (width == 1) {
            scenario<GameAsciiString, char>(in, op);
        } else {
            scenario<GameUnicodeString, char16_t>(in, op);
        }
        std::cout.flush();
    }
    return 0;
}
