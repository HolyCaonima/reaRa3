#include <ra3/common/Thing78.h>
#include <ra3/gamelogic/ObjectUpdates78.h>

#include <cstdint>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

namespace {

std::uint32_t parseHex(const std::string& text) {
    return static_cast<std::uint32_t>(std::stoul(text, nullptr, 16));
}

std::vector<std::uint8_t> parseBlob(const std::string& text) {
    std::vector<std::uint8_t> bytes;
    if (text == "-") {
        return bytes;
    }
    bytes.resize(text.size() / 2u);
    for (std::size_t i = 0; i < bytes.size(); ++i) {
        bytes[i] = static_cast<std::uint8_t>(std::stoul(text.substr(i * 2u, 2u), nullptr, 16));
    }
    return bytes;
}

std::string hexByte(std::uint8_t value) {
    std::ostringstream out;
    out << std::hex << std::setfill('0') << std::setw(2) << static_cast<unsigned>(value);
    return out.str();
}

std::string hex32(std::uint32_t value) {
    std::ostringstream out;
    out << std::hex << std::setfill('0') << std::setw(8) << value;
    return out.str();
}

struct Global {
    std::uint32_t va = 0;
    std::uint32_t value = 0;
};

std::vector<Global> parseGlobals(const std::string& text) {
    std::vector<Global> globals;
    if (text == "-") {
        return globals;
    }
    std::size_t cursor = 0;
    while (cursor < text.size()) {
        const std::size_t comma = text.find(',', cursor);
        const std::string item = text.substr(cursor, comma == std::string::npos ? std::string::npos : comma - cursor);
        const std::size_t eq = item.find('=');
        globals.push_back(Global{parseHex(item.substr(0, eq)), parseHex(item.substr(eq + 1))});
        if (comma == std::string::npos) {
            break;
        }
        cursor = comma + 1;
    }
    return globals;
}

struct TapeWord {
    std::uint32_t eax = 0;
    std::uint32_t st0 = 0;
};

std::vector<TapeWord> parseTape(const std::string& text) {
    std::vector<TapeWord> tape;
    if (text == "-") {
        return tape;
    }
    std::size_t cursor = 0;
    while (cursor < text.size()) {
        const std::size_t semi = text.find(';', cursor);
        const std::string item = text.substr(cursor, semi == std::string::npos ? std::string::npos : semi - cursor);
        const std::size_t colon = item.find(':');
        TapeWord word;
        word.eax = parseHex(item.substr(0, colon));
        std::string bits = item.substr(colon + 1);
        if (!bits.empty() && (bits[0] == 'f' || bits[0] == 'F')) {
            bits.erase(bits.begin());
        }
        word.st0 = parseHex(bits);
        tape.push_back(word);
        if (semi == std::string::npos) {
            break;
        }
        cursor = semi + 1;
    }
    return tape;
}

std::vector<std::uint32_t> parseArgs(const std::string& text) {
    std::vector<std::uint32_t> args;
    if (text == "-") {
        return args;
    }
    std::size_t cursor = 0;
    while (cursor < text.size()) {
        const std::size_t comma = text.find(',', cursor);
        args.push_back(parseHex(text.substr(cursor, comma == std::string::npos ? std::string::npos : comma - cursor)));
        if (comma == std::string::npos) {
            break;
        }
        cursor = comma + 1;
    }
    return args;
}

std::string emit(
    const ra3::common::RunResult& result, const std::vector<std::uint8_t>& blob,
    const ra3::common::Tape& tape) {
    std::ostringstream line;
    line << "eax=" << hex32(result.eax) << " st0=" << hex32(result.st0) << " blob=";
    if (blob.empty()) {
        line << '-';
    } else {
        for (std::uint8_t byte : blob) {
            line << hexByte(byte);
        }
    }
    line << " glob=";
    if (tape.gCount == 0) {
        line << '-';
    } else {
        for (int i = 0; i < tape.gCount; ++i) {
            if (i != 0) {
                line << ',';
            }
            line << hex32(tape.gVa[i]) << '=' << hex32(tape.gVal[i]);
        }
    }
    line << " trace=";
    if (tape.traceCount == 0) {
        line << '-';
    } else {
        for (std::uint32_t i = 0; i < tape.traceCount; ++i) {
            if (i != 0) {
                line << ',';
            }
            line << hex32(tape.trace[i]);
        }
    }
    return line.str();
}

} // namespace

int main() {
    std::string line;
    while (std::getline(std::cin, line)) {
        if (line.empty()) {
            continue;
        }
        std::istringstream input(line);
        std::string command;
        input >> command;
        if (command == "quit") {
            break;
        }
        if (command != "call") {
            std::cout << "error bad-command\n";
            continue;
        }
        std::string vaText;
        std::string ecxText;
        std::string blobText;
        std::string argText;
        std::string globalText;
        std::string tapeText;
        input >> vaText >> ecxText >> blobText >> argText >> globalText >> tapeText;
        try {
            const std::uint32_t va = parseHex(vaText);
            const std::uint32_t ecx = parseHex(ecxText);
            std::vector<std::uint8_t> blob = parseBlob(blobText);
            const std::vector<std::uint32_t> args = parseArgs(argText);
            const std::vector<Global> globals = parseGlobals(globalText);
            const std::vector<TapeWord> words = parseTape(tapeText);
            std::vector<std::uint32_t> eaxTape(words.size());
            std::vector<std::uint32_t> stTape(words.size());
            for (std::size_t i = 0; i < words.size(); ++i) {
                eaxTape[i] = words[i].eax;
                stTape[i] = words[i].st0;
            }
            ra3::common::Tape tape;
            tape.eax = eaxTape.empty() ? nullptr : eaxTape.data();
            tape.st0 = stTape.empty() ? nullptr : stTape.data();
            tape.count = static_cast<std::uint32_t>(words.size());
            tape.gCount = static_cast<int>(globals.size());
            for (int i = 0; i < tape.gCount; ++i) {
                tape.gVa[i] = globals[static_cast<std::size_t>(i)].va;
                tape.gVal[i] = globals[static_cast<std::size_t>(i)].value;
            }
            ra3::common::Mem mem;
            mem.bytes = blob.empty() ? nullptr : blob.data();
            mem.base = ra3::common::kArenaBase;
            mem.size = static_cast<std::uint32_t>(blob.size());
            mem.tape = &tape;
            ra3::common::RunResult result;
            const bool ran = ra3::common::runThing78(
                                 va, mem, ecx, args.empty() ? nullptr : args.data(),
                                 static_cast<int>(args.size()), result) ||
                             ra3::gamelogic::runObjectUpdates78(
                                 va, mem, ecx, args.empty() ? nullptr : args.data(),
                                 static_cast<int>(args.size()), result);
            if (!ran) {
                std::cout << "miss\n";
                continue;
            }
            std::cout << emit(result, blob, tape) << '\n';
        } catch (const std::exception& error) {
            std::cout << "error " << error.what() << '\n';
        }
    }
    return 0;
}
