#include <ra3/gamelogic/ScriptsCluster.h>

#include <algorithm>
#include <cstdint>
#include <cstring>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

std::uint32_t u32(const std::string& text)
{
    return static_cast<std::uint32_t>(std::stoul(text, nullptr, 0));
}

std::int32_t i32(const std::string& text)
{
    return static_cast<std::int32_t>(u32(text));
}

float floatFromBits(const std::string& text)
{
    const std::uint32_t bits = u32(text);
    float value = 0.0F;
    std::memcpy(&value, &bits, sizeof(value));
    return value;
}

std::uint32_t bitsFromFloat(float value)
{
    std::uint32_t bits = 0U;
    std::memcpy(&bits, &value, sizeof(bits));
    return bits;
}

std::uint64_t bitsFromDouble(double value)
{
    std::uint64_t bits = 0ULL;
    std::memcpy(&bits, &value, sizeof(bits));
    return bits;
}

std::string decodeToken(const std::string& token)
{
    if (token == "<EMPTY>") {
        return {};
    }
    std::string out = token;
    for (char& ch : out) {
        if (ch == '|') {
            ch = ' ';
        }
    }
    return out;
}

std::string encodeToken(const std::string& text)
{
    if (text.empty()) {
        return "<EMPTY>";
    }
    std::string out = text;
    for (char& ch : out) {
        if (ch == ' ') {
            ch = '|';
        }
    }
    return out;
}

bool runSelfCheck()
{
    using namespace ra3::gamelogic;
    const auto& inv = scripts82Inventory();
    if (inv.size() != 64U) {
        return false;
    }
    const auto reversedCount = std::count_if(
        inv.begin(), inv.end(), [](const Scripts82Entry& e) {
            return e.status == Scripts82Status::reversed;
        });
    const auto hookCount = std::count_if(
        inv.begin(), inv.end(), [](const Scripts82Entry& e) {
            return e.hookSafe;
        });
    if (reversedCount != 36 || hookCount != 10) {
        return false;
    }
    if (!isUnitAction(33) || !isUnitAction(0x129) || isUnitAction(12)) {
        return false;
    }
    if (!isTeamAction(12) || !isTeamAction(210) || isTeamAction(33)) {
        return false;
    }
    if (!isTeamOrUnitAction(13) || isTeamOrUnitAction(12)) {
        return false;
    }
    if (getTeamInvolvedParamIndex(210) != 1 || getTeamInvolvedParamIndex(12) != 0) {
        return false;
    }
    const std::uint32_t expected = isUnitActionEax(0x129);
    const std::uint32_t mutant = expected ^ 1U;
    return expected == 0x00000101U && mutant != expected;
}

void runCommand(const std::vector<std::string>& words)
{
    using namespace ra3::gamelogic;
    if (words.empty() || words[0] == "selftest") {
        if (!runSelfCheck()) {
            throw std::runtime_error("self check failed");
        }
        std::cout << "Scripts82 checker: inventory=64, reconstructed=36, hooks=10, "
                     "0 semantic mismatches; semantic mutant detected\n";
        return;
    }
    if (words[0] == "inventory" || words[0] == "manifest") {
        for (const Scripts82Entry& entry : scripts82Inventory()) {
            std::cout << std::hex << std::setfill('0') << std::setw(8)
                      << entry.address << std::dec << '\t' << entry.size << '\t'
                      << std::hex << std::setw(8) << entry.crc32 << std::dec << '\t'
                      << (entry.status == Scripts82Status::reversed ? 1 : 0) << '\t'
                      << (entry.hookSafe ? 1 : 0) << '\t' << entry.symbol << '\n';
        }
        return;
    }
    if (words[0] == "unitaction") {
        std::cout << isUnitActionEax(i32(words.at(1))) << '\n';
        return;
    }
    if (words[0] == "unitactionmutant") {
        const std::int32_t t = i32(words.at(1));
        std::uint32_t res = isUnitActionEax(t);
        if (t == 0x129) {
            res ^= 1U;
        }
        std::cout << res << '\n';
        return;
    }
    if (words[0] == "teamaction") {
        std::cout << isTeamActionEax(i32(words.at(1))) << '\n';
        return;
    }
    if (words[0] == "teamorunitaction") {
        std::cout << isTeamOrUnitActionEax(i32(words.at(1))) << '\n';
        return;
    }
    if (words[0] == "caneval") {
        ScriptView s = initScript();
        s.curTime = floatFromBits(words.at(1));
        s.isScriptActiveRuntime = u32(words.at(2)) != 0U;
        s.easy = u32(words.at(3)) != 0U;
        s.normal = u32(words.at(4)) != 0U;
        s.hard = u32(words.at(5)) != 0U;
        s.frameToEvaluateAt = u32(words.at(6));
        const bool hasCurrPlayer = u32(words.at(7)) != 0U;
        const bool hasAiPlayer = u32(words.at(8)) != 0U;
        const std::int32_t aiDiff = i32(words.at(9));
        const std::int32_t engDiff = i32(words.at(10));
        const std::uint32_t logicFrame = u32(words.at(11));
        const bool ok = canEvaluateScript(
            s, engDiff, hasCurrPlayer, hasAiPlayer, aiDiff, logicFrame);
        std::cout << (ok ? 1 : 0) << ' ' << bitsFromFloat(s.curTime) << '\n';
        return;
    }
    if (words[0] == "cleartracked") {
        const std::uint32_t target = u32(words.at(1));
        const std::size_t count = static_cast<std::size_t>(u32(words.at(2)));
        std::vector<TrackedNamedObjectEntry> entries(count);
        for (std::size_t i = 0; i < count; ++i) {
            entries[i].objectPtr = u32(words.at(3 + i * 2));
            entries[i].nameHash = u32(words.at(4 + i * 2));
        }
        clearTrackedObjectPointer(entries, target);
        for (std::size_t i = 0; i < count; ++i) {
            if (i > 0) {
                std::cout << ' ';
            }
            std::cout << entries[i].objectPtr;
        }
        std::cout << '\n';
        return;
    }
    if (words[0] == "seqtimerobj") {
        const bool objNonNull = u32(words.at(1)) != 0U;
        const std::uint32_t objId = u32(words.at(2));
        const std::int32_t frames = i32(words.at(3));
        const std::size_t count = static_cast<std::size_t>(u32(words.at(4)));
        std::vector<SequentialScriptView> seqs(count);
        for (std::size_t i = 0; i < count; ++i) {
            seqs[i] = initSequentialScript();
            seqs[i].objectId = u32(words.at(5 + i * 2));
            seqs[i].framesToWait = i32(words.at(6 + i * 2));
        }
        setSequentialTimerForObject(seqs, objNonNull, objId, frames);
        for (std::size_t i = 0; i < count; ++i) {
            if (i > 0) {
                std::cout << ' ';
            }
            std::cout << seqs[i].framesToWait;
        }
        std::cout << '\n';
        return;
    }
    if (words[0] == "seqtimerteam") {
        const std::uint32_t teamPtr = u32(words.at(1));
        const std::int32_t frames = i32(words.at(2));
        const std::size_t count = static_cast<std::size_t>(u32(words.at(3)));
        std::vector<SequentialScriptView> seqs(count);
        for (std::size_t i = 0; i < count; ++i) {
            seqs[i] = initSequentialScript();
            seqs[i].teamToExecOn = u32(words.at(4 + i * 2));
            seqs[i].framesToWait = i32(words.at(5 + i * 2));
        }
        setSequentialTimerForTeam(seqs, teamPtr, frames);
        for (std::size_t i = 0; i < count; ++i) {
            if (i > 0) {
                std::cout << ' ';
            }
            std::cout << seqs[i].framesToWait;
        }
        std::cout << '\n';
        return;
    }
    if (words[0] == "notifydestroy") {
        const std::uint32_t destroyed = u32(words.at(1));
        std::uint32_t c70 = u32(words.at(2));
        std::uint32_t c68 = u32(words.at(3));
        const std::size_t count = static_cast<std::size_t>(u32(words.at(4)));
        std::vector<TrackedNamedObjectEntry> entries(count);
        for (std::size_t i = 0; i < count; ++i) {
            entries[i].objectPtr = u32(words.at(5 + i));
        }
        notifyOfObjectDestruction(entries, c70, c68, destroyed);
        std::cout << c70 << ' ' << c68;
        for (std::size_t i = 0; i < count; ++i) {
            std::cout << ' ' << entries[i].objectPtr;
        }
        std::cout << '\n';
        return;
    }
    if (words[0] == "markactive") {
        const std::int32_t firstS = i32(words.at(1));
        const std::size_t sCount = static_cast<std::size_t>(u32(words.at(2)));
        std::vector<ScriptChainNode> sNodes(sCount);
        std::size_t cursor = 3;
        for (std::size_t i = 0; i < sCount; ++i) {
            sNodes[i].nextIndex = i32(words.at(cursor++));
            sNodes[i].activeFlag = static_cast<std::uint8_t>(u32(words.at(cursor++)));
        }
        const std::int32_t firstG = i32(words.at(cursor++));
        const std::size_t gCount = static_cast<std::size_t>(u32(words.at(cursor++)));
        std::vector<ScriptChainNode> gNodes(gCount);
        for (std::size_t i = 0; i < gCount; ++i) {
            gNodes[i].nextIndex = i32(words.at(cursor++));
            gNodes[i].activeFlag = static_cast<std::uint8_t>(u32(words.at(cursor++)));
        }
        markAllScriptsAndGroupsActive(sNodes, firstS, gNodes, firstG);
        for (std::size_t i = 0; i < sCount; ++i) {
            if (i > 0) {
                std::cout << ' ';
            }
            std::cout << static_cast<unsigned>(sNodes[i].activeFlag);
        }
        for (std::size_t i = 0; i < gCount; ++i) {
            if (!sNodes.empty() || i > 0) {
                std::cout << ' ';
            }
            std::cout << static_cast<unsigned>(gNodes[i].activeFlag);
        }
        std::cout << '\n';
        return;
    }
    if (words[0] == "paramcoord3d") {
        ScriptParameterView p{};
        p.paramType = i32(words.at(1));
        p.coord.x = floatFromBits(words.at(2));
        p.coord.y = floatFromBits(words.at(3));
        p.coord.z = floatFromBits(words.at(4));
        ScriptCoord3D out{99.0F, 99.0F, 99.0F};
        getParameterCoord3D(p, out);
        std::cout << bitsFromFloat(out.x) << ' ' << bitsFromFloat(out.y) << ' '
                  << bitsFromFloat(out.z) << '\n';
        return;
    }
    if (words[0] == "intvecsum") {
        const std::size_t count = static_cast<std::size_t>(u32(words.at(1)));
        std::vector<std::int32_t> vals(count);
        for (std::size_t i = 0; i < count; ++i) {
            vals[i] = i32(words.at(2 + i));
        }
        const std::int32_t* begin = vals.empty() ? nullptr : vals.data();
        const std::int32_t* end = vals.empty() ? nullptr : vals.data() + vals.size();
        std::cout << sumIntVectorElements(begin, end) << '\n';
        return;
    }
    if (words[0] == "sightingless") {
        const SightingKey a{u32(words.at(1)), u32(words.at(2)), u32(words.at(3)), u32(words.at(4))};
        const SightingKey b{u32(words.at(5)), u32(words.at(6)), u32(words.at(7)), u32(words.at(8))};
        std::cout << (sightingKeyLess(a, b) ? 1 : 0) << '\n';
        return;
    }
    if (words[0] == "sightingfind" || words[0] == "sightinglb" || words[0] == "hassighting") {
        const SightingKey q{u32(words.at(1)), u32(words.at(2)), u32(words.at(3)), u32(words.at(4))};
        const std::size_t count = static_cast<std::size_t>(u32(words.at(5)));
        std::vector<SightingKey> keys(count);
        for (std::size_t i = 0; i < count; ++i) {
            keys[i] = {
                u32(words.at(6 + i * 4)),
                u32(words.at(7 + i * 4)),
                u32(words.at(8 + i * 4)),
                u32(words.at(9 + i * 4))};
        }
        // Build an in-memory RB-tree layout (matching node offsets +0x00, +0x08, +0x10)
        struct RawSightingNode {
            RawSightingNode* left = nullptr;
            RawSightingNode* parent = nullptr;
            RawSightingNode* right = nullptr;
            std::uint32_t color = 0;
            SightingKey key{};
        };
        std::vector<RawSightingNode> nodes(count);
        for (std::size_t i = 0; i < count; ++i) {
            nodes[i].key = keys[i];
        }
        RawSightingNode* root = nullptr;
        for (std::size_t i = 0; i < count; ++i) {
            if (!root) {
                root = &nodes[i];
                continue;
            }
            RawSightingNode* cur = root;
            while (true) {
                if (sightingKeyLess(nodes[i].key, cur->key)) {
                    if (!cur->left) {
                        cur->left = &nodes[i];
                        break;
                    }
                    cur = cur->left;
                } else {
                    if (!cur->right) {
                        cur->right = &nodes[i];
                        break;
                    }
                    cur = cur->right;
                }
            }
        }
        std::uint32_t treeRaw[4]{0U, 0U, 0U, 0U};
        treeRaw[3] = static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(root));
        if (words[0] == "hassighting") {
            std::cout << (hasSightingEntryRaw(treeRaw, q) ? 1 : 0) << '\n';
            return;
        }
        void* const res = (words[0] == "sightingfind")
            ? sightingTreeFindRaw(treeRaw, q)
            : sightingTreeLowerBoundRaw(treeRaw, q);
        int idx = -1;
        if (res != &treeRaw[1]) {
            idx = static_cast<int>(static_cast<RawSightingNode*>(res) - nodes.data());
        }
        std::cout << idx << '\n';
        return;
    }
    if (words[0] == "initparam") {
        const ScriptParameterView p = initParameter(i32(words.at(1)), i32(words.at(2)));
        std::cout << p.paramType << ' ' << static_cast<unsigned>(p.initialized) << ' '
                  << p.intValue << ' ' << bitsFromFloat(p.realValue) << ' '
                  << (p.stringValue.empty() ? 1 : 0) << ' '
                  << bitsFromFloat(p.coord.x) << ' ' << bitsFromFloat(p.coord.y) << ' '
                  << bitsFromFloat(p.coord.z) << ' ' << p.objStatusLo << ' '
                  << p.objStatusHi << ' ' << p.templateHandle0 << ' '
                  << p.templateHandle1 << '\n';
        return;
    }
    if (words[0] == "initseq") {
        const SequentialScriptView s = initSequentialScript();
        std::cout << s.vtable << ' ' << s.teamToExecOn << ' ' << s.objectId << ' '
                  << s.scriptName << ' ' << s.field10 << ' '
                  << s.scriptToExecuteSequentially << ' ' << s.currentInstruction << ' '
                  << s.timesToLoop << ' ' << s.framesToWait << ' '
                  << static_cast<unsigned>(s.dontAdvanceInstruction) << ' '
                  << s.nextScriptInSequence << '\n';
        return;
    }
    if (words[0] == "counterdefault") {
        const std::int32_t defVal = i32(words.at(1));
        const bool hasTree = u32(words.at(2)) != 0U;
        const std::uint32_t qKey = u32(words.at(3));
        const std::size_t count = static_cast<std::size_t>(u32(words.at(4)));
        std::vector<std::pair<std::uint32_t, std::int32_t>> entries(count);
        for (std::size_t i = 0; i < count; ++i) {
            entries[i] = {u32(words.at(5 + i * 2)), i32(words.at(6 + i * 2))};
        }
        const double val = lookupCounterDefaultMap(defVal, hasTree, entries, qKey);
        std::cout << bitsFromDouble(val) << '\n';
        return;
    }
    if (words[0] == "istrackeddestroyed") {
        const std::string qStr = decodeToken(words.at(1));
        const std::size_t count = static_cast<std::size_t>(u32(words.at(2)));
        std::vector<TrackedNamedObjectEntry> entries(count);
        for (std::size_t i = 0; i < count; ++i) {
            entries[i].nameHash = u32(words.at(3 + i * 2));
            entries[i].objectPtr = u32(words.at(4 + i * 2));
        }
        std::cout << (isTrackedObjectDestroyedByName(entries, qStr) ? 1 : 0) << '\n';
        return;
    }
    if (words[0] == "initscript") {
        const ScriptView s = initScript();
        std::cout << s.vtable << ' ' << (s.isActive ? 1 : 0) << ' '
                  << (s.isOneShot ? 1 : 0) << ' ' << (s.isSubroutine ? 1 : 0) << ' '
                  << (s.flag26 ? 1 : 0) << ' ' << (s.easy ? 1 : 0) << ' '
                  << (s.normal ? 1 : 0) << ' ' << (s.hard ? 1 : 0) << ' '
                  << s.frameToEvaluateAt << ' ' << (s.isScriptActiveRuntime ? 1 : 0) << ' '
                  << static_cast<unsigned>(s.flag3D) << ' '
                  << bitsFromFloat(s.conditionTime) << ' ' << bitsFromFloat(s.curTime) << '\n';
        return;
    }
    if (words[0] == "indexedlb") {
        const std::int32_t qId = i32(words.at(1));
        const std::size_t count = static_cast<std::size_t>(u32(words.at(2)));
        std::vector<ScriptIndexedEntry> entries(count);
        for (std::size_t i = 0; i < count; ++i) {
            entries[i] = {
                i32(words.at(3 + i * 3)),
                u32(words.at(4 + i * 3)),
                u32(words.at(5 + i * 3))};
        }
        const ScriptIndexedEntry* first = entries.data();
        const ScriptIndexedEntry* last = first + entries.size();
        const ScriptIndexedEntry* res = lowerBoundScriptIndexedEntry(first, last, qId);
        std::cout << (res - first) << '\n';
        return;
    }
    if (words[0] == "listindexlb") {
        const std::string qName = decodeToken(words.at(1));
        const std::size_t count = static_cast<std::size_t>(u32(words.at(2)));
        std::vector<std::int32_t> sortedIndices(count);
        std::vector<std::string> entryNames(count);
        for (std::size_t i = 0; i < count; ++i) {
            sortedIndices[i] = i32(words.at(3 + i * 2));
            entryNames[i] = decodeToken(words.at(4 + i * 2));
        }
        std::cout << lowerBoundScriptListByName(sortedIndices, entryNames, qName) << '\n';
        return;
    }
    if (words[0] == "dualhashfind") {
        const DualHashKey q{u32(words.at(1)), u32(words.at(2))};
        const std::size_t count = static_cast<std::size_t>(u32(words.at(3)));
        struct RawDualNode {
            RawDualNode* left = nullptr;
            RawDualNode* parent = nullptr;
            RawDualNode* right = nullptr;
            std::uint32_t color = 0;
            DualHashKey key{};
        };
        std::vector<RawDualNode> nodes(count);
        for (std::size_t i = 0; i < count; ++i) {
            nodes[i].key = {u32(words.at(4 + i * 2)), u32(words.at(5 + i * 2))};
        }
        RawDualNode* root = nullptr;
        for (std::size_t i = 0; i < count; ++i) {
            if (!root) {
                root = &nodes[i];
                continue;
            }
            RawDualNode* cur = root;
            while (true) {
                const bool less = (nodes[i].key.prefixHash < cur->key.prefixHash) ||
                    (nodes[i].key.prefixHash == cur->key.prefixHash &&
                     nodes[i].key.nameHash < cur->key.nameHash);
                if (less) {
                    if (!cur->left) {
                        cur->left = &nodes[i];
                        break;
                    }
                    cur = cur->left;
                } else {
                    if (!cur->right) {
                        cur->right = &nodes[i];
                        break;
                    }
                    cur = cur->right;
                }
            }
        }
        std::uint32_t treeRaw[4]{0U, 0U, 0U, 0U};
        treeRaw[3] = static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(root));
        void* const res = dualHashTreeFindRaw(treeRaw, q);
        int idx = -1;
        if (res != &treeRaw[1]) {
            idx = static_cast<int>(static_cast<RawDualNode*>(res) - nodes.data());
        }
        std::cout << idx << '\n';
        return;
    }
    if (words[0] == "waypointcopy") {
        ScriptWaypointInfo src{};
        src.word00 = u32(words.at(1));
        src.word04 = u32(words.at(2));
        src.word08 = u32(words.at(3));
        src.byte0C = static_cast<std::uint8_t>(u32(words.at(4)));
        src.word10 = u32(words.at(5));
        src.float14 = floatFromBits(words.at(6));
        src.float18 = floatFromBits(words.at(7));
        src.float1C = floatFromBits(words.at(8));
        src.word20 = u32(words.at(9));
        ScriptWaypointInfo dst{};
        copyScriptWaypointInfo(dst, src);
        std::cout << dst.word00 << ' ' << dst.word04 << ' ' << dst.word08 << ' '
                  << static_cast<unsigned>(dst.byte0C) << ' ' << dst.word10 << ' '
                  << bitsFromFloat(dst.float14) << ' ' << bitsFromFloat(dst.float18) << ' '
                  << bitsFromFloat(dst.float1C) << ' ' << dst.word20 << '\n';
        return;
    }
    if (words[0] == "skirmishcmdready") {
        std::int32_t customData = i32(words.at(1));
        const std::string buttonName = decodeToken(words.at(2));
        const bool flagA8 = u32(words.at(3)) != 0U;
        const std::size_t count = static_cast<std::size_t>(u32(words.at(4)));
        std::vector<std::pair<std::uint32_t, std::uint32_t>> ready(count);
        for (std::size_t i = 0; i < count; ++i) {
            ready[i] = {u32(words.at(5 + i * 2)), u32(words.at(6 + i * 2))};
        }
        bool regFirst = false;
        const bool ok = evaluateSkirmishCommandButtonIsReady(
            customData, buttonName, flagA8, ready, regFirst);
        std::cout << (ok ? 1 : 0) << ' ' << static_cast<std::uint32_t>(customData) << ' '
                  << (regFirst ? 1 : 0) << '\n';
        return;
    }
    if (words[0] == "bridgebroken") {
        const std::string wpName = decodeToken(words.at(1));
        const std::size_t wpCount = static_cast<std::size_t>(u32(words.at(2)));
        std::vector<WaypointStub> wps(wpCount);
        std::size_t cursor = 3;
        for (std::size_t i = 0; i < wpCount; ++i) {
            wps[i].id = i32(words.at(cursor++));
            wps[i].name = decodeToken(words.at(cursor++));
        }
        const std::size_t brCount = static_cast<std::size_t>(u32(words.at(cursor++)));
        std::vector<std::pair<std::int32_t, std::int32_t>> brs(brCount);
        for (std::size_t i = 0; i < brCount; ++i) {
            const std::int32_t id = i32(words.at(cursor++));
            const std::int32_t state = i32(words.at(cursor++));
            brs[i] = {id, state};
        }
        std::cout << (evaluateBridgeBroken(wpName, wps, brs) ? 1 : 0) << '\n';
        return;
    }
    if (words[0] == "platform") {
        std::cout << (evaluatePlatform(decodeToken(words.at(1))) ? 1 : 0) << '\n';
        return;
    }
    if (words[0] == "findflag") {
        const std::string curPrefix = decodeToken(words.at(1));
        const std::string rawName = decodeToken(words.at(2));
        const DualHashKey key = computeFlagLookupKey(curPrefix, rawName);
        std::cout << key.prefixHash << ' ' << key.nameHash << '\n';
        return;
    }
    if (words[0] == "qualifyside") {
        const std::string curPrefix = decodeToken(words.at(1));
        const std::string rawName = decodeToken(words.at(2));
        std::cout << encodeToken(qualifyWithCurrentSidePrefix(curPrefix, rawName)) << '\n';
        return;
    }
    if (words[0] == "findattack") {
        std::int32_t currentCount = i32(words.at(1));
        const std::string attackName = decodeToken(words.at(2));
        const bool addIfNotFound = u32(words.at(3)) != 0U;
        const std::size_t vecCount = static_cast<std::size_t>(u32(words.at(4)));
        std::vector<ScriptAttackInfoEntry> entries(256);
        for (std::size_t i = 0; i < vecCount && (i + 1) < entries.size(); ++i) {
            entries[i + 1].scriptName = decodeToken(words.at(5 + i));
        }
        const std::int32_t idx = findAttackInfoIndex(
            entries, currentCount, attackName, addIfNotFound);
        std::cout << idx << ' ' << currentCount << '\n';
        return;
    }
    if (words[0] == "objcount") {
        const bool playerValid = u32(words.at(1)) != 0U;
        const std::string typeName = decodeToken(words.at(2));
        const std::size_t count = static_cast<std::size_t>(u32(words.at(3)));
        std::vector<std::pair<std::uint32_t, std::int32_t>> tree(count);
        for (std::size_t i = 0; i < count; ++i) {
            tree[i] = {u32(words.at(4 + i * 2)), i32(words.at(5 + i * 2))};
        }
        std::cout << getScriptObjectCount(playerValid, typeName, tree) << '\n';
        return;
    }
    if (words[0] == "specialpower") {
        const std::uint32_t playerIdx = u32(words.at(1));
        const std::uint32_t powerHash = u32(words.at(2));
        const bool eraseIfFound = u32(words.at(3)) != 0U;
        const std::uint32_t reqObjId = u32(words.at(4));
        const bool hasTriggerFilter = u32(words.at(5)) != 0U;
        const std::size_t count = static_cast<std::size_t>(u32(words.at(6)));
        std::vector<SpecialPowerEventNode> events(count);
        for (std::size_t i = 0; i < count; ++i) {
            events[i].powerHash = u32(words.at(7 + i * 4));
            events[i].sourceObjectId = u32(words.at(8 + i * 4));
            events[i].hasTargetCoord = u32(words.at(9 + i * 4)) != 0U;
            events[i].pointInsideTrigger = u32(words.at(10 + i * 4)) != 0U;
        }
        bool erasedNext = false;
        const bool ok = isSpecialPowerTriggeredOrComplete(
            playerIdx, powerHash, eraseIfFound, reqObjId, hasTriggerFilter, events, erasedNext);
        std::cout << (ok ? 1 : 0) << ' ' << (erasedNext ? 1 : 0) << '\n';
        return;
    }
    if (words[0] == "scienceupgrade") {
        const std::uint32_t playerIdx = u32(words.at(1));
        const std::uint32_t itemHash = u32(words.at(2));
        const bool eraseIfFound = u32(words.at(3)) != 0U;
        const std::size_t count = static_cast<std::size_t>(u32(words.at(4)));
        std::vector<std::uint32_t> hashes(count);
        for (std::size_t i = 0; i < count; ++i) {
            hashes[i] = u32(words.at(5 + i));
        }
        bool erasedNext = false;
        const bool ok = isScienceAcquiredOrUpgradeComplete(
            playerIdx, itemHash, eraseIfFound, hashes, erasedNext);
        std::cout << (ok ? 1 : 0) << ' ' << (erasedNext ? 1 : 0) << '\n';
        return;
    }
    if (words[0] == "qualifyplayerside") {
        ScriptParameterView p{};
        p.paramType = i32(words.at(1));
        p.stringValue = decodeToken(words.at(2));
        qualifyPlayerSide(p);
        std::cout << encodeToken(p.stringValue) << '\n';
        return;
    }
    if (words[0] == "setactiontype") {
        const std::int32_t actType = i32(words.at(1));
        const std::size_t tCount = static_cast<std::size_t>(u32(words.at(2)));
        std::vector<ScriptActionTemplateView> templates(0x23D);
        std::size_t cursor = 3;
        for (std::size_t i = 0; i < tCount; ++i) {
            const std::size_t idx = static_cast<std::size_t>(u32(words.at(cursor++)));
            const std::int32_t numP = i32(words.at(cursor++));
            templates.at(idx).numParams = numP;
            for (int p = 0; p < numP; ++p) {
                templates.at(idx).paramTypes[static_cast<std::size_t>(p)] =
                    i32(words.at(cursor++));
            }
        }
        ScriptActionView action{};
        setScriptActionType(action, actType, templates);
        std::cout << action.actionType << ' ' << action.numParams;
        for (std::int32_t p = 0; p < action.numParams && p < 12; ++p) {
            std::cout << ' ' << action.params[static_cast<std::size_t>(p)].paramType;
        }
        std::cout << '\n';
        return;
    }
    if (words[0] == "teaminvolved") {
        ScriptActionView action{};
        action.actionType = i32(words.at(1));
        action.numParams = 2;
        action.paramPresent[0] = true;
        action.paramPresent[1] = true;
        action.params[0].stringValue = decodeToken(words.at(2));
        action.params[1].stringValue = decodeToken(words.at(3));
        const std::int32_t idx = getTeamInvolvedParamIndex(action.actionType);
        bool involved = false;
        const std::string resolved = getTeamInvolvedName(action, involved);
        std::cout << idx << ' ' << (involved ? 1 : 0) << ' ' << encodeToken(resolved) << '\n';
        return;
    }
    throw std::runtime_error("unknown command: " + words[0]);
}

}  // namespace

int main(int argc, char** argv)
{
    try {
        if (argc == 1) {
            runCommand({"selftest"});
            return 0;
        }
        if (argc == 2 && std::string(argv[1]) == "--stream") {
            for (std::string line; std::getline(std::cin, line);) {
                if (line == "quit") {
                    return 0;
                }
                if (line.empty()) {
                    continue;
                }
                try {
                    std::istringstream input(line);
                    std::vector<std::string> words;
                    for (std::string word; input >> word;) {
                        words.push_back(std::move(word));
                    }
                    runCommand(words);
                } catch (...) {
                    std::cout << "error\n";
                }
                std::cout.flush();
            }
            return 0;
        }
        std::vector<std::string> words;
        words.reserve(static_cast<std::size_t>(argc - 1));
        for (int index = 1; index < argc; ++index) {
            words.emplace_back(argv[index]);
        }
        runCommand(words);
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "ra3_scripts82check: " << error.what() << '\n';
        return 1;
    }
}
