#include "support/TestHarness.h"

#include <ra3/asset/TypedAsset.h>
#include <ra3/asset/NameHash.h>
#include <ra3/asset/types/GeneratedTypes.h>

#include <cstring>

RA3_TEST(TypedAsset_FontSubstitutionOverlayAndBoundsChecks)
{
    std::vector<std::uint8_t> bytes(24, 0);
    bytes[4] = 3;
    bytes[8] = 20;
    std::memcpy(bytes.data() + 20, "abc", 4);
    const std::vector<std::uint8_t> relocations = {
        8, 0, 0, 0, 0xFF, 0xFF, 0xFF, 0xFF};
    const std::vector<std::uint8_t> imports;
    const std::vector<ra3::asset::AssetId> references;
    const ra3::asset::AssetId id{
        ra3::asset::AssetTypeTraits<
            ra3::asset::types::FontSubstitution>::typeId,
        1};
    auto instance = ra3::asset::AssetInstance::parse(
        {bytes.data(), bytes.size()},
        {relocations.data(), relocations.size()},
        {imports.data(), imports.size()},
        references, "synthetic", 0, 0, id);
    RA3_REQUIRE_OK(instance);

    auto typed = ra3::asset::asView<
        ra3::asset::types::FontSubstitution>(instance.value());
    RA3_REQUIRE_OK(typed);
    auto text = ra3::asset::resolve(
        typed.value().instance(), typed.value().value().Name, 4);
    RA3_REQUIRE_OK(text);
    RA3_CHECK_EQ(text.value(), std::string_view("abc"));

    auto invalid = ra3::asset::resolveList(
        instance.value(), 2, 23, 4, 12);
    RA3_CHECK(!invalid);
}

RA3_TEST(TypedAsset_PolymorphicPointerChecksTokenAndRelocation)
{
    const std::uint32_t targetTypeId =
        ra3::asset::typeIdFromName("TargetingCombatChainCompare");
    std::vector<std::uint8_t> bytes(8, 0);
    bytes[0] = 4;
    std::memcpy(bytes.data() + 4, &targetTypeId, sizeof(targetTypeId));
    const std::vector<std::uint8_t> relocations = {
        0, 0, 0, 0, 0xFF, 0xFF, 0xFF, 0xFF};
    auto instance = ra3::asset::AssetInstance::parse(
        {bytes.data(), bytes.size()},
        {relocations.data(), relocations.size()},
        {}, {}, "synthetic-polymorphic");
    RA3_REQUIRE_OK(instance);

    std::uint32_t targetIndex = 0xFFFFFFFFu;
    for (std::uint32_t index = 0; ; ++index) {
        const auto* candidate = ra3::asset::findGeneratedTypeByIndex(index);
        if (candidate == nullptr) break;
        if (candidate->name == "TargetingCombatChainCompare") {
            targetIndex = index;
            break;
        }
    }
    RA3_REQUIRE(targetIndex != 0xFFFFFFFFu);
    const ra3::asset::AlternativeInfo alternative{
        "TargetingCombatChainCompare", targetTypeId, targetIndex};
    ra3::asset::FieldInfo field;
    field.alternatives = &alternative;
    field.alternativeCount = 1;
    auto resolved = ra3::asset::resolve(
        instance.value(), ra3::asset::PolymorphicPointer{4}, 0, field);
    RA3_REQUIRE_OK(resolved);
    RA3_REQUIRE(resolved.value().has_value());
    RA3_CHECK_EQ(
        resolved.value()->type->name,
        std::string_view("TargetingCombatChainCompare"));

    const std::uint32_t unknown = 0xDEADBEEFu;
    std::memcpy(bytes.data() + 4, &unknown, sizeof(unknown));
    auto rejected = ra3::asset::resolve(
        instance.value(), ra3::asset::PolymorphicPointer{4}, 0, field);
    RA3_CHECK(!rejected);

    std::vector<std::uint8_t> listBytes(16, 0);
    listBytes[0] = 1;
    listBytes[4] = 8;
    listBytes[8] = 12;
    std::memcpy(listBytes.data() + 12, &targetTypeId, sizeof(targetTypeId));
    const std::vector<std::uint8_t> listRelocations = {
        4, 0, 0, 0, 8, 0, 0, 0, 0xFF, 0xFF, 0xFF, 0xFF};
    auto listInstance = ra3::asset::AssetInstance::parse(
        {listBytes.data(), listBytes.size()},
        {listRelocations.data(), listRelocations.size()},
        {}, {}, "synthetic-polymorphic-list");
    RA3_REQUIRE_OK(listInstance);
    auto list = ra3::asset::resolve(
        listInstance.value(), ra3::asset::PolymorphicList{1, 8}, 0, field);
    RA3_REQUIRE_OK(list);
    RA3_REQUIRE(list.value().size() == 1);
    RA3_CHECK_EQ(list.value()[0].offset, 12u);
}
