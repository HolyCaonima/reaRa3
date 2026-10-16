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
