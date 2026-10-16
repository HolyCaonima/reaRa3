#include <ra3/io/GameBigLayout.h>

#include <ra3/io/PathCompare.h>
#include <ra3/io/RefPack.h>

#include <algorithm>
#include <cctype>
#include <cstring>

namespace ra3::io {
namespace {

void fieldWidths(const std::uint8_t* header, int& offsetWidth, int& sizeWidth) noexcept
{
    offsetWidth = 4;
    sizeWidth = 4;
    const GameBigFormat format = detectGameBigFormat(header);
    if (format == GameBigFormat::big) {
        offsetWidth = static_cast<int>(static_cast<signed char>(header[3])) - '0';
        if (offsetWidth == 'F' - '0') {
            offsetWidth = 4;
        }
        sizeWidth = std::min(offsetWidth, 4);
    } else if (format == GameBigFormat::c0fb) {
        offsetWidth = 3;
        sizeWidth = 3;
    }
}

std::uint32_t byteSwap(std::uint32_t value) noexcept
{
    return ((value & 0x000000FFu) << 24)
        | ((value & 0x0000FF00u) << 8)
        | ((value & 0x00FF0000u) >> 8)
        | ((value & 0xFF000000u) >> 24);
}

const std::uint8_t* nextRecord(
    const std::uint8_t* record,
    int offsetWidth,
    int sizeWidth) noexcept
{
    const char* name = reinterpret_cast<const char*>(record + offsetWidth + sizeWidth);
    return reinterpret_cast<const std::uint8_t*>(name + std::strlen(name) + 1);
}

void setEntryOutputs(
    const std::uint8_t* record,
    int offsetWidth,
    int sizeWidth,
    std::uint64_t* offsetOut,
    std::uint64_t* sizeOut,
    bool wide) noexcept
{
    if (offsetOut != nullptr) {
        *offsetOut = wide
            ? readGameBigEndian64(record, offsetWidth)
            : (offsetWidth >= 1 && offsetWidth <= 4
                   ? readGameBigEndian32(record, offsetWidth)
                   : 0);
    }
    if (sizeOut != nullptr) {
        *sizeOut = wide
            ? readGameBigEndian64(record + offsetWidth, sizeWidth)
            : (sizeWidth >= 1 && sizeWidth <= 4
                   ? readGameBigEndian32(record + offsetWidth, sizeWidth)
                   : 0);
    }
}

}  // namespace

std::uint32_t readGameBigEndian32(const std::uint8_t* bytes, int width) noexcept
{
    if (width < 1 || width > 4) {
        return 0;
    }
    std::uint32_t value = 0;
    for (int i = 0; i < width; ++i) {
        value = (value << 8) | bytes[i];
    }
    return value;
}

std::uint64_t readGameBigEndian64(const std::uint8_t* bytes, int width) noexcept
{
    std::uint64_t value = 0;
    for (int i = 0; i < width; ++i) {
        value = (value << 8) | bytes[i];
    }
    return value;
}

GameBigFormat detectGameBigFormat(const std::uint8_t* header) noexcept
{
    if (std::memcmp(header, "Viv4", 4) == 0) {
        return GameBigFormat::viv4;
    }
    if (header[0] == 'B' && header[1] == 'I' && header[2] == 'G') {
        return GameBigFormat::big;
    }
    if (header[0] == 0xC0 && header[1] == 0xFB) {
        return GameBigFormat::c0fb;
    }
    return GameBigFormat::invalid;
}

int gameBigHeaderSize(const std::uint8_t* header) noexcept
{
    const GameBigFormat format = detectGameBigFormat(header);
    if (format == GameBigFormat::c0fb) {
        return static_cast<int>(readGameBigEndian32(header + 2, 2)) + 4;
    }
    if (format == GameBigFormat::big || format == GameBigFormat::viv4) {
        return static_cast<int>(readGameBigEndian32(header + 12, 4));
    }
    return 0;
}

std::uint32_t gameBigEntryCount(const GameBigIndex* index) noexcept
{
    if (detectGameBigFormat(index->entries) == GameBigFormat::c0fb) {
        return readGameBigEndian32(index->header + 4, 2);
    }
    const GameBigFormat format = detectGameBigFormat(index->entries);
    return format == GameBigFormat::big || format == GameBigFormat::viv4
        ? readGameBigEndian32(index->header + 8, 4)
        : 0;
}

int gameBigTrailerSize(const GameBigIndex* index, int* numberOut) noexcept
{
    const std::uint8_t* trailer = index->header + gameBigHeaderSize(index->header) - 8;
    const bool letter = (trailer[0] >= 'A' && trailer[0] <= 'Z')
        || (trailer[0] >= 'a' && trailer[0] <= 'z');
    const bool digits = trailer[1] >= '0' && trailer[1] <= '9'
        && trailer[2] >= '0' && trailer[2] <= '9'
        && trailer[3] >= '0' && trailer[3] <= '9';
    if (!letter || !digits) {
        *numberOut = 0;
        return 0;
    }
    *numberOut = (trailer[1] - '0') * 100 + (trailer[2] - '0') * 10
        + trailer[3] - '0';
    return 8;
}

std::uint32_t hashGameBigPath(const char* path, std::uint32_t seed) noexcept
{
    std::uint32_t hash = seed;
    const auto* bytes = reinterpret_cast<const unsigned char*>(path);
    while (*bytes != 0) {
        hash = hash * 33u + static_cast<std::uint32_t>(std::tolower(*bytes));
        ++bytes;
    }
    return byteSwap(hash);
}

std::uint64_t gameBigStoredSize(const GameBigFile* file) noexcept
{
    return file->logicalSize;
}

std::uint64_t gameBigAbsoluteDataOffset(const GameBigFile* file) noexcept
{
    return file->archive->rangeBegin + file->dataOffset;
}

bool applyGameBigSeekState(GameBigFile* file, std::uint64_t position) noexcept
{
    if (position != 0 && file->compressedSize != 0) {
        file->logicalPosition = position;
        return false;
    }
    file->logicalPosition = 0;
    file->sourcePosition = file->dataOffset + position;
    const bool reset = file->inflater != nullptr;
    if (reset) {
        file->inflater = nullptr;
        file->temporaryBuffer = nullptr;
    }
    return reset;
}

bool prepareGameBigFile(
    const GameBigHandler* handler,
    const char* path,
    std::uint32_t ownerToken,
    std::uint32_t openMode,
    GameBigRangeRead read,
    void* readContext,
    GameBigFile* output) noexcept
{
    if (handler == nullptr || path == nullptr || output == nullptr) {
        return false;
    }
    if (std::strncmp(path, "big:", 4) == 0) {
        path += 4;
    }
    if (*path == '/' || *path == '\\') {
        ++path;
    }

    const char* member = path;
    const char* separator = std::strchr(path, '|');
    const std::size_t archiveLength =
        separator == nullptr ? 0u : static_cast<std::size_t>(separator - path);
    if (separator != nullptr) {
        member = separator + 1;
    }

    for (GameBigArchive* archive = handler->head;
         archive != nullptr;
         archive = archive->next) {
        if (separator != nullptr) {
            const char* name = std::strchr(archive->archiveName, ':');
            name = name == nullptr ? archive->archiveName : name + 1;
            if (std::strlen(name) != archiveLength
                || std::memcmp(name, path, archiveLength) != 0) {
                continue;
            }
        }

        std::uint64_t offset = 0;
        std::uint64_t size = 0;
        if (findGameBigEntry(
                &archive->index, member, 0, &offset, &size, nullptr, nullptr)
            == nullptr) {
            // A qualified archive never falls through to a later archive.
            if (separator != nullptr) {
                return false;
            }
            continue;
        }

        std::uint64_t decodedSize = 0;
        if (read != nullptr && size >= 5) {
            std::uint8_t prefix[16]{};
            const std::uint32_t wanted =
                static_cast<std::uint32_t>(std::min<std::uint64_t>(sizeof(prefix), size));
            if (read(readContext, archive, offset, prefix, wanted) == wanted) {
                decodedSize = probeRefPackSize(prefix);
            }
        }

        *output = {};
        output->archive = archive;
        output->ownerToken = ownerToken;
        output->openMode = openMode;
        output->dataOffset = offset;
        output->logicalSize = decodedSize == 0 ? size : decodedSize;
        output->compressedSize = decodedSize == 0 ? 0 : size;
        output->sourcePosition = offset;
        return true;
    }
    return false;
}

std::uint64_t seekGameBigFile(
    GameBigFile* file,
    std::uint64_t position,
    const GameBigReleaseOps& operations) noexcept
{
    void* const inflater = file->inflater;
    void* const temporary = file->temporaryBuffer;
    if (applyGameBigSeekState(file, position)) {
        if (operations.destroyInflater != nullptr) {
            operations.destroyInflater(operations.context, inflater);
        }
        if (operations.release != nullptr) {
            operations.release(operations.context, temporary);
        }
    }
    return position;
}

void closeGameBigFile(
    GameBigFile* file,
    const GameBigReleaseOps& operations) noexcept
{
    if (file == nullptr) {
        return;
    }
    if (file->inflater != nullptr) {
        if (operations.destroyInflater != nullptr) {
            operations.destroyInflater(operations.context, file->inflater);
        }
        if (operations.release != nullptr) {
            operations.release(operations.context, file->temporaryBuffer);
        }
        file->inflater = nullptr;
        file->temporaryBuffer = nullptr;
    }
    if (operations.release != nullptr) {
        operations.release(operations.context, file);
    }
}

void applyGameBigReadResult(GameBigFile* file, std::uint32_t actual) noexcept
{
    if (file->compressedSize == 0) {
        file->sourcePosition += actual;
    } else {
        file->logicalPosition += actual;
    }
}

void initializeGameRefPackInflater(
    GameRefPackInflater* inflater,
    GameAllocator* allocator,
    const void* vtable) noexcept
{
    std::memset(inflater, 0, sizeof(*inflater));
    inflater->vtable = vtable;
    inflater->allocator = allocator;
    inflater->initialized = 1;
    inflater->pendingInputBytes = 2;
}

void setGameRefPackInput(
    GameRefPackInflater* inflater,
    const std::uint8_t* input,
    std::uint32_t count) noexcept
{
    inflater->inputBegin = input;
    inflater->inputRemaining = count;
}

void setGameRefPackOutput(
    GameRefPackInflater* inflater,
    std::uint8_t* output,
    std::uint32_t count) noexcept
{
    inflater->outputBegin = output;
    inflater->outputRemaining = count;
}

namespace {

void* allocateInflaterWindow(GameRefPackInflater* inflater, std::uint32_t size) noexcept
{
    if (inflater->allocator == nullptr || inflater->allocator->vtable == nullptr
        || inflater->allocator->vtable->allocateTagged == nullptr) {
        return nullptr;
    }
    using Function = void*(__thiscall*)(
        GameAllocator*, std::uint32_t, const char*, std::uint32_t);
    return reinterpret_cast<Function>(inflater->allocator->vtable->allocateTagged)(
        inflater->allocator,
        size,
        "rw::core::codec::RefpackInflate::Decompress::mRefWindowBase",
        0);
}

void releaseInflaterAllocation(GameRefPackInflater* inflater, void* allocation) noexcept
{
    if (allocation == nullptr || inflater->allocator == nullptr
        || inflater->allocator->vtable == nullptr
        || inflater->allocator->vtable->freeTagged == nullptr) {
        return;
    }
    using Function = void(__thiscall*)(GameAllocator*, void*, std::uint32_t);
    reinterpret_cast<Function>(inflater->allocator->vtable->freeTagged)(
        inflater->allocator, allocation, 0);
}

void wrapWindowCursor(GameRefPackInflater* inflater) noexcept
{
    if (inflater->windowCursor == inflater->windowBegin + inflater->windowSize) {
        inflater->windowCursor = inflater->windowBegin;
    }
}

std::uint8_t* inflaterCopyCursor(
    GameRefPackInflater* inflater, std::uint32_t distance) noexcept
{
    std::uintptr_t address =
        reinterpret_cast<std::uintptr_t>(inflater->windowCursor) - distance;
    const std::uintptr_t begin =
        reinterpret_cast<std::uintptr_t>(inflater->windowBegin);
    if (address < begin) {
        address += inflater->windowSize;
    }
    return reinterpret_cast<std::uint8_t*>(address);
}

bool emitCoveredByte(GameRefPackInflater* inflater) noexcept
{
    if (inflater->windowProduced < inflater->windowSize) {
        return true;
    }
    if (inflater->outputRemaining == 0) {
        return false;
    }
    wrapWindowCursor(inflater);
    *inflater->outputCursor++ = *inflater->windowCursor;
    ++inflater->outputProduced;
    --inflater->outputRemaining;
    return true;
}

}  // namespace

std::uint32_t processGameRefPackInflater(GameRefPackInflater* inflater) noexcept
{
    if (inflater->status != 0) {
        return inflater->status;
    }

    // The original has the same all-at-once fast path before entering its
    // incremental state machine (ra3_1.0.game 0x009F762F).
    if (inflater->state == 0 && inflater->inputRemaining > 5) {
        const std::uint32_t decoded = probeRefPackSize(inflater->inputBegin);
        if (decoded != 0 && decoded <= inflater->outputRemaining) {
            try {
                auto result = decompressRefPackPrefix(
                    inflater->inputBegin, inflater->inputRemaining, decoded);
                if (!result || result.value().size() != decoded) {
                    inflater->status = 2;
                    return inflater->status;
                }
                std::memcpy(inflater->outputCursor, result.value().data(), decoded);
            } catch (...) {
                inflater->status = 2;
                return inflater->status;
            }
            inflater->outputRemaining -= decoded;
            inflater->outputCursor += decoded;
            inflater->outputProduced = decoded;
            inflater->unknown1C = 0;
            inflater->status = 1;
            return inflater->status;
        }
    }

    for (;;) {
        switch (inflater->state) {
        case 0:
            while (inflater->pendingInputBytes != 0) {
                if (inflater->inputRemaining == 0) {
                    return inflater->status;
                }
                --inflater->inputRemaining;
                --inflater->pendingInputBytes;
                inflater->command =
                    inflater->command * 0x100u + *inflater->inputCursor++;
            }
            if ((inflater->command & 0x3EFFu) != 0x10FBu) {
                inflater->status = 2;
                return inflater->status;
            }
            inflater->windowSize =
                (inflater->command & 0x4000u) != 0 ? 0x4000u : 0x20000u;
            inflater->pendingInputBytes =
                (inflater->command & 0x8000u) != 0 ? 4u : 3u;
            inflater->windowBegin = static_cast<std::uint8_t*>(
                allocateInflaterWindow(inflater, inflater->windowSize));
            if (inflater->windowBegin == nullptr) {
                inflater->status = 2;
                return inflater->status;
            }
            inflater->windowCursor = inflater->windowBegin;
            inflater->state = 1;
            continue;

        case 1:
            while (inflater->pendingInputBytes != 0) {
                if (inflater->inputRemaining == 0) {
                    return inflater->status;
                }
                --inflater->inputRemaining;
                --inflater->pendingInputBytes;
                inflater->decodedLength =
                    inflater->decodedLength * 0x100u + *inflater->inputCursor++;
            }
            inflater->state = 2;
            continue;

        case 2:
            if (inflater->windowProduced
                == static_cast<std::uint64_t>(inflater->decodedLength)) {
                inflater->state = 13;
                continue;
            }
            if (inflater->inputRemaining == 0) {
                return inflater->status;
            }
            --inflater->inputRemaining;
            inflater->command = *inflater->inputCursor++;
            inflater->state = 3;
            continue;

        case 3: {
            const std::uint8_t first = static_cast<std::uint8_t>(inflater->command);
            if (first < 0x80) {
                inflater->pendingInputBytes = 1;
                inflater->state = 4;
                inflater->nextState = 5;
            } else if (first < 0xC0) {
                inflater->pendingInputBytes = 2;
                inflater->state = 4;
                inflater->nextState = 6;
            } else if (first < 0xE0) {
                inflater->pendingInputBytes = 3;
                inflater->state = 4;
                inflater->nextState = 7;
            } else {
                inflater->runRemaining =
                    (static_cast<std::uint32_t>(first & 0x1F) * 4u) + 4u;
                inflater->state = 11;
                inflater->nextState = 2;
                if (inflater->runRemaining > 0x70u) {
                    inflater->runRemaining = first & 3u;
                }
            }
            continue;
        }

        case 4:
            while (inflater->pendingInputBytes != 0) {
                if (inflater->inputRemaining == 0) {
                    return inflater->status;
                }
                --inflater->inputRemaining;
                --inflater->pendingInputBytes;
                inflater->command =
                    inflater->command * 0x100u + *inflater->inputCursor++;
            }
            inflater->state = inflater->nextState;
            continue;

        case 5:
            inflater->runRemaining = (inflater->command >> 8) & 3u;
            if (inflater->runRemaining == 0) {
                inflater->state = 8;
            } else {
                inflater->state = 11;
                inflater->nextState = 8;
            }
            continue;

        case 6:
            inflater->runRemaining = (inflater->command >> 14) & 3u;
            if (inflater->runRemaining == 0) {
                inflater->state = 9;
            } else {
                inflater->state = 11;
                inflater->nextState = 9;
            }
            continue;

        case 7:
            inflater->runRemaining = (inflater->command >> 24) & 3u;
            if (inflater->runRemaining == 0) {
                inflater->state = 10;
            } else {
                inflater->state = 11;
                inflater->nextState = 10;
            }
            continue;

        case 8: {
            const std::uint32_t distance =
                ((inflater->command >> 5) & 0x300u)
                + (inflater->command & 0xFFu) + 1u;
            inflater->copyCursor = inflaterCopyCursor(inflater, distance);
            inflater->runRemaining = ((inflater->command >> 10) & 7u) + 3u;
            inflater->state = 12;
            continue;
        }

        case 9: {
            const std::uint32_t distance = (inflater->command & 0x3FFFu) + 1u;
            inflater->copyCursor = inflaterCopyCursor(inflater, distance);
            inflater->runRemaining = ((inflater->command >> 16) & 0x3Fu) + 4u;
            inflater->state = 12;
            continue;
        }

        case 10: {
            const std::uint32_t distance =
                ((inflater->command >> 12) & 0x10000u)
                + ((inflater->command >> 8) & 0xFFFFu) + 1u;
            inflater->copyCursor = inflaterCopyCursor(inflater, distance);
            inflater->runRemaining =
                ((inflater->command >> 18) & 0x300u)
                + (inflater->command & 0xFFu) + 5u;
            inflater->state = 12;
            continue;
        }

        case 11:
            while (inflater->runRemaining != 0) {
                // The original tests for exhausted literal input before it
                // drains an already-covered window byte (ra3_1.0.game
                // 0x009F779A).  This ordering is observable at yield points.
                if (inflater->windowProduced
                        < static_cast<std::uint64_t>(inflater->decodedLength)
                    && inflater->inputRemaining == 0) {
                    return inflater->status;
                }
                if (!emitCoveredByte(inflater)) {
                    return inflater->status;
                }
                if (inflater->windowProduced
                    < static_cast<std::uint64_t>(inflater->decodedLength)) {
                    wrapWindowCursor(inflater);
                    --inflater->inputRemaining;
                    *inflater->windowCursor++ = *inflater->inputCursor++;
                    ++inflater->windowProduced;
                    --inflater->runRemaining;
                }
            }
            inflater->state = inflater->nextState;
            continue;

        case 12:
            while (inflater->runRemaining != 0) {
                if (!emitCoveredByte(inflater)) {
                    return inflater->status;
                }
                wrapWindowCursor(inflater);
                if (inflater->copyCursor
                    == inflater->windowBegin + inflater->windowSize) {
                    inflater->copyCursor = inflater->windowBegin;
                }
                *inflater->windowCursor++ = *inflater->copyCursor++;
                ++inflater->windowProduced;
                --inflater->runRemaining;
            }
            inflater->state = 2;
            continue;

        case 13:
            if (inflater->windowProduced < inflater->windowSize) {
                inflater->windowCursor = inflater->windowBegin;
            }
            inflater->windowProduced -= inflater->outputProduced;
            inflater->state = 14;
            continue;

        case 14:
            while (inflater->windowProduced != 0) {
                if (inflater->outputRemaining == 0) {
                    return inflater->status;
                }
                if (static_cast<std::uint32_t>(
                        inflater->windowCursor - inflater->windowBegin)
                    >= inflater->windowSize) {
                    inflater->windowCursor = inflater->windowBegin;
                }
                *inflater->outputCursor++ = *inflater->windowCursor++;
                ++inflater->outputProduced;
                --inflater->outputRemaining;
                --inflater->windowProduced;
            }
            inflater->status = 1;
            return inflater->status;

        default:
            inflater->status = 2;
            return inflater->status;
        }
    }
}

std::uint32_t runGameRefPackInflater(GameRefPackInflater* inflater) noexcept
{
    inflater->inputCursor = inflater->inputBegin;
    inflater->outputCursor = inflater->outputBegin;
    inflater->status = processGameRefPackInflater(inflater);
    inflater->inputBegin = inflater->inputCursor;
    inflater->outputBegin = inflater->outputCursor;
    return inflater->status;
}

void destroyGameRefPackInflater(GameRefPackInflater* inflater) noexcept
{
    if (inflater == nullptr) {
        return;
    }
    releaseInflaterAllocation(inflater, inflater->windowBegin);
    inflater->windowBegin = nullptr;
    inflater->initialized = 0;
}

std::uint32_t inflateGameBigRange(
    GameBigFile* file,
    void* destination,
    std::uint32_t destinationCapacity,
    std::uint32_t wanted,
    std::uint32_t sourceChunkSize,
    bool finalFlush,
    const GameBigIoOps& operations) noexcept
{
    auto* inflater = static_cast<GameRefPackInflater*>(file->inflater);
    setGameRefPackOutput(
        inflater, static_cast<std::uint8_t*>(destination),
        std::min(destinationCapacity, wanted));
    const std::uint64_t before = inflater->outputProduced;
    for (;;) {
        const std::uint32_t produced = static_cast<std::uint32_t>(
            inflater->outputProduced - before);
        if (inflater->outputRemaining == 0 || produced >= wanted) {
            return produced;
        }
        if (inflater->inputRemaining == 0) {
            const std::uint64_t end = file->dataOffset + file->compressedSize;
            const std::uint32_t available = static_cast<std::uint32_t>(
                std::min<std::uint64_t>(
                    sourceChunkSize,
                    end > file->sourcePosition ? end - file->sourcePosition : 0));
            if (available != 0) {
                const std::uint32_t actual = operations.read(
                    operations.context, file->temporaryBuffer, available);
                file->sourcePosition += actual;
                setGameRefPackInput(
                    inflater,
                    static_cast<const std::uint8_t*>(file->temporaryBuffer),
                    available);
            }
        }
        const std::uint32_t status = runGameRefPackInflater(inflater);
        const std::uint32_t after = static_cast<std::uint32_t>(
            inflater->outputProduced - before);
        if (status != 0 || after >= wanted) {
            return after;
        }
        if (finalFlush && inflater->outputRemaining == 0) {
            // ra3_1.0.game 0x009F6280 reuses the same bounded scratch span
            // while discarding output for a forward logical seek.
            setGameRefPackOutput(
                inflater,
                static_cast<std::uint8_t*>(destination),
                std::min(destinationCapacity, wanted - after));
        }
    }
}

std::uint32_t readGameBigFile(
    GameBigFile* file,
    void* destination,
    std::uint32_t count,
    const GameBigIoOps& operations) noexcept
{
    if (operations.lock != nullptr) {
        operations.lock(operations.context, file->archive);
    }
    operations.seek(operations.context, file->sourcePosition);

    std::uint32_t actual = 0;
    if (file->compressedSize == 0) {
        const std::uint64_t end = file->dataOffset + file->logicalSize;
        const std::uint32_t wanted = static_cast<std::uint32_t>(
            std::min<std::uint64_t>(
                count,
                end > file->sourcePosition ? end - file->sourcePosition : 0));
        actual = operations.read(operations.context, destination, wanted);
        file->sourcePosition += actual;
    } else if (
        (file->openMode & 0x10000u) != 0
        && static_cast<std::uint64_t>(count) >= file->logicalSize + 0x14u) {
        auto* source = static_cast<std::uint8_t*>(destination)
            + ((count - static_cast<std::uint32_t>(file->compressedSize))
               & ~std::uint32_t{3});
        const std::uint32_t read = operations.read(
            operations.context, source,
            static_cast<std::uint32_t>(file->compressedSize));
        file->sourcePosition += read;
        try {
            auto decoded = decompressRefPackPrefix(
                source, read, static_cast<std::size_t>(file->logicalSize));
            if (decoded) {
                actual = static_cast<std::uint32_t>(decoded.value().size());
                std::memcpy(destination, decoded.value().data(), actual);
            }
        } catch (...) {
            actual = 0;
        }
        file->logicalPosition += actual;
    } else {
        std::uint32_t chunkSize = 0x8000;
        if (count >= file->logicalSize) {
            chunkSize = (static_cast<std::uint32_t>(file->compressedSize)
                         + 0x7FFu)
                & ~0x7FFu;
        }
        if (file->inflater == nullptr) {
            file->inflater = operations.createInflater(operations.context);
            file->temporaryBuffer =
                operations.allocateTemporary(operations.context, chunkSize);
        }
        auto* inflater = static_cast<GameRefPackInflater*>(file->inflater);
        if (file->logicalPosition > inflater->outputProduced) {
            std::array<std::uint8_t, 0x8000> discard{};
            inflateGameBigRange(
                file, discard.data(),
                static_cast<std::uint32_t>(discard.size()),
                static_cast<std::uint32_t>(
                    file->logicalPosition - inflater->outputProduced),
                chunkSize, true, operations);
        }
        actual = inflateGameBigRange(
            file, destination, count, count, chunkSize, false, operations);
        file->logicalPosition += actual;
    }
    if (operations.unlock != nullptr) {
        operations.unlock(operations.context, file->archive);
    }
    return actual;
}

bool matchGameBigPattern(const char* text, const char* pattern) noexcept
{
    if (*text == '\0') {
        return *pattern == '\0';
    }
    if (*pattern == '\0') {
        return false;
    }
    if (*text == *pattern || *pattern == '?') {
        return matchGameBigPattern(text + 1, pattern + 1);
    }
    if (*pattern != '*') {
        return false;
    }
    ++pattern;
    if (*pattern == '\0') {
        return true;
    }
    do {
        if (matchGameBigPattern(text, pattern)) {
            return true;
        }
    } while (*text++ != '\0');
    return false;
}

void initializeGameBigFindState(
    GameBigFindState* state,
    const char* pattern,
    GameBigArchive* firstArchive) noexcept
{
    state->archive = firstArchive;
    state->ordinal = -1;
    state->recordCursor = nullptr;
    state->recordOrdinal = 0;
    std::strcpy(state->pattern.data(), pattern);
    for (char* cursor = state->pattern.data(); *cursor != '\0'; ++cursor) {
        if (*cursor == '\\') {
            *cursor = '/';
        }
    }
    char* end = state->pattern.data() + std::strlen(state->pattern.data());
    while (end != state->pattern.data() && end[-1] == '/') {
        --end;
    }
    *end = '\0';
}

bool findNextGameBig(
    GameBigFindState* state,
    GameBigFindResult* result) noexcept
{
    if (state == nullptr || result == nullptr || state->archive == nullptr) {
        return false;
    }
    char path[0x100]{};
    std::uint64_t size = 0;
    for (;;) {
        ++state->ordinal;
        while (state->archive != nullptr
               && state->ordinal
                   >= static_cast<std::int32_t>(
                       gameBigEntryCount(&state->archive->index))) {
            state->archive = state->archive->next;
            if (state->archive == nullptr) {
                return false;
            }
            state->ordinal = 0;
            state->recordCursor = nullptr;
            state->recordOrdinal = 0;
        }
        const char* found = findGameBigEntry(
            &state->archive->index,
            nullptr,
            state->ordinal,
            nullptr,
            &size,
            &state->recordCursor,
            &state->recordOrdinal);
        if (found == nullptr) {
            return false;
        }
        std::strcpy(path, found);
        for (char* cursor = path; *cursor != '\0'; ++cursor) {
            if (*cursor == '\\') {
                *cursor = '/';
            }
        }
        char* end = path + std::strlen(path);
        while (end != path && end[-1] == '/') {
            --end;
        }
        *end = '\0';
        if (matchGameBigPattern(path, state->pattern.data())) {
            const char* name = end;
            while (name != path && name[-1] != '/' && name[-1] != '\\') {
                --name;
            }
            result->type = 1;
            result->size = size;
            std::strcpy(result->name, name);
            return true;
        }
    }
}

const std::uint8_t* findGameBigHashedEntry(
    const GameBigIndex* index,
    const char* path,
    int offsetWidth,
    int sizeWidth) noexcept
{
    const int count = static_cast<int>(gameBigEntryCount(index));
    int first = 0;
    int last = count - 1;
    const std::uint32_t wanted = hashGameBigPath(path);
    while (first <= last) {
        const int middle = first + (last - first) / 2;
        const GameBigSearchEntry& candidate = index->searchTable[middle];
        if (candidate.hash == wanted) {
            const auto name = reinterpret_cast<const char*>(
                candidate.record + offsetWidth + sizeWidth);
            if (comparePathNames(path, name) == 0) {
                return candidate.record;
            }
            for (int i = middle + 1;
                 i < count && index->searchTable[i].hash == wanted;
                 ++i) {
                const auto other = reinterpret_cast<const char*>(
                    index->searchTable[i].record + offsetWidth + sizeWidth);
                if (comparePathNames(name, other) == 0) {
                    return index->searchTable[i].record;
                }
            }
            for (int i = middle - 1;
                 i >= 0 && index->searchTable[i].hash == wanted;
                 --i) {
                const auto other = reinterpret_cast<const char*>(
                    index->searchTable[i].record + offsetWidth + sizeWidth);
                if (comparePathNames(name, other) == 0) {
                    return index->searchTable[i].record;
                }
            }
        }
        if (wanted < candidate.hash) {
            last = middle - 1;
        } else {
            first = middle + 1;
        }
    }
    return nullptr;
}

const char* findGameBigEntry(
    const GameBigIndex* index,
    const char* path,
    int entryOrdinal,
    std::uint64_t* offsetOut,
    std::uint64_t* sizeOut,
    const std::uint8_t** recordCursor,
    int* cursorOrdinal) noexcept
{
    int currentOrdinal = cursorOrdinal != nullptr ? *cursorOrdinal : -1;
    const std::uint8_t* record = recordCursor != nullptr ? *recordCursor : nullptr;
    if (currentOrdinal < 1 || entryOrdinal < currentOrdinal) {
        currentOrdinal = 0;
        record = index->entries + 0x10;
    }

    const std::uint8_t* header = index->header;
    const GameBigFormat format = detectGameBigFormat(header);
    int offsetWidth = 4;
    int sizeWidth = 4;
    fieldWidths(header, offsetWidth, sizeWidth);
    if (format == GameBigFormat::c0fb) {
        record = header + 6;
    }
    int trailerNumber = 0;
    const std::uint8_t* end =
        header + gameBigHeaderSize(header) - gameBigTrailerSize(index, &trailerNumber);

    const auto succeed = [&](const std::uint8_t* found) noexcept {
        setEntryOutputs(
            found, offsetWidth, sizeWidth, offsetOut, sizeOut, path == nullptr);
        if (recordCursor != nullptr) {
            *recordCursor = found;
        }
        if (cursorOrdinal != nullptr) {
            *cursorOrdinal = currentOrdinal;
        }
        return reinterpret_cast<const char*>(found + offsetWidth + sizeWidth);
    };

    if (path == nullptr) {
        while (record < end) {
            if (currentOrdinal == entryOrdinal) {
                return succeed(record);
            }
            record = nextRecord(record, offsetWidth, sizeWidth);
            ++currentOrdinal;
        }
    } else if (index->searchTable == nullptr) {
        const std::size_t wantedLength = std::strlen(path);
        while (record < end) {
            const char* name = reinterpret_cast<const char*>(record + offsetWidth + sizeWidth);
            if (std::strlen(name) == wantedLength && comparePathNames(name, path) == 0) {
                return succeed(record);
            }
            record = nextRecord(record, offsetWidth, sizeWidth);
            ++currentOrdinal;
        }
    } else {
        const std::uint8_t* found =
            findGameBigHashedEntry(index, path, offsetWidth, sizeWidth);
        if (found != nullptr) {
            return succeed(found);
        }
    }

    if (offsetOut != nullptr) {
        *offsetOut = 0;
    }
    if (sizeOut != nullptr) {
        *sizeOut = 0;
    }
    return nullptr;
}

}  // namespace ra3::io
