"""Differentially verify the logic4d GameLogic & TextFile cluster against ra3_1.0.game."""

from __future__ import annotations

import argparse
import math
import struct
import subprocess
import sys
from pathlib import Path
from typing import Callable

from unicorn import x86_const

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from emulate import CallResult, Emulator, STACK_BASE, STACK_SIZE  # noqa: E402

ACCEPTED_ENTRIES = (
    0x004D05A0, 0x004D2AD0, 0x004D3750, 0x004D3770,
    0x004D3CD0, 0x004D4020, 0x004D4260, 0x004D4320,
    0x004D4330, 0x004D4400, 0x004D4500, 0x004D4910,
    0x004D4C50, 0x004D4CD0, 0x004D4EA0, 0x004D5530,
    0x004D57A0, 0x004D5900, 0x004D5920, 0x004D5A40,
    0x004D5AB0, 0x004D5B10, 0x004D5BB0, 0x004D5BD0,
    0x004D5DF0, 0x004D6170, 0x004D6500, 0x004D6550,
    0x004D6680, 0x004D66A0, 0x004D6710, 0x004D6730,
    0x004D6750, 0x004D67F0, 0x004D6960, 0x004D69A0,
    0x004D6AB0, 0x004D6FF0, 0x004D7040, 0x004D8390,
    0x004D86D0, 0x004D8780, 0x004D8810, 0x004D8930,
    0x004D9230, 0x004D9280, 0x004DA690, 0x004DA780,
    0x004DAD10, 0x004DB8C0, 0x004DBB30, 0x004DBBF0,
    0x004DBD20, 0x004DBEC0, 0x004DC080, 0x004DD390,
    0x004DD3D0, 0x004DFC30,
)

X87_CONTROL_WORDS = (0x007F, 0x027F, 0x037F)
NULL = 0xFFFFFFFF


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


def _fill(size: int, value: int = 0xA5) -> bytearray:
    return bytearray([value]) * size


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
        a3: int = 0,
        a4: int = 0,
        fpcw: int = 0x037F,
    ) -> tuple[int, bytes]:
        assert self.process.stdin is not None and self.process.stdout is not None
        fix_str = ",".join(fixups) if fixups else "-"
        line = (
            f"0x{va:08X} 0x{self_offset:X} {blob.hex()} {fix_str} "
            f"0x{a0 & 0xFFFFFFFF:X} 0x{a1 & 0xFFFFFFFF:X} 0x{a2 & 0xFFFFFFFF:X} "
            f"0x{a3 & 0xFFFFFFFF:X} 0x{a4 & 0xFFFFFFFF:X} 0x{fpcw:04X}\n"
        )
        self.process.stdin.write(line)
        self.process.stdin.flush()
        out = self.process.stdout.readline().strip()
        if not out.startswith("ret=0x"):
            raise RuntimeError(f"ra3_logic4dcheck failed on 0x{va:08X}: {out}")
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
        a3: int = 0,
        a4: int = 0,
        fpcw: int = 0x037F,
        compare_ret: str = "none",  # "none", "u8", "u32", "ptr"
    ) -> None:
        raw_blob = bytes(blob)
        cpp_ret, cpp_blob = self.cpp.call(
            va, self_offset, raw_blob, fixups, a0, a1, a2, a3, a4, fpcw
        )

        self.emu.reset_data()
        self.emu.write(STACK_BASE + STACK_SIZE - 0x200, b"\x00" * 0x100)
        self.emu.uc.reg_write(x86_const.UC_X86_REG_FPCW, fpcw)
        base = self.emu.alloc(raw_blob)
        for item in fixups:
            off = int(item, 0)
            rel = u32(raw_blob, off)
            self.emu.write(base + off, struct.pack("<I", (base + rel) & 0xFFFFFFFF))

        res, game_ret = invoke(self.emu, base)
        if res.stack_cleanup != cleanup:
            raise AssertionError(
                f"0x{va:08X}: stack cleanup {res.stack_cleanup}, expected {cleanup}"
            )

        game_blob = bytearray(self.emu.read(base, len(raw_blob)))
        for off in range(0, len(game_blob) - 3, 4):
            cur = u32(game_blob, off)
            if base <= cur < base + len(game_blob):
                put_u32(game_blob, off, cur - base)

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


def _init_grid_transform(blob: bytearray, off: int = 0) -> None:
    # World-to-grid 3x3 at +0x7C..+0x9C
    put_f32(blob, off + 0x7C, 1.0)
    put_f32(blob, off + 0x80, 0.25)
    put_f32(blob, off + 0x84, 0.0)
    put_f32(blob, off + 0x88, -0.25)
    put_f32(blob, off + 0x8C, 1.0)
    put_f32(blob, off + 0x90, 0.0)
    put_f32(blob, off + 0x94, 12.5)
    put_f32(blob, off + 0x98, -7.5)
    put_f32(blob, off + 0x9C, 1.0)
    # Grid-to-world 3x3 at +0xA0..+0xC0
    put_f32(blob, off + 0xA0, 0.95)
    put_f32(blob, off + 0xA4, -0.15)
    put_f32(blob, off + 0xA8, 0.05)
    put_f32(blob, off + 0xAC, 0.15)
    put_f32(blob, off + 0xB0, 0.95)
    put_f32(blob, off + 0xB4, 0.10)
    put_f32(blob, off + 0xB8, 100.0)
    put_f32(blob, off + 0xBC, 200.0)
    put_f32(blob, off + 0xC0, 15.0)


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
        a3: int = 0,
        a4: int = 0,
        fpcw: int = 0x037F,
        fixups: list[str] | None = None,
        compare: str = "none",
        setup: Callable[[Emulator, int], None] | None = None,
        float_ret: bool = False,
    ) -> None:
        if va not in ACCEPTED_ENTRIES:
            return

        def invoke(machine: Emulator, base: int):
            if setup is not None:
                setup(machine, base)
            ecx = 0 if self_off >= len(blob) else base + self_off
            args = stack(base) if callable(stack) else stack
            if float_ret:
                result, bits = machine.call_float32(
                    va, args, ecx=ecx, fpu_control_word=fpcw
                )
                return result, bits
            result = machine.call(va, args, ecx=ecx)
            return result, result.eax

        oracle.check(
            va,
            self_off,
            blob,
            fixups or [],
            invoke,
            cleanup,
            a0=a0,
            a1=a1,
            a2=a2,
            a3=a3,
            a4=a4,
            fpcw=fpcw,
            compare_ret=compare,
        )

    # 1. 0x004D05A0 - TextFile::getLine(int index) const
    for idx in (0, 1, 2, 3, 10):
        blob = _fill(0x80, 0)
        put_u32(blob, 0x04, 0x20)
        put_u32(blob, 0x08, 0x20 + 3 * 12)
        put_u32(blob, 0x20 + 0 * 12, 0x50)
        put_u32(blob, 0x20 + 1 * 12, 0x60)
        put_u32(blob, 0x20 + 2 * 12, 0x70)
        run(
            0x004D05A0,
            blob,
            [idx],
            4,
            a0=idx,
            fixups=["0x4", "0x8", "0x20", "0x2c", "0x38"],
            compare="ptr",
        )
    # Empty TextFile
    blob = _fill(0x40, 0)
    put_u32(blob, 0x04, 0x20)
    put_u32(blob, 0x08, 0x20)
    run(0x004D05A0, blob, [0], 4, a0=0, fixups=["0x4", "0x8"], compare="ptr")

    # 2. 0x004D2AD0 - relinkGhostObjectNodeBefore4D
    # Case A: node is self-linked (unlinked), insert before targetNode in a 2-node ring
    blob = _fill(0x40, 0)
    put_u32(blob, 0x00, 0x00)
    put_u32(blob, 0x04, 0x00)
    put_u32(blob, 0x10, 0x20)
    put_u32(blob, 0x14, 0x20)
    put_u32(blob, 0x20, 0x10)
    put_u32(blob, 0x24, 0x10)
    run(
        0x004D2AD0,
        blob,
        lambda b: [b + 0x10],
        4,
        self_off=0x00,
        a0=0x10,
        fixups=["0x0", "0x4", "0x10", "0x14", "0x20", "0x24"],
    )
    # Case B: node is already linked between 0x10 and 0x20, relink before 0x20
    blob = _fill(0x40, 0)
    put_u32(blob, 0x00, 0x10)
    put_u32(blob, 0x04, 0x20)
    put_u32(blob, 0x10, 0x20)
    put_u32(blob, 0x14, 0x00)
    put_u32(blob, 0x20, 0x00)
    put_u32(blob, 0x24, 0x10)
    run(
        0x004D2AD0,
        blob,
        lambda b: [b + 0x20],
        4,
        self_off=0x00,
        a0=0x20,
        fixups=["0x0", "0x4", "0x10", "0x14", "0x20", "0x24"],
    )

    # 3. 0x004D3750 - equalsObjectIDPair4D
    for (a_lo, a_hi, b_lo, b_hi) in (
        (10, 20, 10, 20),
        (10, 20, 10, 21),
        (10, 20, 11, 20),
        (0, 0, 0, 0),
    ):
        blob = _fill(0x20, 0)
        put_u32(blob, 0x00, a_lo)
        put_u32(blob, 0x04, a_hi)
        put_u32(blob, 0x08, b_lo)
        put_u32(blob, 0x0C, b_hi)
        run(0x004D3750, blob, lambda b: [b + 0x08], 4, a0=0x08, compare="u32")

    # 4. 0x004D3770 - normalizeAngleRad4D
    for angle in (0.0, 1.5, -1.5, 3.1415927, -3.1415927, 4.5, -4.5, 15.0, -15.0):
        abits = fbits(angle)
        for cw in X87_CONTROL_WORDS:
            run(
                0x004D3770,
                _fill(0x10, 0),
                [abits],
                0,
                a0=abits,
                fpcw=cw,
                compare="u32",
                float_ret=True,
            )

    # 5, 7, 9, 34. Constant string VA getters
    for va in (0x004D3CD0, 0x004D4260, 0x004D4330, 0x004D67F0):
        run(va, _fill(0x10), [], 0, compare="u32")
        run(va, _fill(0x10, 0), [], 0, compare="u32")

    # 6. 0x004D4020 - convertSecondsToFrames4D
    for seconds, fps, scale in (
        (2.5, 15.0, None),
        (0.0, 30.0, None),
        (1.75, 30.0, 1.0),
        (3.25, 15.0, 2.0),
        (-1.5, 15.0, 1.5),
    ):
        fps_bits = fbits(fps)
        for cw in X87_CONTROL_WORDS:
            blob = _fill(0x240, 0)
            put_u32(blob, 0x00, 0x10)
            put_f32(blob, 0x18, seconds)
            gl_off = NULL if scale is None else 0x20
            if scale is not None:
                put_f32(blob, 0x20 + 0x1C4, scale)

            def setup_4020(emu: Emulator, base: int, go=gl_off, fb=fps_bits) -> None:
                emu.write(
                    0x00DEBA64,
                    struct.pack("<I", 0 if go == NULL else (base + go)),
                )
                emu.write(0x00DEEEE8, struct.pack("<I", fb))

            run(
                0x004D4020,
                blob,
                [],
                0,
                a0=gl_off,
                a1=fps_bits,
                fpcw=cw,
                fixups=["0x0"],
                compare="u32",
                setup=setup_4020,
            )

    # 8. 0x004D4320 - resetDelayedExperienceLevelGrantSystem4D
    for fill_byte in (0xA5, 0xFF, 0x01):
        blob = _fill(0x30, fill_byte)
        run(0x004D4320, blob, [], 0)

    # 10, 25, 28, 38, 47. DamageInfoInput / DamageInfoShort / DOTInfoRecord
    for seed in (0, 1):
        # 0x004D4400
        blob = _fill(0x120, 0x5A if seed == 0 else 0x33)
        for i in range(0, 0x84, 4):
            put_u32(blob, 0x88 + i, 0x10000000 + i + seed * 0x101)
        for foff in (0x34, 0x3C, 0x54, 0x58, 0x5C, 0x60, 0x64, 0x68, 0x78):
            put_f32(blob, 0x88 + foff, float(foff) * 0.25 + seed)
        run(0x004D4400, blob, lambda b: [b + 0x88], 4, a0=0x88, compare="ptr")

        # 0x004D5DF0
        blob = _fill(0x120, 0xCC if seed == 0 else 0x11)
        for i in range(0, 0x84, 4):
            put_u32(blob, 0x88 + i, 0x20000000 + i + seed * 0x202)
        for foff in [0x20, 0x24, 0x28, 0x34, 0x3C] + list(range(0x48, 0x7C, 4)):
            put_f32(blob, 0x88 + foff, float(foff) * 0.5 + seed)
        run(0x004D5DF0, blob, lambda b: [b + 0x88], 4, a0=0x88, compare="ptr")

        # 0x004D6550
        blob = _fill(0x90, 0xAA if seed == 0 else 0xFF)
        run(0x004D6550, blob, [], 0, compare="ptr")

        # 0x004D6FF0
        blob = _fill(0x150, 0x77 if seed == 0 else 0x22)
        for i in range(0, 0x9C, 4):
            put_u32(blob, 0xA0 + i, 0x30000000 + i + seed)
        for foff in (0x38, 0x40, 0x58, 0x5C, 0x60, 0x64, 0x68, 0x6C, 0x7C, 0x8C, 0x90, 0x98):
            put_f32(blob, 0xA0 + foff, float(foff) * 0.125 + seed)
        run(0x004D6FF0, blob, lambda b: [b + 0xA0], 4, a0=0xA0, compare="ptr")

        # 0x004DA690
        blob = _fill(0x160, 0x99 if seed == 0 else 0x44)
        for i in range(0, 0xAC, 4):
            put_u32(blob, 0xB0 + i, 0x40000000 + i + seed)
        for foff in (0x38, 0x40, 0x58, 0x5C, 0x60, 0x64, 0x68, 0x6C, 0x7C, 0x8C, 0x90, 0x98):
            put_f32(blob, 0xB0 + foff, float(foff) * 0.375 + seed)
        run(0x004DA690, blob, lambda b: [b + 0xB0], 4, a0=0xB0, compare="ptr")

    # 11. 0x004D4500 - compareCooldownProgress4D
    for (frame_l, scale_l, frame_r, scale_r, cur_frame) in (
        (100, 0.5, 200, 0.5, 50),
        (200, 0.5, 100, 0.5, 50),
        (100, 0.5, 100, 0.5, 50),
        (30, 1.0, 40, 0.5, 50),  # negative diff -> wraps by +2^32
    ):
        for cw in X87_CONTROL_WORDS:
            blob = _fill(0x200, 0)
            put_f32(blob, 0x00 + 0x38, scale_l)
            put_u32(blob, 0x00 + 0xA0, frame_l)
            put_f32(blob, 0xB0 + 0x38, scale_r)
            put_u32(blob, 0xB0 + 0xA0, frame_r)
            put_u32(blob, 0x160 + 0x58, cur_frame)

            def setup_4500(emu: Emulator, base: int) -> None:
                emu.write(0x00DEBA64, struct.pack("<I", base + 0x160))

            run(
                0x004D4500,
                blob,
                lambda b: [b + 0x00, b + 0xB0],
                8,
                a0=0x00,
                a1=0xB0,
                a2=cur_frame,
                fpcw=cw,
                compare="u32",
                setup=setup_4500,
            )

    # 12. 0x004D4910 - initThingTemplateRefWrapper4D
    for ref_val in (0, 0x12345678, 0xDEADBEEF):
        blob = _fill(0x10)
        run(0x004D4910, blob, [ref_val], 4, a0=ref_val, compare="ptr")

    # 13. 0x004D4C50 - allocateNextSequenceId4D
    for start_id in (0, 1, 999, 0xFFFFFFFF):
        blob = _fill(0x150, 0)
        put_u32(blob, 0x144, start_id)
        run(0x004D4C50, blob, [], 0, compare="u32")

    # 14. 0x004D4CD0 - countLinkedNodesAtBC4D
    # 0 nodes
    blob = _fill(0x100, 0)
    run(0x004D4CD0, blob, [], 0, compare="u32")
    # 3 nodes
    blob = _fill(0x300, 0)
    put_u32(blob, 0xBC, 0x100)
    put_u32(blob, 0x100 + 0x7C, 0x190)
    put_u32(blob, 0x190 + 0x7C, 0x220)
    put_u32(blob, 0x220 + 0x7C, 0)
    run(
        0x004D4CD0,
        blob,
        [],
        0,
        fixups=["0xbc", "0x17c", "0x20c"],
        compare="u32",
    )

    # 15. 0x004D4EA0 - updateCrcBufferMaxSize4D
    for cur_max, cand in ((100, 50), (100, 100), (100, 250), (0, 0x06000000)):
        blob = _fill(0x30, 0)
        put_u32(blob, 0x1C, cur_max)
        run(0x004D4EA0, blob, [cand], 4, a0=cand)

    # 16. 0x004D5530 - updateMaxFloatAt204D
    for cur_f, cand_f in ((1.0, 0.5), (1.0, 1.0), (1.0, 2.5), (-5.0, -2.0)):
        blob = _fill(0x30, 0)
        put_f32(blob, 0x20, cur_f)
        cbits = fbits(cand_f)
        run(0x004D5530, blob, [cbits], 4, a0=cbits)

    # 17. 0x004D57A0 - snapAngleToCardinalRad4D
    for angle in (0.0, 0.2, 0.9, 1.7, 2.8, -0.2, -0.9, -1.7, -2.8, 7.0, -7.0):
        blob = _fill(0x10, 0)
        put_f32(blob, 0x00, angle)
        run(0x004D57A0, blob, lambda b: [b], 4, a0=0x00)

    # 18. 0x004D5900 - anyBitFlags288_4D
    blob = _fill(0x30, 0)
    run(0x004D5900, blob, [], 0, compare="u8")
    for word_idx in (0, 4, 8):
        blob = _fill(0x30, 0)
        put_u32(blob, word_idx * 4, 1 << word_idx)
        run(0x004D5900, blob, [], 0, compare="u8")

    # 19. 0x004D5920 - testRequiredAndForbiddenBitFlags288_4D
    for self_w, req_w, forb_w in (
        (0b1101, 0b0101, 0b0010),  # pass
        (0b1101, 0b1111, 0b0000),  # missing required bit
        (0b1101, 0b0101, 0b1000),  # has forbidden bit
    ):
        blob = _fill(0x90, 0)
        put_u32(blob, 0x00 + 8 * 4, self_w)
        put_u32(blob, 0x28 + 8 * 4, req_w)
        put_u32(blob, 0x50 + 8 * 4, forb_w)
        run(
            0x004D5920,
            blob,
            lambda b: [b + 0x28, b + 0x50],
            8,
            a0=0x28,
            a1=0x50,
            compare="u8",
        )

    # 20. 0x004D5A40 - intersectsBitFlags480_4D
    blob = _fill(0x80, 0)
    put_u32(blob, 14 * 4, 0xAA)
    put_u32(blob, 0x40 + 14 * 4, 0x55)
    run(0x004D5A40, blob, lambda b: [b + 0x40], 4, a0=0x40, compare="u8")
    put_u32(blob, 0x40 + 14 * 4, 0x80)
    run(0x004D5A40, blob, lambda b: [b + 0x40], 4, a0=0x40, compare="u8")

    # 21. 0x004D5AB0 - anyBitFlags480_4D
    blob = _fill(0x40, 0)
    run(0x004D5AB0, blob, [], 0, compare="u8")
    put_u32(blob, 14 * 4, 1)
    run(0x004D5AB0, blob, [], 0, compare="u8")

    # 22. 0x004D5B10 - equalsBitFlags480_4D
    blob = _fill(0x80, 0x3C)
    run(0x004D5B10, blob, lambda b: [b, b + 0x40], 0, a0=0x00, a1=0x40, compare="u8")
    blob[0x40 + 59] = 0x3D
    run(0x004D5B10, blob, lambda b: [b, b + 0x40], 0, a0=0x00, a1=0x40, compare="u8")
    blob[0x40 + 59] = 0x3B
    run(0x004D5B10, blob, lambda b: [b, b + 0x40], 0, a0=0x00, a1=0x40, compare="u8")

    # 23. 0x004D5BB0 - anyObjectStatusMask224_4D
    blob = _fill(0x20, 0)
    run(0x004D5BB0, blob, [], 0, compare="u8")
    put_u32(blob, 6 * 4, 0x400)
    run(0x004D5BB0, blob, [], 0, compare="u8")

    # 24. 0x004D5BD0 - intersectsObjectStatusMask224_4D
    blob = _fill(0x40, 0)
    put_u32(blob, 6 * 4, 0x0F)
    put_u32(blob, 0x20 + 6 * 4, 0xF0)
    run(0x004D5BD0, blob, lambda b: [b + 0x20], 4, a0=0x20, compare="u8")
    put_u32(blob, 0x20 + 6 * 4, 0x08)
    run(0x004D5BD0, blob, lambda b: [b + 0x20], 4, a0=0x20, compare="u8")

    # 26. 0x004D6170 - containsPointStrictRegion2D4D
    for px, py in ((5.0, 5.0), (0.0, 5.0), (10.0, 5.0), (5.0, 0.0), (5.0, 10.0), (-1.0, 5.0)):
        blob = _fill(0x30, 0)
        put_f32(blob, 0x00, 0.0)
        put_f32(blob, 0x04, 0.0)
        put_f32(blob, 0x0C, 10.0)
        put_f32(blob, 0x10, 10.0)
        put_f32(blob, 0x20, px)
        put_f32(blob, 0x24, py)
        run(0x004D6170, blob, lambda b: [b + 0x20], 4, a0=0x20, compare="u32")

    # 27. 0x004D6500 - resetMoveGoalResult4D
    for fill_val in (0xA5, 0xFF):
        blob = _fill(0x30, fill_val)
        run(0x004D6500, blob, [], 0, compare="ptr")

    # 29. 0x004D6680 - anyBitAt26C4D
    for word_val in (0, 1, 0x80000000):
        blob = _fill(0x280, 0)
        put_u32(blob, 0x26C, word_val)
        run(0x004D6680, blob, [], 0, compare="u8")

    # 30. 0x004D66A0 - testBitAt26C4D
    for bit_idx in (0, 5, 31, 35):
        blob = _fill(0x280, 0)
        put_u32(blob, 0x26C, 1 << 5)
        put_u32(blob, 0x270, 1 << 3)
        run(0x004D66A0, blob, [bit_idx], 4, a0=bit_idx, compare="u32")

    # 31, 32. 0x004D6710 & 0x004D6730 - getSpecialAbilityField0C4D / 104D
    for va, field_off in ((0x004D6710, 0x0C), (0x004D6730, 0x10)):
        # Null pointer at field_off -> returns 0
        blob = _fill(0x40, 0)
        put_u32(blob, 0x04, 0x10)
        put_u32(blob, 0x10 + field_off, 0)
        run(va, blob, [], 0, fixups=["0x4"], compare="u32")
        # Non-null pointer at field_off -> dereferences value
        blob = _fill(0x40, 0)
        put_u32(blob, 0x04, 0x10)
        put_u32(blob, 0x10 + field_off, 0x30)
        put_u32(blob, 0x30, 0xCAFEBABE)
        run(va, blob, [], 0, fixups=["0x4", hex(0x10 + field_off)], compare="u32")

    # 33. 0x004D6750 - hasAnyLowThreeFlagsAt08_4D
    for flags in (0x00, 0x01, 0x02, 0x04, 0x08, 0xF0):
        blob = _fill(0x30, 0)
        put_u32(blob, 0x04, 0x10)
        put_u32(blob, 0x18, flags)
        run(0x004D6750, blob, [], 0, fixups=["0x4"], compare="u32")

    # 35. 0x004D6960 - stampPassiveAreaEffectMaskedSlots4D
    for mask_val, stamp_val in ((0x0000, 0x11112222), (0x5555, 0x33334444), (0x7FFF, 0x9999AAAA)):
        blob = _fill(0x90, 0xCC)
        put_u32(blob, 0x80, mask_val)
        run(
            0x004D6960,
            blob,
            lambda b, sv=stamp_val: [b + 0x80, sv],
            8,
            a0=0x80,
            a1=stamp_val,
        )

    # 36. 0x004D69A0 - isEntryInBeamExcludeList4D
    # Null listHeader
    blob = _fill(0x40, 0)
    run(0x004D69A0, blob, lambda b: [b, 0], 8, a0=0x00, a1=NULL, compare="u8")
    # Non-empty listHeader (2 entries: keys 111 and 222)
    for target_key in (111, 222, 333):
        blob = _fill(0x100, 0)
        # target chain: 0x00 -> 0x08 -> 0x10 -> 0x18 (+4 = target_key)
        put_u32(blob, 0x00, 0x08)
        put_u32(blob, 0x08, 0x10)
        put_u32(blob, 0x10, 0x18)
        put_u32(blob, 0x1C, target_key)
        # listHeader at 0x20: count=2, array at 0x30
        put_u32(blob, 0x20, 2)
        put_u32(blob, 0x24, 0x30)
        put_u32(blob, 0x30, 0x40)
        put_u32(blob, 0x34, 0x70)
        # entry 0 chain: 0x40 -> 0x48 -> 0x50 (+4 = 111)
        put_u32(blob, 0x40, 0x48)
        put_u32(blob, 0x48, 0x50)
        put_u32(blob, 0x54, 111)
        # entry 1 chain: 0x70 -> 0x78 -> 0x80 (+4 = 222)
        put_u32(blob, 0x70, 0x78)
        put_u32(blob, 0x78, 0x80)
        put_u32(blob, 0x84, 222)
        run(
            0x004D69A0,
            blob,
            lambda b: [b + 0x00, b + 0x20],
            8,
            a0=0x00,
            a1=0x20,
            fixups=["0x0", "0x8", "0x10", "0x24", "0x30", "0x34", "0x40", "0x48", "0x70", "0x78"],
            compare="u8",
        )

    # 37. 0x004D6AB0 - findCampaignRecordByNestedKey4D
    for search_key in (10, 20, 99):
        blob = _fill(0xC0, 0)
        put_u32(blob, 0x0C, 0x20)
        put_u32(blob, 0x10, 0x60)
        put_u32(blob, 0x20 + 0x04, 0x70)
        put_u32(blob, 0x40 + 0x04, 0x90)
        put_u32(blob, 0x70, 0x78)
        put_u32(blob, 0x78 + 0x04, 10)
        put_u32(blob, 0x90, 0x98)
        put_u32(blob, 0x98 + 0x04, 20)
        run(
            0x004D6AB0,
            blob,
            [search_key],
            4,
            a0=search_key,
            fixups=["0xc", "0x10", "0x24", "0x44", "0x70", "0x90"],
            compare="ptr",
        )

    # 39. 0x004D7040 - removeObjectIdSwapLastAt24_4D
    for obj_id in (101, 202, 404):
        blob = _fill(0x160, 0)
        put_u32(blob, 0x24, 0x40)
        put_u32(blob, 0x28, 0x4C)
        put_u32(blob, 0x40, 101)
        put_u32(blob, 0x44, 202)
        put_u32(blob, 0x48, 303)
        put_u32(blob, 0x50 + 0xDC, obj_id)
        run(
            0x004D7040,
            blob,
            lambda b: [b + 0x50],
            4,
            a0=0x50,
            fixups=["0x24", "0x28"],
            compare="u8",
        )

    # 40. 0x004D8390 - resolveAIUpgradeTargetTemplateRef4D
    for kind, ref_val in ((0, 0x1234), (1, 0x1111), (2, 0x2222), (3, 0x3333), (4, 0), (6, 0x6666), (13, 0xDDDD), (14, 0xEEEE)):
        blob = _fill(0x400, 0)
        put_u32(blob, 0x04, 0x10)
        put_u32(blob, 0x10 + 0x04, kind)
        put_u32(blob, 0x10 + 0x0C, 0x30)
        put_u32(blob, 0x10 + 0x10, 0x30)
        put_u32(blob, 0x10 + 0x14, 0x30)
        put_u32(blob, 0x30, 0x40)
        put_u32(blob, 0x40 + 0x04, ref_val)
        put_u32(blob, 0x40 + 0x28, ref_val)
        put_u32(blob, 0x40 + 0x318, ref_val)
        run(
            0x004D8390,
            blob,
            [],
            0,
            fixups=["0x4", "0x1c", "0x20", "0x24", "0x30"],
            compare="u32",
        )

    # 41. 0x004D86D0 - isDamageAndSpawnTableEntryFlag90Set4D
    for idx, flag_val in ((-1, 1), (0, 0), (0, 1), (1, 1), (2, 1)):
        blob = _fill(0x200, 0)
        put_i32(blob, 0x00, idx)
        t_begin = 0x20
        t_end = 0x20 + 2 * 0x9C
        if 0 <= idx < 2:
            blob[t_begin + idx * 0x9C + 0x90] = flag_val

        def setup_86d0(emu: Emulator, base: int, tb=t_begin, te=t_end) -> None:
            emu.write(0x00DEBB30, struct.pack("<I", base + tb))
            emu.write(0x00DEBB34, struct.pack("<I", base + te))

        run(
            0x004D86D0,
            blob,
            [],
            0,
            a0=t_begin,
            a1=t_end,
            compare="u8",
            setup=setup_86d0,
        )

    # 42. 0x004D8780 - matchesObjectStatusFilter4D
    # Null object -> false
    blob = _fill(0x80, 0)
    run(0x004D8780, blob, [0], 4, a0=NULL, compare="u8")
    # Object with status bits
    for req_bit, excl_bit, obj_bit in ((0, 0, 1), (1, 0, 1), (2, 0, 1), (1, 1, 1), (1, 2, 1)):
        blob = _fill(0x180, 0)
        put_u32(blob, 0x58, 0x80)
        put_u32(blob, 0x5C, 0xA0)
        put_u32(blob, 0x80, req_bit)
        put_u32(blob, 0xA0, excl_bit)
        put_u32(blob, 0xC0 + 0x80, obj_bit)
        run(
            0x004D8780,
            blob,
            lambda b: [b + 0xC0],
            4,
            a0=0xC0,
            fixups=["0x58", "0x5c"],
            compare="u8",
        )

    # 43. 0x004D8810 - matchesRelationshipFilter4D
    for mask_val, rel, p_a, p_b in (
        (0, 0, 1, 2),
        (0x02, 0, 1, 2),
        (0x04, 0, 1, 2),
        (0x04, 1, 1, 2),
        (0x01, 2, 1, 2),
        (0x08, 2, 5, 5),
        (0x08, 2, 5, 6),
        (0xFF, 3, 1, 1),
    ):
        blob = _fill(0x80, 0)
        put_u32(blob, 0x00, mask_val)
        put_u32(blob, 0x10 + 0x20, p_a)
        put_u32(blob, 0x40 + 0x20, p_b)
        run(
            0x004D8810,
            blob,
            lambda b, r=rel: [b + 0x00, b + 0x10, b + 0x40, r],
            0,
            a0=0x00,
            a1=0x10,
            a2=0x40,
            a3=rel,
            compare="u8",
        )

    # 44. 0x004D8930 - findTaggedContactMidpoint4D
    # Empty list
    blob = _fill(0x240, 0)
    put_u32(blob, 0x1A8, 0x200)
    put_u32(blob, 0x1AC, 0x200)
    run(
        0x004D8930,
        blob,
        lambda b: [b, b + 0x200],
        8,
        a0=0x00,
        a1=0x200,
        fixups=["0x1a8", "0x1ac"],
        compare="u8",
    )
    # 2 entries, second has tag 0x9063FB6C
    for cw in X87_CONTROL_WORDS:
        blob = _fill(0x300, 0)
        put_u32(blob, 0x1A8, 0x1C0)
        put_u32(blob, 0x1AC, 0x1C0 + 2 * 0x14)
        put_u32(blob, 0x1C0 + 0x04, 0x200)
        put_u32(blob, 0x1D4 + 0x04, 0x240)
        put_u32(blob, 0x200, 0x11223344)
        put_u32(blob, 0x240, 0x9063FB6C)
        put_f32(blob, 0x240 + 0x14, 10.0)
        put_f32(blob, 0x240 + 0x18, 20.0)
        put_f32(blob, 0x240 + 0x1C, 30.0)
        put_f32(blob, 0x240 + 0x20, 13.0)
        put_f32(blob, 0x240 + 0x24, 24.0)
        put_f32(blob, 0x240 + 0x28, 42.0)
        run(
            0x004D8930,
            blob,
            lambda b: [b, b + 0x280],
            8,
            a0=0x00,
            a1=0x280,
            fpcw=cw,
            fixups=["0x1a8", "0x1ac", "0x1c4", "0x1d8"],
            compare="u8",
        )

    # 45. 0x004D9230 - buildZRotationMatrix4D
    for angle in (0.0, 0.5, -1.25, math.pi / 3.0):
        abits = fbits(angle)
        for cw in X87_CONTROL_WORDS:
            blob = _fill(0x40, 0xAA)
            run(
                0x004D9230,
                blob,
                lambda b, ab=abits: [b, ab],
                0,
                a0=0x00,
                a1=abits,
                fpcw=cw,
            )

    # 46. 0x004D9280 - countSetBits288_4D
    for pattern in (0x00000000, 0xFFFFFFFF, 0x55555555, 0x12345678):
        blob = _fill(0x30, 0)
        for i in range(9):
            put_u32(blob, i * 4, (pattern + i * 0x11111111) & 0xFFFFFFFF)
        run(0x004D9280, blob, [], 0, compare="u32")

    # 48, 50, 56, 57. Constructors / default resets
    for va, sz in (
        (0x004DA780, 0x50),
        (0x004DB8C0, 0xA0),
        (0x004DD390, 0x30),
        (0x004DD3D0, 0x30),
    ):
        for fill_b in (0xA5, 0xFF):
            blob = _fill(sz, fill_b)
            run(va, blob, [], 0, compare="ptr")

    # 49. 0x004DAD10 - findRegisteredPairByKey4D
    for k0, k1 in ((10, 20), (30, 40), (10, 99)):
        blob = _fill(0x240, 0)
        # sentinel at 0x1B0 -> node1 at 0x1D0 -> node2 at 0x1F0 -> sentinel 0x1B0
        put_u32(blob, 0x1B0, 0x1D0)
        put_u32(blob, 0x1D0, 0x1F0)
        put_u32(blob, 0x1D8, 10)
        put_u32(blob, 0x1DC, 20)
        put_u32(blob, 0x1F0, 0x1B0)
        put_u32(blob, 0x1F8, 30)
        put_u32(blob, 0x1FC, 40)
        put_u32(blob, 0x210, k0)
        put_u32(blob, 0x214, k1)
        run(
            0x004DAD10,
            blob,
            lambda b: [b + 0x210],
            4,
            a0=0x210,
            fixups=["0x1b0", "0x1d0", "0x1f0"],
            compare="ptr",
        )

    # 51. 0x004DBB30 - initExitGoalEntry4D
    for f20 in (0, 1):
        blob = _fill(0x30, 0xCC)
        run(
            0x004DBB30,
            blob,
            [11, 22, 33, 44, f20],
            0x14,
            a0=11,
            a1=22,
            a2=33,
            a3=44,
            a4=f20,
            compare="ptr",
        )

    # 52. 0x004DBBF0 - gridToWorldPlacement4D
    for gx, gy, center in ((0, 0, 0), (3, 5, 0), (3, 5, 1), (-2, 4, 1)):
        blob = _fill(0x100, 0)
        _init_grid_transform(blob, 0x00)
        run(
            0x004DBBF0,
            blob,
            lambda b, x=gx, y=gy, c=center: [b + 0xD0, x, y, c],
            0x10,
            a0=0xD0,
            a1=gx,
            a2=gy,
            a3=center,
        )

    # 53. 0x004DBD20 - snapWorldPointToGrid4D
    for wx, wy, wz in ((0.0, 0.0, 5.0), (73.2, 141.8, 9.5), (-48.0, 62.0, -3.0)):
        blob = _fill(0x100, 0)
        _init_grid_transform(blob, 0x00)
        put_f32(blob, 0xD0, wx)
        put_f32(blob, 0xD4, wy)
        put_f32(blob, 0xD8, wz)
        run(0x004DBD20, blob, lambda b: [b + 0xD0], 4, a0=0xD0)

    # 54. 0x004DBEC0 - offsetWorldPointByGridCells4D
    for dx, dy in ((0, 0), (2, -3), (-4, 5)):
        blob = _fill(0x120, 0)
        _init_grid_transform(blob, 0x00)
        put_f32(blob, 0xE0, 55.5)
        put_f32(blob, 0xE4, 88.25)
        put_f32(blob, 0xE8, 17.75)
        run(
            0x004DBEC0,
            blob,
            lambda b, x=dx, y=dy: [b + 0xD0, b + 0xE0, x, y],
            0x10,
            a0=0xD0,
            a1=0xE0,
            a2=dx,
            a3=dy,
        )

    # 55. 0x004DC080 - worldToGridCell4D
    for wx, wy, rnd in ((60.0, 90.0, 0), (60.0, 90.0, 1), (74.9, 105.2, 0), (74.9, 105.2, 1)):
        blob = _fill(0x100, 0)
        _init_grid_transform(blob, 0x00)
        put_f32(blob, 0xD0, wx)
        put_f32(blob, 0xD4, wy)
        run(
            0x004DC080,
            blob,
            lambda b, r=rnd: [b + 0xD0, b + 0xE0, b + 0xE4, r],
            0x10,
            a0=0xD0,
            a1=0xE0,
            a2=0xE4,
            a3=rnd,
        )

    # 58. 0x004DFC30 - snapRotatedFootprintToGrid4D
    # Null footprint
    blob = _fill(0x100, 0)
    _init_grid_transform(blob, 0x00)
    put_f32(blob, 0xD0, 50.0)
    put_f32(blob, 0xD4, 60.0)
    run(
        0x004DFC30,
        blob,
        lambda b: [0, b + 0xD0, fbits(0.5)],
        0x0C,
        a0=NULL,
        a1=0xD0,
        a2=fbits(0.5),
    )
    # Non-null footprint: flag68=0, flag68=1 (mode=0), flag68=1 (mode=1)
    for flag68, mode6c, angle in ((0, 0, 0.0), (0, 0, 0.75), (1, 0, 0.5), (1, 1, -0.6)):
        abits = fbits(angle)
        for cw in X87_CONTROL_WORDS:
            blob = _fill(0x200, 0)
            _init_grid_transform(blob, 0x00)
            fp_off = 0x100
            blob[fp_off + 0x68] = flag68
            put_u32(blob, fp_off + 0x6C, mode6c)
            put_f32(blob, fp_off + 0x50, 30.0)
            put_f32(blob, fp_off + 0x54, 45.0)
            put_f32(blob, fp_off + 0x74, 20.0)
            put_f32(blob, fp_off + 0x78, 35.0)
            put_f32(blob, fp_off + 0x90, 1.5)
            put_f32(blob, 0xD0, 125.0)
            put_f32(blob, 0xD4, 215.0)
            put_f32(blob, 0xD8, 10.0)
            run(
                0x004DFC30,
                blob,
                lambda b, fo=fp_off, ab=abits: [b + fo, b + 0xD0, ab],
                0x0C,
                a0=fp_off,
                a1=0xD0,
                a2=abits,
                fpcw=cw,
            )

    missing = [va for va in ACCEPTED_ENTRIES if oracle.counts.get(va, 0) == 0]
    if missing:
        raise AssertionError("untested entries: " + ", ".join(f"0x{va:08X}" for va in missing))
    total = sum(oracle.counts.values())
    print(f"logic4d oracle: {len(oracle.counts)} entries, {total} cases, 0 mismatches")
    oracle.close()


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument(
        "--logic4dcheck",
        type=Path,
        default=Path(__file__).resolve().parents[3]
        / "build/win32/bin/Release/ra3_logic4dcheck.exe",
    )
    args = parser.parse_args()
    verify(args.logic4dcheck.resolve())


if __name__ == "__main__":
    main()
