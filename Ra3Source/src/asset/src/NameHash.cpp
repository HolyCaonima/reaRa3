#include "ra3/asset/NameHash.h"

#include <cstddef>

namespace ra3::asset {
namespace {

template <bool Fold>
std::uint32_t byteAt(const char* p)
{
    char c = *p;
    if constexpr (Fold) {
        if (c >= 'A' && c <= 'Z') {
            c = static_cast<char>(c - 'A' + 'a');
        }
    }
    return static_cast<std::uint32_t>(static_cast<std::int32_t>(static_cast<signed char>(c)));
}

template <bool Fold>
std::uint32_t pairAt(const char* p)
{
    return (byteAt<Fold>(p + 1) << 8) | byteAt<Fold>(p);
}

template <bool Fold>
std::uint32_t mix(std::string_view bytes, std::uint32_t seed, bool finalize)
{
    std::uint32_t h = seed;
    const char* p = bytes.data();
    if (p != nullptr && !bytes.empty()) {
        for (std::size_t blocks = bytes.size() >> 2; blocks != 0; --blocks, p += 4) {
            h += pairAt<Fold>(p);
            h ^= (pairAt<Fold>(p + 2) ^ (h << 5)) << 11;
            h += h >> 11;
        }
        switch (bytes.size() & 3) {
        case 1:
            h += byteAt<Fold>(p);
            h ^= h << 10;
            h += h >> 1;
            break;
        case 2:
            h += pairAt<Fold>(p);
            h ^= h << 11;
            h += h >> 17;
            break;
        case 3:
            h += pairAt<Fold>(p);
            h ^= ((byteAt<Fold>(p + 2) << 2) ^ h) << 16;
            h += h >> 11;
            break;
        default:
            break;
        }
    }
    if (finalize) {
        h ^= h << 3;
        h += h >> 5;
        h ^= h << 2;
        h += h >> 15;
        h ^= h << 10;
    }
    return h;
}

}  // namespace

std::uint32_t hashName(std::string_view bytes, std::uint32_t seed, bool finalize)
{
    return mix<true>(bytes, seed, finalize);
}

std::uint32_t instanceIdFromName(std::string_view name)
{
    const auto length = static_cast<std::uint32_t>(name.size());
    return mix<true>(name, length, true);
}

std::uint32_t typeIdFromName(std::string_view typeName)
{
    const auto length = static_cast<std::uint32_t>(typeName.size());
    return mix<false>(typeName, length, true);
}

}  // namespace ra3::asset
