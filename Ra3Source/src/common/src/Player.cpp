#include "ra3/common/Player.h"

#include <cstring>

namespace ra3::common {
namespace {

template <typename T>
T load(const std::byte* data, std::size_t offset) noexcept
{
    T value{};
    std::memcpy(&value, data + offset, sizeof(value));
    return value;
}

template <typename T>
void store(std::byte* data, std::size_t offset, T value) noexcept
{
    std::memcpy(data + offset, &value, sizeof(value));
}

std::byte* bytes(void* value) noexcept
{
    return static_cast<std::byte*>(value);
}

const std::byte* bytes(const void* value) noexcept
{
    return static_cast<const std::byte*>(value);
}

std::byte* pointer(const std::byte* data, std::size_t offset) noexcept
{
    return reinterpret_cast<std::byte*>(load<std::uintptr_t>(data, offset));
}

}  // namespace

// Reconstructed from ra3_1.0.game 0x007E1530-0x007E20F0.
// Generals structural reference: Common/RTS/Player.cpp and Include/Common/Player.h.
Player::Player(void* storage) noexcept : data_(bytes(storage)) {}
Player::Player(const void* storage) noexcept : data_(const_cast<std::byte*>(bytes(storage))) {}

void Player::adjustCounter125c(std::int32_t delta) noexcept
{
    const auto result = load<std::int32_t>(data_, 0x125c) + delta;
    store(data_, 0x125c, result < 0 ? 0 : result);
}

void Player::setValue1258(std::uint32_t value) noexcept { store(data_, 0x1258, value); }

void* Player::aiData() const noexcept
{
    auto* ai = pointer(data_, 0x8c);
    return ai == nullptr ? nullptr : ai + 0x0c;
}

void Player::setAiValue2c(std::uint32_t value) noexcept
{
    if (auto* ai = pointer(data_, 0x8c); ai != nullptr) {
        store(ai, 0x2c, value);
    }
}

bool Player::isLocalPlayer(const void* local) const noexcept { return data_ == bytes(local); }
void Player::setListInScoreScreen(bool value) noexcept { store(data_, 0x10b, std::uint8_t(value)); }

std::uint32_t Player::aiValue08() const noexcept
{
    const auto* ai = pointer(data_, 0x8c);
    return ai == nullptr ? 0U : load<std::uint32_t>(ai, 0x08);
}

void Player::setValueF8(std::uint32_t value) noexcept { store(data_, 0xf8, value); }
float Player::value1f8() const noexcept { return load<float>(data_, 0x1f8); }

void* Player::slotForIndex(std::uint32_t index) noexcept
{
    return index < 0x55 ? data_ + 0x248 + index * 0x30 : nullptr;
}

std::uint8_t Player::indexedFlag10d(std::uint32_t index) const noexcept
{
    return load<std::uint8_t>(data_, 0x10d + index);
}

void* Player::indexedPointer128(std::int32_t index) const noexcept
{
    if (index < 0 || index >= 20) {
        return nullptr;
    }
    return reinterpret_cast<void*>(load<std::uintptr_t>(data_, 0x128 + std::size_t(index) * 4));
}

std::uint32_t Player::value1c8() const noexcept { return load<std::uint32_t>(data_, 0x1c8); }
std::uint8_t Player::isPlayerObserver() const noexcept { return load<std::uint8_t>(data_, 0x106); }
std::uint8_t Player::isPlayerDead() const noexcept { return load<std::uint8_t>(data_, 0x123c); }
bool Player::isPlayerActive() const noexcept { return !isPlayerObserver() && !isPlayerDead(); }

std::uint32_t Player::selectionValue08() const noexcept
{
    const auto* selection = pointer(data_, 0x1278);
    return selection == nullptr ? 0U : load<std::uint32_t>(selection, 0x08);
}

bool Player::selectionFlag1c() const noexcept
{
    const auto* selection = pointer(data_, 0x1278);
    return selection != nullptr && load<std::uint8_t>(selection, 0x1c) != 0;
}

bool Player::selectionOwnerFlag5a() const noexcept
{
    const auto* selection = pointer(data_, 0x1278);
    if (selection == nullptr) {
        return false;
    }
    const auto* owner = pointer(selection, 0x04);
    return owner != nullptr && load<std::uint8_t>(owner, 0x5a) != 0;
}

PlayerList::PlayerList(void* storage) noexcept : data_(bytes(storage)) {}
PlayerList::PlayerList(const void* storage) noexcept : data_(const_cast<std::byte*>(bytes(storage))) {}

void* PlayerList::getNthPlayer(std::int32_t index) const noexcept
{
    return reinterpret_cast<void*>(load<std::uintptr_t>(data_, 0x34 + std::size_t(index) * 4));
}

void* PlayerList::findByValue1c(std::int32_t value) const noexcept
{
    const auto count = load<std::int32_t>(data_, 0x30);
    for (std::int32_t index = 0; index < count; ++index) {
        auto* candidate = pointer(data_, 0x34 + std::size_t(index) * 4);
        if (candidate != nullptr && load<std::int32_t>(candidate, 0x1c) == value) {
            return candidate;
        }
    }
    return nullptr;
}

std::uint32_t PlayerList::playerTypeMask() const noexcept
{
    const auto count = load<std::int32_t>(data_, 0x30);
    std::uint32_t result = 0;
    for (std::int32_t index = 0; index < count; ++index) {
        const auto* player = pointer(data_, 0x34 + std::size_t(index) * 4);
        if (player != nullptr) {
            result |= 1U << (load<std::uint32_t>(player, 0x20) & 31U);
        }
    }
    return result;
}

// Reconstructed from ra3_1.0.game 0x007E25D0-0x007E2830.
// Generals structural reference: Common/RTS/ScoreKeeper.cpp.
ScoreKeeper::ScoreKeeper(void* storage) noexcept : data_(bytes(storage)) {}
ScoreKeeper::ScoreKeeper(const void* storage) noexcept : data_(const_cast<std::byte*>(bytes(storage))) {}

void ScoreKeeper::setValue04IfUnlocked(std::uint32_t value) noexcept
{
    if (load<std::int32_t>(data_, 0x10) == 0) {
        store(data_, 0x04, value);
    }
}

void ScoreKeeper::setValue0cIfUnlocked(std::uint32_t value) noexcept
{
    if (load<std::int32_t>(data_, 0x10) == 0) {
        store(data_, 0x0c, value);
    }
}

void ScoreKeeper::addD0(std::int32_t delta, bool enabled) noexcept
{
    if (enabled) store(data_, 0xd0, load<std::int32_t>(data_, 0xd0) + delta);
}
void ScoreKeeper::addFloat11c(float delta, bool enabled) noexcept
{
    if (enabled) store(data_, 0x11c, load<float>(data_, 0x11c) + delta);
}
void ScoreKeeper::increment100(bool enabled) noexcept
{
    if (enabled) store(data_, 0x100, load<std::int32_t>(data_, 0x100) + 1);
}
void ScoreKeeper::incrementE8(bool enabled) noexcept
{
    if (enabled) store(data_, 0xe8, load<std::int32_t>(data_, 0xe8) + 1);
}
void ScoreKeeper::incrementEc(bool enabled) noexcept
{
    if (enabled) store(data_, 0xec, load<std::int32_t>(data_, 0xec) + 1);
}
void ScoreKeeper::incrementF0(bool enabled) noexcept
{
    if (enabled) store(data_, 0xf0, load<std::int32_t>(data_, 0xf0) + 1);
}
void ScoreKeeper::incrementD8(bool enabled) noexcept
{
    if (enabled) store(data_, 0xd8, load<std::int32_t>(data_, 0xd8) + 1);
}
void ScoreKeeper::addDc(std::int32_t delta, bool enabled) noexcept
{
    if (enabled) store(data_, 0xdc, load<std::int32_t>(data_, 0xdc) + delta);
}

void ScoreKeeper::stopTimer(std::uint32_t frame, bool enabled) noexcept
{
    const auto started = load<std::uint32_t>(data_, 0xf4);
    if (enabled && started != 0) {
        store(data_, 0xf8, load<std::uint32_t>(data_, 0xf8) + frame - started);
        store(data_, 0xf4, std::uint32_t{0});
    }
}

std::uint32_t ScoreKeeper::elapsedSeconds(
    std::uint32_t frame, std::uint32_t framesPerSecond) const noexcept
{
    const auto ended = load<std::uint32_t>(data_, 0x118);
    return (ended == 0 ? frame : ended) / framesPerSecond;
}

std::uint32_t ScoreKeeper::timedSeconds(
    std::uint32_t frame, std::uint32_t framesPerSecond) const noexcept
{
    const auto started = load<std::uint32_t>(data_, 0xf4);
    const auto elapsed = load<std::uint32_t>(data_, 0xf8);
    return (started == 0 ? elapsed : elapsed + frame - started) / framesPerSecond;
}

std::int32_t ScoreKeeper::sumGroup68() const noexcept
{
    std::int32_t result = 0;
    for (std::size_t group = 0; group < 4; ++group) {
        for (std::size_t item = 0; item < 5; ++item) {
            result += load<std::int32_t>(data_, 0x74 + (group * 5 + item) * 4);
        }
    }
    return result;
}

std::int32_t ScoreKeeper::sumGroup10() const noexcept
{
    std::int32_t result = 0;
    for (std::size_t group = 0; group < 4; ++group) {
        for (std::size_t item = 0; item < 5; ++item) {
            result += load<std::int32_t>(data_, 0x1c + (group * 5 + item) * 4);
        }
    }
    return result;
}

void ScoreKeeper::setFlag134(bool value) noexcept { store(data_, 0x134, std::uint8_t(value)); }
void ScoreKeeper::setFlag135(bool value) noexcept { store(data_, 0x135, std::uint8_t(value)); }
void ScoreKeeper::markFlag134() noexcept { store(data_, 0x134, std::uint8_t{1}); }
void ScoreKeeper::addMoney(std::int32_t delta, bool enabled) noexcept
{
    if (enabled) {
        store(data_, 0x04, load<std::int32_t>(data_, 0x04) + delta);
        store(data_, 0x144, load<std::int32_t>(data_, 0x144) + delta);
    }
}

namespace player_port {

float divideUnlessOne(float numerator, float denominator) noexcept
{
    return denominator == 1.0F ? numerator : numerator / denominator;
}

bool hasCapacity(const void* object, std::uint32_t amount) noexcept
{
    const auto* data = bytes(object);
    std::uint32_t available = 0;
    const auto first = load<std::uint32_t>(data, 0x04);
    const auto firstEnd = load<std::uint32_t>(data, 0x08);
    const auto second = load<std::uint32_t>(data, 0x14);
    const auto secondEnd = load<std::uint32_t>(data, 0x18);
    if (second < secondEnd) available = secondEnd - second;
    if (first < firstEnd) available += firstEnd - first;
    return amount <= available;
}

bool eitherPlayerStateSet(const void* object) noexcept
{
    const auto* player = pointer(bytes(object), 0x2c);
    return load<std::uint8_t>(player, 0x106) != 0 || load<std::uint8_t>(player, 0x123c) != 0;
}

const void* choosePlayerState(const void* object) noexcept
{
    const auto* data = bytes(object);
    auto* player = pointer(data, 0x2c);
    if (player != nullptr &&
        (load<std::uint8_t>(player, 0x106) != 0 || load<std::uint8_t>(player, 0x123c) != 0)) {
        if (auto* alternate = pointer(data, 0x84); alternate != nullptr) return alternate;
    }
    return player;
}

void setBoundedDelay(void* object, std::int32_t delay) noexcept
{
    auto* data = bytes(object);
    const auto limit = load<std::int32_t>(data, 0x38);
    const auto current = load<std::int32_t>(data, 0x34);
    store(data, 0x34, limit > 0 && std::uint32_t(limit) < std::uint32_t(current + delay) ? limit : delay);
}

bool typeIs(const void* wrapper, std::int32_t type) noexcept
{
    const auto* nested = pointer(bytes(wrapper), 0);
    return load<std::int32_t>(nested, 0x04) == type;
}

bool typeIsHumanOrComputer(const void* wrapper) noexcept
{
    return typeIs(wrapper, 3) || typeIs(wrapper, 1);
}

std::uint32_t nestedU32(const void* wrapper, std::size_t offset) noexcept
{
    return load<std::uint32_t>(pointer(bytes(wrapper), 0), offset);
}

void* nestedAddress(void* wrapper, std::size_t offset) noexcept
{
    return pointer(bytes(wrapper), 0) + offset;
}

std::uint32_t indexIsOutsideBinaryRange(std::int32_t value) noexcept
{
    const auto original = static_cast<std::uint32_t>(value);
    return (original & 0xffffff00U) | std::uint32_t(value < 0 || value >= 2);
}

void setFlagD7(void* object, bool value) noexcept
{
    auto* data = bytes(object);
    store(data, 0xd7, std::uint8_t(value));
    if (value) store(data, 0xd6, std::uint8_t{1});
}

void addValue290(void* object, std::int32_t value) noexcept
{
    auto* data = bytes(object);
    store(data, 0x290, load<std::int32_t>(data, 0x290) + value);
}

void subtractValue290(void* object, std::int32_t value) noexcept
{
    auto* data = bytes(object);
    store(data, 0x290, load<std::int32_t>(data, 0x290) - value);
}

}  // namespace player_port
}  // namespace ra3::common
