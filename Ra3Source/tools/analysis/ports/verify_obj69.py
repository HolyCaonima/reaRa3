"""Differentially verify the 0x0069 object-update cluster against ra3_1.0.game."""

from __future__ import annotations

import struct
import subprocess
import sys
from pathlib import Path
from typing import Callable

from unicorn import x86_const

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from emulate import CallResult, Emulator  # noqa: E402

ACCEPTED_ENTRIES = (
    0x00690740, 0x006911D0, 0x00691350, 0x006914B0, 0x006914E0, 0x006914F0,
    0x00691550, 0x00691990, 0x006919E0, 0x00691A30, 0x006923A0, 0x00692500,
    0x00692580, 0x006925F0, 0x006926D0, 0x00692850, 0x00692890, 0x00694670,
    0x00694710, 0x00694730, 0x00695670, 0x00695690, 0x006966B0, 0x00696870,
    0x00697100, 0x00697130, 0x00697720, 0x006978D0, 0x006979E0, 0x00697A40,
    0x00697AA0, 0x006981B0, 0x00699710, 0x006997B0, 0x006997E0, 0x00699840,
    0x00699860, 0x00699CA0, 0x0069B5A0, 0x0069C1B0, 0x0069E5B0,
)


def u32(data: bytes, offset: int) -> int:
    return struct.unpack_from("<I", data, offset)[0]


def put_u32(data: bytearray, offset: int, value: int) -> None:
    struct.pack_into("<I", data, offset, value & 0xFFFFFFFF)


def put_i32(data: bytearray, offset: int, value: int) -> None:
    struct.pack_into("<i", data, offset, value)


def put_f32(data: bytearray, offset: int, value: float) -> None:
    struct.pack_into("<f", data, offset, value)


def fbits(value: float) -> int:
    return struct.unpack("<I", struct.pack("<f", value))[0]


def fill(size: int, value: int = 0xA5) -> bytearray:
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

    def call(self, va: int, self_offset: int, blob: bytes, fixups: list[str],
             a0: int = 0, a1: int = 0, a2: int = 0) -> tuple[int, bytes]:
        assert self.process.stdin is not None and self.process.stdout is not None
        fix_str = ",".join(fixups) if fixups else "-"
        line = (
            f"0x{va:08X} 0x{self_offset:X} {blob.hex()} {fix_str} "
            f"0x{a0 & 0xFFFFFFFF:X} 0x{a1 & 0xFFFFFFFF:X} 0x{a2 & 0xFFFFFFFF:X} 0x037F\n"
        )
        self.process.stdin.write(line)
        self.process.stdin.flush()
        out = self.process.stdout.readline().strip()
        if not out.startswith("ret=0x"):
            raise RuntimeError(f"ra3_obj69check failed on 0x{va:08X}: {out}")
        parts = out.split()
        return int(parts[0].split("=")[1], 16), bytes.fromhex(parts[1].split("=")[1])

    def close(self) -> None:
        if self.process.stdin is not None:
            self.process.stdin.close()
        self.process.wait(timeout=10)


class Verifier:
    def __init__(self, executable: Path) -> None:
        self.emu = Emulator()
        self.cpp = CppOracle(executable)
        self.counts: dict[int, int] = {}
        self.stubs: dict[int, int] = {}
        self._bind_stubs()

    def _bind_stubs(self) -> None:
        def stub0(emu: Emulator) -> int:
            self_ptr = emu.register(x86_const.UC_X86_REG_ECX)
            result = u32(emu.read(self_ptr + 8, 4), 0)
            count = u32(emu.read(self_ptr + 0x0C, 4), 0)
            emu.write(self_ptr + 0x0C, struct.pack("<I", (count + 1) & 0xFFFFFFFF))
            return result

        def stub1(emu: Emulator) -> int:
            self_ptr = emu.register(x86_const.UC_X86_REG_ECX)
            arg = emu.stack_arg(0)
            result = u32(emu.read(self_ptr + 8, 4), 0)
            emu.write(self_ptr + 0x18, struct.pack("<I", arg & 0xFFFFFFFF))
            count = u32(emu.read(self_ptr + 0x0C, 4), 0)
            emu.write(self_ptr + 0x0C, struct.pack("<I", (count + 1) & 0xFFFFFFFF))
            return result

        def stub2(emu: Emulator) -> int:
            self_ptr = emu.register(x86_const.UC_X86_REG_ECX)
            arg0 = emu.stack_arg(0)
            arg1 = emu.stack_arg(1)
            result = u32(emu.read(self_ptr + 8, 4), 0)
            emu.write(self_ptr + 0x18, struct.pack("<I", arg0 & 0xFFFFFFFF))
            emu.write(self_ptr + 0x1C, struct.pack("<I", arg1 & 0xFFFFFFFF))
            count = u32(emu.read(self_ptr + 0x0C, 4), 0)
            emu.write(self_ptr + 0x0C, struct.pack("<I", (count + 1) & 0xFFFFFFFF))
            return result

        def stub3(emu: Emulator) -> int:
            self_ptr = emu.register(x86_const.UC_X86_REG_ECX)
            args = [emu.stack_arg(i) for i in range(3)]
            result = u32(emu.read(self_ptr + 8, 4), 0)
            for index, arg in enumerate(args):
                emu.write(self_ptr + 0x18 + 4 * index, struct.pack("<I", arg & 0xFFFFFFFF))
            count = u32(emu.read(self_ptr + 0x0C, 4), 0)
            emu.write(self_ptr + 0x0C, struct.pack("<I", (count + 1) & 0xFFFFFFFF))
            return result

        def stub4(emu: Emulator) -> int:
            self_ptr = emu.register(x86_const.UC_X86_REG_ECX)
            result = u32(emu.read(self_ptr + 0x10, 4), 0)
            count = u32(emu.read(self_ptr + 0x14, 4), 0)
            emu.write(self_ptr + 0x14, struct.pack("<I", (count + 1) & 0xFFFFFFFF))
            return result

        self.stubs[0] = self.emu.bind_stub(stub0, 0)
        self.stubs[1] = self.emu.bind_stub(stub1, 4)
        self.stubs[2] = self.emu.bind_stub(stub2, 8)
        self.stubs[3] = self.emu.bind_stub(stub3, 12)
        self.stubs[4] = self.emu.bind_stub(stub4, 0)

    def check(self, va: int, self_offset: int, blob: bytes, fixups: list[str],
              invoke: Callable, cleanup: int, *, a0: int = 0, a1: int = 0, a2: int = 0,
              compare_ret: str = "none") -> None:
        cpp_ret, cpp_blob = self.cpp.call(va, self_offset, bytes(blob), fixups, a0, a1, a2)
        self.emu.reset_data()
        self.emu.uc.reg_write(x86_const.UC_X86_REG_FPCW, 0x037F)
        base = self.emu.alloc(bytes(blob))
        for item in fixups:
            if item.startswith("v") and "@" in item:
                kind_str, off_str = item[1:].split("@", 1)
                off = int(off_str, 0)
                self.emu.write(base + off, struct.pack("<I", self.stubs[int(kind_str, 0)]))
            else:
                off = int(item, 0)
                rel = u32(blob, off)
                self.emu.write(base + off, struct.pack("<I", (base + rel) & 0xFFFFFFFF))
        res, game_ret = invoke(self.emu, base)
        if res.stack_cleanup != cleanup:
            raise AssertionError(
                f"0x{va:08X}: stack cleanup {res.stack_cleanup}, expected {cleanup}")
        game_blob = bytearray(self.emu.read(base, len(blob)))
        stub_rev = {addr: kind for kind, addr in self.stubs.items()}
        for off in range(0, len(game_blob) - 3, 4):
            cur = u32(game_blob, off)
            if base <= cur < base + len(game_blob):
                put_u32(game_blob, off, cur - base)
            elif cur in stub_rev:
                put_u32(game_blob, off, 0xF0000000 | stub_rev[cur])
        if bytes(game_blob) != cpp_blob:
            for idx, (g, c) in enumerate(zip(game_blob, cpp_blob)):
                if g != c:
                    raise AssertionError(
                        f"0x{va:08X}: blob mismatch at +0x{idx:X}: "
                        f"game=0x{g:02X} cpp=0x{c:02X}")
        if compare_ret != "none":
            assert game_ret is not None
            if compare_ret == "u8":
                g_val, c_val = game_ret & 0xFF, cpp_ret & 0xFF
            elif compare_ret == "ptr":
                if game_ret == 0:
                    g_val = 0
                elif base <= game_ret < base + len(blob):
                    g_val = game_ret - base
                else:
                    g_val = game_ret & 0xFFFFFFFF
                c_val = cpp_ret & 0xFFFFFFFF
            else:
                g_val, c_val = game_ret & 0xFFFFFFFF, cpp_ret & 0xFFFFFFFF
            if g_val != c_val:
                raise AssertionError(
                    f"0x{va:08X}: return mismatch ({compare_ret}): "
                    f"game=0x{g_val:08X} cpp=0x{c_val:08X}")
        self.counts[va] = self.counts.get(va, 0) + 1

    def close(self) -> None:
        self.cpp.close()


def verify(executable: Path) -> None:
    oracle = Verifier(executable)

    def run(va: int, blob: bytearray, stack, cleanup: int, *, self_off: int = 0,
            a0: int = 0, a1: int = 0, a2: int = 0, fixups: list[str] | None = None,
            compare: str = "none", float_ret: bool = False) -> None:
        if va not in ACCEPTED_ENTRIES:
            raise AssertionError(hex(va))

        def invoke(machine: Emulator, base: int):
            ecx = 0 if self_off >= len(blob) else base + self_off
            args = stack(base) if callable(stack) else stack
            if float_ret:
                result, bits = machine.call_float32(va, args, ecx=ecx)
                return result, bits
            result = machine.call(va, args, ecx=ecx)
            return result, result.eax

        oracle.check(va, self_off, blob, fixups or [], invoke, cleanup,
                     a0=a0, a1=a1, a2=a2, compare_ret=compare)

    # 0x00690740 circular list head at +0xD4.
    blob = fill(0xE0)
    put_u32(blob, 0xD4, 0xD4)
    run(0x00690740, blob, [], 0, fixups=["0xD4"], compare="u32")
    blob = fill(0xE0)
    put_u32(blob, 0xD4, 0x20)
    put_u32(blob, 0x28, 0x11223344)
    run(0x00690740, blob, [], 0, fixups=["0xD4"], compare="u32")

    # 0x006914E0 field +0x5C.
    for value, _expect in ((0, 1), (4, 0)):
        blob = fill(0x60)
        put_u32(blob, 0x5C, value)
        run(0x006914E0, blob, [], 0, compare="u8")

    # 0x006926D0 field +0x78.
    for value in (0, 9):
        blob = fill(0x80)
        put_u32(blob, 0x78, value)
        run(0x006926D0, blob, [], 0, compare="u8")

    # 0x00695670 stores the argument's low byte at +0x39.
    for value in (0xAB, 0):
        blob = fill(0x40)
        run(0x00695670, blob, lambda _b, v=value: [v], 4, a0=value)

    # 0x0069C1B0 null this, and this+0x24.
    blob = fill(0x30)
    run(0x0069C1B0, blob, [], 0, self_off=0xFFFFFFFF, compare="ptr")
    run(0x0069C1B0, blob, [], 0, compare="ptr")

    # 0x00697130 flag bit 8 of object+0xBC.
    for flags in (0x100, 0, 0x200):
        blob = fill(0x100)
        put_u32(blob, 8, 0x20)
        put_u32(blob, 0x20 + 0xBC, flags)
        run(0x00697130, blob, [], 0, fixups=["0x8"], compare="u8")

    # 0x00694670 beam target filter.
    def beam(target_off: int, gate: int, mine_id: int, target_id: int) -> None:
        blob = fill(0x630)
        put_u32(blob, 4, 0x20)
        put_u32(blob, 8, 0x140)
        blob[0x20 + 0x10F] = gate
        put_u32(blob, 0x140 + 0x418, mine_id)
        if target_off != 0xFFFFFFFF:
            put_u32(blob, target_off + 0x418, target_id)
        run(0x00694670, blob, lambda b, off=target_off: [0 if off == 0xFFFFFFFF else b + off],
            4, a0=target_off, fixups=["0x4", "0x8"], compare="u8")

    beam(0xFFFFFFFF, 0, 1, 1)
    beam(0x200, 0, 1, 2)
    beam(0x200, 1, 1, 2)
    beam(0x200, 1, 7, 7)

    # 0x00694710 / 0x006966B0 id compares through an adjustor.
    for va, ptr_off in ((0x00694710, 0x20), (0x006966B0, 0x1C)):
        for ident, arg in ((0x10, 0x10), (0x10, 0x11)):
            blob = fill(0xA0)
            put_u32(blob, ptr_off, 0x80)
            put_u32(blob, 0x88, ident)
            run(va, blob, lambda _b, v=arg: [v], 4, self_off=0x40, a0=arg,
                fixups=[hex(ptr_off)], compare="u8")

    # 0x00694730 / 0x00695690 / 0x0069B5A0 out-parameter reads.
    blob = fill(0x120)
    put_u32(blob, 0x1C, 0x80)
    put_u32(blob, 0x8C, 0x55AA)
    run(0x00694730, blob, lambda b: [b + 0x100], 4, self_off=0x40, a0=0x100, fixups=["0x1C"])
    blob = fill(0x120)
    put_u32(blob, 0x1C, 0x80)
    put_u32(blob, 0x8C, 0)
    run(0x00694730, blob, lambda b: [b + 0x100], 4, self_off=0x40, a0=0x100, fixups=["0x1C"])
    blob = fill(0xC0)
    put_u32(blob, 0x14, 0x40)
    put_u32(blob, 0x40 + 0x54, 0x667788)
    run(0x00695690, blob, lambda b: [b + 0xA0], 4, self_off=0x20, a0=0xA0, fixups=["0x14"])
    blob = fill(0xC0)
    put_u32(blob, 0x14, 0x40)
    put_u32(blob, 0x40 + 0x54, 0)
    run(0x00695690, blob, lambda b: [b + 0xA0], 4, self_off=0x20, a0=0xA0, fixups=["0x14"])
    blob = fill(0xA0)
    put_u32(blob, 0x14, 0x40)
    put_u32(blob, 0x4C, 0x99)
    run(0x0069B5A0, blob, lambda b: [b + 0x80], 4, self_off=0x20, a0=0x80, fixups=["0x14"])
    blob = fill(0xA0)
    put_u32(blob, 0x14, 0x40)
    put_u32(blob, 0x4C, 0xABCDEF)
    run(0x0069B5A0, blob, lambda b: [b + 0x80], 4, self_off=0x20, a0=0x80, fixups=["0x14"])

    # 0x00696870 nested slot-1 read.
    blob = fill(0x140)
    put_u32(blob, 0x1C, 0x80)
    put_u32(blob, 0x88, 0xA0)
    put_u32(blob, 0xA8, 0xC0)
    put_u32(blob, 0xC0, 0xE0)
    put_u32(blob, 0xE4, 0x778899)
    run(0x00696870, blob, lambda b: [b + 0x120], 4, self_off=0x40, a0=0x120,
        fixups=["0x1C", "0x88", "0xA8", "0xC0"])
    blob = fill(0x140)
    put_u32(blob, 0x1C, 0x80)
    put_u32(blob, 0x88, 0xA0)
    put_u32(blob, 0xA8, 0xC0)
    put_u32(blob, 0xC0, 0xE0)
    put_u32(blob, 0xE4, 0)
    run(0x00696870, blob, lambda b: [b + 0x120], 4, self_off=0x40, a0=0x120,
        fixups=["0x1C", "0x88", "0xA8", "0xC0"])

    # 0x006923A0 zeroes seven dwords. 0x00692890 copies seven.
    blob = fill(0x40)
    run(0x006923A0, blob, lambda b: [b + 0x10, 0], 8, a0=0x10)
    blob = fill(0x40)
    put_u32(blob, 0x10, 0xFFFFFFFF)
    run(0x006923A0, blob, lambda b: [b + 0x10, 7], 8, a0=0x10, a1=7)
    blob = fill(0x120)
    put_u32(blob, 0x20, 0x60)
    for index in range(7):
        put_u32(blob, 0x88 + 4 * index, 0x310 + index)
    run(0x00692890, blob, lambda b: [b + 0x100, 0], 8, self_off=0x40, a0=0x100, fixups=["0x20"])
    blob = fill(0x120)
    put_u32(blob, 0x20, 0x60)
    run(0x00692890, blob, lambda b: [b + 0x100, 0], 8, self_off=0x40, a0=0x100, fixups=["0x20"])

    # 0x00692500 clamps the count at 1 and copies two ids.
    for count in (0, 5, -3):
        blob = fill(0x300)
        put_u32(blob, 0x100 + 0xDC, 0x111)
        put_u32(blob, 0x200 + 0xDC, 0x222)
        run(0x00692500, blob, lambda b, n=count: [b + 0x100, n & 0xFFFFFFFF, b + 0x200],
            0xC, a0=0x100, a1=count & 0xFFFFFFFF, a2=0x200)

    # 0x00699840 / 0x006997B0 goal floats.
    for bits in (fbits(1.5), 0):
        blob = fill(0x230)
        run(0x00699840, blob, lambda _b, v=bits: [v], 4, a0=bits)
    blob = fill(0x230)
    run(0x006997B0, blob, [], 0)
    blob = fill(0x230)
    put_u32(blob, 0x21C, 9)
    put_u32(blob, 0x220, 0x11223344)
    put_u32(blob, 0x224, 0x55667788)
    put_u32(blob, 0x228, 0x99AABBCC)
    run(0x006997B0, blob, [], 0)

    # 0x00699860 locomotor predicate.
    blob = fill(0x380)
    put_u32(blob, 0x208, 8)
    run(0x00699860, blob, [], 0, compare="u8")
    blob = fill(0x380)
    put_u32(blob, 0x208, 1)
    put_u32(blob, 0x214, 0)
    run(0x00699860, blob, [], 0, compare="u8")
    blob = fill(0x380)
    put_u32(blob, 0x214, 0x40)
    put_u32(blob, 0x44, 0x80)
    blob[0x88] = 8
    run(0x00699860, blob, [], 0, fixups=["0x214", "0x44"], compare="u8")
    blob = fill(0x380)
    put_u32(blob, 0x214, 0x40)
    put_u32(blob, 0x44, 0x80)
    blob[0x88] = 0
    put_u32(blob, 8, 0x100)
    put_u32(blob, 0x100 + 0x26C, 8)
    run(0x00699860, blob, [], 0, fixups=["0x214", "0x44", "0x8"], compare="u8")
    blob = fill(0x380)
    put_u32(blob, 0x214, 0x40)
    put_u32(blob, 0x44, 0x80)
    blob[0x88] = 0
    put_u32(blob, 8, 0x100)
    put_u32(blob, 0x100 + 0x26C, 0)
    run(0x00699860, blob, [], 0, fixups=["0x214", "0x44", "0x8"], compare="u8")

    # 0x00697720 dword sum, including an empty range.
    blob = fill(0x50)
    put_u32(blob, 0x30, 0x40)
    put_u32(blob, 0x34, 0x4C)
    put_u32(blob, 0x40, 1)
    put_u32(blob, 0x44, 2)
    put_u32(blob, 0x48, 3)
    run(0x00697720, blob, [], 0, fixups=["0x30", "0x34"], compare="u32")
    blob = fill(0x50)
    put_u32(blob, 0x30, 0x40)
    put_u32(blob, 0x34, 0x40)
    run(0x00697720, blob, [], 0, fixups=["0x30", "0x34"], compare="u32")

    # 0x006914B0 first passenger id. Interface this is 0x180, object pointer at 0x18.
    blob = fill(0x230)
    put_u32(blob, 0x18, 0x40)
    put_i32(blob, 0x40 + 0x128, 0)
    run(0x006914B0, blob, lambda b: [b + 0x220], 4, self_off=0x180, a0=0x220, fixups=["0x18"])
    blob = fill(0x230)
    put_u32(blob, 0x18, 0x40)
    put_i32(blob, 0x40 + 0x128, 2)
    put_u32(blob, 0x40 + 0x12C, 0x200)
    put_u32(blob, 0x200, 0x42)
    run(0x006914B0, blob, lambda b: [b + 0x220], 4, self_off=0x180, a0=0x220,
        fixups=["0x18", "0x16C"])

    # 0x00697100 null list returns 1; an empty vector returns 0; a nonempty one returns 1.
    def uncombine(list_begin: int, list_end: int, present: bool) -> None:
        blob = fill(0xD20)
        put_u32(blob, 0x38, 0x400)
        put_u32(blob, 0x400 + 0x374, 0x800 if present else 0)
        if present:
            put_u32(blob, 0x800 + 0x50C, list_begin)
            put_u32(blob, 0x800 + 0x510, list_end)
        fixups = ["0x38"]
        if present:
            fixups.append("0x774")
        run(0x00697100, blob, lambda _b: [0, 0, 0], 0xC, self_off=0x40,
            fixups=fixups, compare="u32")

    uncombine(0, 0, False)
    uncombine(0x10, 0x10, True)
    uncombine(0x10, 0x18, True)

    # 0x006979E0 open flag, empty, null slot, matching id, other id.
    def dock_list(flag: int, slots: list[int], ident: int) -> None:
        blob = fill(0x160)
        put_u32(blob, 0x20, 0x10)
        put_u32(blob, 0x18, flag & 0xFFFFFFFF)
        put_u32(blob, 0x80, 0xC0)
        put_u32(blob, 0x84, 0xC0 + 4 * len(slots))
        for index, slot in enumerate(slots):
            put_u32(blob, 0xC0 + 4 * index, slot)
        put_u32(blob, 0xDC, ident)
        run(0x006979E0, blob, lambda b: [b], 4, self_off=0x40, a0=0,
            fixups=["0x20", "0x80", "0x84"], compare="u8")

    dock_list(0xFFFFFFFF, [], 1)
    dock_list(0, [], 1)
    dock_list(0, [0, 5], 9)
    dock_list(0, [4, 9], 9)
    dock_list(0, [4, 5], 9)

    # 0x00697A40 slot readiness.
    def slot_ready(index: int, ident: int, prev: int, current: int, flag: int) -> None:
        blob = fill(0x100)
        put_u32(blob, 0x40, 0x90)
        put_u32(blob, 0x50, 0xA0)
        if index > 0:
            put_u32(blob, 0x90 + 4 * (index - 1), prev)
        if index >= 0:
            put_u32(blob, 0x90 + 4 * index, current)
            blob[0xA0 + index] = flag
        put_u32(blob, 0xDC, ident)
        run(0x00697A40, blob, lambda b, n=index: [b, n & 0xFFFFFFFF], 8,
            a1=index & 0xFFFFFFFF, fixups=["0x40", "0x50"], compare="u8")

    slot_ready(-1, 1, 0, 1, 1)
    slot_ready(0, 1, 0, 1, 1)
    slot_ready(1, 7, 0, 7, 1)
    slot_ready(1, 7, 3, 7, 1)
    slot_ready(1, 7, 0, 8, 1)
    slot_ready(1, 7, 0, 7, 0)

    # 0x00697AA0 mark a matching id, including a miss and an empty vector.
    blob = fill(0x170)
    put_u32(blob, 0x40, 0x20)
    put_u32(blob, 0x44, 0x2C)
    put_u32(blob, 0x50, 0x60)
    put_u32(blob, 0x20, 1)
    put_u32(blob, 0x24, 7)
    put_u32(blob, 0x28, 9)
    put_u32(blob, 0x80 + 0xDC, 7)
    run(0x00697AA0, blob, lambda b: [b + 0x80], 4, a0=0x80, fixups=["0x40", "0x44", "0x50"])
    blob = fill(0x170)
    put_u32(blob, 0x40, 0x20)
    put_u32(blob, 0x44, 0x28)
    put_u32(blob, 0x50, 0x60)
    put_u32(blob, 0x20, 1)
    put_u32(blob, 0x24, 2)
    put_u32(blob, 0x80 + 0xDC, 7)
    run(0x00697AA0, blob, lambda b: [b + 0x80], 4, a0=0x80, fixups=["0x40", "0x44", "0x50"])
    blob = fill(0x170)
    put_u32(blob, 0x40, 0x20)
    put_u32(blob, 0x44, 0x20)
    put_u32(blob, 0x50, 0x60)
    put_u32(blob, 0x80 + 0xDC, 7)
    run(0x00697AA0, blob, lambda b: [b + 0x80], 4, a0=0x80, fixups=["0x40", "0x44", "0x50"])

    # 0x006919E0 mark a 0x3C-stride record. The index is the second argument.
    def mark(index: int, records: int) -> None:
        blob = fill(0x20 + records * 0x3C + 4)
        begin = 0x20
        put_u32(blob, 8, begin)
        put_u32(blob, 0x0C, begin + records * 0x3C)
        run(0x006919E0, blob, lambda _b, n=index: [0, n & 0xFFFFFFFF], 8,
            a1=index & 0xFFFFFFFF, fixups=["0x8", "0xC"])

    mark(-1, 2)
    mark(0, 2)
    mark(2, 2)

    # 0x00691A30 copy a record vector when its flag byte is set.
    def copy_record(index: int, flag: int) -> None:
        blob = fill(0xA0)
        put_u32(blob, 8, 0x20)
        put_u32(blob, 0x0C, 0x20 + 0x3C)
        put_u32(blob, 0x20, 0x11)
        put_u32(blob, 0x24, 0x22)
        put_u32(blob, 0x28, 0x33)
        blob[0x20 + 0x39] = flag
        run(0x00691A30, blob, lambda b, n=index: [n & 0xFFFFFFFF, b + 0x80], 8,
            a0=index & 0xFFFFFFFF, a1=0x80, fixups=["0x8", "0xC"], compare="u8")

    copy_record(-1, 1)
    copy_record(0, 0)
    copy_record(0, 1)

    # 0x0069E5B0 consume one matching link.
    def consume(head: int, kind: int, token: int, flag: int, tagged: int, key_token: int,
                has_key: bool = True) -> None:
        blob = fill(0x260)
        put_u32(blob, 0x10, head)
        if head:
            put_u32(blob, head + 0x0C, kind)
            put_u32(blob, head + 0x38, token)
            put_u32(blob, head + 0x54, 0)
            blob[head + 0x64] = flag
            blob[head + 0x65] = tagged
        put_u32(blob, 0x200 + 0x0C, 0x220)
        put_u32(blob, 0x220, 0x240)
        put_u32(blob, 0x244, key_token)
        fixups = ["0x20C", "0x220"]
        if head:
            fixups.append("0x10")
        run(0x0069E5B0, blob, lambda b: [b + 0x200 if has_key else 0], 4,
            a0=0x200 if has_key else 0xFFFFFFFF, fixups=fixups, compare="u8")

    consume(0, 0, 0, 0, 0, 1, has_key=False)
    consume(0, 0, 0, 0, 0, 1)
    consume(0x100, 2, 5, 1, 0, 9)
    consume(0x100, 2, 5, 1, 0, 5)
    consume(0x100, 2, 5, 0, 0, 5)
    consume(0x100, 2, 5, 1, 1, 5)

    # 0x006981B0 transform, including a null matrix.
    blob = fill(0xA0)
    put_u32(blob, 8, 0)
    run(0x006981B0, blob, lambda b: [b + 0x80], 4, a0=0x80, compare="u8")
    blob = fill(0xC0)
    put_u32(blob, 4, 0x20)
    put_u32(blob, 8, 0x40)
    put_u32(blob, 0x28, 0x30)
    put_f32(blob, 0x30, 1.0)
    put_f32(blob, 0x34, 2.0)
    put_f32(blob, 0x38, 3.0)
    put_f32(blob, 0x40 + 0x08, 1.0)
    put_f32(blob, 0x40 + 0x14, 4.0)
    put_f32(blob, 0x40 + 0x1C, 1.0)
    put_f32(blob, 0x40 + 0x30, 1.0)
    run(0x006981B0, blob, lambda b: [b + 0x90], 4, a0=0x90,
        fixups=["0x4", "0x8", "0x28"], compare="u8")

    # 0x00699710 path-goal predicate.
    def path_goal(mode: int, path: int, count: int, node: int, child: int,
                  node_goal: int, child_goal: int) -> None:
        blob = fill(0x240)
        put_u32(blob, 0x214, 1 if path or mode else 1)
        put_u32(blob, 0x21C, mode)
        put_u32(blob, 0x150, path)
        if path:
            put_u32(blob, path + 0x10, count)
            put_u32(blob, path + 0x24, node)
        if node:
            put_u32(blob, node + 8, child)
            put_u32(blob, node + 0x20, node_goal)
        if child:
            put_u32(blob, child + 0x20, child_goal)
        fixups = []
        if path:
            fixups.append("0x150")
        if node:
            fixups.append(hex(path + 0x24))
        if child:
            fixups.append(hex(node + 8))
        run(0x00699710, blob, [], 0, fixups=fixups, compare="u8")

    path_goal(0, 0, 0, 0, 0, 0, 0)
    path_goal(1, 0, 0, 0, 0, 0, 0)
    path_goal(1, 0x40, 0, 0, 0, 0, 0)
    path_goal(1, 0x40, 1, 0, 0, 0, 0)
    path_goal(1, 0x40, 1, 0x80, 0, 1, 0)
    path_goal(1, 0x40, 1, 0x80, 0xC0, 5, 0x7FFFFFFF)
    path_goal(2, 0x40, 1, 0x80, 0, 1, 0)
    path_goal(4, 0x40, 1, 0x80, 0, 1, 0)

    # 0x006997E0 release a held object, then copy a position.
    def set_goal(mode: int, refcount: int | None) -> None:
        blob = fill(0x250)
        put_u32(blob, 0x21C, mode)
        put_u32(blob, 0x150, 0)
        put_u32(blob, 0x40, 0x111)
        put_u32(blob, 0x44, 0x222)
        put_u32(blob, 0x48, 0x333)
        fixups = []
        if refcount is not None:
            put_u32(blob, 0x150, 0x80)
            put_u32(blob, 0x80, 0x100)
            put_i32(blob, 0x84, refcount)
            put_u32(blob, 0x100, 0)
            fixups = ["0x150", "0x80", "v1@0x100"]
        run(0x006997E0, blob, lambda b: [b + 0x40], 4, a0=0x40, fixups=fixups)

    set_goal(4, None)
    set_goal(1, None)
    set_goal(1, 3)
    set_goal(1, 1)

    # 0x00699CA0 two virtuals, storing the second argument.
    blob = fill(0xC0)
    put_u32(blob, 0x3C, 0x40)
    put_u32(blob, 0x40, 0x80)
    put_u32(blob, 0x48, 0x15)
    run(0x00699CA0, blob, lambda _b: [0x123, 0x456], 8, a0=0x123, a1=0x456,
        fixups=["0x3C", "0x40", "v0@0xB4", "v1@0x9C"])
    blob = fill(0xC0)
    put_u32(blob, 0x3C, 0x40)
    put_u32(blob, 0x40, 0x80)
    put_u32(blob, 0x48, 0)
    run(0x00699CA0, blob, lambda _b: [0, 0], 8, a0=0, a1=0,
        fixups=["0x3C", "0x40", "v0@0xB4", "v1@0x9C"])

    # 0x00691350 invert a no-argument virtual result. The callee's this is the
    # address of the pointer field; the function pointer is pointee+0xF4.
    for planted in (0, 5):
        blob = fill(0x280)
        put_u32(blob, 0x134, 0x180)
        put_u32(blob, 0x13C, planted)
        run(0x00691350, blob, [], 0, fixups=["0x134", "v0@0x274"])

    # 0x006914F0 / 0x00691550 walk a passenger vector.
    def notify(va: int, slot: int, kind: str, elements: list[int]) -> None:
        blob = fill(0x500)
        put_u32(blob, 0x98, 0x80)
        put_u32(blob, 0x200, 0x300)
        put_u32(blob, 0x80 + 0x1E8, len(elements))
        put_u32(blob, 0x80 + 0x1EC, 0x340)
        for index, element in enumerate(elements):
            put_u32(blob, 0x340 + 4 * index, element)
        put_u32(blob, 0x300 + slot, 0)
        run(va, blob, [], 0, self_off=0x200,
            fixups=["0x98", "0x200", "0x26C", f"{kind}@0x{0x300 + slot:X}"])

    notify(0x006914F0, 0x1D0, "v3", [])
    notify(0x006914F0, 0x1D0, "v3", [0])
    notify(0x006914F0, 0x1D0, "v3", [0x11, 0x22])
    notify(0x00691550, 0x1D8, "v2", [])
    notify(0x00691550, 0x1D8, "v2", [0x11, 0])

    # 0x00691990 early player filter, then the two virtual results.
    def slaughter(bit: int, mine: int, theirs: int, left: int, right: int) -> None:
        blob = fill(0x560)
        blob[0x40 + 0x8C] = bit
        put_u32(blob, 0x40 + 0x418, theirs)
        put_u32(blob, 8, 0x100)
        put_u32(blob, 0x100 + 0x418, mine)
        put_u32(blob, 0x24, 0x200)
        put_u32(blob, 0x2C, left)
        put_u32(blob, 0x34, right)
        run(0x00691990, blob, lambda b: [b + 0x40], 4, a0=0x40,
            fixups=["0x8", "0x24", "v1@0x30C", "v4@0x278"], compare="u8")

    slaughter(1, 1, 2, 0, 0)
    slaughter(1, 4, 4, 1, 2)
    slaughter(0, 4, 9, 5, 5)
    slaughter(0, 4, 9, 1, 8)

    # 0x00692580 / 0x006925F0 plane tests.
    def plane(limit: float, body_z: float, cand_z: float, direction: float, candidate: bool) -> None:
        blob = fill(0x180)
        put_u32(blob, 0x20, 0x10)
        put_u32(blob, 0x24, 0x80)
        put_f32(blob, 0x10 + 0x18, limit)
        put_f32(blob, 0x80 + 0x40, body_z)
        put_f32(blob, 0x80 + 0x38, 0.0)
        put_f32(blob, 0x80 + 0x3C, 0.0)
        if candidate:
            put_f32(blob, 0x100 + 0x40, cand_z)
            put_f32(blob, 0x100 + 0x38, 1.0)
            put_f32(blob, 0x100 + 0x3C, 0.0)
            put_f32(blob, 0x100 + 8, direction)
            put_f32(blob, 0x100 + 0x18, 0.0)
        run(0x00692580, blob,
            lambda b: [0 if not candidate else b + 0x100], 4,
            self_off=0x40, a0=0xFFFFFFFF if not candidate else 0x100,
            fixups=["0x20", "0x24"], compare="u8")

    plane(1.0, 0.0, 0.0, 0.0, False)
    plane(-1.0, 0.0, 0.0, -1.0, True)
    plane(1.0, 0.0, 5.0, -1.0, True)
    plane(10.0, 0.0, 1.0, 1.0, True)
    plane(10.0, 0.0, 1.0, -1.0, True)

    def pair_plane(limit: float, first: bool, second: bool, direction_x: float) -> None:
        blob = fill(0x1C0)
        put_u32(blob, 0x20, 0x10)
        put_u32(blob, 0x24, 0x80)
        put_f32(blob, 0x10 + 0x18, limit)
        put_f32(blob, 0x80 + 0x38, 0.0)
        put_f32(blob, 0x80 + 0x3C, 0.0)
        put_f32(blob, 0x80 + 0x40, 0.0)
        if first:
            put_f32(blob, 0x100 + 0x38, 1.0)
            put_f32(blob, 0x100 + 0x3C, 0.0)
            put_f32(blob, 0x100 + 0x40, 0.0)
        if second:
            put_f32(blob, 0x140 + 0x38, 1.0 + direction_x)
            put_f32(blob, 0x140 + 0x3C, 0.0)
            put_f32(blob, 0x140 + 0x40, 0.0)
        run(0x006925F0, blob,
            lambda b: [0 if not first else b + 0x100, 0 if not second else b + 0x140], 8,
            self_off=0x40,
            a0=0xFFFFFFFF if not first else 0x100,
            a1=0xFFFFFFFF if not second else 0x140,
            fixups=["0x20", "0x24"], compare="u8")

    pair_plane(1.0, False, True, 0.0)
    pair_plane(1.0, True, False, 0.0)
    pair_plane(-1.0, True, True, 1.0)
    pair_plane(10.0, True, True, -1.0)
    pair_plane(10.0, True, True, 1.0)

    # 0x006911D0 range decay. 0x006978D0 doubled range.
    def decay(value: float, base: float, argument: float) -> None:
        blob = fill(0x380)
        put_u32(blob, 0x9C, 0x20)
        put_f32(blob, 0x20 + 0x120, base)
        put_f32(blob, 0x200 + 0x160, value)
        bits = fbits(argument)
        run(0x006911D0, blob, lambda _b, v=bits: [v], 4, self_off=0x200, a0=bits,
            fixups=["0x9C"], compare="u32", float_ret=True)

    decay(1.0, 1.0, 3.0)
    decay(2.0, 0.0, 1.0)
    decay(2.0, 0.0, 100.0)

    def scale(gate: int, value: float) -> None:
        blob = fill(0x600)
        put_u32(blob, 0x0C, 0x20)
        put_u32(blob, 0x08, 0x40)
        put_u32(blob, 0x20 + 0x418, 0x480)
        put_u32(blob, 0x480 + 0x2C, gate)
        put_f32(blob, 0x40 + 0x58, value)
        run(0x006978D0, blob, [], 0, self_off=0x520,
            fixups=["0xC", "0x8", "0x438"], compare="u32", float_ret=True)

    scale(1, 2.0)
    scale(0, 2.0)
    scale(1, 0.0)
    scale(1, -1.0)

    # 0x00692850 bitset. Covered after the float cases so a failure there is obvious.
    def bit_clear(enabled: int, index: int, word: int) -> None:
        blob = fill(0x100)
        put_u32(blob, 0x20, 0x40)
        blob[0x40 + 0x8D] = enabled
        put_u32(blob, 0x40 + 0x24 + 4 * (index >> 5), word)
        put_u32(blob, 0x70, index)
        run(0x00692850, blob, lambda b: [b + 0x70], 4, self_off=0x40, a0=0x70,
            fixups=["0x20"], compare="u32")

    bit_clear(0, 0, 0)
    bit_clear(1, 0, 1)
    bit_clear(1, 0, 0)
    bit_clear(1, 5, 0)
    bit_clear(1, 33, 2)

    missing = [va for va in ACCEPTED_ENTRIES if oracle.counts.get(va, 0) < 2]
    if missing:
        raise AssertionError("fewer than 2 cases: " + ", ".join(f"0x{va:08X}" for va in missing))
    cases = sum(oracle.counts.values())
    print(f"obj69 oracle: {len(ACCEPTED_ENTRIES)} entries, {cases} cases, 0 mismatches")
    oracle.close()


if __name__ == "__main__":
    exe = Path(sys.argv[1]) if len(sys.argv) > 1 else Path(
        "Ra3Source/build/win32/bin/Release/ra3_obj69check.exe")
    verify(exe)
