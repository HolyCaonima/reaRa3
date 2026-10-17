"""Differentially verify clean RA3 FX particle-module functions."""

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


BOX = 0x00661CF0
BOX_RUNTIME = 0x00661AB0
BOX_DIMENSIONS = 0x00661B00
ALPHA_TEMPLATE = 0x00661160
COLOR_RANGE = 0x006619F0
CYLINDER_VELOCITY = 0x00662030
CYLINDER_RUNTIME = 0x006620A0
CYLINDER = 0x006621A0
LINE_RUNTIME = 0x00662330
LINE = 0x00662460
POINT_TEMPLATE = 0x00662570
POINT_VELOCITY = 0x006625B0
ZERO3 = 0x006626F0
SPLINE_RUNTIME = 0x006627C0
UNIT = 0x00665C40
SHUFFLE = 0x00669190
ONE = 0x0066A850
ZERO4 = 0x0066A860
SCALE_UNIT = 0x0066A8D0
VARIABLE_UNIT = 0x0066A940
SPHERE = 0x0066A9A0
UPDATE_POINTERS = (0x0066A710, 0x0066A720, 0x0066A730)
GPU_ACCESSORS = (
    0x0066AB50, 0x0066AB60, 0x0066AB70, 0x0066AB80,
    0x0066AB90, 0x0066ABA0, 0x0066ABB0, 0x0066ABC0,
)
TYPE_NAMES = (
    0x00661B80, 0x00662020, 0x00662380, 0x00662670, 0x00662810,
    0x0066A700, 0x0066A840, 0x0066AB40, 0x0066CDD0,
    0x006611C0, 0x00661200, 0x00661420, 0x006619E0,
)
CLIENT_STATE = 0x00CB73F0
REAL_FACTOR = 0x00DF65B0
CONTROL_WORDS = (0x007F, 0x027F, 0x037F)


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
            raise RuntimeError(f"fxpscheck rejected {command!r}")
        return result

    def close(self) -> None:
        assert self.process.stdin is not None
        self.process.stdin.write("quit\n")
        self.process.stdin.flush()
        if self.process.wait(timeout=10) != 0:
            raise RuntimeError("fxpscheck failed")


def bits(value: float) -> int:
    return struct.unpack("<I", struct.pack("<f", value))[0]


def finite_float(rng: random.Random, magnitude: float = 1000.0) -> int:
    return bits(rng.uniform(-magnitude, magnitude))


def hardware_fld32(value: int) -> int:
    if ((value & 0x7F800000) == 0x7F800000
            and (value & 0x007FFFFF) != 0):
        return value | 0x00400000
    return value


def state_bytes(rng: random.Random) -> bytes:
    return bytes(rng.randrange(256) for _ in range(24))


def cpp_vector(cpp: CppOracle, command: str) -> tuple[bytes, bytes]:
    vector, state = cpp.call(command).split()
    return bytes.fromhex(vector), bytes.fromhex(state)


def set_state(emu: Emulator, state: bytes) -> None:
    emu.write(CLIENT_STATE, state)
    emu.write(REAL_FACTOR, struct.pack("<I", 0x2F800000))


def verify(executable: Path, cases: int) -> None:
    rng = random.Random(0x46585053)
    emu = Emulator()
    cpp = CppOracle(executable)
    output = emu.alloc(bytes(16))
    data = emu.alloc(bytes(0x100))
    module = emu.alloc(bytes(16))
    values = emu.alloc(bytes(256))
    runtime_output = emu.alloc(bytes(256))
    totals: dict[str, int] = {}

    def compare_vector(name: str, command: str, address: int, args: list[int],
                       ecx: int, cleanup: int, state: bytes) -> None:
        emu.write(output, b"\xA5" * 12)
        set_state(emu, state)
        game = emu.call(address, args, ecx=ecx)
        ours, ours_state = cpp_vector(cpp, command)
        game_output = emu.read(output, 12)
        game_state = emu.read(CLIENT_STATE, 24)
        if game.stack_cleanup != cleanup or game_output != ours or game_state != ours_state:
            raise AssertionError(
                f"{name} mismatch cleanup={game.stack_cleanup}/{cleanup} "
                f"command={command} "
                f"game={game_output.hex()} ours={ours.hex()} "
                f"state={game_state.hex()}/{ours_state.hex()}")
        totals[name] = totals.get(name, 0) + 1

    try:
        for control_word in CONTROL_WORDS:
            if cpp.call(f"fpcw {control_word}") != "ok":
                raise RuntimeError("could not set native FPU control word")
            emu.uc.reg_write(x86_const.UC_X86_REG_FPCW, control_word)

            for _ in range(cases):
                state = state_bytes(rng)
                compare_vector(
                    "unit", f"unit {state.hex()}", UNIT, [output], 0, 0, state)

                hollow = rng.randrange(2)
                dimensions = [finite_float(rng, 50.0) for _ in range(3)]
                blob = bytearray(0x18)
                blob[8] = hollow
                struct.pack_into("<III", blob, 0x0C, *dimensions)
                emu.write(data, bytes(blob))
                emu.write(module, struct.pack("<III", 0, 0, data) + bytes(4))
                compare_vector(
                    "box", " ".join(("box", state.hex(), str(hollow),
                                     *(str(value) for value in dimensions))),
                    BOX, [output, 0, 0, 0, 0], module, 20, state)

                radius = finite_float(rng, 50.0)
                length = finite_float(rng, 50.0)
                offset = [finite_float(rng, 50.0) for _ in range(3)]
                blob = bytearray(0x24)
                blob[8] = hollow
                struct.pack_into("<I", blob, 0x14, length)
                struct.pack_into("<III", blob, 0x18, *offset)
                emu.write(data, bytes(blob))
                emu.write(module, struct.pack("<III", 0, 0, data)
                          + struct.pack("<I", radius))
                compare_vector(
                    "cylinder", " ".join(("cylinder", state.hex(), str(hollow),
                                          str(radius), str(length),
                                          *(str(value) for value in offset))),
                    CYLINDER, [output, 0, 0, 0, 0], module, 20, state)

                points = [finite_float(rng, 50.0) for _ in range(6)]
                blob = bytearray(0x24)
                struct.pack_into("<IIIIII", blob, 0x0C, *points)
                emu.write(data, bytes(blob))
                emu.write(module, struct.pack("<III", 0, 0, data) + bytes(4))
                compare_vector(
                    "line", " ".join(("line", state.hex(),
                                      *(str(value) for value in points))),
                    LINE, [output, 0, 0, 0, 0], module, 20, state)

                sphere_radius = finite_float(rng, 50.0)
                blob = bytearray(0x10)
                blob[8] = hollow
                struct.pack_into("<I", blob, 0x0C, sphere_radius)
                emu.write(data, bytes(blob))
                emu.write(module, struct.pack("<III", 0, 0, data) + bytes(4))
                compare_vector(
                    "sphere", f"sphere {state.hex()} {hollow} {sphere_radius}",
                    SPHERE, [output, 0, 0, 0, 0], module, 20, state)

                scale = finite_float(rng, 50.0)
                compare_vector(
                    "scaleunit", f"scaleunit {state.hex()} {scale}",
                    SCALE_UNIT, [output, 0, scale, 0], 0, 16, state)

                distribution = rng.choice((0, 1, 2, 5))
                low = finite_float(rng, 50.0)
                high = low if distribution == 0 and rng.randrange(2) else finite_float(rng, 50.0)
                blob = bytearray(0x20)
                struct.pack_into("<iII", blob, 8, distribution, low, high)
                emu.write(data, bytes(blob))
                emu.write(module, struct.pack("<III", 0, 0, data) + bytes(4))
                compare_vector(
                    "variableunit",
                    f"variableunit {state.hex()} {distribution} {low} {high}",
                    VARIABLE_UNIT, [output, 0, 0], module, 12, state)

                normal_type = rng.choice((0, 1, 2))
                normal_low = finite_float(rng, 50.0)
                normal_high = (normal_low if normal_type == 0 and rng.randrange(2)
                               else finite_float(rng, 50.0))
                blob = bytearray(0x20)
                struct.pack_into("<iII", blob, 8, distribution, low, high)
                struct.pack_into("<iII", blob, 0x14, normal_type, normal_low, normal_high)
                emu.write(data, bytes(blob))
                emu.write(module, struct.pack("<III", 0, 0, data) + bytes(4))
                compare_vector(
                    "cylvelocity",
                    f"cylvelocity {state.hex()} {distribution} {low} {high} "
                    f"{normal_type} {normal_low} {normal_high}",
                    CYLINDER_VELOCITY, [output, 0, 0], module, 12, state)

                point_blob = bytearray(0x2C)
                for offset in (0x08, 0x14, 0x20):
                    point_type = rng.choice((0, 1, 2, 5))
                    point_low = finite_float(rng, 50.0)
                    point_high = (point_low if point_type == 0 and rng.randrange(2)
                                  else finite_float(rng, 50.0))
                    struct.pack_into(
                        "<iII", point_blob, offset,
                        point_type, point_low, point_high)
                emu.write(data, bytes(point_blob))
                emu.write(module, struct.pack("<III", 0, 0, data) + bytes(4))
                compare_vector(
                    "pointvelocity",
                    f"pointvelocity {state.hex()} {point_blob.hex()}",
                    POINT_VELOCITY, [output, 0, 0], module, 12, state)

                color_values = [finite_float(rng, 20.0) for _ in range(10)]
                emu.write(data, struct.pack("<6I", *color_values[:6]))
                emu.write(values, struct.pack("<3I", *color_values[6:9]))
                game = emu.call(
                    COLOR_RANGE, [color_values[9]], ecx=data, edx=values)
                ours = int(cpp.call(
                    " ".join(("colorrange",
                              *(str(value) for value in color_values)))))
                if game.stack_cleanup != 0 or (game.eax & 0xFF) != ours:
                    raise AssertionError("color range mismatch")
                totals["color-range"] = totals.get("color-range", 0) + 1

        constructor_cases = max(50, cases)
        for _ in range(constructor_cases):
            source = bytes(rng.randrange(256) for _ in range(0x20))
            emu.write(data, source)
            for address, kind in (
                    (CYLINDER_RUNTIME, 5),
                    (LINE_RUNTIME, 1),
                    (SPLINE_RUNTIME, 2)):
                emu.write(runtime_output, bytes(64))
                game = emu.call(address, [data], ecx=runtime_output)
                game_bytes = bytearray(emu.read(runtime_output, 0x24))
                for offset in range(0x0C, 0x24, 4):
                    value = struct.unpack_from("<I", game_bytes, offset)[0]
                    struct.pack_into("<I", game_bytes, offset, hardware_fld32(value))
                ours = bytes.fromhex(
                    cpp.call(f"runtimeinit {kind} {source.hex()}"))
                if (game.stack_cleanup != 4 or game.eax != runtime_output
                        or bytes(game_bytes) != ours):
                    raise AssertionError(
                        f"runtime constructor mismatch at 0x{address:08X}")
                totals[f"runtime-{kind}"] = totals.get(f"runtime-{kind}", 0) + 1

            point_source = bytes(rng.randrange(256) for _ in range(0x28))
            emu.write(data, point_source)
            emu.write(runtime_output, bytes(64))
            game = emu.call(POINT_TEMPLATE, [data], ecx=runtime_output)
            game_bytes = bytearray(emu.read(runtime_output, 0x2C))
            for offset in (0x0C, 0x10, 0x18, 0x1C, 0x24, 0x28):
                value = struct.unpack_from("<I", game_bytes, offset)[0]
                struct.pack_into("<I", game_bytes, offset, hardware_fld32(value))
            ours = bytes.fromhex(cpp.call(f"pointinit {point_source.hex()}"))
            if game.stack_cleanup != 4 or game.eax != runtime_output:
                raise AssertionError("point template ABI mismatch")
            if bytes(game_bytes) != ours:
                raise AssertionError("point template constructor mismatch")
            totals["point-template"] = totals.get("point-template", 0) + 1

            alpha_source = bytes(rng.randrange(256) for _ in range(0x98))
            emu.write(data, alpha_source)
            emu.write(runtime_output, bytes(256))
            game = emu.call(ALPHA_TEMPLATE, [data], ecx=runtime_output)
            game_bytes = bytearray(emu.read(runtime_output, 0x98))
            value = struct.unpack_from("<I", game_bytes, 0x94)[0]
            struct.pack_into("<I", game_bytes, 0x94, hardware_fld32(value))
            ours = bytes.fromhex(cpp.call(f"alphainit {alpha_source.hex()}"))
            if (game.stack_cleanup != 4 or game.eax != runtime_output
                    or bytes(game_bytes) != ours):
                raise AssertionError("alpha template constructor mismatch")
            totals["alpha-template"] = totals.get("alpha-template", 0) + 1

            box_source = bytes(rng.randrange(256) for _ in range(0x24))
            emu.write(data, box_source)
            emu.write(runtime_output, bytes(256))
            game = emu.call(BOX_RUNTIME, [data], ecx=runtime_output)
            game_bytes = bytearray(emu.read(runtime_output, 0x24))
            for offset in (0x04, 0x08, 0x0C, 0x10, 0x18, 0x1C):
                value = struct.unpack_from("<I", game_bytes, offset)[0]
                struct.pack_into("<I", game_bytes, offset, hardware_fld32(value))
            ours = bytes.fromhex(cpp.call(f"boxinit {box_source.hex()}"))
            if (game.stack_cleanup != 4 or game.eax != runtime_output
                    or bytes(game_bytes) != ours):
                raise AssertionError("box runtime constructor mismatch")
            totals["box-runtime"] = totals.get("box-runtime", 0) + 1

            emu.write(data, box_source)
            emu.write(module, struct.pack("<III", 0, 0, data) + bytes(4))
            emu.write(output, bytes(16))
            game = emu.call(BOX_DIMENSIONS, [output], ecx=module)
            game_dimensions = bytearray(emu.read(output, 8))
            for offset in (0, 4):
                value = struct.unpack_from("<I", game_dimensions, offset)[0]
                struct.pack_into(
                    "<I", game_dimensions, offset, hardware_fld32(value))
            ours = bytes.fromhex(cpp.call(f"boxdims {box_source.hex()}"))
            if game.stack_cleanup != 4 or bytes(game_dimensions) != ours:
                raise AssertionError("box dimensions mismatch")
            totals["box-dimensions"] = totals.get("box-dimensions", 0) + 1

        for address, offset in zip(UPDATE_POINTERS, (0x44, 0x50, 0x5C)):
            emu.write(module, struct.pack("<III", 0, 0, data) + bytes(4))
            for _ in range(50):
                game = emu.call(address, ecx=module)
                ours = int(cpp.call(f"updateptr {offset}"))
                if (game.stack_cleanup != 0
                        or game.eax != data + offset or ours != offset):
                    raise AssertionError(f"update pointer mismatch at 0x{address:08X}")
            totals[f"update-pointer-{offset:x}"] = 50

        shuffle_cases = cases * 3
        for _ in range(shuffle_cases):
            state = state_bytes(rng)
            count = rng.randrange(0, 33)
            original = [rng.randrange(0x100000000) for _ in range(count)]
            encoded = struct.pack(f"<{count}I", *original) if count else b""
            emu.write(values, encoded)
            set_state(emu, state)
            game = emu.call(SHUFFLE, [values, values + len(encoded)])
            ours_data, ours_state = cpp.call(
                " ".join(("shuffle", state.hex(), *(str(value) for value in original)))).split()
            ours_bytes = b"" if ours_data == "." else bytes.fromhex(ours_data)
            if (game.stack_cleanup != 0
                    or emu.read(values, len(encoded)) != ours_bytes):
                raise AssertionError("shuffle output mismatch")
            if emu.read(CLIENT_STATE, 24) != bytes.fromhex(ours_state):
                raise AssertionError("shuffle state mismatch")
        totals["shuffle"] = shuffle_cases

        simple_cases = max(50, cases // 5)
        for _ in range(simple_cases):
            emu.write(output, bytes(rng.randrange(256) for _ in range(16)))
            result = emu.call(ZERO3, [output, 0, 0, 0, 0])
            if result.stack_cleanup != 20 or emu.read(output, 12) != bytes(12):
                raise AssertionError("zero3 mismatch")
            emu.write(output, bytes(rng.randrange(256) for _ in range(16)))
            result = emu.call(ZERO4, [output])
            if result.stack_cleanup != 4 or emu.read(output, 16) != bytes(16):
                raise AssertionError("zero4 mismatch")
            result, value = emu.call_float32(ONE)
            if result.stack_cleanup != 0 or value != 0x3F800000:
                raise AssertionError("one mismatch")
        totals.update(zero3=simple_cases, zero4=simple_cases, one=simple_cases)

        gpu_cases = max(100, cases)
        for _ in range(gpu_cases):
            blob = bytes(rng.randrange(256) for _ in range(0x44))
            emu.write(data, blob)
            emu.write(module, struct.pack("<III", 0, 0, data) + bytes(4))
            game_values: list[int] = []
            for index, address in enumerate(GPU_ACCESSORS[:-1]):
                if index == 2:
                    result, value = emu.call_float32(address, ecx=module)
                else:
                    result = emu.call(address, ecx=module)
                    value = result.eax
                if result.stack_cleanup != 0:
                    raise AssertionError(f"gpu accessor cleanup at 0x{address:08X}")
                if address == 0x0066AB70:
                    value = hardware_fld32(value)
                if address == 0x0066ABB0:
                    if value & 0xFFFFFF00 != data & 0xFFFFFF00:
                        raise AssertionError("gpu AL accessor did not preserve EAX upper bytes")
                    value &= 0xFF
                game_values.append(value)
            emu.write(output, bytes(16))
            result = emu.call(GPU_ACCESSORS[-1], [output], ecx=module)
            if result.stack_cleanup != 4:
                raise AssertionError("gpu misc cleanup")
            game_misc = struct.pack(
                "<4I", *(hardware_fld32(value)
                         for value in struct.unpack("<4I", emu.read(output, 16))))
            game_encoded = struct.pack("<7I", *game_values) + game_misc
            ours = bytes.fromhex(cpp.call(f"gpu {blob.hex()}"))
            if game_encoded != ours:
                raise AssertionError(f"gpu mismatch game={game_encoded.hex()} ours={ours.hex()}")
        totals["gpu-accessors"] = gpu_cases * len(GPU_ACCESSORS)

        expected_names = cpp.call("names").split("|")
        for address, expected in zip(TYPE_NAMES, expected_names):
            for _ in range(50):
                result = emu.call(address)
                if result.stack_cleanup != 0:
                    raise AssertionError(f"name cleanup at 0x{address:08X}")
                raw = bytearray()
                pointer = result.eax
                while emu.read(pointer + len(raw), 1) != b"\0":
                    raw.extend(emu.read(pointer + len(raw), 1))
                if raw.decode("ascii") != expected:
                    raise AssertionError(f"name mismatch at 0x{address:08X}")
        totals["type-names"] = len(TYPE_NAMES) * 50
    finally:
        cpp.close()

    total = sum(totals.values())
    print(f"FXPS particle modules: {total} cases, 0 mismatches")
    for name, count in sorted(totals.items()):
        print(f"  {name}: {count}")


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("executable", type=Path)
    parser.add_argument("--cases", type=int, default=500)
    args = parser.parse_args()
    verify(args.executable, args.cases)


if __name__ == "__main__":
    main()
