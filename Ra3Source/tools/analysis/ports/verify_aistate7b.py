"""Release oracle for the 0x007B0000 AI-state methods.

Each case runs the shipped bytes in Unicorn and ra3_aistate7bcheck on the same
object graph. EAX, object bytes and the call trace must match. Stack temporaries
are logged as ``ptr``, or as captured bytes when a case asks for them.
"""

from __future__ import annotations

import struct
import subprocess
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from emulate import STACK_BASE, STACK_SIZE, Emulator  # noqa: E402
from unicorn import x86_const as uc  # noqa: E402

SIZE = 0x480
GAME_LOGIC = 0x00DEBA64
LIMIT_ABS = 0x00BD95B8
NEAR_ABS = 0x00BD952C
LIMIT = 0x43C80000
NEAR = 0x42480000
EAX_IN = 0xA5A5A5A5

# Virtual slots planted in test vtables. Each address is bound with the
# thiscall cleanup of that slot's argument count.
V_0_1 = 0x00420100
V_10_0 = 0x00420110
V_14_2 = 0x00420120
V_18_1 = 0x00420130
V_18_2 = 0x00420140
V_1C_1 = 0x00420150
V_1C_2 = 0x00420160
V_20_1 = 0x00420170
V_24_1 = 0x00420180
V_2C_1 = 0x00420190
V_34_1 = 0x004201A0
V_38_0 = 0x004201B0
V_58_1 = 0x004201C0
V_64_1 = 0x004201D0
V_1B0_0 = 0x004201E0
V_270_0 = 0x004201F0
V_274_0 = 0x00420200

POPS = {
    0x007B08F0: 4, 0x007B1150: 4, 0x007B17D0: 4, 0x007B19F0: 4, 0x007B1AF0: 4,
    0x007B1BB0: 4, 0x007B1C60: 4, 0x007B1E20: 4, 0x007B1FA0: 4, 0x007B2010: 4,
    0x007B20D0: 4, 0x007B28D0: 4, 0x007B2D50: 4, 0x007B3950: 4, 0x007B3EF0: 4,
    0x007B4F90: 4, 0x007B52A0: 4, 0x007B52D0: 4, 0x007B5320: 4, 0x007B5460: 4,
    0x007B5630: 4, 0x007B56B0: 4, 0x007B5830: 4, 0x007B60D0: 4, 0x007B62D0: 4,
    0x007B62F0: 4, 0x007B6530: 4, 0x007B6670: 4, 0x007B66E0: 4, 0x007BB4A0: 4,
    0x007BB930: 4, 0x007BC4C0: 4, 0x007BD7A0: 4, 0x007BD880: 4, 0x007BDA90: 4,
}

CLEANUP = {
    0x004166B0: 0, 0x00416820: 0, 0x004D2F98: 0, 0x004D9AB0: 4, 0x004D9AF0: 4,
    0x0054A090: 0, 0x005A9F60: 8, 0x005AA240: 4, 0x005AA250: 0, 0x005B9F30: 0,
    0x005BDB90: 20, 0x005C0A40: 4, 0x00681840: 0, 0x006818C0: 0, 0x00687500: 4,
    0x00687810: 0, 0x00687C30: 0, 0x006996C0: 0, 0x006A4F80: 8, 0x006A4FB0: 0,
    0x006A5060: 8, 0x006A5F50: 0, 0x006A6210: 0, 0x006B6E30: 4, 0x006B7040: 8,
    0x006D2020: 8, 0x006E4760: 0, 0x006E6360: 4, 0x006EFE40: 0, 0x006F0A20: 4,
    0x006F6A20: 0, 0x00748590: 4, 0x007527C0: 4, 0x00757B40: 8, 0x0078D1E0: 4,
    0x00799B70: 0, 0x00799C30: 4, 0x007A1BD0: 4, 0x007A5B20: 0, 0x007A9AB0: 4,
    0x007AB5B0: 4, 0x007B1190: 4, 0x007B12D0: 0, 0x007B42E0: 0, 0x007B7B50: 0,
    0x007BB2B0: 4, 0x007BB640: 4,
    V_0_1: 4, V_10_0: 0, V_14_2: 8, V_18_1: 4, V_18_2: 8, V_1C_1: 4, V_1C_2: 8,
    V_20_1: 4, V_24_1: 4, V_2C_1: 4, V_34_1: 4, V_38_0: 0, V_58_1: 4, V_64_1: 4,
    V_1B0_0: 0, V_270_0: 0, V_274_0: 0,
}

COUNTED = [
    0x007B0880, 0x007B08F0, 0x007B0940, 0x007B1150, 0x007B17D0, 0x007B19F0, 0x007B1A80,
    0x007B1AF0, 0x007B1B70, 0x007B1BB0, 0x007B1C10, 0x007B1C60, 0x007B1E20, 0x007B1F30,
    0x007B1FA0, 0x007B2010, 0x007B2050, 0x007B20D0, 0x007B28D0, 0x007B2D50, 0x007B3950,
    0x007B3EF0, 0x007B4780, 0x007B4F90, 0x007B52A0, 0x007B52D0, 0x007B5320, 0x007B5370,
    0x007B5460, 0x007B5630, 0x007B56B0, 0x007B5830, 0x007B5E30, 0x007B60D0, 0x007B62D0,
    0x007B62F0, 0x007B64A0, 0x007B6530, 0x007B65B0, 0x007B6670, 0x007B66E0, 0x007B9820,
    0x007BB4A0, 0x007BB8E0, 0x007BB930, 0x007BC320, 0x007BC4C0, 0x007BD770, 0x007BD7A0,
    0x007BD830, 0x007BD880, 0x007BD910, 0x007BDA40, 0x007BDA90, 0x007BDEF0,
]


def bits(value: float) -> int:
    return struct.unpack("<I", struct.pack("<f", value))[0]


class Mem:
    def __init__(self) -> None:
        self.blobs: list[bytearray] = []
        self.fix: list[tuple[int, int, int]] = []

    def add(self) -> int:
        self.blobs.append(bytearray(SIZE))
        return len(self.blobs) - 1

    def w(self, index: int, offset: int, value: int) -> None:
        struct.pack_into("<I", self.blobs[index], offset, value & 0xFFFFFFFF)

    def b(self, index: int, offset: int, value: int) -> None:
        self.blobs[index][offset] = value & 0xFF

    def link(self, index: int, offset: int, target: int) -> None:
        self.fix.append((index, offset, target))


def rig():
    mem = Mem()
    state, machine, unit = mem.add(), mem.add(), mem.add()
    mem.link(state, 0x18, machine)
    mem.link(machine, 0x24, unit)
    return mem, state, machine, unit


def loco_rig():
    mem, state, machine, unit = rig()
    loco = mem.add()
    mem.link(unit, 0x374, loco)
    return mem, state, machine, unit, loco


def plant(mem: Mem, obj: int, slots: dict[int, int]) -> int:
    table = mem.add()
    mem.link(obj, 0, table)
    for offset, fn in slots.items():
        mem.w(table, offset, fn)
    return table


CASES: list[dict] = []


def add(va: int, mem: Mem, rets: list[tuple[int, int, str]], args: list[str] | None = None,
        eax: int = EAX_IN, gl: str = "0", limit: int = LIMIT, near: int = NEAR,
        capture: int = 0) -> None:
    CASES.append({
        "va": va, "mem": mem, "rets": rets, "args": args or [], "eax": eax, "gl": gl,
        "limit": limit, "near": near, "capture": capture,
    })


def build() -> None:
    # 007B0880 garrison enter: flag already set, then flag clear so the pair runs.
    mem, state, machine, unit = rig()
    occupant = mem.add()
    plant(mem, occupant, {0x1C: V_1C_1})
    mem.w(occupant, 0x68, 0x44)
    mem.w(unit, 0xA0, 1 << 0x15)
    add(0x007B0880, mem, [(0x00416820, 1, f"@{occupant}"), (0x007A1BD0, 1, f"@{occupant}"),
                          (V_1C_1, 1, "9")])
    mem, state, machine, unit = rig()
    occupant = mem.add()
    plant(mem, occupant, {0x1C: V_1C_1})
    mem.w(occupant, 0x68, 0)
    add(0x007B0880, mem, [(0x00416820, 1, f"@{occupant}"), (0x007A1BD0, 1, f"@{occupant}"),
                          (0x006A4F80, 2, "1"), (0x006A4FB0, 0, "2"), (V_1C_1, 1, "3")])

    # 007B08F0 exit with and without an occupant, flag set and clear.
    mem, state, *_ = rig()
    add(0x007B08F0, mem, [], ["1"])
    mem, state, machine, unit = rig()
    occupant = mem.add()
    plant(mem, occupant, {0: V_0_1})
    mem.link(state, 0x20, occupant)
    mem.w(unit, 0xA0, 1 << 0x15)
    add(0x007B08F0, mem, [(V_0_1, 1, "4"), (0x006A4F80, 2, "5"), (0x006A4FB0, 0, "6")], ["2"])

    # 007B0940 update: empty occupant, then a two-step contain walk.
    mem, state, *_ = rig()
    add(0x007B0940, mem, [(0x006EFE40, 0, "0")])
    mem, state, machine, unit = rig()
    occupant = mem.add()
    plant(mem, occupant, {0x10: V_10_0})
    mem.link(state, 0x20, occupant)
    add(0x007B0940, mem, [(V_10_0, 0, "8"), (0x006EFE40, 0, "11"), (0x006EFE40, 0, "0")])

    # 007B1150 dead exit, bit clear then set.
    mem, *_ = rig()
    add(0x007B1150, mem, [], ["1"], eax=0x11223344)
    mem, _, _, unit = rig()
    mem.w(unit, 0xB0, 1 << 16)
    add(0x007B1150, mem, [(0x006A4F80, 2, "1"), (0x006A4FB0, 0, "7")], ["1"])

    # 007B17D0 / 007B19F0 move and cover exits.
    mem, *_ = loco_rig()
    add(0x007B17D0, mem, [(0x007B1190, 1, "21"), (0x006B7040, 2, "22")], ["3"])
    mem, *_ = rig()
    add(0x007B17D0, mem, [(0x007B1190, 1, "21")], ["3"])
    mem, state, _, unit, _ = loco_rig()
    mem.w(state, 0x20, bits(0))
    mem.w(state, 0x24, bits(0))
    mem.w(unit, 0x38, bits(0))
    mem.w(unit, 0x3C, bits(0))
    add(0x007B19F0, mem, [(0x007B1190, 1, "1"), (0x006B7040, 2, "2"), (0x00687C30, 0, "30"),
                          (0x006F6A20, 0, "31")], ["4"])
    mem, state, _, unit = rig()
    mem.w(state, 0x20, bits(100))
    mem.w(state, 0x24, bits(100))
    mem.w(unit, 0x38, bits(0))
    mem.w(unit, 0x3C, bits(0))
    add(0x007B19F0, mem, [(0x007B1190, 1, "5")], ["4"])

    # 007B1A80 SA update: deadline passed, deadline held, goal copied.
    mem, state, *_ = rig()
    mem.b(state, 0x54, 1)
    mem.w(state, 0x50, 10)
    logic = mem.add()
    mem.w(logic, 0x58, 11)
    add(0x007B1A80, mem, [], gl=f"@{logic}")
    mem, state, _, unit = rig()
    mem.b(state, 0x54, 1)
    mem.w(state, 0x50, 20)
    logic = mem.add()
    mem.w(logic, 0x58, 20)
    add(0x007B1A80, mem, [(0x004D9AB0, 1, "1")], gl=f"@{logic}")
    mem, state, *_ = rig()
    goal = mem.add()
    mem.w(goal, 0x38, 1)
    mem.w(goal, 0x3C, 2)
    mem.w(goal, 0x40, 3)
    add(0x007B1A80, mem, [(0x005B9F30, 0, f"@{goal}"), (0x007B12D0, 0, "9")])
    mem, *_ = rig()
    add(0x007B1A80, mem, [(0x005B9F30, 0, "0"), (0x007B12D0, 0, "8")])

    # 007B1AF0 out of the way.
    mem, _, _, unit, loco = loco_rig()
    mem.w(unit, 0xB0, 1 << 0x16)
    add(0x007B1AF0, mem, [(0x007B1190, 1, "1"), (0x006996C0, 0, "2"), (0x006A4F80, 2, "3"),
                          (0x006A4FB0, 0, "4")], ["6"])
    mem, *_ = rig()
    add(0x007B1AF0, mem, [(0x007B1190, 1, "5")], ["6"])

    # 007B1B70 / 007B1C10 tighten and panic updates.
    mem, state, *_ = rig()
    add(0x007B1B70, mem, [(0x007B12D0, 0, "1")])
    mem, state, _, _, loco = loco_rig()
    mem.b(state, 0x54, 1)
    path = mem.add()
    mem.link(loco, 0x150, path)
    add(0x007B1B70, mem, [(0x007B12D0, 0, "2")])
    mem, state, *_ = rig()
    add(0x007B1C10, mem, [(0x007B12D0, 0, "1")])
    mem, state, _, _, loco = loco_rig()
    mem.b(state, 0x54, 1)
    path = mem.add()
    node = mem.add()
    mem.link(loco, 0x150, path)
    mem.link(path, 0x14, node)
    mem.w(node, 0x0C, 4)
    mem.w(node, 0x10, 5)
    mem.w(node, 0x14, 6)
    add(0x007B1C10, mem, [(0x007B12D0, 0, "3")])

    # Panic / repulsor / fear exits.
    mem, *_ = rig()
    add(0x007B1BB0, mem, [(0x007B1190, 1, "12")], ["1"], eax=0x10)
    mem, _, _, unit, loco = loco_rig()
    mem.w(unit, 0xB4, 1 << 14)
    add(0x007B1BB0, mem, [(0x007B1190, 1, "1"), (0x007527C0, 1, "2"), (0x006A4F80, 2, "3"),
                          (0x006A4FB0, 0, "4")], ["1"])
    mem, *_ = rig()
    add(0x007B1C60, mem, [(0x007B1190, 1, "15")], ["2"])
    mem, _, _, unit = rig()
    mem.w(unit, 0xB4, 1 << 14)
    add(0x007B1C60, mem, [(0x007B1190, 1, "1"), (0x006A4F80, 2, "2"), (0x006A4FB0, 0, "3")], ["2"])
    mem, *_ = rig()
    add(0x007B1E20, mem, [(0x007B1190, 1, "9")], ["3"])
    mem, _, _, unit, loco = loco_rig()
    mem.w(unit, 0xCC, 1 << 15)
    add(0x007B1E20, mem, [(0x007B1190, 1, "1"), (0x006A4F80, 2, "2"), (0x006A4FB0, 0, "3"),
                          (0x007527C0, 1, "4")], ["3"])

    # 007B1F30 / 007B1FA0 cower.
    mem, *_ = rig()
    add(0x007B1F30, mem, [(0x006A5F50, 0, "41"), (0x00687500, 1, "1"), (0x006D2020, 2, "2"),
                          (0x006A4F80, 2, "3"), (0x006A4FB0, 0, "4"), (0x005AA240, 1, "5"),
                          (0x00799B70, 0, "6")])
    mem, state, _, unit = rig()
    mem.w(unit, 0xCC, 1 << 0x14)
    mem.b(state, 0x20, 0x41)
    add(0x007B1F30, mem, [(0x006A5F50, 0, "7"), (0x00687500, 1, "1"), (0x006D2020, 2, "2"),
                          (0x005AA240, 1, "5"), (0x00799B70, 0, "6")])
    mem, state, *_ = rig()
    mem.b(state, 0x20, 2)
    add(0x007B1FA0, mem, [(0x00687500, 1, "1"), (0x006D2020, 2, "2"), (0x005AA250, 0, "3"),
                          (0x00799C30, 1, "4")], ["8"])
    mem, state, _, unit = rig()
    mem.b(state, 0x20, 2)
    mem.w(unit, 0xCC, 1 << 0x14)
    add(0x007B1FA0, mem, [(0x00687500, 1, "1"), (0x006D2020, 2, "2"), (0x006A4F80, 2, "3"),
                          (0x006A4FB0, 0, "4"), (0x005AA250, 0, "5"), (0x00799C30, 1, "6")], ["8"])

    # 007B2010 / 007B2050 cower move.
    mem, *_ = rig()
    add(0x007B2010, mem, [(0x007B1190, 1, "19")], ["1"])
    mem, *_ = loco_rig()
    add(0x007B2010, mem, [(0x007B1190, 1, "1"), (0x007527C0, 1, "2")], ["1"])
    mem, *_ = rig()
    add(0x007B2050, mem, [(0x005B9F30, 0, "0")])
    mem, state, *_ = loco_rig()
    add(0x007B2050, mem, [(0x005B9F30, 0, "@1"), (0x007B12D0, 0, "4")])
    mem, state, _, _, loco = loco_rig()
    mem.b(state, 0x54, 1)
    path = mem.add()
    node = mem.add()
    mem.link(loco, 0x150, path)
    mem.link(path, 0x14, node)
    add(0x007B2050, mem, [(0x005B9F30, 0, "@1")])

    # 007B20D0 back away.
    mem, *_ = rig()
    add(0x007B20D0, mem, [(0x007B1190, 1, "3")], ["1"])
    mem, _, _, unit, loco = loco_rig()
    mem.w(unit, 0xCC, 1 << 16)
    add(0x007B20D0, mem, [(0x007B1190, 1, "1"), (0x006A4F80, 2, "2"), (0x006A4FB0, 0, "3"),
                          (0x007527C0, 1, "4")], ["1"])

    # 007B28D0 / 007B2D50 approach exits. Near limit is 50; zero distance calls through.
    def approach(va: int, status: bool) -> None:
        mem, state, _, unit, loco = loco_rig()
        templ = mem.add()
        mem.link(unit, 4, templ)
        mem.w(templ, 0xC0, 1 << 0x1A)
        node = mem.add()
        mem.link(loco, 0x214, node)
        mem.w(node, 0x40, 0xFF)
        plant(mem, loco, {0x274: V_274_0, 0x270: V_270_0})
        mem.w(state, 0x20, bits(0))
        mem.w(state, 0x24, bits(0))
        mem.w(unit, 0x38, bits(0))
        mem.w(unit, 0x3C, bits(0))
        calls = [(0x007B1190, 1, "1")]
        if status:
            calls.append((0x006D2020, 2, "2"))
        calls += [(0x006E6360, 1, "3"), (V_274_0, 0, "1"), (0x0078D1E0, 1, "4"), (V_270_0, 0, "5")]
        add(va, mem, calls, ["9"])
        mem, *_ = rig()
        calls = [(0x007B1190, 1, "6")]
        if status:
            calls.append((0x006D2020, 2, "7"))
        add(va, mem, calls, ["9"])

    approach(0x007B28D0, True)
    approach(0x007B2D50, False)

    # 007B3950 / 007B3EF0 / 007B4780.
    mem, *_ = rig()
    add(0x007B3950, mem, [(0x007B1190, 1, "1")], ["2"])
    mem, *_ = rig()
    add(0x007B3950, mem, [(0x007B1190, 1, "0")], ["0"])
    mem, *_ = rig()
    add(0x007B3EF0, mem, [(0x007B1190, 1, "1"), (0x006D2020, 2, "2")], ["3"])
    mem, *_ = rig()
    add(0x007B3EF0, mem, [(0x007B1190, 1, "4"), (0x006D2020, 2, "5")], ["3"])
    mem, *_ = rig()
    add(0x007B4780, mem, [(0x007B42E0, 0, "ffffffff"), (0x006D2020, 2, "1")])
    mem, *_ = rig()
    add(0x007B4780, mem, [(0x007B42E0, 0, "0"), (0x006D2020, 2, "2")])

    # 007B4F90 attack move exit.
    mem, state, _, unit, loco = loco_rig()
    child = mem.add()
    plant(mem, child, {0x1C: V_1C_1})
    mem.link(state, 0x58, child)
    mem.w(unit, 0xB0, 1 << 0x19)
    mem.w(unit, 0x90, 1 << 0x0F)
    add(0x007B4F90, mem, [(0x006A4F80, 2, "1"), (0x006A4FB0, 0, "2"), (V_1C_1, 1, "3"),
                          (0x007B1190, 1, "4"), (0x006B7040, 2, "5"), (0x006D2020, 2, "6")], ["7"])
    mem, state, _, unit = rig()
    child = mem.add()
    plant(mem, child, {0x1C: V_1C_1})
    mem.link(state, 0x58, child)
    add(0x007B4F90, mem, [(V_1C_1, 1, "1"), (0x007B1190, 1, "2")], ["7"])

    # Waypoint exits and the exact update.
    mem, _, _, _, loco = loco_rig()
    node = mem.add()
    mem.link(loco, 0x214, node)
    mem.w(node, 0x40, 8)
    add(0x007B52A0, mem, [(0x007B1190, 1, "1")], ["2"])
    mem, *_ = rig()
    add(0x007B52A0, mem, [(0x007B1190, 1, "3")], ["2"])
    mem, *_ = loco_rig()
    add(0x007B52D0, mem, [(0x007B1190, 1, "1"), (0x007527C0, 1, "2")], ["4"])
    mem, *_ = rig()
    add(0x007B52D0, mem, [(0x007B1190, 1, "1"), (0x007527C0, 1, "0")], ["4"])
    mem, state, _, _, loco = loco_rig()
    node = mem.add()
    mem.link(loco, 0x214, node)
    mem.w(state, 0x50, 0x55)
    mem.w(node, 0x40, 0xFF)
    add(0x007B5320, mem, [(0x007B1190, 1, "1"), (0x006B6E30, 1, "2")], ["6"])
    mem, *_ = rig()
    add(0x007B5320, mem, [(0x007B1190, 1, "9")], ["6"])
    mem, *_ = loco_rig()
    add(0x007B5370, mem, [(0x007B12D0, 0, "1")])
    mem, *_ = rig()
    add(0x007B5370, mem, [(0x007B12D0, 0, "2")])

    # 007B5460 attack follow.
    mem, state, _, _, loco = loco_rig()
    child = mem.add()
    node = mem.add()
    plant(mem, child, {0x1C: V_1C_1})
    mem.link(state, 0x6C, child)
    mem.link(loco, 0x214, node)
    add(0x007B5460, mem, [(V_1C_1, 1, "1"), (0x007B1190, 1, "2")], ["3"])
    mem, state, *_ = rig()
    child = mem.add()
    plant(mem, child, {0x1C: V_1C_1})
    mem.link(state, 0x6C, child)
    add(0x007B5460, mem, [(V_1C_1, 1, "4"), (0x007B1190, 1, "5")], ["3"])

    # Wander, wander-in-place, panic, combine, charge, delete, evacuate.
    mem, *_ = rig()
    add(0x007B5630, mem, [(0x007B1190, 1, "1")], ["2"])
    mem, _, _, unit, loco = loco_rig()
    mem.w(unit, 0xB0, 1 << 0x13)
    add(0x007B5630, mem, [(0x007B1190, 1, "1"), (0x006A4F80, 2, "2"), (0x006A4FB0, 0, "3"),
                          (0x007527C0, 1, "4")], ["2"])
    mem, *_ = rig()
    add(0x007B56B0, mem, [(0x007B1190, 1, "8")], ["1"])
    mem, *_ = loco_rig()
    add(0x007B56B0, mem, [(0x007B1190, 1, "1"), (0x007527C0, 1, "2")], ["1"])
    mem, _, _, unit = rig()
    add(0x007B5830, mem, [(0x007B1190, 1, "1")], ["5"])
    mem, _, _, unit = rig()
    mem.w(unit, 0xB4, 1 << 14)
    add(0x007B5830, mem, [(0x006A4F80, 2, "1"), (0x006A4FB0, 0, "2"), (0x007B1190, 1, "3")], ["5"])
    mem, *_ = rig()
    add(0x007B5E30, mem, [(0x007A5B20, 0, "0"), (0x007B12D0, 0, "4")])
    mem, *_ = rig()
    add(0x007B5E30, mem, [(0x007A5B20, 0, "6")])
    mem, *_ = rig()
    add(0x007B60D0, mem, [], ["1"], eax=0xABC)
    mem, *_ = loco_rig()
    add(0x007B60D0, mem, [(0x004D9AB0, 1, "1"), (0x007527C0, 1, "2"), (0x007B1190, 1, "3")], ["8"])
    mem, *_ = rig()
    add(0x007B62D0, mem, [(0x005AA250, 0, "1"), (0x007B1190, 1, "2")], ["3"])
    mem, *_ = rig()
    add(0x007B62D0, mem, [(0x005AA250, 0, "0"), (0x007B1190, 1, "9")], ["0"])
    mem, *_ = loco_rig()
    add(0x007B62F0, mem, [(0x007B1190, 1, "1"), (0x006B7040, 2, "2"), (0x007527C0, 1, "3")], ["4"])
    mem, *_ = rig()
    add(0x007B62F0, mem, [(0x007B1190, 1, "1"), (0x007527C0, 1, "0")], ["4"])

    # Deploy / undeploy.
    mem, *_ = rig()
    add(0x007B64A0, mem, [])
    mem, _, _, unit, loco = loco_rig()
    plant(mem, loco, {0x1B0: V_1B0_0})
    mem.w(unit, 0x84, 1 << 30)
    logic = mem.add()
    mem.w(logic, 0x58, 100)
    add(0x007B64A0, mem, [(V_1B0_0, 0, "5")], gl=f"@{logic}")
    mem, _, _, unit, loco = loco_rig()
    plant(mem, loco, {0x1B0: V_1B0_0})
    logic = mem.add()
    mem.w(logic, 0x58, 7)
    add(0x007B64A0, mem, [(V_1B0_0, 0, "3"), (V_1B0_0, 0, "4"), (0x00681840, 0, "10"),
                          (0x004D9AF0, 1, "1"), (0x006D2020, 2, "2")], gl=f"@{logic}")
    mem, _, _, unit = rig()
    mem.w(unit, 0xB8, 1 << 9)
    mem.w(unit, 0xC0, 1 << 9)
    add(0x007B6530, mem, [(0x006A4F80, 2, "1"), (0x006A4FB0, 0, "2"), (0x006D2020, 2, "3"),
                          (0x006D2020, 2, "4")], ["1"])
    mem, *_ = rig()
    add(0x007B6530, mem, [(0x006A4F80, 2, "1"), (0x006A4FB0, 0, "2"), (0x006D2020, 2, "3"),
                          (0x006D2020, 2, "4")], ["1"])
    mem, *_ = rig()
    add(0x007B65B0, mem, [])
    mem, _, _, unit, loco = loco_rig()
    plant(mem, loco, {0x1B0: V_1B0_0})
    mem.w(unit, 0x88, 1 << 3)
    add(0x007B65B0, mem, [(V_1B0_0, 0, "1")])
    mem, _, _, unit, loco = loco_rig()
    plant(mem, loco, {0x1B0: V_1B0_0})
    mem.w(unit, 0x84, 1 << 30)
    logic = mem.add()
    mem.w(logic, 0x58, 5)
    add(0x007B65B0, mem, [(V_1B0_0, 0, "2"), (0x006D2020, 2, "3"), (0x006D2020, 2, "4"),
                          (V_1B0_0, 0, "6"), (0x006818C0, 0, "8"), (0x004D9AF0, 1, "1"),
                          (0x00757B40, 2, "9")], gl=f"@{logic}")
    mem, *_ = rig()
    add(0x007B6670, mem, [(0x004D2F98, 3, "0"), (0x004D2F98, 3, "0"), (0x006A5060, 2, "1"),
                          (0x006D2020, 2, "2"), (0x006D2020, 2, "3"), (0x006F0A20, 1, "4")],
        ["1"], capture=0x28)
    mem, *_ = rig()
    add(0x007B6670, mem, [(0x004D2F98, 3, "0"), (0x004D2F98, 3, "0"), (0x006A5060, 2, "5"),
                          (0x006D2020, 2, "6"), (0x006D2020, 2, "7"), (0x006F0A20, 1, "8")],
        ["2"], capture=0x28)

    # 007B66E0 xfer, early bit and the full walk.
    mem = Mem()
    machine, xfer = mem.add(), mem.add()
    mem.w(xfer, 4, 1 << 3)
    add(0x007B66E0, mem, [], [f"@{xfer}"], eax=0xAB00)
    mem = Mem()
    machine, xfer = mem.add(), mem.add()
    plant(mem, xfer, {0x18: V_18_1, 0x58: V_58_1, 0x14: V_14_2})
    add(0x007B66E0, mem, [(V_18_1, 1, "1"), (0x005C0A40, 1, "2"), (0x0054A090, 2, "3"),
                          (V_58_1, 1, "4"), (V_14_2, 2, "5")], [f"@{xfer}"])

    # 007B9820 register two states, then both allocations fail.
    mem = Mem()
    machine, first, second = mem.add(), mem.add(), mem.add()
    add(0x007B9820, mem, [(0x007B7B50, 0, "1"), (0x00416820, 1, f"@{first}"), (0x005A9F60, 2, "2"),
                          (0x005BDB90, 5, "3"), (0x00416820, 1, f"@{second}"), (0x005A9F60, 2, "4"),
                          (0x005BDB90, 5, "5")])
    mem = Mem()
    mem.add()
    add(0x007B9820, mem, [(0x007B7B50, 0, "1"), (0x00416820, 1, "0"), (0x005BDB90, 5, "2"),
                          (0x00416820, 1, "0"), (0x005BDB90, 5, "3")])

    # Land, return-for-landing, takeoff.
    mem, *_ = rig()
    add(0x007BB4A0, mem, [], ["1"], eax=0x55)
    mem, _, _, unit, loco = loco_rig()
    mem.w(unit, 0xB4, 1 << 6)
    add(0x007BB4A0, mem, [(0x007A9AB0, 1, "1"), (0x007527C0, 1, "2"), (0x006A4F80, 2, "3"),
                          (0x006A4FB0, 0, "4")], ["9"])
    mem, _, _, unit = rig()
    templ = mem.add()
    created = mem.add()
    mem.link(unit, 4, templ)
    mem.w(templ, 0xC8, 1 << 8)
    add(0x007BB8E0, mem, [(0x00416820, 1, f"@{created}"), (0x007BB2B0, 1, "1")])
    mem, _, _, unit = rig()
    templ = mem.add()
    mem.link(unit, 4, templ)
    add(0x007BB8E0, mem, [(0x00416820, 1, "0")])
    mem, *_ = rig()
    add(0x007BB930, mem, [], ["1"], eax=0x66)
    mem, _, _, unit, loco = loco_rig()
    info = mem.add()
    found = mem.add()
    plant(mem, found, {0x18: V_18_1})
    mem.link(loco, 4, info)
    mem.w(unit, 0xE4, 0x1234)
    mem.w(unit, 0xDC, 0x77)
    mem.w(unit, 0xB4, 1 << 5)
    add(0x007BB930, mem, [(0x006D2020, 2, "1"), (0x007A9AB0, 1, "2"), (0x006E4760, 2, f"@{found}"),
                          (V_18_1, 1, "3"), (0x006A4F80, 2, "4"), (0x006A4FB0, 0, "5")], ["6"])

    # Production-path factory and collect exit.
    mem, *_ = rig()
    created = mem.add()
    add(0x007BC320, mem, [(0x00416820, 1, f"@{created}"), (0x007AB5B0, 1, "1")])
    mem, *_ = rig()
    add(0x007BC320, mem, [(0x00416820, 1, "0")])
    mem, *_ = rig()
    add(0x007BC4C0, mem, [(0x005B9F30, 0, "0"), (0x007B1190, 1, "1")], ["4"])
    mem, _, machine, unit = rig()
    goal = mem.add()
    array = mem.add()
    left, right = mem.add(), mem.add()
    plant(mem, left, {0x64: V_64_1})
    plant(mem, right, {0x64: V_64_1})
    mem.link(array, 0, left)
    mem.link(array, 4, right)
    mem.w(unit, 0xDC, 0x42)
    loco = mem.add()
    mem.link(unit, 0x374, loco)
    add(0x007BC4C0, mem, [
        (0x005B9F30, 0, f"@{goal}"),
        (0x00748590, 1, f"0^0=@{array},4=@{array}+8"),
        (V_64_1, 1, "1"), (V_64_1, 1, "2"), (0x004166B0, 1, "3"),
        (0x007B1190, 1, "4"), (0x006B7040, 2, "5"),
    ], ["8"])

    # Internal move / dock / guard.
    mem, *_ = rig()
    add(0x007BD770, mem, [(0x005B9F30, 0, "0")])
    mem, *_ = rig()
    goal = mem.add()
    add(0x007BD770, mem, [(0x005B9F30, 0, f"@{goal}"), (0x007B12D0, 0, "1")])
    mem, *_ = rig()
    add(0x007BD7A0, mem, [(0x005B9F30, 0, "0"), (0x007B1190, 1, "1")], ["2"])
    mem, _, _, unit = rig()
    goal = mem.add()
    dock = mem.add()
    plant(mem, dock, {0x38: V_38_0, 0x20: V_20_1, 0x34: V_34_1})
    add(0x007BD7A0, mem, [(0x005B9F30, 0, f"@{goal}"), (0x00687810, 0, f"@{dock}"),
                          (V_38_0, 0, "1"), (V_20_1, 1, "2"), (0x007B1190, 1, "3")], ["4"])
    mem, *_ = rig()
    goal = mem.add()
    dock = mem.add()
    plant(mem, dock, {0x38: V_38_0, 0x34: V_34_1})
    add(0x007BD7A0, mem, [(0x005B9F30, 0, f"@{goal}"), (0x00687810, 0, f"@{dock}"),
                          (V_34_1, 1, "1"), (0x007B1190, 1, "2")], ["1"])
    mem, *_ = rig()
    add(0x007BD830, mem, [(0x005B9F30, 0, "0")])
    mem, *_ = rig()
    goal = mem.add()
    dock = mem.add()
    plant(mem, dock, {0x14: V_14_2})
    add(0x007BD830, mem, [(0x005B9F30, 0, f"@{goal}"), (0x00687810, 0, f"@{dock}"),
                          (V_14_2, 2, "1"), (0x007B12D0, 0, "2")])
    mem, *_ = rig()
    add(0x007BD880, mem, [(0x005B9F30, 0, "0"), (0x007B1190, 1, "1")], ["3"])
    mem, *_ = rig()
    goal = mem.add()
    dock = mem.add()
    plant(mem, dock, {0x38: V_38_0, 0x24: V_24_1, 0x34: V_34_1})
    add(0x007BD880, mem, [(0x005B9F30, 0, f"@{goal}"), (0x00687810, 0, f"@{dock}"),
                          (V_38_0, 0, "1"), (V_24_1, 1, "2"), (0x007B1190, 1, "3")], ["9"])
    mem, *_ = rig()
    add(0x007BD910, mem, [(0x005B9F30, 0, "0")])
    mem, *_ = rig()
    goal = mem.add()
    dock = mem.add()
    plant(mem, dock, {0x38: V_38_0, 0x18: V_18_2})
    add(0x007BD910, mem, [(0x005B9F30, 0, f"@{goal}"), (0x00687810, 0, f"@{dock}"),
                          (V_38_0, 0, "1"), (V_18_2, 2, "2"), (0x007B12D0, 0, "3")])
    mem, *_ = rig()
    add(0x007BDA40, mem, [(0x005B9F30, 0, "0")])
    mem, *_ = rig()
    goal = mem.add()
    dock = mem.add()
    plant(mem, dock, {0x1C: V_1C_2})
    add(0x007BDA40, mem, [(0x005B9F30, 0, f"@{goal}"), (0x00687810, 0, f"@{dock}"),
                          (V_1C_2, 2, "1"), (0x007B12D0, 0, "2")])
    mem, *_ = loco_rig()
    add(0x007BDA90, mem, [(0x005B9F30, 0, "0"), (0x005AA250, 0, "1"), (0x006E6360, 1, "2"),
                          (0x007B1190, 1, "3")], ["4"])
    mem, *_ = rig()
    goal = mem.add()
    dock = mem.add()
    plant(mem, dock, {0x2C: V_2C_1})
    add(0x007BDA90, mem, [(0x005B9F30, 0, f"@{goal}"), (0x00687810, 0, f"@{dock}"),
                          (V_2C_1, 1, "1"), (0x005AA250, 0, "2"), (0x007B1190, 1, "3")], ["5"])
    mem, *_ = rig()
    add(0x007BDEF0, mem, [(0x006A6210, 0, "1")])
    mem, *_ = rig()
    add(0x007BDEF0, mem, [(0x006A6210, 0, "0"), (0x007B12D0, 0, "2")])


def norm(value: int, bases: list[int], capture: int, read) -> str:
    for index, base in enumerate(bases):
        if base <= value < base + SIZE:
            return f"@{index}+{value - base:x}"
    if STACK_BASE <= value < STACK_BASE + STACK_SIZE:
        if capture:
            return "*" + read(value, capture).hex()
        return "ptr"
    return f"{value & 0xFFFFFFFF:08x}"


def canon(buf: bytes, bases: list[int]) -> bytes:
    out = bytearray(buf)
    for offset in range(0, len(out) - 3, 4):
        value = struct.unpack_from("<I", out, offset)[0]
        for index, base in enumerate(bases):
            if base <= value < base + SIZE:
                struct.pack_into("<I", out, offset, 0xE0000000 + index * 0x100000 + (value - base))
                break
    return bytes(out)


def canon_eax(value: int, bases: list[int]) -> str:
    for index, base in enumerate(bases):
        if base <= value < base + SIZE:
            return f"@{index}+{value - base:x}"
    return f"{value & 0xFFFFFFFF:08x}"


class Checker:
    def __init__(self, executable: Path) -> None:
        self.process = subprocess.Popen(
            [str(executable)], stdin=subprocess.PIPE, stdout=subprocess.PIPE, text=True, bufsize=1)

    def call(self, line: str) -> dict[str, str]:
        assert self.process.stdin is not None and self.process.stdout is not None
        self.process.stdin.write(line + "\n")
        self.process.stdin.flush()
        result = self.process.stdout.readline().strip()
        if not result or result.startswith("err="):
            raise RuntimeError(result or "checker closed")
        out = {}
        for token in result.split():
            key, value = token.split("=", 1)
            out[key] = value
        return out

    def close(self) -> None:
        assert self.process.stdin is not None
        self.process.stdin.write("quit\n")
        self.process.stdin.flush()
        if self.process.wait(timeout=10) != 0:
            raise RuntimeError("aistate7bcheck failed")


def resolve_token(token: str, bases: list[int]) -> tuple[int, list[tuple[int, int]]]:
    writes: list[tuple[int, int]] = []
    body = token
    if "^" in token:
        body, rest = token.split("^", 1)
        for part in rest.split(","):
            off, val = part.split("=")
            writes.append((int(off, 16), resolve_value(val, bases)))
    return resolve_value(body, bases), writes


def resolve_value(token: str, bases: list[int]) -> int:
    if token.startswith("@"):
        plus = token.find("+")
        head = token[1:] if plus < 0 else token[1:plus]
        value = bases[int(head)]
        if plus >= 0:
            value += int(token[plus + 1:], 16)
        return value
    return int(token, 16)


def line_for(case: dict) -> str:
    mem: Mem = case["mem"]
    words = [
        "run", f"{case['va']:08x}", f"{case['eax']:x}", f"{case['limit']:x}", f"{case['near']:x}",
        case["gl"], str(len(case["args"])), *case["args"], str(len(mem.blobs)),
    ]
    for blob in mem.blobs:
        words.append(blob.hex())
    words.append(str(len(mem.fix)))
    for index, offset, target in mem.fix:
        words.extend((str(index), f"{offset:x}", str(target)))
    words.append(str(len(case["rets"])))
    for _target, _argc, token in case["rets"]:
        words.append(token)
    words.append(str(case["capture"]))
    return " ".join(words)


def bind(emu: Emulator, box: dict) -> None:
    for target, cleanup in CLEANUP.items():
        def stub(active: Emulator, target=target) -> int:
            if not box["pending"]:
                raise RuntimeError(f"extra call 0x{target:08x}")
            spec = box["pending"].pop(0)
            if spec["target"] != target:
                raise RuntimeError(f"expected 0x{spec['target']:08x} got 0x{target:08x}")
            if target == 0x004D2F98:
                dest = struct.unpack("<I", active.read(active.register(uc.UC_X86_REG_ESP) + 4, 4))[0]
                value = struct.unpack("<I", active.read(active.register(uc.UC_X86_REG_ESP) + 8, 4))[0]
                count = struct.unpack("<I", active.read(active.register(uc.UC_X86_REG_ESP) + 12, 4))[0]
                active.write(dest, bytes([value & 0xFF]) * count)
            ecx = active.register(uc.UC_X86_REG_ECX)
            args = [struct.unpack("<I", active.read(
                active.register(uc.UC_X86_REG_ESP) + 4 + 4 * index, 4))[0] for index in range(spec["argc"])]
            text = f"{target:08x}:" + norm(ecx, box["bases"], box["capture"], active.read)
            text += ":" + "+".join(norm(arg, box["bases"], box["capture"], active.read) for arg in args)
            box["log"].append(text)
            if args:
                for offset, value in spec["writes"]:
                    active.write(args[0] + offset, struct.pack("<I", value))
            return spec["ret"]
        emu.bind_function(target, stub, cleanup)


def run_case(emu: Emulator, checker: Checker, case: dict, index: int) -> None:
    mem: Mem = case["mem"]
    emu.write(STACK_BASE + STACK_SIZE - 0x2000, b"\x00" * 0x2000)
    bases = [emu.alloc(bytes(blob), alignment=256) for blob in mem.blobs]
    for obj, offset, target in mem.fix:
        emu.write(bases[obj] + offset, struct.pack("<I", bases[target]))
    gl = 0 if case["gl"] == "0" else bases[int(case["gl"][1:])]
    emu.write(GAME_LOGIC, struct.pack("<I", gl))
    emu.write(LIMIT_ABS, struct.pack("<I", case["limit"]))
    emu.write(NEAR_ABS, struct.pack("<I", case["near"]))
    box = emu.box
    box["bases"] = bases
    box["capture"] = case["capture"]
    box["log"] = []
    box["pending"] = []
    for target, argc, token in case["rets"]:
        value, writes = resolve_token(token, bases)
        box["pending"].append({"target": target, "argc": argc, "ret": value, "writes": writes})
    stack = [resolve_value(token, bases) if token.startswith("@") else int(token, 16) for token in case["args"]]
    try:
        result = emu.call(case["va"], stack, eax=case["eax"], ecx=bases[0])
    except Exception as error:
        raise RuntimeError(f"case {index} 0x{case['va']:08x} emulate: {error}") from None
    if result.stack_cleanup != POPS.get(case["va"], 0):
        raise RuntimeError(
            f"case {index} 0x{case['va']:08x} stack cleanup {result.stack_cleanup} "
            f"!= {POPS.get(case['va'], 0)}")
    if box["pending"]:
        raise RuntimeError(f"case {index} 0x{case['va']:08x} unconsumed stubs")
    native_mem = [emu.read(base, SIZE) for base in bases]
    native_log = ";".join(box["log"]) if box["log"] else "."
    try:
        parsed = checker.call(line_for(case))
    except Exception as error:
        raise RuntimeError(f"case {index} 0x{case['va']:08x} checker: {error}") from None
    port_bases = [int(part, 16) for part in parsed["bases"].split(",") if part]
    port_eax = int(parsed["eax"], 16)
    mem_hex = parsed["mem"]
    width = SIZE * 2
    port_mem = [bytes.fromhex(mem_hex[i * width:(i + 1) * width]) for i in range(len(bases))]
    port_log = parsed["log"]
    problems = []
    if canon_eax(result.eax, bases) != canon_eax(port_eax, port_bases):
        problems.append(f"eax {canon_eax(result.eax, bases)} != {canon_eax(port_eax, port_bases)}")
    if native_log != port_log:
        problems.append(f"calls {native_log} != {port_log}")
    for obj, (native, port) in enumerate(zip(native_mem, port_mem)):
        left, right = canon(native, bases), canon(port, port_bases)
        if left != right:
            offset = next(i for i, (a, b) in enumerate(zip(left, right)) if a != b)
            problems.append(f"obj{obj}+0x{offset:x}")
            break
    if problems:
        raise RuntimeError(f"case {index} 0x{case['va']:08x} " + "; ".join(problems))


def main() -> int:
    build()
    counts: dict[int, int] = {}
    for case in CASES:
        counts[case["va"]] = counts.get(case["va"], 0) + 1
    missing = [f"{va:08x}" for va in COUNTED if counts.get(va, 0) < 2]
    extra = [f"{va:08x}" for va in counts if va not in COUNTED]
    if missing or extra or len(counts) != len(COUNTED):
        raise SystemExit(f"coverage {len(counts)} missing {missing} extra {extra}")
    executable = Path(sys.argv[1]) if len(sys.argv) > 1 else (
        Path(__file__).resolve().parents[3] / "build/win32/bin/Release/ra3_aistate7bcheck.exe")
    emu = Emulator()
    emu.box = {}
    bind(emu, emu.box)
    checker = Checker(executable)
    try:
        for index, case in enumerate(CASES):
            run_case(emu, checker, case, index)
    finally:
        checker.close()
    print(f"aistate7b oracle Release cases={len(CASES)} functions={len(counts)} mismatches=0")
    return 0


if __name__ == "__main__":
    sys.exit(main())
