#include "ra3/net/LobbySession.h"

#include <cstdint>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

std::vector<std::byte> decode(const std::string& text)
{
    if (text == ".") return {};
    if ((text.size() & 1U) != 0) throw std::runtime_error("odd hex length");
    std::vector<std::byte> result(text.size() / 2);
    const auto nibble = [](char value) -> unsigned {
        if (value >= '0' && value <= '9') return unsigned(value - '0');
        if (value >= 'a' && value <= 'f') return unsigned(value - 'a' + 10);
        if (value >= 'A' && value <= 'F') return unsigned(value - 'A' + 10);
        throw std::runtime_error("invalid hex");
    };
    for (std::size_t index = 0; index < result.size(); ++index) {
        result[index] = std::byte((nibble(text[index * 2]) << 4)
                                | nibble(text[index * 2 + 1]));
    }
    return result;
}

std::string encode(const std::vector<std::byte>& data)
{
    std::ostringstream out;
    out << std::hex << std::setfill('0');
    for (const auto value : data) out << std::setw(2) << std::to_integer<unsigned>(value);
    return out.str();
}

std::uint32_t number(const std::string& value) { return std::stoul(value, nullptr, 16); }

void run(const std::vector<std::string>& tokens)
{
    auto data = decode(tokens.at(1));
    if (data.size() < ra3::net::LobbySession::minimumRecoveredSize) {
        throw std::runtime_error("object too small");
    }
    const auto argument = tokens.size() > 2 ? number(tokens[2]) : 0U;
    const auto address = number(tokens[0]);
    std::uint32_t result = 0;
    ra3::net::LobbySession session(data.data());
    switch (address) {
    case 0x00622820:
        result = session.byte00FC();
        break;
    case 0x00622830:
        result = session.dwordE8IsNonzero() ? 1U : 0U;
        break;
    case 0x00622850:
        session.setDword00F4(argument);
        break;
    case 0x00622860:
        result = session.dword00F4();
        break;
    case 0x00622870:
        session.setDword00F8(argument);
        break;
    case 0x00622880:
        result = session.dword00F8();
        break;
    case 0x00622890:
        result = static_cast<std::uint32_t>(reinterpret_cast<std::byte*>(session.field0098()) - data.data());
        break;
    case 0x006228A0:
        result = static_cast<std::uint32_t>(reinterpret_cast<std::byte*>(session.field00B4()) - data.data());
        break;
    case 0x006228C0:
        result = session.dword0104();
        break;
    case 0x00622910:
        session.setDword0118(argument);
        break;
    case 0x00622920:
        session.setDword011C(argument);
        break;
    case 0x00622930:
        result = static_cast<std::uint32_t>(reinterpret_cast<std::byte*>(session.field007C()) - data.data());
        break;
    case 0x00622940:
        result = static_cast<std::uint32_t>(reinterpret_cast<std::byte*>(session.field00DC()) - data.data());
        break;
    case 0x00622950:
        result = session.dword1E2C();
        break;
    case 0x00622980:
        result = session.dword0124();
        break;
    case 0x00622990:
        result = session.dword0128();
        break;
    case 0x006229E0:
        result = session.byte1E88();
        break;
    case 0x006229F0:
        session.setByte1E88(argument);
        break;
    case 0x005D4950:
        result = session.byte0134();
        break;
    case 0x005D4A20:
        result = static_cast<std::uint32_t>(reinterpret_cast<std::byte*>(session.field0018()) - data.data());
        break;
    case 0x005D4A40:
        result = static_cast<std::uint32_t>(reinterpret_cast<std::byte*>(session.field001C()) - data.data());
        break;
    case 0x005D4A50:
        result = static_cast<std::uint32_t>(reinterpret_cast<std::byte*>(session.field1E4C()) - data.data());
        break;
    case 0x005D4A90:
        session.setByte0120(argument);
        break;
    case 0x005D4AA0:
        session.setByte0121(argument);
        break;
    case 0x005D4AB0:
        result = session.byte0120();
        break;
    case 0x005D4AC0:
        result = session.byte0121();
        break;
    case 0x005D4AD0:
        session.setDword012C(argument);
        break;
    case 0x005D4AE0:
        result = session.dword012C();
        break;
    case 0x005D4AF0:
        session.setDword1E8C(argument);
        break;
    case 0x005D4B00:
        session.setDword1E90(argument);
        break;
    case 0x005D4B10:
        session.setDword1E94(argument);
        break;
    case 0x005D4B20:
        session.setDword1EA0(argument);
        break;
    case 0x005D4B30:
        session.setDword1EA4(argument);
        break;
    case 0x005D4B40:
        result = session.dword1E8C();
        break;
    case 0x005D4B50:
        result = session.dword1E90();
        break;
    case 0x005D4B60:
        result = session.dword1E94();
        break;
    case 0x005D4B70:
        result = session.dword1E98();
        break;
    case 0x005D4B80:
        result = session.dword1E9C();
        break;
    case 0x005D4B90:
        result = session.dword1EA0();
        break;
    case 0x005D4BA0:
        result = session.dword1EA4();
        break;
    case 0x005D4BB0:
        result = session.dword1EAC();
        break;
    case 0x005D4BC0:
        result = session.dword1EB0();
        break;
    case 0x005D4BD0:
        session.setByte1EB4(argument);
        break;
    case 0x005D4BE0:
        result = session.byte1EB4();
        break;
    case 0x005D4BF0:
        session.setDword1EB8(argument);
        break;
    case 0x005D4C00:
        result = session.dword1EB8();
        break;
    default:
        throw std::runtime_error("unknown address");
    }
    std::cout << std::hex << result << ' ' << encode(data) << '\n';
}

}  // namespace

int main()
{
    std::ios::sync_with_stdio(false);
    std::string line;
    while (std::getline(std::cin, line)) {
        if (line == "quit") return 0;
        try {
            std::istringstream input(line);
            std::vector<std::string> tokens;
            for (std::string token; input >> token; ) tokens.push_back(token);
            if (tokens.empty()) continue;
            run(tokens);
        } catch (const std::exception&) {
            std::cout << "error\n";
        }
    }
    return 0;
}
