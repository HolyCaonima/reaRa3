#include "ra3/gamelogic/ObjectUpdates78.h"

namespace ra3::gamelogic {
namespace {

using ra3::common::CallArg;
using ra3::common::Mem;
using ra3::common::RunResult;
using ra3::common::rawArg;
using ra3::common::stackArg;

// 0x007885E0. BehaviorModule 00BDBC08 slot 5. Writes [[this+4]+4] through the out pointer.
RunResult writeModuleDataWord(Mem& mem, std::uint32_t self, std::uint32_t dest) {
    const std::uint32_t value = mem.ld(mem.ld(self + 4) + 4);
    mem.st(dest, value);
    RunResult out;
    out.eax = dest;
    return out;
}

// 0x0078B730. DrawModule 00C54498 slot 3. Three version pairs, then slot +0x84 at this+0x10.
RunResult xferDrawModule(Mem& mem, std::uint32_t self, std::uint32_t xfer) {
    const CallArg ver[] = {stackArg(0x60000101u)};
    const CallArg verTail[] = {stackArg(0x7FE00101u)};
    (void)mem.call(xfer, ver, 1);
    (void)mem.call(xfer, ver, 1);
    (void)mem.call(xfer, verTail, 1);
    const CallArg tail[] = {rawArg(self + 0x10)};
    (void)mem.call(xfer, tail, 1);
    return {};
}

// 0x007874A0. GatherEjectUpdate 00C20848 slot 3.
RunResult xferGatherEject(Mem& mem, std::uint32_t self, std::uint32_t xfer) {
    const CallArg ver[] = {stackArg(0x60000101u)};
    (void)mem.call(xfer, ver, 1);
    (void)mem.call(xfer, ver, 1);
    const CallArg one[] = {rawArg(xfer)};
    (void)mem.call(self, one, 1);
    (void)mem.call(self + 0x10, one, 1);
    const CallArg pair[] = {rawArg(xfer), rawArg(self + 0x24)};
    (void)mem.call(self + 0x10, pair, 2);
    const CallArg field[] = {rawArg(self + 0x34)};
    (void)mem.call(xfer, field, 1);
    const CallArg last[] = {rawArg(self + 0x40)};
    (void)mem.call(xfer, last, 1);
    return {};
}

// 0x007877D0. SqueezeFlingUpdate 00C225F0 slot 3. The first version byte is 2.
RunResult xferSqueezeFling(Mem& mem, std::uint32_t self, std::uint32_t xfer) {
    const CallArg first[] = {stackArg(0x60000201u)};
    const CallArg second[] = {stackArg(0x60000101u)};
    (void)mem.call(xfer, first, 1);
    (void)mem.call(xfer, second, 1);
    const CallArg one[] = {rawArg(xfer)};
    (void)mem.call(self, one, 1);
    (void)mem.call(self + 0x10, one, 1);
    const CallArg tail[] = {rawArg(self + 0x48)};
    (void)mem.call(xfer, tail, 1);
    const CallArg list[] = {rawArg(xfer), rawArg(self + 0x24)};
    (void)mem.call(xfer, list, 2);
    const CallArg items[] = {rawArg(xfer), rawArg(self + 0x34)};
    (void)mem.call(self + 0x34, items, 2);
    const CallArg last[] = {rawArg(self + 0x44)};
    (void)mem.call(xfer, last, 1);
    return {};
}

// 0x00781ED0. BalloonMoveToGroundState 00C0B578 slot 5. Returns 0 or -1.
RunResult stepBalloon(Mem& mem, std::uint32_t self) {
    const std::uint32_t obj = mem.ld(self + 0x18);
    const std::uint32_t marker = mem.ld(mem.ld(obj + 0x24) + 0x374);
    if (marker != 0) {
        const std::uint32_t point = mem.ld(marker + 0x3C);
        const CallArg call[] = {
            stackArg(mem.ld(point + 0x2C)),
            rawArg(mem.ld(0x00BE700Cu)),
        };
        (void)mem.call(obj, call, 2);
    }
    RunResult out;
    const CallArg none[1] = {};
    if ((mem.call(self, none, 0) & 0xFFu) != 0) {
        out.eax = 0xFFFFFFFFu;
        return out;
    }
    const std::uint32_t host = mem.ld(obj + 0x24);
    const std::uint32_t other = mem.call(obj, none, 0);
    if (other != 0) {
        const CallArg args[] = {rawArg(other), rawArg(0)};
        if ((mem.call(host, args, 2) & 0xFFu) != 0) {
            out.eax = 0xFFFFFFFFu;
            return out;
        }
    }
    if ((mem.call(self, none, 0) & 0xFFu) != 0 || (mem.call(self, none, 0) & 0xFFu) != 0) {
        out.eax = 0xFFFFFFFFu;
        return out;
    }
    return out;
}

// 0x007879F0. LineSegmentDamageUpdate 00C28380 slot 3. Empty-list and flag-off paths.
RunResult xferLineSegment(Mem& mem, std::uint32_t self, std::uint32_t xfer) {
    const CallArg ver[] = {stackArg(0x60000101u)};
    (void)mem.call(xfer, ver, 1);
    (void)mem.call(xfer, ver, 1);
    const CallArg one[] = {rawArg(xfer)};
    (void)mem.call(self, one, 1);
    (void)mem.call(self + 0x10, one, 1);
    const std::uint32_t head = self + 0x30;
    std::uint32_t cursor = mem.ld(head);
    std::uint32_t count = 0;
    while (cursor != head) {
        cursor = mem.ld(cursor);
        ++count;
    }
    const CallArg counted[] = {stackArg(count)};
    (void)mem.call(xfer, counted, 1);
    const bool flag = ((mem.ld(xfer + 4) >> 1) & 1u) != 0;
    if (!flag) {
        std::uint32_t node = mem.ld(head);
        while (node != head) {
            const CallArg arg[] = {rawArg(xfer)};
            (void)mem.call(node + 8, arg, 1);
            node = mem.ld(node);
        }
    }
    const CallArg tail[] = {rawArg(self + 0x58)};
    (void)mem.call(xfer, tail, 1);
    return {};
}

// 0x00788470. LineSegmentDamageUpdate slot 4. Static NAMEKEY cache of 00C283D8.
RunResult lineSegmentNameKey(Mem& mem) {
    RunResult out;
    if ((mem.ld8(0x00DF9D1Cu) & 1u) == 0) {
        const std::uint32_t guard = mem.ld(0x00DF9D1Cu) | 1u;
        mem.st(0x00DF9D1Cu, guard);
        const CallArg arg[] = {rawArg(0x00C283D8u)};
        mem.st(0x00DF9D18u, mem.call(mem.ld(0x00E1A078u), arg, 1));
        out.eax = mem.ld(0x00DF9D18u);
        return out;
    }
    out.eax = mem.ld(0x00DF9D18u);
    return out;
}

// 0x00787BD0. Called by SqueezeFling secondary slot 0 at 00787CE0. Timer, then a stride-0x34 walk.
RunResult tickSqueeze(Mem& mem, std::uint32_t self) {
    std::uint32_t timer = mem.ld(self + 0x44);
    RunResult out;
    if (timer != 0) {
        --timer;
        mem.st(self + 0x44, timer);
        out.eax = timer;
    }
    if (timer == 0) {
        const std::uint32_t begin = mem.ld(self + 0x24);
        const std::uint32_t end = mem.ld(self + 0x28);
        if (begin == end) {
            const std::uint32_t tmpl = mem.ld(self + 4);
            const float product = mem.ldf(tmpl + 0x0C) * mem.ldf(0x00DEEEE8u);
            long long wide = 0;
            unsigned short savedCw = 0;
            unsigned short truncCw = 0;
            __asm {
                fnstcw word ptr savedCw
                mov ax, word ptr savedCw
                or ax, 0x0C00
                mov word ptr truncCw, ax
                fldcw word ptr truncCw
                fld dword ptr product
                fistp qword ptr wide
                fldcw word ptr savedCw
            }
            const CallArg args[] = {
                rawArg(1),
                rawArg(static_cast<std::uint32_t>(wide)),
                rawArg(0x00C281C0u),
                rawArg(0x1C3u),
            };
            out.eax = mem.call(self, args, 4);
            mem.st(self + 0x44, out.eax);
        }
    }
    const std::uint32_t itemsEnd = mem.ld(self + 0x38);
    for (std::uint32_t item = mem.ld(self + 0x34); item != itemsEnd; item += 0x34) {
        const CallArg args[] = {rawArg(mem.ld(self + 4)), rawArg(mem.ld(self + 8))};
        out.eax = mem.call(item, args, 2);
    }
    return out;
}

// 0x007806F0. Secondary table 00C0DC34 slot 3. Adds a rules field, then one of two follow-ups.
RunResult applyTimedBonus(Mem& mem, std::uint32_t self, std::uint32_t amount) {
    const std::uint32_t rules = mem.ld(0x00DEBA64u);
    const CallArg args[] = {rawArg(mem.ld(self + 0x10)), rawArg(self), rawArg(mem.ld(rules + 0x58) + amount)};
    (void)mem.call(rules, args, 3);
    const std::uint32_t body = self - 0x10;
    RunResult out;
    if (amount == 0x3FFFFFFFu) {
        out.eax = mem.call(body, nullptr, 0);
        return out;
    }
    mem.st(body + 0x34, 0);
    out.eax = mem.call(body, nullptr, 0);
    return out;
}

// 0x00782E20. Secondary table 00C01180 slot 98. Clears +0x2C and returns 0x3FFFFFFF.
// The tail into 0077E1D0 is not taken by the oracle cases.
RunResult releaseAttached(Mem& mem, std::uint32_t self) {
    const std::uint32_t handle = mem.ld(self + 0x2C);
    RunResult out;
    out.eax = 0x3FFFFFFFu;
    if (handle != 0 && mem.ld8(self + 0x31) == 0) {
        mem.st8(self + 0x31, 1);
        const CallArg none[1] = {};
        out.eax = mem.call(self - 0x10, none, 0);
        return out;
    }
    if (handle != 0 && mem.ld8(self + 0x31) != 0) {
        const CallArg none[1] = {};
        (void)mem.call(self - 0x10, none, 0);
        (void)mem.call(self - 0x10, none, 0);
    }
    mem.st(self + 0x2C, 0);
    return out;
}

// 0x00782E70. Secondary table 00C13440 slot 40.
RunResult childHasMarker(Mem& mem, std::uint32_t self, std::uint32_t arg) {
    RunResult out;
    const std::uint32_t child = mem.ld(self - 8);
    if (mem.ld(child + 0x374) == 0) {
        out.eax = child & 0xFFFFFF00u;
        return out;
    }
    const CallArg args[] = {rawArg(arg)};
    if ((mem.call(self, args, 1) & 0xFFu) == 0) {
        return out;
    }
    out.eax = 1;
    return out;
}

// 0x00783940. Secondary table 00BF9A40 slot 3. Forwards two args to 00780E20.
RunResult forwardPair(Mem& mem, std::uint32_t left, std::uint32_t right) {
    const CallArg args[] = {rawArg(left), rawArg(right)};
    RunResult out;
    out.eax = (mem.call(0, args, 2) & 0xFFu) != 0 ? 1u : 0u;
    return out;
}

// 0x00783E70. Secondary table 00C07B48 slot 3. A non-positive field returns 0 before the forward.
RunResult forwardIfPositive(Mem& mem, std::uint32_t self, std::uint32_t left, std::uint32_t right) {
    RunResult out;
    const std::uint32_t field = mem.ld(self + 0x10);
    const float sample = mem.ldf(field + 0x10);
    if (!(0.0F < sample)) {
        out.eax = field & 0xFFFFFF00u;
        return out;
    }
    const CallArg args[] = {rawArg(left), rawArg(right)};
    out.eax = (mem.call(self, args, 2) & 0xFFu) != 0 ? 1u : 0u;
    return out;
}

// 0x007843D0. 00C15200 slot 29.
RunResult classifyPair(Mem& mem, std::uint32_t self, std::uint32_t key, std::uint32_t extra) {
    const CallArg args[] = {rawArg(key), rawArg(extra)};
    RunResult out;
    if ((mem.call(self, args, 2) & 0xFFu) == 0) {
        return out;
    }
    const CallArg next[] = {rawArg(0), rawArg(key), rawArg(2)};
    const std::uint32_t code = mem.call(mem.ld(self - 8), next, 3);
    out.eax = (code == 2 || code == 3) ? 1u : 0u;
    return out;
}

// 0x00784490. 00C183F0 slot 30. On a hit, calls slot +0x50 with FLT_MAX.
RunResult probeWithMaxFloat(Mem& mem, std::uint32_t self, std::uint32_t arg) {
    const CallArg args[] = {rawArg(arg)};
    RunResult out;
    if ((mem.call(self, args, 1) & 0xFFu) == 0) {
        return out;
    }
    const CallArg probe[] = {rawArg(0), rawArg(0), rawArg(mem.ld(0x00BE700Cu))};
    if ((mem.call(self, probe, 3) & 0xFFu) == 0) {
        return out;
    }
    out.eax = 1;
    return out;
}

// 0x00784910. 00C00F4C slot 20.
RunResult gateAndForward(Mem& mem, std::uint32_t self, std::uint32_t a, std::uint32_t b, std::uint32_t c) {
    const CallArg none[1] = {};
    const std::uint32_t obj = mem.call(self, none, 0);
    const std::uint32_t kind = mem.ld(mem.ld(obj + 8) + 0x14);
    RunResult out;
    if (kind != 0 && kind != 3) {
        out.eax = kind & 0xFFFFFF00u;
        return out;
    }
    const CallArg args[] = {rawArg(a), rawArg(0), rawArg(b), rawArg(c)};
    out.eax = mem.call(self - 0x10, args, 4);
    return out;
}

// 0x00786DA0. 00C15200 slot 27. Same gate, then a different argument order.
RunResult gateAndForwardAlt(Mem& mem, std::uint32_t self, std::uint32_t a, std::uint32_t b, std::uint32_t c) {
    (void)b;
    const CallArg none[1] = {};
    const std::uint32_t obj = mem.call(self, none, 0);
    const std::uint32_t kind = mem.ld(mem.ld(obj + 8) + 0x14);
    RunResult out;
    if (kind != 0 && kind != 3) {
        out.eax = kind & 0xFFFFFF00u;
        return out;
    }
    const CallArg args[] = {rawArg(a), rawArg(0), rawArg(0), rawArg(c)};
    out.eax = (mem.call(self - 0x10, args, 4) & 0xFFu) != 0 ? 1u : 0u;
    return out;
}

} // namespace

bool runObjectUpdates78(std::uint32_t va, Mem& mem, std::uint32_t ecx, const std::uint32_t* args, int argc, RunResult& out) {
    const std::uint32_t a0 = argc > 0 ? args[0] : 0;
    const std::uint32_t a1 = argc > 1 ? args[1] : 0;
    const std::uint32_t a2 = argc > 2 ? args[2] : 0;
    const std::uint32_t a3 = argc > 3 ? args[3] : 0;
    switch (va) {
    case 0x007885E0u:
        out = writeModuleDataWord(mem, ecx, a0);
        return true;
    case 0x0078B730u:
        out = xferDrawModule(mem, ecx, a0);
        return true;
    case 0x007874A0u:
        out = xferGatherEject(mem, ecx, a0);
        return true;
    case 0x007877D0u:
        out = xferSqueezeFling(mem, ecx, a0);
        return true;
    case 0x00781ED0u:
        out = stepBalloon(mem, ecx);
        return true;
    case 0x007879F0u:
        out = xferLineSegment(mem, ecx, a0);
        return true;
    case 0x00788470u:
        out = lineSegmentNameKey(mem);
        return true;
    case 0x00787BD0u:
        out = tickSqueeze(mem, ecx);
        return true;
    case 0x007806F0u:
        out = applyTimedBonus(mem, ecx, a0);
        return true;
    case 0x00782E20u:
        out = releaseAttached(mem, ecx);
        return true;
    case 0x00782E70u:
        out = childHasMarker(mem, ecx, a0);
        return true;
    case 0x00783940u:
        out = forwardPair(mem, a0, a1);
        return true;
    case 0x00783E70u:
        out = forwardIfPositive(mem, ecx, a0, a1);
        return true;
    case 0x007843D0u:
        out = classifyPair(mem, ecx, a0, a1);
        return true;
    case 0x00784490u:
        out = probeWithMaxFloat(mem, ecx, a0);
        return true;
    case 0x00784910u:
        out = gateAndForward(mem, ecx, a0, a1, a2);
        return true;
    case 0x00786DA0u:
        out = gateAndForwardAlt(mem, ecx, a0, a1, a2);
        return true;
    default:
        (void)a3;
        return false;
    }
}

} // namespace ra3::gamelogic
