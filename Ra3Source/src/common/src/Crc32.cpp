#include <ra3/common/Crc32.h>

#include <array>

namespace ra3::common {
namespace {

constexpr std::array<std::uint32_t, 256> makeTable() noexcept
{
    std::array<std::uint32_t, 256> table{};
    for (std::uint32_t index = 0; index < table.size(); ++index) {
        std::uint32_t value = index;
        for (int bit = 0; bit < 8; ++bit) {
            value = (value >> 1) ^ ((value & 1) != 0 ? 0xEDB88320U : 0U);
        }
        table[index] = value;
    }
    return table;
}

constexpr auto table = makeTable();

}  // namespace

// ra3_1.0.game 0x0092A0D0.
std::uint32_t crc32CString(const char* text) noexcept
{
    if (text == nullptr || *text == '\0') {
        return 0;
    }
    std::uint32_t crc = 0xFFFFFFFFU;
    do {
        crc = (crc >> 8) ^ table[(crc ^ static_cast<std::uint8_t>(*text)) & 0xFF];
        ++text;
    } while (*text != '\0');
    return ~crc;
}

}  // namespace ra3::common
