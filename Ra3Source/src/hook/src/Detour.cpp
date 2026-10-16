#include <ra3/hook/Detour.h>

#include <Windows.h>
#include <TlHelp32.h>

#include <algorithm>
#include <array>
#include <cstring>
#include <mutex>
#include <string>
#include <utility>

namespace ra3::hook {
namespace {

constexpr std::size_t jumpSize = 5;
constexpr unsigned patchAttempts = 20;
constexpr std::size_t maxSuspendedThreads = 1024;

std::string windowsError(const char* operation, DWORD error)
{
    return std::string(operation) + " failed with Windows error " + std::to_string(error);
}

// rel32 displacements wrap around the 32-bit address space, so a 5-byte jump
// reaches any destination from any source.
void writeJump(std::uint8_t* output, const void* source, const void* destination)
{
    const auto from = static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(source)) + jumpSize;
    const auto to = static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(destination));
    const std::uint32_t relative = to - from;
    output[0] = 0xE9;
    std::memcpy(output + 1, &relative, sizeof(relative));
}

// Trampolines are carved from shared executable blocks: every VirtualAlloc
// takes 64 KiB of address space, which thousands of hooks in a 32-bit process
// cannot afford. Slots are never returned.
std::uint8_t* allocateTrampoline(std::size_t size)
{
    constexpr std::size_t blockSize = 64 * 1024;
    constexpr std::size_t alignment = 16;
    static std::mutex mutex;
    static std::uint8_t* block = nullptr;
    static std::size_t used = blockSize;

    const std::size_t slot = (size + alignment - 1) / alignment * alignment;
    std::lock_guard<std::mutex> lock(mutex);
    if (blockSize - used < slot) {
        block = static_cast<std::uint8_t*>(
            VirtualAlloc(nullptr, blockSize, MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE));
        if (block == nullptr) {
            return nullptr;
        }
        used = 0;
    }
    std::uint8_t* result = block + used;
    used += slot;
    return result;
}

enum class PatchStatus { written, busy, changed, failed };

struct PatchOutcome {
    PatchStatus status = PatchStatus::failed;
    const char* operation = "";
    DWORD error = ERROR_SUCCESS;
};

// Suspends every other thread, checks that none is executing the range and
// that it still holds `expected`, then writes `bytes`. Nothing here may use the
// heap: a suspended thread may own its lock.
PatchOutcome patchWithThreadsSuspended(
    std::uint8_t* target, const std::uint8_t* expected, const std::uint8_t* bytes, std::size_t size)
{
    std::array<HANDLE, maxSuspendedThreads> threads{};
    std::size_t count = 0;
    PatchOutcome outcome;
    const auto fail = [&](const char* operation) {
        outcome = {PatchStatus::failed, operation, GetLastError()};
    };

    HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPTHREAD, 0);
    if (snapshot == INVALID_HANDLE_VALUE) {
        fail("CreateToolhelp32Snapshot");
        return outcome;
    }
    const DWORD processId = GetCurrentProcessId();
    const DWORD currentThreadId = GetCurrentThreadId();
    THREADENTRY32 entry{};
    entry.dwSize = sizeof(entry);
    bool ok = true;
    for (BOOL found = Thread32First(snapshot, &entry); found && ok; found = Thread32Next(snapshot, &entry)) {
        if (entry.th32OwnerProcessID != processId || entry.th32ThreadID == currentThreadId) {
            continue;
        }
        HANDLE thread = OpenThread(THREAD_SUSPEND_RESUME | THREAD_GET_CONTEXT, FALSE, entry.th32ThreadID);
        if (thread == nullptr) {
            continue;  // the thread exited after the snapshot
        }
        if (count == threads.size()) {
            CloseHandle(thread);
            outcome = {PatchStatus::failed, "suspending more than 1024 threads", ERROR_SUCCESS};
            ok = false;
        } else if (SuspendThread(thread) == static_cast<DWORD>(-1)) {
            fail("SuspendThread");
            CloseHandle(thread);
            ok = false;
        } else {
            threads[count++] = thread;
        }
    }
    CloseHandle(snapshot);

    const auto begin = reinterpret_cast<std::uintptr_t>(target);
    for (std::size_t i = 0; ok && i < count; ++i) {
        CONTEXT context{};
        context.ContextFlags = CONTEXT_CONTROL;
        if (!GetThreadContext(threads[i], &context)) {
            fail("GetThreadContext");
            ok = false;
        } else if (context.Eip >= begin && context.Eip < begin + size) {
            outcome.status = PatchStatus::busy;
            ok = false;
        }
    }
    if (ok && std::memcmp(target, expected, size) != 0) {
        outcome.status = PatchStatus::changed;
        ok = false;
    }
    if (ok) {
        DWORD oldProtection = 0;
        DWORD ignored = 0;
        if (!VirtualProtect(target, size, PAGE_EXECUTE_READWRITE, &oldProtection)) {
            fail("VirtualProtect");
        } else {
            std::memcpy(target, bytes, size);
            FlushInstructionCache(GetCurrentProcess(), target, size);
            VirtualProtect(target, size, oldProtection, &ignored);
            outcome.status = PatchStatus::written;
        }
    }

    for (std::size_t i = 0; i < count; ++i) {
        ResumeThread(threads[i]);
        CloseHandle(threads[i]);
    }
    return outcome;
}

Result<bool> patch(
    std::uint8_t* target, const std::vector<std::uint8_t>& expected, const std::vector<std::uint8_t>& bytes)
{
    for (unsigned attempt = 0; attempt < patchAttempts; ++attempt) {
        const PatchOutcome outcome =
            patchWithThreadsSuspended(target, expected.data(), bytes.data(), bytes.size());
        switch (outcome.status) {
        case PatchStatus::written:
            return true;
        case PatchStatus::busy:
            Sleep(1);
            continue;
        case PatchStatus::changed:
            return makeError("target bytes do not match the expected game build");
        case PatchStatus::failed:
            return makeError(windowsError(outcome.operation, outcome.error));
        }
    }
    return makeError("a thread instruction pointer remained inside the patch range");
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
      m_patchBytes(std::move(patchBytes))
{
}

Detour::~Detour()
{
    if (m_installed) {
        (void)uninstall();
    }
}

Result<bool> Detour::uninstall()
{
    if (!m_installed) {
        return true;
    }
    auto restored = patch(static_cast<std::uint8_t*>(m_target), m_patchBytes, m_originalBytes);
    if (!restored.ok()) {
        return restored.error();
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
    auto* target = static_cast<std::uint8_t*>(spec.target);
    if (!std::equal(spec.expectedBytes.begin(), spec.expectedBytes.end(), target)) {
        return makeError("target bytes do not match the expected game build");
    }

    std::uint8_t* trampoline = allocateTrampoline(spec.stolenLength + jumpSize);
    if (trampoline == nullptr) {
        return makeError(windowsError("VirtualAlloc", GetLastError()));
    }
    std::memcpy(trampoline, target, spec.stolenLength);
    writeJump(trampoline + spec.stolenLength, trampoline + spec.stolenLength, target + spec.stolenLength);
    FlushInstructionCache(GetCurrentProcess(), trampoline, spec.stolenLength + jumpSize);

    std::vector<std::uint8_t> original(target, target + spec.stolenLength);
    std::vector<std::uint8_t> patchBytes(spec.stolenLength, 0x90);
    writeJump(patchBytes.data(), target, spec.replacement);
    auto detour = std::unique_ptr<Detour>(new Detour(target, trampoline, original, patchBytes));

    if (spec.trampolineOut != nullptr) {
        *spec.trampolineOut = trampoline;
    }
    auto written = patch(target, original, patchBytes);
    if (!written.ok()) {
        if (spec.trampolineOut != nullptr) {
            *spec.trampolineOut = nullptr;
        }
        return written.error();
    }
    detour->m_installed = true;
    return detour;
}

}  // namespace ra3::hook
