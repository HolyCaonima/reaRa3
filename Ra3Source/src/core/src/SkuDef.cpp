#include <ra3/core/SkuDef.h>

#include <ra3/core/FileUtil.h>
#include <ra3/core/StringUtil.h>

namespace ra3 {

Result<SkuDef> SkuDef::parse(std::string_view text, std::string_view sourceName)
{
    SkuDef sku;
    int lineNumber = 0;
    while (!text.empty()) {
        const std::string_view line = trim(takeLine(text));
        ++lineNumber;
        if (line.empty()) {
            continue;
        }

        const std::size_t space = line.find_first_of(" \t");
        const std::string_view directive = line.substr(0, space);
        const std::string_view argument = space == std::string_view::npos ? std::string_view{} : trim(line.substr(space));
        const auto where = [&] { return std::string(sourceName) + ":" + std::to_string(lineNumber) + ": "; };

        std::string* single = nullptr;
        if (directive == "set-exe") {
            single = &sku.exe;
        } else if (directive == "set-search-path") {
            single = &sku.searchPath;
        } else if (directive != "add-big") {
            sku.unrecognized.emplace_back(line);
            continue;
        }

        if (argument.empty()) {
            return makeError(where() + std::string(directive) + " needs an argument");
        }
        if (single == nullptr) {
            sku.bigFiles.emplace_back(argument);
        } else if (!single->empty()) {
            return makeError(where() + std::string(directive) + " appears more than once");
        } else {
            *single = std::string(argument);
        }
    }
    return sku;
}

Result<SkuDef> SkuDef::load(const std::filesystem::path& path)
{
    auto content = readWholeFile(path);
    if (!content) {
        return content.error();
    }
    return parse(content.value(), pathToUtf8(path));
}

}  // namespace ra3
