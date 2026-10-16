#include <ra3/io/BigArchive.h>

#include <ra3/core/ByteOrder.h>
#include <ra3/core/FileUtil.h>
#include <ra3/core/StringUtil.h>
#include <ra3/io/ReadOnlyFile.h>

#include <algorithm>
#include <cstring>

namespace ra3::io {
namespace {

constexpr std::size_t kFixedHeaderSize = 16;
constexpr std::size_t kTrailerSize = 8;
constexpr std::size_t kMinimumEntrySize = 9;  // offset, size, empty path terminator

bool isAsciiLetter(std::uint8_t c)
{
    return (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z');
}

bool isAsciiDigit(std::uint8_t c)
{
    return c >= '0' && c <= '9';
}

}  // namespace

std::string normalizeArchivePath(std::string_view path)
{
    std::string result = lowerAscii(path);
    std::replace(result.begin(), result.end(), '/', '\\');
    return result;
}

std::optional<BigTrailer> parseBigTrailer(const std::uint8_t* lastEightHeaderBytes)
{
    const std::uint8_t* t = lastEightHeaderBytes;
    if (!isAsciiLetter(t[0]) || !isAsciiDigit(t[1]) || !isAsciiDigit(t[2]) || !isAsciiDigit(t[3])) {
        return std::nullopt;
    }
    BigTrailer trailer;
    std::copy_n(t, kTrailerSize, trailer.bytes.begin());
    trailer.number = (t[1] - '0') * 100 + (t[2] - '0') * 10 + (t[3] - '0');
    return trailer;
}

Result<BigArchive> BigArchive::open(const std::filesystem::path& file)
{
    auto opened = ReadOnlyFile::open(file);
    if (!opened) {
        return opened.error();
    }
    ReadOnlyFile& stream = opened.value();
    const auto fail = [&](const std::string& what) { return makeError(pathToUtf8(file) + ": " + what); };

    if (stream.size() < kFixedHeaderSize) {
        return fail("too small for a BIG header");
    }
    auto fixed = stream.read(0, kFixedHeaderSize);
    if (!fixed) {
        return fixed.error();
    }
    const std::uint8_t* h = fixed.value().data();
    if (std::memcmp(h, "BIG4", 4) != 0 && std::memcmp(h, "BIGF", 4) != 0) {
        return fail("unsupported magic '" + std::string(reinterpret_cast<const char*>(h), 4) +
                    "', expected BIG4 or BIGF");
    }
    const std::uint32_t declaredSize = readU32LE(h + 4);
    const std::uint32_t count = readU32BE(h + 8);
    const std::uint32_t headerSize = readU32BE(h + 12);
    if (declaredSize != stream.size()) {
        return fail("header declares " + std::to_string(declaredSize) + " bytes but the file has " +
                    std::to_string(stream.size()));
    }
    if (headerSize < kFixedHeaderSize || headerSize > stream.size()) {
        return fail("header size " + std::to_string(headerSize) + " is out of range");
    }

    auto header = stream.read(0, headerSize);
    if (!header) {
        return header.error();
    }
    const std::vector<std::uint8_t>& bytes = header.value();

    BigArchive archive;
    archive.m_file = file;
    std::copy_n(h, 4, archive.m_magic.begin());
    archive.m_fileSize = stream.size();
    archive.m_headerSize = headerSize;
    if (headerSize >= kFixedHeaderSize + kTrailerSize) {
        archive.m_trailer = parseBigTrailer(&bytes[headerSize - kTrailerSize]);
    }
    const std::size_t tableEnd = headerSize - (archive.m_trailer ? kTrailerSize : 0);
    if (count > (tableEnd - kFixedHeaderSize) / kMinimumEntrySize) {
        return fail("entry count " + std::to_string(count) + " does not fit in a " + std::to_string(headerSize) +
                    "-byte header");
    }
    archive.m_entries.reserve(count);
    archive.m_lookup.reserve(count);

    std::size_t pos = kFixedHeaderSize;
    for (std::uint32_t i = 0; i < count; ++i) {
        if (tableEnd - pos < kMinimumEntrySize) {
            return fail("entry table ends before entry " + std::to_string(i));
        }
        BigEntry entry;
        entry.offset = readU32BE(&bytes[pos]);
        entry.size = readU32BE(&bytes[pos + 4]);
        pos += 8;

        const char* name = reinterpret_cast<const char*>(&bytes[pos]);
        const void* terminator = std::memchr(name, 0, tableEnd - pos);
        if (terminator == nullptr) {
            return fail("path of entry " + std::to_string(i) + " is not terminated inside the entry table");
        }
        const std::size_t nameLength = static_cast<std::size_t>(static_cast<const char*>(terminator) - name);
        entry.path.assign(name, nameLength);
        pos += nameLength + 1;

        if (entry.offset < headerSize || entry.offset > stream.size() || entry.size > stream.size() - entry.offset) {
            return fail("entry '" + entry.path + "' lies outside the data area");
        }
        if (!archive.m_lookup.emplace(normalizeArchivePath(entry.path), archive.m_entries.size()).second) {
            return fail("duplicate entry '" + entry.path + "'");
        }
        archive.m_entries.push_back(std::move(entry));
    }
    if (pos != tableEnd) {
        return fail("entry table ends at " + std::to_string(pos) + " but the header size places it at " +
                    std::to_string(tableEnd));
    }
    return archive;
}

const BigEntry* BigArchive::find(std::string_view path) const
{
    const auto it = m_lookup.find(normalizeArchivePath(path));
    return it == m_lookup.end() ? nullptr : &m_entries[it->second];
}

Result<std::vector<std::uint8_t>> BigArchive::read(const BigEntry& entry, std::size_t maxBytes) const
{
    auto stream = ReadOnlyFile::open(m_file);
    if (!stream) {
        return stream.error();
    }
    return stream.value().read(entry.offset, std::min<std::size_t>(entry.size, maxBytes));
}

}  // namespace ra3::io
