#include "ra3/gamelogic/ObjectModules6C.h"

#include <cstring>

namespace ra3::gamelogic {
namespace {

Object6CCalls g{};
Object6CEnv e{};

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

std::uint8_t* objOf(std::uint8_t* self) noexcept {
    return *reinterpret_cast<std::uint8_t**>(self + 8);
}

std::uint8_t* locoOf(std::uint8_t* self) noexcept {
    return *reinterpret_cast<std::uint8_t**>(self + 0x3C);
}

std::uint32_t cell(std::uint32_t* slot) noexcept {
    return slot != nullptr ? *slot : 0;
}

using Fn0 = std::uint32_t(__fastcall*)(void*, void*);
using Fn1 = std::uint32_t(__fastcall*)(void*, void*, std::uint32_t);
using Fn2 = std::uint32_t(__fastcall*)(void*, void*, std::uint32_t, std::uint32_t);

std::uint32_t call0(std::uint8_t* obj, int slot) noexcept {
    auto* vt = *reinterpret_cast<std::uint8_t**>(obj);
    auto fn = *reinterpret_cast<Fn0*>(vt + slot);
    return fn(obj, nullptr);
}

std::uint32_t call1(std::uint8_t* obj, int slot, std::uint32_t arg) noexcept {
    auto* vt = *reinterpret_cast<std::uint8_t**>(obj);
    auto fn = *reinterpret_cast<Fn1*>(vt + slot);
    return fn(obj, nullptr, arg);
}

std::uint32_t call2(std::uint8_t* obj, int slot, std::uint32_t first, std::uint32_t second) noexcept {
    auto* vt = *reinterpret_cast<std::uint8_t**>(obj);
    auto fn = *reinterpret_cast<Fn2*>(vt + slot);
    return fn(obj, nullptr, first, second);
}

void copy3(std::uint8_t* dst, const std::uint8_t* src) noexcept {
    std::memcpy(dst, src, 12);
}

void copyObjectPosition(std::uint8_t* self, std::uint8_t* object) noexcept {
    auto* dest = *reinterpret_cast<std::uint8_t**>(self + 0x214);
    if (dest == nullptr || object == nullptr) {
        return;
    }
    copy3(dest + 0x0C, object + 0x38);
}

bool blocked(std::uint8_t* object) noexcept {
    auto* templ = *reinterpret_cast<std::uint8_t**>(object + 4);
    const std::uint32_t flags = loadU32(templ + 0xC0);
    return ((flags >> 0x1A) & 1u) != 0;
}

std::uint32_t templBits(std::uint8_t* object) noexcept {
    return asBits(*reinterpret_cast<std::uint8_t**>(object + 4));
}

void chooseStanceSlot(std::uint8_t* self, std::uint32_t value) noexcept {
    if (loadU32(self + 0x5C) == 4) {
        storeU32(self + 0x5C, value);
    } else {
        storeU32(self + 0x58, value);
    }
}

std::uint32_t bitSlot(std::uint32_t index) noexcept {
    return ((index >> 5) * 4u) + 0x100u;
}

std::uint32_t chainOf(std::uint8_t* object) noexcept {
    const std::uint32_t direct = loadU32(object + 0x328);
    if (direct != 0) {
        return direct;
    }
    const std::uint32_t begin = loadU32(object + 0x32C);
    const std::uint32_t end = loadU32(object + 0x330);
    if (begin == end) {
        return 0;
    }
    return loadU32(asPtr(begin));
}

float bitsToFloat(std::uint32_t bits) noexcept {
    float value = 0;
    std::memcpy(&value, &bits, 4);
    return value;
}

bool above(std::uint32_t lhs, std::uint32_t rhs) noexcept {
    return bitsToFloat(lhs) > bitsToFloat(rhs);
}

std::uint32_t locoAfter(std::uint8_t* self, std::uint32_t target, std::uint32_t mode,
                        std::uint32_t state) noexcept {
    auto* object = objOf(self);
    const std::uint32_t usable = g.usable(object, nullptr);
    if (static_cast<std::uint8_t>(usable) == 0) {
        return usable;
    }
    auto* loco = locoOf(self);
    call0(loco, 0x34);
    call1(loco, 0x38, target);
    storeU32(self + 0x50, mode);
    return call1(loco, 0x1C, state);
}

std::uint32_t locoBefore(std::uint8_t* self, std::uint32_t target, std::uint32_t mode,
                         std::uint32_t state) noexcept {
    auto* object = objOf(self);
    const std::uint32_t usable = g.usable(object, nullptr);
    if (static_cast<std::uint8_t>(usable) == 0) {
        return usable;
    }
    auto* loco = locoOf(self);
    call0(loco, 0x34);
    storeU32(self + 0x50, mode);
    call1(loco, 0x38, target);
    return call1(loco, 0x1C, state);
}

std::uint32_t aimState(std::uint8_t* self, std::uint32_t target, std::uint32_t mode,
                       std::uint32_t state) noexcept {
    auto* object = objOf(self);
    const std::uint32_t usable = g.usable(object, nullptr);
    if (static_cast<std::uint8_t>(usable) == 0) {
        return usable;
    }
    auto* loco = locoOf(self);
    call0(loco, 0x34);
    g.aimAt(loco, nullptr, target);
    storeU32(self + 0x50, mode);
    return call1(loco, 0x1C, state);
}

std::uint32_t tailState(std::uint8_t* self, std::uint32_t mode, std::uint32_t state) noexcept {
    auto* object = objOf(self);
    const std::uint32_t usable = g.usable(object, nullptr);
    if (static_cast<std::uint8_t>(usable) == 0) {
        return usable;
    }
    auto* loco = locoOf(self);
    call0(loco, 0x34);
    storeU32(self + 0x50, mode);
    return call1(loco, 0x1C, state);
}

std::uint32_t tryPair(std::uint8_t* self, std::uint32_t first, std::uint32_t second, bool secondKind) noexcept {
    auto* object = objOf(self);
    const std::uint32_t registry = cell(e.registryCell);
    const std::uint32_t ok = secondKind
        ? g.pairB(asPtr(registry), nullptr, asBits(object), first, second)
        : g.pairA(asPtr(registry), nullptr, asBits(object), first, second);
    if (static_cast<std::uint8_t>(ok) == 0) {
        return ok;
    }
    if (secondKind) {
        return g.apply5A(self + 0x24, nullptr, first, second);
    }
    return g.apply59(self + 0x24, nullptr, first, second);
}

void unlinkNode(std::uint8_t* node) noexcept {
    auto* prev = *reinterpret_cast<std::uint8_t**>(node + 4);
    auto* next = *reinterpret_cast<std::uint8_t**>(node);
    *reinterpret_cast<std::uint8_t**>(prev) = next;
    *reinterpret_cast<std::uint8_t**>(next + 4) = prev;
}

// fild; fmul; fistp toward zero. Matches the control-word sequence at 0x006C7331.
std::uint32_t truncMul(std::int32_t value, std::uint32_t scaleBits) noexcept {
    unsigned short saved = 0;
    unsigned short chopped = 0;
    unsigned __int64 wide = 0;
    __asm {
        fild dword ptr value
        fnstcw word ptr saved
        fmul dword ptr scaleBits
        movzx eax, word ptr saved
        or eax, 0C00h
        mov word ptr chopped, ax
        fldcw word ptr chopped
        fistp qword ptr wide
        fldcw word ptr saved
    }
    return static_cast<std::uint32_t>(wide);
}

bool spawnSuppressed(std::uint8_t* object) noexcept {
    auto* templ = *reinterpret_cast<std::uint8_t**>(object + 4);
    if (((loadU32(templ + 0xC8) >> 0x19) & 1u) != 0) {
        return true;
    }
    if (loadU32(object + 0x418) == 0) {
        return true;
    }
    return object[0x4D7] == 0;
}

}  // namespace

void setObject6CCalls(const Object6CCalls& calls) noexcept { g = calls; }
void setObject6CEnv(const Object6CEnv& env) noexcept { e = env; }

std::uint32_t AIUpdateInterface_followPosition(std::uint8_t* self, std::uint32_t pos,
                                               std::uint32_t mode) noexcept {
    auto* object = objOf(self);
    const std::uint32_t usable = g.usable(object, nullptr);
    if (static_cast<std::uint8_t>(usable) == 0) {
        return usable;
    }
    copyObjectPosition(self, object);
    auto* loco = locoOf(self);
    call0(loco, 0x34);
    g.steer(self, nullptr, pos, kLimitBits, mode);
    storeU32(self + 0x190, 0);
    self[0x432] = 0;
    storeU32(self + 0x50, mode);
    return call1(locoOf(self), 0x1C, 0x3D);
}

std::uint32_t AIUpdateInterface_followObject(std::uint8_t* self, std::uint32_t target,
                                             std::uint32_t mode) noexcept {
    if (self[0x436] != 0) {
        return 0;
    }
    auto* object = objOf(self);
    const std::uint32_t usable = g.usable(object, nullptr);
    if (static_cast<std::uint8_t>(usable) == 0) {
        return usable;
    }
    copyObjectPosition(self, object);
    auto* loco = locoOf(self);
    call0(loco, 0x34);
    call1(loco, 0x38, target);
    storeU32(self + 0x50, mode);
    storeU32(self + 0x190, 0);
    self[0x432] = 0;
    return call1(locoOf(self), 0x1C, 0x3D);
}

std::uint32_t AIUpdateInterface_steerState1B(std::uint8_t* self, std::uint32_t pos,
                                             std::uint32_t mode) noexcept {
    auto* object = objOf(self);
    const std::uint32_t usable = g.usable(object, nullptr);
    if (static_cast<std::uint8_t>(usable) == 0) {
        return usable;
    }
    auto* loco = locoOf(self);
    call0(loco, 0x34);
    call1(loco, 0x38, 0);
    g.steer(self, nullptr, pos, kLimitBits, mode);
    storeU32(self + 0x50, mode);
    return call1(locoOf(self), 0x1C, 0x1B);
}

std::uint32_t AIUpdateInterface_tryCommand59(std::uint8_t* self, std::uint32_t first,
                                             std::uint32_t second) noexcept {
    return tryPair(self, first, second, false);
}

std::uint32_t AIUpdateInterface_tryCommand5A(std::uint8_t* self, std::uint32_t first,
                                             std::uint32_t second) noexcept {
    return tryPair(self, first, second, true);
}

std::uint32_t AIUpdateInterface_weaponOrFollow(std::uint8_t* self, std::uint32_t target,
                                               std::uint32_t mode) noexcept {
    auto* object = objOf(self);
    const std::uint32_t chainBits = chainOf(object);
    if (chainBits != 0) {
        auto* chain = asPtr(chainBits);
        const std::uint32_t child = call0(chain, 0x84);
        if (child != 0) {
            return call2(asPtr(child), 0x80, target, mode);
        }
    }
    const std::uint32_t usable = g.usable(object, nullptr);
    if (static_cast<std::uint8_t>(usable) == 0) {
        return usable;
    }
    std::uint32_t record[2] = {kRecordTag, target};
    const std::uint32_t registry = cell(e.registryCell);
    const std::uint32_t ok = g.dispatch5(asPtr(registry), nullptr, asBits(object), record, mode, 1, 0);
    if (static_cast<std::uint8_t>(ok) == 0) {
        return ok;
    }
    auto* loco = locoOf(self);
    call0(loco, 0x34);
    call1(loco, 0x38, target);
    storeU32(self + 0x50, mode);
    return call1(locoOf(self), 0x1C, 0x0F);
}

std::uint32_t AIUpdateInterface_locoState0E(std::uint8_t* self, std::uint32_t target,
                                            std::uint32_t mode) noexcept {
    return locoAfter(self, target, mode, 0x0E);
}

std::uint32_t AIUpdateInterface_placeState2D(std::uint8_t* self, std::uint32_t pos,
                                             std::uint32_t mode) noexcept {
    auto* object = objOf(self);
    const std::uint32_t usable = g.usable(object, nullptr);
    if (static_cast<std::uint8_t>(usable) == 0) {
        return usable;
    }
    auto* loco = locoOf(self);
    call0(loco, 0x34);
    g.place(loco, nullptr, asPtr(pos), kLimitBits);
    storeU32(self + 0x50, mode);
    return call1(locoOf(self), 0x1C, 0x2D);
}

std::uint32_t AIUpdateInterface_locoState2D(std::uint8_t* self, std::uint32_t target,
                                            std::uint32_t mode) noexcept {
    return locoAfter(self, target, mode, 0x2D);
}

std::uint32_t AIUpdateInterface_locoState69(std::uint8_t* self, std::uint32_t target,
                                            std::uint32_t mode) noexcept {
    return locoAfter(self, target, mode, 0x69);
}

std::uint32_t AIUpdateInterface_notifyPassenger(std::uint8_t* self, std::uint32_t flag,
                                                std::uint32_t code) noexcept {
    auto* object = objOf(self);
    const std::uint32_t chainBits = chainOf(object);
    if (chainBits == 0) {
        return asBits(object);
    }
    auto* chain = asPtr(chainBits);
    if (flag != 0) {
        call0(chain, 0x90);
    }
    auto* passenger = *reinterpret_cast<std::uint8_t**>(object + 0x374);
    if (passenger != nullptr) {
        g.pushState(passenger + 0x24, nullptr, code);
    }
    return call1(chain, 0x88, code);
}

std::uint32_t AIUpdateInterface_aimState11(std::uint8_t* self, std::uint32_t target,
                                           std::uint32_t mode) noexcept {
    return aimState(self, target, mode, 0x11);
}

std::uint32_t AIUpdateInterface_tailState29(std::uint8_t* self, std::uint32_t mode) noexcept {
    return tailState(self, mode, 0x29);
}

std::uint32_t AIUpdateInterface_tailState48(std::uint8_t* self, std::uint32_t mode) noexcept {
    return tailState(self, mode, 0x48);
}

std::uint32_t AIUpdateInterface_aimState12(std::uint8_t* self, std::uint32_t target,
                                           std::uint32_t mode) noexcept {
    return aimState(self, target, mode, 0x12);
}

std::uint32_t AIUpdateInterface_locoState13(std::uint8_t* self, std::uint32_t target,
                                            std::uint32_t mode) noexcept {
    return locoBefore(self, target, mode, 0x13);
}

std::uint32_t AIUpdateInterface_locoState14(std::uint8_t* self, std::uint32_t target,
                                            std::uint32_t mode) noexcept {
    return locoBefore(self, target, mode, 0x14);
}

std::uint32_t AIUpdateInterface_guardRect(std::uint8_t* self, std::uint32_t point,
                                          std::uint32_t aux, std::uint32_t mode) noexcept {
    auto* object = objOf(self);
    const std::uint32_t usable = g.usable(object, nullptr);
    if (static_cast<std::uint8_t>(usable) == 0) {
        return usable;
    }
    if (blocked(object)) {
        return templBits(object);
    }
    chooseStanceSlot(self, 0);
    // Frame offsets match the 0x006CC970 stack: vec at +8, probe out at +0x20.
    std::uint8_t frame[0x40] = {};
    storeU32(frame + 8, loadU32(asPtr(point)));
    storeU32(frame + 0x0C, loadU32(asPtr(point + 4)));
    storeU32(frame + 0x10, loadU32(asPtr(point + 8)));
    if (mode == 0) {
        auto* world = asPtr(cell(e.worldCell));
        call1(world, 0x28, asBits(frame + 0x20));
        const bool inside = above(loadU32(frame + 8), loadU32(frame + 0x20)) &&
                            above(loadU32(frame + 0x2C), loadU32(frame + 8)) &&
                            above(loadU32(frame + 0x0C), loadU32(frame + 0x24)) &&
                            above(loadU32(frame + 0x30), loadU32(frame + 0x0C));
        if (!inside) {
            const std::uint32_t replaced = call2(world, 0x3C, asBits(frame + 0x14), asBits(frame + 8));
            storeU32(frame + 8, loadU32(asPtr(replaced)));
            storeU32(frame + 0x0C, loadU32(asPtr(replaced + 4)));
            storeU32(frame + 0x10, loadU32(asPtr(replaced + 8)));
        }
    }
    storeU32(self + 0x60, loadU32(frame + 8));
    storeU32(self + 0x54, aux);
    storeU32(self + 0x64, loadU32(frame + 0x0C));
    storeU32(self + 0x68, loadU32(frame + 0x10));
    call0(locoOf(self), 0x34);
    storeU32(self + 0x50, mode);
    return call1(locoOf(self), 0x1C, 0x10);
}

std::uint32_t AIUpdateInterface_state18(std::uint8_t* self, std::uint32_t aux,
                                        std::uint32_t mode) noexcept {
    auto* object = objOf(self);
    const std::uint32_t usable = g.usable(object, nullptr);
    if (static_cast<std::uint8_t>(usable) == 0) {
        return usable;
    }
    if (blocked(object)) {
        return templBits(object);
    }
    storeU32(self + 0x54, aux);
    auto* loco = locoOf(self);
    call0(loco, 0x34);
    storeU32(self + 0x50, mode);
    return call1(locoOf(self), 0x1C, 0x18);
}

std::uint32_t AIUpdateInterface_followActor50(std::uint8_t* self, std::uint32_t actor,
                                              std::uint32_t aux, std::uint32_t mode) noexcept {
    auto* object = objOf(self);
    const std::uint32_t usable = g.usable(object, nullptr);
    if (static_cast<std::uint8_t>(usable) == 0) {
        return usable;
    }
    if (blocked(object)) {
        return 1;
    }
    chooseStanceSlot(self, 1);
    storeU32(self + 0x54, aux);
    storeU32(self + 0x6C, loadU32(asPtr(actor + 0xDC)));
    auto* loco = locoOf(self);
    call0(loco, 0x34);
    storeU32(self + 0x50, mode);
    return call1(locoOf(self), 0x1C, 0x50);
}

std::uint32_t AIUpdateInterface_followPoint50(std::uint8_t* self, std::uint32_t point,
                                              std::uint32_t aux, std::uint32_t mode) noexcept {
    auto* object = objOf(self);
    const std::uint32_t usable = g.usable(object, nullptr);
    if (static_cast<std::uint8_t>(usable) == 0) {
        return usable;
    }
    if (blocked(object)) {
        return templBits(object);
    }
    chooseStanceSlot(self, 2);
    storeU32(self + 0x54, aux);
    storeU32(self + 0x6C, 0);
    storeU32(self + 0x70, loadU32(asPtr(point + 8)));
    auto* loco = locoOf(self);
    call0(loco, 0x34);
    storeU32(self + 0x50, mode);
    return call1(locoOf(self), 0x1C, 0x50);
}

std::uint32_t AIUpdateInterface_guardPoint4F(std::uint8_t* self, std::uint32_t id, std::uint32_t aux,
                                             std::uint32_t mode, std::uint32_t point) noexcept {
    auto* object = objOf(self);
    const std::uint32_t usable = g.usable(object, nullptr);
    if (static_cast<std::uint8_t>(usable) == 0) {
        return usable;
    }
    if (blocked(object)) {
        return templBits(object);
    }
    chooseStanceSlot(self, 3);
    storeU32(self + 0x74, id);
    storeU32(self + 0x54, aux);
    auto* extra = *reinterpret_cast<std::uint8_t**>(self + 0x47C);
    if (extra != nullptr) {
        storeU32(extra + 0xA8, id);
    }
    std::uint32_t local[3] = {};
    if (point == 0) {
        g.readPoint(asPtr(id), nullptr, local);
    } else {
        local[0] = loadU32(asPtr(point));
        local[1] = loadU32(asPtr(point + 4));
        local[2] = loadU32(asPtr(point + 8));
    }
    storeU32(self + 0x60, local[0]);
    storeU32(self + 0x64, local[1]);
    storeU32(self + 0x68, local[2]);
    storeU32(self + 0x6C, 0);
    auto* loco = locoOf(self);
    call0(loco, 0x34);
    g.place(loco, nullptr, local, kLimitBits);
    storeU32(self + 0x50, mode);
    return call1(locoOf(self), 0x1C, 0x4F);
}

std::uint32_t AIUpdateInterface_weaponMove3C(std::uint8_t* self, std::uint32_t target, std::uint32_t pos,
                                             std::uint32_t value, std::uint32_t mode) noexcept {
    if (target == 0) {
        return 0;
    }
    auto* loco = locoOf(self);
    call0(loco, 0x34);
    call1(loco, 0x38, target);
    g.steer(self, nullptr, pos, kLimitBits, mode);
    storeU32(self + 0x50, mode);
    const std::uint32_t state = call1(locoOf(self), 0x1C, 0x3C);
    auto* weapons = *reinterpret_cast<std::uint8_t**>(objOf(self) + 0x37C);
    if (weapons == nullptr) {
        return state;
    }
    std::uint32_t found = g.findWeapon(weapons, nullptr, 0xFFFFFFFFu);
    if (found == 0) {
        found = g.findWeaponAlt(weapons, nullptr, found, 0xFFFFFFFFu);
        if (found == 0) {
            return found;
        }
    }
    storeU32(asPtr(found) + 0x24, value);
    return found;
}

std::uint32_t AIUpdateInterface_matchRoster(std::uint8_t* self, std::uint32_t actor,
                                            std::uint32_t code) noexcept {
    if (actor == 0) {
        return 0;
    }
    auto* object = objOf(self);
    if (blocked(object)) {
        return templBits(object);
    }
    auto* passenger = *reinterpret_cast<std::uint8_t**>(object + 0x374);
    if (passenger == nullptr) {
        return templBits(object);
    }
    const std::uint32_t idSlot = g.readId(object, nullptr, &actor);
    const std::uint32_t list = g.lookup(asPtr(cell(e.catalogCell)), nullptr, loadU32(asPtr(idSlot)));
    if (list == 0) {
        return list;
    }
    auto* cursor = asPtr(list);
    std::uint32_t index = 0;
    for (;;) {
        const std::uint32_t count = call0(cursor, 0);
        if (index == count) {
            return count;
        }
        const std::uint32_t element = call1(cursor, 0x0C, index);
        if (element == actor) {
            auto* templ = *reinterpret_cast<std::uint8_t**>(asPtr(actor) + 4);
            if (loadU32(templ + 4) == 2) {
                g.pushState(passenger + 0x24, nullptr, code);
            }
        }
        index += 1;
    }
}

std::uint32_t AIUpdateInterface_walkRoster(std::uint8_t* self, std::uint32_t actor, std::uint32_t,
                                           std::uint32_t) noexcept {
    if (actor == 0) {
        return 0;
    }
    auto* object = objOf(self);
    if (blocked(object)) {
        return templBits(object);
    }
    if (loadU32(object + 0x374) == 0) {
        return templBits(object);
    }
    const std::uint32_t idSlot = g.readId(object, nullptr, &actor);
    const std::uint32_t list = g.lookup(asPtr(cell(e.catalogCell)), nullptr, loadU32(asPtr(idSlot)));
    if (list == 0) {
        return list;
    }
    auto* cursor = asPtr(list);
    std::uint32_t index = 0;
    for (;;) {
        const std::uint32_t count = call0(cursor, 0);
        if (index == count) {
            return count;
        }
        call1(cursor, 0x0C, index);
        index += 1;
    }
}

std::uint32_t AIUpdateInterface_locoState38(std::uint8_t* self, std::uint32_t target,
                                            std::uint32_t mode) noexcept {
    return locoAfter(self, target, mode, 0x38);
}

std::uint32_t StancesBehavior_latchGoal(std::uint8_t* self) noexcept {
    if (self[0x10F] != 0) {
        return 0;
    }
    const std::uint32_t goal = loadU32(self + 0x40);
    self[0x10F] = 1;
    return goal;
}

std::uint32_t StancesBehavior_setBit(std::uint8_t* self, std::uint32_t index) noexcept {
    const std::uint32_t bit = 1u << (index & 0x1Fu);
    const std::uint32_t slot = bitSlot(index);
    storeU32(self + slot, loadU32(self + slot) | bit);
    self[0x10C] = 0;
    return asBits(self);
}

std::uint32_t StancesBehavior_clearBit(std::uint8_t* self, std::uint32_t index) noexcept {
    const std::uint32_t bit = 1u << (index & 0x1Fu);
    const std::uint32_t slot = bitSlot(index);
    storeU32(self + slot, loadU32(self + slot) & ~bit);
    self[0x10C] = 0;
    return asBits(self);
}

std::uint32_t StancesBehavior_setLinkedFlag(std::uint8_t* self, std::uint32_t value) noexcept {
    storeU32(self + 0x14, value);
    auto* primary = *reinterpret_cast<std::uint8_t**>(self - 8);
    const std::uint32_t old = loadU32(primary + 0x98);
    const std::uint32_t mask = 1u << kStanceFlagShift;
    const std::uint32_t shifted = old >> kStanceFlagShift;
    std::uint32_t eax = shifted & 0xFFFFFF00u;
    std::uint32_t flag = 0;
    if (value == 0) {
        storeU32(primary + 0x98, old & ~mask);
        eax |= shifted & 1u;
    } else {
        storeU32(primary + 0x98, old | mask);
        eax |= static_cast<std::uint8_t>(~shifted) & 1u;
        flag = 1;
    }
    if (static_cast<std::uint8_t>(eax) == 0) {
        return eax;
    }
    return g.notifyBit(primary, nullptr, 0xD4, flag);
}

std::uint32_t StancesBehavior_setLinkedByte(std::uint8_t* self, std::uint32_t value) noexcept {
    storeU32(self + 0x28, value);
    auto* primary = *reinterpret_cast<std::uint8_t**>(self - 8);
    const std::uint32_t eax = (value & 0xFFFFFF00u) | (value != 0 ? 1u : 0u);
    return g.notifyAlt(primary, nullptr, 0xB7, eax);
}

std::uint32_t StancesBehavior_releaseRecord(std::uint8_t* self, std::uint32_t key) noexcept {
    auto* sentinel = self + 0xD4;
    auto* node = *reinterpret_cast<std::uint8_t**>(sentinel);
    if (node != sentinel) {
        while (loadU32(node + 8) != key) {
            node = *reinterpret_cast<std::uint8_t**>(node);
            if (node == sentinel) {
                break;
            }
        }
        if (node != sentinel) {
            node = *reinterpret_cast<std::uint8_t**>(node);
            auto* removed = *reinterpret_cast<std::uint8_t**>(node + 4);
            unlinkNode(removed);
            auto* header = asPtr(g.allocNode(self + 0xDC, nullptr, 0x10));
            storeU32(removed, loadU32(header + 0x10));
            storeU32(header + 0x10, asBits(removed));
        }
    }
    auto* cursor = *reinterpret_cast<std::uint8_t**>(sentinel);
    std::uint32_t count = 0;
    if (cursor != sentinel) {
        do {
            cursor = *reinterpret_cast<std::uint8_t**>(cursor);
            count += 1;
        } while (cursor != sentinel);
    }
    if (count != 0) {
        return asBits(sentinel);
    }
    auto* primary = *reinterpret_cast<std::uint8_t**>(self - 8);
    const std::uint32_t old = loadU32(primary + 0x94);
    storeU32(primary + 0x94, old & 0xFFFFBFFFu);
    const std::uint32_t shifted = old >> 14;
    const std::uint32_t eax = (shifted & 0xFFFFFF00u) | (shifted & 1u);
    if (static_cast<std::uint8_t>(eax) == 0) {
        return eax;
    }
    return g.notifyBit(primary, nullptr, 0xAE, 0);
}

std::uint32_t HighlanderBody_readIfUnset(std::uint8_t* self) noexcept {
    if (self[0x11F] != 0) {
        return 0;
    }
    return loadU32(self + 0x50);
}

std::uint32_t HighlanderBody_applyTemplate(std::uint8_t* self) noexcept {
    const std::uint32_t state = loadU32(self + 0x44);
    auto* templ = *reinterpret_cast<std::uint8_t**>(self + 4);
    auto* host = *reinterpret_cast<std::uint8_t**>(self + 8);
    if (state == 1) {
        const std::uint32_t key = loadU32(templ + 0x30);
        std::uint32_t eax = key;
        if (key != 0) {
            eax = g.applyNeg(host, nullptr, key, 0xFFFFFFFFu);
        }
        const std::uint32_t extra = loadU32(templ + 0x34);
        if (extra == 0) {
            return eax;
        }
        return g.applyOne(host, nullptr, extra);
    }
    const std::uint32_t key = loadU32(templ + 0x30);
    std::uint32_t eax = key;
    if (state == 2) {
        if (key != 0) {
            eax = g.applyOne(host, nullptr, key);
        }
        const std::uint32_t extra = loadU32(templ + 0x34);
        if (extra == 0) {
            return eax;
        }
        return g.applyNeg(host, nullptr, extra, 0xFFFFFFFFu);
    }
    if (key != 0) {
        eax = g.applyOne(host, nullptr, key);
    }
    const std::uint32_t extra = loadU32(templ + 0x34);
    if (extra == 0) {
        return eax;
    }
    return g.applyOne(host, nullptr, extra);
}

std::uint32_t HighlanderBody_noteHit(std::uint8_t* self, std::uint8_t* info) noexcept {
    const std::uint32_t stamp = loadU32(info + 0x20);
    if (loadU32(self + 0x3C) == 0) {
        return stamp;
    }
    auto* clock = asPtr(cell(e.clockCell));
    const std::uint32_t now = loadU32(clock + 0x58);
    if (stamp == loadU32(self + 0x40) && loadU32(self + 0x28) > now) {
        return stamp;
    }
    std::uint32_t who = 0;
    std::uint32_t source = loadU32(info + 8);
    if (source != 0) {
        g.lockName(e.nameLock, nullptr, kNameKey);
        std::uint32_t found = 0;
        g.mapFind(clock + 0xC4, nullptr, &found, &source);
        const std::uint32_t begin = loadU32(clock + 0xC8);
        const std::uint32_t count = loadU32(clock + 0xCC);
        if (found != loadU32(asPtr(begin + count * 4))) {
            who = loadU32(asPtr(found + 4));
        }
        g.unlockName(e.nameLock, nullptr, 1);
    }
    const std::uint32_t accepted = g.acceptDamage(asPtr(loadU32(self + 0x3C)), nullptr, info, who,
                                                   loadU32(self + 8));
    if (static_cast<std::uint8_t>(accepted) == 0) {
        return accepted;
    }
    storeU32(self + 0x40, stamp);
    const std::uint32_t delay = g.damageDelay(asPtr(loadU32(self + 0x3C)), nullptr, stamp);
    const std::uint32_t until = delay + now;
    storeU32(self + 0x28, until);
    return until;
}

std::uint32_t AttributeCrateCollide_onCollide(std::uint8_t* self, std::uint8_t* other) noexcept {
    if (other == nullptr) {
        return 1;
    }
    auto* templ = *reinterpret_cast<std::uint8_t**>(self + 4);
    g.applyNeg(other, nullptr, loadU32(templ + 0x6C), 0xFFFFFFFFu);
    return g.collideBase(self, nullptr, other);
}

std::uint32_t ToggleDeploySpecialAbilityUpdate_tryDeploy(std::uint8_t* self,
                                                                     std::uint8_t* object) noexcept {
    const std::uint32_t ready = call2(self + 0x24, 0x18, 0, kLimitBits);
    if (static_cast<std::uint8_t>(ready) == 0) {
        return ready;
    }
    auto* passenger = *reinterpret_cast<std::uint8_t**>(object + 0x374);
    if (passenger == nullptr) {
        return asBits(object);
    }
    auto* component = *reinterpret_cast<std::uint8_t**>(passenger + 0x214);
    bool direct = false;
    if (component != nullptr) {
        const std::uint32_t speed = loadU32(component + 0x38);
        direct = above(speed, kZeroBits) && above(speed, kTenthBits);
    }
    if (!direct) {
        const std::uint32_t armed = call0(passenger, 0x20C);
        if (static_cast<std::uint8_t>(armed) == 0) {
            return armed;
        }
    }
    return g.pushState(passenger + 0x24, nullptr, 2);
}

std::uint32_t HordeContain_removeMember(std::uint8_t* self, std::uint8_t* member) noexcept {
    const std::uint32_t key = loadU32(member + 0xDC);
    auto* container = self + 0x1B4;
    std::uint32_t outA = key;
    std::uint32_t outB = 0;
    g.findNode(container, nullptr, &outB, &outA);
    if (outB != asBits(container + 4)) {
        storeU32(container + 0x14, loadU32(container + 0x14) - 1);
        g.unlinkA(asPtr(outB));
        g.unlinkB(asPtr(outB), container + 4);
        g.release(asPtr(outB));
    }
    auto* face = self + 0x24;
    call1(face, 0x98, asBits(member));
    const std::uint32_t interior = asBits(objOf(self) + 0x3F8);
    return call2(face, 0x154, interior, 0);
}

std::uint32_t OpenContain_removeByKey(std::uint8_t* self, std::uint32_t key) noexcept {
    g.enterLock(kContainLock);
    auto* sentinel = self + 0x78;
    auto* node = *reinterpret_cast<std::uint8_t**>(sentinel);
    while (node != sentinel) {
        if (loadU32(node + 8) == key) {
            auto* next = *reinterpret_cast<std::uint8_t**>(node);
            auto* removed = *reinterpret_cast<std::uint8_t**>(next + 4);
            unlinkNode(removed);
            g.release(removed);
            storeU32(self + 0x84, loadU32(self + 0x84) - 1);
            node = next;
        } else {
            node = *reinterpret_cast<std::uint8_t**>(node);
        }
    }
    return g.leaveLock(kContainLock);
}

std::uint32_t PassiveAreaEffectBehavior_apply(std::uint8_t* self, std::uint8_t* target) noexcept {
    if (target == nullptr) {
        return 0;
    }
    auto* templ = *reinterpret_cast<std::uint8_t**>(self + 4);
    const std::uint32_t count = loadU32(templ + 0x30);
    auto* cursor = asPtr(loadU32(templ + 0x34));
    std::uint32_t eax = 0;
    for (std::uint32_t i = 0; i < count; ++i) {
        eax = g.applyNeg(target, nullptr, loadU32(cursor + i * 4), 0xFFFFFFFFu);
    }
    if (loadU32(target + 0x36C) == 0) {
        return eax;
    }
    auto* clock = asPtr(cell(e.clockCell));
    const std::uint32_t when = truncMul(static_cast<std::int32_t>(loadU32(templ + 0x18)),
                                        loadU32(clock + 0x1C4)) +
                               loadU32(clock + 0x58);
    eax = g.schedule(asPtr(loadU32(target + 0x36C)), nullptr, templ + 0x20, when);
    auto* effect = *reinterpret_cast<std::uint8_t**>(templ + 0x24);
    if (effect == nullptr) {
        return eax;
    }
    eax = g.ready(effect, nullptr);
    if (static_cast<std::uint8_t>(eax) != 0) {
        return eax;
    }
    return g.fireAt(effect, nullptr, target, 0);
}

std::uint32_t UnifiedControlBehavior_relay(std::uint8_t* self) noexcept {
    auto* object = objOf(self);
    call1(self, 0x5C, loadU32(object + 0x418));
    return g.tailControl(self, nullptr);
}

std::uint32_t SpawnBehavior_update(std::uint8_t* self) noexcept {
    auto* templ = *reinterpret_cast<std::uint8_t**>(self + 4);
    std::uint32_t eax = asBits(templ);
    if (templ[0x50] == 0) {
        return eax;
    }
    auto* head = self + 0x5C;
    auto* node = *reinterpret_cast<std::uint8_t**>(head);
    while (node != head) {
        std::uint32_t key = loadU32(node + 8);
        eax = key;
        std::uint32_t who = 0;
        if (key != 0) {
            auto* clock = asPtr(cell(e.clockCell));
            g.lockName(e.nameLock, nullptr, kNameKey);
            std::uint32_t found = 0;
            g.mapFind(clock + 0xC4, nullptr, &found, &key);
            const std::uint32_t begin = loadU32(clock + 0xC8);
            const std::uint32_t count = loadU32(clock + 0xCC);
            if (found != loadU32(asPtr(begin + count * 4))) {
                who = loadU32(asPtr(found + 4));
            }
            eax = g.unlockName(e.nameLock, nullptr, 1);
            if (who != 0) {
                auto* actor = asPtr(who);
                if (!spawnSuppressed(actor)) {
                    eax = g.hideOne(asPtr(loadU32(actor + 0x418)), nullptr, actor);
                }
                storeU32(actor + 0xE4, 0);
                if (!spawnSuppressed(actor)) {
                    eax = g.hideTwo(asPtr(loadU32(actor + 0x418)), nullptr, actor);
                }
            }
        }
        node = *reinterpret_cast<std::uint8_t**>(node);
        if (who != 0 && (asPtr(who)[0x471] & 1u) == 0) {
            eax = g.killObj(asPtr(cell(e.clockCell)), nullptr, asPtr(who));
        }
    }
    return eax;
}

}  // namespace ra3::gamelogic
