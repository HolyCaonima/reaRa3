#pragma once

#include <ra3/core/Result.h>

#include <cstddef>
#include <cstdint>
#include <memory>
#include <vector>

namespace ra3::hook {

struct HookSpec {
    void* target = nullptr;
    std::vector<std::uint8_t> expectedBytes;
    std::size_t stolenLength = 0;
    void* replacement = nullptr;
    void** trampolineOut = nullptr;
};

class Detour {
public:
    ~Detour();

    Detour(const Detour&) = delete;
    Detour& operator=(const Detour&) = delete;

    void* trampoline() const { return m_trampoline; }
    bool installed() const { return m_installed; }
    Result<bool> uninstall();

private:
    friend Result<std::unique_ptr<Detour>> installDetour(const HookSpec& spec);

    Detour(
        void* target,
        void* trampoline,
        std::vector<std::uint8_t> originalBytes,
        std::vector<std::uint8_t> patchBytes);

    void* m_target = nullptr;
    void* m_trampoline = nullptr;
    std::vector<std::uint8_t> m_originalBytes;
    std::vector<std::uint8_t> m_patchBytes;
    bool m_installed = false;
};

Result<std::unique_ptr<Detour>> installDetour(const HookSpec& spec);

}  // namespace ra3::hook
