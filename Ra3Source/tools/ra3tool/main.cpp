// ra3tool: inspects the configured, read-only RA3 installation.

#include <ra3/core/FileUtil.h>
#include <ra3/core/GameConfig.h>
#include <ra3/core/SkuDef.h>
#include <ra3/core/StringUtil.h>
#include <ra3/io/ArchiveSet.h>
#include <ra3/io/BigArchive.h>
#include <ra3/io/RefPack.h>

#include <algorithm>
#include <cstdlib>
#include <iomanip>
#include <iostream>
#include <map>
#include <optional>
#include <sstream>
#include <string>
#include <vector>

#ifdef _WIN32
#include <windows.h>
#endif

namespace {

constexpr const char* kUsage = R"(usage: ra3tool [--config FILE] <command> [arguments]

commands:
  paths                       show the resolved configuration and check the installation
  sku                         show the SkuDef in use and the archives it mounts
  big <archive> [--limit N]   list the entries of one archive (a SkuDef name such as Misc.big, or a path)
  mount                       open every archive of the SkuDef and summarise them
  find <path>                 show which mounted archives contain <path> and which copy the game uses
  unpack-check <archive>      decompress every RefPack entry of one archive and report the results
)";

std::string hexBytes(const std::uint8_t* data, std::size_t count)
{
    std::ostringstream out;
    for (std::size_t i = 0; i < count; ++i) {
        out << (i ? " " : "") << std::hex << std::setw(2) << std::setfill('0') << int(data[i]);
    }
    return out.str();
}

std::string printable(const std::uint8_t* data, std::size_t count)
{
    std::string text;
    for (std::size_t i = 0; i < count; ++i) {
        text += (data[i] >= 0x20 && data[i] < 0x7F) ? static_cast<char>(data[i]) : '.';
    }
    return text;
}

std::string describeTrailer(const ra3::io::BigArchive& big)
{
    const auto& trailer = big.trailer();
    if (!trailer) {
        return "(none)";
    }
    return hexBytes(trailer->bytes.data(), trailer->bytes.size()) + "  \"" +
           printable(trailer->bytes.data(), trailer->bytes.size()) + "\"";
}

int fail(const std::string& message)
{
    std::cerr << "error: " << message << "\n";
    return 1;
}

struct Context {
    ra3::GameConfig config;
    ra3::SkuDef sku;
};

ra3::Result<Context> loadContext(const std::optional<std::filesystem::path>& configPath)
{
    auto config = ra3::loadGameConfig(configPath);
    if (!config) {
        return config.error();
    }
    const auto problems = ra3::validateGameConfig(config.value());
    if (!problems.empty()) {
        std::string message = "the configured installation is not usable:";
        for (const auto& problem : problems) {
            message += "\n  " + problem;
        }
        return ra3::makeError(message);
    }
    auto sku = ra3::SkuDef::load(config.value().skuDefFile);
    if (!sku) {
        return sku.error();
    }
    return Context{std::move(config).value(), std::move(sku).value()};
}

int commandPaths(const std::optional<std::filesystem::path>& configPath)
{
    auto config = ra3::loadGameConfig(configPath);
    if (!config) {
        return fail(config.error().message);
    }
    const ra3::GameConfig& c = config.value();
    std::cout << "config file : " << ra3::pathToUtf8(c.configFile) << "\n"
              << "local file  : " << (c.localOverrideFile ? ra3::pathToUtf8(*c.localOverrideFile) : "(none)") << "\n"
              << "game root   : " << ra3::pathToUtf8(c.gameRoot)
              << (c.rootFromEnvironment ? "  (from RA3_GAME_ROOT)" : "") << "\n"
              << "data dir    : " << ra3::pathToUtf8(c.dataDir) << "\n"
              << "SkuDef      : " << ra3::pathToUtf8(c.skuDefFile) << "\n";
    const auto problems = ra3::validateGameConfig(c);
    for (const auto& problem : problems) {
        std::cout << "problem     : " << problem << "\n";
    }
    std::cout << "status      : " << (problems.empty() ? "ok" : "unusable") << "\n";
    return problems.empty() ? 0 : 1;
}

int commandSku(const Context& context)
{
    const ra3::SkuDef& sku = context.sku;
    std::cout << "SkuDef      : " << ra3::pathToUtf8(context.config.skuDefFile) << "\n"
              << "exe         : " << sku.exe << "\n"
              << "search path : " << sku.searchPath << "\n"
              << "archives    : " << sku.bigFiles.size() << "\n";
    for (const std::string& big : sku.bigFiles) {
        std::error_code error;
        const auto size = std::filesystem::file_size(context.config.gameRoot / ra3::utf8ToPath(big), error);
        std::cout << "  " << std::left << std::setw(28) << big << std::right;
        if (error) {
            std::cout << "missing\n";
        } else {
            std::cout << std::setw(12) << size << " bytes\n";
        }
    }
    for (const std::string& line : sku.unrecognized) {
        std::cout << "unrecognized: " << line << "\n";
    }
    return 0;
}

std::filesystem::path resolveArchive(const Context& context, const std::string& argument)
{
    for (const std::string& big : context.sku.bigFiles) {
        const std::filesystem::path relative = ra3::utf8ToPath(big);
        if (ra3::equalsIgnoreCaseAscii(ra3::pathToUtf8(relative.filename()), argument) ||
            ra3::equalsIgnoreCaseAscii(big, argument)) {
            return context.config.gameRoot / relative;
        }
    }
    const std::filesystem::path given = ra3::utf8ToPath(argument);
    std::error_code error;
    if (std::filesystem::exists(given, error)) {
        return given;
    }
    return context.config.dataDir / given;
}

int commandBig(const Context& context, const std::string& argument, std::size_t limit)
{
    auto archive = ra3::io::BigArchive::open(resolveArchive(context, argument));
    if (!archive) {
        return fail(archive.error().message);
    }
    const ra3::io::BigArchive& big = archive.value();
    std::cout << "archive     : " << ra3::pathToUtf8(big.file()) << "\n"
              << "magic       : " << big.magic() << "\n"
              << "file size   : " << big.fileSize() << "\n"
              << "entries     : " << big.entries().size() << "\n"
              << "header size : " << big.headerSize() << "\n"
              << "trailer     : " << describeTrailer(big) << "\n\n"
              << "    offset        size  path\n";
    std::size_t shown = 0;
    for (const ra3::io::BigEntry& entry : big.entries()) {
        if (shown++ == limit) {
            std::cout << "... " << (big.entries().size() - limit) << " more (use --limit)\n";
            break;
        }
        std::cout << std::setw(10) << entry.offset << "  " << std::setw(10) << entry.size << "  " << entry.path
                  << "\n";
    }
    return 0;
}

ra3::Result<ra3::io::ArchiveSet> mountAll(const Context& context)
{
    return ra3::io::ArchiveSet::mount(context.config.gameRoot, context.sku);
}

int commandMount(const Context& context)
{
    auto mounted = mountAll(context);
    if (!mounted) {
        return fail(mounted.error().message);
    }
    std::size_t totalEntries = 0;
    std::uint64_t totalBytes = 0;
    std::cout << "archive                      entries   header        bytes  trailer\n";
    for (const ra3::io::BigArchive& big : mounted.value().archives()) {
        totalEntries += big.entries().size();
        totalBytes += big.fileSize();
        std::cout << std::left << std::setw(26) << ra3::pathToUtf8(big.file().filename()) << std::right
                  << std::setw(10) << big.entries().size() << std::setw(9) << big.headerSize() << std::setw(13)
                  << big.fileSize() << "  " << describeTrailer(big) << "\n";
    }
    std::cout << "total: " << mounted.value().archives().size() << " archives, " << totalEntries << " entries, "
              << totalBytes << " bytes\n";

    const auto shared = mounted.value().pathsInSeveralArchives();
    std::cout << "paths present in more than one archive (the first listed archive wins): " << shared.size()
              << "\n";
    for (std::size_t i = 0; i < shared.size() && i < 20; ++i) {
        std::cout << "  " << shared[i] << "\n";
    }
    return 0;
}

int commandFind(const Context& context, const std::string& path)
{
    auto mounted = mountAll(context);
    if (!mounted) {
        return fail(mounted.error().message);
    }
    const auto hits = mounted.value().findAll(path);
    if (hits.empty()) {
        std::cout << "not found: " << path << "\n";
        return 1;
    }
    for (std::size_t i = 0; i < hits.size(); ++i) {
        const auto& hit = hits[i];
        auto head = hit.archive->read(*hit.entry, 16);
        std::cout << (i == 0 ? "used     " : "shadowed ") << ra3::pathToUtf8(hit.archive->file().filename())
                  << "  offset=" << hit.entry->offset << "  size=" << hit.entry->size << "  path=" << hit.entry->path;
        if (head) {
            std::cout << "  head=" << hexBytes(head.value().data(), head.value().size());
        }
        std::cout << "\n";
    }
    return 0;
}

int commandUnpackCheck(const Context& context, const std::string& argument)
{
    auto archive = ra3::io::BigArchive::open(resolveArchive(context, argument));
    if (!archive) {
        return fail(archive.error().message);
    }
    const ra3::io::BigArchive& big = archive.value();
    std::size_t packed = 0;
    std::size_t plain = 0;
    std::size_t failed = 0;
    std::uint64_t packedBytes = 0;
    std::uint64_t unpackedBytes = 0;
    std::map<std::string, std::size_t> plainHeads;
    for (const ra3::io::BigEntry& entry : big.entries()) {
        auto head = big.read(entry, 8);
        if (!head) {
            return fail(head.error().message);
        }
        if (!ra3::io::readRefPackHeader(head.value().data(), head.value().size())) {
            ++plain;
            ++plainHeads[hexBytes(head.value().data(), std::min<std::size_t>(head.value().size(), 4))];
            continue;
        }
        ++packed;
        auto bytes = big.read(entry);
        if (!bytes) {
            return fail(bytes.error().message);
        }
        auto unpacked = ra3::io::decompressRefPack(bytes.value().data(), bytes.value().size());
        if (!unpacked) {
            if (++failed <= 10) {
                std::cout << "FAILED " << entry.path << ": " << unpacked.error().message << "\n";
            }
            continue;
        }
        packedBytes += entry.size;
        unpackedBytes += unpacked.value().size();
    }
    std::cout << "archive     : " << ra3::pathToUtf8(big.file().filename()) << "\n"
              << "entries     : " << big.entries().size() << "\n"
              << "refpack     : " << packed << " (" << failed << " failed), " << packedBytes << " -> "
              << unpackedBytes << " bytes\n"
              << "not refpack : " << plain << "\n";
    std::vector<std::pair<std::size_t, std::string>> byCount;
    for (const auto& [head, count] : plainHeads) {
        byCount.emplace_back(count, head);
    }
    std::sort(byCount.begin(), byCount.end(), [](const auto& a, const auto& b) { return a.first > b.first; });
    constexpr std::size_t kShown = 8;
    for (std::size_t i = 0; i < byCount.size() && i < kShown; ++i) {
        std::cout << "  first bytes " << byCount[i].second << " : " << byCount[i].first << "\n";
    }
    if (byCount.size() > kShown) {
        std::cout << "  ... " << (byCount.size() - kShown) << " other distinct first-byte patterns\n";
    }
    return failed == 0 ? 0 : 1;
}

}  // namespace

int main(int argc, char** argv)
{
#ifdef _WIN32
    SetConsoleOutputCP(CP_UTF8);
#endif
    std::vector<std::string> args(argv + 1, argv + argc);
    std::optional<std::filesystem::path> configPath;
    if (args.size() >= 2 && args[0] == "--config") {
        configPath = ra3::utf8ToPath(args[1]);
        args.erase(args.begin(), args.begin() + 2);
    }
    if (args.empty() || args[0] == "--help" || args[0] == "-h") {
        std::cout << kUsage;
        return args.empty() ? 1 : 0;
    }

    const std::string command = args[0];
    if (command == "paths") {
        return commandPaths(configPath);
    }

    auto context = loadContext(configPath);
    if (!context) {
        return fail(context.error().message);
    }
    if (command == "sku") {
        return commandSku(context.value());
    }
    if (command == "mount") {
        return commandMount(context.value());
    }
    if (command == "big" && args.size() >= 2) {
        std::size_t limit = 50;
        if (args.size() >= 4 && args[2] == "--limit") {
            limit = static_cast<std::size_t>(std::strtoul(args[3].c_str(), nullptr, 10));
        }
        return commandBig(context.value(), args[1], limit);
    }
    if (command == "find" && args.size() >= 2) {
        return commandFind(context.value(), args[1]);
    }
    if (command == "unpack-check" && args.size() >= 2) {
        return commandUnpackCheck(context.value(), args[1]);
    }
    std::cerr << kUsage;
    return 1;
}
