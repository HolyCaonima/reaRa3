"""Reference implementation of RA3's compiled-asset instance-name hash.

Program anchor: ra3_1.0.game 0x00417DB0.  The caller supplies the byte length
as both the input length and initial hash value, and requests final avalanche.
The game lowercases every byte before mixing.  Shipping manifest names are all
ASCII, so this implementation deliberately rejects unverified non-ASCII input.
"""

from __future__ import annotations

import argparse
import csv
from pathlib import Path

MASK = 0xFFFFFFFF


def _lower_ascii(data: bytes) -> bytes:
    if any(byte >= 0x80 for byte in data):
        raise ValueError("RA3 shipping-name verification covers ASCII only")
    return bytes(byte + 0x20 if 0x41 <= byte <= 0x5A else byte for byte in data)


def hash_bytes(data: bytes, seed: int, finalize: bool = True) -> int:
    """Implement the clean function at 0x00417DB0."""
    data = _lower_ascii(data)
    value = seed & MASK
    offset = 0
    for _ in range(len(data) // 4):
        value = (value + data[offset] + (data[offset + 1] << 8)) & MASK
        value ^= (((data[offset + 2] + (data[offset + 3] << 8)) ^
                   ((value << 5) & MASK)) << 11) & MASK
        value = (value + (value >> 11)) & MASK
        offset += 4

    remainder = len(data) & 3
    if remainder == 1:
        value = (value + data[offset]) & MASK
        value ^= (value << 10) & MASK
        value = (value + (value >> 1)) & MASK
    elif remainder == 2:
        value = (value + data[offset] + (data[offset + 1] << 8)) & MASK
        value ^= (value << 11) & MASK
        value = (value + (value >> 17)) & MASK
    elif remainder == 3:
        value = (value + data[offset] + (data[offset + 1] << 8)) & MASK
        value ^= ((((data[offset + 2] << 2) ^ value) << 16) & MASK)
        value = (value + (value >> 11)) & MASK

    if finalize:
        value ^= (value << 3) & MASK
        value = (value + (value >> 5)) & MASK
        value ^= (value << 2) & MASK
        value = (value + (value >> 15)) & MASK
        value ^= (value << 10) & MASK
    return value & MASK


def instance_id(name: str) -> int:
    encoded = name.encode("ascii")
    return hash_bytes(encoded, len(encoded), True)


def verify_pairs(path: Path) -> tuple[int, int]:
    checked = 0
    mismatches = 0
    with path.open(encoding="utf-8", newline="") as stream:
        for row in csv.DictReader(stream, delimiter="\t"):
            checked += 1
            if instance_id(row["instanceName"]) != int(row["instanceId"], 16):
                mismatches += 1
    return checked, mismatches


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("name", nargs="?")
    parser.add_argument("--verify-pairs", type=Path)
    args = parser.parse_args()
    if args.name is not None:
        print(f"{instance_id(args.name):08X}")
    if args.verify_pairs:
        checked, mismatches = verify_pairs(args.verify_pairs)
        print(f"checked={checked} mismatches={mismatches}")
        if mismatches:
            raise SystemExit(1)


if __name__ == "__main__":
    main()
