"""Compare RA3's clean GameText lookups with a real gamestrings.csf table.

textcheck reads the CSF through Ra3::io/Ra3::text and emits its labels. This
script builds the corresponding game-owned layouts in Unicorn, executes
0x00539080 and 0x0053B500, and checks every label, a case variant, and misses.
"""

from __future__ import annotations

import argparse
import struct
import subprocess
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))

from emulate import Emulator  # noqa: E402


def cstring(machine: Emulator, address: int) -> bytes:
    value = bytearray()
    while True:
        byte = machine.read(address + len(value), 1)[0]
        if byte == 0:
            return bytes(value)
        value.append(byte)


def stricmp(machine: Emulator) -> int:
    left = cstring(machine, machine.stack_arg(0)).lower()
    right = cstring(machine, machine.stack_arg(1)).lower()
    return 0 if left == right else (-1 if left < right else 1)


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument(
        "--textcheck",
        type=Path,
        default=Path("build/win32/bin/Debug/textcheck.exe"),
    )
    args = parser.parse_args()
    result = subprocess.run(
        [str(args.textcheck.resolve()), "--labels"],
        check=True,
        capture_output=True,
        text=True,
        encoding="utf-8",
    )
    labels = [line.encode("ascii") for line in result.stdout.splitlines()]
    if not labels:
        raise RuntimeError("textcheck returned no labels")

    emu = Emulator()
    emu.bind_import(0x00BBA4D0, stricmp)
    records: list[tuple[bytes, int, int]] = []
    for label in labels:
        text_address = emu.alloc(label + b"\0")
        label_address = emu.alloc(struct.pack("<I", text_address), alignment=4)
        info_address = emu.alloc(
            struct.pack("<III", text_address, 0, 0), alignment=4
        )
        records.append((label, label_address, info_address))
    records.sort(key=lambda item: item[0].lower())
    lookup_address = emu.alloc(
        b"".join(
            struct.pack("<II", label_address, info_address)
            for _, label_address, info_address in records
        ),
        alignment=4,
    )
    table_address = emu.alloc(
        struct.pack("<III", len(records), 0, lookup_address), alignment=4
    )
    manager = bytearray(0x6C)
    struct.pack_into("<I", manager, 0x48, table_address)
    manager_address = emu.alloc(bytes(manager), alignment=4)

    counts = {"table": [0, 0], "manager": [0, 0]}
    for label, _, info_address in records:
        for query in (label, label.swapcase()):
            query_address = emu.alloc(query + b"\0")
            expected = info_address + 4
            table = emu.call(0x00539080, [query_address], ecx=table_address)
            manager_result = emu.call(
                0x0053B500, [query_address], ecx=manager_address
            )
            counts["table"][0] += 1
            counts["table"][1] += (
                table.eax != expected or table.stack_cleanup != 4
            )
            counts["manager"][0] += 1
            counts["manager"][1] += (
                manager_result.eax != expected
                or manager_result.stack_cleanup != 4
            )

    for miss in (b"__RA3SOURCE_MISSING__", b"gui:not-a-real-label"):
        query_address = emu.alloc(miss + b"\0")
        table = emu.call(0x00539080, [query_address], ecx=table_address)
        manager_result = emu.call(
            0x0053B500, [query_address], ecx=manager_address
        )
        counts["table"][0] += 1
        counts["table"][1] += table.eax != 0
        counts["manager"][0] += 1
        counts["manager"][1] += manager_result.eax != 0

    print(
        f"gamestrings.csf: labels={len(labels)} "
        f"table={counts['table'][0]} mismatches={counts['table'][1]} "
        f"manager={counts['manager'][0]} mismatches={counts['manager'][1]}"
    )
    return 1 if any(mismatches for _, mismatches in counts.values()) else 0


if __name__ == "__main__":
    raise SystemExit(main())
