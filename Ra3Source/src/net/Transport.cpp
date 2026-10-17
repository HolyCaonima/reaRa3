#include <ra3/net/Transport.h>

#include <algorithm>
#include <cmath>
#include <cstring>

namespace ra3::net {
namespace {

template <typename T>
T load(const void* object, std::size_t offset) noexcept
{
    T value{};
    std::memcpy(&value, static_cast<const std::uint8_t*>(object) + offset, sizeof(value));
    return value;
}

template <typename T>
void store(void* object, std::size_t offset, T value) noexcept
{
    std::memcpy(static_cast<std::uint8_t*>(object) + offset, &value, sizeof(value));
}

bool validPeerObject(const void* matrix, std::uint32_t index) noexcept
{
    const auto table = load<const std::uint8_t*>(matrix, 0x4FC);
    if (table == nullptr) {
        return false;
    }
    const auto peer = load<const std::uint8_t*>(table, index * 4u);
    return peer != nullptr && load<std::int32_t>(peer, 4) == 6;
}

void advanceBuckets(TimeBuckets& buckets, std::uint32_t now) noexcept
{
    const std::uint32_t elapsed = now >= buckets.epoch ? now - buckets.epoch
                                                       : now - buckets.epoch - 1u;
    if (buckets.count == 0 || buckets.interval == 0) {
        return;
    }
    std::uint32_t slot = (buckets.epoch / buckets.interval) % buckets.count;
    std::uint32_t remaining = elapsed;
    while (remaining >= buckets.interval) {
        slot = (slot + 1u) % buckets.count;
        buckets.values[slot] = 0.0f;
        buckets.epoch += buckets.interval;
        remaining -= buckets.interval;
    }
}

}  // namespace

// ra3_1.0.game 0x005D6000.
void clearPairPointers(void* object) noexcept
{
    store<std::uint32_t>(object, 0x38, 0);
    store<std::uint32_t>(object, 0x3C, 0);
}

// ra3_1.0.game 0x005D6010.
void* linkedNodeAt(void* object, int index) noexcept
{
    if (index < 0) {
        return nullptr;
    }
    auto node = load<std::uint8_t*>(object, 0x28);
    while (index-- != 0 && node != nullptr) {
        node = load<std::uint8_t*>(node, 0x3FC);
    }
    return node;
}

// ra3_1.0.game 0x005D6040.
void unlinkNode(void* object, void* nodeValue) noexcept
{
    if (nodeValue == nullptr) {
        return;
    }
    auto node = static_cast<std::uint8_t*>(nodeValue);
    auto current = load<std::uint8_t*>(object, 0x28);
    if (current == nullptr) {
        return;
    }
    if (current == node) {
        store(object, 0x28, load<std::uint8_t*>(node, 0x3FC));
        return;
    }
    while (load<std::uint8_t*>(current, 0x3FC) != nullptr
           && load<std::uint8_t*>(current, 0x3FC) != node) {
        current = load<std::uint8_t*>(current, 0x3FC);
    }
    if (load<std::uint8_t*>(current, 0x3FC) == node) {
        store(current, 0x3FC, load<std::uint8_t*>(node, 0x3FC));
    }
}

char* copyBounded(char* destination, const char* source, char* end) noexcept
{
    // ra3_1.0.game 0x005D64C0.
    if (end == nullptr) {
        std::strcpy(destination, source);
        return destination + std::strlen(source) + 1;
    }
    if (end <= destination) {
        return destination;
    }
    const std::size_t room = static_cast<std::size_t>(end - destination);
    std::strncpy(destination, source, room);
    const std::size_t length = std::strlen(source);
    if (room <= length) {
        end[-1] = '\0';
        return end;
    }
    return destination + length + 1;
}

// ra3_1.0.game 0x005D69C5.
void* matrixCell(void* object, std::uint32_t row, std::uint32_t column) noexcept
{
    if (row >= 7 || column >= 7) {
        return nullptr;
    }
    return static_cast<std::uint8_t*>(object) + 0x13C + (column + row * 6u) * 0x14u;
}

// ra3_1.0.game 0x005D6A00.
void* resetConnectionMatrix(void* object) noexcept
{
    for (std::uint32_t row = 0; row < 6; ++row) {
        for (std::uint32_t column = 0; column < 6; ++column) {
            const std::size_t cell = (row * 6u + column) * 4u;
            store<std::uint32_t>(object, 0x1C + cell, 0);
            store<std::uint32_t>(object, 0xAC + cell, 0);
            store<std::uint32_t>(object, 0x424 + cell, 0);
            store<std::uint16_t>(object, 0x4B4 + (row * 6u + column) * 2u, 0);
        }
        store<std::uint32_t>(object, 0x40C + row * 4u, 0);
    }
    store<std::uint32_t>(object, 0x4FC, 0);
    store<std::uint16_t>(object, 0x18, 6);
    return static_cast<std::uint8_t*>(object) + 0x13C;
}

// ra3_1.0.game 0x005D6A70.
bool rowHasConnectedPeer(const void* object, std::uint16_t row) noexcept
{
    if (row >= 6) {
        return false;
    }
    for (std::uint32_t column = 0; column < 6; ++column) {
        if ((column != row && load<std::int32_t>(object, 0x1C + (column + row * 6u) * 4u) == 2)
            || load<std::int32_t>(object, 0x1C + (row + column * 6u) * 4u) == 2) {
            return true;
        }
    }
    return false;
}

// ra3_1.0.game 0x005D6AC0.
bool connectionMatrixReady(const void* object, bool requireAll) noexcept
{
    const std::uint16_t local = load<std::uint16_t>(object, 0x18);
    if (local >= 6) {
        return true;
    }
    if (!requireAll) {
        for (std::uint32_t peer = 0; peer < 6; ++peer) {
            if (peer == local || !validPeerObject(object, peer)) {
                continue;
            }
            const auto a = load<std::int32_t>(object, 0x1C + (peer + local * 6u) * 4u);
            const auto b = load<std::int32_t>(object, 0x1C + (local + peer * 6u) * 4u);
            if (!(a == 3 && b == 3)) {
                return false;
            }
        }
        return true;
    }
    for (std::uint32_t first = 0; first < 6; ++first) {
        for (std::uint32_t second = 0; second < 6; ++second) {
            if (first != second && validPeerObject(object, first) && validPeerObject(object, second)
                && load<std::int32_t>(object, 0x1C + (first * 6u + second) * 4u) != 3) {
                return false;
            }
        }
    }
    return true;
}

// ra3_1.0.game 0x005D6B90.
bool connectionPairReady(
    const void* object, std::uint16_t first, std::uint16_t second) noexcept
{
    if (first >= 6 || second >= 6 || first == second) {
        return first < 6 && second < 6;
    }
    return !(validPeerObject(object, first) && validPeerObject(object, second)
             && load<std::int32_t>(object, 0x1C + (second + first * 6u) * 4u) != 3);
}

// ra3_1.0.game 0x005D6C00.
std::uint32_t connectionValue(
    const void* object, std::uint16_t first, std::uint16_t second) noexcept
{
    return first < 6 && second < 6
        ? load<std::uint32_t>(object, 0xAC + (second + first * 6u) * 4u)
        : 0;
}

// ra3_1.0.game 0x005D6E50 and 0x005D7250.
std::uint32_t elapsedPastTimeout(
    bool active, bool stopped, std::uint32_t started, std::uint32_t now,
    std::uint32_t timeout) noexcept
{
    if (!active || stopped) {
        return 0;
    }
    std::uint32_t elapsed = 0;
    if (now < started) {
        const std::uint32_t forward = started - now;
        if (forward < 0x80000000u) {
            return 0;
        }
        elapsed = now - started;
    } else {
        elapsed = now - started;
        if (elapsed > 0x7FFFFFFFu) {
            return 0;
        }
    }
    return elapsed > timeout ? elapsed - timeout : 0;
}

// ra3_1.0.game 0x005D6EA0.
std::uint32_t addLatencySample(void* peer, std::uint32_t sample) noexcept
{
    std::uint32_t index = load<std::uint32_t>(peer, 0x240);
    std::uint32_t total = load<std::uint32_t>(peer, 0x244);
    total -= load<std::uint32_t>(peer, 0x40 + index * 4u);
    sample = std::min(sample, 500u);
    store(peer, 0x40 + index * 4u, sample);
    store(peer, 0x244, total + sample);
    const std::uint32_t next = index + 1u;
    store(peer, 0x240, next == 0x80u ? 0u : next);
    return next;
}

void compactAndAppend(ByteQueue& queue, const void* input, std::uint32_t size,
                      std::uint8_t* replacement) noexcept
{
    // ra3_1.0.game 0x005D6FA0; allocation itself is supplied by the caller.
    if (queue.readOffset != 0) {
        std::memmove(queue.data, queue.data + queue.readOffset, queue.writeOffset - queue.readOffset);
        queue.writeOffset -= queue.readOffset;
        queue.readOffset = 0;
    }
    if (queue.capacity < queue.writeOffset + size) {
        std::memcpy(replacement, queue.data, queue.writeOffset);
        queue.data = replacement;
        queue.capacity = queue.writeOffset + size;
    }
    std::memcpy(queue.data + queue.writeOffset, input, size);
    queue.writeOffset += size;
}

std::uint32_t peekFramedSize(const ByteQueue& queue) noexcept
{
    if (queue.readOffset == queue.writeOffset) {
        return 0;
    }
    const std::uint8_t first = queue.data[queue.readOffset];
    if ((first & 0x80u) == 0) {
        return first;
    }
    if (queue.readOffset + 1u == queue.writeOffset) {
        return 0;
    }
    return (static_cast<std::uint32_t>(first & 0x7Fu) << 8u)
        | queue.data[queue.readOffset + 1u];
}

// ra3_1.0.game 0x005D7020.
std::uint32_t popFramed(ByteQueue& queue, void* destination, std::uint32_t capacity) noexcept
{
    const std::uint32_t size = peekFramedSize(queue);
    if (size == 0 && queue.readOffset == queue.writeOffset) {
        return 0;
    }
    const std::uint32_t prefix = size < 0x80u ? 1u : 2u;
    const std::uint32_t end = queue.readOffset + prefix + size;
    if (end > queue.writeOffset) {
        return 0;
    }
    if (size <= capacity) {
        std::memcpy(destination, queue.data + queue.readOffset + prefix, size);
        queue.readOffset = end;
    }
    return size;
}

// ra3_1.0.game 0x005D70A0.
void appendFramed(ByteQueue& queue, const void* input, std::uint32_t size,
                  std::uint8_t* replacement) noexcept
{
    const std::uint32_t prefix = size < 0x80u ? 1u : 2u;
    const std::uint32_t needed = queue.writeOffset + prefix + size;
    if (queue.capacity < needed) {
        std::memcpy(replacement, queue.data, queue.writeOffset);
        queue.data = replacement;
        queue.capacity = needed;
    }
    if (prefix == 1) {
        queue.data[queue.writeOffset++] = static_cast<std::uint8_t>(size);
    } else {
        queue.data[queue.writeOffset++] = static_cast<std::uint8_t>((size >> 8u) | 0x80u);
        queue.data[queue.writeOffset++] = static_cast<std::uint8_t>(size);
    }
    std::memcpy(queue.data + queue.writeOffset, input, size);
    queue.writeOffset += size;
}

// ra3_1.0.game 0x005D72A0.
std::uint32_t packetHeaderSize(const void* packet) noexcept
{
    auto cursor = static_cast<const std::uint8_t*>(packet) + 10;
    std::uint32_t count = load<std::uint16_t>(packet, 8) >> 12u;
    while (count-- != 0) {
        cursor += (*cursor & 0x80u) != 0 ? 2 : 1;
    }
    return static_cast<std::uint32_t>(cursor - static_cast<const std::uint8_t*>(packet));
}

// ra3_1.0.game 0x005D7500.
std::int32_t activePeerPendingTotal(const void* transport) noexcept
{
    std::int32_t total = 0;
    for (std::size_t base = 0x158; base <= 0x1418; base += 0x3A0) {
        if (load<std::uint8_t>(transport, base + 0x398) != 0
            && load<std::uint8_t>(transport, base + 0x399) == 0) {
            total += load<std::int32_t>(transport, base + 0x9C);
        }
    }
    return total;
}

// ra3_1.0.game 0x005D75E0.
void* copyTransportRecord(void* destination, std::uint32_t tag, const void* record) noexcept
{
    store(destination, 0, tag);
    std::memcpy(static_cast<std::uint8_t*>(destination) + 4, record, 14u * 4u);
    return destination;
}

// ra3_1.0.game 0x005D7640.
std::uint32_t makeConnectionCode(int family, int index, int side) noexcept
{
    const std::uint8_t first = family == 0 ? 'A' : (family == 1 ? 'S' : 'J');
    const std::uint8_t last = side == 0 ? 'E' : (side == 1 ? 'M' : 'H');
    const int value = index + 1;
    return (static_cast<std::uint32_t>(first) << 24u)
        | (static_cast<std::uint32_t>('0' + value / 10) << 16u)
        | (static_cast<std::uint32_t>('0' + value % 10) << 8u)
        | static_cast<std::uint32_t>(last);
}

// ra3_1.0.game 0x005D77E0.
void initializeTimeBuckets(
    TimeBuckets& buckets, std::uint32_t interval, std::uint32_t duration,
    float* storage, std::uint32_t now) noexcept
{
    buckets.values = storage;
    buckets.interval = interval;
    buckets.count = duration / interval;
    buckets.epoch = now;
    if (buckets.count == 0) {
        buckets.count = 1;
    }
    std::fill_n(buckets.values, buckets.count, 0.0f);
}

// ra3_1.0.game 0x005D7850.
void resetTimeBuckets(TimeBuckets& buckets, std::uint32_t now) noexcept
{
    buckets.epoch = now;
    std::fill_n(buckets.values, buckets.count, 0.0f);
}

// ra3_1.0.game 0x005D7880.
void addTimeBucketValue(TimeBuckets& buckets, float value, std::uint32_t now) noexcept
{
    advanceBuckets(buckets, now);
    const std::uint32_t slot = (buckets.epoch / buckets.interval) % buckets.count;
    buckets.values[slot] += value;
}

// ra3_1.0.game 0x005D78F0.
float sumTimeBuckets(const TimeBuckets& buckets) noexcept
{
    float result = 0.0f;
    std::uint32_t i = 0;
    for (; i + 4u <= buckets.count; i += 4u) {
        result = buckets.values[i] + result + buckets.values[i + 1u]
            + buckets.values[i + 2u] + buckets.values[i + 3u];
    }
    for (; i < buckets.count; ++i) {
        result += buckets.values[i];
    }
    return result;
}

// ra3_1.0.game 0x005D77D0.
void setOpaqueValue(void* destination, std::uint32_t value) noexcept
{
    store(destination, 0, value);
}

// ra3_1.0.game 0x005D7AE0.
void scramblePacketWords(void* data, std::int32_t byteCount) noexcept
{
    auto words = static_cast<std::uint32_t*>(data);
    std::uint32_t key = 0x38D9B7D4u;
    const std::int32_t count = (byteCount + (byteCount < 0 ? 3 : 0)) / 4;
    for (std::int32_t i = 0; i < count; ++i) {
        const std::uint32_t value = words[i] ^ key;
        words[i] = (value << 24u) | ((value & 0x0000FF00u) << 8u)
            | ((value & 0x00FF0000u) >> 8u) | (value >> 24u);
        key += 0x80C63AF2u;
    }
}

// ra3_1.0.game 0x005D7EA0.
void clearEndpointSlot(void* owner, std::uint16_t slot) noexcept
{
    if (slot >= 6) {
        return;
    }
    if (load<std::uint32_t>(owner, 0x40E00 + slot * 12u) == 0) {
        return;
    }
    store<std::uint32_t>(owner, 0x40E00 + slot * 12u, 0);
    store<std::uint32_t>(owner, 0x40E04 + slot * 12u, 0);
    store<std::uint16_t>(owner, 0x40E08 + slot * 12u, 0);
}

// ra3_1.0.game 0x005D7F20.
void setEndpointSlot(
    void* owner, std::uint16_t slot, std::uint32_t handle, const Endpoint& endpoint) noexcept
{
    if (slot >= 6) {
        return;
    }
    clearEndpointSlot(owner, slot);
    store(owner, 0x40E00 + slot * 12u, handle);
    std::memcpy(static_cast<std::uint8_t*>(owner) + 0x40E04 + slot * 12u, &endpoint, 8);
}

// ra3_1.0.game 0x005D7F70.
std::uint32_t takeEndpointSlot(
    void* owner, std::uint32_t slot, Endpoint& endpoint) noexcept
{
    if (slot >= 6) {
        return 0;
    }
    const std::uint32_t handle = load<std::uint32_t>(owner, 0x40E00 + slot * 12u);
    std::memcpy(&endpoint, static_cast<std::uint8_t*>(owner) + 0x40E04 + slot * 12u, 8);
    store<std::uint32_t>(owner, 0x40E00 + slot * 12u, 0);
    return handle;
}

// ra3_1.0.game 0x005D7FC0.
void updateEndpointSlot(void* owner, std::uint16_t slot, const Endpoint& endpoint) noexcept
{
    if (slot < 6) {
        std::memcpy(static_cast<std::uint8_t*>(owner) + 0x40E04 + slot * 12u, &endpoint, 8);
    }
}

// ra3_1.0.game 0x005D88E0.
std::int32_t countNonzeroBytes(const std::uint8_t* begin, const std::uint8_t* end) noexcept
{
    std::int32_t result = 0;
    while (begin != end) {
        result += *begin++ != 0 ? 1 : 0;
    }
    return result;
}

// ra3_1.0.game 0x005D8910.
std::int32_t minimumCombinedMagnitude(
    const std::int32_t* firstBegin, const std::int32_t* firstEnd,
    const std::int32_t* secondBegin, const std::int32_t* secondEnd) noexcept
{
    if (firstBegin == nullptr || secondBegin == nullptr
        || firstEnd - firstBegin != secondEnd - secondBegin) {
        return 0x1FC02;
    }
    std::int32_t minimum = 0x1FC02;
    while (firstBegin != firstEnd) {
        const std::int32_t sum = *firstBegin * *firstBegin + *secondBegin * *secondBegin;
        minimum = std::min(minimum, sum);
        ++firstBegin;
        ++secondBegin;
    }
    return static_cast<std::int32_t>(std::sqrt(static_cast<float>(minimum)));
}

}  // namespace ra3::net
