#include "ra3/gamelogic/ObjectModules72.h"

#include <cstring>

namespace ra3::gamelogic {
namespace {

Object72Calls g{};

std::uint32_t loadU32(const void* address) noexcept {
    std::uint32_t value = 0;
    std::memcpy(&value, address, 4);
    return value;
}

void storeU32(void* address, std::uint32_t value) noexcept {
    std::memcpy(address, &value, 4);
}

std::uint32_t call0(void* object, int offset) noexcept {
    auto* vt = *reinterpret_cast<std::uint8_t**>(object);
    auto* fn = *reinterpret_cast<std::uint32_t(__fastcall**)(void*, void*)>(vt + offset);
    return fn(object, nullptr);
}

std::uint32_t call1p(void* object, int offset, void* arg) noexcept {
    auto* vt = *reinterpret_cast<std::uint8_t**>(object);
    auto* fn = *reinterpret_cast<std::uint32_t(__fastcall**)(void*, void*, void*)>(vt + offset);
    return fn(object, nullptr, arg);
}

std::uint32_t call1u(void* object, int offset, std::uint32_t arg) noexcept {
    auto* vt = *reinterpret_cast<std::uint8_t**>(object);
    auto* fn = *reinterpret_cast<std::uint32_t(__fastcall**)(void*, void*, std::uint32_t)>(vt + offset);
    return fn(object, nullptr, arg);
}

std::uint32_t xferVersion(void* xfer, std::uint8_t second) noexcept {
    std::uint8_t bytes[2] = {1, second};
    return call1p(xfer, 0x18, bytes);
}

bool follows(std::uint32_t mode) noexcept {
    return mode == 0 || mode == 1;
}

std::uint8_t* objOf(std::uint8_t* self) noexcept {
    return *reinterpret_cast<std::uint8_t**>(self + 8);
}

std::uint8_t* locoOf(std::uint8_t* self) noexcept {
    return *reinterpret_cast<std::uint8_t**>(self + 0x3C);
}

void copy3(void* dst, const void* src) noexcept {
    std::memcpy(dst, src, 12);
}

void copyObjectPosition(std::uint8_t* self) noexcept {
    // The move bodies copy through the pointer at self+0x214, not the locomotor.
    auto* dest = *reinterpret_cast<std::uint8_t**>(self + 0x214);
    auto* obj = objOf(self);
    if (dest != nullptr && obj != nullptr) {
        copy3(dest + 0xC, obj + 0x38);
    }
}

std::uint32_t resolveWeapon(std::uint8_t* set) noexcept {
    if (set == nullptr) {
        return 0;
    }
    std::uint32_t found = g.findWeapon(set, nullptr, 0xFFFFFFFFu);
    if (found == 0) {
        found = g.findWeaponAlt(set, nullptr, found, 0xFFFFFFFFu);
    }
    return found;
}

// Returns whether EAX was replaced by the weapon pointer (or 0 when the search misses).
// A null weapon set leaves EAX alone, matching the test edi,edi skip.
bool writeWeaponAux(std::uint8_t* obj, std::uint32_t aux, std::uint32_t& eax) noexcept {
    // The search starts with `mov eax, [esi+8]`, so a null weapon set leaves EAX as the object.
    eax = reinterpret_cast<std::uint32_t>(obj);
    auto* set = *reinterpret_cast<std::uint8_t**>(obj + 0x37C);
    if (set == nullptr) {
        return true;
    }
    const std::uint32_t weapon = resolveWeapon(set);
    if (weapon != 0) {
        storeU32(reinterpret_cast<void*>(weapon + 0x24), aux);
    }
    eax = weapon;
    return true;
}

std::uint32_t finishState(std::uint8_t* self, std::uint32_t mode, std::uint32_t state,
                          std::uint32_t followArg, int which) noexcept {
    storeU32(self + 0x50, mode);
    std::uint32_t eax = call1u(locoOf(self), 0x1C, state);
    if (follows(mode)) {
        if (which == 0) {
            eax = g.followCf30(self, nullptr, followArg);
        } else if (which == 1) {
            eax = g.followCd10(self, nullptr, followArg);
        } else {
            eax = g.followCe90(self, nullptr, followArg);
        }
    }
    return eax;
}

std::uint32_t aimCommand(std::uint8_t* self, std::uint32_t target, std::uint32_t mode,
                         bool zeroTimer, std::uint32_t state) noexcept {
    auto* obj = objOf(self);
    const std::uint32_t usable = g.usable(obj, nullptr);
    if (static_cast<std::uint8_t>(usable) == 0) {
        return usable;
    }
    if (zeroTimer) {
        storeU32(obj + 0x458, 0);
    }
    auto* loco = locoOf(self);
    call0(loco, 0x34);
    g.aimAt(loco, nullptr, target);
    return finishState(self, mode, state, target + 0xC, 0);
}

std::uint32_t moveConst(std::uint8_t* self, std::uint32_t pos, std::uint32_t mode,
                        std::uint32_t state, bool clearThree) noexcept {
    auto* obj = objOf(self);
    const std::uint32_t usable = g.usable(obj, nullptr);
    if (static_cast<std::uint8_t>(usable) == 0) {
        return usable;
    }
    copyObjectPosition(self);
    auto* loco = locoOf(self);
    call0(loco, 0x34);
    g.steerB67a0(self, nullptr, reinterpret_cast<void*>(pos), kLocomotorLimitBits, mode);
    storeU32(self + 0x190, 0);
    self[0x432] = 0;
    std::uint32_t eax = finishState(self, mode, state, pos, 0);
    if (clearThree) {
        eax |= 0xFFFFFFFFu;
        storeU32(self + 0x308, eax);
        storeU32(self + 0x368, eax);
        storeU32(self + 0x3C8, eax);
    }
    return eax;
}

bool comissAbove(std::uint32_t lhsBits, std::uint32_t rhsBits) noexcept {
    const auto nan = [](std::uint32_t bits) {
        return (bits & 0x7F800000u) == 0x7F800000u && (bits & 0x007FFFFFu) != 0;
    };
    if (nan(lhsBits) || nan(rhsBits)) {
        return false;
    }
    float lhs = 0.f;
    float rhs = 0.f;
    std::memcpy(&lhs, &lhsBits, 4);
    std::memcpy(&rhs, &rhsBits, 4);
    return lhs > rhs;
}

std::uint32_t flagLow(std::uint32_t old) noexcept {
    const std::uint32_t shifted = old >> 14;
    return (shifted & 0xFFFFFF00u) | ((static_cast<std::uint8_t>(~shifted)) & 1u);
}

}  // namespace

void setObject72Calls(const Object72Calls& calls) noexcept {
    g = calls;
}

std::uint32_t AIUpdateInterface_moveWithVector(std::uint8_t* self, std::uint32_t pos, std::uint32_t vec,
                                               std::uint32_t bits, std::uint32_t mode) noexcept {
    auto* obj = objOf(self);
    const std::uint32_t usable = g.usable(obj, nullptr);
    if (static_cast<std::uint8_t>(usable) == 0) {
        return usable;
    }
    copyObjectPosition(self);
    auto* loco = locoOf(self);
    call0(loco, 0x34);
    g.steerB67a0(self, nullptr, reinterpret_cast<void*>(pos), bits, mode);
    if (vec != 0) {
        copy3(loco + 0x48, reinterpret_cast<void*>(vec));
        loco[0x44] = 1;
    }
    storeU32(self + 0x190, 0);
    self[0x432] = 0;
    return finishState(self, mode, 1, pos, 0);
}

std::uint32_t AIUpdateInterface_moveToObject(std::uint8_t* self, std::uint32_t target,
                                             std::uint32_t mode) noexcept {
    if (self[0x436] != 0) {
        return 0;
    }
    auto* obj = objOf(self);
    const std::uint32_t usable = g.usable(obj, nullptr);
    if (static_cast<std::uint8_t>(usable) == 0) {
        return usable;
    }
    copyObjectPosition(self);
    auto* loco = locoOf(self);
    call0(loco, 0x34);
    call1u(loco, 0x38, target);
    g.placeAc4f0(loco, nullptr, reinterpret_cast<void*>(target + 0x38), kLocomotorLimitBits);
    storeU32(self + 0x190, 0);
    self[0x432] = 0;
    return finishState(self, mode, 1, target + 0x38, 0);
}

std::uint32_t AIUpdateInterface_moveClear1C(std::uint8_t* self, std::uint32_t pos,
                                            std::uint32_t mode) noexcept {
    return moveConst(self, pos, mode, 0x1C, true);
}

std::uint32_t AIUpdateInterface_moveClear1D(std::uint8_t* self, std::uint32_t pos,
                                            std::uint32_t mode) noexcept {
    return moveConst(self, pos, mode, 0x1D, true);
}

std::uint32_t AIUpdateInterface_moveState1E(std::uint8_t* self, std::uint32_t pos,
                                            std::uint32_t mode) noexcept {
    return moveConst(self, pos, mode, 0x1E, false);
}

std::uint32_t AIUpdateInterface_moveState35(std::uint8_t* self, std::uint32_t pos,
                                            std::uint32_t mode) noexcept {
    return moveConst(self, pos, mode, 0x35, false);
}

std::uint32_t AIUpdateInterface_faceObject(std::uint8_t* self, std::uint32_t target,
                                           std::uint32_t mode) noexcept {
    auto* obj = objOf(self);
    const std::uint32_t usable = g.usable(obj, nullptr);
    if (static_cast<std::uint8_t>(usable) == 0) {
        return usable;
    }
    copyObjectPosition(self);
    auto* loco = locoOf(self);
    call0(loco, 0x34);
    call1u(loco, 0x38, target);
    storeU32(self + 0x190, 0);
    self[0x432] = 0;
    return finishState(self, mode, 0x37, reinterpret_cast<std::uintptr_t>(obj + 0x38), 0);
}

std::uint32_t AIUpdateInterface_command2F(std::uint8_t* self, std::uint32_t target,
                                          std::uint32_t mode) noexcept {
    if (self[0x436] != 0) {
        return 0;
    }
    auto* obj = objOf(self);
    const std::uint32_t usable = g.usable(obj, nullptr);
    if (static_cast<std::uint8_t>(usable) == 0) {
        return usable;
    }
    const std::uint32_t current = g.currentTarget(obj, nullptr);
    if (current != 0) {
        return current;
    }
    auto* loco = locoOf(self);
    const std::uint32_t queried = g.queryLoco(loco, nullptr);
    call0(loco, 0x34);
    call1u(loco, 0x38, target);
    storeU32(self + 0x50, mode);
    if (mode != 2) {
        g.markObject(obj, nullptr, queried);
    }
    std::uint32_t eax = call1u(loco, 0x1C, 0x2F);
    if (follows(mode)) {
        eax = g.followCf30(self, nullptr, target + 0x38);
    }
    return eax;
}

std::uint32_t AIUpdateInterface_aimState3(std::uint8_t* self, std::uint32_t target,
                                          std::uint32_t mode) noexcept {
    auto* obj = objOf(self);
    const std::uint32_t usable = g.usable(obj, nullptr);
    if (static_cast<std::uint8_t>(usable) == 0) {
        return usable;
    }
    storeU32(obj + 0x458, 0);
    auto* loco = locoOf(self);
    call0(loco, 0x34);
    g.aimAt(loco, nullptr, target);
    storeU32(self + 0x50, mode);
    auto* templ = *reinterpret_cast<std::uint8_t**>(obj + 4);
    if (((loadU32(templ + 0xD4) >> 21) & 1u) != 0 && target != 0) {
        std::uint32_t chain = *reinterpret_cast<std::uint32_t*>(obj + 0x328);
        if (chain == 0) {
            const std::uint32_t begin = loadU32(obj + 0x32C);
            const std::uint32_t end = loadU32(obj + 0x330);
            chain = begin != end ? loadU32(reinterpret_cast<void*>(begin)) : 0;
        }
        if (chain != 0) {
            std::uint32_t cursor = target;
            while (loadU32(reinterpret_cast<void*>(cursor + 0x20)) != 0) {
                cursor = loadU32(reinterpret_cast<void*>(cursor + 0x20));
            }
        }
    }
    return finishState(self, mode, 3, target + 0xC, 0);
}

std::uint32_t AIUpdateInterface_aimState5(std::uint8_t* self, std::uint32_t target,
                                          std::uint32_t mode) noexcept {
    return aimCommand(self, target, mode, true, 5);
}

std::uint32_t AIUpdateInterface_aimState2(std::uint8_t* self, std::uint32_t target,
                                          std::uint32_t mode) noexcept {
    return aimCommand(self, target, mode, false, kAimState2);
}

std::uint32_t AIUpdateInterface_aimState4(std::uint8_t* self, std::uint32_t target,
                                          std::uint32_t mode) noexcept {
    return aimCommand(self, target, mode, false, 4);
}

std::uint32_t AIUpdateInterface_order2C(std::uint8_t* self, std::uint32_t target,
                                        std::uint32_t mode) noexcept {
    if (target == 0) {
        return 0;
    }
    if (mode != 2) {
        g.markObject(objOf(self), nullptr, target);
    }
    auto* loco = locoOf(self);
    call0(loco, 0x34);
    call1u(loco, 0x38, target);
    storeU32(self + 0x50, mode);
    std::uint32_t eax = call1u(loco, 0x1C, 0x2C);
    writeWeaponAux(objOf(self), 1, eax);
    if (follows(mode)) {
        eax = g.followCd10(self, nullptr, target);
    }
    return eax;
}

std::uint32_t AIUpdateInterface_order0B(std::uint8_t* self, std::uint32_t target, std::uint32_t aux,
                                        std::uint32_t mode) noexcept {
    if (target == 0) {
        return 0;
    }
    auto* loco = locoOf(self);
    call0(loco, 0x34);
    call1u(loco, 0x38, target);
    storeU32(self + 0x50, mode);
    std::uint32_t eax = call1u(loco, 0x1C, 0x0B);
    writeWeaponAux(objOf(self), aux, eax);
    if (follows(mode)) {
        eax = g.followCd10(self, nullptr, target);
    }
    return eax;
}

std::uint32_t AIUpdateInterface_order16(std::uint8_t* self, std::uint32_t key, std::uint32_t aux,
                                        std::uint32_t mode) noexcept {
    auto* loco = locoOf(self);
    call0(loco, 0x34);
    g.prepareDed70(loco, nullptr, key);
    storeU32(self + 0x50, mode);
    call1u(loco, 0x1C, 0x16);
    std::uint32_t ignored = 0;
    writeWeaponAux(objOf(self), aux, ignored);
    const std::uint32_t queried = g.queryLoco(loco, nullptr);
    if (queried != 0 && follows(mode)) {
        return g.followCd10(self, nullptr, queried);
    }
    return queried;
}

std::uint32_t AIUpdateInterface_aimFlagged(std::uint8_t* self, std::uint32_t target, std::uint32_t aux,
                                           std::uint32_t flag, std::uint32_t mode) noexcept {
    if (self[0x436] != 0) {
        return 0;
    }
    auto* obj = objOf(self);
    const std::uint32_t usable = g.usable(obj, nullptr);
    if (static_cast<std::uint8_t>(usable) == 0) {
        return usable;
    }
    auto* loco = locoOf(self);
    call0(loco, 0x34);
    g.aimAt(loco, nullptr, target);
    storeU32(self + 0x50, mode);
    const std::uint32_t state = 0x22u + ((flag & 0xFFu) != 0 ? 1u : 0u);
    std::uint32_t eax = call1u(loco, 0x1C, state);
    writeWeaponAux(obj, aux, eax);
    if (follows(mode)) {
        eax = g.followCe90(self, nullptr, target + 0xC);
    }
    return eax;
}

std::uint32_t AIUpdateInterface_guardPoint(std::uint8_t* self, std::uint32_t point,
                                           std::uint32_t mode) noexcept {
    auto* obj = objOf(self);
    const std::uint32_t usable = g.usable(obj, nullptr);
    if (static_cast<std::uint8_t>(usable) == 0) {
        return usable;
    }
    auto* templ = *reinterpret_cast<std::uint8_t**>(obj + 4);
    if (((loadU32(templ + 0xC0) >> 26) & 1u) != 0) {
        return reinterpret_cast<std::uint32_t>(templ);
    }
    storeU32(self + 0x74, point);
    auto* loco = locoOf(self);
    call0(loco, 0x34);
    storeU32(self + 0x50, mode);
    std::uint32_t eax = call1u(loco, 0x1C, 0x1F);
    if (follows(mode)) {
        std::uint8_t local[12]{};
        g.readPoint(reinterpret_cast<void*>(point), nullptr, local);
        eax = g.followCdf0(self, nullptr, local);
    }
    return eax;
}

std::uint32_t AIUpdateInterface_attach60(std::uint8_t* self, std::uint32_t arg,
                                         std::uint32_t mode) noexcept {
    auto* obj = objOf(self);
    std::uint32_t edi = 0;
    if ((obj[0x471] & 1u) == 0) {
        auto* draw = *reinterpret_cast<std::uint8_t**>(obj + 0x374);
        if (draw != nullptr) {
            auto* nested = *reinterpret_cast<std::uint8_t**>(draw + 0x47C);
            if (nested != nullptr) {
                const std::uint32_t found = g.queryD2120(nested, nullptr);
                if (found != 0) {
                    edi = found;
                }
            }
        }
        if (edi == 0) {
            auto* set = *reinterpret_cast<std::uint8_t**>(obj + 0x37C);
            if (set != nullptr) {
                const std::uint32_t found = g.queryA18a0(set, nullptr);
                if (found != 0) {
                    edi = found;
                }
            }
        }
    }
    auto* loco = locoOf(self);
    call0(loco, 0x34);
    g.bindPair(self + 0x3C8, nullptr, self + 0x368);
    call1u(loco, 0x38, edi);
    g.noteAa2a0(loco, nullptr, arg);
    storeU32(self + 0x50, mode);
    return call1u(loco, 0x1C, 0x60);
}

std::uint32_t StancesBehavior_addStanceId(std::uint8_t* self, std::uint32_t id) noexcept {
    auto* object = *reinterpret_cast<std::uint8_t**>(self - 8);
    if (id == loadU32(object + 0xDC)) {
        return reinterpret_cast<std::uint32_t>(object);
    }
    auto* vec = self + 0xE0;
    std::uint32_t begin = loadU32(vec);
    const std::uint32_t end = loadU32(vec + 4);
    if (begin != end) {
        std::uint32_t cursor = begin;
        while (cursor != end) {
            if (loadU32(reinterpret_cast<void*>(cursor)) == id) {
                return cursor;
            }
            cursor += 4;
        }
    }
    std::uint32_t slot = loadU32(vec + 4);
    const std::uint32_t cap = loadU32(vec + 8);
    if (slot >= cap) {
        return g.growVector(vec, nullptr, slot, reinterpret_cast<std::uint32_t>(&id));
    }
    storeU32(vec + 4, slot + 4);
    if (slot == 0) {
        return 0;
    }
    storeU32(reinterpret_cast<void*>(slot), id);
    return slot;
}

std::uint32_t StancesBehavior_linkStance(std::uint8_t* self, std::uint32_t id) noexcept {
    auto* head = self + 0xD4;
    auto* cursor = *reinterpret_cast<std::uint8_t**>(head);
    if (cursor != head) {
        do {
            if (loadU32(cursor + 8) == id) {
                break;
            }
            cursor = *reinterpret_cast<std::uint8_t**>(cursor);
        } while (cursor != head);
        if (cursor != head) {
            return reinterpret_cast<std::uint32_t>(cursor);
        }
    }
    auto* node = reinterpret_cast<std::uint8_t*>(g.allocNode(head + 8, nullptr, 0xC, 0));
    if (node + 8 != nullptr) {
        storeU32(node + 8, id);
    }
    auto* prev = *reinterpret_cast<std::uint8_t**>(head + 4);
    storeU32(node, reinterpret_cast<std::uint32_t>(head));
    storeU32(node + 4, reinterpret_cast<std::uint32_t>(prev));
    storeU32(prev, reinterpret_cast<std::uint32_t>(node));
    storeU32(head + 4, reinterpret_cast<std::uint32_t>(node));
    auto* object = *reinterpret_cast<std::uint8_t**>(self - 8);
    const std::uint32_t old = loadU32(object + 0x94);
    storeU32(object + 0x94, old | 0x4000u);
    std::uint32_t eax = flagLow(old);
    if (static_cast<std::uint8_t>(eax) != 0) {
        eax = g.notifyBit(object, nullptr, 0xAE, 1);
    }
    return eax;
}

std::uint32_t WeaponFireSpecialAbilityUpdate_templateBit8(std::uint8_t* self) noexcept {
    auto* obj = objOf(self);
    const std::uint32_t bits = loadU32(obj + 0xBC);
    if (((bits >> 8) & 1u) != 0) {
        return (reinterpret_cast<std::uint32_t>(obj) & 0xFFFFFF00u) | 1u;
    }
    return g.abilityReady(self, nullptr);
}

std::uint32_t WeaponFireSpecialAbilityUpdate_armAndTrigger(std::uint8_t* self) noexcept {
    auto* templ = *reinterpret_cast<std::uint8_t**>(self + 4);
    const std::uint32_t key = loadU32(templ + 0x104);
    if (key != 0xFFFFFFFFu) {
        g.applyKey(objOf(self), nullptr, key);
    }
    return g.triggerAbility(self, nullptr, 1);
}

std::uint32_t ToggleMountedSpecialAbilityUpdate_clearMountedBit(std::uint8_t* self, std::uint32_t a,
                                                                std::uint32_t b) noexcept {
    auto* obj = objOf(self);
    auto* mounted = *reinterpret_cast<std::uint8_t**>(obj + 0xF4);
    if (mounted != nullptr) {
        storeU32(mounted + 0x1BC, kUnitFloatBits);
    }
    const std::uint32_t old = loadU32(obj + 0x88);
    storeU32(obj + 0x88, old & 0xFFFFDFFFu);
    if (((old >> 13) & 1u) != 0) {
        g.notifyBit(obj, nullptr, 0x4D, 0);
    }
    return g.xferBase88(self, nullptr, a, b);
}

std::uint32_t BeamSpecialAbilityUpdate_resetBeam(std::uint8_t* self, std::uint32_t a,
                                                 std::uint32_t b) noexcept {
    g.xferBase88(self, nullptr, a, b);
    g.beamPulse(self, nullptr);
    if (self[0xFA] == 0) {
        return 0;
    }
    self[0xFA] = 0;
    storeU32(self + 0xFC, 0);
    storeU32(self + 0x100, 0);
    storeU32(self + 0x104, 0);
    storeU32(self + 0x108, 0);
    auto* obj = objOf(self);
    auto* draw = *reinterpret_cast<std::uint8_t**>(obj + 0x374);
    const std::uint32_t produced = call0(draw, 0x288);
    return g.applyBeam(draw + 0x24, nullptr, obj + 0x38, produced);
}

std::uint32_t ActivateModuleSpecialAbilityUpdate_xferAndActivate(std::uint8_t* self, std::uint32_t a,
                                                                 std::uint32_t b) noexcept {
    g.xferBase88(self, nullptr, a, b);
    return g.activateOne(self, nullptr, 1);
}

std::uint32_t GiveUpgradeUpdate_clearThenXfer(std::uint8_t* self, std::uint32_t a,
                                              std::uint32_t b) noexcept {
    g.clearUpgrade(self, nullptr);
    return g.xferBase88(self, nullptr, a, b);
}

std::uint32_t OCLMonitorSpecialAbilityUpdate_stopThenXfer(std::uint8_t* self, std::uint32_t a,
                                                          std::uint32_t b) noexcept {
    g.stopMonitor(self, nullptr);
    return g.xferBase88(self, nullptr, a, b);
}

std::uint32_t ShieldSphereManager_xfer(std::uint8_t* self, std::uint8_t* xfer) noexcept {
    std::uint8_t version[2] = {1, 2};
    call1p(xfer, 0x18, version);
    g.xferShieldA(self + 8, nullptr, xfer);
    std::uint32_t eax = g.xferShieldB(self + 0x650, nullptr, xfer);
    if (version[1] >= 2) {
        eax = g.xferShieldC(self + 0x32C, nullptr, xfer);
    }
    return eax;
}

std::uint32_t RadarScanMapSpecialAbilityUpdate_xfer(std::uint8_t* self, std::uint8_t* xfer) noexcept {
    g.xferAbility(self, nullptr, xfer);
    xferVersion(xfer, 1);
    g.xferField(xfer, self + 0xC8);
    return g.xferRadarTail(xfer, self + 0xD8);
}

std::uint32_t CaptureSpecialAbilityUpdate_xfer(std::uint8_t* self, std::uint8_t* xfer) noexcept {
    xferVersion(xfer, 1);
    g.xferAbility(self, nullptr, xfer);
    return call1p(xfer, 0x64, self + 0xB8);
}

std::uint32_t CompositeStructureInfoModule_xfer(std::uint8_t* self, std::uint8_t* xfer) noexcept {
    xferVersion(xfer, 1);
    return g.xferField(xfer, self + 0x18);
}

std::uint32_t StoreObjectsSpecialPower_xfer(std::uint8_t* self, std::uint8_t* xfer) noexcept {
    std::uint8_t version[2] = {1, 2};
    call1p(xfer, 0x18, version);
    g.xferPower(self, nullptr, xfer);
    g.xferField(xfer, self + 0x50);
    call1p(xfer, 0x58, self + 0x60);
    std::uint32_t eax = call1p(xfer, 0x64, self + 0x6C);
    if (version[1] >= 2) {
        eax = g.xferPair(xfer, self + 0x70);
    }
    return eax;
}

std::uint32_t GiveUpgradeUpdate_xfer(std::uint8_t* self, std::uint8_t* xfer) noexcept {
    std::uint32_t eax = g.xferAbility(self, nullptr, xfer);
    if (((loadU32(xfer + 4) >> 3) & 1u) != 0) {
        return (eax & 0xFFFFFF00u) | 1u;
    }
    xferVersion(xfer, 1);
    call1p(xfer, 0x84, self + 0xB8);
    call1p(xfer, 0x64, self + 0xBC);
    return call1p(xfer, 0x84, self + 0xB9);
}

std::uint32_t DeployStyleAIUpdate_xfer(std::uint8_t* self, std::uint8_t* xfer) noexcept {
    std::uint32_t eax = g.xferAi(self, nullptr, xfer);
    if (((loadU32(xfer + 4) >> 3) & 1u) != 0) {
        return (eax & 0xFFFFFF00u) | 1u;
    }
    xferVersion(xfer, 1);
    return g.xferDeploy(self + 0x51C, nullptr, xfer);
}

std::uint32_t HordeAIUpdate_xfer(std::uint8_t* self, std::uint8_t* xfer) noexcept {
    xferVersion(xfer, 1);
    return g.xferAi(self, nullptr, xfer);
}

std::uint32_t ManagedActiveBody_xfer(std::uint8_t* self, std::uint8_t* xfer) noexcept {
    xferVersion(xfer, 1);
    g.xferBody(self, nullptr, xfer);
    return g.xferField(xfer, self + 0x124);
}

std::uint32_t SpawnCrateUpdate_xfer(std::uint8_t* self, std::uint8_t* xfer) noexcept {
    xferVersion(xfer, 1);
    xferVersion(xfer, 1);
    g.xferRoot(self, nullptr, xfer);
    g.xferAt10(self + 0x10, nullptr, xfer);
    const std::uint32_t count =
        static_cast<std::uint32_t>(static_cast<std::int32_t>(loadU32(self + 0x38) - loadU32(self + 0x34)) >> 3);
    std::uint32_t stored = count;
    call1p(xfer, 0x6C, &stored);
    // The flag test replaces EAX with xfer+4 >> 1. Resize replaces it again; an empty
    // vector leaves that value in EAX because the element loop does not run.
    std::uint32_t eax = loadU32(xfer + 4) >> 1;
    if ((eax & 1u) != 0) {
        eax = g.resizeCrate(self + 0x34, nullptr, stored);
    }
    std::uint32_t cursor = loadU32(self + 0x34);
    const std::uint32_t end = loadU32(self + 0x38);
    while (cursor != end) {
        g.xferPair(xfer, reinterpret_cast<void*>(cursor));
        eax = call1u(xfer, 0x6C, cursor + 4);
        cursor += 8;
    }
    return eax;
}

std::uint32_t BroadcastStealthUpdate_xfer(std::uint8_t* self, std::uint8_t* xfer) noexcept {
    xferVersion(xfer, 1);
    g.xferStealth(self, nullptr, xfer);
    if (((loadU32(xfer + 4) >> 3) & 1u) != 0) {
        return loadU32(xfer + 4) >> 3;
    }
    g.xferAt10(self + 0x18, nullptr, xfer);
    std::uint32_t eax = 0;
    if ((loadU32(xfer + 4) & 1u) != 0) {
        auto* head = self + 0x2C;
        std::uint32_t count = 0;
        for (auto* node = *reinterpret_cast<std::uint8_t**>(head); node != head;
             node = *reinterpret_cast<std::uint8_t**>(node)) {
            ++count;
        }
        eax = call1p(xfer, 0x6C, &count);
        for (auto* node = *reinterpret_cast<std::uint8_t**>(head); node != head;
             node = *reinterpret_cast<std::uint8_t**>(node)) {
            std::uint32_t value = loadU32(node + 8);
            eax = g.xferPair(xfer, &value);
        }
        return eax;
    }
    std::uint32_t count = 0;
    eax = call1p(xfer, 0x6C, &count);
    auto* head = self + 0x2C;
    for (std::uint32_t index = 0; index < count; ++index) {
        std::uint32_t value = 0;
        g.xferPair(xfer, &value);
        auto* node = reinterpret_cast<std::uint8_t*>(g.allocNode(head + 8, nullptr, 0xC, 0));
        if (node + 8 != nullptr) {
            storeU32(node + 8, value);
        }
        auto* prev = *reinterpret_cast<std::uint8_t**>(head + 4);
        storeU32(node, reinterpret_cast<std::uint32_t>(head));
        storeU32(node + 4, reinterpret_cast<std::uint32_t>(prev));
        storeU32(prev, reinterpret_cast<std::uint32_t>(node));
        storeU32(head + 4, reinterpret_cast<std::uint32_t>(node));
        eax = reinterpret_cast<std::uint32_t>(node);
    }
    return eax;
}

std::uint32_t StrafeAreaTargetChooser_xfer(std::uint8_t* self, std::uint8_t* xfer) noexcept {
    xferVersion(xfer, 1);
    xferVersion(xfer, 1);
    g.xferRoot(self, nullptr, xfer);
    const std::uint32_t count =
        static_cast<std::uint32_t>(static_cast<std::int32_t>(loadU32(self + 0x18) - loadU32(self + 0x14)) >> 3);
    std::uint32_t stored = count;
    std::uint32_t eax = call1p(xfer, 0x6C, &stored);
    if (((loadU32(xfer + 4) >> 1) & 1u) != 0) {
        eax = g.resizeStrafe(self + 0x14, nullptr, stored);
    }
    auto* begin = reinterpret_cast<std::uint8_t*>(loadU32(self + 0x14));
    for (std::uint32_t index = 0; index < stored; ++index) {
        g.xferPair(xfer, begin + index * 8);
        eax = call1p(xfer, 0x7C, begin + index * 8 + 4);
    }
    return call1p(xfer, 0x84, self + 0x24);
}

std::uint32_t SubStructureStorageBehavior_xfer(std::uint8_t* self, std::uint8_t* xfer) noexcept {
    xferVersion(xfer, 1);
    xferVersion(xfer, 1);
    g.xferRoot(self, nullptr, xfer);
    const std::uint32_t raw =
        static_cast<std::uint32_t>(static_cast<std::int32_t>(loadU32(self + 0x14) - loadU32(self + 0x10)) >> 3);
    std::uint8_t count = static_cast<std::uint8_t>(raw);
    std::uint32_t eax = call1p(xfer, 0x7C, &count);
    if (((loadU32(xfer + 4) >> 1) & 1u) != 0) {
        eax = g.resizeStorage(self + 0x10, nullptr, count);
    }
    auto* begin = reinterpret_cast<std::uint8_t*>(loadU32(self + 0x10));
    for (std::uint8_t index = 0; index < count; ++index) {
        call1p(xfer, 0x6C, begin + static_cast<unsigned>(index) * 8);
        eax = call1p(xfer, 0x64, begin + static_cast<unsigned>(index) * 8 + 4);
    }
    return call1p(xfer, 0x84, self + 0x20);
}

std::uint32_t FireWeaponWhenDamagedBehavior_onDamage(std::uint8_t* self, std::uint8_t* damage) noexcept {
    const std::uint32_t ready = call0(self + 0x10, 0);
    if (static_cast<std::uint8_t>(ready) == 0) {
        return ready;
    }
    const std::uint32_t shift = loadU32(damage + 0x1C) & 31u;
    const std::uint32_t mask = 1u << shift;
    auto* templ = *reinterpret_cast<std::uint8_t**>(self + 4);
    // After the mask is loaded, EAX is the template pointer.
    if ((loadU32(templ + 0x24) & mask) == 0) {
        return reinterpret_cast<std::uint32_t>(templ);
    }
    if (comissAbove(loadU32(templ + 0x28), loadU32(damage + 0x8C))) {
        return reinterpret_cast<std::uint32_t>(templ);
    }
    auto* obj = objOf(self);
    auto* selector = *reinterpret_cast<std::uint8_t**>(obj + 0x33C);
    const std::uint32_t kind = call0(selector, 0x54);
    std::uint8_t* module = nullptr;
    if (kind == 3 || kind == 1) {
        module = *reinterpret_cast<std::uint8_t**>(self + (kind == 3 ? 0x38 : 0x30));
    } else if (kind == 2) {
        module = *reinterpret_cast<std::uint8_t**>(self + 0x34);
    } else {
        module = *reinterpret_cast<std::uint8_t**>(self + 0x2C);
    }
    if (module == nullptr) {
        return kind;
    }
    const std::uint32_t idle = g.weaponIdle(module, nullptr);
    if (idle != 0) {
        return idle;
    }
    return g.fireAt(obj, obj + 0x38);
}

}  // namespace ra3::gamelogic
