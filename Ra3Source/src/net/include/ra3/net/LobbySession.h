#pragma once

#include <cstddef>
#include <cstdint>

namespace ra3::net {

// Non-owning view of the lobby/session object whose primary vtable is
// 0x00BF1558.  Ownership of that vtable is the vptr store in 0x006225A0
// and 0x00622B30, the scalar-deleting destructor at slot 0 (0x00627820),
// and the 0x1ED8-byte allocation stored at global 0x00DF6AF4 by
// 0x0062A5E0.  A secondary vptr at +0x04 is 0x00BF1548.
//
// The original class name is not recovered.  Slot 4 (0x00627840) uses
// the literals "quickmatch", "ChatRoom1", "LobbyCustomMap" and
// "GUI:%s", which places the object in the lobby/session module.
// Generals GameNetwork/GameInfo.h and StagingRoomGameInfo.h were read
// only to recognise that family; RA3 offsets are not those fields and
// are not named from the GPL headers.
//
// Byte loads are `mov al, m8` and therefore leave EAX[31:8] unchanged.
// These methods return the byte.  A hook would have to preserve the
// incoming high bits; none of these entries are hooked.
class LobbySession {
public:
    explicit LobbySession(void* storage) noexcept;
    explicit LobbySession(const void* storage) noexcept;

    static constexpr std::size_t minimumRecoveredSize = 0x1ED8;

    [[nodiscard]] std::uint8_t byte00FC() const noexcept;  // 0x00622820 slot 5 +0xFC
    [[nodiscard]] bool dwordE8IsNonzero() const noexcept;  // 0x00622830 slot 14 +0xE8
    void setDword00F4(std::uint32_t value) noexcept;  // 0x00622850 slot 17 +0xF4
    [[nodiscard]] std::uint32_t dword00F4() const noexcept;  // 0x00622860 slot 18 +0xF4
    void setDword00F8(std::uint32_t value) noexcept;  // 0x00622870 slot 19 +0xF8
    [[nodiscard]] std::uint32_t dword00F8() const noexcept;  // 0x00622880 slot 20 +0xF8
    [[nodiscard]] void* field0098() const noexcept;  // 0x00622890 slot 25 +0x98
    [[nodiscard]] void* field00B4() const noexcept;  // 0x006228A0 slot 26 +0xB4
    [[nodiscard]] std::uint32_t dword0104() const noexcept;  // 0x006228C0 slot 39 +0x104
    void setDword0118(std::uint32_t value) noexcept;  // 0x00622910 slot 48 +0x118
    void setDword011C(std::uint32_t value) noexcept;  // 0x00622920 slot 50 +0x11C
    [[nodiscard]] void* field007C() const noexcept;  // 0x00622930 slot 32 +0x7C
    [[nodiscard]] void* field00DC() const noexcept;  // 0x00622940 slot 34 +0xDC
    [[nodiscard]] std::uint32_t dword1E2C() const noexcept;  // 0x00622950 slot 68 +0x1E2C
    [[nodiscard]] std::uint32_t dword0124() const noexcept;  // 0x00622980 slot 98 +0x124
    [[nodiscard]] std::uint32_t dword0128() const noexcept;  // 0x00622990 slot 100 +0x128
    [[nodiscard]] std::uint8_t byte1E88() const noexcept;  // 0x006229E0 slot 110 +0x1E88
    void setByte1E88(std::uint32_t value) noexcept;  // 0x006229F0 slot 111 +0x1E88
    [[nodiscard]] std::uint8_t byte0134() const noexcept;  // 0x005D4950 slot 63 +0x134
    [[nodiscard]] void* field0018() const noexcept;  // 0x005D4A20 slot 80 +0x18
    [[nodiscard]] void* field001C() const noexcept;  // 0x005D4A40 slot 82 +0x1C
    [[nodiscard]] void* field1E4C() const noexcept;  // 0x005D4A50 slot 91 +0x1E4C
    void setByte0120(std::uint32_t value) noexcept;  // 0x005D4A90 slot 69 +0x120
    void setByte0121(std::uint32_t value) noexcept;  // 0x005D4AA0 slot 70 +0x121
    [[nodiscard]] std::uint8_t byte0120() const noexcept;  // 0x005D4AB0 slot 71 +0x120
    [[nodiscard]] std::uint8_t byte0121() const noexcept;  // 0x005D4AC0 slot 72 +0x121
    void setDword012C(std::uint32_t value) noexcept;  // 0x005D4AD0 slot 105 +0x12C
    [[nodiscard]] std::uint32_t dword012C() const noexcept;  // 0x005D4AE0 slot 106 +0x12C
    void setDword1E8C(std::uint32_t value) noexcept;  // 0x005D4AF0 slot 112 +0x1E8C
    void setDword1E90(std::uint32_t value) noexcept;  // 0x005D4B00 slot 113 +0x1E90
    void setDword1E94(std::uint32_t value) noexcept;  // 0x005D4B10 slot 114 +0x1E94
    void setDword1EA0(std::uint32_t value) noexcept;  // 0x005D4B20 slot 117 +0x1EA0
    void setDword1EA4(std::uint32_t value) noexcept;  // 0x005D4B30 slot 118 +0x1EA4
    [[nodiscard]] std::uint32_t dword1E8C() const noexcept;  // 0x005D4B40 slot 121 +0x1E8C
    [[nodiscard]] std::uint32_t dword1E90() const noexcept;  // 0x005D4B50 slot 122 +0x1E90
    [[nodiscard]] std::uint32_t dword1E94() const noexcept;  // 0x005D4B60 slot 123 +0x1E94
    [[nodiscard]] std::uint32_t dword1E98() const noexcept;  // 0x005D4B70 slot 124 +0x1E98
    [[nodiscard]] std::uint32_t dword1E9C() const noexcept;  // 0x005D4B80 slot 125 +0x1E9C
    [[nodiscard]] std::uint32_t dword1EA0() const noexcept;  // 0x005D4B90 slot 126 +0x1EA0
    [[nodiscard]] std::uint32_t dword1EA4() const noexcept;  // 0x005D4BA0 slot 127 +0x1EA4
    [[nodiscard]] std::uint32_t dword1EAC() const noexcept;  // 0x005D4BB0 slot 128 +0x1EAC
    [[nodiscard]] std::uint32_t dword1EB0() const noexcept;  // 0x005D4BC0 slot 129 +0x1EB0
    void setByte1EB4(std::uint32_t value) noexcept;  // 0x005D4BD0 slot 131 +0x1EB4
    [[nodiscard]] std::uint8_t byte1EB4() const noexcept;  // 0x005D4BE0 slot 132 +0x1EB4
    void setDword1EB8(std::uint32_t value) noexcept;  // 0x005D4BF0 slot 133 +0x1EB8
    [[nodiscard]] std::uint32_t dword1EB8() const noexcept;  // 0x005D4C00 slot 134 +0x1EB8

private:
    std::byte* data_;
};

}  // namespace ra3::net
