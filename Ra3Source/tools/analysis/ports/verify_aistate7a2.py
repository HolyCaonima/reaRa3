"""Differential oracle for the second 0x007A0000 AI-state batch.

The Release checker prints kind/log/mem tokens. Unicorn runs the shipped bytes
with the same object graph, the same stub returns, and the same stub side effects.
"""

from __future__ import annotations

import struct
import subprocess
import sys
from pathlib import Path

from unicorn import x86_const

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from emulate import Emulator  # noqa: E402

EXE = Path(__file__).resolve().parents[3] / "build" / "win32" / "bin" / "Release" / "ra3_aistate7a2check.exe"
STACK_LO = 0x7FFFE000
STACK_HI = 0x7FFFFFF0

# address -> (nargs, cdecl). cdecl stubs pop nothing; thiscall stubs pop 4*nargs.
SIGS = {
    0x006A4F80: (2, False), 0x006A4FB0: (0, False), 0x006D2020: (2, False),
    0x005AC4F0: (2, False), 0x006A51A0: (1, False), 0x00685EE0: (0, True),
    0x00685EC0: (0, True), 0x006FFB00: (1, False), 0x007A0B30: (3, True),
    0x00688810: (0, True), 0x006A6210: (0, True), 0x006A4F60: (0, True),
    0x00687CB0: (0, True), 0x00A780D0: (0, True), 0x006AF030: (0, True), 0x006906A0: (4, False),
    0x006A6F60: (0, True), 0x00687C30: (0, True), 0x006AD4B0: (1, False),
    0x005AD150: (1, False), 0x005B9F30: (0, True), 0x0067F130: (1, True),
    0x006D8A20: (1, False), 0x006A7600: (0, False), 0x006A7620: (2, False),
    0x0077B1F0: (1, False), 0x0063EB60: (3, True), 0x00658A80: (6, True),
    0x007527C0: (1, False), 0x005BDCC0: (1, False), 0x0079ECE0: (1, False),
    0x006CFCA0: (0, True), 0x006E4760: (2, True), 0x00687560: (0, True),
    0x0079CEE0: (0, True), 0x00686D90: (3, False), 0x006D21B0: (0, True),
    0x005AA240: (1, False), 0x005AA250: (0, True), 0x004D9AB0: (1, False),
    0x006B7040: (2, False), 0x00682D70: (0, True), 0x006A4500: (1, False),
    0x006870A0: (0, True), 0x005AA280: (1, False), 0x00549D50: (2, True),
    0x008663B0: (1, False), 0x0054A090: (2, True), 0x00549C70: (2, True),
    0x00416820: (1, True), 0x007A1BD0: (1, False), 0x005A9F60: (2, False),
    0x004CB690: (2, False), 0x004CA930: (0, True), 0x007A1FC0: (1, False),
    0x006864D0: (0, True), 0x00757B40: (2, False), 0x00686460: (0, False),
    0x0079CC50: (0, True), 0x007985D0: (0, True), 0x006A76A0: (1, False),
    0x0079EAD0: (0, True), 0x006E6360: (1, False), 0x007EBE30: (0, True),
    0x007E6AF0: (0, True), 0x007A1570: (1, False), 0x007A1400: (3, False),
}

FN = {
    "pfw": 0x007A5400, "pfu": 0x007A5510, "gid": 0x007A0F30, "hws": 0x007AEBF0,
    "hen": 0x007A5D00, "gon": 0x007A6020, "wff": 0x007A55B0, "pfb": 0x007A6530,
    "fex": 0x007AB4F0, "rld": 0x007A9690, "mow": 0x007AC1F0, "jla": 0x007A93F0,
    "off": 0x007AF6F0, "cid": 0x007AABD0, "egl": 0x007AB170, "pke": 0x007A9580,
    "pkx": 0x007A9610, "lnd": 0x007A94E0, "tku": 0x007A98B0, "tng": 0x007A6140,
    "mpx": 0x007ABEC0, "par": 0x007ACF90, "amg": 0x007AAA40, "aps": 0x007AAAC0,
    "jag": 0x007A9220, "xim": 0x007A1FC0, "xfw": 0x007A44D0, "xap": 0x007A2A20,
    "xpa": 0x007A2BB0, "xpu": 0x007A34D0, "xga": 0x007A1CB0, "xmv": 0x007A4200,
    "xfd": 0x007A3620, "xex": 0x007A4650, "xtn": 0x007A68F0, "xbk": 0x007A28D0,
    "xfp": 0x007A3E20, "xmh": 0x007A2CF0, "xsq": 0x007A3AD0, "xch": 0x007A6390,
    "xft": 0x007A3F90, "xme": 0x007A3C00, "xai": 0x007AD4C0,
}
ONE_ARG = {"pkx", "mpx", "off", "xim", "xfw", "xap", "xpa", "xpu", "xga", "xmv", "xfd", "xex", "xtn", "xbk", "xfp", "xmh", "xsq", "xch", "xft", "xme", "xai"}


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
        try:
            self.emu.uc.mem_map(0, 0x1000)
        except Exception:
            pass
        self.log: list[str] = []
        self.ret: dict[int, int] = {}
        self.vret: dict[int, int] = {}
        self.tags: list[tuple[int, int, str]] = []
        self.vt: dict[tuple[int, str], int] = {}
        self.g: dict[str, int] = {}
        self.fill = 0
        self.ver = -1
        self.poke = 0
        self.other84 = 0
        self.no_weapon = False
        self.live = False
        self._bind()

    def _bind(self) -> None:
        def make_v(slot: int, nargs: int, kind: str):
            def stub(emu: Emulator, slot: int = slot, nargs: int = nargs, kind: str = kind) -> int:
                ecx = u32(emu.register(x86_const.UC_X86_REG_ECX))
                args = [u32(emu.stack_arg(i)) for i in range(nargs)]
                if kind == "ver":
                    blob = emu.read(args[0], 2)
                    self.add(f"v18:{blob[0]:02x}{blob[1]:02x}")
                    if self.ver >= 0:
                        emu.write(args[0] + 1, bytes([self.ver & 0xFF]))
                else:
                    text = f"v{slot:x}"
                    for arg in args:
                        text += ":" + self.fmt(arg)
                    self.add(text)
                    if kind == "out" and len(args) >= 3 and args[2]:
                        emu.write(args[2], p32(0x11111111) + p32(0x22222222) + p32(0x33333333))
                    if kind == "poke" and self.poke and args and args[0]:
                        emu.write(args[0], bytes([self.poke & 0xFF]))
                return self.slot_ret(ecx, slot)
            return stub

        specs = [
            (0x00, 0, "n"), (0x00, 2, "n"), (0x04, 0, "n"), (0x04, 1, "n"),
            (0x08, 0, "n"), (0x0C, 0, "n"), (0x10, 0, "n"), (0x14, 2, "n"),
            (0x14, 3, "out"), (0x18, 1, "ver"), (0x1C, 1, "n"), (0x20, 1, "n"),
            (0x38, 1, "n"), (0x3C, 0, "n"), (0x3C, 1, "n"), (0x40, 1, "n"),
            (0x40, 2, "n"), (0x44, 1, "n"), (0x48, 0, "n"), (0x4C, 0, "n"),
            (0x58, 1, "n"), (0x60, 1, "n"), (0x64, 1, "n"), (0x6C, 1, "n"),
            (0x70, 1, "n"), (0x7C, 3, "n"), (0x84, 0, "n"), (0x84, 1, "poke"),
            (0x94, 1, "n"), (0x94, 3, "n"), (0x98, 1, "n"), (0x158, 2, "n"),
            (0x1C4, 0, "n"), (0x244, 0, "n"), (0x288, 1, "cdecl"),
        ]
        for slot, nargs, kind in specs:
            cleanup = 0 if kind == "cdecl" else 4 * nargs
            self.vt[(slot, f"{kind}{nargs}")] = self.emu.bind_stub(make_v(slot, nargs, kind), cleanup)

        def make_d(address: int, nargs: int):
            def stub(emu: Emulator, address: int = address, nargs: int = nargs) -> int:
                ecx = u32(emu.register(x86_const.UC_X86_REG_ECX))
                args = [u32(emu.stack_arg(i)) for i in range(nargs)]
                text = f"d{address:x}:{self.fmt(ecx)}"
                for arg in args:
                    text += ":" + self.fmt(arg)
                self.add(text)
                if address == 0x006E4760 and len(args) >= 2 and args[1]:
                    emu.write(args[1], p32(self.fill))
                return self.ret.get(address, 0)
            return stub

        for address, (nargs, cdecl) in SIGS.items():
            self.emu.bind_function(address, make_d(address, nargs), 0 if cdecl else 4 * nargs)

    def add(self, text: str) -> None:
        self.log.append(text)

    def slot_ret(self, ecx: int, slot: int) -> int:
        if slot == 0x84 and ecx == self.g.get("phys"):
            return 0 if self.no_weapon else self.g["weapon"]
        if slot == 0x84 and ecx == self.g.get("other"):
            return self.other84
        return self.vret.get(slot, 0)

    def reset(self) -> None:
        self.emu.reset_data()
        self.emu.uc.reg_write(x86_const.UC_X86_REG_FPCW, 0x037F)
        self.log = []
        self.tags = []
        self.ver = -1
        self.poke = 0
        self.other84 = 0
        self.no_weapon = False
        self.live = False

    def alloc(self, size: int, alignment: int = 16) -> int:
        return self.emu.alloc(b"\x00" * size, alignment)

    def put(self, address: int, value: int) -> None:
        self.emu.write(address, p32(value))

    def get(self, address: int) -> int:
        return struct.unpack("<I", self.emu.read(address, 4))[0]

    def _stack(self, value: int) -> bool:
        if value in (0x7F7FFFFF, 0x7FFFFFFF):
            return False
        return 0x7F000000 <= value < 0x80000000

    def fmt(self, value: int) -> str:
        value = u32(value)
        if self._stack(value):
            return "s"
        for base, size, name in self.tags:
            if base and base <= value < base + size:
                off = value - base
                return name if off == 0 else f"{name}+{off:x}"
        return f"{value:x}"

    def kind(self, value: int) -> str:
        value = u32(value)
        if self._stack(value):
            return "s"
        for base, _size, name in self.tags:
            if base and value == base:
                return name
        return f"{value:x}"

    def make(self) -> dict[str, int]:
        def vt() -> int:
            return self.alloc(0x300)

        def use(table: int, off: int, kind: str, nargs: int) -> None:
            self.put(table + off, self.vt[(off, f"{kind}{nargs}")])

        tables = {name: vt() for name in
                  ("self", "mach", "loco", "xfer", "sub", "spot", "phys", "weapon", "reg", "glob", "form", "site")}
        use(tables["self"], 0x3C, "n", 0)
        use(tables["self"], 0x4C, "n", 0)
        use(tables["mach"], 0x1C, "n", 1)
        use(tables["mach"], 0x38, "n", 1)
        use(tables["loco"], 0x1C4, "n", 0)
        use(tables["loco"], 0x244, "n", 0)
        use(tables["loco"], 0x288, "cdecl", 1)
        for off, nargs in ((0x04, 1), (0x08, 0), (0x14, 2), (0x20, 1), (0x40, 1), (0x44, 1),
                           (0x58, 1), (0x60, 1), (0x64, 1), (0x6C, 1), (0x70, 1)):
            use(tables["xfer"], off, "n", nargs)
        use(tables["xfer"], 0x18, "ver", 1)
        use(tables["xfer"], 0x84, "poke", 1)
        use(tables["sub"], 0x00, "n", 0)
        use(tables["sub"], 0x10, "n", 0)
        use(tables["sub"], 0x1C, "n", 1)
        use(tables["sub"], 0x38, "n", 1)
        use(tables["spot"], 0x14, "out", 3)
        use(tables["spot"], 0x3C, "n", 1)
        use(tables["phys"], 0x40, "n", 2)
        use(tables["phys"], 0x84, "n", 0)
        use(tables["phys"], 0x94, "n", 3)
        use(tables["phys"], 0x98, "n", 1)
        use(tables["weapon"], 0x7C, "n", 3)
        use(tables["reg"], 0x6C, "n", 1)
        use(tables["reg"], 0x158, "n", 2)
        use(tables["form"], 0x94, "n", 1)
        use(tables["site"], 0x04, "n", 0)
        use(tables["site"], 0x0C, "n", 0)
        use(tables["site"], 0x48, "n", 0)

        def obj(size: int, table: int | None, alignment: int = 16) -> int:
            memory = self.alloc(size, alignment)
            if table:
                self.put(memory, table)
            return memory

        g = {
            "self": obj(0x100, tables["self"]),
            "mach": obj(0x90, tables["mach"]),
            "own": obj(0x500, None),
            "tmpl": obj(0x140, None),
            "loco": obj(0x450, tables["loco"]),
            "goal": obj(0x200, None),
            "info": obj(0x140, None),
            "xfer": obj(0x40, tables["xfer"]),
            "game": obj(0x80, None),
            "sub": obj(0x90, tables["sub"]),
            "spot": obj(0x40, tables["spot"]),
            "pad": obj(0x40, None),
            "phys": obj(0xA0, tables["phys"]),
            "weapon": obj(0x40, tables["weapon"]),
            "other": obj(0xA0, tables["phys"]),
            "block": obj(0x90, None),
            "reg": obj(0x40, tables["reg"]),
            "glob": obj(0x10, None),
            "id": obj(0x10, None),
            "node": obj(0x20, None, 256),
            "team": obj(0x300, None),
            "player": obj(0xA0, None),
            "form": obj(0x20, tables["form"]),
            "ammo": obj(0x20, None),
            "module": obj(0xC0, None),
            "site": obj(0x50, tables["site"]),
            "rec": obj(0x40, None),
        }
        self.put(g["self"] + 0x18, g["mach"])
        self.put(g["mach"] + 0x24, g["own"])
        self.put(g["mach"] + 0x28, 0x28)
        self.put(g["own"] + 4, g["tmpl"])
        self.put(g["own"] + 0x374, g["loco"])
        self.put(g["loco"] + 4, g["tmpl"])
        self.put(g["loco"] + 0x214, g["module"])
        self.put(g["own"] + 0x33C, g["form"])
        self.put(g["own"] + 0x37C, g["ammo"])
        self.put(g["own"] + 0xF4, g["ammo"])
        self.put(g["goal"] + 4, g["info"])
        self.put(g["pad"] + 4, g["info"])
        self.put(g["sub"] + 0x68, 0x68)
        self.put(g["player"] + 0x88, g["player"] + 0x88)
        self.put(g["self"] + 8, g["self"] + 8)
        self.put(g["tmpl"] + 0x54, 0x3F800000)
        self.tags = [
            (g["self"], 0x100, "self"), (g["mach"], 0x90, "mach"), (g["own"], 0x500, "own"),
            (g["tmpl"], 0x140, "tmpl"), (g["loco"], 0x450, "loco"), (g["goal"], 0x200, "goal"),
            (g["info"], 0x140, "info"), (g["xfer"], 0x40, "xfer"), (g["game"], 0x80, "game"),
            (g["sub"], 0x90, "sub"), (g["spot"], 0x40, "spot"), (g["pad"], 0x40, "pad"),
            (g["phys"], 0xA0, "phys"), (g["weapon"], 0x40, "wpn"), (g["other"], 0xA0, "other"),
            (g["block"], 0x90, "block"), (g["reg"], 0x40, "reg"), (g["glob"], 0x10, "glob"),
            (g["id"], 0x10, "id"), (g["node"], 0x20, "node"), (g["team"], 0x300, "team"),
            (g["player"], 0xA0, "player"), (g["form"], 0x20, "form"), (g["ammo"], 0x20, "ammo"),
            (g["module"], 0xC0, "mod"), (g["site"], 0x50, "site"), (g["rec"], 0x40, "rec"),
        ]
        self.put(g["glob"], self.vt[(0x00, "n2")])
        self.put(0, g["glob"])
        self.put(0x00DEBA64, g["game"])
        self.put(0x00BE700C, 0x7F7FFFFF)
        self.put(0x00BD9628, 0xBF800000)
        self.put(0x00CC561C, 0x00CC785C)
        self.put(0x00DF03FC, 0)
        self.put(0x00DFC190, 0)
        self.g = g
        self.fill = g["pad"]
        self.ret = {
            0x006A51A0: g["goal"], 0x007A0B30: 1, 0x006D2020: 0x21, 0x006A4FB0: 0x46,
            0x005B9F30: g["goal"], 0x006A4F60: 1, 0x00416820: g["block"], 0x007A1BD0: g["block"],
            0x008663B0: g["id"], 0x0067F130: g["rec"], 0x006A7600: g["phys"], 0x006A7620: g["other"],
            0x0079CC50: 1, 0x007985D0: 0x77, 0x007527C0: 1, 0x007EBE30: 1, 0x007A1570: 1,
            0x007A1400: 1, 0x006CFCA0: 4, 0x006E4760: g["spot"], 0x00687560: 1, 0x0079EAD0: 3,
            0x006A76A0: 1, 0x007A1FC0: 0x11, 0x006A4500: 0x45,
        }
        self.vret = {
            0x4C: g["sub"], 0x10: 0x55, 0x14: 1, 0x1C: 0x41, 0x244: 1, 0x288: 0x19,
            0x1C4: g["site"], 0x48: 5, 0x0C: 9, 0x04: 3, 0x84: 0x84, 0x08: 0x08, 0x6C: 0x16,
        }
        return g

    def run(self, address: int, ecx: int, stack: tuple[int, ...] = ()) -> int:
        saved = None
        if self.live:
            saved = self.emu._function_stubs.pop(address, None)
        try:
            return u32(self.emu.call(address, list(stack), ecx=ecx).eax)
        finally:
            if saved is not None:
                self.emu._function_stubs[address] = saved

    def mem(self) -> str:
        g = self.g

        def one(name: str, base: int, off: int) -> str:
            return f"{name}{off:x}={self.fmt(self.get(base + off))}"

        parts = [
            one("s", g["self"], 0x20), one("s", g["self"], 0x24), one("s", g["self"], 0x28),
            one("s", g["self"], 0x2C), one("s", g["self"], 0x30), one("s", g["self"], 0x34),
            one("s", g["self"], 0x38), one("s", g["self"], 0x50), one("s", g["self"], 0x54),
            one("s", g["self"], 0x60), one("o", g["own"], 0xA0), one("o", g["own"], 0xA8),
            one("o", g["own"], 0xB0), one("o", g["own"], 0xB4), one("o", g["own"], 0xD4),
            one("l", g["loco"], 0x1F0), one("l", g["loco"], 0x17C), one("l", g["loco"], 0x21C),
            f"b24={self.emu.read(g['self'] + 0x24, 1)[0]:x}",
            f"b3c={self.emu.read(g['self'] + 0x3C, 1)[0]:x}",
            f"u7c={self.emu.read(g['sub'] + 0x7C, 1)[0]:x}",
            f"l434={self.emu.read(g['loco'] + 0x434, 1)[0]:x}",
            f"ma4={self.emu.read(g['module'] + 0xA4, 1)[0]:x}",
            f"m40={self.fmt(self.get(g['module'] + 0x40))}",
        ]
        return ",".join(parts)

    def scenario(self, name: str, args: list[int]) -> dict[str, str]:
        g = self.make()

        def a(index: int) -> int:
            return args[index] if index < len(args) else 0

        ecx = g["self"]
        stack: tuple[int, ...] = ()
        if name == "pfw":
            if a(0) == 0:
                self.ret[0x006A51A0] = 0
            self.put(g["tmpl"] + 0xC0, a(1))
            self.put(g["tmpl"] + 0xCC, a(2))
            if a(3) & 1:
                self.ret[0x00685EE0] = 1
            if a(3) & 2:
                self.ret[0x00685EC0] = 1
            if a(3) & 4:
                self.ret[0x006FFB00] = 1
            if a(3) & 8:
                self.ret[0x007A0B30] = 0
            if a(3) & 16:
                self.ret[0x00688810] = 1
            if a(3) & 32:
                self.put(g["own"] + 0x84, 0x100)
            if a(3) & 64:
                self.emu.write(g["game"] + 0x58, bytes([1]))
        elif name == "pfu":
            if a(0) == 0:
                self.ret[0x006A51A0] = 0
            if a(0) == 2:
                self.emu.write(g["own"] + 0x471, bytes([1]))
            if a(0) == 3:
                self.put(g["own"] + 0x88, 1 << 21)
            if a(0) == 4:
                self.emu.write(g["self"] + 0x24, bytes([1]))
            if a(0) == 5:
                self.ret[0x00688810] = 1
            if a(0) == 6:
                self.ret[0x00688810] = 2
            self.put(g["self"] + 0x20, g["sub"])
        elif name == "gid":
            if a(0) in (1, 2, 3):
                self.ret[0x006A6210] = g["goal"]
            if a(0) == 1:
                self.ret[0x00687CB0] = g["spot"]
                self.ret[0x00A780D0] = 1
            if a(0) == 2:
                self.vret[0x244] = 0
            if a(0) == 3:
                self.ret[0x006906A0] = 3
            if a(0) == 4:
                self.emu.write(g["self"] + 0x20, bytes([1]))
            if a(0) == 5:
                self.ret[0x006A6F60] = 1
            if a(0) == 0:
                self.ret[0x00687C30] = g["goal"]
                self.ret[0x006AD4B0] = 1
        elif name == "hws":
            if a(0) == 0:
                self.vret[0x1C4] = 0
            if a(0) == 1:
                self.ret[0x005B9F30] = 0
            if a(0) == 2:
                self.ret[0x0067F130] = 0
            if a(0) == 3:
                self.put(g["rec"] + 0x38, 1)
            self.put(g["goal"] + 0xDC, 0x11)
            self.put(g["own"] + 0xDC, 0x22)
            self.put(g["rec"] + 0x30, 1 if a(1) else 100)
            self.vret[0x0C] = 50 if a(1) else 4
            self.vret[0x04] = 1
            if a(2):
                self.put(g["own"] + 0xB4, 0x80000000)
            self.emu.write(g["game"] + 0x58, bytes([7]))
        elif name == "hen":
            if a(0) == 0:
                self.put(g["mach"] + 0x24, 0)
            if a(0) == 1:
                self.ret[0x005B9F30] = 0
            if a(0) == 2:
                self.emu.write(g["goal"] + 0x471, bytes([1]))
            if a(0) == 3:
                self.ret[0x006A7600] = 0
            if a(0) == 4:
                self.ret[0x006A7620] = 0
            if a(0) == 5:
                self.other84 = 1
            if a(0) == 8:
                self.no_weapon = True
            if a(0) == 6:
                self.vret[0x94] = 0
            if a(0) not in (5, 6):
                self.vret[0x94] = 1
            if a(0) == 7:
                self.put(g["info"] + 0xC0, 0x10)
        elif name == "gon":
            if a(0) == 0:
                self.put(g["mach"] + 0x24, 0)
            if a(0) == 1:
                self.ret[0x00687C30] = g["goal"]
                self.ret[0x006AD4B0] = 1
            if a(0) == 2:
                self.ret[0x00687CB0] = g["spot"]
        elif name == "wff":
            if a(0) == 0:
                self.ret[0x006A51A0] = 0
            if a(0) == 1:
                self.ret[0x00688810] = 0
            if a(0) == 2:
                self.put(g["own"] + 0x84, 0x100)
                self.emu.write(g["game"] + 0x58, bytes([1]))
                self.ret[0x00688810] = 0
            if a(0) == 3:
                self.ret[0x00688810] = 1
                self.ret[0x006864D0] = 4
            self.emu.write(g["game"] + 0x58, bytes([1 if a(0) == 2 else 9]))
        elif name == "pfb":
            self.ret[0x0063EB60] = 1 if a(0) else 0
            if a(1) == 0:
                self.ret[0x005B9F30] = 0
            self.ret[0x00658A80] = 1 if a(2) else 0
        elif name == "fex":
            self.put(g["loco"] + 0x218, 0x1234)
            self.ret[0x007527C0] = 1 if a(0) else 0
        elif name == "rld":
            if a(0):
                self.put(g["own"] + 0xB4, 0x20)
            if a(1) == 0:
                self.put(g["own"] + 0x37C, 0)
            self.emu.write(g["game"] + 0x58, bytes([9]))
        elif name == "mow":
            if a(0):
                self.put(g["own"] + 0xB0, 1 << 22)
        elif name == "jla":
            self.ret[0x006E4760] = 0 if a(0) else g["spot"]
            self.vret[0x3C] = a(1)
            if a(2) == 0:
                self.put(g["loco"] + 0x214, 0)
            self.vret[0x14] = 0 if a(3) else 1
            self.put(g["module"] + 0x40, 0xFFFFFFFF)
        elif name == "off":
            self.put(g["team"] + 0x28C, 3)
            self.ret[0x007EBE30] = 0 if a(0) else 1
            self.ret[0x007E6AF0] = a(1)
            if a(2) == 1:
                self.put(g["node"], g["player"] + 0x88)
                self.put(g["node"] + 8, g["rec"])
                self.put(g["rec"] + 0x0C, g["info"])
                self.put(g["info"] + 4, g["team"])
                self.put(g["player"] + 0x88, g["node"])
            elif a(2) == 2:
                self.put(g["node"], g["player"] + 0x88)
                self.put(g["node"] + 8, g["rec"])
                self.put(g["rec"] + 0x0C, g["info"])
                self.put(g["info"] + 4, 1)
                self.put(g["player"] + 0x88, g["node"])
            self.ret[0x007A1570] = 0 if a(3) else 1
            self.ret[0x007A1400] = 0 if a(4) else 1
            ecx = g["player"]
            stack = (g["team"],)
        elif name == "cid":
            if a(0) == 0:
                self.ret[0x005B9F30] = 0
            if a(0) == 1:
                self.put(g["own"] + 0xD4, 0x10)
            if a(0) == 2:
                self.put(g["goal"] + 0xD4, 8)
        elif name == "egl":
            if a(0) == 0:
                self.ret[0x005B9F30] = 0
            if a(0) == 2:
                self.put(g["loco"] + 0x214, 0)
            self.put(g["module"] + 0x40, 0x10)
        elif name == "pke":
            if a(0) == 1:
                self.put(g["tmpl"] + 0xC8, 0x100)
            if a(0) == 2:
                self.put(g["own"] + 0xB4, 0x20)
            self.ret[0x0079CC50] = 0 if a(0) == 3 else 1
            self.put(g["mach"] + 0x54, 0x7F7FFFFF if a(0) == 4 else 0x3F800000)
        elif name == "pkx":
            if a(0):
                self.put(g["own"] + 0xB4, 0x20)
            if a(1) == 0:
                self.put(g["own"] + 0x374, 0)
            self.put(g["loco"] + 0x21C, 3 if a(1) == 2 else 1)
            stack = (a(2),)
        elif name == "lnd":
            self.emu.write(g["self"] + 0x3C, bytes([0 if a(0) else 1]))
            self.ret[0x006E4760] = 0 if a(1) else g["spot"]
            self.fill = 0 if a(2) else g["pad"]
            if a(3):
                self.put(g["info"] + 0xC0, 0x10)
            self.ret[0x00687560] = 0 if a(4) else 1
            self.vret[0x14] = 0 if a(5) else 1
            self.ret[0x0079CEE0] = 0x33
            self.put(g["own"] + 0xDC, 0x22)
        elif name == "tku":
            self.put(g["self"] + 0x38, a(0))
            self.put(g["self"] + 0x20, 0x10)
            self.ret[0x0079CEE0] = 0xFFFFFFFF if a(1) else 0
            if a(2):
                self.put(g["own"] + 0xB4, 0x10)
        elif name == "tng":
            self.put(g["self"] + 0x20, 0 if a(0) else g["sub"])
            self.ret[0x006D21B0] = 1 if a(1) else 0
            if a(2):
                self.put(g["tmpl"] + 0xC0, 1 << 26)
        elif name == "mpx":
            if a(0) & 1:
                self.put(g["own"] + 0x90, 1 << 29)
            if a(0) & 2:
                self.put(g["own"] + 0x88, 1 << 14)
            if a(0) & 4:
                self.put(g["own"] + 0xB0, 0xE0000000)
            if a(0) & 8:
                self.put(0x00DF03FC, g["reg"])
            self.put(g["self"] + 0x20, a(1))
            if a(2) == 0:
                self.put(g["own"] + 0x374, 0)
            if a(2) == 2:
                self.put(g["loco"] + 0x214, 0)
            self.emu.write(g["module"] + 0xA4, bytes([1]))
            stack = (0,)
        elif name == "par":
            if a(0):
                self.put(g["own"] + 0xA0, 1 << 19)
        elif name == "amg":
            if a(0):
                self.put(g["own"] + 0xB0, 1 << 25)
            self.put(g["mach"] + 0x54, 0x3F800000)
        elif name == "aps":
            self.put(g["own"] + 0xA8, a(0))
        elif name == "jag":
            self.emu.write(g["mach"] + 0x7C, bytes([7]))
            if a(0):
                self.put(g["own"] + 0xB0, 1 << 25)
            self.put(g["mach"] + 0x54, 0x40000000)
        elif name == "xim":
            self.put(g["xfer"] + 4, a(0))
            if a(1):
                self.put(0x00DF03FC, g["reg"])
            self.live = True
            stack = (g["xfer"],)
        elif name in ("xfw", "xap", "xpu", "xmv", "xfd", "xfp", "xmh", "xsq", "xft"):
            self.put(g["xfer"] + 4, a(0))
            if name == "xfw":
                self.put(g["self"] + 0x60, g["goal"] if a(1) else 0)
                self.put(g["goal"] + 4, 0x42)
                if a(0) & 2:
                    self.put(0x00DFC190, g["reg"])
            if name == "xmv":
                self.put(g["self"] + 0x58, g["goal"])
            if name == "xft":
                self.put(g["self"] + 0x60, g["goal"] if a(1) else 0)
            stack = (g["xfer"],)
        elif name == "xpa":
            self.put(g["xfer"] + 4, a(0))
            self.ver = a(1) - 1
            if self.ver < 0:
                self.ver = -1
            stack = (g["xfer"],)
        elif name == "xga":
            self.put(g["self"] + 0x20, g["sub"] if a(0) else 0)
            self.ret[0x00416820] = 0 if a(1) else g["block"]
            self.ret[0x007A1BD0] = 0 if a(2) else g["block"]
            self.poke = a(3)
            stack = (g["xfer"],)
        elif name == "xex":
            self.put(g["xfer"] + 4, a(0))
            self.put(g["self"] + 0x50, g["goal"] if a(1) else 0)
            self.put(g["goal"] + 4, 0x42)
            if a(0) & 2:
                self.put(0x00DFC190, g["reg"])
            stack = (g["xfer"],)
        elif name in ("xtn", "xbk", "xch"):
            if name == "xtn":
                self.put(g["xfer"] + 4, a(0))
            stack = (g["xfer"],)
        elif name == "xme":
            self.put(g["xfer"] + 4, a(0))
            self.put(g["self"] + 0x50, g["sub"] if a(1) else 0)
            self.put(g["self"] + 0x54, 0x54)
            self.put(g["self"] + 0x78, 0x78)
            self.ret[0x00416820] = 0 if a(2) else g["block"]
            stack = (g["xfer"],)
        elif name == "xai":
            self.put(g["xfer"] + 4, a(0))
            if a(1):
                self.put(g["node"], g["self"] + 8)
                self.put(g["node"] + 8, g["goal"] if a(2) else 0)
                self.put(g["self"] + 8, g["node"])
            stack = (g["xfer"],)
        eax = self.run(FN[name], ecx, stack)
        return {"eax": "00000000", "kind": self.kind(eax), "log": ",".join(self.log) if self.log else ".", "mem": self.mem()}


def cases() -> list[str]:
    rows = [
        "pfw 0 0 0 0", "pfw 1 0 0 0", "pfw 1 0x10 0 0", "pfw 1 0 0x400 0",
        "pfw 1 0 0 1", "pfw 1 0 0 4", "pfw 1 0 0 8", "pfw 1 0 0 16", "pfw 1 0 0 96",
        "pfu 0", "pfu 1", "pfu 2", "pfu 3", "pfu 4", "pfu 5", "pfu 6",
        "gid 0", "gid 1", "gid 2", "gid 3", "gid 4", "gid 5",
        "hws 0 0 0", "hws 1 0 0", "hws 2 0 0", "hws 3 0 0", "hws 4 0 0", "hws 4 1 0", "hws 4 0 1",
        "hen 0", "hen 1", "hen 2", "hen 3", "hen 4", "hen 5", "hen 6", "hen 7", "hen 8", "hen 9",
        "gon 0", "gon 1", "gon 2", "gon 3",
        "wff 0", "wff 1", "wff 2", "wff 3",
        "pfb 0 1 1", "pfb 1 0 1", "pfb 1 1 0", "pfb 1 1 1",
        "fex 0", "fex 1",
        "rld 0 0", "rld 1 1", "rld 0 1",
        "mow 0", "mow 1",
        "jla 1 0 1 0", "jla 0 1 1 0", "jla 0 0 0 0", "jla 0 0 1 1", "jla 0 0 1 0",
        "off 1 0 0 0 0", "off 0 4 0 0 0", "off 0 0 1 0 0", "off 0 0 2 0 0", "off 0 0 0 1 0", "off 0 0 0 0 1",
        "cid 0", "cid 1", "cid 2", "cid 3",
        "egl 0", "egl 1", "egl 2",
        "pke 0", "pke 1", "pke 2", "pke 3", "pke 4",
        "pkx 0 1 0", "pkx 1 0 0", "pkx 0 2 1",
        "lnd 0 0 0 0 0 0", "lnd 1 0 0 0 0 0", "lnd 0 1 0 0 0 0", "lnd 0 0 1 0 0 0",
        "lnd 0 0 0 1 0 0", "lnd 0 0 0 0 1 0", "lnd 0 0 0 0 0 1",
        "tku 0 0 0", "tku 1 1 0", "tku 2 1 1",
        "tng 1 0 0", "tng 0 1 0", "tng 0 1 1", "tng 0 0 0",
        "mpx 0 0 1", "mpx 1 0 1", "mpx 2 0 1", "mpx 4 6 1", "mpx 0 0 0", "mpx 0 0 2", "mpx 8 5 1",
        "par 0", "par 1",
        "amg 0", "amg 1",
        "aps 0", "aps 1", "aps 4",
        "jag 0", "jag 1",
        "xim 0 0", "xim 0 1",
        "xfw 0 0", "xfw 8 0", "xfw 2 1", "xfw 0 1",
        "xap 0", "xap 8",
        "xpa 0 0", "xpa 8 0", "xpa 0 1",
        "xpu 0", "xpu 8",
        "xga 0 0 0 0", "xga 1 0 0 0", "xga 0 0 0 1", "xga 0 1 0 1", "xga 0 0 1 1",
        "xmv 0", "xmv 8",
        "xfd 0", "xfd 8",
        "xex 0 0", "xex 8 0", "xex 2 1", "xex 2 0",
        "xtn 0", "xtn 8",
        "xbk",
        "xfp 0", "xfp 8",
        "xmh 0", "xmh 8",
        "xsq 0", "xsq 8",
        "xch",
        "xft 0 0", "xft 8 0", "xft 0 1",
        "xme 0 0 0", "xme 8 0 0", "xme 0 1 0", "xme 0 0 1",
        "xai 0 0 0", "xai 4 1 1", "xai 4 1 0",
    ]
    return rows


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
    finally:
        checker.close()
    print(f"cases={len(cases())} mismatches={len(failures)}")
    for line in failures[:30]:
        print(line)
    return 1 if failures else 0


if __name__ == "__main__":
    sys.exit(main())
