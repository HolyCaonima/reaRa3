"""Differentially verify the RA3 MusicScripting GameClient cluster."""

from __future__ import annotations

import argparse
import random
import struct
import subprocess
import sys
from pathlib import Path

from unicorn import UC_PROT_ALL, x86_const

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from emulate import Emulator  # noqa: E402


TYPE_NAMES = (
    0x00923740, 0x00923750, 0x00923760, 0x00923890, 0x00923950,
    0x00923960, 0x009239C0, 0x009239D0, 0x009239E0, 0x009239F0,
    0x00923A00, 0x00923A10, 0x00923A70, 0x00923B70, 0x00923BE0,
    0x00923BF0, 0x00923C50, 0x00923C60, 0x00923C70, 0x00923D30,
)
CONSTRUCTORS = (
    0x00924BD0, 0x00924C00, 0x00924C30, 0x00924D10,
    0x00924D90, 0x00924E20, 0x00924E50, 0x00924E90, 0x00925290,
    0x00925360, 0x009256D0, 0x00925700, 0x00925810, 0x00926130,
    0x00926380,
)
BEHAVIOR = (
    0x009236C0, 0x00923970, 0x00923A20, 0x00923A30, 0x00923A50,
    0x00923AD0, 0x00923B20, 0x00923C00, 0x00923C80, 0x00923E20,
    0x00923EF0, 0x00923F00, 0x00923F50,
    0x00923F80, 0x009241F0, 0x00926470,
)
ALL_ENTRIES = frozenset(TYPE_NAMES + CONSTRUCTORS + BEHAVIOR)
EXPECTED_METADATA_DIGEST = 2951366512

GAME_CLOCK = 0x00DEBA64
CHECKS_PER_SECOND = 0x00DEEEE0
TICKS_PER_SECOND = 0x00DEEEE8
CLIENT = 0x00DFBD74
FRAMES_PER_SECOND = 0x00CB0910


class CppOracle:
    def __init__(self, executable: Path) -> None:
        self.process = subprocess.Popen(
            [str(executable)], stdin=subprocess.PIPE, stdout=subprocess.PIPE,
            text=True, bufsize=1)

    def call(self, command: str) -> str:
        assert self.process.stdin is not None and self.process.stdout is not None
        self.process.stdin.write(command + "\n")
        self.process.stdin.flush()
        result = self.process.stdout.readline().strip()
        if not result or result == "error":
            raise RuntimeError(f"gameclient92check rejected {command!r}")
        return result

    def close(self) -> None:
        assert self.process.stdin is not None
        self.process.stdin.write("quit\n")
        self.process.stdin.flush()
        if self.process.wait(timeout=10) != 0:
            raise RuntimeError("gameclient92check failed")


def bits(value: float) -> int:
    return struct.unpack("<I", struct.pack("<f", value))[0]


def u32(data: bytes, offset: int = 0) -> int:
    return struct.unpack_from("<I", data, offset)[0]


def c_string(emu: Emulator, address: int) -> str:
    data = bytearray()
    while emu.read(address + len(data), 1) != b"\0":
        data.extend(emu.read(address + len(data), 1))
    return data.decode("ascii")


def assert_abi(
    emu: Emulator, address: int, cleanup: int, result,
    *, eax: int | None = None, low_byte: int | None = None,
) -> None:
    if result.stack_cleanup != cleanup:
        raise AssertionError(
            f"0x{address:08X} cleanup {result.stack_cleanup}/{cleanup}")
    if eax is not None and result.eax != eax:
        raise AssertionError(
            f"0x{address:08X} EAX 0x{result.eax:08X}/0x{eax:08X}")
    if low_byte is not None and result.eax & 0xFF != low_byte:
        raise AssertionError(
            f"0x{address:08X} AL {result.eax & 0xFF}/{low_byte}")
    for register, name in (
            (x86_const.UC_X86_REG_EBX, "EBX"),
            (x86_const.UC_X86_REG_ESI, "ESI"),
            (x86_const.UC_X86_REG_EDI, "EDI")):
        if emu.register(register) != 0:
            raise AssertionError(f"0x{address:08X} clobbered {name}")


def verify(
    executable: Path, cases: int, *, mutate_address: int | None = None
) -> None:
    rng = random.Random(0x47433932)
    emu = Emulator()
    cpp = CppOracle(executable)
    totals: dict[str, int] = {}
    covered: set[int] = set()

    emu.alloc(bytes(0x10))
    obj = emu.alloc(bytes(0x200))
    resource = emu.alloc(bytes(0x200))
    event = emu.alloc(bytes(0x100))
    holder = emu.alloc(bytes(0x20))
    child_storage = emu.alloc(bytes(0x400))
    replacement_storage = emu.alloc(bytes(0x400))
    world = emu.alloc(bytes(0x300))
    client = emu.alloc(bytes(0x100))
    player = emu.alloc(bytes(0x200))
    clock = emu.alloc(bytes(0x100))
    clock_vtable = emu.alloc(bytes(0x100))

    def bind_self_list(machine: Emulator) -> int:
        destination = machine.register(x86_const.UC_X86_REG_ECX)
        machine.write(
            destination,
            struct.pack("<III", 0x00C5724C, destination + 4, destination + 4))
        return destination

    def bind_listener_list(machine: Emulator) -> int:
        destination = machine.register(x86_const.UC_X86_REG_ECX)
        machine.write(
            destination,
            struct.pack("<III", 0x00C4FBB0, destination + 4, destination + 4))
        return destination

    emu.bind_function(0x00929FD0, bind_self_list)
    emu.bind_function(0x0086E260, bind_listener_list)
    emu.bind_function(0x00590200, lambda _: 0, stack_cleanup=4)
    constructor_actions = {"clear": 0, "refresh": 0}

    def clear_constructor_records(_: Emulator) -> int:
        constructor_actions["clear"] += 1
        return 0

    def refresh_constructor_records(_: Emulator) -> int:
        constructor_actions["refresh"] += 1
        return 0

    emu.bind_function(
        0x009250D0, clear_constructor_records, stack_cleanup=8)
    emu.bind_function(0x00923780, refresh_constructor_records)
    emu.bind_function(0x00925AF0, refresh_constructor_records)
    emu.bind_function(0x00925E40, refresh_constructor_records)

    def copy_typed_state(machine: Emulator) -> int:
        destination = machine.register(x86_const.UC_X86_REG_ECX)
        stack = machine.register(x86_const.UC_X86_REG_ESP)
        source = u32(machine.read(stack + 4, 4))
        machine.write(destination, machine.read(source, 8))
        return destination

    emu.bind_function(0x00417080, copy_typed_state, stack_cleanup=4)
    # Imported ceil returns its double result in x87 ST0. This stub selects
    # round-toward-positive-infinity for FRNDINT and restores the caller CW.
    ceil_stub = 0x30000000
    emu.uc.mem_map(ceil_stub, 0x1000, UC_PROT_ALL)
    emu.write(
        ceil_stub,
        bytes.fromhex(
            "83EC04 D93C24 668B0424 6625FFF3 660D0008 "
            "6689442402 D96C2402 DD442408 D9FC D92C24 83C404 C3"))
    emu.write(0x00BBA4DC, struct.pack("<I", ceil_stub))
    emu.write(GAME_CLOCK, struct.pack("<I", world))
    emu.write(world + 0x1C4, struct.pack("<I", bits(1.0)))
    emu.write(world + 0x58, struct.pack("<I", 1000))
    emu.write(TICKS_PER_SECOND, struct.pack("<I", bits(15.0)))
    emu.write(CHECKS_PER_SECOND, struct.pack("<I", bits(15.0)))
    emu.write(CLIENT, struct.pack("<I", client))
    emu.write(client + 0x2C, struct.pack("<I", 0))
    emu.write(client + 0x30, struct.pack("<I", 0))

    try:
        expected_names = cpp.call("names").split("|")
        if len(expected_names) != len(TYPE_NAMES):
            raise AssertionError("C++ type-name count")
        for address, expected in zip(TYPE_NAMES, expected_names):
            for _ in range(max(2, cases // 10)):
                result = emu.call(address, eax=0xA5A5A5A5, edx=0x5A5A5A5A)
                assert_abi(emu, address, 0, result)
                actual = c_string(emu, result.eax)
                if mutate_address == address:
                    expected += "_mutated"
                if actual != expected:
                    raise AssertionError(
                        f"0x{address:08X} name {actual!r}/{expected!r}")
            covered.add(address)
        totals["type-names"] = len(TYPE_NAMES) * max(2, cases // 10)

        constructor_cases = max(8, cases // 4)
        for address in CONSTRUCTORS:
            for iteration in range(constructor_cases):
                seed = bytes(rng.randrange(256) for _ in range(0x70))
                resource_blob = bytearray(rng.randrange(256) for _ in range(0x100))
                struct.pack_into("<I", resource_blob, 4, 0)
                has_game_clock = not (
                    address == 0x00925360 and (iteration & 1) != 0)
                duration_bits = bits(
                    (0.1 if (iteration & 2) == 0 else -0.1)
                    if not has_game_clock
                    else (0.5, 2.0, -0.5)[iteration % 3])
                struct.pack_into("<I", resource_blob, 8, duration_bits)
                indexed = (
                    address in (0x00924D10, 0x00924D90)
                    and (iteration & 1) != 0)
                if address in (0x00924D10, 0x00924D90):
                    struct.pack_into("<I", resource_blob, 0, event)
                    emu.write(event, struct.pack("<II", iteration, iteration))
                if indexed:
                    struct.pack_into("<I", resource_blob, 4, 1)
                filter_present = address == 0x00925700 and (iteration & 1) != 0
                local_player = player if (iteration & 1) != 0 else 0
                for offset in range(0x0C, 0x84, 4):
                    struct.pack_into("<I", resource_blob, offset, 0)
                if filter_present:
                    struct.pack_into("<I", resource_blob, 0x0C, 0x1234)
                emu.write(obj, seed + bytes(0x90))
                emu.write(resource, bytes(resource_blob))
                emu.write(client + 0x2C, struct.pack("<I", local_player))
                emu.write(
                    GAME_CLOCK,
                    struct.pack("<I", world if has_game_clock else 0))
                constructor_actions["clear"] = 0
                constructor_actions["refresh"] = 0
                try:
                    result = emu.call(
                        address, [resource], eax=0x11223344,
                        ecx=obj, edx=0x55667788)
                except Exception as error:
                    raise AssertionError(
                        f"0x{address:08X} constructor case {iteration}: "
                        f"{error}") from error
                assert_abi(emu, address, 4, result, eax=obj)
                command = (
                    f"construct 0x{address:08X} {seed.hex()} {obj} {resource} "
                    f"{local_player} {bits(15.0)} {bits(1.0)} "
                    f"{int(has_game_clock)} {int(indexed)} "
                    f"{int(filter_present)} {duration_bits}")
                output = cpp.call(command).split()
                ours = bytes.fromhex(output[0])
                actual = emu.read(obj, 0x70)
                if mutate_address == address and iteration == 0:
                    ours = bytes((ours[0] ^ 1,)) + ours[1:]
                if actual != ours:
                    differing = [
                        index for index, pair in enumerate(zip(actual, ours))
                        if pair[0] != pair[1]]
                    raise AssertionError(
                        f"0x{address:08X} constructor writes differ at {differing} "
                        f"game={actual.hex()} ours={ours.hex()}")
                if constructor_actions["clear"] != int(output[1]):
                    raise AssertionError(
                        f"0x{address:08X} constructor clear count")
                if constructor_actions["refresh"] != int(output[2]):
                    raise AssertionError(
                        f"0x{address:08X} constructor refresh count")
            covered.add(address)
        totals["constructors"] = len(CONSTRUCTORS) * constructor_cases

        # 0x00923A20: the original deliberately returns only AL.
        for has_state in (0, 1):
            for active in (0, 1):
                blob = bytearray(0x40)
                blob[0x1C] = has_state
                blob[0x1D] = active
                emu.write(obj, bytes(blob))
                result = emu.call(0x00923A20, eax=0xCCDD0000, ecx=obj)
                ours = int(cpp.call(f"state {has_state} {active}"))
                assert_abi(emu, 0x00923A20, 0, result, low_byte=ours)
        covered.add(0x00923A20)
        totals["state-active"] = 4

        update_cases = max(20, cases)
        for address, active in ((0x00923A30, 1), (0x00923A50, 0)):
            for _ in range(update_cases):
                enabled = rng.randrange(2)
                tracked = rng.randrange(8)
                emitted = rng.randrange(8)
                initial = rng.randrange(2)
                blob = bytearray(rng.randrange(256) for _ in range(0x40))
                blob[0x0C] = enabled
                blob[0x0D] = initial
                emu.write(obj, bytes(blob))
                emu.write(obj - 4, struct.pack("<I", resource))
                emu.write(resource + 4, struct.pack("<I", tracked))
                emu.write(event + 4, struct.pack("<I", emitted))
                emu.write(holder, struct.pack("<I", event))
                before = emu.read(obj - 4, 0x44)
                result = emu.call(address, [holder], ecx=obj)
                output = cpp.call(
                    f"update {enabled} {tracked} {emitted} {active} "
                    f"{initial} 0 0 0").split()
                expected_state = int(output[0])
                assert_abi(emu, address, 4, result)
                after = emu.read(obj - 4, 0x44)
                expected = bytearray(before)
                expected[0x11] = expected_state
                if after != bytes(expected):
                    raise AssertionError(f"0x{address:08X} write-set mismatch")
            covered.add(address)
        totals["state-events"] = update_cases * 2

        timed_cases = max(8, cases // 5)
        for address, event_kind in ((0x00923AD0, 0), (0x00923B20, 1)):
            for iteration in range(timed_cases):
                enabled = iteration & 1
                matched = (iteration >> 1) & 1
                tracked = rng.randrange(0x100000000)
                emitted = tracked if matched else tracked ^ 1
                blob = bytearray(rng.randrange(256) for _ in range(0x40))
                blob[0x0C] = enabled
                blob[0x0D] = rng.randrange(2)
                emu.write(obj, bytes(blob))
                emu.write(obj - 4, struct.pack("<I", resource))
                emu.write(resource + 4, struct.pack("<II", tracked, event_kind))
                emu.write(event, struct.pack("<II", emitted, emitted))
                emu.write(holder, struct.pack("<I", event))
                emu.write(world + 0x58, struct.pack("<I", 1000))
                before = emu.read(obj - 4, 0x44)
                result = emu.call(address, [holder], ecx=obj)
                output = cpp.call(
                    f"update {enabled} {tracked} {emitted} 0 "
                    f"{blob[0x0D] & 1} {u32(blob, 0x10)} 1000 1").split()
                assert_abi(emu, address, 4, result)
                expected = bytearray(before)
                if enabled and matched:
                    expected[0x11] = 1
                    struct.pack_into("<I", expected, 0x14, 1000)
                if emu.read(obj - 4, 0x44) != bytes(expected):
                    raise AssertionError(f"0x{address:08X} timed write set")
                if int(output[0]) != expected[0x11]:
                    raise AssertionError("timed C++ state")
                if int(output[1]) != u32(expected, 0x14):
                    raise AssertionError("timed C++ frame")
            covered.add(address)
        totals["timed-state-events"] = timed_cases * 2

        compare_cases = max(100, cases * 4)
        for _ in range(compare_cases):
            left = bits(rng.uniform(-1000.0, 1000.0))
            right = bits(rng.uniform(-1000.0, 1000.0))
            for operation in range(5):
                result = emu.call(
                    0x00923C80, [operation, left, right], eax=0xABCD1200)
                ours = int(cpp.call(f"compare {operation} {left} {right}"))
                if mutate_address == 0x00923C80:
                    ours ^= 1
                assert_abi(emu, 0x00923C80, 0, result, low_byte=ours)
        covered.add(0x00923C80)
        totals["comparison"] = compare_cases * 5

        time_cases = max(40, cases)
        time_inputs = [
            (rng.randrange(1, 0x100000000), 15,
             bits(rng.uniform(0.0, 5000.0)))
            for _ in range(time_cases)
        ]
        time_inputs.extend((
            (0x01000001, 1, bits(16777216.0)),
            (0xFFFFFFFF, 0xFFFFFFFF, bits(1.0)),
        ))
        for current, rate, seconds_bits in time_inputs:
            emu.write(world + 0x58, struct.pack("<I", current))
            emu.write(FRAMES_PER_SECOND, struct.pack("<I", rate))
            emu.write(obj + 0x0C, struct.pack("<I", resource))
            emu.write(resource + 4, struct.pack("<I", seconds_bits))
            result = emu.call(0x00923C00, eax=0xDEADBE00, ecx=obj)
            ours = int(cpp.call(f"time {current} {rate} {seconds_bits}"))
            assert_abi(emu, 0x00923C00, 0, result, low_byte=ours)
        covered.add(0x00923C00)
        totals["time-from-start"] = len(time_inputs)

        predicate_calls = {"count": 0}

        def predicate_stub(value: int):
            def invoke(_: Emulator) -> int:
                predicate_calls["count"] += 1
                return value
            return invoke

        predicate_stubs = {
            0: emu.bind_stub(predicate_stub(0)),
            1: emu.bind_stub(predicate_stub(1)),
        }
        children = [emu.alloc(bytes(0x10)), emu.alloc(bytes(0x10))]
        vtables = [emu.alloc(bytes(0x10)), emu.alloc(bytes(0x10))]
        for value in (0, 1):
            emu.write(vtables[value] + 4, struct.pack("<I", predicate_stubs[value]))
            emu.write(children[value], struct.pack("<I", vtables[value]))

        folds = (b"", b"\1", b"\0", b"\1\1", b"\1\0", b"\0\1")
        for address, mode in ((0x00923E20, "all"), (0x00923F50, "any")):
            for values in folds:
                pointers = b"".join(
                    struct.pack("<I", children[value != 0]) for value in values)
                emu.write(child_storage, pointers)
                emu.write(
                    obj + 0x10,
                    struct.pack("<II", child_storage, child_storage + len(pointers)))
                predicate_calls["count"] = 0
                result = emu.call(address, eax=0x12340000, ecx=obj)
                encoded = values.hex() if values else "-"
                output = cpp.call(f"fold {mode} {encoded}").split()
                ours = int(output[0])
                assert_abi(emu, address, 0, result, low_byte=ours)
                if predicate_calls["count"] != int(output[1]):
                    raise AssertionError(f"0x{address:08X} short-circuit count")
            covered.add(address)
        totals["condition-folds"] = len(folds) * 2

        for has_child in (0, 1):
            for value in (0, 1):
                child = children[value] if has_child else 0
                emu.write(obj + 0x10, struct.pack("<I", child))
                predicate_calls["count"] = 0
                result = emu.call(0x00923F00, eax=0x33440000, ecx=obj)
                output = cpp.call(f"not {has_child} {value}").split()
                ours = int(output[0])
                assert_abi(emu, 0x00923F00, 0, result, low_byte=ours)
                if predicate_calls["count"] != int(output[1]):
                    raise AssertionError("not-condition callback count")
        covered.add(0x00923F00)
        totals["not-condition"] = 4

        count_cases = max(40, cases)
        for _ in range(count_cases):
            count = rng.randrange(0x100000000)
            required = rng.randrange(0x100000000)
            emu.write(obj + 0x0C, struct.pack("<I", resource))
            emu.write(obj + 0x30, struct.pack("<I", count))
            emu.write(resource + 0x0C, struct.pack("<I", required))
            result = emu.call(0x00923EF0, eax=0x99880000, ecx=obj)
            ours = int(cpp.call(f"count {count} {required}"))
            assert_abi(emu, 0x00923EF0, 0, result, low_byte=ours)
        covered.add(0x00923EF0)
        totals["engagement-count"] = count_cases

        last_cases = max(40, cases)
        last_inputs = [
            (rng.randrange(2), rng.randrange(2), current, changed, 15,
             bits(rng.uniform(0.0, 4000.0)), rng.randrange(5))
            for current, changed in (
                (value, rng.randrange(0, value + 1))
                for value in (rng.randrange(1000, 100000)
                              for _ in range(last_cases))
            )
        ]
        last_inputs.extend((
            (1, 1, 0x01000001, 0, 1, bits(3600.0), 0),
            (1, 1, 0xFFFFFFFF, 0, 0xFFFFFFFF, bits(1.0), 2),
            (1, 1, 16777217, 0, 16777000, 0x3F80006C, 4),
        ))
        for (has_state, active, current, changed, rate,
             time_bits, operation) in last_inputs:
            emu.write(world + 0x58, struct.pack("<I", current))
            emu.write(FRAMES_PER_SECOND, struct.pack("<I", rate))
            emu.write(obj + 0x0C, struct.pack("<I", resource))
            emu.write(obj + 0x1C, bytes((has_state, active)))
            emu.write(obj + 0x20, struct.pack("<I", changed))
            emu.write(resource + 0x0C, struct.pack("<II", time_bits, operation))
            result = emu.call(0x00923F80, eax=0x77665500, ecx=obj)
            ours = int(cpp.call(
                f"last {has_state} {active} {current} {changed} {rate} "
                f"{time_bits} {operation}"))
            assert_abi(emu, 0x00923F80, 0, result, low_byte=ours)
        covered.add(0x00923F80)
        totals["last-state"] = len(last_inputs)

        # Cached predicate: no-refresh and refresh paths, including exact stores.
        clock_state = {"first": 100, "second": 137, "calls": 0}

        def sampled_clock(_: Emulator) -> int:
            result = (
                clock_state["first"] if clock_state["calls"] == 0
                else clock_state["second"])
            clock_state["calls"] += 1
            return result

        now_stub = emu.bind_stub(sampled_clock)
        emu.write(clock_vtable + 0x74, struct.pack("<I", now_stub))
        emu.write(clock, struct.pack("<I", clock_vtable))
        emu.write(0x00DEEA50, struct.pack("<I", clock))
        for refresh in (0, 1):
            for fresh in (0, 1):
                clock_state["calls"] = 0
                next_check = 99 if refresh else 101
                fresh_calls = {"count": 0}

                def fresh_predicate(
                    _: Emulator, value: int = fresh,
                    calls: dict[str, int] = fresh_calls,
                ) -> int:
                    calls["count"] += 1
                    return value

                vtable = emu.alloc(bytes(0x20))
                fresh_stub = emu.bind_stub(fresh_predicate)
                emu.write(vtable + 0x0C, struct.pack("<I", fresh_stub))
                emu.write(obj, struct.pack("<I", vtable) + bytes(0x6C))
                emu.write(obj + 0x0C, struct.pack("<I", resource))
                emu.write(obj + 0x10, struct.pack("<I", next_check))
                emu.write(obj + 0x14, bytes((1 - fresh,)))
                interval_bits = bits(0.1 if refresh else 2.0)
                emu.write(resource + 4, struct.pack("<I", interval_bits))
                result = emu.call(0x009236C0, eax=0x12340000, ecx=obj)
                output = cpp.call(
                    f"cached {clock_state['first']} {clock_state['second']} "
                    f"{next_check} {1 - fresh} "
                    f"{interval_bits} {bits(15.0)} {fresh}").split()
                assert_abi(emu, 0x009236C0, 0, result, low_byte=int(output[0]))
                if u32(emu.read(obj + 0x10, 4)) != int(output[1]):
                    raise AssertionError("cached next-check write")
                if emu.read(obj + 0x14, 1)[0] != int(output[2]):
                    raise AssertionError("cached result write")
                if fresh_calls["count"] != int(output[3]):
                    raise AssertionError("cached callback count")
                if clock_state["calls"] != int(output[4]):
                    raise AssertionError("cached clock sample count")
        covered.add(0x009236C0)
        totals["cached-condition"] = 4

        # Observer branches distinguish the two queries and the player byte.
        observer_cases = (
            (0, 0, 0, 0, 0),
            (1, 1, 0, 0, 0),
            (1, 0, 1, 0, 0),
            (1, 0, 0, 1, 1),
            (1, 0, 0, 0, 1),
            (1, 0, 0, 0, 0),
        )
        for has_player, first, flag, second, fallback in observer_cases:
            emu.write(client + 0x2C, struct.pack("<I", player if has_player else 0))
            emu.write(player + 0x107, bytes((flag,)))
            emu.write(obj + 0x0C, struct.pack("<I", resource))
            emu.write(resource + 4, struct.pack("<I", fallback))
            emu.bind_function(0x007E1B80, lambda _, value=first: value)
            emu.bind_function(0x007E1BA0, lambda _, value=second: value)
            result = emu.call(0x00923970, eax=0xAABBCC00, ecx=obj)
            ours = int(cpp.call(
                f"observer {has_player} {first} {flag} {second} {fallback}"))
            assert_abi(emu, 0x00923970, 0, result, low_byte=ours)
        covered.add(0x00923970)
        totals["observer"] = len(observer_cases)

        track_refreshes = {"count": 0}

        def refresh_track(_: Emulator) -> int:
            track_refreshes["count"] += 1
            return 0

        emu.bind_function(0x00923780, refresh_track)
        track_cases = max(20, cases // 2)
        for same in (0, 1):
            for _ in range(track_cases):
                played = rng.randrange(50)
                required = rng.randrange(50)
                local = player
                cached = local if same else 0
                count_calls = {"count": 0}

                def played_count(
                    _: Emulator, value: int = played,
                    calls: dict[str, int] = count_calls,
                ) -> int:
                    calls["count"] += 1
                    return value

                track_refreshes["count"] = 0
                emu.write(client + 0x2C, struct.pack("<I", local))
                emu.write(obj + 0x64, struct.pack("<I", cached))
                emu.write(obj + 0x0C, struct.pack("<I", resource))
                emu.write(resource + 8, struct.pack("<I", required))
                emu.bind_function(0x007ECAA0, played_count)
                result = emu.call(0x009241F0, eax=0x10203000, ecx=obj)
                output = cpp.call(
                    f"track {cached} {local} {required} {played}").split()
                assert_abi(
                    emu, 0x009241F0, 0, result, low_byte=int(output[0]))
                if u32(emu.read(obj + 0x64, 4)) != int(output[1]):
                    raise AssertionError("track player cache write")
                if track_refreshes["count"] != int(output[2]):
                    raise AssertionError("track refresh count")
                if count_calls["count"] != int(output[3]):
                    raise AssertionError("track query count")
        covered.add(0x009241F0)
        totals["track-count"] = track_cases * 2

        score_cases = max(30, cases)
        score_inputs = [
            ([rng.randrange(0, 8) for _ in range(rng.randrange(0, 8))],
             rng.randrange(0, 30))
            for _ in range(score_cases)
        ]
        score_inputs.insert(1, ([0], 5))
        score_inputs.append(([0x7FFFFFFE, 10, 1], 0x7FFFFFFF))
        score_actions = {
            "clear": 0, "refresh": 0,
            "replacement_begin": replacement_storage,
            "replacement_end": replacement_storage,
        }

        def clear_scores(_: Emulator) -> int:
            score_actions["clear"] += 1
            return 0

        def refresh_scores(_: Emulator) -> int:
            score_actions["refresh"] += 1
            emu.write(
                obj + 0x10,
                struct.pack(
                    "<II",
                    score_actions["replacement_begin"],
                    score_actions["replacement_end"]))
            return 0

        emu.bind_function(0x009250D0, clear_scores, stack_cleanup=8)
        emu.bind_function(0x00925E40, refresh_scores)
        for case_index, (scores, required) in enumerate(score_inputs):
            replacement_scores = [required] if required > 0 else []
            mode = case_index % 3
            cached_player = player if mode != 2 else 0
            cached_version = 1 if mode == 0 else (0 if mode == 1 else 2)
            current_version = 1
            score_actions["clear"] = 0
            score_actions["refresh"] = 0
            emu.write(client + 0x2C, struct.pack("<I", player))
            emu.write(client + 0x30, struct.pack("<I", current_version))
            emu.write(obj + 0x0C, struct.pack("<I", resource))
            emu.write(
                obj + 0x20,
                struct.pack("<II", cached_player, cached_version))
            emu.write(resource + 4, struct.pack("<I", required))
            records = bytearray(len(scores) * 0x34)
            for index, score in enumerate(scores):
                struct.pack_into("<I", records, index * 0x34 + 0x24, score)
            replacement_records = bytearray(len(replacement_scores) * 0x34)
            for index, score in enumerate(replacement_scores):
                struct.pack_into(
                    "<I", replacement_records, index * 0x34 + 0x24, score)
            emu.write(child_storage, bytes(records))
            emu.write(replacement_storage, bytes(replacement_records))
            score_actions["replacement_begin"] = replacement_storage
            score_actions["replacement_end"] = (
                replacement_storage + len(replacement_records))
            emu.write(
                obj + 0x10,
                struct.pack("<II", child_storage, child_storage + len(records)))
            result = emu.call(0x00926470, eax=0x44556600, ecx=obj)
            encoded_scores = ",".join(str(value) for value in scores) or "-"
            encoded_replacement = (
                ",".join(str(value) for value in replacement_scores) or "-")
            command = " ".join((
                "score", str(cached_player), str(cached_version),
                str(player), str(current_version), str(required),
                encoded_scores, encoded_replacement))
            output = cpp.call(command).split()
            assert_abi(
                emu, 0x00926470, 0, result, low_byte=int(output[0]))
            cached = struct.unpack("<II", emu.read(obj + 0x20, 8))
            if cached != (int(output[1]), int(output[2])):
                raise AssertionError(f"score cache writes {cached}/{output[1:3]}")
            if score_actions["clear"] != int(output[3]):
                raise AssertionError("score clear count")
            if score_actions["refresh"] != int(output[4]):
                raise AssertionError("score refresh count")
            expected_range_calls = 0 if mode == 2 else 1
            if expected_range_calls != int(output[5]):
                raise AssertionError("score range provider count")
        covered.add(0x00926470)
        totals["scored-kills"] = len(score_inputs)

        if covered != ALL_ENTRIES:
            missing = sorted(ALL_ENTRIES - covered)
            extra = sorted(covered - ALL_ENTRIES)
            raise AssertionError(f"coverage mismatch missing={missing} extra={extra}")
        metadata_count, metadata_digest = (
            int(value) for value in cpp.call("entries").split())
        if metadata_count != len(ALL_ENTRIES):
            raise AssertionError(
                f"metadata count {metadata_count}/{len(ALL_ENTRIES)}")
        if metadata_digest != EXPECTED_METADATA_DIGEST:
            raise AssertionError(
                f"metadata digest {metadata_digest}/{EXPECTED_METADATA_DIGEST}")
        totals["entry-metadata"] = metadata_count
    finally:
        cpp.close()

    total = sum(totals.values())
    print(f"GameClient92 MusicScripting: {total} cases, 0 mismatches")
    print(f"  entries: {len(covered)}")
    for name, count in sorted(totals.items()):
        print(f"  {name}: {count}")


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("executable", type=Path)
    parser.add_argument("--cases", type=int, default=100)
    parser.add_argument(
        "--mutation-probe", action="store_true",
        help="invert one expected byte and require the oracle to reject it")
    args = parser.parse_args()
    if args.mutation_probe:
        probes = (TYPE_NAMES[0], CONSTRUCTORS[0], 0x00923C80)
        for address in probes:
            try:
                verify(
                    args.executable, max(8, args.cases // 10),
                    mutate_address=address)
            except AssertionError as error:
                print(
                    f"GameClient92 mutation 0x{address:08X}: detected ({error})")
                continue
            raise SystemExit(f"mutation probe 0x{address:08X} was not detected")
        return
    verify(args.executable, args.cases)


if __name__ == "__main__":
    main()
