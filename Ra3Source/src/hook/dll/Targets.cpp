#include "Targets.h"

#include <ra3/asset/NameHash.h>
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
    };
    return table;
}

}  // namespace ra3hook
