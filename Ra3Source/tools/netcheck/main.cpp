#include <ra3/net/Transport.h>

#include <cstdint>
#include <cstring>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>
#include <type_traits>
#include <vector>

namespace {

std::vector<std::uint8_t> decode(const std::string& text)
{
    std::vector<std::uint8_t> result;
    if (text == ".") {
        return result;
    }
    for (std::size_t i = 0; i + 1 < text.size(); i += 2) {
        result.push_back(static_cast<std::uint8_t>(std::stoul(text.substr(i, 2), nullptr, 16)));
    }
    return result;
}

void encode(const void* memory, std::size_t size)
{
    const auto* bytes = static_cast<const std::uint8_t*>(memory);
    std::cout << std::hex << std::setfill('0');
    for (std::size_t i = 0; i < size; ++i) {
        std::cout << std::setw(2) << static_cast<unsigned>(bytes[i]);
    }
    std::cout << std::dec;
}

template <typename T>
T number(std::istringstream& input)
{
    std::string token;
    input >> token;
    if constexpr (std::is_signed_v<T>) {
        return static_cast<T>(std::stoll(token));
    } else {
        return static_cast<T>(std::stoull(token));
    }
}

void run(const std::string& line)
{
    std::istringstream input(line);
    std::string operation;
    std::string hex;
    input >> operation >> hex;
    auto bytes = decode(hex);
    std::uint32_t result = 0;

    if (operation == "clear") {
        ra3::net::clearPairPointers(bytes.data());
    } else if (operation == "cell") {
        const auto row = number<std::uint32_t>(input);
        const auto column = number<std::uint32_t>(input);
        const void* cell = ra3::net::matrixCell(bytes.data(), row, column);
        result = cell == nullptr ? 0xFFFFFFFFu
                                 : static_cast<std::uint32_t>(
                                     static_cast<const std::uint8_t*>(cell) - bytes.data());
    } else if (operation == "latency") {
        result = ra3::net::addLatencySample(bytes.data(), number<std::uint32_t>(input));
    } else if (operation == "opaque") {
        ra3::net::setOpaqueValue(bytes.data(), number<std::uint32_t>(input));
    } else if (operation == "header") {
        result = ra3::net::packetHeaderSize(bytes.data());
    } else if (operation == "active") {
        result = static_cast<std::uint32_t>(ra3::net::activePeerPendingTotal(bytes.data()));
    } else if (operation == "record") {
        const auto tag = number<std::uint32_t>(input);
        std::string recordHex;
        input >> recordHex;
        const auto record = decode(recordHex);
        result = static_cast<std::uint32_t>(
            static_cast<std::uint8_t*>(
                ra3::net::copyTransportRecord(bytes.data(), tag, record.data())) - bytes.data());
    } else if (operation == "code") {
        const int family = number<int>(input);
        const int index = number<int>(input);
        const int side = number<int>(input);
        result = ra3::net::makeConnectionCode(family, index, side);
    } else if (operation == "scramble") {
        ra3::net::scramblePacketWords(bytes.data(), number<std::int32_t>(input));
    } else if (operation == "resetmatrix") {
        result = static_cast<std::uint32_t>(
            static_cast<std::uint8_t*>(ra3::net::resetConnectionMatrix(bytes.data())) - bytes.data());
    } else if (operation == "rowstate") {
        result = ra3::net::rowHasConnectedPeer(bytes.data(), number<std::uint16_t>(input));
    } else if (operation == "matrixvalue") {
        const auto first = number<std::uint16_t>(input);
        const auto second = number<std::uint16_t>(input);
        result = ra3::net::connectionValue(bytes.data(), first, second);
    } else if (operation == "timeout") {
        const bool active = number<unsigned>(input) != 0;
        const bool stopped = number<unsigned>(input) != 0;
        const auto started = number<std::uint32_t>(input);
        const auto now = number<std::uint32_t>(input);
        const auto timeout = number<std::uint32_t>(input);
        result = ra3::net::elapsedPastTimeout(
            active, stopped, started, now, timeout);
    } else if (operation == "endpointclear") {
        const auto slot = number<std::uint16_t>(input);
        std::vector<std::uint8_t> owner(0x40E48, 0);
        std::memcpy(owner.data() + 0x40E00, bytes.data(), bytes.size());
        ra3::net::clearEndpointSlot(owner.data(), slot);
        std::memcpy(bytes.data(), owner.data() + 0x40E00, bytes.size());
    } else if (operation == "endpointset" || operation == "endpointupdate") {
        const auto slot = number<std::uint16_t>(input);
        const auto handle = number<std::uint32_t>(input);
        ra3::net::Endpoint endpoint{
            number<std::uint32_t>(input), number<std::uint16_t>(input),
            number<std::uint16_t>(input)};
        std::vector<std::uint8_t> owner(0x40E48, 0);
        std::memcpy(owner.data() + 0x40E00, bytes.data(), bytes.size());
        if (operation == "endpointset") {
            ra3::net::setEndpointSlot(owner.data(), slot, handle, endpoint);
        } else {
            ra3::net::updateEndpointSlot(owner.data(), slot, endpoint);
        }
        std::memcpy(bytes.data(), owner.data() + 0x40E00, bytes.size());
    } else if (operation == "endpointtake") {
        const auto slot = number<std::uint32_t>(input);
        std::vector<std::uint8_t> owner(0x40E48, 0);
        std::memcpy(owner.data() + 0x40E00, bytes.data(), bytes.size());
        ra3::net::Endpoint endpoint{0xA5A5A5A5u, 0xA5A5u, 0xA5A5u};
        result = ra3::net::takeEndpointSlot(owner.data(), slot, endpoint);
        std::cout << result << ' ' << endpoint.address << ' ' << endpoint.port << ' '
                  << endpoint.padding << ' ';
        std::memcpy(bytes.data(), owner.data() + 0x40E00, bytes.size());
        encode(bytes.data(), bytes.size());
        std::cout << '\n';
        return;
    } else if (operation == "copybounded") {
        std::string source;
        std::size_t end = 0;
        input >> source >> end;
        const auto sourceBytes = decode(source);
        char* endPointer = end == static_cast<std::size_t>(-1)
            ? nullptr : reinterpret_cast<char*>(bytes.data() + end);
        result = static_cast<std::uint32_t>(
            ra3::net::copyBounded(reinterpret_cast<char*>(bytes.data()),
                                  reinterpret_cast<const char*>(sourceBytes.data()), endPointer)
            - reinterpret_cast<char*>(bytes.data()));
    } else if (operation == "framedpop") {
        const std::uint32_t read = number<std::uint32_t>(input);
        const std::uint32_t write = number<std::uint32_t>(input);
        const std::uint32_t capacity = number<std::uint32_t>(input);
        std::vector<std::uint8_t> output(capacity == 0 ? 1u : capacity, 0xA5);
        ra3::net::ByteQueue queue{bytes.data(), static_cast<std::uint32_t>(bytes.size()), read, write};
        result = ra3::net::popFramed(queue, output.data(), capacity);
        std::cout << result << ' ' << queue.readOffset << ' ';
        encode(output.data(), output.size());
        std::cout << ' ';
        encode(bytes.data(), bytes.size());
        std::cout << '\n';
        return;
    } else if (operation == "framedappend" || operation == "rawappend") {
        const std::uint32_t capacity = number<std::uint32_t>(input);
        const std::uint32_t read = number<std::uint32_t>(input);
        const std::uint32_t write = number<std::uint32_t>(input);
        std::string dataHex;
        input >> dataHex;
        const auto data = decode(dataHex);
        std::vector<std::uint8_t> replacement(write + data.size() + 2u, 0xCC);
        ra3::net::ByteQueue queue{bytes.data(), capacity, read, write};
        if (operation == "framedappend") {
            ra3::net::appendFramed(queue, data.data(), static_cast<std::uint32_t>(data.size()),
                                   replacement.data());
        } else {
            ra3::net::compactAndAppend(queue, data.data(), static_cast<std::uint32_t>(data.size()),
                                      replacement.data());
        }
        std::cout << queue.capacity << ' ' << queue.readOffset << ' ' << queue.writeOffset << ' ';
        encode(queue.data, queue.writeOffset);
        std::cout << '\n';
        return;
    } else if (operation == "sum") {
        const auto values = reinterpret_cast<const float*>(bytes.data());
        ra3::net::TimeBuckets buckets{
            const_cast<float*>(values), static_cast<std::uint32_t>(bytes.size() / sizeof(float)), 1, 0};
        float value = ra3::net::sumTimeBuckets(buckets);
        std::memcpy(&result, &value, sizeof(result));
    } else if (operation == "bucketreset" || operation == "bucketadd") {
        const auto count = number<std::uint32_t>(input);
        const auto interval = number<std::uint32_t>(input);
        const auto epoch = number<std::uint32_t>(input);
        const auto now = number<std::uint32_t>(input);
        const auto valueBits = number<std::uint32_t>(input);
        float value = 0.0f;
        std::memcpy(&value, &valueBits, sizeof(value));
        ra3::net::TimeBuckets buckets{
            reinterpret_cast<float*>(bytes.data()), count, interval, epoch};
        if (operation == "bucketreset") {
            ra3::net::resetTimeBuckets(buckets, now);
        } else {
            ra3::net::addTimeBucketValue(buckets, value, now);
        }
        std::cout << buckets.epoch << ' ';
        encode(bytes.data(), bytes.size());
        std::cout << '\n';
        return;
    } else {
        std::cout << "error\n";
        return;
    }

    std::cout << result << ' ';
    encode(bytes.data(), bytes.size());
    std::cout << '\n';
}

}  // namespace

int main()
{
    std::string line;
    while (std::getline(std::cin, line)) {
        if (line == "quit") {
            break;
        }
        try {
            run(line);
        } catch (...) {
            std::cout << "error\n";
        }
        std::cout.flush();
    }
    return 0;
}
