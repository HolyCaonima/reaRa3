#include "ra3/net/LobbySession.h"

#include <cstring>

namespace ra3::net {

LobbySession::LobbySession(void* storage) noexcept
    : data_(static_cast<std::byte*>(storage)) {}

LobbySession::LobbySession(const void* storage) noexcept
    : data_(static_cast<std::byte*>(const_cast<void*>(storage))) {}

std::uint8_t LobbySession::byte00FC() const noexcept {
    return static_cast<std::uint8_t>(data_[0xfc]);
}

bool LobbySession::dwordE8IsNonzero() const noexcept {
    std::uint32_t value;
    std::memcpy(&value, data_ + 0xe8, sizeof(value));
    return value != 0;
}

void LobbySession::setDword00F4(std::uint32_t value) noexcept {
    std::memcpy(data_ + 0xf4, &value, sizeof(value));
}

std::uint32_t LobbySession::dword00F4() const noexcept {
    std::uint32_t value;
    std::memcpy(&value, data_ + 0xf4, sizeof(value));
    return value;
}

void LobbySession::setDword00F8(std::uint32_t value) noexcept {
    std::memcpy(data_ + 0xf8, &value, sizeof(value));
}

std::uint32_t LobbySession::dword00F8() const noexcept {
    std::uint32_t value;
    std::memcpy(&value, data_ + 0xf8, sizeof(value));
    return value;
}

void* LobbySession::field0098() const noexcept {
    return data_ + 0x98;
}

void* LobbySession::field00B4() const noexcept {
    return data_ + 0xb4;
}

std::uint32_t LobbySession::dword0104() const noexcept {
    std::uint32_t value;
    std::memcpy(&value, data_ + 0x104, sizeof(value));
    return value;
}

void LobbySession::setDword0118(std::uint32_t value) noexcept {
    std::memcpy(data_ + 0x118, &value, sizeof(value));
}

void LobbySession::setDword011C(std::uint32_t value) noexcept {
    std::memcpy(data_ + 0x11c, &value, sizeof(value));
}

void* LobbySession::field007C() const noexcept {
    return data_ + 0x7c;
}

void* LobbySession::field00DC() const noexcept {
    return data_ + 0xdc;
}

std::uint32_t LobbySession::dword1E2C() const noexcept {
    std::uint32_t value;
    std::memcpy(&value, data_ + 0x1e2c, sizeof(value));
    return value;
}

std::uint32_t LobbySession::dword0124() const noexcept {
    std::uint32_t value;
    std::memcpy(&value, data_ + 0x124, sizeof(value));
    return value;
}

std::uint32_t LobbySession::dword0128() const noexcept {
    std::uint32_t value;
    std::memcpy(&value, data_ + 0x128, sizeof(value));
    return value;
}

std::uint8_t LobbySession::byte1E88() const noexcept {
    return static_cast<std::uint8_t>(data_[0x1e88]);
}

void LobbySession::setByte1E88(std::uint32_t value) noexcept {
    data_[0x1e88] = static_cast<std::byte>(value);
}

std::uint8_t LobbySession::byte0134() const noexcept {
    return static_cast<std::uint8_t>(data_[0x134]);
}

void* LobbySession::field0018() const noexcept {
    return data_ + 0x18;
}

void* LobbySession::field001C() const noexcept {
    return data_ + 0x1c;
}

void* LobbySession::field1E4C() const noexcept {
    return data_ + 0x1e4c;
}

void LobbySession::setByte0120(std::uint32_t value) noexcept {
    data_[0x120] = static_cast<std::byte>(value);
}

void LobbySession::setByte0121(std::uint32_t value) noexcept {
    data_[0x121] = static_cast<std::byte>(value);
}

std::uint8_t LobbySession::byte0120() const noexcept {
    return static_cast<std::uint8_t>(data_[0x120]);
}

std::uint8_t LobbySession::byte0121() const noexcept {
    return static_cast<std::uint8_t>(data_[0x121]);
}

void LobbySession::setDword012C(std::uint32_t value) noexcept {
    std::memcpy(data_ + 0x12c, &value, sizeof(value));
}

std::uint32_t LobbySession::dword012C() const noexcept {
    std::uint32_t value;
    std::memcpy(&value, data_ + 0x12c, sizeof(value));
    return value;
}

void LobbySession::setDword1E8C(std::uint32_t value) noexcept {
    std::memcpy(data_ + 0x1e8c, &value, sizeof(value));
}

void LobbySession::setDword1E90(std::uint32_t value) noexcept {
    std::memcpy(data_ + 0x1e90, &value, sizeof(value));
}

void LobbySession::setDword1E94(std::uint32_t value) noexcept {
    std::memcpy(data_ + 0x1e94, &value, sizeof(value));
}

void LobbySession::setDword1EA0(std::uint32_t value) noexcept {
    std::memcpy(data_ + 0x1ea0, &value, sizeof(value));
}

void LobbySession::setDword1EA4(std::uint32_t value) noexcept {
    std::memcpy(data_ + 0x1ea4, &value, sizeof(value));
}

std::uint32_t LobbySession::dword1E8C() const noexcept {
    std::uint32_t value;
    std::memcpy(&value, data_ + 0x1e8c, sizeof(value));
    return value;
}

std::uint32_t LobbySession::dword1E90() const noexcept {
    std::uint32_t value;
    std::memcpy(&value, data_ + 0x1e90, sizeof(value));
    return value;
}

std::uint32_t LobbySession::dword1E94() const noexcept {
    std::uint32_t value;
    std::memcpy(&value, data_ + 0x1e94, sizeof(value));
    return value;
}

std::uint32_t LobbySession::dword1E98() const noexcept {
    std::uint32_t value;
    std::memcpy(&value, data_ + 0x1e98, sizeof(value));
    return value;
}

std::uint32_t LobbySession::dword1E9C() const noexcept {
    std::uint32_t value;
    std::memcpy(&value, data_ + 0x1e9c, sizeof(value));
    return value;
}

std::uint32_t LobbySession::dword1EA0() const noexcept {
    std::uint32_t value;
    std::memcpy(&value, data_ + 0x1ea0, sizeof(value));
    return value;
}

std::uint32_t LobbySession::dword1EA4() const noexcept {
    std::uint32_t value;
    std::memcpy(&value, data_ + 0x1ea4, sizeof(value));
    return value;
}

std::uint32_t LobbySession::dword1EAC() const noexcept {
    std::uint32_t value;
    std::memcpy(&value, data_ + 0x1eac, sizeof(value));
    return value;
}

std::uint32_t LobbySession::dword1EB0() const noexcept {
    std::uint32_t value;
    std::memcpy(&value, data_ + 0x1eb0, sizeof(value));
    return value;
}

void LobbySession::setByte1EB4(std::uint32_t value) noexcept {
    data_[0x1eb4] = static_cast<std::byte>(value);
}

std::uint8_t LobbySession::byte1EB4() const noexcept {
    return static_cast<std::uint8_t>(data_[0x1eb4]);
}

void LobbySession::setDword1EB8(std::uint32_t value) noexcept {
    std::memcpy(data_ + 0x1eb8, &value, sizeof(value));
}

std::uint32_t LobbySession::dword1EB8() const noexcept {
    std::uint32_t value;
    std::memcpy(&value, data_ + 0x1eb8, sizeof(value));
    return value;
}

}  // namespace ra3::net
