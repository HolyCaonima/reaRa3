#include "ra3/gameclient/SkirmishClient87.h"

#include <cstddef>
#include <cstring>

namespace ra3::gameclient {
namespace {

template <typename T>
T load(const void* base, std::ptrdiff_t offset) noexcept {
    T value{};
    std::memcpy(&value, static_cast<const std::uint8_t*>(base) + offset, sizeof(T));
    return value;
}

// 0x008D82F0: two version snapshots, both starting at {1, 1}. Outside this
// batch, so the calls are inlined. Their results are discarded.
void xferClientModuleBase(XferOps& ops) noexcept {
    std::uint8_t first[2] = {1, 1};
    ops.version(ops.xfer, first);
    std::uint8_t second[2] = {1, 1};
    ops.version(ops.xfer, second);
}

std::uint8_t versionByte(XferOps& ops, std::uint8_t current, std::uint8_t supported) noexcept {
    std::uint8_t pair[2] = {current, supported};
    ops.version(ops.xfer, pair);
    return pair[1];
}

bool xferFlagBit3Clear(const XferOps& ops) noexcept {
    const auto flags = load<std::uint32_t>(ops.xfer, 4);
    return ((flags >> 3) & 1u) == 0u;
}

}  // namespace

void xferAnimationSoundClientBehavior(std::uint8_t* self, XferOps& ops) noexcept {
    xferClientModuleBase(ops);
    const auto fileVersion = versionByte(ops, 1, 2);
    if (fileVersion >= 2) {
        ops.field84(ops.xfer, self + 0x14);
    }
}

void xferModelConditionClientBehavior(std::uint8_t* /*self*/, XferOps& ops) noexcept {
    xferClientModuleBase(ops);
    versionByte(ops, 1, 1);
}

void xferRandomSoundSelector(std::uint8_t* self, XferOps& ops) noexcept {
    xferClientModuleBase(ops);
    versionByte(ops, 1, 1);
    ops.field64(ops.xfer, self + 0x10);
    ops.field6c(ops.xfer, self + 0x14);
}

void xferLargeGroupAudio(std::uint8_t* self, XferOps& ops) noexcept {
    versionByte(ops, 1, 1);
    xferClientModuleBase(ops);
    versionByte(ops, 1, 1);
    if (xferFlagBit3Clear(ops)) {
        ops.field84(ops.xfer, self + 0x7D);
    }
}

void xferBeaconClientUpdate(std::uint8_t* self, XferOps& ops) noexcept {
    versionByte(ops, 1, 1);
    xferClientModuleBase(ops);
    versionByte(ops, 1, 1);
    if (xferFlagBit3Clear(ops)) {
        // 0x0054A030 forwards to slot +0x88 with the ParticleSystemID name.
        ops.field88(ops.xfer, self + 0x0C, 0x00BE1CDCu, 4u);
        ops.field6c(ops.xfer, self + 0x10);
    }
}

std::uint32_t clientBehaviorRecordOrNull(const std::uint8_t* self) noexcept {
    if (self == nullptr) {
        return 0u;
    }
    return static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(self + 0x0C));
}

std::uint32_t audioLoopRecordOrNull(const std::uint8_t* self) noexcept {
    if (self == nullptr) {
        return 0u;
    }
    return static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(self + 0x10));
}

}  // namespace ra3::gameclient
