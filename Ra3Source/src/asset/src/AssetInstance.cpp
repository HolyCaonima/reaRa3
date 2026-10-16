#include <ra3/asset/AssetInstance.h>

#include <algorithm>
#include <limits>
#include <set>

namespace ra3::asset {
namespace {

std::uint32_t readLittleU32(const std::uint8_t* data)
{
    return static_cast<std::uint32_t>(data[0]) |
           (static_cast<std::uint32_t>(data[1]) << 8) |
           (static_cast<std::uint32_t>(data[2]) << 16) |
           (static_cast<std::uint32_t>(data[3]) << 24);
}

Error tableError(
    std::string_view member,
    std::uint64_t base,
    std::size_t offset,
    const std::string& message)
{
    return makeError(std::string(member) + ": " + message + " at offset " +
                     std::to_string(base + offset));
}

Result<std::vector<std::uint32_t>> parseSites(
    ByteView chunk,
    ByteView instance,
    std::string_view member,
    std::uint64_t memberOffset)
{
    std::vector<std::uint32_t> sites;
    if (chunk.size == 0) return sites;
    if ((chunk.size % 4) != 0) {
        return tableError(member, memberOffset, chunk.size, "chunk size is not a multiple of 4");
    }
    std::set<std::uint32_t> unique;
    for (std::size_t offset = 0; offset < chunk.size; offset += 4) {
        const std::uint32_t site = readLittleU32(chunk.data + offset);
        if (site == 0xFFFFFFFFu) {
            if (offset + 4 != chunk.size) {
                return tableError(member, memberOffset, offset, "terminator is not last");
            }
            return sites;
        }
        if ((site % 4) != 0) {
            return tableError(member, memberOffset, offset, "site " + std::to_string(site) +
                                                           " is not 4-byte aligned");
        }
        if (site > instance.size || instance.size - site < 4) {
            return tableError(member, memberOffset, offset, "site " + std::to_string(site) +
                                                           " is outside the instance");
        }
        if (!unique.insert(site).second) {
            return tableError(member, memberOffset, offset, "duplicate site " + std::to_string(site));
        }
        sites.push_back(site);
    }
    return tableError(member, memberOffset, chunk.size, "chunk has no terminator");
}

}  // namespace

Result<RelocationTable> RelocationTable::parse(
    ByteView chunk,
    ByteView instance,
    std::string_view memberName,
    std::uint64_t memberOffset)
{
    auto parsed = parseSites(chunk, instance, memberName, memberOffset);
    if (!parsed) return parsed.error();
    RelocationTable result;
    result.m_sites = std::move(parsed).value();
    for (std::size_t index = 0; index < result.m_sites.size(); ++index) {
        const std::uint32_t site = result.m_sites[index];
        const std::uint32_t target = readLittleU32(instance.data + site);
        if (target > instance.size) {
            return tableError(memberName, memberOffset, index * 4,
                              "relocation site " + std::to_string(site) + " targets " +
                                  std::to_string(target) + " outside the instance");
        }
    }
    return result;
}

bool RelocationTable::contains(std::uint32_t site) const
{
    return std::find(m_sites.begin(), m_sites.end(), site) != m_sites.end();
}

Result<ImportTable> ImportTable::parse(
    ByteView chunk,
    ByteView instance,
    const std::vector<AssetId>& references,
    std::string_view memberName,
    std::uint64_t memberOffset)
{
    auto sites = parseSites(chunk, instance, memberName, memberOffset);
    if (!sites) return sites.error();
    ImportTable result;
    std::set<std::uint32_t> usedIndices;
    for (std::size_t index = 0; index < sites.value().size(); ++index) {
        const std::uint32_t site = sites.value()[index];
        const std::uint32_t encoded = readLittleU32(instance.data + site);
        if (encoded == 0 || encoded > references.size()) {
            return tableError(memberName, memberOffset, index * 4,
                              "import site " + std::to_string(site) + " contains index " +
                                  std::to_string(encoded) + " outside 1.." +
                                  std::to_string(references.size()));
        }
        if (!usedIndices.insert(encoded).second) {
            return tableError(memberName, memberOffset, index * 4,
                              "reference index " + std::to_string(encoded) +
                                  " occurs at more than one import site");
        }
        result.m_imports.push_back({site, encoded, references[encoded - 1]});
    }
    return result;
}

const Import* ImportTable::find(std::uint32_t site) const
{
    const auto found = std::find_if(
        m_imports.begin(), m_imports.end(),
        [site](const Import& value) { return value.site == site; });
    return found == m_imports.end() ? nullptr : &*found;
}

Result<AssetInstance> AssetInstance::parse(
    ByteView instance,
    ByteView relocations,
    ByteView imports,
    const std::vector<AssetId>& references,
    std::string diagnosticName,
    std::uint64_t relocationMemberOffset,
    std::uint64_t importsMemberOffset)
{
    auto relocationTable = RelocationTable::parse(
        relocations, instance, diagnosticName + ".relo", relocationMemberOffset);
    if (!relocationTable) return relocationTable.error();
    auto importTable = ImportTable::parse(
        imports, instance, references, diagnosticName + ".imp", importsMemberOffset);
    if (!importTable) return importTable.error();
    AssetInstance result;
    result.m_instance = instance;
    result.m_diagnosticName = std::move(diagnosticName);
    result.m_relocations = std::move(relocationTable).value();
    result.m_imports = std::move(importTable).value();
    return result;
}

Result<std::uint32_t> AssetInstance::readU32(std::uint32_t offset) const
{
    if (offset > m_instance.size || m_instance.size - offset < 4) {
        return makeError(m_diagnosticName + ".bin: u32 field is outside the instance at offset " +
                         std::to_string(offset));
    }
    return readLittleU32(m_instance.data + offset);
}

std::optional<std::uint32_t> AssetInstance::relocationTarget(std::uint32_t site) const
{
    if (!m_relocations.contains(site)) return std::nullopt;
    auto value = readU32(site);
    return value ? std::optional<std::uint32_t>(value.value()) : std::nullopt;
}

const Import* AssetInstance::importAt(std::uint32_t site) const
{
    return m_imports.find(site);
}

Result<std::string_view> AssetInstance::readString(std::uint32_t descriptorOffset) const
{
    if (descriptorOffset > m_instance.size || m_instance.size - descriptorOffset < 8) {
        return makeError(m_diagnosticName +
                         ".bin: string descriptor is outside the instance at offset " +
                         std::to_string(descriptorOffset));
    }
    auto length = readU32(descriptorOffset);
    auto offset = readU32(descriptorOffset + 4);
    if (!length) return length.error();
    if (!offset) return offset.error();
    if (!m_relocations.contains(descriptorOffset + 4)) {
        return makeError(m_diagnosticName + ".bin: string offset field is not relocated at instance offset " +
                         std::to_string(descriptorOffset + 4));
    }
    if (offset.value() > m_instance.size ||
        length.value() > m_instance.size - offset.value()) {
        return makeError(m_diagnosticName + ".bin: string range is outside the instance at offset " +
                         std::to_string(descriptorOffset));
    }
    return std::string_view(
        reinterpret_cast<const char*>(m_instance.data + offset.value()), length.value());
}

}  // namespace ra3::asset
