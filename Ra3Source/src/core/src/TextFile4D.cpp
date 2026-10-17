#include <ra3/core/TextFile4D.h>

#include <cstddef>
#include <cstdint>
#include <cstring>

namespace ra3::core {
namespace {

template <class T>
T load(const void* base, std::ptrdiff_t offset) noexcept {
    T value{};
    std::memcpy(&value, static_cast<const std::byte*>(base) + offset, sizeof(value));
    return value;
}

}  // namespace

// 0x004D05A0
const char* getTextFileLine4D(const void* textFile, std::int32_t lineIndex) noexcept {
    const auto beginAddr = load<std::uint32_t>(textFile, 0x04);
    const auto endAddr = load<std::uint32_t>(textFile, 0x08);
    const auto byteDiff = static_cast<std::int32_t>(endAddr - beginAddr);
    const auto prod = static_cast<std::int64_t>(byteDiff) * static_cast<std::int64_t>(0x2AAAAAABLL);
    const auto hi = static_cast<std::int32_t>(static_cast<std::uint64_t>(prod) >> 32U);
    const auto shifted = hi >> 1;
    const auto count =
        shifted + static_cast<std::int32_t>(static_cast<std::uint32_t>(shifted) >> 31U);

    if (lineIndex >= count) {
        return nullptr;
    }

    const auto entryAddr =
        static_cast<std::uintptr_t>(beginAddr + static_cast<std::uint32_t>(lineIndex) * 12U);
    return load<const char*>(reinterpret_cast<const void*>(entryAddr), 0x00);
}

}  // namespace ra3::core
