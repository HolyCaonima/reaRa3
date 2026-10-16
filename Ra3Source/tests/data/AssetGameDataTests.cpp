#include "support/TestHarness.h"

#include <ra3/asset/AssetStream.h>
#include <ra3/core/GameConfig.h>
#include <ra3/core/SkuDef.h>
#include <ra3/io/ArchiveSet.h>

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

ra3::io::ArchiveSet mountInstallation()
{
    const Installation install = requireInstallation();
    auto archives = ra3::io::ArchiveSet::mount(install.config.gameRoot, install.sku);
    RA3_REQUIRE_OK(archives);
    return std::move(archives).value();
}

}  // namespace

RA3_TEST(AssetGameData_LocaleMatchesVerifiedManifest)
{
    const auto archives = mountInstallation();
    auto validation = ra3::asset::validateStreamBundle(archives, "data/locale");
    RA3_REQUIRE_OK(validation);
    const auto& manifest = validation.value().manifest;
    const auto& header = manifest.header();
    RA3_CHECK_EQ(header.assetCount, 19u);
    RA3_CHECK_EQ(header.streamChecksum, 0xDC63FC0Du);
    RA3_CHECK_EQ(header.allTypesHash, 0x5E3B5247u);
    RA3_CHECK_EQ(header.totalInstanceDataSize, 856u);
    RA3_CHECK_EQ(header.assetReferenceBufferSize, 56u);
    RA3_CHECK_EQ(header.referencedManifestBufferSize, 0u);
    RA3_CHECK_EQ(header.assetNameBufferSize, 681u);
    RA3_CHECK_EQ(header.sourceFileNameBufferSize, 75u);
    RA3_CHECK_EQ(manifest.instanceDataSize(), std::uint64_t{856});
    RA3_CHECK_EQ(manifest.relocationDataSize(), std::uint64_t{100});
    RA3_CHECK_EQ(manifest.importsDataSize(), std::uint64_t{56});

    const auto& first = manifest.entries()[0];
    RA3_CHECK_EQ(manifest.name(0), std::string_view("FontSubstitution:Arial"));
    RA3_CHECK_EQ(first.id.typeId, 0x057786F9u);
    RA3_CHECK_EQ(first.id.instanceId, 0x23BAA623u);
    RA3_CHECK_EQ(first.typeHash, 0xA63E49F9u);
    RA3_CHECK_EQ(first.instanceHash, 0x3672E7DEu);
    RA3_CHECK_EQ(manifest.sourceFile(0), std::string_view("DATA:globaldata/fontsubstitution.xml"));
    RA3_CHECK_EQ(first.instanceDataSize, 108u);
    RA3_CHECK_EQ(first.relocationDataSize, 20u);
    RA3_CHECK_EQ(first.importsDataSize, 0u);

    const auto& texture = manifest.entries()[12];
    RA3_CHECK_EQ(manifest.name(12), std::string_view("OnDemandTextureImage:BootupScreen"));
    RA3_CHECK_EQ(texture.id.typeId, 0xFE0E84BBu);
    RA3_CHECK_EQ(texture.instanceDataSize, 36u);
    RA3_CHECK_EQ(texture.importsDataSize, 8u);
    const auto references = manifest.references(12);
    RA3_REQUIRE(references.size() == 1);
    RA3_CHECK_EQ(references[0].typeId, 0x9312B9ACu);
    RA3_CHECK_EQ(references[0].instanceId, 0x01F402C3u);

    RA3_CHECK_EQ(validation.value().members[0].decodedSize, std::uint64_t{1772});
    RA3_CHECK_EQ(validation.value().members[1].decodedSize, std::uint64_t{860});
    RA3_CHECK_EQ(validation.value().members[2].decodedSize, std::uint64_t{104});
    RA3_CHECK_EQ(validation.value().members[3].decodedSize, std::uint64_t{60});
}

RA3_TEST(AssetGameData_AllMountedStreamsPassLightValidation)
{
    const auto archives = mountInstallation();
    const auto stems = ra3::asset::enumerateStreamStems(archives);
    RA3_CHECK_EQ(stems.size(), std::size_t{257});
    std::uint64_t assets = 0;
    std::size_t failures = 0;
    for (const std::string& stem : stems) {
        auto validation = ra3::asset::validateStreamBundle(archives, stem);
        if (!validation) {
            ++failures;
            continue;
        }
        assets += validation.value().manifest.entries().size();
    }
    RA3_CHECK_EQ(failures, std::size_t{0});
    RA3_CHECK_EQ(assets, std::uint64_t{118232});
}

RA3_TEST(AssetGameData_GlobalFullLoadPartitionsEveryMember)
{
    const auto archives = mountInstallation();
    auto stream = ra3::asset::AssetStream::load(archives, "data/global");
    RA3_REQUIRE_OK(stream);
    const auto& manifest = stream.value().manifest();
    std::uint64_t instance = 4;
    std::uint64_t relocation = 4;
    std::uint64_t imports = 4;
    for (std::size_t i = 0; i < manifest.entries().size(); ++i) {
        RA3_CHECK_EQ(stream.value().instanceData(i).size,
                     static_cast<std::size_t>(manifest.entries()[i].instanceDataSize));
        RA3_CHECK_EQ(stream.value().relocationData(i).size,
                     static_cast<std::size_t>(manifest.entries()[i].relocationDataSize));
        RA3_CHECK_EQ(stream.value().importsData(i).size,
                     static_cast<std::size_t>(manifest.entries()[i].importsDataSize));
        instance += manifest.entries()[i].instanceDataSize;
        relocation += manifest.entries()[i].relocationDataSize;
        imports += manifest.entries()[i].importsDataSize;
    }
    RA3_CHECK_EQ(instance, stream.value().instanceMember().size);
    RA3_CHECK_EQ(relocation, stream.value().relocationMember().size);
    RA3_CHECK_EQ(imports, stream.value().importsMember().size);
}
