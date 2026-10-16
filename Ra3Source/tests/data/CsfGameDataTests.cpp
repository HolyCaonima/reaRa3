#include "support/TestHarness.h"

#include <ra3/core/GameConfig.h>
#include <ra3/core/SkuDef.h>
#include <ra3/io/ArchiveSet.h>
#include <ra3/io/RefPack.h>
#include <ra3/text/Csf.h>

#include <algorithm>
#include <cstring>
#include <iostream>
#include <string>
#include <utility>

namespace {

struct Installation {
    ra3::GameConfig config;
    ra3::SkuDef sku;
};

Installation requireInstallation()
{
    auto config = ra3::loadGameConfig();
    if (!config) {
        RA3_SKIP(config.error().message);
    }
    const auto problems = ra3::validateGameConfig(config.value());
    if (!problems.empty()) {
        RA3_SKIP(problems.front());
    }
    auto sku = ra3::SkuDef::load(config.value().skuDefFile);
    RA3_REQUIRE_OK(sku);
    return {std::move(config).value(), std::move(sku).value()};
}

std::string printableSample(std::string text)
{
    std::replace(text.begin(), text.end(), '\r', ' ');
    std::replace(text.begin(), text.end(), '\n', ' ');
    if (text.size() > 120) {
        text.resize(120);
        text += "...";
    }
    return text;
}

}  // namespace

RA3_TEST(GameData_GameStringsCsfParses)
{
    const Installation install = requireInstallation();
    auto mounted = ra3::io::ArchiveSet::mount(install.config.gameRoot, install.sku);
    RA3_REQUIRE_OK(mounted);
    const auto hit = mounted.value().find("data\\gamestrings.csf");
    RA3_REQUIRE(hit.has_value());

    auto packed = hit->archive->read(*hit->entry);
    RA3_REQUIRE_OK(packed);
    auto unpacked = ra3::io::decompressRefPack(packed.value().data(), packed.value().size());
    RA3_REQUIRE_OK(unpacked);
    RA3_REQUIRE(unpacked.value().size() >= 4);
    RA3_CHECK(std::memcmp(unpacked.value().data(), " FSC", 4) == 0);

    auto parsed = ra3::text::parseCsf(unpacked.value());
    RA3_REQUIRE_OK(parsed);
    RA3_CHECK(parsed.value().magic() == " FSC");
    RA3_CHECK(parsed.value().labelCount() > 0);

    const ra3::text::CsfEntry* sample = nullptr;
    for (const ra3::text::CsfEntry& entry : parsed.value().entries()) {
        const auto value = parsed.value().get(entry.label);
        RA3_REQUIRE(value.has_value());
        if (!value->empty()) {
            sample = &entry;
            break;
        }
    }
    RA3_REQUIRE(sample != nullptr);
    std::cout << "[ DATA ] gamestrings.csf: " << parsed.value().labelCount() << " labels; "
              << sample->label << " -> " << printableSample(sample->text) << "\n";
}
