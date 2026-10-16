#include <Windows.h>

#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <thread>
#include <vector>

namespace {

std::wstring quote(const std::filesystem::path& path)
{
    return L"\"" + path.wstring() + L"\"";
}

bool createProcess(const std::wstring& command, PROCESS_INFORMATION& process)
{
    std::vector<wchar_t> writable(command.begin(), command.end());
    writable.push_back(L'\0');
    STARTUPINFOW startup{};
    startup.cb = sizeof(startup);
    process = {};
    return CreateProcessW(
               nullptr,
               writable.data(),
               nullptr,
               nullptr,
               FALSE,
               CREATE_NO_WINDOW,
               nullptr,
               nullptr,
               &startup,
               &process)
        != FALSE;
}

std::string readText(const std::filesystem::path& path)
{
    std::ifstream input(path);
    return std::string(std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>());
}

bool waitForText(const std::filesystem::path& path, const std::string& needle)
{
    for (unsigned attempt = 0; attempt < 400; ++attempt) {
        if (readText(path).find(needle) != std::string::npos) {
            return true;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(25));
    }
    return false;
}

bool runMode(
    const std::string& mode,
    const std::filesystem::path& dummy,
    const std::filesystem::path& injector,
    const std::filesystem::path& dll,
    const std::filesystem::path& root)
{
    const std::filesystem::path directory = root / mode;
    std::filesystem::create_directories(directory);
    const auto config = directory / "hook.ini";
    const auto log = directory / "hook.log";
    const auto addressFile = directory / "address.txt";
    const std::wstring suffix =
        std::to_wstring(GetCurrentProcessId()) + L"_" + std::wstring(mode.begin(), mode.end());
    const std::wstring readyName = L"Local\\Ra3HookReady_" + suffix;
    const std::wstring runName = L"Local\\Ra3HookRun_" + suffix;
    HANDLE ready = CreateEventW(nullptr, TRUE, FALSE, readyName.c_str());
    HANDLE run = CreateEventW(nullptr, TRUE, FALSE, runName.c_str());
    if (ready == nullptr || run == nullptr) {
        return false;
    }

    SetEnvironmentVariableW(L"RA3_HOOK_CONFIG", config.c_str());
    PROCESS_INFORMATION target{};
    const std::wstring targetCommand =
        quote(dummy) + L" \"" + readyName + L"\" \"" + runName + L"\" " + quote(addressFile);
    const bool targetStarted = createProcess(targetCommand, target);
    SetEnvironmentVariableW(L"RA3_HOOK_CONFIG", nullptr);
    if (!targetStarted) {
        CloseHandle(ready);
        CloseHandle(run);
        return false;
    }
    CloseHandle(target.hThread);

    bool success = WaitForSingleObject(ready, 10000) == WAIT_OBJECT_0;
    std::uintptr_t address = 0;
    if (success) {
        std::ifstream input(addressFile);
        input >> address;
        success = address != 0;
    }
    if (success) {
        std::ofstream output(config);
        output << "mode=" << mode << "\n"
               << "target=0x" << std::hex << address << "\n"
               << "log=" << log.string() << "\n"
               << "summary_interval_ms=50\n";
        success = static_cast<bool>(output);
    }

    if (success) {
        PROCESS_INFORMATION injection{};
        const std::wstring command = quote(injector) + L" --pid "
            + std::to_wstring(target.dwProcessId) + L" --dll " + quote(dll);
        success = createProcess(command, injection);
        if (success) {
            CloseHandle(injection.hThread);
            success = WaitForSingleObject(injection.hProcess, 30000) == WAIT_OBJECT_0;
            DWORD exitCode = 1;
            success = success && GetExitCodeProcess(injection.hProcess, &exitCode) && exitCode == 0;
            CloseHandle(injection.hProcess);
        }
    }
    success = success && waitForText(log, "installed refpack mode=" + mode);
    if (success) {
        SetEvent(run);
        success = WaitForSingleObject(target.hProcess, 10000) == WAIT_OBJECT_0;
        DWORD exitCode = 1;
        success = success && GetExitCodeProcess(target.hProcess, &exitCode) && exitCode == 0;
    }
    if (!success) {
        TerminateProcess(target.hProcess, 100);
        WaitForSingleObject(target.hProcess, 5000);
    }
    CloseHandle(target.hProcess);
    CloseHandle(ready);
    CloseHandle(run);

    const std::string contents = readText(log);
    success = success
        && contents.find("summary refpack calls=20 matches=20 mismatches=0") != std::string::npos
        && contents.find("namehash") == std::string::npos;
    if (!success) {
        std::cerr << mode << " integration log:\n" << contents;
    }
    return success;
}

}  // namespace

int wmain(int argc, wchar_t** argv)
{
    if (argc != 4) {
        std::cerr << "expected dummy, injector and DLL paths\n";
        return 2;
    }
    wchar_t temporary[MAX_PATH]{};
    if (GetTempPathW(MAX_PATH, temporary) == 0) {
        return 2;
    }
    const std::filesystem::path root =
        std::filesystem::path(temporary) / ("Ra3HookIntegration_" + std::to_string(GetCurrentProcessId()));
    std::filesystem::remove_all(root);
    const bool shadow = runMode("shadow", argv[1], argv[2], argv[3], root);
    const bool replace = runMode("replace", argv[1], argv[2], argv[3], root);
    std::filesystem::remove_all(root);
    return shadow && replace ? 0 : 1;
}
