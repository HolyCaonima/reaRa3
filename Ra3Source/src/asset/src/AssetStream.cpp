#include <ra3/asset/AssetStream.h>

#include <ra3/io/RefPack.h>

#include <algorithm>
#include <array>
#include <limits>
#include <set>

namespace ra3::asset {
namespace {

constexpr std::array<const char*, 4> kExtensions = {".manifest", ".bin", ".relo", ".imp"};

std::uint32_t readU32(const std::uint8_t* p)
{
    return static_cast<std::uint32_t>(p[0]) |
           (static_cast<std::uint32_t>(p[1]) << 8) |
           (static_cast<std::uint32_t>(p[2]) << 16) |
           (static_cast<std::uint32_t>(p[3]) << 24);
}

std::string memberPath(std::string_view stem, const char* extension)
{
    return std::string(stem) + extension;
}

Error memberError(std::string_view stem, const std::string& path, const std::string& message)
{
    return makeError(std::string(stem) + ": " + path + ": " + message);
}

Result<std::vector<std::uint8_t>> readDecoded(
    const io::ArchiveSet& archives, std::string_view stem, const char* extension)
{
    const std::string path = memberPath(stem, extension);
    const auto location = archives.find(path);
    if (!location) {
        return memberError(stem, path, "member not found");
    }
    auto stored = location->archive->read(*location->entry);
    if (!stored) {
        return memberError(stem, path, stored.error().message);
    }
    const auto header = io::readRefPackHeader(stored.value().data(), stored.value().size());
    if (!header) {
        return std::move(stored).value();
    }
    auto decoded = io::decompressRefPack(stored.value().data(), stored.value().size());
    if (!decoded) {
        return memberError(stem, path, decoded.error().message);
    }
    return std::move(decoded).value();
}

Result<StreamMemberInfo> inspectMember(
    const io::ArchiveSet& archives,
    std::string_view stem,
    const char* extension,
    std::uint32_t checksum,
    std::uint64_t expectedSize,
    bool checkChecksum)
{
    StreamMemberInfo info;
    info.path = memberPath(stem, extension);
    const auto location = archives.find(info.path);
    if (!location) {
        return memberError(stem, info.path, "member not found");
    }
    info.storedSize = location->entry->size;
    auto prefix = location->archive->read(*location->entry, 64);
    if (!prefix) {
        return memberError(stem, info.path, prefix.error().message);
    }
    const auto header = io::readRefPackHeader(prefix.value().data(), prefix.value().size());
    std::vector<std::uint8_t> decodedPrefix;
    if (header) {
        info.refPackCompressed = true;
        info.decodedSize = header->decompressedSize;
        auto decoded = io::decompressRefPackPrefix(prefix.value().data(), prefix.value().size(), 4);
        if (!decoded) {
            return memberError(stem, info.path, decoded.error().message);
        }
        decodedPrefix = std::move(decoded).value();
    } else {
        info.decodedSize = location->entry->size;
        decodedPrefix = std::move(prefix).value();
        if (decodedPrefix.size() > 4) {
            decodedPrefix.resize(4);
        }
    }
    if (expectedSize != std::numeric_limits<std::uint64_t>::max() &&
        info.decodedSize != expectedSize) {
        return memberError(stem, info.path,
                           "decoded length " + std::to_string(info.decodedSize) +
                               " does not equal expected " + std::to_string(expectedSize));
    }
    if (checkChecksum) {
        if (decodedPrefix.size() < 4) {
            return memberError(stem, info.path, "truncated checksum at offset 0");
        }
        const std::uint32_t actual = readU32(decodedPrefix.data());
        if (actual != checksum) {
            return memberError(stem, info.path,
                               "checksum mismatch at offset 0 (expected " +
                                   std::to_string(checksum) + ", got " +
                                   std::to_string(actual) + ")");
        }
    }
    return info;
}

Result<Manifest> parseManifest(
    std::string_view stem, const std::vector<std::uint8_t>& manifestBytes)
{
    auto manifest = Manifest::parse(manifestBytes);
    if (!manifest) {
        return makeError(std::string(stem) + ": " + std::string(stem) +
                         ".manifest: " + manifest.error().message);
    }
    return std::move(manifest).value();
}

Result<std::uint64_t> memberExpected(std::uint64_t payload)
{
    if (payload > std::numeric_limits<std::uint64_t>::max() - 4) {
        return makeError("member length overflows");
    }
    return payload + 4;
}

}  // namespace

std::vector<std::string> enumerateStreamStems(const io::ArchiveSet& archives)
{
    std::set<std::string> stems;
    constexpr std::string_view suffix = ".manifest";
    for (const io::BigArchive& archive : archives.archives()) {
        for (const io::BigEntry& entry : archive.entries()) {
            std::string path = io::normalizeArchivePath(entry.path);
            if (path.size() > suffix.size() &&
                path.compare(path.size() - suffix.size(), suffix.size(), suffix) == 0) {
                path.resize(path.size() - suffix.size());
                std::replace(path.begin(), path.end(), '\\', '/');
                stems.insert(std::move(path));
            }
        }
    }
    return {stems.begin(), stems.end()};
}

Result<StreamValidation> validateStreamBundle(
    const io::ArchiveSet& archives, std::string_view stem)
{
    auto manifestBytes = readDecoded(archives, stem, ".manifest");
    if (!manifestBytes) {
        return manifestBytes.error();
    }
    auto manifestResult = parseManifest(stem, manifestBytes.value());
    if (!manifestResult) {
        return manifestResult.error();
    }
    Manifest manifest = std::move(manifestResult).value();

    auto instanceExpected = memberExpected(manifest.instanceDataSize());
    auto relocationExpected = memberExpected(manifest.relocationDataSize());
    auto importsExpected = memberExpected(manifest.importsDataSize());
    if (!instanceExpected || !relocationExpected || !importsExpected) {
        return makeError(std::string(stem) + ": member length overflows");
    }

    StreamValidation validation;
    validation.stem = std::string(stem);
    validation.totalInstanceDataSizeMatches =
        manifest.header().totalInstanceDataSize == manifest.instanceDataSize();
    auto manifestInfo = inspectMember(
        archives, stem, ".manifest", 0, manifestBytes.value().size(), false);
    auto instanceInfo = inspectMember(
        archives, stem, ".bin", manifest.header().streamChecksum, instanceExpected.value(), true);
    auto relocationInfo = inspectMember(
        archives, stem, ".relo", manifest.header().streamChecksum, relocationExpected.value(), true);
    auto importsInfo = inspectMember(
        archives, stem, ".imp", manifest.header().streamChecksum, importsExpected.value(), true);
    if (!manifestInfo) return manifestInfo.error();
    if (!instanceInfo) return instanceInfo.error();
    if (!relocationInfo) return relocationInfo.error();
    if (!importsInfo) return importsInfo.error();
    validation.members = {
        std::move(manifestInfo).value(),
        std::move(instanceInfo).value(),
        std::move(relocationInfo).value(),
        std::move(importsInfo).value(),
    };
    validation.manifest = std::move(manifest);
    return validation;
}

Result<AssetStream> AssetStream::load(const io::ArchiveSet& archives, std::string_view stem)
{
    auto manifest = readDecoded(archives, stem, ".manifest");
    auto instance = readDecoded(archives, stem, ".bin");
    auto relocation = readDecoded(archives, stem, ".relo");
    auto imports = readDecoded(archives, stem, ".imp");
    if (!manifest) return manifest.error();
    if (!instance) return instance.error();
    if (!relocation) return relocation.error();
    if (!imports) return imports.error();
    return loadDecoded(std::string(stem), std::move(manifest).value(),
                       std::move(instance).value(), std::move(relocation).value(),
                       std::move(imports).value());
}

Result<AssetStream> AssetStream::loadDecoded(
    std::string stem,
    std::vector<std::uint8_t> manifestBytes,
    std::vector<std::uint8_t> instanceData,
    std::vector<std::uint8_t> relocationData,
    std::vector<std::uint8_t> importsData)
{
    auto parsed = parseManifest(stem, manifestBytes);
    if (!parsed) {
        return parsed.error();
    }
    Manifest manifest = std::move(parsed).value();
    const std::array<std::pair<const char*, const std::vector<std::uint8_t>*>, 3> members = {{
        {".bin", &instanceData},
        {".relo", &relocationData},
        {".imp", &importsData},
    }};
    const std::array<std::uint64_t, 3> payloadSizes = {
        manifest.instanceDataSize(), manifest.relocationDataSize(), manifest.importsDataSize()};
    for (std::size_t i = 0; i < members.size(); ++i) {
        const auto& bytes = *members[i].second;
        const std::string path = stem + members[i].first;
        auto expected = memberExpected(payloadSizes[i]);
        if (!expected) {
            return memberError(stem, path, expected.error().message);
        }
        if (bytes.size() != expected.value()) {
            return memberError(stem, path,
                               "decoded length " + std::to_string(bytes.size()) +
                                   " does not equal expected " + std::to_string(expected.value()));
        }
        if (bytes.size() < 4) {
            return memberError(stem, path, "truncated checksum at offset 0");
        }
        const std::uint32_t actual = readU32(bytes.data());
        if (actual != manifest.header().streamChecksum) {
            return memberError(stem, path, "checksum mismatch at offset 0");
        }
    }

    AssetStream stream;
    stream.m_stem = std::move(stem);
    stream.m_manifest = std::move(manifest);
    stream.m_instanceData = std::move(instanceData);
    stream.m_relocationData = std::move(relocationData);
    stream.m_importsData = std::move(importsData);
    return stream;
}

ByteView AssetStream::subview(
    const std::vector<std::uint8_t>& bytes, std::uint64_t offset, std::uint32_t size)
{
    if (offset > bytes.size() || size > bytes.size() - static_cast<std::size_t>(offset)) {
        return {};
    }
    return {bytes.data() + static_cast<std::size_t>(offset), size};
}

ByteView AssetStream::instanceData(std::size_t index) const
{
    if (index >= m_manifest.entries().size()) return {};
    const AssetEntry& entry = m_manifest.entries()[index];
    return subview(m_instanceData, entry.instanceDataOffset, entry.instanceDataSize);
}

ByteView AssetStream::relocationData(std::size_t index) const
{
    if (index >= m_manifest.entries().size()) return {};
    const AssetEntry& entry = m_manifest.entries()[index];
    return subview(m_relocationData, entry.relocationDataOffset, entry.relocationDataSize);
}

ByteView AssetStream::importsData(std::size_t index) const
{
    if (index >= m_manifest.entries().size()) return {};
    const AssetEntry& entry = m_manifest.entries()[index];
    return subview(m_importsData, entry.importsDataOffset, entry.importsDataSize);
}

}  // namespace ra3::asset
