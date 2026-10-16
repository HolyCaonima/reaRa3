#pragma once

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace ra3hook {

enum class Mode {
    off,
    shadow,
    replace
};

const char* modeName(Mode mode);

struct TargetState {
    Mode mode = Mode::off;
    void* original = nullptr;
    std::atomic<std::uint64_t> calls{0};
    std::atomic<std::uint64_t> matches{0};
    std::atomic<std::uint64_t> mismatches{0};
    std::atomic<unsigned> reported{0};
};

// A clean .text function of ra3_1.0.game that the DLL can take over. In shadow
// mode the hook runs the original and our implementation and returns the
// original's result; in replace mode only ours runs.
struct Target {
    const char* key;
    std::uintptr_t rva;
    std::vector<std::uint8_t> expected;
    // Whole, position-independent instructions covering at least 5 bytes.
    std::size_t stolenLength;
    void* replacement;
    TargetState* state;
    // Targets that read game-owned layouts require an explicit key in config.
    bool explicitOnly;
};

const std::vector<Target>& targets();

void writeLine(const std::string& text);

// Counts a shadow mismatch and logs the first few with `detail`.
void reportMismatch(const char* key, TargetState& state, const std::string& detail);

}  // namespace ra3hook
