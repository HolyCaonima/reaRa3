#include <ra3/core/GameConfig.h>
#include <ra3/core/SkuDef.h>
#include <ra3/io/ArchiveSet.h>
#include <ra3/io/RefPack.h>
#include <ra3/text/Csf.h>
#include <ra3/text/GameTextLayout.h>

#include <algorithm>
#include <cctype>
#include <cstddef>
#include <iostream>
#include <string>
#include <vector>

namespace {

int compareLabels(const char* left, const char* right)
{
    while (*left != '\0' && *right != '\0') {
        const auto a = static_cast<unsigned char>(
            std::tolower(static_cast<unsigned char>(*left)));
        const auto b = static_cast<unsigned char>(
            std::tolower(static_cast<unsigned char>(*right)));
        if (a != b) {
            return a < b ? -1 : 1;
        }
        ++left;
        ++right;
    }
    return static_cast<unsigned char>(*left) == static_cast<unsigned char>(*right)
        ? 0
        : (*left == '\0' ? -1 : 1);
}

std::string swapAsciiCase(std::string value)
{
    for (char& character : value) {
        if (character >= 'A' && character <= 'Z') {
            character = static_cast<char>(character + ('a' - 'A'));
        } else if (character >= 'a' && character <= 'z') {
            character = static_cast<char>(character - ('a' - 'A'));
        }
    }
    return value;
}

ra3::Result<ra3::text::CsfStrings> loadStrings()
{
    auto config = ra3::loadGameConfig();
    if (!config) {
        return config.error();
    }
    auto sku = ra3::SkuDef::load(config.value().skuDefFile);
    if (!sku) {
        return sku.error();
    }
    auto archives = ra3::io::ArchiveSet::mount(config.value().gameRoot, sku.value());
    if (!archives) {
        return archives.error();
    }
    const auto hit = archives.value().find("data\\gamestrings.csf");
    if (!hit) {
        return ra3::makeError("data\\gamestrings.csf was not found");
    }
    auto packed = hit->archive->read(*hit->entry);
    if (!packed) {
        return packed.error();
    }
    auto unpacked =
        ra3::io::decompressRefPack(packed.value().data(), packed.value().size());
    if (!unpacked) {
        return unpacked.error();
    }
    return ra3::text::parseCsf(unpacked.value());
}

}  // namespace

int main(int argc, char** argv)
{
    auto parsed = loadStrings();
    if (!parsed) {
        std::cerr << parsed.error().message << '\n';
        return 1;
    }
    const auto& entries = parsed.value().entries();
    if (argc == 2 && std::string(argv[1]) == "--labels") {
        for (const auto& entry : entries) {
            std::cout << entry.label << '\n';
        }
        return 0;
    }

    std::vector<ra3::text::GameTextInfo> infos(entries.size());
    std::vector<ra3::text::GameTextLookupEntry> lookup(entries.size());
    for (std::size_t index = 0; index < entries.size(); ++index) {
        infos[index].label.data = entries[index].label.c_str();
        infos[index].text.data = nullptr;
        infos[index].extra.data = nullptr;
        lookup[index] = {&infos[index].label, &infos[index]};
    }
    std::sort(lookup.begin(), lookup.end(), [](const auto& left, const auto& right) {
        return compareLabels(left.label->data, right.label->data) < 0;
    });
    ra3::text::GameStringTable table{
        static_cast<std::uint32_t>(lookup.size()), infos.data(), lookup.data()};

    std::size_t cases = 0;
    std::size_t mismatches = 0;
    for (const auto& item : lookup) {
        for (const std::string query :
             {std::string(item.label->data), swapAsciiCase(item.label->data)}) {
            const auto* found = ra3::text::findGameText(&table, query.c_str());
            const auto offline = parsed.value().find(query);
            ++cases;
            mismatches += found != &item.info->text
                || offline == nullptr
                || offline->label != item.info->label.data;
        }
    }
    for (const char* miss : {"__RA3SOURCE_MISSING__", "gui:not-a-real-label"}) {
        ++cases;
        mismatches += ra3::text::findGameText(&table, miss) != nullptr
            || parsed.value().find(miss) != nullptr;
    }

    std::cout << "gamestrings.csf: labels=" << entries.size()
              << " cases=" << cases << " mismatches=" << mismatches << '\n';
    return mismatches == 0 ? 0 : 1;
}
