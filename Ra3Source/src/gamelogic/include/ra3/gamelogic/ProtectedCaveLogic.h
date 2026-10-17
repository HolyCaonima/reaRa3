#pragma once

#include <cstddef>
#include <cstdint>

namespace ra3::gamelogic {

// Shared scratch image for the protected-cave oracle. The checker pins this
// image at kCaveImageBase so pointer-sized results match the Unicorn run.
inline constexpr std::uint32_t kCaveImageBase = 0x21000000u;
inline constexpr std::uint32_t kCaveImageSize = 0x1800u;

struct CaveCall {
    std::uint32_t target;
    std::uint32_t ecx;
    std::uint32_t a0;
    std::uint32_t a1;
    std::uint32_t a2;
    std::uint32_t nstack;
    std::uint32_t result;
};

struct CaveState {
    std::uint8_t* image;
    std::uint32_t rets[16];
    int retCount;
    int retPos;
    CaveCall calls[24];
    int callCount;
    std::uint32_t globalAddr[12];
    std::uint32_t globalVal[12];
    int globalCount;
    std::uint32_t eax;
    std::uint32_t edx;
    int cleanup;
};

std::uint32_t caveRead32(CaveState& st, std::uint32_t addr);
void caveWrite32(CaveState& st, std::uint32_t addr, std::uint32_t value);
std::uint8_t caveRead8(CaveState& st, std::uint32_t addr);
void caveWrite8(CaveState& st, std::uint32_t addr, std::uint8_t value);
float caveReadF32(CaveState& st, std::uint32_t addr);
void caveWriteF32(CaveState& st, std::uint32_t addr, float value);
std::uint32_t caveCall(CaveState& st, std::uint32_t target, std::uint32_t ecx,
                       std::uint32_t a0, std::uint32_t a1, std::uint32_t a2, int nstack);

// 0x006E5CE0 inner-patched site 0x006E5CE2 slot 0x018F2AA0 -> 0x006E5CE8
// direct callee of 0x0079B720 at 0x0079B72F; 0x0079B720 is 0x00C30160 slot 4
void runClearNonZeroStatusWord(CaveState& st);

// 0x006A7600 entry-recoverable site 0x006A7600 slot 0x018F2708 -> 0x006A7606
// direct callee of 0x007688E0 at 0x0076985C (GenericObjectCreationNugget.cpp)
void runObjectControllingPlayer(CaveState& st);

// 0x006A51A0 entry-recoverable site 0x006A51A0 slot 0x018F2B00 -> 0x006A51A6
// direct callee of 0x00750C70 at 0x0075117C (BeamSpecialAbilityUpdate.cpp)
void runQueryContainedModule(CaveState& st, std::uint32_t tag);

// 0x00699780 entry-recoverable site 0x00699780 slot 0x018F2C84 -> 0x00699786
// direct callee of 0x007B24E0 at 0x007B266C; 0x007B24E0 is 0x00C2F718 slot 19
void runSubControllerStateActive(CaveState& st);

// 0x006BB360 inner-patched site 0x006BB369 slot 0x018F2C98 -> 0x006BB36F
// direct callee of 0x00745250 at 0x0074534E (WeaponSlot.cpp)
void runClassifyObjectKindMask(CaveState& st, std::uint32_t target, std::uint32_t outMask);

// 0x006BD510 inner-patched site 0x006BD51E slot 0x018F2F18 -> 0x006BD526
// direct callee of 0x00745D40 at 0x00745DC1; 0x00745D40 is 0x00C07788 slot 1
void runObjectPassesKindFilter(CaveState& st, std::uint32_t target);

// 0x006C1500 inner-patched site 0x006C1504 slot 0x018EDDA0 -> 0x006C150B
// direct callee of 0x006EBEA0 at 0x006EBF35; 0x006EBEA0 is 0x00C06274 slot 1
void runFindBehaviorModuleByKey(CaveState& st, std::uint32_t key);

// 0x006A6460 inner-patched site 0x006A6463 slot 0x018ED9EC -> 0x006A646A
// direct callee of 0x006B0F80 at 0x006B0FC5; 0x006B0F80 is 0x00C22B88 slot 29
void runUnregisterObjectSubsystems(CaveState& st, std::uint32_t skipFlag);

// 0x006A6DC0 inner-patched site 0x006A6DC1 slot 0x018F2560 -> 0x006A6DC8
// direct callee of 0x00742940 at 0x00742A40 (NotifyTargetsOfImminentProbableCrushingMux.cpp)
void runObjectPositionStep(CaveState& st, std::uint32_t outPtr);

// 0x00747F10 inner-patched site 0x00747F13 slot 0x018F2E68 -> 0x00747F19
// direct callee of 0x004F7320 at 0x004F7A59; 0x004F7320 is 0x00BDCC74 slot 13
void runConsiderObjectIdle(CaveState& st, std::uint32_t allowBusy);

// 0x006C1D60 inner-patched site 0x006C1D63 slot 0x018F29F0 -> 0x006C1D69
// direct callee of 0x006C85E0 at 0x006C85FF; 0x006C85E0 is 0x00C11970 slot 98
void runReassignControllingPlayer(CaveState& st, std::uint32_t player);

// 0x00731530 inner-patched site 0x00731534 slot 0x018ED904 -> 0x0073153B
// direct callee of 0x00742E90 at 0x00742F46; 0x00742E90 is 0x00C19580 slot 4
void runRegisterActiveModuleListener(CaveState& st, std::uint32_t listener);

// 0x007451E0 inner-patched site 0x007451F5 slot 0x018ED724 -> 0x007451FD
// direct callee of 0x007564A0 at 0x007564A3; 0x007564A0 is 0x00C143B0 slot 1
void runNotifySixBucketListeners(CaveState& st);

// 0x0069E780 inner-patched site 0x0069E78E slot 0x018F2568 -> 0x0069E794
// direct callee of 0x006CE860 at 0x006CEB5D; 0x006CE860 is 0x00C19580 slot 9
void runPartitionModifierCount(CaveState& st, std::uint32_t spec);

// 0x006B7040 inner-patched site 0x006B7045 slot 0x018F27DC -> 0x006B704B
// direct callee of 0x0079FF10 at 0x0079FF61; 0x0079FF10 is 0x00C2EF40 slot 6
void runAssignAiTargetRef(CaveState& st, std::uint32_t unused, std::uint32_t newTarget);

}  // namespace ra3::gamelogic
