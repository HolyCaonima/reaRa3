#include <ra3/hook/Detour.h>
#include <ra3/io/RefPack.h>

#include <Windows.h>

#include <array>
#include <atomic>
#include <chrono>
#include <cstdint>
#include <cstring>
#include <exception>
#include <filesystem>
#include <fstream>
#include <limits>
#include <mutex>
#include <sstream>
#include <string>
#include <thread>
#include <vector>

namespace {

using DecodeFunction = int(__stdcall*)(std::uint8_t*, const std::uint8_t*);

constexpr std::array<std::uint8_t, 16> refPackExpected = {
    0x51, 0x8B, 0x4C, 0x24, 0x0C, 0x85, 0xC9, 0x56,
    0x8B, 0x74, 0x24, 0x0C, 0x75, 0x07, 0x33, 0xC0};
// ra3_1.0.game one-shot RefPack decoder (0x009F72E0).
constexpr std::uintptr_t refPackRva = 0x005F72E0;

enum class Mode {
    shadow,
    replace
};

struct Config {
    Mode mode = Mode::shadow;
    std::uintptr_t target = 0;
    std::filesystem::path logPath;
    unsigned summaryIntervalMs = 1000;
    bool testTarget = false;
};

HMODULE g_module = nullptr;
void* g_originalAddress = nullptr;
ra3::hook::Detour* g_detour = nullptr;
Mode g_mode = Mode::shadow;
std::atomic<std::uint64_t> g_calls{0};
std::atomic<std::uint64_t> g_matches{0};
std::atomic<std::uint64_t> g_mismatches{0};
std::atomic<unsigned> g_mismatchDetails{0};
std::mutex g_logMutex;
std::ofstream g_log;

std::string trim(std::string value)
{
    const auto first = value.find_first_not_of(" \t\r\n");
    if (first == std::string::npos) {
        return {};
    }
    const auto last = value.find_last_not_of(" \t\r\n");
    return value.substr(first, last - first + 1);
}

std::filesystem::path modulePath()
{
    std::vector<wchar_t> buffer(32768);
    const DWORD length = GetModuleFileNameW(g_module, buffer.data(), static_cast<DWORD>(buffer.size()));
    return std::filesystem::path(std::wstring(buffer.data(), length));
}

std::filesystem::path defaultLogPath()
{
    std::vector<wchar_t> buffer(32768);
    const DWORD length =
        GetEnvironmentVariableW(L"LOCALAPPDATA", buffer.data(), static_cast<DWORD>(buffer.size()));
    std::filesystem::path root =
        length != 0 && static_cast<std::size_t>(length) < buffer.size()
        ? std::filesystem::path(std::wstring(buffer.data(), length))
        : std::filesystem::temp_directory_path();
    return root / L"Ra3Source" / L"logs" / L"ra3hook.log";
}

void writeLine(const std::string& text)
{
    std::lock_guard<std::mutex> lock(g_logMutex);
    if (g_log) {
        g_log << text << '\n';
        g_log.flush();
    }
}

void writeSummary()
{
    std::ostringstream out;
    out << "summary calls=" << g_calls.load() << " matches=" << g_matches.load()
        << " mismatches=" << g_mismatches.load();
    writeLine(out.str());
}

bool parseUnsigned(const std::string& text, std::uintptr_t& value)
{
    try {
        std::size_t used = 0;
        const auto parsed = std::stoull(text, &used, 0);
        if (used != text.size() || parsed > std::numeric_limits<std::uintptr_t>::max()) {
            return false;
        }
        value = static_cast<std::uintptr_t>(parsed);
        return true;
    } catch (...) {
        return false;
    }
}

bool loadConfig(Config& config, std::string& error)
{
    std::filesystem::path path;
    std::vector<wchar_t> environment(32768);
    const DWORD length = GetEnvironmentVariableW(
        L"RA3_HOOK_CONFIG", environment.data(), static_cast<DWORD>(environment.size()));
    if (length != 0 && static_cast<std::size_t>(length) < environment.size()) {
        path = std::wstring(environment.data(), length);
    } else {
        path = modulePath().parent_path() / L"ra3hook.ini";
    }

    std::ifstream input(path);
    if (!input) {
        error = "cannot open config: " + path.string();
        return false;
    }
    std::string line;
    bool targetSet = false;
    while (std::getline(input, line)) {
        line = trim(line);
        if (line.empty() || line[0] == '#' || line[0] == ';') {
            continue;
        }
        const auto separator = line.find('=');
        if (separator == std::string::npos) {
            error = "invalid config line: " + line;
            return false;
        }
        const std::string key = trim(line.substr(0, separator));
        const std::string value = trim(line.substr(separator + 1));
        if (key == "mode") {
            if (value == "shadow") {
                config.mode = Mode::shadow;
            } else if (value == "replace") {
                config.mode = Mode::replace;
            } else {
                error = "mode must be shadow or replace";
                return false;
            }
        } else if (key == "target") {
            if (!parseUnsigned(value, config.target)) {
                error = "invalid target address";
                return false;
            }
            config.testTarget = true;
            targetSet = true;
        } else if (key == "target_rva") {
            std::uintptr_t rva = 0;
            if (!parseUnsigned(value, rva) || rva != refPackRva) {
                error = "target_rva is not the registered clean RefPack target";
                return false;
            }
            config.target = reinterpret_cast<std::uintptr_t>(GetModuleHandleW(nullptr)) + rva;
            targetSet = true;
        } else if (key == "log") {
            config.logPath = value;
        } else if (key == "summary_interval_ms") {
            std::uintptr_t interval = 0;
            if (!parseUnsigned(value, interval) || interval < 10 || interval > 60000) {
                error = "summary_interval_ms must be between 10 and 60000";
                return false;
            }
            config.summaryIntervalMs = static_cast<unsigned>(interval);
        } else {
            error = "unknown config key: " + key;
            return false;
        }
    }
    if (!targetSet) {
        config.target = reinterpret_cast<std::uintptr_t>(GetModuleHandleW(nullptr)) + refPackRva;
    }
    if (config.logPath.empty()) {
        config.logPath = defaultLogPath();
    }
    if (config.testTarget) {
        std::vector<wchar_t> executable(32768);
        const DWORD executableLength = GetModuleFileNameW(
            nullptr, executable.data(), static_cast<DWORD>(executable.size()));
        if (executableLength == 0
            || _wcsicmp(
                   std::filesystem::path(
                       std::wstring(executable.data(), executableLength)).filename().c_str(),
                   L"ra3_hook_dummy.exe")
                != 0) {
            error = "absolute target addresses are allowed only in the dummy target";
            return false;
        }
    }
    return true;
}

void logHostImage()
{
    const auto* base = reinterpret_cast<const std::uint8_t*>(GetModuleHandleW(nullptr));
    const auto* dos = reinterpret_cast<const IMAGE_DOS_HEADER*>(base);
    if (dos->e_magic != IMAGE_DOS_SIGNATURE) {
        writeLine("host PE header is invalid");
        return;
    }
    const auto* nt = reinterpret_cast<const IMAGE_NT_HEADERS*>(base + dos->e_lfanew);
    if (nt->Signature != IMAGE_NT_SIGNATURE) {
        writeLine("host NT header is invalid");
        return;
    }
    std::ostringstream out;
    out << std::hex << "host base=0x" << reinterpret_cast<std::uintptr_t>(base)
        << " timestamp=0x" << nt->FileHeader.TimeDateStamp
        << " image_size=0x" << nt->OptionalHeader.SizeOfImage;
    writeLine(out.str());
}

int __stdcall hookedDecode(std::uint8_t* destination, const std::uint8_t* source)
{
    g_calls.fetch_add(1);
    if (g_mode == Mode::shadow) {
        const int size = reinterpret_cast<DecodeFunction>(g_originalAddress)(destination, source);
        bool equal = size >= 0;
        try {
            std::vector<std::uint8_t> decoded;
            if (source == nullptr) {
                equal = size == 0;
            } else if (equal) {
                auto result = ra3::io::decompressRefPackPrefix(
                    source, std::numeric_limits<std::size_t>::max(), static_cast<std::size_t>(size));
                equal = result.ok();
                if (equal) {
                    decoded = std::move(result).value();
                    equal = decoded.empty()
                        || std::memcmp(destination, decoded.data(), decoded.size()) == 0;
                }
            }
        } catch (...) {
            equal = false;
        }
        if (equal) {
            g_matches.fetch_add(1);
        } else {
            g_mismatches.fetch_add(1);
            if (g_mismatchDetails.fetch_add(1) < 5) {
                try {
                    writeLine("shadow mismatch size=" + std::to_string(size));
                } catch (...) {
                }
            }
        }
        return size;
    }

    if (source == nullptr) {
        g_matches.fetch_add(1);
        return 0;
    }
    try {
        const auto header =
            ra3::io::readRefPackHeader(source, std::numeric_limits<std::size_t>::max());
        if (!header) {
            g_mismatches.fetch_add(1);
            return 0;
        }
        auto result = ra3::io::decompressRefPackPrefix(
            source, std::numeric_limits<std::size_t>::max(), header->decompressedSize);
        if (!result.ok()) {
            g_mismatches.fetch_add(1);
            return 0;
        }
        const auto& decoded = result.value();
        std::memcpy(destination, decoded.data(), decoded.size());
        g_matches.fetch_add(1);
        return static_cast<int>(decoded.size());
    } catch (...) {
        g_mismatches.fetch_add(1);
        return 0;
    }
}

DWORD runWorker()
{
    Config config;
    std::string error;
    if (!loadConfig(config, error)) {
        const auto path = defaultLogPath();
        std::filesystem::create_directories(path.parent_path());
        g_log.open(path, std::ios::app);
        writeLine(error);
        return 1;
    }

    std::filesystem::create_directories(config.logPath.parent_path());
    g_log.open(config.logPath, std::ios::app);
    if (!g_log) {
        return 1;
    }
    logHostImage();
    g_mode = config.mode;

    ra3::hook::HookSpec spec;
    spec.target = reinterpret_cast<void*>(config.target);
    spec.expectedBytes.assign(refPackExpected.begin(), refPackExpected.end());
    spec.stolenLength = 5;
    spec.replacement = reinterpret_cast<void*>(&hookedDecode);
    spec.trampolineOut = &g_originalAddress;
    auto installed = ra3::hook::installDetour(spec);
    if (!installed.ok()) {
        writeLine("install failed: " + installed.error().message);
        return 1;
    }
    g_detour = installed.value().release();
    writeLine(std::string("installed mode=") + (g_mode == Mode::shadow ? "shadow" : "replace"));

    for (;;) {
        std::this_thread::sleep_for(std::chrono::milliseconds(config.summaryIntervalMs));
        writeSummary();
    }
}

DWORD WINAPI worker(void*)
{
    try {
        return runWorker();
    } catch (const std::exception& exception) {
        writeLine(std::string("worker exception: ") + exception.what());
        return 1;
    } catch (...) {
        writeLine("worker exception: unknown");
        return 1;
    }
}

}  // namespace

BOOL WINAPI DllMain(HINSTANCE instance, DWORD reason, void*)
{
    if (reason == DLL_PROCESS_ATTACH) {
        g_module = instance;
        DisableThreadLibraryCalls(instance);
        HANDLE thread = CreateThread(nullptr, 0, &worker, nullptr, 0, nullptr);
        if (thread != nullptr) {
            CloseHandle(thread);
        }
    } else if (reason == DLL_PROCESS_DETACH) {
        // At process exit the worker has already been killed, possibly while
        // holding the log lock.
        if (g_logMutex.try_lock()) {
            g_logMutex.unlock();
            writeSummary();
        }
    }
    return TRUE;
}
