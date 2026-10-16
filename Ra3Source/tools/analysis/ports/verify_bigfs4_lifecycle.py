"""End-to-end BIG Read/Seek differential on real Misc.big members."""

from __future__ import annotations

import argparse
import random
import struct
import subprocess
import sys
from pathlib import Path

from unicorn import x86_const as r

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))

from emulate import Emulator  # noqa: E402


ARCHIVE = Path(r"D:\Game\Ra3\Game\Data\Misc.big")


def archive_members(path: Path):
    data = path.read_bytes()
    count = int.from_bytes(data[8:12], "big")
    header_size = int.from_bytes(data[12:16], "big")
    at = 16
    result = []
    for _ in range(count):
        offset = int.from_bytes(data[at:at + 4], "big")
        size = int.from_bytes(data[at + 4:at + 8], "big")
        end = data.index(0, at + 8, header_size)
        name = data[at + 8:end].decode("latin1")
        stored = data[offset:offset + size]
        compressed = (
            len(stored) >= 5
            and (stored[0] & 0x3E) == 0x10
            and stored[1] == 0xFB
        )
        size_bytes = 4 if compressed and stored[0] & 0x80 else 3
        skip = size_bytes if compressed and stored[0] & 1 else 0
        logical = (
            int.from_bytes(stored[2 + skip:2 + skip + size_bytes], "big")
            if compressed else size
        )
        result.append((name, stored, logical, compressed))
        at = end + 1
    return result


def u32(emu: Emulator, address: int) -> int:
    return struct.unpack("<I", emu.read(address, 4))[0]


def u64(emu: Emulator, address: int) -> int:
    return struct.unpack("<Q", emu.read(address, 8))[0]


class CppOracle:
    def __init__(self, executable: Path):
        self.process = subprocess.Popen(
            [str(executable), "--stdio"],
            stdin=subprocess.PIPE,
            stdout=subprocess.PIPE,
            text=True,
        )

    def run(self, packed: bytes, sequence, open_mode: int = 0):
        assert self.process.stdin is not None
        assert self.process.stdout is not None
        encoded = ";".join(f"{position:x},{count:x}" for position, count in sequence)
        self.process.stdin.write(
            f"readseq {packed.hex()} {open_mode:x} {encoded}\n")
        self.process.stdin.flush()
        reply = self.process.stdout.readline().strip()
        if not reply or reply == "error":
            raise RuntimeError("bigcheck readseq failed")
        records = []
        for record in reply.split(";"):
            returned, data, state = record.split(":")
            records.append(
                (int(returned, 16), bytes.fromhex(data),
                 tuple(int(value, 16) for value in state.split(",")))
            )
        return records

    def close(self):
        assert self.process.stdin is not None
        self.process.stdin.write("quit\n")
        self.process.stdin.flush()
        if self.process.wait(timeout=10) != 0:
            raise RuntimeError("bigcheck failed")


def run_original(
        emu: Emulator, packed: bytes, logical: int, compressed: bool,
        sequence, open_mode: int = 0):
    emu.reset_data()
    emu._bigfs4_io = {"cursor": 0, "packed": packed}  # type: ignore[attr-defined]

    def allocate(machine: Emulator) -> int:
        return emu.alloc(b"\0" * max(machine.stack_arg(0), 1))

    def seek(machine: Emulator) -> int:
        emu._bigfs4_io["cursor"] = (  # type: ignore[attr-defined]
            machine.stack_arg(1) | (machine.stack_arg(2) << 32))
        return 0

    def read(machine: Emulator) -> int:
        context = emu._bigfs4_io  # type: ignore[attr-defined]
        cursor = context["cursor"]
        source = context["packed"]
        destination = machine.stack_arg(1)
        count = machine.stack_arg(2)
        actual = min(count, max(0, len(source) - cursor))
        if actual:
            emu.write(destination, source[cursor:cursor + actual])
        context["cursor"] += actual
        return actual

    if not hasattr(emu, "_bigfs4_lifecycle_stubs"):
        emu._bigfs4_lifecycle_stubs = (  # type: ignore[attr-defined]
            emu.bind_stub(allocate, 20),
            emu.bind_stub(allocate, 12),
            emu.bind_stub(lambda machine: 0, 8),
            emu.bind_stub(seek, 24),
            emu.bind_stub(read, 20),
        )
    temporary_stub, tagged_stub, free_stub, seek_stub, read_stub = (
        emu._bigfs4_lifecycle_stubs  # type: ignore[attr-defined]
    )

    allocator_vtable = emu.alloc(
        struct.pack("<IIII", 0, temporary_stub, tagged_stub, free_stub)
    )
    allocator = emu.alloc(struct.pack("<I", allocator_vtable))
    handler = emu.alloc(b"\0" * 0x20)
    emu.write(handler + 0x1C, struct.pack("<I", allocator))
    archive_file_vtable = emu.alloc(b"\0" * 0x20)
    emu.write(archive_file_vtable + 0x14, struct.pack("<I", read_stub))
    emu.write(archive_file_vtable + 0x1C, struct.pack("<I", seek_stub))
    archive_file = emu.alloc(struct.pack("<I", archive_file_vtable))
    archive = emu.alloc(b"\0" * 0x68)
    emu.write(archive + 4, struct.pack("<I", archive_file))
    file_object = emu.alloc(b"\0" * 0x40)
    emu.write(file_object, struct.pack("<I", archive))
    emu.write(file_object + 8, struct.pack("<I", open_mode))
    emu.write(file_object + 0x20, struct.pack("<Q", logical))
    emu.write(file_object + 0x28, struct.pack("<Q", len(packed) if compressed else 0))

    emu.bind_function(0x00416820, allocate)
    emu.bind_function(0x004166B0, lambda machine: 0)
    emu.bind_function(0x00A216A0, lambda machine: 1, 4)
    emu.bind_function(0x00A21760, lambda machine: 0)

    records = []
    for position, count in sequence:
        seek_result = emu.call(
            0x009F5E00,
            [file_object, position & 0xFFFFFFFF, position >> 32, 0, 0, 0],
            ecx=handler,
        )
        assert seek_result.stack_cleanup == 24
        destination = emu.alloc(b"\xA5" * max(count, 1))
        result = emu.call(
            0x009F63B0,
            [file_object, destination, count, archive_file, 0],
            ecx=handler,
            max_instructions=50_000_000,
        )
        assert result.stack_cleanup == 20
        actual = result.eax
        inflater = u32(emu, file_object + 0x0C)
        state = [
            int(u32(emu, file_object) != 0),
            u32(emu, file_object + 0x04),
            u32(emu, file_object + 0x08),
            u32(emu, file_object + 0x14),
            u64(emu, file_object + 0x18),
            u64(emu, file_object + 0x20),
            u64(emu, file_object + 0x28),
            u64(emu, file_object + 0x30),
            u64(emu, file_object + 0x38),
            int(inflater != 0),
            int(u32(emu, file_object + 0x10) != 0),
        ]
        if inflater:
            window = u32(emu, inflater + 0x54)
            copy = u32(emu, inflater + 0x50)
            cursor = u32(emu, inflater + 0x58)
            window_size = u32(emu, inflater + 0x68)
            state.extend([
                u32(emu, inflater + 0x04),
                int(u32(emu, inflater + 0x08) != 0),
                u32(emu, inflater + 0x0C),
                u32(emu, inflater + 0x10),
                u32(emu, inflater + 0x14),
                u32(emu, inflater + 0x18),
                u32(emu, inflater + 0x1C),
                u64(emu, inflater + 0x20),
                u32(emu, inflater + 0x38),
                u32(emu, inflater + 0x3C),
                u32(emu, inflater + 0x40),
                u32(emu, inflater + 0x44),
                u32(emu, inflater + 0x48),
                u32(emu, inflater + 0x4C),
                0xFFFFFFFFFFFFFFFF if copy == 0 else copy - window,
                0xFFFFFFFFFFFFFFFF if cursor == 0 else cursor - window,
                u64(emu, inflater + 0x60),
                window_size,
                u32(emu, inflater + 0x6C),
                sum(emu.read(window, window_size)) & 0xFFFFFFFF,
            ])
        records.append((actual, emu.read(destination, actual), tuple(state)))
    close_result = emu.call(0x009F5DB0, [file_object], ecx=handler)
    assert close_result.stack_cleanup == 4
    return records


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument(
        "--bigcheck", type=Path,
        default=Path("build/win32/bin/Debug/bigcheck.exe"))
    parser.add_argument("--full-limit", type=int, default=0)
    parser.add_argument("--random-members", type=int, default=10)
    args = parser.parse_args()
    members = archive_members(ARCHIVE)
    emu = Emulator()
    cpp = CppOracle(args.bigcheck.resolve())
    full_cases = 0
    random_cases = 0
    rng = random.Random(0x42494734)
    try:
        for name, packed, logical, compressed in members:
            if args.full_limit and full_cases >= args.full_limit:
                break
            sequence = [(0, logical + 32)]
            game = run_original(
                emu, packed, logical, compressed, sequence, 0x10000)
            ours = cpp.run(packed, sequence, 0x10000)
            if game != ours:
                raise AssertionError(f"full read mismatch: {name}")
            full_cases += 1
            if full_cases % 50 == 0:
                print(f"verified {full_cases} full member reads", flush=True)

        compressed_members = [item for item in members if item[3]]
        for name, packed, logical, compressed in compressed_members[:args.random_members]:
            sequence = []
            for _ in range(50):
                position = rng.randrange(logical + 17) if logical else 0
                count = rng.choice([0, 1, 7, 31, 4096, rng.randrange(0x8001)])
                sequence.append((position, count))
            game = run_original(emu, packed, logical, compressed, sequence)
            ours = cpp.run(packed, sequence)
            if game != ours:
                for index, (left, right) in enumerate(zip(game, ours)):
                    if left != right:
                        raise AssertionError(
                            f"random mismatch: {name} operation={index} "
                            f"game={left[0], left[2]} ours={right[0], right[2]}")
                raise AssertionError(f"random mismatch: {name}")
            random_cases += len(sequence)
    finally:
        cpp.close()
    print(
        f"Misc.big: full members={full_cases} random compressed operations="
        f"{random_cases} mismatches=0")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
