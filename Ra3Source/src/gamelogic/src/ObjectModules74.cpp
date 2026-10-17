#include "ra3/gamelogic/ObjectModules74.h"

#include <cstring>

namespace ra3::gamelogic {
namespace {

Object74Calls g_calls{};
Object74Env g_env{};

// Immediate pushed by ClusterBombUpdate_xfer. The bytes at this address are
// the C string ClusterBombUpdateInitialWeapon.
constexpr std::uint32_t kClusterWeaponName = 0x00C2240Cu;

// movss source at 0x00BD90A4. The dword is 0x3F800000.
constexpr std::uint32_t kMountedScaleBits = 0x3F800000u;

std::uint32_t loadU32(const void* address) noexcept {
    std::uint32_t value = 0;
    std::memcpy(&value, address, 4);
    return value;
}

void storeU32(void* address, std::uint32_t value) noexcept {
    std::memcpy(address, &value, 4);
}

std::uint8_t* ptrAt(const std::uint8_t* base, int offset) noexcept {
    return reinterpret_cast<std::uint8_t*>(static_cast<std::uintptr_t>(loadU32(base + offset)));
}

std::uint32_t bits(const void* address) noexcept {
    return static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(address));
}

std::uint32_t call0(void* object, int slot) noexcept {
    auto* vt = *reinterpret_cast<std::uint8_t**>(object);
    auto* fn = *reinterpret_cast<std::uint32_t(__fastcall**)(void*, void*)>(vt + slot);
    return fn(object, nullptr);
}

std::uint32_t call1(void* object, int slot, std::uint32_t arg) noexcept {
    auto* vt = *reinterpret_cast<std::uint8_t**>(object);
    auto* fn = *reinterpret_cast<std::uint32_t(__fastcall**)(void*, void*, std::uint32_t)>(vt + slot);
    return fn(object, nullptr, arg);
}

std::uint32_t xferMode(std::uint8_t* xfer) noexcept {
    std::uint8_t mode[2] = {1, 1};
    return call1(xfer, 0x18, bits(mode));
}

bool flagBit(std::uint32_t flags, unsigned shift) noexcept {
    return ((flags >> shift) & 1u) != 0;
}

std::uint32_t maskLowBit(std::uint32_t value) noexcept {
    return (value & 0xFFFFFF00u) | (value & 1u);
}

// Shipped `mov reg, end; sub reg, end` passes a zero byte count, then folds the
// vector end pointer back onto begin.
std::uint32_t collapseVector(std::uint8_t* beginField, std::uint8_t* endField) noexcept {
    const std::uint32_t endBits = loadU32(endField);
    const std::uint32_t beginBits = loadU32(beginField);
    const std::uint32_t byteCount = endBits - endBits;
    const std::uint32_t moved = g_calls.rawMove(beginBits, endBits, byteCount);
    std::int32_t delta = static_cast<std::int32_t>(endBits - beginBits);
    delta >>= 2;
    delta = -delta;
    delta += delta;
    delta += delta;
    storeU32(endField, endBits + static_cast<std::uint32_t>(delta));
    return moved;
}

std::uint32_t receivePassenger(std::uint8_t* self, std::uint8_t* passenger, bool slaughter) noexcept {
    if (slaughter) {
        g_calls.slaughterNote(self, nullptr, bits(passenger));
    }
    g_calls.containAdd(self + 0x24, nullptr, bits(passenger));
    g_calls.recordAdd(self + 0x760, nullptr, bits(passenger));
    auto* tmpl = ptrAt(passenger, 4);
    // obj74 mutation site: template flag bit 10 (shr 0xA, test cl, 1).
    // The flag load leaves the template pointer in EAX when the bit is clear.
    if (flagBit(loadU32(tmpl + 0xCC), 10)) {
        auto* object = ptrAt(self, 8);
        return g_calls.placePassenger(passenger, nullptr, bits(object + 0x38));
    }
    return bits(tmpl);
}

using Relay3 = std::uint32_t(__fastcall*)(void*, void*, std::uint32_t, std::uint32_t, std::uint32_t);

std::uint32_t notifyPassengers(std::uint8_t* self, std::uint32_t a0, std::uint32_t a1, std::uint32_t a2,
                               Relay3 inner, Relay3 tail) noexcept {
    auto* object = ptrAt(self, 8);
    auto* contain = ptrAt(object, 0x328);
    if (contain == nullptr) {
        const std::uint32_t begin = loadU32(object + 0x32C);
        const std::uint32_t end = loadU32(object + 0x330);
        if (begin != end) {
            contain = reinterpret_cast<std::uint8_t*>(static_cast<std::uintptr_t>(loadU32(
                reinterpret_cast<void*>(static_cast<std::uintptr_t>(begin)))));
        }
    }
    if (contain != nullptr && (call0(contain, 0xB0) & 0xFFu) != 0 && (a2 == 0 || a2 == 1)) {
        struct Out {
            std::uint32_t first;
            std::uint32_t head;
        } out{};
        call1(contain, 0x110, bits(&out));
        std::uint32_t node = loadU32(reinterpret_cast<void*>(static_cast<std::uintptr_t>(out.head)));
        while (node != out.head) {
            auto* rider = reinterpret_cast<std::uint8_t*>(static_cast<std::uintptr_t>(
                loadU32(reinterpret_cast<void*>(static_cast<std::uintptr_t>(node + 8)))));
            const std::uint32_t module = loadU32(rider + 0x374);
            node = loadU32(reinterpret_cast<void*>(static_cast<std::uintptr_t>(node)));
            if (module != 0) {
                inner(reinterpret_cast<void*>(static_cast<std::uintptr_t>(module + 0x24)), nullptr, a0, a1, a2);
            }
        }
        if (out.first != 0) {
            call1(reinterpret_cast<void*>(static_cast<std::uintptr_t>(out.first)), 0, 0);
        }
    }
    return tail(self, nullptr, a0, a1, a2);
}

}  // namespace

void setObject74Calls(const Object74Calls& calls) noexcept {
    g_calls = calls;
}

void setObject74Env(const Object74Env& env) noexcept {
    g_env = env;
}

std::uint32_t TransportContain_xfer(std::uint8_t* self, std::uint8_t* xfer) noexcept {
    xferMode(xfer);
    g_calls.openContainXfer(self, nullptr, bits(xfer));
    call1(xfer, 0x70, bits(self + 0x148));
    call1(xfer, 0x6C, bits(self + 0x14C));
    return call1(xfer, 0x84, bits(self + 0x158));
}

std::uint32_t HealContain_receivePassenger(std::uint8_t* self, std::uint8_t* passenger) noexcept {
    return receivePassenger(self, passenger, false);
}

std::uint32_t HordeGarrisonContain_xfer(std::uint8_t* self, std::uint8_t* xfer) noexcept {
    xferMode(xfer);
    xferMode(xfer);
    g_calls.garrisonXfer(self, nullptr, bits(xfer));
    return call1(xfer, 0x6C, bits(self + 0x770));
}

std::uint32_t SlaughterHordeContain_receivePassenger(std::uint8_t* self, std::uint8_t* passenger) noexcept {
    return receivePassenger(self, passenger, true);
}

std::uint32_t GarrisonStealthContain_xfer(std::uint8_t* self, std::uint8_t* xfer) noexcept {
    xferMode(xfer);
    return g_calls.garrisonXfer(self, nullptr, bits(xfer));
}

std::uint32_t HealContain_xfer(std::uint8_t* self, std::uint8_t* xfer) noexcept {
    xferMode(xfer);
    call1(xfer, 0x6C, bits(self + 0x774));
    return HordeGarrisonContain_xfer(self, xfer);
}

std::uint32_t TunnelContain_xfer(std::uint8_t* self, std::uint8_t* xfer) noexcept {
    std::uint32_t eax = HordeGarrisonContain_xfer(self, xfer);
    if (flagBit(loadU32(xfer + 4), 3)) {
        return (eax & 0xFFFFFF00u) | 1u;
    }
    xferMode(xfer);
    call1(xfer, 0x84, bits(self + 0x774));
    call1(xfer, 0x84, bits(self + 0x775));
    return g_calls.xferField(bits(xfer), bits(self + 0x778));
}

std::uint32_t HealContain_removePassenger(std::uint8_t* self, std::uint8_t* passenger) noexcept {
    const std::uint32_t ready = g_calls.canRemove(self + 0x760, nullptr, bits(self), bits(passenger));
    if ((ready & 0xFFu) == 0) {
        return ready;
    }
    g_calls.doRemove(self + 0x24, nullptr, bits(passenger), 0);
    return call0(self, 0x78);
}

std::uint32_t SlaughterHordeContain_removePassenger(std::uint8_t* self, std::uint8_t* passenger) noexcept {
    const std::uint32_t ready = g_calls.canRemoveAlt(self + 0x760, nullptr, bits(self), bits(passenger));
    if (ready == 0) {
        return ready;
    }
    call0(self, 0x78);
    return g_calls.doRemove(self + 0x24, nullptr, bits(passenger), 0);
}

std::uint32_t RailedTransportDockUpdate_xfer(std::uint8_t* self, std::uint8_t* xfer) noexcept {
    xferMode(xfer);
    g_calls.dockXfer(self, nullptr, bits(xfer));
    g_calls.xferField(bits(xfer), bits(self + 0x90));
    call1(xfer, 0x64, bits(self + 0x94));
    g_calls.xferField(bits(xfer), bits(self + 0x98));
    call1(xfer, 0x64, bits(self + 0x9C));
    return call1(xfer, 0x70, bits(self + 0xA0));
}

std::uint32_t RepairDockUpdate_xfer(std::uint8_t* self, std::uint8_t* xfer) noexcept {
    const std::uint32_t eax = g_calls.dockXfer(self, nullptr, bits(xfer));
    if (flagBit(loadU32(xfer + 4), 3)) {
        return (eax & 0xFFFFFF00u) | 1u;
    }
    xferMode(xfer);
    g_calls.xferField(bits(xfer), bits(self + 0x8C));
    return call1(xfer, 0x64, bits(self + 0x90));
}

std::uint32_t SupplyCenterDockUpdate_xfer(std::uint8_t* self, std::uint8_t* xfer) noexcept {
    xferMode(xfer);
    return g_calls.dockXfer(self, nullptr, bits(xfer));
}

std::uint32_t SupplyWarehouseDockUpdate_xfer(std::uint8_t* self, std::uint8_t* xfer) noexcept {
    xferMode(xfer);
    g_calls.dockXfer(self, nullptr, bits(xfer));
    return call1(xfer, 0x70, bits(self + 0x8C));
}

std::uint32_t AutoDepositUpdate_xfer(std::uint8_t* self, std::uint8_t* xfer) noexcept {
    xferMode(xfer);
    xferMode(xfer);
    g_calls.behaviorXfer(self, nullptr, bits(xfer));
    const std::uint32_t nested = g_calls.xferFrom10(self + 0x10, nullptr, bits(xfer));
    const std::uint32_t flags = loadU32(xfer + 4);
    if (flagBit(flags, 3)) {
        return flags;
    }
    if (flagBit(flags, 1)) {
        collapseVector(self + 0x24, self + 0x28);
    }
    call1(xfer, 0x6C, bits(self + 0x34));
    call1(xfer, 0x84, bits(self + 0x38));
    call1(xfer, 0x84, bits(self + 0x39));
    return g_calls.xferSpan(bits(xfer), bits(self + 0x24));
}

std::uint32_t ClusterBombUpdate_xfer(std::uint8_t* self, std::uint8_t* xfer) noexcept {
    xferMode(xfer);
    xferMode(xfer);
    g_calls.behaviorXfer(self, nullptr, bits(xfer));
    g_calls.xferFrom10(self + 0x10, nullptr, bits(xfer));
    call1(xfer, 0x20, loadU32(self + 0x24));
    call1(xfer, 0x84, bits(self + 0x2C));
    std::uint8_t armed = loadU32(self + 0x28) != 0 ? 1 : 0;
    call1(xfer, 0x84, bits(&armed));
    if (armed != 0) {
        if (loadU32(self + 0x28) != 0) {
            call1(xfer, 0x04, kClusterWeaponName);
            call1(xfer, 0x20, loadU32(self + 0x28));
            call0(xfer, 0x08);
        } else {
            // The name immediate is pushed before the branch, so this arm receives it too.
            call1(xfer, 0x0C, kClusterWeaponName);
        }
    }
    if (flagBit(loadU32(xfer + 4), 1)) {
        collapseVector(self + 0x30, self + 0x34);
    }
    return g_calls.xferSpan(bits(xfer), bits(self + 0x30));
}

std::uint32_t DamageFieldUpdate_xfer(std::uint8_t* self, std::uint8_t* xfer) noexcept {
    xferMode(xfer);
    g_calls.xferBlock(bits(xfer), bits(self + 0x44));
    return g_calls.damageXfer(self, nullptr, bits(xfer));
}

std::uint32_t SupplyTruckAIUpdate_xfer(std::uint8_t* self, std::uint8_t* xfer) noexcept {
    g_calls.supplyXfer(self, nullptr, bits(xfer));
    const std::uint32_t shifted = loadU32(xfer + 4) >> 3;
    if ((shifted & 1u) != 0) {
        return shifted;
    }
    xferMode(xfer);
    call1(xfer, 0x20, loadU32(self + 0x520));
    g_calls.xferField(bits(xfer), bits(self + 0x524));
    call1(xfer, 0x58, bits(self + 0x528));
    call1(xfer, 0x84, bits(self + 0x534));
    call1(xfer, 0x6C, bits(self + 0x55C));
    call1(xfer, 0x84, bits(self + 0x560));
    call1(xfer, 0x84, bits(self + 0x561));
    g_calls.xferField(bits(xfer), bits(self + 0x538));
    g_calls.xferField(bits(xfer), bits(self + 0x53C));
    call1(xfer, 0x6C, bits(self + 0x564));
    call1(xfer, 0x6C, bits(self + 0x568));
    std::uint32_t count = static_cast<std::uint32_t>(
        static_cast<std::int32_t>(loadU32(self + 0x550) - loadU32(self + 0x54C)) >> 2);
    call1(xfer, 0x6C, bits(&count));
    if (flagBit(loadU32(xfer + 4), 1)) {
        g_calls.resizeCount(self + 0x54C, nullptr, count);
    }
    for (std::uint32_t index = 0; index < count; ++index) {
        const std::uint32_t slot = loadU32(self + 0x54C) + index * 4u;
        call1(xfer, 0x6C, slot);
    }
    return call1(xfer, 0x58, bits(self + 0x540));
}

std::uint32_t ObjectStatusSpecialAbilityUpdate_xfer(std::uint8_t* self, std::uint8_t* xfer) noexcept {
    g_calls.updateXfer(self, nullptr, bits(xfer));
    xferMode(xfer);
    return call1(xfer, 0x84, bits(self + 0xB8));
}

std::uint32_t OCLMonitorSpecialAbilityUpdate_xfer(std::uint8_t* self, std::uint8_t* xfer) noexcept {
    xferMode(xfer);
    std::uint32_t count = static_cast<std::uint32_t>(
        static_cast<std::int32_t>(loadU32(self + 0xBC) - loadU32(self + 0xB8)) >> 2);
    std::uint32_t eax = call1(xfer, 0x6C, bits(&count));
    if (flagBit(loadU32(xfer + 4), 1)) {
        eax = g_calls.resizeVector(self + 0xB8, nullptr, count);
    }
    for (std::uint32_t index = 0; index < count; ++index) {
        const std::uint32_t slot = loadU32(self + 0xB8) + index * 4u;
        eax = g_calls.xferField(bits(xfer), slot);
    }
    return eax;
}

std::uint32_t ReplaceObjectUpdate_xfer(std::uint8_t* self, std::uint8_t* xfer) noexcept {
    const std::uint32_t eax = g_calls.updateXfer(self, nullptr, bits(xfer));
    if (flagBit(loadU32(xfer + 4), 3)) {
        return (eax & 0xFFFFFF00u) | 1u;
    }
    return xferMode(xfer);
}

std::uint32_t SubStructureBuildUpgrade_xfer(std::uint8_t* self, std::uint8_t* xfer) noexcept {
    xferMode(xfer);
    g_calls.upgradeXfer(self, nullptr, bits(xfer));
    g_calls.xferFrom10(self + 0x18, nullptr, bits(xfer));
    const std::uint32_t full = static_cast<std::uint32_t>(
        static_cast<std::int32_t>(loadU32(self + 0x34) - loadU32(self + 0x30)) >> 2);
    std::uint8_t count = static_cast<std::uint8_t>(full);
    call1(xfer, 0x7C, bits(&count));
    if (flagBit(loadU32(xfer + 4), 1)) {
        g_calls.resizeVector(self + 0x30, nullptr, count);
    }
    for (std::uint32_t index = 0; index < count; ++index) {
        const std::uint32_t slot = loadU32(self + 0x30) + index * 4u;
        g_calls.xferField(bits(xfer), slot);
    }
    return call1(xfer, 0x84, bits(self + 0x40));
}

std::uint32_t DelayedWeaponFireSpecialAbilityUpdate_xfer(std::uint8_t* self, std::uint8_t* xfer) noexcept {
    xferMode(xfer);
    g_calls.delayedXfer(self, nullptr, bits(xfer));
    return call1(xfer, 0x6C, bits(self + 0xD0));
}

std::uint32_t AIUpdateInterface_storeGoalPosition(std::uint8_t* self, const std::uint8_t* coord) noexcept {
    storeU32(self + 0x60, loadU32(coord));
    storeU32(self + 0x64, loadU32(coord + 4));
    const std::uint32_t z = loadU32(coord + 8);
    storeU32(self + 0x68, z);
    return z;
}

std::uint32_t MissileUpdate_arm(std::uint8_t* self, std::uint32_t arg) noexcept {
    g_calls.missilePrep(self, nullptr, arg);
    if (loadU32(self + 0xE0) != 1u) {
        storeU32(self + 0xE0, 1u);
        storeU32(self + 0xE4, loadU32(g_env.gameLogic + 0x58));
    }
    auto* body = ptrAt(self, 8);
    self[0x120] = 0;
    storeU32(self + 0x104, loadU32(body + 0x38));
    storeU32(self + 0x108, loadU32(body + 0x3C));
    const std::uint32_t z = loadU32(body + 0x40);
    storeU32(self + 0x10C, z);
    return z;
}

std::uint32_t StealUnitUpdate_latch(std::uint8_t* self) noexcept {
    const std::uint32_t primed = g_calls.attachBase(self, nullptr);
    if (self[0x50] != 0) {
        return primed;
    }
    auto* target = reinterpret_cast<std::uint8_t*>(static_cast<std::uintptr_t>(g_calls.currentTarget(self, nullptr)));
    if (target == nullptr) {
        return 0;
    }
    std::uint8_t* weapon = nullptr;
    if (loadU32(target + 0x310) != 0 && loadU32(target + 0x3C0) != 0) {
        weapon = ptrAt(ptrAt(target, 0x3B4), 0x14);
    }
    auto* object = ptrAt(self, 8);
    const std::uint32_t key = loadU32(ptrAt(object, 0x418) + 0x20);
    call1(weapon, 0xB0, key);
    self[0x50] = 1;
    auto* record = ptrAt(target, 0x418);
    storeU32(self + 0x54, loadU32(record + 0x20));
    return bits(record);
}

std::uint32_t SpawnBehavior_prime(std::uint8_t* self, std::uint32_t, std::uint32_t) noexcept {
    auto* tmpl = ptrAt(self, 4);
    if (tmpl[0x58] == 0) {
        return bits(tmpl);
    }
    const std::uint32_t eax = g_calls.spawnOne(self, nullptr, loadU32(self + 0x6C));
    self[0x68] = 1;
    return eax;
}

std::uint32_t ToggleMountedSpecialAbilityUpdate_clear(std::uint8_t* self) noexcept {
    g_calls.mountedPrep(self, nullptr);
    auto* object = ptrAt(self, 8);
    auto* rider = ptrAt(object, 0xF4);
    if (rider != nullptr) {
        storeU32(rider + 0x1BC, kMountedScaleBits);
    }
    const std::uint32_t previous = loadU32(object + 0x88);
    storeU32(object + 0x88, previous & 0xFFFFDFFFu);
    const std::uint32_t eax = maskLowBit(previous >> 13);
    if ((eax & 1u) != 0) {
        return g_calls.clearStatus(object, nullptr, 0x4D, 0);
    }
    return eax;
}

std::uint32_t DozerAIUpdate_routeBuild(std::uint8_t* self) noexcept {
    if (loadU32(self + 0x5DC) != 0) {
        return call0(self + 0x51C, 0x64);
    }
    auto* state = reinterpret_cast<std::uint8_t*>(
        static_cast<std::uintptr_t>(g_calls.queryState(ptrAt(self, 0x538), nullptr)));
    if (state == nullptr) {
        return 0;
    }
    return g_calls.pushBuild(state, nullptr, 6, 1, 0);
}

std::uint32_t StealUnitUpdate_relay(std::uint8_t* self, std::uint32_t arg) noexcept {
    const std::uint32_t forwarded = g_calls.stealForward(self, nullptr, arg);
    auto* target = reinterpret_cast<std::uint8_t*>(static_cast<std::uintptr_t>(g_calls.currentTarget(self, nullptr)));
    if (target != nullptr) {
        std::uint8_t* weapon = nullptr;
        if (loadU32(target + 0x310) != 0 && loadU32(target + 0x3C0) != 0) {
            weapon = ptrAt(ptrAt(target, 0x3B4), 0x14);
        }
        const std::uint32_t key = loadU32(ptrAt(target, 0x418) + 0x20);
        call1(weapon, 0xB0, key);
    }
    return forwarded;
}

std::uint32_t TechUpgrade_finish(std::uint8_t* self) noexcept {
    const std::uint32_t ready = call0(self + 0x10, 0);
    if ((ready & 0xFFu) == 0) {
        return ready;
    }
    auto* object = ptrAt(self, 8);
    return g_calls.techApply(self - 4, nullptr, loadU32(object + 0x418));
}

std::uint32_t TechUpgrade_grant(std::uint8_t* self, std::uint32_t first, std::uint32_t second) noexcept {
    const std::uint32_t ready = call0(self + 0x10, 0);
    if ((ready & 0xFFu) == 0) {
        return ready;
    }
    auto* object = ptrAt(self, 8);
    if (loadU32(object + 0x26C) != 0) {
        return 0;
    }
    std::uint32_t eax = 0;
    if (first != 0) {
        eax = g_calls.techApply(self - 4, nullptr, first);
    }
    if (second != 0) {
        eax = g_calls.techRestore(self - 4, nullptr, second);
    }
    return eax;
}

std::uint32_t LaserStateModule_schedule(std::uint8_t* self, std::uint32_t extra, std::uint32_t) noexcept {
    const std::uint32_t frame = loadU32(g_env.gameLogic + 0x58);
    storeU32(self + 0x58, frame + extra);
    const std::uint32_t next = loadU32(g_env.gameLogic + 0x58) + 1u;
    g_calls.stamp(loadU32(self + 8), bits(self + 0x10), next);
    auto* tmpl = ptrAt(self, 4);
    if (loadU32(tmpl + 0x14) != 0) {
        g_calls.laserPair(self, nullptr, bits(tmpl + 0x14), bits(self + 0x84));
    }
    if (loadU32(tmpl + 0x1C) != 0) {
        g_calls.laserPair(self, nullptr, bits(tmpl + 0x1C), bits(self + 0x74));
    }
    return g_calls.laserFinish(self, nullptr);
}

std::uint32_t GiveOrRestoreUpgradeSpecialPower_apply(std::uint8_t* self) noexcept {
    g_calls.prepareSpecial(self, nullptr);
    auto* object = ptrAt(self, 8);
    if (object == nullptr) {
        return 0;
    }
    auto* tmpl = ptrAt(self, 4);
    const std::uint32_t upgrade = loadU32(tmpl + 0xF8);
    if (upgrade == 0) {
        return bits(tmpl);
    }
    const std::uint32_t has = g_calls.hasUpgrade(object, nullptr, upgrade);
    if ((has & 0xFFu) != 0) {
        return has;
    }
    const std::uint32_t made = g_calls.makeUpgrade(self, nullptr, upgrade, bits(object + 0x38));
    if ((made & 0xFFu) != 0) {
        return made;
    }
    return g_calls.grantUpgrade(object, nullptr, upgrade, 0);
}

std::uint32_t SwitchLocomotorsSpecialAbilityUpdate_commit(std::uint8_t* self, std::uint32_t arg,
                                                          std::uint32_t flag) noexcept {
    g_calls.locoPrelude(self, nullptr, arg, flag);
    auto* tmpl = ptrAt(self, 4);
    auto* object = ptrAt(self, 8);
    const std::uint32_t previous = loadU32(object + 0x88);
    storeU32(object + 0x88, previous & 0xBFFFFFFFu);
    std::uint32_t eax = maskLowBit(previous >> 30);
    if ((eax & 1u) != 0) {
        eax = g_calls.clearStatus(object, nullptr, 0x5E, 0);
    }
    if ((flag & 0xFFu) == 0) {
        return eax;
    }
    auto* cursor = tmpl + 0xA8;
    for (std::uint32_t index = 0; index < 7u; ++index) {
        if (loadU32(cursor) != 0) {
            return g_calls.applyMask(object, nullptr, bits(tmpl + 0xA8), 1);
        }
        cursor += 4;
    }
    return 7u;
}

std::uint32_t ToggleDeploySpecialAbilityUpdate_flip(std::uint8_t* self) noexcept {
    g_calls.prepareSpecial(self, nullptr);
    auto* object = ptrAt(self, 8);
    if (loadU32(self + 0x2C) != 1u) {
        return bits(object);
    }
    const std::uint32_t flags = loadU32(object + 0xC0);
    const int slot = flagBit(flags, 9) ? 0xAC : 0xB0;
    return call1(self, slot, bits(object));
}

std::uint32_t ToggleHiddenSpecialAbilityUpdate_flip(std::uint8_t* self) noexcept {
    const std::uint32_t prepared = g_calls.prepareSpecial(self, nullptr);
    if (loadU32(self + 0x2C) != 1u) {
        return prepared;
    }
    auto* object = ptrAt(self, 8);
    const int slot = flagBit(loadU32(object + 0x80), 18) ? 0xAC : 0xB0;
    return call0(self, slot);
}

std::uint32_t ToggleStatusSpecialAbilityUpdate_rewind(std::uint8_t* self) noexcept {
    if (loadU32(self + 0xBC) == 0) {
        return 0;
    }
    storeU32(self + 0xBC, 0);
    return g_calls.rewindStatus(self, nullptr);
}

std::uint32_t ObjectStatusSpecialAbilityUpdate_swap(std::uint8_t* self) noexcept {
    g_calls.prepareSpecial(self, nullptr);
    auto* tmpl = ptrAt(self, 4);
    auto* object = ptrAt(self, 8);
    const bool armed = self[0xB8] != 0;
    auto* first = armed ? tmpl + 0x10C : tmpl + 0xF0;
    auto* second = armed ? tmpl + 0xF0 : tmpl + 0x10C;
    g_calls.applyMask(object, nullptr, bits(first), 0);
    const std::uint32_t previous = g_calls.applyMask(object, nullptr, bits(second), 1);
    const std::uint32_t eax = (previous & 0xFFFFFF00u) | (armed ? 0u : 1u);
    self[0xB8] = armed ? 0 : 1;
    return eax;
}

std::uint32_t MoveToGroundState_update(std::uint8_t* self) noexcept {
    if ((g_calls.groundBusy(self, nullptr) & 0xFFu) != 0) {
        return 0xFFFFFFFFu;
    }
    auto* link = ptrAt(self, 0x18);
    auto* aim = ptrAt(link, 0x24);
    auto* state = reinterpret_cast<std::uint8_t*>(static_cast<std::uintptr_t>(g_calls.queryState(link, nullptr)));
    if (state != nullptr && (g_calls.groundAim(aim, nullptr, bits(state), 0) & 0xFFu) != 0) {
        return 0xFFFFFFFFu;
    }
    if ((g_calls.groundCheck(self, nullptr) & 0xFFu) != 0) {
        return 0xFFFFFFFFu;
    }
    if ((g_calls.groundNear(self, nullptr) & 0xFFu) != 0) {
        return 0xFFFFFFFFu;
    }
    g_calls.groundClear(self, nullptr);
    g_calls.groundStep(self, nullptr);
    return 0;
}

std::uint32_t TransportAIUpdate_notifyPassengers45(std::uint8_t* self, std::uint32_t a0, std::uint32_t a1,
                                                   std::uint32_t a2) noexcept {
    return notifyPassengers(self, a0, a1, a2, g_calls.inner45, g_calls.relay45);
}

std::uint32_t TransportAIUpdate_notifyPassengers49(std::uint8_t* self, std::uint32_t a0, std::uint32_t a1,
                                                   std::uint32_t a2) noexcept {
    return notifyPassengers(self, a0, a1, a2, g_calls.inner49, g_calls.relay49);
}

std::uint32_t TransportAIUpdate_notifyPassengers52(std::uint8_t* self, std::uint32_t a0, std::uint32_t a1,
                                                   std::uint32_t a2) noexcept {
    return notifyPassengers(self, a0, a1, a2, g_calls.inner52, g_calls.relay52);
}

std::uint32_t PlayerPowerManagerBehavior_refresh(std::uint8_t* self) noexcept {
    auto* object = ptrAt(self, 8);
    if (object == nullptr) {
        return 0;
    }
    auto* record = ptrAt(object, 0x418);
    if (record == nullptr) {
        return 0;
    }
    auto* power = ptrAt(record, 0x1320);
    storeU32(self + 0x40, bits(power));
    if (power == nullptr) {
        return 0;
    }
    g_calls.linkPower(power + 4, nullptr, bits(self + 0x28));
    return g_calls.bindPower(self, nullptr);
}

std::uint32_t FlingPassengerSpecialAbilityUpdate_fling(std::uint8_t* self) noexcept {
    g_calls.prepareSpecial(self, nullptr);
    auto* object = ptrAt(self, 8);
    auto* contain = ptrAt(object, 0x328);
    if (contain == nullptr) {
        const std::uint32_t begin = loadU32(object + 0x32C);
        const std::uint32_t end = loadU32(object + 0x330);
        if (begin != end) {
            contain = reinterpret_cast<std::uint8_t*>(static_cast<std::uintptr_t>(loadU32(
                reinterpret_cast<void*>(static_cast<std::uintptr_t>(begin)))));
        }
    }
    const std::uint32_t count = call1(contain, 0x10C, 0);
    if (count == 0) {
        return count;
    }
    auto* unit = reinterpret_cast<std::uint8_t*>(static_cast<std::uintptr_t>(call1(contain, 0x128, 0)));
    if ((unit[0x471] & 1u) == 0) {
        auto* alt = ptrAt(unit, 0x378);
        if (alt == nullptr) {
            return bits(unit);
        }
        return g_calls.flingAlt(alt, nullptr);
    }
    if ((unit[0xB2] & 1u) == 0) {
        storeU32(unit + 0xB0, loadU32(unit + 0xB0) | 0x10000u);
        g_calls.markObject(unit, nullptr);
    }
    const std::uint32_t shifted = loadU32(unit + 0xBC) >> 7;
    if ((shifted & 1u) != 0) {
        return shifted;
    }
    storeU32(unit + 0xBC, loadU32(unit + 0xBC) | 0x80u);
    return g_calls.markObject(unit, nullptr);
}

std::uint32_t WeaponScatterEffectUpdate_apply(std::uint8_t* self) noexcept {
    auto* object = ptrAt(self, -8);
    if (object == nullptr) {
        return 0;
    }
    auto* record = ptrAt(self, -12);
    if (record[0x29] != 0) {
        return 0;
    }
    auto* weapon = ptrAt(object, 0x33C);
    if (weapon != nullptr) {
        const std::uint32_t index = loadU32(record + 0x24);
        if (record[0x28] == 0) {
            call1(weapon, 0x60, index);
            g_calls.scatterOn(object, nullptr, g_env.scatterIds[index]);
        } else {
            call1(weapon, 0x5C, index);
            g_calls.scatterOff(object, nullptr, g_env.scatterIds[index]);
        }
    }
    object = ptrAt(self, -8);
    if (object == nullptr) {
        return 0;
    }
    record = ptrAt(self, -12);
    auto* idSlot = ptrAt(record, 8);
    if (idSlot == nullptr) {
        return 0;
    }
    const std::uint32_t id = loadU32(idSlot);
    if (id == 0xFFFFFFFFu) {
        return id;
    }
    return g_calls.scatterOn(object, nullptr, id);
}

std::uint32_t ProjectileUpdate_launch(std::uint8_t* self, std::uint8_t* source, const std::uint8_t* coord,
                                      std::uint32_t extra) noexcept {
    const std::uint32_t inherited = source != nullptr ? loadU32(source + 0xDC) : 0u;
    auto* object = ptrAt(self, 8);
    storeU32(self + 0x40, inherited);
    storeU32(self + 0x44, 0);
    storeU32(self + 0x34, loadU32(coord));
    storeU32(self + 0x38, loadU32(coord + 4));
    storeU32(self + 0x3C, loadU32(coord + 8));
    self[0x55] = 1;
    self[0x54] = 1;
    if (extra != 0) {
        g_calls.projectileExtra(object, nullptr, extra);
    }
    const std::uint8_t* from = source != nullptr ? source + 0x38 : object + 0x38;
    storeU32(self + 0x28, loadU32(from));
    storeU32(self + 0x2C, loadU32(from + 4));
    storeU32(self + 0x30, loadU32(from + 8));
    const std::uint32_t next = loadU32(g_env.gameLogic + 0x58) + 1u;
    g_calls.stamp(bits(object), bits(self + 0x10), next);
    return call0(ptrAt(self, 0x50), 0x10);
}

}  // namespace ra3::gamelogic
