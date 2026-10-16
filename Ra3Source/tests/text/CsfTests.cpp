#include "support/TestHarness.h"

#include <ra3/text/Csf.h>

#include <cstdint>
#include <cstring>
#include <string>
#include <vector>

namespace {

void appendBytes(std::vector<std::uint8_t>& bytes, const char* text, std::size_t size)
{
    bytes.insert(bytes.end(), text, text + size);
}

void appendU32(std::vector<std::uint8_t>& bytes, std::uint32_t value)
{
    bytes.push_back(static_cast<std::uint8_t>(value));
    bytes.push_back(static_cast<std::uint8_t>(value >> 8));
    bytes.push_back(static_cast<std::uint8_t>(value >> 16));
    bytes.push_back(static_cast<std::uint8_t>(value >> 24));
}

void appendLabel(std::vector<std::uint8_t>& bytes, const std::string& label)
{
    appendBytes(bytes, " LBL", 4);
    appendU32(bytes, 1);
    appendU32(bytes, static_cast<std::uint32_t>(label.size()));
    appendBytes(bytes, label.data(), label.size());
}

void appendText(std::vector<std::uint8_t>& bytes,
                const char* tag,
                const std::vector<std::uint16_t>& text)
{
    appendBytes(bytes, tag, 4);
    appendU32(bytes, static_cast<std::uint32_t>(text.size()));
    for (std::uint16_t codeUnit : text) {
        const std::uint16_t stored = static_cast<std::uint16_t>(~codeUnit);
        bytes.push_back(static_cast<std::uint8_t>(stored));
        bytes.push_back(static_cast<std::uint8_t>(stored >> 8));
    }
}

std::vector<std::uint8_t> makeCsf()
{
    std::vector<std::uint8_t> bytes;
    appendBytes(bytes, " FSC", 4);
    appendU32(bytes, 3);
    appendU32(bytes, 2);
    appendU32(bytes, 2);
    appendU32(bytes, 0);
    appendU32(bytes, 9);

    appendLabel(bytes, "GUI:GREETING");
    appendText(bytes, " RTS", {u'H', u'e', u'l', u'l', u'o', u' ', 0x7EA2, 0x8B66});

    appendLabel(bytes, "Audio:Theme");
    appendText(bytes, "WRTS", {0x97F3, 0x4E50, u' ', 0xD83C, 0xDFB5});
    const std::string extra = "voice_theme";
    appendU32(bytes, static_cast<std::uint32_t>(extra.size()));
    appendBytes(bytes, extra.data(), extra.size());
    return bytes;
}

}  // namespace

RA3_TEST(Csf_ParsesRtsWrtsAndUtf16IntoUtf8)
{
    const std::vector<std::uint8_t> bytes = makeCsf();
    auto parsed = ra3::text::parseCsf(bytes);
    RA3_REQUIRE_OK(parsed);

    const ra3::text::CsfStrings& strings = parsed.value();
    RA3_CHECK(strings.magic() == " FSC");
    RA3_CHECK_EQ(strings.version(), std::uint32_t{3});
    RA3_CHECK_EQ(strings.languageId(), std::uint32_t{9});
    RA3_CHECK_EQ(strings.labelCount(), std::size_t{2});
    RA3_REQUIRE(strings.get("gui:greeting").has_value());
    RA3_CHECK(strings.get("GUI:GREETING").value() == std::string(u8"Hello 红警"));

    const ra3::text::CsfEntry* theme = strings.find("AUDIO:THEME");
    RA3_REQUIRE(theme != nullptr);
    RA3_CHECK(theme->text == std::string(u8"音乐 🎵"));
    RA3_CHECK(theme->extra == "voice_theme");
    RA3_CHECK_EQ(strings.entries().size(), strings.labelCount());
    RA3_CHECK(!strings.get("missing").has_value());
}
