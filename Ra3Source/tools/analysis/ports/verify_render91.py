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
    0x008E52A0, 0x008E5690, 0x008E57E0, 0x008E5A60,
    0x008E5E80, 0x008E6060, 0x008E6180, 0x008E6260,
    0x008E8550, 0x008E8900, 0x008E8960, 0x008E8D50,
    0x008E8DC0, 0x008E8E90, 0x008E8EC0, 0x008E8FD0,
    0x008E9000, 0x008E9020, 0x008E9050, 0x008E9240,
    0x008E9270, 0x008E93B0, 0x008EB120, 0x008EB360,
    0x008EB490, 0x008EB4E0, 0x008EB540, 0x008EB5E0,
    0x008EC560, 0x008ED690, 0x008EDA20, 0x008EDB30,
    0x008EE860, 0x008EE8E0, 0x008EE990, 0x008EEED0,
    0x008EF1A0, 0x008F0A90, 0x008F0B30, 0x008F0B90,
    0x008F0DF0, 0x008F0F30, 0x008F1010, 0x008F1120,
    0x008F2700, 0x008F2C10, 0x008F3D60, 0x008F3EA0,
    0x008F3FA0, 0x008F5CB0, 0x008F6180, 0x008F61B0,
    0x008F6690, 0x008F6900, 0x008F6C00, 0x008F6ED0,
    0x008F95D0, 0x008FA6D0, 0x008FA790, 0x008FAB20,
    0x00907AB0, 0x00910E50, 0x00910E60, 0x00910E70,
    0x00910E90, 0x00910EB0, 0x00910EC0, 0x00910ED0,
    0x00910EE0, 0x00911250,
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
