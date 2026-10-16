#pragma once

#include <ra3/core/Result.h>

#include <cstddef>
#include <cstdint>
#include <optional>
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

// Fails unless the command stream ends with its stop command exactly at the
// end of `size` and produces exactly the declared number of bytes.
Result<std::vector<std::uint8_t>> decompressRefPack(const std::uint8_t* data, std::size_t size);

}  // namespace ra3::io
