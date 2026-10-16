// CSF layout reference: Generals/Code/GameEngine/Source/GameClient/GameText.cpp
// from EA's GPLv3 Generals source release. This is an original parser for the
// publicly documented format.

#include <ra3/text/Csf.h>

#include <ra3/core/ByteOrder.h>
#include <ra3/core/StringUtil.h>

#include <cstring>
#include <limits>
#include <string>
#include <utility>

namespace ra3::text {
namespace {

class Reader {
public:
    Reader(const std::uint8_t* data, std::size_t size) : m_data(data), m_size(size) {}

    std::size_t remaining() const { return m_size - m_position; }
    std::size_t position() const { return m_position; }

    bool readU32(std::uint32_t& value)
    {
        if (remaining() < sizeof(std::uint32_t)) {
            return false;
        }
        value = readU32LE(m_data + m_position);
        m_position += sizeof(std::uint32_t);
        return true;
    }

    bool readBytes(std::size_t count, const std::uint8_t*& bytes)
    {
        if (count > remaining()) {
            return false;
        }
        bytes = m_data + m_position;
        m_position += count;
        return true;
    }

private:
    const std::uint8_t* m_data;
    std::size_t m_size;
    std::size_t m_position = 0;
};

Error formatError(std::size_t offset, std::string message)
{
    return makeError("CSF offset " + std::to_string(offset) + ": " + std::move(message));
}

void appendUtf8(std::string& output, std::uint32_t codePoint)
{
    if (codePoint <= 0x7F) {
        output.push_back(static_cast<char>(codePoint));
    } else if (codePoint <= 0x7FF) {
        output.push_back(static_cast<char>(0xC0 | (codePoint >> 6)));
        output.push_back(static_cast<char>(0x80 | (codePoint & 0x3F)));
    } else if (codePoint <= 0xFFFF) {
        output.push_back(static_cast<char>(0xE0 | (codePoint >> 12)));
        output.push_back(static_cast<char>(0x80 | ((codePoint >> 6) & 0x3F)));
        output.push_back(static_cast<char>(0x80 | (codePoint & 0x3F)));
    } else {
        output.push_back(static_cast<char>(0xF0 | (codePoint >> 18)));
        output.push_back(static_cast<char>(0x80 | ((codePoint >> 12) & 0x3F)));
        output.push_back(static_cast<char>(0x80 | ((codePoint >> 6) & 0x3F)));
        output.push_back(static_cast<char>(0x80 | (codePoint & 0x3F)));
    }
}

Result<std::string> decodeInvertedUtf16(const std::uint8_t* bytes, std::size_t codeUnitCount)
{
    std::string output;
    output.reserve(codeUnitCount);
    for (std::size_t i = 0; i < codeUnitCount; ++i) {
        const std::uint16_t codeUnit =
            static_cast<std::uint16_t>(~(std::uint16_t(bytes[i * 2]) |
                                         (std::uint16_t(bytes[i * 2 + 1]) << 8)));
        std::uint32_t codePoint = codeUnit;
        if (codeUnit >= 0xD800 && codeUnit <= 0xDBFF) {
            if (++i >= codeUnitCount) {
                return makeError("CSF string ends with an unmatched UTF-16 high surrogate");
            }
            const std::uint16_t low =
                static_cast<std::uint16_t>(~(std::uint16_t(bytes[i * 2]) |
                                             (std::uint16_t(bytes[i * 2 + 1]) << 8)));
            if (low < 0xDC00 || low > 0xDFFF) {
                return makeError("CSF string contains an unmatched UTF-16 high surrogate");
            }
            codePoint = 0x10000 + ((std::uint32_t(codeUnit) - 0xD800) << 10) +
                        (std::uint32_t(low) - 0xDC00);
        } else if (codeUnit >= 0xDC00 && codeUnit <= 0xDFFF) {
            return makeError("CSF string contains an unmatched UTF-16 low surrogate");
        }
        appendUtf8(output, codePoint);
    }
    return output;
}

bool hasTag(const std::uint8_t* bytes, const char (&tag)[5])
{
    return std::memcmp(bytes, tag, 4) == 0;
}

}  // namespace

const CsfEntry* CsfStrings::find(std::string_view label) const
{
    const auto found = m_index.find(lowerAscii(label));
    if (found == m_index.end()) {
        return nullptr;
    }
    return &m_entries[found->second];
}

std::optional<std::string_view> CsfStrings::get(std::string_view label) const
{
    const CsfEntry* entry = find(label);
    if (entry == nullptr) {
        return std::nullopt;
    }
    return entry->text;
}

Result<CsfStrings> parseCsf(const std::uint8_t* data, std::size_t size)
{
    constexpr std::size_t headerSize = 24;
    if (data == nullptr && size != 0) {
        return makeError("CSF input pointer is null");
    }
    if (size < headerSize) {
        return makeError("CSF header is truncated");
    }
    if (std::memcmp(data, " FSC", 4) != 0) {
        return makeError("CSF magic is not \" FSC\"");
    }

    Reader reader(data + 4, size - 4);
    std::uint32_t version = 0;
    std::uint32_t declaredLabels = 0;
    std::uint32_t declaredStrings = 0;
    std::uint32_t reserved = 0;
    std::uint32_t languageId = 0;
    if (!reader.readU32(version) || !reader.readU32(declaredLabels) ||
        !reader.readU32(declaredStrings) || !reader.readU32(reserved) ||
        !reader.readU32(languageId)) {
        return makeError("CSF header is truncated");
    }
    (void)reserved;

    const std::size_t bodySize = size - headerSize;
    if (declaredLabels > bodySize / 12 || declaredStrings > bodySize / 8) {
        return makeError("CSF header counts cannot fit in the file");
    }

    CsfStrings result;
    result.m_version = version;
    result.m_languageId = version >= 2 ? languageId : 0;
    result.m_entries.reserve(declaredLabels);

    std::uint32_t stringsRead = 0;
    for (std::uint32_t labelIndex = 0; labelIndex < declaredLabels; ++labelIndex) {
        const std::size_t labelOffset = reader.position() + 4;
        const std::uint8_t* tag = nullptr;
        std::uint32_t stringCount = 0;
        std::uint32_t labelLength = 0;
        if (!reader.readBytes(4, tag) || !hasTag(tag, " LBL") || !reader.readU32(stringCount) ||
            !reader.readU32(labelLength)) {
            return formatError(labelOffset, "invalid or truncated label record");
        }
        const std::uint8_t* labelBytes = nullptr;
        if (!reader.readBytes(labelLength, labelBytes)) {
            return formatError(labelOffset, "label text is truncated");
        }

        CsfEntry entry;
        entry.label.assign(reinterpret_cast<const char*>(labelBytes), labelLength);
        const std::string foldedLabel = lowerAscii(entry.label);
        if (result.m_index.find(foldedLabel) != result.m_index.end()) {
            return formatError(labelOffset, "duplicate label ignoring ASCII case: " + entry.label);
        }

        if (stringCount > std::numeric_limits<std::uint32_t>::max() - stringsRead) {
            return formatError(labelOffset, "string count overflows");
        }
        stringsRead += stringCount;
        if (stringsRead > declaredStrings) {
            return formatError(labelOffset, "label string counts exceed the header count");
        }

        for (std::uint32_t stringIndex = 0; stringIndex < stringCount; ++stringIndex) {
            const std::size_t stringOffset = reader.position() + 4;
            if (!reader.readBytes(4, tag)) {
                return formatError(stringOffset, "string tag is truncated");
            }
            const bool withExtra = hasTag(tag, "WRTS");
            if (!withExtra && !hasTag(tag, " RTS")) {
                return formatError(stringOffset, "unknown string record tag");
            }

            std::uint32_t codeUnitCount = 0;
            if (!reader.readU32(codeUnitCount) || codeUnitCount > reader.remaining() / 2) {
                return formatError(stringOffset, "UTF-16 text is truncated");
            }
            const std::uint8_t* textBytes = nullptr;
            if (!reader.readBytes(std::size_t(codeUnitCount) * 2, textBytes)) {
                return formatError(stringOffset, "UTF-16 text is truncated");
            }
            auto text = decodeInvertedUtf16(textBytes, codeUnitCount);
            if (!text) {
                return formatError(stringOffset, text.error().message);
            }

            std::string extra;
            if (withExtra) {
                std::uint32_t extraLength = 0;
                const std::uint8_t* extraBytes = nullptr;
                if (!reader.readU32(extraLength) || !reader.readBytes(extraLength, extraBytes)) {
                    return formatError(stringOffset, "WRTS extra string is truncated");
                }
                extra.assign(reinterpret_cast<const char*>(extraBytes), extraLength);
            }
            if (stringIndex == 0) {
                entry.text = std::move(text).value();
                entry.extra = std::move(extra);
            }
        }
        result.m_index.emplace(foldedLabel, result.m_entries.size());
        result.m_entries.push_back(std::move(entry));
    }

    if (stringsRead != declaredStrings) {
        return makeError("CSF label string counts do not match the header count");
    }
    if (reader.remaining() != 0) {
        return formatError(reader.position() + 4, "trailing bytes after the declared labels");
    }
    return result;
}

Result<CsfStrings> parseCsf(const std::vector<std::uint8_t>& data)
{
    return parseCsf(data.data(), data.size());
}

}  // namespace ra3::text
