#include "support/TempDir.h"
#include "support/TestHarness.h"

#include <ra3/core/FileUtil.h>
#include <ra3/core/GameConfig.h>

#include <cstdlib>

using ra3::GameConfig;
using ra3::IniFile;
using ra3::test::TempDir;

RA3_TEST(GameConfig_ResolvesRootAgainstBaseAndOthersAgainstRoot)
{
    auto ini = IniFile::parse("[Game]\nRoot = ..\\Game\nDataDir = Data\nSkuDef = ra3_english_1.0.SkuDef\n", "t");
    RA3_REQUIRE_OK(ini);
    const std::filesystem::path base = std::filesystem::path("C:/Work/Ra3Source/config");
    auto config = ra3::resolveGameConfig(ini.value(), base);
    RA3_REQUIRE_OK(config);
    RA3_CHECK(config.value().gameRoot == std::filesystem::path("C:/Work/Ra3Source/Game").lexically_normal());
    RA3_CHECK(config.value().dataDir == config.value().gameRoot / "Data");
    RA3_CHECK(config.value().skuDefFile == config.value().gameRoot / "ra3_english_1.0.SkuDef");
}

RA3_TEST(GameConfig_KeepsAbsoluteDataDir)
{
    auto ini = IniFile::parse("[Game]\nRoot = C:\\Game\nDataDir = D:\\Elsewhere\\Data\nSkuDef = x.SkuDef\n", "t");
    RA3_REQUIRE_OK(ini);
    auto config = ra3::resolveGameConfig(ini.value(), "C:/base");
    RA3_REQUIRE_OK(config);
    RA3_CHECK(config.value().dataDir == std::filesystem::path("D:\\Elsewhere\\Data"));
}

RA3_TEST(GameConfig_RequiresEveryGameKey)
{
    for (const char* text : {"[Game]\nDataDir = Data\nSkuDef = x\n", "[Game]\nRoot = C:\\G\nSkuDef = x\n",
                             "[Game]\nRoot = C:\\G\nDataDir = Data\n"}) {
        auto ini = IniFile::parse(text, "t");
        RA3_REQUIRE_OK(ini);
        RA3_CHECK(!ra3::resolveGameConfig(ini.value(), "C:/base").ok());
    }
}

RA3_TEST(GameConfig_LoadsExplicitFileAndMergesLocalOverride)
{
    if (std::getenv("RA3_GAME_ROOT") != nullptr) {
        RA3_SKIP("RA3_GAME_ROOT is set and would override Root");
    }
    TempDir dir("config");
    dir.write("Game/Data/placeholder", "");
    dir.write("Game/test.SkuDef", "set-exe Data\\x.game\n");
    const auto configFile = dir.write("config/ra3source.ini",
                                      "[Game]\nRoot = ..\\Missing\nDataDir = Data\nSkuDef = test.SkuDef\n");
    dir.write("config/ra3source.local.ini", "[Game]\nRoot = ..\\Game\n");

    auto config = ra3::loadGameConfig(configFile);
    RA3_REQUIRE_OK(config);
    RA3_CHECK(config.value().localOverrideFile.has_value());
    RA3_CHECK(std::filesystem::equivalent(config.value().gameRoot, dir.path() / "Game"));
    RA3_CHECK(ra3::validateGameConfig(config.value()).empty());
}

RA3_TEST(GameConfig_ValidationReportsMissingInstallation)
{
    TempDir dir("missing");
    GameConfig config;
    config.gameRoot = dir.path() / "NoGame";
    config.dataDir = config.gameRoot / "Data";
    config.skuDefFile = config.gameRoot / "x.SkuDef";
    RA3_CHECK_EQ(ra3::validateGameConfig(config).size(), std::size_t{3});
}

RA3_TEST(GameConfig_MissingExplicitFileIsAnError)
{
    TempDir dir("absent");
    RA3_CHECK(!ra3::loadGameConfig(dir.path() / "nope.ini").ok());
}
