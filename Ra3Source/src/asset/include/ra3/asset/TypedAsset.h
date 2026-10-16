#pragma once

#include <ra3/asset/AssetInstance.h>
#include <ra3/core/Result.h>

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <optional>
#include <string_view>
#include <type_traits>

namespace ra3::asset {

struct RuntimeToken {
    std::uint32_t raw = 0;
};

struct CompiledString {
    std::uint32_t length = 0;
    std::uint32_t offset = 0;
};

template <typename T = void>
struct CompiledList {
    std::uint32_t count = 0;
    std::uint32_t offset = 0;
};

struct FileReference {
    std::uint32_t offset = 0;
};

struct DataBlob {
    std::uint32_t offset = 0;
    std::uint32_t size = 0;
};

struct AssetReference {
    std::uint32_t index = 0;
};

enum class FieldKind : std::uint8_t {
    RuntimeToken,
    Unsigned,
    Signed,
    Float,
    Boolean,
    Enum,
    String,
    FileReference,
    Blob,
    AssetReference,
    Struct,
    List,
    SimpleList,
};

struct EnumValueInfo {
    std::string_view name;
    std::uint32_t value = 0;
};

struct EnumInfo {
    std::string_view name;
    const EnumValueInfo* values = nullptr;
    std::size_t valueCount = 0;
};

struct FieldInfo {
    std::string_view name;
    std::uint32_t offset = 0;
    std::uint32_t size = 0;
    FieldKind kind = FieldKind::Unsigned;
    std::string_view elementType;
    std::uint32_t elementSize = 0;
};

struct TypeInfo {
    std::string_view name;
    std::uint32_t typeId = 0;
    std::uint32_t size = 0;
    const FieldInfo* fields = nullptr;
    std::size_t fieldCount = 0;
};

template <typename T>
struct AssetTypeTraits;

template <typename T>
class TypedInstance {
public:
    const T& value() const { return m_value; }
    const AssetInstance& instance() const { return *m_instance; }

private:
    template <typename U>
    friend Result<TypedInstance<U>> asView(const AssetInstance&);

    const AssetInstance* m_instance = nullptr;
    T m_value{};
};

template <typename T>
Result<TypedInstance<T>> asView(const AssetInstance& instance)
{
    static_assert(std::is_trivially_copyable_v<T>, "compiled asset types must be trivially copyable");
    if (instance.id().typeId != AssetTypeTraits<T>::typeId) {
        return makeError("asset typeId does not match requested typed view");
    }
    if (instance.bytes().size < sizeof(T)) {
        return makeError("asset instance is smaller than requested typed view");
    }
    TypedInstance<T> result;
    result.m_instance = &instance;
    std::memcpy(&result.m_value, instance.bytes().data, sizeof(T));
    return result;
}

Result<std::string_view> resolve(
    const AssetInstance& instance, const CompiledString& value, std::uint32_t site);
Result<std::optional<std::string_view>> resolve(
    const AssetInstance& instance, const FileReference& value, std::uint32_t site);
Result<ByteView> resolve(
    const AssetInstance& instance, const DataBlob& value, std::uint32_t site);
Result<ByteView> resolveList(
    const AssetInstance& instance,
    std::uint32_t count,
    std::uint32_t offset,
    std::uint32_t elementSize,
    std::uint32_t site);
Result<std::optional<AssetId>> resolve(
    const AssetInstance& instance, const AssetReference& value, std::uint32_t site);

const TypeInfo* findGeneratedType(std::uint32_t typeId);
const TypeInfo* findGeneratedType(std::string_view name);
const EnumInfo* findGeneratedEnum(std::string_view name);
const TypeInfo* generatedTypesBegin();
std::size_t generatedTypeCount();

}  // namespace ra3::asset
