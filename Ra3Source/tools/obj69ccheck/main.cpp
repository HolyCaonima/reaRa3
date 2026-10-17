#include "ra3/gamelogic/ObjectUpdate69C.h"

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
    std::memcpy(base + offset, &value, sizeof(value));
}

std::uint32_t bitsOf(float value) noexcept {
    std::uint32_t bits = 0;
    std::memcpy(&bits, &value, sizeof(bits));
    return bits;
}

std::uint32_t __fastcall stubArgs(std::uint8_t* self, void*, std::uint32_t a0, std::uint32_t a1, std::uint32_t a2,
    std::uint32_t a3, std::uint32_t a4, std::uint32_t a5, int argc) noexcept {
    const auto result = load<std::uint32_t>(self, 8);
    for (int index = 0; index < argc; ++index) {
        const std::uint32_t arg = index == 0 ? a0 : index == 1 ? a1 : index == 2 ? a2 : index == 3 ? a3 : index == 4 ? a4 : a5;
        store<std::uint32_t>(self, 0x18 + 4 * index, arg);
    }
    store<std::uint32_t>(self, 0x0C, load<std::uint32_t>(self, 0x0C) + 1U);
    return result;
}

std::uint32_t __fastcall stub0(std::uint8_t* self, void*) noexcept {
    return stubArgs(self, nullptr, 0, 0, 0, 0, 0, 0, 0);
}
std::uint32_t __fastcall stub1(std::uint8_t* self, void*, std::uint32_t a0) noexcept {
    return stubArgs(self, nullptr, a0, 0, 0, 0, 0, 0, 1);
}
std::uint32_t __fastcall stub2(std::uint8_t* self, void*, std::uint32_t a0, std::uint32_t a1) noexcept {
    return stubArgs(self, nullptr, a0, a1, 0, 0, 0, 0, 2);
}
std::uint32_t __fastcall stub3(std::uint8_t* self, void*, std::uint32_t a0, std::uint32_t a1, std::uint32_t a2) noexcept {
    return stubArgs(self, nullptr, a0, a1, a2, 0, 0, 0, 3);
}
std::uint32_t __fastcall stub4(std::uint8_t* self, void*, std::uint32_t a0, std::uint32_t a1, std::uint32_t a2,
    std::uint32_t a3) noexcept {
    return stubArgs(self, nullptr, a0, a1, a2, a3, 0, 0, 4);
}
std::uint32_t __fastcall stub5(std::uint8_t* self, void*, std::uint32_t a0) noexcept {
    const auto result = load<std::uint32_t>(self, 0x10);
    store<std::uint32_t>(self, 0x18, a0);
    store<std::uint32_t>(self, 0x0C, load<std::uint32_t>(self, 0x0C) + 1U);
    return result;
}
std::uint32_t __fastcall stub6(std::uint8_t* self, void*, std::uint32_t a0, std::uint32_t a1, std::uint32_t a2,
    std::uint32_t a3) noexcept {
    const auto result = load<std::uint32_t>(self, 0x20);
    store<std::uint32_t>(self, 0x40, a0);
    store<std::uint32_t>(self, 0x44, a1);
    store<std::uint32_t>(self, 0x48, a2);
    store<std::uint32_t>(self, 0x4C, a3);
    store<std::uint32_t>(self, 0x0C, load<std::uint32_t>(self, 0x0C) + 1U);
    return result;
}

std::uint32_t stubAddress(int kind) noexcept {
    switch (kind) {
    case 0: return static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(&stub0));
    case 1: return static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(&stub1));
    case 2: return static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(&stub2));
    case 3: return static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(&stub3));
    case 4: return static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(&stub4));
    case 5: return static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(&stub5));
    case 6: return static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(&stub6));
    default: return 0U;
    }
}

int stubKindOf(std::uint32_t address) noexcept {
    for (int kind = 0; kind <= 6; ++kind) {
        if (address != 0U && address == stubAddress(kind)) {
            return kind;
        }
    }
    return -1;
}

struct Queued {
    std::uint32_t target = 0;
    bool pointer = false;
    std::uint32_t values[8]{};
    int count = 0;
    int index = 0;
};

struct Plant {
    std::uint32_t address = 0;
    std::uint32_t value = 0;
    bool relative = false;
};

Queued g_queues[16]{};
int g_queueCount = 0;
Plant g_plants[8]{};
int g_plantCount = 0;
std::uint8_t* g_blob = nullptr;
std::uint32_t g_blobSize = 0;

std::uint32_t readAbsolute(std::uint32_t address) noexcept {
    const auto base = static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(g_blob));
    for (int index = 0; index < g_plantCount; ++index) {
        if (g_plants[index].address != address) {
            continue;
        }
        if (!g_plants[index].relative) {
            return g_plants[index].value;
        }
        return base + g_plants[index].value;
    }
    return 0U;
}

std::uint32_t directHandler(const ra3::gamelogic::DirectCall& call) noexcept {
    for (int index = 0; index < g_queueCount; ++index) {
        if (g_queues[index].target != call.target || g_queues[index].index >= g_queues[index].count) {
            continue;
        }
        const auto value = g_queues[index].values[g_queues[index].index++];
        if (!g_queues[index].pointer || value == 0U) {
            return value;
        }
        return static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(g_blob)) + value;
    }
    return 0U;
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
        const auto comma = text.find(',', start);
        const auto item = text.substr(start, comma == std::string::npos ? std::string::npos : comma - start);
        if (!item.empty()) {
            FixupEntry entry{};
            if ((item[0] == 'v' || item[0] == 'V') && item.find('@') != std::string::npos) {
                const auto at = item.find('@');
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

void parseStubs(const std::string& text) {
    g_queueCount = 0;
    g_plantCount = 0;
    if (text == "-" || text.empty()) {
        return;
    }
    std::size_t start = 0;
    while (start <= text.size()) {
        const auto comma = text.find(',', start);
        const auto item = text.substr(start, comma == std::string::npos ? std::string::npos : comma - start);
        if (item.size() > 2 && item[1] == ':') {
            const auto eq = item.find('=');
            if ((item[0] == 'g' || item[0] == 'G' || item[0] == 'n' || item[0] == 'N') && g_plantCount < 8) {
                Plant plant{};
                plant.relative = item[0] == 'g' || item[0] == 'G';
                plant.address = static_cast<std::uint32_t>(std::strtoul(item.substr(2, eq - 2).c_str(), nullptr, 16));
                plant.value = static_cast<std::uint32_t>(std::strtoul(item.substr(eq + 1).c_str(), nullptr, 16));
                g_plants[g_plantCount++] = plant;
            } else if (g_queueCount < 16) {
                Queued queued{};
                queued.pointer = item[0] == 'p' || item[0] == 'P';
                queued.target = static_cast<std::uint32_t>(std::strtoul(item.substr(2, eq - 2).c_str(), nullptr, 16));
                std::size_t cursor = eq + 1;
                while (cursor < item.size() && queued.count < 8) {
                    const auto bar = item.find('|', cursor);
                    queued.values[queued.count++] = static_cast<std::uint32_t>(std::strtoul(
                        item.substr(cursor, bar == std::string::npos ? std::string::npos : bar - cursor).c_str(), nullptr, 16));
                    if (bar == std::string::npos) {
                        break;
                    }
                    cursor = bar + 1;
                }
                g_queues[g_queueCount++] = queued;
            }
        }
        if (comma == std::string::npos) {
            break;
        }
        start = comma + 1;
    }
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

std::string toHex(const std::uint8_t* data, std::size_t size) {
    std::ostringstream oss;
    oss << std::hex << std::setfill('0');
    for (std::size_t i = 0; i < size; ++i) {
        oss << std::setw(2) << static_cast<unsigned>(data[i]);
    }
    return oss.str();
}

void relocate(std::vector<std::uint8_t>& blob, const std::vector<FixupEntry>& fixups) {
    const auto base = static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(blob.data()));
    for (const auto& fix : fixups) {
        if (fix.offset + 4 > blob.size()) {
            continue;
        }
        if (fix.stubKind >= 0) {
            store<std::uint32_t>(blob.data(), static_cast<std::ptrdiff_t>(fix.offset), stubAddress(fix.stubKind));
        } else {
            const auto rel = load<std::uint32_t>(blob.data(), static_cast<std::ptrdiff_t>(fix.offset));
            store<std::uint32_t>(blob.data(), static_cast<std::ptrdiff_t>(fix.offset), base + rel);
        }
    }
}

void normalize(std::vector<std::uint8_t>& blob) {
    const auto base = static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(blob.data()));
    for (std::size_t off = 0; off + 4 <= blob.size(); off += 4) {
        const auto cur = load<std::uint32_t>(blob.data(), static_cast<std::ptrdiff_t>(off));
        if (cur >= base && cur < base + blob.size()) {
            store<std::uint32_t>(blob.data(), static_cast<std::ptrdiff_t>(off), cur - base);
        } else {
            const auto kind = stubKindOf(cur);
            if (kind >= 0) {
                store<std::uint32_t>(blob.data(), static_cast<std::ptrdiff_t>(off), 0xF0000000U | static_cast<std::uint32_t>(kind));
            }
        }
    }
}

} // namespace

int main(int argc, char** argv) {
    using namespace ra3::gamelogic;
    if (argc != 2 || std::string(argv[1]) != "--batch") {
        std::cerr << "usage: ra3_obj69ccheck --batch\n";
        return 2;
    }
    setObjectUpdate69CDirect(&directHandler);
    setObjectUpdate69CAbsolute(&readAbsolute);
    std::string line;
    while (std::getline(std::cin, line)) {
        if (line.empty()) {
            continue;
        }
        std::istringstream iss(line);
        std::string vaStr, selfStr, blobHex, fixStr, a0Str, a1Str, a2Str, a3Str, fpcwStr, stubStr;
        if (!(iss >> vaStr >> selfStr >> blobHex >> fixStr >> a0Str >> a1Str >> a2Str >> a3Str)) {
            return 1;
        }
        if (!(iss >> fpcwStr)) {
            fpcwStr = "0x037F";
        }
        if (!(iss >> stubStr)) {
            stubStr = "-";
        }
        const auto va = static_cast<std::uint32_t>(std::strtoul(vaStr.c_str(), nullptr, 0));
        const auto selfOff = static_cast<std::size_t>(std::strtoul(selfStr.c_str(), nullptr, 0));
        auto blob = parseHex(blobHex);
        const auto a0 = static_cast<std::uint32_t>(std::strtoul(a0Str.c_str(), nullptr, 0));
        const auto a1 = static_cast<std::uint32_t>(std::strtoul(a1Str.c_str(), nullptr, 0));
        const auto a2 = static_cast<std::uint32_t>(std::strtoul(a2Str.c_str(), nullptr, 0));
        const auto a3 = static_cast<std::uint32_t>(std::strtoul(a3Str.c_str(), nullptr, 0));
        g_blob = blob.data();
        g_blobSize = static_cast<std::uint32_t>(blob.size());
        parseStubs(stubStr);
        relocate(blob, parseFixups(fixStr));
        g_blob = blob.data();
        auto* self = selfOff < blob.size() ? blob.data() + selfOff : nullptr;
        std::uint32_t ret = 0;
        switch (va) {
        case 0x00699C10: aiUpdateIssue61(self, a0, a1, a2); break;
        case 0x00699C60: aiUpdateIssue51(self, a0, a1); break;
        case 0x00699CD0: aiUpdateIssue3A(self, a0, a1); break;
        case 0x00699D20: aiUpdateIssue32(self, a0, a1); break;
        case 0x00699D60: aiUpdateIssue40(self, a0, a1, a2); break;
        case 0x00699DC0: aiUpdateIssue42(self, a0, a1, a2); break;
        case 0x00699E20: aiUpdateIssue3F(self, a0, a1, a2); break;
        case 0x00699E80: aiUpdateIssue43(self, a0, a1, a2); break;
        case 0x00699EE0: aiUpdateIssue49(self, a0, a1, a2); break;
        case 0x00699F60: aiUpdateIssue2A(self, a0, a1); break;
        case 0x006991B0: aiUpdateTouchLocomotor(self); break;
        case 0x00695990: specialPowerModuleNoteReady(self); break;
        case 0x00696430: specialPowerModuleTrigger(self, a0, a1); break;
        case 0x006906E0: managerBodyCacheTemplate(self); break;
        case 0x00690A80: ret = moneyCrateCollideGrant(self, ptrAt(blob, a0)); break;
        case 0x00692BA0: ret = ptrToResult(blob, tunnelContainOpenIterator(self, ptrAt(blob, a0))); break;
        case 0x00690240: unitVisionBehaviorStoreRange(self, a0, a1); break;
        case 0x00690270: unitVisionBehaviorXfer(self, ptrAt(blob, a0)); break;
        case 0x00693FB0: ret = detonateStateRequest(self); break;
        case 0x006975C0: dozerAiUpdateAcceptTask(self, ptrAt(blob, a0), a1); break;
        case 0x006908E0: ret = crateCollideAllows(self, ptrAt(blob, a0)); break;
        case 0x0069FF20: stealthDetectorUpdateXfer(self, ptrAt(blob, a0)); break;
        case 0x00696BC0: specialPowerUpdateModuleXfer(self, ptrAt(blob, a0)); break;
        case 0x0069F100: secondaryDamageTrackerXfer(self, ptrAt(blob, a0)); break;
        case 0x00696740: ret = bitsOf(specialPowerUpdateReadiness(self)); break;
        case 0x00696800: ret = specialPowerUpdateNotifyOwner(self); break;
        case 0x00697630: ret = bitsOf(dozerAiUpdateTerrainScale(self)); break;
        case 0x006920B0: ret = garrisonContainAccepts(self, ptrAt(blob, a0), a1, a2); break;
        case 0x00692460: ret = ptrToResult(blob, garrisonContainAnchorOrSlot(self)); break;
        case 0x00692490: ret = ptrToResult(blob, garrisonContainLocatePoint(self)); break;
        case 0x0069E660: ret = productionUpdateCancelOne(self, a0); break;
        case 0x0069FA90: ret = specialAbilityUpdateRelay(self, a0, a1); break;
        case 0x00695720: ret = ptrToResult(blob, specialPowerDispatchCopyKind(self, reinterpret_cast<std::uint32_t*>(ptrAt(blob, a0)))); break;
        case 0x006953E0: specialPowerDispatchAdjustCount(self, a0); break;
        case 0x00695210: specialPowerDispatchApplyKind4(self, a0); break;
        case 0x00695170: specialPowerDispatchApplyKind23(self, a0, a1); break;
        case 0x006950D0: specialPowerDispatchApplyKind03(self, a0, a1); break;
        case 0x00695020: specialPowerDispatchApplyKind1(self, a0, a1, a2); break;
        case 0x00697B80: ret = bitsOf(dockUpdateOccupiedScale(self)); break;
        case 0x006913D0: ret = hordeContainSharesSide(self, ptrAt(blob, a0), a1, ptrAt(blob, a2), a3); break;
        case 0x00691620: hordeContainPushTimer(self, a0); break;
        default:
            std::cerr << "Unknown VA: 0x" << std::hex << va << "\n";
            return 2;
        }
        normalize(blob);
        std::cout << "ret=0x" << std::hex << std::setw(8) << std::setfill('0') << ret
                  << " blob=" << toHex(blob.data(), blob.size()) << "\n";
    }
    return 0;
}
