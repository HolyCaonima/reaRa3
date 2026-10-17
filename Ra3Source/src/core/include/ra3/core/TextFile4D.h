#pragma once

#include <cstdint>

namespace ra3::core {

// 12-byte line entry in ra3_1.0.game TextFile vector at +0x04..+0x08.
struct TextFileLineEntry4D {
    const char* text;
    std::uint32_t reserved04;
    std::uint32_t reserved08;
};
static_assert(sizeof(TextFileLineEntry4D) == 0x0C);

// 0x004D05A0 - TextFile::getLine(int index) const
// Reads the 12-byte line vector [m_linesBegin (+0x04), m_linesEnd (+0x08)) and
// returns entry[index].text when index < (m_linesEnd - m_linesBegin) / 12, else nullptr.
const char* getTextFileLine4D(const void* textFile, std::int32_t lineIndex) noexcept;

}  // namespace ra3::core
