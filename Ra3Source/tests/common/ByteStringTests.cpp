#include "support/TestHarness.h"

#include <ra3/common/ByteString.h>

#include <cstdint>
#include <cstring>

RA3_TEST(byteStringPortsPreserveSignedOrderingAndLimits)
{
    const char high[] = "\x80";
    RA3_CHECK_EQ(ra3::common::compareStrings(high, "\x7F"), -1);
    RA3_CHECK_EQ(ra3::common::compareStringsLimited("abcX", "abcY", 3), 0);
    RA3_CHECK_EQ(ra3::common::compareStringsCaseInsensitiveLimited("Az_", "aZ_", 3), 0);
    RA3_CHECK_EQ(ra3::common::compareBytes(
                     reinterpret_cast<const std::uint8_t*>("a\xFF"),
                     reinterpret_cast<const std::uint8_t*>("a\x80"), 2),
                 1);
}

RA3_TEST(byteStringPortsCopyFoldFindAndWiden)
{
    char buffer[16] = "Hi";
    ra3::common::appendStringLimited(buffer, "There", 3);
    RA3_CHECK_EQ(std::strcmp(buffer, "HiThe"), 0);
    ra3::common::copyStringLowercase(buffer, "A_\xC0");
    RA3_CHECK_EQ(std::memcmp(buffer, "a_\xC0", 4), 0);
    const char path[] = "a/b/c";
    RA3_CHECK_EQ(ra3::common::findLastCharacter(path, '/'), path + 3);

    std::uint16_t wide[4]{};
    ra3::common::widenBytes(wide, "A\xFF");
    RA3_CHECK_EQ(wide[0], 0x41u);
    RA3_CHECK_EQ(wide[1], 0xFFu);
    RA3_CHECK_EQ(wide[2], 0u);
}

RA3_TEST(byteStringPortsMatchLegacyHashAndUtf8)
{
    RA3_CHECK_EQ(ra3::common::hashLowercaseFnv1a("AbC"), 0x1A47E90Bu);
    const std::uint8_t encoded[] = {0xF4, 0x8F, 0xBF, 0xBF};
    RA3_CHECK_EQ(ra3::common::decodeUtf8(encoded), 0x10FFFFu);
    std::uint32_t codePoint = 0;
    RA3_CHECK_EQ(ra3::common::decodeUtf8AndAdvance(encoded, codePoint), encoded + 4);
    RA3_CHECK_EQ(codePoint, 0x10FFFFu);

    std::uint8_t output[4]{};
    ra3::common::encodeUtf8(output, codePoint);
    RA3_CHECK_EQ(std::memcmp(output, encoded, 4), 0);
}
