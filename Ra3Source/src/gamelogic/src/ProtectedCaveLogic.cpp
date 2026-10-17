#include "ra3/gamelogic/ProtectedCaveLogic.h"

#include <cstring>

namespace ra3::gamelogic {
namespace {

std::uint32_t load32(const std::uint8_t* p) {
    std::uint32_t v = 0;
    std::memcpy(&v, p, 4);
    return v;
}

void store32(std::uint8_t* p, std::uint32_t v) {
    std::memcpy(p, &v, 4);
}

float loadF(const std::uint8_t* p) {
    float v = 0.0f;
    std::memcpy(&v, p, 4);
    return v;
}

void storeF(std::uint8_t* p, float v) {
    std::memcpy(p, &v, 4);
}

std::uint8_t* slot(CaveState& st, std::uint32_t addr) {
    return st.image + (addr - kCaveImageBase);
}

bool inImage(std::uint32_t addr) {
    return addr >= kCaveImageBase && addr + 4 <= kCaveImageBase + kCaveImageSize;
}

// x87 reciprocal of a 32-bit magnitude, matching fild / optional +2^32 / fld1 / fdivrp / fstp.
float recipX87(std::uint32_t magnitude) {
    float recip = 0.0f;
#if defined(_M_IX86)
    const float two32 = 4294967296.0f;
    __asm {
        fild dword ptr magnitude
        test dword ptr magnitude, 0x80000000
        jns positive
        fadd dword ptr two32
    positive:
        fld1
        fdivrp st(1), st(0)
        fstp dword ptr recip
    }
#else
    const float adjusted = (magnitude & 0x80000000u) != 0u
                               ? static_cast<float>(static_cast<double>(static_cast<std::int32_t>(magnitude)) + 4294967296.0)
                               : static_cast<float>(static_cast<std::int32_t>(magnitude));
    recip = 1.0f / adjusted;
#endif
    return recip;
}

bool releaseRef(CaveState& st, std::uint32_t obj) {
    if (obj == 0u) {
        return false;
    }
    const std::uint32_t next = caveRead32(st, obj + 4u) - 1u;
    caveWrite32(st, obj + 4u, next);
    if (static_cast<std::int32_t>(next) > 0) {
        return false;
    }
    const std::uint32_t vt = caveRead32(st, obj);
    const std::uint32_t dtor = caveRead32(st, vt);
    st.eax = caveCall(st, dtor, obj, 1u, 0u, 0u, 1);
    st.edx = vt;
    return true;
}

std::uint32_t classifyMask(CaveState& st, std::uint32_t target) {
    const std::uint32_t tmpl = caveRead32(st, target + 4u);
    st.eax = tmpl;
    st.edx = target;
    const std::uint32_t c4 = caveRead32(st, tmpl + 0xC4u);
    if (((c4 >> 24) & 1u) != 0u) {
        return 0x201u;
    }
    if (((c4 >> 21) & 1u) != 0u) {
        return 0x20u;
    }
    const std::uint32_t c8 = caveRead32(st, tmpl + 0xC8u);
    if (((c8 >> 11) & 1u) != 0u) {
        return 0x100u;
    }
    const std::uint32_t c0 = caveRead32(st, tmpl + 0xC0u);
    if (((c0 >> 26) & 1u) != 0u) {
        st.eax = c0 >> 26;
        return 0x10u;
    }
    std::uint32_t mask = 0u;
    const std::uint32_t flags80 = caveRead32(st, target + 0x80u);
    if (((flags80 >> 7) & 1u) != 0u) {
        const std::uint32_t flags98 = caveRead32(st, target + 0x98u);
        st.eax = flags98 >> 19;
        st.edx = tmpl;
        if (((flags98 >> 19) & 1u) != 0u) {
            mask |= 0x800u;
        }
        const std::uint32_t d8 = caveRead32(st, tmpl + 0xD8u);
        const std::uint32_t c0b = caveRead32(st, tmpl + 0xC0u);
        if (((d8 >> 12) & 1u) != 0u || ((c0b >> 11) & 1u) != 0u || ((c0b >> 14) & 1u) != 0u) {
            return mask | 0x2040u;
        }
        st.eax = c0b >> 10;
        if ((st.eax & 1u) != 0u) {
            return mask | 0x1080u;
        }
        const std::uint32_t c8b = caveRead32(st, tmpl + 0xC8u);
        st.edx = c8b >> 14;
        if ((st.edx & 1u) != 0u) {
            return mask | 0x400u;
        }
        return mask;
    }
    const std::uint8_t byte98 = caveRead8(st, target + 0x98u);
    if ((byte98 & 1u) != 0u) {
        mask |= 4u;
    } else {
        const std::uint32_t air = caveCall(st, 0x006A6070u, target, 0u, 0u, 0u, 0);
        if ((air & 0xFFu) != 0u) {
            mask |= 2u;
        } else {
            const std::uint32_t water = caveCall(st, 0x006875D0u, target, 0u, 0u, 0u, 0);
            if ((water & 0xFFu) != 0u) {
                mask |= 2u;
            } else if (((c0 >> 9) & 1u) != 0u) {
                mask |= 8u;
                const float x = caveReadF32(st, target + 0x38u);
                const float y = caveReadF32(st, target + 0x3Cu);
                const float z = caveReadF32(st, target + 0x40u);
                std::uint32_t xb = 0u;
                std::uint32_t yb = 0u;
                std::uint32_t zb = 0u;
                std::memcpy(&xb, &x, 4);
                std::memcpy(&yb, &y, 4);
                std::memcpy(&zb, &z, 4);
                const std::uint32_t over = caveCall(st, 0x0085CBC0u, caveRead32(st, 0x00DFC198u), xb, yb, zb, 3);
                st.eax = over;
                mask |= ((over & 0xFFu) != 0u) ? 2u : 1u;
                return mask;
            } else {
                mask |= 1u;
            }
        }
    }
    st.edx = tmpl;
    const std::uint32_t tailC0 = caveRead32(st, tmpl + 0xC0u);
    st.eax = tailC0 >> 10;
    if ((st.eax & 1u) != 0u) {
        return mask | 0x1000u;
    }
    const std::uint32_t tailD8 = caveRead32(st, tmpl + 0xD8u);
    if (((tailD8 >> 12) & 1u) != 0u) {
        return mask | 0x2000u;
    }
    return mask;
}

}  // namespace

std::uint32_t caveRead32(CaveState& st, std::uint32_t addr) {
    if (inImage(addr)) {
        return load32(slot(st, addr));
    }
    for (int i = 0; i < st.globalCount; ++i) {
        if (st.globalAddr[i] == addr) {
            return st.globalVal[i];
        }
    }
    return 0u;
}

void caveWrite32(CaveState& st, std::uint32_t addr, std::uint32_t value) {
    if (inImage(addr)) {
        store32(slot(st, addr), value);
        return;
    }
    for (int i = 0; i < st.globalCount; ++i) {
        if (st.globalAddr[i] == addr) {
            st.globalVal[i] = value;
            return;
        }
    }
}

std::uint8_t caveRead8(CaveState& st, std::uint32_t addr) {
    if (addr >= kCaveImageBase && addr < kCaveImageBase + kCaveImageSize) {
        return st.image[addr - kCaveImageBase];
    }
    return static_cast<std::uint8_t>(caveRead32(st, addr & ~3u) >> ((addr & 3u) * 8u));
}

void caveWrite8(CaveState& st, std::uint32_t addr, std::uint8_t value) {
    if (addr >= kCaveImageBase && addr < kCaveImageBase + kCaveImageSize) {
        st.image[addr - kCaveImageBase] = value;
    }
}

float caveReadF32(CaveState& st, std::uint32_t addr) {
    const std::uint32_t bits = caveRead32(st, addr);
    float v = 0.0f;
    std::memcpy(&v, &bits, 4);
    return v;
}

void caveWriteF32(CaveState& st, std::uint32_t addr, float value) {
    std::uint32_t bits = 0u;
    std::memcpy(&bits, &value, 4);
    caveWrite32(st, addr, bits);
}

std::uint32_t caveCall(CaveState& st, std::uint32_t target, std::uint32_t ecx,
                       std::uint32_t a0, std::uint32_t a1, std::uint32_t a2, int nstack) {
    const std::uint32_t result = st.retPos < st.retCount ? st.rets[st.retPos++] : 0u;
    if (st.callCount < static_cast<int>(sizeof(st.calls) / sizeof(st.calls[0]))) {
        CaveCall& row = st.calls[st.callCount++];
        row.target = target;
        row.ecx = ecx;
        row.a0 = a0;
        row.a1 = a1;
        row.a2 = a2;
        row.nstack = static_cast<std::uint32_t>(nstack);
        row.result = result;
    }
    return result;
}

void runClearNonZeroStatusWord(CaveState& st) {
    const std::uint32_t self = kCaveImageBase;
    st.eax = 0u;
    st.edx = 0u;
    st.cleanup = 0;
    if (caveRead32(st, self + 0x26Cu) != 0u) {
        caveWrite32(st, self + 0x26Cu, 0u);
    }
}

void runObjectControllingPlayer(CaveState& st) {
    const std::uint32_t self = kCaveImageBase;
    st.edx = 0u;
    st.cleanup = 0;
    const std::uint32_t direct = caveRead32(st, self + 0x328u);
    if (direct != 0u) {
        st.eax = direct;
        return;
    }
    const std::uint32_t begin = caveRead32(st, self + 0x32Cu);
    const std::uint32_t end = caveRead32(st, self + 0x330u);
    st.eax = (begin != end) ? caveRead32(st, begin) : 0u;
}

void runQueryContainedModule(CaveState& st, std::uint32_t tag) {
    const std::uint32_t self = kCaveImageBase;
    st.edx = 0u;
    st.cleanup = 4;
    const std::uint32_t container = caveRead32(st, self + 0x37Cu);
    if (container == 0u) {
        st.eax = 0u;
        return;
    }
    st.eax = caveCall(st, 0x006A1720u, container, 0u, tag, 0u, 2);
}

void runSubControllerStateActive(CaveState& st) {
    const std::uint32_t self = kCaveImageBase;
    st.edx = 0u;
    st.cleanup = 0;
    std::uint32_t eax = caveRead32(st, self + 0x21Cu);
    eax -= 1u;
    if (eax != 0u) {
        eax -= 3u;
        if (eax != 0u) {
            st.eax = eax & 0xFFFFFF00u;
            return;
        }
    }
    const std::uint32_t sub = caveRead32(st, self + 0x150u);
    if (sub == 0u) {
        st.eax = eax & 0xFFFFFF00u;
        return;
    }
    const std::uint32_t helper = caveCall(st, 0x00631C20u, sub, 0u, 0u, 0u, 0);
    if ((helper & 0xFFu) == 0u) {
        st.eax = helper & 0xFFFFFF00u;
        return;
    }
    st.eax = (helper & 0xFFFFFF00u) | 1u;
}

void runClassifyObjectKindMask(CaveState& st, std::uint32_t target, std::uint32_t outMask) {
    st.cleanup = 0;
    const std::uint32_t mask = classifyMask(st, target);
    caveWrite32(st, outMask, mask);
}

void runObjectPassesKindFilter(CaveState& st, std::uint32_t target) {
    const std::uint32_t self = kCaveImageBase;
    st.edx = 0u;
    st.cleanup = 4;
    const std::uint32_t mask = classifyMask(st, target);
    const std::uint32_t module = caveRead32(st, self + 4u);
    const std::uint32_t helper = caveCall(st, 0x00688B40u, module + 0xD0u, mask, 0u, 0u, 1);
    if ((helper & 0xFFu) != 0u) {
        st.eax = helper & 0xFFFFFF00u;
        st.edx = 0u;
        return;
    }
    st.edx = mask;
    const std::uint32_t forbidden = caveRead32(st, module + 0xD4u);
    if ((forbidden & mask) != 0u) {
        st.eax = 0u;
        return;
    }
    st.eax = 1u;
}

void runFindBehaviorModuleByKey(CaveState& st, std::uint32_t key) {
    const std::uint32_t self = kCaveImageBase;
    st.edx = 0u;
    st.cleanup = 4;
    if (caveRead32(st, self + 0x310u) == 0u || caveRead32(st, self + 0x3C0u) == 0u) {
        st.eax = 0u;
        return;
    }
    if (key == 0xFFFFFFFFu) {
        const std::uint32_t node = caveRead32(st, self + 0x3B4u);
        st.eax = caveRead32(st, node + 0x14u);
        return;
    }
    const std::uint32_t iter = caveCall(st, 0x00AAFE10u, self + 0x3ACu, key, 0u, 0u, 2);
    st.eax = (iter != self + 0x3B0u) ? caveRead32(st, iter + 0x14u) : 0u;
}

void runUnregisterObjectSubsystems(CaveState& st, std::uint32_t skipFlag) {
    const std::uint32_t self = kCaveImageBase;
    st.edx = 0u;
    st.cleanup = 4;
    st.eax = 0u;
    if (caveRead8(st, self + 0x4D8u) != 0u && skipFlag != 1u) {
        st.eax = caveCall(st, 0x004F9950u, caveRead32(st, 0x00DEBA88u), self, 0u, 0u, 1);
        caveWrite8(st, self + 0x4D8u, 0u);
    }
    if (caveRead8(st, self + 0x4D7u) == 0u) {
        return;
    }
    const std::uint32_t sub9c = caveRead32(st, self + 0x9Cu);
    if (sub9c != 0u) {
        st.eax = caveCall(st, 0x0098C370u, sub9c, 0u, 0u, 0u, 0);
    }
    const std::uint32_t tmpl = caveRead32(st, self + 4u);
    const std::uint32_t flags = caveRead32(st, tmpl + 0xC8u);
    if (((flags >> 25) & 1u) == 0u) {
        if (caveRead32(st, self + 0x4E4u) != 0u) {
            st.edx = self + 0x6Cu;
            st.eax = caveCall(st, 0x00990690u, caveRead32(st, 0x00DF609Cu), self + 0x6Cu, 0u, 0u, 1);
        }
        if (caveRead32(st, self + 0x4E8u) != 0u) {
            st.eax = caveCall(st, 0x009939C0u, caveRead32(st, 0x00DF60A4u), self + 0x74u, 0u, 0u, 1);
        }
        const std::uint32_t global = caveRead32(st, 0x00DFC044u);
        if (global != 0u) {
            st.eax = caveCall(st, 0x0081B300u, global, self, 0u, 0u, 1);
        }
    }
    const std::uint32_t player = caveRead32(st, self + 0x418u);
    if (player != 0u) {
        st.eax = caveCall(st, 0x007E16A0u, player, self, 0u, 0u, 1);
    }
    caveWrite8(st, self + 0x4D7u, 0u);
}

void runObjectPositionStep(CaveState& st, std::uint32_t outPtr) {
    const std::uint32_t self = kCaveImageBase;
    st.cleanup = 4;
    st.eax = outPtr;
    st.edx = 0u;
    auto writeOut = [&](float x, float y, float z) {
        caveWriteF32(st, outPtr, x);
        caveWriteF32(st, outPtr + 4u, y);
        caveWriteF32(st, outPtr + 8u, z);
    };
    if (caveRead8(st, self + 0x236u) != 0u) {
        writeOut(caveReadF32(st, self + 0x228u) - caveReadF32(st, self + 0x38u),
                 caveReadF32(st, self + 0x22Cu) - caveReadF32(st, self + 0x3Cu),
                 caveReadF32(st, self + 0x230u) - caveReadF32(st, self + 0x40u));
        return;
    }
    const std::uint32_t glob = caveRead32(st, 0x00DEBA64u);
    if (caveRead8(st, self + 0x234u) != 0u) {
        const std::uint32_t last = caveRead32(st, self + 0x218u);
        const std::uint32_t current = caveRead32(st, glob + 0x58u);
        st.edx = last;
        if (last < current) {
            const float inv = recipX87(current - last);
            const float x = (caveReadF32(st, self + 0x38u) + caveReadF32(st, self + 0x1F4u) * -1.0f) * inv;
            const float y = (caveReadF32(st, self + 0x3Cu) + caveReadF32(st, self + 0x204u) * -1.0f) * inv;
            const float z = (caveReadF32(st, self + 0x40u) + caveReadF32(st, self + 0x214u) * -1.0f) * inv;
            writeOut(x, y, z);
            return;
        }
    }
    if (caveRead8(st, self + 0x235u) != 0u) {
        const std::uint32_t last = caveRead32(st, self + 0x218u);
        const std::uint32_t current = caveRead32(st, glob + 0x58u);
        st.edx = last;
        if (last <= current) {
            const float inv = recipX87((current - last) + 1u);
            writeOut((caveReadF32(st, self + 0x38u) - caveReadF32(st, self + 0x21Cu)) * inv,
                     (caveReadF32(st, self + 0x3Cu) - caveReadF32(st, self + 0x220u)) * inv,
                     (caveReadF32(st, self + 0x40u) - caveReadF32(st, self + 0x224u)) * inv);
            return;
        }
    }
    writeOut(0.0f, 0.0f, 0.0f);
}

void runConsiderObjectIdle(CaveState& st, std::uint32_t allowBusy) {
    const std::uint32_t self = kCaveImageBase;
    st.eax = 0u;
    st.edx = 0u;
    st.cleanup = 4;
    const std::uint32_t ai = caveRead32(st, self + 0x374u);
    if (ai != 0u) {
        const std::uint32_t vt = caveRead32(st, ai);
        const std::uint32_t fn = caveRead32(st, vt + 0x214u);
        st.edx = fn;
        const std::uint32_t busy = caveCall(st, fn, ai, 0u, 0u, 0u, 0);
        st.eax = busy;
        if ((busy & 0xFFu) != 0u) {
            return;
        }
    }
    if ((caveRead8(st, self + 0x471u) & 1u) != 0u) {
        return;
    }
    if (((caveRead32(st, self + 0x80u) >> 30) & 1u) != 0u && (allowBusy & 0xFFu) == 0u) {
        st.eax = caveRead32(st, self + 0x80u) >> 30;
        return;
    }
    const std::uint32_t sub = caveRead32(st, self + 0x378u);
    if (sub != 0u && caveRead8(st, sub + 0xE0u) != 0u) {
        st.eax = sub;
        return;
    }
    const std::uint32_t ok = caveCall(st, 0x00638B70u, ai, self, 0u, 0u, 1);
    st.eax = ok;
    if ((ok & 0xFFu) == 0u) {
        return;
    }
    st.eax = caveCall(st, 0x00747C10u, self, 0u, 0u, 0u, 0);
}

void runReassignControllingPlayer(CaveState& st, std::uint32_t player) {
    const std::uint32_t self = kCaveImageBase;
    st.eax = 0u;
    st.edx = 0u;
    st.cleanup = 4;
    std::uint32_t ecx = caveRead32(st, self + 0x328u);
    std::uint32_t eax = ecx;
    if (ecx == 0u) {
        const std::uint32_t begin = caveRead32(st, self + 0x32Cu);
        const std::uint32_t end = caveRead32(st, self + 0x330u);
        if (begin == end) {
            st.eax = caveCall(st, 0x00687B80u, self, player, 0u, 0u, 1);
            return;
        }
        eax = caveRead32(st, begin);
    }
    if (eax == 0u) {
        st.eax = caveCall(st, 0x00687B80u, self, player, 0u, 0u, 1);
        return;
    }
    if (ecx == 0u) {
        const std::uint32_t begin = caveRead32(st, self + 0x32Cu);
        const std::uint32_t end = caveRead32(st, self + 0x330u);
        if (begin != end) {
            ecx = caveRead32(st, begin);
        }
    }
    const std::uint32_t vt = caveRead32(st, ecx);
    const std::uint32_t fn = caveRead32(st, vt + 0x84u);
    st.edx = vt;
    const std::uint32_t child = caveCall(st, fn, ecx, 0u, 0u, 0u, 0);
    st.eax = child;
    if (child == 0u) {
        st.eax = caveCall(st, 0x00687B80u, self, player, 0u, 0u, 1);
        return;
    }
    const std::uint32_t childVt = caveRead32(st, child);
    const std::uint32_t slot = caveRead32(st, childVt + 0x1D8u);
    st.edx = slot;
    st.eax = caveCall(st, slot, child, player, 0u, 0u, 2);
}

void runRegisterActiveModuleListener(CaveState& st, std::uint32_t listener) {
    const std::uint32_t self = kCaveImageBase;
    st.edx = 0u;
    st.cleanup = 4;
    st.eax = 0u;
    if (caveRead8(st, self + 0x138u) == 0u) {
        const std::uint32_t end = caveRead32(st, 0x00DF8A54u);
        const std::uint32_t cap = caveRead32(st, 0x00DF8A58u);
        if (end < cap) {
            caveWrite32(st, 0x00DF8A54u, end + 4u);
            if (end != 0u) {
                caveWrite32(st, end, self);
            }
        } else {
            caveCall(st, 0x008ADD50u, 0x00DF8A50u, end, self, 0u, 2);
        }
        caveWrite8(st, self + 0x138u, 1u);
    }
    const std::uint32_t localEnd = caveRead32(st, self + 0x140u);
    const std::uint32_t localCap = caveRead32(st, self + 0x144u);
    st.eax = localEnd;
    if (localEnd < localCap) {
        st.edx = localEnd + 4u;
        caveWrite32(st, self + 0x140u, localEnd + 4u);
        if (localEnd != 0u) {
            caveWrite32(st, localEnd, listener);
        }
    } else {
        st.eax = caveCall(st, 0x008ADD50u, self + 0x13Cu, localEnd, listener, 0u, 2);
    }
    caveWrite8(st, listener + 0x65u, 1u);
    if (caveRead32(st, listener + 0x0Cu) != 1u) {
        return;
    }
    const std::uint32_t tmpl = caveRead32(st, listener + 0x10u);
    if (tmpl == 0u || ((caveRead32(st, tmpl + 0xE0u) >> 10) & 1u) == 0u) {
        return;
    }
    const std::uint32_t owner = caveRead32(st, self + 8u);
    const std::uint32_t player = caveRead32(st, owner + 0x418u);
    st.eax = player;
    caveWrite32(st, player + 0x1318u, caveRead32(st, player + 0x1318u) - 1u);
}

void runNotifySixBucketListeners(CaveState& st) {
    const std::uint32_t self = kCaveImageBase;
    st.edx = 0u;
    st.cleanup = 0;
    st.eax = caveCall(st, 0x00734380u, self, 0u, 0u, 0u, 0);
    const std::uint32_t vt = caveRead32(st, self);
    const std::uint32_t virt = vt == 0u ? 0u : caveRead32(st, vt + 0x48u);
    const std::uint32_t arg = caveRead32(st, self + 0x10u);
    for (int bucket = 0; bucket < 6; ++bucket) {
        const std::uint32_t base = self + 0x34u + static_cast<std::uint32_t>(bucket) * 0x10u;
        const std::uint32_t begin = caveRead32(st, base);
        const std::uint32_t end = caveRead32(st, base + 4u);
        for (std::uint32_t cursor = begin; cursor != end; cursor += 4u) {
            const std::uint32_t elem = caveRead32(st, cursor);
            caveCall(st, 0x006A8490u, elem, arg, 0u, 0u, 1);
            st.edx = vt;
            caveCall(st, virt, self, elem, 0u, 0u, 1);
        }
    }
    st.eax = self + 0x94u;
}

void runPartitionModifierCount(CaveState& st, std::uint32_t spec) {
    const std::uint32_t self = kCaveImageBase;
    st.edx = 0u;
    st.cleanup = 4;
    const std::uint32_t owner = caveRead32(st, self + 8u);
    if (owner == 0u) {
        st.eax = 1u;
        return;
    }
    const std::uint32_t player = caveRead32(st, owner + 0x418u);
    st.edx = player;
    if (player == 0u || spec == 0u) {
        st.eax = 1u;
        return;
    }
    std::uint32_t count = 1u;
    const std::uint32_t mode = caveRead32(st, spec + 0x0Cu);
    if (mode == 1u) {
        count = caveCall(st, 0x0078BDA0u, caveRead32(st, spec + 0x10u), player, owner, 0xFFFFFFFFu, 4);
    } else if (mode == 2u) {
        count = caveCall(st, 0x0054B680u, caveRead32(st, spec + 0x14u), player, owner, 0u, 3);
    }
    const std::uint32_t mod = caveRead32(st, owner + 0x36Cu);
    if (mod != 0u) {
        const std::uint32_t applied = caveCall(st, 0x004D9CE0u, mod, 0xDu, 0u, 0u, 3);
        if ((applied & 0xFFu) != 0u) {
            const float divisor = caveReadF32(st, 0x00BD90A4u);
            const float scaled = static_cast<float>(static_cast<std::int32_t>(count)) / divisor;
            count = static_cast<std::uint32_t>(static_cast<std::int32_t>(scaled));
        }
    }
    st.eax = count;
}

void runAssignAiTargetRef(CaveState& st, std::uint32_t /*unused*/, std::uint32_t newTarget) {
    const std::uint32_t self = kCaveImageBase;
    st.eax = 0u;
    st.edx = 0u;
    st.cleanup = 8;
    const std::uint32_t old = caveRead32(st, self + 0x150u);
    if (old != 0u) {
        releaseRef(st, old);
        caveWrite32(st, self + 0x150u, 0u);
    }
    const std::uint32_t vt = caveRead32(st, self);
    const std::uint32_t fn = caveRead32(st, vt + 0x270u);
    caveWrite8(st, self + 0x428u, 0u);
    caveWrite8(st, self + 0x432u, 0u);
    st.edx = fn;
    st.eax = caveCall(st, fn, self, 0u, 0u, 0u, 0);
    if (newTarget != 0u) {
        caveWrite32(st, newTarget + 4u, caveRead32(st, newTarget + 4u) + 1u);
    }
    const std::uint32_t current = caveRead32(st, self + 0x150u);
    if (current != 0u) {
        releaseRef(st, current);
    }
    caveWrite32(st, self + 0x150u, newTarget);
    if (newTarget != 0u) {
        caveWrite32(st, newTarget + 4u, caveRead32(st, newTarget + 4u) + 1u);
    }
    st.eax = caveCall(st, 0x006B6900u, self, 0x00BF21D8u, newTarget, 0u, 3);
    if (newTarget != 0u) {
        releaseRef(st, newTarget);
    }
}

}  // namespace ra3::gamelogic
