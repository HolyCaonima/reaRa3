#include "support/TestHarness.h"

#include <ra3/core/SkuDef.h>

using ra3::SkuDef;

// Same layout as ra3_english_1.0.SkuDef, shortened.
constexpr const char* kEnglishSku =
    "set-exe Data\\ra3_1.0.game\r\n"
    "set-search-path big:;.\r\n"
    "add-big Data\\MapsCampaign.big\r\n"
    "add-big Data\\English.big\r\n"
    "add-big Data\\WBData.big\r\n";

RA3_TEST(SkuDef_ParsesDirectivesInOrder)
{
    auto sku = SkuDef::parse(kEnglishSku, "english");
    RA3_REQUIRE_OK(sku);
    RA3_CHECK_EQ(sku.value().exe, std::string("Data\\ra3_1.0.game"));
    RA3_CHECK_EQ(sku.value().searchPath, std::string("big:;."));
    RA3_REQUIRE(sku.value().bigFiles.size() == 3);
    RA3_CHECK_EQ(sku.value().bigFiles[0], std::string("Data\\MapsCampaign.big"));
    RA3_CHECK_EQ(sku.value().bigFiles[2], std::string("Data\\WBData.big"));
    RA3_CHECK(sku.value().unrecognized.empty());
}

RA3_TEST(SkuDef_KeepsUnrecognizedDirectives)
{
    auto sku = SkuDef::parse("set-exe a.game\nadd-patch p.big\n", "x");
    RA3_REQUIRE_OK(sku);
    RA3_REQUIRE(sku.value().unrecognized.size() == 1);
    RA3_CHECK_EQ(sku.value().unrecognized[0], std::string("add-patch p.big"));
}

RA3_TEST(SkuDef_RejectsMissingArgumentsAndRepeatedSingletons)
{
    RA3_CHECK(!SkuDef::parse("add-big\n", "x").ok());
    RA3_CHECK(!SkuDef::parse("set-exe a.game\nset-exe b.game\n", "x").ok());
}
