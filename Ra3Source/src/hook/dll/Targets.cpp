#include "Targets.h"

#include <ra3/asset/NameHash.h>
#include <ra3/common/ByteString.h>
#include <ra3/io/GameBigLayout.h>
#include <ra3/io/PathCompare.h>
#include <ra3/io/RefPack.h>

#include <algorithm>
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
    };
    return table;
}

}  // namespace ra3hook
