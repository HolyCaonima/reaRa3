#include <ra3/core/GameConfig.h>

#include <ra3/core/FileUtil.h>

#include <cstdlib>
#include <string>
#include <system_error>

#ifdef _WIN32
#include <windows.h>
#endif

namespace ra3 {
namespace {

std::optional<std::filesystem::path> environmentPath(const char* name)
{
#ifdef _WIN32
    const std::wstring wideName(name, name + std::char_traits<char>::length(name));
    const wchar_t* value = _wgetenv(wideName.c_str());
#else
    const char* value = std::getenv(name);
#endif
    if (value == nullptr || *value == 0) {
        return std::nullopt;
    }
    return std::filesystem::path(value);
}

std::optional<std::filesystem::path> executableDirectory()
{
#ifdef _WIN32
    std::wstring buffer(MAX_PATH, L'\0');
    for (;;) {
        const DWORD length = GetModuleFileNameW(nullptr, buffer.data(), static_cast<DWORD>(buffer.size()));
        if (length == 0) {
            return std::nullopt;
        }
        if (length < buffer.size()) {
            buffer.resize(length);
            return std::filesystem::path(buffer).parent_path();
        }
        buffer.resize(buffer.size() * 2);
    }
#else
    std::error_code error;
    const std::filesystem::path self = std::filesystem::read_symlink("/proc/self/exe", error);
    if (error) {
        return std::nullopt;
    }
    return self.parent_path();
#endif
}

bool isFile(const std::filesystem::path& path)
{
    std::error_code error;
    return std::filesystem::is_regular_file(path, error);
}

bool isDirectory(const std::filesystem::path& path)
{
    std::error_code error;
    return std::filesystem::is_directory(path, error);
}

std::filesystem::path makeAbsolute(const std::filesystem::path& path)
{
    std::error_code error;
    std::filesystem::path result = std::filesystem::absolute(path, error);
    return (error ? path : result).lexically_normal();
}

std::filesystem::path resolveAgainst(const std::filesystem::path& base, const std::string& value)
{
    std::filesystem::path path = utf8ToPath(value);
    if (path.is_relative()) {
        path = base / path;
    }
    return path.lexically_normal();
}

Result<std::filesystem::path> findConfigFile(const std::optional<std::filesystem::path>& explicitPath)
{
    if (explicitPath) {
        if (!isFile(*explicitPath)) {
            return makeError("configuration file not found: " + pathToUtf8(*explicitPath));
        }
        return *explicitPath;
    }
    if (auto fromEnvironment = environmentPath("RA3_CONFIG")) {
        if (!isFile(*fromEnvironment)) {
            return makeError("RA3_CONFIG names a missing file: " + pathToUtf8(*fromEnvironment));
        }
        return *fromEnvironment;
    }

    std::vector<std::filesystem::path> candidates;
    if (auto directory = executableDirectory()) {
        candidates.push_back(*directory / "ra3source.ini");
    }
    candidates.push_back(utf8ToPath(RA3_DEFAULT_CONFIG_PATH));
    for (const auto& candidate : candidates) {
        if (isFile(candidate)) {
            return candidate;
        }
    }
    std::string message = "no configuration file found; looked for:";
    for (const auto& candidate : candidates) {
        message += "\n  " + pathToUtf8(candidate);
    }
    return makeError(message);
}

}  // namespace

Result<GameConfig> resolveGameConfig(const IniFile& ini, const std::filesystem::path& baseDir)
{
    GameConfig config;
    const char* const keys[] = {"Root", "DataDir", "SkuDef"};
    std::string values[3];
    for (int i = 0; i < 3; ++i) {
        const auto value = ini.get("Game", keys[i]);
        if (!value || value->empty()) {
            return makeError(std::string("[Game] ") + keys[i] + " is not set");
        }
        values[i] = *value;
    }
    config.gameRoot = resolveAgainst(baseDir, values[0]);
    config.dataDir = resolveAgainst(config.gameRoot, values[1]);
    config.skuDefFile = resolveAgainst(config.gameRoot, values[2]);
    return config;
}

Result<GameConfig> loadGameConfig(const std::optional<std::filesystem::path>& explicitPath)
{
    auto found = findConfigFile(explicitPath);
    if (!found) {
        return found.error();
    }
    const std::filesystem::path configFile = makeAbsolute(found.value());
    auto ini = IniFile::load(configFile);
    if (!ini) {
        return ini.error();
    }

    std::optional<std::filesystem::path> localOverrideFile;
    std::filesystem::path localFile = configFile;
    localFile.replace_extension(".local.ini");
    if (isFile(localFile)) {
        auto overrides = IniFile::load(localFile);
        if (!overrides) {
            return overrides.error();
        }
        ini.value().merge(overrides.value());
        localOverrideFile = localFile;
    }

    bool rootFromEnvironment = false;
    if (auto root = environmentPath("RA3_GAME_ROOT")) {
        ini.value().set("Game", "Root", pathToUtf8(makeAbsolute(*root)));
        rootFromEnvironment = true;
    }

    auto config = resolveGameConfig(ini.value(), configFile.parent_path());
    if (!config) {
        return config.error();
    }
    config.value().configFile = configFile;
    config.value().localOverrideFile = localOverrideFile;
    config.value().rootFromEnvironment = rootFromEnvironment;
    return config;
}

std::vector<std::string> validateGameConfig(const GameConfig& config)
{
    std::vector<std::string> problems;
    if (!isDirectory(config.gameRoot)) {
        problems.push_back("game root is not a directory: " + pathToUtf8(config.gameRoot));
    }
    if (!isDirectory(config.dataDir)) {
        problems.push_back("data directory is not a directory: " + pathToUtf8(config.dataDir));
    }
    if (!isFile(config.skuDefFile)) {
        problems.push_back("SkuDef file not found: " + pathToUtf8(config.skuDefFile));
    }
    return problems;
}

}  // namespace ra3
