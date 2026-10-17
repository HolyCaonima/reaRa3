"""Release oracle for the counted 0x007D0000 AI-state bodies.

Each case runs the shipped bytes in Unicorn and ra3_aistate7dcheck. EAX, object
bytes and the normalized call trace must match. Direct and virtual callees are
stubbed, so only the body under test is compared.
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
TIME_SCALE = 0x00DEEEE8
PARTITION = 0x00DFB9D8
SUBSYSTEM = 0x00DF03FC
REGISTRY = 0x00DFBD7C
EAX_IN = 0xA5A5A5A5

POPS = {
    0x007D6050: 4, 0x007D1A00: 4, 0x007DC680: 4, 0x007DA660: 4, 0x007D19C0: 4,
    0x007D0980: 12, 0x007D09B0: 8, 0x007D09E0: 4, 0x007D0A00: 4, 0x007DF320: 4,
    0x007D1D60: 4, 0x007DCAA0: 4, 0x007D2F90: 8,
}

# Bytes the callee pops. cdecl sites stay 0 because the body executes add esp.
CLEANUP = {
    0x004166B0: 0, 0x00416820: 0, 0x004D66A0: 4, 0x0054A090: 0, 0x0054BB10: 0,
    0x00549E10: 0, 0x0054F490: 4, 0x00550290: 0, 0x005559F0: 4, 0x005A9F60: 8,
    0x005AC4F0: 8, 0x005B6240: 4, 0x005B9F30: 0, 0x005BDC20: 20, 0x0063B510: 0,
    0x0063BF20: 8, 0x00639FB0: 0, 0x0063EB60: 0, 0x0065BF00: 0, 0x0067D700: 0,
    0x00682D70: 0, 0x00683320: 0, 0x006835B0: 0, 0x00686D90: 12, 0x00686FA0: 0,
    0x006873C0: 0, 0x006876F0: 0, 0x00687810: 0, 0x00687C90: 0, 0x00687CB0: 0,
    0x006996C0: 0, 0x006A1930: 20, 0x006A4F80: 8, 0x006A4FB0: 0, 0x006A51A0: 4,
    0x006A51C0: 4, 0x006A5270: 0, 0x006A6210: 0, 0x006A6510: 0, 0x006A6CB0: 4,
    0x006A7620: 8, 0x006A86C0: 0, 0x006C0F40: 4, 0x006C2300: 8, 0x006C50C0: 8,
    0x006C5680: 12, 0x006C5710: 12, 0x006C5BE0: 8, 0x006D2020: 8, 0x006E5CE0: 0,
    0x006EFE40: 0, 0x006F04C0: 0, 0x006F1350: 0, 0x006F1880: 4, 0x006F2100: 16,
    0x006F2140: 16, 0x00706820: 4, 0x00708900: 0, 0x0071D020: 0, 0x007445C0: 4,
    0x007483A0: 12, 0x00798BE0: 8, 0x00798D10: 0, 0x007A1EC0: 0, 0x007A7730: 4,
    0x007AA5B0: 8, 0x007AD2D0: 0, 0x007B12D0: 0, 0x007B1810: 0, 0x007BB9C0: 0,
    0x007BC640: 0, 0x007C08A0: 0, 0x007C6F80: 8, 0x007C70E0: 4, 0x007C7240: 4,
    0x007C7EF0: 0, 0x007C91E0: 4, 0x007CA0D0: 0, 0x007CD460: 0, 0x007CD890: 4,
    0x007CE950: 4, 0x007CF730: 0, 0x007CFC10: 4, 0x007D0A00: 4, 0x007D0EE0: 12,
    0x007D2120: 0, 0x007D2F90: 8, 0x007D3070: 8, 0x007D34C0: 0, 0x007D8D60: 0,
    0x007D99A0: 0, 0x007DA110: 0, 0x007DAD60: 0, 0x007DB090: 0, 0x007DB740: 0,
    0x007DBB20: 0, 0x007DC420: 8, 0x007DC910: 8, 0x007DCAA0: 4, 0x007DEE00: 0,
    0x007DF320: 4, 0x007E57C0: 0, 0x007EBDE0: 4, 0x007F0710: 0, 0x007F1050: 0,
    0x00879050: 0, 0x00685EB0: 0, 0x00685EC0: 0,
}

COUNTED = [
    0x007D0AD0, 0x007D6050, 0x007D6230, 0x007DC160, 0x007DB660, 0x007D19B0,
    0x007DABC0, 0x007DAD30, 0x007DC1B0, 0x007D0B30, 0x007DB6C0, 0x007DBF70,
    0x007D1A00, 0x007D1AA0, 0x007DC680, 0x007DA660, 0x007DBBC0, 0x007D19C0,
    0x007D0980, 0x007D09B0, 0x007D0AA0, 0x007D09E0, 0x007D0A00, 0x007DB530,
    0x007DB430, 0x007DE580, 0x007DFE70, 0x007D0D00, 0x007DBB20, 0x007DA4D0,
    0x007DA590, 0x007D6090, 0x007DF320, 0x007DFCF0, 0x007D1860, 0x007DFF50,
    0x007D1D60, 0x007DCAA0, 0x007D2F90, 0x007DB090,
]


def bits(value: float) -> int:
    return struct.unpack("<I", struct.pack("<f", value))[0]


class Mem:
    def __init__(self) -> None:
        self.blobs: list[bytearray] = []
        self.fix: list[tuple[int, int, int]] = []
        self.slots: list[tuple[int, int, int]] = []

    def add(self) -> int:
        self.blobs.append(bytearray(SIZE))
        return len(self.blobs) - 1

    def w(self, index: int, offset: int, value: int) -> None:
        struct.pack_into("<I", self.blobs[index], offset, value & 0xFFFFFFFF)

    def b(self, index: int, offset: int, value: int) -> None:
        self.blobs[index][offset] = value & 0xFF

    def h(self, index: int, offset: int, value: int) -> None:
        struct.pack_into("<H", self.blobs[index], offset, value & 0xFFFF)

    def link(self, index: int, offset: int, target: int) -> None:
        self.fix.append((index, offset, target))

    def slot(self, index: int, offset: int, cleanup: int) -> None:
        self.slots.append((index, offset, cleanup))


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


CASES: list[dict] = []


def add(va: int, mem: Mem, rets: list[tuple], args: list[str] | None = None,
        eax: int = EAX_IN, gl: str = "-", table: str = "-", scale: int = 0,
        part: str = "-", sub: str = "-", reg: str = "-") -> None:
    CASES.append({
        "va": va, "mem": mem, "rets": rets, "args": args or [], "eax": eax,
        "gl": gl, "table": table, "scale": scale, "part": part, "sub": sub, "reg": reg,
    })


def R(eax: str = "0", argc: int = 0, st: int | None = None, writes: str = "") -> tuple:
    return (eax, argc, st, writes)


def build() -> None:
    # 007D0AD0 locate-node consider.
    mem, _, _, unit = rig()
    add(0x007D0AD0, mem, [])
    mem, _, _, unit = rig()
    mem.w(unit, 0x38, bits(1.0))
    add(0x007D0AD0, mem, [R("1")])
    mem, _, _, unit = rig()
    mem.w(unit, 0x40, bits(1.0))
    add(0x007D0AD0, mem, [R("0"), R("2")])

    # 007D6050 aim slot 19. Both probe bytes must be set.
    mem, _, machine, _ = rig()
    mem.w(machine, 0x80, 0x10)
    add(0x007D6050, mem, [R("11223300", 2)], ["0"])
    mem, _, machine, _ = rig()
    mem.w(machine, 0x80, 0x10)
    add(0x007D6050, mem, [R("11223300", 2, writes="0+0b:1,1+0b:1")], ["7"])

    # 007D6230 fire update. A zero weapon field succeeds immediately.
    mem, state, _, _ = rig()
    weapon = mem.add()
    mem.link(state, 0x20, weapon)
    add(0x007D6230, mem, [])
    mem, state, _, _ = rig()
    weapon = mem.add()
    mem.link(state, 0x20, weapon)
    mem.w(weapon, 0x78, 1)
    add(0x007D6230, mem, [R("0", 2)])
    mem, state, _, _ = rig()
    weapon = mem.add()
    mem.link(state, 0x20, weapon)
    mem.w(weapon, 0x78, 1)
    add(0x007D6230, mem, [R("1", 2)])

    # 007DC160 boarding.
    mem, *_ = rig()
    add(0x007DC160, mem, [R("0", 3)])
    mem, _, _, _, _loco = loco_rig()
    add(0x007DC160, mem, [R("1", 3), R("0", 1)])

    # 007DB660 evacuate. The occupied bit fails before any call.
    mem, _, _, unit = rig()
    mem.b(unit, 0x471, 1)
    add(0x007DB660, mem, [])
    mem, *_ = loco_rig()
    add(0x007DB660, mem, [R("0")])
    mem, _, _, unit, loco = loco_rig()
    add(0x007DB660, mem, [R("3"), R("0", 2), R("4"), R("0")])

    # 007D19B0 aim update forwards a zero.
    mem, *_ = rig()
    add(0x007D19B0, mem, [R("0", 1)])
    mem, *_ = rig()
    add(0x007D19B0, mem, [R("ffffffff", 1)])

    # 007DABC0 player slot 26.
    mem = Mem(); state, vt = mem.add(), mem.add()
    mem.link(state, 0, vt); mem.slot(vt, 0x58, 0)
    add(0x007DABC0, mem, [R("100")])
    mem = Mem(); state, vt = mem.add(), mem.add()
    mem.link(state, 0, vt); mem.slot(vt, 0x58, 0)
    add(0x007DABC0, mem, [R("1"), R("9")])

    # 007DAD30 player slot 5.
    for final in ("0", "3"):
        mem = Mem(); state, vt = mem.add(), mem.add()
        mem.link(state, 0, vt)
        for off in (0x40, 0x44, 0x48, 0x4C):
            mem.slot(vt, off, 0)
        add(0x007DAD30, mem, [R("0"), R("0"), R("0"), R("0"), R(final)])

    # 007DC1B0 tunnel guard return.
    mem, *_ = rig()
    add(0x007DC1B0, mem, [R("0"), R("0"), R("0")])
    mem, *_ = rig()
    add(0x007DC1B0, mem, [R("0"), R("0"), R("6")])

    # 007D0B30 dock. A missing goal fails.
    mem, *_ = rig()
    add(0x007D0B30, mem, [R("0")])
    mem, *_ = rig()
    goal = mem.add(); mem.b(goal, 0x80, 1)
    add(0x007D0B30, mem, [R(f"@{goal}")])

    # 007DB6C0 follow-waypoint evacuate.
    mem, *_ = loco_rig()
    add(0x007DB6C0, mem, [R("0")])
    mem, *_ = loco_rig()
    add(0x007DB6C0, mem, [R("ffffffff"), R("0", 3)])

    # 007DBF70 move-to evacuate. The virtual argument is the caller's EBP (0).
    mem, *_ = loco_rig()
    add(0x007DBF70, mem, [R("0")])
    mem = Mem(); state, machine, unit, loco, vt = mem.add(), mem.add(), mem.add(), mem.add(), mem.add()
    mem.link(state, 0x18, machine); mem.link(machine, 0x24, unit); mem.link(unit, 0x374, loco)
    mem.link(state, 0, vt); mem.slot(vt, 0x50, 0)
    add(0x007DBF70, mem, [R("ffffffff"), R("4"), R("0", 3)])

    # 007D1A00 tunnel xfer.
    mem, state, _, _ = rig(); xfer, vt = mem.add(), mem.add()
    mem.link(xfer, 0, vt); mem.w(xfer, 4, 8)
    add(0x007D1A00, mem, [], [f"@{xfer}"], eax=0xA5A5A500)
    mem, state, _, _ = rig(); xfer, vt = mem.add(), mem.add()
    mem.link(xfer, 0, vt); mem.slot(vt, 0x18, 4); mem.slot(vt, 0x84, 4)
    add(0x007D1A00, mem, [R("0", 1), R("42", 1)], [f"@{xfer}"])

    # 007D1AA0 tunnel onEnter. A failing probe returns before the last virtual.
    mem, _, _, _, loco = loco_rig()
    created, lvt, ovt = mem.add(), mem.add(), mem.add()
    mem.link(loco, 0, lvt); mem.link(created, 0, ovt)
    mem.slot(lvt, 0x224, 0); mem.slot(lvt, 0x244, 0); mem.slot(ovt, 0x14, 0)
    vec = mem.add()
    add(0x007D1AA0, mem, [R(f"@{created}", 1), R(f"@{created}", 1), R(f"@{vec}"), R("0"), R("fffffffe")])
    mem, _, _, _, loco = loco_rig()
    created, lvt, ovt = mem.add(), mem.add(), mem.add()
    mem.link(loco, 0, lvt); mem.link(created, 0, ovt)
    mem.slot(lvt, 0x224, 0); mem.slot(lvt, 0x244, 0)
    mem.slot(ovt, 0x14, 0); mem.slot(ovt, 0x1C, 4)
    vec = mem.add()
    add(0x007D1AA0, mem, [R(f"@{created}", 1), R(f"@{created}", 1), R(f"@{vec}"), R("1"), R("0"), R("3", 1)])

    # 007DC680 squad xfer.
    mem = Mem(); state, xfer = mem.add(), mem.add()
    mem.w(xfer, 4, 8)
    add(0x007DC680, mem, [], [f"@{xfer}"])
    mem = Mem(); state, xfer, vt = mem.add(), mem.add(), mem.add()
    mem.link(xfer, 0, vt); mem.slot(vt, 0x18, 4); mem.slot(vt, 0x84, 4)
    add(0x007DC680, mem, [R("1", 1), R("2", 2), R("3", 1), R("4", 1)], [f"@{xfer}"])

    # 007DA660 work-order xfer. Mode bit replaces only AL.
    mem = Mem(); state, xfer = mem.add(), mem.add()
    mem.w(xfer, 4, 8)
    add(0x007DA660, mem, [], [f"@{xfer}"], eax=0xA5A5A500)
    mem = Mem(); state, xfer, vt = mem.add(), mem.add(), mem.add()
    mem.link(xfer, 0, vt)
    for off, clean in ((0x18, 4), (0x70, 4), (0x84, 4), (0x6C, 4)):
        mem.slot(vt, off, clean)
    add(0x007DA660, mem, [
        R("0", 1), R("0", 2), R("0", 2), R("0", 1), R("0", 1), R("0", 1), R("0", 1),
        R("0", 1), R("0", 2), R("0", 1), R("0", 1), R("0", 1), R("8", 1),
    ], [f"@{xfer}"])

    # 007DBBC0 charge. No locomotor fails after the goal lookup.
    mem, *_ = rig()
    add(0x007DBBC0, mem, [R("0")])
    mem, _, _, unit, _loco = loco_rig()
    goal = mem.add()
    mem.w(goal, 0x38, 1); mem.w(goal, 0x3C, 2); mem.w(goal, 0x40, 3)
    add(0x007DBBC0, mem, [R(f"@{goal}"), R("5")])

    # Factories. Allocation failure returns 0; success forwards the constructor.
    mem = Mem(); mem.add()
    add(0x007D19C0, mem, [R("0", 1)], ["11"])
    mem = Mem(); machine, created = mem.add(), mem.add()
    mem.w(machine, 0x24, 0x55)
    add(0x007D19C0, mem, [R(f"@{created}", 1), R("77", 3)], ["11"])
    mem = Mem(); mem.add()
    add(0x007D0980, mem, [R("0", 1)], ["1", "2", "3"])
    mem = Mem(); machine, created = mem.add(), mem.add()
    add(0x007D0980, mem, [R(f"@{created}", 1), R(f"@{created}", 2)], ["1", "2", "3"])
    mem = Mem(); mem.add()
    add(0x007D09B0, mem, [R("0", 1)], ["8", "9"])
    mem = Mem(); machine, created = mem.add(), mem.add()
    add(0x007D09B0, mem, [R(f"@{created}", 1), R("6", 1)], ["8", "9"])
    mem, *_ = rig()
    add(0x007D0AA0, mem, [R("0", 1)])
    mem, _, machine, unit = rig(); created = mem.add()
    add(0x007D0AA0, mem, [R(f"@{created}", 1), R("44", 1)])
    mem = Mem(); mem.add()
    add(0x007D09E0, mem, [R("0", 1)], ["12"])
    mem = Mem(); machine, created = mem.add(), mem.add()
    add(0x007D09E0, mem, [R(f"@{created}", 1), R(f"@{created}", 1)], ["12"])

    # 007D0A00 constructor.
    mem = Mem(); self_i, unit = mem.add(), mem.add()
    add(0x007D0A00, mem, [R("0", 1), R("0", 1), R("0", 5)], [f"@{unit}"])
    mem = Mem(); self_i, unit, created = mem.add(), mem.add(), mem.add()
    add(0x007D0A00, mem, [R("0", 1), R(f"@{created}", 1), R("0", 2), R("0", 5)], [f"@{unit}"])

    # 007DB530 back away. The CE bit skips the flag update; a null goal succeeds.
    mem, _, _, unit = rig()
    mem.b(unit, 0xCE, 1)
    add(0x007DB530, mem, [R("0")])
    mem, _, _, unit = rig()
    add(0x007DB530, mem, [R("0", 2), R("0"), R("0")])

    # 007DB430 cower.
    mem, *_ = rig()
    add(0x007DB430, mem, [R("0")])
    mem, *_ = rig(); goal, record = mem.add(), mem.add()
    mem.link(goal, 4, record); mem.b(goal, 0x471, 1)
    add(0x007DB430, mem, [R(f"@{goal}")])
    mem, state, _, unit = rig(); goal, vt = mem.add(), mem.add()
    mem.link(state, 0, vt); mem.slot(vt, 0x4C, 0)
    add(0x007DB430, mem, [R(f"@{goal}"), R("0")])

    # 007DE580 wide face. Null goal fails.
    mem, *_ = rig()
    add(0x007DE580, mem, [R("0")])
    mem, *_ = rig(); goal = mem.add(); mem.b(goal, 0x471, 1)
    add(0x007DE580, mem, [R(f"@{goal}")])

    # 007DFE70 squad onEnter.
    mem = Mem()
    state, machine = mem.add(), mem.add()
    mem.link(state, 0x18, machine)
    add(0x007DFE70, mem, [])
    mem = Mem()
    state, machine, unit = mem.add(), mem.add(), mem.add()
    vt, chosen, cvt = mem.add(), mem.add(), mem.add()
    mem.link(state, 0x18, machine); mem.link(machine, 0x24, unit)
    mem.link(machine, 0, vt); mem.link(chosen, 0, cvt)
    mem.slot(vt, 0x20, 4); mem.slot(cvt, 0x14, 0)
    add(0x007DFE70, mem, [R(f"@{chosen}", 1), R("0"), R("0", 1), R("0"), R("11")])

    # 007D0D00 guard return. Equal positions are inside the 400 limit.
    mem, _, _, unit, loco = loco_rig()
    lvt, vec = mem.add(), mem.add()
    mem.link(loco, 0, lvt); mem.slot(lvt, 0x224, 0)
    add(0x007D0D00, mem, [R(f"@{vec}")])
    mem, *_ = rig()
    add(0x007D0D00, mem, [R("1")])

    # 007DBB20 face. A zero angle succeeds; a missing goal fails.
    mem, *_ = loco_rig()
    add(0x007DBB20, mem, [R("0", 1, st=0)])
    mem, state, _, _, _ = loco_rig()
    mem.w(state, 0x28, 1)
    add(0x007DBB20, mem, [R("0")])

    # 007DA4D0 / 007DA590 countdown.
    mem = Mem(); state, record = mem.add(), mem.add()
    mem.link(state, 4, record)
    add(0x007DA4D0, mem, [])
    mem = Mem(); state, record = mem.add(), mem.add()
    mem.link(state, 4, record); mem.b(record, 0x105, 1); mem.w(state, 0x28, 5); mem.w(state, 0x30, 1)
    add(0x007DA4D0, mem, [])
    mem = Mem(); state, record = mem.add(), mem.add()
    mem.link(state, 4, record)
    add(0x007DA590, mem, [])
    mem = Mem(); state, record, vt = mem.add(), mem.add(), mem.add()
    mem.link(state, 4, record); mem.link(state, 0, vt); mem.slot(vt, 0x68, 0)
    mem.b(record, 0x104, 1); mem.b(state, 0x20, 1); mem.w(state, 0x34, 1)
    add(0x007DA590, mem, [R("0"), R("0")])

    # 007D6090 aim update stops when the probe mode is 0.
    mem, _, machine, _ = rig(); turret = mem.add()
    mem.w(machine, 0x80, 0)
    mem.link(machine, 0x80, turret)
    add(0x007D6090, mem, [R("0", 2)])
    mem, state, machine, _ = rig()
    turret, tune, vt = mem.add(), mem.add(), mem.add()
    mem.link(machine, 0x80, turret)
    mem.link(turret, 0x58, tune)
    mem.link(state, 0, vt)
    mem.slot(vt, 0x4C, 4)
    add(0x007D6090, mem, [R("3", 2), R("0", 1), R("0", 1)])

    # 007DF320 forwards the two dwords at the argument.
    mem = Mem(); state, arg = mem.add(), mem.add()
    mem.w(state, 0x0C, 0x21); mem.w(arg, 0, 3); mem.w(arg, 4, 4)
    add(0x007DF320, mem, [R("9", 2)], [f"@{arg}"])
    mem = Mem(); state, arg = mem.add(), mem.add()
    mem.w(state, 0x0C, 0x21)
    add(0x007DF320, mem, [R("0", 2)], [f"@{arg}"])

    # 007DFCF0 pick target.
    mem = Mem(); state, machine, unit, vt = mem.add(), mem.add(), mem.add(), mem.add()
    mem.link(state, 0x18, machine); mem.link(machine, 0x24, unit); mem.link(machine, 0, vt)
    mem.slot(vt, 0x44, 0)
    add(0x007DFCF0, mem, [R("0")])
    mem = Mem(); state, machine, unit, vt = mem.add(), mem.add(), mem.add(), mem.add()
    mem.link(state, 0x18, machine); mem.link(machine, 0x24, unit); mem.link(machine, 0, vt)
    mem.slot(vt, 0x44, 0); mem.slot(vt, 0x38, 4)
    add(0x007DFCF0, mem, [R("20"), R("0", 2), R("0", 1), R("0", 1)])

    # 007D1860 aim onEnter reaches the continue call when the slot is empty.
    mem, *_ = loco_rig()
    add(0x007D1860, mem, [R("0", 1), R("0"), R("0"), R("0", 3), R("0")])
    mem, *_ = loco_rig()
    add(0x007D1860, mem, [R("0", 1), R("0"), R("5"), R("0", 3), R("0")])

    # 007DFF50 squad update.
    mem, state, _, _ = rig()
    add(0x007DFF50, mem, [])
    mem, *_ = loco_rig()
    mem.w(0, 0x20, 1)
    add(0x007DFF50, mem, [R("0")])

    # 007D1D60 turret xfer.
    mem = Mem(); state, xfer = mem.add(), mem.add()
    mem.w(xfer, 4, 8)
    add(0x007D1D60, mem, [], [f"@{xfer}"])
    mem = Mem(); state, xfer, vt = mem.add(), mem.add(), mem.add()
    mem.link(xfer, 0, vt)
    for off in (0x18, 0x20, 0x64, 0x6C, 0x84, 0x70):
        mem.slot(vt, off, 4)
    add(0x007D1D60, mem, [R("0", 1)] * 5 + [R("0", 2)] + [R("0", 1)] * 12 + [R("0", 1)], [f"@{xfer}"])

    # 007DCAA0 bind target.
    mem = Mem(); mem.add()
    add(0x007DCAA0, mem, [], ["0"])
    mem = Mem(); state, target = mem.add(), mem.add()
    mem.w(state, 0x28, 5); mem.w(target, 0xDC, 5)
    add(0x007DCAA0, mem, [], [f"@{target}"])
    mem = Mem(); state, target = mem.add(), mem.add()
    mem.w(target, 0xDC, 9)
    add(0x007DCAA0, mem, [R("7", 2)], [f"@{target}"])

    # 007D2F90 aim probe.
    mem = Mem(); turret, out_id, out_vec = mem.add(), mem.add(), mem.add()
    add(0x007D2F90, mem, [], [f"@{out_id}", f"@{out_vec}"])
    mem = Mem(); turret, out_id, out_vec, source = mem.add(), mem.add(), mem.add(), mem.add()
    mem.w(turret, 0x78, 2); mem.link(turret, 0x60, source)
    mem.w(source, 0x2C, 4); mem.w(source, 0x30, 5); mem.w(source, 0x34, 6)
    add(0x007D2F90, mem, [], [f"@{out_id}", f"@{out_vec}"])

    # 007DB090 idle. Bit 29 skips the countdown; the sum is the base call.
    mem, _, _, unit, loco = loco_rig()
    mem.w(unit, 0x84, 1 << 29);     mem.h(0, 0x20, 5)
    add(0x007DB090, mem, [R("3"), R("4")])
    mem, state, _, unit, loco = loco_rig()
    mem.h(state, 0x22, 1)
    add(0x007DB090, mem, [R("0"), R("0"), R("1")])


def norm(value: int, bases: list[int]) -> str:
    for index, base in enumerate(bases):
        if base <= value < base + SIZE:
            return f"@{index}+{value - base:x}"
    if STACK_BASE <= value < STACK_BASE + STACK_SIZE:
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


def push_fpu(emu: Emulator, bits_value: int) -> None:
    value = struct.unpack("<f", struct.pack("<I", bits_value & 0xFFFFFFFF))[0]
    fpsw = emu.uc.reg_read(uc.UC_X86_REG_FPSW)
    top = (fpsw >> 11) & 7
    new_top = (top - 1) & 7
    emu.uc.reg_write(uc.UC_X86_REG_FPSW, (fpsw & ~(7 << 11)) | (new_top << 11))
    from emulate import FPU_REGISTERS
    if value == 0.0:
        emu.uc.reg_write(FPU_REGISTERS[new_top], (0, 0))
        return
    sign = 1 if value < 0 else 0
    import math
    mantissa_float, exponent = math.frexp(abs(value))
    mantissa = int(round(mantissa_float * (1 << 64)))
    if mantissa >= (1 << 64):
        mantissa >>= 1
        exponent += 1
    exp15 = (exponent - 1 + 16383) & 0x7FFF
    emu.uc.reg_write(FPU_REGISTERS[new_top], (mantissa, (sign << 15) | exp15))


def apply_writes(active: Emulator, args: list[int], writes: str) -> None:
    if not writes:
        return
    for piece in writes.split(","):
        plus = piece.find("+")
        colon = piece.find(":")
        arg = int(piece[:plus])
        off_text = piece[plus + 1:colon]
        size = 4
        if off_text.endswith("b"):
            size = 1
            off_text = off_text[:-1]
        offset = int(off_text, 16)
        value = int(piece[colon + 1:], 16)
        active.write(args[arg] + offset, value.to_bytes(size, "little"))


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
        if self.process.stdin is None:
            return
        self.process.stdin.write("quit\n")
        self.process.stdin.flush()
        self.process.wait(timeout=10)


def line_for(case: dict) -> str:
    mem: Mem = case["mem"]
    words = [
        "run", f"{case['va']:08x}", f"{case['eax']:x}", case["gl"], case["table"],
        f"{case['scale']:x}", case["part"], case["sub"], case["reg"],
        str(len(case["args"])), *case["args"], str(len(mem.blobs)),
    ]
    for blob in mem.blobs:
        words.append(blob.hex())
    words.append(str(len(mem.fix)))
    for index, offset, target in mem.fix:
        words.extend((str(index), f"{offset:x}", str(target)))
    words.append(str(len(case["rets"])))
    for eax, _argc, st, writes in case["rets"]:
        token = eax
        if st is not None:
            token += f"~{st:08x}"
        if writes:
            token += "*" + writes
        words.append(token)
    return " ".join(words)


def parse_ok(text: str, count: int) -> tuple[int, list[int], list[bytes], str]:
    head, mem_hex, log = (part.strip() for part in text.split("|"))
    parts = head.split()
    eax = int(parts[1], 16)
    bases = [int(part, 16) for part in parts[2:]]
    width = SIZE * 2
    mems = [bytes.fromhex(mem_hex[index * width:(index + 1) * width]) for index in range(count)]
    return eax, bases, mems, log.strip() or "-"


def resolve_token(token: str, bases: list[int]) -> int:
    if token.startswith("@"):
        plus = token.find("+")
        base = bases[int(token[1:] if plus < 0 else token[1:plus])]
        return base if plus < 0 else base + int(token[plus + 1:], 16)
    return int(token, 16)


def run_case(emu: Emulator, checker: Checker, case: dict, index: int, stubs: dict[int, int]) -> None:
    mem: Mem = case["mem"]
    for obj, offset, cleanup in mem.slots:
        mem.w(obj, offset, stubs[cleanup])
    bases = [emu.alloc(bytes(blob), alignment=256) for blob in mem.blobs]
    for obj, offset, target in mem.fix:
        emu.write(bases[obj] + offset, struct.pack("<I", bases[target]))
    pointers = {
        "gl": case["gl"], "table": case["table"], "part": case["part"],
        "sub": case["sub"], "reg": case["reg"],
    }
    resolved = {name: 0 if token == "-" else resolve_token(token, bases) for name, token in pointers.items()}
    emu.write(GAME_LOGIC, struct.pack("<I", resolved["gl"]))
    emu.write(LOGIC_TABLE, struct.pack("<I", resolved["table"]))
    emu.write(TIME_SCALE, struct.pack("<I", case["scale"]))
    emu.write(PARTITION, struct.pack("<I", resolved["part"]))
    emu.write(SUBSYSTEM, struct.pack("<I", resolved["sub"]))
    emu.write(REGISTRY, struct.pack("<I", resolved["reg"]))
    box = emu.box
    box["bases"] = bases
    box["log"] = []
    box["pending"] = [{
        "eax": resolve_token(eax, bases),
        "argc": argc,
        "st": st,
        "writes": writes,
    } for eax, argc, st, writes in case["rets"]]
    stack = [resolve_token(token, bases) if token.startswith("@") else int(token, 16) for token in case["args"]]
    saved = emu._function_stubs.pop(case["va"], None)
    emu.uc.reg_write(uc.UC_X86_REG_FPCW, 0x027F)
    emu.uc.reg_write(uc.UC_X86_REG_FPSW, 0)
    try:
        result = emu.call(case["va"], stack, eax=case["eax"], ecx=bases[0])
    except Exception as error:
        raise RuntimeError(f"case {index} 0x{case['va']:08x} emulate: {error}") from None
    finally:
        if saved is not None:
            emu._function_stubs[case["va"]] = saved
    if result.stack_cleanup != POPS.get(case["va"], 0):
        raise RuntimeError(
            f"case {index} 0x{case['va']:08x} stack cleanup {result.stack_cleanup} "
            f"!= {POPS.get(case['va'], 0)}")
    if box["pending"]:
        raise RuntimeError(f"case {index} 0x{case['va']:08x} unconsumed stubs {len(box['pending'])}")
    native_mem = [emu.read(base, SIZE) for base in bases]
    native_log = ";".join(box["log"]) if box["log"] else "-"
    text = checker.call(line_for(case))
    port_eax, port_bases, port_mem, port_log = parse_ok(text, len(bases))
    problems = []
    if canon_eax(result.eax, bases) != canon_eax(port_eax, port_bases):
        problems.append(f"eax {canon_eax(result.eax, bases)} != {canon_eax(port_eax, port_bases)}")
    if native_log != port_log:
        problems.append(f"calls\n  native {native_log}\n  port   {port_log}")
    for obj, (native, port) in enumerate(zip(native_mem, port_mem)):
        left, right = canon(native, bases), canon(port, port_bases)
        if left != right:
            offset = next(i for i, (a, b) in enumerate(zip(left, right)) if a != b)
            problems.append(f"obj{obj}+0x{offset:x} {left[offset]:02x}!={right[offset]:02x}")
            break
    if problems:
        raise RuntimeError(f"case {index} 0x{case['va']:08x} " + "; ".join(problems))
    emu.reset_data()


def main() -> int:
    build()
    only = int(sys.argv[2], 16) if len(sys.argv) > 2 else None
    counts: dict[int, int] = {}
    for case in CASES:
        counts[case["va"]] = counts.get(case["va"], 0) + 1
    missing = [f"{va:08x}" for va in COUNTED if counts.get(va, 0) < 2]
    extra = [f"{va:08x}" for va in counts if va not in COUNTED]
    if (missing or extra or len(counts) != len(COUNTED)) and only is None:
        raise SystemExit(f"coverage {len(counts)} missing {missing} extra {extra}")
    executable = Path(sys.argv[1]) if len(sys.argv) > 1 else (
        Path(__file__).resolve().parents[3] / "build/win32/bin/Release/ra3_aistate7dcheck.exe")
    emu = Emulator()
    box: dict = {}
    emu.box = box

    def consume(active: Emulator) -> int:
        if not box["pending"]:
            eip = active.register(uc.UC_X86_REG_EIP)
            raise RuntimeError(f"extra call 0x{eip:08x}")
        spec = box["pending"].pop(0)
        called = active.register(uc.UC_X86_REG_EIP)
        ecx = active.register(uc.UC_X86_REG_ECX)
        args = [struct.unpack("<I", active.read(
            active.register(uc.UC_X86_REG_ESP) + 4 + 4 * slot, 4))[0] for slot in range(spec["argc"])]
        text = f"{called:08x}:" + norm(ecx, box["bases"])
        text += ":" + "+".join(norm(arg, box["bases"]) for arg in args)
        box["log"].append(text)
        apply_writes(active, args, spec["writes"])
        if spec["st"] is not None:
            push_fpu(active, spec["st"])
        return spec["eax"]

    stubs: dict[int, int] = {}
    for cleanup in (0, 4, 8, 12, 16, 20, 24):
        stubs[cleanup] = emu.bind_stub(lambda active: consume(active), cleanup)
    for target, cleanup in CLEANUP.items():
        emu.bind_function(target, lambda active: consume(active), cleanup)
    checker = Checker(executable)
    ran = 0
    try:
        for index, case in enumerate(CASES):
            if only is not None and case["va"] != only:
                continue
            run_case(emu, checker, case, index, stubs)
            ran += 1
    finally:
        checker.close()
    print(f"aistate7d oracle Release cases={ran} functions={len(counts)} mismatches=0")
    return 0


if __name__ == "__main__":
    sys.exit(main())
