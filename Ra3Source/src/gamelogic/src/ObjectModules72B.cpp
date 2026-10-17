#include "ra3/gamelogic/ObjectModules72B.h"

#include <cmath>
#include <cstring>

namespace ra3::gamelogic {
namespace {

Object72BCalls g{};
Object72BEnv* env = nullptr;

std::uint32_t loadU32(const void* address) noexcept {
    std::uint32_t value = 0;
    std::memcpy(&value, address, 4);
    return value;
}

void storeU32(void* address, std::uint32_t value) noexcept {
    std::memcpy(address, &value, 4);
}

float loadF(const void* address) noexcept {
    float value = 0.f;
    std::memcpy(&value, address, 4);
    return value;
}

void storeF(void* address, float value) noexcept {
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

std::uint32_t call2u(void* object, int offset, std::uint32_t a, std::uint32_t b) noexcept {
    auto* vt = *reinterpret_cast<std::uint8_t**>(object);
    auto* fn = *reinterpret_cast<std::uint32_t(__fastcall**)(void*, void*, std::uint32_t, std::uint32_t)>(vt + offset);
    return fn(object, nullptr, a, b);
}

std::uint32_t version(void* xfer, std::uint8_t first, std::uint8_t second) noexcept {
    std::uint8_t bytes[2] = {first, second};
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

std::uint32_t div12(std::uint32_t bytes) noexcept {
    auto ns = static_cast<std::int32_t>(bytes);
    const std::int64_t prod = static_cast<std::int64_t>(ns) * static_cast<std::int32_t>(0x2AAAAAABu);
    std::int32_t edx = static_cast<std::int32_t>(prod >> 32);
    edx >>= 1;
    const std::uint32_t sign = static_cast<std::uint32_t>(edx) >> 31;
    return sign + static_cast<std::uint32_t>(edx);
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

std::uint32_t lookupMap(std::uint32_t key) noexcept {
    g.lockMap(reinterpret_cast<void*>(0x00DEBB00), nullptr, 0x00BF605Cu);
    auto* table = reinterpret_cast<std::uint8_t*>(static_cast<std::uintptr_t>(env->mapRoot) + 0xC4);
    std::uint32_t out = 0;
    g.findMap(table, nullptr, &out, &key);
    const std::uint32_t buckets = loadU32(table + 4);
    const std::uint32_t index = loadU32(table + 8);
    const std::uint32_t end = loadU32(reinterpret_cast<void*>(buckets + index * 4));
    std::uint32_t value = 0;
    if (out != end) {
        value = loadU32(reinterpret_cast<void*>(out + 4));
    }
    g.unlockMap(reinterpret_cast<void*>(0x00DEBB00), nullptr, 1);
    return value;
}

std::uint32_t ensureName(std::uint32_t& flag, std::uint32_t& cache, std::uint32_t text) noexcept {
    if ((flag & 1u) == 0) {
        flag |= 1u;
        cache = g.nameKey(reinterpret_cast<void*>(static_cast<std::uintptr_t>(env->nameRegistry)), nullptr, text);
    }
    return cache;
}

void copyLastPoint(std::uint8_t* path, float out[3]) noexcept {
    const std::uint32_t begin = loadU32(path);
    const std::uint32_t end = loadU32(path + 4);
    const std::uint32_t count = div12(end - begin);
    const auto last = begin + count * 12 - 12;
    copy3(out, reinterpret_cast<void*>(last));
}

std::uint32_t followPath(std::uint8_t* self, std::uint8_t* path, std::uint8_t* object, std::uint32_t bits,
                         std::uint32_t mode, std::uint32_t state) noexcept {
    const std::uint32_t usable = g.usable(objOf(self), nullptr);
    if (static_cast<std::uint8_t>(usable) == 0) {
        return usable;
    }
    auto* loco = locoOf(self);
    call0(loco, 0x34);
    const std::uint32_t begin = loadU32(path);
    const std::uint32_t end = loadU32(path + 4);
    if (begin == end) {
        return end;
    }
    float local[3];
    copyLastPoint(path, local);
    g.place(loco, nullptr, local, kLocomotorLimitBits);
    if (follows(mode)) {
        g.followCf(self, nullptr, local);
    }
    g.bindPath(loco, nullptr, path);
    const std::uint32_t id = object == nullptr ? 0u : loadU32(object + 0xDC);
    storeU32(self + 0x50, mode);
    storeU32(self + 0x180, id);
    storeU32(self + 0x1F0, bits);
    return call1u(loco, 0x1C, state);
}

std::int32_t truncTowardZero(float value) noexcept {
    return static_cast<std::int32_t>(value);
}

struct Blend {
    float value;
    std::int32_t chopped;
};

Blend blendScale(std::uint8_t* self) noexcept {
    auto* templ = *reinterpret_cast<std::uint8_t**>(self + 4);
    float unit = 0.f;
    std::memcpy(&unit, &kUnitFloatBits, 4);
    float xmm0 = unit;
    float xmm1 = 0.f;
    float xmm2 = 0.f;
    std::int32_t chopped = 0;
    if (loadU32(self + 0x38) == 2) {
        chopped = truncTowardZero(loadF(templ + 0x24));
        xmm1 = loadF(templ + 0xF0);
        xmm2 = xmm0;
    } else {
        chopped = truncTowardZero(loadF(templ + 0x20));
        xmm2 = loadF(templ + 0xF0);
        xmm1 = xmm0;
    }
    float xmm3 = static_cast<float>(static_cast<std::int32_t>(loadU32(self + 0x30)));
    const float xmm4 = static_cast<float>(chopped);
    xmm3 = xmm3 / xmm4;
    xmm0 = xmm0 - xmm3;
    xmm1 = xmm1 - xmm2;
    xmm0 = xmm0 * xmm1;
    xmm0 = xmm0 + xmm2;
    return {xmm0, chopped};
}

void transformPassenger(const float* v, const float* m, float out[3]) noexcept {
    const float vx = v[0];
    const float vy = v[1];
    const float vz = v[2];
    float r0 = m[0x0C / 4] * vy;
    r0 += m[0x10 / 4] * vz;
    r0 += m[0x08 / 4] * vx;
    r0 += m[0x14 / 4];
    float r1 = m[0x1C / 4] * vy;
    r1 += m[0x20 / 4] * vz;
    r1 += m[0x18 / 4] * vx;
    r1 += m[0x24 / 4];
    float r2 = m[0x2C / 4] * vy;
    r2 += m[0x30 / 4] * vz;
    r2 += m[0x28 / 4] * vx;
    r2 += m[0x34 / 4];
    r0 -= m[0x14 / 4];
    r1 -= m[0x24 / 4];
    r2 -= m[0x34 / 4];
    out[0] = r0;
    out[1] = r1;
    out[2] = r2;
}

}  // namespace

void setObject72BCalls(const Object72BCalls& calls) noexcept {
    g = calls;
}

void setObject72BEnv(Object72BEnv* next) noexcept {
    env = next;
}

std::uint32_t AIUpdateInterface_snapNear(std::uint8_t* self, void* point, std::uint32_t gate,
                                         std::uint32_t mode) noexcept {
    auto* obj = objOf(self);
    auto* origin = obj + 0x38;
    float local[4]{};
    const std::uint32_t near = g.nearCheck(origin, obj, local);
    void* aim = point;
    if (static_cast<std::uint8_t>(near) != 0) {
        float limit = 0.f;
        std::memcpy(&limit, &kSnapLimitBits, 4);
        const float dz = loadF(static_cast<std::uint8_t*>(point) + 8) - loadF(origin + 8);
        const float dy = loadF(static_cast<std::uint8_t*>(point) + 4) - loadF(origin + 4);
        const float dx = loadF(static_cast<std::uint8_t*>(point)) - loadF(origin);
        const float dist = std::sqrt((dz * dz + dy * dy) + dx * dx);
        // fcomip of the 100.0f constant against the distance; jbe keeps the caller point.
        if (!(dist >= limit)) {
            aim = origin;
        }
    }
    auto* pose = *reinterpret_cast<std::uint8_t**>(self + 0x214);
    if (pose != nullptr && obj != nullptr) {
        copy3(pose + 0xC, origin);
    }
    auto* loco = locoOf(self);
    call0(loco, 0x34);
    g.steer(self, nullptr, aim, kLocomotorLimitBits, mode);
    storeU32(self + 0x190, 0);
    self[0x432] = 0;
    storeU32(self + 0x50, mode);
    const std::uint32_t state = gate == 0 ? 0x3Eu : 0x4Bu;
    std::uint32_t eax = call1u(loco, 0x1C, state);
    if (follows(mode)) {
        eax = g.followCf(self, nullptr, aim);
    }
    return eax;
}

std::uint32_t AIUpdateInterface_followPathFlag(std::uint8_t* self, std::uint8_t* path, std::uint8_t* object,
                                               std::uint32_t mode, std::uint32_t flag) noexcept {
    const std::uint32_t usable = g.usable(objOf(self), nullptr);
    if (static_cast<std::uint8_t>(usable) == 0) {
        return usable;
    }
    auto* loco = locoOf(self);
    call0(loco, 0x34);
    const std::uint32_t begin = loadU32(path);
    const std::uint32_t end = loadU32(path + 4);
    const bool flagged = static_cast<std::uint8_t>(flag) != 0;
    if (begin != end) {
        float local[3];
        copyLastPoint(path, local);
        g.place(loco, nullptr, local, kLocomotorLimitBits);
        if (!flagged && follows(mode)) {
            g.followCf(self, nullptr, local);
        }
    }
    g.bindPath(loco, nullptr, path);
    const std::uint32_t id = object == nullptr ? 0u : loadU32(object + 0xDC);
    storeU32(self + 0x50, mode);
    storeU32(self + 0x180, id);
    return call1u(loco, 0x1C, flagged ? 7u : 6u);
}

std::uint32_t AIUpdateInterface_followPath34(std::uint8_t* self, std::uint8_t* path, std::uint8_t* object,
                                             std::uint32_t bits, std::uint32_t mode) noexcept {
    return followPath(self, path, object, bits, mode, 0x34);
}

std::uint32_t AIUpdateInterface_followPath3B(std::uint8_t* self, std::uint8_t* path, std::uint8_t* object,
                                             std::uint32_t bits, std::uint32_t mode) noexcept {
    return followPath(self, path, object, bits, mode, 0x3B);
}

std::uint32_t AIUpdateInterface_orderWeapon0A(std::uint8_t* self, std::uint32_t target, std::uint32_t aux,
                                              std::uint32_t mode) noexcept {
    if (target == 0) {
        return 0;
    }
    auto* obj = objOf(self);
    if ((obj[0x471] & 8u) != 0) {
        return 0;
    }
    std::uint32_t weapon = resolveWeapon(*reinterpret_cast<std::uint8_t**>(obj + 0x37C));
    auto* draw = *reinterpret_cast<std::uint8_t**>(obj + 0x374);
    storeU32(obj + 0x490, 0);
    if (draw != nullptr && loadU32(draw + 0x40) != 0) {
        return static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(draw));
    }
    auto* loco = locoOf(self);
    call0(loco, 0x34);
    if (mode != 2 || loadU32(self + 0x508) != 0) {
        g.markObject(obj, nullptr, target);
    }
    call1u(loco, 0x38, target);
    storeU32(self + 0x50, mode);
    call1u(loco, 0x1C, kOrderState0A);
    if (weapon != 0) {
        storeU32(reinterpret_cast<void*>(weapon + 0x24), aux);
    }
    if (follows(mode)) {
        g.followCd(self, nullptr, target);
    }
    const std::uint32_t key = ensureName(env->nameFlag9960, env->nameCache995c, 0x00C08E50u);
    std::uint32_t eax = g.findModule(obj, nullptr, key);
    if (eax != 0) {
        eax = g.pokeModule(reinterpret_cast<void*>(eax), nullptr);
    }
    return eax;
}

std::uint32_t AIUpdateInterface_buildList16(std::uint8_t* self, std::uint32_t begin, std::uint32_t end,
                                            std::uint32_t, std::uint32_t, std::uint32_t aux,
                                            std::uint32_t mode) noexcept {
    std::uint32_t created = g.allocBytes(0x30);
    if (created != 0) {
        created = g.ctorList(reinterpret_cast<void*>(created), nullptr);
    }
    for (std::uint32_t cursor = begin; cursor != end; cursor += 4) {
        g.pushItem(reinterpret_cast<void*>(created), nullptr, loadU32(reinterpret_cast<void*>(cursor)));
    }
    auto* loco = locoOf(self);
    call0(loco, 0x34);
    g.setList(loco, nullptr, reinterpret_cast<void*>(created));
    storeU32(self + 0x50, mode);
    call1u(loco, 0x1C, 0x16);
    if (created != 0) {
        call1u(reinterpret_cast<void*>(created), 0, 1);
    }
    auto* obj = objOf(self);
    auto* set = *reinterpret_cast<std::uint8_t**>(obj + 0x37C);
    if (set != nullptr) {
        bool stored = false;
        const std::uint32_t wEnd = loadU32(set + 0x28);
        for (std::uint32_t cursor = loadU32(set + 0x24); cursor != wEnd; cursor += 4) {
            auto* weapon = *reinterpret_cast<std::uint8_t**>(reinterpret_cast<void*>(cursor));
            auto* sub = *reinterpret_cast<std::uint8_t**>(weapon + 0xC);
            const bool terminal = loadU32(sub + 4) == 0xFFFFFFFFu;
            const std::uint32_t hit = call0(weapon, 0x24);
            if (hit != 0) {
                storeU32(reinterpret_cast<void*>(hit + 0x24), aux);
                stored = true;
                break;
            }
            if (terminal) {
                break;
            }
        }
        if (!stored) {
            const std::uint32_t nEnd = loadU32(set + 0x28);
            for (std::uint32_t cursor = loadU32(set + 0x24); cursor != nEnd; cursor += 4) {
                auto* weapon = *reinterpret_cast<std::uint8_t**>(reinterpret_cast<void*>(cursor));
                auto* sub = *reinterpret_cast<std::uint8_t**>(weapon + 0xC);
                const bool terminal = loadU32(sub + 4) == 0xFFFFFFFFu;
                const std::uint32_t hit = call1u(weapon, 0x20, 0);
                if (hit != 0) {
                    storeU32(reinterpret_cast<void*>(hit + 0x24), aux);
                    break;
                }
                if (terminal) {
                    break;
                }
            }
        }
    }
    const std::uint32_t queried = g.queryLoco(loco, nullptr);
    if (queried != 0 && follows(mode)) {
        g.followCd(self, nullptr, queried);
    }
    if (begin != 0) {
        return g.freePtr(reinterpret_cast<void*>(begin));
    }
    return 0;
}

std::uint32_t AIUpdateInterface_steerFloat21(std::uint8_t* self, void* point, std::uint32_t bits,
                                             std::uint32_t aux, std::uint32_t mode) noexcept {
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
    g.steer(self, nullptr, point, bits, mode);
    storeU32(self + 0x50, mode);
    call1u(loco, 0x1C, 0x21);
    auto* set = *reinterpret_cast<std::uint8_t**>(obj + 0x37C);
    std::uint32_t weapon = resolveWeapon(set);
    if (weapon != 0) {
        storeU32(reinterpret_cast<void*>(weapon + 0x24), aux);
    }
    if (follows(mode)) {
        g.followCe(self, nullptr, point);
    }
    const std::uint32_t scriptObj = loadU32(obj + 0x418);
    if (scriptObj != 0) {
        return g.scriptCall(reinterpret_cast<void*>(static_cast<std::uintptr_t>(env->scriptEngine)), nullptr,
                            scriptObj, 0);
    }
    return 0;
}

std::uint32_t AIUpdateInterface_placeTarget21(std::uint8_t* self, std::uint8_t* target,
                                              std::uint32_t mode) noexcept {
    if (target == nullptr || self[0x436] != 0) {
        return 0;
    }
    auto* obj = objOf(self);
    const std::uint32_t usable = g.usable(obj, nullptr);
    if (static_cast<std::uint8_t>(usable) == 0) {
        return usable;
    }
    auto* pose = *reinterpret_cast<std::uint8_t**>(self + 0x214);
    if (pose != nullptr) {
        g.notePose(pose, nullptr, obj);
    }
    auto* loco = locoOf(self);
    call0(loco, 0x34);
    g.place(loco, nullptr, target + 0x38, kLocomotorLimitBits);
    storeU32(self + 0x190, 0);
    self[0x432] = 0;
    storeU32(self + 0x50, mode);
    call1u(loco, 0x1C, 0x21);
    if (follows(mode)) {
        g.followCe(self, nullptr, target + 0x38);
    }
    const std::uint32_t scriptObj = loadU32(obj + 0x418);
    if (scriptObj != 0) {
        return g.scriptCall(reinterpret_cast<void*>(static_cast<std::uintptr_t>(env->scriptEngine)), nullptr,
                            scriptObj, 0);
    }
    return 0;
}

std::uint32_t AIUpdateInterface_attackOrState26(std::uint8_t* self, std::uint32_t target,
                                                std::uint32_t mode) noexcept {
    auto* obj = objOf(self);
    if (target == 0) {
        target = g.currentTarget(obj, nullptr);
        if (target == 0) {
            return target;
        }
    }
    std::uint8_t* chain = *reinterpret_cast<std::uint8_t**>(obj + 0x328);
    if (chain == nullptr) {
        const std::uint32_t begin = loadU32(obj + 0x32C);
        const std::uint32_t end = loadU32(obj + 0x330);
        if (begin == end) {
            chain = nullptr;
        } else {
            chain = *reinterpret_cast<std::uint8_t**>(reinterpret_cast<void*>(begin));
        }
    }
    if (chain != nullptr) {
        const std::uint32_t handler = call0(chain, 0x84);
        if (handler != 0) {
            return call2u(reinterpret_cast<void*>(handler), 0x84, target, mode);
        }
    }
    const std::uint32_t key = ensureName(env->nameFlag9968, env->nameCache9964, 0x00C07C88u);
    const std::uint32_t module = g.findModule(reinterpret_cast<void*>(target), nullptr, key);
    if (module != 0) {
        g.disguiseSlot(reinterpret_cast<void*>(module), nullptr, 0);
    }
    auto* loco = locoOf(self);
    call0(loco, 0x34);
    call1u(loco, 0x38, target);
    storeU32(self + 0x50, mode);
    call1u(loco, 0x1C, 0x26);
    storeU32(self + 0x308, 0xFFFFFFFFu);
    storeU32(self + 0x368, 0xFFFFFFFFu);
    storeU32(self + 0x3C8, 0xFFFFFFFFu);
    return 0xFFFFFFFFu;
}

std::uint32_t EncloseBehavior_tryApplyKey(std::uint8_t* self, std::uint32_t arg) noexcept {
    auto* object = *reinterpret_cast<std::uint8_t**>(self - 0xC);
    const std::uint32_t pred = g.encloseTest(reinterpret_cast<void*>(arg), nullptr, object + 0xC);
    if (static_cast<std::uint8_t>(pred) == 0 || self[4] == 0) {
        return pred & 0xFFFFFF00u;
    }
    auto* keyObj = *reinterpret_cast<std::uint8_t**>(self - 8);
    object = *reinterpret_cast<std::uint8_t**>(self - 0xC);
    if (keyObj == nullptr) {
        return (pred & 0xFFFFFF00u) | 1u;
    }
    auto* slot = *reinterpret_cast<std::uint8_t**>(object + 8);
    if (slot == nullptr || loadU32(slot) == 0xFFFFFFFFu) {
        return (pred & 0xFFFFFF00u) | 1u;
    }
    const std::uint32_t applied = g.applyKey(keyObj, nullptr, loadU32(slot));
    return (applied & 0xFFFFFF00u) | 1u;
}

std::uint32_t CaptureSpecialAbilityUpdate_releaseHeld(std::uint8_t* self, std::uint32_t a,
                                                      std::uint32_t b) noexcept {
    const std::uint32_t key = loadU32(self + 0x4C);
    std::uint32_t found = 0;
    if (key != 0) {
        found = lookupMap(key);
        if (found != 0) {
            g.floatCall(self, nullptr, kUnitFloatBits, found);
            storeU32(reinterpret_cast<void*>(found + 0xEC), 0);
            storeU32(self + 0x4C, 0);
        }
    }
    return g.xferBase(self, nullptr, a, b);
}

std::uint32_t SpecialDisguiseUpdate_blendScale(std::uint8_t* self) noexcept {
    const std::uint32_t ready = g.ready(self, nullptr);
    if (static_cast<std::uint8_t>(ready) == 0) {
        return ready;
    }
    const Blend scale = blendScale(self);
    storeF(self + 0xBC, scale.value);
    return (static_cast<std::uint32_t>(scale.chopped) & 0xFFFFFF00u) | (ready & 0xFFu);
}

std::uint32_t ToggleMountedSpecialAbilityUpdate_blendMounted(std::uint8_t* self) noexcept {
    const std::uint32_t ready = g.ready(self, nullptr);
    if (static_cast<std::uint8_t>(ready) == 0) {
        return ready;
    }
    const Blend scale = blendScale(self);
    auto* mounted = *reinterpret_cast<std::uint8_t**>(objOf(self) + 0xF4);
    if (mounted == nullptr) {
        return ready & 0xFFu;
    }
    storeF(mounted + 0x1BC, scale.value);
    return (static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(mounted)) & 0xFFFFFF00u) | (ready & 0xFFu);
}

std::uint32_t HordeAIUpdate_forwardMove(std::uint8_t* self, std::uint32_t a, std::uint32_t b,
                                        std::uint32_t bits, std::uint32_t mode) noexcept {
    auto* obj = objOf(self);
    auto* chain = *reinterpret_cast<std::uint8_t**>(obj + 0x328);
    if (chain == nullptr) {
        const std::uint32_t begin = loadU32(obj + 0x32C);
        if (begin == loadU32(obj + 0x330)) {
            return static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(obj));
        }
        chain = *reinterpret_cast<std::uint8_t**>(reinterpret_cast<void*>(begin));
        if (chain == nullptr) {
            return static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(obj));
        }
    }
    chain = *reinterpret_cast<std::uint8_t**>(obj + 0x328);
    if (chain == nullptr) {
        const std::uint32_t begin = loadU32(obj + 0x32C);
        if (begin == loadU32(obj + 0x330)) {
            chain = nullptr;
        } else {
            chain = *reinterpret_cast<std::uint8_t**>(reinterpret_cast<void*>(begin));
        }
    }
    if (chain == nullptr || call0(chain, 0x84) == 0) {
        return 0;
    }
    return g.moveVec(self, nullptr, a, b, bits, mode);
}

std::uint32_t CompositeStructureInfoModule_applyRecords(std::uint8_t* self) noexcept {
    std::uint32_t eax = g.beginRecords(self, nullptr);
    const std::uint32_t end = loadU32(self + 0x1C);
    for (std::uint32_t cursor = loadU32(self + 0x18); cursor != end; cursor += 4) {
        const std::uint32_t id = loadU32(reinterpret_cast<void*>(cursor));
        std::uint32_t value = 0;
        if (id != 0) {
            value = lookupMap(id);
        }
        eax = g.applyRecord(self, nullptr, value);
    }
    return eax;
}

std::uint32_t CaptureSpecialAbilityUpdate_onTrigger(std::uint8_t* self, std::uint32_t flag) noexcept {
    const std::uint32_t eax = g.trigger(self, nullptr, flag);
    if (static_cast<std::uint8_t>(flag) == 0) {
        return eax;
    }
    const std::uint32_t held = loadU32(objOf(self) + 0xF4);
    auto* node = reinterpret_cast<std::uint8_t*>(g.allocNode(self, nullptr, 0xC, 0));
    storeU32(node + 8, held);
    g.linkNode(node, nullptr);
    g.postList(node, 0x10, node);
    return g.wipeList(self, nullptr);
}

std::uint32_t NotifyWhenAttackingBehavior_xfer(std::uint8_t* self, std::uint8_t* xfer) noexcept {
    version(xfer, 1, 1);
    version(xfer, 1, 1);
    g.xferRoot(self, nullptr, xfer);
    g.xferAt10(self + 0x10, nullptr, xfer);
    call1p(xfer, 0x84, self + 0x40);
    call1p(xfer, 0x84, self + 0x41);
    call1p(xfer, 0x84, self + 0x42);
    g.xferField(xfer, self + 0x30);
    g.xferPair(xfer, self + 0x44);
    return call1p(xfer, 0x6C, self + 0x48);
}

std::uint32_t PassiveAreaEffectBehavior_xfer(std::uint8_t* self, std::uint8_t* xfer) noexcept {
    version(xfer, 1, 1);
    version(xfer, 1, 1);
    g.xferRoot(self, nullptr, xfer);
    g.xferAt10(self + 0x10, nullptr, xfer);
    call1p(xfer, 0x6C, self + 0x28);
    return g.xferFieldB(xfer, self + 0x2C);
}

std::uint32_t ManagerBody_xfer(std::uint8_t* self, std::uint8_t* xfer) noexcept {
    version(xfer, 1, 1);
    version(xfer, 1, 1);
    version(xfer, 1, 1);
    g.xferRoot(self, nullptr, xfer);
    g.xferField(xfer, self + 0x124);
    return g.xferExtra(xfer, self + 0x134);
}

std::uint32_t CrateCollide_xfer(std::uint8_t* self, std::uint8_t* xfer) noexcept {
    version(xfer, 1, 1);
    version(xfer, 1, 1);
    version(xfer, 1, 1);
    g.xferRoot(self, nullptr, xfer);
    call1p(xfer, 0x84, self + 0x14);
    return g.xferField(xfer, self + 0x18);
}

std::uint32_t SquishCollide_xfer(std::uint8_t* self, std::uint8_t* xfer) noexcept {
    std::uint8_t ver[2] = {1, 3};
    call1p(xfer, 0x18, ver);
    call1p(xfer, 0x84, self + 0x30);
    call1p(xfer, 0x6C, self + 0x2C);
    std::uint32_t eax = 0;
    if (ver[1] == 2) {
        std::uint32_t slot = reinterpret_cast<std::uint32_t>(xfer);
        eax = g.xferPair(xfer, &slot);
    }
    if (ver[1] >= 3) {
        eax = g.xferField(xfer, self + 0x34);
    }
    version(xfer, 1, 1);
    version(xfer, 1, 1);
    return g.xferRoot(self, nullptr, xfer);
}

std::uint32_t CircularDamageUpdate_xfer(std::uint8_t* self, std::uint8_t* xfer) noexcept {
    std::uint8_t gate[2] = {1, 2};
    call1p(xfer, 0x18, gate);
    version(xfer, 1, 1);
    g.xferRoot(self, nullptr, xfer);
    g.xferAt10(self + 0x10, nullptr, xfer);
    call1p(xfer, 0x70, self + 0x24);
    call1p(xfer, 0x64, self + 0x28);
    call1p(xfer, 0x64, self + 0x2C);
    call1p(xfer, 0x70, self + 0x30);
    g.xferPairB(xfer, self + 0x34);
    call1p(xfer, 0x58, self + 0x38);
    g.xferField(xfer, self + 0x44);
    std::uint32_t eax = call1p(xfer, 0x6C, self + 0x54);
    if (gate[1] >= 2) {
        eax = call1p(xfer, 0x64, self + 0x58);
    }
    return eax;
}

std::uint32_t InvalidTargetsDeathUpdate_xfer(std::uint8_t* self, std::uint8_t* xfer) noexcept {
    version(xfer, 1, 1);
    version(xfer, 1, 1);
    g.xferRoot(self, nullptr, xfer);
    g.xferAt10(self + 0x10, nullptr, xfer);
    return g.xferField(xfer, self + 0x24);
}

std::uint32_t LinearDamageUpdate_xfer(std::uint8_t* self, std::uint8_t* xfer) noexcept {
    version(xfer, 1, 1);
    version(xfer, 1, 1);
    g.xferRoot(self, nullptr, xfer);
    g.xferAt10(self + 0x10, nullptr, xfer);
    call1p(xfer, 0x6C, self + 0x2C);
    call1p(xfer, 0x6C, self + 0x30);
    call1p(xfer, 0x58, self + 0x34);
    call1p(xfer, 0x58, self + 0x40);
    call1p(xfer, 0x6C, self + 0x4C);
    call1p(xfer, 0x64, self + 0x50);
    call1p(xfer, 0x70, self + 0x54);
    g.xferPairB(xfer, self + 0x58);
    call1p(xfer, 0x84, self + 0x5C);
    g.xferField(xfer, self + 0x60);
    return call1p(xfer, 0x6C, self + 0x70);
}

std::uint32_t MissileUpdate_xfer(std::uint8_t* self, std::uint8_t* xfer) noexcept {
    version(xfer, 1, 1);
    g.xferMissile(self, nullptr, xfer);
    call1p(xfer, 0x58, self + 0xF8);
    call1p(xfer, 0x58, self + 0x110);
    if (((loadU32(xfer + 4) >> 1) & 1u) != 0) {
        storeU32(self + 0x110, 0);
        storeU32(self + 0x114, 0);
        storeU32(self + 0x118, 0);
    }
    call1p(xfer, 0x6C, self + 0xE4);
    g.xferPair(xfer, self + 0xE8);
    g.xferPair(xfer, self + 0xEC);
    call1p(xfer, 0x84, self + 0x121);
    call1p(xfer, 0x6C, self + 0xF0);
    call1p(xfer, 0x64, self + 0xF4);
    call1p(xfer, 0x84, self + 0x122);
    call1p(xfer, 0x84, self + 0x120);
    call1p(xfer, 0x58, self + 0x104);
    call2u(xfer, 0x14, reinterpret_cast<std::uint32_t>(self + 0xE0), 4);
    return call2u(xfer, 0x14, reinterpret_cast<std::uint32_t>(self + 0x11C), 4);
}

std::uint32_t BeamSpecialAbilityUpdate_xfer(std::uint8_t* self, std::uint8_t* xfer) noexcept {
    version(xfer, 1, 1);
    g.xferAbility(self, nullptr, xfer);
    g.xferPair(xfer, self + 0xC4);
    call1p(xfer, 0x84, self + 0xF5);
    call1p(xfer, 0x84, self + 0xF4);
    call1p(xfer, 0x58, self + 0xC8);
    call1p(xfer, 0x58, self + 0xD4);
    call1p(xfer, 0x58, self + 0xE0);
    call1p(xfer, 0x70, self + 0xEC);
    call1p(xfer, 0x70, self + 0xF0);
    call1p(xfer, 0x84, self + 0xF6);
    call1p(xfer, 0x84, self + 0xFA);
    g.xferPair(xfer, self + 0xFC);
    call1p(xfer, 0x58, self + 0x100);
    call1p(xfer, 0x84, self + 0xF9);
    return call1p(xfer, 0x84, self + 0xF8);
}

std::uint32_t CloneStoredObjectsSpecialPowerUpdate_xfer(std::uint8_t* self, std::uint8_t* xfer) noexcept {
    version(xfer, 1, 1);
    g.xferAbility(self, nullptr, xfer);
    g.xferFieldB(xfer, self + 0xB8);
    call1p(xfer, 0x6C, self + 0xC4);
    return call1p(xfer, 0x6C, self + 0xC8);
}

std::uint32_t EjectPassengersSpecialAbilityUpdate_xfer(std::uint8_t* self, std::uint8_t* xfer) noexcept {
    version(xfer, 1, 1);
    g.xferAbility(self, nullptr, xfer);
    call1p(xfer, 0x6C, self + 0xB8);
    call1p(xfer, 0x64, self + 0xBC);
    call1p(xfer, 0x84, self + 0xC5);
    call1p(xfer, 0x64, self + 0xC0);
    return call1p(xfer, 0x84, self + 0xC4);
}

std::uint32_t SpecialDisguiseUpdate_xfer(std::uint8_t* self, std::uint8_t* xfer) noexcept {
    version(xfer, 1, 1);
    g.xferAbility(self, nullptr, xfer);
    call1p(xfer, 0x64, self + 0xBC);
    call1p(xfer, 0x70, self + 0xC4);
    call1p(xfer, 0x70, self + 0xC8);
    call1p(xfer, 0x6C, self + 0xC0);
    call1p(xfer, 0x6C, self + 0xD0);
    call1p(xfer, 0x70, self + 0xD4);
    const std::uint32_t shifted = loadU32(xfer + 4) >> 1;
    if ((shifted & 1u) != 0) {
        auto* obj = objOf(self);
        if (((loadU32(obj + 0xC8) >> 9) & 1u) != 0 && loadU32(self + 0xC0) != 0 && loadU32(self + 0xD0) != 0) {
            const std::uint32_t id = loadU32(obj + 0x418);
            const std::uint32_t slot = loadU32(reinterpret_cast<void*>(static_cast<std::uintptr_t>(env->player) + 0x2C));
            const std::uint32_t flag = id == slot ? 1u : 0u;
            return g.disguiseFlag(self, nullptr, flag);
        }
    }
    return shifted;
}

std::uint32_t ToggleHiddenSpecialAbilityUpdate_xfer(std::uint8_t* self, std::uint8_t* xfer) noexcept {
    version(xfer, 1, 1);
    call1p(xfer, 0x6C, self + 0xB8);
    return g.xferAbility(self, nullptr, xfer);
}

std::uint32_t ToggleMountedSpecialAbilityUpdate_xfer(std::uint8_t* self, std::uint8_t* xfer) noexcept {
    version(xfer, 1, 1);
    g.xferAbility(self, nullptr, xfer);
    return call1p(xfer, 0x84, self + 0xBC);
}

std::uint32_t ToggleStatusSpecialAbilityUpdate_xfer(std::uint8_t* self, std::uint8_t* xfer) noexcept {
    version(xfer, 1, 1);
    g.xferAbility(self, nullptr, xfer);
    return call1p(xfer, 0x6C, self + 0xBC);
}

std::uint32_t WeaponFireSpecialAbilityUpdate_xfer(std::uint8_t* self, std::uint8_t* xfer) noexcept {
    version(xfer, 1, 1);
    g.xferAbility(self, nullptr, xfer);
    const std::uint32_t flags = loadU32(xfer + 4);
    if (((flags >> 2) & 1u) != 0) {
        return flags >> 2;
    }
    std::uint8_t present = loadU32(self + 0xBC) != 0 ? 1 : 0;
    call1p(xfer, 0x84, &present);
    if (present != 0) {
        if ((flags & 1u) != 0 || loadU32(self + 0xBC) != 0) {
            call1u(xfer, 4, 0x00C21F4Cu);
            call1u(xfer, 0x20, loadU32(self + 0xBC));
            call0(xfer, 8);
        } else {
            call1u(xfer, 0x0C, 0x00C21F4Cu);
        }
    }
    g.xferPair(xfer, self + 0xCC);
    return call1p(xfer, 0x58, self + 0xC0);
}

std::uint32_t AnimalAIUpdate_xfer(std::uint8_t* self, std::uint8_t* xfer) noexcept {
    version(xfer, 1, 1);
    g.xferPair(xfer, self + 0x520);
    call1p(xfer, 0x84, self + 0x548);
    call1p(xfer, 0x58, self + 0x530);
    call1p(xfer, 0x84, self + 0x549);
    g.xferAi(self, nullptr, xfer);
    call1p(xfer, 0x64, self + 0x544);
    call1p(xfer, 0x58, self + 0x524);
    call1p(xfer, 0x6C, self + 0x53C);
    return call1p(xfer, 0x6C, self + 0x540);
}

std::uint32_t AssaultTransportAIUpdate_xfer(std::uint8_t* self, std::uint8_t* xfer) noexcept {
    g.xferAi(self, nullptr, xfer);
    const std::uint32_t flags = loadU32(xfer + 4);
    if (((flags >> 3) & 1u) != 0) {
        return flags >> 3;
    }
    version(xfer, 1, 1);
    call1p(xfer, 0x70, self + 0x574);
    const auto count = static_cast<std::int32_t>(loadU32(self + 0x574));
    for (std::int32_t index = 0; index < count; ++index) {
        g.xferPair(xfer, self + 0x520 + static_cast<std::uint32_t>(index) * 4);
        call1p(xfer, 0x84, self + 0x548 + index);
    }
    call1p(xfer, 0x58, self + 0x55C);
    g.xferPair(xfer, self + 0x568);
    std::uint32_t saved = loadU32(self + 0x56C);
    call1p(xfer, 0x70, &saved);
    storeU32(self + 0x56C, saved);
    call1p(xfer, 0x6C, self + 0x570);
    call1p(xfer, 0x84, self + 0x578);
    call1p(xfer, 0x84, self + 0x579);
    return call1p(xfer, 0x84, self + 0x57A);
}

std::uint32_t LaserStateModule_fillOutput(std::uint8_t* self, std::uint8_t* out) noexcept {
    auto* set = *reinterpret_cast<std::uint8_t**>(objOf(self) + 0x37C);
    std::uint32_t paired = 0;
    if (set != nullptr) {
        paired = g.weaponPair(set, nullptr, loadU32(self + 0x64), loadU32(self + 0x68));
    }
    const std::uint32_t key = loadU32(self + 0x54);
    std::uint32_t found = 0;
    if (key != 0) {
        found = lookupMap(key);
    }
    if (found == 0) {
        storeU32(out, loadU32(self + 0x3C));
        storeU32(out + 4, loadU32(self + 0x40));
        storeU32(out + 8, loadU32(self + 0x44));
        return static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(out));
    }
    auto* src = reinterpret_cast<std::uint8_t*>(found);
    storeU32(out, loadU32(src + 0x38));
    storeU32(out + 4, loadU32(src + 0x3C));
    storeU32(out + 8, loadU32(src + 0x40));
    if (paired == 0) {
        return loadU32(src + 0x3C);
    }
    const std::uint32_t accepted = g.testPair(reinterpret_cast<void*>(paired), nullptr, out, src);
    if (static_cast<std::uint8_t>(accepted) == 0 && self[0x5C] == 0) {
        g.floatAdjust(src + 0x110, nullptr, 0);
    }
    if (self[0x5C] != 0) {
        const float x = loadF(out);
        const float y = loadF(out + 4);
        const float z = loadF(out + 8);
        const bool same = x == loadF(self + 0x48) && y == loadF(self + 0x4C) && z == loadF(self + 0x50);
        if (!same) {
            copy3(out, self + 0x48);
        }
    }
    if (((loadU32(src + 0x94) >> 14) & 1u) != 0 && loadU32(src + 0x33C) != 0) {
        auto* nested = *reinterpret_cast<std::uint8_t**>(src + 0x33C);
        const std::uint32_t produced = call1u(nested, 0x24, 0);
        if (produced != 0) {
            return g.laserApply(reinterpret_cast<void*>(static_cast<std::uintptr_t>(env->laserGlobal)), nullptr,
                                produced, self + 0x30, out);
        }
    }
    return 0;
}

std::uint32_t SpawnBehavior_xfer(std::uint8_t* self, std::uint8_t* xfer) noexcept {
    g.xferStealth(self, nullptr, xfer);
    version(xfer, 1, 1);
    const std::uint32_t flags = loadU32(xfer + 4);
    if (((flags >> 3) & 1u) != 0) {
        return flags >> 3;
    }
    call1p(xfer, 0x84, self + 0x6A);
    g.xferAt10(self + 0x2C, nullptr, xfer);
    call1p(xfer, 0x6C, self + 0x74);
    std::uint32_t text[2] = {0, 0};
    auto* named = *reinterpret_cast<std::uint8_t**>(self + 0x40);
    if (named != nullptr) {
        std::memcpy(text, named + 4, 8);
    } else {
        auto* made = reinterpret_cast<std::uint8_t*>(g.stringCtor(&text, nullptr));
        std::memcpy(text, made, 8);
    }
    g.xferString(xfer, text);
    if (((flags >> 1) & 1u) != 0) {
        storeU32(self + 0x40, 0);
        if (text[1] != 0) {
            const std::uint32_t resolved =
                g.nameKey(reinterpret_cast<void*>(static_cast<std::uintptr_t>(env->nameRegistry)), nullptr, text[0]);
            storeU32(self + 0x40, resolved);
        }
    }
    call1p(xfer, 0x70, self + 0x44);
    call1p(xfer, 0x70, self + 0x48);
    call1p(xfer, 0x70, self + 0x4C);
    if (((flags >> 1) & 1u) != 0) {
        g.clearList(self + 0x50, nullptr);
    }
    g.xferList(xfer, self + 0x50);
    g.xferFieldB(xfer, self + 0x5C);
    call1p(xfer, 0x84, self + 0x68);
    call1p(xfer, 0x84, self + 0x69);
    call1p(xfer, 0x70, self + 0x6C);
    return call1p(xfer, 0x6C, self + 0x70);
}

std::uint32_t WeaponScatterEffectUpdate_xfer(std::uint8_t* self, std::uint8_t* xfer) noexcept {
    version(xfer, 1, 1);
    version(xfer, 1, 1);
    g.xferRoot(self, nullptr, xfer);
    g.xferAt10(self + 0x10, nullptr, xfer);
    auto transfer = [&](int beginOff, std::uint8_t& countByte) {
        const auto begin = static_cast<std::int32_t>(loadU32(self + beginOff));
        const auto end = static_cast<std::int32_t>(loadU32(self + beginOff + 4));
        countByte = static_cast<std::uint8_t>((end - begin) >> 4);
        call1p(xfer, 0x7C, &countByte);
        if (((loadU32(xfer + 4) >> 1) & 1u) != 0) {
            g.resize16(self + beginOff, nullptr, countByte);
        }
        for (std::uint8_t index = 0; index < countByte; ++index) {
            const std::uint32_t base = loadU32(self + beginOff) + static_cast<std::uint32_t>(index) * 16u;
            g.xferPair(xfer, reinterpret_cast<void*>(base));
            call1p(xfer, 0x58, reinterpret_cast<void*>(base + 4));
        }
    };
    std::uint8_t first = 0;
    std::uint8_t second = 0;
    transfer(0x2C, first);
    transfer(0x3C, second);
    g.xferPair(xfer, self + 0x4C);
    g.xferPair(xfer, self + 0x50);
    call1p(xfer, 0x84, self + 0x54);
    return call1p(xfer, 0x84, self + 0x55);
}

std::uint32_t RadarScanMapSpecialAbilityUpdate_clearScan(std::uint8_t* self, std::uint32_t a,
                                                         std::uint32_t b) noexcept {
    for (auto* cursor = *reinterpret_cast<std::uint8_t**>(self + 0xC8);
         cursor != *reinterpret_cast<std::uint8_t**>(self + 0xCC); cursor += 4) {
        const std::uint32_t id = loadU32(cursor);
        if (id == 0) {
            continue;
        }
        const std::uint32_t found = lookupMap(id);
        if (found == 0) {
            continue;
        }
        const std::uint32_t held = loadU32(reinterpret_cast<void*>(found + 0xF4));
        if (held != 0) {
            g.visitHeld(self, nullptr, held);
        }
    }
    auto* begin = *reinterpret_cast<std::uint8_t**>(self + 0xC8);
    auto* end = *reinterpret_cast<std::uint8_t**>(self + 0xCC);
    g.memMove(begin, end, 0);
    const auto bytes = static_cast<std::int32_t>(reinterpret_cast<std::uintptr_t>(end) -
                                                 reinterpret_cast<std::uintptr_t>(begin));
    const std::int32_t dwords = bytes >> 2;
    storeU32(self + 0xCC, loadU32(self + 0xCC) + static_cast<std::uint32_t>(-dwords * 4));
    auto* head = self + 0xD8;
    for (auto* node = *reinterpret_cast<std::uint8_t**>(head); node != head;
         node = *reinterpret_cast<std::uint8_t**>(node)) {
        auto* obj = objOf(self);
        auto* info = *reinterpret_cast<std::uint8_t**>(obj + 0x418);
        const std::uint32_t shift = loadU32(info + 0x20);
        const std::uint32_t mask = 1u << (shift & 31u);
        g.scanPulse(reinterpret_cast<void*>(static_cast<std::uintptr_t>(env->scanGlobal)), nullptr, node + 8, 0,
                    mask);
    }
    auto* node = *reinterpret_cast<std::uint8_t**>(head);
    while (node != head) {
        auto* next = *reinterpret_cast<std::uint8_t**>(node);
        g.freePtr(node);
        node = next;
    }
    storeU32(head, reinterpret_cast<std::uint32_t>(head));
    storeU32(head + 4, reinterpret_cast<std::uint32_t>(head));
    return g.xferBase(self, nullptr, a, b);
}

std::uint32_t TransportContain_refreshPassengers(std::uint8_t* self) noexcept {
    auto* face = self + 0x24;
    std::uint32_t count = call1u(face, 0x10C, 0);
    if (count == 0) {
        return count;
    }
    auto* templ = *reinterpret_cast<std::uint8_t**>(self + 4);
    auto* matrix = *reinterpret_cast<std::uint8_t**>(self + 8);
    auto* vec = *reinterpret_cast<float**>(templ + 0x130);
    float out[3];
    transformPassenger(vec, reinterpret_cast<float*>(matrix), out);
    float local[3]{};
    std::uint32_t eax = count;
    while (count != 0) {
        auto* passenger = reinterpret_cast<std::uint8_t*>(call1u(face, 0x128, 0));
        eax = reinterpret_cast<std::uint32_t>(passenger);
        auto* draw = *reinterpret_cast<std::uint8_t**>(passenger + 0x378);
        if (draw != nullptr) {
            g.projectPassengers(draw, nullptr, local, out, matrix, loadU32(templ + 0x118), 0);
            g.touchPassenger(draw, nullptr, 1);
            const std::uint32_t bits = loadU32(passenger + 0xBC);
            if (((bits >> 4) & 1u) == 0) {
                storeU32(passenger + 0xBC, bits | 0x10u);
                eax = g.markPassenger(passenger, nullptr);
            }
        }
        --count;
    }
    return eax;
}

}  // namespace ra3::gamelogic
