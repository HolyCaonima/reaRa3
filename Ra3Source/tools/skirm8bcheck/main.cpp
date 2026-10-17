#include "ra3/gamelogic/SkirmishAI8B.h"

#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

namespace {

constexpr std::uint32_t kLog = 0xC00;
constexpr std::uint32_t kHeap = 0xA80;
constexpr std::uint32_t kHeapEnd = 0xC00;
constexpr int kMaxLog = 24;

struct Blob {
    std::uint8_t* ptr = nullptr;
    std::size_t length = 0;
    std::uint8_t* data() noexcept { return ptr; }
    const std::uint8_t* data() const noexcept { return ptr; }
    std::size_t size() const noexcept { return length; }
    bool empty() const noexcept { return length == 0; }
};

Blob* gBlob = nullptr;
alignas(256) std::uint8_t gStorage[0x2000] = {};
std::uint8_t* gHeap = nullptr;
bool gFailAlloc = false;

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

std::uint32_t asU(const void* pointer) noexcept {
    return static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(pointer));
}

bool inBlob(std::uint32_t value) noexcept {
    if (gBlob == nullptr || gBlob->empty()) {
        return false;
    }
    const auto base = asU(gBlob->data());
    return value >= base && value < base + gBlob->size();
}

bool onStack(std::uint32_t value) noexcept {
    const std::uint32_t here = asU(&value);
    const std::uint32_t dist = value > here ? value - here : here - value;
    return value > 0x10000u && dist < 0x40000u;
}

std::uint32_t enc(std::uint32_t value) noexcept {
    if (value == 0) {
        return 0;
    }
    if (inBlob(value)) {
        return (value - asU(gBlob->data())) | 0x80000000u;
    }
    if (onStack(value)) {
        return 0x40000000u;
    }
    return value;
}

std::uint32_t keySlot(std::uint32_t key) noexcept {
    switch (key) {
    case 0x00DFC6D8:
        return 0;
    case 0x00DFC680:
        return 4;
    case 0x00DFC6D4:
        return 8;
    case 0x00DFC67C:
        return 12;
    default:
        return 0;
    }
}

void logCall(std::uint32_t fn, std::uint32_t ecx, const std::uint32_t* args, int nargs,
             const std::uint32_t* extra) noexcept {
    if (gBlob == nullptr || gBlob->size() < kLog + 4u + 64u) {
        return;
    }
    auto* raw = gBlob->data();
    const auto count = load<std::uint32_t>(raw, kLog);
    if (count >= static_cast<std::uint32_t>(kMaxLog)) {
        return;
    }
    const auto at = kLog + 4u + count * 64u;
    if (at + 64u > gBlob->size()) {
        return;
    }
    store<std::uint32_t>(raw, at, fn);
    store<std::uint32_t>(raw, at + 4, enc(ecx));
    for (int i = 0; i < 6; ++i) {
        store<std::uint32_t>(raw, at + 8 + i * 4, i < nargs ? enc(args[i]) : 0u);
    }
    for (int i = 0; i < 8; ++i) {
        store<std::uint32_t>(raw, at + 32 + i * 4, extra != nullptr ? extra[i] : 0u);
    }
    store<std::uint32_t>(raw, kLog, count + 1u);
}

std::uint32_t readBlob(std::uint32_t address) noexcept {
    if (!inBlob(address) || address + 4 > asU(gBlob->data()) + gBlob->size()) {
        return 0;
    }
    return load<std::uint32_t>(gBlob->data(), static_cast<std::ptrdiff_t>(address - asU(gBlob->data())));
}

void writeBlob(std::uint32_t address, std::uint32_t value) noexcept {
    if (!inBlob(address) || address + 4 > asU(gBlob->data()) + gBlob->size()) {
        return;
    }
    store<std::uint32_t>(gBlob->data(), static_cast<std::ptrdiff_t>(address - asU(gBlob->data())), value);
}

std::uint8_t* allocateStub(std::uint32_t size) noexcept {
    const std::uint32_t args[1] = {size};
    if (gFailAlloc || gHeap == nullptr || gBlob == nullptr) {
        logCall(0x00416820, 0, args, 1, nullptr);
        return nullptr;
    }
    const auto bytes = (size + 3u) & ~3u;
    if (asU(gHeap) + bytes > asU(gBlob->data()) + kHeapEnd) {
        logCall(0x00416820, 0, args, 1, nullptr);
        return nullptr;
    }
    auto* result = gHeap;
    gHeap += bytes;
    const std::uint32_t extra[8] = {enc(asU(result))};
    logCall(0x00416820, 0, args, 1, extra);
    return result;
}

std::uint32_t construct3Stub(std::uint32_t ctor, std::uint8_t* obj, std::uint32_t ai,
                             std::uint32_t player, std::uint32_t record) noexcept {
    const std::uint32_t args[3] = {ai, player, record};
    logCall(ctor, asU(obj), args, 3, nullptr);
    if (obj == nullptr) {
        return 0;
    }
    store<std::uint32_t>(obj, 0, ctor);
    store<std::uint32_t>(obj, 4, enc(ai));
    store<std::uint32_t>(obj, 8, enc(player));
    store<std::uint32_t>(obj, 12, enc(record));
    return asU(obj);
}

std::uint32_t construct4Stub(std::uint32_t ctor, std::uint8_t* obj, std::uint32_t ai,
                             std::uint32_t player, std::uint32_t record, std::uint32_t packed) noexcept {
    const std::uint32_t args[4] = {ai, player, record, packed};
    logCall(ctor, asU(obj), args, 4, nullptr);
    if (obj == nullptr) {
        return 0;
    }
    store<std::uint32_t>(obj, 0, ctor);
    store<std::uint32_t>(obj, 4, enc(ai));
    store<std::uint32_t>(obj, 8, enc(player));
    store<std::uint32_t>(obj, 12, enc(record));
    store<std::uint32_t>(obj, 16, enc(packed));
    return asU(obj);
}

std::uint32_t call0Stub(std::uint32_t fn, std::uint8_t* self) noexcept {
    logCall(fn, asU(self), nullptr, 0, nullptr);
    if (self == nullptr) {
        return 0;
    }
    switch (fn) {
    case 0x0089A740:
    case 0x0089A6D0:
        return asU(self);
    case 0x007E6850:
        return load<std::uint32_t>(self, 0);
    case 0x0089A640:
        return self[0];
    case 0x008A8B20:
    case 0x008B5050:
        return self[1];
    case 0x008A88C0:
        return self[2];
    case 0x00890400:
        return load<std::uint32_t>(self, 8);
    case 0x004CA930:
    default:
        return 0;
    }
}

std::uint32_t call1Stub(std::uint32_t fn, std::uint8_t* self, std::uint32_t arg) noexcept {
    std::uint32_t extra[8] = {};
    if (fn == 0x007E5660 && arg != 0) {
        auto* dest = reinterpret_cast<std::uint8_t*>(static_cast<std::uintptr_t>(arg));
        store<std::uint32_t>(dest, 0, 0x3F800000u);
        store<std::uint32_t>(dest, 4, 0x40000000u);
        store<std::uint32_t>(dest, 8, 0x40400000u);
    }
    const std::uint32_t args[1] = {arg};
    logCall(fn, asU(self), args, 1, fn == 0x007E5660 ? extra : nullptr);
    if (fn == 0x00625090) {
        return asU(self);
    }
    if (self == nullptr) {
        return 0;
    }
    switch (fn) {
    case 0x00885BB0:
        return load<std::uint32_t>(self, 0);
    case 0x008818E0:
        return load<std::uint32_t>(self, 4 + (arg & 1u) * 4u);
    case 0x008A8A90:
        return self[3];
    case 0x008CB3B0:
        return load<std::uint32_t>(self, static_cast<std::ptrdiff_t>(keySlot(arg)));
    case 0x0078D880:
        return load<std::uint32_t>(self, 0);
    case 0x008884E0:
        return asU(self + 0x20);
    case 0x004CBFF0:
        return asU(self);
    default:
        return 0;
    }
}

std::uint32_t call2Stub(std::uint32_t fn, std::uint8_t* self, std::uint32_t arg0, std::uint32_t arg1) noexcept {
    std::uint32_t extra[8] = {};
    if (fn == 0x008ADD50 && arg1 != 0) {
        extra[0] = enc(load<std::uint32_t>(reinterpret_cast<std::uint8_t*>(static_cast<std::uintptr_t>(arg1)), 0));
    }
    const std::uint32_t args[2] = {arg0, arg1};
    logCall(fn, asU(self), args, 2, fn == 0x008ADD50 ? extra : nullptr);
    if (self == nullptr) {
        return 0;
    }
    switch (fn) {
    case 0x008878C0:
    case 0x008908C0:
        return load<std::uint32_t>(self, 0);
    default:
        return 0;
    }
}

std::uint32_t call5Stub(std::uint32_t fn, std::uint8_t* obj, std::uint32_t a0, std::uint32_t a1,
                        std::uint32_t a2, std::uint32_t a3, std::uint32_t a4) noexcept {
    const std::uint32_t args[5] = {a0, a1, a2, a3, a4};
    logCall(fn, asU(obj), args, 5, nullptr);
    if (obj == nullptr) {
        return 0;
    }
    store<std::uint32_t>(obj, 0, fn);
    store<std::uint32_t>(obj, 4, enc(a0));
    store<std::uint32_t>(obj, 8, enc(a1));
    store<std::uint32_t>(obj, 12, enc(a2));
    store<std::uint32_t>(obj, 16, enc(a3));
    store<std::uint32_t>(obj, 20, enc(a4));
    return asU(obj);
}

std::uint32_t call6Stub(std::uint32_t fn, std::uint8_t* self, std::uint32_t a0, std::uint32_t a1,
                        std::uint32_t a2, std::uint32_t a3, std::uint32_t a4, std::uint32_t a5) noexcept {
    std::uint32_t extra[8] = {};
    if (a4 != 0) {
        auto* floats = reinterpret_cast<std::uint8_t*>(static_cast<std::uintptr_t>(a4));
        extra[0] = load<std::uint32_t>(floats, 0);
        extra[1] = load<std::uint32_t>(floats, 4);
        extra[2] = load<std::uint32_t>(floats, 8);
    }
    if (a3 != 0) {
        auto* buffer = reinterpret_cast<std::uint8_t*>(static_cast<std::uintptr_t>(a3));
        extra[3] = load<std::uint32_t>(buffer, 0);
        extra[4] = load<std::uint32_t>(buffer, 4);
        extra[5] = load<std::uint32_t>(buffer, 8);
    }
    const std::uint32_t args[6] = {a0, a1, a2, a3, a4, a5};
    logCall(fn, asU(self), args, 6, extra);
    return 0;
}

void cdecl2Stub(std::uint32_t fn, std::uint32_t arg0, std::uint32_t arg1) noexcept {
    std::uint32_t extra[8] = {};
    if (fn == 0x008BC5B0 && arg1 != 0) {
        extra[0] = enc(load<std::uint32_t>(reinterpret_cast<std::uint8_t*>(static_cast<std::uintptr_t>(arg1)), 0));
    } else if (inBlob(arg1)) {
        const auto salt = 0x51000000u ^ (fn & 0xFFFFu);
        writeBlob(arg1, readBlob(arg1) ^ salt);
    }
    const std::uint32_t args[2] = {arg0, arg1};
    logCall(fn, 0, args, 2, extra);
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
void field6cStub(void* xfer, void* field) noexcept { fieldXor(xfer, field, 0x44440004u); }
void field70Stub(void* xfer, void* field) noexcept { fieldXor(xfer, field, 0x55550005u); }
void field84Stub(void* xfer, void* field) noexcept { fieldXor(xfer, field, 0x66660006u); }

void field88Stub(void* xfer, void* field, std::uint32_t name, std::uint32_t count) noexcept {
    fieldXor(xfer, field, 0x77770000u ^ (name & 0xFFu) ^ count);
}

std::uint32_t stubAddress(int kind) noexcept {
    switch (kind) {
    case 0x18:
        return asU(&versionStub);
    case 0x20:
        return asU(&field20Stub);
    case 0x6C:
        return asU(&field6cStub);
    case 0x70:
        return asU(&field70Stub);
    case 0x84:
        return asU(&field84Stub);
    case 0x88:
        return asU(&field88Stub);
    default:
        return 0;
    }
}

bool isStubAddress(std::uint32_t address) noexcept {
    for (int kind : {0x18, 0x20, 0x6C, 0x70, 0x84, 0x88}) {
        if (address != 0 && address == stubAddress(kind)) {
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

std::uint8_t* ptrAt(Blob& blob, std::uint32_t offset) noexcept {
    if (offset == 0xFFFFFFFFu || offset >= blob.size()) {
        return nullptr;
    }
    return blob.data() + offset;
}

std::uint32_t ptrToResult(const Blob& blob, std::uint32_t value) noexcept {
    if (value == 0) {
        return 0;
    }
    const auto base = asU(blob.data());
    if (value >= base && value < base + blob.size()) {
        return value - base;
    }
    return value;
}

ra3::gamelogic::SkirmishCalls makeCalls(std::uint8_t* xfer, std::uint32_t frame, std::uint8_t* catalog) noexcept {
    ra3::gamelogic::SkirmishCalls calls{};
    calls.allocate = &allocateStub;
    calls.construct3 = &construct3Stub;
    calls.construct4 = &construct4Stub;
    calls.call0 = &call0Stub;
    calls.call1 = &call1Stub;
    calls.call2 = &call2Stub;
    calls.call5 = &call5Stub;
    calls.call6 = &call6Stub;
    calls.cdecl2 = &cdecl2Stub;
    calls.xfer.xfer = xfer;
    calls.xfer.version = &versionStub;
    calls.xfer.field20 = &field20Stub;
    calls.xfer.field6c = &field6cStub;
    calls.xfer.field70 = &field70Stub;
    calls.xfer.field84 = &field84Stub;
    calls.xfer.field88 = &field88Stub;
    calls.gameFrame = frame;
    calls.catalog = catalog;
    return calls;
}

int runCase(std::uint32_t va, std::size_t selfOffset, Blob& blob,
            const std::vector<FixupEntry>& fixups, std::uint32_t a0, std::uint32_t a1, std::uint32_t a2) {
    gBlob = &blob;
    gFailAlloc = a2 == 1u;
    gHeap = blob.size() > kHeap ? blob.data() + kHeap : nullptr;
    const auto baseAddr = asU(blob.data());
    for (const FixupEntry& fix : fixups) {
        if (fix.offset + 4 > blob.size()) {
            continue;
        }
        if (fix.stubKind >= 0) {
            store<std::uint32_t>(blob.data(), static_cast<std::ptrdiff_t>(fix.offset), stubAddress(fix.stubKind));
        } else {
            const auto rel = load<std::uint32_t>(blob.data(), static_cast<std::ptrdiff_t>(fix.offset));
            store<std::uint32_t>(blob.data(), static_cast<std::ptrdiff_t>(fix.offset), baseAddr + rel);
        }
    }

    std::uint8_t* self = selfOffset < blob.size() ? blob.data() + selfOffset : nullptr;
    auto calls = makeCalls(ptrAt(blob, 0x200), a0, ptrAt(blob, a1));
    std::uint32_t ret = 0;

    switch (va) {
    case 0x008B85F0:
        ret = ra3::gamelogic::buildableUnitPhaseIsNotOne(self);
        break;
    case 0x008B8600:
        ret = ra3::gamelogic::buildableUnitPhaseIsOne(self);
        break;
    case 0x008B8610:
        ret = ptrToResult(blob, ra3::gamelogic::copyBuildableUnitAnchor(self, ptrAt(blob, a0)));
        break;
    case 0x008B8FE0:
        ret = ra3::gamelogic::pushAIBuildableStructureRecord(self, ptrAt(blob, a0), a1, calls);
        break;
    case 0x008CCB90:
        ret = ptrToResult(blob, ra3::gamelogic::createAIReactiveDefenseTactic(self, calls));
        break;
    case 0x008CCC10:
        ret = ptrToResult(blob, ra3::gamelogic::createAISimpleDefenseTactic(self, calls));
        break;
    case 0x008CCEB0:
        ret = ptrToResult(blob, ra3::gamelogic::createAIStaticDefenseTactic(self, calls));
        break;
    case 0x008CCF60:
        ret = ptrToResult(blob, ra3::gamelogic::createAIExactExpansionTactic(self, calls));
        break;
    case 0x008CD020:
        ret = ptrToResult(blob, ra3::gamelogic::createAISimpleExpansionTactic(self, calls));
        break;
    case 0x008CD110:
        ret = ptrToResult(blob, ra3::gamelogic::createAIEngineerTactic(self, calls));
        break;
    case 0x008CD4B0:
        ret = ptrToResult(blob, ra3::gamelogic::createAIGarrisonTactic(self, calls));
        break;
    case 0x008CD6E0:
        ret = ptrToResult(blob, ra3::gamelogic::createAISimpleAttackTactic(self, calls));
        break;
    case 0x008CD7B0:
        ret = ptrToResult(blob, ra3::gamelogic::createAISimpleSiegeTactic(self, calls));
        break;
    case 0x008CD8D0:
        ret = ptrToResult(blob, ra3::gamelogic::createAIHuntTactic(self, calls));
        break;
    case 0x008CD950:
        ret = ptrToResult(blob, ra3::gamelogic::createAINullTactic(self, calls));
        break;
    case 0x008CD3F0:
        ret = ptrToResult(blob, ra3::gamelogic::createAIFlankAttackTactic(self, calls));
        break;
    case 0x008CD9F0:
        ret = ptrToResult(blob, ra3::gamelogic::createAIScriptedTactic(self, calls));
        break;
    case 0x008C1340:
        ret = ra3::gamelogic::markAISimpleDefenseContact(self, calls);
        break;
    case 0x008C1370:
        ret = ra3::gamelogic::cacheAIEngineerTacticRecord(self, calls);
        break;
    case 0x008C9A70:
        ret = ra3::gamelogic::gateAIScriptedTactic(self, calls);
        break;
    case 0x008C8DA0:
        ret = ra3::gamelogic::tryAISimpleDefenseActivation(self, calls);
        break;
    case 0x008C8DE0:
        ret = ra3::gamelogic::cacheAIStaticDefensePosition(self, calls);
        break;
    case 0x008C96F0:
        ret = ra3::gamelogic::updateAIFlankAttackTactic(self, calls);
        break;
    case 0x008CCAA0:
        ret = ra3::gamelogic::updateAIReactiveDefenseTactic(self, calls);
        break;
    case 0x008CCB40:
        ret = ra3::gamelogic::clearAIReactiveDefenseAlert(self, calls);
        break;
    case 0x008CD880:
        ret = ra3::gamelogic::clearAIHuntAlert(self, calls);
        break;
    case 0x008CCA40:
        ret = ptrToResult(blob, ra3::gamelogic::aiReactiveDefenseWindowOpen(self, calls));
        break;
    case 0x008CD840:
        ret = ra3::gamelogic::aiHuntWindowOpen(self, calls);
        break;
    case 0x008C9950:
        ret = ra3::gamelogic::continueAISimpleAttack(self, calls);
        break;
    case 0x008C9AA0:
        ret = ra3::gamelogic::chooseAIScriptedTacticMode(self, calls);
        break;
    case 0x008CD760:
        ret = ra3::gamelogic::updateAISimpleSiegeTactic(self, calls);
        break;
    case 0x008CD9D0:
        ret = ra3::gamelogic::updateAIScriptedTactic(self, calls);
        break;
    case 0x008C4D40:
        ret = ra3::gamelogic::bindAIBuildableUnitTarget(self, calls);
        break;
    case 0x008C5140:
        ret = ra3::gamelogic::bindAIBuildableStructureTarget(self, calls);
        break;
    case 0x008C0100:
        ra3::gamelogic::xferSkirmishAISpecialPowerStore(self, calls);
        break;
    case 0x008BDC40:
        ra3::gamelogic::xferAiBuildEfficiencyCalculator(self, calls);
        break;
    case 0x008C3040:
        ra3::gamelogic::xferAINeedsBuilder(self, calls);
        break;
    case 0x008B7860:
        ra3::gamelogic::xferAIRecruitmentManagerDataCache(self, calls);
        break;
    case 0x008C4B10:
        ra3::gamelogic::xferAIUpgradeScience(self, calls);
        break;
    case 0x008C48E0:
        ra3::gamelogic::xferAIUnitUpgrader(self, calls);
        break;
    case 0x008BFF40:
        ra3::gamelogic::xferAIOpeningMover(self, calls);
        break;
    case 0x008C58F0:
        ra3::gamelogic::xferAIRebuildableStructure(self, calls);
        break;
    default:
        std::cerr << "Unknown VA: 0x" << std::hex << va << "\n";
        return 2;
    }

    for (std::size_t off = 0; off + 4 <= blob.size(); off += 4) {
        const auto cur = load<std::uint32_t>(blob.data(), static_cast<std::ptrdiff_t>(off));
        if (cur >= baseAddr && cur < baseAddr + blob.size()) {
            store<std::uint32_t>(blob.data(), static_cast<std::ptrdiff_t>(off), cur - baseAddr);
        } else if (isStubAddress(cur)) {
            for (int kind : {0x18, 0x20, 0x6C, 0x70, 0x84, 0x88}) {
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
              << " st=0x" << std::setw(8) << 0u << "\n";
    gBlob = nullptr;
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
            const auto parsed = parseHex(blobHex);
            if (parsed.size() > sizeof(gStorage)) {
                return 2;
            }
            std::memcpy(gStorage, parsed.data(), parsed.size());
            Blob blob{gStorage, parsed.size()};
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
    std::cerr << "Usage: ra3_skirm8bcheck --batch\n";
    return argc == 1 ? 1 : 2;
}
