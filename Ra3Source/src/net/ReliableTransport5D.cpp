#include "ra3/net/ReliableTransport5D.h"

#include <cstring>
#include <cstdint>

namespace ra3::net {
namespace {

template <typename T>
T load(const std::uint8_t* base, std::size_t offset) noexcept {
    T value{};
    std::memcpy(&value, base + offset, sizeof(T));
    return value;
}

template <typename T>
void store(std::uint8_t* base, std::size_t offset, T value) noexcept {
    std::memcpy(base + offset, &value, sizeof(T));
}

std::uint8_t* ptr(std::uint32_t bits) noexcept {
    return reinterpret_cast<std::uint8_t*>(static_cast<std::uintptr_t>(bits));
}

std::uint32_t bits(const void* pointer) noexcept {
    return static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(pointer));
}

std::uint32_t childByte(const std::uint8_t* self, std::uint32_t offset) noexcept {
    const auto child = load<std::uint32_t>(self, 0xC0);
    if (child == 0u) {
        return 0u;
    }
    return ptr(child)[offset];
}

std::uint32_t childDword(const std::uint8_t* self, std::uint32_t offset) noexcept {
    const auto child = load<std::uint32_t>(self, 0xC0);
    if (child == 0u) {
        return kEmptyString;
    }
    return load<std::uint32_t>(ptr(child), offset);
}

bool endpointSame(const std::uint8_t* left, const std::uint8_t* right) noexcept {
    return load<std::uint32_t>(left, 0) == load<std::uint32_t>(right, 0) &&
           load<std::uint16_t>(left, 4) == load<std::uint16_t>(right, 4);
}

bool acceptedSlot(const std::uint8_t* slot) noexcept {
    if (slot == nullptr) {
        return false;
    }
    const auto state = load<std::uint32_t>(slot, 4);
    if (state != 6u && state != 2u && state != 3u && state != 4u && state != 5u) {
        return false;
    }
    return slot[0x54] != 0;
}

std::uint8_t* wideNext(std::uint8_t* node) noexcept {
    return ptr(load<std::uint32_t>(node, 0x3FC));
}

std::uint8_t* shortNext(std::uint8_t* node) noexcept {
    return ptr(load<std::uint32_t>(node, 0x10));
}

}  // namespace

std::uint32_t childByte2E(const std::uint8_t* self) noexcept { return childByte(self, 0x2E); }
std::uint32_t childByte2D(const std::uint8_t* self) noexcept { return childByte(self, 0x2D); }
std::uint32_t childByte29(const std::uint8_t* self) noexcept { return childByte(self, 0x29); }
std::uint32_t childByte28(const std::uint8_t* self) noexcept { return childByte(self, 0x28); }

std::uint32_t childDword30(const std::uint8_t* self) noexcept {
    const auto child = load<std::uint32_t>(self, 0xC0);
    if (child == 0u) {
        return 0u;
    }
    return load<std::uint32_t>(ptr(child), 0x30);
}

std::uint32_t childField8C(const std::uint8_t* self) noexcept {
    const auto child = load<std::uint32_t>(self, 0xC0);
    if (child == 0u) {
        return kEmptyString;
    }
    return child + 0x8Cu;
}

std::uint32_t childDword6C(const std::uint8_t* self) noexcept { return childDword(self, 0x6C); }
std::uint32_t childDword7C(const std::uint8_t* self) noexcept { return childDword(self, 0x7C); }

std::uint32_t cursorFieldsDiffer(const std::uint8_t* self) noexcept {
    const auto first = load<std::uint32_t>(self, 0xF0);
    const auto second = load<std::uint32_t>(self, 0xF4);
    return (first & 0xFFFFFF00u) | (first != second ? 1u : 0u);
}

std::uint32_t storePair124(std::uint8_t* self, std::uint32_t first, std::uint32_t second) noexcept {
    store<std::uint32_t>(self, 0x124, first);
    store<std::uint32_t>(self, 0x128, second);
    return first;
}

std::uint32_t flagOrPointerSet(const std::uint8_t* self) noexcept {
    if (self[0x134] != 0 || load<std::uint32_t>(self, 0x130) != 0u) {
        return 1u;
    }
    return 0u;
}

std::int32_t natBufferRead(std::uint8_t* self, std::uint8_t* dest, std::int32_t count) noexcept {
    if (count < 0) {
        return -1;
    }
    const auto pos = load<std::uint32_t>(self, 0x1C);
    const auto size = load<std::uint32_t>(self, 0x14);
    const auto avail = static_cast<std::int32_t>(size - pos);
    if (count > avail) {
        count = avail;
    }
    if (count != 0 && dest != nullptr) {
        std::memcpy(dest, ptr(load<std::uint32_t>(self, 0x20)) + pos,
                    static_cast<std::size_t>(count));
    }
    store<std::uint32_t>(self, 0x1C, pos + static_cast<std::uint32_t>(count));
    return count;
}

std::uint32_t natBufferSeek(std::uint8_t* self, std::uint32_t offset, std::uint32_t origin) noexcept {
    std::uint32_t pos = 0;
    if (origin == 0u) {
        pos = offset;
    } else if (origin == 1u) {
        pos = load<std::uint32_t>(self, 0x1C) + offset;
    } else if (origin == 2u) {
        pos = load<std::uint32_t>(self, 0x14) + offset;
    } else {
        return 0xFFFFFFFFu;
    }
    if (pos > load<std::uint32_t>(self, 0x14)) {
        return 0xFFFFFFFFu;
    }
    store<std::uint32_t>(self, 0x1C, pos);
    return pos;
}

std::uint32_t clearSlotIpAddresses(std::uint8_t* self) noexcept {
    for (std::uint32_t index = 0; index < 6u; ++index) {
        auto* slot = ptr(load<std::uint32_t>(self, 4u + index * 4u));
        if (slot != nullptr) {
            store<std::uint32_t>(slot, 0x10, 0xFFFFFFFFu);
        }
    }
    return 6u;
}

std::uint32_t dismissIndexedSlot(std::uint8_t* self) noexcept {
    const auto raw = self[0xFA];
    const auto signedIndex = static_cast<std::int8_t>(raw);
    std::uint32_t result = raw;
    if (signedIndex > 0 && raw < 6u) {
        auto* slot = ptr(load<std::uint32_t>(self, 4u + static_cast<std::uint32_t>(raw) * 4u));
        auto* table = ptr(load<std::uint32_t>(slot, 0));
        using Method = std::uint32_t (*)(void*);
        auto method = reinterpret_cast<Method>(static_cast<std::uintptr_t>(load<std::uint32_t>(table, 0x10)));
        result = method(slot);
    }
    self[0xFA] = 0xFF;
    return result;
}

std::uint32_t findLocalHumanSlot(const std::uint8_t* self) noexcept {
    // Mutation point: the shipped walk is six slots.
    constexpr std::uint32_t kLocalHumanSlotLimit = 6u;
    if (self[0x2C] == 0) {
        return 0xFFFFFFFFu;
    }
    for (std::uint32_t index = 0; index < kLocalHumanSlotLimit; ++index) {
        auto* slot = ptr(load<std::uint32_t>(self, 4u + index * 4u));
        if (slot != nullptr && load<std::uint32_t>(slot, 4) == 6u &&
            load<std::uint32_t>(slot, 0x38) == load<std::uint32_t>(self, 0x30)) {
            return index;
        }
    }
    return 0xFFFFFFFFu;
}

std::uint32_t slotZeroIsLocalHuman(const std::uint8_t* self) noexcept {
    if (self[0x2C] == 0) {
        return 0u;
    }
    auto* slot = ptr(load<std::uint32_t>(self, 4));
    if (load<std::uint32_t>(slot, 4) == 6u &&
        load<std::uint32_t>(slot, 0x38) == load<std::uint32_t>(self, 0x30)) {
        return 1u;
    }
    return 0u;
}

std::uint32_t otherSlotIsHuman(const std::uint8_t* self) noexcept {
    for (std::uint32_t index = 0; index < 6u; ++index) {
        // Slot 12 of GameInfo 00BE8018 / SkirmishGameInfo 00BE1F68 is findLocalHumanSlot.
        const auto local = findLocalHumanSlot(self);
        if (index == local) {
            continue;
        }
        auto* slot = ptr(load<std::uint32_t>(self, 4u + index * 4u));
        if (load<std::uint32_t>(slot, 4) == 6u) {
            return 1u;
        }
    }
    return 0u;
}

std::uint32_t countAcceptedSlots(const std::uint8_t* self) noexcept {
    std::uint32_t count = 0;
    for (std::uint32_t index = 0; index < 6u; ++index) {
        if (acceptedSlot(ptr(load<std::uint32_t>(self, 4u + index * 4u)))) {
            ++count;
        }
    }
    return count;
}

std::uint32_t clearLanHumanReady(std::uint8_t* self, std::uint8_t* network) noexcept {
    if (network != nullptr) {
        auto* table = ptr(load<std::uint32_t>(network, 0));
        using Method = void (*)(void*);
        auto method = reinterpret_cast<Method>(static_cast<std::uintptr_t>(load<std::uint32_t>(table, 0x7C)));
        method(network);
    }
    constexpr std::uint32_t kOffsets[6] = {0x100, 0x180, 0x200, 0x280, 0x300, 0x380};
    for (const auto offset : kOffsets) {
        if (load<std::uint32_t>(self, offset) == 6u) {
            self[offset + 4] = 0;
        }
    }
    return 6u;
}

std::uint32_t linkStateIsOne(const std::uint8_t* self) noexcept {
    return load<std::uint32_t>(self, 0x24) == 1u ? 1u : 0u;
}

std::uint32_t linkChildField8(const std::uint8_t* self) noexcept {
    const auto child = load<std::uint32_t>(self, 0x28);
    if (child == 0u) {
        return 0u;
    }
    return load<std::uint32_t>(ptr(child), 8);
}

std::uint32_t clampChannelStamp(std::uint8_t* self, std::uint32_t index, std::uint32_t value) noexcept {
    auto* cell = ptr(load<std::uint32_t>(self, 0x28)) + static_cast<std::size_t>(index) * 0x188u + 0x4C;
    if (value < load<std::uint32_t>(cell, 0)) {
        store<std::uint32_t>(cell, 0, value);
    }
    return bits(cell);
}

std::uint32_t setChannelBit(std::uint8_t* self, std::uint32_t bit) noexcept {
    const auto mask = 1u << (bit & 31u);
    store<std::uint32_t>(self, 0x2C, load<std::uint32_t>(self, 0x2C) | mask);
    return bits(self);
}

std::uint32_t channelFrameReached(const std::uint8_t* self, std::uint32_t index,
                                  const std::uint8_t* gameInfo, const std::uint8_t* gameLogic) noexcept {
    if (index >= 6u) {
        return 0u;
    }
    if (gameInfo == nullptr || bits(gameInfo) + 4u == 0u) {
        return 0u;
    }
    auto* slot = ptr(load<std::uint32_t>(gameInfo, 4u + index * 4u));
    if (slot == nullptr || load<std::uint32_t>(slot, 4) != 6u) {
        return 0u;
    }
    auto* child = ptr(load<std::uint32_t>(self, 0x28));
    const auto stamp = load<std::uint32_t>(child, static_cast<std::size_t>(index) * 0x188u + 0x4C);
    const auto frame = load<std::uint32_t>(gameLogic, 0x58);
    return stamp > frame ? 0u : 1u;
}

std::uint32_t chainNodeAt(const std::uint8_t* self, std::int32_t index) noexcept {
    auto* node = ptr(load<std::uint32_t>(self, 0x28));
    if (index < 0) {
        return 0u;
    }
    if (index == 0) {
        return bits(node);
    }
    while (index != 0) {
        --index;
        if (node == nullptr) {
            return 0u;
        }
        node = wideNext(node);
        if (index == 0) {
            break;
        }
    }
    return bits(node);
}

std::uint32_t findEndpointNode(const std::uint8_t* self, const std::uint8_t* key) noexcept {
    auto* node = ptr(load<std::uint32_t>(self, 0x24));
    if (node == nullptr) {
        return 0u;
    }
    const auto address = load<std::uint32_t>(key, 0);
    const auto port = load<std::uint16_t>(key, 4);
    while (node != nullptr) {
        const auto nodeAddress = load<std::uint32_t>(node, 0x14);
        if (nodeAddress == address && load<std::uint16_t>(node, 0x18) == port) {
            return bits(node);
        }
        if (port == 0 && nodeAddress == address) {
            return bits(node);
        }
        node = shortNext(node);
    }
    return 0u;
}

std::uint32_t headEndpointMatches(std::uint8_t* self) noexcept {
    auto* head = ptr(load<std::uint32_t>(self, 0x5C));
    if (head == nullptr) {
        return 0u;
    }
    auto* table = ptr(load<std::uint32_t>(self, 0));
    using Method = std::uint32_t (*)(void*);
    auto method = reinterpret_cast<Method>(static_cast<std::uintptr_t>(load<std::uint32_t>(table, 0xF8)));
    auto* endpoint = ptr(method(self));
    if (load<std::uint32_t>(head, 0x134) != load<std::uint32_t>(endpoint, 0)) {
        return 0u;
    }
    if (load<std::uint16_t>(head, 0x138) != load<std::uint16_t>(endpoint, 4)) {
        return 0u;
    }
    return 1u;
}

std::uint32_t endpointsEqual(const std::uint8_t* self, const std::uint8_t* other) noexcept {
    return endpointSame(self, other) ? 1u : 0u;
}

std::uint32_t endpointsDiffer(const std::uint8_t* self, const std::uint8_t* other) noexcept {
    return endpointSame(self, other) ? 0u : 1u;
}

std::uint32_t clampTinyState(std::uint8_t* self, std::uint32_t value) noexcept {
    const auto next = value + 1u;
    store<std::uint32_t>(self, 0x1C, next > 3u ? 0xFFFFFFFFu : value);
    return value;
}

std::uint32_t recordAtFc(const std::uint8_t* self, std::uint32_t index) noexcept {
    if (index > 5u) {
        return 0u;
    }
    return bits(self + (static_cast<std::size_t>(index) << 7) + 0xFC);
}

std::uint32_t occupiedRecordField(const std::uint8_t* self, std::uint32_t index) noexcept {
    const auto stateOffset = (index + 2u) << 7;
    if (load<std::uint32_t>(self, stateOffset) != 6u) {
        return 0u;
    }
    return load<std::uint32_t>(self, (static_cast<std::size_t>(index) << 7) + 0x178);
}

std::uint32_t setOccupiedRecordField(std::uint8_t* self, std::uint32_t index, std::uint32_t value) noexcept {
    const auto stateOffset = (index + 2u) << 7;
    if (load<std::uint32_t>(self, stateOffset) != 6u) {
        return index;
    }
    store<std::uint32_t>(self, (static_cast<std::size_t>(index) << 7) + 0x178, value);
    return index << 7;
}

std::uint32_t unlinkWideChain(std::uint8_t* self, std::uint8_t* node, std::uint8_t*& gameInfo) noexcept {
    if (gameInfo == node) {
        gameInfo = nullptr;
    }
    auto* head = ptr(load<std::uint32_t>(self, 0x28));
    if (node == nullptr || head == nullptr) {
        return bits(head);
    }
    if (head == node) {
        auto* next = wideNext(head);
        store<std::uint32_t>(self, 0x28, bits(next));
        return bits(next);
    }
    auto* prev = head;
    if (wideNext(prev) != nullptr) {
        for (;;) {
            auto* current = wideNext(prev);
            if (current == node) {
                break;
            }
            prev = current;
            if (wideNext(prev) == nullptr) {
                break;
            }
        }
    }
    if (wideNext(prev) == node) {
        store<std::uint32_t>(prev, 0x3FC, bits(wideNext(node)));
    }
    return bits(prev);
}

std::uint32_t unlinkShortChain(std::uint8_t* self, std::uint8_t* node) noexcept {
    if (node == nullptr) {
        return load<std::uint32_t>(self, 0x24);
    }
    auto* head = ptr(load<std::uint32_t>(self, 0x24));
    if (head == node) {
        auto* next = shortNext(head);
        store<std::uint32_t>(self, 0x24, bits(next));
        store<std::uint32_t>(node, 0x10, 0u);
        return bits(next);
    }
    auto* prev = head;
    if (shortNext(prev) != nullptr) {
        for (;;) {
            auto* current = shortNext(prev);
            if (current == node) {
                break;
            }
            prev = current;
            if (shortNext(prev) == nullptr) {
                break;
            }
        }
    }
    if (shortNext(prev) == node) {
        store<std::uint32_t>(prev, 0x10, bits(shortNext(node)));
    }
    store<std::uint32_t>(node, 0x10, 0u);
    return bits(prev);
}

std::uint32_t initTaggedPointer(std::uint8_t* self, std::uint8_t* value) noexcept {
    store<std::uint32_t>(self, 0, kNatBufferVtable);
    store<std::uint32_t>(self, 4, bits(value));
    return bits(self);
}

std::uint32_t stateIsSixOrSeven(const std::uint8_t* self) noexcept {
    const auto state = load<std::uint32_t>(self, 0x5C);
    return (state == 6u || state == 7u) ? 1u : 0u;
}

std::uint32_t attachAndRetain(std::uint8_t* self, std::uint8_t* object) noexcept {
    const auto header = load<std::uint32_t>(object, 0);
    store<std::uint32_t>(self, 4, 0u);
    store<std::uint32_t>(self, 8, 0u);
    store<std::uint32_t>(self, 0, header);
    store<std::uint32_t>(ptr(header), 0x10, load<std::uint32_t>(ptr(header), 0x10) + 1u);
    return bits(self);
}

void initFloatExtents(std::uint8_t* self) noexcept {
    store<std::uint32_t>(self, 0, 0u);
    store<std::uint32_t>(self, 4, 0u);
    for (std::uint32_t offset = 8; offset <= 0x20; offset += 4) {
        store<std::uint32_t>(self, offset, kFloatMaxBits);
    }
}

std::uint32_t otherSlotSharesId(const std::uint8_t* self, std::uint32_t id, std::uint32_t skip) noexcept {
    for (std::uint32_t index = 0; index < 6u; ++index) {
        auto* slot = ptr(load<std::uint32_t>(self, 4u + index * 4u));
        if (slot != nullptr && load<std::uint32_t>(slot, 0xC) == id && index != skip) {
            return 1u;
        }
    }
    return 0u;
}

std::uint32_t slotStateIsAi(const std::uint8_t* self) noexcept {
    const auto state = load<std::uint32_t>(self, 4);
    return (state == 2u || state == 3u || state == 4u || state == 5u) ? 1u : 0u;
}

}  // namespace ra3::net
