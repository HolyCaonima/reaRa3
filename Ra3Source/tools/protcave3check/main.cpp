#include "ra3/gamelogic/ProtectedCaveLogic3.h"

#include <cstdint>
#include <cstring>
#include <iostream>
#include <vector>

namespace {

using ra3::gamelogic::CaveCallTrace;

void writeU8(std::vector<std::uint8_t>& buf, std::size_t off, std::uint8_t v) {
    buf[off] = v;
}

void writeU16(std::vector<std::uint8_t>& buf, std::size_t off, std::uint16_t v) {
    std::memcpy(buf.data() + off, &v, sizeof(v));
}

void writeU32(std::vector<std::uint8_t>& buf, std::size_t off, std::uint32_t v) {
    std::memcpy(buf.data() + off, &v, sizeof(v));
}

void writeF32(std::vector<std::uint8_t>& buf, std::size_t off, float v) {
    std::memcpy(buf.data() + off, &v, sizeof(v));
}

std::uint8_t readU8(const std::vector<std::uint8_t>& buf, std::size_t off) {
    return buf[off];
}

std::uint16_t readU16(const std::vector<std::uint8_t>& buf, std::size_t off) {
    std::uint16_t v = 0;
    std::memcpy(&v, buf.data() + off, sizeof(v));
    return v;
}

std::uint32_t readU32(const std::vector<std::uint8_t>& buf, std::size_t off) {
    std::uint32_t v = 0;
    std::memcpy(&v, buf.data() + off, sizeof(v));
    return v;
}

void printTrace(const char* caseId, const CaveCallTrace& trace, std::uint32_t retVal, std::uint32_t m0, std::uint32_t m1, std::uint32_t m2) {
    std::cout << caseId
              << " ret=" << retVal
              << " m0=" << m0
              << " m1=" << m1
              << " m2=" << m2
              << " count=" << trace.count;
    for (std::uint32_t i = 0; i < trace.count; ++i) {
        const auto& e = trace.entries[i];
        std::cout << " c" << i << "=" << e.targetVa
                  << ":" << e.ecx
                  << ":" << e.argCount
                  << ":" << e.arg0
                  << ":" << e.arg1
                  << ":" << e.arg2;
    }
    std::cout << "\n";
}

// 1. 0x004CB260
void emit4CB260() {
    struct Spec {
        const char* id;
        std::uint32_t selfVa;
        std::uint32_t initDword0;
        std::uint32_t arg0;
    };
    const Spec specs[] = {
        {"init4cb260_a", 0x60010000u, 0xDEADBEEFu, 0x60010100u},
        {"init4cb260_b", 0x60010200u, 0x12345678u, 0u},
        {"init4cb260_c", 0x60010400u, 0xFFFFFFFFu, 0x60010500u},
    };
    for (const auto& s : specs) {
        std::vector<std::uint8_t> self(0x20, 0);
        writeU32(self, 0x00, s.initDword0);
        CaveCallTrace trace{};
        const std::uint32_t ret = ra3::gamelogic::runInitZeroAndCall4CB200(
            self.data(), s.selfVa, s.arg0, &trace);
        printTrace(s.id, trace, ret, readU32(self, 0x00), 0u, 0u);
    }
}

// 2. 0x004D66D0
void emit4D66D0() {
    struct Spec {
        const char* id;
        std::uint32_t childVa;
        std::int32_t initRef;
        std::uint32_t dtorVa;
    };
    const Spec specs[] = {
        {"dtor4d66d0_null", 0u, 0, 0x00601000u},
        {"dtor4d66d0_keep", 0x60020100u, 3, 0x00601000u},
        {"dtor4d66d0_free", 0x60020200u, 1, 0x00601000u},
    };
    for (const auto& s : specs) {
        std::vector<std::uint8_t> self(0x10, 0);
        std::vector<std::uint8_t> child(0x10, 0);
        writeU32(self, 0x00, 0x11111111u);
        writeU32(self, 0x04, s.childVa);
        writeU32(child, 0x04, static_cast<std::uint32_t>(s.initRef));
        CaveCallTrace trace{};
        ra3::gamelogic::runDestroyObjectBDB8E8(
            self.data(),
            s.childVa,
            s.childVa ? child.data() : nullptr,
            s.dtorVa,
            &trace);
        const std::uint32_t childRefAfter = s.childVa ? readU32(child, 0x04) : 0u;
        printTrace(s.id, trace, 0u, readU32(self, 0x00), childRefAfter, 0u);
    }
}

// 3. 0x004D99E0
void emit4D99E0() {
    struct Spec {
        const char* id;
        std::uint32_t selfVa;
        std::uint32_t reqSize;
        std::uint32_t maxPoolSize;
        std::uint32_t largeRetVa;
        std::uint32_t poolVa;
        std::uint32_t initFreeHeadVa;
        std::uint32_t growRetByte;
        std::uint32_t grownNodeVa;
        std::uint32_t nextNodeVa;
    };
    const Spec specs[] = {
        {"alloc4d99e0_large", 0x60030000u, 0x200u, 0x100u, 0x60039900u, 0x60030100u, 0u, 0u, 0u, 0u},
        {"alloc4d99e0_ready", 0x60030000u, 0x0Cu, 0x100u, 0u, 0x60030100u, 0x60030200u, 1u, 0x60030200u, 0x60030300u},
        {"alloc4d99e0_grow", 0x60030000u, 0x20u, 0x100u, 0u, 0x60030100u, 0u, 1u, 0x60030400u, 0x60030500u},
    };
    for (const auto& s : specs) {
        std::vector<std::uint8_t> pool(0x20, 0);
        std::vector<std::uint8_t> headNode(0x10, 0);
        writeU32(pool, 0x10, s.initFreeHeadVa);
        writeU32(headNode, 0x00, s.nextNodeVa);
        CaveCallTrace trace{};
        const std::uint32_t ret = ra3::gamelogic::runAllocateFromPowerOfTwoPool(
            s.selfVa,
            s.reqSize,
            s.maxPoolSize,
            s.largeRetVa,
            s.poolVa,
            pool.data(),
            s.growRetByte,
            s.grownNodeVa,
            headNode.data(),
            &trace);
        printTrace(s.id, trace, ret, readU32(pool, 0x10), 0u, 0u);
    }
}

// 4. 0x004DA710
void emit4DA710() {
    struct Spec {
        const char* id;
        std::uint32_t selfVa;
        std::uint32_t dirtyWord;
    };
    const Spec specs[] = {
        {"ctor4da710_a", 0x60040000u, 0xCAFEBABEu},
        {"ctor4da710_b", 0x60041000u, 0xFFFFFFFFu},
        {"ctor4da710_c", 0x60042000u, 0x13572468u},
    };
    for (const auto& s : specs) {
        std::vector<std::uint8_t> self(0xA0, 0x5A);
        writeU32(self, 0x8C, s.dirtyWord);
        writeU32(self, 0x90, s.dirtyWord);
        writeU32(self, 0x98, s.dirtyWord);
        CaveCallTrace trace{};
        const std::uint32_t ret = ra3::gamelogic::runConstructObjectBDC368(
            self.data(), s.selfVa, &trace);
        const std::uint32_t m0 = readU32(self, 0x00);
        const std::uint32_t m1 = readU32(self, 0x88);
        const std::uint32_t m2 = readU32(self, 0x8C) | readU32(self, 0x90) | readU8(self, 0x94) | readU32(self, 0x98);
        printTrace(s.id, trace, ret, m0, m1, m2);
    }
}

// 5. 0x004E5C90
void emit4E5C90() {
    constexpr std::uint32_t kLocalOutVa = 0x7F0FFEF8u;
    // Case 1: both lists empty
    {
        const std::uint32_t selfVa = 0x60050000u;
        std::vector<std::uint8_t> self(0xB0, 0);
        writeU32(self, 0x8C, 0u);
        writeU32(self, 0x94, selfVa + 0x94u);
        writeU32(self, 0x98, selfVa + 0x94u);
        writeU8(self, 0xAA, 1u);
        CaveCallTrace trace{};
        ra3::gamelogic::runClearLists8CAnd94(
            self.data(), selfVa, kLocalOutVa, nullptr, 0u, &trace);
        printTrace("clear4e5c90_empty", trace, 0u, readU32(self, 0x8C), readU32(self, 0x94), readU8(self, 0xAA));
    }
    // Case 2: 2 nodes in list 8C, empty list 94
    {
        const std::uint32_t selfVa = 0x60051000u;
        std::vector<std::uint8_t> self(0xB0, 0);
        std::vector<std::uint8_t> n1(0x20, 0);
        std::vector<std::uint8_t> n2(0x20, 0);
        writeU32(n1, 0x14, 0x60051200u);
        writeU32(n2, 0x14, 0u);
        writeU32(self, 0x8C, 0x60051100u);
        writeU32(self, 0x94, selfVa + 0x94u);
        writeU32(self, 0x98, selfVa + 0x94u);
        writeU8(self, 0xAA, 1u);
        const ra3::gamelogic::ListNode8C94Entry entries[] = {
            {0x60051100u, n1.data(), 0u, 0u},
            {0x60051200u, n2.data(), 0u, 0u},
        };
        CaveCallTrace trace{};
        ra3::gamelogic::runClearLists8CAnd94(
            self.data(), selfVa, kLocalOutVa, entries, 2u, &trace);
        printTrace("clear4e5c90_list8c", trace, 0u, readU32(self, 0x8C), readU32(self, 0x94), readU8(self, 0xAA));
    }
    // Case 3: 1 node in list 8C + 2 nodes in list 94 (one with payload, one without)
    {
        const std::uint32_t selfVa = 0x60052000u;
        const std::uint32_t sentVa = selfVa + 0x94u;
        std::vector<std::uint8_t> self(0xB0, 0);
        std::vector<std::uint8_t> n8c(0x20, 0);
        std::vector<std::uint8_t> m1(0x20, 0);
        std::vector<std::uint8_t> m2(0x20, 0);
        writeU32(n8c, 0x14, 0u);
        writeU32(m1, 0x00, 0x60052300u);
        writeU32(m1, 0x04, sentVa);
        writeU32(m1, 0x08, 0x60052900u);
        writeU32(m2, 0x00, sentVa);
        writeU32(m2, 0x04, 0x60052200u);
        writeU32(m2, 0x08, 0u);
        writeU32(self, 0x8C, 0x60052100u);
        writeU32(self, 0x94, 0x60052200u);
        writeU32(self, 0x98, 0x60052300u);
        writeU8(self, 0xAA, 7u);
        const ra3::gamelogic::ListNode8C94Entry entries[] = {
            {0x60052100u, n8c.data(), 0u, 0u},
            {0x60052200u, m1.data(), 0x77u, 0x88u},
            {0x60052300u, m2.data(), 0u, 0u},
        };
        CaveCallTrace trace{};
        ra3::gamelogic::runClearLists8CAnd94(
            self.data(), selfVa, kLocalOutVa, entries, 3u, &trace);
        printTrace("clear4e5c90_both", trace, 0u, readU32(self, 0x8C), readU32(self, 0x94), readU8(self, 0xAA));
    }
}

// 6. 0x004EDD70
void emit4EDD70() {
    // Case 1: targetVa == 0
    {
        std::vector<std::uint8_t> self(0x160, 0);
        writeU32(self, 0x138, 0x60060138u);
        writeU32(self, 0x13C, 0x60060138u);
        CaveCallTrace trace{};
        ra3::gamelogic::runDeactivateAndEnqueueObject138(
            self.data(), 0x60060000u, 0u, nullptr, nullptr, 0u, 0u, nullptr, nullptr, &trace);
        printTrace("deact4edd70_null", trace, 0u, readU32(self, 0x138), readU32(self, 0x13C), 0u);
    }
    // Case 2: target [edi+0x80] & 1 != 0 (early exit)
    {
        std::vector<std::uint8_t> self(0x160, 0);
        std::vector<std::uint8_t> target(0x320, 0);
        writeU32(self, 0x138, 0x60060138u);
        writeU32(self, 0x13C, 0x60060138u);
        writeU8(target, 0x80, 0x01u);
        CaveCallTrace trace{};
        ra3::gamelogic::runDeactivateAndEnqueueObject138(
            self.data(), 0x60060000u, 0x60061000u, target.data(), nullptr, 0u, 0u, nullptr, nullptr, &trace);
        printTrace("deact4edd70_flagged", trace, 0u, readU32(self, 0x138), readU32(self, 0x13C), 0u);
    }
    // Case 3: active target with 2 modules (one returning subObj, one returning 0)
    {
        const std::uint32_t selfVa = 0x60060000u;
        const std::uint32_t targetVa = 0x60061000u;
        const std::uint32_t allocNodeVa = 0x60062000u;
        std::vector<std::uint8_t> self(0x160, 0);
        std::vector<std::uint8_t> target(0x320, 0);
        std::vector<std::uint8_t> allocNode(0x10, 0);
        writeU32(self, 0x138, selfVa + 0x138u);
        writeU32(self, 0x13C, selfVa + 0x138u);
        writeU8(target, 0x80, 0x02u);
        const ra3::gamelogic::Module310DeactivateEntry mods[] = {
            {0x60063000u, 0x00603010u, 0x60063100u, 0x00603020u},
            {0x60063200u, 0x00603030u, 0u, 0u},
        };
        CaveCallTrace trace{};
        ra3::gamelogic::runDeactivateAndEnqueueObject138(
            self.data(),
            selfVa,
            targetVa,
            target.data(),
            mods,
            2u,
            allocNodeVa,
            allocNode.data(),
            nullptr,
            &trace);
        printTrace("deact4edd70_active", trace, 0u, readU32(self, 0x138), readU32(self, 0x13C), readU32(allocNode, 0x08));
    }
}

// 7. 0x00504510
void emit504510() {
    // Case 1: self == other
    {
        const std::uint32_t selfVa = 0x60070000u;
        std::vector<std::uint8_t> self(0x08, 0);
        writeU32(self, 0x00, 0x60070100u);
        CaveCallTrace trace{};
        const std::uint32_t ret = ra3::gamelogic::runAssignRefObjectAtOffset94(
            self.data(), selfVa, self.data(), selfVa, 0u, nullptr, 0u, nullptr, 0u, &trace);
        printTrace("assign504510_same", trace, ret, readU32(self, 0x00), 0u, 0u);
    }
    // Case 2: newObj incremented, oldObj decremented to > 0
    {
        const std::uint32_t selfVa = 0x60070000u;
        const std::uint32_t otherVa = 0x60070010u;
        const std::uint32_t newObjVa = 0x60071000u;
        const std::uint32_t oldObjVa = 0x60072000u;
        std::vector<std::uint8_t> self(0x08, 0);
        std::vector<std::uint8_t> other(0x08, 0);
        std::vector<std::uint8_t> newObj(0xA0, 0);
        std::vector<std::uint8_t> oldObj(0xA0, 0);
        writeU32(self, 0x00, oldObjVa);
        writeU32(other, 0x00, newObjVa);
        writeU32(newObj, 0x98, 2u);
        writeU32(oldObj, 0x98, 2u);
        CaveCallTrace trace{};
        const std::uint32_t ret = ra3::gamelogic::runAssignRefObjectAtOffset94(
            self.data(), selfVa, other.data(), otherVa, newObjVa, newObj.data(), oldObjVa, oldObj.data(), 0x00607010u, &trace);
        printTrace("assign504510_keep", trace, ret, readU32(self, 0x00), readU32(newObj, 0x98), readU32(oldObj, 0x98));
    }
    // Case 3: newObj incremented, oldObj decremented to 0 -> dtor called on oldObj+0x94
    {
        const std::uint32_t selfVa = 0x60070000u;
        const std::uint32_t otherVa = 0x60070010u;
        const std::uint32_t newObjVa = 0x60071000u;
        const std::uint32_t oldObjVa = 0x60072000u;
        std::vector<std::uint8_t> self(0x08, 0);
        std::vector<std::uint8_t> other(0x08, 0);
        std::vector<std::uint8_t> newObj(0xA0, 0);
        std::vector<std::uint8_t> oldObj(0xA0, 0);
        writeU32(self, 0x00, oldObjVa);
        writeU32(other, 0x00, newObjVa);
        writeU32(newObj, 0x98, 4u);
        writeU32(oldObj, 0x98, 1u);
        CaveCallTrace trace{};
        const std::uint32_t ret = ra3::gamelogic::runAssignRefObjectAtOffset94(
            self.data(), selfVa, other.data(), otherVa, newObjVa, newObj.data(), oldObjVa, oldObj.data(), 0x00607010u, &trace);
        printTrace("assign504510_free", trace, ret, readU32(self, 0x00), readU32(newObj, 0x98), readU32(oldObj, 0x98));
    }
}

// 8. 0x0050A9C0
void emit50A9C0() {
    constexpr std::uint32_t kOutIterStackVa = 0x7F0FFEFCu;
    struct Spec {
        const char* id;
        std::uint32_t index30;
        bool found;
        float scaleVal;
        float denom10;
        float factor1C;
    };
    const Spec specs[] = {
        {"scale50a9c0_found_pos", 2u, true, 0.75f, 1.5f, 4.0f},
        {"scale50a9c0_found_zero", 1u, true, 0.50f, 0.0f, 3.0f},
        {"scale50a9c0_fallback", 3u, false, 0.0f, 2.25f, 5.5f},
    };
    for (const auto& s : specs) {
        const std::uint32_t targetVa = 0x60080000u;
        const std::uint32_t sub0CVa = 0x60080100u;
        const std::uint32_t vt08TargetVa = 0x00608010u;
        const std::uint32_t keyReturnVa = 0x60080200u;
        const std::uint32_t entryBase = 0x00DEE290u + s.index30 * 0x1FCu;
        const std::uint32_t foundNodeVa = s.found ? 0x60080300u : (entryBase + 0x1E4u);
        std::vector<std::uint8_t> target(0x40, 0);
        std::vector<std::uint8_t> sub0C(0x30, 0);
        writeU32(target, 0x0C, sub0CVa);
        writeU32(target, 0x30, s.index30);
        writeF32(sub0C, 0x10, s.denom10);
        writeF32(sub0C, 0x1C, s.factor1C);
        CaveCallTrace trace{};
        ra3::gamelogic::runApplyIndexedTableScaleToAudioObject(
            targetVa,
            target.data(),
            sub0C.data(),
            vt08TargetVa,
            keyReturnVa,
            kOutIterStackVa,
            foundNodeVa,
            s.scaleVal,
            &trace);
        printTrace(s.id, trace, 0u, s.index30, foundNodeVa, 0u);
    }
}

// 9. 0x00520120
void emit520120() {
    struct Spec {
        const char* id;
        std::uint8_t init13C;
        std::uint32_t arg0;
        std::uint32_t listenerVa;
        std::uint32_t listenerVt04TargetVa;
    };
    const Spec specs[] = {
        {"bit520120_same", 0x15u, 1u, 0x60090200u, 0x00609010u},
        {"bit520120_set_notify", 0x05u, 1u, 0x60090200u, 0x00609010u},
        {"bit520120_clear_nolistener", 0x15u, 0u, 0u, 0x00609010u},
    };
    for (const auto& s : specs) {
        const std::uint32_t selfVa = 0x60090000u;
        std::vector<std::uint8_t> self(0x3A0, 0);
        writeU8(self, 0x13C, s.init13C);
        writeU32(self, 0x394, s.listenerVa);
        CaveCallTrace trace{};
        ra3::gamelogic::runSetDrawableBit13CAndNotify394(
            self.data(), selfVa, s.arg0, s.listenerVt04TargetVa, &trace);
        printTrace(s.id, trace, 0u, readU8(self, 0x13C), s.listenerVa, 0u);
    }
}

// 10. 0x00523E60
void emit523E60() {
    // Case 1: empty array at [self+0x178]
    {
        std::vector<std::uint8_t> self(0x180, 0);
        writeU32(self, 0x178, 0x600A0100u);
        const ra3::gamelogic::DrawableModule178QueryEntry entries[] = {
            {0u, 0u, 0u, 0u, 0u},
        };
        CaveCallTrace trace{};
        const std::uint8_t ret = ra3::gamelogic::runQueryDrawableModules178(
            self.data(), 0x111u, 0x222u, entries, 1u, &trace);
        printTrace("query523e60_empty", trace, ret, 0u, 0u, 0u);
    }
    // Case 2: 2 modules, first has subObj==0, second returns 0 -> returns 0
    {
        std::vector<std::uint8_t> self(0x180, 0);
        writeU32(self, 0x178, 0x600A0100u);
        const ra3::gamelogic::DrawableModule178QueryEntry entries[] = {
            {0x600A0200u, 0x0060A010u, 0u, 0u, 0u},
            {0x600A0300u, 0x0060A020u, 0x600A0400u, 0x0060A030u, 0u},
        };
        CaveCallTrace trace{};
        const std::uint8_t ret = ra3::gamelogic::runQueryDrawableModules178(
            self.data(), 0x111u, 0x222u, entries, 2u, &trace);
        printTrace("query523e60_miss", trace, ret, 0u, 0u, 0u);
    }
    // Case 3: first module returns true -> short-circuits and returns 1
    {
        std::vector<std::uint8_t> self(0x180, 0);
        writeU32(self, 0x178, 0x600A0100u);
        const ra3::gamelogic::DrawableModule178QueryEntry entries[] = {
            {0x600A0200u, 0x0060A010u, 0x600A0400u, 0x0060A030u, 1u},
            {0x600A0300u, 0x0060A020u, 0x600A0500u, 0x0060A040u, 1u},
        };
        CaveCallTrace trace{};
        const std::uint8_t ret = ra3::gamelogic::runQueryDrawableModules178(
            self.data(), 0x333u, 0x444u, entries, 2u, &trace);
        printTrace("query523e60_hit", trace, ret, 0u, 0u, 0u);
    }
}

// 11. 0x005242B0
void emit5242B0() {
    struct Spec {
        const char* id;
        std::uint32_t arg0;
        std::int32_t idxRet;
    };
    const Spec specs[] = {
        {"mark5242b0_neg", 0x10u, -1},
        {"mark5242b0_zero", 0x20u, 0},
        {"mark5242b0_three", 0x30u, 3},
    };
    for (const auto& s : specs) {
        const std::uint32_t selfVa = 0x600B0000u;
        std::vector<std::uint8_t> self(0x240, 0);
        std::vector<std::uint8_t> arr224(8, 0);
        std::vector<std::uint8_t> arr234(8, 0);
        CaveCallTrace trace{};
        ra3::gamelogic::runMarkDrawableSlotFlags224And234(
            self.data(), selfVa, s.arg0, s.idxRet, arr224.data(), arr234.data(), &trace);
        const std::size_t probeIdx = (s.idxRet >= 0) ? static_cast<std::size_t>(s.idxRet) : 0u;
        printTrace(s.id, trace, 0u, readU8(arr224, probeIdx), readU8(arr234, probeIdx), 0u);
    }
}

// 12. 0x0052D6B0
void emit52D6B0() {
    constexpr std::uint32_t kLocalTempVa = 0x7F0FFED4u;
    struct Spec {
        const char* id;
        std::uint32_t arg0;
        std::uint8_t global13F;
        std::uint32_t obj138Va;
        std::uint8_t helper7886B0Ret;
    };
    const Spec specs[] = {
        {"vis52d6b0_global_on", 0x12345600u, 1u, 0x600C0200u, 0u},
        {"vis52d6b0_helper_false", 0xABCDEF00u, 0u, 0x600C0200u, 0u},
        {"vis52d6b0_helper_true", 0xABCDEF00u, 0u, 0x600C0200u, 1u},
    };
    for (const auto& s : specs) {
        const std::uint32_t selfVa = 0x600C0000u;
        std::vector<std::uint8_t> self(0x390, 0);
        writeU32(self, 0x138, s.obj138Va);
        CaveCallTrace trace{};
        ra3::gamelogic::runSetDrawableField384AndSyncVisibility(
            self.data(),
            selfVa,
            s.arg0,
            s.global13F,
            kLocalTempVa,
            0x600C0900u,
            s.helper7886B0Ret,
            &trace);
        printTrace(s.id, trace, 0u, readU32(self, 0x384), 0u, 0u);
    }
}

// 13. 0x00535410
void emit535410() {
    struct Spec {
        const char* id;
        std::uint32_t outPtrVa;
        std::uint32_t arg1;
        std::uint32_t allocVa;
    };
    const Spec specs[] = {
        {"create535410_ok1", 0x600D0000u, 0x11223344u, 0x600D0100u},
        {"create535410_ok2", 0x600D0000u, 0x99887766u, 0x600D0200u},
        {"create535410_null", 0x600D0000u, 0x55555555u, 0u},
    };
    for (const auto& s : specs) {
        std::vector<std::uint8_t> outPtr(0x08, 0xFF);
        std::vector<std::uint8_t> allocObj(0x20, 0xAA);
        CaveCallTrace trace{};
        const std::uint32_t ret = ra3::gamelogic::runCreateRefObjectBE03E4(
            outPtr.data(),
            s.outPtrVa,
            s.arg1,
            s.allocVa,
            s.allocVa ? allocObj.data() : nullptr,
            &trace);
        const std::uint32_t m0 = readU32(outPtr, 0x00);
        const std::uint32_t m1 = s.allocVa ? readU32(allocObj, 0x00) : 0u;
        const std::uint32_t m2 = s.allocVa ? (readU32(allocObj, 0x04) ^ readU32(allocObj, 0x08) ^ readU32(allocObj, 0x0C)) : 0u;
        printTrace(s.id, trace, ret, m0, m1, m2);
    }
}

// 14. 0x00537A40
void emit537A40() {
    constexpr std::uint32_t kLocalEventVa = 0x7F0FFEECu;
    struct Spec {
        const char* id;
        std::uint8_t initFlags13D;
        std::uint32_t obj138Va;
    };
    const Spec specs[] = {
        {"rem537a40_unflagged", 0x20u, 0x600E0500u},
        {"rem537a40_with138", 0x65u, 0x600E0500u},
        {"rem537a40_no138", 0x41u, 0u},
    };
    for (const auto& s : specs) {
        const std::uint32_t selfVa = 0x600E0000u;
        const std::uint32_t targetVa = 0x600E0100u;
        const std::uint32_t nodeVa = 0x600E0200u;
        std::vector<std::uint8_t> self(0x80, 0);
        std::vector<std::uint8_t> target(0x150, 0);
        std::vector<std::uint8_t> globalDEEA50(0xD0, 0);
        std::vector<std::uint8_t> node(0x10, 0);
        std::vector<std::uint8_t> pool(0x20, 0);
        std::vector<std::uint8_t> localEvent(0x10, 0);
        writeU32(self, 0x50, nodeVa);
        writeU32(self, 0x54, nodeVa);
        writeU32(self, 0x5C, 4u);
        writeU8(target, 0x13D, s.initFlags13D);
        writeU32(target, 0x138, s.obj138Va);
        writeU32(globalDEEA50, 0xBC, 0x99u);
        writeU32(node, 0x00, selfVa + 0x50u);
        writeU32(node, 0x04, selfVa + 0x50u);
        writeU32(node, 0x08, targetVa);
        writeU32(pool, 0x10, 0x600E0900u);
        CaveCallTrace trace{};
        ra3::gamelogic::runRemoveFlaggedDrawableFromList50(
            self.data(),
            selfVa,
            target.data(),
            targetVa,
            0x7788u,
            globalDEEA50.data(),
            node.data(),
            nodeVa,
            self.data() + 0x50,
            self.data() + 0x50,
            pool.data(),
            kLocalEventVa,
            localEvent.data(),
            &trace);
        printTrace(s.id, trace, 0u, readU8(target, 0x13D), readU32(self, 0x5C), readU32(pool, 0x10));
    }
}

// 15. 0x00538440
void emit538440() {
    const std::uint32_t selfVa = 0x600F0000u;
    const std::uint32_t sourceVa = 0x600F0100u;
    // Case 1: 0 items
    {
        std::vector<std::uint8_t> self(0x20, 0);
        CaveCallTrace trace{};
        const std::uint32_t ret = ra3::gamelogic::runConstructObjectBE082C(
            self.data(), selfVa, sourceVa, nullptr, 0u, &trace);
        printTrace("ctor538440_empty", trace, ret, readU32(self, 0x00), readU32(self, 0x04), readU32(self, 0x08));
    }
    // Case 2: 1 matching item
    {
        std::vector<std::uint8_t> self(0x20, 0);
        std::vector<std::uint8_t> node1(0x10, 0);
        const ra3::gamelogic::FactoryItem538440Entry items[] = {
            {0x600F0200u, 10u, 10u, 0x600F0300u, 0x0060F010u, 0x600F0400u, 0x600F0500u, node1.data()},
        };
        CaveCallTrace trace{};
        const std::uint32_t ret = ra3::gamelogic::runConstructObjectBE082C(
            self.data(), selfVa, sourceVa, items, 1u, &trace);
        printTrace("ctor538440_one", trace, ret, readU32(self, 0x04), readU32(self, 0x08), readU32(node1, 0x08));
    }
    // Case 3: 2 items (first unmatched, second matched)
    {
        std::vector<std::uint8_t> self(0x20, 0);
        std::vector<std::uint8_t> node2(0x10, 0);
        const ra3::gamelogic::FactoryItem538440Entry items[] = {
            {0x600F0200u, 5u, 10u, 0u, 0u, 0u, 0u, nullptr},
            {0x600F0210u, 10u, 10u, 0x600F0300u, 0x0060F010u, 0x600F0410u, 0x600F0510u, node2.data()},
        };
        CaveCallTrace trace{};
        const std::uint32_t ret = ra3::gamelogic::runConstructObjectBE082C(
            self.data(), selfVa, sourceVa, items, 2u, &trace);
        printTrace("ctor538440_mixed", trace, ret, readU32(self, 0x04), readU32(self, 0x08), readU32(node2, 0x08));
    }
}

// 16. 0x0055B530
void emit55B530() {
    struct Spec {
        const char* id;
        std::int32_t initRef;
    };
    const Spec specs[] = {
        {"dtor55b530_allnull", 0},
        {"dtor55b530_keep", 2},
        {"dtor55b530_release", 1},
    };
    for (const auto& s : specs) {
        const std::uint32_t selfVa = 0x60100000u;
        std::vector<std::uint8_t> self(0xA0, 0);
        std::vector<std::uint8_t> o20(0x10, 0), o1C(0x10, 0), o14(0xA0, 0), o0C(0x10, 0), o04(0x10, 0);
        const bool hasObjs = (s.initRef > 0);
        const std::uint32_t v20 = hasObjs ? 0x60100100u : 0u;
        const std::uint32_t v1C = hasObjs ? 0x60100200u : 0u;
        const std::uint32_t v14 = hasObjs ? 0x60100300u : 0u;
        const std::uint32_t v0C = hasObjs ? 0x60100400u : 0u;
        const std::uint32_t v04 = hasObjs ? 0x60100500u : 0u;
        writeU32(self, 0x20, v20);
        writeU32(self, 0x1C, v1C);
        writeU32(self, 0x14, v14);
        writeU32(self, 0x0C, v0C);
        writeU32(self, 0x04, v04);
        writeU32(o20, 0x04, static_cast<std::uint32_t>(s.initRef));
        writeU32(o1C, 0x04, static_cast<std::uint32_t>(s.initRef));
        writeU32(o14, 0x98, static_cast<std::uint32_t>(s.initRef));
        writeU32(o0C, 0x04, static_cast<std::uint32_t>(s.initRef));
        writeU32(o04, 0x04, static_cast<std::uint32_t>(s.initRef));
        CaveCallTrace trace{};
        ra3::gamelogic::runDestroyAudioEventBDF2C4(
            self.data(),
            selfVa,
            {v20, hasObjs ? o20.data() : nullptr, 0x00610010u},
            {v1C, hasObjs ? o1C.data() : nullptr, 0x00610020u},
            {v14, hasObjs ? o14.data() : nullptr, 0x00610030u},
            {v0C, hasObjs ? o0C.data() : nullptr, 0x00610040u},
            {v04, hasObjs ? o04.data() : nullptr, 0x00610050u},
            &trace);
        printTrace(s.id, trace, 0u, readU32(self, 0x00), hasObjs ? readU32(o14, 0x98) : 0u, hasObjs ? readU32(o0C, 0x04) : 0u);
    }
}

// 17. 0x0055CC60
void emit55CC60() {
    struct Spec {
        const char* id;
        std::uint32_t selfVa;
        std::uint8_t fillByte;
    };
    const Spec specs[] = {
        {"ctor55cc60_a", 0x60110000u, 0xAAu},
        {"ctor55cc60_b", 0x60111000u, 0x55u},
        {"ctor55cc60_c", 0x60112000u, 0xFFu},
    };
    for (const auto& s : specs) {
        std::vector<std::uint8_t> self(0x120, s.fillByte);
        CaveCallTrace trace{};
        const std::uint32_t ret = ra3::gamelogic::runConstructObjectBE2CD8(
            self.data(), s.selfVa, &trace);
        const std::uint32_t m0 = readU32(self, 0x00) ^ readU32(self, 0x04);
        const std::uint32_t m1 = readU32(self, 0x10) ^ readU32(self, 0x54) ^ readU32(self, 0x70);
        const std::uint32_t m2 = readU32(self, 0xCC) ^ readU32(self, 0xD0) ^ readU32(self, 0xE8) ^ readU32(self, 0x114);
        printTrace(s.id, trace, ret, m0, m1, m2);
    }
}

// 18. 0x005838F0
void emit5838F0() {
    constexpr std::uint32_t kLockTempVa = 0x7F0FFEF7u;
    struct Spec {
        const char* id;
        std::int32_t initRef;
    };
    const Spec specs[] = {
        {"rel5838f0_null", 0},
        {"rel5838f0_keep", 3},
        {"rel5838f0_free", 1},
    };
    for (const auto& s : specs) {
        std::vector<std::uint8_t> self(0x6C80, 0);
        std::vector<std::uint8_t> o68(0x10, 0), o6C(0x10, 0);
        const bool hasObjs = (s.initRef > 0);
        const std::uint32_t v68 = hasObjs ? 0x60127100u : 0u;
        const std::uint32_t v6C = hasObjs ? 0x60127200u : 0u;
        writeU32(self, 0x6C68, v68);
        writeU32(self, 0x6C6C, v6C);
        writeU8(self, 0x2C, 1u);
        writeU32(o68, 0x04, static_cast<std::uint32_t>(s.initRef));
        writeU32(o6C, 0x04, static_cast<std::uint32_t>(s.initRef));
        CaveCallTrace trace{};
        ra3::gamelogic::runReleaseLockedObjects6C68And6C6C(
            self.data(),
            kLockTempVa,
            {v68, hasObjs ? o68.data() : nullptr, 0x00612010u},
            {v6C, hasObjs ? o6C.data() : nullptr, 0x00612020u},
            0x00612030u,
            &trace);
        printTrace(s.id, trace, 0u, readU32(self, 0x6C68) | readU32(self, 0x6C6C), readU8(self, 0x2C), hasObjs ? readU32(o68, 0x04) : 0u);
    }
}

// 19. 0x005A4ED0
void emit5A4ED0() {
    const std::uint32_t selfVa = 0x60130000u;
    // Case 1: global [0x00DF6F94] == 0
    {
        std::vector<std::uint8_t> self(0x540, 0);
        writeU32(self, 0x524, 0x60130100u);
        writeU32(self, 0x528, 0x60130108u);
        CaveCallTrace trace{};
        ra3::gamelogic::runReleaseAndClearHandleVector524(
            self.data(), selfVa, 0u, nullptr, 0u, 0x60130100u, &trace);
        printTrace("clear5a4ed0_disabled", trace, 0u, readU32(self, 0x524), readU32(self, 0x528), 0u);
    }
    // Case 2: empty vector (begin == end)
    {
        std::vector<std::uint8_t> self(0x540, 0);
        writeU32(self, 0x524, 0x60130100u);
        writeU32(self, 0x528, 0x60130100u);
        CaveCallTrace trace{};
        ra3::gamelogic::runReleaseAndClearHandleVector524(
            self.data(), selfVa, 0x60139000u, nullptr, 0u, 0x60130100u, &trace);
        printTrace("clear5a4ed0_empty", trace, 0u, readU32(self, 0x524), readU32(self, 0x528), 0u);
    }
    // Case 3: 3 entries (1 sentinel/null, 2 active handles)
    {
        std::vector<std::uint8_t> self(0x540, 0);
        writeU32(self, 0x524, 0x60130100u);
        writeU32(self, 0x528, 0x6013010Cu);
        const std::uint32_t handles[] = {0x60130200u, 0u, 0x60130300u};
        CaveCallTrace trace{};
        ra3::gamelogic::runReleaseAndClearHandleVector524(
            self.data(), selfVa, 0x60139000u, handles, 3u, 0x6013010Cu, &trace);
        printTrace("clear5a4ed0_three", trace, 0u, readU32(self, 0x524), readU32(self, 0x528), 0u);
    }
}

// 20. 0x005C5D70
void emit5C5D70() {
    struct Spec {
        const char* id;
        std::uint32_t selfVa;
        std::uint32_t field10;
    };
    const Spec specs[] = {
        {"dtor5c5d70_a", 0x60140000u, 0x60140100u},
        {"dtor5c5d70_b", 0x60141000u, 0u},
        {"dtor5c5d70_c", 0x60142000u, 0x60142500u},
    };
    for (const auto& s : specs) {
        std::vector<std::uint8_t> self(0x30, 0);
        writeU32(self, 0x10, s.field10);
        CaveCallTrace trace{};
        ra3::gamelogic::runDestroyObjectBE97D4(self.data(), s.selfVa, &trace);
        printTrace(s.id, trace, 0u, readU32(self, 0x00), readU32(self, 0x10), 0u);
    }
}

// 21. 0x005EF110
void emit5EF110() {
    struct Spec {
        const char* id;
        std::uint32_t selfVa;
        std::uint8_t fillByte;
    };
    const Spec specs[] = {
        {"ctor5ef110_a", 0x60150000u, 0xCCu},
        {"ctor5ef110_b", 0x60151000u, 0x77u},
        {"ctor5ef110_c", 0x60152000u, 0xFFu},
    };
    for (const auto& s : specs) {
        std::vector<std::uint8_t> self(0x60, s.fillByte);
        CaveCallTrace trace{};
        const std::uint32_t ret = ra3::gamelogic::runConstructObjectBE2030(
            self.data(), s.selfVa, &trace);
        const std::uint32_t m0 = readU32(self, 0x00);
        const std::uint32_t m1 = readU32(self, 0x30) | readU32(self, 0x34) | readU32(self, 0x38);
        const std::uint32_t m2 = readU16(self, 0x3C) | readU32(self, 0x58);
        printTrace(s.id, trace, ret, m0, m1, m2);
    }
}

// 22. 0x006232F0
void emit6232F0() {
    struct Spec {
        const char* id;
        std::uint32_t initDF6BC8;
        std::uint32_t initDF6B34;
    };
    const Spec specs[] = {
        {"shut6232f0_already_null", 0u, 0x60160100u},
        {"shut6232f0_both_active", 0x60160200u, 0x60160100u},
        {"shut6232f0_only_bc8", 0x60160300u, 0u},
    };
    for (const auto& s : specs) {
        std::uint32_t gBC8 = s.initDF6BC8;
        CaveCallTrace trace{};
        ra3::gamelogic::runShutdownGlobalsDF6BC8AndDF6B34(&gBC8, s.initDF6B34, &trace);
        printTrace(s.id, trace, 0u, gBC8, 0u, 0u);
    }
}

// 23. 0x0064BD80
void emit64BD80() {
    struct Spec {
        const char* id;
        std::uint32_t arg0;
        std::uint32_t secOffset;
        std::uint32_t refVa;
        std::int32_t initRef;
    };
    const Spec specs[] = {
        {"upd64bd80_nullref", 0x60170200u, 0xA0u, 0u, 0},
        {"upd64bd80_keepref", 0x60170200u, 0xA0u, 0x60170300u, 3},
        {"upd64bd80_freeref", 0x60170200u, 0xA0u, 0x60170300u, 1},
    };
    for (const auto& s : specs) {
        std::vector<std::uint8_t> self(0x50, 0);
        std::vector<std::uint8_t> sub3C(0xC0, 0);
        std::vector<std::uint8_t> refObj(0x10, 0);
        const std::uint32_t sub3CVa = 0x60170100u;
        writeU32(self, 0x3C, sub3CVa);
        writeU32(sub3C, 0xB0, s.refVa);
        writeU32(refObj, 0x04, static_cast<std::uint32_t>(s.initRef));
        CaveCallTrace trace{};
        ra3::gamelogic::runUpdateSubObject3CAndReleaseB0(
            self.data(),
            sub3C.data(),
            s.arg0,
            s.secOffset,
            {s.refVa, s.refVa ? refObj.data() : nullptr, 0x00617010u},
            &trace);
        printTrace(s.id, trace, 0u, readU32(sub3C, 0xB0), s.refVa ? readU32(refObj, 0x04) : 0u, 0u);
    }
}

// 24. 0x00683140
void emit683140() {
    struct Spec {
        const char* id;
        std::uint32_t arg0;
    };
    const Spec specs[] = {
        {"set683140_a", 0x11223344u},
        {"set683140_b", 0u},
        {"set683140_c", 0xAABBCCDDu},
    };
    for (const auto& s : specs) {
        std::vector<std::uint8_t> self(0x190, 0xEE);
        CaveCallTrace trace{};
        ra3::gamelogic::runSetObjectField180(self.data(), s.arg0);
        printTrace(s.id, trace, 0u, readU32(self, 0x180), 0u, 0u);
    }
}

// 25. 0x00685EB0
void emit685EB0() {
    struct Spec {
        const char* id;
        std::uint8_t val144;
    };
    const Spec specs[] = {
        {"read685eb0_zero", 0u},
        {"read685eb0_one", 1u},
        {"read685eb0_byte", 0x7Au},
    };
    for (const auto& s : specs) {
        std::vector<std::uint8_t> sub4(0x150, 0);
        writeU8(sub4, 0x144, s.val144);
        CaveCallTrace trace{};
        const std::uint8_t ret = ra3::gamelogic::runReadSubObject4Byte144(sub4.data());
        printTrace(s.id, trace, ret, s.val144, 0u, 0u);
    }
}

// 26. 0x006879B0
void emit6879B0() {
    struct Spec {
        const char* id;
        std::uint32_t selfVa;
        std::uint32_t sub238Va;
    };
    const Spec specs[] = {
        {"detach6879b0_null", 0x601A0000u, 0u},
        {"detach6879b0_a", 0x601A0000u, 0x601A0100u},
        {"detach6879b0_b", 0x601A1000u, 0x601A1200u},
    };
    for (const auto& s : specs) {
        std::vector<std::uint8_t> self(0x240, 0);
        writeU32(self, 0x238, s.sub238Va);
        CaveCallTrace trace{};
        ra3::gamelogic::runDetachSubObject238(self.data(), s.selfVa, &trace);
        printTrace(s.id, trace, 0u, readU32(self, 0x238), 0u, 0u);
    }
}

// 27. 0x00688190
void emit688190() {
    struct Spec {
        const char* id;
        bool hasOwner;
        std::uint32_t owner418;
        std::uint8_t flagByte;
    };
    const Spec specs[] = {
        {"ctor688190_with_owner", true, 0x601B0500u, 1u},
        {"ctor688190_null_owner", false, 0u, 0u},
        {"ctor688190_custom_flag", true, 0x601B0900u, 5u},
    };
    for (const auto& s : specs) {
        std::vector<std::uint8_t> self(0x20, 0xFF);
        std::vector<std::uint8_t> owner(0x420, 0);
        writeU32(owner, 0x418, s.owner418);
        CaveCallTrace trace{};
        ra3::gamelogic::runConstructObjectBF9CBC(
            self.data(), s.hasOwner ? owner.data() : nullptr, s.flagByte);
        printTrace(s.id, trace, 0u, readU32(self, 0x00), readU32(self, 0x04) ^ readU32(self, 0x08), readU8(self, 0x0C));
    }
}

// 28. 0x0071C460
void emit71C460() {
    constexpr std::uint32_t kLocalMapVa = 0x7F0FFEE0u;
    const std::uint32_t selfVa = 0x601C0000u;
    // Case 1: [self+0x418] == 0
    {
        std::vector<std::uint8_t> self(0x420, 0);
        CaveCallTrace trace{};
        ra3::gamelogic::runPropagateObjectMap418ToModules310(
            self.data(), selfVa, kLocalMapVa, nullptr, nullptr, 0u, &trace);
        printTrace("prop71c460_no418", trace, 0u, 0u, 0u, 0u);
    }
    // Case 2: 1 module whose sub->vt[0]() returns 1 (skips map copy)
    {
        std::vector<std::uint8_t> self(0x420, 0);
        std::vector<std::uint8_t> localMap(0x20, 0);
        writeU32(self, 0x418, 0x601C0100u);
        const ra3::gamelogic::Module310MapPropagateEntry entries[] = {
            {0x601C0200u, 0x0061C010u, 0x601C0300u, 0x0061C020u, 1u, 0x0061C030u, 0x0061C040u},
        };
        CaveCallTrace trace{};
        ra3::gamelogic::runPropagateObjectMap418ToModules310(
            self.data(), selfVa, kLocalMapVa, localMap.data(), entries, 1u, &trace);
        printTrace("prop71c460_skip", trace, 0u, 0u, 0u, 0u);
    }
    // Case 3: 1 module whose sub->vt[0]() returns 0 (copies map, merges +0x3F8, notifies, frees)
    {
        std::vector<std::uint8_t> self(0x420, 0);
        std::vector<std::uint8_t> localMap(0x20, 0);
        writeU32(self, 0x418, 0x601C0100u);
        const ra3::gamelogic::Module310MapPropagateEntry entries[] = {
            {0x601C0200u, 0x0061C010u, 0x601C0300u, 0x0061C020u, 0u, 0x0061C030u, 0x0061C040u},
        };
        CaveCallTrace trace{};
        ra3::gamelogic::runPropagateObjectMap418ToModules310(
            self.data(), selfVa, kLocalMapVa, localMap.data(), entries, 1u, &trace);
        printTrace("prop71c460_copy", trace, 0u, readU32(localMap, 0x04), readU32(localMap, 0x08), 0u);
    }
}

}  // namespace

int main() {
    emit4CB260();
    emit4D66D0();
    emit4D99E0();
    emit4DA710();
    emit4E5C90();
    emit4EDD70();
    emit504510();
    emit50A9C0();
    emit520120();
    emit523E60();
    emit5242B0();
    emit52D6B0();
    emit535410();
    emit537A40();
    emit538440();
    emit55B530();
    emit55CC60();
    emit5838F0();
    emit5A4ED0();
    emit5C5D70();
    emit5EF110();
    emit6232F0();
    emit64BD80();
    emit683140();
    emit685EB0();
    emit6879B0();
    emit688190();
    emit71C460();
    return 0;
}
