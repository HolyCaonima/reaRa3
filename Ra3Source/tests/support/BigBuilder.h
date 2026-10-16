#pragma once

#include <ra3/core/ByteOrder.h>

#include <cstdint>
#include <cstring>
#include <string>
#include <utility>
#include <vector>

namespace ra3::test {

struct BigLayout {
    std::string magic = "BIG4";
    std::string trailer = std::string("L280\x15\x05\x00\x01", 8);  // empty: no trailer
};

// Builds an archive the way the shipped ones are laid out: header, entry
// table, optional trailer, then data aligned to 64 bytes.
inline std::string buildBig(const std::vector<std::pair<std::string, std::string>>& files,
                            const BigLayout& layout = {})
{
    std::size_t tableSize = 0;
    for (const auto& file : files) {
        tableSize += 8 + file.first.size() + 1;
    }
    const std::size_t headerSize = 16 + tableSize + layout.trailer.size();
    std::vector<std::uint32_t> offsets;
    std::size_t cursor = (headerSize + 63) / 64 * 64;
    for (const auto& file : files) {
        offsets.push_back(static_cast<std::uint32_t>(cursor));
        cursor = (cursor + file.second.size() + 63) / 64 * 64;
    }
    const std::size_t total = offsets.empty() ? headerSize : offsets.back() + files.back().second.size();

    std::vector<std::uint8_t> out(total, 0);
    std::memcpy(out.data(), layout.magic.data(), 4);
    writeU32LE(&out[4], static_cast<std::uint32_t>(total));
    writeU32BE(&out[8], static_cast<std::uint32_t>(files.size()));
    writeU32BE(&out[12], static_cast<std::uint32_t>(headerSize));
    std::size_t pos = 16;
    for (std::size_t i = 0; i < files.size(); ++i) {
        writeU32BE(&out[pos], offsets[i]);
        writeU32BE(&out[pos + 4], static_cast<std::uint32_t>(files[i].second.size()));
        std::memcpy(&out[pos + 8], files[i].first.c_str(), files[i].first.size() + 1);
        pos += 8 + files[i].first.size() + 1;
        std::memcpy(&out[offsets[i]], files[i].second.data(), files[i].second.size());
    }
    std::memcpy(&out[pos], layout.trailer.data(), layout.trailer.size());
    return std::string(out.begin(), out.end());
}

}  // namespace ra3::test
