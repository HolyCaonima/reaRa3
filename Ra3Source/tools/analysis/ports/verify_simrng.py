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
CLIENT_STATE = 0x00CB73F0
AUDIO_STATE = 0x00CB7408
LOGIC_STATE = 0x00CB7420
BASE_SEED = 0x00DF60CC


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

    cpp.close()
    print(f"step: {step_cases}/{step_cases} matched, 4 consecutive seeded streams")
    print(f"seed-all: {seed_cases}/{seed_cases} matched")
    for name, total in range_totals.items():
        print(f"{name}-integer: {total}/{total} matched")


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
