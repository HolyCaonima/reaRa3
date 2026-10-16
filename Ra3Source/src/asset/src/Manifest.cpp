#include <ra3/asset/Manifest.h>

#include <algorithm>
#include <iomanip>
#include <limits>
#include <sstream>

namespace ra3::asset {
namespace {

std::uint16_t readU16(const std::uint8_t* p)
{
    return static_cast<std::uint16_t>(p[0] | (static_cast<std::uint16_t>(p[1]) << 8));
}

std::uint32_t readU32(const std::uint8_t* p)
{
    return static_cast<std::uint32_t>(p[0]) |
           (static_cast<std::uint32_t>(p[1]) << 8) |
           (static_cast<std::uint32_t>(p[2]) << 16) |
           (static_cast<std::uint32_t>(p[3]) << 24);
}

bool add(std::uint64_t& value, std::uint64_t amount)
{
    if (amount > std::numeric_limits<std::uint64_t>::max() - value) {
        return false;
    }
    value += amount;
    return true;
}

bool hasString(const std::string& buffer, std::uint32_t offset)
{
    if (offset >= buffer.size()) {
        return false;
    }
    return buffer.find('\0', offset) != std::string::npos;
}

std::string_view stringAt(const std::string& buffer, std::uint32_t offset)
{
    if (!hasString(buffer, offset)) {
        return {};
    }
    return std::string_view(buffer.data() + offset);
}

}  // namespace

Result<Manifest> Manifest::parse(const std::uint8_t* data, std::size_t size)
{
    constexpr std::uint64_t kHeaderSize = 48;
    constexpr std::uint64_t kEntrySize = 48;
    if (size < kHeaderSize) {
        return makeError("manifest truncated at header offset " + std::to_string(size));
    }

    Manifest result;
    ManifestHeader& h = result.m_header;
    h.isBigEndian = data[0];
    h.isLinked = data[1];
    h.version = readU16(data + 2);
    const std::uint8_t* p = data + 4;
    std::uint32_t* fields[] = {
        &h.streamChecksum, &h.allTypesHash, &h.assetCount, &h.totalInstanceDataSize,
        &h.maxInstanceChunkSize, &h.maxRelocationChunkSize, &h.maxImportsChunkSize,
        &h.assetReferenceBufferSize, &h.referencedManifestBufferSize,
        &h.assetNameBufferSize, &h.sourceFileNameBufferSize,
    };
    for (std::uint32_t* field : fields) {
        *field = readU32(p);
        p += 4;
    }
    if (h.isBigEndian != 0) {
        return makeError("manifest uses unsupported big-endian layout");
    }
    if (h.version != 6) {
        return makeError("unsupported manifest version " + std::to_string(h.version));
    }

    std::uint64_t expected = kHeaderSize;
    if (!add(expected, static_cast<std::uint64_t>(h.assetCount) * kEntrySize) ||
        !add(expected, h.assetReferenceBufferSize) ||
        !add(expected, h.referencedManifestBufferSize) ||
        !add(expected, h.assetNameBufferSize) ||
        !add(expected, h.sourceFileNameBufferSize)) {
        return makeError("manifest size equation overflows");
    }
    if (expected != size) {
        return makeError("manifest size equation gives " + std::to_string(expected) +
                         " bytes, file has " + std::to_string(size));
    }
    if ((h.assetReferenceBufferSize % 8) != 0) {
        return makeError("asset reference buffer size is not a multiple of 8");
    }

    const std::size_t recordsOffset = static_cast<std::size_t>(kHeaderSize);
    const std::size_t referenceOffset =
        recordsOffset + static_cast<std::size_t>(h.assetCount) * static_cast<std::size_t>(kEntrySize);
    const std::size_t referencedManifestOffset = referenceOffset + h.assetReferenceBufferSize;
    const std::size_t namesOffset = referencedManifestOffset + h.referencedManifestBufferSize;
    const std::size_t sourcesOffset = namesOffset + h.assetNameBufferSize;

    result.m_names.assign(reinterpret_cast<const char*>(data + namesOffset), h.assetNameBufferSize);
    result.m_sourceFiles.assign(
        reinterpret_cast<const char*>(data + sourcesOffset), h.sourceFileNameBufferSize);

    result.m_references.reserve(h.assetReferenceBufferSize / 8);
    for (std::size_t offset = 0; offset < h.assetReferenceBufferSize; offset += 8) {
        const std::uint8_t* ref = data + referenceOffset + offset;
        result.m_references.push_back({readU32(ref), readU32(ref + 4)});
    }

    std::size_t referencedOffset = 0;
    while (referencedOffset < h.referencedManifestBufferSize) {
        const std::uint8_t kind = data[referencedManifestOffset + referencedOffset++];
        const std::size_t begin = referencedOffset;
        while (referencedOffset < h.referencedManifestBufferSize &&
               data[referencedManifestOffset + referencedOffset] != 0) {
            ++referencedOffset;
        }
        if (referencedOffset == h.referencedManifestBufferSize) {
            return makeError("referenced manifest name is not NUL-terminated at offset " +
                             std::to_string(begin - 1));
        }
        result.m_referencedManifests.push_back(
            {kind, std::string(reinterpret_cast<const char*>(data + referencedManifestOffset + begin),
                               referencedOffset - begin)});
        ++referencedOffset;
    }

    result.m_entries.reserve(h.assetCount);
    for (std::uint32_t i = 0; i < h.assetCount; ++i) {
        const std::size_t recordOffset = recordsOffset + static_cast<std::size_t>(i) * kEntrySize;
        const std::uint8_t* record = data + recordOffset;
        AssetEntry entry;
        entry.id.typeId = readU32(record);
        entry.id.instanceId = readU32(record + 4);
        entry.typeHash = readU32(record + 8);
        entry.instanceHash = readU32(record + 12);
        entry.assetReferenceOffset = readU32(record + 16);
        entry.assetReferenceCount = readU32(record + 20);
        entry.nameOffset = readU32(record + 24);
        entry.sourceFileNameOffset = readU32(record + 28);
        entry.instanceDataSize = readU32(record + 32);
        entry.relocationDataSize = readU32(record + 36);
        entry.importsDataSize = readU32(record + 40);
        entry.flags = readU32(record + 44);
        if (result.m_instanceDataSize > std::numeric_limits<std::uint64_t>::max() - 4 ||
            result.m_relocationDataSize > std::numeric_limits<std::uint64_t>::max() - 4 ||
            result.m_importsDataSize > std::numeric_limits<std::uint64_t>::max() - 4) {
            return makeError("asset member offset overflows at asset " + std::to_string(i));
        }
        entry.instanceDataOffset = 4 + result.m_instanceDataSize;
        entry.relocationDataOffset = 4 + result.m_relocationDataSize;
        entry.importsDataOffset = 4 + result.m_importsDataSize;

        const std::uint64_t referenceEnd =
            static_cast<std::uint64_t>(entry.assetReferenceOffset) +
            static_cast<std::uint64_t>(entry.assetReferenceCount) * 8;
        if ((entry.assetReferenceOffset % 8) != 0 || referenceEnd > h.assetReferenceBufferSize) {
            return makeError("asset " + std::to_string(i) +
                             " reference range is outside the reference buffer at record offset " +
                             std::to_string(recordOffset + 16));
        }
        if (!hasString(result.m_names, entry.nameOffset)) {
            return makeError("asset " + std::to_string(i) +
                             " name offset is outside a NUL-terminated string");
        }
        if (!hasString(result.m_sourceFiles, entry.sourceFileNameOffset)) {
            return makeError("asset " + std::to_string(i) +
                             " source-file offset is outside a NUL-terminated string");
        }
        if (!add(result.m_instanceDataSize, entry.instanceDataSize) ||
            !add(result.m_relocationDataSize, entry.relocationDataSize) ||
            !add(result.m_importsDataSize, entry.importsDataSize)) {
            return makeError("asset data-size sum overflows at asset " + std::to_string(i));
        }
        result.m_entries.push_back(entry);
    }
    return result;
}

std::string_view Manifest::name(std::size_t index) const
{
    return index < m_entries.size() ? stringAt(m_names, m_entries[index].nameOffset) : std::string_view{};
}

std::string_view Manifest::sourceFile(std::size_t index) const
{
    return index < m_entries.size()
               ? stringAt(m_sourceFiles, m_entries[index].sourceFileNameOffset)
               : std::string_view{};
}

std::vector<AssetId> Manifest::references(std::size_t index) const
{
    if (index >= m_entries.size()) {
        return {};
    }
    const AssetEntry& entry = m_entries[index];
    const std::size_t begin = entry.assetReferenceOffset / 8;
    return {m_references.begin() + begin,
            m_references.begin() + begin + entry.assetReferenceCount};
}

const AssetEntry* Manifest::find(AssetId id) const
{
    const auto found = std::find_if(
        m_entries.begin(), m_entries.end(), [id](const AssetEntry& entry) { return entry.id == id; });
    return found == m_entries.end() ? nullptr : &*found;
}

const AssetEntry* Manifest::findByName(std::string_view wanted) const
{
    for (std::size_t i = 0; i < m_entries.size(); ++i) {
        if (name(i) == wanted) {
            return &m_entries[i];
        }
    }
    return nullptr;
}

std::string cdataPath(std::string_view stem, const AssetEntry& entry)
{
    std::ostringstream out;
    out << stem << "/cdata/" << std::hex << std::setfill('0') << std::nouppercase
        << std::setw(8) << entry.id.typeId << "." << std::setw(8) << entry.typeHash << "."
        << std::setw(8) << entry.id.instanceId << "." << std::setw(8) << entry.instanceHash
        << ".cdata";
    return out.str();
}

}  // namespace ra3::asset
