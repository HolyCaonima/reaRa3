#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

namespace ra3::io {

// ra3_1.0.game 0x009F7130 / 0x009F6B90.
struct GameBigSearchEntry {
    std::uint32_t hash;
    const std::uint8_t* record;
};

// Embedded at GameBigArchive +0x0C. Constructed by ra3_1.0.game 0x009F6A20.
struct GameBigIndex {
    const std::uint8_t* header;
    const std::uint8_t* entries;
    const GameBigSearchEntry* searchTable;
};

// Allocated as 0x68 bytes by ra3_1.0.game 0x009F5AF0 and initialized at
// 0x009F5A80. Only fields observed in the clean BigHandler cluster are named.
struct GameBigArchive {
    GameBigArchive* next;
    void* file;
    std::uint8_t* ownedHeader;
    GameBigIndex index;
    const char* archiveName;
    std::uint32_t unknown1C;
    std::uint64_t rangeBegin;
    std::uint64_t streamBase;
    std::uint32_t format;
    std::uint32_t unknown34;
    std::array<std::uint8_t, 0x30> synchronization;
};

// Base layout initialized by ra3_1.0.game 0x009F5F40. The live derived object
// is allocated by 0x004D2490 and stored through the global at 0x00DEB69C.
struct GameBigHandler {
    const void* vtable;
    std::array<std::uint32_t, 3> baseFields;
    GameBigArchive* head;
    GameBigArchive* tail;
    std::uint32_t archiveCount;
    void* allocator;
};

// File object allocated by ra3_1.0.game 0x009F5F80. Its virtual operations are
// close 0x009F5DB0, read 0x009F63B0, seek 0x009F5E00 and size 0x009F5E80.
struct GameBigFile {
    GameBigArchive* archive;
    std::uint32_t ownerToken;
    std::uint32_t openMode;
    void* inflater;
    void* temporaryBuffer;
    std::uint32_t unknown14;
    std::uint64_t dataOffset;
    std::uint64_t storedSize;
    std::uint64_t expandedSize;
    std::uint64_t sourcePosition;
    std::uint64_t logicalPosition;
};

static_assert(sizeof(GameBigSearchEntry) == 0x08);
static_assert(offsetof(GameBigSearchEntry, record) == 0x04);
static_assert(sizeof(GameBigIndex) == 0x0C);
static_assert(offsetof(GameBigIndex, searchTable) == 0x08);
static_assert(sizeof(GameBigArchive) == 0x68);
static_assert(offsetof(GameBigArchive, index) == 0x0C);
static_assert(offsetof(GameBigArchive, archiveName) == 0x18);
static_assert(offsetof(GameBigArchive, format) == 0x30);
static_assert(sizeof(GameBigHandler) == 0x20);
static_assert(offsetof(GameBigHandler, head) == 0x10);
static_assert(offsetof(GameBigHandler, archiveCount) == 0x18);
static_assert(sizeof(GameBigFile) == 0x40);
static_assert(offsetof(GameBigFile, dataOffset) == 0x18);
static_assert(offsetof(GameBigFile, logicalPosition) == 0x38);

// ra3_1.0.game 0x009F6990. The returned DJB2 value is byte-swapped.
std::uint32_t hashGameBigPath(const char* path) noexcept;

// ra3_1.0.game 0x009F6B90. Returns the start of an entry record or nullptr.
const std::uint8_t* findGameBigHashedEntry(
    const GameBigIndex* index,
    const char* path,
    int offsetWidth,
    int sizeWidth) noexcept;

// ra3_1.0.game 0x009F6D00. With path != nullptr this finds by path; otherwise
// it returns entryOrdinal. On success the return value points to the stored
// NUL-terminated path. recordCursor/cursorOrdinal are optional in/out cursors.
const char* findGameBigEntry(
    const GameBigIndex* index,
    const char* path,
    int entryOrdinal,
    std::uint64_t* offsetOut,
    std::uint64_t* sizeOut,
    const std::uint8_t** recordCursor,
    int* cursorOrdinal) noexcept;

}  // namespace ra3::io
