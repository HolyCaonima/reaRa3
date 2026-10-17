#include "ra3/render/RenderScript9B.h"

#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <malloc.h>
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

std::uint32_t __fastcall vBegin(std::uint8_t* self, void*, std::uint32_t code, std::uint32_t text) noexcept {
    store<std::uint32_t>(self, 0x90, code);
    store<std::uint32_t>(self, 0x94, text);
    return 0;
}

std::uint32_t __fastcall vEnd(std::uint8_t* self, void*, std::uint32_t text) noexcept {
    store<std::uint32_t>(self, 0x98, text);
    return 0;
}

std::uint32_t __fastcall vMarker(std::uint8_t* self, void*, std::uint32_t code, std::uint32_t text) noexcept {
    store<std::uint32_t>(self, 0x9C, code);
    store<std::uint32_t>(self, 0xA0, text);
    return 0;
}

std::uint32_t __fastcall vQuery(std::uint8_t* self, void*) noexcept {
    return load<std::uint32_t>(self, 0x10);
}

std::uint32_t __fastcall vCast(std::uint8_t* self, void*, std::uint32_t format) noexcept {
    store<std::uint32_t>(self, 0x64, format);
    return 0;
}

std::uint32_t __fastcall vStateFirst(std::uint8_t*, void*, std::uint32_t device, std::uint32_t, std::uint32_t name) noexcept {
    auto* bytes = reinterpret_cast<std::uint8_t*>(device);
    store<std::uint32_t>(bytes, 0x2C, name);
    return load<std::uint32_t>(bytes, 0x10);
}

std::uint32_t __fastcall vStateSecond(std::uint8_t*, void*, std::uint32_t device, std::uint32_t found, std::uint32_t value) noexcept {
    auto* bytes = reinterpret_cast<std::uint8_t*>(device);
    store<std::uint32_t>(bytes, 0x18, found);
    store<std::uint32_t>(bytes, 0x1C, value);
    return load<std::uint32_t>(bytes, 0x14);
}

std::uint32_t __fastcall vSizeFirst(std::uint8_t*, void*, std::uint32_t device, std::uint32_t name) noexcept {
    auto* bytes = reinterpret_cast<std::uint8_t*>(device);
    store<std::uint32_t>(bytes, 0x2C, name);
    return load<std::uint32_t>(bytes, 0x28);
}

std::uint32_t __fastcall vSizeSecond(std::uint8_t*, void*, std::uint32_t device, std::uint32_t, std::uint32_t buffer) noexcept {
    auto* bytes = reinterpret_cast<std::uint8_t*>(device);
    auto* out = reinterpret_cast<std::uint8_t*>(buffer);
    store<std::uint32_t>(out, 0, load<std::uint32_t>(bytes, 0x20));
    store<std::uint32_t>(out, 4, load<std::uint32_t>(bytes, 0x24));
    return 0;
}

std::uint32_t __fastcall vRelease(std::uint8_t*, void*, std::uint32_t object) noexcept {
    store<std::uint32_t>(reinterpret_cast<std::uint8_t*>(object), 0x20, 0x11u);
    return 0x11u;
}

std::uint32_t __fastcall vReset(std::uint8_t* self, void*) noexcept {
    store<std::uint32_t>(self, 0x28, 1u);
    return 0;
}

bool textIsOk(std::uint32_t name) noexcept {
    if (name == 0u) {
        return false;
    }
    return std::strcmp(reinterpret_cast<const char*>(name), "ok") == 0;
}

std::uint32_t __fastcall vFloatVar(std::uint8_t*, void*, std::uint32_t name, std::uint32_t dest) noexcept {
    if (!textIsOk(name)) {
        return 0;
    }
    store<std::uint32_t>(reinterpret_cast<std::uint8_t*>(dest), 0, 0x3F800000u);
    return 1u;
}

std::uint32_t __fastcall vIntVar(std::uint8_t*, void*, std::uint32_t name, std::uint32_t dest) noexcept {
    if (!textIsOk(name)) {
        return 0;
    }
    store<std::uint32_t>(reinterpret_cast<std::uint8_t*>(dest), 0, 7u);
    return 1u;
}

std::uint32_t __fastcall vIntern(std::uint8_t* pool, void*, std::uint32_t name) noexcept {
    store<std::uint32_t>(pool, 0x10, name);
    return name == 0u ? 0u : 0x42u;
}

std::uint32_t __fastcall vReleaseKey(std::uint8_t* pool, void*, std::uint32_t key) noexcept {
    store<std::uint32_t>(pool, 0x14, key);
    return 0;
}

std::uint32_t __fastcall vCompare(std::uint8_t* self, void*, std::uint32_t left, std::uint32_t right) noexcept {
    store<std::uint32_t>(self, 0x18, left);
    store<std::uint32_t>(self, 0x1C, right);
    return left == (right ^ 0x01000000u) ? 1u : 0u;
}

std::uint32_t vstubAddress(int kind) noexcept {
    switch (kind) {
    case 0: return static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(&vBegin));
    case 1: return static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(&vEnd));
    case 2: return static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(&vMarker));
    case 3: return static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(&vQuery));
    case 4: return static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(&vCast));
    case 5: return static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(&vStateFirst));
    case 6: return static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(&vStateSecond));
    case 7: return static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(&vSizeFirst));
    case 8: return static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(&vSizeSecond));
    case 9: return static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(&vRelease));
    case 10: return static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(&vReset));
    case 11: return static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(&vFloatVar));
    case 12: return static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(&vIntVar));
    case 13: return static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(&vIntern));
    case 14: return static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(&vReleaseKey));
    case 15: return static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(&vCompare));
    default: return 0u;
    }
}

bool isVstubAddress(std::uint32_t address) noexcept {
    for (int kind = 0; kind <= 15; ++kind) {
        if (address != 0u && address == vstubAddress(kind)) {
            return true;
        }
    }
    return false;
}

std::uint32_t clearSurface(std::uint8_t* device, std::uint32_t flags, std::uint32_t color, std::uint32_t depth, std::uint32_t stencil) noexcept {
    store<std::uint32_t>(device, 0xB0, flags);
    store<std::uint32_t>(device, 0xB4, color == 0u ? 0u : load<std::uint32_t>(reinterpret_cast<std::uint8_t*>(color), 0));
    store<std::uint32_t>(device, 0xB8, depth);
    store<std::uint32_t>(device, 0xBC, stencil);
    return 0;
}

std::uint32_t setRenderTarget(std::uint8_t* device, std::uint32_t first, std::uint32_t second) noexcept {
    store<std::uint32_t>(device, 0xC0, first);
    store<std::uint32_t>(device, 0xC4, second);
    return 0;
}

std::uint32_t bindShader(std::uint8_t* shader, std::uint32_t arg) noexcept {
    store<std::uint32_t>(shader, 8, arg);
    return 0;
}

std::uint32_t vectorReserve(std::uint8_t*, std::uint32_t, std::uint32_t) noexcept { return 0; }
std::uint32_t vectorShrink(std::uint8_t*) noexcept { return 0; }

std::uint32_t lookupFormat(std::uint8_t*, std::uint32_t text) noexcept {
    if (text == 0u) {
        return 0;
    }
    const char* bytes = reinterpret_cast<const char*>(text);
    return bytes[0] == 'D' ? 0x21u : 0u;
}

std::uint32_t nameEquals(std::uint8_t* entry, std::uint32_t left, std::uint32_t right) noexcept {
    store<std::uint32_t>(entry, 0x10, left);
    store<std::uint32_t>(entry, 0x14, right);
    return left == (right ^ 1u) ? 1u : 0u;
}

std::uint32_t ensurePass(std::uint8_t* self, std::uint32_t index) noexcept {
    store<std::uint32_t>(self, 0x40, index);
    return 0;
}

std::uint32_t bindConstants(std::uint8_t*, std::uint32_t block) noexcept {
    store<std::uint32_t>(reinterpret_cast<std::uint8_t*>(block), 0x70, 0xABu);
    return 0;
}

std::uint32_t refreshConstants(std::uint8_t*, std::uint32_t block) noexcept {
    store<std::uint32_t>(reinterpret_cast<std::uint8_t*>(block), 0x6C, 0x11u);
    return 0;
}

std::uint32_t flushTarget(std::uint8_t*, std::uint32_t block) noexcept {
    store<std::uint32_t>(reinterpret_cast<std::uint8_t*>(block), 0x70, 0xCDu);
    return 0;
}

std::uint32_t logMessage(std::uint32_t, std::uint32_t) noexcept { return 0; }

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
    int pool = 0;
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
        const std::string item = text.substr(start, comma == std::string::npos ? std::string::npos : comma - start);
        if (!item.empty()) {
            FixupEntry entry{};
            if (item[0] == 'p' && item.find('@') != std::string::npos) {
                entry.pool = 1;
                entry.offset = static_cast<std::size_t>(std::strtoul(item.c_str() + item.find('@') + 1, nullptr, 0));
            } else if ((item[0] == 'v' || item[0] == 'V') && item.find('@') != std::string::npos) {
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

std::uint8_t* gBytes = nullptr;
std::size_t gSize = 0;

std::uint8_t* ptrAt(std::uint32_t offset) noexcept {
    if (offset == 0xFFFFFFFFu || offset >= gSize) {
        return nullptr;
    }
    return gBytes + offset;
}

std::uint32_t relativizeReturn(const std::vector<std::uint8_t>& blob, std::uint32_t value, std::uint32_t base) noexcept {
    if (value >= base && value < base + blob.size()) {
        return value - base;
    }
    return value;
}

int runCase(std::uint32_t va, std::size_t selfOffset, std::vector<std::uint8_t>& blob, const std::vector<FixupEntry>& fixups,
            std::uint32_t a0, std::uint32_t a1, std::uint32_t a2, std::uint32_t a3, std::uint16_t fpcw) {
    using namespace ra3::render;
    // Match the emulator heap, which is 4K-aligned, so `mov al, imm` on a
    // pointer relativizes to the same offset on both sides.
    void* aligned = _aligned_malloc(blob.size() == 0 ? 1 : blob.size(), 4096);
    if (aligned == nullptr) {
        return 2;
    }
    gBytes = static_cast<std::uint8_t*>(aligned);
    gSize = blob.size();
    if (!blob.empty()) {
        std::memcpy(gBytes, blob.data(), blob.size());
    }
    const auto baseAddr = static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(gBytes));
    std::uint8_t* pool = nullptr;
    for (const FixupEntry& fix : fixups) {
        if (fix.pool != 0) {
            pool = ptrAt( static_cast<std::uint32_t>(fix.offset));
            continue;
        }
        if (fix.offset + 4 <= blob.size()) {
            if (fix.vstubKind >= 0) {
                store<std::uint32_t>(gBytes, static_cast<std::ptrdiff_t>(fix.offset), vstubAddress(fix.vstubKind));
            } else {
                const auto rel = load<std::uint32_t>(gBytes, static_cast<std::ptrdiff_t>(fix.offset));
                store<std::uint32_t>(gBytes, static_cast<std::ptrdiff_t>(fix.offset), baseAddr + rel);
            }
        }
    }
    ScriptServices services{};
    services.stringPool = pool;
    services.log = &logMessage;
    services.clearSurface = &clearSurface;
    services.setRenderTarget = &setRenderTarget;
    services.bindShader = &bindShader;
    services.vectorReserve = &vectorReserve;
    services.vectorShrink = &vectorShrink;
    services.lookupFormat = &lookupFormat;
    services.nameEquals = &nameEquals;
    services.ensurePass = &ensurePass;
    services.bindConstants = &bindConstants;
    services.refreshConstants = &refreshConstants;
    services.flushTarget = &flushTarget;
    setScriptServices(services);

    std::uint16_t savedCw = 0x037F;
    __asm {
        fnstcw savedCw
        fldcw fpcw
    }

    std::uint8_t* self = selfOffset < gSize ? gBytes + selfOffset : nullptr;
    std::uint32_t ret = 0;
    switch (va) {
    case 0x009B7E50: ret = parseFloatTokens(self, ptrAt( a0), a1, ptrAt( a2), a3); break;
    case 0x009B7EF0: ret = parseIntTokens(self, ptrAt( a0), a1, ptrAt( a2), a3); break;
    case 0x009B7740: ret = findShaderByName(self, reinterpret_cast<const char*>(ptrAt( a0))); break;
    case 0x009B7980: ret = findTextureByName(self, reinterpret_cast<const char*>(ptrAt( a0))); break;
    case 0x009B7050: ret = parseClearCommand(self, ptrAt( a0), ptrAt( a1), ptrAt( a2)); break;
    case 0x009B70A0: ret = parseClearTargetCommand(self, ptrAt( a0), ptrAt( a1), ptrAt( a2)); break;
    case 0x009B9CC0: ret = parseSetClearColorCommand(self, ptrAt( a0), ptrAt( a1), ptrAt( a2)); break;
    case 0x009B9CF0: ret = parseSetClearDepthCommand(self, ptrAt( a0), ptrAt( a1), ptrAt( a2)); break;
    case 0x009B9FC0: ret = parseRenderQuadCommand(self, ptrAt( a0), ptrAt( a1), ptrAt( a2)); break;
    case 0x009B8150: ret = parseClearShaderOverrideCommand(self, ptrAt( a0), ptrAt( a1), ptrAt( a2)); break;
    case 0x009B8230: ret = parseCastTextureCommand(self, ptrAt( a0), ptrAt( a1), ptrAt( a2)); break;
    case 0x009B31E0: ret = parseEndProcCommand(self, ptrAt( a0), ptrAt( a1), ptrAt( a2)); break;
    case 0x009B31F0: ret = parseReturnCommand(self, ptrAt( a0), ptrAt( a1), ptrAt( a2)); break;
    case 0x009B6F70: ret = parseElseCommand(self, ptrAt( a0), ptrAt( a1), ptrAt( a2)); break;
    case 0x009B7150: ret = execSetClearColorCommand(self, ptrAt( a0), ptrAt( a1), ptrAt( a2), ptrAt( a3)); break;
    case 0x009B31B0: ret = execSetClearDepthCommand(self, ptrAt( a0), ptrAt( a1), ptrAt( a2), ptrAt( a3)); break;
    case 0x009B3100: ret = execClearCommand(self, ptrAt( a0), ptrAt( a1), ptrAt( a2), ptrAt( a3)); break;
    case 0x009B3130: ret = execClearColorCommand(self, ptrAt( a0), ptrAt( a1), ptrAt( a2), ptrAt( a3)); break;
    case 0x009B3180: ret = execClearZCommand(self, ptrAt( a0), ptrAt( a1), ptrAt( a2), ptrAt( a3)); break;
    case 0x009B3160: ret = execClearTargetCommand(self, ptrAt( a0), ptrAt( a1), ptrAt( a2), ptrAt( a3)); break;
    case 0x009B30E0: ret = execGotoCommand(self, ptrAt( a0), ptrAt( a1), ptrAt( a2), ptrAt( a3)); break;
    case 0x009B6FE0: ret = execElseCommand(self, ptrAt( a0), ptrAt( a1), ptrAt( a2), ptrAt( a3)); break;
    case 0x009B6EA0: ret = execElifCommand(self, ptrAt( a0), ptrAt( a1), ptrAt( a2), ptrAt( a3)); break;
    case 0x009B8110: ret = execSetShaderOverrideCommand(self, ptrAt( a0), ptrAt( a1), ptrAt( a2), ptrAt( a3)); break;
    case 0x009B81A0: ret = execClearShaderOverrideCommand(self, ptrAt( a0), ptrAt( a1), ptrAt( a2), ptrAt( a3)); break;
    case 0x009B8070: ret = execSetShaderCommand(self, ptrAt( a0), ptrAt( a1), ptrAt( a2), ptrAt( a3)); break;
    case 0x009B81D0: ret = execSetTextureCommand(self, ptrAt( a0), ptrAt( a1), ptrAt( a2), ptrAt( a3)); break;
    case 0x009B82A0: ret = execCastTextureCommand(self, ptrAt( a0), ptrAt( a1), ptrAt( a2), ptrAt( a3)); break;
    case 0x009B75B0: ret = execSetIntCommand(self, ptrAt( a0), ptrAt( a1), ptrAt( a2), ptrAt( a3)); break;
    case 0x009B7620: ret = execSetBoolCommand(self, ptrAt( a0), ptrAt( a1), ptrAt( a2), ptrAt( a3)); break;
    case 0x009B71C0: ret = execSetDepthCommand(self, ptrAt( a0), ptrAt( a1), ptrAt( a2), ptrAt( a3)); break;
    case 0x009B73C0: ret = execBeginEventCommand(self, ptrAt( a0), ptrAt( a1), ptrAt( a2), ptrAt( a3)); break;
    case 0x009B7410: ret = execEndEventCommand(self, ptrAt( a0), ptrAt( a1), ptrAt( a2), ptrAt( a3)); break;
    case 0x009B7460: ret = execSetMarkerCommand(self, ptrAt( a0), ptrAt( a1), ptrAt( a2), ptrAt( a3)); break;
    case 0x009BC200: ret = execEndProcCommand(self, ptrAt( a0), ptrAt( a1), ptrAt( a2), ptrAt( a3)); break;
    case 0x009BC3D0: ret = execCallProcCommand(self, ptrAt( a0), ptrAt( a1), ptrAt( a2), ptrAt( a3)); break;
    case 0x009B2E30: ret = clearStreamBindings(self); break;
    case 0x009B6B80: ret = assignStreamSource(self, ptrAt( a0), ptrAt( a1)); break;
    case 0x009B58A0: ret = resetPassOverrides(self); break;
    case 0x009B5510: ret = applyNamedState(self, a0, a1); break;
    case 0x009B54D0: ret = applyNamedSampler(self, a0, a1); break;
    case 0x009B5550: ret = applyNamedStage(self, a0, a1); break;
    case 0x009B87A0: ret = querySurfaceExtent(self, ptrAt( a0), ptrAt( a1)); break;
    case 0x009B5330: ret = flushActiveTarget(self); break;
    case 0x009B8800: ret = selectPassConstant(self, a0); break;
    case 0x009B5860: ret = releasePrimarySurface(self); break;
    case 0x009B5880: ret = releaseSecondarySurface(self); break;
    case 0x009B8740: ret = bindTechniquePass(self, a0, a1); break;
    default:
        __asm { fldcw savedCw }
        std::cerr << "Unknown VA: 0x" << std::hex << va << "\n";
        _aligned_free(aligned);
        return 2;
    }
    __asm { fldcw savedCw }

    for (std::size_t off = 0; off + 4 <= gSize; off += 4) {
        const auto cur = load<std::uint32_t>(gBytes, static_cast<std::ptrdiff_t>(off));
        if (cur >= baseAddr && cur < baseAddr + gSize) {
            store<std::uint32_t>(gBytes, static_cast<std::ptrdiff_t>(off), cur - baseAddr);
        } else if (isVstubAddress(cur)) {
            for (int kind = 0; kind <= 15; ++kind) {
                if (cur == vstubAddress(kind)) {
                    store<std::uint32_t>(gBytes, static_cast<std::ptrdiff_t>(off), 0xF0000000u | static_cast<std::uint32_t>(kind));
                    break;
                }
            }
        }
    }
    ret = relativizeReturn(blob, ret, baseAddr);
    std::cout << "ret=0x" << std::hex << std::setw(8) << std::setfill('0') << ret
              << " blob=" << toHex(gBytes, gSize) << "\n";
    _aligned_free(aligned);
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
            std::string a3Str;
            if (!(iss >> vaStr >> selfStr >> blobHex >> fixStr >> a0Str >> a1Str >> a2Str)) {
                return 1;
            }
            std::uint16_t fpcw = 0x037F;
            if (iss >> fpcwStr) {
                fpcw = static_cast<std::uint16_t>(std::strtoul(fpcwStr.c_str(), nullptr, 0));
            }
            std::uint32_t a3 = 0;
            if (iss >> a3Str) {
                a3 = static_cast<std::uint32_t>(std::strtoul(a3Str.c_str(), nullptr, 0));
            }
            auto blob = parseHex(blobHex);
            const int rc = runCase(
                static_cast<std::uint32_t>(std::strtoul(vaStr.c_str(), nullptr, 0)),
                static_cast<std::size_t>(std::strtoul(selfStr.c_str(), nullptr, 0)),
                blob,
                parseFixups(fixStr),
                static_cast<std::uint32_t>(std::strtoul(a0Str.c_str(), nullptr, 0)),
                static_cast<std::uint32_t>(std::strtoul(a1Str.c_str(), nullptr, 0)),
                static_cast<std::uint32_t>(std::strtoul(a2Str.c_str(), nullptr, 0)),
                a3,
                fpcw);
            if (rc != 0) {
                return rc;
            }
        }
        return 0;
    }
    return 0;
}
