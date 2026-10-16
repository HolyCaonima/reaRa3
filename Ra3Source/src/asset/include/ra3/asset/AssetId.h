#pragma once

#include <cstddef>
#include <cstdint>
#include <functional>

namespace ra3::asset {

struct AssetId {
    std::uint32_t typeId = 0;
    std::uint32_t instanceId = 0;

    friend bool operator==(AssetId a, AssetId b)
    {
        return a.typeId == b.typeId && a.instanceId == b.instanceId;
    }
    friend bool operator!=(AssetId a, AssetId b) { return !(a == b); }
    friend bool operator<(AssetId a, AssetId b)
    {
        return a.typeId < b.typeId || (a.typeId == b.typeId && a.instanceId < b.instanceId);
    }
};

}  // namespace ra3::asset

namespace std {

template <>
struct hash<ra3::asset::AssetId> {
    std::size_t operator()(ra3::asset::AssetId id) const noexcept
    {
        return (static_cast<std::size_t>(id.typeId) << 1) ^
               static_cast<std::size_t>(id.instanceId);
    }
};

}  // namespace std
