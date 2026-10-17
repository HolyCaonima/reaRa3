"""Differential oracle for the second 0x00720000 object-module batch.

The Release checker prints eax/log/mem/stk tokens. Unicorn runs the shipped
bytes with the same inputs and the same stub side effects.
"""

from __future__ import annotations

import struct
import subprocess
import sys
from pathlib import Path

from unicorn import x86_const

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from emulate import Emulator  # noqa: E402

EXE = Path(__file__).resolve().parents[3] / "build" / "win32" / "bin" / "Release" / "ra3_obj72bcheck.exe"

MAP_ROOT = 0x00DEBA64
PLAYER = 0x00DFBD74
SCAN = 0x00DF60A0
MEMMOVE = 0x00BBA500


def u32(value: int) -> int:
    return value & 0xFFFFFFFF


def p32(value: int) -> bytes:
    return struct.pack("<I", u32(value))


def fmt(value: int) -> str:
    value = u32(value)
    if value < 0x10000 or value >= 0xF0000000:
        return f"{value:x}"
    return "p"


def hex8(value: int) -> str:
    return f"{u32(value):08x}"


class Oracle:
    def __init__(self) -> None:
        self.emu = Emulator()
        self.log: list[str] = []
        self.seq = 0
        self.usable = 1
        self.ready = 1
        self.query = 0
        self.cur = 0
        self.near = 1
        self.module = 0
        self.chain_ret = 0
        self.count = 0
        self.whit = 0
        self.whit2 = 0
        self.pred = 0x110
        self.ver_out = -1
        self.v70arm = 0
        self.poke = False
        self.find_hit = False
        self.map_hit = False
        self.hit_node = 0
        self.end_node = 0
        self.node = 0
        self.pass_ret = 0
        self.list_vt = 0
        self.bases: list[tuple[str, int, int]] = []
        self.w: dict[str, int] = {}
        self._bind()

    def _bind(self) -> None:
        e = self.emu

        def stub(name: str, cleanup: int, body):
            return e.bind_stub(body, cleanup)

        self.vt_slot = {}
        self.vt_slot["v34"] = stub("v34", 0, lambda emu: self.take("v34"))
        self.vt_slot["v1c"] = stub("v1c", 4, lambda emu: self.take("v1c:" + fmt(emu.stack_arg(0))))
        self.vt_slot["v38"] = stub("v38", 4, lambda emu: self.take("v38:" + fmt(emu.stack_arg(0))))
        self.vt_slot["del"] = stub("del", 4, lambda emu: self.take("del:" + fmt(emu.stack_arg(0))))
        self.vt_slot["n"] = stub("n", 4, lambda emu: self.note_ret("n:" + fmt(self.count), self.count))
        self.vt_slot["ps"] = stub("ps", 4, lambda emu: self.note_ret("ps", self.pass_ret))
        self.vt_slot["w24"] = stub("w24", 0, lambda emu: self.note_ret("w24" if self.whit else "w24", self.whit))
        self.vt_slot["w20"] = stub("w20", 4, lambda emu: self.note_ret("w20:" + fmt(emu.stack_arg(0)), self.whit2))
        self.vt_slot["ch"] = stub("ch", 0, lambda emu: self.note_ret("ch", self.chain_ret))
        self.vt_slot["hv"] = stub(
            "hv", 8, lambda emu: self.take("hv:" + fmt(emu.stack_arg(0)) + ":" + fmt(emu.stack_arg(1)))
        )
        for slot in (0x18, 0x58, 0x64, 0x6C, 0x70, 0x7C, 0x84):
            self.vt_slot[slot] = stub(f"p{slot:x}", 4, lambda emu, slot=slot: self.on_ptr(slot))
        self.vt_slot[0x04] = stub("v4", 4, lambda emu: self.take("v4:" + hex8(emu.stack_arg(0))))
        self.vt_slot[0x08] = stub("v8", 0, lambda emu: self.take("v8"))
        self.vt_slot[0x0C] = stub("vc", 4, lambda emu: self.take("vc:" + hex8(emu.stack_arg(0))))
        self.vt_slot[0x14] = stub(
            "v14", 8, lambda emu: self.take("v14:" + self.rel(emu.stack_arg(0)) + ":" + fmt(emu.stack_arg(1)))
        )
        self.vt_slot[0x20] = stub("v20", 4, lambda emu: self.take("v20:" + fmt(emu.stack_arg(0))))

        b = e.bind_function
        b(0x006C0ED0, lambda emu: self.note_ret("u:" + fmt(self.usable), self.usable), 0)
        b(0x0063EB60, lambda emu: self.note_ret("near:" + fmt(self.near), self.near), 0)
        b(0x006B67A0, self.on_steer, 12)
        b(0x005AC4F0, self.on_place, 8)
        b(0x0072CF30, lambda emu: self.take("cf:" + self.floats(emu.stack_arg(0))), 4)
        b(0x0072CE90, lambda emu: self.take("ce:" + self.floats(emu.stack_arg(0))), 4)
        b(0x0072CD10, lambda emu: self.take("cd:" + fmt(emu.stack_arg(0))), 4)
        b(0x005BDCC0, lambda emu: self.take("bp"), 4)
        b(0x006A1770, self.on_find, 4)
        b(0x006A1720, self.on_find_alt, 8)
        b(0x006C0F40, lambda emu: self.take("mk:" + fmt(emu.stack_arg(0))), 4)
        b(0x00986150, lambda emu: self.take("nk:" + hex8(emu.stack_arg(0))), 4)
        b(0x00687460, lambda emu: self.note_ret("fm:" + fmt(emu.stack_arg(0)), self.module), 4)
        b(0x006D7810, lambda emu: self.take("pk"), 0)
        b(0x00729D50, lambda emu: self.take("dg:" + fmt(emu.stack_arg(0))), 4)
        b(0x00681030, lambda emu: self.take("vh:" + fmt(emu.stack_arg(0))), 4)
        b(0x005B9F30, lambda emu: self.note_ret("q:" + fmt(self.query), self.query), 0)
        b(0x006EFE40, lambda emu: self.note_ret("cur:" + fmt(self.cur), self.cur), 0)
        b(0x00686B60, lambda emu: self.take("np"), 4)
        b(0x00844460, lambda emu: self.take("sc:" + fmt(emu.stack_arg(0)) + ":" + fmt(emu.stack_arg(1))), 8)
        b(0x006D3870, lambda emu: self.note_ret("et:" + fmt(self.pred), self.pred), 4)
        b(0x004D9AB0, lambda emu: self.take("key:" + fmt(emu.stack_arg(0))), 4)
        b(0x00718B80, lambda emu: self.take("xb:" + fmt(emu.stack_arg(0)) + ":" + fmt(emu.stack_arg(1))), 8)
        b(0x00A21BE0, lambda emu: self.take("lk"), 4)
        b(0x0050B5F0, self.on_find_map, 8)
        b(0x00A21C90, lambda emu: self.take("ul"), 4)
        b(0x006B3660, lambda emu: self.take("fc:" + hex8(emu.stack_arg(0)) + ":" + fmt(emu.stack_arg(1))), 8)
        b(0x00731C20, lambda emu: self.note_ret("rdy:" + fmt(self.ready), self.ready), 0)
        b(0x007321F0, lambda emu: self.take("ab"), 4)
        b(0x008D82F0, lambda emu: self.take("rt"), 4)
        b(0x00685080, lambda emu: self.take("a10:" + self.rel(emu.register(x86_const.UC_X86_REG_ECX))), 4)
        b(0x006858E0, lambda emu: self.take("sth"), 4)
        b(0x0072E810, lambda emu: self.take("ai"), 4)
        b(0x0071FAB0, lambda emu: self.take("ms"), 4)
        b(0x0071D020, lambda emu: self.take("fd:" + self.rel(emu.stack_arg(1))), 0)
        b(0x0071D140, lambda emu: self.take("fb:" + self.rel(emu.stack_arg(1))), 0)
        b(0x0054A090, lambda emu: self.take("pr:" + self.rel(emu.stack_arg(1))), 0)
        b(0x00549D30, lambda emu: self.take("pb:" + self.rel(emu.stack_arg(1))), 0)
        b(0x00708050, lambda emu: self.take("ex:" + self.rel(emu.stack_arg(1))), 0)
        b(0x00980820, self.on_string, 0)
        b(0x006D38E0, lambda emu: self.take("ls:" + self.rel(emu.stack_arg(1))), 0)
        b(0x00416820, self.on_alloc, 0)
        b(0x007B6840, self.on_ctor, 0)
        b(0x007DC280, lambda emu: self.take("pu:" + fmt(emu.stack_arg(0))), 4)
        b(0x007DB000, lambda emu: self.take("sl"), 4)
        b(0x004166B0, lambda emu: self.take("fr"), 0)
        b(0x006BB310, lambda emu: self.note_ret("pair:" + fmt(emu.stack_arg(0)) + ":" + fmt(emu.stack_arg(1)), 0), 8)
        b(0x006A88F0, lambda emu: self.take("tp"), 8)
        b(0x00981070, lambda emu: self.take("adj"), 4)
        b(0x00717D50, lambda emu: self.take("lap"), 12)
        b(0x00417070, self.on_str, 0)
        b(0x008835F0, lambda emu: self.take("cl"), 0)
        b(0x00710520, lambda emu: self.take("br"), 0)
        b(0x007282C0, lambda emu: self.take("ar:" + fmt(emu.stack_arg(0))), 4)
        b(0x00731E00, lambda emu: self.take("tg:" + fmt(emu.stack_arg(0))), 4)
        b(0x004D99E0, lambda emu: self.note_ret("an", self.node), 8)
        b(0x008758C0, lambda emu: self.take("ln"), 0)
        b(0x00879050, lambda emu: self.take("po:" + fmt(emu.stack_arg(1))), 0)
        b(0x00550290, lambda emu: self.take("wp"), 0)
        b(0x00712390, lambda emu: self.take("df:" + fmt(emu.stack_arg(0) & 0xFF)), 4)
        b(0x0072CFD0, self.on_move, 16)
        b(0x0098DF80, lambda emu: self.take("sp:" + hex8(emu.stack_arg(1)) + ":" + fmt(emu.stack_arg(2))), 12)
        b(0x0071DBB0, lambda emu: self.take("rz:" + self.rel(emu.register(x86_const.UC_X86_REG_ECX)) + ":" + fmt(emu.stack_arg(0))), 4)
        b(0x00716020, self.on_project, 20)
        b(0x007016B0, lambda emu: self.take("th:" + fmt(emu.stack_arg(0))), 4)
        b(0x006A4FB0, lambda emu: self.take("mp"), 0)
        e.bind_import(MEMMOVE, lambda emu: self.take("mm:" + fmt(emu.stack_arg(2))), 0)

    def note(self, text: str) -> None:
        self.seq += 1
        self.log.append(text)

    def take(self, text: str) -> int:
        self.note(text)
        return self.seq * 0x10

    def note_ret(self, text: str, value: int) -> int:
        self.note(text)
        return u32(value)

    def rel(self, address: int) -> str:
        address = u32(address)
        for tag, start, size in self.bases:
            if start <= address < start + size:
                return f"{tag}{address - start:x}"
        return fmt(address)

    def floats(self, address: int) -> str:
        raw = self.emu.read(address, 12)
        a, b, c = struct.unpack("<III", raw)
        return f"{hex8(a)}:{hex8(b)}:{hex8(c)}"

    def rd(self, address: int) -> int:
        return struct.unpack("<I", self.emu.read(address, 4))[0]

    def wr(self, address: int, value: int) -> None:
        self.emu.write(address, p32(value))

    def on_ptr(self, slot: int) -> int:
        arg = self.emu.stack_arg(0)
        if slot == 0x18:
            raw = self.emu.read(arg, 2)
            text = f"v18:{raw[0]:02x}{raw[1]:02x}"
            if self.ver_out >= 0:
                self.emu.write(arg + 1, bytes([self.ver_out & 0xFF]))
            return self.take(text)
        if slot == 0x7C:
            return self.take("v7c:" + self.rel(arg) + ":" + fmt(self.emu.read(arg, 1)[0]))
        if slot == 0x70 and self.v70arm > 0:
            if self.v70arm == 1:
                self.wr(arg, 0x55)
                self.v70arm = 0
            else:
                self.v70arm -= 1
        if slot == 0x84 and self.poke:
            token = self.take("v84:" + self.rel(arg) + ":" + fmt(self.emu.read(arg, 1)[0]))
            self.emu.write(arg, b"\x01")
            self.poke = False
            return token
        if slot == 0x84:
            return self.take("v84:" + self.rel(arg) + ":" + fmt(self.emu.read(arg, 1)[0]))
        return self.take(f"v{slot:x}:" + self.rel(arg))

    def on_steer(self, emu: Emulator) -> int:
        return self.take("st:" + self.floats(emu.stack_arg(0)) + ":" + hex8(emu.stack_arg(1)) + ":" + fmt(emu.stack_arg(2)))

    def on_place(self, emu: Emulator) -> int:
        return self.take("pl:" + self.floats(emu.stack_arg(0)) + ":" + hex8(emu.stack_arg(1)))

    def on_find(self, emu: Emulator) -> int:
        self.note("fw" if self.find_hit else "fw0")
        return self.whit if self.find_hit else 0

    def on_find_alt(self, emu: Emulator) -> int:
        self.note("fa" if self.find_hit else "fa0")
        return self.whit if self.find_hit else 0

    def on_find_map(self, emu: Emulator) -> int:
        self.note("fmap")
        out = emu.stack_arg(0)
        self.wr(out, self.hit_node if self.map_hit else self.end_node)
        return 0

    def on_string(self, emu: Emulator) -> int:
        field = emu.stack_arg(1)
        return self.take("xs:" + hex8(self.rd(field)) + ":" + hex8(self.rd(field + 4)))

    def on_alloc(self, emu: Emulator) -> int:
        size = emu.stack_arg(0)
        self.note("al:" + fmt(size))
        return self.emu.alloc(b"\x00" * max(size, 4))

    def on_ctor(self, emu: Emulator) -> int:
        self.note("ct")
        ecx = emu.register(x86_const.UC_X86_REG_ECX)
        self.wr(ecx, self.list_vt)
        return ecx

    def on_str(self, emu: Emulator) -> int:
        self.note("str")
        ecx = emu.register(x86_const.UC_X86_REG_ECX)
        self.wr(ecx, 0)
        self.wr(ecx + 4, 0)
        return ecx

    def on_move(self, emu: Emulator) -> int:
        return self.take(
            "mv:" + fmt(emu.stack_arg(0)) + ":" + fmt(emu.stack_arg(1)) + ":" + hex8(emu.stack_arg(2)) + ":" + fmt(emu.stack_arg(3))
        )

    def on_project(self, emu: Emulator) -> int:
        return self.take("pj:" + self.floats(emu.stack_arg(1)) + ":" + fmt(emu.stack_arg(3)))

    def alloc(self, size: int) -> int:
        return self.emu.alloc(b"\x00" * size)

    def watch(self, tag: str, address: int, size: int) -> None:
        self.bases.append((tag, address, size))

    def vt(self, slots: dict[int, int], size: int = 0x100) -> int:
        data = bytearray(size)
        for offset, target in slots.items():
            struct.pack_into("<I", data, offset, target)
        return self.emu.alloc(bytes(data))

    def reset_case(self) -> None:
        self.emu.reset_data()
        self.log = []
        self.seq = 0
        self.usable = 1
        self.ready = 1
        self.query = 0
        self.cur = 0
        self.near = 1
        self.module = 0
        self.chain_ret = 0
        self.count = 0
        self.whit = 0
        self.whit2 = 0
        self.pred = 0x110
        self.ver_out = -1
        self.v70arm = 0
        self.poke = False
        self.find_hit = False
        self.map_hit = False
        self.bases = []
        w = {}
        w["self"] = self.alloc(0x800)
        w["obj"] = self.alloc(0x500)
        w["loco"] = self.alloc(0x80)
        w["xfer"] = self.alloc(0x20)
        w["pose"] = self.alloc(0x40)
        w["point"] = self.alloc(0x10)
        w["path"] = self.alloc(0x10)
        w["pts"] = self.alloc(0x30)
        w["weapon"] = self.alloc(0x40)
        w["set"] = self.alloc(0x40)
        w["draw"] = self.alloc(0x80)
        w["mounted"] = self.alloc(0x200)
        w["out"] = self.alloc(0x10)
        w["hit"] = self.alloc(0x40)
        w["held"] = self.alloc(0x100)
        w["chain"] = self.alloc(0x10)
        w["handler"] = self.alloc(0x10)
        w["node"] = self.alloc(0x20)
        w["map"] = self.alloc(0x200)
        w["passenger"] = self.alloc(0x400)
        w["face"] = self.alloc(0x40)
        w["templ"] = self.alloc(0x140)
        w["matrix"] = self.alloc(0x40)
        w["vec"] = self.alloc(0x10)
        w["rec"] = self.alloc(0x20)
        w["named"] = self.alloc(0x20)
        w["player"] = self.alloc(0x40)
        w["info"] = self.alloc(0x40)
        w["sub"] = self.alloc(0x10)
        w["items"] = self.alloc(0x10)
        s = self.vt_slot
        w["locoVt"] = self.vt({0x1C: s["v1c"], 0x34: s["v34"], 0x38: s["v38"]})
        w["xferVt"] = self.vt({k: s[k] for k in (0x04, 0x08, 0x0C, 0x14, 0x18, 0x20, 0x58, 0x64, 0x6C, 0x70, 0x7C, 0x84)})
        w["listVt"] = self.vt({0x00: s["del"]}, 0x20)
        w["faceVt"] = self.vt({0x10C: s["n"], 0x128: s["ps"]}, 0x140)
        w["weaponVt"] = self.vt({0x20: s["w20"], 0x24: s["w24"]}, 0x40)
        w["chainVt"] = self.vt({0x84: s["ch"]}, 0x90)
        w["handVt"] = self.vt({0x84: s["hv"]}, 0x90)
        self.list_vt = w["listVt"]
        self.wr(w["loco"], w["locoVt"])
        self.wr(w["xfer"], w["xferVt"])
        self.wr(w["weapon"], w["weaponVt"])
        self.wr(w["chain"], w["chainVt"])
        self.wr(w["handler"], w["handVt"])
        self.wr(w["weapon"] + 0xC, w["sub"])
        self.wr(w["self"] + 8, w["obj"])
        self.wr(w["self"] + 0x3C, w["loco"])
        self.wr(w["self"] + 4, w["templ"])
        self.wr(w["obj"] + 0xDC, 0x61)
        self.end_node = w["map"] + 0x190
        self.hit_node = w["map"] + 0x1A0
        self.node = w["node"]
        self.pass_ret = w["passenger"]
        self.wr(w["map"] + 0xC4 + 4, w["map"] + 0x180)
        self.wr(w["map"] + 0xC4 + 8, 0)
        self.wr(w["map"] + 0x180, self.end_node)
        for tag, key, size in (
            ("s", "self", 0x800), ("o", "obj", 0x500), ("x", "xfer", 0x20), ("p", "point", 0x10),
            ("a", "pts", 0x30), ("u", "out", 0x10), ("m", "mounted", 0x200), ("r", "rec", 0x20),
            ("h", "hit", 0x40), ("d", "draw", 0x80), ("n", "node", 0x20), ("e", "held", 0x100),
            ("q", "pose", 0x40), ("w", "weapon", 0x40), ("b", "passenger", 0x400),
        ):
            self.watch(tag, w[key], size)
        self.w = w

    def plant_map(self, value: int) -> None:
        self.map_hit = True
        self.wr(self.hit_node + 4, value)
        self.wr(MAP_ROOT, self.w["map"])

    def one_point(self, x: int, y: int, z: int) -> None:
        self.wr(self.w["pts"], x)
        self.wr(self.w["pts"] + 4, y)
        self.wr(self.w["pts"] + 8, z)
        self.wr(self.w["path"], self.w["pts"])
        self.wr(self.w["path"] + 4, self.w["pts"] + 12)

    def call(self, address: int, args: list[int], ecx: int) -> int:
        self.emu.uc.reg_write(x86_const.UC_X86_REG_FPCW, 0x037F)
        return self.emu.call(address, args, ecx=ecx).eax

    def invoke(self, address: int, args: list[int], ecx: int) -> tuple[int, int]:
        self.emu.uc.reg_write(x86_const.UC_X86_REG_FPCW, 0x037F)
        result = self.emu.call(address, args, ecx=ecx)
        return result.eax, result.stack_cleanup

    def tag(self, eax: int) -> str:
        w = self.w
        if eax == w["obj"]:
            return "obj"
        if eax == w["out"]:
            return "out"
        if eax == w["draw"]:
            return "draw"
        if eax == w["mounted"]:
            return "mnt"
        if (eax & 0xFFFFFF00) == (w["mounted"] & 0xFFFFFF00) and (eax & 0xFF) == (self.ready & 0xFF):
            return "mix" + fmt(eax & 0xFF)
        return fmt(eax)

    def snapshot(self, eax: int, cleanup: int) -> str:
        w = self.w
        s = w["self"]
        collapsed = self.rd(s + 0xC8) == self.rd(s + 0xCC)
        head = self.rd(s + 0xD8) == (s + 0xD8)

        def dword(name: str, address: int) -> str:
            return f"{name}={hex8(self.rd(address))}"

        mem = ",".join([
            dword("s50", s + 0x50), dword("s180", s + 0x180), dword("s190", s + 0x190),
            dword("s1f0", s + 0x1F0), dword("s308", s + 0x308), dword("s4c", s + 0x4C),
            dword("sbc", s + 0xBC), dword("s56c", s + 0x56C), dword("s110", s + 0x110),
            dword("s114", s + 0x114), dword("s118", s + 0x118),
            "b432=" + format(self.emu.read(s + 0x432, 1)[0], "x"),
            dword("o490", w["obj"] + 0x490), dword("w24", w["weapon"] + 0x24),
            dword("h24", w["hit"] + 0x24), dword("pbc", w["passenger"] + 0xBC),
            dword("m1bc", w["mounted"] + 0x1BC), dword("u0", w["out"]), dword("u4", w["out"] + 4),
            dword("u8", w["out"] + 8), dword("hec", w["held"] + 0xEC), dword("n8", w["node"] + 8),
            dword("qc", w["pose"] + 0xC), "cc=" + ("1" if collapsed else "0"), "hd=" + ("1" if head else "0"),
        ])
        return f"eax={self.tag(eax)} log={','.join(self.log)} mem={mem} stk={fmt(cleanup)}"

    def go(self, cmd: str) -> str:
        self.reset_case()
        eax, cleanup = self.dispatch(cmd)
        return self.snapshot(eax, cleanup)

    def dispatch(self, cmd: str) -> tuple[int, int]:
        w = self.w
        s = w["self"]
        o = w["obj"]
        if cmd == "snapFar":
            self.near = 1
            self.wr(o + 0x38, 0)
            self.wr(w["point"], 0x43480000)
            self.wr(s + 0x214, w["pose"])
            return self.invoke(0x0072D090, [w["point"], 0, 0], s)
        if cmd == "snapNear":
            self.wr(o + 0x38, 0)
            self.wr(w["point"], 0x3F800000)
            return self.invoke(0x0072D090, [w["point"], 1, 2], s)
        if cmd == "snapOff":
            self.near = 0
            self.wr(w["point"], 0x3F800000)
            return self.invoke(0x0072D090, [w["point"], 0, 3], s)
        if cmd == "flagBad":
            self.usable = 0x100
            return self.invoke(0x0072D9C0, [w["path"], o, 0, 0], s)
        if cmd == "flagEmpty":
            return self.invoke(0x0072D9C0, [w["path"], o, 1, 0], s)
        if cmd == "flagGo":
            self.one_point(0x3F800000, 0x40000000, 0x40400000)
            return self.invoke(0x0072D9C0, [w["path"], o, 0, 0], s)
        if cmd == "flagBit":
            self.one_point(0x3F800000, 0, 0)
            return self.invoke(0x0072D9C0, [w["path"], 0, 2, 1], s)
        if cmd == "p34Empty":
            return self.invoke(0x0072DAB0, [w["path"], o, 0x3F800000, 0], s)
        if cmd == "p34Go":
            self.one_point(0x3F800000, 0x40000000, 0x40400000)
            return self.invoke(0x0072DAB0, [w["path"], o, 0x3F800000, 0], s)
        if cmd == "p3bGo":
            self.one_point(0x3F800000, 0, 0)
            return self.invoke(0x0072DBA0, [w["path"], o, 0x40000000, 2], s)
        if cmd == "p34Bad":
            self.usable = 0x100
            return self.invoke(0x0072DAB0, [w["path"], o, 0, 0], s)
        if cmd == "ord0":
            return self.invoke(0x0072DC90, [0, 0x11, 0], s)
        if cmd == "ordBit":
            self.emu.write(o + 0x471, b"\x08")
            return self.invoke(0x0072DC90, [0x21, 0x11, 0], s)
        if cmd == "ordDraw":
            self.wr(o + 0x374, w["draw"])
            self.wr(w["draw"] + 0x40, 1)
            self.wr(o + 0x37C, w["set"])
            self.find_hit = True
            self.whit = w["weapon"]
            return self.invoke(0x0072DC90, [0x21, 0x11, 0], s)
        if cmd in ("ordGo", "ordTwice", "ordMode2"):
            self.wr(o + 0x37C, w["set"])
            self.find_hit = True
            self.whit = w["weapon"]
            mode = 2 if cmd == "ordMode2" else 0
            eax, cleanup = self.invoke(0x0072DC90, [0x21, 0x11, mode], s)
            if cmd == "ordTwice":
                eax, cleanup = self.invoke(0x0072DC90, [0x21, 0x12, 0], s)
            return eax, cleanup
        if cmd == "ordMod":
            self.module = 0x30
            return self.invoke(0x0072DC90, [0x21, 0x11, 1], s)
        if cmd == "bld0":
            return self.invoke(0x0072DF50, [0, 0, 0, 0, 0x11, 2], s)
        if cmd == "bldQ":
            self.query = 0x70
            return self.invoke(0x0072DF50, [0, 0, 0, 0, 0x11, 0], s)
        if cmd == "bldItem":
            self.wr(w["items"], 0x55)
            return self.invoke(0x0072DF50, [w["items"], w["items"] + 4, 0, 0, 0x11, 2], s)
        if cmd == "bldWep":
            self.wr(o + 0x37C, w["set"])
            self.wr(w["set"] + 0x24, w["items"])
            self.wr(w["set"] + 0x28, w["items"] + 4)
            self.wr(w["items"], w["weapon"])
            self.whit = 0
            return self.invoke(0x0072DF50, [0, 0, 0, 0, 0x11, 2], s)
        if cmd == "bldHit":
            self.wr(o + 0x37C, w["set"])
            self.wr(w["set"] + 0x24, w["items"])
            self.wr(w["set"] + 0x28, w["items"] + 4)
            self.wr(w["items"], w["weapon"])
            self.whit = w["hit"]
            return self.invoke(0x0072DF50, [0, 0, 0, 0, 0x11, 2], s)
        if cmd == "stByte":
            self.emu.write(s + 0x436, b"\x01")
            return self.invoke(0x0072E3D0, [w["point"], 0x40000000, 0, 0], s)
        if cmd == "stBad":
            self.usable = 0x100
            return self.invoke(0x0072E3D0, [w["point"], 0x40000000, 0, 0], s)
        if cmd == "stGo":
            self.wr(w["point"], 0x3F800000)
            return self.invoke(0x0072E3D0, [w["point"], 0x40000000, 0x11, 0], s)
        if cmd == "stScript":
            self.wr(o + 0x418, 0x42)
            return self.invoke(0x0072E3D0, [w["point"], 0x40000000, 0, 2], s)
        if cmd == "pl0":
            return self.invoke(0x0072E490, [0, 0], s)
        if cmd == "plBad":
            self.usable = 0x100
            return self.invoke(0x0072E490, [o, 0], s)
        if cmd == "plGo":
            self.wr(o + 0x38, 0x3F800000)
            self.wr(s + 0x214, w["pose"])
            return self.invoke(0x0072E490, [o, 0], s)
        if cmd == "plScr":
            self.wr(o + 0x418, 0x42)
            return self.invoke(0x0072E490, [o, 2], s)
        if cmd == "at0":
            return self.invoke(0x0072E720, [0, 0], s)
        if cmd == "atCur":
            self.cur = 5
            return self.invoke(0x0072E720, [0, 0], s)
        if cmd == "atChain":
            self.wr(o + 0x328, w["chain"])
            self.chain_ret = w["handler"]
            return self.invoke(0x0072E720, [0x21, 3], s)
        if cmd == "atMod":
            self.module = 0x30
            return self.invoke(0x0072E720, [0x21, 1], s)
        face = w["face"] + 0x20
        if cmd == "en0":
            self.pred = 0x200
            return self.invoke(0x007340F0, [0x12], face)
        if cmd == "enByte":
            return self.invoke(0x007340F0, [0x12], face)
        if cmd == "enOpen":
            self.emu.write(face + 4, b"\x01")
            self.wr(face - 0xC, o)
            return self.invoke(0x007340F0, [0x12], face)
        if cmd == "enKey":
            self.emu.write(face + 4, b"\x01")
            self.wr(face - 8, w["held"])
            self.wr(face - 0xC, o)
            self.wr(o + 8, w["info"])
            self.wr(w["info"], 7)
            return self.invoke(0x007340F0, [0x12], face)
        if cmd == "rel0":
            return self.invoke(0x00728F80, [1, 2], s)
        if cmd == "relHit":
            self.wr(s + 0x4C, 0x21)
            self.wr(w["held"] + 0xEC, 0x99)
            self.plant_map(w["held"])
            return self.invoke(0x00728F80, [1, 2], s)
        if cmd == "bd0":
            self.ready = 0
            return self.invoke(0x0073EFA0, [], s)
        if cmd in ("bdA", "bd2", "bdBig"):
            self.ready = 1
            self.wr(s + 0x30, 1 if cmd == "bd2" else 2)
            self.wr(s + 0x38, 2 if cmd == "bd2" else 0)
            self.wr(w["templ"] + 0x20, 0x43960000 if cmd == "bdBig" else 0x40800000)
            self.wr(w["templ"] + 0x24, 0x40800000)
            self.wr(w["templ"] + 0xF0, 0x3E800000)
            return self.invoke(0x0073EFA0, [], s)
        if cmd == "mb0":
            self.ready = 0
            return self.invoke(0x0073F430, [], s)
        if cmd in ("mbNull", "mbOn"):
            self.ready = 1
            self.wr(s + 0x30, 2)
            self.wr(w["templ"] + 0x20, 0x40800000)
            self.wr(w["templ"] + 0xF0, 0x3E800000)
            if cmd == "mbOn":
                self.wr(o + 0xF4, w["mounted"])
            return self.invoke(0x0073F430, [], s)
        if cmd == "hoEmpty":
            self.wr(o + 0x32C, 1)
            self.wr(o + 0x330, 1)
            return self.invoke(0x0073FE20, [1, 2, 0x3F800000, 3], s)
        if cmd == "hoZero":
            self.wr(o + 0x328, w["chain"])
            self.chain_ret = 0
            return self.invoke(0x0073FE20, [1, 2, 0x3F800000, 3], s)
        if cmd == "hoGo":
            self.wr(o + 0x328, w["chain"])
            self.chain_ret = 1
            return self.invoke(0x0073FE20, [1, 2, 0x3F800000, 3], s)
        if cmd == "rec0":
            return self.invoke(0x00728500, [], s)
        if cmd == "recZ":
            self.wr(s + 0x18, w["items"])
            self.wr(s + 0x1C, w["items"] + 4)
            return self.invoke(0x00728500, [], s)
        if cmd == "recHit":
            self.wr(s + 0x18, w["items"])
            self.wr(s + 0x1C, w["items"] + 4)
            self.wr(w["items"], 0x21)
            self.plant_map(0x44)
            return self.invoke(0x00728500, [], s)
        if cmd == "tg0":
            return self.invoke(0x0073E6E0, [0], s)
        if cmd == "tg1":
            self.wr(o + 0xF4, 0x77)
            return self.invoke(0x0073E6E0, [1], s)
        if cmd == "laserM":
            self.wr(s + 0x3C, 0x100)
            self.wr(s + 0x40, 0x200)
            self.wr(s + 0x44, 0x300)
            return self.invoke(0x00720200, [w["out"]], s)
        if cmd == "laserH":
            self.wr(s + 0x54, 0x21)
            self.wr(w["held"] + 0x38, 0x11)
            self.wr(w["held"] + 0x3C, 0x22)
            self.wr(w["held"] + 0x40, 0x33)
            self.plant_map(w["held"])
            return self.invoke(0x00720200, [w["out"]], s)
        if cmd == "rad0":
            head = s + 0xD8
            self.wr(s + 0xC8, head)
            self.wr(s + 0xCC, head)
            self.wr(s + 0xD8, head)
            self.wr(s + 0xDC, head)
            return self.invoke(0x00729980, [1, 2], s)
        if cmd == "radZ":
            self.wr(w["items"], 0)
            self.wr(s + 0xC8, w["items"])
            self.wr(s + 0xCC, w["items"] + 4)
            head = s + 0xD8
            self.wr(s + 0xD8, head)
            self.wr(s + 0xDC, head)
            return self.invoke(0x00729980, [1, 2], s)
        if cmd == "radHit":
            self.wr(w["items"], 0x21)
            self.wr(s + 0xC8, w["items"])
            self.wr(s + 0xCC, w["items"] + 4)
            self.wr(w["held"] + 0xF4, 0x15)
            self.plant_map(w["held"])
            self.wr(w["node"], s + 0xD8)
            self.wr(s + 0xD8, w["node"])
            self.wr(o + 0x418, w["info"])
            self.wr(w["info"] + 0x20, 3)
            self.wr(SCAN, 1)
            return self.invoke(0x00729980, [1, 2], s)
        if cmd == "trn0":
            self.wr(s + 0x24, w["faceVt"])
            self.count = 0
            return self.invoke(0x007280F0, [], s)
        if cmd == "trn1":
            self.wr(s + 0x24, w["faceVt"])
            self.wr(s + 4, w["templ"])
            self.wr(s + 8, w["matrix"])
            self.wr(w["templ"] + 0x130, w["vec"])
            self.wr(w["templ"] + 0x118, 0x99)
            self.wr(w["vec"], 0x3F800000)
            self.wr(w["matrix"] + 8, 0x3F800000)
            self.wr(w["matrix"] + 0x18, 0x40000000)
            self.wr(w["matrix"] + 0x28, 0x40800000)
            self.wr(w["passenger"] + 0x378, w["draw"])
            self.pass_ret = w["passenger"]
            self.count = 1
            return self.invoke(0x007280F0, [], s)
        return self.xfer(cmd)

    def xfer(self, cmd: str) -> tuple[int, int]:
        rows = {
            "notify": (0x00720380, 0, -1), "passive": (0x00720DF0, 0, -1), "manager": (0x00722AB0, 0, -1),
            "crate": (0x00722C00, 0, -1), "squish": (0x007232C0, 0, -1), "squish2": (0x007232C0, 0, 2),
            "squish1": (0x007232C0, 0, 1), "circ": (0x0072FB90, 0, -1), "circ1": (0x0072FB90, 0, 1),
            "invalid": (0x007305C0, 0, -1), "linear": (0x00730750, 0, -1), "miss0": (0x00730980, 0, -1),
            "missZ": (0x00730980, 2, -1), "beam": (0x0073E380, 0, -1), "clone": (0x0073EA60, 0, -1),
            "eject": (0x0073EB10, 0, -1), "dis0": (0x0073F060, 0, -1), "dis1": (0x0073F060, 2, -1),
            "hidden": (0x0073F3F0, 0, -1), "mountx": (0x0073F500, 0, -1), "status": (0x0073F550, 0, -1),
            "fire2": (0x0073F600, 4, -1), "fire0": (0x0073F600, 0, -1), "fireP": (0x0073F600, 0, -1),
            "fireF": (0x0073F600, 1, -1), "animal": (0x0073F6D0, 0, -1), "assE": (0x0073F8A0, 8, -1),
            "ass0": (0x0073F8A0, 0, -1), "ass2": (0x0073F8A0, 0, -1), "spE": (0x007212B0, 8, -1),
            "sp0": (0x007212B0, 0, -1), "sp1": (0x007212B0, 2, -1), "sc0": (0x007331A0, 0, -1),
            "sc1": (0x007331A0, 0, -1), "scR": (0x007331A0, 2, -1),
        }
        address, flags, ver = rows[cmd]
        w = self.w
        self.ver_out = ver
        self.wr(w["xfer"] + 4, flags)
        if cmd in ("miss0", "missZ"):
            self.wr(w["self"] + 0x110, 0x11111111)
            self.wr(w["self"] + 0x114, 0x22222222)
            self.wr(w["self"] + 0x118, 0x33333333)
        if cmd == "dis1":
            self.wr(w["obj"] + 0xC8, 0x200)
            self.wr(w["self"] + 0xC0, 1)
            self.wr(w["self"] + 0xD0, 1)
            self.wr(w["obj"] + 0x418, 5)
            self.wr(w["player"] + 0x2C, 5)
            self.wr(PLAYER, w["player"])
        if cmd == "fireP":
            self.poke = True
        if cmd == "fireF":
            self.wr(w["self"] + 0xBC, 0x42)
        if cmd == "ass2":
            self.wr(w["self"] + 0x574, 2)
            self.v70arm = 2
        if cmd in ("sp0", "sp1"):
            self.wr(w["self"] + 0x40, w["named"])
            self.wr(w["named"] + 4, 0x10)
            self.wr(w["named"] + 8, 0 if cmd == "sp1" else 0x20)
        if cmd in ("sc0", "sc1", "scR"):
            self.wr(w["self"] + 0x3C, 0)
            self.wr(w["self"] + 0x40, 0)
        if cmd == "sc1":
            self.wr(w["self"] + 0x2C, w["rec"])
            self.wr(w["self"] + 0x30, w["rec"] + 16)
        return self.invoke(address, [w["xfer"]], w["self"])


CASES = [
    "snapFar", "snapNear", "snapOff", "flagBad", "flagEmpty", "flagGo", "flagBit", "p34Empty", "p34Go", "p3bGo",
    "p34Bad", "ord0", "ordBit", "ordDraw", "ordGo", "ordTwice", "ordMode2", "ordMod", "bld0", "bldQ", "bldItem",
    "bldWep", "bldHit", "stByte", "stBad", "stGo", "stScript", "pl0", "plBad", "plGo", "plScr", "at0", "atCur",
    "atChain", "atMod", "en0", "enByte", "enOpen", "enKey", "rel0", "relHit", "bd0", "bdA", "bd2", "bdBig",
    "mb0", "mbNull", "mbOn", "hoEmpty", "hoZero", "hoGo", "rec0", "recZ", "recHit", "tg0", "tg1", "notify",
    "passive", "manager", "crate", "squish", "squish2", "squish1", "circ", "circ1", "invalid", "linear", "miss0",
    "missZ", "beam", "clone", "eject", "dis0", "dis1", "hidden", "mountx", "status", "fire2", "fire0", "fireP",
    "fireF", "animal", "assE", "ass0", "ass2", "laserM", "laserH", "spE", "sp0", "sp1", "sc0", "sc1", "scR",
    "rad0", "radZ", "radHit", "trn0", "trn1",
]


def main() -> int:
    wanted = sys.argv[1:] or CASES
    checker = subprocess.Popen([str(EXE)], stdin=subprocess.PIPE, stdout=subprocess.PIPE, text=True, bufsize=1)
    oracle = Oracle()
    failed = 0
    assert checker.stdin is not None and checker.stdout is not None
    for name in wanted:
        checker.stdin.write(name + "\n")
        checker.stdin.flush()
        got = checker.stdout.readline().strip()
        try:
            exp = oracle.go(name)
        except Exception as error:  # noqa: BLE001 - surface the failing case
            print(f"FAIL {name} emu {error}")
            print(f"  cpp {got}")
            failed += 1
            continue
        if got != exp:
            print(f"FAIL {name}")
            print(f"  cpp {got}")
            print(f"  emu {exp}")
            failed += 1
        else:
            print(f"ok {name}")
    checker.stdin.write("quit\n")
    checker.stdin.flush()
    checker.wait(timeout=10)
    print(f"mismatches={failed}")
    return 1 if failed else 0


if __name__ == "__main__":
    raise SystemExit(main())
