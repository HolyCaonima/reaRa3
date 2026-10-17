#pragma once

#include <cstddef>
#include <cstdint>

namespace ra3::common {

// Layout-compatible, non-owning views over RA3 game objects.  These types do
// not construct or destroy game-owned objects.
class Player {
public:
    explicit Player(void* storage) noexcept;
    explicit Player(const void* storage) noexcept;

    void adjustCounter125c(std::int32_t delta) noexcept;             // 0x007e1530
    void setValue1258(std::uint32_t value) noexcept;                 // 0x007e15d0
    [[nodiscard]] void* aiData() const noexcept;                     // 0x007e15e0
    void setAiValue2c(std::uint32_t value) noexcept;                 // 0x007e17a0
    [[nodiscard]] bool isLocalPlayer(const void* local) const noexcept; // 0x007e1830
    void setListInScoreScreen(bool value) noexcept;                  // 0x007e1840
    [[nodiscard]] std::uint32_t aiValue08() const noexcept;          // 0x007e18c0
    void setValueF8(std::uint32_t value) noexcept;                   // 0x007e1930
    [[nodiscard]] float value1f8() const noexcept;                   // 0x007e1950
    [[nodiscard]] void* slotForIndex(std::uint32_t index) noexcept;  // 0x007e19e0
    [[nodiscard]] std::uint8_t indexedFlag10d(std::uint32_t index) const noexcept; // 0x007e1b40
    [[nodiscard]] void* indexedPointer128(std::int32_t index) const noexcept;       // 0x007e1b50
    [[nodiscard]] std::uint32_t value1c8() const noexcept;           // 0x007e1b70
    [[nodiscard]] std::uint8_t isPlayerObserver() const noexcept;    // 0x007e1b80
    [[nodiscard]] std::uint8_t isPlayerDead() const noexcept;        // 0x007e1b90
    [[nodiscard]] bool isPlayerActive() const noexcept;              // 0x007e1ba0
    [[nodiscard]] std::uint32_t selectionValue08() const noexcept;   // 0x007e1c00
    [[nodiscard]] bool selectionFlag1c() const noexcept;             // 0x007e1c20
    [[nodiscard]] bool selectionOwnerFlag5a() const noexcept;        // 0x007e1c40

    static constexpr std::size_t minimumRecoveredSize = 0x123d;

private:
    std::byte* data_;
};

class PlayerList {
public:
    explicit PlayerList(void* storage) noexcept;
    explicit PlayerList(const void* storage) noexcept;

    [[nodiscard]] void* getNthPlayer(std::int32_t index) const noexcept; // 0x007e1c70
    [[nodiscard]] void* findByValue1c(std::int32_t value) const noexcept; // 0x007e1c80
    [[nodiscard]] std::uint32_t playerTypeMask() const noexcept;          // 0x007e2060

private:
    std::byte* data_;
};

class ScoreKeeper {
public:
    explicit ScoreKeeper(void* storage) noexcept;
    explicit ScoreKeeper(const void* storage) noexcept;

    void setValue04IfUnlocked(std::uint32_t value) noexcept;         // 0x007e25d0
    void setValue0cIfUnlocked(std::uint32_t value) noexcept;         // 0x007e25e0
    void addD0(std::int32_t delta, bool scoringEnabled) noexcept;    // 0x007e2600
    void addFloat11c(float delta, bool scoringEnabled) noexcept;     // 0x007e2620
    void increment100(bool scoringEnabled) noexcept;                 // 0x007e2650
    void incrementE8(bool scoringEnabled) noexcept;                  // 0x007e2670
    void incrementEc(bool scoringEnabled) noexcept;                  // 0x007e2690
    void incrementF0(bool scoringEnabled) noexcept;                  // 0x007e26b0
    void incrementD8(bool scoringEnabled) noexcept;                  // 0x007e26d0
    void addDc(std::int32_t delta, bool scoringEnabled) noexcept;    // 0x007e26f0
    void stopTimer(std::uint32_t frame, bool scoringEnabled) noexcept; // 0x007e2710
    [[nodiscard]] std::uint32_t elapsedSeconds(
        std::uint32_t frame, std::uint32_t framesPerSecond) const noexcept; // 0x007e2740
    [[nodiscard]] std::uint32_t timedSeconds(
        std::uint32_t frame, std::uint32_t framesPerSecond) const noexcept; // 0x007e2760
    [[nodiscard]] std::int32_t sumGroup68() const noexcept;          // 0x007e27a0
    [[nodiscard]] std::int32_t sumGroup10() const noexcept;          // 0x007e27d0
    void setFlag134(bool value) noexcept;                            // 0x007e2800
    void setFlag135(bool value) noexcept;                            // 0x007e2810
    void markFlag134() noexcept;                                     // 0x007e2820
    void addMoney(std::int32_t delta, bool scoringEnabled) noexcept; // 0x007e2830

    static constexpr std::size_t minimumRecoveredSize = 0x148;

private:
    std::byte* data_;
};

namespace player_port {
[[nodiscard]] float divideUnlessOne(float numerator, float denominator) noexcept; // 0x007e13b0
[[nodiscard]] bool hasCapacity(const void* object, std::uint32_t amount) noexcept; // 0x007e1480
[[nodiscard]] bool eitherPlayerStateSet(const void* object) noexcept;              // 0x007e20c0
[[nodiscard]] const void* choosePlayerState(const void* object) noexcept;           // 0x007e20f0
void setBoundedDelay(void* object, std::int32_t delay) noexcept;                    // 0x007e21e0
[[nodiscard]] bool typeIs(const void* wrapper, std::int32_t type) noexcept;          // 0x007e2260...
[[nodiscard]] bool typeIsHumanOrComputer(const void* wrapper) noexcept;             // 0x007e2290
[[nodiscard]] std::uint32_t nestedU32(const void* wrapper, std::size_t offset) noexcept;
[[nodiscard]] void* nestedAddress(void* wrapper, std::size_t offset) noexcept;
[[nodiscard]] std::uint32_t indexIsOutsideBinaryRange(std::int32_t value) noexcept;  // 0x007e2470
void setFlagD7(void* object, bool value) noexcept;                                   // 0x007e2ca0
void addValue290(void* object, std::int32_t value) noexcept;                         // 0x007e2d30
void subtractValue290(void* object, std::int32_t value) noexcept;                    // 0x007e2d40
}  // namespace player_port

}  // namespace ra3::common
