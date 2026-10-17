#include "ra3/render/RenderCore99.h"

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

void __stdcall v0AddRef(std::uint8_t* object) noexcept {
    store<std::uint32_t>(object, 0x18, load<std::uint32_t>(object, 0x18) + 1U);
}

void __stdcall v1Release(std::uint8_t* object) noexcept {
    store<std::uint32_t>(object, 0x1C, load<std::uint32_t>(object, 0x1C) + 1U);
}

void __fastcall v2Destroy(std::uint8_t* self, void*, std::uint32_t flag) noexcept {
    store<std::uint32_t>(self, 0x08, flag);
}

std::uint32_t __fastcall v3Classify(std::uint8_t* self, void*, std::uint32_t arg) noexcept {
    store<std::uint32_t>(self, 0x20, arg);
    return arg == 0U ? 0U : load<std::uint32_t>(reinterpret_cast<const std::uint8_t*>(arg), 0);
}

std::uint32_t __fastcall v4Mask(std::uint8_t* self, void*, std::uint32_t arg) noexcept {
    store<std::uint32_t>(self, 0x24, arg);
    if (arg == 0U) {
        return 0U;
    }
    return static_cast<std::uint32_t>(*reinterpret_cast<const std::uint8_t*>(arg));
}

void __fastcall v5Apply(
    std::uint8_t* self, void*, std::uint32_t a, std::uint32_t b, std::uint32_t c) noexcept {
    store<std::uint32_t>(self, 0x30, a);
    store<std::uint32_t>(self, 0x34, b);
    store<std::uint32_t>(self, 0x38, c);
}

std::uint8_t* __fastcall v6Registration(std::uint8_t* self, void*) noexcept {
    return reinterpret_cast<std::uint8_t*>(load<std::uint32_t>(self, 0x40));
}

void __fastcall v7Notify(std::uint8_t* self, void*, std::uint32_t arg) noexcept {
    store<std::uint32_t>(self, 0x44, arg);
}

std::uint32_t __fastcall v8Allocate(
    std::uint8_t* self, void*, std::uint32_t, std::uint32_t, std::uint32_t) noexcept {
    return load<std::uint32_t>(self, 4);
}

void __fastcall v9Bind(
    std::uint8_t* self, void*, std::uint32_t found, std::uint32_t name, std::uint32_t* slot) noexcept {
    if (slot != nullptr) {
        *slot = 0xA1000000U ^ name;
    }
    store<std::uint32_t>(self, 0x20, found);
    store<std::uint32_t>(self, 0x24, name);
}

void __fastcall v10Render(std::uint8_t* self, void*, std::uint32_t a, std::uint32_t b, std::uint32_t c, std::uint32_t d) noexcept {
    store<std::uint32_t>(self, 0x50, a);
    store<std::uint32_t>(self, 0x54, b);
    store<std::uint32_t>(self, 0x58, c);
    store<std::uint32_t>(self, 0x5C, d);
}

std::uint32_t vstubAddress(int kind) noexcept {
    switch (kind) {
    case 0:
        return static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(&v0AddRef));
    case 1:
        return static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(&v1Release));
    case 2:
        return static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(&v2Destroy));
    case 3:
        return static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(&v3Classify));
    case 4:
        return static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(&v4Mask));
    case 5:
        return static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(&v5Apply));
    case 6:
        return static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(&v6Registration));
    case 7:
        return static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(&v7Notify));
    case 8:
        return static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(&v8Allocate));
    case 9:
        return static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(&v9Bind));
    case 10:
        return static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(&v10Render));
    default:
        return 0U;
    }
}

bool isVstubAddress(std::uint32_t addr) noexcept {
    for (int kind = 0; kind <= 10; ++kind) {
        if (addr != 0U && addr == vstubAddress(kind)) {
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
    int vstubKind = -1;
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
                entry.vstubKind = std::atoi(item.substr(1, at - 1).c_str());
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

std::uint8_t* gEngineService = nullptr;

std::uint8_t* engineService() noexcept {
    return gEngineService;
}

void applyRegistry(std::vector<std::uint8_t>& blob, std::uint32_t secondId, std::uint32_t queryHit) noexcept {
    ra3::render::ShaderRegistryView view{};
    const auto begin = load<std::uint32_t>(blob.data(), 0);
    const auto end = load<std::uint32_t>(blob.data(), 4);
    view.begin = blob.data() + begin;
    view.end = blob.data() + end;
    view.cacheKey = load<std::uint32_t>(blob.data(), 8);
    view.ready = blob[0x10];
    view.secondConstantId = secondId == 0U ? 0x42U : secondId;
    view.constantQueryHit = queryHit == 0U ? nullptr : reinterpret_cast<std::uint8_t*>(queryHit);
    ra3::render::setShaderRegistryView(view);
}

int runCase(
    std::uint32_t va,
    std::size_t selfOffset,
    std::vector<std::uint8_t>& blob,
    const std::vector<FixupEntry>& fixups,
    std::uint32_t a0,
    std::uint32_t a1,
    std::uint32_t a2,
    std::uint16_t fpcw) {
    using namespace ra3::render;
    const auto baseAddr = static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(blob.data()));
    for (const FixupEntry& fix : fixups) {
        if (fix.offset + 4 <= blob.size()) {
            if (fix.vstubKind >= 0) {
                store<std::uint32_t>(blob.data(), fix.offset, vstubAddress(fix.vstubKind));
            } else {
                const auto rel = load<std::uint32_t>(blob.data(), fix.offset);
                store<std::uint32_t>(blob.data(), fix.offset, baseAddr + rel);
            }
        }
    }

    std::uint16_t savedCw = 0x037F;
    __asm {
        fnstcw savedCw
        fldcw fpcw
    }

    std::uint8_t* self = selfOffset < blob.size() ? (blob.data() + selfOffset) : nullptr;
    std::uint32_t ret = 0;
    ra3::render::RenderBinServices services{};
    services.service = &engineService;
    services.reserve = nullptr;
    services.submit = nullptr;
    setRenderBinServices(services);

    switch (va) {
    case 0x00990080:
        removePartitionObject(self, ptrAt(blob, a0));
        break;
    case 0x00990100:
        clearPartitionBuckets(self);
        if (self != nullptr &&
            ((load<std::uint32_t>(self, 0x1C) - load<std::uint32_t>(self, 0x18)) & 0xFFFFFFF8U) == 0U) {
            ret = selfOffset + 0x18;
        }
        break;
    case 0x009901E0:
        setPartitionWorldBounds(self, reinterpret_cast<const float*>(ptrAt(blob, a0)));
        ret = ptrToResult(blob, ptrAt(blob, a0));
        break;
    case 0x00990620:
        resetPartitionBounds(self);
        break;
    case 0x00993120: {
        const auto link = self == nullptr ? 0U : load<std::uint32_t>(self, 0x14);
        const auto prev = self == nullptr ? 0U : load<std::uint32_t>(self, 0x0C);
        unlinkCollisionEntry(self);
        ret = ptrToResult(blob, reinterpret_cast<void*>(link != 0U ? link : prev));
        break;
    }
    case 0x00993360:
        sweepIdleCollisionNodes(self);
        ret = 0x493;
        break;
    case 0x00993600:
        retireCollisionHashNode(self, reinterpret_cast<const std::uint32_t*>(ptrAt(blob, a0)));
        break;
    case 0x009937E0:
        retireCollisionObjectList(self, ptrAt(blob, a0));
        break;
    case 0x009938A0:
        updateCollisionRegistration(self, ptrAt(blob, a0));
        break;
    case 0x00995D00:
        matrix3x3ToQuaternion(reinterpret_cast<float*>(ptrAt(blob, a0)), reinterpret_cast<const float*>(ptrAt(blob, a1)));
        ret = ptrToResult(blob, ptrAt(blob, a0));
        break;
    case 0x00997EF0:
        ret = ptrToResult(blob, fillShaderParamRange(ptrAt(blob, a0), ptrAt(blob, a1), ptrAt(blob, a2)));
        break;
    case 0x00997FD0:
        ret = ptrToResult(blob, copyShaderParamsForward(ptrAt(blob, a0), ptrAt(blob, a1), ptrAt(blob, a2)));
        break;
    case 0x009980D0:
        ret = ptrToResult(blob, copyShaderParamsBackward(ptrAt(blob, a0), ptrAt(blob, a1), ptrAt(blob, a2)));
        break;
    case 0x009984F0:
        ret = ptrToResult(blob, defaultShaderParam(self));
        break;
    case 0x00998F40:
        ret = ptrToResult(blob, assignShaderParamRelaxed(self, ptrAt(blob, a0)));
        break;
    case 0x0099A8E0:
        readNestedNameWord(self, reinterpret_cast<std::uint32_t*>(ptrAt(blob, a0)));
        ret = ptrToResult(blob, ptrAt(blob, a0));
        break;
    case 0x0099B010:
        ret = ptrToResult(blob, assignRefCountedPointer(self, ptrAt(blob, a0)));
        break;
    case 0x0099BD10:
        copySubObjectRecords(reinterpret_cast<void**>(self), ptrAt(blob, a0), ptrAt(blob, a1), ptrAt(blob, a2));
        break;
    case 0x0099BD70:
        ret = ptrToResult(blob, copyExtendedRecords(reinterpret_cast<void**>(self), ptrAt(blob, a0), ptrAt(blob, a1), ptrAt(blob, a2)));
        break;
    case 0x0099BE10:
        ret = ptrToResult(blob, assignShaderParam(self, ptrAt(blob, a0)));
        break;
    case 0x0099C0A0:
        ret = ptrToResult(blob, constructShaderParamRange(reinterpret_cast<void**>(self), ptrAt(blob, a0), ptrAt(blob, a1), ptrAt(blob, a2)));
        break;
    case 0x0099C160:
        ret = ptrToResult(blob, copyExtendedRecordsBackward(ptrAt(blob, a0), ptrAt(blob, a1), ptrAt(blob, a2)));
        break;
    case 0x0099C290:
        composeShaderTransform(
            reinterpret_cast<const float*>(ptrAt(blob, a0)),
            reinterpret_cast<const float*>(ptrAt(blob, a1)),
            reinterpret_cast<float*>(ptrAt(blob, a2)));
        ret = ptrToResult(blob, ptrAt(blob, a2));
        break;
    case 0x0099C440:
        composeShaderTransformInPlace(self, reinterpret_cast<const float*>(ptrAt(blob, a0)));
        if (self != nullptr) {
            ret = load<std::uint32_t>(self, 0x0C);
        }
        break;
    case 0x0099F1C0:
        applyRegistry(blob, a2, 0);
        ret = ptrToResult(blob, resolveRegisteredShader(reinterpret_cast<std::uint32_t*>(ptrAt(blob, a0)), reinterpret_cast<const std::uint32_t*>(ptrAt(blob, a1))));
        break;
    case 0x0099F310:
        applyRegistry(blob, a0, a1);
        bindShaderConstantIds(self);
        if (self != nullptr) {
            ret = ptrToResult(blob, reinterpret_cast<void*>(load<std::uint32_t>(self, 0)));
        }
        break;
    case 0x009A0500:
        ret = ptrToResult(blob, replicateShaderParam(ptrAt(blob, a0), a1, ptrAt(blob, a2)));
        break;
    case 0x009A0640:
        ret = ptrToResult(blob, moveSubObjectRecordsBackward(ptrAt(blob, a0), ptrAt(blob, a1), ptrAt(blob, a2)));
        break;
    case 0x009A2540: {
        SemanticNameCache cache{};
        cache.ready = blob[0] != 0;
        const auto text = load<std::uint32_t>(blob.data(), 4);
        cache.text = text == 0U ? nullptr : blob.data() + text;
        setSemanticNameCache(cache);
        ret = ptrToResult(blob, reinterpret_cast<void*>(static_cast<std::uintptr_t>(shaderSemanticNames())));
        break;
    }
    case 0x009A2DF0:
        refreshShaderParamChain(self, reinterpret_cast<const float*>(ptrAt(blob, a0)));
        if (self != nullptr) {
            const auto first = load<std::uint32_t>(self, 8);
            ret = ptrToResult(blob, reinterpret_cast<void*>(static_cast<std::uintptr_t>(first) + 0x24U));
        }
        break;
    case 0x009A82B0:
        skinShaderInstances(self);
        break;
    case 0x009A8EA0:
        applyRegistry(blob, 0, 0);
        ret = bindShaderAsset(ptrAt(blob, a0), reinterpret_cast<const std::uint32_t*>(ptrAt(blob, a1))) ? 1U : 0U;
        break;
    case 0x009AB8E0:
        ret = resizeShaderParamVector(self, a0);
        break;
    case 0x009ACB60:
        ret = ptrToResult(blob, importShaderParamBlock(self, ptrAt(blob, a0)));
        break;
    case 0x009ACFE0:
        ret = ptrToResult(blob, appendSubObjectRecord(self, ptrAt(blob, a0)));
        break;
    case 0x00992F70:
        ret = ptrToResult(blob, findWideCollisionNode(self, reinterpret_cast<const std::uint32_t*>(ptrAt(blob, a0))));
        break;
    case 0x00997720:
        ret = runVisibleObjectsCommand(ptrAt(blob, a0), ptrAt(blob, a1), 0, 0) ? 1U : 0U;
        break;
    case 0x0099A790:
        ret = parseVisibleObjectsCommand(ptrAt(blob, a0), ptrAt(blob, a1), ptrAt(blob, a2)) ? 1U : 0U;
        break;
    case 0x0099A850:
        ret = runRenderBinCommand(ptrAt(blob, a0), ptrAt(blob, a1), ptrAt(blob, a2), self) ? 1U : 0U;
        break;
    case 0x009A1990: {
        gEngineService = ptrAt(blob, load<std::uint32_t>(blob.data(), 0));
        services.reserve = [](void*, std::uint32_t, std::uint32_t) {};
        services.submit = [](void* console, std::uint32_t, void*, std::uint32_t, std::uint32_t) {
            auto* bytes = reinterpret_cast<std::uint8_t*>(console);
            store<std::uint32_t>(bytes, 0x90, load<std::uint32_t>(bytes, 0x90) + 1U);
        };
        services.allocate = [](void* service, std::uint32_t, std::uint32_t, std::uint32_t) -> std::uint8_t* {
            return reinterpret_cast<std::uint8_t*>(load<std::uint32_t>(reinterpret_cast<std::uint8_t*>(service), 4));
        };
        setRenderBinServices(services);
        ret = parseRenderBinCommand(ptrAt(blob, a0), ptrAt(blob, a1), ptrAt(blob, a2)) ? 1U : 0U;
        break;
    }
    default:
        __asm { fldcw savedCw }
        std::cerr << "Unknown VA: 0x" << std::hex << va << "\n";
        return 2;
    }

    __asm {
        fldcw savedCw
    }

    for (std::size_t off = 0; off + 4 <= blob.size(); off += 4) {
        const auto cur = load<std::uint32_t>(blob.data(), off);
        if (cur >= baseAddr && cur < baseAddr + blob.size()) {
            store<std::uint32_t>(blob.data(), off, cur - baseAddr);
        } else if (isVstubAddress(cur)) {
            for (int kind = 0; kind <= 10; ++kind) {
                if (cur == vstubAddress(kind)) {
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
            std::string fpcwStr;
            if (!(iss >> vaStr >> selfStr >> blobHex >> fixStr >> a0Str >> a1Str >> a2Str)) {
                return 1;
            }
            std::uint16_t fpcw = 0x037F;
            if (iss >> fpcwStr) {
                fpcw = static_cast<std::uint16_t>(std::strtoul(fpcwStr.c_str(), nullptr, 0));
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
                static_cast<std::uint32_t>(std::strtoul(a2Str.c_str(), nullptr, 0)),
                fpcw);
            if (rc != 0) {
                return rc;
            }
        }
        return 0;
    }
    std::cerr << "Usage: ra3_render99check --batch\n";
    return argc == 1 ? 1 : 2;
}
