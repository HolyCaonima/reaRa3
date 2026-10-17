#include "../Targets.h"

#include <ra3/common/GameString.h>

#include <cstdint>
#include <cstring>
#include <vector>

namespace ra3hook {
namespace {

constexpr auto relaxed = std::memory_order_relaxed;

TargetState g_asciiFind;
TargetState g_unicodeFind;
TargetState g_wideCompare;
TargetState g_asciiSourceCopyRange;
TargetState g_unicodeSourceCopyRange;
TargetState g_asciiIsNone;

const char* __fastcall asciiFind(
    const ra3::common::GameAsciiString* self, void*, std::uint32_t character)
{
    auto& state = g_asciiFind;
    state.calls.fetch_add(1, relaxed);
    using Original = const char*(__thiscall*)(
        const ra3::common::GameAsciiString*, std::uint32_t);
    if (state.mode == Mode::replace) {
        return ra3::common::findGameCharacter(
            self, static_cast<char>(character));
    }
    const char* const game =
        reinterpret_cast<Original>(state.original)(self, character);
    const char* const ours = ra3::common::findGameCharacter(
        self, static_cast<char>(character));
    recordScalar(
        "stringbase2asciifind",
        state,
        static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(game)),
        static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(ours)));
    return game;
}

const char16_t* __fastcall unicodeFind(
    const ra3::common::GameUnicodeString* self, void*, std::uint32_t character)
{
    auto& state = g_unicodeFind;
    state.calls.fetch_add(1, relaxed);
    using Original = const char16_t*(__thiscall*)(
        const ra3::common::GameUnicodeString*, std::uint32_t);
    if (state.mode == Mode::replace) {
        return ra3::common::findGameCharacter(
            self, static_cast<char16_t>(character));
    }
    const char16_t* const game =
        reinterpret_cast<Original>(state.original)(self, character);
    const char16_t* const ours = ra3::common::findGameCharacter(
        self, static_cast<char16_t>(character));
    recordScalar(
        "stringbase2unicodefind",
        state,
        static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(game)),
        static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(ours)));
    return game;
}

int __cdecl wideCompare(
    const char16_t* left,
    int leftLength,
    const char16_t* right,
    int rightLength)
{
    auto& state = g_wideCompare;
    state.calls.fetch_add(1, relaxed);
    if (state.mode == Mode::replace) {
        return ra3::common::compareWide(
            left, leftLength, right, rightLength);
    }
    using Original = int(__cdecl*)(
        const char16_t*, int, const char16_t*, int);
    const int game = reinterpret_cast<Original>(state.original)(
        left, leftLength, right, rightLength);
    const int ours =
        ra3::common::compareWide(left, leftLength, right, rightLength);
    recordScalar(
        "stringbase2widecompare",
        state,
        static_cast<std::uint32_t>(game),
        static_cast<std::uint32_t>(ours));
    return game;
}

template <typename Char>
void* sourceCopyRange(
    const char* key,
    TargetState& state,
    const ra3::common::GameStringSource<Char>* source,
    void* destination,
    int offset,
    int count)
{
    state.calls.fetch_add(1, relaxed);
    if (state.mode == Mode::replace) {
        return ra3::common::copyGameStringSourceRange(
            source, destination, offset, count);
    }

    using Original = void*(__thiscall*)(
        const ra3::common::GameStringSource<Char>*, void*, int, int);
    if (count < 0) {
        state.skipped.fetch_add(1, relaxed);
        return reinterpret_cast<Original>(state.original)(
            source, destination, offset, count);
    }
    const std::size_t bytes =
        static_cast<std::size_t>(count) * sizeof(Char);
    std::vector<Char> sourceCopy;
    std::vector<Char> ours;
    try {
        sourceCopy.resize(static_cast<std::size_t>(count));
        ours.resize(static_cast<std::size_t>(count));
        if (bytes != 0) {
            std::memcpy(sourceCopy.data(), source->text + offset, bytes);
        }
    } catch (...) {
        state.skipped.fetch_add(1, relaxed);
        return reinterpret_cast<Original>(state.original)(
            source, destination, offset, count);
    }
    void* const game = reinterpret_cast<Original>(state.original)(
        source, destination, offset, count);
    const ra3::common::GameStringSource<Char> copy{
        source->vtable, sourceCopy.data(), count};
    ra3::common::copyGameStringSourceRange(
        &copy, ours.data(), 0, count);
    try {
        recordOutput(key, state, destination, ours.data(), bytes);
    } catch (...) {
        state.skipped.fetch_add(1, relaxed);
    }
    return game;
}

void* __fastcall asciiSourceCopyRange(
    const ra3::common::GameStringSource<char>* source,
    void*,
    void* destination,
    int offset,
    int count)
{
    return sourceCopyRange(
        "stringbase2asciisourcecopyrange",
        g_asciiSourceCopyRange,
        source,
        destination,
        offset,
        count);
}

void* __fastcall unicodeSourceCopyRange(
    const ra3::common::GameStringSource<char16_t>* source,
    void*,
    void* destination,
    int offset,
    int count)
{
    return sourceCopyRange(
        "stringbase2unicodesourcecopyrange",
        g_unicodeSourceCopyRange,
        source,
        destination,
        offset,
        count);
}

std::uint32_t __fastcall asciiIsNone(
    const ra3::common::GameAsciiString* self, void*)
{
    auto& state = g_asciiIsNone;
    state.calls.fetch_add(1, relaxed);
    using Original = std::uint32_t(__thiscall*)(
        const ra3::common::GameAsciiString*);
    if (state.mode == Mode::replace) {
        return ra3::common::isNoneGameString(self) ? 1u : 0u;
    }
    const std::uint32_t game =
        reinterpret_cast<Original>(state.original)(self);
    const std::uint32_t ours =
        ra3::common::isNoneGameString(self) ? 1u : 0u;
    recordScalar("stringbase2asciiisnone", state, game, ours);
    return game;
}

}  // namespace

void appendStringBase2Targets(std::vector<Target>& table)
{
    table.insert(table.end(), {
        {"stringbase2asciifind", 0x000CA050,
         {0x8B,0x09,0x85,0xC9,0x8B,0xC1,0x75,0x28,0xB8,0x2C,0x28,0xBD,0x00,0x33,0xC9,0x03},
         6, reinterpret_cast<void*>(&asciiFind), &g_asciiFind, true},
        {"stringbase2unicodefind", 0x000CA260,
         {0x8B,0x09,0x85,0xC9,0x8B,0xC1,0x75,0x29,0xB8,0xD0,0xAA,0xBD,0x00,0x33,0xC9,0x8D},
         6, reinterpret_cast<void*>(&unicodeFind), &g_unicodeFind, true},
        {"stringbase2widecompare", 0x000CA510,
         {0x53,0x8B,0x5C,0x24,0x14,0x56,0x57,0x8B,0x7C,0x24,0x14,0x3B,0xFB,0x8B,0xCF,0x7C},
         5, reinterpret_cast<void*>(&wideCompare), &g_wideCompare, true},
        {"stringbase2asciisourcecopyrange", 0x000CB020,
         {0x8B,0x49,0x04,0x8B,0x44,0x24,0x0C,0x03,0x4C,0x24,0x08,0x8B,0x54,0x24,0x04,0x50},
         7, reinterpret_cast<void*>(&asciiSourceCopyRange), &g_asciiSourceCopyRange, true},
        {"stringbase2unicodesourcecopyrange", 0x000CB0E0,
         {0x8B,0x44,0x24,0x0C,0x8D,0x14,0x00,0x8B,0x41,0x04,0x8B,0x4C,0x24,0x08,0x52,0x8D},
         7, reinterpret_cast<void*>(&unicodeSourceCopyRange), &g_unicodeSourceCopyRange, true},
        {"stringbase2asciiisnone", 0x000CB990,
         {0x68,0x80,0xAC,0xBD,0x00,0xE8,0xC6,0xFA,0xFF,0xFF,0xF7,0xD8,0x1B,0xC0,0x83,0xC0},
         5, reinterpret_cast<void*>(&asciiIsNone), &g_asciiIsNone, true},
    });
}

}  // namespace ra3hook
