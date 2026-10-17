#pragma once

#include <cstdint>

namespace ra3::gamelogic {

// Second pass over clean update/collide methods whose entries lie in
// 0x00690000-0x0069FFFF. Direct callees that are not reconstructed here are
// reached through directCall, which the offline oracle binds to the same
// return sequence on both sides. Virtual calls go through the vtable pointer
// stored in the object.

struct DirectCall {
    std::uint32_t target = 0;
    void* ecx = nullptr;
    std::uint32_t arg0 = 0;
    std::uint32_t arg1 = 0;
    std::uint32_t arg2 = 0;
    std::uint32_t arg3 = 0;
    std::uint32_t arg4 = 0;
    std::uint32_t arg5 = 0;
    int argc = 0;
};

using DirectHandler = std::uint32_t (*)(const DirectCall&);

void setObjectUpdate69BDirect(DirectHandler handler) noexcept;

// 0x00690AF0 SalvageCrateCollide 00C1A178 slot 22.
// Slot 2 returns SalvageCrateCollide.
std::uint8_t salvageCrateCollideAccepts(void* self, const void* other) noexcept;

// 0x00690E60 FireWeaponCollide 00BF7488 slot 22.
// Slot 2 returns FireWeaponCollide.
std::uint8_t fireWeaponCollideMasksAllow(const void* self) noexcept;

// 0x00691250 HordeContain secondary 00C21520 slot 150.
// 0073CB3B stores HordeContain 00C21A98; 0073CB8A stores 00C21520 at +0x16C.
std::uint32_t hordeContainAnchorRejected(void* self) noexcept;

// 0x006912E0 HordeContain secondary 00C21520 slot 45. Tail-calls slot 0xB0.
std::uint32_t hordeContainForwardId(void* self, const void* record) noexcept;

// 0x00691660 HordeContain secondary 00C21520 slot 135.
void hordeContainRelayAnchor(void* self, std::uint32_t argument) noexcept;

// 0x00691C30 HordeContain record object 00C0A7D0 slot 107.
// 0074D2C6 stores HordeContain 00C21A98; 0074D555 stores 00C0A7D0.
const void* hordeContainRecordSpanPointer(const void* self) noexcept;

// 0x00691CB0 HordeContain record object 00C0A7D0 slot 104.
std::uint8_t hordeContainSelectRecordId(const void* self, const void* other, std::uint32_t* outId) noexcept;

// 0x00691E20 GarrisonContain secondary 00C21BF0 slot 62.
// 0074DFD6 stores GarrisonContain 00C21E90; 0074DFEA stores 00C21BF0 at +0x24.
void* garrisonContainSlotPointer(void* self, std::uint32_t index) noexcept;

// 0x00691FC0 GarrisonContain secondary 00C21BF0 slot 71.
std::uint8_t garrisonContainCopySlotTriple(
    void* self, void* destination, std::uint32_t key) noexcept;

// 0x00692270 GarrisonContain secondary 00C21BC8 slot 2.
// 0074DFFF stores 00C21BC8 at +0x30 in the same constructor.
void garrisonContainReleaseIfCount(void* self, std::uint32_t, std::uint32_t, std::uint32_t kind) noexcept;

// 0x00692670 OpenContain 00C1FB90 slot 29. Slot 2 returns OpenContain.
void openContainRefreshMask(void* self) noexcept;

// 0x00692AC0 TransportContain 00C200A0 slot 10. Slot 2 returns TransportContain.
void transportContainRedirect(void* self, std::uint32_t left, std::uint32_t right) noexcept;

// 0x00694F00 ScavengerSpecialPower secondary 00C18DE0 slot 10.
// 006E2954 stores ScavengerSpecialPower 00C18FB8; 006E2961 stores 00C18DE0 at +0x10.
void scavengerSpecialPowerPushScale(void* self, std::uint32_t flag) noexcept;

// 0x00695290 / 00695300 / 00695370 / 00695490 / 006954F0
// SpecialPowerDispatchSpecialPower secondary 00C19058 slots 1, 4, 9, 19, 21.
// 006E2A34 stores 00C19230 (slot 2 returns SpecialPowerDispatchSpecialPower);
// 006E2A41 stores 00C19058 at +0x10.
std::uint8_t specialPowerDispatchAnyReady(void* self) noexcept;
std::uint8_t specialPowerDispatchAnyOrPending(void* self) noexcept;
void specialPowerDispatchStoreArg(void* self, std::uint32_t value) noexcept;
void specialPowerDispatchNotifyClear4C(void* self) noexcept;
void specialPowerDispatchNotifyClear54(void* self) noexcept;

// 0x006963F0 SpecialPowerModule secondary 00C00FD8 slot 73.
// 006955C2 stores SpecialPowerModule 00C01108; 006955C8 stores 00C00FD8 at +0x0C.
void specialPowerModuleScaleIfFlag(void* self, std::uint32_t scaleBits, std::uint32_t flag) noexcept;

// 0x006968A0 / 006968F0 / 00696940 / 00696990 / 006969E0
// SpecialPowerUpdateModule secondary 00BF7C68 slots 11-15.
// 006B479D stores SpecialPowerUpdateModule 00C0B860; 006B47BB stores 00BF7C68.
void specialPowerUpdateFirePacked(void* self, std::uint32_t arg0) noexcept;
void specialPowerUpdateFirePair(void* self, std::uint32_t arg0, std::uint32_t arg1) noexcept;
void specialPowerUpdateFireInsert(void* self, std::uint32_t arg0, std::uint32_t arg1) noexcept;
void specialPowerUpdateFireFloat(void* self, std::uint32_t arg0, std::uint32_t floatBits, std::uint32_t arg2) noexcept;
void specialPowerUpdateFireSpread(void* self, std::uint32_t arg0, std::uint32_t arg1, std::uint32_t arg2) noexcept;

// 0x00696C50 SpecialPowerUpdateModule secondary 00C0B730 slot 73.
// 006B47A3 stores 00C0B730 at +0x0C in the same constructor.
void specialPowerUpdateScaleIfFlag(void* self, std::uint32_t scaleBits, std::uint32_t flag) noexcept;

// 0x00697680 DozerAIUpdate secondary 00C24878 slot 29.
// 00751D25 stores DozerAIUpdate 00C24A50; 00751D5B stores 00C24878.
void dozerAiUpdateIssueOrder(void* self) noexcept;

// 0x00697B50 DockUpdate secondary 00C0BBB0 slot 22.
// 00740541 stores DockUpdate 00C0BD58; 00740555 stores 00C0BBB0 at +0x24.
std::uint8_t dockUpdateForwardVisitor(void* self, std::uint32_t visitor) noexcept;

// 0x006983B0 / 0x006984E0 QueueProductionExitUpdate secondary 00BF851C slots 11 and 1.
// 006982E3 stores QueueProductionExitUpdate 00C01758; 006982F7 stores 00BF851C at +0x24.
std::uint8_t queueProductionExitProject(const void* self, std::uint32_t* outPoint, float* outExtra) noexcept;
std::int32_t queueProductionExitIdle(const void* self) noexcept;

// 0x00699BC0 AIUpdateInterface 00C22118 slot 23. Slot 2 returns AIUpdateInterface.
void aiUpdateStampGoal(void* self, std::uint32_t packed, std::uint32_t stamp) noexcept;

// 0x00699FB0 AIUpdateInterface 00C22118 slot 147.
std::uint32_t aiUpdateGoalKindAllowed(void* self) noexcept;

// 0x0069A040 AIUpdateInterface 00C22118 slot 104.
void aiUpdateApplyFacing(void* self, std::uint32_t facingBits, std::uint32_t stamp) noexcept;

// 0x0069C350 sits on 00C03DA4 slot 0. Slot 2 is 006CDB90 and is not a name getter.
// 0069C1C6, inside 0069C1C0, stores 00C03DA4 at this+0x24 while unwinding the
// object whose primary table is 00C03DC0.
float invisibilityMemberScale(const void* self) noexcept;

// 0x0069C6B0 InvisibilityUpdate secondary 00C03F64 slot 2.
// 0069C3C0 stores InvisibilityUpdate 00C040B0; 0069C3D3 stores 00C03F64 at +0x24.
std::uint8_t invisibilityUpdateArgsDiffer(void* self, std::uint32_t, std::uint32_t left, std::uint32_t right) noexcept;

// 0x0069CCD0 LeechTargetingAttachUpdate 00C209E8 slot 28.
// Slot 2 returns LeechTargetingAttachUpdate.
std::uint32_t leechTargetingAttachForward(void* self, void* record) noexcept;

// 0x0069E6F0 ProductionUpdate secondary 00C19580 slot 30.
// 006E8ED7 stores ProductionUpdate 00C19788; 006E8EEB stores 00C19580 at +0x24.
void* productionUpdateNextReadyLink(void* self) noexcept;

// 0x0069FB60 SpecialAbilityUpdate secondary 00C14184 slot 3.
// 006CF1A0 stores SpecialAbilityUpdate 00C142D8; 006CF1AD stores 00C14184 at +0x24.
std::uint8_t specialAbilityUpdateKindMatch(const void* self) noexcept;

// 0x0069FBC0 SpecialAbilityUpdate 00C142D8 slot 25.
// Slot 2 returns SpecialAbilityUpdate.
std::uint8_t specialAbilityUpdateIsReady(void* self) noexcept;

// 0x0069FCD0 SpecialAbilityUpdate secondary 00C14384 slot 2.
// 006CF199 stores 00C14384 at +0x10 in the same constructor.
std::uint32_t* specialAbilityUpdateResolveOut(void* self, std::uint32_t* outValue) noexcept;

// 0x0069FEC0 StealthDetectorUpdate 00C0DFC0 slot 23.
// Slot 2 returns StealthDetectorUpdate.
void* stealthDetectorUpdateTouch(void* self) noexcept;

} // namespace ra3::gamelogic
