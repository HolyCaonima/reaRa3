#include <ra3/io/RefPack.h>

#include <algorithm>
#include <array>
#include <cstring>
#include <limits>
#include <string>

namespace ra3::io {
namespace {

std::uint32_t readBigEndian(const std::uint8_t* p, std::size_t bytes)
{
    std::uint32_t value = 0;
    for (std::size_t i = 0; i < bytes; ++i) {
        value = (value << 8) | p[i];
    }
    return value;
}

struct Command {
    std::size_t encodedSize = 1;
    std::size_t literal = 0;
    std::size_t length = 0;
    std::size_t distance = 0;
    bool stop = false;
};

Command decodeCommand(const std::uint8_t* c)
{
    Command command;
    const std::uint8_t b0 = c[0];
    if (b0 < 0x80) {
        command.encodedSize = 2;
        command.literal = b0 & 0x03;
        command.length = ((b0 & 0x1C) >> 2) + 3;
        command.distance = ((b0 & 0x60) << 3) + c[1] + 1;
    } else if (b0 < 0xC0) {
        command.encodedSize = 3;
        command.literal = (c[1] >> 6) & 0x03;
        command.length = (b0 & 0x3F) + 4;
        command.distance = ((c[1] & 0x3F) << 8) + c[2] + 1;
    } else if (b0 < 0xE0) {
        command.encodedSize = 4;
        command.literal = b0 & 0x03;
        command.length = ((b0 & 0x0C) << 6) + c[3] + 5;
        command.distance = ((b0 & 0x10) << 12) + (c[1] << 8) + c[2] + 1;
    } else if (b0 < 0xFC) {
        command.literal = ((b0 & 0x1F) << 2) + 4;
    } else {
        command.literal = b0 & 0x03;
        command.stop = true;
    }
    return command;
}

std::size_t commandSize(std::uint8_t b0)
{
    return b0 < 0x80 ? 2 : (b0 < 0xC0 ? 3 : (b0 < 0xE0 ? 4 : 1));
}

// With no `prefix`, the whole stream is decoded and checked strictly. With a
// prefix length, decoding stops once that many bytes exist, so the input may
// end anywhere after the commands that produce them. Input bytes are touched
// only as the commands consume them.
Result<std::vector<std::uint8_t>> inflate(
    const std::uint8_t* data, std::size_t size, std::optional<std::size_t> prefix)
{
    const auto header = readRefPackHeader(data, size);
    if (!header) {
        return makeError("not a RefPack stream");
    }
    const bool whole = !prefix;
    const std::size_t limit = header->decompressedSize;
    const std::size_t target = whole ? limit : std::min(*prefix, limit);
    std::vector<std::uint8_t> out;
    out.reserve(target);
    std::size_t in = header->headerSize;

    const auto fail = [&](const char* what) {
        return makeError(std::string("RefPack: ") + what + " at input offset " + std::to_string(in));
    };

    for (;;) {
        if (!whole && out.size() == target) {
            return out;
        }
        if (in >= size) {
            return fail(whole ? "stream ends without a stop command" : "truncated before requested prefix");
        }
        if (size - in < commandSize(data[in])) {
            return fail("truncated command");
        }
        const Command command = decodeCommand(data + in);
        in += command.encodedSize;

        if (whole && out.size() + command.literal + command.length > limit) {
            return fail("output exceeds declared size");
        }
        const std::size_t literalTaken = std::min(command.literal, target - out.size());
        if (size - in < literalTaken) {
            return fail("literal run past end of input");
        }
        out.insert(out.end(), data + in, data + in + literalTaken);
        in += literalTaken;

        const std::size_t lengthTaken = std::min(command.length, target - out.size());
        if (lengthTaken != 0) {
            if (command.distance > out.size()) {
                return fail("back-reference before start of output");
            }
            const std::size_t from = out.size() - command.distance;
            for (std::size_t i = 0; i < lengthTaken; ++i) {
                const std::uint8_t byte = out[from + i];
                out.push_back(byte);
            }
        }
        if (command.stop) {
            break;
        }
    }
    if (whole && in != size) {
        return fail("trailing bytes after stop command");
    }
    if (out.size() != target) {
        return fail(whole ? "output shorter than declared size" : "stream ends before requested prefix");
    }
    return out;
}

}  // namespace

std::optional<RefPackHeader> readRefPackHeader(const std::uint8_t* data, std::size_t size)
{
    if (size < 2 || (data[0] & 0x3E) != 0x10 || data[1] != 0xFB) {
        return std::nullopt;
    }
    RefPackHeader header;
    header.flags = data[0];
    const std::size_t sizeBytes = (header.flags & 0x80) ? 4 : 3;
    const std::size_t skip = (header.flags & 0x01) ? sizeBytes : 0;
    header.headerSize = 2 + skip + sizeBytes;
    if (size < header.headerSize) {
        return std::nullopt;
    }
    header.decompressedSize = readBigEndian(data + 2 + skip, sizeBytes);
    return header;
}

std::uint32_t probeRefPackSize(const std::uint8_t* data)
{
    if ((data[0] & 0x3E) != 0x10 || data[1] != 0xFB) {
        return 0;
    }
    return readBigEndian(data + 2, (data[0] & 0x80) ? 4 : 3);
}

Result<std::vector<std::uint8_t>> decompressRefPack(const std::uint8_t* data, std::size_t size)
{
    return inflate(data, size, std::nullopt);
}

Result<std::vector<std::uint8_t>> decompressRefPackPrefix(
    const std::uint8_t* data, std::size_t size, std::size_t maxOutput)
{
    return inflate(data, size, maxOutput);
}

struct DecodedStream::Impl {
    RangeReader source;
    std::uint64_t storedSize = 0;
    std::uint64_t decodedLength = 0;
    std::uint64_t produced = 0;
    std::uint64_t consumed = 0;
    std::uint64_t fetched = 0;
    std::size_t sourceChunkSize = 0;
    std::string name;
    bool isCompressed = false;
    bool done = false;
    bool stop = false;
    std::vector<std::uint8_t> input;
    std::size_t inputPosition = 0;
    std::vector<std::uint8_t> window;
    std::size_t windowPosition = 0;
    std::size_t literalRemaining = 0;
    std::size_t copyRemaining = 0;
    std::size_t copyDistance = 0;

    Error fail(const std::string& what) const
    {
        return makeError(name + ": " + what + " at compressed offset " +
                         std::to_string(consumed));
    }

    Result<bool> ensure(std::size_t count)
    {
        while (input.size() - inputPosition < count && fetched < storedSize) {
            if (inputPosition != 0) {
                input.erase(input.begin(), input.begin() + inputPosition);
                inputPosition = 0;
            }
            const std::size_t amount = static_cast<std::size_t>(
                std::min<std::uint64_t>(sourceChunkSize, storedSize - fetched));
            auto bytes = source(fetched, amount);
            if (!bytes) {
                return bytes.error();
            }
            if (bytes.value().size() != amount) {
                return fail("source returned a short range");
            }
            input.insert(input.end(), bytes.value().begin(), bytes.value().end());
            fetched += amount;
        }
        return input.size() - inputPosition >= count;
    }

    void consume(std::size_t count)
    {
        inputPosition += count;
        consumed += count;
    }

    Result<std::uint8_t> inputByte()
    {
        auto available = ensure(1);
        if (!available) return available.error();
        if (!available.value()) return fail("literal run passes end of input");
        const std::uint8_t value = input[inputPosition];
        consume(1);
        return value;
    }

    Error emitBlock(
        std::vector<std::uint8_t>& output, const std::uint8_t* bytes, std::size_t count)
    {
        if (count > decodedLength - produced) {
            return fail("output exceeds declared size");
        }
        output.insert(output.end(), bytes, bytes + count);
        const std::size_t first = std::min(count, window.size() - windowPosition);
        std::copy_n(bytes, first, window.begin() + windowPosition);
        if (count > first) {
            std::copy_n(bytes + first, count - first, window.begin());
        }
        windowPosition = (windowPosition + count) % window.size();
        produced += count;
        return {};
    }
};

DecodedStream::DecodedStream(std::unique_ptr<Impl> impl) : m_impl(std::move(impl)) {}
DecodedStream::DecodedStream(DecodedStream&&) noexcept = default;
DecodedStream& DecodedStream::operator=(DecodedStream&&) noexcept = default;
DecodedStream::~DecodedStream() = default;

Result<DecodedStream> DecodedStream::open(
    RangeReader source,
    std::uint64_t storedSize,
    std::string name,
    std::size_t sourceChunkSize)
{
    if (!source || sourceChunkSize == 0) {
        return makeError(name + ": invalid streaming source");
    }
    auto impl = std::make_unique<Impl>();
    impl->source = std::move(source);
    impl->storedSize = storedSize;
    impl->decodedLength = storedSize;
    impl->sourceChunkSize = sourceChunkSize;
    impl->name = std::move(name);

    auto available = impl->ensure(static_cast<std::size_t>(std::min<std::uint64_t>(10, storedSize)));
    if (!available) return available.error();
    const std::size_t present = impl->input.size() - impl->inputPosition;
    const std::uint8_t* first = present == 0 ? nullptr : impl->input.data() + impl->inputPosition;
    const bool signature =
        present >= 2 && (first[0] & 0x3E) == 0x10 && first[1] == 0xFB;
    const auto header = readRefPackHeader(first, present);
    if (signature && !header) {
        return makeError(impl->name + ": truncated RefPack header at compressed offset 0");
    }
    if (header) {
        impl->isCompressed = true;
        impl->decodedLength = header->decompressedSize;
        impl->window.resize((header->flags & 0x40) != 0 ? 16 * 1024 : 128 * 1024);
        impl->consume(header->headerSize);
    } else if (storedSize == 0) {
        impl->done = true;
    }
    return DecodedStream(std::move(impl));
}

Result<std::vector<std::uint8_t>> DecodedStream::read(std::size_t maxBytes)
{
    std::vector<std::uint8_t> output;
    if (maxBytes == 0 || m_impl->done) return output;
    output.reserve(static_cast<std::size_t>(
        std::min<std::uint64_t>(maxBytes, m_impl->decodedLength - m_impl->produced)));

    if (!m_impl->isCompressed) {
        const std::size_t count = static_cast<std::size_t>(
            std::min<std::uint64_t>(maxBytes, m_impl->storedSize - m_impl->produced));
        auto bytes = m_impl->source(m_impl->produced, count);
        if (!bytes) return bytes.error();
        if (bytes.value().size() != count) return m_impl->fail("source returned a short range");
        m_impl->produced += count;
        m_impl->done = m_impl->produced == m_impl->storedSize;
        return bytes;
    }

    while (output.size() < maxBytes && !m_impl->done) {
        if (m_impl->literalRemaining != 0) {
            auto available = m_impl->ensure(1);
            if (!available) return available.error();
            if (!available.value()) return m_impl->fail("literal run passes end of input");
            const std::size_t amount = std::min({
                m_impl->literalRemaining,
                maxBytes - output.size(),
                m_impl->input.size() - m_impl->inputPosition});
            const std::uint8_t* source = m_impl->input.data() + m_impl->inputPosition;
            const Error error = m_impl->emitBlock(output, source, amount);
            if (!error.message.empty()) return error;
            m_impl->consume(amount);
            m_impl->literalRemaining -= amount;
            continue;
        }
        if (m_impl->copyRemaining != 0) {
            if (m_impl->copyDistance > m_impl->produced ||
                m_impl->copyDistance > m_impl->window.size()) {
                return m_impl->fail("back-reference is outside the sliding window");
            }
            const std::size_t amount =
                std::min(m_impl->copyRemaining, maxBytes - output.size());
            const std::size_t outputStart = output.size();
            output.resize(outputStart + amount);
            const std::size_t seed = std::min(amount, m_impl->copyDistance);
            for (std::size_t i = 0; i < seed; ++i) {
                const std::size_t position =
                    (m_impl->windowPosition + m_impl->window.size() -
                     m_impl->copyDistance + i) %
                    m_impl->window.size();
                output[outputStart + i] = m_impl->window[position];
            }
            for (std::size_t i = seed; i < amount; ++i) {
                output[outputStart + i] =
                    output[outputStart + i - m_impl->copyDistance];
            }
            const std::size_t first =
                std::min(amount, m_impl->window.size() - m_impl->windowPosition);
            std::copy_n(
                output.data() + outputStart, first,
                m_impl->window.begin() + m_impl->windowPosition);
            if (amount > first) {
                std::copy_n(output.data() + outputStart + first, amount - first,
                            m_impl->window.begin());
            }
            m_impl->windowPosition =
                (m_impl->windowPosition + amount) % m_impl->window.size();
            m_impl->produced += amount;
            const Error error =
                m_impl->produced > m_impl->decodedLength
                    ? m_impl->fail("output exceeds declared size")
                    : Error{};
            if (!error.message.empty()) return error;
            m_impl->copyRemaining -= amount;
            continue;
        }
        if (m_impl->stop) {
            if (m_impl->produced != m_impl->decodedLength) {
                return m_impl->fail("output is shorter than declared size");
            }
            if (m_impl->consumed != m_impl->storedSize) {
                return m_impl->fail("trailing bytes after stop command");
            }
            m_impl->done = true;
            break;
        }

        auto firstAvailable = m_impl->ensure(1);
        if (!firstAvailable) return firstAvailable.error();
        if (!firstAvailable.value()) return m_impl->fail("stream ends without a stop command");
        const std::uint8_t b0 = m_impl->input[m_impl->inputPosition];
        const std::size_t commandSize =
            b0 < 0x80 ? 2 : (b0 < 0xC0 ? 3 : (b0 < 0xE0 ? 4 : 1));
        auto commandAvailable = m_impl->ensure(commandSize);
        if (!commandAvailable) return commandAvailable.error();
        if (!commandAvailable.value()) return m_impl->fail("truncated command");
        const Command command = decodeCommand(m_impl->input.data() + m_impl->inputPosition);
        m_impl->consume(command.encodedSize);
        m_impl->literalRemaining = command.literal;
        m_impl->copyRemaining = command.length;
        m_impl->copyDistance = command.distance;
        m_impl->stop = command.stop;
    }
    return output;
}

std::uint64_t DecodedStream::decodedSize() const { return m_impl->decodedLength; }
std::uint64_t DecodedStream::position() const { return m_impl->produced; }
bool DecodedStream::compressed() const { return m_impl->isCompressed; }
bool DecodedStream::finished() const { return m_impl->done; }

}  // namespace ra3::io
