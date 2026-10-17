#include "ra3/gamelogic/ProtectedCaveLogic2.h"

#include <cstring>

namespace ra3::gamelogic {
namespace {

float bitsToFloat(std::uint32_t bits) {
    float v = 0.0f;
    std::memcpy(&v, &bits, 4);
    return v;
}

}  // namespace

std::uint16_t caveRead16(CaveState& st, std::uint32_t addr) {
    const std::uint32_t lo = caveRead8(st, addr);
    const std::uint32_t hi = caveRead8(st, addr + 1u);
    return static_cast<std::uint16_t>(lo | (hi << 8));
}

void caveWrite16(CaveState& st, std::uint32_t addr, std::uint16_t value) {
    caveWrite8(st, addr, static_cast<std::uint8_t>(value & 0xFFu));
    caveWrite8(st, addr + 1u, static_cast<std::uint8_t>((value >> 8) & 0xFFu));
}

// 1. 0x004D5550
void runNotifySpecialPowerReady(CaveState& st, std::uint32_t obj) {
    st.cleanup = 4;
    st.edx = 0u;
    const std::uint32_t val = caveCall(st, 0x006A77F0u, obj, 0u, 0u, 0u, 0);
    caveCall(st, 0x00687D60u, obj, val, 0u, 0u, 1);
    const std::uint32_t sub = caveCall(st, 0x006A7600u, obj, 0u, 0u, 0u, 0);
    if (sub == 0u) {
        st.eax = 0u;
        return;
    }
    const std::uint32_t vt = caveRead32(st, sub);
    const std::uint32_t fn84 = caveRead32(st, vt + 0x84u);
    st.edx = fn84;
    const std::uint32_t ok = caveCall(st, fn84, sub, 0u, 0u, 0u, 0);
    if (ok == 0u) {
        st.eax = 0u;
        return;
    }
    const std::uint32_t vt2 = caveRead32(st, sub);
    const std::uint32_t fn108 = caveRead32(st, vt2 + 0x108u);
    st.edx = fn108;
    st.eax = caveCall(st, fn108, sub, 0x004D5480u, val, 1u, 3);
}

// 2. 0x00502700
void runStopSubChannelStream(CaveState& st) {
    const std::uint32_t self = kCaveImageBase;
    st.cleanup = 0;
    st.eax = 0u;
    st.edx = 0u;
    if (caveRead8(st, self + 0x80u) == 0u || caveRead32(st, self + 0xACu) == 0u) {
        return;
    }
    const std::uint32_t lockObj = caveRead32(st, self + 0x88u);
    const std::uint32_t fpFn = caveRead32(st, 0x00BBA3A8u);
    const std::uint32_t savedFp = caveCall(st, fpFn, self, 0u, 0u, 0u, 2);
    caveCall(st, fpFn, self, 0x10000u, 0x30000u, 0u, 2);
    if (lockObj != 0u) {
        caveCall(st, 0x009CCF80u, lockObj, 0u, 0u, 0u, 0);
    }
    const std::uint32_t stream = caveRead32(st, self + 0xACu);
    caveCall(st, 0x009CFEC0u, stream, 0u, 0u, 0u, 2);
    caveWrite8(st, self + 0x80u, 0u);
    caveWrite8(st, self + 0x81u, 1u);
    std::uint32_t lastEcx = stream;
    if (lockObj != 0u) {
        caveCall(st, 0x009CCFA0u, lockObj, 0u, 0u, 0u, 0);
        lastEcx = lockObj;
    }
    st.eax = caveCall(st, fpFn, lastEcx, savedFp, 0x30000u, 0u, 2);
}

// 3. 0x00505060
void runUpdateSubChannelEnvelope(CaveState& st, std::uint32_t dtBits) {
    const std::uint32_t self = kCaveImageBase;
    st.cleanup = 4;
    st.eax = 0u;
    st.edx = 0u;
    if (caveRead8(st, self + 0xECu) == 0u) {
        return;
    }
    const float dt = bitsToFloat(dtBits);
    const float scale = caveReadF32(st, 0x00BD94BCu);
    float t = dt * scale + caveReadF32(st, self + 0xE8u);
    caveWriteF32(st, self + 0xE8u, t);

    const float d0 = caveReadF32(st, self + 0xDCu);
    if (d0 >= t) {
        const float v0 = caveReadF32(st, self + 0xD4u);
        const float v1 = caveReadF32(st, self + 0xD8u);
        caveWriteF32(st, self + 0xB4u, (v1 - v0) * (t / d0) + v0);
    } else {
        t -= d0;
        const float d1 = caveReadF32(st, self + 0xE0u);
        if (d1 >= t) {
            caveWriteF32(st, self + 0xB4u, caveReadF32(st, self + 0xD8u));
        } else {
            t -= d1;
            const float d2 = caveReadF32(st, self + 0xE4u);
            if (d2 >= t) {
                const float v0 = caveReadF32(st, self + 0xD8u);
                const float v1 = caveReadF32(st, self + 0xD4u);
                caveWriteF32(st, self + 0xB4u, (v1 - v0) * (t / d2) + v0);
            } else {
                caveWriteF32(st, self + 0xB4u, caveReadF32(st, self + 0xD4u));
                caveWrite8(st, self + 0xECu, 0u);
            }
        }
    }

    float val = caveReadF32(st, self + 0xB4u);
    const float maxVal = caveReadF32(st, 0x00BD90A4u);
    if (0.0f > val) {
        val = 0.0f;
    } else if (val > maxVal) {
        val = maxVal;
    }
    caveWriteF32(st, self + 0xB4u, val);
    st.eax = caveCall(st, 0x00504DC0u, self, 0u, 0u, 0u, 0);
}

// 4. 0x00513340
void runCreateRefCountedStateObject(CaveState& st, std::uint32_t outPtr) {
    const std::uint32_t self = kCaveImageBase;
    st.cleanup = 4;
    const std::uint32_t mem = caveCall(st, 0x00416820u, self, 0x48u, 0u, 0u, 1);
    if (mem != 0u) {
        caveWrite32(st, mem + 0x04u, 0u);
        caveWrite32(st, mem + 0x00u, 0x00BDF428u);
        caveWrite32(st, mem + 0x08u, 0u);
        caveWrite32(st, mem + 0x0Cu, 0u);
        caveWrite32(st, mem + 0x10u, 1u);
        for (std::uint32_t off = 0x14u; off <= 0x3Cu; off += 4u) {
            caveWrite32(st, mem + off, 0u);
        }
        for (std::uint32_t off = 0x40u; off <= 0x44u; ++off) {
            caveWrite8(st, mem + off, 0u);
        }
    }
    caveWrite32(st, outPtr, mem);
    if (mem != 0u) {
        caveWrite32(st, mem + 0x04u, caveRead32(st, mem + 0x04u) + 1u);
    }
    st.eax = outPtr;
    st.edx = mem;
}

// 5. 0x0051F780
void runApplyTransformAndRefreshView(CaveState& st, std::uint32_t a0, std::uint32_t a1,
                                     std::uint32_t a2, std::uint32_t /*a3*/, std::uint32_t /*a4*/) {
    const std::uint32_t self = kCaveImageBase;
    st.cleanup = 20;
    const std::uint32_t vt0 = caveRead32(st, self);
    const std::uint32_t fn98 = caveRead32(st, vt0 + 0x98u);
    caveCall(st, fn98, self, 0u, 0u, 0u, 0);

    const std::uint32_t vt1 = caveRead32(st, self);
    const std::uint32_t fnAC = caveRead32(st, vt1 + 0xACu);
    caveCall(st, fnAC, self, a0, a1, a2, 5);

    const std::uint32_t vt2 = caveRead32(st, self);
    const std::uint32_t fnC4 = caveRead32(st, vt2 + 0xC4u);
    st.edx = fnC4;
    st.eax = caveCall(st, fnC4, self, 0u, 0u, 0u, 0);
}

// 6. 0x00521050
void runResolveObjectTeamOrPlayerEntry(CaveState& st) {
    const std::uint32_t self = kCaveImageBase;
    st.cleanup = 0;
    st.edx = 0u;
    const std::uint32_t p138 = caveRead32(st, self + 0x138u);
    if (p138 != 0u) {
        st.eax = p138 + 0x78u;
        return;
    }
    const std::uint32_t p32c = caveRead32(st, self + 0x32Cu);
    if (p32c != 0u) {
        const std::uint32_t g74 = caveRead32(st, 0x00DFBD74u);
        const std::uint32_t mgr = caveCall(st, 0x007E20F0u, g74, 0u, 0u, 0u, 0);
        const std::uint32_t idx = caveRead32(st, mgr + 0x20u);
        st.eax = caveCall(st, 0x006A43A0u, p32c, idx, 0u, 0u, 1);
        return;
    }
    st.eax = 0u;
}

// 7. 0x005259E0
void runFlushDirtySubObjectsAndNotify(CaveState& st, std::uint32_t forceFlag) {
    const std::uint32_t self = kCaveImageBase;
    st.cleanup = 4;
    st.edx = 0u;
    const std::uint8_t b13f = caveRead8(st, self + 0x13Fu);
    if ((b13f & 0x08u) == 0u && (forceFlag & 0xFFu) == 0u) {
        st.eax = forceFlag;
        return;
    }
    const std::uint32_t p264 = self + 0x264u;
    std::uint32_t eax = caveCall(st, 0x00522880u, p264, self + 0x2A0u, self + 0x2DCu, 0u, 2);
    const std::uint32_t end = caveRead32(st, self + 0x218u);
    std::uint32_t cur = caveRead32(st, self + 0x214u);
    std::uint32_t edx = 0u;
    while (cur != end) {
        const std::uint32_t listener = caveRead32(st, cur);
        const std::uint32_t vt = caveRead32(st, listener);
        const std::uint32_t fn0 = caveRead32(st, vt);
        edx = fn0;
        eax = caveCall(st, fn0, listener, p264, forceFlag, 0u, 3);
        cur += 4u;
    }
    caveWrite8(st, self + 0x13Fu, static_cast<std::uint8_t>(caveRead8(st, self + 0x13Fu) & 0xF7u));
    st.eax = eax;
    st.edx = edx;
}

// 8. 0x0054DF20
void runInitTerrainRenderHelper(CaveState& st, std::uint32_t provider) {
    const std::uint32_t self = kCaveImageBase;
    st.cleanup = 4;
    caveWrite32(st, self + 0x00u, provider);
    caveWrite32(st, self + 0x04u, 0u);
    caveWrite32(st, self + 0x08u, 0u);
    caveWrite32(st, self + 0x0Cu, 1u);
    caveWrite8(st, self + 0x10u, 0u);
    caveWrite32(st, self + 0x1Cu, 0u);
    caveWrite32(st, self + 0x20u, 0u);
    caveWrite32(st, self + 0x24u, 0u);
    caveWrite32(st, self + 0x18u, 0u);

    caveCall(st, 0x0054DDC0u, self + 4u, caveRead32(st, self), 0u, 0u, 1);
    const std::uint32_t curProv = caveRead32(st, self);
    const std::uint32_t vt = caveRead32(st, curProv);
    const std::uint32_t fn4 = caveRead32(st, vt + 4u);
    const std::uint32_t val14 = caveCall(st, fn4, curProv, 0u, 0u, 0u, 0);
    caveWrite32(st, self + 0x14u, val14);
    st.eax = self;
    st.edx = fn4;
}

// 9. 0x0057F540
void runInitLocalizedProjectileString(CaveState& st, std::uint32_t srcPtr) {
    const std::uint32_t self = kCaveImageBase;
    st.cleanup = 4;
    st.edx = 0u;
    caveWrite32(st, self, 0u);
    const std::uint32_t rawStr = caveRead32(st, srcPtr);
    const std::uint32_t resolved = caveCall(st, 0x00499640u, self, rawStr, 0u, 0u, 2);
    if (resolved != 0u) {
        std::uint32_t len = 0u;
        while (caveRead8(st, resolved + len) != 0u) {
            ++len;
        }
        caveCall(st, 0x004CB690u, self, resolved, len, 0u, 2);
        caveCall(st, 0x004CAA30u, self, 0u, 0u, 0u, 0);
        st.eax = self;
        return;
    }
    const std::uint32_t str2 = caveRead32(st, srcPtr);
    caveCall(st, 0x004CBCA0u, str2, self, 0x00BD9254u, str2, 3);
    caveCall(st, 0x004CAA30u, self, 0u, 0u, 0u, 0);
    st.eax = self;
}

// 10. 0x00598630
void runDestroySubObjectArrayBuffer(CaveState& st) {
    const std::uint32_t self = kCaveImageBase;
    st.cleanup = 0;
    st.edx = 0u;
    if (caveRead32(st, self + 0xB4u) == 0u) {
        caveWrite32(st, self + 0xC0u, 0u);
        caveWrite32(st, self + 0xC4u, 0u);
        caveWrite32(st, self + 0xBCu, 0u);
        st.eax = 0u;
        return;
    }
    caveCall(st, 0x00997410u, 0u, 0u, 0u, 0u, 0);
    const std::uint32_t arr = caveRead32(st, self + 0xB4u);
    if (arr != 0u) {
        std::int32_t count = static_cast<std::int32_t>(caveRead32(st, arr - 0x10u));
        const std::uint32_t rawBlock = arr - 0x10u;
        std::uint32_t cursor = arr + (static_cast<std::uint32_t>(count) << 6);
        std::uint32_t lastEcx = 0u;
        --count;
        while (count >= 0) {
            cursor -= 0x40u;
            caveCall(st, 0x0058BA70u, cursor, 0u, 0u, 0u, 0);
            lastEcx = cursor;
            --count;
        }
        caveCall(st, 0x004166B0u, lastEcx, rawBlock, 0u, 0u, 1);
    }
    caveWrite32(st, self + 0xB4u, 0u);
    st.eax = caveCall(st, 0x00997460u, 0u, 0u, 0u, 0u, 0);
    caveWrite32(st, self + 0xC0u, 0u);
    caveWrite32(st, self + 0xC4u, 0u);
    caveWrite32(st, self + 0xBCu, 0u);
}

// 11. 0x005B5180
void runIsIndexedTableEntryEmpty(CaveState& st, std::uint32_t index) {
    const std::uint32_t self = kCaveImageBase;
    st.cleanup = 4;
    const std::uint32_t entryKey = caveRead32(st, 0x00CB7378u + index * 12u);
    caveCall(st, 0x004CBFF0u, 0u, entryKey, 0u, 0u, 1);
    const std::uint32_t vt = caveRead32(st, self);
    const std::uint32_t fn20 = caveRead32(st, vt + 0x20u);
    const std::uint32_t valStrPtr = caveCall(st, fn20, self, 0u, 0u, 0x00DEAD04u, 3);
    caveCall(st, 0x004CA930u, 0u, 0u, 0u, 0u, 0);
    const bool isEmpty = (valStrPtr == 0u) || (caveRead16(st, valStrPtr - 4u) == 0u);
    const std::uint32_t dtorRet = caveCall(st, 0x004CA930u, 0u, 0u, 0u, 0u, 0);
    st.eax = (dtorRet & 0xFFFFFF00u) | (isEmpty ? 1u : 0u);
    st.edx = fn20;
}

// 12. 0x005E65B0
void runDestroyGameInfoBase(CaveState& st) {
    const std::uint32_t self = kCaveImageBase;
    st.cleanup = 0;
    st.edx = 0u;
    const std::uint32_t subD4 = caveRead32(st, self + 0xD4u);
    caveWrite32(st, self, 0x00BEF5C8u);
    if (subD4 != 0u) {
        caveCall(st, 0x0054CB10u, subD4 + 4u, 0u, 0u, 0u, 0);
        caveCall(st, 0x004CAB60u, subD4, 0u, 0u, 0u, 0);
        caveCall(st, 0x004166B0u, subD4, subD4, 0u, 0u, 1);
        caveWrite32(st, self + 0xD4u, 0u);
    }
    static constexpr std::uint32_t kStrOffsets[] = {
        0xC4u, 0xC0u, 0xBCu, 0xB8u, 0xB4u, 0xB0u, 0xACu, 0x38u, 0x34u
    };
    for (std::uint32_t off : kStrOffsets) {
        caveCall(st, 0x004CA930u, self + off, 0u, 0u, 0u, 0);
    }
    st.eax = caveCall(st, 0x004CAB60u, self + 0x1Cu, 0u, 0u, 0u, 0);
    caveWrite32(st, self, 0x00BDB8E8u);
}

// 13. 0x005E6B30
void runResetGameSlot(CaveState& st) {
    const std::uint32_t self = kCaveImageBase;
    st.cleanup = 0;
    caveWrite32(st, self + 0x04u, 1u);
    caveWrite8(st, self + 0x08u, 0u);
    caveWrite8(st, self + 0x09u, 1u);
    caveWrite8(st, self + 0x0Au, 0u);
    caveWrite32(st, self + 0x0Cu, 0xFFFFFFFFu);
    caveWrite32(st, self + 0x10u, 0xFFFFFFFFu);
    caveWrite32(st, self + 0x14u, 0xFFFFFFFFu);
    caveWrite32(st, self + 0x18u, 0u);
    const std::uint32_t g78 = caveRead32(st, 0x00DFBD78u);
    std::uint32_t edxVal = 0u;
    if (g78 != 0u) {
        edxVal = caveRead32(st, g78 + 0x90u);
        caveWrite32(st, self + 0x18u, edxVal);
    }
    caveWrite32(st, self + 0x1Cu, 0xFFFFFFFFu);
    caveWrite32(st, self + 0x20u, 0u);
    caveWrite32(st, self + 0x24u, 0xFFFFFFFFu);
    caveWrite32(st, self + 0x28u, 0xFFFFFFFFu);
    caveWrite32(st, self + 0x2Cu, 0xFFFFFFFFu);
    caveCall(st, 0x004CAB60u, self + 0x30u, 0u, 0u, 0u, 0);
    caveCall(st, 0x004CA930u, self + 0x34u, 0u, 0u, 0u, 0);
    caveWrite32(st, self + 0x3Cu, 0u);
    caveWrite32(st, self + 0x38u, 0u);
    caveWrite32(st, self + 0x40u, 1u);
    caveWrite32(st, self + 0x44u, 0u);
    caveWrite8(st, self + 0x48u, 0u);
    caveWrite32(st, self + 0x4Cu, 0xFFFFFFFFu);
    caveWrite32(st, self + 0x50u, 0xFFFFFFFFu);
    caveWrite8(st, self + 0x54u, 1u);
    st.eax = caveCall(st, 0x004CA9B0u, self + 0x58u, 0x00DEAD04u, 0u, 0u, 1);
    st.edx = 0u;
}

// 14. 0x00674640
void runRegisterEntryInTaggedMap(CaveState& st, std::uint32_t entry) {
    const std::uint32_t self = kCaveImageBase;
    st.cleanup = 4;
    const bool use40 = (caveRead32(st, entry + 0x08u) != 0u);
    const std::uint32_t k0 = 0x56626220u;
    const std::uint32_t k1 = caveRead32(st, entry + 0x10u);
    const std::uint32_t mapAddr = self + (use40 ? 0x40u : 0x24u);
    const std::uint32_t slotAddr = caveCall(st, 0x00673220u, mapAddr, k0, k1, 0u, 1);
    caveWrite32(st, slotAddr, entry);
    st.eax = slotAddr;
    st.edx = use40 ? k1 : 0u;
}

// 15. 0x0067E600
void runFindModuleMatchingSubObject(CaveState& st, std::uint32_t obj, std::uint32_t target) {
    st.cleanup = 0;
    st.edx = 0u;
    if (target == 0u) {
        st.eax = 0u;
        return;
    }
    std::uint32_t cursor = caveRead32(st, obj + 0x310u);
    std::uint32_t modAddr = caveRead32(st, cursor);
    std::uint32_t lastEdx = 0u;
    while (modAddr != 0u) {
        const std::uint32_t ifc = modAddr + 0x0Cu;
        const std::uint32_t vt1 = caveRead32(st, ifc);
        const std::uint32_t fn50 = caveRead32(st, vt1 + 0x50u);
        lastEdx = vt1;
        const std::uint32_t sub = caveCall(st, fn50, ifc, 0u, 0u, 0u, 0);
        if (sub != 0u) {
            const std::uint32_t vt2 = caveRead32(st, sub);
            const std::uint32_t fn20 = caveRead32(st, vt2 + 0x20u);
            lastEdx = vt2;
            const std::uint32_t match = caveCall(st, fn20, sub, target, 0u, 0u, 1);
            if ((match & 0xFFu) != 0u) {
                st.eax = sub;
                st.edx = lastEdx;
                return;
            }
        }
        cursor += 4u;
        modAddr = caveRead32(st, cursor);
    }
    st.eax = 0u;
    st.edx = lastEdx;
}

// 16. 0x006885D0
void runInitKindOfFilterPredicate(CaveState& st, std::uint32_t filterTarget,
                                  std::uint32_t arg1, std::uint32_t invertFlag) {
    const std::uint32_t self = kCaveImageBase;
    st.cleanup = 12;
    caveWrite32(st, self + 0x04u, 0u);
    caveWrite32(st, self + 0x00u, 0x00BF6664u);
    caveWrite32(st, self + 0x08u, 0u);
    caveWrite32(st, self + 0x0Cu, filterTarget);
    caveWrite32(st, self + 0x10u, arg1);
    caveWrite32(st, self + 0x14u, 0x000FFFFFu);
    caveWrite8(st, self + 0x18u, static_cast<std::uint8_t>(invertFlag & 0xFFu));
    if (filterTarget != 0u) {
        const std::uint32_t mask = caveCall(st, 0x004D88C0u, filterTarget, arg1, 0u, 0u, 1);
        caveWrite32(st, self + 0x14u, mask);
        if (caveRead8(st, self + 0x18u) == 0u) {
            caveWrite32(st, self + 0x14u, ~mask);
        }
    }
    st.eax = self;
    st.edx = invertFlag & 0xFFu;
}

// 17. 0x0068C150
void runHaveDifferentContainerStatuses(CaveState& st, std::uint32_t objA, std::uint32_t objB) {
    st.cleanup = 0;
    std::uint32_t lastEdx = 0u;
    const std::uint32_t cA = caveRead32(st, objA + 0x33Cu);
    std::uint32_t valA = 0u;
    if (cA != 0u) {
        const std::uint32_t vtA = caveRead32(st, cA);
        const std::uint32_t fnA = caveRead32(st, vtA + 0xCCu);
        lastEdx = vtA;
        valA = caveCall(st, fnA, cA, 0u, 0u, 0u, 0);
    }
    const std::uint32_t cB = caveRead32(st, objB + 0x33Cu);
    std::uint32_t valB = 0u;
    if (cB != 0u) {
        const std::uint32_t vtB = caveRead32(st, cB);
        const std::uint32_t fnB = caveRead32(st, vtB + 0xCCu);
        lastEdx = vtB;
        valB = caveCall(st, fnB, cB, 0u, 0u, 0u, 0);
    }
    st.eax = (valB & 0xFFFFFF00u) | (valA != valB ? 1u : 0u);
    st.edx = lastEdx;
}

// 18. 0x006A8400
void runTriggerWeaponInitAudioEvent(CaveState& st) {
    const std::uint32_t self = kCaveImageBase;
    st.cleanup = 0;
    caveCall(st, 0x00B23C00u, 0u, 0u, 0u, 0u, 0);
    const std::uint32_t p4 = caveRead32(st, self + 0x04u);
    const std::uint32_t tmpl = caveRead32(st, p4 + 0x04u);
    const std::uint32_t idx = caveRead32(st, self + 0x1Cu);
    const std::uint32_t thresh = caveRead32(st, tmpl + 0x70u);
    const std::uint32_t soundRef =
        (idx < thresh) ? caveRead32(st, tmpl + 0xBCu) : caveRead32(st, tmpl + 0xC0u);
    caveCall(st, 0x0055A0B0u, 0u, soundRef, 0u, 0u, 1);
    caveCall(st, 0x0055A0F0u, 0u, 0u, 0u, 0u, 1);
    caveCall(st, 0x0055A140u, 0u, 0u, 0u, 0u, 0);
    const std::uint32_t ownerId = caveRead32(st, self + 0x0Cu);
    caveCall(st, 0x0055D130u, 0u, 0u, ownerId, 0u, 2);
    const std::uint32_t audioMgr = caveRead32(st, 0x00DF03FCu);
    const std::uint32_t vt = caveRead32(st, audioMgr);
    const std::uint32_t fn64 = caveRead32(st, vt + 0x64u);
    caveCall(st, fn64, audioMgr, 0u, 0u, 0u, 1);
    caveCall(st, 0x0055B530u, 0u, 0u, 0u, 0u, 0);
    st.eax = caveCall(st, 0x0055A140u, 0u, 0u, 0u, 0u, 0);
    st.edx = fn64;
}

// 19. 0x006C0620
void runResolveObjectIndicatorColor(CaveState& st) {
    const std::uint32_t self = kCaveImageBase;
    st.cleanup = 0;
    st.edx = 0u;
    const std::uint32_t overrideColor = caveRead32(st, self + 0x424u);
    if (overrideColor != 0u) {
        st.eax = overrideColor;
        return;
    }
    const std::uint32_t p414 = caveRead32(st, self + 0x414u);
    std::uint32_t controller = p414;
    if (p414 != 0u && caveRead32(st, p414 + 0x1F8u) == 0u) {
        controller = caveCall(st, 0x00B11D40u, caveRead32(st, self + 0x418u), 0u, 0u, 0u, 0);
    }
    const std::uint32_t tmpl = caveRead32(st, self + 0x04u);
    std::uint32_t lastEdx = 0u;
    if ((caveRead8(st, tmpl + 0xCBu) & 1u) != 0u) {
        const std::uint32_t fC8 = caveRead32(st, self + 0xC8u) >> 9;
        lastEdx = fC8;
        if ((fC8 & 1u) != 0u) {
            const std::uint32_t playerList =
                caveCall(st, 0x007E20F0u, caveRead32(st, 0x00DFBD74u), 0u, 0u, 0u, 0);
            const std::uint32_t rel = caveCall(st, 0x007F2690u, playerList, controller, 0u, 0u, 1);
            if (rel != 2u) {
                const std::uint32_t g70 = caveRead32(st, 0x00DF9470u);
                if ((g70 & 1u) == 0u) {
                    caveWrite32(st, 0x00DF9470u, g70 | 1u);
                    const std::uint32_t key =
                        caveCall(st, 0x00986150u, caveRead32(st, 0x00E1A078u), 0x00C07C88u, 0u, 0u, 1);
                    caveWrite32(st, 0x00DF946Cu, key);
                }
                const std::uint32_t k6c = caveRead32(st, 0x00DF946Cu);
                lastEdx = k6c;
                const std::uint32_t mod = caveCall(st, 0x00687460u, self, k6c, 0u, 0u, 1);
                if (mod == 0u) {
                    st.eax = 0xFF000000u;
                    st.edx = lastEdx;
                    return;
                }
                const std::uint32_t src = caveCall(st, 0x00694FF0u, mod, 0u, 0u, 0u, 0);
                st.eax = caveRead32(st, src + 0x80u);
                st.edx = lastEdx;
                return;
            }
        }
    }
    if (controller != 0u) {
        const std::uint32_t src = caveCall(st, 0x0067E3C0u, controller, 0u, 0u, 0u, 0);
        if (src != 0u) {
            st.eax = caveRead32(st, src + 0x80u);
            st.edx = lastEdx;
            return;
        }
    }
    st.eax = 0xFF000000u;
    st.edx = lastEdx;
}

// 20. 0x006C2C30
void runUpdateObjectPowerRadarStatusBit(CaveState& st, std::uint32_t enableFlag) {
    const std::uint32_t self = kCaveImageBase;
    st.cleanup = 4;
    st.edx = 0u;
    const std::uint32_t p418 = caveRead32(st, self + 0x418u);
    if (p418 == 0u) {
        st.eax = self;
        return;
    }
    const std::uint32_t p74 = caveRead32(st, p418 + 0x74u);
    if (p74 == 0u) {
        st.eax = self;
        return;
    }
    const std::uint32_t masked = caveRead32(st, self + 0x26Cu) & 0xFFFFFEBFu;
    const std::uint32_t dl = (masked != 0u) ? 1u : 0u;
    std::uint32_t lastEdx = dl;
    std::uint32_t desired = 0u;
    if ((enableFlag & 0xFFu) != 0u) {
        const std::uint32_t tmpl = caveRead32(st, self + 0x04u);
        const std::uint32_t sub14 = caveRead32(st, tmpl + 0x14u);
        const std::int32_t val8c = static_cast<std::int32_t>(caveRead32(st, sub14 + 0x8Cu));
        if (val8c != 0) {
            if (((caveRead32(st, self + 0x80u) >> 2) & 1u) == 0u &&
                ((caveRead32(st, self + 0x88u) >> 26) & 1u) == 0u &&
                ((caveRead32(st, self + 0x8Cu) >> 11) & 1u) == 0u) {
                const std::uint32_t f94 = caveRead32(st, self + 0x94u) >> 2;
                lastEdx = f94;
                if ((f94 & 1u) == 0u) {
                    const std::uint32_t fd0 = caveRead32(st, tmpl + 0xD0u) >> 21;
                    if (((fd0 & 1u) == 0u || caveRead8(st, self + 0x301u) != 0u) &&
                        (caveRead8(st, self + 0x471u) & 1u) == 0u) {
                        if (dl == 0u || val8c <= 0) {
                            desired = 1u;
                        }
                    }
                }
            }
        }
    }
    lastEdx = (lastEdx & 0xFFFFFF00u) | desired;
    const std::uint8_t cur471 = caveRead8(st, self + 0x471u);
    const std::uint32_t curBit = (cur471 >> 5) & 1u;
    st.edx = lastEdx;
    if (curBit == desired) {
        st.eax = self;
        return;
    }
    if (desired != 0u) {
        caveWrite8(st, self + 0x471u, static_cast<std::uint8_t>(cur471 | 0x20u));
        st.eax = caveCall(st, 0x007F55B0u, p74, self, 0u, 0u, 1);
        return;
    }
    caveWrite8(st, self + 0x471u, static_cast<std::uint8_t>(cur471 & 0xDFu));
    st.eax = caveCall(st, 0x007F5620u, p74, self, 0u, 0u, 1);
}

// 21. 0x006C4F10
void runDispatchVec3CommandMessage(CaveState& st, std::uint32_t vec3Ptr, std::uint32_t cmdCtx) {
    const std::uint32_t self = kCaveImageBase;
    st.cleanup = 8;
    st.edx = 0u;
    const std::uint32_t initMeta = caveCall(st, 0x007D2CB0u, 0u, 0u, cmdCtx, 1u, 3);
    const std::uint32_t v0 = caveRead32(st, vec3Ptr);
    const std::uint32_t v1 = caveRead32(st, vec3Ptr + 4u);
    const std::uint32_t v2 = caveRead32(st, vec3Ptr + 8u);
    const std::uint32_t vt = caveRead32(st, self);
    const std::uint32_t fn0 = caveRead32(st, vt);
    std::uint32_t lastEdx = fn0;
    std::uint32_t lastEax = caveCall(st, fn0, self, v0, v1, v2, 1);

    const std::uint32_t refObj = caveRead32(st, kCaveImageBase + 0x700u);
    const std::uint32_t ptr2c = caveRead32(st, kCaveImageBase + 0x704u);
    const std::uint32_t ptr1c = caveRead32(st, kCaveImageBase + 0x708u);
    const std::uint32_t cmd4c = 0x00BDB8E8u;
    (void)initMeta;
    lastEax = refObj;

    if (refObj != 0u) {
        const std::uint32_t nextRef = caveRead32(st, refObj + 4u) - 1u;
        caveWrite32(st, refObj + 4u, nextRef);
        if (static_cast<std::int32_t>(nextRef) <= 0) {
            const std::uint32_t rvt = caveRead32(st, refObj);
            const std::uint32_t rdtor = caveRead32(st, rvt);
            lastEdx = rdtor;
            lastEax = caveCall(st, rdtor, refObj, 1u, cmd4c, 0u, 1);
        }
    }
    if (ptr2c != 0u) {
        lastEax = caveCall(st, 0x004166B0u, 0u, ptr2c, cmd4c, 0u, 1);
    } else {
        lastEax = 0u;
    }
    if (ptr1c != 0u) {
        lastEax = caveCall(st, 0x004166B0u, 0u, ptr1c, cmd4c, 0u, 1);
    } else {
        lastEax = 0u;
    }
    st.eax = lastEax;
    st.edx = lastEdx;
}

// 22. 0x006C5480
void runDispatchTargetCommandMessage3D(CaveState& st, std::uint32_t targetArg, std::uint32_t cmdCtx) {
    const std::uint32_t self = kCaveImageBase;
    st.cleanup = 8;
    st.edx = 0u;
    caveCall(st, 0x007D2CB0u, 0u, 0x3Du, cmdCtx, 1u, 3);
    const std::uint32_t vt = caveRead32(st, self);
    const std::uint32_t fn0 = caveRead32(st, vt);
    std::uint32_t lastEdx = fn0;
    std::uint32_t lastEax = caveCall(st, fn0, self, targetArg, 0u, 0u, 1);

    const std::uint32_t refObj = caveRead32(st, kCaveImageBase + 0x700u);
    const std::uint32_t ptr2c = caveRead32(st, kCaveImageBase + 0x704u);
    const std::uint32_t ptr1c = caveRead32(st, kCaveImageBase + 0x708u);
    const std::uint32_t cmd4c = 0x00BDB8E8u;
    lastEax = refObj;

    if (refObj != 0u) {
        const std::uint32_t nextRef = caveRead32(st, refObj + 4u) - 1u;
        caveWrite32(st, refObj + 4u, nextRef);
        if (static_cast<std::int32_t>(nextRef) <= 0) {
            const std::uint32_t rvt = caveRead32(st, refObj);
            const std::uint32_t rdtor = caveRead32(st, rvt);
            lastEdx = rdtor;
            lastEax = caveCall(st, rdtor, refObj, 1u, cmd4c, 0u, 1);
        }
    }
    if (ptr2c != 0u) {
        lastEax = caveCall(st, 0x004166B0u, 0u, ptr2c, cmd4c, 0u, 1);
    } else {
        lastEax = 0u;
    }
    if (ptr1c != 0u) {
        lastEax = caveCall(st, 0x004166B0u, 0u, ptr1c, cmd4c, 0u, 1);
    } else {
        lastEax = 0u;
    }
    st.eax = lastEax;
    st.edx = lastEdx;
}

// 23. 0x006C8380
void runDestroyActiveBodyBase(CaveState& st) {
    const std::uint32_t self = kCaveImageBase;
    st.cleanup = 0;
    caveWrite32(st, self + 0x00u, 0x00C11AA0u);
    caveWrite32(st, self + 0x0Cu, 0x00C11970u);
    caveWrite32(st, self + 0x10u, 0x00C11898u);
    const std::uint32_t p100 = caveRead32(st, self + 0x100u);
    if (p100 != 0u) {
        caveCall(st, 0x004166B0u, self, p100, 0u, 0u, 1);
    }
    const std::uint32_t pF0 = caveRead32(st, self + 0xF0u);
    if (pF0 != 0u) {
        caveCall(st, 0x004166B0u, self, pF0, 0u, 0u, 1);
    }
    const std::uint32_t listHead = self + 0xE4u;
    std::uint32_t cur = caveRead32(st, listHead);
    std::uint32_t lastEdx = 0u;
    while (cur != listHead) {
        const std::uint32_t pool = caveRead32(st, 0x00DEA3E0u);
        const std::uint32_t nextFree = caveRead32(st, pool + 0x10u);
        const std::uint32_t node = cur;
        cur = caveRead32(st, cur);
        caveWrite32(st, node, nextFree);
        caveWrite32(st, pool + 0x10u, node);
        lastEdx = pool + 0x10u;
    }
    caveWrite32(st, self + 0x10u, 0x00BF62D0u);
    caveWrite32(st, self + 0x00u, 0x00BDBC08u);
    caveWrite32(st, self + 0x0Cu, 0x00BDBAD8u);
    caveWrite32(st, self + 0x00u, 0x00BDB8E8u);
    st.eax = listHead;
    st.edx = lastEdx;
}

// 24. 0x006CACE0
void runTryExecuteSpecialPowerOnTarget(CaveState& st, std::uint32_t targetObj,
                                       std::uint32_t arg0, std::uint32_t arg2) {
    const std::uint32_t self = kCaveImageBase + 0x20u;
    st.cleanup = 12;
    const std::uint32_t ebp = caveRead32(st, self - 0x08u);
    const std::uint32_t vt0 = caveRead32(st, self);
    const std::uint32_t fn9c = caveRead32(st, vt0 + 0x9Cu);
    std::uint32_t lastEdx = fn9c;
    const std::uint32_t r9c = caveCall(st, fn9c, self, 0u, 0u, 0u, 0);
    if ((r9c & 0xFFu) == 0u) {
        st.eax = r9c;
        st.edx = lastEdx;
        return;
    }
    const std::uint32_t edi = self - 0x10u;
    const std::uint32_t r4220 = caveCall(st, 0x006B4220u, edi, arg0, 0u, 0u, 1);
    if ((r4220 & 0xFFu) == 0u) {
        st.eax = r4220;
        st.edx = lastEdx;
        return;
    }
    if (targetObj != 0u) {
        const std::uint32_t m0c = caveRead32(st, self - 0x0Cu);
        if (caveRead8(st, m0c + 0x1BBu) != 0u) {
            const std::uint32_t r44e0 = caveCall(st, 0x006B44E0u, edi, targetObj, 0u, 0u, 1);
            if ((r44e0 & 0xFFu) == 0u) {
                st.eax = r44e0 & 0xFFFFFF00u;
                st.edx = lastEdx;
                return;
            }
        }
    }
    const std::uint32_t vt1 = caveRead32(st, self);
    const std::uint32_t fn1c = caveRead32(st, vt1 + 0x1Cu);
    lastEdx = vt1;
    const std::uint32_t sub1c = caveCall(st, fn1c, self, 0u, 0u, 0u, 0);
    if (sub1c != 0u && targetObj != 0u) {
        const std::uint32_t mgr = caveRead32(st, 0x00CC569Cu) + 4u;
        const std::uint32_t r03b0 = caveCall(st, 0x007F03B0u, mgr, sub1c, targetObj, 0u, 2);
        if ((r03b0 & 0xFFu) != 0u) {
            st.eax = r03b0 & 0xFFFFFF00u;
            st.edx = lastEdx;
            return;
        }
    }
    if (((caveRead32(st, ebp + 0x88u) >> 12) & 1u) != 0u) {
        st.eax = 0u;
        st.edx = lastEdx;
        return;
    }
    const std::uint32_t f84 = caveRead32(st, ebp + 0x84u) >> 17;
    lastEdx = f84;
    if ((f84 & 1u) != 0u) {
        st.eax = 0u;
        st.edx = lastEdx;
        return;
    }
    const std::uint32_t vt2 = caveRead32(st, self);
    const std::uint32_t fn1c_2 = caveRead32(st, vt2 + 0x1Cu);
    lastEdx = fn1c_2;
    const std::uint32_t id1c = caveCall(st, fn1c_2, self, 0u, 0u, 0u, 0);
    const std::uint32_t r4f20 = caveCall(st, 0x006A4F20u, ebp, id1c, 0u, 0u, 1);
    if ((r4f20 & 0xFFu) != 0u) {
        st.eax = r4f20 & 0xFFFFFF00u;
        st.edx = lastEdx;
        return;
    }
    const std::uint32_t vt3 = caveRead32(st, self);
    const std::uint32_t fn1c_3 = caveRead32(st, vt3 + 0x1Cu);
    lastEdx = fn1c_3;
    const std::uint32_t id1c_3 = caveCall(st, fn1c_3, self, 0u, 0u, 0u, 0);
    const std::uint32_t handler = caveCall(st, 0x00687780u, ebp, id1c_3, 0u, 0u, 1);
    if (handler != 0u) {
        const std::uint32_t hvt = caveRead32(st, handler);
        const std::uint32_t fn18 = caveRead32(st, hvt + 0x18u);
        lastEdx = hvt;
        st.eax = caveCall(st, fn18, handler, targetObj, arg2, 0u, 2);
        st.edx = lastEdx;
        return;
    }
    st.eax = 1u;
    st.edx = lastEdx;
}

// 25. 0x006D1DD0
void runRefreshObjectModulesAndDrawableState(CaveState& st) {
    const std::uint32_t self = kCaveImageBase;
    st.cleanup = 0;
    std::uint32_t lastEdx = 0u;
    std::uint32_t cursor = caveRead32(st, self + 0x310u);
    std::uint32_t modAddr = caveRead32(st, cursor);
    while (modAddr != 0u) {
        const std::uint32_t ifc = modAddr + 0x0Cu;
        const std::uint32_t vt = caveRead32(st, ifc);
        const std::uint32_t fn0c = caveRead32(st, vt + 0x0Cu);
        lastEdx = fn0c;
        const std::uint32_t sub = caveCall(st, fn0c, ifc, 0u, 0u, 0u, 0);
        if (sub != 0u) {
            const std::uint32_t svt = caveRead32(st, sub);
            const std::uint32_t fn08 = caveRead32(st, svt + 0x08u);
            lastEdx = svt;
            caveCall(st, fn08, sub, 0u, 0u, 0u, 0);
        }
        cursor += 4u;
        modAddr = caveRead32(st, cursor);
    }
    const std::uint32_t g54 = caveRead32(st, 0x00DEEE54u);
    const std::uint32_t gvt = caveRead32(st, g54);
    const std::uint32_t fn9c = caveRead32(st, gvt + 0x9Cu);
    lastEdx = gvt;
    const std::uint32_t r9c = caveCall(st, fn9c, g54, 0u, 0u, 0u, 0);
    if ((r9c & 0xFFu) == 0u) {
        caveCall(st, 0x006C2C30u, self, 1u, 0u, 0u, 1);
    }
    const std::uint32_t edi = caveRead32(st, self + 0xF4u);
    if (edi != 0u) {
        std::uint32_t c180 = caveRead32(st, edi + 0x180u);
        if (c180 != 0u) {
            std::uint32_t m = caveRead32(st, c180);
            while (m != 0u) {
                const std::uint32_t mvt = caveRead32(st, m);
                const std::uint32_t fn4c = caveRead32(st, mvt + 0x4Cu);
                lastEdx = mvt;
                const std::uint32_t sub = caveCall(st, fn4c, m, 0u, 0u, 0u, 0);
                if (sub != 0u) {
                    const std::uint32_t svt = caveRead32(st, sub);
                    const std::uint32_t fn08 = caveRead32(st, svt + 0x08u);
                    lastEdx = svt;
                    caveCall(st, fn08, sub, 0u, 0u, 0u, 0);
                }
                c180 += 4u;
                m = caveRead32(st, c180);
            }
        }
        std::uint32_t c17c = caveRead32(st, edi + 0x17Cu);
        if (c17c != 0u) {
            std::uint32_t m = caveRead32(st, c17c);
            while (m != 0u) {
                const std::uint32_t mvt = caveRead32(st, m);
                const std::uint32_t fn38 = caveRead32(st, mvt + 0x38u);
                lastEdx = mvt;
                const std::uint32_t sub = caveCall(st, fn38, m, 0u, 0u, 0u, 0);
                if (sub != 0u) {
                    const std::uint32_t svt = caveRead32(st, sub);
                    const std::uint32_t fn08 = caveRead32(st, svt + 0x08u);
                    lastEdx = svt;
                    caveCall(st, fn08, sub, 0u, 0u, 0u, 0);
                }
                c17c += 4u;
                m = caveRead32(st, c17c);
            }
        }
    }
    const std::uint32_t f90 = caveRead32(st, self + 0x90u) >> 6;
    if ((f90 & 1u) == 0u) {
        const std::uint32_t tmpl = caveRead32(st, self + 0x04u);
        const std::uint32_t fc0 = caveRead32(st, tmpl + 0xC0u) >> 9;
        lastEdx = tmpl;
        if ((fc0 & 1u) != 0u) {
            const std::uint32_t f94 = caveRead32(st, self + 0x94u) >> 5;
            if ((f94 & 1u) != 0u) {
                st.eax = fc0;
                st.edx = lastEdx;
                return;
            }
        }
    }
    st.eax = caveCall(st, 0x006A4D20u, self, 0u, 0u, 0u, 0);
    st.edx = lastEdx;
}

// 26. 0x006E4760
void runFindSelectableObjectAndQueryModule44(CaveState& st, std::uint32_t objId, std::uint32_t outObj) {
    st.cleanup = 0;
    st.edx = 0u;
    if (outObj != 0u) {
        caveWrite32(st, outObj, 0u);
    }
    const std::uint32_t logic = caveRead32(st, 0x00DEBA64u);
    if (objId == 0u) {
        st.eax = 0u;
        return;
    }
    caveCall(st, 0x00A21BE0u, 0x00DEBB00u, 0x00BF605Cu, 0u, 0u, 1);
    const std::uint32_t table = logic + 0xC4u;
    const std::uint32_t iterNode = caveCall(st, 0x0050B5F0u, table, objId, 0u, 0u, 2);
    const std::uint32_t bucketCount = caveRead32(st, table + 0x08u);
    const std::uint32_t buckets = caveRead32(st, table + 0x04u);
    const std::uint32_t endSentinel = caveRead32(st, buckets + bucketCount * 4u);
    std::uint32_t esi = 0u;
    if (iterNode != endSentinel) {
        esi = caveRead32(st, iterNode + 0x04u);
    }
    caveCall(st, 0x00A21C90u, 0x00DEBB00u, 1u, 0u, 0u, 1);
    if (esi == 0u) {
        st.eax = 0u;
        st.edx = bucketCount;
        return;
    }
    if ((caveRead8(st, esi + 0x471u) & 1u) != 0u) {
        st.eax = 0u;
        st.edx = bucketCount;
        return;
    }
    const std::uint32_t tmpl = caveRead32(st, esi + 0x04u);
    if (((caveRead32(st, tmpl + 0xC4u) >> 4) & 1u) == 0u) {
        st.eax = 0u;
        st.edx = tmpl;
        return;
    }
    if (((caveRead32(st, esi + 0x80u) >> 21) & 1u) != 0u) {
        st.eax = 0u;
        st.edx = tmpl;
        return;
    }
    if (outObj != 0u) {
        caveWrite32(st, outObj, esi);
    }
    std::uint32_t cursor = caveRead32(st, esi + 0x310u);
    std::uint32_t m = caveRead32(st, cursor);
    std::uint32_t lastEdx = tmpl;
    while (m != 0u) {
        const std::uint32_t ifc = m + 0x0Cu;
        const std::uint32_t vt = caveRead32(st, ifc);
        const std::uint32_t fn44 = caveRead32(st, vt + 0x44u);
        lastEdx = vt;
        const std::uint32_t res = caveCall(st, fn44, ifc, 0u, 0u, 0u, 0);
        if (res != 0u) {
            st.eax = res;
            st.edx = lastEdx;
            return;
        }
        cursor += 4u;
        m = caveRead32(st, cursor);
    }
    st.eax = 0u;
    st.edx = lastEdx;
}

// 27. 0x006EFE40
void runResolveObjectByProducerId(CaveState& st) {
    const std::uint32_t self = kCaveImageBase;
    st.cleanup = 0;
    st.edx = 0u;
    const std::uint32_t producerId = caveRead32(st, self + 0x3ECu);
    const std::uint32_t logic = caveRead32(st, 0x00DEBA64u);
    if (producerId == 0u) {
        st.eax = 0u;
        return;
    }
    caveCall(st, 0x00A21BE0u, 0x00DEBB00u, 0x00BF605Cu, 0u, 0u, 1);
    const std::uint32_t table = logic + 0xC4u;
    const std::uint32_t iterNode = caveCall(st, 0x0050B5F0u, table, producerId, 0u, 0u, 2);
    const std::uint32_t bucketCount = caveRead32(st, table + 0x08u);
    const std::uint32_t buckets = caveRead32(st, table + 0x04u);
    const std::uint32_t endSentinel = caveRead32(st, buckets + bucketCount * 4u);
    std::uint32_t result = 0u;
    if (iterNode != endSentinel) {
        result = caveRead32(st, iterNode + 0x04u);
    }
    caveCall(st, 0x00A21C90u, 0x00DEBB00u, 1u, 0u, 0u, 1);
    st.eax = result;
    st.edx = bucketCount;
}

// 28. 0x006F1350
void runIsObjectMobileAndControllable(CaveState& st) {
    const std::uint32_t self = kCaveImageBase;
    st.cleanup = 0;
    std::uint32_t lastEdx = 0u;
    std::uint32_t eax = caveRead32(st, self + 0x80u) >> 5;
    if ((eax & 1u) != 0u) {
        st.eax = eax & 0xFFFFFF00u;
        st.edx = 0u;
        return;
    }
    eax = caveCall(st, 0x006EFE40u, self, 0u, 0u, 0u, 0);
    if (eax != 0u) {
        const std::uint32_t tmpl = caveRead32(st, eax + 0x04u);
        const std::uint32_t fcc = caveRead32(st, tmpl + 0xCCu) >> 10;
        lastEdx = fcc;
        if ((fcc & 1u) != 0u) {
            eax = caveCall(st, 0x006EFE40u, eax, 0u, 0u, 0u, 0);
        }
        if (eax != 0u) {
            std::uint32_t edi = caveRead32(st, eax + 0x328u);
            if (edi == 0u) {
                const std::uint32_t p32c = caveRead32(st, eax + 0x32Cu);
                if (p32c != caveRead32(st, eax + 0x330u)) {
                    lastEdx = p32c;
                    edi = caveRead32(st, p32c);
                }
            }
            if (edi != 0u) {
                eax = caveRead32(st, self + 0x84u) >> 7;
                if ((eax & 1u) == 0u) {
                    const std::uint32_t ivt = caveRead32(st, edi);
                    const std::uint32_t fnAC = caveRead32(st, ivt + 0xACu);
                    lastEdx = fnAC;
                    const std::uint32_t pOut = caveCall(st, fnAC, edi, 0u, self, 0u, 2);
                    eax = caveRead32(st, pOut) >> 1;
                    if ((eax & 1u) == 0u) {
                        st.eax = eax & 0xFFFFFF00u;
                        st.edx = lastEdx;
                        return;
                    }
                    if (caveRead32(st, self + 0x374u) != 0u) {
                        eax = caveCall(st, 0x006A6210u, self, 0u, 0u, 0u, 0);
                        if (eax != 0u) {
                            const std::uint32_t ivt2 = caveRead32(st, edi);
                            const std::uint32_t fnB8 = caveRead32(st, ivt2 + 0xB8u);
                            lastEdx = ivt2;
                            eax = caveCall(st, fnB8, edi, self, eax, 0u, 2);
                            if ((eax & 0xFFu) != 0u) {
                                st.eax = eax & 0xFFFFFF00u;
                                st.edx = lastEdx;
                                return;
                            }
                        }
                    }
                }
            }
        }
    }

    std::uint32_t ecx = caveRead32(st, self + 0x80u);
    const std::uint32_t edx80 = ecx >> 2;
    lastEdx = edx80;
    if ((edx80 & 1u) != 0u) {
        st.eax = eax & 0xFFFFFF00u;
        st.edx = lastEdx;
        return;
    }
    eax = ecx >> 21;
    if ((eax & 1u) != 0u) {
        st.eax = eax & 0xFFFFFF00u;
        st.edx = lastEdx;
        return;
    }
    const std::uint32_t tmpl = caveRead32(st, self + 0x04u);
    lastEdx = tmpl;
    eax = caveRead32(st, tmpl + 0xC0u);
    if (((eax >> 14) & 1u) != 0u) {
        lastEdx = eax >> 14;
        if (((eax >> 1) & 1u) != 0u) {
            lastEdx = eax >> 1;
            const std::uint32_t g = caveRead32(st, 0x00DEBA64u);
            lastEdx = g;
            if (caveRead8(st, g + 0xACu) == 0u) {
                st.eax = eax & 0xFFFFFF00u;
                st.edx = lastEdx;
                return;
            }
        }
    } else {
        lastEdx = eax >> 14;
    }
    eax >>= 5;
    if ((eax & 1u) != 0u) {
        st.eax = (eax & 0xFFFFFF00u) | 1u;
        st.edx = lastEdx;
        return;
    }
    ecx >>= 1;
    if ((ecx & 1u) != 0u) {
        st.eax = (eax & 0xFFFFFF00u) | 1u;
        st.edx = lastEdx;
        return;
    }
    const std::uint32_t edi = caveCall(st, 0x006A7600u, self, 0u, 0u, 0u, 0);
    eax = edi;
    if (edi != 0u) {
        const std::uint32_t pvt = caveRead32(st, edi);
        const std::uint32_t fnB0 = caveRead32(st, pvt + 0xB0u);
        lastEdx = fnB0;
        eax = caveCall(st, fnB0, edi, 0u, 0u, 0u, 0);
        if ((eax & 0xFFu) != 0u) {
            const std::uint32_t pvt2 = caveRead32(st, edi);
            const std::uint32_t fn10C = caveRead32(st, pvt2 + 0x10Cu);
            lastEdx = fn10C;
            eax = caveCall(st, fn10C, edi, 0u, 0u, 0u, 1);
            if (static_cast<std::int32_t>(eax) > 0) {
                st.eax = (eax & 0xFFFFFF00u) | 1u;
                st.edx = lastEdx;
                return;
            }
        }
    }
    const std::uint32_t p37c = caveRead32(st, self + 0x37Cu);
    if (p37c != 0u && caveRead32(st, self + 0x374u) != 0u) {
        eax = caveCall(st, 0x006A19E0u, p37c, 0u, 0u, 0u, 0);
        if ((eax & 0xFFu) != 0u) {
            st.eax = (eax & 0xFFFFFF00u) | 1u;
            st.edx = lastEdx;
            return;
        }
    }
    const std::uint32_t sub68 = caveCall(st, 0x00687880u, self, 0u, 0u, 0u, 0);
    if (sub68 == 0u) {
        st.eax = 0u;
        st.edx = lastEdx;
        return;
    }
    const std::uint32_t svt = caveRead32(st, sub68);
    const std::uint32_t fn1C = caveRead32(st, svt + 0x1Cu);
    lastEdx = svt;
    eax = caveCall(st, fn1C, sub68, 0u, 0u, 0u, 0);
    if ((eax & 0xFFu) == 0u) {
        st.eax = eax & 0xFFFFFF00u;
        st.edx = lastEdx;
        return;
    }
    st.eax = (eax & 0xFFFFFF00u) | 1u;
    st.edx = lastEdx;
}

// 29. 0x0072CF30
void runApplyDrawableTintEnvelope(CaveState& st, std::uint32_t tintVec3) {
    const std::uint32_t self = kCaveImageBase;
    st.cleanup = 4;
    st.edx = 0u;
    const std::uint32_t owner = caveRead32(st, self + 0x08u);
    const std::uint32_t drawable = caveRead32(st, owner + 0xF4u);
    if (drawable == 0u) {
        st.eax = owner;
        return;
    }
    const std::uint32_t node = caveCall(st, 0x004D99E0u, 0u, 0x0Cu, 0u, 0u, 2);
    if (node + 8u != 0u) {
        caveWrite32(st, node + 8u, drawable);
    }
    caveWrite32(st, node + 0u, 0u);
    caveWrite32(st, node + 4u, 0u);
    caveCall(st, 0x008758C0u, 0u, 0u, 0u, 0u, 0);
    const std::uint32_t t0 = caveRead32(st, tintVec3 + 0u);
    const std::uint32_t t1 = caveRead32(st, tintVec3 + 4u);
    const std::uint32_t t2 = caveRead32(st, tintVec3 + 8u);
    caveCall(st, 0x00879050u, 0x1Bu, t0, t1, t2, 3);
    st.eax = caveCall(st, 0x00550290u, 0u, 0u, 0u, 0u, 0);
    st.edx = t2;
}

// 30. 0x007461C0
void runCopyWeaponAimTransformAndUpdate(CaveState& st, std::uint32_t targetObj,
                                        std::uint32_t arg1, std::uint32_t arg2) {
    const std::uint32_t self = kCaveImageBase;
    st.cleanup = 12;
    st.edx = 0u;
    std::uint32_t eax = caveRead32(st, self + 0x40u) >> 7;
    if ((eax & 1u) != 0u) {
        for (std::uint32_t i = 0u; i < 11u; ++i) {
            caveWrite32(st, self + 0x70u + i * 4u, caveRead32(st, targetObj + 0x08u + i * 4u));
        }
        const std::uint32_t r = caveCall(st, 0x006A4540u, self, targetObj, 0u, 0u, 1);
        st.edx = arg1;
        st.eax = caveCall(st, 0x00735DB0u, self, targetObj, arg1, r, 4);
        (void)arg2;
        return;
    }
    if (caveRead8(st, self + 0xA2u) == 0u) {
        st.eax = eax;
        return;
    }
    const float f38 = caveReadF32(st, self + 0x38u);
    const float zero = caveReadF32(st, 0x00BDB9A0u);
    if (f38 != zero) {
        const std::uint32_t ah = (f38 < zero) ? 0x03u : 0x02u;
        st.eax = (eax & 0xFFFF00FFu) | (ah << 8);
        return;
    }
    for (std::uint32_t i = 0u; i < 11u; ++i) {
        caveWrite32(st, self + 0x70u + i * 4u, caveRead32(st, targetObj + 0x08u + i * 4u));
    }
    const std::uint32_t r = caveCall(st, 0x006A4540u, self, targetObj, 0u, 0u, 1);
    st.edx = arg1;
    st.eax = caveCall(st, 0x00735DB0u, self, targetObj, arg1, r, 4);
    (void)arg2;
}

}  // namespace ra3::gamelogic
