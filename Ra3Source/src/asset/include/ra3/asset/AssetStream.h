#pragma once

#include <ra3/asset/Manifest.h>
#include <ra3/io/ArchiveSet.h>
#include <ra3/io/RefPack.h>

#include <array>
#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace ra3::asset {

struct ByteView {
    const std::uint8_t* data = nullptr;
    std::size_t size = 0;
};

struct StreamMemberInfo {
    std::string path;
    std::uint64_t storedSize = 0;
    std::uint64_t decodedSize = 0;
    bool refPackCompressed = false;
};

struct StreamValidation {
    std::string stem;
    Manifest manifest;
    std::array<StreamMemberInfo, 4> members;
    bool totalInstanceDataSizeMatches = false;
};

struct DeepStreamValidation {
    std::string stem;
    std::uint64_t assets = 0;
    std::uint64_t relocations = 0;
    std::uint64_t imports = 0;
    std::uint64_t assetsWithMoreReferencesThanImports = 0;
    std::uint64_t nonSequentialImportSites = 0;
    std::uint64_t relocationTargetsAtInstanceEnd = 0;
    std::uint64_t relocationTargetsPastInstanceEnd = 0;
    std::uint64_t largestInstance = 0;
    std::vector<std::string> problems;
};

// Each of .bin, .relo and .imp starts with the manifest's streamChecksum and is
// then exactly the concatenation of the assets' chunks, so its length is 4 plus
// the matching size sum. ra3_1.0.game (ResourceManagerFileStream, 0x004479E0)
// rejects a member whose checksum differs ("... file is not in synch with
// manifest!"). Validation decodes only those first 4 bytes, because a 32-bit
// process cannot hold every decompressed .bin at once.
std::vector<std::string> enumerateStreamStems(const io::ArchiveSet& archives);
Result<Manifest> readStreamManifest(
    const io::ArchiveSet& archives, std::string_view stem);
Result<StreamValidation> validateStreamBundle(
    const io::ArchiveSet& archives, std::string_view stem);
Result<DeepStreamValidation> validateStreamDeep(
    const io::ArchiveSet& archives, std::string_view stem);

Result<io::DecodedStream> openDecodedMember(
    const io::ArchiveSet& archives, std::string_view memberPath);

class AssetStream {
public:
    static Result<AssetStream> load(const io::ArchiveSet& archives, std::string_view stem);
    static Result<AssetStream> loadDecoded(
        std::string stem,
        std::vector<std::uint8_t> manifest,
        std::vector<std::uint8_t> instanceData,
        std::vector<std::uint8_t> relocationData,
        std::vector<std::uint8_t> importsData);

    const std::string& stem() const { return m_stem; }
    const Manifest& manifest() const { return m_manifest; }
    ByteView instanceData(std::size_t index) const;
    ByteView relocationData(std::size_t index) const;
    ByteView importsData(std::size_t index) const;
    ByteView instanceMember() const { return view(m_instanceData); }
    ByteView relocationMember() const { return view(m_relocationData); }
    ByteView importsMember() const { return view(m_importsData); }

private:
    static ByteView view(const std::vector<std::uint8_t>& bytes)
    {
        return {bytes.data(), bytes.size()};
    }
    static ByteView subview(
        const std::vector<std::uint8_t>& bytes, std::uint64_t offset, std::uint32_t size);

    std::string m_stem;
    Manifest m_manifest;
    std::vector<std::uint8_t> m_instanceData;
    std::vector<std::uint8_t> m_relocationData;
    std::vector<std::uint8_t> m_importsData;
};

}  // namespace ra3::asset
