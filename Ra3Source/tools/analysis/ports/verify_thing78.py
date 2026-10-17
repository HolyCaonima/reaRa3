"""Release oracle for the clean bodies ported from 0x00780000-0x0078FFFF.

The checker is the reconstruction. This script executes the shipped bytes.
Call traces record ecx and each stack argument: a raw value, or the first
dword of a stack slot. EAX and ST0 are compared when the case asks for them.
"""

from __future__ import annotations

import struct
import subprocess
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
import unicorn
from emulate import STACK_BASE, STACK_SIZE, Emulator  # noqa: E402
from unicorn import x86_const as uc  # noqa: E402

ROOT = Path(__file__).resolve().parents[3]
EXE = Path(sys.argv[1]) if len(sys.argv) > 1 and sys.argv[1].endswith(".exe") else (
    ROOT / "build" / "win32" / "bin" / "Release" / "ra3_thing78check.exe")
FILTER = sys.argv[-1] if len(sys.argv) > 1 and not sys.argv[-1].endswith(".exe") else ""

HEAP = 0x60000000
S0 = 0x7FD00000
S4 = S0 + 0x10
S8 = S0 + 0x20
S12 = S0 + 0x30
S16 = S0 + 0x40
FP = (
    uc.UC_X86_REG_FP0, uc.UC_X86_REG_FP1, uc.UC_X86_REG_FP2, uc.UC_X86_REG_FP3,
    uc.UC_X86_REG_FP4, uc.UC_X86_REG_FP5, uc.UC_X86_REG_FP6, uc.UC_X86_REG_FP7,
)
DIRECT = {
    0x00499640: 0, 0x004A62E0: 0, 0x004ED9C0: 12, 0x00528920: 0, 0x005A94C0: 0,
    0x005B9F30: 0, 0x00685080: 4, 0x00694000: 0, 0x006A5900: 8, 0x006B3200: 0,
    0x006C1560: 4, 0x006C3A10: 8, 0x00702390: 0, 0x0071D020: 0, 0x00751740: 0,
    0x007578F0: 12, 0x007782F0: 4, 0x007796D0: 0, 0x00779950: 8, 0x0077CE10: 0,
    0x0077DA40: 0, 0x00780E20: 8, 0x007828E0: 16, 0x00782C20: 8, 0x00782D80: 4,
    0x00786A20: 0, 0x00786BA0: 0, 0x0085CBC0: 16, 0x008D82F0: 4, 0x00986150: 4,
    0x00AAFE10: 8,
}

COUNTED = {
    0x007806F0, 0x00781ED0, 0x00782E20, 0x00782E70, 0x00783940, 0x00783E70, 0x007843D0,
    0x00784490, 0x00784910, 0x00786DA0, 0x007874A0, 0x007877D0, 0x007879F0, 0x00788470,
    0x007885E0, 0x00788650, 0x007886C0, 0x007886E0, 0x00788730, 0x00788750, 0x00788760,
    0x00788770, 0x00788800, 0x00787BD0, 0x0078B690, 0x0078B730, 0x0078B7A0, 0x0078B870,
    0x0078B8E0, 0x0078B990, 0x0078BA00, 0x0078BB60, 0x0078BDA0, 0x0078BDF0, 0x0078BF60,
    0x0078BFB0, 0x0078CB90, 0x0078D4F0, 0x0078D520, 0x0078D560,
}


def hx(value: int) -> str:
    return f"{value & 0xFFFFFFFF:08x}"


def p(offset: int) -> int:
    return HEAP + offset


class Blob:
    def __init__(self, size: int = 0x400) -> None:
        self.raw = bytearray(size)

    def d(self, offset: int, value: int) -> "Blob":
        struct.pack_into("<I", self.raw, offset, value & 0xFFFFFFFF)
        return self

    def at(self, offset: int, target: int) -> "Blob":
        return self.d(offset, p(target))


class Sink:
    tape: list[tuple[int, int, int, bool]] = []
    index = 0
    recorded: list[list[int]] = []


SINK = Sink()


def push_f32(emu: Emulator, bits: int) -> None:
    bits &= 0xFFFFFFFF
    sign = bits >> 31
    exp = (bits >> 23) & 0xFF
    frac = bits & 0x7FFFFF
    if exp == 0 or exp == 255:
        mant = 0 if frac == 0 and exp == 0 else (1 << 63) | (frac << 40)
        exponent = (sign << 15) | (0x7FFF if exp == 255 else 0)
    else:
        mant = (1 << 63) | (frac << 40)
        exponent = (sign << 15) | ((exp - 127 + 16383) & 0xFFFF)
    fpsw = emu.uc.reg_read(uc.UC_X86_REG_FPSW)
    top = (fpsw >> 11) & 7
    new_top = (top - 1) & 7
    fpsw = (fpsw & ~(7 << 11)) | (new_top << 11)
    emu.uc.reg_write(uc.UC_X86_REG_FPSW, fpsw)
    tag = emu.uc.reg_read(uc.UC_X86_REG_FPTAG) & ~(3 << (2 * new_top))
    emu.uc.reg_write(uc.UC_X86_REG_FPTAG, tag)
    emu.uc.reg_write(FP[new_top], (mant, exponent))


def on_call(emu: Emulator) -> int:
    eax, st0, nargs, push = SINK.tape[SINK.index]
    SINK.index += 1
    ecx = emu.register(uc.UC_X86_REG_ECX)
    row = [nargs & 0xFFFFFFFF, ecx & 0xFFFFFFFF]
    for index in range(nargs):
        value = emu.stack_arg(index) & 0xFFFFFFFF
        if STACK_BASE <= value < STACK_BASE + STACK_SIZE:
            row.extend((1, struct.unpack("<I", emu.read(value, 4))[0]))
        else:
            row.extend((0, value))
    SINK.recorded.append(row)
    if push:
        push_f32(emu, st0)
    return eax


def prepare(emu: Emulator) -> None:
    emu.uc.mem_map(S0, 0x1000, unicorn.UC_PROT_ALL)
    for address, cleanup in ((S0, 0), (S4, 4), (S8, 8), (S12, 12), (S16, 16)):
        emu.bind_function(address, on_call, cleanup)
    for address, cleanup in DIRECT.items():
        emu.bind_function(address, on_call, cleanup)


class Case:
    def __init__(self, va, blob, ecx, args, globs, tape, mode) -> None:
        self.va = va
        self.blob = bytes(blob)
        self.ecx = ecx
        self.args = tuple(args)
        self.globs = tuple(globs)
        self.tape = tuple(tape)
        self.mode = mode

    def checker_line(self) -> str:
        blob = "-" if not self.blob else self.blob.hex()
        args = "-" if not self.args else ",".join(hx(arg) for arg in self.args)
        globs = "-" if not self.globs else ",".join(f"{hx(va)}={hx(val)}" for va, val in self.globs)
        if not self.tape:
            tape = "-"
        else:
            parts = []
            for eax, st0, _nargs, push in self.tape:
                bits = f"f{hx(st0)}" if push else hx(st0)
                parts.append(f"{hx(eax)}:{bits}")
            tape = ";".join(parts)
        return f"call {hx(self.va)} {hx(self.ecx)} {blob} {args} {globs} {tape}"


CASES: list[Case] = []


def add(va, blob, ecx=0, args=(), globs=(), tape=(), mode="e") -> None:
    CASES.append(Case(va, blob.raw if isinstance(blob, Blob) else blob, ecx, args, globs, tape, mode))


def build() -> None:
    b = Blob().at(0, 4).d(4, 0x12345678)
    add(0x007886E0, b, ecx=p(0))
    b = Blob().at(0, 8).d(8, 0x00000002)
    add(0x007886E0, b, ecx=p(0))

    b = Blob().at(0x14, 0x20).d(0x150, 0xAABBCCDD)
    add(0x00788730, b, ecx=p(0))
    b = Blob().at(0x14, 0x30).d(0x160, 0)
    add(0x00788730, b, ecx=p(0))

    b = Blob().d(0x14, p(0x40))
    add(0x00788750, b, ecx=p(0))
    b = Blob().d(0x14, 0)
    add(0x00788750, b, ecx=p(0))

    b = Blob().d(0x2C4, 0x55AA55AA)
    add(0x00788760, b, ecx=p(0))
    b = Blob().d(0x2C4, 0)
    add(0x00788760, b, ecx=p(0))

    b = Blob()
    add(0x00788650, b, ecx=p(0), args=(0, 0x99))
    b = Blob().at(0x40, 0x80).d(0x90, S0)
    add(0x00788650, b, ecx=p(0), args=(p(0x40), 0x42), tape=((0x777, 0, 0, False),))

    b = Blob().d(0x48, 2).d(0x58, 0x3F800000)
    add(0x007886C0, b, ecx=p(0), mode="s")
    b = Blob().at(0, 0x20).d(0x20, S0)
    add(0x007886C0, b, ecx=p(0), tape=((0x11, 0x40000000, 0, True),), mode="b")

    add(0x00788770, Blob(), ecx=p(0), args=(0, 0, 7))
    b = Blob().d(0x304, 0x00001234)
    add(0x00788770, b, ecx=p(0), args=(0, 0, 0xFFFFFFFF))
    b = Blob().at(0x80, 0xA0).d(0xA0 + 0xA0, S4)
    add(0x00788770, b, ecx=p(0), args=(1, p(0x10), 3), tape=(
        (p(0x80), 0, 1, False), (0, 0x40000000, 1, True)))

    rules = Blob().d(0x350, 0)
    add(0x00788800, rules, ecx=p(0), args=(0, 0, 4, 0), globs=((0x00DEBA64, p(0x200)),), mode="b")
    b = Blob().at(0x14, 0x40).d(0xC8, 0x40400000).d(0x350, 0)
    add(0x00788800, b, ecx=p(0), args=(0, 0, 0xFFFFFFFF, 0), globs=((0x00DEBA64, p(0x200)),), mode="b")

    add(0x0078B7A0, Blob(0x80), ecx=p(0), args=(0,), globs=((0x00DF9D24, 5),))
    add(0x0078B7A0, Blob(0x80), ecx=p(0), args=(0x42,), globs=((0x00DF9D24, 0),))

    b = Blob().d(0x48, 1).d(0x4C, 0x3F000000).d(0x50, 0x3E800000).d(0x54, 0)
    add(0x0078B870, b, ecx=p(0))
    b = Blob().d(0x44, 0).d(0x48, 0)
    add(0x0078B870, b, ecx=p(0))

    b = Blob().d(0x48, 1).d(0x4C, 0x3F800000).d(0x50, 0).d(0x54, 0)
    add(0x0078B8E0, b, ecx=p(0), args=(p(0x80),))
    b = Blob().d(0x44, 0).d(0x48, 0)
    add(0x0078B8E0, b, ecx=p(0), args=(p(0x90),))

    b = Blob().d(0x48, 4).d(0x5C, 0x40400000)
    add(0x0078B990, b, ecx=p(0), mode="s")
    b = Blob().d(0x48, 2).d(0x58, 0x3F800000).d(0x38, 0x3F800000).d(0x3C, 0x40000000)
    add(0x0078B990, b, ecx=p(0), globs=((0x00DFC198, 0x111),), tape=((0, 0, 4, False),), mode="s")

    ident = Blob()
    ident.d(0x08, 0x3F800000).d(0x1C, 0x3F800000).d(0x30, 0x3F800000)
    ident.d(0x80, 0x40000000).d(0x84, 0x40400000).d(0x88, 0x40800000)
    add(0x0078BA00, ident, ecx=p(0), args=(p(0x80), 0, p(0x90), 0))
    add(0x0078BA00, ident, ecx=p(0), args=(p(0x80), 0, 0, 0))

    b = Blob().d(0x350, 0)
    add(0x0078BDA0, b, ecx=p(0), args=(0, 0, 4, 0), globs=((0x00DEBA64, p(0x200)), (0x00CB0910, 2)))
    b = Blob().d(0x350, 0)
    add(0x0078BDA0, b, ecx=p(0), args=(0, 0, 0, 0), globs=((0x00DEBA64, p(0x200)), (0x00CB0910, 0)))

    both = Blob().at(4, 0x20).d(0xF0, 1 << 13).d(0x98, 1 << 3)
    add(0x0078BDF0, both, args=(p(0),))
    one = Blob().at(4, 0x20).d(0xF0, 1 << 13).d(0x98, 0)
    add(0x0078BDF0, one, args=(p(0),))
    add(0x0078BDF0, Blob().at(4, 0x20), args=(p(0),))

    span = Blob().at(0x1A8, 0x100).d(0x1AC, p(0x114))
    span.at(0x100 + 4, 0x180).d(0x180, 0x77)
    for begin, end in ((0x1CC, 0x1D0), (0x1F0, 0x1F4), (0x214, 0x218)):
        span.d(begin, p(0x200)).d(end, p(0x200))
    add(0x0078BB60, span, ecx=p(0), args=(0x77,))
    add(0x0078BB60, span, ecx=p(0), args=(0x78,))
    empty = Blob()
    for begin, end in ((0x1A8, 0x1AC), (0x1CC, 0x1D0), (0x1F0, 0x1F4), (0x214, 0x218)):
        empty.d(begin, p(0x200)).d(end, p(0x200))
    add(0x0078BB60, empty, ecx=p(0), args=(1,))

    col = Blob().d(0x18, 0x3F800000)
    add(0x0078BFB0, col, ecx=p(0), args=(p(0x40),))
    add(0x0078BFB0, Blob(), ecx=p(0), args=(p(0x40),))
    col = Blob().d(8, 0x40400000).d(0x28, 0x40A00000)
    add(0x0078BFB0, col, ecx=p(0), args=(p(0x40),))

    miss = Blob().d(0x100, 0x99).d(0x120, p(0x28))
    add(0x0078BF60, miss, ecx=p(0), args=(0x1, p(0x100), 0), tape=((p(0x120), 0, 2, False),))
    hit = Blob().d(0x100, 0x99).d(0x120, p(0x140)).d(0x154, p(0x160)).at(0x160, 0x180).d(0x180, S8)
    add(0x0078BF60, hit, ecx=p(0), args=(0x1, p(0x100), 0), tape=(
        (p(0x120), 0, 2, False), (0x55, 0, 2, False)))

    name = Blob().d(0x40, 0xABC).at(0, 0x80).d(0x80 + 0x58, S8).d(0x80 + 0x30, S4).d(0x80 + 0x3C, S4)
    add(0x0078B690, name, args=(p(0), p(0x40)), tape=(
        (0, 0, 2, False), (0, 0, 2, False), (0, 0, 1, False)))
    add(0x0078B690, name, args=(p(0), p(0x40)), tape=((0x777, 0, 2, False), (0, 0, 1, False)))

    filled = Blob().d(0x2AC, 1).d(0x2B0, 1).d(0x2B4, 1).d(0x2B8, 1)
    add(0x0078CB90, filled, ecx=p(0))
    one = Blob().at(0x14, 0x40).d(0x40 + 0x98, 0x1000).d(0x2B0, 1).d(0x2B4, 1).d(0x2B8, 1)
    add(0x0078CB90, one, ecx=p(0), tape=((0x42, 0, 3, False),))

    add(0x0078D4F0, Blob(), ecx=p(0))
    node = Blob().at(0x24, 0x80)
    for off in (0x2AC, 0x2B0, 0x2B4, 0x2B8):
        node.d(0x80 + off, 1)
    add(0x0078D4F0, node, ecx=p(0), tape=((0x99, 0, 1, False),))

    add(0x0078D520, Blob(), ecx=p(0), args=(5,))
    b = Blob().d(0x294, 1).d(0x300, p(0x284))
    add(0x0078D520, b, ecx=p(0), args=(5,), tape=((p(0x300), 0, 2, False),))
    b = Blob().d(0x294, 1).d(0x300, p(0x320))
    add(0x0078D520, b, ecx=p(0), args=(9,), tape=((p(0x300), 0, 2, False),))

    add(0x0078D560, Blob().d(0, p(0x20)).d(4, p(0x20)), args=(p(0), p(0x10)))
    dense = Blob().d(0, p(0x20)).d(4, p(0x24)).d(0x10, 0xF).at(0x20, 0x40).d(0x48, 0xF)
    add(0x0078D560, dense, args=(p(0), p(0x10)))
    zero = Blob().d(0, p(0x20)).d(4, p(0x24)).d(0x10, 0xF).at(0x20, 0x40)
    add(0x0078D560, zero, args=(p(0), p(0x10)))

    word = Blob().at(4, 0x10).d(0x14, 0xABCD)
    add(0x007885E0, word, ecx=p(0), args=(p(0x20),))
    word = Blob().at(4, 0x10)
    add(0x007885E0, word, ecx=p(0), args=(p(0x20),))

    xfer = Blob().at(0x200, 0x100)
    xfer.d(0x100 + 0x18, S4).d(0x100 + 0x84, S4)
    add(0x0078B730, xfer, ecx=p(0), args=(p(0x200),), tape=tuple((0, 0, 1, False) for _ in range(4)))
    add(0x0078B730, xfer, ecx=p(0x10), args=(p(0x200),), tape=tuple((0, 0, 1, False) for _ in range(4)))

    g = Blob().at(0x200, 0x100)
    g.d(0x100 + 0x18, S4).d(0x100 + 0x58, S4).d(0x100 + 0x6C, S4)
    gtape = ((0, 0, 1, False),) * 2 + ((0, 0, 1, False),) * 2 + ((0, 0, 2, False),) + ((0, 0, 1, False),) * 2
    add(0x007874A0, g, ecx=p(0), args=(p(0x200),), tape=gtape)
    add(0x007874A0, g, ecx=p(0x20), args=(p(0x200),), tape=gtape)

    s = Blob().at(0x200, 0x100)
    s.d(0x100 + 0x18, S4).d(0x100 + 0x6C, S4)
    stape = ((0, 0, 1, False),) * 2 + ((0, 0, 1, False),) * 3 + ((0, 0, 2, False),) * 2 + ((0, 0, 1, False),)
    add(0x007877D0, s, ecx=p(0), args=(p(0x200),), tape=stape)
    add(0x007877D0, s, ecx=p(0x30), args=(p(0x200),), tape=stape)

    bal = Blob().at(0x18, 0x20).at(0x20 + 0x24, 0x40)
    add(0x00781ED0, bal, ecx=p(0), tape=((0, 0, 0, False),) * 4)
    add(0x00781ED0, bal, ecx=p(0), tape=((1, 0, 0, False),))

    line = Blob().at(0x30, 0x30).at(0x200, 0x100)
    line.d(0x100 + 0x18, S4).d(0x100 + 0x6C, S4).d(0x100 + 0x84, S4)
    ltape = ((0, 0, 1, False),) * 2 + ((0, 0, 1, False),) * 2 + ((0, 0, 1, False),) + ((0, 0, 1, False),)
    add(0x007879F0, line, ecx=p(0), args=(p(0x200),), tape=ltape)
    flagged = Blob().at(0x30, 0x30).at(0x200, 0x100).d(0x204, 2)
    flagged.d(0x100 + 0x18, S4).d(0x100 + 0x6C, S4).d(0x100 + 0x84, S4)
    add(0x007879F0, flagged, ecx=p(0), args=(p(0x200),), tape=ltape)

    add(0x00788470, Blob(), globs=((0x00DF9D1C, 0), (0x00DF9D18, 0), (0x00E1A078, 0x222)),
        tape=((0x333, 0, 1, False),))
    add(0x00788470, Blob(), globs=((0x00DF9D1C, 1), (0x00DF9D18, 0x444), (0x00E1A078, 0x222)))

    tick = Blob().d(0x44, 5).d(0x34, p(0x100)).d(0x38, p(0x100))
    add(0x00787BD0, tick, ecx=p(0))
    tick = Blob().d(0x24, p(0x80)).d(0x28, p(0x80)).d(0x34, p(0x100)).d(0x38, p(0x100)).at(4, 0x40)
    add(0x00787BD0, tick, ecx=p(0), tape=((0x15, 0, 4, False),))
    tick = Blob().d(0x44, 1).d(0x24, p(0x80)).d(0x28, p(0x80)).d(0x34, p(0x100)).d(0x38, p(0x100)).at(4, 0x40)
    add(0x00787BD0, tick, ecx=p(0), tape=((7, 0, 4, False),))

    bonus = Blob().d(0x158, 10).d(0x30, 7)
    add(0x007806F0, bonus, ecx=p(0x20), args=(3,), globs=((0x00DEBA64, p(0x100)),),
        tape=((0, 0, 3, False), (0x66, 0, 0, False)))
    add(0x007806F0, bonus, ecx=p(0x20), args=(0x3FFFFFFF,), globs=((0x00DEBA64, p(0x100)),),
        tape=((0, 0, 3, False), (0x21, 0, 0, False)))

    rel = Blob()
    add(0x00782E20, rel, ecx=p(0x20))
    rel = Blob().d(0x20 + 0x2C, 5).d(0x20 + 0x30, 0x100)
    add(0x00782E20, rel, ecx=p(0x20), tape=((0, 0, 0, False), (0, 0, 0, False)))

    child = Blob().at(0x18, 0x40)
    add(0x00782E70, child, ecx=p(0x20), args=(9,))
    child = Blob().at(0x18, 0x40).d(0x40 + 0x374, 1)
    add(0x00782E70, child, ecx=p(0x20), args=(9,), tape=((0, 0, 1, False),))
    add(0x00782E70, child, ecx=p(0x20), args=(9,), tape=((1, 0, 1, False),))

    add(0x00783940, Blob(), args=(0x11, 0x22), tape=((1, 0, 2, False),))
    add(0x00783940, Blob(), args=(0x11, 0x22), tape=((0, 0, 2, False),))

    pos = Blob().at(0x10, 0x30)
    add(0x00783E70, pos, ecx=p(0), args=(1, 2))
    pos = Blob().at(0x10, 0x30).d(0x40, 0x3F800000)
    add(0x00783E70, pos, ecx=p(0), args=(1, 2), tape=((1, 0, 2, False),))
    add(0x00783E70, pos, ecx=p(0), args=(1, 2), tape=((0, 0, 2, False),))

    body = Blob().at(0x18, 0x50)
    add(0x007843D0, body, ecx=p(0x20), args=(0x3, 0x4), tape=((0, 0, 2, False),))
    add(0x007843D0, body, ecx=p(0x20), args=(0x3, 0x4), tape=((1, 0, 2, False), (3, 0, 3, False)))
    add(0x007843D0, body, ecx=p(0x20), args=(0x3, 0x4), tape=((1, 0, 2, False), (1, 0, 3, False)))

    probe = Blob().at(0, 0x40).d(0x40 + 0x50, S12)
    add(0x00784490, probe, ecx=p(0), args=(8,), tape=((0, 0, 1, False),))
    add(0x00784490, probe, ecx=p(0), args=(8,), tape=((1, 0, 1, False), (0, 0, 3, False)))
    add(0x00784490, probe, ecx=p(0), args=(8,), tape=((1, 0, 1, False), (1, 0, 3, False)))

    gate = Blob().at(0x20, 0x40).d(0x40 + 0x1C, S0).d(0x68, p(0x80)).d(0x94, 1)
    add(0x00784910, gate, ecx=p(0x20), args=(1, 2, 3), tape=((p(0x60), 0, 0, False),))
    gate = Blob().at(0x20, 0x40).d(0x40 + 0x1C, S0).d(0x68, p(0x80)).d(0x94, 0)
    add(0x00784910, gate, ecx=p(0x20), args=(1, 2, 3), tape=((p(0x60), 0, 0, False), (0x77, 0, 4, False)))
    gate = Blob().at(0x20, 0x40).d(0x40 + 0x1C, S0).d(0x68, p(0x80)).d(0x94, 3)
    add(0x00784910, gate, ecx=p(0x20), args=(1, 2, 3), tape=((p(0x60), 0, 0, False), (0, 0, 4, False)))

    alt = Blob().at(0x20, 0x40).d(0x40 + 0x1C, S0).d(0x68, p(0x80)).d(0x94, 1)
    add(0x00786DA0, alt, ecx=p(0x20), args=(4, 5, 6), tape=((p(0x60), 0, 0, False),))
    alt = Blob().at(0x20, 0x40).d(0x40 + 0x1C, S0).d(0x68, p(0x80)).d(0x94, 0)
    add(0x00786DA0, alt, ecx=p(0x20), args=(4, 5, 6), tape=((p(0x60), 0, 0, False), (1, 0, 4, False)))
    alt = Blob().at(0x20, 0x40).d(0x40 + 0x1C, S0).d(0x68, p(0x80)).d(0x94, 3)
    add(0x00786DA0, alt, ecx=p(0x20), args=(4, 5, 6), tape=((p(0x60), 0, 0, False), (0, 0, 4, False)))


def fields(line: str) -> dict[str, str]:
    out = {}
    for part in line.split(" "):
        key, value = part.split("=", 1)
        out[key] = value
    return out


def run_emu(emu: Emulator, case: Case) -> str:
    emu.reset_data()
    emu.write(0x00E4354C, struct.pack("<I", 1))
    for address, value in case.globs:
        emu.write(address, struct.pack("<I", value & 0xFFFFFFFF))
    blob_at = emu.alloc(case.blob if case.blob else b"\x00")
    if blob_at != HEAP:
        raise RuntimeError(f"heap base {blob_at:#x}")
    emu.uc.reg_write(uc.UC_X86_REG_FPCW, 0x027F)
    emu.uc.reg_write(uc.UC_X86_REG_FPSW, 0)
    emu.uc.reg_write(uc.UC_X86_REG_FPTAG, 0xFFFF)
    SINK.tape = list(case.tape)
    SINK.index = 0
    SINK.recorded = []
    if case.mode in ("s", "b"):
        result, bits = emu.call_float32(case.va, case.args, ecx=case.ecx, fpu_control_word=0x027F)
        st0 = bits
    else:
        result = emu.call(case.va, case.args, ecx=case.ecx)
        st0 = 0
    if SINK.index != len(case.tape):
        raise RuntimeError(f"tape used {SINK.index} of {len(case.tape)}")
    blob = emu.read(HEAP, len(case.blob)).hex() if case.blob else "-"
    if case.globs:
        glob = ",".join(
            f"{hx(address)}={hx(struct.unpack('<I', emu.read(address, 4))[0])}" for address, _ in case.globs)
    else:
        glob = "-"
    if SINK.recorded:
        trace = ",".join(hx(word) for row in SINK.recorded for word in row)
    else:
        trace = "-"
    return f"eax={hx(result.eax)} st0={hx(st0)} blob={blob} glob={glob} trace={trace}"


def main() -> int:
    build()
    selected = [case for case in CASES if not FILTER or FILTER in hx(case.va)]
    counts: dict[int, int] = {}
    for case in CASES:
        counts[case.va] = counts.get(case.va, 0) + 1
    missing = sorted(va for va in COUNTED if counts.get(va, 0) < 2)
    if missing and not FILTER:
        print("thin " + " ".join(hx(va) for va in missing))
    checker = subprocess.Popen(
        [str(EXE)], stdin=subprocess.PIPE, stdout=subprocess.PIPE, text=True, bufsize=1)
    emu = Emulator()
    prepare(emu)
    mismatches = 0
    assert checker.stdin is not None and checker.stdout is not None
    for index, case in enumerate(selected):
        line = case.checker_line()
        checker.stdin.write(line + "\n")
        checker.stdin.flush()
        cpp = checker.stdout.readline().strip()
        try:
            native = run_emu(emu, case)
        except Exception as error:  # noqa: BLE001 - one case must not hide the rest
            native = f"emu {error}"
        if cpp.startswith("error") or cpp == "miss" or native.startswith("emu"):
            mismatches += 1
            print(f"FAIL {hx(case.va)} #{index}\n  cpp {cpp}\n  nat {native}")
            continue
        left = fields(cpp)
        right = fields(native)
        bad = []
        for key in ("blob", "glob", "trace"):
            if left.get(key) != right.get(key):
                bad.append(key)
        if case.mode in ("e", "b") and left.get("eax") != right.get("eax"):
            bad.append("eax")
        if case.mode in ("s", "b") and left.get("st0") != right.get("st0"):
            bad.append("st0")
        if bad:
            mismatches += 1
            print(f"FAIL {hx(case.va)} #{index} {','.join(bad)}")
            for key in bad:
                print(f"  {key}\n    cpp {left.get(key)}\n    nat {right.get(key)}")
            if mismatches >= 25:
                print("stopping after 25 mismatches")
                break
    checker.stdin.write("quit\n")
    checker.stdin.flush()
    checker.wait(timeout=20)
    print(f"cases={len(selected)} mismatches={mismatches}")
    return 1 if mismatches or (missing and not FILTER) else 0


if __name__ == "__main__":
    sys.exit(main())
