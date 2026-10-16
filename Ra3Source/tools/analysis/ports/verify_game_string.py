"""Run the original and reconstructed engine string functions on random scenarios.

A scenario is a set of string blocks (reference count, length, capacity and
the full character area including garbage past the terminator) and string
objects pointing into them, possibly sharing a block. Each original function
runs in the emulator with the class lock, allocator, free, memcpy and CRT case
mappers stubbed; ra3_stringcheck runs the port with equivalent bindings. Both
sides report the ordered lock/allocate/free trace, where every object points
and the header and bytes of every live block.

Paths the original sends to SecuROM inner-patched code (oversized requests,
ASCII concat onto an empty string) are not executed: the port must decline
them with no side effects so the hook can hand the call to the original.
"""

from __future__ import annotations

import argparse
import random
import struct
import subprocess
import sys
from dataclasses import dataclass, field
from pathlib import Path
from typing import Optional

from unicorn import x86_const as r

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))

from emulate import Emulator  # noqa: E402


FUNCTIONS = {
    1: {
        "copy": (0x004CA8F0, 4),
        "assign": (0x004CA9B0, 4),
        "ensure": (0x004CA710, 16),
        "lower": (0x004CAA30, 0),
        "upper": (0x004CAA80, 0),
        "concat": (0x004CB6E0, 8),
    },
    2: {
        "copy": (0x004CAD90, 4),
        "release": (0x004CAB60, 0),
        "ensure": (0x004CABA0, 16),
        "lower": (0x004CAF40, 0),
        "removelast": (0x004CAF90, 0),
        "set": (0x004CB9B0, 8),
        "concat": (0x004CBA00, 8),
    },
}
LOCK_GETTER = {1: 0x004CA440, 2: 0x004CA490}
TEXT_SOURCE_VTABLE = {1: 0x00BDAB6C, 2: 0x00BDAB7C}
ALLOCATE = 0x004165A0
FREE = 0x004166B0
LEAVE_CRITICAL_SECTION = 0x00BBA1E8
MEMCPY = 0x00BBA53C
TOLOWER = 0x00BBA4D4
TOUPPER = 0x00BBA4C0
TOWLOWER = 0x00BBA4C8
POOL_SIZE = 4 << 20

ASCII_CHARS = list(range(0x20, 0x7F)) + [0x80, 0xC4, 0xD6, 0xE4, 0xFF]
UNICODE_CHARS = list(range(0x20, 0x7F)) + [
    0x80, 0xC4, 0xE4, 0x130, 0x3A3, 0x3C3, 0x410, 0x430, 0x4E2D, 0xFF21, 0xFF41, 0xFFFF]


@dataclass
class BlockSpec:
    refcount: int
    length: int
    capacity: int
    area: bytes


@dataclass
class Scenario:
    width: int
    op: str
    blocks: list[BlockSpec]
    slots: list[int]
    args: list = field(default_factory=list)
    delegated: bool = False

    def line(self) -> str:
        parts = [str(self.width), self.op, str(len(self.blocks))]
        for block in self.blocks:
            parts += [str(block.refcount), str(block.length), str(block.capacity),
                      block.area.hex()]
        parts.append(str(len(self.slots)))
        parts += [str(slot) for slot in self.slots]
        parts += [str(arg) for arg in self.args]
        return " ".join(parts)


def encode_text(width: int, chars: Optional[list[int]]) -> str:
    if chars is None:
        return "-"
    if not chars:
        return "."
    return b"".join(c.to_bytes(width, "little") for c in chars).hex()


def decode_text(text: str) -> Optional[bytes]:
    if text == "-":
        return None
    if text == ".":
        return b""
    return bytes.fromhex(text)


class Original:
    def __init__(self) -> None:
        self.emu = Emulator()
        emu = self.emu
        self.pool = emu.alloc(bytes(POOL_SIZE))
        self.cursor = self.pool
        self.trace: list[str] = []
        self.blocks: list[tuple[int, int, str]] = []
        self.new_blocks = 0
        self.faults: list[str] = []

        def lock(machine: Emulator) -> int:
            if machine.register(r.UC_X86_REG_ECX) != self.lock or machine.stack_arg(0) != 0xFFFFFFFF:
                self.faults.append("bad lock call")
            self.trace.append("L")
            return 0

        def unlock(machine: Emulator) -> int:
            if machine.stack_arg(0) != self.lock + 8:
                self.faults.append("bad unlock call")
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
            self.trace.append("F" + self.block_id(machine.stack_arg(0), 0))
            return 0

        def memcpy(machine: Emulator) -> int:
            destination, source, count = (machine.stack_arg(i) for i in range(3))
            if count:
                emu.write(destination, emu.read(source, count))
            return destination

        def lower(machine: Emulator) -> int:
            c = machine.stack_arg(0)
            return c + 0x20 if 0x41 <= c <= 0x5A else c

        def upper(machine: Emulator) -> int:
            c = machine.stack_arg(0)
            return c - 0x20 if 0x61 <= c <= 0x7A else c

        lock_vtable = emu.alloc(struct.pack("<I", emu.bind_stub(lock, 4)))
        self.lock = emu.alloc(struct.pack("<I", lock_vtable) + bytes(0x1C))
        for getter in LOCK_GETTER.values():
            emu.bind_function(getter, lambda machine: self.lock)
        emu.bind_import(LEAVE_CRITICAL_SECTION, unlock, 4)
        emu.bind_function(ALLOCATE, allocate)
        emu.bind_function(FREE, free)
        emu.bind_import(MEMCPY, memcpy)
        emu.bind_import(TOLOWER, lower)
        emu.bind_import(TOUPPER, upper)
        emu.bind_import(TOWLOWER, lower)

    def take(self, data: bytes) -> int:
        address = (self.cursor + 15) & ~15
        if address + len(data) > self.pool + POOL_SIZE:
            raise RuntimeError("scenario pool exhausted")
        self.emu.write(address, data)
        self.cursor = address + max(len(data), 1)
        return address

    def block_id(self, address: int, bias: int) -> str:
        if address == 0:
            return "0"
        for base, _, name in self.blocks:
            if address == base + bias:
                return name
        return "?"

    def text(self, width: int, encoded: str) -> int:
        data = decode_text(encoded)
        if data is None:
            return 0
        return self.take(data + bytes(width))

    def run(self, scenario: Scenario) -> str:
        self.cursor = self.pool
        self.trace = []
        self.blocks = []
        self.new_blocks = 0
        self.faults = []
        width = scenario.width
        for index, block in enumerate(scenario.blocks):
            address = self.take(
                struct.pack("<iHH", block.refcount, block.length, block.capacity) + block.area)
            self.blocks.append((address, len(block.area), f"b{index}"))
        slots = self.take(bytes(4 * len(scenario.slots)))
        for index, block in enumerate(scenario.slots):
            data = 0 if block < 0 else self.blocks[block][0] + 8
            self.emu.write(slots + 4 * index, struct.pack("<I", data))

        address, cleanup = FUNCTIONS[width][scenario.op]
        args = scenario.args
        self_slot = slots + 4 * args[0]
        if scenario.op in ("copy", "assign"):
            stack = [slots + 4 * args[1]]
        elif scenario.op == "ensure":
            sources = []
            for encoded in args[3:5]:
                text = decode_text(encoded)
                if text is None:
                    sources.append(0)
                    continue
                sources.append(self.take(struct.pack(
                    "<III", TEXT_SOURCE_VTABLE[width], self.text(width, encoded),
                    len(text) // width)))
            stack = [args[1], args[2]] + sources
        elif scenario.op in ("set", "concat"):
            if args[1] == "self":
                text = struct.unpack("<I", self.emu.read(self_slot, 4))[0]
            else:
                text = self.text(width, args[1])
            stack = [text, args[2]]
        else:
            stack = []
        result = self.emu.call(address, stack, ecx=self_slot)
        if result.stack_cleanup != cleanup:
            raise AssertionError(f"{scenario.op}: cleanup {result.stack_cleanup}")
        if self.faults:
            raise AssertionError(f"{scenario.op}: {self.faults}")
        if scenario.op == "copy":
            returned = "self" if result.eax == self_slot else "other"
        else:
            returned = "1"
        return self.describe(returned, slots, len(scenario.slots))

    def describe(self, returned: str, slots: int, slot_count: int) -> str:
        freed = {entry[1:] for entry in self.trace if entry.startswith("F")}
        slot_ids = [
            self.block_id(struct.unpack("<I", self.emu.read(slots + 4 * i, 4))[0], 8)
            for i in range(slot_count)]
        blocks = []
        for base, area, name in self.blocks:
            if name in freed:
                continue
            refcount, length, capacity = struct.unpack("<iHH", self.emu.read(base, 8))
            blocks.append(
                f"{name}:{refcount}:{length}:{capacity}:{self.emu.read(base + 8, area).hex()}")
        return (f"ret={returned} trace={','.join(self.trace) or '-'} "
                f"slots={','.join(slot_ids)} blocks={';'.join(blocks)}")


def initial_state(scenario: Scenario) -> str:
    blocks = [
        f"b{i}:{b.refcount}:{b.length}:{b.capacity}:{b.area.hex()}"
        for i, b in enumerate(scenario.blocks)]
    slots = ["0" if slot < 0 else f"b{slot}" for slot in scenario.slots]
    return f"ret=0 trace=- slots={','.join(slots)} blocks={';'.join(blocks)}"


class Generator:
    def __init__(self, seed: int) -> None:
        self.rng = random.Random(seed)

    def chars(self, width: int, count: int) -> list[int]:
        alphabet = ASCII_CHARS if width == 1 else UNICODE_CHARS
        return [self.rng.choice(alphabet) for _ in range(count)]

    def block(self, width: int) -> BlockSpec:
        rng = self.rng
        capacity = rng.choice([1, 2, 3, 4, 5, 7, 8, 9, 16, 31, 32, 33, 60, 100])
        length = rng.randrange(capacity)
        area = b"".join(c.to_bytes(width, "little") for c in self.chars(width, length))
        area += bytes(width)
        area += bytes(rng.randrange(256) for _ in range((capacity - length - 1) * width))
        return BlockSpec(rng.choice([1, 1, 1, 2, 3]), length, capacity, area)

    def scenario(self, width: int, op: str) -> Scenario:
        rng = self.rng
        blocks = [self.block(width) for _ in range(rng.randint(1, 3))]
        slots = [rng.randrange(-1, len(blocks)) for _ in range(rng.randint(2, 3))]
        if rng.random() < 0.3:
            slots[1] = slots[0]
        scenario = Scenario(width, op, blocks, slots)
        target = rng.randrange(len(slots))
        data = None if slots[target] < 0 else blocks[slots[target]]
        old_length = 0 if data is None else data.length

        if op in ("copy", "assign"):
            source = target if rng.random() < 0.15 else rng.randrange(len(slots))
            scenario.args = [target, source]
        elif op in ("release", "lower", "upper", "removelast"):
            scenario.args = [target]
        elif op == "ensure":
            preserve = rng.randint(0, 1)
            copy = None if rng.random() < 0.5 else self.chars(width, rng.choice([0, 1, 3, 10, 40]))
            cat = None if rng.random() < 0.5 else self.chars(width, rng.choice([0, 1, 3, 10, 40]))
            final = max(old_length, len(copy) if copy is not None else 0) + (
                len(cat) if cat is not None else 0)
            choices = [final, final + 1, final + rng.randrange(1, 50)]
            if data is not None:
                choices += [c for c in (data.capacity - 1, data.capacity) if c >= final]
            needed = rng.choice(choices)
            scenario.args = [target, needed, preserve,
                             encode_text(width, copy), encode_text(width, cat)]
        elif op == "set":
            if data is not None and rng.random() < 0.15:
                scenario.args = [target, "self", rng.randint(0, old_length)]
            else:
                text = self.chars(width, rng.choice([0, 1, 2, 5, 20, 60]))
                scenario.args = [target, encode_text(width, text), len(text)]
        elif op == "concat":
            text = self.chars(width, rng.choice([0, 1, 2, 5, 20, 60]))
            scenario.args = [target, encode_text(width, text), len(text)]
            scenario.delegated = width == 1 and data is None and text != []
        return scenario


def edge_scenarios(generator: Generator) -> list[Scenario]:
    edges = []
    # Largest accepted requests and the first ones the port must decline.
    for width, needed, delegated in ((1, 0x7FF6, False), (1, 0x7FF7, True),
                                     (2, 0x3FFA, False), (2, 0x3FFB, True)):
        for slot in (-1, 0):
            scenario = generator.scenario(width, "ensure")
            scenario.slots = [slot, -1]
            scenario.args = [0, needed, 1, "-", "-"]
            scenario.delegated = delegated
            edges.append(scenario)
    # Concatenation growth onto a full, unshared block.
    for width in (1, 2):
        for capacity in (1, 2, 3, 4, 33):
            block = generator.block(width)
            block.refcount = 1
            block.capacity = capacity
            block.length = capacity - 1
            chars = generator.chars(width, block.length)
            block.area = b"".join(c.to_bytes(width, "little") for c in chars) + bytes(width)
            scenario = Scenario(width, "concat", [block], [0, -1])
            text = generator.chars(width, 1)
            scenario.args = [0, encode_text(width, text), 1]
            edges.append(scenario)
    return edges


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument(
        "--stringcheck", type=Path,
        default=Path("build/win32/bin/Debug/ra3_stringcheck.exe"))
    parser.add_argument("--cases", type=int, default=1000)
    parser.add_argument("--seed", type=int, default=0x5354)
    args = parser.parse_args()

    generator = Generator(args.seed)
    scenarios = edge_scenarios(generator)
    for width, functions in FUNCTIONS.items():
        for op in functions:
            scenarios += [generator.scenario(width, op) for _ in range(args.cases)]

    process = subprocess.run(
        [str(args.stringcheck.resolve())],
        input="".join(s.line() + "\n" for s in scenarios),
        capture_output=True, text=True, check=True)
    ours = process.stdout.splitlines()
    if len(ours) != len(scenarios):
        raise AssertionError(f"stringcheck answered {len(ours)} of {len(scenarios)} scenarios")

    original = Original()
    counts: dict[str, int] = {}
    mismatches = 0
    for scenario, port in zip(scenarios, ours):
        expected = initial_state(scenario) if scenario.delegated else original.run(scenario)
        name = f"{'ascii' if scenario.width == 1 else 'unicode'}.{scenario.op}"
        counts[name] = counts.get(name, 0) + 1
        if expected != port:
            mismatches += 1
            if mismatches <= 5:
                print(f"MISMATCH {name}: {scenario.line()[:400]}")
                print(f"  game: {expected[:400]}")
                print(f"  port: {port[:400]}")
    for name in sorted(counts):
        print(f"{name}: {counts[name]}")
    print(f"scenarios={len(scenarios)} mismatches={mismatches}")
    return 1 if mismatches else 0


if __name__ == "__main__":
    raise SystemExit(main())
