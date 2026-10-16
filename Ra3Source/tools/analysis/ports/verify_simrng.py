"""Differentially verify RA3 random-state ports against ra3_1.0.game."""

from __future__ import annotations

import argparse
import random
import struct
import subprocess
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from emulate import Emulator  # noqa: E402


STEP = 0x005A9310
INIT = 0x005A93B0
LOGIC_INT = 0x005A94C0
CLIENT_INT = 0x005A94F0
AUDIO_INT = 0x005A9520
LOGIC_REAL = 0x005A9550
CLIENT_REAL = 0x005A95A0
AUDIO_REAL = 0x005A95F0
SEED_CRC = 0x005A9390
ROTATING_CRC = 0x0092A0A0
VARIABLE_SET = 0x005A9640
VARIABLE_COPY_RANGE_FIRST = 0x005A9660
VARIABLE_COPY_TYPE_FIRST = 0x005A9680
CLIENT_VARIABLE = 0x005A96A0
LOGIC_VARIABLE = 0x005A9730
CLIENT_STATE = 0x00CB73F0
AUDIO_STATE = 0x00CB7408
LOGIC_STATE = 0x00CB7420
BASE_SEED = 0x00DF60CC
# Written at startup by 0x00BA7EC0 (1 / (2^32 - 1) stored as a float); zero on disk.
REAL_FACTOR = 0x00DF65B0
REAL_FACTOR_BITS = 0x2F800000
CONTROL_WORDS = (0x007F, 0x027F, 0x037F)
SPECIAL_FLOATS = (
    0x00000000, 0x80000000, 0x3F800000, 0xBF800000, 0x00000001, 0x80000001,
    0x007FFFFF, 0x00800000, 0x7F7FFFFF, 0xFF7FFFFF, 0x7F800000, 0xFF800000,
    0x7FC00000, 0xFFC00000, 0x7F800001, 0x34000000, 0x4B800000, 0xCB800000,
)


def is_nan80(value: bytes) -> bool:
    exponent = int.from_bytes(value[8:10], "little") & 0x7FFF
    fraction = int.from_bytes(value[:8], "little") & ((1 << 63) - 1)
    return exponent == 0x7FFF and fraction != 0


def within_normal_range(bits: int) -> bool:
    exponent = (bits >> 23) & 0xFF
    return (bits & 0x7FFFFFFF) == 0 or 127 - 60 <= exponent <= 127 + 60


def float_bits(value: float) -> int:
    return struct.unpack("<I", struct.pack("<f", value))[0]


def portable_comparable(low: int, high: int) -> bool:
    """Inputs for which no x87 intermediate leaves single precision's normal range."""
    if not (within_normal_range(low) and within_normal_range(high)):
        return False
    as_float = lambda bits: struct.unpack("<f", struct.pack("<I", bits))[0]
    return within_normal_range(float_bits(as_float(high) - as_float(low)))


def random_float(rng: random.Random) -> int:
    choice = rng.random()
    if choice < 0.15:
        return rng.choice(SPECIAL_FLOATS)
    if choice < 0.6:
        return struct.unpack("<I", struct.pack("<f", rng.uniform(-1000.0, 1000.0)))[0]
    return rng.randrange(0x100000000)


def random_distribution(rng: random.Random) -> int:
    choice = rng.random()
    if choice < 0.35:
        return 0
    if choice < 0.7:
        return 1
    if choice < 0.9:
        return 5
    return rng.choice((2, 3, 4, 6, 0xFFFFFFFF, 0x80000001, rng.randrange(0x100000000)))


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
            raise RuntimeError(f"portcheck rejected {command!r}")
        return result

    def close(self) -> None:
        assert self.process.stdin is not None
        self.process.stdin.write("quit\n")
        self.process.stdin.flush()
        if self.process.wait(timeout=10) != 0:
            raise RuntimeError("portcheck failed")


def cpp_transition(cpp: CppOracle, operation: str, state: bytes, *args: int):
    result = cpp.call(" ".join([operation, state.hex(), *(str(value) for value in args)]))
    value, encoded = result.split()
    return int(value), bytes.fromhex(encoded)


def verify(executable: Path) -> None:
    rng = random.Random(0x53494D52)
    emu = Emulator()
    cpp = CppOracle(executable)

    step_cases = 0
    state_address = emu.alloc(bytes(24))
    for seed in (0, 1, 0x80000000, 0xFFFFFFFF):
        initial = bytes.fromhex(cpp.call(f"rngseed {seed}"))
        emu.write(state_address, initial)
        state = initial
        for _ in range(100_000):
            game = emu.call(STEP, ecx=state_address)
            if game.stack_cleanup != 0:
                raise AssertionError(f"step stack cleanup {game.stack_cleanup}")
            ours_value, state = cpp_transition(cpp, "rngstep", state)
            game_state = emu.read(state_address, 24)
            if game.eax != ours_value or game_state != state:
                raise AssertionError(f"step mismatch after seed 0x{seed:08X}")
            step_cases += 1

    seed_cases = 0
    for index in range(10_000):
        seed = rng.randrange(0x100000000)
        emu.reset_data()
        game = emu.call(INIT, [seed])
        if game.stack_cleanup != 0:
            raise AssertionError(f"init stack cleanup {game.stack_cleanup}")
        ours = bytes.fromhex(cpp.call(f"rngseed {seed}"))
        if any(emu.read(address, 24) != ours
               for address in (CLIENT_STATE, AUDIO_STATE, LOGIC_STATE)):
            raise AssertionError(f"seeded state mismatch for 0x{seed:08X}")
        if struct.unpack("<I", emu.read(BASE_SEED, 4))[0] != seed:
            raise AssertionError(f"base seed mismatch for 0x{seed:08X}")
        seed_cases += 1

    edges = [
        (-0x80000000, 0x7FFFFFFF), (0x7FFFFFFF, -0x80000000),
        (-1, -1), (0, 0), (1, 0), (-100, 100),
    ]
    range_totals: dict[str, int] = {}
    for name, address, global_address, operation in (
        ("logic", LOGIC_INT, LOGIC_STATE, "rnglogic"),
        ("client", CLIENT_INT, CLIENT_STATE, "rngclient"),
        ("audio", AUDIO_INT, AUDIO_STATE, "rngclient"),
    ):
        total = 0
        for index in range(10_000 + len(edges)):
            low, high = (edges[index - 10_000] if index >= 10_000 else
                         (rng.randrange(-0x80000000, 0x80000000),
                          rng.randrange(-0x80000000, 0x80000000)))
            state = bytes(rng.randrange(256) for _ in range(24))
            emu.reset_data()
            emu.write(global_address, state)
            game = emu.call(address, [low & 0xFFFFFFFF, high & 0xFFFFFFFF])
            if game.stack_cleanup != 0:
                raise AssertionError(f"{name} stack cleanup {game.stack_cleanup}")
            ours_value, ours_state = cpp_transition(cpp, operation, state, low, high)
            if game.eax != ours_value or emu.read(global_address, 24) != ours_state:
                raise AssertionError(f"{name} mismatch low={low} high={high}")
            total += 1
        range_totals[name] = total

    real_totals: dict[str, int] = {}
    nan_results = 0
    portable_cases = 0
    for name, address, global_address in (
        ("logic", LOGIC_REAL, LOGIC_STATE),
        ("client", CLIENT_REAL, CLIENT_STATE),
        ("audio", AUDIO_REAL, AUDIO_STATE),
    ):
        total = 0
        for control_word in CONTROL_WORDS:
            if cpp.call(f"fpcw {control_word}") != "ok":
                raise RuntimeError("portcheck could not set the control word")
            for index in range(2_000):
                low, high = random_float(rng), random_float(rng)
                if index % 10 == 0:
                    high = low
                state = bytes(rng.randrange(256) for _ in range(24))
                emu.reset_data()
                emu.write(REAL_FACTOR, struct.pack("<I", REAL_FACTOR_BITS))
                emu.write(global_address, state)
                game, game_value = emu.call_float80(
                    address, [low, high], fpu_control_word=control_word)
                if game.stack_cleanup != 0:
                    raise AssertionError(f"{name} real stack cleanup {game.stack_cleanup}")
                game_state = emu.read(global_address, 24)
                extended, single, ours_state = cpp.call(
                    f"rngrealx87 {state.hex()} {low} {high}").split()
                # Unicorn's softfloat picks a different NaN than hardware when
                # both x87 operands are NaN, so NaN payloads are left to the
                # live shadow run.
                both_nan = is_nan80(game_value) and is_nan80(bytes.fromhex(extended))
                if ((bytes.fromhex(extended) != game_value and not both_nan)
                        or bytes.fromhex(ours_state) != game_state):
                    raise AssertionError(
                        f"{name} real mismatch cw=0x{control_word:04X} low=0x{low:08X} "
                        f"high=0x{high:08X}: game {game_value.hex()} ours {extended}")
                if both_nan:
                    nan_results += 1
                elif control_word == 0x007F and portable_comparable(low, high):
                    portable, portable_state = cpp_transition(cpp, "rngreal", state, low, high)
                    if portable != int(single) or portable_state != game_state:
                        raise AssertionError(
                            f"{name} portable real mismatch low=0x{low:08X} high=0x{high:08X}")
                    portable_cases += 1
                total += 1
        real_totals[name] = total

    crc_cases = 0
    buffer_address = emu.alloc(bytes(512))
    for index in range(2_000):
        length = rng.choice((0, 1, 2, 3, 4, 24, 476, rng.randrange(64)))
        data = bytes(rng.randrange(256) for _ in range(length))
        crc = rng.choice((0, 0xFFFFFFFF, 0x80000000, rng.randrange(0x100000000)))
        null_buffer = index % 50 == 0
        emu.write(buffer_address, data)
        game = emu.call(ROTATING_CRC, [0 if null_buffer else buffer_address, length, crc])
        if game.stack_cleanup != 0:
            raise AssertionError(f"rotating CRC stack cleanup {game.stack_cleanup}")
        encoded = "-" if null_buffer else data.hex() or "."
        ours = int(cpp.call(f"rotcrc {encoded} {length} {crc}"))
        if game.eax != ours:
            raise AssertionError(f"rotating CRC mismatch length={length} crc=0x{crc:08X}")
        crc_cases += 1

    seed_crc_cases = 0
    for _ in range(1_000):
        state = bytes(rng.randrange(256) for _ in range(24))
        emu.reset_data()
        emu.write(LOGIC_STATE, state)
        game = emu.call(SEED_CRC)
        if game.stack_cleanup != 0 or emu.read(LOGIC_STATE, 24) != state:
            raise AssertionError("seed CRC changed the stack or the logic state")
        if game.eax != int(cpp.call(f"rngseedcrc {state.hex()}")):
            raise AssertionError(f"seed CRC mismatch for {state.hex()}")
        seed_crc_cases += 1

    signaling = (0x7F800001, 0xFF800001, 0x7FBFFFFF, 0xFFA00000)
    variable_float = lambda: (rng.choice(signaling) if rng.random() < 0.25
                              else random_float(rng))
    variable_address = emu.alloc(bytes(12))
    source_address = emu.alloc(bytes(12))
    set_cases = 0
    for _ in range(2_000):
        low, high = variable_float(), variable_float()
        distribution = random_distribution(rng)
        emu.write(variable_address, bytes(rng.randrange(256) for _ in range(12)))
        game = emu.call(VARIABLE_SET, [low, high, distribution], ecx=variable_address)
        if game.stack_cleanup != 12:
            raise AssertionError(f"variable set stack cleanup {game.stack_cleanup}")
        ours = bytes.fromhex(cpp.call(f"rngvarset {low} {high} {distribution}"))
        if emu.read(variable_address, 12) != ours:
            raise AssertionError(f"variable set mismatch {low:08X} {high:08X} {distribution}")
        set_cases += 1

    copy_totals: dict[str, int] = {}
    quieted = 0
    emulator_snan_gaps = 0
    for name, address in (("range-first", VARIABLE_COPY_RANGE_FIRST),
                          ("type-first", VARIABLE_COPY_TYPE_FIRST)):
        total = 0
        for _ in range(2_000):
            source = struct.pack("<III", random_distribution(rng), variable_float(), variable_float())
            emu.write(source_address, source)
            emu.write(variable_address, bytes(rng.randrange(256) for _ in range(12)))
            game = emu.call(address, [source_address], ecx=variable_address)
            if game.stack_cleanup != 4 or game.eax != variable_address:
                raise AssertionError(f"{name} copy ABI: cleanup {game.stack_cleanup} "
                                     f"eax 0x{game.eax:08X}")
            ours = bytes.fromhex(cpp.call(f"rngvarcopy {source.hex()}"))
            game_bytes = emu.read(variable_address, 12)
            # Unicorn's fld m32 does not quiet a signaling NaN; the host FPU
            # does, so those fields take the native fld/fstp round trip.
            fields = list(struct.unpack("<III", game_bytes))
            for slot in (1, 2):
                native = int(cpp.call(f"x87roundtrip {fields[slot]}"))
                if native != fields[slot]:
                    emulator_snan_gaps += 1
                    fields[slot] = native
            game_bytes = struct.pack("<III", *fields)
            if game_bytes != ours:
                raise AssertionError(f"{name} copy mismatch {source.hex()}: "
                                     f"game {game_bytes.hex()} ours {ours.hex()}")
            quieted += game_bytes != source
            total += 1
        copy_totals[name] = total

    variable_totals: dict[str, int] = {}
    variable_nan = 0
    variable_portable = 0
    for name, address, global_address in (("client", CLIENT_VARIABLE, CLIENT_STATE),
                                          ("logic", LOGIC_VARIABLE, LOGIC_STATE)):
        total = 0
        for control_word in CONTROL_WORDS:
            if cpp.call(f"fpcw {control_word}") != "ok":
                raise RuntimeError("portcheck could not set the control word")
            for index in range(2_000):
                low, high = random_float(rng), random_float(rng)
                if index % 8 == 0:
                    high = low
                variable = struct.pack("<III", random_distribution(rng), low, high)
                state = bytes(rng.randrange(256) for _ in range(24))
                emu.reset_data()
                emu.write(REAL_FACTOR, struct.pack("<I", REAL_FACTOR_BITS))
                emu.write(global_address, state)
                emu.write(variable_address, variable)
                game, game_value = emu.call_float80(
                    address, ecx=variable_address, fpu_control_word=control_word)
                if game.stack_cleanup != 0:
                    raise AssertionError(f"{name} variable stack cleanup {game.stack_cleanup}")
                game_state = emu.read(global_address, 24)
                extended, single, ours_state = cpp.call(
                    f"rngvarx87 {variable.hex()} {state.hex()} {name}").split()
                both_nan = is_nan80(game_value) and is_nan80(bytes.fromhex(extended))
                if ((bytes.fromhex(extended) != game_value and not both_nan)
                        or bytes.fromhex(ours_state) != game_state):
                    raise AssertionError(
                        f"{name} variable mismatch cw=0x{control_word:04X} {variable.hex()}: "
                        f"game {game_value.hex()} ours {extended}")
                if both_nan:
                    variable_nan += 1
                elif control_word == 0x007F and portable_comparable(low, high):
                    portable, portable_state = cpp.call(
                        f"rngvar {variable.hex()} {state.hex()} {name}").split()
                    if int(portable) != int(single) or bytes.fromhex(portable_state) != game_state:
                        raise AssertionError(
                            f"{name} portable variable mismatch {variable.hex()}: "
                            f"x87 single {int(single):08X} portable {int(portable):08X}")
                    variable_portable += 1
                total += 1
        variable_totals[name] = total

    cpp.close()
    print(f"step: {step_cases}/{step_cases} matched, 4 consecutive seeded streams")
    print(f"seed-all: {seed_cases}/{seed_cases} matched")
    for name, total in range_totals.items():
        print(f"{name}-integer: {total}/{total} matched")
    for name, total in real_totals.items():
        print(f"{name}-real: {total}/{total} matched (80-bit ST0 and state, "
              f"CW {'/'.join(f'{cw:04X}' for cw in CONTROL_WORDS)})")
    print(f"real NaN results (payload not compared): {nan_results}; portable float at "
          f"CW 007F: {portable_cases}/{portable_cases} matched")
    print(f"rotating-crc: {crc_cases}/{crc_cases} matched; seed-crc: "
          f"{seed_crc_cases}/{seed_crc_cases} matched")
    print(f"variable-set: {set_cases}/{set_cases} matched")
    for name, total in copy_totals.items():
        print(f"variable-copy-{name}: {total}/{total} matched")
    print(f"copies that quieted a signaling NaN: {quieted} (fields Unicorn left "
          f"signaling, taken from the native x87 round trip: {emulator_snan_gaps})")
    for name, total in variable_totals.items():
        print(f"{name}-variable: {total}/{total} matched (80-bit ST0 and state, "
              f"CW {'/'.join(f'{cw:04X}' for cw in CONTROL_WORDS)})")
    print(f"variable NaN results (payload not compared): {variable_nan}; portable float at "
          f"CW 007F: {variable_portable}/{variable_portable} matched")


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument(
        "--portcheck", type=Path,
        default=Path(__file__).resolve().parents[3]
        / "build/win32/bin/Debug/ra3_portcheck.exe")
    args = parser.parse_args()
    verify(args.portcheck.resolve())


if __name__ == "__main__":
    main()
