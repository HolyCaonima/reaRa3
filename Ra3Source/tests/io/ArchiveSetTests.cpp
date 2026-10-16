#include "support/BigBuilder.h"
#include "support/TempDir.h"
#include "support/TestHarness.h"

#include <ra3/io/ArchiveSet.h>

using ra3::io::ArchiveSet;
using ra3::test::buildBig;
using ra3::test::TempDir;

namespace {

std::string contentOf(const ArchiveSet::Location& location)
{
    auto bytes = location.archive->read(*location.entry);
    return bytes ? std::string(bytes.value().begin(), bytes.value().end()) : "<error>";
}

}  // namespace

RA3_TEST(ArchiveSet_FirstMountedArchiveWins)
{
    TempDir dir("archive-set");
    dir.write("Data/Patch.big", buildBig({{"data\\shared.ini", "patched"}, {"data\\new.ini", "new"}}));
    dir.write("Data/Base.big", buildBig({{"data\\shared.ini", "original"}, {"data\\base.ini", "base"}}));

    ra3::SkuDef sku;
    sku.bigFiles = {"Data\\Patch.big", "Data\\Base.big"};
    auto mounted = ArchiveSet::mount(dir.path(), sku);
    RA3_REQUIRE_OK(mounted);
    const ArchiveSet& set = mounted.value();

    const auto shared = set.find("DATA\\Shared.ini");
    RA3_REQUIRE(shared.has_value());
    RA3_CHECK_EQ(contentOf(*shared), std::string("patched"));

    const auto all = set.findAll("data\\shared.ini");
    RA3_REQUIRE(all.size() == 2);
    RA3_CHECK_EQ(contentOf(all[1]), std::string("original"));

    const auto base = set.find("data\\base.ini");
    RA3_REQUIRE(base.has_value());
    RA3_CHECK_EQ(contentOf(*base), std::string("base"));
    RA3_CHECK(!set.find("data\\missing.ini").has_value());

    const auto several = set.pathsInSeveralArchives();
    RA3_REQUIRE(several.size() == 1);
    RA3_CHECK_EQ(several.front(), std::string("data\\shared.ini"));
}

RA3_TEST(ArchiveSet_MountFailsOnMissingArchive)
{
    TempDir dir("archive-set-missing");
    ra3::SkuDef sku;
    sku.bigFiles = {"Data\\Missing.big"};
    RA3_CHECK(!ArchiveSet::mount(dir.path(), sku).ok());
}
