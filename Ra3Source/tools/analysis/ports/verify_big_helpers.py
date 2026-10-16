"""Differentially verify clean BIG helpers against the C++ bigcheck oracle."""

from __future__ import annotations

import argparse
import random
import struct
import subprocess
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))

from emulate import Emulator  # noqa: E402


CASES = 1000


class CppOracle:
    def __init__(self, executable: Path) -> None:
        self.process = subprocess.Popen(
            [str(executable), "--stdio"],
            stdin=subprocess.PIPE,
            stdout=subprocess.PIPE,
            text=True,
        )

    def call(self, command: str) -> tuple[int, ...]:
        assert self.process.stdin is not None
        assert self.process.stdout is not None
        self.process.stdin.write(command + "\n")
        self.process.stdin.flush()
        reply = self.process.stdout.readline().strip()
        if not reply or reply == "error":
            raise RuntimeError(f"bigcheck rejected {command!r}")
        return tuple(int(item, 16) for item in reply.split())

    def close(self) -> None:
        assert self.process.stdin is not None
        self.process.stdin.write("quit\n")
        self.process.stdin.flush()
        if self.process.wait(timeout=10) != 0:
            raise RuntimeError("bigcheck failed")


def checked(emu: Emulator, address: int, args: list[int], cleanup: int, **registers):
    result = emu.call(address, args, **registers)
    if result.stack_cleanup != cleanup:
        raise AssertionError(
            f"0x{address:08X}: cleanup {result.stack_cleanup}, expected {cleanup}"
        )
    return result


def make_header(rng: random.Random, kind: int, count: int, tagged: bool) -> bytes:
    header = bytearray(32)
    if kind == 0:
        header[:4] = b"BIG4"
        header[8:12] = count.to_bytes(4, "big")
        header[12:16] = len(header).to_bytes(4, "big")
    elif kind == 1:
        header[:2] = b"\xC0\xFB"
        header[2:4] = (len(header) - 4).to_bytes(2, "big")
        header[4:6] = (count & 0xFFFF).to_bytes(2, "big")
    elif kind == 2:
        header[:4] = b"Viv4"
        header[8:12] = count.to_bytes(4, "big")
        header[12:16] = len(header).to_bytes(4, "big")
    else:
        header[:4] = rng.randbytes(4)
        if header[:3] == b"BIG" or header[:4] == b"Viv4" or header[:2] == b"\xC0\xFB":
            header[0] ^= 0x55
    if tagged and kind != 3:
        number = rng.randrange(1000)
        header[-8:-4] = f"L{number:03d}".encode("ascii")
    return bytes(header)


def verify(executable: Path) -> None:
    rng = random.Random(0x42494732)
    emu = Emulator()
    emu.bind_import(
        0x00BBA4D4,
        lambda machine: (
            machine.stack_arg(0) + 0x20
            if 0x41 <= machine.stack_arg(0) <= 0x5A
            else machine.stack_arg(0)
        ),
    )
    cpp = CppOracle(executable)
    totals = {
        name: [0, 0]
        for name in (
            "readbe32", "readbe64", "format", "hash", "headersize",
            "entrycount", "trailersize", "storedsize", "absoluteoffset", "seekstate",
        )
    }

    def compare(name: str, game, ours) -> None:
        totals[name][0] += 1
        if game != ours:
            totals[name][1] += 1
            raise AssertionError(f"{name}: game={game!r} ours={ours!r}")

    try:
        for case in range(CASES):
            emu.reset_data()
            data = rng.randbytes(12)
            pointer = emu.alloc(data)
            width32 = case % 7 - 1
            result = checked(emu, 0x009F6810, [pointer, width32], 0)
            ours = cpp.call(f"be32 {data.hex()} {width32}")[0]
            compare("readbe32", result.eax, ours)

            width64 = case % 13 - 2
            result = checked(emu, 0x009F6880, [pointer, width64], 0)
            game64 = result.eax | (result.edx << 32)
            ours64 = cpp.call(f"be64 {data.hex()} {width64}")[0]
            compare("readbe64", game64, ours64)

            kind = case % 4
            header = make_header(rng, kind, rng.randrange(0x10000), case % 2 == 0)
            header_pointer = emu.alloc(header)
            result = checked(emu, 0x009F6910, [header_pointer], 0)
            compare("format", result.eax, cpp.call(f"format {header.hex()}")[0])

            result = checked(emu, 0x009F6A50, [header_pointer], 0)
            compare("headersize", result.eax, cpp.call(f"headersize {header.hex()}")[0])

            index_pointer = emu.alloc(
                struct.pack("<III", header_pointer, header_pointer, 0)
            )
            result = checked(emu, 0x009F6AA0, [], 0, ecx=index_pointer)
            compare("entrycount", result.eax, cpp.call(f"entrycount {header.hex()}")[0])

            trailer_header = (
                header if kind != 3
                else make_header(rng, 0, rng.randrange(0x10000), case % 2 == 0)
            )
            trailer_pointer = emu.alloc(trailer_header)
            trailer_index = emu.alloc(
                struct.pack("<III", trailer_pointer, trailer_pointer, 0)
            )
            number_pointer = emu.alloc(b"\xA5" * 4)
            result = checked(
                emu, 0x009F6B00, [number_pointer], 4, ecx=trailer_index
            )
            game_trailer = (
                result.eax,
                struct.unpack("<I", emu.read(number_pointer, 4))[0],
            )
            compare(
                "trailersize", game_trailer,
                cpp.call(f"trailer {trailer_header.hex()}"),
            )

            text = bytes(rng.randrange(0x20, 0x7F) for _ in range(case % 31)) + b"\0"
            text_pointer = emu.alloc(text)
            seed = rng.getrandbits(32)
            result = checked(emu, 0x009F6990, [text_pointer, seed], 0)
            compare("hash", result.eax, cpp.call(f"hash {text.hex()} {seed:x}")[0])

            archive = bytearray(0x68)
            range_begin = rng.getrandbits(64)
            struct.pack_into("<Q", archive, 0x20, range_begin)
            archive_pointer = emu.alloc(bytes(archive))
            file_bytes = bytearray(0x40)
            stored_size = rng.getrandbits(64)
            data_offset = rng.getrandbits(64)
            struct.pack_into("<I", file_bytes, 0, archive_pointer)
            struct.pack_into("<Q", file_bytes, 0x18, data_offset)
            struct.pack_into("<Q", file_bytes, 0x20, stored_size)
            file_pointer = emu.alloc(bytes(file_bytes))

            result = checked(emu, 0x009F5E80, [file_pointer], 4)
            compare(
                "storedsize",
                result.eax | (result.edx << 32),
                cpp.call(f"stored {stored_size:x}")[0],
            )
            result = checked(emu, 0x009F5E90, [file_pointer], 4)
            compare(
                "absoluteoffset",
                result.eax | (result.edx << 32),
                cpp.call(f"absolute {range_begin:x} {data_offset:x}")[0],
            )

            expanded_size = rng.getrandbits(64) if case % 2 else 0
            source_position = rng.getrandbits(64)
            logical_position = rng.getrandbits(64)
            position = rng.getrandbits(64) if case % 3 else 0
            seek_file = bytearray(0x40)
            struct.pack_into("<Q", seek_file, 0x18, data_offset)
            struct.pack_into("<Q", seek_file, 0x28, expanded_size)
            struct.pack_into("<Q", seek_file, 0x30, source_position)
            struct.pack_into("<Q", seek_file, 0x38, logical_position)
            seek_pointer = emu.alloc(bytes(seek_file))
            handler_pointer = emu.alloc(b"\0" * 0x20)
            result = checked(
                emu,
                0x009F5E00,
                [
                    seek_pointer,
                    position & 0xFFFFFFFF,
                    position >> 32,
                    0,
                    0,
                    0,
                ],
                24,
                ecx=handler_pointer,
            )
            game_seek = (
                struct.unpack("<Q", emu.read(seek_pointer + 0x30, 8))[0],
                struct.unpack("<Q", emu.read(seek_pointer + 0x38, 8))[0],
                0,
                result.eax | (result.edx << 32),
            )
            cpp_seek = cpp.call(
                f"seek {data_offset:x} {expanded_size:x} {source_position:x} "
                f"{logical_position:x} {position:x}"
            )
            compare("seekstate", game_seek, (*cpp_seek, position))
    finally:
        cpp.close()

    for name, (cases, mismatches) in totals.items():
        print(f"{name}: cases={cases} mismatches={mismatches}")


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument(
        "--bigcheck",
        type=Path,
        default=Path("build/win32/bin/Debug/bigcheck.exe"),
    )
    args = parser.parse_args()
    verify(args.bigcheck.resolve())
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
