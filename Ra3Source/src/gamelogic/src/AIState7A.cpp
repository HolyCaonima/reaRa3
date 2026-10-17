#include "ra3/gamelogic/AIState7A.h"

#include <cstring>

namespace ra3::gamelogic {
namespace {

AIState7ACalls g_calls{};

// Dword read from 0x00BE700C. The image bytes are FF 7F FF 7F.
constexpr std::uint32_t kGoalScalarMax = 0x7F7FFFFFu;
constexpr std::uint32_t kStateFailure = 0xFFFFFFFEu;

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

std::uint32_t direct(std::uint32_t address, void* ecx, std::uint32_t a0 = 0, std::uint32_t a1 = 0,
                     std::uint32_t a2 = 0, int nargs = 0) noexcept {
    return g_calls.direct(address, ecx, a0, a1, a2, nargs);
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

std::uint32_t call2(void* object, int slot, std::uint32_t a0, std::uint32_t a1) noexcept {
    auto* vt = *reinterpret_cast<std::uint8_t**>(object);
    auto* fn = *reinterpret_cast<std::uint32_t(__fastcall**)(void*, void*, std::uint32_t, std::uint32_t)>(
        vt + slot);
    return fn(object, nullptr, a0, a1);
}

std::uint8_t* machineOf(std::uint8_t* self) noexcept {
    return *reinterpret_cast<std::uint8_t**>(self + 0x18);
}

std::uint8_t* ownerOf(std::uint8_t* machine) noexcept {
    return *reinterpret_cast<std::uint8_t**>(machine + 0x24);
}

// ucomiss + lahf + test ah,0x44 + jp. Ordered equality falls through to -1.
// Unordered (NaN sets ZF, PF and CF) has even parity and takes the tail call.
bool angleIsGoalMax(std::uint32_t bits) noexcept {
    return bits == kGoalScalarMax;
}

std::uint32_t xferVersion(std::uint8_t* xfer) noexcept {
    std::uint8_t bytes[2] = {1, 1};
    return call1(xfer, 0x18, ptrBits(bytes));
}

bool statusBit(std::uint8_t* object, int offset, int shift) noexcept {
    return ((loadU32(object + offset) >> shift) & 1u) != 0;
}

std::uint32_t publishStatus(std::uint8_t* owner, std::uint32_t index, std::uint32_t value) noexcept {
    direct(0x006A4F80, owner, index, value, 0, 2);
    return direct(0x006A4FB0, owner, 0, 0, 0, 0);
}

std::uint32_t setStatusIfClear(std::uint8_t* owner, int offset, int shift, std::uint32_t mask,
                               std::uint32_t index) noexcept {
    const std::uint32_t bits = loadU32(owner + offset);
    if (((bits >> shift) & 1u) != 0) {
        return 0;
    }
    storeU32(owner + offset, bits | mask);
    publishStatus(owner, index, 1);
    return 1;
}

std::uint32_t clearStatusIfSet(std::uint8_t* owner, int offset, int shift, std::uint32_t mask,
                               std::uint32_t index) noexcept {
    const std::uint32_t bits = loadU32(owner + offset);
    if (((bits >> shift) & 1u) == 0) {
        return 0;
    }
    storeU32(owner + offset, bits & mask);
    publishStatus(owner, index, 0);
    return 1;
}

bool goalBlocked(std::uint8_t* goal) noexcept {
    return goal == nullptr || (goal[0x80] & 1) != 0 || (goal[0x471] & 1) != 0;
}

std::uint32_t dynamicBit(std::uint32_t index) noexcept {
    return 1u << (index & 0x1Fu);
}

int dynamicWord(std::uint32_t index) noexcept {
    return static_cast<int>(index >> 5) * 4;
}

}  // namespace

void setAIState7ACalls(const AIState7ACalls& calls) noexcept {
    g_calls = calls;
}

std::uint32_t UnitAIRotateToAngleState_onEnter(std::uint8_t* self) noexcept {
    auto* machine = machineOf(self);
    if (angleIsGoalMax(loadU32(machine + 0x54))) {
        return 0xFFFFFFFFu;
    }
    return direct(0x007985D0, self, 0, 0, 0, 0);
}

std::uint32_t AIWaitUntilMembersFinishedBeforeExitState_onEnter(std::uint8_t* self) noexcept {
    auto* machine = machineOf(self);
    auto* owner = ownerOf(machine);
    auto* templ = *reinterpret_cast<std::uint8_t**>(owner + 4);
    // neg/sbb of bit 10: set -> 0, clear -> 0xFFFFFFFE. Mutation site is this shift.
    const std::uint32_t al = (loadU32(templ + 0xCC) >> 10) & 1u;
    std::uint32_t eax = al != 0 ? 0xFFFFFFFFu : 0u;
    eax &= 2u;
    eax += kStateFailure;
    return eax;
}

std::uint32_t UnitAICombinedUnitMoveToState_update(std::uint8_t* self) noexcept {
    direct(0x0079E250, self, 0, 0, 0, 0);
    auto* path = *reinterpret_cast<std::uint8_t**>(self + 0x20);
    return call0(path, 0x10);
}

std::uint32_t UnitAICombinedUnitMoveToState_onExit(std::uint8_t* self, std::uint32_t status) noexcept {
    auto* owner = ownerOf(machineOf(self));
    auto* loco = *reinterpret_cast<std::uint8_t**>(owner + 0x374);
    if (loco != nullptr) {
        direct(0x00683140, loco, 0, 0, 0, 1);
    }
    return direct(0x0079FF10, self, status, 0, 0, 1);
}

std::uint32_t AIRotateFiringarc_onEnter(std::uint8_t* self) noexcept {
    auto* machine = machineOf(self);
    auto* owner = ownerOf(machine);
    auto* loco = *reinterpret_cast<std::uint8_t**>(owner + 0x374);
    if (loco == nullptr) {
        return kStateFailure;
    }
    direct(0x007527C0, loco, 0x0F, 0, 0, 1);
    const std::uint32_t angle = loadU32(machineOf(self) + 0x2C);
    call1(loco, 0x26C, angle);
    return 0;
}

std::uint32_t UnitAIDepartBeach_onEnter(std::uint8_t* self) noexcept {
    auto* machine = machineOf(self);
    std::uint8_t local[12]{};
    const std::uint32_t ok = direct(0x0079E530, self, ptrBits(local), 0, 0, 1);
    if ((ok & 0xFFu) == 0) {
        return kStateFailure;
    }
    direct(0x005AC4F0, machine, ptrBits(local), kGoalScalarMax, 0, 2);
    return 0xFFFFFFFFu;
}

std::uint32_t UnitAIDriftState_onEnter(std::uint8_t* self) noexcept {
    auto* owner = ownerOf(machineOf(self));
    setStatusIfClear(owner, 0xA0, 0x11, 0x00020000u, 0x11);
    return 0;
}

std::uint32_t UnitAIHordeGarrisonedState_onEnter(std::uint8_t* self) noexcept {
    auto* owner = ownerOf(machineOf(self));
    setStatusIfClear(owner, 0xA0, 0x15, 0x00200000u, 0x15);
    return 0;
}

std::uint32_t UnitAIHordeGarrisonedState_onExit(std::uint8_t* self, std::uint32_t) noexcept {
    auto* machine = machineOf(self);
    auto* owner = ownerOf(machine);
    if (!statusBit(owner, 0xA0, 0x15)) {
        return ptrBits(machine);
    }
    storeU32(owner + 0xA0, loadU32(owner + 0xA0) & 0xFFDFFFFFu);
    return publishStatus(owner, 0x15, 0);
}

std::uint32_t UnitAIDisguiseHoldFireState_onEnter(std::uint8_t* self) noexcept {
    auto* owner = ownerOf(machineOf(self));
    direct(0x006AEFE0, owner, ptrBits(owner), 0, 1, 3);
    const std::uint32_t produced = call0(self, 0x4C);
    storeU32(self + 0x20, produced);
    auto* object = reinterpret_cast<std::uint8_t*>(static_cast<std::uintptr_t>(produced));
    return call1(object, 0x1C, loadU32(object + 0x68));
}

std::uint32_t AIStateMachine_createLinkedState(std::uint8_t* self, std::uint32_t argument) noexcept {
    const std::uint32_t block = direct(0x00416820, self, 0x80, 0, 0, 1);
    if (block == 0) {
        return 0;
    }
    auto* memory = reinterpret_cast<std::uint8_t*>(static_cast<std::uintptr_t>(block));
    const std::uint32_t owner = loadU32(self + 0x24);
    return direct(0x007A57A0, memory, owner, 0x71E396C1u, argument, 3);
}

std::uint32_t UnitAIPrepareForBoardingState_onEnter(std::uint8_t* self) noexcept {
    const std::uint32_t produced = call0(self, 0x4C);
    storeU32(self + 0x20, produced);
    auto* machine = machineOf(self);
    const std::uint32_t goal = direct(0x005B9F30, machine, 0, 0, 0, 0);
    auto* object = reinterpret_cast<std::uint8_t*>(static_cast<std::uintptr_t>(produced));
    call1(object, 0x38, goal);
    auto* owner = ownerOf(machine);
    direct(0x006D2020, owner, 0x29, 1, 0, 2);
    return call1(object, 0x1C, loadU32(object + 0x68));
}

std::uint32_t UnitAIMoveToGoalAndEnterState_onExit(std::uint8_t* self, std::uint32_t) noexcept {
    auto* machine = machineOf(self);
    auto* owner = ownerOf(machine);
    std::uint32_t eax = direct(0x005B9F30, machine, 0, 0, 0, 0);
    if (eax != 0) {
        eax = direct(0x006A7620, reinterpret_cast<void*>(static_cast<std::uintptr_t>(eax)), ptrBits(owner), 1,
                     0, 2);
        if (eax != 0) {
            eax = call2(reinterpret_cast<void*>(static_cast<std::uintptr_t>(eax)), 0x40, ptrBits(owner), 2);
        }
    }
    if (loadU32(owner + 0x374) != 0) {
        auto* loco = *reinterpret_cast<std::uint8_t**>(owner + 0x374);
        eax = direct(0x00683140, loco, 0, 0, 0, 1);
    }
    auto* sub = *reinterpret_cast<std::uint8_t**>(self + 0x20);
    if (sub != nullptr) {
        eax = call1(sub, 0x00, 1);
    }
    storeU32(self + 0x20, 0);
    return eax;
}

std::uint32_t UnitAIParachuteToGroundState_onExit(std::uint8_t* self, std::uint32_t) noexcept {
    auto* owner = ownerOf(machineOf(self));
    direct(0x006F04A0, owner, 0x11, 1, 0, 2);
    direct(0x006D2020, owner, 5, 0, 0, 2);
    direct(0x006D2020, owner, 0x4D, 0, 0, 2);
    std::uint32_t eax = direct(0x006D2020, owner, 8, 0, 0, 2);
    auto* sub = *reinterpret_cast<std::uint8_t**>(self + 0x20);
    if (sub != nullptr) {
        eax = call1(sub, 0x00, 1);
    }
    storeU32(self + 0x20, 0);
    return eax;
}

std::uint32_t UnitAIPrepareForBoardingState_onExit(std::uint8_t* self, std::uint32_t) noexcept {
    auto* owner = ownerOf(machineOf(self));
    auto* loco = *reinterpret_cast<std::uint8_t**>(owner + 0x374);
    if (loco != nullptr) {
        direct(0x007527C0, loco, 0, 0, 0, 1);
    }
    const std::uint32_t phys = direct(0x006A7600, owner, 0, 0, 0, 0);
    auto* body = reinterpret_cast<std::uint8_t*>(static_cast<std::uintptr_t>(phys));
    const std::uint32_t inner = call0(body, 0x08);
    direct(0x006DF450, reinterpret_cast<void*>(static_cast<std::uintptr_t>(inner)), 0, 0, 0, 0);
    direct(0x006D2020, owner, 0x29, 0, 0, 2);
    direct(0x006D2020, owner, 0xA7, 0, 0, 2);
    std::uint32_t eax = direct(0x006D2020, owner, 0x4D, 0, 0, 2);
    auto* sub = *reinterpret_cast<std::uint8_t**>(self + 0x20);
    if (sub != nullptr) {
        eax = call1(sub, 0x00, 1);
    }
    storeU32(self + 0x20, 0);
    return eax;
}

std::uint32_t AIFaceState_onExit(std::uint8_t* self, std::uint32_t status) noexcept {
    auto* machine = machineOf(self);
    std::uint32_t eax = direct(0x005B9F30, machine, 0, 0, 0, 0);
    if (eax == 0) {
        return eax;
    }
    auto* goal = reinterpret_cast<std::uint8_t*>(static_cast<std::uintptr_t>(eax));
    if ((goal[0x80] & 1) != 0 || (goal[0x471] & 1) != 0) {
        return eax;
    }
    eax = direct(0x00687810, goal, 0, 0, 0, 0);
    if (eax == 0) {
        return eax;
    }
    auto* module = reinterpret_cast<std::uint8_t*>(static_cast<std::uintptr_t>(eax));
    eax = call0(module, 0x38);
    if ((eax & 0xFFu) != 0 && status != 1) {
        return eax;
    }
    return call1(module, 0x34, ptrBits(ownerOf(machine)));
}

std::uint32_t AIAttackMeleeHordeWaitState_onExit(std::uint8_t* self, std::uint32_t) noexcept {
    auto* machine = machineOf(self);
    auto* owner = ownerOf(machine);
    if ((owner[0x80] & 1) != 0) {
        return ptrBits(machine);
    }
    std::uint32_t eax = direct(0x006A7600, owner, 0, 0, 0, 0);
    if (eax == 0) {
        return eax;
    }
    eax = direct(0x006A7600, owner, 0, 0, 0, 0);
    auto* phys = reinterpret_cast<std::uint8_t*>(static_cast<std::uintptr_t>(eax));
    eax = call0(phys, 0x84);
    if (eax == 0) {
        return eax;
    }
    auto* weapon = reinterpret_cast<std::uint8_t*>(static_cast<std::uintptr_t>(eax));
    call0(weapon, 0x13C);
    return call1(weapon, 0x16C, 0);
}

std::uint32_t AIHordeEnterState_onExit(std::uint8_t* self, std::uint32_t status) noexcept {
    auto* machine = machineOf(self);
    const std::uint32_t goalBits = direct(0x005B9F30, machine, 0, 0, 0, 0);
    if (goalBits != 0) {
        auto* goal = reinterpret_cast<std::uint8_t*>(static_cast<std::uintptr_t>(goalBits));
        auto* info = *reinterpret_cast<std::uint8_t**>(goal + 4);
        if (!statusBit(info, 0xC0, 4)) {
            direct(0x00706E00, goal, 8, 0, 0, 1);
        }
        std::uint32_t phys = direct(0x006A7600, goal, 0, 0, 0, 0);
        if (phys != 0) {
            phys = direct(0x006A7600, goal, 0, 0, 0, 0);
            call2(reinterpret_cast<void*>(static_cast<std::uintptr_t>(phys)), 0x40, ptrBits(ownerOf(machine)),
                  2);
        }
    }
    return direct(0x009CCDA0, self, status, 0, 0, 1);
}

std::uint32_t UnitAIReloadAmmoState_onExit(std::uint8_t* self, std::uint32_t) noexcept {
    auto* owner = ownerOf(machineOf(self));
    clearStatusIfSet(owner, 0xB4, 5, 0xFFFFFFDFu, 0xA5);
    return direct(0x006D2020, owner, 0xCF, 0, 0, 2);
}

std::uint32_t UnitAIDeliverToRefineryState_onExit(std::uint8_t* self, std::uint32_t) noexcept {
    auto* owner = ownerOf(machineOf(self));
    clearStatusIfSet(owner, 0xB4, 0x17, 0xFF7FFFFFu, 0xB7);
    owner = ownerOf(machineOf(self));
    return direct(0x006D2020, owner, 0x34, 0, 0, 2);
}

std::uint32_t UnitAICreatePathState_onEnter(std::uint8_t* self) noexcept {
    auto* machine = machineOf(self);
    auto* owner = ownerOf(machine);
    const std::uint32_t probed = direct(0x00686E50, owner, 0, 0, 0, 0);
    direct(0x00686D70, owner, ptrBits(machine + 0x2C), probed, 0, 2);
    machine = machineOf(self);
    const std::uint32_t alt = machine[0x44] != 0 ? ptrBits(machine + 0x48) : ptrBits(machine + 0x2C);
    auto* loco = *reinterpret_cast<std::uint8_t**>(ownerOf(machine) + 0x374);
    direct(0x007525F0, loco, ptrBits(machine + 0x2C), alt, 0, 2);
    return direct(0x0079E460, self, 0, 0, 0, 0);
}

std::uint32_t AIFollowExitProductionPathState_onExit(std::uint8_t* self, std::uint32_t status) noexcept {
    auto* owner = ownerOf(machineOf(self));
    direct(0x006F19F0, owner, 0, 0, 0, 0);
    auto* loco = *reinterpret_cast<std::uint8_t**>(ownerOf(machineOf(self)) + 0x374);
    const std::uint32_t mode = loadU32(self + 0x24);
    if (loco != nullptr && mode != 0xFFFFFFFFu) {
        direct(0x007527C0, loco, mode, 0, 0, 1);
    }
    return direct(0x0079EE50, self, status, 0, 0, 1);
}

std::uint32_t UnitAIGarrisonedState_onExit(std::uint8_t* self, std::uint32_t) noexcept {
    auto* machine = machineOf(self);
    auto* owner = ownerOf(machine);
    std::uint32_t eax = ptrBits(machine);
    if (statusBit(owner, 0xA0, 0x15)) {
        storeU32(owner + 0xA0, loadU32(owner + 0xA0) & 0xFFDFFFFFu);
        eax = publishStatus(owner, 0x15, 0);
    }
    auto* sub = *reinterpret_cast<std::uint8_t**>(self + 0x20);
    if (sub != nullptr) {
        eax = call1(sub, 0x00, 1);
    }
    storeU32(self + 0x20, 0);
    return eax;
}

std::uint32_t UnitAIMoveOutOfTheWayState_onExit(std::uint8_t* self, std::uint32_t) noexcept {
    auto* machine = machineOf(self);
    auto* owner = ownerOf(machine);
    std::uint32_t eax = ptrBits(machine);
    if (statusBit(owner, 0xB0, 0x16)) {
        storeU32(owner + 0xB0, loadU32(owner + 0xB0) & 0xFFBFFFFFu);
        eax = publishStatus(owner, 0x96, 0);
    }
    auto* loco = *reinterpret_cast<std::uint8_t**>(owner + 0x374);
    if (loco != nullptr) {
        eax = direct(0x006996C0, loco, 0, 0, 0, 0);
        loco[0x434] = 0;
        storeU32(loco + 0x1E8, 0);
        storeU32(loco + 0x1EC, 0);
    }
    auto* sub = *reinterpret_cast<std::uint8_t**>(self + 0x20);
    if (sub != nullptr) {
        eax = call1(sub, 0x00, 1);
    }
    storeU32(self + 0x20, 0);
    return eax;
}

std::uint32_t UnitAIMoveToGoalState_onExit(std::uint8_t* self, std::uint32_t status) noexcept {
    auto* owner = ownerOf(machineOf(self));
    auto* loco = *reinterpret_cast<std::uint8_t**>(owner + 0x374);
    if (loco != nullptr) {
        direct(0x00683140, loco, 0, 0, 0, 1);
    }
    direct(0x006D2020, owner, 0x73, 0, 0, 2);
    direct(0x006D2020, owner, 0x9D, 0, 0, 2);
    return direct(0x0079FF10, self, status, 0, 0, 1);
}

std::uint32_t UnitAIMoveToDeliverState_onExit(std::uint8_t* self, std::uint32_t status) noexcept {
    direct(0x009CCDA0, self, status, 0, 0, 1);
    auto* owner = ownerOf(machineOf(self));
    const std::uint32_t index = loadU32(self + 0x28);
    const std::uint32_t mask = dynamicBit(index);
    std::uint8_t* cell = owner + 0xA0 + dynamicWord(index);
    if ((loadU32(cell) & mask) == 0) {
        return mask;
    }
    storeU32(cell, loadU32(cell) & ~mask);
    return publishStatus(owner, index, 0);
}

std::uint32_t AIHarvestPrepareSiteState_onExit(std::uint8_t* self, std::uint32_t) noexcept {
    auto* machine = machineOf(self);
    auto* owner = ownerOf(machine);
    if (!statusBit(owner, 0xB4, 0x1E)) {
        return ptrBits(machine);
    }
    storeU32(owner + 0xB4, loadU32(owner + 0xB4) & 0xBFFFFFFFu);
    return publishStatus(owner, 0xBE, 0);
}

std::uint32_t JetAIAttackMoveToGoalState_onExit(std::uint8_t* self, std::uint32_t) noexcept {
    auto* machine = machineOf(self);
    auto* owner = ownerOf(machine);
    std::uint32_t eax = ptrBits(machine);
    if (statusBit(owner, 0xB0, 0x19)) {
        storeU32(owner + 0xB0, loadU32(owner + 0xB0) & 0xFDFFFFFFu);
        eax = publishStatus(owner, 0x99, 0);
    }
    auto* sub = *reinterpret_cast<std::uint8_t**>(self + 0x20);
    if (sub != nullptr) {
        eax = call1(sub, 0x00, 1);
    }
    storeU32(self + 0x20, 0);
    return eax;
}

std::uint32_t UnitAIAttackPositionState_onExit(std::uint8_t* self, std::uint32_t) noexcept {
    auto* owner = ownerOf(machineOf(self));
    if ((loadU32(owner + 0xA8) & 1u) != 0) {
        storeU32(owner + 0xA8, loadU32(owner + 0xA8) & 0xFFFFFFFEu);
        publishStatus(owner, 0x40, 0);
    }
    if (statusBit(owner, 0xA8, 2)) {
        storeU32(owner + 0xA8, loadU32(owner + 0xA8) & 0xFFFFFFFBu);
        publishStatus(owner, 0x42, 0);
    }
    std::uint32_t eax = direct(0x006876D0, owner, 0, 0, 0, 1);
    auto* sub = *reinterpret_cast<std::uint8_t**>(self + 0x20);
    if (sub != nullptr) {
        eax = call1(sub, 0x00, 1);
    }
    storeU32(self + 0x20, 0);
    return eax;
}

std::uint32_t AIDockWaitForClearanceState_update(std::uint8_t* self) noexcept {
    auto* machine = machineOf(self);
    const std::uint32_t goalBits = direct(0x005B9F30, machine, 0, 0, 0, 0);
    if (goalBits == 0) {
        return kStateFailure;
    }
    auto* goal = reinterpret_cast<std::uint8_t*>(static_cast<std::uintptr_t>(goalBits));
    if (goalBlocked(goal)) {
        return kStateFailure;
    }
    const std::uint32_t moduleBits = direct(0x00687810, goal, 0, 0, 0, 0);
    if (moduleBits == 0) {
        return kStateFailure;
    }
    auto* module = reinterpret_cast<std::uint8_t*>(static_cast<std::uintptr_t>(moduleBits));
    const std::uint32_t busy = call0(module, 0x38);
    const std::uint32_t owner = ptrBits(ownerOf(machine));
    if ((busy & 0xFFu) == 0) {
        call1(module, 0x34, owner);
        return kStateFailure;
    }
    const std::uint32_t al = call1(module, 0x0C, owner) & 0xFFu;
    return al != 0 ? 0xFFFFFFFFu : 0u;
}

std::uint32_t AIDockProcessDockState_onEnter(std::uint8_t* self) noexcept {
    auto* machine = machineOf(self);
    const std::uint32_t goalBits = direct(0x005B9F30, machine, 0, 0, 0, 0);
    if (goalBits == 0) {
        return kStateFailure;
    }
    auto* goal = reinterpret_cast<std::uint8_t*>(static_cast<std::uintptr_t>(goalBits));
    if (goalBlocked(goal)) {
        return kStateFailure;
    }
    const std::uint32_t moduleBits = direct(0x00687810, goal, 0, 0, 0, 0);
    if (moduleBits == 0) {
        return kStateFailure;
    }
    direct(0x00798D10, self, 0, 0, 0, 0);
    auto* logic = reinterpret_cast<std::uint8_t*>(static_cast<std::uintptr_t>(g_calls.gameLogic));
    const std::uint32_t base = loadU32(logic + 0x58);
    auto* module = reinterpret_cast<std::uint8_t*>(static_cast<std::uintptr_t>(moduleBits));
    const std::uint32_t extra = call0(module, 0x54);
    storeU32(self + 0x28, base + extra);
    return 0;
}

std::uint32_t AIGuardGoingIdleState_onEnter(std::uint8_t* self) noexcept {
    auto* machine = machineOf(self);
    auto* owner = ownerOf(machine);
    if (owner == nullptr) {
        return kStateFailure;
    }
    const std::uint32_t found = direct(0x00687C30, owner, 0, 0, 0, 0);
    if (found == 0) {
        return kStateFailure;
    }
    std::uint8_t local[12]{};
    const std::uint32_t ok =
        direct(0x006AD4B0, reinterpret_cast<void*>(static_cast<std::uintptr_t>(found)), ptrBits(local), 0, 0, 1);
    if ((ok & 0xFFu) == 0) {
        return kStateFailure;
    }
    call1(machine, 0x38, 0);
    direct(0x005AD150, machine, ptrBits(local), 0, 0, 1);
    call1(machine, 0x1C, 5);
    return 0;
}

std::uint32_t AIStartAttackObjState_onEnter(std::uint8_t* self) noexcept {
    auto* machine = machineOf(self);
    auto* owner = ownerOf(machine);
    const std::uint32_t goal = direct(0x005B9F30, machine, 0, 0, 0, 0);
    if (goal == 0) {
        return 0xFFFFFFFFu;
    }
    auto* loco = *reinterpret_cast<std::uint8_t**>(owner + 0x374);
    const std::uint32_t clip = call0(loco, 0x288);
    const std::uint32_t armed = self[0x20] != 0 ? 1u : 0u;
    const std::uint32_t started = direct(0x007578F0, owner, armed, goal, clip, 3);
    if (started == 0) {
        return kStateFailure;
    }
    return 0xFFFFFFFFu;
}

std::uint32_t UnitAIMoveToDeliverState_onEnter(std::uint8_t* self) noexcept {
    storeU32(self + 0x24, loadU32(self + 0x20));
    auto* owner = ownerOf(machineOf(self));
    const std::uint32_t index = loadU32(self + 0x28);
    const std::uint32_t mask = dynamicBit(index);
    std::uint8_t* cell = owner + 0xA0 + dynamicWord(index);
    if ((loadU32(cell) & mask) == 0) {
        storeU32(cell, loadU32(cell) | mask);
        publishStatus(owner, index, 1);
    }
    return 0;
}

std::uint32_t UnitAITaxiFromHangerState_onExit(std::uint8_t* self, std::uint32_t) noexcept {
    auto* owner = ownerOf(machineOf(self));
    auto* loco = *reinterpret_cast<std::uint8_t**>(owner + 0x374);
    const std::uint32_t moduleBits = direct(0x00686CB0, owner, 0, 0, 0, 0);
    if (loco != nullptr && moduleBits != 0) {
        auto* module = reinterpret_cast<std::uint8_t*>(static_cast<std::uintptr_t>(moduleBits));
        storeU32(module + 0x40, loadU32(module + 0x40) & 0xFFFFFFB5u);
        loco[0x434] = 0;
    }
    clearStatusIfSet(owner, 0xB4, 5, 0xFFFFFFDFu, 0xA5);
    std::uint32_t eax = direct(0x006D2020, owner, 0xCF, 0, 0, 2);
    auto* sub = *reinterpret_cast<std::uint8_t**>(self + 0x20);
    if (sub != nullptr) {
        eax = call1(sub, 0x00, 1);
    }
    storeU32(self + 0x20, 0);
    return eax;
}

std::uint32_t UnitAIGarrisonedIdleState_onEnter(std::uint8_t* self) noexcept {
    auto* owner = ownerOf(machineOf(self));
    const std::uint32_t hostBits = direct(0x006EFEB0, owner, 0, 0, 0, 0);
    if (hostBits != 0) {
        auto* host = reinterpret_cast<std::uint8_t*>(static_cast<std::uintptr_t>(hostBits));
        std::uint32_t phys = direct(0x006A7600, host, 0, 0, 0, 0);
        if (phys != 0) {
            phys = direct(0x006A7600, host, 0, 0, 0, 0);
            auto* body = reinterpret_cast<std::uint8_t*>(static_cast<std::uintptr_t>(phys));
            const std::uint32_t inner = call0(body, 0x08);
            call1(reinterpret_cast<void*>(static_cast<std::uintptr_t>(inner)), 0x70, ptrBits(owner));
            direct(0x0079F330, self, hostBits, 0, 0, 1);
        }
    }
    return 0;
}

std::uint32_t AIMoveToStateSA_xfer(std::uint8_t* self, std::uint8_t* xfer) noexcept {
    xferVersion(xfer);
    direct(0x007A1FC0, self, ptrBits(xfer), 0, 0, 1);
    std::uint32_t eax = loadU32(xfer + 4) >> 3;
    if ((eax & 1u) == 0) {
        call1(xfer, 0x6C, ptrBits(self + 0x50));
        eax = call1(xfer, 0x84, ptrBits(self + 0x54));
    }
    return eax;
}

std::uint32_t AIMoveAndTightenState_xfer(std::uint8_t* self, std::uint8_t* xfer) noexcept {
    xferVersion(xfer);
    direct(0x007A1FC0, self, ptrBits(xfer), 0, 0, 1);
    call1(xfer, 0x70, ptrBits(self + 0x50));
    return call1(xfer, 0x84, ptrBits(self + 0x54));
}

std::uint32_t AIPickUpCrateState_xfer(std::uint8_t* self, std::uint8_t* xfer) noexcept {
    xferVersion(xfer);
    direct(0x007A1FC0, self, ptrBits(xfer), 0, 0, 1);
    call1(xfer, 0x70, ptrBits(self + 0x50));
    return call1(xfer, 0x58, ptrBits(self + 0x20));
}

std::uint32_t AIMoveAndEvacuateState_xfer(std::uint8_t* self, std::uint8_t* xfer) noexcept {
    xferVersion(xfer);
    direct(0x007A1FC0, self, ptrBits(xfer), 0, 0, 1);
    return call1(xfer, 0x58, ptrBits(self + 0x50));
}

std::uint32_t AIMoveAndDeleteState_xfer(std::uint8_t* self, std::uint8_t* xfer) noexcept {
    xferVersion(xfer);
    direct(0x007A1FC0, self, ptrBits(xfer), 0, 0, 1);
    return call1(xfer, 0x84, ptrBits(self + 0x50));
}

std::uint32_t AIAttackFollowWaypointPathState_xfer(std::uint8_t* self, std::uint8_t* xfer) noexcept {
    xferVersion(xfer);
    direct(0x007A44D0, self, ptrBits(xfer), 0, 0, 1);
    std::uint32_t eax = loadU32(xfer + 4) >> 3;
    if ((eax & 1u) == 0) {
        eax = call1(xfer, 0x20, loadU32(self + 0x6C));
    }
    return eax;
}

std::uint32_t AIWanderState_xfer(std::uint8_t* self, std::uint8_t* xfer) noexcept {
    xferVersion(xfer);
    direct(0x007A44D0, self, ptrBits(xfer), 0, 0, 1);
    call1(xfer, 0x70, ptrBits(self + 0x6C));
    return call1(xfer, 0x70, ptrBits(self + 0x70));
}

std::uint32_t AIEnterState_xfer(std::uint8_t* self, std::uint8_t* xfer) noexcept {
    xferVersion(xfer);
    direct(0x007A1FC0, self, ptrBits(xfer), 0, 0, 1);
    direct(0x0054A090, self, ptrBits(xfer), ptrBits(self + 0x50), 0, 2);
    return call1(xfer, 0x6C, ptrBits(self + 0x54));
}

std::uint32_t AIEnterAndAttackState_xfer(std::uint8_t* self, std::uint8_t* xfer) noexcept {
    xferVersion(xfer);
    direct(0x007A1FC0, self, ptrBits(xfer), 0, 0, 1);
    direct(0x0054A090, self, ptrBits(xfer), ptrBits(self + 0x50), 0, 2);
    return direct(0x0054A090, self, ptrBits(xfer), ptrBits(self + 0x54), 0, 2);
}

std::uint32_t AIMoveToPositionAndEnterState_xfer(std::uint8_t* self, std::uint8_t* xfer) noexcept {
    xferVersion(xfer);
    direct(0x0054A090, xfer, ptrBits(xfer), ptrBits(self + 0x54), 0, 2);
    return direct(0x007A1FC0, self, ptrBits(xfer), 0, 0, 1);
}

std::uint32_t AIHarvestApproachSiteState_xfer(std::uint8_t* self, std::uint8_t* xfer) noexcept {
    xferVersion(xfer);
    call1(xfer, 0x84, ptrBits(self + 0x50));
    return direct(0x007A1FC0, self, ptrBits(xfer), 0, 0, 1);
}

}  // namespace ra3::gamelogic
