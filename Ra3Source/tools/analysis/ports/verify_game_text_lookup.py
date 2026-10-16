"""Compare RA3's clean GameText lookups with a real gamestrings.csf table.

textcheck reads the CSF through Ra3::io/Ra3::text and emits its labels. This
script builds the corresponding game-owned layouts in Unicorn, executes
0x00539080 and 0x0053B500, and checks every label, a case variant, near misses
(truncated, extended, last byte bumped) and the manager's table precedence.
textcheck runs the same queries against the C++ ports.
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


def queries(label: bytes) -> list[bytes]:
    """Must match textcheck's queriesFor()."""
    result = [label, label.swapcase(), label + b"_"]
    if len(label) > 1:
        result.append(label[:-1])
    if label[-1] < 0x7E:
        result.append(label[:-1] + bytes([label[-1] + 1]))
    return result


class Table:
    def __init__(self, emu: Emulator, labels: list[bytes]):
        self.text_by_key: dict[bytes, int] = {}
        records = []
        for label in labels:
            text_address = emu.alloc(label + b"\0")
            label_address = emu.alloc(struct.pack("<I", text_address), alignment=4)
            info_address = emu.alloc(
                struct.pack("<III", text_address, 0, 0), alignment=4
            )
            records.append((label, label_address, info_address))
            self.text_by_key[label.lower()] = info_address + 4
        records.sort(key=lambda item: item[0].lower())
        lookup_address = emu.alloc(
            b"".join(
                struct.pack("<II", label_address, info_address)
                for _, label_address, info_address in records
            ),
            alignment=4,
        )
        self.address = emu.alloc(
            struct.pack("<III", len(records), 0, lookup_address), alignment=4
        )

    def expected(self, query: bytes) -> int:
        return self.text_by_key.get(query.lower(), 0)


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
    primary = Table(emu, labels)
    # Must match textcheck: override holds every 5th label, map every 3rd plus
    # labels that exist only there.
    override = Table(emu, labels[::5])
    map_labels = labels[::3] + [b"MAP:RA3SOURCE_ONLY_%d" % i for i in range(50)]
    map_table = Table(emu, map_labels)

    primary_only = bytearray(0x6C)
    struct.pack_into("<I", primary_only, 0x48, primary.address)
    primary_manager = emu.alloc(bytes(primary_only), alignment=4)
    layered = bytearray(0x6C)
    struct.pack_into("<III", layered, 0x48, primary.address, map_table.address,
                     override.address)
    layered_manager = emu.alloc(bytes(layered), alignment=4)

    all_queries = [q for label in map_labels + labels for q in queries(label)]
    all_queries += [b"__RA3SOURCE_MISSING__", b"gui:not-a-real-label", b"", b"\x7f"]
    counts = {"table": [0, 0], "manager": [0, 0], "layered": [0, 0]}
    hits = 0
    for query in all_queries:
        query_address = emu.alloc(query + b"\0")
        expected = primary.expected(query)
        hits += expected != 0
        layered_expected = (override.expected(query) or expected
                            or map_table.expected(query))
        for key, ecx, function, want in (
                ("table", primary.address, 0x00539080, expected),
                ("manager", primary_manager, 0x0053B500, expected),
                ("layered", layered_manager, 0x0053B500, layered_expected)):
            call = emu.call(function, [query_address], ecx=ecx)
            counts[key][0] += 1
            counts[key][1] += call.eax != want or call.stack_cleanup != 4

    print(
        f"gamestrings.csf: labels={len(labels)} queries={len(all_queries)} "
        f"primary_hits={hits} "
        + " ".join(f"{key}={total} mismatches={bad}"
                   for key, (total, bad) in counts.items())
    )
    return 1 if any(bad for _, bad in counts.values()) else 0


if __name__ == "__main__":
    raise SystemExit(main())
