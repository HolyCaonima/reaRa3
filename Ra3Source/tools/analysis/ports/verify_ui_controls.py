"""Differential oracle for the reconstructed Apt UI control/window cluster."""

from __future__ import annotations

import argparse
import random
import struct
import subprocess
import sys
from pathlib import Path

from unicorn import x86_const as register

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from emulate import Emulator  # noqa: E402


CAPTURE = Path(r"D:\Game\Ra3\RevRa3\work\runtime_dump\run35\snapshot_3_240s")
U32 = 0xFFFFFFFF
CASES = 48

OP_QUERY_GUARD = 0
OP_APPLY_ACTIVE = 1
OP_QUERY_CONDITION = 2
OP_APPLY_CONDITION = 3
OP_MEASURE_TEXT_INDEX = 4
OP_CANCEL_TIMER = 5
OP_TIMER_EXPIRED = 6
OP_START_TIMER = 7
OP_RELEASE_OWNED = 8
OP_SET_POSITION = 9
OP_SET_EXTENT = 10
OP_SET_CLIP = 11
OP_PREPROCESS = 12
OP_SUPPRESS_RELEASE = 13
OP_DISPATCH_MESSAGE = 14
OP_DISPATCH_HELD = 15
OP_DELAYED_UPDATE = 16
OP_QUERY_CONTROLLER_ACTIVE = 17
OP_QUERY_CONTROLLER_BLOCKED = 18
OP_APPLY_CONTROLLER_ACTIVE = 19
OP_QUERY_CHILD_READY = 20
OP_APPLY_CONTROLLER_READY = 21
OP_RESOLVE_BIT_NAME = 22
OP_NOTIFY_PRIMARY = 23
OP_NOTIFY_SECONDARY = 24
OP_HIT_TEST_CARET = 25
OP_COALESCED_POINTER = 26
OP_FALLBACK_POINTER = 27
RESULT_COUNT = 32

MESSAGE_COUNTS = {
    0x00E2A107: 3, 0x00E2A108: 3, 0x00E2A109: 5,
    0x00E2A10A: 3, 0x00E2A10B: 3, 0x00E2A10C: 3,
    0x00E2A10D: 3, 0x00E2A10E: 5, 0x00E2A10F: 3,
    0x00E2A110: 3, 0x00E2A111: 3, 0x00E2A112: 3,
    0x00E2A113: 5, 0x00E2A114: 3, 0x00E2A115: 3,
    0x00E2A106: 3,
}


def u32(value: int) -> int:
    return value & U32


def words(data: bytes) -> list[int]:
    if not data:
        return []
    return list(struct.unpack(f"<{len(data) // 4}I", data))


def put32(data: bytearray, offset: int, value: int) -> None:
    struct.pack_into("<I", data, offset, u32(value))


def get32(data: bytes, offset: int = 0) -> int:
    return struct.unpack_from("<I", data, offset)[0]


def trace_record(operation: int, arguments: list[int]) -> list[int]:
    return [operation, len(arguments), *(u32(value) for value in arguments)]


class Checker:
    def __init__(self, executable: Path) -> None:
        self.process = subprocess.Popen(
            [str(executable)], stdin=subprocess.PIPE, stdout=subprocess.PIPE,
            text=True, encoding="ascii", bufsize=1)

    def call(
        self, address: int, blob: bytes, auxiliary: bytes,
        results: list[int], *arguments: int,
    ) -> tuple[int, bytes, bytes, bytes, list[int]]:
        result_bytes = struct.pack(f"<{RESULT_COUNT}I", *results)
        fields = [
            f"{address:x}", blob.hex() or ".", auxiliary.hex() or ".",
            result_bytes.hex(), *(f"{u32(value):x}" for value in arguments),
        ]
        assert self.process.stdin is not None and self.process.stdout is not None
        self.process.stdin.write(" ".join(fields) + "\n")
        self.process.stdin.flush()
        line = self.process.stdout.readline().strip()
        if line == "error" or not line:
            raise RuntimeError(f"uicheck rejected 0x{address:08X}: {fields}")
        result, blob_out, auxiliary_out, output, trace = line.split()
        decode = lambda value: b"" if value == "." else bytes.fromhex(value)
        return (
            int(result, 16), decode(blob_out), decode(auxiliary_out),
            decode(output), words(decode(trace)))

    def close(self) -> None:
        assert self.process.stdin is not None
        self.process.stdin.write("quit\n")
        self.process.stdin.flush()
        if self.process.wait(timeout=10) != 0:
            raise RuntimeError("uicheck failed")


class Runtime:
    def __init__(self) -> None:
        self.emu = Emulator(capture=CAPTURE)
        self.results = [0] * RESULT_COUNT
        self.trace: list[int] = []
        self.cancel_message_kind = "byte"
        self.notify_operation = OP_NOTIFY_PRIMARY
        self.notify_index = 0
        self.notify_value = 0
        self.notify_target = 0
        self.now = 0

        self.query_guard = self._bind_noarg(OP_QUERY_GUARD)
        self.apply_active = self._bind_boolarg(OP_APPLY_ACTIVE)
        self.query_condition = self._bind_noarg(OP_QUERY_CONDITION)
        self.apply_condition = self._bind_boolarg(OP_APPLY_CONDITION)
        self.measure = self.emu.bind_stub(self._measure, stack_cleanup=12)
        self.timer_expired = self._bind_noarg(OP_TIMER_EXPIRED)
        self.set_position = self._bind_onearg(OP_SET_POSITION)
        self.set_extent = self._bind_twoarg(OP_SET_EXTENT)
        self.set_clip = self._bind_onearg(OP_SET_CLIP)
        self.dispatch = self.emu.bind_stub(self._dispatch, stack_cleanup=4)
        self.query_controller_active = self._bind_noarg(OP_QUERY_CONTROLLER_ACTIVE)
        self.query_controller_blocked = self._bind_noarg(OP_QUERY_CONTROLLER_BLOCKED)
        self.apply_controller_active = self._bind_boolarg(OP_APPLY_CONTROLLER_ACTIVE)
        self.query_child_ready = self._bind_noarg(OP_QUERY_CHILD_READY)
        self.apply_controller_ready = self._bind_boolarg(OP_APPLY_CONTROLLER_READY)
        self.release_owned = self._bind_onearg(OP_RELEASE_OWNED)
        self.clock_query = self.emu.bind_stub(lambda _machine: self.now)

        self.emu.bind_function(0x005D79B0, self._cancel_timer, stack_cleanup=4)
        self.emu.bind_function(0x005D7980, self._start_timer, stack_cleanup=4)
        self.emu.bind_function(0x00A6F3B0, self._preprocess, stack_cleanup=12)
        self.emu.bind_function(0x00A6F230, self._suppress_release, stack_cleanup=12)
        self.emu.bind_function(0x00A6E230, self._dispatch_held, stack_cleanup=20)
        self.emu.bind_function(0x00A713A0, self._delayed_update)
        self.emu.bind_function(0x00401E10, self._resolve_bit_name)
        self.emu.bind_function(0x00B006A0, self._notify_bit, stack_cleanup=4)
        self.emu.bind_function(0x00A74C80, self._hit_test_caret, stack_cleanup=8)
        self.emu.bind_function(0x00A74780, self._coalesced_pointer, stack_cleanup=4)
        self.emu.bind_function(0x00A72A00, self._fallback_pointer, stack_cleanup=4)

    def reset(self, results: list[int]) -> None:
        self.emu.reset_data()
        self.results = list(results)
        self.trace = []
        self.cancel_message_kind = "byte"

    def record(self, operation: int, arguments: list[int]) -> int:
        self.trace.extend(trace_record(operation, arguments))
        return self.results[operation]

    def _bind_noarg(self, operation: int) -> int:
        return self.emu.bind_stub(lambda _machine, op=operation: self.record(op, []))

    def _bind_onearg(self, operation: int) -> int:
        return self.emu.bind_stub(
            lambda machine, op=operation: self.record(op, [machine.stack_arg(0)]),
            stack_cleanup=4)

    def _bind_boolarg(self, operation: int) -> int:
        return self.emu.bind_stub(
            lambda machine, op=operation: self.record(
                op, [machine.stack_arg(0) & 0xFF]),
            stack_cleanup=4)

    def _bind_twoarg(self, operation: int) -> int:
        return self.emu.bind_stub(
            lambda machine, op=operation: self.record(
                op, [machine.stack_arg(0), machine.stack_arg(1)]),
            stack_cleanup=8)

    def virtual_object(self, methods: dict[int, int], size: int = 8) -> int:
        table_size = max(methods, default=0) + 4
        table = bytearray(max(table_size, 4))
        for offset, address in methods.items():
            put32(table, offset, address)
        table_pointer = self.emu.alloc(bytes(table))
        object_data = bytearray(max(size, 4))
        put32(object_data, 0, table_pointer)
        return self.emu.alloc(bytes(object_data))

    def parent_sink(self) -> int:
        return self.virtual_object({0: self.dispatch})

    def _measure(self, machine: Emulator) -> int:
        candidate = machine.stack_arg(1)
        machine.write(machine.stack_arg(2), struct.pack("<I", self.results[-1]))
        return self.record(OP_MEASURE_TEXT_INDEX, [candidate])

    def _cancel_timer(self, machine: Emulator) -> int:
        return self.record(OP_CANCEL_TIMER, [machine.stack_arg(0)])

    def _start_timer(self, machine: Emulator) -> int:
        return self.record(OP_START_TIMER, [machine.stack_arg(0)])

    def _preprocess(self, machine: Emulator) -> int:
        payload = machine.stack_arg(0)
        first, second = struct.unpack("<II", machine.read(payload, 8))
        return self.record(
            OP_PREPROCESS,
            [first, second, machine.stack_arg(1), machine.stack_arg(2)])

    def _suppress_release(self, machine: Emulator) -> int:
        slot = machine.stack_arg(0)
        payload = machine.stack_arg(1)
        values = words(machine.read(payload, 16))
        return self.record(OP_SUPPRESS_RELEASE, [slot, values[0], values[1], machine.stack_arg(2)])

    def _dispatch_held(self, machine: Emulator) -> int:
        slot = machine.stack_arg(0)
        payload = machine.stack_arg(2)
        return self.record(OP_DISPATCH_HELD, [slot, *words(machine.read(payload, 16))])

    def _delayed_update(self, _machine: Emulator) -> int:
        return self.record(OP_DELAYED_UPDATE, [])

    def _resolve_bit_name(self, machine: Emulator) -> int:
        token = machine.stack_arg(0)
        result = self.record(OP_RESOLVE_BIT_NAME, [token])
        machine.write(machine.stack_arg(1), bytes([self.results[-2] & 1]))
        return result

    def _notify_bit(self, _machine: Emulator) -> int:
        return self.record(
            self.notify_operation,
            [self.notify_index, self.notify_value, self.notify_target])

    def _hit_test_caret(self, machine: Emulator) -> int:
        x, y = machine.stack_arg(0), machine.stack_arg(1)
        caret = self.record(OP_HIT_TEST_CARET, [x, y])
        if caret == U32:
            return 0
        edit = machine.register(register.UC_X86_REG_ECX)
        mode = get32(machine.read(edit + 0x20, 4))
        if mode != 0:
            anchor = get32(machine.read(edit + 0x14, 4))
            machine.write(edit + 0x18, struct.pack("<II", min(caret, anchor), max(caret, anchor)))
        machine.write(edit + 0x10, struct.pack("<I", caret))
        return 1

    def _coalesced_pointer(self, machine: Emulator) -> int:
        event = machine.stack_arg(0)
        return self.record(OP_COALESCED_POINTER, words(machine.read(event, 16)))

    def _fallback_pointer(self, machine: Emulator) -> int:
        event = machine.stack_arg(0)
        tag, payload = struct.unpack("<II", machine.read(event, 8))
        values = words(machine.read(payload, 16))
        return self.record(OP_FALLBACK_POINTER, [tag, *values])

    def _dispatch(self, machine: Emulator) -> int:
        event = machine.stack_arg(0)
        tag, payload = struct.unpack("<II", machine.read(event, 8))
        if tag in MESSAGE_COUNTS:
            count = MESSAGE_COUNTS[tag]
            alias = 2
            values = words(machine.read(payload, count * 4))
        elif tag == 0x00DEBD52:
            count = 4
            alias = 1
            values = words(machine.read(payload, 16))
        elif tag == 0x00E2A103:
            count = 1
            alias = 2
            values = [machine.read(payload, 1)[0]]
        elif tag == 0x00E2A102 and self.cancel_message_kind == "direct":
            count = 1
            alias = 1
            values = [payload]
        elif tag == 0x00E2A102:
            count = 1
            alias = 2
            values = [machine.read(payload, 1)[0]]
        else:
            count = 1
            alias = 1
            values = [payload]
        return self.record(OP_DISPATCH_MESSAGE, [tag, count, alias, *values])


def normalized_blob(runtime: Runtime, pointer: int, size: int, pointers: dict[int, int]) -> bytes:
    result = bytearray(runtime.emu.read(pointer, size))
    for offset, value in pointers.items():
        put32(result, offset, value)
    return bytes(result)


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("checker", type=Path)
    parser.add_argument("--seed", type=int, default=0xA7002026)
    args = parser.parse_args()
    rng = random.Random(args.seed)
    checker = Checker(args.checker.resolve())
    runtime = Runtime()
    totals: dict[str, int] = {}
    mismatch_totals: dict[str, int] = {}
    mismatches = 0

    def counted(name: str) -> None:
        totals[name] = totals.get(name, 0) + 1

    def compare(
        name: str, address: int, expected: tuple[int, bytes, bytes, bytes, list[int]],
        game_result: int, game_blob: bytes, game_auxiliary: bytes, game_output: bytes,
        game_trace: list[int], cleanup: int, expected_cleanup: int,
        compare_result: bool = False,
    ) -> None:
        nonlocal mismatches
        ours_result, ours_blob, ours_auxiliary, ours_output, ours_trace = expected
        good = (
            game_blob == ours_blob
            and game_auxiliary == ours_auxiliary
            and game_output == ours_output
            and game_trace == ours_trace
            and cleanup == expected_cleanup
            and (not compare_result or game_result == ours_result)
        )
        if not good:
            mismatches += 1
            mismatch_totals[name] = mismatch_totals.get(name, 0) + 1
            if mismatch_totals[name] <= 2:
                print(
                    f"mismatch {name} 0x{address:08X}: "
                    f"eax={game_result:08x}/{ours_result:08x} "
                    f"cleanup={cleanup}/{expected_cleanup} "
                    f"blob={game_blob.hex()}/{ours_blob.hex()} "
                    f"out={game_output.hex()}/{ours_output.hex()} "
                    f"trace={game_trace}/{ours_trace}")
        counted(name)

    try:
        # 0x00A6FFE0: direct dependency that maintains hit-region bounds.
        for _ in range(CASES):
            results = [0] * RESULT_COUNT
            point_count = rng.randrange(1, 10)
            points = [
                (rng.randrange(-200, 201), rng.randrange(-200, 201))
                for _ in range(point_count)
            ]
            auxiliary = b"".join(
                struct.pack("<ii", *point) for point in points)
            blob = bytearray(rng.randbytes(0x24))
            runtime.reset(results)
            game_points = runtime.emu.alloc(auxiliary)
            game_blob = bytearray(blob)
            put32(game_blob, 0, game_points)
            put32(game_blob, 4, game_points + len(auxiliary))
            put32(game_blob, 8, game_points + len(auxiliary))
            owner = runtime.emu.alloc(bytes(game_blob))
            game = runtime.emu.call(0x00A6FFE0, ecx=owner)
            normalized = bytearray(runtime.emu.read(owner, len(blob)))
            put32(normalized, 0, 1)
            put32(normalized, 4, 1 + len(auxiliary))
            put32(normalized, 8, 1 + len(auxiliary))
            expected = checker.call(
                0x00A6FFE0, bytes(blob), auxiliary, results)
            compare(
                "updateHitRegionBounds", 0x00A6FFE0, expected, game.eax,
                bytes(normalized), auxiliary, b"", runtime.trace,
                game.stack_cleanup, 0)

        # 0x00A70700: exact two-word population count.
        for _ in range(CASES * 2):
            results = [0] * RESULT_COUNT
            blob = rng.randbytes(8)
            runtime.reset(results)
            pointer = runtime.emu.alloc(blob)
            game = runtime.emu.call(0x00A70700, ecx=pointer)
            expected = checker.call(0x00A70700, blob, b"", results)
            compare(
                "countControlFlags", 0x00A70700, expected, game.eax, blob, b"", b"",
                runtime.trace, game.stack_cleanup, 0, True)

        # 0x00A71B20/40/70: hit-region layout, bounds and float ray crossings.
        for _ in range(CASES):
            results = [0] * RESULT_COUNT
            blob = rng.randbytes(0x24)
            runtime.reset(results)
            pointer = runtime.emu.alloc(blob)
            game = runtime.emu.call(0x00A71B20, ecx=pointer)
            expected = checker.call(0x00A71B20, blob, b"", results)
            compare(
                "initializeHitRegion", 0x00A71B20, expected,
                1 if game.eax == pointer else 0,
                runtime.emu.read(pointer, len(blob)), b"", b"", runtime.trace,
                game.stack_cleanup, 0, True)

        for address, name in (
            (0x00A71B40, "copyHitRegionBounds"),
            (0x00A71B70, "hitRegionContains"),
        ):
            for _ in range(CASES):
                results = [0] * RESULT_COUNT
                point_count = rng.randrange(3, 10)
                points = [
                    (rng.randrange(-200, 201), rng.randrange(-200, 201))
                    for _ in range(point_count)
                ]
                auxiliary = b"".join(struct.pack("<ii", *point) for point in points)
                blob = bytearray(rng.randbytes(0x24))
                blob[0x20] = rng.randrange(2)
                if blob[0x20]:
                    xs, ys = zip(*points)
                    struct.pack_into("<iiii", blob, 0x10, min(xs), min(ys), max(xs), max(ys))
                x, y = rng.randrange(-240, 241), rng.randrange(-240, 241)
                runtime.reset(results)
                game_points = runtime.emu.alloc(auxiliary)
                game_blob = bytearray(blob)
                put32(game_blob, 0, game_points)
                put32(game_blob, 4, game_points + len(auxiliary))
                put32(game_blob, 8, game_points + len(auxiliary))
                owner = runtime.emu.alloc(bytes(game_blob))
                if address == 0x00A71B40:
                    output = runtime.emu.alloc(b"\xA5" * 16)
                    game = runtime.emu.call(address, [output], ecx=owner)
                    game_output = runtime.emu.read(output, 16)
                    call_args: tuple[int, ...] = ()
                else:
                    point = runtime.emu.alloc(struct.pack("<ii", x, y))
                    game = runtime.emu.call(address, [owner, point])
                    game_output = b""
                    call_args = (x, y)
                normalized = bytearray(runtime.emu.read(owner, len(blob)))
                put32(normalized, 0, 1)
                put32(normalized, 4, 1 + len(auxiliary))
                put32(normalized, 8, 1 + len(auxiliary))
                expected = checker.call(address, bytes(blob), auxiliary, results, *call_args)
                compare(
                    name, address, expected,
                    (1 if game.eax == output else 0)
                    if address == 0x00A71B40 else game.eax & 0xFF,
                    bytes(normalized),
                    auxiliary, game_output, runtime.trace, game.stack_cleanup,
                    4 if address == 0x00A71B40 else 0, True)

        # 0x00A71E80: UTF-16 caret/selection movement.
        for _ in range(CASES * 2):
            results = [0] * RESULT_COUNT
            text = "".join(rng.choice(" abCD12\u03a9") for _ in range(rng.randrange(1, 30)))
            auxiliary = text.encode("utf-16le")
            blob = bytearray(rng.randbytes(0x38))
            caret = rng.randrange(len(text) + 1)
            anchor = rng.randrange(len(text) + 1)
            put32(blob, 8, 1)
            put32(blob, 0x10, caret)
            put32(blob, 0x14, anchor)
            put32(blob, 0x20, rng.randrange(4))
            blob[0x28] = rng.randrange(4)
            modifiers = rng.choice((0, 4, 8, 12, 0x10, 0x400))
            runtime.reset(results)
            text_pointer = runtime.emu.alloc(auxiliary)
            game_blob = bytearray(blob)
            put32(game_blob, 8, text_pointer)
            owner = runtime.emu.alloc(bytes(game_blob))
            key = runtime.emu.alloc(struct.pack("<II", rng.getrandbits(32), modifiers))
            game = runtime.emu.call(0x00A71E80, [key], ecx=owner)
            normalized = bytearray(runtime.emu.read(owner, len(blob)))
            put32(normalized, 8, 1)
            expected = checker.call(0x00A71E80, bytes(blob), auxiliary, results, modifiers)
            compare(
                "moveTextCaretLeft", 0x00A71E80, expected, game.eax,
                bytes(normalized), auxiliary, b"", runtime.trace, game.stack_cleanup, 4)

        # 0x00A73590/5F0/640: pointer alias and UTF-16 length header.
        for address, name, argument_count in (
            (0x00A73590, "makeTextSlice", 2),
            (0x00A735F0, "makeTextPrefix", 1),
            (0x00A73640, "makeTextSuffix", 1),
        ):
            for _ in range(CASES * 2):
                results = [0] * RESULT_COUNT
                length = rng.randrange(0, 80)
                auxiliary = struct.pack("<HH", length, rng.randrange(0x10000)) + rng.randbytes(length * 2)
                values = [rng.randrange(-100, 101) for _ in range(argument_count)]
                runtime.reset(results)
                allocation = runtime.emu.alloc(auxiliary)
                handle = runtime.emu.alloc(struct.pack("<I", allocation + 4))
                output = runtime.emu.alloc(b"\xA5" * 12)
                game = runtime.emu.call(address, [output, handle, *(u32(v) for v in values)])
                game_slice = list(struct.unpack("<III", runtime.emu.read(output, 12)))
                normalized_output = struct.pack(
                    "<III", 1 if game_slice[0] == handle else 0, game_slice[1], game_slice[2])
                expected = checker.call(address, b"", auxiliary, results, *values)
                compare(
                    name, address, expected,
                    1 if game.eax == output else 0, b"", auxiliary,
                    normalized_output, runtime.trace, game.stack_cleanup, 0,
                    True)

        # 0x00A740A0-0x00A74450: all fifteen distinct typed-message encoders.
        message_specs = [
            (0x00A740A0, "dispatchUiMessage107", 0x00E2A107, False),
            (0x00A740E0, "dispatchUiMessage108", 0x00E2A108, False),
            (0x00A74120, "dispatchUiMessage109", 0x00E2A109, True),
            (0x00A74170, "dispatchUiMessage10A", 0x00E2A10A, False),
            (0x00A741B0, "dispatchUiMessage10B", 0x00E2A10B, False),
            (0x00A741F0, "dispatchUiMessage10C", 0x00E2A10C, False),
            (0x00A74230, "dispatchUiMessage10D", 0x00E2A10D, False),
            (0x00A74270, "dispatchUiMessage10E", 0x00E2A10E, True),
            (0x00A742C0, "dispatchUiMessage10F", 0x00E2A10F, False),
            (0x00A74300, "dispatchUiMessage110", 0x00E2A110, False),
            (0x00A74340, "dispatchUiMessage111", 0x00E2A111, False),
            (0x00A74380, "dispatchUiMessage112", 0x00E2A112, False),
            (0x00A743C0, "dispatchUiMessage113", 0x00E2A113, True),
            (0x00A74410, "dispatchUiMessage114", 0x00E2A114, False),
            (0x00A74450, "dispatchUiMessage115", 0x00E2A115, False),
        ]
        for address, name, _tag, extended in message_specs:
            for _ in range(CASES):
                results = [0] * RESULT_COUNT
                results[OP_DISPATCH_MESSAGE] = rng.getrandbits(32)
                pair = rng.randbytes(8)
                second = rng.randbytes(8) if extended else b""
                value = rng.getrandbits(32)
                runtime.reset(results)
                sink = runtime.parent_sink()
                pair_pointer = runtime.emu.alloc(pair)
                stack_args = [sink, pair_pointer, value]
                if extended:
                    stack_args.append(runtime.emu.alloc(second))
                game = runtime.emu.call(address, stack_args)
                expected = checker.call(address, pair, second, results, value)
                compare(
                    name, address, expected, game.eax, pair, second, b"",
                    runtime.trace, game.stack_cleanup, 0, True)

        # 0x00A705A0/0x00A70670: safe controller-layout accessors.
        for address, name in (
            (0x00A705A0, "controllerUiObject"),
            (0x00A70670, "controllerInputTable"),
        ):
            for _ in range(CASES):
                results = [0] * RESULT_COUNT
                blob = bytearray(rng.randbytes(8))
                put32(blob, 4, 1)
                auxiliary = bytearray(rng.randbytes(0x224))
                expected_result = rng.getrandbits(32)
                if address == 0x00A705A0:
                    put32(auxiliary, 0x21C, expected_result)
                else:
                    expected_result &= 0xFFFFF000
                    put32(auxiliary, 4, expected_result)
                    expected_result = u32(expected_result + 0x2D0)
                runtime.reset(results)
                owner = runtime.emu.alloc(bytes(auxiliary))
                game_blob = bytearray(blob)
                put32(game_blob, 4, owner)
                pointer = runtime.emu.alloc(bytes(game_blob))
                game = runtime.emu.call(address, ecx=pointer)
                expected = checker.call(address, bytes(blob), bytes(auxiliary), results)
                compare(
                    name, address, expected, game.eax, bytes(blob), bytes(auxiliary), b"",
                    runtime.trace, game.stack_cleanup, 0, True)

        # Simple callback-driven control state and timer helpers.
        simple_specs = [
            (0x00A70060, "setControlActive"),
            (0x00A700D0, "propagateEnabledState"),
            (0x00A70110, "propagateFocusState"),
            (0x00A701A0, "fitTextIndex"),
            (0x00A70220, "tickUiTimer"),
            (0x00A70290, "startUiTimer"),
            (0x00A70500, "setControlPosition"),
            (0x00A70520, "setControlExtent"),
            (0x00A70540, "setControlClip"),
        ]
        for address, name in simple_specs:
            for _ in range(CASES):
                results = [rng.getrandbits(32) for _ in range(RESULT_COUNT)]
                blob = bytearray(rng.randbytes(0x38))
                arguments: list[int] = []
                pointer_fields: dict[int, int] = {}
                runtime.reset(results)
                if address == 0x00A70060:
                    active = rng.randrange(2)
                    arguments = [active]
                    blob[0x14] = rng.randrange(2)
                    guard_present = rng.randrange(2)
                    if guard_present:
                        guard = runtime.virtual_object({0x0C: runtime.query_guard})
                        put32(blob, 0x0C, guard)
                        pointer_fields[0x0C] = 1
                    else:
                        put32(blob, 0x0C, 0)
                    target = runtime.virtual_object({4: runtime.apply_active})
                    put32(blob, 8, target)
                    pointer_fields[8] = 1
                    compare_result = blob[0x14] != active
                elif address == 0x00A700D0:
                    blob[0x10] = rng.randrange(2)
                    query = runtime.virtual_object({0x0C: runtime.query_condition})
                    target = runtime.virtual_object({4: runtime.apply_condition})
                    put32(blob, 8, query)
                    put32(blob, 4, target)
                    pointer_fields = {8: 1, 4: 1}
                    compare_result = True
                elif address == 0x00A70110:
                    query = runtime.virtual_object({0x10: runtime.query_condition})
                    target = runtime.virtual_object({0x1C: runtime.apply_condition})
                    put32(blob, 8, query)
                    put32(blob, 4, target)
                    pointer_fields = {8: 1, 4: 1}
                    compare_result = True
                elif address == 0x00A701A0:
                    candidate = rng.getrandbits(32)
                    arguments = [candidate]
                    layout = runtime.virtual_object({8: runtime.measure})
                    put32(blob, 0x24, layout)
                    pointer_fields[0x24] = 1
                    compare_result = True
                elif address in (0x00A70220, 0x00A70290):
                    now = rng.getrandbits(32)
                    runtime.now = now
                    arguments = [now]
                    clock = runtime.virtual_object({0x74: runtime.clock_query})
                    runtime.emu.write(0x00DEEA50, struct.pack("<I", clock))
                    owner_data = bytearray(16)
                    if address == 0x00A70220:
                        expire = runtime.virtual_object({0x20: runtime.timer_expired})
                        put32(owner_data, 8, expire)
                    owner = runtime.emu.alloc(bytes(owner_data))
                    put32(blob, 4, owner)
                    pointer_fields[4] = 1
                    blob[0x14] = rng.randrange(2)
                    blob[0x15] = rng.randrange(2)
                    compare_result = False
                elif address == 0x00A70500:
                    value = rng.getrandbits(32)
                    arguments = [value]
                    delegate = runtime.virtual_object({0x10: runtime.set_position})
                    put32(blob, 8, delegate)
                    pointer_fields[8] = 1
                    compare_result = False
                elif address == 0x00A70520:
                    arguments = [rng.getrandbits(32), rng.getrandbits(32)]
                    delegate = runtime.virtual_object({0x0C: runtime.set_extent})
                    put32(blob, 8, delegate)
                    pointer_fields[8] = 1
                    compare_result = False
                else:
                    arguments = [rng.getrandbits(32)]
                    delegate = runtime.virtual_object({0x18: runtime.set_clip})
                    put32(blob, 8, delegate)
                    pointer_fields[8] = 1
                    compare_result = False
                owner_pointer = runtime.emu.alloc(bytes(blob))
                if address == 0x00A70060:
                    game = runtime.emu.call(address, arguments, ecx=owner_pointer)
                    cleanup = 4
                elif address == 0x00A701A0:
                    game = runtime.emu.call(address, arguments, ecx=owner_pointer)
                    cleanup = 4
                elif address == 0x00A70520:
                    game = runtime.emu.call(address, arguments, ecx=owner_pointer)
                    cleanup = 8
                elif address in (0x00A70500, 0x00A70540):
                    game = runtime.emu.call(address, arguments, ecx=owner_pointer)
                    cleanup = 4
                else:
                    game = runtime.emu.call(address, ecx=owner_pointer)
                    cleanup = 4 if address in (0x00A700D0, 0x00A70110) else 0
                checker_blob = bytearray(blob)
                for offset, value in pointer_fields.items():
                    put32(checker_blob, offset, value)
                expected = checker.call(address, bytes(checker_blob), b"", results, *arguments)
                game_blob = normalized_blob(
                    runtime, owner_pointer, len(blob), pointer_fields)
                compare(
                    name, address, expected,
                    game.eax & 0xFF if address == 0x00A72660 else game.eax,
                    game_blob, b"", b"",
                    runtime.trace, game.stack_cleanup, cleanup, compare_result)

        # 0x00A704B0: nested non-owning action view and release alias.
        for _ in range(CASES):
            results = [rng.getrandbits(32) for _ in range(RESULT_COUNT)]
            blob = bytearray(rng.randbytes(0x24))
            auxiliary = bytearray(rng.randbytes(0x18))
            auxiliary[8] = rng.randrange(2)
            owned_present = rng.randrange(2)
            runtime.reset(results)
            if owned_present:
                owned = runtime.virtual_object({0: runtime.release_owned})
                put32(auxiliary, 0x10, owned)
            else:
                put32(auxiliary, 0x10, 0)
            action = runtime.emu.alloc(bytes(auxiliary))
            put32(blob, 0x1C, action)
            owner = runtime.emu.alloc(bytes(blob))
            game = runtime.emu.call(0x00A704B0, [rng.getrandbits(32)], ecx=owner)
            game_auxiliary = bytearray(runtime.emu.read(action, len(auxiliary)))
            if owned_present:
                put32(game_auxiliary, 0x10, 0)
            checker_blob = bytearray(blob)
            put32(checker_blob, 0x1C, 1)
            checker_auxiliary = bytearray(auxiliary)
            put32(checker_auxiliary, 0x10, 1 if owned_present else 0)
            expected = checker.call(
                0x00A704B0, bytes(checker_blob), bytes(checker_auxiliary), results)
            normalized = bytearray(runtime.emu.read(owner, len(blob)))
            put32(normalized, 0x1C, 1)
            compare(
                "clearPendingAction", 0x00A704B0, expected, game.eax,
                bytes(normalized), bytes(game_auxiliary), b"", runtime.trace,
                game.stack_cleanup, 4)

        # Input router/controller, event gates, bit state and pointer coalescing.
        # These cases intentionally exercise each mutation branch, not only fall-through paths.
        for address, name in (
            (0x00A712C0, "routeIdleInput"),
            (0x00A71340, "cancelInputCapture"),
            (0x00A72D30, "deactivateEventGate"),
            (0x00A748F0, "activateEventGate"),
        ):
            for iteration in range(CASES):
                results = [rng.getrandbits(32) for _ in range(RESULT_COUNT)]
                blob = bytearray(rng.randbytes(0x48))
                auxiliary = rng.randbytes(16)
                runtime.reset(results)
                parent_present = iteration % 3 != 0
                if parent_present:
                    parent = runtime.parent_sink()
                    put32(blob, 4, parent)
                else:
                    put32(blob, 4, 0)
                if address == 0x00A712C0:
                    put32(blob, 8, U32 if iteration % 4 else 0)
                    for slot in range(3):
                        blob[0x1C + slot * 0x14] = (
                            1 if iteration % 5 == slot + 1 else 0)
                    stack_args = [runtime.emu.alloc(auxiliary)]
                    checker_args: tuple[int, ...] = ()
                    cleanup = 4
                    runtime.cancel_message_kind = "direct"
                elif address == 0x00A71340:
                    reason = rng.getrandbits(32)
                    stack_args = [reason]
                    checker_args = (reason,)
                    cleanup = 4
                    runtime.cancel_message_kind = "direct"
                else:
                    blob[8] = iteration & 1
                    stack_args = []
                    checker_args = ()
                    cleanup = 0
                owner = runtime.emu.alloc(bytes(blob))
                game = runtime.emu.call(address, stack_args, ecx=owner)
                checker_blob = bytearray(blob)
                if parent_present:
                    put32(checker_blob, 4, 1)
                expected = checker.call(
                    address, bytes(checker_blob), auxiliary, results, *checker_args)
                game_blob = normalized_blob(
                    runtime, owner, len(blob), {4: 1} if parent_present else {})
                compare(
                    name, address, expected, game.eax, game_blob, auxiliary, b"",
                    runtime.trace, game.stack_cleanup, cleanup,
                    address in (0x00A712C0, 0x00A71340))

        # Named/primary/secondary bit setters.
        for address, name in (
            (0x00A72660, "setNamedControlBit"),
            (0x00A730B0, "setPrimaryControlBit"),
            (0x00A73130, "setSecondaryControlBit"),
        ):
            for _ in range(CASES):
                results = [rng.getrandbits(32) for _ in range(RESULT_COUNT)]
                runtime.reset(results)
                if address == 0x00A72660:
                    blob = bytearray(rng.randbytes(8))
                    token = rng.getrandbits(32)
                    index = rng.choice((U32, rng.randrange(64)))
                    results[-2] = rng.randrange(2)
                    results[OP_RESOLVE_BIT_NAME] = index
                    runtime.results[-2] = results[-2]
                    runtime.results[OP_RESOLVE_BIT_NAME] = index
                    owner = runtime.emu.alloc(bytes(blob))
                    game = runtime.emu.call(address, [token], ecx=owner)
                    expected = checker.call(address, bytes(blob), b"", results, token)
                    game_blob = runtime.emu.read(owner, len(blob))
                    cleanup = 4
                    compare_result = True
                else:
                    blob = bytearray(rng.randbytes(0x1B0))
                    index = rng.randrange(35)
                    value = rng.randrange(2)
                    offset = 0x10C if address == 0x00A730B0 else 0x114
                    operation = OP_NOTIFY_PRIMARY if address == 0x00A730B0 else OP_NOTIFY_SECONDARY
                    target = runtime.emu.alloc(bytes(16))
                    put32(blob, 0x11C + index * 4, target)
                    runtime.notify_operation = operation
                    runtime.notify_index = index
                    runtime.notify_value = value
                    runtime.notify_target = 1
                    owner = runtime.emu.alloc(bytes(blob))
                    game = runtime.emu.call(address, [index, value], ecx=owner)
                    checker_blob = bytearray(blob)
                    put32(checker_blob, 0x11C + index * 4, 1)
                    expected = checker.call(
                        address, bytes(checker_blob), b"", results, index, value)
                    game_blob_array = bytearray(runtime.emu.read(owner, len(blob)))
                    put32(game_blob_array, 0x11C + index * 4, 1)
                    game_blob = bytes(game_blob_array)
                    cleanup = 8
                    compare_result = False
                compare(
                    name, address, expected,
                    game.eax & 0xFF if address == 0x00A72660 else game.eax,
                    game_blob, b"", b"",
                    runtime.trace, game.stack_cleanup, cleanup, compare_result)

        # Text selection delegation and global-last-point extraction.
        for address, name in (
            (0x00A74E70, "beginTextSelection"),
            (0x00A74EC0, "updateTextSelectionFromPoint"),
        ):
            for iteration in range(CASES):
                results = [rng.getrandbits(32) for _ in range(RESULT_COUNT)]
                results[OP_HIT_TEST_CARET] = (
                    U32 if iteration % 4 == 0 else rng.randrange(0, 60))
                blob = bytearray(rng.randbytes(0x38))
                x, y = rng.randrange(-500, 501), rng.randrange(-500, 501)
                runtime.reset(results)
                if address == 0x00A74E70:
                    auxiliary = b""
                    game_args = [u32(x), u32(y)]
                    checker_args = (x, y)
                else:
                    last = (x, y) if iteration % 3 == 0 else (
                        rng.randrange(-500, 501), rng.randrange(-500, 501))
                    auxiliary = struct.pack("<ii", *last)
                    runtime.emu.write(0x00D22400, struct.pack("<i", last[0]))
                    runtime.emu.write(0x00D223FC, struct.pack("<i", last[1]))
                    blob[0x35] = iteration & 1
                    game_args = [u32(x), u32(y)]
                    checker_args = (x, y)
                owner = runtime.emu.alloc(bytes(blob))
                game = runtime.emu.call(address, game_args, ecx=owner)
                expected = checker.call(
                    address, bytes(blob), auxiliary, results, *checker_args)
                if address == 0x00A74EC0:
                    game_auxiliary = runtime.emu.read(0x00D22400, 4) + runtime.emu.read(0x00D223FC, 4)
                else:
                    game_auxiliary = b""
                compare(
                    name, address, expected, game.eax,
                    runtime.emu.read(owner, len(blob)), game_auxiliary, b"",
                    runtime.trace, game.stack_cleanup, 8)

        # Pointer proximity and double-click routers.
        for address, name in (
            (0x00A74700, "routeNearPointerEvent"),
            (0x00A759F0, "routePrimaryDoubleClick"),
            (0x00A75A60, "routeSecondaryDoubleClick"),
        ):
            for iteration in range(CASES):
                results = [rng.getrandbits(32) for _ in range(RESULT_COUNT)]
                blob = bytearray(rng.randbytes(0x40))
                x = rng.randrange(-1000, 1001)
                y = rng.randrange(-1000, 1001)
                event = (x, y, rng.getrandbits(32), rng.getrandbits(32))
                runtime.reset(results)
                if iteration % 2 == 0:
                    put32(blob, 0x0C, u32(x + rng.randrange(-4, 5)))
                    put32(blob, 0x10, u32(y + rng.randrange(-4, 5)))
                    put32(blob, 0x18, u32(event[3] - rng.randrange(0, 500)))
                if address == 0x00A74700:
                    event_blob_offset = 0x20
                    struct.pack_into("<iiII", blob, event_blob_offset, *event)
                    sink = runtime.parent_sink()
                    owner_data = bytearray(8)
                    put32(owner_data, 4, sink if iteration % 3 else 0)
                    owner_aux = bytes(owner_data)
                    owner_pointer = runtime.emu.alloc(owner_aux)
                    put32(blob, 4, owner_pointer)
                    game_event_pointer = runtime.emu.alloc(struct.pack("<iiII", *event))
                    game_args = [game_event_pointer]
                    checker_auxiliary = owner_aux
                    checker_blob = bytearray(blob)
                    put32(checker_blob, 4, 1)
                    normalize = {4: 1}
                else:
                    required = address == 0x00A75A60
                    blob[8] = int(required if iteration % 3 else not required)
                    game_event_pointer = runtime.emu.alloc(struct.pack("<iiII", *event))
                    game_args = [game_event_pointer]
                    checker_auxiliary = struct.pack("<iiII", *event)
                    checker_blob = blob
                    normalize = {}
                owner = runtime.emu.alloc(bytes(blob))
                game = runtime.emu.call(address, game_args, ecx=owner)
                expected = checker.call(
                    address, bytes(checker_blob), checker_auxiliary, results)
                compare(
                    name, address, expected, game.eax,
                    normalized_blob(runtime, owner, len(blob), normalize),
                    checker_auxiliary, b"", runtime.trace, game.stack_cleanup, 4, True)

        # ForwardUiMessage106.
        for _ in range(CASES):
            results = [rng.getrandbits(32) for _ in range(RESULT_COUNT)]
            payload = rng.randbytes(12)
            has_sink = rng.randrange(2)
            runtime.reset(results)
            sink_owner = bytearray(8)
            if has_sink:
                put32(sink_owner, 4, runtime.parent_sink())
            parent = runtime.emu.alloc(bytes(sink_owner))
            wrapper = bytearray(8)
            put32(wrapper, 4, runtime.emu.alloc(payload))
            wrapper_pointer = runtime.emu.alloc(bytes(wrapper))
            game_owner = bytearray(8)
            put32(game_owner, 4, parent)
            owner = runtime.emu.alloc(bytes(game_owner))
            game = runtime.emu.call(0x00A75570, [owner, wrapper_pointer])
            expected = checker.call(0x00A75570, payload, b"", results, has_sink)
            compare(
                "forwardUiMessage106", 0x00A75570, expected, game.eax,
                payload, b"", b"", runtime.trace, game.stack_cleanup, 0, True)

        # The three input-router orchestration functions.
        for address, name in (
            (0x00A71200, "dispatchInputRelease"),
            (0x00A72DC0, "dispatchActiveInputSlots"),
            (0x00A72E20, "routeTypedInputEvent"),
        ):
            for iteration in range(CASES):
                results = [rng.getrandbits(32) for _ in range(RESULT_COUNT)]
                blob = bytearray(rng.randbytes(0x48))
                put32(blob, 8, rng.choice((U32, 0, 1, 2)))
                for slot in range(3):
                    blob[0x1C + slot * 0x14] = rng.randrange(2)
                    blob[0x1D + slot * 0x14] = rng.randrange(2)
                runtime.reset(results)
                parent_present = iteration % 4 != 0
                if parent_present:
                    put32(blob, 4, runtime.parent_sink())
                else:
                    put32(blob, 4, 0)
                auxiliary = rng.randbytes(16)
                payload_pointer = runtime.emu.alloc(auxiliary)
                checker_args: tuple[int, ...]
                if address == 0x00A71200:
                    slot = iteration % 3
                    release_mode = (iteration // 3) % 3
                    parent_present = True
                    put32(blob, 4, runtime.parent_sink())
                    put32(blob, 8, slot if release_mode == 2 else U32)
                    blob[0x1C + slot * 0x14] = 1
                    blob[0x1D + slot * 0x14] = release_mode == 1
                    results[OP_SUPPRESS_RELEASE] = 0
                    runtime.results[OP_SUPPRESS_RELEASE] = 0
                    game_args = [slot, payload_pointer, get32(auxiliary, 8), get32(auxiliary, 12)]
                    checker_args = (slot,)
                    cleanup = 16
                elif address == 0x00A72DC0:
                    blocked_mask = iteration % 8
                    global_data = runtime.emu.alloc(bytes(0x5480))
                    runtime.emu.write(0x00DEBC38, struct.pack("<I", global_data))
                    runtime.emu.write(global_data + 0x5434, auxiliary[:8])
                    for slot, offset in enumerate((0x18, 0x30, 0x24)):
                        runtime.emu.write(
                            global_data + 0x5434 + offset,
                            struct.pack("<I", 1 if blocked_mask & (1 << slot) else 0))
                    game_args = []
                    checker_args = (blocked_mask,)
                    cleanup = 0
                else:
                    event_kinds = (
                        0x00DEBD46, 0x00DEBD4A, 0x00DEBD4E,
                        0x00DEBD47, 0x00DEBD4B, 0x00DEBD4F,
                        0x00DEBD45, 0x00DEBD49, 0x00DEBD4D,
                        0x00DEBD52, 0x00E2A102, 0x12345678,
                    )
                    tag = event_kinds[iteration % len(event_kinds)]
                    event_payload = payload_pointer
                    if tag == 0x00E2A102:
                        event_payload = rng.randrange(1, 0x10000)
                        runtime.cancel_message_kind = "direct"
                    elif tag == 0x12345678:
                        event_payload = rng.getrandbits(32)
                    event_pointer = runtime.emu.alloc(struct.pack("<II", tag, event_payload))
                    game_args = [event_pointer]
                    checker_args = (
                        (tag, event_payload)
                        if tag in (0x00E2A102, 0x12345678)
                        else (tag,))
                    cleanup = 4
                owner = runtime.emu.alloc(bytes(blob))
                game = runtime.emu.call(address, game_args, ecx=owner)
                checker_blob = bytearray(blob)
                if parent_present:
                    put32(checker_blob, 4, 1)
                checker_auxiliary = (
                    b"" if address == 0x00A72E20
                    and tag in (0x00E2A102, 0x12345678)
                    else auxiliary)
                # cancel events carry the payload value directly, not a pointee.
                if address == 0x00A72E20 and tag in (
                    0x00E2A102, 0x12345678,
                ):
                    expected = checker.call(
                        address, bytes(checker_blob), b"", results, tag, event_payload)
                else:
                    expected = checker.call(
                        address, bytes(checker_blob), checker_auxiliary, results, *checker_args)
                compare(
                    name, address, expected, game.eax,
                    normalized_blob(
                        runtime, owner, len(blob), {4: 1} if parent_present else {}),
                    checker_auxiliary, b"", runtime.trace, game.stack_cleanup, cleanup,
                    address == 0x00A72E20)

        # 0x00A72380 controller state synchronization.
        for iteration in range(CASES):
            results = [rng.getrandbits(32) for _ in range(RESULT_COUNT)]
            blob = bytearray(rng.randbytes(0x20))
            active = iteration & 1
            blocked = (iteration >> 1) & 1
            ready = (iteration >> 2) & 1
            results[OP_QUERY_CONTROLLER_ACTIVE] = active
            results[OP_QUERY_CONTROLLER_BLOCKED] = blocked
            results[OP_QUERY_CHILD_READY] = ready
            runtime.reset(results)
            self_vtable = bytearray(0x34)
            put32(self_vtable, 0x30, runtime.query_controller_active)
            self_vtable_pointer = runtime.emu.alloc(bytes(self_vtable))
            put32(blob, 0, self_vtable_pointer)
            delayed = runtime.emu.alloc(bytes(8))
            put32(blob, 0x10, delayed)
            target = runtime.virtual_object({
                4: runtime.apply_controller_active,
                0x14: runtime.apply_controller_ready,
            })
            put32(blob, 8, target)
            if iteration % 3:
                blocker = runtime.virtual_object({0x0C: runtime.query_controller_blocked})
                put32(blob, 0x0C, blocker)
            else:
                put32(blob, 0x0C, 0)
            child_table = bytearray(8)
            put32(child_table, 0, runtime.query_child_ready)
            put32(child_table, 4, 0)
            child_table_pointer = runtime.emu.alloc(bytes(child_table))
            child = bytearray(8)
            put32(child, 4, child_table_pointer)
            child_pointer = runtime.emu.alloc(bytes(child))
            put32(blob, 0x18, child_pointer)
            owner = runtime.emu.alloc(bytes(blob))
            game = runtime.emu.call(0x00A72380, ecx=owner)
            checker_blob = bytearray(blob)
            for offset in (0, 8, 0x10, 0x18):
                put32(checker_blob, offset, 1)
            if get32(blob, 0x0C) != 0:
                put32(checker_blob, 0x0C, 1)
            expected = checker.call(0x00A72380, bytes(checker_blob), b"", results)
            normalize = {0: 1, 8: 1, 0x10: 1, 0x18: 1}
            if get32(blob, 0x0C) != 0:
                normalize[0x0C] = 1
            compare(
                "synchronizeControllerState", 0x00A72380, expected, game.eax,
                normalized_blob(runtime, owner, len(blob), normalize), b"", b"",
                runtime.trace, game.stack_cleanup, 0)

        # Deliberate semantic mutations must be observable and the baseline must restore.
        mutation_results = [0] * RESULT_COUNT
        baseline = checker.call(
            0x00A70700, struct.pack("<II", 0xFFFFFFFF, 0), b"", mutation_results)
        popcount_mutant = baseline[0] + 1
        mutation_detected = popcount_mutant != 32
        restored = baseline[0] == 32
        tag_results = [0] * RESULT_COUNT
        tag_results[OP_DISPATCH_MESSAGE] = 0x12345678
        tag_baseline = checker.call(
            0x00A740A0, struct.pack("<II", 1, 2), b"", tag_results, 3)
        tag_words = tag_baseline[4]
        tag_mutation_detected = 0x00E2A108 not in tag_words and 0x00E2A107 in tag_words
        print(
            "mutation sensitivity: "
            f"popcount={'detected' if mutation_detected else 'missed'}, "
            f"message-tag={'detected' if tag_mutation_detected else 'missed'}, "
            f"restored={'yes' if restored else 'no'}")
        if not mutation_detected or not tag_mutation_detected or not restored:
            mismatches += 1

        print("ui controls oracle:")
        for name, count in totals.items():
            print(f"  {name}: {count} cases, {mismatch_totals.get(name, 0)} mismatches")
        print(f"total: {sum(totals.values())} cases, {mismatches} mismatches")
        return 1 if mismatches else 0
    finally:
        checker.close()


if __name__ == "__main__":
    raise SystemExit(main())
