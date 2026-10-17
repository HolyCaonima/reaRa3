#include <ra3/audio/AudioEvent.h>

#include <algorithm>
#include <cstring>

namespace ra3::audio {
namespace {

template <typename T>
T load(const void* storage, std::size_t offset) noexcept
{
    T value{};
    std::memcpy(&value, static_cast<const std::byte*>(storage) + offset, sizeof(value));
    return value;
}

template <typename T>
void store(void* storage, std::size_t offset, T value) noexcept
{
    std::memcpy(static_cast<std::byte*>(storage) + offset, &value, sizeof(value));
}

const std::byte* pointer32(const void* storage, std::size_t offset) noexcept
{
    return reinterpret_cast<const std::byte*>(
        static_cast<std::uintptr_t>(load<std::uint32_t>(storage, offset)));
}

std::byte* pointer32(void* storage, std::size_t offset) noexcept
{
    return reinterpret_cast<std::byte*>(
        static_cast<std::uintptr_t>(load<std::uint32_t>(storage, offset)));
}

float clampUnit(float value) noexcept
{
    if (value < 0.0f) {
        return 0.0f;
    }
    if (value > 1.0f) {
        return 1.0f;
    }
    return value;
}

std::int32_t weightedIndex(const WeightedSample* begin, const WeightedSample* end,
                           std::uint32_t randomLimit, std::int32_t& count,
                           RandomInteger random, void* context) noexcept
{
    count = static_cast<std::int32_t>(end - begin);
    if (randomLimit == 0) {
        return -1;
    }
    std::int32_t value =
        random == nullptr ? 0 : random(context, 0, static_cast<std::int32_t>(randomLimit - 1));
    for (auto current = begin; current != end; ++current) {
        if (value < current->weight) {
            return static_cast<std::int32_t>(current - begin);
        }
        value -= current->weight;
    }
    return 0;
}

bool eventInfoPlayable(const std::byte* info) noexcept
{
    const std::uint32_t type = load<std::uint32_t>(info, 0xE8);
    if (type != 4) {
        return type == 0 || type == 3 || (load<std::uint8_t>(info, 0x78) & 1u) != 0;
    }
    if ((load<std::uint8_t>(info, 0x100) & 1u) != 0) {
        return true;
    }
    const auto begin = pointer32(info, 0x104);
    const auto end = pointer32(info, 0x108);
    for (auto current = begin; current != end; current += 0x20) {
        const auto child = pointer32(current, 0);
        if (child != nullptr && eventInfoPlayable(child)) {
            return true;
        }
    }
    return false;
}

bool eventInfoMatches(const std::byte* info, std::uint32_t category) noexcept
{
    const std::uint32_t type = load<std::uint32_t>(info, 0xE8);
    if (type != 4) {
        return type == category;
    }
    const auto begin = pointer32(info, 0x104);
    const auto end = pointer32(info, 0x108);
    bool matched = false;
    for (auto current = begin; current != end; current += 0x20) {
        const auto child = pointer32(current, 0);
        if (child != nullptr && !eventInfoMatches(child, category)) {
            return false;
        }
        matched = matched || child != nullptr;
    }
    return matched;
}

}  // namespace

AudioEventView::AudioEventView(void* storage) noexcept
    : data_(static_cast<std::byte*>(storage)), constData_(data_)
{
}

AudioEventView::AudioEventView(const void* storage) noexcept
    : data_(nullptr), constData_(static_cast<const std::byte*>(storage))
{
}

void AudioEventView::setPlayerIndex(std::uint32_t value) noexcept
{
    if (load<std::uint32_t>(data_, 0x30) != value) {
        store(data_, 0x30, value);
    }
}

float AudioEventView::minimumRange() const noexcept
{
    return minimumRangeExtended(constData_);
}

void AudioEventView::decrementDelay(float amount) noexcept
{
    store(data_, 0x68, load<float>(data_, 0x68) - amount);
}

void AudioEventView::setPosition(const Vector3* position) noexcept
{
    if (position == nullptr) {
        return;
    }
    const auto owner = load<std::uint32_t>(data_, 0x38);
    if (owner != static_cast<std::uint32_t>(OwnerType::positional)
        && owner != static_cast<std::uint32_t>(OwnerType::invalid)) {
        return;
    }
    store(data_, 0x3C, load<float>(position, 0));
    store(data_, 0x40, load<float>(position, 4));
    store(data_, 0x44, load<float>(position, 8));
    store<std::uint8_t>(data_, 0x48, 1);
    store(data_, 0x38, static_cast<std::uint32_t>(OwnerType::positional));
}

void AudioEventView::setObjectId(std::uint32_t id) noexcept
{
    const auto owner = load<std::uint32_t>(data_, 0x38);
    if (owner == static_cast<std::uint32_t>(OwnerType::object)
        || owner == static_cast<std::uint32_t>(OwnerType::invalid)) {
        store(data_, 0x34, id);
        store(data_, 0x38, static_cast<std::uint32_t>(OwnerType::object));
    }
}

std::uint32_t AudioEventView::objectId() const noexcept
{
    return load<std::uint32_t>(constData_, 0x38)
            == static_cast<std::uint32_t>(OwnerType::object)
        ? load<std::uint32_t>(constData_, 0x34)
        : 0;
}

void AudioEventView::setDrawableId(std::uint32_t id) noexcept
{
    const auto owner = load<std::uint32_t>(data_, 0x38);
    if (owner == static_cast<std::uint32_t>(OwnerType::drawable)
        || owner == static_cast<std::uint32_t>(OwnerType::invalid)) {
        store(data_, 0x34, id);
        store(data_, 0x38, static_cast<std::uint32_t>(OwnerType::drawable));
    }
}

std::uint32_t AudioEventView::drawableId() const noexcept
{
    return load<std::uint32_t>(constData_, 0x38)
            == static_cast<std::uint32_t>(OwnerType::drawable)
        ? load<std::uint32_t>(constData_, 0x34)
        : 0;
}

void AudioEventView::setShouldFade(bool value) noexcept
{
    store<std::uint8_t>(data_, 0x49, value ? 1 : 0);
}

bool AudioEventView::shouldFade() const noexcept
{
    return load<std::uint8_t>(constData_, 0x49) != 0;
}

void AudioEventView::setMinimumRangeOverride(float value) noexcept
{
    store(data_, 0x24, value);
}

void AudioEventView::setMaximumRangeOverride(float value) noexcept
{
    store(data_, 0x28, value);
}

void AudioEventView::setVolume(float value) noexcept
{
    store(data_, 0x2C, value);
}

float AudioEventView::effectivePitch() const noexcept
{
    return effectivePitchExtended(constData_);
}

#if defined(_MSC_VER) && defined(_M_IX86)
__declspec(naked) float __cdecl minimumRangeExtended(const void*) noexcept
{
    __asm {
        mov ecx, dword ptr [esp + 4]
        fld dword ptr [ecx + 70h]
        ret
    }
}

__declspec(naked) float __cdecl effectivePitchExtended(const void*) noexcept
{
    __asm {
        mov ecx, dword ptr [esp + 4]
        cmp byte ptr [ecx + 54h], 0
        je withoutInfo
        mov eax, dword ptr [ecx + 0Ch]
        fld dword ptr [eax + 20h]
        fmul dword ptr [ecx + 5Ch]
        fmul dword ptr [ecx + 58h]
        ret

    withoutInfo:
        fld dword ptr [ecx + 5Ch]
        fmul dword ptr [ecx + 58h]
        ret
    }
}
#else
float minimumRangeExtended(const void* event) noexcept
{
    return load<float>(event, 0x70);
}

float effectivePitchExtended(const void* event) noexcept
{
    float result = load<float>(event, 0x5C) * load<float>(event, 0x58);
    if (load<std::uint8_t>(event, 0x54) != 0) {
        result = load<float>(pointer32(event, 0x0C), 0x20)
            * load<float>(event, 0x5C) * load<float>(event, 0x58);
    }
    return result;
}
#endif

bool AudioEventView::canContinue() const noexcept
{
    if (load<std::uint8_t>(constData_, 0x4C) != 0) {
        return false;
    }
    const auto info = pointer32(constData_, 0x0C);
    if (info == nullptr) {
        return true;
    }
    const std::uint32_t type = load<std::uint32_t>(info, 0xE8);
    if (type == 3) {
        return true;
    }
    if (type == 0) {
        const std::int32_t loops = load<std::int32_t>(constData_, 0x84);
        return loops > 0 || loops == -12345;
    }
    return (load<std::uint8_t>(info, 0x78) & 1u) != 0;
}

void AudioEventView::advancePlayPortion() noexcept
{
    const std::uint32_t portion = load<std::uint32_t>(data_, 0x7C);
    if (portion == static_cast<std::uint32_t>(PlayPortion::attack)) {
        store(data_, 0x7C, static_cast<std::uint32_t>(PlayPortion::sound));
    } else if (portion == static_cast<std::uint32_t>(PlayPortion::sound)) {
        std::int32_t loops = load<std::int32_t>(data_, 0x84);
        if (loops != -12345) {
            --loops;
            store(data_, 0x84, loops);
        }
        if (!canContinue()) {
            store(data_, 0x7C, load<std::uint32_t>(data_, 0x20) != 0
                ? static_cast<std::uint32_t>(PlayPortion::decay)
                : static_cast<std::uint32_t>(PlayPortion::done));
        }
    } else if (portion == static_cast<std::uint32_t>(PlayPortion::decay)) {
        store(data_, 0x7C, static_cast<std::uint32_t>(PlayPortion::done));
    }
}

void AudioEventView::randomizePitchAndVolume(RandomReal random, void* context) noexcept
{
    const auto info = pointer32(data_, 0x0C);
#if defined(_MSC_VER) && defined(_M_IX86)
    const float percentScale = 0.01f;
    float pitchMinimum = 0.0f;
    float pitchMaximum = 0.0f;
    __asm {
        mov eax, info
        fld dword ptr [eax + 34h]
        fld dword ptr [percentScale]
        fmul st(1), st(0)
        fld1
        fadd st(2), st(0)
        fld dword ptr [eax + 30h]
        fmulp st(2), st(0)
        faddp st(1), st(0)
        fxch st(1)
        fstp dword ptr [pitchMaximum]
        fstp dword ptr [pitchMinimum]
    }
#else
    const float pitchMinimum = 1.0f + load<float>(info, 0x30) * 0.01f;
    const float pitchMaximum = 1.0f + load<float>(info, 0x34) * 0.01f;
#endif
    store(data_, 0x5C, random(context, pitchMinimum, pitchMaximum));
    store(data_, 0x64, random(context, 1.0f + load<float>(info, 0x18), 1.0f));
    if (load<std::uint8_t>(data_, 0x50) != 0) {
        store<std::uint8_t>(data_, 0x4D, 1);
        store<std::uint8_t>(data_, 0x50, 0);
    }
}

void AudioEventView::randomizeDelay(RandomReal random, void* context) noexcept
{
    const auto info = pointer32(data_, 0x0C);
    if (info != nullptr) {
        store(data_, 0x68, random(context, static_cast<float>(load<std::int32_t>(info, 0x3C)),
                                  static_cast<float>(load<std::int32_t>(info, 0x40))));
        randomizePitchAndVolume(random, context);
    }
}

void AudioEventView::randomizePostDelay(RandomReal random, void* context) noexcept
{
    const auto info = pointer32(data_, 0x0C);
    if (info != nullptr) {
        store(data_, 0x68, random(context, static_cast<float>(load<std::int32_t>(info, 0x44)),
                                  static_cast<float>(load<std::int32_t>(info, 0x48))));
        randomizePitchAndVolume(random, context);
    }
}

void AudioEventView::regenerate(RandomReal random, GeneratePlayInfo generate,
                                void* context) noexcept
{
    const auto savedHandle = load<std::uint32_t>(data_, 0x88);
    const auto savedPortion = load<std::uint32_t>(data_, 0x7C);
    store<std::uint8_t>(data_, 0x50, 1);
    generate(context, data_);
    store(data_, 0x88, savedHandle);
    const auto info = pointer32(data_, 0x0C);
    if (info != nullptr) {
        store(data_, 0x68, random(context, static_cast<float>(load<std::int32_t>(info, 0x3C)),
                                  static_cast<float>(load<std::int32_t>(info, 0x40))));
        randomizePitchAndVolume(random, context);
    }
    store(data_, 0x7C, savedPortion);
}

AudioEventInfoView::AudioEventInfoView(const void* storage) noexcept
    : data_(static_cast<const std::byte*>(storage))
{
}

bool AudioEventInfoView::hasOverrideSlots() const noexcept
{
    return load<std::uint32_t>(data_, 0x14) != 0;
}

bool AudioEventInfoView::isPlayable() const noexcept
{
    return eventInfoPlayable(data_);
}

bool AudioEventInfoView::hasAnySamples() const noexcept
{
    if (load<std::uint32_t>(data_, 0xE8) == 4) {
        return load<std::uint32_t>(data_, 0x104) != load<std::uint32_t>(data_, 0x108);
    }
    return load<std::uint32_t>(data_, 0x7C) != load<std::uint32_t>(data_, 0x80)
        || load<std::uint32_t>(data_, 0x90) != load<std::uint32_t>(data_, 0x94)
        || load<std::uint32_t>(data_, 0xA4) != load<std::uint32_t>(data_, 0xA8);
}

std::uint32_t AudioEventInfoView::resolvedPriority() const noexcept
{
    std::int32_t priority = load<std::int32_t>(data_, 0xEC);
    if (priority == -1) {
        if (((load<std::uint32_t>(data_, 0x74) >> 3u) & 1u) != 0) {
            priority = 1;
        } else {
            switch (load<std::uint32_t>(data_, 0xE8)) {
            case 0: priority = 2; break;
            case 1: priority = 1; break;
            case 3: priority = 3; break;
            default: priority = 0; break;
            }
        }
        store(const_cast<std::byte*>(data_), 0xEC, priority);
    }
    return static_cast<std::uint32_t>(priority);
}

bool AudioEventInfoView::matchesCategory(std::uint32_t category) const noexcept
{
    return eventInfoMatches(data_, category);
}

std::int32_t AudioEventInfoView::sampleIndex(std::uint32_t sample) const noexcept
{
    const auto begin = reinterpret_cast<const std::uint32_t*>(pointer32(data_, 0xB8));
    const auto end = reinterpret_cast<const std::uint32_t*>(pointer32(data_, 0xBC));
    for (auto current = begin; current != end; ++current) {
        if (*current == sample) {
            return static_cast<std::int32_t>(current - begin);
        }
    }
    return -1;
}

SageAudioManagerView::SageAudioManagerView(void* storage) noexcept
    : data_(static_cast<std::byte*>(storage)), constData_(data_)
{
}

SageAudioManagerView::SageAudioManagerView(const void* storage) noexcept
    : data_(nullptr), constData_(static_cast<const std::byte*>(storage))
{
}

void SageAudioManagerView::allocateRequestSerial() noexcept
{
    const auto value = load<std::uint32_t>(data_, 0x140) + 1u;
    store(data_, 0x140, value);
}

bool SageAudioManagerView::isOn(std::uint8_t affect) const noexcept
{
    if ((affect & music) != 0) return load<std::uint8_t>(constData_, 0x139) != 0;
    if ((affect & sound) != 0) return load<std::uint8_t>(constData_, 0x137) != 0;
    if ((affect & sound3D) != 0) return load<std::uint8_t>(constData_, 0x138) != 0;
    if ((affect & unknown10) != 0) return load<std::uint8_t>(constData_, 0x13A) != 0;
    return load<std::uint8_t>(constData_, 0x136) != 0;
}

void SageAudioManagerView::setOn(bool enabled, std::uint8_t affect) noexcept
{
    const std::uint8_t value = enabled ? 1 : 0;
    if ((affect & music) != 0) store(data_, 0x139, value);
    if ((affect & unknown10) != 0) store(data_, 0x13A, value);
    if ((affect & sound) != 0) store(data_, 0x137, value);
    if ((affect & sound3D) != 0) store(data_, 0x138, value);
    if ((affect & speech) != 0) store(data_, 0x136, value);
}

void SageAudioManagerView::listenerPosition(float* x, float* y, float* z) const noexcept
{
    const auto listener = pointer32(constData_, 0x350);
    *x = load<float>(listener, 0x88);
    *z = load<float>(listener, 0x90);
    *y = load<float>(listener, 0x8C);
}

void* SageAudioManagerView::eventInfoRegistry() noexcept
{
    return data_ + 0x94;
}

std::uint32_t SageAudioManagerView::playerHandle(std::uint32_t index) const noexcept
{
    return load<std::uint32_t>(constData_, 0x2F0 + index * 4u);
}

std::uint32_t SageAudioManagerView::allocatePlayingHandle() noexcept
{
    const auto result = load<std::uint32_t>(data_, 0xB4);
    store(data_, 0xB4, result + 1u);
    return result;
}

std::int32_t selectWeightedIndex(const WeightedSample* begin, const WeightedSample* end,
                                 std::uint32_t randomLimit, std::int32_t& count,
                                 RandomInteger random, void* context) noexcept
{
    return weightedIndex(begin, end, randomLimit, count, random, context);
}

std::int32_t selectCacheWeightedIndex(const WeightedSample* begin, const WeightedSample* end,
                                      std::uint32_t randomLimit, std::int32_t& count,
                                      RandomInteger random, void* context) noexcept
{
    return weightedIndex(begin, end, randomLimit, count, random, context);
}

bool audioKeysDiffer(const std::uint32_t* left, const std::uint32_t* right) noexcept
{
    return left[0] != right[0] || left[1] != right[1];
}

bool requestUsesAudioEvent(const void* request) noexcept
{
    return request != nullptr && load<std::uint32_t>(request, 0x0C) != 0;
}

bool recentChannelUsed(const void* state, std::int32_t relativeIndex) noexcept
{
    std::int32_t index = static_cast<std::int8_t>(load<std::uint8_t>(state, 0x54))
        + relativeIndex;
    if (index >= 10) {
        index -= 10;
    }
    const auto bits = load<std::uint32_t>(state, 0x50 + (static_cast<std::uint32_t>(index) >> 5u) * 4u);
    return (bits & (1u << (static_cast<std::uint32_t>(index) & 31u))) != 0;
}

void initializeAudioChannelGrid(void* state, ChannelInitializer initialize,
                                void* context) noexcept
{
    for (std::uint32_t category = 0; category != 6; ++category) {
        for (std::uint32_t channel = 0; channel != 2; ++channel) {
            initialize(context, state, category, channel);
        }
    }
}

void resetAudioCategoryForPlayers(void* playerStates, std::uint32_t category,
                                  ChannelInitializer initialize, void* context) noexcept
{
    auto state = static_cast<std::byte*>(playerStates);
    for (std::uint32_t player = 0; player != 3; ++player, state += 0x1FC) {
        for (std::uint32_t channel = 0; channel != 2; ++channel) {
            initialize(context, state, category, channel);
        }
    }
}

bool consumeAudioResetFlag(bool& flag) noexcept
{
    if (!flag) {
        return false;
    }
    flag = false;
    return true;
}

void markAudioSettingsDirty(bool& flag) noexcept
{
    flag = true;
}

float indexedVolume(const float* sixValues, std::uint32_t index) noexcept
{
    return index <= 5 ? sixValues[index] : 0.0f;
}

float playerVolume(const void* playerStates, std::uint32_t playerIndex) noexcept
{
    return load<float>(playerStates, playerIndex * 0x1FCu + 0x174u);
}

void configureEnvelope(void* playerStates, const EnvelopeArguments& arguments,
                       EnvelopeRefresher refresh, void* context) noexcept
{
    auto state = static_cast<std::byte*>(playerStates) + arguments.playerIndex * 0x1FCu;
    const float current = clampUnit(arguments.current);
    const float target = clampUnit(arguments.target);
    store(state, 0xD4, current);
    store(state, 0xD8, target);
    store(state, 0xDC, arguments.attack);
    store(state, 0xE0, arguments.hold);
    store(state, 0xE4, arguments.release);
    store(state, 0xE8, 0.0f);
    store<std::uint8_t>(state, 0xEC, 1);
    store(state, 0xB4, current);
    refresh(context, state);
}

void setPlayerEnvelope(void* playerStates, float value, std::uint32_t mode,
                       std::uint32_t playerIndex, PlayerEnvelopeSetter setter,
                       void* context) noexcept
{
    auto state = static_cast<std::byte*>(playerStates) + playerIndex * 0x1FCu;
    setter(context, state, value, mode);
}

std::int32_t incrementPlaybackCount(std::int32_t* count) noexcept
{
    return count == nullptr ? -1 : ++*count;
}

std::int32_t decrementPlaybackCount(std::int32_t* count) noexcept
{
    return count == nullptr ? -1 : --*count;
}

std::int32_t playbackCount(const std::int32_t* count) noexcept
{
    return count == nullptr ? -1 : *count;
}

bool playbackLimitReached(const std::int32_t* count, std::int32_t limit) noexcept
{
    return count != nullptr && *count >= limit;
}

}  // namespace ra3::audio
