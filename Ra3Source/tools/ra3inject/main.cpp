#include <Windows.h>
#include <TlHelp32.h>

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <cwchar>
#include <iostream>
#include <string>
#include <vector>

namespace {

struct RemoteData {
    void* loadLibrary = nullptr;
    void* getLastError = nullptr;
    HMODULE module = nullptr;
    DWORD error = ERROR_SUCCESS;
    wchar_t path[32768]{};
};

void printWindowsError(const wchar_t* operation, DWORD error = GetLastError())
{
    std::wcerr << operation << L" failed with Windows error " << error << L'\n';
}

bool parsePid(const std::wstring& text, DWORD& pid)
{
    try {
        std::size_t used = 0;
        const unsigned long value = std::stoul(text, &used, 10);
        if (used != text.size() || value == 0 || value > MAXDWORD) {
            return false;
        }
        pid = static_cast<DWORD>(value);
        return true;
    } catch (...) {
        return false;
    }
}

std::vector<DWORD> findProcesses(const std::wstring& executable)
{
    std::vector<DWORD> matches;
    HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (snapshot == INVALID_HANDLE_VALUE) {
        return matches;
    }
    PROCESSENTRY32W entry{};
    entry.dwSize = sizeof(entry);
    for (BOOL found = Process32FirstW(snapshot, &entry); found; found = Process32NextW(snapshot, &entry)) {
        if (_wcsicmp(entry.szExeFile, executable.c_str()) == 0) {
            matches.push_back(entry.th32ProcessID);
        }
    }
    CloseHandle(snapshot);
    return matches;
}

bool is32BitProcess(HANDLE process)
{
    SYSTEM_INFO system{};
    GetNativeSystemInfo(&system);
    const bool system64 = system.wProcessorArchitecture == PROCESSOR_ARCHITECTURE_AMD64
        || system.wProcessorArchitecture == PROCESSOR_ARCHITECTURE_IA64
        || system.wProcessorArchitecture == PROCESSOR_ARCHITECTURE_ARM64;
    if (!system64) {
        return true;
    }
    BOOL wow64 = FALSE;
    return IsWow64Process(process, &wow64) && wow64 != FALSE;
}

struct WindowSearch {
    DWORD pid = 0;
    bool found = false;
};

BOOL CALLBACK findWindow(HWND window, LPARAM parameter)
{
    auto* search = reinterpret_cast<WindowSearch*>(parameter);
    DWORD pid = 0;
    GetWindowThreadProcessId(window, &pid);
    if (pid == search->pid && IsWindowVisible(window) && GetWindow(window, GW_OWNER) == nullptr) {
        search->found = true;
        return FALSE;
    }
    return TRUE;
}

bool waitForWindow(DWORD pid)
{
    for (unsigned attempt = 0; attempt < 600; ++attempt) {
        WindowSearch search{pid, false};
        EnumWindows(&findWindow, reinterpret_cast<LPARAM>(&search));
        if (search.found) {
            return true;
        }
        HANDLE process = OpenProcess(SYNCHRONIZE, FALSE, pid);
        if (process == nullptr) {
            return false;
        }
        const DWORD state = WaitForSingleObject(process, 100);
        CloseHandle(process);
        if (state == WAIT_OBJECT_0) {
            return false;
        }
    }
    return false;
}

void* remoteProcedureAddress(DWORD pid, const char* name)
{
    HMODULE kernel = GetModuleHandleW(L"kernel32.dll");
    FARPROC procedure = GetProcAddress(kernel, name);
    if (procedure == nullptr) {
        return nullptr;
    }
    MEMORY_BASIC_INFORMATION memory{};
    if (VirtualQuery(reinterpret_cast<const void*>(procedure), &memory, sizeof(memory)) == 0) {
        return nullptr;
    }
    const auto* localBase = static_cast<const std::uint8_t*>(memory.AllocationBase);
    std::array<wchar_t, MAX_PATH> modulePath{};
    if (GetModuleFileNameW(static_cast<HMODULE>(memory.AllocationBase), modulePath.data(), MAX_PATH) == 0) {
        return nullptr;
    }
    const wchar_t* moduleName = std::wcsrchr(modulePath.data(), L'\\');
    moduleName = moduleName == nullptr ? modulePath.data() : moduleName + 1;

    HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPMODULE | TH32CS_SNAPMODULE32, pid);
    if (snapshot == INVALID_HANDLE_VALUE) {
        return nullptr;
    }
    MODULEENTRY32W entry{};
    entry.dwSize = sizeof(entry);
    void* result = nullptr;
    for (BOOL found = Module32FirstW(snapshot, &entry); found; found = Module32NextW(snapshot, &entry)) {
        if (_wcsicmp(entry.szModule, moduleName) == 0) {
            const auto offset = reinterpret_cast<const std::uint8_t*>(procedure) - localBase;
            result = entry.modBaseAddr + offset;
            break;
        }
    }
    CloseHandle(snapshot);
    return result;
}

void append32(std::vector<std::uint8_t>& code, std::size_t value)
{
    const auto narrow = static_cast<std::uint32_t>(value);
    const auto* bytes = reinterpret_cast<const std::uint8_t*>(&narrow);
    code.insert(code.end(), bytes, bytes + sizeof(narrow));
}

std::vector<std::uint8_t> makeLoaderCode()
{
    std::vector<std::uint8_t> code = {
        0x56,                         // push esi
        0x8B, 0x74, 0x24, 0x08,       // mov esi,[esp+8]
        0x8D, 0x86};                  // lea eax,[esi+path]
    append32(code, offsetof(RemoteData, path));
    code.push_back(0x50);             // push eax
    code.insert(code.end(), {0xFF, 0x96});
    append32(code, offsetof(RemoteData, loadLibrary));
    code.insert(code.end(), {0x89, 0x86});
    append32(code, offsetof(RemoteData, module));
    code.insert(code.end(), {0x85, 0xC0, 0x75, 0x0C});
    code.insert(code.end(), {0xFF, 0x96});
    append32(code, offsetof(RemoteData, getLastError));
    code.insert(code.end(), {0x89, 0x86});
    append32(code, offsetof(RemoteData, error));
    code.insert(code.end(), {0x33, 0xC0, 0x5E, 0xC2, 0x04, 0x00});
    return code;
}

int inject(DWORD pid, const std::wstring& dllPath, bool waitWindow)
{
    if (waitWindow && !waitForWindow(pid)) {
        std::wcerr << L"no visible top-level window appeared within 60 seconds\n";
        return 1;
    }
    HANDLE process = OpenProcess(
        PROCESS_CREATE_THREAD | PROCESS_QUERY_INFORMATION | PROCESS_VM_OPERATION | PROCESS_VM_READ
            | PROCESS_VM_WRITE | SYNCHRONIZE,
        FALSE,
        pid);
    if (process == nullptr) {
        printWindowsError(L"OpenProcess");
        return 1;
    }
    if (!is32BitProcess(process)) {
        std::wcerr << L"target is not a 32-bit process\n";
        CloseHandle(process);
        return 1;
    }

    RemoteData data;
    if (dllPath.size() >= std::size(data.path)) {
        std::wcerr << L"DLL path is too long\n";
        CloseHandle(process);
        return 1;
    }
    // Module snapshots of a process that is still loading modules fail
    // transiently (ERROR_BAD_LENGTH / ERROR_PARTIAL_COPY).
    for (int attempt = 0; attempt < 50; ++attempt) {
        data.loadLibrary = remoteProcedureAddress(pid, "LoadLibraryW");
        data.getLastError = remoteProcedureAddress(pid, "GetLastError");
        if (data.loadLibrary != nullptr && data.getLastError != nullptr) {
            break;
        }
        Sleep(100);
    }
    if (data.loadLibrary == nullptr || data.getLastError == nullptr) {
        printWindowsError(L"resolving remote LoadLibraryW/GetLastError");
        CloseHandle(process);
        return 1;
    }
    std::copy(dllPath.begin(), dllPath.end(), data.path);
    data.path[dllPath.size()] = L'\0';

    const auto code = makeLoaderCode();
    const std::size_t totalSize = sizeof(data) + code.size();
    auto* remote = static_cast<std::uint8_t*>(
        VirtualAllocEx(process, nullptr, totalSize, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE));
    if (remote == nullptr) {
        printWindowsError(L"VirtualAllocEx");
        CloseHandle(process);
        return 1;
    }
    bool success = WriteProcessMemory(process, remote, &data, sizeof(data), nullptr) != FALSE
        && WriteProcessMemory(process, remote + sizeof(data), code.data(), code.size(), nullptr) != FALSE;
    DWORD oldProtection = 0;
    success = success
        && VirtualProtectEx(
               process, remote + sizeof(data), code.size(), PAGE_EXECUTE_READ, &oldProtection)
            != FALSE;
    if (!success) {
        printWindowsError(L"writing remote loader");
        VirtualFreeEx(process, remote, 0, MEM_RELEASE);
        CloseHandle(process);
        return 1;
    }
    FlushInstructionCache(process, remote + sizeof(data), code.size());

    HANDLE thread = CreateRemoteThread(
        process,
        nullptr,
        0,
        reinterpret_cast<LPTHREAD_START_ROUTINE>(remote + sizeof(data)),
        remote,
        0,
        nullptr);
    if (thread == nullptr) {
        printWindowsError(L"CreateRemoteThread");
        VirtualFreeEx(process, remote, 0, MEM_RELEASE);
        CloseHandle(process);
        return 1;
    }
    const DWORD wait = WaitForSingleObject(thread, 30000);
    if (wait != WAIT_OBJECT_0) {
        std::wcerr << L"remote loader did not finish within 30 seconds\n";
        CloseHandle(thread);
        CloseHandle(process);
        return 1;
    }
    if (!ReadProcessMemory(process, remote, &data, sizeof(data), nullptr)) {
        printWindowsError(L"ReadProcessMemory");
        CloseHandle(thread);
        VirtualFreeEx(process, remote, 0, MEM_RELEASE);
        CloseHandle(process);
        return 1;
    }

    std::wcout << L"remote LoadLibraryW result=0x" << std::hex
               << reinterpret_cast<std::uintptr_t>(data.module) << L" GetLastError=" << std::dec
               << data.error << L'\n';
    CloseHandle(thread);
    VirtualFreeEx(process, remote, 0, MEM_RELEASE);
    CloseHandle(process);
    return data.module == nullptr ? 1 : 0;
}

}  // namespace

int wmain(int argc, wchar_t** argv)
{
    DWORD pid = 0;
    std::wstring name;
    std::wstring dll;
    bool waitWindow = false;
    for (int index = 1; index < argc; ++index) {
        const std::wstring argument = argv[index];
        if (argument == L"--pid" && index + 1 < argc) {
            if (!parsePid(argv[++index], pid)) {
                std::wcerr << L"invalid PID\n";
                return 2;
            }
        } else if (argument == L"--name" && index + 1 < argc) {
            name = argv[++index];
        } else if (argument == L"--dll" && index + 1 < argc) {
            dll = argv[++index];
        } else if (argument == L"--wait-window") {
            waitWindow = true;
        } else {
            std::wcerr << L"unknown or incomplete argument: " << argument << L'\n';
            return 2;
        }
    }
    if ((pid == 0) == name.empty() || dll.empty()) {
        std::wcerr << L"usage: ra3inject (--pid PID | --name EXE) --dll PATH [--wait-window]\n";
        return 2;
    }
    if (!name.empty()) {
        const auto matches = findProcesses(name);
        if (matches.empty()) {
            std::wcerr << L"no process named " << name << L" was found\n";
            return 1;
        }
        if (matches.size() != 1) {
            std::wcerr << L"several processes named " << name << L" were found; use --pid\n";
            return 1;
        }
        pid = matches.front();
    }

    std::vector<wchar_t> fullPath(32768);
    const DWORD length =
        GetFullPathNameW(dll.c_str(), static_cast<DWORD>(fullPath.size()), fullPath.data(), nullptr);
    if (length == 0 || static_cast<std::size_t>(length) >= fullPath.size()
        || GetFileAttributesW(fullPath.data()) == INVALID_FILE_ATTRIBUTES) {
        std::wcerr << L"DLL path does not exist or is invalid\n";
        return 1;
    }
    return inject(pid, std::wstring(fullPath.data(), length), waitWindow);
}
