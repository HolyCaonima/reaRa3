"""Differentially verify the render91 W3DScriptedModelDraw cluster against ra3_1.0.game."""

from __future__ import annotations

import argparse
import random
import struct
import subprocess
import sys
from pathlib import Path
from typing import Callable

import unicorn
from unicorn import x86_const

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from emulate import CallResult, Emulator  # noqa: E402

ACCEPTED_ENTRIES = (
    0x008E52A0, 0x008E5690, 0x008E5A60, 0x008E8900,
    0x008E8D50, 0x008E8DC0, 0x008E8E90, 0x008E8EC0,
    0x008E8FD0, 0x008E9000, 0x008E9020, 0x008E9050,
    0x008E9240, 0x008E9270, 0x008E93B0, 0x008EDA20,
    0x008EDB30, 0x008EE860, 0x008EE8E0, 0x008EE990,
    0x008EEED0, 0x008F0B90, 0x008F2700, 0x008F2C10,
    0x008F3D60, 0x008F5CB0, 0x008F6ED0, 0x008F95D0,
    0x00910E50, 0x00910E60, 0x00910E70, 0x00910E90,
    0x00910EB0, 0x00910ED0, 0x00910EE0, 0x00911250,
)

X87_CONTROL_WORDS = (0x007F, 0x027F, 0x037F)
SENTINEL_PARTICLE_HANDLE_VA = 0x00DF88F8


def u32(data: bytearray | bytes, offset: int) -> int:
    return struct.unpack_from("<I", data, offset)[0]


def put_u32(data: bytearray, offset: int, value: int) -> None:
    struct.pack_into("<I", data, offset, value & 0xFFFFFFFF)


def put_i32(data: bytearray, offset: int, value: int) -> None:
    struct.pack_into("<i", data, offset, value)


def put_f32(data: bytearray, offset: int, value: float) -> None:
    struct.pack_into("<f", data, offset, value)


def fbits(value: float) -> int:
    return struct.unpack("<I", struct.pack("<f", value))[0]


class CppOracle:
    def __init__(self, executable: Path) -> None:
        self.process = subprocess.Popen(
            [str(executable), "--batch"],
            stdin=subprocess.PIPE,
            stdout=subprocess.PIPE,
            text=True,
            bufsize=1,
        )

    def call(
        self,
        va: int,
        self_offset: int,
        blob: bytes,
        fixups: list[str],
        a0: int = 0,
        a1: int = 0,
        a2: int = 0,
        fpcw: int = 0x037F,
    ) -> tuple[int, bytes]:
        assert self.process.stdin is not None and self.process.stdout is not None
        fix_str = ",".join(fixups) if fixups else "-"
        line = (
            f"0x{va:08X} 0x{self_offset:X} {blob.hex()} {fix_str} "
            f"0x{a0 & 0xFFFFFFFF:X} 0x{a1 & 0xFFFFFFFF:X} 0x{a2 & 0xFFFFFFFF:X} 0x{fpcw:04X}\n"
        )
        self.process.stdin.write(line)
        self.process.stdin.flush()
        out = self.process.stdout.readline().strip()
        if not out.startswith("ret=0x"):
            raise RuntimeError(f"ra3_render91check failed on 0x{va:08X}: {out}")
        parts = out.split()
        ret_val = int(parts[0].split("=")[1], 16)
        out_blob = bytes.fromhex(parts[1].split("=")[1])
        return ret_val, out_blob

    def close(self) -> None:
        if self.process.stdin is not None:
            self.process.stdin.close()
        self.process.wait(timeout=10)


class Verifier:
    def __init__(self, executable: Path) -> None:
        self.emu = Emulator()
        self.cpp = CppOracle(executable)
        self.counts: dict[int, int] = {}
        self.vstubs: dict[int, int] = {}
        self._bind_vstubs()
        # 0x008E57E0 uses custom LTCG register convention: ESI = animState, EDX = triggerFrame.
        self.emu.uc.hook_add(
            unicorn.UC_HOOK_CODE,
            lambda uc, addr, sz, data: uc.reg_write(
                x86_const.UC_X86_REG_ESI, uc.reg_read(x86_const.UC_X86_REG_ECX)
            ),
            begin=0x008E57E0,
            end=0x008E57E0,
        )

    def _bind_vstubs(self) -> None:
        def vstub0(emu: Emulator) -> int:
            self_ptr = emu.register(x86_const.UC_X86_REG_ECX)
            cur = struct.unpack("<I", emu.read(self_ptr + 0x08, 4))[0]
            emu.write(self_ptr + 0x08, struct.pack("<I", (cur + 0x11110001) & 0xFFFFFFFF))
            return 0

        def vstub1(emu: Emulator) -> int:
            self_ptr = emu.register(x86_const.UC_X86_REG_ECX)
            arg = emu.stack_arg(0)
            emu.write(self_ptr + 0x08, struct.pack("<I", arg & 0xFFFFFFFF))
            cur = struct.unpack("<I", emu.read(self_ptr + 0x14, 4))[0]
            emu.write(
                self_ptr + 0x14,
                struct.pack("<I", (cur + 0x100 + (arg & 0xFF)) & 0xFFFFFFFF),
            )
            return 0

        def vstub2(emu: Emulator) -> int:
            self_ptr = emu.register(x86_const.UC_X86_REG_ECX)
            arg = emu.stack_arg(0)
            emu.write(self_ptr + 0x0C, struct.pack("<I", arg & 0xFFFFFFFF))
            cur = struct.unpack("<I", emu.read(self_ptr + 0x14, 4))[0]
            emu.write(
                self_ptr + 0x14,
                struct.pack("<I", (cur + 0x200 + (arg & 0xFF)) & 0xFFFFFFFF),
            )
            return 0

        def vstub3(emu: Emulator) -> int:
            self_ptr = emu.register(x86_const.UC_X86_REG_ECX)
            vec_ptr = emu.stack_arg(0)
            if vec_ptr != 0:
                emu.write(self_ptr + 0x08, emu.read(vec_ptr, 12))
            cur = struct.unpack("<I", emu.read(self_ptr + 0x14, 4))[0]
            emu.write(self_ptr + 0x14, struct.pack("<I", (cur + 0x300) & 0xFFFFFFFF))
            return 0

        def vstub4(emu: Emulator) -> int:
            self_ptr = emu.register(x86_const.UC_X86_REG_ECX)
            flags_ptr = emu.stack_arg(0)
            if flags_ptr != 0:
                b0, b1 = emu.read(flags_ptr, 2)
                packed = 0x44440000 | (b1 << 8) | b0
                emu.write(self_ptr + 0x08, struct.pack("<I", packed))
            cur = struct.unpack("<I", emu.read(self_ptr + 0x14, 4))[0]
            emu.write(self_ptr + 0x14, struct.pack("<I", (cur + 0x400) & 0xFFFFFFFF))
            return 0

        def vstub5(emu: Emulator) -> int:
            self_ptr = emu.register(x86_const.UC_X86_REG_ECX)
            arg_ptr = emu.stack_arg(0)
            cur = struct.unpack("<I", emu.read(self_ptr + 0x08, 4))[0]
            emu.write(
                self_ptr + 0x08,
                struct.pack("<I", (cur + 1 + (0x10 if arg_ptr != 0 else 0)) & 0xFFFFFFFF),
            )
            emu.write(self_ptr + 0x10, struct.pack("<I", arg_ptr & 0xFFFFFFFF))
            return emu.read(self_ptr + 0x0C, 1)[0]

        self.vstubs[0] = self.emu.bind_stub(vstub0, 0)
        self.vstubs[1] = self.emu.bind_stub(vstub1, 4)
        self.vstubs[2] = self.emu.bind_stub(vstub2, 4)
        self.vstubs[3] = self.emu.bind_stub(vstub3, 4)
        self.vstubs[4] = self.emu.bind_stub(vstub4, 4)
        self.vstubs[5] = self.emu.bind_stub(vstub5, 4)

    def check(
        self,
        va: int,
        self_offset: int,
        blob: bytes | bytearray,
        fixups: list[str],
        invoke: Callable[[Emulator, int], tuple[CallResult, int | None]],
        cleanup: int,
        *,
        a0: int = 0,
        a1: int = 0,
        a2: int = 0,
        fpcw: int = 0x037F,
        compare_ret: str = "none",  # "none", "u8", "u32", "ptr"
    ) -> None:
        raw_blob = bytes(blob)
        cpp_ret, cpp_blob = self.cpp.call(va, self_offset, raw_blob, fixups, a0, a1, a2, fpcw)

        self.emu.reset_data()
        self.emu.uc.reg_write(x86_const.UC_X86_REG_FPCW, fpcw)
        base = self.emu.alloc(raw_blob)
        for item in fixups:
            if item.startswith("v") and "@" in item:
                k_str, off_str = item[1:].split("@", 1)
                kind = int(k_str, 0)
                off = int(off_str, 0)
                self.emu.write(base + off, struct.pack("<I", self.vstubs[kind]))
            else:
                off = int(item, 0)
                rel = u32(raw_blob, off)
                self.emu.write(base + off, struct.pack("<I", (base + rel) & 0xFFFFFFFF))

        res, game_ret = invoke(self.emu, base)
        if res.stack_cleanup != cleanup:
            raise AssertionError(
                f"0x{va:08X}: stack cleanup {res.stack_cleanup}, expected {cleanup}"
            )

        game_blob = bytearray(self.emu.read(base, len(raw_blob)))
        vstub_rev = {addr: k for k, addr in self.vstubs.items()}
        for off in range(0, len(game_blob) - 3, 4):
            cur = u32(game_blob, off)
            if base <= cur < base + len(game_blob):
                put_u32(game_blob, off, cur - base)
            elif cur in vstub_rev:
                put_u32(game_blob, off, 0xF0000000 | vstub_rev[cur])

        if bytes(game_blob) != cpp_blob:
            for idx, (g, c) in enumerate(zip(game_blob, cpp_blob)):
                if g != c:
                    s = max(0, idx - 4)
                    e = min(len(game_blob), idx + 8)
                    raise AssertionError(
                        f"0x{va:08X}: blob mismatch at +0x{idx:X}: "
                        f"game=0x{g:02X} cpp=0x{c:02X} "
                        f"(game={game_blob[s:e].hex()} cpp={cpp_blob[s:e].hex()})"
                    )

        if compare_ret != "none":
            assert game_ret is not None
            if compare_ret == "u8":
                g_val = game_ret & 0xFF
                c_val = cpp_ret & 0xFF
            elif compare_ret == "ptr":
                if game_ret == 0:
                    g_val = 0
                elif base <= game_ret < base + len(raw_blob):
                    g_val = game_ret - base
                else:
                    g_val = game_ret & 0xFFFFFFFF
                c_val = cpp_ret & 0xFFFFFFFF
            else:
                g_val = game_ret & 0xFFFFFFFF
                c_val = cpp_ret & 0xFFFFFFFF
            if g_val != c_val:
                raise AssertionError(
                    f"0x{va:08X}: return mismatch ({compare_ret}): "
                    f"game=0x{g_val:08X} cpp=0x{c_val:08X}"
                )

        self.counts[va] = self.counts.get(va, 0) + 1

    def close(self) -> None:
        self.cpp.close()


NULL = 0xFFFFFFFF


def _fill(size: int, value: int = 0xA5) -> bytearray:
    return bytearray([value]) * size


def _refs(blob: bytearray, obj: int, count: int, slot: int = 4) -> None:
    put_u32(blob, obj + slot, count)


def verify(executable: Path) -> None:
    oracle = Verifier(executable)

    def run(
        va: int,
        blob: bytearray,
        stack,
        cleanup: int,
        *,
        self_off: int = 0,
        a0: int = 0,
        a1: int = 0,
        a2: int = 0,
        fixups: list[str] | None = None,
        compare: str = "none",
        edx: int = 0,
        eax_off: int | None = None,
    ) -> None:
        if va not in ACCEPTED_ENTRIES:
            return

        def invoke(machine: Emulator, base: int):
            ecx = 0 if self_off >= len(blob) else base + self_off
            eax = 0 if eax_off is None else base + eax_off
            args = stack(base) if callable(stack) else stack
            result = machine.call(va, args, ecx=ecx, edx=edx & 0xFFFFFFFF, eax=eax)
            return result, result.eax

        oracle.check(
            va, self_off, blob, fixups or [], invoke, cleanup,
            a0=a0, a1=a1, a2=a2, compare_ret=compare)

    def ptrs(base: int, *offsets: int) -> list[int]:
        return [0 if off >= 0x80000000 else base + off for off in offsets]

    # 0x008E52A0 field write, including a zero that must replace a sentinel.
    for value in (0x11223344, 0):
        blob = _fill(0x300)
        run(0x008E52A0, blob, lambda _b, v=value: [v], 4, a0=value)

    # 0x008E5690 terrain-decal release. Slot +8 is a no-argument virtual.
    blob = _fill(0x80)
    put_u32(blob, 0x74, 0)
    run(0x008E5690, blob, [], 0)
    blob = _fill(0xA0)
    put_u32(blob, 0x74, 0x40)
    put_u32(blob, 0x40, 0x80)
    run(0x008E5690, blob, [], 0, fixups=["0x74", "0x40", "v0@0x88"])

    # 0x008E57E0 custom ESI/EDX frame-crossing predicate.
    def trigger(cur: float, prev: float, mode: int, direction: int, frame: int) -> None:
        blob = _fill(0x20)
        put_f32(blob, 4, cur)
        put_f32(blob, 8, prev)
        put_u32(blob, 0x10, mode)
        put_i32(blob, 0x14, direction)
        run(0x008E57E0, blob, [], 0, a0=frame & 0xFFFFFFFF, edx=frame, compare="u8")

    trigger(3.0, 3.0, 1, 1, 4)
    trigger(4.0, 1.0, 1, 1, 4)
    trigger(4.0, 1.0, 1, 1, 2)
    trigger(4.0, 1.0, 1, 1, 1)
    trigger(1.0, 4.0, 1, 1, 5)
    trigger(4.0, 1.0, 2, 1, 2)
    trigger(1.0, 4.0, 2, 0, 2)
    trigger(1.0, 4.0, 3, 1, 5)
    trigger(4.0, 1.0, 5, 1, 2)
    trigger(4.0, 1.0, 0, 0, 2)

    # 0x008E5A60 one direct slot-6 call plus 0x0078B730's three more and slot 0x21.
    blob = _fill(0x220)
    put_u32(blob, 0x40, 0x180)
    blob[0x4C] = 0x3C
    run(
        0x008E5A60, blob, lambda b: [b + 0x40], 4, a0=0x40,
        fixups=["0x40", "v4@0x198", "v5@0x204"], compare="u8")
    blob = _fill(0x220)
    put_u32(blob, 0x40, 0x180)
    blob[0x4C] = 0
    run(
        0x008E5A60, blob, lambda b: [b + 0x40], 4, a0=0x40,
        fixups=["0x40", "v4@0x198", "v5@0x204"], compare="u8")

    # Particle-system slot assignment and 8-byte record copies.
    def particle_sys(blob: bytearray, obj: int, ref: int) -> None:
        _refs(blob, obj, ref, 0x24)

    blob = _fill(0x60)
    put_u32(blob, 0, 0)
    put_u32(blob, 0x20, 0x30)
    particle_sys(blob, 0x30, 2)
    run(0x008E5E80, blob, lambda b: ptrs(b, 0x20), 4, a0=0x20, fixups=["0x20"], compare="ptr")
    blob = _fill(0x60)
    put_u32(blob, 0, 0x30)
    put_u32(blob, 0x20, 0x30)
    particle_sys(blob, 0x30, 4)
    run(0x008E5E80, blob, lambda b: ptrs(b, 0x20), 4, a0=0x20, fixups=["0x0", "0x20"], compare="ptr")
    blob = _fill(0x60)
    put_u32(blob, 4, 0)
    put_u32(blob, 0x10, 0x1111)
    put_u32(blob, 0x14, 0x30)
    particle_sys(blob, 0x30, 3)
    run(0x008E6060, blob, lambda b: ptrs(b, 0x10), 4, a0=0x10, fixups=["0x14"], compare="ptr")

    def records8(dst: int, src: int, count: int, backward: bool) -> None:
        blob = _fill(0x120 + count * 0x20)
        for i in range(count):
            put_u32(blob, dst + i * 8 + 4, 0)
        for i in range(count):
            put_u32(blob, src + i * 8, 0x1000 + i)
            sys = 0xC0 + i * 0x20
            put_u32(blob, src + i * 8 + 4, sys)
            particle_sys(blob, sys, 3)
        last = src + count * 8
        dst_last = dst + count * 8
        va = 0x008E6260 if backward else 0x008E6180
        a2 = dst_last if backward else dst
        run(
            va, blob, lambda b, s=src, e=last, d=a2: ptrs(b, s, e, d), 0,
            a0=src, a1=last, a2=a2,
            fixups=[f"0x{src + i * 8 + 4:X}" for i in range(count)], compare="ptr")

    records8(0x40, 0x10, 0, False)
    records8(0x40, 0x10, 1, False)
    records8(0x40, 0x10, 2, False)
    records8(0x18, 0x10, 2, False)
    records8(0x40, 0x10, 0, True)
    records8(0x40, 0x10, 2, True)

    # 0x008E8550 zero / negative-zero stay put; any other component notifies slot 0.
    def truck(x: float, y: float, z: float, call: bool) -> None:
        blob = _fill(0x300)
        put_f32(blob, 0x2C8, x)
        put_f32(blob, 0x2CC, y)
        put_f32(blob, 0x2D0, z)
        fixups: list[str] = []
        if call:
            put_u32(blob, 0x290, 0x100)
            fixups = ["0x290", "v3@0x100"]
        run(0x008E8550, blob, [], 0, fixups=fixups)

    truck(0.0, 0.0, 0.0, False)
    truck(-0.0, -0.0, -0.0, False)
    truck(1.0, 0.0, 0.0, True)
    truck(0.0, 0.0, float("nan"), True)

    def anim_names(count: int, index: int | None, entry_null: bool) -> None:
        blob = _fill(0xA0)
        put_u32(blob, 0, 0x20)
        put_u32(blob, 0x20 + 0x68, count)
        fixups = ["0x0"]
        if count:
            put_u32(blob, 0x20 + 0x6C, 0x40)
            fixups.append("0x8C")
            slot = 0 if index is None else index % count
            if entry_null:
                put_u32(blob, 0x40 + slot * 8, 0)
            else:
                put_u32(blob, 0x40 + slot * 8, 0x60)
                put_u32(blob, 0x64, 0xABC0 + slot)
                put_u32(blob, 0x68, 0xDEF0 + slot)
                fixups.append(f"0x{0x40 + slot * 8:X}")
        out = 0x70
        blob[out:out + 8] = b"\xFF" * 8
        if index is None:
            run(0x008E8900, blob, lambda b: [b + out], 4, a0=out, fixups=fixups, compare="ptr")
        else:
            run(
                0x008E8960, blob, lambda b, i=index: [b + out, i], 8,
                a0=out, a1=index, fixups=fixups, compare="ptr")

    anim_names(0, None, False)
    anim_names(2, None, False)
    anim_names(1, None, True)
    anim_names(0, 3, False)
    anim_names(3, 0, False)
    anim_names(3, 5, False)
    anim_names(3, 5, True)

    # 0x008E8D50 EAX is the layer. Modes 0/1/3/5/7 return the raw byte.
    def layer_case(slot_word: int, mode: int, flag: int, byte: int, frames: int, cur: float) -> None:
        blob = _fill(0x60)
        put_u32(blob, 0, slot_word)
        put_u32(blob, 0x10, 0x30 if slot_word else 0)
        put_u32(blob, 0x20, mode)
        blob[0x28] = byte
        put_i32(blob, 0x44, frames)
        put_f32(blob, 0x14, cur)
        fixups = ["0x10"] if slot_word else []
        run(
            0x008E8D50, blob, lambda _b, f=flag: [f], 0,
            a0=0x10, a1=flag, eax_off=0x10, fixups=fixups, compare="u8")

    layer_case(0, 0, 1, 0, 1, 0.0)
    layer_case(1, 0, 1, 0, 1, 0.0)
    # Null animation object: slot word is non-zero but the layer pointer is cleared below.
    blob = _fill(0x30)
    put_u32(blob, 0, 1)
    put_u32(blob, 0x10, 0)
    run(0x008E8D50, blob, lambda _b: [1], 0, a0=0x10, a1=1, eax_off=0x10, compare="u8")
    layer_case(1, 9, 1, 2, 1, 0.0)
    layer_case(1, 0, 0, 2, 1, 0.0)
    layer_case(1, 0, 1, 2, 1, 0.0)
    layer_case(1, 0, 1, 0, 1, 0.0)
    layer_case(1, 2, 0, 0, 10, 9.0)
    layer_case(1, 2, 0, 0, 10, 8.0)
    layer_case(1, 4, 0, 0, 10, 0.0)
    layer_case(1, 4, 0, 0, 10, 0.5)
    layer_case(1, 6, 0, 9, 1, 0.0)

    def module_key(override: int, module_key_value: int) -> None:
        blob = _fill(0x270)
        put_u32(blob, 0x25C, override)
        put_u32(blob, 4, 0x40)
        put_u32(blob, 0x44, module_key_value)
        run(0x008E8DC0, blob, lambda b: [b + 0x260], 4, a0=0x260, fixups=["0x4"], compare="ptr")

    module_key(0x99, 0x11)
    module_key(0, 0x42)

    def shadow_pair(va: int, slot: int, cleanup: int, args) -> None:
        blob = _fill(0x220)
        put_u32(blob, slot, 0)
        run(va, blob, args(0), cleanup, a0=args(0)[0] if args(0) else 0, a1=args(0)[1] if len(args(0)) > 1 else 0)
        blob = _fill(0x220)
        put_u32(blob, slot, 0x40)
        put_u32(blob, 0x40, 0x100)
        values = args(1)
        run(
            va, blob, lambda _b, v=values: v, cleanup,
            a0=values[0] if values else 0, a1=values[1] if len(values) > 1 else 0,
            fixups=["0x{:X}".format(slot), "0x40", "v1@0x108", "v2@0x10C"])

    shadow_pair(0x008E8E90, 0x1F0, 8, lambda n: [0x10, 0x20] if n else [0, 0])
    shadow_pair(0x008E9020, 0x1F4, 8, lambda n: [0x7, 0x8] if n else [0, 0])

    def shadow_tail(va: int, slot: int, arg: int | None) -> None:
        cleanup = 0 if arg is None else 4
        blob = _fill(0x220)
        put_u32(blob, slot, 0)
        run(va, blob, [] if arg is None else [arg], cleanup, a0=0 if arg is None else arg)
        blob = _fill(0x220)
        put_u32(blob, slot, 0x40)
        put_u32(blob, 0x40, 0x100)
        stub = "v0@0x104" if arg is None else "v1@0x114"
        run(
            va, blob, [] if arg is None else [arg], cleanup,
            a0=0 if arg is None else arg,
            fixups=[f"0x{slot:X}", "0x40", stub])

    shadow_tail(0x008E8EC0, 0x1F0, 0x55)
    shadow_tail(0x008E9050, 0x1F4, 0)
    shadow_tail(0x008E9000, 0x200, None)

    def release_shadow(va: int, slot: int, present: bool, ref: int) -> None:
        blob = _fill(0x220)
        if present:
            put_u32(blob, slot, 0x40)
            _refs(blob, 0x40, ref)
            fixups = [f"0x{slot:X}"]
        else:
            put_u32(blob, slot, 0)
            fixups = []
        run(va, blob, [], 0, fixups=fixups)

    release_shadow(0x008E8FD0, 0x1F0, False, 2)
    release_shadow(0x008E8FD0, 0x1F0, True, 2)
    release_shadow(0x008E9240, 0x1F4, False, 3)
    release_shadow(0x008E9240, 0x1F4, True, 3)
    release_shadow(0x008E9270, 0x200, False, 2)
    release_shadow(0x008E9270, 0x200, True, 2)
    # Refcount 1 calls the deleting virtual at slot 0. Install it only for that case.
    blob = _fill(0x220)
    put_u32(blob, 0x1F0, 0x40)
    put_u32(blob, 0x40, 0x100)
    _refs(blob, 0x40, 1)
    run(0x008E8FD0, blob, [], 0, fixups=["0x1F0", "0x40", "v0@0x100"])

    def house(flag: int, same: bool) -> None:
        blob = _fill(0x1E0)
        put_u32(blob, 0x10, 0x100)
        blob[0x100 + 0xC4] = flag
        mask = bytes.fromhex("03000000" + "11223344" + "00000000" + "00000000")
        other = mask if same else bytes.fromhex("01000000" + "AABBCCDD" + "00000000" + "00000000")
        blob[0x50:0x60] = mask
        blob[0xB0:0xC0] = other
        put_u32(blob, 0x18, 0)
        put_u32(blob, 0x80, 0)
        run(0x008E93B0, blob, lambda b: [b + 0xB0], 4, self_off=0x20, a0=0xB0, fixups=["0x10"])

    house(0, False)
    house(1, True)
    house(1, False)

    def uninit8(count: int) -> None:
        blob = _fill(0x180)
        src, dst, out = 0x20, 0x80, 0x00
        for i in range(count):
            put_u32(blob, src + i * 8, 0x50 + i)
            sys = 0x100 + i * 0x28
            put_u32(blob, src + i * 8 + 4, sys)
            particle_sys(blob, sys, 2)
        last = src + count * 8
        run(
            0x008EB120, blob,
            lambda b, s=src, e=last, d=dst: [b + out, b + s, b + e, b + d], 0,
            self_off=out, a0=src, a1=last, a2=dst,
            fixups=[f"0x{src + i * 8 + 4:X}" for i in range(count)])

    uninit8(0)
    uninit8(1)
    uninit8(2)

    def uninit28(count: int) -> None:
        blob = _fill(0xC0 + count * 0x20)
        src, dst, out = 0x10, 0x80, 0x00
        for i in range(count):
            blob[src + i * 0x1C: src + i * 0x1C + 0x1C] = bytes([0x30 + i]) * 0x1C
        last = src + count * 0x1C
        run(
            0x008EB360, blob,
            lambda b, s=src, e=last, d=dst: [b + out, b + s, b + e, b + d], 0,
            self_off=out, a0=src, a1=last, a2=dst)

    uninit28(0)
    uninit28(1)
    uninit28(2)

    def spline_copy(count: int) -> None:
        blob = _fill(0x20 + max(count, 1) * 0xC0)
        src, dst, out = 0x10, 0x70, 0x00
        for i in range(count):
            for word in range(0x50 // 4):
                put_u32(blob, src + i * 0x50 + word * 4, 0x3F800000 + i * 0x10 + word)
        last = src + count * 0x50
        run(
            0x008EB490, blob,
            lambda b, s=src, e=last, d=dst: [b + out, b + s, b + e, b + d], 0,
            self_off=out, a0=src, a1=last, a2=dst, compare="ptr")

    spline_copy(0)
    spline_copy(1)

    blob = _fill(0x80)
    put_u32(blob, 0, 0x1234)
    for word in range(1, 13):
        put_f32(blob, 0x40 + word * 4, (-1.0) ** word * (word + 0.25))
    put_u32(blob, 0x40, 0x99)
    put_u32(blob, 0x40 + 0x34, 0xABCDEF01)
    put_f32(blob, 0x40 + 4, -0.0)
    run(0x008EB4E0, blob, lambda b: [b + 0x40], 4, a0=0x40)

    def render68_copy(count: int) -> None:
        blob = _fill(0x20 + max(count, 1) * 0xB0)
        src, dst, out = 0x10, 0x60, 0x00
        for i in range(count):
            blob[src + i * 0x44: src + i * 0x44 + 0x44] = bytes([0x40 + i]) * 0x44
        last = src + count * 0x44
        run(
            0x008EB540, blob,
            lambda b, s=src, e=last, d=dst: [b + out, b + s, b + e, b + d], 0,
            self_off=out, a0=src, a1=last, a2=dst, compare="ptr")

    render68_copy(0)
    render68_copy(1)

    def state204(count: int) -> None:
        blob = _fill(0x20 + max(count, 1) * 0x1A0)
        src, dst_last = 0x10, 0x10 + max(count, 1) * 0xCC + 0x10 + max(count, 1) * 0xCC
        for i in range(count):
            blob[src + i * 0xCC: src + i * 0xCC + 0xCC] = bytes([0x11 + i]) * 0xCC
        last = src + count * 0xCC
        run(
            0x008EB5E0, blob, lambda b, s=src, e=last, d=dst_last: ptrs(b, s, e, d), 0,
            a0=src, a1=last, a2=dst_last, compare="ptr")

    state204(0)
    state204(1)

    blob = _fill(0x30)
    put_u32(blob, 4, 0)
    blob[0x10:0x14] = b"\xFF\xFF\xFF\xFF"
    run(0x008EC560, blob, lambda b: [b + 0x10], 4, a0=0x10)
    blob = _fill(0x40)
    put_u32(blob, 4, 0x20)
    _refs(blob, 0x20, 5)
    run(0x008EC560, blob, lambda b: [b + 0x10], 4, a0=0x10, fixups=["0x4"])

    def bone_find(bit: int, key: int, node_key: int) -> None:
        blob = _fill(0x140)
        blob[0x84] = bit
        put_u32(blob, 0x58, 0xA0 if bit & 1 else 0)
        put_u32(blob, 0x50, 0)
        put_u32(blob, 0xA0, 0)
        put_u32(blob, 0xA4, 0)
        put_u32(blob, 0xA8, 0x50)
        put_i32(blob, 0xB0, node_key)
        put_u32(blob, 0xE4, 4)
        fixups = ["0x58", "0xA8"] if bit & 1 else []
        run(0x008ED690, blob, lambda b, k=key: [k, b + 0x120], 8, a0=key & 0xFFFFFFFF, a1=0x120, fixups=fixups, compare="ptr")

    bone_find(0, 7, 7)
    bone_find(1, 0, 7)
    bone_find(1, 7, 7)
    bone_find(1, 9, 7)

    def propagate(entries: int, bone_index: int, parent_index: int, hide: int) -> None:
        blob = _fill(0x500)
        begin, bones, sub = 0x200, 0x300, 0x480
        put_u32(blob, 0x108, begin)
        put_u32(blob, 0x10C, begin + entries * 0x14)
        put_u32(blob, 0x17C, bones)
        put_u32(blob, 0x180, bones + 3 * 72)
        _refs(blob, 0x100, 4)
        if entries:
            put_i32(blob, begin + 4, bone_index)
            put_u32(blob, begin + 8, sub)
            parent = bones + parent_index * 72
            put_u32(blob, bones + 72 + 4, parent)
        fixups = ["0x108", "0x10C", "0x17C", "0x180"]
        if entries:
            fixups += ["0x208", "0x34C"]
        run(
            0x008EDA20, blob, lambda b, h=hide, p=parent_index: [h, p & 0xFFFFFFFF, b + 0x100], 0,
            a0=hide, a1=parent_index & 0xFFFFFFFF, a2=0x100, fixups=fixups)

    propagate(0, 0, 0, 1)
    propagate(1, 0, 0, 1)
    propagate(1, 1, 2, 1)
    propagate(1, 1, 2, 0)

    # Named lookup misses, then hits and propagates through the real 0x008EDA20 body.
    blob = _fill(0x500)
    put_u32(blob, 0, 0)
    run(0x008EDB30, blob, lambda _b: [0, 1], 8, a0=NULL, a1=1)
    blob = _fill(0x500)
    begin = 0x200
    put_u32(blob, 0x108, begin)
    put_u32(blob, 0x10C, begin + 0x14)
    put_i32(blob, begin + 4, 3)
    put_u32(blob, begin + 8, 0x480)
    _refs(blob, 0x100, 4)
    put_i32(blob, 8, 9)
    run(
        0x008EDB30, blob, lambda b: [b + 0x100, 1], 8, a0=0x100, a1=1,
        fixups=["0x108", "0x10C", "0x208"])

    def opacity(value: float) -> None:
        blob = _fill(0xA0)
        _refs(blob, 0x40, 4)
        run(0x008EE860, blob, lambda b, bits=fbits(value): [b + 0x40, bits], 8, a0=0x40, a1=fbits(value))

    opacity(-1.0)
    opacity(0.0)
    opacity(0.25)
    opacity(1.0)
    opacity(3.0)

    blob = _fill(0x180)
    put_u32(blob, 0x0C, 0x40)
    put_u32(blob, 0x48, 0x80)
    put_u32(blob, 0x4C, 0x80)
    put_i32(blob, 0x84, 0)
    blob[0x90 + 8] = 1
    _refs(blob, 0xA0, 3)
    run(
        0x008EE8E0, blob, lambda b: [b + 0x90, b + 0xA0, 0], 0xC,
        a0=0x90, a1=0xA0, a2=0, fixups=["0x0C", "0x48", "0x4C"])
    blob = _fill(0x180)
    put_u32(blob, 0x0C, 0x40)
    put_u32(blob, 0x48, 0x80)
    put_u32(blob, 0x4C, 0x80)
    put_i32(blob, 0x84, 0)
    blob[0x90 + 8] = 0
    _refs(blob, 0xA0, 3)
    run(
        0x008EE8E0, blob, lambda b: [b + 0x90, b + 0xA0, 0], 0xC,
        a0=0x90, a1=0xA0, a2=0, fixups=["0x0C", "0x48", "0x4C"])
    blob = _fill(0x180)
    put_u32(blob, 0x0C, 0x40)
    put_u32(blob, 0x48, 0x80)
    put_u32(blob, 0x4C, 0x94)
    put_i32(blob, 0x84, 0)
    put_f32(blob, 0x90 + 0x0C, 0.0)
    blob[0x90 + 8] = 0
    _refs(blob, 0xA0, 3)
    run(
        0x008EE990, blob, lambda b: [b + 0x90, b + 0xA0, 0], 0xC,
        a0=0x90, a1=0xA0, a2=0, fixups=["0x0C", "0x48", "0x4C"])
    blob = _fill(0x1C0)
    put_u32(blob, 0x0C, 0x40)
    put_u32(blob, 0x48, 0x80)
    put_u32(blob, 0x4C, 0x94)
    put_u32(blob, 8, 0x60)
    blob[0x60 + 0x13C] = 0
    put_f32(blob, 0x90 + 0x0C, 0.25)
    put_f32(blob, 0x90 + 0x10, 0.25)
    put_i32(blob, 0x84, 0)
    _refs(blob, 0xC0, 5)
    run(
        0x008EE990, blob, lambda b: [b + 0x90, b + 0xC0, 0], 0xC,
        a0=0x90, a1=0xC0, a2=0, fixups=["0x0C", "0x48", "0x4C", "0x8"])

    blob = _fill(0x160)
    put_u32(blob, 0x11C, 0x140)
    _refs(blob, 0x140, 3)
    put_u32(blob, 0x12C, 0x99)
    put_f32(blob, 0x120, 1.5)
    blob[0x134] = 7
    blob[0x135] = 1
    run(0x008EEED0, blob, lambda _b: [0], 4, a0=0, fixups=["0x11C"])
    blob = _fill(0x160)
    put_u32(blob, 0x11C, 0)
    run(0x008EEED0, blob, lambda _b: [0], 4, a0=0)

    def hermite(scale: float) -> None:
        blob = _fill(0x90)
        put_f32(blob, 0x60, 0.0)
        put_f32(blob, 0x64, 1.0)
        put_f32(blob, 0x68, 2.0)
        put_f32(blob, 0x70, 4.0)
        put_f32(blob, 0x74, 1.0)
        put_f32(blob, 0x78, 2.0)
        put_f32(blob, 0x80, 2.0)
        put_f32(blob, 0x84, scale)
        run(
            0x008EF1A0, blob,
            lambda b: [b + 0x60, b + 0x70, u32(blob, 0x80), u32(blob, 0x84)],
            0x10, a0=0x60, a1=0x70, a2=0x80)

    hermite(0.0)
    hermite(0.5)

    def destroy8(count: int, reverse: bool) -> None:
        blob = _fill(0xC0)
        first = 0x10
        for i in range(count):
            sys = 0x40 + i * 0x28
            put_u32(blob, first + i * 8 + 4, sys)
            particle_sys(blob, sys, 3)
        last = first + count * 8
        if reverse:
            run(0x008F0A90, blob, lambda b: ptrs(b, last, first), 8, a0=last, a1=first,
                fixups=[f"0x{first + i * 8 + 4:X}" for i in range(count)])
        else:
            run(0x008F0A90, blob, lambda b, s=first, e=last: ptrs(b, s, e), 8, a0=first, a1=last,
                fixups=[f"0x{first + i * 8 + 4:X}" for i in range(count)])

    destroy8(0, False)
    destroy8(2, False)
    destroy8(1, True)

    blob = _fill(0x90)
    put_u32(blob, 0x40, 0x77)
    for word in range(0x34 // 4):
        put_u32(blob, 0x50 + word * 4, 0x3F000000 + word)
    run(
        0x008F0B30, blob, lambda b: [b + 0x40, b + 0x50], 8,
        self_off=0x10, a0=0x10, a1=0x40, a2=0x50, compare="ptr")

    def tree_header(blob: bytearray, header: int, root: int) -> None:
        put_u32(blob, header + 0x0C, root)
        put_u32(blob, header + 4, 0)

    def tree_node(blob: bytearray, node: int, key: int, parent: int, right: int = 0, left: int = 0) -> None:
        put_u32(blob, node, right)
        put_u32(blob, node + 4, left)
        put_u32(blob, node + 8, parent)
        put_i32(blob, node + 0x10, key)

    def equal_range(key: int, with_node: bool) -> None:
        blob = _fill(0x80)
        if with_node:
            tree_header(blob, 0, 0x30)
            tree_node(blob, 0x30, 7, 4)
            fixups = ["0x0C", "0x38"]
        else:
            tree_header(blob, 0, 0)
            fixups = []
        put_i32(blob, 0x70, key)
        run(0x008F0B90, blob, lambda b, k=key: [b + 0x60, b + 0x70], 8, a0=0x60, a1=0x70, fixups=fixups, compare="ptr")

    equal_range(7, False)
    equal_range(7, True)
    equal_range(3, True)
    equal_range(8, True)

    def anim12(count: int) -> None:
        blob = _fill(0x160)
        src, dst, out = 0x20, 0x80, 0x00
        for i in range(count):
            obj = 0x120 + i * 0x10
            put_u32(blob, src + i * 0x0C, 0x10 + i)
            put_u32(blob, src + i * 0x0C + 4, obj)
            put_f32(blob, src + i * 0x0C + 8, 0.5 * (i + 1))
            _refs(blob, obj, 2)
        last = src + count * 0x0C
        run(
            0x008F0DF0, blob,
            lambda b, s=src, e=last, d=dst: [b + out, b + s, b + e, b + d], 0,
            self_off=out, a0=src, a1=last, a2=dst,
            fixups=[f"0x{src + i * 0x0C + 4:X}" for i in range(count)])

    anim12(0)
    anim12(2)

    def fill68(count: int) -> None:
        blob = _fill(0x100)
        blob[0x10:0x54] = bytes(range(0x44))
        run(0x008F0F30, blob, lambda b, n=count: [b + 0x60, n, b + 0x10], 0, a0=0x60, a1=count, a2=0x10)

    fill68(0)
    fill68(2)

    def fill_pairs(count: int) -> None:
        blob = _fill(0x40 + max(count, 1) * 0x20)
        put_u32(blob, 0x10, 0x30)
        put_u32(blob, 0x14, 0x40)
        _refs(blob, 0x30, 2)
        _refs(blob, 0x40, 2)
        run(
            0x008F1010, blob, lambda b, n=count: [b + 0x60, n, b + 0x10], 0,
            a0=0x60, a1=count, a2=0x10, fixups=["0x10", "0x14"])

    fill_pairs(0)
    fill_pairs(2)

    blob = _fill(0x100)
    blob[0x80:0xC4] = bytes([0x5A]) * 0x44
    run(0x008F1120, blob, lambda b: ptrs(b, 0x00, 0x00, 0x80), 0, a0=0, a1=0, a2=0x80)
    run(0x008F1120, _fill(0x100), lambda b: ptrs(b, 0x00, 0x88, 0x90), 0, a0=0, a1=0x88, a2=0x90)

    def stop_particles(use_sentinel: bool, flags: int) -> None:
        blob = _fill(0xB0)
        put_u32(blob, 0, 0x20)
        put_u32(blob, 4, 0x28)
        if use_sentinel:
            put_u32(blob, 0x20, SENTINEL_PARTICLE_HANDLE_VA)
            put_u32(blob, 0x24, SENTINEL_PARTICLE_HANDLE_VA)
            fixups: list[str] = ["0x0", "0x4"]
        else:
            put_u32(blob, 0x20, 0x40)
            put_u32(blob, 0x24, 0x48)
            put_u32(blob, 0x40, 0x50)
            put_u32(blob, 0x48, 0x60)
            blob[0x50 + 0x40] = flags
            blob[0x60 + 0x40] = flags
            fixups = ["0x0", "0x4", "0x20", "0x24", "0x40", "0x48"]
        run(0x008F2700, blob, [], 0, fixups=fixups)

    stop_particles(True, 0)
    stop_particles(False, 0x01)
    stop_particles(False, 0x10)

    def particle_list(hidden: bool, flag69: int, flags: int) -> None:
        blob = _fill(0x220)
        put_u32(blob, 8, 0x40)
        blob[0x40 + 0x13C] = 6 if hidden else 0
        put_u32(blob, 0x40 + 0x1C8, 0)
        blob[0x69] = flag69
        put_u32(blob, 0x7C, 0xA0)
        put_u32(blob, 0xA0, 0x7C)
        put_u32(blob, 0xA0 + 0x0C, 0xC0)
        put_u32(blob, 0xC0, 0xD0)
        blob[0xD0 + 0x40] = flags
        run(
            0x008F2C10, blob, [], 0,
            fixups=["0x8", "0x7C", "0xA0", "0xAC", "0xC0"])

    particle_list(False, 0, 0x10)
    particle_list(True, 0, 0x00)
    particle_list(False, 1, 0x00)
    blob = _fill(0x220)
    put_u32(blob, 8, 0x40)
    blob[0x40 + 0x13C] = 0
    put_u32(blob, 0x7C, 0x7C)
    run(0x008F2C10, blob, [], 0, fixups=["0x8", "0x7C"])

    def subobjects(render_null: bool, entries: int) -> None:
        blob = _fill(0x220)
        put_u32(blob, 0x18, 0 if render_null else 0x80)
        put_u32(blob, 0x94, 0x100)
        put_u32(blob, 0x98, 0x100 + entries * 0x1C)
        put_u32(blob, 0xA4, 0x160)
        put_u32(blob, 0xA8, 0x160)
        if not render_null:
            put_u32(blob, 0x88, 0x180)
            put_u32(blob, 0x8C, 0x180)
        if entries:
            put_u32(blob, 0x104, 0x999)
        fixups = ["0x18", "0x94", "0x98", "0xA4", "0xA8"]
        if not render_null:
            fixups += ["0x88", "0x8C"]
        run(0x008F3D60, blob, [], 0, self_off=0x20, fixups=fixups)

    subobjects(True, 0)
    subobjects(False, 0)
    subobjects(False, 1)

    def assign_layer(existing: bool) -> None:
        blob = _fill(0x180)
        put_u32(blob, 0x11C, 0)
        if existing:
            put_u32(blob, 0x11C, 0x140)
            _refs(blob, 0x140, 4)
        put_u32(blob, 0x150, 0)
        _refs(blob, 0x150, 4)
        put_u32(blob, 0x160, 1)
        put_u32(blob, 0x164, 3)
        put_f32(blob, 0x168, 0.5)
        put_f32(blob, 0x16C, 1.25)
        put_f32(blob, 0x170, 0.75)
        put_u32(blob, 0x174, 1)
        put_u32(blob, 0x178, 2)
        fixups = ["0x150"] + (["0x11C"] if existing else [])
        run(
            0x008F3EA0, blob,
            lambda b: [0, b + 0x150, 1, 3, u32(blob, 0x168), u32(blob, 0x16C), u32(blob, 0x170), 1, 2],
            0x24, a0=0, a1=0x150, a2=0x160, fixups=fixups)

    assign_layer(False)
    assign_layer(True)
    blob = _fill(0x180)
    put_u32(blob, 0x11C, 0)
    put_u32(blob, 0x160, 0)
    run(
        0x008F3EA0, blob, lambda b: [0, 0, 0, 0, 0, 0, 0, 0, 0], 0x24,
        a0=0, a1=NULL, a2=0x160)

    blob = _fill(0x180)
    put_u32(blob, 0x11C, 0x140)
    _refs(blob, 0x140, 5)
    put_u32(blob, 0x12C, 4)
    put_f32(blob, 0x128, 0.25)
    put_f32(blob, 0x120, 2.0)
    put_f32(blob, 0x124, 0.5)
    put_u32(blob, 0x130, 6)
    blob[0x134] = 1
    blob[0x135] = 1
    put_u32(blob, 0x138, 0)
    run(0x008F3FA0, blob, lambda _b: [1, 0], 8, a0=1, a1=0, fixups=["0x11C"])

    def count_keys(key: int, nodes: int) -> None:
        blob = _fill(0x90)
        if nodes == 0:
            tree_header(blob, 0, 0)
            fixups: list[str] = []
        elif nodes == 1:
            tree_header(blob, 0, 0x30)
            tree_node(blob, 0x30, 5, 4)
            fixups = ["0x0C", "0x38"]
        else:
            tree_header(blob, 0, 0x30)
            tree_node(blob, 0x30, 5, 4, right=0x50)
            tree_node(blob, 0x50, 5, 0x30)
            fixups = ["0x0C", "0x30", "0x38", "0x58"]
        put_i32(blob, 0x80, key)
        run(0x008F5CB0, blob, lambda b: [b + 0x80], 4, a0=0x80, fixups=fixups, compare="u32")

    count_keys(5, 0)
    count_keys(5, 1)
    count_keys(6, 1)
    count_keys(5, 2)

    def copy28(backward: bool, count: int) -> None:
        blob = _fill(0xC0 + count * 0x20)
        src, dst = 0x10, 0x80
        for i in range(count):
            blob[src + i * 0x1C: src + i * 0x1C + 0x1C] = bytes([0x21 + i]) * 0x1C
        last = src + count * 0x1C
        end = dst + count * 0x1C
        va = 0x008F6690 if backward else 0x008F6180
        a2 = end if backward else dst
        run(va, blob, lambda b, s=src, e=last, d=a2: ptrs(b, s, e, d), 0, a0=src, a1=last, a2=a2, compare="ptr")

    copy28(False, 0)
    copy28(False, 2)
    copy28(True, 0)
    copy28(True, 2)

    def destroy12(count: int) -> None:
        blob = _fill(0x20 + max(count, 1) * 0x20)
        first = 0x10
        for i in range(count):
            obj = 0x40 + i * 0x10
            put_u32(blob, first + i * 0x0C + 4, obj)
            _refs(blob, obj, 3)
        last = first + count * 0x0C
        run(
            0x008F61B0, blob, lambda b, s=first, e=last: ptrs(b, s, e), 8,
            a0=first, a1=last, fixups=[f"0x{first + i * 0x0C + 4:X}" for i in range(count)])

    destroy12(0)
    destroy12(2)

    def copy_anim(backward: bool, count: int, alias: bool) -> None:
        blob = _fill(0xE0)
        src = 0x10
        dst = src if alias else 0x70
        for i in range(count):
            obj = 0xA0 + i * 0x10
            put_u32(blob, src + i * 0x0C, 0x30 + i)
            put_u32(blob, src + i * 0x0C + 4, obj)
            put_f32(blob, src + i * 0x0C + 8, float(i + 1))
            _refs(blob, obj, 4)
            if not alias:
                put_u32(blob, dst + i * 0x0C + 4, 0)
        last = src + count * 0x0C
        end = dst + count * 0x0C
        va = 0x008F6C00 if backward else 0x008F6900
        a2 = end if backward else dst
        run(
            va, blob, lambda b, s=src, e=last, d=a2: ptrs(b, s, e, d), 0,
            a0=src, a1=last, a2=a2,
            fixups=[f"0x{src + i * 0x0C + 4:X}" for i in range(count)], compare="ptr")

    copy_anim(False, 0, False)
    copy_anim(False, 1, True)
    copy_anim(False, 2, False)
    copy_anim(True, 2, False)

    def bone_count(state_null: bool, found: bool, enabled: bool, inner: int) -> None:
        blob = _fill(0x280)
        blob[0x1D7] = 0
        if state_null:
            put_u32(blob, 0x2C, 0)
            fixups: list[str] = []
        else:
            put_u32(blob, 0x2C, 0x40)
            blob[0x40 + 0x84] = 8 if enabled else 0
            root = 0x100 if found else 0
            tree_header(blob, 0x40 + 0x68, root)
            fixups = ["0x2C"]
            if found:
                tree_node(blob, 0x100, 4, 0x40 + 0x6C)
                fixups += ["0x40+0x68+0x0C".replace("0x40+0x68+0x0C", "0xB4"), "0x108"]
                put_u32(blob, 0x120, 0)
                if inner:
                    inner_root = 0x180
                    put_u32(blob, 0x100 + 0x14 + 0x0C, inner_root)
                    tree_node(blob, inner_root, 9, 0x100 + 0x14 + 4)
                    fixups += ["0x120", "0x188"]
        run(0x008F6ED0, blob, lambda _b: [9, 4], 8, a0=9, a1=4, fixups=fixups, compare="u32")

    bone_count(True, False, False, 0)
    bone_count(False, False, True, 0)
    bone_count(False, True, False, 0)
    bone_count(False, True, True, 0)
    bone_count(False, True, True, 1)

    def erase28(count: int, drop: int) -> None:
        blob = _fill(0x20 + count * 0x1C)
        begin = 0x10
        for i in range(count):
            blob[begin + i * 0x1C: begin + i * 0x1C + 0x1C] = bytes([0x41 + i]) * 0x1C
        end = begin + count * 0x1C
        put_u32(blob, 4, end)
        first = begin + 0 * 0x1C
        last = begin + drop * 0x1C
        run(
            0x008F95D0, blob, lambda b, s=first, e=last: ptrs(b, s, e), 8,
            a0=first, a1=last, fixups=["0x4"], compare="ptr")

    erase28(3, 0)
    erase28(3, 1)
    erase28(3, 3)

    def fill_ref_range(same: bool) -> None:
        blob = _fill(0x40)
        put_u32(blob, 0x20, 0x28)
        put_u32(blob, 0x24, 0x30)
        _refs(blob, 0x28, 3)
        _refs(blob, 0x30, 3)
        if same:
            run(0x008FA6D0, blob, lambda b: ptrs(b, 0x20, 0x28, 0x20), 0, a0=0x20, a1=0x28, a2=0x20, fixups=["0x20", "0x24"])
        else:
            put_u32(blob, 0x00, 0)
            put_u32(blob, 0x04, 0)
            run(0x008FA6D0, blob, lambda b: ptrs(b, 0x00, 0x08, 0x20), 0, a0=0, a1=8, a2=0x20, fixups=["0x20", "0x24"])

    fill_ref_range(True)
    fill_ref_range(False)

    def copy_pairs(alias: bool) -> None:
        blob = _fill(0x50)
        put_u32(blob, 0x20, 0x30)
        put_u32(blob, 0x24, 0x40)
        _refs(blob, 0x30, 3)
        _refs(blob, 0x40, 3)
        dst = 0x20 if alias else 0x10
        if not alias:
            put_u32(blob, dst, 0)
            put_u32(blob, dst + 4, 0)
        run(
            0x008FA790, blob, lambda b, d=dst: ptrs(b, 0x20, 0x28, d), 0,
            a0=0x20, a1=0x28, a2=dst, fixups=["0x20", "0x24"], compare="ptr")

    copy_pairs(True)
    copy_pairs(False)

    blob = _fill(0x40)
    run(0x008FAB20, blob, [], 0, compare="ptr")
    blob = _fill(0x90)
    run(0x00907AB0, blob, [], 0, compare="ptr")

    for value in (0, 0xAB, 0x1FF):
        blob = _fill(0x220)
        run(0x00910E50, blob, lambda _b, v=value: [v], 4, a0=value)
        blob = _fill(0x220)
        run(0x00910E90, blob, lambda _b, v=value: [v], 4, a0=value)
        blob = _fill(0x220)
        run(0x00910EB0, blob, lambda _b, v=value: [v], 4, a0=value)
        blob = _fill(0x220)
        run(0x00910EE0, blob, lambda _b, v=value: [v], 4, a0=value)

    for data_byte in (0, 0x5A):
        blob = _fill(0x120)
        put_u32(blob, 4, 0x20)
        blob[0x20 + 0xD3] = data_byte
        run(0x00910E60, blob, [], 0, fixups=["0x4"], compare="u8")

    for flag, data_byte, _name in ((0, 0, "neither"), (1, 0, "flag"), (0, 1, "data"), (0, 0x80, "high")):
        blob = _fill(0x120)
        blob[0x6D] = flag
        put_u32(blob, 4, 0x20)
        blob[0x20 + 0xD4] = data_byte
        run(0x00910E70, blob, [], 0, fixups=["0x4"], compare="u32")

    for va, offset in ((0x00910EC0, 0x14), (0x00910ED0, 0x18), (0x00911250, 0x290)):
        blob = _fill(offset + 0x10)
        run(va, blob, [], 0, compare="ptr")
        run(va, _fill(0x10), [], 0, self_off=NULL, compare="ptr")

    missing = [va for va in ACCEPTED_ENTRIES if oracle.counts.get(va, 0) == 0]
    if missing:
        raise AssertionError("untested entries: " + ", ".join(f"0x{va:08X}" for va in missing))
    total = sum(oracle.counts.values())
    print(f"render91 oracle: {len(oracle.counts)} entries, {total} cases, 0 mismatches")
    oracle.close()


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument(
        "--render91check", type=Path,
        default=Path(__file__).resolve().parents[3] / "build/win32/bin/Release/ra3_render91check.exe")
    args = parser.parse_args()
    verify(args.render91check.resolve())


if __name__ == "__main__":
    main()
