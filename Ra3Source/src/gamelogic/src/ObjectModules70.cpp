#include "ra3/gamelogic/ObjectModules70.h"

#include <cstring>
#include <xmmintrin.h>

namespace ra3::gamelogic {
namespace {

Object70Env g_env{};
Object70Calls g_calls{};

std::uint32_t ld(const void* address) {
    std::uint32_t value = 0;
    std::memcpy(&value, address, 4);
    return value;
}

std::uint32_t ld(std::uint32_t address) {
    return ld(reinterpret_cast<const void*>(static_cast<std::uintptr_t>(address)));
}

void st(void* address, std::uint32_t value) {
    std::memcpy(address, &value, 4);
}

void st(std::uint32_t address, std::uint32_t value) {
    st(reinterpret_cast<void*>(static_cast<std::uintptr_t>(address)), value);
}

std::uint8_t ld8(const void* address) {
    return *static_cast<const std::uint8_t*>(address);
}

std::uint8_t ld8(std::uint32_t address) {
    return ld8(reinterpret_cast<const void*>(static_cast<std::uintptr_t>(address)));
}

void st8(void* address, std::uint8_t value) {
    *static_cast<std::uint8_t*>(address) = value;
}

void st8(std::uint32_t address, std::uint8_t value) {
    st8(reinterpret_cast<void*>(static_cast<std::uintptr_t>(address)), value);
}

// comiss relation of a versus b: -2 unordered, -1 a<b, 0 equal, +1 a>b.
int sseRel(std::uint32_t a, std::uint32_t b) {
    const auto nan = [](std::uint32_t x) { return (x & 0x7fffffffu) > 0x7f800000u; };
    if (nan(a) || nan(b)) {
        return -2;
    }
    const auto zero = [](std::uint32_t x) { return (x & 0x7fffffffu) == 0; };
    if (zero(a) && zero(b)) {
        return 0;
    }
    float fa = 0;
    float fb = 0;
    std::memcpy(&fa, &a, 4);
    std::memcpy(&fb, &b, 4);
    if (fa < fb) {
        return -1;
    }
    if (fa > fb) {
        return 1;
    }
    return 0;
}

bool sseEqual(std::uint32_t a, std::uint32_t b) {
    return sseRel(a, b) == 0;
}

template <class T>
T vslot(const void* self, unsigned offset) {
    const auto vt = reinterpret_cast<const std::uint8_t*>(static_cast<std::uintptr_t>(ld(self)));
    T fn = nullptr;
    std::memcpy(&fn, vt + offset, 4);
    return fn;
}

// EASTL vector<uint32> push. end >= cap (unsigned) grows; a null end is stored but not written.
std::uint32_t pushRecord(std::uint8_t* vec, std::uint32_t source) {
    const std::uint32_t end = ld(vec + 4);
    const std::uint32_t cap = ld(vec + 8);
    if (end >= cap) {
        return g_calls.vectorGrow(vec, nullptr, end, source);
    }
    st(vec + 4, end + 4);
    if (end == 0) {
        return 0;
    }
    st(end, ld(source));
    return end;
}

std::uint32_t pushDword(std::uint8_t* vec, std::uint32_t value) {
    const std::uint32_t end = ld(vec + 4);
    const std::uint32_t cap = ld(vec + 8);
    if (end >= cap) {
        return g_calls.vectorGrow(vec, nullptr, end, reinterpret_cast<std::uint32_t>(&value));
    }
    st(vec + 4, end + 4);
    if (end == 0) {
        return 0;
    }
    st(end, value);
    return end;
}

std::uint32_t boolFromAl(std::uint32_t eax) {
    const std::uint8_t al = static_cast<std::uint8_t>(eax);
    // neg al; sbb eax, eax; add eax, 1  -> 1 when al was 0, else 0.
    return al == 0 ? 1u : 0u;
}

void xferVersion(std::uint8_t* xfer, std::uint8_t major, std::uint8_t minor, std::uint8_t out[2]) {
    out[0] = major;
    out[1] = minor;
    vslot<std::uint32_t(__fastcall*)(void*, void*, std::uint32_t)>(xfer, 0x18)(
        xfer, nullptr, reinterpret_cast<std::uint32_t>(out));
}

}  // namespace

void setObject70(const Object70Env& env, const Object70Calls& calls) noexcept {
    g_env = env;
    g_calls = calls;
}

void object70Env(Object70Env& out) noexcept {
    out = g_env;
}

std::uint32_t BeamSpecialAbilityUpdate_setMask46(std::uint32_t arg) noexcept {
    auto* slot = reinterpret_cast<std::uint32_t*>(static_cast<std::uintptr_t>(arg));
    *slot = 0;
    *slot |= 0x46u;
    return arg;
}

std::uint32_t BeamSpecialAbilityUpdate_adjustB4(std::uint8_t* self) noexcept {
    if (reinterpret_cast<std::uintptr_t>(self) == 0x0c) {
        return 0;
    }
    return reinterpret_cast<std::uint32_t>(self + 0xb4);
}

std::uint32_t BeamSpecialAbilityUpdate_adjustB0(std::uint8_t* self) noexcept {
    if (reinterpret_cast<std::uintptr_t>(self) == 0x0c) {
        return 0;
    }
    return reinterpret_cast<std::uint32_t>(self + 0xb0);
}

std::uint32_t BeamSpecialAbilityUpdate_sameOwner(std::uint8_t* self, std::uint32_t other) noexcept {
    const std::uint32_t mine = ld(reinterpret_cast<void*>(ld(ld(ld(self - 0xb8) + 8) + 8)));
    const std::uint32_t theirs = ld(reinterpret_cast<void*>(ld(other + 8)));
    return ld(mine + 4) == ld(theirs + 4) ? 1u : 0u;
}

std::uint32_t OCLMonitorSpecialAbilityUpdate_spanOpen(std::uint8_t* self) noexcept {
    const std::uint32_t eax = ld(self + 0xb8);
    const std::uint8_t al = eax != ld(self + 0xbc) ? 1u : 0u;
    return (eax & 0xffffff00u) | al;
}

std::uint32_t OCLMonitorSpecialAbilityUpdate_forward(std::uint8_t* self, std::uint32_t a0, std::uint32_t a1,
                                                     std::uint32_t a2, std::uint32_t a3, std::uint32_t a4,
                                                     std::uint32_t a5) noexcept {
    g_calls.oclForward(self, nullptr, a0, a1, a2, a3, a4, a5);
    return g_calls.oclFinish(self - 0x24, nullptr);
}

std::uint32_t WeaponFireSpecialAbilityUpdate_sameOwner(std::uint8_t* self, std::uint32_t other) noexcept {
    const std::uint32_t mine = ld(reinterpret_cast<void*>(ld(ld(ld(self - 0xb4) + 8) + 8)));
    const std::uint32_t theirs = ld(reinterpret_cast<void*>(ld(other + 8)));
    return ld(mine + 4) == ld(theirs + 4) ? 1u : 0u;
}

std::uint32_t LinearDamageUpdate_relayIfArmed(std::uint8_t* self, std::uint32_t) noexcept {
    const std::uint32_t eax = ld(self - 0x20);
    if ((ld8(reinterpret_cast<void*>(eax + 8)) & 1) == 0) {
        return eax;
    }
    return g_calls.clearDamage(self - 0x24, nullptr);
}

std::uint32_t LinearDamageUpdate_clearIfUnset(std::uint8_t* self, std::uint32_t bit) noexcept {
    std::uint32_t scratch = 0;
    const auto bits = vslot<std::uint32_t(__fastcall*)(void*, void*, std::uint32_t)>(self - 0x18, 8)(
        self - 0x18, nullptr, reinterpret_cast<std::uint32_t>(&scratch));
    const std::uint32_t mask = 1u << (bit & 31u);
    if ((ld(reinterpret_cast<void*>(static_cast<std::uintptr_t>(bits) + (bit >> 5) * 4)) & mask) == 0) {
        return g_calls.clearDamage(self - 0x28, nullptr);
    }
    return bits;
}

std::uint32_t SubStructureBuildUpgrade_maskIf(std::uint8_t* self, std::uint32_t arg) noexcept {
    const std::uint32_t eax = g_calls.subRelay(self, nullptr, arg);
    if ((eax & 0xff) == 0) {
        return eax;
    }
    return (eax & 0xffffff00u) | ld8(self + 0x30);
}

std::uint32_t SubObjectsUpgrade_relay(std::uint8_t* self, std::uint32_t arg) noexcept {
    const std::uint32_t object = ld(self - 8);
    if (ld(object + 0xf4) == 0) {
        return object & 0xffffff00u;
    }
    return g_calls.subRelay(self, nullptr, arg);
}

std::uint32_t SpecialAbilityUpdate_pointInside(std::uint8_t* self, std::uint32_t a0, std::uint32_t a1) noexcept {
    const std::uint32_t point = ld(self - 0x1c) + 0x38;
    const std::uint32_t eax = g_calls.pointTest(self - 0x24, nullptr, point, a0, a1);
    return boolFromAl(eax);
}

std::uint32_t StoreTemplateIDBehavior_pushId(std::uint8_t* self, std::uint32_t id) noexcept {
    return pushDword(self + 4, id);
}

std::uint32_t StancesBehavior_clearRecords(std::uint8_t* self) noexcept {
    const std::uint32_t begin = ld(self + 0xf0);
    const std::uint32_t end = ld(self + 0xf4);
    const std::uint32_t eax = g_calls.moveRange(end, end, begin);
    std::int32_t esi = static_cast<std::int32_t>(end) - static_cast<std::int32_t>(begin);
    esi >>= 4;
    esi = -esi;
    esi <<= 4;
    st(self + 0xf4, ld(self + 0xf4) + static_cast<std::uint32_t>(esi));
    return eax;
}

std::uint32_t StancesBehavior_accumulate(std::uint8_t* self, std::uint32_t deltaBits, std::uint32_t source) noexcept {
    const std::uint32_t previous = ld(self + 4);
    st(self + 8, previous);
    __m128 sum = _mm_load_ss(reinterpret_cast<const float*>(self + 4));
    float delta = 0;
    std::memcpy(&delta, &deltaBits, 4);
    sum = _mm_add_ss(sum, _mm_set_ss(delta));
    _mm_store_ss(reinterpret_cast<float*>(self + 4), sum);
    if (source != 0 && sseRel(0, deltaBits) == 1) {
        const std::uint32_t extra = ld(source + 8);
        g_calls.stancePush(self - 0x10, nullptr, extra, deltaBits);
    }
    if (sseRel(ld(self + 4), ld(self + 0x0c)) == 1) {
        st(self + 4, ld(self + 0x0c));
    }
    if (sseRel(g_env.stanceFloorBits, ld(self + 4)) == 1) {
        st(self + 4, 0);
    }
    auto* parent = self - 0x10;
    g_calls.stancePulse(parent, nullptr);
    const std::uint32_t saved = ld(self + 0x34);
    std::uint32_t eax = vslot<std::uint32_t(__fastcall*)(void*, void*)>(parent, 0x6c)(parent, nullptr);
    const std::uint32_t owner = ld(self - 8);
    if (ld(self + 0x34) != saved) {
        const int rel = sseRel(0, ld(self + 4));
        const bool nonPositive = rel == 0 || rel == 1;
        const bool bit = (ld(owner + 0x80) >> 2) & 1u;
        if (nonPositive || !bit) {
            eax = vslot<std::uint32_t(__fastcall*)(void*, void*)>(self, 0xac)(self, nullptr);
        }
    }
    const int rel = sseRel(0, ld(self + 4));
    const std::uint32_t flag = (eax & 0xffffff00u) | ((rel == 0 || rel == 1) ? 1u : 0u);
    return g_calls.stanceFlag(reinterpret_cast<void*>(owner), nullptr, flag);
}

std::uint32_t SwitchLocomotorsSpecialAbilityUpdate_latch(std::uint8_t* self) noexcept {
    g_calls.specialReady(self, nullptr);
    const std::uint32_t eax = ld(self + 4);
    if (ld8(eax + 0x101) == 0) {
        return eax;
    }
    auto* object = reinterpret_cast<std::uint8_t*>(ld(self + 8));
    const std::uint32_t flags = ld(object + 0x88);
    st(object + 0x88, flags | 0x40000000u);
    const std::uint8_t al = static_cast<std::uint8_t>(~static_cast<std::uint8_t>(flags >> 30)) & 1u;
    if (al == 0) {
        return 0;
    }
    return g_calls.setStatus(object, nullptr, 0x5e, 1);
}

std::uint32_t BehaviorModule_clearMotion(std::uint8_t* self, std::uint32_t) noexcept {
    const std::uint32_t gate = ld(self - 0x20);
    if (ld8(gate + 0x6d) != 0) {
        return gate;
    }
    st(self + 4, 0);
    st(self + 8, 0);
    st(self + 0x0c, 0);
    st(self + 0x10, 0);
    auto* object = reinterpret_cast<std::uint8_t*>(ld(self - 0x1c));
    const std::uint32_t flags = ld(object + 0x80);
    st(object + 0x80, flags & 0xfffffff7u);
    if (((flags >> 3) & 1u) != 0) {
        g_calls.setStatus(object, nullptr, 3, 0);
    }
    return g_calls.clearSlot(reinterpret_cast<void*>(ld(self - 0x1c)), nullptr, 3);
}

std::uint32_t WeaponReadyState_poll(std::uint8_t* self) noexcept {
    g_calls.enterState(self, nullptr, 0);
    auto* weapon = reinterpret_cast<std::uint8_t*>(ld(self + 0x20));
    std::uint32_t queried = 0;
    if (weapon != nullptr && ld(weapon + 0x3c) != 0) {
        const std::uint32_t gate = ld(ld(self + 0x18) + 0x24);
        if (ld(weapon + 0x5c) == 0) {
            queried = g_calls.weaponQuery(weapon, nullptr);
        }
        if ((ld8(gate + 0x9b) & 1) == 0) {
            bool keep = false;
            if (queried != 0) {
                keep = ((ld(queried + 0x94) >> 14) & 1u) != 0;
            }
            if (!keep) {
                st(weapon + 0x3c, 0);
            }
        }
    }
    const std::uint8_t al = ld8(ld(self + 0x20) + 0x60);
    return al == 0 ? 0u : 0xffffffffu;
}

std::uint32_t WeaponFireState_enter(std::uint8_t* self, std::uint32_t) noexcept {
    st(self + 0x28, 0);
    auto* holder = reinterpret_cast<std::uint8_t*>(ld(ld(self + 0x18) + 0x24));
    const std::uint32_t flags = ld(holder + 0x80);
    st(holder + 0x80, flags & 0xffffbfffu);
    if (((flags >> 14) & 1u) != 0) {
        g_calls.setStatus(holder, nullptr, 0x0e, 0);
    }
    auto* weapon = reinterpret_cast<std::uint8_t*>(ld(self + 0x20));
    const std::uint32_t kind = ld(ld(ld(weapon + 4) + 4) + 0xb4);
    if (kind == 5 && ld(weapon + 0x5c) == 0) {
        const std::uint32_t found = g_calls.weaponQuery(weapon, nullptr);
        if (found != 0) {
            const std::uint32_t slot = ld(ld(self + 0x18) + 0x24);
            g_calls.weaponLink(weapon, nullptr, slot, found);
        }
    }
    if (static_cast<std::int32_t>(ld(ld(self + 0x20) + 0x24)) <= 0) {
        st8(ld(self + 0x20) + 0x60, 0);
    }
    return g_calls.enterState(self, nullptr, 3);
}

std::uint32_t WeaponOutOfAmmoState_enter(std::uint8_t* self) noexcept {
    auto* weapon = reinterpret_cast<std::uint8_t*>(ld(self + 0x20));
    const std::uint32_t found = g_calls.weaponQuery(weapon, nullptr);
    if (found != 0) {
        const std::uint32_t kind = ld(ld(ld(weapon + 4) + 4) + 0xb4);
        if (kind == 2) {
            const std::uint32_t slot = ld(ld(self + 0x18) + 0x24);
            g_calls.weaponLink(weapon, nullptr, slot, found);
        }
    }
    const std::uint32_t templ = ld(ld(weapon + 4) + 4);
    if (static_cast<std::int32_t>(ld(templ + 0x70)) <= 0) {
        std::uint32_t value = ld(templ + 0x70);
        const std::uint32_t previous = ld(weapon + 0x1c);
        st(weapon + 0x1c, value);
        if (value == 0) {
            st(weapon + 0x1c, 0x7fffffffu);
        }
        if (ld(weapon + 0x1c) > previous) {
            g_calls.ammoNotify(weapon, nullptr);
        }
        return 0xfffffffeu;
    }
    if (ld(weapon + 0x1c) > 0) {
        return 0xfffffffeu;
    }
    if (ld(templ + 0xb8) == 0) {
        return 0xffffffffu;
    }
    g_calls.enterState(self, nullptr, 4);
    return vslot<std::uint32_t(__fastcall*)(void*, void*)>(self, 0x14)(self, nullptr);
}

std::uint32_t ProductionUpdate_doorCode(std::uint8_t* self, std::uint32_t arg) noexcept {
    const std::uint32_t index = ld(self + 0x1c);
    if (index >= ld(ld(self - 0x20) + 8)) {
        return 4;
    }
    auto* object = reinterpret_cast<std::uint8_t*>(ld(self - 0x1c));
    if (((ld(object + 0x88) >> 1) & 1u) != 0 || ((ld(object + 0x94) >> 2) & 1u) != 0) {
        return 3;
    }
    const std::uint32_t alt = ld(object + 0x418);
    if ((g_calls.doorTest(object, nullptr, arg) & 0xff) != 0) {
        return 0x0e;
    }
    if (alt != 0 && (g_calls.doorAlt(reinterpret_cast<void*>(alt), nullptr, arg) & 0xff) != 0) {
        return 0x0e;
    }
    return 0;
}

std::uint32_t ProductionUpdate_scanOrPush(std::uint8_t* self, std::uint32_t record, std::uint32_t flag) noexcept {
    if ((flag & 0xff) == 0) {
        auto* vec = self + 0xf8;
        const std::uint32_t end = ld(self + 0xfc);
        const std::uint32_t source = record + 4;
        if (end >= ld(vec + 8)) {
            return g_calls.push8(vec, nullptr, end, source);
        }
        st(vec + 4, end + 8);
        if (end == 0) {
            return 0;
        }
        st(reinterpret_cast<void*>(end), ld(source));
        st(reinterpret_cast<void*>(end + 4), ld(source + 4));
        return end;
    }
    std::uint32_t eax = 0;
    for (std::uint32_t node = ld(self + 0x10); node != 0;) {
        const std::uint32_t next = ld(node + 0x54);
        if (ld8(node + 0x65) == 0 && sseRel(g_env.prodLimitBits, ld(node + 0x1c)) == 1 &&
            ld(node + 0x0c) == 1) {
            eax = g_calls.doorScan(reinterpret_cast<void*>(record), nullptr, ld(node + 0x10));
            if ((eax & 0xff) != 0) {
                eax = vslot<std::uint32_t(__fastcall*)(void*, void*, std::uint32_t)>(self, 0x2c)(
                    self, nullptr, node);
                node = next;
                continue;
            }
        }
        node = next;
    }
    return eax;
}

std::uint32_t BoobyTrapUpdate_capture(std::uint8_t* self, std::uint32_t) noexcept {
    if (ld8(self + 0x54) != 0) {
        return 0;
    }
    const std::uint32_t templ = ld(self + 4);
    if (((ld(templ + 0x104) >> 4) & 1u) == 0) {
        return templ;
    }
    auto* object = reinterpret_cast<std::uint8_t*>(ld(self + 8));
    std::uint32_t coords[3] = {ld(object + 0x38), ld(object + 0x3c), ld(object + 0x40)};
    const std::uint32_t projected = g_calls.projectCoord(reinterpret_cast<std::uint32_t>(coords));
    const bool moved = !sseEqual(coords[0], ld(object + 0x38)) || !sseEqual(coords[1], ld(object + 0x3c)) ||
                       !sseEqual(coords[2], ld(object + 0x40));
    std::uint32_t eax = projected;
    if (moved) {
        eax = g_calls.setPosition(object, nullptr, reinterpret_cast<std::uint32_t>(coords), 0);
    } else {
        // ucomiss equal; lahf leaves AH = 0x42 (ZF and the reserved bit).
        eax = (eax & 0xffff00ffu) | 0x4200u;
    }
    st8(self + 0x54, 1);
    return eax;
}

std::uint32_t MagneticEffectUpdate_onBit(std::uint8_t* self, std::uint32_t bit) noexcept {
    if (ld8(self + 0x45) == 0) {
        return 0;
    }
    std::uint32_t scratch = 0;
    const auto bits = vslot<std::uint32_t(__fastcall*)(void*, void*, std::uint32_t)>(self - 0x18, 8)(
        self - 0x18, nullptr, reinterpret_cast<std::uint32_t>(&scratch));
    const std::uint32_t mask = 1u << (bit & 31u);
    if ((ld(reinterpret_cast<void*>(static_cast<std::uintptr_t>(bits) + (bit >> 5) * 4)) & mask) == 0) {
        return bits;
    }
    const std::uint32_t eax = g_calls.effectOn(reinterpret_cast<void*>(ld(self - 0x20)), nullptr);
    if ((eax & 0xff) != 0) {
        return eax;
    }
    return g_calls.stopEffect(self - 0x28, nullptr);
}

std::uint32_t HordeContain_refresh(std::uint8_t* self) noexcept {
    vslot<std::uint32_t(__fastcall*)(void*, void*, std::uint32_t)>(self, 0xb4)(self, nullptr, 0);
    g_calls.hordeBase(self, nullptr);
    auto* sub = self + 0x16c;
    const std::uint32_t raw = vslot<std::uint32_t(__fastcall*)(void*, void*)>(sub, 0xf4)(sub, nullptr);
    const std::uint32_t want = boolFromAl(raw);
    if (ld8(self + 0x291) != want) {
        return vslot<std::uint32_t(__fastcall*)(void*, void*)>(sub, 0x60)(sub, nullptr);
    }
    return want;
}

std::uint32_t HordeContain_collectIds(std::uint8_t* self, std::uint8_t* out) noexcept {
    auto* parent = reinterpret_cast<std::uint8_t*>(ld(self - 0x168));
    const std::int32_t groups = static_cast<std::int32_t>(ld(parent + 0x128));
    if (groups > 0) {
        auto* rec = reinterpret_cast<std::uint8_t*>(ld(parent + 0x12c));
        auto* end = rec + static_cast<std::uint32_t>(groups) * 8u;
        for (; rec != end; rec += 8) {
            const std::int32_t copies = static_cast<std::int32_t>(ld(rec + 4));
            for (std::int32_t i = 0; i < copies; ++i) {
                pushRecord(out, reinterpret_cast<std::uint32_t>(rec));
            }
        }
    }
    const std::int32_t count = (static_cast<std::int32_t>(ld(out + 4)) - static_cast<std::int32_t>(ld(out))) >> 2;
    const std::uint32_t bits = static_cast<std::uint32_t>(count);
    return (bits & 0xffffff00u) | (count != 0 ? 1u : 0u);
}

std::uint32_t GhostObject_linkWatcher(std::uint8_t* self) noexcept {
    auto* sentinel = reinterpret_cast<std::uint8_t*>(g_env.ghostRoot + 0x14);
    const std::uint32_t node = g_calls.allocNode(0x0c, 0x00bc8e74u, 0, 0, 0x00bc8e80u, 0xd1);
    st(node + 8, reinterpret_cast<std::uint32_t>(self - 4));
    st(node, reinterpret_cast<std::uint32_t>(sentinel));
    const std::uint32_t prev = ld(sentinel + 4);
    st(node + 4, prev);
    st(prev, node);
    st(sentinel + 4, node);
    st(self + 0x18, 0);
    return node;
}

std::uint32_t SpecialDisguiseUpdate_seed(std::uint8_t*, std::uint32_t arg) noexcept {
    g_calls.seedDisguise(reinterpret_cast<void*>(arg), nullptr, 1, 4, 2, 5);
    return arg;
}

std::uint32_t Object_maskedFind(std::uint8_t* self) noexcept {
    const std::uint32_t object = ld(self - 0x60);
    if (((ld(object + 0xc0) >> 4) & 1u) == 0) {
        return 0;
    }
    const std::uint32_t found = g_calls.ghostFind(reinterpret_cast<void*>(g_env.ghostRoot), nullptr,
                                                  reinterpret_cast<std::uint32_t>(self - 0x64));
    if (found == 0) {
        return 0;
    }
    return found + 4;
}

std::uint32_t RecallUnitsSpecialPower_filter(std::uint8_t* self, std::uint32_t a0, std::uint32_t a1,
                                             std::uint32_t) noexcept {
    const std::uint32_t eax = g_calls.recallTest(self, nullptr, a0, a1, g_env.recallBits);
    if ((eax & 0xff) == 0) {
        return eax;
    }
    const std::uint32_t object = ld(self - 8);
    const std::uint32_t list = ld(object + 0x418);
    const std::uint32_t counted = g_calls.recallScan(reinterpret_cast<void*>(list), nullptr, 0x00711ed0u, object);
    return (counted & 0xffffff00u) | (counted != 1u ? 1u : 0u);
}

std::uint32_t GiveOrRestoreUpgradeSpecialPower_try(std::uint8_t* self, std::uint32_t target,
                                                   std::uint32_t bits) noexcept {
    const std::uint32_t eax = g_calls.upgradeCheck(self, nullptr, target, bits);
    if ((eax & 0xff) == 0) {
        return eax;
    }
    std::uint32_t use = target;
    if (use == 0) {
        use = ld(self - 0x1c) + 0x38;
    }
    const std::uint32_t key = ld(ld(self - 0x20) + 0xf8);
    const std::uint32_t applied = g_calls.upgradeApply(self - 0x24, nullptr, key, use);
    return boolFromAl(applied);
}

std::uint32_t BridgeBuilderBehavior_xfer(std::uint8_t* self, std::uint8_t* xfer) noexcept {
    std::uint8_t version[2];
    xferVersion(xfer, 1, 1, version);
    const std::uint32_t text = vslot<std::uint32_t(__fastcall*)(void*, void*, std::uint32_t, std::uint32_t)>(
        xfer, 0x58)(xfer, nullptr, reinterpret_cast<std::uint32_t>(self + 0x28),
                    reinterpret_cast<std::uint32_t>(self + 0x3c));
    std::uint32_t eax = g_calls.assignField(text, reinterpret_cast<std::uint32_t>(self + 0x3c));
    if (((ld(xfer + 4) >> 1) & 1u) == 0) {
        return eax;
    }
    g_calls.bridgeCount(self, nullptr);
    eax = g_calls.bridgePlace(reinterpret_cast<void*>(g_env.bridgeWorld), nullptr,
                              reinterpret_cast<std::uint32_t>(self + 0x28));
    st(self + 0x4c, eax);
    return eax;
}

std::uint32_t EncloseBehavior_xfer(std::uint8_t* self, std::uint8_t* xfer) noexcept {
    std::uint8_t first[2];
    std::uint8_t second[2];
    xferVersion(xfer, 1, 3, first);
    xferVersion(xfer, 1, 1, second);
    g_calls.xferBase(self, nullptr, reinterpret_cast<std::uint32_t>(xfer));
    std::uint32_t eax = g_calls.xferAt10(self + 0x10, nullptr, reinterpret_cast<std::uint32_t>(xfer));
    if (first[1] < 3) {
        return eax;
    }
    return g_calls.assignField(reinterpret_cast<std::uint32_t>(xfer), reinterpret_cast<std::uint32_t>(self + 0x30));
}

std::uint32_t ShieldSphereUpdate_xfer(std::uint8_t* self, std::uint8_t* xfer) noexcept {
    std::uint8_t first[2];
    xferVersion(xfer, 1, 1, first);
    g_calls.xferBase(self, nullptr, reinterpret_cast<std::uint32_t>(xfer));
    g_calls.xferAt10(self + 0x10, nullptr, reinterpret_cast<std::uint32_t>(xfer));
    const std::uint32_t flags = ld(xfer + 4);
    if ((flags >> 3) & 1u) {
        return flags >> 3;
    }
    std::uint8_t second[2];
    xferVersion(xfer, 1, 2, second);
    auto xferDword = vslot<std::uint32_t(__fastcall*)(void*, void*, std::uint32_t)>(xfer, 0x64);
    xferDword(xfer, nullptr, reinterpret_cast<std::uint32_t>(self + 0x48));
    xferDword(xfer, nullptr, reinterpret_cast<std::uint32_t>(self + 0x4c));
    xferDword(xfer, nullptr, reinterpret_cast<std::uint32_t>(self + 0x50));
    xferDword(xfer, nullptr, reinterpret_cast<std::uint32_t>(self + 0x54));
    auto xferWide = vslot<std::uint32_t(__fastcall*)(void*, void*, std::uint32_t)>(xfer, 0x6c);
    xferWide(xfer, nullptr, reinterpret_cast<std::uint32_t>(self + 0x58));
    xferWide(xfer, nullptr, reinterpret_cast<std::uint32_t>(self + 0x5c));
    if (second[1] >= 2) {
        vslot<std::uint32_t(__fastcall*)(void*, void*, std::uint32_t)>(xfer, 0x84)(
            xfer, nullptr, reinterpret_cast<std::uint32_t>(self + 0x68));
    }
    return g_calls.xferTail30(reinterpret_cast<std::uint32_t>(xfer), reinterpret_cast<std::uint32_t>(self + 0x2c));
}

std::uint32_t BombardAreaUpdate_xfer(std::uint8_t* self, std::uint8_t* xfer) noexcept {
    std::uint8_t first[2];
    std::uint8_t second[2];
    xferVersion(xfer, 1, 1, first);
    xferVersion(xfer, 1, 1, second);
    g_calls.xferBase(self, nullptr, reinterpret_cast<std::uint32_t>(xfer));
    g_calls.xferAt10(self + 0x10, nullptr, reinterpret_cast<std::uint32_t>(xfer));
    if ((ld(xfer + 4) >> 1) & 1u) {
        g_calls.releasePair(self + 0x2c, nullptr, ld(self + 0x2c), ld(self + 0x30));
    }
    vslot<std::uint32_t(__fastcall*)(void*, void*, std::uint32_t)>(xfer, 0x70)(
        xfer, nullptr, reinterpret_cast<std::uint32_t>(self + 0x3c));
    return g_calls.xferTail2c(reinterpret_cast<std::uint32_t>(xfer), reinterpret_cast<std::uint32_t>(self + 0x2c));
}

std::uint32_t MagneticEffectUpdate_xfer(std::uint8_t* self, std::uint8_t* xfer) noexcept {
    std::uint8_t first[2];
    std::uint8_t second[2];
    xferVersion(xfer, 1, 3, first);
    xferVersion(xfer, 1, 1, second);
    g_calls.xferBase(self, nullptr, reinterpret_cast<std::uint32_t>(xfer));
    g_calls.xferAt10(self + 0x10, nullptr, reinterpret_cast<std::uint32_t>(xfer));
    std::uint32_t eax = g_calls.xferBlock(reinterpret_cast<std::uint32_t>(xfer),
                                          reinterpret_cast<std::uint32_t>(self + 0x4c));
    if (first[1] < 3) {
        return eax;
    }
    vslot<std::uint32_t(__fastcall*)(void*, void*, std::uint32_t)>(xfer, 0x84)(
        xfer, nullptr, reinterpret_cast<std::uint32_t>(self + 0x6d));
    return g_calls.xferTail30(reinterpret_cast<std::uint32_t>(xfer), reinterpret_cast<std::uint32_t>(self + 0x30));
}

std::uint32_t BeamTargetModule_xfer(std::uint8_t* self, std::uint8_t* xfer) noexcept {
    std::uint8_t first[2];
    std::uint8_t second[2];
    xferVersion(xfer, 1, 1, first);
    xferVersion(xfer, 1, 1, second);
    g_calls.xferBase(self, nullptr, reinterpret_cast<std::uint32_t>(xfer));
    g_calls.assignField(reinterpret_cast<std::uint32_t>(xfer), reinterpret_cast<std::uint32_t>(self + 0x14));
    g_calls.xferPair(reinterpret_cast<std::uint32_t>(xfer), reinterpret_cast<std::uint32_t>(self + 0x24));
    vslot<std::uint32_t(__fastcall*)(void*, void*, std::uint32_t)>(xfer, 0x84)(
        xfer, nullptr, reinterpret_cast<std::uint32_t>(self + 0x2c));
    g_calls.xferPair(reinterpret_cast<std::uint32_t>(xfer), reinterpret_cast<std::uint32_t>(self + 0x28));
    return g_calls.xferPair(reinterpret_cast<std::uint32_t>(xfer), reinterpret_cast<std::uint32_t>(self + 0x30));
}

std::uint32_t LiftObjectUpdate_xfer(std::uint8_t* self, std::uint8_t* xfer) noexcept {
    std::uint8_t first[2];
    std::uint8_t second[2];
    xferVersion(xfer, 1, 1, first);
    xferVersion(xfer, 1, 1, second);
    g_calls.xferBase(self, nullptr, reinterpret_cast<std::uint32_t>(xfer));
    g_calls.xferAt10(self + 0x10, nullptr, reinterpret_cast<std::uint32_t>(xfer));
    const std::uint32_t flags = ld(xfer + 4);
    if ((flags >> 3) & 1u) {
        return flags >> 3;
    }
    auto* head = self + 0x2c;
    std::uint32_t count = 0;
    for (std::uint32_t node = ld(head); node != reinterpret_cast<std::uint32_t>(head); node = ld(node)) {
        ++count;
    }
    std::uint32_t eax = vslot<std::uint32_t(__fastcall*)(void*, void*, std::uint32_t)>(xfer, 0x6c)(
        xfer, nullptr, reinterpret_cast<std::uint32_t>(&count));
    if ((ld(xfer + 4) >> 1) & 1u) {
        const std::uint32_t prepared = g_calls.liftPrepare(head, nullptr);
        eax = g_calls.liftAttach(head, nullptr, count, prepared);
    }
    for (std::uint32_t node = ld(head); node != reinterpret_cast<std::uint32_t>(head); node = ld(node)) {
        eax = g_calls.xferNode(reinterpret_cast<void*>(node + 8), nullptr, reinterpret_cast<std::uint32_t>(xfer));
    }
    return eax;
}

std::uint32_t EmotionTrackerUpdate_release(std::uint8_t* self) noexcept {
    const std::uint32_t held = ld(self + 0x78);
    if (held != 0) {
        g_calls.releaseOne(reinterpret_cast<void*>(held), nullptr);
        st(self + 0x78, 0);
    }
    const std::uint32_t end = ld(self + 0x6c);
    for (std::uint32_t cur = ld(self + 0x68); cur != end; cur += 4) {
        const std::uint32_t item = ld(cur);
        if (item != 0) {
            g_calls.itemDtor(reinterpret_cast<void*>(item), nullptr);
            g_calls.freePtr(item);
        }
    }
    const std::uint32_t begin = ld(self + 0x68);
    const std::uint32_t span = ld(self + 0x6c);
    const std::uint32_t eax = g_calls.moveBytes(begin, span, 0);
    std::int32_t esi = static_cast<std::int32_t>(span) - static_cast<std::int32_t>(begin);
    esi >>= 2;
    esi = -esi;
    esi += esi;
    esi += esi;
    st(self + 0x6c, ld(self + 0x6c) + static_cast<std::uint32_t>(esi));
    return eax;
}

std::uint32_t NotifyWhenAttackingBehavior_clearIds(std::uint8_t* self, std::uint32_t) noexcept {
    const std::uint32_t begin = ld(self + 8);
    const std::uint32_t end = ld(self + 0x0c);
    const std::uint32_t eax = g_calls.moveBytes(begin, end, 0);
    std::int32_t esi = static_cast<std::int32_t>(end) - static_cast<std::int32_t>(begin);
    esi >>= 2;
    esi = -esi;
    esi += esi;
    esi += esi;
    st(self + 0x0c, ld(self + 0x0c) + static_cast<std::uint32_t>(esi));
    return eax;
}

std::uint32_t SpecialPowerModule_trigger(std::uint8_t* self, std::uint8_t* object) noexcept {
    auto* data = reinterpret_cast<std::uint8_t*>(ld(self + 4));
    const std::uint32_t subject = ld(object + 0x3cc);
    if (subject != 0) {
        std::int32_t left = static_cast<std::int32_t>(ld(data + 0x38));
        while (left > 0) {
            const std::uint32_t has = g_calls.playerHas(reinterpret_cast<void*>(g_env.player), nullptr, subject);
            if ((has & 0xff) == 0) {
                break;
            }
            const std::uint32_t count = g_calls.playerCount(reinterpret_cast<void*>(g_env.player), nullptr, subject, 0);
            if (static_cast<std::int32_t>(count) > 0) {
                __m128 bits = _mm_cvtsi32_ss(_mm_setzero_ps(), static_cast<int>(count));
                std::uint32_t encoded = 0;
                _mm_store_ss(reinterpret_cast<float*>(&encoded), bits);
                g_calls.fireWeapon(reinterpret_cast<void*>(subject), nullptr, encoded, 0, 0, 1, 0);
            }
            --left;
        }
    }
    std::uint32_t eax = ld(data + 0x10);
    if (eax != 0) {
        eax = g_calls.giveTemplate(object, nullptr, eax, 0xffffffffu);
    }
    const std::uint32_t extra = ld(data + 0x18);
    if (extra != 0 && ((ld(ld(object + 4) + 0xcc) >> 10) & 1u) == 0) {
        eax = g_calls.powerReady(reinterpret_cast<void*>(extra), nullptr);
        if ((eax & 0xff) == 0) {
            eax = g_calls.finishPower(reinterpret_cast<void*>(extra), nullptr,
                                      reinterpret_cast<std::uint32_t>(object), 0);
        }
    }
    return eax;
}

std::uint32_t AIUpdateInterface_applyProne(std::uint8_t* self, std::uint8_t* source) noexcept {
    if ((g_env.namekeyReady & 0xffu) == 0) {
        g_env.namekeyReady |= 1u;
        g_env.namekeyValue = g_calls.nameKey(reinterpret_cast<void*>(g_env.namekeyThis), nullptr, 0x00c053b8u);
    }
    const std::uint32_t module = g_calls.findModule(reinterpret_cast<void*>(ld(self + 8)), nullptr, g_env.namekeyValue);
    if (module == 0) {
        return 0;
    }
    const std::int32_t old = static_cast<std::int32_t>(ld(module + 0x24));
    const int truncated = _mm_cvttss_si32(_mm_load_ss(reinterpret_cast<const float*>(source + 0x8c)));
    __m128 scaled = _mm_cvtsi32_ss(_mm_setzero_ps(), truncated);
    scaled = _mm_mul_ss(scaled, _mm_load_ss(reinterpret_cast<const float*>(ld(module + 4) + 8)));
    const __m128 added = _mm_add_ss(scaled, _mm_cvtsi32_ss(_mm_setzero_ps(), old));
    const std::int32_t neu = _mm_cvttss_si32(added);
    st(module + 0x24, static_cast<std::uint32_t>(neu));
    if (old > 0 || neu <= 0) {
        return module;
    }
    return g_calls.proneRefresh(reinterpret_cast<void*>(module), nullptr);
}

std::uint32_t BezierProjectileBehavior_copyAim(std::uint8_t* self, std::uint32_t src, std::uint32_t extra) noexcept {
    const std::uint32_t owner = ld(self - 0x1c);
    if (owner == 0) {
        return 0;
    }
    st(self + 0x40, ld(src));
    st(self + 0x44, ld(src + 4));
    st(self + 0x48, ld(src + 8));
    st(self + 0x34, ld(owner + 0x38));
    st(self + 0x38, ld(owner + 0x3c));
    st(self + 0x3c, ld(owner + 0x40));
    st(self + 0x60, 0);
    return g_calls.bezierTail(self - 0x24, nullptr, 1, extra);
}

std::uint32_t ShieldSphereManager_retarget(std::uint8_t* self) noexcept {
    const std::uint32_t probed = g_calls.sphereProbe(self, nullptr);
    if ((probed & 0xff) != 0) {
        return g_calls.sphereMain(self, nullptr);
    }
    return g_calls.sphereAlt(self, nullptr);
}

}  // namespace ra3::gamelogic
