#include "ra3/gamelogic/ObjectUpdate69.h"

#include <cstdint>
#include <cstdlib>
#include <cstring>
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

std::uint32_t bitsOf(float value) noexcept {
    std::uint32_t bits = 0;
    std::memcpy(&bits, &value, sizeof(bits));
    return bits;
}

float floatFromBits(std::uint32_t bits) noexcept {
    float value = 0.0F;
    std::memcpy(&value, &bits, sizeof(value));
    return value;
}

// Shared with verify_obj69.py. thiscall callers invoke these __fastcall
// bodies; both sides record the call in the object and return a planted dword.
std::uint32_t __fastcall stub0(std::uint8_t* self, void*) noexcept {
    const auto result = load<std::uint32_t>(self, 8);
    store<std::uint32_t>(self, 0x0C, load<std::uint32_t>(self, 0x0C) + 1U);
    return result;
}

std::uint32_t __fastcall stub1(std::uint8_t* self, void*, std::uint32_t arg) noexcept {
    const auto result = load<std::uint32_t>(self, 8);
    store<std::uint32_t>(self, 0x18, arg);
    store<std::uint32_t>(self, 0x0C, load<std::uint32_t>(self, 0x0C) + 1U);
    return result;
}

std::uint32_t __fastcall stub2(
    std::uint8_t* self, void*, std::uint32_t arg0, std::uint32_t arg1) noexcept {
    const auto result = load<std::uint32_t>(self, 8);
    store<std::uint32_t>(self, 0x18, arg0);
    store<std::uint32_t>(self, 0x1C, arg1);
    store<std::uint32_t>(self, 0x0C, load<std::uint32_t>(self, 0x0C) + 1U);
    return result;
}

std::uint32_t __fastcall stub3(
    std::uint8_t* self, void*, std::uint32_t arg0, std::uint32_t arg1, std::uint32_t arg2) noexcept {
    const auto result = load<std::uint32_t>(self, 8);
    store<std::uint32_t>(self, 0x18, arg0);
    store<std::uint32_t>(self, 0x1C, arg1);
    store<std::uint32_t>(self, 0x20, arg2);
    store<std::uint32_t>(self, 0x0C, load<std::uint32_t>(self, 0x0C) + 1U);
    return result;
}

std::uint32_t __fastcall stub4(std::uint8_t* self, void*) noexcept {
    const auto result = load<std::uint32_t>(self, 0x10);
    store<std::uint32_t>(self, 0x14, load<std::uint32_t>(self, 0x14) + 1U);
    return result;
}

std::uint32_t stubAddress(int kind) noexcept {
    switch (kind) {
    case 0:
        return static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(&stub0));
    case 1:
        return static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(&stub1));
    case 2:
        return static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(&stub2));
    case 3:
        return static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(&stub3));
    case 4:
        return static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(&stub4));
    default:
        return 0U;
    }
}

bool isStubAddress(std::uint32_t addr) noexcept {
    for (int kind = 0; kind <= 4; ++kind) {
        if (addr != 0U && addr == stubAddress(kind)) {
            return true;
        }
    }
    return false;
}

std::vector<std::uint8_t> parseHex(const std::string& text) {
    std::vector<std::uint8_t> out;
    if (text == "-" || text.empty()) {
        return out;
    }
    for (std::size_t i = 0; i + 1 < text.size(); i += 2) {
        out.push_back(static_cast<std::uint8_t>(std::strtoul(text.substr(i, 2).c_str(), nullptr, 16)));
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

struct FixupEntry {
    int stubKind = -1;
    std::size_t offset = 0;
};

std::vector<FixupEntry> parseFixups(const std::string& text) {
    std::vector<FixupEntry> out;
    if (text == "-" || text.empty()) {
        return out;
    }
    std::size_t start = 0;
    while (start <= text.size()) {
        const std::size_t comma = text.find(',', start);
        const std::string item =
            text.substr(start, comma == std::string::npos ? std::string::npos : comma - start);
        if (!item.empty()) {
            FixupEntry entry{};
            if ((item[0] == 'v' || item[0] == 'V') && item.find('@') != std::string::npos) {
                const std::size_t at = item.find('@');
                entry.stubKind = std::atoi(item.substr(1, at - 1).c_str());
                entry.offset = static_cast<std::size_t>(std::strtoul(item.substr(at + 1).c_str(), nullptr, 0));
            } else {
                entry.offset = static_cast<std::size_t>(std::strtoul(item.c_str(), nullptr, 0));
            }
            out.push_back(entry);
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
    const std::vector<FixupEntry>& fixups,
    std::uint32_t a0,
    std::uint32_t a1,
    std::uint32_t a2) {
    using namespace ra3::gamelogic;
    const auto baseAddr = static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(blob.data()));
    for (const FixupEntry& fix : fixups) {
        if (fix.offset + 4 > blob.size()) {
            continue;
        }
        if (fix.stubKind >= 0) {
            store<std::uint32_t>(blob.data(), fix.offset, stubAddress(fix.stubKind));
        } else {
            const auto rel = load<std::uint32_t>(blob.data(), fix.offset);
            store<std::uint32_t>(blob.data(), fix.offset, baseAddr + rel);
        }
    }
    std::uint8_t* self = selfOffset < blob.size() ? blob.data() + selfOffset : nullptr;
    std::uint32_t ret = 0;
    switch (va) {
    case 0x00690740:
        ret = stancesBehaviorFirstNodeValue(self);
        break;
    case 0x006911D0:
        ret = bitsOf(hordeContainDecayRange(self, floatFromBits(a0)));
        break;
    case 0x00691350:
        hordeContainInvertSlotF4(self);
        break;
    case 0x006914B0:
        hordeContainFirstPassengerId(self, reinterpret_cast<std::uint32_t*>(ptrAt(blob, a0)));
        break;
    case 0x006914E0:
        ret = hordeContainField5cClear(self);
        break;
    case 0x006914F0:
        hordeContainNotifyPassengerList(self);
        break;
    case 0x00691550:
        hordeContainNotifyPassengerListShort(self);
        break;
    case 0x006919E0:
        hordeContainMarkRecord(self, a0, static_cast<std::int32_t>(a1));
        break;
    case 0x00691A30:
        ret = hordeContainCopyRecordVector(self, static_cast<std::int32_t>(a0), ptrAt(blob, a1));
        break;
    case 0x00691990:
        ret = slaughterHordeContainPasses(self, ptrAt(blob, a0));
        break;
    case 0x006923A0:
        infiltratorContainZeroSevenDwords(self, ptrAt(blob, a0), a1);
        break;
    case 0x00692500:
        openContainStorePairIds(
            self, ptrAt(blob, a0), static_cast<std::int32_t>(a1), ptrAt(blob, a2));
        break;
    case 0x00692580:
        ret = openContainPlaneAccepts(self, ptrAt(blob, a0));
        break;
    case 0x006925F0:
        ret = openContainPairPlaneAccepts(self, ptrAt(blob, a0), ptrAt(blob, a1));
        break;
    case 0x006926D0:
        ret = openContainHasField78(self);
        break;
    case 0x00692850:
        ret = openContainBitClear(self, reinterpret_cast<std::uint32_t*>(ptrAt(blob, a0)));
        break;
    case 0x00692890:
        openContainCopySevenDwords(self, ptrAt(blob, a0), a1);
        break;
    case 0x00694670:
        ret = beamSpecialAbilityAllowsTarget(self, ptrAt(blob, a0));
        break;
    case 0x00694710:
        ret = specialPowerUpdateIdMatches(self, a0);
        break;
    case 0x00694730:
        specialPowerUpdateReadFieldC(self, reinterpret_cast<std::uint32_t*>(ptrAt(blob, a0)));
        break;
    case 0x00695670:
        table00C00F4CStoreByte39(self, a0);
        break;
    case 0x00695690:
        table00C00F4CReadField54(self, reinterpret_cast<std::uint32_t*>(ptrAt(blob, a0)));
        break;
    case 0x006966B0:
        ret = specialPowerUpdateIdEquals(self, a0);
        break;
    case 0x00696870:
        specialPowerUpdateReadSlot1(self, reinterpret_cast<std::uint32_t*>(ptrAt(blob, a0)));
        break;
    case 0x00697100:
        ret = uncombineSpecialPowerHasEntries(self);
        break;
    case 0x00697130:
        ret = weaponFireSpecialAbilityFlagBit8(self);
        break;
    case 0x00697720:
        ret = supplyTruckSumDwords(self);
        break;
    case 0x006978D0:
        ret = bitsOf(supplyTruckRangeScale(self));
        break;
    case 0x006979E0:
        ret = dockUpdateIdPresentOrOpen(self, ptrAt(blob, a0));
        break;
    case 0x00697A40:
        ret = dockUpdateSlotReady(self, ptrAt(blob, a0), static_cast<std::int32_t>(a1));
        break;
    case 0x00697AA0:
        dockUpdateMarkId(self, ptrAt(blob, a0));
        break;
    case 0x006981B0:
        ret = defaultProductionExitTransformPoint(self, reinterpret_cast<float*>(ptrAt(blob, a0)));
        break;
    case 0x00699710:
        ret = aiUpdatePathGoalActive(self);
        break;
    case 0x006997B0:
        aiUpdateClearGoalFloats(self);
        break;
    case 0x006997E0:
        aiUpdateSetGoalPosition(self, ptrAt(blob, a0));
        break;
    case 0x00699840:
        aiUpdateSetGoalFloat(self, a0);
        break;
    case 0x00699860:
        ret = aiUpdateLocomotorAllowsMove(self);
        break;
    case 0x00699CA0:
        aiUpdatePulseModule3c(self, a0, a1);
        break;
    case 0x0069B5A0:
        fireWeaponUpdateReadFieldC(self, reinterpret_cast<std::uint32_t*>(ptrAt(blob, a0)));
        break;
    case 0x0069C1B0:
        ret = ptrToResult(blob, invisibilityBroadcastAdjustor24(self));
        break;
    case 0x0069E5B0:
        ret = productionUpdateConsumeMatchingLink(self, ptrAt(blob, a0));
        break;
    default:
        std::cerr << "Unknown VA: 0x" << std::hex << va << "\n";
        return 2;
    }

    for (std::size_t off = 0; off + 4 <= blob.size(); off += 4) {
        const auto cur = load<std::uint32_t>(blob.data(), off);
        if (cur >= baseAddr && cur < baseAddr + blob.size()) {
            store<std::uint32_t>(blob.data(), off, cur - baseAddr);
        } else if (isStubAddress(cur)) {
            for (int kind = 0; kind <= 4; ++kind) {
                if (cur == stubAddress(kind)) {
                    store<std::uint32_t>(blob.data(), off, 0xF0000000U | static_cast<std::uint32_t>(kind));
                    break;
                }
            }
        }
    }
    std::cout << "ret=0x" << std::hex << std::setw(8) << std::setfill('0') << ret
              << " blob=" << toHex(blob.data(), blob.size()) << "\n";
    return 0;
}

} // namespace

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
            std::string fpcwStr;
            if (!(iss >> vaStr >> selfStr >> blobHex >> fixStr >> a0Str >> a1Str >> a2Str)) {
                return 1;
            }
            if (iss >> fpcwStr) {
                // Accepted for protocol compatibility. These bodies do not read the x87 control word.
            }
            const auto va = static_cast<std::uint32_t>(std::strtoul(vaStr.c_str(), nullptr, 0));
            const auto selfOffset = static_cast<std::size_t>(std::strtoul(selfStr.c_str(), nullptr, 0));
            auto blob = parseHex(blobHex);
            const int rc = runCase(
                va,
                selfOffset,
                blob,
                parseFixups(fixStr),
                static_cast<std::uint32_t>(std::strtoul(a0Str.c_str(), nullptr, 0)),
                static_cast<std::uint32_t>(std::strtoul(a1Str.c_str(), nullptr, 0)),
                static_cast<std::uint32_t>(std::strtoul(a2Str.c_str(), nullptr, 0)));
            if (rc != 0) {
                return rc;
            }
        }
        return 0;
    }
    std::cerr << "usage: ra3_obj69check --batch\n";
    return 2;
}
