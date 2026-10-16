#include <ra3/hook/Detour.h>

#include <Windows.h>
#include <TlHelp32.h>

#include <algorithm>
#include <cstring>
#include <limits>
#include <string>
#include <utility>

namespace ra3::hook {
namespace {

constexpr std::size_t jumpSize = 5;
constexpr std::size_t absoluteJumpSize = 6;
constexpr unsigned suspendAttempts = 20;

std::string windowsError(const char* operation, DWORD error = GetLastError())
{
    return std::string(operation) + " failed with Windows error " + std::to_string(error);
}

class SuspendedThreads {
public:
    ~SuspendedThreads()
    {
        for (HANDLE thread : m_threads) {
            ResumeThread(thread);
            CloseHandle(thread);
        }
    }

    SuspendedThreads(const SuspendedThreads&) = delete;
    SuspendedThreads& operator=(const SuspendedThreads&) = delete;

    static Result<std::unique_ptr<SuspendedThreads>> acquire(const void* address, std::size_t length)
    {
        for (unsigned attempt = 0; attempt < suspendAttempts; ++attempt) {
            auto suspended = std::unique_ptr<SuspendedThreads>(new SuspendedThreads());
            const auto result = suspended->suspendOnce(address, length);
            if (result.ok() && result.value()) {
                return suspended;
            }
            if (!result.ok()) {
                return result.error();
            }
            suspended.reset();
            Sleep(1);
        }
        return makeError("a thread instruction pointer remained inside the patch range");
    }

private:
    SuspendedThreads() = default;

    Result<bool> suspendOnce(const void* address, std::size_t length)
    {
        const DWORD processId = GetCurrentProcessId();
        const DWORD currentThreadId = GetCurrentThreadId();
        HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPTHREAD, 0);
        if (snapshot == INVALID_HANDLE_VALUE) {
            return makeError(windowsError("CreateToolhelp32Snapshot"));
        }

        THREADENTRY32 entry{};
        entry.dwSize = sizeof(entry);
        BOOL found = Thread32First(snapshot, &entry);
        while (found) {
            if (entry.th32OwnerProcessID == processId && entry.th32ThreadID != currentThreadId) {
                HANDLE thread = OpenThread(
                    THREAD_SUSPEND_RESUME | THREAD_GET_CONTEXT | THREAD_QUERY_INFORMATION,
                    FALSE,
                    entry.th32ThreadID);
                if (thread == nullptr) {
                    const DWORD error = GetLastError();
                    if (error != ERROR_INVALID_PARAMETER) {
                        CloseHandle(snapshot);
                        return makeError(windowsError("OpenThread", error));
                    }
                } else if (SuspendThread(thread) == static_cast<DWORD>(-1)) {
                    const DWORD error = GetLastError();
                    CloseHandle(thread);
                    CloseHandle(snapshot);
                    return makeError(windowsError("SuspendThread", error));
                } else {
                    m_threads.push_back(thread);
                }
            }
            found = Thread32Next(snapshot, &entry);
        }
        const DWORD enumerationError = GetLastError();
        CloseHandle(snapshot);
        if (enumerationError != ERROR_NO_MORE_FILES) {
            return makeError(windowsError("Thread32Next", enumerationError));
        }

        const auto begin = reinterpret_cast<std::uintptr_t>(address);
        const auto end = begin + length;
        for (HANDLE thread : m_threads) {
            CONTEXT context{};
            context.ContextFlags = CONTEXT_CONTROL;
            if (!GetThreadContext(thread, &context)) {
                return makeError(windowsError("GetThreadContext"));
            }
            const auto instruction = static_cast<std::uintptr_t>(context.Eip);
            if (instruction >= begin && instruction < end) {
                return false;
            }
        }
        return true;
    }

    std::vector<HANDLE> m_threads;
};

bool makeJump(std::uint8_t* output, const void* source, const void* destination)
{
    const auto from =
        static_cast<std::int64_t>(reinterpret_cast<std::uintptr_t>(source)) + jumpSize;
    const auto to = static_cast<std::int64_t>(reinterpret_cast<std::uintptr_t>(destination));
    const std::int64_t displacement = to - from;
    if (displacement < std::numeric_limits<std::int32_t>::min()
        || displacement > std::numeric_limits<std::int32_t>::max()) {
        return false;
    }
    output[0] = 0xE9;
    const auto relative = static_cast<std::int32_t>(displacement);
    std::memcpy(output + 1, &relative, sizeof(relative));
    return true;
}

void* allocateNear(const void* target, std::size_t size)
{
    SYSTEM_INFO system{};
    GetSystemInfo(&system);
    const std::uintptr_t granularity = system.dwAllocationGranularity;
    const std::uintptr_t address = reinterpret_cast<std::uintptr_t>(target);
    const std::uintptr_t base = address - (address % granularity);
    const std::uintptr_t maximumDistance =
        static_cast<std::uintptr_t>(std::numeric_limits<std::int32_t>::max());
    for (std::uintptr_t distance = granularity; distance < maximumDistance; distance += granularity) {
        if (base >= distance) {
            void* allocation = VirtualAlloc(
                reinterpret_cast<void*>(base - distance),
                size,
                MEM_COMMIT | MEM_RESERVE,
                PAGE_READWRITE);
            if (allocation != nullptr) {
                return allocation;
            }
        }
        if (base <= std::numeric_limits<std::uintptr_t>::max() - distance) {
            void* allocation = VirtualAlloc(
                reinterpret_cast<void*>(base + distance),
                size,
                MEM_COMMIT | MEM_RESERVE,
                PAGE_READWRITE);
            if (allocation != nullptr) {
                return allocation;
            }
        }
    }
    return nullptr;
}

Result<bool> writeCode(void* target, const std::vector<std::uint8_t>& bytes)
{
    DWORD oldProtection = 0;
    if (!VirtualProtect(target, bytes.size(), PAGE_EXECUTE_READWRITE, &oldProtection)) {
        return makeError(windowsError("VirtualProtect"));
    }
    std::memcpy(target, bytes.data(), bytes.size());
    const BOOL flushed = FlushInstructionCache(GetCurrentProcess(), target, bytes.size());
    DWORD ignored = 0;
    const BOOL restored = VirtualProtect(target, bytes.size(), oldProtection, &ignored);
    if (!flushed) {
        return makeError(windowsError("FlushInstructionCache"));
    }
    if (!restored) {
        return makeError(windowsError("VirtualProtect restore"));
    }
    return true;
}

}  // namespace

Detour::Detour(
    void* target,
    void* trampoline,
    std::vector<std::uint8_t> originalBytes,
    std::vector<std::uint8_t> patchBytes)
    : m_target(target),
      m_trampoline(trampoline),
      m_originalBytes(std::move(originalBytes)),
      m_patchBytes(std::move(patchBytes)),
      m_installed(true)
{
}

Detour::~Detour()
{
    if (m_installed) {
        const auto result = uninstall();
        if (!result.ok()) {
            return;
        }
    }
    if (m_trampoline != nullptr) {
        VirtualFree(m_trampoline, 0, MEM_RELEASE);
    }
}

Result<bool> Detour::uninstall()
{
    if (!m_installed) {
        return true;
    }
    auto suspended = SuspendedThreads::acquire(m_target, m_originalBytes.size());
    if (!suspended.ok()) {
        return suspended.error();
    }
    if (!std::equal(m_patchBytes.begin(), m_patchBytes.end(), static_cast<std::uint8_t*>(m_target))) {
        return makeError("target bytes no longer match this detour");
    }
    auto written = writeCode(m_target, m_originalBytes);
    if (!written.ok()) {
        return written.error();
    }
    m_installed = false;
    return true;
}

Result<std::unique_ptr<Detour>> installDetour(const HookSpec& spec)
{
    static_assert(sizeof(void*) == 4, "inline detours require a 32-bit build");
    if (spec.target == nullptr || spec.replacement == nullptr) {
        return makeError("hook target and replacement must be non-null");
    }
    if (spec.stolenLength < jumpSize) {
        return makeError("stolen length must be at least five bytes");
    }
    if (spec.expectedBytes.size() < spec.stolenLength) {
        return makeError("expected bytes must cover every stolen byte");
    }

    const std::size_t trampolineSize = spec.stolenLength + jumpSize + absoluteJumpSize;
    void* trampoline = allocateNear(spec.target, trampolineSize);
    if (trampoline == nullptr) {
        return makeError(windowsError("VirtualAlloc"));
    }
    std::memcpy(trampoline, spec.target, spec.stolenLength);
    auto* trampolineBytes = static_cast<std::uint8_t*>(trampoline);
    if (!makeJump(
            trampolineBytes + spec.stolenLength,
            trampolineBytes + spec.stolenLength,
            static_cast<std::uint8_t*>(spec.target) + spec.stolenLength)) {
        VirtualFree(trampoline, 0, MEM_RELEASE);
        return makeError("trampoline jump is outside rel32 range");
    }
    std::uint8_t* relay = trampolineBytes + spec.stolenLength + jumpSize;
    relay[0] = 0x68;
    const auto replacement = static_cast<std::uint32_t>(
        reinterpret_cast<std::uintptr_t>(spec.replacement));
    std::memcpy(relay + 1, &replacement, sizeof(replacement));
    relay[5] = 0xC3;
    DWORD oldProtection = 0;
    if (!VirtualProtect(trampoline, trampolineSize, PAGE_EXECUTE_READ, &oldProtection)) {
        const auto error = makeError(windowsError("VirtualProtect trampoline"));
        VirtualFree(trampoline, 0, MEM_RELEASE);
        return error;
    }
    FlushInstructionCache(GetCurrentProcess(), trampoline, trampolineSize);

    auto suspended = SuspendedThreads::acquire(spec.target, spec.stolenLength);
    if (!suspended.ok()) {
        VirtualFree(trampoline, 0, MEM_RELEASE);
        return suspended.error();
    }
    if (!std::equal(
            spec.expectedBytes.begin(),
            spec.expectedBytes.end(),
            static_cast<const std::uint8_t*>(spec.target))) {
        VirtualFree(trampoline, 0, MEM_RELEASE);
        return makeError("target bytes do not match the expected game build");
    }

    std::vector<std::uint8_t> original(
        static_cast<const std::uint8_t*>(spec.target),
        static_cast<const std::uint8_t*>(spec.target) + spec.stolenLength);
    std::vector<std::uint8_t> patch(spec.stolenLength, 0x90);
    if (!makeJump(patch.data(), spec.target, relay)) {
        VirtualFree(trampoline, 0, MEM_RELEASE);
        return makeError("replacement jump is outside rel32 range");
    }
    if (spec.trampolineOut != nullptr) {
        *spec.trampolineOut = trampoline;
    }
    auto written = writeCode(spec.target, patch);
    if (!written.ok()) {
        if (spec.trampolineOut != nullptr) {
            *spec.trampolineOut = nullptr;
        }
        VirtualFree(trampoline, 0, MEM_RELEASE);
        return written.error();
    }
    return std::unique_ptr<Detour>(
        new Detour(spec.target, trampoline, std::move(original), std::move(patch)));
}

}  // namespace ra3::hook
