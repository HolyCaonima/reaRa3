#pragma once

#include <cstddef>
#include <cstdint>

namespace ra3::common {

char* copyStringLimited(char* destination, const char* source, std::uint32_t count);
char* appendStringLimited(char* destination, const char* source, std::uint32_t count);
int compareStringsLimited(const char* a, const char* b, std::uint32_t count);
int compareStringsCaseInsensitiveLimited(const char* a, const char* b, std::uint32_t count);
char* copyString(char* destination, const char* source);
int compareStrings(const char* a, const char* b);
std::uint32_t stringLength(const char* text);
char* copyStringLowercase(char* destination, const char* source);
const char* findLastCharacter(const char* text, std::uint8_t character);
std::uint16_t* widenBytes(std::uint16_t* destination, const char* source);

int compareBytes(const std::uint8_t* a, const std::uint8_t* b, std::uint32_t length);
int compareLengthPrefixedToCString(
    const char* prefixedData, std::uint16_t prefixedLength, const char* cString);
int compareLengthPrefixed(const char* a, std::uint16_t aLength,
                          const char* b, std::uint16_t bLength);

std::uint32_t hashLowercaseFnv1a(const char* text);
std::uint32_t decodeUtf8(const std::uint8_t* text);
void encodeUtf8(std::uint8_t* destination, std::uint32_t codePoint);
const std::uint8_t* decodeUtf8AndAdvance(const std::uint8_t* text, std::uint32_t& codePoint);

}  // namespace ra3::common
