#include "support/TestHarness.h"

#include <ra3/gamelogic/ScriptsCluster.h>

#include <algorithm>
#include <cstring>
#include <vector>

RA3_TEST(scripts82InventorySeparatesReconstructedFromInventoryOnly)
{
    using namespace ra3::gamelogic;
    const auto& inv = scripts82Inventory();
    const auto reversedCount = std::count_if(
        inv.begin(), inv.end(), [](const Scripts82Entry& e) {
            return e.status == Scripts82Status::reversed;
        });
    const auto hookCount = std::count_if(
        inv.begin(), inv.end(), [](const Scripts82Entry& e) {
            return e.hookSafe;
        });
    RA3_CHECK_EQ(inv.size(), 64U);
    RA3_CHECK_EQ(reversedCount, 36);
    RA3_CHECK_EQ(hookCount, 10);
}

RA3_TEST(scripts82ActionPredicatesAndTeamInvolvedMatchShippedTables)
{
    using namespace ra3::gamelogic;
    RA3_CHECK(isUnitAction(33));
    RA3_CHECK(isUnitAction(0x129));
    RA3_CHECK(!isUnitAction(12));
    RA3_CHECK_EQ(isUnitActionEax(0x129), 0x00000101U);

    RA3_CHECK(isTeamAction(12));
    RA3_CHECK(isTeamAction(0x11A));
    RA3_CHECK(isTeamAction(210));
    RA3_CHECK(!isTeamAction(33));
    RA3_CHECK_EQ(getTeamInvolvedParamIndex(12), 0);
    RA3_CHECK_EQ(getTeamInvolvedParamIndex(210), 1);
    RA3_CHECK_EQ(getTeamInvolvedParamIndex(352), 1);
    RA3_CHECK_EQ(getTeamInvolvedParamIndex(385), 1);
    RA3_CHECK_EQ(getTeamInvolvedParamIndex(33), -1);

    RA3_CHECK(isTeamOrUnitAction(13));
    RA3_CHECK(isTeamOrUnitAction(0x11F));
    RA3_CHECK(!isTeamOrUnitAction(12));
}

RA3_TEST(scripts82ParameterAndSideQualificationBehaveCorrectly)
{
    using namespace ra3::gamelogic;
    ScriptParameterView param = initParameter(0x10, 42);
    param.coord = {10.5F, -20.25F, 30.0F};
    ScriptCoord3D out{};
    getParameterCoord3D(param, out);
    RA3_CHECK_EQ(out.x, 10.5F);
    RA3_CHECK_EQ(out.y, -20.25F);
    RA3_CHECK_EQ(out.z, 30.0F);

    param.paramType = 0x0B;
    param.stringValue = "<Local Player's Enemies>";
    qualifyPlayerSide(param);
    RA3_CHECK_EQ(param.stringValue, "<1st Human Player's Enemies>");

    RA3_CHECK_EQ(
        qualifyWithCurrentSidePrefix("PlyrAllies", "AttackFlag"),
        "PlyrAllies/AttackFlag");
    RA3_CHECK_EQ(
        qualifyWithCurrentSidePrefix("PlyrAllies", "PlyrSoviet/AttackFlag"),
        "PlyrSoviet/AttackFlag");
}

RA3_TEST(scripts82ScriptEvaluationAndConditionLeafHelpers)
{
    using namespace ra3::gamelogic;
    ScriptView script = initScript();
    script.curTime = 99.0F;
    script.frameToEvaluateAt = 50U;
    script.normal = false;
    RA3_CHECK(!canEvaluateScript(script, 1, false, false, 0, 100U));
    RA3_CHECK_EQ(script.curTime, 0.0F);
    RA3_CHECK(canEvaluateScript(script, 2, false, false, 0, 50U));
    RA3_CHECK(!canEvaluateScript(script, 2, false, false, 0, 49U));

    RA3_CHECK(evaluatePlatform("PC"));
    RA3_CHECK(!evaluatePlatform("360"));
    RA3_CHECK(!evaluatePlatform("PS3"));

    std::vector<WaypointStub> waypoints = {{7, "Bridge_01"}, {9, "Bridge_02"}};
    std::vector<std::pair<std::int32_t, std::int32_t>> states = {{7, 1}, {9, 2}};
    RA3_CHECK(!evaluateBridgeBroken("Bridge_01", waypoints, states));
    RA3_CHECK(evaluateBridgeBroken("Bridge_02", waypoints, states));
}
