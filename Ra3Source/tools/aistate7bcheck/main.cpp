#include <ra3/gamelogic/AIState7B.h>

#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace {

constexpr std::size_t kAlign = 256;
constexpr std::uint32_t kObjectBytes = 0x480;
constexpr std::uint32_t kScratchBytes = 0x80;

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
};

Block allocate(std::uint32_t size)
{
    Block block;
    block.raw = ::operator new(size + kAlign);
    const auto raw = reinterpret_cast<std::uintptr_t>(block.raw);
    block.data = reinterpret_cast<std::uint8_t*>((raw + (kAlign - 1)) & ~(kAlign - 1));
    std::memset(block.data, 0, size);
    return block;
}

struct Ret {
    std::uint32_t value = 0;
    std::vector<std::pair<std::uint32_t, std::uint32_t>> writes;
};

struct Run {
    std::vector<Block> blocks;
    Block scratch;
    ra3::gamelogic::AIState7BEnv env{};
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
        ::operator delete(scratch.raw);
    }
};

std::string normalize(const Run& run, std::uint32_t value)
{
    for (std::size_t index = 0; index < run.blocks.size(); ++index) {
        const auto base = addressOf(run.blocks[index].data);
        if (value >= base && value < base + kObjectBytes) {
            char text[32];
            std::snprintf(text, sizeof(text), "@%u+%x", static_cast<unsigned>(index), value - base);
            return text;
        }
    }
    const auto scratch = addressOf(run.scratch.data);
    if (value >= scratch && value < scratch + kScratchBytes) {
        if (run.env.captureBytes == 0) {
            return "ptr";
        }
        const auto available = scratch + kScratchBytes - value;
        const auto count = std::min<std::uint32_t>(run.env.captureBytes, available);
        return "*" + hexBytes(reinterpret_cast<const std::uint8_t*>(static_cast<std::uintptr_t>(value)), count);
    }
    return hex8(value);
}

void applyMemset(std::uint32_t target, const std::uint32_t* args, int argc)
{
    if (target != 0x004D2F98u || argc < 3) {
        return;
    }
    std::memset(reinterpret_cast<void*>(static_cast<std::uintptr_t>(args[0])),
                static_cast<int>(args[1] & 0xFFu), args[2]);
}

std::uint32_t onCall(void* ctx, std::uint32_t target, std::uint32_t ecx, const std::uint32_t* args, int argc)
{
    auto* run = static_cast<Run*>(ctx);
    if (run->retIndex >= run->rets.size()) {
        throw std::runtime_error("stub queue empty");
    }
    applyMemset(target, args, argc);
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
    const Ret& spec = run->rets[run->retIndex++];
    if (argc > 0) {
        auto* dest = reinterpret_cast<std::uint8_t*>(static_cast<std::uintptr_t>(args[0]));
        for (const auto& write : spec.writes) {
            std::memcpy(dest + write.first, &write.second, 4);
        }
    }
    return spec.value;
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
        const auto head = plus == std::string::npos ? token.substr(1) : token.substr(1, plus - 1);
        const int index = std::stoi(head);
        if (index < 0 || static_cast<std::size_t>(index) >= run.blocks.size()) {
            throw std::runtime_error("object index");
        }
        auto value = addressOf(run.blocks[static_cast<std::size_t>(index)].data);
        if (plus != std::string::npos) {
            value += static_cast<std::uint32_t>(std::stoul(token.substr(plus + 1), nullptr, 16));
        }
        return value;
    }
    return static_cast<std::uint32_t>(std::stoul(token, nullptr, 16));
}

std::vector<std::uint8_t> parseHex(const std::string& text)
{
    if (text.size() != kObjectBytes * 2) {
        throw std::runtime_error("object hex");
    }
    std::vector<std::uint8_t> out(kObjectBytes);
    for (std::size_t index = 0; index < out.size(); ++index) {
        out[index] = static_cast<std::uint8_t>(std::stoul(text.substr(index * 2, 2), nullptr, 16));
    }
    return out;
}

Ret parseRet(const std::string& token, const Run& run)
{
    Ret ret;
    const auto hat = token.find('^');
    ret.value = resolve(hat == std::string::npos ? token : token.substr(0, hat), run);
    if (hat == std::string::npos) {
        return ret;
    }
    std::stringstream writes(token.substr(hat + 1));
    std::string item;
    while (std::getline(writes, item, ',')) {
        const auto eq = item.find('=');
        if (eq == std::string::npos) {
            throw std::runtime_error("write");
        }
        ret.writes.emplace_back(static_cast<std::uint32_t>(std::stoul(item.substr(0, eq), nullptr, 16)),
                                resolve(item.substr(eq + 1), run));
    }
    return ret;
}

void execute(const std::vector<std::string>& words)
{
    if (words.size() < 8 || words[0] != "run") {
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
    const auto limit = static_cast<std::uint32_t>(std::stoul(take(), nullptr, 16));
    const auto nearBits = static_cast<std::uint32_t>(std::stoul(take(), nullptr, 16));
    const std::string gameLogicToken = take();
    const int argc = std::stoi(take(), nullptr, 10);
    std::vector<std::string> argTokens;
    for (int index = 0; index < argc; ++index) {
        argTokens.push_back(take());
    }
    const int objects = std::stoi(take(), nullptr, 10);
    Run run;
    run.scratch = allocate(kScratchBytes);
    run.env.limitBits = limit;
    run.env.nearBits = nearBits;
    run.env.call = onCall;
    run.env.callCtx = &run;
    run.env.scratch = run.scratch.data;
    run.env.scratchBytes = kScratchBytes;
    for (int index = 0; index < objects; ++index) {
        const auto bytes = parseHex(take());
        Block block = allocate(kObjectBytes);
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
    run.env.captureBytes = static_cast<std::uint32_t>(std::stoul(take(), nullptr, 10));
    if (cursor != words.size()) {
        throw std::runtime_error("trailing tokens");
    }
    run.env.gameLogic = gameLogicToken == "0" ? 0u : resolve(gameLogicToken, run);
    std::vector<std::uint32_t> stack;
    for (const auto& token : argTokens) {
        stack.push_back(resolve(token, run));
    }
    std::uint32_t eax = 0;
    const bool known = ra3::gamelogic::runAIState7B(
        va, run.blocks.at(0).data, eaxIn, stack.empty() ? nullptr : stack.data(), argc, run.env, eax);
    if (!known) {
        throw std::runtime_error("unknown va");
    }
    std::cout << "eax=" << hex8(eax) << " bases=";
    for (std::size_t index = 0; index < run.blocks.size(); ++index) {
        if (index != 0) {
            std::cout << ',';
        }
        std::cout << hex8(addressOf(run.blocks[index].data));
    }
    std::cout << " log=" << (run.log.empty() ? "." : run.log) << " mem=";
    for (const auto& block : run.blocks) {
        std::cout << hexBytes(block.data, kObjectBytes);
    }
    std::cout << '\n';
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
            std::cout << "err=" << error.what() << '\n';
            return 1;
        }
    }
    return 0;
}
