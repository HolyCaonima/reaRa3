"""Differential oracle for the clean GameLogic AI state-machine cluster."""

from __future__ import annotations

import argparse
import random
import struct
import subprocess
import sys
from dataclasses import dataclass
from pathlib import Path

from unicorn import x86_const as x86

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from emulate import Emulator  # noqa: E402


ARENA_SIZE = 0x1800
THIS_STATE = 0x100
THIS_ADJUSTED = 0x718
THIS_UNIT = 0x100
THIS_WEAPON = 0x100


def u32(value: int) -> int:
    return value & 0xFFFFFFFF


def p32(value: int) -> bytes:
    return struct.pack("<I", u32(value))


def f32(bits: int) -> float:
    return struct.unpack("<f", p32(bits))[0]


def fbits(value: float) -> int:
    return struct.unpack("<I", struct.pack("<f", value))[0]


def put32(data: bytearray, offset: int, value: int) -> None:
    data[offset:offset + 4] = p32(value)


def put8(data: bytearray, offset: int, value: int) -> None:
    data[offset] = value & 0xFF


@dataclass(frozen=True)
class Spec:
    op: str
    address: int
    cleanup: int
    result: str = "u32"  # u32, void, pointer, float, byte-register
    source: str = ""
    iterations: int = 64


SPECS = [
    Spec("dozeractionenter", 0x006819B0, 0),
    Spec("dozeractionexit", 0x00681A00, 4, "void"),
    Spec("stateenter", 0x00681BB0, 0),
    Spec("stateexit", 0x00681BD0, 4, "void"),
    Spec("dozernestedenter", 0x00681C80, 0),
    Spec("dozernestedexit", 0x00681CC0, 4, "void"),
    Spec("dozerhastask", 0x00681E80, 4),
    Spec("dozeranytask", 0x00681E90, 0),
    Spec("dozertarget", 0x00681EC0, 4),
    Spec("dozerrecent", 0x00681F10, 0),
    Spec("dozerdock", 0x00681F60, 8, "pointer"),
    Spec("dozerrepair", 0x00681FA0, 0, "float"),
    Spec("dozerbored", 0x00681FB0, 0, "float"),
    Spec("supplyferry", 0x00682090, 0),
    Spec("supplylose", 0x006820D0, 0, "byte-register", "supplylose"),
    Spec("supplywant", 0x00682170, 0, "byte-register", "module"),
    Spec("supplybusy", 0x00682180, 0, "byte-register", "module"),
    Spec("supplyposition", 0x00682190, 4, "void"),
    Spec("supplyscan", 0x006821B0, 0, "float"),
    Spec("supplyflag6c", 0x006821C0, 0, "byte-register", "module"),
    Spec("supplyflag6d", 0x006821D0, 0, "byte-register", "module"),
    Spec("supplyflag6e", 0x006821E0, 0, "byte-register", "module"),
    Spec("supplyfloat60", 0x006821F0, 0, "float"),
    Spec("supplycenter", 0x00682200, 0),
    Spec("supplywarehouse", 0x00682280, 0),
    Spec("supplybusyenter", 0x00682310, 0),
    Spec("supplyidleenter", 0x00682350, 0),
    Spec("supplydockenter", 0x006823C0, 0),
    Spec("onesecond", 0x00682890, 0),
    Spec("unitset144", 0x00682D30, 4, "void"),
    Spec("unitreset", 0x00682D70, 0, "void"),
    Spec("unitnestedflag", 0x00682DF0, 0, "byte-register", "nested-value"),
    Spec("unitsetgoal", 0x00682E40, 4, "void"),
    Spec("unitcleargoal", 0x00682E70, 0, "void"),
    Spec("unitweaponusable", 0x00682E80, 0, "byte-register", "weapon"),
    Spec("unitclearids", 0x00682F00, 0, "void"),
    Spec("unitmatchids", 0x00682FE0, 4, "byte-register", "queued"),
    Spec("unitstate69", 0x00683030, 0, "byte-register", "state69"),
    Spec("unitvalue250", 0x006830F0, 0),
    Spec("unitstateid", 0x00683100, 0),
    Spec("unitqueued", 0x00683320, 0),
    Spec("unitcurrentid", 0x00683520, 0),
    Spec("unitconditions", 0x00683570, 0, "pointer"),
    Spec("unittransitions", 0x00683580, 0, "pointer"),
    Spec("unitdesired", 0x00683590, 4, "void"),
    Spec("unitscaled", 0x006835B0, 0),
    Spec("unitclearcommand", 0x00683650, 0, "void"),
    Spec("unitdelay", 0x00683670, 0),
    Spec("weaponready", 0x00685CC0, 0),
    Spec("weaponcooldown", 0x00685D20, 0),
    Spec("weaponreload", 0x00685DA0, 0),
]


class StubRuntime:
    def __init__(self, emulator: Emulator) -> None:
        self.emu = emulator
        self.base = 0
        self.interface_offset = 0
        self.worker_result = 0
        self.event = 0
        self.dispatch_addresses: dict[tuple[int, int], int] = {}
        for slot, argc in [
            (0x1BC, 0), (0x1C4, 0), (0x288, 0), (0x48, 1),
            (0x58, 0), (0x28, 1), (0x18, 0), (0x18, 1),
            (0x5C, 1), (0x54, 1),
        ]:
            self.dispatch_addresses[(slot, argc)] = emulator.bind_stub(
                self._dispatch(slot, argc), argc * 4)
        self.ready_address = emulator.bind_stub(self._ready, 4)
        emulator.bind_function(0x0052DDE0, self._supply_changed, 8)

    def reset(self, base: int, interface_offset: int, worker_result: int) -> None:
        self.base = base
        self.interface_offset = interface_offset
        self.worker_result = worker_result
        self.event = 0

    def _mix(self, target: int, slot: int, argument: int) -> None:
        target_offset = target - self.base if self.base <= target < self.base + ARENA_SIZE else target
        self.event = u32((self.event * 16777619) ^ slot ^ (u32(target_offset) << 8) ^ argument)

    def _dispatch(self, slot: int, argc: int):
        def callback(emu: Emulator) -> int:
            target = emu.register(x86.UC_X86_REG_ECX)
            argument = emu.stack_arg(0) if argc else 0
            self._mix(target, slot, argument)
            if slot in (0x1BC, 0x1C4):
                return 0 if self.interface_offset == 0 else self.base + self.interface_offset
            if slot == 0x288:
                return self.worker_result
            if slot == 0x18 and argc == 1 and argument < 3:
                raw = self.emu.read(target + 4 + argument * 8, 4)
                value = struct.unpack("<I", raw)[0]
                return 0xA5000000 | int(value != 0)
            return 0
        return callback

    def _ready(self, emu: Emulator) -> int:
        self.event ^= 0x57000000 ^ emu.stack_arg(0)
        return 0

    def _supply_changed(self, emu: Emulator) -> int:
        maximum = emu.stack_arg(0)
        current = emu.stack_arg(1)
        self.event ^= 0x51000000 ^ maximum ^ u32(current * 0x10001)
        return 0


@dataclass
class Case:
    arena: bytearray
    relocations: list[int]
    ignored: list[tuple[int, int, int]]
    this_offset: int
    stack_args: list[int]
    checker_args: list[int]
    logic_present: bool
    frame: int
    scale_bits: int
    fps_bits: int
    integer_fps: int
    interface_offset: int = 0
    worker_result: int = 0
    source_offset: int | None = None


FLOAT_EDGES = [
    0x00000000, 0x80000000, 0x00000001, 0x007FFFFF,
    0x3F000000, 0x3F800000, 0x41200000, 0x7F7FFFFF,
    0x7F800000, 0xFF800000, 0x7FC00001,
]


def make_case(spec: Spec, iteration: int, rng: random.Random) -> Case:
    arena = bytearray(rng.randbytes(ARENA_SIZE))
    relocs: list[int] = []
    ignored: list[tuple[int, int, int]] = []
    args = [0, 0, 0, 0]
    stack: list[int] = []
    this_offset = THIS_UNIT
    logic = bool(rng.randrange(2))
    frame = rng.getrandbits(32)
    scale_bits = FLOAT_EDGES[iteration % len(FLOAT_EDGES)] if iteration < len(FLOAT_EDGES) else fbits(rng.uniform(0.1, 3.0))
    fps_bits = fbits(float(rng.randrange(2, 121)))
    integer_fps = rng.randrange(2, 121)
    interface_offset = 0
    worker_result = 0
    source_offset: int | None = None

    def pointer(position: int, target: int | None) -> None:
        put32(arena, position, 0 if target is None else target)
        if position not in relocs:
            relocs.append(position)

    def dispatch_object(offset: int, table: int, slots: list[tuple[int, int]]) -> None:
        pointer(offset, table)
        for slot, argc in slots:
            position = table + slot
            put32(arena, position, 0)
            ignored.append((position, slot, argc))

    if spec.address in {
        0x006819B0, 0x00681A00, 0x00681C80, 0x00681CC0,
        0x00682310, 0x00682350, 0x006823C0,
    }:
        this_offset = THIS_STATE
        machine, owner, update, interface, nested = 0x300, 0x500, 0x900, 0xA00, 0xB00
        pointer(this_offset + 0x18, machine)
        pointer(machine + 0x24, owner if iteration % 7 else None)
        pointer(owner + 0x374, update if iteration % 5 else None)
        pointer(this_offset + 0x24, nested)
        interface_offset = 0 if iteration % 6 == 0 else interface
        worker_result = 0 if iteration & 1 else 1
        args[0], args[1] = interface_offset, worker_result
        put32(arena, this_offset + 0x20, rng.randrange(5))
        dispatch_object(update, 0x1000, [
            (0x1BC, 0), (0x1C4, 0), (0x288, 0)])
        dispatch_object(interface, 0x1200, [
            (0x48, 1), (0x58, 0), (0x28, 1), (0x5C, 1), (0x54, 1)])
        dispatch_object(nested, 0x1400, [(0x18, 0)])
        if spec.address in (0x00681A00, 0x00681CC0):
            stack = [rng.getrandbits(32)]
        # Avoid original null-dereference paths while retaining every valid branch.
        if spec.address in (
            0x006819B0, 0x00681A00, 0x00681C80, 0x00681CC0,
            0x00682350, 0x006823C0,
        ):
            pointer(machine + 0x24, owner)
        if spec.address in (0x006819B0, 0x00681C80, 0x00682350, 0x006823C0):
            pointer(owner + 0x374, update)
        if spec.address == 0x006819B0:
            logic = True
        if spec.address in (0x00681C80, 0x00681CC0):
            interface_offset = interface
            args[0] = interface_offset

    elif spec.address in (0x00681BB0, 0x00681BD0):
        this_offset = THIS_STATE
        if spec.address == 0x00681BB0:
            logic = True
        if spec.address == 0x00681BD0:
            put8(arena, this_offset + 0x28, iteration & 1)
            stack = [rng.getrandbits(32)]

    elif 0x00681E80 <= spec.address <= 0x00682280:
        this_offset = THIS_ADJUSTED
        root, module, owner = this_offset - 0x518, 0x1000, 0x1400
        pointer(root, module)
        pointer(root + 4, owner)
        dispatch_object(this_offset, 0x300, [(0x18, 1)])
        for task in range(3):
            put32(arena, this_offset + 4 + task * 8, rng.choice([0, rng.getrandbits(32)]))
            put32(arena, this_offset + 8 + task * 8, rng.getrandbits(32))
        for index in range(9):
            put8(arena, this_offset + 0x2C + index * 0x10, rng.randrange(2))
        if spec.address >= 0x00682090:
            pointer(this_offset + 4, 0x900 if iteration % 4 else None)
            pointer(0x900 + 4, 0xA00 if iteration % 5 else None)
            put32(arena, 0xA00 + 4, rng.randrange(8))
        put32(arena, this_offset + 0x40, rng.randrange(5))
        put32(arena, owner + 0xF4, 0 if iteration & 1 else rng.getrandbits(32) | 1)
        for offset in (0x4C, 0x50, 0x5C, 0x60, 0x64, 0x68):
            put32(arena, module + offset,
                  FLOAT_EDGES[(iteration + offset) % len(FLOAT_EDGES)] if iteration < 12
                  else fbits(rng.uniform(-1000.0, 1000.0)))
        if spec.op in ("supplycenter", "supplywarehouse") and iteration == 0:
            logic = True
            scale_bits = 0x3FFFFFFF
            fps_bits = 0x42EE0000
            put32(arena, module + (0x64 if spec.op == "supplycenter" else 0x68),
                  0x56FFFFFF)
        for offset in (0x6C, 0x6D, 0x6E, 0x6F, 0x70):
            put8(arena, module + offset, rng.randrange(256))
        if spec.op in ("dozerhastask", "dozertarget"):
            args[0] = rng.randrange(3)
            stack = [args[0]]
        elif spec.op == "dozerdock":
            args[0], args[1] = rng.randrange(5), rng.randrange(5)
            stack = [args[0], args[1]]
        elif spec.op == "supplyposition":
            output = (
                this_offset + (0x24, 0x28, 0x20)[iteration % 3]
                if iteration % 4 != 3 else 0x1600
            )
            args[0] = output
            stack = [output]
        if spec.source == "module":
            source_offset = module
        elif spec.source == "supplylose":
            current_boxes = load32(arena, this_offset + 0x40)
            drawable = load32(arena, owner + 0xF4)
            source_offset = None if current_boxes != 0 and drawable != 0 else this_offset

    elif spec.address == 0x00682890:
        this_offset = 0
        if iteration == 0:
            logic = True
            scale_bits = 0x40333333
            fps_bits = fbits(5.0)

    elif 0x00682D30 <= spec.address <= 0x00683670:
        this_offset = THIS_UNIT
        module, machine, current, queued = 0x800, 0x500, 0x700, 0x740
        owner, weapon, level1, level2 = 0xC00, 0x900, 0xA00, 0xB00
        dispatch_object(this_offset, 0x1100, [(0x5C, 1)])
        pointer(this_offset + 4, module)
        pointer(this_offset + 8, owner)
        pointer(this_offset + 0x3C, machine)
        pointer(machine + 4, current if iteration % 5 else None)
        pointer(machine + 0x88, queued if iteration % 4 else None)
        put32(arena, machine + 0x8C, rng.choice([-1, 0, 1]))
        state_ids = [0x1A, 0x69, 0x6A, 999999, rng.randrange(0x80)]
        put32(arena, current + 4, rng.choice(state_ids))
        put32(arena, queued + 4, rng.choice(state_ids))
        pointer(this_offset + 0x214, weapon if iteration % 5 else None)
        pointer(weapon + 4, level1)
        pointer(level1 + 4, level2)
        put32(arena, level2 + 0x64, rng.randrange(7))
        put32(arena, weapon + 0x44, fbits(rng.uniform(-5.0, 5.0)))
        put32(arena, weapon + 0x54, fbits(rng.uniform(-5.0, 5.0)))
        nested, nested_value = 0xD00, 0xE00
        pointer(this_offset + 0x47C, nested if iteration % 4 else None)
        pointer(nested + 0x14, nested_value)
        put8(arena, nested_value + 0x1E, rng.randrange(256))
        put32(arena, module + 0x28,
              FLOAT_EDGES[iteration % len(FLOAT_EDGES)] if iteration < 12
              else fbits(rng.uniform(-100.0, 100.0)))
        put32(arena, module + 0x30,
              FLOAT_EDGES[(iteration + 3) % len(FLOAT_EDGES)] if iteration < 12
              else fbits(rng.uniform(-100.0, 100.0)))
        if spec.op in ("unitscaled", "unitdelay") and iteration == 0:
            logic = True
            scale_bits = 0x3FFFFFFF
            fps_bits = 0x42EE0000
            put32(arena, module + (0x30 if spec.op == "unitscaled" else 0x28),
                  0x56FFFFFF)
        put32(arena, owner + 0xDC, rng.getrandbits(32))
        put32(arena, this_offset + 0x1E8, rng.getrandbits(32))
        put32(arena, this_offset + 0x1EC, rng.getrandbits(32))
        if iteration % 3 == 0:
            put32(arena, this_offset + 0x1E8, load32(arena, owner + 0xDC))
        if spec.op in ("unitset144", "unitdesired"):
            args[0] = rng.getrandbits(32)
            stack = [args[0]]
        elif spec.op == "unitsetgoal":
            source = (this_offset + 0x21C) if iteration % 4 == 0 else 0x1000
            args[0] = source
            stack = [source]
        elif spec.op == "unitmatchids":
            args[0] = owner
            stack = [owner]
        if spec.source == "nested-value":
            source_offset = nested_value
        elif spec.source == "weapon":
            source_offset = weapon
        elif spec.source == "queued":
            source_offset = None if load32(arena, machine + 0x88) == 0 else queued
        elif spec.source == "state69":
            queued_pointer = load32(arena, machine + 0x88)
            current_pointer = load32(arena, machine + 4)
            queued_id = 999999 if queued_pointer == 0 else load32(arena, queued + 4)
            current_id = 999999 if current_pointer == 0 else load32(arena, current + 4)
            source_offset = machine if queued_id in (0x69, 0x6A) or current_id == 0x69 else (
                None if current_pointer == 0 else current)
        # Functions that dereference these fields require a valid instance.
        if spec.op in ("unitconditions", "unittransitions") and iteration % 5 == 0:
            pointer(this_offset + 0x3C, None)
        elif spec.op not in ("unitnestedflag",):
            pointer(this_offset + 0x3C, machine)
        if spec.op == "unitnestedflag" and iteration % 4 == 0:
            source_offset = None
        if spec.op == "unitweaponusable" and iteration % 5 == 0:
            source_offset = None

    elif 0x00685CC0 <= spec.address <= 0x00685DA0:
        this_offset = THIS_WEAPON
        machine, weapon = 0x300, 0x500
        level1, level2 = 0x700, 0x780
        pointer(this_offset + 0x18, machine)
        pointer(this_offset + 0x20, weapon)
        if spec.op in ("weaponcooldown", "weaponreload"):
            logic = True
        pointer(weapon + 4, level1)
        pointer(level1 + 4, level2)
        put8(arena, weapon + 0x60, rng.randrange(2))
        put32(arena, machine + 0x84, rng.getrandbits(32))
        put32(arena, this_offset + 0x24, rng.getrandbits(32))
        put32(arena, this_offset + 0x28, rng.getrandbits(32))
        put32(arena, level2 + 0xB0, rng.randrange(8))

    return Case(
        arena, relocs, ignored, this_offset, stack, args, logic, frame,
        scale_bits, fps_bits, integer_fps, interface_offset, worker_result,
        source_offset)


def load32(data: bytes | bytearray, offset: int) -> int:
    return struct.unpack_from("<I", data, offset)[0]


def call_checker(
    process: subprocess.Popen[str], spec: Spec, case: Case
) -> tuple[int, int, int, bytes]:
    relocs = "-" if not case.relocations else ",".join(f"{item:x}" for item in case.relocations)
    fields = [
        spec.op, case.arena.hex(), relocs, f"{case.this_offset:x}",
        *(f"{u32(value):x}" for value in case.checker_args),
        "1" if case.logic_present else "0", f"{case.frame:x}",
        f"{case.scale_bits:x}", f"{case.fps_bits:x}", f"{case.integer_fps:x}",
    ]
    assert process.stdin is not None and process.stdout is not None
    process.stdin.write(" ".join(fields) + "\n")
    process.stdin.flush()
    output = process.stdout.readline().split()
    if not output or output[0] == "error":
        raise RuntimeError(f"aistatecheck rejected {spec.op}@{spec.address:08x}")
    return int(output[0], 16), int(output[1], 16), int(output[2], 16), bytes.fromhex(output[3])


def execute(
    emu: Emulator, runtime: StubRuntime, spec: Spec, case: Case,
) -> tuple[int, int, bytes, int]:
    emu.reset_data()
    logic = emu.alloc(bytes(0x300), alignment=16)
    emu.write(0x00DEBA64, p32(logic if case.logic_present else 0))
    emu.write(logic + 0x58, p32(case.frame))
    emu.write(logic + 0x1C4, p32(case.scale_bits))
    emu.write(0x00DEEEE8, p32(case.fps_bits))
    fps = f32(case.fps_bits)
    frame_duration = 0.0 if fps == 0.0 else 1.0 / fps
    emu.write(0x00DEEF24, struct.pack("<f", frame_duration))
    emu.write(0x00CB0910, p32(case.integer_fps))

    base = emu.alloc(bytes(case.arena), alignment=0x1000)
    for position in case.relocations:
        target = load32(case.arena, position)
        emu.write(base + position, p32(0 if target == 0 else base + target))

    runtime.reset(base, case.interface_offset, case.worker_result)
    for position, slot, argc in case.ignored:
        address = runtime.dispatch_addresses[(slot, argc)]
        emu.write(base + position, p32(address))

    if spec.op == "weaponready":
        service, table = 0x1500, 0x1600
        emu.write(base + service, p32(base + table))
        emu.write(base + table + 0x6C, p32(runtime.ready_address))
        emu.write(0x00DF03FC, p32(base + service))

    stack_args = [
        base + value if spec.op in ("supplyposition", "unitsetgoal", "unitmatchids") else value
        for value in case.stack_args
    ]
    if spec.result == "float":
        call, result = emu.call_float32(
            spec.address, stack_args, ecx=base + case.this_offset)
    else:
        call = emu.call(spec.address, stack_args, ecx=base + case.this_offset)
        result = call.eax

    game = bytearray(emu.read(base, ARENA_SIZE))
    for position in case.relocations:
        value = load32(game, position)
        if value == 0:
            put32(game, position, 0)
        elif base <= value < base + ARENA_SIZE:
            put32(game, position, value - base)
    for position, _, _ in case.ignored:
        game[position:position + 4] = case.arena[position:position + 4]
    if spec.op == "weaponready":
        game[0x1500:0x1504] = case.arena[0x1500:0x1504]
        game[0x166C:0x1670] = case.arena[0x166C:0x1670]
    if spec.result == "pointer" and base <= result < base + ARENA_SIZE:
        result -= base
    return u32(result), runtime.event, bytes(game), call.stack_cleanup


def compare_result(
    spec: Spec, case: Case, game: int, ours: int,
    game_base: int, checker_base: int,
) -> bool:
    if spec.result == "void":
        return True
    if spec.result != "byte-register":
        return game == ours
    if (game & 0xFF) != (ours & 0xFF):
        return False
    if case.source_offset is None:
        return (game & 0xFFFFFF00) == 0 and (ours & 0xFFFFFF00) == 0
    game_source = game_base + case.source_offset
    checker_source = checker_base + case.source_offset
    return ((game & 0xFFFFFF00) == (game_source & 0xFFFFFF00) and
            (ours & 0xFFFFFF00) == (checker_source & 0xFFFFFF00))


def run_oracle(
    checker: Path, specs: list[Spec], seed: int, quiet: bool = False,
) -> tuple[int, int]:
    rng = random.Random(seed)
    emu = Emulator()
    runtime = StubRuntime(emu)
    process = subprocess.Popen(
        [str(checker.resolve())], stdin=subprocess.PIPE, stdout=subprocess.PIPE,
        text=True, encoding="ascii")
    total = bad = 0
    for spec in specs:
        local_bad = 0
        for iteration in range(spec.iterations):
            case = make_case(spec, iteration, rng)
            game, game_event, game_blob, cleanup = execute(emu, runtime, spec, case)
            ours, ours_event, checker_base, ours_blob = call_checker(process, spec, case)
            game_base = 0x60001000  # logic allocation consumes the first aligned block.
            mismatch = (
                game_blob != ours_blob or game_event != ours_event or
                cleanup != spec.cleanup or
                not compare_result(spec, case, game, ours, game_base, checker_base)
            )
            if mismatch:
                local_bad += 1
                if local_bad <= 2 and not quiet:
                    print(
                        f"{spec.op}@{spec.address:08x} mismatch iteration={iteration} "
                        f"eax={game:08x}/{ours:08x} event={game_event:08x}/{ours_event:08x} "
                        f"cleanup={cleanup}/{spec.cleanup} writes={game_blob == ours_blob}")
            total += 1
        bad += local_bad
        if not quiet:
            print(f"{spec.op}@{spec.address:08x}: {spec.iterations} mismatches={local_bad}")
    assert process.stdin is not None
    process.stdin.close()
    process.wait(timeout=20)
    return total, bad


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument(
        "--checker", type=Path,
        default=Path("build/win32/bin/Debug/ra3_aistatecheck.exe"))
    parser.add_argument("--mutation-checker", type=Path)
    parser.add_argument("--seed", type=int, default=0x681A2026)
    args = parser.parse_args()

    if args.mutation_checker is not None:
        mutation_spec = [Spec("unitreset", 0x00682D70, 0, "void", iterations=16)]
        mutation_total, mutation_bad = run_oracle(
            args.mutation_checker, mutation_spec, args.seed ^ 0xBAD, quiet=True)
        print(
            f"mutation proof: cases={mutation_total} detected={mutation_bad} "
            f"status={'PASS' if mutation_bad == mutation_total else 'FAIL'}")
        if mutation_bad != mutation_total:
            return 1

    total, bad = run_oracle(args.checker, SPECS, args.seed)
    print(f"AI state oracle: functions={len(SPECS)} cases={total} mismatches={bad}")
    return 1 if bad else 0


if __name__ == "__main__":
    raise SystemExit(main())
