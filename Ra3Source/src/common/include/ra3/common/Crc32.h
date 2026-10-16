#pragma once

#include <cstdint>

namespace ra3::common {

std::uint32_t crc32CString(const char* text) noexcept;

}  // namespace ra3::common
