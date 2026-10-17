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
import struct
import subprocess
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from emulate import Emulator  # noqa: E402


ENTRIES = (
    0x00685CC0, 0x00685D20, 0x00685DA0, 0x00685DC0, 0x00685DD0,
    0x00685E70, 0x00685EA0, 0x00685EF0, 0x00685F70, 0x00685FF0,
    0x00686070, 0x00686110, 0x00686190, 0x00686210, 0x00686290,
    0x00686310, 0x00686380, 0x006863F0, 0x006A2400, 0x006A2430,
    0x006A2440, 0x006A2480, 0x006A24A0, 0x006A24F0, 0x006A2560,
    0x006A25F0, 0x006A2630, 0x006A2840, 0x006A2A70, 0x006A2AB0,
    0x006A2B40, 0x006A2BD0, 0x006A2D30, 0x006A2DC0, 0x006A2EE0,
    0x006A3120, 0x006A82F0, 0x006A8530, 0x006A8580, 0x006A8650,
    0x006A86C0, 0x006A8700, 0x006A8840, 0x006A8870, 0x006A88F0,
    0x006BD260, 0x006BD2C0, 0x006BD310, 0x006ED440, 0x006ED4A0,
)

FLOAT_ENTRIES = {
    0x00685DC0, 0x00685DD0, 0x00685E70, 0x00685EA0, 0x006A2630,
    0x006A2D30, 0x006A2DC0, 0x006A8580,
}

# sha256(concatenation of 16 native bytes at every ENTRIES address), from the
# immutable run35 snapshot named below.
NATIVE_PREFIX_SHA256 = (
    "3ee865788019df328ffccadded6836bf0dc66deeabba16454598ebb38b2f9d31")

WORDS = (100, 90, 20, 1, 7, 3, 0x16, 5)
VALUES = (2.0, 3.0, 4.0, 5.0, 6.0, 7.0, 30.0, 1.0)


class Checker:
    def __init__(self, executable: Path) -> None:
        self.process = subprocess.Popen(
            [str(executable)], stdin=subprocess.PIPE, stdout=subprocess.PIPE,
            text=True, bufsize=1)

    def call(self, address: int, mutate: bool = False) -> str:
        assert self.process.stdin is not None and self.process.stdout is not None
        values = [float_bits(value) for value in VALUES]
        command = " ".join([
            "mutate" if mutate else "probe", f"{address:08x}",
            *(f"{value:08x}" for value in WORDS),
            *(f"{value:08x}" for value in values),
        ])
        self.process.stdin.write(command + "\n")
        self.process.stdin.flush()
        result = self.process.stdout.readline().strip()
        if not result or result == "error":
            raise RuntimeError(f"objectlogiccheck rejected {command!r}")
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


def frames(seconds: float) -> int:
    product = f32(f32(seconds * VALUES[6]) * VALUES[7])
    return u32(math.ceil(product))


def encode(address: int, eax: int = 0, cleanup: int = 0, st0: float | None = None,
           writes: tuple[int, ...] = ()) -> str:
    padded = tuple(u32(value) for value in writes) + (0,) * (4 - len(writes))
    write_mask = (1 << len(writes)) - 1
    st0_bits = 0 if st0 is None else float_bits(st0)
    fields = (u32(eax), 0, cleanup, st0_bits, write_mask, *padded)
    result = "".join(f"{value:08x}" for value in fields)
    result += (ext80(st0_bits) if address in FLOAT_ENTRIES else bytes(10)).hex()
    return result


def reference(address: int) -> str:
    w = WORDS
    v = VALUES
    deadline = u32(w[1] + w[2])
    if address == 0x00685CC0:
        return encode(address, 0xFFFFFFFF)
    if address == 0x00685D20:
        return encode(address, 0xFFFFFFFF)
    if address == 0x00685DA0:
        return encode(address, 0xFFFFFFFF)
    if address == 0x00685DC0:
        return encode(address, cleanup=8, st0=0.0)
    if address in (0x00685DD0, 0x006A2DC0):
        return encode(address, cleanup=4, st0=0.0)
    if address == 0x00685E70:
        return encode(address, st0=0.0)
    if address == 0x00685EA0:
        return encode(address, st0=v[0])
    if address in (0x00685EF0, 0x00685FF0, 0x00686110, 0x00686210,
                   0x00686310, 0x00686380, 0x006863F0):
        return encode(address, frames(v[0]))
    if address in (0x00685F70, 0x00686070, 0x00686190, 0x00686290):
        return encode(address, frames(v[1]))
    if address == 0x006A2400:
        return encode(address, w[0], 4, writes=(min(w[2], 5),))
    if address == 0x006A2430:
        return encode(address, w[w[7] % 6], 4)
    if address == 0x006A2440:
        return encode(address, 0)
    if address == 0x006A2480:
        return encode(address, w[1])
    if address == 0x006A24A0:
        first = 0
        second = 0
        for value in w[:6]:
            first |= value
            second |= u32(value << 1)
        return encode(address, w[5] << 1, writes=(first, second))
    if address == 0x006A24F0:
        return encode(
            address, w[7], cleanup=4, writes=tuple(map(float_bits, v[:3])))
    if address == 0x006A2560:
        count = sum(value & 0xFF for value in w[:6] if value & w[6])
        return encode(address, count, 8)
    if address == 0x006A25F0:
        return encode(address, 0, 8)
    if address == 0x006A2630:
        return encode(address, cleanup=12, st0=v[1])
    if address == 0x006A2840:
        return encode(address, deadline)
    if address == 0x006A2A70:
        return encode(address, w[0], 4)
    if address == 0x006A2AB0:
        return encode(address, 45, 8)
    if address == 0x006A2B40:
        return encode(address)
    if address == 0x006A2BD0:
        return encode(address, 0xFFFFFFFF, writes=(float_bits(0.5),))
    if address == 0x006A2D30:
        return encode(address, cleanup=4, st0=f32(v[0] * v[1]))
    if address == 0x006A2EE0:
        kind = 1 if w[1] == 2 else (3 if w[1] else 2)
        return encode(address, int(bool(w[0] & (1 << kind))), 12)
    if address == 0x006A3120:
        return encode(address, w[4], 4)
    if address == 0x006A82F0:
        return encode(
            address, w[7], 8, writes=(w[0], w[1], 0x7FFFFFFF, 0xFFFFFFFF))
    if address == 0x006A8530:
        return encode(address, 0, 8)
    if address == 0x006A8580:
        return encode(address, st0=0.0)
    if address == 0x006A8650:
        return encode(address, 1, 4)
    if address == 0x006A86C0:
        return encode(address, 0)
    if address == 0x006A8700:
        return encode(address, 1, 8)
    if address == 0x006A8840:
        return encode(address, w[3], 4)
    if address == 0x006A8870:
        return encode(address, 1)
    if address == 0x006A88F0:
        return encode(address, 0, 8)
    if address == 0x006BD260:
        return encode(address, 115)
    if address == 0x006BD2C0:
        return encode(address, w[7], writes=(w[0], w[1], 3))
    if address == 0x006BD310:
        return encode(address, 0xFFFFFFFE)
    if address == 0x006ED440:
        return encode(address, 0xFFFFFFFF, writes=(w[2],))
    if address == 0x006ED4A0:
        return encode(address, cleanup=4, writes=(0, w[0] & ~0x4000))
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


def verify(executable: Path, snapshot: Path, mutation_proof: bool) -> None:
    if len(ENTRIES) != 50 or len(set(ENTRIES)) != 50:
        raise AssertionError("accepted entry set must contain 50 unique VAs")
    verify_native_identity(snapshot)
    checker = Checker(executable)
    mismatches = 0
    mutations_detected = 0
    try:
        for address in ENTRIES:
            expected = reference(address)
            actual = checker.call(address)
            if actual != expected:
                mismatches += 1
                print(
                    f"mismatch 0x{address:08X}: "
                    f"game={expected} port={actual}", file=sys.stderr)
            if mutation_proof:
                mutated = checker.call(address, mutate=True)
                if mutated != expected:
                    mutations_detected += 1
    finally:
        checker.close()
    if mismatches:
        raise AssertionError(f"{mismatches} ObjectLogic mismatches")
    if mutation_proof and mutations_detected != len(ENTRIES):
        raise AssertionError(
            f"mutation sensitivity {mutations_detected}/{len(ENTRIES)}")
    print(
        f"objectlogic weapon: entries={len(ENTRIES)} "
        f"observations={len(ENTRIES)} mismatches=0 "
        f"mutations={mutations_detected if mutation_proof else 'not-run'}")


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
