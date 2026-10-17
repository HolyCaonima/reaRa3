#pragma once

#include <chrono>
#include <cstdint>
#include <filesystem>
#include <map>
#include <string>
#include <vector>

namespace ra3hook {

struct DumpConfig {
    std::filesystem::path directory;  // empty: dump mode off
    std::vector<unsigned> snapshotSeconds{0, 60, 180};
    unsigned pollIntervalMs = 500;
};

// Read-only capture of the host image's SecuROM jump sites for offline analysis.
//
// Every `FF 25`/`FF 15` in .text whose memory operand lies in .securom is a site.
// The worker polls each slot value and the six site bytes, logging changes, and at
// the configured times writes a snapshot: slot values with the memory region they
// point into, the process memory map, and raw copies of the game's .text, .rdata,
// .data, the .securom section and any non-image region a slot points into.
// Pages that are not committed, are PAGE_NOACCESS or carry PAGE_GUARD are never
// touched; they are zero-filled in the copy and listed as unreadable.
class RuntimeDump {
public:
    explicit RuntimeDump(DumpConfig config);

    bool start(std::string& error);
    void tick();
    unsigned pollIntervalMs() const { return config_.pollIntervalMs; }

private:
    struct Section {
        std::string name;
        std::uintptr_t start = 0;
        std::size_t size = 0;
        std::uint32_t characteristics = 0;
    };
    struct Site {
        std::uintptr_t address = 0;
        char kind = 'j';  // 'j' jmp [slot], 'c' call [slot]
        std::uintptr_t slot = 0;
        std::uint8_t bytes[6]{};
    };

    const Section* section(const char* name) const;
    std::uint64_t elapsedMs() const;
    void poll();
    void writeSnapshot(std::size_t index, unsigned seconds);

    DumpConfig config_;
    std::chrono::steady_clock::time_point started_;
    std::uintptr_t imageBase_ = 0;
    std::size_t imageSize_ = 0;
    std::vector<Section> sections_;
    std::vector<Site> sites_;
    std::map<std::uintptr_t, std::uint32_t> slots_;
    std::size_t nextSnapshot_ = 0;
};

}  // namespace ra3hook
