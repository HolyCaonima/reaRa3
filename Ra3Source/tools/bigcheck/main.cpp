#include <ra3/io/GameBigLayout.h>
#include <ra3/io/RefPack.h>

#include <algorithm>
#include <array>
#include <cstdlib>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <iterator>
#include <sstream>
#include <string>
#include <vector>

namespace {

void* __fastcall testAllocateTagged(
    ra3::io::GameAllocator*,
    void*,
    std::uint32_t size,
    const char*,
    std::uint32_t)
{
    return std::calloc(size, 1);
}

void __fastcall testFreeTagged(
    ra3::io::GameAllocator*,
    void*,
    void* allocation,
    std::uint32_t)
{
    std::free(allocation);
}

std::int64_t pointerOffset(const void* pointer, const void* base)
{
    if (pointer == nullptr) {
        return -1;
    }
    return static_cast<const std::uint8_t*>(pointer)
        - static_cast<const std::uint8_t*>(base);
}

struct MemoryIo {
    const std::vector<std::uint8_t>* source;
    std::uint64_t position = 0;
    ra3::io::GameAllocator* allocator;
};

void memorySeek(void* opaque, std::uint64_t position)
{
    static_cast<MemoryIo*>(opaque)->position = position;
}

std::uint32_t memoryRead(void* opaque, void* destination, std::uint32_t count)
{
    auto* io = static_cast<MemoryIo*>(opaque);
    const std::size_t available = io->position < io->source->size()
        ? io->source->size() - static_cast<std::size_t>(io->position) : 0;
    const std::uint32_t actual = static_cast<std::uint32_t>(
        std::min<std::size_t>(count, available));
    if (actual != 0) {
        std::memcpy(
            destination,
            io->source->data() + static_cast<std::size_t>(io->position),
            actual);
    }
    io->position += actual;
    return actual;
}

ra3::io::GameRefPackInflater* memoryCreateInflater(void* opaque)
{
    auto* io = static_cast<MemoryIo*>(opaque);
    auto* inflater = static_cast<ra3::io::GameRefPackInflater*>(
        std::calloc(1, sizeof(ra3::io::GameRefPackInflater)));
    ra3::io::initializeGameRefPackInflater(inflater, io->allocator, nullptr);
    return inflater;
}

void* memoryAllocateTemporary(void*, std::uint32_t size)
{
    return std::calloc(size, 1);
}

void memoryDestroyInflater(void*, void* pointer)
{
    auto* inflater = static_cast<ra3::io::GameRefPackInflater*>(pointer);
    ra3::io::destroyGameRefPackInflater(inflater);
    std::free(inflater);
}

void memoryRelease(void*, void* pointer)
{
    std::free(pointer);
}

std::uint32_t vectorRangeRead(
    void* context,
    const ra3::io::GameBigArchive*,
    std::uint64_t offset,
    void* destination,
    std::uint32_t count)
{
    const auto* bytes = static_cast<const std::vector<std::uint8_t>*>(context);
    if (offset >= bytes->size()) {
        return 0;
    }
    const auto actual = static_cast<std::uint32_t>(
        std::min<std::uint64_t>(count, bytes->size() - offset));
    std::memcpy(destination, bytes->data() + static_cast<std::size_t>(offset), actual);
    return actual;
}

std::uint32_t readBe32(const std::uint8_t* bytes)
{
    return (static_cast<std::uint32_t>(bytes[0]) << 24)
        | (static_cast<std::uint32_t>(bytes[1]) << 16)
        | (static_cast<std::uint32_t>(bytes[2]) << 8)
        | static_cast<std::uint32_t>(bytes[3]);
}

std::string swapAsciiCase(std::string value)
{
    for (char& c : value) {
        if (c >= 'A' && c <= 'Z') {
            c = static_cast<char>(c + ('a' - 'A'));
        } else if (c >= 'a' && c <= 'z') {
            c = static_cast<char>(c - ('a' - 'A'));
        }
    }
    return value;
}

struct Record {
    const std::uint8_t* bytes;
    const char* name;
    std::uint32_t offset;
    std::uint32_t size;
};

std::vector<std::uint8_t> decodeHex(const std::string& text)
{
    std::vector<std::uint8_t> bytes;
    bytes.reserve(text.size() / 2);
    for (std::size_t i = 0; i + 1 < text.size(); i += 2) {
        bytes.push_back(static_cast<std::uint8_t>(
            std::stoul(text.substr(i, 2), nullptr, 16)));
    }
    return bytes;
}

int helperOracle()
{
    std::string line;
    std::vector<std::uint8_t> mountedBytes;
    std::vector<std::uint8_t> mountedHeader;
    std::string mountedName;
    std::string mountedSecondName;
    ra3::io::GameBigArchive mountedArchive{};
    ra3::io::GameBigArchive mountedSecondArchive{};
    ra3::io::GameBigHandler mountedHandler{};
    while (std::getline(std::cin, line)) {
        std::istringstream input(line);
        std::string command;
        input >> command;
        if (command == "quit") {
            return 0;
        }
        std::cout << std::hex;
        if (command == "mount" || command == "mountpair") {
            std::string pathText;
            input >> pathText;
            const auto pathBytes = decodeHex(pathText);
            const std::filesystem::path path(std::string(
                reinterpret_cast<const char*>(pathBytes.data()), pathBytes.size()));
            std::ifstream archiveInput(path, std::ios::binary);
            mountedBytes.assign(
                std::istreambuf_iterator<char>(archiveInput),
                std::istreambuf_iterator<char>());
            if (mountedBytes.size() < 16) {
                std::cout << "error\n" << std::flush;
                continue;
            }
            const std::uint32_t headerSize =
                readBe32(mountedBytes.data() + 12);
            mountedHeader.assign(
                mountedBytes.begin(), mountedBytes.begin() + headerSize);
            mountedName = command == "mountpair"
                ? "First.big" : path.filename().string();
            mountedSecondName = "Second.big";
            mountedArchive = {};
            mountedArchive.index = {
                mountedHeader.data(), mountedHeader.data(), nullptr};
            mountedArchive.archiveName = mountedName.c_str();
            mountedArchive.file =
                reinterpret_cast<ra3::io::GameArchiveFile*>(1);
            mountedSecondArchive = {};
            if (command == "mountpair") {
                mountedSecondArchive.index = {
                    mountedHeader.data(), mountedHeader.data(), nullptr};
                mountedSecondArchive.archiveName = mountedSecondName.c_str();
                mountedSecondArchive.file =
                    reinterpret_cast<ra3::io::GameArchiveFile*>(2);
                mountedArchive.next = &mountedSecondArchive;
            }
            mountedHandler = {};
            mountedHandler.head = &mountedArchive;
            mountedHandler.tail = command == "mountpair"
                ? &mountedSecondArchive : &mountedArchive;
            mountedHandler.archiveCount = command == "mountpair" ? 2 : 1;
            std::cout << "ok";
        } else if (command == "open") {
            std::string pathText;
            input >> pathText;
            const auto pathBytes = decodeHex(pathText);
            const std::string path(
                reinterpret_cast<const char*>(pathBytes.data()), pathBytes.size());
            ra3::io::GameBigFile file{};
            const ra3::io::GameBigOpenResult result = ra3::io::prepareGameBigFile(
                &mountedHandler, path.c_str(), 0x12345678u, 0x10000u,
                &vectorRangeRead, &mountedBytes, &file);
            if (result != ra3::io::GameBigOpenResult::found) {
                std::cout << '0';
            } else {
                std::cout << "1 " << file.ownerToken << ' ' << file.openMode
                          << ' ' << file.dataOffset << ' ' << file.logicalSize
                          << ' ' << file.compressedSize << ' '
                          << file.sourcePosition << ' ' << file.logicalPosition
                          << ' ' << (file.archive == &mountedArchive ? 1 : 2);
            }
        } else if (command == "be32" || command == "be64" || command == "format"
            || command == "headersize" || command == "entrycount"
            || command == "trailer") {
            std::string encoded;
            input >> encoded;
            const auto bytes = decodeHex(encoded);
            if (command == "be32" || command == "be64") {
                int width = 0;
                input >> std::dec >> width;
                if (command == "be32") {
                    std::cout << ra3::io::readGameBigEndian32(bytes.data(), width);
                } else {
                    std::cout << ra3::io::readGameBigEndian64(bytes.data(), width);
                }
            } else if (command == "format") {
                std::cout << static_cast<std::uint32_t>(
                    ra3::io::detectGameBigFormat(bytes.data()));
            } else if (command == "headersize") {
                std::cout << ra3::io::gameBigHeaderSize(bytes.data());
            } else {
                const ra3::io::GameBigIndex index{
                    bytes.data(), bytes.data(), nullptr};
                if (command == "entrycount") {
                    std::cout << ra3::io::gameBigEntryCount(&index);
                } else {
                    int number = 0;
                    const int size = ra3::io::gameBigTrailerSize(&index, &number);
                    std::cout << size << ' ' << number;
                }
            }
        } else if (command == "hash") {
            std::string encoded;
            std::uint32_t seed = 0;
            input >> encoded >> std::hex >> seed;
            const auto bytes = decodeHex(encoded);
            std::cout << ra3::io::hashGameBigPath(
                reinterpret_cast<const char*>(bytes.data()), seed);
        } else if (command == "stored") {
            std::uint64_t size = 0;
            input >> std::hex >> size;
            ra3::io::GameBigFile file{};
            file.storedSize = size;
            std::cout << ra3::io::gameBigStoredSize(&file);
        } else if (command == "absolute") {
            std::uint64_t range = 0;
            std::uint64_t offset = 0;
            input >> std::hex >> range >> offset;
            ra3::io::GameBigArchive archive{};
            archive.rangeBegin = range;
            ra3::io::GameBigFile file{};
            file.archive = &archive;
            file.dataOffset = offset;
            std::cout << ra3::io::gameBigAbsoluteDataOffset(&file);
        } else if (command == "seek") {
            std::uint64_t dataOffset = 0;
            std::uint64_t expandedSize = 0;
            std::uint64_t sourcePosition = 0;
            std::uint64_t logicalPosition = 0;
            std::uint64_t position = 0;
            input >> std::hex >> dataOffset >> expandedSize >> sourcePosition
                  >> logicalPosition >> position;
            ra3::io::GameBigFile file{};
            file.dataOffset = dataOffset;
            file.expandedSize = expandedSize;
            file.sourcePosition = sourcePosition;
            file.logicalPosition = logicalPosition;
            const bool reset = ra3::io::applyGameBigSeekState(&file, position);
            std::cout << file.sourcePosition << ' ' << file.logicalPosition << ' '
                      << static_cast<unsigned>(reset);
        } else if (command == "inflate") {
            std::string encoded;
            std::uint32_t inputChunk = 0;
            std::uint32_t outputChunk = 0;
            input >> encoded >> std::dec >> inputChunk >> outputChunk;
            const auto packed = decodeHex(encoded);
            const auto header =
                ra3::io::readRefPackHeader(packed.data(), packed.size());
            if (!header || inputChunk == 0 || outputChunk == 0) {
                std::cout << "error\n" << std::flush;
                continue;
            }
            ra3::io::GameAllocatorVtable vtable{};
            vtable.allocateTagged = reinterpret_cast<void*>(&testAllocateTagged);
            vtable.freeTagged = reinterpret_cast<void*>(&testFreeTagged);
            ra3::io::GameAllocator allocator{&vtable};
            ra3::io::GameRefPackInflater inflater{};
            ra3::io::initializeGameRefPackInflater(
                &inflater, &allocator, nullptr);
            std::vector<std::uint8_t> output(header->decompressedSize);
            std::size_t source = 0;
            std::size_t destination = 0;
            std::size_t iterations = 0;
            bool firstInput = true;
            std::ostringstream snapshots;
            while (inflater.status == 0 && iterations++ < 100000) {
                if (inflater.inputRemaining == 0 && source < packed.size()) {
                    const auto count = static_cast<std::uint32_t>(
                        std::min<std::size_t>(
                            firstInput ? 5u : inputChunk,
                            packed.size() - source));
                    ra3::io::setGameRefPackInput(
                        &inflater, packed.data() + source, count);
                    source += count;
                    firstInput = false;
                }
                if (inflater.outputRemaining == 0 && destination < output.size()) {
                    const auto count = static_cast<std::uint32_t>(
                        std::min<std::size_t>(
                            outputChunk, output.size() - destination));
                    ra3::io::setGameRefPackOutput(
                        &inflater, output.data() + destination, count);
                    destination += count;
                }
                ra3::io::runGameRefPackInflater(&inflater);
                std::uint32_t windowHash = 0;
                if (inflater.windowBegin != nullptr) {
                    for (std::uint32_t i = 0; i < inflater.windowSize; ++i) {
                        windowHash += inflater.windowBegin[i];
                    }
                }
                if (snapshots.tellp() != std::streampos(0)) {
                    snapshots << ';';
                }
                snapshots << std::hex
                          << inflater.inputRemaining << ','
                          << inflater.outputRemaining << ','
                          << inflater.outputProduced << ','
                          << inflater.state << ','
                          << inflater.nextState << ','
                          << inflater.pendingInputBytes << ','
                          << inflater.decodedLength << ','
                          << inflater.runRemaining << ','
                          << inflater.command << ','
                          << pointerOffset(inflater.copyCursor, inflater.windowBegin) << ','
                          << pointerOffset(inflater.windowCursor, inflater.windowBegin) << ','
                          << inflater.windowProduced << ','
                          << inflater.windowSize << ','
                          << inflater.status << ','
                          << windowHash;
            }
            if (inflater.status != 1) {
                ra3::io::destroyGameRefPackInflater(&inflater);
                std::cout << "error\n" << std::flush;
                continue;
            }
            for (const std::uint8_t value : output) {
                std::cout << std::hex << std::setw(2) << std::setfill('0')
                          << static_cast<unsigned>(value);
            }
            std::cout << ' ' << std::hex
                      << inflater.inputRemaining << ' '
                      << inflater.outputRemaining << ' '
                      << inflater.outputProduced << ' '
                      << inflater.state << ' '
                      << inflater.nextState << ' '
                      << inflater.pendingInputBytes << ' '
                      << inflater.decodedLength << ' '
                      << inflater.runRemaining << ' '
                      << inflater.command << ' '
                      << pointerOffset(inflater.copyCursor, inflater.windowBegin) << ' '
                      << pointerOffset(inflater.windowCursor, inflater.windowBegin) << ' '
                      << inflater.windowProduced << ' '
                      << inflater.windowSize << ' '
                      << inflater.status << ' '
                      << snapshots.str();
            ra3::io::destroyGameRefPackInflater(&inflater);
        } else if (command == "readseq") {
            std::string encoded;
            std::uint32_t openMode = 0;
            std::string sequence;
            input >> encoded >> std::hex >> openMode >> sequence;
            const auto packed = decodeHex(encoded);
            const auto header =
                ra3::io::readRefPackHeader(packed.data(), packed.size());
            ra3::io::GameAllocatorVtable vtable{};
            vtable.allocateTagged = reinterpret_cast<void*>(&testAllocateTagged);
            vtable.freeTagged = reinterpret_cast<void*>(&testFreeTagged);
            ra3::io::GameAllocator allocator{&vtable};
            ra3::io::GameBigArchive archive{};
            auto* file = static_cast<ra3::io::GameBigFile*>(
                std::calloc(1, sizeof(ra3::io::GameBigFile)));
            file->archive = &archive;
            file->openMode = openMode;
            file->logicalSize =
                header ? header->decompressedSize : packed.size();
            file->compressedSize = header ? packed.size() : 0;
            MemoryIo memory{&packed, 0, &allocator};
            const ra3::io::GameBigIoOps io{
                &memory, nullptr, nullptr, &memorySeek, &memoryRead,
                &memoryCreateInflater, &memoryAllocateTemporary};
            const ra3::io::GameBigReleaseOps release{
                nullptr, &memoryDestroyInflater, &memoryRelease};
            std::istringstream operations(sequence);
            std::string operation;
            bool first = true;
            while (std::getline(operations, operation, ';')) {
                const std::size_t comma = operation.find(',');
                const std::uint64_t position =
                    std::stoull(operation.substr(0, comma), nullptr, 16);
                const std::uint32_t count = static_cast<std::uint32_t>(
                    std::stoul(operation.substr(comma + 1), nullptr, 16));
                ra3::io::seekGameBigFile(file, position, release);
                std::vector<std::uint8_t> bytes(count, 0xA5);
                const std::uint32_t actual =
                    ra3::io::readGameBigFile(file, bytes.data(), count, io);
                if (!first) {
                    std::cout << ';';
                }
                first = false;
                std::cout << std::hex << actual << ':';
                for (std::uint32_t i = 0; i < actual; ++i) {
                    std::cout << std::setw(2) << std::setfill('0')
                              << static_cast<unsigned>(bytes[i]);
                }
                std::cout << ':' << (file->archive != nullptr) << ','
                          << file->ownerToken << ','
                          << file->openMode << ','
                          << file->unknown14 << ','
                          << file->dataOffset << ','
                          << file->logicalSize << ','
                          << file->compressedSize << ','
                          << file->sourcePosition << ','
                          << file->logicalPosition << ','
                          << (file->inflater != nullptr) << ','
                          << (file->temporaryBuffer != nullptr);
                if (file->inflater != nullptr) {
                    const auto* inflater =
                        static_cast<const ra3::io::GameRefPackInflater*>(
                            file->inflater);
                    std::uint32_t windowHash = 0;
                    for (std::uint32_t i = 0; i < inflater->windowSize; ++i) {
                        windowHash += inflater->windowBegin[i];
                    }
                    std::cout << ',' << inflater->unknown04 << ','
                              << (inflater->allocator != nullptr) << ','
                              << static_cast<unsigned>(inflater->initialized) << ','
                              << inflater->unknown10 << ','
                              << inflater->inputRemaining << ','
                              << inflater->outputRemaining << ','
                              << inflater->unknown1C << ','
                              << inflater->outputProduced << ','
                              << inflater->state << ','
                              << inflater->nextState << ','
                              << inflater->pendingInputBytes << ','
                              << inflater->decodedLength << ','
                              << inflater->runRemaining << ','
                              << inflater->command << ','
                              << pointerOffset(
                                     inflater->copyCursor,
                                     inflater->windowBegin) << ','
                              << pointerOffset(
                                     inflater->windowCursor,
                                     inflater->windowBegin) << ','
                              << inflater->windowProduced << ','
                              << inflater->windowSize << ','
                              << inflater->status << ','
                              << windowHash;
                }
            }
            ra3::io::closeGameBigFile(file, release);
        } else if (command == "enumerate") {
            std::string encoded;
            std::string pattern;
            input >> encoded >> pattern;
            const auto header = decodeHex(encoded);
            std::vector<ra3::io::GameBigSearchEntry> noSearch;
            ra3::io::GameBigArchive archive{};
            archive.index = {header.data(), header.data(), noSearch.data()};
            archive.index.searchTable = nullptr;
            ra3::io::GameBigFindState state{};
            ra3::io::initializeGameBigFindState(
                &state, pattern.c_str(), &archive);
            std::array<std::uint8_t, 0x200> resultBytes{};
            auto* result = reinterpret_cast<ra3::io::GameBigFindResult*>(
                resultBytes.data());
            bool first = true;
            while (ra3::io::findNextGameBig(&state, result)) {
                if (!first) {
                    std::cout << ';';
                }
                first = false;
                std::cout << result->name << ':' << std::hex << result->size
                          << ':' << state.ordinal << ','
                          << state.recordOrdinal << ','
                          << (state.recordCursor - header.data());
            }
        } else {
            std::cout << "error";
        }
        std::cout << '\n' << std::flush;
    }
    return 0;
}

}  // namespace

int main(int argc, char** argv)
{
    if (argc == 2 && std::strcmp(argv[1], "--stdio") == 0) {
        return helperOracle();
    }
    if (argc != 2) {
        std::cerr << "usage: bigcheck <BIG4 archive>\n";
        return 2;
    }
    const std::filesystem::path path(argv[1]);
    std::ifstream input(path, std::ios::binary);
    if (!input) {
        std::cerr << "cannot open " << path.string() << '\n';
        return 2;
    }
    std::vector<std::uint8_t> fixed(16);
    input.read(reinterpret_cast<char*>(fixed.data()), static_cast<std::streamsize>(fixed.size()));
    if (!input || std::memcmp(fixed.data(), "BIG4", 4) != 0) {
        std::cerr << "expected a BIG4 archive\n";
        return 2;
    }
    const std::uint32_t count = readBe32(fixed.data() + 8);
    const std::uint32_t headerSize = readBe32(fixed.data() + 12);
    std::vector<std::uint8_t> header(headerSize);
    input.seekg(0);
    input.read(reinterpret_cast<char*>(header.data()), static_cast<std::streamsize>(header.size()));
    if (!input) {
        std::cerr << "cannot read the complete header\n";
        return 2;
    }

    std::size_t end = header.size();
    if (end >= 8) {
        const std::uint8_t* trailer = header.data() + end - 8;
        const bool letter = (trailer[0] >= 'A' && trailer[0] <= 'Z')
            || (trailer[0] >= 'a' && trailer[0] <= 'z');
        const bool digits = trailer[1] >= '0' && trailer[1] <= '9'
            && trailer[2] >= '0' && trailer[2] <= '9'
            && trailer[3] >= '0' && trailer[3] <= '9';
        if (letter && digits) {
            end -= 8;
        }
    }

    std::vector<Record> records;
    records.reserve(count);
    std::size_t position = 16;
    while (position < end && records.size() < count) {
        const std::uint8_t* record = header.data() + position;
        const char* name = reinterpret_cast<const char*>(record + 8);
        const std::size_t remaining = end - position - 8;
        const void* terminator = std::memchr(name, 0, remaining);
        if (terminator == nullptr) {
            std::cerr << "unterminated entry path\n";
            return 2;
        }
        records.push_back({record, name, readBe32(record), readBe32(record + 4)});
        position += 8
            + static_cast<std::size_t>(static_cast<const char*>(terminator) - name) + 1;
    }
    if (records.size() != count) {
        std::cerr << "parsed " << records.size() << " of " << count << " entries\n";
        return 2;
    }

    std::vector<ra3::io::GameBigSearchEntry> search;
    search.reserve(records.size());
    for (const Record& record : records) {
        search.push_back({ra3::io::hashGameBigPath(record.name), record.bytes});
    }
    std::sort(search.begin(), search.end(), [](const auto& left, const auto& right) {
        return left.hash < right.hash;
    });
    const ra3::io::GameBigIndex index{header.data(), header.data(), search.data()};

    std::size_t cases = 0;
    std::size_t mismatches = 0;
    for (std::size_t i = 0; i < records.size(); ++i) {
        const Record& expected = records[i];
        for (const std::string& query : {std::string(expected.name), swapAsciiCase(expected.name)}) {
            ++cases;
            if (ra3::io::findGameBigHashedEntry(&index, query.c_str(), 4, 4) != expected.bytes) {
                ++mismatches;
            }
            std::uint64_t offset = 0;
            std::uint64_t size = 0;
            ++cases;
            const char* found = ra3::io::findGameBigEntry(
                &index, query.c_str(), 0, &offset, &size, nullptr, nullptr);
            if (found != expected.name || offset != expected.offset || size != expected.size) {
                ++mismatches;
            }
        }

        std::uint64_t offset = 0;
        std::uint64_t size = 0;
        const std::uint8_t* cursor = nullptr;
        int ordinal = -1;
        ++cases;
        const char* found = ra3::io::findGameBigEntry(
            &index,
            nullptr,
            static_cast<int>(i),
            &offset,
            &size,
            &cursor,
            &ordinal);
        if (found != expected.name || offset != expected.offset || size != expected.size
            || cursor != expected.bytes || ordinal != static_cast<int>(i)) {
            ++mismatches;
        }
    }

    constexpr const char* missing = "__ra3source_missing__.bin";
    ++cases;
    mismatches += ra3::io::findGameBigHashedEntry(&index, missing, 4, 4) != nullptr ? 1u : 0u;
    ++cases;
    mismatches +=
        ra3::io::findGameBigEntry(&index, missing, 0, nullptr, nullptr, nullptr, nullptr)
            != nullptr
        ? 1u
        : 0u;

    std::cout << path.filename().string() << ": entries=" << records.size()
              << " cases=" << cases << " mismatches=" << mismatches << '\n';
    return mismatches == 0 ? 0 : 1;
}
