#include "Targets.h"

#include <ra3/asset/NameHash.h>
#include <ra3/common/ByteString.h>
#include <ra3/io/GameBigLayout.h>
#include <ra3/io/PathCompare.h>
#include <ra3/io/RefPack.h>
#include <ra3/text/GameTextLayout.h>

#include <Windows.h>

#include <algorithm>
#include <array>
#include <cstring>
#include <filesystem>
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
TargetState g_copyStringLimited;
TargetState g_appendStringLimited;
TargetState g_compareStringsLimited;
TargetState g_compareStringsCaseLimited;
TargetState g_copyString;
TargetState g_compareStrings;
TargetState g_stringLength;
TargetState g_copyStringLowercase;
TargetState g_findLastCharacter;
TargetState g_widenBytes;
TargetState g_compareBytes;
TargetState g_comparePrefixedCString;
TargetState g_comparePrefixed;
TargetState g_hashLowercaseFnv1a;
TargetState g_decodeUtf8;
TargetState g_encodeUtf8;
TargetState g_decodeUtf8Advance;
TargetState g_refPackProbe;
TargetState g_bigFindHash;
TargetState g_bigFindEntry;
TargetState g_bigReadBe32;
TargetState g_bigReadBe64;
TargetState g_bigDetectFormat;
TargetState g_bigHashPath;
TargetState g_bigHeaderSize;
TargetState g_bigEntryCount;
TargetState g_bigTrailerSize;
TargetState g_bigStoredSize;
TargetState g_bigAbsoluteOffset;
TargetState g_gameTextTable;
TargetState g_gameTextManager;
TargetState g_bigSeek;
TargetState g_bigClose;
TargetState g_bigOpen;
TargetState g_bigRead;
TargetState g_bigFindFirst;
TargetState g_bigFindNext;
TargetState g_bigFindClose;

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

void recordScalar(const char* key, TargetState& state, std::uint32_t game, std::uint32_t ours)
{
    if (game == ours) {
        state.matches.fetch_add(1, relaxed);
    } else {
        try {
            std::ostringstream detail;
            detail << std::hex << std::uppercase << "game=0x" << game << " ours=0x" << ours;
            reportMismatch(key, state, detail.str());
        } catch (...) {
            state.mismatches.fetch_add(1, relaxed);
        }
    }
}

void recordOutput(const char* key, TargetState& state, const void* game, const void* ours,
                  std::size_t size)
{
    if (size == 0 || std::memcmp(game, ours, size) == 0) {
        state.matches.fetch_add(1, relaxed);
    } else {
        try {
            reportMismatch(key, state, "output differs, size=" + std::to_string(size));
        } catch (...) {
            state.mismatches.fetch_add(1, relaxed);
        }
    }
}

// Shadow comparisons may touch only bytes the original reads: a limited source
// need not be terminated within `count`, and the destination buffer may be
// shorter than `count`.
std::size_t boundedLength(const char* text, std::uint32_t count)
{
    std::size_t length = 0;
    while (length < count && text[length] != '\0') {
        ++length;
    }
    return length;
}

using ThreeStringFunction = char*(__cdecl*)(char*, const char*, std::uint32_t);
using ThreeCompareFunction = int(__cdecl*)(const char*, const char*, std::uint32_t);
using TwoStringFunction = char*(__cdecl*)(char*, const char*);
using TwoCompareFunction = int(__cdecl*)(const char*, const char*);

// ra3_1.0.game 0x009F3470.
char* __cdecl copyStringLimited(char* destination, const char* source, std::uint32_t count)
{
    TargetState& state = g_copyStringLimited;
    state.calls.fetch_add(1, relaxed);
    if (state.mode == Mode::replace) {
        state.matches.fetch_add(1, relaxed);
        return ra3::common::copyStringLimited(destination, source, count);
    }
    std::vector<char> expected;
    const std::size_t length = boundedLength(source, count);
    const std::size_t written = length < count ? length + 1 : length;
    try {
        expected.resize(written);
        if (written != 0) {
            ra3::common::copyStringLimited(expected.data(), source, count);
        }
    } catch (...) {
        char* result =
            reinterpret_cast<ThreeStringFunction>(state.original)(destination, source, count);
        state.mismatches.fetch_add(1, relaxed);
        return result;
    }
    char* result =
        reinterpret_cast<ThreeStringFunction>(state.original)(destination, source, count);
    try {
        recordOutput("stringcopylimit", state, destination, expected.data(), written);
    } catch (...) {
        state.mismatches.fetch_add(1, relaxed);
    }
    return result;
}

// ra3_1.0.game 0x009F34B0.
char* __cdecl appendStringLimited(char* destination, const char* source, std::uint32_t count)
{
    TargetState& state = g_appendStringLimited;
    state.calls.fetch_add(1, relaxed);
    if (state.mode == Mode::replace) {
        state.matches.fetch_add(1, relaxed);
        return ra3::common::appendStringLimited(destination, source, count);
    }
    std::vector<char> expected;
    std::size_t size = 0;
    try {
        if (count == 0) {
            char* result =
                reinterpret_cast<ThreeStringFunction>(state.original)(destination, source, count);
            recordScalar("stringappendlimit", state,
                         static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(result)),
                         static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(destination)));
            return result;
        }
        const std::size_t destinationLength = std::strlen(destination);
        const std::size_t copied = boundedLength(source, count);
        size = destinationLength + copied + 1;
        expected.assign(size, '\0');
        std::memcpy(expected.data(), destination, destinationLength + 1);
        ra3::common::appendStringLimited(expected.data(), source, count);
    } catch (...) {
        char* result =
            reinterpret_cast<ThreeStringFunction>(state.original)(destination, source, count);
        state.mismatches.fetch_add(1, relaxed);
        return result;
    }
    char* result =
        reinterpret_cast<ThreeStringFunction>(state.original)(destination, source, count);
    try {
        recordOutput("stringappendlimit", state, destination, expected.data(), size);
    } catch (...) {
        state.mismatches.fetch_add(1, relaxed);
    }
    return result;
}

int compareThree(const char* key, TargetState& state, const char* a, const char* b,
                 std::uint32_t count, int ours)
{
    if (state.mode == Mode::replace) {
        state.matches.fetch_add(1, relaxed);
        return ours;
    }
    const int game =
        reinterpret_cast<ThreeCompareFunction>(state.original)(a, b, count);
    recordScalar(key, state, static_cast<std::uint32_t>(game), static_cast<std::uint32_t>(ours));
    return game;
}

// ra3_1.0.game 0x009F3500.
int __cdecl compareStringsLimited(const char* a, const char* b, std::uint32_t count)
{
    TargetState& state = g_compareStringsLimited;
    state.calls.fetch_add(1, relaxed);
    return compareThree("stringcomparelimit", state, a, b, count,
                        ra3::common::compareStringsLimited(a, b, count));
}

// ra3_1.0.game 0x009F3560.
int __cdecl compareStringsCaseLimited(const char* a, const char* b, std::uint32_t count)
{
    TargetState& state = g_compareStringsCaseLimited;
    state.calls.fetch_add(1, relaxed);
    return compareThree("stringcomparecaselimit", state, a, b, count,
                        ra3::common::compareStringsCaseInsensitiveLimited(a, b, count));
}

char* shadowCopy(const char* key, TargetState& state, char* destination, const char* source,
                 char* (*implementation)(char*, const char*))
{
    if (state.mode == Mode::replace) {
        state.matches.fetch_add(1, relaxed);
        return implementation(destination, source);
    }
    std::vector<char> expected;
    std::size_t size = 0;
    try {
        size = std::strlen(source) + 1;
        expected.resize(size);
        implementation(expected.data(), source);
    } catch (...) {
        char* result =
            reinterpret_cast<TwoStringFunction>(state.original)(destination, source);
        state.mismatches.fetch_add(1, relaxed);
        return result;
    }
    char* result =
        reinterpret_cast<TwoStringFunction>(state.original)(destination, source);
    try {
        recordOutput(key, state, destination, expected.data(), size);
    } catch (...) {
        state.mismatches.fetch_add(1, relaxed);
    }
    return result;
}

// ra3_1.0.game 0x009F35E0.
char* __cdecl copyString(char* destination, const char* source)
{
    TargetState& state = g_copyString;
    state.calls.fetch_add(1, relaxed);
    return shadowCopy("stringcopy", state, destination, source, &ra3::common::copyString);
}

// ra3_1.0.game 0x009F3610.
int __cdecl compareStrings(const char* a, const char* b)
{
    TargetState& state = g_compareStrings;
    state.calls.fetch_add(1, relaxed);
    const int ours = ra3::common::compareStrings(a, b);
    if (state.mode == Mode::replace) {
        state.matches.fetch_add(1, relaxed);
        return ours;
    }
    const int game = reinterpret_cast<TwoCompareFunction>(state.original)(a, b);
    recordScalar("stringcompare", state, static_cast<std::uint32_t>(game),
                 static_cast<std::uint32_t>(ours));
    return game;
}

// ra3_1.0.game 0x009F36C0.
std::uint32_t __cdecl stringLength(const char* text)
{
    TargetState& state = g_stringLength;
    state.calls.fetch_add(1, relaxed);
    const std::uint32_t ours = ra3::common::stringLength(text);
    if (state.mode == Mode::replace) {
        state.matches.fetch_add(1, relaxed);
        return ours;
    }
    const auto game = reinterpret_cast<std::uint32_t(__cdecl*)(const char*)>(state.original)(text);
    recordScalar("stringlength", state, game, ours);
    return game;
}

// ra3_1.0.game 0x009F36E0.
char* __cdecl copyStringLowercase(char* destination, const char* source)
{
    TargetState& state = g_copyStringLowercase;
    state.calls.fetch_add(1, relaxed);
    return shadowCopy(
        "stringlowercopy", state, destination, source, &ra3::common::copyStringLowercase);
}

// ra3_1.0.game 0x009F3720.
const char* __cdecl findLastCharacter(const char* text, int character)
{
    TargetState& state = g_findLastCharacter;
    state.calls.fetch_add(1, relaxed);
    const char* ours =
        ra3::common::findLastCharacter(text, static_cast<std::uint8_t>(character));
    if (state.mode == Mode::replace) {
        state.matches.fetch_add(1, relaxed);
        return ours;
    }
    const auto game =
        reinterpret_cast<const char*(__cdecl*)(const char*, int)>(state.original)(text, character);
    const auto gameOffset = game == nullptr ? -1 : game - text;
    const auto oursOffset = ours == nullptr ? -1 : ours - text;
    recordScalar("stringfindlast", state, static_cast<std::uint32_t>(gameOffset),
                 static_cast<std::uint32_t>(oursOffset));
    return game;
}

// ra3_1.0.game 0x009F3F10.
std::uint16_t* __cdecl widenBytes(std::uint16_t* destination, const char* source)
{
    TargetState& state = g_widenBytes;
    state.calls.fetch_add(1, relaxed);
    if (state.mode == Mode::replace) {
        state.matches.fetch_add(1, relaxed);
        return ra3::common::widenBytes(destination, source);
    }
    std::vector<std::uint16_t> expected;
    std::size_t count = 0;
    try {
        count = std::strlen(source) + 1;
        expected.resize(count);
        ra3::common::widenBytes(expected.data(), source);
    } catch (...) {
        auto game = reinterpret_cast<std::uint16_t*(__cdecl*)(std::uint16_t*, const char*)>(
            state.original)(destination, source);
        state.mismatches.fetch_add(1, relaxed);
        return game;
    }
    auto game = reinterpret_cast<std::uint16_t*(__cdecl*)(std::uint16_t*, const char*)>(
        state.original)(destination, source);
    try {
        recordOutput("bytewiden", state, destination, expected.data(),
                     count * sizeof(std::uint16_t));
    } catch (...) {
        state.mismatches.fetch_add(1, relaxed);
    }
    return game;
}

// ra3_1.0.game 0x004C9EA0.
int __stdcall compareBytes(const std::uint8_t* a, const std::uint8_t* b, std::uint32_t length)
{
    TargetState& state = g_compareBytes;
    state.calls.fetch_add(1, relaxed);
    const int ours = ra3::common::compareBytes(a, b, length);
    if (state.mode == Mode::replace) {
        state.matches.fetch_add(1, relaxed);
        return ours;
    }
    const int game = reinterpret_cast<int(__stdcall*)(
        const std::uint8_t*, const std::uint8_t*, std::uint32_t)>(
        state.original)(a, b, length);
    recordScalar("bytecompare", state, static_cast<std::uint32_t>(game),
                 static_cast<std::uint32_t>(ours));
    return game;
}

struct GameString {
    const char* data;
};

std::uint16_t gameStringLength(const GameString* string)
{
    if (string->data == nullptr) {
        return 0;
    }
    std::uint16_t length = 0;
    std::memcpy(&length, string->data - 4, sizeof(length));
    return length;
}

// ra3_1.0.game 0x004CB3A0.
int __fastcall comparePrefixedCString(
    const GameString* self, void*, const char* other)
{
    TargetState& state = g_comparePrefixedCString;
    state.calls.fetch_add(1, relaxed);
    const int ours = ra3::common::compareLengthPrefixedToCString(
        self->data, gameStringLength(self), other);
    if (state.mode == Mode::replace) {
        state.matches.fetch_add(1, relaxed);
        return ours;
    }
    const int game =
        reinterpret_cast<int(__thiscall*)(const GameString*, const char*)>(
            state.original)(self, other);
    recordScalar("prefixedccompare", state, static_cast<std::uint32_t>(game),
                 static_cast<std::uint32_t>(ours));
    return game;
}

// ra3_1.0.game 0x004CB400.
int __fastcall comparePrefixed(
    const GameString* self, void*, const GameString* other)
{
    TargetState& state = g_comparePrefixed;
    state.calls.fetch_add(1, relaxed);
    const int ours = ra3::common::compareLengthPrefixed(
        self->data, gameStringLength(self), other->data, gameStringLength(other));
    if (state.mode == Mode::replace) {
        state.matches.fetch_add(1, relaxed);
        return ours;
    }
    const int game =
        reinterpret_cast<int(__thiscall*)(const GameString*, const GameString*)>(
            state.original)(self, other);
    recordScalar("prefixedcompare", state, static_cast<std::uint32_t>(game),
                 static_cast<std::uint32_t>(ours));
    return game;
}

// ra3_1.0.game 0x00A28290.
std::uint32_t __cdecl hashLowercaseFnv1a(const char* text)
{
    TargetState& state = g_hashLowercaseFnv1a;
    state.calls.fetch_add(1, relaxed);
    const std::uint32_t ours = ra3::common::hashLowercaseFnv1a(text);
    if (state.mode == Mode::replace) {
        state.matches.fetch_add(1, relaxed);
        return ours;
    }
    const auto game = reinterpret_cast<std::uint32_t(__cdecl*)(const char*)>(
        state.original)(text);
    recordScalar("fnvhash", state, game, ours);
    return game;
}

// ra3_1.0.game 0x00A282D0.
std::uint32_t __cdecl decodeUtf8(const std::uint8_t* text)
{
    TargetState& state = g_decodeUtf8;
    state.calls.fetch_add(1, relaxed);
    const std::uint32_t ours = ra3::common::decodeUtf8(text);
    if (state.mode == Mode::replace) {
        state.matches.fetch_add(1, relaxed);
        return ours;
    }
    const auto game = reinterpret_cast<std::uint32_t(__cdecl*)(const std::uint8_t*)>(
        state.original)(text);
    recordScalar("utf8decode", state, game, ours);
    return game;
}

// ra3_1.0.game 0x00A28350.
void __cdecl encodeUtf8(std::uint8_t* destination, std::uint32_t codePoint)
{
    TargetState& state = g_encodeUtf8;
    state.calls.fetch_add(1, relaxed);
    if (state.mode == Mode::replace) {
        ra3::common::encodeUtf8(destination, codePoint);
        state.matches.fetch_add(1, relaxed);
        return;
    }
    std::uint8_t expected[4]{};
    ra3::common::encodeUtf8(expected, codePoint);
    reinterpret_cast<void(__cdecl*)(std::uint8_t*, std::uint32_t)>(
        state.original)(destination, codePoint);
    const auto signedPoint = static_cast<std::int32_t>(codePoint);
    const std::size_t size =
        signedPoint < 0x80 ? 1u : (signedPoint < 0x800 ? 2u : (signedPoint < 0x10000 ? 3u : 4u));
    recordOutput("utf8encode", state, destination, expected, size);
}

// ra3_1.0.game 0x00A284B0.
const std::uint8_t* __cdecl decodeUtf8Advance(
    const std::uint8_t* text, std::uint32_t* codePoint)
{
    TargetState& state = g_decodeUtf8Advance;
    state.calls.fetch_add(1, relaxed);
    std::uint32_t oursValue = 0;
    const std::uint8_t* ours = ra3::common::decodeUtf8AndAdvance(text, oursValue);
    if (state.mode == Mode::replace) {
        *codePoint = oursValue;
        state.matches.fetch_add(1, relaxed);
        return ours;
    }
    const auto game = reinterpret_cast<const std::uint8_t*(__cdecl*)(
        const std::uint8_t*, std::uint32_t*)>(state.original)(text, codePoint);
    const bool equal = *codePoint == oursValue && game - text == ours - text;
    if (equal) {
        state.matches.fetch_add(1, relaxed);
    } else {
        try {
            reportMismatch("utf8next", state, "decoded value or consumed length differs");
        } catch (...) {
            state.mismatches.fetch_add(1, relaxed);
        }
    }
    return game;
}

// ra3_1.0.game 0x009F74F0: RefPack size probe, 0 for non-RefPack data.
using ProbeFunction = std::uint32_t(__stdcall*)(const std::uint8_t*);

std::uint32_t __stdcall refPackProbe(const std::uint8_t* data)
{
    TargetState& state = g_refPackProbe;
    state.calls.fetch_add(1, relaxed);
    const std::uint32_t ours = ra3::io::probeRefPackSize(data);
    if (state.mode == Mode::replace) {
        state.matches.fetch_add(1, relaxed);
        return ours;
    }
    const std::uint32_t game = reinterpret_cast<ProbeFunction>(state.original)(data);
    if (game == ours) {
        state.matches.fetch_add(1, relaxed);
    } else {
        const bool signature = (data[0] & 0x3E) == 0x10 && data[1] == 0xFB;
        const int read = signature ? ((data[0] & 0x80) ? 6 : 5) : 2;
        std::ostringstream detail;
        detail << std::hex << std::uppercase << std::setfill('0') << "header";
        for (int i = 0; i < read; ++i) {
            detail << ' ' << std::setw(2) << static_cast<unsigned>(data[i]);
        }
        detail << " game=0x" << game << " ours=0x" << ours;
        reportMismatch("refpackprobe", state, detail.str());
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

void recordScalar64(
    const char* key, TargetState& state, std::uint64_t game, std::uint64_t ours)
{
    if (game == ours) {
        state.matches.fetch_add(1, relaxed);
    } else {
        try {
            std::ostringstream detail;
            detail << std::hex << std::uppercase << "game=0x" << game << " ours=0x" << ours;
            reportMismatch(key, state, detail.str());
        } catch (...) {
            state.mismatches.fetch_add(1, relaxed);
        }
    }
}

std::uint32_t __cdecl bigReadBe32(const std::uint8_t* bytes, int width)
{
    TargetState& state = g_bigReadBe32;
    state.calls.fetch_add(1, relaxed);
    const std::uint32_t ours = ra3::io::readGameBigEndian32(bytes, width);
    if (state.mode == Mode::replace) {
        state.matches.fetch_add(1, relaxed);
        return ours;
    }
    const auto game = reinterpret_cast<std::uint32_t(__cdecl*)(const std::uint8_t*, int)>(
        state.original)(bytes, width);
    recordScalar("bigreadbe32", state, game, ours);
    return game;
}

std::uint64_t __cdecl bigReadBe64(const std::uint8_t* bytes, int width)
{
    TargetState& state = g_bigReadBe64;
    state.calls.fetch_add(1, relaxed);
    const std::uint64_t ours = ra3::io::readGameBigEndian64(bytes, width);
    if (state.mode == Mode::replace) {
        state.matches.fetch_add(1, relaxed);
        return ours;
    }
    const auto game = reinterpret_cast<std::uint64_t(__cdecl*)(const std::uint8_t*, int)>(
        state.original)(bytes, width);
    recordScalar64("bigreadbe64", state, game, ours);
    return game;
}

std::uint32_t __cdecl bigDetectFormat(const std::uint8_t* header)
{
    TargetState& state = g_bigDetectFormat;
    state.calls.fetch_add(1, relaxed);
    const auto ours = static_cast<std::uint32_t>(ra3::io::detectGameBigFormat(header));
    if (state.mode == Mode::replace) {
        state.matches.fetch_add(1, relaxed);
        return ours;
    }
    const auto game = reinterpret_cast<std::uint32_t(__cdecl*)(const std::uint8_t*)>(
        state.original)(header);
    recordScalar("bigformat", state, game, ours);
    return game;
}

std::uint32_t __cdecl bigHashPath(const char* path, std::uint32_t seed)
{
    TargetState& state = g_bigHashPath;
    state.calls.fetch_add(1, relaxed);
    const std::uint32_t ours = ra3::io::hashGameBigPath(path, seed);
    if (state.mode == Mode::replace) {
        state.matches.fetch_add(1, relaxed);
        return ours;
    }
    const auto game = reinterpret_cast<std::uint32_t(__cdecl*)(const char*, std::uint32_t)>(
        state.original)(path, seed);
    recordScalar("bighashpath", state, game, ours);
    return game;
}

int __cdecl bigHeaderSize(const std::uint8_t* header)
{
    TargetState& state = g_bigHeaderSize;
    state.calls.fetch_add(1, relaxed);
    const int ours = ra3::io::gameBigHeaderSize(header);
    if (state.mode == Mode::replace) {
        state.matches.fetch_add(1, relaxed);
        return ours;
    }
    const int game =
        reinterpret_cast<int(__cdecl*)(const std::uint8_t*)>(state.original)(header);
    recordScalar("bigheadersize", state, static_cast<std::uint32_t>(game),
                 static_cast<std::uint32_t>(ours));
    return game;
}

std::uint32_t __fastcall bigEntryCount(
    const ra3::io::GameBigIndex* index, void*)
{
    TargetState& state = g_bigEntryCount;
    state.calls.fetch_add(1, relaxed);
    const std::uint32_t ours = ra3::io::gameBigEntryCount(index);
    if (state.mode == Mode::replace) {
        state.matches.fetch_add(1, relaxed);
        return ours;
    }
    const auto game = reinterpret_cast<std::uint32_t(__thiscall*)(
        const ra3::io::GameBigIndex*)>(state.original)(index);
    recordScalar("bigentrycount", state, game, ours);
    return game;
}

int __fastcall bigTrailerSize(
    const ra3::io::GameBigIndex* index, void*, int* numberOut)
{
    TargetState& state = g_bigTrailerSize;
    state.calls.fetch_add(1, relaxed);
    if (state.mode == Mode::replace) {
        const int result = ra3::io::gameBigTrailerSize(index, numberOut);
        state.matches.fetch_add(1, relaxed);
        return result;
    }
    int expectedNumber = *numberOut;
    const int ours = ra3::io::gameBigTrailerSize(index, &expectedNumber);
    const int game = reinterpret_cast<int(__thiscall*)(
        const ra3::io::GameBigIndex*, int*)>(state.original)(index, numberOut);
    if (game == ours && *numberOut == expectedNumber) {
        state.matches.fetch_add(1, relaxed);
    } else {
        reportMismatch("bigtrailersize", state, "size or trailer number differs");
    }
    return game;
}

// ra3_1.0.game 0x00539080: lookup in one sorted GameText table.
using GameTextTableFunction = const ra3::text::GameUnicodeString*(__thiscall*)(
    const ra3::text::GameStringTable*, const char*);

const ra3::text::GameUnicodeString* __fastcall gameTextTable(
    const ra3::text::GameStringTable* table, void*, const char* label)
{
    TargetState& state = g_gameTextTable;
    state.calls.fetch_add(1, relaxed);
    if (state.mode == Mode::replace) {
        state.matches.fetch_add(1, relaxed);
        return ra3::text::findGameText(table, label);
    }
    const ra3::text::GameUnicodeString* game =
        reinterpret_cast<GameTextTableFunction>(state.original)(table, label);
    const ra3::text::GameUnicodeString* ours = ra3::text::findGameText(table, label);
    if (game == ours) {
        state.matches.fetch_add(1, relaxed);
    } else {
        reportMismatch("gametexttable", state,
                       describeName(label, static_cast<std::uint32_t>(std::strlen(label))));
    }
    return game;
}

// ra3_1.0.game 0x0053B500: manager lookup with override/primary/map precedence.
using GameTextManagerFunction = const ra3::text::GameUnicodeString*(__thiscall*)(
    const ra3::text::GameTextManager*, const char*);

const ra3::text::GameUnicodeString* __fastcall gameTextManager(
    const ra3::text::GameTextManager* manager, void*, const char* label)
{
    TargetState& state = g_gameTextManager;
    state.calls.fetch_add(1, relaxed);
    if (state.mode == Mode::replace) {
        state.matches.fetch_add(1, relaxed);
        return ra3::text::findGameText(manager, label);
    }
    const ra3::text::GameUnicodeString* game =
        reinterpret_cast<GameTextManagerFunction>(state.original)(manager, label);
    const ra3::text::GameUnicodeString* ours = ra3::text::findGameText(manager, label);
    if (game == ours) {
        state.matches.fetch_add(1, relaxed);
    } else {
        reportMismatch("gametextmanager", state,
                       describeName(label, static_cast<std::uint32_t>(std::strlen(label))));
    }
    return game;
}

std::uint64_t __stdcall bigStoredSize(const ra3::io::GameBigFile* file)
{
    TargetState& state = g_bigStoredSize;
    state.calls.fetch_add(1, relaxed);
    const std::uint64_t ours = ra3::io::gameBigStoredSize(file);
    if (state.mode == Mode::replace) {
        state.matches.fetch_add(1, relaxed);
        return ours;
    }
    const auto game = reinterpret_cast<std::uint64_t(__stdcall*)(
        const ra3::io::GameBigFile*)>(state.original)(file);
    recordScalar64("bigstoredsize", state, game, ours);
    return game;
}

std::uint64_t __stdcall bigAbsoluteOffset(const ra3::io::GameBigFile* file)
{
    TargetState& state = g_bigAbsoluteOffset;
    state.calls.fetch_add(1, relaxed);
    const std::uint64_t ours = ra3::io::gameBigAbsoluteDataOffset(file);
    if (state.mode == Mode::replace) {
        state.matches.fetch_add(1, relaxed);
        return ours;
    }
    const auto game = reinterpret_cast<std::uint64_t(__stdcall*)(
        const ra3::io::GameBigFile*)>(state.original)(file);
    recordScalar64("bigabsoluteoffset", state, game, ours);
    return game;
}

void releaseGameAllocation(void* context, void* allocation);

std::filesystem::path archiveDiskPath(const ra3::io::GameBigArchive* archive)
{
    const char* name = archive->archiveName;
    const char* colon = std::strchr(name, ':');
    if (colon != nullptr) {
        name = colon + 1;
    }
    while (*name == '/' || *name == '\\') {
        ++name;
    }
    std::array<wchar_t, 32768> module{};
    const DWORD length = GetModuleFileNameW(
        nullptr, module.data(), static_cast<DWORD>(module.size()));
    return std::filesystem::path(std::wstring(module.data(), length)).parent_path()
        / std::filesystem::path(name).filename();
}

std::uint32_t readArchiveRange(
    void* context,
    const ra3::io::GameBigArchive* archive,
    std::uint64_t memberOffset,
    void* destination,
    std::uint32_t count)
{
    if (count == 0) {
        return 0;
    }
    const std::filesystem::path path = archiveDiskPath(archive);
    const HANDLE handle = CreateFileW(
        path.c_str(), GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE,
        nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (handle == INVALID_HANDLE_VALUE) {
        if (context != nullptr) {
            *static_cast<bool*>(context) = true;
        }
        return 0;
    }
    LARGE_INTEGER offset{};
    offset.QuadPart = static_cast<LONGLONG>(archive->rangeBegin + memberOffset);
    DWORD actual = 0;
    const bool ok = SetFilePointerEx(handle, offset, nullptr, FILE_BEGIN) != 0
        && ReadFile(handle, destination, count, &actual, nullptr) != 0;
    CloseHandle(handle);
    if (!ok && context != nullptr) {
        *static_cast<bool*>(context) = true;
    }
    return ok ? actual : 0;
}

void* allocateGameTagged(
    ra3::io::GameAllocator* allocator,
    std::uint32_t size,
    const char* tag)
{
    return reinterpret_cast<void*(__thiscall*)(
        ra3::io::GameAllocator*, std::uint32_t, const char*, std::uint32_t)>(
        allocator->vtable->allocateTagged)(allocator, size, tag, 0);
}

void* allocateGameTemporary(
    ra3::io::GameAllocator* allocator,
    std::uint32_t size,
    const char* tag)
{
    return reinterpret_cast<void*(__thiscall*)(
        ra3::io::GameAllocator*,
        std::uint32_t,
        const char*,
        std::uint32_t,
        std::uint32_t,
        std::uint32_t)>(
        allocator->vtable->allocateTemporary)(
        allocator, size, tag, 0, 4, 0);
}

using BigOpenFunction = ra3::io::GameBigFile*(__thiscall*)(
    ra3::io::GameBigHandler*, const char*, std::uint32_t,
    ra3::io::GameArchiveFile**);

ra3::io::GameBigFile* __fastcall bigOpen(
    ra3::io::GameBigHandler* handler,
    void*,
    const char* path,
    std::uint32_t openMode,
    ra3::io::GameArchiveFile** archiveFileOut)
{
    TargetState& state = g_bigOpen;
    state.calls.fetch_add(1, relaxed);
    const auto* image = reinterpret_cast<const std::uint8_t*>(
        GetModuleHandleW(nullptr));
    const std::uint32_t ownerToken =
        *reinterpret_cast<const std::uint32_t*>(image + 0x915AC0);

    if (state.mode == Mode::replace) {
        ra3::io::GameBigFile expected{};
        bool rangeReadFailed = false;
        if (!ra3::io::prepareGameBigFile(
                handler, path, ownerToken, openMode, &readArchiveRange,
                &rangeReadFailed, &expected)
            || rangeReadFailed) {
            return nullptr;
        }
        if (archiveFileOut != nullptr) {
            *archiveFileOut = expected.archive->file;
        }
        auto* result = static_cast<ra3::io::GameBigFile*>(
            allocateGameTagged(
                handler->allocator, sizeof(expected), "BigFileHandler"));
        if (result == nullptr) {
            return nullptr;
        }
        *result = expected;
        state.matches.fetch_add(1, relaxed);
        return result;
    }

    ra3::io::GameBigFile* game =
        reinterpret_cast<BigOpenFunction>(state.original)(
            handler, path, openMode, archiveFileOut);
    ra3::io::GameBigFile expected{};
    bool rangeReadFailed = false;
    const bool found = ra3::io::prepareGameBigFile(
        handler, path, ownerToken, openMode, &readArchiveRange, &rangeReadFailed,
        &expected);
    if (rangeReadFailed) {
        state.skipped.fetch_add(1, relaxed);
    } else if ((!found && game == nullptr)
        || (found && game != nullptr
            && std::memcmp(game, &expected, sizeof(expected)) == 0
            && (archiveFileOut == nullptr
                || *archiveFileOut == expected.archive->file))) {
        state.matches.fetch_add(1, relaxed);
    } else if (found || game != nullptr) {
        reportMismatch("bigopen", state, "return or 0x40 file state differs");
    } else {
        state.skipped.fetch_add(1, relaxed);
    }
    return game;
}

std::uint32_t __fastcall runReplacementInflater(
    ra3::io::GameRefPackInflater* inflater, void*, std::uint32_t)
{
    return ra3::io::runGameRefPackInflater(inflater);
}

void __fastcall deleteReplacementInflater(
    ra3::io::GameRefPackInflater* inflater, void*, std::uint32_t flags)
{
    ra3::io::GameAllocator* allocator = inflater->allocator;
    ra3::io::destroyGameRefPackInflater(inflater);
    if ((flags & 1u) != 0) {
        releaseGameAllocation(allocator, inflater);
    }
}

struct ReplacementInflaterVtable {
    void* deletingDestructor;
    void* unknown04;
    void* unknown08;
    void* run;
};

const ReplacementInflaterVtable g_replacementInflaterVtable{
    reinterpret_cast<void*>(&deleteReplacementInflater),
    nullptr,
    nullptr,
    reinterpret_cast<void*>(&runReplacementInflater)};

using ArchiveSeekFunction = void(__thiscall*)(
    ra3::io::GameArchiveFile*, void*, std::uint32_t, std::uint32_t,
    std::uint32_t, std::uint32_t, std::uint32_t);
using ArchiveReadFunction = std::uint32_t(__thiscall*)(
    ra3::io::GameArchiveFile*, void*, void*, std::uint32_t,
    std::uint32_t, std::uint32_t);

using BigReadFunction = std::uint32_t(__thiscall*)(
    ra3::io::GameBigHandler*, ra3::io::GameBigFile*, void*,
    std::uint32_t, ra3::io::GameArchiveFile*, void*);

struct GameIoContext {
    ra3::io::GameBigHandler* handler;
    ra3::io::GameArchiveFile* archiveFile;
    void* fileContext;
};

void gameIoLock(void*, const ra3::io::GameBigArchive* archive)
{
    const auto* image = reinterpret_cast<const std::uint8_t*>(
        GetModuleHandleW(nullptr));
    const auto acquire = reinterpret_cast<int(__thiscall*)(
        void*, const std::uint32_t*)>(image + 0x6216A0);
    const std::uint32_t infinite = 0xFFFFFFFFu;
    acquire(const_cast<std::uint8_t*>(archive->synchronization.data()), &infinite);
}

void gameIoUnlock(void*, const ra3::io::GameBigArchive* archive)
{
    const auto* image = reinterpret_cast<const std::uint8_t*>(
        GetModuleHandleW(nullptr));
    const auto release =
        reinterpret_cast<int(__fastcall*)(void*)>(image + 0x621760);
    release(const_cast<std::uint8_t*>(archive->synchronization.data()));
}

void gameIoSeek(void* opaque, std::uint64_t position)
{
    auto* context = static_cast<GameIoContext*>(opaque);
    reinterpret_cast<ArchiveSeekFunction>(context->archiveFile->vtable->seek)(
        context->archiveFile, context->fileContext,
        static_cast<std::uint32_t>(position),
        static_cast<std::uint32_t>(position >> 32), 0, 0, 0);
}

std::uint32_t gameIoRead(
    void* opaque, void* destination, std::uint32_t count)
{
    auto* context = static_cast<GameIoContext*>(opaque);
    return reinterpret_cast<ArchiveReadFunction>(
        context->archiveFile->vtable->read)(
        context->archiveFile, context->fileContext, destination, count, 0, 0);
}

ra3::io::GameRefPackInflater* gameIoCreateInflater(void* opaque)
{
    auto* context = static_cast<GameIoContext*>(opaque);
    auto* inflater = static_cast<ra3::io::GameRefPackInflater*>(
        allocateGameTagged(
            context->handler->allocator,
            sizeof(ra3::io::GameRefPackInflater),
            "rw::core::codec::RefpackInflate"));
    ra3::io::initializeGameRefPackInflater(
        inflater, context->handler->allocator, &g_replacementInflaterVtable);
    return inflater;
}

void* gameIoAllocateTemporary(void* opaque, std::uint32_t size)
{
    auto* context = static_cast<GameIoContext*>(opaque);
    return allocateGameTemporary(
        context->handler->allocator, size,
        "rw::core::BigHandler::temp Z buf");
}

std::uint32_t readReplacement(
    ra3::io::GameBigHandler* handler,
    ra3::io::GameBigFile* file,
    void* destination,
    std::uint32_t count,
    ra3::io::GameArchiveFile* archiveFile,
    void* fileContext)
{
    GameIoContext context{handler, archiveFile, fileContext};
    const ra3::io::GameBigIoOps operations{
        &context, &gameIoLock, &gameIoUnlock, &gameIoSeek, &gameIoRead,
        &gameIoCreateInflater, &gameIoAllocateTemporary};
    return ra3::io::readGameBigFile(file, destination, count, operations);
}

std::uint32_t __fastcall bigRead(
    ra3::io::GameBigHandler* handler,
    void*,
    ra3::io::GameBigFile* file,
    void* destination,
    std::uint32_t count,
    ra3::io::GameArchiveFile* archiveFile,
    void* fileContext)
{
    TargetState& state = g_bigRead;
    state.calls.fetch_add(1, relaxed);
    if (state.mode == Mode::replace) {
        const std::uint32_t result = readReplacement(
            handler, file, destination, count, archiveFile, fileContext);
        state.matches.fetch_add(1, relaxed);
        return result;
    }

    const ra3::io::GameBigFile before = *file;
    const std::uint32_t game =
        reinterpret_cast<BigReadFunction>(state.original)(
            handler, file, destination, count, archiveFile, fileContext);
    ra3::io::GameBigFile expected = before;
    ra3::io::applyGameBigReadResult(&expected, game);
    const bool stable =
        file->archive == before.archive
        && file->ownerToken == before.ownerToken
        && file->openMode == before.openMode
        && file->unknown14 == before.unknown14
        && file->dataOffset == before.dataOffset
        && file->logicalSize == before.logicalSize
        && file->compressedSize == before.compressedSize;
    const bool bookkeeping = stable && (before.compressedSize == 0
        ? file->sourcePosition == expected.sourcePosition
            && file->logicalPosition == expected.logicalPosition
            && file->inflater == before.inflater
            && file->temporaryBuffer == before.temporaryBuffer
        : file->logicalPosition == expected.logicalPosition
            && file->sourcePosition >= before.sourcePosition
            && file->sourcePosition
                <= before.dataOffset + before.compressedSize);
    constexpr std::uint64_t maxVerification = 8u * 1024u * 1024u;
    bool bytes = true;
    try {
        if (game != 0 && before.logicalPosition <= maxVerification - game) {
            const std::uint64_t storedSize = before.compressedSize == 0
                ? before.logicalSize : before.compressedSize;
            if (storedSize > maxVerification) {
                state.skipped.fetch_add(1, relaxed);
                return game;
            }
            std::vector<std::uint8_t> stored(
                static_cast<std::size_t>(storedSize));
            if (readArchiveRange(
                    nullptr, before.archive, before.dataOffset,
                    stored.data(), static_cast<std::uint32_t>(stored.size()))
                != stored.size()) {
                state.skipped.fetch_add(1, relaxed);
                return game;
            }
            if (before.compressedSize == 0) {
                bytes = std::memcmp(
                    destination,
                    stored.data() + static_cast<std::size_t>(
                        before.sourcePosition - before.dataOffset),
                    game) == 0;
            } else {
                auto decoded = ra3::io::decompressRefPackPrefix(
                    stored.data(), stored.size(),
                    static_cast<std::size_t>(before.logicalPosition + game));
                bytes = decoded
                    && decoded.value().size() >= before.logicalPosition + game
                    && std::memcmp(
                        destination,
                        decoded.value().data()
                            + static_cast<std::size_t>(before.logicalPosition),
                        game) == 0;
            }
        } else if (game != 0) {
            state.skipped.fetch_add(1, relaxed);
            return game;
        }
    } catch (...) {
        state.skipped.fetch_add(1, relaxed);
        return game;
    }
    if (bookkeeping && bytes) {
        state.matches.fetch_add(1, relaxed);
    } else {
        reportMismatch("bigread", state, "bookkeeping or returned bytes differ");
    }
    return game;
}

using BigFindFirstFunction = ra3::io::GameBigFindState*(__thiscall*)(
    ra3::io::GameBigHandler*, const char*, ra3::io::GameBigFindResult*);
using BigFindNextFunction = bool(__thiscall*)(
    ra3::io::GameBigHandler*, ra3::io::GameBigFindState*,
    ra3::io::GameBigFindResult*);

const char* enumerationPattern(const char* pattern)
{
    const char* colon = std::strchr(pattern, ':');
    if (colon == nullptr) {
        return pattern;
    }
    pattern = colon + 1;
    return (*pattern == '/' || *pattern == '\\') ? pattern + 1 : pattern;
}

bool equalFindResult(
    const ra3::io::GameBigFindResult* left,
    const ra3::io::GameBigFindResult* right)
{
    return left->type == right->type && left->size == right->size
        && std::strcmp(left->name, right->name) == 0;
}

bool __fastcall bigFindNext(
    ra3::io::GameBigHandler* handler,
    void*,
    ra3::io::GameBigFindState* cursor,
    ra3::io::GameBigFindResult* result)
{
    TargetState& state = g_bigFindNext;
    state.calls.fetch_add(1, relaxed);
    if (state.mode == Mode::replace) {
        const bool found = ra3::io::findNextGameBig(cursor, result);
        state.matches.fetch_add(1, relaxed);
        return found;
    }
    ra3::io::GameBigFindState expected = *cursor;
    std::array<std::uint8_t, 0x200> expectedBytes{};
    auto* expectedResult =
        reinterpret_cast<ra3::io::GameBigFindResult*>(expectedBytes.data());
    const bool ours = ra3::io::findNextGameBig(&expected, expectedResult);
    const bool game = reinterpret_cast<BigFindNextFunction>(state.original)(
        handler, cursor, result);
    if (game == ours
        && std::memcmp(cursor, &expected, sizeof(expected)) == 0
        && (!game || equalFindResult(result, expectedResult))) {
        state.matches.fetch_add(1, relaxed);
    } else {
        reportMismatch("bigfindnext", state, "cursor or result differs");
    }
    return game;
}

ra3::io::GameBigFindState* __fastcall bigFindFirst(
    ra3::io::GameBigHandler* handler,
    void*,
    const char* pattern,
    ra3::io::GameBigFindResult* result)
{
    TargetState& state = g_bigFindFirst;
    state.calls.fetch_add(1, relaxed);
    if (state.mode == Mode::replace) {
        auto* cursor = static_cast<ra3::io::GameBigFindState*>(
            allocateGameTagged(
                handler->allocator, sizeof(ra3::io::GameBigFindState),
                "BigHandler::FindStruct"));
        if (cursor == nullptr) {
            return nullptr;
        }
        ra3::io::initializeGameBigFindState(
            cursor, enumerationPattern(pattern), handler->head);
        if (!ra3::io::findNextGameBig(cursor, result)) {
            releaseGameAllocation(handler->allocator, cursor);
            return nullptr;
        }
        state.matches.fetch_add(1, relaxed);
        return cursor;
    }
    ra3::io::GameBigFindState expected{};
    ra3::io::initializeGameBigFindState(
        &expected, enumerationPattern(pattern), handler->head);
    std::array<std::uint8_t, 0x200> expectedBytes{};
    auto* expectedResult =
        reinterpret_cast<ra3::io::GameBigFindResult*>(expectedBytes.data());
    const bool ours = ra3::io::findNextGameBig(&expected, expectedResult);
    auto* game = reinterpret_cast<BigFindFirstFunction>(state.original)(
        handler, pattern, result);
    if ((game == nullptr && !ours)
        || (game != nullptr && ours
            && std::memcmp(game, &expected, sizeof(expected)) == 0
            && equalFindResult(result, expectedResult))) {
        state.matches.fetch_add(1, relaxed);
    } else {
        reportMismatch("bigfindfirst", state, "cursor or first result differs");
    }
    return game;
}

using BigFindCloseFunction = bool(__thiscall*)(
    ra3::io::GameBigHandler*, ra3::io::GameBigFindState*);

bool __fastcall bigFindClose(
    ra3::io::GameBigHandler* handler,
    void*,
    ra3::io::GameBigFindState* cursor)
{
    TargetState& state = g_bigFindClose;
    state.calls.fetch_add(1, relaxed);
    if (state.mode == Mode::shadow) {
        const bool expected = cursor != nullptr;
        const bool game = reinterpret_cast<BigFindCloseFunction>(
            state.original)(handler, cursor);
        if (game == expected) {
            state.matches.fetch_add(1, relaxed);
        } else {
            reportMismatch("bigfindclose", state, "return value differs");
        }
        return game;
    }
    if (cursor == nullptr) {
        return false;
    }
    releaseGameAllocation(handler->allocator, cursor);
    state.matches.fetch_add(1, relaxed);
    return true;
}

using BigSeekFunction = std::uint64_t(__thiscall*)(
    ra3::io::GameBigHandler*,
    ra3::io::GameBigFile*,
    std::uint64_t,
    std::uint32_t,
    std::uint32_t,
    std::uint32_t);

void destroyGameInflater(void*, void* inflater)
{
    const auto vtable = *static_cast<void***>(inflater);
    reinterpret_cast<void(__thiscall*)(void*, int)>(vtable[0])(inflater, 1);
}

void releaseGameAllocation(void* context, void* allocation)
{
    auto* allocator = static_cast<ra3::io::GameAllocator*>(context);
    reinterpret_cast<void(__thiscall*)(
        ra3::io::GameAllocator*, void*, std::uint32_t)>(
        allocator->vtable->freeTagged)(allocator, allocation, 0);
}

std::uint64_t __fastcall bigSeek(
    ra3::io::GameBigHandler* handler,
    void*,
    ra3::io::GameBigFile* file,
    std::uint64_t position,
    std::uint32_t argument3,
    std::uint32_t argument4,
    std::uint32_t argument5)
{
    TargetState& state = g_bigSeek;
    state.calls.fetch_add(1, relaxed);
    if (state.mode == Mode::replace) {
        const ra3::io::GameBigReleaseOps operations{
            handler->allocator, &destroyGameInflater, &releaseGameAllocation};
        state.matches.fetch_add(1, relaxed);
        return ra3::io::seekGameBigFile(file, position, operations);
    }

    ra3::io::GameBigFile expected = *file;
    const std::uint64_t game = reinterpret_cast<BigSeekFunction>(state.original)(
        handler, file, position, argument3, argument4, argument5);
    const bool reset = ra3::io::applyGameBigSeekState(&expected, position);
    const bool equal = game == position
        && std::memcmp(file, &expected, sizeof(expected)) == 0;
    if (equal) {
        state.matches.fetch_add(1, relaxed);
    } else {
        reportMismatch(
            "bigseek", state,
            std::string("position=") + std::to_string(position)
                + (reset ? " reset" : " logical"));
    }
    return game;
}

using BigCloseFunction = void(__thiscall*)(
    ra3::io::GameBigHandler*, ra3::io::GameBigFile*);

void __fastcall bigClose(
    ra3::io::GameBigHandler* handler, void*, ra3::io::GameBigFile* file)
{
    TargetState& state = g_bigClose;
    state.calls.fetch_add(1, relaxed);
    if (state.mode == Mode::shadow) {
        // The original releases `file`; there is no safe post-call object to compare.
        reinterpret_cast<BigCloseFunction>(state.original)(handler, file);
        state.matches.fetch_add(1, relaxed);
        return;
    }
    const ra3::io::GameBigReleaseOps operations{
        handler->allocator, &destroyGameInflater, &releaseGameAllocation};
    ra3::io::closeGameBigFile(file, operations);
    state.matches.fetch_add(1, relaxed);
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
         5, reinterpret_cast<void*>(&refPackDecode), &g_refPack},
        {"namehash", 0x00017DB0,
         {0x53, 0x55, 0x56, 0x57, 0x8B, 0x7C, 0x24, 0x14,
          0x85, 0xFF, 0x0F, 0x84, 0x1F, 0x01, 0x00, 0x00},
         8, reinterpret_cast<void*>(&nameHash), &g_nameHash},
        {"pathcompare", 0x005F3650,
         {0x56, 0x8B, 0x74, 0x24, 0x08, 0x57, 0x8B, 0x7C,
          0x24, 0x10, 0x8D, 0x9B, 0x00, 0x00, 0x00, 0x00},
         5, reinterpret_cast<void*>(&pathCompare), &g_pathCompare},
        {"stringcopylimit", 0x005F3470,
         {0x8B, 0x54, 0x24, 0x0C, 0x85, 0xD2, 0x8B, 0x44,
          0x24, 0x04, 0x57, 0x8B, 0xF8, 0x74, 0x2A, 0x56},
         6, reinterpret_cast<void*>(&copyStringLimited), &g_copyStringLimited},
        {"stringappendlimit", 0x005F34B0,
         {0x8B, 0x44, 0x24, 0x04, 0x56, 0x8B, 0x74, 0x24,
          0x10, 0x85, 0xF6, 0x57, 0x8B, 0xF8, 0x74, 0x30},
         5, reinterpret_cast<void*>(&appendStringLimited), &g_appendStringLimited},
        {"stringcomparelimit", 0x005F3500,
         {0x56, 0x8B, 0x74, 0x24, 0x10, 0x85, 0xF6, 0x75,
          0x04, 0x33, 0xC0, 0x5E, 0xC3, 0x8B, 0x4C, 0x24},
         5, reinterpret_cast<void*>(&compareStringsLimited), &g_compareStringsLimited},
        {"stringcomparecaselimit", 0x005F3560,
         {0x55, 0x8B, 0x6C, 0x24, 0x10, 0x85, 0xED, 0x75,
          0x04, 0x33, 0xC0, 0x5D, 0xC3, 0x56, 0x8B, 0x74},
         5, reinterpret_cast<void*>(&compareStringsCaseLimited), &g_compareStringsCaseLimited},
        {"stringcopy", 0x005F35E0,
         {0x8B, 0x44, 0x24, 0x04, 0x56, 0x8B, 0x74, 0x24,
          0x0C, 0x8A, 0x0E, 0x84, 0xC9, 0x8B, 0xD0, 0x74},
         5, reinterpret_cast<void*>(&copyString), &g_copyString},
        {"stringcompare", 0x005F3610,
         {0x8B, 0x4C, 0x24, 0x04, 0x8A, 0x01, 0x84, 0xC0,
          0x8B, 0x54, 0x24, 0x08, 0x74, 0x13, 0x8B, 0xFF},
         6, reinterpret_cast<void*>(&compareStrings), &g_compareStrings},
        {"stringlength", 0x005F36C0,
         {0x8B, 0x4C, 0x24, 0x04, 0x33, 0xC0, 0x38, 0x01,
          0x74, 0x11, 0x8D, 0x9B, 0x00, 0x00, 0x00, 0x00},
         6, reinterpret_cast<void*>(&stringLength), &g_stringLength},
        {"stringlowercopy", 0x005F36E0,
         {0x8B, 0x54, 0x24, 0x04, 0x56, 0x8B, 0x74, 0x24,
          0x0C, 0x8A, 0x0E, 0x84, 0xC9, 0x57, 0x8B, 0xFA},
         5, reinterpret_cast<void*>(&copyStringLowercase), &g_copyStringLowercase},
        {"stringfindlast", 0x005F3720,
         {0x8B, 0x4C, 0x24, 0x04, 0x80, 0x39, 0x00, 0x8A,
          0x54, 0x24, 0x08, 0x8B, 0xC1, 0x74, 0x1C, 0x90},
         7, reinterpret_cast<void*>(&findLastCharacter), &g_findLastCharacter},
        {"bytewiden", 0x005F3F10,
         {0x8B, 0x54, 0x24, 0x04, 0x56, 0x8B, 0x74, 0x24,
          0x0C, 0x8A, 0x0E, 0x84, 0xC9, 0x8B, 0xC2, 0x74},
         5, reinterpret_cast<void*>(&widenBytes), &g_widenBytes},
        {"bytecompare", 0x000C9EA0,
         {0x8B, 0x44, 0x24, 0x0C, 0x83, 0xF8, 0x04, 0x8B,
          0x4C, 0x24, 0x08, 0x8B, 0x54, 0x24, 0x04, 0x56},
         7, reinterpret_cast<void*>(&compareBytes), &g_compareBytes},
        {"prefixedccompare", 0x000CB3A0,
         {0x53, 0x8B, 0x5C, 0x24, 0x08, 0x85, 0xDB, 0x56,
          0x57, 0x74, 0x14, 0x8B, 0xC3, 0x8D, 0x70, 0x01},
         5, reinterpret_cast<void*>(&comparePrefixedCString), &g_comparePrefixedCString},
        {"prefixedcompare", 0x000CB400,
         {0x8B, 0x44, 0x24, 0x04, 0x8B, 0x00, 0x85, 0xC0,
          0x56, 0x57, 0x74, 0x06, 0x0F, 0xB7, 0x78, 0xFC},
         6, reinterpret_cast<void*>(&comparePrefixed), &g_comparePrefixed},
        {"fnvhash", 0x00628290,
         {0x8B, 0x54, 0x24, 0x04, 0x0F, 0xBE, 0x0A, 0x85,
          0xC9, 0xB8, 0xC5, 0x9D, 0x1C, 0x81, 0x74, 0x2A},
         7, reinterpret_cast<void*>(&hashLowercaseFnv1a), &g_hashLowercaseFnv1a},
        {"utf8decode", 0x006282D0,
         {0x8B, 0x4C, 0x24, 0x04, 0x8A, 0x01, 0x3C, 0x7F,
          0x77, 0x04, 0x0F, 0xB6, 0xC0, 0xC3, 0x8A, 0xD0},
         6, reinterpret_cast<void*>(&decodeUtf8), &g_decodeUtf8},
        {"utf8encode", 0x00628350,
         {0x8B, 0x44, 0x24, 0x08, 0x3D, 0x80, 0x00, 0x00,
          0x00, 0x8B, 0x4C, 0x24, 0x04, 0x7D, 0x03, 0x88},
         9, reinterpret_cast<void*>(&encodeUtf8), &g_encodeUtf8},
        {"utf8next", 0x006284B0,
         {0x8B, 0x4C, 0x24, 0x04, 0x8A, 0x01, 0x3C, 0x7F,
          0x77, 0x0D, 0x0F, 0xB6, 0xD0, 0x8D, 0x41, 0x01},
         6, reinterpret_cast<void*>(&decodeUtf8Advance), &g_decodeUtf8Advance},
        {"refpackprobe", 0x005F74F0,
         {0x8B, 0x54, 0x24, 0x04, 0x33, 0xC9, 0x8A, 0x2A,
          0x56, 0x33, 0xC0, 0x8A, 0x4A, 0x01, 0x0F, 0xB7},
         6, reinterpret_cast<void*>(&refPackProbe), &g_refPackProbe},
        {"bigreadbe32", 0x005F6810,
         {0x8B, 0x44, 0x24, 0x08, 0x83, 0xF8, 0x01, 0x75,
          0x08, 0x8B, 0x44, 0x24, 0x04, 0x0F, 0xB6, 0x00},
         7, reinterpret_cast<void*>(&bigReadBe32), &g_bigReadBe32},
        {"bigreadbe64", 0x005F6880,
         {0x53, 0x8B, 0x5C, 0x24, 0x0C, 0x56, 0x57, 0x33,
          0xF6, 0x33, 0xFF, 0x33, 0xC9, 0x85, 0xDB, 0x7E},
         5, reinterpret_cast<void*>(&bigReadBe64), &g_bigReadBe64},
        {"bigformat", 0x005F6910,
         {0x53, 0x55, 0x56, 0x8B, 0x74, 0x24, 0x10, 0x0F,
          0xB6, 0x4E, 0x01, 0x0F, 0xB6, 0x5E, 0x02, 0x57},
         7, reinterpret_cast<void*>(&bigDetectFormat), &g_bigDetectFormat},
        {"bighashpath", 0x005F6990,
         {0x53, 0x8B, 0x1D, 0xD4, 0xA4, 0xBB, 0x00, 0x56,
          0x57, 0x8B, 0x7C, 0x24, 0x10, 0x0F, 0xB6, 0x07},
         7, reinterpret_cast<void*>(&bigHashPath), &g_bigHashPath},
        {"bigheadersize", 0x005F6A50,
         {0x56, 0x8B, 0x74, 0x24, 0x08, 0x57, 0x56, 0x33,
          0xFF, 0xE8, 0xB2, 0xFE, 0xFF, 0xFF, 0x83, 0xC4},
         5, reinterpret_cast<void*>(&bigHeaderSize), &g_bigHeaderSize},
        {"bigentrycount", 0x005F6AA0,
         {0x56, 0x8B, 0xF1, 0x8B, 0x46, 0x04, 0x57, 0x50,
          0x33, 0xFF, 0xE8, 0x61, 0xFE, 0xFF, 0xFF, 0x83},
         6, reinterpret_cast<void*>(&bigEntryCount), &g_bigEntryCount, true},
        {"bigtrailersize", 0x005F6B00,
         {0x53, 0x56, 0x8B, 0x31, 0x56, 0xE8, 0x46, 0xFF,
          0xFF, 0xFF, 0x8A, 0x4C, 0x30, 0xF8, 0x8D, 0x44},
         5, reinterpret_cast<void*>(&bigTrailerSize), &g_bigTrailerSize, true},
        {"bigstoredsize", 0x005F5E80,
         {0x8B, 0x4C, 0x24, 0x04, 0x8B, 0x41, 0x20, 0x8B,
          0x51, 0x24, 0xC2, 0x04, 0x00, 0xCC, 0xCC, 0xCC},
         7, reinterpret_cast<void*>(&bigStoredSize), &g_bigStoredSize, true},
        {"bigabsoluteoffset", 0x005F5E90,
         {0x8B, 0x4C, 0x24, 0x04, 0x8B, 0x11, 0x8B, 0x42,
          0x20, 0x03, 0x41, 0x18, 0x8B, 0x52, 0x24, 0x13},
         6, reinterpret_cast<void*>(&bigAbsoluteOffset), &g_bigAbsoluteOffset, true},
        {"bigfindhash", 0x005F6B90,
         {0x83, 0xEC, 0x14, 0x53, 0x55, 0x56, 0x33, 0xF6,
          0x57, 0x8B, 0xE9, 0x89, 0x74, 0x24, 0x14, 0xE8},
         5, reinterpret_cast<void*>(&bigFindHash), &g_bigFindHash, true},
        {"bigfindentry", 0x005F6D00,
         {0x83, 0xEC, 0x10, 0x8B, 0x44, 0x24, 0x28, 0x53,
          0x55, 0x56, 0x8B, 0xE9, 0x83, 0xC9, 0xFF, 0x33},
         7, reinterpret_cast<void*>(&bigFindEntry), &g_bigFindEntry, true},
        {"gametexttable", 0x00139080,
         {0x8B, 0x11, 0x83, 0xEC, 0x08, 0x85, 0xD2, 0x74,
          0x3E, 0x83, 0x79, 0x08, 0x00, 0x74, 0x38, 0x8B},
         5, reinterpret_cast<void*>(&gameTextTable), &g_gameTextTable, true},
        {"gametextmanager", 0x0013B500,
         {0x56, 0x8B, 0xF1, 0x8B, 0x4E, 0x50, 0x33, 0xC0,
          0x85, 0xC9, 0x57, 0x8B, 0x7C, 0x24, 0x0C, 0x74},
         6, reinterpret_cast<void*>(&gameTextManager), &g_gameTextManager, true},
        {"bigseek", 0x005F5E00,
         {0x51, 0x53, 0x8B, 0x5C, 0x24, 0x14, 0x56, 0x8B,
          0x74, 0x24, 0x10, 0x57, 0x8B, 0x7C, 0x24, 0x18},
         6, reinterpret_cast<void*>(&bigSeek), &g_bigSeek, true},
        {"bigclose", 0x005F5DB0,
         {0x56, 0x8B, 0x74, 0x24, 0x08, 0x57, 0x8B, 0xF9,
          0x8B, 0x4E, 0x0C, 0x85, 0xC9, 0x74, 0x26, 0x8B},
         5, reinterpret_cast<void*>(&bigClose), &g_bigClose, true},
        {"bigfindclose", 0x005F5EB0,
         {0x8B, 0x44, 0x24, 0x04, 0x85, 0xC0, 0x74, 0x12,
          0x8B, 0x49, 0x1C, 0x8B, 0x11, 0x6A, 0x00, 0x50},
         6, reinterpret_cast<void*>(&bigFindClose), &g_bigFindClose, true},
        {"bigopen", 0x005F5F80,
         {0x81, 0xEC, 0xC4, 0x01, 0x00, 0x00, 0x53, 0x55,
          0x56, 0x8B, 0xB4, 0x24, 0xD4, 0x01, 0x00, 0x00},
         6, reinterpret_cast<void*>(&bigOpen), &g_bigOpen, true},
        {"bigread", 0x005F63B0,
         {0x83, 0xEC, 0x0C, 0x53, 0x55, 0x56, 0x8B, 0x74,
          0x24, 0x1C, 0x8B, 0x2E, 0x57, 0x83, 0xC5, 0x38},
         5, reinterpret_cast<void*>(&bigRead), &g_bigRead, true},
        {"bigfindfirst", 0x005F65E0,
         {0x53, 0x8B, 0x5C, 0x24, 0x08, 0x56, 0x57, 0x6A,
          0x3A, 0x53, 0x8B, 0xF9, 0xFF, 0x15, 0x20, 0xA5},
         5, reinterpret_cast<void*>(&bigFindFirst), &g_bigFindFirst, true},
        {"bigfindnext", 0x005F6670,
         {0x81, 0xEC, 0x08, 0x01, 0x00, 0x00, 0x53, 0x56,
          0x8B, 0xB4, 0x24, 0x14, 0x01, 0x00, 0x00, 0x33},
         6, reinterpret_cast<void*>(&bigFindNext), &g_bigFindNext, true},
    };
    return table;
}

}  // namespace ra3hook
