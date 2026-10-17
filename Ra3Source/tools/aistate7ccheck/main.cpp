#include <ra3/gamelogic/AIState7C.h>

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

struct Block {
    void* raw = nullptr;
    std::uint8_t* data = nullptr;
    std::uint32_t size = 0;
};

Block allocate(std::uint32_t size)
{
    Block block;
    block.raw = ::operator new(size + kAlign);
    auto raw = reinterpret_cast<std::uintptr_t>(block.raw);
    block.data = reinterpret_cast<std::uint8_t*>((raw + (kAlign - 1)) & ~(kAlign - 1));
    block.size = size;
    std::memset(block.raw, 0, size + kAlign);
    return block;
}

struct Run {
    std::vector<Block> blocks;
    ra3::gamelogic::AIState7CEnv env{};
    std::vector<std::uint32_t> rets;
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

std::string stackToken(std::uint32_t value, std::uint32_t entry)
{
    const auto delta = static_cast<std::int32_t>(value - entry);
    char text[32];
    if (delta < 0) {
        std::snprintf(text, sizeof(text), "s-%x", static_cast<unsigned>(-delta));
    } else {
        std::snprintf(text, sizeof(text), "s+%x", static_cast<unsigned>(delta));
    }
    return text;
}

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
    if (run.env.frameAddr != 0 && value >= run.env.frameAddr &&
        value < run.env.frameAddr + run.env.frameBytes) {
        return stackToken(value, run.env.entryEsp);
    }
    return hex8(value);
}

std::uint32_t onCall(void* ctx, std::uint32_t target, std::uint32_t ecx, const std::uint32_t* args, int argc)
{
    auto* run = static_cast<Run*>(ctx);
    if (!run->log.empty()) {
        run->log += ';';
    }
    run->log += hex8(target);
    run->log += ':';
    run->log += normalize(*run, ecx);
    if (argc <= 0) {
        run->log += ':';
    }
    for (int index = 0; index < argc; ++index) {
        run->log += index == 0 ? ':' : '+';
        run->log += normalize(*run, args[index]);
    }
    if (run->retIndex >= run->rets.size()) {
        return 0;
    }
    return run->rets[run->retIndex++];
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
        const int index = std::stoi(token.substr(1));
        if (index < 0 || static_cast<std::size_t>(index) >= run.blocks.size()) {
            throw std::runtime_error("object index");
        }
        return addressOf(run.blocks[static_cast<std::size_t>(index)].data);
    }
    return static_cast<std::uint32_t>(std::stoul(token, nullptr, 16));
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

void execute(const std::vector<std::string>& words)
{
    if (words.size() < 5 || words[0] != "run") {
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
    if (objects > 24) {
        throw std::runtime_error("too many objects");
    }
    run.env.spanCount = objects;
    for (int index = 0; index < objects; ++index) {
        run.env.spans[index].base = addressOf(run.blocks[index].data);
        run.env.spans[index].size = run.blocks[index].size;
        run.env.spans[index].data = run.blocks[index].data;
    }
    const int fixups = std::stoi(take(), nullptr, 10);
    for (int index = 0; index < fixups; ++index) {
        const int object = std::stoi(take(), nullptr, 10);
        const auto offset = static_cast<std::uint32_t>(std::stoul(take(), nullptr, 16));
        const int target = std::stoi(take(), nullptr, 10);
        const auto value = addressOf(run.blocks.at(static_cast<std::size_t>(target)).data);
        std::memcpy(run.blocks.at(static_cast<std::size_t>(object)).data + offset, &value, 4);
    }
    const int overrides = std::stoi(take(), nullptr, 10);
    for (int index = 0; index < overrides; ++index) {
        const auto address = static_cast<std::uint32_t>(std::stoul(take(), nullptr, 16));
        run.env.overrides[address] = resolve(take(), run);
    }
    const int returns = std::stoi(take(), nullptr, 10);
    for (int index = 0; index < returns; ++index) {
        run.rets.push_back(resolve(take(), run));
    }
    if (cursor != words.size()) {
        throw std::runtime_error("trailing tokens");
    }
    std::vector<std::uint32_t> stack;
    for (const auto& token : argTokens) {
        stack.push_back(resolve(token, run));
    }
    std::uint32_t eax = 0;
    const bool known = ra3::gamelogic::runAIState7C(
        va, run.blocks.at(0).data, eaxIn, stack.empty() ? nullptr : stack.data(), argc, run.env, eax);
    if (!known) {
        throw std::runtime_error("unknown va");
    }
    std::cout << "OK " << hex8(eax) << ' ' << hex8(run.env.frameAddr) << ' ' << hex8(run.env.entryEsp);
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
