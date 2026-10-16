#pragma once

#include <ra3/core/Result.h>

#include <filesystem>
#include <string>
#include <string_view>
#include <vector>

namespace ra3 {

// A launcher .SkuDef file from the game root, for example ra3_english_1.0.SkuDef:
//
//   set-exe Data\ra3_1.0.game
//   set-search-path big:;.
//   add-big Data\English.big
//
// Paths are kept exactly as written; they are relative to the game root.
//
// ra3_1.0.game's directive handler (0x004D1B60) matches prefixes ignoring case
// and also knows add-bigs, add-bigs-recurse, add-search-path, add-config and
// try-add-config; such lines end up in `unrecognized`. set-exe is not among
// the game's directives, so it is presumably consumed by the launcher.
struct SkuDef {
    std::string exe;
    std::string searchPath;
    std::vector<std::string> bigFiles;      // in file order, which is mount order
    std::vector<std::string> unrecognized;  // lines whose directive is none of the above

    static Result<SkuDef> parse(std::string_view text, std::string_view sourceName);
    static Result<SkuDef> load(const std::filesystem::path& path);
};

}  // namespace ra3
