#pragma once

#include <cstdint>

namespace ra3::gamelogic {

// Stand-in for the Sage xfer object. Slot numbers are the byte offsets of the
// virtuals these methods actually call. The oracle installs the same stand-in
// on the C++ side and in the emulator.
struct SkirmishXferOps {
    void* xfer{};
    void (*version)(void* xfer, std::uint8_t* pair) noexcept {};       // +0x18
    void (*field20)(void* xfer, void* field) noexcept {};               // +0x20
    void (*field6c)(void* xfer, void* field) noexcept {};               // +0x6C
    void (*field70)(void* xfer, void* field) noexcept {};               // +0x70
    void (*field84)(void* xfer, void* field) noexcept {};               // +0x84
    void (*field88)(void* xfer, void* field, std::uint32_t name,
                    std::uint32_t count) noexcept {};                   // +0x88
};

// Out-of-range callees. fn is the shipped address. Pointer returns are real
// pointers in the caller's address space; the checker relocates them.
struct SkirmishCalls {
    std::uint8_t* (*allocate)(std::uint32_t size) noexcept {};
    std::uint32_t (*construct3)(std::uint32_t ctor, std::uint8_t* obj, std::uint32_t ai,
                                std::uint32_t player, std::uint32_t record) noexcept {};
    std::uint32_t (*construct4)(std::uint32_t ctor, std::uint8_t* obj, std::uint32_t ai,
                                std::uint32_t player, std::uint32_t record,
                                std::uint32_t packed) noexcept {};
    std::uint32_t (*call0)(std::uint32_t fn, std::uint8_t* self) noexcept {};
    std::uint32_t (*call1)(std::uint32_t fn, std::uint8_t* self, std::uint32_t arg) noexcept {};
    std::uint32_t (*call2)(std::uint32_t fn, std::uint8_t* self, std::uint32_t arg0,
                           std::uint32_t arg1) noexcept {};
    std::uint32_t (*call5)(std::uint32_t fn, std::uint8_t* self, std::uint32_t arg0,
                           std::uint32_t arg1, std::uint32_t arg2, std::uint32_t arg3,
                           std::uint32_t arg4) noexcept {};
    std::uint32_t (*call6)(std::uint32_t fn, std::uint8_t* self, std::uint32_t arg0,
                           std::uint32_t arg1, std::uint32_t arg2, std::uint32_t arg3,
                           std::uint32_t arg4, std::uint32_t arg5) noexcept {};
    void (*cdecl2)(std::uint32_t fn, std::uint32_t arg0, std::uint32_t arg1) noexcept {};
    SkirmishXferOps xfer{};
    std::uint32_t gameFrame{};
    std::uint8_t* catalog{};
};

// AIBuildableUnit 00C52CD8 slots 16, 7 and 19.
std::uint32_t buildableUnitPhaseIsNotOne(const std::uint8_t* self) noexcept;
std::uint32_t buildableUnitPhaseIsOne(const std::uint8_t* self) noexcept;
std::uint32_t copyBuildableUnitAnchor(const std::uint8_t* self, std::uint8_t* dest) noexcept;

// AIBuildableStructure 00C527C8 slot 9. Also installed on the buildable/upgrade tables.
std::uint32_t pushAIBuildableStructureRecord(std::uint8_t* self, std::uint8_t* playerObject,
                                              std::uint32_t clearPhase, SkirmishCalls& calls) noexcept;

// Slot 4 factories. Each passes ai, player index and this+0x38 to its own constructor.
std::uint32_t createAIReactiveDefenseTactic(std::uint8_t* self, SkirmishCalls& calls) noexcept;
std::uint32_t createAISimpleDefenseTactic(std::uint8_t* self, SkirmishCalls& calls) noexcept;
std::uint32_t createAIStaticDefenseTactic(std::uint8_t* self, SkirmishCalls& calls) noexcept;
std::uint32_t createAIExactExpansionTactic(std::uint8_t* self, SkirmishCalls& calls) noexcept;
std::uint32_t createAISimpleExpansionTactic(std::uint8_t* self, SkirmishCalls& calls) noexcept;
std::uint32_t createAIEngineerTactic(std::uint8_t* self, SkirmishCalls& calls) noexcept;
std::uint32_t createAIGarrisonTactic(std::uint8_t* self, SkirmishCalls& calls) noexcept;
std::uint32_t createAISimpleAttackTactic(std::uint8_t* self, SkirmishCalls& calls) noexcept;
std::uint32_t createAISimpleSiegeTactic(std::uint8_t* self, SkirmishCalls& calls) noexcept;
std::uint32_t createAIHuntTactic(std::uint8_t* self, SkirmishCalls& calls) noexcept;
std::uint32_t createAINullTactic(std::uint8_t* self, SkirmishCalls& calls) noexcept;
std::uint32_t createAIFlankAttackTactic(std::uint8_t* self, SkirmishCalls& calls) noexcept;
std::uint32_t createAIScriptedTactic(std::uint8_t* self, SkirmishCalls& calls) noexcept;

// Tactic slot 10/11 updates and the two window predicates (ret 4, argument unused).
std::uint32_t markAISimpleDefenseContact(std::uint8_t* self, SkirmishCalls& calls) noexcept;
std::uint32_t cacheAIEngineerTacticRecord(std::uint8_t* self, SkirmishCalls& calls) noexcept;
std::uint32_t gateAIScriptedTactic(std::uint8_t* self, SkirmishCalls& calls) noexcept;
std::uint32_t tryAISimpleDefenseActivation(std::uint8_t* self, SkirmishCalls& calls) noexcept;
std::uint32_t cacheAIStaticDefensePosition(std::uint8_t* self, SkirmishCalls& calls) noexcept;
std::uint32_t updateAIFlankAttackTactic(std::uint8_t* self, SkirmishCalls& calls) noexcept;
std::uint32_t updateAIReactiveDefenseTactic(std::uint8_t* self, SkirmishCalls& calls) noexcept;
std::uint32_t clearAIReactiveDefenseAlert(std::uint8_t* self, SkirmishCalls& calls) noexcept;
std::uint32_t clearAIHuntAlert(std::uint8_t* self, SkirmishCalls& calls) noexcept;
std::uint32_t aiReactiveDefenseWindowOpen(std::uint8_t* self, SkirmishCalls& calls) noexcept;
std::uint32_t aiHuntWindowOpen(std::uint8_t* self, SkirmishCalls& calls) noexcept;
std::uint32_t updateAISimpleSiegeTactic(std::uint8_t* self, SkirmishCalls& calls) noexcept;
std::uint32_t updateAIScriptedTactic(std::uint8_t* self, SkirmishCalls& calls) noexcept;
std::uint32_t continueAISimpleAttack(std::uint8_t* self, SkirmishCalls& calls) noexcept;
std::uint32_t chooseAIScriptedTacticMode(std::uint8_t* self, SkirmishCalls& calls) noexcept;

// Build-record binders and the xfer snapshots.
std::uint32_t bindAIBuildableUnitTarget(std::uint8_t* self, SkirmishCalls& calls) noexcept;
std::uint32_t bindAIBuildableStructureTarget(std::uint8_t* self, SkirmishCalls& calls) noexcept;

void xferSkirmishAISpecialPowerStore(std::uint8_t* self, SkirmishCalls& calls) noexcept;
void xferAiBuildEfficiencyCalculator(std::uint8_t* self, SkirmishCalls& calls) noexcept;
void xferAINeedsBuilder(std::uint8_t* self, SkirmishCalls& calls) noexcept;
void xferAIRecruitmentManagerDataCache(std::uint8_t* self, SkirmishCalls& calls) noexcept;
void xferAIUpgradeScience(std::uint8_t* self, SkirmishCalls& calls) noexcept;
void xferAIUnitUpgrader(std::uint8_t* self, SkirmishCalls& calls) noexcept;
void xferAIOpeningMover(std::uint8_t* self, SkirmishCalls& calls) noexcept;
void xferAIRebuildableStructure(std::uint8_t* self, SkirmishCalls& calls) noexcept;

}  // namespace ra3::gamelogic
