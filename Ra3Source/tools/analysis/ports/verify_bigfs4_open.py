"""Run original and reconstructed BigHandler::Open for every Misc.big member."""

from __future__ import annotations

import argparse
import struct
import subprocess
import sys
from pathlib import Path

from unicorn import x86_const as r

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))

from emulate import Emulator  # noqa: E402


ARCHIVE = Path(r"D:\Game\Ra3\Game\Data\Misc.big")


def archive_index(path: Path):
    data = path.read_bytes()
    count = int.from_bytes(data[8:12], "big")
    header_size = int.from_bytes(data[12:16], "big")
    header = data[:header_size]
    at = 16
    records = []
    for _ in range(count):
        offset = int.from_bytes(header[at:at + 4], "big")
        size = int.from_bytes(header[at + 4:at + 8], "big")
        end = header.index(0, at + 8)
        records.append((header[at + 8:end].decode("latin1"), offset, size))
        at = end + 1
    return data, header, records


def cstring(emu: Emulator, address: int) -> bytes:
    result = bytearray()
    while True:
        value = emu.read(address + len(result), 1)[0]
        if value == 0:
            return bytes(result)
        result.append(value)


class Original:
    def __init__(self, data: bytes, header: bytes, pair: bool = False):
        self.data = data
        self.emu = Emulator()
        emu = self.emu

        def strncmp(machine: Emulator) -> int:
            a = cstring(emu, machine.stack_arg(0))
            b = cstring(emu, machine.stack_arg(1))
            count = machine.stack_arg(2)
            left, right = a[:count], b[:count]
            return (left > right) - (left < right)

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

        def strncpy(machine: Emulator) -> int:
            destination = machine.stack_arg(0)
            source = machine.stack_arg(1)
            count = machine.stack_arg(2)
            emu.write(destination, emu.read(source, count))
            return destination

        emu.bind_import(0x00BBA3AC, strncmp)
        emu.bind_import(0x00BBA520, strchr)
        emu.bind_import(0x00BBA544, strncpy)
        emu.bind_import(
            0x00BBA4D4,
            lambda machine: (
                machine.stack_arg(0) + 0x20
                if 0x41 <= machine.stack_arg(0) <= 0x5A
                else machine.stack_arg(0)))

        def allocate(machine: Emulator) -> int:
            return emu.alloc(b"\0" * machine.stack_arg(0))

        allocate_stub = emu.bind_stub(allocate, 12)
        allocator_vtable = emu.alloc(b"\0" * 0x10)
        emu.write(allocator_vtable + 8, struct.pack("<I", allocate_stub))
        allocator = emu.alloc(struct.pack("<I", allocator_vtable))

        def configure(machine: Emulator) -> int:
            destination = machine.stack_arg(1)
            offset = machine.stack_arg(2) | (machine.stack_arg(3) << 32)
            count = machine.stack_arg(4)
            emu.write(destination, self.data[offset:offset + count])
            return 0

        def position(machine: Emulator) -> int:
            emu.uc.reg_write(r.UC_X86_REG_EDX, 0)
            return 0

        emu.bind_function(0x009C9F40, lambda machine: machine.register(r.UC_X86_REG_ECX))
        emu.bind_function(0x009CA390, configure, 36)
        emu.bind_function(0x009CA280, position)
        emu.bind_function(0x009C9F90, lambda machine: 0)

        header_address = emu.alloc(header)
        names = ("First.big", "Second.big") if pair else ("Misc.big",)
        archives = []
        self.archive_files = []
        for name in names:
            archive_name = emu.alloc(name.encode("ascii") + b"\0")
            archive_file = emu.alloc(b"\0" * 4)
            archive = emu.alloc(b"\0" * 0x68)
            emu.write(archive + 4, struct.pack("<I", archive_file))
            emu.write(archive + 8, struct.pack("<I", header_address))
            emu.write(
                archive + 0x0C,
                struct.pack("<III", header_address, header_address, 0))
            emu.write(archive + 0x18, struct.pack("<I", archive_name))
            if archives:
                emu.write(archives[-1], struct.pack("<I", archive))
            archives.append(archive)
            self.archive_files.append(archive_file)
        handler = emu.alloc(b"\0" * 0x20)
        emu.write(
            handler + 0x10, struct.pack("<II", archives[0], archives[-1]))
        emu.write(handler + 0x18, struct.pack("<I", len(archives)))
        emu.write(handler + 0x1C, struct.pack("<I", allocator))
        self.handler = handler
        emu.write(0x00D15AC0, struct.pack("<I", 0x12345678))

    def open(self, path: str):
        emu = self.emu
        path_address = emu.alloc(path.encode("latin1") + b"\0")
        output = emu.alloc(b"\0" * 4)
        result = emu.call(
            0x009F5F80, [path_address, 0x10000, output], ecx=self.handler,
            max_instructions=10_000_000)
        assert result.stack_cleanup == 12
        if result.eax == 0:
            return None
        raw = emu.read(result.eax, 0x40)
        return (
            struct.unpack_from("<I", raw, 4)[0],
            struct.unpack_from("<I", raw, 8)[0],
            struct.unpack_from("<Q", raw, 0x18)[0],
            struct.unpack_from("<Q", raw, 0x20)[0],
            struct.unpack_from("<Q", raw, 0x28)[0],
            struct.unpack_from("<Q", raw, 0x30)[0],
            struct.unpack_from("<Q", raw, 0x38)[0],
            self.archive_files.index(
                struct.unpack("<I", emu.read(output, 4))[0]) + 1,
        )


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument(
        "--bigcheck", type=Path,
        default=Path("build/win32/bin/Debug/bigcheck.exe"))
    args = parser.parse_args()
    data, header, records = archive_index(ARCHIVE)
    original = Original(data, header)
    process = subprocess.Popen(
        [str(args.bigcheck.resolve()), "--stdio"],
        stdin=subprocess.PIPE, stdout=subprocess.PIPE, text=True)
    assert process.stdin is not None and process.stdout is not None
    process.stdin.write(f"mount {str(ARCHIVE).encode().hex()}\n")
    process.stdin.flush()
    assert process.stdout.readline().strip() == "ok"
    cases = 0
    for name, _, _ in records:
        for query in ("big:/" + name, "big:/Misc.big|" + name):
            game = original.open(query)
            process.stdin.write(f"open {query.encode('latin1').hex()}\n")
            process.stdin.flush()
            reply = process.stdout.readline().strip().split()
            ours = None if reply == ["0"] else tuple(
                int(value, 16) for value in reply[1:])
            if game != ours:
                raise AssertionError(f"{query}: game={game} ours={ours}")
            cases += 1
    pair = Original(data, header, pair=True)
    process.stdin.write(f"mountpair {str(ARCHIVE).encode().hex()}\n")
    process.stdin.flush()
    assert process.stdout.readline().strip() == "ok"
    shared = records[0][0]
    for query in (
        "big:/" + shared,
        "big:/First.big|" + shared,
        "big:/Second.big|" + shared,
        "big:/Missing.big|" + shared,
        "big:/does/not/exist",
    ):
        game = pair.open(query)
        process.stdin.write(f"open {query.encode('latin1').hex()}\n")
        process.stdin.flush()
        reply = process.stdout.readline().strip().split()
        ours = None if reply == ["0"] else tuple(
            int(value, 16) for value in reply[1:])
        if game != ours:
            raise AssertionError(
                f"cross-archive {query}: game={game} ours={ours}")
        cases += 1
    process.stdin.write("quit\n")
    process.stdin.flush()
    if process.wait(timeout=10) != 0:
        raise RuntimeError("bigcheck failed")
    print(f"Misc.big: open cases={cases} mismatches=0")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
