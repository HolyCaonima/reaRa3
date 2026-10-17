"""Differentially verify the skirm87 AI and client-behavior cluster against ra3_1.0.game."""

from __future__ import annotations

import argparse
import struct
import subprocess
import sys
from pathlib import Path
from typing import Callable

from unicorn import x86_const

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from emulate import HEAP_BASE, CallResult, Emulator  # noqa: E402

ACCEPTED_ENTRIES = (
    0x008865E0, 0x00886680, 0x0087D2B0, 0x00886660, 0x0087D2A0, 0x00881820,
    0x00882E20, 0x0087EDD0, 0x00882E60, 0x00882E90, 0x00882E70, 0x0087D860,
    0x0087F450, 0x00885700, 0x0087CB80, 0x0087D3E0, 0x00885BB0, 0x00886640,
    0x0087D870, 0x0087D760, 0x0087D7D0, 0x0087D890, 0x0087D940, 0x0087DC90,
    0x0087C730, 0x0087C830, 0x0087C400, 0x0087C4D0, 0x008822E0, 0x00887870,
    0x00887BE0, 0x00887C90, 0x0087FD80, 0x0087D6E0, 0x00873E00, 0x00873EB0,
    0x00874170, 0x00874470, 0x00874250, 0x00874160, 0x00874870,
)

XFER = 0x200
VTAB = 0x280
XFER_SLOTS = (0x18, 0x20, 0x58, 0x64, 0x6C, 0x70, 0x84, 0x88)
LOGIC_GLOBAL = 0x00DEBA64
LOGIC_STATE = 0x00CB7420
NULL = 0xFFFFFFFF


def u32(data: bytes | bytearray, offset: int) -> int:
    return struct.unpack_from("<I", data, offset)[0]


def put_u32(data: bytearray, offset: int, value: int) -> None:
    struct.pack_into("<I", data, offset, value & 0xFFFFFFFF)


def put_f32(data: bytearray, offset: int, value: float) -> None:
    struct.pack_into("<f", data, offset, value)


def _fill(size: int, value: int = 0xA5) -> bytearray:
    return bytearray([value & 0xFF]) * size


def _second_alloc(blob_len: int) -> int:
    end = HEAP_BASE + max(blob_len, 1)
    return (end + 15) & ~15


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
    ) -> tuple[int, bytes, int]:
        assert self.process.stdin is not None and self.process.stdout is not None
        fix_str = ",".join(fixups) if fixups else "-"
        line = (
            f"0x{va:08X} 0x{self_offset:X} {blob.hex()} {fix_str} "
            f"0x{a0 & 0xFFFFFFFF:X} 0x{a1 & 0xFFFFFFFF:X} 0x{a2 & 0xFFFFFFFF:X}\n"
        )
        self.process.stdin.write(line)
        self.process.stdin.flush()
        out = self.process.stdout.readline().strip()
        if not out.startswith("ret=0x"):
            raise RuntimeError(f"ra3_skirm87check failed on 0x{va:08X}: {out}")
        parts = out.split()
        ret_val = int(parts[0].split("=")[1], 16)
        out_blob = bytes.fromhex(parts[1].split("=")[1])
        st_val = int(parts[2].split("=")[1], 16)
        return ret_val, out_blob, st_val

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

    def _bind_vstubs(self) -> None:
        def version_stub(emu: Emulator) -> int:
            xfer = emu.register(x86_const.UC_X86_REG_ECX)
            pair = emu.stack_arg(0)
            if emu.read(xfer + 0x20, 1)[0] != 0:
                emu.write(pair + 1, emu.read(xfer + 0x21, 1))
            count = struct.unpack("<I", emu.read(xfer + 0x24, 4))[0]
            emu.write(xfer + 0x24, struct.pack("<I", (count + 1) & 0xFFFFFFFF))
            return 0

        def field_stub(salt: int) -> Callable[[Emulator], int]:
            def stub(emu: Emulator) -> int:
                xfer = emu.register(x86_const.UC_X86_REG_ECX)
                field = emu.stack_arg(0)
                if field != 0:
                    cur = struct.unpack("<I", emu.read(field, 4))[0]
                    emu.write(field, struct.pack("<I", (cur ^ salt) & 0xFFFFFFFF))
                count = struct.unpack("<I", emu.read(xfer + 0x28, 4))[0]
                emu.write(xfer + 0x28, struct.pack("<I", (count + 1) & 0xFFFFFFFF))
                return 0

            return stub

        def named_stub(emu: Emulator) -> int:
            xfer = emu.register(x86_const.UC_X86_REG_ECX)
            name = emu.stack_arg(0)
            field = emu.stack_arg(1)
            count = emu.stack_arg(2)
            salt = (0x77770000 ^ (name & 0xFF) ^ count) & 0xFFFFFFFF
            if field != 0:
                cur = struct.unpack("<I", emu.read(field, 4))[0]
                emu.write(field, struct.pack("<I", (cur ^ salt) & 0xFFFFFFFF))
            seen = struct.unpack("<I", emu.read(xfer + 0x28, 4))[0]
            emu.write(xfer + 0x28, struct.pack("<I", (seen + 1) & 0xFFFFFFFF))
            return 0

        def phase_stub(emu: Emulator) -> int:
            self_ptr = emu.register(x86_const.UC_X86_REG_ECX)
            phase = emu.stack_arg(0)
            emu.write(self_ptr + 0x18, struct.pack("<I", phase & 0xFFFFFFFF))
            return 0xA11

        salts = {
            0x20: 0x11110001,
            0x58: 0x22220002,
            0x64: 0x33330003,
            0x6C: 0x44440004,
            0x70: 0x55550005,
            0x84: 0x66660006,
        }
        self.vstubs[0x18] = self.emu.bind_stub(version_stub, 4)
        for slot, salt in salts.items():
            self.vstubs[slot] = self.emu.bind_stub(field_stub(salt), 4)
        self.vstubs[0x88] = self.emu.bind_stub(named_stub, 12)
        self.vstubs[0x28] = self.emu.bind_stub(phase_stub, 4)

    def check(
        self,
        va: int,
        self_offset: int,
        blob: bytes | bytearray,
        fixups: list[str],
        invoke: Callable[[Emulator, int], tuple],
        cleanup: int,
        *,
        a0: int = 0,
        a1: int = 0,
        a2: int = 0,
        compare_ret: str = "none",
        compare_st: bool = False,
    ) -> None:
        raw_blob = bytes(blob)
        cpp_ret, cpp_blob, cpp_st = self.cpp.call(
            va, self_offset, raw_blob, fixups, a0, a1, a2)

        self.emu.reset_data()
        base = self.emu.alloc(raw_blob)
        for item in fixups:
            if item.startswith("v") and "@" in item:
                kind_text, off_text = item[1:].split("@", 1)
                kind = int(kind_text, 16)
                off = int(off_text, 0)
                self.emu.write(base + off, struct.pack("<I", self.vstubs[kind]))
            else:
                off = int(item, 0)
                rel = u32(raw_blob, off)
                self.emu.write(base + off, struct.pack("<I", (base + rel) & 0xFFFFFFFF))

        outcome = invoke(self.emu, base)
        res: CallResult = outcome[0]
        game_ret = outcome[1]
        game_st = outcome[2] if len(outcome) > 2 else 0
        if res.stack_cleanup != cleanup:
            raise AssertionError(
                f"0x{va:08X}: stack cleanup {res.stack_cleanup}, expected {cleanup}"
            )

        game_blob = bytearray(self.emu.read(base, len(raw_blob)))
        reverse = {addr: kind for kind, addr in self.vstubs.items()}
        for off in range(0, len(game_blob) - 3, 4):
            cur = u32(game_blob, off)
            if base <= cur < base + len(game_blob):
                put_u32(game_blob, off, cur - base)
            elif cur in reverse:
                put_u32(game_blob, off, 0xF0000000 | reverse[cur])

        if bytes(game_blob) != cpp_blob:
            for idx, (game_byte, cpp_byte) in enumerate(zip(game_blob, cpp_blob)):
                if game_byte != cpp_byte:
                    start = max(0, idx - 4)
                    end = min(len(game_blob), idx + 8)
                    raise AssertionError(
                        f"0x{va:08X}: blob mismatch at +0x{idx:X}: "
                        f"game=0x{game_byte:02X} cpp=0x{cpp_byte:02X} "
                        f"(game={game_blob[start:end].hex()} cpp={cpp_blob[start:end].hex()})"
                    )
            if len(game_blob) != len(cpp_blob):
                raise AssertionError(
                    f"0x{va:08X}: blob length game={len(game_blob)} cpp={len(cpp_blob)}"
                )

        if compare_ret != "none":
            if compare_ret == "ptr":
                if game_ret == 0:
                    game_val = 0
                elif base <= game_ret < base + len(raw_blob):
                    game_val = game_ret - base
                else:
                    game_val = game_ret & 0xFFFFFFFF
            else:
                game_val = game_ret & 0xFFFFFFFF
            if game_val != (cpp_ret & 0xFFFFFFFF):
                raise AssertionError(
                    f"0x{va:08X}: return mismatch ({compare_ret}): "
                    f"game=0x{game_val:08X} cpp=0x{cpp_ret & 0xFFFFFFFF:08X}"
                )

        if compare_st and (game_st & 0xFFFFFFFF) != (cpp_st & 0xFFFFFFFF):
            raise AssertionError(
                f"0x{va:08X}: st mismatch game=0x{game_st & 0xFFFFFFFF:08X} "
                f"cpp=0x{cpp_st & 0xFFFFFFFF:08X}"
            )

        self.counts[va] = self.counts.get(va, 0) + 1

    def close(self) -> None:
        self.cpp.close()


def xfer_fixups() -> list[str]:
    fixes = [f"0x{XFER:X}"]
    for slot in XFER_SLOTS:
        fixes.append(f"v{slot:X}@0x{VTAB + slot:X}")
    return fixes


def make_xfer(flags: int = 0, override: int | None = None, fill: int = 0xA5,
              size: int = 0x320) -> bytearray:
    blob = _fill(size, fill)
    put_u32(blob, XFER, VTAB)
    put_u32(blob, XFER + 4, flags)
    blob[XFER + 0x20] = 0 if override is None else 1
    blob[XFER + 0x21] = 0 if override is None else override & 0xFF
    put_u32(blob, XFER + 0x24, 0)
    put_u32(blob, XFER + 0x28, 0)
    return blob


def plain(machine: Emulator, base: int, va: int, stack: list[int], ecx: int) -> tuple:
    result = machine.call(va, stack, ecx=ecx)
    return result, result.eax


def verify(executable: Path) -> None:
    oracle = Verifier(executable)
    fixes = xfer_fixups()

    def run_xfer(va: int, blob: bytearray, *, extra: list[str] | None = None) -> None:
        use = fixes + (extra or [])

        def invoke(machine: Emulator, base: int):
            return plain(machine, base, va, [base + XFER], base)

        oracle.check(va, 0, blob, use, invoke, 4, a0=XFER)

    # Tactic record writers. The stack argument is the record, not this.
    for fill, target in ((0xA5, 0), (0x11, 0x10), (0x00, 0x20)):
        blob = _fill(0x280, fill)
        oracle.check(
            0x008865E0, 0, blob, [],
            lambda m, b, t=target: plain(m, b, 0x008865E0, [b + t], 0),
            4, a0=target, compare_ret="u32")
        blob = _fill(0x300, fill)
        put_u32(blob, target + 0x294, 0x01 if fill else 0xFFFFFFF0)
        oracle.check(
            0x00886680, 0, blob, [],
            lambda m, b, t=target: plain(m, b, 0x00886680, [b + t], 0),
            4, a0=target, compare_ret="ptr")
        blob = _fill(0x250, fill)
        oracle.check(
            0x0087D2B0, 0, blob, [],
            lambda m, b, t=target: plain(m, b, 0x0087D2B0, [b + t], 0),
            4, a0=target, compare_ret="ptr")
        blob = _fill(0x260, fill)
        put_u32(blob, target + 0x21C, 0 if fill else 0x02000000)
        oracle.check(
            0x00886660, 0, blob, [],
            lambda m, b, t=target: plain(m, b, 0x00886660, [b + t], 0),
            4, a0=target, compare_ret="ptr")

    for nested, prior in ((0x40, 0x5A), (0x80, 0x00), (0x10, 0xFF)):
        blob = _fill(0x1A0)
        put_u32(blob, 0x2C, nested)
        blob[nested + 0x10A] = prior
        oracle.check(
            0x0087D2A0, 0, blob, ["0x2C"],
            lambda m, b: plain(m, b, 0x0087D2A0, [], b),
            0, compare_ret="ptr")

    for argument in (0, 1, 0x80000000, 0xFFFFFFFF):
        oracle.check(
            0x00881820, 0, _fill(4), [],
            lambda m, b, arg=argument: plain(m, b, 0x00881820, [arg], 0),
            4, a0=argument, compare_ret="u32")

    def plant_position(blob: bytearray, record: int | None, position: int | None,
                       radius: int | None) -> list[str]:
        extra: list[str] = []
        if record is None:
            put_u32(blob, 0x70, 0)
        else:
            put_u32(blob, 0x70, record)
            extra.append("0x70")
            put_u32(blob, record + 0x0C, 0 if position is None else position)
            put_u32(blob, record + 0x10, 0 if radius is None else radius)
            if position is not None:
                extra.append(f"0x{record + 0x0C:X}")
            if radius is not None:
                extra.append(f"0x{record + 0x10:X}")
        return extra

    for record, position, radius, dest, values in (
        (0x40, 0x80, 0x90, 0x100, (1.5, -2.0, 0.25)),
        (0x40, None, 0x90, 0x100, None),
        (None, None, None, 0x100, None),
        (0x40, 0x80, None, 0xB0, (0.0, 3.5, -0.5)),
    ):
        blob = _fill(0x140)
        extra = plant_position(blob, record, position, radius)
        if values is not None and position is not None:
            for index, value in enumerate(values):
                put_f32(blob, position + index * 4, value)
        if radius is not None:
            put_f32(blob, radius, 1.5 if values else -0.25)
        oracle.check(
            0x00882E20, 0, blob, extra,
            lambda m, b, d=dest: plain(m, b, 0x00882E20, [b + d], b),
            4, a0=dest, compare_ret="ptr")
        oracle.check(
            0x00882E60, 0, blob, extra,
            lambda m, b: plain(m, b, 0x00882E60, [], b),
            0, compare_ret="ptr")
        oracle.check(
            0x00882E90, 0, blob, extra,
            lambda m, b: plain(m, b, 0x00882E90, [], b),
            0, compare_ret="ptr")

        def invoke_radius(machine: Emulator, base: int):
            result, bits = machine.call_float32(0x00882E70, [], ecx=base)
            return result, result.eax, bits

        oracle.check(
            0x00882E70, 0, blob, extra, invoke_radius, 0,
            compare_ret="ptr", compare_st=True)

    for anchor, dest, values in (
        (0x60, 0x100, (4.0, 5.0, 6.0)),
        (0x30, 0x90, (0.0, -1.0, 0.5)),
        (0x80, 0x10, (-8.0, 0.125, 9.0)),
    ):
        blob = _fill(0x120)
        put_u32(blob, 0x44, anchor)
        for index, value in enumerate(values):
            put_f32(blob, anchor + index * 4, value)
        oracle.check(
            0x0087EDD0, 0, blob, ["0x44"],
            lambda m, b, d=dest: plain(m, b, 0x0087EDD0, [b + d], b),
            4, a0=dest, compare_ret="ptr")

    for bits in (0x3FC00000, 0x00000000, 0xBF800000):
        blob = _fill(0x40)
        put_u32(blob, 8, bits)
        oracle.check(
            0x0087D860, 0, blob, [],
            lambda m, b: plain(m, b, 0x0087D860, [], b),
            0, compare_ret="u32")

    for frame, due in ((0, 1), (5, 5), (9, 4), (0, 0xFFFFFFFF), (0xFFFFFFFF, 0), (1, 2)):
        blob = _fill(0x20)
        put_u32(blob, 0x14, due)

        def invoke_reached(machine: Emulator, base: int, current=frame):
            logic = machine.alloc(b"\x00" * 0x60)
            machine.write(logic + 0x58, struct.pack("<I", current & 0xFFFFFFFF))
            machine.write(LOGIC_GLOBAL, struct.pack("<I", logic))
            return plain(machine, base, 0x0087F450, [], base)

        oracle.check(
            0x0087F450, 0, blob, [], invoke_reached, 0,
            a0=frame, compare_ret="u32")

    for delay, frame in ((0, 0), (7, 5), (1, 0xFFFFFFFF), (0xFFFFFFFF, 1), (0x10, 0x20)):
        blob = _fill(0x20)
        put_u32(blob, 0x10, delay)
        logic_addr = _second_alloc(len(blob))

        def invoke_arm(machine: Emulator, base: int, current=frame):
            logic = machine.alloc(b"\x00" * 0x60)
            machine.write(logic + 0x58, struct.pack("<I", current & 0xFFFFFFFF))
            machine.write(LOGIC_GLOBAL, struct.pack("<I", logic))
            return plain(machine, base, 0x00885700, [], base)

        oracle.check(
            0x00885700, 0, blob, [], invoke_arm, 0,
            a0=logic_addr, a1=frame, compare_ret="u32")

    for present in (0, 1, 0xFFFFFFFF, 0x80000000):
        blob = _fill(0x30)
        put_u32(blob, 0x28, present)
        oracle.check(
            0x0087CB80, 0, blob, [],
            lambda m, b: plain(m, b, 0x0087CB80, [], b),
            0, compare_ret="u32")

    for seed, target in (
        ((1, 2, 3, 4, 5, 6), 0),
        ((0, 0, 0, 0, 0, 0), 0x10),
        ((0xFFFFFFFF, 0x80000000, 7, 9, 11, 13), 0x20),
        ((0x12345678, 0x9ABCDEF0, 0x11111111, 0x22222222, 0x33333333, 0x44444444), 0),
    ):
        blob = _fill(0x420)
        for index, word in enumerate(seed):
            put_u32(blob, 0x400 + index * 4, word)

        def invoke_roll(machine: Emulator, base: int, words=bytes(blob[0x400:0x418]), off=target):
            machine.write(LOGIC_STATE, words)
            return plain(machine, base, 0x0087D3E0, [base + off], 0)

        oracle.check(
            0x0087D3E0, 0, blob, [], invoke_roll, 4,
            a0=target, compare_ret="u32")

    def plant_node(blob: bytearray, node: int, parent: int, key: int,
                   result: int, right: int = 0) -> list[str]:
        put_u32(blob, node + 0, right)
        put_u32(blob, node + 4, 0)
        put_u32(blob, node + 8, parent)
        put_u32(blob, node + 0x10, result)
        first = node + 0x30
        second = node + 0x44
        third = node + 0x58
        slot = node + 0x74
        put_u32(blob, node + 0x14, first)
        put_u32(blob, first + 0x0C, second)
        put_u32(blob, second + 4, third)
        put_u32(blob, third + 0x18, slot)
        put_u32(blob, slot, key)
        extra = [
            f"0x{node + 8:X}", f"0x{node + 0x10:X}", f"0x{node + 0x14:X}",
            f"0x{first + 0x0C:X}", f"0x{second + 4:X}", f"0x{third + 0x18:X}",
        ]
        if right:
            extra.append(f"0x{node:X}")
        return extra

    # Empty header, a miss, a direct hit, and a hit that requires the successor.
    blob = _fill(0x40)
    put_u32(blob, 0x18, 0x14)
    oracle.check(
        0x00885BB0, 0, blob, ["0x18"],
        lambda m, b: plain(m, b, 0x00885BB0, [2], b),
        4, a0=2, compare_ret="ptr")
    blob = _fill(0x180)
    extra = ["0x18"] + plant_node(blob, 0x80, 0x14, 9, 0x40)
    put_u32(blob, 0x18, 0x80)
    oracle.check(
        0x00885BB0, 0, blob, extra,
        lambda m, b: plain(m, b, 0x00885BB0, [2], b),
        4, a0=2, compare_ret="ptr")
    blob = _fill(0x180)
    extra = ["0x18"] + plant_node(blob, 0x80, 0x14, 2, 0x40)
    put_u32(blob, 0x18, 0x80)
    oracle.check(
        0x00885BB0, 0, blob, extra,
        lambda m, b: plain(m, b, 0x00885BB0, [2], b),
        4, a0=2, compare_ret="ptr")
    blob = _fill(0x220)
    extra = ["0x18"]
    extra += plant_node(blob, 0x80, 0x14, 1, 0x40, right=0x100)
    extra += plant_node(blob, 0x100, 0x80, 7, 0x50)
    put_u32(blob, 0x18, 0x80)
    oracle.check(
        0x00885BB0, 0, blob, extra,
        lambda m, b: plain(m, b, 0x00885BB0, [7], b),
        4, a0=7, compare_ret="ptr")
    oracle.check(
        0x00885BB0, 0, blob, extra,
        lambda m, b: plain(m, b, 0x00885BB0, [4], b),
        4, a0=4, compare_ret="ptr")

    for key, result in ((2, 0x30), (9, 0x30), (2, 0)):
        blob = _fill(0x200)
        header = 0x58
        extra = [f"0x{header + 0x18:X}"]
        extra += plant_node(blob, 0x100, header + 0x14, key, result)
        put_u32(blob, header + 0x18, 0x100)
        oracle.check(
            0x00886640, 0, blob, extra,
            lambda m, b: plain(m, b, 0x00886640, [], b),
            0, compare_ret="ptr")
    blob = _fill(0xB0)
    put_u32(blob, 0x58 + 0x18, 0x58 + 0x14)
    oracle.check(
        0x00886640, 0, blob, ["0x70"],
        lambda m, b: plain(m, b, 0x00886640, [], b),
        0, compare_ret="ptr")

    for phase in (4, 0, 3, 5, 0xFFFFFFFF):
        blob = _fill(0x80)
        put_u32(blob, 0, 0x40)
        put_u32(blob, 0x14, phase)
        oracle.check(
            0x0087D870, 0, blob, ["0x0", "v28@0x68"],
            lambda m, b: plain(m, b, 0x0087D870, [0x99], b),
            4, compare_ret="u32")

    for fill in (0xA5, 0x00, 0xFF):
        run_xfer(0x0087D760, make_xfer(fill=fill))
        run_xfer(0x0087D7D0, make_xfer(fill=fill))
        run_xfer(0x0087D890, make_xfer(fill=fill))
        run_xfer(0x0087DC90, make_xfer(fill=fill))
        run_xfer(0x0087C730, make_xfer(fill=fill))
        run_xfer(0x0087C830, make_xfer(fill=fill))
        run_xfer(0x0087D6E0, make_xfer(fill=fill))
        run_xfer(0x00873EB0, make_xfer(fill=fill))
        run_xfer(0x00874170, make_xfer(fill=fill))

    for override in (None, 1, 2, 0):
        run_xfer(0x0087D940, make_xfer(override=override))
        run_xfer(0x0087C4D0, make_xfer(override=override))
        run_xfer(0x00873E00, make_xfer(override=override))

    for flags in (0, 2, 1, 0x80000002, 0xFFFFFFFD):
        run_xfer(0x0087C400, make_xfer(flags=flags, size=0x320))

    for fill in (0xA5, 0x3C, 0x00):
        run_xfer(0x008822E0, make_xfer(fill=fill))
        run_xfer(0x00887870, make_xfer(fill=fill))

    for outer, fill in ((None, 0xA5), (0x140, 0xA5), (0x140, 0x11)):
        blob = make_xfer(fill=fill)
        extra: list[str] = []
        if outer is None:
            put_u32(blob, 0x44, 0)
        else:
            put_u32(blob, 0x44, 0x140)
            put_u32(blob, 0x144, 0x160)
            put_u32(blob, 0x160, 0x180)
            put_u32(blob, 0x184, 0x13579BDF)
            extra = ["0x44", "0x144", "0x160"]
        run_xfer(0x00887BE0, blob, extra=extra)
        run_xfer(0x00887C90, blob, extra=extra)

    for override in (None, 1, 2, 3, 0):
        run_xfer(0x0087FD80, make_xfer(override=override))

    for flags in (0, 8, 7, 0x80000008, 0xFFFFFFF7):
        run_xfer(0x00874470, make_xfer(flags=flags))
        run_xfer(0x00874250, make_xfer(flags=flags))

    for self_off in (0, 0x10, NULL):
        blob = _fill(0x30)
        oracle.check(
            0x00874160, self_off, blob, [],
            lambda m, b, off=self_off: plain(
                m, b, 0x00874160, [], 0 if off >= len(blob) else b + off),
            0, compare_ret="ptr")
        oracle.check(
            0x00874870, self_off, blob, [],
            lambda m, b, off=self_off: plain(
                m, b, 0x00874870, [], 0 if off >= len(blob) else b + off),
            0, compare_ret="ptr")

    missing = [va for va in ACCEPTED_ENTRIES if oracle.counts.get(va, 0) < 3]
    if missing:
        raise AssertionError(
            "entries with fewer than 3 cases: "
            + ", ".join(f"0x{va:08X}={oracle.counts.get(va, 0)}" for va in missing)
        )
    total = sum(oracle.counts.values())
    print(f"skirm87 oracle: {len(oracle.counts)} entries, {total} cases, 0 mismatches")
    oracle.close()


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument(
        "--skirm87check", type=Path,
        default=Path(__file__).resolve().parents[3] / "build/win32/bin/Release/ra3_skirm87check.exe")
    args = parser.parse_args()
    verify(args.skirm87check.resolve())


if __name__ == "__main__":
    main()
