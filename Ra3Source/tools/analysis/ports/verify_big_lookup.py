"""Run the shipped BigHandler lookups 0x009F6B90/0x009F6D00 on a real archive index.

Builds the game's in-memory index for Misc.big (read-only) inside the emulator and
compares every entry's lookup by path and by ordinal with the expected record.
"""

from __future__ import annotations

import struct
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))

from emulate import Emulator  # noqa: E402


def be32(data: bytes, at: int) -> int:
    return int.from_bytes(data[at:at + 4], "big")


def path_hash(name: bytes) -> int:
    value = 0x1505
    for byte in name:
        folded = byte + 0x20 if 0x41 <= byte <= 0x5A else byte
        value = (value * 33 + folded) & 0xFFFFFFFF
    return int.from_bytes(value.to_bytes(4, "little"), "big")


def main() -> int:
    archive_path = Path(r"D:\Game\Ra3\Game\Data\Misc.big")
    with archive_path.open("rb") as stream:
        fixed = stream.read(16)
        header_size = be32(fixed, 12)
        stream.seek(0)
        header = stream.read(header_size)
    count = be32(header, 8)
    trailer = 8 if header[-8:-7].isalpha() and header[-7:-4].isdigit() else 0
    end = len(header) - trailer
    records: list[tuple[int, bytes, int, int]] = []
    at = 16
    while at < end and len(records) < count:
        offset = be32(header, at)
        size = be32(header, at + 4)
        nul = header.index(0, at + 8, end)
        records.append((at, header[at + 8:nul], offset, size))
        at = nul + 1
    if len(records) != count:
        raise RuntimeError(f"parsed {len(records)} of {count} entries")

    emu = Emulator()
    emu.bind_import(
        0x00BBA4D4,
        lambda machine: (
            machine.stack_arg(0) + 0x20
            if 0x41 <= machine.stack_arg(0) <= 0x5A
            else machine.stack_arg(0)
        ),
    )
    header_address = emu.alloc(header)
    table = sorted((path_hash(name), header_address + record) for record, name, _, _ in records)
    table_address = emu.alloc(b"".join(struct.pack("<II", *item) for item in table))
    index_address = emu.alloc(struct.pack("<III", header_address, header_address, table_address))
    offset_out = emu.alloc(b"\0" * 8)
    size_out = emu.alloc(b"\0" * 8)
    record_out = emu.alloc(b"\0" * 4)
    ordinal_out = emu.alloc(b"\xff" * 4)

    mismatches = 0
    cases = 0
    sample = records if count <= 512 else records[:: max(1, count // 512)][:512]
    for record, name, expected_offset, expected_size in sample:
        for query in (name, name.swapcase()):
            query_address = emu.alloc(query + b"\0")
            expected_record = header_address + record
            hashed = emu.call(
                0x009F6B90, [query_address, 4, 4], ecx=index_address
            )
            cases += 1
            mismatches += hashed.eax != expected_record or hashed.stack_cleanup != 12

            emu.write(offset_out, b"\xA5" * 8)
            emu.write(size_out, b"\xA5" * 8)
            emu.write(record_out, b"\0" * 4)
            emu.write(ordinal_out, b"\xff" * 4)
            found = emu.call(
                0x009F6D00,
                [
                    query_address,
                    0,
                    offset_out,
                    size_out,
                    record_out,
                    ordinal_out,
                ],
                ecx=index_address,
            )
            got_offset = struct.unpack("<Q", emu.read(offset_out, 8))[0]
            got_size = struct.unpack("<Q", emu.read(size_out, 8))[0]
            cases += 1
            mismatches += (
                found.eax != expected_record + 8
                or got_offset != expected_offset
                or got_size != expected_size
                or found.stack_cleanup != 24
            )

    missing = emu.alloc(b"__ra3source_missing__.bin\0")
    hashed = emu.call(0x009F6B90, [missing, 4, 4], ecx=index_address)
    found = emu.call(
        0x009F6D00,
        [missing, 0, offset_out, size_out, record_out, ordinal_out],
        ecx=index_address,
    )
    cases += 2
    mismatches += hashed.eax != 0 or found.eax != 0

    for ordinal in range(min(count, 512)):
        expected_record, _, expected_offset, expected_size = records[ordinal]
        emu.write(record_out, b"\0" * 4)
        emu.write(ordinal_out, b"\xff" * 4)
        found = emu.call(
            0x009F6D00,
            [0, ordinal, offset_out, size_out, record_out, ordinal_out],
            ecx=index_address,
        )
        cases += 1
        mismatches += (
            found.eax != header_address + expected_record + 8
            or struct.unpack("<Q", emu.read(offset_out, 8))[0] != expected_offset
            or struct.unpack("<Q", emu.read(size_out, 8))[0] != expected_size
        )

    print(
        f"{archive_path.name}: entries={count} sampled={len(sample)} "
        f"cases={cases} mismatches={mismatches}"
    )
    return 1 if mismatches else 0


if __name__ == "__main__":
    raise SystemExit(main())
