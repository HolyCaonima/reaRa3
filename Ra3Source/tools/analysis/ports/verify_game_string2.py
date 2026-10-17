"""Differential oracle for the remaining RA3 1.0 StringBase cluster.

The original side executes shipped .text bytes and resolved native bodies from
the read-only run35 capture. Unresolved protected dependencies (the ASCII
setter/get-buffer entries and character-set conversion thunks) are bound as
opaque game-level services; none of their protected implementation is inferred.
The checker side drives ra3::common through ra3_stringbase2check.

Every accepted address gets edge and randomized cases. Calls check stack
cleanup and normalize return values, exact live block headers/bytes, aliases,
reference counts, and lock/allocation/free order. The final sensitivity pass
enables one deliberate checker mutation, requires a mismatch, then removes the
mutation and requires the restored result to match.
"""

from __future__ import annotations

import argparse
import os
import random
import struct
import subprocess
import sys
from dataclasses import dataclass, field
from pathlib import Path
from typing import Optional

from unicorn import x86_const as r

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))

from emulate import Emulator, entry_targets  # noqa: E402


SOURCE = Path(__file__).resolve().parents[3]
SITES = SOURCE / "docs" / "analysis" / "ra3_1.0" / "securom_sites.tsv"
CAPTURE = Path(r"D:\Game\Ra3\RevRa3\work\runtime_dump\run35\snapshot_3_240s")

FUNCTIONS = {
    "widefold": (0x004C9F80, 12, "fixed UTF-16 no-case comparison"),
    "sourcedtor1": (0x004CA010, 4, "ASCII source deleting destructor"),
    "empty1": (0x004CA030, 0, "ASCII isEmpty"),
    "find1": (0x004CA050, 4, "ASCII find character"),
    "starts1": (0x004CA0D0, 8, "ASCII starts-with range"),
    "ends1": (0x004CA170, 8, "ASCII ends-with range"),
    "endsnocase1": (0x004CA1D0, 8, "ASCII ends-with-no-case range"),
    "data2": (0x004CA250, 0, "Unicode data/static-empty fallback"),
    "find2": (0x004CA260, 4, "Unicode find character"),
    "startsnocase2": (0x004CA330, 8, "Unicode starts-with-no-case range"),
    "ends2": (0x004CA380, 8, "Unicode ends-with range"),
    "skipspace2": (0x004CA4E0, 0, "skip leading UTF-16 whitespace"),
    "comparewide": (0x004CA510, 0, "length-aware UTF-16 comparison"),
    "release1": (0x004CA930, 0, "ASCII release"),
    "removelast1": (0x004CAAD0, 0, "ASCII remove-last"),
    "removefirst1": (0x004CAB10, 0, "ASCII remove-first"),
    "prepare2": (0x004CADD0, 4, "Unicode writable buffer"),
    "assign2": (0x004CAE10, 4, "Unicode assignment"),
    "setsource2": (0x004CAE90, 4, "Unicode set abstract source"),
    "concatsource2": (0x004CAF00, 4, "Unicode append abstract source"),
    "copyrange1": (0x004CB020, 12, "ASCII source copyRange"),
    "skipsep1": (0x004CB060, 0, "ASCII skip separators"),
    "skipnonsep1": (0x004CB0A0, 0, "ASCII skip non-separators"),
    "copyrange2": (0x004CB0E0, 12, "Unicode source copyRange"),
    "sourcedtor2": (0x004CB140, 4, "Unicode source deleting destructor"),
    "skipsep2": (0x004CB160, 0, "Unicode skip separators"),
    "skipnonsep2": (0x004CB1B0, 0, "Unicode skip non-separators"),
    "translate1": (0x004CB200, 4, "Unicode-object to ASCII translation"),
    "translate2": (0x004CB2A0, 4, "ASCII-text to Unicode translation"),
    "cmpnocasec1": (0x004CB460, 4, "ASCII no-case compare C string"),
    "cmpnocaseobj1": (0x004CB4C0, 4, "ASCII no-case compare object"),
    "cmpc2": (0x004CB520, 4, "Unicode compare C string"),
    "cmpnocaseobj2": (0x004CB630, 4, "Unicode no-case compare object"),
    "trim1": (0x004CB730, 0, "ASCII trim"),
    "token1": (0x004CB8E0, 8, "ASCII nextToken"),
    "isnone1": (0x004CB990, 0, "ASCII isNone"),
    "trim2": (0x004CBA50, 0, "Unicode trim"),
    "format2": (0x004CBB40, 8, "Unicode format_va"),
    "token2": (0x004CBBE0, 8, "Unicode nextToken"),
    "translateobj2": (0x004CBCE0, 4, "Unicode translation object wrapper"),
    "constructtranslated2": (0x004CBD00, 4, "Unicode constructor from ASCII object"),
    "concatc2": (0x004CBE80, 4, "Unicode append C string"),
    "substringc1": (0x004CC010, 12, "ASCII substring from C string"),
    "substringobj1": (0x004CC0B0, 12, "ASCII substring from object"),
    "replace1": (0x004CC160, 8, "ASCII replace-all"),
    "constructchar2": (0x004CC3B0, 4, "Unicode one-unit constructor"),
    "constructrange2": (0x004CC3F0, 8, "Unicode pointer-length constructor"),
    "substringc2": (0x004CC460, 12, "Unicode substring from C string"),
    "replace2": (0x004CC5B0, 8, "Unicode replace-all"),
    "replaceobj2": (0x004CC820, 8, "Unicode replace-all object wrapper"),
}

PROTECTED = entry_targets(SITES)

LOCK_GETTER = {1: 0x004CA440, 2: 0x004CA490}
TEXT_SOURCE_VTABLE = {1: 0x00BDAB6C, 2: 0x00BDAB7C}
SOURCE_BASE_VTABLE = {1: 0x00BDAAB0, 2: 0x00BDAAC0}
STATIC_EMPTY = {1: 0x00BD282C, 2: 0x00BDAAD0}
ALLOCATE = 0x004165A0
FREE = 0x004166B0
ASCII_SET = 0x004CB690
ASCII_BUFFER = 0x004CA970
ASCII_STR = 0x00402770
WIDE_TO_ASCII = 0x004CD890
ASCII_TO_WIDE = 0x004C9FE0
LEAVE_CRITICAL_SECTION = 0x00BBA1E8
MEMCPY = 0x00BBA53C
ISSPACE = 0x00BBA558
ISWSPACE = 0x00BBA4C4
TOWLOWER = 0x00BBA4C8
STRSTR = 0x00BBA4F4
WCSSTR = 0x00BBA388
VSNWPRINTF = 0x00BBA384
POOL_SIZE = 8 << 20

ASCII_ALPHABET = list(range(0x20, 0x7F)) + [0x80, 0xC4, 0xE4, 0xFF]
WIDE_ALPHABET = list(range(0x20, 0x7F)) + [
    0x80, 0xC4, 0xE4, 0x130, 0x3A3, 0x3C3, 0x410, 0x430, 0x4E2D, 0xFF21, 0xFFFF]
ADVERSARIAL_CASE_START = 64
ADVERSARIAL_OPERATIONS = {
    "trim1", "trim2", "token1", "token2",
    "replace1", "replace2", "replaceobj2",
}


def encode(width: int, values: Optional[list[int]]) -> str:
    if values is None:
        return "-"
    if not values:
        return "."
    return b"".join(value.to_bytes(width, "little") for value in values).hex()


def decode(encoded: str) -> Optional[bytes]:
    if encoded == "-":
        return None
    if encoded == ".":
        return b""
    return bytes.fromhex(encoded)


def signed32(value: int) -> int:
    return struct.unpack("<i", struct.pack("<I", value & 0xFFFFFFFF))[0]


@dataclass
class BlockSpec:
    refcount: int
    length: int
    capacity: int
    area: bytes


@dataclass
class Case:
    operation: str
    line: str
    width: int = 0
    blocks: list[BlockSpec] = field(default_factory=list)
    slots: list[int] = field(default_factory=list)
    arguments: list[str] = field(default_factory=list)
    adversarial: bool = False


class Original:
    def __init__(self) -> None:
        self.emu = Emulator(capture=CAPTURE)
        self.pool = self.emu.alloc(bytes(POOL_SIZE))
        self.cursor = self.pool
        self.trace: list[str] = []
        self.blocks: list[tuple[int, int, str]] = []
        self.new_blocks = 0
        self.faults: list[str] = []
        self.formatted = b""
        self.last_free = 0
        self._install_services()

    def _install_services(self) -> None:
        emu = self.emu

        def lock(machine: Emulator) -> int:
            if machine.register(r.UC_X86_REG_ECX) != self.lock:
                self.faults.append("bad lock receiver")
            self.trace.append("L")
            return 0

        def unlock(machine: Emulator) -> int:
            if machine.stack_arg(0) != self.lock + 8:
                self.faults.append("bad unlock receiver")
            self.trace.append("U")
            return 0

        def allocate(machine: Emulator) -> int:
            size = machine.stack_arg(0)
            if machine.stack_arg(1) != 0 or machine.stack_arg(2) != 0:
                self.faults.append("unexpected allocator arguments")
            address = self.take(b"\xCD" * size)
            self.trace.append(f"A{size}")
            self.blocks.append((address, size - 8, f"n{self.new_blocks}"))
            self.new_blocks += 1
            return address

        def free(machine: Emulator) -> int:
            address = machine.stack_arg(0)
            self.last_free = address
            self.trace.append("F" + self.block_id(address, 0))
            return 0

        def memcpy(machine: Emulator) -> int:
            destination = machine.stack_arg(0)
            source = machine.stack_arg(1)
            count = machine.stack_arg(2)
            if count:
                emu.write(destination, emu.read(source, count))
            return destination

        def is_space(machine: Emulator) -> int:
            value = signed32(machine.stack_arg(0))
            return int(value == 0x20 or 0x09 <= value <= 0x0D)

        def lower(machine: Emulator) -> int:
            value = machine.stack_arg(0)
            return value + 0x20 if 0x41 <= value <= 0x5A else value

        def strstr(machine: Emulator) -> int:
            return self.find_subsequence(machine.stack_arg(0), machine.stack_arg(1), 1)

        def wcsstr(machine: Emulator) -> int:
            return self.find_subsequence(machine.stack_arg(0), machine.stack_arg(1), 2)

        def formatter(machine: Emulator) -> int:
            destination = machine.stack_arg(0)
            capacity = machine.stack_arg(1)
            count = min(len(self.formatted) // 2, capacity)
            if count:
                emu.write(destination, self.formatted[:count * 2])
            emu.write(destination + count * 2, b"\0\0")
            return count

        def converter(machine: Emulator, source_width: int, destination_width: int) -> int:
            source = machine.stack_arg(0)
            destination = machine.stack_arg(2)
            capacity = machine.stack_arg(3)
            values = self.read_units(source, source_width)
            if destination != 0 and capacity != 0:
                converted = bytearray()
                for value in values:
                    if destination_width == 1:
                        converted.append(value & 0xFF)
                    else:
                        converted += (value & 0xFF).to_bytes(2, "little")
                converted += bytes(destination_width)
                emu.write(destination, bytes(converted))
            return len(values) + 1

        def source_length(machine: Emulator) -> int:
            source = machine.register(r.UC_X86_REG_ECX)
            return struct.unpack("<i", emu.read(source + 8, 4))[0]

        def source_copy(machine: Emulator) -> int:
            source = machine.register(r.UC_X86_REG_ECX)
            destination = machine.stack_arg(0)
            text, length = struct.unpack("<Ii", emu.read(source + 4, 8))
            width = self.source_widths.get(source, 1)
            if length:
                emu.write(destination, emu.read(text, length * width))
            return length

        def ascii_set(machine: Emulator) -> int:
            self.manual_set(
                machine.register(r.UC_X86_REG_ECX),
                machine.stack_arg(0),
                signed32(machine.stack_arg(1)),
                1)
            return 0

        def ascii_buffer(machine: Emulator) -> int:
            self_slot = machine.register(r.UC_X86_REG_ECX)
            length = signed32(machine.stack_arg(0))
            if length <= 0:
                return 0
            data = self.manual_ensure(self_slot, length, False, None, 1)
            if data == 0:
                return 0
            self.write_header(data, length=length)
            emu.write(data + length, b"\0")
            return data

        def ascii_str(machine: Emulator) -> int:
            data = struct.unpack("<I", emu.read(machine.register(r.UC_X86_REG_ECX), 4))[0]
            return data or STATIC_EMPTY[1]

        def ascii_compare_no_case(machine: Emulator) -> int:
            self_slot = machine.register(r.UC_X86_REG_ECX)
            left = struct.unpack("<I", emu.read(self_slot, 4))[0] or STATIC_EMPTY[1]
            right = machine.stack_arg(0)
            left_values = self.read_units(left, 1)
            right_values = self.read_units(right, 1) if right else []
            for left_value, right_value in zip(left_values, right_values):
                folded_left = (
                    left_value + 0x20
                    if 0x41 <= left_value <= 0x5A and 0x61 <= right_value <= 0x7A
                    else left_value - 0x20
                    if 0x61 <= left_value <= 0x7A and 0x41 <= right_value <= 0x5A
                    else left_value)
                if folded_left != right_value:
                    signed_left = folded_left if folded_left < 0x80 else folded_left - 0x100
                    signed_right = right_value if right_value < 0x80 else right_value - 0x100
                    return 1 if signed_left > signed_right else -1
            return len(left_values) - len(right_values)

        lock_vtable = emu.alloc(struct.pack("<I", emu.bind_stub(lock, 4)))
        self.lock = emu.alloc(struct.pack("<I", lock_vtable) + bytes(0x1C))
        for getter in LOCK_GETTER.values():
            emu.bind_function(getter, lambda machine: self.lock)
        emu.bind_import(LEAVE_CRITICAL_SECTION, unlock, 4)
        emu.bind_function(ALLOCATE, allocate)
        emu.bind_function(FREE, free)
        emu.bind_import(MEMCPY, memcpy)
        emu.bind_import(ISSPACE, is_space)
        emu.bind_import(ISWSPACE, is_space)
        emu.bind_import(TOWLOWER, lower)
        emu.bind_import(STRSTR, strstr)
        emu.bind_import(WCSSTR, wcsstr)
        emu.bind_import(VSNWPRINTF, formatter)
        emu.bind_function(WIDE_TO_ASCII, lambda machine: converter(machine, 2, 1))
        emu.bind_function(ASCII_TO_WIDE, lambda machine: converter(machine, 1, 2))
        emu.bind_function(ASCII_SET, ascii_set, 8)
        emu.bind_function(ASCII_BUFFER, ascii_buffer, 4)
        emu.bind_function(ASCII_STR, ascii_str)
        # 0x004CB990 calls the protected comparison entry rather than its
        # recovered body; bind that dependency while direct comparison cases
        # still execute the captured body at 0x0045BB30.
        emu.bind_function(0x004CB460, ascii_compare_no_case, 4)
        # The unresolved 0x004CA95C slot has one observed game-level effect:
        # clear self->data. Patch only that six-byte replaced site.
        emu.write(0x004CA95C, b"\xC7\x07\x00\x00\x00\x00")

        self.source_widths: dict[int, int] = {}
        self.source_length_stub = emu.bind_stub(source_length)
        self.source_copy_stub = emu.bind_stub(source_copy, 4)
        self.source_vtables = {
            width: emu.alloc(struct.pack(
                "<IIII", 0, self.source_length_stub, 0, self.source_copy_stub))
            for width in (1, 2)
        }

    def reset(self) -> None:
        self.cursor = self.pool
        self.trace = []
        self.blocks = []
        self.new_blocks = 0
        self.faults = []
        self.formatted = b""
        self.last_free = 0
        self.source_widths = {}

    def take(self, data: bytes) -> int:
        address = (self.cursor + 15) & ~15
        if address + max(len(data), 1) > self.pool + POOL_SIZE:
            raise RuntimeError("scenario pool exhausted")
        self.emu.write(address, data)
        self.cursor = address + max(len(data), 1)
        return address

    def read_units(self, address: int, width: int) -> list[int]:
        values = []
        while True:
            value = int.from_bytes(self.emu.read(address, width), "little")
            if value == 0:
                return values
            values.append(value)
            address += width

    def find_subsequence(self, text: int, needle: int, width: int) -> int:
        target = self.read_units(needle, width)
        if not target:
            return text
        values = self.read_units(text, width)
        for index in range(0, len(values) - len(target) + 1):
            if values[index:index + len(target)] == target:
                return text + index * width
        return 0

    def block_id(self, address: int, bias: int) -> str:
        if address == 0:
            return "0"
        for base, _, name in self.blocks:
            if address == base + bias:
                return name
        return "?"

    def text(self, width: int, encoded: str) -> int:
        data = decode(encoded)
        if data is None:
            return 0
        return self.take(data + bytes(width))

    def object(
        self, width: int, encoded: str, recorded_length: Optional[int] = None,
        *, track: bool = True
    ) -> int:
        data = decode(encoded)
        slot = self.take(bytes(4))
        if data is None:
            return slot
        length = len(data) // width if recorded_length is None else recorded_length
        area = data + bytes(width)
        base = self.take(struct.pack("<iHH", 1, length, length + 1) + area)
        if track:
            self.blocks.append((base, len(area), f"x{len(self.blocks)}"))
        self.emu.write(slot, struct.pack("<I", base + 8))
        return slot

    def source(self, width: int, encoded: str) -> int:
        data = decode(encoded)
        text = self.text(width, encoded)
        length = 0 if data is None else len(data) // width
        source = self.take(struct.pack("<IIi", self.source_vtables[width], text, length))
        self.source_widths[source] = width
        return source

    def header(self, data: int) -> tuple[int, int, int]:
        return struct.unpack("<iHH", self.emu.read(data - 8, 8))

    def write_header(
        self, data: int, *, refcount: Optional[int] = None,
        length: Optional[int] = None, capacity: Optional[int] = None
    ) -> None:
        old_refcount, old_length, old_capacity = self.header(data)
        self.emu.write(data - 8, struct.pack(
            "<iHH",
            old_refcount if refcount is None else refcount,
            old_length if length is None else length,
            old_capacity if capacity is None else capacity))

    def manual_release(self, slot: int) -> None:
        self.trace.append("L")
        data = struct.unpack("<I", self.emu.read(slot, 4))[0]
        if data:
            refcount, length, capacity = self.header(data)
            refcount -= 1
            self.write_header(data, refcount=refcount)
            if refcount == 0:
                self.trace.append("F" + self.block_id(data - 8, 0))
            self.emu.write(slot, bytes(4))
        self.trace.append("U")

    def manual_ensure(
        self, slot: int, needed: int, preserve: bool,
        copied: Optional[tuple[int, int]], width: int
    ) -> int:
        old = struct.unpack("<I", self.emu.read(slot, 4))[0]
        if old:
            refcount, old_length, capacity = self.header(old)
            if capacity > needed and refcount == 1:
                if copied is not None:
                    source, length = copied
                    if length:
                        self.emu.write(old, self.emu.read(source, length * width))
                    self.write_header(old, length=length)
                _, current_length, _ = self.header(old)
                self.emu.write(old + current_length * width, bytes(width))
                return old
        request = (needed * width + 8 + width) & 0xFFFFFFFF
        if request > 0x7FFF:
            return 0
        size = (request + 3) & ~3
        base = self.take(b"\xCD" * size)
        self.trace.append(f"A{size}")
        self.blocks.append((base, size - 8, f"n{self.new_blocks}"))
        self.new_blocks += 1
        capacity = (size - 8) // width
        length = 0
        self.emu.write(base, struct.pack("<iHH", 1, length, capacity))
        data = base + 8
        if old and preserve:
            _, length, _ = self.header(old)
            if length:
                self.emu.write(data, self.emu.read(old, length * width))
            self.write_header(data, length=length)
        if copied is not None:
            source, length = copied
            if length:
                self.emu.write(data, self.emu.read(source, length * width))
            self.write_header(data, length=length)
        self.emu.write(data + length * width, bytes(width))
        self.manual_release(slot)
        self.emu.write(slot, struct.pack("<I", data))
        return data

    def manual_set(self, slot: int, text: int, length: int, width: int) -> None:
        old = struct.unpack("<I", self.emu.read(slot, 4))[0]
        if old and old == text:
            return
        if length == 0:
            self.manual_release(slot)
            return
        self.manual_ensure(slot, length, False, (text, length), width)

    def address(self, operation: str) -> int:
        entry = FUNCTIONS[operation][0]
        return PROTECTED.get(entry, entry)

    def call(
        self, operation: str, stack: list[int], *,
        eax: int = 0, ecx: int = 0, edi: int = 0
    ):
        # Emulator.call initializes EDI to zero. Register-only scanner helpers
        # need it set after frame setup, so use a tiny bound adapter when needed.
        if edi:
            code = b"\xBF" + struct.pack("<I", edi) + b"\xE9"
            adapter = self.emu.alloc(b"\x90" * (len(code) + 4))
            target = self.address(operation)
            relative = target - (adapter + len(code) + 4)
            self.emu.write(adapter, code + struct.pack("<i", relative))
            address = adapter
        else:
            address = self.address(operation)
        result = self.emu.call(address, stack, eax=eax, ecx=ecx)
        cleanup = FUNCTIONS[operation][1]
        if result.stack_cleanup != cleanup:
            raise AssertionError(
                f"{operation}: cleanup {result.stack_cleanup}, expected {cleanup}")
        if self.faults:
            raise AssertionError(f"{operation}: {self.faults}")
        return result

    def describe(self, returned: str, slots: int, count: int) -> str:
        freed = {entry[1:] for entry in self.trace if entry.startswith("F")}
        slot_ids = [
            self.block_id(struct.unpack("<I", self.emu.read(slots + 4 * index, 4))[0], 8)
            for index in range(count)]
        blocks = []
        for base, area, name in self.blocks:
            if name in freed:
                continue
            refcount, length, capacity = struct.unpack("<iHH", self.emu.read(base, 8))
            blocks.append(
                f"{name}:{refcount}:{length}:{capacity}:"
                f"{self.emu.read(base + 8, area).hex()}")
        return (
            f"ret={returned} trace={','.join(self.trace) or '-'} "
            f"slots={','.join(slot_ids)} blocks={';'.join(blocks)}")

    def run_pure(self, case: Case) -> str:
        self.reset()
        operation = case.operation
        args = case.arguments

        if operation == "widefold":
            left = self.text(2, args[0])
            right = self.text(2, args[1])
            result = self.call(
                operation, [left, right, int(args[2])])
            return str(signed32(result.eax))

        if operation == "comparewide":
            left = self.text(2, args[0])
            right = self.text(2, args[2])
            result = self.call(
                operation, [left, int(args[1]), right, int(args[3])])
            return str(signed32(result.eax))

        if operation in {
            "empty1", "find1", "starts1", "ends1", "endsnocase1",
            "cmpnocasec1", "cmpnocaseobj1", "isnone1",
        }:
            encoded, length_text = args[0:2]
            self_slot = self.object(1, encoded, int(length_text))
            data = struct.unpack("<I", self.emu.read(self_slot, 4))[0]
            if operation == "empty1":
                return str(self.call(operation, [], ecx=self_slot).eax & 0xFF)
            if operation == "find1":
                result = self.call(operation, [int(args[2])], ecx=self_slot).eax
                return "-1" if result == 0 else str(result - data)
            if operation == "isnone1":
                return str(self.call(operation, [], ecx=self_slot).eax & 0xFF)
            other = self.text(1, args[2])
            other_length = int(args[3])
            if operation in {"starts1", "ends1", "endsnocase1"}:
                return str(self.call(
                    operation, [other, other_length], ecx=self_slot).eax & 0xFF)
            if operation == "cmpnocasec1":
                return str(signed32(self.call(
                    operation, [other], ecx=self_slot).eax))
            other_slot = self.object(1, args[2], other_length)
            return str(signed32(self.call(
                operation, [other_slot], ecx=self_slot).eax))

        if operation in {
            "data2", "find2", "startsnocase2", "ends2", "cmpc2", "cmpnocaseobj2",
        }:
            encoded, length_text = args[0:2]
            self_slot = self.object(2, encoded, int(length_text))
            data = struct.unpack("<I", self.emu.read(self_slot, 4))[0]
            if operation == "data2":
                result = self.call(operation, [], ecx=self_slot).eax
                return "empty" if result == STATIC_EMPTY[2] else str((result - data) // 2)
            if operation == "find2":
                result = self.call(operation, [int(args[2])], ecx=self_slot).eax
                return "-1" if result == 0 else str((result - data) // 2)
            other = self.text(2, args[2])
            other_length = int(args[3])
            if operation in {"startsnocase2", "ends2"}:
                return str(self.call(
                    operation, [other, other_length], ecx=self_slot).eax & 0xFF)
            if operation == "cmpc2":
                return str(signed32(self.call(
                    operation, [other], ecx=self_slot).eax))
            other_slot = self.object(2, args[2], other_length)
            return str(signed32(self.call(
                operation, [other_slot], ecx=self_slot).eax))

        if operation in {
            "skipspace2", "skipsep1", "skipnonsep1", "skipsep2", "skipnonsep2",
        }:
            width = 2 if operation.endswith("2") else 1
            text = self.text(width, args[0])
            if operation == "skipspace2":
                result = self.call(operation, [], eax=text).eax
            else:
                separators = self.text(width, args[1])
                if width == 1:
                    result = self.call(operation, [], eax=text, edi=separators).eax
                else:
                    result = self.call(operation, [separators], eax=text).eax
            return str((result - text) // width)

        if operation in {"copyrange1", "copyrange2"}:
            width = 1 if operation.endswith("1") else 2
            source_text = self.text(width, args[0])
            source_data = decode(args[0]) or b""
            source = self.take(struct.pack(
                "<IIi", TEXT_SOURCE_VTABLE[width], source_text, len(source_data) // width))
            size = int(args[3])
            output = self.take(b"\xA5" * size)
            result = self.call(
                operation,
                [output, int(args[1]), int(args[2])],
                ecx=source)
            return f"{int(result.eax == output)}:{self.emu.read(output, size).hex()}"

        if operation in {"sourcedtor1", "sourcedtor2"}:
            width = 1 if operation.endswith("1") else 2
            derived = self.take(bytes(16))
            source = self.take(struct.pack("<II", derived, 0x12345678))
            result = self.call(operation, [int(args[0])], ecx=source)
            vtable = struct.unpack("<I", self.emu.read(source, 4))[0]
            free_calls = sum(entry.startswith("F") for entry in self.trace)
            return (
                f"{int(result.eax == source)}:"
                f"{int(vtable == SOURCE_BASE_VTABLE[width])}:"
                f"{free_calls}:{int(self.last_free == source)}")

        raise AssertionError(f"no pure runner for {operation}")

    def load_state(self, case: Case) -> tuple[int, int]:
        for index, block in enumerate(case.blocks):
            base = self.take(
                struct.pack("<iHH", block.refcount, block.length, block.capacity)
                + block.area)
            self.blocks.append((base, len(block.area), f"b{index}"))
        slots = self.take(bytes(4 * len(case.slots)))
        for index, block_index in enumerate(case.slots):
            data = 0 if block_index < 0 else self.blocks[block_index][0] + 8
            self.emu.write(slots + 4 * index, struct.pack("<I", data))
        return slots, len(case.slots)

    def run_state(self, case: Case) -> str:
        self.reset()
        width = case.width
        operation = case.operation
        args = case.arguments
        slots, slot_count = self.load_state(case)
        target_index = int(args[0])
        target = slots + target_index * 4
        stack: list[int] = []
        returned = "1"

        if operation == "release1":
            pass
        elif operation in {"removelast1", "removefirst1", "trim1", "trim2"}:
            pass
        elif operation == "prepare2":
            stack = [int(args[1])]
        elif operation == "assign2":
            stack = [slots + int(args[1]) * 4]
        elif operation in {"setsource2", "concatsource2"}:
            stack = [self.source(2, args[1])]
        elif operation == "translate1":
            source = self.object(2, args[1], track=False)
            stack = [source]
        elif operation == "translate2":
            stack = [self.text(1, args[1])]
        elif operation in {"translateobj2", "constructtranslated2"}:
            source = self.object(1, args[1], track=False)
            stack = [source]
        elif operation in {"token1", "token2"}:
            token = slots + int(args[1]) * 4
            separators = self.text(width, args[2])
            stack = [token, separators]
        elif operation == "format2":
            self.formatted = decode(args[1]) or b""
            format_text = self.text(2, encode(2, [ord("%"), ord("s")]))
            arguments = self.take(bytes(16))
            stack = [format_text, arguments]
        elif operation == "concatc2":
            stack = [self.text(2, args[1])]
        elif operation in {"substringc1", "substringc2"}:
            stack = [self.text(width, args[1]), int(args[2]), int(args[3])]
        elif operation == "substringobj1":
            stack = [
                slots + int(args[1]) * 4,
                int(args[2]),
                int(args[3]),
            ]
        elif operation in {"replace1", "replace2"}:
            stack = [self.text(width, args[1]), self.text(width, args[2])]
        elif operation == "replaceobj2":
            stack = [
                slots + int(args[1]) * 4,
                slots + int(args[2]) * 4,
            ]
        elif operation == "constructchar2":
            stack = [int(args[1])]
        elif operation == "constructrange2":
            stack = [self.text(2, args[1]), int(args[2])]
        else:
            raise AssertionError(f"no state runner for {operation}")

        result = self.call(operation, stack, ecx=target)
        if operation == "prepare2":
            returned = self.block_id(result.eax, 8) if result.eax else "0"
        elif operation in {"token1", "token2"}:
            returned = str(result.eax & 0xFF)
        elif operation in {"replace1", "replace2", "replaceobj2"}:
            returned = str(signed32(result.eax))
        elif operation in {"constructtranslated2", "constructchar2", "constructrange2"}:
            returned = "self" if result.eax == target else "other"
        return self.describe(returned, slots, slot_count)

    def run(self, case: Case) -> str:
        return self.run_pure(case) if case.width == 0 else self.run_state(case)


STATE_ALIAS = {
    "release1": "release",
    "removelast1": "removelast",
    "removefirst1": "removefirst",
    "prepare2": "prepare",
    "assign2": "assign",
    "setsource2": "setsource",
    "concatsource2": "concatsource",
    "trim1": "trim",
    "trim2": "trim",
    "token1": "token",
    "token2": "token",
    "substringc1": "substringc",
    "substringc2": "substringc",
    "substringobj1": "substringobj",
    "replace1": "replace",
    "replace2": "replace",
}


class Generator:
    def __init__(self, seed: int) -> None:
        self.rng = random.Random(seed)

    def values(self, width: int, count: int) -> list[int]:
        alphabet = ASCII_ALPHABET if width == 1 else WIDE_ALPHABET
        return [self.rng.choice(alphabet) for _ in range(count)]

    def block(
        self,
        width: int,
        values: Optional[list[int]] = None,
        minimum_slack: int = 0,
    ) -> BlockSpec:
        rng = self.rng
        if values is None:
            length = rng.choice([0, 1, 2, 3, 5, 8, 15])
            values = self.values(width, length)
        length = len(values)
        slack = (max)(minimum_slack, rng.choice([0, 0, 1, 3, 8]))
        capacity = length + 1 + slack
        area = b"".join(value.to_bytes(width, "little") for value in values)
        area += bytes(width)
        # Make unused capacity observably nonzero. This catches implementations
        # which scan the allocation instead of honoring either NUL or length.
        area += bytes(
            rng.randrange(1, 256)
            for _ in range((capacity - length - 1) * width))
        return BlockSpec(rng.choice([1, 1, 1, 2, 3]), length, capacity, area)

    def pure(self, operation: str, index: int) -> Case:
        rng = self.rng
        if operation == "widefold":
            count = rng.choice([0, 1, 2, 4, 8])
            left = self.values(2, count)
            right = list(left)
            if count and index % 3:
                position = rng.randrange(count)
                right[position] = rng.choice(WIDE_ALPHABET)
                if index % 3 == 1 and 0x41 <= left[position] <= 0x5A:
                    right[position] = left[position] + 0x20
            args = [encode(2, left), encode(2, right), str(count)]
        elif operation == "comparewide":
            left_length = rng.choice([0, 1, 2, 4, 8])
            right_length = rng.choice([0, 1, 2, 4, 8])
            left = self.values(2, left_length)
            right = self.values(2, right_length)
            if index % 3 == 0:
                shared = min(left_length, right_length)
                right[:shared] = left[:shared]
            args = [
                encode(2, left), str(left_length),
                encode(2, right), str(right_length),
            ]
        elif operation in {
            "empty1", "find1", "starts1", "ends1", "endsnocase1",
            "cmpnocasec1", "cmpnocaseobj1", "isnone1",
        }:
            if operation == "isnone1":
                options: list[Optional[list[int]]] = [
                    None, [], list(b"None"), list(b"none"), list(b"NONE"),
                    list(b"NoneX"), list(b"Other")]
                left = options[index % len(options)]
            else:
                left = None if index % 11 == 0 else self.values(1, rng.choice([0, 1, 3, 8]))
            left_length = 0 if left is None else len(left)
            args = [encode(1, left), str(left_length)]
            if operation == "find1":
                character = rng.choice(ASCII_ALPHABET)
                if left and index % 2 == 0:
                    character = rng.choice(left)
                args.append(str(character))
            elif operation not in {"empty1", "isnone1"}:
                right = self.values(1, rng.choice([0, 1, 2, 5]))
                if left and right and index % 3 == 0:
                    right = left[:rng.randint(1, len(left))]
                    if operation.startswith("ends"):
                        right = left[len(left) - len(right):]
                if "nocase" in operation and index % 4 == 0:
                    right = [
                        value + 0x20 if 0x41 <= value <= 0x5A else value
                        for value in right]
                if operation.startswith("cmp") and index % 13 == 0:
                    right_value: Optional[list[int]] = None
                else:
                    right_value = right
                args += [encode(1, right_value), str(0 if right_value is None else len(right_value))]
        elif operation in {
            "data2", "find2", "startsnocase2", "ends2", "cmpc2", "cmpnocaseobj2",
        }:
            left = None if index % 11 == 0 else self.values(2, rng.choice([0, 1, 3, 8]))
            left_length = 0 if left is None else len(left)
            args = [encode(2, left), str(left_length)]
            if operation == "find2":
                character = rng.choice(WIDE_ALPHABET)
                if left and index % 2 == 0:
                    character = rng.choice(left)
                args.append(str(character))
            elif operation != "data2":
                right = self.values(2, rng.choice([0, 1, 2, 5]))
                if left and right and index % 3 == 0:
                    right = left[:rng.randint(1, len(left))]
                    if operation == "ends2":
                        right = left[len(left) - len(right):]
                if "nocase" in operation and index % 4 == 0:
                    right = [
                        value + 0x20 if 0x41 <= value <= 0x5A else value
                        for value in right]
                right_value = None if operation == "cmpc2" and index % 13 == 0 else right
                args += [encode(2, right_value), str(0 if right_value is None else len(right_value))]
        elif operation in {
            "skipspace2", "skipsep1", "skipnonsep1", "skipsep2", "skipnonsep2",
        }:
            width = 2 if operation.endswith("2") else 1
            if operation == "skipspace2":
                prefix = [rng.choice([9, 10, 13, 32]) for _ in range(index % 5)]
                args = [encode(2, prefix + self.values(2, rng.choice([0, 1, 4])))]
            else:
                separators = [ord(","), ord(";"), ord(" ")]
                if "non" in operation:
                    prefix = self.values(width, index % 5)
                else:
                    prefix = [rng.choice(separators) for _ in range(index % 5)]
                text = prefix + self.values(width, rng.choice([0, 1, 4]))
                args = [encode(width, text), encode(width, separators)]
        elif operation in {"copyrange1", "copyrange2"}:
            width = 1 if operation.endswith("1") else 2
            values = self.values(width, rng.choice([1, 2, 5, 12]))
            offset = rng.randrange(len(values))
            count = rng.randrange(len(values) - offset + 1)
            size = count * width + rng.choice([0, 1, 4])
            args = [encode(width, values), str(offset), str(count), str(size)]
        elif operation in {"sourcedtor1", "sourcedtor2"}:
            args = [str(index % 4)]
        else:
            raise AssertionError(f"no pure generator for {operation}")
        return Case(operation, "P " + operation + " " + " ".join(args), arguments=args)

    def state(self, operation: str, index: int) -> Case:
        rng = self.rng
        width = 1 if operation.endswith("1") else 2
        blocks = [self.block(width) for _ in range(rng.choice([1, 2, 3]))]
        slots = [
            rng.randrange(-1, len(blocks)),
            rng.randrange(-1, len(blocks)),
            rng.randrange(-1, len(blocks)),
        ]
        if index % 7 == 0:
            slots[1] = slots[0]
        target = index % len(slots)
        args = [str(target)]

        if operation == "prepare2":
            args.append(str([-2, 0, 1, 2, 9, 33][index % 6]))
        elif operation == "assign2":
            args.append(str((target + 1 + index % 2) % len(slots)))
        elif operation in {"setsource2", "concatsource2"}:
            values = self.values(2, rng.choice([0, 1, 3, 12]))
            args.append(encode(2, values))
        elif operation == "translate1":
            values = None if index % 9 == 0 else self.values(2, rng.choice([0, 1, 3, 12]))
            args.append(encode(2, values))
        elif operation == "translate2":
            args.append(encode(1, self.values(1, rng.choice([0, 1, 3, 12]))))
        elif operation in {"translateobj2", "constructtranslated2"}:
            values = None if index % 9 == 0 else self.values(1, rng.choice([0, 1, 3, 12]))
            args.append(encode(1, values))
            if operation == "constructtranslated2":
                slots[target] = -1
        elif operation in {"trim1", "trim2"}:
            if index >= ADVERSARIAL_CASE_START:
                # NUL is inside the recorded range; characters after it are
                # deliberately nonzero and can include trailing whitespace.
                values = (
                    [rng.choice([9, 32]) for _ in range(index % 3)]
                    + [ord("A"), 0, ord("B")]
                    + [rng.choice([9, 10, 13, 32]) for _ in range(1 + index % 3)]
                )
            else:
                values = (
                    [rng.choice([9, 10, 13, 32]) for _ in range(index % 4)]
                    + self.values(width, rng.choice([0, 1, 4]))
                    + [rng.choice([9, 10, 13, 32]) for _ in range((index // 2) % 4)]
                )
            blocks.append(self.block(
                width,
                values,
                3 if index >= ADVERSARIAL_CASE_START else 0))
            slots[target] = len(blocks) - 1
        elif operation in {"token1", "token2"}:
            token = (target + 1) % len(slots)
            separators = [ord(","), ord(";"), ord(" ")]
            if index >= ADVERSARIAL_CASE_START:
                values = (
                    [rng.choice(separators) for _ in range(index % 3)]
                    + [ord("A"), ord("B"), 0, ord("C")]
                    + [rng.choice(separators), ord("D")]
                )
            else:
                values = (
                    [rng.choice(separators) for _ in range(index % 3)]
                    + self.values(width, rng.choice([0, 1, 3, 6]))
                    + [rng.choice(separators)]
                    + self.values(width, rng.choice([0, 2, 4]))
                )
            blocks.append(self.block(
                width,
                values,
                3 if index >= ADVERSARIAL_CASE_START else 0))
            slots[target] = len(blocks) - 1
            if index < ADVERSARIAL_CASE_START and index % 13 == 0:
                token = target
            args += [
                str(token),
                "-" if operation == "token1" and index % 5 == 0
                else encode(width, separators),
            ]
        elif operation == "format2":
            args.append(encode(2, self.values(2, rng.choice([0, 1, 5, 30]))))
        elif operation == "concatc2":
            args.append(encode(2, self.values(2, rng.choice([0, 1, 3, 20]))))
        elif operation in {"substringc1", "substringc2"}:
            values = self.values(width, rng.choice([0, 1, 3, 8, 20]))
            start = rng.choice([-5, -2, -1, 0, 1, 4, 20])
            count = rng.choice([0, 1, 2, 5, 20])
            args += [encode(width, values), str(start), str(count)]
        elif operation == "substringobj1":
            source = (target + 1) % len(slots)
            start = rng.choice([-5, -2, -1, 0, 1, 4, 20])
            count = rng.choice([0, 1, 2, 5, 20])
            args += [str(source), str(start), str(count)]
        elif operation in {"replace1", "replace2"}:
            unit_a = ord("a")
            unit_b = ord("b")
            if index >= ADVERSARIAL_CASE_START:
                # Matching text occurs on both sides of an embedded NUL.
                values = [unit_a, unit_b, unit_a, 0, unit_a, unit_b, unit_a]
            else:
                values = (
                    [unit_a, unit_b] * rng.choice([0, 1, 2, 4])
                    + self.values(width, 2))
            blocks.append(self.block(
                width,
                values,
                3 if index >= ADVERSARIAL_CASE_START else 0))
            slots[target] = len(blocks) - 1
            search = [unit_a, unit_b] if index % 2 else [unit_a]
            replacement = self.values(width, rng.choice([0, 1, 3]))
            args += [encode(width, search), encode(width, replacement)]
        elif operation == "replaceobj2":
            search_slot = (target + 1) % len(slots)
            replacement_slot = (target + 2) % len(slots)
            source_values = (
                [ord("a"), ord("b"), 0, ord("a"), ord("b"), ord("x")]
                if index >= ADVERSARIAL_CASE_START
                else [ord("a"), ord("b"), ord("a"), ord("b"), ord("x")])
            search_values = [ord("a"), ord("b")]
            replacement_values = self.values(2, rng.choice([0, 1, 3]))
            for slot_index, values in (
                (target, source_values),
                (search_slot, search_values),
                (replacement_slot, replacement_values),
            ):
                blocks.append(self.block(
                    2,
                    values,
                    3 if (
                        index >= ADVERSARIAL_CASE_START
                        and slot_index == target) else 0))
                slots[slot_index] = len(blocks) - 1
            args += [str(search_slot), str(replacement_slot)]
        elif operation == "constructchar2":
            slots[target] = -1
            args.append(str(rng.choice([0, ord("A"), 0x410, 0xFFFF])))
        elif operation == "constructrange2":
            slots[target] = -1
            values = self.values(2, rng.choice([0, 1, 3, 12]))
            length = rng.randrange(len(values) + 1)
            args += [encode(2, values), str(length)]
        elif operation in {
            "release1", "removelast1", "removefirst1",
        }:
            pass
        else:
            raise AssertionError(f"no state generator for {operation}")

        alias = STATE_ALIAS.get(operation, operation)
        parts = ["S", str(width), alias, str(len(blocks))]
        for block in blocks:
            parts += [
                str(block.refcount),
                str(block.length),
                str(block.capacity),
                block.area.hex(),
            ]
        parts.append(str(len(slots)))
        parts += [str(slot) for slot in slots]
        parts += args
        return Case(
            operation,
            " ".join(parts),
            width=width,
            blocks=blocks,
            slots=slots,
            arguments=args,
            adversarial=(
                operation in ADVERSARIAL_OPERATIONS
                and index >= ADVERSARIAL_CASE_START))


def checker_outputs(
    executable: Path, cases: list[Case], *, mutated: bool = False
) -> list[str]:
    environment = os.environ.copy()
    if mutated:
        environment["RA3_STRINGBASE2_MUTATE"] = "1"
    else:
        environment.pop("RA3_STRINGBASE2_MUTATE", None)
    process = subprocess.run(
        [str(executable.resolve())],
        input="".join(case.line + "\n" for case in cases),
        capture_output=True,
        text=True,
        check=True,
        env=environment)
    outputs = process.stdout.splitlines()
    if len(outputs) != len(cases):
        raise AssertionError(
            f"checker answered {len(outputs)} of {len(cases)} scenarios")
    return outputs


def compare_cases(
    original: Original, cases: list[Case], outputs: list[str], *,
    show: bool = True
) -> tuple[int, dict[str, int]]:
    mismatches = 0
    counts: dict[str, int] = {}
    for case, actual in zip(cases, outputs):
        expected = original.run(case)
        counts[case.operation] = counts.get(case.operation, 0) + 1
        if expected != actual:
            mismatches += 1
            if show and mismatches <= 8:
                print(f"MISMATCH {case.operation}: {case.line[:500]}")
                print(f"  game: {expected[:700]}")
                print(f"  port: {actual[:700]}")
    return mismatches, counts


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument(
        "--checker",
        type=Path,
        default=SOURCE / "build" / "win32" / "bin" / "Debug"
        / "ra3_stringbase2check.exe")
    parser.add_argument("--cases", type=int, default=80)
    parser.add_argument("--seed", type=int, default=0x534232)
    args = parser.parse_args()

    generator = Generator(args.seed)
    state_operations = {
        "release1", "removelast1", "removefirst1", "prepare2", "assign2",
        "setsource2", "concatsource2", "translate1", "translate2", "trim1",
        "token1", "trim2", "format2", "token2", "translateobj2",
        "constructtranslated2", "concatc2", "substringc1", "substringobj1",
        "replace1", "constructchar2", "constructrange2", "substringc2",
        "replace2", "replaceobj2",
    }
    cases: list[Case] = []
    for operation in FUNCTIONS:
        for index in range(args.cases):
            case = (
                generator.state(operation, index)
                if operation in state_operations
                else generator.pure(operation, index))
            cases.append(case)

    outputs = checker_outputs(args.checker, cases)
    original = Original()
    mismatches, counts = compare_cases(original, cases, outputs)

    sensitivity_case = generator.pure("widefold", 1)
    normal = checker_outputs(args.checker, [sensitivity_case])
    mutated = checker_outputs(args.checker, [sensitivity_case], mutated=True)
    expected = original.run(sensitivity_case)
    mutation_detected = mutated[0] != expected
    restored = normal[0] == expected

    for operation, (address, _, name) in FUNCTIONS.items():
        print(f"{address:08x} {operation}: {counts.get(operation, 0)} ({name})")
    print(
        "sensitivity="
        f"{'detected' if mutation_detected else 'missed'} "
        f"restore={'ok' if restored else 'failed'}")
    print(
        f"functions={len(FUNCTIONS)} scenarios={len(cases)} "
        f"adversarial={sum(case.adversarial for case in cases)} "
        f"mismatches={mismatches}")
    return 1 if mismatches or not mutation_detected or not restored else 0


if __name__ == "__main__":
    raise SystemExit(main())
