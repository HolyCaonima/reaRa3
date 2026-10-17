#pragma once

#include <cstddef>
#include <cstdint>

namespace ra3::audio {

// Reconstructed clean functions in 0x00500000-0x0051FFFF belonging to the
// SageAudioManager / PerPlayerAudioSettings / SagePathMusicManager /
// SageAudioCache / RenderingSound / PlayingAudio cluster.
// Reference: Generals/Code/GameEngine/Source/Common/Audio/AudioManager.cpp,
// Generals/Code/GameEngineDevice/Source/MilesAudioDevice/MilesAudioManager.cpp.

// 0x00502270: RenderingSound speaker-mode helper (direct callee of 0x00515050 at 0x00515423).
std::uint32_t resolveSpeakerCenterChannelEnabled(std::int32_t modeOverride,
                                                 const void* speakerSystem,
                                                 std::uint32_t initialEax = 0) noexcept;

// 0x00502F00: RenderingSound::pushSampleDuration (direct callee of 0x0050A7A0 at 0x0050A8D7).
std::uint32_t pushRenderingSoundSampleDuration(void* renderingSound, float duration,
                                               std::uint32_t initialEax = 0) noexcept;

// 0x00502FB0: 00BDF5A8 slot 73 (SageAudioManager::emptyAudioEventRTS).
std::uint32_t sageAudioManagerEmptyEventAddress() noexcept;

// 0x00502FF0: 00BDF5A8 slot 4 (SageAudioManager::stopAllAudioImmediately).
std::uint32_t sageAudioManagerStopAllImmediately(void* manager) noexcept;

// 0x00503000: 00BDF5A8 slot 102 (SageAudioManager::updateFrameDeltaSeconds).
float sageAudioManagerUpdateFrameDeltaSeconds(void* manager, std::uint32_t listenerCount,
                                              std::uint8_t pausedFlag, void* timerSource,
                                              float defaultStep,
                                              std::uint32_t* outEax = nullptr,
                                              std::uint32_t initialEax = 0) noexcept;

// 0x00503290: 00BDF5A8 slot 20 (SageAudioManager::listenerOrientationCount).
std::uint32_t sageAudioManagerListenerOrientationCount(
    const std::uint32_t* globalListenerCount) noexcept;

// 0x00503300: 00BDF594 slot 2 (SageAudioManager::subsystemName).
const char* sageAudioManagerSubsystemNameLiteral() noexcept;
std::uint32_t sageAudioManagerSubsystemNameAddress() noexcept;

// 0x00503400: 00BDF3A4 slot 2 (SagePathMusicManager::subsystemName).
const char* sagePathMusicManagerSubsystemNameLiteral() noexcept;
std::uint32_t sagePathMusicManagerSubsystemNameAddress() noexcept;

// 0x005044F0: AudioEventRTSHandle::initFromPointer (direct callee of 0x0051E560 at 0x0051E698).
void* initAudioEventRtsHandle(void* handleStorage, void* eventRts) noexcept;

// 0x00504560: AudioEventRTSHandle::release (direct callee of 0x0051E560 at 0x0051E6B5).
std::uint32_t releaseAudioEventRtsHandle(void* handleStorage) noexcept;

// 0x00504590: PlayingAudioHandle::releaseAndClear (direct callee of 0x0051ACD0 at 0x0051AF72).
std::uint32_t releaseAndClearPlayingAudioHandle(void* handleStorage,
                                                std::uint32_t initialEax = 0) noexcept;

// 0x005045C0: PlayingAudioHandle::release (direct callee of 0x0051E560 at 0x0051E9F4).
std::uint32_t releasePlayingAudioHandle(void* handleStorage,
                                        std::uint32_t initialEax = 0) noexcept;

// 0x005045E0: PlayingAudioNode::unlinkFromList (direct callee of 0x00519740 at 0x005198E9).
std::uint32_t unlinkPlayingAudioNode(void* node) noexcept;

// 0x00504AC0: AudioEventRTS::testCategoryBit (direct callee of 0x0051D790 at 0x0051D8F4).
std::uint32_t testAudioEventCategoryBit(const void* eventInfo,
                                        std::uint32_t bitIndex) noexcept;

// 0x00504AF0: AudioEventRTS::intersectsCategoryMask (direct callee of 0x00508510 at 0x005085E2).
std::uint32_t intersectsAudioEventCategoryMask(const void* eventInfo,
                                               const std::uint32_t* mask) noexcept;

// 0x00504B20: PlayingAudioPoolNode::initEntry (direct callee of 0x00510AC0 at 0x00510AFE).
void* initPlayingAudioPoolNode(void* node, void* nextNode, void* prevNode,
                               void* refCountedPayload) noexcept;

// 0x00504C80: PerPlayerAudioSettings::recomputeFinalVolume (direct callee of 0x00519680 at 0x005196D8).
std::uint32_t recomputePlayerCategoryFinalVolume(
    void* playerSettings, std::uint32_t category, std::uint32_t subchannel,
    const void* globalAudioSettings, const float* globalCategoryScaleTable) noexcept;

// 0x00504DF0: PerPlayerAudioSettings::recomputeCategoryModifierProduct (direct callee of 0x00505340 at 0x00505375).
std::uint32_t recomputePlayerCategoryModifierProduct(
    void* playerSettings, std::uint32_t category,
    std::int32_t globalModifierLockCount, const void* globalAudioSettings) noexcept;

// 0x00504EC0: PerPlayerAudioSettings::decrementVolumeChangeCounters (direct callee of 0x0051EFF0 at 0x0051F182).
std::uint32_t decrementPlayerVolumeChangeCounters(void* playerSettings) noexcept;

// 0x00504F20: PerPlayerAudioSettings::hasPendingVolumeChangeForEvent (direct callee of 0x005159F0 at 0x00515B0F).
std::uint32_t hasPendingVolumeChangeForEvent(const void* playerSettings,
                                             void* const* eventInfoPtr,
                                             std::uint32_t subchannel) noexcept;

// 0x00504F80: PerPlayerAudioSettings::updateDuckingMultiplierFromDistance (direct callee of 0x005077C0 at 0x00507B20).
std::uint32_t updatePlayerDuckingMultiplierFromDistance(
    void* playerSettings, const void* duckingParams, const float* deltaVector3,
    const void* globalAudioSettings, const float* globalCategoryScaleTable) noexcept;

// 0x00505150: PerPlayerAudioSettings::applyVolumeSettingsByFlags (direct callee of 0x00506690 at 0x005066AD).
std::uint32_t applyPlayerVolumeSettingsByFlags(
    void* playerSettings, float value, std::uint8_t flags,
    const void* globalAudioSettings, const float* globalCategoryScaleTable) noexcept;

// 0x00505340: SageAudioManager::acquireGlobalVolumeModifierLock (direct callee of 0x00506670 at 0x00506675).
std::uint32_t acquireGlobalVolumeModifierLock(
    const void* modifierGuard, std::int32_t& globalModifierLockCount,
    void* playerSettingsArray3, const void* globalAudioSettings,
    const float* globalCategoryScaleTable) noexcept;

// 0x005053B0: SageAudioManager::releaseGlobalVolumeModifierLock (direct callee of 0x00506680 at 0x00506685).
std::uint32_t releaseGlobalVolumeModifierLock(
    const void* modifierGuard, std::int32_t& globalModifierLockCount,
    void* playerSettingsArray3, const void* globalAudioSettings,
    const float* globalCategoryScaleTable) noexcept;

// 0x00506670: 00BDF5A8 slot 71 (SageAudioManager::onAcquireVolumeModifierLock).
std::uint32_t sageAudioManagerOnAcquireVolumeModifierLock(
    const void* modifierGuard, std::int32_t& globalModifierLockCount,
    void* playerSettingsArray3, const void* globalAudioSettings,
    const float* globalCategoryScaleTable) noexcept;

// 0x00506680: 00BDF5A8 slot 72 (SageAudioManager::onReleaseVolumeModifierLock).
std::uint32_t sageAudioManagerOnReleaseVolumeModifierLock(
    const void* modifierGuard, std::int32_t& globalModifierLockCount,
    void* playerSettingsArray3, const void* globalAudioSettings,
    const float* globalCategoryScaleTable) noexcept;

// 0x00506C20: AudioCategoryBitset::initFromFourBits (direct callee of 0x00508FF0 at 0x00509023).
void* initCategoryBitsetFromFourBits(void* bitset, std::uint32_t bit0,
                                     std::uint32_t bit1, std::uint32_t bit2,
                                     std::uint32_t bit3) noexcept;

// 0x00506C90: AudioCategoryBitset::initFromTwoBits (direct callee of 0x00508510 at 0x00508594).
void* initCategoryBitsetFromTwoBits(void* bitset, std::uint32_t bit0,
                                    std::uint32_t bit1) noexcept;

// 0x00506CD0: PlayingAudioNode::unlinkIfOwned (direct callee of 0x00513CF0 at 0x00513D5F).
std::uint32_t unlinkPlayingAudioNodeIfOwned(void* node,
                                            std::uint32_t initialEax = 0) noexcept;

// 0x00507720: PlayingAudio::hasActiveRenderCursor (direct callee of 0x0051D3C0 at 0x0051D6BB).
std::uint32_t hasActiveRenderCursor(const void* playingAudioSlot) noexcept;

// 0x00508330: RenderingSound::sumWindowSamplePlayheadRatios (direct callee of 0x00514A00 at 0x00514B8A).
double sumWindowSamplePlayheadRatios(const void* renderingSound,
                                     std::uint32_t* outEax = nullptr) noexcept;

// 0x005084B0: RenderingSound::sumAllChannelSamplePlayheadRatios (direct callee of 0x00514870 at 0x00514876).
double sumAllChannelSamplePlayheadRatios(const void* renderingSound,
                                         std::uint32_t* outEax = nullptr) noexcept;

// 0x00508640: PlayingAudioNode::appendToOwnerList (direct callee of 0x005181E0 at 0x005182DD).
void* appendPlayingAudioNodeToOwnerList(void* node, void* owner) noexcept;

// 0x00508AF0: SageAudioManager::findPlayingAudioByHandle (direct callee of 0x0051E560 at 0x0051E5CB).
void* findPlayingAudioByHandle(void* manager, void** outHandle,
                               const std::uint32_t* targetIdPtr) noexcept;

// 0x00508D50: SageAudioManager::hasActiveEventForObjectOrDrawable (direct callee of 0x0050AD80 at 0x0050ADBB).
std::uint32_t hasActiveEventForObjectOrDrawable(const void* manager,
                                                std::uint32_t objectId,
                                                std::uint32_t drawableId,
                                                std::uint32_t initialEax = 0) noexcept;

// 0x0050A220: PerPlayerAudioSettings::initializeDefaults (calls 0x00504C80 at 0x0050A3AC with same this).
void* initializePlayerAudioSettingsDefaults(void* playerSettings,
                                            void* playerSettingsArrayBase,
                                            const void* globalAudioSettings,
                                            const float* globalCategoryScaleTable) noexcept;

// 0x0050AD80: 00BDF5A8 slot 30 (SageAudioManager::isObjectOrDrawablePlayingAudio).
std::uint32_t isObjectOrDrawablePlayingAudio(const void* manager,
                                             std::uint32_t drawableId,
                                             void* gameClientLookup,
                                             std::uint32_t initialEax = 0) noexcept;

// 0x0050BE70: SageAudioCache::totalEntryCount (direct callee of 0x0050FDB0 at 0x0050FF4C).
std::uint32_t sageAudioCacheTotalEntryCount(const void* audioCache) noexcept;

// 0x0050DE20: PerPlayerAudioSettings::removeEventVolumeModifiers (direct callee of 0x0050E040 at 0x0050E05D).
std::uint32_t removePlayerEventVolumeModifiers(void* playerSettings,
                                               void* const* eventInfoPtr,
                                               std::uint32_t sourcePlayerIndex,
                                               std::int32_t globalModifierLockCount,
                                               const void* globalAudioSettings,
                                               const float* globalCategoryScaleTable) noexcept;

// 0x0050DF80: SageAudioCache::removeListenerEntry (direct callee of 0x0050E510 at 0x0050E517).
std::uint32_t removeAudioCacheListenerEntry(void* audioCache,
                                            std::uint32_t listenerKey) noexcept;

// 0x0050E040: PlayingAudio::detachPlayerVolumeModifiers (direct callee of 0x00513120 at 0x0051313C).
std::uint32_t detachPlayingAudioVolumeModifiers(void* playingAudio,
                                                void* playerSettingsArray3,
                                                std::int32_t globalModifierLockCount,
                                                const void* globalAudioSettings,
                                                const float* globalCategoryScaleTable) noexcept;

// 0x0050E510: 00BDF5A8 slot 92 & 94 (SageAudioManager::removeAudioListenerEntry).
std::uint32_t sageAudioManagerRemoveListenerEntry(void* manager,
                                                  std::uint32_t listenerKey,
                                                  std::uint32_t initialEax = 0) noexcept;

}  // namespace ra3::audio
