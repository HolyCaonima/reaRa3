#include "ra3/render/RenderScript9B.h"
#include "ra3/render/RenderScript9B2.h"

#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <iomanip>
#include <iostream>
#include <malloc.h>
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

std::uint8_t* gManager = nullptr;

std::uint32_t __fastcall vIntern(std::uint8_t* pool, void*, std::uint32_t name) noexcept {
    store<std::uint32_t>(pool, 0x10, name);
    return name == 0u ? 0u : 0x42u;
}

std::uint32_t __fastcall vReleaseKey(std::uint8_t* pool, void*, std::uint32_t key) noexcept {
    store<std::uint32_t>(pool, 0x14, key);
    return 0u;
}

std::uint32_t __fastcall vCompare(std::uint8_t* self, void*, std::uint32_t left, std::uint32_t right) noexcept {
    store<std::uint32_t>(self, 0x18, left);
    store<std::uint32_t>(self, 0x1C, right);
    return left == (right ^ 0x01000000u) ? 1u : 0u;
}

std::uint8_t* __fastcall vAlloc(std::uint8_t* self, void*, std::uint32_t size, std::uint32_t, std::uint32_t) noexcept {
    const auto next = load<std::uint32_t>(self, 0x20);
    store<std::uint32_t>(self, 0x20, next + size);
    return reinterpret_cast<std::uint8_t*>(next);
}

std::uint32_t __fastcall vFree(std::uint8_t* self, void*, std::uint32_t object, std::uint32_t) noexcept {
    store<std::uint32_t>(self, 0x48, object);
    return 0u;
}

std::uint32_t __fastcall vQueryName(std::uint8_t*, void*, std::uint32_t name, std::uint32_t out) noexcept {
    auto* dest = reinterpret_cast<std::uint8_t*>(out);
    if (name == 0x00C600D4u) {
        store<std::uint32_t>(dest, 0, load<std::uint32_t>(gManager, 0x60));
        return load<std::uint32_t>(gManager, 0x64) & 0xFFu;
    }
    store<std::uint32_t>(dest, 0, load<std::uint32_t>(gManager, 0x68));
    return load<std::uint32_t>(gManager, 0x6C) & 0xFFu;
}

std::uint32_t __fastcall vCamera(
    std::uint8_t* self, void*, std::uint32_t device, std::uint32_t camera, std::uint32_t extra,
    std::uint32_t width, std::uint32_t height) noexcept {
    store<std::uint32_t>(self, 0x200, device);
    store<std::uint32_t>(self, 0x204, camera);
    store<std::uint32_t>(self, 0x208, extra);
    store<std::uint32_t>(self, 0x20C, width);
    store<std::uint32_t>(self, 0x210, height);
    return 0u;
}

std::uint32_t __fastcall vDirty(std::uint8_t* self, void*, std::uint32_t maskA, std::uint32_t maskB) noexcept {
    store<std::uint32_t>(self, 0x40, maskA);
    store<std::uint32_t>(self, 0x44, maskB);
    return 0u;
}

void __cdecl vNode(std::uint32_t node, std::uint32_t device, std::uint32_t extra) noexcept {
    auto* bytes = reinterpret_cast<std::uint8_t*>(node);
    store<std::uint32_t>(bytes, 0x30, 0xA1u);
    store<std::uint32_t>(bytes, 0x34, device);
    store<std::uint32_t>(bytes, 0x38, extra);
}

std::uint32_t __fastcall vDeviceEnd(std::uint8_t*, void*, std::uint32_t device) noexcept {
    store<std::uint32_t>(reinterpret_cast<std::uint8_t*>(device), 0x90, 1u);
    return 0u;
}

std::uint32_t __fastcall vDeviceSet(std::uint8_t*, void*, std::uint32_t device, std::uint32_t value) noexcept {
    auto* bytes = reinterpret_cast<std::uint8_t*>(device);
    store<std::uint32_t>(bytes, 0x90, 2u);
    store<std::uint32_t>(bytes, 0x94, value);
    return 0u;
}

std::uint32_t __fastcall vDeviceSame(std::uint8_t*, void*, std::uint32_t device) noexcept {
    store<std::uint32_t>(reinterpret_cast<std::uint8_t*>(device), 0x90, 3u);
    return 0u;
}

std::uint32_t __fastcall vNested(std::uint8_t* self, void*) noexcept {
    store<std::uint32_t>(self, 4, 1u);
    return 0u;
}

std::uint32_t __fastcall vUpload(
    std::uint8_t*, void*, std::uint32_t device, std::uint32_t mode, std::uint32_t count, std::uint32_t source,
    std::uint32_t stride) noexcept {
    auto* bytes = reinterpret_cast<std::uint8_t*>(device);
    store<std::uint32_t>(bytes, 0x40, mode);
    store<std::uint32_t>(bytes, 0x44, count);
    store<std::uint32_t>(bytes, 0x48, source);
    store<std::uint32_t>(bytes, 0x4C, stride);
    return 0u;
}

std::uint32_t __fastcall vStream(std::uint8_t* self, void*, std::uint32_t) noexcept {
    return load<std::uint32_t>(self, 0x30);
}

std::uint32_t __fastcall vUsage(std::uint8_t* self, void*) noexcept {
    return load<std::uint32_t>(self, 4);
}

std::uint32_t __fastcall vObject(std::uint8_t* self, void*) noexcept {
    return load<std::uint32_t>(self, 8);
}

std::uint32_t vstubAddress(int kind) noexcept {
    switch (kind) {
    case 13: return static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(&vIntern));
    case 14: return static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(&vReleaseKey));
    case 15: return static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(&vCompare));
    case 16: return static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(&vAlloc));
    case 17: return static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(&vFree));
    case 18: return static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(&vQueryName));
    case 19: return static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(&vCamera));
    case 20: return static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(&vDirty));
    case 21: return static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(&vNode));
    case 22: return static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(&vDeviceEnd));
    case 23: return static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(&vDeviceSet));
    case 24: return static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(&vDeviceSame));
    case 25: return static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(&vNested));
    case 26: return static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(&vUpload));
    case 27: return static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(&vStream));
    case 28: return static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(&vUsage));
    case 29: return static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(&vObject));
    default: return 0u;
    }
}

bool isVstubAddress(std::uint32_t address) noexcept {
    for (int kind = 13; kind <= 29; ++kind) {
        if (address != 0u && address == vstubAddress(kind)) {
            return true;
        }
    }
    return false;
}

std::uint32_t vectorReserve(std::uint8_t*, std::uint32_t, std::uint32_t) noexcept { return 0u; }
std::uint32_t vectorShrink(std::uint8_t*) noexcept { return 0u; }

std::uint32_t nameEquals(std::uint8_t* entry, std::uint32_t left, std::uint32_t right) noexcept {
    store<std::uint32_t>(entry, 0x10, left);
    store<std::uint32_t>(entry, 0x14, right);
    return left == (right ^ 1u) ? 1u : 0u;
}

std::uint32_t logMessage(std::uint32_t format, std::uint32_t text) noexcept {
    if (gManager != nullptr) {
        store<std::uint32_t>(gManager, 0x50, format);
        store<std::uint32_t>(gManager, 0x54, text);
    }
    return 0u;
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
    int pool = 0;
    int manager = 0;
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
            } else if (item[0] == 'm' && item.find('@') != std::string::npos) {
                entry.manager = 1;
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

std::uint32_t relativizeReturn(std::uint32_t value, std::uint32_t base) noexcept {
    if (value >= base && value < base + gSize) {
        return value - base;
    }
    return value;
}

int runCase(std::uint32_t va, std::size_t selfOffset, std::vector<std::uint8_t>& blob, const std::vector<FixupEntry>& fixups,
            std::uint32_t a0, std::uint32_t a1, std::uint32_t a2, std::uint32_t a3, std::uint16_t fpcw) {
    using namespace ra3::render;
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
    gManager = nullptr;
    for (const FixupEntry& fix : fixups) {
        if (fix.pool != 0) {
            pool = ptrAt(static_cast<std::uint32_t>(fix.offset));
            continue;
        }
        if (fix.manager != 0) {
            gManager = ptrAt(static_cast<std::uint32_t>(fix.offset));
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
    services.vectorReserve = &vectorReserve;
    services.vectorShrink = &vectorShrink;
    services.nameEquals = &nameEquals;
    setScriptServices(services);
    ScriptHost host{};
    host.manager = gManager;
    host.pool = pool;
    host.log = &logMessage;
    host.reserve = &vectorReserve;
    host.shrink = &vectorShrink;
    setScriptHost(host);

    std::uint16_t savedCw = 0x037F;
    __asm {
        fnstcw savedCw
        fldcw fpcw
    }
    std::uint8_t* self = selfOffset < gSize ? gBytes + selfOffset : nullptr;
    std::uint32_t ret = 0;
    switch (va) {
    case 0x009BCC40: ret = parseSetIntCommand(self, ptrAt(a0), ptrAt(a1), ptrAt(a2)); break;
    case 0x009BCD20: ret = parseSetBoolCommand(self, ptrAt(a0), ptrAt(a1), ptrAt(a2)); break;
    case 0x009BCB10: ret = parseSetFloatCommand(self, ptrAt(a0), ptrAt(a1), ptrAt(a2)); break;
    case 0x009B9D50: ret = parseSetDepthCommand(self, ptrAt(a0), ptrAt(a1), ptrAt(a2)); break;
    case 0x009BBFA0: ret = parseGotoCommand(self, ptrAt(a0), ptrAt(a1), ptrAt(a2)); break;
    case 0x009BC070: ret = parseBeginEventCommand(self, ptrAt(a0), ptrAt(a1), ptrAt(a2)); break;
    case 0x009BC310: ret = parseCallProcCommand(self, ptrAt(a0), ptrAt(a1), ptrAt(a2)); break;
    case 0x009BD1D0: ret = parseSetShaderCommand(self, ptrAt(a0), ptrAt(a1), ptrAt(a2)); break;
    case 0x009BD380: ret = parseSetShaderOverrideCommand(self, ptrAt(a0), ptrAt(a1), ptrAt(a2)); break;
    case 0x009BD460: ret = parseSetTextureCommand(self, ptrAt(a0), ptrAt(a1), ptrAt(a2)); break;
    case 0x009BD560: ret = parseSetToSizeCommand(self, ptrAt(a0), ptrAt(a1), ptrAt(a2)); break;
    case 0x009BBE20: ret = parseElifCommand(self, ptrAt(a0), ptrAt(a1), ptrAt(a2)); break;
    case 0x009BB930: ret = parseIfNotCommand(self, ptrAt(a0), ptrAt(a1), ptrAt(a2)); break;
    case 0x009BBB30: ret = parseIfCommand(self, ptrAt(a0), ptrAt(a1), ptrAt(a2)); break;
    case 0x009BBD30: ret = parseEndIfCommand(self, ptrAt(a0), ptrAt(a1), ptrAt(a2)); break;
    case 0x009B9C40: ret = parseSetCameraCommand(self, ptrAt(a0), ptrAt(a1), ptrAt(a2)); break;
    case 0x009BC420: ret = execRenderQuadCommand(self, ptrAt(a0), ptrAt(a1), ptrAt(a2), ptrAt(a3)); break;
    case 0x009BC780: ret = execRenderFullQuadCommand(self, ptrAt(a0), ptrAt(a1), ptrAt(a2), ptrAt(a3)); break;
    case 0x009B6BD0: ret = execSetViewportCommand(self, ptrAt(a0), ptrAt(a1), ptrAt(a2), ptrAt(a3)); break;
    case 0x009B6CA0: ret = execSetScissorCommand(self, ptrAt(a0), ptrAt(a1), ptrAt(a2), ptrAt(a3)); break;
    case 0x009B6D70: ret = execSetCameraCommand(self, ptrAt(a0), ptrAt(a1), ptrAt(a2), ptrAt(a3)); break;
    case 0x009BACE0: ret = execSetToSizeCommand(self, ptrAt(a0), ptrAt(a1), ptrAt(a2), ptrAt(a3)); break;
    case 0x009BBA80: ret = execIfNotCommand(self, ptrAt(a0), ptrAt(a1), ptrAt(a2), ptrAt(a3)); break;
    case 0x009BBC80: ret = execIfCommand(self, ptrAt(a0), ptrAt(a1), ptrAt(a2), ptrAt(a3)); break;
    case 0x009BBDB0: ret = execEndIfCommand(self, ptrAt(a0), ptrAt(a1), ptrAt(a2), ptrAt(a3)); break;
    case 0x009B58C0: ret = flushLineList(self); break;
    case 0x009B5960: ret = flushPointList(self); break;
    case 0x009B59D0: ret = flushLineStrip(self); break;
    case 0x009B5A40: ret = flushTriangleList(self); break;
    case 0x009B5AF0: ret = flushTriangleStrip(self); break;
    case 0x009B5B60: ret = flushTriangleFan(self); break;
    case 0x009B3910: ret = dispatchDirtyState(self, a0, a1); break;
    case 0x009B5380: ret = classifyPrimitive(self, ptrAt(a0)); break;
    case 0x009B5430: ret = readMatrixOrder(self); break;
    case 0x009BB040: ret = selectConstantRoutine(self, a0, a1, ptrAt(a2)); break;
    case 0x009B9630: ret = bindDynamicStream(self, ptrAt(a0), ptrAt(a1)); break;
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
            for (int kind = 13; kind <= 29; ++kind) {
                if (cur == vstubAddress(kind)) {
                    store<std::uint32_t>(gBytes, static_cast<std::ptrdiff_t>(off), 0xF0000000u | static_cast<std::uint32_t>(kind));
                    break;
                }
            }
        }
    }
    ret = relativizeReturn(ret, baseAddr);
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
