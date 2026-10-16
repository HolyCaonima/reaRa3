#pragma once

#include <ra3/asset/AssetId.h>
#include <ra3/core/Result.h>

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace ra3::asset {

// A compiled SAGE stream's .manifest: 48-byte header, assetCount 48-byte
// entries, then the reference, referenced-manifest, asset-name and
// source-file buffers, which end exactly at the end of the file. All 257
// shipped streams are version 6, little-endian and linked.
//
// ra3_1.0.game reads it in FullAssetList (0x00495890): it allocates
// count * 0x30 bytes of entries, sums their three chunk sizes and recursively
// loads referenced manifests of kind 2. Kind 1 names a dependency stream.
struct ManifestHeader {
    std::uint8_t isBigEndian = 0;
    std::uint8_t isLinked = 0;
    std::uint16_t version = 0;
    std::uint32_t streamChecksum = 0;
    std::uint32_t allTypesHash = 0;
    std::uint32_t assetCount = 0;
    std::uint32_t totalInstanceDataSize = 0;
    std::uint32_t maxInstanceChunkSize = 0;
    std::uint32_t maxRelocationChunkSize = 0;
    std::uint32_t maxImportsChunkSize = 0;
    std::uint32_t assetReferenceBufferSize = 0;
    std::uint32_t referencedManifestBufferSize = 0;
    std::uint32_t assetNameBufferSize = 0;
    std::uint32_t sourceFileNameBufferSize = 0;
};

// The three *Offset members are not stored in the file: each asset's chunk
// follows the previous asset's chunk, after the member's 4-byte checksum.
// `flags` is 1 for every GameObject, WeaponTemplate and ArmorTemplate and 0
// otherwise; its meaning is unknown.
struct AssetEntry {
    AssetId id;
    std::uint32_t typeHash = 0;
    std::uint32_t instanceHash = 0;
    std::uint32_t assetReferenceOffset = 0;
    std::uint32_t assetReferenceCount = 0;
    std::uint32_t nameOffset = 0;
    std::uint32_t sourceFileNameOffset = 0;
    std::uint32_t instanceDataSize = 0;
    std::uint32_t relocationDataSize = 0;
    std::uint32_t importsDataSize = 0;
    std::uint32_t flags = 0;
    std::uint64_t instanceDataOffset = 0;
    std::uint64_t relocationDataOffset = 0;
    std::uint64_t importsDataOffset = 0;
};

struct ReferencedManifest {
    std::uint8_t kind = 0;
    std::string name;
};

class Manifest {
public:
    static Result<Manifest> parse(const std::uint8_t* data, std::size_t size);
    static Result<Manifest> parse(const std::vector<std::uint8_t>& bytes)
    {
        return parse(bytes.data(), bytes.size());
    }

    const ManifestHeader& header() const { return m_header; }
    const std::vector<AssetEntry>& entries() const { return m_entries; }
    const std::vector<ReferencedManifest>& referencedManifests() const { return m_referencedManifests; }

    std::string_view name(std::size_t index) const;
    std::string_view sourceFile(std::size_t index) const;
    std::vector<AssetId> references(std::size_t index) const;
    const AssetEntry* find(AssetId id) const;
    const AssetEntry* findByName(std::string_view name) const;

    std::uint64_t instanceDataSize() const { return m_instanceDataSize; }
    std::uint64_t relocationDataSize() const { return m_relocationDataSize; }
    std::uint64_t importsDataSize() const { return m_importsDataSize; }

private:
    ManifestHeader m_header;
    std::vector<AssetEntry> m_entries;
    std::vector<AssetId> m_references;
    std::vector<ReferencedManifest> m_referencedManifests;
    std::string m_names;
    std::string m_sourceFiles;
    std::uint64_t m_instanceDataSize = 0;
    std::uint64_t m_relocationDataSize = 0;
    std::uint64_t m_importsDataSize = 0;
};

// "<stem>/cdata/typeId.typeHash.instanceId.instanceHash.cdata"; the game
// formats the same four values with "%s%08X.%08X.%08X.%08X.cdata" (0x004A64CC)
// and looks the result up ignoring case.
std::string cdataPath(std::string_view stem, const AssetEntry& entry);

}  // namespace ra3::asset
