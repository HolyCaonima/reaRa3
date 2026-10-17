"""Differential oracle for the RA3 AudioEventRTS/SageAudioManager helper cluster."""

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


CASES = 64
EVENT_SIZE = 0x94
INFO_SIZE = 0x124
MANAGER_SIZE = 0x35C
PLAYER_STATE_SIZE = 0x1FC
RANDOM_SELECTOR = 0x00CAA185
CONTROL_WORDS = (0x007F, 0x027F, 0x037F)


def u32(value: int) -> int:
    return value & 0xFFFFFFFF


def pack_u32(data: bytearray, offset: int, value: int) -> None:
    struct.pack_into("<I", data, offset, u32(value))


def unpack_u32(data: bytes, offset: int = 0) -> int:
    return struct.unpack_from("<I", data, offset)[0]


def random_float_bits(rng: random.Random) -> int:
    edges = (0x00000000, 0x80000000, 0x3F800000, 0xBF800000,
             0x7F800000, 0xFF800000, 0x7FC01234)
    if rng.randrange(4) == 0:
        return rng.choice(edges)
    return unpack_u32(struct.pack("<f", rng.uniform(-10000.0, 10000.0)))


class Checker:
    def __init__(self, executable: Path) -> None:
        self.process = subprocess.Popen(
            [str(executable.resolve())], stdin=subprocess.PIPE, stdout=subprocess.PIPE,
            text=True, encoding="ascii", bufsize=1)

    def call(self, operation: str, blob: bytes, *arguments: int) -> tuple[int, bytes, bytes]:
        command = " ".join(
            [operation, blob.hex() or ".", *(f"{u32(value):x}" for value in arguments)])
        assert self.process.stdin is not None and self.process.stdout is not None
        self.process.stdin.write(command + "\n")
        self.process.stdin.flush()
        output = self.process.stdout.readline().strip()
        if not output or output == "error":
            raise RuntimeError(f"audioeventcheck rejected {command!r}")
        result, encoded_blob, auxiliary = output.split()
        return (
            int(result, 16),
            b"" if encoded_blob == "." else bytes.fromhex(encoded_blob),
            b"" if auxiliary == "." else bytes.fromhex(auxiliary),
        )

    def close(self) -> None:
        assert self.process.stdin is not None
        self.process.stdin.write("quit\n")
        self.process.stdin.flush()
        if self.process.wait(timeout=10) != 0:
            raise RuntimeError("audioeventcheck failed")


class Oracle:
    def __init__(self, checker: Checker, seed: int) -> None:
        self.checker = checker
        self.rng = random.Random(seed)
        self.emu = Emulator()
        self.totals: dict[str, int] = {}
        self.mismatches: dict[str, int] = {}
        self.last_equal: tuple[int, bytes, bytes] | None = None
        self.random_integer = 0
        self.channel_log: list[int] = []
        self.channel_base = 0
        self.count_node = 0
        self.count_present = False
        self.envelope_log: list[int] = []
        self.envelope_base = 0

        # The shipped random-real helper is clean but outside this ownership cluster.
        # This stub selects either endpoint while preserving an x87 ST0 return.
        self.emu.write(
            0x005A95F0,
            b"\x80\x3D" + struct.pack("<I", RANDOM_SELECTOR)
            + b"\x00\x75\x05\xD9\x44\x24\x04\xC3\xD9\x44\x24\x08\xC3")
        self.emu.bind_function(0x005A9520, lambda _machine: self.random_integer)

        def generated(machine: Emulator) -> int:
            event = machine.register(r.UC_X86_REG_ECX)
            count = unpack_u32(machine.read(event + 0x84, 4))
            handle = unpack_u32(machine.read(event + 0x88, 4))
            machine.write(event + 0x84, struct.pack("<I", u32(count + 7)))
            machine.write(event + 0x88, struct.pack("<I", handle ^ 0xA5A5A5A5))
            machine.write(event + 0x7C, struct.pack("<I", 3))
            return 0

        self.emu.bind_function(0x0055A7A0, generated)

        def channel(machine: Emulator) -> int:
            state = machine.register(r.UC_X86_REG_ECX)
            self.channel_log.extend((
                u32(state - self.channel_base),
                machine.stack_arg(0),
                machine.stack_arg(1),
            ))
            return 0

        self.emu.bind_function(0x00504C80, channel, stack_cleanup=8)

        def count_find(machine: Emulator) -> int:
            output = machine.stack_arg(0)
            owner = machine.register(r.UC_X86_REG_ECX)
            machine.write(
                output,
                struct.pack("<I", self.count_node if self.count_present else owner + 4))
            return output

        self.emu.bind_function(0x006C3A10, count_find, stack_cleanup=8)

        def set_envelope(machine: Emulator) -> int:
            state = machine.register(r.UC_X86_REG_ECX)
            self.envelope_log.extend((
                u32(state - self.envelope_base),
                machine.stack_arg(0),
                machine.stack_arg(1),
            ))
            return 0

        self.emu.bind_function(0x00505150, set_envelope, stack_cleanup=8)

    def compare(self, name: str, game: tuple[int, bytes, bytes],
                ours: tuple[int, bytes, bytes]) -> None:
        self.totals[name] = self.totals.get(name, 0) + 1
        if game != ours:
            self.mismatches[name] = self.mismatches.get(name, 0) + 1
            if self.mismatches[name] <= 2:
                difference = next(
                    (index for index, pair in enumerate(zip(game[1], ours[1]))
                     if pair[0] != pair[1]), None)
                print(f"{name} mismatch game={game[0]:08x} ours={ours[0]:08x} "
                      f"byte={difference} aux={game[2].hex()}/{ours[2].hex()}")
        else:
            self.last_equal = game

    def event_blob(self) -> tuple[bytearray, bytearray]:
        event = bytearray(self.rng.randbytes(EVENT_SIZE))
        info = bytearray(self.rng.randbytes(INFO_SIZE))
        pack_u32(event, 0x0C, 0)
        return event, info

    def event_call(self, name: str, address: int, operation: str, event: bytearray,
                   info: bytearray, arguments: list[int], cleanup: int,
                   *, info_present: bool = True, float_result: bool = False,
                   bool_result: bool = False, integer_result: bool = False,
                   checker_arguments: list[int] | None = None,
                   fpu_control_word: int = 0x037F) -> None:
        self.emu.reset_data()
        if checker_arguments is not None and name.startswith(("randomize", "regenerate")):
            self.emu.write(RANDOM_SELECTOR, bytes([checker_arguments[0]]))
        info_pointer = self.emu.alloc(bytes(info))
        game_event = bytearray(event)
        pack_u32(game_event, 0x0C, info_pointer if info_present else 0)
        event_pointer = self.emu.alloc(bytes(game_event))
        extended = b""
        extended_cleanup = cleanup
        if float_result:
            call, result = self.emu.call_float32(
                address, arguments, ecx=event_pointer,
                fpu_control_word=fpu_control_word)
            extended_call, extended = self.emu.call_float80(
                address, arguments, ecx=event_pointer,
                fpu_control_word=fpu_control_word)
            extended_cleanup = extended_call.stack_cleanup
        else:
            call = self.emu.call(address, arguments, ecx=event_pointer)
            result = call.eax & (0xFF if bool_result else 0xFFFFFFFF) \
                if bool_result or integer_result else 0
        output = bytearray(self.emu.read(event_pointer, EVENT_SIZE))
        pack_u32(output, 0x0C, 0)
        game = (result, bytes(output) + self.emu.read(info_pointer, INFO_SIZE), extended)
        ours = self.checker.call(
            operation, bytes(event + info), int(info_present),
            *(arguments if checker_arguments is None else checker_arguments))
        if call.stack_cleanup != cleanup or extended_cleanup != cleanup:
            game = (game[0] ^ 0x80000000, game[1], game[2])
        self.compare(name, game, ours)

    def verify_events(self) -> None:
        simple = (
            ("setPlayerIndex", 0x00559C10, "event_setplayer", 4, False, False),
            ("minimumRange", 0x00559C20, "event_minrange", 0, True, False),
            ("decrementDelay", 0x00559C30, "event_decrement", 4, False, False),
            ("setObjectId", 0x00559C90, "event_setobject", 4, False, False),
            ("objectId", 0x00559CB0, "event_object", 0, False, False),
            ("setDrawableId", 0x00559CC0, "event_setdrawable", 4, False, False),
            ("drawableId", 0x00559CE0, "event_drawable", 0, False, False),
            ("setShouldFade", 0x00559CF0, "event_setfade", 4, False, False),
            ("shouldFade", 0x00559D00, "event_fade", 0, False, True),
            ("setMinimumRangeOverride", 0x00559D10, "event_min_override", 4, False, False),
            ("setMaximumRangeOverride", 0x00559D20, "event_max_override", 4, False, False),
            ("setVolume", 0x00559D30, "event_volume", 4, False, False),
        )
        for name, address, operation, cleanup, float_result, bool_result in simple:
            for _ in range(CASES):
                event, info = self.event_blob()
                arguments: list[int] = []
                if cleanup:
                    arguments = [self.rng.getrandbits(32)]
                if name == "setShouldFade":
                    arguments = [self.rng.randrange(2)]
                if name in {
                    "decrementDelay", "setMinimumRangeOverride",
                    "setMaximumRangeOverride", "setVolume",
                }:
                    arguments = [random_float_bits(self.rng)]
                if name == "shouldFade":
                    event[0x49] = self.rng.randrange(2)
                control_word = self.rng.choice(CONTROL_WORDS)
                self.event_call(
                    name, address, operation, event, info, arguments, cleanup,
                    float_result=float_result, bool_result=bool_result,
                    integer_result=name in {"objectId", "drawableId"},
                    checker_arguments=[control_word] if float_result else None,
                    fpu_control_word=control_word)

        for _ in range(CASES):
            event, info = self.event_blob()
            owner = self.rng.randrange(4)
            pack_u32(event, 0x38, owner)
            values = [self.rng.getrandbits(32) for _ in range(3)]
            present = self.rng.randrange(4) != 0
            self.emu.reset_data()
            info_pointer = self.emu.alloc(bytes(info))
            vector = self.emu.alloc(struct.pack("<III", *values))
            game_event = bytearray(event)
            pack_u32(game_event, 0x0C, info_pointer)
            event_pointer = self.emu.alloc(bytes(game_event))
            call = self.emu.call(
                0x00559C50, [vector if present else 0], ecx=event_pointer)
            output = bytearray(self.emu.read(event_pointer, EVENT_SIZE))
            pack_u32(output, 0x0C, 0)
            game = (0, bytes(output) + self.emu.read(info_pointer, INFO_SIZE), b"")
            ours = self.checker.call(
                "event_position", bytes(event + info), 1, *values, int(present))
            if call.stack_cleanup != 4:
                game = (0x80000000, game[1], b"")
            self.compare("setPosition", game, ours)

        for _ in range(CASES):
            event, info = self.event_blob()
            for offset in (0x58, 0x5C, 0x70):
                pack_u32(event, offset, self.rng.getrandbits(32))
            pack_u32(info, 0x20, self.rng.getrandbits(32))
            event[0x54] = self.rng.randrange(2)
            control_word = self.rng.choice(CONTROL_WORDS)
            self.event_call(
                "effectivePitch", 0x0055A990, "event_pitch", event, info, [], 0,
                float_result=True, checker_arguments=[control_word],
                fpu_control_word=control_word)

        for _ in range(CASES):
            event, info = self.event_blob()
            event[0x4C] = self.rng.randrange(2)
            pack_u32(event, 0x84, self.rng.choice((0, 1, 2, u32(-12345), u32(-1))))
            pack_u32(info, 0xE8, self.rng.randrange(5))
            info[0x78] = self.rng.randrange(256)
            self.event_call(
                "canContinue", 0x0055A9E0, "event_continue", event, info, [], 0,
                info_present=self.rng.randrange(5) != 0, bool_result=True)

        for _ in range(CASES):
            event, info = self.event_blob()
            pack_u32(event, 0x7C, self.rng.randrange(5))
            pack_u32(event, 0x84, self.rng.choice((0, 1, 2, u32(-12345))))
            pack_u32(event, 0x20, self.rng.randrange(2))
            event[0x4C] = self.rng.randrange(2)
            pack_u32(info, 0xE8, self.rng.randrange(5))
            info[0x78] = self.rng.randrange(256)
            self.event_call(
                "advancePlayPortion", 0x0055B760, "event_advance",
                event, info, [], 0)

        for name, address, operation in (
            ("randomizePitchAndVolume", 0x0055A5D0, "event_randomize"),
            ("randomizeDelay", 0x0055B5E0, "event_delay"),
            ("randomizePostDelay", 0x0055B630, "event_postdelay"),
            ("regenerate", 0x0055B6F0, "event_regenerate"),
        ):
            for _ in range(CASES):
                event, info = self.event_blob()
                maximum = self.rng.randrange(2)
                event[0x50] = self.rng.randrange(2)
                for offset in (0x14, 0x18, 0x28, 0x2C, 0x30, 0x34):
                    pack_u32(info, offset, random_float_bits(self.rng))
                for offset in (0x3C, 0x40, 0x44, 0x48):
                    pack_u32(info, offset, self.rng.randrange(-2000, 2001))
                self.event_call(
                    name, address, operation, event, info, [], 0,
                    checker_arguments=[maximum])

    def info_bundle(self, group: bool) -> bytearray:
        blob = bytearray(self.rng.randbytes(INFO_SIZE * 3 + 16))
        for base in (0, INFO_SIZE, INFO_SIZE * 2):
            for offset in (0x104, 0x108, 0xB8, 0xBC):
                pack_u32(blob, base + offset, 0)
        if group:
            pack_u32(blob, 0xE8, 4)
        return blob

    def info_call(self, name: str, address: int, operation: str, blob: bytearray,
                  arguments: list[int], cleanup: int, *, group: bool,
                  bool_result: bool = False) -> None:
        self.emu.reset_data()
        main = self.emu.alloc(bytes(blob[:INFO_SIZE]))
        child0 = self.emu.alloc(bytes(blob[INFO_SIZE:INFO_SIZE * 2]))
        child1 = self.emu.alloc(bytes(blob[INFO_SIZE * 2:INFO_SIZE * 3]))
        records = bytearray(0x40)
        pack_u32(records, 0, child0)
        pack_u32(records, 0x20, child1)
        record_pointer = self.emu.alloc(bytes(records))
        samples = self.emu.alloc(bytes(blob[INFO_SIZE * 3:]))
        if group:
            self.emu.write(main + 0x104, struct.pack("<II", record_pointer, record_pointer + 0x40))
        self.emu.write(main + 0xB8, struct.pack("<II", samples, samples + 16))
        call = self.emu.call(address, arguments, ecx=main)
        result = call.eax & (0xFF if bool_result else 0xFFFFFFFF)
        game_main = bytearray(self.emu.read(main, INFO_SIZE))
        for offset in (0x104, 0x108, 0xB8, 0xBC):
            pack_u32(game_main, offset, 0)
        game_blob = (
            bytes(game_main) + self.emu.read(child0, INFO_SIZE)
            + self.emu.read(child1, INFO_SIZE) + self.emu.read(samples, 16))
        game = (result, game_blob, b"")
        ours = self.checker.call(operation, bytes(blob), int(group), *arguments)
        if call.stack_cleanup != cleanup:
            game = (result ^ 0x80000000, game_blob, b"")
        self.compare(name, game, ours)

    def verify_info(self) -> None:
        for name, address, operation, cleanup, boolean in (
            ("hasOverrideSlots", 0x00559E40, "info_override", 0, True),
            ("isPlayable", 0x00559E60, "info_playable", 0, True),
            ("hasAnySamples", 0x00559ED0, "info_samples", 0, True),
            ("resolvedPriority", 0x00559F20, "info_priority", 0, False),
            ("matchesCategory", 0x00559FA0, "info_matches", 4, True),
            ("sampleIndex", 0x0055A020, "info_index", 4, False),
        ):
            for iteration in range(CASES):
                group = name in {"isPlayable", "matchesCategory"} and iteration % 2 == 0
                blob = self.info_bundle(group)
                for base in (0, INFO_SIZE, INFO_SIZE * 2):
                    pack_u32(blob, base + 0xE8, 4 if (base == 0 and group)
                             else self.rng.randrange(5))
                    blob[base + 0x78] = self.rng.randrange(256)
                    blob[base + 0x100] = self.rng.randrange(2)
                pack_u32(blob, 0xEC, self.rng.choice((u32(-1), 0, 1, 2, 3, 9)))
                values = [self.rng.getrandbits(32) for _ in range(4)]
                struct.pack_into("<IIII", blob, INFO_SIZE * 3, *values)
                arguments: list[int] = []
                if name == "matchesCategory":
                    arguments = [self.rng.randrange(5)]
                elif name == "sampleIndex":
                    arguments = [self.rng.choice(values + [self.rng.getrandbits(32)])]
                self.info_call(
                    name, address, operation, blob, arguments, cleanup,
                    group=group, bool_result=boolean)

    def manager_call(self, name: str, address: int, operation: str, blob: bytearray,
                     arguments: list[int], cleanup: int, *, bool_result: bool = False,
                     listener: bool = False, pointer_result: bool = False) -> None:
        self.emu.reset_data()
        listener_pointer = self.emu.alloc(bytes(blob[MANAGER_SIZE:]))
        manager_data = bytearray(blob[:MANAGER_SIZE])
        pack_u32(manager_data, 0x350, listener_pointer)
        manager = self.emu.alloc(bytes(manager_data))
        auxiliary = b""
        stack_arguments = list(arguments)
        outputs: list[int] = []
        if listener:
            outputs = [self.emu.alloc(b"\0" * 4) for _ in range(3)]
            stack_arguments = outputs
        call = self.emu.call(address, stack_arguments, ecx=manager)
        if bool_result:
            result = call.eax & 0xFF
        elif pointer_result:
            result = u32(call.eax - manager)
        elif name in {"playerHandle", "allocatePlayingHandle"}:
            result = call.eax
        else:
            result = 0
        output = bytearray(self.emu.read(manager, MANAGER_SIZE))
        pack_u32(output, 0x350, 0)
        if listener:
            auxiliary = b"".join(self.emu.read(pointer, 4) for pointer in outputs)
        game = (
            result, bytes(output) + self.emu.read(listener_pointer, 0x94), auxiliary)
        ours = self.checker.call(operation, bytes(blob), *arguments)
        if call.stack_cleanup != cleanup:
            game = (result ^ 0x80000000, game[1], auxiliary)
        self.compare(name, game, ours)

    def verify_manager(self) -> None:
        specs = (
            ("allocateRequestSerial", 0x00503170, "manager_serial", 0),
            ("isOn", 0x00503180, "manager_ison", 4),
            ("setOn", 0x005031D0, "manager_seton", 8),
            ("listenerPosition", 0x00503220, "manager_listener", 12),
            ("eventInfoRegistry", 0x00503280, "manager_registry", 0),
            ("playerHandle", 0x005032F0, "manager_handle", 4),
            ("allocatePlayingHandle", 0x00506490, "manager_playing", 0),
        )
        for name, address, operation, cleanup in specs:
            for _ in range(CASES):
                blob = bytearray(self.rng.randbytes(MANAGER_SIZE + 0x94))
                pack_u32(blob, 0x350, 0)
                for offset in (MANAGER_SIZE + 0x88, MANAGER_SIZE + 0x8C,
                               MANAGER_SIZE + 0x90):
                    pack_u32(blob, offset, random_float_bits(self.rng))
                for offset in range(0x136, 0x13B):
                    blob[offset] = self.rng.randrange(2)
                arguments: list[int] = []
                if name == "isOn":
                    arguments = [self.rng.randrange(32)]
                elif name == "setOn":
                    arguments = [self.rng.randrange(2), self.rng.randrange(32)]
                elif name == "playerHandle":
                    arguments = [self.rng.randrange(16)]
                self.manager_call(
                    name, address, operation, blob, arguments, cleanup,
                    bool_result=name == "isOn", listener=name == "listenerPosition",
                    pointer_result=name == "eventInfoRegistry")

    def verify_weighted(self, name: str, address: int, operation: str) -> None:
        for _ in range(CASES):
            self.emu.reset_data()
            count = self.rng.randrange(0, 12)
            records = bytearray(self.rng.randbytes(count * 12))
            weights = [self.rng.randrange(0, 20) for _ in range(count)]
            for index, weight in enumerate(weights):
                pack_u32(records, index * 12 + 4, weight)
            limit = self.rng.choice((0, sum(weights), self.rng.randrange(1, 100)))
            self.random_integer = 0 if limit == 0 else self.rng.randrange(limit)
            begin = self.emu.alloc(bytes(records))
            vector = self.emu.alloc(struct.pack("<II", begin, begin + len(records)))
            output = self.emu.alloc(b"\xA5" * 4)
            self.emu.write(0x00DF03FC, b"\0\0\0\0")
            if address == 0x0055A170:
                call = self.emu.call(address, [output], eax=limit, ecx=vector)
                cleanup = 0
            else:
                call = self.emu.call(address, [vector, limit, output])
                cleanup = 12
            game = (call.eax, bytes(records), self.emu.read(output, 4))
            ours = self.checker.call(
                operation, bytes(records), limit, self.random_integer)
            if call.stack_cleanup != cleanup:
                game = (call.eax ^ 0x80000000, game[1], game[2])
            self.compare(name, game, ours)

    def verify_misc(self) -> None:
        self.verify_weighted(
            "selectWeightedIndex", 0x0055A170, "weighted")
        self.verify_weighted(
            "selectCacheWeightedIndex", 0x00504BB0, "cache_weighted")

        for _ in range(CASES):
            blob = self.rng.randbytes(16)
            self.emu.reset_data()
            left = self.emu.alloc(blob[:8])
            right = self.emu.alloc(blob[8:])
            call = self.emu.call(0x0055A060, [right], ecx=left)
            game = (call.eax, blob, b"")
            if call.stack_cleanup != 4:
                game = (call.eax ^ 0x80000000, blob, b"")
            self.compare("audioKeysDiffer", game, self.checker.call("keys", blob))

        for _ in range(CASES):
            blob = bytearray(self.rng.randbytes(16))
            present = self.rng.randrange(3) != 0
            self.emu.reset_data()
            pointer = self.emu.alloc(bytes(blob))
            call = self.emu.call(0x00506410, [pointer if present else 0])
            game = (call.eax & 0xFF, bytes(blob), b"")
            if call.stack_cleanup != 4:
                game = (game[0] ^ 0x80, game[1], game[2])
            self.compare("requestUsesAudioEvent", game,
                         self.checker.call("request", bytes(blob), int(present)))

        for _ in range(CASES):
            blob = bytearray(self.rng.randbytes(0x58))
            base = struct.unpack("<b", bytes([blob[0x54]]))[0]
            relative = self.rng.randrange(-base, 10 - base)
            self.emu.reset_data()
            pointer = self.emu.alloc(bytes(blob))
            call = self.emu.call(0x00504C40, [relative], ecx=pointer)
            game = (call.eax, bytes(blob), b"")
            if call.stack_cleanup != 4:
                game = (call.eax ^ 0x80000000, bytes(blob), b"")
            self.compare("recentChannelUsed", game,
                         self.checker.call("recent", bytes(blob), relative))

        for name, address, operation in (
            ("initializeAudioChannelGrid", 0x00504DC0, "grid"),
            ("resetAudioCategoryForPlayers", 0x00505300, "reset_category"),
        ):
            for _ in range(CASES):
                blob = bytearray(self.rng.randbytes(PLAYER_STATE_SIZE * 6))
                self.channel_log = []
                self.emu.reset_data()
                if operation == "grid":
                    pointer = self.emu.alloc(bytes(blob))
                    self.channel_base = pointer
                    call = self.emu.call(address, ecx=pointer)
                    arguments: list[int] = []
                    game_blob = self.emu.read(pointer, len(blob))
                    cleanup = 0
                else:
                    self.emu.write(0x00DEE290, bytes(blob))
                    self.channel_base = 0x00DEE290
                    category = self.rng.getrandbits(32)
                    call = self.emu.call(address, [category])
                    arguments = [category]
                    game_blob = self.emu.read(0x00DEE290, len(blob))
                    cleanup = 0
                auxiliary = struct.pack(f"<{len(self.channel_log)}I", *self.channel_log)
                game = (0, game_blob, auxiliary)
                if call.stack_cleanup != cleanup:
                    game = (0x80000000, game_blob, auxiliary)
                self.compare(name, game, self.checker.call(operation, bytes(blob), *arguments))

        for name, address, operation, global_address in (
            ("consumeAudioResetFlag", 0x00502FC0, "consume", 0x00DEE1E9),
            ("markAudioSettingsDirty", 0x00502FE0, "dirty", 0x00CAA184),
        ):
            for _ in range(CASES):
                value = self.rng.randrange(2)
                self.emu.reset_data()
                self.emu.write(global_address, bytes([value]))
                call = self.emu.call(address)
                result = call.eax & 0xFF if operation == "consume" else 0
                game = (result, self.emu.read(global_address, 1), b"")
                self.compare(name, game, self.checker.call(operation, bytes([value])))

        for _ in range(CASES):
            blob = self.rng.randbytes(24)
            index = self.rng.randrange(10)
            self.emu.reset_data()
            self.emu.write(0x00CA9F04, blob)
            call, result = self.emu.call_float32(0x005032A0, [index])
            game = (result, blob, b"")
            if call.stack_cleanup != 4:
                game = (result ^ 0x80000000, blob, b"")
            self.compare("indexedVolume", game, self.checker.call("indexed", blob, index))

        for _ in range(CASES):
            blob = self.rng.randbytes(PLAYER_STATE_SIZE * 6)
            index = self.rng.randrange(6)
            self.emu.reset_data()
            self.emu.write(0x00DEE290, blob)
            self.emu.write(0x00CA7D7C, struct.pack("<I", index))
            call, result = self.emu.call_float32(0x005032D0)
            self.compare(
                "playerVolume", (result, blob, b""),
                self.checker.call("player_volume", blob, index))
            if call.stack_cleanup != 0:
                self.mismatches["playerVolume"] = self.mismatches.get("playerVolume", 0) + 1

        for _ in range(CASES):
            blob = self.rng.randbytes(PLAYER_STATE_SIZE * 6)
            values = [random_float_bits(self.rng) for _ in range(5)]
            index = self.rng.randrange(6)
            self.emu.reset_data()
            self.emu.write(0x00DEE290, blob)
            refreshes: list[int] = []

            def refresh(machine: Emulator) -> int:
                refreshes.append(u32(machine.register(r.UC_X86_REG_ECX) - 0x00DEE290))
                return 0

            self.emu.bind_function(0x00504DC0, refresh)
            call = self.emu.call(0x005065C0, [*values, index])
            game_blob = self.emu.read(0x00DEE290, len(blob))
            auxiliary = struct.pack(f"<{len(refreshes)}I", *refreshes)
            game = (0, game_blob, auxiliary)
            if call.stack_cleanup != 24:
                game = (0x80000000, game_blob, auxiliary)
            self.compare(
                "configureEnvelope", game,
                self.checker.call("envelope", blob, *values, index))

        for _ in range(CASES):
            blob = self.rng.randbytes(PLAYER_STATE_SIZE * 6)
            value = random_float_bits(self.rng)
            mode = self.rng.getrandbits(32)
            index = self.rng.randrange(6)
            self.emu.reset_data()
            self.emu.write(0x00DEE290, blob)
            self.envelope_base = 0x00DEE290
            self.envelope_log = []
            call = self.emu.call(0x00506690, [value, mode, index])
            auxiliary = struct.pack("<III", *self.envelope_log)
            game = (0, blob, auxiliary)
            if call.stack_cleanup != 12:
                game = (0x80000000, blob, auxiliary)
            self.compare(
                "setPlayerEnvelope", game,
                self.checker.call("set_envelope", blob, value, mode, index))

        for name, address, operation in (
            ("incrementPlaybackCount", 0x0055B060, "count_inc"),
            ("decrementPlaybackCount", 0x0055B0A0, "count_dec"),
            ("playbackCount", 0x0055B0E0, "count_get"),
            ("playbackLimitReached", 0x0055B110, "count_limit"),
        ):
            for _ in range(CASES):
                initial = self.rng.randrange(-1000, 1001)
                present = self.rng.randrange(4) != 0
                limit = self.rng.randrange(-1000, 1001)
                self.emu.reset_data()
                owner = self.emu.alloc(b"\0" * 16)
                key = bytearray(self.rng.randbytes(8))
                pack_u32(key, 4, limit)
                key_pointer = self.emu.alloc(bytes(key))
                node = bytearray(self.rng.randbytes(0x18))
                pack_u32(node, 0x14, initial)
                self.count_node = self.emu.alloc(bytes(node))
                self.count_present = present
                call = self.emu.call(address, [key_pointer], ecx=owner)
                result = call.eax & (0xFF if operation == "count_limit" else 0xFFFFFFFF)
                output = self.emu.read(self.count_node + 0x14, 4) if present \
                    else struct.pack("<i", initial)
                game = (result, output, b"")
                arguments = [int(present)]
                if operation == "count_limit":
                    arguments.append(limit)
                if call.stack_cleanup != 4:
                    game = (result ^ 0x80000000, output, b"")
                self.compare(name, game, self.checker.call(
                    operation, struct.pack("<i", initial), *arguments))

    def run(self) -> int:
        self.verify_events()
        self.verify_info()
        self.verify_manager()
        self.verify_misc()
        expected = {
            "setPlayerIndex", "minimumRange", "decrementDelay", "setPosition",
            "setObjectId", "objectId", "setDrawableId", "drawableId",
            "setShouldFade", "shouldFade", "setMinimumRangeOverride",
            "setMaximumRangeOverride", "setVolume", "effectivePitch", "canContinue",
            "advancePlayPortion", "randomizePitchAndVolume", "randomizeDelay",
            "randomizePostDelay", "regenerate", "hasOverrideSlots", "isPlayable",
            "hasAnySamples", "resolvedPriority", "matchesCategory", "sampleIndex",
            "allocateRequestSerial", "isOn", "setOn", "listenerPosition",
            "eventInfoRegistry", "playerHandle", "allocatePlayingHandle",
            "selectWeightedIndex", "selectCacheWeightedIndex", "audioKeysDiffer",
            "requestUsesAudioEvent", "recentChannelUsed", "initializeAudioChannelGrid",
            "resetAudioCategoryForPlayers", "consumeAudioResetFlag",
            "markAudioSettingsDirty", "indexedVolume", "playerVolume",
            "configureEnvelope", "setPlayerEnvelope", "incrementPlaybackCount",
            "decrementPlaybackCount", "playbackCount", "playbackLimitReached",
        }
        if set(self.totals) != expected:
            missing = expected - set(self.totals)
            extra = set(self.totals) - expected
            raise RuntimeError(f"coverage mismatch missing={missing} extra={extra}")

        mutation_blob = struct.pack("<IIII", 1, 2, 1, 2)
        self.emu.reset_data()
        left = self.emu.alloc(mutation_blob[:8])
        right = self.emu.alloc(mutation_blob[8:])
        mutation_call = self.emu.call(0x0055A060, [right], ecx=left)
        game = (mutation_call.eax, mutation_blob, b"")
        baseline = self.checker.call("keys", mutation_blob)
        self.checker.call("mutation", b"", 1)
        mutated = self.checker.call("keys", mutation_blob)
        self.checker.call("mutation", b"", 0)
        restored_result = self.checker.call("keys", mutation_blob)
        detected = (
            mutation_call.stack_cleanup == 4 and baseline == game and mutated != game)
        restored = restored_result == game
        print(f"mutation sensitivity: detected={int(detected)} restored={int(restored)}")
        if not detected or not restored:
            raise RuntimeError("mutation sensitivity check failed")

        total = sum(self.totals.values())
        bad = sum(self.mismatches.values())
        for name in sorted(expected):
            print(f"{name}: cases={self.totals[name]} mismatches={self.mismatches.get(name, 0)}")
        print(f"audio event: functions={len(expected)} cases={total} mismatches={bad}")
        return 1 if bad else 0


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument(
        "--audioeventcheck", type=Path,
        default=Path("build/win32/bin/Debug/ra3_audioeventcheck.exe"))
    parser.add_argument("--seed", type=int, default=0xA0D10E7)
    args = parser.parse_args()
    checker = Checker(args.audioeventcheck)
    try:
        return Oracle(checker, args.seed).run()
    finally:
        checker.close()


if __name__ == "__main__":
    raise SystemExit(main())
