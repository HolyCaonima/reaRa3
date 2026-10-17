#pragma once

#include <cstdint>

namespace ra3::audio {

// Clean SageAudio device helpers in 0x00500000-0x0050FFFF.
// 00BDF5A8 is the SageAudioManager instance vtable: 0051BA00 stores it at
// object+0 and stores 00BDF594 (slot 2 returns "SageAudioManager") at +0x24.
// 00BDF3A4 slot 2 returns "SagePathMusicManager".

constexpr std::uint32_t kSageAudioManagerNameAddress = 0x00BDEDB4u;
constexpr std::uint32_t kSagePathMusicManagerNameAddress = 0x00BDEDC8u;
constexpr std::uint32_t kEmptyAudioEventAddress = 0x00CAA11Cu;
constexpr std::uint32_t kOneFloatAddress = 0x00BD90A4u;
constexpr std::uint32_t kCategoryScaleTableAddress = 0x00CA9F04u;
constexpr std::uint32_t kListenerCountAddress = 0x00CA7D7Cu;
constexpr std::uint32_t kSpeakerModeAddress = 0x00CA7E7Cu;
constexpr std::uint32_t kAudioSystemAddress = 0x00DF6098u;
constexpr std::uint32_t kAudioSettingsAddress = 0x00DF03F8u;
constexpr std::uint32_t kVolumeLockAddress = 0x00DEE1C8u;
constexpr std::uint32_t kPlayerAudioBaseAddress = 0x00DEE290u;
constexpr std::uint32_t kPlayerAudioStride = 0x1FCu;

// 0x00502270. Called from 00515050, itself called from RenderingSound.cpp 00515B70.
std::uint32_t resolveSpeakerCenterChannel() noexcept;

// 0x00502FB0. 00BDF5A8 slot 73. Returns the empty-event address.
std::uint32_t sageAudioManagerEmptyEventAddress() noexcept;

// 0x00502FF0. 00BDF5A8 slot 4. Forwards to vtable slot 14 with argument 1.
std::uint32_t sageAudioManagerStopAllImmediately(void* manager) noexcept;

// 0x00503290. 00BDF5A8 slot 20. Reads the listener-count global.
std::uint32_t sageAudioManagerListenerOrientationCount() noexcept;

// 0x00503300. 00BDF594 slot 2.
std::uint32_t sageAudioManagerSubsystemNameAddress() noexcept;

// 0x00503400. 00BDF3A4 slot 2.
std::uint32_t sagePathMusicManagerSubsystemNameAddress() noexcept;

// 0x005044F0. Callee of SageAudioManager.cpp 0051E560. ret 4, returns this.
// The stack argument is the address of the event pointer (caller passes &object+0x14).
void* initAudioEventHandle(void* handle, void* eventSlot) noexcept;

// 0x00504560. Callee of SageAudioManager.cpp 005161C0.
std::uint32_t releaseAudioEventHandle(void* handle) noexcept;

// 0x005045C0. Callee of SageAudioManager.cpp 0051E560.
// Preserves EAX when the handle is empty or the count stays positive.
std::uint32_t releasePlayingAudioHandle(void* handle, std::uint32_t incomingEax) noexcept;

// 0x005045E0. Callee of 00519740, itself called from the EASTL-anchored 00519C50.
std::uint32_t unlinkPlayingAudioNode(void* node) noexcept;

// 0x00504AC0. Callee of SageAudioManager.cpp 0051D3C0. ret 4.
std::uint32_t testAudioEventCategoryBit(const void* eventInfo, std::uint32_t bitIndex) noexcept;

// 0x00504AF0. Callee of 00508510, itself called from RenderingSound.cpp 00515B70.
std::uint32_t intersectsAudioEventCategoryMask(const void* eventInfo,
                                               const std::uint32_t* mask) noexcept;

// 0x00504C80. Callee of 00505340. thiscall, two stack arguments, ret 8.
// EAX is a leftover stack temporary; callers use the written fields.
void recomputePlayerCategoryVolume(void* player, std::uint32_t category,
                                   std::uint32_t subchannel) noexcept;

// 0x00504DF0. Callee of 00505340. ret 4.
// Returns the local-vector end pointer, or the audio-settings pointer once that
// object is loaded. Callers ignore the value.
std::uint32_t recomputePlayerCategoryProduct(void* player, std::uint32_t category) noexcept;

// 0x00504EC0. Callee of 00BDF5A8 slot 5 (0051EFF0). Returns this+0x1E2.
std::uint32_t decrementPlayerVolumeCounters(void* player) noexcept;

// 0x00505340 / 0x005053B0. Callers clean the one stack argument.
std::uint32_t acquireVolumeModifierLock(const void* modifierGuard) noexcept;
std::uint32_t releaseVolumeModifierLock(const void* modifierGuard) noexcept;

// 0x00506670 / 0x00506680. 00BDF5A8 slots 71 and 72. ret 4.
std::uint32_t sageAudioManagerOnAcquireVolumeModifierLock(const void* modifierGuard) noexcept;
std::uint32_t sageAudioManagerOnReleaseVolumeModifierLock(const void* modifierGuard) noexcept;

// 0x00506430. 00BDF5A8 slot 101. ret 4. Ignores the manager this.
std::uint32_t sageAudioManagerQuerySuppressed(void* request,
                                              std::uint32_t incomingEax) noexcept;

// 0x00506C20. Callee of vtable 00BDC45C slot 2. ret 0x10, returns the bitset.
void* initCategoryBitsFour(void* bitset, std::uint32_t bit0, std::uint32_t bit1,
                           std::uint32_t bit2, std::uint32_t bit3) noexcept;

// 0x00506C90. Callee of 00508510. ret 8, returns the bitset.
void* initCategoryBitsTwo(void* bitset, std::uint32_t bit0, std::uint32_t bit1) noexcept;

// 0x00502540. Callee of SagePathMusicManager 00BDF3A4 slot 1. ret 4.
std::uint32_t bindSoundProvider(void* provider, const void* name) noexcept;

// 0x00507720. Callee of SageAudioManager.cpp 0051D3C0.
std::uint32_t hasActiveRenderCursor(const void* playingAudio) noexcept;

// 0x00508640. Callee of 005181E0, reached from the EASTL-anchored 00519740. ret 4.
void* appendPlayingAudioNode(void* node, void* owner) noexcept;

// 0x0050B090. 00BDF3A4 slot 3. ret 4.
std::uint32_t sagePathMusicManagerTransferState(void* manager, void* stream) noexcept;

// 0x0050DF80. Tail callee of 00BDF5A8 slots 92 and 94. ret 4.
std::uint32_t eraseListenerRecord(void* cache, std::uint32_t key) noexcept;

// 0x0050E510. 00BDF5A8 slots 92 and 94. ret 4.
std::uint32_t sageAudioManagerRemoveListenerEntry(void* manager, std::uint32_t key,
                                                  std::uint32_t incomingEax) noexcept;

// 0x0050FA80. Callee of 0050FD50. ret 8. Returns the source-vector end pointer.
std::uint32_t applyListedVolumeModifiers(void* player, void* listHolder,
                                         std::uint32_t playerIndex) noexcept;

// 0x0050FD50. Direct callee of RenderingSound.cpp 00515B70.
std::uint32_t refreshRenderingPlayerVolumes(void* rendering) noexcept;

}  // namespace ra3::audio
