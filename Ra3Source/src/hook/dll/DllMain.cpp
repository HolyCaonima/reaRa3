#include "Targets.h"

#include <ra3/hook/Detour.h>

#include <Windows.h>

#include <chrono>
#include <cstdint>
#include <exception>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <limits>
#include <map>
#include <memory>
#include <mutex>
#include <sstream>
#include <string>
#include <thread>
#include <vector>

namespace ra3hook {
namespace {

// Config (ra3hook.ini next to the DLL, or RA3_HOOK_CONFIG):
//   mode=shadow|replace|off       default for every target
//   <target key>=shadow|replace|off
//   target=0x...                  dummy process only: absolute address for "refpack"
//   log=PATH, summary_interval_ms=N
struct Config {
    Mode defaultMode = Mode::shadow;
    std::map<std::string, Mode> modes;
    std::uintptr_t testAddress = 0;
    std::filesystem::path logPath;
    unsigned summaryIntervalMs = 1000;
};

HMODULE g_module = nullptr;
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

std::filesystem::path modulePath(HMODULE module)
{
    std::vector<wchar_t> buffer(32768);
    const DWORD length = GetModuleFileNameW(module, buffer.data(), static_cast<DWORD>(buffer.size()));
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

void writeSummary()
{
    for (const Target& target : targets()) {
        const TargetState& state = *target.state;
        if (state.mode == Mode::off) {
            continue;
        }
        std::ostringstream out;
        out << "summary " << target.key << " calls=" << state.calls.load()
            << " matches=" << state.matches.load()
            << " mismatches=" << state.mismatches.load()
            << " skipped=" << state.skipped.load();
        const std::uint32_t controlWord = state.firstFpuControlWord.load();
        if (controlWord != 0) {
            out << std::hex << std::uppercase << std::setfill('0')
                << " cw=0x" << std::setw(4) << controlWord
                << " mxcsr=0x" << std::setw(8) << state.firstMxcsr.load()
                << std::dec
                << " cw_changes=" << state.fpuControlWordChanges.load();
        }
        writeLine(out.str());
    }
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

bool parseMode(const std::string& text, Mode& mode)
{
    for (const Mode candidate : {Mode::off, Mode::shadow, Mode::replace}) {
        if (text == modeName(candidate)) {
            mode = candidate;
            return true;
        }
    }
    return false;
}

const Target* findTarget(const std::string& key)
{
    for (const Target& target : targets()) {
        if (key == target.key) {
            return &target;
        }
    }
    return nullptr;
}

bool isDummyHost()
{
    const auto name = modulePath(nullptr).filename();
    return _wcsicmp(name.c_str(), L"ra3_hook_dummy.exe") == 0;
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
        path = modulePath(g_module).parent_path() / L"ra3hook.ini";
    }

    std::ifstream input(path);
    if (!input) {
        error = "cannot open config: " + path.string();
        return false;
    }
    std::string line;
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
        if (key == "mode" || findTarget(key) != nullptr) {
            Mode mode = Mode::off;
            if (!parseMode(value, mode)) {
                error = key + " must be off, shadow or replace";
                return false;
            }
            if (key == "mode") {
                config.defaultMode = mode;
            } else {
                config.modes[key] = mode;
            }
        } else if (key == "target") {
            if (!parseUnsigned(value, config.testAddress) || config.testAddress == 0) {
                error = "invalid target address";
                return false;
            }
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
    if (config.testAddress != 0 && !isDummyHost()) {
        error = "absolute target addresses are allowed only in the dummy target";
        return false;
    }
    if (config.logPath.empty()) {
        config.logPath = defaultLogPath();
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

Mode modeFor(const Config& config, const Target& target)
{
    if (config.testAddress != 0 && std::string(target.key) != "refpack") {
        return Mode::off;
    }
    const auto found = config.modes.find(target.key);
    if (found != config.modes.end()) {
        return found->second;
    }
    return target.explicitOnly ? Mode::off : config.defaultMode;
}

// Installs every enabled target or none of them.
bool installTargets(const Config& config)
{
    const auto base = reinterpret_cast<std::uintptr_t>(GetModuleHandleW(nullptr));
    std::vector<std::pair<const Target*, std::unique_ptr<ra3::hook::Detour>>> installed;
    for (const Target& target : targets()) {
        target.state->mode = modeFor(config, target);
        if (target.state->mode == Mode::off) {
            continue;
        }
        ra3::hook::HookSpec spec;
        spec.target = reinterpret_cast<void*>(
            config.testAddress != 0 ? config.testAddress : base + target.rva);
        spec.expectedBytes = target.expected;
        spec.stolenLength = target.stolenLength;
        spec.rel32Offsets = target.rel32Offsets;
        spec.replacement = target.replacement;
        spec.trampolineOut = &target.state->original;
        auto detour = ra3::hook::installDetour(spec);
        if (!detour.ok()) {
            writeLine(std::string("install failed: ") + target.key + ": " + detour.error().message);
            for (auto it = installed.rbegin(); it != installed.rend(); ++it) {
                if (!it->second->uninstall().ok()) {
                    writeLine(std::string("uninstall failed: ") + it->first->key);
                }
                it->first->state->mode = Mode::off;
            }
            target.state->mode = Mode::off;
            return false;
        }
        installed.emplace_back(&target, std::move(detour).value());
    }
    for (auto& item : installed) {
        writeLine(std::string("installed ") + item.first->key + " mode=" + modeName(item.first->state->mode));
        // Patches stay in place for the life of the process.
        (void)item.second.release();
    }
    return true;
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
    if (!installTargets(config)) {
        return 1;
    }

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

void writeLine(const std::string& text)
{
    std::lock_guard<std::mutex> lock(g_logMutex);
    if (g_log) {
        g_log << text << '\n';
        g_log.flush();
    }
}

}  // namespace ra3hook

BOOL WINAPI DllMain(HINSTANCE instance, DWORD reason, void*)
{
    if (reason == DLL_PROCESS_ATTACH) {
        ra3hook::g_module = instance;
        DisableThreadLibraryCalls(instance);
        HANDLE thread = CreateThread(nullptr, 0, &ra3hook::worker, nullptr, 0, nullptr);
        if (thread != nullptr) {
            CloseHandle(thread);
        }
    } else if (reason == DLL_PROCESS_DETACH) {
        // At process exit the worker has already been killed, possibly while
        // holding the log lock.
        if (ra3hook::g_logMutex.try_lock()) {
            ra3hook::g_logMutex.unlock();
            ra3hook::writeSummary();
        }
    }
    return TRUE;
}
