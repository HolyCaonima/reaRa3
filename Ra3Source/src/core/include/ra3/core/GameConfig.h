#pragma once

#include <ra3/core/IniFile.h>
#include <ra3/core/Result.h>

#include <filesystem>
#include <optional>
#include <string>
#include <vector>

namespace ra3 {

// Where the read-only game installation lives, as configured in config/ra3source.ini.
struct GameConfig {
    std::filesystem::path configFile;
    std::optional<std::filesystem::path> localOverrideFile;
    bool rootFromEnvironment = false;

    std::filesystem::path gameRoot;
    std::filesystem::path dataDir;
    std::filesystem::path skuDefFile;
};

// Finds and reads the configuration. The first existing candidate wins:
// `explicitPath`, the RA3_CONFIG environment variable, ra3source.ini next to the
// running executable, then the build's default config/ra3source.ini.
// <stem>.local.ini beside the chosen file is merged over it, and the
// RA3_GAME_ROOT environment variable finally replaces [Game] Root.
Result<GameConfig> loadGameConfig(const std::optional<std::filesystem::path>& explicitPath = std::nullopt);

// Resolves [Game] Root against `baseDir`, then DataDir and SkuDef against Root.
Result<GameConfig> resolveGameConfig(const IniFile& ini, const std::filesystem::path& baseDir);

// Missing directories or files; empty when the configuration points at a usable installation.
std::vector<std::string> validateGameConfig(const GameConfig& config);

}  // namespace ra3
