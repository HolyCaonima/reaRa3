#pragma once

#include <ra3/asset/Manifest.h>
#include <ra3/io/ArchiveSet.h>

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

std::vector<std::string> enumerateStreamStems(const io::ArchiveSet& archives);
Result<StreamValidation> validateStreamBundle(
    const io::ArchiveSet& archives, std::string_view stem);

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
