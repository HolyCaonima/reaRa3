"""Differentially verify the render8e W3D cluster against ra3_1.0.game."""

from __future__ import annotations

import argparse
import random
import struct
import subprocess
import sys
from pathlib import Path
from typing import Callable

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


class Verifier:
    def __init__(self, executable: Path) -> None:
        self.emu = Emulator()
        self.cpp = CppOracle(executable)
        self.counts: dict[int, int] = {}

    def case(
        self, address: int, blob: bytes, fixups: list[int], arguments: list[int],
        invoke: Invoke, cleanup: int, *, compare_result: bool = False
    ) -> None:
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

    def close(self) -> None:
        self.cpp.close()


def verify_deliberate_mutation(executable: Path) -> None:
    address = 0x008E4610
    value = 0xA17E5EED
    blob = bytes(0x110)
    cpp = CppOracle(executable)
    try:
        _, candidate = cpp.call(address, blob, [], [0, value])
    finally:
        cpp.close()
    mutated = bytearray(candidate)
    mutated[0x104] ^= 1

    emu = Emulator()
    base = emu.alloc(blob)
    result = emu.call(address, [value], ecx=base)
    if result.stack_cleanup != 4:
        raise AssertionError(
            f"mutation probe cleanup {result.stack_cleanup}, expected 4")
    game = emu.read(base, len(blob))
    if game == bytes(mutated):
        raise AssertionError("deliberate mutation was not detected")
    first = next(
        index for index, pair in enumerate(zip(game, mutated))
        if pair[0] != pair[1])
    if first != 0x104:
        raise AssertionError(
            f"deliberate mutation detected at unexpected +0x{first:X}")
    print("deliberate mutation: detected at 0x008E4610 +0x104")


def verify(executable: Path) -> None:
    rng = random.Random(0x008E53D)
    verifier = Verifier(executable)
    emu = verifier.emu

    def random_blob(size: int) -> bytearray:
        return bytearray(rng.randbytes(size))

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
            compare_result=result is not None)

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

    verifier.close()
    for address, count in sorted(verifier.counts.items()):
        print(f"0x{address:08X}: {count}/{count} matched, mismatches=0")
    print(f"total: {sum(verifier.counts.values())} cases, "
          f"{len(verifier.counts)}/53 entries, mismatches=0")
    verify_deliberate_mutation(executable)


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
