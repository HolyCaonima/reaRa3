#pragma once

#include <ra3/core/Result.h>
#include <ra3/core/SkuDef.h>
#include <ra3/io/BigArchive.h>

#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace ra3::io {

// The archives a SkuDef mounts, opened in SkuDef order.
//
// In ra3_1.0.game each "add-big" line is mounted as soon as it is parsed
// (directive handler 0x004D1B60 -> BigHandler::AddArchiveFromFile 0x009F5AF0,
// which appends to the handler's archive list) and opening a file walks that
// list from its head, stopping at the first archive holding the path
// (0x009F5F80). The archive listed first therefore wins.
class ArchiveSet {
public:
    struct Location {
        const BigArchive* archive = nullptr;
        const BigEntry* entry = nullptr;
    };

    // `sku` paths are resolved against `gameRoot`.
    static Result<ArchiveSet> mount(const std::filesystem::path& gameRoot, const SkuDef& sku);

    const std::vector<BigArchive>& archives() const { return m_archives; }

    // The copy the game opens: the first archive, in mount order, holding `path`.
    std::optional<Location> find(std::string_view path) const;

    // Every archive containing `path`, in mount order.
    std::vector<Location> findAll(std::string_view path) const;

    // Normalized paths that occur in more than one archive, sorted.
    std::vector<std::string> pathsInSeveralArchives() const;

private:
    std::vector<BigArchive> m_archives;
};

}  // namespace ra3::io
