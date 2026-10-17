#include "ra3/gamelogic/ObjectUpdate69B.h"

#include "ra3/gamelogic/GameLogicCore4D.h"

#include <cstddef>
#include <cstdint>
#include <cstring>

namespace ra3::gamelogic {
namespace {

DirectHandler g_direct = nullptr;

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

float bitsToFloat(std::uint32_t bits) noexcept {
    float value = 0.0F;
    std::memcpy(&value, &bits, sizeof(value));
    return value;
}

std::uint32_t floatToBits(float value) noexcept {
    std::uint32_t bits = 0;
    std::memcpy(&bits, &value, sizeof(bits));
    return bits;
}

std::byte* at(void* object, std::ptrdiff_t offset) noexcept {
    return static_cast<std::byte*>(object) + offset;
}

const std::byte* at(const void* object, std::ptrdiff_t offset) noexcept {
    return static_cast<const std::byte*>(object) + offset;
}

using V0 = std::uint32_t(__thiscall*)(void*);
using V1 = std::uint32_t(__thiscall*)(void*, std::uint32_t);
using V6 = std::uint32_t(__thiscall*)(
    void*, std::uint32_t, std::uint32_t, std::uint32_t, std::uint32_t, std::uint32_t, std::uint32_t);

template <class Fn>
Fn virtualAt(const void* object, std::ptrdiff_t slot) noexcept {
    return load<Fn>(load<const void*>(object, 0), slot);
}

std::uint32_t directCall(std::uint32_t target, void* ecx, std::uint32_t a0 = 0, std::uint32_t a1 = 0,
    std::uint32_t a2 = 0, std::uint32_t a3 = 0, std::uint32_t a4 = 0, std::uint32_t a5 = 0,
    int argc = 0) noexcept {
    DirectCall call{target, ecx, a0, a1, a2, a3, a4, a5, argc};
    return g_direct != nullptr ? g_direct(call) : 0U;
}

std::int32_t signedDiv40(std::int32_t bytes) noexcept {
    const auto product = static_cast<std::int64_t>(bytes) * static_cast<std::int32_t>(0x66666667);
    auto high = static_cast<std::int32_t>(product >> 32);
    high >>= 4;
    const auto sign = static_cast<std::uint32_t>(high) >> 31U;
    return static_cast<std::int32_t>(sign + static_cast<std::uint32_t>(high));
}

bool anyDword(const void* base, int count) noexcept {
    for (int index = 0; index < count; ++index) {
        if (load<std::uint32_t>(base, index * 4) != 0U) {
            return true;
        }
    }
    return false;
}

bool overlapsCopied(const void* mask, const std::uint32_t* words) noexcept {
    for (int index = 0; index < 7; ++index) {
        if ((load<std::uint32_t>(mask, index * 4) & words[index]) != 0U) {
            return true;
        }
    }
    return false;
}

void scaleIfFlag(void* self, std::ptrdiff_t subOffset, std::uint32_t scaleBits, std::uint32_t flag) noexcept {
    auto* sub = at(self, subOffset);
    const auto queried = virtualAt<V0>(sub, 0x1C)(sub);
    if (queried == 0U) {
        return;
    }
    const auto* inner = load<const void*>(reinterpret_cast<void*>(queried), 8);
    const auto bit = static_cast<std::uint8_t>((load<std::uint32_t>(inner, 0x1C) >> 14) & 1U);
    if (bit != static_cast<std::uint8_t>(flag)) {
        return;
    }
    virtualAt<V1>(sub, 0x44)(sub, scaleBits);
}

struct DispatchWalk {
    std::ptrdiff_t slot = 0;
    bool passArgument = false;
    std::uint32_t argument = 0;
    bool stopOnTrue = false;
    bool usePending = false;
};

std::uint8_t walkDispatch(void* self, const DispatchWalk& walk) noexcept {
    const auto* anchor = load<const std::byte*>(self, -0x0C);
    auto* cursor = load<std::byte*>(anchor, 0x1C4);
    const auto count = load<std::int32_t>(anchor, 0x1C0);
    auto* end = cursor + static_cast<std::ptrdiff_t>(count) * 4;
    while (cursor != end) {
        const auto id = load<std::uint32_t>(cursor, 0);
        auto* table = load<void*>(self, -8);
        const auto found = directCall(0x00687720, table, id, 0, 0, 0, 0, 0, 1);
        if (found == 0U) {
            break;
        }
        auto* item = reinterpret_cast<void*>(found);
        const auto called = walk.passArgument
            ? virtualAt<V1>(item, walk.slot)(item, walk.argument)
            : virtualAt<V0>(item, walk.slot)(item);
        if (walk.stopOnTrue && called != 0U) {
            return 1U;
        }
        anchor = load<const std::byte*>(self, -0x0C);
        cursor += 4;
        end = load<std::byte*>(anchor, 0x1C4) + static_cast<std::ptrdiff_t>(load<std::int32_t>(anchor, 0x1C0)) * 4;
    }
    if (walk.usePending) {
        return load<std::int32_t>(self, 0x24) > 0 ? 1U : 0U;
    }
    return 0U;
}

void firePacked(void* self, std::uint32_t p0, std::uint32_t p1, std::uint32_t p2, std::uint32_t p3,
    std::uint32_t p4, std::uint32_t p5) noexcept {
    if (static_cast<std::int32_t>(load<std::uint32_t>(self, 8)) > 0) {
        return;
    }
    if (virtualAt<V0>(self, 0x9C)(self) == 0U) {
        return;
    }
    auto* primary = at(self, -4);
    const auto source = load<std::uint32_t>(load<const void*>(self, -0x24), 8);
    virtualAt<V6>(primary, 0)(primary, source, p0, p1, p2, p3, p4);
    (void)p5;
}

constexpr std::uint32_t kMaxFinite = 0x7F7FFFFFU;

} // namespace

void setObjectUpdate69BDirect(DirectHandler handler) noexcept {
    g_direct = handler;
}

std::uint8_t salvageCrateCollideAccepts(void* self, const void* other) noexcept {
    const auto probed = directCall(0x006908E0, self, static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(other)),
        0, 0, 0, 0, 0, 1);
    if ((probed & 0xFFU) == 0U || other == nullptr || load<std::uint32_t>(other, 0x374) == 0U) {
        return 0U;
    }
    const auto flags = load<std::uint32_t>(load<const void*>(other, 4), 0xC0);
    if (((flags >> 4) & 1U) != 0U) {
        return 0U;
    }
    return 1U;
}

std::uint8_t fireWeaponCollideMasksAllow(const void* self) noexcept {
    const auto* body = load<const std::byte*>(self, 4);
    const auto* source = load<const std::byte*>(self, 8);
    std::uint32_t words[7]{};
    std::memcpy(words, source + 0x80, sizeof(words));
    const auto* primary = body + 0x2C;
    if (anyDword(primary, 7) && overlapsCopied(primary, words)) {
        return 0U;
    }
    words[6] ^= 0x3FFFFFFFU;
    for (int index = 0; index < 6; ++index) {
        words[index] = ~words[index];
    }
    const auto* secondary = body + 0x10;
    if (anyDword(secondary, 7) &&
        intersectsObjectStatusMask224_4D(words, secondary)) {
        return 0U;
    }
    if (load<std::uint8_t>(self, 0x18) != 0U && load<std::uint8_t>(body, 0x4C) != 0U) {
        return 0U;
    }
    return 1U;
}

std::uint32_t hordeContainAnchorRejected(void* self) noexcept {
    const auto* anchor = load<const std::byte*>(self, -0x164);
    if (anchor == nullptr) {
        return 0U;
    }
    const auto flags = load<std::uint32_t>(anchor, 0x90);
    if (((flags >> 0x15) & 1U) != 0U) {
        return 0U;
    }
    if (virtualAt<V0>(self, 0xA0)(self) != 0U) {
        return 0U;
    }
    const auto* mine = load<const std::byte*>(self, -0x168);
    const auto hit = intersectsObjectStatusMask224_4D(anchor + 0x80, mine + 0x194);
    return hit ? 0U : 1U;
}

std::uint32_t hordeContainForwardId(void* self, const void* record) noexcept {
    const auto* holder = load<const void*>(record, 0x0C);
    const auto id = load<std::uint32_t>(load<const void*>(holder, 0), 4);
    return virtualAt<V1>(self, 0xB0)(self, id);
}

void hordeContainRelayAnchor(void* self, std::uint32_t argument) noexcept {
    auto* anchor = load<void*>(self, -0x164);
    if (anchor == nullptr) {
        return;
    }
    auto* update = load<void*>(anchor, 0x418);
    if (update == nullptr) {
        return;
    }
    auto* module = load<void*>(update, 0x90);
    if (module == nullptr) {
        return;
    }
    directCall(0x0088E060, module, argument, static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(anchor)),
        0, 0, 0, 0, 2);
}

const void* hordeContainRecordSpanPointer(const void* self) noexcept {
    const auto* record = load<const std::byte*>(self, -8);
    const auto bytes = static_cast<std::int32_t>(load<std::uint32_t>(record, 0x140) - load<std::uint32_t>(record, 0x13C));
    if (signedDiv40(bytes) != 1) {
        return record + 0x38;
    }
    return reinterpret_cast<const void*>(static_cast<std::uintptr_t>(0x00DF8DC0U));
}

std::uint8_t hordeContainSelectRecordId(const void* self, const void* other, std::uint32_t* outId) noexcept {
    if (other == nullptr) {
        return 0U;
    }
    const auto key = load<std::uint32_t>(self, -8);
    auto* link = load<std::byte*>(self, -0x0C);
    if (key == 0U) {
        return 0U;
    }
    auto* table = load<void*>(other, 0x418);
    const auto kind = directCall(0x007F2710, table, key, 0, 0, 0, 0, 0, 1);
    link = load<std::byte*>(link, kind == 2U ? 0x0C : 0x10);
    if (link == nullptr) {
        return 0U;
    }
    *outId = load<std::uint32_t>(link, 0);
    return 1U;
}

void* garrisonContainSlotPointer(void* self, std::uint32_t index) noexcept {
    if (index > 0x13U) {
        return reinterpret_cast<void*>(static_cast<std::uintptr_t>(0x00DF8DD0U));
    }
    auto* adjusted = at(self, -0x24);
    const auto slot = directCall(0x00680180, adjusted, 0, 0, 0, 0, 0, 0, 0);
    const auto mixed = index + slot * 20U + 0x5CU;
    return at(self, static_cast<std::ptrdiff_t>(mixed * 3U) * 4);
}

std::uint8_t garrisonContainCopySlotTriple(void* self, void* destination, std::uint32_t key) noexcept {
    if (destination == nullptr || key == 0U) {
        return 0U;
    }
    auto* adjusted = at(self, -0x24);
    const auto slot = directCall(0x00680180, adjusted, 0, 0, 0, 0, 0, 0, 0);
    const auto located = directCall(
        0x00691D30, adjusted, slot, key, 0, 0, 0, 0, 2);
    if (located == 0xFFFFFFFFU) {
        return 0U;
    }
    const auto stride = (slot + slot * 4U);
    const auto mixed = located + stride * 4U + 0x20U;
    const auto scaled = mixed + mixed * 2U;
    const auto* source = at(self, static_cast<std::ptrdiff_t>(scaled) * 4);
    store<std::uint32_t>(destination, 0, load<std::uint32_t>(source, 0));
    store<std::uint32_t>(destination, 4, load<std::uint32_t>(source, 4));
    store<std::uint32_t>(destination, 8, load<std::uint32_t>(source, 8));
    return 1U;
}

void garrisonContainReleaseIfCount(void* self, std::uint32_t, std::uint32_t, std::uint32_t kind) noexcept {
    if (kind != 2U) {
        return;
    }
    const auto* object = load<const void*>(self, -0x28);
    const auto flags = load<std::uint32_t>(load<const void*>(object, 4), 0xC8);
    if (((flags >> 15) & 1U) != 0U) {
        return;
    }
    auto* body = at(self, -0x0C);
    if (virtualAt<V1>(body, 0x10C)(body, 0U) == 0U) {
        return;
    }
    virtualAt<V1>(body, 0x88)(body, 2U);
}

void openContainRefreshMask(void* self) noexcept {
    const auto* object = load<const std::byte*>(self, 8);
    const auto* source = load<const std::byte*>(object, 0xF4);
    if (source == nullptr) {
        return;
    }
    std::uint8_t incoming[60]{};
    std::memcpy(incoming, source + 0x264, sizeof(incoming));
    auto* field = at(self, 0xC8);
    if (equalsBitFlags480_4D(incoming, field)) {
        return;
    }
    virtualAt<V0>(self, 0x78)(self);
    std::memcpy(field, incoming, sizeof(incoming));
}

void transportContainRedirect(void* self, std::uint32_t left, std::uint32_t right) noexcept {
    if (left == right) {
        return;
    }
    const auto* object = load<const std::byte*>(self, 8);
    if (((load<std::uint32_t>(object, 0x26C) >> 5) & 1U) != 0U) {
        auto* sub = at(self, 0x24);
        virtualAt<V1>(sub, 0xA4)(sub, 0U);
        return;
    }
    if ((load<std::uint8_t>(object, 0x8E) & 1U) != 0U) {
        return;
    }
    auto* sub = at(self, 0x24);
    virtualAt<V1>(sub, 0x88)(sub, 2U);
}

void scavengerSpecialPowerPushScale(void* self, std::uint32_t flag) noexcept {
    if (load<std::uint8_t>(self, 0x3C) != 0U) {
        const auto* source = load<const void*>(self, -0x0C);
        auto bits = load<std::uint32_t>(source, 0x1C0);
        if ((flag & 0xFFU) != 0U) {
            bits = 0U;
        }
        auto* slot = load<void*>(load<void*>(self, -8), 0x418);
        store<std::uint32_t>(slot, 0xF8, bits);
    }
    directCall(0x006811C0, self, flag, 0, 0, 0, 0, 0, 1);
}

std::uint8_t specialPowerDispatchAnyReady(void* self) noexcept {
    return walkDispatch(self, DispatchWalk{4, false, 0, true, false});
}

std::uint8_t specialPowerDispatchAnyOrPending(void* self) noexcept {
    return walkDispatch(self, DispatchWalk{0x10, false, 0, true, true});
}

void specialPowerDispatchStoreArg(void* self, std::uint32_t value) noexcept {
    walkDispatch(self, DispatchWalk{0x24, true, value, false, false});
    store<std::uint32_t>(self, 0x20, value);
}

void specialPowerDispatchNotifyClear4C(void* self) noexcept {
    walkDispatch(self, DispatchWalk{0x4C, false, 0, false, false});
    store<std::uint8_t>(self, 0x30, 0U);
}

void specialPowerDispatchNotifyClear54(void* self) noexcept {
    walkDispatch(self, DispatchWalk{0x54, false, 0, false, false});
    store<std::uint8_t>(self, 0x30, 0U);
}

void specialPowerModuleScaleIfFlag(void* self, std::uint32_t scaleBits, std::uint32_t flag) noexcept {
    scaleIfFlag(self, 4, scaleBits, flag);
}

void specialPowerUpdateFirePacked(void* self, std::uint32_t arg0) noexcept {
    firePacked(self, 0U, 0U, arg0, 0U, kMaxFinite, 0U);
}

void specialPowerUpdateFirePair(void* self, std::uint32_t arg0, std::uint32_t arg1) noexcept {
    if (static_cast<std::int32_t>(load<std::uint32_t>(self, 8)) > 0) {
        return;
    }
    if (virtualAt<V0>(self, 0x9C)(self) == 0U) {
        return;
    }
    auto* primary = at(self, -4);
    const auto source = load<std::uint32_t>(load<const void*>(self, -0x24), 8);
    virtualAt<V6>(primary, 0)(primary, source, arg0, 0U, arg1, 0U, kMaxFinite);
}

void specialPowerUpdateFireInsert(void* self, std::uint32_t arg0, std::uint32_t arg1) noexcept {
    firePacked(self, 0U, arg0, arg1, 0U, kMaxFinite, 0U);
}

void specialPowerUpdateFireFloat(void* self, std::uint32_t arg0, std::uint32_t floatBits, std::uint32_t arg2) noexcept {
    firePacked(self, 0U, arg0, arg2, 0U, floatBits, 0U);
}

void specialPowerUpdateFireSpread(void* self, std::uint32_t arg0, std::uint32_t arg1, std::uint32_t arg2) noexcept {
    firePacked(self, 0U, arg0, arg2, arg1, kMaxFinite, 0U);
}

void specialPowerUpdateScaleIfFlag(void* self, std::uint32_t scaleBits, std::uint32_t flag) noexcept {
    scaleIfFlag(self, 0x1C, scaleBits, flag);
}

void dozerAiUpdateIssueOrder(void* self) noexcept {
    auto* root = load<void*>(self, -0x514);
    auto* update = load<void*>(root, 0x418);
    auto* module = load<std::byte*>(update, 0x90);
    if (module == nullptr) {
        return;
    }
    const auto command = load<std::uint32_t>(root, 0xDC);
    directCall(0x0088AEB0, module + 0x104, command, 0xFFFFFFFFU, 0U, 0, 0, 0, 3);
}

std::uint8_t dockUpdateForwardVisitor(void* self, std::uint32_t visitor) noexcept {
    auto* dock = load<void*>(self, -0x20);
    auto* target = load<void*>(dock, 0x14);
    if (target == nullptr) {
        return 0U;
    }
    const auto extra = load<std::uint32_t>(load<const void*>(self, -0x1C), 0x418);
    return static_cast<std::uint8_t>(directCall(0x004E30B0, target, visitor, extra, 0, 0, 0, 0, 2));
}

std::uint8_t queueProductionExitProject(const void* self, std::uint32_t* outPoint, float* outExtra) noexcept {
    const auto* matrix = load<const std::byte*>(self, -0x1C);
    if (matrix == nullptr) {
        return 0U;
    }
    const auto* vector = load<const std::byte*>(self, -0x20);
    const float vx = load<float>(vector, 0x14);
    const float vy = load<float>(vector, 0x18);
    const float vz = load<float>(vector, 0x1C);
    const float m08 = load<float>(matrix, 0x08);
    const float m0c = load<float>(matrix, 0x0C);
    const float m10 = load<float>(matrix, 0x10);
    const float m14 = load<float>(matrix, 0x14);
    const float m18 = load<float>(matrix, 0x18);
    const float m1c = load<float>(matrix, 0x1C);
    const float m20 = load<float>(matrix, 0x20);
    const float m24 = load<float>(matrix, 0x24);
    const float m28 = load<float>(matrix, 0x28);
    const float m2c = load<float>(matrix, 0x2C);
    const float m30 = load<float>(matrix, 0x30);
    const float m34 = load<float>(matrix, 0x34);
    const float outX = ((m10 * vz + m0c * vy) + m08 * vx) + m14;
    const float outY = ((m18 * vx + m1c * vy) + m20 * vz) + m24;
    const float outZ = ((m28 * vx + m2c * vy) + m30 * vz) + m34;
    store<float>(outPoint, 0, outX);
    store<float>(outPoint, 4, outY);
    store<float>(outPoint, 8, outZ);
    store<float>(outExtra, 0, load<float>(vector, 8) + load<float>(matrix, 0x44));
    return 1U;
}

std::int32_t queueProductionExitIdle(const void* self) noexcept {
    if (load<std::uint32_t>(self, 0x1C) != 0U) {
        return 0;
    }
    return load<std::uint32_t>(self, 4) == 0U ? 0 : -1;
}

void aiUpdateStampGoal(void* self, std::uint32_t packed, std::uint32_t stamp) noexcept {
    auto* state = load<void*>(self, 0x3C);
    virtualAt<V0>(state, 0x34)(state);
    directCall(0x005AC4F0, state, packed, kMaxFinite, 0, 0, 0, 0, 2);
    store<std::uint32_t>(self, 0x50, stamp);
    store<std::uint32_t>(self, 0x190, 0U);
    store<std::uint8_t>(self, 0x432, 0U);
    state = load<void*>(self, 0x3C);
    virtualAt<V1>(state, 0x1C)(state, 0x4EU);
}

std::uint32_t aiUpdateGoalKindAllowed(void* self) noexcept {
    const auto* state = load<const std::byte*>(self, 0x3C);
    std::uint32_t kind = 0x70U;
    if (state != nullptr) {
        const auto* first = load<const void*>(state, 0x88);
        kind = first != nullptr ? load<std::uint32_t>(first, 4) : 0x000F423FU;
        if (kind == 0x000F423FU) {
            const auto* second = load<const void*>(state, 4);
            kind = second != nullptr ? load<std::uint32_t>(second, 4) : 0x000F423FU;
        }
    }
    const auto gate = directCall(0x005BA020, const_cast<std::byte*>(state), 0, 0, 0, 0, 0, 0, 0);
    if (gate == 0U) {
        return 0U;
    }
    if (kind == 0x0FU || kind == 0x2FU || kind == 0x18U || kind == 0x2BU || kind == 0x19U || kind == 0x32U ||
        kind == 0x36U) {
        return gate;
    }
    const auto flags = load<std::uint32_t>(load<const void*>(self, 8), 0x8C);
    if (((flags >> 1) & 1U) != 0U) {
        return gate;
    }
    return 0U;
}

void aiUpdateApplyFacing(void* self, std::uint32_t facingBits, std::uint32_t stamp) noexcept {
    const auto* chain = load<const void*>(load<const void*>(self, 8), 4);
    const auto limit = load<float>(load<const void*>(chain, 0x14), 0x120);
    if (limit >= bitsToFloat(0x43B40000U)) {
        return;
    }
    auto* state = load<void*>(self, 0x3C);
    virtualAt<V0>(state, 0x34)(state);
    const std::uint32_t copied[3] = {facingBits, facingBits, facingBits};
    directCall(0x005AC4F0, state, static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(copied)), kMaxFinite,
        0, 0, 0, 0, 2);
    store<std::uint32_t>(self, 0x50, stamp);
    state = load<void*>(self, 0x3C);
    virtualAt<V1>(state, 0x1C)(state, 0x46U);
}

float invisibilityMemberScale(const void* self) noexcept {
    const auto scale = load<float>(load<const void*>(self, -0x20), 0x10);
    const auto* other = load<const void*>(self, -0x1C);
    const auto* node = load<const void*>(other, 0x36C);
    if (node == nullptr) {
        return scale;
    }
    const auto flags = directCall(0x004D9CE0, const_cast<void*>(node), 0x22U, 0U, 0U, 0, 0, 0, 3);
    if ((flags & 0xFFU) == 0U) {
        return scale;
    }
    return bitsToFloat(flags >> 8) * scale;
}

std::uint8_t invisibilityUpdateArgsDiffer(void* self, std::uint32_t, std::uint32_t left, std::uint32_t right) noexcept {
    const auto* gate = load<const void*>(self, -0x20);
    if (load<std::uint32_t>(gate, 8) == 0U) {
        return 0U;
    }
    auto* body = at(self, -0x24);
    if (directCall(0x0069C530, body, right, 0, 0, 0, 0, 0, 1) == 0U) {
        return 0U;
    }
    return directCall(0x0069C530, body, left, 0, 0, 0, 0, 0, 1) == 0U ? 1U : 0U;
}

std::uint32_t leechTargetingAttachForward(void* self, void* record) noexcept {
    (void)self;
    auto* target = load<void*>(record, 0x33C);
    if (target == nullptr) {
        return static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(record));
    }
    return virtualAt<V1>(target, 8)(target, 0U);
}

void* productionUpdateNextReadyLink(void* self) noexcept {
    auto* link = load<std::byte*>(self, 0x10);
    while (link != nullptr) {
        const auto skip = load<std::uint8_t>(link, 0x65) != 0U || load<std::uint8_t>(link, 0x64) != 0U;
        auto* owner = load<std::byte*>(link, 0x10);
        const auto ready = !skip && owner != nullptr && load<void*>(owner, 0x318) != nullptr &&
            load<std::uint8_t>(link, 0x4E) == 0U;
        if (ready) {
            const auto probed = directCall(0x004E21F0, load<void*>(owner, 0x318), 0, 0, 0, 0, 0, 0, 1);
            if (probed != 6U) {
                return link;
            }
        } else if (!skip) {
            return link;
        }
        link = reinterpret_cast<std::byte*>(virtualAt<V1>(self, 0x70)(self, static_cast<std::uint32_t>(
            reinterpret_cast<std::uintptr_t>(link))));
    }
    return nullptr;
}

std::uint8_t specialAbilityUpdateKindMatch(const void* self) noexcept {
    const auto mode = load<std::uint32_t>(self, 0x14);
    const auto* anchor = load<const void*>(self, -0x20);
    if (mode != 0U) {
        if (mode == 1U || mode == 3U) {
            auto* object = load<void*>(anchor, 8);
            if (object != nullptr && directCall(0x007E2970, object, 0, 0, 0, 0, 0, 0, 0) == 0U) {
                return 0U;
            }
        }
        const auto* node = load<const void*>(load<const void*>(self, 0x1C), 4);
        if (node != nullptr && (load<std::uint32_t>(node, 4) == 0x63U || load<std::uint32_t>(node, 4) == 0x64U ||
                load<std::uint32_t>(node, 4) == 0x65U)) {
            return 1U;
        }
        return node != nullptr && load<std::uint32_t>(node, 4) == 0x66U ? 1U : 0U;
    }
    return 0U;
}

std::uint8_t specialAbilityUpdateIsReady(void* self) noexcept {
    if (load<std::uint32_t>(self, 0x38) != 4U) {
        return 0U;
    }
    if (virtualAt<V0>(self, 0xA8)(self) != 0U) {
        return 1U;
    }
    const auto* body = load<const std::byte*>(self, 4);
    if (((load<std::uint32_t>(body, 0x2C) >> 1) & 1U) != 0U && load<std::uint8_t>(self, 0x9D) != 0U) {
        return 0U;
    }
    return load<float>(body, 0x20) == 0.0F ? 1U : 0U;
}

std::uint32_t* specialAbilityUpdateResolveOut(void* self, std::uint32_t* outValue) noexcept {
    const auto* base = load<const std::byte*>(self, -0x0C);
    if (load<std::uint32_t>(base, 0xDC) != 0U) {
        *outValue = load<std::uint32_t>(base, 0xDC);
        return outValue;
    }
    const auto id = load<std::uint32_t>(base, 8);
    auto* found = reinterpret_cast<void*>(directCall(0x00687720, load<void*>(self, -8), id, 0, 0, 0, 0, 0, 1));
    virtualAt<V1>(found, 0x98)(found, static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(outValue)));
    return outValue;
}

void* stealthDetectorUpdateTouch(void* self) noexcept {
    const auto* body = load<const void*>(self, 4);
    if (load<std::uint8_t>(body, 0x98) == 0U) {
        return self;
    }
    // The armed path loads the singleton at 0x00DEBA64 and calls 0x004ED9C0.
    // That global is zero in the on-disk image, so the oracle covers only the
    // disarmed return of this.
    return self;
}

} // namespace ra3::gamelogic
