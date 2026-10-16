#include "support/TestHarness.h"

#include <ra3/asset/AssetInstance.h>
#include <ra3/asset/AssetStream.h>
#include <ra3/asset/Manifest.h>

#include <cstring>
#include <string>
#include <vector>

namespace {

void putU16(std::vector<std::uint8_t>& bytes, std::size_t offset, std::uint16_t value)
{
    bytes[offset] = static_cast<std::uint8_t>(value);
    bytes[offset + 1] = static_cast<std::uint8_t>(value >> 8);
}

void putU32(std::vector<std::uint8_t>& bytes, std::size_t offset, std::uint32_t value)
{
    for (std::size_t i = 0; i < 4; ++i) {
        bytes[offset + i] = static_cast<std::uint8_t>(value >> (i * 8));
    }
}

void append(std::vector<std::uint8_t>& bytes, const void* data, std::size_t size)
{
    const auto* first = static_cast<const std::uint8_t*>(data);
    bytes.insert(bytes.end(), first, first + size);
}

std::vector<std::uint8_t> makeManifest()
{
    constexpr char names[] = "Type:One";
    constexpr char sources[] = "DATA:x.xml";
    constexpr std::uint8_t refs[] = {0x22, 0x22, 0x11, 0x11, 0x44, 0x44, 0x33, 0x33};
    constexpr char referenced[] = "\x01" "audio.manifest";

    std::vector<std::uint8_t> bytes(48 + 48, 0);
    bytes[1] = 1;
    putU16(bytes, 2, 6);
    putU32(bytes, 4, 0x12345678);
    putU32(bytes, 8, 0xABCDEF01);
    putU32(bytes, 12, 1);
    putU32(bytes, 16, 3);
    putU32(bytes, 20, 3);
    putU32(bytes, 24, 2);
    putU32(bytes, 28, 1);
    putU32(bytes, 32, sizeof(refs));
    putU32(bytes, 36, sizeof(referenced));
    putU32(bytes, 40, sizeof(names));
    putU32(bytes, 44, sizeof(sources));

    putU32(bytes, 48, 0x01020304);
    putU32(bytes, 52, 0x05060708);
    putU32(bytes, 56, 0x11121314);
    putU32(bytes, 60, 0x15161718);
    putU32(bytes, 64, 0);
    putU32(bytes, 68, 1);
    putU32(bytes, 72, 0);
    putU32(bytes, 76, 0);
    putU32(bytes, 80, 3);
    putU32(bytes, 84, 2);
    putU32(bytes, 88, 1);
    putU32(bytes, 92, 7);
    append(bytes, refs, sizeof(refs));
    append(bytes, referenced, sizeof(referenced));
    append(bytes, names, sizeof(names));
    append(bytes, sources, sizeof(sources));
    return bytes;
}

std::vector<std::uint8_t> member(std::size_t payload)
{
    std::vector<std::uint8_t> bytes(4 + payload, 0);
    putU32(bytes, 0, 0x12345678);
    return bytes;
}

bool parseFails(const std::vector<std::uint8_t>& bytes)
{
    return !ra3::asset::Manifest::parse(bytes).ok();
}

}  // namespace

RA3_TEST(Manifest_ParsesVerifiedLayoutAndLookups)
{
    auto parsed = ra3::asset::Manifest::parse(makeManifest());
    RA3_REQUIRE_OK(parsed);
    const auto& manifest = parsed.value();
    RA3_CHECK_EQ(manifest.header().version, std::uint16_t{6});
    RA3_CHECK_EQ(manifest.entries().size(), std::size_t{1});
    RA3_CHECK_EQ(manifest.name(0), std::string_view("Type:One"));
    RA3_CHECK_EQ(manifest.sourceFile(0), std::string_view("DATA:x.xml"));
    RA3_REQUIRE(manifest.find({0x01020304, 0x05060708}) != nullptr);
    RA3_REQUIRE(manifest.findByName("Type:One") != nullptr);
    RA3_CHECK_EQ(manifest.references(0).size(), std::size_t{1});
    RA3_CHECK_EQ(manifest.references(0)[0].typeId, 0x11112222u);
    RA3_CHECK_EQ(manifest.referencedManifests().size(), std::size_t{1});
    RA3_CHECK_EQ(manifest.referencedManifests()[0].name, std::string("audio.manifest"));
    RA3_CHECK_EQ(manifest.entries()[0].instanceDataOffset, std::uint64_t{4});
    RA3_CHECK_EQ(ra3::asset::cdataPath("data/test", manifest.entries()[0]),
                 std::string("data/test/cdata/01020304.11121314.05060708.15161718.cdata"));
}

RA3_TEST(Manifest_RejectsTruncationAndWrongSizeEquation)
{
    auto headerTruncated = makeManifest();
    headerTruncated.resize(47);
    RA3_CHECK(parseFails(headerTruncated));

    auto tailTruncated = makeManifest();
    tailTruncated.pop_back();
    RA3_CHECK(parseFails(tailTruncated));
}

RA3_TEST(Manifest_RejectsInvalidNameAndReferenceRanges)
{
    auto badName = makeManifest();
    putU32(badName, 72, 1000);
    RA3_CHECK(parseFails(badName));

    auto badReference = makeManifest();
    putU32(badReference, 68, 2);
    RA3_CHECK(parseFails(badReference));
}

RA3_TEST(Manifest_RejectsUnsupportedVersionAndBigEndian)
{
    auto version = makeManifest();
    putU16(version, 2, 5);
    RA3_CHECK(parseFails(version));

    auto bigEndian = makeManifest();
    bigEndian[0] = 1;
    RA3_CHECK(parseFails(bigEndian));
}

RA3_TEST(AssetStream_ValidatesChecksumAndMemberLengths)
{
    auto loaded = ra3::asset::AssetStream::loadDecoded(
        "data/test", makeManifest(), member(3), member(2), member(1));
    RA3_REQUIRE_OK(loaded);
    RA3_CHECK_EQ(loaded.value().instanceData(0).size, std::size_t{3});
    RA3_CHECK_EQ(loaded.value().relocationData(0).size, std::size_t{2});
    RA3_CHECK_EQ(loaded.value().importsData(0).size, std::size_t{1});

    auto wrongChecksum = member(3);
    wrongChecksum[0] = 0;
    RA3_CHECK(!ra3::asset::AssetStream::loadDecoded(
                   "data/test", makeManifest(), wrongChecksum, member(2), member(1))
                   .ok());

    RA3_CHECK(!ra3::asset::AssetStream::loadDecoded(
                   "data/test", makeManifest(), member(4), member(2), member(1))
                   .ok());
}

RA3_TEST(AssetInstance_ParsesRelocationsImportsAndStrings)
{
    std::vector<std::uint8_t> instance(32, 0);
    putU32(instance, 0, 5);
    putU32(instance, 4, 16);
    putU32(instance, 8, 1);
    std::memcpy(instance.data() + 16, "Arial", 5);
    std::vector<std::uint8_t> relocations(8);
    putU32(relocations, 0, 4);
    putU32(relocations, 4, 0xFFFFFFFFu);
    std::vector<std::uint8_t> imports(8);
    putU32(imports, 0, 8);
    putU32(imports, 4, 0xFFFFFFFFu);
    const std::vector<ra3::asset::AssetId> references = {{0x11, 0x22}};

    auto parsed = ra3::asset::AssetInstance::parse(
        {instance.data(), instance.size()},
        {relocations.data(), relocations.size()},
        {imports.data(), imports.size()},
        references, "test/Type:Name", 100, 200);
    RA3_REQUIRE_OK(parsed);
    RA3_CHECK_EQ(parsed.value().relocations().sites().size(), std::size_t{1});
    RA3_REQUIRE(parsed.value().relocationTarget(4).has_value());
    RA3_CHECK_EQ(*parsed.value().relocationTarget(4), 16u);
    RA3_REQUIRE(parsed.value().importAt(8) != nullptr);
    RA3_CHECK_EQ(parsed.value().importAt(8)->target.typeId, 0x11u);
    auto text = parsed.value().readString(0);
    RA3_REQUIRE_OK(text);
    RA3_CHECK_EQ(text.value(), std::string_view("Arial"));
}

RA3_TEST(AssetInstance_RejectsMalformedTablesAndPastEndPointer)
{
    std::vector<std::uint8_t> instance(16, 0);
    putU32(instance, 4, 16);
    std::vector<std::uint8_t> noTerminator(4);
    putU32(noTerminator, 0, 4);
    std::vector<std::uint8_t> empty;
    auto badRelocation = ra3::asset::AssetInstance::parse(
        {instance.data(), instance.size()},
        {noTerminator.data(), noTerminator.size()},
        {empty.data(), empty.size()}, {}, "test/Type:Name");
    RA3_CHECK(!badRelocation.ok());

    std::vector<std::uint8_t> relocation(8);
    putU32(relocation, 0, 4);
    putU32(relocation, 4, 0xFFFFFFFFu);
    auto endPointer = ra3::asset::AssetInstance::parse(
        {instance.data(), instance.size()},
        {relocation.data(), relocation.size()},
        {empty.data(), empty.size()}, {}, "test/Type:Name");
    RA3_REQUIRE_OK(endPointer);
    putU32(instance, 4, 17);
    auto pastEndPointer = ra3::asset::AssetInstance::parse(
        {instance.data(), instance.size()},
        {relocation.data(), relocation.size()},
        {empty.data(), empty.size()}, {}, "test/Type:Name");
    RA3_CHECK(!pastEndPointer.ok());

    std::vector<std::uint8_t> imports(8);
    putU32(imports, 0, 8);
    putU32(imports, 4, 0xFFFFFFFFu);
    putU32(instance, 8, 2);
    auto outOfRangeImport = ra3::asset::AssetInstance::parse(
        {instance.data(), instance.size()},
        {empty.data(), empty.size()},
        {imports.data(), imports.size()}, {{1, 2}}, "test/Type:Name");
    RA3_CHECK(!outOfRangeImport.ok());
}
