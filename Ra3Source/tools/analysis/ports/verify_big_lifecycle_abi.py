"""Execute clean BIG lifecycle code with Python ABI stubs.

This oracle verifies the allocator, archive-file and synchronization call shapes
used by ra3_1.0.game. It maps only .text/.rdata/.data through emulate.py.
"""

from __future__ import annotations

import struct
import sys
from pathlib import Path

from unicorn import x86_const as r

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))

from emulate import Emulator  # noqa: E402


def u32(emu: Emulator, address: int) -> int:
    return struct.unpack("<I", emu.read(address, 4))[0]


def main() -> int:
    emu = Emulator()
    calls: dict[str, list[tuple[int, ...]]] = {}

    def record(name: str, count: int, result: int = 0):
        def stub(machine: Emulator) -> int:
            calls.setdefault(name, []).append(
                (machine.register(r.UC_X86_REG_ECX),)
                + tuple(machine.stack_arg(i) for i in range(count))
            )
            return result

        return stub

    allocator = emu.alloc(b"\0" * 4)
    allocator_vtable = emu.alloc(b"\0" * 0x10)
    emu.write(allocator, struct.pack("<I", allocator_vtable))
    temporary = emu.alloc(b"\0" * 0x8000)
    tagged = emu.alloc(b"\0" * 0x200)
    temporary_stub = emu.bind_stub(record("temporary", 5, temporary), 20)
    tagged_stub = emu.bind_stub(record("tagged", 3, tagged), 12)
    free_stub = emu.bind_stub(record("free", 2), 8)
    emu.write(
        allocator_vtable + 4,
        struct.pack("<III", temporary_stub, tagged_stub, free_stub),
    )
    handler = emu.alloc(b"\0" * 0x20)
    emu.write(handler + 0x1C, struct.pack("<I", allocator))

    # Close: inflater deleting destructor, then temporary and GameBigFile frees.
    inflater = emu.alloc(b"\0" * 0x70)
    inflater_vtable = emu.alloc(b"\0" * 4)
    inflater_destructor = emu.bind_stub(record("inflater_delete", 1), 4)
    emu.write(inflater_vtable, struct.pack("<I", inflater_destructor))
    emu.write(inflater, struct.pack("<I", inflater_vtable))
    file_object = emu.alloc(b"\0" * 0x40)
    emu.write(file_object + 0x0C, struct.pack("<II", inflater, temporary))
    result = emu.call(0x009F5DB0, [file_object], ecx=handler)
    assert result.stack_cleanup == 4
    assert calls["inflater_delete"] == [(inflater, 1)]
    assert calls["free"] == [(allocator, temporary, 0), (allocator, file_object, 0)]

    # Seek: six stack arguments, logical-only compressed seek, reset/free on zero.
    calls["free"].clear()
    seek_file = emu.alloc(b"\0" * 0x40)
    emu.write(seek_file + 0x0C, struct.pack("<II", inflater, temporary))
    emu.write(seek_file + 0x18, struct.pack("<Q", 0x1234))
    emu.write(seek_file + 0x28, struct.pack("<Q", 0x80))
    result = emu.call(0x009F5E00, [seek_file, 7, 0, 0, 0, 0], ecx=handler)
    assert result.stack_cleanup == 24
    assert u32(emu, seek_file + 0x38) == 7 and u32(emu, seek_file + 0x0C) == inflater
    result = emu.call(0x009F5E00, [seek_file, 0, 0, 0, 0, 0], ecx=handler)
    assert result.stack_cleanup == 24
    assert u32(emu, seek_file + 0x30) == 0x1234
    assert u32(emu, seek_file + 0x0C) == 0 and u32(emu, seek_file + 0x10) == 0
    assert calls["free"] == [(allocator, temporary, 0)]

    # FindFirst proves tagged allocation argument order and the 0x110-byte size.
    handler_vtable = emu.alloc(b"\0" * 0x44)
    find_next = emu.bind_stub(record("find_next", 2, 1), 8)
    emu.write(handler_vtable + 0x3C, struct.pack("<I", find_next))
    emu.write(handler, struct.pack("<I", handler_vtable))
    archive = emu.alloc(b"\0" * 0x68)
    emu.write(handler + 0x10, struct.pack("<I", archive))

    def init_find(machine: Emulator) -> int:
        this = machine.register(r.UC_X86_REG_ECX)
        emu.write(this, struct.pack("<Iiii", machine.stack_arg(1), -1, 0, 0))
        return this

    emu.bind_function(0x009F5ED0, init_find, 8)
    def strchr_stub(machine: Emulator) -> int:
        text = machine.stack_arg(0)
        character = machine.stack_arg(1) & 0xFF
        offset = 0
        while True:
            value = emu.read(text + offset, 1)[0]
            if value == character:
                return text + offset
            if value == 0:
                return 0
            offset += 1

    emu.bind_import(0x00BBA520, strchr_stub)
    path = emu.alloc(b"big:/Data/Test.bin\0")
    output = emu.alloc(b"\0" * 0x28)
    result = emu.call(0x009F65E0, [path, output], ecx=handler)
    assert result.stack_cleanup == 8 and result.eax == tagged
    tag_call = calls["tagged"][-1]
    assert tag_call[0] == allocator and tag_call[1] == 0x110 and tag_call[3] == 0

    # Read proves archive seek/read vtable shapes and temporary allocation.
    calls.clear()
    archive_file = emu.alloc(b"\0" * 4)
    archive_file_vtable = emu.alloc(b"\0" * 0x20)
    seek_stub = emu.bind_stub(record("seek", 6), 24)
    read_stub = emu.bind_stub(record("read", 5, 0), 20)
    emu.write(archive_file, struct.pack("<I", archive_file_vtable))
    emu.write(archive_file_vtable + 0x14, struct.pack("<I", read_stub))
    emu.write(archive_file_vtable + 0x1C, struct.pack("<I", seek_stub))
    emu.bind_function(0x00A216A0, lambda machine: 1, 4)
    emu.bind_function(0x00A21760, lambda machine: 0)
    new_inflater = emu.alloc(b"\0" * 0x70)
    emu.bind_function(0x00416820, lambda machine: new_inflater)

    def construct_inflater(machine: Emulator) -> int:
        this = machine.register(r.UC_X86_REG_ECX)
        emu.write(this + 8, struct.pack("<I", machine.stack_arg(0)))
        return this

    emu.bind_function(0x009F75C0, construct_inflater, 4)

    def inflate_range(machine: Emulator) -> int:
        game_file = machine.stack_arg(7)
        state = u32(emu, game_file + 0x0C)
        before = u32(emu, state + 0x20)
        emu.write(state + 0x20, struct.pack("<I", before + machine.stack_arg(6)))
        return 0

    emu.bind_function(0x009F6280, inflate_range)
    archive2 = emu.alloc(b"\0" * 0x68)
    emu.write(archive2 + 4, struct.pack("<I", archive_file))
    compressed = emu.alloc(b"\0" * 0x40)
    emu.write(compressed, struct.pack("<I", archive2))
    emu.write(compressed + 0x20, struct.pack("<Q", 0x100))
    emu.write(compressed + 0x28, struct.pack("<Q", 0x80))
    destination = emu.alloc(b"\0" * 16)
    result = emu.call(
        0x009F63B0,
        [compressed, destination, 8, archive_file, destination],
        ecx=handler,
    )
    assert result.stack_cleanup == 20
    assert calls["seek"][0][0] == archive_file and len(calls["seek"][0]) == 7
    assert calls["temporary"][0][0] == allocator
    assert calls["temporary"][0][1] == 0x8000
    assert calls["temporary"][0][3:] == (0, 4, 0)

    # Execute the clean synchronization functions on their critical-section path.
    sync_emu = Emulator()
    sync_calls: dict[str, list[tuple[int, ...]]] = {}

    def sync_record(name: str):
        def stub(machine: Emulator) -> int:
            sync_calls.setdefault(name, []).append((machine.stack_arg(0),))
            return 0

        return stub

    sync = sync_emu.alloc(b"\0" * 0x30)
    timeout = sync_emu.alloc(struct.pack("<I", 0xFFFFFFFF))
    sync_emu.write(sync + 0x24, b"\1")
    sync_emu.bind_import(0x00BBA1E4, sync_record("enter_cs"), 4)
    sync_emu.bind_import(0x00BBA1E8, sync_record("leave_cs"), 4)
    acquired = sync_emu.call(0x00A216A0, [timeout], ecx=sync)
    released = sync_emu.call(0x00A21760, [], ecx=sync)
    assert acquired.stack_cleanup == 4 and released.stack_cleanup == 0
    assert acquired.eax == 1 and released.eax == 0
    assert sync_calls["enter_cs"] == [(sync,)]
    assert sync_calls["leave_cs"] == [(sync,)]

    print("allocator: temporary=1 tagged=1 free=3; argument order verified")
    print("archive file: seek(this+6 args), read(this+5 args); callee cleanup verified")
    print("sync: acquire thiscall+ret4, release fastcall/ret; clean bodies executed")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
