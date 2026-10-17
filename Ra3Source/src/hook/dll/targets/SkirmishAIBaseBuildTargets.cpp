#include "../Targets.h"

#include <ra3/gamelogic/SkirmishAIBaseBuild.h>

#include <cstdint>

namespace ra3hook {
namespace {

namespace sbb = ra3::gamelogic::skirmishai_base_build;

constexpr auto relaxed = std::memory_order_relaxed;

TargetState g_findEntry20ByBitState;
TargetState g_findEntryByTemplateIdState;
TargetState g_groupContainsTemplateIdState;
TargetState g_canAffordRequestState;
TargetState g_isCandidateBetterThanBestState;
TargetState g_findBestMatchingBaseState;

std::uint32_t addressValue(const void* value) noexcept {
    return static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(value));
}

// 1. 0x00882AE0 (RVA 0x00482AE0) — AIBuildPersonalityConfig::findEntry20ByBit
const void* __fastcall hookedFindEntry20ByBit(
    const void* self,
    void* /*edx*/,
    std::uint32_t bitIndex) noexcept {
    auto& state = g_findEntry20ByBitState;
    state.calls.fetch_add(1, relaxed);
    const void* const ours = sbb::findEntry20ByBit_882AE0(self, bitIndex);
    if (state.mode == Mode::replace) {
        return ours;
    }
    using OriginalFn = const void*(__thiscall*)(const void*, std::uint32_t);
    const void* const game =
        reinterpret_cast<OriginalFn>(state.original)(self, bitIndex);
    recordScalar(
        "SkirmishAIBaseBuild.findEntry20ByBit",
        state,
        addressValue(game),
        addressValue(ours));
    return game;
}

// 2. 0x00887300 (RVA 0x00487300) — AIBuildableRegistry::findEntryByTemplateId
void* __fastcall hookedFindEntryByTemplateId(
    const void* self,
    void* /*edx*/,
    std::uint32_t templateId) noexcept {
    auto& state = g_findEntryByTemplateIdState;
    state.calls.fetch_add(1, relaxed);
    void* const ours = sbb::findEntryByTemplateId_887300(self, templateId);
    if (state.mode == Mode::replace) {
        return ours;
    }
    using OriginalFn = void*(__thiscall*)(const void*, std::uint32_t);
    void* const game =
        reinterpret_cast<OriginalFn>(state.original)(self, templateId);
    recordScalar(
        "SkirmishAIBaseBuild.findEntryByTemplateId",
        state,
        addressValue(game),
        addressValue(ours));
    return game;
}

// 3. 0x0088D120 (RVA 0x0048D120) — AIBuildableGroup::containsTemplateId
std::uint8_t __fastcall hookedGroupContainsTemplateId(
    const void* self,
    void* /*edx*/,
    std::uint32_t templateId) noexcept {
    auto& state = g_groupContainsTemplateIdState;
    state.calls.fetch_add(1, relaxed);
    const std::uint8_t ours =
        sbb::groupContainsTemplateId_88D120(self, templateId) ? 1u : 0u;
    if (state.mode == Mode::replace) {
        return ours;
    }
    using OriginalFn = std::uint8_t(__thiscall*)(const void*, std::uint32_t);
    const std::uint8_t game =
        reinterpret_cast<OriginalFn>(state.original)(self, templateId);
    recordScalar(
        "SkirmishAIBaseBuild.groupContainsTemplateId",
        state,
        game != 0u ? 1u : 0u,
        ours);
    return game;
}

// 5. 0x008900B0 (RVA 0x004900B0) — AIBankAccount::canAffordRequest
std::uint8_t __fastcall hookedCanAffordRequest(
    const sbb::AIBankAccountView* self,
    void* /*edx*/,
    float requestCost,
    float requestPriority,
    float timeHorizon) noexcept {
    auto& state = g_canAffordRequestState;
    state.calls.fetch_add(1, relaxed);
    const std::uint8_t ours = sbb::canAffordRequest_8900B0(
        self, requestCost, requestPriority, timeHorizon) ? 1u : 0u;
    if (state.mode == Mode::replace) {
        return ours;
    }
    using OriginalFn = std::uint8_t(__thiscall*)(
        const sbb::AIBankAccountView*, float, float, float);
    const std::uint8_t game = reinterpret_cast<OriginalFn>(state.original)(
        self, requestCost, requestPriority, timeHorizon);
    recordScalar(
        "SkirmishAIBaseBuild.canAffordRequest",
        state,
        game != 0u ? 1u : 0u,
        ours);
    return game;
}

// 6. 0x00890600 (RVA 0x00490600) — AIBaseQueryCriteria::isCandidateBetterThanBest
std::uint8_t __fastcall hookedIsCandidateBetterThanBest(
    const sbb::AIBaseQueryCriteriaView* self,
    void* /*edx*/,
    const sbb::AIBaseView* candidateBase,
    const sbb::AIBaseView* currentBestBase) noexcept {
    auto& state = g_isCandidateBetterThanBestState;
    state.calls.fetch_add(1, relaxed);
    const std::uint8_t ours = sbb::isCandidateBetterThanBest_890600(
        self, candidateBase, currentBestBase) ? 1u : 0u;
    if (state.mode == Mode::replace) {
        return ours;
    }
    using OriginalFn = std::uint8_t(__thiscall*)(
        const sbb::AIBaseQueryCriteriaView*,
        const sbb::AIBaseView*,
        const sbb::AIBaseView*);
    const std::uint8_t game = reinterpret_cast<OriginalFn>(state.original)(
        self, candidateBase, currentBestBase);
    recordScalar(
        "SkirmishAIBaseBuild.isCandidateBetterThanBest",
        state,
        game != 0u ? 1u : 0u,
        ours);
    return game;
}

// 7. 0x008908C0 (RVA 0x004908C0) — AIBaseBuilder::findBestMatchingBase
sbb::AIBaseView* __fastcall hookedFindBestMatchingBase(
    const void* self,
    void* /*edx*/,
    const sbb::AIBaseQueryCriteriaConfigView* config,
    const sbb::Coord3DView* referencePos) noexcept {
    auto& state = g_findBestMatchingBaseState;
    state.calls.fetch_add(1, relaxed);
    sbb::AIBaseView* const ours =
        sbb::findBestMatchingBase_8908C0(self, config, referencePos);
    if (state.mode == Mode::replace) {
        return ours;
    }
    using OriginalFn = sbb::AIBaseView*(__thiscall*)(
        const void*,
        const sbb::AIBaseQueryCriteriaConfigView*,
        const sbb::Coord3DView*);
    sbb::AIBaseView* const game =
        reinterpret_cast<OriginalFn>(state.original)(self, config, referencePos);
    recordScalar(
        "SkirmishAIBaseBuild.findBestMatchingBase",
        state,
        addressValue(game),
        addressValue(ours));
    return game;
}

}  // namespace

void appendSkirmishAIBaseBuildTargets(std::vector<Target>& table) {
    table.insert(table.end(), {
        {
            "SkirmishAIBaseBuild.findEntry20ByBit",
            0x00482AE0U,
            {0x8B, 0x09, 0x8B, 0x81, 0x10, 0x01, 0x00, 0x00,
             0x8B, 0x91, 0x0C, 0x01, 0x00, 0x00, 0x56, 0x8D},
            8U,
            reinterpret_cast<void*>(&hookedFindEntry20ByBit),
            &g_findEntry20ByBitState,
            true,
        },
        {
            "SkirmishAIBaseBuild.findEntryByTemplateId",
            0x00487300U,
            {0x56, 0x8B, 0x74, 0x24, 0x08, 0x8D, 0x51, 0x1C,
             0x57, 0x8B, 0x7A, 0x04, 0x33, 0xC9, 0x8B, 0xFF},
            5U,
            reinterpret_cast<void*>(&hookedFindEntryByTemplateId),
            &g_findEntryByTemplateIdState,
            true,
        },
        {
            "SkirmishAIBaseBuild.groupContainsTemplateId",
            0x0048D120U,
            {0x51, 0x8B, 0x41, 0x0C, 0x53, 0x55, 0x8B, 0x69,
             0x14, 0x56, 0x8B, 0x71, 0x10, 0x3B, 0xF5, 0x57},
            5U,
            reinterpret_cast<void*>(&hookedGroupContainsTemplateId),
            &g_groupContainsTemplateIdState,
            true,
        },
        {
            "SkirmishAIBaseBuild.canAffordRequest",
            0x004900B0U,
            {0xF3, 0x0F, 0x10, 0x44, 0x24, 0x08, 0xD9, 0xEE,
             0xF3, 0x0F, 0x10, 0x6C, 0x24, 0x0C, 0x56, 0x8D},
            6U,
            reinterpret_cast<void*>(&hookedCanAffordRequest),
            &g_canAffordRequestState,
            true,
        },
        {
            "SkirmishAIBaseBuild.isCandidateBetterThanBest",
            0x00490600U,
            {0x83, 0xEC, 0x08, 0x8B, 0x44, 0x24, 0x0C, 0x85,
             0xC0, 0x56, 0x8B, 0xF1, 0x89, 0x74, 0x24, 0x04},
            7U,
            reinterpret_cast<void*>(&hookedIsCandidateBetterThanBest),
            &g_isCandidateBetterThanBestState,
            true,
        },
        {
            "SkirmishAIBaseBuild.findBestMatchingBase",
            0x004908C0U,
            {0x83, 0xEC, 0x20, 0x33, 0xC0, 0x83, 0xC8, 0x04,
             0x89, 0x44, 0x24, 0x0C, 0x8B, 0x44, 0x24, 0x28},
            5U,
            reinterpret_cast<void*>(&hookedFindBestMatchingBase),
            &g_findBestMatchingBaseState,
            true,
        },
    });
}

}  // namespace ra3hook
