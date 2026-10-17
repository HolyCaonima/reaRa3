#include "support/TestHarness.h"

#include <ra3/gamelogic/AITNGuardViews.h>

#include <algorithm>
#include <vector>

RA3_TEST(aitnGuardDirectLeafReconstructionsPreserveRa3Comparisons)
{
    using namespace ra3::gamelogic;
    RA3_CHECK_EQ(byteWithPointerUpperBits(0x12345678U, 0xABU), 0x123456ABU);
    RA3_CHECK_EQ(classifyEndpointPair(1U, 2U, 1U, 2U), 1U);
    RA3_CHECK_EQ(classifyEndpointPair(2U, 1U, 1U, 2U), 2U);
    RA3_CHECK_EQ(classifyEndpointPair(1U, 3U, 1U, 2U), 1U);
    RA3_CHECK(objectCooldownElapsed(2U, 1U, 0, 100, 50.0F));
    RA3_CHECK(!objectCooldownElapsed(1U, 1U, 149, 100, 50.0F));
    RA3_CHECK(objectCooldownElapsed(1U, 1U, 150, 100, 50.0F));
}

RA3_TEST(aitnGuardDeadlinePolicyModelUsesUnsignedComparison)
{
    using namespace ra3::gamelogic;
    RA3_CHECK(attackDeadlineExpired(10U, 10U));
    RA3_CHECK(!attackDeadlineExpired(9U, 10U));
}

RA3_TEST(aitnGuardNearestTunnelPolicyModelSkipsMissingObjects)
{
    using namespace ra3::gamelogic;
    const std::vector<GuardCandidate> candidates{
        {1U, {10.0F, 0.0F}, true, false, false},
        {2U, {1.0F, 0.0F}, false, false, false},
        {3U, {2.0F, 0.0F}, true, false, false}};
    const GuardCandidate* result = findNearestTunnel(candidates, {0.0F, 0.0F});
    RA3_REQUIRE(result != nullptr);
    RA3_CHECK_EQ(result->id, 3U);
}

RA3_TEST(aitnGuardIdleTransitionPolicyModelRemainsBounded)
{
    using namespace ra3::gamelogic;
    IdleStateView state{};
    beginIdle(state, 100U, 7U);
    IdleUpdateInput input{};
    input.frame = 103U;
    RA3_CHECK_EQ(updateIdle(state, input).sleepFrames, 4U);

    input.frame = 107U;
    input.scanPeriod = 15U;
    input.foundInnerTarget = true;
    const IdleUpdateResult attack = updateIdle(state, input);
    RA3_CHECK(attack.state == StateResult::success);
    RA3_CHECK_EQ(attack.sleepFrames, 0U);
}

RA3_TEST(aitnGuardInventorySeparatesDirectPortsFromFingerprints)
{
    using namespace ra3::gamelogic;
    const auto& inventory = aitnGuardInventory();
    const auto directPorts = std::count_if(
        inventory.begin(), inventory.end(), [](const InventoryEntry& entry) {
            return entry.directPort;
        });
    const auto hooks = std::count_if(
        inventory.begin(), inventory.end(), [](const InventoryEntry& entry) {
            return entry.hookSafe;
        });
    RA3_CHECK_EQ(inventory.size(), 50U);
    RA3_CHECK_EQ(directPorts, 8);
    RA3_CHECK_EQ(hooks, 8);
}
