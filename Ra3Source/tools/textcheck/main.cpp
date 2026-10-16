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
#include <map>
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

std::string lowerAscii(std::string value)
{
    for (char& character : value) {
        if (character >= 'A' && character <= 'Z') {
            character = static_cast<char>(character + ('a' - 'A'));
        }
    }
    return value;
}

// Same queries as queries() in verify_game_text_lookup.py.
void appendQueries(const std::string& label, std::vector<std::string>& out)
{
    out.push_back(label);
    out.push_back(swapAsciiCase(label));
    out.push_back(label + "_");
    if (label.size() > 1) {
        out.push_back(label.substr(0, label.size() - 1));
    }
    const auto last = static_cast<unsigned char>(label.back());
    if (last < 0x7E) {
        out.push_back(label.substr(0, label.size() - 1)
                      + static_cast<char>(last + 1));
    }
}

// A game-layout GameStringTable over its own label storage.
class Table {
public:
    explicit Table(const std::vector<std::string>& labels)
        : labels_(labels), infos_(labels_.size()), lookup_(labels_.size())
    {
        for (std::size_t index = 0; index < labels_.size(); ++index) {
            infos_[index].label.data = labels_[index].c_str();
            infos_[index].text.data = nullptr;
            infos_[index].extra.data = nullptr;
            lookup_[index] = {&infos_[index].label, &infos_[index]};
            byKey_.emplace(lowerAscii(labels_[index]), &infos_[index].text);
        }
        std::sort(lookup_.begin(), lookup_.end(), [](const auto& left, const auto& right) {
            return compareLabels(left.label->data, right.label->data) < 0;
        });
        table = {static_cast<std::uint32_t>(lookup_.size()), infos_.data(), lookup_.data()};
    }
    Table(const Table&) = delete;
    Table& operator=(const Table&) = delete;

    const ra3::text::GameUnicodeString* expected(const std::string& query) const
    {
        const auto found = byKey_.find(lowerAscii(query));
        return found == byKey_.end() ? nullptr : found->second;
    }

    ra3::text::GameStringTable table{};

private:
    std::vector<std::string> labels_;
    std::vector<ra3::text::GameTextInfo> infos_;
    std::vector<ra3::text::GameTextLookupEntry> lookup_;
    std::map<std::string, const ra3::text::GameUnicodeString*> byKey_;
};

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

    std::vector<std::string> labels;
    labels.reserve(entries.size());
    for (const auto& entry : entries) {
        labels.push_back(entry.label);
    }
    // Same table split as verify_game_text_lookup.py.
    std::vector<std::string> overrideLabels;
    for (std::size_t index = 0; index < labels.size(); index += 5) {
        overrideLabels.push_back(labels[index]);
    }
    std::vector<std::string> mapLabels;
    for (std::size_t index = 0; index < labels.size(); index += 3) {
        mapLabels.push_back(labels[index]);
    }
    for (int index = 0; index < 50; ++index) {
        mapLabels.push_back("MAP:RA3SOURCE_ONLY_" + std::to_string(index));
    }
    const Table primary(labels);
    const Table overrides(overrideLabels);
    const Table map(mapLabels);

    ra3::text::GameTextManager primaryOnly{};
    primaryOnly.primary = &primary.table;
    ra3::text::GameTextManager layered{};
    layered.primary = &primary.table;
    layered.map = &map.table;
    layered.overrideTable = &overrides.table;

    std::vector<std::string> queries;
    for (const auto* source : {&mapLabels, &labels}) {
        for (const auto& label : *source) {
            appendQueries(label, queries);
        }
    }
    for (const char* extra : {"__RA3SOURCE_MISSING__", "gui:not-a-real-label", "", "\x7f"}) {
        queries.emplace_back(extra);
    }

    std::size_t mismatches = 0;
    std::size_t hits = 0;
    for (const auto& query : queries) {
        const auto* expected = primary.expected(query);
        hits += expected != nullptr;
        const ra3::text::GameUnicodeString* layeredExpected = overrides.expected(query);
        if (layeredExpected == nullptr) {
            layeredExpected = expected != nullptr ? expected : map.expected(query);
        }
        const auto offline = parsed.value().find(query);
        mismatches += ra3::text::findGameText(&primary.table, query.c_str()) != expected
            || ra3::text::findGameText(&primaryOnly, query.c_str()) != expected
            || ra3::text::findGameText(&layered, query.c_str()) != layeredExpected
            || (offline != nullptr) != (expected != nullptr);
    }

    std::cout << "gamestrings.csf: labels=" << entries.size() << " queries=" << queries.size()
              << " primary_hits=" << hits << " cases=" << queries.size() * 3
              << " mismatches=" << mismatches << '\n';
    return mismatches == 0 ? 0 : 1;
}
