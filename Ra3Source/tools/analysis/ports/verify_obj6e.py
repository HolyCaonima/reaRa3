"""Differential oracle for the 0x006E0000–0x006FFFFF object-module batch.

The Release checker and Unicorn execute the same command script. Logs, watched
dwords and the selected eax view must match.
"""

from __future__ import annotations

import struct
import subprocess
import sys
from pathlib import Path

from unicorn import x86_const

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from emulate import Emulator  # noqa: E402

EXE = Path(__file__).resolve().parents[3] / "build" / "win32" / "bin" / "Release" / "ra3_obj6echeck.exe"


def u32(value: int) -> int:
    return value & 0xFFFFFFFF


def p32(value: int) -> bytes:
    return struct.pack("<I", u32(value))


def hx(value: int) -> str:
    return format(u32(value), "x")


def pv(value: int) -> str:
    return "0" if u32(value) == 0 else "p"


def f32_to_80(bits: int) -> tuple[int, int]:
    bits &= 0xFFFFFFFF
    sign = (bits >> 31) & 1
    exp = (bits >> 23) & 0xFF
    frac = bits & 0x7FFFFF
    if exp == 0:
        return (0, sign << 15)
    if exp == 255:
        return ((1 << 63) | (frac << 40), (sign << 15) | 0x7FFF)
    return ((1 << 63) | (frac << 40), (sign << 15) | ((exp - 127 + 16383) & 0x7FFF))


class Checker:
    def __init__(self, executable: Path) -> None:
        self.process = subprocess.Popen(
            [str(executable)],
            stdin=subprocess.PIPE,
            stdout=subprocess.PIPE,
            text=True,
            bufsize=1,
        )

    def send(self, command: str) -> None:
        assert self.process.stdin is not None
        self.process.stdin.write(command + "\n")
        self.process.stdin.flush()

    def read(self) -> dict[str, str]:
        assert self.process.stdout is not None
        line = self.process.stdout.readline().strip()
        if not line:
            raise RuntimeError("checker closed")
        out = {}
        for token in line.split():
            key, value = token.split("=", 1)
            out[key] = value
        return out

    def close(self) -> None:
        try:
            if self.process.stdin is not None:
                self.process.stdin.write("quit\n")
                self.process.stdin.flush()
            self.process.wait(timeout=10)
        except OSError:
            self.process.kill()


class Oracle:
    def __init__(self, checker: Checker) -> None:
        self.checker = checker
        self.emu = Emulator()
        self._bind()

    def _bind(self) -> None:
        self.fn: dict[tuple[str, int], int] = {}
        specs = [
            ("n", 0x10, 0), ("n", 0x14, 0), ("n", 0x18, 0), ("n", 0x1C, 0), ("n", 0x20, 0),
            ("n", 0x24, 0), ("n", 0x28, 0), ("n", 0x30, 0), ("n", 0x34, 0), ("n", 0x4C, 0),
            ("n", 0x64, 0), ("n", 0x6C, 0), ("n", 0x70, 0), ("n", 0x84, 0), ("n", 0xB0, 0),
            ("n", 0xB4, 0), ("n", 0x20C, 0),
            ("u", 0x1C, 4), ("u", 0x28, 4), ("u", 0x3C, 4), ("u", 0x5C, 4), ("u", 0xB0, 4),
            ("u", 0x10C, 4),
            ("p", 0x18, 4), ("p", 0x1C, 4), ("p", 0x38, 4),
            ("w", 0x18, 8), ("m", 0x08, 8), ("m", 0x58, 8),
            ("s", 0x6C, 4), ("s", 0x70, 4), ("q", 0xAC, 8),
        ]
        for kind, slot, cleanup in specs:
            self.fn[(kind, slot)] = self.emu.bind_stub(self._virt(kind, slot), cleanup)
        bind = self.emu.bind_function
        bind(0x00A21BE0, lambda emu: self.add("lk"), 4)
        bind(0x00A21C90, lambda emu: self.add("ul"), 4)
        bind(0x0050B5F0, self.on_find, 8)
        bind(0x004D3B50, lambda emu: self.log_ret("d4d3b50:" + hx(emu.stack_arg(0))), 4)
        bind(0x006A4FB0, lambda emu: self.add("d6a4fb0"), 0)
        bind(0x00687720, lambda emu: self.log_ret("d687720:" + hx(emu.stack_arg(0))), 4)
        bind(0x006D1ED0, lambda emu: self.log_ret("d6d1ed0:" + hx(emu.stack_arg(0)) + ":" + hx(emu.stack_arg(1))), 8)
        bind(0x00687560, lambda emu: self.log_ret("d687560"), 0)
        bind(0x0054F490, lambda emu: self.add("d54f490:" + hx(emu.stack_arg(0))), 4)
        bind(0x006C0ED0, lambda emu: self.log_ret("d6c0ed0"), 0)
        bind(0x005AC4F0, lambda emu: self.add("d5ac4f0:" + hx(emu.stack_arg(0)) + ":" + hx(emu.stack_arg(1))), 8)
        bind(0x006CC2E0, lambda emu: self.log_ret("d6cc2e0:" + pv(emu.stack_arg(0))), 4)
        bind(0x006CACE0, lambda emu: self.log_ret(
            "d6cace0:" + hx(emu.stack_arg(0)) + ":" + hx(emu.stack_arg(1)) + ":" + hx(emu.stack_arg(2))), 12)
        bind(0x006A7B60, lambda emu: self.log_ret("d6a7b60:" + hx(emu.stack_arg(0)) + ":" + hx(emu.stack_arg(1))), 8)
        bind(0x006CAB20, lambda emu: self.log_ret("d6cab20"), 0)
        bind(0x006EAD80, lambda emu: self.log_ret("d6ead80"), 0)
        bind(0x006E6830, lambda emu: self.add("d6e6830:" + pv(emu.stack_arg(0))), 4)
        bind(0x008D82F0, lambda emu: self.add("d8d82f0:p"), 4)
        bind(0x00685080, lambda emu: self.add("d685080:p"), 4)
        bind(0x006E2470, lambda emu: self.log_ret("d6e2470:p"), 4)
        bind(0x006D3A00, lambda emu: self.log_ret("d6d3a00:p"), 4)
        bind(0x006D3870, lambda emu: self.log_ret("d6d3870:p"), 4)
        bind(0x006A8400, lambda emu: self.add("d6a8400"), 0)
        bind(0x006BCF30, lambda emu: self.add("d6bcf30:" + hx(emu.stack_arg(0))), 4)
        bind(0x006D0330, lambda emu: self.add("d6d0330"), 0)
        bind(0x006A88C0, lambda emu: self.log_ret("d6a88c0:" + hx(emu.stack_arg(0))), 4)
        bind(0x006D6EC0, lambda emu: self.log_ret("d6d6ec0"), 0)
        bind(0x006D9560, self.on_9560, 4)
        bind(0x006EFE40, lambda emu: self.log_ret("d6efe40"), 0)
        bind(0x00AAFE10, self.on_aafe, 8)
        bind(0x006F2140, lambda emu: self.log_ret(
            "d6f2140:" + ":".join(hx(emu.stack_arg(i)) for i in range(4))), 16)
        bind(0x006FA190, lambda emu: self.log_ret("d6fa190:" + hx(emu.stack_arg(0))), 4)
        bind(0x006E0450, lambda emu: self.log_ret("d6e0450:" + hx(emu.stack_arg(0))), 4)
        bind(0x006E1630, lambda emu: self.add("d6e1630"), 0)
        bind(0x006EAEF0, lambda emu: self.log_ret("d6eaef0"), 0)
        bind(0x006FF750, self.on_ff750, 0)
        bind(0x0052DDE0, lambda emu: self.add("d52dde0:" + hx(emu.stack_arg(0)) + ":" + hx(emu.stack_arg(1))), 8)
        bind(0x006E3A60, lambda emu: self.log_ret("d6e3a60:" + hx(emu.stack_arg(0)) + ":" + hx(emu.stack_arg(1))), 0)
        bind(0x006C0E50, lambda emu: self.log_ret("d6c0e50:" + hx(emu.stack_arg(0))), 4)
        bind(0x006E90B0, lambda emu: self.log_ret("d6e90b0:" + hx(emu.stack_arg(0)) + ":" + hx(emu.stack_arg(1))), 8)
        bind(0x0069D570, lambda emu: self.log_ret("d69d570:" + pv(emu.stack_arg(0))), 4)
        bind(0x006F1350, lambda emu: self.log_ret("d6f1350"), 0)

    def _virt(self, kind: str, slot: int):
        def stub(emu: Emulator, kind: str = kind, slot: int = slot) -> int:
            return self.on_virtual(emu, kind, slot)
        return stub

    def add(self, text: str) -> int:
        self.log.append(text)
        return 0

    def log_ret(self, text: str) -> int:
        self.add(text)
        return self.pop()

    def pop(self) -> int:
        if not self.rets:
            return 0
        token = self.rets.pop(0)
        if token.startswith("@"):
            ident = int(token[1:], 0)
            return 0 if ident == 0 else self.objs[ident][0]
        return int(token, 0)

    def reset(self) -> None:
        self.emu.reset_data()
        self.log = []
        self.rets: list[str] = []
        self.table: dict[int, int] = {}
        self.objs: dict[int, tuple[int, int]] = {}
        self.watch = 0
        self.offs: list[int] = []
        self.mode = "full"
        self.mix = 0
        self.patch_on = False
        self.patch = 0
        self.st0 = 0
        self.clock = 0
        self.buckets = 0
        self.end_node = 0
        self.hit_node = 0
        self.emu.uc.reg_write(x86_const.UC_X86_REG_FPCW, 0x037F)

    def get(self, address: int) -> int:
        return struct.unpack("<I", self.emu.read(address, 4))[0]

    def put(self, address: int, value: int) -> None:
        self.emu.write(address, p32(value))

    def on_find(self, emu: Emulator) -> int:
        out = u32(emu.stack_arg(0))
        key = self.get(u32(emu.stack_arg(1)))
        self.add("fd:" + hx(key))
        found = self.table.get(key, 0)
        if found:
            self.put(self.hit_node + 4, found)
            self.put(out, self.hit_node)
        else:
            self.put(out, self.end_node)
        return 0

    def on_9560(self, emu: Emulator) -> int:
        ecx = u32(emu.register(x86_const.UC_X86_REG_ECX))
        arg = u32(emu.stack_arg(0))
        self.add("d6d9560:" + pv(arg))
        if self.patch_on:
            self.put(ecx + 0x14, self.patch)
        return self.pop()

    def on_aafe(self, emu: Emulator) -> int:
        ecx = u32(emu.register(x86_const.UC_X86_REG_ECX))
        self.add("daafe10")
        flag = self.pop()
        if not hasattr(self, "cell") or self.cell == 0:
            self.cell = self.emu.alloc(b"\0\0\0\0")
        value = (ecx + 4) & 0xFFFFFFFF if flag == 1 else 0
        self.put(self.cell, value)
        return self.cell

    def on_ff750(self, emu: Emulator) -> int:
        self.add("d6ff750")
        mant, exp = f32_to_80(self.st0)
        emu.uc.reg_write(x86_const.UC_X86_REG_FPSW, 0)
        emu.uc.reg_write(x86_const.UC_X86_REG_FPTAG, 0xFFFC)
        emu.uc.reg_write(x86_const.UC_X86_REG_FP0, (mant, exp))
        return 0

    def on_virtual(self, emu: Emulator, kind: str, slot: int) -> int:
        if kind == "n":
            self.add(f"v{slot:x}")
            return self.pop()
        if kind in ("u", "p"):
            arg = u32(emu.stack_arg(0))
            shown = hx(arg) if kind == "u" else pv(arg)
            self.add(f"v{slot:x}:{shown}")
            return self.pop()
        if kind in ("w", "m"):
            first = u32(emu.stack_arg(0))
            second = u32(emu.stack_arg(1))
            left = hx(first) if kind == "w" else pv(first)
            self.add(f"v{slot:x}:{left}:{hx(second)}")
            return self.pop()
        if kind == "s":
            self.add(f"v{slot:x}:p")
            value = self.pop()
            self.put(u32(emu.stack_arg(0)), value)
            return value
        if kind == "q":
            self.add(f"v{slot:x}:p:0")
            if not hasattr(self, "word") or self.word == 0:
                self.word = self.emu.alloc(b"\0\0\0\0")
            value = self.pop()
            self.put(self.word, value)
            return self.word
        raise RuntimeError(kind)

    def apply(self, line: str) -> None:
        parts = line.split()
        cmd = parts[0]
        if cmd == "reset":
            self.reset()
            self.cell = 0
            self.word = 0
        elif cmd == "obj":
            ident = int(parts[1], 0)
            size = int(parts[2], 0)
            self.objs[ident] = (self.emu.alloc(b"\x11" * size), size)
        elif cmd == "u32":
            base = self.objs[int(parts[1], 0)][0]
            self.put(base + int(parts[2], 0), int(parts[3], 0))
        elif cmd == "u8":
            base = self.objs[int(parts[1], 0)][0]
            self.emu.write(base + int(parts[2], 0), bytes([int(parts[3], 0) & 0xFF]))
        elif cmd == "ptr":
            base = self.objs[int(parts[1], 0)][0]
            other = int(parts[3], 0)
            self.put(base + int(parts[2], 0), 0 if other == 0 else self.objs[other][0])
        elif cmd == "ext":
            base = self.objs[int(parts[1], 0)][0]
            other = self.objs[int(parts[3], 0)][0]
            self.put(base + int(parts[2], 0), other + int(parts[4], 0))
        elif cmd == "vt":
            base = self.objs[int(parts[1], 0)][0]
            raw = bytearray(b"\x11" * 0x400)
            for item in parts[3].split(","):
                slot_text, kind = item.split(":")
                slot = int(slot_text, 16)
                struct.pack_into("<I", raw, slot, self.fn[(kind, slot)])
            table = self.emu.alloc(bytes(raw))
            self.put(base + int(parts[2], 0), table)
        elif cmd == "gclock":
            self.clock = self.objs[int(parts[1], 0)][0]
            self.put(0x00DEBA64, self.clock)
            self.buckets = self.emu.alloc(b"\0" * 16)
            self.end_node = self.emu.alloc(b"\0" * 8)
            self.hit_node = self.emu.alloc(b"\0" * 8)
            self.put(self.buckets, self.end_node)
            self.put(self.clock + 0xC8, self.buckets)
            self.put(self.clock + 0xCC, 0)
        elif cmd == "map":
            key = int(parts[1], 0)
            ident = int(parts[2], 0)
            self.table[key] = 0 if ident == 0 else self.objs[ident][0]
        elif cmd == "ret":
            self.rets.append(parts[1])
        elif cmd == "st0":
            self.st0 = int(parts[1], 0)
        elif cmd == "patch":
            self.patch_on = True
            self.patch = int(parts[1], 0)
        elif cmd == "watch":
            self.watch = int(parts[1], 0)
            self.offs = [int(item, 0) for item in parts[2].split(",")]
        elif cmd == "mode":
            self.mode = parts[1]
        elif cmd == "mix":
            self.mix = int(parts[1], 0)

    def execute(self, va: int, ecx: str, args: list[str]) -> dict[str, str]:
        if ecx.startswith("o"):
            plus = ecx.find("+")
            ident = int(ecx[1:] if plus < 0 else ecx[1:plus], 0)
            off = 0 if plus < 0 else int(ecx[plus + 1:], 0)
            this = self.objs[ident][0] + off
        elif ecx.startswith("r"):
            this = int(ecx[1:], 0)
        else:
            raise RuntimeError(ecx)
        stack = []
        for token in args:
            if token.startswith("@"):
                ident = int(token[1:], 0)
                stack.append(0 if ident == 0 else self.objs[ident][0])
            else:
                stack.append(int(token, 0))
        self.emu.uc.reg_write(x86_const.UC_X86_REG_FPCW, 0x037F)
        result = self.emu.call(va, stack, ecx=this, eax=0)
        eax = u32(result.eax)
        if self.offs and self.watch:
            base = self.objs[self.watch][0]
            mem = ",".join(f"{off:x}:{self.get(base + off):08x}" for off in self.offs)
        else:
            mem = "."
        out = {
            "eax": f"{eax:08x}",
            "al": f"{eax & 0xFF:02x}",
            "log": ",".join(self.log) if self.log else ".",
            "mem": mem,
        }
        if self.mode == "mix":
            out["mix"] = self.mix_of(eax)
        return out

    def mix_of(self, eax: int) -> str:
        if eax == 0:
            return "0"
        if self.mix not in self.objs:
            return "miss"
        base, size = self.objs[self.mix]
        if base <= eax < base + size:
            return hx(eax - base)
        return "miss"


CASES = r'''
# movement null and field
reset
mode full
run 0x6e1c80 r0xC
---
reset
obj 1 0x80
mode mix
mix 1
run 0x6e1c80 o1+0xC
---
# evacuate miss, zero count, dispatch
reset
obj 1 0x20
obj 2 0x340
ptr 1 0 2
u32 2 0x328 0
u32 2 0x32c 0
u32 2 0x330 0
mode al
run 0x6e1d30 o1+8 1 2 0x3f800000
---
reset
obj 1 0x20
obj 2 0x340
obj 3 0x20
ptr 1 0 2
ptr 2 0x328 3
vt 3 0 10c:u
ret 0
mode full
run 0x6e1d30 o1+8 1 2 0x3f800000
---
reset
obj 1 0x20
obj 2 0x340
obj 3 0x20
ptr 1 0 2
ptr 2 0x328 3
vt 3 0 10c:u
ret 3
ret 0x15
mode full
run 0x6e1d30 o1+8 4 5 0
---
# replace and ocl
reset
obj 1 0x10
ret 0x21
mode full
run 0x6e1e40 o1 7 8 0x3f800000
---
reset
obj 1 0x80
obj 2 0x200
ptr 1 4 2
u32 2 0x1c0 0
u32 2 0x1cc 5
u32 1 0x50 5
mode full
run 0x6e1f70 o1+0x10 1 2 0
---
reset
obj 1 0x80
obj 2 0x200
ptr 1 4 2
u32 2 0x1c0 0
u32 2 0x1cc 0
ret 0
mode full
run 0x6e1f70 o1+0x10 1 2 3
---
reset
obj 1 0x80
obj 2 0x200
ptr 1 4 2
u32 2 0x1c0 0
u32 2 0x1cc 0
ret 1
mode full
run 0x6e1f70 o1+0x10 1 2 3
---
reset
obj 1 0x80
obj 2 0x200
obj 3 0x10
ptr 1 4 2
ptr 1 8 3
u32 2 0x1c0 9
u32 2 0x1cc 0
u32 1 8 4
ret 0
mode full
run 0x6e1f70 o1+0x10 6 2 3
---
# special power bits
reset
obj 1 0x20
obj 2 0x10
obj 3 0x100
ptr 1 8 2
ptr 2 4 3
u32 3 0xcc 0
mode full
run 0x6e2f30 o1
---
reset
obj 1 0x20
obj 2 0x10
obj 3 0x100
ptr 1 8 2
ptr 2 4 3
u32 3 0xcc 0x2000000
ret 0x44
mode full
run 0x6e2f30 o1
---
reset
obj 1 0x20
obj 2 0x100
ptr 1 0 2
u32 2 0x90 0x40
mode mix
mix 2
run 0x6e2f70 o1+0xC
---
reset
obj 1 0x20
obj 2 0x100
ptr 1 0 2
u32 2 0x90 0
ret 0x45
mode full
run 0x6e2f70 o1+0xC
---
# hidden clear / apply
reset
obj 1 0x20
u32 1 8 0
mode full
run 0x6e3220 o1
---
reset
obj 1 0x20
obj 2 0x100
ptr 1 8 2
u32 2 0x80 0
mode full
run 0x6e3220 o1
---
reset
obj 1 0x30
obj 2 0x400
obj 3 0x20
obj 4 0x40
ptr 1 8 2
ptr 1 4 3
u32 3 8 0x12
u32 2 0x80 0x40000
u32 2 0xc8 0
vt 4 0 28:u
ret 0
ret 0
ret 7
mode full
run 0x6e3220 o1
---
reset
obj 1 0x40
vt 1 0x24 18:w
ret 0
mode full
run 0x6e3290 o1
---
reset
obj 1 0x100
obj 2 0x400
obj 3 0x20
obj 4 0x40
obj 5 0x40
obj 9 0x80
vt 1 0x24 18:w
ptr 1 8 2
ptr 1 4 3
ptr 2 0x374 4
u32 3 8 0x30
u32 2 0x80 0
u32 2 0xc8 0
vt 5 0 28:u
vt 4 0 20c:n
gclock 9
u32 9 0x58 0x60
ret 1
ret @5
ret 0
ret 1
ret 0
ret 0x77
watch 1 0xb8
watch 2 0x80,0xc8
mode full
run 0x6e3290 o1
---
# dozer
reset
obj 1 0x40
mode full
run 0x6e3e10 o1 0
---
reset
obj 1 0x40
vt 1 0 24:n
ret 0
mode full
run 0x6e3e10 o1 5
---
reset
obj 1 0x40
obj 9 0x200
vt 1 0 24:n
u32 1 0xc 0
ret 1
gclock 9
mode full
run 0x6e3e10 o1 5
---
reset
obj 1 0x40
obj 2 0x20
obj 9 0x200
vt 1 0 24:n
u32 1 0xc 0x70
gclock 9
map 0x70 2
ret 1
mode full
run 0x6e3e10 o1 @2
---
reset
obj 1 0x40
obj 2 0x20
obj 3 0x20
obj 9 0x200
vt 1 0 24:n
u32 1 0xc 0x70
gclock 9
map 0x70 3
ret 1
mode full
run 0x6e3e10 o1 @2
---
reset
obj 1 0x600
obj 2 0x20
vt 1 0x514 3c:u,6c:n,64:n
vt 2 0 18:n
ptr 1 0x530 2
ret 0
ret 0
ret 0
ret 9
mode full
run 0x6e4220 o1+0x514 4
---
reset
obj 1 0x600
obj 2 0x20
vt 1 0x514 3c:u,6c:n,64:n
vt 2 0 18:n
ptr 1 0x530 2
ret 0
ret 0
ret 1
ret 0
ret 8
mode full
run 0x6e4220 o1+0x514 4
---
reset
obj 1 0x100
u32 1 0xc0 0
mode full
run 0x6e44d0 o1
---
reset
obj 1 0x100
obj 2 0x30
obj 9 0x200
u32 1 0xc0 0x55
gclock 9
map 0x55 2
mode mix
mix 2
run 0x6e44d0 o1
---
reset
obj 1 0x100
obj 9 0x200
u32 1 0xc0 0x55
gclock 9
mode full
run 0x6e44d0 o1
---
# horde flag and supply
reset
obj 1 0x600
obj 2 0x340
obj 3 0x20
ptr 1 0 2
ptr 2 0x328 3
vt 3 0 84:n
ret 0
mode full
run 0x6e46e0 o1+0x514 1 0
---
reset
obj 1 0x600
obj 2 0x340
obj 3 0x20
obj 4 0x100
obj 5 0x40
ptr 1 0 2
ptr 2 0x328 3
vt 3 0 84:n
vt 5 0 1c:p
ptr 4 0 5
u32 4 0x84 0
ret @5
ret 0
ret 0
watch 4 0x84
mode full
run 0x6e46e0 o1+0x514 @4 0
---
reset
obj 1 0x580
vt 1 0x51c 5c:u
ret 0
ret 3
mode full
run 0x6e48f0 o1 0
---
reset
obj 1 0x20
ret 6
mode full
run 0x6e48f0 o1 4
---
reset
obj 1 0x560
obj 2 0x20
obj 3 0x40
obj 4 0x100
ptr 1 8 2
ptr 1 0x3c 3
vt 3 0 34:n,38:p,1c:u
u32 4 0xdc 0x81
ret 0
watch 1 0x524
mode full
run 0x6e4920 o1 @4 0
---
reset
obj 1 0x560
obj 2 0x20
obj 3 0x40
obj 4 0x100
ptr 1 8 2
ptr 1 0x3c 3
vt 3 0 34:n,38:p,1c:u
ret 1
ret 0
ret 0
ret 0x2e
watch 1 0x50
mode full
run 0x6e4920 o1 @4 0x19
---
reset
obj 1 0x20
obj 2 0x100
ptr 1 8 2
u32 2 0x88 0x1000
mode full
run 0x6e4980 o1 1 2
---
reset
obj 1 0x560
obj 2 0x100
obj 4 0x20
ptr 1 8 2
u32 2 0x88 0
u32 4 0 0x10
u32 4 4 0x20
u32 4 8 0x30
ret 0
watch 1 0x528,0x534
mode full
run 0x6e4980 o1 @4 0
---
reset
obj 1 0x20
obj 2 0x100
ptr 1 8 2
u32 2 0x88 0x1000
mode full
run 0x6e4a10 o1 1 2
---
reset
obj 1 0x580
obj 2 0x100
obj 4 0x100
ptr 1 8 2
u32 2 0x88 0
u32 4 0xe4 0x64
ret 0
watch 1 0x538,0x53c
mode full
run 0x6e4a10 o1 @4 0
---
# money and production
reset
obj 1 0x10
mode full
run 0x6e8750 o1 1
---
reset
obj 1 0x40
obj 2 0x160
ptr 1 4 2
u32 2 0x14c 1
mode mix
mix 2
run 0x6e8750 o1 0
---
reset
obj 1 0x40
obj 2 0x160
obj 9 0x200
ptr 1 4 2
u32 2 0x14c 0
u32 1 0x2c 0
gclock 9
mode full
run 0x6e8750 o1 0
---
reset
obj 1 0x40
obj 2 0x160
obj 3 0x360
obj 4 0x40
obj 5 0x40
obj 9 0x200
ptr 1 4 2
u32 2 0x14c 0
u32 1 0x2c 0x61
gclock 9
map 0x61 3
ptr 3 0x33c 4
vt 4 0 64:n
ret @5
u32 5 0x34 0x12
mode full
run 0x6e8750 o1 0
---
reset
obj 1 0x40
obj 2 0x160
obj 3 0x360
obj 4 0x40
obj 5 0x40
obj 9 0x200
ptr 1 4 2
u32 2 0x14c 0
u32 1 0x2c 0x61
gclock 9
map 0x61 3
ptr 3 0x33c 4
vt 4 0 64:n
ret @5
u32 5 0x34 0x3
ret 0x66
mode full
run 0x6e8750 o1 0
---
reset
obj 1 0x10
mode full
run 0x6e8fd0 o1 0
---
reset
obj 1 0x10
obj 2 0x60
u32 2 0x54 0x1234
mode full
run 0x6e8fd0 o1 @2
---
reset
obj 1 0x40
u32 1 0x3c 0xffffffff
watch 1 0x3c
mode full
run 0x6e9010 o1
---
reset
obj 1 0x100
u32 1 0xec 0
mode full
run 0x6e9040 o1
---
reset
obj 1 0x100
obj 2 0x20
obj 9 0x200
u32 1 0xec 0x44
gclock 9
map 0x44 2
mode mix
mix 2
run 0x6e9040 o1
---
reset
obj 1 0x40
ret 0x31
mode full
run 0x6e91f0 o1+0x24 6
---
reset
obj 1 0x120
obj 2 0x20
vt 1 0x1c 4c:n
ptr 1 0 2
ret 0
ret 0
mode full
run 0x6e9390 o1+0x1c
---
reset
obj 1 0x120
obj 2 0x40
obj 9 0x200
vt 1 0x1c 4c:n
vt 2 0 8:m,30:n
ptr 1 0 2
u32 1 0x108 0
ret 0
ret @2
ret 0x51
mode full
run 0x6e9390 o1+0x1c
---
reset
obj 1 0x20
u32 1 4 0
mode full
run 0x6e9ee0 o1
---
reset
obj 1 0x20
obj 2 0x50
obj 9 0x200
u32 1 4 0x19
gclock 9
map 0x19 2
mode mix
mix 2
run 0x6e9ee0 o1
---
# ability, steal, xfer, enclose
reset
obj 1 0x10
obj 2 0x20
ptr 1 4 2
u32 2 0x10 0x4b189680
mode al
run 0x6eb4e0 o1
---
reset
obj 1 0x10
obj 2 0x20
ptr 1 4 2
u32 2 0x10 0
ret 0
mode full
run 0x6eb4e0 o1
---
reset
obj 1 0x10
obj 2 0x20
ptr 1 4 2
u32 2 0x10 0
ret 1
mode full
run 0x6eb4e0 o1
---
reset
obj 1 0x60
obj 2 0x430
obj 3 0x30
obj 4 0x20
obj 5 0x20
ptr 2 0x418 3
u32 3 0x20 0x5
ptr 2 0x310 1
ptr 2 0x3c0 1
ptr 2 0x3b4 4
ptr 4 0x14 5
vt 5 0 b4:n
vt 1 0 5c:u
u32 1 0x54 0x11
ret 0x11
ret 0x22
mode full
run 0x6ebac0 o1 @2
---
reset
obj 1 0x60
obj 2 0x430
obj 3 0x30
obj 4 0x20
obj 5 0x20
obj 6 0x430
ptr 2 0x418 3
u32 3 0x20 0x5
ptr 2 0x310 1
ptr 2 0x3c0 1
ptr 2 0x3b4 4
ptr 4 0x14 5
ptr 1 8 6
ptr 6 0x418 3
vt 5 0 b4:n,b0:u
u32 1 0x54 0x9
ret 0x11
ret 0
watch 1 0x54
mode mix
mix 3
run 0x6ebac0 o1 @2
---
reset
obj 1 0x40
obj 2 0x20
vt 2 0 18:p,6c:s,70:s
u32 1 0x24 0
ret 0
ret 0
ret 0
ret 0x12
mode full
run 0x6ebbd0 o1 @2
---
reset
obj 1 0x40
obj 2 0x20
vt 2 0 18:p,6c:s,70:s
u32 1 0x24 0
ret 0
ret 0
ret 2
ret 0
ret 0x3
ret 0x4
mode full
run 0x6ebbd0 o1 @2
---
reset
obj 1 0x20
obj 2 0x30
ptr 1 0 2
u32 2 0xc 0
u8 1 0x10 0
mode full
run 0x6ece70 o1+0xC 1
---
reset
obj 1 0x20
obj 2 0x30
ptr 1 0 2
u32 2 0xc 1
u8 1 0x10 1
mode full
run 0x6ece70 o1+0xC 1
---
reset
obj 1 0x20
obj 2 0x30
ptr 1 0 2
u32 2 0xc 4
u8 1 0x10 0
u8 2 0x1d 1
ret 1
mode full
run 0x6ece70 o1+0xC 3
---
reset
obj 1 0x20
obj 2 0x30
ptr 1 0 2
u32 2 0xc 4
u8 1 0x10 0
u8 2 0x1d 0
u8 2 0x1c 1
ret 0
ret 0x17
mode full
run 0x6ece70 o1+0xC 3
---
# weapons
reset
obj 1 0x30
obj 2 0x80
obj 3 0x80
obj 4 0x150
ptr 1 0x20 2
ptr 2 4 3
ptr 3 4 4
u8 4 0x13f 1
u32 4 0x70 0
u32 2 0x1c 1
mode full
run 0x6ed440 o1
---
reset
obj 1 0x30
obj 2 0x80
obj 3 0x80
obj 4 0x150
ptr 1 0x20 2
ptr 2 4 3
ptr 3 4 4
u8 4 0x13f 1
u32 4 0x70 0x20
u32 2 0x1c 0x10
watch 2 0x1c
mode full
run 0x6ed440 o1
---
reset
obj 1 0x30
obj 2 0x80
obj 3 0x80
obj 4 0x150
ptr 1 0x20 2
ptr 2 4 3
ptr 3 4 4
u8 4 0x13f 1
u32 4 0x70 0x20
u32 2 0x1c 0x30
watch 2 0x1c
mode full
run 0x6ed440 o1
---
reset
obj 1 0x30
obj 2 0x80
obj 3 0x80
obj 4 0x150
ptr 1 0x20 2
ptr 2 4 3
ptr 3 4 4
u8 4 0x13f 0
u32 2 0x64 0
mode full
run 0x6ed440 o1
---
reset
obj 1 0x30
obj 2 0x80
obj 3 0x90
ptr 1 0x18 2
ptr 2 0x24 3
u32 1 0x28 0x99
u32 3 0x80 0
watch 1 0x28
watch 3 0x80
mode full
run 0x6ed4a0 o1 0
---
reset
obj 1 0x30
obj 2 0x30
obj 3 0x90
ptr 1 0x18 2
ptr 2 0x24 3
u32 3 0x80 0x4000
ret 0x2a
watch 3 0x80
mode full
run 0x6ed4a0 o1 0
---
reset
obj 1 0x30
obj 2 0x80
obj 9 0x80
ptr 1 0x20 2
gclock 9
u32 9 0x58 0x10
u32 1 0x28 0x9
u32 1 0x24 0x8
u32 1 8 0
u32 2 0x1c 0
watch 1 8
mode full
run 0x6ed4e0 o1
---
reset
obj 1 0x30
obj 2 0x80
obj 9 0x80
ptr 1 0x20 2
gclock 9
u32 9 0x58 0x10
u32 1 0x28 0x8
u32 1 0x24 0x8
u32 1 8 0x5
u32 2 0x1c 0
obj 3 0x10
obj 4 0xb0
ptr 2 4 3
ptr 3 4 4
u32 4 0xac 0
watch 1 8
mode full
run 0x6ed4e0 o1
---
reset
obj 1 0x30
obj 2 0x80
obj 3 0x10
obj 4 0xb0
obj 9 0x80
ptr 1 0x20 2
ptr 2 4 3
ptr 3 4 4
gclock 9
u32 9 0x58 0x10
u32 1 0x24 0
u32 1 0x28 0
u32 2 0x1c 4
u32 2 0x24 0
u8 2 0x60 0
u32 4 0xac 0
watch 1 8
mode full
run 0x6ed4e0 o1
---
reset
obj 1 0x30
obj 2 0x80
obj 3 0x10
obj 4 0xb0
obj 9 0x80
ptr 1 0x20 2
ptr 2 4 3
ptr 3 4 4
gclock 9
u32 9 0x58 5
u32 1 0x24 1
u32 1 0x28 1
u32 2 0x1c 4
u32 2 0x24 1
u8 2 0x60 1
u32 4 0xac 0
watch 1 8
mode full
run 0x6ed4e0 o1
---
reset
obj 1 0x30
obj 2 0x80
obj 3 0x10
obj 4 0xb0
obj 5 0x30
obj 6 0x10
obj 9 0x80
ptr 1 0x20 2
ptr 2 4 3
ptr 3 4 4
ptr 1 0x18 5
ptr 5 0x24 6
gclock 9
u32 9 0x58 5
u32 1 0x24 1
u32 1 0x28 1
u32 2 0x1c 4
u32 2 0x24 3
u8 2 0x60 0
u32 4 0xac 0x200
ret 1
ret 0
watch 1 8
mode full
run 0x6ed4e0 o1
---
# gates and unified
reset
obj 1 0x60
obj 2 0x30
ptr 1 0x50 2
vt 2 0 14:n
ret 0
mode full
run 0x6f6c10 o1
---
reset
obj 1 0x60
obj 2 0x30
ptr 1 0x50 2
vt 2 0 14:n
ret 1
ret 0x33
mode full
run 0x6f6c10 o1
---
reset
obj 1 0x60
obj 2 0x30
ptr 1 0x50 2
vt 2 0 10:n
ret 0x105
mode full
run 0x6f6c70 o1
---
reset
obj 1 0x60
obj 2 0x30
ptr 1 0x50 2
vt 2 0 20:n
ret 1
ret 0x41
mode full
run 0x6f6c90 o1
---
reset
obj 1 0x60
obj 2 0x30
ptr 1 0x50 2
vt 2 0 18:n,1c:n
ret 0
mode full
run 0x6f6c30 o1
---
reset
obj 1 0x60
obj 2 0x30
ptr 1 0x50 2
vt 2 0 1c:n
ret 1
ret 0x19
mode full
run 0x6f6c50 o1
---
reset
obj 1 0x20
vt 1 0 58:m
u32 1 0x14 0x3f800000
ret 0
mode al
run 0x6f8a60 o1 0
---
reset
obj 1 0x20
obj 2 0x10
vt 1 0 58:m
u32 1 0x14 0x3f800000
patch 0
ret 0
ret 0x28
mode full
run 0x6f8a60 o1 @2
---
reset
obj 1 0x20
obj 2 0x10
vt 1 0 58:m
u32 1 0x14 0
patch 0x3f800000
ret 0
ret 0x29
mode full
run 0x6f8a60 o1 @2
---
# horde contain, heal, open, spawn, ai
reset
obj 1 0x30
ret 0
mode full
run 0x6f94b0 o1+0x1c
---
reset
obj 1 0x30
obj 2 0x340
obj 3 0x20
ptr 1 0 2
ptr 2 0x328 3
vt 3 0 b0:n
ret @2
ret 0x53
mode full
run 0x6f94b0 o1+0x1c
---
reset
obj 1 0x20
ret 0
mode full
run 0x6f94f0 o1+0x1c
---
reset
obj 1 0x30
obj 2 0x340
obj 3 0x20
ptr 1 0 2
ptr 2 0x328 3
vt 3 0 28:n
ret @2
ret 0x54
mode full
run 0x6f94f0 o1+0x1c
---
reset
obj 1 0x1c0
obj 2 0x10
u32 1 0 7
ret 7
mode full
run 0x6f9710 o1+0x164 @2
---
reset
obj 1 0x1c0
obj 2 0x100
u32 1 0 1
u32 2 0xdc 0x80
ret 2
ret 0
mode full
run 0x6f9710 o1+0x164 @2
---
reset
obj 1 0x1c0
obj 2 0x100
u32 1 0 1
u32 2 0xdc 0x80
ret 2
ret 1
mode al
run 0x6f9710 o1+0x164 @2
---
reset
obj 1 0x10
mode full
run 0x6f9b00 o1 0 2 3
---
reset
obj 1 0x10
ret 0
mode full
run 0x6f9b00 o1 1 2 3
---
reset
obj 1 0x10
ret 0x101
mode full
run 0x6f9b00 o1 1 2 3
---
reset
obj 1 0x30
u32 1 0 0
mode full
run 0x6f9b40 o1+0x1c 1 2 3
---
reset
obj 1 0x30
obj 2 0x400
ptr 1 0 2
u32 2 0x3ec 0
mode full
run 0x6f9b40 o1+0x1c 1 2 3
---
reset
obj 1 0x30
obj 2 0x400
obj 3 0x20
ptr 1 0 2
u32 2 0x3ec 1
ptr 2 0x328 3
vt 3 0 70:n
ret @2
ret 0x61
mode full
run 0x6f9b40 o1+0x1c 1 2 3
---
reset
obj 1 0x760
obj 9 0x80
gclock 9
u32 9 0x58 0x70
ret 0x15
watch 1 0x750
mode full
run 0x6fa9e0 o1 4
---
reset
obj 1 0x30
vt 1 0 ac:q
ret 0
mode full
run 0x6fbb50 o1
---
reset
obj 1 0x40
vt 1 0x1c ac:q
u32 1 0 0
ret 2
ret 0
mode full
run 0x6fbb50 o1+0x1c
---
reset
obj 1 0x40
obj 2 0x340
obj 3 0x20
vt 1 0x1c ac:q
ptr 1 0 2
ptr 2 0x328 3
vt 3 0 b0:n
ret 2
ret @2
ret @2
ret 0x62
mode full
run 0x6fbb50 o1+0x1c
---
reset
obj 1 0x20
u32 1 8 0
mode full
run 0x6fcad0 o1
---
reset
obj 1 0x20
u32 1 8 1
ret 0x18
mode full
run 0x6fcad0 o1
---
reset
obj 1 0x10
ret 0
mode full
run 0x6fda30 o1
---
reset
obj 1 0x10
ret 0x110
mode full
run 0x6fda30 o1
---
reset
obj 1 0x30
u32 1 0x2c 2
mode full
run 0x6fe070 o1
---
reset
obj 1 0x30
obj 2 0x20
ptr 1 4 2
u32 1 0x2c 0
u32 2 0x10 0x4b189681
mode al
run 0x6fe070 o1
---
reset
obj 1 0x30
obj 2 0x20
ptr 1 4 2
u32 1 0x2c 0
u32 2 0x10 0
ret 0
mode full
run 0x6fe070 o1
---
reset
obj 1 0x30
obj 2 0x20
ptr 1 4 2
u32 1 0x2c 0
u32 2 0x10 0
ret 4
mode full
run 0x6fe070 o1
---
reset
obj 1 0x40
u32 1 0x30 0
u32 1 0x34 0
mode full
run 0x6ff060 o1
---
reset
obj 1 0x580
obj 5 0x100
obj 6 0x80
ptr 1 4 5
ptr 1 0 6
u32 5 0xf4 0
ext 1 0x548 1 0x560
ext 1 0x54c 1 0x564
u32 1 0x560 0x99
mode full
run 0x6ff060 o1+0x518
---
reset
obj 1 0x580
obj 5 0x100
obj 6 0x80
obj 7 0x10
ptr 1 4 5
ptr 1 0 6
ptr 5 0xf4 7
u32 6 0x4c 0x3
ext 1 0x548 1 0x560
ext 1 0x54c 1 0x564
u32 1 0x560 0x99
mode full
run 0x6ff060 o1+0x518
---
reset
obj 1 0x80
obj 2 0x20
ext 1 0x44 1 0x44
mode full
run 0x6f7db0 o1
---
reset
obj 1 0x80
obj 2 0x20
obj 3 0x20
obj 9 0x200
ext 1 0x44 2 0
ext 2 0 1 0x44
u32 2 8 0x42
gclock 9
map 0x42 3
ret 0
mode full
run 0x6f7db0 o1
---
reset
obj 1 0x80
obj 2 0x20
obj 3 0x20
obj 9 0x200
ext 1 0x44 2 0
ext 2 0 1 0x44
u32 2 8 0x42
gclock 9
map 0x42 3
ret 1
mode full
run 0x6f7db0 o1
---
# allows move, locomotor, order
reset
obj 1 0x480
obj 2 0x480
ptr 1 8 2
u8 2 0x471 1
mode al
run 0x6ff9f0 o1 0
---
reset
obj 1 0x480
obj 2 0x480
obj 3 0x10
ptr 1 8 2
u8 2 0x471 0
u32 1 0x250 0xfffffffe
mode al
run 0x6ff9f0 o1 0
---
reset
obj 1 0x480
obj 2 0x480
obj 3 0x10
obj 4 0x90
obj 5 0xd0
ptr 1 8 2
ptr 1 0x3c 4
ptr 2 4 5
u8 2 0x471 0
u32 1 0x250 0
u32 3 4 2
u32 4 0x88 0
u32 4 0x8c 0
u32 5 0xc0 0
mode al
run 0x6ff9f0 o1 @3
---
reset
obj 1 0x480
obj 2 0x480
obj 3 0x10
obj 4 0x90
obj 5 0xd0
obj 9 0xc0
ptr 1 8 2
ptr 1 0x3c 4
ptr 2 4 5
u8 2 0x471 0
u32 1 0x250 0
u32 3 0 1
u32 3 4 2
u8 1 0x43e 0
u8 1 0x43f 0
u32 2 0xcc 0
u32 2 0x90 0
u32 4 0x88 0
u32 5 0xc0 0x4002
gclock 9
u8 9 0xac 0
mode al
run 0x6ff9f0 o1 @3
---
reset
obj 1 0x480
obj 2 0x480
obj 3 0x10
obj 4 0x90
obj 5 0xd0
obj 9 0xc0
ptr 1 8 2
ptr 1 0x3c 4
ptr 2 4 5
u8 2 0x471 0
u32 1 0x250 0
u32 3 0 0
u32 3 4 0
u8 1 0x43e 0
u8 1 0x43f 0
u32 2 0xcc 0
u32 2 0x90 0
u32 4 0x88 0
u32 5 0xc0 0
gclock 9
st0 0
mode al
run 0x6ff9f0 o1 @3
---
reset
obj 1 0x480
obj 2 0x480
obj 3 0x10
obj 4 0x90
obj 5 0xd0
obj 9 0xc0
ptr 1 8 2
ptr 1 0x3c 4
ptr 2 4 5
u8 2 0x471 0
u32 1 0x250 0
u32 3 0 5
u32 3 4 0
u8 1 0x43e 0
u8 1 0x43f 0
u32 2 0xcc 0
u32 2 0x90 0
u32 4 0x88 0
u32 5 0xc0 0
gclock 9
st0 0x3f800000
mode al
run 0x6ff9f0 o1 @3
---
reset
obj 1 0x40
obj 2 0x480
ptr 1 8 2
u8 2 0x471 1
mode al
run 0x6ffad0 o1
---
reset
obj 1 0x40
obj 2 0x480
obj 3 0x10
ptr 1 8 2
ptr 1 0x3c 3
u8 2 0x471 0
u32 3 4 0
mode al
run 0x6ffad0 o1
---
reset
obj 1 0x40
obj 2 0x480
obj 3 0x20
obj 4 0x30
ptr 1 8 2
ptr 1 0x3c 3
ptr 3 4 4
u8 2 0x471 0
vt 4 0 20:n
ret 0x22
mode al
run 0x6ffad0 o1
---
reset
obj 1 0x400
obj 2 0x20
ptr 1 8 2
ret 0
mode full
run 0x6ffd50 o1 0 0x10
---
reset
obj 1 0x400
obj 2 0x40
obj 3 0x20
ptr 1 0x3c 2
vt 2 0 34:n,38:p,1c:u
ret 0
ret 0
ret 0
watch 1 0x50,0x308,0x368,0x3c8
mode full
run 0x6ffd50 o1 @3 0x10
---
'''.strip().split("\n---\n")


def main() -> None:
    checker = Checker(EXE)
    oracle = Oracle(checker)
    mismatches = 0
    try:
        for index, block in enumerate(CASES, 1):
            lines = [line.strip() for line in block.splitlines() if line.strip() and not line.strip().startswith("#")]
            run = lines[-1]
            try:
                for line in lines:
                    checker.send(line)
                    oracle.apply(line)
                    if line.startswith("run "):
                        parts = line.split()
                        port = checker.read()
                        native = oracle.execute(int(parts[1], 0), parts[2], parts[3:])
                        bad = []
                        for key in ("log", "mem"):
                            if native[key] != port.get(key):
                                bad.append(key)
                        if oracle.mode == "full" and native["eax"] != port.get("eax"):
                            bad.append("eax")
                        if oracle.mode == "al" and native["al"] != port.get("al"):
                            bad.append("al")
                        if oracle.mode == "mix" and native["mix"] != port.get("mix"):
                            bad.append("mix")
                        if bad:
                            mismatches += 1
                            if mismatches <= 25:
                                print(f"MISMATCH {index} {run} fields={','.join(bad)}")
                                print(" native", native)
                                print(" port  ", port)
            except Exception as error:
                mismatches += 1
                if mismatches <= 25:
                    print(f"FAIL {index} {run}: {error}")
    finally:
        checker.close()
    print(f"cases={len(CASES)} mismatches={mismatches}")
    if mismatches:
        sys.exit(1)


if __name__ == "__main__":
    main()
