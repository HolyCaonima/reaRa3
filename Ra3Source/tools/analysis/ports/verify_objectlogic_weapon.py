"""Offline oracle for the closed ObjectLogic Weapon family.

The native identity check reads only the immutable run35 absolute snapshot.
The checker protocol compares the independently transcribed semantic
observations, including ABI cleanup, register results, explicit writes, float
bits, and full 80-bit x87 encodings.
"""

from __future__ import annotations

import argparse
import hashlib
import math
import random
import struct
import subprocess
import sys
from fractions import Fraction
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from emulate import Emulator  # noqa: E402


ENTRIES = (
    0x00685DC0, 0x006A2400, 0x006A2430,
    0x006A2440, 0x006A24A0, 0x006A24F0, 0x006A2560,
    0x006A25F0, 0x006A2630, 0x006A2840, 0x006A2AB0,
    0x006A2BD0, 0x006A2D30, 0x006A2DC0,
    0x006A3120, 0x006A82F0, 0x006A8530, 0x006A8580, 0x006A8650,
    0x006A86C0, 0x006A8700, 0x006A8840, 0x006A8870, 0x006A88F0,
    0x006BD260, 0x006BD2C0, 0x006BD310, 0x006ED440, 0x006ED4A0,
)

FLOAT_ENTRIES = {
    0x00685DC0, 0x006A2630, 0x006A2D30, 0x006A2DC0, 0x006A8580,
}

# sha256(concatenation of 16 native bytes at every ENTRIES address), from the
# immutable run35 snapshot named below.
NATIVE_PREFIX_SHA256 = (
    "6c767613ad8585de65970edaef2eb7c43545c067eee822647c431bf4b6015f4d")

WORDS = (100, 90, 20, 1, 7, 3, 0x16, 5)
VALUES = (2.0, 3.0, 4.0, 5.0, 6.0, 7.0, 30.0, 1.0)
CONTROL_WORDS = (0x007F, 0x027F, 0x037F)
CURRENT_CONTROL_WORD = CONTROL_WORDS[0]

SIMPLE_ENTRIES = {
    0x00685DC0, 0x006A2430, 0x006A2840,
    0x006A2D30, 0x006A86C0, 0x006A8840,
}
SCENARIO_COUNTS = {
    address: (32 if address in SIMPLE_ENTRIES else 64)
    for address in ENTRIES
}


def scenario(address: int, index: int) -> tuple[tuple[int, ...], tuple[float, ...], int]:
    rng = random.Random((address << 9) ^ index ^ 0x4F424A4C)
    words = [rng.getrandbits(24) | 1 for _ in range(8)]
    values = [f32(rng.uniform(-20.0, 20.0)) for _ in range(8)]
    values[6] = f32(rng.uniform(1.0, 60.0))
    values[7] = f32(rng.uniform(0.25, 2.0))

    if address == 0x00685DC0:
        edge_bits = (
            0x00000000, 0x80000000, 0x00000001, 0x007FFFFF,
            0x7F800000, 0xFF800000, 0x7FC00001, 0xFFC00001,
        )
        values[index % 8] = struct.unpack(
            "<f", struct.pack("<I", edge_bits[index % len(edge_bits)]))[0]
    elif address == 0x006A2400:
        words[0] = 0 if index % 8 == 0 else rng.getrandbits(24) | 1
        words[1] = 0 if index % 4 == 0 else 1
        words[2] = index % 10
    elif address == 0x006A2440:
        mode = index % 5
        words[0] = 7 if mode == 0 else index % 6
        words[1] = 3 if mode == 1 else 4
        words[2] = 0 if mode == 2 else 1
        words[3] = 1 if mode == 3 else 0
    elif address == 0x006A2560:
        words[6] = rng.getrandbits(8)
    elif address == 0x006A25F0:
        words[0] = 0 if index % 8 == 0 else rng.getrandbits(24) | 1
        words[6] = words[0] if index % 3 == 0 else (words[0] ^ 0x100)
    elif address == 0x006A2630:
        words[0] = index & 1
        branch = index % 4
        if branch == 0:
            values[:3] = [12.0, 2.0, 1.0]
        elif branch == 1:
            values[:3] = [6.0, 4.0, 1.0]
        elif branch == 2:
            values[:3] = [4.0, 3.0, 5.0]
        else:
            values[:3] = [20.0, 7.0, 2.0]
    elif address == 0x006A2AB0:
        words[1] = rng.randrange(0, 1000)
        words[2] = words[1] + rng.randrange(0, 1000)
        words[3] = rng.getrandbits(24)
        values[0] = f32((index % 9 + 1) * 0.25)
    elif address == 0x006A2BD0:
        mode = index % 6
        words[0] = rng.randrange(0, 1000)
        words[1] = rng.randrange(0, 1000)
        words[2] = rng.randrange(1, 300)
        words[3] = 0 if mode == 0 else 1
        words[4] = 0xFFFFFFFF if mode == 1 else rng.getrandbits(16)
        words[5] = 1 if mode in (2, 4) else 0
    elif address == 0x006A2D30:
        words[0] = index & 1
        values[0] = f32((index % 11 + 1) * 0.125)
        values[1] = f32((index % 7 + 1) * 0.5)
        if index >= 24:
            edge_bits = (
                0x00000000, 0x80000000, 0x00000001, 0x80000001,
                0x007FFFFF, 0x807FFFFF, 0x3F800000, 0xBF800000,
            )
            values[0] = struct.unpack(
                "<f", struct.pack("<I", edge_bits[index - 24]))[0]
            values[1] = struct.unpack(
                "<f", struct.pack("<I", edge_bits[31 - index]))[0]
    elif address == 0x006A2DC0:
        values[0] = f32(5.0 + (index % 13))
        values[1] = f32(-2.0 + (index % 5))
        values[2] = f32(0.5 + (index % 7))
        values[3] = f32(1.0 + (index % 9))
        values[4] = f32(-4.0 + (index % 11))
    elif address == 0x006A3120:
        words[2] = 4 if index % 3 == 0 else index % 8
    elif address == 0x006A82F0:
        words[0] = 0 if index % 8 == 0 else 1
        words[1] = rng.getrandbits(24)
        words[2] = rng.getrandbits(24)
        words[3] = rng.randrange(0, 1000)
        words[4] = rng.randrange(0, 100)
        pitch_mode = index % 4
        values[0] = -4.0 if pitch_mode in (0, 3) else -1.0
        values[1] = 4.0 if pitch_mode in (0, 2) else 1.0
    elif address == 0x006A8530:
        values[0] = f32(index % 13)
        range_value = max(0.0, values[0] - 5.0)
        distance = (
            0.0 if index % 4 == 0
            else range_value * range_value if index % 4 == 1
            else max(0.0, range_value * range_value - 0.25)
            if index % 4 == 2 else range_value * range_value + 0.25)
        values[1] = f32(distance)
    elif address == 0x006A8580:
        words[2] = 0 if index % 11 == 0 else 1
        words[4] = index % 8
        words[5] = 0 if index % 13 == 0 else 1
        words[3] = rng.randrange(0, 1000)
        words[1] = words[3] + rng.randrange(0, 1000)
        timeline = index % 4
        words[0] = (
            words[3] if timeline == 0
            else words[1] if timeline == 1
            else (words[3] + words[1]) // 2 if timeline == 2
            else max(0, words[3] - 1))
    elif address == 0x006A8650:
        words[0] = 1 if index & 1 else 0
        words[1] = 1 if index & 2 else 0
        words[2] = 1 if index & 4 else 0
        words[3] = 1 if index & 8 else 0
    elif address == 0x006A86C0:
        words[0] = 0 if index % 7 == 0 else 1
        words[1] = index % 8
        words[2] = 0 if index % 11 == 0 else 1
    elif address == 0x006A8700:
        words[0] = 1 if index % 9 == 0 else 0
        words[1] = 0 if index % 7 == 0 else 1
        values[0] = 0.0
        values[1] = 5.0 if index % 5 == 0 else 15.0
        values[2] = f32(-4.0 + index % 9)
        values[3] = f32(-4.0 + (index * 3) % 9)
        values[4] = -1.5
        values[5] = 1.5
    elif address == 0x006A8840:
        words[0] = 0 if index % 7 == 0 else 1
        words[1] = index % 8
        words[2] = index & 1
        words[3] = rng.getrandbits(24)
    elif address == 0x006A8870:
        words[0] = 0 if index % 7 == 0 else 1
        words[1] = 0 if index % 5 == 0 else 1
        words[2] = 0 if index % 11 == 0 else 1
        words[3] = index % 7
        words[4] = index & 1
    elif address == 0x006A88F0:
        mode = index % 5
        words[0] = 0 if mode == 0 else 1
        words[1] = index + 1
        words[2] = 0 if mode == 1 else 1
        words[3] = words[1] if mode >= 3 else words[1] + 1
        words[4] = 1 if mode == 4 else 0
    elif address == 0x006BD260:
        words[3] = index & 1
        words[4] = rng.randrange(0, 1000)
        words[5] = rng.randrange(0, 1000)
    elif address == 0x006ED440:
        words[1] = rng.randrange(0, 1000)
        words[0] = (
            words[1] + 1 + rng.randrange(1000)
            if index & 1 else rng.randrange(0, words[1] + 1))
        words[2] = 0 if index % 4 == 0 else rng.randrange(1, 1000)

    control_word = CONTROL_WORDS[index % len(CONTROL_WORDS)]
    return tuple(u32(value) for value in words), tuple(values), control_word


class Checker:
    def __init__(self, executable: Path) -> None:
        self.process = subprocess.Popen(
            [str(executable)], stdin=subprocess.PIPE, stdout=subprocess.PIPE,
            text=True, bufsize=1)

    def call(self, address: int, mutate: bool = False) -> str:
        assert self.process.stdin is not None and self.process.stdout is not None
        self.process.stdin.write(f"fpcw {CURRENT_CONTROL_WORD:04x}\n")
        self.process.stdin.flush()
        if self.process.stdout.readline().strip() != "ok":
            raise RuntimeError("objectlogiccheck rejected FPU control word")
        values = [float_bits(value) for value in VALUES]
        operation = (
            "layout_mutate" if mutate else "layout"
        ) if address == 0x006A82F0 else ("mutate" if mutate else "probe")
        command = " ".join([
            operation, f"{address:08x}",
            *(f"{value:08x}" for value in WORDS),
            *(f"{value:08x}" for value in values),
        ])
        self.process.stdin.write(command + "\n")
        self.process.stdin.flush()
        result = self.process.stdout.readline().strip()
        if not result or result == "error":
            raise RuntimeError(f"objectlogiccheck rejected {command!r}")
        return result

    def alias(self, mutate: bool = False) -> str:
        assert self.process.stdin is not None and self.process.stdout is not None
        self.process.stdin.write("alias_mutate\n" if mutate else "alias\n")
        self.process.stdin.flush()
        result = self.process.stdout.readline().strip()
        if not result or result == "error":
            raise RuntimeError("objectlogiccheck rejected alias case")
        return result

    def close(self) -> None:
        assert self.process.stdin is not None
        self.process.stdin.write("quit\n")
        self.process.stdin.flush()
        if self.process.wait(timeout=10) != 0:
            raise RuntimeError("objectlogiccheck failed")


def u32(value: int) -> int:
    return value & 0xFFFFFFFF


def f32(value: float) -> float:
    return struct.unpack("<f", struct.pack("<f", value))[0]


def float_bits(value: float) -> int:
    return struct.unpack("<I", struct.pack("<f", value))[0]


def ext80(bits: int) -> bytes:
    sign = bits >> 31
    exponent = (bits >> 23) & 0xFF
    fraction = bits & 0x7FFFFF
    if exponent == 0 and fraction == 0:
        return struct.pack("<QH", 0, sign << 15)
    if exponent == 0:
        value = struct.unpack("<f", struct.pack("<I", bits))[0]
        return ext80(float_bits(value))
    if exponent == 0xFF:
        significand = 0x8000000000000000 | (fraction << 40)
        return struct.pack("<QH", significand, (sign << 15) | 0x7FFF)
    significand = 0x8000000000000000 | (fraction << 40)
    extended_exponent = exponent - 127 + 16383
    return struct.pack("<QH", significand, (sign << 15) | extended_exponent)


def ratio_ext80(numerator: int, denominator: int, control_word: int) -> bytes:
    value = Fraction(numerator, denominator)
    if value == 0:
        return bytes(10)
    exponent = value.numerator.bit_length() - value.denominator.bit_length()
    if exponent >= 0:
        normalized = value / (1 << exponent)
    else:
        normalized = value * (1 << -exponent)
    if normalized < 1:
        exponent -= 1
        normalized *= 2
    precision_code = (control_word >> 8) & 3
    precision = {0: 24, 2: 53, 3: 64}[precision_code]
    scaled = normalized * (1 << (precision - 1))
    quotient, remainder = divmod(scaled.numerator, scaled.denominator)
    doubled = remainder * 2
    if doubled > scaled.denominator or (
            doubled == scaled.denominator and quotient & 1):
        quotient += 1
    if quotient == 1 << precision:
        quotient >>= 1
        exponent += 1
    significand = quotient << (64 - precision)
    return struct.pack("<QH", significand, exponent + 16383)


def encode(address: int, eax: int = 0, cleanup: int = 0, st0: float | None = None,
           writes: tuple[int, ...] = ()) -> str:
    padded = tuple(u32(value) for value in writes) + (0,) * (4 - len(writes))
    write_mask = (1 << len(writes)) - 1
    st0_bits = 0 if st0 is None else float_bits(st0)
    fields = (u32(eax), 0, cleanup, st0_bits, write_mask, *padded)
    result = "".join(f"{value:08x}" for value in fields)
    result += (ext80(st0_bits) if address in FLOAT_ENTRIES else bytes(10)).hex()
    return result


def constructor_layout() -> str:
    result = bytearray(b"\xA5" * 0x78)

    def store(offset: int, value: int) -> None:
        result[offset:offset + 4] = struct.pack("<I", u32(value))

    store(0x00, 0x00C07D54)
    store(0x04, 1 if WORDS[0] else 0)
    for offset in (0x08, 0x0C, 0x10, 0x18, 0x1C, 0x28,
                   0x34, 0x38, 0x3C, 0x4C, 0x50, 0x54,
                   0x58, 0x64, 0x68, 0x6C, 0x70, 0x74):
        store(offset, 0)
    store(0x14, WORDS[1])
    store(0x20, u32(WORDS[2] + WORDS[3]) if WORDS[0] else 0)
    store(0x24, 0x7FFFFFFF)
    store(0x2C, WORDS[4] if WORDS[0] else 1)
    pitch_limited = (
        WORDS[0] != 0
        and (VALUES[0] > -3.1415927410125732
             or VALUES[1] < 3.1415927410125732))
    result[0x30] = 1 if pitch_limited else 0
    for offset in (0x40, 0x44, 0x48):
        store(offset, float_bits(-1.0))
    store(0x5C, 0xFFFFFFFF)
    result[0x60:0x64] = bytes(4)
    return "L00000008" + result.hex()


def reference(address: int) -> str:
    w = WORDS
    v = VALUES
    deadline = u32(w[1] + w[2])
    if address == 0x00685DC0:
        return encode(address, cleanup=8, st0=0.0)
    if address == 0x006A2DC0:
        pitch = -v[4]
        value = f32(v[0] - 5.0)
        if -pitch >= v[1]:
            value = f32(value + f32(
                v[2] - f32(f32(v[1] + pitch) * v[3])))
        if (v[3] > 0.0 and abs(pitch) > v[3]) or value < 0.0:
            value = 0.0
        return encode(address, cleanup=4, st0=value)
    if address == 0x006A2400:
        writes = (min(w[2], 5),) if w[0] and w[1] else ()
        return encode(address, w[0], 4, writes=writes)
    if address == 0x006A2430:
        return encode(address, w[w[7] % 6], 4)
    if address == 0x006A2440:
        if w[0] >= 6:
            value = 1
        elif w[1] != 4 or not w[2]:
            value = 0
        else:
            value = 0 if w[3] else 1
        return encode(address, value)
    if address == 0x006A24A0:
        first = 0
        second = 0
        for value in w[:6]:
            first |= value
            second |= u32(value << 1)
        return encode(address, w[5] << 1, writes=(first, second))
    if address == 0x006A24F0:
        best = max(range(6), key=lambda index: w[index])
        return encode(
            address, w[7], cleanup=4,
            writes=tuple(
                float_bits(v[(best + offset) % len(v)])
                for offset in range(3)))
    if address == 0x006A2560:
        count = sum(value & 0xFF for value in w[:6] if value & w[6])
        return encode(address, count, 8)
    if address == 0x006A25F0:
        matched = w[0] != 0 and w[0] == w[6]
        return encode(address, w[7] if matched else 0, 8)
    if address == 0x006A2630:
        if w[0]:
            value = v[1]
        else:
            minimum = max(0.0, f32(v[0] - 5.0))
            if minimum > v[1]:
                value = f32(minimum - v[1])
            elif v[1] > v[2]:
                value = f32(v[1] - v[2])
            else:
                value = 0.0
        return encode(address, cleanup=12, st0=value)
    if address == 0x006A2840:
        return encode(address, deadline)
    if address == 0x006A2AB0:
        span = u32(w[2] - w[1] + 1)
        random_value = w[1] + w[3] % span
        divisor = v[0] if v[0] != 0.0 else 1.0
        return encode(address, int(f32(random_value) / divisor), 8)
    if address == 0x006A2BD0:
        if not w[3] or w[4] == 0xFFFFFFFF:
            return encode(address, 0xFFFFFFFE)
        fraction = min(1.0, max(0.0, u32(w[0] - w[1]) / max(w[2], 1)))
        transition = 0xFFFFFFFF if w[1] + w[2] <= w[0] or w[5] else 0
        return encode(
            address, transition, writes=(float_bits(f32(fraction)),))
    if address == 0x006A2D30:
        value = f32(v[0] * v[1]) if w[0] else 1.0
        return encode(address, cleanup=4, st0=value)
    if address == 0x006A3120:
        return encode(address, w[3] if w[3] and w[2] == 4 else w[4], 4)
    if address == 0x006A82F0:
        return constructor_layout()
    if address == 0x006A8530:
        range_value = max(0.0, f32(v[0] - 5.0))
        return encode(
            address, 1 if range_value > 0.0
            and v[1] < f32(range_value * range_value) else 0, 8)
    if address == 0x006A8580:
        if not w[2]:
            value = 1.0
        else:
            state = w[4] if w[5] else 0xF423F
            if state == 0:
                value = 1.0
            elif state in (1, 4) or state > 5:
                value = 0.0
            elif w[0] >= w[1]:
                value = 1.0
            else:
                duration = u32(w[1] - w[3])
                elapsed = u32(w[0] - w[3])
                value = (
                    1.0 if not duration or elapsed >= duration
                    else f32(elapsed / duration))
        result = encode(address, st0=value)
        if (w[2] and w[5] and w[4] in (2, 3, 5)
                and w[0] < w[1]):
            duration = u32(w[1] - w[3])
            elapsed = u32(w[0] - w[3])
            if duration and elapsed < duration:
                result = result[:-20] + ratio_ext80(
                    elapsed, duration, CURRENT_CONTROL_WORD).hex()
        return result
    if address == 0x006A8650:
        value = 0 if w[0] and w[2] else (1 if not w[1] or w[3] else 0)
        return encode(address, value, 4)
    if address == 0x006A86C0:
        return encode(
            address, 1 if w[0] and w[2] and w[1] == 2 else 0)
    if address == 0x006A8700:
        if w[0] or not w[1] or abs(v[1] - v[0]) < 10.0:
            value = 1
        else:
            first_inside = v[4] <= v[2] <= v[5]
            second_inside = v[4] <= v[3] <= v[5]
            value = int(
                first_inside or second_inside
                or (v[2] <= v[4] and v[3] >= v[5]))
        return encode(address, value, 8)
    if address == 0x006A8840:
        return encode(
            address, 0 if w[2] and w[0] and w[1] == 5 else w[3], 4)
    if address == 0x006A8870:
        state = 0 if not w[1] else (w[3] if w[2] else 0xF423F)
        value = w[0] and (
            state in (1, 2) or (state == 0 and w[4]))
        return encode(address, 1 if value else 0)
    if address == 0x006A88F0:
        valid = w[0] and w[2] and w[1] == w[3]
        writes: tuple[int, ...] = ()
        if valid:
            output = [v[0], v[1], v[2]]
            if w[4]:
                output = [
                    f32(f32(v[3] * output[0]) + v[6]),
                    f32(f32(v[4] * output[1]) + v[7]),
                    f32(v[5] * output[2]),
                ]
            writes = tuple(float_bits(value) for value in output)
        return encode(address, 1 if valid else 0, 8, writes=writes)
    if address == 0x006BD260:
        return encode(address, u32(
            deadline + u32(w[4] + w[5] + 1) // 2))
    if address == 0x006BD2C0:
        return encode(address, w[7], writes=(w[0], w[1], 3))
    if address == 0x006BD310:
        return encode(address, 0xFFFFFFFE)
    if address == 0x006ED440:
        return encode(
            address, 0xFFFFFFFF,
            writes=(0x7FFFFFFF if w[2] == 0 else w[2],))
    if address == 0x006ED4A0:
        shifted_flags = (w[0] >> 14) & 0xFFFFFF01
        eax = w[7] if w[0] & 0x4000 else shifted_flags
        return encode(
            address, eax, cleanup=4, writes=(0, w[0] & ~0x4000))
    raise AssertionError(f"missing reference for 0x{address:08X}")


def verify_native_identity(snapshot: Path) -> None:
    emulator = Emulator(capture=str(snapshot))
    digest = hashlib.sha256()
    for address in ENTRIES:
        digest.update(emulator.read(address, 16))
    actual = digest.hexdigest()
    if actual != NATIVE_PREFIX_SHA256:
        raise AssertionError(
            f"run35 Weapon native identity {actual} != {NATIVE_PREFIX_SHA256}")


def native_projection(snapshot: Path, address: int) -> str:
    """Execute one accepted native body and normalize only pointer identities."""
    emu = Emulator(capture=str(snapshot))

    def alloc(size: int) -> int:
        return emu.alloc(bytes(size))

    def put32(pointer: int, offset: int, value: int) -> None:
        emu.write(pointer + offset, struct.pack("<I", u32(value)))

    def putf(pointer: int, offset: int, value: float) -> None:
        put32(pointer, offset, float_bits(value))

    logic = alloc(0x200)
    put32(logic, 0x58, WORDS[0])
    putf(logic, 0x1C4, VALUES[6])
    emu.write(0x00DEBA64, struct.pack("<I", logic))
    emu.write(0x00DEEEE8, struct.pack("<f", VALUES[7]))

    def integer(
        args: list[int] | tuple[int, ...] = (), *, ecx: int = 0,
        eax: int | None = None, al: bool = False,
        writes: tuple[int, ...] = ()) -> str:
        result = emu.call(address, args, ecx=ecx)
        observed = result.eax if eax is None else eax
        if al:
            observed = result.eax & 0xFF
        return encode(address, observed, result.stack_cleanup, writes=writes)

    def floating(
        args: list[int] | tuple[int, ...] = (), *, ecx: int = 0) -> str:
        result, extended = emu.call_float80(
            address, args, ecx=ecx, fpu_control_word=CURRENT_CONTROL_WORD)
        significand, exponent = struct.unpack("<QH", extended)
        if (exponent & 0x7FFF) == 0:
            value = -0.0 if exponent >> 15 else 0.0
        else:
            value = math.ldexp(
                significand / float(1 << 63),
                (exponent & 0x7FFF) - 16383)
            if exponent >> 15:
                value = -value
        result_text = encode(
            address, cleanup=result.stack_cleanup, st0=f32(value))
        if address == 0x006A8580:
            return result_text[:-20] + extended.hex()
        if result_text[-20:] != extended.hex():
            raise AssertionError(
                f"0x{address:08X} x87 mismatch "
                f"{extended.hex()}/{result_text[-20:]}")
        return result_text

    if address == 0x00685DC0:
        return floating([0, 0])
    if address == 0x006A2DC0:
        data = alloc(0x160)
        putf(data, 0x04, VALUES[0])
        putf(data, 0x08, VALUES[0])
        putf(data, 0x0C, VALUES[1])
        putf(data, 0x10, VALUES[2])
        putf(data, 0x14, VALUES[3])
        putf(data, 0xA8, VALUES[3])
        wrapper = alloc(8)
        put32(wrapper, 4, data)
        target = alloc(0x10)
        putf(target, 8, VALUES[4])
        return floating([target], ecx=wrapper)
    if address == 0x006A2400:
        slots = alloc(24)
        put32(slots, 0, WORDS[0])
        slot_wrapper = alloc(16)
        put32(slot_wrapper, 4, WORDS[2])
        owner = alloc(0xA0)
        put32(owner, 0x0C, slot_wrapper)
        put32(owner, 0x94, slots)
        output = alloc(4) if WORDS[1] != 0 else 0
        result = emu.call(address, [output], ecx=owner)
        writes = ()
        if output != 0 and WORDS[0] != 0:
            writes = (struct.unpack("<I", emu.read(output, 4))[0],)
        return encode(
            address, result.eax, result.stack_cleanup,
            writes=writes)
    if address == 0x006A2430:
        slots = alloc(24)
        for index, value in enumerate(WORDS[:6]):
            put32(slots, index * 4, value)
        owner = alloc(0xA0)
        put32(owner, 0x94, slots)
        return integer([WORDS[7] % 6], ecx=owner)
    if address == 0x006A2440:
        owner = alloc(0xA0)
        put32(owner, 0x14, WORDS[0])
        if WORDS[0] < 6:
            descriptor = alloc(0x10)
            put32(descriptor, 0x0C, WORDS[1])
            put32(owner, 0x0C, descriptor)
            slots = alloc(24)
            put32(owner, 0x94, slots)
            if WORDS[2] != 0:
                weapon = alloc(4)
                put32(slots, WORDS[0] * 4, weapon)
                emu.bind_function(0x00688810, lambda _: WORDS[3])
        return integer(ecx=owner)
    if address == 0x006A24A0:
        slots = alloc(24)
        for index, value in enumerate(WORDS[:6]):
            data = alloc(0xD8)
            put32(data, 0xD0, value)
            put32(data, 0xD4, value << 1)
            wrapper = alloc(8)
            put32(wrapper, 4, data)
            weapon = alloc(8)
            put32(weapon, 4, wrapper)
            put32(slots, index * 4, weapon)
        owner = alloc(0xA0)
        put32(owner, 0x94, slots)
        result = emu.call(address, ecx=owner)
        outputs = struct.unpack("<II", emu.read(owner + 4, 8))
        return encode(address, result.eax, result.stack_cleanup, writes=outputs)
    if address == 0x006A24F0:
        slots = alloc(24)
        for index, value in enumerate(WORDS[:6]):
            runtime = alloc(0x40)
            put32(runtime, 0x30, value)
            putf(runtime, 0x34, VALUES[index])
            putf(runtime, 0x38, VALUES[(index + 1) % len(VALUES)])
            putf(runtime, 0x3C, VALUES[(index + 2) % len(VALUES)])
            weapon = alloc(0x70)
            put32(weapon, 0x68, runtime)
            put32(slots, index * 4, weapon)
        owner = alloc(0xA0)
        put32(owner, 0x94, slots)
        output = alloc(12)
        result = emu.call(address, [output], ecx=owner)
        if result.eax != output:
            raise AssertionError("006A24F0 hidden output EAX")
        return encode(
            address, WORDS[7], result.stack_cleanup,
            writes=struct.unpack("<III", emu.read(output, 12)))
    if address == 0x006A2560:
        slots = alloc(24)
        token = 0x13579BDF
        contributions = tuple(
            value & 0xFF if value & WORDS[6] else 0
            for value in WORDS[:6])
        for index, value in enumerate(contributions):
            runtime = alloc(0x24)
            put32(runtime, 0x0C, value)
            put32(runtime, 0x10, token)
            weapon = alloc(0x70)
            put32(weapon, 0x68, runtime)
            put32(slots, index * 4, weapon)
        owner = alloc(0xA0)
        put32(owner, 0x94, slots)
        target = alloc(0xE0)
        put32(target, 0xDC, token)
        return integer([target, 0], ecx=owner)
    if address == 0x006A25F0:
        owner = alloc(0xA0)
        slot = WORDS[7] % 6
        needle = alloc(4) if WORDS[0] != 0 else 0
        if needle != 0:
            candidate_template = needle if WORDS[0] == WORDS[6] else alloc(4)
            candidate = alloc(8)
            put32(candidate, 4, candidate_template)
            array = alloc(4)
            put32(array, 0, candidate)
            put32(owner, 0x34 + slot * 16, array)
            put32(owner, 0x38 + slot * 16, array + 4)
        result = emu.call(address, [needle, slot], ecx=owner)
        normalized = WORDS[7] if needle != 0 and result.eax != 0 else 0
        return encode(address, normalized, result.stack_cleanup)
    if address == 0x006A2630:
        data = alloc(0x140)
        emu.write(data + 0x12F, bytes((1 if WORDS[0] != 0 else 0,)))
        putf(data, 8, VALUES[0])
        wrapper = alloc(8)
        put32(wrapper, 4, data)
        weapon = alloc(8)
        put32(weapon, 4, wrapper)
        return floating([
            weapon, float_bits(VALUES[1]), float_bits(VALUES[2])])
    if address == 0x006A2840:
        state = alloc(0x40)
        put32(state, 0x24, WORDS[1])
        put32(state, 0x28, WORDS[2])
        return integer(ecx=state)
    if address == 0x006A2AB0:
        ability = alloc(4)
        game_object = alloc(0x370)
        put32(game_object, 0x36C, ability)
        owner = alloc(0x28)
        put32(owner, 0x24, game_object)
        state = alloc(0x20)
        put32(state, 0x18, owner)

        def boost(machine: Emulator) -> int:
            machine.write(machine.stack_arg(1), struct.pack("<f", VALUES[0]))
            return 1

        emu.bind_function(0x004D9CE0, boost, stack_cleanup=12)
        span = u32(WORDS[2] - WORDS[1] + 1)
        random_value = WORDS[1] + WORDS[3] % span
        emu.bind_function(0x005A94C0, lambda _: random_value)
        return integer([WORDS[1], WORDS[2]], ecx=state)
    if address == 0x006A2BD0:
        weapon = alloc(0x90)
        emu.write(weapon + 0x60, bytes((1 if WORDS[3] != 0 else 0,)))
        put32(weapon, 0x5C, WORDS[4])
        target = alloc(0x98)
        put32(target, 0x94, (1 << 20) if WORDS[5] != 0 else 0)
        put32(weapon, 0x24, target)
        state = alloc(0x30)
        put32(state, 0x18, weapon)
        put32(state, 0x20, weapon)
        put32(state, 0x24, WORDS[1])
        put32(state, 0x28, WORDS[2])
        result = emu.call(address, ecx=state)
        writes = ()
        if WORDS[3] != 0 and WORDS[4] != 0xFFFFFFFF:
            writes = (struct.unpack("<I", emu.read(weapon + 0x88, 4))[0],)
        return encode(
            address, result.eax, result.stack_cleanup,
            writes=writes)
    if address == 0x006A2D30:
        data = alloc(0x130)
        wrapper = alloc(8)
        put32(wrapper, 4, data)
        ability = alloc(4)
        target = alloc(0x370)
        put32(target, 0x36C, ability if WORDS[0] != 0 else 0)
        put32(target, 0x84, (1 << 29) if WORDS[0] != 0 else 0)
        settings = alloc(0x44)
        putf(settings, 0x40, VALUES[1] if WORDS[0] != 0 else 1.0)
        emu.write(0x00DFC4B4, struct.pack("<I", settings))

        def speed(machine: Emulator) -> int:
            machine.write(machine.stack_arg(1), struct.pack("<f", VALUES[0]))
            return 1

        emu.bind_function(0x004D9CE0, speed, stack_cleanup=12)
        return floating([target], ecx=wrapper)
    if address == 0x006A3120:
        data = alloc(0x68)
        put32(data, 0x60, WORDS[4])
        put32(data, 0x64, WORDS[3])
        wrapper = alloc(8)
        put32(wrapper, 4, data)
        target_state = alloc(0x28)
        put32(target_state, 0x24, WORDS[2])
        target = alloc(0x3D0)
        put32(target, 0x3CC, target_state)
        return integer([target], ecx=wrapper)
    if address == 0x006A82F0:
        data = alloc(0x150)
        putf(data, 0x44, VALUES[0])
        putf(data, 0x48, VALUES[1])
        put32(data, 0x84, WORDS[4])
        putf(data, 0x94, 0.0)
        wrapper = alloc(8)
        put32(wrapper, 4, data)
        template_argument = wrapper if WORDS[0] != 0 else 0
        put32(logic, 0x58, WORDS[2])
        emu.bind_function(0x006863F0, lambda _: WORDS[3])
        output = emu.alloc(b"\xA5" * 0x78)
        ceil_stub = emu.alloc(b"\xDD\x44\x24\x04\xC3")
        emu.write(0x00BBA4DC, struct.pack("<I", ceil_stub))
        result = emu.call(
            address, [template_argument, WORDS[1]], ecx=output)
        if result.eax != output:
            raise AssertionError("006A82F0 this return")
        if result.stack_cleanup != 8:
            raise AssertionError("006A82F0 stack cleanup")
        layout = bytearray(emu.read(output, 0x78))
        pointer = struct.unpack_from("<I", layout, 4)[0]
        if pointer not in (0, wrapper):
            raise AssertionError("006A82F0 template pointer")
        struct.pack_into("<I", layout, 4, 1 if pointer else 0)
        if layout[0x31:0x34] != b"\xA5\xA5\xA5":
            raise AssertionError("006A82F0 touched reserved bytes")
        return "L00000008" + layout.hex()
    if address == 0x006A8530:
        data = alloc(0x10)
        putf(data, 8, VALUES[0])
        wrapper = alloc(8)
        put32(wrapper, 4, data)
        weapon = alloc(8)
        put32(weapon, 4, wrapper)
        distance = alloc(4)
        putf(distance, 0, VALUES[1])
        emu.write(
            0x00633670,
            b"\xD9\x05" + struct.pack("<I", distance) + b"\xC2\x04\x00")
        return integer([0, 0], ecx=weapon, al=True)
    if address == 0x006A8580:
        deadline_callback = emu.bind_stub(lambda _: WORDS[1])
        vtable = alloc(0x50)
        put32(vtable, 0x4C, deadline_callback)
        current_record = alloc(0x28)
        put32(current_record, 0, vtable)
        reload_record = alloc(0x28)
        put32(reload_record, 0, vtable)
        put32(reload_record, 0x24, WORDS[3])

        def state_lookup(machine: Emulator) -> int:
            return reload_record if machine.stack_arg(0) == 5 else current_record

        emu.bind_function(0x005B6200, state_lookup, stack_cleanup=4)
        state_type = alloc(8)
        put32(state_type, 4, WORDS[4])
        state = alloc(8)
        put32(state, 4, state_type if WORDS[5] != 0 else 0)
        weapon = alloc(0x70)
        put32(weapon, 0x64, state if WORDS[2] != 0 else 0)
        return floating(ecx=weapon)
    if address == 0x006A8650:
        data = alloc(0x90)
        include_filter = alloc(7 * 4)
        exclude_filter = alloc(7 * 4)
        put32(include_filter, 0, 1 if WORDS[0] != 0 else 0)
        put32(exclude_filter, 0, 1 if WORDS[1] != 0 else 0)
        put32(data, 0x8C, include_filter)
        put32(data, 0x88, exclude_filter)
        wrapper = alloc(8)
        put32(wrapper, 4, data)
        weapon = alloc(8)
        put32(weapon, 4, wrapper)
        target = alloc(4)
        emu.bind_function(
            0x004D5BD0, lambda _: WORDS[2], stack_cleanup=4)
        emu.bind_function(
            0x00688930, lambda _: WORDS[3], stack_cleanup=4)
        return integer([target], ecx=weapon, al=True)
    if address == 0x006A86C0:
        state_type = alloc(8)
        put32(state_type, 4, WORDS[1])
        state = alloc(8)
        put32(state, 4, state_type if WORDS[2] != 0 else 0)
        weapon = alloc(0x70)
        put32(weapon, 0x64, state if WORDS[0] != 0 else 0)
        return integer(ecx=weapon, al=True)
    if address == 0x006A8700:
        data = alloc(0x148)
        emu.write(data + 0x144, bytes((1 if WORDS[0] != 0 else 0,)))
        putf(data, 0x44, VALUES[4])
        putf(data, 0x48, VALUES[5])
        wrapper = alloc(8)
        put32(wrapper, 4, data)
        weapon = alloc(0x34)
        put32(weapon, 4, wrapper)
        emu.write(weapon + 0x30, bytes((1 if WORDS[1] != 0 else 0,)))
        first = alloc(0x120)
        second = alloc(0x120)
        putf(first, 0x40, VALUES[0])
        putf(second, 0x40, VALUES[1])

        def pitch_angles(machine: Emulator) -> int:
            machine.write(
                machine.stack_arg(3),
                struct.pack("<f", VALUES[2]))
            machine.write(
                machine.stack_arg(4),
                struct.pack("<f", VALUES[3]))
            return 0

        emu.bind_function(0x00982A80, pitch_angles, stack_cleanup=20)
        return integer([first, second], ecx=weapon, al=True)
    if address in (0x006A8840, 0x006A8870):
        state_type = alloc(8)
        put32(
            state_type, 4,
            WORDS[3] if address == 0x006A8870 else WORDS[1])
        state = alloc(8)
        descriptor_present = (
            WORDS[2] != 0 if address == 0x006A8870 else True)
        put32(state, 4, state_type if descriptor_present else 0)
        weapon = alloc(0x70)
        state_present = (
            WORDS[1] != 0 if address == 0x006A8870
            else WORDS[0] != 0)
        put32(weapon, 0x64, state if state_present else 0)
        put32(weapon, 0x1C, WORDS[3])
        if address == 0x006A8840:
            return integer([WORDS[2]], ecx=weapon)
        data = alloc(0x140)
        emu.write(data + 0x13A, bytes((1 if WORDS[0] != 0 else 0,)))
        wrapper = alloc(8)
        put32(wrapper, 4, data)
        put32(weapon, 4, wrapper)
        emu.write(weapon + 0x60, bytes((1 if WORDS[4] != 0 else 0,)))
        return integer(ecx=weapon, al=True)
    if address == 0x006A88F0:
        weapon = alloc(0x78)
        emu.write(weapon + 0x62, bytes((1 if WORDS[0] != 0 else 0,)))
        emu.write(weapon + 0x63, bytes((1 if WORDS[4] != 0 else 0,)))
        put32(weapon, 0x58, WORDS[1])
        for offset, value in enumerate(VALUES[:3]):
            putf(weapon, 0x4C + offset * 4, value)
        target = alloc(0xE0)
        put32(target, 0xDC, WORDS[3])
        matrix = (
            VALUES[3], 0.0, 0.0, VALUES[6],
            0.0, VALUES[4], 0.0, VALUES[7],
            0.0, 0.0, VALUES[5], 0.0,
        )
        for index, value in enumerate(matrix):
            putf(target, 8 + index * 4, value)
        output = emu.alloc(b"\xA5" * 12)
        target_argument = target if WORDS[2] != 0 else 0
        result = emu.call(
            address, [output, target_argument], ecx=weapon)
        after = emu.read(output, 12)
        valid = WORDS[0] and WORDS[2] and WORDS[1] == WORDS[3]
        if not valid and after != b"\xA5" * 12:
            raise AssertionError("006A88F0 failure path wrote output")
        writes = struct.unpack("<III", after) if valid else ()
        return encode(
            address, result.eax & 0xFF, result.stack_cleanup, writes=writes)
    if address == 0x006BD260:
        emu.bind_function(0x00686110, lambda _: WORDS[4])
        emu.bind_function(0x00686190, lambda _: WORDS[5])
        emu.bind_function(0x00686210, lambda _: WORDS[4])
        emu.bind_function(0x00686290, lambda _: WORDS[5])
        duration_range = alloc(8)
        putf(duration_range, 0, 7.0 / VALUES[6])
        putf(duration_range, 4, 3.0 / VALUES[6])
        data = alloc(0x100)
        put32(data, 0xF4, duration_range)
        wrapper = alloc(8)
        put32(wrapper, 4, data)
        weapon = alloc(0x70)
        put32(weapon, 4, wrapper)
        put32(weapon, 0x1C, WORDS[1])
        put32(weapon, 0x1C, WORDS[3])
        state = alloc(0x30)
        put32(state, 0x20, weapon)
        put32(state, 0x24, WORDS[1])
        put32(state, 0x28, WORDS[2])
        ceil_stub = emu.alloc(b"\xDD\x44\x24\x04\xC3")
        emu.write(0x00BBA4DC, struct.pack("<I", ceil_stub))
        return integer(ecx=state)
    if address == 0x006BD2C0:
        emu.bind_function(0x00686190, lambda _: WORDS[5])
        emu.bind_function(0x00686110, lambda _: WORDS[4])
        emu.bind_function(0x006A2AB0, lambda _: WORDS[1], stack_cleanup=8)
        emu.bind_function(0x006BCF30, lambda _: 0, stack_cleanup=4)
        callback = emu.bind_stub(lambda _: WORDS[7])
        vtable = alloc(0x18)
        put32(vtable, 0x14, callback)
        weapon = alloc(8)
        wrapper = alloc(8)
        put32(weapon, 4, wrapper)
        state = alloc(0x30)
        put32(state, 0, vtable)
        put32(state, 0x20, weapon)
        result = emu.call(address, ecx=state)
        writes = (
            struct.unpack("<I", emu.read(state + 0x24, 4))[0],
            struct.unpack("<I", emu.read(state + 0x28, 4))[0],
            3,
        )
        return encode(address, result.eax, result.stack_cleanup, writes=writes)
    if address == 0x006BD310:
        weapon = alloc(0x20)
        put32(weapon, 0x1C, WORDS[0])
        state = alloc(0x24)
        put32(state, 0x20, weapon)
        return integer(ecx=state)
    if address == 0x006ED440:
        emu.bind_function(0x006A8400, lambda _: 0)
        emu.bind_function(0x006BCF30, lambda _: 0, stack_cleanup=4)
        data = alloc(0x140)
        emu.write(data + 0x13F, b"\1")
        put32(data, 0x70, WORDS[2])
        wrapper = alloc(8)
        put32(wrapper, 4, data)
        weapon = alloc(0x70)
        put32(weapon, 4, wrapper)
        state = alloc(0x24)
        put32(state, 0x20, weapon)
        result = emu.call(address, ecx=state)
        return encode(
            address, result.eax, result.stack_cleanup,
            writes=(struct.unpack("<I", emu.read(weapon + 0x1C, 4))[0],))
    if address == 0x006ED4A0:
        emu.bind_function(
            0x006D1ED0, lambda _: WORDS[7], stack_cleanup=8)
        game_object = alloc(0x84)
        put32(game_object, 0x80, WORDS[0])
        owner = alloc(0x28)
        put32(owner, 0x24, game_object)
        state = alloc(0x30)
        put32(state, 0x18, owner)
        result = emu.call(address, [0], ecx=state)
        writes = (
            struct.unpack("<I", emu.read(state + 0x28, 4))[0],
            struct.unpack("<I", emu.read(game_object + 0x80, 4))[0],
        )
        return encode(address, result.eax, result.stack_cleanup, writes=writes)
    raise AssertionError(f"missing native scenario for 0x{address:08X}")


def native_alias_projection(snapshot: Path) -> str:
    emu = Emulator(capture=str(snapshot))
    weapon = emu.alloc(bytes(0x78))
    target = emu.alloc(bytes(0xE0))
    emu.write(weapon + 0x58, struct.pack("<I", 20))
    emu.write(weapon + 0x62, b"\1")
    emu.write(weapon + 0x63, b"\1")
    vector = struct.pack("<III", *(float_bits(value) for value in (2.0, 3.0, 4.0)))
    emu.write(weapon + 0x4C, vector)
    emu.write(target + 0xDC, struct.pack("<I", 20))
    matrix = (
        2.0, 0.0, 0.0, 10.0,
        0.0, 3.0, 0.0, 20.0,
        0.0, 0.0, 4.0, 30.0,
    )
    for index, value in enumerate(matrix):
        emu.write(
            target + 8 + index * 4,
            struct.pack("<f", value))
    before = emu.read(weapon, 0x78)
    result = emu.call(
        0x006A88F0, [weapon + 0x4C, target], ecx=weapon)
    after = emu.read(weapon, 0x78)
    if result.stack_cleanup != 8 or (result.eax & 0xFF) != 1:
        raise AssertionError("006A88F0 alias ABI")
    if after[:0x4C] != before[:0x4C] or after[0x58:] != before[0x58:]:
        raise AssertionError("006A88F0 in-place alias changed untouched bytes")
    values = struct.unpack("<III", emu.read(weapon + 0x4C, 12))
    return "1" + "".join(f"{value:08x}" for value in values)


def verify(executable: Path, snapshot: Path, mutation_proof: bool) -> None:
    global WORDS, VALUES, CURRENT_CONTROL_WORD
    if len(ENTRIES) != 29 or len(set(ENTRIES)) != 29:
        raise AssertionError("accepted entry set must contain 29 unique VAs")
    verify_native_identity(snapshot)
    checker = Checker(executable)
    mismatches = 0
    mutations_detected = 0
    restorations = 0
    observations = 0
    try:
        for address in ENTRIES:
            count = SCENARIO_COUNTS[address]
            entry_mutation_detected = False
            for index in range(count):
                WORDS, VALUES, CURRENT_CONTROL_WORD = scenario(
                    address, index)
                expected = native_projection(snapshot, address)
                transcribed = reference(address)
                if expected != transcribed:
                    raise AssertionError(
                        f"native/transcription mismatch 0x{address:08X} "
                        f"scenario={index}: {expected}/{transcribed}")
                actual = checker.call(address)
                observations += 1
                if actual != expected:
                    mismatches += 1
                    print(
                        f"mismatch 0x{address:08X} scenario={index}: "
                        f"game={expected} port={actual}", file=sys.stderr)
                if mutation_proof and not entry_mutation_detected:
                    mutated = checker.call(address, mutate=True)
                    if mutated != expected:
                        mutations_detected += 1
                        entry_mutation_detected = True
                        restored = checker.call(address)
                        if restored == expected:
                            restorations += 1
        native_alias = native_alias_projection(snapshot)
        port_alias = checker.alias()
        observations += 1
        if native_alias != port_alias:
            mismatches += 1
            print(
                f"mismatch 0x006A88F0 alias: "
                f"game={native_alias} port={port_alias}", file=sys.stderr)
        if mutation_proof:
            if checker.alias(mutate=True) == native_alias:
                raise AssertionError("006A88F0 alias mutation was not detected")
            if checker.alias() != native_alias:
                raise AssertionError("006A88F0 alias mutation did not restore")
    finally:
        checker.close()
    if mismatches:
        raise AssertionError(f"{mismatches} ObjectLogic mismatches")
    if mutation_proof and mutations_detected != len(ENTRIES):
        raise AssertionError(
            f"mutation sensitivity {mutations_detected}/{len(ENTRIES)}")
    if mutation_proof and restorations != len(ENTRIES):
        raise AssertionError(
            f"mutation restoration {restorations}/{len(ENTRIES)}")
    print(
        f"objectlogic weapon: entries={len(ENTRIES)} "
        f"minimum={min(SCENARIO_COUNTS.values())} "
        f"scenarios={sum(SCENARIO_COUNTS.values())} "
        f"observations={observations} mismatches=0 "
        f"mutations={mutations_detected if mutation_proof else 'not-run'} "
        f"restorations={restorations if mutation_proof else 'not-run'} "
        f"alias-mutation={'detected' if mutation_proof else 'not-run'}")


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("checker", type=Path)
    parser.add_argument(
        "--capture", type=Path, required=True,
        help="absolute run35 snapshot directory")
    parser.add_argument("--mutation-proof", action="store_true")
    args = parser.parse_args()
    if not args.capture.is_absolute():
        parser.error("--capture must be absolute")
    verify(args.checker, args.capture, args.mutation_proof)


if __name__ == "__main__":
    main()
