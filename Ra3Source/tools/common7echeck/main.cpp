#include <ra3/common/CommonSubsystem.h>

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

std::string hexDword(std::uint32_t value)
{
    std::ostringstream stream;
    stream << std::hex << value;
    const std::string text = stream.str();
    return std::string(8U - text.size(), '0') + text;
}

std::string hexByte(std::uint8_t value)
{
    constexpr char digits[] = "0123456789abcdef";
    return std::string{digits[value >> 4], digits[value & 0xF]};
}

std::string hexBytes(const std::uint8_t* data, std::size_t size)
{
    std::string text;
    text.reserve(size * 2U);
    for (std::size_t index = 0; index < size; ++index) {
        text += hexByte(data[index]);
    }
    return text;
}

void putU32(std::uint8_t* data, std::size_t offset, std::uint32_t value)
{
    std::memcpy(data + offset, &value, sizeof(value));
}

template <typename Member>
std::uint32_t codeAddress(Member member)
{
    static_assert(sizeof(member) == sizeof(std::uint32_t), "single-inheritance member pointer");
    std::uint32_t address = 0;
    std::memcpy(&address, &member, sizeof(address));
    return address;
}

#if defined(_MSC_VER) && defined(_M_IX86)
std::uint32_t call0(void* object, std::uint32_t incoming, std::uint32_t function)
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

std::uint32_t call1(void* object, std::uint32_t incoming, std::uint32_t argument, std::uint32_t function)
{
    std::uint32_t result = 0;
    __asm {
        mov eax, incoming
        mov ecx, object
        mov edx, function
        push argument
        call edx
        mov result, eax
    }
    return result;
}

std::uint32_t call2(void* object, std::uint32_t incoming, std::uint32_t first, std::uint32_t second,
                    std::uint32_t function)
{
    std::uint32_t result = 0;
    __asm {
        mov eax, incoming
        mov ecx, object
        mov edx, function
        push second
        push first
        call edx
        mov result, eax
    }
    return result;
}
#endif

int g_calls = 0;
std::uint32_t g_ecx = 0;
std::uint32_t g_arg0 = 0;
std::uint32_t g_arg1 = 0;
std::uint32_t g_result = 0;

void resetTrace(std::uint32_t result)
{
    g_calls = 0;
    g_ecx = 0;
    g_arg0 = 0;
    g_arg1 = 0;
    g_result = result;
}

std::uint32_t __declspec(naked) availStub()
{
    __asm {
        mov eax, dword ptr [esp + 4]
        mov g_arg0, eax
        mov g_ecx, ecx
        inc g_calls
        mov eax, g_result
        ret 4
    }
}

std::uint32_t __declspec(naked) forwardOneStub()
{
    __asm {
        mov eax, dword ptr [esp + 4]
        mov g_arg0, eax
        mov g_ecx, ecx
        inc g_calls
        mov eax, g_result
        ret 4
    }
}

std::uint32_t __declspec(naked) forwardTwoStub()
{
    __asm {
        mov eax, dword ptr [esp + 4]
        mov g_arg0, eax
        mov eax, dword ptr [esp + 8]
        mov g_arg1, eax
        mov g_ecx, ecx
        inc g_calls
        mov eax, g_result
        ret 8
    }
}

using NameFn = std::uint32_t (ra3::common::CommonSubsystem::*)() const noexcept;

const NameFn kNames[] = {
    &ra3::common::CommonSubsystem::disguiseGuardName,
    &ra3::common::CommonSubsystem::engagementTrackerName,
    &ra3::common::CommonSubsystem::redAlertName,
    &ra3::common::CommonSubsystem::scoredKillTrackerName,
    &ra3::common::CommonSubsystem::moneyName,
    &ra3::common::CommonSubsystem::commandName,
    &ra3::common::CommonSubsystem::playerListName,
    &ra3::common::CommonSubsystem::energyName,
    &ra3::common::CommonSubsystem::playerRelationMapName,
    &ra3::common::CommonSubsystem::playerTechManagerName,
    &ra3::common::CommonSubsystem::resourceGatheringManagerName,
    &ra3::common::CommonSubsystem::teamRelationMapName,
    &ra3::common::CommonSubsystem::tunnelTrackerName,
    &ra3::common::CommonSubsystem::playerTeamListName,
    &ra3::common::CommonSubsystem::teamFactoryName,
    &ra3::common::CommonSubsystem::playerName,
    &ra3::common::CommonSubsystem::teamTemplateInfoName,
    &ra3::common::CommonSubsystem::teamName,
    &ra3::common::CommonSubsystem::teamPrototypeName,
    &ra3::common::CommonSubsystem::scoreKeeperName,
    &ra3::common::CommonSubsystem::logicEvaTrackerName,
    &ra3::common::CommonSubsystem::logicPlayingAudioTrackerName,
    &ra3::common::CommonSubsystem::logicViewTrackerName,
};

void commandName(std::uint32_t index)
{
    if (index >= sizeof(kNames) / sizeof(kNames[0])) {
        throw std::runtime_error("name index");
    }
    std::cout << hexDword(call0(nullptr, 0, codeAddress(kNames[index]))) << "\n";
}

void commandTextLen(std::uint32_t incoming, int length)
{
    std::uint8_t header[8] = {};
    std::uint8_t nodeB[4] = {};
    std::uint8_t nodeA[4] = {};
    std::uint8_t object[8] = {};
    putU32(object, 4, reinterpret_cast<std::uint32_t>(nodeA));
    putU32(nodeA, 0, reinterpret_cast<std::uint32_t>(nodeB));
    if (length < 0) {
        putU32(nodeB, 0, 0);
    } else {
        const auto stored = static_cast<std::uint16_t>(length);
        std::memcpy(header, &stored, sizeof(stored));
        putU32(nodeB, 0, reinterpret_cast<std::uint32_t>(header + 4));
    }
    const auto function = codeAddress(&ra3::common::CommonSubsystem::linkedTextLength);
    std::cout << hexDword(call0(object, incoming, function)) << "\n";
}

void commandTextPlus(std::uint32_t incoming, std::uint32_t extra, int length)
{
    std::uint8_t header[8] = {};
    std::uint8_t link[4] = {};
    std::uint8_t node[12] = {};
    std::uint8_t object[8] = {};
    putU32(object, 4, reinterpret_cast<std::uint32_t>(node));
    putU32(node, 4, extra);
    putU32(node, 8, reinterpret_cast<std::uint32_t>(link));
    if (length < 0) {
        putU32(link, 0, 0);
    } else {
        const auto stored = static_cast<std::uint16_t>(length);
        std::memcpy(header, &stored, sizeof(stored));
        putU32(link, 0, reinterpret_cast<std::uint32_t>(header + 4));
    }
    const auto function = codeAddress(&ra3::common::CommonSubsystem::linkedTextLengthPlus);
    std::cout << hexDword(call0(object, incoming, function)) << "\n";
}

void commandAdjust(std::uint32_t flag, std::uint32_t incoming, std::uint32_t start)
{
    std::uint8_t object[0x20];
    std::memset(object, 0xA5, sizeof(object));
    putU32(object, 0x10, start);
    const auto function = codeAddress(&ra3::common::CommonSubsystem::adjustTunnelCount);
    const auto eax = call1(object, incoming, flag, function);
    std::cout << hexDword(eax) << " " << hexBytes(object, sizeof(object)) << "\n";
}

void commandCopy(const std::vector<std::string>& words)
{
    const auto function = codeAddress(&ra3::common::CommonSubsystem::copyNestedTriple);
    const std::uint32_t incoming = number(words.at(2));
    if (words.at(1) == "null") {
        std::uint8_t object[0x90];
        std::uint8_t out[12];
        std::memset(object, 0x11, sizeof(object));
        std::memset(out, 0xA5, sizeof(out));
        putU32(object, 0x8C, 0);
        const auto eax = call1(object, incoming, reinterpret_cast<std::uint32_t>(out), function);
        const int upper = (eax & 0xFFFFFF00U) == (incoming & 0xFFFFFF00U) ? 1 : 0;
        std::cout << hexDword(eax & 0xFFU) << " " << upper << " " << hexBytes(out, sizeof(out)) << "\n";
        return;
    }
    std::uint8_t nested[0x54];
    std::memset(nested, 0x22, sizeof(nested));
    putU32(nested, 0x44, number(words.at(3)));
    putU32(nested, 0x48, number(words.at(4)));
    putU32(nested, 0x4C, number(words.at(5)));
    nested[0x50] = static_cast<std::uint8_t>(number(words.at(6)));
    std::uint8_t object[0x90];
    std::memset(object, 0x11, sizeof(object));
    putU32(object, 0x8C, reinterpret_cast<std::uint32_t>(nested));
    const auto nestedAddress = reinterpret_cast<std::uint32_t>(nested);
    if (words.at(1) == "alias") {
        auto* out = reinterpret_cast<std::uint32_t*>(nested + 0x48);
        const auto eax = call1(object, incoming, reinterpret_cast<std::uint32_t>(out), function);
        const int upper = (eax & 0xFFFFFF00U) == (nestedAddress & 0xFFFFFF00U) ? 1 : 0;
        std::cout << hexDword(eax & 0xFFU) << " " << upper << " " << hexBytes(nested, sizeof(nested)) << "\n";
        return;
    }
    std::uint8_t out[12];
    std::memset(out, 0xA5, sizeof(out));
    const auto eax = call1(object, incoming, reinterpret_cast<std::uint32_t>(out), function);
    const int upper = (eax & 0xFFFFFF00U) == (nestedAddress & 0xFFFFFF00U) ? 1 : 0;
    std::cout << hexDword(eax & 0xFFU) << " " << upper << " " << hexBytes(out, sizeof(out)) << "\n";
}

void commandForward(const std::vector<std::string>& words, bool two)
{
    resetTrace(number(words.back()));
    const std::uint32_t incoming = number(words.at(2));
    std::uint8_t object[0x90];
    std::memset(object, 0x33, sizeof(object));
    std::uint32_t eax = 0;
    int ecxOk = 0;
    if (words.at(1) == "null") {
        putU32(object, 0x8C, 0);
        if (two) {
            const auto function = codeAddress(&ra3::common::CommonSubsystem::forwardNested34);
            eax = call2(object, incoming, number(words.at(3)), number(words.at(4)), function);
        } else {
            const auto function = codeAddress(&ra3::common::CommonSubsystem::forwardNested38);
            eax = call1(object, incoming, number(words.at(3)), function);
        }
    } else {
        std::uint32_t vtable[16] = {};
        std::uint8_t nested[4] = {};
        putU32(nested, 0, reinterpret_cast<std::uint32_t>(vtable));
        putU32(object, 0x8C, reinterpret_cast<std::uint32_t>(nested));
        if (two) {
            vtable[0x34 / 4] = reinterpret_cast<std::uint32_t>(&forwardTwoStub);
            const auto function = codeAddress(&ra3::common::CommonSubsystem::forwardNested34);
            eax = call2(object, incoming, number(words.at(3)), number(words.at(4)), function);
        } else {
            vtable[0x38 / 4] = reinterpret_cast<std::uint32_t>(&forwardOneStub);
            const auto function = codeAddress(&ra3::common::CommonSubsystem::forwardNested38);
            eax = call1(object, incoming, number(words.at(3)), function);
        }
        ecxOk = g_ecx == reinterpret_cast<std::uint32_t>(nested) ? 1 : 0;
    }
    std::cout << hexDword(eax) << " " << g_calls << " " << hexDword(g_arg0) << " "
              << hexDword(g_arg1) << " " << ecxOk << "\n";
}

void commandStore(std::uint32_t incoming, std::uint32_t value)
{
    std::uint8_t object[0xD4];
    std::memset(object, 0x5A, sizeof(object));
    const auto function = codeAddress(&ra3::common::CommonSubsystem::storeFlagCf);
    const auto eax = call1(object, incoming, value, function);
    std::cout << hexDword(eax) << " " << hexBytes(object, sizeof(object)) << "\n";
}

void commandBytes(std::uint32_t incoming, std::uint32_t b8, std::uint32_t ba, std::uint32_t at40,
                  std::uint32_t atD0)
{
    std::uint8_t object[0xD8];
    std::memset(object, 0x11, sizeof(object));
    object[0xB8] = static_cast<std::uint8_t>(b8);
    object[0xBA] = static_cast<std::uint8_t>(ba);
    object[0x40] = static_cast<std::uint8_t>(at40);
    object[0xD0] = static_cast<std::uint8_t>(atD0);
    const auto at40Fn = codeAddress(&ra3::common::CommonSubsystem::byteAt40);
    const auto atD0Fn = codeAddress(&ra3::common::CommonSubsystem::byteAtD0);
    const auto pred = codeAddress(&ra3::common::CommonSubsystem::byteB8UnlessBa);
    std::cout << hexDword(call0(object, incoming, at40Fn)) << " "
              << hexDword(call0(object, incoming, atD0Fn)) << " "
              << hexDword(call0(object, incoming, pred)) << "\n";
}

void commandMutant(std::uint32_t incoming, std::uint32_t b8, std::uint32_t ba)
{
    std::uint8_t object[0xC0];
    std::memset(object, 0x11, sizeof(object));
    object[0xB8] = static_cast<std::uint8_t>(b8);
    object[0xBA] = static_cast<std::uint8_t>(ba);
    const auto function = codeAddress(&ra3::common::CommonSubsystem::byteB8UnlessBaMutant);
    std::cout << hexDword(call0(object, incoming, function)) << "\n";
}

void commandAvail(std::uint32_t incoming, std::uint32_t ba, std::uint32_t b9, std::uint32_t bc,
                  std::uint32_t index, std::uint32_t slot, std::uint32_t stubret)
{
    resetTrace(stubret);
    std::uint8_t object[0xC0];
    std::memset(object, 0xA5, sizeof(object));
    std::uint32_t vtable[0x48 / 4] = {};
    vtable[0x44 / 4] = reinterpret_cast<std::uint32_t>(&availStub);
    putU32(object, 0, reinterpret_cast<std::uint32_t>(vtable));
    object[0xBA] = static_cast<std::uint8_t>(ba);
    object[0xB9] = static_cast<std::uint8_t>(b9);
    putU32(object, 0xBC, bc);
    putU32(object, 0x9C, index);
    const auto signedIndex = static_cast<std::int32_t>(index);
    if (signedIndex >= 0 && signedIndex < 0x14) {
        putU32(object, static_cast<std::size_t>(0x4C + signedIndex * 4), slot);
    }
    const auto function = codeAddress(&ra3::common::CommonSubsystem::availabilityByte);
    const auto eax = call0(object, incoming, function);
    const int ecxOk = g_calls != 0 && g_ecx == reinterpret_cast<std::uint32_t>(object) ? 1 : 0;
    std::cout << hexDword(eax) << " " << g_calls << " " << hexDword(g_arg0) << " " << ecxOk << "\n";
}

std::vector<std::string> split(const std::string& line)
{
    std::istringstream stream(line);
    std::vector<std::string> words;
    std::string word;
    while (stream >> word) {
        words.push_back(word);
    }
    return words;
}

}  // namespace

int main()
{
    try {
        std::string line;
        while (std::getline(std::cin, line)) {
            const auto words = split(line);
            if (words.empty()) {
                continue;
            }
            if (words[0] == "quit") {
                return 0;
            }
            if (words[0] == "name" && words.size() == 2) {
                commandName(number(words[1]));
            } else if (words[0] == "textlen" && words.size() == 3) {
                commandTextLen(number(words[1]), std::stoi(words[2], nullptr, 0));
            } else if (words[0] == "textplus" && words.size() == 4) {
                commandTextPlus(number(words[1]), number(words[2]), std::stoi(words[3], nullptr, 0));
            } else if (words[0] == "adjust" && words.size() == 4) {
                commandAdjust(number(words[1]), number(words[2]), number(words[3]));
            } else if (words[0] == "copy") {
                commandCopy(words);
            } else if (words[0] == "fwd34") {
                commandForward(words, true);
            } else if (words[0] == "fwd38") {
                commandForward(words, false);
            } else if (words[0] == "storecf" && words.size() == 3) {
                commandStore(number(words[1]), number(words[2]));
            } else if (words[0] == "bytes" && words.size() == 6) {
                commandBytes(number(words[1]), number(words[2]), number(words[3]), number(words[4]),
                             number(words[5]));
            } else if (words[0] == "mutant" && words.size() == 4) {
                commandMutant(number(words[1]), number(words[2]), number(words[3]));
            } else if (words[0] == "avail" && words.size() == 8) {
                commandAvail(number(words[1]), number(words[2]), number(words[3]), number(words[4]),
                             number(words[5]), number(words[6]), number(words[7]));
            } else {
                throw std::runtime_error("command");
            }
        }
    } catch (const std::exception&) {
        std::cout << "error\n";
        return 1;
    }
    return 0;
}
