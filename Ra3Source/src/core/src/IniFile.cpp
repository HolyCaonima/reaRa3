#include <ra3/core/IniFile.h>

#include <ra3/core/FileUtil.h>
#include <ra3/core/StringUtil.h>

namespace ra3 {

Result<IniFile> IniFile::parse(std::string_view text, std::string_view sourceName)
{
    constexpr std::string_view utf8Bom = "\xEF\xBB\xBF";
    if (text.substr(0, utf8Bom.size()) == utf8Bom) {
        text.remove_prefix(utf8Bom.size());
    }

    IniFile ini;
    std::string section;
    int lineNumber = 0;
    while (!text.empty()) {
        const std::string_view line = trim(takeLine(text));
        ++lineNumber;

        if (line.empty() || line.front() == ';' || line.front() == '#') {
            continue;
        }
        const auto where = [&] { return std::string(sourceName) + ":" + std::to_string(lineNumber) + ": "; };
        if (line.front() == '[') {
            if (line.back() != ']') {
                return makeError(where() + "unterminated section header");
            }
            section = lowerAscii(trim(line.substr(1, line.size() - 2)));
            continue;
        }
        const std::size_t equals = line.find('=');
        if (equals == std::string_view::npos) {
            return makeError(where() + "expected 'Key = Value' or '[Section]'");
        }
        const std::string_view key = trim(line.substr(0, equals));
        if (key.empty()) {
            return makeError(where() + "empty key");
        }
        std::string_view value = trim(line.substr(equals + 1));
        if (value.size() >= 2 && value.front() == '"' && value.back() == '"') {
            value = value.substr(1, value.size() - 2);
        }
        ini.m_sections[section][lowerAscii(key)] = std::string(value);
    }
    return ini;
}

Result<IniFile> IniFile::load(const std::filesystem::path& path)
{
    auto content = readWholeFile(path);
    if (!content) {
        return content.error();
    }
    return parse(content.value(), pathToUtf8(path));
}

std::optional<std::string> IniFile::get(std::string_view section, std::string_view key) const
{
    const auto s = m_sections.find(lowerAscii(section));
    if (s == m_sections.end()) {
        return std::nullopt;
    }
    const auto k = s->second.find(lowerAscii(key));
    if (k == s->second.end()) {
        return std::nullopt;
    }
    return k->second;
}

void IniFile::set(std::string_view section, std::string_view key, std::string value)
{
    m_sections[lowerAscii(section)][lowerAscii(key)] = std::move(value);
}

void IniFile::merge(const IniFile& overrides)
{
    for (const auto& [section, values] : overrides.m_sections) {
        for (const auto& [key, value] : values) {
            m_sections[section][key] = value;
        }
    }
}

}  // namespace ra3
