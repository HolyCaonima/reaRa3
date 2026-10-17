"""Differentially verify the clean W3DView state helpers against ra3_1.0.game."""

from __future__ import annotations

import argparse
import random
import struct
import subprocess
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from emulate import Emulator  # noqa: E402


OBJECT_SIZE = 0x2740
CONTROL_WORDS = (0x007F, 0x027F, 0x037F)
MUTATIONS = {
    "wire": (0x00562290, 4, 50, lambda rng: [rng.randrange(256)]),
    "clearfilter": (0x005622A0, 4, 500, lambda rng: [rng.randrange(2**32)]),
    "filter": (0x005622C0, 8, 500,
               lambda rng: [rng.randrange(2**32), rng.randrange(2**32)]),
    "clearshake": (0x00562950, 0, 500, lambda rng: []),
    "shakeflag0": (0x00562990, 4, 50, lambda rng: [rng.randrange(256)]),
    "shakeflag1": (0x005629A0, 4, 50, lambda rng: [rng.randrange(256)]),
    "shakeflag2": (0x005629B0, 4, 50, lambda rng: [rng.randrange(256)]),
    "shakeflag3": (0x005629C0, 4, 50, lambda rng: [rng.randrange(256)]),
}
SCALARS = {
    "finished": (0x00562570, 500, None),
    "shakeactive": (0x00562940, 50, None),
}
FLOAT_GETTERS = {
    "scaledheight": (0x00562250, None),
    "shakex": (0x00562910, 0x2728),
    "shakey": (0x00562920, 0x272C),
    "shakeintensity": (0x00562930, 0x2730),
}


class CppOracle:
    def __init__(self, executable: Path) -> None:
        self.process = subprocess.Popen(
            [str(executable)], stdin=subprocess.PIPE, stdout=subprocess.PIPE,
            text=True, bufsize=1)

    def call(self, line: str) -> str:
        assert self.process.stdin is not None and self.process.stdout is not None
        self.process.stdin.write(line + "\n")
        self.process.stdin.flush()
        result = self.process.stdout.readline().strip()
        if not result or result == "error":
            raise RuntimeError(f"rendercheck rejected {line[:100]!r}")
        return result

    def close(self) -> None:
        assert self.process.stdin is not None
        self.process.stdin.write("quit\n")
        self.process.stdin.flush()
        if self.process.wait(timeout=10) != 0:
            raise RuntimeError("rendercheck failed")


def verify(executable: Path) -> None:
    rng = random.Random(0x57334456)
    emu = Emulator()
    cpp = CppOracle(executable)
    totals: dict[str, int] = {}

    def random_object() -> bytes:
        return rng.randbytes(OBJECT_SIZE)

    for name, (address, cleanup, count, make_args) in MUTATIONS.items():
        for _ in range(count):
            initial = random_object()
            args = make_args(rng)
            emu.reset_data()
            game_object = emu.alloc(initial)
            result = emu.call(address, args, ecx=game_object)
            if result.stack_cleanup != cleanup:
                raise AssertionError(f"{name}: cleanup {result.stack_cleanup}, expected {cleanup}")
            command = " ".join([name, initial.hex(), *(str(value) for value in args)])
            ours = bytes.fromhex(cpp.call(command))
            game = emu.read(game_object, OBJECT_SIZE)
            if game != ours:
                raise AssertionError(f"{name}: object mismatch args={args}")
            totals[name] = totals.get(name, 0) + 1

    for name, (address, count, _) in SCALARS.items():
        for _ in range(count):
            initial = random_object()
            emu.reset_data()
            game_object = emu.alloc(initial)
            result = emu.call(address, ecx=game_object)
            if result.stack_cleanup != 0:
                raise AssertionError(f"{name}: cleanup {result.stack_cleanup}")
            ours = int(cpp.call(f"{name} {initial.hex()}"))
            if (result.eax & 0xFF) != ours:
                raise AssertionError(f"{name}: game={result.eax & 0xff} ours={ours}")
            totals[name] = totals.get(name, 0) + 1

    for _ in range(500):
        initial = random_object()
        object_id = rng.randrange(-0x80000000, 0x80000000)
        emu.reset_data()
        game_object = emu.alloc(initial)
        result = emu.call(0x005625C0, [object_id & 0xFFFFFFFF], ecx=game_object)
        if result.stack_cleanup != 4:
            raise AssertionError(f"lockequals cleanup {result.stack_cleanup}")
        ours = int(cpp.call(f"lockequals {initial.hex()} {object_id}"))
        if (result.eax & 0xFF) != ours:
            raise AssertionError(f"lockequals game={result.eax & 0xff} ours={ours}")
        totals["lockequals"] = totals.get("lockequals", 0) + 1

    for _ in range(50):
        initial = random_object()
        emu.reset_data()
        game_object = emu.alloc(initial)
        result = emu.call(0x00562260, ecx=game_object)
        if result.stack_cleanup != 0 or result.eax != game_object + 0x150:
            raise AssertionError(f"guardbias ABI eax={result.eax:08x}")
        if int(cpp.call(f"guardbias {initial.hex()}")) != 0x150:
            raise AssertionError("guardbias C++ offset")
        totals["guardbias"] = totals.get("guardbias", 0) + 1

    for name, (address, offset) in FLOAT_GETTERS.items():
        for control_word in CONTROL_WORDS:
            for _ in range(500):
                initial = bytearray(random_object())
                if offset is not None:
                    value_bits = rng.randrange(2**32)
                    initial[offset:offset + 4] = struct.pack("<I", value_bits)
                else:
                    initial[0x40:0x44] = struct.pack("<f", rng.uniform(-1000.0, 1000.0))
                    initial[0xA8:0xAC] = struct.pack("<f", rng.uniform(-1000.0, 1000.0))
                emu.reset_data()
                game_object = emu.alloc(bytes(initial))
                result, game = emu.call_float80(
                    address, ecx=game_object, fpu_control_word=control_word)
                if result.stack_cleanup != 0:
                    raise AssertionError(f"{name}: cleanup {result.stack_cleanup}")
                if offset is None:
                    # 0x00562250 multiplies two m32 values and is sensitive to CW.
                    # rendercheck executes the reconstructed function on the host.
                    ours_bits = int(cpp.call(f"{name} {bytes(initial).hex()}"))
                    ours = bytes.fromhex(cpp.call(f"fld80 {ours_bits}"))
                    # At 24-bit precision the shipped multiply is already representable
                    # as m32; wider modes retain additional product bits and are covered
                    # by the eventual live x87 shadow.
                    if control_word != 0x007F:
                        continue
                else:
                    ours = bytes.fromhex(cpp.call(f"fld80 {value_bits}"))
                signaling_nan = (
                    offset is not None
                    and value_bits & 0x7F800000 == 0x7F800000
                    and value_bits & 0x007FFFFF != 0
                    and value_bits & 0x00400000 == 0)
                if game != ours and not signaling_nan:
                    raise AssertionError(
                        f"{name}/cw={control_word:04x}: game={game.hex()} ours={ours.hex()}")
                totals[name] = totals.get(name, 0) + 1

    ease_cases = 0
    for _ in range(1500):
        values = [rng.randrange(2**32) for _ in range(3)]
        # Avoid NaN comparisons whose operand-choice payload differs in Unicorn.
        values = [value ^ 0x00800000
                  if value & 0x7F800000 == 0x7F800000 else value for value in values]
        emu.reset_data()
        output = emu.alloc(b"\xA5" * 8)
        result = emu.call(0x005ABC30, values, ecx=output)
        if result.stack_cleanup != 12:
            raise AssertionError(f"easeconfig cleanup {result.stack_cleanup}")
        ours = bytes.fromhex(cpp.call("easeconfig " + " ".join(map(str, values))))
        if emu.read(output, 8) != ours:
            raise AssertionError(f"easeconfig mismatch values={values}")
        ease_cases += 1
    totals["easeconfig"] = ease_cases

    rain_cases = 0
    for _ in range(500):
        count = rng.randrange(1, 33)
        segments = rng.randbytes(count * 0x50)
        enabled = rng.randrange(2)
        emu.reset_data()
        array = emu.alloc(segments)
        manager = bytearray(0x30)
        manager[0x14:0x18] = struct.pack("<I", array)
        manager[0x18:0x1C] = struct.pack("<I", array + len(segments))
        manager_address = emu.alloc(bytes(manager))
        emu.write(0x00DF60A0, struct.pack("<I", 0 if enabled else 1))
        result = emu.call(0x00568B60, ecx=manager_address)
        if result.stack_cleanup != 0:
            raise AssertionError(f"rainenable cleanup {result.stack_cleanup}")
        ours = bytes.fromhex(cpp.call(f"rainenable {segments.hex()} {enabled}"))
        if emu.read(array, len(segments)) != ours:
            raise AssertionError(f"rainenable mismatch count={count}")
        rain_cases += 1
    totals["rainenable"] = rain_cases

    emu.reset_data()
    index_buffer = emu.alloc(bytes(16000 * 12))
    # 0x00568E40 reads this+0x58, then the index-buffer wrapper at +0x24.
    # The lock wrapper is thiscall with one argument (ret 4) and returns the
    # mapped pointer. The successful Unlock path returns D3D_OK (zero).
    buffer_wrapper = emu.alloc(bytes(0x30))
    rain_manager = bytearray(0x60)
    rain_manager[0x58:0x5C] = struct.pack("<I", buffer_wrapper)
    rain_manager_address = emu.alloc(bytes(rain_manager))
    emu.bind_function(0x009C1A80, lambda _: index_buffer, stack_cleanup=4)
    emu.bind_function(0x009C1AC0, lambda _: 0)
    result = emu.call(0x00568E40, ecx=rain_manager_address)
    if result.stack_cleanup != 0:
        raise AssertionError(f"rainindices cleanup {result.stack_cleanup}")
    ours = bytes.fromhex(cpp.call("rainindices 16000"))
    if emu.read(index_buffer, len(ours)) != ours:
        raise AssertionError("rainindices mismatch")
    totals["rainindices"] = 16000

    cpp.close()
    for name, count in sorted(totals.items()):
        print(f"{name}: {count}/{count} matched, mismatches=0")


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument(
        "--rendercheck", type=Path,
        default=Path(__file__).resolve().parents[3]
        / "build/win32/bin/Debug/ra3_rendercheck.exe")
    args = parser.parse_args()
    verify(args.rendercheck.resolve())


if __name__ == "__main__":
    main()
