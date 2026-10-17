"""Release oracle for the 0x00790000 AI-state methods.

Each case runs the shipped bytes in Unicorn and the C++ port in ra3_aistate79check
on the same object graph, stub returns and absolute floats. EAX, object bytes and
the normalized call trace must match. Stack-temporary pointers are logged as
`ptr`, or as eight captured bytes when the body publishes an initialized buffer.
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
LOGIC_TABLE = 0x00DFC4B4
LIMIT_ABS = 0x00BD95B8
MAX_ABS = 0x00BE700C
ONE_ABS = 0x00BD90A4
LIMIT = 0x43C80000
MAXF = 0x7F7FFFFF
ONE = 0x3F800000
EAX_IN = 0xA5A5A5A5

POPS = {
    0x00796F20: 4, 0x00796F50: 4, 0x007970A0: 4, 0x007971E0: 4, 0x00797470: 4,
    0x00797B00: 4, 0x00797D60: 4, 0x00798690: 4, 0x007989E0: 4, 0x00798DC0: 4,
    0x0079A0A0: 4, 0x0079C580: 4,
}
CLEANUP = {
    0x004EDD70: 4, 0x005A94C0: 0, 0x005AA1E0: 0, 0x005AA200: 0, 0x005AA250: 0,
    0x005B6230: 4, 0x005B9F30: 0, 0x005BA110: 4, 0x005C0840: 0, 0x005C08C0: 0,
    0x0067E340: 8, 0x0067E350: 4, 0x0067E3D0: 8, 0x0067E3E0: 4, 0x0067E860: 8,
    0x00682C20: 12, 0x00682C30: 8, 0x00682C40: 4, 0x006835B0: 0, 0x00686CB0: 0,
    0x00687C30: 0, 0x006A4EA0: 0, 0x006AEFE0: 0, 0x006B69E0: 8, 0x006C6D40: 0,
    0x006D2020: 8, 0x006D2840: 0, 0x006EFEB0: 0, 0x006F1990: 0, 0x006F6A20: 0,
    0x00713FD0: 0, 0x007525F0: 8, 0x007527C0: 4, 0x007985D0: 0, 0x0079CC50: 0,
    0x0079F330: 4, 0x009CCDA0: 4,
}


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


def frame_of(mem: Mem, frame: int) -> int:
    logic = mem.add()
    mem.w(logic, 0x58, frame)
    return logic


CASES: list[dict] = []


def add(va: int, mem: Mem, rets: list[tuple[int, int, str]], args: list[str] | None = None,
        eax: int = EAX_IN, gl: str = "0", table: str = "0", limit: int = LIMIT,
        maxf: int = MAXF, one: int = ONE, capture: bool = False) -> None:
    CASES.append({
        "va": va, "mem": mem, "rets": rets, "args": args or [], "eax": eax, "gl": gl,
        "table": table, "limit": limit, "maxf": maxf, "one": one, "capture": capture,
    })


def build() -> None:
    # 00796F20 / 00796F50 tolerance and flag stores.
    for value in (0x3F800000, 0):
        mem = Mem(); mem.add()
        add(0x00796F20, mem, [], [f"{value:x}"], eax=0xA5A5A5A5)
    for value, eax in ((0xAB, 0xA5A5A5A5), (0, 0x11223344)):
        mem = Mem(); mem.add()
        add(0x00796F50, mem, [], [f"{value:x}"], eax=eax)

    # 007970A0 parked onExit, 0079CC20 parked onEnter.
    mem, *_ = loco_rig()
    add(0x007970A0, mem, [(0x006F1990, 0, "1"), (0x009CCDA0, 1, "55")], ["77"])
    mem, *_ = loco_rig()
    add(0x007970A0, mem, [(0x006F1990, 0, "0"), (0x009CCDA0, 1, "0")], ["0"])
    for _ in range(2):
        mem, *_ = loco_rig()
        add(0x0079CC20, mem, [(0x006D2840, 0, "1"), (0x006A4EA0, 0, "2")])

    # 007971E0 best-bone onExit.
    for ret in ("0", "abcdef"):
        mem, *_ = rig()
        add(0x007971E0, mem, [(0x006D2020, 2, ret)], ["1"])

    # 00797380 update-node onEnter: missing goal, then a live goal.
    mem, state, machine, unit = rig()
    add(0x00797380, mem, [(0x005B9F30, 0, "0")])
    mem, state, machine, unit = rig()
    goal, node, slot = mem.add(), mem.add(), mem.add()
    mem.link(goal, 0x394, slot)
    mem.link(unit, 0x39C, node)
    mem.w(node, 0x1C, 0x11)
    mem.w(unit, 0xDC, 0x22)
    add(0x00797380, mem, [(0x005B9F30, 0, f"@{goal}"), (0x0067E860, 2, "1")])

    # 00797440 delivery update, 00797470 onExit, 007974D0 miner update.
    mem, *_, unit = rig(); node = mem.add(); mem.link(unit, 0x39C, node)
    add(0x00797440, mem, [(0x0067E350, 1, "1")])
    mem, *_, unit = rig(); node = mem.add(); mem.link(unit, 0x39C, node)
    add(0x00797440, mem, [(0x0067E350, 1, "100"), (0x006C6D40, 0, "9")])
    mem, *_ = rig()
    add(0x00797470, mem, [(0x006C6D40, 0, "3")], ["0"])
    mem, *_, unit = rig(); node = mem.add(); mem.link(unit, 0x39C, node)
    add(0x00797470, mem, [(0x0067E340, 2, "1"), (0x006C6D40, 0, "4")], ["8"])
    mem, *_, unit = rig(); node = mem.add(); mem.link(unit, 0x398, node)
    add(0x007974D0, mem, [(0x0067E3E0, 1, "0")])
    mem, *_, unit = rig(); node = mem.add(); mem.link(unit, 0x398, node)
    add(0x007974D0, mem, [(0x0067E3E0, 1, "1"), (0x0067E3D0, 2, "7")])

    # 00797960 / 007979A0 turret idle.
    mem, state, machine, _unit = rig()
    info = mem.add(); mem.link(machine, 0x80, info)
    mem.w(info, 0x4C, 3); mem.w(info, 0x50, 9)
    gl = frame_of(mem, 100)
    add(0x00797960, mem, [(0x005A94C0, 4, "5")], gl=f"@{gl}")
    mem, state, machine, _unit = rig()
    info = mem.add(); mem.link(machine, 0x80, info)
    mem.w(info, 0x4C, 1); mem.w(info, 0x50, 1)
    gl = frame_of(mem, 1)
    add(0x00797960, mem, [(0x005A94C0, 4, "0")], gl=f"@{gl}")
    for low, high, frame, deadline in ((0, 5, 10, 0), (4, 0, 10, 0), (4, 5, 3, 9), (4, 5, 9, 9), (4, 5, 12, 9)):
        mem, state, machine, _unit = rig()
        info = mem.add(); mem.link(machine, 0x80, info)
        mem.w(info, 0x4C, low); mem.w(info, 0x50, high)
        mem.w(state, 0x20, deadline)
        gl = frame_of(mem, frame)
        add(0x007979A0, mem, [], gl=f"@{gl}")

    # 00797AC0 / 00797B00 aim-fire.
    mem, *_rest, unit, loco = loco_rig(); mem.w(loco, 0x218, 0x0C)
    add(0x00797AC0, mem, [(0x007527C0, 1, "0"), (0x006A4EA0, 0, "1")])
    mem, *_rest, unit, loco = loco_rig(); mem.w(loco, 0x218, 0x22)
    add(0x00797AC0, mem, [(0x007527C0, 1, "1"), (0x006A4EA0, 0, "1")])
    mem, *_ = rig()
    add(0x00797B00, mem, [(0x006D2020, 2, "15")], ["0"])
    mem, *_rest, unit, loco = loco_rig(); mem.w(0, 0x20, 0xFFFFFFFF)
    add(0x00797B00, mem, [(0x006D2020, 2, "16")], ["1"])
    mem, *_rest, unit, loco = loco_rig(); mem.w(0, 0x20, 3)
    add(0x00797B00, mem, [(0x007527C0, 1, "1"), (0x006D2020, 2, "17")], ["2"])

    # 00797C50 / 00797C80 / 007981F0 attack slots.
    mem, state, _m, unit, loco = loco_rig(); goal = mem.add(); mem.w(state, 0x24, 0x44)
    add(0x00797C50, mem, [(0x005B9F30, 0, f"@{goal}"), (0x00682C20, 3, "21")])
    mem, state, _m, unit, loco = loco_rig(); mem.w(state, 0x24, 0)
    add(0x00797C50, mem, [(0x005B9F30, 0, "0"), (0x00682C20, 3, "0")])
    mem, *_ = loco_rig()
    add(0x00797C80, mem, [(0x00682C40, 1, "0")])
    mem, *_ = loco_rig()
    add(0x00797C80, mem, [(0x00682C40, 1, "10")])
    mem, *_ = loco_rig(); goal = mem.add()
    add(0x007981F0, mem, [(0x005B9F30, 0, f"@{goal}"), (0x00682C30, 2, "31")])
    mem, *_ = loco_rig()
    add(0x007981F0, mem, [(0x005B9F30, 0, "0"), (0x00682C30, 2, "0")])

    # 00797D60 create-path onExit. oneBits is the absolute at 0x00BD90A4.
    mem, *_ = rig()
    add(0x00797D60, mem, [], ["9"], eax=0xA5A5A5A5)
    mem, *_rest, loco = loco_rig()
    add(0x00797D60, mem, [], ["9"], one=ONE)
    mem, *_rest, loco = loco_rig()
    add(0x00797D60, mem, [], ["9"], one=0x40000000)

    # 00797DD0 delete, 007987C0 set locomotor, 007982D0 hold fire, 007983D0 idle.
    mem, *_ = rig(); gl = frame_of(mem, 0)
    add(0x00797DD0, mem, [(0x004EDD70, 1, "1")], gl=f"@{gl}")
    mem, *_ = rig(); gl = frame_of(mem, 8)
    add(0x00797DD0, mem, [(0x004EDD70, 1, "0")], gl=f"@{gl}")
    mem, state, *_ = loco_rig(); mem.w(state, 0x20, 6)
    add(0x007987C0, mem, [(0x007527C0, 1, "0")])
    mem, state, *_ = loco_rig(); mem.w(state, 0x20, 0)
    add(0x007987C0, mem, [(0x007527C0, 1, "1")])
    mem, *_ = rig(); add(0x007982D0, mem, [(0x006AEFE0, 3, "4")])
    mem, *_ = rig(); add(0x007982D0, mem, [(0x006AEFE0, 3, "0")])
    mem, *_ = rig(); add(0x007983D0, mem, [(0x006A4EA0, 0, "1")])
    mem, *_ = rig(); add(0x007983D0, mem, [(0x006A4EA0, 0, "0")])

    # 00798690 rotate onExit.
    mem, *_ = rig()
    add(0x00798690, mem, [], ["0"])
    mem, *_rest, loco = loco_rig(); mem.w(loco, 0x21C, 1)
    add(0x00798690, mem, [], ["0"])
    mem, *_rest, loco = loco_rig(); mem.w(loco, 0x21C, 3)
    add(0x00798690, mem, [], ["0"], maxf=0x42C80000)

    # 00798880 super slot 15, 007988F0 / 00798900 timed idle.
    mem = Mem(); state = mem.add()
    add(0x00798880, mem, [], eax=0xA5A5A5A5)
    mem = Mem(); state = mem.add(); nested = mem.add(); mem.link(state, 0x20, nested)
    add(0x00798880, mem, [(0x005AA1E0, 0, "42")])
    for value in (0x111, 0):
        mem = Mem(); state = mem.add(); mem.w(state, 0x20, value)
        add(0x007988F0, mem, [])
    for value in (5, 1, 0):
        mem = Mem(); state = mem.add(); mem.w(state, 0x24, value)
        add(0x00798900, mem, [])

    # 007989E0 occupants onExit.
    mem, *_ = rig()
    add(0x007989E0, mem, [(0x006D2020, 2, "1"), (0x009CCDA0, 1, "66")], ["88"])
    mem, *_ = rig()
    add(0x007989E0, mem, [(0x006D2020, 2, "0"), (0x009CCDA0, 1, "0")], ["0"])

    # 00798CF0 dock wait, 00798DC0 dock process.
    for stamp in (0, 0xFFFFFFFF, 40):
        mem = Mem(); mem.add(); gl = frame_of(mem, stamp)
        add(0x00798CF0, mem, [], gl=f"@{gl}")
    mem, *_ = rig()
    add(0x00798DC0, mem, [(0x005AA250, 0, "12")], ["3"])
    mem, *_ = rig()
    add(0x00798DC0, mem, [(0x005AA250, 0, "0")], ["0"])

    # 00798F00 retaliate predicate. Objects are 256-aligned so AL edits stay inside them.
    mem = Mem(); state = mem.add()
    add(0x00798F00, mem, [])
    mem = Mem(); state = mem.add(); child = mem.add(); mem.link(state, 4, child); mem.w(child, 4, 5)
    add(0x00798F00, mem, [])
    mem = Mem(); state = mem.add(); child = mem.add(); mem.link(state, 4, child); mem.w(child, 4, 0)
    add(0x00798F00, mem, [])

    # 00799890 AIIdle onEnter.
    mem, state, *_ = loco_rig(); mem.w(state, 0x20, 0xFFFFFFFF)
    # drop the locomotor link
    mem.fix = [item for item in mem.fix if item[1] != 0x374]
    add(0x00799890, mem, [])
    mem, state, *_rest, loco = loco_rig(); mem.w(state, 0x20, 0xFFFFFFFF)
    add(0x00799890, mem, [(0x006835B0, 2, "1111"), (0x005A94C0, 2, "abcd")])

    # 00799920 internal move slot 19.
    mem, state, machine, _u, loco = loco_rig()
    mem.b(machine, 0x44, 1); mem.b(loco, 0x428, 1)
    mem.w(state, 0x20, 0x10); mem.w(state, 0x24, 0x20); mem.w(state, 0x28, 0x30)
    add(0x00799920, mem, [(0x007525F0, 2, "1"), (0x00713FD0, 0, "11223300")])
    mem, state, machine, _u, loco = loco_rig()
    mem.b(machine, 0x44, 0); mem.b(loco, 0x428, 0)
    mem.w(state, 0x20, 0x10); mem.w(state, 0x24, 0x20); mem.w(state, 0x28, 0x30)
    gl = frame_of(mem, 77)
    add(0x00799920, mem, [(0x007525F0, 2, "1"), (0x00713FD0, 0, "99")], gl=f"@{gl}")

    # 0079A0A0 aim-fire onExit, 0079A0C0 slot 19.
    for ret in ("0", "1234"):
        mem, *_ = rig()
        add(0x0079A0A0, mem, [(0x006D2020, 2, ret)], ["4"])
    mem = Mem(); state = mem.add(); machine = mem.add(); mem.link(state, 0x18, machine)
    add(0x0079A0C0, mem, [])
    mem = Mem(); state = mem.add(); machine = mem.add(); mem.link(state, 0x18, machine)
    mem.b(state, 0x21, 1)
    add(0x0079A0C0, mem, [(0x005B9F30, 0, "99")])

    # 0079A3D0 / 0079A4F0 nested tail calls.
    mem = Mem(); mem.add()
    add(0x0079A3D0, mem, [], eax=0xA5A5A5A5)
    mem = Mem(); state = mem.add(); nested = mem.add(); mem.link(state, 0x24, nested)
    add(0x0079A3D0, mem, [(0x005AA200, 0, "7")])
    mem = Mem(); mem.add()
    add(0x0079A4F0, mem, [], eax=0x01020304)
    mem = Mem(); state = mem.add(); nested = mem.add(); mem.link(state, 0x20, nested)
    add(0x0079A4F0, mem, [(0x005AA200, 0, "8")])

    # 0079B470 hold-turret deadline.
    for frame, deadline in ((10, 20), (20, 20), (21, 20), (0, 0)):
        mem = Mem(); state = mem.add(); mem.w(state, 0x20, deadline); gl = frame_of(mem, frame)
        add(0x0079B470, mem, [], gl=f"@{gl}")

    # 0079C490 face slot 8.
    for value in (2, 0, 3):
        mem = Mem(); state = mem.add(); mem.w(state, 0x28, value)
        add(0x0079C490, mem, [])

    # 0079C520 / 0079C550 / 0079C580 turret machine.
    mem = Mem(); mem.add()
    add(0x0079C520, mem, [(0x005C0840, 0, "55")])
    mem = Mem(); state = mem.add(); link = mem.add(); mem.link(state, 0x80, link); gl = frame_of(mem, 15)
    add(0x0079C520, mem, [(0x005C0840, 0, "55")], gl=f"@{gl}")
    mem = Mem(); mem.add()
    add(0x0079C550, mem, [(0x005C08C0, 0, "66")])
    mem = Mem(); state = mem.add(); link = mem.add(); mem.link(state, 0x80, link); gl = frame_of(mem, 18)
    add(0x0079C550, mem, [(0x005C08C0, 0, "66")], gl=f"@{gl}")
    mem = Mem(); mem.add(); gl = frame_of(mem, 4)
    add(0x0079C580, mem, [(0x005B6230, 1, "2")], ["1"], gl=f"@{gl}")
    mem = Mem(); state = mem.add(); link = mem.add(); mem.link(state, 0x80, link); gl = frame_of(mem, 4)
    add(0x0079C580, mem, [(0x005B6230, 1, "2")], ["1"], gl=f"@{gl}")
    mem = Mem(); state = mem.add(); current = mem.add(); link = mem.add()
    mem.link(state, 4, current); mem.w(current, 4, 7); mem.link(state, 0x80, link)
    gl = frame_of(mem, 4)
    add(0x0079C580, mem, [(0x005B6230, 1, "2")], ["7"], gl=f"@{gl}")
    mem = Mem(); state = mem.add(); current = mem.add(); link = mem.add()
    mem.link(state, 4, current); mem.w(current, 4, 7); mem.link(state, 0x80, link)
    gl = frame_of(mem, 4)
    add(0x0079C580, mem, [(0x005B6230, 1, "2")], ["8"], gl=f"@{gl}")

    # 0079C8D0 far-enough.
    mem, *_ = rig()
    add(0x0079C8D0, mem, [(0x00686CB0, 0, "0")])
    mem, state, machine, unit = rig()
    probe, mid, leaf = mem.add(), mem.add(), mem.add()
    mem.link(probe, 4, mid); mem.link(mid, 4, leaf)
    mem.w(machine, 0x2C, bits(0)); mem.w(machine, 0x30, bits(0))
    mem.w(unit, 0x38, bits(0)); mem.w(unit, 0x3C, bits(0))
    add(0x0079C8D0, mem, [(0x00686CB0, 0, f"@{probe}")])
    mem, state, machine, unit = rig()
    probe, mid, leaf, radius = mem.add(), mem.add(), mem.add(), mem.add()
    mem.link(probe, 4, mid); mem.link(mid, 4, leaf); mem.link(leaf, 0x16C, radius)
    mem.w(radius, 4, bits(1))
    mem.w(machine, 0x2C, bits(10)); mem.w(machine, 0x30, bits(0))
    mem.w(unit, 0x38, bits(0)); mem.w(unit, 0x3C, bits(0))
    add(0x0079C8D0, mem, [(0x00686CB0, 0, f"@{probe}")])
    mem, state, machine, unit = rig()
    probe, mid, leaf, radius = mem.add(), mem.add(), mem.add(), mem.add()
    mem.link(probe, 4, mid); mem.link(mid, 4, leaf); mem.link(leaf, 0x16C, radius)
    mem.w(radius, 4, bits(100))
    mem.w(machine, 0x2C, bits(1)); mem.w(machine, 0x30, bits(0))
    mem.w(unit, 0x38, bits(0)); mem.w(unit, 0x3C, bits(0))
    add(0x0079C8D0, mem, [(0x00686CB0, 0, f"@{probe}")])

    # 0079CCF0 park orient.
    mem, _s, _m, unit = rig(); mem.b(unit, 0x471, 1)
    add(0x0079CCF0, mem, [])
    mem, *_ = rig(); mem.b(2, 0x471, 0)
    add(0x0079CCF0, mem, [(0x0079CC50, 0, "0")])
    mem, *_ = rig()
    add(0x0079CCF0, mem, [(0x0079CC50, 0, "1"), (0x007985D0, 0, "27")])

    # 0079CFA0 clearance bit 11.
    for flags in (0x800, 0, 0xFFFFF7FF, 0x1800):
        mem, _s, _m, unit = rig(); mem.w(unit, 0x94, flags)
        add(0x0079CFA0, mem, [])

    # 0079E0C0 skip-move. Close point calls the status helper; a far point does not.
    table = None
    mem, _s, _m, unit = rig()
    point = mem.add(); table = mem.add()
    mem.w(point, 0, bits(3)); mem.w(point, 4, bits(4))
    mem.w(unit, 0x38, bits(0)); mem.w(unit, 0x3C, bits(0))
    mem.w(table, 0x80, bits(50))
    add(0x0079E0C0, mem, [(0x005BA110, 1, f"@{point}"), (0x006D2020, 2, "1")], table=f"@{table}")
    mem, _s, _m, unit = rig()
    point = mem.add(); table = mem.add()
    mem.w(point, 0, bits(100)); mem.w(point, 4, bits(100))
    mem.w(unit, 0x38, bits(0)); mem.w(unit, 0x3C, bits(0))
    mem.w(table, 0x80, bits(50))
    add(0x0079E0C0, mem, [(0x005BA110, 1, f"@{point}")], table=f"@{table}")

    # 0079E150 can-enter.
    mem, _s, machine, _u = rig(); mem.fix = [item for item in mem.fix if not (item[0] == machine and item[1] == 0x24)]
    add(0x0079E150, mem, [(0x005B9F30, 0, "5")])
    mem, *_ = rig()
    add(0x0079E150, mem, [(0x005B9F30, 0, "0")])
    mem, *_ = rig(); goal = mem.add()
    add(0x0079E150, mem, [(0x005B9F30, 0, f"@{goal}"), (0x006EFEB0, 0, "1")])
    mem, *_ = rig(); goal = mem.add()
    add(0x0079E150, mem, [(0x005B9F30, 0, f"@{goal}"), (0x006EFEB0, 0, f"@{goal}")])

    # 0079E4D0 exact path. capture publishes the two floats passed to the helper.
    mem, state, *_ = loco_rig(); record = mem.add(); mem.link(state, 0x20, record)
    add(0x0079E4D0, mem, [])
    mem, state, *_rest, loco = loco_rig(); record = mem.add(); mem.link(state, 0x20, record)
    mem.w(record, 0x80, 0x1234); mem.w(record, 0x88, bits(1)); mem.w(record, 0x8C, bits(2))
    add(0x0079E4D0, mem, [(0x006B69E0, 2, "1")], capture=True)

    # 0079EA60 enter cover. Limit 400 is the absolute; a zero limit flips the near case.
    mem, _s, machine, unit = rig()
    mem.w(machine, 0x2C, bits(30)); mem.w(machine, 0x30, bits(30))
    mem.w(unit, 0x38, bits(0)); mem.w(unit, 0x3C, bits(0))
    add(0x0079EA60, mem, [])
    mem, _s, machine, unit = rig()
    mem.w(machine, 0x2C, bits(10)); mem.w(machine, 0x30, bits(10))
    mem.w(unit, 0x38, bits(0)); mem.w(unit, 0x3C, bits(0))
    add(0x0079EA60, mem, [(0x00687C30, 0, "0")])
    mem, _s, machine, unit = rig()
    mem.w(machine, 0x2C, bits(10)); mem.w(machine, 0x30, bits(10))
    mem.w(unit, 0x38, bits(0)); mem.w(unit, 0x3C, bits(0))
    cover = mem.add()
    add(0x0079EA60, mem, [(0x00687C30, 0, f"@{cover}"), (0x006F6A20, 0, "1")])
    mem, _s, machine, unit = rig()
    mem.w(machine, 0x2C, bits(1)); mem.w(machine, 0x30, bits(0))
    mem.w(unit, 0x38, bits(0)); mem.w(unit, 0x3C, bits(0))
    add(0x0079EA60, mem, [], limit=0)

    # 0079F3D0 garrisoned idle.
    mem, *_ = rig()
    add(0x0079F3D0, mem, [(0x006EFEB0, 0, "0")])
    mem, *_ = rig(); found = mem.add()
    add(0x0079F3D0, mem, [(0x006EFEB0, 0, f"@{found}"), (0x0079F330, 1, "1")])


def norm(value: int, bases: list[int], capture: bool, read) -> str:
    for index, base in enumerate(bases):
        if base <= value < base + SIZE:
            return f"@{index}+{value - base:x}"
    if STACK_BASE <= value < STACK_BASE + STACK_SIZE:
        if capture:
            return "*" + read(value, 8).hex()
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

    def call(self, line: str) -> str:
        assert self.process.stdin is not None and self.process.stdout is not None
        self.process.stdin.write(line + "\n")
        self.process.stdin.flush()
        result = self.process.stdout.readline().strip()
        if not result.startswith("OK "):
            raise RuntimeError(result or "checker closed")
        return result

    def close(self) -> None:
        assert self.process.stdin is not None
        self.process.stdin.write("quit\n")
        self.process.stdin.flush()
        if self.process.wait(timeout=10) != 0:
            raise RuntimeError("aistate79check failed")


def line_for(case: dict, bases: list[int] | None = None) -> str:
    mem: Mem = case["mem"]
    words = [
        "run", f"{case['va']:08x}", f"{case['eax']:x}", case["gl"], case["table"],
        f"{case['limit']:x}", f"{case['maxf']:x}", f"{case['one']:x}", str(len(case["args"])),
        *case["args"], str(len(mem.blobs)),
    ]
    for blob in mem.blobs:
        words.append(blob.hex())
    words.append(str(len(mem.fix)))
    for index, offset, target in mem.fix:
        words.extend((str(index), f"{offset:x}", str(target)))
    words.append(str(len(case["rets"])))
    for _target, _argc, token in case["rets"]:
        words.append(token)
    return " ".join(words)


def parse_ok(text: str, count: int) -> tuple[int, list[int], list[bytes], str]:
    head, mem_hex, log = (part.strip() for part in text.split("|"))
    parts = head.split()
    eax = int(parts[1], 16)
    bases = [int(part, 16) for part in parts[2:]]
    if len(bases) != count:
        raise RuntimeError(f"base count {len(bases)} != {count}")
    width = SIZE * 2
    if len(mem_hex) != width * count:
        raise RuntimeError(f"memory hex {len(mem_hex)} != {width * count}")
    mems = [bytes.fromhex(mem_hex[index * width:(index + 1) * width]) for index in range(count)]
    return eax, bases, mems, log.strip() or "-"


def bind(emu: Emulator, box: dict) -> None:
    for target, cleanup in CLEANUP.items():
        def stub(active: Emulator, target=target) -> int:
            if not box["pending"]:
                raise RuntimeError(f"extra call 0x{target:08x}")
            spec = box["pending"].pop(0)
            if spec["target"] != target:
                raise RuntimeError(f"expected 0x{spec['target']:08x} got 0x{target:08x}")
            ecx = active.register(uc.UC_X86_REG_ECX)
            args = [struct.unpack("<I", active.read(
                active.register(uc.UC_X86_REG_ESP) + 4 + 4 * index, 4))[0] for index in range(spec["argc"])]
            text = f"{target:08x}:" + norm(ecx, box["bases"], box["capture"], active.read)
            text += ":" + "+".join(norm(arg, box["bases"], box["capture"], active.read) for arg in args)
            box["log"].append(text)
            return spec["ret"]
        emu.bind_function(target, stub, cleanup)


def resolve_token(token: str, bases: list[int]) -> int:
    if token.startswith("@"):
        return bases[int(token[1:])]
    return int(token, 16)


def run_case(emu: Emulator, checker: Checker, case: dict, index: int) -> None:
    mem: Mem = case["mem"]
    bases = [emu.alloc(bytes(blob), alignment=256) for blob in mem.blobs]
    for obj, offset, target in mem.fix:
        emu.write(bases[obj] + offset, struct.pack("<I", bases[target]))
    gl = 0 if case["gl"] == "0" else bases[int(case["gl"][1:])]
    table = 0 if case["table"] == "0" else bases[int(case["table"][1:])]
    emu.write(GAME_LOGIC, struct.pack("<I", gl))
    emu.write(LOGIC_TABLE, struct.pack("<I", table))
    emu.write(LIMIT_ABS, struct.pack("<I", case["limit"]))
    emu.write(MAX_ABS, struct.pack("<I", case["maxf"]))
    emu.write(ONE_ABS, struct.pack("<I", case["one"]))
    box = emu.box
    box["bases"] = bases
    box["capture"] = case["capture"]
    box["log"] = []
    box["pending"] = [{
        "target": target,
        "argc": argc,
        "ret": resolve_token(token, bases),
    } for target, argc, token in case["rets"]]
    stack = [resolve_token(token, bases) if token.startswith("@") else int(token, 16) for token in case["args"]]
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
    native_log = ";".join(box["log"]) if box["log"] else "-"
    try:
        text = checker.call(line_for(case))
    except Exception as error:
        raise RuntimeError(f"case {index} 0x{case['va']:08x} checker: {error}") from None
    port_eax, port_bases, port_mem, port_log = parse_ok(text, len(bases))
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
    missing = [va for va, n in sorted(counts.items()) if n < 2]
    if missing or len(counts) != 48:
        raise SystemExit(f"coverage {len(counts)} missing-edges {missing}")
    executable = Path(sys.argv[1]) if len(sys.argv) > 1 else (
        Path(__file__).resolve().parents[3] / "build/win32/bin/Release/ra3_aistate79check.exe")
    emu = Emulator()
    box: dict = {}
    emu.box = box
    bind(emu, box)
    checker = Checker(executable)
    try:
        for index, case in enumerate(CASES):
            run_case(emu, checker, case, index)
    finally:
        checker.close()
    print(f"aistate79 oracle Release cases={len(CASES)} functions={len(counts)} mismatches=0")
    return 0


if __name__ == "__main__":
    sys.exit(main())
