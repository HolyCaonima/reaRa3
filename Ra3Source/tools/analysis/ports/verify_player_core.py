"""Differential oracle for clean Player/PlayerList/ScoreKeeper leaf functions."""

from __future__ import annotations

import argparse
import random
import struct
import subprocess
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from emulate import Emulator  # noqa: E402


def u32(value: int) -> int:
    return value & 0xFFFFFFFF


def call_checker(process: subprocess.Popen[str], op: str, blob: bytes, *args: int):
    line = " ".join([op, blob.hex(), *(f"{u32(value):x}" for value in args)])
    assert process.stdin is not None and process.stdout is not None
    process.stdin.write(line + "\n")
    process.stdin.flush()
    result, output = process.stdout.readline().split()
    if result == "error":
        raise RuntimeError(f"playercheck rejected {op}")
    return int(result, 16), bytes.fromhex(output)


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--playercheck", type=Path,
                        default=Path("build/win32/bin/Debug/ra3_playercheck.exe"))
    parser.add_argument("--seed", type=int, default=0x7E2026)
    args = parser.parse_args()
    rng = random.Random(args.seed)
    emu = Emulator()
    process = subprocess.Popen([str(args.playercheck.resolve())], stdin=subprocess.PIPE,
                               stdout=subprocess.PIPE, text=True, encoding="ascii")

    total = bad = 0

    # op, VA, object size, iteration count, stack-arg count, expected cleanup.
    specs = [
        ("adjust125c", 0x007E1530, 0x1260, 500, 1, 4),
        ("set1258", 0x007E15D0, 0x1260, 50, 1, 4),
        ("setlistscore", 0x007E1840, 0x1260, 50, 1, 4),
        ("setf8", 0x007E1930, 0x200, 50, 1, 4),
        ("slot19e0", 0x007E19E0, 0x1240, 500, 1, 4),
        ("pointer128", 0x007E1B50, 0x200, 50, 1, 4),
        ("value1c8", 0x007E1B70, 0x200, 50, 0, 0),
        ("observer", 0x007E1B80, 0x1260, 50, 0, 0),
        ("dead", 0x007E1B90, 0x1260, 50, 0, 0),
        ("active", 0x007E1BA0, 0x1260, 500, 0, 0),
        ("score04", 0x007E25D0, 0x148, 500, 1, 4),
        ("score0c", 0x007E25E0, 0x148, 500, 1, 4),
        ("addd0", 0x007E2600, 0x148, 500, 1, 4),
        ("addf11c", 0x007E2620, 0x148, 500, 1, 4),
        ("inc100", 0x007E2650, 0x148, 500, 0, 0),
        ("ince8", 0x007E2670, 0x148, 500, 0, 0),
        ("incec", 0x007E2690, 0x148, 500, 0, 0),
        ("incf0", 0x007E26B0, 0x148, 500, 0, 0),
        ("incd8", 0x007E26D0, 0x148, 500, 0, 0),
        ("adddc", 0x007E26F0, 0x148, 500, 1, 4),
        ("stoptimer", 0x007E2710, 0x148, 500, 0, 0),
        ("elapsed", 0x007E2740, 0x148, 500, 0, 0),
        ("timed", 0x007E2760, 0x148, 500, 0, 0),
        ("sum68", 0x007E27A0, 0x148, 500, 0, 0),
        ("sum10", 0x007E27D0, 0x148, 500, 0, 0),
        ("flag134", 0x007E2800, 0x148, 50, 1, 4),
        ("flag135", 0x007E2810, 0x148, 50, 1, 4),
        ("mark134", 0x007E2820, 0x148, 50, 0, 0),
        ("addmoney", 0x007E2830, 0x148, 500, 1, 4),
        ("capacity", 0x007E1480, 0x20, 500, 1, 4),
        ("delay", 0x007E21E0, 0x40, 500, 1, 4),
        ("flagd7", 0x007E2CA0, 0xE0, 500, 1, 4),
        ("add290", 0x007E2D30, 0x2A0, 500, 1, 4),
        ("sub290", 0x007E2D40, 0x2A0, 500, 1, 4),
    ]
    result_ops = {
        "slot19e0", "pointer128", "value1c8", "observer", "dead", "active",
        "elapsed", "timed", "sum68", "sum10", "capacity",
    }

    for op, address, size, iterations, argc, cleanup in specs:
        local_bad = 0
        for iteration in range(iterations):
            emu.reset_data()
            global_data = emu.alloc(bytes(0x300), alignment=16)
            emu.write(0x00DEBA64, struct.pack("<I", global_data))
            blob = bytearray(rng.randbytes(size))
            values = [rng.getrandbits(32) for _ in range(argc)]
            if op in {"setlistscore", "flag134", "flag135", "flagd7"}:
                values = [rng.randrange(2)]
            enabled = rng.randrange(2)
            frame = rng.randrange(1, 2**31)
            fps = rng.randrange(1, 121)
            emu.write(global_data + 0xAB, bytes([enabled]))
            emu.write(global_data + 0x58, struct.pack("<I", frame))
            emu.write(0x00CB0910, struct.pack("<I", fps))

            checker_args = list(values)
            if op in {"addd0", "addf11c", "adddc", "addmoney"}:
                checker_args.append(enabled)
            elif op == "stoptimer":
                checker_args = [frame, enabled]
            elif op in {"inc100", "ince8", "incec", "incf0", "incd8"}:
                checker_args = [enabled]
            elif op in {"elapsed", "timed"}:
                checker_args = [frame, fps]

            obj = emu.alloc(bytes(blob), alignment=16)
            call = emu.call(address, values, ecx=obj)
            game_blob = emu.read(obj, size)
            ours_result, ours_blob = call_checker(process, op, bytes(blob), *checker_args)
            game_result = call.eax
            if op == "slot19e0":
                game_result = 0 if call.eax == 0 else call.eax - obj
            if (game_blob != ours_blob or (op in result_ops and game_result != ours_result)
                    or call.stack_cleanup != cleanup):
                local_bad += 1
                if local_bad <= 2:
                    print(f"{op} mismatch iteration={iteration} eax={game_result:08x}/"
                          f"{ours_result:08x} cleanup={call.stack_cleanup}/{cleanup}")
            total += 1
        bad += local_bad
        print(f"{op}: {iterations} mismatches={local_bad}")

    # cdecl integer predicate has no object.
    for _ in range(500):
        value = rng.getrandbits(32)
        game = emu.call(0x007E2470, [value])
        ours, _ = call_checker(process, "outside2", b"\0", value)
        bad += game.eax != ours or game.stack_cleanup != 0
        total += 1

    process.stdin.close()  # type: ignore[union-attr]
    process.wait(timeout=10)
    print(f"player core: cases={total} mismatches={bad}")
    return 1 if bad else 0


if __name__ == "__main__":
    raise SystemExit(main())
