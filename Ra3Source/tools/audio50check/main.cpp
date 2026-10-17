#include "ra3/audio/SageAudioManager50.h"

#include <windows.h>

#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

namespace {

constexpr std::uint32_t kNull = 0xFFFFFFFFu;
constexpr std::uint32_t kVtableMarker = 0xF1000000u;
constexpr std::uintptr_t kVtablePage = 0x20000000u;

std::uint32_t g_releaseStub = 0;
std::uint32_t g_stopStub = 0;
std::uint32_t g_headStub = 0;
std::uint32_t g_bodyStub = 0;
std::uintptr_t g_vtable = 0;

std::uint8_t* g_dee = nullptr;
std::uint8_t* g_ca7 = nullptr;
std::uint8_t* g_ca9 = nullptr;
std::uint8_t* g_bd9 = nullptr;
std::uint8_t* g_df0 = nullptr;
std::uint8_t* g_df6 = nullptr;

std::uint32_t __fastcall releaseStub(void* self, void*, int flag) {
    *reinterpret_cast<std::uint32_t*>(static_cast<std::uint8_t*>(self) + 8) =
        static_cast<std::uint32_t>(flag);
    return 0x42u;
}

std::uint32_t __fastcall stopStub(void* self, void*, int flag) {
    *reinterpret_cast<std::uint32_t*>(static_cast<std::uint8_t*>(self) + 4) =
        static_cast<std::uint32_t>(flag);
    return 0x55u;
}

std::uint32_t __fastcall transferHeadStub(void* self, void*, void* slot) {
    const auto* bytes = static_cast<const std::uint8_t*>(slot);
    *reinterpret_cast<std::uint32_t*>(static_cast<std::uint8_t*>(self) + 0x10) =
        bytes[0] | (static_cast<std::uint32_t>(bytes[1]) << 8);
    return 0x11u;
}

std::uint32_t __fastcall transferBodyStub(void* self, void*, void* arg) {
    *reinterpret_cast<void**>(static_cast<std::uint8_t*>(self) + 0x14) = arg;
    return 0x77u;
}

// VirtualAlloc reservations must start on the 64 KiB allocation granularity.
std::uint8_t* mapRegion(std::uintptr_t address, std::size_t bytes) {
    constexpr std::uintptr_t kGranularity = 0x10000u;
    const auto start = address & ~(kGranularity - 1u);
    const auto tail = (address + bytes + kGranularity - 1u) & ~(kGranularity - 1u);
    void* page = VirtualAlloc(reinterpret_cast<void*>(start), tail - start, MEM_COMMIT | MEM_RESERVE,
                              PAGE_READWRITE);
    if (page == nullptr) {
        std::cerr << "VirtualAlloc failed at 0x" << std::hex << start << " size 0x" << (tail - start)
                  << " err " << std::dec << GetLastError() << "\n";
        std::exit(1);
    }
    return static_cast<std::uint8_t*>(page);
}

// Python's process mitigation enables bottom-up ASLR, and a child inherits it.
// That packs private allocations over the game globals. Relaunch with it off
// and hand the same stdin/stdout to the inner process.
void relaunchForFixedAddresses() {
    if (GetEnvironmentVariableA("RA3_AUDIO50_INNER", nullptr, 0) != 0) {
        return;
    }
    if (!SetEnvironmentVariableA("RA3_AUDIO50_INNER", "1")) {
        std::cerr << "SetEnvironmentVariable failed\n";
        std::exit(1);
    }
    SIZE_T bytes = 0;
    InitializeProcThreadAttributeList(nullptr, 1, 0, &bytes);
    auto* attributes = static_cast<PPROC_THREAD_ATTRIBUTE_LIST>(HeapAlloc(GetProcessHeap(), 0, bytes));
    if (attributes == nullptr || !InitializeProcThreadAttributeList(attributes, 1, 0, &bytes)) {
        std::cerr << "InitializeProcThreadAttributeList failed\n";
        std::exit(1);
    }
    DWORD64 policy = PROCESS_CREATION_MITIGATION_POLICY_BOTTOM_UP_ASLR_ALWAYS_OFF |
                     PROCESS_CREATION_MITIGATION_POLICY_FORCE_RELOCATE_IMAGES_ALWAYS_OFF |
                     PROCESS_CREATION_MITIGATION_POLICY_HIGH_ENTROPY_ASLR_ALWAYS_OFF;
    if (!UpdateProcThreadAttribute(attributes, 0, PROC_THREAD_ATTRIBUTE_MITIGATION_POLICY, &policy,
                                   sizeof(policy), nullptr, nullptr)) {
        std::cerr << "UpdateProcThreadAttribute failed " << GetLastError() << "\n";
        std::exit(1);
    }
    STARTUPINFOEXA startup{};
    startup.StartupInfo.cb = sizeof(startup);
    startup.StartupInfo.dwFlags = STARTF_USESTDHANDLES;
    startup.StartupInfo.hStdInput = GetStdHandle(STD_INPUT_HANDLE);
    startup.StartupInfo.hStdOutput = GetStdHandle(STD_OUTPUT_HANDLE);
    startup.StartupInfo.hStdError = GetStdHandle(STD_ERROR_HANDLE);
    startup.lpAttributeList = attributes;
    char path[MAX_PATH] = {};
    GetModuleFileNameA(nullptr, path, MAX_PATH);
    std::string command = GetCommandLineA();
    PROCESS_INFORMATION process{};
    if (!CreateProcessA(path, command.data(), nullptr, nullptr, TRUE, EXTENDED_STARTUPINFO_PRESENT,
                        nullptr, nullptr, &startup.StartupInfo, &process)) {
        std::cerr << "CreateProcess failed " << GetLastError() << "\n";
        std::exit(1);
    }
    WaitForSingleObject(process.hProcess, INFINITE);
    DWORD code = 1;
    GetExitCodeProcess(process.hProcess, &code);
    CloseHandle(process.hThread);
    CloseHandle(process.hProcess);
    DeleteProcThreadAttributeList(attributes);
    HeapFree(GetProcessHeap(), 0, attributes);
    std::exit(static_cast<int>(code));
}

void installRuntime() {
    auto* dataBd = mapRegion(0x00BD0000u, 0x10000u);
    auto* dataCa = mapRegion(0x00CA0000u, 0x10000u);
    auto* dataDe = mapRegion(0x00DE0000u, 0x10000u);
    auto* dataDf = mapRegion(0x00DF0000u, 0x10000u);
    g_bd9 = dataBd + 0x9000u;
    g_ca7 = dataCa + 0x7000u;
    g_ca9 = dataCa + 0x9000u;
    g_dee = dataDe + 0xE000u;
    g_df0 = dataDf;
    g_df6 = dataDf + 0x6000u;
    auto* vtPage = mapRegion(kVtablePage, 0x1000u);
    g_vtable = reinterpret_cast<std::uintptr_t>(vtPage);
    auto* slots = reinterpret_cast<std::uintptr_t*>(vtPage);
    g_releaseStub = static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(&releaseStub));
    g_stopStub = static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(&stopStub));
    g_headStub = static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(&transferHeadStub));
    g_bodyStub = static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(&transferBodyStub));
    slots[0] = g_releaseStub;
    slots[6] = g_headStub;
    slots[14] = g_stopStub;
    slots[27] = g_bodyStub;
}

void baselineGlobals() {
    std::memset(g_dee, 0, 0x1000);
    std::memset(g_ca7, 0, 0x1000);
    std::memset(g_df0, 0, 0x1000);
    std::memset(g_df6, 0, 0x1000);
    *reinterpret_cast<float*>(g_bd9 + 0xA4) = 1.f;
    for (int i = 0; i < 8; ++i) {
        *reinterpret_cast<float*>(g_ca9 + 0xF04 + i * 4) = 1.f;
    }
}

int hexValue(char ch) {
    if (ch >= '0' && ch <= '9') {
        return ch - '0';
    }
    if (ch >= 'a' && ch <= 'f') {
        return ch - 'a' + 10;
    }
    if (ch >= 'A' && ch <= 'F') {
        return ch - 'A' + 10;
    }
    return -1;
}

std::vector<std::uint8_t> parseHex(const std::string& text) {
    std::vector<std::uint8_t> out;
    if (text == "-" || text == ".") {
        return out;
    }
    if ((text.size() % 2) != 0) {
        std::cerr << "odd hex length\n";
        std::exit(1);
    }
    out.reserve(text.size() / 2);
    for (std::size_t i = 0; i < text.size(); i += 2) {
        const int hi = hexValue(text[i]);
        const int lo = hexValue(text[i + 1]);
        if (hi < 0 || lo < 0) {
            std::cerr << "bad hex\n";
            std::exit(1);
        }
        out.push_back(static_cast<std::uint8_t>((hi << 4) | lo));
    }
    return out;
}

std::string toHex(const std::uint8_t* data, std::size_t size) {
    std::ostringstream out;
    out << std::hex << std::setfill('0');
    for (std::size_t i = 0; i < size; ++i) {
        out << std::setw(2) << static_cast<unsigned>(data[i]);
    }
    return out.str();
}

std::uint32_t parseU32(const std::string& text) {
    const int base = (text.size() > 1 && text[0] == '0' && text[1] != 'x' && text[1] != 'X' &&
                      text.find_first_of("abcdefABCDEF") != std::string::npos)
                         ? 16
                         : 0;
    return static_cast<std::uint32_t>(std::strtoul(text.c_str(), nullptr, base));
}

void applyPatches(const std::string& text) {
    if (text == "-" || text == ".") {
        return;
    }
    std::size_t start = 0;
    while (start < text.size()) {
        const auto comma = text.find(',', start);
        const auto item = text.substr(start, comma == std::string::npos ? std::string::npos : comma - start);
        const auto eq = item.find('=');
        if (eq == std::string::npos) {
            std::cerr << "bad patch\n";
            std::exit(1);
        }
        const auto address = parseU32(item.substr(0, eq));
        const auto bytes = parseHex(item.substr(eq + 1));
        std::memcpy(reinterpret_cast<void*>(address), bytes.data(), bytes.size());
        if (comma == std::string::npos) {
            break;
        }
        start = comma + 1;
    }
}

struct Fixup {
    enum Kind { Rel, Vtable, GlobalPtr } kind;
    std::uint32_t offset;
    std::uint32_t global;
};

std::vector<Fixup> parseFixups(const std::string& text) {
    std::vector<Fixup> out;
    if (text == "-" || text == ".") {
        return out;
    }
    std::size_t start = 0;
    while (start < text.size()) {
        const auto comma = text.find(',', start);
        const auto item = text.substr(start, comma == std::string::npos ? std::string::npos : comma - start);
        Fixup fix{};
        if (item.size() > 2 && item[0] == 't' && item[1] == '@') {
            fix.kind = Fixup::Vtable;
            fix.offset = parseU32(item.substr(2));
        } else if (!item.empty() && item[0] == 'g') {
            const auto at = item.find('@');
            fix.kind = Fixup::GlobalPtr;
            fix.global = parseU32(item.substr(1, at - 1));
            fix.offset = parseU32(item.substr(at + 1));
        } else {
            fix.kind = Fixup::Rel;
            fix.offset = parseU32(item);
        }
        out.push_back(fix);
        if (comma == std::string::npos) {
            break;
        }
        start = comma + 1;
    }
    return out;
}

void* blobPtr(std::uint8_t* blob, std::size_t size, std::uint32_t offset) {
    if (offset == kNull || blob == nullptr || offset >= size) {
        return nullptr;
    }
    return blob + offset;
}

std::uint32_t stubMarker(std::uint32_t value) {
    if (value == g_releaseStub) {
        return 0xF0000000u;
    }
    if (value == g_stopStub) {
        return 0xF0000001u;
    }
    if (value == g_headStub) {
        return 0xF0000002u;
    }
    if (value == g_bodyStub) {
        return 0xF0000003u;
    }
    return 0u;
}

std::uint32_t normalize(std::uint32_t value, std::uintptr_t base, std::size_t size) {
    if (size != 0 && value >= base && value < base + size) {
        return static_cast<std::uint32_t>(value - base);
    }
    if (value == static_cast<std::uint32_t>(g_vtable)) {
        return kVtableMarker;
    }
    if (const auto marker = stubMarker(value)) {
        return marker;
    }
    if (size == 0) {
        return value;
    }
    const auto high = value & 0xFFFFFF00u;
    const auto lo = static_cast<std::uint32_t>(base) & 0xFFFFFF00u;
    const auto hi = static_cast<std::uint32_t>(base + size - 1) & 0xFFFFFF00u;
    if (high >= lo && high <= hi) {
        return value - lo;
    }
    return value;
}

void normalizeDwords(std::uint8_t* data, std::size_t size, std::uintptr_t base, std::size_t span) {
    for (std::size_t off = 0; off + 4 <= size; off += 4) {
        std::uint32_t cur = 0;
        std::memcpy(&cur, data + off, 4);
        const auto next = normalize(cur, base, span);
        if (next != cur) {
            std::memcpy(data + off, &next, 4);
        }
    }
}

std::vector<std::uint8_t> captureGlobals(std::uintptr_t blobBase, std::size_t blobSize) {
    std::vector<std::uint8_t> out(4u * 5u + 0x5F4u);
    std::memcpy(out.data() + 0, reinterpret_cast<void*>(0x00DEE1C8u), 4);
    std::memcpy(out.data() + 4, reinterpret_cast<void*>(0x00CA7D7Cu), 4);
    std::memcpy(out.data() + 8, reinterpret_cast<void*>(0x00CA7E7Cu), 4);
    std::memcpy(out.data() + 12, reinterpret_cast<void*>(0x00DF03F8u), 4);
    std::memcpy(out.data() + 16, reinterpret_cast<void*>(0x00DF6098u), 4);
    std::memcpy(out.data() + 20, reinterpret_cast<void*>(0x00DEE290u), 0x5F4);
    normalizeDwords(out.data(), out.size(), blobBase, blobSize);
    return out;
}

std::uint32_t run(std::uint32_t va, void* self, std::uint8_t* blob, std::size_t blobSize, std::uint32_t a0,
                  std::uint32_t a1, std::uint32_t a2, std::uint32_t a3, std::uint32_t eax) {
    using namespace ra3::audio;
    switch (va) {
    case 0x00502270:
        return resolveSpeakerCenterChannel();
    case 0x00502FB0:
        return sageAudioManagerEmptyEventAddress();
    case 0x00502FF0:
        return sageAudioManagerStopAllImmediately(self);
    case 0x00503290:
        return sageAudioManagerListenerOrientationCount();
    case 0x00503300:
        return sageAudioManagerSubsystemNameAddress();
    case 0x00503400:
        return sagePathMusicManagerSubsystemNameAddress();
    case 0x005044F0:
        return static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(
            initAudioEventHandle(self, blobPtr(blob, blobSize, a0))));
    case 0x00504560:
        return releaseAudioEventHandle(self);
    case 0x005045C0:
        return releasePlayingAudioHandle(self, eax);
    case 0x005045E0:
        return unlinkPlayingAudioNode(self);
    case 0x00504AC0:
        return testAudioEventCategoryBit(self, a0);
    case 0x00504AF0:
        return intersectsAudioEventCategoryMask(self, reinterpret_cast<std::uint32_t*>(blobPtr(blob, blobSize, a0)));
    case 0x00504C80:
        recomputePlayerCategoryVolume(self, a0, a1);
        return 0;
    case 0x00504DF0:
        return recomputePlayerCategoryProduct(self, a0);
    case 0x00504EC0:
        return decrementPlayerVolumeCounters(self);
    case 0x00505340:
        return acquireVolumeModifierLock(blobPtr(blob, blobSize, a0));
    case 0x005053B0:
        return releaseVolumeModifierLock(blobPtr(blob, blobSize, a0));
    case 0x00506670:
        return sageAudioManagerOnAcquireVolumeModifierLock(blobPtr(blob, blobSize, a0));
    case 0x00506680:
        return sageAudioManagerOnReleaseVolumeModifierLock(blobPtr(blob, blobSize, a0));
    case 0x00506430:
        return sageAudioManagerQuerySuppressed(blobPtr(blob, blobSize, a0), eax);
    case 0x00506C20:
        return static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(
            initCategoryBitsFour(self, a0, a1, a2, a3)));
    case 0x00506C90:
        return static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(
            initCategoryBitsTwo(self, a0, a1)));
    case 0x00502540:
        return bindSoundProvider(self, blobPtr(blob, blobSize, a0));
    case 0x00507720:
        return hasActiveRenderCursor(self);
    case 0x00508640:
        return static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(
            appendPlayingAudioNode(self, blobPtr(blob, blobSize, a0))));
    case 0x0050B090:
        return sagePathMusicManagerTransferState(self, blobPtr(blob, blobSize, a0));
    case 0x0050DF80:
        return eraseListenerRecord(self, a0);
    case 0x0050E510:
        return sageAudioManagerRemoveListenerEntry(self, a0, eax);
    case 0x0050FA80:
        return applyListedVolumeModifiers(self, blobPtr(blob, blobSize, a0), a1);
    case 0x0050FD50:
        return refreshRenderingPlayerVolumes(self);
    default:
        std::cerr << "unknown va\n";
        std::exit(1);
    }
}

void runLine(const std::string& line) {
    std::istringstream iss(line);
    std::string vaStr;
    std::string selfStr;
    std::string blobHex;
    std::string fixStr;
    std::string a0Str;
    std::string a1Str;
    std::string a2Str;
    std::string a3Str;
    std::string eaxStr;
    std::string fpcwStr;
    std::string patchStr;
    if (!(iss >> vaStr >> selfStr >> blobHex >> fixStr >> a0Str >> a1Str >> a2Str >> a3Str >>
          eaxStr >> fpcwStr >> patchStr)) {
        std::cerr << "bad line\n";
        std::exit(1);
    }
    baselineGlobals();
    applyPatches(patchStr);
    const auto raw = parseHex(blobHex);
    // 256-byte alignment keeps "mov al, mem" residues at the same blob offset
    // on both sides of the oracle. malloc's 16-byte alignment does not.
    std::vector<std::uint8_t> holder(raw.size() + 255u);
    const auto base = raw.empty()
                         ? 0u
                         : (reinterpret_cast<std::uintptr_t>(holder.data()) + 255u) & ~std::uintptr_t{255u};
    auto* blob = raw.empty() ? nullptr : reinterpret_cast<std::uint8_t*>(base);
    if (!raw.empty()) {
        std::memcpy(blob, raw.data(), raw.size());
    }
    for (const auto& fix : parseFixups(fixStr)) {
        if (fix.kind == Fixup::Vtable) {
            std::memcpy(blob + fix.offset, &g_vtable, 4);
        } else if (fix.kind == Fixup::GlobalPtr) {
            const auto target = static_cast<std::uint32_t>(base + fix.offset);
            std::memcpy(reinterpret_cast<void*>(fix.global), &target, 4);
        } else {
            std::uint32_t rel = 0;
            std::memcpy(&rel, blob + fix.offset, 4);
            const auto abs = static_cast<std::uint32_t>(base + rel);
            std::memcpy(blob + fix.offset, &abs, 4);
        }
    }
    const bool absolute = !selfStr.empty() && (selfStr[0] == 'A' || selfStr[0] == 'a');
    const auto selfOff = parseU32(absolute ? selfStr.substr(1) : selfStr);
    void* self = nullptr;
    if (absolute) {
        self = reinterpret_cast<void*>(selfOff);
    } else if (blob != nullptr && selfOff != kNull && selfOff < raw.size()) {
        self = blob + selfOff;
    }
    const auto fpcw = static_cast<std::uint16_t>(parseU32(fpcwStr));
    unsigned short saved = 0;
    __asm {
        fnstcw saved
        fldcw fpcw
    }
    auto ret = run(parseU32(vaStr), self, blob, raw.size(), parseU32(a0Str), parseU32(a1Str),
                   parseU32(a2Str), parseU32(a3Str), parseU32(eaxStr));
    __asm { fldcw saved }
    if (!raw.empty()) {
        normalizeDwords(blob, raw.size(), base, raw.size());
        ret = normalize(ret, base, raw.size());
    }
    const auto globals = captureGlobals(base, raw.size());
    std::cout << "ret=0x" << std::hex << std::setw(8) << std::setfill('0') << ret << " blob="
              << toHex(blob, raw.size()) << " g=" << toHex(globals.data(), globals.size()) << std::endl;
}

}  // namespace

int main(int argc, char** argv) {
    relaunchForFixedAddresses();
    installRuntime();
    if (argc == 2 && std::string(argv[1]) == "--batch") {
        std::string line;
        while (std::getline(std::cin, line)) {
            if (!line.empty()) {
                runLine(line);
            }
        }
        return 0;
    }
    std::cerr << "usage: ra3_audio50check --batch\n";
    return 1;
}
