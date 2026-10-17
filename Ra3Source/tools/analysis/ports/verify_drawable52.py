"""Release oracle for the Drawable accessor cluster in 0x00520000-0x0052FFFF.

The checker is the reconstruction.  This script is the shipped image.  Tags
name pointers so the two processes can be compared without sharing addresses.
AH-only and AL-only residues use different masks; an AF-only lahf difference
is reported rather than hidden.
"""

from __future__ import annotations

import struct
import subprocess
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from emulate import STACK_BASE, STACK_SIZE, Emulator  # noqa: E402
from unicorn import x86_const as uc  # noqa: E402

GAME = Path(r"D:\Game\Ra3\Game\Data\ra3_1.0.game")
INCOMING = 0xA5A5A500
POSITION = 0x00DEED68
TRANSFORM = 0x00DEEDC0
LOGIC = 0x00DEBA64


def token(value: int, names: list[tuple[str, int]]) -> str:
    for name, address in names:
        if value == address & 0xFFFFFFFF:
            return name
    if value > 0xFFFF and value != 0x00BDFEAC:
        return "p"
    return format(value & 0xFFFFFFFF, "x")


def tag_of(eax: int, exact: list[tuple[str, int]], ah_bases: list[tuple[str, int]], incoming: int) -> str:
    for name, address in exact:
        if eax == address & 0xFFFFFFFF:
            return name
    for name, address in ah_bases:
        base = address & 0xFFFFFFFF
        if eax != base and (eax & 0xFFFF00FF) == (base & 0xFFFF00FF):
            return f"{name}-ah:{(eax >> 8) & 0xFF:02x}"
    for name, address in ah_bases:
        base = address & 0xFFFFFFFF
        if eax != base and (eax & 0xFFFFFF00) == (base & 0xFFFFFF00):
            return f"{name}-al:{eax & 0xFF:02x}"
    if (eax & 0xFFFFFF00) == (incoming & 0xFFFFFF00):
        return f"in-al:{eax & 0xFF:02x}"
    return f"imm:{eax & 0xFFFFFFFF:08x}"


class Checker:
    def __init__(self, executable: Path) -> None:
        self.process = subprocess.Popen(
            [str(executable)], stdin=subprocess.PIPE, stdout=subprocess.PIPE, text=True, bufsize=1)

    def call(self, line: str) -> str:
        assert self.process.stdin is not None and self.process.stdout is not None
        self.process.stdin.write(line + "\n")
        self.process.stdin.flush()
        result = self.process.stdout.readline().strip()
        if not result or result.startswith("error"):
            raise RuntimeError(f"drawable52check rejected {line!r}: {result!r}")
        return result

    def close(self) -> None:
        assert self.process.stdin is not None
        self.process.stdin.write("quit\n")
        self.process.stdin.flush()
        if self.process.wait(timeout=20) != 0:
            raise RuntimeError("drawable52check failed")


class World:
    def __init__(self) -> None:
        self.results: list[int] = []
        self.names: list[tuple[str, int]] = []
        self.trace = ""
        self.stamp = False
        self.version_byte = -1

    def take(self) -> int:
        if not self.results:
            return 0
        return self.results.pop(0)

    def note(self, kind: str, ident: int, ecx: int, args: list[int]) -> None:
        text = f"{kind}{ident:x}@{token(ecx, self.names)}"
        for arg in args:
            text += ":" + token(arg, self.names)
        self.trace += text + ";"


def parse(line: str) -> tuple[str, str, str]:
    fields = dict(part.split("=", 1) for part in line.split())
    return fields["tag"], fields["mem"], fields["trace"]


def plant(emu: Emulator, world: World, slots: dict[int, tuple[int, int]]) -> int:
    if not slots:
        return emu.alloc(b"\x00\x00\x00\x00")
    size = max(slots) + 4
    table = bytearray(size)

    def make(slot: int, nargs: int, cleanup: int):
        def stub(machine: Emulator) -> int:
            ecx = machine.register(uc.UC_X86_REG_ECX)
            args = [machine.stack_arg(index) for index in range(nargs)]
            if world.stamp and slot in (0x58, 0x64, 0x6C, 0x80, 0x84) and nargs == 1:
                machine.write(args[0], struct.pack("<I", 0xA1000000 | slot))
            if slot == 0x18 and world.version_byte >= 0 and nargs == 1:
                data = bytearray(machine.read(args[0], 2))
                data[1] = world.version_byte & 0xFF
                machine.write(args[0], bytes(data))
            world.note("s", slot, ecx, args)
            return world.take()

        return stub, cleanup

    for slot, (nargs, cleanup) in slots.items():
        stub, cleanup = make(slot, nargs, cleanup)
        struct.pack_into("<I", table, slot, emu.bind_stub(stub, cleanup))
    return emu.alloc(bytes(table))


def bind_direct(emu: Emulator, world: World, address: int, nargs: int, cleanup: int) -> None:
    def stub(machine: Emulator) -> int:
        ecx = machine.register(uc.UC_X86_REG_ECX)
        args = [machine.stack_arg(index) for index in range(nargs)]
        world.note("d", address, ecx, args)
        return world.take()

    emu.bind_function(address, stub, stack_cleanup=cleanup)


def check(label: str, native_tag: str, native_mem: str, native_trace: str, checker_line: str,
          cleanup: int, got_cleanup: int) -> None:
    tag, mem, trace = parse(checker_line)
    if tag != native_tag or mem != native_mem or trace != native_trace or cleanup != got_cleanup:
        raise SystemExit(
            f"{label}\n"
            f"  native tag={native_tag} mem={native_mem} trace={native_trace} clean={got_cleanup}\n"
            f"  checker tag={tag} mem={mem} trace={trace} clean={cleanup}")


def u32(value: int) -> bytes:
    return struct.pack("<I", value & 0xFFFFFFFF)


def f32(value: float) -> bytes:
    return struct.pack("<f", value)


def compare(label, emu, checker, missed, covered, va, command, stack, ecx, cleanup, exact, ah, slices,
             world=None, eax=INCOMING):
    covered.add(f"{va:08x}")
    emu.uc.reg_write(uc.UC_X86_REG_FPCW, 0x027F)
    result = emu.call(va, stack, ecx=ecx, eax=eax)
    native_trace = "-" if world is None or not world.trace else world.trace
    native_mem = "-" if not slices else b"".join(emu.read(addr, size) for addr, size in slices).hex()
    line = checker.call(command)
    tag, got, trace = parse(line)
    native_tag = tag_of(result.eax, exact, ah, eax)
    if tag != native_tag or got != native_mem or trace != native_trace or result.stack_cleanup != cleanup:
        missed["n"] += 1
        print(label)
        print(f"  native tag={native_tag} mem={native_mem} trace={native_trace} clean={result.stack_cleanup}")
        print(f"  checker tag={tag} mem={got} trace={trace} clean={cleanup}")


def run_rest(emu, checker, missed, covered) -> None:
    def position(field, ready):
        emu.reset_data()
        obj = emu.alloc(b"\xA5" * 0x140)
        emu.write(obj + 0x130, u32(field & 0xFFFFFFFF))
        emu.write(POSITION, f32(4) + f32(5) + f32(6) + u32(ready & 0xFFFFFFFF))
        compare(
            f"position {field} {ready}", emu, checker, missed, covered, 0x00525CF0,
            f"position {field if field != 0xFFFFFFFF else -1} {ready}", [], obj, 0,
            [("self+124", obj + 0x124), ("pos", POSITION)], [], [(POSITION, 16)])

    for field, ready in ((0, 0), (0xFFFFFFFF, 0), (0xFFFFFFFF, 1), (0xFFFFFFFF, 256)):
        position(field, ready)

    def transform(field, ready):
        emu.reset_data()
        obj = emu.alloc(b"\xA5" * 0x140)
        emu.write(obj + 0x130, u32(field & 0xFFFFFFFF))
        emu.write(TRANSFORM, f32(9) * 12 + u32(ready & 0xFFFFFFFF))
        compare(
            f"transform {field} {ready}", emu, checker, missed, covered, 0x0052D790,
            f"transform {field if field != 0xFFFFFFFF else -1} {ready}", [], obj, 0,
            [("self+f4", obj + 0xF4), ("xfm", TRANSFORM)], [], [(TRANSFORM, 52)])

    for field, ready in ((3, 0), (0xFFFFFFFF, 0), (0xFFFFFFFF, 1), (0xFFFFFFFF, 256)):
        transform(field, ready)

    def modules(label, va, command, slots, count, results, cleanup, second=0, stack=None):
        emu.reset_data()
        world = World()
        world.results = list(results)
        vtable = plant(emu, world, slots)
        bodies = [emu.alloc(u32(vtable)) for _ in range(max(count, 1))]
        listing = emu.alloc(b"".join(u32(body) for body in bodies[:count]) + u32(0))
        obj = emu.alloc(b"\xA5" * 0x400)
        emu.write(obj + 0x178, u32(listing))
        for index, body in enumerate(bodies[:count]):
            world.names.append((f"m{index}", body))
        world.names.append(("list", listing))
        if second or "scalar" in command:
            second_bodies = [emu.alloc(u32(vtable)) for _ in range(second)]
            second_list = emu.alloc(b"".join(u32(body) for body in second_bodies) + u32(0))
            emu.write(obj + 0x180, u32(second_list))
            for index, body in enumerate(second_bodies):
                world.names.append((f"n{index}", body))
        inner = emu.alloc(u32(vtable))
        world.inner = inner
        world.names.append(("i0", inner))
        old_take = world.take

        def take() -> int:
            value = old_take()
            if value == 1:
                return inner
            return value

        world.take = take  # type: ignore[method-assign]
        compare(
            label, emu, checker, missed, covered, va, command, stack or [], obj, cleanup,
            [("in", INCOMING), ("list", listing)], [],
            [(obj + 0x13C, 4), (obj + 0x25A, 2)], world)

    modules("slot20 empty", 0x005209B0, "slot20 0 0", {0x20: (0, 0)}, 0, [], 0)
    modules("slot20 one", 0x005209B0, "slot20 1 1 11", {0x20: (0, 0)}, 1, [0x11], 0)
    modules("slot20 two", 0x005209B0, "slot20 2 2 11 22", {0x20: (0, 0)}, 2, [0x11, 0x22], 0)
    modules("query empty", 0x00520F50, "query 0 0 7", {0xB0: (0, 0), 0x70: (1, 4)}, 0, [], 4, stack=[7])
    modules("query zero", 0x00520F50, "query 1 1 0 7", {0xB0: (0, 0), 0x70: (1, 4)}, 1, [0], 4, stack=[7])
    modules("query hit", 0x00520F50, "query 1 2 1 55 7", {0xB0: (0, 0), 0x70: (1, 4)}, 1, [1, 0x55], 4, stack=[7])
    modules(
        "scalar both", 0x00525BD0, "scalar 1 1 2 11 22 3 4 3f800000",
        {0xA4: (3, 12), 0x30: (3, 12)}, 1, [0x11, 0x22], 12, second=1, stack=[3, 4, 0x3F800000])
    modules("scalar first", 0x00525BD0, "scalar 1 0 1 11 9 8 0", {0xA4: (3, 12), 0x30: (3, 12)}, 1, [0x11], 12, stack=[9, 8, 0])
    modules("scalar second", 0x00525BD0, "scalar 0 1 1 33 1 2 1", {0xA4: (3, 12), 0x30: (3, 12)}, 0, [0x33], 12, second=1, stack=[1, 2, 1])
    modules("scalar empty", 0x00525BD0, "scalar 0 0 0 5 6 2", {0xA4: (3, 12), 0x30: (3, 12)}, 0, [], 12, stack=[5, 6, 2])

    def condition(label, command, count, results, flags, has_object):
        emu.reset_data()
        world = World()
        world.results = list(results)
        vtable = plant(emu, world, {0x90: (1, 4)})
        bodies = [emu.alloc(u32(vtable)) for _ in range(count)]
        listing = emu.alloc(b"".join(u32(body) for body in bodies) + u32(0))
        obj = emu.alloc(b"\xA5" * 0x400)
        game = emu.alloc(b"\x11\x00\x00\x00")
        emu.write(obj + 0x178, u32(listing))
        for index, body in enumerate(bodies):
            world.names.append((f"m{index}", body))
        world.names.append(("list", listing))
        if has_object:
            emu.write(obj + 0x138, u32(game))
            world.names.append(("obj", game))
            bind_direct(emu, world, 0x006A54D0, 0, 0)
        emu.write(obj + 0x25B, bytes([flags[0]]))
        emu.write(obj + 0x13D, bytes([flags[1]]))
        emu.write(obj + 0x13E, bytes([flags[2]]))
        emu.write(obj + 0x25A, bytes([flags[3]]))
        compare(
            label, emu, checker, missed, covered, 0x00524050, command, [], obj, 0,
            [("in", INCOMING), ("list", listing)], [],
            [(obj + 0x13C, 4), (obj + 0x25A, 2)], world)

    condition("cond byte", "condition 1 1 11 5 0 0 0 0", 1, [0x11], (5, 0, 0, 0), False)
    condition("cond query", "condition 1 2 1 11 0 64 0 0 1", 1, [1, 0x11], (0, 0x40, 0, 0), True)
    condition("cond hidden", "condition 1 1 11 0 0 64 7 0", 1, [0x11], (0, 0, 0x40, 7), False)
    condition("cond field", "condition 1 1 11 0 0 0 7 0", 1, [0x11], (0, 0, 0, 7), False)
    condition("cond empty", "condition 0 0 5 0 0 0 0", 0, [], (5, 0, 0, 0), False)

    def argument(label, command, count, second, results, arg):
        emu.reset_data()
        world = World()
        world.results = list(results)
        vtable = plant(emu, world, {0x80: (1, 4), 0x50: (0, 0)})
        bodies = [emu.alloc(u32(vtable)) for _ in range(count)]
        listing = emu.alloc(b"".join(u32(body) for body in bodies) + u32(0))
        obj = emu.alloc(b"\xA5" * 0x400)
        emu.write(obj + 0x178, u32(listing))
        for index, body in enumerate(bodies):
            world.names.append((f"m{index}", body))
        world.names.append(("list", listing))
        if second:
            extra = [emu.alloc(u32(vtable)) for _ in range(second)]
            second_list = emu.alloc(b"".join(u32(body) for body in extra) + u32(0))
            emu.write(obj + 0x180, u32(second_list))
            for index, body in enumerate(extra):
                world.names.append((f"n{index}", body))
        else:
            emu.write(obj + 0x180, u32(0))
        compare(
            label, emu, checker, missed, covered, 0x00524010, command, [arg], obj, 4,
            [("in", INCOMING), ("list", listing)], [],
            [(obj + 0x13C, 4), (obj + 0x25A, 2)], world)

    argument("arg empty", "argument 0 0 0 9", 0, 0, [], 9)
    argument("arg first", "argument 1 0 1 11 9", 1, 0, [0x11], 9)
    argument("arg second", "argument 0 1 1 22 9", 0, 1, [0x22], 9)

    def assign(which, label, command, count, results, arg, old_a, old_b):
        emu.reset_data()
        world = World()
        world.results = list(results)
        vtable = plant(emu, world, {0x90: (1, 4)})
        bodies = [emu.alloc(u32(vtable)) for _ in range(count)]
        listing = emu.alloc(b"".join(u32(body) for body in bodies) + u32(0))
        obj = emu.alloc(b"\xA5" * 0x400)
        emu.write(obj + 0x178, u32(listing))
        emu.write(obj + 0x13D, b"\x00")
        emu.write(obj + 0x13E, b"\x00")
        emu.write(obj + 0x25A, bytes([old_a]))
        emu.write(obj + 0x25B, bytes([old_b]))
        for index, body in enumerate(bodies):
            world.names.append((f"m{index}", body))
        compare(
            label, emu, checker, missed, covered, which, command, [arg], obj, 4,
            [("in", INCOMING)], [], [(obj + 0x13C, 4), (obj + 0x25A, 2)], world)

    assign(0x0052B960, "seta equal", "seta 0 0 5 5 0", 0, [], 5, 5, 0)
    assign(0x0052B960, "seta change", "seta 1 1 11 7 0 3", 1, [0x11], 7, 0, 3)
    assign(0x0052B960, "seta high", "seta 0 0 256 0 1", 0, [], 256, 0, 1)
    assign(0x0052B980, "setb equal", "setb 0 0 4 0 4", 0, [], 4, 0, 4)
    assign(0x0052B980, "setb change", "setb 1 1 12 1 0 0", 1, [0x12], 1, 0, 0)

    def sway_objects(mode: int):
        self = emu.alloc(b"\xA5" * 0x220)
        sway = emu.alloc(b"\xA5" * 0x70)
        record = emu.alloc(b"\x00" * 8)
        middle = emu.alloc(b"\x00" * 8)
        inner = emu.alloc(b"\xA5" * 0x110)
        emu.write(self + 0x208, u32(sway))
        emu.write(record + 4, u32(middle))
        emu.write(middle + 4, u32(inner))
        emu.write(sway + 0x18, f32(0.25))
        emu.write(sway + 0x34, f32(0.5))
        emu.write(sway + 0x38, f32(0.75))
        point = emu.alloc(f32(3.0) + f32(4.0))
        emu.write(inner + 0x104, f32(0.0))
        emu.write(inner + 0x108, f32(0.0))
        emu.write(inner + 0xFC, f32(0.0))
        emu.write(inner + 0x100, f32(0.0))
        if mode in (1, 3):
            emu.write(inner + 0x104, f32(0.5))
            emu.write(inner + 0x108, f32(0.25))
        if mode in (2, 3):
            emu.write(inner + 0xFC, f32(0.5))
            emu.write(inner + 0x100, f32(0.25))
        return self, sway, record, inner, point

    for mode in (0, 1, 2, 3):
        emu.reset_data()
        self, sway, record, inner, point = sway_objects(mode)
        compare(
            f"sway {mode}", emu, checker, missed, covered, 0x00520170, f"sway {mode}",
            [record, point], self, 8,
            [("sway", sway), ("rec", record)],
            [("sway", sway), ("inner", inner), ("rec", record)],
            [(sway + 0x34, 8), (point, 8)])

    def angle_objects(mode: int):
        self = emu.alloc(b"\xA5" * 0x220)
        sway = emu.alloc(b"\xA5" * 0x70)
        record = emu.alloc(b"\x00" * 8)
        middle = emu.alloc(b"\x00" * 8)
        inner = emu.alloc(b"\xA5" * 0x110)
        emu.write(self + 0x208, u32(sway))
        emu.write(record + 4, u32(middle))
        emu.write(middle + 4, u32(inner))
        emu.write(sway + 0x18, f32(0.25))
        if mode == 1:
            emu.write(inner + 0xA4, f32(0.0))
            emu.write(inner + 0xA8, f32(0.5))
        elif mode == 2:
            emu.write(inner + 0xA4, f32(0.5))
            emu.write(inner + 0xA8, f32(0.0))
        else:
            emu.write(inner + 0xA4, f32(0.5))
            emu.write(inner + 0xA8, f32(0.25))
            if mode == 4:
                emu.write(sway + 0x18, f32(4.0))
            elif mode == 5:
                emu.write(sway + 0x18, f32(-4.0))
            elif mode == 6:
                emu.write(sway + 0x18, struct.pack("<f", 1.5707963705062866))
        sample = 3.0 if mode == 3 else 0.5
        return self, sway, record, middle, struct.unpack("<I", f32(sample))[0]

    for mode in range(1, 7):
        emu.reset_data()
        self, sway, record, middle, sample = angle_objects(mode)
        compare(
            f"angle {mode}", emu, checker, missed, covered, 0x00520220, f"angle {mode}",
            [record, sample], self, 8,
            [("sway", sway), ("mid", middle)], [("mid", middle)],
            [(sway + 0x18, 4)])

    def aux_case(mode: int) -> None:
        emu.reset_data()
        world = World()
        obj = emu.alloc(b"\xA5" * 0x340)
        existing = emu.alloc(b"\xA5" * 0x24)
        created = emu.alloc(b"\xA5" * 0x24)
        emu.write(obj + 0x328, u32(existing if mode == 0 else 0))

        def stub(machine: Emulator) -> int:
            size = machine.stack_arg(0)
            world.trace += f"alloc:{size};"
            return 0 if mode == 2 else created

        emu.bind_function(0x00416820, stub, stack_cleanup=0)
        aux = existing if mode == 0 else (None if mode == 2 else created)
        compare(
            f"aux {mode}", emu, checker, missed, covered, 0x00520380, f"aux {mode}",
            [], obj, 0, [("aux", aux)] if aux else [], [],
            [] if aux is None else [(aux, 0x24)], world)

    for mode in (0, 1, 2):
        aux_case(mode)

    registry = 0x00DF03FC

    def reg_world(slots):
        world = World()
        table = plant(emu, world, slots)
        reg = emu.alloc(u32(table) + b"\x00" * 16)
        emu.write(registry, u32(reg))
        world.names.append(("reg", reg))
        return world, table

    def linked(label, va, command, slots, mode, results, stack, cleanup, prepare, flags=0):
        emu.reset_data()
        world, table = reg_world(slots)
        world.results = list(results)
        obj = emu.alloc(b"\xA5" * 0x400)
        tracked = emu.alloc(u32(table) + u32(1 if mode == 2 else 2) + b"\x00" * 8)
        for offset in (0x138, 0x178, 0x17C, 0x180, 0x1F4):
            emu.write(obj + offset, u32(0))
        emu.write(obj + 0x13D, bytes([flags]))
        emu.write(obj + 0x20C, u32(0x22))
        emu.write(obj + 0x210, u32(0x33))
        if mode:
            emu.write(obj + 0x1F4, u32(tracked))
        world.names.append(("track", tracked))
        prepare(emu, world, table, obj)
        compare(
            label, emu, checker, missed, covered, va, command, stack, obj, cleanup,
            [("in", INCOMING)], [],
            [(obj + 0x13D, 1), (obj + 0x1F4, 4), (obj + 0x20C, 8), (tracked + 4, 4)], world)

    def nop(*_args):
        return None

    release_slots = {0x6C: (1, 4), 0: (1, 4)}
    linked("release none", 0x00525D40, "release 0 1 11 0 0", release_slots, 0, [0x11], [], 0, nop)
    linked("release keep", 0x00525D40, "release 1 1 11 0 0", release_slots, 1, [0x11], [], 0, nop)
    linked("release drop", 0x00525D40, "release 2 2 11 12 0 0", release_slots, 2, [0x11, 0x12], [], 0, nop)
    linked("clear absent", 0x00525DA0, "clear 0 0 0 0 0", release_slots, 0, [], [], 0, nop)
    linked("clear other", 0x00525DA0, "clear 0 0 0 0 5", release_slots, 0, [], [], 0, nop, 5)

    def clear_module(machine, world, table, obj, child):
        module = machine.alloc(u32(table) + b"\x00" * 12)
        listing = machine.alloc(u32(module) + u32(0))
        machine.write(obj + 0x180, u32(listing))
        world.names.append(("m0", module))
        if child:
            inner = machine.alloc(u32(table))
            world.inner = inner
            world.names.append(("i0", inner))
            old = world.take

            def take():
                value = old()
                return inner if value == 1 else value

            world.take = take  # type: ignore[method-assign]

    linked(
        "clear plain", 0x00525DA0, "clear 0 2 11 22 1 0 16",
        {0x6C: (1, 4), 0: (1, 4), 0x34: (0, 0)}, 0, [0x11, 0x22], [], 0,
        lambda m, w, t, o: clear_module(m, w, t, o, False), 0x10)
    linked(
        "clear child", 0x00525DA0, "clear 0 4 11 22 1 44 1 0 16",
        {0x6C: (1, 4), 0: (1, 4), 0x34: (0, 0), 4: (0, 0)}, 0, [0x11, 0x22, 1, 0x44], [], 0,
        lambda m, w, t, o: clear_module(m, w, t, o, True), 0x10)

    def bind_lookup(world, clear=False):
        def lookup(machine):
            args = [machine.stack_arg(index) for index in range(2)]
            if clear:
                machine.write(args[0], u32(0))
            world.note("d", 0x0052D830, machine.register(uc.UC_X86_REG_ECX), args)
            return world.take()

        emu.bind_function(0x0052D830, lookup, stack_cleanup=8)
        bind_direct(emu, world, 0x0055A140, 0, 0)

    def alt_state(field):
        def prepare(machine, _world, _table, obj):
            machine.write(obj + 0x26C, u32(field))
            machine.write(obj + 0x210, u32(0x44))
        return prepare

    linked("visible early", 0x0052D990, "visible 0 0 0 0 2 0 0", release_slots, 0, [], [0], 4, nop, 2)
    linked(
        "visible run", 0x0052D990, "visible 0 4 0 0 0 55 0 0 22 0 0", release_slots, 0,
        [0, 0, 0, 0x55], [0], 4, lambda *_args: bind_lookup(_args[1], True), 0x16)
    linked("flagd same0", 0x0052DAE0, "flagd 0 0 0 0 0 0", release_slots, 0, [], [0], 4, nop)
    linked("flagd same1", 0x0052DAE0, "flagd 0 0 0 0 2 1", release_slots, 0, [], [1], 4, nop, 2)
    linked("flagd clear", 0x0052DAE0, "flagd 0 1 11 0 0 2 0", release_slots, 0, [0x11], [0], 4, nop, 2)
    linked("flagd raise", 0x0052DAE0, "flagd 0 0 0 0 0 1", release_slots, 0, [], [1], 4, nop)
    linked("attach held", 0x0052DBD0, "attach 0 0 0 0 16", release_slots, 0, [], [], 0, nop, 0x10)
    linked("attach new", 0x0052DBD0, "attach 0 0 0 0 0", release_slots, 0, [], [], 0, nop)
    linked("alternate miss", 0x0052DA30, "alternate 0 0 0 0 0 0", {0xD8: (1, 4)}, 0, [], [], 0, alt_state(0))
    linked("alternate field", 0x0052DA30, "alternate 0 0 0 0 28 0", {0xD8: (1, 4)}, 0, [], [], 0, alt_state(0), 0x1C)
    linked(
        "alternate reject", 0x0052DA30, "alternate 0 1 11 0 0 28 8", {0xD8: (1, 4)}, 0, [0x11], [], 0,
        alt_state(8), 0x1C)
    linked("supplied early", 0x0052D9E0, "supplied 0 0 0 0 0 0", release_slots, 0, [], [0], 4, nop)
    def supplied_same(machine, world, _table, obj):
        bind_lookup(world)
        machine.write(obj + 0x1F4, u32(7))

    linked(
        "supplied same", 0x0052D9E0, "supplied 0 2 0 55 0 0 22 7", release_slots, 0, [0, 0x55], [7], 4,
        supplied_same, 0x16)
    linked(
        "install empty", 0x0052D8F0, "install 0 3 0 0 55 0 0 0 0 0", release_slots, 0, [0, 0, 0x55],
        [0, 0], 8, lambda *_args: bind_lookup(_args[1], True))

    def gate(label, command, count, results, flags):
        emu.reset_data()
        emu.write(STACK_BASE + STACK_SIZE - 0x400, b"\x00" * 0x300)
        world = World()
        world.results = list(results)
        table = plant(emu, world, {0x4C: (1, 4), 0xE4: (0, 0)})
        bodies = [emu.alloc(u32(table)) for _ in range(count)]
        listing = emu.alloc(b"".join(u32(body) for body in bodies) + u32(0))
        obj = emu.alloc(b"\xA5" * 0x400)
        emu.write(obj + 0x178, u32(listing))
        emu.write(obj + 0x1C8, u32(0))
        emu.write(obj + 0x138, u32(0))
        emu.write(obj + 0x13C, bytes([flags[0]]))
        if "gate" in command:
            emu.write(obj + 0x13D, bytes([flags[2]]))
            emu.write(obj + 0x13E, bytes([flags[1]]))
            emu.write(obj + 0x258, bytes([flags[3]]))
        for index, body in enumerate(bodies):
            world.names.append((f"m{index}", body))
        compare(
            label, emu, checker, missed, covered, 0x0052DC90 if "gate" in command else 0x0052DDB0,
            command, [] if "gate" in command else [flags[3] if "flagc" in command else 0], obj,
            0 if "gate" in command else 4, [("in", INCOMING)], [], [(obj + 0x13C, 4)], world)

    gate("gate empty", "gate 0 0 0 0 0 0", 0, [], (0, 0, 0, 0))
    gate("gate blocked", "gate 1 11 1 0 0 0 0", 1, [0x11], (0, 0, 0, 0))
    gate("gate open", "gate 1 11 1 2 0 0 0", 1, [0x11], (2, 0, 0, 0))
    gate("gate replace", "gate 2 7 11 1 2 1 0 5", 1, [7, 0x11], (2, 1, 0, 5))
    gate("gate two", "gate 2 11 22 2 6 0 0 0", 2, [0x11, 0x22], (6, 0, 0, 0))
    gate("flagc same", "flagc 0 0 2 1", 0, [], (2, 0xA5, 0xA5, 1))
    gate("flagc set", "flagc 1 11 1 0 1", 1, [0x11], (0, 0xA5, 0xA5, 1))
    gate("flagc clear", "flagc 0 0 2 0", 0, [], (2, 0xA5, 0xA5, 0))

    def tint_case(version, shift):
        emu.reset_data()
        world = World()
        world.stamp = True
        world.version_byte = version
        table = plant(emu, world, {0x18: (1, 4), 0x58: (1, 4), 0x6C: (1, 4), 0x84: (1, 4), 0x80: (1, 4), 0x64: (1, 4)})
        xfer = emu.alloc(u32(table) + u32(shift) + b"\x00" * 8)
        obj = emu.alloc(b"\xA5" * 0x80)
        world.names.append(("xfer", xfer))
        compare(
            f"tintread {version} {shift}", emu, checker, missed, covered, 0x00520CB0,
            f"tintread 0 {version} {shift}", [xfer], obj, 4,
            [("in", INCOMING), ("xfer", xfer)], [], [(obj + 4, 0x44)], world)

    tint_case(0, 8)
    tint_case(1, 0)
    tint_case(2, 0)

    def block_case(count, byte, rewrite):
        emu.reset_data()
        world = World()
        world.stamp = rewrite >= 0
        table = plant(emu, world, {0x6C: (1, 4), 0x58: (1, 4), 0x8C: (2, 0)})
        xfer = emu.alloc(u32(table) + u32(0) + u32(byte) + u32(0))
        counter = emu.alloc(u32(0x1000) + u32(count))
        world.names += [("xfer", xfer), ("vt", table)]
        compare(
            f"block {count} {byte} {rewrite}", emu, checker, missed, covered, 0x00523140,
            f"block 0 {count} {byte} {rewrite}", [xfer, counter], emu.alloc(b"\x00\x00\x00\x00"), 0,
            [("xfer", xfer)], [], [(counter, 8)], world)

    block_case(0, 0, -1)
    block_case(1, 1, -1)
    block_case(2, 0, -1)
    block_case(1, 1, 0)

    def record_case(kind, key, mods, owner_flag, results):
        emu.reset_data()
        world = World()
        world.results = list(results)
        table = plant(emu, world, {0x38: (0, 0), 0xC: (1, 4)})
        obj = emu.alloc(b"\xA5" * 0x400)
        emu.write(obj + 4, u32(0))
        emu.write(obj + 0x180, u32(0))
        bodies = [emu.alloc(u32(table)) for _ in range(mods)]
        if mods:
            listing = emu.alloc(b"".join(u32(body) for body in bodies) + u32(0))
            emu.write(obj + 0x180, u32(listing))
            world.names.append(("m0", bodies[0]))
        if owner_flag:
            owner = emu.alloc(b"\x00" * 0x180)
            emu.write(obj + 4, u32(owner))
            world.names.append(("owner", owner + 0x15C))
            bind_direct(emu, world, 0x0055BE60, 1, 4)
        if kind == "select":
            bind_direct(emu, world, 0x0052CD70, 0, 0)
        if 1 in results:
            inner = emu.alloc(u32(table))
            world.inner = inner
            world.names.append(("i0", inner))
            old = world.take

            def take():
                value = old()
                return inner if value == 1 else value

            world.take = take  # type: ignore[method-assign]
        text = " ".join(f"{value:x}" for value in results)
        command = f"{kind} {len(results)} {text} {key} {mods} {owner_flag}".replace("  ", " ")
        compare(
            command, emu, checker, missed, covered,
            0x005268E0 if kind == "resolve" else 0x0052CDD0, command,
            [key & 0xFFFFFFFF], obj, 4, [("in", INCOMING), ("miss", 0x00DF0408)], [], [], world)

    record_case("resolve", -1, 0, 0, [])
    record_case("resolve", 4, 0, 0, [])
    record_case("resolve", 4, 1, 0, [1, 0x55])
    record_case("resolve", 4, 1, 1, [0, 0x66])
    record_case("select", 1, 1, 0, [0x11, 1, 0x55])
    record_case("select", 1, 1, 0, [0, 1, 0x55])
    record_case("select", 3, 1, 0, [1, 0x55])

    def secondary(has_object, bit, diverge, result):
        emu.reset_data()
        world = World()
        world.results = [result]
        obj = emu.alloc(b"\xA5" * 0x400)
        if (obj & 0xFF) == 0:
            emu.alloc(b"\x00" * 16)
            obj = emu.alloc(b"\xA5" * 0x400)
        game = emu.alloc(u32(1) + u32(2) + u32(3) + u32(4))
        empty = emu.alloc(u32(0))
        emu.write(obj + 0x178, u32(empty))
        emu.write(obj + 0x138, u32(game if has_object else 0))
        emu.write(obj + 0x13D, b"\x00")
        emu.write(obj + 0x25B, b"\x00")
        world.names += [("self", obj), ("list", empty)]
        if has_object:
            emu.write(obj + 0x138, u32(game))
            world.names.append(("obj", game))
            bind_direct(emu, world, 0x0078C500, 1, 4)
        emu.write(obj + 0x13E, bytes([bit]))
        emu.write(obj + 0x38, u32(0x111))
        emu.write(obj + 0x3C, u32(0x222))
        emu.write(obj + 0x40, u32(0x333))
        for index in range(12):
            emu.write(obj + 8 + index * 4, u32(0x40 + index))
        emu.write(obj + 0x244, u32(1 if diverge else 0))
        emu.write(obj + 0x248, u32(0))
        head = f"secondary 1 {result:x}" if result else "secondary 0"
        compare(
            f"secondary {has_object} {bit}", emu, checker, missed, covered, 0x0052DEF0,
            f"{head} {has_object} {bit} {diverge}", [], obj + 0x60, 0,
            [("in", INCOMING), ("self", obj), ("body", obj + 0xF4)], [("body", obj + 0xF4)],
            [(obj + 0xF4, 0x30), (obj + 0x124, 12)], world)

    secondary(0, 0, 0, 0)
    secondary(1, 0, 0, 0x1100)
    secondary(1, 4, 1, 0)

    def patch_random():
        counter = emu.alloc(u32(0))
        quarter = emu.alloc(f32(0.25))
        half = emu.alloc(f32(0.5))
        code = b"\x83\x3D" + u32(counter) + b"\x00\x75\x0D\xFF\x05" + u32(counter)
        code += b"\xD9\x05" + u32(quarter) + b"\xC3\xD9\x05" + u32(half) + b"\xC3"
        emu.write(0x005A95A0, code)

    def sway_update(mode):
        emu.reset_data()
        world = World()
        obj = emu.alloc(b"\xA5" * 0x400)
        sway = emu.alloc(b"\xA5" * 0x70)
        record = emu.alloc(b"\x00" * 8)
        middle = emu.alloc(b"\x00" * 8)
        inner = emu.alloc(b"\xA5" * 0xD0)
        out = emu.alloc(b"\xA5" * 0x10)
        game = emu.alloc(b"\x00" * 0x380)
        emu.write(record + 4, u32(middle))
        emu.write(middle + 4, u32(inner))
        for offset, value in ((0x88, 1.0), (0x94, 0.5), (0x9C, 0.25), (0xC0, 0.5), (0xB4, 2.0)):
            emu.write(inner + offset, f32(value))
        for offset, value in ((0xC, 0.5), (0x10, 0.25), (0x14, 0.125), (0x1C, 2.0), (0x24, -2.0)):
            emu.write(sway + offset, f32(value))
        for offset, value in ((0xC4, 0.0), (0xC8, 0.0), (0xD0, 1.0), (0xD4, 0.0), (0xDC, 0.0), (0xE0, 1.0)):
            emu.write(obj + offset, f32(value))
        emu.write(obj + 0x390, u32(10))
        emu.write(obj + 0x138, u32(0))
        emu.write(obj + 0x208, u32(0 if mode in (3, 4) else sway))
        if mode in (1, 2):
            emu.write(obj + 0x138, u32(game))
            emu.write(game + 0x374, u32(1))
        clock = emu.alloc(b"\x00" * 0x60)
        emu.write(clock + 0x58, u32(11 if mode == 2 else 10))
        emu.write(LOGIC, u32(clock))
        created = emu.alloc(b"\xA5" * 0x70)

        def allocate(machine: Emulator) -> int:
            world.trace += f"alloc:{machine.stack_arg(0)};"
            return 0 if mode == 4 else created

        if mode in (3, 4):
            emu.bind_function(0x00416820, allocate, stack_cleanup=0)
            patch_random()
        state = None if mode == 4 else created if mode == 3 else sway
        slices = [(out, 0x10)]
        if state is not None:
            slices.append((state, 0x70))
        slices.append((obj + 0x390, 4))
        exact = [("rec", record), ("inner", inner)]
        if state is not None:
            exact.insert(0, ("sway", state))
        compare(
            f"updatesway {mode}", emu, checker, missed, covered, 0x00524310, f"updatesway {mode}",
            [record, out], obj, 8, exact, [], slices, world)

    for mode in range(5):
        sway_update(mode)


def main() -> None:
    if len(sys.argv) != 2:
        raise SystemExit(f"usage: {sys.argv[0]} ra3_drawable52check.exe")
    checker = Checker(Path(sys.argv[1]))
    emu = Emulator(GAME)
    failures = 0
    covered: set[str] = set()

    def exercise(label: str, va: int, command: str, stack: list[int], *, ecx: int = 0,
                 eax: int = INCOMING, cleanup: int, exact: list[tuple[str, int]],
                 ah: list[tuple[str, int]] | None = None, mem: bytes = b"",
                 world: World | None = None, edx: int = 0) -> None:
        covered.add(f"{va:08x}")
        result = emu.call(va, stack, ecx=ecx, eax=eax, edx=edx)
        native_trace = world.trace if world is not None else "-"
        if not native_trace:
            native_trace = "-"
        try:
            check(
                label, tag_of(result.eax, exact, ah or [], eax), mem.hex() if mem else "-",
                native_trace, checker.call(command), cleanup, result.stack_cleanup)
        except SystemExit as error:
            print(error)
            nonlocal_bump()

    missed = {"n": 0}

    def nonlocal_bump() -> None:
        missed["n"] += 1

    # The closure above can't assign failures cleanly; count via missed.
    def run(label, va, command, stack, ecx=0, eax=INCOMING, cleanup=0, exact=None, ah=None,
            mem=b"", world=None, edx=0):
        covered.add(f"{va:08x}")
        emu.uc.reg_write(uc.UC_X86_REG_FPCW, 0x027F)
        result = emu.call(va, stack, ecx=ecx, eax=eax, edx=edx)
        native_trace = "-" if world is None or not world.trace else world.trace
        native_mem = "-" if not mem else mem.hex()
        line = checker.call(command)
        tag, got_mem, trace = parse(line)
        native_tag = tag_of(result.eax, exact or [], ah or [], eax)
        if tag != native_tag or got_mem != native_mem or trace != native_trace or result.stack_cleanup != cleanup:
            missed["n"] += 1
            print(f"{label}")
            print(f"  native tag={native_tag} mem={native_mem} trace={native_trace} clean={result.stack_cleanup}")
            print(f"  checker tag={tag} mem={got_mem} trace={trace} clean={cleanup}")

    # 0x005261A0.  The body ignores EAX and returns the TintEnvelope static slot.
    for incoming in (0, INCOMING):
        emu.reset_data()
        run("tint", 0x005261A0, "tint", [], eax=incoming)

    # 0x0052A800
    def copy_case(mode: int) -> None:
        emu.reset_data()
        source = bytearray(64)
        for index in range(12):
            struct.pack_into("<I", source, index * 4, 0x01000000 + index * 0x1111)
        struct.pack_into("<I", source, 12, 0x7FC00000)
        src = emu.alloc(bytes(source))
        if mode == 0:
            dest = emu.alloc(b"\x5A" * 64)
            run("copy0", 0x0052A800, "copy 0", [src], ecx=dest, cleanup=4,
                exact=[("dest", dest)], mem=b"")
            # mem is filled after the call inside run from the argument, so compare
            # by re-reading.  Re-issue through a direct compare below.
        else:
            dest = src if mode == 1 else src + 4
            run("copy" + str(mode), 0x0052A800, f"copy {mode}", [src], ecx=dest, cleanup=4,
                exact=[("dest", dest)], mem=emu.read(src, 64))

    # Mode 0 is checked after a dedicated call so the destination bytes are read
    # after emulation.  The helper above would read them too early for mode 0.
    emu.reset_data()
    source = bytearray(64)
    for index in range(12):
        struct.pack_into("<I", source, index * 4, 0x01000000 + index * 0x1111)
    struct.pack_into("<I", source, 12, 0x7FC00000)
    src = emu.alloc(bytes(source))
    dest = emu.alloc(b"\x5A" * 64)
    result = emu.call(0x0052A800, [src], ecx=dest)
    line = checker.call("copy 0")
    tag, got, trace = parse(line)
    native = tag_of(result.eax, [("dest", dest)], [], 0)
    native_mem = emu.read(dest, 48).hex()
    if tag != native or got != native_mem or trace != "-" or result.stack_cleanup != 4:
        missed["n"] += 1
        print("copy0", native, native_mem, tag, got, result.stack_cleanup)
    covered.add("0052a800")
    for mode in (1, 2):
        emu.reset_data()
        src = emu.alloc(bytes(source))
        dest = src if mode == 1 else src + 4
        result = emu.call(0x0052A800, [src], ecx=dest)
        line = checker.call(f"copy {mode}")
        tag, got, trace = parse(line)
        native = tag_of(result.eax, [("dest", dest)], [], 0)
        native_mem = emu.read(src, 64).hex()
        if tag != native or got != native_mem or trace != "-" or result.stack_cleanup != 4:
            missed["n"] += 1
            print(f"copy{mode}", native, tag, got == native_mem, result.stack_cleanup, trace)

    run_rest(emu, checker, missed, covered)
    print(f"drawable52 oracle mismatches: {missed['n']}  covered {len(covered)}")
    checker.close()
    if missed["n"]:
        raise SystemExit(1)


if __name__ == "__main__":
    main()
