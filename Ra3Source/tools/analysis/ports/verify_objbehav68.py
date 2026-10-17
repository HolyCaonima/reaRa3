"""Differential oracle for the 0x0068 behavior-module batch.

The Release checker prints eax/log/mem tokens. Unicorn runs the shipped bytes
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

EXE = Path(__file__).resolve().parents[3] / "build" / "win32" / "bin" / "Release" / "ra3_behaviormodules68check.exe"
SENTINEL = 0xBF800000


def u32(value: int) -> int:
    return value & 0xFFFFFFFF


def p32(value: int) -> bytes:
    return struct.pack("<I", u32(value))


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
    def __init__(self, checker: Checker) -> None:
        self.checker = checker
        self.emu = Emulator()
        self.log: list[str] = []
        self.seq = 0
        self.clear = False
        self.imm = 0
        self.nret: dict[int, int] = {}
        self._bind()

    def _bind(self) -> None:
        def one(slot: int):
            def stub(emu: Emulator, slot: int = slot) -> int:
                return self.on_one(emu, slot)
            return stub

        def none(slot: int):
            def stub(emu: Emulator, slot: int = slot) -> int:
                self.add(f"n{slot:x}")
                return self.nret.get(slot, 0)
            return stub

        self.fn = {}
        for slot, cleanup in (
            (0x04, 0), (0x14, 0), (0x24, 0), (0x2C, 0), (0x30, 0), (0x34, 0),
            (0x58, 0), (0x5C, 0), (0x60, 0), (0x9C, 0), (0x274, 0), (0x2A4, 0),
        ):
            self.fn[("n", slot)] = self.emu.bind_stub(none(slot), cleanup)
        for slot in (0x08, 0x18, 0x58, 0x60, 0x64, 0x6C, 0x70, 0x84):
            self.fn[("p", slot)] = self.emu.bind_stub(one(slot), 4)
        for slot in (0x1C, 0x20, 0x24, 0x28, 0x6C):
            self.fn[("u", slot)] = self.emu.bind_stub(one(slot), 4)
        self.fn[("t", 0x88)] = self.emu.bind_stub(self.on_three, 12)
        self.fn[("k", 0x10)] = self.emu.bind_stub(self.on_key, 0)
        self.emu.bind_function(0x008D82F0, lambda emu: self.stamp(emu, "h8d82"), 4)
        self.emu.bind_function(0x00685080, lambda emu: self.stamp(emu, "h8508"), 4)
        self.emu.bind_function(0x00680850, lambda emu: self.stamp(emu, "h0850"), 4)
        self.emu.bind_function(0x0052DDE0, self.on_dock, 8)
        self.emu.bind_function(0x007F8BC0, self.on_energy, 4)
        self.emu.bind_function(0x0054A090, lambda emu: self.on_cdecl(emu, "c4a09"), 0)
        self.emu.bind_function(0x00986340, lambda emu: self.on_cdecl(emu, "c6340"), 0)

    def add(self, text: str) -> None:
        self.log.append(text)

    def reset(self) -> None:
        self.emu.reset_data()
        self.log = []
        self.seq = 0
        self.clear = False
        self.imm = 0
        self.nret = {}
        self.emu.uc.reg_write(x86_const.UC_X86_REG_FPCW, 0x037F)

    def alloc(self, size: int) -> int:
        return self.emu.alloc(b"\x11" * size)

    def put(self, address: int, value: int) -> None:
        self.emu.write(address, p32(value))

    def get(self, address: int) -> int:
        return struct.unpack("<I", self.emu.read(address, 4))[0]

    def vt(self, entries: dict[int, tuple[str, int]], size: int = 0x300) -> int:
        raw = bytearray(b"\x11" * size)
        for slot, key in entries.items():
            struct.pack_into("<I", raw, slot, self.fn[key])
        return self.emu.alloc(bytes(raw))

    def on_one(self, emu: Emulator, slot: int) -> int:
        arg = u32(emu.stack_arg(0))
        if arg < 0x10000:
            self.add(f"i{slot:x}:{arg:x}")
            return self.imm
        blob = emu.read(arg, 2)
        if blob[0] == 1 and blob[1] in (1, 2):
            self.add(f"m{slot:x}:{blob[0]:02x}{blob[1]:02x}")
            if self.clear and blob[1] == 2:
                emu.write(arg + 1, b"\x00")
            return 0
        self.seq += 1
        emu.write(arg, p32(0xA1000000 + self.seq))
        self.add(f"f{slot:x}:{self.seq}")
        return 0

    def on_three(self, emu: Emulator) -> int:
        first = u32(emu.stack_arg(0))
        field = u32(emu.stack_arg(1))
        count = u32(emu.stack_arg(2))
        self.seq += 1
        emu.write(field, p32(0xA1000000 + self.seq))
        self.add(f"e88:{first:x}:{count:x}:{self.seq}")
        return 0

    def on_key(self, emu: Emulator) -> int:
        self.add("n10")
        ecx = u32(emu.register(x86_const.UC_X86_REG_ECX))
        return self.get(ecx + 4)

    def stamp(self, emu: Emulator, name: str) -> int:
        ecx = u32(emu.register(x86_const.UC_X86_REG_ECX))
        self.seq += 1
        emu.write(ecx + 0x80, p32(0xD2000000 + self.seq))
        self.add(f"{name}:{self.seq}")
        return 0

    def on_dock(self, emu: Emulator) -> int:
        ecx = u32(emu.register(x86_const.UC_X86_REG_ECX))
        first = u32(emu.stack_arg(0))
        second = u32(emu.stack_arg(1))
        emu.write(ecx + 0x80, p32(first))
        emu.write(ecx + 0x84, p32(second))
        self.add(f"h2dde:{first:x}:{second:x}")
        return 0

    def on_energy(self, emu: Emulator) -> int:
        ecx = u32(emu.register(x86_const.UC_X86_REG_ECX))
        ident = u32(emu.stack_arg(0))
        emu.write(ecx + 0x80, p32(ident))
        self.add(f"hf8b:{ident:x}")
        return 0

    def on_cdecl(self, emu: Emulator, name: str) -> int:
        field = u32(emu.stack_arg(1))
        self.seq += 1
        emu.write(field, p32(0xA1000000 + self.seq))
        self.add(f"{name}:{self.seq}")
        return 0

    def tokens(self, obj: int, offsets: list[int], eax: int, extra: dict[str, str] | None = None) -> dict[str, str]:
        if offsets:
            mem = ",".join(f"{off:x}:{self.get(obj + off):08x}" for off in offsets)
        else:
            mem = "."
        out = {
            "eax": f"{u32(eax):08x}",
            "log": ",".join(self.log) if self.log else ".",
            "mem": mem,
        }
        if extra:
            out.update(extra)
        return out

    def finish(self, address: int, obj: int, stack: tuple[int, ...] = (), offsets: list[int] | None = None,
               cleanup_eax: int | None = None, extra: dict[str, str] | None = None) -> dict[str, str]:
        result = self.emu.call(address, stack, ecx=obj)
        eax = result.eax if cleanup_eax is None else cleanup_eax
        return self.tokens(obj, offsets or [], eax, extra)


def offsets_of(command: str) -> list[int]:
    parts = command.split()
    if "mem" not in parts:
        return []
    index = parts.index("mem")
    return [int(token, 0) for token in parts[index + 1:]]


def compare(name: str, got: dict[str, str], exp: dict[str, str], skip_eax: bool) -> str | None:
    keys = ["log", "mem"]
    if not skip_eax:
        keys.append("eax")
    for key in keys:
        if got.get(key) != exp.get(key):
            return f"{name} {key}\n  cpp {got.get(key)}\n  emu {exp.get(key)}"
    for key, value in exp.items():
        if key in ("eax", "log", "mem"):
            continue
        if got.get(key) != value:
            return f"{name} {key}\n  cpp {got.get(key)}\n  emu {value}"
    return None


def main() -> int:
    checker = Checker(EXE)
    oracle = Oracle(checker)
    failures = []
    count = 0

    def run(command: str, build, skip_eax: bool = False) -> None:
        nonlocal count
        count += 1
        got = checker.call(command)
        oracle.reset()
        exp = build()
        mismatch = compare(command, got, exp, skip_eax)
        if mismatch:
            failures.append(mismatch)

    def fbits(value: float) -> int:
        return struct.unpack("<I", struct.pack("<f", value))[0]

    # GarrisonContain
    for flag, old in ((0, 0x22), (1, 0x22), (0x9E, 0)):
        cmd = f"garrison {flag} {old} mem 0x148"

        def build(flag=flag, old=old, cmd=cmd):
            self_ = oracle.alloc(0x600)
            body = oracle.alloc(0x100)
            oracle.emu.write(body + 0x9E, bytes([flag]))
            oracle.put(self_ + 4, body)
            oracle.put(self_ + 0x148, old)
            result = oracle.emu.call(0x00680260, (0, 0), ecx=self_)
            mix = "1" if u32(result.eax) == body else "0"
            return oracle.tokens(self_, offsets_of(cmd), 0, {"mix": mix})
        run(cmd, build, skip_eax=True)

    # OpenContain
    for old in (0, 0xAB):
        cmd = f"opencontain {old} mem 0x110"

        def build(old=old, cmd=cmd):
            self_ = oracle.alloc(0x600)
            oracle.emu.write(self_ + 0x110, bytes([old]))
            return oracle.finish(0x00680690, self_, (0, 0), offsets_of(cmd))
        run(cmd, build)

    # TransportContain high bits follow the template pointer.
    for flag in (0, 0x7F, 0xFF):
        cmd = f"transport {flag}"

        def build(flag=flag):
            self_ = oracle.alloc(0x600)
            body = oracle.alloc(0x160)
            oracle.emu.write(body + 0x143, bytes([flag]))
            oracle.put(self_ + 4, body)
            result = oracle.emu.call(0x00680730, (0,), ecx=self_)
            lo = result.eax & 0xFF
            expect = (body & 0xFFFFFF00) | lo
            return {
                "eax": f"{u32(result.eax):08x}",
                "log": ".",
                "mem": ".",
                "lo": f"{lo:08x}",
                "mix": "1" if result.eax == expect else "0",
            }
        run(cmd, build, skip_eax=True)

    # IdleState
    for al in (0, 1, 0x2A):
        cmd = f"idle {al}"

        def build(al=al):
            vt = oracle.vt({0x04: ("n", 0x04)}, 0x10)
            second = oracle.alloc(0x40)
            oracle.put(second + 0x24, vt)
            first = oracle.alloc(0x90)
            oracle.put(first + 0x80, second)
            self_ = oracle.alloc(0x600)
            oracle.put(self_ + 0x18, first)
            oracle.nret[0x04] = al
            return oracle.finish(0x00680B70, self_)
        run(cmd, build)

    # MoveInStraightLineState, both branches of the mode byte.
    for clear in (0, 1):
        cmd = f"line {clear} mem 0x28 0x34"

        def build(clear=clear, cmd=cmd):
            oracle.clear = clear != 0
            vt = oracle.vt({0x18: ("p", 0x18), 0x58: ("p", 0x58), 0x64: ("p", 0x64)}, 0x80)
            xfer = oracle.alloc(0x20)
            oracle.put(xfer, vt)
            self_ = oracle.alloc(0x600)
            return oracle.finish(0x00680BB0, self_, (xfer,), offsets_of(cmd))
        run(cmd, build)

    run("ground mem 0x28 0x2c", lambda: _xfer(oracle, 0x00680C10, {0x18: ("p", 0x18), 0x64: ("p", 0x64)}, [0x28, 0x2C]))

    for flag in (0, 1, 0x80):
        cmd = f"beam {flag}"

        def build(flag=flag):
            self_ = oracle.alloc(0x600)
            body = oracle.alloc(0x120)
            oracle.emu.write(body + 0x10D, bytes([flag]))
            oracle.put(self_ + 4, body)
            result = oracle.emu.call(0x00680DC0, (), ecx=self_)
            lo = result.eax & 0xFF
            expect = (body & 0xFFFFFF00) | lo
            return {"eax": f"{u32(result.eax):08x}", "log": ".", "mem": ".",
                    "lo": f"{lo:08x}", "mix": "1" if result.eax == expect else "0"}
        run(cmd, build, skip_eax=True)

    for bits in (0x3F800000, 0xBF800000, 0x7F800000, 0x00000000, 0x80000000, 0x7FC00000):
        cmd = f"radius 0x{bits:x}"

        def build(bits=bits):
            self_ = oracle.alloc(0x600)
            body = oracle.alloc(0x100)
            oracle.put(body + 0xF0, bits)
            oracle.put(self_ + 4, body)
            _result, raw = oracle.emu.call_float80(0x00680E70, (), ecx=self_)
            return {"eax": "00000000", "log": ".", "mem": ".", "st0": raw.hex()}
        run(cmd, build)

    # ToggleStatus entry and wrapped next entry.
    for index, count, which in ((0, 4, 0), (3, 4, 0), (3, 4, 1), (4, 5, 1), (0xFFFFFFFF, 5, 1), (0, 0, 1)):
        cmd = f"toggle {index} {count} {which}"

        def build(index=index, count=count, which=which):
            table = oracle.alloc(0x2000)
            body = oracle.alloc(0x100)
            oracle.put(body + 0xF0, count)
            oracle.put(body + 0xF4, table)
            self_ = oracle.alloc(0x600)
            oracle.put(self_ + 4, body)
            oracle.put(self_ + 0xBC, index)
            address = 0x00681670 if which == 0 else 0x00681690
            result = oracle.emu.call(address, (), ecx=self_)
            return {"eax": "00000000", "log": ".", "mem": ".", "off": f"{u32(result.eax - table):08x}"}
        run(cmd, build)

    run("dozer mem 0x20 0x24 0x28", lambda: _xfer(oracle, 0x00681B50, {
        0x18: ("p", 0x18), 0x6C: ("p", 0x6C), 0x70: ("p", 0x70), 0x84: ("p", 0x84)}, [0x20, 0x24, 0x28]))

    for present, first, second in ((0, 0x11, 0x22), (1, 0x11, 0x22), (1, 0, 0xFFFFFFFF)):
        cmd = f"dock {present} {first} {second}"

        def build(present=present, first=first, second=second):
            self_ = oracle.alloc(0x600)
            body = oracle.alloc(0x40)
            oracle.put(body + 0x1C, first)
            oracle.put(self_ + 4, body)
            oracle.put(self_ + 0x8C, second)
            update = oracle.alloc(0x100)
            dock = 0
            if present:
                dock = oracle.alloc(0x100)
                oracle.put(update + 0xF4, dock)
            else:
                oracle.put(update + 0xF4, 0)
            oracle.put(self_ + 8, update)
            oracle.emu.call(0x00682670, (), ecx=self_)
            if dock:
                extra = {"d80": f"{oracle.get(dock + 0x80):08x}", "d84": f"{oracle.get(dock + 0x84):08x}"}
            else:
                extra = {"d80": "00000000", "d84": "00000000"}
            return oracle.tokens(self_, [], 0, extra)
        run(cmd, build)

    cmd = "ensure 1 0x1234 mem 0x3c"

    def build_ensure_keep():
        vt = oracle.vt({0x2A4: ("n", 0x2A4)})
        self_ = oracle.alloc(0x600)
        oracle.put(self_, vt)
        oracle.put(self_ + 0x3C, 0x1234)
        result = oracle.emu.call(0x00682C70, (), ecx=self_)
        out = oracle.tokens(self_, [0x3C], result.eax, {"stored": "0"})
        return out
    run(cmd, build_ensure_keep)

    cmd = "ensure 0 0x77"

    def build_ensure_make():
        vt = oracle.vt({0x2A4: ("n", 0x2A4)})
        loco_vt = oracle.vt({0x14: ("n", 0x14)}, 0x20)
        loco = oracle.alloc(0x20)
        oracle.put(loco, loco_vt)
        oracle.nret[0x2A4] = loco
        oracle.nret[0x14] = 0x77
        self_ = oracle.alloc(0x600)
        oracle.put(self_, vt)
        oracle.put(self_ + 0x3C, 0)
        result = oracle.emu.call(0x00682C70, (), ecx=self_)
        stored = "1" if oracle.get(self_ + 0x3C) == loco else "0"
        return oracle.tokens(self_, [], result.eax, {"stored": stored})
    run(cmd, build_ensure_make)

    for which, value in ((0, 0), (0, 0xFFFFFFFF), (1, 0x55)):
        slot = 0x480 if which == 0 else 0x484
        cmd = f"setgoal {which} {value} mem 0x{slot:x}"

        def build(which=which, value=value, slot=slot):
            self_ = oracle.alloc(0x600)
            address = 0x00682CF0 if which == 0 else 0x00682D00
            return oracle.finish(address, self_, (value,), [slot])
        run(cmd, build)

    # query34: missing locomotor, inner null (high bits), and a real tail call.
    cmd = "query34 0 0"

    def build_q0():
        self_ = oracle.alloc(0x600)
        oracle.put(self_ + 0x3C, 0)
        result = oracle.emu.call(0x00682DD0, (), ecx=self_)
        return {"eax": f"{u32(result.eax):08x}", "log": ",".join(oracle.log) or ".", "mem": ".",
                "lo": "00000000", "mix": "1"}
    run(cmd, build_q0, skip_eax=True)

    cmd = "query34 1 0"

    def build_q1():
        self_ = oracle.alloc(0x600)
        loco = oracle.alloc(0x20)
        oracle.put(loco + 4, 0)
        oracle.put(self_ + 0x3C, loco)
        result = oracle.emu.call(0x00682DD0, (), ecx=self_)
        lo = result.eax & 0xFF
        mix = "1" if result.eax == ((loco & 0xFFFFFF00) | lo) else "0"
        return {"eax": f"{u32(result.eax):08x}", "log": "." if not oracle.log else ",".join(oracle.log),
                "mem": ".", "lo": f"{lo:08x}", "mix": mix}
    run(cmd, build_q1, skip_eax=True)

    cmd = "query34 2 0x3C"

    def build_q2():
        inner_vt = oracle.vt({0x34: ("n", 0x34)}, 0x40)
        inner = oracle.alloc(0x20)
        oracle.put(inner, inner_vt)
        loco = oracle.alloc(0x20)
        oracle.put(loco + 4, inner)
        self_ = oracle.alloc(0x600)
        oracle.put(self_ + 0x3C, loco)
        oracle.nret[0x34] = 0x3C
        return oracle.finish(0x00682DD0, self_)
    run(cmd, build_q2)

    for vret, speed, chain, kind in (
        (0, fbits(0.0), 0, 0),
        (1, fbits(0.5), 0, 0),
        (0, fbits(1.0), 0, 0),
        (0, fbits(1.5), 0, 0),
        (1, fbits(0.25), 1, 2),
        (1, fbits(0.25), 1, 1),
        (0, 0x7FC00000, 0, 0),
    ):
        cmd = f"advance {vret} 0x{speed:x} {chain} {kind}"

        def build(vret=vret, speed=speed, chain=chain, kind=kind):
            vt = oracle.vt({0x274: ("n", 0x274)})
            self_ = oracle.alloc(0x600)
            oracle.put(self_, vt)
            oracle.nret[0x274] = vret
            oracle.put(self_ + 0x188, speed)
            if chain:
                leaf = oracle.alloc(0x80)
                oracle.put(leaf + 0x64, kind)
                mid = oracle.alloc(0x10)
                oracle.put(mid + 4, leaf)
                link = oracle.alloc(0x10)
                oracle.put(link + 4, mid)
                oracle.put(self_ + 0x214, link)
            else:
                oracle.put(self_ + 0x214, 0)
            return oracle.finish(0x00682EC0, self_)
        run(cmd, build)

    for name, address, slot in (("query24", 0x00682F70, 0x24), ("query2c", 0x00682FA0, 0x2C), ("query30", 0x00682FC0, 0x30)):
        cmd = f"{name} 0 0"

        def build(address=address):
            self_ = oracle.alloc(0x600)
            loco = oracle.alloc(0x20)
            oracle.put(loco + 4, 0)
            oracle.put(self_ + 0x3C, loco)
            result = oracle.emu.call(address, (), ecx=self_)
            lo = result.eax & 0xFF
            mix = "1" if result.eax == ((loco & 0xFFFFFF00) | lo) else "0"
            return {"eax": f"{u32(result.eax):08x}", "log": ",".join(oracle.log) or ".", "mem": ".",
                    "lo": f"{lo:08x}", "mix": mix}
        run(cmd, build, skip_eax=True)
        cmd = f"{name} 1 0x21"

        def build_call(address=address, slot=slot):
            kind = ("n", slot)
            inner_vt = oracle.vt({slot: kind}, 0x40)
            inner = oracle.alloc(0x10)
            oracle.put(inner, inner_vt)
            loco = oracle.alloc(0x20)
            oracle.put(loco + 4, inner)
            self_ = oracle.alloc(0x600)
            oracle.put(self_ + 0x3C, loco)
            oracle.nret[slot] = 0x21
            return oracle.finish(address, self_)
        run(cmd, build_call)

    for arg in (0, 0x99, 0xFFFFFFFF):
        cmd = f"state4d {arg} mem 0x50 0x308 0x368 0x3c8"

        def build(arg=arg, cmd=cmd):
            vt = oracle.vt({0x34: ("n", 0x34), 0x1C: ("u", 0x1C)}, 0x40)
            loco = oracle.alloc(0x10)
            oracle.put(loco, vt)
            self_ = oracle.alloc(0x600)
            oracle.put(self_ + 0x3C, loco)
            return oracle.finish(0x00683220, self_, (arg,), offsets_of(cmd))
        run(cmd, build)

    for slot_sel, present, arg, ret in ((0, 0, 5, 0), (1, 0, 5, 0), (0, 1, 0x20, 0x77), (1, 1, 0, 0xFFFFFFFF)):
        cmd = f"forward {slot_sel} {present} {arg} {ret}"

        def build(slot_sel=slot_sel, present=present, arg=arg, ret=ret):
            slot = 0x20 if slot_sel == 0 else 0x24
            address = 0x006834E0 if slot == 0x20 else 0x00683500
            self_ = oracle.alloc(0x600)
            if not present:
                oracle.put(self_ + 0x3C, 0)
                return oracle.finish(address, self_, (arg,))
            vt = oracle.vt({slot: ("u", slot)}, 0x30)
            loco = oracle.alloc(0x10)
            oracle.put(loco, vt)
            oracle.put(self_ + 0x3C, loco)
            oracle.imm = ret
            return oracle.finish(address, self_, (arg,))
        run(cmd, build)

    cmd = "leech 0 0 0"

    def build_leech0():
        self_ = oracle.alloc(0x600)
        obj = oracle.alloc(0x340)
        oracle.put(obj + 0x33C, 0)
        return oracle.finish(0x00683E20, self_, (obj,))
    run(cmd, build_leech0)
    cmd = "leech 1 0x42 0x55"

    def build_leech1():
        vt = oracle.vt({0x08: ("p", 0x08)}, 0x10)
        listener = oracle.alloc(0x10)
        oracle.put(listener, vt)
        obj = oracle.alloc(0x340)
        oracle.put(obj + 0x33C, listener)
        draw = oracle.alloc(0xE0)
        oracle.put(draw + 0xDC, 0x42)
        self_ = oracle.alloc(0x600)
        oracle.put(self_ + 8, draw)
        oracle.imm = 0x55
        return oracle.finish(0x00683E20, self_, (obj,))
    run(cmd, build_leech1)

    for old in (0x12, 0):
        cmd = f"missile {old} mem 0x11c"

        def build(old=old, cmd=cmd):
            self_ = oracle.alloc(0x600)
            oracle.put(self_ + 0x11C, old)
            return oracle.finish(0x00683EE0, self_, (), offsets_of(cmd))
        run(cmd, build)

    for frames, scale, spf, frame in (
        (1.9, 1.0, 1.0, 10),
        (-1.9, 1.0, 1.0, 10),
        (0.0, 2.0, 3.0, 0),
        (2.0, 0.5, 0.5, 0xFFFFFFFF),
        (100.25, 1.0, 0.0, 4),
    ):
        cmd = f"projectile 0x{fbits(frames):x} 0x{fbits(scale):x} 0x{fbits(spf):x} {frame} mem 0x58"

        def build(frames=frames, scale=scale, spf=spf, frame=frame, cmd=cmd):
            clock = oracle.alloc(0x1D0)
            oracle.put(clock + 0x1C4, fbits(scale))
            oracle.put(clock + 0x58, frame)
            oracle.put(0x00DEBA64, clock)
            oracle.put(0x00DEEEE8, fbits(spf))
            self_ = oracle.alloc(0x600)
            return oracle.finish(0x006844E0, self_, (fbits(frames),), offsets_of(cmd))
        run(cmd, build)

    cmd = "repair 0 0x99 mem 0x30"

    def build_repair0():
        self_ = oracle.alloc(0x600)
        oracle.put(self_ + 0x30, 0x99)
        oracle.put(0x00DF03FC, 0)
        return oracle.finish(0x006846B0, self_, (), [0x30], extra={"p80": "00000000"})
    run(cmd, build_repair0)
    cmd = "repair 1 0x99 0x42 mem 0x30"

    def build_repair1():
        vt = oracle.vt({0x6C: ("u", 0x6C)}, 0x80)
        player = oracle.alloc(0x90)
        oracle.put(player, vt)
        oracle.put(0x00DF03FC, player)
        oracle.imm = 0x42
        self_ = oracle.alloc(0x600)
        oracle.put(self_ + 0x30, 0x99)
        result = oracle.emu.call(0x006846B0, (), ecx=self_)
        extra = {"p80": f"{oracle.get(player + 0x80):08x}"}
        return oracle.tokens(self_, [0x30], result.eax, extra)
    run(cmd, build_repair1)

    for left, vret in ((0, 0), (1, 0), (5, 1), (2, 0x100)):
        cmd = f"countdown {left} {vret} mem 0x3c"

        def build(left=left, vret=vret, cmd=cmd):
            vt = oracle.vt({0x9C: ("n", 0x9C)}, 0xA0)
            self_ = oracle.alloc(0x600)
            oracle.put(self_, vt)
            oracle.put(self_ + 0x3C, left)
            oracle.nret[0x9C] = vret
            return oracle.finish(0x00684C90, self_, (), offsets_of(cmd))
        run(cmd, build)

    for armed, ret in ((0, 0x55), (1, 0), (1, 0x21)):
        cmd = f"relay {armed} {ret}"

        def build(armed=armed, ret=ret):
            base = oracle.alloc(0x40)
            face = base + 0x10
            vt = oracle.vt({0x5C: ("n", 0x5C)}, 0x60)
            oracle.put(base, vt)
            oracle.emu.write(face + 4, bytes([armed]))
            oracle.nret[0x5C] = ret
            return oracle.finish(0x00685940, face)
        run(cmd, build)

    for accepted in (0, 1, 0x100, 0x201):
        cmd = f"tryopen {accepted}"

        def build(accepted=accepted):
            base = oracle.alloc(0x40)
            face = base + 0x10
            base_vt = oracle.vt({0x58: ("n", 0x58), 0x60: ("n", 0x60), 0x5C: ("n", 0x5C)}, 0x70)
            face_vt = oracle.vt({0x08: ("p", 0x08), 0x28: ("u", 0x28)}, 0x30)
            oracle.put(base, base_vt)
            oracle.put(face, face_vt)
            oracle.imm = accepted
            return oracle.finish(0x00685960, face, (0,))
        run(cmd, build)

    for keys, target in (([], 1), ([1, 2, 3], 2), ([1, 2, 3], 9), ([7], 7), ([0, 0], 0)):
        args = " ".join(str(key) for key in keys)
        cmd = f"find {len(keys)} {args} {target}".replace("  ", " ")

        def build(keys=keys, target=target):
            listing = oracle.alloc((len(keys) + 1) * 4)
            key_vt = oracle.vt({0x10: ("k", 0x10)}, 0x14)
            for index, key in enumerate(keys):
                module = oracle.alloc(0x10)
                oracle.put(module, key_vt)
                oracle.put(module + 4, key)
                oracle.put(listing + index * 4, module)
            oracle.put(listing + len(keys) * 4, 0)
            self_ = oracle.alloc(0x600)
            oracle.put(self_ + 0x310, listing)
            result = oracle.emu.call(0x00687460, (target,), ecx=self_)
            index = -1
            for i in range(len(keys)):
                if result.eax == oracle.get(listing + i * 4):
                    index = i
            return oracle.tokens(self_, [], 0, {"idx": str(index)})
        run(cmd, build, skip_eax=True)

    nan = 0x7FC00000
    for xyz in (
        (SENTINEL, SENTINEL, SENTINEL),
        (SENTINEL, SENTINEL, 0),
        (0, 0, 0),
        (nan, SENTINEL, SENTINEL),
        (0x80000000, SENTINEL, SENTINEL),
    ):
        cmd = f"coord 0x{xyz[0]:x} 0x{xyz[1]:x} 0x{xyz[2]:x} mem 0x40 0x44 0x48 0x5c"

        def build(xyz=xyz, cmd=cmd):
            coord = oracle.alloc(12)
            for i, bits in enumerate(xyz):
                oracle.put(coord + i * 4, bits)
            self_ = oracle.alloc(0x600)
            return oracle.finish(0x00689E80, self_, (coord,), offsets_of(cmd))
        run(cmd, build)

    run("areaxfer mem 0x80", lambda: _xfer(oracle, 0x0068A140, {0x18: ("p", 0x18)}, [0x80]))
    run("supplyxfer mem 0x80", lambda: _xfer(oracle, 0x0068A680, {0x18: ("p", 0x18)}, [0x80]))
    run("autoxfer mem 0x24 0x28 0x34 0x80 0x90", lambda: _xfer(oracle, 0x0068A6B0, {
        0x18: ("p", 0x18), 0x60: ("p", 0x60), 0x84: ("p", 0x84), 0x58: ("p", 0x58)},
        [0x24, 0x28, 0x34, 0x80, 0x90]))

    for dx, dy, width, step, toggle, alt in (
        (3.0, 4.0, 1.0, 2.0, 0, 0.0),
        (0.0, 0.0, 1.0, 1.0, 0, 0.0),
        (30.0, 40.0, 0.0, 10.0, 0, 0.0),
        (3.0, 4.0, 1.0, 2.0, 1, 10.0),
        (5.0, 0.0, 0.0, 1.0, 1, 1.0),
    ):
        cmd = (f"bridge 0x{fbits(dx):x} 0x{fbits(dy):x} 0x{fbits(width):x} "
               f"0x{fbits(step):x} {toggle} 0x{fbits(alt):x} mem 0x34 0x38")

        def build(dx=dx, dy=dy, width=width, step=step, toggle=toggle, alt=alt, cmd=cmd):
            ends = oracle.alloc(0x40)
            info = oracle.alloc(0x30)
            self_ = oracle.alloc(0x600)
            oracle.put(self_ + 0x28, 0)
            oracle.put(self_ + 0x2C, 0)
            oracle.put(ends + 0x38, fbits(dx))
            oracle.put(ends + 0x3C, fbits(dy))
            oracle.put(info + 0x10, fbits(width))
            oracle.put(info + 0x18, fbits(step))
            if toggle:
                alt_obj = oracle.alloc(0x10)
                oracle.put(alt_obj, fbits(alt))
                oracle.put(info + 0x20, alt_obj)
                oracle.put(info + 0x1C, 1)
            else:
                oracle.put(info + 0x1C, 0)
                oracle.put(info + 0x20, 0)
            oracle.put(self_ + 8, ends)
            oracle.put(self_ + 4, info)
            return oracle.finish(0x0068B180, self_, (), offsets_of(cmd))
        run(cmd, build, skip_eax=True)

    run("moneyxfer mem 0x1c 0x80", lambda: _xfer(oracle, 0x0068BFA0, {
        0x18: ("p", 0x18), 0x84: ("p", 0x84)}, [0x1C, 0x80]))

    for obj, player, target, ident in ((0, 0, 0, 0), (1, 0, 0, 5), (1, 1, 0, 5), (1, 1, 1, 0), (1, 1, 1, 0x3E)):
        cmd = f"energy {obj} {player} {target} {ident}"

        def build(obj=obj, player=player, target=target, ident=ident):
            self_ = oracle.alloc(0x600)
            written = 0
            if not obj:
                oracle.put(self_ + 8, 0)
            else:
                object_ = oracle.alloc(0x420)
                oracle.put(object_ + 0xDC, ident)
                if not player:
                    oracle.put(object_ + 0x418, 0)
                else:
                    player_obj = oracle.alloc(0x80)
                    if not target:
                        oracle.put(player_obj + 0x74, 0)
                    else:
                        target_obj = oracle.alloc(0x90)
                        oracle.put(player_obj + 0x74, target_obj)
                        written = target_obj
                    oracle.put(object_ + 0x418, player_obj)
                oracle.put(self_ + 8, object_)
            oracle.emu.call(0x0068C210, (), ecx=self_)
            value = oracle.get(written + 0x80) if written else 0
            return oracle.tokens(self_, [], 0, {"t80": f"{value:08x}"})
        run(cmd, build)

    patterns = [
        [0x11111111] * 11,
        [0x80000000, 0x7FC00000, 0, 1, 0x3F800000, 0xBF800000, 0x7F800000, 2, 3, 4, 5],
        list(range(11)),
    ]
    for pattern in patterns:
        cmd = "copy " + " ".join(str(v) for v in pattern)

        def build(pattern=pattern):
            src = oracle.alloc(0x30)
            for i, value in enumerate(pattern):
                oracle.put(src + i * 4, value)
            self_ = oracle.alloc(0x600)
            result = oracle.emu.call(0x0068C520, (src,), ecx=self_)
            retdst = "1" if result.eax == self_ else "0"
            return oracle.tokens(self_, list(range(0, 44, 4)), 0, {"retdst": retdst})
        run(cmd, build, skip_eax=True)

    for values in ([], [1], [0xFFFFFFFF, 1], [2, 3, 4, 5]):
        cmd = "sum " + str(len(values)) + (" " + " ".join(str(v) for v in values) if values else "")

        def build(values=values):
            pairs = oracle.alloc((len(values) + 1) * 8)
            for i, value in enumerate(values):
                oracle.put(pairs + i * 8, value)
            self_ = oracle.alloc(0x600)
            oracle.put(self_ + 0x24, pairs)
            oracle.put(self_ + 0x28, pairs + len(values) * 8)
            return oracle.finish(0x0068D650, self_)
        run(cmd.strip(), build)

    for clear in (0, 1):
        cmd = f"power {clear} mem 0x54 0x80 0x90"

        def build(clear=clear, cmd=cmd):
            oracle.clear = clear != 0
            return _xfer(oracle, 0x0068DC20, {0x18: ("p", 0x18), 0x84: ("p", 0x84)}, offsets_of(cmd))
        run(cmd, build)

    run("slave mem 0x24 0x28 0x80 0x90", lambda: _xfer(
        oracle, 0x0068EB00, {0x18: ("p", 0x18)}, [0x24, 0x28, 0x80, 0x90]))
    run("stances mem 0x38 0x80 0x90", lambda: _xfer(
        oracle, 0x0068F170, {0x18: ("p", 0x18), 0x88: ("t", 0x88)}, [0x38, 0x80, 0x90]))

    checker.close()
    print(f"cases={count} mismatches={len(failures)}")
    for failure in failures[:30]:
        print(failure)
    return 1 if failures else 0


def _xfer(oracle: Oracle, address: int, slots: dict[int, tuple[str, int]], mem: list[int]) -> dict[str, str]:
    vt = oracle.vt(slots, 0x90)
    xfer = oracle.alloc(0x20)
    oracle.put(xfer, vt)
    self_ = oracle.alloc(0x600)
    return oracle.finish(address, self_, (xfer,), mem)


if __name__ == "__main__":
    sys.exit(main())
