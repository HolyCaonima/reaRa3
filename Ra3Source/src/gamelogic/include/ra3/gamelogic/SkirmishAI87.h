#pragma once

#include <cstdint>

namespace ra3::gamelogic {

// Stand-in for the Sage xfer object used by the skirmish-AI snapshot methods.
// Slot numbers are byte offsets of the virtuals these functions actually call.
// The oracle supplies both sides with the same stand-in behavior.
struct XferOps {
    void* xfer{};
    void (*version)(void* xfer, std::uint8_t* pair) noexcept {};       // +0x18
    void (*field20)(void* xfer, void* field) noexcept {};               // +0x20
    void (*field58)(void* xfer, void* field) noexcept {};               // +0x58
    void (*field64)(void* xfer, void* field) noexcept {};               // +0x64
    void (*field6c)(void* xfer, void* field) noexcept {};               // +0x6C
    void (*field70)(void* xfer, void* field) noexcept {};               // +0x70
    void (*field84)(void* xfer, void* field) noexcept {};               // +0x84
    void (*field88)(void* xfer, void* field, std::uint32_t name,
                    std::uint32_t count) noexcept {};                   // +0x88
};

struct RandomWords {
    std::uint32_t word[6]{};
};

// AIStaticDefenseTactic 00C5336C slot 8. Writes mode 1 and a 9-dword record.
std::uint32_t applyStaticDefenseTacticRecord(std::uint8_t* target) noexcept;

// AIHuntTactic 00C53560 slot 8.
std::uint32_t applyHuntTacticFlags(std::uint8_t* target) noexcept;

// AISimpleDefenseTactic 00C53334 slot 8.
std::uint32_t applySimpleDefenseTacticMode(std::uint8_t* target) noexcept;

// AISimpleSiegeTactic 00C53524 slot 8.
std::uint32_t applySimpleSiegeTacticFlag(std::uint8_t* target) noexcept;

// AISimpleDefenseTactic 00C53334 slot 7.
std::uint32_t clearSimpleDefenseAlertByte(std::uint8_t* self) noexcept;

// AIStaticDefenseTactic 00C5336C slot 5. Also installed on flank and simple attack.
std::uint32_t tacticArgumentPresent(std::uint32_t argument) noexcept;

// AIRebuildableStructure 00C53048 slot 19.
std::uint32_t copyRebuildableStructurePosition(std::uint8_t* self, std::uint8_t* destination) noexcept;

// AIBuildableStructure 00C527C8 slot 19.
std::uint32_t copyBuildableStructureAnchor(std::uint8_t* self, std::uint8_t* destination) noexcept;

// AIRebuildableStructure 00C53048 slots 24 and 28.
std::uint32_t rebuildableStructurePositionPointer(const std::uint8_t* self) noexcept;
std::uint32_t rebuildableStructureRadiusPointer(const std::uint8_t* self) noexcept;

// AIRebuildableStructure 00C53048 slot 27. Float bits are the x87 result of fld/fldz.
struct RadiusResult {
    std::uint32_t pointer{};
    std::uint32_t floatBits{};
};
RadiusResult rebuildableStructureRadiusOrZero(const std::uint8_t* self) noexcept;

// AIBuildableStructure 00C527C8 slot 8. Also AIRebuildableStructure slot 8.
void copyBuildableStructureScore(std::uint8_t* self) noexcept;

// AIRedAlertManager 00C51768 slots 8 and 6. Frame is TheGameLogic+0x58 (global 0x00DEBA64).
std::uint32_t aiRedAlertFrameReached(const std::uint8_t* self, std::uint32_t frame) noexcept;
std::uint32_t armAIRedAlertFrame(std::uint8_t* self, std::uint32_t gameLogic,
                                 std::uint32_t frame) noexcept;

// AISpecialPowerUpdatableAttackObject 00C517F0 slot 5.
std::uint32_t attackObjectPresent(const std::uint8_t* self) noexcept;

// AIReactiveDefenseTactic 00C532FC slot 8 (shared by the other tactic tables).
// Pushes AITactic.cpp and stores logicRandomInteger(3, 5) at target+0x214.
std::uint32_t rollAITacticActivation(std::uint8_t* target, RandomWords& logic) noexcept;

// EASTL rbtree successor used by 0x00885BB0. Not a counted function.
std::uint8_t* treeSuccessor(std::uint8_t* node) noexcept;

// Called from AISimpleExpansionTactic::cache at 0x00886648.
std::uint32_t findSimpleExpansionRecord(std::uint8_t* header, std::uint32_t key) noexcept;

// AISimpleExpansionTactic 00C533DC slot 10.
std::uint32_t cacheSimpleExpansionRecord(std::uint8_t* self) noexcept;

// AIBuildableUnit 00C52CD8 slot 17. Tail-calls vtable+0x28 unless +0x14 is already 4.
using PhaseSlot = std::uint32_t (*)(std::uint8_t* self, std::uint32_t phase) noexcept;
std::uint32_t ensureBuildablePhaseFour(std::uint8_t* self, PhaseSlot invoke) noexcept;

void xferPrerequisiteBuilder(std::uint8_t* self, XferOps& ops) noexcept;
void xferUnitBuilder(std::uint8_t* self, XferOps& ops) noexcept;
void xferBuildableStructureFactory(std::uint8_t* self, XferOps& ops) noexcept;
void xferEmergencyManager(std::uint8_t* self, XferOps& ops) noexcept;
void xferSubsystemEnabler(std::uint8_t* self, XferOps& ops) noexcept;
void xferEconomyManager(std::uint8_t* self, XferOps& ops) noexcept;
void xferRedAlertManager(std::uint8_t* self, XferOps& ops) noexcept;
void xferAIBank(std::uint8_t* self, XferOps& ops) noexcept;
void xferBuildHeuristicStore(std::uint8_t* self, XferOps& ops) noexcept;

// 00C52768 slot 3. Slot 2 is 0x00411440, not a class-name getter.
// Shared snapshot body used by AIBuildableUnit and AIUpgrade.
void xferSharedBuildRecord(std::uint8_t* self, XferOps& ops) noexcept;

void xferBuildableUnit(std::uint8_t* self, XferOps& ops) noexcept;
void xferAIUpgrade(std::uint8_t* self, XferOps& ops) noexcept;
void xferUpgradeOpeningMove(std::uint8_t* self, XferOps& ops) noexcept;
void xferSpecialPowerAttackObject(std::uint8_t* self, XferOps& ops) noexcept;
void xferStrategyManager(std::uint8_t* self, XferOps& ops) noexcept;

}  // namespace ra3::gamelogic
