#include "support/TestHarness.h"

#include <ra3/io/RefPack.h>

#include <string>
#include <vector>

using ra3::io::decompressRefPack;
using ra3::io::readRefPackHeader;

namespace {

std::string decode(const std::vector<std::uint8_t>& stream)
{
    auto out = decompressRefPack(stream.data(), stream.size());
    return out ? std::string(out.value().begin(), out.value().end()) : "<error: " + out.error().message + ">";
}

bool rejects(const std::vector<std::uint8_t>& stream)
{
    return !decompressRefPack(stream.data(), stream.size()).ok();
}

}  // namespace

RA3_TEST(RefPack_ReadsThreeAndFourByteHeaders)
{
    const std::uint8_t small[] = {0x10, 0xFB, 0x17, 0x0C, 0x47};
    const auto a = readRefPackHeader(small, sizeof(small));
    RA3_REQUIRE(a.has_value());
    RA3_CHECK_EQ(a->decompressedSize, 0x170C47u);
    RA3_CHECK_EQ(a->headerSize, std::size_t{5});

    const std::uint8_t large[] = {0x90, 0xFB, 0x13, 0x15, 0xAC, 0xDC};
    const auto b = readRefPackHeader(large, sizeof(large));
    RA3_REQUIRE(b.has_value());
    RA3_CHECK_EQ(b->decompressedSize, 0x1315ACDCu);

    const std::uint8_t withCompressedSize[] = {0x11, 0xFB, 0x00, 0x00, 0x09, 0x00, 0x00, 0x20};
    const auto c = readRefPackHeader(withCompressedSize, sizeof(withCompressedSize));
    RA3_REQUIRE(c.has_value());
    RA3_CHECK_EQ(c->decompressedSize, 0x20u);
    RA3_CHECK_EQ(c->headerSize, std::size_t{8});

    const std::uint8_t notRefPack[] = {0x20, 0x46, 0x53, 0x43};
    RA3_CHECK(!readRefPackHeader(notRefPack, sizeof(notRefPack)).has_value());

    // The game also requires (flags & 0x3E) == 0x10, so "xx FB" alone is not enough.
    const std::uint8_t wrongFlags[] = {0x00, 0xFB, 0x00, 0x00, 0x01};
    RA3_CHECK(!readRefPackHeader(wrongFlags, sizeof(wrongFlags)).has_value());
    const std::uint8_t smallWindow[] = {0x50, 0xFB, 0x00, 0x00, 0x01};
    RA3_CHECK(readRefPackHeader(smallWindow, sizeof(smallWindow)).has_value());
}

RA3_TEST(RefPack_DecodesEveryCommandForm)
{
    const std::vector<std::uint8_t> stream = {
        0x10, 0xFB, 0x00, 0x00, 0x13,  // header: 19 bytes
        0xE0, 'A', 'B', 'C', 'D',      // literal run of 4          -> ABCD
        0x08, 0x03,                    // 2-byte copy, 5 from 4 back -> ABCDA (overlapping)
        0x80, 0x00, 0x08,              // 3-byte copy, 4 from 9 back -> ABCD
        0xC0, 0x00, 0x0C, 0x00,        // 4-byte copy, 5 from 13 back -> ABCDA
        0xFD, 'Z',                     // stop with one literal
    };
    RA3_CHECK_EQ(decode(stream), std::string("ABCDABCDAABCDABCDAZ"));
}

RA3_TEST(RefPack_RejectsMalformedStreams)
{
    RA3_CHECK(rejects({0x10, 0xFB, 0x00, 0x00, 0x04, 0xE0, 'A', 'B', 'C', 'D'}));              // no stop command
    RA3_CHECK(rejects({0x10, 0xFB, 0x00, 0x00, 0x05, 0xE0, 'A', 'B', 'C', 'D', 0xFC}));        // too short
    RA3_CHECK(rejects({0x10, 0xFB, 0x00, 0x00, 0x09, 0xE0, 'A', 'B', 'C', 'D', 0x08, 0x05, 0xFC}));  // copy before start
    RA3_CHECK(rejects({0x10, 0xFB, 0x00, 0x00, 0x01, 0xFD, 'A', 0x00}));                       // trailing byte
    RA3_CHECK(rejects({0x10, 0xFB, 0x00, 0x00, 0x02, 0xFD, 'A', 'B'}));                        // longer than declared
}
