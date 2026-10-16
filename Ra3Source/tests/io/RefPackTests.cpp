#include "support/TestHarness.h"

#include <ra3/io/RefPack.h>

#include <algorithm>
#include <limits>
#include <string>
#include <vector>

#include <windows.h>

using ra3::io::decompressRefPack;
using ra3::io::decompressRefPackPrefix;
using ra3::io::DecodedStream;
using ra3::io::probeRefPackSize;
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

RA3_TEST(RefPack_ProbeReadsTheFirstSizeField)
{
    const std::uint8_t plain[] = {0x10, 0xFB, 0x17, 0x0C, 0x47};
    RA3_CHECK_EQ(probeRefPackSize(plain), 0x170C47u);
    const std::uint8_t large[] = {0xD1, 0xFB, 0x13, 0x15, 0xAC, 0xDC};
    RA3_CHECK_EQ(probeRefPackSize(large), 0x1315ACDCu);
    const std::uint8_t withCompressedSize[] = {0x11, 0xFB, 0x00, 0x00, 0x09, 0x00, 0x00, 0x20};
    RA3_CHECK_EQ(probeRefPackSize(withCompressedSize), 0x09u);
    const std::uint8_t wrongFlags[] = {0x00, 0xFB, 0x00, 0x00, 0x01};
    RA3_CHECK_EQ(probeRefPackSize(wrongFlags), 0u);
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

RA3_TEST(RefPack_DecodesPrefixWithoutTheRestOfTheStream)
{
    const std::vector<std::uint8_t> partial = {
        0x10, 0xFB, 0x00, 0x00, 0x13,
        0xE0, 'A', 'B', 'C', 'D',
    };
    auto prefix = decompressRefPackPrefix(partial.data(), partial.size(), 4);
    RA3_REQUIRE_OK(prefix);
    RA3_CHECK_EQ(std::string(prefix.value().begin(), prefix.value().end()), std::string("ABCD"));

    auto tooLong = decompressRefPackPrefix(partial.data(), partial.size(), 5);
    RA3_CHECK(!tooLong.ok());
}

// The hook passes an unbounded size because 0x009F72E0 is never told the
// input length; reading ahead would fault inside the game.
RA3_TEST(RefPack_PrefixReadsNoInputPastItsCommands)
{
    const std::vector<std::uint8_t> stream = {
        0x10, 0xFB, 0x00, 0x00, 0x13, 0xE0, 'A', 'B', 'C', 'D', 0x08, 0x03,
        0x80, 0x00, 0x08, 0xC0, 0x00, 0x0C, 0x00, 0xFD, 'Z',
    };
    SYSTEM_INFO info{};
    GetSystemInfo(&info);
    const std::size_t page = info.dwPageSize;
    auto* pages = static_cast<std::uint8_t*>(
        VirtualAlloc(nullptr, 2 * page, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE));
    RA3_REQUIRE(pages != nullptr);
    DWORD previous = 0;
    RA3_REQUIRE(VirtualProtect(pages + page, page, PAGE_NOACCESS, &previous) != 0);
    std::uint8_t* data = pages + page - stream.size();
    std::copy(stream.begin(), stream.end(), data);

    auto out = decompressRefPackPrefix(data, std::numeric_limits<std::size_t>::max(), 19);
    RA3_REQUIRE_OK(out);
    RA3_CHECK_EQ(std::string(out.value().begin(), out.value().end()),
                 std::string("ABCDABCDAABCDABCDAZ"));
    VirtualFree(pages, 0, MEM_RELEASE);
}

RA3_TEST(RefPack_StreamingWrapsWindowAndUsesLongestDistance)
{
    constexpr std::size_t literalSize = 128 * 1024;
    constexpr std::size_t copySize = 5;
    std::vector<std::uint8_t> packed = {
        0x90, 0xFB, 0x00, 0x02, 0x00, 0x05,
    };
    std::vector<std::uint8_t> expected;
    expected.reserve(literalSize + copySize);
    std::size_t emitted = 0;
    while (literalSize - emitted >= 112) {
        packed.push_back(0xFB);
        for (std::size_t i = 0; i < 112; ++i) {
            const auto value = static_cast<std::uint8_t>((emitted + i) * 37u);
            packed.push_back(value);
            expected.push_back(value);
        }
        emitted += 112;
    }
    const std::size_t remainder = literalSize - emitted;
    RA3_REQUIRE(remainder >= 4 && (remainder % 4) == 0);
    packed.push_back(static_cast<std::uint8_t>(0xE0 + (remainder - 4) / 4));
    for (std::size_t i = 0; i < remainder; ++i) {
        const auto value = static_cast<std::uint8_t>((emitted + i) * 37u);
        packed.push_back(value);
        expected.push_back(value);
    }
    packed.insert(packed.end(), {0xD0, 0xFF, 0xFF, 0x00, 0xFC});
    expected.insert(expected.end(), expected.begin(), expected.begin() + copySize);

    auto stream = DecodedStream::open(
        [&packed](std::uint64_t offset, std::size_t count)
            -> ra3::Result<std::vector<std::uint8_t>> {
            const auto begin = packed.begin() + static_cast<std::size_t>(offset);
            return std::vector<std::uint8_t>(begin, begin + count);
        },
        packed.size(), "synthetic", 7);
    RA3_REQUIRE_OK(stream);
    std::vector<std::uint8_t> actual;
    while (!stream.value().finished()) {
        auto chunk = stream.value().read(997);
        RA3_REQUIRE_OK(chunk);
        actual.insert(actual.end(), chunk.value().begin(), chunk.value().end());
    }
    RA3_CHECK_EQ(actual.size(), expected.size());
    RA3_CHECK(actual == expected);
}

RA3_TEST(RefPack_StreamingPlainBytesUsesSameInterface)
{
    const std::vector<std::uint8_t> plain = {'p', 'l', 'a', 'i', 'n'};
    auto stream = DecodedStream::open(
        [&plain](std::uint64_t offset, std::size_t count)
            -> ra3::Result<std::vector<std::uint8_t>> {
            const auto begin = plain.begin() + static_cast<std::size_t>(offset);
            return std::vector<std::uint8_t>(begin, begin + count);
        },
        plain.size(), "plain", 2);
    RA3_REQUIRE_OK(stream);
    RA3_CHECK(!stream.value().compressed());
    auto first = stream.value().read(2);
    auto rest = stream.value().read(20);
    RA3_REQUIRE_OK(first);
    RA3_REQUIRE_OK(rest);
    RA3_CHECK_EQ(std::string(first.value().begin(), first.value().end()), std::string("pl"));
    RA3_CHECK_EQ(std::string(rest.value().begin(), rest.value().end()), std::string("ain"));
    RA3_CHECK(stream.value().finished());
}
