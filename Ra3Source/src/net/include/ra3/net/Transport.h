#pragma once

#include <cstddef>
#include <cstdint>

namespace ra3::net {

struct ByteQueue {
    std::uint8_t* data;
    std::uint32_t capacity;
    std::uint32_t readOffset;
    std::uint32_t writeOffset;
};

struct TimeBuckets {
    float* values;
    std::uint32_t count;
    std::uint32_t interval;
    std::uint32_t epoch;
};

struct Endpoint {
    std::uint32_t address;
    std::uint16_t port;
    std::uint16_t padding;
};

void clearPairPointers(void* object) noexcept;
void* linkedNodeAt(void* object, int index) noexcept;
void unlinkNode(void* object, void* node) noexcept;

char* copyBounded(char* destination, const char* source, char* end) noexcept;
void* matrixCell(void* object, std::uint32_t row, std::uint32_t column) noexcept;
void* resetConnectionMatrix(void* object) noexcept;
bool rowHasConnectedPeer(const void* object, std::uint16_t row) noexcept;
bool connectionMatrixReady(const void* object, bool requireAll) noexcept;
bool connectionPairReady(
    const void* object, std::uint16_t first, std::uint16_t second) noexcept;
std::uint32_t connectionValue(
    const void* object, std::uint16_t first, std::uint16_t second) noexcept;

std::uint32_t elapsedPastTimeout(
    bool active, bool stopped, std::uint32_t started, std::uint32_t now,
    std::uint32_t timeout = 5000) noexcept;
std::uint32_t addLatencySample(void* peer, std::uint32_t sample) noexcept;

void compactAndAppend(ByteQueue& queue, const void* data, std::uint32_t size,
                      std::uint8_t* replacement) noexcept;
std::uint32_t peekFramedSize(const ByteQueue& queue) noexcept;
std::uint32_t popFramed(ByteQueue& queue, void* destination, std::uint32_t capacity) noexcept;
void appendFramed(ByteQueue& queue, const void* data, std::uint32_t size,
                  std::uint8_t* replacement) noexcept;

std::uint32_t packetHeaderSize(const void* packet) noexcept;
std::int32_t activePeerPendingTotal(const void* transport) noexcept;
void* copyTransportRecord(void* destination, std::uint32_t tag, const void* record) noexcept;
std::uint32_t makeConnectionCode(int family, int index, int side) noexcept;

void initializeTimeBuckets(
    TimeBuckets& buckets, std::uint32_t interval, std::uint32_t duration,
    float* storage, std::uint32_t now) noexcept;
void resetTimeBuckets(TimeBuckets& buckets, std::uint32_t now) noexcept;
void addTimeBucketValue(TimeBuckets& buckets, float value, std::uint32_t now) noexcept;
float sumTimeBuckets(const TimeBuckets& buckets) noexcept;

void setOpaqueValue(void* destination, std::uint32_t value) noexcept;
void scramblePacketWords(void* data, std::int32_t byteCount) noexcept;

void clearEndpointSlot(void* owner, std::uint16_t slot) noexcept;
void setEndpointSlot(
    void* owner, std::uint16_t slot, std::uint32_t handle, const Endpoint& endpoint) noexcept;
std::uint32_t takeEndpointSlot(
    void* owner, std::uint32_t slot, Endpoint& endpoint) noexcept;
void updateEndpointSlot(void* owner, std::uint16_t slot, const Endpoint& endpoint) noexcept;

std::int32_t countNonzeroBytes(const std::uint8_t* begin, const std::uint8_t* end) noexcept;
std::int32_t minimumCombinedMagnitude(
    const std::int32_t* firstBegin, const std::int32_t* firstEnd,
    const std::int32_t* secondBegin, const std::int32_t* secondEnd) noexcept;

}  // namespace ra3::net
