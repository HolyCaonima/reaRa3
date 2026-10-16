#pragma once

#include <ra3/core/Result.h>

#include <array>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <limits>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace ra3::io {

struct BigEntry {
    std::string path;  // as stored, e.g. "data\static.bin"
    std::uint32_t offset = 0;
    std::uint32_t size = 0;
};

// Optional last 8 bytes of the header. The game (ra3_1.0.game 0x009F6B00)
// recognises it by an ASCII letter followed by three digits and only uses it
// to shorten the entry table. Shipped archives end with "L280" 15 05 00 01
// or "L280" 15 05 00 00; the last four bytes are not read by the game's BIG
// code.
struct BigTrailer {
    std::array<std::uint8_t, 8> bytes{};
    int number = 0;  // the three digits, 280 for "L280"
};

// Index of a .big archive with 32-bit offsets ("BIG4", as shipped, or "BIGF"):
//
//   0x00  char[4]  magic
//   0x04  u32 LE   archive size
//   0x08  u32 BE   entry count
//   0x0C  u32 BE   header size: 16 + entry table [+ trailer]
//   0x10  entries  u32 BE offset, u32 BE size, NUL-terminated path
//
// rw::core::BigHandler in the game also takes "BIG<n>" with n-byte fields,
// "Viv4" and 0xC0FB archives (format check at 0x009F6910); those are not
// supported here. open() is stricter than the game, which checks neither the
// archive size field, duplicate paths nor where the table ends.
class BigArchive {
public:
    static Result<BigArchive> open(const std::filesystem::path& file);

    const std::filesystem::path& file() const { return m_file; }
    std::string_view magic() const { return {m_magic.data(), m_magic.size()}; }
    std::uint64_t fileSize() const { return m_fileSize; }
    std::uint32_t headerSize() const { return m_headerSize; }
    const std::optional<BigTrailer>& trailer() const { return m_trailer; }
    const std::vector<BigEntry>& entries() const { return m_entries; }

    // Lookup ignores ASCII case, like the game's comparison (0x009F3650).
    // Slash direction is significant; callers use the archive's '\' spelling.
    const BigEntry* find(std::string_view path) const;

    // Reads the entry's bytes, at most `maxBytes` of them.
    Result<std::vector<std::uint8_t>> read(const BigEntry& entry,
                                           std::size_t maxBytes = std::numeric_limits<std::size_t>::max()) const;

    // Reads exactly `count` bytes at an offset relative to the entry.
    Result<std::vector<std::uint8_t>> read(
        const BigEntry& entry, std::uint64_t offset, std::size_t count) const;

private:
    std::filesystem::path m_file;
    std::array<char, 4> m_magic{};
    std::uint64_t m_fileSize = 0;
    std::uint32_t m_headerSize = 0;
    std::optional<BigTrailer> m_trailer;
    std::vector<BigEntry> m_entries;
    std::unordered_map<std::string, std::size_t> m_lookup;
};

// Lower-case ASCII: the key BigArchive::find compares.
std::string normalizeArchivePath(std::string_view path);

// The game's trailer test, applied to the 8 bytes ending a header.
std::optional<BigTrailer> parseBigTrailer(const std::uint8_t* lastEightHeaderBytes);

}  // namespace ra3::io
