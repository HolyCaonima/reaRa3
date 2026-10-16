#include <ra3/io/RefPack.h>

#include <algorithm>
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

// With no `prefix`, the whole stream is decoded and checked strictly. With a
// prefix length, decoding stops once that many bytes exist, so the input may
// end anywhere after the commands that produce them.
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
        const std::uint8_t b0 = data[in];
        std::size_t literal = 0;
        std::size_t length = 0;
        std::size_t distance = 0;
        std::size_t commandSize = 1;
        bool stop = false;

        if (b0 < 0x80) {
            commandSize = 2;
        } else if (b0 < 0xC0) {
            commandSize = 3;
        } else if (b0 < 0xE0) {
            commandSize = 4;
        }
        if (size - in < commandSize) {
            return fail("truncated command");
        }
        const std::uint8_t* c = data + in;
        if (b0 < 0x80) {
            literal = b0 & 0x03;
            length = ((b0 & 0x1C) >> 2) + 3;
            distance = ((b0 & 0x60) << 3) + c[1] + 1;
        } else if (b0 < 0xC0) {
            literal = (c[1] >> 6) & 0x03;
            length = (b0 & 0x3F) + 4;
            distance = ((c[1] & 0x3F) << 8) + c[2] + 1;
        } else if (b0 < 0xE0) {
            literal = b0 & 0x03;
            length = ((b0 & 0x0C) << 6) + c[3] + 5;
            distance = ((b0 & 0x10) << 12) + (c[1] << 8) + c[2] + 1;
        } else if (b0 < 0xFC) {
            literal = ((b0 & 0x1F) << 2) + 4;
        } else {
            literal = b0 & 0x03;
            stop = true;
        }
        in += commandSize;

        if (whole && out.size() + literal + length > limit) {
            return fail("output exceeds declared size");
        }
        const std::size_t literalTaken = std::min(literal, target - out.size());
        if (size - in < literalTaken) {
            return fail("literal run past end of input");
        }
        out.insert(out.end(), data + in, data + in + literalTaken);
        in += literalTaken;

        const std::size_t lengthTaken = std::min(length, target - out.size());
        if (lengthTaken != 0) {
            if (distance > out.size()) {
                return fail("back-reference before start of output");
            }
            const std::size_t from = out.size() - distance;
            for (std::size_t i = 0; i < lengthTaken; ++i) {
                const std::uint8_t byte = out[from + i];
                out.push_back(byte);
            }
        }
        if (stop) {
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

Result<std::vector<std::uint8_t>> decompressRefPack(const std::uint8_t* data, std::size_t size)
{
    return inflate(data, size, std::nullopt);
}

Result<std::vector<std::uint8_t>> decompressRefPackPrefix(
    const std::uint8_t* data, std::size_t size, std::size_t maxOutput)
{
    return inflate(data, size, maxOutput);
}

}  // namespace ra3::io
