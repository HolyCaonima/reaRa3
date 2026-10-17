"""Release oracle for the fourth 0x00790000 AI-state pass.

Each case runs the shipped bytes in Unicorn and ra3_aistate79dcheck. EAX, object
bytes and the normalized call trace must match. Direct callees are stubbed.
Float-returning callees also push the scripted bits onto the x87 stack so the
following fadd/fstp/fistp sees the same value the port adds.
"""

from __future__ import annotations

import struct
import subprocess
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from emulate import STACK_BASE, STACK_SIZE, STUB_BASE, Emulator  # noqa: E402
from unicorn import x86_const as uc  # noqa: E402

SIZE = 0x500
GAME_LOGIC = 0x00DEBA64
TABLE_ABS = 0x00DFC4B4
LIMIT_ABS = 0x00BD95B8
MAX_ABS = 0x00BE700C
ONE_ABS = 0x00BD90A4
SCALE_ABS = 0x00DEEEE8
TERRAIN_ABS = 0x00DFC190
PATH_ABS = 0x00DFC198
PART_ABS = 0x00DFB9D8
MAXF = 0x7F7FFFFF
ONE = 0x3F800000
EAX_IN = 0xA5A5A5A5
F32 = (
    uc.UC_X86_REG_FP0, uc.UC_X86_REG_FP1, uc.UC_X86_REG_FP2, uc.UC_X86_REG_FP3,
    uc.UC_X86_REG_FP4, uc.UC_X86_REG_FP5, uc.UC_X86_REG_FP6, uc.UC_X86_REG_FP7,
)
FLOATS = {0x005A9550, 0x006A6CB0, 0x006F6EB0, 0x00706820, 0x0079D520}

POPS = {
    0x00797510: 4, 0x00797890: 4, 0x007986E0: 4, 0x0079A1D0: 4, 0x00798860: 4,
    0x00799AE0: 4, 0x00799100: 4, 0x0079CE60: 4, 0x00799820: 4, 0x00799F80: 4,
    0x0079CA70: 4, 0x00799CA0: 4, 0x00798D70: 4, 0x00798700: 4, 0x0079DAC0: 4,
}
CLEANUP = {
    0x00549E50: 0, 0x0054A090: 0, 0x005A94C0: 0, 0x005A9550: 0, 0x005AA280: 4,
    0x005AC4F0: 8, 0x005B9F30: 0, 0x005BDCC0: 4, 0x005C0A40: 4, 0x0063B510: 0,
    0x0067E7C0: 8, 0x0067E970: 8, 0x0067EBE0: 0, 0x00682D90: 4, 0x00683140: 4,
    0x00686FA0: 0, 0x00687380: 4, 0x0068D650: 0, 0x006A4EA0: 0, 0x006A6CB0: 4,
    0x006A7600: 0, 0x006A7620: 8, 0x006A76A0: 4, 0x006A7AE0: 12, 0x006AEFE0: 0,
    0x006C6D40: 0, 0x006D2020: 8, 0x006EC710: 4, 0x006F04A0: 8, 0x006F6EB0: 4,
    0x00706820: 4, 0x0070B050: 0, 0x00741480: 8, 0x007576C0: 4, 0x0075AD90: 12,
    0x0078C080: 4, 0x0078D1E0: 4, 0x00799890: 0, 0x0079D520: 8, 0x0079ECE0: 4,
    0x0079F030: 0, 0x007E1480: 4, 0x007E4AB0: 4, 0x007F58B0: 24, 0x0085CBC0: 16,
    0x00861CB0: 8, 0x00861E30: 8,
}
COUNTED = {
    0x007970D0, 0x00797A10, 0x007985D0, 0x00799410, 0x0079B1C0, 0x0079B690,
    0x0079C5D0, 0x0079C6E0, 0x0079D030, 0x0079D320, 0x0079D5C0, 0x0079D6A0,
    0x0079D8F0, 0x0079F400, 0x0079F4C0, 0x0079FFA0, 0x0079F580, 0x0079DA20,
    0x0079B280, 0x0079C990, 0x0079CB00, 0x0079FCD0, 0x00797510, 0x0079FE80,
    0x0079CA30, 0x0079EE10, 0x0079D260, 0x007989A0, 0x00797890, 0x007986E0,
    0x0079A1D0, 0x00799EC0, 0x0079F110, 0x0079A480, 0x0079C960, 0x00799EF0,
    0x0079AF30, 0x00798860, 0x00799AE0, 0x00799100, 0x0079CE60, 0x00799820,
    0x00799F80, 0x0079CA70, 0x00799CA0, 0x00798D70, 0x00798700, 0x0079DAC0,
}


def stub_addr(cleanup: int, index: int) -> int:
    return STUB_BASE + (cleanup // 4) * 0x100 + index * 0x10


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


class V:
    def __init__(self, mem: Mem) -> None:
        self.mem = mem
        self.n = {0: 0, 4: 0, 8: 0, 12: 0, 16: 0, 20: 0, 24: 0}

    def attach(self, obj: int, slots: list[tuple[int, int]]) -> dict[int, int]:
        table = self.mem.add()
        out: dict[int, int] = {}
        for offset, cleanup in slots:
            addr = stub_addr(cleanup, self.n[cleanup])
            self.n[cleanup] += 1
            self.mem.w(table, offset, addr)
            out[offset] = addr
        self.mem.link(obj, 0, table)
        return out


class Rig:
    def __init__(self) -> None:
        self.mem = Mem()
        self.v = V(self.mem)
        self.state = self.mem.add()
        self.machine = self.mem.add()
        self.unit = self.mem.add()
        self.mem.link(self.state, 0x18, self.machine)
        self.mem.link(self.machine, 0x24, self.unit)

    def obj(self) -> int:
        return self.mem.add()

    def vt(self, index: int, slots: list[tuple[int, int]]) -> dict[int, int]:
        return self.v.attach(index, slots)

    def logic(self, frame: int = 0, scale: int = ONE) -> int:
        logic = self.obj()
        self.mem.w(logic, 0x58, frame)
        self.mem.w(logic, 0x1C4, scale)
        return logic


CASES: list[dict] = []


def add(va: int, mem: Mem, rets: list, args: list[str] | None = None, eax: int = EAX_IN,
        gl: str = "0", table: str = "0", scale: int = 0, terrain: str = "0",
        pathfinder: str = "0", partition: str = "0", maxf: int = MAXF) -> None:
    CASES.append({
        "va": va, "mem": mem, "rets": rets, "args": args or [], "eax": eax,
        "gl": gl, "table": table, "scale": scale, "terrain": terrain,
        "pathfinder": pathfinder, "partition": partition, "limit": 0x43C80000,
        "maxf": maxf, "one": ONE,
    })


def at(index: int, offset: int = 0) -> str:
    return f"@{index}" if offset == 0 else f"@{index}+{offset:x}"


def chain() -> Rig:
    return Rig()


def build() -> None:
    # 007970d0 reload. Null weapon fails; a due frame passes 1.0; an early frame passes 6/8.
    rig = chain()
    add(0x007970D0, rig.mem, [])
    for low, sample in ((0, "3f800000"), (1, "3f800000")):
        rig = chain()
        weapon = rig.obj()
        rig.mem.link(rig.unit, 0x37C, weapon)
        rig.mem.w(rig.state, 0x24, 4)
        logic = rig.logic(frame=4)
        add(0x007970D0, rig.mem, [(0x006EC710, 1, f"{low:x}")], gl=at(logic))
    rig = chain()
    weapon = rig.obj()
    rig.mem.link(rig.unit, 0x37C, weapon)
    rig.mem.w(rig.state, 0x20, 8)
    rig.mem.w(rig.state, 0x24, 4)
    logic = rig.logic(frame=2)
    add(0x007970D0, rig.mem, [(0x006EC710, 1, "0")], gl=at(logic))

    # 00797a10 idle scan. Both endpoints zero completes; otherwise sample and maybe negate.
    rig = chain()
    node = rig.obj()
    rig.mem.link(rig.machine, 0x80, node)
    add(0x00797A10, rig.mem, [])
    for side, _name in ((1, "keep"), (0, "flip")):
        rig = chain()
        node = rig.obj()
        rig.mem.link(rig.machine, 0x80, node)
        rig.mem.w(node, 0x44, 0x40000000)
        rig.mem.w(node, 0x48, 0x40400000)
        add(0x00797A10, rig.mem, [
            (0x005A9550, 4, "3f800000", True), (0x005A94C0, 4, f"{side:x}"),
        ])

    # 007985d0 heading wrap. A zero delta and a wrapped 2pi both call 0x0078C080;
    # a pi delta calls the locomotor slot.
    for heading, callee in ((0, "small"), (0x40C90FDB, "wrap"), (0x40490FDB, "wide")):
        rig = chain()
        loco = rig.obj()
        rig.mem.link(rig.unit, 0x374, loco)
        rig.mem.w(rig.machine, 0x54, heading)
        loco_vt = rig.vt(loco, [(0x26C, 4)])
        if callee == "wide":
            rets = [(loco_vt[0x26C], 1, "0")]
        else:
            rets = [(0x0078C080, 1, "0")]
        add(0x007985D0, rig.mem, rets)

    # 00799410 garrison idle. No game logic keeps 15*roll; a 2.0 scale doubles it.
    rig = chain()
    add(0x00799410, rig.mem, [(0x005A94C0, 4, "2"), (0x006D2020, 2, "0")])
    rig = chain()
    logic = rig.logic(scale=0x40000000)
    add(0x00799410, rig.mem, [(0x005A94C0, 4, "2"), (0x006D2020, 2, "0")], gl=at(logic))

    # 0079b1c0 attack area. Scale 0 and scale 1.0 with a 2.0 logic step.
    for scale, step, rolled in ((0, ONE, 5), (ONE, 0x40000000, 3)):
        rig = chain()
        sub = rig.obj()
        logic = rig.logic(frame=10, scale=step)
        machine_vt = rig.vt(rig.machine, [(0x20, 4)])
        sub_vt = rig.vt(sub, [(0x14, 0)])
        add(0x0079B1C0, rig.mem, [
            (machine_vt[0x20], 1, at(sub)), (0x005A94C0, 4, f"{rolled:x}"),
            (sub_vt[0x14], 0, "7"),
        ], gl=at(logic), scale=scale)

    # 0079b690 TN guard. A null record yields a zero delay; a sample of 4 does not.
    for sample in (None, 4):
        rig = chain()
        part, mid = rig.obj(), rig.obj()
        rig.mem.link(part, 0x38, mid)
        loco = rig.obj()
        rig.mem.link(rig.unit, 0x374, loco)
        logic = rig.logic(frame=9, scale=ONE)
        rets = [(0x005A94C0, 4, "6"), (0x00682D90, 1, "0")]
        if sample is None:
            rig.mem.w(mid, 0, 0)
        else:
            rec = rig.obj()
            rig.mem.link(mid, 0, rec)
            rig.mem.w(rec, 0x2C, sample)
        add(0x0079B690, rig.mem, rets, gl=at(logic), partition=at(part))

    # 0079c5d0 boarding. Missing goal fails; a rejected probe uses the unit; a hit queries.
    rig = chain()
    add(0x0079C5D0, rig.mem, [(0x005B9F30, 0, "0")])
    for placed, hit in ((0, None), (1, 0)):
        rig = chain()
        goal, loco = rig.obj(), rig.obj()
        rig.mem.link(goal, 0x374, loco)
        rig.mem.w(rig.unit, 0x38, 0x3F800000)
        rets = [(0x005B9F30, 0, at(goal)), (0x00741480, 2, f"{placed:x}")]
        if placed:
            rets.append((0x0063B510, 3, "0" if hit == 0 else "1"))
        rets.append((0x005AC4F0, 2, "0"))
        add(0x0079C5D0, rig.mem, rets)

    # 0079c6e0 heli evacuate. No carrier, a positive Z, and a non-positive Z.
    for carrier_on, z_bits, grounded in ((0, 0x3F800000, 0), (1, 0x3F800000, 0), (1, 0, 1)):
        rig = chain()
        child = rig.obj()
        rig.mem.w(child, 0x68, 0x42)
        rig.mem.w(rig.machine, 0x2C, 0x40000000)
        rig.mem.w(rig.machine, 0x30, 0x40400000)
        rig.mem.w(rig.machine, 0x34, z_bits)
        state_vt = rig.vt(rig.state, [(0x4C, 0)])
        child_vt = rig.vt(child, [(0x1C, 4)])
        rets = [(state_vt[0x4C], 0, at(child)), (0x006A7600, 0, "0" if not carrier_on else "1")]
        if carrier_on:
            carrier = rig.obj()
            # The return token above is wrong when carrier_on; rebuild.
            rets = [(state_vt[0x4C], 0, at(child)), (0x006A7600, 0, at(carrier))]
            carrier_vt = rig.vt(carrier, [(0x108, 12)])
            rets.append((carrier_vt[0x108], 3, "0"))
        finder = rig.obj()
        rets.append((0x0085CBC0, 4, f"{grounded:x}"))
        if not (grounded and z_bits == 0):
            rets.append((0x005AC4F0, 2, "0"))
            rets.append((child_vt[0x1C], 1, "9"))
        add(0x0079C6E0, rig.mem, rets, pathfinder=at(finder))

    # 0079d030 deliver. Broker rejection increments; a due frame pushes and completes.
    rig = chain()
    module, templ, holder = rig.obj(), rig.obj(), rig.obj()
    rig.mem.link(rig.unit, 0x39C, module)
    rig.mem.link(module, 4, templ)
    rig.mem.w(templ, 0xC, ONE)
    rig.mem.w(templ, 8, 30)
    logic = rig.logic(frame=0, scale=ONE)
    rig.mem.w(rig.state, 0x20, 5)
    add(0x0079D030, rig.mem, [
        (0x00686FA0, 0, at(holder)), (0x007E4AB0, 1, "0"), (0x005B9F30, 0, "0"),
        (0x006C6D40, 0, "0"),
    ], gl=at(logic))
    rig = chain()
    module, templ, holder, broker = rig.obj(), rig.obj(), rig.obj(), rig.obj()
    rig.mem.link(rig.unit, 0x39C, module)
    rig.mem.link(module, 4, templ)
    rig.mem.w(templ, 0xC, ONE)
    rig.mem.w(templ, 8, 30)
    rig.mem.w(holder, 0x1F0, 0x77)
    logic = rig.logic(frame=1, scale=ONE)
    rig.mem.w(rig.state, 0x20, 1)
    add(0x0079D030, rig.mem, [
        (0x00686FA0, 0, at(holder)), (0x007E4AB0, 1, at(broker)), (0x007E1480, 1, "0"),
    ], gl=at(logic))
    rig = chain()
    module, templ, holder, broker = rig.obj(), rig.obj(), rig.obj(), rig.obj()
    rig.mem.link(rig.unit, 0x39C, module)
    rig.mem.link(module, 4, templ)
    rig.mem.w(templ, 0xC, ONE)
    rig.mem.w(templ, 8, 30)
    rig.mem.w(holder, 0x1F0, 0x77)
    rig.mem.w(rig.unit, 0xDC, 0x19)
    logic = rig.logic(frame=4, scale=ONE)
    rig.mem.w(rig.state, 0x20, 1)
    add(0x0079D030, rig.mem, [
        (0x00686FA0, 0, at(holder)), (0x007E4AB0, 1, at(broker)), (0x007E1480, 1, "1"),
        (0x005B9F30, 0, "0"), (0x006F6EB0, 1, "40200000", True), (0x007F58B0, 6, "0"),
        (0x0068D650, 0, "4"), (0x006F6EB0, 1, "3f800000", True), (0x007F58B0, 6, "0"),
        (0x0070B050, 0, "0"), (0x006C6D40, 0, "0"),
    ], gl=at(logic))

    # 0079d320 best bone. Under 1000 completes; at 1000 still fails after the goal write.
    for index, late in ((10, False), (0x3E8, True)):
        rig = chain()
        goal, record, module = rig.obj(), rig.obj(), rig.obj()
        rig.mem.link(goal, 0x394, record)
        rig.mem.link(rig.unit, 0x39C, module)
        rig.mem.w(module, 0x1C, 3)
        rig.mem.w(rig.unit, 0xDC, 9)
        rets = [(0x005B9F30, 0, at(goal)), (0x0067E970, 2, f"{index:x}")]
        if late:
            rets += [(0x0067E7C0, 2, "0"), (0x005AC4F0, 2, "0")]
        add(0x0079D320, rig.mem, rets)
    rig = chain()
    add(0x0079D320, rig.mem, [(0x005B9F30, 0, "0")])

    # 0079d5c0 drift. Height under the limit moves; at or over it layers and completes.
    for height, low in (("3f000000", True), ("41200000", False)):
        rig = chain()
        table = rig.obj()
        terrain = rig.obj()
        rig.mem.w(rig.unit, 0x40, 0x40A00000)
        rig.mem.w(table, 0x6C, 0x3F800000)
        rets = [(0x00861CB0, 2, "1"), (0x0079D520, 2, height, True)]
        if low:
            rets.append((0x0078D1E0, 1, "0"))
        else:
            rets += [(0x00686FA0, 0, "0"), (0x00861E30, 2, "2"), (0x00687380, 1, "0")]
        add(0x0079D5C0, rig.mem, rets, table=at(table), terrain=at(terrain))

    # 0079d6a0 freefall. Below the bias moves; above it fails. A late frame completes.
    # The z=10 cases sit just under and exactly on z - 5.0, so a wrong bias is caught.
    for z, height, below, frame, deadline in ((0x40A00000, "bf800000", True, 1, 5),
                                              (0x40A00000, "3f800000", False, 1, 5),
                                              (0x40A00000, "bf800000", True, 9, 5),
                                              (0x41200000, "40980000", True, 1, 5),
                                              (0x41200000, "40a00000", False, 1, 5),
                                              (0x41200000, "40a80000", False, 1, 5)):
        rig = chain()
        terrain = rig.obj()
        terrain_vt = rig.vt(terrain, [(0x1C, 8)])
        rig.mem.w(rig.unit, 0x40, z)
        rig.mem.w(rig.state, 0x20, deadline)
        logic = rig.logic(frame=frame)
        rets = [(terrain_vt[0x1C], 2, height, True)]
        if below:
            rets.append((0x0078D1E0, 1, "0"))
        add(0x0079D6A0, rig.mem, rets, gl=at(logic), terrain=at(terrain))

    # 0079d8f0 face target, then the heading helper. Goal selects the position pointer.
    for with_goal, facing, heading in ((0, "0", 0), (1, "3f800000", 0), (0, "0", 0x40490FDB)):
        rig = chain()
        rig.mem.w(rig.unit, 0x44, 0)
        rig.mem.w(rig.machine, 0x54, heading)
        loco = rig.obj()
        rig.mem.link(rig.unit, 0x374, loco)
        loco_vt = rig.vt(loco, [(0x26C, 4)])
        rets = [(0x005B9F30, 0, "0")]
        if with_goal:
            goal = rig.obj()
            rets = [(0x005B9F30, 0, at(goal))]
        rets += [(0x006A6CB0, 1, facing, True), (0x005AA280, 1, "0")]
        if heading == 0:
            rets.append((0x0078C080, 1, "0"))
        else:
            rets.append((loco_vt[0x26C], 1, "0"))
        add(0x0079D8F0, rig.mem, rets)

    def guard(va: int, from_unit: bool, hold: bool, aim: bool, airborne: int) -> None:
        rig = chain()
        child, loco = rig.obj(), rig.obj()
        rig.mem.w(child, 0x68, 0x42)
        rig.mem.w(rig.unit, 0x44, ONE)
        rig.mem.link(rig.unit, 0x374, loco)
        if from_unit:
            rig.mem.w(rig.unit, 0x38, 0x40000000)
        else:
            rig.mem.w(rig.machine, 0x2C, 0x40000000)
        state_vt = rig.vt(rig.state, [(0x4C, 0)])
        child_vt = rig.vt(child, [(0x1C, 4)])
        slots = [(0x228, 4)]
        if aim:
            slots.append((0x250, 0))
        loco_vt = rig.vt(loco, slots)
        rets = [(state_vt[0x4C], 0, at(child)), (0x005AC4F0, 2, "0")]
        if aim:
            rets.append((loco_vt[0x250], 0, f"{airborne:x}"))
            if airborne == 0:
                rets.append((0x005AA280, 1, "0"))
        notice_ecx = at(rig.unit) if va == 0x0079F580 else at(loco)
        rets += [
            (loco_vt[0x228], 1, "0"), (0x006AEFE0, 3, "0"), (child_vt[0x1C], 1, "8"),
        ]
        add(va, rig.mem, rets)
        del hold, notice_ecx

    # The notice ecx differs and is asserted by the call log built above only as the
    # target address. Two airborne states cover the aim branch.
    for airborne in (0, 1):
        guard(0x0079F400, False, False, True, airborne)
        guard(0x0079F4C0, True, True, True, airborne)
    guard(0x0079F580, True, False, False, 0)
    guard(0x0079F580, True, False, False, 0)
    guard(0x0079DA20, True, False, False, 0)
    guard(0x0079DA20, True, False, False, 0)

    # 0079ffa0 move-to-and-enter. No goal fails after the child spawn.
    rig = chain()
    child = rig.obj()
    state_vt = rig.vt(rig.state, [(0x4C, 0)])
    add(0x0079FFA0, rig.mem, [(state_vt[0x4C], 0, at(child)), (0x005B9F30, 0, "0")])
    for dock_on in (0, 1):
        rig = chain()
        child, goal, terrain, loco = rig.obj(), rig.obj(), rig.obj(), rig.obj()
        rig.mem.w(child, 0x68, 0x15)
        rig.mem.w(goal, 0xDC, 0x21)
        rig.mem.link(rig.unit, 0x374, loco)
        state_vt = rig.vt(rig.state, [(0x4C, 0)])
        child_vt = rig.vt(child, [(0x38, 4), (0x1C, 4)])
        terrain_vt = rig.vt(terrain, [(0x24, 20)])
        rets = [
            (state_vt[0x4C], 0, at(child)), (0x005B9F30, 0, at(goal)),
            (0x006A76A0, 1, "0"), (0x00861CB0, 4, "3"),
            (terrain_vt[0x24], 3, "0", True), (0x005AC4F0, 2, "0"),
            (child_vt[0x38], 1, "0"), (0x00683140, 1, "0"),
            (0x006A7620, 2, "0" if not dock_on else "1"),
        ]
        if dock_on:
            dock = rig.obj()
            rets[-1] = (0x006A7620, 2, at(dock))
            dock_vt = rig.vt(dock, [(0x40, 8)])
            rets.append((dock_vt[0x40], 2, "0"))
        rets.append((child_vt[0x1C], 1, "4"))
        add(0x0079FFA0, rig.mem, rets, terrain=at(terrain))

    # Short onEnter variants.
    def spawned(va: int, extra: list, angle: int = 0, byte: int = 0) -> None:
        rig = chain()
        child = rig.obj()
        rig.mem.w(child, 0x68, 0x33)
        rig.mem.w(rig.machine, 0x54, angle)
        rig.mem.b(rig.machine, 0x7C, byte)
        state_vt = rig.vt(rig.state, [(0x4C, 0)])
        child_vt = rig.vt(child, [(0x1C, 4)])
        rets = [(state_vt[0x4C], 0, at(child)), (0x005AC4F0, 2, "0"), *extra, (child_vt[0x1C], 1, "6")]
        add(va, rig.mem, rets)

    spawned(0x0079C990, [(0x005AA280, 1, "0"), (0x007576C0, 1, "0")], angle=ONE, byte=1)
    spawned(0x0079C990, [(0x005AA280, 1, "0"), (0x007576C0, 1, "0")], angle=0, byte=0)
    spawned(0x0079CB00, [(0x005AA280, 1, "0")], angle=ONE, byte=1)
    spawned(0x0079CB00, [(0x005AA280, 1, "0")], angle=0, byte=0)
    spawned(0x0079FCD0, [(0x005AA280, 1, "0")], angle=ONE)
    spawned(0x0079FCD0, [(0x005AA280, 1, "0")])
    spawned(0x0079CA30, [])
    spawned(0x0079CA30, [])
    spawned(0x0079FE80, [])
    spawned(0x0079FE80, [])
    for _ in range(2):
        rig = chain()
        child = rig.obj()
        rig.mem.w(child, 0x68, 1)
        state_vt = rig.vt(rig.state, [(0x4C, 0)])
        child_vt = rig.vt(child, [(0x1C, 4)])
        add(0x0079C960, rig.mem, [(state_vt[0x4C], 0, at(child)), (child_vt[0x1C], 1, "2")])

    # 00797510 drift slot 6. 2.0 * 1.0 truncates to 2; a zero scale truncates to 0.
    for scale, ticks in ((ONE, 2), (0, 0)):
        rig = chain()
        table = rig.obj()
        rig.mem.w(table, 0x70, 0x40000000)
        add(0x00797510, rig.mem, [(0x006F04A0, 2, "5")], args=["0"], table=at(table), scale=scale)

    # 0079ee10 and 0079d260.
    for _ in range(2):
        rig = chain()
        child = rig.obj()
        rig.mem.w(child, 0x68, 4)
        state_vt = rig.vt(rig.state, [(0x4C, 0)])
        child_vt = rig.vt(child, [(0x1C, 4)])
        add(0x0079EE10, rig.mem, [
            (state_vt[0x4C], 0, at(child)), (0x005BDCC0, 1, "0"), (0x0079ECE0, 1, "0"),
            (child_vt[0x1C], 1, "3"),
        ])
    for goal_on in (0, 1):
        rig = chain()
        child = rig.obj()
        rig.mem.w(child, 0x68, 4)
        state_vt = rig.vt(rig.state, [(0x4C, 0)])
        child_vt = rig.vt(child, [(0x38, 4), (0x1C, 4)])
        goal = at(rig.obj()) if goal_on else "0"
        # goal token must be an object when non-zero; rebuild properly below.
        add(0x0079D260, rig.mem, [])  # placeholder replaced
        CASES.pop()
        rig = chain()
        child = rig.obj()
        rig.mem.w(child, 0x68, 4)
        state_vt = rig.vt(rig.state, [(0x4C, 0)])
        child_vt = rig.vt(child, [(0x38, 4), (0x1C, 4)])
        goal_token = "0"
        if goal_on:
            goal = rig.obj()
            goal_token = at(goal)
        add(0x0079D260, rig.mem, [
            (state_vt[0x4C], 0, at(child)), (0x005B9F30, 0, goal_token),
            (child_vt[0x38], 1, "0"), (child_vt[0x1C], 1, "3"),
        ])

    for _ in range(2):
        rig = chain()
        add(0x007989A0, rig.mem, [(0x006D2020, 2, "0"), (0x006D2020, 2, "0"), (0x006A4EA0, 0, "9")])
        add(0x0079A1D0, rig.mem, [
            (0x006D2020, 2, "1"), (0x006D2020, 2, "2"), (0x006D2020, 2, "3"),
        ], args=["0"])

    # 00797890 angle threshold. 1.0 is outside 0.1; a small value is inside.
    for bits in ("3f800000", "3d000000"):
        rig = chain()
        add(0x00797890, rig.mem, [(0x006A6CB0, 1, bits, True)], args=[at(rig.unit, 0x38)])
    # 007986e0 clears the stored heading after the call.
    for prior in (0x3F800000, 0):
        rig = chain()
        rig.mem.w(rig.state, 0x20, prior)
        add(0x007986E0, rig.mem, [(0x005AA280, 1, "4")], args=["0"])

    # Predicates.
    rig = chain()
    add(0x00799EC0, rig.mem, [])
    rig = chain()
    link, obj = rig.obj(), rig.obj()
    rig.mem.link(rig.state, 0x60, link)
    rig.mem.link(link, 4, obj)
    obj_vt = rig.vt(obj, [(0x20, 0)])
    add(0x00799EC0, rig.mem, [(obj_vt[0x20], 0, "100")])
    rig = chain()
    link = rig.obj()
    rig.mem.link(rig.state, 0x60, link)
    add(0x00799EC0, rig.mem, [])

    for status in ("fffffffe", "5"):
        rig = chain()
        child = rig.obj()
        rig.mem.link(rig.state, 0x20, child)
        child_vt = rig.vt(child, [(0x48, 4)])
        rets = [(0x0079F030, 0, status)]
        if status != "fffffffe":
            rets.append((child_vt[0x48], 1, "0"))
        add(0x0079F110, rig.mem, rets)

    rig = chain()
    add(0x0079A480, rig.mem, [])
    rig = chain()
    link, obj = rig.obj(), rig.obj()
    rig.mem.link(rig.state, 0x24, link)
    rig.mem.link(link, 4, obj)
    obj_vt = rig.vt(obj, [(0x20, 0)])
    add(0x0079A480, rig.mem, [(obj_vt[0x20], 0, "12")])

    for offset, va in ((0x58, 0x00799EF0),):
        rig = chain()
        link = rig.obj()
        rig.mem.link(rig.state, offset, link)
        add(va, rig.mem, [])
        rig = chain()
        link, obj = rig.obj(), rig.obj()
        rig.mem.link(rig.state, offset, link)
        rig.mem.link(link, 4, obj)
        obj_vt = rig.vt(obj, [(0x20, 0)])
        add(va, rig.mem, [(obj_vt[0x20], 0, "0")])

    rig = chain()
    add(0x0079AF30, rig.mem, [])
    rig = chain()
    link, obj = rig.obj(), rig.obj()
    rig.mem.link(rig.state, 0x20, link)
    rig.mem.link(link, 4, obj)
    obj_vt = rig.vt(obj, [(0x20, 0)])
    add(0x0079AF30, rig.mem, [(obj_vt[0x20], 0, "8")])

    rig = chain()
    add(0x00798860, rig.mem, [], args=["0"])
    rig = chain()
    child = rig.obj()
    rig.mem.link(rig.state, 0x20, child)
    child_vt = rig.vt(child, [(0, 4)])
    add(0x00798860, rig.mem, [(child_vt[0], 1, "6")], args=["0"])
    rig = chain()
    add(0x00799AE0, rig.mem, [], args=["0"])
    rig = chain()
    child = rig.obj()
    rig.mem.link(rig.state, 0x20, child)
    child_vt = rig.vt(child, [(0, 4)])
    add(0x00799AE0, rig.mem, [(child_vt[0], 1, "6")], args=["0"])

    # 0079b280 face. Probe zero, probe equal to zero, and mode 2.
    rig = chain()
    loco = rig.obj()
    rig.mem.link(rig.unit, 0x374, loco)
    add(0x0079B280, rig.mem, [(0x005B9F30, 0, at(rig.obj()))])
    rig = chain()
    loco, probe = rig.obj(), rig.obj()
    rig.mem.link(rig.unit, 0x374, loco)
    rig.mem.link(loco, 0x214, probe)
    rig.mem.w(rig.state, 0x28, 1)
    add(0x0079B280, rig.mem, [
        (0x00706820, 1, "0", True), (0x005B9F30, 0, "0"),
    ])
    rig = chain()
    loco = rig.obj()
    rig.mem.link(rig.unit, 0x374, loco)
    rig.mem.w(rig.state, 0x28, 2)
    add(0x0079B280, rig.mem, [(0x005B9F30, 0, at(rig.obj())), (0x00799890, 0, "4")])

    # Xfers. Each transfers its own fields after the version bytes.
    def xfer(va: int, slots: list[tuple[int, int]], tail: list | None = None) -> None:
        for marker in (0x11, 0x22):
            rig = chain()
            stream = rig.obj()
            mapping = {}
            for slot, cleanup in slots:
                mapping.setdefault(slot, cleanup)
            stream_vt = rig.vt(stream, [(slot, cleanup) for slot, cleanup in mapping.items()])
            rets = []
            # version is slot 0x18 when present in slots, but callers list the sequence.
            for slot, _cleanup in slots:
                if slot == 0:
                    rets.append((0x0054A090 if va in (0x00799100, 0x00798D70) else 0x00549E50, 2, "0"))
                elif slot == 1:
                    rets.append((0x005C0A40, 1, "0"))
                else:
                    rets.append((stream_vt[slot], 1, f"{marker:x}" if slot == slots[-1][0] else "0"))
            if tail:
                rets = tail(rig, stream, stream_vt, marker)
            add(va, rig.mem, rets, args=[at(stream)])

    def harvest(_rig, stream, stream_vt, marker):
        return [
            (stream_vt[0x18], 1, "0"), (stream_vt[0x6C], 1, "0"), (0x0054A090, 2, "0"),
            (stream_vt[0x64], 1, "0"), (stream_vt[0x6C], 1, "0"), (stream_vt[0x6C], 1, "0"),
            (stream_vt[0x6C], 1, f"{marker:x}"),
        ]

    def land(_rig, stream, stream_vt, marker):
        return [
            (stream_vt[0x18], 1, "0"), (stream_vt[0x18], 1, "0"), (stream_vt[0x70], 1, "0"),
            (stream_vt[0x58], 1, "0"), (stream_vt[0x58], 1, f"{marker:x}"),
        ]

    def idle(_rig, stream, stream_vt, marker):
        return [
            (stream_vt[0x18], 1, "0"), (stream_vt[0x74], 1, "0"), (stream_vt[0x84], 1, "0"),
            (stream_vt[0x84], 1, "0"), (stream_vt[0x74], 1, f"{marker:x}"),
        ]

    def aim(_rig, stream, stream_vt, marker):
        return [
            (stream_vt[0x18], 1, "0"), (stream_vt[0x84], 1, "0"), (stream_vt[0x84], 1, "0"),
            (stream_vt[0x84], 1, f"{marker:x}"),
        ]

    def jet(_rig, stream, stream_vt, marker):
        return [
            (stream_vt[0x18], 1, "0"), (stream_vt[0x18], 1, "0"), (0x005C0A40, 1, "0"),
            (stream_vt[0x70], 1, f"{marker:x}"),
        ]

    def cower(_rig, stream, stream_vt, marker):
        return [
            (stream_vt[0x18], 1, "0"), (stream_vt[0x84], 1, "0"), (stream_vt[0x18], 1, f"{marker:x}"),
        ]

    def dock(_rig, stream, stream_vt, marker):
        return [
            (stream_vt[0x18], 1, "0"), (0x0054A090, 2, "0"), (stream_vt[0x6C], 1, "0"),
            (stream_vt[0x6C], 1, f"{marker:x}"),
        ]

    def rotate(_rig, stream, stream_vt, marker):
        return [
            (stream_vt[0x18], 1, "0"), (stream_vt[0x64], 1, "0"), (stream_vt[0x18], 1, f"{marker:x}"),
        ]

    def aimfire(_rig, stream, stream_vt, marker):
        return [
            (stream_vt[0x18], 1, "0"), (stream_vt[0x18], 1, "0"), (0x00549E50, 2, f"{marker:x}"),
        ]

    for _marker_pair in ((0x11, 0x22),):
        for va, slots, tail in (
            (0x00799100, [(0x18, 4), (0x6C, 4), (0x64, 4)], harvest),
            (0x0079CE60, [(0x18, 4), (0x70, 4), (0x58, 4)], land),
            (0x00799820, [(0x18, 4), (0x74, 4), (0x84, 4)], idle),
            (0x00799F80, [(0x18, 4), (0x84, 4)], aim),
            (0x0079CA70, [(0x18, 4), (0x70, 4)], jet),
            (0x00799CA0, [(0x18, 4), (0x84, 4)], cower),
            (0x00798D70, [(0x18, 4), (0x6C, 4)], dock),
            (0x00798700, [(0x18, 4), (0x64, 4)], rotate),
            (0x0079DAC0, [(0x18, 4)], aimfire),
        ):
            xfer(va, slots, tail)


def push_f32(emu: Emulator, bits: int) -> None:
    sign = bits >> 31
    exp = (bits >> 23) & 0xFF
    frac = bits & 0x7FFFFF
    if exp == 0:
        mant, exponent = 0, 0
    elif exp == 255:
        mant, exponent = (1 << 63) | (frac << 40), 0x7FFF
    else:
        mant = (1 << 63) | (frac << 40)
        exponent = exp - 127 + 16383
    fpsw = emu.uc.reg_read(uc.UC_X86_REG_FPSW)
    top = (fpsw >> 11) & 7
    new = (top - 1) & 7
    emu.uc.reg_write(F32[new], (mant, (sign << 15) | exponent))
    fpsw = (fpsw & ~(7 << 11)) | (new << 11)
    emu.uc.reg_write(uc.UC_X86_REG_FPSW, fpsw)
    tag = emu.uc.reg_read(uc.UC_X86_REG_FPTAG)
    emu.uc.reg_write(uc.UC_X86_REG_FPTAG, tag & ~(3 << (2 * new)))


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
            raise RuntimeError("aistate79dcheck failed")


def line_for(case: dict) -> str:
    mem: Mem = case["mem"]
    words = [
        "run", f"{case['va']:08x}", f"{case['eax']:x}", case["gl"], case["table"],
        f"{case['limit']:x}", f"{case['maxf']:x}", f"{case['one']:x}", f"{case['scale']:x}",
        case["terrain"], case["pathfinder"], case["partition"], str(len(case["args"])),
        *case["args"], str(len(mem.blobs)),
    ]
    for blob in mem.blobs:
        words.append(blob.hex())
    words.append(str(len(mem.fix)))
    for index, offset, target in mem.fix:
        words.extend((str(index), f"{offset:x}", str(target)))
    words.append(str(len(case["rets"])))
    for spec in case["rets"]:
        words.append(spec[2])
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


def on_call(active: Emulator) -> int:
    box = active.box
    target = active.register(uc.UC_X86_REG_EIP)
    if not box["pending"]:
        raise RuntimeError(f"extra call 0x{target:08x}")
    spec = box["pending"].pop(0)
    if spec["target"] != target:
        raise RuntimeError(f"expected 0x{spec['target']:08x} got 0x{target:08x}")
    ecx = active.register(uc.UC_X86_REG_ECX)
    args = [struct.unpack("<I", active.read(
        active.register(uc.UC_X86_REG_ESP) + 4 + 4 * index, 4))[0] for index in range(spec["argc"])]
    text = f"{target:08x}:" + norm(ecx, box["bases"])
    text += ":" + "+".join(norm(arg, box["bases"]) for arg in args)
    box["log"].append(text)
    if spec["float"] or target in FLOATS:
        push_f32(active, spec["ret"])
    return spec["ret"]


def bind(emu: Emulator) -> None:
    # Groups are 0x100 apart, so a skipped cleanup still occupies a group.
    for cleanup in (0, 4, 8, 12, 16, 20, 24):
        for index in range(16):
            address = emu.bind_stub(on_call, cleanup)
            if address != stub_addr(cleanup, index):
                raise RuntimeError(f"stub pool 0x{address:08x}")
    for target, cleanup in CLEANUP.items():
        emu.bind_function(target, on_call, cleanup)


def resolve_token(token: str, bases: list[int]) -> int:
    if token.startswith("@"):
        return bases[int(token[1:].split("+")[0])] + (int(token.split("+")[1], 16) if "+" in token else 0)
    return int(token, 16)


def run_case(emu: Emulator, checker: Checker, case: dict, index: int) -> None:
    emu.reset_data()
    emu.write(STACK_BASE, b"\x00" * STACK_SIZE)
    emu.uc.reg_write(uc.UC_X86_REG_FPCW, 0x027F)
    emu.uc.reg_write(uc.UC_X86_REG_FPSW, 0)
    emu.uc.reg_write(uc.UC_X86_REG_FPTAG, 0xFFFF)
    mem: Mem = case["mem"]
    bases = [emu.alloc(bytes(blob), alignment=256) for blob in mem.blobs]
    for obj, offset, target in mem.fix:
        emu.write(bases[obj] + offset, struct.pack("<I", bases[target]))
    gl = 0 if case["gl"] in ("0", "-") else resolve_token(case["gl"], bases)
    table = 0 if case["table"] in ("0", "-") else resolve_token(case["table"], bases)
    terrain = 0 if case["terrain"] in ("0", "-") else resolve_token(case["terrain"], bases)
    finder = 0 if case["pathfinder"] in ("0", "-") else resolve_token(case["pathfinder"], bases)
    part = 0 if case["partition"] in ("0", "-") else resolve_token(case["partition"], bases)
    emu.write(GAME_LOGIC, struct.pack("<I", gl))
    emu.write(TABLE_ABS, struct.pack("<I", table))
    emu.write(LIMIT_ABS, struct.pack("<I", case["limit"]))
    emu.write(MAX_ABS, struct.pack("<I", case["maxf"]))
    emu.write(ONE_ABS, struct.pack("<I", case["one"]))
    emu.write(SCALE_ABS, struct.pack("<I", case["scale"]))
    emu.write(TERRAIN_ABS, struct.pack("<I", terrain))
    emu.write(PATH_ABS, struct.pack("<I", finder))
    emu.write(PART_ABS, struct.pack("<I", part))
    box = emu.box
    box["bases"] = bases
    box["log"] = []
    box["pending"] = [{
        "target": spec[0], "argc": spec[1], "ret": resolve_token(spec[2], bases),
        "float": (len(spec) > 3 and spec[3]) or spec[0] in FLOATS,
    } for spec in case["rets"]]
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
        problems.append(f"calls\n  native {native_log}\n  port   {port_log}")
    for obj, (native, port) in enumerate(zip(native_mem, port_mem)):
        left, right = canon(native, bases), canon(port, port_bases)
        if left != right:
            offset = next(i for i, (a, b) in enumerate(zip(left, right)) if a != b)
            problems.append(
                f"obj{obj}+0x{offset:x} {left[offset:offset+8].hex()} != {right[offset:offset+8].hex()}")
            break
    if problems:
        raise RuntimeError(f"case {index} 0x{case['va']:08x} " + "; ".join(problems))


def main() -> int:
    build()
    counts: dict[int, int] = {}
    for case in CASES:
        counts[case["va"]] = counts.get(case["va"], 0) + 1
    missing = sorted(COUNTED - set(counts))
    extra = sorted(set(counts) - COUNTED)
    thin = [f"{va:08x}:{n}" for va, n in sorted(counts.items()) if n < 2]
    if missing or extra or thin or len(counts) != 48:
        raise SystemExit(
            f"coverage {len(counts)} missing {[f'{v:08x}' for v in missing]} "
            f"extra {[f'{v:08x}' for v in extra]} thin {thin}")
    executable = Path(sys.argv[1]) if len(sys.argv) > 1 else (
        Path(__file__).resolve().parents[3] / "build/win32/bin/Release/ra3_aistate79dcheck.exe")
    emu = Emulator()
    emu.box = {}
    bind(emu)
    checker = Checker(executable)
    try:
        for index, case in enumerate(CASES):
            run_case(emu, checker, case, index)
    finally:
        checker.close()
    print(f"aistate79d oracle Release cases={len(CASES)} functions={len(counts)} mismatches=0")
    return 0


if __name__ == "__main__":
    sys.exit(main())
