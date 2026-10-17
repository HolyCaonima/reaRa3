"""Release oracle for the display-extent leaves against ra3_1.0.game.

Four leaf cases cover every accessor.  Six predicate cases, one aliased
triple write, and one quad write cover the comparison and the ordered
copies.  One mutant flips the predicate and must disagree with the image.
"""

from __future__ import annotations

import struct
import subprocess
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from emulate import Emulator  # noqa: E402


OBJECT_BYTES = 0x130
LOAD_PRIMARY = 0x0052AF80
STORE_PRIMARY = 0x005D5B20
LOAD_SECONDARY = 0x00740FA0
STORE_SECONDARY = 0x00560DB0
LOAD_FLAG_5E = 0x0052AF90
ASPECT = 0x0052AFA0
STORE_FLAG_5F = 0x0052AFC0
LOAD_FLAG_5F = 0x0052AFD0
STORE_FLAG_60 = 0x0052AFE0
LOAD_FLAG_60 = 0x00ADA7C0
STORE_TRIPLE = 0x0052AF60
STORE_PAIR = 0x0051FB90
LOAD_FIELD_74 = 0x0051FBB0
STORE_FIELD_CC = 0x0052B000
LOAD_FLAG_12C = 0x0058C900
STORE_FLAG_12C = 0x0058C910
LOAD_FIELD_110 = 0x0058C920
STORE_FIELD_110 = 0x0058C930
STORE_QUAD = 0x00560800
LOAD_FIELD_118 = 0x00560A10

ASPECT_CASES = (
    (5, 3, 1),
    (4, 3, 0),
    (3, 3, 0),
    (1, 0, 1),
    (0x55555556, 1, 0),
    (1, 0x40000000, 1),
)


class Checker:
    def __init__(self, executable: Path) -> None:
        self.process = subprocess.Popen(
            [str(executable)], stdin=subprocess.PIPE, stdout=subprocess.PIPE,
            text=True, bufsize=1)

    def call(self, line: str) -> str:
        assert self.process.stdin is not None and self.process.stdout is not None
        self.process.stdin.write(line + "\n")
        self.process.stdin.flush()
        result = self.process.stdout.readline().strip()
        if not result or result == "error":
            raise RuntimeError(f"displaycheck rejected {line[:80]!r}")
        return result

    def close(self) -> None:
        assert self.process.stdin is not None
        self.process.stdin.write("quit\n")
        self.process.stdin.flush()
        if self.process.wait(timeout=10) != 0:
            raise RuntimeError("displaycheck failed")


def hex_object(data: bytes) -> str:
    return data.hex()


def put_dword(data: bytearray, offset: int, value: int) -> None:
    data[offset:offset + 4] = struct.pack("<I", value & 0xFFFFFFFF)


def require(condition: bool, message: str) -> None:
    if not condition:
        raise RuntimeError(message)


def verify(executable: Path) -> None:
    emu = Emulator()
    checker = Checker(executable)

    def native(address: int, args: list[int], ecx: int, eax: int = 0):
        return emu.call(address, args, ecx=ecx, eax=eax)

    # Leaf 1: dword loads, including a zero and a high bit.
    loads = bytearray(b"\xA5" * OBJECT_BYTES)
    put_dword(loads, 0x4C, 0xFFFFFFFF)
    put_dword(loads, 0x50, 0x00000000)
    put_dword(loads, 0x74, 0x11223344)
    put_dword(loads, 0x110, 0x80000000)
    put_dword(loads, 0x118, 0x00DF445C)
    address = emu.alloc(bytes(loads))
    expected_loads = []
    for function, cleanup in (
        (LOAD_PRIMARY, 0), (LOAD_SECONDARY, 0), (LOAD_FIELD_74, 0),
        (LOAD_FIELD_110, 0), (LOAD_FIELD_118, 0),
    ):
        result = native(function, [], address)
        require(result.stack_cleanup == cleanup, f"cleanup {function:#x}")
        expected_loads.append(result.eax)
    got = [int(part, 16) for part in checker.call(f"loads {hex_object(loads)}").split()]
    require(got == expected_loads, f"dword loads {got} != {expected_loads}")

    # Leaf 2: byte loads keep EAX[31:8].
    flags = bytearray(b"\xA5" * OBJECT_BYTES)
    flags[0x5E] = 0x3C
    flags[0x5F] = 0x00
    flags[0x60] = 0xFF
    flags[0x12C] = 0x7F
    incoming = 0xA5A5A500
    address = emu.alloc(bytes(flags))
    expected_flags = []
    for function in (LOAD_FLAG_5E, LOAD_FLAG_5F, LOAD_FLAG_60, LOAD_FLAG_12C):
        result = native(function, [], address, incoming)
        require(result.stack_cleanup == 0, f"cleanup {function:#x}")
        expected_flags.append(result.eax)
    got = [int(part, 16) for part in checker.call(
        f"byteloads {incoming:#x} {hex_object(flags)}").split()]
    require(got == expected_flags, f"byte loads {got} != {expected_flags}")

    # Leaf 3: dword stores, including the two-argument pair.  Neighbors stay.
    stored = bytearray(b"\x5A" * OBJECT_BYTES)
    values = (0xFFFFFFFF, 0x00000001, 0x80000000, 0x11223344, 0x01020304, 0x0A0B0C0D)
    address = emu.alloc(bytes(stored))
    native_eaxes = []
    steps = (
        (STORE_PRIMARY, [values[0]], 4),
        (STORE_SECONDARY, [values[1]], 4),
        (STORE_FIELD_CC, [values[2]], 4),
        (STORE_FIELD_110, [values[3]], 4),
        (STORE_PAIR, [values[4], values[5]], 8),
    )
    for function, args, cleanup in steps:
        result = native(function, list(args), address)
        require(result.stack_cleanup == cleanup, f"cleanup {function:#x}")
        native_eaxes.append(result.eax)
    native_image = emu.read(address, OBJECT_BYTES)
    text = checker.call(
        "dwordstores " + " ".join(f"{value:#x}" for value in values)
        + " " + hex_object(stored))
    parts = text.split()
    require([int(part, 16) for part in parts[:5]] == native_eaxes, "store eax")
    require(parts[5] == native_image.hex(), "store image")

    # Leaf 4: byte stores write one byte and keep EAX[31:8].
    byte_image = bytearray(b"\x11" * OBJECT_BYTES)
    byte_args = (0xFFFFFF3C, 0x00000000, 0xABCD007F)
    incoming = 0xA5A5A500
    address = emu.alloc(bytes(byte_image))
    native_eaxes = []
    for function, argument in (
        (STORE_FLAG_5F, byte_args[0]),
        (STORE_FLAG_60, byte_args[1]),
        (STORE_FLAG_12C, byte_args[2]),
    ):
        result = native(function, [argument], address, incoming)
        require(result.stack_cleanup == 4, f"cleanup {function:#x}")
        native_eaxes.append(result.eax)
    native_image = emu.read(address, OBJECT_BYTES)
    text = checker.call(
        f"bytestores {incoming:#x} "
        + " ".join(f"{value:#x}" for value in byte_args)
        + " " + hex_object(byte_image))
    parts = text.split()
    require([int(part, 16) for part in parts[:3]] == native_eaxes, "byte store eax")
    require(parts[3] == native_image.hex(), "byte store image")

    # Predicate cases.  Expectations are the shipped results, not the C++ expression.
    for primary, secondary, expected in ASPECT_CASES:
        image = bytearray(OBJECT_BYTES)
        put_dword(image, 0x4C, primary)
        put_dword(image, 0x50, secondary)
        address = emu.alloc(bytes(image))
        result = native(ASPECT, [], address)
        require(result.stack_cleanup == 0 and result.eax == expected,
                f"native aspect {primary:#x},{secondary:#x} -> {result.eax:#x}")
        got = int(checker.call(f"aspect {hex_object(image)}"), 16)
        require(got == expected, f"aspect {primary:#x},{secondary:#x}")

    # Aliased triple: source begins four bytes before the destination.
    alias = bytearray(b"\xA5" * OBJECT_BYTES)
    put_dword(alias, 0x60, 0x11111111)
    put_dword(alias, 0x64, 0x22222222)
    put_dword(alias, 0x68, 0x33333333)
    put_dword(alias, 0x6C, 0x44444444)
    address = emu.alloc(bytes(alias))
    result = native(STORE_TRIPLE, [address + 0x60], address)
    require(result.stack_cleanup == 4 and result.eax == 0x11111111, "native triple")
    native_image = emu.read(address, OBJECT_BYTES)
    text = checker.call(f"triplealias {hex_object(alias)}")
    eax_text, image_text = text.split()
    require(int(eax_text, 16) == result.eax and image_text == native_image.hex(),
            "triple alias")

    # Quad copy plus the one-byte flag, leaving the next byte alone.
    quad = bytearray(b"\xA5" * OBJECT_BYTES)
    quad[0x12D] = 0xAB
    source = emu.alloc(struct.pack("<IIII", 1, 2, 3, 4))
    address = emu.alloc(bytes(quad))
    result = native(STORE_QUAD, [source], address)
    require(result.stack_cleanup == 4 and result.eax == 4, "native quad")
    native_image = emu.read(address, OBJECT_BYTES)
    text = checker.call(f"quad 1 2 3 4 {hex_object(quad)}")
    eax_text, image_text = text.split()
    require(int(eax_text, 16) == 4 and image_text == native_image.hex(), "quad")
    require(bytes.fromhex(image_text)[0x12C] == 1 and bytes.fromhex(image_text)[0x12D] == 0xAB,
            "quad flag")

    # The equal 4*secondary == 3*primary case is 0 in the image and 1 in the mutant.
    equal = bytearray(OBJECT_BYTES)
    put_dword(equal, 0x4C, 4)
    put_dword(equal, 0x50, 3)
    address = emu.alloc(bytes(equal))
    shipped = native(ASPECT, [], address).eax
    restored = int(checker.call(f"aspect {hex_object(equal)}"), 16)
    mutant = int(checker.call(f"aspectmutant {hex_object(equal)}"), 16)
    require(shipped == 0 and restored == shipped and mutant == 1 and mutant != shipped,
            "semantic mutant did not disagree")

    checker.close()
    print("display extent: 20 leaves, 4 load/store cases, 6 predicate cases,")
    print("  1 aliased triple, 1 quad flag write;")
    print("  deliberate semantic mutant: detected against shipped behavior")


if __name__ == "__main__":
    if len(sys.argv) != 2:
        raise SystemExit(f"usage: {sys.argv[0]} ra3_displaycheck.exe")
    verify(Path(sys.argv[1]))
