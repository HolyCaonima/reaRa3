#include "RuntimeDump.h"

#include "Targets.h"

#include <Windows.h>

#include <TlHelp32.h>

#include <algorithm>
#include <cstring>
#include <fstream>
#include <iomanip>
#include <sstream>
#include <system_error>
#include <utility>

namespace ra3hook {
namespace {

constexpr std::uintptr_t pageSize = 0x1000;
constexpr std::uintptr_t addressLimit = 0x7FFF0000;
constexpr std::size_t readChunk = 0x100000;
constexpr std::size_t privateRegionCap = 64u << 20;

struct Module {
    std::uintptr_t start = 0;
    std::size_t size = 0;
    std::string name;
};

// One VirtualQuery region inside a copied range.
struct Run {
    std::uintptr_t start = 0;
    std::size_t size = 0;
    DWORD state = 0;
    DWORD protect = 0;
    DWORD type = 0;
    std::size_t failedPages = 0;
    bool readable = false;
};

std::string hex(std::uintptr_t value)
{
    std::ostringstream out;
    out << std::hex << std::setfill('0') << std::setw(8) << value;
    return out.str();
}

std::string hexBytes(const std::uint8_t* bytes, std::size_t size)
{
    std::ostringstream out;
    out << std::hex << std::setfill('0');
    for (std::size_t i = 0; i < size; ++i) {
        out << std::setw(2) << static_cast<unsigned>(bytes[i]);
    }
    return out.str();
}

std::string narrow(const wchar_t* text)
{
    const int length = WideCharToMultiByte(CP_UTF8, 0, text, -1, nullptr, 0, nullptr, nullptr);
    if (length <= 1) {
        return {};
    }
    std::string result(static_cast<std::size_t>(length - 1), '\0');
    WideCharToMultiByte(CP_UTF8, 0, text, -1, result.data(), length, nullptr, nullptr);
    return result;
}

bool isReadable(const MEMORY_BASIC_INFORMATION& info)
{
    return info.State == MEM_COMMIT && info.Protect != 0
        && (info.Protect & (PAGE_GUARD | PAGE_NOACCESS)) == 0;
}

// ReadProcessMemory on the own process fails instead of faulting if a page
// changed protection since it was queried.
bool readMemory(std::uintptr_t address, void* out, std::size_t size)
{
    SIZE_T copied = 0;
    return ReadProcessMemory(GetCurrentProcess(), reinterpret_cast<const void*>(address), out, size,
                             &copied) != 0
        && copied == size;
}

bool query(std::uintptr_t address, MEMORY_BASIC_INFORMATION& info)
{
    return VirtualQuery(reinterpret_cast<const void*>(address), &info, sizeof(info)) == sizeof(info);
}

std::vector<Run> copyRange(std::uintptr_t start, std::size_t size, std::vector<std::uint8_t>& out)
{
    out.assign(size, 0);
    std::vector<Run> runs;
    const std::uintptr_t end = start + size;
    std::uintptr_t cursor = start;
    while (cursor < end) {
        MEMORY_BASIC_INFORMATION info{};
        Run run;
        run.start = cursor;
        if (!query(cursor, info)) {
            run.size = end - cursor;
            runs.push_back(run);
            break;
        }
        const auto regionEnd = std::min<std::uintptr_t>(
            reinterpret_cast<std::uintptr_t>(info.BaseAddress) + info.RegionSize, end);
        run.size = regionEnd - cursor;
        run.state = info.State;
        run.protect = info.Protect;
        run.type = info.Type;
        run.readable = isReadable(info);
        if (run.readable) {
            for (std::uintptr_t chunk = cursor; chunk < regionEnd;) {
                const std::size_t length = std::min<std::size_t>(regionEnd - chunk, readChunk);
                if (!readMemory(chunk, out.data() + (chunk - start), length)) {
                    for (std::uintptr_t page = chunk; page < chunk + length; page += pageSize) {
                        const std::size_t pageLength =
                            std::min<std::size_t>(pageSize, chunk + length - page);
                        if (!readMemory(page, out.data() + (page - start), pageLength)) {
                            std::memset(out.data() + (page - start), 0, pageLength);
                            ++run.failedPages;
                        }
                    }
                }
                chunk += length;
            }
        }
        runs.push_back(run);
        cursor = regionEnd;
    }
    return runs;
}

std::vector<Module> listModules()
{
    std::vector<Module> modules;
    HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPMODULE, 0);
    if (snapshot == INVALID_HANDLE_VALUE) {
        return modules;
    }
    MODULEENTRY32W entry{};
    entry.dwSize = sizeof(entry);
    for (BOOL more = Module32FirstW(snapshot, &entry); more; more = Module32NextW(snapshot, &entry)) {
        modules.push_back({reinterpret_cast<std::uintptr_t>(entry.modBaseAddr), entry.modBaseSize,
                           narrow(entry.szModule)});
    }
    CloseHandle(snapshot);
    return modules;
}

const Module* moduleAt(const std::vector<Module>& modules, std::uintptr_t address)
{
    for (const Module& module : modules) {
        if (address >= module.start && address - module.start < module.size) {
            return &module;
        }
    }
    return nullptr;
}

void appendLine(const std::filesystem::path& path, const std::string& line)
{
    std::ofstream out(path, std::ios::app);
    out << line << '\n';
}

bool writeFile(const std::filesystem::path& path, const std::vector<std::uint8_t>& bytes)
{
    std::ofstream out(path, std::ios::binary | std::ios::trunc);
    out.write(reinterpret_cast<const char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
    return static_cast<bool>(out);
}

}  // namespace

RuntimeDump::RuntimeDump(DumpConfig config) : config_(std::move(config)) {}

const RuntimeDump::Section* RuntimeDump::section(const char* name) const
{
    for (const Section& candidate : sections_) {
        if (candidate.name == name) {
            return &candidate;
        }
    }
    return nullptr;
}

std::uint64_t RuntimeDump::elapsedMs() const
{
    return static_cast<std::uint64_t>(std::chrono::duration_cast<std::chrono::milliseconds>(
                                          std::chrono::steady_clock::now() - started_)
                                          .count());
}

bool RuntimeDump::start(std::string& error)
{
    started_ = std::chrono::steady_clock::now();
    const auto* base = reinterpret_cast<const std::uint8_t*>(GetModuleHandleW(nullptr));
    const auto* dos = reinterpret_cast<const IMAGE_DOS_HEADER*>(base);
    const auto* nt = reinterpret_cast<const IMAGE_NT_HEADERS32*>(base + dos->e_lfanew);
    if (dos->e_magic != IMAGE_DOS_SIGNATURE || nt->Signature != IMAGE_NT_SIGNATURE) {
        error = "dump: host PE header is invalid";
        return false;
    }
    imageBase_ = reinterpret_cast<std::uintptr_t>(base);
    imageSize_ = nt->OptionalHeader.SizeOfImage;
    const IMAGE_SECTION_HEADER* header = IMAGE_FIRST_SECTION(nt);
    for (WORD i = 0; i < nt->FileHeader.NumberOfSections; ++i, ++header) {
        const auto* name = reinterpret_cast<const char*>(header->Name);
        Section entry;
        entry.name.assign(name, strnlen(name, IMAGE_SIZEOF_SHORT_NAME));
        entry.start = imageBase_ + header->VirtualAddress;
        entry.size = (std::max<std::size_t>(header->Misc.VirtualSize, header->SizeOfRawData) + pageSize - 1)
            & ~(pageSize - 1);
        entry.characteristics = header->Characteristics;
        sections_.push_back(entry);
    }
    const Section* text = section(".text");
    const Section* securom = section(".securom");
    if (text == nullptr || securom == nullptr) {
        error = "dump: host has no .text or .securom section";
        return false;
    }

    std::error_code created;
    std::filesystem::create_directories(config_.directory, created);
    if (created) {
        error = "dump: cannot create " + config_.directory.string() + ": " + created.message();
        return false;
    }

    std::vector<std::uint8_t> code;
    const auto runs = copyRange(text->start, text->size, code);
    for (const Run& run : runs) {
        if (!run.readable || run.failedPages != 0) {
            error = "dump: .text is not fully readable at 0x" + hex(run.start);
            return false;
        }
    }
    for (std::size_t offset = 0; offset + 6 <= code.size(); ++offset) {
        if (code[offset] != 0xFF || (code[offset + 1] != 0x25 && code[offset + 1] != 0x15)) {
            continue;
        }
        std::uint32_t slot = 0;
        std::memcpy(&slot, &code[offset + 2], sizeof(slot));
        if (slot < securom->start || slot - securom->start >= securom->size) {
            continue;
        }
        Site site;
        site.address = text->start + offset;
        site.kind = code[offset + 1] == 0x25 ? 'j' : 'c';
        site.slot = slot;
        std::memcpy(site.bytes, &code[offset], sizeof(site.bytes));
        sites_.push_back(site);
        std::uint32_t value = 0;
        if (slots_.count(slot) == 0) {
            slots_[slot] = readMemory(slot, &value, sizeof(value)) ? value : 0;
        }
    }

    std::ofstream manifest(config_.directory / "manifest.txt", std::ios::trunc);
    manifest << "image_base " << hex(imageBase_) << "\nimage_size " << hex(imageSize_)
             << "\ntimestamp " << hex(nt->FileHeader.TimeDateStamp) << "\nentry "
             << hex(imageBase_ + nt->OptionalHeader.AddressOfEntryPoint) << "\nprocess "
             << GetCurrentProcessId() << "\nsnapshot_seconds";
    for (const unsigned seconds : config_.snapshotSeconds) {
        manifest << ' ' << seconds;
    }
    manifest << "\npoll_ms " << config_.pollIntervalMs << '\n';
    for (const Section& entry : sections_) {
        manifest << "section " << entry.name << ' ' << hex(entry.start) << ' ' << hex(entry.size) << ' '
                 << hex(entry.characteristics) << '\n';
    }
    manifest.close();

    std::ofstream table(config_.directory / "sites.tsv", std::ios::trunc);
    table << "site\tkind\tslot\tvalue\tbytes\n";
    for (const Site& site : sites_) {
        table << hex(site.address) << '\t' << site.kind << '\t' << hex(site.slot) << '\t'
              << hex(slots_[site.slot]) << '\t' << hexBytes(site.bytes, sizeof(site.bytes)) << '\n';
    }
    table.close();
    appendLine(config_.directory / "slot_changes.tsv", "ms\tslot\told\tnew");
    appendLine(config_.directory / "site_changes.tsv", "ms\tsite\told\tnew");

    std::ostringstream line;
    line << "dump: " << sites_.size() << " sites, " << slots_.size() << " slots -> "
         << config_.directory.string();
    writeLine(line.str());
    return true;
}

void RuntimeDump::poll()
{
    const std::uint64_t now = elapsedMs();
    for (auto& [slot, value] : slots_) {
        std::uint32_t current = 0;
        if (readMemory(slot, &current, sizeof(current)) && current != value) {
            appendLine(config_.directory / "slot_changes.tsv",
                       std::to_string(now) + '\t' + hex(slot) + '\t' + hex(value) + '\t' + hex(current));
            value = current;
        }
    }
    for (Site& site : sites_) {
        std::uint8_t current[sizeof(site.bytes)]{};
        if (readMemory(site.address, current, sizeof(current))
            && std::memcmp(current, site.bytes, sizeof(current)) != 0) {
            appendLine(config_.directory / "site_changes.tsv",
                       std::to_string(now) + '\t' + hex(site.address) + '\t'
                           + hexBytes(site.bytes, sizeof(site.bytes)) + '\t'
                           + hexBytes(current, sizeof(current)));
            std::memcpy(site.bytes, current, sizeof(current));
        }
    }
}

void RuntimeDump::tick()
{
    poll();
    while (nextSnapshot_ < config_.snapshotSeconds.size()
           && elapsedMs() >= std::uint64_t{config_.snapshotSeconds[nextSnapshot_]} * 1000) {
        writeSnapshot(nextSnapshot_, config_.snapshotSeconds[nextSnapshot_]);
        ++nextSnapshot_;
    }
}

void RuntimeDump::writeSnapshot(std::size_t index, unsigned seconds)
{
    const auto began = elapsedMs();
    const auto directory =
        config_.directory / ("snapshot_" + std::to_string(index) + "_" + std::to_string(seconds) + "s");
    std::error_code created;
    std::filesystem::create_directories(directory, created);
    if (created) {
        writeLine("dump: cannot create " + directory.string());
        return;
    }
    const std::vector<Module> modules = listModules();
    auto where = [&](std::uintptr_t address) -> std::string {
        if (address >= imageBase_ && address - imageBase_ < imageSize_) {
            for (const Section& entry : sections_) {
                if (address >= entry.start && address - entry.start < entry.size) {
                    return entry.name;
                }
            }
            return "image";
        }
        const Module* module = moduleAt(modules, address);
        return module != nullptr ? module->name : std::string();
    };

    std::ofstream map(directory / "memory_map.tsv", std::ios::trunc);
    map << "base\tsize\talloc_base\talloc_protect\tstate\tprotect\ttype\twhere\n";
    for (std::uintptr_t cursor = 0x10000; cursor < addressLimit;) {
        MEMORY_BASIC_INFORMATION info{};
        if (!query(cursor, info) || info.RegionSize == 0) {
            break;
        }
        const auto regionBase = reinterpret_cast<std::uintptr_t>(info.BaseAddress);
        if (info.State != MEM_FREE) {
            std::string label = where(regionBase);
            if (label.empty()) {
                label = info.Type == MEM_PRIVATE ? "private" : info.Type == MEM_MAPPED ? "mapped" : "image?";
            }
            map << hex(regionBase) << '\t' << hex(info.RegionSize) << '\t'
                << hex(reinterpret_cast<std::uintptr_t>(info.AllocationBase)) << '\t'
                << hex(info.AllocationProtect) << '\t' << hex(info.State) << '\t' << hex(info.Protect) << '\t'
                << hex(info.Type) << '\t' << label << '\n';
        }
        cursor = regionBase + info.RegionSize;
    }
    map.close();

    // Ranges to copy: the game's own sections, .securom, and every non-image
    // allocation a slot points into.
    std::vector<std::pair<std::string, std::pair<std::uintptr_t, std::size_t>>> ranges;
    for (const char* name : {".text", ".rdata", ".data", ".securom"}) {
        if (const Section* entry = section(name)) {
            ranges.push_back({entry->name, {entry->start, entry->size}});
        }
    }
    std::ofstream slots(directory / "slots.tsv", std::ios::trunc);
    slots << "slot\tvalue\tregion_base\tregion_size\talloc_base\tprotect\ttype\twhere\n";
    std::map<std::uintptr_t, std::size_t> allocations;
    for (const auto& [slot, cached] : slots_) {
        std::uint32_t value = cached;
        readMemory(slot, &value, sizeof(value));
        MEMORY_BASIC_INFORMATION info{};
        const bool known = query(value, info);
        const auto allocation = reinterpret_cast<std::uintptr_t>(info.AllocationBase);
        slots << hex(slot) << '\t' << hex(value) << '\t'
              << hex(known ? reinterpret_cast<std::uintptr_t>(info.BaseAddress) : 0) << '\t'
              << hex(known ? info.RegionSize : 0) << '\t' << hex(known ? allocation : 0) << '\t'
              << hex(known ? info.Protect : 0) << '\t' << hex(known ? info.Type : 0) << '\t'
              << where(value) << '\n';
        if (!known || info.State != MEM_COMMIT || allocation == imageBase_
            || moduleAt(modules, value) != nullptr || allocations.count(allocation) != 0) {
            continue;
        }
        std::size_t extent = 0;
        for (std::uintptr_t cursor = allocation; extent < privateRegionCap;) {
            MEMORY_BASIC_INFORMATION part{};
            if (!query(cursor, part) || reinterpret_cast<std::uintptr_t>(part.AllocationBase) != allocation) {
                break;
            }
            extent += part.RegionSize;
            cursor = reinterpret_cast<std::uintptr_t>(part.BaseAddress) + part.RegionSize;
        }
        allocations[allocation] = std::min(extent, privateRegionCap);
    }
    slots.close();
    for (const auto& [allocation, extent] : allocations) {
        ranges.push_back({"alloc_" + hex(allocation), {allocation, extent}});
    }

    std::ofstream regions(directory / "regions.tsv", std::ios::trunc);
    regions << "file\trun_start\trun_size\tstate\tprotect\ttype\treadable\tfailed_pages\n";
    std::size_t total = 0;
    for (const auto& [name, range] : ranges) {
        std::string file = name;
        std::replace(file.begin(), file.end(), '.', '_');
        file += ".bin";
        std::vector<std::uint8_t> bytes;
        const auto runs = copyRange(range.first, range.second, bytes);
        if (!writeFile(directory / file, bytes)) {
            writeLine("dump: cannot write " + (directory / file).string());
        }
        total += bytes.size();
        for (const Run& run : runs) {
            regions << file << '\t' << hex(run.start) << '\t' << hex(run.size) << '\t' << hex(run.state)
                    << '\t' << hex(run.protect) << '\t' << hex(run.type) << '\t' << (run.readable ? 1 : 0)
                    << '\t' << run.failedPages << '\n';
        }
    }
    regions.close();

    std::ostringstream line;
    line << "dump: snapshot " << index << " at " << began << " ms: " << ranges.size() << " ranges, " << total
         << " bytes, " << (elapsedMs() - began) << " ms";
    writeLine(line.str());
}

}  // namespace ra3hook
