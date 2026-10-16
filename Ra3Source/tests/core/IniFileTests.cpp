#include "support/TestHarness.h"

#include <ra3/core/IniFile.h>

using ra3::IniFile;

RA3_TEST(IniFile_LooksUpSectionsAndKeysIgnoringCase)
{
    auto ini = IniFile::parse("[Game]\nRoot = D:\\Game\\Ra3\\Game\n", "test");
    RA3_REQUIRE_OK(ini);
    RA3_CHECK_EQ(ini.value().get("game", "ROOT").value_or(""), std::string("D:\\Game\\Ra3\\Game"));
    RA3_CHECK(!ini.value().get("Game", "Missing"));
    RA3_CHECK(!ini.value().get("Other", "Root"));
}

RA3_TEST(IniFile_SkipsCommentsBlankLinesBomAndCarriageReturns)
{
    auto ini = IniFile::parse("\xEF\xBB\xBF; comment\r\n\r\n# another\r\n[A]\r\nKey=Value\r\n", "test");
    RA3_REQUIRE_OK(ini);
    RA3_CHECK_EQ(ini.value().get("A", "Key").value_or(""), std::string("Value"));
}

RA3_TEST(IniFile_TrimsValuesAndRemovesOnePairOfQuotes)
{
    auto ini = IniFile::parse("[A]\nPlain =   spaced value  \nQuoted = \" kept spaces \"\n", "test");
    RA3_REQUIRE_OK(ini);
    RA3_CHECK_EQ(ini.value().get("A", "Plain").value_or(""), std::string("spaced value"));
    RA3_CHECK_EQ(ini.value().get("A", "Quoted").value_or(""), std::string(" kept spaces "));
}

RA3_TEST(IniFile_ReportsMalformedLinesWithTheirLineNumber)
{
    auto ini = IniFile::parse("[A]\nKey = 1\nnot a pair\n", "sample.ini");
    RA3_REQUIRE(!ini.ok());
    RA3_CHECK(ini.error().message.find("sample.ini:3:") != std::string::npos);

    RA3_CHECK(!IniFile::parse("[Unterminated\n", "x").ok());
    RA3_CHECK(!IniFile::parse("[A]\n= value\n", "x").ok());
}

RA3_TEST(IniFile_MergeReplacesAndAddsValues)
{
    auto base = IniFile::parse("[Game]\nRoot = C:\\A\nSkuDef = a.SkuDef\n", "base");
    auto overrides = IniFile::parse("[game]\nroot = C:\\B\nDataDir = Data\n", "local");
    RA3_REQUIRE_OK(base);
    RA3_REQUIRE_OK(overrides);
    base.value().merge(overrides.value());
    RA3_CHECK_EQ(base.value().get("Game", "Root").value_or(""), std::string("C:\\B"));
    RA3_CHECK_EQ(base.value().get("Game", "SkuDef").value_or(""), std::string("a.SkuDef"));
    RA3_CHECK_EQ(base.value().get("Game", "DataDir").value_or(""), std::string("Data"));
}
