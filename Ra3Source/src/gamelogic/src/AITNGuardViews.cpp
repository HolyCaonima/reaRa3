#include <ra3/gamelogic/AITNGuardViews.h>

#include <cmath>
#include <limits>

namespace ra3::gamelogic {
namespace {

// These are the 50 nontrivial clean native bodies accepted for this port. The
// address-qualified names are intentional where RA3 has no surviving symbol:
// they record translation-unit membership without inventing a C++ method name.
constexpr std::array<EntryEvidence, 50> entries{{
    {0x007A68C0, 36, "AITNGuardMachine::deletingDestructor", false, false},
    {0x0079B4D0, 55, "AITNGuardInnerState::xfer", false, false},
    {0x0079B510, 185, "AITNGuard.cpp::sub_0079B510", false, false},
    {0x0079B5D0, 48, "AITNGuardInnerState::onExit", false, false},
    {0x0079B610, 48, "AITNGuardOuterState::onExit", false, false},
    {0x0079B640, 65, "AITNGuardIdleState::xfer", false, false},
    {0x0079B690, 144, "AITNGuardIdleState::onEnter", true, false},
    {0x0079B720, 94, "AITNGuardPickUpCrateState::onEnter", false, false},
    {0x0079B790, 83, "AITNGuardAttackAggressorState::onExit", false, false},
    {0x0079B930, 61, "AITNGuard.cpp::sub_0079B930", false, false},
    {0x0079B9A0, 68, "AITNGuard.cpp::sub_0079B9A0", false, false},
    {0x0079B9F0, 54, "AITNGuard.cpp::sub_0079B9F0", false, false},
    {0x0079BA30, 38, "AITNGuard.cpp::sub_0079BA30", false, false},
    {0x0079BA60, 57, "AITNGuard.cpp::sub_0079BA60", false, false},
    {0x0079BB00, 27, "AITNGuard.cpp::sub_0079BB00", false, false},
    {0x0079BB20, 283, "AITNGuard.cpp::sub_0079BB20", true, false},
    {0x0079BC50, 136, "AITNGuard.cpp::sub_0079BC50", false, false},
    {0x0079BCE0, 23, "AITNGuard.cpp::sub_0079BCE0", true, true},
    {0x0079BD40, 122, "AITNGuard.cpp::sub_0079BD40", false, false},
    {0x0079BDC0, 34, "AITNGuard.cpp::sub_0079BDC0", false, false},
    {0x0079BDF0, 191, "AITNGuard.cpp::sub_0079BDF0", true, false},
    {0x0079BED0, 83, "AITNGuard.cpp::classifyEndpointPair", true, true},
    {0x0079BF30, 124, "AITNGuard.cpp::sub_0079BF30", true, false},
    {0x0079BFB0, 50, "AITNGuard.cpp::sub_0079BFB0", false, false},
    {0x0079BFF0, 46, "AITNGuard.cpp::sub_0079BFF0", false, false},
    {0x0079C020, 93, "AITNGuard.cpp::objectCooldownElapsed", true, true},
    {0x0079C080, 100, "AITNGuard.cpp::targetCooldownElapsed", true, true},
    {0x0079C100, 45, "AITNGuard.cpp::setTrackedTarget", true, true},
    {0x0079C130, 45, "AITNGuard.cpp::sub_0079C130", false, false},
    {0x0079C160, 32, "AITNGuard.cpp::sub_0079C160", false, false},
    {0x0079C180, 28, "AITNGuard.cpp::sub_0079C180", false, false},
    {0x0079C1A0, 17, "AITNGuard.cpp::sub_0079C1A0", false, false},
    {0x0079C1C0, 28, "AITNGuard.cpp::sub_0079C1C0", false, false},
    {0x0079C1E0, 17, "AITNGuard.cpp::sub_0079C1E0", false, false},
    {0x0079C280, 31, "AITNGuard.cpp::sub_0079C280", false, false},
    {0x0079C2A0, 83, "AITNGuard.cpp::isCandidateNear", true, true},
    {0x0079C300, 64, "AITNGuard.cpp::sub_0079C300", false, false},
    {0x0079C340, 67, "AITNGuard.cpp::isNearGuardPoint", true, true},
    {0x0079C390, 51, "AITNGuard.cpp::sub_0079C390", false, false},
    {0x0079C3D0, 67, "AITNGuard.cpp::isNearReturnPoint", true, true},
    {0x007CE860, 235, "AITNGuard.cpp::findBestTunnel", true, false},
    {0x007CEB30, 706, "AITNGuardMachine::lookForInnerTarget", true, false},
    {0x007CEE00, 226, "AITNGuardInnerState::onEnter", true, false},
    {0x007CEEF0, 403, "AITNGuardInnerState::update", true, false},
    {0x007CF090, 243, "AITNGuardOuterState::onEnter", true, false},
    {0x007CF190, 227, "AITNGuardOuterState::update", true, false},
    {0x007CF280, 196, "AITNGuardReturnState::onEnter", true, false},
    {0x007CF360, 459, "AITNGuardIdleState::update", true, false},
    {0x007CF530, 313, "AITNGuardAttackAggressorState::onEnter", true, false},
    {0x007CF670, 177, "AITNGuardAttackAggressorState::update", true, false},
}};

}  // namespace

// The class and method intent is cross-checked against:
// GeneralsMD/Code/GameEngine/Include/GameLogic/AITNGuard.h:54-208 and
// GeneralsMD/Code/GameEngine/Source/GameLogic/AI/AITNGuard.cpp:130-877.
// Layouts, constants, return values, and branch order here come from RA3.

bool attackDeadlineExpired(
    std::uint32_t frame, std::uint32_t deadline) noexcept
{
    return frame >= deadline;
}

StateResult deadlineState(
    std::uint32_t frame, std::uint32_t deadline) noexcept
{
    return attackDeadlineExpired(frame, deadline)
        ? StateResult::success
        : StateResult::continueState;
}

std::uint32_t byteWithPointerUpperBits(
    std::uint32_t pointer, std::uint8_t value) noexcept
{
    return (pointer & 0xFFFFFF00U) | value;
}

std::int32_t conditionalPayloadOffset(
    bool present, std::int32_t mode) noexcept
{
    return present && mode == 1 ? 0x74 : -1;
}

std::uint32_t trackedTargetId(
    bool present, std::uint32_t targetId) noexcept
{
    return present ? targetId : 0U;
}

std::uint32_t classifyEndpointPair(
    std::uint32_t first, std::uint32_t second,
    std::uint32_t expectedFirst, std::uint32_t expectedSecond) noexcept
{
    const bool firstMatches =
        first == expectedFirst || first == expectedSecond;
    const bool secondMatches =
        second == expectedFirst || second == expectedSecond;
    if (firstMatches != secondMatches) {
        return firstMatches ? 1U : 2U;
    }
    if (first == expectedFirst && second == expectedSecond) {
        return 1U;
    }
    if (first == expectedSecond && second == expectedFirst) {
        return 2U;
    }
    return 0U;
}

bool objectCooldownElapsed(
    std::uint32_t objectId, std::uint32_t previousId,
    std::int32_t frame, std::int32_t previousFrame,
    float cooldownFrames) noexcept
{
    if (objectId != previousId) {
        return true;
    }
    return static_cast<float>(frame)
        >= static_cast<float>(previousFrame) + cooldownFrames;
}

std::uint32_t objectCooldownResult(
    std::uint32_t objectId, std::uint32_t previousId,
    std::int32_t frame, std::int32_t previousFrame,
    float cooldownFrames, std::uint32_t slot) noexcept
{
    const bool elapsed = objectCooldownElapsed(
        objectId, previousId, frame, previousFrame, cooldownFrames);
    const std::uint32_t carrier = objectId == previousId
        ? static_cast<std::uint32_t>(previousFrame)
        : slot;
    return byteWithPointerUpperBits(
        carrier, static_cast<std::uint8_t>(elapsed ? 1U : 0U));
}

bool targetCooldownElapsed(
    std::uint32_t objectId, std::uint32_t previousId,
    std::int32_t frame, std::int32_t previousFrame,
    float cooldownFrames, std::int32_t mode) noexcept
{
    return mode == 1
        || objectCooldownElapsed(
            objectId, previousId, frame, previousFrame, cooldownFrames);
}

std::uint32_t targetCooldownResult(
    std::uint32_t objectId, std::uint32_t previousId,
    std::int32_t frame, std::int32_t previousFrame,
    float cooldownFrames, std::int32_t mode, std::uint32_t slot) noexcept
{
    const bool elapsed = targetCooldownElapsed(
        objectId, previousId, frame, previousFrame, cooldownFrames, mode);
    const std::uint32_t carrier = objectId == previousId && mode != 1
        ? static_cast<std::uint32_t>(previousFrame)
        : slot;
    return byteWithPointerUpperBits(
        carrier, static_cast<std::uint8_t>(elapsed ? 1U : 0U));
}

bool isNear2D(
    Coord2D value, Coord2D center, float distanceSquared) noexcept
{
    const float dx = value.x - center.x;
    const float dy = value.y - center.y;
    return dx * dx + dy * dy < distanceSquared;
}

std::uint32_t candidateNearResult(
    std::uint32_t pointer, bool disabled, bool near) noexcept
{
    if (pointer == 0U || disabled) {
        return byteWithPointerUpperBits(pointer, 1U);
    }
    return near ? 1U : 0U;
}

const GuardCandidate* findNearestTunnel(
    const std::vector<GuardCandidate>& candidates, Coord2D position) noexcept
{
    const GuardCandidate* best = nullptr;
    float bestDistance = std::numeric_limits<float>::max();
    for (const GuardCandidate& candidate : candidates) {
        if (!candidate.present) {
            continue;
        }
        const float dx = candidate.position.x - position.x;
        const float dy = candidate.position.y - position.y;
        const float distance = dx * dx + dy * dy;
        if (best == nullptr || distance < bestDistance) {
            best = &candidate;
            bestDistance = distance;
        }
    }
    return best;
}

void beginIdle(
    IdleStateView& state, std::uint32_t frame,
    std::uint32_t randomDelay) noexcept
{
    state.nextEnemyScanFrame = frame + randomDelay;
}

IdleUpdateResult updateIdle(
    IdleStateView& state, const IdleUpdateInput& input) noexcept
{
    IdleUpdateResult output{};
    if (input.frame < state.nextEnemyScanFrame) {
        output.sleepFrames = state.nextEnemyScanFrame - input.frame;
        return output;
    }

    state.nextEnemyScanFrame = input.frame + input.scanPeriod;
    output.sleepFrames = input.scanPeriod;
    output.clearGoal = true;
    if (input.hasCrate) {
        output.requestCrateState = true;
        return output;
    }
    if (input.foundInnerTarget) {
        if (!input.contained) {
            output.state = StateResult::success;
            output.sleepFrames = 0;
            return output;
        }
        if (!input.exitAvailable) {
            output.state = StateResult::failure;
            output.sleepFrames = 0;
            return output;
        }
        if (!input.exitBusy) {
            output.requestEmergencyExit = true;
        }
        output.sleepFrames = 0;
        return output;
    }
    if (!input.contained && input.hasReturnTunnel) {
        output.state = StateResult::failure;
        output.sleepFrames = 0;
    }
    return output;
}

AttackUpdateResult updateInnerAttack(
    const AttackUpdateInput& input) noexcept
{
    AttackUpdateResult output{};
    if (!input.attackStatePresent) {
        return output;
    }
    output.state = input.attackUpdate;
    if (!input.attackHasGoal && input.nemesisPresent) {
        output.setGoalToNemesis = true;
        output.callAttackEnter = true;
        output.state = input.attackEnter;
    }
    return output;
}

AttackUpdateResult updateOuterAttack(
    const AttackUpdateInput& input) noexcept
{
    AttackUpdateResult output = updateInnerAttack(input);
    if (input.attackStatePresent && !input.attackHasGoal
        && !input.nemesisPresent && input.commonTargetEnabled
        && input.teamTargetPresent) {
        output.setGoalToNemesis = false;
        output.setGoalToTeamTarget = true;
        output.callAttackEnter = true;
        output.state = input.attackEnter;
    }
    return output;
}

const std::array<EntryEvidence, 50>& aitnGuardEntries() noexcept
{
    return entries;
}

}  // namespace ra3::gamelogic
