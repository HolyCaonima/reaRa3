#include "ra3/gamelogic/ObjectModules6E.h"

#include <cstring>

namespace ra3::gamelogic {
namespace {

Object6ECalls g_calls{};
std::uint8_t* g_clock = nullptr;
std::uint32_t g_st0 = 0;

constexpr std::uint32_t kMaxFloat = 0x7F7FFFFFu;      // 0x00BE700C
constexpr std::uint32_t kReadyThreshold = 0x4B189680u;  // 0x00BD9F94
constexpr std::uint32_t kOne = 0x3F800000u;             // 0x00BD90A4

std::uint32_t loadU32(const void* address) noexcept {
    std::uint32_t value = 0;
    std::memcpy(&value, address, 4);
    return value;
}

void storeU32(void* address, std::uint32_t value) noexcept {
    std::memcpy(address, &value, 4);
}

std::uint8_t* asPtr(std::uint32_t value) noexcept {
    return reinterpret_cast<std::uint8_t*>(static_cast<std::uintptr_t>(value));
}

std::uint32_t asBits(const void* pointer) noexcept {
    return static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(pointer));
}

bool feq(std::uint32_t left, std::uint32_t right) noexcept {
    float a = 0.f;
    float b = 0.f;
    std::memcpy(&a, &left, 4);
    std::memcpy(&b, &right, 4);
    return a == b;
}

// comiss a, b / jbe : jump when a <= b or unordered.
bool comissJbe(std::uint32_t aBits, std::uint32_t bBits) noexcept {
    float a = 0.f;
    float b = 0.f;
    std::memcpy(&a, &aBits, 4);
    std::memcpy(&b, &bBits, 4);
    const bool nan = (a != a) || (b != b);
    return nan || a <= b;
}

std::uint32_t withLow(std::uint32_t value, std::uint8_t low) noexcept {
    return (value & 0xFFFFFF00u) | low;
}

bool bitWasClear(std::uint32_t old, unsigned shift) noexcept {
    return (((old >> shift) ^ 0xFFu) & 1u) != 0;
}

std::uint32_t vcallN(std::uint8_t* self, int slot) noexcept {
    auto* vt = *reinterpret_cast<std::uint8_t**>(self);
    auto* fn = *reinterpret_cast<std::uint32_t(__fastcall**)(void*, void*)>(vt + slot);
    return fn(self, nullptr);
}

std::uint32_t vcallU(std::uint8_t* self, int slot, std::uint32_t arg) noexcept {
    auto* vt = *reinterpret_cast<std::uint8_t**>(self);
    auto* fn = *reinterpret_cast<std::uint32_t(__fastcall**)(void*, void*, std::uint32_t)>(vt + slot);
    return fn(self, nullptr, arg);
}

std::uint32_t vcallP(std::uint8_t* self, int slot, std::uint32_t arg) noexcept {
    return vcallU(self, slot, arg);
}

std::uint32_t vcallW(std::uint8_t* self, int slot, std::uint32_t first, std::uint32_t second) noexcept {
    auto* vt = *reinterpret_cast<std::uint8_t**>(self);
    auto* fn = *reinterpret_cast<std::uint32_t(__fastcall**)(void*, void*, std::uint32_t, std::uint32_t)>(
        vt + slot);
    return fn(self, nullptr, first, second);
}

std::uint32_t vcallS(std::uint8_t* self, int slot, std::uint32_t field) noexcept {
    return vcallU(self, slot, field);
}

std::uint8_t* firstOf(std::uint8_t* owner) noexcept {
    const std::uint32_t direct = loadU32(owner + 0x328);
    if (direct != 0) {
        return asPtr(direct);
    }
    const std::uint32_t begin = loadU32(owner + 0x32C);
    const std::uint32_t end = loadU32(owner + 0x330);
    if (begin == end || begin == 0) {
        return nullptr;
    }
    const std::uint32_t value = loadU32(asPtr(begin));
    return value == 0 ? nullptr : asPtr(value);
}

std::uint32_t lookupKey(std::uint32_t key) noexcept {
    g_calls.lock(nullptr, nullptr);
    const std::uint32_t found = g_calls.find(nullptr, nullptr, key);
    g_calls.unlock(nullptr, nullptr);
    return found;
}

std::uint32_t gateForward(std::uint8_t* self, int slot, bool clearLow) noexcept {
    const std::uint32_t ready = g_calls.d6d6ec0(self, nullptr);
    if ((ready & 0xFFu) != 0) {
        auto* inner = asPtr(loadU32(self + 0x50));
        return vcallN(inner, slot);
    }
    return clearLow ? (ready & 0xFFFFFF00u) : ready;
}

}  // namespace

void setObject6ECalls(const Object6ECalls& calls) noexcept { g_calls = calls; }

void setObject6EClock(std::uint8_t* clock) noexcept { g_clock = clock; }

void setObject6ESt0(std::uint32_t bits) noexcept { g_st0 = bits; }

std::uint32_t __fastcall MovementSpecialPower_adjustedField(std::uint8_t* self, void*) noexcept {
    if (asBits(self) == 0x0Cu) {
        return 0;
    }
    return asBits(self + 0x40);
}

std::uint32_t __fastcall MoveToPositionAndEvacuateSpecialPower_dispatch(std::uint8_t* self, void*,
                                                                        std::uint32_t arg0, std::uint32_t arg1,
                                                                        std::uint32_t arg2) noexcept {
    auto* owner = asPtr(loadU32(self - 8));
    const std::uint32_t direct = loadU32(owner + 0x328);
    if (direct == 0) {
        const std::uint32_t begin = loadU32(owner + 0x32C);
        const std::uint32_t end = loadU32(owner + 0x330);
        if (begin == end || begin == 0 || loadU32(asPtr(begin)) == 0) {
            return 0;
        }
    }
    auto* passenger = firstOf(owner);
    const std::uint32_t count = vcallU(passenger, 0x10C, 0);
    if (count == 0) {
        return 0;
    }
    return g_calls.d6cace0(self, nullptr, arg0, arg1, arg2);
}

std::uint32_t __fastcall MoveToPositionAndReplaceSelfSpecialPower_dispatch(std::uint8_t* self, void*,
                                                                           std::uint32_t arg0, std::uint32_t arg1,
                                                                           std::uint32_t arg2) noexcept {
    return g_calls.d6cace0(self, nullptr, arg0, arg1, arg2);
}

std::uint32_t __fastcall OCLSpecialPower_dispatch(std::uint8_t* self, void*, std::uint32_t arg0, std::uint32_t arg1,
                                                  std::uint32_t arg2) noexcept {
    auto* body = asPtr(loadU32(self - 0x0C));
    const std::uint32_t probe = loadU32(body + 0x1C0);
    std::uint32_t allow = 1;
    if (probe != 0 && arg0 != 0) {
        const std::uint32_t other = loadU32(self - 8);
        if (other != 0) {
            allow = g_calls.d6a7b60(asPtr(probe), nullptr, other, arg0);
        }
    }
    const std::uint32_t limit = loadU32(body + 0x1CC);
    if (limit != 0 && loadU32(self + 0x40) >= limit) {
        return allow & 0xFFFFFF00u;
    }
    if ((allow & 0xFFu) == 0) {
        return 0;
    }
    const std::uint32_t result = g_calls.d6cace0(self, nullptr, arg0, arg1, arg2);
    if ((result & 0xFFu) == 0) {
        return 0;
    }
    return 1;
}

std::uint32_t __fastcall SpecialPowerModule_dispatchIfBit25(std::uint8_t* self, void*) noexcept {
    auto* middle = asPtr(loadU32(self + 8));
    auto* inner = asPtr(loadU32(middle + 4));
    const std::uint32_t flags = loadU32(inner + 0xCC);
    const std::uint32_t shifted = flags >> 0x19;
    if ((shifted & 1u) == 0) {
        return shifted;
    }
    return g_calls.d6cab20(self, nullptr);
}

std::uint32_t __fastcall SpecialPowerModule_dispatchUnlessBit6(std::uint8_t* self, void*) noexcept {
    auto* body = asPtr(loadU32(self - 0x0C));
    const std::uint32_t flags = loadU32(body + 0x90);
    if (((flags >> 6) & 1u) != 0) {
        return asBits(body);
    }
    return g_calls.d6cab20(self - 0x14, nullptr);
}

std::uint32_t __fastcall ToggleHiddenSpecialAbilityUpdate_clearHidden(std::uint8_t* self, void*) noexcept {
    const std::uint32_t objectBits = loadU32(self + 8);
    if (objectBits == 0) {
        return 0;
    }
    auto* object = asPtr(objectBits);
    const std::uint32_t hidden = loadU32(object + 0x80) >> 0x12;
    if ((hidden & 1u) == 0) {
        return hidden;
    }
    std::uint32_t result = g_calls.d4d3b50(object, nullptr, 0x12);
    if (((loadU32(object + 0xC8) >> 0x0A) & 1u) != 0) {
        storeU32(object + 0xC8, loadU32(object + 0xC8) & 0xFFFFFBFFu);
        g_calls.d6a4fb0(object, nullptr);
    }
    const std::uint32_t key = loadU32(asPtr(loadU32(self + 4)) + 8);
    const std::uint32_t found = g_calls.d687720(object, nullptr, key);
    if (found != 0) {
        vcallU(asPtr(found), 0x28, 0);
    }
    result = g_calls.d4d3b50(object, nullptr, 0x14);
    return result;
}

std::uint32_t __fastcall ToggleHiddenSpecialAbilityUpdate_applyHidden(std::uint8_t* self, void*) noexcept {
    auto* face = self + 0x24;
    const std::uint32_t probed = vcallW(face, 0x18, 0, kMaxFloat);
    if ((probed & 0xFFu) == 0) {
        return probed;
    }
    const std::uint32_t objectBits = loadU32(self + 8);
    if (objectBits == 0) {
        return probed;
    }
    auto* object = asPtr(objectBits);
    if (((loadU32(object + 0x80) >> 0x12) & 1u) != 0) {
        return probed;
    }
    const std::uint32_t host = loadU32(object + 0x374);
    if (host == 0) {
        return probed;
    }
    const std::uint32_t key = loadU32(asPtr(loadU32(self + 4)) + 8);
    const std::uint32_t found = g_calls.d687720(object, nullptr, key);
    if (found != 0) {
        vcallU(asPtr(found), 0x28, 1);
    }
    const std::uint32_t contained = g_calls.d687560(object, nullptr);
    std::uint32_t open = contained;
    if ((contained & 0xFFu) == 0) {
        open = vcallN(asPtr(host), 0x20C);
    }
    if ((open & 0xFFu) != 0) {
        g_calls.d54f490(asPtr(host + 0x24), nullptr, 2);
    }
    storeU32(self + 0xB8, loadU32(g_clock + 0x58));
    const std::uint32_t oldHidden = loadU32(object + 0x80);
    storeU32(object + 0x80, oldHidden | 0x00040000u);
    if (bitWasClear(oldHidden, 0x12)) {
        g_calls.d6d1ed0(object, nullptr, 0x12, 1);
    }
    const std::uint32_t oldDraw = loadU32(object + 0xC8);
    if (((oldDraw >> 0x0A) & 1u) == 0) {
        storeU32(object + 0xC8, oldDraw | 0x400u);
        g_calls.d6a4fb0(object, nullptr);
    }
    const std::uint32_t oldMask = loadU32(object + 0x80);
    storeU32(object + 0x80, oldMask | 0x00100000u);
    if (bitWasClear(oldMask, 0x14)) {
        return g_calls.d6d1ed0(object, nullptr, 0x14, 1);
    }
    return (oldMask >> 0x14) & 0xFFFFFF00u;
}

std::uint32_t __fastcall DozerAIUpdate_acceptsObject(std::uint8_t* self, void*, std::uint32_t object) noexcept {
    if (object == 0) {
        return 0;
    }
    const std::uint32_t state = vcallN(self, 0x24);
    if (state != 1) {
        return withLow(state, 1);
    }
    const std::uint32_t key = loadU32(self + 0x0C);
    if (key == 0) {
        return 1;
    }
    const std::uint32_t found = lookupKey(key);
    if (found != 0 && found == object) {
        return 0;
    }
    return 1;
}

std::uint32_t __fastcall DozerAIUpdate_resetAction(std::uint8_t* self, void*, std::uint32_t arg) noexcept {
    vcallU(self, 0x3C, arg);
    auto* nested = asPtr(loadU32(self + 0x1C));
    vcallN(nested, 0x18);
    const std::uint32_t active = vcallN(self, 0x6C);
    if (active != 0) {
        vcallN(self, 0x64);
    }
    (void)loadU32(self - 0x514);
    return g_calls.d6e3a60(0, 0);
}

std::uint32_t __fastcall DozerAIUpdate_workerFromId(std::uint8_t* self, void*) noexcept {
    const std::uint32_t key = loadU32(self + 0xC0);
    if (key == 0) {
        return 0;
    }
    return lookupKey(key);
}

std::uint32_t __fastcall HordeAIUpdate_flagPassenger(std::uint8_t* self, void*, std::uint32_t object,
                                                    std::uint32_t) noexcept {
    auto* owner = asPtr(loadU32(self - 0x514));
    auto* passenger = firstOf(owner);
    const std::uint32_t ai = vcallN(passenger, 0x84);
    if (ai == 0) {
        return 0;
    }
    const std::uint32_t old = loadU32(asPtr(object) + 0x84);
    storeU32(asPtr(object) + 0x84, old | 0x100u);
    if (bitWasClear(old, 8)) {
        g_calls.d6d1ed0(asPtr(object), nullptr, 0x28, 1);
    }
    vcallP(asPtr(ai), 0x1C, object);
    return 0;
}

std::uint32_t __fastcall SupplyTruckAIUpdate_acceptTarget(std::uint8_t* self, void*, std::uint32_t target) noexcept {
    if (target == 0) {
        vcallU(self + 0x51C, 0x5C, 1);
    }
    return g_calls.d6cc2e0(self, nullptr, target);
}

std::uint32_t __fastcall SupplyTruckAIUpdate_deliverBoxes(std::uint8_t* self, void*, std::uint32_t object,
                                                          std::uint32_t value) noexcept {
    auto* body = asPtr(loadU32(self + 8));
    std::uint32_t ready = g_calls.d6c0ed0(body, nullptr);
    if ((ready & 0xFFu) != 0) {
        auto* machine = asPtr(loadU32(self + 0x3C));
        vcallN(machine, 0x34);
        vcallP(machine, 0x38, object);
        storeU32(self + 0x50, value);
        ready = vcallU(machine, 0x1C, 0x0E);
    }
    if (value == 0 && object != 0) {
        const std::uint32_t ident = loadU32(asPtr(object) + 0xDC);
        storeU32(self + 0x524, ident);
        ready = ident;
    }
    return ready;
}

std::uint32_t __fastcall SupplyTruckAIUpdate_deliverPosition(std::uint8_t* self, void*, std::uint32_t position,
                                                             std::uint32_t value) noexcept {
    auto* body = asPtr(loadU32(self + 8));
    const std::uint32_t flags = loadU32(body + 0x88);
    if (((flags >> 0x0C) & 1u) != 0) {
        return flags >> 0x0C;
    }
    std::uint32_t ready = g_calls.d6c0ed0(body, nullptr);
    if ((ready & 0xFFu) != 0) {
        auto* machine = asPtr(loadU32(self + 0x3C));
        vcallN(machine, 0x34);
        g_calls.d5ac4f0(machine, nullptr, position, kMaxFloat);
        storeU32(self + 0x50, value);
        ready = vcallU(machine, 0x1C, 0x2D);
    }
    if (value == 0 && position != 0) {
        auto* src = asPtr(position);
        storeU32(self + 0x528, loadU32(src));
        storeU32(self + 0x52C, loadU32(src + 4));
        const std::uint32_t third = loadU32(src + 8);
        storeU32(self + 0x530, third);
        self[0x534] = 1;
        ready = third;
    }
    return ready;
}

std::uint32_t __fastcall SupplyTruckAIUpdate_recordCargo(std::uint8_t* self, void*, std::uint32_t object,
                                                         std::uint32_t value) noexcept {
    auto* body = asPtr(loadU32(self + 8));
    const std::uint32_t flags = loadU32(body + 0x88);
    if (((flags >> 0x0C) & 1u) != 0) {
        return flags >> 0x0C;
    }
    std::uint32_t ready = g_calls.d6c0ed0(body, nullptr);
    if ((ready & 0xFFu) != 0) {
        auto* machine = asPtr(loadU32(self + 0x3C));
        vcallN(machine, 0x34);
        vcallP(machine, 0x38, object);
        storeU32(self + 0x50, value);
        ready = vcallU(machine, 0x1C, 0x2D);
    }
    const std::uint32_t cargo = loadU32(asPtr(object) + 0xE4);
    storeU32(self + 0x53C, cargo);
    if (value == 0) {
        storeU32(self + 0x538, cargo);
    }
    return ready;
}

std::uint32_t __fastcall MoneyGainAttachUpdate_maybeGain(std::uint8_t* self, void*, std::uint32_t blocked) noexcept {
    if (blocked != 0) {
        return 0;
    }
    auto* record = asPtr(loadU32(self + 4));
    if (loadU32(record + 0x14C) != 0) {
        return asBits(record);
    }
    const std::uint32_t key = loadU32(self + 0x2C);
    if (key == 0) {
        return 0;
    }
    const std::uint32_t found = lookupKey(key);
    if (found == 0) {
        return 0;
    }
    const std::uint32_t module = loadU32(asPtr(found) + 0x33C);
    if (module == 0) {
        return 0;
    }
    const std::uint32_t info = vcallN(asPtr(module), 0x64);
    if (info == 0) {
        return 0;
    }
    const std::uint32_t kind = loadU32(asPtr(info) + 0x34);
    if (kind == 0x12 || kind == 0x19) {
        return kind;
    }
    return g_calls.d69d570(self, nullptr, found);
}

std::uint32_t __fastcall ProductionUpdate_producerField(std::uint8_t*, void*, std::uint32_t object) noexcept {
    if (object == 0) {
        return 0;
    }
    return loadU32(asPtr(object) + 0x54);
}

std::uint32_t __fastcall ProductionUpdate_incrementQueued(std::uint8_t* self, void*) noexcept {
    const std::uint32_t old = loadU32(self + 0x3C);
    storeU32(self + 0x3C, old + 1);
    return old;
}

std::uint32_t __fastcall ProductionUpdate_objectFromId(std::uint8_t* self, void*) noexcept {
    const std::uint32_t key = loadU32(self + 0xEC);
    if (key == 0) {
        return 0;
    }
    return lookupKey(key);
}

std::uint32_t __fastcall ProductionUpdate_clearLink(std::uint8_t* self, void*, std::uint32_t arg) noexcept {
    return g_calls.d6e90b0(self - 0x24, nullptr, arg, 0);
}

std::uint32_t __fastcall ProductionUpdate_bindProducer(std::uint8_t* self, void*) noexcept {
    vcallN(self, 0x4C);
    auto* owner = asPtr(loadU32(self - 0x1C));
    const std::uint32_t produced = g_calls.d6c0e50(owner, nullptr, 0);
    if (produced == 0) {
        return 0;
    }
    const std::uint32_t key = loadU32(self + 0xEC);
    if (key != 0) {
        const std::uint32_t found = lookupKey(key);
        if (found != 0) {
            vcallW(asPtr(produced), 0x08, found, 0);
        }
    }
    return vcallN(asPtr(produced), 0x30);
}

std::uint32_t __fastcall BehaviorModule_objectOrigin(std::uint8_t* self, void*) noexcept {
    const std::uint32_t key = loadU32(self + 4);
    if (key == 0) {
        return 0;
    }
    const std::uint32_t found = lookupKey(key);
    if (found == 0) {
        return 0;
    }
    return found + 0x38;
}

std::uint32_t __fastcall SpecialAbilityUpdate_isReady(std::uint8_t* self, void*) noexcept {
    auto* record = asPtr(loadU32(self + 4));
    const std::uint32_t field = loadU32(record + 0x10);
    if (comissJbe(kReadyThreshold, field)) {
        return withLow(asBits(record), 1);
    }
    const std::uint32_t resume = g_calls.d6ead80(self, nullptr);
    if ((resume & 0xFFu) != 0) {
        return withLow(resume, 1);
    }
    return resume;
}

std::uint32_t __fastcall StealUnitUpdate_syncTarget(std::uint8_t* self, void*, std::uint32_t object) noexcept {
    auto* target = asPtr(object);
    g_calls.d6e6830(self, nullptr, object);
    const std::uint32_t saved = loadU32(asPtr(loadU32(target + 0x418)) + 0x20);
    std::uint32_t box = 0;
    if (loadU32(target + 0x310) != 0 && loadU32(target + 0x3C0) != 0) {
        box = loadU32(asPtr(loadU32(target + 0x3B4)) + 0x14);
    }
    auto* boxObject = asPtr(box);
    const std::uint32_t current = loadU32(self + 0x54);
    const std::uint32_t observed = vcallN(boxObject, 0xB4);
    if (current == observed) {
        return vcallU(self, 0x5C, 0);
    }
    if (saved != current) {
        const std::uint32_t mine = loadU32(asPtr(loadU32(asPtr(loadU32(self + 8)) + 0x418)) + 0x20);
        vcallU(boxObject, 0xB0, mine);
        self[0x50] = 1;
        const std::uint32_t slot = loadU32(target + 0x418);
        storeU32(self + 0x54, loadU32(asPtr(slot) + 0x20));
        return slot;
    }
    return observed;
}

std::uint32_t __fastcall StoreCurrentStateUpdate_xfer(std::uint8_t* self, void*, std::uint32_t xfer) noexcept {
    vcallP(asPtr(xfer), 0x18, 0x0101u);
    vcallP(asPtr(xfer), 0x18, 0x0101u);
    g_calls.d8d82f0(self, nullptr, xfer);
    g_calls.d685080(self + 0x10, nullptr, xfer);
    vcallS(asPtr(xfer), 0x6C, asBits(self + 0x28));
    std::uint32_t result = vcallS(asPtr(xfer), 0x70, asBits(self + 0x2C));
    const std::uint32_t count = loadU32(self + 0x28);
    const std::uint32_t base = loadU32(self + 0x24);
    for (std::uint32_t index = 0; index < count; ++index) {
        result = g_calls.d6e2470(asPtr(base + index * 0x74u), nullptr, xfer);
    }
    return result;
}

std::uint32_t __fastcall EncloseBehavior_offerToContainer(std::uint8_t* self, void*, std::uint32_t container) noexcept {
    auto* body = asPtr(loadU32(self - 0x0C));
    if (loadU32(body + 0x0C) == 0 || self[4] != 0) {
        return 0;
    }
    const std::uint32_t point = asBits(body + 0x14);
    const std::uint32_t first = body[0x1D] != 0 ? g_calls.d6d3a00(asPtr(container), nullptr, point)
                                                : g_calls.d6d3870(asPtr(container), nullptr, point);
    if ((first & 0xFFu) != 0) {
        return 0;
    }
    const std::uint32_t list = asBits(body + 0x0C);
    if (body[0x1C] != static_cast<std::uint8_t>(first)) {
        return g_calls.d6d3a00(asPtr(container), nullptr, list);
    }
    return g_calls.d6d3870(asPtr(container), nullptr, list);
}

std::uint32_t __fastcall WeaponInitState_enter(std::uint8_t* self, void*) noexcept {
    auto* weapon = asPtr(loadU32(self + 0x20));
    auto* record = asPtr(loadU32(weapon + 4));
    auto* inner = asPtr(loadU32(record + 4));
    if (inner[0x13F] != 0) {
        const std::uint32_t delay = loadU32(asPtr(loadU32(record + 4)) + 0x70);
        const std::uint32_t previous = loadU32(weapon + 0x1C);
        std::uint32_t next = delay;
        if (delay == 0) {
            next = 0x7FFFFFFFu;
        }
        storeU32(weapon + 0x1C, next);
        if (next > previous) {
            g_calls.d6a8400(weapon, nullptr);
            g_calls.d6bcf30(self, nullptr, 7);
            return 0xFFFFFFFFu;
        }
    } else {
        auto* extra = asPtr(loadU32(weapon + 0x64));
        if (extra != nullptr) {
            g_calls.d6d0330(extra, nullptr);
        }
    }
    g_calls.d6bcf30(self, nullptr, 7);
    return 0xFFFFFFFFu;
}

std::uint32_t __fastcall WeaponPreFireState_clearFlag(std::uint8_t* self, void*, std::uint32_t) noexcept {
    auto* owner = asPtr(loadU32(self + 0x18));
    storeU32(self + 0x28, 0);
    auto* object = asPtr(loadU32(owner + 0x24));
    const std::uint32_t old = loadU32(object + 0x80);
    storeU32(object + 0x80, old & 0xFFFFBFFFu);
    const std::uint32_t shifted = old >> 0x0E;
    if ((shifted & 1u) != 0) {
        return g_calls.d6d1ed0(object, nullptr, 0x0E, 0);
    }
    return shifted & 0xFFFFFF00u;
}

std::uint32_t __fastcall WeaponFireState_advance(std::uint8_t* self, void*) noexcept {
    const std::uint32_t sum = loadU32(self + 0x28) + loadU32(self + 0x24);
    const std::uint32_t frame = loadU32(g_clock + 0x58);
#if defined(RA3_OBJ6E_ORACLE_MUTANT)
    const bool waiting = sum >= frame;
#else
    const bool waiting = sum > frame;
#endif
    if (waiting) {
        return 0;
    }
    auto* weapon = asPtr(loadU32(self + 0x20));
    std::uint32_t next = 0;
    if (loadU32(weapon + 0x1C) == 0) {
        next = 4;
    } else if (static_cast<std::int32_t>(loadU32(weapon + 0x24)) <= 0) {
        weapon[0x60] = 0;
        next = 0;
    } else if (weapon[0x60] != 0) {
        next = 3;
    } else {
        const std::uint32_t shot = g_calls.d6a88c0(weapon, nullptr, 5);
        next = (shot & 0xFFu) != 0 ? 5u : 0u;
    }
    storeU32(self + 8, next);
    auto* record = asPtr(loadU32(asPtr(loadU32(weapon + 4)) + 4));
    if (((loadU32(record + 0xAC) >> 9) & 1u) != 0) {
        auto* object = asPtr(loadU32(asPtr(loadU32(self + 0x18)) + 0x24));
        g_calls.d4d3b50(object, nullptr, 0x4D);
    }
    return 0xFFFFFFFFu;
}

std::uint32_t __fastcall GateProxyBehavior_call14(std::uint8_t* self, void*) noexcept {
    return gateForward(self, 0x14, false);
}

std::uint32_t __fastcall GateProxyBehavior_call18(std::uint8_t* self, void*) noexcept {
    return gateForward(self, 0x18, false);
}

std::uint32_t __fastcall GateProxyBehavior_call1C(std::uint8_t* self, void*) noexcept {
    return gateForward(self, 0x1C, false);
}

std::uint32_t __fastcall GateProxyBehavior_call10(std::uint8_t* self, void*) noexcept {
    return gateForward(self, 0x10, true);
}

std::uint32_t __fastcall GateProxyBehavior_call20(std::uint8_t* self, void*) noexcept {
    return gateForward(self, 0x20, true);
}

std::uint32_t __fastcall UnifiedControlBehavior_onRadius(std::uint8_t* self, void*, std::uint32_t object) noexcept {
    const std::uint32_t old = loadU32(self + 0x14);
    const std::uint32_t probed = g_calls.d6d9560(self, nullptr, object);
    const std::uint32_t now = loadU32(self + 0x14);
    if (!feq(now, old) && feq(old, kOne)) {
        return vcallW(self, 0x58, object, 0);
    }
    if (!feq(now, old) && feq(now, kOne)) {
        return vcallW(self, 0x58, object, 1);
    }
    return probed;
}

std::uint32_t __fastcall HordeContain_passengerSlotB0(std::uint8_t* self, void*) noexcept {
    auto* owner = asPtr(loadU32(self - 0x1C));
    const std::uint32_t aiBits = g_calls.d6efe40(owner, nullptr);
    if (aiBits == 0) {
        return 0;
    }
    auto* passenger = firstOf(asPtr(aiBits));
    if (passenger == nullptr) {
        return 0;
    }
    return vcallN(passenger, 0xB0);
}

std::uint32_t __fastcall HordeContain_passengerSlot28(std::uint8_t* self, void*) noexcept {
    auto* owner = asPtr(loadU32(self - 0x1C));
    const std::uint32_t aiBits = g_calls.d6efe40(owner, nullptr);
    if (aiBits == 0) {
        return 1;
    }
    auto* passenger = firstOf(asPtr(aiBits));
    if (passenger == nullptr) {
        return withLow(aiBits, 1);
    }
    return vcallN(passenger, 0x28);
}

std::uint32_t __fastcall HordeContain_knowsObject(std::uint8_t* self, void*, std::uint32_t object) noexcept {
    const std::uint32_t expected = loadU32(self - 0x164);
    const std::uint32_t ai = g_calls.d6efe40(asPtr(object), nullptr);
    if (ai == expected) {
        return withLow(ai, 1);
    }
    const std::uint32_t cell = g_calls.daafe10(self + 0x48, nullptr);
    const std::uint32_t value = loadU32(asPtr(cell));
    const std::uint32_t matched = value == asBits(self + 0x4C) ? 0u : 1u;
    return (value & 0xFFFFFF00u) | matched;
}

std::uint32_t __fastcall HordeContain_testTriple(std::uint8_t*, void*, std::uint32_t arg0, std::uint32_t arg1,
                                                 std::uint32_t arg2) noexcept {
    if (arg0 == 0 || arg1 == 0 || arg2 == 0) {
        return 0;
    }
    const std::uint32_t result = g_calls.d6f2140(asPtr(arg1), nullptr, arg0, arg2, 0, 1);
    return (result & 0xFFu) != 0 ? withLow(result, 1) : (result & 0xFFFFFF00u);
}

std::uint32_t __fastcall HordeContain_passengerSlot70(std::uint8_t* self, void*, std::uint32_t, std::uint32_t,
                                                      std::uint32_t) noexcept {
    auto* owner = asPtr(loadU32(self - 0x1C));
    if (owner == nullptr || loadU32(owner + 0x3EC) == 0) {
        return 0;
    }
    const std::uint32_t aiBits = g_calls.d6efe40(owner, nullptr);
    if (aiBits == 0) {
        return 0;
    }
    auto* passenger = firstOf(asPtr(aiBits));
    if (passenger == nullptr) {
        return aiBits;
    }
    return vcallN(passenger, 0x70);
}

std::uint32_t __fastcall HealContain_noteFrame(std::uint8_t* self, void*, std::uint32_t arg) noexcept {
    const std::uint32_t result = g_calls.d6fa190(self, nullptr, arg);
    storeU32(self + 0x750, loadU32(g_clock + 0x58));
    return result;
}

std::uint32_t __fastcall OpenContain_passengerFlag(std::uint8_t* self, void*) noexcept {
    const std::uint32_t wordAddress = vcallW(self, 0xAC, 0, 0);
    const std::uint32_t flags = loadU32(asPtr(wordAddress));
    if (((flags >> 1) & 1u) == 0) {
        return flags >> 1 & 0xFFFFFF00u;
    }
    auto* owner = asPtr(loadU32(self - 0x1C));
    const std::uint32_t first = g_calls.d6efe40(owner, nullptr);
    if (first == 0) {
        return 1;
    }
    const std::uint32_t second = g_calls.d6efe40(owner, nullptr);
    auto* passenger = firstOf(asPtr(second));
    if (passenger == nullptr) {
        return 0;
    }
    return vcallN(passenger, 0xB0);
}

std::uint32_t __fastcall ActivateModuleSpecialAbilityUpdate_enable(std::uint8_t* self, void*) noexcept {
    if (loadU32(self + 8) == 0) {
        return 0;
    }
    return g_calls.d6e0450(self, nullptr, 1);
}

std::uint32_t __fastcall GiveUpgradeUpdate_runGrant(std::uint8_t* self, void*) noexcept {
    g_calls.d6e1630(self, nullptr);
    const std::uint32_t result = g_calls.d6eaef0(self, nullptr);
    return result & 0xFFFFFF00u;
}

std::uint32_t __fastcall ToggleMountedSpecialAbilityUpdate_mountedReady(std::uint8_t* self, void*) noexcept {
    if (loadU32(self + 0x2C) == 2) {
        return 0;
    }
    auto* record = asPtr(loadU32(self + 4));
    const std::uint32_t field = loadU32(record + 0x10);
    if (comissJbe(kReadyThreshold, field)) {
        return withLow(asBits(record), 1);
    }
    const std::uint32_t resume = g_calls.d6ead80(self, nullptr);
    if ((resume & 0xFFu) == 0) {
        return resume & 0xFFFFFF00u;
    }
    return withLow(resume, 1);
}

std::uint32_t __fastcall SupplyTruckAIUpdate_popPending(std::uint8_t* self, void*) noexcept {
    const std::uint32_t begin = loadU32(self + 0x30);
    const std::uint32_t end = loadU32(self + 0x34);
    if (((end - begin) & 0xFFFFFFFCu) == 0) {
        return 0;
    }
    const std::uint32_t popped = loadU32(asPtr(end - 4));
    storeU32(self + 0x34, end - 4);
    auto* link = asPtr(loadU32(self - 0x514));
    const std::uint32_t dock = loadU32(link + 0xF4);
    if (dock != 0) {
        const std::uint32_t count = (loadU32(self + 0x34) - loadU32(self + 0x30)) >> 2;
        auto* player = asPtr(loadU32(self - 0x518));
        g_calls.d52dde0(asPtr(dock), nullptr, loadU32(player + 0x4C), count);
    }
    return popped;
}

std::uint32_t __fastcall AIUpdateInterface_allowsMove(std::uint8_t* self, void*, std::uint32_t query) noexcept {
    auto* object = asPtr(loadU32(self + 8));
    if ((object[0x471] & 1u) != 0) {
        return 0;
    }
    if (loadU32(self + 0x250) == 0xFFFFFFFEu) {
        return 0;
    }
    auto* args = asPtr(query);
    const std::uint32_t code = loadU32(args + 4);
    if (code == 0) {
        if (self[0x43E] != 0) {
            return 0;
        }
        if (((loadU32(object + 0xCC) >> 0x0A) & 1u) != 0) {
            return 0;
        }
        if (((loadU32(object + 0x90) >> 4) & 1u) != 0) {
            return 0;
        }
    }
    if (self[0x43F] != 0 && code == 1) {
        return 0;
    }
    auto* machine = asPtr(loadU32(self + 0x3C));
    if (loadU32(machine + 0x88) != 0 && loadU32(machine + 0x8C) == 0xFFFFFFFFu) {
        return 0;
    }
    if (code == 0) {
        g_calls.d6ff750(self, nullptr);
        float value = 0.f;
        std::memcpy(&value, &g_st0, 4);
        if (value == 0.f) {
            const std::uint32_t kind = loadU32(args);
            if (kind == 0 || kind == 1) {
                return 0;
            }
        }
    }
    auto* templ = asPtr(loadU32(object + 4));
    const std::uint32_t flags = loadU32(templ + 0xC0);
    if (((flags >> 0x0E) & 1u) == 0 || ((flags >> 1) & 1u) == 0) {
        return 1;
    }
    if (g_clock[0xAC] != 0 || loadU32(args) == 0x0A) {
        return 1;
    }
    return 0;
}

std::uint32_t __fastcall AIUpdateInterface_locomotorReady(std::uint8_t* self, void*) noexcept {
    auto* object = asPtr(loadU32(self + 8));
    if ((object[0x471] & 1u) != 0) {
        return withLow(asBits(object), 1);
    }
    auto* machine = asPtr(loadU32(self + 0x3C));
    if (loadU32(machine + 4) == 0) {
        return withLow(asBits(machine), 1);
    }
    auto* loco = asPtr(loadU32(machine + 4));
    return vcallN(loco, 0x20);
}

std::uint32_t __fastcall AIUpdateInterface_orderState33(std::uint8_t* self, void*, std::uint32_t object,
                                                        std::uint32_t value) noexcept {
    std::uint32_t target = object;
    if (target == 0) {
        target = g_calls.d6efe40(asPtr(loadU32(self + 8)), nullptr);
        if (target == 0) {
            return 0;
        }
    }
    auto* machine = asPtr(loadU32(self + 0x3C));
    vcallN(machine, 0x34);
    vcallP(machine, 0x38, target);
    storeU32(self + 0x50, value);
    vcallU(machine, 0x1C, 0x33);
    storeU32(self + 0x308, 0xFFFFFFFFu);
    storeU32(self + 0x368, 0xFFFFFFFFu);
    storeU32(self + 0x3C8, 0xFFFFFFFFu);
    return 0xFFFFFFFFu;
}

std::uint32_t __fastcall SpawnBehavior_anyLiving(std::uint8_t* self, void*) noexcept {
    const std::uint32_t head = asBits(self + 0x44);
    std::uint32_t node = loadU32(self + 0x44);
    while (node != head) {
        auto* link = asPtr(node);
        const std::uint32_t key = loadU32(link + 8);
        if (key != 0) {
            const std::uint32_t found = lookupKey(key);
            if (found != 0) {
                const std::uint32_t alive = g_calls.d6f1350(asPtr(found), nullptr);
                if ((alive & 0xFFu) != 0) {
                    return 1;
                }
            }
        }
        node = loadU32(link);
    }
    return 0;
}

}  // namespace ra3::gamelogic
