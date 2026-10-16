#pragma once

#include <ra3/core/Result.h>

#include <filesystem>
#include <string>

namespace ra3 {

// Reads a whole file as raw bytes. The file is opened read-only.
Result<std::string> readWholeFile(const std::filesystem::path& path);

// UTF-8 rendering of a path, safe for any Windows file name.
std::string pathToUtf8(const std::filesystem::path& path);

// Interprets UTF-8 text (configuration values, command-line arguments) as a path.
std::filesystem::path utf8ToPath(const std::string& text);

}  // namespace ra3
