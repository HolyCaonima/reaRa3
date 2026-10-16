#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

namespace ra3::io {

enum class GameBigFormat : std::uint32_t {
    big = 0,
    c0fb = 1,
    viv4 = 2,
    invalid = 3
};

// rw::core allocator interface used through GameBigHandler +0x1C. The first
// slot is not identified; +0x08 allocates tagged blocks and +0x0C frees them.
struct GameAllocatorVtable {
    void* unknown00;
    void* allocateTemporary;
    void* allocateTagged;
    void* freeTagged;
};

struct GameAllocator {
    const GameAllocatorVtable* vtable;
};

// The archive file object is an rw::core file implementation. BigHandler uses
// virtual read +0x14 and seek +0x1C; the other slots remain deliberately opaque.
struct GameArchiveFileVtable {
    std::array<void*, 5> unknown00;
    void* read;
    void* unknown18;
    void* seek;
};

struct GameArchiveFile {
    const GameArchiveFileVtable* vtable;
};

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
    GameArchiveFile* file;
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
    GameAllocator* allocator;
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
    union {
        std::uint64_t logicalSize;
        std::uint64_t storedSize;  // Compatibility name used by the round-2 helper.
    };
    union {
        std::uint64_t compressedSize;
        std::uint64_t expandedSize;  // Compatibility name used by the round-2 seek port.
    };
    std::uint64_t sourcePosition;
    std::uint64_t logicalPosition;
};

// rw::core::codec::RefpackInflate object used by ra3_1.0.game 0x009F7620.
// Pointer fields are deliberately kept as pointers: original and replacement
// Read calls may operate on an object created by the other implementation.
struct GameRefPackInflater {
    const void* vtable;
    std::uint32_t unknown04;
    GameAllocator* allocator;
    std::uint8_t initialized;
    std::array<std::uint8_t, 3> padding0D;
    std::uint32_t unknown10;
    std::uint32_t inputRemaining;
    std::uint32_t outputRemaining;
    std::uint32_t unknown1C;
    std::uint64_t outputProduced;
    const std::uint8_t* inputBegin;
    std::uint8_t* outputBegin;
    const std::uint8_t* inputCursor;
    std::uint8_t* outputCursor;
    std::uint32_t state;
    std::uint32_t nextState;
    std::uint32_t pendingInputBytes;
    std::uint32_t decodedLength;
    std::uint32_t runRemaining;
    std::uint32_t command;
    std::uint8_t* copyCursor;
    std::uint8_t* windowBegin;
    std::uint8_t* windowCursor;
    std::uint32_t unknown5C;
    std::uint64_t windowProduced;
    std::uint32_t windowSize;
    std::uint32_t status;
};

// 0x009F5ED0 / 0x009F6670 enumeration cursor and the known result prefix.
struct GameBigFindState {
    GameBigArchive* archive;
    std::int32_t ordinal;
    std::int32_t recordOrdinal;
    const std::uint8_t* recordCursor;
    std::array<char, 0x100> pattern;
};

struct GameBigFindResult {
    std::uint32_t type;
    std::array<std::uint8_t, 0x1C> unknown04;
    std::uint64_t size;
    char name[1];
};

using GameBigRangeRead = std::uint32_t (*)(
    void* context,
    const GameBigArchive* archive,
    std::uint64_t memberOffset,
    void* destination,
    std::uint32_t count);

struct GameBigReleaseOps {
    void* context = nullptr;
    void (*destroyInflater)(void* context, void* inflater) = nullptr;
    void (*release)(void* context, void* pointer) = nullptr;
};

struct GameBigIoOps {
    void* context = nullptr;
    void (*lock)(void* context, const GameBigArchive* archive) = nullptr;
    void (*unlock)(void* context, const GameBigArchive* archive) = nullptr;
    void (*seek)(void* context, std::uint64_t position) = nullptr;
    std::uint32_t (*read)(
        void* context, void* destination, std::uint32_t count) = nullptr;
    GameRefPackInflater* (*createInflater)(void* context) = nullptr;
    void* (*allocateTemporary)(void* context, std::uint32_t size) = nullptr;
};

static_assert(sizeof(GameBigSearchEntry) == 0x08);
static_assert(offsetof(GameBigSearchEntry, record) == 0x04);
static_assert(sizeof(GameBigIndex) == 0x0C);
static_assert(offsetof(GameBigIndex, searchTable) == 0x08);
static_assert(sizeof(GameBigArchive) == 0x68);
static_assert(offsetof(GameBigArchive, index) == 0x0C);
static_assert(offsetof(GameBigArchive, archiveName) == 0x18);
static_assert(offsetof(GameBigArchive, rangeBegin) == 0x20);
static_assert(offsetof(GameBigArchive, streamBase) == 0x28);
static_assert(offsetof(GameBigArchive, format) == 0x30);
static_assert(offsetof(GameBigArchive, synchronization) == 0x38);
static_assert(sizeof(GameBigHandler) == 0x20);
static_assert(offsetof(GameBigHandler, head) == 0x10);
static_assert(offsetof(GameBigHandler, archiveCount) == 0x18);
static_assert(offsetof(GameBigHandler, allocator) == 0x1C);
static_assert(sizeof(GameBigFile) == 0x40);
static_assert(offsetof(GameBigFile, inflater) == 0x0C);
static_assert(offsetof(GameBigFile, dataOffset) == 0x18);
static_assert(offsetof(GameBigFile, logicalSize) == 0x20);
static_assert(offsetof(GameBigFile, compressedSize) == 0x28);
static_assert(offsetof(GameBigFile, sourcePosition) == 0x30);
static_assert(offsetof(GameBigFile, logicalPosition) == 0x38);
static_assert(sizeof(GameRefPackInflater) == 0x70);
static_assert(offsetof(GameRefPackInflater, inputRemaining) == 0x14);
static_assert(offsetof(GameRefPackInflater, outputProduced) == 0x20);
static_assert(offsetof(GameRefPackInflater, state) == 0x38);
static_assert(offsetof(GameRefPackInflater, windowBegin) == 0x54);
static_assert(offsetof(GameRefPackInflater, windowProduced) == 0x60);
static_assert(offsetof(GameRefPackInflater, status) == 0x6C);
static_assert(sizeof(GameBigFindState) == 0x110);
static_assert(offsetof(GameBigFindState, pattern) == 0x10);
static_assert(offsetof(GameBigFindResult, size) == 0x20);
static_assert(offsetof(GameBigFindResult, name) == 0x28);

// ra3_1.0.game 0x009F6810 / 0x009F6880.
std::uint32_t readGameBigEndian32(const std::uint8_t* bytes, int width) noexcept;
std::uint64_t readGameBigEndian64(const std::uint8_t* bytes, int width) noexcept;

// ra3_1.0.game 0x009F6910 / 0x009F6A50 / 0x009F6AA0 / 0x009F6B00.
GameBigFormat detectGameBigFormat(const std::uint8_t* header) noexcept;
int gameBigHeaderSize(const std::uint8_t* header) noexcept;
std::uint32_t gameBigEntryCount(const GameBigIndex* index) noexcept;
int gameBigTrailerSize(const GameBigIndex* index, int* numberOut) noexcept;

// ra3_1.0.game 0x009F6990. The returned DJB2 value is byte-swapped.
std::uint32_t hashGameBigPath(const char* path, std::uint32_t seed = 0x1505) noexcept;

// ra3_1.0.game 0x009F5E80 / 0x009F5E90.
std::uint64_t gameBigStoredSize(const GameBigFile* file) noexcept;
std::uint64_t gameBigAbsoluteDataOffset(const GameBigFile* file) noexcept;

// ra3_1.0.game 0x009F5F80 state construction without allocation. The range
// reader is optional; without it compressed-size probing is skipped.
bool prepareGameBigFile(
    const GameBigHandler* handler,
    const char* path,
    std::uint32_t ownerToken,
    std::uint32_t openMode,
    GameBigRangeRead read,
    void* readContext,
    GameBigFile* output) noexcept;

// State-only part of ra3_1.0.game 0x009F5E00. Returns true when the caller must
// destroy inflater and release temporaryBuffer after applying the transition.
bool applyGameBigSeekState(GameBigFile* file, std::uint64_t position) noexcept;

// Stateful release portions of ra3_1.0.game 0x009F5E00 / 0x009F5DB0.
std::uint64_t seekGameBigFile(
    GameBigFile* file,
    std::uint64_t position,
    const GameBigReleaseOps& operations) noexcept;
void closeGameBigFile(
    GameBigFile* file,
    const GameBigReleaseOps& operations) noexcept;

// Bookkeeping prediction used by bounded Read shadow verification. `actual`
// is the number of bytes returned by the archive/decoder.
void applyGameBigReadResult(GameBigFile* file, std::uint32_t actual) noexcept;

// Layout-compatible state-machine operations for ra3_1.0.game
// 0x009F75C0 / 0x009F7620 / 0x009F7C50. `vtable` is supplied by the ABI
// boundary so the same portable implementation works in the DLL and oracles.
void initializeGameRefPackInflater(
    GameRefPackInflater* inflater,
    GameAllocator* allocator,
    const void* vtable) noexcept;
void setGameRefPackInput(
    GameRefPackInflater* inflater,
    const std::uint8_t* input,
    std::uint32_t count) noexcept;
void setGameRefPackOutput(
    GameRefPackInflater* inflater,
    std::uint8_t* output,
    std::uint32_t count) noexcept;
std::uint32_t processGameRefPackInflater(GameRefPackInflater* inflater) noexcept;
std::uint32_t runGameRefPackInflater(GameRefPackInflater* inflater) noexcept;
void destroyGameRefPackInflater(GameRefPackInflater* inflater) noexcept;

// ra3_1.0.game 0x009F6280 / 0x009F63B0. I/O, synchronization and allocation
// are supplied by the verified ABI boundary or by offline oracle stubs.
std::uint32_t inflateGameBigRange(
    GameBigFile* file,
    void* destination,
    std::uint32_t destinationCapacity,
    std::uint32_t wanted,
    std::uint32_t sourceChunkSize,
    bool finalFlush,
    const GameBigIoOps& operations) noexcept;
std::uint32_t readGameBigFile(
    GameBigFile* file,
    void* destination,
    std::uint32_t count,
    const GameBigIoOps& operations) noexcept;

// ra3_1.0.game 0x004CE600 / 0x009F5ED0 / 0x009F6670.
bool matchGameBigPattern(const char* text, const char* pattern) noexcept;
void initializeGameBigFindState(
    GameBigFindState* state,
    const char* pattern,
    GameBigArchive* firstArchive) noexcept;
bool findNextGameBig(
    GameBigFindState* state,
    GameBigFindResult* result) noexcept;

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
