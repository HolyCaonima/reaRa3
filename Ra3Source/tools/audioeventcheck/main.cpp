#include <ra3/audio/AudioEvent.h>

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

using Bytes = std::vector<std::byte>;

bool deliberateMutation = false;

std::uint32_t parseWord(const std::string& text)
{
    return static_cast<std::uint32_t>(std::stoul(text, nullptr, 16));
}

std::uint32_t argument(std::istringstream& input)
{
    std::string text;
    input >> text;
    if (text.empty()) {
        throw std::runtime_error("missing argument");
    }
    return parseWord(text);
}

Bytes decode(const std::string& text)
{
    if (text == ".") {
        return {};
    }
    if ((text.size() & 1u) != 0) {
        throw std::runtime_error("odd hex input");
    }
    Bytes result(text.size() / 2);
    for (std::size_t index = 0; index != result.size(); ++index) {
        result[index] = static_cast<std::byte>(
            parseWord(text.substr(index * 2, 2)));
    }
    return result;
}

void encode(const void* storage, std::size_t size)
{
    const auto bytes = static_cast<const std::byte*>(storage);
    std::cout << std::hex << std::setfill('0');
    for (std::size_t index = 0; index != size; ++index) {
        std::cout << std::setw(2) << std::to_integer<unsigned>(bytes[index]);
    }
    std::cout << std::dec;
}

template <typename T>
T load(const void* storage, std::size_t offset)
{
    T value{};
    std::memcpy(&value, static_cast<const std::byte*>(storage) + offset, sizeof(value));
    return value;
}

template <typename T>
void store(void* storage, std::size_t offset, T value)
{
    std::memcpy(static_cast<std::byte*>(storage) + offset, &value, sizeof(value));
}

float asFloat(std::uint32_t bits)
{
    float value = 0.0f;
    std::memcpy(&value, &bits, sizeof(value));
    return value;
}

std::uint32_t floatBits(float value)
{
    std::uint32_t bits = 0;
    std::memcpy(&bits, &value, sizeof(bits));
    return bits;
}

using ExtendedFloatFunction = float(__cdecl*)(const void*) noexcept;

std::uint32_t captureExtended(ExtendedFloatFunction function, const void* storage,
                              std::uint16_t controlWord, Bytes& auxiliary)
{
#if defined(_MSC_VER) && defined(_M_IX86)
    float rounded = 0.0f;
    std::array<std::byte, 10> extended{};
    unsigned short savedControl = 0;
    unsigned short gameControl = controlWord;
    __asm {
        fnstcw savedControl
        fldcw gameControl
        push storage
        call function
        add esp, 4
        fst dword ptr [rounded]
        fstp tbyte ptr [extended]
        fldcw savedControl
    }
    auxiliary.assign(extended.begin(), extended.end());
    return floatBits(rounded);
#else
    (void)controlWord;
    return floatBits(function(storage));
#endif
}

float endpointRandomReal(void* context, float minimum, float maximum)
{
    return *static_cast<const bool*>(context) ? maximum : minimum;
}

std::int32_t selectedRandomInteger(void* context, std::int32_t minimum,
                                   std::int32_t maximum)
{
    const auto selected = *static_cast<const std::int32_t*>(context);
    return selected < minimum ? minimum : (selected > maximum ? maximum : selected);
}

void generatePlayInfo(void*, void* event)
{
    store(event, 0x84, load<std::uint32_t>(event, 0x84) + 7u);
    store(event, 0x88, load<std::uint32_t>(event, 0x88) ^ 0xA5A5A5A5u);
    store(event, 0x7C, 3u);
}

struct CallbackLog {
    std::byte* base = nullptr;
    std::vector<std::uint32_t> words;
};

void initializeChannel(void* context, void* state, std::uint32_t category,
                       std::uint32_t channel)
{
    auto& log = *static_cast<CallbackLog*>(context);
    log.words.push_back(static_cast<std::uint32_t>(
        static_cast<std::byte*>(state) - log.base));
    log.words.push_back(category);
    log.words.push_back(channel);
}

void setEnvelope(void* context, void* state, float value, std::uint32_t mode)
{
    auto& log = *static_cast<CallbackLog*>(context);
    log.words.push_back(static_cast<std::uint32_t>(
        static_cast<std::byte*>(state) - log.base));
    log.words.push_back(floatBits(value));
    log.words.push_back(mode);
}

void refreshEnvelope(void* context, void* state)
{
    auto& log = *static_cast<CallbackLog*>(context);
    log.words.push_back(static_cast<std::uint32_t>(
        static_cast<std::byte*>(state) - log.base));
}

struct PointerRestore {
    void* object;
    std::size_t offset;
    std::uint32_t original;

    ~PointerRestore()
    {
        store(object, offset, original);
    }
};

std::uint32_t runEvent(const std::string& operation, Bytes& blob,
                       std::istringstream& input, Bytes& auxiliary)
{
    constexpr std::size_t eventSize = ra3::audio::AudioEventView::minimumSize;
    constexpr std::size_t infoSize = ra3::audio::AudioEventInfoView::minimumSize;
    if (blob.size() != eventSize + infoSize) {
        throw std::runtime_error("bad event bundle");
    }
    auto event = blob.data();
    auto info = blob.data() + eventSize;
    const bool hasInfo = argument(input) != 0;
    const auto original = load<std::uint32_t>(event, 0x0C);
    store(event, 0x0C, hasInfo ? static_cast<std::uint32_t>(
        reinterpret_cast<std::uintptr_t>(info)) : 0u);
    PointerRestore restore{event, 0x0C, original};
    ra3::audio::AudioEventView view(event);

    if (operation == "event_setplayer") {
        view.setPlayerIndex(argument(input));
    } else if (operation == "event_minrange") {
        return captureExtended(
            ra3::audio::minimumRangeExtended, event,
            static_cast<std::uint16_t>(argument(input)), auxiliary);
    } else if (operation == "event_decrement") {
        view.decrementDelay(asFloat(argument(input)));
    } else if (operation == "event_position") {
        ra3::audio::Vector3 position{
            asFloat(argument(input)), asFloat(argument(input)), asFloat(argument(input))};
        view.setPosition(argument(input) != 0 ? &position : nullptr);
    } else if (operation == "event_setobject") {
        view.setObjectId(argument(input));
    } else if (operation == "event_object") {
        return view.objectId();
    } else if (operation == "event_setdrawable") {
        view.setDrawableId(argument(input));
    } else if (operation == "event_drawable") {
        return view.drawableId();
    } else if (operation == "event_setfade") {
        view.setShouldFade(argument(input) != 0);
    } else if (operation == "event_fade") {
        return view.shouldFade() ? 1u : 0u;
    } else if (operation == "event_min_override") {
        view.setMinimumRangeOverride(asFloat(argument(input)));
    } else if (operation == "event_max_override") {
        view.setMaximumRangeOverride(asFloat(argument(input)));
    } else if (operation == "event_volume") {
        view.setVolume(asFloat(argument(input)));
    } else if (operation == "event_pitch") {
        return captureExtended(
            ra3::audio::effectivePitchExtended, event,
            static_cast<std::uint16_t>(argument(input)), auxiliary);
    } else if (operation == "event_continue") {
        return view.canContinue() ? 1u : 0u;
    } else if (operation == "event_advance") {
        view.advancePlayPortion();
    } else if (operation == "event_randomize") {
        bool maximum = argument(input) != 0;
        view.randomizePitchAndVolume(endpointRandomReal, &maximum);
    } else if (operation == "event_delay") {
        bool maximum = argument(input) != 0;
        view.randomizeDelay(endpointRandomReal, &maximum);
    } else if (operation == "event_postdelay") {
        bool maximum = argument(input) != 0;
        view.randomizePostDelay(endpointRandomReal, &maximum);
    } else if (operation == "event_regenerate") {
        bool maximum = argument(input) != 0;
        view.regenerate(endpointRandomReal, generatePlayInfo, &maximum);
    } else {
        throw std::runtime_error("unknown event operation");
    }
    return 0;
}

std::uint32_t runInfo(const std::string& operation, Bytes& blob,
                      std::istringstream& input)
{
    constexpr std::size_t size = ra3::audio::AudioEventInfoView::minimumSize;
    if (blob.size() != size * 3 + 16) {
        throw std::runtime_error("bad info bundle");
    }
    auto main = blob.data();
    auto child0 = main + size;
    auto child1 = child0 + size;
    std::array<std::byte, 0x40> records{};
    store(records.data(), 0, static_cast<std::uint32_t>(
        reinterpret_cast<std::uintptr_t>(child0)));
    store(records.data(), 0x20, static_cast<std::uint32_t>(
        reinterpret_cast<std::uintptr_t>(child1)));
    std::array<PointerRestore, 4> restore{{
        {main, 0x104, load<std::uint32_t>(main, 0x104)},
        {main, 0x108, load<std::uint32_t>(main, 0x108)},
        {main, 0xB8, load<std::uint32_t>(main, 0xB8)},
        {main, 0xBC, load<std::uint32_t>(main, 0xBC)},
    }};
    const bool group = argument(input) != 0;
    if (group) {
        store(main, 0x104, static_cast<std::uint32_t>(
            reinterpret_cast<std::uintptr_t>(records.data())));
        store(main, 0x108, static_cast<std::uint32_t>(
            reinterpret_cast<std::uintptr_t>(records.data() + records.size())));
    }
    auto samples = reinterpret_cast<std::uint32_t*>(main + size * 3);
    store(main, 0xB8, static_cast<std::uint32_t>(
        reinterpret_cast<std::uintptr_t>(samples)));
    store(main, 0xBC, static_cast<std::uint32_t>(
        reinterpret_cast<std::uintptr_t>(samples + 4)));
    ra3::audio::AudioEventInfoView view(main);

    if (operation == "info_override") return view.hasOverrideSlots() ? 1u : 0u;
    if (operation == "info_playable") return view.isPlayable() ? 1u : 0u;
    if (operation == "info_samples") return view.hasAnySamples() ? 1u : 0u;
    if (operation == "info_priority") return view.resolvedPriority();
    if (operation == "info_matches") return view.matchesCategory(argument(input)) ? 1u : 0u;
    if (operation == "info_index") {
        return static_cast<std::uint32_t>(view.sampleIndex(argument(input)));
    }
    throw std::runtime_error("unknown info operation");
}

std::uint32_t runManager(const std::string& operation, Bytes& blob,
                         std::istringstream& input, Bytes& auxiliary)
{
    constexpr std::size_t size = ra3::audio::SageAudioManagerView::minimumSize;
    if (blob.size() != size + 0x94) {
        throw std::runtime_error("bad manager bundle");
    }
    auto manager = blob.data();
    auto listener = manager + size;
    const auto original = load<std::uint32_t>(manager, 0x350);
    store(manager, 0x350, static_cast<std::uint32_t>(
        reinterpret_cast<std::uintptr_t>(listener)));
    PointerRestore restore{manager, 0x350, original};
    ra3::audio::SageAudioManagerView view(manager);

    if (operation == "manager_serial") {
        view.allocateRequestSerial();
    } else if (operation == "manager_ison") {
        return view.isOn(static_cast<std::uint8_t>(argument(input))) ? 1u : 0u;
    } else if (operation == "manager_seton") {
        const bool enabled = argument(input) != 0;
        view.setOn(enabled, static_cast<std::uint8_t>(argument(input)));
    } else if (operation == "manager_listener") {
        std::array<float, 3> output{};
        view.listenerPosition(&output[0], &output[1], &output[2]);
        auxiliary.resize(sizeof(output));
        std::memcpy(auxiliary.data(), output.data(), sizeof(output));
    } else if (operation == "manager_registry") {
        return static_cast<std::uint32_t>(
            static_cast<std::byte*>(view.eventInfoRegistry()) - manager);
    } else if (operation == "manager_handle") {
        return view.playerHandle(argument(input));
    } else if (operation == "manager_playing") {
        return view.allocatePlayingHandle();
    } else {
        throw std::runtime_error("unknown manager operation");
    }
    return 0;
}

std::uint32_t runHelper(const std::string& operation, Bytes& blob,
                        std::istringstream& input, Bytes& auxiliary)
{
    if (operation == "weighted" || operation == "cache_weighted") {
        if (blob.size() % sizeof(ra3::audio::WeightedSample) != 0) {
            throw std::runtime_error("bad weighted records");
        }
        const ra3::audio::WeightedSample empty{};
        const auto begin = blob.empty()
            ? &empty
            : reinterpret_cast<const ra3::audio::WeightedSample*>(blob.data());
        const auto end = begin + blob.size() / sizeof(*begin);
        const auto limit = argument(input);
        std::int32_t random = static_cast<std::int32_t>(argument(input));
        std::int32_t count = 0;
        const auto result = operation == "weighted"
            ? ra3::audio::selectWeightedIndex(
                begin, end, limit, count, selectedRandomInteger, &random)
            : ra3::audio::selectCacheWeightedIndex(
                begin, end, limit, count, selectedRandomInteger, &random);
        auxiliary.resize(sizeof(count));
        std::memcpy(auxiliary.data(), &count, sizeof(count));
        return static_cast<std::uint32_t>(result);
    }
    if (operation == "keys") {
        return ra3::audio::audioKeysDiffer(
            reinterpret_cast<const std::uint32_t*>(blob.data()),
            reinterpret_cast<const std::uint32_t*>(blob.data() + 8)) ? 1u : 0u;
    }
    if (operation == "request") {
        const bool present = argument(input) != 0;
        return ra3::audio::requestUsesAudioEvent(present ? blob.data() : nullptr) ? 1u : 0u;
    }
    if (operation == "recent") {
        return ra3::audio::recentChannelUsed(
            blob.data(), static_cast<std::int32_t>(argument(input))) ? 1u : 0u;
    }
    if (operation == "grid" || operation == "reset_category") {
        CallbackLog log{blob.data(), {}};
        if (operation == "grid") {
            ra3::audio::initializeAudioChannelGrid(
                blob.data(), initializeChannel, &log);
        } else {
            ra3::audio::resetAudioCategoryForPlayers(
                blob.data(), argument(input), initializeChannel, &log);
        }
        auxiliary.resize(log.words.size() * sizeof(std::uint32_t));
        std::memcpy(auxiliary.data(), log.words.data(), auxiliary.size());
        return 0;
    }
    if (operation == "consume") {
        bool flag = !blob.empty() && blob[0] != std::byte{0};
        const bool result = ra3::audio::consumeAudioResetFlag(flag);
        blob.assign(1, flag ? std::byte{1} : std::byte{0});
        return result ? 1u : 0u;
    }
    if (operation == "dirty") {
        bool flag = !blob.empty() && blob[0] != std::byte{0};
        ra3::audio::markAudioSettingsDirty(flag);
        blob.assign(1, flag ? std::byte{1} : std::byte{0});
        return 0;
    }
    if (operation == "indexed") {
        return floatBits(ra3::audio::indexedVolume(
            reinterpret_cast<const float*>(blob.data()), argument(input)));
    }
    if (operation == "player_volume") {
        return floatBits(ra3::audio::playerVolume(blob.data(), argument(input)));
    }
    if (operation == "envelope") {
        ra3::audio::EnvelopeArguments arguments{
            asFloat(argument(input)), asFloat(argument(input)), asFloat(argument(input)),
            asFloat(argument(input)), asFloat(argument(input)), argument(input)};
        CallbackLog log{blob.data(), {}};
        ra3::audio::configureEnvelope(
            blob.data(), arguments, refreshEnvelope, &log);
        auxiliary.resize(log.words.size() * sizeof(std::uint32_t));
        std::memcpy(auxiliary.data(), log.words.data(), auxiliary.size());
        return 0;
    }
    if (operation == "set_envelope") {
        const float value = asFloat(argument(input));
        const auto mode = argument(input);
        const auto player = argument(input);
        CallbackLog log{blob.data(), {}};
        ra3::audio::setPlayerEnvelope(
            blob.data(), value, mode, player, setEnvelope, &log);
        auxiliary.resize(log.words.size() * sizeof(std::uint32_t));
        std::memcpy(auxiliary.data(), log.words.data(), auxiliary.size());
        return 0;
    }
    if (operation == "count_inc" || operation == "count_dec"
        || operation == "count_get" || operation == "count_limit") {
        const bool present = argument(input) != 0;
        auto count = present ? reinterpret_cast<std::int32_t*>(blob.data()) : nullptr;
        if (operation == "count_inc") {
            return static_cast<std::uint32_t>(ra3::audio::incrementPlaybackCount(count));
        }
        if (operation == "count_dec") {
            return static_cast<std::uint32_t>(ra3::audio::decrementPlaybackCount(count));
        }
        if (operation == "count_get") {
            return static_cast<std::uint32_t>(ra3::audio::playbackCount(count));
        }
        return ra3::audio::playbackLimitReached(
            count, static_cast<std::int32_t>(argument(input))) ? 1u : 0u;
    }
    throw std::runtime_error("unknown helper operation");
}

void run(const std::string& line)
{
    std::istringstream input(line);
    std::string operation;
    std::string encoded;
    input >> operation >> encoded;
    Bytes blob = decode(encoded);
    Bytes auxiliary;
    std::uint32_t result = 0;

    if (operation == "mutation") {
        deliberateMutation = argument(input) != 0;
        std::cout << "00000000 . .\n";
        return;
    }

    if (operation.rfind("event_", 0) == 0) {
        result = runEvent(operation, blob, input, auxiliary);
    } else if (operation.rfind("info_", 0) == 0) {
        result = runInfo(operation, blob, input);
    } else if (operation.rfind("manager_", 0) == 0) {
        result = runManager(operation, blob, input, auxiliary);
    } else {
        result = runHelper(operation, blob, input, auxiliary);
    }

    if (deliberateMutation) {
        result ^= 1u;
    }
    std::cout << std::hex << std::setfill('0') << std::setw(8) << result << ' ';
    if (blob.empty()) {
        std::cout << '.';
    } else {
        encode(blob.data(), blob.size());
    }
    std::cout << ' ';
    if (auxiliary.empty()) {
        std::cout << '.';
    } else {
        encode(auxiliary.data(), auxiliary.size());
    }
    std::cout << '\n';
}

}  // namespace

int main()
{
    std::string line;
    while (std::getline(std::cin, line)) {
        if (line == "quit") {
            break;
        }
        try {
            run(line);
        } catch (...) {
            std::cout << "error\n";
        }
        std::cout.flush();
    }
    return 0;
}
