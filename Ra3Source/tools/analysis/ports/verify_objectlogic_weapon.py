"""Offline oracle for the closed ObjectLogic Weapon family.

The native identity check reads only the immutable run35 absolute snapshot.
The checker protocol compares the independently transcribed semantic
observations, including ABI cleanup, register results, explicit writes, float
bits, and full 80-bit x87 encodings.
"""

from __future__ import annotations

import argparse
import hashlib
import math
import struct
import subprocess
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from emulate import Emulator  # noqa: E402


ENTRIES = (
    0x00685CC0, 0x00685D20, 0x00685DA0, 0x00685DC0, 0x00685DD0,
    0x00685E70, 0x00685EA0, 0x00685EF0, 0x00685F70, 0x00685FF0,
    0x00686070, 0x00686110, 0x00686190, 0x00686210, 0x00686290,
    0x00686310, 0x00686380, 0x006863F0, 0x006A2400, 0x006A2430,
    0x006A2440, 0x006A2480, 0x006A24A0, 0x006A24F0, 0x006A2560,
    0x006A25F0, 0x006A2630, 0x006A2840, 0x006A2A70, 0x006A2AB0,
    0x006A2B40, 0x006A2BD0, 0x006A2D30, 0x006A2DC0, 0x006A2EE0,
    0x006A3120, 0x006A82F0, 0x006A8530, 0x006A8580, 0x006A8650,
    0x006A86C0, 0x006A8700, 0x006A8840, 0x006A8870, 0x006A88F0,
    0x006BD260, 0x006BD2C0, 0x006BD310, 0x006ED440, 0x006ED4A0,
)

FLOAT_ENTRIES = {
    0x00685DC0, 0x00685DD0, 0x00685E70, 0x00685EA0, 0x006A2630,
    0x006A2D30, 0x006A2DC0, 0x006A8580,
}

# sha256(concatenation of 16 native bytes at every ENTRIES address), from the
# immutable run35 snapshot named below.
NATIVE_PREFIX_SHA256 = (
    "3ee865788019df328ffccadded6836bf0dc66deeabba16454598ebb38b2f9d31")

WORDS = (100, 90, 20, 1, 7, 3, 0x16, 5)
VALUES = (2.0, 3.0, 4.0, 5.0, 6.0, 7.0, 30.0, 1.0)


class Checker:
    def __init__(self, executable: Path) -> None:
        self.process = subprocess.Popen(
            [str(executable)], stdin=subprocess.PIPE, stdout=subprocess.PIPE,
            text=True, bufsize=1)

    def call(self, address: int, mutate: bool = False) -> str:
        assert self.process.stdin is not None and self.process.stdout is not None
        values = [float_bits(value) for value in VALUES]
        command = " ".join([
            "mutate" if mutate else "probe", f"{address:08x}",
            *(f"{value:08x}" for value in WORDS),
            *(f"{value:08x}" for value in values),
        ])
        self.process.stdin.write(command + "\n")
        self.process.stdin.flush()
        result = self.process.stdout.readline().strip()
        if not result or result == "error":
            raise RuntimeError(f"objectlogiccheck rejected {command!r}")
        return result

    def close(self) -> None:
        assert self.process.stdin is not None
        self.process.stdin.write("quit\n")
        self.process.stdin.flush()
        if self.process.wait(timeout=10) != 0:
            raise RuntimeError("objectlogiccheck failed")


def u32(value: int) -> int:
    return value & 0xFFFFFFFF


def f32(value: float) -> float:
    return struct.unpack("<f", struct.pack("<f", value))[0]


def float_bits(value: float) -> int:
    return struct.unpack("<I", struct.pack("<f", value))[0]


def ext80(bits: int) -> bytes:
    sign = bits >> 31
    exponent = (bits >> 23) & 0xFF
    fraction = bits & 0x7FFFFF
    if exponent == 0 and fraction == 0:
        return struct.pack("<QH", 0, sign << 15)
    if exponent == 0:
        value = struct.unpack("<f", struct.pack("<I", bits))[0]
        return ext80(float_bits(value))
    if exponent == 0xFF:
        significand = 0x8000000000000000 | (fraction << 40)
        return struct.pack("<QH", significand, (sign << 15) | 0x7FFF)
    significand = 0x8000000000000000 | (fraction << 40)
    extended_exponent = exponent - 127 + 16383
    return struct.pack("<QH", significand, (sign << 15) | extended_exponent)


def frames(seconds: float) -> int:
    product = f32(f32(seconds * VALUES[6]) * VALUES[7])
    return u32(math.ceil(product))


def encode(address: int, eax: int = 0, cleanup: int = 0, st0: float | None = None,
           writes: tuple[int, ...] = ()) -> str:
    padded = tuple(u32(value) for value in writes) + (0,) * (4 - len(writes))
    write_mask = (1 << len(writes)) - 1
    st0_bits = 0 if st0 is None else float_bits(st0)
    fields = (u32(eax), 0, cleanup, st0_bits, write_mask, *padded)
    result = "".join(f"{value:08x}" for value in fields)
    result += (ext80(st0_bits) if address in FLOAT_ENTRIES else bytes(10)).hex()
    return result


def reference(address: int) -> str:
    w = WORDS
    v = VALUES
    deadline = u32(w[1] + w[2])
    if address == 0x00685CC0:
        return encode(address, 0xFFFFFFFF)
    if address == 0x00685D20:
        return encode(address, 0)
    if address == 0x00685DA0:
        return encode(address, 0)
    if address == 0x00685DC0:
        return encode(address, cleanup=8, st0=0.0)
    if address in (0x00685DD0, 0x006A2DC0):
        return encode(address, cleanup=4, st0=0.0)
    if address == 0x00685E70:
        return encode(address, st0=0.0)
    if address == 0x00685EA0:
        return encode(address, st0=v[0])
    if address in (0x00685EF0, 0x00685FF0, 0x00686110, 0x00686210,
                   0x00686310, 0x00686380, 0x006863F0):
        return encode(address, frames(v[0]))
    if address in (0x00685F70, 0x00686070, 0x00686190, 0x00686290):
        return encode(address, frames(v[1]))
    if address == 0x006A2400:
        return encode(address, w[0], 4, writes=(min(w[2], 5),))
    if address == 0x006A2430:
        return encode(address, w[w[7] % 6], 4)
    if address == 0x006A2440:
        return encode(address, 1)
    if address == 0x006A2480:
        return encode(address, w[1])
    if address == 0x006A24A0:
        first = 0
        second = 0
        for value in w[:6]:
            first |= value
            second |= u32(value << 1)
        return encode(address, w[5] << 1, writes=(first, second))
    if address == 0x006A24F0:
        return encode(
            address, w[7], cleanup=4, writes=tuple(map(float_bits, v[:3])))
    if address == 0x006A2560:
        count = sum(value & 0xFF for value in w[:6] if value & w[6])
        return encode(address, count, 8)
    if address == 0x006A25F0:
        return encode(address, 0, 8)
    if address == 0x006A2630:
        return encode(address, cleanup=12, st0=v[1])
    if address == 0x006A2840:
        return encode(address, deadline)
    if address == 0x006A2A70:
        return encode(address, w[0], 4)
    if address == 0x006A2AB0:
        return encode(address, 45, 8)
    if address == 0x006A2B40:
        return encode(address, WORDS[3])
    if address == 0x006A2BD0:
        return encode(address, 0xFFFFFFFF, writes=(float_bits(0.5),))
    if address == 0x006A2D30:
        return encode(address, cleanup=4, st0=f32(v[0] * v[1]))
    if address == 0x006A2EE0:
        kind = 1 if w[1] == 2 else (3 if w[1] else 2)
        return encode(address, int(bool(w[0] & (1 << kind))), 12)
    if address == 0x006A3120:
        return encode(address, w[4], 4)
    if address == 0x006A82F0:
        return encode(
            address, w[7], 8, writes=(w[0], w[1], 0x7FFFFFFF, 0xFFFFFFFF))
    if address == 0x006A8530:
        return encode(address, 0, 8)
    if address == 0x006A8580:
        return encode(address, st0=1.0)
    if address == 0x006A8650:
        return encode(address, 1, 4)
    if address == 0x006A86C0:
        return encode(address, 0)
    if address == 0x006A8700:
        return encode(address, 1, 8)
    if address == 0x006A8840:
        return encode(address, w[3], 4)
    if address == 0x006A8870:
        return encode(address, 1)
    if address == 0x006A88F0:
        return encode(address, 0, 8)
    if address == 0x006BD260:
        return encode(address, 115)
    if address == 0x006BD2C0:
        return encode(address, w[7], writes=(w[0], w[1], 3))
    if address == 0x006BD310:
        return encode(address, 0xFFFFFFFE)
    if address == 0x006ED440:
        return encode(address, 0xFFFFFFFF, writes=(w[2],))
    if address == 0x006ED4A0:
        return encode(address, cleanup=4, writes=(0, w[0] & ~0x4000))
    raise AssertionError(f"missing reference for 0x{address:08X}")


def verify_native_identity(snapshot: Path) -> None:
    emulator = Emulator(capture=str(snapshot))
    digest = hashlib.sha256()
    for address in ENTRIES:
        digest.update(emulator.read(address, 16))
    actual = digest.hexdigest()
    if actual != NATIVE_PREFIX_SHA256:
        raise AssertionError(
            f"run35 Weapon native identity {actual} != {NATIVE_PREFIX_SHA256}")


def native_projection(snapshot: Path, address: int) -> str:
    """Execute one accepted native body and normalize only pointer identities."""
    emu = Emulator(capture=str(snapshot))

    def alloc(size: int) -> int:
        return emu.alloc(bytes(size))

    def put32(pointer: int, offset: int, value: int) -> None:
        emu.write(pointer + offset, struct.pack("<I", u32(value)))

    def putf(pointer: int, offset: int, value: float) -> None:
        put32(pointer, offset, float_bits(value))

    logic = alloc(0x200)
    put32(logic, 0x58, WORDS[0])
    putf(logic, 0x1C4, VALUES[6])
    emu.write(0x00DEBA64, struct.pack("<I", logic))
    emu.write(0x00DEEEE8, struct.pack("<f", VALUES[7]))

    def integer(
        args: list[int] | tuple[int, ...] = (), *, ecx: int = 0,
        eax: int | None = None, al: bool = False,
        writes: tuple[int, ...] = ()) -> str:
        result = emu.call(address, args, ecx=ecx)
        observed = result.eax if eax is None else eax
        if al:
            observed = result.eax & 0xFF
        return encode(address, observed, result.stack_cleanup, writes=writes)

    def floating(
        args: list[int] | tuple[int, ...] = (), *, ecx: int = 0) -> str:
        result, extended = emu.call_float80(address, args, ecx=ecx)
        significand, exponent = struct.unpack("<QH", extended)
        if (exponent & 0x7FFF) == 0:
            value = -0.0 if exponent >> 15 else 0.0
        else:
            value = math.ldexp(
                significand / float(1 << 63),
                (exponent & 0x7FFF) - 16383)
            if exponent >> 15:
                value = -value
        result_text = encode(
            address, cleanup=result.stack_cleanup, st0=f32(value))
        if result_text[-20:] != extended.hex():
            raise AssertionError(
                f"0x{address:08X} x87 mismatch "
                f"{extended.hex()}/{result_text[-20:]}")
        return result_text

    if address == 0x00685CC0:
        weapon = alloc(0x80)
        emu.write(weapon + 0x60, b"\1")
        state = alloc(0x40)
        put32(state, 0x20, weapon)
        return integer(ecx=state)
    if address == 0x00685D20:
        state = alloc(0x40)
        put32(state, 0x24, WORDS[1])
        put32(state, 0x28, WORDS[2])
        return integer(ecx=state)
    if address == 0x00685DA0:
        state = alloc(0x40)
        put32(state, 0x24, WORDS[1])
        put32(state, 0x28, WORDS[2])
        return integer(ecx=state)
    if address == 0x00685DC0:
        return floating([0, 0])
    if address in (0x00685DD0, 0x00685E70, 0x00685EA0, 0x006A2DC0):
        data = alloc(0x160)
        putf(data, 0x04, VALUES[0])
        putf(data, 0x08, VALUES[0])
        putf(data, 0x0C, VALUES[1])
        putf(data, 0x10, VALUES[2])
        putf(data, 0x14, VALUES[3])
        putf(data, 0xA8, VALUES[3])
        wrapper = alloc(8)
        put32(wrapper, 4, data)
        if address == 0x00685DD0:
            return floating([float_bits(VALUES[4])], ecx=wrapper)
        if address == 0x00685E70:
            return floating(ecx=wrapper)
        if address == 0x00685EA0:
            return floating(ecx=wrapper)
        target = alloc(0x10)
        putf(target, 8, VALUES[4])
        return floating([target], ecx=wrapper)
    if address in (
        0x00685EF0, 0x00685F70, 0x00685FF0, 0x00686070,
        0x00686110, 0x00686190, 0x00686210, 0x00686290,
        0x00686310, 0x00686380, 0x006863F0,
    ):
        data = alloc(0x100)
        if address < 0x00686070:
            range_offset = 0xEC if address < 0x00685FF0 else 0xF0
        elif address < 0x00686210:
            range_offset = 0xF0 if address == 0x00686070 else 0xF4
        else:
            range_offset = 0xF8
        if address <= 0x00686290:
            duration_range = alloc(8)
            putf(duration_range, 0, VALUES[0])
            putf(duration_range, 4, VALUES[1])
            put32(data, range_offset, duration_range)
        scalar_offset = {
            0x00686310: 0x7C,
            0x00686380: 0x80,
            0x006863F0: 0x94,
        }.get(address)
        if scalar_offset is not None:
            putf(data, scalar_offset, VALUES[0])
        wrapper = alloc(8)
        put32(wrapper, 4, data)
        # Inputs are integral in this scenario, so ceil(x) == x. This tiny
        # native stub preserves the x87 double return without modeling CRT.
        ceil_stub = emu.alloc(b"\xDD\x44\x24\x04\xC3")
        emu.write(0x00BBA4DC, struct.pack("<I", ceil_stub))
        return integer(ecx=wrapper)
    if address == 0x006A2400:
        slots = alloc(24)
        put32(slots, 0, WORDS[0])
        slot_data = alloc(8)
        put32(slot_data, 4, WORDS[2])
        slot_wrapper = alloc(16)
        put32(slot_wrapper, 4, slot_data)
        owner = alloc(0xA0)
        put32(owner, 0x0C, slot_wrapper)
        put32(owner, 0x94, slots)
        output = alloc(4)
        result = emu.call(address, [output], ecx=owner)
        return encode(
            address, result.eax, result.stack_cleanup,
            writes=(struct.unpack("<I", emu.read(output, 4))[0],))
    if address == 0x006A2430:
        slots = alloc(24)
        for index, value in enumerate(WORDS[:6]):
            put32(slots, index * 4, value)
        owner = alloc(0xA0)
        put32(owner, 0x94, slots)
        return integer([WORDS[7] % 6], ecx=owner)
    if address == 0x006A2440:
        owner = alloc(0x20)
        put32(owner, 0x14, WORDS[0])
        return integer(ecx=owner)
    if address == 0x006A2480:
        weapon_set = alloc(0x480)
        put32(weapon_set, 0x47C, WORDS[1])
        game_object = alloc(0x380)
        put32(game_object, 0x374, weapon_set)
        owner = alloc(0x20)
        put32(owner, 0x10, game_object)
        return integer(ecx=owner)
    if address == 0x006A24A0:
        slots = alloc(24)
        for index, value in enumerate(WORDS[:6]):
            data = alloc(0xD8)
            put32(data, 0xD0, value)
            put32(data, 0xD4, value << 1)
            wrapper = alloc(8)
            put32(wrapper, 4, data)
            weapon = alloc(8)
            put32(weapon, 4, wrapper)
            put32(slots, index * 4, weapon)
        owner = alloc(0xA0)
        put32(owner, 0x94, slots)
        result = emu.call(address, ecx=owner)
        outputs = struct.unpack("<II", emu.read(owner + 4, 8))
        return encode(address, result.eax, result.stack_cleanup, writes=outputs)
    if address == 0x006A24F0:
        slots = alloc(24)
        for index, value in enumerate(WORDS[:6]):
            runtime = alloc(0x40)
            put32(runtime, 0x30, value)
            putf(runtime, 0x34, VALUES[index])
            putf(runtime, 0x38, VALUES[(index + 1) % len(VALUES)])
            putf(runtime, 0x3C, VALUES[(index + 2) % len(VALUES)])
            weapon = alloc(0x70)
            put32(weapon, 0x68, runtime)
            put32(slots, index * 4, weapon)
        owner = alloc(0xA0)
        put32(owner, 0x94, slots)
        output = alloc(12)
        result = emu.call(address, [output], ecx=owner)
        if result.eax != output:
            raise AssertionError("006A24F0 hidden output EAX")
        return encode(
            address, WORDS[7], result.stack_cleanup,
            writes=struct.unpack("<III", emu.read(output, 12)))
    if address == 0x006A2560:
        slots = alloc(24)
        token = 0x13579BDF
        contributions = (100, 90, 20, 0, 7, 3)
        for index, value in enumerate(contributions):
            runtime = alloc(0x24)
            put32(runtime, 0x0C, value)
            put32(runtime, 0x10, token)
            weapon = alloc(0x70)
            put32(weapon, 0x68, runtime)
            put32(slots, index * 4, weapon)
        owner = alloc(0xA0)
        put32(owner, 0x94, slots)
        target = alloc(0xE0)
        put32(target, 0xDC, token)
        return integer([target, 0], ecx=owner)
    if address == 0x006A25F0:
        owner = alloc(0xA0)
        return integer([0, WORDS[7] % 6], ecx=owner)
    if address == 0x006A2630:
        data = alloc(0x140)
        emu.write(data + 0x12F, b"\1")
        wrapper = alloc(8)
        put32(wrapper, 4, data)
        weapon = alloc(8)
        put32(weapon, 4, wrapper)
        return floating([weapon, float_bits(VALUES[1]), 0])
    if address == 0x006A2840:
        state = alloc(0x40)
        put32(state, 0x24, WORDS[1])
        put32(state, 0x28, WORDS[2])
        return integer(ecx=state)
    if address == 0x006A2A70:
        emu.write(0x00DF03FC, bytes(4))
        emu.bind_function(0x005B2370, lambda _: 0)
        state = alloc(0x90)
        result = emu.call(address, [0], ecx=state)
        if result.eax != state:
            raise AssertionError("006A2A70 this return")
        return encode(address, WORDS[0], result.stack_cleanup)
    if address == 0x006A2AB0:
        ability = alloc(4)
        game_object = alloc(0x370)
        put32(game_object, 0x36C, ability)
        owner = alloc(0x28)
        put32(owner, 0x24, game_object)
        state = alloc(0x20)
        put32(state, 0x18, owner)

        def boost(machine: Emulator) -> int:
            machine.write(machine.stack_arg(1), struct.pack("<f", VALUES[0]))
            return 1

        emu.bind_function(0x004D9CE0, boost, stack_cleanup=12)
        emu.bind_function(0x005A94C0, lambda _: 91)
        return integer([WORDS[1], WORDS[2]], ecx=state)
    if address == 0x006A2B40:
        callback = emu.bind_stub(lambda _: 1, stack_cleanup=4)
        vtable = alloc(0xDC)
        put32(vtable, 0xD8, callback)
        manager = alloc(4)
        put32(manager, 0, vtable)
        put32(manager, 0, vtable)
        emu.write(0x00DF03FC, struct.pack("<I", manager))
        weapon = alloc(0x88)
        state = alloc(0x20)
        put32(state, 0x18, weapon)
        return integer(ecx=state)
    if address == 0x006A2BD0:
        weapon = alloc(0x90)
        emu.write(weapon + 0x60, b"\1")
        put32(weapon, 0x5C, 0)
        target = alloc(0x98)
        put32(target, 0x94, 1 << 20)
        put32(weapon, 0x24, target)
        state = alloc(0x30)
        put32(state, 0x18, weapon)
        put32(state, 0x20, weapon)
        put32(state, 0x24, WORDS[1])
        put32(state, 0x28, WORDS[2])
        result = emu.call(address, ecx=state)
        return encode(
            address, result.eax, result.stack_cleanup,
            writes=(struct.unpack("<I", emu.read(weapon + 0x88, 4))[0],))
    if address == 0x006A2D30:
        data = alloc(0x130)
        wrapper = alloc(8)
        put32(wrapper, 4, data)
        ability = alloc(4)
        target = alloc(0x370)
        put32(target, 0x36C, ability)
        put32(target, 0x84, 1 << 29)
        settings = alloc(0x44)
        putf(settings, 0x40, VALUES[1])
        emu.write(0x00DFC4B4, struct.pack("<I", settings))

        def speed(machine: Emulator) -> int:
            machine.write(machine.stack_arg(1), struct.pack("<f", VALUES[0]))
            return 1

        emu.bind_function(0x004D9CE0, speed, stack_cleanup=12)
        return floating([target], ecx=wrapper)
    if address == 0x006A2EE0:
        mask = alloc(4)
        put32(mask, 0, WORDS[0])
        return integer([mask, WORDS[1], WORDS[6]])
    if address == 0x006A3120:
        data = alloc(0x68)
        put32(data, 0x60, WORDS[4])
        put32(data, 0x64, WORDS[3])
        wrapper = alloc(8)
        put32(wrapper, 4, data)
        target_state = alloc(0x28)
        put32(target_state, 0x24, WORDS[2])
        target = alloc(0x3D0)
        put32(target, 0x3CC, target_state)
        return integer([target], ecx=wrapper)
    if address == 0x006A82F0:
        data = alloc(0x150)
        putf(data, 0x44, -4.0)
        putf(data, 0x48, 4.0)
        put32(data, 0x84, 1)
        putf(data, 0x94, 0.0)
        wrapper = alloc(8)
        put32(wrapper, 4, data)
        output = emu.alloc(b"\xA5" * 0x78)
        ceil_stub = emu.alloc(b"\xDD\x44\x24\x04\xC3")
        emu.write(0x00BBA4DC, struct.pack("<I", ceil_stub))
        result = emu.call(address, [wrapper, WORDS[1]], ecx=output)
        if result.eax != output:
            raise AssertionError("006A82F0 this return")
        writes = (
            WORDS[0] if struct.unpack("<I", emu.read(output + 4, 4))[0] == wrapper
            else 0,
            struct.unpack("<I", emu.read(output + 0x14, 4))[0],
            struct.unpack("<I", emu.read(output + 0x24, 4))[0],
            struct.unpack("<I", emu.read(output + 0x5C, 4))[0],
        )
        return encode(address, WORDS[7], result.stack_cleanup, writes=writes)
    if address == 0x006A8530:
        data = alloc(0x10)
        putf(data, 8, VALUES[0])
        wrapper = alloc(8)
        put32(wrapper, 4, data)
        weapon = alloc(8)
        put32(weapon, 4, wrapper)
        return integer([0, 0], ecx=weapon, al=True)
    if address == 0x006A8580:
        weapon = alloc(0x70)
        return floating(ecx=weapon)
    if address == 0x006A8650:
        data = alloc(0x90)
        wrapper = alloc(8)
        put32(wrapper, 4, data)
        weapon = alloc(8)
        put32(weapon, 4, wrapper)
        return integer([0], ecx=weapon, al=True)
    if address == 0x006A86C0:
        state_type = alloc(8)
        put32(state_type, 4, WORDS[1])
        state = alloc(8)
        put32(state, 4, state_type)
        weapon = alloc(0x70)
        put32(weapon, 0x64, state)
        return integer(ecx=weapon, al=True)
    if address == 0x006A8700:
        data = alloc(0x148)
        emu.write(data + 0x144, b"\1")
        wrapper = alloc(8)
        put32(wrapper, 4, data)
        weapon = alloc(8)
        put32(weapon, 4, wrapper)
        return integer([0, 0], ecx=weapon, al=True)
    if address in (0x006A8840, 0x006A8870):
        state_type = alloc(8)
        put32(state_type, 4, 1 if address == 0x006A8870 else WORDS[1])
        state = alloc(8)
        put32(state, 4, state_type)
        weapon = alloc(0x70)
        put32(weapon, 0x64, state)
        put32(weapon, 0x1C, WORDS[3])
        if address == 0x006A8840:
            return integer([1], ecx=weapon)
        data = alloc(0x140)
        emu.write(data + 0x13A, b"\1")
        wrapper = alloc(8)
        put32(wrapper, 4, data)
        put32(weapon, 4, wrapper)
        return integer(ecx=weapon, al=True)
    if address == 0x006A88F0:
        weapon = alloc(0x78)
        emu.write(weapon + 0x62, b"\1")
        put32(weapon, 0x58, WORDS[1])
        target = alloc(0xE0)
        put32(target, 0xDC, WORDS[2])
        output = emu.alloc(b"\xA5" * 12)
        result = emu.call(address, [output, target], ecx=weapon)
        if emu.read(output, 12) != b"\xA5" * 12:
            raise AssertionError("006A88F0 mismatch path wrote output")
        return encode(address, result.eax & 0xFF, result.stack_cleanup)
    if address == 0x006BD260:
        duration_range = alloc(8)
        putf(duration_range, 0, 7.0 / VALUES[6])
        putf(duration_range, 4, 3.0 / VALUES[6])
        data = alloc(0x100)
        put32(data, 0xF4, duration_range)
        wrapper = alloc(8)
        put32(wrapper, 4, data)
        weapon = alloc(0x70)
        put32(weapon, 4, wrapper)
        put32(weapon, 0x1C, 1)
        state = alloc(0x30)
        put32(state, 0x20, weapon)
        put32(state, 0x24, WORDS[1])
        put32(state, 0x28, WORDS[2])
        ceil_stub = emu.alloc(b"\xDD\x44\x24\x04\xC3")
        emu.write(0x00BBA4DC, struct.pack("<I", ceil_stub))
        return integer(ecx=state)
    if address == 0x006BD2C0:
        emu.bind_function(0x00686190, lambda _: WORDS[5])
        emu.bind_function(0x00686110, lambda _: WORDS[4])
        emu.bind_function(0x006A2AB0, lambda _: WORDS[1], stack_cleanup=8)
        emu.bind_function(0x006BCF30, lambda _: 0, stack_cleanup=4)
        callback = emu.bind_stub(lambda _: WORDS[7])
        vtable = alloc(0x18)
        put32(vtable, 0x14, callback)
        weapon = alloc(8)
        wrapper = alloc(8)
        put32(weapon, 4, wrapper)
        state = alloc(0x30)
        put32(state, 0, vtable)
        put32(state, 0x20, weapon)
        result = emu.call(address, ecx=state)
        writes = (
            struct.unpack("<I", emu.read(state + 0x24, 4))[0],
            struct.unpack("<I", emu.read(state + 0x28, 4))[0],
            3,
        )
        return encode(address, result.eax, result.stack_cleanup, writes=writes)
    if address == 0x006BD310:
        weapon = alloc(0x20)
        put32(weapon, 0x1C, WORDS[0])
        state = alloc(0x24)
        put32(state, 0x20, weapon)
        return integer(ecx=state)
    if address == 0x006ED440:
        emu.bind_function(0x006A8400, lambda _: 0)
        emu.bind_function(0x006BCF30, lambda _: 0, stack_cleanup=4)
        data = alloc(0x140)
        emu.write(data + 0x13F, b"\1")
        put32(data, 0x70, WORDS[2])
        wrapper = alloc(8)
        put32(wrapper, 4, data)
        weapon = alloc(0x70)
        put32(weapon, 4, wrapper)
        state = alloc(0x24)
        put32(state, 0x20, weapon)
        result = emu.call(address, ecx=state)
        return encode(
            address, result.eax, result.stack_cleanup,
            writes=(struct.unpack("<I", emu.read(weapon + 0x1C, 4))[0],))
    if address == 0x006ED4A0:
        game_object = alloc(0x84)
        put32(game_object, 0x80, WORDS[0])
        owner = alloc(0x28)
        put32(owner, 0x24, game_object)
        state = alloc(0x30)
        put32(state, 0x18, owner)
        result = emu.call(address, [0], ecx=state)
        writes = (
            struct.unpack("<I", emu.read(state + 0x28, 4))[0],
            struct.unpack("<I", emu.read(game_object + 0x80, 4))[0],
        )
        return encode(address, result.eax, result.stack_cleanup, writes=writes)
    raise AssertionError(f"missing native scenario for 0x{address:08X}")


def verify(executable: Path, snapshot: Path, mutation_proof: bool) -> None:
    if len(ENTRIES) != 50 or len(set(ENTRIES)) != 50:
        raise AssertionError("accepted entry set must contain 50 unique VAs")
    verify_native_identity(snapshot)
    checker = Checker(executable)
    mismatches = 0
    mutations_detected = 0
    try:
        for address in ENTRIES:
            expected = native_projection(snapshot, address)
            transcribed = reference(address)
            if expected != transcribed:
                raise AssertionError(
                    f"native/transcription mismatch 0x{address:08X}: "
                    f"{expected}/{transcribed}")
            actual = checker.call(address)
            if actual != expected:
                mismatches += 1
                print(
                    f"mismatch 0x{address:08X}: "
                    f"game={expected} port={actual}", file=sys.stderr)
            if mutation_proof:
                mutated = checker.call(address, mutate=True)
                if mutated != expected:
                    mutations_detected += 1
    finally:
        checker.close()
    if mismatches:
        raise AssertionError(f"{mismatches} ObjectLogic mismatches")
    if mutation_proof and mutations_detected != len(ENTRIES):
        raise AssertionError(
            f"mutation sensitivity {mutations_detected}/{len(ENTRIES)}")
    print(
        f"objectlogic weapon: entries={len(ENTRIES)} "
        f"observations={len(ENTRIES)} mismatches=0 "
        f"mutations={mutations_detected if mutation_proof else 'not-run'}")


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("checker", type=Path)
    parser.add_argument(
        "--capture", type=Path, required=True,
        help="absolute run35 snapshot directory")
    parser.add_argument("--mutation-proof", action="store_true")
    args = parser.parse_args()
    if not args.capture.is_absolute():
        parser.error("--capture must be absolute")
    verify(args.checker, args.capture, args.mutation_proof)


if __name__ == "__main__":
    main()
