#include <ra3/gamelogic/ScriptsCluster.h>

#include <algorithm>
#include <cstring>

namespace ra3::gamelogic {
namespace {

constexpr std::array<std::int32_t, 90> kUnitActionTypes = {
    33,  34,  35,  36,  37,  40,  41,  42,  45,  47,  49,  50,  52,  63,  64,
    66,  67,  70,  74,  77,  94,  95,  96,  118, 134, 135, 136, 137, 140, 151,
    152, 153, 157, 158, 161, 162, 168, 181, 192, 194, 198, 201, 214, 220, 226,
    230, 231, 235, 248, 255, 260, 268, 269, 270, 297, 301, 306, 307, 319, 320,
    321, 322, 323, 324, 325, 327, 345, 346, 347, 362, 367, 375, 376, 378, 388,
    391, 395, 402, 411, 415, 416, 417, 428, 431, 440, 441, 453, 466, 467, 468
};

constexpr std::array<std::int32_t, 106> kTeamActionTypes = {
    12,  13,  29,  30,  32,  38,  43,  44,  46,  48,  51,  53,  62,  65,  68,
    71,  82,  90,  92,  93,  98,  99,  117, 127, 141, 142, 143, 144, 145, 146,
    147, 149, 154, 155, 156, 159, 160, 164, 165, 166, 173, 185, 186, 187, 190,
    191, 193, 195, 196, 199, 200, 204, 205, 206, 207, 208, 209, 210, 211, 212,
    213, 227, 232, 233, 237, 246, 249, 250, 261, 265, 266, 267, 282, 287, 288,
    289, 290, 293, 294, 295, 296, 298, 299, 308, 309, 326, 329, 348, 352, 355,
    361, 363, 368, 374, 377, 379, 385, 389, 392, 396, 412, 413, 414, 418, 429,
    454
};

constexpr std::array<std::int32_t, 104> kTeamOrUnitActionTypes = {
    13,  29,  32,  33,  34,  40,  41,  42,  43,  44,  45,  46,  47,  48,  49,
    50,  51,  52,  53,  90,  92,  93,  94,  95,  96,  98,  99,  137, 151, 152,
    154, 155, 157, 158, 159, 160, 161, 162, 164, 165, 166, 185, 190, 191, 192,
    193, 196, 199, 200, 204, 205, 206, 207, 208, 209, 210, 211, 212, 213, 214,
    230, 231, 232, 233, 237, 246, 287, 288, 289, 290, 295, 296, 297, 306, 307,
    308, 309, 319, 320, 321, 322, 323, 324, 345, 346, 347, 348, 355, 361, 367,
    368, 376, 377, 378, 379, 413, 414, 415, 416, 417, 418, 428, 430, 431
};

template <std::size_t N>
bool containsSorted(const std::array<std::int32_t, N>& table, std::int32_t value) noexcept
{
    return std::binary_search(table.begin(), table.end(), value);
}

std::uint8_t* rawBytes(void* ptr) noexcept
{
    return static_cast<std::uint8_t*>(ptr);
}

const std::uint8_t* rawBytes(const void* ptr) noexcept
{
    return static_cast<const std::uint8_t*>(ptr);
}

template <typename T>
T readAt(const void* base, std::size_t offset) noexcept
{
    T value{};
    std::memcpy(&value, rawBytes(base) + offset, sizeof(T));
    return value;
}

SightingKey readSightingNodeKey(const void* node) noexcept
{
    return SightingKey{
        readAt<std::uint32_t>(node, 0x10),
        readAt<std::uint32_t>(node, 0x14),
        readAt<std::uint32_t>(node, 0x18),
        readAt<std::uint32_t>(node, 0x1C),
    };
}

constexpr std::array<Scripts82Entry, 64> kScripts82Manifest = {{
    {0x00804590U,   68U, 0xA6C4617AU, Scripts82Status::reversed,      true,  "ScriptAction::isUnitAction"},
    {0x00804780U,  104U, 0x33A1ABE4U, Scripts82Status::reversed,      true,  "ScriptAction::isTeamAction"},
    {0x008049A0U,   73U, 0x4B81251DU, Scripts82Status::reversed,      true,  "ScriptAction::isTeamOrUnitAction"},
    {0x0081A3A0U,   31U, 0xAC6E416DU, Scripts82Status::inventoryOnly, false, "ScriptBase::scalarDeletingDestructor"},
    {0x0081A3C0U,   53U, 0xA3A5158EU, Scripts82Status::inventoryOnly, false, "ScriptSubobject::scalarDeletingDestructor"},
    {0x0081A400U,   18U, 0x6F2ED002U, Scripts82Status::inventoryOnly, false, "ScriptEngine::forwardGlobalCallback"},
    {0x0081A420U,  106U, 0x2E18B822U, Scripts82Status::reversed,      false, "ScriptEngine::canEvaluateScript"},
    {0x0081A4A0U,   45U, 0xEB204A9AU, Scripts82Status::reversed,      false, "ScriptEngine::clearTrackedObjectPointer"},
    {0x0081A4D0U,   65U, 0x014F6440U, Scripts82Status::reversed,      false, "ScriptEngine::setSequentialTimerForObject"},
    {0x0081A520U,   52U, 0x98879165U, Scripts82Status::reversed,      false, "ScriptEngine::setSequentialTimerForTeam"},
    {0x0081A560U,   75U, 0x9E634368U, Scripts82Status::inventoryOnly, false, "ScriptEngine::findTrackedObjectByTemplateHandle"},
    {0x0081A5B0U,   82U, 0xB0153D66U, Scripts82Status::reversed,      false, "ScriptEngine::notifyOfObjectDestruction"},
    {0x0081A610U,   82U, 0x0FD1A43FU, Scripts82Status::inventoryOnly, false, "ScriptEngine::applyParameterTemplateHandle"},
    {0x0081A700U,   73U, 0x805ABF6CU, Scripts82Status::reversed,      false, "ScriptList::markAllScriptsAndGroupsActive"},
    {0x0081A750U,   46U, 0x7EBBBA12U, Scripts82Status::reversed,      true,  "Parameter::getCoord3D"},
    {0x0081AA40U,   26U, 0xE4E98D4DU, Scripts82Status::reversed,      true,  "IntVector::sumElements"},
    {0x0081AA60U,   63U, 0xA0E768F1U, Scripts82Status::reversed,      true,  "SightingKey::less"},
    {0x0081AAA0U,  184U, 0x18BBC6C7U, Scripts82Status::reversed,      true,  "SightingTree::find"},
    {0x0081AB60U,  103U, 0xEB16C86EU, Scripts82Status::reversed,      true,  "SightingTree::lowerBound"},
    {0x0081B130U,   70U, 0x0C9B3BE8U, Scripts82Status::reversed,      false, "Parameter::Parameter"},
    {0x0081B600U,   58U, 0x4C8CE17CU, Scripts82Status::inventoryOnly, false, "ScriptEngine::triggerEvaOrAudioEvent"},
    {0x0081BE00U,   44U, 0xB73D4987U, Scripts82Status::reversed,      false, "SequentialScript::SequentialScript"},
    {0x0081BEC0U,   37U, 0x64E98E78U, Scripts82Status::inventoryOnly, false, "ScriptEngine::seedFrameRandomState"},
    {0x0081BEF0U,   42U, 0x6AC7114EU, Scripts82Status::inventoryOnly, false, "ScriptEngine::seedClientRandomState"},
    {0x0081BF20U,   88U, 0xE6CC11E9U, Scripts82Status::inventoryOnly, false, "ScriptEngine::releaseAllFlagNodes"},
    {0x0081BF90U,   82U, 0x57EC88C4U, Scripts82Status::reversed,      false, "ScriptCounterDefaultMap::lookupOrDefault"},
    {0x0081BFF0U,   97U, 0xE1C7C578U, Scripts82Status::reversed,      false, "ScriptEngine::isTrackedObjectDestroyedByName"},
    {0x0081C3E0U,   95U, 0x7DFF4396U, Scripts82Status::reversed,      false, "Script::Script"},
    {0x0081C530U,  134U, 0x8E6EBBD5U, Scripts82Status::inventoryOnly, false, "ActionTemplate::ActionTemplate"},
    {0x0081DD80U,   51U, 0xB2FC04ADU, Scripts82Status::inventoryOnly, false, "ScriptStringNode::ScriptStringNode"},
    {0x0081DE20U,   76U, 0xF187D6FDU, Scripts82Status::reversed,      true,  "ScriptIndexedEntry::lowerBoundById"},
    {0x0081DE70U,  204U, 0xA8E2794AU, Scripts82Status::reversed,      false, "ScriptListIndex::lowerBoundByName"},
    {0x0081DF50U,   97U, 0xF2232F06U, Scripts82Status::reversed,      true,  "DualHashTree::find"},
    {0x0081DFC0U,   61U, 0xD097AEFEU, Scripts82Status::reversed,      false, "ScriptWaypointInfo::copyFrom"},
    {0x0081E030U,   57U, 0xB7976532U, Scripts82Status::inventoryOnly, false, "ScriptListEntry::copyFrom"},
    {0x0081F710U,  156U, 0x99894E3FU, Scripts82Status::reversed,      false, "ScriptConditions::evaluateSkirmishCommandButtonIsReady"},
    {0x0081F7B0U,  111U, 0x6F859EB2U, Scripts82Status::reversed,      false, "ScriptConditions::evaluateBridgeBroken"},
    {0x0081F820U,  114U, 0x64436FADU, Scripts82Status::inventoryOnly, false, "ScriptConditions::evaluateTechTreeStatus"},
    {0x0081F8A0U,   91U, 0x5F7520BEU, Scripts82Status::reversed,      false, "ScriptConditions::evaluatePlatform"},
    {0x0081F900U,  242U, 0x325F230DU, Scripts82Status::inventoryOnly, false, "ScriptEngine::appendDebugWatchMessage"},
    {0x0081FA00U,  209U, 0x7E52E431U, Scripts82Status::reversed,      false, "ScriptEngine::findFlagByName"},
    {0x0081FAE0U,   90U, 0x785D6B4DU, Scripts82Status::reversed,      false, "ScriptEngine::qualifyWithCurrentSidePrefix"},
    {0x0081FB40U,  247U, 0x04B1F285U, Scripts82Status::reversed,      false, "ScriptEngine::findAttackInfo"},
    {0x0081FC40U,  107U, 0xF47B2AA5U, Scripts82Status::reversed,      false, "ScriptEngine::getObjectCount"},
    {0x0081FCB0U,   47U, 0x44AB8722U, Scripts82Status::inventoryOnly, false, "OrCondition::scalarDeletingDestructor"},
    {0x0081FCE0U,  117U, 0x0DB1E2AEU, Scripts82Status::inventoryOnly, false, "OrCondition::duplicate"},
    {0x0081FD60U,   43U, 0x22AD90C5U, Scripts82Status::reversed,      false, "ScriptEngine::hasSightingEntry"},
    {0x0081FD90U,  236U, 0x7EB6247DU, Scripts82Status::inventoryOnly, false, "ScriptEngine::formatScriptLogMessage"},
    {0x0081FE80U,  616U, 0x354E3B67U, Scripts82Status::inventoryOnly, false, "ScriptEngine::getTeamNamed"},
    {0x008200F0U,  183U, 0xB0FFA8DCU, Scripts82Status::inventoryOnly, false, "ScriptEngine::evaluateConditionTeamOrUnit"},
    {0x008201B0U,  197U, 0xABD47C69U, Scripts82Status::reversed,      false, "ScriptEngine::isSpecialPowerTriggeredOrComplete"},
    {0x00820280U,  140U, 0x1BE2C6B1U, Scripts82Status::reversed,      false, "ScriptEngine::isScienceAcquiredOrUpgradeComplete"},
    {0x00820370U,  337U, 0xFDAB65A5U, Scripts82Status::inventoryOnly, false, "ScriptEngine::recordSpecialPowerTriggered"},
    {0x008204D0U,   91U, 0x97F8D437U, Scripts82Status::inventoryOnly, false, "ScriptEngine::recordScienceOrUpgradeEvent"},
    {0x00820530U,   30U, 0xC166B1B7U, Scripts82Status::inventoryOnly, false, "Parameter::Destructor"},
    {0x00820550U,  102U, 0x026CD74EU, Scripts82Status::inventoryOnly, false, "Condition::scalarDeletingDestructor"},
    {0x008205C0U,  111U, 0x1D9AC021U, Scripts82Status::inventoryOnly, false, "Condition::setConditionType"},
    {0x00820630U,  317U, 0xD32BF855U, Scripts82Status::reversed,      false, "Parameter::qualifyPlayerSide"},
    {0x00820780U,  992U, 0xC9DBE9E5U, Scripts82Status::inventoryOnly, false, "Parameter::ReadParameterDataChunk"},
    {0x00820B70U,  239U, 0x24AAE396U, Scripts82Status::reversed,      false, "ScriptAction::setActionType"},
    {0x00820C60U,  102U, 0x2BE7CED8U, Scripts82Status::inventoryOnly, false, "ScriptAction::scalarDeletingDestructor"},
    {0x00820CD0U,  289U, 0xBCCF4D3EU, Scripts82Status::inventoryOnly, false, "ScriptAction::duplicate"},
    {0x00820E00U,  175U, 0xBCE7AF8FU, Scripts82Status::reversed,      false, "ScriptAction::getTeamInvolved"},
    {0x00821080U, 1942U, 0x26A4140CU, Scripts82Status::inventoryOnly, false, "Condition::ParseConditionDataChunk"},
}};

}  // namespace

// 1. 0x00804590: ScriptAction::isUnitAction
bool isUnitAction(std::int32_t actionType) noexcept
{
    return containsSorted(kUnitActionTypes, actionType);
}

std::uint32_t isUnitActionEax(std::int32_t actionType) noexcept
{
    const auto raw = static_cast<std::uint32_t>(actionType);
    const std::uint32_t low = isUnitAction(actionType) ? 1U : 0U;
    std::uint32_t high = 0U;
    if (actionType == 0x129) {
        high = raw & 0xFFFFFF00U;
    } else if (actionType < 0x129) {
        const std::uint32_t sub = raw - 0x21U;
        high = sub <= 0xEDU ? 0U : (sub & 0xFFFFFF00U);
    } else {
        const std::uint32_t sub = raw - 0x12DU;
        high = sub & 0xFFFFFF00U;
    }
    return high | low;
}

// 2. 0x00804780: ScriptAction::isTeamAction
bool isTeamAction(std::int32_t actionType) noexcept
{
    return containsSorted(kTeamActionTypes, actionType);
}

std::uint32_t isTeamActionEax(std::int32_t actionType) noexcept
{
    const auto raw = static_cast<std::uint32_t>(actionType);
    const std::uint32_t low = isTeamAction(actionType) ? 1U : 0U;
    std::uint32_t high = 0U;
    if (actionType == 0x11A || (actionType >= 0x8D && actionType <= 0x93)) {
        high = raw & 0xFFFFFF00U;
    } else if (actionType < 0x8D) {
        const std::uint32_t sub = raw - 0x0CU;
        high = sub <= 0x73U ? 0U : (sub & 0xFFFFFF00U);
    } else if (actionType <= 0x119) {
        const std::uint32_t sub = raw - 0x95U;
        high = sub & 0xFFFFFF00U;
    } else {
        const std::uint32_t sub = raw - 0x11FU;
        high = sub & 0xFFFFFF00U;
    }
    return high | low;
}

// 3. 0x008049A0: ScriptAction::isTeamOrUnitAction
bool isTeamOrUnitAction(std::int32_t actionType) noexcept
{
    return containsSorted(kTeamOrUnitActionTypes, actionType);
}

std::uint32_t isTeamOrUnitActionEax(std::int32_t actionType) noexcept
{
    const auto raw = static_cast<std::uint32_t>(actionType);
    const std::uint32_t low = isTeamOrUnitAction(actionType) ? 1U : 0U;
    std::uint32_t high = 0U;
    if (actionType >= 0x11F && actionType <= 0x122) {
        high = raw & 0xFFFFFF00U;
    } else if (actionType < 0x11F) {
        const std::uint32_t sub = raw - 0x0DU;
        high = sub <= 0xE9U ? 0U : (sub & 0xFFFFFF00U);
    } else {
        const std::uint32_t sub = raw - 0x127U;
        high = sub & 0xFFFFFF00U;
    }
    return high | low;
}

// 4. 0x0081A420: ScriptEngine::canEvaluateScript
bool canEvaluateScript(
    ScriptView& script,
    std::int32_t engineDifficulty,
    bool hasCurrentPlayer,
    bool currentPlayerHasAi,
    std::int32_t aiDifficulty,
    std::uint32_t currentFrame) noexcept
{
    script.curTime = 0.0F;
    if (!script.isScriptActiveRuntime) {
        return false;
    }
    std::int32_t difficulty = engineDifficulty;
    if (hasCurrentPlayer) {
        difficulty = currentPlayerHasAi ? aiDifficulty : engineDifficulty;
    }
    switch (static_cast<std::uint32_t>(difficulty)) {
        case 0U:
            if (!script.easy) {
                return false;
            }
            break;
        case 1U:
            if (!script.normal) {
                return false;
            }
            break;
        case 2U:
        case 3U:
            if (!script.hard) {
                return false;
            }
            break;
        default:
            break;
    }
    return currentFrame >= script.frameToEvaluateAt;
}

// 5. 0x0081A4A0: ScriptEngine::clearTrackedObjectPointer
void clearTrackedObjectPointer(
    std::vector<TrackedNamedObjectEntry>& entries,
    std::uint32_t objectPtr) noexcept
{
    for (TrackedNamedObjectEntry& entry : entries) {
        if (entry.objectPtr == objectPtr) {
            entry.objectPtr = 0U;
            return;
        }
    }
}

// 6. 0x0081A4D0: ScriptEngine::setSequentialTimerForObject
void setSequentialTimerForObject(
    std::vector<SequentialScriptView>& seqScripts,
    bool objectPresent,
    std::uint32_t objectId,
    std::int32_t frameCount) noexcept
{
    if (!objectPresent) {
        return;
    }
    for (SequentialScriptView& seq : seqScripts) {
        if (seq.objectId == objectId) {
            seq.framesToWait = frameCount;
            return;
        }
    }
}

// 7. 0x0081A520: ScriptEngine::setSequentialTimerForTeam
void setSequentialTimerForTeam(
    std::vector<SequentialScriptView>& seqScripts,
    std::uint32_t teamPtr,
    std::int32_t frameCount) noexcept
{
    if (teamPtr == 0U) {
        return;
    }
    for (SequentialScriptView& seq : seqScripts) {
        if (seq.teamToExecOn == teamPtr) {
            seq.framesToWait = frameCount;
            return;
        }
    }
}

// 8. 0x0081A5B0: ScriptEngine::notifyOfObjectDestruction
void notifyOfObjectDestruction(
    std::vector<TrackedNamedObjectEntry>& entries,
    std::uint32_t& cachedObject17D70,
    std::uint32_t& cachedObject17D68,
    std::uint32_t destroyedObjectPtr) noexcept
{
    for (TrackedNamedObjectEntry& entry : entries) {
        if (entry.objectPtr == destroyedObjectPtr) {
            entry.objectPtr = 0U;
            break;
        }
    }
    if (cachedObject17D70 == destroyedObjectPtr) {
        cachedObject17D70 = 0U;
    }
    if (cachedObject17D68 == destroyedObjectPtr) {
        cachedObject17D68 = 0U;
    }
}

// 9. 0x0081A700: ScriptList::markAllScriptsAndGroupsActive
void markAllScriptsAndGroupsActive(
    std::vector<ScriptChainNode>& scripts,
    std::int32_t firstScriptIndex,
    std::vector<ScriptChainNode>& groups,
    std::int32_t firstGroupIndex) noexcept
{
    std::int32_t idx = firstScriptIndex;
    while (idx != -1 && idx >= 0 && static_cast<std::size_t>(idx) < scripts.size()) {
        scripts[static_cast<std::size_t>(idx)].activeFlag = 1U;
        idx = scripts[static_cast<std::size_t>(idx)].nextIndex;
    }
    idx = firstGroupIndex;
    while (idx != -1 && idx >= 0 && static_cast<std::size_t>(idx) < groups.size()) {
        groups[static_cast<std::size_t>(idx)].activeFlag = 1U;
        idx = groups[static_cast<std::size_t>(idx)].nextIndex;
    }
}

// 10. 0x0081A750: Parameter::getCoord3D
void getParameterCoord3D(
    const ScriptParameterView& param,
    ScriptCoord3D& outCoord) noexcept
{
    outCoord.x = 0.0F;
    outCoord.y = 0.0F;
    outCoord.z = 0.0F;
    if (param.paramType == 0x10) {
        outCoord = param.coord;
    }
}

// 11. 0x0081AA40: IntVector::sumElements
std::uint32_t sumIntVectorElements(
    const std::int32_t* begin,
    const std::int32_t* end) noexcept
{
    std::uint32_t sum = 0U;
    for (const std::int32_t* it = begin; it != end; ++it) {
        sum += static_cast<std::uint32_t>(*it);
    }
    return sum;
}

// 12. 0x0081AA60: SightingKey::less
bool sightingKeyLess(
    const SightingKey& lhs,
    const SightingKey& rhs) noexcept
{
    if ((lhs.maskC & rhs.maskC) == 0U) {
        return lhs.maskC < rhs.maskC;
    }
    if ((lhs.mask8 & rhs.mask8) == 0U) {
        return lhs.mask8 < rhs.mask8;
    }
    if (lhs.word1 != rhs.word1) {
        return lhs.word1 < rhs.word1;
    }
    return lhs.word0 < rhs.word0;
}

// 14. 0x0081AB60: SightingTree::lowerBound
void* sightingTreeLowerBoundRaw(
    void* treeBase,
    const SightingKey& key) noexcept
{
    void* const anchor = rawBytes(treeBase) + 0x04;
    void* candidate = anchor;
    void* current = readAt<void*>(treeBase, 0x0C);
    while (current != nullptr) {
        const SightingKey nodeKey = readSightingNodeKey(current);
        if (!sightingKeyLess(nodeKey, key)) {
            candidate = current;
            current = readAt<void*>(current, 0x04);
        } else {
            current = readAt<void*>(current, 0x00);
        }
    }
    return candidate;
}

// 13. 0x0081AAA0: SightingTree::find
void* sightingTreeFindRaw(
    void* treeBase,
    const SightingKey& key) noexcept
{
    void* const anchor = rawBytes(treeBase) + 0x04;
    void* const candidate = sightingTreeLowerBoundRaw(treeBase, key);
    if (candidate == anchor) {
        return anchor;
    }
    const SightingKey nodeKey = readSightingNodeKey(candidate);
    return sightingKeyLess(key, nodeKey) ? anchor : candidate;
}

// 15. 0x0081B130: Parameter::Parameter(ParameterType, int)
ScriptParameterView initParameter(
    std::int32_t paramType,
    std::int32_t intValue) noexcept
{
    ScriptParameterView param{};
    param.paramType = paramType;
    param.initialized = 0U;
    param.intValue = intValue;
    param.realValue = 0.0F;
    param.coord = {0.0F, 0.0F, 0.0F};
    param.objStatusLo = 0U;
    param.objStatusHi = 0U;
    param.templateHandle0 = 0U;
    param.templateHandle1 = 0U;
    return param;
}

// 16. 0x0081BE00: SequentialScript::SequentialScript()
SequentialScriptView initSequentialScript() noexcept
{
    SequentialScriptView seq{};
    seq.vtable = 0x00C4D4F4U;
    seq.teamToExecOn = 0U;
    seq.objectId = 0U;
    seq.scriptName = 0U;
    seq.field10 = 0U;
    seq.scriptToExecuteSequentially = 0U;
    seq.currentInstruction = -1;
    seq.timesToLoop = 0;
    seq.framesToWait = -1;
    seq.dontAdvanceInstruction = 0U;
    seq.nextScriptInSequence = 0U;
    return seq;
}

// 17. 0x0081BF90: ScriptCounterDefaultMap::lookupOrDefault
double lookupCounterDefaultMap(
    std::int32_t defaultValue,
    bool hasTree,
    const std::vector<std::pair<std::uint32_t, std::int32_t>>& sortedMap,
    std::uint32_t key) noexcept
{
    std::int32_t result = defaultValue;
    if (key != 0U && hasTree && !sortedMap.empty()) {
        for (const auto& [entryKey, entryVal] : sortedMap) {
            if (entryKey == key) {
                result = entryVal;
                break;
            }
        }
    }
    return static_cast<double>(result);
}

// 18. 0x0081BFF0: ScriptEngine::isTrackedObjectDestroyedByName
std::uint32_t fnv1a32(std::string_view text) noexcept
{
    std::uint32_t hash = 0x811C9DC5U;
    for (unsigned char ch : text) {
        if (ch == 0U) {
            break;
        }
        hash *= 0x01000193U;
        hash ^= static_cast<std::uint32_t>(ch);
    }
    return hash;
}

bool isTrackedObjectDestroyedByName(
    const std::vector<TrackedNamedObjectEntry>& entries,
    std::string_view unitName) noexcept
{
    const std::uint32_t targetHash = fnv1a32(unitName);
    for (const TrackedNamedObjectEntry& entry : entries) {
        if (entry.nameHash == targetHash) {
            return entry.objectPtr == 0U;
        }
    }
    return false;
}

// 19. 0x0081C3E0: Script::Script()
ScriptView initScript() noexcept
{
    ScriptView script{};
    script.vtable = 0x00C4D57CU;
    script.scriptName = 0U;
    script.comment = 0U;
    script.conditionComment = 0U;
    script.flag10 = 0U;
    script.flag11 = 0U;
    script.delayEvaluationSeconds = 0;
    script.isActive = true;
    script.field1C = 0U;
    script.field20 = 0U;
    script.isOneShot = true;
    script.isSubroutine = true;
    script.flag26 = false;
    script.easy = true;
    script.normal = true;
    script.hard = true;
    script.condition = 0U;
    script.action = 0U;
    script.actionFalse = 0U;
    script.frameToEvaluateAt = 0U;
    script.isScriptActiveRuntime = true;
    script.flag3D = 0U;
    script.conditionTeamName = 0U;
    script.conditionTime = 0.0F;
    script.curTime = 0.0F;
    script.nextScript = 0U;
    return script;
}

// 20. 0x0081DE20: ScriptIndexedEntry::lowerBoundById
const ScriptIndexedEntry* lowerBoundScriptIndexedEntry(
    const ScriptIndexedEntry* first,
    const ScriptIndexedEntry* last,
    std::int32_t key) noexcept
{
    std::ptrdiff_t count = last - first;
    const ScriptIndexedEntry* base = first;
    while (count > 0) {
        const std::ptrdiff_t step = count >> 1;
        const ScriptIndexedEntry* mid = base + step;
        if (mid->id < key) {
            base = mid + 1;
            count += -1 - step;
        } else {
            count = step;
        }
    }
    return base;
}

// 21. 0x0081DE70: ScriptListIndex::lowerBoundByName
int compareAsciiStrings(std::string_view lhs, std::string_view rhs) noexcept
{
    const std::size_t minLen = std::min(lhs.size(), rhs.size());
    for (std::size_t i = 0; i < minLen; ++i) {
        const int diff =
            static_cast<int>(static_cast<unsigned char>(lhs[i])) -
            static_cast<int>(static_cast<unsigned char>(rhs[i]));
        if (diff != 0) {
            return diff;
        }
    }
    return static_cast<int>(lhs.size()) - static_cast<int>(rhs.size());
}

std::int32_t lowerBoundScriptListByName(
    const std::vector<std::int32_t>& sortedIndices,
    const std::vector<std::string>& entryNames,
    std::string_view targetName) noexcept
{
    std::int32_t low = 0;
    auto high = static_cast<std::int32_t>(sortedIndices.size());
    while (high > low) {
        const std::int32_t mid = (high + low) >> 1;
        const std::int32_t entryIdx = sortedIndices[static_cast<std::size_t>(mid)];
        const std::string_view midName =
            (entryIdx >= 0 && static_cast<std::size_t>(entryIdx) < entryNames.size())
                ? std::string_view(entryNames[static_cast<std::size_t>(entryIdx)])
                : std::string_view{};
        const int cmp = compareAsciiStrings(targetName, midName);
        if (cmp == 0) {
            return mid;
        }
        if (cmp < 0) {
            high = mid;
        } else {
            low = mid + 1;
        }
    }
    return low;
}

// 22. 0x0081DF50: DualHashTree::find
void* dualHashTreeFindRaw(
    void* treeBase,
    const DualHashKey& key) noexcept
{
    void* const anchor = rawBytes(treeBase) + 0x04;
    void* candidate = anchor;
    void* current = readAt<void*>(treeBase, 0x0C);
    while (current != nullptr) {
        const auto nodeKey0 = readAt<std::uint32_t>(current, 0x10);
        const auto nodeKey1 = readAt<std::uint32_t>(current, 0x14);
        const bool nodeLess =
            (nodeKey0 < key.prefixHash) ||
            (nodeKey0 == key.prefixHash && nodeKey1 < key.nameHash);
        if (!nodeLess) {
            candidate = current;
            current = readAt<void*>(current, 0x04);
        } else {
            current = readAt<void*>(current, 0x00);
        }
    }
    if (candidate == anchor) {
        return anchor;
    }
    const auto candKey0 = readAt<std::uint32_t>(candidate, 0x10);
    const auto candKey1 = readAt<std::uint32_t>(candidate, 0x14);
    const bool keyLess =
        (key.prefixHash < candKey0) ||
        (key.prefixHash == candKey0 && key.nameHash < candKey1);
    return keyLess ? anchor : candidate;
}

// 23. 0x0081DFC0: ScriptWaypointInfo::copyFrom
void copyScriptWaypointInfo(
    ScriptWaypointInfo& dst,
    const ScriptWaypointInfo& src) noexcept
{
    dst.word00 = src.word00;
    dst.word04 = src.word04;
    dst.word08 = src.word08;
    dst.byte0C = src.byte0C;
    dst.word10 = src.word10;
    dst.float14 = src.float14;
    dst.float18 = src.float18;
    dst.float1C = src.float1C;
    dst.word20 = src.word20;
}

// 24. 0x0081F710: ScriptConditions::evaluateSkirmishCommandButtonIsReady
std::uint32_t crc32Ra3String(std::string_view text) noexcept
{
    if (text.empty() || text[0] == '\0') {
        return 0U;
    }
    std::uint32_t crc = 0xFFFFFFFFU;
    for (unsigned char ch : text) {
        if (ch == 0U) {
            break;
        }
        std::uint32_t entry = (crc ^ ch) & 0xFFU;
        for (int bit = 0; bit < 8; ++bit) {
            entry = (entry & 1U) != 0U ? ((entry >> 1U) ^ 0xEDB88320U) : (entry >> 1U);
        }
        crc = (crc >> 8U) ^ entry;
    }
    return ~crc;
}

bool evaluateSkirmishCommandButtonIsReady(
    std::int32_t& conditionCustomData,
    std::string_view buttonName,
    bool gameLogicFlagA8,
    const std::vector<std::pair<std::uint32_t, std::uint32_t>>& readyPairs,
    bool& outRegisteredFirstTime) noexcept
{
    constexpr std::uint32_t kCommandButtonClassId = 0x01D2592FU;
    outRegisteredFirstTime = false;
    if (conditionCustomData == 0) {
        conditionCustomData = 1;
        outRegisteredFirstTime = !gameLogicFlagA8;
    }
    const std::uint32_t buttonCrc = crc32Ra3String(buttonName);
    for (const auto& [classId, itemCrc] : readyPairs) {
        if (classId == kCommandButtonClassId && itemCrc == buttonCrc) {
            return true;
        }
    }
    return false;
}

// 25. 0x0081F7B0: ScriptConditions::evaluateBridgeBroken
bool evaluateBridgeBroken(
    std::string_view bridgeWaypointName,
    const std::vector<WaypointStub>& waypoints,
    const std::vector<std::pair<std::int32_t, std::int32_t>>& bridgeDamageStates) noexcept
{
    if (bridgeWaypointName.empty()) {
        return false;
    }
    const WaypointStub* matchedWaypoint = nullptr;
    for (const WaypointStub& wp : waypoints) {
        if (compareAsciiStrings(wp.name, bridgeWaypointName) == 0) {
            matchedWaypoint = &wp;
            break;
        }
    }
    if (matchedWaypoint == nullptr) {
        return false;
    }
    for (const auto& [waypointId, damageState] : bridgeDamageStates) {
        if (waypointId == matchedWaypoint->id) {
            return damageState == 2;
        }
    }
    return false;
}

// 26. 0x0081F8A0: ScriptConditions::evaluatePlatform
bool evaluatePlatform(std::string_view platformName) noexcept
{
    if (compareAsciiStrings(platformName, "PC") == 0) {
        return true;
    }
    return false;
}

// 27 & 28. 0x0081FA00 & 0x0081FAE0: findFlagByName & qualifyWithCurrentSidePrefix
void splitSidePrefix(
    std::string_view currentSidePrefix,
    std::string& inOutName,
    std::string& outPrefix)
{
    const std::size_t slashPos = inOutName.find('/');
    if (slashPos == std::string::npos) {
        outPrefix.assign(currentSidePrefix);
    } else {
        outPrefix = inOutName.substr(0U, slashPos);
        inOutName = inOutName.substr(slashPos + 1U);
    }
}

std::string qualifyWithCurrentSidePrefix(
    std::string_view currentSidePrefix,
    std::string_view rawName)
{
    std::string name(rawName);
    std::string prefix;
    splitSidePrefix(currentSidePrefix, name, prefix);
    prefix.push_back('/');
    prefix.append(name);
    return prefix;
}

DualHashKey computeFlagLookupKey(
    std::string_view currentSidePrefix,
    std::string_view flagName)
{
    std::string localName(flagName);
    std::string prefix;
    splitSidePrefix(currentSidePrefix, localName, prefix);
    return DualHashKey{
        fnv1a32(prefix),
        fnv1a32(localName),
    };
}

// 29. 0x0081FB40: ScriptEngine::findAttackInfo
std::int32_t findAttackInfoIndex(
    std::vector<ScriptAttackInfoEntry>& entries,
    std::int32_t& currentCount,
    std::string_view attackName,
    bool addIfNotFound)
{
    for (std::int32_t idx = 1; idx < currentCount; ++idx) {
        if (static_cast<std::size_t>(idx) < entries.size() &&
            compareAsciiStrings(entries[static_cast<std::size_t>(idx)].scriptName, attackName) == 0) {
            return idx;
        }
    }
    if (addIfNotFound && currentCount < 0x100) {
        const std::int32_t newIdx = currentCount;
        if (static_cast<std::size_t>(newIdx) >= entries.size()) {
            entries.resize(static_cast<std::size_t>(newIdx + 1));
        }
        entries[static_cast<std::size_t>(newIdx)].scriptName.assign(attackName);
        ++currentCount;
        return newIdx;
    }
    return 0;
}

// 30. 0x0081FC40: ScriptEngine::getObjectCount
std::int32_t getScriptObjectCount(
    bool playerValid,
    std::string_view objectTypeName,
    const std::vector<std::pair<std::uint32_t, std::int32_t>>& playerObjectCounts) noexcept
{
    if (!playerValid) {
        return 0;
    }
    const std::uint32_t crc = crc32Ra3String(objectTypeName);
    for (const auto& [keyCrc, count] : playerObjectCounts) {
        if (keyCrc == crc) {
            return count;
        }
    }
    return 0;
}

// 31. 0x0081FD60: ScriptEngine::hasSightingEntry
bool hasSightingEntryRaw(
    void* sightingTreeBase,
    const SightingKey& key) noexcept
{
    void* const anchor = rawBytes(sightingTreeBase) + 0x04;
    return sightingTreeFindRaw(sightingTreeBase, key) != anchor;
}

// 32. 0x008201B0: ScriptEngine::isSpecialPowerTriggeredOrComplete
bool isSpecialPowerTriggeredOrComplete(
    std::uint32_t playerIndex,
    std::uint32_t powerFastHash,
    bool eraseIfFound,
    std::uint32_t requiredSourceObjId,
    bool hasTriggerFilter,
    std::vector<SpecialPowerEventNode>& events,
    bool& outErasedNextLink) noexcept
{
    outErasedNextLink = false;
    if (playerIndex > 0x13U) {
        return false;
    }
    for (std::size_t i = 0; i < events.size(); ++i) {
        const SpecialPowerEventNode& ev = events[i];
        if (ev.powerHash != powerFastHash) {
            continue;
        }
        if (requiredSourceObjId != 0U && ev.sourceObjectId != requiredSourceObjId) {
            continue;
        }
        if (hasTriggerFilter) {
            if (!ev.hasTargetCoord || !ev.pointInsideTrigger) {
                continue;
            }
        }
        if (eraseIfFound) {
            outErasedNextLink = true;
            events.erase(events.begin() + static_cast<std::ptrdiff_t>(i));
        }
        return true;
    }
    return false;
}

// 33. 0x00820280: ScriptEngine::isScienceAcquiredOrUpgradeComplete
bool isScienceAcquiredOrUpgradeComplete(
    std::uint32_t playerIndex,
    std::uint32_t itemFastHash,
    bool eraseIfFound,
    std::vector<std::uint32_t>& eventHashes,
    bool& outErasedNextLink) noexcept
{
    outErasedNextLink = false;
    if (playerIndex > 0x13U) {
        return false;
    }
    for (std::size_t i = 0; i < eventHashes.size(); ++i) {
        if (eventHashes[i] == itemFastHash) {
            if (eraseIfFound) {
                outErasedNextLink = true;
                eventHashes.erase(eventHashes.begin() + static_cast<std::ptrdiff_t>(i));
            }
            return true;
        }
    }
    return false;
}

// 34. 0x00820630: Parameter::qualifyPlayerSide
void qualifyPlayerSide(ScriptParameterView& param)
{
    if (param.paramType != 0x0B) {
        return;
    }
    static constexpr std::array<std::pair<std::string_view, std::string_view>, 5> kSideReplacements = {{
        {"<Local Player>", "<1st Human Player>"},
        {"<Local Player's Enemy>", "<1st Human Player's Enemies>"},
        {"<Local Player's Enemies>", "<1st Human Player's Enemies>"},
        {"<Local Player's Allies>", "<1st Human Player's Allies>"},
        {"<Local Player's Allies incl Self>", "<1st Human Player's Allies incl Self>"},
    }};
    for (const auto& [fromSide, toSide] : kSideReplacements) {
        if (compareAsciiStrings(param.stringValue, fromSide) == 0) {
            param.stringValue.assign(toSide);
            return;
        }
    }
}

// 35. 0x00820B70: ScriptAction::setActionType
void setScriptActionType(
    ScriptActionView& action,
    std::int32_t actionType,
    const std::vector<ScriptActionTemplateView>& templates)
{
    for (std::int32_t i = 0; i < action.numParams && i < 12; ++i) {
        action.params[static_cast<std::size_t>(i)] = ScriptParameterView{};
        action.paramPresent[static_cast<std::size_t>(i)] = false;
    }
    action.actionType = actionType;
    const std::int32_t templateIdx =
        (actionType >= 0 && actionType < 0x23D) ? actionType : 0;
    const ScriptActionTemplateView& tmpl =
        (static_cast<std::size_t>(templateIdx) < templates.size())
            ? templates[static_cast<std::size_t>(templateIdx)]
            : ScriptActionTemplateView{};
    action.numParams = tmpl.numParams;
    for (std::int32_t i = 0; i < action.numParams && i < 12; ++i) {
        const std::int32_t ptype =
            (i >= 0 && i < tmpl.numParams) ? tmpl.paramTypes[static_cast<std::size_t>(i)] : 0;
        action.params[static_cast<std::size_t>(i)] = initParameter(ptype, 0);
        action.paramPresent[static_cast<std::size_t>(i)] = true;
    }
}

// 36. 0x00820E00: ScriptAction::getTeamInvolved
std::int32_t getTeamInvolvedParamIndex(std::int32_t actionType) noexcept
{
    if (!isTeamAction(actionType)) {
        return -1;
    }
    if (actionType == 210 || actionType == 352 || actionType == 385) {
        return 1;
    }
    return 0;
}

std::string getTeamInvolvedName(
    const ScriptActionView& action,
    bool& outInvolved)
{
    const std::int32_t paramIdx = getTeamInvolvedParamIndex(action.actionType);
    if (paramIdx < 0) {
        outInvolved = false;
        return {};
    }
    outInvolved = true;
    if (action.numParams > paramIdx && action.paramPresent[static_cast<std::size_t>(paramIdx)]) {
        return action.params[static_cast<std::size_t>(paramIdx)].stringValue;
    }
    return {};
}

const std::array<Scripts82Entry, 64>& scripts82Inventory() noexcept
{
    return kScripts82Manifest;
}

}  // namespace ra3::gamelogic
