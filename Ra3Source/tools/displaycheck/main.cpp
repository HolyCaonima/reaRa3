#include <ra3/gameclient/DisplayExtent.h>

#include <cstdint>
#include <cstring>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

std::uint32_t number(const std::string& text)
{
    return static_cast<std::uint32_t>(std::stoul(text, nullptr, 0));
}

std::vector<std::uint8_t> parseHex(const std::string& text)
{
    if (text.size() % 2U != 0U) {
        throw std::runtime_error("odd hex");
    }
    std::vector<std::uint8_t> bytes(text.size() / 2U);
    for (std::size_t index = 0; index < bytes.size(); ++index) {
        bytes[index] = static_cast<std::uint8_t>(
            std::stoul(text.substr(index * 2U, 2U), nullptr, 16));
    }
    return bytes;
}

std::string hexByte(std::uint8_t value)
{
    constexpr char digits[] = "0123456789abcdef";
    return std::string{digits[value >> 4], digits[value & 0xF]};
}

std::string hexDword(std::uint32_t value)
{
    std::ostringstream stream;
    stream << std::hex << value;
    const std::string text = stream.str();
    return std::string(8U - text.size(), '0') + text;
}

std::string hexBytes(const std::vector<std::uint8_t>& bytes)
{
    std::string text;
    text.reserve(bytes.size() * 2U);
    for (std::uint8_t byte : bytes) {
        text += hexByte(byte);
    }
    return text;
}

constexpr std::size_t kObjectBytes = 0x130;

std::vector<std::uint8_t> objectFrom(const std::string& text)
{
    std::vector<std::uint8_t> bytes = parseHex(text);
    if (bytes.size() != kObjectBytes) {
        throw std::runtime_error("object size");
    }
    return bytes;
}

#if defined(_MSC_VER) && defined(_M_IX86)
std::uint32_t callFlag(const void* object, std::uint32_t incoming, const void* function)
{
    std::uint32_t result = 0;
    __asm {
        mov eax, incoming
        mov ecx, object
        mov edx, function
        call edx
        mov result, eax
    }
    return result;
}

std::uint32_t callStore(
    void* object, std::uint32_t incoming, std::uint32_t argument, const void* function)
{
    std::uint32_t result = 0;
    __asm {
        mov eax, incoming
        mov ecx, object
        push argument
        mov edx, function
        call edx
        mov result, eax
    }
    return result;
}

const void* codeOf(std::uint32_t (__thiscall ra3::gameclient::DisplayExtentObject::*method)() const)
{
    static_assert(sizeof(method) == sizeof(void*), "member pointer is a code address");
    const void* code = nullptr;
    std::memcpy(&code, &method, sizeof(code));
    return code;
}

const void* codeOf(std::uint32_t (__thiscall ra3::gameclient::DisplayExtentObject::*method)(std::uint32_t))
{
    static_assert(sizeof(method) == sizeof(void*), "member pointer is a code address");
    const void* code = nullptr;
    std::memcpy(&code, &method, sizeof(code));
    return code;
}
#endif

void run(const std::vector<std::string>& words)
{
    using ra3::gameclient::DisplayExtentObject;
    if (words.empty()) {
        throw std::runtime_error("empty");
    }
    const std::string& command = words[0];
    if (command == "loads") {
        const std::vector<std::uint8_t> bytes = objectFrom(words.at(1));
        const auto* object = reinterpret_cast<const DisplayExtentObject*>(bytes.data());
        std::cout << hexDword(object->loadPrimaryExtent()) << ' '
                  << hexDword(object->loadSecondaryExtent()) << ' '
                  << hexDword(object->loadField74()) << ' '
                  << hexDword(object->loadField110()) << ' '
                  << hexDword(object->loadField118()) << '\n';
        return;
    }
    if (command == "byteloads") {
#if defined(_MSC_VER) && defined(_M_IX86)
        const std::uint32_t incoming = number(words.at(1));
        std::vector<std::uint8_t> bytes = objectFrom(words.at(2));
        const auto* object = reinterpret_cast<const DisplayExtentObject*>(bytes.data());
        using Load = std::uint32_t (__thiscall DisplayExtentObject::*)() const;
        const Load loads[] = {
            &DisplayExtentObject::loadFlag5E,
            &DisplayExtentObject::loadFlag5F,
            &DisplayExtentObject::loadFlag60,
            &DisplayExtentObject::loadFlag12C,
        };
        for (std::size_t index = 0; index < 4; ++index) {
            if (index != 0) {
                std::cout << ' ';
            }
            std::cout << hexDword(callFlag(object, incoming, codeOf(loads[index])));
        }
        std::cout << '\n';
        return;
#else
        throw std::runtime_error("byteloads");
#endif
    }
    if (command == "dwordstores") {
        const std::uint32_t primary = number(words.at(1));
        const std::uint32_t secondary = number(words.at(2));
        const std::uint32_t fieldCC = number(words.at(3));
        const std::uint32_t field110 = number(words.at(4));
        const std::uint32_t pairFirst = number(words.at(5));
        const std::uint32_t pairSecond = number(words.at(6));
        std::vector<std::uint8_t> bytes = objectFrom(words.at(7));
        auto* object = reinterpret_cast<DisplayExtentObject*>(bytes.data());
        std::cout << hexDword(object->storePrimaryExtent(primary)) << ' '
                  << hexDword(object->storeSecondaryExtent(secondary)) << ' '
                  << hexDword(object->storeFieldCC(fieldCC)) << ' '
                  << hexDword(object->storeField110(field110)) << ' '
                  << hexDword(object->storePair74(pairFirst, pairSecond)) << ' '
                  << hexBytes(bytes) << '\n';
        return;
    }
    if (command == "bytestores") {
#if defined(_MSC_VER) && defined(_M_IX86)
        const std::uint32_t incoming = number(words.at(1));
        const std::uint32_t flag5F = number(words.at(2));
        const std::uint32_t flag60 = number(words.at(3));
        const std::uint32_t flag12C = number(words.at(4));
        std::vector<std::uint8_t> bytes = objectFrom(words.at(5));
        auto* object = reinterpret_cast<DisplayExtentObject*>(bytes.data());
        using Store = std::uint32_t (__thiscall DisplayExtentObject::*)(std::uint32_t);
        const Store stores[] = {
            &DisplayExtentObject::storeFlag5F,
            &DisplayExtentObject::storeFlag60,
            &DisplayExtentObject::storeFlag12C,
        };
        const std::uint32_t arguments[] = {flag5F, flag60, flag12C};
        for (std::size_t index = 0; index < 3; ++index) {
            std::cout << hexDword(callStore(object, incoming, arguments[index],
                                            codeOf(stores[index])))
                      << ' ';
        }
        std::cout << hexBytes(bytes) << '\n';
        return;
#else
        throw std::runtime_error("bytestores");
#endif
    }
    if (command == "aspect" || command == "aspectmutant") {
        const std::vector<std::uint8_t> bytes = objectFrom(words.at(1));
        const auto* object = reinterpret_cast<const DisplayExtentObject*>(bytes.data());
        const std::uint32_t result = command == "aspect"
            ? object->secondaryTimesFourBelowPrimaryTimesThree()
            : secondaryTimesFourBelowPrimaryTimesThreeMutant(*object);
        std::cout << hexDword(result) << '\n';
        return;
    }
    if (command == "triplealias") {
        std::vector<std::uint8_t> bytes = objectFrom(words.at(1));
        auto* object = reinterpret_cast<DisplayExtentObject*>(bytes.data());
        const auto* source = reinterpret_cast<const std::uint32_t*>(bytes.data() + 0x60);
        std::cout << hexDword(object->storeTriple64(source)) << ' '
                  << hexBytes(bytes) << '\n';
        return;
    }
    if (command == "quad") {
        const std::uint32_t values[4] = {
            number(words.at(1)), number(words.at(2)),
            number(words.at(3)), number(words.at(4)),
        };
        std::vector<std::uint8_t> bytes = objectFrom(words.at(5));
        auto* object = reinterpret_cast<DisplayExtentObject*>(bytes.data());
        std::cout << hexDword(object->storeQuad11C(values)) << ' '
                  << hexBytes(bytes) << '\n';
        return;
    }
    throw std::runtime_error("unknown command");
}

}  // namespace

int main()
{
    std::string line;
    while (std::getline(std::cin, line)) {
        if (line == "quit") {
            return 0;
        }
        try {
            std::istringstream stream(line);
            std::vector<std::string> words;
            std::string word;
            while (stream >> word) {
                words.push_back(word);
            }
            run(words);
        } catch (const std::exception&) {
            std::cout << "error\n";
            return 1;
        }
    }
    return 0;
}
