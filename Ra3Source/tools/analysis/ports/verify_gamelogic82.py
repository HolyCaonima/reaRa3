"""Differential oracle for the named-object script queries at 0x00838D80-0x0083FF90.

Unicorn executes the shipped bytes. Callees are stubbed the same way on both
sides, including the AsciiString helpers, so the counted functions' own
control flow, full EAX and memory writes are what the comparison checks.
"""

from __future__ import annotations

import argparse
import struct
import subprocess
import sys
from pathlib import Path

from unicorn import x86_const as x86

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from emulate import Emulator  # noqa: E402

ROOT = Path(__file__).resolve().parents[4]
ENGINE = 0x00DFC058
FLAG = 0x00DFC164
GATE = 0x00BD9528
LOOKUP = 0x00837420
LOOKUP_ECX = 0xC0FFEE11
LOOKUP_EDX = 0xD0FFEE22
COPY_EAX = 0xA5A5A5C0
DTOR_EAX = 0xA5A5A500
CTOR_EAX = 0xA5A5A5B0
APPEND_EAX = 0xA5A5A5D0
DEBUG_EAX = 0xA5A5A5E0

# cleanup, argument count recorded at the call
DIRECT = {
    0x0052DB30: (4, 1),
    0x006CCF40: (0, 0),
    0x006CCF10: (0, 0),
    0x006879B0: (0, 0),
    0x006C5BE0: (8, 2),
    0x004D9AF0: (4, 1),
    0x00686FA0: (0, 0),
    0x007E1550: (8, 2),
    0x004EDD70: (4, 1),
    0x007483A0: (12, 3),
    0x00687000: (4, 1),
    0x00822890: (0, 2),
    0x00524530: (4, 1),
    0x00706890: (8, 2),
    0x004DD080: (8, 2),
    0x004DCFF0: (8, 2),
    0x00757B40: (8, 2),
    0x007E2950: (4, 1),
    0x00687720: (4, 1),
    0x0054F490: (4, 1),
    0x0081B730: (0, 1),
    0x00401B40: (0, 2),
    0x004D9AB0: (4, 1),
    0x00803800: (0, 2),
    0x006A7600: (0, 0),
    0x00549BA0: (4, 1),
    0x0071C890: (8, 2),
    0x007070A0: (4, 1),
    0x006EE8F0: (0x14, 5),
    0x006EE9E0: (8, 2),
    0x0081F0D0: (0xC, 3),
    0x006D2020: (8, 2),
    0x00861A70: (4, 1),
    0x006F0910: (8, 2),
    0x006EE7B0: (4, 1),
    0x0081F6C0: (0, 0),
    0x004E6D30: (0, 1),
    0x00401AD0: (0, 2),
    0x0076C400: (4, 1),
    0x006F0A20: (4, 1),
    0x008230C0: (0, 1),
    0x006C5710: (0xC, 3),
    0x006C57A0: (0xC, 3),
    0x006867D0: (0, 0),
    0x0082BA30: (4, 1),
    0x006A5AB0: (4, 1),
    0x006A5B10: (4, 1),
    0x00658A80: (0, 6),
    0x0081BFF0: (4, 1),
}


def u32(value: int) -> int:
    return value & 0xFFFFFFFF


def p32(value: int) -> bytes:
    return struct.pack("<I", u32(value))


class Scene:
    def __init__(self, va: int, ret: int, name: str, rank: bool = False) -> None:
        self.va = va
        self.ret = ret
        self.name = name
        self.rank = rank
        self.ecx = 0
        self.args: list[int] = []
        self.mem: dict[int, int] = {}
        self.stubs: list[tuple] = []
        self.watches: list[tuple[int, int]] = []
        self.next = 0x60000010
        self.w32(ENGINE, 0x60000000)

    def alloc(self, size: int) -> int:
        addr = self.next
        if (addr & 0xFF) == 0:
            addr += 0x10
        self.next = (addr + size + 15) & ~15
        for i in range(size):
            self.mem[addr + i] = 0
        return addr

    def w32(self, addr: int, value: int) -> None:
        for i in range(4):
            self.mem[addr + i] = (value >> (8 * i)) & 0xFF

    def w8(self, addr: int, value: int) -> None:
        self.mem[addr] = value & 0xFF

    def obj(self) -> int:
        return self.alloc(0x480)

    def param(self, obj: int) -> int:
        addr = self.alloc(0x30)
        self.w32(addr, obj)
        return addr

    def stub(self, target: int, eax: int = 0x111, ecx: int = 0, edx: int = 0,
             poke: int = -1, val: int = 0) -> None:
        self.stubs.append((target, u32(eax), u32(ecx), u32(edx), poke, val & 0xFF))


class Runtime:
    def __init__(self) -> None:
        self.emu = Emulator()
        self.queue: list[tuple] = []
        self.trace: list[tuple] = []
        self.v4 = [self._virtual(4) for _ in range(4)]
        self.v12 = [self._virtual(12) for _ in range(2)]
        self._bind(LOOKUP, 4, self._lookup)
        self._bind(0x004CA8F0, 4, self._copy)
        self._bind(0x004CA930, 0, self._dtor)
        self._bind(0x004CBFF0, 4, self._ctor)
        self._bind(0x004CB6E0, 8, self._append)
        self._bind(0x0081FCE0, 8, self._debug)
        for target, (cleanup, argc) in DIRECT.items():
            self._bind(target, cleanup, lambda emu, t=target, n=argc: self._queued(emu, t, n))

    def _bind(self, address: int, cleanup: int, fn) -> None:
        self.emu.bind_function(address, fn, cleanup)

    def _virtual(self, cleanup: int) -> int:
        box: dict[str, int] = {}

        def stub(emu, box=box, n=cleanup // 4):
            return self._queued(emu, box["addr"], n)

        addr = self.emu.bind_stub(stub, cleanup)
        box["addr"] = addr
        return addr

    def _note(self, emu, target: int, argc: int) -> None:
        args = tuple(emu.stack_arg(i) for i in range(argc))
        self.trace.append((
            target,
            emu.register(x86.UC_X86_REG_ECX),
            emu.register(x86.UC_X86_REG_EAX),
            args,
        ))

    def _lookup(self, emu) -> int:
        self._note(emu, LOOKUP, 1)
        obj = struct.unpack("<I", emu.read(emu.stack_arg(0), 4))[0]
        emu.uc.reg_write(x86.UC_X86_REG_ECX, LOOKUP_ECX)
        emu.uc.reg_write(x86.UC_X86_REG_EDX, LOOKUP_EDX)
        return obj

    def _copy(self, emu) -> int:
        self._note(emu, 0x004CA8F0, 1)
        source = emu.stack_arg(0)
        dest = emu.register(x86.UC_X86_REG_ECX)
        emu.write(dest, emu.read(source, 4))
        return COPY_EAX

    def _dtor(self, emu) -> int:
        self._note(emu, 0x004CA930, 0)
        return DTOR_EAX

    def _ctor(self, emu) -> int:
        self._note(emu, 0x004CBFF0, 1)
        dest = emu.register(x86.UC_X86_REG_ECX)
        emu.write(dest, p32(emu.stack_arg(0)))
        return CTOR_EAX

    def _append(self, emu) -> int:
        self._note(emu, 0x004CB6E0, 2)
        return APPEND_EAX

    def _debug(self, emu) -> int:
        self._note(emu, 0x0081FCE0, 2)
        return DEBUG_EAX

    def _queued(self, emu, target: int, argc: int) -> int:
        if not self.queue:
            raise RuntimeError(f"unexpected call 0x{target:08X}")
        item = self.queue.pop(0)
        if item[0] != target:
            raise RuntimeError(f"call 0x{target:08X} but queue has 0x{item[0]:08X}")
        if item[4] >= 0:
            ptr = emu.stack_arg(item[4])
            emu.write(ptr, bytes([item[5]]))
        self._note(emu, target, argc)
        emu.uc.reg_write(x86.UC_X86_REG_ECX, item[2])
        emu.uc.reg_write(x86.UC_X86_REG_EDX, item[3])
        return item[1]

    def run(self, scene: Scene):
        self.queue = list(scene.stubs)
        self.trace = []
        self.emu.reset_data()
        for addr, data in runs(scene.mem):
            self.emu.write(addr, data)
        result = self.emu.call(scene.va, scene.args, ecx=scene.ecx)
        if self.queue:
            raise RuntimeError(f"unused stubs {[hex(s[0]) for s in self.queue]}")
        if result.stack_cleanup != scene.ret:
            raise RuntimeError(
                f"cleanup {result.stack_cleanup} != {scene.ret}")
        watches = []
        for addr, size in scene.watches:
            watches.append((addr, self.emu.read(addr, size)))
        return result.eax, list(self.trace), watches


def runs(mem: dict[int, int]) -> list[tuple[int, bytes]]:
    out = []
    start = prev = None
    buf = bytearray()
    for addr in sorted(mem):
        if prev is None or addr != prev + 1:
            if start is not None:
                out.append((start, bytes(buf)))
            start = addr
            buf = bytearray()
        buf.append(mem[addr])
        prev = addr
    if start is not None:
        out.append((start, bytes(buf)))
    return out


def render(scenes: list[Scene]) -> str:
    lines = []
    for scene in scenes:
        lines.append("case")
        lines.append(f"va {scene.va:08x}")
        lines.append(f"ecx {scene.ecx:08x}")
        for arg in scene.args:
            lines.append(f"arg {arg:08x}")
        for addr, data in runs(scene.mem):
            lines.append(f"mem {addr:08x} {data.hex()}")
        for target, eax, ecx, edx, poke, val in scene.stubs:
            line = f"stub {target:08x} {eax:08x} {ecx:08x} {edx:08x}"
            if poke >= 0:
                line += f" {poke:x} {val:02x}"
            lines.append(line)
        for addr, size in scene.watches:
            lines.append(f"watch {addr:08x} {size:x}")
        lines.append("end")
    return "\n".join(lines) + "\n"


def parse_output(text: str) -> list:
    cases = []
    lines = [line for line in text.splitlines() if line != ""]
    index = 0
    while index < len(lines):
        if lines[index].startswith("error"):
            message = [lines[index]]
            index += 1
            while index < len(lines) and lines[index] != "end":
                message.append(lines[index])
                index += 1
            index += 1
            cases.append(("error", " ".join(message)))
            continue
        eax = int(lines[index].split()[1], 16)
        index += 1
        parts = lines[index].split()
        if parts[0] != "trace":
            raise SystemExit(f"expected trace, got {lines[index]}")
        trace = []
        for event in parts[1:]:
            target, ecx, eaxin, args = event.split(":")
            arg_list = () if args == "-" else tuple(int(x, 16) for x in args.split(","))
            trace.append((int(target, 16), int(ecx, 16), int(eaxin, 16), arg_list))
        index += 1
        watches = []
        while lines[index] != "end":
            _, addr, data = lines[index].split()
            watches.append((int(addr, 16), bytes.fromhex(data)))
            index += 1
        index += 1
        cases.append((eax, trace, watches))
    return cases


def build(rt: Runtime) -> list[Scene]:
    cases: list[Scene] = []

    def add(scene: Scene) -> None:
        cases.append(scene)

    def module(va: int, target: int) -> None:
        s = Scene(va, 8, f"{va:08x} null")
        s.args = [s.param(0), 0x42]
        add(s)
        s = Scene(va, 8, f"{va:08x} no-module")
        obj = s.obj()
        s.args = [s.param(obj), 0x42]
        add(s)
        s = Scene(va, 8, f"{va:08x} call")
        obj = s.obj()
        s.w32(obj + 0xF4, s.alloc(0x10))
        s.args = [s.param(obj), 0x42]
        s.stub(target, 0x222)
        add(s)

    module(0x00838D80, 0x0052DB30)
    module(0x0083B260, 0x00524530)

    s = Scene(0x00839EA0, 8, "ai null obj")
    s.args = [s.param(0), 0]
    add(s)
    s = Scene(0x00839EA0, 8, "ai no ai")
    obj = s.obj()
    s.args = [s.param(obj), 1]
    add(s)
    for byte, target in ((0, 0x006CCF40), (1, 0x006CCF10)):
        s = Scene(0x00839EA0, 8, f"ai byte {byte}")
        obj = s.obj()
        s.w32(obj + 0x374, s.alloc(0x30))
        s.args = [s.param(obj), byte]
        s.stub(target, 0x333)
        add(s)

    s = Scene(0x0083A570, 4, "repulsor null")
    s.args = [s.param(0)]
    add(s)
    s = Scene(0x0083A570, 4, "repulsor no ai")
    obj = s.obj()
    s.args = [s.param(obj)]
    add(s)
    s = Scene(0x0083A570, 4, "repulsor call")
    obj = s.obj()
    s.w32(obj + 0x374, s.alloc(0x40))
    s.args = [s.param(obj)]
    s.stub(0x006879B0, 0x10)
    s.stub(0x006C5BE0, 0x444)
    add(s)

    s = Scene(0x0083A990, 4, "sell null")
    s.args = [s.param(0)]
    add(s)
    s = Scene(0x0083A990, 4, "sell no ai")
    obj = s.obj()
    s.args = [s.param(obj)]
    add(s)
    s = Scene(0x0083A990, 4, "sell call")
    obj = s.obj()
    s.w32(obj + 0x374, 1)
    s.args = [s.param(obj)]
    s.stub(0x004D9AF0, 0x1)
    s.stub(0x00686FA0, 0x555)
    s.stub(0x007E1550, 0x666)
    add(s)

    s = Scene(0x0083ABD0, 4, "hand null")
    s.args = [s.param(0)]
    add(s)
    s = Scene(0x0083ABD0, 4, "hand call")
    obj = s.obj()
    s.w32(0x00DEBA64, 0x60000020)
    s.args = [s.param(obj)]
    s.stub(0x004EDD70, 0x777)
    add(s)

    s = Scene(0x0083AC60, 4, "stance null")
    s.args = [s.param(0)]
    add(s)
    s = Scene(0x0083AC60, 4, "stance call")
    s.args = [s.param(s.obj())]
    s.stub(0x007483A0, 0x888)
    add(s)

    s = Scene(0x0083AC90, 8, "xp null")
    s.args = [s.param(0), 7]
    add(s)
    s = Scene(0x0083AC90, 8, "xp call")
    s.args = [s.param(s.obj()), 7]
    s.stub(0x00687000, 0x999)
    add(s)

    s = Scene(0x0083AFC0, 8, "pos null")
    s.args = [s.param(0), 3]
    add(s)
    s = Scene(0x0083AFC0, 8, "pos call")
    s.args = [s.param(s.obj()), 3]
    s.stub(0x00822890, 0xAAA)
    add(s)

    s = Scene(0x0083B290, 8, "rel null")
    s.args = [s.param(0), 0]
    add(s)
    for flag in (0, 1, 0x80):
        s = Scene(0x0083B290, 8, f"rel {flag}")
        s.args = [s.param(s.obj()), flag]
        s.stub(0x00706890, 0xB0)
        add(s)

    s = Scene(0x0083B2D0, 8, "fixed null")
    s.args = [s.param(0), 9]
    add(s)
    s = Scene(0x0083B2D0, 8, "fixed call")
    s.args = [s.param(s.obj()), 9]
    s.stub(0x00706890, 0xB1)
    add(s)

    s = Scene(0x0083B300, 4, "prodbit null")
    s.args = [s.param(0)]
    add(s)
    s = Scene(0x0083B300, 4, "prodbit no ai")
    s.args = [s.param(s.obj())]
    add(s)
    for bits, target in ((0, 0x004DCFF0), (1 << 10, 0x004DD080)):
        s = Scene(0x0083B300, 4, f"prodbit {bits:x}")
        obj = s.obj()
        templ = s.alloc(0xD0)
        s.w32(obj + 4, templ)
        s.w32(templ + 0xCC, bits)
        s.w32(obj + 0x374, s.alloc(0x30))
        s.args = [s.param(obj)]
        s.stub(target, 0xB2)
        add(s)

    s = Scene(0x0083B350, 8, "recruit null")
    s.args = [s.param(0), 0]
    add(s)
    for flag in (0, 1):
        s = Scene(0x0083B350, 8, f"recruit {flag}")
        s.args = [s.param(s.obj()), flag]
        s.stub(0x00757B40, 0xB3)
        add(s)

    s = Scene(0x0083B390, 8, "float null")
    s.w32(GATE, 0x3F000000)
    s.args = [s.param(0), 0x3F800000]
    add(s)
    s = Scene(0x0083B390, 8, "float no ai")
    s.w32(GATE, 0x3F000000)
    s.args = [s.param(s.obj()), 0x3F800000]
    add(s)
    s = Scene(0x0083B390, 8, "float no body")
    s.w32(GATE, 0x3F000000)
    obj = s.obj()
    s.w32(obj + 0x374, s.alloc(0x220))
    s.args = [s.param(obj), 0x3F800000]
    add(s)
    for name, bits in (("below", 0x3E800000), ("equal", 0x3F000000),
                       ("above", 0x3F800000), ("nan", 0x7FC00000)):
        s = Scene(0x0083B390, 8, f"float {name}")
        s.w32(GATE, 0x3F000000)
        obj = s.obj()
        ai = s.alloc(0x220)
        body = s.alloc(0x40)
        s.w32(obj + 0x374, ai)
        s.w32(ai + 0x214, body)
        s.args = [s.param(obj), bits]
        s.watches.append((body + 0x34, 4))
        add(s)

    s = Scene(0x0083B3D0, 4, "unheld null")
    s.args = [s.param(0)]
    add(s)
    s = Scene(0x0083B3D0, 4, "unheld bit")
    obj = s.obj()
    s.w8(obj + 0x471, 1)
    s.args = [s.param(obj)]
    add(s)
    s = Scene(0x0083B3D0, 4, "unheld virtual")
    obj = s.obj()
    vt = s.alloc(0x50)
    mgr = s.alloc(0x10)
    s.w32(vt + 0x4C, rt.v4[0])
    s.w32(mgr, vt)
    s.w32(0x00DEEE5C, mgr)
    s.args = [s.param(obj)]
    s.stub(rt.v4[0], 0xB4)
    add(s)

    s = Scene(0x0083B410, 0xC, "weapon null obj")
    s.w32(0x00CC569C, 0x60000030)
    s.args = [s.param(0), s.param(1), 4]
    s.stub(0x007E2950, 0x55)
    add(s)
    s = Scene(0x0083B410, 0xC, "weapon no player")
    s.w32(0x00CC569C, 0x60000030)
    s.args = [s.param(s.obj()), s.param(1), 4]
    s.stub(0x007E2950, 0)
    add(s)
    s = Scene(0x0083B410, 0xC, "weapon none")
    s.w32(0x00CC569C, 0x60000030)
    s.args = [s.param(s.obj()), s.param(1), 4]
    s.stub(0x007E2950, 0x60)
    s.stub(0x00687720, 0)
    add(s)
    s = Scene(0x0083B410, 0xC, "weapon call")
    s.w32(0x00CC569C, 0x60000030)
    weapon = s.alloc(0x10)
    vt = s.alloc(0x2C)
    s.w32(vt + 0x28, rt.v4[0])
    s.w32(weapon, vt)
    s.args = [s.param(s.obj()), s.param(1), 0x77]
    s.stub(0x007E2950, 0x60)
    s.stub(0x00687720, weapon)
    s.stub(rt.v4[0], 0xB5)
    add(s)

    s = Scene(0x0083C430, 4, "one null")
    s.args = [s.param(0)]
    add(s)
    s = Scene(0x0083C430, 4, "one no ai")
    s.args = [s.param(s.obj())]
    add(s)
    s = Scene(0x0083C430, 4, "one call")
    obj = s.obj()
    s.w32(obj + 0x374, s.alloc(0x30))
    s.args = [s.param(obj)]
    s.stub(0x0054F490, 0xC0)
    add(s)

    s = Scene(0x0083C640, 0xC, "pass null")
    s.args = [s.param(0), 0x11, 0x22]
    add(s)
    s = Scene(0x0083C640, 0xC, "pass call")
    s.args = [s.param(s.obj()), 0x11, 0x22]
    s.stub(0x0081B730, 0xC1)
    add(s)

    s = Scene(0x0083C8D0, 8, "class null")
    s.args = [s.param(0), s.alloc(0x10)]
    add(s)
    s = Scene(0x0083C8D0, 8, "class miss")
    spec = s.alloc(0x10)
    s.args = [s.param(s.obj()), spec]
    s.stub(0x00401B40, 0xC2, poke=1, val=0)
    add(s)
    s = Scene(0x0083C8D0, 8, "class hit")
    spec = s.alloc(0x10)
    s.args = [s.param(s.obj()), spec]
    s.stub(0x00401B40, 0xC3, poke=1, val=1)
    s.stub(0x004D9AB0, 0xC4)
    add(s)

    s = Scene(0x0083C920, 8, "link null")
    s.args = [s.param(0), 5]
    add(s)
    s = Scene(0x0083C920, 8, "link call")
    s.args = [s.param(s.obj()), 5]
    s.stub(0x00803800, 0xC5)
    add(s)

    s = Scene(0x0083C950, 8, "enable null")
    s.args = [s.param(0), 0]
    add(s)
    s = Scene(0x0083C950, 8, "enable empty")
    s.args = [s.param(s.obj()), 0]
    s.stub(0x006A7600, 0)
    add(s)
    s = Scene(0x0083C950, 8, "enable stop")
    produced = s.alloc(0x10)
    vt = s.alloc(0x110)
    s.w32(produced, vt)
    s.w32(vt + 0x10C, rt.v4[0])
    s.args = [s.param(s.obj()), 0]
    s.stub(0x006A7600, produced)
    s.stub(rt.v4[0], 0)
    add(s)
    s = Scene(0x0083C950, 8, "enable go")
    produced = s.alloc(0x10)
    vt = s.alloc(0x110)
    s.w32(produced, vt)
    s.w32(vt + 0x10C, rt.v4[0])
    s.w32(vt + 0x108, rt.v12[0])
    s.args = [s.param(s.obj()), 0]
    s.stub(0x006A7600, produced)
    s.stub(rt.v4[0], 1)
    s.stub(rt.v12[0], 0xC6)
    add(s)

    def attack(va: int, final: int) -> None:
        s = Scene(va, 8, f"{va:08x} no obj")
        s.w32(0x00DEEE80, 0x60000040)
        s.args = [s.param(0), s.param(0)]
        add(s)
        s = Scene(va, 8, f"{va:08x} no target")
        s.w32(0x00DEEE80, 0x60000040)
        obj = s.obj()
        templ = s.alloc(0xD0)
        s.w32(obj + 4, templ)
        s.args = [s.param(1), s.param(obj)]
        s.stub(0x00549BA0, 0)
        add(s)
        s = Scene(va, 8, f"{va:08x} bit")
        s.w32(0x00DEEE80, 0x60000040)
        obj = s.obj()
        templ = s.alloc(0xD0)
        s.w32(obj + 4, templ)
        s.w32(templ + 0xCC, 1 << 10)
        s.args = [s.param(1), s.param(obj)]
        s.stub(0x00549BA0, 0xABC)
        add(s)
        s = Scene(va, 8, f"{va:08x} call")
        s.w32(0x00DEEE80, 0x60000040)
        obj = s.obj()
        templ = s.alloc(0xD0)
        s.w32(obj + 4, templ)
        s.args = [s.param(1), s.param(obj)]
        s.stub(0x00549BA0, 0xABC)
        s.stub(final, 0xC7)
        add(s)

    attack(0x0083CB30, 0x0071C890)
    attack(0x0083CB80, 0x007070A0)

    def scaled(va: int, target: int) -> None:
        s = Scene(va, 8, f"{va:08x} null")
        s.args = [s.param(0), 2]
        add(s)
        s = Scene(va, 8, f"{va:08x} no tracker")
        s.args = [s.param(s.obj()), 2]
        add(s)
        s = Scene(va, 8, f"{va:08x} call")
        obj = s.obj()
        s.w32(obj + 0x3CC, s.alloc(0x30))
        s.args = [s.param(obj), 0xFFFFFFF6]
        s.stub(target, 0xC8)
        add(s)

    scaled(0x0083CC30, 0x006EE8F0)
    scaled(0x0083CC70, 0x006EE9E0)

    s = Scene(0x0083CCB0, 8, "rankwrite null")
    s.args = [s.param(0), 9]
    add(s)
    s = Scene(0x0083CCB0, 8, "rankwrite none")
    s.args = [s.param(s.obj()), 9]
    add(s)
    s = Scene(0x0083CCB0, 8, "rankwrite set")
    obj = s.obj()
    tracker = s.alloc(0x30)
    s.w32(obj + 0x3CC, tracker)
    s.args = [s.param(obj), 0x1234]
    s.watches.append((tracker + 0x28, 4))
    add(s)

    s = Scene(0x0083CDD0, 0xC, "this null")
    s.ecx = 0x60000ABC
    s.args = [s.param(0), 1, 2]
    add(s)
    s = Scene(0x0083CDD0, 0xC, "this call")
    s.ecx = 0x60000ABC
    s.args = [s.param(s.obj()), 1, 2]
    s.stub(0x0081F0D0, 0xC9)
    add(s)

    s = Scene(0x0083DA80, 8, "flagcmd null")
    spec = s.alloc(0x10)
    s.args = [s.param(0), spec]
    add(s)
    for value in (0, 5):
        s = Scene(0x0083DA80, 8, f"flagcmd {value}")
        spec = s.alloc(0x10)
        s.w32(spec + 8, value)
        s.args = [s.param(s.obj()), spec]
        s.stub(0x006D2020, 0xDA)
        add(s)

    s = Scene(0x0083DAC0, 8, "upgrade null")
    s.w32(0x00DFC190, 0x60000050)
    s.args = [s.param(0), 1]
    add(s)
    s = Scene(0x0083DAC0, 8, "upgrade missing")
    s.w32(0x00DFC190, 0x60000050)
    s.args = [s.param(s.obj()), 1]
    s.stub(0x00861A70, 0)
    add(s)
    s = Scene(0x0083DAC0, 8, "upgrade call")
    s.w32(0x00DFC190, 0x60000050)
    found = s.alloc(0x20)
    s.args = [s.param(s.obj()), 1]
    s.stub(0x00861A70, found)
    s.stub(0x006F0910, 0xDB)
    add(s)

    s = Scene(0x0083DC70, 8, "xparg null")
    s.args = [s.param(0), 3]
    add(s)
    s = Scene(0x0083DC70, 8, "xparg no tracker")
    s.args = [s.param(s.obj()), 3]
    add(s)
    s = Scene(0x0083DC70, 8, "xparg call")
    obj = s.obj()
    s.w32(obj + 0x3CC, s.alloc(0x10))
    s.args = [s.param(obj), 3]
    s.stub(0x006EE7B0, 0xDC)
    add(s)

    s = Scene(0x0083DD90, 0xC, "two null")
    s.args = [s.param(0), 1, 2]
    add(s)
    s = Scene(0x0083DD90, 0xC, "two call")
    s.args = [s.param(s.obj()), 1, 2]
    s.stub(0x006D2020, 0xDD)
    add(s)

    s = Scene(0x0083DDC0, 8, "bind none")
    s.args = [s.param(0), s.param(s.obj())]
    add(s)
    s = Scene(0x0083DDC0, 8, "bind second none")
    s.args = [s.param(s.obj()), s.param(0)]
    add(s)
    s = Scene(0x0083DDC0, 8, "bind call")
    s.args = [s.param(s.obj()), s.param(s.obj())]
    s.stub(0x0081F6C0, 0xDE)
    add(s)

    s = Scene(0x0083E220, 0xC, "attach none")
    s.args = [s.param(1), s.param(s.obj()), 0]
    s.stub(0x004E6D30, 0)
    add(s)
    s = Scene(0x0083E220, 0xC, "attach no obj")
    host = s.alloc(0x10)
    s.args = [s.param(1), s.param(0), 1]
    s.stub(0x004E6D30, host)
    add(s)
    for flag, slot in ((0, 0x2C), (1, 0x34)):
        s = Scene(0x0083E220, 0xC, f"attach {flag}")
        host = s.alloc(0x10)
        vt = s.alloc(0x38)
        s.w32(host, vt)
        s.w32(vt + slot, rt.v4[0])
        s.args = [s.param(1), s.param(s.obj()), flag]
        s.stub(0x004E6D30, host)
        s.stub(rt.v4[0], 0xE0)
        add(s)

    s = Scene(0x0083E270, 0xC, "play null")
    s.args = [s.param(0), s.alloc(4), 0]
    add(s)
    for flag, target in ((0, 0x006F0A20), (1, 0x0076C400)):
        s = Scene(0x0083E270, 0xC, f"play {flag}")
        text = s.alloc(4)
        s.args = [s.param(s.obj()), text, flag]
        s.stub(0x00401AD0, 0xE1)
        s.stub(target, 0xE2)
        add(s)

    s = Scene(0x0083E350, 4, "publish null")
    s.args = [s.param(0)]
    add(s)
    s = Scene(0x0083E350, 4, "publish none")
    s.args = [s.param(s.obj())]
    add(s)
    s = Scene(0x0083E350, 4, "publish call")
    obj = s.obj()
    s.w32(obj + 0xF4, s.alloc(0x10))
    s.args = [s.param(obj)]
    s.stub(0x008230C0, 0xE3)
    add(s)

    s = Scene(0x0083E380, 8, "guard second only")
    second = s.obj()
    s.args = [s.param(0), s.param(second)]
    add(s)
    s = Scene(0x0083E380, 8, "guard no second")
    s.args = [s.param(s.obj()), s.param(0)]
    add(s)
    s = Scene(0x0083E380, 8, "guard no ai")
    s.args = [s.param(s.obj()), s.param(s.obj())]
    add(s)
    s = Scene(0x0083E380, 8, "guard call")
    first = s.obj()
    s.w32(first + 0x374, s.alloc(0x30))
    s.args = [s.param(first), s.param(s.obj())]
    s.stub(0x006C5710, 0xE4)
    add(s)

    s = Scene(0x0083E3D0, 8, "guardup null")
    s.w32(0x00DFC190, 0x60000060)
    s.args = [s.param(0), 1]
    add(s)
    s = Scene(0x0083E3D0, 8, "guardup missing")
    s.w32(0x00DFC190, 0x60000060)
    s.args = [s.param(s.obj()), 1]
    s.stub(0x00861A70, 0)
    add(s)
    s = Scene(0x0083E3D0, 8, "guardup no ai")
    s.w32(0x00DFC190, 0x60000060)
    s.args = [s.param(s.obj()), 1]
    s.stub(0x00861A70, s.alloc(0x20))
    add(s)
    s = Scene(0x0083E3D0, 8, "guardup call")
    s.w32(0x00DFC190, 0x60000060)
    obj = s.obj()
    s.w32(obj + 0x374, s.alloc(0x30))
    s.args = [s.param(obj), 1]
    s.stub(0x00861A70, s.alloc(0x20))
    s.stub(0x006C57A0, 0xE5)
    add(s)

    def rank_case(name: str, rank: int, limit: int, tracker_null: bool = False) -> None:
        s = Scene(0x0083E520, 8, name, rank=not tracker_null)
        obj = s.obj()
        if not tracker_null:
            tracker = s.alloc(0x30)
            s.w32(obj + 0x3CC, tracker)
            s.w32(tracker + 0x24, u32(rank))
        lim = s.alloc(0x10)
        s.w32(lim + 8, u32(limit))
        s.w8(FLAG, 0)
        s.watches.append((FLAG, 1))
        s.args = [s.param(obj), lim]
        add(s)

    rank_case("rank equal", 5, 5)
    rank_case("rank less", 4, 5)
    rank_case("rank greater", 6, 5)
    rank_case("rank negative", -1, 0)
    rank_case("rank neg pass", -1, -2)
    rank_case("rank no tracker", 0, 1, tracker_null=True)
    s = Scene(0x0083E520, 8, "rank miss")
    s.w8(FLAG, 0)
    s.watches.append((FLAG, 1))
    s.args = [s.param(0), s.alloc(0x10)]
    add(s)
    s = Scene(0x0083E520, 8, "rank miss once")
    s.w8(FLAG, 1)
    s.watches.append((FLAG, 1))
    s.args = [s.param(0), s.alloc(0x10)]
    add(s)

    s = Scene(0x0083E5D0, 4, "bit found 0")
    obj = s.obj()
    s.args = [s.param(obj)]
    add(s)
    s = Scene(0x0083E5D0, 4, "bit found 1")
    obj = s.obj()
    s.w8(obj + 0x471, 0xF1)
    s.args = [s.param(obj)]
    add(s)
    for value in (0, 0x55):
        s = Scene(0x0083E5D0, 4, f"bit miss {value:x}")
        s.args = [s.param(0)]
        s.stub(0x0081BFF0, 0xA5A5A500 | value)
        add(s)

    s = Scene(0x0083E630, 4, "clear null")
    s.args = [s.param(0)]
    add(s)
    for byte in (0, 1, 0xFE):
        s = Scene(0x0083E630, 4, f"clear {byte:x}")
        obj = s.obj()
        s.w8(obj + 0x471, byte)
        s.args = [s.param(obj)]
        add(s)

    s = Scene(0x0083E660, 4, "keep found")
    obj = s.obj()
    s.w8(obj + 0x471, 1)
    s.args = [s.param(obj)]
    add(s)
    s = Scene(0x0083E660, 4, "keep miss")
    s.args = [s.param(0)]
    s.stub(0x0081BFF0, 0x11)
    add(s)

    s = Scene(0x0083E6C0, 4, "destroyed found")
    s.args = [s.param(s.obj())]
    add(s)
    for value in (0, 1):
        s = Scene(0x0083E6C0, 4, f"destroyed miss {value}")
        s.args = [s.param(0)]
        s.stub(0x0081BFF0, value)
        add(s)

    s = Scene(0x0083EBA0, 4, "present no")
    s.args = [s.param(0)]
    add(s)
    s = Scene(0x0083EBA0, 4, "present yes")
    s.args = [s.param(s.obj())]
    add(s)

    s = Scene(0x0083ED80, 4, "count null")
    s.args = [s.param(0)]
    add(s)
    s = Scene(0x0083ED80, 4, "count empty")
    s.args = [s.param(s.obj())]
    s.stub(0x006A7600, 0)
    add(s)
    s = Scene(0x0083ED80, 4, "count zero")
    produced = s.alloc(0x10)
    vt = s.alloc(0x110)
    s.w32(produced, vt)
    s.w32(vt + 0x10C, rt.v4[0])
    s.args = [s.param(s.obj())]
    s.stub(0x006A7600, produced)
    s.stub(rt.v4[0], 0)
    add(s)
    s = Scene(0x0083ED80, 4, "count bits")
    produced = s.alloc(0x10)
    vt = s.alloc(0x110)
    s.w32(produced, vt)
    s.w32(vt + 0x10C, rt.v4[0])
    s.args = [s.param(s.obj())]
    s.stub(0x006A7600, produced)
    s.stub(rt.v4[0], 0x1200)
    add(s)

    def team(va: int, final: int, bit: bool) -> None:
        s = Scene(va, 8, f"{va:08x} null")
        s.args = [s.param(0), s.alloc(0x20)]
        add(s)
        if bit:
            s = Scene(va, 8, f"{va:08x} bit")
            obj = s.obj()
            templ = s.alloc(0xD0)
            s.w32(obj + 4, templ)
            s.w32(templ + 0xC8, 1 << 0x19)
            s.args = [s.param(obj), s.alloc(0x20)]
            add(s)
        s = Scene(va, 8, f"{va:08x} empty team")
        obj = s.obj()
        templ = s.alloc(0xD0)
        s.w32(obj + 4, templ)
        s.args = [s.param(obj), s.alloc(0x20)]
        s.stub(0x0082BA30, 0)
        add(s)
        s = Scene(va, 8, f"{va:08x} call")
        obj = s.obj()
        templ = s.alloc(0xD0)
        s.w32(obj + 4, templ)
        s.args = [s.param(obj), s.alloc(0x20)]
        s.stub(0x0082BA30, 0x70)
        s.stub(final, 0xE6)
        add(s)

    team(0x0083F6A0, 0x006A5AB0, True)
    team(0x0083F700, 0x006A5B10, False)

    s = Scene(0x0083F7E0, 8, "kind null")
    spec = s.alloc(0x10)
    s.args = [s.param(0), spec]
    add(s)
    for index, word, value in ((0, 1, 1), (0, 0, 0), (31, 1 << 31, 1),
                               (32, 1, 1), (33, 0, 0), (5, 1 << 5, 1)):
        s = Scene(0x0083F7E0, 8, f"kind {index}")
        obj = s.obj()
        s.w32(obj + 0x80 + (index >> 5) * 4, u32(word))
        spec = s.alloc(0x10)
        s.w32(spec + 8, index)
        s.args = [s.param(obj), spec]
        add(s)

    s = Scene(0x0083FA40, 4, "frame no param")
    s.args = [0]
    add(s)
    s = Scene(0x0083FA40, 4, "frame no obj")
    s.args = [s.param(0)]
    add(s)
    s = Scene(0x0083FA40, 4, "frame no tracker")
    s.args = [s.param(s.obj())]
    add(s)
    for name, rank, frame in (("above", 10, 0xABC), ("equal", 5, 5), ("below", 1, 9),
                              ("neg", -2, 0x100)):
        s = Scene(0x0083FA40, 4, f"frame {name}")
        obj = s.obj()
        tracker = s.alloc(0x30)
        s.w32(obj + 0x3CC, tracker)
        s.w32(tracker + 0x24, u32(rank))
        s.args = [s.param(obj)]
        s.stub(0x006867D0, u32(frame))
        add(s)

    s = Scene(0x0083FA80, 8, "count no param")
    s.args = [0, 0]
    add(s)
    s = Scene(0x0083FA80, 8, "count no spec")
    s.args = [s.param(0), 0]
    add(s)
    s = Scene(0x0083FA80, 8, "count no obj")
    s.args = [s.param(0), s.alloc(0x30)]
    add(s)
    s = Scene(0x0083FA80, 8, "count expected 0")
    spec = s.alloc(0x30)
    s.args = [s.param(s.obj()), spec]
    add(s)
    s = Scene(0x0083FA80, 8, "count no tracker")
    spec = s.alloc(0x30)
    s.w32(spec + 0x2C, 4)
    s.args = [s.param(s.obj()), spec]
    add(s)
    for name, actual, expected in (("eq", 0x11223344, 0x11223344),
                                   ("ne", 0x11223344, 0x99)):
        s = Scene(0x0083FA80, 8, f"count {name}")
        obj = s.obj()
        tracker = s.alloc(0x10)
        s.w32(obj + 0x3CC, tracker)
        s.w32(tracker + 8, actual)
        spec = s.alloc(0x30)
        s.w32(spec + 0x2C, expected)
        s.w32(spec + 0x28, 1)
        s.args = [s.param(obj), spec]
        add(s)

    s = Scene(0x0083FDB0, 8, "contain none")
    s.args = [s.param(0), s.param(s.obj())]
    add(s)
    s = Scene(0x0083FDB0, 8, "contain no second")
    s.args = [s.param(s.obj()), s.param(0)]
    add(s)
    s = Scene(0x0083FDB0, 8, "contain call")
    s.args = [s.param(s.obj()), s.param(s.obj())]
    s.stub(0x00658A80, 0xF0)
    add(s)

    s = Scene(0x0083FF90, 4, "positive null")
    s.args = [s.param(0)]
    add(s)
    s = Scene(0x0083FF90, 4, "positive empty")
    s.args = [s.param(s.obj())]
    s.stub(0x006A7600, 0)
    add(s)
    s = Scene(0x0083FF90, 4, "positive zero")
    produced = s.alloc(0x10)
    vt = s.alloc(0x110)
    s.w32(produced, vt)
    s.w32(vt + 0x10C, rt.v4[0])
    s.args = [s.param(s.obj())]
    s.stub(0x006A7600, produced)
    s.stub(0x006A7600, produced)
    s.stub(rt.v4[0], 0)
    add(s)
    s = Scene(0x0083FF90, 4, "positive bits")
    produced = s.alloc(0x10)
    vt = s.alloc(0x110)
    s.w32(produced, vt)
    s.w32(vt + 0x10C, rt.v4[0])
    s.args = [s.param(s.obj())]
    s.stub(0x006A7600, produced)
    s.stub(0x006A7600, produced)
    s.stub(rt.v4[0], 0x1200)
    add(s)

    covered = {scene.va for scene in cases}
    missing = sorted(set(DIRECT_FUNCS) - covered)
    if missing:
        raise SystemExit("missing cases " + ", ".join(f"{va:08x}" for va in missing))
    return cases


DIRECT_FUNCS = {
    0x00838D80, 0x00839EA0, 0x0083A570, 0x0083A990, 0x0083ABD0, 0x0083AC60,
    0x0083AC90, 0x0083AFC0, 0x0083B260, 0x0083B290, 0x0083B2D0, 0x0083B300,
    0x0083B350, 0x0083B390, 0x0083B3D0, 0x0083B410, 0x0083C430, 0x0083C640,
    0x0083C8D0, 0x0083C920, 0x0083C950, 0x0083CB30, 0x0083CB80, 0x0083CC30,
    0x0083CC70, 0x0083CCB0, 0x0083CDD0, 0x0083DA80, 0x0083DAC0, 0x0083DC70,
    0x0083DD90, 0x0083DDC0, 0x0083E220, 0x0083E270, 0x0083E350, 0x0083E380,
    0x0083E3D0, 0x0083E520, 0x0083E5D0, 0x0083E630, 0x0083E660, 0x0083E6C0,
    0x0083EBA0, 0x0083ED80, 0x0083F6A0, 0x0083F700, 0x0083F7E0, 0x0083FA40,
    0x0083FA80, 0x0083FDB0, 0x0083FF90,
}


def compare(scenes, native, other, label: str) -> list[str]:
    errors = []
    if len(other) != len(scenes):
        return [f"{label} returned {len(other)} cases, expected {len(scenes)}"]
    for scene, got in zip(scenes, other):
        if isinstance(got[0], str):
            errors.append(f"{scene.name}: {got[1]}")
            continue
        eax, trace, watches = got
        n_eax, n_trace, n_watches = native[scene]
        if eax != n_eax or trace != n_trace or watches != n_watches:
            errors.append(
                f"{scene.name}: eax {eax:08x}/{n_eax:08x} "
                f"trace {len(trace)}/{len(n_trace)} watch {watches != n_watches}")
            if trace != n_trace:
                for index, (left, right) in enumerate(zip(trace, n_trace)):
                    if left != right:
                        errors.append(f"  event {index} cpp {left} uni {right}")
                        break
                if len(trace) != len(n_trace):
                    errors.append(f"  trace len cpp {len(trace)} uni {len(n_trace)}")
    return errors


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--check", type=Path,
                        default=ROOT / "Ra3Source/build/win32/bin/Release/ra3_gamelogic82check.exe")
    parser.add_argument("--mutant", type=Path,
                        default=ROOT / "Ra3Source/build/win32/bin/Release/ra3_gamelogic82check_mutant.exe")
    args = parser.parse_args()
    runtime = Runtime()
    scenes = build(runtime)
    native = {}
    for scene in scenes:
        try:
            native[scene] = runtime.run(scene)
        except Exception as error:  # noqa: BLE001 - oracle reports the case
            print(f"unicorn {scene.name}: {error}")
            return 1
    payload = render(scenes).encode()
    normal = subprocess.run([str(args.check)], input=payload, capture_output=True)
    if normal.returncode != 0 and not normal.stdout:
        print(normal.stderr.decode(errors="replace"))
        return 1
    parsed = parse_output(normal.stdout.decode(errors="replace"))
    errors = compare(scenes, native, parsed, "check")
    print(f"cases {len(scenes)}")
    print(f"mismatches {len(errors)}")
    for line in errors[:20]:
        print(line)
    mutant = subprocess.run([str(args.mutant)], input=payload, capture_output=True)
    mutant_parsed = parse_output(mutant.stdout.decode(errors="replace"))
    detected = 0
    unexpected = []
    if len(mutant_parsed) != len(scenes):
        unexpected.append(f"mutant returned {len(mutant_parsed)}")
    else:
        for scene, got in zip(scenes, mutant_parsed):
            if isinstance(got[0], str):
                unexpected.append(f"{scene.name}: {got[1]}")
                continue
            eax, trace, watches = got
            n_eax, n_trace, n_watches = native[scene]
            differs = eax != n_eax or trace != n_trace or watches != n_watches
            if scene.rank:
                if eax != n_eax and trace == n_trace and watches == n_watches:
                    detected += 1
                else:
                    unexpected.append(f"{scene.name} mutant did not isolate the rank compare")
            elif differs:
                unexpected.append(f"{scene.name} mutant changed a non-rank case")
    print(f"mutant_detected {detected}")
    print(f"mutant_unexpected {len(unexpected)}")
    for line in unexpected[:12]:
        print(line)
    return 1 if errors or unexpected or detected == 0 else 0


if __name__ == "__main__":
    sys.exit(main())
