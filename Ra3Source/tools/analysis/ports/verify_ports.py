"""Differentially verify common byte/string ports against ra3_1.0.game."""

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
FILL = 0xA5

TARGETS = {
    "copylim": (0x009F3470, 0),
    "appendlim": (0x009F34B0, 0),
    "ncmp": (0x009F3500, 0),
    "nicmp": (0x009F3560, 0),
    "copy": (0x009F35E0, 0),
    "cmp": (0x009F3610, 0),
    "len": (0x009F36C0, 0),
    "lower": (0x009F36E0, 0),
    "rchr": (0x009F3720, 0),
    "widen": (0x009F3F10, 0),
    "bytes": (0x004C9EA0, 12),
    "pcstr": (0x004CB3A0, 4),
    "pp": (0x004CB400, 4),
    "fnv": (0x00A28290, 0),
    "decode": (0x00A282D0, 0),
    "encode": (0x00A28350, 0),
    "decodeadv": (0x00A284B0, 0),
}


def signed32(value: int) -> int:
    return value if value < 0x80000000 else value - 0x100000000


def token(data: bytes | None) -> str:
    return "-" if data is None else data.hex()


def random_cstring(rng: random.Random, maximum: int = 48) -> bytes:
    size = rng.randrange(maximum + 1)
    return bytes(rng.randrange(1, 256) for _ in range(size)) + b"\0"


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

    def close(self) -> None:
        if self.process.stdin is not None:
            self.process.stdin.write("quit\n")
            self.process.stdin.flush()
        if self.process.wait(timeout=10) != 0:
            raise RuntimeError("portcheck failed")


def checked_call(emu: Emulator, name: str, args: list[int], **registers):
    result = emu.call(TARGETS[name][0], args, **registers)
    if result.stack_cleanup != TARGETS[name][1]:
        raise AssertionError(
            f"{name}: cleanup {result.stack_cleanup}, expected {TARGETS[name][1]}")
    return result


def verify(executable: Path) -> None:
    rng = random.Random(0x504F525431)
    emu = Emulator()
    cpp = CppOracle(executable)
    totals = {name: [0, 0] for name in TARGETS}

    def check(name: str, game, ours, detail: str) -> None:
        totals[name][0] += 1
        if game != ours:
            totals[name][1] += 1
            raise AssertionError(f"{name}: game={game!r} ours={ours!r} {detail}")

    string_edges = [
        b"\0", b"a\0", b"A_Z\0", b"\x7f\0", b"\x80\0", b"\xff\0",
        b"0123456789\0",
    ]

    for index in range(RANDOM_CASES + len(string_edges)):
        source = random_cstring(rng) if index < RANDOM_CASES else string_edges[index - RANDOM_CASES]
        count = rng.randrange(0, 64) if index < RANDOM_CASES else index - RANDOM_CASES
        emu.reset_data()
        src = emu.alloc(source)
        output_size = count + 2
        destination = emu.alloc(bytes([FILL]) * output_size)
        result = checked_call(emu, "copylim", [destination, src, count])
        game = (result.eax - destination, emu.read(destination, output_size))
        ours_bytes = bytes.fromhex(cpp.call(f"copylim {token(source)} {count}"))
        check("copylim", game, (0, ours_bytes), f"source={source.hex()} count={count}")

        initial = random_cstring(rng)
        emu.reset_data()
        initial_buffer = initial + bytes([FILL]) * (count + 1)
        destination = emu.alloc(initial_buffer)
        src = emu.alloc(source)
        result = checked_call(emu, "appendlim", [destination, src, count])
        game = (result.eax - destination, emu.read(destination, len(initial_buffer)))
        ours_bytes = bytes.fromhex(
            cpp.call(f"appendlim {token(initial)} {token(source)} {count}"))
        check("appendlim", game, (0, ours_bytes),
              f"initial={initial.hex()} source={source.hex()} count={count}")

        other = random_cstring(rng)
        for name in ("ncmp", "nicmp"):
            emu.reset_data()
            a = emu.alloc(source)
            b = emu.alloc(other)
            game = signed32(checked_call(emu, name, [a, b, count]).eax)
            ours = int(cpp.call(f"{name} {token(source)} {token(other)} {count}"))
            check(name, game, ours, f"a={source.hex()} b={other.hex()} count={count}")

    # Explicit null-pointer cases whose count-zero machine path does not dereference.
    for name, args, command in (
        ("copylim", [0, 0, 0], "copylim - 0"),
        ("ncmp", [0, 0, 0], "ncmp - - 0"),
        ("nicmp", [0, 0, 0], "nicmp - - 0"),
    ):
        emu.reset_data()
        result = checked_call(emu, name, args)
        ours = cpp.call(command)
        game = result.eax if name == "copylim" else signed32(result.eax)
        ours_value = 0 if name == "copylim" else int(ours)
        check(name, game, ours_value, "null/count=0")

    for index in range(RANDOM_CASES + len(string_edges)):
        text = random_cstring(rng) if index < RANDOM_CASES else string_edges[index - RANDOM_CASES]
        other = random_cstring(rng)

        for name in ("copy", "lower"):
            emu.reset_data()
            source = emu.alloc(text)
            size = len(text) + 2
            destination = emu.alloc(bytes([FILL]) * size)
            result = checked_call(emu, name, [destination, source])
            game = (result.eax - destination, emu.read(destination, size))
            ours = bytes.fromhex(cpp.call(f"{name} {token(text)}"))
            check(name, game, (0, ours), f"text={text.hex()}")

        emu.reset_data()
        a = emu.alloc(text)
        b = emu.alloc(other)
        game = signed32(checked_call(emu, "cmp", [a, b]).eax)
        ours = int(cpp.call(f"cmp {token(text)} {token(other)}"))
        check("cmp", game, ours, f"a={text.hex()} b={other.hex()}")

        emu.reset_data()
        address = emu.alloc(text)
        game = checked_call(emu, "len", [address]).eax
        ours = int(cpp.call(f"len {token(text)}"))
        check("len", game, ours, f"text={text.hex()}")

        character = rng.randrange(256)
        emu.reset_data()
        address = emu.alloc(text)
        pointer = checked_call(emu, "rchr", [address, character]).eax
        game = -1 if pointer == 0 else pointer - address
        ours = int(cpp.call(f"rchr {token(text)} {character}"))
        check("rchr", game, ours, f"text={text.hex()} char={character}")

        emu.reset_data()
        source = emu.alloc(text)
        output_size = (len(text) + 1) * 2
        destination = emu.alloc(bytes([FILL]) * output_size)
        result = checked_call(emu, "widen", [destination, source])
        game = (result.eax - destination, emu.read(destination, output_size))
        ours = bytes.fromhex(cpp.call(f"widen {token(text)}"))
        check("widen", game, (0, ours), f"text={text.hex()}")

        emu.reset_data()
        address = emu.alloc(text)
        game = checked_call(emu, "fnv", [address]).eax
        ours = int(cpp.call(f"fnv {token(text)}"))
        check("fnv", game, ours, f"text={text.hex()}")

    for index in range(RANDOM_CASES + 6):
        length = rng.randrange(0, 64) if index < RANDOM_CASES else index
        a_data = bytes(rng.randrange(256) for _ in range(length + 4))
        b_data = bytes(rng.randrange(256) for _ in range(length + 4))
        emu.reset_data()
        a = emu.alloc(a_data)
        b = emu.alloc(b_data)
        game = signed32(checked_call(emu, "bytes", [a, b, length]).eax)
        ours = int(cpp.call(f"bytes {token(a_data)} {token(b_data)} {length}"))
        check("bytes", game, ours, f"length={length}")

        left_length = rng.randrange(0, min(len(a_data), 0x10000))
        c_string = random_cstring(rng)
        header = struct.pack("<HH", left_length, 0) + a_data
        emu.reset_data()
        allocation = emu.alloc(header)
        left_data = allocation + 4
        left_object = emu.alloc(struct.pack("<I", left_data))
        right = emu.alloc(c_string)
        game = signed32(checked_call(emu, "pcstr", [right], ecx=left_object).eax)
        ours = int(cpp.call(
            f"pcstr {token(a_data)} {token(c_string)} {left_length}"))
        check("pcstr", game, ours, f"leftLength={left_length}")

        right_length = rng.randrange(0, min(len(b_data), 0x10000))
        right_header = struct.pack("<HH", right_length, 0) + b_data
        emu.reset_data()
        left_allocation = emu.alloc(header)
        right_allocation = emu.alloc(right_header)
        left_object = emu.alloc(struct.pack("<I", left_allocation + 4))
        right_object = emu.alloc(struct.pack("<I", right_allocation + 4))
        game = signed32(checked_call(emu, "pp", [right_object], ecx=left_object).eax)
        ours = int(cpp.call(
            f"pp {token(a_data)} {token(b_data)} {left_length} {right_length}"))
        check("pp", game, ours,
              f"leftLength={left_length} rightLength={right_length}")

    utf_edges = [
        bytes.fromhex(value) for value in
        ("00a5a5a5", "7fa5a5a5", "80ffffff", "c080a5a5", "dfbfa5a5",
         "e08080a5", "efbfbfa5", "f0808080", "f7bfbfbf", "ffffffff")
    ]
    for index in range(RANDOM_CASES + len(utf_edges)):
        encoded = (bytes(rng.randrange(256) for _ in range(4))
                   if index < RANDOM_CASES else utf_edges[index - RANDOM_CASES])
        emu.reset_data()
        address = emu.alloc(encoded)
        game = checked_call(emu, "decode", [address]).eax
        ours = int(cpp.call(f"decode {token(encoded)}"))
        check("decode", game, ours, f"bytes={encoded.hex()}")

        emu.reset_data()
        address = emu.alloc(encoded)
        output = emu.alloc(b"\0\0\0\0")
        result = checked_call(emu, "decodeadv", [address, output])
        game = (struct.unpack("<I", emu.read(output, 4))[0], result.eax - address)
        ours_text = cpp.call(f"decodeadv {token(encoded)}")
        ours_parts = tuple(int(value) for value in ours_text.split())
        check("decodeadv", game, ours_parts, f"bytes={encoded.hex()}")

        code_point = rng.randrange(0x100000000)
        emu.reset_data()
        output = emu.alloc(bytes([FILL]) * 4)
        checked_call(emu, "encode", [output, code_point])
        game = emu.read(output, 4)
        ours = bytes.fromhex(cpp.call(f"encode {code_point}"))
        check("encode", game, ours, f"codePoint=0x{code_point:08X}")

    cpp.close()
    for name, (cases, mismatches) in totals.items():
        print(f"{name}: {cases}/{cases} matched, mismatches={mismatches}")


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument(
        "--portcheck", type=Path,
        default=Path(__file__).resolve().parents[3] / "build/win32/bin/Debug/ra3_portcheck.exe")
    args = parser.parse_args()
    verify(args.portcheck.resolve())


if __name__ == "__main__":
    main()
