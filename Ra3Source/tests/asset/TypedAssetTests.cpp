#include "support/TestHarness.h"

#include <ra3/asset/TypedAsset.h>
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
