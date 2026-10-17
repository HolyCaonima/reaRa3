#include "ra3/core/TextFile4D.h"
#include "ra3/gamelogic/GameLogicCore4D.h"

#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <float.h>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

namespace {

template <typename T>
T load(const std::uint8_t* base, std::ptrdiff_t offset) noexcept {
    T value{};
    std::memcpy(&value, base + offset, sizeof(T));
    return value;
}

template <typename T>
void store(std::uint8_t* base, std::ptrdiff_t offset, T value) noexcept {
    std::memcpy(base + offset, &value, sizeof(T));
}

float bitsToFloat(std::uint32_t bits) noexcept {
    float value = 0.0F;
    std::memcpy(&value, &bits, sizeof(value));
    return value;
}

std::uint32_t floatToBits(float value) noexcept {
    std::uint32_t bits = 0U;
    std::memcpy(&bits, &value, sizeof(bits));
    return bits;
}

std::vector<std::uint8_t> parseHex(const std::string& text) {
    std::vector<std::uint8_t> out;
    if (text == "-" || text.empty()) {
        return out;
    }
    for (std::size_t i = 0; i + 1 < text.size(); i += 2) {
        out.push_back(
            static_cast<std::uint8_t>(std::strtoul(text.substr(i, 2).c_str(), nullptr, 16)));
    }
    return out;
}

std::string toHex(const std::uint8_t* data, std::size_t size) {
    std::ostringstream oss;
    oss << std::hex << std::setfill('0');
    for (std::size_t i = 0; i < size; ++i) {
        oss << std::setw(2) << static_cast<unsigned>(data[i]);
    }
    return oss.str();
}

std::vector<std::size_t> parseFixups(const std::string& text) {
    std::vector<std::size_t> out;
    if (text == "-" || text.empty()) {
        return out;
    }
    std::size_t start = 0;
    while (start <= text.size()) {
        const std::size_t comma = text.find(',', start);
        const std::string item =
            text.substr(start, comma == std::string::npos ? std::string::npos : comma - start);
        if (!item.empty()) {
            out.push_back(static_cast<std::size_t>(std::strtoul(item.c_str(), nullptr, 0)));
        }
        if (comma == std::string::npos) {
            break;
        }
        start = comma + 1;
    }
    return out;
}

std::uint8_t* ptrAt(std::vector<std::uint8_t>& blob, std::uint32_t offset) noexcept {
    if (offset == 0xFFFFFFFFU || offset >= blob.size()) {
        return nullptr;
    }
    return blob.data() + offset;
}

std::uint32_t ptrToResult(const std::vector<std::uint8_t>& blob, const void* ptr) noexcept {
    if (ptr == nullptr) {
        return 0U;
    }
    const auto base = reinterpret_cast<std::uintptr_t>(blob.data());
    const auto raw = reinterpret_cast<std::uintptr_t>(ptr);
    if (raw >= base && raw < base + blob.size()) {
        return static_cast<std::uint32_t>(raw - base);
    }
    return static_cast<std::uint32_t>(raw);
}

int runCase(
    std::uint32_t va,
    std::size_t selfOffset,
    std::vector<std::uint8_t>& blob,
    const std::vector<std::size_t>& fixups,
    std::uint32_t a0,
    std::uint32_t a1,
    std::uint32_t a2,
    std::uint32_t a3,
    std::uint32_t a4,
    std::uint16_t fpcw) {
    using namespace ra3::core;
    using namespace ra3::gamelogic;

    const auto baseAddr =
        static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(blob.data()));
    for (const std::size_t off : fixups) {
        if (off + 4 <= blob.size()) {
            const auto rel = load<std::uint32_t>(blob.data(), static_cast<std::ptrdiff_t>(off));
            store<std::uint32_t>(
                blob.data(), static_cast<std::ptrdiff_t>(off), baseAddr + rel);
        }
    }

    std::uint16_t savedCw = 0x037F;
#if defined(_MSC_VER) && defined(_M_IX86)
    __asm {
        fnstcw savedCw
        fldcw fpcw
    }
#endif

    std::uint8_t* self = selfOffset < blob.size() ? (blob.data() + selfOffset) : nullptr;
    std::uint32_t ret = 0U;

    switch (va) {
    case 0x004D05A0:
        ret = ptrToResult(blob, getTextFileLine4D(self, static_cast<std::int32_t>(a0)));
        break;
    case 0x004D2AD0:
        relinkGhostObjectNodeBefore4D(self, ptrAt(blob, a0));
        break;
    case 0x004D3750:
        ret = equalsObjectIDPair4D(self, ptrAt(blob, a0));
        break;
    case 0x004D3770:
        ret = floatToBits(normalizeAngleRad4D(bitsToFloat(a0)));
        break;
    case 0x004D3CD0:
        ret = getBehaviorModuleClassNameVa4D();
        break;
    case 0x004D4020:
        ret = static_cast<std::uint32_t>(
            convertSecondsToFrames4D(self, ptrAt(blob, a0), bitsToFloat(a1)));
        break;
    case 0x004D4260:
        ret = getCampaignSystemNameVa4D();
        break;
    case 0x004D4320:
        resetDelayedExperienceLevelGrantSystem4D(self);
        break;
    case 0x004D4330:
        ret = getDelayedExperienceLevelGrantSystemNameVa4D();
        break;
    case 0x004D4400:
        ret = ptrToResult(blob, assignDamageInfoInputPayload4D(self, ptrAt(blob, a0)));
        break;
    case 0x004D4500:
        ret = compareCooldownProgress4D(ptrAt(blob, a0), ptrAt(blob, a1), a2);
        break;
    case 0x004D4910:
        ret = ptrToResult(blob, initThingTemplateRefWrapper4D(self, a0));
        break;
    case 0x004D4C50:
        ret = allocateNextSequenceId4D(self);
        break;
    case 0x004D4CD0:
        ret = countLinkedNodesAtBC4D(self);
        break;
    case 0x004D4EA0:
        updateCrcBufferMaxSize4D(self, a0);
        break;
    case 0x004D5530:
        updateMaxFloatAt204D(self, bitsToFloat(a0));
        break;
    case 0x004D57A0:
        snapAngleToCardinalRad4D(reinterpret_cast<float*>(ptrAt(blob, a0)));
        break;
    case 0x004D5900:
        ret = anyBitFlags288_4D(self) ? 1U : 0U;
        break;
    case 0x004D5920:
        ret = testRequiredAndForbiddenBitFlags288_4D(self, ptrAt(blob, a0), ptrAt(blob, a1)) ? 1U
                                                                                              : 0U;
        break;
    case 0x004D5A40:
        ret = intersectsBitFlags480_4D(self, ptrAt(blob, a0)) ? 1U : 0U;
        break;
    case 0x004D5AB0:
        ret = anyBitFlags480_4D(self) ? 1U : 0U;
        break;
    case 0x004D5B10:
        ret = equalsBitFlags480_4D(ptrAt(blob, a0), ptrAt(blob, a1)) ? 1U : 0U;
        break;
    case 0x004D5BB0:
        ret = anyObjectStatusMask224_4D(self) ? 1U : 0U;
        break;
    case 0x004D5BD0:
        ret = intersectsObjectStatusMask224_4D(self, ptrAt(blob, a0)) ? 1U : 0U;
        break;
    case 0x004D5DF0:
        ret = ptrToResult(blob, copyConstructDamageInfoInput4D(self, ptrAt(blob, a0)));
        break;
    case 0x004D6170:
        ret = containsPointStrictRegion2D4D(self, ptrAt(blob, a0));
        break;
    case 0x004D6500:
        ret = ptrToResult(blob, resetMoveGoalResult4D(self));
        break;
    case 0x004D6550:
        ret = ptrToResult(blob, defaultConstructDamageInfoInput4D(self));
        break;
    case 0x004D6680:
        ret = anyBitAt26C4D(self) ? 1U : 0U;
        break;
    case 0x004D66A0:
        ret = testBitAt26C4D(self, a0);
        break;
    case 0x004D6710:
        ret = getSpecialAbilityField0C4D(self);
        break;
    case 0x004D6730:
        ret = getSpecialAbilityField104D(self);
        break;
    case 0x004D6750:
        ret = hasAnyLowThreeFlagsAt08_4D(self);
        break;
    case 0x004D67F0:
        ret = getUpdateModuleClassNameVa4D();
        break;
    case 0x004D6960:
        stampPassiveAreaEffectMaskedSlots4D(
            self, reinterpret_cast<const std::uint32_t*>(ptrAt(blob, a0)), a1);
        break;
    case 0x004D69A0:
        ret = isEntryInBeamExcludeList4D(ptrAt(blob, a0), ptrAt(blob, a1)) ? 1U : 0U;
        break;
    case 0x004D6AB0:
        ret = ptrToResult(blob, findCampaignRecordByNestedKey4D(self, a0));
        break;
    case 0x004D6FF0:
        ret = ptrToResult(blob, assignDamageInfoShort4D(self, ptrAt(blob, a0)));
        break;
    case 0x004D7040:
        ret = removeObjectIdSwapLastAt24_4D(self, ptrAt(blob, a0)) ? 1U : 0U;
        break;
    case 0x004D8390:
        ret = resolveAIUpgradeTargetTemplateRef4D(self);
        break;
    case 0x004D86D0: {
        const std::uint32_t beginVa = (a0 == 0xFFFFFFFFU) ? 0U : (baseAddr + a0);
        const std::uint32_t endVa = (a1 == 0xFFFFFFFFU) ? 0U : (baseAddr + a1);
        ret = isDamageAndSpawnTableEntryFlag90Set4D(self, beginVa, endVa) ? 1U : 0U;
        break;
    }
    case 0x004D8780:
        ret = matchesObjectStatusFilter4D(self, ptrAt(blob, a0)) ? 1U : 0U;
        break;
    case 0x004D8810:
        ret = matchesRelationshipFilter4D(
                  ptrAt(blob, a0), ptrAt(blob, a1), ptrAt(blob, a2), static_cast<std::int32_t>(a3))
                  ? 1U
                  : 0U;
        break;
    case 0x004D8930:
        ret = findTaggedContactMidpoint4D(ptrAt(blob, a0), ptrAt(blob, a1)) ? 1U : 0U;
        break;
    case 0x004D9230:
        buildZRotationMatrix4D(ptrAt(blob, a0), bitsToFloat(a1));
        break;
    case 0x004D9280:
        ret = countSetBits288_4D(self);
        break;
    case 0x004DA690:
        ret = ptrToResult(blob, assignDOTInfoRecord4D(self, ptrAt(blob, a0)));
        break;
    case 0x004DA780:
        ret = ptrToResult(blob, defaultConstructLuaEventParameters4D(self));
        break;
    case 0x004DAD10:
        ret = ptrToResult(
            blob,
            findRegisteredPairByKey4D(
                self, reinterpret_cast<const std::uint32_t*>(ptrAt(blob, a0))));
        break;
    case 0x004DB8C0:
        ret = ptrToResult(blob, resetGlobalWeatherData4D(self));
        break;
    case 0x004DBB30:
        ret = ptrToResult(
            blob, initExitGoalEntry4D(self, a0, a1, a2, a3, static_cast<std::uint8_t>(a4)));
        break;
    case 0x004DBBF0:
        gridToWorldPlacement4D(
            self,
            ptrAt(blob, a0),
            static_cast<std::int32_t>(a1),
            static_cast<std::int32_t>(a2),
            static_cast<std::uint8_t>(a3));
        break;
    case 0x004DBD20:
        snapWorldPointToGrid4D(self, ptrAt(blob, a0));
        break;
    case 0x004DBEC0:
        offsetWorldPointByGridCells4D(
            self,
            ptrAt(blob, a0),
            ptrAt(blob, a1),
            static_cast<std::int32_t>(a2),
            static_cast<std::int32_t>(a3));
        break;
    case 0x004DC080:
        worldToGridCell4D(
            self,
            ptrAt(blob, a0),
            reinterpret_cast<std::int32_t*>(ptrAt(blob, a1)),
            reinterpret_cast<std::int32_t*>(ptrAt(blob, a2)),
            static_cast<std::uint8_t>(a3));
        break;
    case 0x004DD390:
        ret = ptrToResult(blob, defaultConstructMapMissionObjectives4D(self));
        break;
    case 0x004DD3D0:
        ret = ptrToResult(blob, defaultConstructCampaignRecordTable4D(self));
        break;
    case 0x004DFC30:
        snapRotatedFootprintToGrid4D(self, ptrAt(blob, a0), ptrAt(blob, a1), bitsToFloat(a2));
        break;
    default:
#if defined(_MSC_VER) && defined(_M_IX86)
        __asm { fldcw savedCw }
#endif
        std::cerr << "Unknown VA: 0x" << std::hex << va << "\n";
        return 2;
    }

#if defined(_MSC_VER) && defined(_M_IX86)
    __asm {
        fldcw savedCw
    }
#endif

    for (std::size_t off = 0; off + 4 <= blob.size(); off += 4) {
        const auto cur = load<std::uint32_t>(blob.data(), static_cast<std::ptrdiff_t>(off));
        if (cur >= baseAddr && cur < baseAddr + blob.size()) {
            store<std::uint32_t>(
                blob.data(), static_cast<std::ptrdiff_t>(off), cur - baseAddr);
        }
    }

    std::cout << "ret=0x" << std::hex << std::setw(8) << std::setfill('0') << ret
              << " blob=" << toHex(blob.data(), blob.size()) << "\n";
    return 0;
}

}  // namespace

int main(int argc, char** argv) {
    if (argc == 2 && std::string(argv[1]) == "--batch") {
        std::string line;
        while (std::getline(std::cin, line)) {
            if (line.empty()) {
                continue;
            }
            std::istringstream iss(line);
            std::string vaStr;
            std::string selfStr;
            std::string blobHex;
            std::string fixStr;
            std::string a0Str;
            std::string a1Str;
            std::string a2Str;
            std::string a3Str;
            std::string a4Str;
            std::string fpcwStr;
            if (!(iss >> vaStr >> selfStr >> blobHex >> fixStr >> a0Str >> a1Str >> a2Str >>
                  a3Str >> a4Str)) {
                return 1;
            }
            std::uint16_t fpcw = 0x037F;
            if (iss >> fpcwStr) {
                fpcw = static_cast<std::uint16_t>(std::strtoul(fpcwStr.c_str(), nullptr, 0));
            }
            const auto va = static_cast<std::uint32_t>(std::strtoul(vaStr.c_str(), nullptr, 0));
            const auto selfOffset =
                static_cast<std::size_t>(std::strtoul(selfStr.c_str(), nullptr, 0));
            auto blob = parseHex(blobHex);
            const auto fixups = parseFixups(fixStr);
            const auto a0 = static_cast<std::uint32_t>(std::strtoul(a0Str.c_str(), nullptr, 0));
            const auto a1 = static_cast<std::uint32_t>(std::strtoul(a1Str.c_str(), nullptr, 0));
            const auto a2 = static_cast<std::uint32_t>(std::strtoul(a2Str.c_str(), nullptr, 0));
            const auto a3 = static_cast<std::uint32_t>(std::strtoul(a3Str.c_str(), nullptr, 0));
            const auto a4 = static_cast<std::uint32_t>(std::strtoul(a4Str.c_str(), nullptr, 0));
            const int rc = runCase(va, selfOffset, blob, fixups, a0, a1, a2, a3, a4, fpcw);
            if (rc != 0) {
                return rc;
            }
        }
        return 0;
    }

    if (argc < 10) {
        std::cerr << "Usage: ra3_logic4dcheck <va> <self_offset> <blob_hex> <ptr_fixups_csv> "
                     "<a0> <a1> <a2> <a3> <a4> [fpcw]\n"
                     "       ra3_logic4dcheck --batch\n";
        return 1;
    }

    const auto va = static_cast<std::uint32_t>(std::strtoul(argv[1], nullptr, 0));
    const auto selfOffset = static_cast<std::size_t>(std::strtoul(argv[2], nullptr, 0));
    auto blob = parseHex(argv[3]);
    const auto fixups = parseFixups(argv[4]);
    const auto a0 = static_cast<std::uint32_t>(std::strtoul(argv[5], nullptr, 0));
    const auto a1 = static_cast<std::uint32_t>(std::strtoul(argv[6], nullptr, 0));
    const auto a2 = static_cast<std::uint32_t>(std::strtoul(argv[7], nullptr, 0));
    const auto a3 = static_cast<std::uint32_t>(std::strtoul(argv[8], nullptr, 0));
    const auto a4 = static_cast<std::uint32_t>(std::strtoul(argv[9], nullptr, 0));
    const std::uint16_t fpcw =
        argc >= 11 ? static_cast<std::uint16_t>(std::strtoul(argv[10], nullptr, 0)) : 0x037F;

    return runCase(va, selfOffset, blob, fixups, a0, a1, a2, a3, a4, fpcw);
}
