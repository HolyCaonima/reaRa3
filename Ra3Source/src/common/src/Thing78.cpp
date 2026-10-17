#include "ra3/common/Thing78.h"

namespace ra3::common {
namespace {

std::uint32_t imageConst(std::uint32_t addr) noexcept {
    switch (addr) {
    case 0x00BD90A4u:
        return 0x3F800000u;
    case 0x00BD90A8u:
        return 0x461C4000u;
    case 0x00BDBDD4u:
        return 0x4F800000u;
    case 0x00BE700Cu:
        return 0x7F7FFFFFu;
    case 0x00BBA3E4u:
        return 0x78189DE5u;
    default:
        return 0;
    }
}

int overlayIndex(const Tape& tape, std::uint32_t addr) noexcept {
    for (int i = 0; i < tape.gCount; ++i) {
        if (tape.gVa[i] == addr) {
            return i;
        }
    }
    return -1;
}

int truncFloat(float value) noexcept {
    int result = 0;
    double slot = 0.0;
    __asm {
        fld dword ptr value
        fstp qword ptr slot
        cvttsd2si eax, qword ptr slot
        mov result, eax
    }
    return result;
}

float mulInt(float value, int imm) noexcept {
    float slotf = 0.0F;
    __asm {
        fld dword ptr value
        fimul dword ptr imm
        fstp dword ptr slotf
    }
    return slotf;
}

float addInt(float value, int imm) noexcept {
    float slotf = 0.0F;
    __asm {
        fld dword ptr value
        fiadd dword ptr imm
        fstp dword ptr slotf
    }
    return slotf;
}

float divRev(float numerator, float denominator) noexcept {
    float slotf = 0.0F;
    __asm {
        fld dword ptr denominator
        fdivr dword ptr numerator
        fstp dword ptr slotf
    }
    return slotf;
}

void sinCos(float angle, float& sine, float& cosine) noexcept {
    float sineBits = 0.0F;
    float cosineBits = 0.0F;
    __asm {
        fld dword ptr angle
        fsincos
        fstp dword ptr cosineBits
        fstp dword ptr sineBits
    }
    sine = sineBits;
    cosine = cosineBits;
}

float invSqrtPositive(float sum) noexcept {
    float slotf = 0.0F;
    __asm {
        fld dword ptr sum
        fsqrt
        fld1
        fdivrp st(1), st(0)
        fstp dword ptr slotf
    }
    return slotf;
}

int clockScale(float value, std::uint32_t clock) noexcept {
    int result = 0;
    const float two32 = bitsToFloat(0x4F800000u);
    double slot = 0.0;
    __asm {
        fld dword ptr value
        fild dword ptr clock
        mov eax, clock
        test eax, eax
        jge thing78_clock_pos
        fadd dword ptr two32
    thing78_clock_pos:
        fmulp st(1), st(0)
        fstp qword ptr slot
        cvttsd2si eax, qword ptr slot
        mov result, eax
    }
    return result;
}

std::uint32_t swar(std::uint32_t value) noexcept {
    std::uint32_t ecx = value;
    std::uint32_t edx = ecx >> 1;
    edx &= 0x55555555u;
    ecx -= edx;
    edx = ecx >> 2;
    edx &= 0x33333333u;
    ecx &= 0x33333333u;
    edx += ecx;
    ecx = edx >> 4;
    ecx += edx;
    ecx &= 0x0F0F0F0Fu;
    ecx *= 0x01010101u;
    return ecx >> 24;
}

// 0x007886e0. [[this]]. HighlanderBody 00c11d28 slot 25 calls it at 0069070f.
RunResult firstSlot(Mem& mem, std::uint32_t self) {
    RunResult out;
    out.eax = mem.ld(mem.ld(self));
    return out;
}

// 0x00788730. [[this+0x14]+0x130]. AIBuildableStructure slot 21 calls it.
RunResult templateField130(Mem& mem, std::uint32_t self) {
    RunResult out;
    out.eax = mem.ld(mem.ld(self + 0x14) + 0x130);
    return out;
}

// 0x00788750. [this+0x14]+0x230. UnitAIExitGoalState -> 004f5780 calls it.
RunResult templatePlus230(Mem& mem, std::uint32_t self) {
    RunResult out;
    out.eax = mem.ld(self + 0x14) + 0x230u;
    return out;
}

// 0x00788760. [this+0x2C4]. AIBuildableStructure slot 23 calls it.
RunResult field2C4(Mem& mem, std::uint32_t self) {
    RunResult out;
    out.eax = mem.ld(self + 0x2C4);
    return out;
}

// 0x00788650. Writes vtable 00C28498, stores the id, and queries slot +0x10 when src != 0.
RunResult assignTagged(Mem& mem, std::uint32_t self, std::uint32_t src, std::uint32_t id) {
    mem.st(self + 4, id);
    mem.st(self, 0x00C28498u);
    RunResult out;
    out.eax = self;
    if (src == 0) {
        mem.st(self + 8, 0);
        return out;
    }
    const CallArg none[1] = {};
    const std::uint32_t value = mem.call(src, none, 0);
    mem.st(self + 8, value);
    return out;
}

// 0x007886c0. Bit 1 of [this+0x48] caches ST0 from slot 0 into +0x58.
RunResult cachedScalar(Mem& mem, std::uint32_t self) {
    RunResult out;
    if ((mem.ld8(self + 0x48) & 2) == 0) {
        const CallArg none[1] = {};
        std::uint32_t st0 = 0;
        out.eax = mem.call(self, none, 0, &st0);
        mem.st(self + 0x58, st0);
        mem.st(self + 0x48, mem.ld(self + 0x48) | 2u);
    }
    out.st0 = mem.ld(self + 0x58);
    return out;
}

// 0x00788770. count==-1 reads a word at +0x304. A live query replaces the 1.0 scale.
RunResult scaleCount(Mem& mem, std::uint32_t self, std::uint32_t gate, std::uint32_t query, std::uint32_t count) {
    const std::uint32_t used = count == 0xFFFFFFFFu ? mem.ld16(self + 0x304) : count;
    RunResult out;
    if (gate == 0) {
        out.eax = used;
        return out;
    }
    const CallArg arg[] = {rawArg(0xFFFFFFFFu)};
    const std::uint32_t found = mem.call(query, arg, 1);
    std::uint32_t scale = 0x3F800000u;
    if (found != 0) {
        const CallArg pushed[] = {rawArg(self)};
        std::uint32_t st0 = 0;
        (void)mem.call(found, pushed, 1, &st0);
        scale = st0;
    }
    const float product = bitsToFloat(scale) * static_cast<float>(static_cast<int>(used));
    out.eax = static_cast<std::uint32_t>(truncFloat(product));
    return out;
}

// 0x00788800. Dozer clock scale. ST0 is the scaled value; EAX is the rules mode word
// on the paths that do not rewrite it.
RunResult scaleToFloat(
    Mem& mem, std::uint32_t self, std::uint32_t divisor, std::uint32_t query, std::uint32_t count,
    std::uint32_t flags) {
    float xmm = 0.0F;
    if (count == 0xFFFFFFFFu) {
        xmm = mem.ldf(mem.ld(self + 0x14) + 0x88);
    } else {
        xmm = static_cast<float>(static_cast<int>(count));
    }
    int work = truncFloat(xmm);
    if (query != 0) {
        const CallArg minus[] = {rawArg(0xFFFFFFFFu)};
        const std::uint32_t obj = mem.call(query, minus, 1);
        if (obj != 0) {
            const CallArg pushed[] = {rawArg(self)};
            std::uint32_t st0 = 0;
            (void)mem.call(obj, pushed, 1, &st0);
            work = truncFloat(mulInt(bitsToFloat(st0), work));
            st0 = 0;
            const CallArg none[1] = {};
            (void)mem.call(obj, none, 0, &st0);
            work = truncFloat(addInt(bitsToFloat(st0), work));
        }
    }
    if (divisor != 0) {
        const std::uint32_t denomObj = mem.ld(divisor + 0x74);
        if (denomObj != 0) {
            const float numer = static_cast<float>(work);
            const CallArg none[1] = {};
            std::uint32_t st0 = 0;
            (void)mem.call(denomObj, none, 0, &st0);
            work = truncFloat(divRev(numer, bitsToFloat(st0)));
        }
    }
    const std::uint32_t rules = mem.ld(0x00DEBA64u);
    std::uint32_t eax = mem.ld(rules + 0x150);
    if (eax == 1 || eax == 4 || eax == 2) {
        int ecx = -static_cast<int>(mem.ld(divisor + 0x7C));
        const int edx = ecx - 1;
        if (static_cast<std::uint32_t>(edx) <= 0x63u) {
            const int quot = ecx / 5;
            if (ecx - quot * 5 == 0) {
                const int index = quot - 1;
                if (index > 0) {
                    const std::uint32_t table = mem.ld(0x00DEEE70u);
                    if (static_cast<std::uint32_t>(index) < mem.ld(table + 0x0C)) {
                        const float scaled =
                            static_cast<float>(work) * mem.ldf(mem.ld(table + 0x10) + static_cast<std::uint32_t>(index) * 4u);
                        work = truncFloat(scaled);
                    }
                }
            }
        }
    }
    float result = 0.0F;
    const float factor = mem.ldf(rules + 0x1C4);
    __asm {
        fild dword ptr work
        mov eax, flags
        test al, al
        je thing78_scale_nomul
        fmul dword ptr factor
    thing78_scale_nomul:
        fstp dword ptr result
    }
    RunResult out;
    out.eax = eax;
    out.st0 = floatToBits(result);
    return out;
}

// 0x0078B7A0. Writes record vtable 00C284C8, bumps [00DF9D24], and builds an identity when arg != 0.
RunResult initMatrixRecord(Mem& mem, std::uint32_t self, std::uint32_t arg) {
    mem.st(self, 0x00C284C8u);
    mem.st(0x00DF9D24u, mem.ld(0x00DF9D24u) + 1u);
    RunResult out;
    out.eax = self;
    if (arg == 0) {
        return out;
    }
    const float one = bitsToFloat(mem.ld(0x00BD90A4u));
    const float zero = 0.0F;
    mem.st(self + 4, arg);
    const float row0[] = {one, zero, zero, zero};
    const float row1[] = {zero, one, zero, zero};
    const float row2[] = {zero, zero, one, zero};
    for (int i = 0; i < 4; ++i) {
        mem.stf(self + 0x08 + static_cast<std::uint32_t>(i) * 4u, row0[i]);
    }
    for (int i = 0; i < 4; ++i) {
        mem.stf(self + 0x18 + static_cast<std::uint32_t>(i) * 4u, row1[i]);
    }
    for (int i = 0; i < 4; ++i) {
        mem.stf(self + 0x28 + static_cast<std::uint32_t>(i) * 4u, row2[i]);
    }
    mem.stf(self + 0x38, zero);
    mem.stf(self + 0x3C, zero);
    mem.stf(self + 0x40, zero);
    mem.st(self + 0x48, 0);
    mem.stf(self + 0x4C, zero);
    mem.stf(self + 0x50, zero);
    mem.stf(self + 0x54, zero);
    mem.stf(self + 0x58, zero);
    mem.stf(self + 0x5C, zero);
    return out;
}

// 0x0078B870. Bit 0 of +0x48 caches fsincos(angle at +0x44) at +0x4C. EAX is that pointer.
RunResult directionCache(Mem& mem, std::uint32_t self) {
    if ((mem.ld8(self + 0x48) & 1) == 0) {
        float sine = 0.0F;
        float cosine = 0.0F;
        sinCos(mem.ldf(self + 0x44), sine, cosine);
        mem.stf(self + 0x4C, cosine);
        mem.stf(self + 0x50, sine);
        mem.stf(self + 0x54, 0.0F);
        mem.st(self + 0x48, mem.ld(self + 0x48) | 1u);
    }
    RunResult out;
    out.eax = self + 0x4C;
    return out;
}

// 0x0078B8E0. Same cache, then copies 12 bytes to out. EAX is out.
RunResult directionWrite(Mem& mem, std::uint32_t self, std::uint32_t dest) {
    if ((mem.ld8(self + 0x48) & 1) == 0) {
        float sine = 0.0F;
        float cosine = 0.0F;
        sinCos(mem.ldf(self + 0x44), sine, cosine);
        mem.stf(self + 0x4C, cosine);
        mem.stf(self + 0x50, sine);
        mem.stf(self + 0x54, 0.0F);
        mem.st(self + 0x48, mem.ld(self + 0x48) | 1u);
    }
    mem.st(dest, mem.ld(self + 0x4C));
    mem.st(dest + 4, mem.ld(self + 0x50));
    mem.st(dest + 8, mem.ld(self + 0x54));
    RunResult out;
    out.eax = dest;
    return out;
}

// 0x0078B990. Height cache at +0x5C. The terrain call is thiscall, 16 bytes of args.
RunResult cachedHeight(Mem& mem, std::uint32_t self) {
    RunResult out;
    if ((mem.ld8(self + 0x48) & 4) != 0) {
        out.st0 = mem.ld(self + 0x5C);
        return out;
    }
    const float x = mem.ldf(self + 0x38);
    const float z = mem.ldf(self + 0x3C);
    const std::uint32_t probe = self + 0x08;
    const CallArg args[] = {
        rawArg(floatToBits(x)),
        rawArg(floatToBits(z)),
        stackArg(self),
        rawArg(0),
    };
    const std::uint32_t terrain = mem.ld(0x00DFC198u);
    const std::uint32_t eax = mem.call(terrain, args, 4);
    if ((eax & 0xFFu) != 0) {
        const float height = mem.ldf(self + 0x40) - mem.ldf(probe);
        mem.st(self + 0x48, mem.ld(self + 0x48) | 4u);
        mem.stf(self + 0x5C, height);
        out.st0 = mem.ld(self + 0x5C);
        return out;
    }
    if ((mem.ld8(self + 0x48) & 2) == 0) {
        const CallArg none[1] = {};
        std::uint32_t st0 = 0;
        (void)mem.call(self, none, 0, &st0);
        mem.st(self + 0x58, st0);
        mem.st(self + 0x48, mem.ld(self + 0x48) | 2u);
    }
    mem.st(self + 0x5C, mem.ld(self + 0x58));
    mem.st(self + 0x48, mem.ld(self + 0x48) | 4u);
    out.st0 = mem.ld(self + 0x5C);
    return out;
}

// 0x0078BA00. 3x4 at this+8 times a point. adjust != 0 runs 00660D50 first.
RunResult transformPoint(
    Mem& mem, std::uint32_t self, std::uint32_t point, std::uint32_t ignored, std::uint32_t dest,
    std::uint32_t adjust) {
    if (adjust != 0) {
        const CallArg args[] = {rawArg(self + 8), rawArg(ignored)};
        (void)mem.call(adjust, args, 2);
    }
    RunResult out;
    if (dest == 0) {
        return out;
    }
    out.eax = point;
    std::uint32_t src = point;
    float local[3] = {};
    if (dest == point) {
        local[0] = mem.ldf(point);
        local[1] = mem.ldf(point + 4);
        local[2] = mem.ldf(point + 8);
        src = 0;
    }
    const float px = src == 0 ? local[0] : mem.ldf(src);
    const float py = src == 0 ? local[1] : mem.ldf(src + 4);
    const float pz = src == 0 ? local[2] : mem.ldf(src + 8);
    const float x = (mem.ldf(self + 0x10) * pz) + (mem.ldf(self + 0x0C) * py) +
                    (mem.ldf(self + 0x08) * px) + mem.ldf(self + 0x14);
    const float y = (mem.ldf(self + 0x20) * pz) + (mem.ldf(self + 0x18) * px) +
                    (mem.ldf(self + 0x1C) * py) + mem.ldf(self + 0x24);
    const float z = (mem.ldf(self + 0x30) * pz) + (mem.ldf(self + 0x28) * px) +
                    (mem.ldf(self + 0x2C) * py) + mem.ldf(self + 0x34);
    mem.stf(dest, x);
    mem.stf(dest + 4, y);
    mem.stf(dest + 8, z);
    if (dest == point) {
        out.eax = dest;
    }
    return out;
}

// 0x0078BDA0. Forwards four args to 0x00788800, then multiplies ST0 by the unsigned clock.
RunResult scaleByClock(
    Mem& mem, std::uint32_t self, std::uint32_t a, std::uint32_t b, std::uint32_t c, std::uint32_t d) {
    const RunResult scaled = scaleToFloat(mem, self, a, b, c, d);
    const std::uint32_t clock = mem.ld(0x00CB0910u);
    RunResult out;
    out.eax = static_cast<std::uint32_t>(clockScale(bitsToFloat(scaled.st0), clock));
    return out;
}

// 0x0078BDF0. Bit 13 of [[obj+4]+0xD0] and bit 3 of [obj+0x98].
RunResult objectFlagPair(Mem& mem, std::uint32_t obj) {
    RunResult out;
    const std::uint32_t inner = mem.ld(obj + 4);
    const std::uint32_t flags = mem.ld(inner + 0xD0);
    if (((flags >> 13) & 1u) == 0) {
        return out;
    }
    const std::uint32_t second = mem.ld(obj + 0x98);
    if (((second >> 3) & 1u) == 0) {
        return out;
    }
    out.eax = 1;
    return out;
}

// 0x0078BB60. Four stride-0x14 spans. A hit returns the inner pointer.
RunResult findSpanRecord(Mem& mem, std::uint32_t self, std::uint32_t key) {
    const std::uint32_t spans[][2] = {
        {0x1A8, 0x1AC},
        {0x1CC, 0x1D0},
        {0x1F0, 0x1F4},
        {0x214, 0x218},
    };
    RunResult out;
    for (int span = 0; span < 4; ++span) {
        std::uint32_t cursor = mem.ld(self + spans[span][0]);
        const std::uint32_t end = mem.ld(self + spans[span][1]);
        while (cursor != end) {
            const std::uint32_t inner = mem.ld(cursor + 4);
            if (mem.ld(inner) == key) {
                out.eax = inner;
                return out;
            }
            cursor += 0x14;
        }
    }
    return out;
}

// 0x0078BFB0. Copies the column at +8/+18/+28 and normalizes it. EAX is the Y bits.
RunResult normalizeColumn(Mem& mem, std::uint32_t self, std::uint32_t dest) {
    const float x = mem.ldf(self + 0x08);
    const float y = mem.ldf(self + 0x18);
    const float z = mem.ldf(self + 0x28);
    mem.stf(dest, x);
    mem.stf(dest + 4, y);
    mem.stf(dest + 8, z);
    const float xx = x * x;
    const float yy = y * y;
    const float zz = z * z;
    const float sum = (xx + yy) + zz;
    float scale = 0.0F;
    if (sum > 0.0F) {
        scale = invSqrtPositive(sum);
    }
    const float nx = x * scale;
    const float ny = y * scale;
    const float nz = z * scale;
    mem.stf(dest, nx);
    mem.stf(dest + 4, ny);
    mem.stf(dest + 8, nz);
    RunResult out;
    out.eax = floatToBits(ny);
    return out;
}

// 0x0078BF60. Map at this+0x24. arg1 is the record. A hit calls the target's slot 0.
RunResult dispatchKeyed(Mem& mem, std::uint32_t self, std::uint32_t extra, std::uint32_t record, std::uint32_t arg2) {
    (void)arg2;
    const std::uint32_t key = mem.ld(record);
    const CallArg args[] = {stackArg(self), stackArg(key)};
    const std::uint32_t found = mem.call(self + 0x24, args, 2);
    const std::uint32_t node = mem.ld(found);
    RunResult out;
    if (node == self + 0x28) {
        return out;
    }
    const std::uint32_t target = mem.ld(node + 0x14);
    if (target == 0) {
        return out;
    }
    const CallArg callArgs[] = {rawArg(extra), rawArg(record)};
    out.eax = mem.call(target, callArgs, 2);
    return out;
}

// 0x0078B690. Looks up [name], then slot +0x3C or the miss path slot +0x58/+0x30.
RunResult resolveDisplayName(Mem& mem, std::uint32_t self, std::uint32_t name) {
    const CallArg lookup[] = {rawArg(mem.ld(name)), rawArg(0)};
    const std::uint32_t found = mem.call(0, lookup, 2);
    RunResult out;
    out.eax = self;
    if (found != 0) {
        const CallArg arg[] = {rawArg(found)};
        (void)mem.call(self, arg, 1);
        return out;
    }
    const CallArg miss[] = {rawArg(0x00BD60E4u), rawArg(0x10)};
    (void)mem.call(self, miss, 2);
    const CallArg text[] = {rawArg(mem.ld(name))};
    (void)mem.call(self, text, 1);
    return out;
}

// 0x0078CB90. Fills four cached ids from template strings via 004A62E0.
RunResult ensureAudioIds(Mem& mem, std::uint32_t self) {
    const std::uint32_t slots[][2] = {
        {0x2AC, 0x98},
        {0x2B0, 0x9C},
        {0x2B4, 0xA0},
        {0x2B8, 0xA4},
    };
    RunResult out;
    for (const auto& slot : slots) {
        if (mem.ld(self + slot[0]) != 0) {
            continue;
        }
        const std::uint32_t text = mem.ld(mem.ld(self + 0x14) + slot[1]);
        const CallArg args[] = {rawArg(0x56626220u), rawArg(text), rawArg(0)};
        out.eax = mem.call(self, args, 3);
        mem.st(self + slot[0], out.eax);
    }
    return out;
}

// 0x0078D4F0. Walks [this+0x24] through +0x0C, caching ids then 00528920.
RunResult resolveAudioChain(Mem& mem, std::uint32_t self) {
    std::uint32_t node = mem.ld(self + 0x24);
    RunResult out;
    while (node != 0) {
        out = ensureAudioIds(mem, node);
        const CallArg args[] = {rawArg(node)};
        out.eax = mem.call(node, args, 1);
        mem.st(node + 0x2C0, out.eax);
        node = mem.ld(node + 0x0C);
    }
    return out;
}

// 0x0078D520. [this+0x294] gates a map lookup at this+0x280. Miss and the header return 0.
RunResult lookupMappedValue(Mem& mem, std::uint32_t self, std::uint32_t key) {
    RunResult out;
    if (mem.ld(self + 0x294) == 0) {
        return out;
    }
    const CallArg args[] = {stackArg(self), stackArg(key)};
    const std::uint32_t retPtr = mem.call(self + 0x280, args, 2);
    const std::uint32_t node = mem.ld(retPtr);
    if (node == self + 0x284) {
        return out;
    }
    out.eax = node + 0x14;
    return out;
}

// 0x0078D560. Picks the record whose masked popcount is greatest. Ties keep the smaller complement.
// An empty range returns 0. bestNeg starts at 0x3E7, so a non-empty range returns a cursor.
RunResult selectDensestMask(Mem& mem, std::uint32_t range, std::uint32_t maskPtr) {
    const std::uint32_t begin = mem.ld(range);
    const std::uint32_t end = mem.ld(range + 4);
    RunResult out;
    if (begin == end) {
        return out;
    }
    const std::uint32_t mask = mem.ld(maskPtr);
    std::uint32_t bestPos = 0;
    std::uint32_t bestNeg = 0x3E7u;
    std::uint32_t best = end;
    for (std::uint32_t cursor = begin; cursor != end; cursor += 4) {
        const std::uint32_t bits = mem.ld(mem.ld(cursor) + 8);
        const std::uint32_t pos = swar(mask & bits);
        const std::uint32_t neg = swar((~mask) & bits);
        const bool better = pos > bestPos || (pos == bestPos && neg < bestNeg);
        if (better) {
            best = cursor;
            bestPos = pos;
            bestNeg = neg;
        }
    }
    out.eax = best;
    return out;
}

} // namespace

std::uint32_t Mem::ld(std::uint32_t addr) const {
    const int slot = tape == nullptr ? -1 : overlayIndex(*tape, addr);
    if (slot >= 0) {
        return tape->gVal[slot];
    }
    if (addr >= base && addr + 4 <= base + size) {
        std::uint32_t value = 0;
        std::memcpy(&value, bytes + (addr - base), 4);
        return value;
    }
    return imageConst(addr);
}

void Mem::st(std::uint32_t addr, std::uint32_t value) {
    if (tape != nullptr) {
        const int slot = overlayIndex(*tape, addr);
        if (slot >= 0) {
            tape->gVal[slot] = value;
            return;
        }
    }
    if (addr >= base && addr + 4 <= base + size) {
        std::memcpy(bytes + (addr - base), &value, 4);
    }
}

std::uint8_t Mem::ld8(std::uint32_t addr) const {
    return static_cast<std::uint8_t>(ld(addr & ~3u) >> ((addr & 3u) * 8u));
}

void Mem::st8(std::uint32_t addr, std::uint8_t value) {
    const std::uint32_t aligned = addr & ~3u;
    const std::uint32_t shift = (addr & 3u) * 8u;
    const std::uint32_t mask = 0xFFu << shift;
    st(aligned, (ld(aligned) & ~mask) | (static_cast<std::uint32_t>(value) << shift));
}

std::uint16_t Mem::ld16(std::uint32_t addr) const {
    return static_cast<std::uint16_t>(ld(addr & ~3u) >> ((addr & 3u) * 8u));
}

float Mem::ldf(std::uint32_t addr) const {
    return bitsToFloat(ld(addr));
}

void Mem::stf(std::uint32_t addr, float value) {
    st(addr, floatToBits(value));
}

std::uint32_t Mem::call(std::uint32_t ecx, const CallArg* args, int n, std::uint32_t* st0) {
    std::uint32_t eax = 0;
    std::uint32_t bits = 0;
    if (tape != nullptr && tape->index < tape->count) {
        eax = tape->eax[tape->index];
        bits = tape->st0 == nullptr ? 0 : tape->st0[tape->index];
        ++tape->index;
    }
    if (tape != nullptr && tape->traceCount + 2u + static_cast<std::uint32_t>(n) * 2u < 768u) {
        tape->trace[tape->traceCount++] = static_cast<std::uint32_t>(n);
        tape->trace[tape->traceCount++] = ecx;
        for (int i = 0; i < n; ++i) {
            tape->trace[tape->traceCount++] = args[i].kind;
            tape->trace[tape->traceCount++] = args[i].value;
        }
    }
    if (st0 != nullptr) {
        *st0 = bits;
    }
    return eax;
}

bool runThing78(std::uint32_t va, Mem& mem, std::uint32_t ecx, const std::uint32_t* args, int argc, RunResult& out) {
    const std::uint32_t a0 = argc > 0 ? args[0] : 0;
    const std::uint32_t a1 = argc > 1 ? args[1] : 0;
    const std::uint32_t a2 = argc > 2 ? args[2] : 0;
    const std::uint32_t a3 = argc > 3 ? args[3] : 0;
    switch (va) {
    case 0x007886E0u:
        out = firstSlot(mem, ecx);
        return true;
    case 0x00788730u:
        out = templateField130(mem, ecx);
        return true;
    case 0x00788750u:
        out = templatePlus230(mem, ecx);
        return true;
    case 0x00788760u:
        out = field2C4(mem, ecx);
        return true;
    case 0x00788650u:
        out = assignTagged(mem, ecx, a0, a1);
        return true;
    case 0x007886C0u:
        out = cachedScalar(mem, ecx);
        return true;
    case 0x00788770u:
        out = scaleCount(mem, ecx, a0, a1, a2);
        return true;
    case 0x00788800u:
        out = scaleToFloat(mem, ecx, a0, a1, a2, a3);
        return true;
    case 0x0078B7A0u:
        out = initMatrixRecord(mem, ecx, a0);
        return true;
    case 0x0078B870u:
        out = directionCache(mem, ecx);
        return true;
    case 0x0078B8E0u:
        out = directionWrite(mem, ecx, a0);
        return true;
    case 0x0078B990u:
        out = cachedHeight(mem, ecx);
        return true;
    case 0x0078BA00u:
        out = transformPoint(mem, ecx, a0, a1, a2, a3);
        return true;
    case 0x0078BDA0u:
        out = scaleByClock(mem, ecx, a0, a1, a2, a3);
        return true;
    case 0x0078BDF0u:
        out = objectFlagPair(mem, a0);
        return true;
    case 0x0078BB60u:
        out = findSpanRecord(mem, ecx, a0);
        return true;
    case 0x0078BFB0u:
        out = normalizeColumn(mem, ecx, a0);
        return true;
    case 0x0078BF60u:
        out = dispatchKeyed(mem, ecx, a0, a1, a2);
        return true;
    case 0x0078B690u:
        out = resolveDisplayName(mem, a0, a1);
        return true;
    case 0x0078CB90u:
        out = ensureAudioIds(mem, ecx);
        return true;
    case 0x0078D4F0u:
        out = resolveAudioChain(mem, ecx);
        return true;
    case 0x0078D520u:
        out = lookupMappedValue(mem, ecx, a0);
        return true;
    case 0x0078D560u:
        out = selectDensestMask(mem, a0, a1);
        return true;
    default:
        return false;
    }
}

} // namespace ra3::common
