#pragma once

#include "ra3/gamelogic/ProtectedCaveLogic.h"

#include <cstdint>

namespace ra3::gamelogic {

std::uint16_t caveRead16(CaveState& st, std::uint32_t addr);
void caveWrite16(CaveState& st, std::uint32_t addr, std::uint16_t value);

// 0x004D5550 inner-patched site 0x004D5577 slot 0x018ED888 -> 0x004D557D
// direct callee of 0x0071C540 at 0x0071C639; 0x0071C540 is 0x00C27548 slot 1
void runNotifySpecialPowerReady(CaveState& st, std::uint32_t obj);

// 0x00502700 inner-patched site 0x00502703 slot 0x018EDC04 -> 0x0050270A
// direct callee of 0x005064A0 at 0x005064B1; 0x005064A0 is 0x00BDF5A8 slot 89
void runStopSubChannelStream(CaveState& st);

// 0x00505060 entry-recoverable site 0x00505060 slot 0x018ED768 -> 0x00505067
// direct callee of 0x0051EFF0 at 0x0051F0E7; 0x0051EFF0 is 0x00BDF5A8 slot 5
void runUpdateSubChannelEnvelope(CaveState& st, std::uint32_t dtBits);

// 0x00513340 inner-patched site 0x00513353 slot 0x018F2750 -> 0x00513359
// direct callee of 0x005134D0 at 0x005134E2; 0x005134D0 is 0x00BDF5A8 slot 36
void runCreateRefCountedStateObject(CaveState& st, std::uint32_t outPtr);

// 0x0051F780 inner-patched site 0x0051F785 slot 0x018F25B8 -> 0x0051F78B
// direct callee of 0x0052F5A0 at 0x0052F77D; 0x0052F5A0 is 0x00BDFADC slot 2
void runApplyTransformAndRefreshView(CaveState& st, std::uint32_t a0, std::uint32_t a1,
                                     std::uint32_t a2, std::uint32_t a3, std::uint32_t a4);

// 0x00521050 inner-patched site 0x00521053 slot 0x018EDDEC -> 0x00521059
// direct callee of 0x00579700 at 0x00579744; 0x00579700 is 0x00BE5580 slot 1
void runResolveObjectTeamOrPlayerEntry(CaveState& st);

// 0x005259E0 inner-patched site 0x005259E3 slot 0x018F2DD0 -> 0x005259EA
// direct callee of 0x006FE4A0 at 0x006FEA1A; 0x006FE4A0 is 0x00BF7E40 slot 5
void runFlushDirtySubObjectsAndNotify(CaveState& st, std::uint32_t forceFlag);

// 0x0054DF20 inner-patched site 0x0054DF33 slot 0x018ED9F0 -> 0x0054DF3A
// direct callee of 0x00864630 at 0x0086467E; 0x00864630 is 0x00C4F778 slot 4
void runInitTerrainRenderHelper(CaveState& st, std::uint32_t provider);

// 0x0057F540 inner-patched site 0x0057F548 slot 0x018F293C -> 0x0057F54E
// direct callee of 0x0071FAB0 at 0x0071FD38; 0x0071FAB0 is 0x00C11090 slot 3
void runInitLocalizedProjectileString(CaveState& st, std::uint32_t srcPtr);

// 0x00598630 inner-patched site 0x00598637 slot 0x018EDCE0 -> 0x0059863D
// direct callee of 0x005986B0 at 0x00598739; 0x005986B0 is 0x00BE4290 slot 6
void runDestroySubObjectArrayBuffer(CaveState& st);

// 0x005B5180 inner-patched site 0x005B518C slot 0x018F290C -> 0x005B5193
// direct callee of 0x005E8B00 at 0x005E8F12; 0x005E8B00 is 0x00BEF40C slot 0
void runIsIndexedTableEntryEmpty(CaveState& st, std::uint32_t index);

// 0x005E65B0 inner-patched site 0x005E65B4 slot 0x018ED564 -> 0x005E65BA
// direct callee of 0x005EECE0 at 0x005EECE3; 0x005EECE0 is 0x00BEF5C8 slot 0
void runDestroyGameInfoBase(CaveState& st);

// 0x005E6B30 inner-patched site 0x005E6B3D slot 0x018EDC50 -> 0x005E6B44
// 0x00BE2030 slot 4 (GameSlot)
void runResetGameSlot(CaveState& st);

// 0x00674640 inner-patched site 0x0067465A slot 0x018F2CC8 -> 0x00674662
// direct callee of 0x00674690 at 0x006746B4; 0x00674690 is 0x00BF3AE0 slot 5
void runRegisterEntryInTaggedMap(CaveState& st, std::uint32_t entry);

// 0x0067E600 inner-patched site 0x0067E60F slot 0x018F251C -> 0x0067E615
// direct callee of 0x006812F0 at 0x0068131D; 0x006812F0 is 0x00C00F4C slot 33
void runFindModuleMatchingSubObject(CaveState& st, std::uint32_t target, std::uint32_t obj);

// 0x006885D0 inner-patched site 0x006885E1 slot 0x018ED534 -> 0x006885E8
// direct callee of 0x006C89E0 at 0x006C8A9E; 0x006C89E0 is 0x00C19C68 slot 23
void runInitKindOfFilterPredicate(CaveState& st, std::uint32_t filterTarget,
                                  std::uint32_t arg1, std::uint32_t invertFlag);

// 0x0068C150 inner-patched site 0x0068C154 slot 0x018F2A58 -> 0x0068C15A
// direct callee of 0x006C9DB0 at 0x006C9EF4; 0x006C9DB0 is 0x00C1FB90 slot 22
void runHaveDifferentContainerStatuses(CaveState& st, std::uint32_t objA, std::uint32_t objB);

// 0x006A8400 entry-recoverable site 0x006A8400 slot 0x018F28C4 -> 0x006A8406
// direct callee of 0x006ED440 at 0x006ED471; 0x006ED440 is 0x00BF96A8 slot 4
void runTriggerWeaponInitAudioEvent(CaveState& st);

// 0x006C0620 inner-patched site 0x006C0623 slot 0x018F2B84 -> 0x006C0629
// direct callee of 0x006FD280 at 0x006FD3D6; 0x006FD280 is 0x00C1BC90 slot 39
void runResolveObjectIndicatorColor(CaveState& st);

// 0x006C2C30 inner-patched site 0x006C2C33 slot 0x018ED5E0 -> 0x006C2C39
// direct callee of 0x007028B0 at 0x0070295D; 0x007028B0 is 0x00C06420 slot 0
void runUpdateObjectPowerRadarStatusBit(CaveState& st, std::uint32_t enableFlag);

// 0x006C4F10 inner-patched site 0x006C4F53 slot 0x018F2C0C -> 0x006C4F5B
// direct callee of 0x006E6830 at 0x006E6B21; 0x006E6830 is 0x00C01E40 slot 25
void runDispatchVec3CommandMessage(CaveState& st, std::uint32_t vec3Ptr, std::uint32_t cmdCtx);

// 0x006C5480 inner-patched site 0x006C54B3 slot 0x018ED740 -> 0x006C54BB
// direct callee of 0x007512C0 at 0x00751377; 0x007512C0 is 0x00C1C560 slot 29
void runDispatchTargetCommandMessage3D(CaveState& st, std::uint32_t targetArg, std::uint32_t cmdCtx);

// 0x006C8380 inner-patched site 0x006C8384 slot 0x018F2B94 -> 0x006C838A
// direct callee of 0x006D9C20 at 0x006D9C37; 0x006D9C20 is 0x00C11D28 slot 0
void runDestroyActiveBodyBase(CaveState& st);

// 0x006CACE0 inner-patched site 0x006CACE6 slot 0x018F28A8 -> 0x006CACEC
// 0x00C00F4C slot 13
void runTryExecuteSpecialPowerOnTarget(CaveState& st, std::uint32_t arg0,
                                       std::uint32_t targetObj, std::uint32_t arg2);

// 0x006D1DD0 inner-patched site 0x006D1DD4 slot 0x018EDA4C -> 0x006D1DDA
// direct callee of 0x006FE4A0 at 0x006FEA0D; 0x006FE4A0 is 0x00BF7E40 slot 5
void runRefreshObjectModulesAndDrawableState(CaveState& st);

// 0x006E4760 inner-patched site 0x006E476E slot 0x018ED40C -> 0x006E4774
// direct callee of 0x0079CD20 at 0x0079CD3C; 0x0079CD20 is 0x00C29F40 slot 4
void runFindSelectableObjectAndQueryModule44(CaveState& st, std::uint32_t outObj, std::uint32_t objId);

// 0x006EFE40 inner-patched site 0x006EFE43 slot 0x018F2654 -> 0x006EFE49
// direct callee of 0x006FFC00 at 0x006FFC7D; 0x006FFC00 is 0x00C22118 slot 99
void runResolveObjectByProducerId(CaveState& st);

// 0x006F1350 inner-patched site 0x006F1356 slot 0x018F2574 -> 0x006F135C
// direct callee of 0x007CA9E0 at 0x007CAA89; 0x007CA9E0 is 0x00C31DC8 slot 5
void runIsObjectMobileAndControllable(CaveState& st);

// 0x0072CF30 inner-patched site 0x0072CF37 slot 0x018EDA38 -> 0x0072CF3D
// direct callee of 0x0072CFD0 at 0x0072D07B; 0x0072CFD0 is 0x00C22118 slot 22
void runApplyDrawableTintEnvelope(CaveState& st, std::uint32_t tintVec3);

// 0x007461C0 inner-patched site 0x007461CD slot 0x018EDD24 -> 0x007461D4
// direct callee of 0x0074634F at 0x007465A4; 0x0074634F is 0x00C8CA1C slot 0
void runCopyWeaponAimTransformAndUpdate(CaveState& st, std::uint32_t targetObj,
                                        std::uint32_t arg1, std::uint32_t arg2);

}  // namespace ra3::gamelogic
