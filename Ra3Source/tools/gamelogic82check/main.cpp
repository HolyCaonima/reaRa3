#include <ra3/gamelogic/ScriptNamedQueries.h>

#include <cstdint>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

using ra3::gamelogic::ScriptQueryCpu;
using ra3::gamelogic::ScriptQueryStub;
using ra3::gamelogic::runScriptNamedQuery;

constexpr std::uint32_t kStackTop = 0x7F0FFF00u;
constexpr std::uint32_t kReturnSentinel = 0x7FE00FF0u;

std::uint32_t parseHex(const std::string& text) {
    if (text.empty()) {
        throw std::runtime_error("empty hex");
    }
    return static_cast<std::uint32_t>(std::stoul(text, nullptr, 16));
}

void writeBytes(ScriptQueryCpu& cpu, std::uint32_t addr, const std::string& hex) {
    if ((hex.size() & 1u) != 0u) {
        throw std::runtime_error("odd hex length");
    }
    auto nibble = [](char c) -> unsigned {
        if (c >= '0' && c <= '9') return static_cast<unsigned>(c - '0');
        if (c >= 'a' && c <= 'f') return static_cast<unsigned>(c - 'a' + 10);
        if (c >= 'A' && c <= 'F') return static_cast<unsigned>(c - 'A' + 10);
        throw std::runtime_error("invalid hex");
    };
    for (std::size_t i = 0; i < hex.size(); i += 2) {
        const auto value = static_cast<std::uint8_t>((nibble(hex[i]) << 4u) | nibble(hex[i + 1]));
        cpu.w8(addr + static_cast<std::uint32_t>(i / 2), value);
    }
}

std::string hexBytes(const ScriptQueryCpu& cpu, std::uint32_t addr, std::uint32_t len) {
    std::ostringstream out;
    out << std::hex;
    for (std::uint32_t i = 0; i < len; ++i) {
        const unsigned value = cpu.r8(addr + i);
        if (value < 16) {
            out << '0';
        }
        out << value;
    }
    return out.str();
}

std::string hex8(std::uint32_t value) {
    const char* digits = "0123456789abcdef";
    std::string text(8, '0');
    for (int i = 7; i >= 0; --i) {
        text[static_cast<std::size_t>(i)] = digits[value & 0xF];
        value >>= 4;
    }
    return text;
}

struct Watch {
    std::uint32_t addr = 0;
    std::uint32_t len = 0;
};

struct Case {
    std::uint32_t va = 0;
    std::uint32_t ecx = 0;
    std::vector<std::uint32_t> args;
    std::vector<std::pair<std::uint32_t, std::string>> mem;
    std::vector<ScriptQueryStub> stubs;
    std::vector<Watch> watches;
};

Case parseCase(std::istream& in) {
    Case item;
    std::string line;
    while (std::getline(in, line)) {
        if (line.empty() || line[0] == '#') {
            continue;
        }
        std::istringstream iss(line);
        std::string op;
        iss >> op;
        if (op == "end") {
            return item;
        }
        if (op == "va") {
            std::string text;
            iss >> text;
            item.va = parseHex(text);
        } else if (op == "ecx") {
            std::string text;
            iss >> text;
            item.ecx = parseHex(text);
        } else if (op == "arg") {
            std::string text;
            iss >> text;
            item.args.push_back(parseHex(text));
        } else if (op == "mem") {
            std::string addr;
            std::string bytes;
            iss >> addr >> bytes;
            item.mem.emplace_back(parseHex(addr), bytes);
        } else if (op == "stub") {
            std::string target;
            std::string eax;
            std::string ecx;
            std::string edx;
            iss >> target >> eax >> ecx >> edx;
            ScriptQueryStub stub;
            stub.target = parseHex(target);
            stub.eax = parseHex(eax);
            stub.ecx = parseHex(ecx);
            stub.edx = parseHex(edx);
            std::string poke;
            if (iss >> poke) {
                std::string value;
                iss >> value;
                stub.pokeArg = static_cast<int>(parseHex(poke));
                stub.pokeValue = static_cast<std::uint8_t>(parseHex(value));
            }
            item.stubs.push_back(stub);
        } else if (op == "watch") {
            std::string addr;
            std::string len;
            iss >> addr >> len;
            item.watches.push_back(Watch{parseHex(addr), parseHex(len)});
        } else {
            throw std::runtime_error("unknown op " + op);
        }
    }
    throw std::runtime_error("truncated case");
}

void runCase(const Case& item) {
    ScriptQueryCpu cpu;
    for (const auto& blob : item.mem) {
        writeBytes(cpu, blob.first, blob.second);
    }
    cpu.ecx = item.ecx;
    cpu.esp = kStackTop - 4u * static_cast<std::uint32_t>(1 + item.args.size());
    cpu.w32(cpu.esp, kReturnSentinel);
    for (std::size_t i = 0; i < item.args.size(); ++i) {
        cpu.w32(cpu.esp + 4u + 4u * static_cast<std::uint32_t>(i), item.args[i]);
    }
    cpu.stubs = item.stubs;
    const std::uint32_t eax = runScriptNamedQuery(cpu, item.va);
    std::cout << "eax " << hex8(eax) << '\n';
    std::cout << "trace";
    for (const auto& event : cpu.trace) {
        std::cout << ' ' << hex8(event.target) << ':' << hex8(event.ecx) << ':' << hex8(event.eaxIn);
        if (event.args.empty()) {
            std::cout << ":-";
        } else {
            std::cout << ':';
            for (std::size_t i = 0; i < event.args.size(); ++i) {
                if (i != 0) {
                    std::cout << ',';
                }
                std::cout << hex8(event.args[i]);
            }
        }
    }
    std::cout << '\n';
    for (const Watch& watch : item.watches) {
        std::cout << "watch " << hex8(watch.addr) << ' ' << hexBytes(cpu, watch.addr, watch.len) << '\n';
    }
    std::cout << "end\n";
}

}  // namespace

int main() {
    std::ios::sync_with_stdio(false);
    std::string line;
    while (std::getline(std::cin, line)) {
        if (line.empty() || line[0] == '#') {
            continue;
        }
        if (line != "case") {
            std::cout << "error expected case\nend\n";
            return 1;
        }
        try {
            runCase(parseCase(std::cin));
        } catch (const std::exception& error) {
            std::cout << "error " << error.what() << "\nend\n";
        }
    }
    return 0;
}
