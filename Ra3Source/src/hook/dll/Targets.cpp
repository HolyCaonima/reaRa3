#include "Targets.h"

#include <ra3/asset/NameHash.h>
#include <ra3/io/GameBigLayout.h>
#include <ra3/io/PathCompare.h>
#include <ra3/io/RefPack.h>

#include <cstring>
#include <iomanip>
#include <limits>
#include <sstream>
#include <string_view>

namespace ra3hook {
namespace {

constexpr auto relaxed = std::memory_order_relaxed;

TargetState g_refPack;
TargetState g_nameHash;
TargetState g_pathCompare;
TargetState g_bigFindHash;
TargetState g_bigFindEntry;

// ra3_1.0.game 0x009F72E0: one-shot RefPack decoder, returns the decoded size.
using DecodeFunction = int(__stdcall*)(std::uint8_t*, const std::uint8_t*);

int __stdcall refPackDecode(std::uint8_t* destination, const std::uint8_t* source)
{
    TargetState& state = g_refPack;
    state.calls.fetch_add(1, relaxed);
    if (state.mode == Mode::shadow) {
        const int size = reinterpret_cast<DecodeFunction>(state.original)(destination, source);
        bool equal = size >= 0;
        try {
            if (source == nullptr) {
                equal = size == 0;
            } else if (equal) {
                auto result = ra3::io::decompressRefPackPrefix(
                    source, std::numeric_limits<std::size_t>::max(), static_cast<std::size_t>(size));
                equal = result.ok()
                    && (result.value().empty()
                        || std::memcmp(destination, result.value().data(), result.value().size()) == 0);
            }
        } catch (...) {
            equal = false;
        }
        if (equal) {
            state.matches.fetch_add(1, relaxed);
        } else {
            reportMismatch("refpack", state, "size=" + std::to_string(size));
        }
        return size;
    }

    if (source == nullptr) {
        state.matches.fetch_add(1, relaxed);
        return 0;
    }
    try {
        const auto header =
            ra3::io::readRefPackHeader(source, std::numeric_limits<std::size_t>::max());
        if (header) {
            auto result = ra3::io::decompressRefPackPrefix(
                source, std::numeric_limits<std::size_t>::max(), header->decompressedSize);
            if (result.ok()) {
                const auto& decoded = result.value();
                std::memcpy(destination, decoded.data(), decoded.size());
                state.matches.fetch_add(1, relaxed);
                return static_cast<int>(decoded.size());
            }
        }
    } catch (...) {
    }
    state.mismatches.fetch_add(1, relaxed);
    return 0;
}

// ra3_1.0.game 0x00417DB0: case-folding name hash (bytes, length, seed, finalize).
using HashFunction = std::uint32_t(__cdecl*)(const char*, std::uint32_t, std::uint32_t, char);

std::string describeName(const char* data, std::uint32_t length)
{
    std::ostringstream out;
    out << std::hex << std::uppercase << std::setfill('0') << '"';
    const std::uint32_t shown = data == nullptr ? 0 : (length < 64 ? length : 64);
    for (std::uint32_t i = 0; i < shown; ++i) {
        const auto c = static_cast<unsigned char>(data[i]);
        if (c >= 0x20 && c < 0x7F && c != '"' && c != '\\') {
            out << static_cast<char>(c);
        } else {
            out << "\\x" << std::setw(2) << static_cast<unsigned>(c);
        }
    }
    out << '"' << std::dec << " length=" << length;
    return out.str();
}

std::uint32_t __cdecl nameHash(const char* data, std::uint32_t length, std::uint32_t seed, char finalize)
{
    TargetState& state = g_nameHash;
    state.calls.fetch_add(1, relaxed);
    const std::string_view bytes =
        data == nullptr ? std::string_view() : std::string_view(data, length);
    const std::uint32_t ours = ra3::asset::hashName(bytes, seed, finalize != 0);
    if (state.mode == Mode::replace) {
        state.matches.fetch_add(1, relaxed);
        return ours;
    }
    const std::uint32_t game =
        reinterpret_cast<HashFunction>(state.original)(data, length, seed, finalize);
    if (game == ours) {
        state.matches.fetch_add(1, relaxed);
    } else {
        std::ostringstream detail;
        detail << describeName(data, length) << " seed=" << seed << " finalize=" << int(finalize)
               << std::hex << std::uppercase << " game=0x" << game << " ours=0x" << ours;
        reportMismatch("namehash", state, detail.str());
    }
    return game;
}

// ra3_1.0.game 0x009F3650: archive path compare, returns -1/0/1.
using CompareFunction = int(__cdecl*)(const char*, const char*);

int __cdecl pathCompare(const char* a, const char* b)
{
    TargetState& state = g_pathCompare;
    state.calls.fetch_add(1, relaxed);
    const int ours = ra3::io::comparePathNames(a, b);
    if (state.mode == Mode::replace) {
        state.matches.fetch_add(1, relaxed);
        return ours;
    }
    const int game = reinterpret_cast<CompareFunction>(state.original)(a, b);
    if (game == ours) {
        state.matches.fetch_add(1, relaxed);
    } else {
        reportMismatch("pathcompare", state,
                       describeName(a, static_cast<std::uint32_t>(std::strlen(a))) + " vs "
                           + describeName(b, static_cast<std::uint32_t>(std::strlen(b)))
                           + " game=" + std::to_string(game) + " ours=" + std::to_string(ours));
    }
    return game;
}

// ra3_1.0.game 0x009F6B90: read-only lookup in one archive's sorted hash table.
using BigFindHashFunction = const std::uint8_t*(__thiscall*)(
    const ra3::io::GameBigIndex*, const char*, int, int);

const std::uint8_t* __fastcall bigFindHash(
    const ra3::io::GameBigIndex* index,
    void*,
    const char* path,
    int offsetWidth,
    int sizeWidth)
{
    TargetState& state = g_bigFindHash;
    state.calls.fetch_add(1, relaxed);
    if (state.mode == Mode::replace) {
        const std::uint8_t* ours =
            ra3::io::findGameBigHashedEntry(index, path, offsetWidth, sizeWidth);
        state.matches.fetch_add(1, relaxed);
        return ours;
    }
    const std::uint8_t* game = reinterpret_cast<BigFindHashFunction>(state.original)(
        index, path, offsetWidth, sizeWidth);
    const std::uint8_t* ours =
        ra3::io::findGameBigHashedEntry(index, path, offsetWidth, sizeWidth);
    if (game == ours) {
        state.matches.fetch_add(1, relaxed);
    } else {
        std::ostringstream detail;
        detail << describeName(path, static_cast<std::uint32_t>(std::strlen(path)))
               << " game=" << static_cast<const void*>(game)
               << " ours=" << static_cast<const void*>(ours);
        reportMismatch("bigfindhash", state, detail.str());
    }
    return game;
}

// ra3_1.0.game 0x009F6D00: read-only lookup by path or ordinal in one archive.
using BigFindEntryFunction = const char*(__thiscall*)(
    const ra3::io::GameBigIndex*,
    const char*,
    int,
    std::uint64_t*,
    std::uint64_t*,
    const std::uint8_t**,
    int*);

const char* __fastcall bigFindEntry(
    const ra3::io::GameBigIndex* index,
    void*,
    const char* path,
    int entryOrdinal,
    std::uint64_t* offsetOut,
    std::uint64_t* sizeOut,
    const std::uint8_t** recordCursor,
    int* cursorOrdinal)
{
    TargetState& state = g_bigFindEntry;
    state.calls.fetch_add(1, relaxed);
    if (state.mode == Mode::replace) {
        const char* result = ra3::io::findGameBigEntry(
            index, path, entryOrdinal, offsetOut, sizeOut, recordCursor, cursorOrdinal);
        state.matches.fetch_add(1, relaxed);
        return result;
    }

    const std::uint64_t initialOffset = offsetOut != nullptr ? *offsetOut : 0;
    const std::uint64_t initialSize = sizeOut != nullptr ? *sizeOut : 0;
    const std::uint8_t* initialRecord = recordCursor != nullptr ? *recordCursor : nullptr;
    const int initialCursor = cursorOrdinal != nullptr ? *cursorOrdinal : -1;
    const char* game = reinterpret_cast<BigFindEntryFunction>(state.original)(
        index, path, entryOrdinal, offsetOut, sizeOut, recordCursor, cursorOrdinal);

    std::uint64_t ourOffset = initialOffset;
    std::uint64_t ourSize = initialSize;
    const std::uint8_t* ourRecord = initialRecord;
    int ourCursor = initialCursor;
    const char* ours = ra3::io::findGameBigEntry(
        index,
        path,
        entryOrdinal,
        offsetOut != nullptr ? &ourOffset : nullptr,
        sizeOut != nullptr ? &ourSize : nullptr,
        recordCursor != nullptr ? &ourRecord : nullptr,
        cursorOrdinal != nullptr ? &ourCursor : nullptr);
    const bool equal = game == ours
        && (offsetOut == nullptr || *offsetOut == ourOffset)
        && (sizeOut == nullptr || *sizeOut == ourSize)
        && (recordCursor == nullptr || *recordCursor == ourRecord)
        && (cursorOrdinal == nullptr || *cursorOrdinal == ourCursor);
    if (equal) {
        state.matches.fetch_add(1, relaxed);
    } else {
        std::ostringstream detail;
        detail << "path="
               << (path == nullptr
                       ? std::string("<ordinal>")
                       : describeName(path, static_cast<std::uint32_t>(std::strlen(path))))
               << " ordinal=" << entryOrdinal
               << " game=" << static_cast<const void*>(game)
               << " ours=" << static_cast<const void*>(ours);
        reportMismatch("bigfindentry", state, detail.str());
    }
    return game;
}

}  // namespace

const char* modeName(Mode mode)
{
    switch (mode) {
    case Mode::shadow:
        return "shadow";
    case Mode::replace:
        return "replace";
    default:
        return "off";
    }
}

void reportMismatch(const char* key, TargetState& state, const std::string& detail)
{
    state.mismatches.fetch_add(1, relaxed);
    if (state.reported.fetch_add(1, relaxed) < 5) {
        try {
            writeLine(std::string(key) + " mismatch " + detail);
        } catch (...) {
        }
    }
}

const std::vector<Target>& targets()
{
    // RVAs are relative to the 0x00400000 image base; expected bytes are the
    // first 16 bytes of the shipped function.
    static const std::vector<Target> table = {
        {"refpack", 0x005F72E0,
         {0x51, 0x8B, 0x4C, 0x24, 0x0C, 0x85, 0xC9, 0x56,
          0x8B, 0x74, 0x24, 0x0C, 0x75, 0x07, 0x33, 0xC0},
         5, reinterpret_cast<void*>(&refPackDecode), &g_refPack, false},
        {"namehash", 0x00017DB0,
         {0x53, 0x55, 0x56, 0x57, 0x8B, 0x7C, 0x24, 0x14,
          0x85, 0xFF, 0x0F, 0x84, 0x1F, 0x01, 0x00, 0x00},
         8, reinterpret_cast<void*>(&nameHash), &g_nameHash, false},
        {"pathcompare", 0x005F3650,
         {0x56, 0x8B, 0x74, 0x24, 0x08, 0x57, 0x8B, 0x7C,
          0x24, 0x10, 0x8D, 0x9B, 0x00, 0x00, 0x00, 0x00},
         5, reinterpret_cast<void*>(&pathCompare), &g_pathCompare, false},
        {"bigfindhash", 0x005F6B90,
         {0x83, 0xEC, 0x14, 0x53, 0x55, 0x56, 0x33, 0xF6,
          0x57, 0x8B, 0xE9, 0x89, 0x74, 0x24, 0x14, 0xE8},
         5, reinterpret_cast<void*>(&bigFindHash), &g_bigFindHash, true},
        {"bigfindentry", 0x005F6D00,
         {0x83, 0xEC, 0x10, 0x8B, 0x44, 0x24, 0x28, 0x53,
          0x55, 0x56, 0x8B, 0xE9, 0x83, 0xC9, 0xFF, 0x33},
         7, reinterpret_cast<void*>(&bigFindEntry), &g_bigFindEntry, true},
    };
    return table;
}

}  // namespace ra3hook
