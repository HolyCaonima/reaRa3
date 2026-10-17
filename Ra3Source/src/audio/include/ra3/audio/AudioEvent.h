#pragma once

#include <cstddef>
#include <cstdint>

namespace ra3::audio {

struct Vector3 {
    float x;
    float y;
    float z;
};

enum class OwnerType : std::uint32_t {
    positional = 0,
    drawable = 1,
    object = 2,
    invalid = 3,
};

enum class PlayPortion : std::uint32_t {
    attack = 0,
    sound = 1,
    decay = 2,
    done = 3,
};

enum AudioAffect : std::uint8_t {
    music = 0x01,
    sound = 0x02,
    sound3D = 0x04,
    speech = 0x08,
    unknown10 = 0x10,
};

using RandomInteger = std::int32_t (*)(void* context, std::int32_t minimum,
                                       std::int32_t maximum);
using RandomReal = float (*)(void* context, float minimum, float maximum);
using GeneratePlayInfo = void (*)(void* context, void* event);
using ChannelInitializer = void (*)(void* context, void* state, std::uint32_t category,
                                    std::uint32_t channel);
using PlayerEnvelopeSetter = void (*)(void* context, void* playerState, float value,
                                      std::uint32_t mode);
using EnvelopeRefresher = void (*)(void* context, void* playerState);

// Non-owning view of the recovered RA3 AudioEventRTS storage. It never retains
// or releases the pointers embedded in the game object.
class AudioEventView {
public:
    static constexpr std::size_t minimumSize = 0x94;

    explicit AudioEventView(void* storage) noexcept;
    explicit AudioEventView(const void* storage) noexcept;

    void setPlayerIndex(std::uint32_t value) noexcept;             // 0x00559C10
    float minimumRange() const noexcept;                           // 0x00559C20
    void decrementDelay(float amount) noexcept;                    // 0x00559C30
    void setPosition(const Vector3* position) noexcept;             // 0x00559C50
    void setObjectId(std::uint32_t id) noexcept;                   // 0x00559C90
    std::uint32_t objectId() const noexcept;                       // 0x00559CB0
    void setDrawableId(std::uint32_t id) noexcept;                 // 0x00559CC0
    std::uint32_t drawableId() const noexcept;                     // 0x00559CE0
    void setShouldFade(bool value) noexcept;                       // 0x00559CF0
    bool shouldFade() const noexcept;                              // 0x00559D00
    void setMinimumRangeOverride(float value) noexcept;            // 0x00559D10
    void setMaximumRangeOverride(float value) noexcept;            // 0x00559D20
    void setVolume(float value) noexcept;                          // 0x00559D30
    float effectivePitch() const noexcept;                         // 0x0055A990
    bool canContinue() const noexcept;                             // 0x0055A9E0
    void advancePlayPortion() noexcept;                            // 0x0055B760

    void randomizePitchAndVolume(RandomReal random, void* context) noexcept; // 0x0055A5D0
    void randomizeDelay(RandomReal random, void* context) noexcept;          // 0x0055B5E0
    void randomizePostDelay(RandomReal random, void* context) noexcept;      // 0x0055B630
    void regenerate(RandomReal random, GeneratePlayInfo generate, void* context) noexcept;
                                                                        // 0x0055B6F0

private:
    std::byte* data_;
    const std::byte* constData_;
};

// Non-owning view of the fields used by recovered AudioEventInfo predicates.
class AudioEventInfoView {
public:
    static constexpr std::size_t minimumSize = 0x124;

    explicit AudioEventInfoView(const void* storage) noexcept;

    bool hasOverrideSlots() const noexcept;                        // 0x00559E40
    bool isPlayable() const noexcept;                              // 0x00559E60
    bool hasAnySamples() const noexcept;                           // 0x00559ED0
    std::uint32_t resolvedPriority() const noexcept;               // 0x00559F20
    bool matchesCategory(std::uint32_t category) const noexcept;   // 0x00559FA0
    std::int32_t sampleIndex(std::uint32_t sample) const noexcept; // 0x0055A020

private:
    const std::byte* data_;
};

class SageAudioManagerView {
public:
    static constexpr std::size_t minimumSize = 0x35C;

    explicit SageAudioManagerView(void* storage) noexcept;
    explicit SageAudioManagerView(const void* storage) noexcept;

    void allocateRequestSerial() noexcept;                         // 0x00503170
    bool isOn(std::uint8_t affect) const noexcept;                 // 0x00503180
    void setOn(bool enabled, std::uint8_t affect) noexcept;        // 0x005031D0
    void listenerPosition(float* x, float* y, float* z) const noexcept; // 0x00503220
    void* eventInfoRegistry() noexcept;                            // 0x00503280
    std::uint32_t playerHandle(std::uint32_t index) const noexcept; // 0x005032F0
    std::uint32_t allocatePlayingHandle() noexcept;                // 0x00506490

private:
    std::byte* data_;
    const std::byte* constData_;
};

struct WeightedSample {
    std::uint32_t value;
    std::int32_t weight;
    std::uint32_t metadata;
};

std::int32_t selectWeightedIndex(const WeightedSample* begin, const WeightedSample* end,
                                 std::uint32_t randomLimit, std::int32_t& count,
                                 RandomInteger random, void* context) noexcept; // 0x0055A170
std::int32_t selectCacheWeightedIndex(
    const WeightedSample* begin, const WeightedSample* end, std::uint32_t randomLimit,
    std::int32_t& count, RandomInteger random, void* context) noexcept;        // 0x00504BB0

bool audioKeysDiffer(const std::uint32_t* left, const std::uint32_t* right) noexcept; // 0x0055A060
bool requestUsesAudioEvent(const void* request) noexcept;                            // 0x00506410
bool recentChannelUsed(const void* state, std::int32_t relativeIndex) noexcept;       // 0x00504C40

// ABI-exact x87 entries used when the caller needs the full ST0 value.
float minimumRangeExtended(const void* event) noexcept;                              // 0x00559C20
float effectivePitchExtended(const void* event) noexcept;                           // 0x0055A990

void initializeAudioChannelGrid(void* state, ChannelInitializer initialize,
                                void* context) noexcept;
                                                                                     // 0x00504DC0
void resetAudioCategoryForPlayers(void* playerStates, std::uint32_t category,
                                  ChannelInitializer initialize, void* context) noexcept;
                                                                                     // 0x00505300

bool consumeAudioResetFlag(bool& flag) noexcept;                           // 0x00502FC0
void markAudioSettingsDirty(bool& flag) noexcept;                         // 0x00502FE0
float indexedVolume(const float* sixValues, std::uint32_t index) noexcept; // 0x005032A0
float playerVolume(const void* playerStates, std::uint32_t playerIndex) noexcept; // 0x005032D0

struct EnvelopeArguments {
    float current;
    float target;
    float attack;
    float hold;
    float release;
    std::uint32_t playerIndex;
};

void configureEnvelope(void* playerStates, const EnvelopeArguments& arguments,
                       EnvelopeRefresher refresh, void* context) noexcept;               // 0x005065C0
void setPlayerEnvelope(void* playerStates, float value, std::uint32_t mode,
                       std::uint32_t playerIndex, PlayerEnvelopeSetter setter,
                       void* context) noexcept;                                       // 0x00506690

std::int32_t incrementPlaybackCount(std::int32_t* count) noexcept; // 0x0055B060
std::int32_t decrementPlaybackCount(std::int32_t* count) noexcept; // 0x0055B0A0
std::int32_t playbackCount(const std::int32_t* count) noexcept;    // 0x0055B0E0
bool playbackLimitReached(const std::int32_t* count, std::int32_t limit) noexcept; // 0x0055B110

}  // namespace ra3::audio
