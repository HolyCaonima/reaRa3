#include <ra3/asset/TypedAsset.h>
#include <ra3/asset/types/GeneratedTypes.h>

#include <algorithm>
#include <limits>

namespace ra3::asset {
namespace {

Result<ByteView> resolveRange(
    const AssetInstance& instance,
    std::uint32_t offset,
    std::uint64_t size,
    std::uint32_t relocationSite)
{
    if (offset == 0) {
        if (size == 0) return ByteView{};
        return makeError("non-empty compiled range has a null offset");
    }
    if (!instance.relocations().contains(relocationSite)) {
        return makeError("non-null compiled offset is absent from the relocation table");
    }
    const ByteView bytes = instance.bytes();
    if (offset > bytes.size || size > bytes.size - offset) {
        return makeError("compiled range is outside the asset instance");
    }
    return ByteView{bytes.data + offset, static_cast<std::size_t>(size)};
}

}  // namespace

Result<std::string_view> resolve(
    const AssetInstance& instance, const CompiledString& value, std::uint32_t site)
{
    auto bytes = resolveRange(instance, value.offset, value.length, site + 4);
    if (!bytes) return bytes.error();
    if (value.offset == 0) return std::string_view{};
    if (value.offset + static_cast<std::uint64_t>(value.length) >= instance.bytes().size ||
        instance.bytes().data[value.offset + value.length] != 0) {
        return makeError("compiled string is not NUL-terminated at its declared length");
    }
    return std::string_view(
        reinterpret_cast<const char*>(bytes.value().data), bytes.value().size);
}

Result<std::optional<std::string_view>> resolve(
    const AssetInstance& instance, const FileReference& value, std::uint32_t site)
{
    if (value.offset == 0) return std::optional<std::string_view>{};
    auto bytes = resolveRange(instance, value.offset, 1, site);
    if (!bytes) return bytes.error();
    const std::uint8_t* begin = bytes.value().data;
    const std::uint8_t* end = instance.bytes().data + instance.bytes().size;
    const auto terminator = std::find(begin, end, std::uint8_t{0});
    if (terminator == end) {
        return makeError("compiled file reference has no NUL terminator");
    }
    return std::optional<std::string_view>(std::string_view(
        reinterpret_cast<const char*>(begin),
        static_cast<std::size_t>(terminator - begin)));
}

Result<ByteView> resolve(
    const AssetInstance& instance, const DataBlob& value, std::uint32_t site)
{
    return resolveRange(instance, value.offset, value.size, site);
}

Result<ByteView> resolveList(
    const AssetInstance& instance,
    std::uint32_t count,
    std::uint32_t offset,
    std::uint32_t elementSize,
    std::uint32_t site)
{
    if (count != 0 && elementSize == 0) {
        return makeError("compiled list has a zero-sized element type");
    }
    return resolveRange(
        instance, offset,
        static_cast<std::uint64_t>(count) * elementSize, site + 4);
}

Result<std::optional<AssetId>> resolve(
    const AssetInstance& instance, const AssetReference& value, std::uint32_t site)
{
    if (value.index == 0) return std::optional<AssetId>{};
    const Import* imported = instance.importAt(site);
    if (imported == nullptr || imported->referenceIndex != value.index) {
        return makeError("compiled asset reference is absent from the import table");
    }
    return std::optional<AssetId>(imported->target);
}

const TypeInfo* findGeneratedType(std::uint32_t typeId)
{
    for (const TypeInfo& type : generated::kTypes) {
        if (type.typeId == typeId) return &type;
    }
    return nullptr;
}

const TypeInfo* findGeneratedType(std::string_view name)
{
    for (const TypeInfo& type : generated::kAllTypes) {
        if (type.name == name) return &type;
    }
    return nullptr;
}

const EnumInfo* findGeneratedEnum(std::string_view name)
{
    for (const EnumInfo& item : generated::kEnums) {
        if (item.name == name) return &item;
    }
    return nullptr;
}

const TypeInfo* generatedTypesBegin()
{
    return generated::kTypes.data();
}

std::size_t generatedTypeCount()
{
    return std::size(generated::kTypes);
}

}  // namespace ra3::asset
