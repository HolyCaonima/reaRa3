"""Compare the shipped 0x70-byte RefPack inflater with the C++ port.

Only clean .text code is executed. Real compressed members are read from the
installed BIG archives without modifying them.
"""

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


DATA = Path(r"D:\Game\Ra3\Game\Data")


def u32(emu: Emulator, address: int) -> int:
    return struct.unpack("<I", emu.read(address, 4))[0]


def u64(emu: Emulator, address: int) -> int:
    return struct.unpack("<Q", emu.read(address, 8))[0]


def members(path: Path):
    data = path.read_bytes()
    count = int.from_bytes(data[8:12], "big")
    header_size = int.from_bytes(data[12:16], "big")
    at = 16
    for _ in range(count):
        offset = int.from_bytes(data[at:at + 4], "big")
        size = int.from_bytes(data[at + 4:at + 8], "big")
        end = data.index(0, at + 8, header_size)
        name = data[at + 8:end].decode("latin1")
        stored = data[offset:offset + size]
        if len(stored) >= 5 and (stored[0] & 0x3E) == 0x10 and stored[1] == 0xFB:
            yield name, stored
        at = end + 1


class CppOracle:
    def __init__(self, executable: Path):
        self.process = subprocess.Popen(
            [str(executable), "--stdio"],
            stdin=subprocess.PIPE,
            stdout=subprocess.PIPE,
            text=True,
        )

    def inflate(self, packed: bytes, input_chunk: int, output_chunk: int):
        assert self.process.stdin is not None
        assert self.process.stdout is not None
        self.process.stdin.write(
            f"inflate {packed.hex()} {input_chunk} {output_chunk}\n"
        )
        self.process.stdin.flush()
        reply = self.process.stdout.readline().strip()
        if reply == "error" or not reply:
            raise RuntimeError("C++ inflater rejected a real member")
        fields = reply.split()
        snapshots = [
            tuple(int(value, 16) for value in item.split(","))
            for item in fields[15].split(";")
        ]
        return bytes.fromhex(fields[0]), snapshots

    def close(self):
        assert self.process.stdin is not None
        self.process.stdin.write("quit\n")
        self.process.stdin.flush()
        if self.process.wait(timeout=10) != 0:
            raise RuntimeError("bigcheck failed")


def run_original(emu: Emulator, packed: bytes, input_chunk: int, output_chunk: int):
    emu.reset_data()

    allocator_vtable = emu.alloc(b"\0" * 0x10)
    allocator = emu.alloc(struct.pack("<I", allocator_vtable))
    if not hasattr(emu, "_bigfs4_allocate_stub"):
        emu._bigfs4_allocate_stub = emu.bind_stub(  # type: ignore[attr-defined]
            lambda machine: emu.alloc(b"\0" * machine.stack_arg(0)), 12
        )
        emu._bigfs4_free_stub = emu.bind_stub(  # type: ignore[attr-defined]
            lambda machine: 0, 8
        )
    emu.write(
        allocator_vtable + 8,
        struct.pack(
            "<II",
            emu._bigfs4_allocate_stub,  # type: ignore[attr-defined]
            emu._bigfs4_free_stub,  # type: ignore[attr-defined]
        ),
    )

    inflater = emu.alloc(b"\0" * 0x70)
    created = emu.call(0x009F75C0, [allocator], ecx=inflater)
    assert created.eax == inflater and created.stack_cleanup == 4

    source = emu.alloc(packed)
    decoded_size = int.from_bytes(packed[2:6] if packed[0] & 0x80 else packed[2:5], "big")
    output = emu.alloc(b"\0" * decoded_size)
    source_at = 0
    output_at = 0
    first_input = True
    snapshots = []
    for _ in range(100000):
        if u32(emu, inflater + 0x14) == 0 and source_at < len(packed):
            count = min(5 if first_input else input_chunk, len(packed) - source_at)
            result = emu.call(
                0x009F7250, [source + source_at, count], ecx=inflater
            )
            assert result.stack_cleanup == 8
            source_at += count
            first_input = False
        if u32(emu, inflater + 0x18) == 0 and output_at < decoded_size:
            count = min(output_chunk, decoded_size - output_at)
            result = emu.call(
                0x009F7270, [output + output_at, count], ecx=inflater
            )
            assert result.stack_cleanup == 8
            output_at += count
        result = emu.call(0x009F7C50, [0], ecx=inflater)
        assert result.stack_cleanup == 4
        window = u32(emu, inflater + 0x54)
        copy = u32(emu, inflater + 0x50)
        cursor = u32(emu, inflater + 0x58)
        window_size = u32(emu, inflater + 0x68)
        snapshots.append(
            (
                u32(emu, inflater + 0x14),
                u32(emu, inflater + 0x18),
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
                sum(emu.read(window, window_size)) & 0xFFFFFFFF if window != 0 else 0,
            )
        )
        if u32(emu, inflater + 0x6C) != 0:
            break
    return emu.read(output, decoded_size), snapshots


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument(
        "--bigcheck",
        type=Path,
        default=Path("build/win32/bin/Debug/bigcheck.exe"),
    )
    parser.add_argument("--archive", type=Path, default=DATA / "Misc.big")
    parser.add_argument("--limit", type=int, default=0)
    args = parser.parse_args()

    rng = random.Random(0x42494734)
    emu = Emulator()
    cpp = CppOracle(args.bigcheck.resolve())
    cases = 0
    try:
        for name, packed in members(args.archive):
            if args.limit and cases >= args.limit:
                break
            if args.limit:
                print(f"checking {name} ({len(packed)} bytes)", flush=True)
            input_chunk = rng.randrange(0x100, 0x1001)
            output_chunk = rng.randrange(0x100, 0x1001)
            game_output, game_states = run_original(
                emu, packed, input_chunk, output_chunk
            )
            if args.limit:
                print(f"original states={len(game_states)}", flush=True)
            our_output, our_states = cpp.inflate(
                packed, input_chunk, output_chunk
            )
            cases += 1
            if game_output != our_output or game_states != our_states:
                difference = next(
                    ((index, left, right)
                     for index, (left, right) in enumerate(
                         zip(game_states, our_states))
                     if left != right),
                    None,
                )
                raise AssertionError(
                    f"{name}: output={game_output == our_output} "
                    f"states={len(game_states)}/{len(our_states)} "
                    f"first={difference}"
                )
            if cases % 25 == 0:
                print(f"verified {cases} compressed members", flush=True)
    finally:
        cpp.close()
    print(f"{args.archive.name}: compressed members={cases} mismatches=0")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
