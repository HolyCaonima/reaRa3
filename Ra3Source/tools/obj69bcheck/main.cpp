#include "ra3/gamelogic/ObjectUpdate69B.h"

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

float floatFromBits(std::uint32_t bits) noexcept {
    float value = 0.0F;
    std::memcpy(&value, &bits, sizeof(value));
    return value;
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
std::uint32_t __fastcall stub5(std::uint8_t* self, void*, std::uint32_t a0, std::uint32_t a1, std::uint32_t a2,
    std::uint32_t a3, std::uint32_t a4) noexcept {
    return stubArgs(self, nullptr, a0, a1, a2, a3, a4, 0, 5);
}
std::uint32_t __fastcall stub6(std::uint8_t* self, void*, std::uint32_t a0, std::uint32_t a1, std::uint32_t a2,
    std::uint32_t a3, std::uint32_t a4, std::uint32_t a5) noexcept {
    return stubArgs(self, nullptr, a0, a1, a2, a3, a4, a5, 6);
}
std::uint32_t __fastcall stub7(std::uint8_t* self, void*) noexcept {
    const auto result = load<std::uint32_t>(self, 0x10);
    store<std::uint32_t>(self, 0x14, load<std::uint32_t>(self, 0x14) + 1U);
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
    case 7: return static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(&stub7));
    default: return 0U;
    }
}

int stubKindOf(std::uint32_t address) noexcept {
    for (int kind = 0; kind <= 7; ++kind) {
        if (address != 0U && address == stubAddress(kind)) {
            return kind;
        }
    }
    return -1;
}

struct Queued {
    std::uint32_t target = 0;
    bool pointer = false;
    std::uint32_t values[6]{};
    int count = 0;
    int index = 0;
};

Queued g_queues[12]{};
int g_queueCount = 0;
std::uint8_t* g_blob = nullptr;
std::uint32_t g_blobSize = 0;

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
    if (text == "-" || text.empty()) {
        return;
    }
    std::size_t start = 0;
    while (start <= text.size() && g_queueCount < 12) {
        const auto comma = text.find(',', start);
        const auto item = text.substr(start, comma == std::string::npos ? std::string::npos : comma - start);
        if (item.size() > 2 && item[1] == ':') {
            const auto eq = item.find('=');
            Queued queued{};
            queued.pointer = item[0] == 'p' || item[0] == 'P';
            queued.target = static_cast<std::uint32_t>(std::strtoul(item.substr(2, eq - 2).c_str(), nullptr, 16));
            std::size_t cursor = eq + 1;
            while (cursor < item.size() && queued.count < 6) {
                const auto bar = item.find('|', cursor);
                queued.values[queued.count++] = static_cast<std::uint32_t>(
                    std::strtoul(item.substr(cursor, bar == std::string::npos ? std::string::npos : bar - cursor).c_str(),
                        nullptr, 16));
                if (bar == std::string::npos) {
                    break;
                }
                cursor = bar + 1;
            }
            g_queues[g_queueCount++] = queued;
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
        std::cerr << "usage: ra3_obj69bcheck --batch\n";
        return 2;
    }
    setObjectUpdate69BDirect(&directHandler);
    std::string line;
    while (std::getline(std::cin, line)) {
        if (line.empty()) {
            continue;
        }
        std::istringstream iss(line);
        std::string vaStr, selfStr, blobHex, fixStr, a0Str, a1Str, a2Str, fpcwStr, stubStr;
        if (!(iss >> vaStr >> selfStr >> blobHex >> fixStr >> a0Str >> a1Str >> a2Str)) {
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
        g_blob = blob.data();
        g_blobSize = static_cast<std::uint32_t>(blob.size());
        parseStubs(stubStr);
        relocate(blob, parseFixups(fixStr));
        auto* self = selfOff < blob.size() ? blob.data() + selfOff : nullptr;
        std::uint32_t ret = 0;
        switch (va) {
        case 0x00690AF0: ret = salvageCrateCollideAccepts(self, ptrAt(blob, a0)); break;
        case 0x00690E60: ret = fireWeaponCollideMasksAllow(self); break;
        case 0x00691250: ret = hordeContainAnchorRejected(self); break;
        case 0x006912E0: ret = hordeContainForwardId(self, ptrAt(blob, a0)); break;
        case 0x00691660: hordeContainRelayAnchor(self, a0); break;
        case 0x00691C30: ret = ptrToResult(blob, hordeContainRecordSpanPointer(self)); break;
        case 0x00691CB0: ret = hordeContainSelectRecordId(self, ptrAt(blob, a0), reinterpret_cast<std::uint32_t*>(ptrAt(blob, a1))); break;
        case 0x00691E20: ret = ptrToResult(blob, garrisonContainSlotPointer(self, a0)); break;
        case 0x00691FC0: ret = garrisonContainCopySlotTriple(self, ptrAt(blob, a0), a1); break;
        case 0x00692270: garrisonContainReleaseIfCount(self, a0, a1, a2); break;
        case 0x00692670: openContainRefreshMask(self); break;
        case 0x00692AC0: transportContainRedirect(self, a0, a1); break;
        case 0x00694F00: scavengerSpecialPowerPushScale(self, a0); break;
        case 0x00695290: ret = specialPowerDispatchAnyReady(self); break;
        case 0x00695300: ret = specialPowerDispatchAnyOrPending(self); break;
        case 0x00695370: specialPowerDispatchStoreArg(self, a0); break;
        case 0x00695490: specialPowerDispatchNotifyClear4C(self); break;
        case 0x006954F0: specialPowerDispatchNotifyClear54(self); break;
        case 0x006963F0: specialPowerModuleScaleIfFlag(self, a0, a1); break;
        case 0x006968A0: specialPowerUpdateFirePacked(self, a0); break;
        case 0x006968F0: specialPowerUpdateFirePair(self, a0, a1); break;
        case 0x00696940: specialPowerUpdateFireInsert(self, a0, a1); break;
        case 0x00696990: specialPowerUpdateFireFloat(self, a0, a1, a2); break;
        case 0x006969E0: specialPowerUpdateFireSpread(self, a0, a1, a2); break;
        case 0x00696C50: specialPowerUpdateScaleIfFlag(self, a0, a1); break;
        case 0x00697680: dozerAiUpdateIssueOrder(self); break;
        case 0x00697B50: ret = dockUpdateForwardVisitor(self, a0); break;
        case 0x006983B0:
            ret = queueProductionExitProject(self, reinterpret_cast<std::uint32_t*>(ptrAt(blob, a0)),
                reinterpret_cast<float*>(ptrAt(blob, a1)));
            break;
        case 0x006984E0: ret = static_cast<std::uint32_t>(queueProductionExitIdle(self)); break;
        case 0x00699BC0: aiUpdateStampGoal(self, a0, a1); break;
        case 0x00699FB0: ret = aiUpdateGoalKindAllowed(self); break;
        case 0x0069A040: aiUpdateApplyFacing(self, a0, a1); break;
        case 0x0069C350: ret = bitsOf(invisibilityMemberScale(self)); break;
        case 0x0069C6B0: ret = invisibilityUpdateArgsDiffer(self, a0, a1, a2); break;
        case 0x0069CCD0: ret = ptrToResult(blob, reinterpret_cast<void*>(static_cast<std::uintptr_t>(leechTargetingAttachForward(self, ptrAt(blob, a0))))); break;
        case 0x0069E6F0: ret = ptrToResult(blob, productionUpdateNextReadyLink(self)); break;
        case 0x0069FB60: ret = specialAbilityUpdateKindMatch(self); break;
        case 0x0069FBC0: ret = specialAbilityUpdateIsReady(self); break;
        case 0x0069FCD0: ret = ptrToResult(blob, specialAbilityUpdateResolveOut(self, reinterpret_cast<std::uint32_t*>(ptrAt(blob, a0)))); break;
        case 0x0069FEC0: ret = ptrToResult(blob, stealthDetectorUpdateTouch(self)); break;
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
