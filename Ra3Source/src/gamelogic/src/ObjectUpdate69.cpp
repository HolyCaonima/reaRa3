#include "ra3/gamelogic/ObjectUpdate69.h"

#include <cstddef>
#include <cstdint>
#include <cstring>

namespace ra3::gamelogic {
namespace {

template <class T>
T load(const void* object, std::ptrdiff_t offset) noexcept {
    T value{};
    std::memcpy(&value, static_cast<const std::byte*>(object) + offset, sizeof(T));
    return value;
}

template <class T>
void store(void* object, std::ptrdiff_t offset, T value) noexcept {
    std::memcpy(static_cast<std::byte*>(object) + offset, &value, sizeof(value));
}

float loadFloat(const void* object, std::ptrdiff_t offset) noexcept {
    return load<float>(object, offset);
}

void storeFloat(void* object, std::ptrdiff_t offset, float value) noexcept {
    store(object, offset, value);
}

float floatFromBits(std::uint32_t bits) noexcept {
    float value = 0.0F;
    std::memcpy(&value, &bits, sizeof(value));
    return value;
}

// MSVC signed divide by 60, matching the 0x88888889 sequence at 0x006919F0.
std::int32_t countStride60(std::int32_t bytes) noexcept {
    const auto magic = static_cast<std::int64_t>(static_cast<std::int32_t>(0x88888889U));
    const std::int64_t product = static_cast<std::int64_t>(bytes) * magic;
    auto edx = static_cast<std::int32_t>(product >> 32);
    edx = static_cast<std::int32_t>(static_cast<std::uint32_t>(edx) + static_cast<std::uint32_t>(bytes));
    edx >>= 5;
    const std::uint32_t sign = static_cast<std::uint32_t>(edx) >> 31U;
    return static_cast<std::int32_t>(sign + static_cast<std::uint32_t>(edx));
}

template <class Fn>
Fn functionAt(const void* base, std::ptrdiff_t offset) noexcept {
    return load<Fn>(base, offset);
}

using Call0 = std::uint32_t(__thiscall*)(void*);
using Call1 = std::uint32_t(__thiscall*)(void*, std::uint32_t);
using Call2 = std::uint32_t(__thiscall*)(void*, std::uint32_t, std::uint32_t);
using Call3 = std::uint32_t(__thiscall*)(void*, std::uint32_t, std::uint32_t, std::uint32_t);

} // namespace

std::uint32_t stancesBehaviorFirstNodeValue(const void* self) noexcept {
    const auto* head = static_cast<const std::byte*>(self) + 0xD4;
    const auto* next = load<const std::byte*>(head, 0);
    if (next == head) {
        return 0;
    }
    return load<std::uint32_t>(next, 8);
}

float hordeContainDecayRange(void* self, float argument) noexcept {
    const float value = loadFloat(self, 0x160);
    const auto* anchor = load<const std::byte*>(self, -0x164);
    const float diff = value - loadFloat(anchor, 0x120);
    if (!(diff > 0.0F)) {
        return argument;
    }
    const float nineTenths = floatFromBits(0x3F666666U);
    const float hundredth = floatFromBits(0x3C23D70AU);
    const float decayed = value * nineTenths;
    float product = (1.0F - value * hundredth) * argument;
    storeFloat(self, 0x160, decayed);
    const float cap = floatFromBits(0x40C00000U);
    if (product > cap) {
        product = cap;
    }
    return product;
}

void hordeContainInvertSlotF4(void* self) noexcept {
    auto* field = static_cast<std::byte*>(self) + 0x134;
    const auto* target = load<const std::byte*>(field, 0);
    const auto function = functionAt<Call0>(target, 0xF4);
    const auto result = static_cast<std::uint8_t>(function(field));
    store(self, std::ptrdiff_t{0x259}, static_cast<std::uint8_t>(result == 0 ? 1 : 0));
}

void hordeContainFirstPassengerId(const void* self, std::uint32_t* outId) noexcept {
    const auto* object = load<const std::byte*>(self, -0x168);
    if (load<std::int32_t>(object, 0x128) <= 0) {
        *outId = 0;
        return;
    }
    const auto* first = load<const std::byte*>(object, 0x12C);
    *outId = load<std::uint32_t>(first, 0);
}

std::uint8_t hordeContainField5cClear(const void* self) noexcept {
    return load<std::uint32_t>(self, 0x5C) == 0 ? 1U : 0U;
}

void hordeContainNotifyPassengerList(void* self) noexcept {
    const auto* container = load<const std::byte*>(self, -0x168);
    auto* cursor = load<std::uint32_t*>(container, 0x1EC);
    for (;;) {
        const auto count = load<std::uint32_t>(container, 0x1E8);
        auto* begin = load<std::uint32_t*>(container, 0x1EC);
        if (cursor == begin + count) {
            return;
        }
        const std::uint32_t element = *cursor;
        if (element != 0) {
            const auto* vtable = load<const std::byte*>(self, 0);
            const auto function = functionAt<Call3>(vtable, 0x1D0);
            function(self, element, 0, 0xFFFFFFFFU);
        }
        ++cursor;
    }
}

void hordeContainNotifyPassengerListShort(void* self) noexcept {
    const auto* container = load<const std::byte*>(self, -0x168);
    auto* cursor = load<std::uint32_t*>(container, 0x1EC);
    for (;;) {
        const auto count = load<std::uint32_t>(container, 0x1E8);
        auto* begin = load<std::uint32_t*>(container, 0x1EC);
        if (cursor == begin + count) {
            return;
        }
        const std::uint32_t element = *cursor;
        if (element != 0) {
            const auto* vtable = load<const std::byte*>(self, 0);
            const auto function = functionAt<Call2>(vtable, 0x1D8);
            function(self, element, 0);
        }
        ++cursor;
    }
}

void hordeContainMarkRecord(void* self, std::uint32_t /*ignored*/, std::int32_t index) noexcept {
    if (index < 0) {
        return;
    }
    const auto bytes = static_cast<std::int32_t>(
        reinterpret_cast<std::uintptr_t>(load<const std::byte*>(self, 0x0C)) -
        reinterpret_cast<std::uintptr_t>(load<const std::byte*>(self, 0x08)));
    if (index >= countStride60(bytes)) {
        return;
    }
    auto* record = load<std::byte*>(self, 0x08) + static_cast<std::ptrdiff_t>(index) * 0x3C;
    store(record, 0x0C, static_cast<std::uint8_t>(1));
    store(record, 0x10, 0U);
}

std::uint8_t hordeContainCopyRecordVector(
    const void* self, std::int32_t index, void* outVector) noexcept {
    if (index < 0) {
        return 0;
    }
    const auto bytes = static_cast<std::int32_t>(
        reinterpret_cast<std::uintptr_t>(load<const std::byte*>(self, 0x0C)) -
        reinterpret_cast<std::uintptr_t>(load<const std::byte*>(self, 0x08)));
    if (index >= countStride60(bytes)) {
        return 0;
    }
    const auto* record = load<const std::byte*>(self, 0x08) + static_cast<std::ptrdiff_t>(index) * 0x3C;
    if (load<std::uint8_t>(record, 0x39) == 0) {
        return 0;
    }
    store(outVector, 0, load<std::uint32_t>(record, 0));
    store(outVector, 4, load<std::uint32_t>(record, 4));
    store(outVector, 8, load<std::uint32_t>(record, 8));
    return 1;
}

std::uint8_t slaughterHordeContainPasses(void* self, const void* other) noexcept {
    if ((load<std::uint8_t>(other, 0x8C) & 1U) != 0) {
        const auto* mine = load<const std::byte*>(self, 8);
        if (load<std::uint32_t>(other, 0x418) != load<std::uint32_t>(mine, 0x418)) {
            return 1;
        }
    }
    auto* field = static_cast<std::byte*>(self) + 0x24;
    const auto* target = load<const std::byte*>(field, 0);
    const auto first = functionAt<Call1>(target, 0x10C);
    const auto left = first(field, 0);
    const auto* reloaded = load<const std::byte*>(field, 0);
    const auto second = functionAt<Call0>(reloaded, 0x78);
    const auto right = second(field);
    return static_cast<std::uint32_t>(left) < right ? 1U : 0U;
}

void infiltratorContainZeroSevenDwords(
    void* /*self*/, void* destination, std::uint32_t /*ignored*/) noexcept {
    for (int index = 0; index < 7; ++index) {
        store(destination, index * 4, 0U);
    }
}

void openContainStorePairIds(
    void* self, const void* first, std::int32_t count, const void* second) noexcept {
    if (count <= 1) {
        count = 1;
    }
    const auto firstId = load<std::uint32_t>(first, 0xDC);
    store(self, 0xFC, static_cast<std::uint32_t>(count));
    const auto secondId = load<std::uint32_t>(second, 0xDC);
    store(self, 0xF4, firstId);
    store(self, 0xF8, secondId);
}

std::uint8_t openContainPlaneAccepts(const void* self, const void* candidate) noexcept {
    if (candidate == nullptr) {
        return 0;
    }
    const auto* limitObject = load<const std::byte*>(self, -0x20);
    const float limit = loadFloat(limitObject, 0x18);
    if (limit < 0.0F) {
        return 0;
    }
    const auto* body = load<const std::byte*>(self, -0x1C);
    if (loadFloat(candidate, 0x40) - loadFloat(body, 0x40) > limit) {
        return 0;
    }
    const float dy = loadFloat(body, 0x3C) - loadFloat(candidate, 0x3C);
    const float dx = loadFloat(body, 0x38) - loadFloat(candidate, 0x38);
    const float dot = dy * loadFloat(candidate, 0x18) + dx * loadFloat(candidate, 8);
    return dot < 0.0F ? 0U : 1U;
}

std::uint8_t openContainPairPlaneAccepts(
    const void* self, const void* first, const void* second) noexcept {
    if (first == nullptr || second == nullptr) {
        return 0;
    }
    const auto* limitObject = load<const std::byte*>(self, -0x20);
    const float limit = loadFloat(limitObject, 0x18);
    if (limit < 0.0F) {
        return 0;
    }
    const auto* body = load<const std::byte*>(self, -0x1C);
    if (loadFloat(first, 0x40) - loadFloat(body, 0x40) > limit) {
        return 0;
    }
    const float ax = loadFloat(second, 0x38) - loadFloat(first, 0x38);
    const float ay = loadFloat(second, 0x3C) - loadFloat(first, 0x3C);
    const float bx = loadFloat(body, 0x38) - loadFloat(first, 0x38);
    const float by = loadFloat(body, 0x3C) - loadFloat(first, 0x3C);
    return (by * ay + bx * ax) < 0.0F ? 0U : 1U;
}

std::uint8_t openContainHasField78(const void* self) noexcept {
    return load<std::uint32_t>(self, 0x78) != 0 ? 1U : 0U;
}

std::uint32_t openContainBitClear(const void* self, const std::uint32_t* bitIndex) noexcept {
    const auto* mask = load<const std::byte*>(self, -0x20);
    if (load<std::uint8_t>(mask, 0x8D) == 0) {
        return 0;
    }
    const std::uint32_t index = *bitIndex;
    const std::uint32_t bit = 1U << (index & 0x1FU);
    const std::uint32_t word = load<std::uint32_t>(mask, static_cast<std::ptrdiff_t>((index >> 5U) * 4U + 0x24U));
    return (word & bit) != 0 ? 0U : 1U;
}

void openContainCopySevenDwords(const void* self, void* destination, std::uint32_t /*ignored*/) noexcept {
    const auto* source = load<const std::byte*>(self, -0x20) + 0x28;
    std::memcpy(destination, source, 7 * sizeof(std::uint32_t));
}

std::uint8_t beamSpecialAbilityAllowsTarget(const void* self, const void* target) noexcept {
    if (target == nullptr) {
        return 1;
    }
    const auto* gate = load<const std::byte*>(self, 4);
    if (load<std::uint8_t>(gate, 0x10F) == 0) {
        return 0;
    }
    const auto* mine = load<const std::byte*>(self, 8);
    return load<std::uint32_t>(target, 0x418) != load<std::uint32_t>(mine, 0x418) ? 1U : 0U;
}

std::uint8_t specialPowerUpdateIdMatches(const void* self, std::uint32_t id) noexcept {
    const auto* object = load<const std::byte*>(self, -0x20);
    return id == load<std::uint32_t>(object, 8) ? 1U : 0U;
}

void specialPowerUpdateReadFieldC(const void* self, std::uint32_t* outValue) noexcept {
    const auto* object = load<const std::byte*>(self, -0x24);
    *outValue = load<std::uint32_t>(object, 0x0C);
}

std::uint8_t specialPowerUpdateIdEquals(const void* self, std::uint32_t id) noexcept {
    const auto* object = load<const std::byte*>(self, -0x24);
    return load<std::uint32_t>(object, 8) == id ? 1U : 0U;
}

void specialPowerUpdateReadSlot1(const void* self, std::uint32_t* outValue) noexcept {
    const auto* object = load<const std::byte*>(self, -0x24);
    const auto* middle = load<const std::byte*>(object, 8);
    const auto* inner = load<const std::byte*>(middle, 8);
    const auto* vtable = load<const std::byte*>(inner, 0);
    *outValue = load<std::uint32_t>(vtable, 4);
}

void table00C00F4CStoreByte39(void* self, std::uint32_t value) noexcept {
    store(self, std::ptrdiff_t{0x39}, static_cast<std::uint8_t>(value));
}

void table00C00F4CReadField54(const void* self, std::uint32_t* outValue) noexcept {
    const auto* object = load<const std::byte*>(self, -0x0C);
    *outValue = load<std::uint32_t>(object, 0x54);
}

std::uint32_t uncombineSpecialPowerHasEntries(const void* self) noexcept {
    const auto* object = load<const std::byte*>(self, -8);
    const auto* list = load<const std::byte*>(object, 0x374);
    if (list == nullptr) {
        return 1;
    }
    const auto end = load<std::uint32_t>(list, 0x510);
    const auto begin = load<std::uint32_t>(list, 0x50C);
    const auto count = static_cast<std::int32_t>(end - begin) >> 2;
    return count == 0 ? 0U : 1U;
}

std::uint8_t weaponFireSpecialAbilityFlagBit8(const void* self) noexcept {
    const auto* object = load<const std::byte*>(self, 8);
    return static_cast<std::uint8_t>((load<std::uint32_t>(object, 0xBC) >> 8) & 1U);
}

std::uint32_t supplyTruckSumDwords(const void* self) noexcept {
    auto* cursor = load<const std::uint32_t*>(self, 0x30);
    const auto* end = load<const std::uint32_t*>(self, 0x34);
    std::uint32_t sum = 0;
    while (cursor != end) {
        sum += *cursor;
        ++cursor;
    }
    return sum;
}

float supplyTruckRangeScale(const void* self) noexcept {
    const auto* gateObject = load<const std::byte*>(self, -0x514);
    const auto* gate = load<const std::byte*>(gateObject, 0x418);
    const auto* valueObject = load<const std::byte*>(self, -0x518);
    const float value = loadFloat(valueObject, 0x58);
    if (load<std::uint32_t>(gate, 0x2C) == 1) {
        return value + value;
    }
    return value;
}

std::uint8_t dockUpdateIdPresentOrOpen(const void* self, const void* visitor) noexcept {
    const auto* flagObject = load<const std::byte*>(self, -0x20);
    if (load<std::uint32_t>(flagObject, 8) == 0xFFFFFFFFU) {
        return 1;
    }
    const std::uint32_t id = load<std::uint32_t>(visitor, 0xDC);
    auto* cursor = load<const std::uint32_t*>(self, 0x40);
    const auto bytes = static_cast<std::int32_t>(
        reinterpret_cast<std::uintptr_t>(load<const std::byte*>(self, 0x44)) -
        reinterpret_cast<std::uintptr_t>(load<const std::byte*>(self, 0x40)));
    const auto count = bytes >> 2;
    for (std::int32_t index = 0; index < count; ++index) {
        const std::uint32_t slot = *cursor;
        if (slot == 0 || slot == id) {
            return 1;
        }
        ++cursor;
    }
    return 0;
}

std::uint32_t dockUpdateSlotReady(const void* self, const void* visitor, std::int32_t index) noexcept {
    if (index < 0) {
        return 0;
    }
    const auto* ids = load<const std::uint32_t*>(self, 0x40);
    const auto* flags = load<const std::uint8_t*>(self, 0x50);
    const bool flag = flags[index] != 0;
    const bool matches = load<std::uint32_t>(visitor, 0xDC) == ids[index];
    const bool previousEmpty = index > 0 && ids[index - 1] == 0;
    return (matches && flag && previousEmpty) ? 1U : 0U;
}

void dockUpdateMarkId(void* self, const void* visitor) noexcept {
    const std::uint32_t id = load<std::uint32_t>(visitor, 0xDC);
    const auto initialBytes = static_cast<std::int32_t>(
        reinterpret_cast<std::uintptr_t>(load<const std::byte*>(self, 0x44)) -
        reinterpret_cast<std::uintptr_t>(load<const std::byte*>(self, 0x40)));
    if ((initialBytes >> 2) == 0) {
        return;
    }
    auto* cursor = load<std::uint32_t*>(self, 0x40);
    std::uint32_t index = 0;
    for (;;) {
        if (*cursor == id) {
            auto* flags = load<std::uint8_t*>(self, 0x50);
            flags[index] = 1;
            return;
        }
        ++index;
        ++cursor;
        const auto bytes = static_cast<std::int32_t>(
            reinterpret_cast<std::uintptr_t>(load<const std::byte*>(self, 0x44)) -
            reinterpret_cast<std::uintptr_t>(load<const std::byte*>(self, 0x40)));
        if (index >= static_cast<std::uint32_t>(bytes >> 2)) {
            return;
        }
    }
}

std::uint8_t defaultProductionExitTransformPoint(const void* self, float* outPoint) noexcept {
    const auto* matrix = load<const std::byte*>(self, 8);
    if (matrix == nullptr) {
        return 0;
    }
    const auto* point = load<const std::byte*>(load<const std::byte*>(self, 4), 8);
    const float x = loadFloat(point, 0);
    const float y = loadFloat(point, 4);
    const float z = loadFloat(point, 8);
    outPoint[0] = loadFloat(matrix, 0x10) * z + loadFloat(matrix, 0x0C) * y +
        loadFloat(matrix, 0x08) * x + loadFloat(matrix, 0x14);
    outPoint[1] = loadFloat(matrix, 0x18) * x + loadFloat(matrix, 0x1C) * y +
        loadFloat(matrix, 0x20) * z + loadFloat(matrix, 0x24);
    outPoint[2] = loadFloat(matrix, 0x28) * x + loadFloat(matrix, 0x2C) * y +
        loadFloat(matrix, 0x30) * z + loadFloat(matrix, 0x34);
    return 1;
}

std::uint8_t aiUpdatePathGoalActive(const void* self) noexcept {
    if (load<std::uint32_t>(self, 0x214) == 0) {
        return 0;
    }
    const auto mode = load<std::uint32_t>(self, 0x21C);
    if (mode != 1 && mode != 4) {
        return 0;
    }
    const auto* path = load<const std::byte*>(self, 0x150);
    if (path == nullptr || load<std::uint32_t>(path, 0x10) == 0) {
        return 0;
    }
    const auto* node = load<const std::byte*>(path, 0x24);
    if (node != nullptr) {
        const auto* child = load<const std::byte*>(node, 8);
        const auto* checked = child != nullptr ? child : node;
        if (load<std::uint32_t>(checked, 0x20) != 0x7FFFFFFFU) {
            return 1;
        }
    }
    if (load<std::uint32_t>(path, 0x10) == 0) {
        return 0;
    }
    const auto* again = load<const std::byte*>(path, 0x24);
    if (again == nullptr) {
        return 0;
    }
    return load<std::uint32_t>(again, 0x20) != 0x7FFFFFFFU ? 1U : 0U;
}

void aiUpdateClearGoalFloats(void* self) noexcept {
    store(self, 0x21C, 1U);
    storeFloat(self, 0x220, 0.0F);
    storeFloat(self, 0x224, 0.0F);
    storeFloat(self, 0x228, 0.0F);
}

void aiUpdateSetGoalPosition(void* self, const void* position) noexcept {
    if (load<std::uint32_t>(self, 0x21C) != 4) {
        auto* held = load<std::byte*>(self, 0x150);
        if (held != nullptr) {
            const auto count = load<std::int32_t>(held, 4) - 1;
            store(held, 4, count);
            if (count <= 0) {
                const auto* vtable = load<const std::byte*>(held, 0);
                const auto function = functionAt<Call1>(vtable, 0);
                function(held, 1);
            }
            store(self, 0x150, static_cast<std::uint32_t>(0));
        }
    }
    store(self, 0x21C, 4U);
    store(self, 0x220, load<std::uint32_t>(position, 0));
    store(self, 0x224, load<std::uint32_t>(position, 4));
    store(self, 0x228, load<std::uint32_t>(position, 8));
}

void aiUpdateSetGoalFloat(void* self, std::uint32_t floatBits) noexcept {
    store(self, 0x21C, 3U);
    store(self, 0x220, floatBits);
}

std::uint8_t aiUpdateLocomotorAllowsMove(const void* self) noexcept {
    if (load<std::uint32_t>(self, 0x208) == 8) {
        return 0;
    }
    const auto* locomotor = load<const std::byte*>(self, 0x214);
    if (locomotor == nullptr) {
        return 0;
    }
    const auto* info = load<const std::byte*>(locomotor, 4);
    if ((load<std::uint8_t>(info, 8) & 8U) != 0) {
        return 0;
    }
    const auto* object = load<const std::byte*>(self, 8);
    const auto flags = load<std::uint32_t>(object, 0x26C);
    return static_cast<std::uint8_t>((~static_cast<std::uint8_t>(flags >> 3)) & 1U);
}

void aiUpdatePulseModule3c(void* self, std::uint32_t /*ignored*/, std::uint32_t stored) noexcept {
    auto* module = load<std::byte*>(self, 0x3C);
    const auto* vtable = load<const std::byte*>(module, 0);
    const auto first = functionAt<Call0>(vtable, 0x34);
    first(module);
    store(self, 0x50, stored);
    module = load<std::byte*>(self, 0x3C);
    vtable = load<const std::byte*>(module, 0);
    const auto second = functionAt<Call1>(vtable, 0x1C);
    second(module, 0x6BU);
}

void fireWeaponUpdateReadFieldC(const void* self, std::uint32_t* outValue) noexcept {
    const auto* object = load<const std::byte*>(self, -0x0C);
    *outValue = load<std::uint32_t>(object, 0x0C);
}

void* invisibilityBroadcastAdjustor24(void* self) noexcept {
    if (self == nullptr) {
        return nullptr;
    }
    return static_cast<std::byte*>(self) + 0x24;
}

std::uint8_t productionUpdateConsumeMatchingLink(void* self, const void* key) noexcept {
    if (key == nullptr) {
        return 0;
    }
    auto* link = load<std::byte*>(self, 0x10);
    while (link != nullptr) {
        const auto token = load<std::uint32_t>(link, 0x38);
        const bool tagged = load<std::uint8_t>(link, 0x65) != 0;
        const bool kind = load<std::uint32_t>(link, 0x0C) == 2;
        if (!tagged && kind) {
            const auto* inner = load<const std::byte*>(load<const std::byte*>(key, 0x0C), 0);
            if (load<std::uint32_t>(inner, 4) == token) {
                if (load<std::uint8_t>(link, 0x64) == 0) {
                    return 0;
                }
                store(link, std::ptrdiff_t{0x64}, static_cast<std::uint8_t>(0));
                return 1;
            }
        }
        link = load<std::byte*>(link, 0x54);
    }
    return 0;
}

} // namespace ra3::gamelogic
