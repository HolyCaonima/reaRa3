#include "hook/EmittedDecoder.h"
#include "support/TestHarness.h"

#include <ra3/hook/Detour.h>

#include <array>
#include <cstdint>
#include <string>
#include <vector>

namespace {

void* g_trampoline = nullptr;
std::uint32_t g_replacementCalls = 0;

int __stdcall replacement(std::uint8_t* destination, const std::uint8_t* source)
{
    ++g_replacementCalls;
    const auto original = reinterpret_cast<ra3::test::EmittedDecoder::Function>(g_trampoline);
    return original(destination, source) + 1;
}

ra3::hook::HookSpec specFor(ra3::test::EmittedDecoder& decoder)
{
    ra3::hook::HookSpec spec;
    spec.target = decoder.address();
    spec.expectedBytes.assign(
        ra3::test::decoderPrologue.begin(), ra3::test::decoderPrologue.end());
    spec.stolenLength = 5;
    spec.replacement = reinterpret_cast<void*>(&replacement);
    spec.trampolineOut = &g_trampoline;
    return spec;
}

}  // namespace

RA3_TEST(Hook_RefusesMismatchedExpectedBytes)
{
    ra3::test::EmittedDecoder decoder;
    RA3_REQUIRE(decoder.valid());
    auto spec = specFor(decoder);
    spec.expectedBytes[0] ^= 0xFF;
    auto installed = ra3::hook::installDetour(spec);
    RA3_CHECK(!installed.ok());
    RA3_CHECK(g_trampoline == nullptr);
}

RA3_TEST(Hook_InstallsCallsTrampolineAndUninstalls)
{
    ra3::test::EmittedDecoder decoder;
    RA3_REQUIRE(decoder.valid());
    const std::array<std::uint8_t, 10> source = {
        0x10, 0xFB, 0x00, 0x00, 0x04, 0xE0, 'A', 'B', 'C', 'D'};
    std::array<std::uint8_t, 4> destination{};

    RA3_CHECK_EQ(decoder.function()(destination.data(), source.data()), 4);
    RA3_CHECK_EQ(std::string(destination.begin(), destination.end()), std::string("ABCD"));

    g_replacementCalls = 0;
    auto installed = ra3::hook::installDetour(specFor(decoder));
    RA3_REQUIRE_OK(installed);
    RA3_REQUIRE(g_trampoline != nullptr);
    destination.fill(0);
    RA3_CHECK_EQ(decoder.function()(destination.data(), source.data()), 5);
    RA3_CHECK_EQ(g_replacementCalls, 1u);
    RA3_CHECK_EQ(std::string(destination.begin(), destination.end()), std::string("ABCD"));

    auto removed = installed.value()->uninstall();
    RA3_REQUIRE_OK(removed);
    RA3_CHECK(removed.value());
    destination.fill(0);
    RA3_CHECK_EQ(decoder.function()(destination.data(), source.data()), 4);
    RA3_CHECK_EQ(g_replacementCalls, 1u);
}
