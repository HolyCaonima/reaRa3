"""Differentially verify the render8e W3D cluster against ra3_1.0.game."""

from __future__ import annotations

import argparse
import random
import struct
import subprocess
import sys
from pathlib import Path
from typing import Callable

from unicorn import x86_const

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from emulate import CallResult, Emulator  # noqa: E402


class CppOracle:
    def __init__(self, executable: Path) -> None:
        self.process = subprocess.Popen(
            [str(executable)], stdin=subprocess.PIPE, stdout=subprocess.PIPE,
            text=True, bufsize=1)

    def call(
        self, address: int, blob: bytes, fixups: list[int], arguments: list[int]
    ) -> tuple[int, bytes]:
        assert self.process.stdin is not None and self.process.stdout is not None
        fixup_text = ",".join(f"{offset:x}" for offset in fixups) or "."
        line = " ".join([
            f"{address:08x}", blob.hex(), fixup_text,
            *(f"{value & 0xFFFFFFFF:x}" for value in arguments),
        ])
        self.process.stdin.write(line + "\n")
        self.process.stdin.flush()
        output = self.process.stdout.readline().strip()
        if not output or output == "error":
            raise RuntimeError(f"render8echeck rejected 0x{address:08X}")
        result, encoded = output.split()
        return int(result, 16), bytes.fromhex(encoded)

    def set_control_word(self, value: int) -> None:
        assert self.process.stdin is not None and self.process.stdout is not None
        self.process.stdin.write(f"fpcw {value:04x}\n")
        self.process.stdin.flush()
        if self.process.stdout.readline().strip() != "ok":
            raise RuntimeError("render8echeck rejected FPU control word")

    def close(self) -> None:
        assert self.process.stdin is not None
        self.process.stdin.write("quit\n")
        self.process.stdin.flush()
        if self.process.wait(timeout=10) != 0:
            raise RuntimeError("render8echeck failed")


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


def float32_to_float80(bits: int) -> bytes:
    sign = bits >> 31
    exponent = bits >> 23 & 0xFF
    fraction = bits & 0x7FFFFF
    if exponent == 0:
        if fraction == 0:
            extended_exponent = 0
            mantissa = 0
        else:
            leading = fraction.bit_length() - 1
            extended_exponent = leading - 149 + 16383
            mantissa = fraction << (63 - leading)
    elif exponent == 0xFF:
        extended_exponent = 0x7FFF
        mantissa = 0x8000000000000000 | fraction << 40
    else:
        extended_exponent = exponent - 127 + 16383
        mantissa = ((1 << 23) | fraction) << 40
    return struct.pack("<QH", mantissa, extended_exponent | sign << 15)


Invoke = Callable[[Emulator, int], tuple[CallResult, int | None]]

ACCEPTED_ENTRIES = (
    0x008E4610, 0x008E4620, 0x008E4680, 0x008E46A0,
    0x008E4820, 0x008E4840, 0x008E4910, 0x008E4950,
    0x008E49E0, 0x008E4A40, 0x008E4AA0, 0x008E4AE0,
    0x008E4B30, 0x008E5550, 0x008E56B0, 0x008E5710,
    0x008E5740, 0x008E5B00, 0x008E5B80, 0x008E5F70,
    0x008E62E0, 0x008E6330, 0x008E6620, 0x008E6770,
    0x008E6800, 0x008E6DD0, 0x008E7000, 0x008E70F0,
    0x008E71E0, 0x008E87D0, 0x008E88A0, 0x008E89D0,
    0x008E8A00, 0x008E8AB0, 0x008E8B20, 0x008E8C10,
    0x008E92A0, 0x008E9350, 0x008E95F0, 0x008E96B0,
    0x008E98A0, 0x008E9980, 0x008E9BE0, 0x008EADD0,
    0x008EAE00, 0x008EAF70, 0x008EB160, 0x008EB2C0,
    0x008EB620, 0x008EB6D0, 0x008ED5B0, 0x008EE0B0,
    0x008EFE00,
)
RANDOM_SCENARIOS_PER_ENTRY = 64
X87_CONTROL_WORDS = (0x007F, 0x027F, 0x037F)


class Verifier:
    def __init__(self, executable: Path) -> None:
        self.emu = Emulator()
        self.cpp = CppOracle(executable)
        self.counts: dict[int, int] = {}
        self.control_word: int | None = None

    def select_control_word(self, control_word: int) -> None:
        if self.control_word == control_word:
            return
        self.cpp.set_control_word(control_word)
        self.emu.uc.reg_write(x86_const.UC_X86_REG_FPCW, control_word)
        self.control_word = control_word

    def case(
        self, address: int, blob: bytes, fixups: list[int], arguments: list[int],
        invoke: Invoke, cleanup: int, *, compare_result: bool = False,
        control_word: int = 0x037F
    ) -> None:
        self.select_control_word(control_word)
        cpp_result, cpp_blob = self.cpp.call(address, blob, fixups, arguments)
        self.emu.reset_data()
        base = self.emu.alloc(blob)
        for offset in fixups:
            relative = u32(blob, offset)
            self.emu.write(
                base + offset, struct.pack("<I", 0 if relative == 0 else base + relative))
        result, game_value = invoke(self.emu, base)
        if result.stack_cleanup != cleanup:
            raise AssertionError(
                f"0x{address:08X}: cleanup {result.stack_cleanup}, expected {cleanup}")
        game_blob = bytearray(self.emu.read(base, len(blob)))
        for offset in fixups:
            value = u32(game_blob, offset)
            if value == 0:
                relative = 0
            elif base <= value <= base + len(blob):
                relative = value - base
            else:
                relative = 0xFFFFFFFF
            put_u32(game_blob, offset, relative)
        if bytes(game_blob) != cpp_blob:
            for index, (game, ours) in enumerate(zip(game_blob, cpp_blob)):
                if game != ours:
                    start = max(0, index - 4)
                    end = min(len(game_blob), index + 8)
                    raise AssertionError(
                        f"0x{address:08X}: byte mismatch +0x{index:X}: "
                        f"game={game:02x} ours={ours:02x}; "
                        f"context game={game_blob[start:end].hex()} "
                        f"ours={cpp_blob[start:end].hex()}")
            raise AssertionError(f"0x{address:08X}: output size mismatch")
        if compare_result and game_value is not None and cpp_result != game_value & 0xFFFFFFFF:
            raise AssertionError(
                f"0x{address:08X}: result game={game_value & 0xFFFFFFFF:08x} "
                f"ours={cpp_result:08x}")
        self.counts[address] = self.counts.get(address, 0) + 1

    def float80_case(
        self, address: int, blob: bytes, fixups: list[int], arguments: list[int],
        stack: Callable[[int], list[int]], cleanup: int, control_word: int
    ) -> None:
        self.select_control_word(control_word)
        cpp_bits, cpp_blob = self.cpp.call(address, blob, fixups, arguments)
        self.emu.reset_data()
        base = self.emu.alloc(blob)
        for offset in fixups:
            relative = u32(blob, offset)
            self.emu.write(
                base + offset,
                struct.pack("<I", 0 if relative == 0 else base + relative))
        result, game80 = self.emu.call_float80(
            address, stack(base), ecx=base, fpu_control_word=control_word)
        if result.stack_cleanup != cleanup:
            raise AssertionError(
                f"0x{address:08X}/cw={control_word:04X}: "
                f"cleanup {result.stack_cleanup}, expected {cleanup}")
        game_blob = bytearray(self.emu.read(base, len(blob)))
        for offset in fixups:
            value = u32(game_blob, offset)
            if value == 0:
                relative = 0
            elif base <= value <= base + len(blob):
                relative = value - base
            else:
                relative = 0xFFFFFFFF
            put_u32(game_blob, offset, relative)
        if bytes(game_blob) != cpp_blob:
            raise AssertionError(
                f"0x{address:08X}/cw={control_word:04X}: caller blob mismatch")
        expected80 = float32_to_float80(cpp_bits)
        if game80 != expected80:
            raise AssertionError(
                f"0x{address:08X}/cw={control_word:04X}: "
                f"x87 game={game80.hex()} ours={expected80.hex()}")
        self.counts[address] = self.counts.get(address, 0) + 1

    def close(self) -> None:
        self.cpp.close()


def verify_deliberate_mutations(executable: Path) -> None:
    address = 0x008E4610
    value = 0xA17E5EED
    blob = bytes(0x110)
    cpp = CppOracle(executable)
    try:
        cpp.set_control_word(0x037F)
        _, candidate = cpp.call(address, blob, [], [0, value])
    finally:
        cpp.close()
    mutated = bytearray(candidate)
    mutated[0x104] ^= 1

    emu = Emulator()
    emu.uc.reg_write(x86_const.UC_X86_REG_FPCW, 0x037F)
    base = emu.alloc(blob)
    result = emu.call(address, [value], ecx=base)
    if result.stack_cleanup != 4:
        raise AssertionError(
            f"mutation probe cleanup {result.stack_cleanup}, expected 4")
    game = emu.read(base, len(blob))
    if game != candidate:
        raise AssertionError("setter mutation baseline does not match shipped behavior")
    if game == bytes(mutated):
        raise AssertionError("deliberate mutation was not detected")
    first = next(
        index for index, pair in enumerate(zip(game, mutated))
        if pair[0] != pair[1])
    if first != 0x104:
        raise AssertionError(
            f"deliberate mutation detected at unexpected +0x{first:X}")
    print("deliberate mutation: detected at 0x008E4610 +0x104")

    address = 0x008E5550
    blob = bytearray(4)
    put_f32(blob, 0, 3.1)
    target = fbits(-3.1)
    cpp = CppOracle(executable)
    try:
        cpp.set_control_word(0x037F)
        _, candidate = cpp.call(address, bytes(blob), [], [0, target])
    finally:
        cpp.close()
    emu = Emulator()
    emu.uc.reg_write(x86_const.UC_X86_REG_FPCW, 0x037F)
    base = emu.alloc(bytes(blob))
    result = emu.call(address, [base, target])
    if result.stack_cleanup != 0:
        raise AssertionError(
            f"angle mutation probe cleanup {result.stack_cleanup}, expected 0")
    game = emu.read(base, len(blob))
    if game != candidate:
        raise AssertionError("angle mutation baseline does not match shipped behavior")
    mutated = bytearray(candidate)
    # Semantic fault: retain the pre-wrap 3.2-radian result instead of subtracting 2*pi.
    put_f32(mutated, 0, 3.2)
    if game == bytes(mutated):
        raise AssertionError("wrapped-angle semantic mutation was not detected")
    print("deliberate mutation: detected omitted-wrap branch at 0x008E5550")


def verify(executable: Path) -> None:
    rng = random.Random(0x008E53D)
    verifier = Verifier(executable)
    emu = verifier.emu

    def random_blob(size: int) -> bytearray:
        return bytearray(rng.randbytes(size))

    current_control_word = 0x037F

    def call(
        address: int, blob: bytearray, fixups: list[int], args: list[int],
        stack: Callable[[int], list[int]], *, ecx: int | None = None,
        edx: int = 0, edx_offset: int | None = None, eax: int = 0, cleanup: int = 0,
        result: Callable[[CallResult, int], int | None] | None = None
    ) -> None:
        def invoke(machine: Emulator, base: int) -> tuple[CallResult, int | None]:
            outcome = machine.call(
                address, stack(base), ecx=base + ecx if ecx is not None else 0,
                edx=base + edx_offset if edx_offset is not None else edx,
                eax=base + eax if eax else 0)
            value = result(outcome, base) if result is not None else None
            return outcome, value

        verifier.case(
            address, bytes(blob), fixups, args, invoke, cleanup,
            compare_result=result is not None,
            control_word=current_control_word)

    # Drawable state and pass masks: exact writes, cleanup, and overlapping copies.
    for address, offset in ((0x008E4610, 0x104), (0x008E4620, 0x108)):
        blob = random_blob(0x800)
        value = rng.randrange(2**32)
        call(address, blob, [], [0, value], lambda _: [value], ecx=0, cleanup=4)
        assert offset < len(blob)

    blob = random_blob(0x800)
    values = [rng.randrange(2**32), rng.randrange(2**32)]
    call(0x008E4680, blob, [], [0, *values], lambda _: values, ecx=0, cleanup=8)

    for mode in (0, 3):
        blob = random_blob(0x800)
        put_u32(blob, 0x76C, mode)
        values = [fbits(value) for value in (1.25, -2.5, 12.0, 19.0)]
        call(0x008E46A0, blob, [], [0, *values], lambda _: values, ecx=0, cleanup=16)

    for address in (0x008E4820, 0x008E4910):
        call(address, random_blob(0x800), [], [0], lambda _: [], ecx=0)

    blob = random_blob(0x800)
    values = [321, 3, 654, 2]
    call(0x008E4840, blob, [], [0, *values], lambda _: values, ecx=0, cleanup=16)

    blob = random_blob(0xC0)
    call(0x008E4950, blob, [], [0x40, 0], lambda base: [base], ecx=0x40, cleanup=4)
    blob = random_blob(0x80)
    call(0x008E4950, blob, [], [0, 0], lambda base: [base], ecx=0, cleanup=4)
    blob = random_blob(0xC0)
    for offset in range(0, 0x44, 4):
        put_u32(blob, offset, 0x7FC12345)
    put_u32(blob, 4, 1)
    call(0x008E4950, blob, [], [0x60, 0], lambda base: [base], ecx=0x60, cleanup=4)

    for address in (0x008E49E0, 0x008E4A40):
        blob = random_blob(16)
        if address == 0x008E49E0:
            args = [0, 4, fbits(0.25), fbits(-0.5), 1920, 1080]
            stack = lambda base, a=args: [a[2], a[3], base, base + 4, a[4], a[5]]
        else:
            args = [0, 4, 120, 240, 1920, 1080]
            stack = lambda base, a=args: [a[2], a[3], base, base + 4, a[4], a[5]]
        call(address, blob, [], args, stack)
    for x_bits, y_bits in (
        (0x80000000, 0x00000000),
        (0x7F800000, 0xFF800000),
        (0x7FC12345, fbits(0.75)),
    ):
        blob = random_blob(8)
        args = [0, 4, x_bits, y_bits, 1920, 1080]
        call(
            0x008E49E0, blob, [], args,
            lambda base, a=args: [a[2], a[3], base, base + 4, a[4], a[5]])

    for address in (0x008E4AA0, 0x008E4AE0):
        blob = random_blob(64)
        put_u32(blob, 0, 3)
        put_u32(blob, 32, 3)
        call(
            address, blob, [], [0, 32], lambda base: [base + 32], ecx=0,
            cleanup=4, result=(lambda outcome, _: outcome.eax & 0xFF)
            if address == 0x008E4AE0 else None)
    blob = random_blob(32)
    put_u32(blob, 0, 3)
    call(
        0x008E4AA0, blob, [], [0, 0], lambda base: [base],
        ecx=0, cleanup=4)

    blob = random_blob(0x120)
    source = 0
    put_u32(blob, source, 2)
    call(0x008E4B30, blob, [], [0, source], lambda base: [base], ecx=0, cleanup=4)
    blob = random_blob(0x120)
    put_u32(blob, 0xD8, 2)
    call(
        0x008E4B30, blob, [], [0, 0xD8],
        lambda base: [base + 0xD8], ecx=0, cleanup=4)

    blob = random_blob(0x400)
    put_u32(blob, 0x74, 0x300)
    call(0x008E56B0, blob, [0x74], [0, 1], lambda _: [1], ecx=0, cleanup=4)

    for address, cache_offset, table_offset in (
        (0x008E5710, 0x254, 0x50), (0x008E5740, 0x258, 0x54)
    ):
        blob = random_blob(0x500)
        put_u32(blob, 0x3C, 0x300)
        put_u32(blob, 0x300, 0x400)
        put_u32(blob, 0x400 + table_offset, 0x12345678 + table_offset)
        if address == 0x008E5710:
            blob[0x251] = 1
        call(
            address, blob, [0x3C, 0x300], [0], lambda _: [], ecx=0,
            result=lambda outcome, _: outcome.eax)
        assert cache_offset < len(blob)

    # Material, matrix, copy, tree, and interpolation helpers.
    for value, target in ((3.0, -3.0), (-2.0, 1.5), (0.25, 0.3)):
        blob = bytearray(4)
        put_f32(blob, 0, value)
        call(
            0x008E5550, blob, [], [0, fbits(target)],
            lambda base, t=target: [base, fbits(t)])

    for address in (0x008E5B00, 0x008E5B80):
        left, right = rng.randrange(2**32), rng.randrange(2**32)
        if address == 0x008E5B00:
            call(
                address, bytearray(1), [], [left, right], lambda _: [right],
                edx=left, result=lambda outcome, _: outcome.eax)
        else:
            call(
                address, bytearray(1), [], [left, right], lambda _: [left, right],
                result=lambda outcome, _: outcome.eax)

    for address, words in (
        (0x008E5F70, 18), (0x008E88A0, 13), (0x008E8AB0, 15),
        (0x008EAF70, 50), (0x008EB2C0, 17),
    ):
        blob = random_blob(words * 8 + 16)
        call(
            address, blob, [], [words * 4, 0], lambda base: [base],
            ecx=words * 4, cleanup=4)
        blob = random_blob(words * 4 + 8)
        call(
            address, blob, [], [4, 0], lambda base: [base],
            ecx=4, cleanup=4)
        special = (
            0x00000000, 0x80000000, 0x7F800000, 0xFF800000,
            0x7FC12345, 0x00000001, 0x807FFFFF, 0x3F800000,
        )
        blob = random_blob(words * 8 + 16)
        for index in range(words):
            put_u32(blob, index * 4, special[index % len(special)])
        call(
            address, blob, [], [words * 4, 0], lambda base: [base],
            ecx=words * 4, cleanup=4)

    for address, words, raw_offsets in (
        (0x008E88A0, 13, (0x30,)),
        (0x008E8AB0, 15, (0, 4, 8)),
        (0x008EAF70, 50, (0x60, 0x64)),
        (0x008EB2C0, 17, (4, 8, 0xC, 0x10)),
    ):
        destination = words * 4 + 8
        blob = random_blob(destination + words * 4 + 8)
        for offset in range(0, words * 4, 4):
            put_u32(blob, offset, 0x7FC12345)
        for offset in raw_offsets:
            put_u32(blob, offset, 0x7F812345)
        call(
            address, blob, [], [destination, 0], lambda base: [base],
            ecx=destination, cleanup=4)

    blob = random_blob(0x90)
    call(
        0x008E62E0, blob, [], [0, 48, 0x90],
        lambda base: [base, base + 48, base + 0x90])
    blob = random_blob(0x78)
    call(
        0x008E62E0, blob, [], [0, 48, 72],
        lambda base: [base, base + 48, base + 72])

    blob = random_blob(96)
    blob[48:96] = blob[:48]
    call(
        0x008E6330, blob, [], [0, 48], lambda base: [base, base + 48],
        result=lambda outcome, _: outcome.eax & 0xFF)

    blob = random_blob(80)
    for index, value in enumerate((0.2, -0.3, 0.4, 0.5, 10.0, 20.0, 30.0)):
        put_f32(blob, index * 4, value)
    call(0x008E6620, blob, [], [0, 32], lambda base: [base + 32], ecx=0, cleanup=4)

    blob = random_blob(0xC0)
    call(0x008E6770, blob, [], [0x60, 0], lambda base: [base], ecx=0x60, cleanup=4)
    blob = random_blob(0x60)
    call(0x008E6770, blob, [], [4, 0], lambda base: [base], ecx=4, cleanup=4)
    blob = random_blob(0xC0)
    put_u32(blob, 0x3C, 0x7FC12345)
    put_u32(blob, 0x40, 0x7FC12345)
    put_u32(blob, 0x44, 0x7F812345)
    call(0x008E6770, blob, [], [0x60, 0], lambda base: [base], ecx=0x60, cleanup=4)
    call(0x008E6800, random_blob(0x60), [], [0], lambda _: [], ecx=0)

    for address in (0x008E87D0, 0x008ED5B0):
        value = 0x120
        if address == 0x008E87D0:
            blob = bytearray(8)
            put_u32(blob, 0, value)
            put_u32(blob, 4, value)
            call(
                address, blob, [], [value, value], lambda base: [base, base + 4],
                result=lambda outcome, _: outcome.eax & 0xFF)
        else:
            blob = bytearray(4)
            put_u32(blob, 0, value)
            call(
                address, blob, [], [value], lambda base: [base],
                result=lambda outcome, _: outcome.eax & 0xFF)

    # Tree nodes use relative pointers; output slots are normalized by both sides.
    for address in (0x008EADD0, 0x008EAE00):
        blob = bytearray(0xA0)
        put_u32(blob, 0x0C, 0x40)
        put_u32(blob, 0x40, 0x60)
        put_u32(blob, 0x44, 0x80)
        put_i32(blob, 0x50, 20)
        put_i32(blob, 0x70, 10)
        put_i32(blob, 0x90, 30)
        put_i32(blob, 0x24, 22)
        fixups = [0x0C, 0x20, 0x40, 0x44, 0x60, 0x64, 0x80, 0x84]

        def tree_invoke(machine: Emulator, base: int, target: int = address) -> tuple[CallResult, int]:
            result = machine.call(target, [base + 0x20, base + 0x24], ecx=base)
            return result, u32(machine.read(base + 0x20, 4), 0) - base

        verifier.case(
            address, bytes(blob), fixups, [0, 0x20, 0x24], tree_invoke, 8,
            compare_result=True)

    blob = random_blob(64)
    for index, value in enumerate((0.2, 0.3, 0.4, 0.5, -0.6, 0.2, 0.1, 0.7)):
        put_f32(blob, index * 4, value)
    call(
        0x008EB160, blob, [], [32, 0, 16, fbits(0.25)],
        lambda base: [base + 32, base, base + 16, fbits(0.25)])

    for address in (0x008EB620, 0x008EB6D0):
        blob = random_blob(0x160)
        for record in (0, 0x44):
            for offset in range(4, 0x44, 4):
                put_u32(blob, record + offset, 0x7FC12345)
            for offset in (4, 8, 0xC, 0x10):
                put_u32(blob, record + offset, 0x7F812345)
        if address == 0x008EB620:
            args = [0, 0x88, 0x88]
            call(
                address, blob, [], args,
                lambda base: [base, base + 0x88, base + 0x88],
                result=lambda outcome, base: outcome.eax - base)
        else:
            args = [0, 0x88, 0x110]
            call(
                address, blob, [], args,
                lambda base: [base, base + 0x88, base + 0x110],
                result=lambda outcome, base: outcome.eax - base)

    blob = random_blob(0x240)
    for index, value in enumerate((1.0, 2.0, 3.0)):
        put_f32(blob, index * 4, value)
        put_f32(blob, 0x20 + index * 4, value + 4.0)
        put_f32(blob, 0x40 + index * 4, value * 0.25)
    put_f32(blob, 0x100 + 0x64, 0.3)
    put_f32(blob, 0x100 + 0x68, -0.2)
    put_u32(blob, 0x100 + 0x98, 0)
    call(
        0x008EE0B0, blob, [], [0x80, 0, 0x100, 0x20, 0x40],
        lambda base: [base + 0x80, base, base + 0x100, base + 0x20, base + 0x40])

    for first, second, width in (
        ((1.0, 2.0, 3.0), (11.0, 12.0, 13.0), 2.0),
        ((-7.25, 0.125, 20.0), (3.5, -9.0, 2.75), 0.25),
        ((4.0, 5.0, 6.0), (4.0, 5.0, 6.0), 4.0),
    ):
        blob = random_blob(0x200)
        put_u32(blob, 0, 0x100)
        for index, value in enumerate(first):
            put_f32(blob, 0x10 + index * 4, value)
        for index, value in enumerate(second):
            put_f32(blob, 0x20 + index * 4, value)
        call(
            0x008EFE00, blob, [0],
            [0, 0x10, 0x20, fbits(width), 0xAABBCCDD],
            lambda base, w=width: [
                base + 0x10, base + 0x20, fbits(w), 0xAABBCCDD],
            edx_offset=0, cleanup=16,
            result=lambda outcome, base: u32(emu.read(base, 4), 0) - base)

    # Terrain helpers share one fully relocated height map.
    terrain = bytearray(0x17600)
    put_u32(terrain, 0x14, 0x100)
    put_i32(terrain, 0x124, 4)
    put_i32(terrain, 0x128, 4)
    put_i32(terrain, 0x12C, 1)
    put_u32(terrain, 0x144, 0x17300)
    put_i32(terrain, 0x100 + 0x17148, 2)
    put_i32(terrain, 0x100 + 0x1714C, -1)
    for index in range(16):
        struct.pack_into("<H", terrain, 0x17300 + index * 2, index * 25)
    for index, value in enumerate((0.0, 0.0, 4.0, 7.0)):
        put_f32(terrain, 0x20 + index * 4, value)
    put_f32(terrain, 0x2C, -5.0)
    put_f32(terrain, 0x30, 15.0)
    terrain_fixups = [0x14, 0x144]

    def terrain_float(address: int, stack_builder: Callable[[int], list[int]], args: list[int]) -> None:
        def invoke(machine: Emulator, base: int) -> tuple[CallResult, int]:
            result, bits = machine.call_float32(address, stack_builder(base), ecx=base)
            return result, bits

        verifier.case(
            address, bytes(terrain), terrain_fixups, args, invoke,
            8, compare_result=True)
        cpp_bits, _ = verifier.cpp.call(address, bytes(terrain), terrain_fixups, args)
        expected80 = float32_to_float80(cpp_bits)
        for control_word in (0x007F, 0x027F, 0x037F):
            verifier.emu.reset_data()
            base = verifier.emu.alloc(bytes(terrain))
            for offset in terrain_fixups:
                relative = u32(terrain, offset)
                verifier.emu.write(
                    base + offset,
                    struct.pack("<I", 0 if relative == 0 else base + relative))
            result, game80 = verifier.emu.call_float80(
                address, stack_builder(base), ecx=base,
                fpu_control_word=control_word)
            if result.stack_cleanup != 8:
                raise AssertionError(
                    f"0x{address:08X}/cw={control_word:04X}: "
                    f"cleanup {result.stack_cleanup}, expected 8")
            if game80 != expected80:
                raise AssertionError(
                    f"0x{address:08X}/cw={control_word:04X}: "
                    f"x87 game={game80.hex()} ours={expected80.hex()}")
            verifier.counts[address] += 1

    terrain_float(
        0x008E6DD0, lambda base: [base + 0x20, base + 0x28], [0, 0x20, 0x28])
    terrain_float(
        0x008E7000, lambda _: [fbits(0.0), fbits(0.0)],
        [0, fbits(0.0), fbits(0.0)])
    for address, output_size in ((0x008E70F0, 16), (0x008E71E0, 24)):
        blob = terrain + bytearray(output_size)
        output = len(terrain)
        call(
            address, blob, terrain_fixups, [0, output],
            lambda base, out=output: [base + out], ecx=0, cleanup=4)

    # Scripted-model condition selection and animation math.
    blob = bytearray(0x200)
    put_u32(blob, 0, 0x100)
    put_u32(blob, 0x100 + 0x0C, 1)
    put_u32(blob, 0x100 + 0x58, 0x10)
    call(
        0x008E89D0, blob, [0], [0], lambda _: [], ecx=0,
        result=lambda outcome, _: outcome.eax & 0xFF)

    blob = bytearray(0x400)
    put_u32(blob, 8, 0x100)
    put_u32(blob, 0xC, 0x124)
    fixups = [8, 0xC]
    for index, weight in enumerate((2, 4, 3)):
        entry = 0x100 + index * 12
        model = 0x200 + index * 0x40
        put_u32(blob, entry, model)
        put_i32(blob, model + 0x34, weight)
        fixups.append(entry)

    def weighted(machine: Emulator, base: int) -> tuple[CallResult, int]:
        machine.bind_function(0x005A94F0, lambda _: 3)
        result = machine.call(0x008E8A00, [1, 3], ecx=base)
        return result, result.eax

    verifier.case(
        0x008E8A00, bytes(blob), fixups, [0, 1, 3, 3], weighted, 8,
        compare_result=True)

    for address, begin_field, end_field, stride, fallback_field in (
        (0x008E8B20, 0x260, 0x264, 0x74, 0x210),
        (0x008E8C10, 0x2C, 0x30, 0x88, 0x20C),
    ):
        blob = bytearray(0x600)
        begin = 0x300
        put_u32(blob, begin_field, begin)
        put_u32(blob, end_field, begin + stride * 2)
        conditions = 0x280
        put_u32(blob, conditions, 1)
        fixups = [begin_field, end_field, fallback_field]
        for index in range(2):
            model = 0x500 + index * 0x50
            put_u32(blob, begin + index * stride, model)
            put_u32(blob, model + 0xC, index)
            fixups.append(begin + index * stride)
        call(
            address, blob, fixups, [0, conditions],
            lambda base, cond=conditions: [base + cond], ecx=0, cleanup=4,
            result=lambda outcome, base: outcome.eax - base if outcome.eax else 0)

    blob = bytearray(0x200)
    blob[0x70] = 1
    blob[0x100 + 0x70] = 1
    put_u32(blob, 0x60, 0x80)
    put_u32(blob, 0x64, 0x9C)
    put_u32(blob, 0x100 + 0x60, 0x180)
    put_u32(blob, 0x100 + 0x64, 0x19C)
    blob[0x180:0x19C] = blob[0x80:0x9C]
    call(
        0x008E92A0, blob, [0x60, 0x64, 0x160, 0x164], [0, 0x100],
        lambda _: [], ecx=0, eax=0x100,
        result=lambda outcome, _: outcome.eax & 0xFF)

    blob = bytearray(0x400)
    put_u32(blob, 0, 0x300)
    put_u32(blob, 8, 0x200)
    blob[0x300 + 0xCA] = 0
    blob[0x300 + 0xCB] = 1
    call(0x008E9350, blob, [0, 8], [0x10, 1], lambda _: [1], ecx=0x10, cleanup=4)

    blob = bytearray(0x200)
    put_u32(blob, 0x108, 0x180)
    put_i32(blob, 0x180 + 0x14, 20)
    put_i32(blob, 0x118, 3)
    call(
        0x008E95F0, blob, [0x108], [0, fbits(0.25)],
        lambda _: [fbits(0.25)], ecx=0, cleanup=4)

    blob = random_blob(0x160)
    put_u32(blob, 0x11C, 1)
    put_u32(blob, 0x138, 2)
    call(
        0x008E96B0, blob, [], [0], lambda _: [], ecx=0,
        result=lambda outcome, _: outcome.eax & 0xFF)

    blob = bytearray(18 * 4)
    for index in range(9):
        put_f32(blob, index * 4, rng.uniform(-20.0, 20.0))
    call(0x008E98A0, blob, [], [0], lambda _: [], ecx=0)

    blob += bytearray(12)
    call(
        0x008E9980, blob, [], [0, 72, fbits(0.73)],
        lambda base: [base + 72, fbits(0.73)], ecx=0, cleanup=8)

    blob = bytearray(0x180)
    put_u32(blob, 4, 0x40)
    put_u32(blob, 8, 0xE0)
    put_f32(blob, 0x40 + 0x48, 2.0)
    put_f32(blob, 0x40 + 0x4C, 3.0)
    put_f32(blob, 0x90 + 0x48, 4.0)
    put_f32(blob, 0x90 + 0x4C, 5.0)
    call(
        0x008E9BE0, blob, [4, 8], [0, fbits(0.5)],
        lambda _: [fbits(0.5)], ecx=0, cleanup=4)

    # Deterministic randomized phase. Every accepted entry receives 64 distinct
    # scenarios in addition to the focused fixtures above.
    finite_edges = (
        0x00000000, 0x80000000, 0x00000001, 0x80000001,
        0x00800000, 0x80800000, 0x3F800000, 0xBF800000,
        0x41200000, 0xC1200000, 0x7F7FFFFF, 0xFF7FFFFF,
    )
    copy_edges = finite_edges + (
        0x7F800000, 0xFF800000, 0x7FC12345, 0xFFC12345,
    )

    def finite_bits(index: int, scale: float = 100.0) -> int:
        if index < len(finite_edges):
            return finite_edges[index]
        return fbits(rng.uniform(-scale, scale))

    def fill_copy_source(blob: bytearray, source: int, size: int, scenario: int) -> None:
        for offset in range(0, size & ~3, 4):
            put_u32(
                blob, source + offset,
                copy_edges[(scenario + offset // 4) % len(copy_edges)])
        for offset in range(size & ~3, size):
            blob[source + offset] = (scenario * 37 + offset) & 0xFF

    def make_terrain(scenario: int) -> tuple[bytearray, list[int], int, int, int]:
        blob = bytearray(0x17600)
        width = 2 + scenario % 7
        height = 2 + scenario * 3 % 7
        border = scenario % 4
        data = 0x100
        samples = 0x17300
        null_data = scenario % 16 == 0
        put_u32(blob, 0x14, 0 if null_data else data)
        put_i32(blob, data + 0x24, width)
        put_i32(blob, data + 0x28, height)
        put_i32(blob, data + 0x2C, border)
        put_u32(blob, data + 0x44, 0 if null_data else samples)
        put_i32(blob, data + 0x17148, scenario % 7 - 3)
        put_i32(blob, data + 0x1714C, 3 - scenario % 7)
        for index in range(width * height):
            value = (scenario * 997 + index * 811 + index * index * 13) & 0xFFFF
            struct.pack_into("<H", blob, samples + index * 2, value)
        return blob, [0x14, data + 0x44], width, height, border

    def tree_case(address: int, scenario: int) -> None:
        blob = random_blob(0x300)
        count = scenario % 8
        keys = sorted({((scenario * 29 + index * 37) % 101) - 50
                       for index in range(count)})
        nodes = [0x80 + index * 0x20 for index in range(len(keys))]
        fixups = [0x0C, 0x20]

        def construct(first: int, last: int) -> int:
            if first == last:
                return 0
            middle = (first + last) // 2
            node = nodes[middle]
            right = construct(middle + 1, last)
            left = construct(first, middle)
            put_u32(blob, node, right)
            put_u32(blob, node + 4, left)
            put_i32(blob, node + 0x10, keys[middle])
            fixups.extend((node, node + 4))
            return node

        put_u32(blob, 0x0C, construct(0, len(keys)))
        put_u32(blob, 0x20, 0)
        probes = (-51, -50, -1, 0, 1, 50, 51)
        key = probes[scenario % len(probes)] if scenario < len(probes) else (
            (scenario * 17) % 121 - 60)
        put_i32(blob, 0x24, key)

        def invoke(
            machine: Emulator, base: int, target: int = address
        ) -> tuple[CallResult, int]:
            outcome = machine.call(target, [base + 0x20, base + 0x24], ecx=base)
            return outcome, u32(machine.read(base + 0x20, 4), 0) - base

        verifier.case(
            address, bytes(blob), fixups, [0, 0x20, 0x24],
            invoke, 8, compare_result=True)

    for scenario in range(RANDOM_SCENARIOS_PER_ENTRY):
        current_control_word = X87_CONTROL_WORDS[scenario % len(X87_CONTROL_WORDS)]
        value = (scenario * 0x9E3779B9 + rng.randrange(2**32)) & 0xFFFFFFFF
        for address in (0x008E4610, 0x008E4620):
            call(
                address, random_blob(0x110), [], [0, value],
                lambda _, v=value: [v], ecx=0, cleanup=4)

        values = [
            (value + 0x11111111) & 0xFFFFFFFF,
            (value ^ 0xA5A5A5A5) & 0xFFFFFFFF,
        ]
        call(
            0x008E4680, random_blob(0x784), [], [0, *values],
            lambda _, a=values: a, ecx=0, cleanup=8)

        blob = random_blob(0x77B)
        put_u32(blob, 0x76C, (0, 3, 3, 2)[scenario % 4])
        viewport = [
            finite_bits((scenario + index) % len(finite_edges), 1000.0)
            for index in range(4)
        ]
        if scenario % 8 == 2:
            viewport[2] = fbits(100000.0)
        call(
            0x008E46A0, blob, [], [0, *viewport],
            lambda _, a=viewport: a, ecx=0, cleanup=16)

        for address in (0x008E4820, 0x008E4910):
            call(address, random_blob(0x77B), [], [0], lambda _: [], ecx=0)

        modes = (-2, 0, 1, 2, 3, 4)
        dimensions = (0, 1, -1, 32767, -32768, 0x1000000)
        mode_values = [
            dimensions[scenario % len(dimensions)],
            modes[scenario % len(modes)],
            dimensions[(scenario + 3) % len(dimensions)],
            modes[(scenario * 3) % len(modes)],
        ]
        call(
            0x008E4840, random_blob(0x77B), [], [0, *mode_values],
            lambda _, a=mode_values: a, ecx=0, cleanup=16)

        blob = random_blob(0x180)
        source = 0x60
        destination = (source, source + 4, source - 4, 0x100)[scenario % 4]
        fill_copy_source(blob, source, 0x44, scenario)
        put_u32(blob, source + 4, (0, 1, 0x80000000)[scenario % 3])
        call(
            0x008E4950, blob, [], [destination, source],
            lambda base, src=source: [base + src],
            ecx=destination, cleanup=4)

        xy = (
            (0x00000000, 0x80000000),
            (0x7F800000, 0xFF800000),
            (0x7FC12345, 0x00000001),
        )
        x_bits, y_bits = xy[scenario % len(xy)] if scenario < 12 else (
            finite_bits(scenario, 4.0), finite_bits(scenario + 5, 4.0))
        width = (1, -1, 2, 1920, 0x1000000)[scenario % 5]
        height = (1, -1, 3, 1080, 0x1000000)[scenario * 3 % 5]
        blob = random_blob(8)
        args = [0, 4, x_bits, y_bits, width, height]
        call(
            0x008E49E0, blob, [], args,
            lambda base, a=args: [a[2], a[3], base, base + 4, a[4], a[5]])

        pixels = (
            scenario * 7919 - 200000, 100000 - scenario * 3571)
        blob = random_blob(8)
        args = [0, 4, pixels[0], pixels[1], width or 1, height or 1]
        call(
            0x008E4A40, blob, [], args,
            lambda base, a=args: [a[2], a[3], base, base + 4, a[4], a[5]])

        blob = random_blob(0x80)
        source = 0x30
        destination = (source, source + 4, source - 4, 0)[scenario % 4]
        for offset in (source, destination):
            put_u32(blob, offset, rng.randrange(2**32) & ~7 | scenario % 8)
            for word in range(3):
                put_u32(blob, offset + 4 + word * 4, rng.randrange(2**32))
        call(
            0x008E4AA0, blob, [], [destination, source],
            lambda base, src=source: [base + src],
            ecx=destination, cleanup=4)

        blob = random_blob(0x40)
        left_count = scenario % 8
        right_count = left_count if scenario % 3 else (left_count + 1) % 8
        put_u32(blob, 0, rng.randrange(2**32) & ~7 | left_count)
        put_u32(blob, 0x20, rng.randrange(2**32) & ~7 | right_count)
        for word in range(3):
            left_word = rng.randrange(2**32)
            right_word = left_word if scenario % 4 else left_word ^ (1 << word)
            put_u32(blob, 4 + word * 4, left_word)
            put_u32(blob, 0x24 + word * 4, right_word)
        call(
            0x008E4AE0, blob, [], [0, 0x20],
            lambda base: [base + 0x20], ecx=0, cleanup=4,
            result=lambda outcome, _: outcome.eax & 0xFF)

        blob = random_blob(0x180)
        source = (0xD8, 0xD4, 0xE0, 0x140)[scenario % 4]
        put_u32(blob, 0xD8, rng.randrange(2**32) & ~7 | scenario % 8)
        put_u32(blob, source, rng.randrange(2**32) & ~7 | (scenario + 3) % 8)
        for word in range(3):
            put_u32(blob, source + 4 + word * 4, rng.randrange(2**32))
        call(
            0x008E4B30, blob, [], [0, source],
            lambda base, src=source: [base + src], ecx=0, cleanup=4)

        angle_values = (
            -3.2415927, -3.1415927, -3.0415926, -0.1, -0.0,
            0.0, 0.1, 3.0415926, 3.1415927, 3.2415927,
        )
        value_float = angle_values[scenario % len(angle_values)]
        target_float = angle_values[(scenario * 7 + 3) % len(angle_values)]
        blob = bytearray(4)
        put_f32(blob, 0, value_float)
        call(
            0x008E5550, blob, [], [0, fbits(target_float)],
            lambda base, target=target_float: [base, fbits(target)])

        blob = random_blob(0x180)
        animation = 0 if scenario % 5 == 0 else 0x100
        put_u32(blob, 0x74, animation)
        blob[0x6B] = scenario // 2 & 1
        visible = (0, 1, 2, 0xFF)[scenario % 4]
        call(
            0x008E56B0, blob, [0x74], [0, visible],
            lambda _, v=visible: [v], ecx=0, cleanup=4)

        for address, table_offset in (
            (0x008E5710, 0x50), (0x008E5740, 0x54)
        ):
            blob = random_blob(0x300)
            present = scenario % 4 != 0
            put_u32(blob, 0x3C, 0x100 if present else 0)
            put_u32(blob, 0x100, 0x180 if present else 0)
            put_u32(blob, 0x180 + table_offset, value ^ table_offset)
            blob[0x251] = scenario & 1
            call(
                address, blob, [0x3C, 0x100], [0], lambda _: [], ecx=0,
                result=lambda outcome, _: outcome.eax)

        left = rng.randrange(2**32)
        right = rng.randrange(2**32)
        call(
            0x008E5B00, bytearray(1), [], [left, right],
            lambda _, r=right: [r], edx=left,
            result=lambda outcome, _: outcome.eax)
        call(
            0x008E5B80, bytearray(1), [], [left, right],
            lambda _, l=left, r=right: [l, r],
            result=lambda outcome, _: outcome.eax)

        for address, size in (
            (0x008E5F70, 0x48), (0x008E88A0, 0x34),
            (0x008E8AB0, 0x3C), (0x008EAF70, 0xC9),
            (0x008EB2C0, 0x44),
        ):
            blob = random_blob(0x400)
            source = 0x100
            destination = (source, source + 4, source - 4, 0x200)[scenario % 4]
            fill_copy_source(blob, source, size, scenario)
            call(
                address, blob, [], [destination, source],
                lambda base, src=source: [base + src],
                ecx=destination, cleanup=4)

        blob = random_blob(0x400)
        count = scenario % 5
        first = 0x100
        last = first + count * 24
        destination_last = (
            last, last + 24, max(0x80, last - 24), 0x300 + count * 24
        )[scenario % 4]
        call(
            0x008E62E0, blob, [], [first, last, destination_last],
            lambda base, a=first, b=last, d=destination_last:
            [base + a, base + b, base + d])

        blob = random_blob(0xC0)
        left_offset = 0x20
        right_offset = 0x70
        for word in range(12):
            bits = finite_bits(scenario + word, 1000.0)
            put_u32(blob, left_offset + word * 4, bits)
            put_u32(blob, right_offset + word * 4, bits)
        if scenario % 3 == 0:
            put_u32(
                blob, right_offset + (scenario % 12) * 4,
                finite_bits(scenario + 17, 1000.0) ^ 0x00800000)
        if scenario % 11 == 0:
            put_u32(blob, left_offset, 0x7FC12345)
            put_u32(blob, right_offset, 0x7FC12345)
        call(
            0x008E6330, blob, [], [left_offset, right_offset],
            lambda base, left=left_offset, right=right_offset:
            [base + left, base + right],
            result=lambda outcome, _: outcome.eax & 0xFF)

        blob = random_blob(0x100)
        quaternion = 0x20
        matrix = 0x70
        for word in range(7):
            put_u32(blob, quaternion + word * 4, finite_bits(scenario + word, 4.0))
        call(
            0x008E6620, blob, [], [quaternion, matrix],
            lambda base, out=matrix: [base + out],
            ecx=quaternion, cleanup=4)

        blob = random_blob(0x200)
        source = 0x80
        destination = (source, 0x120, source, 0x160)[scenario % 4]
        fill_copy_source(blob, source, 0x50, scenario)
        call(
            0x008E6770, blob, [], [destination, source],
            lambda base, src=source: [base + src],
            ecx=destination, cleanup=4)

        call(0x008E6800, random_blob(0x60), [], [0], lambda _: [], ecx=0)

        line_blob, terrain_fixups, width_cells, height_cells, border = make_terrain(scenario)
        endpoint_patterns = (
            (0, 0, 0, 0),
            (0, 0, width_cells - 2, 0),
            (0, 0, 0, height_cells - 2),
            (-1, 0, width_cells, height_cells),
        )
        x0, y0, x1, y1 = endpoint_patterns[scenario % 4]
        for offset, coordinate in (
            (0x20, x0), (0x24, y0), (0x28, x1), (0x2C, y1)
        ):
            put_f32(line_blob, offset, float(coordinate - border) * 10.0)
        verifier.float80_case(
            0x008E6DD0, bytes(line_blob), terrain_fixups,
            [0, 0x20, 0x28],
            lambda base: [base + 0x20, base + 0x28], 8,
            X87_CONTROL_WORDS[scenario % len(X87_CONTROL_WORDS)])

        cell_blob, terrain_fixups, width_cells, height_cells, border = make_terrain(scenario)
        coordinates = (
            -2, -1, 0, 1, width_cells - 2, width_cells - 1,
            width_cells, width_cells + 2,
        )
        cell_x = coordinates[scenario % len(coordinates)]
        cell_y = coordinates[(scenario * 3 + 1) % len(coordinates)]
        x_value = fbits(float(cell_x - border) * 10.0)
        y_value = fbits(float(cell_y - border) * 10.0)
        verifier.float80_case(
            0x008E7000, bytes(cell_blob), terrain_fixups,
            [0, x_value, y_value], lambda _: [x_value, y_value], 8,
            X87_CONTROL_WORDS[(scenario + 1) % len(X87_CONTROL_WORDS)])

        for address, output_size in ((0x008E70F0, 16), (0x008E71E0, 24)):
            terrain_blob, terrain_fixups, _, _, _ = make_terrain(scenario)
            put_f32(terrain_blob, 0x2C, float(scenario - 32))
            put_f32(terrain_blob, 0x30, float(63 - scenario))
            output = len(terrain_blob)
            terrain_blob += random_blob(output_size)
            call(
                address, terrain_blob, terrain_fixups, [0, output],
                lambda base, out=output: [base + out], ecx=0, cleanup=4)

        flags_blob = bytearray(8)
        left_flags = rng.randrange(2**32)
        right_flags = rng.randrange(2**32)
        put_u32(flags_blob, 0, left_flags)
        put_u32(flags_blob, 4, right_flags)
        call(
            0x008E87D0, flags_blob, [], [left_flags, right_flags],
            lambda base: [base, base + 4],
            result=lambda outcome, _: outcome.eax & 0xFF)

        blob = bytearray(0x180)
        model = 0x80
        put_u32(blob, 0, model)
        slot = scenario % 16
        if slot < 15:
            put_u32(blob, model + 0xC + slot * 4, 1 << (scenario % 31))
        blob[model + 0x88] = scenario // 3 & 1
        put_u32(blob, model + 0x58, (scenario & 1) << 4)
        call(
            0x008E89D0, blob, [0], [0], lambda _: [], ecx=0,
            result=lambda outcome, _: outcome.eax & 0xFF)

        blob = bytearray(0x700)
        count = scenario % 6
        begin = 0x100
        put_u32(blob, 8, begin)
        put_u32(blob, 0xC, begin + count * 12)
        weighted_fixups = [8, 0xC]
        weights: list[int] = []
        for index in range(count):
            model_offset = 0x300 + index * 0x60
            put_u32(blob, begin + index * 12, model_offset)
            weight = 1 + (scenario + index * 3) % 7
            weights.append(weight)
            put_i32(blob, model_offset + 0x34, weight)
            weighted_fixups.append(begin + index * 12)
        limits = (-1, 0, 1, count, count + 2)
        limit = limits[scenario % len(limits)]
        excluded = (-1, 0, max(0, count - 1), count + 1)[scenario % 4]
        total_weight = sum(weights[:max(0, min(count, limit))])
        random_value = (-1, 0, max(0, total_weight - 1), total_weight, total_weight + 1)[
            scenario % 5]

        def weighted_invoke(
            machine: Emulator, base: int, excluded_value: int = excluded,
            limit_value: int = limit, random_result: int = random_value
        ) -> tuple[CallResult, int]:
            machine.bind_function(
                0x005A94F0, lambda _, result=random_result: result)
            outcome = machine.call(
                0x008E8A00, [excluded_value, limit_value], ecx=base)
            return outcome, outcome.eax

        verifier.case(
            0x008E8A00, bytes(blob), weighted_fixups,
            [0, excluded, limit, random_value], weighted_invoke, 8,
            compare_result=True)

        for address, begin_field, end_field, stride, selected_field in (
            (0x008E8B20, 0x260, 0x264, 0x74, 0x210),
            (0x008E8C10, 0x2C, 0x30, 0x88, 0x20C),
        ):
            blob = bytearray(0xA00)
            entry_count = scenario % 5
            begin = 0x300
            conditions = 0x900
            put_u32(blob, begin_field, begin)
            put_u32(blob, end_field, begin + entry_count * stride)
            requested_word = scenario % 15
            requested_bit = 1 << (scenario % 31)
            if scenario % 4:
                put_u32(blob, conditions + requested_word * 4, requested_bit)
            condition_fixups = [begin_field, end_field, selected_field]
            for index in range(entry_count):
                model_offset = 0x600 + index * 0x60
                entry = begin + index * stride
                put_u32(blob, entry, model_offset)
                condition_fixups.append(entry)
                mode = (scenario + index) % 4
                if mode == 1:
                    put_u32(blob, model_offset + 0xC + requested_word * 4, requested_bit)
                elif mode == 2:
                    put_u32(
                        blob, model_offset + 0xC + requested_word * 4,
                        requested_bit | 0x80000000)
                elif mode == 3:
                    put_u32(
                        blob, model_offset + 0xC + ((requested_word + 1) % 15) * 4,
                        1 << ((scenario + 7) % 31))
            selected = 0
            if entry_count and scenario % 7 == 0:
                selected = begin + (scenario % entry_count) * stride
            put_u32(blob, selected_field, selected)
            call(
                address, blob, condition_fixups, [0, conditions],
                lambda base, cond=conditions: [base + cond],
                ecx=0, cleanup=4,
                result=lambda outcome, base:
                outcome.eax - base if outcome.eax else 0)

        blob = bytearray(0x800)
        right_state = 0x100
        left_count = scenario % 4
        right_count = left_count if scenario % 3 else (left_count + 1) % 4
        left_begin = 0x300
        right_begin = 0x500
        blob[0x70] = 0 if scenario % 11 == 0 else 1
        blob[right_state + 0x70] = 0 if scenario % 13 == 0 else 1
        put_u32(blob, 0x60, left_begin)
        put_u32(blob, 0x64, left_begin + left_count * 28)
        put_u32(blob, right_state + 0x60, right_begin)
        put_u32(blob, right_state + 0x64, right_begin + right_count * 28)
        for index in range(max(left_count, right_count)):
            first = rng.randrange(2**32)
            second = rng.randrange(2**32)
            if index < left_count:
                put_u32(blob, left_begin + index * 28, first)
                put_u32(blob, left_begin + index * 28 + 4, second)
            if index < right_count:
                put_u32(
                    blob, right_begin + index * 28,
                    first ^ (1 if scenario % 5 == 0 else 0))
                put_u32(blob, right_begin + index * 28 + 4, second)
        call(
            0x008E92A0, blob, [0x60, 0x64, 0x160, 0x164],
            [0, right_state], lambda _: [], ecx=0, eax=right_state,
            result=lambda outcome, _: outcome.eax & 0xFF)

        blob = random_blob(0x380)
        subobject = 0x20
        owner = 0x200
        render_object = 0 if scenario % 4 == 0 else 0x300
        put_u32(blob, subobject - 0x10, owner)
        put_u32(blob, subobject - 8, render_object)
        blob[owner + 0xCA] = scenario // 2 & 1
        blob[owner + 0xCB] = scenario // 3 & 1
        selectable = (0, 1, 2, 0xFF)[scenario % 4]
        call(
            0x008E9350, blob, [subobject - 0x10, subobject - 8],
            [subobject, selectable], lambda _, value=selectable: [value],
            ecx=subobject, cleanup=4)

        blob = random_blob(0x240)
        animation = 0 if scenario % 8 == 0 else 0x1C0
        put_u32(blob, 0x108, animation)
        if animation:
            put_i32(blob, animation + 0x14, (0, 1, 2, 20, 1000)[scenario % 5])
        put_i32(blob, 0x118, (-1, 0, 3, 4, 5)[scenario % 5])
        fraction_bits = finite_bits(scenario, 2.0)
        call(
            0x008E95F0, blob, [0x108], [0, fraction_bits],
            lambda _, fraction=fraction_bits: [fraction],
            ecx=0, cleanup=4)

        blob = random_blob(0x160)
        put_u32(blob, 0x11C, (0, 1, 0xFFFFFFFF)[scenario % 3])
        put_u32(blob, 0x138, (0, 2, 0x80000000)[scenario * 2 % 3])
        call(
            0x008E96B0, blob, [], [0], lambda _: [], ecx=0,
            result=lambda outcome, _: outcome.eax & 0xFF)

        blob = random_blob(18 * 4)
        for word in range(9):
            put_u32(blob, word * 4, finite_bits(scenario + word, 100.0))
        call(0x008E98A0, blob, [], [0], lambda _: [], ecx=0)

        blob = random_blob(0x100)
        for word in range(18):
            put_u32(blob, word * 4, finite_bits(scenario + word, 20.0))
        output = 0x80
        fractions = (-1.0, -0.0, 0.0, 0.25, 0.5, 0.75, 1.0, 2.0)
        fraction = fbits(fractions[scenario % len(fractions)])
        call(
            0x008E9980, blob, [], [0, output, fraction],
            lambda base, out=output, frac=fraction: [base + out, frac],
            ecx=0, cleanup=8)

        blob = random_blob(0x300)
        frame_count = scenario % 5
        begin = 0x40
        put_u32(blob, 4, begin)
        put_u32(blob, 8, begin + frame_count * 0x50)
        for index in range(frame_count):
            duration = (0.25, 0.5, 1.0, 2.0, -4.0)[
                (scenario + index) % 5]
            put_f32(blob, begin + index * 0x50 + 0x48, duration)
            put_u32(
                blob, begin + index * 0x50 + 0x4C,
                finite_bits(scenario + index, 10.0))
        elapsed = finite_bits(scenario + 3, 2.0)
        call(
            0x008E9BE0, blob, [4, 8], [0, elapsed],
            lambda _, value=elapsed: [value], ecx=0, cleanup=4)

        tree_case(0x008EADD0, scenario)
        tree_case(0x008EAE00, scenario)

        blob = random_blob(0xC0)
        first = 0x20
        second = 0x50
        output = (first, second, 0x80)[scenario % 3]
        for word in range(4):
            first_value = struct.unpack(
                "<f", struct.pack("<I", finite_bits(scenario + word, 2.0)))[0]
            second_value = first_value if scenario % 2 else -first_value
            put_f32(blob, first + word * 4, first_value)
            put_f32(blob, second + word * 4, second_value)
        fraction = fbits((-1.0, -0.0, 0.0, 0.25, 0.5, 1.0, 2.0)[scenario % 7])
        call(
            0x008EB160, blob, [], [output, first, second, fraction],
            lambda base, out=output, a=first, b=second, frac=fraction:
            [base + out, base + a, base + b, frac])

        for address in (0x008EB620, 0x008EB6D0):
            blob = random_blob(0x500)
            record_count = scenario % 5
            first = 0x100
            last = first + record_count * 0x44
            for index in range(record_count):
                fill_copy_source(blob, first + index * 0x44, 0x44, scenario + index)
            if scenario % 4 == 0:
                destination = first
            elif scenario % 4 == 1:
                destination = first + 0x44
            elif scenario % 4 == 2:
                destination = first - 0x44
            else:
                destination = 0x300
            if address == 0x008EB620:
                args = [first, last, destination]
                call(
                    address, blob, [], args,
                    lambda base, a=first, b=last, d=destination:
                    [base + a, base + b, base + d],
                    result=lambda outcome, base: outcome.eax - base)
            else:
                destination_last = destination + record_count * 0x44
                args = [first, last, destination_last]
                call(
                    address, blob, [], args,
                    lambda base, a=first, b=last, d=destination_last:
                    [base + a, base + b, base + d],
                    result=lambda outcome, base: outcome.eax - base)

        flags = rng.randrange(2**32)
        blob = bytearray(4)
        put_u32(blob, 0, flags)
        call(
            0x008ED5B0, blob, [], [flags], lambda base: [base],
            result=lambda outcome, _: outcome.eax & 0xFF)

        blob = random_blob(0x300)
        origin = 0x20
        target = 0x40
        offset = 0x60
        matrix = 0x200
        drawable = 0x100
        parent = 0 if scenario % 3 == 0 else 0x180
        for vector, delta in ((origin, 0), (target, 5), (offset, -3)):
            for axis in range(3):
                put_u32(
                    blob, vector + axis * 4,
                    finite_bits(scenario + axis + delta, 50.0))
        angles = (-6.2831855, -3.1415927, -0.0, 0.0, 0.5, 3.1415927, 6.2831855)
        put_f32(blob, drawable + 0x64, angles[scenario % len(angles)])
        put_f32(blob, drawable + 0x68, angles[(scenario * 3 + 1) % len(angles)])
        put_u32(blob, drawable + 0x98, parent)
        if parent:
            put_f32(blob, parent + 0x64, angles[(scenario + 2) % len(angles)])
        call(
            0x008EE0B0, blob, [drawable + 0x98],
            [matrix, origin, drawable, target, offset],
            lambda base, out=matrix, a=origin, d=drawable, b=target, off=offset:
            [base + out, base + a, base + d, base + b, base + off])

        blob = random_blob(0x300)
        cursor = 0x100
        first = 0x20
        second = 0x40
        put_u32(blob, 0, cursor)
        segment_patterns = (
            ((0.0, 0.0, 0.0), (0.0, 0.0, 0.0)),
            ((0.0, 0.0, 0.0), (0.009, 0.0, 0.0)),
            ((0.0, 0.0, 0.0), (0.01, 0.0, 0.0)),
            ((1.0, -2.0, 3.0), (-4.0, 5.0, -6.0)),
        )
        first_values, second_values = segment_patterns[scenario % 4]
        if scenario >= len(segment_patterns):
            first_values = tuple(rng.uniform(-100.0, 100.0) for _ in range(3))
            second_values = tuple(rng.uniform(-100.0, 100.0) for _ in range(3))
        for axis in range(3):
            put_f32(blob, first + axis * 4, first_values[axis])
            put_f32(blob, second + axis * 4, second_values[axis])
        widths = (-4.0, -0.0, 0.0, 0.25, 1.0, 2.0, 100.0)
        segment_width = fbits(widths[scenario % len(widths)])
        attribute = rng.randrange(2**32)
        call(
            0x008EFE00, blob, [0],
            [0, first, second, segment_width, attribute],
            lambda base, a=first, b=second, w=segment_width, attr=attribute:
            [base + a, base + b, w, attr],
            edx_offset=0, cleanup=16,
            result=lambda outcome, base: u32(emu.read(base, 4), 0) - base)

    missing = set(ACCEPTED_ENTRIES) - set(verifier.counts)
    if missing:
        raise AssertionError(
            "entries without oracle coverage: "
            + ", ".join(f"0x{address:08X}" for address in sorted(missing)))
    undercovered = {
        address: verifier.counts[address] for address in ACCEPTED_ENTRIES
        if verifier.counts[address] < RANDOM_SCENARIOS_PER_ENTRY
    }
    if undercovered:
        raise AssertionError(
            "entries below scenario minimum: "
            + ", ".join(
                f"0x{address:08X}={count}"
                for address, count in sorted(undercovered.items())))

    verifier.close()
    for address, count in sorted(verifier.counts.items()):
        print(f"0x{address:08X}: {count}/{count} matched, mismatches=0")
    print(f"total: {sum(verifier.counts.values())} cases, "
          f"{len(verifier.counts)}/53 entries, mismatches=0")
    verify_deliberate_mutations(executable)


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument(
        "--render8echeck", type=Path,
        default=Path(__file__).resolve().parents[3]
        / "build/win32/bin/Debug/ra3_render8echeck.exe")
    args = parser.parse_args()
    verify(args.render8echeck.resolve())


if __name__ == "__main__":
    main()
