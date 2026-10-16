"""Differentially verify ports2 math and CRC functions against ra3_1.0.game."""

from __future__ import annotations

import argparse
import random
import struct
import subprocess
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from emulate import Emulator  # noqa: E402


RANDOM_CASES = 2000
CONTROL_WORDS = (0x007F, 0x027F, 0x037F)
ADDRESSES = {
    "maximum": 0x00504620,
    "clamp": 0x00504650,
    "equal3": 0x00504680,
    "equal2range": 0x005046C0,
    "distance2": 0x005049E0,
    "minimum": 0x00522A30,
    "approximatelength": 0x00523240,
    "colorargb": 0x005235B0,
    "contains2": 0x00528BC0,
    "separate2": 0x00528C00,
    "clampoffset68": 0x0055A9B0,
    "distance": 0x00564150,
    "normalize2": 0x0056F370,
    "offsetdistance2": 0x006ABCB0,
    "scale3": 0x009814E0,
    "crc32": 0x0092A0D0,
    "transform3": 0x00994A10,
}
SENSITIVE = {
    "distance2", "distance", "approximatelength", "normalize2", "offsetdistance2"
}


def pack32(*values: int) -> bytes:
    return struct.pack("<" + "I" * len(values), *values)


def token(data: bytes) -> str:
    return data.hex() if data else "-"


class CppOracle:
    def __init__(self, executable: Path) -> None:
        self.process = subprocess.Popen(
            [str(executable)], stdin=subprocess.PIPE, stdout=subprocess.PIPE,
            text=True, bufsize=1)

    def call(self, command: str) -> str:
        assert self.process.stdin is not None and self.process.stdout is not None
        self.process.stdin.write(command + "\n")
        self.process.stdin.flush()
        result = self.process.stdout.readline().strip()
        if not result or result == "error":
            raise RuntimeError(f"portcheck rejected {command!r}")
        return result

    def set_control_word(self, value: int) -> None:
        if self.call(f"fpcw {value}") != "ok":
            raise RuntimeError("portcheck rejected FPU control word")

    def close(self) -> None:
        assert self.process.stdin is not None
        self.process.stdin.write("quit\n")
        self.process.stdin.flush()
        if self.process.wait(timeout=10) != 0:
            raise RuntimeError("portcheck failed")


def checked_float(emu: Emulator, name: str, args: list[int], cw: int, **registers) -> int:
    result, bits = emu.call_float32(ADDRESSES[name], args, fpu_control_word=cw, **registers)
    if result.stack_cleanup != 0:
        raise AssertionError(f"{name}: unexpected cleanup {result.stack_cleanup}")
    return bits


def checked_extended(
        emu: Emulator, name: str, args: list[int], cw: int,
        expected_cleanup: int = 0, **registers) -> bytes:
    result, value = emu.call_float80(
        ADDRESSES[name], args, fpu_control_word=cw, **registers)
    if result.stack_cleanup != expected_cleanup:
        raise AssertionError(f"{name}: unexpected cleanup {result.stack_cleanup}")
    return value


def verify(executable: Path) -> None:
    rng = random.Random(0x504F525432)
    emu = Emulator()
    cpp = CppOracle(executable)
    totals: dict[tuple[str, int], int] = {}
    edges = [
        0x00000000, 0x80000000, 0x00000001, 0x80000001,
        0x007FFFFF, 0x807FFFFF, 0x00800000, 0x80800000,
        0x3F800000, 0xBF800000, 0x7F7FFFFF, 0xFF7FFFFF,
        0x7F800000, 0xFF800000, 0x7FC00000, 0x7FC00001, 0xFFC12345,
    ]
    def random_finite_bits() -> int:
        bits = rng.getrandbits(32)
        if bits & 0x7F800000 == 0x7F800000:
            bits ^= 0x00800000
        return bits

    cases = [tuple(random_finite_bits() for _ in range(9)) for _ in range(RANDOM_CASES)]
    finite_edges = edges[:12]
    extended_edges = [
        tuple(finite_edges[(i + j) % len(finite_edges)] for j in range(9))
        for i in range(len(finite_edges))
    ]
    extended_edges.extend([
        (0x7F7FFFFF, 0x7F7FFFFF, 0x7F7FFFFF,
         0xFF7FFFFF, 0xFF7FFFFF, 0xFF7FFFFF, 0, 0, 0),
        (0x00000001, 0x80000001, 0x00000001,
         0x80000001, 0x00000001, 0x80000001, 0, 0, 0),
    ])
    cases.extend(extended_edges)
    extended_case_count = len(cases)
    cases.extend(tuple(edges[(i + j) % len(edges)] for j in range(9))
                 for i in range(len(edges)))

    def check(name: str, cw: int, game, ours, detail: str) -> None:
        if game != ours:
            raise AssertionError(
                f"{name}/cw=0x{cw:04X}: game={game!r} ours={ours!r} {detail}")
        totals[(name, cw)] = totals.get((name, cw), 0) + 1

    for cw in CONTROL_WORDS:
        cpp.set_control_word(cw)
        for index, values in enumerate(cases):
            a3_data = pack32(*values[:3])
            b3_data = pack32(*values[3:6])
            a2_data = a3_data[:8]
            b2_data = b3_data[:8]
            point_data = pack32(values[6], values[7])

            if cw == CONTROL_WORDS[0]:
                for name, args in (
                    ("maximum", [values[0], values[1]]),
                    ("minimum", [values[0], values[1]]),
                    ("clamp", [values[0], values[1], values[2]]),
                ):
                    game = checked_float(emu, name, args, cw)
                    command = f"{name} " + " ".join(str(value) for value in args)
                    check(name, cw, game, int(cpp.call(command)), f"values={args}")

                emu.reset_data()
                a3 = emu.alloc(a3_data)
                b3 = emu.alloc(b3_data)
                game = emu.call(ADDRESSES["equal3"], [a3, b3]).eax
                check("equal3", cw, game, int(cpp.call(
                    f"equal3 {token(a3_data)} {token(b3_data)}")), f"index={index}")

                count = index % 5
                left = b"".join(pack32(
                    values[(item * 2) % 9], values[(item * 2 + 1) % 9])
                    for item in range(count))
                right = bytearray(left)
                if count != 0 and index % 3 == 0:
                    right[(index % len(right))] ^= 1
                emu.reset_data()
                left_address = emu.alloc(left)
                right_address = emu.alloc(bytes(right))
                game = emu.call(
                    ADDRESSES["equal2range"],
                    [left_address, left_address + len(left), right_address]).eax & 0xFF
                check("equal2range", cw, game, int(cpp.call(
                    f"equal2range {token(left)} {token(bytes(right))}")),
                    f"count={count}")

                emu.reset_data()
                output = emu.alloc(b"\xA5" * 16)
                game_result = emu.call(ADDRESSES["colorargb"], [output, values[0]])
                assert game_result.stack_cleanup == 0
                assert game_result.eax == output
                check("colorargb", cw, emu.read(output, 16), bytes.fromhex(cpp.call(
                    f"colorargb {values[0]}")), f"argb=0x{values[0]:08X}")

                emu.reset_data()
                point = emu.alloc(point_data)
                minimum = emu.alloc(a2_data)
                maximum = emu.alloc(b2_data)
                game = emu.call(
                    ADDRESSES["contains2"], [point, minimum, maximum]).eax & 0xFF
                check("contains2", cw, game, int(cpp.call(
                    f"contains2 {token(point_data)} {token(a2_data)} {token(b2_data)}")),
                    f"index={index}")

                emu.reset_data()
                a2 = emu.alloc(a2_data)
                b2 = emu.alloc(b2_data)
                emu.call(ADDRESSES["separate2"], [a2, b2, values[8]])
                game = emu.read(a2, 8) + emu.read(b2, 8)
                check("separate2", cw, game, bytes.fromhex(cpp.call(
                    f"separate2 {token(a2_data)} {token(b2_data)} {values[8]}")),
                    f"index={index}")

                emu.reset_data()
                obj = bytearray(0x28)
                obj[0x10:0x1C] = a3_data
                obj[0x24:0x28] = pack32(values[8])
                obj_address = emu.alloc(bytes(obj))
                output = emu.alloc(b"\xA5" * 12)
                result = emu.call(ADDRESSES["scale3"], [output], ecx=obj_address)
                assert result.stack_cleanup == 4
                assert result.eax == output
                check("scale3", cw, emu.read(output, 12), bytes.fromhex(cpp.call(
                    f"scale3 {token(a3_data)} {values[8]}")), f"index={index}")

                if index < RANDOM_CASES:
                    matrix_data = pack32(*values[:9], values[0], values[1], values[2])
                    emu.reset_data()
                    matrix = emu.alloc(matrix_data)
                    input_address = emu.alloc(b3_data)
                    output = emu.alloc(b"\xA5" * 12)
                    result = emu.call(
                        ADDRESSES["transform3"], [output, input_address], ecx=matrix)
                    assert result.stack_cleanup == 8
                    assert result.eax == output
                    check("transform3", cw, emu.read(output, 12), bytes.fromhex(cpp.call(
                        f"transform3 {token(matrix_data)} {token(b3_data)}")), f"index={index}")

                emu.reset_data()
                obj = bytearray(0x6C)
                obj[0x68:0x6C] = pack32(values[0])
                obj_address = emu.alloc(bytes(obj))
                result = emu.call(
                    ADDRESSES["clampoffset68"], [values[1], values[2]], ecx=obj_address)
                assert result.stack_cleanup == 8
                check("clampoffset68", cw, emu.read(obj_address + 0x68, 4),
                      bytes.fromhex(cpp.call(
                          f"clampoffset68 {token(pack32(values[0]))} "
                          f"{values[1]} {values[2]}")), f"index={index}")

            if index < extended_case_count:
                for name in ("distance2", "distance"):
                    emu.reset_data()
                    a3 = emu.alloc(a3_data)
                    b3 = emu.alloc(b3_data)
                    game = checked_extended(emu, name, [a3, b3], cw)
                    ours = bytes.fromhex(cpp.call(
                        f"{name}x87 {token(a3_data)} {token(b3_data)}"))
                    check(name, cw, game, ours, f"index={index}")

                emu.reset_data()
                a3 = emu.alloc(a3_data)
                game = checked_extended(emu, "approximatelength", [a3], cw)
                check("approximatelength", cw, game, bytes.fromhex(cpp.call(
                    f"approximatelengthx87 {token(a3_data)}")), f"index={index}")

                emu.reset_data()
                output = emu.alloc(b"\xA5" * 8)
                input_address = emu.alloc(a2_data)
                assert emu.call(ADDRESSES["normalize2"], [output, input_address]).eax == output
                check("normalize2", cw, emu.read(output, 8), bytes.fromhex(cpp.call(
                    f"normalize2 {token(a2_data)}")), f"index={index}")

                emu.reset_data()
                obj = bytearray(0x44)
                obj[0x38:0x44] = a3_data
                obj_address = emu.alloc(bytes(obj))
                point = emu.alloc(b3_data)
                game = checked_extended(
                    emu, "offsetdistance2", [point], cw,
                    expected_cleanup=4, ecx=obj_address)
                check("offsetdistance2", cw, game, bytes.fromhex(cpp.call(
                    f"offsetdistance2x87 {token(bytes(obj))} {token(b3_data)}")),
                    f"index={index}")

    cpp.set_control_word(CONTROL_WORDS[-1])
    crc_cases = [b"\0", b"123456789\0", b"\x80\xff\0"]
    crc_cases.extend(bytes(rng.randrange(1, 256) for _ in range(rng.randrange(65))) + b"\0"
                     for _ in range(RANDOM_CASES))
    for data in crc_cases:
        emu.reset_data()
        address = emu.alloc(data)
        game = emu.call(ADDRESSES["crc32"], [address]).eax
        check("crc32", CONTROL_WORDS[-1], game, int(cpp.call(
            f"crc32 {token(data)}")), f"data={data.hex()}")
    emu.reset_data()
    game = emu.call(ADDRESSES["crc32"], [0]).eax
    check("crc32", CONTROL_WORDS[-1], game, int(cpp.call("crc32 -")), "null")

    cpp.close()
    for (name, cw), count in sorted(totals.items()):
        suffix = f" cw=0x{cw:04X}" if name in SENSITIVE else ""
        print(f"{name}{suffix}: {count}/{count} matched, mismatches=0")


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument(
        "--portcheck", type=Path,
        default=Path(__file__).resolve().parents[3]
        / "build/win32/bin/Debug/ra3_portcheck.exe")
    args = parser.parse_args()
    verify(args.portcheck.resolve())


if __name__ == "__main__":
    main()
