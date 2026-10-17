#include "ra3/gamelogic/AIState7A2.h"

#include <cstring>

namespace ra3::gamelogic {
namespace {

AIState7A2Calls g_calls{};

constexpr std::uint32_t kFail = 0xFFFFFFFEu;
constexpr std::uint32_t kNeg = 0xFFFFFFFFu;
constexpr std::uint32_t kIdNone = 0x7FFFFFFFu;

std::uint32_t loadU32(const void* address) noexcept {
    std::uint32_t value = 0;
    std::memcpy(&value, address, 4);
    return value;
}

void storeU32(void* address, std::uint32_t value) noexcept {
    std::memcpy(address, &value, 4);
}

std::uint32_t ptrBits(const void* pointer) noexcept {
    return static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(pointer));
}

std::uint8_t* asPtr(std::uint32_t bits) noexcept {
    return reinterpret_cast<std::uint8_t*>(static_cast<std::uintptr_t>(bits));
}

std::uint32_t absDword(std::uint32_t address) noexcept {
    return g_calls.absDword(address);
}

std::uint32_t direct(std::uint32_t address, void* ecx, const std::uint32_t* args, int nargs) noexcept {
    return g_calls.direct(address, ecx, args, nargs);
}

std::uint32_t direct0(std::uint32_t address, void* ecx) noexcept {
    return direct(address, ecx, nullptr, 0);
}

std::uint32_t direct1(std::uint32_t address, void* ecx, std::uint32_t a0) noexcept {
    return direct(address, ecx, &a0, 1);
}

std::uint32_t direct2(std::uint32_t address, void* ecx, std::uint32_t a0, std::uint32_t a1) noexcept {
    const std::uint32_t args[2] = {a0, a1};
    return direct(address, ecx, args, 2);
}

std::uint32_t direct3(std::uint32_t address, void* ecx, std::uint32_t a0, std::uint32_t a1,
                      std::uint32_t a2) noexcept {
    const std::uint32_t args[3] = {a0, a1, a2};
    return direct(address, ecx, args, 3);
}

std::uint32_t direct4(std::uint32_t address, void* ecx, std::uint32_t a0, std::uint32_t a1,
                      std::uint32_t a2, std::uint32_t a3) noexcept {
    const std::uint32_t args[4] = {a0, a1, a2, a3};
    return direct(address, ecx, args, 4);
}

std::uint32_t direct6(std::uint32_t address, void* ecx, std::uint32_t a0, std::uint32_t a1,
                      std::uint32_t a2, std::uint32_t a3, std::uint32_t a4, std::uint32_t a5) noexcept {
    const std::uint32_t args[6] = {a0, a1, a2, a3, a4, a5};
    return direct(address, ecx, args, 6);
}

template <typename Fn>
Fn slotFn(void* object, int slot) noexcept {
    auto* vt = *reinterpret_cast<std::uint8_t**>(object);
    return *reinterpret_cast<Fn*>(vt + slot);
}

std::uint32_t call0(void* object, int slot) noexcept {
    return slotFn<std::uint32_t(__fastcall*)(void*, void*)>(object, slot)(object, nullptr);
}

std::uint32_t call1(void* object, int slot, std::uint32_t a0) noexcept {
    return slotFn<std::uint32_t(__fastcall*)(void*, void*, std::uint32_t)>(object, slot)(object, nullptr, a0);
}

std::uint32_t call2(void* object, int slot, std::uint32_t a0, std::uint32_t a1) noexcept {
    return slotFn<std::uint32_t(__fastcall*)(void*, void*, std::uint32_t, std::uint32_t)>(object, slot)(
        object, nullptr, a0, a1);
}

std::uint32_t call3(void* object, int slot, std::uint32_t a0, std::uint32_t a1, std::uint32_t a2) noexcept {
    return slotFn<std::uint32_t(__fastcall*)(void*, void*, std::uint32_t, std::uint32_t, std::uint32_t)>(
        object, slot)(object, nullptr, a0, a1, a2);
}

std::uint8_t* machineOf(std::uint8_t* self) noexcept {
    return *reinterpret_cast<std::uint8_t**>(self + 0x18);
}

std::uint8_t* ownerOf(std::uint8_t* machine) noexcept {
    return *reinterpret_cast<std::uint8_t**>(machine + 0x24);
}

bool bit(std::uint32_t word, int shift) noexcept {
    return ((word >> shift) & 1u) != 0;
}

std::uint32_t publish(std::uint8_t* owner, std::uint32_t index, std::uint32_t value) noexcept {
    direct2(0x006A4F80, owner, index, value);
    return direct0(0x006A4FB0, owner);
}

std::uint32_t code2(std::uint8_t* owner, std::uint32_t code, std::uint32_t value) noexcept {
    return direct2(0x006D2020, owner, code, value);
}

std::uint32_t roundTripF32(std::uint32_t bits) noexcept {
    std::uint32_t rounded = bits;
    __asm {
        fld dword ptr rounded
        fstp dword ptr rounded
    }
    return rounded;
}

bool sseEqual(std::uint32_t left, std::uint32_t right) noexcept {
    if ((left & 0x7FFFFFFFu) > 0x7F800000u || (right & 0x7FFFFFFFu) > 0x7F800000u) {
        return false;
    }
    if (left == right) {
        return true;
    }
    return (left & 0x7FFFFFFFu) == 0 && (right & 0x7FFFFFFFu) == 0;
}

std::uint32_t withAl(std::uint32_t eax, std::uint32_t al) noexcept {
    return (eax & 0xFFFFFF00u) | (al & 0xFFu);
}

std::uint32_t xferVersion(std::uint8_t* xfer, std::uint8_t low, std::uint8_t high) noexcept {
    std::uint8_t bytes[2] = {low, high};
    const std::uint32_t result = call1(xfer, 0x18, ptrBits(bytes));
    // The stub may rewrite the version bytes; callers that test them read `bytes` after the call
    // only when the buffer is the one the stub received. Keep the locals alive by copying back
    // through the pointer the callee was given — the bytes array itself is that buffer.
    (void)bytes;
    return result;
}

bool xferGated(std::uint8_t* xfer) noexcept {
    return bit(loadU32(xfer + 4), 3);
}

std::uint32_t xfer1(std::uint8_t* xfer, int slot, const void* field) noexcept {
    return call1(xfer, slot, ptrBits(field));
}

std::uint32_t goalMaxBits() noexcept {
    return roundTripF32(absDword(0x00BE700C));
}

void aimAtGoal(std::uint8_t* self, std::uint8_t* sub) noexcept {
    auto* machine = machineOf(self);
    direct2(0x005AC4F0, sub, ptrBits(machine + 0x2C), goalMaxBits());
}

std::uint32_t finishSub(std::uint8_t* sub) noexcept {
    return call1(sub, 0x1C, loadU32(sub + 0x68));
}

std::uint8_t* makeSub(std::uint8_t* self) noexcept {
    auto* sub = asPtr(call0(self, 0x4C));
    storeU32(self + 0x20, ptrBits(sub));
    return sub;
}

}  // namespace

void setAIState7A2Calls(const AIState7A2Calls& calls) noexcept {
    g_calls = calls;
}

std::uint32_t AIAttackPositionFireWeaponState_onEnter(std::uint8_t* self) noexcept {
    auto* machine = machineOf(self);
    auto* owner = ownerOf(machine);
    auto* loco = *reinterpret_cast<std::uint8_t**>(owner + 0x374);
    self[0x24] = 0;
    code2(owner, 0x55, 0);
    const std::uint32_t target = direct1(0x006A51A0, owner, kNeg);
    if (target == 0) {
        return kFail;
    }
    auto* targetPtr = asPtr(target);
    auto* templ = *reinterpret_cast<std::uint8_t**>(owner + 4);
    if (!bit(loadU32(templ + 0xC0), 4)) {
        auto* info = *reinterpret_cast<std::uint8_t**>(targetPtr + 4);
        if ((direct0(0x00685EE0, info) & 0xFFu) == 0 && (direct0(0x00685EC0, info) & 0xFFu) == 0 &&
            (direct1(0x006FFB00, loco, 0) & 0xFFu) != 0) {
            return kFail;
        }
    }
    templ = *reinterpret_cast<std::uint8_t**>(owner + 4);
    if (bit(loadU32(templ + 0xCC), 10)) {
        auto* global = asPtr(absDword(0));
        using Slot0 = std::uint32_t(__fastcall*)(void*, void*, std::uint32_t, std::uint32_t);
        auto fn = *reinterpret_cast<Slot0*>(global);
        fn(nullptr, nullptr, ptrBits(machineOf(self) + 0x2C), 0);
    }
    auto* aim = machineOf(self) + 0x2C;
    const std::uint32_t again = direct1(0x006A51A0, owner, kNeg);
    if ((direct3(0x007A0B30, owner, ptrBits(owner), ptrBits(aim), again) & 0xFFu) == 0) {
        return kFail;
    }
    if (direct0(0x00688810, targetPtr) != 0) {
        return kNeg;
    }
    if (bit(loadU32(owner + 0x84), 8)) {
        auto* game = asPtr(absDword(0x00DEBA64));
        if ((game[0x58] & 1u) != 0) {
            self[0x24] = 1;
            return 0;
        }
    }
    code2(owner, 0x0E, 1);
    return 0;
}

std::uint32_t AIAttackPositionFireWeaponState_update(std::uint8_t* self) noexcept {
    auto* owner = ownerOf(machineOf(self));
    const std::uint32_t target = direct1(0x006A51A0, owner, kNeg);
    if (target == 0 || (owner[0x471] & 1u) != 0 || bit(loadU32(owner + 0x88), 0x15)) {
        return kFail;
    }
    if (self[0x24] != 0) {
        self[0x24] = 0;
        code2(owner, 0x0E, 1);
        return 0;
    }
    const std::uint32_t busy = direct0(0x00688810, asPtr(target));
    if (busy == 1) {
        return 0;
    }
    if (busy != 0) {
        return kFail;
    }
    code2(owner, 0x1D, busy);
    return call0(*reinterpret_cast<std::uint8_t**>(self + 0x20), 0) | kNeg;
}

std::uint32_t AIGuardIdleState_update(std::uint8_t* self) noexcept {
    auto* owner = ownerOf(machineOf(self));
    const std::uint32_t found = direct0(0x006A6210, owner);
    if (found != 0 && (direct0(0x006A4F60, owner) & 0xFFu) != 0) {
        const std::uint32_t anim = direct0(0x00687CB0, owner);
        if (anim != 0 && direct0(0x00A780D0, asPtr(anim)) == 1) {
            return kNeg;
        }
        auto* loco = *reinterpret_cast<std::uint8_t**>(owner + 0x374);
        if (call0(loco, 0x244) != 1) {
            return kNeg;
        }
        const std::uint32_t clip = call1(loco, 0x288, 0);
        // Slot 0x288 is cdecl here, so the pushed 0 remains as 6906a0's last argument.
        if (direct4(0x006906A0, asPtr(absDword(0x00CC561C)), ptrBits(owner), found, clip, 0) == 3) {
            return kFail;
        }
    }
    if (self[0x20] == 0) {
        self[0x20] = 1;
        if ((direct0(0x006A6F60, owner) & 0xFFu) == 0) {
            const std::uint32_t where = direct0(0x00687C30, owner);
            if (where != 0) {
                std::uint8_t local[12]{};
                if ((direct1(0x006AD4B0, asPtr(where), ptrBits(local)) & 0xFFu) != 0) {
                    auto* machine = machineOf(self);
                    call1(machine, 0x38, 0);
                    direct1(0x005AD150, machine, ptrBits(local));
                    call1(machine, 0x1C, 5);
                }
            }
        }
    }
    return 0;
}

std::uint32_t AIHarvestWorkSiteState_onEnter(std::uint8_t* self) noexcept {
    auto* machine = machineOf(self);
    auto* owner = ownerOf(machine);
    auto* loco = *reinterpret_cast<std::uint8_t**>(owner + 0x374);
    const std::uint32_t siteBits = call0(loco, 0x1C4);
    if (siteBits == 0) {
        return kFail;
    }
    auto* site = asPtr(siteBits);
    const std::uint32_t goal = direct0(0x005B9F30, machine);
    if (goal != 0) {
        const std::uint32_t record = direct1(0x0067F130, machine, goal);
        if (record != 0) {
            if (loadU32(asPtr(record) + 0x38) != 0) {
                return kFail;
            }
            auto* rec = asPtr(record);
            storeU32(self + 0x34, loadU32(asPtr(goal) + 0xDC));
            direct1(0x006D8A20, rec, loadU32(owner + 0xDC));
            auto* game = asPtr(absDword(0x00DEBA64));
            const std::uint32_t frame = loadU32(game + 0x58);
            storeU32(self + 0x20, call0(site, 0x48) + frame);
            storeU32(self + 0x24, frame);
            const auto span = static_cast<std::int32_t>(call0(site, 0x0C) - call0(site, 0x04));
            const auto cap = static_cast<std::int32_t>(loadU32(rec + 0x30));
            storeU32(self + 0x28, static_cast<std::uint32_t>(span < cap ? span : cap));
        }
    }
    owner = ownerOf(machineOf(self));
    if (!bit(loadU32(owner + 0xB4), 0x1F)) {
        storeU32(owner + 0xB4, loadU32(owner + 0xB4) | 0x80000000u);
        publish(owner, 0xBF, 1);
    }
    code2(ownerOf(machineOf(self)), 0x8B, 1);
    return 0;
}

std::uint32_t AIHordeEnterState_onEnter(std::uint8_t* self) noexcept {
    auto* machine = machineOf(self);
    auto* owner = ownerOf(machine);
    const std::uint32_t goal = direct0(0x005B9F30, machine);
    if (owner == nullptr || goal == 0 || (asPtr(goal)[0x471] & 1u) != 0) {
        return kFail;
    }
    const std::uint32_t phys = direct0(0x006A7600, owner);
    if (phys == 0) {
        return kFail;
    }
    auto* body = asPtr(phys);
    const std::uint32_t weapon = call0(body, 0x84);
    if (weapon == 0) {
        return kFail;
    }
    const std::uint32_t other = direct2(0x006A7620, asPtr(goal), ptrBits(owner), 1);
    if (other == 0) {
        return kFail;
    }
    auto* otherBody = asPtr(other);
    if (call0(otherBody, 0x84) != 0) {
        return kFail;
    }
    if ((call3(otherBody, 0x94, ptrBits(owner), 1, 0) & 0xFFu) == 0) {
        return kFail;
    }
    auto* goalPtr = asPtr(goal);
    call3(asPtr(weapon), 0x7C, ptrBits(goalPtr + 0x38), 2, ptrBits(goalPtr));
    auto* info = *reinterpret_cast<std::uint8_t**>(goalPtr + 4);
    if (!bit(loadU32(info + 0xC0), 4)) {
        direct1(0x0077B1F0, goalPtr, 8);
    }
    call1(otherBody, 0x98, ptrBits(owner));
    call2(otherBody, 0x40, ptrBits(owner), 0);
    return 0;
}

std::uint32_t AIGoingIdleState_onEnter(std::uint8_t* self) noexcept {
    auto* owner = ownerOf(machineOf(self));
    if (owner == nullptr) {
        return kFail;
    }
    const std::uint32_t where = direct0(0x00687C30, owner);
    if (where != 0) {
        std::uint8_t local[12]{};
        if ((direct1(0x006AD4B0, asPtr(where), ptrBits(local)) & 0xFFu) != 0) {
            auto* machine = machineOf(self);
            call1(machine, 0x38, 0);
            direct1(0x005AD150, machine, ptrBits(local));
            call1(machine, 0x1C, 0x4E);
            return 0;
        }
    }
    const std::uint32_t anim = direct0(0x00687CB0, owner);
    if (anim != 0) {
        direct0(0x006AF030, asPtr(anim));
    }
    return kFail;
}

std::uint32_t AIWaitUntilFinishedFiringState_onEnter(std::uint8_t* self) noexcept {
    auto* owner = ownerOf(machineOf(self));
    const std::uint32_t target = direct1(0x006A51A0, owner, kNeg);
    owner = ownerOf(machineOf(self));
    if (target == 0) {
        return kFail;
    }
    auto* targetPtr = asPtr(target);
    if (direct0(0x00688810, targetPtr) == 0) {
        if (bit(loadU32(owner + 0x84), 8)) {
            auto* game = asPtr(absDword(0x00DEBA64));
            if ((game[0x58] & 1u) != 0) {
                return 0;
            }
        }
        return kNeg;
    }
    auto* info = *reinterpret_cast<std::uint8_t**>(targetPtr + 4);
    const std::uint32_t delay = direct0(0x006864D0, info);
    if (static_cast<std::int32_t>(delay) > 0) {
        const std::uint32_t frame = loadU32(asPtr(absDword(0x00DEBA64)) + 0x58);
        const std::uint32_t again = direct0(0x006864D0, info);
        direct2(0x00757B40, owner, 8, again + frame);
    }
    direct0(0x00686460, info);
    return 0;
}

std::uint32_t AIPrepareForBoarding_onEnter(std::uint8_t* self) noexcept {
    auto* machine = machineOf(self);
    auto* owner = ownerOf(machine);
    std::uint8_t local[12]{};
    const std::uint32_t placed =
        direct3(0x0063EB60, local, ptrBits(owner + 0x38), ptrBits(owner), ptrBits(local));
    const std::uint32_t goal = direct0(0x005B9F30, machine);
    call1(machine, 0x38, 0);
    if ((placed & 0xFFu) == 0) {
        return kNeg;
    }
    if (goal == 0) {
        return kFail;
    }
    std::uint8_t probe[4]{};
    const std::uint32_t fit = direct6(0x00658A80, probe, goal, ptrBits(asPtr(goal) + 0x38), ptrBits(probe), 0, 0, 0);
    if ((fit & 0xFFu) == 0) {
        return kNeg;
    }
    return kFail;
}

std::uint32_t AIFollowExitProductionPathState_onEnter(std::uint8_t* self) noexcept {
    auto* owner = ownerOf(machineOf(self));
    auto* loco = *reinterpret_cast<std::uint8_t**>(owner + 0x374);
    code2(owner, 0x5D, 1);
    storeU32(self + 0x24, loadU32(loco + 0x218));
    loco[0x434] = 1;
    if ((direct1(0x007527C0, loco, 0x11) & 0xFFu) == 0) {
        storeU32(self + 0x24, kNeg);
    }
    auto* sub = asPtr(call0(self, 0x4C));
    storeU32(self + 0x20, ptrBits(sub));
    direct1(0x005BDCC0, sub, ptrBits(machineOf(self) + 0x58));
    direct1(0x0079ECE0, self, ptrBits(sub));
    return finishSub(sub);
}

std::uint32_t UnitAIReloadAmmoState_onEnter(std::uint8_t* self) noexcept {
    auto* owner = ownerOf(machineOf(self));
    code2(owner, 0xCF, 0);
    if (bit(loadU32(owner + 0xB4), 5)) {
        storeU32(owner + 0xB4, loadU32(owner + 0xB4) & 0xFFFFFFDFu);
        publish(owner, 0xA5, 0);
    }
    auto* ammo = *reinterpret_cast<std::uint8_t**>(owner + 0x37C);
    if (ammo != nullptr) {
        const std::uint32_t count = direct0(0x006CFCA0, ammo);
        storeU32(self + 0x20, count);
        storeU32(self + 0x24, count + loadU32(asPtr(absDword(0x00DEBA64)) + 0x58));
    }
    return 0;
}

std::uint32_t UnitAIMoveOutOfTheWayState_onEnter(std::uint8_t* self) noexcept {
    auto* sub = asPtr(call0(self, 0x4C));
    storeU32(self + 0x20, ptrBits(sub));
    const std::uint32_t goal = direct0(0x005B9F30, machineOf(self));
    call1(sub, 0x38, goal);
    auto* owner = ownerOf(machineOf(self));
    if (!bit(loadU32(owner + 0xB0), 0x16)) {
        storeU32(owner + 0xB0, loadU32(owner + 0xB0) | 0x00400000u);
        publish(owner, 0x96, 1);
    }
    return finishSub(*reinterpret_cast<std::uint8_t**>(self + 0x20));
}

std::uint32_t JetAILandingApproachState_onEnter(std::uint8_t* self) noexcept {
    auto* sub = makeSub(self);
    auto* machine = machineOf(self);
    auto* owner = ownerOf(machine);
    const std::uint32_t spot = direct2(0x006E4760, self, loadU32(machine + 0x28), 0);
    if (spot != 0) {
        auto* spotPtr = asPtr(spot);
        if ((call1(spotPtr, 0x3C, ptrBits(owner)) & 0xFFu) != 0) {
            finishSub(sub);
            return kNeg;
        }
        auto* loco = *reinterpret_cast<std::uint8_t**>(owner + 0x374);
        if (loco != nullptr) {
            auto* templ = *reinterpret_cast<std::uint8_t**>(loco + 4);
            const std::uint32_t height = roundTripF32(loadU32(templ + 0x54));
            std::uint8_t out[12]{};
            const std::uint32_t dc = loadU32(owner + 0xDC);
            if ((call3(spotPtr, 0x14, dc, height, ptrBits(out)) & 0xFFu) != 0) {
                direct2(0x005AC4F0, sub, ptrBits(out), goalMaxBits());
                sub[0x7C] = 0;
            }
        }
    }
    call0(self, 0x3C);
    auto* loco = *reinterpret_cast<std::uint8_t**>(ownerOf(machineOf(self)) + 0x374);
    auto* module = *reinterpret_cast<std::uint8_t**>(loco + 0x214);
    if (module != nullptr) {
        storeU32(module + 0x40, loadU32(module + 0x40) & 0xFFFFFFEFu);
    }
    return finishSub(*reinterpret_cast<std::uint8_t**>(self + 0x20));
}

std::uint32_t AIPlayer_offerTeam(std::uint8_t* self, std::uint32_t team) noexcept {
    auto* teamPtr = asPtr(team);
    const std::uint32_t ready = direct0(0x007EBE30, teamPtr);
    if ((ready & 0xFFu) == 0) {
        return withAl(ready, 0);
    }
    const std::uint32_t count = direct0(0x007E6AF0, teamPtr);
    if (static_cast<std::int32_t>(count) >= static_cast<std::int32_t>(loadU32(teamPtr + 0x28C))) {
        return withAl(count, 0);
    }
    auto* sentinel = self + 0x88;
    for (auto* node = *reinterpret_cast<std::uint8_t**>(sentinel); node != sentinel;
         node = *reinterpret_cast<std::uint8_t**>(node)) {
        auto* held = *reinterpret_cast<std::uint8_t**>(node + 8);
        auto* inner = *reinterpret_cast<std::uint8_t**>(held + 0x0C);
        if (loadU32(inner + 4) == team) {
            return withAl(ptrBits(node), 0);
        }
    }
    const std::uint32_t accepted = direct1(0x007A1570, self, team);
    if ((accepted & 0xFFu) != 0) {
        return withAl(accepted, 1);
    }
    std::uint32_t local = 0;
    const std::uint32_t queued = direct3(0x007A1400, self, team, 1, ptrBits(&local));
    if ((queued & 0xFFu) == 0) {
        return withAl(queued, 0);
    }
    return withAl(queued, 1);
}

std::uint32_t UnitAICombinedUnitIdleState_onEnter(std::uint8_t* self) noexcept {
    auto* machine = machineOf(self);
    auto* owner = ownerOf(machine);
    const std::uint32_t goal = direct0(0x005B9F30, machine);
    if (goal == 0) {
        return kFail;
    }
    code2(owner, 0x3F, 1);
    auto* formation = *reinterpret_cast<std::uint8_t**>(owner + 0x33C);
    call1(formation, 0x94, 1);
    code2(owner, 0x84, 1);
    code2(asPtr(goal), 0x83, 1);
    if (!bit(loadU32(owner + 0xD4), 4)) {
        storeU32(owner + 0xD4, loadU32(owner + 0xD4) | 0x10u);
        publish(owner, 0x1A4, 1);
    }
    auto* goalPtr = asPtr(goal);
    if (!bit(loadU32(goalPtr + 0xD4), 3)) {
        storeU32(goalPtr + 0xD4, loadU32(goalPtr + 0xD4) | 8u);
        publish(goalPtr, 0x1A3, 1);
    }
    code2(owner, 0x85, 1);
    return 0;
}

std::uint32_t UnitAIEnterGoalState_onEnter(std::uint8_t* self) noexcept {
    auto* machine = machineOf(self);
    const std::uint32_t goal = direct0(0x005B9F30, machine);
    if (goal == 0) {
        return kFail;
    }
    auto* owner = ownerOf(machine);
    std::uint8_t local[12]{};
    direct1(0x006A76A0, asPtr(goal), ptrBits(local));
    auto* loco = *reinterpret_cast<std::uint8_t**>(owner + 0x374);
    const std::uint32_t mode = direct0(0x0079EAD0, self);
    direct1(0x006E6360, loco, mode);
    auto* module = *reinterpret_cast<std::uint8_t**>(loco + 0x214);
    if (module != nullptr) {
        storeU32(module + 0x40, loadU32(module + 0x40) | 2u);
    }
    auto* sub = asPtr(call0(self, 0x4C));
    storeU32(self + 0x20, ptrBits(sub));
    direct2(0x005AC4F0, sub, ptrBits(local), goalMaxBits());
    call1(sub, 0x38, goal);
    sub[0x7C] = 0;
    code2(owner, 0x61, 1);
    return finishSub(sub);
}

std::uint32_t UnitAIParkOrientState_onEnter(std::uint8_t* self) noexcept {
    auto* machine = machineOf(self);
    auto* owner = ownerOf(machine);
    auto* templ = *reinterpret_cast<std::uint8_t**>(owner + 4);
    if (bit(loadU32(templ + 0xC8), 8)) {
        return kNeg;
    }
    if (bit(loadU32(owner + 0xB4), 5)) {
        storeU32(owner + 0xB4, loadU32(owner + 0xB4) & 0xFFFFFFDFu);
        publish(owner, 0xA5, 0);
    }
    code2(owner, 0xCF, 1);
    if ((direct0(0x0079CC50, self) & 0xFFu) == 0) {
        return kFail;
    }
    if (sseEqual(loadU32(machine + 0x54), absDword(0x00BE700C))) {
        return kNeg;
    }
    return direct0(0x007985D0, self);
}

std::uint32_t UnitAIParkOrientState_onExit(std::uint8_t* self, std::uint32_t) noexcept {
    auto* owner = ownerOf(machineOf(self));
    code2(owner, 0xCF, 0);
    if (bit(loadU32(owner + 0xB4), 5)) {
        storeU32(owner + 0xB4, loadU32(owner + 0xB4) & 0xFFFFFFDFu);
        publish(owner, 0xA5, 0);
    }
    owner = ownerOf(machineOf(self));
    auto* loco = *reinterpret_cast<std::uint8_t**>(owner + 0x374);
    if (loco != nullptr) {
        storeU32(loco + 0x1F0, absDword(0x00BE700C));
        if (loadU32(loco + 0x21C) == 3) {
            storeU32(loco + 0x21C, 0);
        }
        return ptrBits(loco);
    }
    return ptrBits(owner);
}

std::uint32_t UnitAILandState_update(std::uint8_t* self) noexcept {
    auto* machine = machineOf(self);
    auto* owner = ownerOf(machine);
    if (self[0x3C] != 0) {
        std::uint32_t pad = 0;
        const std::uint32_t spot = direct2(0x006E4760, &pad, loadU32(machine + 0x28), ptrBits(&pad));
        if (spot != 0 && pad != 0) {
            auto* padInfo = *reinterpret_cast<std::uint8_t**>(asPtr(pad) + 4);
            if (!bit(loadU32(padInfo + 0xC0), 4) && (direct0(0x00687560, asPtr(pad)) & 0xFFu) != 0) {
                auto* loco = *reinterpret_cast<std::uint8_t**>(owner + 0x374);
                auto* templ = *reinterpret_cast<std::uint8_t**>(loco + 4);
                const std::uint32_t height = roundTripF32(loadU32(templ + 0x54));
                std::uint32_t out[3]{};
                if ((call3(asPtr(spot), 0x14, loadU32(owner + 0xDC), height, ptrBits(out)) & 0xFFu) != 0) {
                    storeU32(self + 0x2C, out[0]);
                    storeU32(self + 0x30, out[1]);
                    storeU32(self + 0x34, out[2]);
                }
            }
        }
    }
    return direct0(0x0079CEE0, self);
}

std::uint32_t UnitAITakeOffState_update(std::uint8_t* self) noexcept {
    auto* owner = ownerOf(machineOf(self));
    const std::uint32_t index = loadU32(self + 0x38);
    auto* sample = self + 0x20 + index * 12;
    direct3(0x00686D90, owner, ptrBits(sample), 1, 0);
    const std::uint32_t status = direct0(0x0079CEE0, self);
    if (status == kNeg && !bit(loadU32(owner + 0xB4), 4)) {
        storeU32(owner + 0xB4, loadU32(owner + 0xB4) | 0x10u);
        publish(owner, 0xA4, 1);
    }
    return status;
}

std::uint32_t AITunnelNetworkGuardState_update(std::uint8_t* self) noexcept {
    if (loadU32(self + 0x20) == 0) {
        return kFail;
    }
    auto* owner = ownerOf(machineOf(self));
    if ((direct0(0x006D21B0, owner) & 0xFFu) != 0) {
        auto* templ = *reinterpret_cast<std::uint8_t**>(owner + 4);
        if (!bit(loadU32(templ + 0xC0), 0x1A)) {
            return kFail;
        }
    }
    direct1(0x005AA240, machineOf(self), 0x00C2FF28u);
    auto* sub = *reinterpret_cast<std::uint8_t**>(self + 0x20);
    const std::uint32_t result = call0(sub, 0x10);
    direct0(0x005AA250, machineOf(self));
    return result;
}

std::uint32_t UnitAIMoveOnPathState_onExit(std::uint8_t* self, std::uint32_t) noexcept {
    auto* owner = ownerOf(machineOf(self));
    const bool wide = bit(loadU32(owner + 0x90), 0x1D);
    std::uint32_t eax = 0;
    if (!wide && !bit(loadU32(owner + 0x88), 0x0E)) {
        eax = direct1(0x004D9AB0, owner, 0x8F);
    }
    if (bit(loadU32(owner + 0xB0), 0x1D)) {
        storeU32(owner + 0xB0, loadU32(owner + 0xB0) & 0xDFFFFFFFu);
        eax = publish(owner, 0x9D, 0);
    }
    if (bit(loadU32(owner + 0xB0), 0x1E)) {
        storeU32(owner + 0xB0, loadU32(owner + 0xB0) & 0xBFFFFFFFu);
        eax = publish(owner, 0x9E, 0);
    }
    if (bit(loadU32(owner + 0xB0), 0x1F)) {
        storeU32(owner + 0xB0, loadU32(owner + 0xB0) & 0x7FFFFFFFu);
        eax = publish(owner, 0x9F, 0);
    }
    auto* reg = asPtr(absDword(0x00DF03FC));
    if (reg != nullptr) {
        const std::uint32_t phase = loadU32(self + 0x20);
        if (phase >= 5) {
            eax = call1(reg, 0x6C, phase);
            storeU32(self + 0x20, 1);
        }
    }
    auto* loco = *reinterpret_cast<std::uint8_t**>(owner + 0x374);
    if (loco != nullptr) {
        if (!wide) {
            direct2(0x006B7040, loco, 0x00BF21D8u, 0);
            eax = direct0(0x00682D70, loco);
        }
        auto* module = *reinterpret_cast<std::uint8_t**>(loco + 0x214);
        if (module != nullptr) {
            if (module[0xA4] != 0) {
                module[0xA4] = 0;
            }
            eax = direct1(0x006A4500, module, roundTripF32(absDword(0x00BD9628)));
        }
        storeU32(loco + 0x17C, 0);
    }
    return eax;
}

std::uint32_t UnitAIParachuteToGroundState_onEnter(std::uint8_t* self) noexcept {
    auto* sub = makeSub(self);
    auto* owner = ownerOf(machineOf(self));
    if (bit(loadU32(owner + 0xA0), 0x13)) {
        storeU32(owner + 0xA0, loadU32(owner + 0xA0) & 0xFFF7FFFFu);
        publish(owner, 0x13, 0);
    }
    code2(owner, 8, 1);
    code2(owner, 0x4D, 1);
    code2(owner, 5, 1);
    direct0(0x006870A0, *reinterpret_cast<std::uint8_t**>(owner + 0xF4));
    return finishSub(sub);
}

std::uint32_t UnitAIAttackMoveToGoalState_onEnter(std::uint8_t* self) noexcept {
    auto* owner = ownerOf(machineOf(self));
    auto* sub = makeSub(self);
    aimAtGoal(self, sub);
    direct1(0x005AA280, sub, roundTripF32(loadU32(machineOf(self) + 0x54)));
    if (!bit(loadU32(owner + 0xB0), 0x19)) {
        storeU32(owner + 0xB0, loadU32(owner + 0xB0) | 0x02000000u);
        publish(owner, 0x99, 1);
    }
    return finishSub(*reinterpret_cast<std::uint8_t**>(self + 0x20));
}

std::uint32_t UnitAIAttackPositionState_onEnter(std::uint8_t* self) noexcept {
    auto* owner = ownerOf(machineOf(self));
    auto* sub = makeSub(self);
    aimAtGoal(self, sub);
    if ((owner[0xA8] & 1u) == 0) {
        storeU32(owner + 0xA8, loadU32(owner + 0xA8) | 1u);
        publish(owner, 0x40, 1);
    }
    if (!bit(loadU32(owner + 0xA8), 2)) {
        storeU32(owner + 0xA8, loadU32(owner + 0xA8) | 4u);
        publish(owner, 0x42, 1);
    }
    return finishSub(sub);
}

std::uint32_t JetAIAttackMoveToGoalState_onEnter(std::uint8_t* self) noexcept {
    auto* sub = makeSub(self);
    aimAtGoal(self, sub);
    direct1(0x005AA280, sub, roundTripF32(loadU32(machineOf(self) + 0x54)));
    sub[0x7C] = machineOf(self)[0x7C];
    auto* owner = ownerOf(machineOf(self));
    if (!bit(loadU32(owner + 0xB0), 0x19)) {
        storeU32(owner + 0xB0, loadU32(owner + 0xB0) | 0x02000000u);
        publish(owner, 0x99, 1);
    }
    return finishSub(sub);
}

std::uint32_t AIInternalMoveToState_xfer(std::uint8_t* self, std::uint8_t* xfer) noexcept {
    xferVersion(xfer, 1, 1);
    xfer1(xfer, 0x58, self + 0x20);
    direct2(0x00549D50, xfer, ptrBits(xfer), ptrBits(self + 0x30));
    xfer1(xfer, 0x84, self + 0x4D);
    xfer1(xfer, 0x58, self + 0x34);
    xfer1(xfer, 0x6C, self + 0x48);
    xfer1(xfer, 0x84, self + 0x4C);
    xferVersion(xfer, 1, 1);
    auto* reg = asPtr(absDword(0x00DF03FC));
    if (reg != nullptr) {
        call2(reg, 0x158, ptrBits(xfer), ptrBits(self + 0x44));
    }
    xfer1(xfer, 0x84, self + 0x4F);
    xfer1(xfer, 0x64, self + 0x2C);
    xfer1(xfer, 0x84, self + 0x4E);
    return xfer1(xfer, 0x84, self + 0x40);
}

std::uint32_t AIFollowWaypointPathState_xfer(std::uint8_t* self, std::uint8_t* xfer) noexcept {
    xferVersion(xfer, 1, 1);
    direct1(0x007A1FC0, self, ptrBits(xfer));
    if (xferGated(xfer)) {
        return loadU32(xfer + 4) >> 3;
    }
    xfer1(xfer, 0x44, self + 0x50);
    xfer1(xfer, 0x64, self + 0x58);
    xfer1(xfer, 0x70, self + 0x5C);
    std::uint32_t local = kIdNone;
    if (const std::uint32_t cur = loadU32(self + 0x60)) {
        local = loadU32(asPtr(cur) + 4);
    }
    xfer1(xfer, 0x6C, &local);
    if (bit(loadU32(xfer + 4), 1)) {
        storeU32(self + 0x60, direct1(0x008663B0, asPtr(absDword(0x00DFC190)), local));
    }
    local = kIdNone;
    if (const std::uint32_t cur = loadU32(self + 0x64)) {
        local = loadU32(asPtr(cur) + 4);
    }
    xfer1(xfer, 0x6C, &local);
    if (bit(loadU32(xfer + 4), 1)) {
        storeU32(self + 0x64, direct1(0x008663B0, asPtr(absDword(0x00DFC190)), local));
    }
    return xfer1(xfer, 0x84, self + 0x68);
}

std::uint32_t AIAttackApproachTargetState_xfer(std::uint8_t* self, std::uint8_t* xfer) noexcept {
    xferVersion(xfer, 1, 1);
    direct1(0x007A1FC0, self, ptrBits(xfer));
    if (xferGated(xfer)) {
        return loadU32(xfer + 4) >> 3;
    }
    xfer1(xfer, 0x58, self + 0x50);
    xfer1(xfer, 0x6C, self + 0x68);
    xfer1(xfer, 0x84, self + 0x70);
    xfer1(xfer, 0x84, self + 0x71);
    xfer1(xfer, 0x84, self + 0x72);
    xfer1(xfer, 0x84, self + 0x73);
    xfer1(xfer, 0x58, self + 0x5C);
    xfer1(xfer, 0x6C, self + 0x6C);
    xfer1(xfer, 0x84, self + 0x75);
    return xfer1(xfer, 0x84, self + 0x74);
}

std::uint32_t AIAttackPositionApproachTargetState_xfer(std::uint8_t* self, std::uint8_t* xfer) noexcept {
    std::uint8_t version[2] = {1, 2};
    call1(xfer, 0x18, ptrBits(version));
    direct1(0x007A1FC0, self, ptrBits(xfer));
    if (xferGated(xfer)) {
        return loadU32(xfer + 4) >> 3;
    }
    xfer1(xfer, 0x6C, self + 0x5C);
    xfer1(xfer, 0x84, self + 0x64);
    xfer1(xfer, 0x84, self + 0x65);
    xfer1(xfer, 0x58, self + 0x50);
    xfer1(xfer, 0x6C, self + 0x60);
    std::uint32_t eax = xfer1(xfer, 0x84, self + 0x66);
    if (version[1] < 2) {
        return eax;
    }
    return xfer1(xfer, 0x84, self + 0x67);
}

std::uint32_t AIAttackPursueTargetState_xfer(std::uint8_t* self, std::uint8_t* xfer) noexcept {
    xferVersion(xfer, 1, 1);
    direct1(0x007A1FC0, self, ptrBits(xfer));
    if (xferGated(xfer)) {
        return loadU32(xfer + 4) >> 3;
    }
    xfer1(xfer, 0x58, self + 0x50);
    xfer1(xfer, 0x6C, self + 0x5C);
    xfer1(xfer, 0x84, self + 0x60);
    xfer1(xfer, 0x84, self + 0x61);
    xfer1(xfer, 0x84, self + 0x62);
    xfer1(xfer, 0x84, self + 0x63);
    return xfer1(xfer, 0x84, self + 0x64);
}

std::uint32_t AIGarrisonedState_xfer(std::uint8_t* self, std::uint8_t* xfer) noexcept {
    xferVersion(xfer, 1, 1);
    std::uint8_t flag = loadU32(self + 0x20) != 0 ? 1 : 0;
    std::uint32_t eax = xfer1(xfer, 0x84, &flag);
    if (flag != 0 && loadU32(self + 0x20) == 0) {
        const std::uint32_t block = direct1(0x00416820, xfer, 0x80);
        if (block == 0) {
            eax = 0;
        } else {
            eax = direct1(0x007A1BD0, asPtr(block), ptrBits(ownerOf(machineOf(self))));
        }
        storeU32(self + 0x20, eax);
    }
    if (loadU32(self + 0x20) == 0) {
        return eax;
    }
    call1(xfer, 0x04, 0x00C2F360u);
    xfer1(xfer, 0x20, asPtr(loadU32(self + 0x20)));
    return call0(xfer, 0x08);
}

std::uint32_t AIAttackMoveToState_xfer(std::uint8_t* self, std::uint8_t* xfer) noexcept {
    xferVersion(xfer, 1, 1);
    direct1(0x007A1FC0, self, ptrBits(xfer));
    if (xferGated(xfer)) {
        return loadU32(xfer + 4) >> 3;
    }
    xfer1(xfer, 0x6C, self + 0x5C);
    xfer1(xfer, 0x70, self + 0x60);
    xfer1(xfer, 0x20, asPtr(loadU32(self + 0x58)));
    xfer1(xfer, 0x58, self + 0x64);
    direct2(0x0054A090, xfer, ptrBits(xfer), ptrBits(self + 0x70));
    return direct2(0x00549C70, xfer, ptrBits(xfer), ptrBits(self + 0x54));
}

std::uint32_t AIAttackFireDuringApproachState_xfer(std::uint8_t* self, std::uint8_t* xfer) noexcept {
    xferVersion(xfer, 1, 1);
    direct1(0x007A1FC0, self, ptrBits(xfer));
    if (xferGated(xfer)) {
        return loadU32(xfer + 4) >> 3;
    }
    xfer1(xfer, 0x58, self + 0x54);
    xfer1(xfer, 0x40, self + 0x60);
    xfer1(xfer, 0x6C, self + 0x50);
    direct2(0x0054A090, xfer, ptrBits(xfer), ptrBits(self + 0x68));
    return xfer1(xfer, 0x84, self + 0x6C);
}

std::uint32_t AIFollowWaypointPathExactState_xfer(std::uint8_t* self, std::uint8_t* xfer) noexcept {
    xferVersion(xfer, 1, 1);
    direct1(0x007A1FC0, self, ptrBits(xfer));
    if (xferGated(xfer)) {
        return loadU32(xfer + 4) >> 3;
    }
    std::uint32_t local = kIdNone;
    if (const std::uint32_t cur = loadU32(self + 0x50)) {
        local = loadU32(asPtr(cur) + 4);
    }
    std::uint32_t eax = xfer1(xfer, 0x6C, &local);
    if (bit(loadU32(xfer + 4), 1)) {
        eax = direct1(0x008663B0, asPtr(absDword(0x00DFC190)), local);
        storeU32(self + 0x50, eax);
        return eax;
    }
    return loadU32(xfer + 4) >> 1;
}

std::uint32_t AITNGuardReturnState_xfer(std::uint8_t* self, std::uint8_t* xfer) noexcept {
    xferVersion(xfer, 1, 1);
    direct1(0x007A1FC0, self, ptrBits(xfer));
    direct2(0x0054A090, self, ptrBits(xfer), ptrBits(self + 0x50));
    std::uint32_t eax = xfer1(xfer, 0x6C, self + 0x54);
    if (!bit(loadU32(xfer + 4), 3)) {
        xferVersion(xfer, 1, 1);
        eax = xfer1(xfer, 0x6C, self + 0x58);
    } else {
        eax = loadU32(xfer + 4) >> 3;
    }
    return eax;
}

std::uint32_t AIBackAwayState_xfer(std::uint8_t* self, std::uint8_t* xfer) noexcept {
    xferVersion(xfer, 1, 1);
    xfer1(xfer, 0x84, self + 0x58);
    xferVersion(xfer, 1, 1);
    direct1(0x007A1FC0, self, ptrBits(xfer));
    xfer1(xfer, 0x70, self + 0x50);
    return xfer1(xfer, 0x84, self + 0x54);
}

std::uint32_t AIFollowPathState_xfer(std::uint8_t* self, std::uint8_t* xfer) noexcept {
    xferVersion(xfer, 1, 1);
    direct1(0x007A1FC0, self, ptrBits(xfer));
    if (xferGated(xfer)) {
        return loadU32(xfer + 4) >> 3;
    }
    xfer1(xfer, 0x70, self + 0x50);
    xfer1(xfer, 0x84, self + 0x54);
    xfer1(xfer, 0x84, self + 0x55);
    return xfer1(xfer, 0x70, self + 0x58);
}

std::uint32_t AIAttackMeleeHordeApproachTargetState_xfer(std::uint8_t* self, std::uint8_t* xfer) noexcept {
    xferVersion(xfer, 1, 1);
    direct1(0x007A1FC0, self, ptrBits(xfer));
    if (xferGated(xfer)) {
        return loadU32(xfer + 4) >> 3;
    }
    xfer1(xfer, 0x58, self + 0x50);
    xfer1(xfer, 0x6C, self + 0x5C);
    xfer1(xfer, 0x84, self + 0x60);
    return xfer1(xfer, 0x84, self + 0x61);
}

std::uint32_t AIAttackMeleeSquishState_xfer(std::uint8_t* self, std::uint8_t* xfer) noexcept {
    xferVersion(xfer, 1, 1);
    direct1(0x007A1FC0, self, ptrBits(xfer));
    if (xferGated(xfer)) {
        return loadU32(xfer + 4) >> 3;
    }
    xfer1(xfer, 0x58, self + 0x54);
    xfer1(xfer, 0x40, self + 0x60);
    xfer1(xfer, 0x6C, self + 0x50);
    return xfer1(xfer, 0x84, self + 0x68);
}

std::uint32_t AIChargeTargetState_xfer(std::uint8_t* self, std::uint8_t* xfer) noexcept {
    xferVersion(xfer, 1, 1);
    xfer1(xfer, 0x84, self + 0x59);
    xfer1(xfer, 0x84, self + 0x58);
    xfer1(xfer, 0x84, self + 0x50);
    xfer1(xfer, 0x70, self + 0x54);
    return direct1(0x007A1FC0, self, ptrBits(xfer));
}

std::uint32_t AIFollowPathAsTeamState_xfer(std::uint8_t* self, std::uint8_t* xfer) noexcept {
    xferVersion(xfer, 1, 1);
    direct1(0x007A1FC0, self, ptrBits(xfer));
    if (xferGated(xfer)) {
        return loadU32(xfer + 4) >> 3;
    }
    xfer1(xfer, 0x70, self + 0x50);
    xfer1(xfer, 0x84, self + 0x58);
    xfer1(xfer, 0x84, self + 0x59);
    xfer1(xfer, 0x84, self + 0x5A);
    xfer1(xfer, 0x70, self + 0x54);
    direct2(0x00549C70, xfer, ptrBits(xfer), ptrBits(self + 0x5C));
    if (const std::uint32_t cur = loadU32(self + 0x60)) {
        xfer1(xfer, 0x20, asPtr(cur));
    }
    xfer1(xfer, 0x64, self + 0x64);
    xfer1(xfer, 0x84, self + 0x68);
    return xfer1(xfer, 0x84, self + 0x5B);
}

std::uint32_t AIAttackMeleeEngageState_xfer(std::uint8_t* self, std::uint8_t* xfer) noexcept {
    xferVersion(xfer, 1, 1);
    direct1(0x007A1FC0, self, ptrBits(xfer));
    if (xferGated(xfer)) {
        return loadU32(xfer + 4) >> 3;
    }
    xfer1(xfer, 0x58, self + 0x5C);
    xfer1(xfer, 0x40, self + 0x68);
    xfer1(xfer, 0x6C, self + 0x58);
    xfer1(xfer, 0x84, self + 0x75);
    if (loadU32(self + 0x50) == 0) {
        const std::uint32_t block = direct1(0x00416820, xfer, 0x28);
        if (block != 0) {
            direct2(0x005A9F60, asPtr(block), ptrBits(machineOf(self)), 0xAAEC3BFDu);
            storeU32(asPtr(block) + 0x20, loadU32(self + 0x78));
            storeU32(asPtr(block), 0x00C2C9E0u);
            asPtr(block)[0x24] = 0;
        }
        storeU32(self + 0x50, block);
    }
    std::uint32_t local = loadU32(self + 0x54);
    xfer1(xfer, 0x70, &local);
    storeU32(self + 0x54, local);
    xfer1(xfer, 0x20, asPtr(loadU32(self + 0x50)));
    xfer1(xfer, 0x6C, self + 0x70);
    return xfer1(xfer, 0x84, self + 0x74);
}

std::uint32_t AI_xfer(std::uint8_t* self, std::uint8_t* xfer) noexcept {
    if (bit(loadU32(xfer + 4), 2)) {
        auto* sentinel = self + 8;
        std::uint32_t local = 0;
        for (auto* node = *reinterpret_cast<std::uint8_t**>(sentinel); node != sentinel;
             node = *reinterpret_cast<std::uint8_t**>(node)) {
            if (loadU32(node + 8) != 0) {
                std::uint8_t text[16]{};
                direct2(0x004CB690, text, 0x00C31A98u, 0x0Eu);
                xfer1(xfer, 0x60, &local);
                xfer1(xfer, 0x20, asPtr(loadU32(node + 8)));
            }
        }
        return direct0(0x004CA930, &local);
    }
    xferVersion(xfer, 1, 1);
    xfer1(xfer, 0x6C, self + 0x18);
    return call2(xfer, 0x14, ptrBits(self + 0x1C), 4);
}

}  // namespace ra3::gamelogic
