#pragma once

#include <ra3/core/Result.h>

#include <filesystem>
#include <map>
#include <optional>
#include <string>
#include <string_view>

namespace ra3 {

// Minimal INI reader: "[Section]" headers, "Key = Value" lines, and ';' or '#'
// comment lines. Section and key lookup is case-insensitive. Values are
// trimmed, and one pair of surrounding double quotes is removed.
class IniFile {
public:
    static Result<IniFile> parse(std::string_view text, std::string_view sourceName);
    static Result<IniFile> load(const std::filesystem::path& path);

    std::optional<std::string> get(std::string_view section, std::string_view key) const;
    void set(std::string_view section, std::string_view key, std::string value);

    // Copies every value of `overrides` into this file, replacing existing ones.
    void merge(const IniFile& overrides);

private:
    std::map<std::string, std::map<std::string, std::string>> m_sections;
};

}  // namespace ra3
