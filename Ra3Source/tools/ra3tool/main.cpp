// ra3tool: inspects the configured, read-only RA3 installation.

#include <ra3/asset/AssetInstance.h>
#include <ra3/asset/AssetStream.h>
#include <ra3/asset/NameHash.h>
#include <ra3/asset/TypedAsset.h>
#include <ra3/core/FileUtil.h>
#include <ra3/core/GameConfig.h>
#include <ra3/core/SkuDef.h>
#include <ra3/core/StringUtil.h>
#include <ra3/io/ArchiveSet.h>
#include <ra3/io/BigArchive.h>
#include <ra3/io/RefPack.h>

#include <algorithm>
#include <chrono>
#include <cstring>
#include <cstdlib>
#include <iomanip>
#include <iostream>
#include <map>
#include <optional>
#include <set>
#include <sstream>
#include <string>
#include <unordered_map>
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
  streams [--deep]            validate every mounted compiled-asset stream
  manifest <stem> [--limit N] show one stream manifest (for example data/locale)
  asset <stem> <Type:Name>    dump one asset's fixups and references
  dump <stem> <Type:Name>     dump one validated asset through generated reflection
  types-check                 walk every shipped instance of every generated type
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

std::string hex32(std::uint32_t value)
{
    std::ostringstream out;
    out << "0x" << std::hex << std::uppercase << std::setw(8) << std::setfill('0') << value;
    return out.str();
}

std::string typePrefix(std::string_view name)
{
    const std::size_t colon = name.find(':');
    return std::string(name.substr(0, colon));
}

int commandStreams(const Context& context, bool deep)
{
    const auto started = std::chrono::steady_clock::now();
    auto mounted = mountAll(context);
    if (!mounted) {
        return fail(mounted.error().message);
    }
    const auto stems = ra3::asset::enumerateStreamStems(mounted.value());
    std::uint64_t assetCount = 0;
    std::size_t totalSizeMismatches = 0;
    std::size_t idMismatches = 0;
    std::vector<std::string> problems;
    std::map<std::string, std::uint64_t> countsByType;
    std::map<std::uint32_t, std::string> namesByTypeId;
    std::map<std::uint32_t, std::set<std::uint32_t>> hashesByTypeId;
    std::uint64_t relocationCount = 0;
    std::uint64_t importCount = 0;
    std::uint64_t referencesGreaterThanImports = 0;
    std::uint64_t nonSequentialImports = 0;
    std::uint64_t targetsAtEnd = 0;
    std::uint64_t targetsPastEnd = 0;
    std::uint64_t largestInstance = 0;

    for (const std::string& stem : stems) {
        auto validation = ra3::asset::validateStreamBundle(mounted.value(), stem);
        if (!validation) {
            problems.push_back(validation.error().message);
            continue;
        }
        const ra3::asset::Manifest& manifest = validation.value().manifest;
        assetCount += manifest.entries().size();
        if (!validation.value().totalInstanceDataSizeMatches) {
            ++totalSizeMismatches;
        }
        for (std::size_t i = 0; i < manifest.entries().size(); ++i) {
            const auto& entry = manifest.entries()[i];
            const std::string_view name = manifest.name(i);
            const std::string prefix = typePrefix(name);
            const std::size_t colon = name.find(':');
            const std::string_view instanceName =
                colon == std::string_view::npos ? name : name.substr(colon + 1);
            if (ra3::asset::instanceIdFromName(instanceName) != entry.id.instanceId &&
                ++idMismatches <= 10) {
                problems.push_back(stem + ": " + std::string(name) + " instanceId " +
                                   hex32(entry.id.instanceId) + " != name hash " +
                                   hex32(ra3::asset::instanceIdFromName(instanceName)));
            }
            if (ra3::asset::typeIdFromName(prefix) != entry.id.typeId && ++idMismatches <= 10) {
                problems.push_back(stem + ": " + std::string(name) + " typeId " +
                                   hex32(entry.id.typeId) + " != type name hash " +
                                   hex32(ra3::asset::typeIdFromName(prefix)));
            }
            ++countsByType[prefix];
            const auto inserted = namesByTypeId.emplace(entry.id.typeId, prefix);
            if (!inserted.second && inserted.first->second != prefix) {
                problems.push_back(
                    stem + ": typeId " + hex32(entry.id.typeId) + " maps to both " +
                    inserted.first->second + " and " + prefix);
            }
            hashesByTypeId[entry.id.typeId].insert(entry.typeHash);
        }
        if (deep) {
            auto deepValidation = ra3::asset::validateStreamDeep(mounted.value(), stem);
            if (!deepValidation) {
                problems.push_back(deepValidation.error().message);
                continue;
            }
            relocationCount += deepValidation.value().relocations;
            importCount += deepValidation.value().imports;
            referencesGreaterThanImports +=
                deepValidation.value().assetsWithMoreReferencesThanImports;
            nonSequentialImports += deepValidation.value().nonSequentialImportSites;
            targetsAtEnd += deepValidation.value().relocationTargetsAtInstanceEnd;
            targetsPastEnd += deepValidation.value().relocationTargetsPastInstanceEnd;
            largestInstance =
                std::max(largestInstance, deepValidation.value().largestInstance);
            problems.insert(
                problems.end(), deepValidation.value().problems.begin(),
                deepValidation.value().problems.end());
        }
    }

    std::size_t varyingTypeHashes = 0;
    for (const auto& item : hashesByTypeId) {
        if (item.second.size() > 1) {
            ++varyingTypeHashes;
        }
    }
    std::vector<std::pair<std::uint64_t, std::string>> topTypes;
    for (const auto& item : countsByType) {
        topTypes.emplace_back(item.second, item.first);
    }
    std::sort(topTypes.begin(), topTypes.end(),
              [](const auto& a, const auto& b) {
                  return a.first != b.first ? a.first > b.first : a.second < b.second;
              });

    std::cout << "streams                         : " << stems.size() << "\n"
              << "assets                          : " << assetCount << "\n"
              << "problems                        : " << problems.size() << "\n"
              << "totalInstanceDataSize mismatches: " << totalSizeMismatches << "\n"
              << "type/instance id != name hash   : " << idMismatches << "\n"
              << "typeIds with varying typeHash   : " << varyingTypeHashes << " (information only)\n"
              << "deep validation                 : " << (deep ? "yes" : "no") << "\n";
    if (deep) {
        std::cout << "relocations                     : " << relocationCount << "\n"
                  << "imports                         : " << importCount << "\n"
                  << "assets with refs > imports      : " << referencesGreaterThanImports << "\n"
                  << "non-sequential import indices   : " << nonSequentialImports << "\n"
                  << "relocation targets == size (ok) : " << targetsAtEnd << "\n"
                  << "relocation targets > size       : " << targetsPastEnd << "\n"
                  << "largest instance                : " << largestInstance << " bytes\n";
    }
    const auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now() - started);
    std::cout << "wall time                       : " << elapsed.count() << " ms\n"
              << "top asset types:\n";
    constexpr std::size_t kTopTypes = 15;
    for (std::size_t i = 0; i < topTypes.size() && i < kTopTypes; ++i) {
        std::cout << "  " << std::setw(8) << topTypes[i].first << "  " << topTypes[i].second << "\n";
    }
    if (!problems.empty()) {
        std::cout << "problem list:\n";
        for (const std::string& problem : problems) {
            std::cout << "  " << problem << "\n";
        }
    }
    return problems.empty() ? 0 : 1;
}

ra3::Result<std::vector<std::uint8_t>> readDecodedRange(
    ra3::io::DecodedStream& stream,
    std::uint64_t offset,
    std::size_t count,
    const std::string& member)
{
    while (stream.position() < offset) {
        const std::size_t amount = static_cast<std::size_t>(
            std::min<std::uint64_t>(64 * 1024, offset - stream.position()));
        auto skipped = stream.read(amount);
        if (!skipped) return skipped.error();
        if (skipped.value().empty()) {
            return ra3::makeError(member + ": ended while seeking to decoded offset " +
                                  std::to_string(offset));
        }
    }
    std::vector<std::uint8_t> result;
    result.reserve(count);
    while (result.size() < count) {
        auto chunk = stream.read(count - result.size());
        if (!chunk) return chunk.error();
        if (chunk.value().empty()) {
            return ra3::makeError(member + ": ended while reading decoded offset " +
                                  std::to_string(offset + result.size()));
        }
        result.insert(result.end(), chunk.value().begin(), chunk.value().end());
    }
    return result;
}

std::uint64_t readUnsigned(
    const ra3::asset::AssetInstance& instance, std::uint32_t offset, std::uint32_t size)
{
    const auto bytes = instance.bytes();
    if (offset > bytes.size || size > bytes.size - offset || size > 8) return 0;
    std::uint64_t value = 0;
    for (std::uint32_t index = 0; index < size; ++index) {
        value |= static_cast<std::uint64_t>(bytes.data[offset + index]) << (index * 8);
    }
    return value;
}

struct TypedWalk {
    const ra3::asset::AssetInstance& instance;
    std::set<std::uint32_t> relocations;
    std::set<std::uint32_t> imports;
    std::vector<std::string> errors;
    std::uint64_t fields = 0;

    void error(const std::string& path, const std::string& message)
    {
        errors.push_back(path + ": " + message);
    }

    bool validEnum(std::string_view name, std::uint32_t value) const
    {
        const auto* info = ra3::asset::findGeneratedEnum(name);
        if (info == nullptr) return true;
        return std::any_of(
            info->values, info->values + info->valueCount,
            [value](const ra3::asset::EnumValueInfo& item) {
                return item.value == value;
            });
    }

    void walk(
        const ra3::asset::TypeInfo& type,
        std::uint32_t base,
        const std::string& path)
    {
        if (base > instance.bytes().size ||
            type.size > instance.bytes().size - base) {
            error(path, "object is outside the instance");
            return;
        }
        for (std::size_t index = 0; index < type.fieldCount; ++index) {
            const ra3::asset::FieldInfo& field = type.fields[index];
            const std::uint32_t offset = base + field.offset;
            const std::string fieldPath =
                path.empty() ? std::string(field.name) :
                               path + "." + std::string(field.name);
            ++fields;
            switch (field.kind) {
            case ra3::asset::FieldKind::Boolean:
                if (const auto value = readUnsigned(instance, offset, field.size);
                    value > 1) {
                    error(fieldPath, "boolean value " + std::to_string(value) +
                                         " is not 0 or 1");
                }
                break;
            case ra3::asset::FieldKind::Enum: {
                const auto value = static_cast<std::uint32_t>(
                    readUnsigned(instance, offset, field.size));
                if (!validEnum(field.elementType, value)) {
                    error(fieldPath, "enum value " + std::to_string(value) +
                                         " is not declared");
                }
                break;
            }
            case ra3::asset::FieldKind::String: {
                ra3::asset::CompiledString value;
                std::memcpy(&value, instance.bytes().data + offset, sizeof(value));
                auto resolved = ra3::asset::resolve(instance, value, offset);
                if (!resolved) error(fieldPath, resolved.error().message);
                if (value.offset != 0) relocations.insert(offset + 4);
                break;
            }
            case ra3::asset::FieldKind::FileReference: {
                ra3::asset::FileReference value;
                std::memcpy(&value, instance.bytes().data + offset, sizeof(value));
                auto resolved = ra3::asset::resolve(instance, value, offset);
                if (!resolved) error(fieldPath, resolved.error().message);
                if (value.offset != 0) relocations.insert(offset);
                break;
            }
            case ra3::asset::FieldKind::Blob: {
                ra3::asset::DataBlob value;
                std::memcpy(&value, instance.bytes().data + offset, sizeof(value));
                auto resolved = ra3::asset::resolve(instance, value, offset);
                if (!resolved) error(fieldPath, resolved.error().message);
                if (value.offset != 0) relocations.insert(offset);
                break;
            }
            case ra3::asset::FieldKind::AssetReference: {
                ra3::asset::AssetReference value;
                std::memcpy(&value, instance.bytes().data + offset, sizeof(value));
                auto resolved = ra3::asset::resolve(instance, value, offset);
                if (!resolved) error(fieldPath, resolved.error().message);
                if (value.index != 0) imports.insert(offset);
                break;
            }
            case ra3::asset::FieldKind::Struct: {
                const auto* child = ra3::asset::findGeneratedType(field.elementType);
                if (child == nullptr) {
                    error(fieldPath, "missing reflected struct type");
                } else {
                    walk(*child, offset, fieldPath);
                }
                break;
            }
            case ra3::asset::FieldKind::List:
            case ra3::asset::FieldKind::SimpleList: {
                ra3::asset::CompiledList<> value;
                std::memcpy(&value, instance.bytes().data + offset, sizeof(value));
                auto resolved = ra3::asset::resolveList(
                    instance, value.count, value.offset, field.elementSize, offset);
                if (!resolved) {
                    error(fieldPath, resolved.error().message);
                    break;
                }
                if (value.offset != 0) relocations.insert(offset + 4);
                const auto* child = ra3::asset::findGeneratedType(field.elementType);
                const auto* enumeration =
                    ra3::asset::findGeneratedEnum(field.elementType);
                for (std::uint32_t item = 0; item < value.count; ++item) {
                    const std::uint32_t itemOffset =
                        value.offset + item * field.elementSize;
                    const std::string itemPath =
                        fieldPath + "[" + std::to_string(item) + "]";
                    if (field.kind == ra3::asset::FieldKind::List &&
                        child != nullptr) {
                        walk(*child, itemOffset, itemPath);
                    } else if (enumeration != nullptr) {
                        ++fields;
                        const auto enumValue = static_cast<std::uint32_t>(
                            readUnsigned(instance, itemOffset, field.elementSize));
                        if (!validEnum(field.elementType, enumValue)) {
                            error(itemPath, "enum value " +
                                                std::to_string(enumValue) +
                                                " is not declared");
                        }
                    }
                }
                break;
            }
            default:
                break;
            }
        }
    }

    void finish()
    {
        const std::set<std::uint32_t> actualRelocations(
            instance.relocations().sites().begin(),
            instance.relocations().sites().end());
        std::set<std::uint32_t> actualImports;
        for (const auto& item : instance.imports().imports()) {
            actualImports.insert(item.site);
        }
        if (relocations != actualRelocations) {
            error("$relocations", "reflection does not cover the relocation table exactly");
        }
        if (imports != actualImports) {
            error("$imports", "reflection does not cover the import table exactly");
        }
    }
};

std::string enumName(std::string_view type, std::uint32_t value)
{
    const auto* info = ra3::asset::findGeneratedEnum(type);
    if (info != nullptr) {
        for (std::size_t index = 0; index < info->valueCount; ++index) {
            if (info->values[index].value == value) {
                return std::string(info->values[index].name);
            }
        }
    }
    return std::to_string(value);
}

void dumpTyped(
    const ra3::asset::AssetInstance& instance,
    const ra3::asset::TypeInfo& type,
    std::uint32_t base,
    const ra3::asset::Manifest& manifest,
    const std::unordered_map<std::uint64_t, std::string>& assetNames,
    int indent)
{
    const std::string spaces(static_cast<std::size_t>(indent), ' ');
    for (std::size_t index = 0; index < type.fieldCount; ++index) {
        const auto& field = type.fields[index];
        const std::uint32_t offset = base + field.offset;
        std::cout << spaces << field.name << " = ";
        switch (field.kind) {
        case ra3::asset::FieldKind::String: {
            ra3::asset::CompiledString value;
            std::memcpy(&value, instance.bytes().data + offset, sizeof(value));
            auto resolved = ra3::asset::resolve(instance, value, offset);
            std::cout << (resolved ? "\"" + std::string(resolved.value()) + "\""
                                   : "<" + resolved.error().message + ">")
                      << "\n";
            break;
        }
        case ra3::asset::FieldKind::FileReference: {
            ra3::asset::FileReference value;
            std::memcpy(&value, instance.bytes().data + offset, sizeof(value));
            auto resolved = ra3::asset::resolve(instance, value, offset);
            std::cout << (resolved && resolved.value() ?
                              "\"" + std::string(*resolved.value()) + "\"" :
                              (resolved ? "null" : "<" + resolved.error().message + ">"))
                      << "\n";
            break;
        }
        case ra3::asset::FieldKind::AssetReference: {
            const auto* imported = instance.importAt(offset);
            if (imported == nullptr) {
                std::cout << "null\n";
            } else {
                const auto* target = manifest.find(imported->target);
                if (target != nullptr) {
                    const auto targetIndex = static_cast<std::size_t>(
                        target - manifest.entries().data());
                    std::cout << manifest.name(targetIndex) << "\n";
                } else {
                    const std::uint64_t key =
                        (static_cast<std::uint64_t>(imported->target.typeId) << 32) |
                        imported->target.instanceId;
                    const auto known = assetNames.find(key);
                    if (known != assetNames.end()) {
                        std::cout << known->second << "\n";
                    } else {
                        std::cout << hex32(imported->target.typeId) << ":"
                                  << hex32(imported->target.instanceId) << "\n";
                    }
                }
            }
            break;
        }
        case ra3::asset::FieldKind::Struct: {
            std::cout << "{\n";
            const auto* child = ra3::asset::findGeneratedType(field.elementType);
            if (child != nullptr) {
                dumpTyped(instance, *child, offset, manifest, assetNames, indent + 2);
            }
            std::cout << spaces << "}\n";
            break;
        }
        case ra3::asset::FieldKind::List:
        case ra3::asset::FieldKind::SimpleList: {
            ra3::asset::CompiledList<> value;
            std::memcpy(&value, instance.bytes().data + offset, sizeof(value));
            std::cout << "[" << value.count << "]\n";
            const auto* child = ra3::asset::findGeneratedType(field.elementType);
            for (std::uint32_t item = 0; item < value.count; ++item) {
                const auto itemOffset = value.offset + item * field.elementSize;
                std::cout << spaces << "  [" << item << "] ";
                if (field.kind == ra3::asset::FieldKind::List && child != nullptr) {
                    std::cout << "{\n";
                    dumpTyped(
                        instance, *child, itemOffset, manifest, assetNames, indent + 4);
                    std::cout << spaces << "  }\n";
                } else {
                    const auto raw = static_cast<std::uint32_t>(
                        readUnsigned(instance, itemOffset, field.elementSize));
                    std::cout << enumName(field.elementType, raw) << "\n";
                }
            }
            break;
        }
        case ra3::asset::FieldKind::Enum: {
            const auto raw = static_cast<std::uint32_t>(
                readUnsigned(instance, offset, field.size));
            std::cout << enumName(field.elementType, raw) << "\n";
            break;
        }
        case ra3::asset::FieldKind::Blob: {
            ra3::asset::DataBlob value;
            std::memcpy(&value, instance.bytes().data + offset, sizeof(value));
            std::cout << "<" << value.size << " bytes @" << hex32(value.offset)
                      << ">\n";
            break;
        }
        default:
            std::cout << readUnsigned(instance, offset, field.size) << "\n";
            break;
        }
    }
}

bool displayable(std::string_view value)
{
    if (value.empty()) return false;
    return std::all_of(value.begin(), value.end(), [](char c) {
        const auto byte = static_cast<unsigned char>(c);
        return byte >= 0x20 && byte < 0x7F;
    });
}

int commandAsset(const Context& context, const std::string& stem, const std::string& name)
{
    auto mounted = mountAll(context);
    if (!mounted) return fail(mounted.error().message);
    auto validation = ra3::asset::validateStreamBundle(mounted.value(), stem);
    if (!validation) return fail(validation.error().message);
    const ra3::asset::Manifest& manifest = validation.value().manifest;
    const ra3::asset::AssetEntry* entry = manifest.findByName(name);
    if (entry == nullptr) return fail(stem + ": asset not found: " + name);
    const std::size_t index =
        static_cast<std::size_t>(entry - manifest.entries().data());

    auto bin = ra3::asset::openDecodedMember(mounted.value(), stem + ".bin");
    auto relo = ra3::asset::openDecodedMember(mounted.value(), stem + ".relo");
    auto imp = ra3::asset::openDecodedMember(mounted.value(), stem + ".imp");
    if (!bin) return fail(bin.error().message);
    if (!relo) return fail(relo.error().message);
    if (!imp) return fail(imp.error().message);
    auto instanceBytes = readDecodedRange(
        bin.value(), entry->instanceDataOffset, entry->instanceDataSize, stem + ".bin");
    auto relocationBytes = readDecodedRange(
        relo.value(), entry->relocationDataOffset, entry->relocationDataSize, stem + ".relo");
    auto importBytes = readDecodedRange(
        imp.value(), entry->importsDataOffset, entry->importsDataSize, stem + ".imp");
    if (!instanceBytes) return fail(instanceBytes.error().message);
    if (!relocationBytes) return fail(relocationBytes.error().message);
    if (!importBytes) return fail(importBytes.error().message);
    const auto references = manifest.references(index);
    auto instance = ra3::asset::AssetInstance::parse(
        {instanceBytes.value().data(), instanceBytes.value().size()},
        {relocationBytes.value().data(), relocationBytes.value().size()},
        {importBytes.value().data(), importBytes.value().size()},
        references, stem + "/" + name, entry->relocationDataOffset,
        entry->importsDataOffset, entry->id);
    if (!instance) return fail(instance.error().message);

    std::cout << "asset       : " << name << "\n"
              << "stem        : " << stem << "\n"
              << "source      : " << manifest.sourceFile(index) << "\n"
              << "typeId      : " << hex32(entry->id.typeId) << "\n"
              << "instanceId  : " << hex32(entry->id.instanceId) << "\n"
              << "typeHash    : " << hex32(entry->typeHash) << "\n"
              << "instanceHash: " << hex32(entry->instanceHash) << "\n"
              << "sizes       : " << entry->instanceDataSize << "/" << entry->relocationDataSize
              << "/" << entry->importsDataSize << "\n"
              << "flags       : " << entry->flags << "\n"
              << "references:\n";
    for (std::size_t i = 0; i < references.size(); ++i) {
        const auto* target = manifest.find(references[i]);
        std::cout << "  [" << (i + 1) << "] " << hex32(references[i].typeId) << ":"
                  << hex32(references[i].instanceId);
        if (target != nullptr) {
            const std::size_t targetIndex =
                static_cast<std::size_t>(target - manifest.entries().data());
            std::cout << "  " << manifest.name(targetIndex);
        }
        std::cout << "\n";
    }
    std::cout << "relocations:\n";
    for (const std::uint32_t site : instance.value().relocations().sites()) {
        const auto target = instance.value().relocationTarget(site);
        std::cout << "  " << hex32(site) << " -> "
                  << (target ? hex32(*target) : std::string("(invalid)"));
        if (site >= 4) {
            auto text = instance.value().readString(site - 4);
            if (text && displayable(text.value())) {
                std::cout << "  string=\"" << text.value() << "\"";
            }
        }
        std::cout << "\n";
    }
    std::cout << "imports:\n";
    for (const auto& imported : instance.value().imports().imports()) {
        const auto* target = manifest.find(imported.target);
        std::cout << "  " << hex32(imported.site) << " -> [" << imported.referenceIndex << "] "
                  << hex32(imported.target.typeId) << ":" << hex32(imported.target.instanceId);
        if (target != nullptr) {
            const std::size_t targetIndex =
                static_cast<std::size_t>(target - manifest.entries().data());
            std::cout << "  " << manifest.name(targetIndex);
        }
        std::cout << "\n";
    }
    return 0;
}

int commandDump(const Context& context, const std::string& stem, const std::string& name)
{
    auto mounted = mountAll(context);
    if (!mounted) return fail(mounted.error().message);
    auto validation = ra3::asset::validateStreamBundle(mounted.value(), stem);
    if (!validation) return fail(validation.error().message);
    const auto& manifest = validation.value().manifest;
    const auto* entry = manifest.findByName(name);
    if (entry == nullptr) return fail(stem + ": asset not found: " + name);
    const auto* type = ra3::asset::findGeneratedType(entry->id.typeId);
    if (type == nullptr) return fail(name + ": type has no fully validated generated layout");
    const auto index = static_cast<std::size_t>(entry - manifest.entries().data());

    auto bin = ra3::asset::openDecodedMember(mounted.value(), stem + ".bin");
    auto relo = ra3::asset::openDecodedMember(mounted.value(), stem + ".relo");
    auto imp = ra3::asset::openDecodedMember(mounted.value(), stem + ".imp");
    if (!bin) return fail(bin.error().message);
    if (!relo) return fail(relo.error().message);
    if (!imp) return fail(imp.error().message);
    auto bytes = readDecodedRange(
        bin.value(), entry->instanceDataOffset, entry->instanceDataSize, stem + ".bin");
    auto relocations = readDecodedRange(
        relo.value(), entry->relocationDataOffset, entry->relocationDataSize,
        stem + ".relo");
    auto imports = readDecodedRange(
        imp.value(), entry->importsDataOffset, entry->importsDataSize, stem + ".imp");
    if (!bytes) return fail(bytes.error().message);
    if (!relocations) return fail(relocations.error().message);
    if (!imports) return fail(imports.error().message);
    auto instance = ra3::asset::AssetInstance::parse(
        {bytes.value().data(), bytes.value().size()},
        {relocations.value().data(), relocations.value().size()},
        {imports.value().data(), imports.value().size()},
        manifest.references(index), stem + "/" + name,
        entry->relocationDataOffset, entry->importsDataOffset, entry->id);
    if (!instance) return fail(instance.error().message);
    if (bytes.value().empty()) return fail(name + ": external placeholder has no instance");

    std::unordered_map<std::uint64_t, std::string> assetNames;
    for (std::size_t asset = 0; asset < manifest.entries().size(); ++asset) {
        const auto& id = manifest.entries()[asset].id;
        const std::uint64_t key =
            (static_cast<std::uint64_t>(id.typeId) << 32) | id.instanceId;
        assetNames.emplace(key, std::string(manifest.name(asset)));
    }
    std::set<std::uint64_t> unresolved;
    for (const auto& imported : instance.value().imports().imports()) {
        const std::uint64_t key =
            (static_cast<std::uint64_t>(imported.target.typeId) << 32) |
            imported.target.instanceId;
        if (assetNames.find(key) == assetNames.end()) unresolved.insert(key);
    }
    const auto mountedStems = ra3::asset::enumerateStreamStems(mounted.value());
    std::vector<std::string> catalogQueue;
    std::set<std::string> queued;
    const auto enqueueDependencies = [&](const ra3::asset::Manifest& source) {
        for (const auto& reference : source.referencedManifests()) {
            const std::string wanted =
                ra3::io::normalizeArchivePath(reference.name);
            for (const std::string& candidate : mountedStems) {
                const std::string member = candidate + ".manifest";
                if (member.size() >= wanted.size() &&
                    member.compare(
                        member.size() - wanted.size(), wanted.size(), wanted) == 0 &&
                    queued.insert(candidate).second) {
                    catalogQueue.push_back(candidate);
                }
            }
        }
    };
    enqueueDependencies(manifest);
    for (std::size_t catalogIndex = 0;
         catalogIndex < catalogQueue.size(); ++catalogIndex) {
        if (unresolved.empty()) break;
        const std::string& catalogStem = catalogQueue[catalogIndex];
        if (catalogStem == stem) continue;
        auto catalog = ra3::asset::readStreamManifest(
            mounted.value(), catalogStem);
        if (!catalog) return fail(catalog.error().message);
        const auto& catalogManifest = catalog.value();
        enqueueDependencies(catalogManifest);
        for (std::size_t asset = 0;
             asset < catalogManifest.entries().size(); ++asset) {
            const auto& id = catalogManifest.entries()[asset].id;
            const std::uint64_t key =
                (static_cast<std::uint64_t>(id.typeId) << 32) | id.instanceId;
            if (unresolved.erase(key) != 0) {
                assetNames.emplace(key, std::string(catalogManifest.name(asset)));
            }
        }
    }
    TypedWalk check{instance.value()};
    check.walk(*type, 0, "");
    check.finish();
    if (!check.errors.empty()) return fail(name + ": " + check.errors.front());
    std::cout << name << " {\n";
    dumpTyped(instance.value(), *type, 0, manifest, assetNames, 2);
    std::cout << "}\n";
    return 0;
}

int commandTypesCheck(const Context& context)
{
    const auto started = std::chrono::steady_clock::now();
    auto mounted = mountAll(context);
    if (!mounted) return fail(mounted.error().message);
    std::set<std::uint32_t> checkedTypes;
    std::uint64_t checkedInstances = 0;
    std::uint64_t checkedFields = 0;
    std::uint64_t failures = 0;
    std::vector<std::string> examples;
    std::map<std::string, std::uint64_t> failureSummary;

    for (const std::string& stem :
         ra3::asset::enumerateStreamStems(mounted.value())) {
        auto validation = ra3::asset::validateStreamBundle(mounted.value(), stem);
        if (!validation) return fail(validation.error().message);
        const auto& manifest = validation.value().manifest;
        auto bin = ra3::asset::openDecodedMember(mounted.value(), stem + ".bin");
        auto relo = ra3::asset::openDecodedMember(mounted.value(), stem + ".relo");
        auto imp = ra3::asset::openDecodedMember(mounted.value(), stem + ".imp");
        if (!bin) return fail(bin.error().message);
        if (!relo) return fail(relo.error().message);
        if (!imp) return fail(imp.error().message);

        for (std::size_t index = 0; index < manifest.entries().size(); ++index) {
            const auto& entry = manifest.entries()[index];
            const auto* type = ra3::asset::findGeneratedType(entry.id.typeId);
            if (type == nullptr) continue;
            checkedTypes.insert(entry.id.typeId);
            ++checkedInstances;
            auto bytes = readDecodedRange(
                bin.value(), entry.instanceDataOffset, entry.instanceDataSize,
                stem + ".bin");
            auto relocations = readDecodedRange(
                relo.value(), entry.relocationDataOffset, entry.relocationDataSize,
                stem + ".relo");
            auto imports = readDecodedRange(
                imp.value(), entry.importsDataOffset, entry.importsDataSize,
                stem + ".imp");
            if (!bytes || !relocations || !imports) {
                return fail(stem + ": failed to read typed instance chunks");
            }
            const std::string assetName =
                stem + "/" + std::string(manifest.name(index));
            auto instance = ra3::asset::AssetInstance::parse(
                {bytes.value().data(), bytes.value().size()},
                {relocations.value().data(), relocations.value().size()},
                {imports.value().data(), imports.value().size()},
                manifest.references(index), assetName,
                entry.relocationDataOffset, entry.importsDataOffset, entry.id);
            if (!instance) {
                ++failures;
                if (examples.size() < 20) {
                    examples.push_back(assetName + ": " + instance.error().message);
                }
                continue;
            }
            if (bytes.value().empty()) continue;
            TypedWalk walk{instance.value()};
            walk.walk(*type, 0, "");
            walk.finish();
            checkedFields += walk.fields;
            failures += walk.errors.size();
            for (const std::string& item : walk.errors) {
                const std::size_t separator = item.find(':');
                ++failureSummary[item.substr(0, separator)];
                if (examples.size() < 20) {
                    examples.push_back(assetName + ": " + item);
                }
            }
        }
    }
    const auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now() - started);
    std::cout << "types     : " << checkedTypes.size() << " / "
              << ra3::asset::generatedTypeCount() << "\n"
              << "instances : " << checkedInstances << "\n"
              << "fields    : " << checkedFields << "\n"
              << "failures  : " << failures << "\n"
              << "wall time : " << elapsed.count() << " ms\n";
    for (const std::string& example : examples) {
        std::cout << "  " << example << "\n";
    }
    if (!failureSummary.empty()) {
        std::vector<std::pair<std::uint64_t, std::string>> ranked;
        for (const auto& item : failureSummary) {
            ranked.emplace_back(item.second, item.first);
        }
        std::sort(ranked.begin(), ranked.end(), std::greater<>());
        std::cout << "failure summary:\n";
        for (std::size_t index = 0; index < ranked.size() && index < 20; ++index) {
            std::cout << "  " << ranked[index].first << "  "
                      << ranked[index].second << "\n";
        }
    }
    return failures == 0 &&
                   checkedTypes.size() == ra3::asset::generatedTypeCount() ?
               0 :
               1;
}

int commandManifest(const Context& context, const std::string& stem, std::size_t limit)
{
    auto mounted = mountAll(context);
    if (!mounted) {
        return fail(mounted.error().message);
    }
    auto validation = ra3::asset::validateStreamBundle(mounted.value(), stem);
    if (!validation) {
        return fail(validation.error().message);
    }
    const ra3::asset::Manifest& manifest = validation.value().manifest;
    const auto& h = manifest.header();
    std::cout << "stem                : " << stem << "\n"
              << "isBigEndian         : " << unsigned(h.isBigEndian) << "\n"
              << "isLinked            : " << unsigned(h.isLinked) << "\n"
              << "version             : " << h.version << "\n"
              << "streamChecksum      : " << hex32(h.streamChecksum) << "\n"
              << "allTypesHash        : " << hex32(h.allTypesHash) << "\n"
              << "assetCount          : " << h.assetCount << "\n"
              << "totalInstanceDataSize: " << h.totalInstanceDataSize << "\n"
              << "instance size sum   : " << manifest.instanceDataSize() << "\n"
              << "max chunks (bin/relo/imp): " << h.maxInstanceChunkSize << " / "
              << h.maxRelocationChunkSize << " / " << h.maxImportsChunkSize << "\n"
              << "buffers (refs/manifests/names/sources): " << h.assetReferenceBufferSize << " / "
              << h.referencedManifestBufferSize << " / " << h.assetNameBufferSize << " / "
              << h.sourceFileNameBufferSize << "\n"
              << "referenced manifests:\n";
    for (const auto& reference : manifest.referencedManifests()) {
        std::cout << "  kind=" << unsigned(reference.kind) << "  " << reference.name << "\n";
    }
    std::cout << "assets:\n";
    const std::size_t shown = std::min(limit, manifest.entries().size());
    for (std::size_t i = 0; i < shown; ++i) {
        const auto& entry = manifest.entries()[i];
        std::cout << "  [" << i << "] " << manifest.name(i) << "\n"
                  << "      typeId=" << hex32(entry.id.typeId)
                  << " instanceId=" << hex32(entry.id.instanceId)
                  << " typeHash=" << hex32(entry.typeHash)
                  << " instanceHash=" << hex32(entry.instanceHash) << "\n"
                  << "      sizes=" << entry.instanceDataSize << "/" << entry.relocationDataSize
                  << "/" << entry.importsDataSize
                  << " references=" << entry.assetReferenceCount
                  << " flags=" << entry.flags << "\n";
    }
    if (shown < manifest.entries().size()) {
        std::cout << "... " << (manifest.entries().size() - shown) << " more (use --limit)\n";
    }
    return 0;
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
    if (command == "streams") {
        return commandStreams(
            context.value(), args.size() >= 2 && args[1] == "--deep");
    }
    if (command == "manifest" && args.size() >= 2) {
        std::size_t limit = 50;
        if (args.size() >= 4 && args[2] == "--limit") {
            limit = static_cast<std::size_t>(std::strtoul(args[3].c_str(), nullptr, 10));
        }
        return commandManifest(context.value(), args[1], limit);
    }
    if (command == "asset" && args.size() >= 3) {
        return commandAsset(context.value(), args[1], args[2]);
    }
    if (command == "dump" && args.size() >= 3) {
        return commandDump(context.value(), args[1], args[2]);
    }
    if (command == "types-check") {
        return commandTypesCheck(context.value());
    }
    std::cerr << kUsage;
    return 1;
}
