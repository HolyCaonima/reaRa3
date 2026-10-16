#pragma once

#include <cstdint>
#include <string_view>

namespace ra3::asset {

// ra3_1.0.game 0x00417DB0: SuperFastHash over case-folded bytes. Each byte goes
// through the CRT tolower as a signed char and stays sign-extended when mixed,
// so bytes >= 0x80 do not hash like the reference SuperFastHash. Case folding
// assumes the default "C" locale (only A-Z change).
std::uint32_t hashName(std::string_view bytes, std::uint32_t seed, bool finalize);

// Asset instanceId from the instance name (the part after "Type:" in a manifest
// name). ra3_1.0.game 0x0047BE90 calls hashName(name, length, length, true).
std::uint32_t instanceIdFromName(std::string_view name);

// Asset typeId from the type name ("Type" in "Type:Name"): the same mix without
// case folding. Matches all 257 shipped types; the game-side caller has not been
// located yet, so bytes >= 0x80 are unverified.
std::uint32_t typeIdFromName(std::string_view typeName);

}  // namespace ra3::asset
