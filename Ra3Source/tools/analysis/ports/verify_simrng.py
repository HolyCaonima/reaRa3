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
