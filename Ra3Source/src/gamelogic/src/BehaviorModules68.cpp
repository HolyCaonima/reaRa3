#include "ra3/gamelogic/BehaviorModules68.h"

#include <cstring>

namespace ra3::gamelogic {
namespace {

Behavior68Calls g_calls{};

std::uint32_t loadU32(const void* address) noexcept {
    std::uint32_t value = 0;
    std::memcpy(&value, address, 4);
    return value;
}

void storeU32(void* address, std::uint32_t value) noexcept {
    std::memcpy(address, &value, 4);
}

float loadF32(const void* address) noexcept {
    float value = 0.f;
    std::memcpy(&value, address, 4);
    return value;
}

std::uint32_t call0(void* object, int slot) noexcept {
    auto* vt = *reinterpret_cast<std::uint8_t**>(object);
    auto* fn = *reinterpret_cast<std::uint32_t(__fastcall**)(void*, void*)>(vt + slot);
    return fn(object, nullptr);
}

std::uint32_t call1(void* object, int slot, void* arg) noexcept {
    auto* vt = *reinterpret_cast<std::uint8_t**>(object);
    auto* fn = *reinterpret_cast<std::uint32_t(__fastcall**)(void*, void*, void*)>(vt + slot);
    return fn(object, nullptr, arg);
}

std::uint32_t call1u(void* object, int slot, std::uint32_t arg) noexcept {
    auto* vt = *reinterpret_cast<std::uint8_t**>(object);
    auto* fn = *reinterpret_cast<std::uint32_t(__fastcall**)(void*, void*, std::uint32_t)>(vt + slot);
    return fn(object, nullptr, arg);
}

void call3(void* object, int slot, std::uint32_t a, void* b, std::uint32_t c) noexcept {
    auto* vt = *reinterpret_cast<std::uint8_t**>(object);
    auto* fn = *reinterpret_cast<void(__fastcall**)(void*, void*, std::uint32_t, void*, std::uint32_t)>(
        vt + slot);
    fn(object, nullptr, a, b, c);
}

void xferModes(std::uint8_t* xfer, std::uint8_t firstSecond, std::uint8_t secondSecond) noexcept {
    std::uint8_t first[2] = {1, firstSecond};
    std::uint8_t second[2] = {1, secondSecond};
    call1(xfer, 0x18, first);
    call1(xfer, 0x18, second);
}

std::uint32_t withLowByte(std::uint32_t value, std::uint8_t low) noexcept {
    return (value & 0xFFFFFF00u) | low;
}

// ucomiss equality used by 0x00689E80: ordered and equal. The shipped immediate
// at 0x00BD9628 is -1.0f, which has a single encoding, so a bit compare matches
// including NaN (not equal) and excludes every other value.
bool isSentinelCoord(const std::uint8_t* coord) noexcept {
    constexpr std::uint32_t kSentinel = 0xBF800000u;
    return loadU32(coord) == kSentinel && loadU32(coord + 4) == kSentinel &&
           loadU32(coord + 8) == kSentinel;
}

}  // namespace

void setBehavior68Calls(const Behavior68Calls& calls) noexcept {
    g_calls = calls;
}

std::uint32_t GarrisonContain_clearReservedIfOccupied(std::uint8_t* self, std::uint32_t, std::uint32_t) noexcept {
    auto* body = *reinterpret_cast<std::uint8_t**>(self + 4);
    if (body[0x9E] != 0) {
        storeU32(self + 0x148, 0);
    }
    return static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(body));
}

void OpenContain_clearClosingByte(std::uint8_t* self, std::uint32_t, std::uint32_t) noexcept {
    self[0x110] = 0;
}

std::uint32_t TransportContain_passengerFlag(std::uint8_t* self, std::uint32_t) noexcept {
    auto* body = *reinterpret_cast<std::uint8_t**>(self + 4);
    return withLowByte(reinterpret_cast<std::uintptr_t>(body), body[0x143]);
}

std::uint32_t IdleState_busyMask(std::uint8_t* self) noexcept {
    auto* first = *reinterpret_cast<std::uint8_t**>(self + 0x18);
    auto* second = *reinterpret_cast<std::uint8_t**>(first + 0x80);
    auto* state = second + 0x24;
    const std::uint32_t eax = call0(state, 4);
    const std::uint8_t al = static_cast<std::uint8_t>(eax);
    return al != 0 ? 0xFFFFFFFFu : 0u;
}

void MoveInStraightLineState_xfer(std::uint8_t* self, std::uint8_t* xfer) noexcept {
    std::uint8_t modeA[2] = {1, 2};
    std::uint8_t modeB[2] = {1, 1};
    call1(xfer, 0x18, modeA);
    call1(xfer, 0x18, modeB);
    call1(xfer, 0x58, self + 0x28);
    if (modeA[1] >= 2) {
        call1(xfer, 0x64, self + 0x34);
    }
}

void MoveToGroundState_xfer(std::uint8_t* self, std::uint8_t* xfer) noexcept {
    xferModes(xfer, 1, 1);
    call1(xfer, 0x64, self + 0x28);
    call1(xfer, 0x64, self + 0x2C);
}

std::uint32_t BeamSpecialAbilityUpdate_templateFlag(std::uint8_t* self) noexcept {
    auto* body = *reinterpret_cast<std::uint8_t**>(self + 4);
    return withLowByte(reinterpret_cast<std::uintptr_t>(body), body[0x10D]);
}

void DominateEnemySpecialPower_storeRadius80(const std::uint8_t* self, std::uint8_t encoded[10]) noexcept {
    const auto* body = *reinterpret_cast<const std::uint8_t* const*>(self + 4);
    const float* radius = reinterpret_cast<const float*>(body + 0xF0);
    __asm {
        mov eax, radius
        fld dword ptr [eax]
        mov eax, encoded
        fstp tbyte ptr [eax]
    }
}

std::uint32_t ToggleStatusSpecialAbilityUpdate_entryPointer(std::uint8_t* self) noexcept {
    const std::uint32_t index = loadU32(self + 0xBC);
    auto* body = *reinterpret_cast<std::uint8_t**>(self + 4);
    auto* table = *reinterpret_cast<std::uint8_t**>(body + 0xF4);
    return static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(table + index * 0x118u + 0x7C));
}

std::uint32_t ToggleStatusSpecialAbilityUpdate_nextEntryPointer(std::uint8_t* self) noexcept {
    std::uint32_t index = loadU32(self + 0xBC) + 1u;
    auto* body = *reinterpret_cast<std::uint8_t**>(self + 4);
    if (index >= loadU32(body + 0xF0)) {
        index = 0;
    }
    auto* table = *reinterpret_cast<std::uint8_t**>(body + 0xF4);
    return static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(table + index * 0x118u + 0x7C));
}

void DozerPrimaryIdleState_xfer(std::uint8_t* self, std::uint8_t* xfer) noexcept {
    std::uint8_t mode[2] = {1, 1};
    call1(xfer, 0x18, mode);
    call1(xfer, 0x6C, self + 0x20);
    call1(xfer, 0x70, self + 0x24);
    call1(xfer, 0x84, self + 0x28);
}

void SupplyWarehouseDockUpdate_dispatch(std::uint8_t* self) noexcept {
    auto* update = *reinterpret_cast<std::uint8_t**>(self + 8);
    auto* dock = *reinterpret_cast<std::uint8_t**>(update + 0xF4);
    if (dock == nullptr) {
        return;
    }
    auto* body = *reinterpret_cast<std::uint8_t**>(self + 4);
    g_calls.dockDispatch(dock, nullptr, loadU32(body + 0x1C), loadU32(self + 0x8C));
}

std::uint32_t AIUpdateInterface_ensureLocomotor(std::uint8_t* self) noexcept {
    if (loadU32(self + 0x3C) != 0) {
        return 0;
    }
    const std::uint32_t created = call0(self, 0x2A4);
    storeU32(self + 0x3C, created);
    return call0(reinterpret_cast<void*>(static_cast<std::uintptr_t>(created)), 0x14);
}

std::uint32_t AIUpdateInterface_setGoal(std::uint8_t* self, std::uint32_t value) noexcept {
    storeU32(self + 0x480, value);
    return value;
}

std::uint32_t AIUpdateInterface_setGoalAux(std::uint8_t* self, std::uint32_t value) noexcept {
    storeU32(self + 0x484, value);
    return value;
}

std::uint32_t AIUpdateInterface_query34(std::uint8_t* self) noexcept {
    auto* loco = *reinterpret_cast<std::uint8_t**>(self + 0x3C);
    if (loco == nullptr || loadU32(loco + 4) == 0) {
        return withLowByte(reinterpret_cast<std::uintptr_t>(loco), 0);
    }
    return call0(reinterpret_cast<void*>(static_cast<std::uintptr_t>(loadU32(loco + 4))), 0x34);
}

std::uint32_t AIUpdateInterface_wantsAdvance(std::uint8_t* self) noexcept {
    std::uint32_t eax = call0(self, 0x274);
    std::uint8_t al = static_cast<std::uint8_t>(eax) == 0 ? 1 : 0;
    const float speed = loadF32(self + 0x188);
    // 0x00BD90A4 is 1.0f. comiss / jbe skips the force when speed <= 1 or unordered.
    if (speed > 1.0f) {
        al = 1;
    }
    auto* link = *reinterpret_cast<std::uint8_t**>(self + 0x214);
    if (link != nullptr) {
        auto* mid = *reinterpret_cast<std::uint8_t**>(link + 4);
        auto* leaf = *reinterpret_cast<std::uint8_t**>(mid + 4);
        if (loadU32(leaf + 0x64) == 2) {
            al = 1;
        }
    }
    return withLowByte(eax, al);
}

std::uint32_t AIUpdateInterface_query24(std::uint8_t* self) noexcept {
    auto* loco = *reinterpret_cast<std::uint8_t**>(self + 0x3C);
    if (loadU32(loco + 4) == 0) {
        return withLowByte(reinterpret_cast<std::uintptr_t>(loco), 1);
    }
    return call0(reinterpret_cast<void*>(static_cast<std::uintptr_t>(loadU32(loco + 4))), 0x24);
}

std::uint32_t AIUpdateInterface_query2C(std::uint8_t* self) noexcept {
    auto* loco = *reinterpret_cast<std::uint8_t**>(self + 0x3C);
    if (loadU32(loco + 4) == 0) {
        return withLowByte(reinterpret_cast<std::uintptr_t>(loco), 1);
    }
    return call0(reinterpret_cast<void*>(static_cast<std::uintptr_t>(loadU32(loco + 4))), 0x2C);
}

std::uint32_t AIUpdateInterface_query30(std::uint8_t* self) noexcept {
    auto* loco = *reinterpret_cast<std::uint8_t**>(self + 0x3C);
    if (loadU32(loco + 4) == 0) {
        return withLowByte(reinterpret_cast<std::uintptr_t>(loco), 1);
    }
    return call0(reinterpret_cast<void*>(static_cast<std::uintptr_t>(loadU32(loco + 4))), 0x30);
}

std::uint32_t AIUpdateInterface_enterState4D(std::uint8_t* self, std::uint32_t arg) noexcept {
    auto* loco = *reinterpret_cast<std::uint8_t**>(self + 0x3C);
    call0(loco, 0x34);
    storeU32(self + 0x50, arg);
    call1u(loco, 0x1C, 0x4D);
    const std::uint32_t cleared = 0xFFFFFFFFu;
    storeU32(self + 0x308, cleared);
    storeU32(self + 0x368, cleared);
    storeU32(self + 0x3C8, cleared);
    return cleared;
}

std::uint32_t AIUpdateInterface_forwardOrZero20(std::uint8_t* self, std::uint32_t arg) noexcept {
    auto* loco = *reinterpret_cast<std::uint8_t**>(self + 0x3C);
    if (loco == nullptr) {
        return 0;
    }
    return call1u(loco, 0x20, arg);
}

std::uint32_t AIUpdateInterface_forwardOrZero24(std::uint8_t* self, std::uint32_t arg) noexcept {
    auto* loco = *reinterpret_cast<std::uint8_t**>(self + 0x3C);
    if (loco == nullptr) {
        return 0;
    }
    return call1u(loco, 0x24, arg);
}

std::uint32_t LeechTargetingAttachUpdate_notify(std::uint8_t* self, std::uint8_t* object) noexcept {
    auto* listener = *reinterpret_cast<std::uint8_t**>(object + 0x33C);
    if (listener == nullptr) {
        return 0;
    }
    auto* draw = *reinterpret_cast<std::uint8_t**>(self + 8);
    auto* id = *reinterpret_cast<std::uint8_t**>(draw + 0xDC);
    return call1(listener, 8, id);
}

void MissileUpdate_clearTimer(std::uint8_t* self) noexcept {
    storeU32(self + 0x11C, 0);
}

std::uint32_t ProjectileUpdate_addFrames(std::uint8_t* self, float frames, const std::uint8_t* clock,
                                          float secondsPerFrame) noexcept {
    const float* scale = reinterpret_cast<const float*>(clock + 0x1C4);
    const float* spf = &secondsPerFrame;
    const float* frameArg = &frames;
    std::uint32_t frame = loadU32(clock + 0x58);
    unsigned short saved = 0;
    unsigned short trunc = 0;
    unsigned __int64 truncated = 0;
    __asm {
        mov eax, scale
        fld dword ptr [eax]
        mov eax, spf
        fmul dword ptr [eax]
        mov eax, frameArg
        fmul dword ptr [eax]
        fnstcw saved
        movzx eax, saved
        or eax, 0C00h
        mov trunc, ax
        fldcw trunc
        fistp qword ptr truncated
        fldcw saved
    }
    const auto delta = static_cast<std::uint32_t>(truncated);
    frame += delta;
    storeU32(self + 0x58, frame);
    return delta;
}

std::uint32_t RepairUpdate_notify(std::uint8_t* self, std::uint8_t* player) noexcept {
    if (player == nullptr) {
        return 0;
    }
    const std::uint32_t previous = loadU32(self + 0x30);
    const std::uint32_t eax = call1u(player, 0x6C, previous);
    storeU32(self + 0x30, 1);
    return eax;
}

std::uint32_t SpecialAbilityUpdate_countdown(std::uint8_t* self) noexcept {
    std::uint32_t left = loadU32(self + 0x3C);
    if (left == 0) {
        return 1;
    }
    left -= 1;
    storeU32(self + 0x3C, left);
    const std::uint32_t eax = call0(self, 0x9C);
    return withLowByte(eax, static_cast<std::uint8_t>(static_cast<std::uint8_t>(eax) == 0 ? 1 : 0));
}

std::uint32_t EncloseBehavior_relayIfArmed(std::uint8_t* self) noexcept {
    if (self[4] == 0) {
        return 0;
    }
    return call0(self - 0x10, 0x5C);
}

std::uint32_t EncloseBehavior_tryOpen(std::uint8_t* self, std::uint32_t arg) noexcept {
    void* argPointer = reinterpret_cast<void*>(static_cast<std::uintptr_t>(arg));
    const std::uint32_t accepted = call1(self, 8, argPointer);
    if (static_cast<std::uint8_t>(accepted) == 0) {
        return withLowByte(accepted, 0);
    }
    auto* base = self - 0x10;
    call0(base, 0x58);
    call0(base, 0x60);
    call0(base, 0x5C);
    // The following load replaces EAX; mov al, 1 then keeps the slot-0x28 return's high bits.
    const std::uint32_t eax = call1u(self, 0x28, 1);
    return withLowByte(eax, 1);
}

std::uint32_t findModuleByInterfaceKey(std::uint8_t* self, std::uint32_t key) noexcept {
    auto* slot = *reinterpret_cast<std::uint8_t***>(self + 0x310);
    std::uint8_t* module = *slot;
    if (module == nullptr) {
        return 0;
    }
    for (;;) {
        if (call0(module, 0x10) == key) {
            return static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(module));
        }
        slot += 1;
        module = *slot;
        if (module == nullptr) {
            return 0;
        }
    }
}

std::uint32_t storeCoordAndSentinelSign(std::uint8_t* self, const std::uint8_t* coord) noexcept {
    std::memcpy(self + 0x40, coord, 12);
    // Both shipped paths compute sete/lea into -1 (all three components match
    // the -1 sentinel) or +1 (anything else, including a NaN). EAX is left as
    // 1 on the match path and 0 on the other path.
    const bool match = isSentinelCoord(coord);
    const std::int32_t sign = match ? -1 : 1;
    storeU32(self + 0x5C, static_cast<std::uint32_t>(sign));
    return match ? 1u : 0u;
}

void AreaRestrictSpecialPowerBehavior_xfer(std::uint8_t* self, std::uint8_t* xfer) noexcept {
    xferModes(xfer, 1, 1);
    g_calls.xferBase(self, nullptr, xfer);
}

void SupplyCenterCreate_xfer(std::uint8_t* self, std::uint8_t* xfer) noexcept {
    std::uint8_t mode[2] = {1, 1};
    call1(xfer, 0x18, mode);
    g_calls.xferModule(self, nullptr, xfer);
}

void AutoAbilityBehavior_xfer(std::uint8_t* self, std::uint8_t* xfer) noexcept {
    xferModes(xfer, 1, 1);
    g_calls.xferBase(self, nullptr, xfer);
    g_calls.xferAt10(self + 0x10, nullptr, xfer);
    call1(xfer, 0x60, self + 0x24);
    call1(xfer, 0x84, self + 0x34);
    call1(xfer, 0x58, self + 0x28);
}

void BridgeBuilderBehavior_countSpanSegments(std::uint8_t* self) noexcept {
    auto* ends = *reinterpret_cast<std::uint8_t**>(self + 8);
    auto* info = *reinterpret_cast<std::uint8_t**>(self + 4);
    const float dx = loadF32(ends + 0x38) - loadF32(self + 0x28);
    const float dy = loadF32(ends + 0x3C) - loadF32(self + 0x2C);
    const float width = loadF32(info + 0x10);
    const float* dxp = &dx;
    const float* dyp = &dy;
    const float* widthp = &width;
    float remainder = 0.f;
    float* remp = &remainder;
    __asm {
        mov eax, dyp
        fld dword ptr [eax]
        fmul st(0), st(0)
        mov eax, dxp
        fld dword ptr [eax]
        fmul st(0), st(0)
        faddp st(1), st(0)
        fsqrt
        mov eax, widthp
        fld dword ptr [eax]
        fadd st(0), st(0)
        fsubp st(1), st(0)
        mov eax, remp
        fstp dword ptr [eax]
    }
    storeU32(self + 0x34, 0);
    storeU32(self + 0x38, 0);
    if (!(remainder >= 0.f)) {
        return;
    }
    std::uint32_t which = 0;
    while (remainder >= 0.f) {
        storeU32(self + which * 4 + 0x34, loadU32(self + which * 4 + 0x34) + 1);
        float step = 0.f;
        if (which == 0) {
            step = loadF32(info + 0x18);
        } else {
            auto* alt = *reinterpret_cast<std::uint8_t**>(info + 0x20);
            step = loadF32(alt);
        }
        if (loadU32(info + 0x1C) != 0 && loadU32(info + 0x20) != 0) {
            which = 1u - which;
        }
        remainder -= step;
    }
}

void DistributedMoneyBehavior_xfer(std::uint8_t* self, std::uint8_t* xfer) noexcept {
    std::uint8_t mode[2] = {1, 1};
    call1(xfer, 0x18, mode);
    call1(xfer, 0x84, self + 0x1C);
    g_calls.xferModule(self, nullptr, xfer);
}

void EnergyDisablingBehavior_notifyOwner(std::uint8_t* self) noexcept {
    auto* object = *reinterpret_cast<std::uint8_t**>(self + 8);
    if (object == nullptr) {
        return;
    }
    auto* player = *reinterpret_cast<std::uint8_t**>(object + 0x418);
    if (player == nullptr) {
        return;
    }
    auto* target = *reinterpret_cast<std::uint8_t**>(player + 0x74);
    if (target == nullptr) {
        return;
    }
    g_calls.notifyEnergy(target, nullptr, loadU32(object + 0xDC));
}

std::uint8_t* copyPackedDamageBlock(std::uint8_t* dst, const std::uint8_t* src) noexcept {
    std::memcpy(dst, src, 0x10);
    const float* srcF = reinterpret_cast<const float*>(src);
    float* dstF = reinterpret_cast<float*>(dst);
    __asm {
        mov eax, srcF
        mov ecx, dstF
        fld dword ptr [eax + 10h]
        fstp dword ptr [ecx + 10h]
        fld dword ptr [eax + 14h]
        fstp dword ptr [ecx + 14h]
        fld dword ptr [eax + 18h]
        fstp dword ptr [ecx + 18h]
    }
    std::memcpy(dst + 0x1C, src + 0x1C, 0x10);
    return dst;
}

std::uint32_t sumStride8Keys(const std::uint8_t* self) noexcept {
    const auto* cursor = *reinterpret_cast<const std::uint8_t* const*>(self + 0x24);
    const auto* end = *reinterpret_cast<const std::uint8_t* const*>(self + 0x28);
    std::uint32_t sum = 0;
    while (cursor != end) {
        sum += loadU32(cursor);
        cursor += 8;
    }
    return sum;
}

void PlayerPowerManagerBehavior_xfer(std::uint8_t* self, std::uint8_t* xfer) noexcept {
    std::uint8_t modeA[2] = {1, 2};
    std::uint8_t modeB[2] = {1, 1};
    call1(xfer, 0x18, modeA);
    call1(xfer, 0x18, modeB);
    g_calls.xferBase(self, nullptr, xfer);
    g_calls.xferAt10(self + 0x10, nullptr, xfer);
    if (modeA[1] >= 2) {
        call1(xfer, 0x84, self + 0x54);
    }
}

void SlaveWatcherBehavior_xfer(std::uint8_t* self, std::uint8_t* xfer) noexcept {
    xferModes(xfer, 1, 1);
    g_calls.xferBase(self, nullptr, xfer);
    g_calls.xferAt10(self + 0x10, nullptr, xfer);
    g_calls.xferFieldA(xfer, self + 0x24);
    g_calls.xferFieldB(xfer, self + 0x28);
}

void StancesBehavior_xfer(std::uint8_t* self, std::uint8_t* xfer) noexcept {
    xferModes(xfer, 1, 1);
    g_calls.xferBase(self, nullptr, xfer);
    g_calls.xferAt10(self + 0x10, nullptr, xfer);
    call3(xfer, 0x88, 0x00BF7000u, self + 0x38, 4);
}

}  // namespace ra3::gamelogic
