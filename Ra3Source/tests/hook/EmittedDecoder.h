#pragma once

#include <Windows.h>

#include <array>
#include <cstdint>
#include <cstring>
#include <vector>

namespace ra3::test {

inline constexpr std::array<std::uint8_t, 16> decoderPrologue = {
    0x51, 0x8B, 0x4C, 0x24, 0x0C, 0x85, 0xC9, 0x56,
    0x8B, 0x74, 0x24, 0x0C, 0x75, 0x07, 0x33, 0xC0};

class EmittedDecoder {
public:
    using Function = int(__stdcall*)(std::uint8_t*, const std::uint8_t*);

    EmittedDecoder()
    {
        const std::vector<std::uint8_t> code = {
            0x51, 0x8B, 0x4C, 0x24, 0x0C, 0x85, 0xC9, 0x56,
            0x8B, 0x74, 0x24, 0x0C, 0x75, 0x07, 0x33, 0xC0,
            0x5E, 0x59, 0xC2, 0x08, 0x00,
            0xC7, 0x06, 0x41, 0x42, 0x43, 0x44,
            0xB8, 0x04, 0x00, 0x00, 0x00,
            0x5E, 0x59, 0xC2, 0x08, 0x00};
        m_memory = VirtualAlloc(nullptr, code.size(), MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
        if (m_memory == nullptr) {
            return;
        }
        std::memcpy(m_memory, code.data(), code.size());
        DWORD oldProtection = 0;
        if (!VirtualProtect(m_memory, code.size(), PAGE_EXECUTE_READ, &oldProtection)) {
            VirtualFree(m_memory, 0, MEM_RELEASE);
            m_memory = nullptr;
            return;
        }
        FlushInstructionCache(GetCurrentProcess(), m_memory, code.size());
    }

    ~EmittedDecoder()
    {
        if (m_memory != nullptr) {
            VirtualFree(m_memory, 0, MEM_RELEASE);
        }
    }

    EmittedDecoder(const EmittedDecoder&) = delete;
    EmittedDecoder& operator=(const EmittedDecoder&) = delete;

    bool valid() const { return m_memory != nullptr; }
    void* address() const { return m_memory; }
    Function function() const { return reinterpret_cast<Function>(m_memory); }

private:
    void* m_memory = nullptr;
};

}  // namespace ra3::test
