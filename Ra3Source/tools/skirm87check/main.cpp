#include "ra3/gameclient/SkirmishClient87.h"
#include "ra3/gamelogic/SkirmishAI87.h"

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

void fieldXor(void* xfer, void* field, std::uint32_t salt) noexcept {
    if (field != nullptr) {
        auto* raw = static_cast<std::uint8_t*>(field);
        store<std::uint32_t>(raw, 0, load<std::uint32_t>(raw, 0) ^ salt);
    }
    auto* host = static_cast<std::uint8_t*>(xfer);
    store<std::uint32_t>(host, 0x28, load<std::uint32_t>(host, 0x28) + 1u);
}

void versionStub(void* xfer, std::uint8_t* pair) noexcept {
    auto* host = static_cast<std::uint8_t*>(xfer);
    if (host[0x20] != 0) {
        pair[1] = host[0x21];
    }
    store<std::uint32_t>(host, 0x24, load<std::uint32_t>(host, 0x24) + 1u);
}

void field20Stub(void* xfer, void* field) noexcept { fieldXor(xfer, field, 0x11110001u); }
void field58Stub(void* xfer, void* field) noexcept { fieldXor(xfer, field, 0x22220002u); }
void field64Stub(void* xfer, void* field) noexcept { fieldXor(xfer, field, 0x33330003u); }
void field6cStub(void* xfer, void* field) noexcept { fieldXor(xfer, field, 0x44440004u); }
void field70Stub(void* xfer, void* field) noexcept { fieldXor(xfer, field, 0x55550005u); }
void field84Stub(void* xfer, void* field) noexcept { fieldXor(xfer, field, 0x66660006u); }

void field88Stub(void* xfer, void* field, std::uint32_t name, std::uint32_t count) noexcept {
    fieldXor(xfer, field, 0x77770000u ^ (name & 0xFFu) ^ count);
}

std::uint32_t phaseStub(std::uint8_t* self, std::uint32_t phase) noexcept {
    store<std::uint32_t>(self, 0x18, phase);
    return 0xA11u;
}

std::uint32_t stubAddress(int kind) noexcept {
    switch (kind) {
    case 0x18:
        return static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(&versionStub));
    case 0x20:
        return static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(&field20Stub));
    case 0x28:
        return static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(&phaseStub));
    case 0x58:
        return static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(&field58Stub));
    case 0x64:
        return static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(&field64Stub));
    case 0x6C:
        return static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(&field6cStub));
    case 0x70:
        return static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(&field70Stub));
    case 0x84:
        return static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(&field84Stub));
    case 0x88:
        return static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(&field88Stub));
    default:
        return 0u;
    }
}

bool isStubAddress(std::uint32_t address) noexcept {
    for (int kind : {0x18, 0x20, 0x28, 0x58, 0x64, 0x6C, 0x70, 0x84, 0x88}) {
        if (address != 0u && address == stubAddress(kind)) {
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
                entry.stubKind = static_cast<int>(std::strtoul(item.substr(1, at - 1).c_str(), nullptr, 16));
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
    if (offset == 0xFFFFFFFFu || offset >= blob.size()) {
        return nullptr;
    }
    return blob.data() + offset;
}

std::uint32_t ptrToResult(const std::vector<std::uint8_t>& blob, std::uint32_t value) noexcept {
    if (value == 0u) {
        return 0u;
    }
    const auto base = static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(blob.data()));
    if (value >= base && value < base + blob.size()) {
        return value - base;
    }
    return value;
}

template <typename Ops>
Ops makeOps(std::uint8_t* xfer) noexcept {
    Ops ops{};
    ops.xfer = xfer;
    ops.version = &versionStub;
    ops.field20 = &field20Stub;
    ops.field58 = &field58Stub;
    ops.field64 = &field64Stub;
    ops.field6c = &field6cStub;
    ops.field70 = &field70Stub;
    ops.field84 = &field84Stub;
    ops.field88 = &field88Stub;
    return ops;
}

int runCase(
    std::uint32_t va,
    std::size_t selfOffset,
    std::vector<std::uint8_t>& blob,
    const std::vector<FixupEntry>& fixups,
    std::uint32_t a0,
    std::uint32_t a1,
    std::uint32_t a2) {
    const auto baseAddr = static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(blob.data()));
    for (const FixupEntry& fix : fixups) {
        if (fix.offset + 4 > blob.size()) {
            continue;
        }
        if (fix.stubKind >= 0) {
            store<std::uint32_t>(blob.data(), static_cast<std::ptrdiff_t>(fix.offset),
                                 stubAddress(fix.stubKind));
        } else {
            const auto rel = load<std::uint32_t>(blob.data(), static_cast<std::ptrdiff_t>(fix.offset));
            store<std::uint32_t>(blob.data(), static_cast<std::ptrdiff_t>(fix.offset), baseAddr + rel);
        }
    }

    std::uint8_t* self = selfOffset < blob.size() ? blob.data() + selfOffset : nullptr;
    std::uint32_t ret = 0;
    std::uint32_t st = 0;
    using G = ra3::gamelogic::XferOps;
    using C = ra3::gameclient::XferOps;

    switch (va) {
    case 0x008865E0:
        ret = ra3::gamelogic::applyStaticDefenseTacticRecord(ptrAt(blob, a0));
        break;
    case 0x00886680:
        ret = ptrToResult(blob, ra3::gamelogic::applyHuntTacticFlags(ptrAt(blob, a0)));
        break;
    case 0x0087D2B0:
        ret = ptrToResult(blob, ra3::gamelogic::applySimpleDefenseTacticMode(ptrAt(blob, a0)));
        break;
    case 0x00886660:
        ret = ptrToResult(blob, ra3::gamelogic::applySimpleSiegeTacticFlag(ptrAt(blob, a0)));
        break;
    case 0x0087D2A0:
        ret = ptrToResult(blob, ra3::gamelogic::clearSimpleDefenseAlertByte(self));
        break;
    case 0x00881820:
        ret = ra3::gamelogic::tacticArgumentPresent(a0);
        break;
    case 0x00882E20:
        ret = ptrToResult(blob, ra3::gamelogic::copyRebuildableStructurePosition(self, ptrAt(blob, a0)));
        break;
    case 0x0087EDD0:
        ret = ptrToResult(blob, ra3::gamelogic::copyBuildableStructureAnchor(self, ptrAt(blob, a0)));
        break;
    case 0x00882E60:
        ret = ptrToResult(blob, ra3::gamelogic::rebuildableStructurePositionPointer(self));
        break;
    case 0x00882E90:
        ret = ptrToResult(blob, ra3::gamelogic::rebuildableStructureRadiusPointer(self));
        break;
    case 0x00882E70: {
        const auto radius = ra3::gamelogic::rebuildableStructureRadiusOrZero(self);
        ret = ptrToResult(blob, radius.pointer);
        st = radius.floatBits;
        break;
    }
    case 0x0087D860:
        ra3::gamelogic::copyBuildableStructureScore(self);
        break;
    case 0x0087F450:
        ret = ra3::gamelogic::aiRedAlertFrameReached(self, a0);
        break;
    case 0x00885700:
        ret = ra3::gamelogic::armAIRedAlertFrame(self, a0, a1);
        break;
    case 0x0087CB80:
        ret = ra3::gamelogic::attackObjectPresent(self);
        break;
    case 0x0087D3E0: {
        ra3::gamelogic::RandomWords words{};
        for (int index = 0; index < 6; ++index) {
            words.word[index] = load<std::uint32_t>(blob.data(), 0x400 + index * 4);
        }
        ret = ra3::gamelogic::rollAITacticActivation(ptrAt(blob, a0), words);
        break;
    }
    case 0x00885BB0:
        ret = ptrToResult(blob, ra3::gamelogic::findSimpleExpansionRecord(self, a0));
        break;
    case 0x00886640:
        ret = ptrToResult(blob, ra3::gamelogic::cacheSimpleExpansionRecord(self));
        break;
    case 0x0087D870:
        ret = ra3::gamelogic::ensureBuildablePhaseFour(self, &phaseStub);
        break;
    case 0x0087D760: {
        auto ops = makeOps<G>(ptrAt(blob, a0));
        ra3::gamelogic::xferPrerequisiteBuilder(self, ops);
        break;
    }
    case 0x0087D7D0: {
        auto ops = makeOps<G>(ptrAt(blob, a0));
        ra3::gamelogic::xferUnitBuilder(self, ops);
        break;
    }
    case 0x0087D890: {
        auto ops = makeOps<G>(ptrAt(blob, a0));
        ra3::gamelogic::xferBuildableStructureFactory(self, ops);
        break;
    }
    case 0x0087D940: {
        auto ops = makeOps<G>(ptrAt(blob, a0));
        ra3::gamelogic::xferEmergencyManager(self, ops);
        break;
    }
    case 0x0087DC90: {
        auto ops = makeOps<G>(ptrAt(blob, a0));
        ra3::gamelogic::xferSubsystemEnabler(self, ops);
        break;
    }
    case 0x0087C730: {
        auto ops = makeOps<G>(ptrAt(blob, a0));
        ra3::gamelogic::xferEconomyManager(self, ops);
        break;
    }
    case 0x0087C830: {
        auto ops = makeOps<G>(ptrAt(blob, a0));
        ra3::gamelogic::xferRedAlertManager(self, ops);
        break;
    }
    case 0x0087C400: {
        auto ops = makeOps<G>(ptrAt(blob, a0));
        ra3::gamelogic::xferAIBank(self, ops);
        break;
    }
    case 0x0087C4D0: {
        auto ops = makeOps<G>(ptrAt(blob, a0));
        ra3::gamelogic::xferBuildHeuristicStore(self, ops);
        break;
    }
    case 0x008822E0: {
        auto ops = makeOps<G>(ptrAt(blob, a0));
        ra3::gamelogic::xferSharedBuildRecord(self, ops);
        break;
    }
    case 0x00887870: {
        auto ops = makeOps<G>(ptrAt(blob, a0));
        ra3::gamelogic::xferBuildableUnit(self, ops);
        break;
    }
    case 0x00887BE0: {
        auto ops = makeOps<G>(ptrAt(blob, a0));
        ra3::gamelogic::xferAIUpgrade(self, ops);
        break;
    }
    case 0x00887C90: {
        auto ops = makeOps<G>(ptrAt(blob, a0));
        ra3::gamelogic::xferUpgradeOpeningMove(self, ops);
        break;
    }
    case 0x0087FD80: {
        auto ops = makeOps<G>(ptrAt(blob, a0));
        ra3::gamelogic::xferSpecialPowerAttackObject(self, ops);
        break;
    }
    case 0x0087D6E0: {
        auto ops = makeOps<G>(ptrAt(blob, a0));
        ra3::gamelogic::xferStrategyManager(self, ops);
        break;
    }
    case 0x00873E00: {
        auto ops = makeOps<C>(ptrAt(blob, a0));
        ra3::gameclient::xferAnimationSoundClientBehavior(self, ops);
        break;
    }
    case 0x00873EB0: {
        auto ops = makeOps<C>(ptrAt(blob, a0));
        ra3::gameclient::xferModelConditionClientBehavior(self, ops);
        break;
    }
    case 0x00874170: {
        auto ops = makeOps<C>(ptrAt(blob, a0));
        ra3::gameclient::xferRandomSoundSelector(self, ops);
        break;
    }
    case 0x00874470: {
        auto ops = makeOps<C>(ptrAt(blob, a0));
        ra3::gameclient::xferLargeGroupAudio(self, ops);
        break;
    }
    case 0x00874250: {
        auto ops = makeOps<C>(ptrAt(blob, a0));
        ra3::gameclient::xferBeaconClientUpdate(self, ops);
        break;
    }
    case 0x00874160:
        ret = ptrToResult(blob, ra3::gameclient::clientBehaviorRecordOrNull(self));
        break;
    case 0x00874870:
        ret = ptrToResult(blob, ra3::gameclient::audioLoopRecordOrNull(self));
        break;
    default:
        std::cerr << "Unknown VA: 0x" << std::hex << va << "\n";
        return 2;
    }

    (void)a2;

    for (std::size_t off = 0; off + 4 <= blob.size(); off += 4) {
        const auto cur = load<std::uint32_t>(blob.data(), static_cast<std::ptrdiff_t>(off));
        if (cur >= baseAddr && cur < baseAddr + blob.size()) {
            store<std::uint32_t>(blob.data(), static_cast<std::ptrdiff_t>(off), cur - baseAddr);
        } else if (isStubAddress(cur)) {
            for (int kind : {0x18, 0x20, 0x28, 0x58, 0x64, 0x6C, 0x70, 0x84, 0x88}) {
                if (cur == stubAddress(kind)) {
                    store<std::uint32_t>(blob.data(), static_cast<std::ptrdiff_t>(off),
                                         0xF0000000u | static_cast<std::uint32_t>(kind));
                    break;
                }
            }
        }
    }

    std::cout << "ret=0x" << std::hex << std::setw(8) << std::setfill('0') << ret
              << " blob=" << toHex(blob.data(), blob.size())
              << " st=0x" << std::setw(8) << st << "\n";
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
            if (!(iss >> vaStr >> selfStr >> blobHex >> fixStr >> a0Str >> a1Str >> a2Str)) {
                return 1;
            }
            const auto va = static_cast<std::uint32_t>(std::strtoul(vaStr.c_str(), nullptr, 0));
            const auto selfOffset = static_cast<std::size_t>(std::strtoul(selfStr.c_str(), nullptr, 0));
            auto blob = parseHex(blobHex);
            const int rc = runCase(va, selfOffset, blob, parseFixups(fixStr),
                                   static_cast<std::uint32_t>(std::strtoul(a0Str.c_str(), nullptr, 0)),
                                   static_cast<std::uint32_t>(std::strtoul(a1Str.c_str(), nullptr, 0)),
                                   static_cast<std::uint32_t>(std::strtoul(a2Str.c_str(), nullptr, 0)));
            if (rc != 0) {
                return rc;
            }
        }
        return 0;
    }
    std::cerr << "Usage: ra3_skirm87check --batch\n";
    return argc == 1 ? 1 : 2;
}
