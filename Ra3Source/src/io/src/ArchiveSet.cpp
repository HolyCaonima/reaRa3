#include <ra3/io/ArchiveSet.h>

#include <ra3/core/FileUtil.h>

#include <algorithm>
#include <unordered_map>

namespace ra3::io {

Result<ArchiveSet> ArchiveSet::mount(const std::filesystem::path& gameRoot, const SkuDef& sku)
{
    ArchiveSet set;
    set.m_archives.reserve(sku.bigFiles.size());
    for (std::string relative : sku.bigFiles) {
        std::replace(relative.begin(), relative.end(), '\\', '/');
        auto archive = BigArchive::open(gameRoot / utf8ToPath(relative));
        if (!archive) {
            return archive.error();
        }
        set.m_archives.push_back(std::move(archive).value());
    }
    return set;
}

std::optional<ArchiveSet::Location> ArchiveSet::find(std::string_view path) const
{
    std::string canonical(path);
    std::replace(canonical.begin(), canonical.end(), '/', '\\');
    for (const BigArchive& archive : m_archives) {
        if (const BigEntry* entry = archive.find(canonical)) {
            return Location{&archive, entry};
        }
    }
    return std::nullopt;
}

std::vector<ArchiveSet::Location> ArchiveSet::findAll(std::string_view path) const
{
    std::vector<Location> hits;
    std::string canonical(path);
    std::replace(canonical.begin(), canonical.end(), '/', '\\');
    for (const BigArchive& archive : m_archives) {
        if (const BigEntry* entry = archive.find(canonical)) {
            hits.push_back({&archive, entry});
        }
    }
    return hits;
}

std::vector<std::string> ArchiveSet::pathsInSeveralArchives() const
{
    std::unordered_map<std::string, int> occurrences;
    for (const BigArchive& archive : m_archives) {
        for (const BigEntry& entry : archive.entries()) {
            ++occurrences[normalizeArchivePath(entry.path)];
        }
    }
    std::vector<std::string> shared;
    for (const auto& [path, count] : occurrences) {
        if (count > 1) {
            shared.push_back(path);
        }
    }
    std::sort(shared.begin(), shared.end());
    return shared;
}

}  // namespace ra3::io
