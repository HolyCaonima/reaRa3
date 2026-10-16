// Checks against the real, read-only installation named by config/ra3source.ini.
// Every test skips when that installation is not available.

#include "support/TestHarness.h"

#include <ra3/core/GameConfig.h>
#include <ra3/core/SkuDef.h>
#include <ra3/io/ArchiveSet.h>
#include <ra3/io/RefPack.h>

#include <cstring>

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

}  // namespace

RA3_TEST(GameData_SkuDefNamesTheGameExecutableAndArchives)
{
    const Installation install = requireInstallation();
    RA3_CHECK(!install.sku.exe.empty());
    RA3_CHECK(!install.sku.bigFiles.empty());
    RA3_CHECK(install.sku.unrecognized.empty());
}

RA3_TEST(GameData_EveryMountedArchiveParses)
{
    const Installation install = requireInstallation();
    auto mounted = ra3::io::ArchiveSet::mount(install.config.gameRoot, install.sku);
    RA3_REQUIRE_OK(mounted);
    RA3_CHECK_EQ(mounted.value().archives().size(), install.sku.bigFiles.size());
    for (const ra3::io::BigArchive& archive : mounted.value().archives()) {
        RA3_CHECK(archive.magic() == "BIG4");
        RA3_CHECK(!archive.entries().empty());
        RA3_REQUIRE(archive.trailer().has_value());
        RA3_CHECK(std::memcmp(archive.trailer()->bytes.data(), "L280", 4) == 0);
    }
}

RA3_TEST(GameData_StaticStreamIsReadable)
{
    const Installation install = requireInstallation();
    auto mounted = ra3::io::ArchiveSet::mount(install.config.gameRoot, install.sku);
    RA3_REQUIRE_OK(mounted);
    const auto hit = mounted.value().find("data/static.bin");
    RA3_REQUIRE(hit.has_value());
    auto head = hit->archive->read(*hit->entry, 64);
    RA3_REQUIRE_OK(head);
    RA3_CHECK_EQ(head.value().size(), std::size_t{64});
}

RA3_TEST(GameData_GameStringsAreRefPackCompressedCsf)
{
    const Installation install = requireInstallation();
    auto mounted = ra3::io::ArchiveSet::mount(install.config.gameRoot, install.sku);
    RA3_REQUIRE_OK(mounted);
    const auto hit = mounted.value().find("data\\gamestrings.csf");
    RA3_REQUIRE(hit.has_value());
    auto packed = hit->archive->read(*hit->entry);
    RA3_REQUIRE_OK(packed);
    const auto header = ra3::io::readRefPackHeader(packed.value().data(), packed.value().size());
    RA3_REQUIRE(header.has_value());

    auto csf = ra3::io::decompressRefPack(packed.value().data(), packed.value().size());
    RA3_REQUIRE_OK(csf);
    RA3_CHECK_EQ(csf.value().size(), std::size_t{header->decompressedSize});
    RA3_REQUIRE(csf.value().size() >= 4);
    RA3_CHECK(std::memcmp(csf.value().data(), " FSC", 4) == 0);
}
