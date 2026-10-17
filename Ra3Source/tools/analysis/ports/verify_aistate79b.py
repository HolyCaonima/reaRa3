"""Release oracle for the second 0x00790000 AI-state pass.

Each case runs the shipped bytes in Unicorn and the C++ port in ra3_aistate79bcheck
on the same object graph. EAX, object bytes and the normalized call trace must match.
Virtual slots are filled with a fixed stub pool so both sides log the same target.
.data is restored before every case: 0079cda0 sets the name-key guard, and 0079fc40
reads 0x00DF03FC, which this oracle mirrors from the table token.
"""

from __future__ import annotations

import struct
import subprocess
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from emulate import STACK_BASE, STACK_SIZE, STUB_BASE, Emulator  # noqa: E402
from unicorn import x86_const as uc  # noqa: E402

SIZE = 0x480
GAME_LOGIC = 0x00DEBA64
LOGIC_TABLE = 0x00DFC4B4
LOGIC_TABLE_2 = 0x00DF03FC
LIMIT_ABS = 0x00BD95B8
MAX_ABS = 0x00BE700C
ONE_ABS = 0x00BD90A4
LIMIT = 0x43C80000
MAXF = 0x7F7FFFFF
ONE = 0x3F800000
EAX_IN = 0xA5A5A5A5
INNER = 0x0079F030

POPS = {
    0x00797C00: 4, 0x00798F80: 4, 0x00799C30: 4, 0x0079A520: 4, 0x0079A620: 4,
    0x0079A9B0: 4, 0x0079AC50: 4, 0x0079ADF0: 4, 0x0079AF50: 4, 0x0079B130: 4,
    0x0079C820: 4, 0x0079CDA0: 4, 0x0079CFD0: 4, 0x0079D2A0: 4, 0x0079DEB0: 4,
    0x0079EBA0: 4, 0x0079EC10: 4, 0x0079EEF0: 4, 0x0079F290: 4, 0x0079F6F0: 4,
    0x0079FC40: 4, 0x0079FF10: 4,
}
CLEANUP = {
    0x00549C90: 0, 0x0054A090: 0, 0x005A94C0: 0, 0x005AA240: 4, 0x005AA250: 0,
    0x005B9F30: 0, 0x005C0A40: 4, 0x0067D870: 0, 0x0067E410: 4, 0x0067E860: 8,
    0x00682D70: 0, 0x00682F20: 4, 0x00683140: 4, 0x00684BD0: 0, 0x00684C00: 0,
    0x00685C20: 0, 0x00686460: 0, 0x00686FA0: 0, 0x00687460: 4, 0x006876D0: 4,
    0x006878B0: 4, 0x00687C90: 0, 0x00687CB0: 0, 0x00688810: 0, 0x00688830: 0,
    0x0069FC10: 0, 0x006A4EA0: 0, 0x006A51A0: 4, 0x006A72A0: 4, 0x006A7600: 0,
    0x006A7620: 8, 0x006B6E30: 4, 0x006B7040: 8, 0x006C0F40: 4, 0x006C1D60: 4,
    0x006D2020: 8, 0x006E4760: 0, 0x006E5CE0: 0, 0x006E6360: 4, 0x006EFE40: 0,
    0x006F0440: 0, 0x006F17F0: 4, 0x006F19F0: 0, 0x007000E0: 0, 0x00706E00: 4,
    0x00718FA0: 4, 0x007527C0: 4, 0x0077B1F0: 4, 0x00798150: 8, 0x00799920: 0,
    0x0079EF90: 0, 0x007E2950: 4, 0x00986150: 4, 0x009CCDA0: 4, 0x00A780D0: 0,
}
COUNTED = {
    0x00797B80, 0x00797C00, 0x00797660, 0x00797760, 0x007977C0, 0x00797FA0,
    0x00798310, 0x00798560, 0x00798F80, 0x00799A20, 0x00799C30, 0x0079A310,
    0x0079A520, 0x0079A620, 0x0079A680, 0x0079A780, 0x0079A800, 0x0079A870,
    0x0079A9B0, 0x0079AAC0, 0x0079AB10, 0x0079AC50, 0x0079AD70, 0x0079ADF0,
    0x0079AF50, 0x0079AFD0, 0x0079B130, 0x0079B720, 0x0079C820, 0x0079CDA0,
    0x0079CFD0, 0x0079D2A0, 0x0079D930, 0x0079D9B0, 0x0079DEB0, 0x0079E190,
    0x0079E960, 0x0079E9C0, 0x0079EBA0, 0x0079EC10, 0x0079EEF0, 0x0079F030,
    0x0079F150, 0x0079F290, 0x0079F610, 0x0079F6F0, 0x0079FC40, 0x0079FF10,
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
        self.n = {0: 0, 4: 0, 8: 0, 12: 0}

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
        eax: int = EAX_IN, gl: str = "0", table: str = "0") -> None:
    CASES.append({
        "va": va, "mem": mem, "rets": rets, "args": args or [], "eax": eax,
        "gl": gl, "table": table, "limit": LIMIT, "maxf": MAXF, "one": ONE, "capture": False,
    })


def xfer_body(va: int, ident: int, tail: bool) -> None:
    mem = Mem()
    state = mem.add()
    xfer = mem.add()
    mem.w(state, 0x20, ident)
    slots = V(mem).attach(xfer, [(0x18, 4), (0x84, 4), (0x20, 4), (0x6C, 4)])
    rets = [(slots[0x18], 1, "1"), (slots[0x84], 1, "1")]
    if ident != 0:
        rets.append((slots[0x20], 1, "2"))
    if tail:
        rets.append((slots[0x6C], 1, "3"))
    add(va, mem, rets, ["@1"])


def build() -> None:
    # Shared guard xfers. Bit 3 returns immediately; a present id takes slot 0x20.
    for va in (0x0079A520, 0x0079AC50, 0x0079ADF0, 0x0079AF50, 0x0079B130):
        mem = Mem(); mem.add(); xfer = mem.add(); mem.w(xfer, 4, 8)
        V(mem).attach(xfer, [(0x18, 4)])
        add(va, mem, [], ["@1"])
        xfer_body(va, 0, va == 0x0079B130)
        xfer_body(va, 0x55, va == 0x0079B130)

    # 0079EEF0 waypoint-machine xfer. Version byte 2 always keeps the second 00798150.
    for token in ("11", "22"):
        mem = Mem(); mem.add(); xfer = mem.add()
        slots = V(mem).attach(xfer, [(0x18, 4), (0x44, 4), (0x64, 4), (0x84, 4)])
        add(0x0079EEF0, mem, [
            (slots[0x18], 1, "1"), (0x005C0A40, 1, "1"), (0x00798150, 2, "1"),
            (slots[0x44], 1, "1"), (slots[0x64], 1, "1"), (slots[0x84], 1, "1"),
            (0x00798150, 2, token),
        ], ["@1"])

    # 0079DEB0 attack-object xfer, with and without the mode bit that fills +0x20.
    mem = Mem(); state = mem.add(); xfer = mem.add()
    slots = V(mem).attach(xfer, [(0x18, 4), (0x20, 4)])
    add(0x0079DEB0, mem, [
        (slots[0x18], 1, "1"), (0x00549C90, 2, "1"), (slots[0x18], 1, "1"), (slots[0x20], 1, "9"),
    ], ["@1"])
    mem = Mem(); state = mem.add(); xfer = mem.add(); mem.w(xfer, 4, 2)
    v = V(mem)
    slots = v.attach(xfer, [(0x18, 4), (0x20, 4)])
    self_slots = v.attach(state, [(0x4C, 0)])
    add(0x0079DEB0, mem, [
        (slots[0x18], 1, "1"), (0x00549C90, 2, "1"), (slots[0x18], 1, "1"),
        (self_slots[0x4C], 0, "77"), (slots[0x20], 1, "9"),
    ], ["@1"])

    # 0079C820 heli evacuate xfer.
    mem = Mem(); mem.add(); xfer = mem.add()
    slots = V(mem).attach(xfer, [(0x18, 4), (0x20, 4)])
    add(0x0079C820, mem, [(slots[0x18], 1, "1"), (slots[0x18], 1, "1"), (slots[0x20], 1, "4")], ["@1"])
    mem = Mem(); state = mem.add(); xfer = mem.add(); mem.w(xfer, 4, 2)
    v = V(mem)
    slots = v.attach(xfer, [(0x18, 4), (0x20, 4)])
    self_slots = v.attach(state, [(0x4C, 0)])
    add(0x0079C820, mem, [
        (slots[0x18], 1, "1"), (slots[0x18], 1, "1"), (self_slots[0x4C], 0, "66"), (slots[0x20], 1, "4"),
    ], ["@1"])

    # 00798F80 retaliate machine: early bit, then the full xfer.
    mem = Mem(); mem.add(); xfer = mem.add(); mem.w(xfer, 4, 8)
    add(0x00798F80, mem, [], ["@1"])
    mem = Mem(); mem.add(); xfer = mem.add()
    slots = V(mem).attach(xfer, [(0x18, 4), (0x58, 4)])
    add(0x00798F80, mem, [
        (slots[0x18], 1, "1"), (0x005C0A40, 1, "1"), (0x0054A090, 2, "1"), (slots[0x58], 1, "8"),
    ], ["@1"])

    # 0079FC40 move-on-path xfer, global absent and present.
    mem = Mem(); mem.add(); xfer = mem.add()
    slots = V(mem).attach(xfer, [(0x18, 4)])
    add(0x0079FC40, mem, [(slots[0x18], 1, "1"), (slots[0x18], 1, "5")], ["@1"])
    mem = Mem(); mem.add(); xfer = mem.add(); root = mem.add()
    v = V(mem)
    slots = v.attach(xfer, [(0x18, 4)])
    root_slots = v.attach(root, [(0x158, 8)])
    add(0x0079FC40, mem, [
        (slots[0x18], 1, "1"), (slots[0x18], 1, "1"), (root_slots[0x158], 2, "6"),
    ], ["@1"], table=f"@{root}")

    # 0079A870 horde-exit update.
    mem = Mem(); state, machine = mem.add(), mem.add(); mem.link(state, 0x18, machine)
    add(0x0079A870, mem, [(0x005B9F30, 0, "1")])
    mem, *_rest, unit = rig(); goal, loco = mem.add(), mem.add(); mem.link(goal, 0x374, loco)
    slots = V(mem).attach(loco, [(0x1F8, 4)])
    add(0x0079A870, mem, [(0x005B9F30, 0, f"@{goal}"), (slots[0x1F8], 1, "2")])
    mem, *_u = rig(); goal, contain, contain2, member = mem.add(), mem.add(), mem.add(), mem.add()
    v = V(mem)
    cs = v.attach(contain, [(0x84, 0)])
    c2 = v.attach(contain2, [(0x7C, 0)])
    ms = v.attach(member, [(0, 0), (4, 8)])
    add(0x0079A870, mem, [
        (0x005B9F30, 0, f"@{goal}"), (0x006A7600, 0, f"@{contain}"), (cs[0x84], 0, "1"),
        (0x006A7600, 0, "1"), (0x006A7600, 0, f"@{contain2}"), (c2[0x7C], 0, f"@{member}"),
        (ms[0], 0, "0"), (ms[4], 2, "ffffffff"),
    ])
    mem, *_u = rig()
    goal, contain, contain2, member, horde = (mem.add() for _ in range(5))
    v = V(mem)
    cs = v.attach(contain, [(0x84, 0)])
    c2 = v.attach(contain2, [(0x7C, 0)])
    ms = v.attach(member, [(0, 0), (4, 8), (8, 8)])
    hs = v.attach(horde, [(0xFC, 0)])
    add(0x0079A870, mem, [
        (0x005B9F30, 0, f"@{goal}"), (0x006A7600, 0, f"@{contain}"), (cs[0x84], 0, f"@{horde}"),
        (0x006A7600, 0, "1"), (0x006A7600, 0, f"@{contain2}"), (c2[0x7C], 0, f"@{member}"),
        (ms[0], 0, "0"), (ms[4], 2, "3"), (ms[8], 2, "0"), (hs[0xFC], 0, "0"), (0x006EFE40, 0, "1"),
    ])

    # 0079F610 horde-enter onEnter. The 0x471 case is the mutation probe.
    mem, *_u = rig()
    add(0x0079F610, mem, [(0x005B9F30, 0, "0")])
    mem, *_u = rig(); goal = mem.add(); mem.b(goal, 0x471, 1)
    add(0x0079F610, mem, [(0x005B9F30, 0, f"@{goal}")])
    mem, *_u = rig()
    goal, contain, member, horde = mem.add(), mem.add(), mem.add(), mem.add()
    v = V(mem)
    cs = v.attach(contain, [(0x84, 0)])
    ms = v.attach(member, [(0x84, 0), (0x94, 12)])
    add(0x0079F610, mem, [
        (0x005B9F30, 0, f"@{goal}"), (0x006A7600, 0, f"@{contain}"), (cs[0x84], 0, f"@{horde}"),
        (0x006A7620, 2, f"@{member}"), (ms[0x84], 0, "0"), (ms[0x94], 3, "0"),
    ])
    mem, *_u = rig()
    goal, contain, member, horde, template = (mem.add() for _ in range(5))
    mem.link(goal, 4, template); mem.w(template, 0xC0, 0x10)
    v = V(mem)
    cs = v.attach(contain, [(0x84, 0)])
    ms = v.attach(member, [(0x84, 0), (0x94, 12), (0x98, 4), (0x40, 8)])
    hs = v.attach(horde, [(0x7C, 12)])
    add(0x0079F610, mem, [
        (0x005B9F30, 0, f"@{goal}"), (0x006A7600, 0, f"@{contain}"), (cs[0x84], 0, f"@{horde}"),
        (0x006A7620, 2, f"@{member}"), (ms[0x84], 0, "0"), (ms[0x94], 3, "1"),
        (hs[0x7C], 3, "0"), (ms[0x98], 1, "0"), (ms[0x40], 2, "0"),
    ])

    # 0079AB10 exit update.
    mem, *_u = rig()
    add(0x0079AB10, mem, [(0x005B9F30, 0, "0")])
    mem, *_u = rig(); goal, loco = mem.add(), mem.add(); mem.link(goal, 0x374, loco)
    slots = V(mem).attach(loco, [(0x1F8, 4)])
    add(0x0079AB10, mem, [(0x005B9F30, 0, f"@{goal}"), (slots[0x1F8], 1, "2")])
    mem, state, machine, unit = rig()
    goal, contain, member, head = mem.add(), mem.add(), mem.add(), mem.add()
    mem.w(state, 4, 9); mem.link(machine, 4, head); mem.w(head, 4, 9)
    v = V(mem)
    cs = v.attach(contain, [(0x7C, 0)])
    ms = v.attach(member, [(0, 0), (4, 8), (8, 8)])
    add(0x0079AB10, mem, [
        (0x005B9F30, 0, f"@{goal}"), (0x006A7600, 0, "1"), (0x006A7600, 0, f"@{contain}"),
        (cs[0x7C], 0, f"@{member}"), (ms[0], 0, "0"), (ms[4], 2, "3"), (ms[8], 2, "0"),
    ])

    # 0079CDA0 return-for-landing onExit.
    mem, *_u = rig()
    add(0x0079CDA0, mem, [
        (0x006D2020, 2, "1"), (0x005B9F30, 0, "0"), (0x00986150, 1, "55"), (0x00687460, 1, "0"),
    ], ["0"])
    mem, state, machine, unit = rig()
    goal, producer, module, record, child = (mem.add() for _ in range(5))
    mem.w(goal, 0xDC, 0x21); mem.w(unit, 0xDC, 0x22); mem.link(module, 4, record); mem.w(record, 0x10, 0x33)
    mem.link(state, 0x20, child)
    v = V(mem)
    ps = v.attach(producer, [(0x54, 4), (0x18, 4)])
    cs = v.attach(child, [(0, 4)])
    add(0x0079CDA0, mem, [
        (0x006D2020, 2, "1"), (0x005B9F30, 0, f"@{goal}"), (0x006E4760, 2, f"@{producer}"),
        (ps[0x54], 1, "1"), (ps[0x18], 1, "1"), (0x00986150, 1, "55"), (0x00687460, 1, f"@{module}"),
        (0x006C1D60, 1, "1"), (cs[0], 1, "7"),
    ], ["0"])

    # 0079E9C0 end waypoint path.
    mem, state, machine, unit, _loco = loco_rig(); path = mem.add(); mem.link(state, 0x20, path)
    slots = V(mem).attach(path, [(0x4C, 0)])
    add(0x0079E9C0, mem, [(slots[0x4C], 0, "0"), (0x006B6E30, 1, "1")])
    mem, state, *_r, unit, loco = loco_rig()
    path, table = mem.add(), mem.add()
    mem.link(state, 0x20, path); mem.link(path, 0x80, table)
    mem.w(table, 0x4C, 1); mem.w(table, 0x20, 0x42)
    slots = V(mem).attach(path, [(0x4C, 0)])
    add(0x0079E9C0, mem, [(0x005A94C0, 4, "0"), (slots[0x4C], 0, "0")])

    # 0079F030 follow waypoint, and 0079F150 which calls it.
    mem, state, *_u = rig(); path = mem.add()
    slots = V(mem).attach(state, [(0x4C, 0)])
    add(0x0079F030, mem, [(slots[0x4C], 0, f"@{path}"), (0x0079EF90, 0, "0")])
    mem, state, *_u = rig(); path, coords = mem.add(), mem.add()
    mem.w(coords, 0, 0x11111111); mem.w(coords, 4, 0x22222222); mem.w(path, 0x68, 0x44)
    v = V(mem)
    ss = v.attach(state, [(0x4C, 0), (0x50, 4)])
    ps = v.attach(path, [(0x4C, 0), (0x1C, 4)])
    add(0x0079F030, mem, [
        (ss[0x4C], 0, f"@{path}"), (0x0079EF90, 0, "99"), (ss[0x50], 1, f"@{coords}"),
        (ps[0x4C], 0, "0"), (ps[0x1C], 1, "7"),
    ])
    mem, state, *_u, loco = loco_rig()
    add(0x0079F150, mem, [(INNER, 0, "fffffffe")])
    mem, state, *_u, loco = loco_rig(); path, link = mem.add(), mem.add()
    mem.link(state, 0x20, path); mem.link(loco, 0x214, link); mem.w(link, 0x40, 1)
    slots = V(mem).attach(path, [(0x48, 4)])
    add(0x0079F150, mem, [(INNER, 0, "0"), (slots[0x48], 1, "1")])

    # 00797B80 / 00797C00 aim force-fire.
    mem, *_r, unit, loco = loco_rig(); mem.w(loco, 0x218, 0x0C)
    slots = V(mem).attach(loco, [(0x288, 0)])
    add(0x00797B80, mem, [
        (0x006D2020, 2, "1"), (slots[0x288], 0, "2"), (0x007527C0, 1, "1"), (0x006A4EA0, 0, "1"),
    ])
    mem, *_r, unit, loco = loco_rig(); mem.w(loco, 0x218, 3); goal = mem.add()
    slots = V(mem).attach(loco, [(0x288, 0)])
    add(0x00797B80, mem, [
        (0x006D2020, 2, "1"), (slots[0x288], 0, "0"), (0x005B9F30, 0, f"@{goal}"),
        (0x006C0F40, 1, "1"), (0x007527C0, 1, "0"), (0x006A4EA0, 0, "1"),
    ])
    mem, state, *_u = rig(); mem.w(state, 0x20, 0xFFFFFFFF)
    add(0x00797C00, mem, [(0x006D2020, 2, "1"), (0x006D2020, 2, "4")], ["0"])
    mem, state, *_u, loco = loco_rig(); mem.w(state, 0x20, 3)
    add(0x00797C00, mem, [(0x006D2020, 2, "1"), (0x007527C0, 1, "1"), (0x006D2020, 2, "4")], ["0"])

    # 00797FA0 end path move.
    mem, state, *_u = rig(); path = mem.add(); mem.link(state, 0x20, path); mem.w(path, 0x80, 5)
    slots = V(mem).attach(path, [(0x48, 0), (0x4C, 0)])
    add(0x00797FA0, mem, [(slots[0x48], 0, "0"), (slots[0x4C], 0, "0")])
    mem, state, *_u, loco = loco_rig(); path = mem.add(); mem.link(state, 0x20, path)
    slots = V(mem).attach(path, [(0x48, 0), (0x4C, 0)])
    add(0x00797FA0, mem, [
        (slots[0x48], 0, "1"), (0x00683140, 1, "1"), (slots[0x4C], 0, "1"), (0x006F19F0, 0, "1"),
    ])

    # SA lookup states.
    mem, *_u = rig(); add(0x00797660, mem, [(0x007E2950, 1, "0")])
    mem, state, machine, unit = rig(); mem.w(machine, 0x80, 0x42); module = mem.add()
    slots = V(mem).attach(module, [(0x64, 0)])
    add(0x00797660, mem, [(0x007E2950, 1, "1"), (0x006878B0, 1, f"@{module}"), (slots[0x64], 0, "0")])
    mem, state, machine, unit = rig(); module = mem.add()
    slots = V(mem).attach(module, [(0x64, 0), (0x68, 4)])
    add(0x00797660, mem, [
        (0x007E2950, 1, "1"), (0x006878B0, 1, f"@{module}"), (slots[0x64], 0, "1"), (slots[0x68], 1, "1"),
    ])
    mem, *_u = rig(); add(0x00797760, mem, [(0x007E2950, 1, "0")])
    mem, *_u = rig(); module = mem.add(); slots = V(mem).attach(module, [(0x6C, 0)])
    add(0x00797760, mem, [
        (0x007E2950, 1, "1"), (0x006878B0, 1, f"@{module}"), (slots[0x6C], 0, "1"), (0x00684BD0, 0, "0"),
    ])
    mem, *_u = rig(); module = mem.add(); slots = V(mem).attach(module, [(0x6C, 0)])
    add(0x00797760, mem, [
        (0x007E2950, 1, "1"), (0x006878B0, 1, f"@{module}"), (slots[0x6C], 0, "1"),
        (0x00684BD0, 0, "1"), (0x00684C00, 0, "1"),
    ])
    mem, *_u = rig(); add(0x007977C0, mem, [(0x007E2950, 1, "0")])
    mem, *_u = rig(); module = mem.add()
    add(0x007977C0, mem, [(0x007E2950, 1, "1"), (0x006878B0, 1, f"@{module}"), (0x0069FC10, 0, "0")])
    mem, *_u = rig(); module = mem.add(); slots = V(mem).attach(module, [(0x58, 0)])
    add(0x007977C0, mem, [
        (0x007E2950, 1, "1"), (0x006878B0, 1, f"@{module}"), (0x0069FC10, 0, "1"), (slots[0x58], 0, "1"),
    ])
    mem, *_u = rig(); add(0x0079D930, mem, [(0x007E2950, 1, "0")])
    mem, state, machine, unit = rig(); mem.w(machine, 0x80, 7)
    first, second = mem.add(), mem.add()
    v = V(mem)
    a = v.attach(first, [(0x60, 0)])
    b = v.attach(second, [(0x70, 0)])
    add(0x0079D930, mem, [
        (0x007E2950, 1, "1"), (0x006878B0, 1, f"@{first}"), (a[0x60], 0, "1"),
        (0x007E2950, 1, "1"), (0x006878B0, 1, f"@{second}"), (b[0x70], 0, "1"),
    ])
    mem, *_u = rig(); add(0x0079D9B0, mem, [(0x007E2950, 1, "0")])
    mem, *_u = rig(); module, record = mem.add(), mem.add(); mem.link(module, 4, record)
    add(0x0079D9B0, mem, [(0x007E2950, 1, "1"), (0x006878B0, 1, f"@{module}")])
    mem, *_u = rig(); module, record = mem.add(), mem.add()
    mem.link(module, 4, record); mem.w(record, 0x2C, 0x40000000)
    add(0x0079D9B0, mem, [(0x007E2950, 1, "1"), (0x006878B0, 1, f"@{module}"), (0x00718FA0, 1, "0")])
    mem, *_u = rig(); module, record = mem.add(), mem.add()
    mem.link(module, 4, record); mem.w(record, 0x2C, 0x40000000)
    add(0x0079D9B0, mem, [(0x007E2950, 1, "1"), (0x006878B0, 1, f"@{module}"), (0x00718FA0, 1, "1")])

    # 0079E960 special power.
    mem, state, machine, unit = rig(); mem.w(machine, 0x70, 0x15)
    add(0x0079E960, mem, [(0x007E2950, 1, "0"), (0x006876D0, 1, "1"), (0x006C0F40, 1, "1")])
    mem, state, machine, unit = rig(); found, record = mem.add(), mem.add()
    mem.link(found, 8, record); mem.w(record, 0x1C, 1 << 26)
    add(0x0079E960, mem, [
        (0x007E2950, 1, f"@{found}"), (0x006876D0, 1, "1"), (0x006C0F40, 1, "1"),
    ])
    mem, *_u = rig(); found, record, goal = mem.add(), mem.add(), mem.add()
    mem.link(found, 8, record)
    add(0x0079E960, mem, [
        (0x007E2950, 1, f"@{found}"), (0x005B9F30, 0, f"@{goal}"), (0x006876D0, 1, "1"), (0x006C0F40, 1, "1"),
    ])

    # 0079FF10 move-to-goal onExit.
    mem, *_u = rig()
    add(0x0079FF10, mem, [], ["0"])
    mem, *_r, unit = rig(); mem.w(unit, 0x90, 1 << 29)
    add(0x0079FF10, mem, [(0x006D2020, 2, "9")], ["0"])
    mem, state, *_u, loco = loco_rig(); child = mem.add(); mem.link(state, 0x20, child)
    slots = V(mem).attach(child, [(0, 4)])
    add(0x0079FF10, mem, [(slots[0], 1, "1"), (0x006B7040, 2, "1"), (0x00682D70, 0, "4")], ["0"])

    # 0079A310 wait-until-finished-firing.
    mem, *_u = rig()
    add(0x0079A310, mem, [(0x006A51A0, 1, "0")])
    mem, *_u = rig(); weapon = mem.add()
    add(0x0079A310, mem, [
        (0x006A51A0, 1, f"@{weapon}"), (0x00686460, 0, "ffffffff"), (0x00688810, 0, "0"),
        (0x00686460, 0, "ffffffff"),
    ])
    mem, *_u = rig(); weapon = mem.add(); logic = frame_of(mem, 1)
    add(0x0079A310, mem, [
        (0x006A51A0, 1, f"@{weapon}"), (0x00686460, 0, "5"), (0x00686460, 0, "5"),
        (0x00688830, 0, "0"), (0x00685C20, 0, "5"),
    ], gl=f"@{logic}")

    # 0079A680 dock update. 005B9F30's ecx is the child.
    mem, state, *_u = rig(); child, link = mem.add(), mem.add()
    mem.link(state, 0x20, child); mem.link(child, 4, link); mem.w(link, 4, 5)
    slots = V(mem).attach(child, [(0x10, 0)])
    add(0x0079A680, mem, [(slots[0x10], 0, "3")])
    mem, state, *_u = rig(); child = mem.add(); mem.link(state, 0x20, child)
    slots = V(mem).attach(child, [(0x10, 0)])
    add(0x0079A680, mem, [(0x005B9F30, 0, "0"), (slots[0x10], 0, "0")])
    mem, state, *_u = rig(); child, goal = mem.add(), mem.add(); mem.link(state, 0x20, child)
    add(0x0079A680, mem, [(0x005B9F30, 0, f"@{goal}"), (0x00686FA0, 0, "1"), (0x00686FA0, 0, "2")])

    # 0079AD70 guard update.
    mem, *_u = rig(); add(0x0079AD70, mem, [])
    mem, state, *_u = rig(); child = mem.add(); mem.link(state, 0x20, child)
    slots = V(mem).attach(child, [(0x10, 0)])
    add(0x0079AD70, mem, [
        (0x00687CB0, 0, "0"), (0x005AA240, 1, "1"), (slots[0x10], 0, "6"), (0x005AA250, 0, "1"),
    ])
    mem, state, *_u = rig(); child, held = mem.add(), mem.add(); mem.link(state, 0x20, child)
    add(0x0079AD70, mem, [(0x00687CB0, 0, f"@{held}"), (0x00A780D0, 0, "1")])

    # 0079A620 dock onExit.
    mem, *_u = rig(); add(0x0079A620, mem, [], ["0"])
    mem, state, *_u, loco = loco_rig(); child = mem.add(); mem.link(state, 0x20, child); mem.b(state, 0x25, 9)
    slots = V(mem).attach(child, [(0x30, 0), (0, 4)])
    add(0x0079A620, mem, [(slots[0x30], 0, "1"), (slots[0], 1, "2"), (0x006E6360, 1, "3")], ["0"])

    # 00799C30 cower onExit.
    mem = Mem(); state, machine = mem.add(), mem.add(); mem.link(state, 0x18, machine)
    add(0x00799C30, mem, [(0x009CCDA0, 1, "15")], ["7"])
    mem, *_u = rig()
    add(0x00799C30, mem, [(0x009CCDA0, 1, "15"), (0x006F17F0, 1, "1"), (0x006A7600, 0, "0")], ["7"])
    mem, *_u = rig(); contain, member = mem.add(), mem.add()
    v = V(mem)
    cs = v.attach(contain, [(0x84, 0)])
    ms = v.attach(member, [(0x1A0, 4)])
    add(0x00799C30, mem, [
        (0x009CCDA0, 1, "15"), (0x006F17F0, 1, "1"), (0x006A7600, 0, "1"),
        (0x006A7600, 0, f"@{contain}"), (cs[0x84], 0, f"@{member}"), (ms[0x1A0], 1, "8"),
    ], ["7"])

    # 00799A20 move-to slot 19.
    mem = Mem(); state = mem.add(); mem.b(state, 0x4D, 1)
    add(0x00799A20, mem, [])
    mem, state, machine, unit = rig(); mem.w(machine, 0x2C, 1); mem.w(machine, 0x30, 2); mem.w(machine, 0x34, 3)
    add(0x00799A20, mem, [(0x005B9F30, 0, "0"), (0x00799920, 0, "101")])
    mem, state, machine, unit = rig(); goal = mem.add()
    mem.w(goal, 0x38, 4); mem.w(goal, 0x3C, 5); mem.w(goal, 0x40, 6)
    add(0x00799A20, mem, [
        (0x005B9F30, 0, "1"), (0x005B9F30, 0, f"@{goal}"), (0x006A72A0, 1, "1"), (0x00799920, 0, "101"),
    ])

    # 0079B720 pick up crate.
    mem, *_u = rig(); add(0x0079B720, mem, [(0x006E5CE0, 0, "0")])
    mem, state, machine, unit = rig(); found = mem.add(); slots = V(mem).attach(machine, [(0x38, 4)])
    add(0x0079B720, mem, [(0x006E5CE0, 0, f"@{found}"), (slots[0x38], 1, "1"), (0x005B9F30, 0, "0")])
    mem, state, machine, unit = rig(); found, goal = mem.add(), mem.add()
    mem.w(goal, 0x38, 8); mem.w(goal, 0x3C, 9); mem.w(goal, 0x40, 10)
    slots = V(mem).attach(machine, [(0x38, 4)])
    add(0x0079B720, mem, [(0x006E5CE0, 0, f"@{found}"), (slots[0x38], 1, "1"), (0x005B9F30, 0, f"@{goal}")])

    # 00798560 reset enter.
    mem, *_u = rig(); add(0x00798560, mem, [(0x005B9F30, 0, "0")])
    mem, *_u = rig(); goal = mem.add()
    add(0x00798560, mem, [(0x005B9F30, 0, f"@{goal}"), (0x006A7620, 2, "0")])
    mem, *_u, loco = loco_rig(); goal, member = mem.add(), mem.add()
    slots = V(mem).attach(member, [(0x40, 8)])
    add(0x00798560, mem, [
        (0x005B9F30, 0, f"@{goal}"), (0x006A7620, 2, f"@{member}"), (slots[0x40], 2, "1"), (0x00683140, 1, "1"),
    ])

    # 0079A800 / 00798310 horde exit onEnter and force-members.
    for va, with_loco in ((0x0079A800, True), (0x00798310, False)):
        mem, *_u = rig(); add(va, mem, [(0x005B9F30, 0, "0")])
        mem, *_u = (loco_rig() if with_loco else (*rig(), None))[:5] if False else rig()
    mem, *_u, loco = loco_rig(); goal, contain = mem.add(), mem.add()
    slots = V(mem).attach(contain, [(0x84, 0)])
    add(0x0079A800, mem, [
        (0x005B9F30, 0, f"@{goal}"), (0x006A7600, 0, f"@{contain}"), (slots[0x84], 0, "1"),
        (0x006A7600, 0, "0"), (0x006E6360, 1, "1"),
    ])
    mem, *_u = rig(); goal, contain, other = mem.add(), mem.add(), mem.add()
    v = V(mem)
    cs = v.attach(contain, [(0x84, 0)])
    os_ = v.attach(other, [(0x40, 8)])
    add(0x00798310, mem, [
        (0x005B9F30, 0, f"@{goal}"), (0x006A7600, 0, f"@{contain}"), (cs[0x84], 0, "1"),
        (0x006A7600, 0, f"@{other}"), (os_[0x40], 2, "1"),
    ])
    mem, *_u = rig(); add(0x00798310, mem, [(0x005B9F30, 0, "0")])

    # 0079A780 horde-enter update.
    mem, *_u = rig(); add(0x0079A780, mem, [(0x005B9F30, 0, "0")])
    mem, *_u = rig(); goal, contain, horde = mem.add(), mem.add(), mem.add()
    v = V(mem)
    cs = v.attach(contain, [(0x84, 0)])
    hs = v.attach(horde, [(0xF8, 0), (0x100, 4)])
    add(0x0079A780, mem, [
        (0x005B9F30, 0, f"@{goal}"), (0x006A7600, 0, f"@{contain}"), (cs[0x84], 0, f"@{horde}"),
        (hs[0xF8], 0, "1"),
    ])
    mem, *_u = rig(); goal, contain, horde = mem.add(), mem.add(), mem.add()
    v = V(mem)
    cs = v.attach(contain, [(0x84, 0)])
    hs = v.attach(horde, [(0xF8, 0), (0x100, 4)])
    add(0x0079A780, mem, [
        (0x005B9F30, 0, f"@{goal}"), (0x006A7600, 0, f"@{contain}"), (cs[0x84], 0, f"@{horde}"),
        (hs[0xF8], 0, "0"), (hs[0x100], 1, "1"),
    ])

    # 0079F6F0 horde-enter onExit.
    mem, *_u = rig(); add(0x0079F6F0, mem, [(0x005B9F30, 0, "0")], ["0"])
    mem, *_u = rig(); goal, template = mem.add(), mem.add(); mem.link(goal, 4, template); mem.w(template, 0xC0, 0x10)
    add(0x0079F6F0, mem, [(0x005B9F30, 0, f"@{goal}"), (0x006A7600, 0, "0")], ["0"])
    mem, *_u = rig(); goal, template, contain = mem.add(), mem.add(), mem.add()
    mem.link(goal, 4, template)
    slots = V(mem).attach(contain, [(0x40, 8)])
    add(0x0079F6F0, mem, [
        (0x005B9F30, 0, f"@{goal}"), (0x00706E00, 1, "1"), (0x006A7600, 0, "1"),
        (0x006A7600, 0, f"@{contain}"), (slots[0x40], 2, "5"),
    ], ["0"])

    # 0079AAC0 exit onEnter.
    mem, *_u = rig(); add(0x0079AAC0, mem, [(0x005B9F30, 0, "0")])
    mem, *_u = rig(); goal, contain = mem.add(), mem.add(); mem.w(goal, 0xDC, 0x61)
    slots = V(mem).attach(contain, [(0x40, 8)])
    add(0x0079AAC0, mem, [
        (0x005B9F30, 0, f"@{goal}"), (0x006A7600, 0, f"@{contain}"), (slots[0x40], 2, "1"),
    ])

    # 0079A9B0 horde-exit onExit. The machine virtual is the tail.
    mem, state, machine, unit = rig()
    slots = V(mem).attach(machine, [(0x38, 4)])
    add(0x0079A9B0, mem, [(0x005B9F30, 0, "0"), (slots[0x38], 1, "4")], ["0"])
    mem, state, machine, unit, loco = loco_rig(); goal, contain = mem.add(), mem.add()
    v = V(mem)
    ms = v.attach(machine, [(0x38, 4)])
    cs = v.attach(contain, [(0x40, 8)])
    add(0x0079A9B0, mem, [
        (0x005B9F30, 0, f"@{goal}"), (0x006A7600, 0, "1"), (0x006A7600, 0, f"@{contain}"),
        (cs[0x40], 2, "1"), (0x006E6360, 1, "1"), (ms[0x38], 1, "4"),
    ], ["0"])

    # 0079CFD0 deliver ore, 0079D2A0 dock-with-node.
    mem, *_u = rig(); add(0x0079CFD0, mem, [(0x005B9F30, 0, "0")], ["0"])
    mem, state, *_u, loco = loco_rig(); goal, child = mem.add(), mem.add()
    mem.w(unit_dc := 0, 0, 0)
    mem.link(state, 0x20, child); mem.w(2, 0xDC, 0x19)
    slots = V(mem).attach(child, [(0, 4)])
    add(0x0079CFD0, mem, [
        (0x005B9F30, 0, f"@{goal}"), (0x006E6360, 1, "1"), (0x0067E860, 2, "1"), (slots[0], 1, "6"),
    ], ["0"])
    mem, *_u = rig(); add(0x0079D2A0, mem, [(0x005B9F30, 0, "0")], ["0"])
    mem, state, *_u = loco_rig(); goal, miner, child = mem.add(), mem.add(), mem.add()
    mem.link(goal, 0x398, miner); mem.link(state, 0x20, child); mem.w(2, 0xDC, 0x19)
    slots = V(mem).attach(child, [(0, 4)])
    add(0x0079D2A0, mem, [
        (0x005B9F30, 0, f"@{goal}"), (0x006E6360, 1, "1"), (0x0067E860, 2, "1"),
        (0x0067E410, 1, "1"), (slots[0], 1, "6"),
    ], ["1"])

    # 0079EBA0 enter-goal onExit, 0079EC10 exit-goal onExit.
    mem, *_u = rig(); add(0x0079EBA0, mem, [(0x006D2020, 2, "3")], ["0"])
    mem, state, *_u, loco = loco_rig(); link, child = mem.add(), mem.add()
    mem.link(loco, 0x214, link); mem.w(link, 0x40, 0xF); mem.link(state, 0x20, child)
    slots = V(mem).attach(child, [(0, 4)])
    add(0x0079EBA0, mem, [(0x006E6360, 1, "1"), (0x006D2020, 2, "1"), (slots[0], 1, "5")], ["0"])
    mem, state, machine, unit = rig(); slots = V(mem).attach(machine, [(0x38, 4)])
    add(0x0079EC10, mem, [(0x005B9F30, 0, "0"), (slots[0x38], 1, "2")], ["0"])
    mem, state, machine, unit = rig(); goal, contain, child = mem.add(), mem.add(), mem.add()
    mem.link(state, 0x20, child)
    v = V(mem)
    ms = v.attach(machine, [(0x38, 4)])
    cs = v.attach(contain, [(0x40, 8)])
    ch = v.attach(child, [(0, 4)])
    add(0x0079EC10, mem, [
        (0x005B9F30, 0, f"@{goal}"), (0x006A7600, 0, f"@{contain}"), (cs[0x40], 2, "1"),
        (ms[0x38], 1, "2"), (ch[0], 1, "5"),
    ], ["0"])

    # 0079F290 force-attack onExit.
    mem, *_u = rig()
    add(0x0079F290, mem, [
        (0x006D2020, 2, "1"), (0x006F0440, 0, "1"), (0x006C0F40, 1, "1"), (0x0067D870, 1, "0"),
    ], ["0"])
    mem, state, *_u = rig(); found, child = mem.add(), mem.add(); mem.link(state, 0x20, child)
    v = V(mem)
    fs = v.attach(found, [(4, 4)])
    cs = v.attach(child, [(0, 4)])
    add(0x0079F290, mem, [
        (0x006D2020, 2, "1"), (0x006F0440, 0, "1"), (0x006C0F40, 1, "1"), (0x0067D870, 1, f"@{found}"),
        (fs[4], 1, "1"), (cs[0], 1, "8"),
    ], ["0"])

    # 0079E190 combined unit.
    mem, *_u = rig()
    add(0x0079E190, mem, [(0x005B9F30, 0, "0"), (0x007000E0, 0, "1")])
    mem, state, *_u, loco = loco_rig(); goal, nested = mem.add(), mem.add(); mem.w(nested, 0x68, 0x12)
    v = V(mem)
    ss = v.attach(state, [(0x4C, 0)])
    ns = v.attach(nested, [(0x38, 4), (0x1C, 4)])
    add(0x0079E190, mem, [
        (0x005B9F30, 0, f"@{goal}"), (ss[0x4C], 0, f"@{nested}"), (ns[0x38], 1, "1"), (ns[0x1C], 1, "7"),
    ])

    # 0079AFD0 guard-object onEnter.
    mem, state, machine, unit, loco = loco_rig()
    slots = V(mem).attach(loco, [(0x22C, 0), (0x230, 0)])
    add(0x0079AFD0, mem, [(slots[0x22C], 0, "0"), (slots[0x230], 0, "0")])
    mem, state, machine, unit, loco = loco_rig(); created = mem.add()
    v = V(mem)
    ls = v.attach(loco, [(0x22C, 0), (0x23C, 0)])
    ms = v.attach(machine, [(0x24, 4)])
    add(0x0079AFD0, mem, [
        (ls[0x22C], 0, "1"), (ms[0x24], 1, f"@{created}"), (ls[0x23C], 0, "4"), (0x00682F20, 1, "1"),
    ])
    mem, state, machine, unit, loco = loco_rig(); created = mem.add(); mem.w(created, 0x68, 0x44)
    v = V(mem)
    ls = v.attach(loco, [(0x22C, 0), (0x23C, 0)])
    ms = v.attach(machine, [(0x24, 4)])
    cs = v.attach(created, [(0x14, 0), (0x1C, 4)])
    add(0x0079AFD0, mem, [
        (ls[0x22C], 0, "1"), (ms[0x24], 1, f"@{created}"), (ls[0x23C], 0, "0"),
        (cs[0x14], 0, "0"), (cs[0x1C], 1, "9"),
    ])


def norm(value: int, bases: list[int], capture: bool, read) -> str:
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
            raise RuntimeError("aistate79bcheck failed")


def line_for(case: dict) -> str:
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
    text = f"{target:08x}:" + norm(ecx, box["bases"], False, active.read)
    text += ":" + "+".join(norm(arg, box["bases"], False, active.read) for arg in args)
    box["log"].append(text)
    return spec["ret"]


def bind(emu: Emulator) -> None:
    for cleanup in (0, 4, 8, 12):
        for index in range(16):
            address = emu.bind_stub(on_call, cleanup)
            if address != stub_addr(cleanup, index):
                raise RuntimeError(f"stub pool 0x{address:08x}")
    for target, cleanup in CLEANUP.items():
        emu.bind_function(target, on_call, cleanup)


def resolve_token(token: str, bases: list[int]) -> int:
    if token.startswith("@"):
        return bases[int(token[1:])]
    return int(token, 16)


def run_case(emu: Emulator, checker: Checker, case: dict, index: int) -> None:
    emu.reset_data()
    emu._function_stubs.pop(INNER, None)
    if case["va"] == 0x0079F150:
        emu.bind_function(INNER, on_call, 0)
    mem: Mem = case["mem"]
    bases = [emu.alloc(bytes(blob), alignment=256) for blob in mem.blobs]
    for obj, offset, target in mem.fix:
        emu.write(bases[obj] + offset, struct.pack("<I", bases[target]))
    gl = 0 if case["gl"] == "0" else bases[int(case["gl"][1:])]
    table = 0 if case["table"] == "0" else bases[int(case["table"][1:])]
    emu.write(GAME_LOGIC, struct.pack("<I", gl))
    emu.write(LOGIC_TABLE, struct.pack("<I", table))
    emu.write(LOGIC_TABLE_2, struct.pack("<I", table))
    emu.write(LIMIT_ABS, struct.pack("<I", case["limit"]))
    emu.write(MAX_ABS, struct.pack("<I", case["maxf"]))
    emu.write(ONE_ABS, struct.pack("<I", case["one"]))
    box = emu.box
    box["bases"] = bases
    box["log"] = []
    box["pending"] = [{
        "target": target, "argc": argc, "ret": resolve_token(token, bases),
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
        problems.append(f"calls\n  native {native_log}\n  port   {port_log}")
    for obj, (native, port) in enumerate(zip(native_mem, port_mem)):
        left, right = canon(native, bases), canon(port, port_bases)
        if left != right:
            offset = next(i for i, (a, b) in enumerate(zip(left, right)) if a != b)
            problems.append(
                f"obj{obj}+0x{offset:x} {left[offset:offset+4].hex()} != {right[offset:offset+4].hex()}")
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
    thin = [va for va, n in sorted(counts.items()) if n < 2]
    if missing or extra or thin or len(counts) != 48:
        raise SystemExit(f"coverage {len(counts)} missing {missing} extra {extra} thin {thin}")
    executable = Path(sys.argv[1]) if len(sys.argv) > 1 else (
        Path(__file__).resolve().parents[3] / "build/win32/bin/Release/ra3_aistate79bcheck.exe")
    emu = Emulator()
    emu.box = {}
    bind(emu)
    checker = Checker(executable)
    try:
        for index, case in enumerate(CASES):
            run_case(emu, checker, case, index)
    finally:
        checker.close()
    print(f"aistate79b oracle Release cases={len(CASES)} functions={len(counts)} mismatches=0")
    return 0


if __name__ == "__main__":
    sys.exit(main())
