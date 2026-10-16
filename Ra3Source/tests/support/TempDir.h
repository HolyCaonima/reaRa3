#pragma once

#include <chrono>
#include <filesystem>
#include <fstream>
#include <string>
#include <string_view>
#include <system_error>

namespace ra3::test {

// A fresh directory under the system temp directory, removed on destruction.
class TempDir {
public:
    explicit TempDir(std::string_view name)
    {
        const auto stamp = std::chrono::steady_clock::now().time_since_epoch().count();
        m_path = std::filesystem::temp_directory_path() / "ra3source-tests" /
                 (std::string(name) + "-" + std::to_string(stamp));
        std::filesystem::create_directories(m_path);
    }
    ~TempDir()
    {
        std::error_code ignored;
        std::filesystem::remove_all(m_path, ignored);
    }
    TempDir(const TempDir&) = delete;
    TempDir& operator=(const TempDir&) = delete;

    const std::filesystem::path& path() const { return m_path; }

    std::filesystem::path write(const std::filesystem::path& relative, std::string_view content) const
    {
        const std::filesystem::path file = m_path / relative;
        std::filesystem::create_directories(file.parent_path());
        std::ofstream(file, std::ios::binary).write(content.data(), static_cast<std::streamsize>(content.size()));
        return file;
    }

private:
    std::filesystem::path m_path;
};

}  // namespace ra3::test
