#include <ra3/gamelogic/AIState7D.h>

#include <cstdint>
#include <cstdio>
#include <cstring>
#include <iostream>
#include <new>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

constexpr std::size_t kAlign = 256;

std::uint32_t addressOf(const void* pointer)
{
    return static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(pointer));
}

std::string hex8(std::uint32_t value)
{
    char text[9];
    std::snprintf(text, sizeof(text), "%08x", value);
    return text;
}

struct Block {
    void* raw = nullptr;
    std::uint8_t* data = nullptr;
    std::uint32_t size = 0;
};

Block allocate(std::uint32_t size)
{
    Block block;
    block.raw = ::operator new(size + kAlign);
    const auto raw = reinterpret_cast<std::uintptr_t>(block.raw);
    block.data = reinterpret_cast<std::uint8_t*>((raw + (kAlign - 1)) & ~(kAlign - 1));
    block.size = size;
    std::memset(block.data, 0, size);
    return block;
}

struct Write {
    int arg = 0;
    std::uint32_t offset = 0;
    std::uint32_t value = 0;
    int size = 4;
};

struct Ret {
    std::uint32_t eax = 0;
    std::uint32_t st0 = 0;
    bool hasSt0 = false;
    std::vector<Write> writes;
};

struct Run {
    std::vector<Block> blocks;
    ra3::gamelogic::AIState7DEnv env{};
    std::vector<Ret> rets;
    std::size_t retIndex = 0;
    std::string log;

    Run() = default;
    Run(const Run&) = delete;
    Run& operator=(const Run&) = delete;
    ~Run()
    {
        for (const auto& block : blocks) {
            ::operator delete(block.raw);
        }
    }
};

std::string normalize(const Run& run, std::uint32_t value)
{
    for (std::size_t index = 0; index < run.blocks.size(); ++index) {
        const auto base = addressOf(run.blocks[index].data);
        if (value >= base && value < base + run.blocks[index].size) {
            char text[32];
            std::snprintf(text, sizeof(text), "@%u+%x", static_cast<unsigned>(index), value - base);
            return text;
        }
    }
    const auto scratch = addressOf(run.env.scratch);
    if (value >= scratch && value < scratch + sizeof(run.env.scratch)) {
        return "ptr";
    }
    return hex8(value);
}

std::uint32_t onCall(void* ctx, std::uint32_t target, std::uint32_t ecx, const std::uint32_t* args, int argc)
{
    auto* run = static_cast<Run*>(ctx);
    if (run->retIndex >= run->rets.size()) {
        throw std::runtime_error("stub queue empty");
    }
    const Ret& ret = run->rets[run->retIndex++];
    if (!run->log.empty()) {
        run->log += ';';
    }
    run->log += hex8(target);
    run->log += ':';
    run->log += normalize(*run, ecx);
    for (int index = 0; index < argc; ++index) {
        run->log += index == 0 ? ':' : '+';
        run->log += normalize(*run, args[index]);
    }
    if (argc == 0) {
        run->log += ':';
    }
    for (const Write& write : ret.writes) {
        if (write.arg < 0 || write.arg >= argc) {
            throw std::runtime_error("writeback arg");
        }
        auto* dest = reinterpret_cast<std::uint8_t*>(static_cast<std::uintptr_t>(args[write.arg])) + write.offset;
        std::memcpy(dest, &write.value, static_cast<std::size_t>(write.size));
    }
    run->env.lastSt0 = ret.hasSt0 ? ret.st0 : 0u;
    return ret.eax;
}

std::vector<std::string> tokens(const std::string& line)
{
    std::istringstream input(line);
    std::vector<std::string> out;
    std::string token;
    while (input >> token) {
        out.push_back(token);
    }
    return out;
}

std::uint32_t resolve(const std::string& token, const Run& run)
{
    if (!token.empty() && token[0] == '@') {
        const auto plus = token.find('+');
        const int index = std::stoi(plus == std::string::npos ? token.substr(1) : token.substr(1, plus - 1));
        if (index < 0 || static_cast<std::size_t>(index) >= run.blocks.size()) {
            throw std::runtime_error("object index");
        }
        std::uint32_t value = addressOf(run.blocks[static_cast<std::size_t>(index)].data);
        if (plus != std::string::npos) {
            value += static_cast<std::uint32_t>(std::stoul(token.substr(plus + 1), nullptr, 16));
        }
        return value;
    }
    return static_cast<std::uint32_t>(std::stoul(token, nullptr, 16));
}

Ret parseRet(const std::string& token, const Run& run)
{
    Ret ret;
    std::string body = token;
    const auto star = body.find('*');
    std::string writes;
    if (star != std::string::npos) {
        writes = body.substr(star + 1);
        body = body.substr(0, star);
    }
    const auto tilde = body.find('~');
    if (tilde != std::string::npos) {
        ret.hasSt0 = true;
        ret.st0 = static_cast<std::uint32_t>(std::stoul(body.substr(tilde + 1), nullptr, 16));
        body = body.substr(0, tilde);
    }
    ret.eax = resolve(body, run);
    std::size_t cursor = 0;
    while (cursor < writes.size()) {
        const auto comma = writes.find(',', cursor);
        const std::string piece = writes.substr(cursor, comma == std::string::npos ? std::string::npos : comma - cursor);
        const auto colon = piece.find(':');
        const auto plus = piece.find('+');
        if (colon == std::string::npos || plus == std::string::npos) {
            throw std::runtime_error("writeback");
        }
        Write write;
        write.arg = std::stoi(piece.substr(0, plus));
        const std::string off = piece.substr(plus + 1, colon - plus - 1);
        if (!off.empty() && off.back() == 'b') {
            write.size = 1;
            write.offset = static_cast<std::uint32_t>(std::stoul(off.substr(0, off.size() - 1), nullptr, 16));
        } else {
            write.offset = static_cast<std::uint32_t>(std::stoul(off, nullptr, 16));
        }
        write.value = static_cast<std::uint32_t>(std::stoul(piece.substr(colon + 1), nullptr, 16));
        ret.writes.push_back(write);
        if (comma == std::string::npos) {
            break;
        }
        cursor = comma + 1;
    }
    return ret;
}

std::vector<std::uint8_t> parseHex(const std::string& text)
{
    if (text.size() % 2 != 0) {
        throw std::runtime_error("odd hex");
    }
    std::vector<std::uint8_t> out(text.size() / 2);
    for (std::size_t index = 0; index < out.size(); ++index) {
        out[index] = static_cast<std::uint8_t>(std::stoul(text.substr(index * 2, 2), nullptr, 16));
    }
    return out;
}

std::string hexBytes(const std::uint8_t* data, std::size_t size)
{
    static const char digits[] = "0123456789abcdef";
    std::string text(size * 2, '0');
    for (std::size_t index = 0; index < size; ++index) {
        text[index * 2] = digits[data[index] >> 4];
        text[index * 2 + 1] = digits[data[index] & 0x0F];
    }
    return text;
}

void execute(const std::vector<std::string>& words)
{
    if (words.size() < 12 || words[0] != "run") {
        throw std::runtime_error("expected run");
    }
    std::size_t cursor = 1;
    auto take = [&]() -> const std::string& {
        if (cursor >= words.size()) {
            throw std::runtime_error("truncated run");
        }
        return words[cursor++];
    };
    const auto va = static_cast<std::uint32_t>(std::stoul(take(), nullptr, 16));
    const auto eaxIn = static_cast<std::uint32_t>(std::stoul(take(), nullptr, 16));
    const std::string gameLogicToken = take();
    const std::string tableToken = take();
    const std::string scaleToken = take();
    const std::string partToken = take();
    const std::string subToken = take();
    const std::string regToken = take();
    const int argc = std::stoi(take(), nullptr, 10);
    std::vector<std::string> argTokens;
    for (int index = 0; index < argc; ++index) {
        argTokens.push_back(take());
    }
    const int objects = std::stoi(take(), nullptr, 10);
    Run run;
    run.env.call = onCall;
    run.env.callCtx = &run;
    for (int index = 0; index < objects; ++index) {
        const auto bytes = parseHex(take());
        Block block = allocate(static_cast<std::uint32_t>(bytes.size()));
        std::memcpy(block.data, bytes.data(), bytes.size());
        run.blocks.push_back(block);
    }
    const int fixups = std::stoi(take(), nullptr, 10);
    for (int index = 0; index < fixups; ++index) {
        const int object = std::stoi(take(), nullptr, 10);
        const auto offset = static_cast<std::uint32_t>(std::stoul(take(), nullptr, 16));
        const int target = std::stoi(take(), nullptr, 10);
        const auto value = addressOf(run.blocks.at(static_cast<std::size_t>(target)).data);
        std::memcpy(run.blocks.at(static_cast<std::size_t>(object)).data + offset, &value, 4);
    }
    const int returns = std::stoi(take(), nullptr, 10);
    for (int index = 0; index < returns; ++index) {
        run.rets.push_back(parseRet(take(), run));
    }
    if (cursor != words.size()) {
        throw std::runtime_error("trailing tokens");
    }
    auto global = [&](const std::string& token) -> std::uint32_t {
        return token == "-" ? 0u : resolve(token, run);
    };
    run.env.gameLogic = global(gameLogicToken);
    run.env.logicTable = global(tableToken);
    run.env.timeScaleBits = static_cast<std::uint32_t>(std::stoul(scaleToken, nullptr, 16));
    run.env.partition = global(partToken);
    run.env.subsystem = global(subToken);
    run.env.registry = global(regToken);
    std::vector<std::uint32_t> stack;
    for (const auto& token : argTokens) {
        stack.push_back(resolve(token, run));
    }
    std::uint32_t eax = 0;
    const bool known = ra3::gamelogic::runAIState7D(
        va, run.blocks.at(0).data, eaxIn, stack.data(), argc, run.env, eax);
    if (!known) {
        throw std::runtime_error("unknown va");
    }
    std::cout << "OK " << hex8(eax);
    for (const auto& block : run.blocks) {
        std::cout << ' ' << hex8(addressOf(block.data));
    }
    std::cout << " |";
    for (const auto& block : run.blocks) {
        std::cout << hexBytes(block.data, block.size);
    }
    std::cout << " |" << (run.log.empty() ? "-" : run.log) << '\n';
}

}  // namespace

int main()
{
    std::string line;
    while (std::getline(std::cin, line)) {
        if (!line.empty() && line.back() == '\r') {
            line.pop_back();
        }
        if (line.empty()) {
            continue;
        }
        if (line == "quit") {
            return 0;
        }
        try {
            execute(tokens(line));
        } catch (const std::exception& error) {
            std::cout << "error " << error.what() << '\n';
            return 1;
        }
    }
    return 0;
}
