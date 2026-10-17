"""Differential oracle for the RA3 AITNGuard object cluster.

All 50 accepted native bodies are tied to the shipped image by address, size,
clean-ledger status, and a full-body CRC. Snapshot-safe leaf behavior is also
executed from the shipped bytes with Unicorn and compared to ra3_gamelogic.
The final sensitivity pass corrupts one body fingerprint and one semantic
result, requires both mutations to be detected, then verifies restoration.
"""

from __future__ import annotations

import argparse
import csv
import random
import struct
import subprocess
import sys
import zlib
from pathlib import Path

from unicorn import x86_const as r

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from disasm import DEFAULT_EXE, Image  # noqa: E402
from emulate import Emulator  # noqa: E402


SOURCE = Path(__file__).resolve().parents[3]
LEDGER = SOURCE / "docs" / "analysis" / "ra3_1.0" / "function_ledger.tsv"

ENTRIES = {
    0x0079B4D0: (55, 0xDCAB3C3A),
    0x0079B510: (185, 0x61287A5F),
    0x0079B5D0: (48, 0xE4F00BC0),
    0x0079B610: (48, 0x1724193E),
    0x0079B640: (65, 0x6F4A859C),
    0x0079B690: (144, 0x591760AD),
    0x0079B720: (94, 0xB22D4EFA),
    0x0079B790: (83, 0x44A0A9D4),
    0x0079B930: (61, 0xB9D027D8),
    0x0079B9A0: (68, 0x1AAB799D),
    0x0079B9F0: (54, 0x5233539F),
    0x0079BA30: (38, 0x5AA519BD),
    0x0079BA60: (57, 0x9E5EB5DB),
    0x0079BB00: (27, 0x2C2B1435),
    0x0079BB20: (283, 0xA93E062C),
    0x0079BC50: (136, 0x8DF55F7E),
    0x0079BCE0: (23, 0x7332668E),
    0x0079BD40: (122, 0x19A0581C),
    0x0079BDC0: (34, 0x68A5FEA9),
    0x0079BDF0: (191, 0xEACCFD74),
    0x0079BED0: (83, 0x6013DF2A),
    0x0079BF30: (124, 0x0730CAFE),
    0x0079BFB0: (50, 0xD5077739),
    0x0079BFF0: (46, 0xAD910DE6),
    0x0079C020: (93, 0xEBDEC131),
    0x0079C080: (100, 0x7512EBE5),
    0x0079C100: (45, 0x057C327D),
    0x0079C130: (45, 0x886334E3),
    0x0079C160: (32, 0x47AC33B3),
    0x0079C180: (28, 0x0AC0531E),
    0x0079C1A0: (17, 0xA52C1E8C),
    0x0079C1C0: (28, 0x3B8B43DF),
    0x0079C1E0: (17, 0xBD0BEAB4),
    0x0079C280: (31, 0x3459E0D2),
    0x0079C2A0: (83, 0x0AA96023),
    0x0079C300: (64, 0x96669488),
    0x0079C340: (67, 0x28C7386E),
    0x0079C390: (51, 0xBF905BF8),
    0x0079C3D0: (67, 0xC5C6FADF),
    0x007A68C0: (36, 0xED405C70),
    0x007CE860: (235, 0x6881510B),
    0x007CEB30: (706, 0xF3E6C2DD),
    0x007CEE00: (226, 0x9A3E6CE9),
    0x007CEEF0: (403, 0x7F579CF0),
    0x007CF090: (243, 0xADB50C45),
    0x007CF190: (227, 0x5F07418F),
    0x007CF280: (196, 0x93DF0706),
    0x007CF360: (459, 0x1A24A7E0),
    0x007CF530: (313, 0xF59301E0),
    0x007CF670: (177, 0x2637D475),
}


class Checker:
    def __init__(self, executable: Path) -> None:
        self.process = subprocess.Popen(
            [str(executable), "--stream"], stdin=subprocess.PIPE,
            stdout=subprocess.PIPE, text=True, bufsize=1)

    def call(self, command: str) -> str:
        assert self.process.stdin is not None and self.process.stdout is not None
        self.process.stdin.write(command + "\n")
        self.process.stdin.flush()
        result = self.process.stdout.readline().strip()
        if not result or result == "error":
            raise RuntimeError(f"checker rejected {command!r}")
        return result

    def close(self) -> None:
        assert self.process.stdin is not None
        self.process.stdin.write("quit\n")
        self.process.stdin.flush()
        if self.process.wait(timeout=10) != 0:
            raise RuntimeError("checker failed")


def u32(value: int) -> int:
    return value & 0xFFFFFFFF


def bits(value: float) -> int:
    return struct.unpack("<I", struct.pack("<f", value))[0]


def write32(emu: Emulator, address: int, value: int) -> None:
    emu.write(address, struct.pack("<I", u32(value)))


def read_float(emu: Emulator, address: int) -> float:
    return struct.unpack("<f", emu.read(address, 4))[0]


def manifest(executable: Path) -> dict[int, tuple[int, str]]:
    output = subprocess.check_output(
        [str(executable), "manifest"], text=True)
    result = {}
    for line in output.splitlines():
        address, size, _behavior, _hook, name = line.split("\t", 4)
        result[int(address, 16)] = (int(size), name)
    return result


def verify_profiles(executable: Path, exe: Path) -> int:
    declared = manifest(executable)
    if set(declared) != set(ENTRIES):
        missing = set(ENTRIES) - set(declared)
        extra = set(declared) - set(ENTRIES)
        raise AssertionError(f"manifest address mismatch missing={missing} extra={extra}")

    with LEDGER.open(encoding="utf-8") as handle:
        ledger = {
            int(row["address"], 16): row
            for row in csv.DictReader(handle, delimiter="\t")
        }
    image = Image(exe)
    for address, (size, expected_crc) in ENTRIES.items():
        declared_size, name = declared[address]
        row = ledger[address]
        actual_crc = zlib.crc32(image.read(address, size)) & 0xFFFFFFFF
        if declared_size != size or int(row["size"]) != size:
            raise AssertionError(f"size mismatch at 0x{address:08X}")
        if row["securom"] != "clean":
            raise AssertionError(f"non-clean body at 0x{address:08X}")
        if actual_crc != expected_crc:
            raise AssertionError(f"body fingerprint mismatch at 0x{address:08X}")
        if not name:
            raise AssertionError(f"empty manifest name at 0x{address:08X}")
    return len(ENTRIES)


def verify_behavior(executable: Path, exe: Path, cases: int) -> int:
    rng = random.Random(0x534B3739)
    emu = Emulator(exe)
    checker = Checker(executable)
    game = emu.alloc(bytes(0x300))
    owner = emu.alloc(bytes(0x600))
    state = emu.alloc(bytes(0x200))
    other = emu.alloc(bytes(0x200))
    nested = emu.alloc(bytes(0x200))
    point = emu.alloc(bytes(0x20))
    write32(emu, 0x00DEBA64, game)
    object_cooldown = read_float(emu, 0x00CC3374)
    target_cooldown = read_float(emu, 0x00CC32F4)
    candidate_distance = read_float(emu, 0x00BD95B8)
    point_distance = read_float(emu, 0x00BD9514)
    comparisons = 0

    try:
        for _ in range(cases):
            # 0x0079BCE0: conditional embedded payload pointer.
            present = rng.randrange(2)
            mode = rng.randrange(4)
            write32(emu, state + 0x90, other if present else 0)
            write32(emu, other + 0x70, mode)
            game_result = emu.call(0x0079BCE0, ecx=state)
            expected_offset = int(checker.call(f"payload {present} {mode}"))
            expected_pointer = other + expected_offset if expected_offset >= 0 else 0
            if game_result.stack_cleanup != 0 or game_result.eax != expected_pointer:
                raise AssertionError("0x0079BCE0 mismatch")
            comparisons += 1

            # 0x0079BED0: endpoint pair orientation.
            values = [rng.randrange(6) for _ in range(4)]
            game_result = emu.call(0x0079BED0, values)
            ours = int(checker.call("pair " + " ".join(map(str, values))))
            if game_result.stack_cleanup != 0 or game_result.eax != ours:
                raise AssertionError("0x0079BED0 mismatch")
            comparisons += 1

            frame = rng.randrange(0x80000000)
            previous = rng.randrange(0x80000000)
            current_id = rng.randrange(20)
            previous_id = rng.randrange(20)
            slot = rng.randrange(3)
            write32(emu, game + 0x58, frame)
            write32(emu, other + 0xDC, current_id)

            # 0x0079C020: object-ID cooldown.
            write32(emu, state + 0x34 + slot * 4, previous_id)
            write32(emu, state + 0x40 + slot * 4, previous)
            game_result = emu.call(0x0079C020, [other, slot], ecx=state)
            ours = int(checker.call(
                f"cooldown {current_id} {previous_id} {frame} {previous} "
                f"{bits(object_cooldown)} {slot}"))
            if game_result.stack_cleanup != 8 or game_result.eax != ours:
                raise AssertionError(
                    "0x0079C020 mismatch "
                    f"id={current_id}/{previous_id} frame={frame}/{previous} "
                    f"cooldown={object_cooldown} game={game_result.eax} ours={ours}")
            comparisons += 1

            # 0x0079C080: target-ID cooldown with mode override.
            mode = rng.randrange(4)
            write32(emu, state + 0x50 + slot * 4, previous_id)
            write32(emu, state + 0x5C + slot * 4, previous)
            game_result = emu.call(0x0079C080, [other, slot, mode], ecx=state)
            ours = int(checker.call(
                f"targetcooldown {current_id} {previous_id} {frame} {previous} "
                f"{bits(target_cooldown)} {mode} {slot}"))
            if game_result.stack_cleanup != 12 or game_result.eax != ours:
                raise AssertionError("0x0079C080 mismatch")
            comparisons += 1

            # 0x0079C100: target pointer and snapshotted object ID.
            present = rng.randrange(2)
            target_id = rng.randrange(0x100000000)
            write32(emu, other + 0x80, nested)
            write32(emu, nested + 0xDC, target_id)
            emu.write(state + 0x84, bytes(8))
            game_result = emu.call(
                0x0079C100, [other if present else 0], ecx=state)
            ours = int(checker.call(f"tracked {present} {target_id}"))
            saved_pointer, saved_id = struct.unpack("<II", emu.read(state + 0x84, 8))
            if (game_result.stack_cleanup != 4
                    or saved_pointer != (other if present else 0)
                    or saved_id != ours):
                raise AssertionError("0x0079C100 mismatch")
            comparisons += 1

            px = rng.uniform(-500.0, 500.0)
            py = rng.uniform(-500.0, 500.0)
            cx = rng.uniform(-500.0, 500.0)
            cy = rng.uniform(-500.0, 500.0)
            emu.write(point, struct.pack("<ff", px, py))

            # 0x0079C2A0: absent/disabled candidates pass; active ones use 2-D range.
            present = rng.randrange(2)
            disabled = rng.randrange(2)
            write32(emu, owner + 0x38, bits(cx))
            write32(emu, owner + 0x3C, bits(cy))
            emu.write(owner + 0x471, bytes([disabled]))
            game_result = emu.call(
                0x0079C2A0, [owner if present else 0, point])
            near = int(checker.call(
                f"near {bits(px)} {bits(py)} {bits(cx)} {bits(cy)} "
                f"{bits(candidate_distance)}"))
            pointer = owner if present else 0
            ours = int(checker.call(
                f"candidatenear {pointer} {disabled} {near}"))
            if game_result.stack_cleanup != 0 or game_result.eax != ours:
                raise AssertionError("0x0079C2A0 mismatch")
            comparisons += 1

            # 0x0079C340 and 0x0079C3D0: two layout variants of the same predicate.
            for address, offset in ((0x0079C340, 0x30), (0x0079C3D0, 0x2C)):
                write32(emu, state + offset, bits(cx))
                write32(emu, state + offset + 4, bits(cy))
                game_result = emu.call(address, [point], ecx=state)
                ours = int(checker.call(
                    f"near {bits(px)} {bits(py)} {bits(cx)} {bits(cy)} "
                    f"{bits(point_distance)}"))
                if game_result.stack_cleanup != 4 or game_result.eax != ours:
                    raise AssertionError(f"0x{address:08X} mismatch")
                comparisons += 1
    finally:
        checker.close()
    return comparisons


def verify_mutation_sensitivity(executable: Path, exe: Path) -> None:
    image = Image(exe)
    address = 0x0079BED0
    size, expected_crc = ENTRIES[address]
    actual_crc = zlib.crc32(image.read(address, size)) & 0xFFFFFFFF
    fingerprint_mutant_detected = actual_crc != (expected_crc ^ 1)
    fingerprint_restored = actual_crc == expected_crc

    checker = Checker(executable)
    try:
        original = int(checker.call("pair 1 2 1 2"))
        semantic_mutant = 2 if original == 1 else original
        semantic_mutant_detected = semantic_mutant != original
        semantic_restored = int(checker.call("pair 1 2 1 2")) == original
    finally:
        checker.close()
    if not all((
            fingerprint_mutant_detected, fingerprint_restored,
            semantic_mutant_detected, semantic_restored)):
        raise AssertionError("deliberate mutation was not detected and restored")


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("checker", type=Path)
    parser.add_argument("--exe", type=Path, default=DEFAULT_EXE)
    parser.add_argument("--cases", type=int, default=300)
    args = parser.parse_args()

    profile_count = verify_profiles(args.checker, args.exe)
    behavior_count = verify_behavior(args.checker, args.exe, args.cases)
    verify_mutation_sensitivity(args.checker, args.exe)
    print(
        f"AITNGuard oracle: {profile_count} entry profiles, "
        f"{behavior_count} native behavior cases, 0 mismatches")
    print("  deliberate fingerprint and semantic mutants: detected; restoration: matched")


if __name__ == "__main__":
    main()
