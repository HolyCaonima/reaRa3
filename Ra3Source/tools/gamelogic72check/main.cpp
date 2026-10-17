#include <ra3/gamelogic/ModuleIdentity72.h>

#include <cstdint>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>

namespace {

int hexValue(const std::string& text, std::uint32_t* out)
{
    if (text.empty()) {
        return 0;
    }
    try {
        std::size_t used = 0;
        const unsigned long value = std::stoul(text, &used, 16);
        if (used != text.size() || value > 0xFFFFFFFFul) {
            return 0;
        }
        *out = static_cast<std::uint32_t>(value);
        return 1;
    } catch (const std::exception&) {
        return 0;
    }
}

}  // namespace

int main()
{
    std::string line;
    while (std::getline(std::cin, line)) {
        if (line.empty()) {
            continue;
        }
        std::istringstream input(line);
        std::string command;
        input >> command;
        if (command == "quit") {
            return 0;
        }
        if (command == "name") {
            std::string vaText;
            input >> vaText;
            std::uint32_t va = 0;
            std::uint32_t eax = 0;
            if (!hexValue(vaText, &va) || !ra3::gamelogic::moduleIdentityRunName(va, &eax)) {
                std::cout << "error" << std::endl;
                continue;
            }
            std::cout << std::hex << "eax=" << eax << std::endl;
            continue;
        }
        if (command == "key") {
            std::string vaText;
            std::string guardText;
            std::string cacheText;
            std::string managerText;
            std::string stubText;
            input >> vaText >> guardText >> cacheText >> managerText >> stubText;
            std::uint32_t va = 0;
            std::uint32_t guard = 0;
            std::uint32_t cacheWord = 0;
            std::uint32_t manager = 0;
            std::uint32_t stub = 0;
            if (!hexValue(vaText, &va) || !hexValue(guardText, &guard) || !hexValue(cacheText, &cacheWord)
                || !hexValue(managerText, &manager) || !hexValue(stubText, &stub)) {
                std::cout << "error" << std::endl;
                continue;
            }
            ra3::gamelogic::ModuleDataCache cache{guard, cacheWord};
            std::uint32_t eax = 0;
            int called = 0;
            std::uint32_t seenManager = 0;
            std::uint32_t seenArg = 0;
            if (!ra3::gamelogic::moduleIdentityRunKey(
                    va, cache, manager, stub, &eax, &called, &seenManager, &seenArg)) {
                std::cout << "error" << std::endl;
                continue;
            }
            std::cout << std::hex << "eax=" << eax << " guard=" << cache.guard << " cache=" << cache.value
                      << std::dec << " called=" << called << std::hex << " arg=" << seenArg
                      << " mgr=" << seenManager << std::endl;
            continue;
        }
        std::cout << "error" << std::endl;
    }
    return 0;
}
