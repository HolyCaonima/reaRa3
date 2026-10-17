#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <vector>

namespace ra3::gamelogic {

enum class StateResult : std::int32_t {
    continueState = 0,
    success = -1,
    failure = -2
};

struct Coord2D {
    float x;
    float y;
};

struct GuardCandidate {
    std::uint32_t id;
    Coord2D position;
    bool present;
    bool enemy;
    bool attackable;
};

struct GuardMachineView {
    Coord2D positionToGuard{};
    std::uint32_t nemesisId = 0;
    std::int32_t guardMode = 0;
};

struct IdleStateView {
    std::uint32_t nextEnemyScanFrame = 0;
};

struct IdleUpdateInput {
    std::uint32_t frame = 0;
    std::uint32_t scanPeriod = 0;
    bool hasCrate = false;
    bool foundInnerTarget = false;
    bool contained = false;
    bool exitAvailable = false;
    bool exitBusy = false;
    bool hasReturnTunnel = false;
};

struct IdleUpdateResult {
    StateResult state = StateResult::continueState;
    std::uint32_t sleepFrames = 0;
    bool clearGoal = false;
    bool requestCrateState = false;
    bool requestEmergencyExit = false;
};

struct AttackUpdateInput {
    bool attackStatePresent = false;
    bool attackHasGoal = false;
    bool nemesisPresent = false;
    bool commonTargetEnabled = false;
    bool teamTargetPresent = false;
    StateResult attackUpdate = StateResult::continueState;
    StateResult attackEnter = StateResult::continueState;
};

struct AttackUpdateResult {
    StateResult state = StateResult::success;
    bool setGoalToNemesis = false;
    bool setGoalToTeamTarget = false;
    bool callAttackEnter = false;
};

struct InventoryEntry {
    std::uint32_t address;
    std::uint32_t size;
    const char* name;
    bool directPort;
    bool hookSafe;
};

// RA3 uses unsigned frame comparisons for state deadlines.
bool attackDeadlineExpired(
    std::uint32_t frame, std::uint32_t deadline) noexcept;                 // 0x0079B470 pattern
StateResult deadlineState(
    std::uint32_t frame, std::uint32_t deadline) noexcept;

// The original two accessors replace AL after first loading a pointer in EAX.
std::uint32_t byteWithPointerUpperBits(
    std::uint32_t pointer, std::uint8_t value) noexcept;                   // 0x0079BEB0/0x0079BEC0
std::int32_t conditionalPayloadOffset(
    bool present, std::int32_t mode) noexcept;                              // 0x0079BCE0
std::uint32_t trackedTargetId(
    bool present, std::uint32_t targetId) noexcept;                         // 0x0079C100

// Returns 1 for the forward pairing, 2 for the reverse pairing, otherwise 0.
std::uint32_t classifyEndpointPair(
    std::uint32_t first, std::uint32_t second,
    std::uint32_t expectedFirst, std::uint32_t expectedSecond) noexcept;   // 0x0079BED0

bool objectCooldownElapsed(
    std::uint32_t objectId, std::uint32_t previousId,
    std::int32_t frame, std::int32_t previousFrame,
    float cooldownFrames) noexcept;                                        // 0x0079C020
std::uint32_t objectCooldownResult(
    std::uint32_t objectId, std::uint32_t previousId,
    std::int32_t frame, std::int32_t previousFrame,
    float cooldownFrames, std::uint32_t slot) noexcept;
bool targetCooldownElapsed(
    std::uint32_t objectId, std::uint32_t previousId,
    std::int32_t frame, std::int32_t previousFrame,
    float cooldownFrames, std::int32_t mode) noexcept;                     // 0x0079C080
std::uint32_t targetCooldownResult(
    std::uint32_t objectId, std::uint32_t previousId,
    std::int32_t frame, std::int32_t previousFrame,
    float cooldownFrames, std::int32_t mode, std::uint32_t slot) noexcept;

bool isNear2D(
    Coord2D value, Coord2D center, float distanceSquared) noexcept;         // 0x0079C2A0/340/3D0
std::uint32_t candidateNearResult(
    std::uint32_t pointer, bool disabled, bool near) noexcept;              // 0x0079C2A0 EAX/AL convention

// Analysis-only policy models for selected state behavior. These omit
// game-owned calls/effects and are not direct ports of the addressed bodies.
const GuardCandidate* findNearestTunnel(
    const std::vector<GuardCandidate>& candidates, Coord2D position) noexcept; // 0x007CE860

void beginIdle(
    IdleStateView& state, std::uint32_t frame,
    std::uint32_t randomDelay) noexcept;                                   // 0x0079B690
IdleUpdateResult updateIdle(
    IdleStateView& state, const IdleUpdateInput& input) noexcept;           // 0x007CF360
AttackUpdateResult updateInnerAttack(
    const AttackUpdateInput& input) noexcept;                               // 0x007CEEF0
AttackUpdateResult updateOuterAttack(
    const AttackUpdateInput& input) noexcept;                               // 0x007CF190

// Static-analysis inventory. Only entries with directPort=true are exact C++
// reconstructions; the others remain identity evidence or policy models.
const std::array<InventoryEntry, 50>& aitnGuardInventory() noexcept;

}  // namespace ra3::gamelogic
