#include <ra3/render/W3DAnimation.h>

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstring>
#include <xmmintrin.h>

namespace ra3::render {
namespace {

template <class T>
T load(const void* object, std::size_t offset) noexcept
{
    T value{};
    std::memcpy(&value, static_cast<const std::byte*>(object) + offset, sizeof(value));
    return value;
}

template <class T>
void store(void* object, std::size_t offset, T value) noexcept
{
    std::memcpy(static_cast<std::byte*>(object) + offset, &value, sizeof(value));
}

template <class T = void>
T* pointer(const void* object, std::size_t offset) noexcept
{
    return reinterpret_cast<T*>(static_cast<std::uintptr_t>(
        load<std::uint32_t>(object, offset)));
}

std::int32_t truncate(float value) noexcept
{
    return _mm_cvtt_ss2si(_mm_set_ss(value));
}

struct TerrainData {
    std::int32_t width;
    std::int32_t height;
    std::int32_t border;
    const std::uint16_t* samples;
};

bool terrainData(const void* terrain, TerrainData& output) noexcept
{
    const auto* data = pointer<std::byte>(terrain, 0x14);
    if (data == nullptr) {
        return false;
    }
    output.width = load<std::int32_t>(data, 0x24);
    output.height = load<std::int32_t>(data, 0x28);
    output.border = load<std::int32_t>(data, 0x2C);
    output.samples = pointer<std::uint16_t>(data, 0x44);
    return true;
}

float sampleQuadMaximum(const TerrainData& data, std::int32_t x, std::int32_t y) noexcept
{
    const auto index = y * data.width + x;
    auto maximum = data.samples[index];
    maximum = (std::max)(maximum, data.samples[index + 1]);
    maximum = (std::max)(maximum, data.samples[index + data.width]);
    maximum = (std::max)(maximum, data.samples[index + data.width + 1]);
    return static_cast<float>(maximum) * 0.0390625F;
}

std::int32_t terrainCoordinate(float value, std::int32_t border) noexcept
{
    return border - truncate(value * -0.1F);
}

bool anyCondition(const std::uint32_t* conditions) noexcept
{
    for (std::size_t index = 0; index != 15; ++index) {
        if (conditions[index] != 0) {
            return true;
        }
    }
    return false;
}

bool conditionSubset(
    const std::uint32_t* candidate, const std::uint32_t* requested) noexcept
{
    for (std::size_t index = 0; index != 15; ++index) {
        if ((requested[index] & candidate[index]) != candidate[index]) {
            return false;
        }
    }
    return true;
}

}  // namespace

// Original implementation; ra3_1.0.game 0x008E6DD0.
float terrainLineMaximum(const void* terrain, const float* first, const float* second) noexcept
{
    TerrainData data{};
    if (!terrainData(terrain, data)) {
        return 0.0F;
    }
    auto x = terrainCoordinate(first[0], data.border);
    auto y = terrainCoordinate(first[1], data.border);
    const auto endX = terrainCoordinate(second[0], data.border);
    const auto endY = terrainCoordinate(second[1], data.border);
    const auto dx = std::abs(endX - x);
    const auto dy = std::abs(endY - y);
    if (dx == 0 && dy == 0) {
        return static_cast<float>(data.samples[y * data.width + x]) * 0.0390625F;
    }

    const auto stepX = endX < x ? -1 : 1;
    const auto stepY = endY < y ? -1 : 1;
    const bool yMajor = dx < dy;
    const auto major = yMajor ? dy : dx;
    const auto minor = yMajor ? dx : dy;
    auto error = major / 2;
    float maximum = 0.0F;
    for (std::int32_t index = 0; index < major; ++index) {
        if (x < 0 || y < 0 || data.width - 1 <= x || data.height - 1 <= y) {
            break;
        }
        maximum = (std::max)(maximum, sampleQuadMaximum(data, x, y));
        error += minor;
        if (major <= error) {
            if (yMajor) {
                x += stepX;
            } else {
                y += stepY;
            }
            error -= major;
        }
        if (yMajor) {
            y += stepY;
        } else {
            x += stepX;
        }
    }
    return maximum;
}

// Original implementation; ra3_1.0.game 0x008E7000.
float terrainCellMaximum(const void* terrain, float x, float y) noexcept
{
    TerrainData data{};
    if (!terrainData(terrain, data)) {
        return 0.0F;
    }
    auto cellX = terrainCoordinate(x, data.border);
    auto cellY = terrainCoordinate(y, data.border);
    cellX = (std::max)(0, (std::min)(cellX, data.width - 2));
    cellY = (std::max)(0, (std::min)(cellY, data.height - 2));
    return sampleQuadMaximum(data, cellX, cellY);
}

// Original implementation; ra3_1.0.game 0x008E70F0.
void terrainExtent(const void* terrain, float* extent) noexcept
{
    auto width = 0;
    auto height = 0;
    const auto* data = pointer<std::byte>(terrain, 0x14);
    if (data != nullptr) {
        width = load<std::int32_t>(data, 0x24);
        height = load<std::int32_t>(data, 0x28);
    }
    auto x = static_cast<float>(width) * 5.0F;
    auto y = static_cast<float>(height) * 5.0F;
    const auto z = (load<float>(terrain, 0x30) - load<float>(terrain, 0x2C)) * 0.5F
        + load<float>(terrain, 0x2C);
    const auto radius = std::sqrt(z * z + y * y + x * x);
    if (data != nullptr) {
        x += static_cast<float>(load<std::int32_t>(data, 0x17148)) * 10.0F;
        y += static_cast<float>(load<std::int32_t>(data, 0x1714C)) * 10.0F;
    }
    extent[0] = x;
    extent[1] = y;
    extent[2] = z;
    extent[3] = radius;
}

// Original implementation; ra3_1.0.game 0x008E71E0.
void terrainCorners(const void* terrain, float* corners) noexcept
{
    auto width = 0;
    auto height = 0;
    const auto* data = pointer<std::byte>(terrain, 0x14);
    if (data != nullptr) {
        width = load<std::int32_t>(data, 0x24);
        height = load<std::int32_t>(data, 0x28);
    }
    const auto fullX = static_cast<float>(width) * 10.0F;
    const auto fullY = static_cast<float>(height) * 10.0F;
    const auto low = load<float>(terrain, 0x2C);
    const auto high = load<float>(terrain, 0x30);
    corners[0] = fullX * 0.5F;
    corners[1] = fullY * 0.5F;
    corners[2] = (high + low) * 0.5F;
    corners[3] = fullX * 0.5F;
    corners[4] = fullY * 0.5F;
    corners[5] = (high - low) * 0.5F;
}

// Original implementation; ra3_1.0.game 0x008E89D0.
bool modelHasTextureSlots(const void* modelPointerSlot) noexcept
{
    const auto* model = pointer<std::byte>(modelPointerSlot, 0);
    for (std::size_t index = 0; index != 15; ++index) {
        if (load<std::uint32_t>(model, 0xC + index * 4) != 0) {
            if (load<std::uint8_t>(model, 0x88) == 0) {
                return (load<std::uint32_t>(model, 0x58) >> 4 & 1U) != 0;
            }
            return true;
        }
    }
    return true;
}

// Original implementation; ra3_1.0.game 0x008E8A00.
// Assertion source:
// SageEngine/source/GameEngineDevice/W3DDevice/GameClient/Drawable/W3DScriptedModelDraw.cpp:0x542.
std::int32_t chooseWeightedModel(
    const void* modelVector, std::int32_t excludedIndex, std::int32_t limit,
    std::int32_t randomValue) noexcept
{
    const auto* begin = pointer<std::byte>(modelVector, 8);
    const auto* end = pointer<std::byte>(modelVector, 0xC);
    const auto available = static_cast<std::int32_t>((end - begin) / 12);
    const auto count = (std::min)(available, limit);
    if (count <= 1) {
        return 0;
    }
    for (std::int32_t index = 0; index < count; ++index) {
        const auto* model = pointer<std::byte>(begin + index * 12, 0);
        auto weight = load<std::int32_t>(model, 0x34);
        if (index == excludedIndex) {
            --weight;
        }
        randomValue -= weight;
        if (randomValue < 0) {
            return index;
        }
    }
    return 0;
}

// Original implementation; ra3_1.0.game 0x008E8B20.
void* findConditionSubset260(void* state, const std::uint32_t* conditions) noexcept
{
    if (auto* selected = pointer<void>(state, 0x210); selected != nullptr) {
        return selected;
    }
    const bool requestedAny = anyCondition(conditions);
    void* fallback = nullptr;
    auto* current = pointer<std::byte>(state, 0x260);
    const auto* last = pointer<std::byte>(state, 0x264);
    for (; current != last; current += 0x74) {
        auto* candidate = pointer<std::byte>(current, 0);
        const auto* mask = reinterpret_cast<const std::uint32_t*>(candidate + 0xC);
        const bool candidateAny = anyCondition(mask);
        if (!requestedAny && !candidateAny) {
            return current;
        }
        if (requestedAny && !candidateAny) {
            fallback = current;
        } else if (conditionSubset(mask, conditions)) {
            return current;
        }
    }
    return pointer<void>(state, 0x210) != nullptr ? pointer<void>(state, 0x210) : fallback;
}

// Original implementation; ra3_1.0.game 0x008E8C10.
void* findConditionSubset2c(void* state, const std::uint32_t* conditions) noexcept
{
    void* fallback = nullptr;
    auto* current = pointer<std::byte>(state, 0x2C);
    const auto* last = pointer<std::byte>(state, 0x30);
    for (; current != last; current += 0x88) {
        auto* candidate = pointer<std::byte>(current, 0);
        const auto* mask = reinterpret_cast<const std::uint32_t*>(candidate + 0xC);
        if (!anyCondition(mask)) {
            if (fallback == nullptr) {
                fallback = current;
            }
        } else if (conditionSubset(mask, conditions)) {
            return current;
        }
    }
    return pointer<void>(state, 0x20C) != nullptr ? pointer<void>(state, 0x20C) : fallback;
}

// Original implementation; ra3_1.0.game 0x008E92A0.
bool modelConditionStateDifferent(const void* left, const void* right) noexcept
{
    if (load<std::uint8_t>(left, 0x70) == 0 || load<std::uint8_t>(right, 0x70) == 0) {
        return true;
    }
    const auto* leftBegin = pointer<std::byte>(left, 0x60);
    const auto* leftEnd = pointer<std::byte>(left, 0x64);
    const auto* rightBegin = pointer<std::byte>(right, 0x60);
    const auto* rightEnd = pointer<std::byte>(right, 0x64);
    if ((leftEnd - leftBegin) / 28 != (rightEnd - rightBegin) / 28) {
        return true;
    }
    for (auto* l = leftBegin; l != leftEnd; l += 28) {
        for (auto* r = rightBegin; r != rightEnd; r += 28) {
            if (load<std::uint32_t>(l, 0) != load<std::uint32_t>(r, 0)
                || load<std::uint32_t>(l, 4) != load<std::uint32_t>(r, 4)) {
                return true;
            }
        }
    }
    return false;
}

// Original implementation; ra3_1.0.game 0x008E9350.
void setDrawableSelectable(void* subobject, std::uint8_t selectable) noexcept
{
    auto* bytes = static_cast<std::byte*>(subobject);
    const auto* owner = pointer<std::byte>(bytes - 0x10, 0);
    if (load<std::uint8_t>(owner, 0xCA) != 0 || load<std::uint8_t>(owner, 0xCB) == 0) {
        selectable = 0;
    }
    store<std::uint32_t>(subobject, 0x1E8, selectable != 0 ? 1U : 0U);
    auto* renderObject = pointer<std::byte>(bytes - 8, 0);
    if (renderObject != nullptr) {
        auto flags = load<std::uint32_t>(renderObject, 0xD4);
        flags = selectable != 0 ? flags | 2U : flags & ~2U;
        store(renderObject, 0xD4, flags);
    }
}

// Original implementation; ra3_1.0.game 0x008E95F0.
void setAnimationFrame(void* state, float fraction) noexcept
{
    const auto* animation = pointer<std::byte>(state, 0x108);
    if (animation == nullptr) {
        return;
    }
    const auto maximum = static_cast<float>(load<std::int32_t>(animation, 0x14) - 1);
    auto frame = maximum * fraction;
    if (maximum < frame) {
        frame = maximum;
    }
    const auto mode = load<std::int32_t>(state, 0x118);
    if (mode == 3 || mode == 4) {
        frame = maximum - frame;
    }
    const auto previous = load<std::uint32_t>(state, 0x10C);
    store(state, 0x10C, frame);
    store(state, 0x110, previous);
}

// Original implementation; ra3_1.0.game 0x008E96B0.
bool animationBlendActive(const void* state) noexcept
{
    return load<std::uint32_t>(state, 0x11C) != 0
        && load<std::uint32_t>(state, 0x138) != 0;
}

// Original implementation; ra3_1.0.game 0x008E98A0.
// Generals reference: WWVegas/WWMath/hermitespline.cpp, Hermite tangent preparation.
void buildHermiteTangents(float* values) noexcept
{
    const auto half = 0.5F;
    values[9] = (values[3] - values[0]) * half;
    values[10] = (values[4] - values[1]) * half;
    values[11] = (values[5] - values[2]) * half;
    values[12] = (values[6] - values[3]) * half + values[9];
    values[13] = (values[7] - values[4]) * half + values[10];
    values[14] = (values[8] - values[5]) * half + values[11];
    values[15] = (values[6] - values[3]) * half;
    values[16] = (values[7] - values[4]) * half;
    values[17] = (values[8] - values[5]) * half;
}

// Original implementation; ra3_1.0.game 0x008E9980.
// Generals reference: WWVegas/WWMath/hermitespline.cpp:90-129.
void evaluateHermite(const float* values, float* output, float fraction) noexcept
{
    auto t = (std::max)(0.0F, (std::min)(fraction, 1.0F)) * 2.0F;
    const bool second = fraction >= 0.5F;
    if (second) {
        t -= 1.0F;
    }
    const auto t2 = t * t;
    const auto t3 = t2 * t;
    const auto h0 = 2.0F * t3 - 3.0F * t2 + 1.0F;
    const auto h1 = -2.0F * t3 + 3.0F * t2;
    const auto h2 = t3 - 2.0F * t2 + t;
    const auto h3 = t3 - t2;
    const std::size_t p0 = second ? 3 : 0;
    const std::size_t p1 = second ? 6 : 3;
    const std::size_t outTangent = second ? 12 : 9;
    const std::size_t inTangent = second ? 15 : 12;
    for (std::size_t axis = 0; axis != 3; ++axis) {
        output[axis] = values[p0 + axis] * h0 + values[p1 + axis] * h1
            + values[outTangent + axis] * h2 + values[inTangent + axis] * h3;
    }
}

// Original implementation; ra3_1.0.game 0x008E9BE0.
void advanceTextureFrames(void* frameVector, float elapsed) noexcept
{
    auto* current = pointer<std::byte>(frameVector, 4);
    const auto* last = pointer<std::byte>(frameVector, 8);
    for (; current != last; current += 0x50) {
        const auto duration = load<float>(current, 0x48);
        store(current, 0x4C, load<float>(current, 0x4C) + elapsed / duration);
    }
}

}  // namespace ra3::render
