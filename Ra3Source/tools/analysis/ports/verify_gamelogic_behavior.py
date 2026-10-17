"""Differential checks for the RA3 0x006B GameLogic behavior cluster.

Every catalog entry is exercised through the C++ per-entry checker and its
shipped clean body in Emulator.  One representative nontrivial constructor
scenario also carries an intentional output mutation.
For methods that orchestrate engine-owned objects, stubs expose the exact
callback arguments and writes that implement the reconstructed decision.  The
oracle also checks stack cleanup, EAX where meaningful, x87 float returns,
aliases, and preservation of bytes outside the native write footprint.
"""

from __future__ import annotations

import argparse
import random
import struct
import subprocess
import sys
from pathlib import Path

from unicorn import x86_const

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from emulate import Emulator  # noqa: E402


SIZE = 1024
ENTRIES = (
    0x006B1540, 0x006B1670, 0x006B16E0, 0x006B1900, 0x006B1E90,
    0x006B1F00, 0x006B20E0, 0x006B21C0, 0x006B2220, 0x006B2260,
    0x006B2320, 0x006B23B0, 0x006B2690, 0x006B27B0, 0x006B3120,
    0x006B3160, 0x006B3270, 0x006B3660, 0x006B3820, 0x006B3980,
    0x006B39A0, 0x006B3DC0, 0x006B3F10, 0x006B3FC0, 0x006B4070,
    0x006B40F0, 0x006B44E0, 0x006B4720, 0x006B48D0, 0x006B4A40,
    0x006B4D40, 0x006B4EB0, 0x006B51C0, 0x006B5AE0, 0x006B5AF0,
    0x006B5BA0, 0x006B5C20, 0x006B5CA0, 0x006B6EC0, 0x006B7340,
    0x006B7850, 0x006B7B00, 0x006B7D70, 0x006B7D80, 0x006B8480,
    0x006B8C70, 0x006B8CF0, 0x006B8EC0, 0x006B91E0, 0x006B9510,
    0x006B9580, 0x006B95D0, 0x006B96C0, 0x006B99C0, 0x006B9B70,
    0x006B9BB0, 0x006BA5D0,
)


def u32(blob: bytearray | bytes, index: int) -> int:
    return struct.unpack_from("<I", blob, index * 4)[0]


def put_u32(blob: bytearray, index: int, value: int) -> None:
    struct.pack_into("<I", blob, index * 4, value & 0xFFFFFFFF)


def f32(blob: bytearray | bytes, index: int) -> float:
    return struct.unpack_from("<f", blob, index * 4)[0]


def put_f32(blob: bytearray, index: int, value: float) -> None:
    struct.pack_into("<f", blob, index * 4, value)


class Checker:
    def __init__(self, executable: Path) -> None:
        self.process = subprocess.Popen(
            [str(executable.resolve())],
            stdin=subprocess.PIPE, stdout=subprocess.PIPE,
            text=True, encoding="ascii", bufsize=1)

    def call(self, operation: str, address: int, blob: bytes) -> bytes:
        assert self.process.stdin is not None and self.process.stdout is not None
        self.process.stdin.write(
            f"{operation} {address:08x} {blob.hex()}\n")
        self.process.stdin.flush()
        line = self.process.stdout.readline().strip()
        if line == "error" or len(line) != SIZE * 2:
            raise RuntimeError(
                f"behaviorcheck rejected {operation} 0x{address:08X}")
        return bytes.fromhex(line)

    def close(self) -> None:
        assert self.process.stdin is not None
        self.process.stdin.write("quit\n")
        self.process.stdin.flush()
        if self.process.wait(timeout=10) != 0:
            raise RuntimeError("behaviorcheck failed")


def manager_with_frame(emu: Emulator, frame: int) -> int:
    manager = emu.alloc(bytes(0x200))
    emu.write(manager + 0x58, struct.pack("<I", frame))
    emu.write(0x00DEBA64, struct.pack("<I", manager))
    return manager


def linked_conditions(
        emu: Emulator, owner: int,
        values: list[tuple[int, int]]) -> None:
    sentinel = owner + 0x24
    if not values:
        emu.write(sentinel, struct.pack("<II", sentinel, sentinel))
        return
    nodes = [emu.alloc(bytes(16)) for _ in values]
    for index, ((condition, expiration), node) in enumerate(zip(values, nodes)):
        next_node = nodes[index + 1] if index + 1 < len(nodes) else sentinel
        previous = nodes[index - 1] if index else sentinel
        emu.write(node, struct.pack(
            "<IIII", next_node, previous, condition, expiration))
    emu.write(sentinel, struct.pack("<II", nodes[0], nodes[-1]))


def compare_direct(
        emu: Emulator, checker: Checker, rng: random.Random,
        cases: int) -> tuple[int, dict[int, int]]:
    bad = 0
    totals: dict[int, int] = {}
    lock_stub = emu.bind_stub(lambda _: 0, stack_cleanup=4)
    lock_vtable = emu.alloc(struct.pack("<I", lock_stub))
    emu.bind_import(0x00BBA1E4, lambda _: 0, stack_cleanup=4)
    emu.bind_import(0x00BBA1E8, lambda _: 0, stack_cleanup=4)

    def checked(address: int, scenario: bytearray) -> bytearray:
        return bytearray(checker.call("eval", address, bytes(scenario)))

    def record(address: int, mismatch: bool) -> None:
        nonlocal bad
        if mismatch:
            bad += 1
            print(f"native mismatch at 0x{address:08X}")

    scalar_specs = (
        (0x006B3980, 0x28),
        (0x006B3DC0, 0xF4),
    )
    for address, offset in scalar_specs:
        for _ in range(cases):
            flags = rng.getrandbits(32)
            data = emu.alloc(bytes(0x120))
            owner = emu.alloc(bytes(16))
            emu.write(owner + 4, struct.pack("<I", data))
            emu.write(data + offset, struct.pack("<I", flags))
            game = emu.call(address, ecx=owner)
            scenario = bytearray(SIZE)
            put_u32(scenario, 0, flags)
            ours = checked(address, scenario)
            record(address, game.eax != u32(ours, 0) or game.stack_cleanup != 0)
            totals[address] = totals.get(address, 0) + 1

    for address, expected in (
            (0x006B7D70, 0x42),
            (0x006B7D80, 0x40),
            (0x006B8480, 0x1C),
            (0x006B8C70, 0x90)):
        for _ in range(cases):
            initial = rng.getrandbits(32)
            target = emu.alloc(struct.pack("<I", initial))
            game = emu.call(address, [target])
            scenario = bytearray(SIZE)
            put_u32(scenario, 0, initial)
            ours = checked(address, scenario)
            game_value = struct.unpack("<I", emu.read(target, 4))[0]
            record(address, (
                game_value != expected or game_value != u32(ours, 0)
                or game.eax != target or game.stack_cleanup != 4))
            totals[address] = totals.get(address, 0) + 1

    for _ in range(cases):
        enabled = rng.randrange(2)
        owner = emu.alloc(rng.randbytes(0x80))
        game = emu.call(0x006B5AE0, [enabled], ecx=owner)
        scenario = bytearray(SIZE)
        put_u32(scenario, 0, enabled)
        ours = checked(0x006B5AE0, scenario)
        record(0x006B5AE0, (
            emu.read(owner + 0x66, 1) != ours[0x66:0x67]
            or game.stack_cleanup != 4))
        totals[0x006B5AE0] = totals.get(0x006B5AE0, 0) + 1

    for _ in range(cases):
        frame = rng.getrandbits(31)
        emu.write(0x00DF8AB8, struct.pack("<I", frame))
        owner_value = rng.getrandbits(32)
        data_value = rng.getrandbits(32)
        initial = rng.randbytes(0x70)
        destination = emu.alloc(initial)
        game = emu.call(
            0x006B8EC0, [owner_value, data_value], ecx=destination)
        scenario = bytearray(SIZE)
        put_u32(scenario, 0, owner_value)
        put_u32(scenario, 1, data_value)
        put_u32(scenario, 2, frame)
        scenario[0x100:0x170] = initial
        ours = checked(0x006B8EC0, scenario)
        game_bytes = emu.read(destination, 0x70)
        mismatch = (
            game_bytes != ours[:0x70]
            or game.eax != destination or game.stack_cleanup != 8)
        if mismatch:
            print(f"physics ctor game={game_bytes.hex()} ours={ours[:0x70].hex()} "
                  f"eax={game.eax:08x}/{destination:08x}")
        record(0x006B8EC0, mismatch)
        totals[0x006B8EC0] = totals.get(0x006B8EC0, 0) + 1

    for _ in range(cases):
        has_target = rng.randrange(2)
        status = rng.getrandbits(32)
        target = emu.alloc(bytes(0xC4))
        emu.write(target + 0xC0, struct.pack("<I", status))
        physics = emu.alloc(bytes(0x20))
        emu.write(
            physics + 0x10,
            struct.pack("<I", target if has_target else 0))
        game = emu.call(0x006B91E0, ecx=physics)
        scenario = bytearray(SIZE)
        put_u32(scenario, 0, has_target)
        put_u32(scenario, 1, status)
        ours = checked(0x006B91E0, scenario)
        record(0x006B91E0, game.eax != u32(ours, 0) or game.stack_cleanup != 0)
        totals[0x006B91E0] = totals.get(0x006B91E0, 0) + 1

    for address in (0x006B3120, 0x006B3160):
        for _ in range(cases):
            frame = rng.randrange(1, 0x70000000)
            manager_with_frame(emu, frame)
            count = rng.randrange(0, 9)
            values = [
                (rng.randrange(0, 456), rng.randrange(1, 0x70000000))
                for _ in range(count)]
            owner = emu.alloc(bytes(0x40))
            linked_conditions(emu, owner, values)
            scenario = bytearray(SIZE)
            put_u32(scenario, 0, count)
            if address == 0x006B3120:
                condition = rng.randrange(0, 500)
                put_u32(scenario, 1, condition)
                put_u32(scenario, 2, frame)
                for index, value in enumerate(values):
                    struct.pack_into("<II", scenario, 0x100 + index * 8, *value)
                game = emu.call(address, [condition], ecx=owner)
            else:
                put_u32(scenario, 1, frame)
                for index, value in enumerate(values):
                    struct.pack_into("<II", scenario, 0x100 + index * 8, *value)
                game = emu.call(address, ecx=owner)
            ours = checked(address, scenario)
            expected_cleanup = 4 if address == 0x006B3120 else 0
            mismatch = (
                game.eax != u32(ours, 0)
                or game.stack_cleanup != expected_cleanup)
            if mismatch:
                print(
                    f"condition address={address:08x} frame={frame:08x} "
                    f"values={values} game={game.eax:08x} "
                    f"ours={u32(ours, 0):08x}")
            record(address, mismatch)
            totals[address] = totals.get(address, 0) + 1

    for _ in range(cases):
        destination = [rng.getrandbits(32) for _ in range(3)]
        value = [rng.getrandbits(32) for _ in range(3)]
        flags = rng.getrandbits(32)
        target_value = rng.getrandbits(32)
        physics = emu.alloc(bytes(0xD0))
        emu.write(physics + 0x40, struct.pack("<I", flags))
        emu.write(physics + 0xC0, struct.pack("<I", target_value))
        owner = emu.alloc(bytes(0x220))
        emu.write(owner + 0x1BC, struct.pack("<III", *destination))
        emu.write(owner + 0x214, struct.pack("<I", physics))
        vector_address = emu.alloc(struct.pack("<III", *value))
        game = emu.call(0x006B6EC0, [vector_address], ecx=owner)
        scenario = bytearray(SIZE)
        struct.pack_into("<III", scenario, 0, *destination)
        put_u32(scenario, 3, flags)
        put_u32(scenario, 4, target_value)
        struct.pack_into("<III", scenario, 20, *value)
        ours = checked(0x006B6EC0, scenario)
        mismatch = (
            emu.read(owner + 0x1BC, 12) != ours[:12]
            or emu.read(physics + 0x40, 4) != ours[12:16]
            or emu.read(physics + 0xC0, 4) != ours[16:20]
            or game.eax != value[2] or game.stack_cleanup != 4)
        if mismatch:
            print(
                f"destination vec={emu.read(owner + 0x1BC, 12).hex()}/{ours[:12].hex()} "
                f"flags={emu.read(physics + 0x40, 4).hex()}/{ours[12:16].hex()} "
                f"target={emu.read(physics + 0xC0, 4).hex()}/{ours[16:20].hex()} "
                f"eax={game.eax:08x}/{value[2]:08x}")
        record(0x006B6EC0, mismatch)
        totals[0x006B6EC0] = totals.get(0x006B6EC0, 0) + 1

    for _ in range(cases):
        output_initial = rng.getrandbits(32)
        output = emu.alloc(struct.pack("<I", output_initial))
        owner = emu.alloc(bytes(0x20))
        game = emu.call(0x006B1E90, [0, output], ecx=owner + 0xC)
        scenario = bytearray(SIZE)
        put_u32(scenario, 4, output_initial)
        ours = checked(0x006B1E90, scenario)
        record(0x006B1E90, (
            (game.eax & 0xFF) != u32(ours, 0)
            or emu.read(output, 4) != ours[16:20]
            or game.stack_cleanup != 8))
        totals[0x006B1E90] = totals.get(0x006B1E90, 0) + 1

    for _ in range(cases):
        wanted = rng.getrandbits(32)
        owner = emu.alloc(bytes(0x80))
        emu.write(owner, struct.pack("<I", lock_vtable))
        sentinel = owner + 0x54
        emu.write(sentinel, struct.pack("<II", sentinel, sentinel))
        game = emu.call(0x006B21C0, [wanted], ecx=owner)
        scenario = bytearray(SIZE)
        put_u32(scenario, 1, wanted)
        ours = checked(0x006B21C0, scenario)
        record(0x006B21C0, (
            game.eax != u32(ours, 0) or game.stack_cleanup != 4)
        )
        totals[0x006B21C0] = totals.get(0x006B21C0, 0) + 1

    emu.bind_function(0x006A4FB0, lambda _: 0)
    for _ in range(cases):
        status = rng.getrandbits(32)
        owner = emu.alloc(bytes(0xC0))
        emu.write(owner, struct.pack("<I", owner))
        emu.write(owner + 0xA4, struct.pack("<I", status))
        game = emu.call(0x006B27B0, ecx=owner + 0x1C)
        scenario = bytearray(SIZE)
        put_u32(scenario, 0, status)
        ours = checked(0x006B27B0, scenario)
        game_status = struct.unpack("<I", emu.read(owner + 0xA4, 4))[0]
        record(
            0x006B27B0,
            game_status != u32(ours, 0) or game.stack_cleanup != 0)
        totals[0x006B27B0] = totals.get(0x006B27B0, 0) + 1

    for address, back_words, expected in (
            (0x006B4720, 2, 1),
            (0x006B48D0, 8, 0)):
        for _ in range(cases):
            obj = emu.alloc(bytes(0x280))
            interior = emu.alloc(bytes(0x80)) + back_words * 4
            emu.write(interior - back_words * 4, struct.pack("<I", obj))
            game = emu.call(address, ecx=interior)
            scenario = bytearray(SIZE)
            ours = checked(address, scenario)
            record(address, (
                game.eax != expected or game.eax != u32(ours, 0)
                or game.stack_cleanup != 0))
            totals[address] = totals.get(address, 0) + 1

    for _ in range(cases):
        status = 0x80 | (rng.getrandbits(32) & 0xFFFFFF00)
        obj = emu.alloc(bytes(0xC0))
        emu.write(obj, struct.pack("<I", obj))
        emu.write(obj + 0xB8, struct.pack("<I", status))
        interior = obj + 0x1C
        game = emu.call(0x006B4D40, [0, 0], ecx=interior)
        scenario = bytearray(SIZE)
        put_u32(scenario, 0, status)
        put_u32(scenario, 1, 1)
        ours = checked(0x006B4D40, scenario)
        record(0x006B4D40, (
            (game.eax & 0xFF) != u32(ours, 0)
            or game.stack_cleanup != 8))
        totals[0x006B4D40] = totals.get(0x006B4D40, 0) + 1

    for _ in range(cases):
        target_bits = [rng.getrandbits(32) for _ in range(3)]
        owner = emu.alloc(bytes(0xC0))
        output = emu.alloc(struct.pack("<III", *target_bits))
        game = emu.call(0x006B4EB0, [output], ecx=owner)
        scenario = bytearray(SIZE)
        struct.pack_into("<III", scenario, 0, *target_bits)
        ours = checked(0x006B4EB0, scenario)
        record(0x006B4EB0, (
            emu.read(output, 12) != ours[:12]
            or game.stack_cleanup != 4))
        totals[0x006B4EB0] = totals.get(0x006B4EB0, 0) + 1

    for _ in range(cases):
        obj = emu.alloc(bytes(0x520))
        status = rng.getrandbits(32) & ~0x800
        emu.write(obj, struct.pack("<I", obj))
        emu.write(obj + 0xB4, struct.pack("<I", status))
        game = emu.call(0x006B51C0, [2], ecx=obj + 0x514)
        scenario = bytearray(SIZE)
        put_u32(scenario, 0, status)
        ours = checked(0x006B51C0, scenario)
        record(0x006B51C0, (
            emu.read(obj + 0xB4, 4) != ours[:4]
            or game.stack_cleanup != 4))
        totals[0x006B51C0] = totals.get(0x006B51C0, 0) + 1

    for _ in range(cases):
        game_object = emu.alloc(bytes(0x400))
        owner = emu.alloc(bytes(0x400))
        emu.write(owner + 8, struct.pack("<I", game_object))
        game = emu.call(0x006B7340, ecx=owner)
        scenario = bytearray(SIZE)
        ours = checked(0x006B7340, scenario)
        record(0x006B7340, (
            (game.eax & 0xFF) != u32(ours, 0)
            or game.stack_cleanup != 0))
        totals[0x006B7340] = totals.get(0x006B7340, 0) + 1

    for _ in range(cases):
        radius = rng.uniform(-100.0, 100.0)
        data = emu.alloc(bytes(0x10))
        emu.write(data + 0xC, struct.pack("<f", radius))
        obj = emu.alloc(bytes(0x380))
        owner = emu.alloc(bytes(16))
        emu.write(owner + 4, struct.pack("<I", data))
        emu.write(owner + 8, struct.pack("<I", obj))
        game, game_bits = emu.call_float32(0x006B7850, ecx=owner)
        scenario = bytearray(SIZE)
        put_f32(scenario, 0, radius)
        ours = checked(0x006B7850, scenario)
        record(0x006B7850, (
            game_bits != u32(ours, 0)
            or game.stack_cleanup != 0))
        totals[0x006B7850] = totals.get(0x006B7850, 0) + 1

    for _ in range(cases):
        data = emu.alloc(bytes(0x40))
        threshold = rng.uniform(-10.0, 0.0)
        emu.write(data + 0x38, struct.pack("<f", threshold))
        obj = emu.alloc(bytes(0xC0))
        owner = emu.alloc(bytes(16))
        emu.write(owner + 4, struct.pack("<I", data))
        emu.write(owner + 8, struct.pack("<I", obj))
        before = emu.read(obj + 0xB8, 4)
        game = emu.call(0x006B9B70, ecx=owner)
        scenario = bytearray(SIZE)
        put_f32(scenario, 0, threshold)
        put_u32(scenario, 1, struct.unpack("<I", before)[0])
        ours = checked(0x006B9B70, scenario)
        record(0x006B9B70, (
            u32(ours, 0) != 0 or emu.read(obj + 0xB8, 4) != before
            or game.stack_cleanup != 0))
        totals[0x006B9B70] = totals.get(0x006B9B70, 0) + 1

    for _ in range(cases):
        cached = rng.getrandbits(31)
        owner = emu.alloc(bytes(0x80))
        emu.write(owner + 0x60, struct.pack("<I", cached))
        game = emu.call(0x006B1F00, [0], ecx=owner)
        scenario = bytearray(SIZE)
        put_u32(scenario, 2, cached)
        ours = checked(0x006B1F00, scenario)
        record(
            0x006B1F00,
            game.eax != u32(ours, 0) or game.stack_cleanup != 4)
        totals[0x006B1F00] = totals.get(0x006B1F00, 0) + 1

    for _ in range(cases):
        owner = emu.alloc(bytes(0x80))
        game = emu.call(0x006B3820, ecx=owner)
        scenario = bytearray(SIZE)
        ours = checked(0x006B3820, scenario)
        record(
            0x006B3820,
            game.eax != u32(ours, 4) or game.stack_cleanup != 0)
        totals[0x006B3820] = totals.get(0x006B3820, 0) + 1

    for _ in range(cases):
        owner = emu.alloc(bytes(0x20))
        target = emu.alloc(bytes(12))
        game = emu.call(0x006B44E0, [target], ecx=owner)
        scenario = bytearray(SIZE)
        put_u32(scenario, 0, 1)
        ours = checked(0x006B44E0, scenario)
        record(
            0x006B44E0,
            game.eax != u32(ours, 0) or game.stack_cleanup != 4)
        totals[0x006B44E0] = totals.get(0x006B44E0, 0) + 1

    for _ in range(cases):
        source_status = 0x00800000
        docker_status = 0x00800000
        source = emu.alloc(bytes(0xC0))
        docker = emu.alloc(bytes(0xC0))
        emu.write(source + 0xB4, struct.pack("<I", source_status))
        emu.write(docker + 0xB4, struct.pack("<I", docker_status))
        module = emu.alloc(bytes(0x40))
        emu.write(module, struct.pack("<I", source))
        game = emu.call(0x006B5CA0, [docker], ecx=module + 0x1C)
        scenario = bytearray(SIZE)
        put_u32(scenario, 0, source_status)
        put_u32(scenario, 1, docker_status)
        ours = checked(0x006B5CA0, scenario)
        record(
            0x006B5CA0,
            emu.read(source + 0xB4, 4) != ours[:4]
            or emu.read(docker + 0xB4, 4) != ours[4:8]
            or game.stack_cleanup != 4)
        totals[0x006B5CA0] = totals.get(0x006B5CA0, 0) + 1

    for _ in range(cases):
        owner = emu.alloc(bytes(0x140))
        before = emu.read(owner, 0x140)
        game = emu.call(0x006B95D0, [5, rng.randrange(2)], ecx=owner)
        scenario = bytearray(SIZE)
        put_u32(scenario, 0, 5)
        put_u32(scenario, 1, 1)
        ours = checked(0x006B95D0, scenario)
        record(
            0x006B95D0,
            emu.read(owner, 0x140) != before
            or u32(ours, 0) != 0 or game.stack_cleanup != 8)
        totals[0x006B95D0] = totals.get(0x006B95D0, 0) + 1

    for _ in range(cases):
        owner = emu.alloc(bytes(0x680))
        before = emu.read(owner, 0x680)
        game = emu.call(0x006B99C0, [rng.getrandbits(32)], ecx=owner)
        scenario = bytearray(SIZE)
        ours = checked(0x006B99C0, scenario)
        record(
            0x006B99C0,
            emu.read(owner, 0x680) != before
            or u32(ours, 0) != 0 or game.stack_cleanup != 4)
        totals[0x006B99C0] = totals.get(0x006B99C0, 0) + 1

    for _ in range(cases):
        owner = emu.alloc(bytes(0x20))
        game = emu.call(0x006B96C0, ecx=owner)
        scenario = bytearray(SIZE)
        ours = checked(0x006B96C0, scenario)
        record(
            0x006B96C0,
            game.eax != 1 or u32(ours, 0) != 0
            or game.stack_cleanup != 0)
        totals[0x006B96C0] = totals.get(0x006B96C0, 0) + 1

    for _ in range(cases):
        owner = emu.alloc(bytes(0x20))
        ids = emu.alloc(bytes(16))
        game = emu.call(0x006B9580, [ids], ecx=owner)
        scenario = bytearray(SIZE)
        ours = checked(0x006B9580, scenario)
        record(
            0x006B9580,
            game.eax != u32(ours, 0) or game.stack_cleanup != 4)
        totals[0x006B9580] = totals.get(0x006B9580, 0) + 1

    # Garrison helpers are full methods, while the checker exposes the
    # independently useful decision made by each method.  Capture the method's
    # callback and object writes to compare that decision without replacing the
    # surrounding engine-owned object graph.
    station_calls: list[tuple[int, int, int, int]] = []
    destination_calls: list[tuple[int, int, int, int]] = []

    def station_stub(machine: Emulator) -> int:
        station_calls.append((
            machine.register(x86_const.UC_X86_REG_ECX),
            machine.stack_arg(0), machine.stack_arg(1), machine.stack_arg(2)))
        return 0

    def destination_stub(machine: Emulator) -> int:
        destination_calls.append((
            machine.register(x86_const.UC_X86_REG_ECX),
            machine.stack_arg(0), machine.stack_arg(1), machine.stack_arg(2)))
        return 0

    emu.bind_function(0x00691F10, station_stub, stack_cleanup=12)
    emu.bind_function(0x00692030, destination_stub, stack_cleanup=12)
    emu.bind_function(0x005A94C0, lambda _: 0)
    for _ in range(cases):
        station_calls.clear()
        strategy_vtable = emu.alloc(bytes(0x58))
        category_stub = emu.bind_stub(lambda _: 0)
        emu.write(strategy_vtable + 0x54, struct.pack("<I", category_stub))
        strategy = emu.alloc(struct.pack("<I", strategy_vtable))
        game_object = emu.alloc(bytes(0x380))
        emu.write(game_object + 0x33C, struct.pack("<I", strategy))
        owner = emu.alloc(bytes(0x760))
        emu.write(owner + 8, struct.pack("<I", game_object))
        emu.write(owner + 0x744, struct.pack("<I", 3))
        emu.write(owner + 0x150, struct.pack("<III", 9, 0, 8))
        target = emu.alloc(bytes(4))
        game = emu.call(0x006B1540, [target, 0, 0], ecx=owner)
        scenario = bytearray(SIZE)
        put_u32(scenario, 0, 0)
        put_u32(scenario, 1, 3)
        struct.pack_into("<III", scenario, 0x100, 9, 0, 8)
        ours = checked(0x006B1540, scenario)
        record(0x006B1540, (
            u32(ours, 0) != 1
            or station_calls != [(owner, target, 0, 1)]
            or game.stack_cleanup != 12))
        totals[0x006B1540] = totals.get(0x006B1540, 0) + 1

    for _ in range(cases):
        destination_calls.clear()
        owner = emu.alloc(bytes(0x20))
        target = emu.alloc(bytes(0x100))
        target_object = emu.alloc(bytes(4))
        target_position = emu.alloc(bytes(12))
        game = emu.call(
            0x006B1670, [target, target_object, target_position], ecx=owner)
        scenario = bytearray(SIZE)
        put_u32(scenario, 0, 1)
        put_u32(scenario, 1, 1)
        put_u32(scenario, 2, 7)
        ours = checked(0x006B1670, scenario)
        record(0x006B1670, (
            u32(ours, 0) != 1
            or destination_calls != [(owner, target, target_object, 0)]
            or game.stack_cleanup != 12))
        totals[0x006B1670] = totals.get(0x006B1670, 0) + 1

    reassignment_results = iter((3, -1))
    assignment_stub = emu.bind_stub(
        lambda _: next(reassignment_results) & 0xFFFFFFFF,
        stack_cleanup=4)
    reassignment_vtable = emu.alloc(bytes(0xA4))
    emu.write(reassignment_vtable + 0xA0, struct.pack("<I", assignment_stub))
    removal_calls: list[tuple[int, int, int]] = []

    def removal_stub(machine: Emulator) -> int:
        removal_calls.append((
            machine.register(x86_const.UC_X86_REG_ECX),
            machine.stack_arg(0), machine.stack_arg(1)))
        return 0

    emu.bind_function(0x006801D0, removal_stub, stack_cleanup=8)
    emu.bind_function(
        0x006B1540,
        lambda machine: station_calls.append((
            machine.register(x86_const.UC_X86_REG_ECX),
            machine.stack_arg(0), machine.stack_arg(1), machine.stack_arg(2))) or 0,
        stack_cleanup=12)
    for _ in range(cases):
        station_calls.clear()
        removal_calls.clear()
        reassignment_results = iter((3, -1))
        owner = emu.alloc(bytes(0x20))
        emu.write(owner, struct.pack("<I", reassignment_vtable))
        target = emu.alloc(bytes(0x100))
        target_id = rng.getrandbits(32)
        emu.write(target + 0xDC, struct.pack("<I", target_id))
        game = emu.call(0x006B16E0, [target], ecx=owner)
        scenario = bytearray(SIZE)
        put_u32(scenario, 0, 3)
        put_u32(scenario, 1, 0xFFFFFFFF)
        ours = checked(0x006B16E0, scenario)
        record(0x006B16E0, (
            u32(ours, 0) != 3
            or removal_calls != [(owner, target, 0xFFFFFFFF)]
            or station_calls != [(owner, target, 0, target + 0x38)]
            or game.stack_cleanup != 4))
        totals[0x006B16E0] = totals.get(0x006B16E0, 0) + 1

    emu.bind_function(0x006B1540, lambda _: 0, stack_cleanup=12)
    for _ in range(cases):
        vtable = emu.alloc(bytes(0xA4))
        point_stub = emu.bind_stub(lambda _: 0xFFFFFFFF, stack_cleanup=4)
        emu.write(vtable + 0xA0, struct.pack("<I", point_stub))
        owner = emu.alloc(struct.pack("<I", vtable))
        target = emu.alloc(bytes(0x100))
        pair = emu.alloc(struct.pack("<II", owner, 0))
        game = emu.call(0x006B1900, [target, pair])
        scenario = bytearray(SIZE)
        put_u32(scenario, 0, 0xFFFFFFFF)
        put_u32(scenario, 1, 0)
        put_f32(scenario, 2, 10.0)
        put_f32(scenario, 3, 1.0)
        ours = checked(0x006B1900, scenario)
        record(0x006B1900, (
            u32(ours, 0) != 0 or game.stack_cleanup != 0))
        totals[0x006B1900] = totals.get(0x006B1900, 0) + 1

    relation_value = 0
    emu.bind_function(0x006A5370, lambda _: relation_value, stack_cleanup=4)
    for _ in range(cases):
        relation_value = rng.randrange(3)
        data = emu.alloc(bytes(0x90))
        owner_object = emu.alloc(bytes(0x420))
        candidate = emu.alloc(bytes(0x420))
        owner = emu.alloc(bytes(12))
        emu.write(owner + 4, struct.pack("<I", data))
        emu.write(owner + 8, struct.pack("<I", owner_object))
        allow = [rng.randrange(2) for _ in range(5)]
        emu.write(data + 0x89, bytes(allow[:4]))
        owner_player = rng.getrandbits(32)
        candidate_player = owner_player if allow[4] else owner_player ^ 1
        emu.write(owner_object + 0x418, struct.pack("<I", owner_player))
        emu.write(candidate + 0x418, struct.pack("<I", candidate_player))
        game = emu.call(0x006B20E0, [candidate], ecx=owner)
        scenario = bytearray(SIZE)
        put_u32(scenario, 0, relation_value)
        put_u32(scenario, 1, allow[2])
        put_u32(scenario, 2, allow[3])
        put_u32(scenario, 3, allow[4])
        put_u32(scenario, 4, allow[0])
        put_u32(scenario, 5, allow[1])
        ours = checked(0x006B20E0, scenario)
        record(0x006B20E0, (
            (game.eax & 0xFF) != u32(ours, 0)
            or game.stack_cleanup != 4))
        totals[0x006B20E0] = totals.get(0x006B20E0, 0) + 1

    for address, wanted in ((0x006B2220, 0), (0x006B2260, 1)):
        scenario_kinds = (
            (False, ()),
            (True, (wanted,)),
            (True, (1 - wanted, wanted)),
        )
        for base_check, kinds in scenario_kinds:
            for _ in range(cases):
                vtable = emu.alloc(bytes(0x48))
                base_stub = emu.bind_stub(
                    lambda _, value=base_check: 1 if value else 0)
                emu.write(vtable + 0x44, struct.pack("<I", base_stub))
                owner = emu.alloc(bytes(0x80))
                emu.write(owner, struct.pack("<I", vtable))
                sentinel = owner + 0x68
                if not kinds:
                    emu.write(owner + 0x6C, struct.pack("<I", sentinel))
                else:
                    nodes = [emu.alloc(bytes(0x18)) for _ in kinds]
                    emu.write(owner + 0x6C, struct.pack("<I", nodes[0]))
                    for index, (node, kind) in enumerate(zip(nodes, kinds)):
                        successor = (
                            nodes[index + 1]
                            if index + 1 < len(nodes) else 0)
                        emu.write(node, struct.pack("<I", successor))
                        emu.write(node + 0x14, struct.pack("<I", kind))
                game = emu.call(address, ecx=owner)
                scenario = bytearray(SIZE)
                put_u32(scenario, 0, 1 if base_check else 0)
                put_u32(scenario, 1, len(kinds))
                scenario[0x100:0x100 + len(kinds)] = bytes(kinds)
                ours = checked(address, scenario)
                record(address, (
                    (game.eax & 0xFF) != u32(ours, 0)
                    or game.stack_cleanup != 0))
                totals[address] = totals.get(address, 0) + 1

    for _ in range(cases):
        owner = emu.alloc(bytes(0x80))
        sentinel = owner + 0x54
        contained = emu.alloc(bytes(0x80))
        node = emu.alloc(struct.pack("<III", sentinel, sentinel, contained))
        emu.write(sentinel, struct.pack("<II", node, node))
        x = rng.uniform(-20.0, 20.0)
        y = rng.uniform(-20.0, 20.0)
        emu.write(contained + 0x38, struct.pack("<ff", x, y))
        position = emu.alloc(struct.pack("<ff", x + 1.0, y + 2.0))
        game = emu.call(0x006B2320, [position], ecx=owner)
        scenario = bytearray(SIZE)
        put_u32(scenario, 0, 1)
        put_f32(scenario, 1, x + 1.0)
        put_f32(scenario, 2, y + 2.0)
        struct.pack_into("<IffIBB", scenario, 0x100, 1, x, y, 0, 0, 1)
        ours = checked(0x006B2320, scenario)
        record(0x006B2320, (
            game.eax != contained or u32(ours, 0) != 0
            or game.stack_cleanup != 4))
        totals[0x006B2320] = totals.get(0x006B2320, 0) + 1

    for configured in (2, 0, -3):
        for _ in range(cases):
            scale = 0.25
            entries = emu.alloc(struct.pack(
                "<II", 0, configured & 0xFFFFFFFF))
            data = emu.alloc(bytes(0x140))
            emu.write(data + 0x128, struct.pack("<II", 1, entries))
            owner = emu.alloc(bytes(0x160))
            emu.write(owner + 4, struct.pack("<I", data))
            emu.write(owner + 0x154, struct.pack("<f", scale))
            game = emu.call(0x006B23B0, ecx=owner)
            scenario = bytearray(SIZE)
            put_u32(scenario, 0, configured)
            put_f32(scenario, 1, scale)
            ours = checked(0x006B23B0, scenario)
            expected_result = 0 if configured > 0 else configured & 0xFFFFFFFF
            record(0x006B23B0, (
                u32(ours, 0) != expected_result
                or emu.read(owner + 0x154, 4) != ours[4:8]
                or game.stack_cleanup != 0))
            totals[0x006B23B0] = totals.get(0x006B23B0, 0) + 1

    damage_match = 1
    emu.bind_function(0x0078B960, lambda _: damage_match, stack_cleanup=8)
    emu.bind_function(0x006A4FB0, lambda _: 0)
    for _ in range(cases):
        data = emu.alloc(bytes(0x120))
        obj = emu.alloc(bytes(0x120))
        emu.write(data + 0xA4, struct.pack("<I", 1))
        vtable = emu.alloc(bytes(0xD8))
        update_stub = emu.bind_stub(lambda _: 0)
        emu.write(vtable + 0xD4, struct.pack("<I", update_stub))
        module = emu.alloc(bytes(0x40))
        interior = module + 0x20
        emu.write(module, struct.pack("<II", data, obj))
        emu.write(interior, struct.pack("<I", vtable))
        game = emu.call(0x006B2690, [0], ecx=interior)
        scenario = bytearray(SIZE)
        put_u32(scenario, 0, 1)
        put_u32(scenario, 0x100 // 4, 1)
        ours = checked(0x006B2690, scenario)
        status = struct.unpack("<I", emu.read(obj + 0xA4, 4))[0]
        record(0x006B2690, (
            status != u32(ours, 0) or game.stack_cleanup != 4))
        totals[0x006B2690] = totals.get(0x006B2690, 0) + 1

    for _ in range(cases):
        direction = [rng.uniform(-5.0, 5.0) for _ in range(3)]
        scale = rng.uniform(0.25, 3.0)
        owner = emu.alloc(bytes(0x40))
        holder = emu.alloc(bytes(0x28))
        emu.write(owner + 0x18, struct.pack("<I", holder))
        emu.write(owner + 0x28, struct.pack("<fff", *direction))
        emu.write(owner + 0x34, struct.pack("<f", scale))
        emu.bind_function(0x0078BFB0, lambda _: 0, stack_cleanup=4)
        emu.write(0x00694090, b"\xD9\x41\x34\xC3")
        game = emu.call(0x006B3270, ecx=owner)
        scenario = bytearray(SIZE)
        for index, value in enumerate(direction):
            put_f32(scenario, index, value)
        put_f32(scenario, 3, scale)
        ours = checked(0x006B3270, scenario)
        record(0x006B3270, (
            emu.read(owner + 0x28, 12) != ours[:12]
            or game.stack_cleanup != 0))
        totals[0x006B3270] = totals.get(0x006B3270, 0) + 1

    damage_flags = 0

    def damage_event_stub(machine: Emulator) -> int:
        nonlocal damage_flags
        damage_flags = struct.unpack(
            "<I", machine.read(machine.stack_arg(1) + 0x28, 4))[0]
        return 0

    def memset_stub(machine: Emulator) -> int:
        destination = machine.stack_arg(0)
        value = machine.stack_arg(1) & 0xFF
        size = machine.stack_arg(2)
        machine.write(destination, bytes([value]) * size)
        return destination

    emu.bind_function(0x004D2F98, memset_stub)
    emu.bind_function(0x0052A0A0, lambda _: 0x12345678, stack_cleanup=16)
    emu.bind_function(0x006A5060, damage_event_stub, stack_cleanup=8)
    for _ in range(cases):
        ratio = 2.0
        damage_flags = 0
        game = emu.call(0x006B3660, [
            struct.unpack("<I", struct.pack("<f", ratio))[0], 0])
        scenario = bytearray(SIZE)
        put_f32(scenario, 0, ratio)
        ours = checked(0x006B3660, scenario)
        record(0x006B3660, (
            u32(ours, 0) != 4 or damage_flags != 0x10
            or game.stack_cleanup != 8))
        totals[0x006B3660] = totals.get(0x006B3660, 0) + 1

    emu.bind_function(0x0063ED00, lambda _: 0)
    for _ in range(cases):
        start = [rng.uniform(-20.0, 20.0) for _ in range(3)]
        finish = [rng.uniform(-20.0, 20.0) for _ in range(3)]
        distance = rng.uniform(0.25, 5.0)
        data = emu.alloc(bytes(0x1CC))
        emu.write(data + 0x1C8, struct.pack("<f", distance))
        owner = emu.alloc(bytes(8))
        emu.write(owner + 4, struct.pack("<I", data))
        output = emu.alloc(bytes(12))
        start_address = emu.alloc(struct.pack("<fff", *start))
        finish_address = emu.alloc(struct.pack("<fff", *finish))
        game = emu.call(
            0x006B39A0, [output, start_address, finish_address], ecx=owner)
        adjusted_z = struct.unpack("<f", emu.read(output + 8, 4))[0]
        scenario = bytearray(SIZE)
        for index, value in enumerate(start + finish):
            put_f32(scenario, index, value)
        put_f32(scenario, 6, distance)
        put_f32(scenario, 7, adjusted_z)
        ours = checked(0x006B39A0, scenario)
        record(0x006B39A0, (
            emu.read(output, 12) != ours[:12]
            or game.stack_cleanup != 12))
        totals[0x006B39A0] = totals.get(0x006B39A0, 0) + 1

    for _ in range(cases):
        data = emu.alloc(bytes(0x100))
        owner_object = emu.alloc(bytes(0x100))
        owner = emu.alloc(bytes(0xD0))
        emu.write(owner + 4, struct.pack("<I", data))
        emu.write(owner + 8, struct.pack("<I", owner_object))
        before = emu.read(owner + 0xCA, 1)
        game = emu.call(0x006B3F10, ecx=owner)
        scenario = bytearray(SIZE)
        ours = checked(0x006B3F10, scenario)
        record(0x006B3F10, (
            u32(ours, 0) != 0 or emu.read(owner + 0xCA, 1) != before
            or game.stack_cleanup != 0))
        totals[0x006B3F10] = totals.get(0x006B3F10, 0) + 1

    # Empty child arrays isolate each composite special-power method's base
    # callback.  Small x87 stubs preserve the original caller-side float ABI.
    emu.write(0x00695740, b"\xD9\xE8\xC3")
    for _ in range(cases):
        data = emu.alloc(bytes(0x1C8))
        module = emu.alloc(bytes(0x20))
        emu.write(module, struct.pack("<I", data))
        game, game_bits = emu.call_float32(0x006B3FC0, ecx=module + 0xC)
        scenario = bytearray(SIZE)
        put_f32(scenario, 1, 1.0)
        ours = checked(0x006B3FC0, scenario)
        record(0x006B3FC0, (
            game_bits != u32(ours, 0) or game.stack_cleanup != 0))
        totals[0x006B3FC0] = totals.get(0x006B3FC0, 0) + 1

    progress_values: list[int] = []

    def progress_stub(machine: Emulator) -> int:
        progress_values.append(machine.stack_arg(0))
        return 0

    emu.bind_function(0x00695840, progress_stub, stack_cleanup=4)
    for _ in range(cases):
        progress_values.clear()
        value = rng.uniform(0.0, 1.0)
        value_bits = struct.unpack("<I", struct.pack("<f", value))[0]
        data = emu.alloc(bytes(0x1C8))
        module = emu.alloc(bytes(0x20))
        emu.write(module, struct.pack("<I", data))
        game = emu.call(0x006B4070, [value_bits], ecx=module + 0xC)
        scenario = bytearray(SIZE)
        put_f32(scenario, 2, value)
        ours = checked(0x006B4070, scenario)
        record(0x006B4070, (
            progress_values != [u32(ours, 1)]
            or game.stack_cleanup != 4))
        totals[0x006B4070] = totals.get(0x006B4070, 0) + 1

    child_by_id: dict[int, int] = {}
    child_reset_calls: list[int] = []
    base_progress_values: list[int] = []

    def child_lookup_stub(machine: Emulator) -> int:
        return child_by_id.get(machine.stack_arg(0), 0)

    def child_reset_stub(machine: Emulator) -> int:
        child = machine.register(x86_const.UC_X86_REG_ECX)
        child_reset_calls.append(child)
        machine.write(child + 4, struct.pack("<f", 0.0))
        return 0

    def base_progress_stub(machine: Emulator) -> int:
        value = machine.stack_arg(0)
        base_progress_values.append(value)
        machine.write(
            machine.register(x86_const.UC_X86_REG_ECX) + 4,
            struct.pack("<I", value))
        return 0

    emu.bind_function(0x00687720, child_lookup_stub, stack_cleanup=4)
    child_reset_address = emu.bind_stub(child_reset_stub)
    base_progress_address = emu.bind_stub(
        base_progress_stub, stack_cleanup=4)
    for _ in range(cases):
        child_by_id.clear()
        child_reset_calls.clear()
        base_progress_values.clear()
        child_ids = [rng.getrandbits(32) for _ in range(2)]
        child_values = [rng.uniform(0.1, 0.9) for _ in child_ids]
        children: list[int] = []
        child_vtable = emu.alloc(bytes(0x24))
        emu.write(
            child_vtable + 0x20, struct.pack("<I", child_reset_address))
        for child_id, child_value in zip(child_ids, child_values):
            child = emu.alloc(bytes(8))
            emu.write(
                child, struct.pack("<If", child_vtable, child_value))
            child_by_id[child_id] = child
            children.append(child)

        ids = emu.alloc(struct.pack("<II", *child_ids))
        data = emu.alloc(bytes(0x1C8))
        emu.write(data + 0x1C0, struct.pack("<II", len(child_ids), ids))

        descriptor_data = emu.alloc(bytes(0x20))
        descriptor = emu.alloc(bytes(12))
        emu.write(descriptor + 8, struct.pack("<I", descriptor_data))
        descriptor_address = emu.bind_stub(lambda _: descriptor)
        base_vtable = emu.alloc(bytes(0x48))
        emu.write(
            base_vtable + 0x1C, struct.pack("<I", descriptor_address))
        emu.write(
            base_vtable + 0x44, struct.pack("<I", base_progress_address))

        module = emu.alloc(bytes(0x30))
        emu.write(module, struct.pack("<I", data))
        emu.write(module + 4, struct.pack("<I", 1))
        interior = module + 0xC
        emu.write(interior, struct.pack("<I", base_vtable))
        emu.write(interior + 4, struct.pack("<f", 0.75))
        game = emu.call(0x006B40F0, ecx=interior)
        scenario = bytearray(SIZE)
        put_u32(scenario, 0, len(child_values))
        put_f32(scenario, 1, 0.75)
        for index, value in enumerate(child_values):
            put_f32(scenario, 0x100 // 4 + index, value)
        ours = checked(0x006B40F0, scenario)
        record(0x006B40F0, (
            [emu.read(child + 4, 4) for child in children]
            != [ours[0x100 + index * 4:0x104 + index * 4]
                for index in range(len(children))]
            or base_progress_values != [u32(ours, 1)]
            or emu.read(interior + 4, 4) != ours[4:8]
            or child_reset_calls != children
            or game.stack_cleanup != 0))
        totals[0x006B40F0] = totals.get(0x006B40F0, 0) + 1

    for _ in range(cases):
        maximum = 2.0
        data = emu.alloc(bytes(0x100))
        emu.write(data + 0xFC, struct.pack("<f", maximum))
        owner_object = emu.alloc(bytes(0x80))
        emu.write(owner_object + 0x38, struct.pack("<fff", 0.0, 0.0, 0.0))
        module = emu.alloc(bytes(0x40))
        interior = module + 0x20
        emu.write(module, struct.pack("<II", data, owner_object))
        target = emu.alloc(struct.pack("<fff", 5.0, 0.0, 0.0))
        game = emu.call(
            0x006B4A40, [target, struct.unpack("<I", struct.pack("<f", 0.5))[0]],
            ecx=interior)
        scenario = bytearray(SIZE)
        put_u32(scenario, 0, 1)
        put_f32(scenario, 1, maximum)
        put_f32(scenario, 5, 5.0)
        put_u32(scenario, 8, 1)
        ours = checked(0x006B4A40, scenario)
        record(0x006B4A40, (
            (game.eax & 0xFF) != u32(ours, 0)
            or game.stack_cleanup != 8))
        totals[0x006B4A40] = totals.get(0x006B4A40, 0) + 1

    for address in (0x006B5AF0, 0x006B5BA0, 0x006B5C20):
        for _ in range(cases):
            docker_values = [rng.uniform(-100.0, 100.0) for _ in range(3)]
            docker = emu.alloc(bytes(0x80))
            emu.write(docker + 0x38, struct.pack("<fff", *docker_values))
            state = emu.alloc(bytes(0x40))
            emu.write(state + 0x2C, b"\x01")
            output = emu.alloc(bytes(12))
            game = emu.call(address, [docker, output], ecx=state)
            scenario = bytearray(SIZE)
            for index, value in enumerate(docker_values):
                put_f32(scenario, 6 if address == 0x006B5AF0 else 3 + index, value)
            if address == 0x006B5AF0:
                for index, value in enumerate(docker_values):
                    put_f32(scenario, 6 + index, value)
            ours = checked(address, scenario)
            record(address, (
                emu.read(output, 12) != ours[:12]
                or game.stack_cleanup != 8))
            totals[address] = totals.get(address, 0) + 1

    fire_dirty_calls = 0

    def fire_dirty_stub(_: Emulator) -> int:
        nonlocal fire_dirty_calls
        fire_dirty_calls += 1
        return 0

    emu.bind_function(0x006A4FB0, fire_dirty_stub)
    for _ in range(cases):
        weapon_words = [rng.getrandbits(32) for _ in range(12)]
        position = [rng.uniform(-50.0, 50.0) for _ in range(3)]
        status_object = emu.alloc(bytes(0xC4))
        emu.write(status_object + 0xC0, struct.pack("<I", 0x400))
        source = emu.alloc(bytes(0xC0))
        emu.write(source + 4, struct.pack("<I", status_object))
        emu.write(source + 8, struct.pack("<12I", *weapon_words))
        emu.write(source + 0x38, struct.pack("<fff", *position))
        destination = emu.alloc(bytes(0x60))
        fire_dirty_calls = 0
        game = emu.call(0x006B7B00, [source], ecx=destination)
        scenario = bytearray(SIZE)
        struct.pack_into("<12I", scenario, 0, *weapon_words)
        for index, value in enumerate(position):
            put_f32(scenario, 12 + index, value)
        put_u32(scenario, 15, 1)
        ours = checked(0x006B7B00, scenario)
        record(0x006B7B00, (
            emu.read(destination, 48) != ours[:48]
            or emu.read(destination + 0x30, 12) != ours[48:60]
            or struct.unpack("<I", emu.read(destination + 0x50, 4))[0] != u32(ours, 15)
            or emu.read(destination + 0x48, 4) != ours[64:68]
            or ((struct.unpack("<I", emu.read(source + 0xBC, 4))[0] >> 4) & 1)
            != u32(ours, 17)
            or fire_dirty_calls != u32(ours, 18)
            or game.stack_cleanup != 4))
        totals[0x006B7B00] = totals.get(0x006B7B00, 0) + 1

    for _ in range(cases):
        output = emu.alloc(bytes(12))
        game = emu.call(0x006B8CF0, [0, 0, 0, 0, 0, 0, output])
        scenario = bytearray(SIZE)
        ours = checked(0x006B8CF0, scenario)
        record(0x006B8CF0, (
            emu.read(output, 12) != ours[:12]
            or game.stack_cleanup != 0))
        totals[0x006B8CF0] = totals.get(0x006B8CF0, 0) + 1

    for has_prior in (False, True):
        for _ in range(cases):
            game_object = emu.alloc(bytes(0xE0))
            module = emu.alloc(bytes(0x140))
            prior = emu.alloc(bytes(0x60)) if has_prior else 0
            contact = emu.alloc(bytes(0x60))
            prior_id = rng.getrandbits(32)
            contact_id = rng.getrandbits(32)
            count = rng.randrange(1, 20) if has_prior else 0
            emu.write(module + 8, struct.pack("<I", game_object))
            emu.write(module + 0x34, struct.pack("<II", prior, prior))
            emu.write(module + 0x40, struct.pack("<I", count))
            emu.write(contact + 0x38, struct.pack("<I", contact_id))
            if has_prior:
                emu.write(prior + 0x38, struct.pack("<I", prior_id))
            game = emu.call(0x006B9510, [contact], ecx=module)
            scenario = bytearray(SIZE)
            put_u32(scenario, 0, 1 if has_prior else 0)
            put_u32(scenario, 1, contact_id)
            put_u32(scenario, 2, count)
            put_u32(scenario, 6, prior_id)
            ours = checked(0x006B9510, scenario)
            native_first = struct.unpack(
                "<I", emu.read(module + 0x34, 4))[0]
            native_last = struct.unpack(
                "<I", emu.read(module + 0x38, 4))[0]
            native_previous = struct.unpack(
                "<I", emu.read(contact + 0x58, 4))[0]
            normalized_previous = (
                prior_id if has_prior and native_previous == prior else 0)
            native_prior_next = (
                struct.unpack("<I", emu.read(prior + 0x54, 4))[0]
                if has_prior else 0)
            normalized_prior_next = (
                contact_id if native_prior_next == contact else 0)
            record(0x006B9510, (
                (native_first == contact) != (u32(ours, 0) != 0)
                or (native_last == contact) != (u32(ours, 1) != 0)
                or emu.read(module + 0x40, 4) != ours[8:12]
                or emu.read(contact + 0x54, 4) != b"\0\0\0\0"
                or normalized_previous != u32(ours, 4)
                or normalized_prior_next != u32(ours, 5)
                or game.stack_cleanup != 4))
            totals[0x006B9510] = totals.get(0x006B9510, 0) + 1

    condition_dirty_calls = 0

    def condition_dirty_stub(_: Emulator) -> int:
        nonlocal condition_dirty_calls
        condition_dirty_calls += 1
        return 0

    emu.bind_function(0x006A4FB0, condition_dirty_stub)
    for condition, already_set in ((0, False), (42, False), (42, True)):
        for _ in range(cases):
            status_a8 = rng.getrandbits(32) | 0xC6018000
            status_b8 = rng.getrandbits(32) | 0x00000280
            obj = emu.alloc(bytes(0x420))
            emu.write(obj + 0xA8, struct.pack("<I", status_a8))
            emu.write(obj + 0xB8, struct.pack("<I", status_b8))
            condition_word = obj + 0xA0 + 4 * (condition >> 5)
            condition_mask = 1 << (condition & 31)
            emu.write(
                condition_word,
                struct.pack("<I", condition_mask if already_set else 0))
            module = emu.alloc(bytes(12))
            emu.write(module + 8, struct.pack("<I", obj))
            condition_dirty_calls = 0
            game = emu.call(0x006B9BB0, [condition], ecx=module)
            scenario = bytearray(SIZE)
            put_u32(scenario, 0, status_a8)
            put_u32(scenario, 1, status_b8)
            put_u32(scenario, 2, condition)
            put_u32(scenario, 3, 1 if already_set else 0)
            ours = checked(0x006B9BB0, scenario)
            condition_set = (
                struct.unpack("<I", emu.read(condition_word, 4))[0]
                & condition_mask) != 0
            expected_dirty_calls = (
                (status_a8 & 0xC6018000).bit_count()
                + (status_b8 & 0x00000280).bit_count()
                + (0 if already_set else 1))
            record(0x006B9BB0, (
                emu.read(obj + 0xA8, 4) != ours[:4]
                or emu.read(obj + 0xB8, 4) != ours[4:8]
                or not condition_set
                or (u32(ours, 2) != 0) == already_set
                or condition_dirty_calls != expected_dirty_calls
                or game.stack_cleanup != 4))
            totals[0x006B9BB0] = totals.get(0x006B9BB0, 0) + 1

    # Imported ceil returns through ST0.  This local native stub changes x87
    # rounding to +infinity for FRNDINT and restores the caller's control word.
    ceil_stub = emu.alloc(bytes.fromhex(
        "83ec04d93c24668b042466894424026681642402fff3"
        "66814c24020008d96c2402dd442408d9fcd92c2483c404c3"))
    timing_scenarios = (
        (False, 10.0, 2.0, 1.75, 17),
        (True, 10.0, 1.5, 1.25, 19),
    )
    for has_manager, frames_per_second, manager_scale, delay, expected in (
            timing_scenarios):
        for _ in range(cases):
            owner_value = rng.getrandbits(32)
            owner_vtable = emu.alloc(bytes(12))
            owner_id_stub = emu.bind_stub(
                lambda _, value=owner_value: value)
            emu.write(owner_vtable + 8, struct.pack("<I", owner_id_stub))
            owner_object = emu.alloc(struct.pack("<I", owner_vtable))
            data = emu.alloc(bytes(0x20))
            emu.write(data + 0x0C, struct.pack("<f", delay))
            initial = rng.randbytes(0x3C)
            destination = emu.alloc(initial)
            emu.write(
                0x00DEEEE8, struct.pack("<f", frames_per_second))
            if has_manager:
                manager = emu.alloc(bytes(0x1C8))
                emu.write(
                    manager + 0x1C4, struct.pack("<f", manager_scale))
                emu.write(0x00DEBA64, struct.pack("<I", manager))
            else:
                emu.write(0x00DEBA64, struct.pack("<I", 0))
            emu.write(0x00BBA4DC, struct.pack("<I", ceil_stub))
            game = emu.call(
                0x006BA5D0, [owner_object, data], ecx=destination)
            scenario = bytearray(SIZE)
            put_u32(scenario, 0, owner_value)
            put_u32(scenario, 1, data)
            put_u32(scenario, 2, 1 if has_manager else 0)
            put_f32(scenario, 3, frames_per_second)
            put_f32(scenario, 4, manager_scale)
            put_f32(scenario, 5, delay)
            scenario[0x100:0x13C] = initial
            ours = checked(0x006BA5D0, scenario)
            record(0x006BA5D0, (
                emu.read(destination, 0x3C) != ours[:0x3C]
                or u32(ours, 14) != expected
                or game.eax != destination or game.stack_cleanup != 8))
            totals[0x006BA5D0] = totals.get(0x006BA5D0, 0) + 1

    return bad, totals


def verify_representative_mutation(
        checker: Checker, rng: random.Random) -> tuple[int, dict[int, int]]:
    address = 0x006BA5D0
    scenario = bytearray(SIZE)
    put_u32(scenario, 0, rng.getrandbits(32))
    put_u32(scenario, 1, rng.getrandbits(32))
    put_u32(scenario, 2, 1)
    put_f32(scenario, 3, 10.0)
    put_f32(scenario, 4, 1.5)
    put_f32(scenario, 5, 1.25)
    scenario[0x100:0x13C] = rng.randbytes(0x3C)
    ordinary = checker.call("eval", address, bytes(scenario))
    mutated = checker.call("mutate", address, bytes(scenario))
    bad = 1 if ordinary == mutated or ordinary[0] == mutated[0] else 0
    return bad, {address: 1}


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("checker", type=Path)
    parser.add_argument("--cases", type=int, default=10)
    args = parser.parse_args()
    rng = random.Random(0x6B2026)
    checker = Checker(args.checker)
    try:
        emu = Emulator()
        direct_bad, direct_totals = compare_direct(
            emu, checker, rng, args.cases)
        mutation_bad, mutation_totals = verify_representative_mutation(
            checker, rng)
    finally:
        checker.close()

    bad = direct_bad + mutation_bad
    total = sum(direct_totals.values()) + sum(mutation_totals.values())
    print(
        f"GameLogic behavior: {len(ENTRIES)} entries, {total} cases, "
        f"{bad} mismatches")
    print(
        "  representative mutation 006ba5d0: "
        f"{'detected' if mutation_bad == 0 else 'not detected'}")
    for address in ENTRIES:
        print(
            f"  {address:08x}: native={direct_totals.get(address, 0)} "
            f"mutation={mutation_totals.get(address, 0)}")
    return 1 if bad else 0


if __name__ == "__main__":
    raise SystemExit(main())
