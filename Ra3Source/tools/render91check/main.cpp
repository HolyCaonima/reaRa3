#include "ra3/render/W3DScriptedModelDraw.h"

#include <cctype>
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

// Deterministic virtual-method stubs shared between C++ (render91check) and
// Unicorn (verify_render91.py) so virtual calls inside draw-module methods can
// be verified byte-for-byte.
void __fastcall vstub0_noarg(std::uint8_t* self, void* /*edx*/) noexcept {
    store<std::uint32_t>(self, 0x08, load<std::uint32_t>(self, 0x08) + 0x11110001U);
}

void __fastcall vstub1_onearg(std::uint8_t* self, void* /*edx*/, std::uint32_t arg) noexcept {
    store<std::uint32_t>(self, 0x08, arg);
    store<std::uint32_t>(self, 0x14, load<std::uint32_t>(self, 0x14) + 0x100U + (arg & 0xFFU));
}

void __fastcall vstub2_onearg_b(std::uint8_t* self, void* /*edx*/, std::uint32_t arg) noexcept {
    store<std::uint32_t>(self, 0x0C, arg);
    store<std::uint32_t>(self, 0x14, load<std::uint32_t>(self, 0x14) + 0x200U + (arg & 0xFFU));
}

void __fastcall vstub3_vecptr(
    std::uint8_t* self, void* /*edx*/, const std::uint8_t* vecPtr) noexcept {
    if (vecPtr != nullptr) {
        store<std::uint32_t>(self, 0x08, load<std::uint32_t>(vecPtr, 0x00));
        store<std::uint32_t>(self, 0x0C, load<std::uint32_t>(vecPtr, 0x04));
        store<std::uint32_t>(self, 0x10, load<std::uint32_t>(vecPtr, 0x08));
    }
    store<std::uint32_t>(self, 0x14, load<std::uint32_t>(self, 0x14) + 0x300U);
}

void __fastcall vstub4_flags2(
    std::uint8_t* self, void* /*edx*/, const std::uint8_t* flagsPtr) noexcept {
    if (flagsPtr != nullptr) {
        const std::uint32_t packed =
            0x44440000U | (static_cast<std::uint32_t>(flagsPtr[1]) << 8U) |
            static_cast<std::uint32_t>(flagsPtr[0]);
        store<std::uint32_t>(self, 0x08, packed);
    }
    store<std::uint32_t>(self, 0x14, load<std::uint32_t>(self, 0x14) + 0x400U);
}

std::uint32_t __fastcall vstub5_subcheck(
    std::uint8_t* self, void* /*edx*/, const std::uint8_t* drawablePtr) noexcept {
    store<std::uint32_t>(
        self,
        0x08,
        load<std::uint32_t>(self, 0x08) + 1U + (drawablePtr != nullptr ? 0x10U : 0U));
    store<std::uint32_t>(
        self, 0x10, static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(drawablePtr)));
    return load<std::uint8_t>(self, 0x0C);
}

std::uint32_t vstubAddress(int kind) noexcept {
    switch (kind) {
    case 0:
        return static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(&vstub0_noarg));
    case 1:
        return static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(&vstub1_onearg));
    case 2:
        return static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(&vstub2_onearg_b));
    case 3:
        return static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(&vstub3_vecptr));
    case 4:
        return static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(&vstub4_flags2));
    case 5:
        return static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(&vstub5_subcheck));
    default:
        return 0U;
    }
}

bool isVstubAddress(std::uint32_t addr) noexcept {
    for (int k = 0; k <= 5; ++k) {
        if (addr != 0U && addr == vstubAddress(k)) {
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
                entry.offset =
                    static_cast<std::size_t>(std::strtoul(item.substr(at + 1).c_str(), nullptr, 0));
            } else {
                entry.vstubKind = -1;
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
    std::uint32_t a2,
    std::uint16_t fpcw) {
    using namespace ra3::render;

    const auto baseAddr =
        static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(blob.data()));
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

    switch (va) {
    case 0x008E52A0:
        setDynamicsDrawWord2FC(self, a0);
        break;
    case 0x008E5690:
        releaseScriptedModelTerrainDecal(self);
        break;
    case 0x008E57E0:
        ret = animTriggerCrossedFrame(self, static_cast<std::int32_t>(a0)) ? 1U : 0U;
        break;
    case 0x008E5A60:
        ret = notifySimpleDrawBoundToObject(self, ptrAt(blob, a0));
        break;
    case 0x008E5E80:
        ret = ptrToResult(blob, assignParticleSysRef(self, ptrAt(blob, a0)));
        break;
    case 0x008E6060:
        ret = ptrToResult(blob, assignParticleAttachRecord8(self, ptrAt(blob, a0)));
        break;
    case 0x008E6180:
        ret = ptrToResult(
            blob,
            copyParticleAttachRecords8Forward(ptrAt(blob, a0), ptrAt(blob, a1), ptrAt(blob, a2)));
        break;
    case 0x008E6260:
        ret = ptrToResult(
            blob,
            copyParticleAttachRecords8Backward(ptrAt(blob, a0), ptrAt(blob, a1), ptrAt(blob, a2)));
        break;
    case 0x008E8550:
        resetTruckExtraRotationIfNonZero(self);
        break;
    case 0x008E8900:
        ret = ptrToResult(blob, getFirstAnimationName(self, ptrAt(blob, a0)));
        break;
    case 0x008E8960:
        ret = ptrToResult(blob, getIndexedAnimationName(self, ptrAt(blob, a0), a1));
        break;
    case 0x008E8D50:
        ret = isAnimationLayerComplete(
                  self, ptrAt(blob, a0), static_cast<std::uint8_t>(a1))
                  ? 1U
                  : 0U;
        break;
    case 0x008E8DC0:
        ret = ptrToResult(
            blob,
            getScriptedModelModuleKeyOrOverride(
                self, reinterpret_cast<std::uint32_t*>(ptrAt(blob, a0))));
        break;
    case 0x008E8E90:
        updatePrimaryShadowTransform(self, a0, a1);
        break;
    case 0x008E8EC0:
        updatePrimaryShadowVisibility(self, a0);
        break;
    case 0x008E8FD0:
        releasePrimaryShadow(self);
        break;
    case 0x008E9000:
        updateAuxShadowModule200(self);
        break;
    case 0x008E9020:
        updateSecondaryShadowTransform(self, a0, a1);
        break;
    case 0x008E9050:
        updateSecondaryShadowVisibility(self, a0);
        break;
    case 0x008E9240:
        releaseSecondaryShadow(self);
        break;
    case 0x008E9270:
        releaseAuxShadowModule200(self);
        break;
    case 0x008E93B0:
        applyScriptedModelHouseColor(self, reinterpret_cast<const PassMask*>(ptrAt(blob, a0)));
        break;
    case 0x008EB120:
        uninitializedCopyParticleAttachRecords8(
            reinterpret_cast<void**>(self), ptrAt(blob, a0), ptrAt(blob, a1), ptrAt(blob, a2));
        break;
    case 0x008EB360:
        uninitializedCopySubObjectRecords28(
            reinterpret_cast<void**>(self), ptrAt(blob, a0), ptrAt(blob, a1), ptrAt(blob, a2));
        break;
    case 0x008EB490:
        ret = ptrToResult(
            blob,
            uninitializedCopySplineRecords80(
                reinterpret_cast<void**>(self), ptrAt(blob, a0), ptrAt(blob, a1), ptrAt(blob, a2)));
        break;
    case 0x008EB4E0:
        copyBoneTransformRecord56(self, ptrAt(blob, a0));
        break;
    case 0x008EB540:
        ret = ptrToResult(
            blob,
            uninitializedCopyRenderRecords68(
                reinterpret_cast<void**>(self), ptrAt(blob, a0), ptrAt(blob, a1), ptrAt(blob, a2)));
        break;
    case 0x008EB5E0:
        ret = ptrToResult(
            blob, copyRenderStates204Backward(ptrAt(blob, a0), ptrAt(blob, a1), ptrAt(blob, a2)));
        break;
    case 0x008EC560:
        retainSubRenderObjectAt4(self, reinterpret_cast<void**>(ptrAt(blob, a0)));
        break;
    case 0x008ED690:
        ret = ptrToResult(
            blob,
            findAnimationBoneInfoByKey(
                self,
                static_cast<std::int32_t>(a0),
                reinterpret_cast<std::uint32_t*>(ptrAt(blob, a1))));
        break;
    case 0x008EDA20:
        propagateSubObjectBoneVisibility(a0, static_cast<std::int32_t>(a1), ptrAt(blob, a2));
        break;
    case 0x008EDB30:
        applyNamedSubObjectVisibility(self, ptrAt(blob, a0), a1);
        break;
    case 0x008EE860:
        applySubObjectOpacityAndRelease(self, ptrAt(blob, a0), bitsToFloat(a1));
        break;
    case 0x008EE8E0:
        applySubObjectHideState(
            self, ptrAt(blob, a0), ptrAt(blob, a1), static_cast<std::int32_t>(a2));
        break;
    case 0x008EE990:
        updateSubObjectFadeAndVisibility(
            self, ptrAt(blob, a0), ptrAt(blob, a1), static_cast<std::int32_t>(a2));
        break;
    case 0x008EEED0:
        resetAnimationLayerSlot(self, static_cast<std::int32_t>(a0));
        break;
    case 0x008EF1A0: {
        const auto* params = ptrAt(blob, a2);
        const float durationDivisor = load<float>(params, 0);
        const float midpointZScale = load<float>(params, 4);
        ret = ptrToResult(
            blob,
            initializeHermiteSplineSegment80(
                self,
                reinterpret_cast<const float*>(ptrAt(blob, a0)),
                reinterpret_cast<const float*>(ptrAt(blob, a1)),
                durationDivisor,
                midpointZScale));
        break;
    }
    case 0x008F0A90:
        destroyParticleAttachRecords8(ptrAt(blob, a0), ptrAt(blob, a1));
        break;
    case 0x008F0B30:
        ret = ptrToResult(
            blob,
            constructBoneTransformRecord56(
                ptrAt(blob, a0),
                reinterpret_cast<const std::uint32_t*>(ptrAt(blob, a1)),
                ptrAt(blob, a2)));
        break;
    case 0x008F0B90:
        ret = ptrToResult(
            blob,
            rbTreeEqualRangeIntKey(
                self,
                reinterpret_cast<void**>(ptrAt(blob, a0)),
                reinterpret_cast<const std::int32_t*>(ptrAt(blob, a1))));
        break;
    case 0x008F0DF0:
        uninitializedCopyAnimHandleRecords12(
            reinterpret_cast<void**>(self), ptrAt(blob, a0), ptrAt(blob, a1), ptrAt(blob, a2));
        break;
    case 0x008F0F30:
        uninitializedFillRenderRecords68(ptrAt(blob, a0), a1, ptrAt(blob, a2));
        break;
    case 0x008F1010:
        uninitializedFillRefCountPairRecords8(ptrAt(blob, a0), a1, ptrAt(blob, a2));
        break;
    case 0x008F1120:
        fillRenderRecords68Range(ptrAt(blob, a0), ptrAt(blob, a1), ptrAt(blob, a2));
        break;
    case 0x008F2700:
        stopAttachedParticleSystemsInVector(self);
        break;
    case 0x008F2C10:
        updateAttachedParticleSystemsVisibility(self);
        break;
    case 0x008F3D60:
        updateScriptedModelSubObjects(self);
        break;
    case 0x008F3EA0: {
        const auto* params = ptrAt(blob, a2);
        const auto loopFlag = static_cast<std::uint8_t>(load<std::uint32_t>(params, 0));
        const auto animMode = load<std::uint32_t>(params, 4);
        const float frameStep = load<float>(params, 8);
        const float currentFrame = load<float>(params, 12);
        const float blendFactor = load<float>(params, 16);
        const auto activeFlag = static_cast<std::uint8_t>(load<std::uint32_t>(params, 20));
        const auto direction = load<std::uint32_t>(params, 24);
        assignAnimationLayerSlot(
            self,
            static_cast<std::int32_t>(a0),
            ptrAt(blob, a1),
            loopFlag,
            animMode,
            frameStep,
            currentFrame,
            blendFactor,
            activeFlag,
            direction);
        break;
    }
    case 0x008F3FA0:
        moveAnimationLayerSlot(
            self, static_cast<std::int32_t>(a0), static_cast<std::int32_t>(a1));
        break;
    case 0x008F5CB0:
        ret = rbTreeCountIntKey(
            self, reinterpret_cast<const std::int32_t*>(ptrAt(blob, a0)));
        break;
    case 0x008F6180:
        ret = ptrToResult(
            blob, copySubObjectRecords28Forward(ptrAt(blob, a0), ptrAt(blob, a1), ptrAt(blob, a2)));
        break;
    case 0x008F61B0:
        destroyAnimHandleRecords12(ptrAt(blob, a0), ptrAt(blob, a1));
        break;
    case 0x008F6690:
        ret = ptrToResult(
            blob,
            copySubObjectRecords28Backward(ptrAt(blob, a0), ptrAt(blob, a1), ptrAt(blob, a2)));
        break;
    case 0x008F6900:
        ret = ptrToResult(
            blob,
            copyAnimHandleRecords12Forward(ptrAt(blob, a0), ptrAt(blob, a1), ptrAt(blob, a2)));
        break;
    case 0x008F6C00:
        ret = ptrToResult(
            blob,
            copyAnimHandleRecords12Backward(ptrAt(blob, a0), ptrAt(blob, a1), ptrAt(blob, a2)));
        break;
    case 0x008F6ED0:
        ret = getSubObjectBoneCountForState(
            self, static_cast<std::int32_t>(a0), static_cast<std::int32_t>(a1));
        break;
    case 0x008F95D0:
        ret = ptrToResult(
            blob, eraseSubObjectRecords28Range(self, ptrAt(blob, a0), ptrAt(blob, a1)));
        break;
    case 0x008FA6D0:
        fillRefCountPairRecords8Range(ptrAt(blob, a0), ptrAt(blob, a1), ptrAt(blob, a2));
        break;
    case 0x008FA790:
        ret = ptrToResult(
            blob,
            copyRefCountPairRecords8Forward(ptrAt(blob, a0), ptrAt(blob, a1), ptrAt(blob, a2)));
        break;
    case 0x008FAB20:
        ret = ptrToResult(blob, initializeModelConditionAnimationEntry(self));
        break;
    case 0x00907AB0:
        ret = ptrToResult(blob, initializeW3DModelDrawConditionState(self));
        break;
    case 0x00910E50:
        setScriptedModelFlag6D(self, static_cast<std::uint8_t>(a0));
        break;
    case 0x00910E60:
        ret = getScriptedModelDataFlagD3(self);
        break;
    case 0x00910E70:
        ret = isScriptedModelActive6DOrDataD4(self) ? 1U : 0U;
        break;
    case 0x00910E90:
        setScriptedModelFlag6C(self, static_cast<std::uint8_t>(a0));
        break;
    case 0x00910EB0:
        setScriptedModelFlag216(self, static_cast<std::uint8_t>(a0));
        break;
    case 0x00910EC0:
        ret = ptrToResult(blob, getScriptedModelDrawInterface(self));
        break;
    case 0x00910ED0:
        ret = ptrToResult(blob, getScriptedModelSubInterface18(self));
        break;
    case 0x00910EE0:
        setScriptedModelFlag215(self, static_cast<std::uint8_t>(a0));
        break;
    case 0x00911250:
        ret = ptrToResult(blob, getDynamicsDrawSubInterface290(self));
        break;
    default:
        __asm { fldcw savedCw }
        std::cerr << "Unknown VA: 0x" << std::hex << va << "\n";
        return 2;
    }

    __asm {
        fldcw savedCw
    }

    // Normalize internal blob pointers and vstub addresses back to canonical offsets/tokens
    for (std::size_t off = 0; off + 4 <= blob.size(); off += 4) {
        const auto cur = load<std::uint32_t>(blob.data(), off);
        if (cur >= baseAddr && cur < baseAddr + blob.size()) {
            store<std::uint32_t>(blob.data(), off, cur - baseAddr);
        } else if (isVstubAddress(cur)) {
            for (int k = 0; k <= 5; ++k) {
                if (cur == vstubAddress(k)) {
                    store<std::uint32_t>(
                        blob.data(), off, 0xF0000000U | static_cast<std::uint32_t>(k));
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
            const int rc = runCase(va, selfOffset, blob, fixups, a0, a1, a2, fpcw);
            if (rc != 0) {
                return rc;
            }
        }
        return 0;
    }

    if (argc < 8) {
        std::cerr << "Usage: ra3_render91check <va> <self_offset> <blob_hex> <ptr_fixups_csv> "
                     "<arg0> <arg1> <arg2> [fpcw]\n"
                     "       ra3_render91check --batch\n";
        return 1;
    }

    const auto va = static_cast<std::uint32_t>(std::strtoul(argv[1], nullptr, 0));
    const auto selfOffset = static_cast<std::size_t>(std::strtoul(argv[2], nullptr, 0));
    auto blob = parseHex(argv[3]);
    const auto fixups = parseFixups(argv[4]);
    const auto a0 = static_cast<std::uint32_t>(std::strtoul(argv[5], nullptr, 0));
    const auto a1 = static_cast<std::uint32_t>(std::strtoul(argv[6], nullptr, 0));
    const auto a2 = static_cast<std::uint32_t>(std::strtoul(argv[7], nullptr, 0));
    const std::uint16_t fpcw =
        argc >= 9 ? static_cast<std::uint16_t>(std::strtoul(argv[8], nullptr, 0)) : 0x037F;

    return runCase(va, selfOffset, blob, fixups, a0, a1, a2, fpcw);
}
