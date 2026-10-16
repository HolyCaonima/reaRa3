#include <ra3/io/ReadOnlyFile.h>

#include <ra3/core/FileUtil.h>

#include <system_error>

namespace ra3::io {

ReadOnlyFile::ReadOnlyFile(std::filesystem::path path, std::ifstream stream, std::uint64_t size)
    : m_path(std::move(path)), m_stream(std::move(stream)), m_size(size)
{
}

Result<ReadOnlyFile> ReadOnlyFile::open(const std::filesystem::path& path)
{
    std::error_code error;
    const std::uintmax_t size = std::filesystem::file_size(path, error);
    if (error) {
        return makeError("cannot stat " + pathToUtf8(path) + ": " + error.message());
    }
    std::ifstream stream(path, std::ios::binary);
    if (!stream) {
        return makeError("cannot open " + pathToUtf8(path));
    }
    return ReadOnlyFile(path, std::move(stream), size);
}

Result<std::vector<std::uint8_t>> ReadOnlyFile::read(std::uint64_t offset, std::size_t count)
{
    if (offset > m_size || count > m_size - offset) {
        return makeError("read of " + std::to_string(count) + " bytes at offset " + std::to_string(offset) +
                         " is past the end of " + pathToUtf8(m_path));
    }
    std::vector<std::uint8_t> buffer(count);
    m_stream.clear();
    m_stream.seekg(static_cast<std::streamoff>(offset));
    m_stream.read(reinterpret_cast<char*>(buffer.data()), static_cast<std::streamsize>(count));
    if (!m_stream || static_cast<std::size_t>(m_stream.gcount()) != count) {
        return makeError("short read at offset " + std::to_string(offset) + " in " + pathToUtf8(m_path));
    }
    return buffer;
}

}  // namespace ra3::io
