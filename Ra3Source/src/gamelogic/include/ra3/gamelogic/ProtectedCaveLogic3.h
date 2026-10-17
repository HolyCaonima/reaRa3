#pragma once

#include "ra3/gamelogic/ProtectedCaveLogic.h"

#include <cstdint>

namespace ra3::gamelogic {

struct CaveCallEntry {
    std::uint32_t targetVa;
    std::uint32_t ecx;
    std::uint32_t argCount;
    std::uint32_t arg0;
    std::uint32_t arg1;
    std::uint32_t arg2;
};

struct CaveCallTrace {
    static constexpr std::uint32_t kMaxEntries = 32;
    CaveCallEntry entries[kMaxEntries];
    std::uint32_t count;
};

// ============================================================================
// Third Protected-Cave Cluster (recon/protcave3)
//
// Closed-body reconstructions of 28 SecuROM-protected functions (inner-patched
// and entry-recoverable) whose trampolines jump through .securomDATA slots
// (0x018EC000..0x018F3FFF) into runtime-reconstructed caves at
// 0x00401000..0x0045FE80 before returning to .text continuations.
// ============================================================================

/// 0x004CB260 (inner-patched: site 0x004CB268 -> 0x0044DD7F via slot 0x018F2E38,
/// continuation 0x004CB26E, replaced `mov dword ptr [esi], 0`).
/// Direct callee of 0x005362E0 at 0x0053652A (vtable 0x00BE0BD0 slot 5).
std::uint32_t runInitZeroAndCall4CB200(
    void* self,
    std::uint32_t selfVa,
    std::uint32_t arg0,
    CaveCallTrace* trace = nullptr) noexcept;

/// 0x004D66D0 (entry-recoverable: site 0x004D66D0 -> 0x00459C86 via slot 0x018F25C8,
/// continuation 0x004D66D6, replaced `mov dword ptr [ecx], 0xBDB8E8`).
/// Direct callee of 0x00785B90 at 0x00785DC4 (vtable 0x00C22118 slot 168).
void runDestroyObjectBDB8E8(
    void* self,
    std::uint32_t childVa,
    void* child,
    std::uint32_t childDtorTargetVa,
    CaveCallTrace* trace = nullptr) noexcept;

/// 0x004D99E0 (inner-patched: site 0x004D99E5 -> 0x0042055A via slot 0x018ED488,
/// continuation 0x004D99EB, replaced `cmp esi, dword ptr [0x018F9C38]`).
/// Direct callee of 0x00541370 at 0x005413A5 (vtable 0x00BE1468 slot 27).
std::uint32_t runAllocateFromPowerOfTwoPool(
    std::uint32_t selfVa,
    std::uint32_t requestedSize,
    std::uint32_t maxPoolSize,
    std::uint32_t largeAllocReturnVa,
    std::uint32_t poolEntryVa,
    void* poolEntry,
    std::uint32_t growPoolReturnByte,
    std::uint32_t grownHeadNodeVa,
    const void* headNode,
    CaveCallTrace* trace = nullptr) noexcept;

/// 0x004DA710 (inner-patched: site 0x004DA716 -> 0x0044F45B via slot 0x018F2570,
/// continuation 0x004DA71C, replaced `mov dword ptr [esi], 0xBDC368`).
/// Direct callee of 0x00745AF0 at 0x00745B30 (vtable 0x00BF99D8 slot 1).
std::uint32_t runConstructObjectBDC368(
    void* self,
    std::uint32_t selfVa,
    CaveCallTrace* trace = nullptr) noexcept;

struct ListNode8C94Entry {
    std::uint32_t nodeVa;
    void* node;
    std::uint32_t helper4CE3D0Ret;
    std::uint32_t localOutVal;
};

/// 0x004E5C90 (inner-patched: site 0x004E5C95 -> 0x004440D2 via slot 0x018F2BB4,
/// continuation 0x004E5C9C, replaced `cmp dword ptr [esi + 0x8C], 0`).
/// Direct callee of 0x004F6C50 at 0x004F6E97 (vtable 0x00BDCC74 slot 4).
void runClearLists8CAnd94(
    void* self,
    std::uint32_t selfVa,
    std::uint32_t localOutStackVa,
    const ListNode8C94Entry* nodes,
    std::uint32_t nodeCount,
    CaveCallTrace* trace = nullptr) noexcept;

struct Module310DeactivateEntry {
    std::uint32_t moduleVa;
    std::uint32_t vt18TargetVa;
    std::uint32_t subObjVa;
    std::uint32_t subVt00TargetVa;
};

/// 0x004EDD70 (inner-patched: site 0x004EDD7C -> 0x004404A6 via slot 0x018EDA00,
/// continuation 0x004EDD83, replaced `test byte ptr [edi + 0x80], 1`).
/// Direct callee of 0x004F5380 at 0x004F54B7 (vtable 0x00BDD028 slot 7).
void runDeactivateAndEnqueueObject138(
    void* self,
    std::uint32_t selfVa,
    std::uint32_t targetVa,
    const void* target,
    const Module310DeactivateEntry* modules,
    std::uint32_t moduleCount,
    std::uint32_t allocatedNodeVa,
    void* allocatedNode,
    void* prevTailNode,
    CaveCallTrace* trace = nullptr) noexcept;

/// 0x00504510 (inner-patched: site 0x00504522 -> 0x0040B62E via slot 0x018ED658,
/// continuation 0x00504529, replaced `add dword ptr [eax + 0x98], 1`).
/// Direct callee of 0x0050E710 at 0x0050E7A9 (vtable 0x00BDF5A8 slot 99).
std::uint32_t runAssignRefObjectAtOffset94(
    void* self,
    std::uint32_t selfVa,
    const void* other,
    std::uint32_t otherVa,
    std::uint32_t newObjVa,
    void* newObj,
    std::uint32_t oldObjVa,
    void* oldObj,
    std::uint32_t oldObjDtorTargetVa,
    CaveCallTrace* trace = nullptr) noexcept;

/// 0x0050A9C0 (inner-patched: site 0x0050A9D3 -> 0x0044026E via slot 0x018ED9F8,
/// continuation 0x0050A9D9, replaced `imul esi, esi, 0x1FC`).
/// Direct callee of 0x005138F0 at 0x0051392A (vtable 0x00BDF5A8 slot 79).
void runApplyIndexedTableScaleToAudioObject(
    std::uint32_t targetVa,
    const void* target,
    const void* subObj0C,
    std::uint32_t subObj0CVt08TargetVa,
    std::uint32_t keyReturnVa,
    std::uint32_t outIterStackVa,
    std::uint32_t foundNodeVa,
    float foundScale18,
    CaveCallTrace* trace = nullptr) noexcept;

/// 0x00520120 (entry-recoverable: site 0x00520120 -> 0x00401B26 via slot 0x018F285C,
/// continuation 0x00520126, replaced `mov dl, byte ptr [ecx + 0x13C]`).
/// Direct callee of 0x00763BC0 at 0x007646C7 (vtable 0x00C19644 slot 0).
void runSetDrawableBit13CAndNotify394(
    void* self,
    std::uint32_t selfVa,
    std::uint32_t arg0,
    std::uint32_t listenerVt04TargetVa,
    CaveCallTrace* trace = nullptr) noexcept;

struct DrawableModule178QueryEntry {
    std::uint32_t moduleVa;
    std::uint32_t vtB0TargetVa;
    std::uint32_t subObjVa;
    std::uint32_t subVt0CTargetVa;
    std::uint8_t subVt0CReturn;
};

/// 0x00523E60 (inner-patched: site 0x00523E62 -> 0x0040D226 via slot 0x018ED3AC,
/// continuation 0x00523E68, replaced `mov esi, dword ptr [ecx + 0x178]`).
/// Direct callee of 0x006B7E30 at 0x006B7FD8 (vtable 0x00BF89A0 slot 12, LaserUpdate).
std::uint8_t runQueryDrawableModules178(
    const void* self,
    std::uint32_t arg0,
    std::uint32_t arg1,
    const DrawableModule178QueryEntry* entries,
    std::uint32_t entryCount,
    CaveCallTrace* trace = nullptr) noexcept;

/// 0x005242B0 (inner-patched: site 0x005242C3 -> 0x004128B0 via slot 0x018EDA54,
/// continuation 0x005242C9, replaced `mov ecx, dword ptr [esi + 0x224]`).
/// Direct callee of 0x008742C0 at 0x008742FB (vtable 0x00C50128 slot 5).
void runMarkDrawableSlotFlags224And234(
    const void* self,
    std::uint32_t selfVa,
    std::uint32_t arg0,
    std::int32_t indexReturn,
    void* array224,
    void* array234,
    CaveCallTrace* trace = nullptr) noexcept;

/// 0x0052D6B0 (inner-patched: site 0x0052D6BA -> 0x0040A37E via slot 0x018ED3A0,
/// continuation 0x0052D6C0, replaced `mov dword ptr [esi + 0x384], eax`).
/// Direct callee of 0x0075D150 at 0x0075D223 (vtable 0x00C21BF0 slot 22).
void runSetDrawableField384AndSyncVisibility(
    void* self,
    std::uint32_t selfVa,
    std::uint32_t arg0,
    std::uint8_t globalDEEA50Byte13F,
    std::uint32_t localTempStackVa,
    std::uint32_t helper529FF0ReturnVa,
    std::uint8_t helper7886B0Return,
    CaveCallTrace* trace = nullptr) noexcept;

/// 0x00535410 (inner-patched: site 0x00535428 -> 0x0042DBC6 via slot 0x018ED604,
/// continuation 0x0053542F, replaced `mov dword ptr [eax + 4], 0`).
/// Direct callee of 0x004D8520 at 0x004D8557 (vtable 0x00BDC404 slot 1).
std::uint32_t runCreateRefObjectBE03E4(
    void* outPtr,
    std::uint32_t outPtrVa,
    std::uint32_t arg1,
    std::uint32_t allocatedVa,
    void* allocatedObj,
    CaveCallTrace* trace = nullptr) noexcept;

/// 0x00537A40 (inner-patched: site 0x00537A48 -> 0x00420536 via slot 0x018F29B8,
/// continuation 0x00537A4F, replaced `test byte ptr [ebx + 0x13D], 0x40`).
/// Direct callee of 0x005386A0 at 0x0053873A (vtable 0x00BE1468 slot 32).
void runRemoveFlaggedDrawableFromList50(
    void* self,
    std::uint32_t selfVa,
    void* target,
    std::uint32_t targetVa,
    std::uint32_t globalDEBA64Field58,
    void* globalDEEA50,
    void* removedNode,
    std::uint32_t removedNodeVa,
    void* prevNode,
    void* nextNode,
    void* poolEntry,
    std::uint32_t localEventStackVa,
    void* localEventBuf,
    CaveCallTrace* trace = nullptr) noexcept;

struct FactoryItem538440Entry {
    std::uint32_t itemDescVa;
    std::uint32_t itemKey;
    std::uint32_t matchedTreeKey;
    std::uint32_t factoryObjVa;
    std::uint32_t factoryVt00TargetVa;
    std::uint32_t createdObjVa;
    std::uint32_t allocatedNodeVa;
    void* allocatedNode;
};

/// 0x00538440 (inner-patched: site 0x0053844A -> 0x00456F76 via slot 0x018F2A04,
/// continuation 0x00538450, replaced `mov dword ptr [edi], 0xBE082C`).
/// Direct callee of 0x00540F40 at 0x00540F81 (vtable 0x00BE1174 slot 7).
std::uint32_t runConstructObjectBE082C(
    void* self,
    std::uint32_t selfVa,
    std::uint32_t sourceVa,
    const FactoryItem538440Entry* items,
    std::uint32_t itemCount,
    CaveCallTrace* trace = nullptr) noexcept;

struct RefSubObjectDesc {
    std::uint32_t objVa;
    void* obj;
    std::uint32_t dtorTargetVa;
};

/// 0x0055B530 (inner-patched: site 0x0055B534 -> 0x004036AA via slot 0x018ED650,
/// continuation 0x0055B53A, replaced `lea ecx, [esi + 0x90]`).
/// Direct callee of 0x0051C890 at 0x0051CCB1 (vtable 0x00BDF5A8 slot 26).
void runDestroyAudioEventBDF2C4(
    void* self,
    std::uint32_t selfVa,
    const RefSubObjectDesc& sub20,
    const RefSubObjectDesc& sub1C,
    const RefSubObjectDesc& sub14,
    const RefSubObjectDesc& sub0C,
    const RefSubObjectDesc& sub04,
    CaveCallTrace* trace = nullptr) noexcept;

/// 0x0055CC60 (inner-patched: site 0x0055CC63 -> 0x004025EC via slot 0x018ED450,
/// continuation 0x0055CC69, replaced `mov dword ptr [esi], 0xBE2A48`).
/// Direct callee of 0x0055DC20 at 0x0055DC42 (vtable 0x00BE2BDC slot 5).
std::uint32_t runConstructObjectBE2CD8(
    void* self,
    std::uint32_t selfVa,
    CaveCallTrace* trace = nullptr) noexcept;

/// 0x005838F0 (inner-patched: site 0x00583900 -> 0x0043A04F via slot 0x018EDBC4,
/// continuation 0x00583906, replaced `mov ecx, dword ptr [esi + 0x6C68]`).
/// Direct callee of 0x0058D000 at 0x0058D000 (vtable 0x00BE4D18 slot 4).
void runReleaseLockedObjects6C68And6C6C(
    void* self,
    std::uint32_t lockTempStackVa,
    const RefSubObjectDesc& sub6C68,
    const RefSubObjectDesc& sub6C6C,
    std::uint32_t sub6C6CVt14TargetVa,
    CaveCallTrace* trace = nullptr) noexcept;

/// 0x005A4ED0 (entry-recoverable: site 0x005A4ED0 -> 0x0040BA01 via slot 0x018F27D4,
/// continuation 0x005A4ED7, replaced `cmp dword ptr [0x00DF6F94], 0`).
/// Direct callee of 0x005A4F50 at 0x005A4F56 (vtable 0x00BE5C8C slot 0).
void runReleaseAndClearHandleVector524(
    void* self,
    std::uint32_t selfVa,
    std::uint32_t globalDF6F94,
    const std::uint32_t* handleTargets,
    std::uint32_t handleCount,
    std::uint32_t helper591C30ReturnVa,
    CaveCallTrace* trace = nullptr) noexcept;

/// 0x005C5D70 (inner-patched: site 0x005C5D76 -> 0x00450B64 via slot 0x018ED958,
/// continuation 0x005C5D7C, replaced `mov dword ptr [esi], 0xBE97D4`).
/// Direct callee of 0x005303D0 at 0x005303E4 (vtable 0x00BE0AC0 slot 0).
void runDestroyObjectBE97D4(
    void* self,
    std::uint32_t selfVa,
    CaveCallTrace* trace = nullptr) noexcept;

/// 0x005EF110 (inner-patched: site 0x005EF115 -> 0x0043A77D via slot 0x018F2B10,
/// continuation 0x005EF11B, replaced `mov dword ptr [esi], 0xBE2030`).
/// Direct callee of 0x005BEB80 at 0x005BED6A (vtable 0x00BDEDF0 slot 3).
std::uint32_t runConstructObjectBE2030(
    void* self,
    std::uint32_t selfVa,
    CaveCallTrace* trace = nullptr) noexcept;

/// 0x006232F0 (entry-recoverable: site 0x006232F0 -> 0x00455491 via slot 0x018F2E58,
/// continuation 0x006232F7, replaced `cmp dword ptr [0x00DF6BC8], 0`).
/// Direct callee of 0x005D27C0 at 0x005D2D0F (vtable 0x00BDEDF0 slot 14).
void runShutdownGlobalsDF6BC8AndDF6B34(
    std::uint32_t* globalDF6BC8,
    std::uint32_t globalDF6B34,
    CaveCallTrace* trace = nullptr) noexcept;

/// 0x0064BD80 (inner-patched: site 0x0064BD9A -> 0x00445F07 via slot 0x018F26AC,
/// continuation 0x0064BDA0, replaced `add edi, dword ptr [0x018F9E50]`).
/// Direct callee of 0x0064E3C0 at 0x0064E43E (vtable 0x00BF22E8 slot 10).
void runUpdateSubObject3CAndReleaseB0(
    const void* self,
    void* subObj3C,
    std::uint32_t arg0,
    std::uint32_t securomOffset18F9E50,
    const RefSubObjectDesc& refB0,
    CaveCallTrace* trace = nullptr) noexcept;

/// 0x00683140 (inner-patched: site 0x00683144 -> 0x00459661 via slot 0x018F24C4,
/// continuation 0x0068314A, replaced `mov dword ptr [ecx + 0x180], eax`).
/// Direct callee of 0x00797FA0 at 0x00797FC9 (vtable 0x00C2AE60 slot 4, UnitAIEndPathMoveState).
void runSetObjectField180(void* self, std::uint32_t value) noexcept;

/// 0x00685EB0 (inner-patched: site 0x00685EB3 -> 0x00459EF6 via slot 0x018ED43C,
/// continuation 0x00685EB9, replaced `mov al, byte ptr [eax + 0x144]`).
/// Direct callee of 0x007A4FD0 at 0x007A50D4 (vtable 0x00C2C9E0 slot 5, AIAttackFireWeaponState).
std::uint8_t runReadSubObject4Byte144(const void* subObj4) noexcept;

/// 0x006879B0 (inner-patched: site 0x006879B2 -> 0x00459DE0 via slot 0x018ED738,
/// continuation 0x006879B8, replaced `mov ecx, dword ptr [eax + 0x238]`).
/// Direct callee of 0x007F12B0 at 0x007F136E (vtable 0x00C35158 slot 1, TunnelTracker).
void runDetachSubObject238(
    void* self,
    std::uint32_t selfVa,
    CaveCallTrace* trace = nullptr) noexcept;

/// 0x00688190 (inner-patched: site 0x00688198 -> 0x004399A6 via slot 0x018ED554,
/// continuation 0x0068819F, replaced `mov dword ptr [eax + 4], 0`).
/// Direct callee of 0x007B5E50 at 0x007B5F94 (vtable 0x00C2CFC8 slot 4, AIMeleeReAcquireState).
void runConstructObjectBF9CBC(
    void* self,
    const void* ownerObj,
    std::uint8_t flagByte) noexcept;

struct Module310MapPropagateEntry {
    std::uint32_t moduleVa;
    std::uint32_t vt30TargetVa;
    std::uint32_t subObjVa;
    std::uint32_t subVt00TargetVa;
    std::uint8_t subVt00Return;
    std::uint32_t subVt04TargetVa;
    std::uint32_t subVt18TargetVa;
};

/// 0x0071C460 (inner-patched: site 0x0071C467 -> 0x00456E6C via slot 0x018ED4FC,
/// continuation 0x0071C46D, replaced `mov eax, dword ptr [ebp + 0x418]`).
/// Direct callee of 0x007210D0 at 0x00721180 (vtable 0x00C14790 slot 0).
void runPropagateObjectMap418ToModules310(
    const void* self,
    std::uint32_t selfVa,
    std::uint32_t localMapStackVa,
    void* localMapBuf,
    const Module310MapPropagateEntry* entries,
    std::uint32_t entryCount,
    CaveCallTrace* trace = nullptr) noexcept;

}  // namespace ra3::gamelogic
