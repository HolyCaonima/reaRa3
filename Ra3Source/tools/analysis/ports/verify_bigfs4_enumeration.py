"""Compare original and reconstructed BIG FindFirst/FindNext enumeration."""

from __future__ import annotations

import argparse
import struct
import subprocess
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))

from emulate import Emulator  # noqa: E402


ARCHIVE = Path(r"D:\Game\Ra3\Game\Data\Misc.big")
PATTERNS = ("*", "*.*", "*.ini", "*.ttf", "*.fx", "*.fxo", "mapmetadata_*.manifest")


def header(path: Path) -> bytes:
    with path.open("rb") as stream:
        fixed = stream.read(16)
        size = int.from_bytes(fixed[12:16], "big")
        stream.seek(0)
        return stream.read(size)


def u32(emu: Emulator, address: int) -> int:
    return struct.unpack("<I", emu.read(address, 4))[0]


def cstring(emu: Emulator, address: int) -> str:
    result = bytearray()
    while True:
        value = emu.read(address + len(result), 1)[0]
        if value == 0:
            return result.decode("latin1")
        result.append(value)


def original(pattern: str, archive_header: bytes):
    emu = Emulator()
    def strchr(machine: Emulator) -> int:
        text = machine.stack_arg(0)
        wanted = machine.stack_arg(1) & 0xFF
        offset = 0
        while True:
            value = emu.read(text + offset, 1)[0]
            if value == wanted:
                return text + offset
            if value == 0:
                return 0
            offset += 1

    emu.bind_import(0x00BBA520, strchr)
    header_address = emu.alloc(archive_header)
    archive = emu.alloc(b"\0" * 0x68)
    emu.write(
        archive + 0x0C,
        struct.pack("<III", header_address, header_address, 0),
    )
    def allocate(machine: Emulator) -> int:
        return emu.alloc(b"\0" * machine.stack_arg(0))

    allocate_stub = emu.bind_stub(allocate, 12)
    free_stub = emu.bind_stub(lambda machine: 0, 8)
    allocator_vtable = emu.alloc(
        struct.pack("<IIII", 0, 0, allocate_stub, free_stub))
    allocator = emu.alloc(struct.pack("<I", allocator_vtable))
    handler = emu.alloc(b"\0" * 0x20)
    emu.write(handler, struct.pack("<I", 0x00C69460))
    emu.write(handler + 0x10, struct.pack("<I", archive))
    emu.write(handler + 0x1C, struct.pack("<I", allocator))
    pattern_address = emu.alloc(pattern.encode("latin1") + b"\0")
    result = emu.alloc(b"\0" * 0x200)
    first = emu.call(0x009F65E0, [pattern_address, result], ecx=handler)
    assert first.stack_cleanup == 8
    state = first.eax
    records = []
    if state:
        records.append((
            cstring(emu, result + 0x28),
            struct.unpack("<Q", emu.read(result + 0x20, 8))[0],
            struct.unpack("<i", emu.read(state + 4, 4))[0],
            struct.unpack("<i", emu.read(state + 8, 4))[0],
            u32(emu, state + 0x0C) - header_address,
        ))
    while True:
        if state == 0:
            break
        more = emu.call(0x009F6670, [state, result], ecx=handler)
        assert more.stack_cleanup == 8
        if more.eax == 0:
            break
        records.append((
            cstring(emu, result + 0x28),
            struct.unpack("<Q", emu.read(result + 0x20, 8))[0],
            struct.unpack("<i", emu.read(state + 4, 4))[0],
            struct.unpack("<i", emu.read(state + 8, 4))[0],
            u32(emu, state + 0x0C) - header_address,
        ))
    return records


def reconstructed(executable: Path, archive_header: bytes, pattern: str):
    process = subprocess.run(
        [str(executable), "--stdio"],
        input=f"enumerate {archive_header.hex()} {pattern}\nquit\n",
        text=True,
        capture_output=True,
        check=True,
    )
    line = process.stdout.splitlines()[0]
    if not line:
        return []
    records = []
    for item in line.split(";"):
        name, size, state = item.split(":")
        ordinal, record_ordinal, record = (
            int(value, 16) for value in state.split(","))
        records.append((name, int(size, 16), ordinal, record_ordinal, record))
    return records


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument(
        "--bigcheck", type=Path,
        default=Path("build/win32/bin/Debug/bigcheck.exe"))
    args = parser.parse_args()
    archive_header = header(ARCHIVE)
    total = 0
    for pattern in PATTERNS:
        game = original("big:/" + pattern, archive_header)
        ours = reconstructed(args.bigcheck.resolve(), archive_header, pattern)
        if game != ours:
            difference = next(
                ((index, left, right)
                 for index, (left, right) in enumerate(zip(game, ours))
                 if left != right),
                None)
            raise AssertionError(
                f"{pattern}: game={len(game)} ours={len(ours)} "
                f"first={difference}")
        total += len(game)
    print(
        f"Misc.big: patterns={len(PATTERNS)} results={total} mismatches=0")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
