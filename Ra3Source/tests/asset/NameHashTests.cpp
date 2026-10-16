#include "support/TestHarness.h"

#include <ra3/asset/NameHash.h>

#include <cstdint>
#include <string_view>

using ra3::asset::hashName;
using ra3::asset::instanceIdFromName;

RA3_TEST(nameHashMatchesShippedFontSubstitutionIds)
{
    RA3_CHECK_EQ(instanceIdFromName("Arial"), 0x23BAA623u);
    RA3_CHECK_EQ(instanceIdFromName("LucidaSans"), 0xD548386Bu);
    RA3_CHECK_EQ(instanceIdFromName("RedAlert"), 0x1E51591Fu);
}

RA3_TEST(typeIdIsCaseSensitiveNameHash)
{
    RA3_CHECK_EQ(ra3::asset::typeIdFromName("FontSubstitution"), 0x057786F9u);
    RA3_CHECK_EQ(ra3::asset::typeIdFromName("FXList"), 0x86682E78u);
}

RA3_TEST(nameHashFoldsAsciiCase)
{
    RA3_CHECK_EQ(instanceIdFromName("REDALERT"), instanceIdFromName("redalert"));
    RA3_CHECK_EQ(hashName("", 0, true), 0u);
    RA3_CHECK_EQ(hashName("", 7, false), 7u);
}

RA3_TEST(nameHashSignExtendsHighBytes)
{
    using namespace std::string_view_literals;
    RA3_CHECK_EQ(instanceIdFromName("\xE9"sv), 0x5F3E367Fu);
    RA3_CHECK_EQ(instanceIdFromName("A\xE9"sv), 0x23BE9083u);
    RA3_CHECK_EQ(instanceIdFromName("Caf\xE9!"sv), 0x95269002u);
}
