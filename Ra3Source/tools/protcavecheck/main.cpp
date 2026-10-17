#include "ra3/gamelogic/ProtectedCaveLogic.h"

#include <cstdint>
#include <cstring>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

#ifdef _WIN32
#include <windows.h>
#endif

namespace {

using ra3::gamelogic::CaveState;
using ra3::gamelogic::kCaveImageBase;
using ra3::gamelogic::kCaveImageSize;

int hexVal(char c) {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return -1;
}

bool parseHex(const std::string& text, std::uint8_t* out, std::size_t bytes) {
    if (text.size() != bytes * 2u) return false;
    for (std::size_t i = 0; i < bytes; ++i) {
        const int hi = hexVal(text[i * 2u]);
        const int lo = hexVal(text[i * 2u + 1u]);
        if (hi < 0 || lo < 0) return false;
        out[i] = static_cast<std::uint8_t>((hi << 4) | lo);
    }
    return true;
}

std::vector<std::uint32_t> parseList(const std::string& text) {
    std::vector<std::uint32_t> out;
    if (text == ".") return out;
    std::size_t pos = 0;
    while (pos < text.size()) {
        const std::size_t comma = text.find(',', pos);
        const std::string part = text.substr(pos, comma == std::string::npos ? std::string::npos : comma - pos);
        out.push_back(static_cast<std::uint32_t>(std::stoul(part, nullptr, 16)));
        if (comma == std::string::npos) break;
        pos = comma + 1u;
    }
    return out;
}

void parseGlobals(CaveState& st, const std::string& text) {
    st.globalCount = 0;
    if (text == ".") return;
    std::size_t pos = 0;
    while (pos < text.size() && st.globalCount < 12) {
        const std::size_t comma = text.find(',', pos);
        const std::string part = text.substr(pos, comma == std::string::npos ? std::string::npos : comma - pos);
        const std::size_t eq = part.find('=');
        if (eq == std::string::npos) return;
        st.globalAddr[st.globalCount] = static_cast<std::uint32_t>(std::stoul(part.substr(0, eq), nullptr, 16));
        st.globalVal[st.globalCount] = static_cast<std::uint32_t>(std::stoul(part.substr(eq + 1u), nullptr, 16));
        ++st.globalCount;
        if (comma == std::string::npos) break;
        pos = comma + 1u;
    }
}

void printHex(const std::uint8_t* bytes, std::size_t n) {
    static const char kHex[] = "0123456789abcdef";
    for (std::size_t i = 0; i < n; ++i) {
        std::cout << kHex[bytes[i] >> 4] << kHex[bytes[i] & 0x0F];
    }
}

void dispatch(const std::string& name, CaveState& st, const std::vector<std::uint32_t>& args) {
    const std::uint32_t a0 = args.size() > 0u ? args[0] : 0u;
    const std::uint32_t a1 = args.size() > 1u ? args[1] : 0u;
    if (name == "clear26c") ra3::gamelogic::runClearNonZeroStatusWord(st);
    else if (name == "player") ra3::gamelogic::runObjectControllingPlayer(st);
    else if (name == "query") ra3::gamelogic::runQueryContainedModule(st, a0);
    else if (name == "substate") ra3::gamelogic::runSubControllerStateActive(st);
    else if (name == "classify") ra3::gamelogic::runClassifyObjectKindMask(st, a0, a1);
    else if (name == "filter") ra3::gamelogic::runObjectPassesKindFilter(st, a0);
    else if (name == "findkey") ra3::gamelogic::runFindBehaviorModuleByKey(st, a0);
    else if (name == "unreg") ra3::gamelogic::runUnregisterObjectSubsystems(st, a0);
    else if (name == "step") ra3::gamelogic::runObjectPositionStep(st, a0);
    else if (name == "idle") ra3::gamelogic::runConsiderObjectIdle(st, a0);
    else if (name == "reassign") ra3::gamelogic::runReassignControllingPlayer(st, a0);
    else if (name == "listen") ra3::gamelogic::runRegisterActiveModuleListener(st, a0);
    else if (name == "buckets") ra3::gamelogic::runNotifySixBucketListeners(st);
    else if (name == "part") ra3::gamelogic::runPartitionModifierCount(st, a0);
    else if (name == "aitarget") ra3::gamelogic::runAssignAiTargetRef(st, a0, a1);
    else throw std::runtime_error("unknown op");
}

}  // namespace

int main() {
#ifdef _WIN32
    void* mem = VirtualAlloc(reinterpret_cast<void*>(static_cast<std::uintptr_t>(kCaveImageBase)),
                             kCaveImageSize, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
    if (mem != reinterpret_cast<void*>(static_cast<std::uintptr_t>(kCaveImageBase))) {
        std::cerr << "image alloc failed\n";
        return 2;
    }
    auto* image = static_cast<std::uint8_t*>(mem);
#else
    static std::uint8_t image[kCaveImageSize];
#endif
    std::string line;
    while (std::getline(std::cin, line)) {
        if (!line.empty() && line.back() == '\r') line.pop_back();
        if (line.empty()) continue;
        if (line == "quit") return 0;
        std::istringstream iss(line);
        std::string name;
        std::string obj;
        std::string argsText;
        std::string retsText;
        std::string globText;
        iss >> name >> obj >> argsText >> retsText >> globText;
        if (!parseHex(obj, image, kCaveImageSize)) {
            std::cerr << "bad image\n";
            return 3;
        }
        const std::vector<std::uint32_t> args = parseList(argsText);
        const std::vector<std::uint32_t> rets = parseList(retsText);
        CaveState st{};
        st.image = image;
        st.retCount = static_cast<int>(rets.size());
        for (int i = 0; i < st.retCount; ++i) st.rets[i] = rets[static_cast<std::size_t>(i)];
        parseGlobals(st, globText);
        try {
            dispatch(name, st, args);
        } catch (const std::exception& ex) {
            std::cerr << ex.what() << "\n";
            return 4;
        }
        std::cout << std::hex;
        std::cout << "eax=" << st.eax << " edx=" << st.edx << " clean=" << std::dec << st.cleanup << " cbs=";
        if (st.callCount == 0) {
            std::cout << ".";
        } else {
            for (int i = 0; i < st.callCount; ++i) {
                if (i != 0) std::cout << ";";
                const auto& c = st.calls[i];
                std::cout << std::hex << c.target << ":" << c.ecx << ":" << c.a0 << ":" << c.a1
                          << ":" << c.a2 << ":" << std::dec << c.nstack << ":" << std::hex << c.result;
            }
        }
        std::cout << " glob=";
        if (st.globalCount == 0) {
            std::cout << ".";
        } else {
            for (int i = 0; i < st.globalCount; ++i) {
                if (i != 0) std::cout << ",";
                std::cout << std::hex << st.globalAddr[i] << "=" << st.globalVal[i];
            }
        }
        std::cout << " obj=";
        printHex(image, kCaveImageSize);
        std::cout << "\n" << std::dec;
        std::cout.flush();
    }
    return 0;
}
