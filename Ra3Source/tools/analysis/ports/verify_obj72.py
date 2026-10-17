"""Differential oracle for the 0x00720000 object-module batch.

The Release checker prints eax/log/mem/tag tokens. Unicorn runs the shipped
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

EXE = Path(__file__).resolve().parents[3] / "build" / "win32" / "bin" / "Release" / "ra3_obj72check.exe"
AI_MEM = [0x50, 0x190, 0x308, 0x368, 0x3C8]


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


class Hints:
    def __init__(self) -> None:
        self.weapon = 0
        self.templ = 0
        self.object = 0
        self.planted = 0
        self.old_end = 0
        self.begin = 0
        self.end = 0


def classify(eax: int, hints: Hints) -> tuple[int, str]:
    eax = u32(eax)
    if hints.weapon and eax == hints.weapon:
        return 0, "wpn"
    if hints.templ and eax == hints.templ:
        return 0, "tmpl"
    if hints.object and eax == hints.object:
        return 0, "obj"
    if hints.object and (eax & 0xFFFFFF00) == (hints.object & 0xFFFFFF00) and (eax & 0xFF) == 1:
        return 1, "mix"
    if hints.planted and eax == hints.planted:
        return 0, "hit"
    if hints.old_end and eax == hints.old_end:
        return 0, "slot"
    if hints.begin and hints.begin <= eax < hints.end:
        return 0, "elem"
    if 0x10000 <= eax < 0xF0000000:
        return 0, "ptr"
    return eax, "raw"


class Checker:
    def __init__(self, executable: Path) -> None:
        self.process = subprocess.Popen(
            [str(executable)],
            stdin=subprocess.PIPE,
            stdout=subprocess.PIPE,
            text=True,
            bufsize=1,
        )

    def call(self, command: str) -> dict[str, str]:
        assert self.process.stdin is not None and self.process.stdout is not None
        self.process.stdin.write(command + "\n")
        self.process.stdin.flush()
        line = self.process.stdout.readline().strip()
        if not line:
            raise RuntimeError(f"checker closed on: {command}")
        out: dict[str, str] = {}
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
        self.seq = 0
        self.usable = 1
        self.cur = 0
        self.query = 0
        self.d2120 = 0
        self.a18a0 = 0
        self.ready = 1
        self.kind = 0
        self.idle = 0
        self.ver_out = -1
        self.stream = -1
        self.find_miss = False
        self.weapon = 0
        self.fbits = 0
        self._bind()

    def _bind(self) -> None:
        def none(slot: int):
            def stub(emu: Emulator, slot: int = slot) -> int:
                if slot == 0:
                    self.add(f"v0:{fmt(self.ready)}")
                    return self.ready
                if slot == 0x54:
                    self.add(f"v54:{fmt(self.kind)}")
                    return self.kind
                if slot == 0x288:
                    return self.take("v288")
                return self.take("v34")
            return stub

        def ptr(slot: int):
            def stub(emu: Emulator, slot: int = slot) -> int:
                return self.on_ptr(emu, slot)
            return stub

        def imm(slot: int):
            def stub(emu: Emulator, slot: int = slot) -> int:
                return self.take(f"v{slot:x}:{fmt(u32(emu.stack_arg(0)))}")
            return stub

        self.vt: dict[int, int] = {}
        for slot in (0, 0x34, 0x54, 0x288):
            self.vt[slot] = self.emu.bind_stub(none(slot), 0)
        for slot in (0x18, 0x58, 0x64, 0x6C, 0x7C, 0x84):
            self.vt[slot] = self.emu.bind_stub(ptr(slot), 4)
        for slot in (0x1C, 0x38):
            self.vt[slot] = self.emu.bind_stub(imm(slot), 4)

        bind = self.emu.bind_function
        bind(0x006C0ED0, lambda emu: self.forced("u:", self.usable), 0)
        bind(0x007997B0, lambda emu: self.take("aim:" + fmt(u32(emu.stack_arg(0)))), 4)
        bind(0x006A1770, self.on_find, 4)
        bind(0x006A1720, self.on_find_alt, 8)
        bind(0x0072CD10, lambda emu: self.take("cd:" + fmt(u32(emu.stack_arg(0)))), 4)
        bind(0x0072CE90, lambda emu: self.take("ce:" + fmt(u32(emu.stack_arg(0)))), 4)
        bind(0x0072CF30, lambda emu: self.take("cf:" + fmt(u32(emu.stack_arg(0)))), 4)
        bind(0x0072CDF0, lambda emu: self.take("cdf"), 4)
        bind(0x006C0F40, lambda emu: self.take("mk:" + fmt(u32(emu.stack_arg(0)))), 4)
        bind(0x007DED70, lambda emu: self.take("ded:" + fmt(u32(emu.stack_arg(0)))), 4)
        bind(0x005B9F30, lambda emu: self.forced("q:", self.query), 0)
        bind(0x006EFE40, lambda emu: self.forced("cur:", self.cur), 0)
        bind(0x0085CEE0, lambda emu: self.take("rpt"), 4)
        bind(0x007D2120, lambda emu: self.forced("qd:", self.d2120), 0)
        bind(0x006A18A0, lambda emu: self.forced("qa:", self.a18a0), 0)
        bind(0x00714060, lambda emu: self.take("bind"), 4)
        bind(0x005AA2A0, lambda emu: self.take("note:" + fmt(u32(emu.stack_arg(0)))), 4)
        bind(0x005AC4F0, self.on_place, 8)
        bind(0x006B67A0, self.on_steer, 12)
        bind(0x004D9AB0, lambda emu: self.take("key:" + fmt(u32(emu.stack_arg(0)))), 4)
        bind(0x00731E00, lambda emu: self.take("trig:" + fmt(u32(emu.stack_arg(0)))), 4)
        bind(0x00731C20, lambda emu: self.take("rdy"), 0)
        bind(0x00718B80, lambda emu: self.take(f"x88:{fmt(u32(emu.stack_arg(0)))}:{fmt(u32(emu.stack_arg(1)))}"), 8)
        bind(0x00710C90, lambda emu: self.take("pulse"), 0)
        bind(0x006C5030, lambda emu: self.take("beam:" + fmt(u32(emu.stack_arg(1)))), 8)
        bind(0x006D1ED0, lambda emu: self.take(f"ntf:{fmt(u32(emu.stack_arg(0)))}:{fmt(u32(emu.stack_arg(1)))}"), 8)
        bind(0x004D99E0, self.on_alloc, 8)
        bind(0x008ADD50, self.on_grow, 8)
        bind(0x008D82F0, lambda emu: self.take("root"), 4)
        bind(0x00685080, lambda emu: self.take("at10"), 4)
        bind(0x006858E0, lambda emu: self.take("stl"), 4)
        bind(0x005C3300, lambda emu: self.take("rsz:" + fmt(u32(emu.stack_arg(0)))), 4)
        bind(0x0071D4E0, lambda emu: self.take("rsz:" + fmt(u32(emu.stack_arg(0)))), 4)
        bind(0x0071D540, lambda emu: self.take("rsz:" + fmt(u32(emu.stack_arg(0)))), 4)
        bind(0x0071D250, lambda emu: self.take("sa"), 4)
        bind(0x00707C20, lambda emu: self.take("sb"), 4)
        bind(0x0071D2F0, lambda emu: self.take("sc"), 4)
        bind(0x00681220, lambda emu: self.take("pwr"), 4)
        bind(0x007010D0, lambda emu: self.take("stop"), 0)
        bind(0x006E1890, lambda emu: self.take("clr"), 0)
        bind(0x006E0450, lambda emu: self.take("act:" + fmt(u32(emu.stack_arg(0)))), 4)
        bind(0x00688810, lambda emu: self.forced("idle:", self.idle), 0)
        bind(0x00739E60, lambda emu: self.take("fire"), 8)
        bind(0x007DAE90, lambda emu: self.take("dep"), 4)
        bind(0x007321F0, lambda emu: self.take("abil"), 4)
        bind(0x0072E810, lambda emu: self.take("ai"), 4)
        bind(0x00722750, lambda emu: self.take("body"), 4)
        bind(0x0071D020, lambda emu: self.stamp("fld", u32(emu.stack_arg(1))), 0)
        bind(0x0054A090, lambda emu: self.stamp("pair", u32(emu.stack_arg(1))), 0)
        bind(0x006F3160, lambda emu: self.stamp("rad", u32(emu.stack_arg(1))), 0)

    def add(self, text: str) -> None:
        self.log.append(text)

    def take(self, name: str) -> int:
        self.seq += 1
        self.add(name)
        return self.seq * 0x10

    def forced(self, prefix: str, value: int) -> int:
        self.add(prefix + fmt(value))
        return value

    def stamp(self, name: str, field: int) -> int:
        self.seq += 1
        self.emu.write(field, p32(0xA1000000 + self.seq))
        self.add(f"{name}:{self.seq}")
        return self.seq * 0x10

    def is_version(self, address: int) -> bool:
        if address < 0x10000 or address >= 0xF0000000:
            return False
        blob = self.emu.read(address, 2)
        return blob[0] == 1 and blob[1] in (1, 2)

    def on_ptr(self, emu: Emulator, slot: int) -> int:
        arg = u32(emu.stack_arg(0))
        if slot == 0x18 and self.is_version(arg):
            blob = emu.read(arg, 2)
            if self.ver_out >= 0:
                emu.write(arg + 1, bytes([self.ver_out & 0xFF]))
            return self.take(f"v{slot:x}:{blob[0]:02x}{blob[1]:02x}")
        if slot == 0x6C and self.stream >= 0 and 0x10000 <= arg < 0xF0000000:
            emu.write(arg, p32(self.stream))
            self.stream = -1
        return self.take(f"v{slot:x}:{fmt(arg)}")

    def on_find(self, emu: Emulator) -> int:
        if self.weapon == 0 or self.find_miss:
            self.add("fw0")
            return 0
        self.add("fw")
        return self.weapon

    def on_find_alt(self, emu: Emulator) -> int:
        if self.weapon == 0:
            self.add("fa0")
            return 0
        self.add("fa")
        return self.weapon

    def on_place(self, emu: Emulator) -> int:
        pos = u32(emu.stack_arg(0))
        bits = u32(emu.stack_arg(1))
        self.fbits = bits
        return self.take(f"place:{fmt(pos)}:{hex8(bits)}")

    def on_steer(self, emu: Emulator) -> int:
        pos = u32(emu.stack_arg(0))
        bits = u32(emu.stack_arg(1))
        mode = u32(emu.stack_arg(2))
        self.fbits = bits
        return self.take(f"steer:{fmt(pos)}:{hex8(bits)}:{fmt(mode)}")

    def on_alloc(self, emu: Emulator) -> int:
        self.add("alloc")
        return self.alloc(0x20)

    def on_grow(self, emu: Emulator) -> int:
        vec = u32(emu.register(x86_const.UC_X86_REG_ECX))
        end = u32(emu.stack_arg(0))
        ident = self.get(u32(emu.stack_arg(1)))
        if end:
            self.put(end, ident)
        self.put(vec + 4, end + 4)
        return self.take("grow:" + fmt(ident))

    def reset(self) -> None:
        self.emu.reset_data()
        self.log = []
        self.seq = 0
        self.usable = 1
        self.cur = 0
        self.query = 0
        self.d2120 = 0
        self.a18a0 = 0
        self.ready = 1
        self.kind = 0
        self.idle = 0
        self.ver_out = -1
        self.stream = -1
        self.find_miss = False
        self.weapon = 0
        self.fbits = 0
        self.emu.uc.reg_write(x86_const.UC_X86_REG_FPCW, 0x037F)

    def alloc(self, size: int) -> int:
        return self.emu.alloc(b"\x00" * size, alignment=256)

    def put(self, address: int, value: int) -> None:
        self.emu.write(address, p32(value))

    def get(self, address: int) -> int:
        return struct.unpack("<I", self.emu.read(address, 4))[0]

    def _plant(self, raw: bytearray) -> int:
        return self.emu.alloc(bytes(raw), alignment=256)

    def make_ai(self):
        self_ = self.alloc(0x800)
        obj = self.alloc(0x500)
        loco = self.alloc(0x80)
        templ = self.alloc(0x120)
        weapon = self.alloc(0x40)
        pose = self.alloc(0x40)
        loco_vt = self._plant(self._vt_bytes([0x1C, 0x34, 0x38], 0x40))
        self.put(loco, loco_vt)
        self.put(self_ + 8, obj)
        self.put(self_ + 4, templ)
        self.put(self_ + 0x3C, loco)
        self.put(self_ + 0x214, pose)
        self.put(obj + 4, templ)
        self.put(obj + 0x37C, weapon)
        self.put(self_ + 0x50, 0x99)
        self.put(self_ + 0x190, 0x55)
        self.put(self_ + 0x308, 0x77)
        self.put(self_ + 0x368, 0x77)
        self.put(self_ + 0x3C8, 0x77)
        self.emu.write(self_ + 0x432, b"\x07")
        self.put(obj + 0x458, 0x99)
        self.put(obj + 0x38, 0x111)
        self.put(obj + 0x3C, 0x222)
        self.put(obj + 0x40, 0x333)
        self.put(templ + 0x104, 0xFFFFFFFF)
        self.weapon = weapon
        bag = type("Ai", (), {})()
        bag.self = self_
        bag.obj = obj
        bag.loco = loco
        bag.templ = templ
        bag.weapon = weapon
        bag.pose = pose
        bag.hints = Hints()
        bag.hints.weapon = weapon
        bag.hints.templ = templ
        bag.hints.object = obj
        return bag

    def _vt_bytes(self, slots: list[int], size: int) -> bytearray:
        raw = bytearray(size)
        for slot in slots:
            struct.pack_into("<I", raw, slot, self.vt[slot])
        return raw

    def make_xfer(self, flags: int) -> int:
        vt = self._plant(self._vt_bytes([0x18, 0x58, 0x64, 0x6C, 0x7C, 0x84], 0x90))
        xfer = self.alloc(0x20)
        self.put(xfer, vt)
        self.put(xfer + 4, flags)
        return xfer

    def ai_out(self, ai, eax: int, mem: list[int]) -> dict[str, str]:
        shown, tag = classify(eax, ai.hints)
        extra = (
            f"tag={tag} b432={hex8(self.emu.read(ai.self + 0x432, 1)[0])} "
            f"t458={hex8(self.get(ai.obj + 0x458))} "
            f"p0={hex8(self.get(ai.pose + 0xC))} p1={hex8(self.get(ai.pose + 0x10))} "
            f"p2={hex8(self.get(ai.pose + 0x14))} v44={hex8(self.emu.read(ai.loco + 0x44, 1)[0])} "
            f"v0={hex8(self.get(ai.loco + 0x48))} v1={hex8(self.get(ai.loco + 0x4C))} "
            f"v2={hex8(self.get(ai.loco + 0x50))} w24={hex8(self.get(ai.weapon + 0x24))} "
            f"fbits={hex8(self.fbits)}"
        )
        return self.tokens(ai.self, mem, shown, extra)

    def tokens(self, obj: int, offsets: list[int], eax: int, extra: str) -> dict[str, str]:
        if offsets:
            mem = ",".join(f"{off:x}:{self.get(obj + off):08x}" for off in offsets)
        else:
            mem = "."
        out = {
            "eax": hex8(eax),
            "log": ",".join(self.log) if self.log else ".",
            "mem": mem,
        }
        for token in extra.split():
            key, value = token.split("=", 1)
            out[key] = value
        return out

    def call(self, address: int, stack: tuple[int, ...] = (), ecx: int = 0) -> int:
        self.emu.uc.reg_write(x86_const.UC_X86_REG_FPCW, 0x037F)
        return u32(self.emu.call(address, stack, ecx=ecx).eax)


def compare(name: str, got: dict[str, str], exp: dict[str, str]) -> str | None:
    keys = ["eax", "log", "mem"]
    keys.extend(key for key in exp if key not in keys)
    for key in keys:
        if got.get(key) != exp.get(key):
            return f"{name} {key}\n  cpp {got.get(key)}\n  emu {exp.get(key)}"
    return None


def main() -> int:
    checker = Checker(EXE)
    oracle = Oracle()
    failures: list[str] = []
    count = 0

    def run(command: str, build) -> None:
        nonlocal count
        count += 1
        got = checker.call(command)
        oracle.reset()
        exp = build()
        mismatch = compare(command, got, exp)
        if mismatch:
            failures.append(mismatch)

    def ai():
        return oracle.make_ai()

    def aim(cmd: str, addr: int, target: int, mode: int, usable: int | None = None, walk: bool = False):
        def build(addr=addr, target=target, mode=mode, usable=usable, walk=walk):
            box = ai()
            if usable is not None:
                oracle.usable = usable
            if walk:
                oracle.put(box.templ + 0xD4, 1 << 21)
                oracle.put(box.obj + 0x328, 1)
                target = oracle.alloc(0x30)
            eax = oracle.call(addr, (target, mode), ecx=box.self)
            return oracle.ai_out(box, eax, AI_MEM)
        run(cmd, build)

    aim("aim2 0x20 0", 0x0072D740, 0x20, 0)
    aim("aim2 0x20 2", 0x0072D740, 0x20, 2)
    aim("aim2u", 0x0072D740, 0x20, 0, usable=0)
    aim("aim4 0x20 1", 0x0072D7A0, 0x20, 1)
    aim("aim4 0x21 2", 0x0072D7A0, 0x21, 2)
    aim("aim5 0x20 0", 0x0072D6D0, 0x20, 0)
    aim("aim5u", 0x0072D6D0, 0x20, 0, usable=0x100)
    aim("aim3 0x20 0", 0x0072D620, 0x20, 0)
    aim("aim3w", 0x0072D620, 0x20, 0, walk=True)

    def move(cmd: str, addr: int, pos: int, mode: int, usable: int | None = None):
        def build(addr=addr, pos=pos, mode=mode, usable=usable):
            box = ai()
            if usable is not None:
                oracle.usable = usable
            eax = oracle.call(addr, (pos, mode), ecx=box.self)
            return oracle.ai_out(box, eax, AI_MEM)
        run(cmd, build)

    move("move1e 0x1000 0", 0x0072D3C0, 0x1000, 0)
    move("move1e 0x1000 2", 0x0072D3C0, 0x1000, 2)
    move("move1eu", 0x0072D3C0, 0x1000, 0, usable=0)
    move("move35 0x1000 1", 0x0072D460, 0x1000, 1)
    move("move1c 0x1000 0", 0x0072D260, 0x1000, 0)
    move("move1c 0x1000 2", 0x0072D260, 0x1000, 2)
    move("move1d 0x1000 0", 0x0072D310, 0x1000, 0)

    def build_vec(use_vec: int, bits: int, mode: int):
        box = ai()
        vec = 0
        if use_vec:
            vec = oracle.alloc(12)
            oracle.put(vec, 0x44)
            oracle.put(vec + 4, 0x55)
            oracle.put(vec + 8, 0x66)
        eax = oracle.call(0x0072CFD0, (0x1000, vec, bits, mode), ecx=box.self)
        return oracle.ai_out(box, eax, AI_MEM)

    run("movevec 0x1000 0 0x3f800000 2", lambda: build_vec(0, 0x3F800000, 2))
    run("movevec 0x1000 1 0x3f800000 0", lambda: build_vec(1, 0x3F800000, 0))

    def build_moveto(byte: int, usable: int, target: int, mode: int):
        box = ai()
        if byte:
            oracle.emu.write(box.self + 0x436, bytes([byte]))
        oracle.usable = usable
        eax = oracle.call(0x0072D1A0, (target, mode), ecx=box.self)
        return oracle.ai_out(box, eax, AI_MEM)

    run("moveto 0x40 0", lambda: build_moveto(0, 1, 0x40, 0))
    run("movetoB", lambda: build_moveto(1, 1, 0x40, 0))
    run("movetoU", lambda: build_moveto(0, 0, 0x40, 0))

    def build_face(target: int, mode: int):
        box = ai()
        eax = oracle.call(0x0072D500, (target, mode), ecx=box.self)
        return oracle.ai_out(box, eax, AI_MEM)

    run("face 0x40 0", lambda: build_face(0x40, 0))
    run("face 0x40 2", lambda: build_face(0x40, 2))

    def build_cmd(cur: int, byte: int, target: int, mode: int):
        box = ai()
        oracle.query = 0x21
        oracle.cur = cur
        if byte:
            oracle.emu.write(box.self + 0x436, bytes([byte]))
        eax = oracle.call(0x0072D590, (target, mode), ecx=box.self)
        return oracle.ai_out(box, eax, AI_MEM)

    run("cmd2f 0x40 0", lambda: build_cmd(0, 0, 0x40, 0))
    run("cmd2f 0x40 2", lambda: build_cmd(0, 0, 0x40, 2))
    run("cmd2fC", lambda: build_cmd(0x55, 0, 0x40, 0))
    run("cmd2fB", lambda: build_cmd(0, 1, 0x40, 0))

    def build_ord2c(kind: str, target: int, mode: int):
        box = ai()
        if kind == "null":
            oracle.put(box.obj + 0x37C, 0)
        if kind == "miss":
            oracle.find_miss = True
        eax = oracle.call(0x0072DDB0, (target, mode), ecx=box.self)
        return oracle.ai_out(box, eax, AI_MEM)

    run("ord2c 0x30 0", lambda: build_ord2c("ok", 0x30, 0))
    run("ord2c 0x30 2", lambda: build_ord2c("ok", 0x30, 2))
    run("ord2c0", lambda: build_ord2c("ok", 0, 0))
    run("ord2cN 0x30 2", lambda: build_ord2c("null", 0x30, 2))
    run("ord2cM 0x30 2", lambda: build_ord2c("miss", 0x30, 2))

    def build_ord0b(target: int, aux: int, mode: int):
        box = ai()
        eax = oracle.call(0x0072DE40, (target, aux, mode), ecx=box.self)
        return oracle.ai_out(box, eax, AI_MEM)

    run("ord0b 0x30 7 0", lambda: build_ord0b(0x30, 7, 0))
    run("ord0b 0x30 7 2", lambda: build_ord0b(0x30, 7, 2))
    run("ord0b0", lambda: build_ord0b(0, 0, 0))

    def build_ord16(key: int, aux: int, mode: int, query: int):
        box = ai()
        oracle.query = query
        eax = oracle.call(0x0072DEC0, (key, aux, mode), ecx=box.self)
        return oracle.ai_out(box, eax, AI_MEM)

    run("ord16 0x12 9 0 0x21", lambda: build_ord16(0x12, 9, 0, 0x21))
    run("ord16 0x12 9 2 0x21", lambda: build_ord16(0x12, 9, 2, 0x21))
    run("ord16 0x12 9 0 0", lambda: build_ord16(0x12, 9, 0, 0))

    def build_aimf(byte: int, target: int, aux: int, flag: int, mode: int):
        box = ai()
        if byte:
            oracle.emu.write(box.self + 0x436, bytes([1]))
        eax = oracle.call(0x0072E560, (target, aux, flag, mode), ecx=box.self)
        return oracle.ai_out(box, eax, AI_MEM)

    run("aimf 0x20 4 0 0", lambda: build_aimf(0, 0x20, 4, 0, 0))
    run("aimf 0x20 4 1 2", lambda: build_aimf(0, 0x20, 4, 1, 2))
    run("aimfB", lambda: build_aimf(1, 0x20, 4, 0, 0))

    def build_guard(kind: str, point: int, mode: int):
        box = ai()
        if kind == "bit":
            oracle.put(box.templ + 0xC0, 1 << 26)
        if kind == "usable":
            oracle.usable = 0
        eax = oracle.call(0x0072E600, (point, mode), ecx=box.self)
        return oracle.ai_out(box, eax, AI_MEM)

    run("guard 0x30 0", lambda: build_guard("ok", 0x30, 0))
    run("guard 0x30 2", lambda: build_guard("ok", 0x30, 2))
    run("guardB", lambda: build_guard("bit", 0x30, 0))
    run("guardU", lambda: build_guard("usable", 0x30, 0))

    def build_att(arg: int, mode: int, path: int):
        box = ai()
        if path == 1:
            draw = oracle.alloc(0x480)
            nested = oracle.alloc(0x10)
            oracle.put(draw + 0x47C, nested)
            oracle.put(box.obj + 0x374, draw)
            oracle.d2120 = 0x61
        elif path == 2:
            oracle.a18a0 = 0x62
        elif path == 3:
            oracle.emu.write(box.obj + 0x471, b"\x01")
        elif path == 4:
            draw = oracle.alloc(0x480)
            oracle.put(box.obj + 0x374, draw)
            oracle.a18a0 = 0x62
        eax = oracle.call(0x0072E680, (arg, mode), ecx=box.self)
        return oracle.ai_out(box, eax, AI_MEM)

    run("att 0x15 0 0", lambda: build_att(0x15, 0, 0))
    run("att 0x15 0 1", lambda: build_att(0x15, 0, 1))
    run("att 0x15 0 2", lambda: build_att(0x15, 0, 2))
    run("att 0x15 2 3", lambda: build_att(0x15, 2, 3))
    run("att 0x15 0 4", lambda: build_att(0x15, 0, 4))

    def stance_n(self_addr: int) -> str:
        begin = oracle.get(self_addr + 0xE0)
        end = oracle.get(self_addr + 0xE4)
        count = 0
        first = 0
        endv = end if end < 0x10000 else 0
        if begin >= 0x10000 and end >= begin and (end - begin) % 4 == 0 and (end - begin) // 4 <= 8:
            count = (end - begin) // 4
            if count:
                first = oracle.get(begin)
        return f" n={hex8(count)} e0={hex8(first)} endv={hex8(endv)}"

    def build_stance(kind: str, ident: int):
        slab = oracle.alloc(0x200)
        self_ = slab + 8
        obj = oracle.alloc(0x100)
        oracle.put(slab, obj)
        hints = Hints()
        hints.object = obj
        if kind == "same":
            oracle.put(obj + 0xDC, ident)
        elif kind in ("found", "app", "grow"):
            buf = oracle.alloc(0x20)
            oracle.put(buf, ident if kind == "found" else 0x11)
            oracle.put(self_ + 0xE0, buf)
            end = buf + 4 if kind in ("found", "app") else buf
            cap = buf if kind == "grow" else buf + 16
            oracle.put(self_ + 0xE4, end)
            oracle.put(self_ + 0xE8, cap)
            hints.begin = buf
            hints.end = end
            hints.old_end = end
        eax = oracle.call(0x00722660, (ident,), ecx=self_)
        shown, tag = classify(eax, hints)
        return oracle.tokens(self_, [], shown, "tag=" + tag + stance_n(self_))

    run("stSame 0x2a", lambda: build_stance("same", 0x2A))
    run("stFound 0x2a", lambda: build_stance("found", 0x2A))
    run("stApp 0x2b", lambda: build_stance("app", 0x2B))
    run("stZero 0x2c", lambda: build_stance("zero", 0x2C))
    run("stGrow 0x2d", lambda: build_stance("grow", 0x2D))

    def build_link(kind: str, ident: int, old: int):
        slab = oracle.alloc(0x200)
        self_ = slab + 8
        obj = oracle.alloc(0x100)
        oracle.put(slab, obj)
        head = self_ + 0xD4
        oracle.put(head, head)
        oracle.put(head + 4, head)
        hints = Hints()
        hints.object = obj
        if kind == "hit":
            node = oracle.alloc(0x20)
            oracle.put(node, head)
            oracle.put(node + 4, head)
            oracle.put(node + 8, ident)
            oracle.put(head, node)
            oracle.put(head + 4, node)
            hints.planted = node
        else:
            oracle.put(obj + 0x94, old)
        eax = oracle.call(0x007226C0, (ident,), ecx=self_)
        shown, tag = classify(eax, hints)
        return oracle.tokens(self_, [], shown, f"tag={tag} fl={hex8(oracle.get(obj + 0x94))}")

    run("stHit 0x2e", lambda: build_link("hit", 0x2E, 0))
    run("stNew 0x2e 0", lambda: build_link("new", 0x2E, 0))
    run("stNew 0x2e 0x4000", lambda: build_link("new", 0x2E, 0x4000))

    def build_wf(bit: int):
        box = ai()
        if bit:
            oracle.put(box.obj + 0xBC, 0x100)
        eax = oracle.call(0x0073F5C0, (), ecx=box.self)
        return oracle.ai_out(box, eax, AI_MEM)

    run("wfSet", lambda: build_wf(1))
    run("wfClr", lambda: build_wf(0))

    def build_arm(key: int):
        box = ai()
        oracle.put(box.templ + 0x104, key)
        eax = oracle.call(0x0073F590, (0,), ecx=box.self)
        return oracle.ai_out(box, eax, AI_MEM)

    run("wfArm 0x20", lambda: build_arm(0x20))
    run("wfArm 0xffffffff", lambda: build_arm(0xFFFFFFFF))

    def build_mount(has: int, bits: int):
        box = ai()
        mounted = 0
        if has:
            mounted = oracle.alloc(0x1C0)
            oracle.put(box.obj + 0xF4, mounted)
        oracle.put(box.obj + 0x88, bits)
        eax = oracle.call(0x0072A8D0, (1, 2), ecx=box.self)
        shown, tag = classify(eax, box.hints)
        ubits = oracle.get(mounted + 0x1BC) if mounted else 0
        extra = f"tag={tag} m88={hex8(oracle.get(box.obj + 0x88))} ubits={hex8(ubits)}"
        return oracle.tokens(box.self, [0x88], shown, extra)

    run("mount 1 0x2000", lambda: build_mount(1, 0x2000))
    run("mount 0 0", lambda: build_mount(0, 0))

    def build_beam(flag: int):
        box = ai()
        oracle.emu.write(box.self + 0xFA, bytes([flag]))
        oracle.put(box.self + 0xFC, 0x99)
        oracle.put(box.self + 0x100, 0x99)
        oracle.put(box.self + 0x104, 0x99)
        oracle.put(box.self + 0x108, 0x99)
        if flag:
            draw = oracle.alloc(0x40)
            vt = oracle._plant(oracle._vt_bytes([0x288], 0x290))
            oracle.put(draw, vt)
            oracle.put(box.obj + 0x374, draw)
        eax = oracle.call(0x007289D0, (1, 2), ecx=box.self)
        shown, tag = classify(eax, box.hints)
        extra = f"tag={tag} fa={hex8(oracle.emu.read(box.self + 0xFA, 1)[0])} b100={hex8(oracle.get(box.self + 0x100))}"
        return oracle.tokens(box.self, [0xFC, 0x104, 0x108], shown, extra)

    run("beam 0", lambda: build_beam(0))
    run("beam 1", lambda: build_beam(1))

    def build_pair(addr: int, a: int, b: int):
        box = ai()
        eax = oracle.call(addr, (a, b), ecx=box.self)
        return oracle.ai_out(box, eax, AI_MEM)

    run("act 1 2", lambda: build_pair(0x007287E0, 1, 2))
    run("act 0 0", lambda: build_pair(0x007287E0, 0, 0))
    run("giveC 3 4", lambda: build_pair(0x007295C0, 3, 4))
    run("giveC 0 1", lambda: build_pair(0x007295C0, 0, 1))
    run("ocl 5 6", lambda: build_pair(0x00730D90, 5, 6))
    run("ocl 0 0", lambda: build_pair(0x00730D90, 0, 0))

    def build_xfer(kind: str, flags: int, count: int = 0):
        self_ = oracle.alloc(0x800)
        ver = kind in ("shield", "store") and flags == 1
        if ver:
            oracle.ver_out = 1
        xfer_flags = 0 if kind in ("shield", "store") else flags
        xfer = oracle.make_xfer(xfer_flags)
        hints = Hints()
        mem: list[int] = []
        extra = ""
        if kind == "shield":
            eax = oracle.call(0x00731900, (xfer,), ecx=self_)
            mem = [8]
        elif kind == "radar":
            eax = oracle.call(0x0073EE80, (xfer,), ecx=self_)
            mem = [0xC8, 0xD8]
        elif kind == "cap":
            eax = oracle.call(0x0073E780, (xfer,), ecx=self_)
        elif kind == "comp":
            eax = oracle.call(0x007284C0, (xfer,), ecx=self_)
            mem = [0x18]
        elif kind == "store":
            oracle.put(self_ + 0x70, 0x99)
            eax = oracle.call(0x0072A580, (xfer,), ecx=self_)
            mem = [0x50, 0x70]
        elif kind == "givex":
            eax = oracle.call(0x0073ED30, (xfer,), ecx=self_)
        elif kind == "deploy":
            eax = oracle.call(0x0073FAA0, (xfer,), ecx=self_)
        elif kind == "horde":
            eax = oracle.call(0x0073FFF0, (xfer,), ecx=self_)
        elif kind == "body":
            eax = oracle.call(0x00722970, (xfer,), ecx=self_)
            mem = [0x124]
        elif kind in ("spawn", "strafe", "subst"):
            begin_off = {"spawn": 0x34, "strafe": 0x14, "subst": 0x10}[kind]
            if count:
                buf = oracle.alloc(0x10)
                oracle.put(buf, 0x22)
                oracle.put(buf + 4, 0x33)
                oracle.put(self_ + begin_off, buf)
                oracle.put(self_ + begin_off + 4, buf + 8)
            addr = {"spawn": 0x00731B70, "strafe": 0x007216E0, "subst": 0x00721820}[kind]
            eax = oracle.call(addr, (xfer,), ecx=self_)
            begin = oracle.get(self_ + begin_off)
            el0 = oracle.get(begin) if begin >= 0x10000 else 0
            shown, tag = classify(eax, hints)
            return oracle.tokens(self_, [], shown, f"tag={tag} el0={hex8(el0)}")
        else:
            head = self_ + 0x2C
            oracle.put(head, head)
            oracle.put(head + 4, head)
            if flags & 1:
                node = oracle.alloc(0x20)
                oracle.put(node, head)
                oracle.put(node + 4, head)
                oracle.put(node + 8, 0x42)
                oracle.put(head, node)
                oracle.put(head + 4, node)
            else:
                oracle.stream = count
            eax = oracle.call(0x0072FA80, (xfer,), ecx=self_)
            nlink = 0
            node = oracle.get(head)
            while node != head and nlink < 8:
                nlink += 1
                node = oracle.get(node)
            val = oracle.get(oracle.get(head + 4) + 8) if nlink else 0
            shown, tag = classify(eax, hints)
            return oracle.tokens(self_, [], shown, f"tag={tag} val={hex8(val)} nlink={hex8(nlink)}")
        shown, tag = classify(eax, hints)
        return oracle.tokens(self_, mem, shown, f"tag={tag}{extra}")

    run("shield 0", lambda: build_xfer("shield", 0))
    run("shield 1", lambda: build_xfer("shield", 1))
    run("radar 0", lambda: build_xfer("radar", 0))
    run("radar 1", lambda: build_xfer("radar", 1))
    run("cap 0", lambda: build_xfer("cap", 0))
    run("cap 1", lambda: build_xfer("cap", 1))
    run("comp 0", lambda: build_xfer("comp", 0))
    run("comp 1", lambda: build_xfer("comp", 1))
    run("store 0", lambda: build_xfer("store", 0))
    run("store 1", lambda: build_xfer("store", 1))
    run("givex 8", lambda: build_xfer("givex", 8))
    run("givex 0", lambda: build_xfer("givex", 0))
    run("deploy 8", lambda: build_xfer("deploy", 8))
    run("deploy 0", lambda: build_xfer("deploy", 0))
    run("horde 0", lambda: build_xfer("horde", 0))
    run("horde 1", lambda: build_xfer("horde", 1))
    run("body 0", lambda: build_xfer("body", 0))
    run("body 1", lambda: build_xfer("body", 1))
    run("spawn 0 0", lambda: build_xfer("spawn", 0, 0))
    run("spawn 0 1", lambda: build_xfer("spawn", 0, 1))
    run("spawn 2 0", lambda: build_xfer("spawn", 2, 0))
    run("strafe 0 0", lambda: build_xfer("strafe", 0, 0))
    run("strafe 0 1", lambda: build_xfer("strafe", 0, 1))
    run("strafe 2 1", lambda: build_xfer("strafe", 2, 1))
    run("subst 0 0", lambda: build_xfer("subst", 0, 0))
    run("subst 0 1", lambda: build_xfer("subst", 0, 1))
    run("subst 2 1", lambda: build_xfer("subst", 2, 1))
    run("stealth 8 0", lambda: build_xfer("stealth", 8, 0))
    run("stealth 1 0", lambda: build_xfer("stealth", 1, 0))
    run("stealth 0 0", lambda: build_xfer("stealth", 0, 0))
    run("stealth 0 1", lambda: build_xfer("stealth", 0, 1))

    def build_dmg(kind: str):
        self_ = oracle.alloc(0x80)
        obj = oracle.alloc(0x400)
        templ = oracle.alloc(0x40)
        damage = oracle.alloc(0xA0)
        selector = oracle.alloc(0x10)
        ready_vt = oracle._plant(oracle._vt_bytes([0], 0x10))
        kind_vt = oracle._plant(oracle._vt_bytes([0x54], 0x60))
        oracle.put(self_ + 0x10, ready_vt)
        oracle.put(selector, kind_vt)
        oracle.put(self_ + 8, obj)
        oracle.put(self_ + 4, templ)
        oracle.put(obj + 0x33C, selector)
        oracle.put(templ + 0x24, 1)
        oracle.put(templ + 0x28, 0x3F800000)
        oracle.put(damage + 0x8C, 0x40000000)
        hints = Hints()
        hints.templ = templ
        hints.object = obj
        if kind == "zero":
            oracle.ready = 0
        elif kind == "hi":
            oracle.ready = 0x100
        elif kind == "mask":
            oracle.put(templ + 0x24, 0)
        elif kind == "above":
            oracle.put(templ + 0x28, 0x40000000)
            oracle.put(damage + 0x8C, 0x3F800000)
        elif kind == "nan":
            oracle.put(templ + 0x28, 0x7FC00001)
        elif kind == "idle":
            oracle.kind = 1
            oracle.idle = 5
            oracle.put(self_ + 0x30, oracle.alloc(0x10))
        elif kind == "fire":
            oracle.kind = 2
            oracle.put(self_ + 0x34, oracle.alloc(0x10))
        elif kind == "k3":
            oracle.kind = 3
            oracle.put(self_ + 0x38, oracle.alloc(0x10))
        elif kind == "default":
            oracle.kind = 4
        elif kind == "go":
            oracle.kind = 4
            oracle.put(self_ + 0x2C, oracle.alloc(0x10))
        eax = oracle.call(0x0073B200, (damage,), ecx=self_)
        shown, tag = classify(eax, hints)
        return oracle.tokens(self_, [], shown, f"tag={tag}")

    run("dmg0", lambda: build_dmg("zero"))
    run("dmgHi", lambda: build_dmg("hi"))
    run("dmgMask", lambda: build_dmg("mask"))
    run("dmgAbove", lambda: build_dmg("above"))
    run("dmgNan", lambda: build_dmg("nan"))
    run("dmgIdle", lambda: build_dmg("idle"))
    run("dmgFire", lambda: build_dmg("fire"))
    run("dmgK3", lambda: build_dmg("k3"))
    run("dmgDef", lambda: build_dmg("default"))
    run("dmgGo", lambda: build_dmg("go"))

    checker.close()
    print(f"cases={count} mismatches={len(failures)}")
    for failure in failures[:40]:
        print(failure)
    return 1 if failures else 0


if __name__ == "__main__":
    sys.exit(main())
