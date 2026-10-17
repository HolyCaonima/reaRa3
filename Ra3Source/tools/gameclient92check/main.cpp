#include <ra3/gameclient/GameClient92.h>

#include <array>
#include <cstdint>
#include <cstring>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

struct PredicateInput {
    const std::vector<std::uint8_t>* values;
    std::size_t calls;
};

struct FrameInput {
    std::uint32_t first;
    std::uint32_t second;
    std::size_t calls;
};

struct TrackInput {
    std::int32_t played;
    std::size_t countCalls;
    std::size_t refreshCalls;
};

struct ScoreInput {
    std::size_t clearCalls;
    std::size_t refreshCalls;
    std::size_t rangeCalls;
    const std::vector<std::int32_t>* active;
    const std::vector<std::int32_t>* replacement;
};

struct ConstructionActions {
    std::size_t clearCalls;
    std::size_t refreshCalls;
};

bool indexedPredicate(void* context, std::size_t index) noexcept
{
    auto& input = *static_cast<PredicateInput*>(context);
    ++input.calls;
    return input.values->at(index) != 0;
}

std::uint32_t nextFrame(void* context) noexcept
{
    auto& input = *static_cast<FrameInput*>(context);
    const std::uint32_t result = input.calls == 0 ? input.first : input.second;
    ++input.calls;
    return result;
}

std::int32_t trackCount(void* context) noexcept
{
    auto& input = *static_cast<TrackInput*>(context);
    ++input.countCalls;
    return input.played;
}

void refreshTrack(void* context) noexcept
{
    ++static_cast<TrackInput*>(context)->refreshCalls;
}

void clearConstruction(void* context) noexcept
{
    ++static_cast<ConstructionActions*>(context)->clearCalls;
}

void refreshConstruction(void* context) noexcept
{
    ++static_cast<ConstructionActions*>(context)->refreshCalls;
}

void clearScores(void* context) noexcept
{
    ++static_cast<ScoreInput*>(context)->clearCalls;
}

void refreshScores(void* context) noexcept
{
    auto& input = *static_cast<ScoreInput*>(context);
    ++input.refreshCalls;
    input.active = input.replacement;
}

ra3::gameclient::MusicScoreRange scoreRange(void* context) noexcept
{
    auto& input = *static_cast<ScoreInput*>(context);
    ++input.rangeCalls;
    return {input.active->data(), input.active->size()};
}

std::uint32_t number(const std::string& text)
{
    return static_cast<std::uint32_t>(std::stoul(text, nullptr, 0));
}

std::vector<std::int32_t> scoreList(const std::string& text)
{
    std::vector<std::int32_t> result;
    if (text == "-") {
        return result;
    }
    std::istringstream input(text);
    for (std::string value; std::getline(input, value, ',');) {
        result.push_back(static_cast<std::int32_t>(number(value)));
    }
    return result;
}

float floatValue(const std::string& text)
{
    const std::uint32_t bits = number(text);
    float value = 0.0F;
    std::memcpy(&value, &bits, sizeof(value));
    return value;
}

std::vector<std::uint8_t> fromHex(const std::string& text)
{
    if (text == "-") {
        return {};
    }
    if ((text.size() & 1U) != 0U) {
        throw std::runtime_error("odd hex string");
    }
    std::vector<std::uint8_t> result(text.size() / 2U);
    for (std::size_t index = 0; index < result.size(); ++index) {
        result[index] = static_cast<std::uint8_t>(
            std::stoul(text.substr(index * 2U, 2), nullptr, 16));
    }
    return result;
}

void printBytes(const void* data, std::size_t size)
{
    const auto* bytes = static_cast<const std::uint8_t*>(data);
    const auto flags = std::cout.flags();
    const char fill = std::cout.fill();
    std::cout << std::hex << std::setfill('0');
    for (std::size_t index = 0; index < size; ++index) {
        std::cout << std::setw(2) << static_cast<unsigned>(bytes[index]);
    }
    std::cout.flags(flags);
    std::cout.fill(fill);
}

void hashByte(std::uint32_t& digest, std::uint8_t value) noexcept
{
    digest ^= value;
    digest *= 16777619U;
}

void hashText(std::uint32_t& digest, const char* text) noexcept
{
    do {
        hashByte(digest, static_cast<std::uint8_t>(*text));
    } while (*text++ != '\0');
}

std::uint32_t metadataDigest(
    const ra3::gameclient::MusicReconstructionEntry* entries,
    std::size_t count) noexcept
{
    std::uint32_t digest = 2166136261U;
    for (std::size_t index = 0; index < count; ++index) {
        const auto& entry = entries[index];
        for (unsigned shift = 0; shift != 32; shift += 8) {
            hashByte(
                digest,
                static_cast<std::uint8_t>(entry.address >> shift));
        }
        hashByte(digest, entry.stackCleanup);
        hashText(digest, entry.name);
        hashText(digest, entry.registers);
        hashText(digest, entry.writes);
        hashText(digest, entry.aliases);
        hashByte(
            digest,
            static_cast<std::uint8_t>(entry.floats));
    }
    return digest;
}

void construct(const std::vector<std::string>& words)
{
    using namespace ra3::gameclient;
    const std::uint32_t address = number(words.at(1));
    const auto seed = fromHex(words.at(2));
    if (seed.size() != sizeof(MusicConditionObject)) {
        throw std::runtime_error("constructor seed size");
    }
    MusicConditionObject object{};
    std::memcpy(object.bytes.data(), seed.data(), seed.size());
    ConstructionActions actions{0, 0};
    const MusicConditionConstructionContext context{
        number(words.at(3)), number(words.at(4)), number(words.at(5)),
        number(words.at(6)), number(words.at(7)), number(words.at(8)) != 0,
        number(words.at(9)) != 0, number(words.at(10)) != 0,
        &clearConstruction, &refreshConstruction, &actions};
    switch (address) {
    case 0x00924BD0: constructAnyTrackPlaying(object, context); break;
    case 0x00924C00: constructEvaEventPlayedRecently(object, context); break;
    case 0x00924C30: constructLocalPlayerIsObserver(object, context); break;
    case 0x00924D10: constructPathMusicStateCurrentlyActive(object, context); break;
    case 0x00924D90: constructPathMusicStateLastActive(object, context); break;
    case 0x00924E20: constructSpecificTrackTypePlaying(object, context); break;
    case 0x00924E50: constructTimeFromStartOfLevel(object, context); break;
    case 0x00924E90: constructTrackPlayedCount(object, context); break;
    case 0x00925290: constructEngagementCount(object, context); break;
    case 0x00925360:
        constructHitBySuperWeapon(object, context, number(words.at(11)));
        break;
    case 0x009256D0: constructObjectsNearEvaEvent(object, context); break;
    case 0x00925700: constructObjectsOfTypeExist(object, context); break;
    case 0x00925810: constructUnitsFarFromBase(object, context); break;
    case 0x00926130: constructLocalPlayerKillRatio(object, context); break;
    case 0x00926380: constructScoredKillCount(object, context); break;
    default: throw std::runtime_error("unknown constructor");
    }
    printBytes(object.bytes.data(), object.bytes.size());
    std::cout << ' ' << actions.clearCalls << ' '
              << actions.refreshCalls << '\n';
}

void run(const std::vector<std::string>& words)
{
    using namespace ra3::gameclient;
    if (words.empty()) {
        throw std::runtime_error("empty command");
    }
    if (words[0] == "names") {
        for (std::uint32_t index = 0; index < 20; ++index) {
            if (index != 0) {
                std::cout << '|';
            }
            std::cout << musicConditionTypeName(
                static_cast<MusicConditionKind>(index));
        }
        std::cout << '\n';
        return;
    }
    if (words[0] == "construct") {
        construct(words);
        return;
    }
    if (words[0] == "compare") {
        const bool result = compareMusicValue(
            static_cast<MusicComparison>(number(words.at(1))),
            floatValue(words.at(2)), floatValue(words.at(3)));
        std::cout << (result ? 1 : 0) << '\n';
        return;
    }
    if (words[0] == "state") {
        std::cout << (musicStateIsActive(
            number(words.at(1)) != 0, number(words.at(2)) != 0) ? 1 : 0) << '\n';
        return;
    }
    if (words[0] == "update") {
        bool state = number(words.at(5)) != 0;
        std::uint32_t changedAt = number(words.at(6));
        if (number(words.at(8)) != 0) {
            updateTimedTrackedMusicState(
                number(words.at(1)) != 0, number(words.at(2)), number(words.at(3)),
                number(words.at(7)), changedAt, state);
        } else {
            updateTrackedMusicState(
                number(words.at(1)) != 0, number(words.at(2)), number(words.at(3)),
                number(words.at(4)) != 0, state);
        }
        std::cout << (state ? 1 : 0) << ' ' << changedAt << '\n';
        return;
    }
    if (words[0] == "fold") {
        const auto values = fromHex(words.at(2));
        PredicateInput input{&values, 0};
        const bool result = words.at(1) == "all"
            ? allMusicConditions(values.size(), &indexedPredicate, &input)
            : anyMusicCondition(values.size(), &indexedPredicate, &input);
        std::cout << (result ? 1 : 0) << ' ' << input.calls << '\n';
        return;
    }
    if (words[0] == "not") {
        const std::vector<std::uint8_t> value{
            static_cast<std::uint8_t>(number(words.at(2)))};
        PredicateInput input{&value, 0};
        const bool result = negateMusicCondition(
            number(words.at(1)) != 0, &indexedPredicate, &input);
        std::cout << (result ? 1 : 0) << ' ' << input.calls << '\n';
        return;
    }
    if (words[0] == "count") {
        std::cout << (engagementCountSatisfied(
            number(words.at(1)), number(words.at(2))) ? 1 : 0) << '\n';
        return;
    }
    if (words[0] == "time") {
        std::cout << (timeFromStartSatisfied(
            number(words.at(1)), number(words.at(2)),
            floatValue(words.at(3))) ? 1 : 0) << '\n';
        return;
    }
    if (words[0] == "last") {
        std::cout << (lastMusicStateSatisfied(
            number(words.at(1)) != 0, number(words.at(2)) != 0,
            number(words.at(3)), number(words.at(4)), number(words.at(5)),
            floatValue(words.at(6)),
            static_cast<MusicComparison>(number(words.at(7)))) ? 1 : 0) << '\n';
        return;
    }
    if (words[0] == "score") {
        std::uint32_t cachedPlayer = number(words.at(1));
        std::uint32_t cachedVersion = number(words.at(2));
        const std::uint32_t localPlayer = number(words.at(3));
        const std::uint32_t currentVersion = number(words.at(4));
        const std::int32_t required =
            static_cast<std::int32_t>(number(words.at(5)));
        const std::vector<std::int32_t> initial = scoreList(words.at(6));
        const std::vector<std::int32_t> replacement = scoreList(words.at(7));
        ScoreInput input{0, 0, 0, &initial, &replacement};
        const bool result = scoredKillCountSatisfied(
            cachedPlayer, cachedVersion, localPlayer, currentVersion,
            required, &scoreRange, &clearScores, &refreshScores, &input);
        std::cout << (result ? 1 : 0) << ' ' << cachedPlayer << ' '
                  << cachedVersion << ' ' << input.clearCalls << ' '
                  << input.refreshCalls << ' ' << input.rangeCalls << '\n';
        return;
    }
    if (words[0] == "cached") {
        FrameInput frames{number(words.at(1)), number(words.at(2)), 0};
        std::uint32_t nextCheck = number(words.at(3));
        bool cached = number(words.at(4)) != 0;
        const std::vector<std::uint8_t> value{
            static_cast<std::uint8_t>(number(words.at(7)))};
        PredicateInput input{&value, 0};
        const bool result = cachedMusicCondition(
            nextCheck, cached, floatValue(words.at(5)),
            floatValue(words.at(6)), &indexedPredicate, &input,
            &nextFrame, &frames);
        std::cout << (result ? 1 : 0) << ' ' << nextCheck << ' '
                  << (cached ? 1 : 0) << ' ' << input.calls << ' '
                  << frames.calls << '\n';
        return;
    }
    if (words[0] == "observer") {
        const bool result = localPlayerObserverCondition(
            number(words.at(1)) != 0, number(words.at(2)) != 0,
            number(words.at(3)) != 0, number(words.at(4)) != 0,
            number(words.at(5)) != 0);
        std::cout << (result ? 1 : 0) << '\n';
        return;
    }
    if (words[0] == "track") {
        std::uint32_t cachedPlayer = number(words.at(1));
        TrackInput input{
            static_cast<std::int32_t>(number(words.at(4))), 0, 0};
        const bool result = trackPlayedCountSatisfied(
            cachedPlayer, number(words.at(2)),
            static_cast<std::int32_t>(number(words.at(3))),
            &trackCount, &refreshTrack, &input);
        std::cout << (result ? 1 : 0) << ' ' << cachedPlayer << ' '
                  << input.refreshCalls << ' ' << input.countCalls << '\n';
        return;
    }
    if (words[0] == "entries") {
        std::size_t count = 0;
        const auto* entries = musicReconstructionEntries(count);
        std::cout << count << ' ' << metadataDigest(entries, count) << '\n';
        return;
    }
    throw std::runtime_error("unknown command");
}

int selfCheck()
{
    using namespace ra3::gameclient;
    std::size_t count = 0;
    const auto* entries = musicReconstructionEntries(count);
    std::size_t mismatches = count == 51 ? 0 : 1;
    for (std::size_t index = 0; index < count; ++index) {
        const auto& entry = entries[index];
        if (*entry.name == '\0' || *entry.registers == '\0'
            || *entry.writes == '\0' || *entry.aliases == '\0'
            || static_cast<std::uint8_t>(entry.floats) > 2) {
            ++mismatches;
        }
    }
    for (std::size_t index = 1; index < count; ++index) {
        if (entries[index - 1].address >= entries[index].address) {
            ++mismatches;
        }
    }
    if (!compareMusicValue(MusicComparison::approximatelyEqual, 1.0F, 1.0F)) {
        ++mismatches;
    }
    const std::vector<std::uint8_t> predicateValues{1, 1, 0};
    PredicateInput allInput{&predicateValues, 0};
    PredicateInput anyInput{&predicateValues, 0};
    if (allMusicConditions(3, &indexedPredicate, &allInput)
        || !anyMusicCondition(3, &indexedPredicate, &anyInput)) {
        ++mismatches;
    }
    std::cout << "GameClient92 checker: " << count
              << " entries, " << mismatches << " mismatches\n";
    return mismatches == 0 ? 0 : 1;
}

}  // namespace

int main()
{
    bool handled = false;
    std::string line;
    while (std::getline(std::cin, line)) {
        if (line == "quit") {
            return 0;
        }
        handled = true;
        try {
            std::istringstream input(line);
            std::vector<std::string> words;
            for (std::string word; input >> word;) {
                words.push_back(std::move(word));
            }
            run(words);
        } catch (...) {
            std::cout << "error\n";
        }
        std::cout.flush();
    }
    return handled ? 0 : selfCheck();
}
