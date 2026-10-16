#include "support/BigBuilder.h"
#include "support/TempDir.h"
#include "support/TestHarness.h"

#include <ra3/core/ByteOrder.h>
#include <ra3/io/BigArchive.h>
#include <ra3/io/GameBigLayout.h>

#include <algorithm>
#include <cstring>
#include <vector>

using ra3::io::BigArchive;
using ra3::test::BigLayout;
using ra3::test::buildBig;
using ra3::test::TempDir;

RA3_TEST(BigArchive_ReadsIndexTrailerAndData)
{
    TempDir dir("big");
    const auto file = dir.write("test.big", buildBig({{"data\\static.bin", "STATIC"}, {"art\\a.tga", "TGA!"}}));

    auto archive = BigArchive::open(file);
    RA3_REQUIRE_OK(archive);
    const BigArchive& big = archive.value();
    RA3_CHECK(big.magic() == "BIG4");
    RA3_REQUIRE(big.entries().size() == 2);
    RA3_CHECK_EQ(big.entries()[0].path, std::string("data\\static.bin"));
    RA3_CHECK_EQ(big.entries()[1].size, 4u);
    RA3_REQUIRE(big.trailer().has_value());
    RA3_CHECK(std::memcmp(big.trailer()->bytes.data(), "L280", 4) == 0);
    RA3_CHECK_EQ(big.trailer()->number, 280);

    const ra3::io::BigEntry* entry = big.find("DATA\\Static.BIN");
    RA3_REQUIRE(entry != nullptr);
    auto data = big.read(*entry);
    RA3_REQUIRE_OK(data);
    RA3_CHECK_EQ(std::string(data.value().begin(), data.value().end()), std::string("STATIC"));

    auto head = big.read(*entry, 3);
    RA3_REQUIRE_OK(head);
    RA3_CHECK_EQ(head.value().size(), std::size_t{3});
    RA3_CHECK(big.find("data/static.bin") == nullptr);
    RA3_CHECK(big.find("data\\missing.bin") == nullptr);
}

RA3_TEST(BigArchive_AcceptsEmptyArchive)
{
    TempDir dir("big-empty");
    auto archive = BigArchive::open(dir.write("empty.big", buildBig({})));
    RA3_REQUIRE_OK(archive);
    RA3_CHECK(archive.value().entries().empty());
}

RA3_TEST(BigArchive_TrailerIsOptional)
{
    TempDir dir("big-trailer");
    BigLayout noTrailer;
    noTrailer.trailer.clear();
    auto plain = BigArchive::open(dir.write("plain.big", buildBig({{"data\\a.bin", "AAAA"}}, noTrailer)));
    RA3_REQUIRE_OK(plain);
    RA3_CHECK(!plain.value().trailer().has_value());
    RA3_REQUIRE(plain.value().find("data\\a.bin") != nullptr);

    // Same rule as the game: a letter and three digits, whatever follows.
    BigLayout otherTag;
    otherTag.trailer = std::string("x007\0\0\0\0", 8);
    auto tagged = BigArchive::open(dir.write("tagged.big", buildBig({{"b", "B"}}, otherTag)));
    RA3_REQUIRE_OK(tagged);
    RA3_REQUIRE(tagged.value().trailer().has_value());
    RA3_CHECK_EQ(tagged.value().trailer()->number, 7);

    RA3_CHECK(!ra3::io::parseBigTrailer(reinterpret_cast<const std::uint8_t*>("L28A\0\0\0\0")).has_value());
    RA3_CHECK(!ra3::io::parseBigTrailer(reinterpret_cast<const std::uint8_t*>("1280\0\0\0\0")).has_value());
}

RA3_TEST(BigArchive_AcceptsBigFAndRejectsOtherMagic)
{
    TempDir dir("big-magic");
    BigLayout bigF;
    bigF.magic = "BIGF";
    auto f = BigArchive::open(dir.write("bigf.big", buildBig({{"a", "1"}}, bigF)));
    RA3_REQUIRE_OK(f);
    RA3_CHECK(f.value().magic() == "BIGF");

    for (const char* magic : {"BIG3", "Viv4", "big4"}) {
        BigLayout other;
        other.magic = magic;
        auto archive = BigArchive::open(dir.write("other.big", buildBig({{"a", "1"}}, other)));
        RA3_REQUIRE(!archive.ok());
        RA3_CHECK(archive.error().message.find(magic) != std::string::npos);
    }
}

RA3_TEST(BigArchive_RejectsSizeMismatch)
{
    TempDir dir("big-size");
    std::string bytes = buildBig({{"a", "1"}});
    bytes += "extra";
    RA3_CHECK(!BigArchive::open(dir.write("long.big", bytes)).ok());
}

RA3_TEST(BigArchive_RejectsCorruptIndex)
{
    TempDir dir("big-corrupt");
    const std::string good = buildBig({{"data\\a.bin", "AAAA"}, {"data\\b.bin", "BBBB"}});

    std::string tooMany = good;
    tooMany[11] = 3;  // entry count low byte
    RA3_CHECK(!BigArchive::open(dir.write("count.big", tooMany)).ok());

    std::string outOfBounds = good;
    ra3::writeU32BE(reinterpret_cast<std::uint8_t*>(&outOfBounds[20]), 0x7FFFFFF0u);  // first entry size
    RA3_CHECK(!BigArchive::open(dir.write("bounds.big", outOfBounds)).ok());

    std::string duplicate = buildBig({{"data\\a.bin", "A"}, {"DATA\\A.BIN", "B"}});
    RA3_CHECK(!BigArchive::open(dir.write("dup.big", duplicate)).ok());

    RA3_CHECK(!BigArchive::open(dir.write("tiny.big", "BIG4")).ok());
}

RA3_TEST(GameBigLayout_FindsEntriesByOrdinalAndHash)
{
    std::string bytes = buildBig({{"data\\alpha.bin", "AAAA"}, {"DATA\\Beta.bin", "BBBBB"}});
    const auto* header = reinterpret_cast<const std::uint8_t*>(bytes.data());
    const std::uint8_t* first = header + 16;
    const char* firstName = reinterpret_cast<const char*>(first + 8);
    const std::uint8_t* second = first + 8 + std::strlen(firstName) + 1;
    const char* secondName = reinterpret_cast<const char*>(second + 8);

    std::vector<ra3::io::GameBigSearchEntry> search = {
        {ra3::io::hashGameBigPath(firstName), first},
        {ra3::io::hashGameBigPath(secondName), second},
    };
    std::sort(search.begin(), search.end(), [](const auto& left, const auto& right) {
        return left.hash < right.hash;
    });
    const ra3::io::GameBigIndex index{header, header, search.data()};

    std::uint64_t offset = 0;
    std::uint64_t size = 0;
    const std::uint8_t* cursor = nullptr;
    int ordinal = -1;
    const char* byOrdinal =
        ra3::io::findGameBigEntry(&index, nullptr, 1, &offset, &size, &cursor, &ordinal);
    RA3_REQUIRE(byOrdinal != nullptr);
    RA3_CHECK_EQ(std::string(byOrdinal), std::string("DATA\\Beta.bin"));
    RA3_CHECK_EQ(size, std::uint64_t{5});
    RA3_CHECK_EQ(ordinal, 1);
    RA3_CHECK(cursor == second);

    const char* byName =
        ra3::io::findGameBigEntry(&index, "data\\BETA.BIN", 0, &offset, &size, nullptr, nullptr);
    RA3_REQUIRE(byName != nullptr);
    RA3_CHECK_EQ(std::string(byName), std::string("DATA\\Beta.bin"));
    RA3_CHECK_EQ(size, std::uint64_t{5});

    RA3_CHECK(
        ra3::io::findGameBigEntry(
            &index, "data/Beta.bin", 0, &offset, &size, nullptr, nullptr)
        == nullptr);
    RA3_CHECK_EQ(offset, std::uint64_t{0});
    RA3_CHECK_EQ(size, std::uint64_t{0});
}
