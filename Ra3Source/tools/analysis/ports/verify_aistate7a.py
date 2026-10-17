"""Differential oracle for the 0x007A0000 AI-state batch.

The Release checker prints kind/log/mem tokens. Unicorn runs the shipped bytes
with the same inputs and the same stub side effects.
"""

from __future__ import annotations

import struct
import subprocess
import sys
from pathlib import Path

from unicorn import x86_const

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from emulate import Emulator  # noqa: E402

EXE = Path(__file__).resolve().parents[3] / "build" / "win32" / "bin" / "Release" / "ra3_aistate7acheck.exe"
GAME_LOGIC = 0x00DEBA64

SIGS = {
    0x007985D0: (0, False), 0x0079E250: (0, False), 0x00683140: (1, False),
    0x0079FF10: (1, False), 0x007527C0: (1, False), 0x0079E530: (1, False),
    0x005AC4F0: (2, False), 0x006A4F80: (2, False), 0x006A4FB0: (0, False),
    0x006AEFE0: (3, True), 0x00416820: (1, True), 0x007A57A0: (3, False),
    0x005B9F30: (0, False), 0x006A7620: (2, False), 0x006F04A0: (2, False),
    0x006D2020: (2, False), 0x006A7600: (0, False), 0x006DF450: (0, False),
    0x00687810: (0, False), 0x009CCDA0: (1, False), 0x00686E50: (0, False),
    0x00686D70: (2, False), 0x007525F0: (2, False), 0x0079E460: (0, False),
    0x006F19F0: (0, False), 0x0079EE50: (1, False), 0x006996C0: (0, False),
    0x00687C30: (0, False), 0x006AD4B0: (1, False), 0x005AD150: (1, False),
    0x00798D10: (0, False), 0x007578F0: (3, False), 0x007A1FC0: (1, False),
    0x007A44D0: (1, False), 0x0054A090: (2, True), 0x00706E00: (1, False),
    0x00686CB0: (0, False), 0x006EFEB0: (0, False), 0x0079F330: (1, False),
    0x006876D0: (1, False),
}
STACK0 = {0x0079E530, 0x005AC4F0, 0x006AD4B0, 0x005AD150}

FN = {
    "rot": 0x007A04D0, "wait": 0x007A5590, "upd": 0x007AAE40, "uex": 0x007AAE60,
    "arc": 0x007A65B0, "beach": 0x007AAFF0, "drift": 0x007AA4D0, "hent": 0x007ABBF0,
    "hexit": 0x007ABC30, "hold": 0x007A85E0, "make": 0x007A58A0, "board": 0x007A03F0,
    "goalx": 0x007A00A0, "para": 0x007A0120, "boardx": 0x007A0440, "facex": 0x007A0CC0,
    "melee": 0x007A32F0, "horde": 0x007A5DE0, "reload": 0x007A9700, "refine": 0x007AA080,
    "path": 0x007AAE90, "exitp": 0x007AB570, "garx": 0x007ABA00, "wayx": 0x007AC260,
    "goalox": 0x007AC810, "delx": 0x007AD210, "harvest": 0x007AEBB0, "jetx": 0x007A92B0,
    "atkpos": 0x007AAB50, "docku": 0x007A0C50, "docke": 0x007A0E30, "guard": 0x007A1090,
    "start": 0x007A5640, "delen": 0x007AD1C0, "taxi": 0x007A9A30, "gidl": 0x007ABA50,
    "xsa": 0x007A2340, "xtight": 0x007A2520, "xcrate": 0x007A3DD0, "xevac": 0x007A41C0,
    "xdel": 0x007A4310, "xatk": 0x007A46D0, "xwander": 0x007A4810, "xenter": 0x007A5A50,
    "xboth": 0x007A5CB0, "xposent": 0x007A6480, "xapp": 0x007AE9D0,
}


def u32(value: int) -> int:
    return value & 0xFFFFFFFF


def p32(value: int) -> bytes:
    return struct.pack("<I", u32(value))


class Checker:
    def __init__(self, executable: Path) -> None:
        self.process = subprocess.Popen(
            [str(executable)], stdin=subprocess.PIPE, stdout=subprocess.PIPE, text=True, bufsize=1,
        )

    def call(self, command: str) -> dict[str, str]:
        assert self.process.stdin is not None and self.process.stdout is not None
        self.process.stdin.write(command + "\n")
        self.process.stdin.flush()
        line = self.process.stdout.readline().strip()
        if not line:
            raise RuntimeError(f"checker closed on: {command}")
        out = {}
        for token in line.split():
            key, value = token.split("=", 1)
            out[key] = value
        return out

    def close(self) -> None:
        if self.process.stdin is not None:
            self.process.stdin.write("quit\n")
            self.process.stdin.flush()
        self.process.wait(timeout=10)


class Oracle:
    def __init__(self) -> None:
        self.emu = Emulator()
        self.log: list[str] = []
        self.ret: dict[int, int] = {}
        self.vret: dict[int, int] = {}
        self.tags: list[tuple[int, int, str]] = []
        self.vt: dict[tuple[int, int], int] = {}
        self.g = None
        self._bind()

    def _bind(self) -> None:
        def make_v(slot: int, nargs: int, version: bool = False):
            def stub(emu: Emulator, slot: int = slot, nargs: int = nargs, version: bool = version) -> int:
                args = [u32(emu.stack_arg(i)) for i in range(nargs)]
                if version:
                    blob = emu.read(args[0], 2)
                    self.add(f"v18:{blob[0]:02x}{blob[1]:02x}")
                else:
                    text = f"v{slot:x}"
                    for arg in args:
                        text += ":" + self.fmt(arg, False)
                    self.add(text)
                return self.vret.get(slot, 0)
            return stub

        slots = [
            (0x00, 1), (0x08, 0), (0x0C, 1), (0x10, 0), (0x1C, 1), (0x20, 1),
            (0x34, 1), (0x40, 2), (0x4C, 0), (0x54, 0), (0x58, 1), (0x6C, 1),
            (0x70, 1), (0x13C, 0), (0x16C, 1), (0x26C, 1), (0x288, 0),
        ]
        for slot, nargs in slots:
            self.vt[(slot, nargs)] = self.emu.bind_stub(make_v(slot, nargs), 4 * nargs)
        self.vt[(0x18, 1)] = self.emu.bind_stub(make_v(0x18, 1, True), 4)
        self.vt[(0x38, 0)] = self.emu.bind_stub(make_v(0x38, 0), 0)
        self.vt[(0x38, 1)] = self.emu.bind_stub(make_v(0x38, 1), 4)
        self.vt[(0x84, 0)] = self.emu.bind_stub(make_v(0x84, 0), 0)
        self.vt[(0x84, 1)] = self.emu.bind_stub(make_v(0x84, 1), 4)

        def make_d(address: int, nargs: int):
            def stub(emu: Emulator, address: int = address, nargs: int = nargs) -> int:
                ecx = u32(emu.register(x86_const.UC_X86_REG_ECX))
                text = f"d{address:x}:{self.fmt(ecx, False)}"
                for index in range(nargs):
                    arg = u32(emu.stack_arg(index))
                    text += ":" + self.fmt(arg, index == 0 and address in STACK0)
                self.add(text)
                return self.ret.get(address, 0)
            return stub

        for address, (nargs, cdecl) in SIGS.items():
            self.emu.bind_function(address, make_d(address, nargs), 0 if cdecl else 4 * nargs)

    def add(self, text: str) -> None:
        self.log.append(text)

    def reset(self) -> None:
        self.emu.reset_data()
        self.log = []
        self.tags = []
        self.ret = {
            0x007985D0: 0x55, 0x0079E460: 0x77, 0x009CCDA0: 0x45, 0x006D2020: 0x21,
            0x0079FF10: 0x31, 0x0079EE50: 0x32, 0x006A4FB0: 0x46, 0x00683140: 0x61,
            0x006876D0: 0x47, 0x006996C0: 0x48, 0x0079F330: 0x49, 0x007A1FC0: 0x11,
            0x007A44D0: 0x12, 0x0054A090: 0x18, 0x007578F0: 1,
        }
        self.vret = {
            0x00: 0x41, 0x10: 0x21, 0x34: 0x35, 0x40: 0x62, 0x1C: 0x33, 0x16C: 0x34,
            0x84: 0x13, 0x70: 0x14, 0x58: 0x15, 0x6C: 0x16, 0x20: 0x17, 0x288: 0x19,
        }
        self.emu.uc.reg_write(x86_const.UC_X86_REG_FPCW, 0x037F)

    def alloc(self, size: int) -> int:
        return self.emu.alloc(b"\x00" * size)

    def put(self, address: int, value: int) -> None:
        self.emu.write(address, p32(value))

    def get(self, address: int) -> int:
        return struct.unpack("<I", self.emu.read(address, 4))[0]

    def fmt(self, value: int, stack: bool) -> str:
        value = u32(value)
        if stack:
            return "s"
        for base, size, name in self.tags:
            if base and base <= value < base + size:
                off = value - base
                return name if off == 0 else f"{name}+{off:x}"
        return f"{value:x}"

    def kind(self, value: int) -> str:
        value = u32(value)
        for base, _size, name in self.tags:
            if base and value == base:
                return name
        return f"{value:x}"

    def make(self):
        def vt() -> int:
            return self.alloc(0x300)

        def slot(table: int, off: int, nargs: int) -> None:
            self.put(table + off, self.vt[(off, nargs)])

        sv, mv, lv, modv = vt(), vt(), vt(), vt()
        pv, wv, xv, bv = vt(), vt(), vt(), vt()
        iv, hv, pathv, subv = vt(), vt(), vt(), vt()
        slot(sv, 0x1C, 1)
        slot(sv, 0x38, 1)
        slot(sv, 0x4C, 0)
        slot(mv, 0x1C, 1)
        slot(mv, 0x38, 1)
        slot(lv, 0x26C, 1)
        slot(lv, 0x288, 0)
        slot(modv, 0x0C, 1)
        slot(modv, 0x34, 1)
        slot(modv, 0x38, 0)
        slot(modv, 0x54, 0)
        slot(pv, 0x08, 0)
        slot(pv, 0x40, 2)
        slot(pv, 0x84, 0)
        slot(wv, 0x13C, 0)
        slot(wv, 0x16C, 1)
        slot(xv, 0x18, 1)
        slot(xv, 0x20, 1)
        slot(xv, 0x58, 1)
        slot(xv, 0x6C, 1)
        slot(xv, 0x70, 1)
        slot(xv, 0x84, 1)
        slot(bv, 0x40, 2)
        slot(iv, 0x70, 1)
        slot(hv, 0x00, 1)
        slot(pathv, 0x10, 0)
        slot(subv, 0x00, 1)
        slot(pv, 0x00, 1)

        def obj(size: int, table: int) -> int:
            pointer = self.alloc(size)
            if table:
                self.put(pointer, table)
            return pointer

        g = type("G", (), {})()
        g.self = obj(0x90, sv)
        g.mach = obj(0x80, mv)
        g.own = obj(0x480, 0)
        g.tmpl = obj(0x100, 0)
        g.goal = obj(0x480, 0)
        g.info = obj(0x100, 0)
        g.loco = obj(0x440, lv)
        g.mod = obj(0x80, modv)
        g.prod = obj(0x80, sv)
        g.wpn = obj(0x40, wv)
        g.xfer = obj(0x20, xv)
        g.game = obj(0x60, 0)
        g.box = obj(0x20, bv)
        g.phys = obj(0x40, pv)
        g.inner = obj(0x80, iv)
        g.host = obj(0x40, hv)
        g.path = obj(0x20, pathv)
        g.sub = obj(0x20, subv)
        g.block = obj(0x80, 0)
        self.put(g.self + 0x18, g.mach)
        self.put(g.mach + 0x24, g.own)
        self.put(g.own + 4, g.tmpl)
        self.put(g.own + 0x374, g.loco)
        self.put(g.goal + 4, g.info)
        self.put(g.prod + 0x68, 0x68)
        self.put(g.mod + 0x40, 0xFFFFFFFF)
        self.emu.write(g.loco + 0x434, b"\xAB")
        self.put(g.loco + 0x1E8, 0x111)
        self.put(g.loco + 0x1EC, 0x222)
        self.tags = [
            (g.self, 0x90, "self"), (g.mach, 0x80, "mach"), (g.own, 0x480, "own"),
            (g.tmpl, 0x100, "tmpl"), (g.goal, 0x480, "goal"), (g.info, 0x100, "info"),
            (g.loco, 0x440, "loco"), (g.mod, 0x80, "mod"), (g.prod, 0x80, "prod"),
            (g.wpn, 0x40, "wpn"), (g.xfer, 0x20, "xfer"), (g.game, 0x60, "game"),
            (g.box, 0x20, "box"), (g.phys, 0x40, "phys"), (g.inner, 0x80, "inner"),
            (g.host, 0x40, "host"), (g.path, 0x20, "path"), (g.sub, 0x20, "sub"),
            (g.block, 0x80, "alloc"),
        ]
        self.put(GAME_LOGIC, g.game)
        self.g = g
        return g

    def invoke(self, address: int, ecx: int, stack: tuple[int, ...] = ()) -> int:
        return u32(self.emu.call(address, stack, ecx=ecx).eax)

    def tokens(self, eax: int, mem: list[int], extra: dict[str, str] | None = None) -> dict[str, str]:
        g = self.g
        if mem:
            rendered = ",".join(f"{off:x}:{self.get(g.self + off):08x}" for off in mem)
        else:
            rendered = "."
        out = {
            "eax": "00000000",
            "kind": self.kind(eax),
            "log": ",".join(self.log) if self.log else ".",
            "mem": rendered,
        }
        if extra:
            out.update(extra)
        return out

    def ow(self, offset: int) -> str:
        return f"{self.get(self.g.own + offset):08x}"

    def scenario(self, name: str, args: list[int]) -> dict[str, str]:
        g = self.make()
        a = args
        mem: list[int] = []
        extra: dict[str, str] = {}
        ecx = g.self
        stack: tuple[int, ...] = ()

        def arg(index: int) -> int:
            return a[index] if index < len(a) else 0

        if name == "rot":
            self.put(g.mach + 0x54, arg(0))
        elif name == "wait":
            self.put(g.tmpl + 0xCC, arg(0))
        elif name == "upd":
            self.put(g.self + 0x20, g.path)
        elif name == "uex":
            if arg(1) == 0:
                self.put(g.own + 0x374, 0)
            stack = (arg(0),)
        elif name == "arc":
            self.put(g.mach + 0x2C, arg(1))
            if arg(0) == 0:
                self.put(g.own + 0x374, 0)
        elif name == "beach":
            self.ret[0x0079E530] = arg(0)
        elif name == "drift":
            self.put(g.own + 0xA0, 0x00020001 if arg(0) else 1)
            extra["ow"] = ""
        elif name == "hent":
            self.put(g.own + 0xA0, 0x00200001 if arg(0) else 1)
            extra["ow"] = ""
        elif name == "hexit":
            self.put(g.own + 0xA0, 0x00200001 if arg(0) else 1)
            stack = (0,)
            extra["ow"] = ""
        elif name == "hold":
            self.vret[0x4C] = g.prod
            extra["p20"] = ""
        elif name == "make":
            self.ret[0x00416820] = 0 if arg(0) else g.block
            self.ret[0x007A57A0] = g.block
            ecx = g.mach
            stack = (arg(1),)
        elif name == "board":
            self.vret[0x4C] = g.prod
            self.ret[0x005B9F30] = g.goal
            extra["p20"] = ""
        elif name == "goalx":
            self.ret[0x005B9F30] = g.goal if arg(0) else 0
            self.ret[0x006A7620] = g.box if arg(1) else 0
            if arg(2) == 0:
                self.put(g.own + 0x374, 0)
            if arg(3):
                self.put(g.self + 0x20, g.sub)
            stack = (0,)
            mem = [0x20]
        elif name == "para":
            if arg(0):
                self.put(g.self + 0x20, g.sub)
            stack = (0,)
            mem = [0x20]
        elif name == "boardx":
            if arg(0) == 0:
                self.put(g.own + 0x374, 0)
            self.ret[0x006A7600] = g.phys
            if arg(1):
                self.put(g.self + 0x20, g.sub)
            stack = (0,)
            mem = [0x20]
        elif name == "facex":
            if arg(0) == 0:
                self.ret[0x005B9F30] = 0
            else:
                self.ret[0x005B9F30] = g.goal
                if arg(0) == 1:
                    self.emu.write(g.goal + 0x80, b"\x01")
                elif arg(0) == 2:
                    self.emu.write(g.goal + 0x471, b"\x01")
            self.ret[0x00687810] = g.mod
            self.vret[0x38] = arg(2)
            stack = (arg(1),)
        elif name == "melee":
            self.emu.write(g.own + 0x80, bytes([arg(0) & 0xFF]))
            self.ret[0x006A7600] = g.phys if arg(1) else 0
            self.vret[0x84] = g.wpn if arg(2) else 0
            stack = (0,)
        elif name == "horde":
            self.ret[0x005B9F30] = g.goal if arg(1) else 0
            if arg(0):
                self.put(g.info + 0xC0, 0x10)
            self.ret[0x006A7600] = g.phys if arg(2) else 0
            stack = (arg(3),)
        elif name == "reload":
            self.put(g.own + 0xB4, 0x21 if arg(0) else 1)
            stack = (0,)
            extra["ow"] = ""
        elif name == "refine":
            self.put(g.own + 0xB4, 0x00800001 if arg(0) else 1)
            stack = (0,)
            extra["ow"] = ""
        elif name == "path":
            self.emu.write(g.mach + 0x44, bytes([arg(0) & 0xFF]))
            if arg(1) == 0:
                self.put(g.own + 0x374, 0)
            self.ret[0x00686E50] = 0x3
        elif name == "exitp":
            self.put(g.self + 0x24, arg(0))
            if arg(1) == 0:
                self.put(g.own + 0x374, 0)
            stack = (arg(2),)
        elif name == "garx":
            self.put(g.own + 0xA0, 0x00200001 if arg(0) else 1)
            if arg(1):
                self.put(g.self + 0x20, g.sub)
            stack = (0,)
            mem = [0x20]
            extra["ow"] = ""
        elif name == "wayx":
            self.put(g.own + 0xB0, 0x00400001 if arg(0) else 1)
            if arg(1) == 0:
                self.put(g.own + 0x374, 0)
            if arg(2):
                self.put(g.self + 0x20, g.sub)
            stack = (0,)
            mem = [0x20]
            extra["ow"] = ""
            extra["lb"] = ""
            extra["l1e8"] = ""
            extra["l1ec"] = ""
        elif name == "goalox":
            if arg(0) == 0:
                self.put(g.own + 0x374, 0)
            stack = (arg(1),)
        elif name == "delx":
            self.put(g.self + 0x28, arg(1))
            if arg(0):
                index = arg(1)
                self.put(g.own + 0xA0 + (index >> 5) * 4, 1 << (index & 31))
            stack = (arg(2),)
            extra["c0"] = ""
            extra["c1"] = ""
        elif name == "harvest":
            self.put(g.own + 0xB4, 0x40000001 if arg(0) else 1)
            stack = (0,)
            extra["ow"] = ""
        elif name == "jetx":
            self.put(g.own + 0xB0, 0x02000001 if arg(0) else 1)
            if arg(1):
                self.put(g.self + 0x20, g.sub)
            stack = (0,)
            mem = [0x20]
            extra["ow"] = ""
        elif name == "atkpos":
            bits = 0x100
            if arg(0):
                bits |= 1
            if arg(1):
                bits |= 4
            self.put(g.own + 0xA8, bits)
            if arg(2):
                self.put(g.self + 0x20, g.sub)
            stack = (0,)
            mem = [0x20]
            extra["a8"] = ""
        elif name == "docku":
            if arg(0) == 0:
                self.ret[0x005B9F30] = 0
            else:
                self.ret[0x005B9F30] = g.goal
                if arg(0) == 1:
                    self.emu.write(g.goal + 0x80, b"\x01")
                elif arg(0) == 2:
                    self.emu.write(g.goal + 0x471, b"\x01")
            self.ret[0x00687810] = g.mod if arg(1) else 0
            self.vret[0x38] = arg(2)
            self.vret[0x0C] = arg(3)
        elif name == "docke":
            if arg(0) == 0:
                self.ret[0x005B9F30] = 0
            else:
                self.ret[0x005B9F30] = g.goal
                if arg(0) == 1:
                    self.emu.write(g.goal + 0x80, b"\x01")
            self.ret[0x00687810] = g.mod if arg(1) else 0
            self.put(g.game + 0x58, arg(2))
            self.vret[0x54] = arg(3)
            mem = [0x28]
        elif name == "guard":
            if arg(0) == 0:
                self.put(g.mach + 0x24, 0)
            self.ret[0x00687C30] = arg(1)
            self.ret[0x006AD4B0] = arg(2)
        elif name == "start":
            self.ret[0x005B9F30] = g.goal if arg(0) else 0
            self.emu.write(g.self + 0x20, bytes([arg(1) & 0xFF]))
            self.ret[0x007578F0] = arg(2)
        elif name == "delen":
            self.put(g.self + 0x20, arg(2))
            self.put(g.self + 0x28, arg(0))
            if arg(1):
                index = arg(0)
                self.put(g.own + 0xA0 + (index >> 5) * 4, 1 << (index & 31))
            mem = [0x24]
            extra["c0"] = ""
            extra["c1"] = ""
        elif name == "taxi":
            self.ret[0x00686CB0] = g.mod if arg(0) else 0
            if arg(1) == 0:
                self.put(g.own + 0x374, 0)
            self.put(g.own + 0xB4, 0x21 if arg(2) else 1)
            if arg(3):
                self.put(g.self + 0x20, g.sub)
            stack = (0,)
            mem = [0x20]
            extra["ow"] = ""
            extra["m40"] = ""
            extra["lb"] = ""
        elif name == "gidl":
            self.ret[0x006EFEB0] = g.host if arg(0) else 0
            self.ret[0x006A7600] = g.phys if arg(1) else 0
            self.vret[0x08] = g.inner
        elif name == "xsa":
            self.put(g.xfer + 4, arg(0))
            stack = (g.xfer,)
        elif name == "xatk":
            self.put(g.xfer + 4, arg(0))
            self.put(g.self + 0x6C, arg(1))
            stack = (g.xfer,)
        elif name in ("xtight", "xcrate", "xevac", "xdel", "xwander", "xenter", "xboth", "xposent", "xapp"):
            stack = (g.xfer,)
        else:
            raise KeyError(name)

        eax = self.invoke(FN[name], ecx, stack)
        if "ow" in extra:
            offset = 0xB4 if name in ("reload", "refine", "harvest", "taxi") else 0xA0
            if name in ("wayx", "jetx"):
                offset = 0xB0
            extra["ow"] = self.ow(offset)
        if "p20" in extra:
            extra["p20"] = "1" if self.get(g.self + 0x20) == g.prod else "0"
        if "lb" in extra:
            extra["lb"] = f"{self.emu.read(g.loco + 0x434, 1)[0]:x}"
        if "l1e8" in extra:
            extra["l1e8"] = f"{self.get(g.loco + 0x1E8):08x}"
        if "l1ec" in extra:
            extra["l1ec"] = f"{self.get(g.loco + 0x1EC):08x}"
        if "c0" in extra:
            extra["c0"] = f"{self.get(g.own + 0xA0):08x}"
        if "c1" in extra:
            extra["c1"] = f"{self.get(g.own + 0xA4):08x}"
        if "a8" in extra:
            extra["a8"] = f"{self.get(g.own + 0xA8):08x}"
        if "m40" in extra:
            extra["m40"] = f"{self.get(g.mod + 0x40):08x}"
        return self.tokens(eax, mem, extra)


def cases() -> list[str]:
    out = []
    for bits in (0x7F7FFFFF, 0x3F800000, 0, 0x7F800000, 0x7FC00000, 0xFF7FFFFF):
        out.append(f"rot 0x{bits:x}")
    for bits in (0, 0x400, 0x200, 0xFFFFFFFF):
        out.append(f"wait 0x{bits:x}")
    out.append("upd")
    for status, loco in ((0, 0), (0, 1), (7, 1)):
        out.append(f"uex {status} {loco}")
    for loco, angle in ((0, 0), (1, 0), (1, 0x3F800000)):
        out.append(f"arc {loco} 0x{angle:x}")
    out += ["beach 0", "beach 1", "drift 0", "drift 1", "hent 0", "hent 1", "hexit 0", "hexit 1", "hold"]
    out += ["make 1 0x99", "make 0 0x99", "board"]
    for goal, box, loco, sub in ((0, 0, 0, 0), (1, 0, 0, 0), (1, 1, 0, 0), (1, 1, 1, 0), (1, 1, 1, 1), (0, 0, 1, 1)):
        out.append(f"goalx {goal} {box} {loco} {sub}")
    out += ["para 0", "para 1"]
    for loco, sub in ((0, 0), (1, 0), (1, 1)):
        out.append(f"boardx {loco} {sub}")
    for goal, status, busy in ((0, 0, 0), (1, 0, 0), (2, 1, 0), (3, 0, 0), (3, 0, 1), (3, 1, 1), (3, 0, 0x101)):
        out.append(f"facex {goal} {status} 0x{busy:x}")
    for flag, phys, weapon in ((1, 0, 0), (0, 0, 0), (0, 1, 0), (0, 1, 1)):
        out.append(f"melee {flag} {phys} {weapon}")
    for bit, goal, phys, status in ((0, 0, 0, 4), (1, 1, 0, 4), (0, 1, 1, 9)):
        out.append(f"horde {bit} {goal} {phys} {status}")
    out += ["reload 0", "reload 1", "refine 0", "refine 1"]
    for alt, loco in ((0, 0), (0, 1), (1, 1)):
        out.append(f"path {alt} {loco}")
    for mode, loco, status in ((0xFFFFFFFF, 0, 3), (5, 1, 3), (0xFFFFFFFF, 1, 0)):
        out.append(f"exitp 0x{mode:x} {loco} {status}")
    for bit, sub in ((0, 0), (1, 0), (1, 1), (0, 1)):
        out.append(f"garx {bit} {sub}")
    for bit, loco, sub in ((0, 0, 0), (1, 1, 0), (0, 1, 1), (1, 1, 1)):
        out.append(f"wayx {bit} {loco} {sub}")
    out += ["goalox 0 4", "goalox 1 4"]
    for present, index, status in ((0, 0, 2), (1, 0, 2), (1, 0x20, 2), (0, 0x15, 8)):
        out.append(f"delx {present} 0x{index:x} {status}")
    out += ["harvest 0", "harvest 1"]
    for bit, sub in ((0, 0), (1, 0), (1, 1)):
        out.append(f"jetx {bit} {sub}")
    for b0, b2, sub in ((0, 0, 0), (1, 0, 0), (0, 1, 0), (1, 1, 1)):
        out.append(f"atkpos {b0} {b2} {sub}")
    for goal, mod, busy, low in ((0, 0, 0, 0), (1, 0, 0, 0), (3, 0, 0, 0), (3, 1, 0, 0), (3, 1, 1, 0), (3, 1, 1, 1)):
        out.append(f"docku {goal} {mod} {busy} {low}")
    for goal, mod, base, extra in ((0, 0, 0x10, 3), (1, 0, 0x10, 3), (3, 0, 0x10, 3), (3, 1, 0x10, 3)):
        out.append(f"docke {goal} {mod} 0x{base:x} {extra}")
    for owner, found, ok in ((0, 1, 1), (1, 0, 1), (1, 1, 0), (1, 1, 1)):
        out.append(f"guard {owner} {found} {ok}")
    for goal, armed, helper in ((0, 0, 1), (1, 0, 0), (1, 1, 0), (1, 0, 5), (1, 1, 5)):
        out.append(f"start {goal} {armed} {helper}")
    for index, already, copied in ((0, 0, 0x42), (0, 1, 0x42), (0x20, 0, 0x99), (0x15, 1, 7)):
        out.append(f"delen 0x{index:x} {already} 0x{copied:x}")
    for mod, loco, bit, sub in ((0, 0, 0, 0), (1, 1, 0, 0), (1, 1, 1, 0), (1, 0, 1, 1), (0, 1, 0, 1)):
        out.append(f"taxi {mod} {loco} {bit} {sub}")
    for host, phys in ((0, 0), (1, 0), (1, 1)):
        out.append(f"gidl {host} {phys}")
    out += ["xsa 0", "xsa 8", "xtight", "xcrate", "xevac", "xdel", "xatk 0 0x22", "xatk 8 0x22"]
    out += ["xwander", "xenter", "xboth", "xposent", "xapp"]
    return out


def main() -> int:
    checker = Checker(EXE)
    oracle = Oracle()
    failures = []
    try:
        for command in cases():
            got = checker.call(command)
            oracle.reset()
            name, *raw = command.split()
            args = [int(token, 0) for token in raw]
            try:
                exp = oracle.scenario(name, args)
            except Exception as error:  # noqa: BLE001 - surface the first emulation fault
                failures.append(f"{command} emu {error}")
                continue
            for key in ("kind", "log", "mem", "eax"):
                if got.get(key) != exp.get(key):
                    failures.append(f"{command} {key}\n  cpp {got.get(key)}\n  emu {exp.get(key)}")
                    break
            else:
                for key, value in exp.items():
                    if key in ("kind", "log", "mem", "eax"):
                        continue
                    if got.get(key) != value:
                        failures.append(f"{command} {key}\n  cpp {got.get(key)}\n  emu {value}")
                        break
    finally:
        checker.close()
    print(f"cases={len(cases())} mismatches={len(failures)}")
    for line in failures[:40]:
        print(line)
    return 1 if failures else 0


if __name__ == "__main__":
    sys.exit(main())
