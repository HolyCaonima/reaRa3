#pragma once

#include <ra3/core/Result.h>

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <vector>

namespace ra3::io {

// Random-access reader over a file that is never opened for writing.
class ReadOnlyFile {
public:
    static Result<ReadOnlyFile> open(const std::filesystem::path& path);

    const std::filesystem::path& path() const { return m_path; }
    std::uint64_t size() const { return m_size; }

    // Reads exactly `count` bytes starting at `offset`.
    Result<std::vector<std::uint8_t>> read(std::uint64_t offset, std::size_t count);

private:
    ReadOnlyFile(std::filesystem::path path, std::ifstream stream, std::uint64_t size);

    std::filesystem::path m_path;
    std::ifstream m_stream;
    std::uint64_t m_size = 0;
};

}  // namespace ra3::io
