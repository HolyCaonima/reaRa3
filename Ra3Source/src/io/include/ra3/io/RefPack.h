#pragma once

#include <ra3/core/Result.h>

#include <cstddef>
#include <cstdint>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace ra3::io {

// EA RefPack compression, the format of many entries inside the shipped .big
// archives (first bytes "10 FB", "90 FB", ...).
//
// Header: flags byte, 0xFB, [compressed size if flags & 0x01], uncompressed
// size; sizes are big-endian, 4 bytes if flags & 0x80, otherwise 3.
//
// ra3_1.0.game has three readers of it: the one-shot decoder 0x009F72E0,
// which this code matches (it skips the 0x01 field and ends at the stop
// command), plus the size probe 0x009F74F0 and the streaming
// rw::core::codec::RefpackInflate 0x009F7620, which accept any header with
// (b0 << 8 | b1) & 0x3EFF == 0x10FB but do not skip the 0x01 field. No
// shipped entry checked so far sets 0x01. Flag 0x40 only shrinks the
// streaming decoder's window from 128 KiB to 16 KiB.
struct RefPackHeader {
    std::uint8_t flags = 0;
    std::uint32_t decompressedSize = 0;
    std::size_t headerSize = 0;
};

std::optional<RefPackHeader> readRefPackHeader(const std::uint8_t* data, std::size_t size);

// ra3_1.0.game 0x009F74F0: the first size field after the signature, or 0 when
// `data` is not RefPack. With flag 0x01 that field is the compressed size, not
// the decompressed size readRefPackHeader reports.
std::uint32_t probeRefPackSize(const std::uint8_t* data);

using RangeReader =
    std::function<Result<std::vector<std::uint8_t>>(std::uint64_t offset, std::size_t count)>;

// Sequential decoded view of either a RefPack stream or plain bytes. Input is
// fetched in bounded ranges and RefPack history is limited to its 128 KiB
// window, matching rw::core::codec::RefpackInflate (0x009F7620).
class DecodedStream {
public:
    static Result<DecodedStream> open(
        RangeReader source,
        std::uint64_t storedSize,
        std::string name,
        std::size_t sourceChunkSize = 64 * 1024);

    DecodedStream(DecodedStream&&) noexcept;
    DecodedStream& operator=(DecodedStream&&) noexcept;
    ~DecodedStream();

    Result<std::vector<std::uint8_t>> read(std::size_t maxBytes);
    std::uint64_t decodedSize() const;
    std::uint64_t position() const;
    bool compressed() const;
    bool finished() const;

private:
    struct Impl;
    explicit DecodedStream(std::unique_ptr<Impl> impl);
    std::unique_ptr<Impl> m_impl;
};

// Fails unless the command stream ends with its stop command exactly at the
// end of `size` and produces exactly the declared number of bytes.
Result<std::vector<std::uint8_t>> decompressRefPack(const std::uint8_t* data, std::size_t size);

// Decodes at most `maxOutput` bytes and does not require the compressed input
// to contain the remainder of the stream. Reads no input beyond the commands
// it decodes, so `size` may be an upper bound larger than the real buffer.
// Like the one-shot decoder 0x009F72E0, back-references are not limited to the
// streaming decoder's window.
Result<std::vector<std::uint8_t>> decompressRefPackPrefix(
    const std::uint8_t* data, std::size_t size, std::size_t maxOutput);

}  // namespace ra3::io
