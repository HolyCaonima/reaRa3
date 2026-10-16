#include "ra3/common/ByteString.h"

#include <algorithm>

namespace ra3::common {

namespace {

int signedByteCompare(std::uint8_t a, std::uint8_t b)
{
    const auto left = static_cast<std::int8_t>(a);
    const auto right = static_cast<std::int8_t>(b);
    return left > right ? 1 : (left < right ? -1 : 0);
}

std::uint32_t decodeUtf8Value(const std::uint8_t* text)
{
    const std::uint32_t first = text[0];
    if (first <= 0x7F) {
        return first;
    }
    if ((first & 0xE0) == 0xC0) {
        return ((first & 0x1F) << 6) | (text[1] & 0x3F);
    }
    if ((first & 0xF0) == 0xE0) {
        return ((first & 0x0F) << 12) | ((text[1] & 0x3F) << 6) | (text[2] & 0x3F);
    }
    return ((first & 0x07) << 18) | ((text[1] & 0x3F) << 12)
        | ((text[2] & 0x3F) << 6) | (text[3] & 0x3F);
}

}  // namespace

// ra3_1.0.game 0x009F3470: count zero permits null pointers.
char* copyStringLimited(char* destination, const char* source, std::uint32_t count)
{
    char* const result = destination;
    while (count != 0) {
        --count;
        const char value = *source;
        if (value == '\0') {
            *destination = '\0';
            break;
        }
        *destination++ = value;
        ++source;
    }
    return result;
}

// ra3_1.0.game 0x009F34B0: count zero leaves the destination untouched.
char* appendStringLimited(char* destination, const char* source, std::uint32_t count)
{
    char* const result = destination;
    if (count == 0) {
        return result;
    }
    while (*destination != '\0') {
        ++destination;
    }
    while (count != 0) {
        --count;
        const char value = *source++;
        if (value == '\0') {
            break;
        }
        *destination++ = value;
    }
    if (*destination != '\0') {
        *destination = '\0';
    }
    return result;
}

// ra3_1.0.game 0x009F3500: ordering uses signed bytes.
int compareStringsLimited(const char* a, const char* b, std::uint32_t count)
{
    if (count == 0) {
        return 0;
    }
    while (*a != '\0' && *a == *b) {
        if (--count == 0) {
            return 0;
        }
        ++a;
        ++b;
    }
    return signedByteCompare(static_cast<std::uint8_t>(*a), static_cast<std::uint8_t>(*b));
}

// ra3_1.0.game 0x009F3560: only opposite-case ASCII pairs are folded.
int compareStringsCaseInsensitiveLimited(const char* a, const char* b, std::uint32_t count)
{
    if (count == 0) {
        return 0;
    }
    while (true) {
        std::uint8_t left = static_cast<std::uint8_t>(*a++);
        const std::uint8_t right = static_cast<std::uint8_t>(*b++);
        if (left >= 'A' && left <= 'Z' && right >= 'a' && right <= 'z') {
            left = static_cast<std::uint8_t>(left + 0x20);
        } else if (left >= 'a' && left <= 'z' && right >= 'A' && right <= 'Z') {
            left = static_cast<std::uint8_t>(left - 0x20);
        }
        if (left == 0 || right == 0 || left != right || --count == 0) {
            return signedByteCompare(left, right);
        }
    }
}

// ra3_1.0.game 0x009F35E0.
char* copyString(char* destination, const char* source)
{
    char* const result = destination;
    while ((*destination++ = *source++) != '\0') {
    }
    return result;
}

// ra3_1.0.game 0x009F3610: ordering uses signed bytes.
int compareStrings(const char* a, const char* b)
{
    while (*a != '\0' && *a == *b) {
        ++a;
        ++b;
    }
    return signedByteCompare(static_cast<std::uint8_t>(*a), static_cast<std::uint8_t>(*b));
}

// ra3_1.0.game 0x009F36C0.
std::uint32_t stringLength(const char* text)
{
    const char* end = text;
    while (*end != '\0') {
        ++end;
    }
    return static_cast<std::uint32_t>(end - text);
}

// ra3_1.0.game 0x009F36E0: folds ASCII A-Z only.
char* copyStringLowercase(char* destination, const char* source)
{
    char* const result = destination;
    while (*source != '\0') {
        std::uint8_t value = static_cast<std::uint8_t>(*source++);
        if (value >= 'A' && value <= 'Z') {
            value = static_cast<std::uint8_t>(value + 0x20);
        }
        *destination++ = static_cast<char>(value);
    }
    *destination = '\0';
    return result;
}

// ra3_1.0.game 0x009F3720.
const char* findLastCharacter(const char* text, std::uint8_t character)
{
    const char* end = text;
    while (*end != '\0') {
        ++end;
    }
    do {
        if (static_cast<std::uint8_t>(*end) == character) {
            return end;
        }
    } while (end-- != text);
    return nullptr;
}

// ra3_1.0.game 0x009F3F10: widens bytes without sign extension.
std::uint16_t* widenBytes(std::uint16_t* destination, const char* source)
{
    std::uint16_t* const result = destination;
    while (*source != '\0') {
        *destination++ = static_cast<std::uint8_t>(*source++);
    }
    *destination = 0;
    return result;
}

// ra3_1.0.game 0x004C9EA0.
int compareBytes(const std::uint8_t* a, const std::uint8_t* b, std::uint32_t length)
{
    for (std::uint32_t i = 0; i < length; ++i) {
        if (a[i] != b[i]) {
            return a[i] < b[i] ? -1 : 1;
        }
    }
    return 0;
}

// ra3_1.0.game 0x004CB3A0: the original string stores a uint16 length before data.
int compareLengthPrefixedToCString(
    const char* prefixedData, std::uint16_t prefixedLength, const char* cString)
{
    const std::uint32_t cLength = cString == nullptr ? 0 : stringLength(cString);
    static constexpr char empty = '\0';
    const auto* left = reinterpret_cast<const std::uint8_t*>(
        prefixedData == nullptr ? &empty : prefixedData);
    const auto* right =
        reinterpret_cast<const std::uint8_t*>(cString == nullptr ? &empty : cString);
    const int compared = compareBytes(left, right, std::min<std::uint32_t>(prefixedLength, cLength));
    return compared != 0 ? compared : static_cast<int>(prefixedLength) - static_cast<int>(cLength);
}

// ra3_1.0.game 0x004CB400: each original string stores a uint16 length before data.
int compareLengthPrefixed(const char* a, std::uint16_t aLength,
                          const char* b, std::uint16_t bLength)
{
    static constexpr char empty = '\0';
    const auto* left =
        reinterpret_cast<const std::uint8_t*>(a == nullptr ? &empty : a);
    const auto* right =
        reinterpret_cast<const std::uint8_t*>(b == nullptr ? &empty : b);
    const int compared = compareBytes(left, right, std::min(aLength, bLength));
    return compared != 0 ? compared : static_cast<int>(aLength) - static_cast<int>(bLength);
}

// ra3_1.0.game 0x00A28290: signed input bytes and a nonzero low-word sentinel.
std::uint32_t hashLowercaseFnv1a(const char* text)
{
    std::uint32_t hash = 0x811C9DC5;
    while (*text != '\0') {
        std::int32_t value = static_cast<std::int8_t>(*text++);
        if (value >= 'A' && value <= 'Z') {
            value += 0x20;
        }
        hash = (hash ^ static_cast<std::uint32_t>(value)) * 0x01000193;
    }
    return (hash & 0xFFFF) == 0 ? (hash | 0x4567) : hash;
}

// ra3_1.0.game 0x00A282D0: permissive legacy UTF-8 decoding without validation.
std::uint32_t decodeUtf8(const std::uint8_t* text)
{
    return decodeUtf8Value(text);
}

// ra3_1.0.game 0x00A28350: signed comparisons preserve negative code-point behavior.
void encodeUtf8(std::uint8_t* destination, std::uint32_t codePoint)
{
    const auto signedPoint = static_cast<std::int32_t>(codePoint);
    if (signedPoint < 0x80) {
        destination[0] = static_cast<std::uint8_t>(codePoint);
    } else if (signedPoint < 0x800) {
        destination[0] = static_cast<std::uint8_t>((codePoint >> 6) | 0xC0);
        destination[1] = static_cast<std::uint8_t>((codePoint & 0x3F) | 0x80);
    } else if (signedPoint < 0x10000) {
        destination[0] = static_cast<std::uint8_t>((codePoint >> 12) | 0xE0);
        destination[1] = static_cast<std::uint8_t>(((codePoint >> 6) & 0x3F) | 0x80);
        destination[2] = static_cast<std::uint8_t>((codePoint & 0x3F) | 0x80);
    } else {
        destination[0] = static_cast<std::uint8_t>((codePoint >> 18) | 0xF0);
        destination[1] = static_cast<std::uint8_t>(((codePoint >> 12) & 0x3F) | 0x80);
        destination[2] = static_cast<std::uint8_t>(((codePoint >> 6) & 0x3F) | 0x80);
        destination[3] = static_cast<std::uint8_t>((codePoint & 0x3F) | 0x80);
    }
}

// ra3_1.0.game 0x00A284B0: permissive legacy UTF-8 decoding without validation.
const std::uint8_t* decodeUtf8AndAdvance(const std::uint8_t* text, std::uint32_t& codePoint)
{
    codePoint = decodeUtf8Value(text);
    const std::uint8_t first = text[0];
    if (first <= 0x7F) {
        return text + 1;
    }
    if ((first & 0xE0) == 0xC0) {
        return text + 2;
    }
    if ((first & 0xF0) == 0xE0) {
        return text + 3;
    }
    return text + 4;
}

}  // namespace ra3::common
