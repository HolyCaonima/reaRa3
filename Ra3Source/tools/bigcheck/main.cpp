#include <ra3/io/GameBigLayout.h>

#include <algorithm>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

namespace {

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
    while (std::getline(std::cin, line)) {
        std::istringstream input(line);
        std::string command;
        input >> command;
        if (command == "quit") {
            return 0;
        }
        std::cout << std::hex;
        if (command == "be32" || command == "be64" || command == "format"
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
