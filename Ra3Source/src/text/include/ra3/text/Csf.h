#pragma once

#include <ra3/core/Result.h>

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace ra3::text {

struct CsfEntry {
    std::string label;
    std::string text;
    std::string extra;
};

class CsfStrings {
public:
    std::string_view magic() const { return " FSC"; }
    std::uint32_t version() const { return m_version; }
    std::uint32_t languageId() const { return m_languageId; }
    std::size_t labelCount() const { return m_entries.size(); }
    const std::vector<CsfEntry>& entries() const { return m_entries; }

    // CSF labels are compared using ASCII case folding, matching the game.
    const CsfEntry* find(std::string_view label) const;
    std::optional<std::string_view> get(std::string_view label) const;

private:
    friend Result<CsfStrings> parseCsf(const std::uint8_t*, std::size_t);

    std::uint32_t m_version = 0;
    std::uint32_t m_languageId = 0;
    std::vector<CsfEntry> m_entries;
    std::unordered_map<std::string, std::size_t> m_index;
};

Result<CsfStrings> parseCsf(const std::uint8_t* data, std::size_t size);
Result<CsfStrings> parseCsf(const std::vector<std::uint8_t>& data);

}  // namespace ra3::text
