#pragma once

#include <cstdint>

namespace ra3::gameclient {

// Same xfer stand-in as ra3::gamelogic::XferOps. Duplicated so gameclient does
// not depend on gamelogic. Slot numbers are the virtual byte offsets.
struct XferOps {
    void* xfer{};
    void (*version)(void* xfer, std::uint8_t* pair) noexcept {};
    void (*field20)(void* xfer, void* field) noexcept {};
    void (*field58)(void* xfer, void* field) noexcept {};
    void (*field64)(void* xfer, void* field) noexcept {};
    void (*field6c)(void* xfer, void* field) noexcept {};
    void (*field70)(void* xfer, void* field) noexcept {};
    void (*field84)(void* xfer, void* field) noexcept {};
    void (*field88)(void* xfer, void* field, std::uint32_t name,
                    std::uint32_t count) noexcept {};
};

// AnimationSoundClientBehavior 00C4FE40 slot 3.
void xferAnimationSoundClientBehavior(std::uint8_t* self, XferOps& ops) noexcept;

// ModelConditionAudioLoopClientBehavior 00C500B8 slot 3.
// The same body is also slot 3 of the eva and upgrade selectors.
void xferModelConditionClientBehavior(std::uint8_t* self, XferOps& ops) noexcept;

// RandomSoundSelectorClientBehavior 00C50318 slot 3.
void xferRandomSoundSelector(std::uint8_t* self, XferOps& ops) noexcept;

// LargeGroupAudioClientUpdate 00C503E0 slot 3.
void xferLargeGroupAudio(std::uint8_t* self, XferOps& ops) noexcept;

// BeaconClientUpdate 00C4FF48 slot 3.
void xferBeaconClientUpdate(std::uint8_t* self, XferOps& ops) noexcept;

// AnimationSoundClientBehavior 00C4FE40 slot 13. Null this returns 0.
std::uint32_t clientBehaviorRecordOrNull(const std::uint8_t* self) noexcept;

// ModelConditionAudioLoopClientBehavior 00C500B8 slots 10 and 11.
std::uint32_t audioLoopRecordOrNull(const std::uint8_t* self) noexcept;

}  // namespace ra3::gameclient
