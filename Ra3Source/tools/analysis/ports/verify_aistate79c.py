"""Release oracle for the third 0x00790000 AI-state pass.

Each case runs the shipped bytes in Unicorn and ra3_aistate79ccheck. EAX, object
bytes and the normalized call trace must match. Direct callees are stubbed, including
import thunks and the inner-patched 0x00683140 entry. 0x0079F240 is left to fall
into 0x0079DDB0. Stack temporaries compare as ptr.
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
LIMIT_ABS = 0x00BD95B8
MAX_ABS = 0x00BE700C
ONE_ABS = 0x00BD90A4
LIMIT = 0x43C80000
MAXF = 0x7F7FFFFF
ONE = 0x3F800000
EAX_IN = 0xA5A5A5A5
LOW = 0x42C80000
HIGH = 0x43FA0000

POPS = {
    0x00799FE0: 4, 0x0079A140: 4, 0x0079DE60: 4, 0x00799E20: 4, 0x00798890: 4,
    0x0079A700: 4, 0x0079A5C0: 4, 0x00799D90: 4, 0x0079B360: 4, 0x00798370: 4,
    0x0079E210: 4, 0x0079F0C0: 4, 0x0079B080: 4, 0x0079DD50: 4, 0x00797710: 4,
    0x0079B5D0: 4, 0x0079B610: 4, 0x0079C8A0: 4, 0x0079C9F0: 4, 0x0079AD30: 4,
}
CLEANUP = {
    0x00416820: 0, 0x005AA1A0: 0, 0x005AA240: 4, 0x005AA250: 0, 0x005AC4F0: 8,
    0x005B2450: 4, 0x005B9F30: 0, 0x0067D870: 0, 0x0067E3D0: 8, 0x00683140: 4,
    0x00683590: 4, 0x00685EC0: 0, 0x00686460: 0, 0x00687720: 4, 0x006878B0: 4,
    0x0068E1F0: 4, 0x006A51A0: 4, 0x006A51C0: 4, 0x006A5370: 4, 0x006A7600: 0,
    0x006A76A0: 4, 0x006A88C0: 4, 0x006C0F40: 4, 0x006D2020: 8, 0x006D21F0: 4,
    0x006D23F0: 4, 0x006EB6C0: 0, 0x006F0440: 0, 0x0070B000: 4, 0x007483A0: 12,
    0x007527C0: 4, 0x007576C0: 4, 0x00798400: 4, 0x0079DAC0: 4, 0x007E2950: 4,
}
COUNTED = {
    0x0079DDB0, 0x0079AE70, 0x00799FE0, 0x0079A140, 0x007973D0, 0x0079DE60,
    0x00799E20, 0x00798890, 0x00798760, 0x0079F240, 0x0079A700, 0x0079A5C0,
    0x00797810, 0x0079B0C0, 0x007990A0, 0x0079F880, 0x00798060, 0x00797210,
    0x00797F30, 0x00799D90, 0x007976D0, 0x0079B360, 0x00798370, 0x0079E210,
    0x00799530, 0x00799E70, 0x0079F0C0, 0x0079AEE0, 0x0079B080, 0x0079DD50,
    0x00797610, 0x0079A210, 0x0079AD30, 0x00797710, 0x00799CF0, 0x0079B5D0,
    0x0079B610, 0x0079DE30, 0x0079C8A0, 0x00799AB0, 0x0079C9F0, 0x00799780,
    0x007984F0, 0x007998F0, 0x0079E080, 0x0079FEE0, 0x0079FD20,
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


CASES: list[dict] = []


def add(va: int, mem: Mem, rets: list[tuple[int, int, str]], args: list[str] | None = None,
        eax: int = EAX_IN, gl: str = "0", maxf: int = MAXF) -> None:
    CASES.append({
        "va": va, "mem": mem, "rets": rets, "args": args or [], "eax": eax,
        "gl": gl, "table": "0", "limit": LIMIT, "maxf": maxf, "one": ONE,
    })


def at(index: int, offset: int = 0) -> str:
    return f"@{index}" if offset == 0 else f"@{index}+{offset:x}"


def chain() -> Rig:
    return Rig()


def build() -> None:
    # 0079ddb0 attack-object onEnter, with and without the +0x24 goal push.
    for flag, maxf in ((0, MAXF), (1, 0x40000000)):
        rig = chain()
        goal, child, node = rig.obj(), rig.obj(), rig.obj()
        rig.mem.link(goal, 0x33C, node)
        rig.mem.w(rig.state, 0x24, flag)
        rig.mem.w(rig.unit, 0xDC, 0x42)
        rig.mem.w(child, 0x68, 0x68)
        state_vt = rig.vt(rig.state, [(0x4C, 0)])
        child_vt = rig.vt(child, [(0x38, 4), (0x1C, 4)])
        node_vt = rig.vt(node, [(0x28, 4)])
        rets = [
            (0x005B9F30, 0, at(goal)), (state_vt[0x4C], 0, at(child)),
        ]
        if flag == 1:
            rets.append((child_vt[0x38], 1, "0"))
        rets += [
            (0x005AC4F0, 2, "0"), (0x006D21F0, 1, "0"), (node_vt[0x28], 1, "0"),
            (0x006A5370, 1, "99"), (child_vt[0x1C], 1, "7"),
        ]
        add(0x0079DDB0, rig.mem, rets, maxf=maxf)

    # 0079ae70 guard-area onEnter. Slot 0x234 gates the position write; slot 0x14 can fail.
    rig = chain()
    loco, child = rig.obj(), rig.obj()
    rig.mem.link(rig.unit, 0x374, loco)
    rig.mem.w(child, 0x68, 0x11)
    machine_vt = rig.vt(rig.machine, [(0x24, 4)])
    loco_vt = rig.vt(loco, [(0x234, 0), (0x224, 0)])
    child_vt = rig.vt(child, [(0x14, 0), (0x1C, 4)])
    add(0x0079AE70, rig.mem, [
        (machine_vt[0x24], 1, at(child)), (loco_vt[0x234], 0, "0"),
        (child_vt[0x14], 0, "1"), (child_vt[0x1C], 1, "5"),
    ])
    rig = chain()
    loco, child = rig.obj(), rig.obj()
    rig.mem.link(rig.unit, 0x374, loco)
    machine_vt = rig.vt(rig.machine, [(0x24, 4)])
    loco_vt = rig.vt(loco, [(0x234, 0), (0x224, 0)])
    child_vt = rig.vt(child, [(0x14, 0)])
    add(0x0079AE70, rig.mem, [
        (machine_vt[0x24], 1, at(child)), (loco_vt[0x234], 0, "1"),
        (loco_vt[0x224], 0, "12345678"), (0x005AC4F0, 2, "0"), (child_vt[0x14], 0, "fffffffe"),
    ])

    def aim(va: int, arm: int, hold: int, armed: int, held: int, sample: int, with_loco: bool) -> None:
        rig = chain()
        template, row = rig.obj(), rig.obj()
        rig.mem.link(rig.unit, 4, template)
        rig.mem.link(template, 0x14, row)
        rig.mem.w(row, 0x120, sample)
        rig.mem.b(rig.state, arm, armed)
        rig.mem.b(rig.state, hold, held)
        rets: list[tuple[int, int, str]] = []
        if with_loco:
            loco = rig.obj()
            rig.mem.link(rig.unit, 0x374, loco)
            loco_vt = rig.vt(loco, [(0x270, 0)])
            if armed and held:
                rets.append((loco_vt[0x270], 0, "0"))
            if sample == LOW:
                rets.append((0x007527C0, 1, "0"))
        rets.append((0x006D2020, 2, "3"))
        add(va, rig.mem, rets, ["1"])

    for va, arm, hold in ((0x00799FE0, 0x21, 0x22), (0x0079A140, 0x20, 0x21)):
        aim(va, arm, hold, 1, 1, LOW, True)
        aim(va, arm, hold, 0, 0, HIGH, True)
        aim(va, arm, hold, 1, 0, LOW, True)
        aim(va, arm, hold, 1, 1, LOW, False)

    # 007973d0 delivery onEnter.
    rig = chain()
    goal, dock = rig.obj(), rig.obj()
    rig.mem.link(goal, 0x394, dock)
    add(0x007973D0, rig.mem, [(0x005B9F30, 0, at(goal)), (0x0068E1F0, 1, "0")])
    rig = chain()
    goal, dock, miner, templ, node = rig.obj(), rig.obj(), rig.obj(), rig.obj(), rig.obj()
    rig.mem.link(goal, 0x394, dock)
    rig.mem.link(goal, 0x398, miner)
    rig.mem.link(miner, 4, templ)
    rig.mem.link(rig.unit, 0x39C, node)
    rig.mem.w(goal, 0xDC, 0x77)
    rig.mem.w(templ, 0x14, ONE)
    add(0x007973D0, rig.mem, [
        (0x005B9F30, 0, at(goal)), (0x0068E1F0, 1, "1"), (0x0067E3D0, 2, "0"), (0x0070B000, 1, "0"),
    ])

    # 0079de60 attack-object onExit.
    rig = chain()
    child = rig.obj()
    rig.mem.link(rig.state, 0x20, child)
    child_vt = rig.vt(child, [(0, 4)])
    add(0x0079DE60, rig.mem, [
        (0x006F0440, 0, "0"), (0x006C0F40, 1, "0"), (0x0067D870, 1, "0"), (child_vt[0], 1, "8"),
    ], ["1"])
    rig = chain()
    module = rig.obj()
    module_vt = rig.vt(module, [(4, 4)])
    add(0x0079DE60, rig.mem, [
        (0x006F0440, 0, "0"), (0x006C0F40, 1, "0"), (0x0067D870, 1, at(module)), (module_vt[4], 1, "6"),
    ], ["1"])

    def xfer_pair(va: int, second_slot: int | None) -> None:
        for mode in (8, 0):
            rig = Rig.__new__(Rig)
            rig.mem = Mem()
            rig.v = V(rig.mem)
            rig.state = rig.mem.add()
            xfer = rig.mem.add()
            rig.mem.w(xfer, 4, mode)
            slots = [(0x18, 4)]
            if second_slot is not None:
                slots.append((second_slot, 4))
            if va == 0x00799E20:
                slots.append((0x70, 4))
            xfer_vt = rig.v.attach(xfer, slots)
            rets = [(xfer_vt[0x18], 1, "1")]
            if (mode & 8) == 0 and second_slot is not None:
                rets.append((xfer_vt[second_slot], 1, "2"))
                if va == 0x00799E20:
                    rets.append((xfer_vt[0x70], 1, "3"))
            add(va, rig.mem, rets, [at(xfer)])

    xfer_pair(0x00799E20, 0x6C)
    xfer_pair(0x00799D90, 0x6C)

    # 00798890 super xfer. Mode bit 1 creates the nested state.
    for mode, created in ((0, False), (2, True)):
        mem = Mem()
        state, xfer = mem.add(), mem.add()
        mem.w(xfer, 4, mode)
        mem.w(state, 0x20, 0x55)
        v = V(mem)
        state_vt = v.attach(state, [(0x4C, 0)]) if created else {}
        xfer_vt = v.attach(xfer, [(0x18, 4), (0x20, 4)])
        child = mem.add() if created else 0
        rets = [(xfer_vt[0x18], 1, "1")]
        if created:
            rets.append((state_vt[0x4C], 0, at(child)))
        rets.append((xfer_vt[0x20], 1, "9"))
        add(0x00798890, mem, rets, [at(xfer)])

    # 00798760 goal position.
    mem = Mem(); state, machine = mem.add(), mem.add(); mem.link(state, 0x18, machine)
    add(0x00798760, mem, [(0x005B9F30, 0, "0")])
    mem = Mem(); state, machine, goal = mem.add(), mem.add(), mem.add()
    mem.link(state, 0x18, machine)
    mem.w(machine, 0x54, ONE)
    mem.w(goal, 0x38, 1); mem.w(goal, 0x3C, 2); mem.w(goal, 0x40, 3)
    add(0x00798760, mem, [(0x005B9F30, 0, at(goal)), (0x005AC4F0, 2, "0")])

    # 0079f240 force-attack tails into 0079ddb0.
    for mode, flag in ((0, 0), (2, 1)):
        rig = chain()
        loco, goal, child, node = rig.obj(), rig.obj(), rig.obj(), rig.obj()
        rig.mem.link(rig.unit, 0x374, loco)
        rig.mem.link(goal, 0x33C, node)
        rig.mem.w(rig.state, 0x24, flag)
        rig.mem.w(rig.unit, 0xDC, 0x42)
        rig.mem.w(child, 0x68, 0x68)
        loco_vt = rig.vt(loco, [(0x288, 0)])
        state_vt = rig.vt(rig.state, [(0x4C, 0)])
        child_vt = rig.vt(child, [(0x38, 4), (0x1C, 4)])
        node_vt = rig.vt(node, [(0x28, 4)])
        rets = [(0x006D2020, 2, "0"), (loco_vt[0x288], 0, str(mode))]
        if mode != 2:
            rets += [(0x005B9F30, 0, at(goal)), (0x006C0F40, 1, "0")]
        rets += [(0x005B9F30, 0, at(goal)), (state_vt[0x4C], 0, at(child))]
        if flag == 1:
            rets.append((child_vt[0x38], 1, "0"))
        rets += [
            (0x005AC4F0, 2, "0"), (0x006D21F0, 1, "0"), (node_vt[0x28], 1, "0"),
            (0x006A5370, 1, "99"), (child_vt[0x1C], 1, "7"),
        ]
        add(0x0079F240, rig.mem, rets)

    # 0079a700 harvest onExit.
    rig = chain(); loco, child = rig.obj(), rig.obj()
    rig.mem.link(rig.unit, 0x374, loco)
    rig.mem.link(rig.state, 0x20, child)
    rig.mem.b(rig.state, 0x25, 0xAB)
    child_vt = rig.vt(child, [(0x30, 0), (0, 4)])
    add(0x0079A700, rig.mem, [(child_vt[0x30], 0, "0"), (child_vt[0], 1, "0")], ["1"])
    rig = chain(); loco = rig.obj(); rig.mem.link(rig.unit, 0x374, loco); rig.mem.b(rig.state, 0x25, 0xAB)
    add(0x0079A700, rig.mem, [], ["1"])

    # 0079a5c0 squad onExit.
    rig = chain(); child = rig.obj(); rig.mem.link(rig.state, 0x20, child)
    child_vt = rig.vt(child, [(0, 4)])
    add(0x0079A5C0, rig.mem, [(child_vt[0], 1, "4")], ["1"])
    rig = chain(); loco = rig.obj(); rig.mem.link(rig.unit, 0x374, loco)
    add(0x0079A5C0, rig.mem, [(0x006C0F40, 1, "12")], ["1"])

    def sa_lookup(va: int, finder: int, slot: int | None, direct: int | None, tails: list[tuple[str, str]]) -> None:
        for token, final in tails:
            rig = chain()
            rig.mem.w(rig.machine, 0x80, 0xABC)
            rig.mem.w(rig.state, 0x20, 0xABC)
            module = rig.obj()
            module_vt = rig.vt(module, [(slot, 0)]) if slot is not None else {}
            rets: list[tuple[int, int, str]] = [(0x007E2950, 1, "0" if token == "miss" else at(module))]
            if token != "miss":
                rets.append((finder, 1, "0" if token == "nomod" else at(module)))
            if token not in ("miss", "nomod"):
                if slot is not None:
                    rets.append((module_vt[slot], 0, final))
                if direct is not None:
                    rets.append((direct, 0, final))
            add(va, rig.mem, rets, ["1"] if va in POPS else None)

    sa_lookup(0x00797810, 0x006878B0, 0x5C, None, [("miss", "0"), ("nomod", "0"), ("flag0", "0"), ("flag1", "1")])
    sa_lookup(0x007976D0, 0x006878B0, 0x70, None, [("miss", "0"), ("flag0", "0"), ("flag1", "5")])
    sa_lookup(0x00797610, 0x006878B0, 0x74, None, [("miss", "0"), ("flag1", "1")])
    sa_lookup(0x00797710, 0x006878B0, None, 0x006EB6C0, [("miss", "0"), ("nomod", "0"), ("go", "33")])
    sa_lookup(0x00797F30, 0x00687720, 4, None, [("miss", "0"), ("nomod", "0"), ("busy", "1"), ("idle", "0")])

    # 0079b0c0 retaliate update.
    mem = Mem(); mem.add()
    add(0x0079B0C0, mem, [])
    rig = chain(); child = rig.obj(); rig.mem.link(rig.state, 0x20, child)
    child_vt = rig.vt(child, [(0x10, 0)])
    add(0x0079B0C0, rig.mem, [(0x005B9F30, 0, "0"), (child_vt[0x10], 0, "3")])
    rig = chain(); child, goal = rig.obj(), rig.obj(); rig.mem.link(rig.state, 0x20, child)
    child_vt = rig.vt(child, [(0x38, 4), (0x10, 0)])
    add(0x0079B0C0, rig.mem, [
        (0x005B9F30, 0, at(goal)), (0x005B9F30, 0, "0"), (child_vt[0x38], 1, "0"), (child_vt[0x10], 0, "4"),
    ])
    rig = chain(); child, goal = rig.obj(), rig.obj(); rig.mem.link(rig.state, 0x20, child)
    child_vt = rig.vt(child, [(0x10, 0)])
    add(0x0079B0C0, rig.mem, [
        (0x005B9F30, 0, at(goal)), (0x005B9F30, 0, at(goal)), (child_vt[0x10], 0, "5"),
    ])

    # 007990a0 prepare site.
    rig = chain(); loco = rig.obj(); rig.mem.link(rig.unit, 0x374, loco)
    loco_vt = rig.vt(loco, [(0x1C4, 0)])
    add(0x007990A0, rig.mem, [(loco_vt[0x1C4], 0, "0")])
    rig = chain(); loco, found, logic = rig.obj(), rig.obj(), rig.obj()
    rig.mem.link(rig.unit, 0x374, loco)
    rig.mem.w(logic, 0x58, 0x10)
    loco_vt = rig.vt(loco, [(0x1C4, 0)])
    found_vt = rig.vt(found, [(0x44, 0)])
    add(0x007990A0, rig.mem, [(loco_vt[0x1C4], 0, at(found)), (found_vt[0x44], 0, "20")], gl=at(logic))

    # 0079f880 horde-exit onEnter.
    rig = chain()
    add(0x0079F880, rig.mem, [(0x005B9F30, 0, "0")])
    rig = chain(); goal, child = rig.obj(), rig.obj(); rig.mem.w(child, 0x68, 0x68)
    state_vt = rig.vt(rig.state, [(0x4C, 0)])
    child_vt = rig.vt(child, [(0x38, 4), (0x1C, 4)])
    add(0x0079F880, rig.mem, [
        (0x005B9F30, 0, at(goal)), (state_vt[0x4C], 0, at(child)),
        (child_vt[0x38], 1, "0"), (child_vt[0x1C], 1, "7"),
    ])

    # 00798060 evacuate.
    rig = chain()
    add(0x00798060, rig.mem, [(0x006A7600, 0, "0")])
    rig = chain(); loco, contain = rig.obj(), rig.obj(); rig.mem.link(rig.unit, 0x374, loco)
    loco_vt = rig.vt(loco, [(0x288, 0)])
    contain_vt = rig.vt(contain, [(0x88, 4)])
    add(0x00798060, rig.mem, [
        (0x006A7600, 0, at(contain)), (loco_vt[0x288], 0, "9"), (contain_vt[0x88], 1, "0"),
    ])

    # 00797210 kill miner. Always completes.
    rig = chain()
    add(0x00797210, rig.mem, [(0x005B9F30, 0, "0")])
    rig = chain(); goal = rig.obj()
    add(0x00797210, rig.mem, [(0x005B9F30, 0, at(goal))])
    rig = chain(); goal, node, info = rig.obj(), rig.obj(), rig.obj()
    rig.mem.link(goal, 0x33C, node)
    rig.mem.w(info, 0x1C, 0x111); rig.mem.w(info, 0x34, 0x222); rig.mem.w(info, 8, 0x333)
    node_vt = rig.vt(node, [(0x64, 0)])
    add(0x00797210, rig.mem, [(0x005B9F30, 0, at(goal)), (node_vt[0x64], 0, at(info)), (0x007483A0, 3, "0")])

    # 0079b360 firing arc.
    rig = chain()
    add(0x0079B360, rig.mem, [], ["1"])
    rig = chain(); loco = rig.obj(); rig.mem.link(rig.unit, 0x374, loco); rig.mem.w(rig.unit, 0x44, 0x42200000)
    loco_vt = rig.vt(loco, [(0x270, 0)])
    add(0x0079B360, rig.mem, [(0x007527C0, 1, "0"), (loco_vt[0x270], 0, "6")], ["1"])

    # 00798370 horde members onExit.
    rig = chain()
    add(0x00798370, rig.mem, [(0x005B9F30, 0, "0")], ["1"])
    rig = chain(); goal = rig.obj()
    add(0x00798370, rig.mem, [(0x005B9F30, 0, at(goal)), (0x006A7600, 0, "0")], ["1"])
    rig = chain(); goal, contain = rig.obj(), rig.obj()
    contain_vt = rig.vt(contain, [(0x40, 8)])
    add(0x00798370, rig.mem, [
        (0x005B9F30, 0, at(goal)), (0x006A7600, 0, at(contain)), (0x006A7600, 0, at(contain)),
        (contain_vt[0x40], 2, "8"),
    ], ["1"])

    # 0079e210 combined onExit.
    rig = chain()
    add(0x0079E210, rig.mem, [], ["1"])
    rig = chain(); loco, link, child = rig.obj(), rig.obj(), rig.obj()
    rig.mem.link(rig.unit, 0x374, loco)
    rig.mem.link(loco, 0x214, link)
    rig.mem.link(rig.state, 0x20, child)
    rig.mem.w(link, 0x40, 0xFF)
    child_vt = rig.vt(child, [(0, 4)])
    add(0x0079E210, rig.mem, [(child_vt[0], 1, "4")], ["1"])

    def anchored(va: int, text_calls: bool, null_result: str) -> None:
        mem = Mem(); state, machine = mem.add(), mem.add(); mem.link(state, 0x18, machine)
        add(va, mem, [], ["1"] if va in POPS else None)
        mem = Mem(); state, machine, child = mem.add(), mem.add(), mem.add()
        mem.link(state, 0x18, machine)
        mem.link(state, 0x20, child)
        child_vt = V(mem).attach(child, [(0x10, 0)])
        add(va, mem, [
            (0x005AA240, 1, "0"), (child_vt[0x10], 0, "5"), (0x005AA250, 0, "0"),
        ], ["1"] if va in POPS else None)
        mem = Mem(); state, machine, child = mem.add(), mem.add(), mem.add()
        mem.link(state, 0x18, machine)
        mem.link(state, 0x20, child)
        child_vt = V(mem).attach(child, [(0x10, 0)])
        add(va, mem, [
            (0x005AA240, 1, "0"), (child_vt[0x10], 0, null_result), (0x005AA250, 0, "0"),
        ], ["1"] if va in POPS else None)
        del text_calls

    anchored(0x00799530, True, "fffffffe")
    anchored(0x0079AEE0, True, "7")

    # 00799e70 crate onEnter.
    rig = chain()
    add(0x00799E70, rig.mem, [(0x005B9F30, 0, "0")])
    rig = chain(); goal = rig.obj()
    rig.mem.w(goal, 0x38, 0x11); rig.mem.w(goal, 0x3C, 0x22); rig.mem.w(goal, 0x40, 0x33)
    add(0x00799E70, rig.mem, [(0x005B9F30, 0, at(goal))])

    # 0079f0c0 waypoint onExit.
    rig = chain()
    add(0x0079F0C0, rig.mem, [], ["1"])
    rig = chain(); loco, child = rig.obj(), rig.obj()
    rig.mem.link(rig.unit, 0x374, loco)
    rig.mem.link(rig.state, 0x20, child)
    child_vt = rig.vt(child, [(0, 4)])
    add(0x0079F0C0, rig.mem, [(0x00683590, 1, "0"), (child_vt[0], 1, "4")], ["1"])

    # 0079b080 retaliate onExit.
    rig = chain()
    add(0x0079B080, rig.mem, [], ["1"])
    rig = chain(); loco, child = rig.obj(), rig.obj()
    rig.mem.link(rig.unit, 0x374, loco)
    rig.mem.link(rig.state, 0x20, child)
    child_vt = rig.vt(child, [(0, 4)])
    loco_vt = rig.vt(loco, [(0x240, 0)])
    add(0x0079B080, rig.mem, [(child_vt[0], 1, "0"), (loco_vt[0x240], 0, "9")], ["1"])

    # 0079dd50 aim-force xfer always keeps version byte 2 under a non-writing stub.
    for result in ("15", "16"):
        mem = Mem(); state, xfer = mem.add(), mem.add()
        xfer_vt = V(mem).attach(xfer, [(0x18, 4)])
        add(0x0079DD50, mem, [(xfer_vt[0x18], 1, "1"), (0x0079DAC0, 1, result)], [at(xfer)])

    # 0079a210 weapon predicate. AL is merged into the last call's EAX.
    rig = chain()
    add(0x0079A210, rig.mem, [(0x006A51A0, 1, "0")])
    rig = chain(); weapon = rig.obj(); rig.mem.w(weapon, 4, 0x10)
    add(0x0079A210, rig.mem, [(0x006A51A0, 1, at(weapon)), (0x00685EC0, 0, "110")])
    rig = chain(); weapon = rig.obj(); rig.mem.w(weapon, 4, 0x10)
    add(0x0079A210, rig.mem, [
        (0x006A51A0, 1, at(weapon)), (0x00685EC0, 0, "100"), (0x00686460, 0, "80000020"),
    ])
    rig = chain(); weapon = rig.obj(); rig.mem.w(weapon, 4, 0x10)
    add(0x0079A210, rig.mem, [
        (0x006A51A0, 1, at(weapon)), (0x00685EC0, 0, "200"), (0x00686460, 0, "20"),
    ])

    # 0079ad30 guard onExit.
    rig = chain()
    add(0x0079AD30, rig.mem, [], ["1"])
    rig = chain(); loco, child = rig.obj(), rig.obj()
    rig.mem.link(rig.unit, 0x374, loco)
    rig.mem.link(rig.state, 0x20, child)
    rig.mem.b(loco, 0x444, 9)
    child_vt = rig.vt(child, [(0, 4)])
    add(0x0079AD30, rig.mem, [(child_vt[0], 1, "4")], ["1"])

    # 00799cf0 back away.
    rig = chain(); rig.mem.b(rig.state, 0x4C, 5)
    add(0x00799CF0, rig.mem, [(0x005B9F30, 0, "0")])
    rig = chain(); loco, goal = rig.obj(), rig.obj()
    rig.mem.link(rig.unit, 0x374, loco)
    rig.mem.b(rig.state, 0x4C, 5)
    add(0x00799CF0, rig.mem, [(0x005B9F30, 0, at(goal)), (0x007527C0, 1, "0")])

    def linked_exit(va: int, field: int) -> None:
        mem = Mem(); mem.add()
        add(va, mem, [], ["7"], eax=0x11223344)
        mem = Mem(); state, child = mem.add(), mem.add(); mem.link(state, field, child)
        child_vt = V(mem).attach(child, [(0x18, 4), (0, 4)])
        add(va, mem, [(child_vt[0x18], 1, "1"), (child_vt[0], 1, "2")], ["7"])

    linked_exit(0x0079B5D0, 0x2C)
    linked_exit(0x0079B610, 0x28)

    # 0079de30 attack update.
    rig = chain(); child = rig.obj(); rig.mem.link(rig.state, 0x20, child); rig.mem.w(rig.state, 0x28, 0x55)
    child_vt = rig.vt(child, [(0x10, 0)])
    add(0x0079DE30, rig.mem, [(0x005B9F30, 0, "0"), (child_vt[0x10], 0, "4")])
    rig = chain(); goal, child = rig.obj(), rig.obj()
    rig.mem.link(rig.state, 0x20, child); rig.mem.w(rig.state, 0x28, 0x55)
    child_vt = rig.vt(child, [(0x10, 0)])
    add(0x0079DE30, rig.mem, [(0x005B9F30, 0, at(goal)), (0x006A5370, 1, "1")])
    rig = chain(); goal, child = rig.obj(), rig.obj()
    rig.mem.link(rig.state, 0x20, child); rig.mem.w(rig.state, 0x28, 0x55)
    child_vt = rig.vt(child, [(0x10, 0)])
    add(0x0079DE30, rig.mem, [
        (0x005B9F30, 0, at(goal)), (0x006A5370, 1, "55"), (child_vt[0x10], 0, "4"),
    ])

    # 0079c8a0 / 0079c9f0 boarding and dive-bomb exits.
    for va, callee in ((0x0079C8A0, 0x006D2020), (0x0079C9F0, 0x007576C0)):
        rig = chain()
        add(va, rig.mem, [(callee, 2 if callee == 0x006D2020 else 1, "3")], ["1"])
        rig = chain(); child = rig.obj(); rig.mem.link(rig.state, 0x20, child)
        child_vt = rig.vt(child, [(0, 4)])
        add(va, rig.mem, [(callee, 2 if callee == 0x006D2020 else 1, "3"), (child_vt[0], 1, "4")], ["1"])

    # 00799ab0 cower update.
    mem = Mem(); mem.add()
    add(0x00799AB0, mem, [])
    mem = Mem(); state, child, link = mem.add(), mem.add(), mem.add()
    mem.link(state, 0x20, child); mem.link(child, 4, link); mem.w(link, 4, 0xF423E)
    add(0x00799AB0, mem, [])
    mem = Mem(); state, child = mem.add(), mem.add(); mem.link(state, 0x20, child)
    child_vt = V(mem).attach(child, [(0x10, 0)])
    add(0x00799AB0, mem, [(child_vt[0x10], 0, "6")])

    # 00799780 state-machine slot 10.
    mem = Mem(); machine, watcher = mem.add(), mem.add(); mem.link(machine, 0x88, watcher)
    watcher_vt = V(mem).attach(watcher, [(0x28, 0)])
    add(0x00799780, mem, [(watcher_vt[0x28], 0, "110")])
    mem = Mem(); machine, watcher = mem.add(), mem.add(); mem.link(machine, 0x88, watcher)
    watcher_vt = V(mem).attach(watcher, [(0x28, 0)])
    add(0x00799780, mem, [(watcher_vt[0x28], 0, "200"), (0x005AA1A0, 0, "44")])
    mem = Mem(); mem.add()
    add(0x00799780, mem, [(0x005AA1A0, 0, "45")])

    # 007984f0 parachute slot 19.
    rig = chain()
    add(0x007984F0, rig.mem, [(0x00416820, 1, "0")])
    rig = chain()
    add(0x007984F0, rig.mem, [(0x00416820, 1, "11111111"), (0x00798400, 1, "222")])

    # 007998f0 dead update.
    rig = chain()
    add(0x007998F0, rig.mem, [(0x006D23F0, 1, "0")])
    rig = chain(); loco = rig.obj(); rig.mem.link(rig.unit, 0x374, loco)
    loco_vt = rig.vt(loco, [(0x270, 0)])
    add(0x007998F0, rig.mem, [(loco_vt[0x270], 0, "0"), (0x006D23F0, 1, "0")])

    # 0079e080 / 0079fee0 squad and move-to-goal onEnter.
    for stored in (0x80, 0x81):
        mem = Mem(); state, machine, child = mem.add(), mem.add(), mem.add()
        mem.link(state, 0x18, machine)
        mem.w(child, 0x68, 0x68)
        v = V(mem)
        state_vt = v.attach(state, [(0x4C, 0)])
        machine_vt = v.attach(machine, [(0x44, 0)])
        child_vt = v.attach(child, [(0x1C, 4)])
        add(0x0079E080, mem, [
            (state_vt[0x4C], 0, at(child)), (machine_vt[0x44], 0, f"{stored:x}"), (child_vt[0x1C], 1, "7"),
        ])
    for marker in (0x68, 0x69):
        mem = Mem(); state, machine, child = mem.add(), mem.add(), mem.add()
        mem.link(state, 0x18, machine)
        mem.w(child, 0x68, marker)
        v = V(mem)
        state_vt = v.attach(state, [(0x4C, 0)])
        child_vt = v.attach(child, [(0x1C, 4)])
        add(0x0079FEE0, mem, [
            (state_vt[0x4C], 0, at(child)), (0x005B2450, 1, "0"), (child_vt[0x1C], 1, "7"),
        ])

    # 0079fd20 wide move-to-goal onEnter.
    rig = chain()
    add(0x0079FD20, rig.mem, [(0x005B9F30, 0, "0")])

    def wide(special: bool, weapon: bool, mark_byte: bool) -> None:
        rig = chain()
        goal, child = rig.obj(), rig.obj()
        goal_template, unit_template, loco = rig.obj(), rig.obj(), rig.obj()
        rig.mem.link(goal, 4, goal_template)
        rig.mem.link(rig.unit, 4, unit_template)
        rig.mem.link(rig.unit, 0x374, loco)
        rig.mem.w(goal, 0xDC, 0xDC)
        rig.mem.w(goal, 0x38, 1); rig.mem.w(goal, 0x3C, 2); rig.mem.w(goal, 0x40, 3)
        rig.mem.w(child, 0x68, 0x68)
        rig.mem.b(child, 0x7C, 0x5A)
        rig.mem.b(unit_template, 0xE2, 1 if mark_byte else 0)
        if special:
            rig.mem.w(goal_template, 0xE0, 2)
            rig.mem.w(unit_template, 0xC4, 0x02008000)
            rig.mem.w(unit_template, 0xDC, 0)
        state_vt = rig.vt(rig.state, [(0x4C, 0)])
        child_vt = rig.vt(child, [(0x38, 4), (0x1C, 4)])
        rets = [(0x005B9F30, 0, at(goal)), (state_vt[0x4C], 0, at(child))]
        if special:
            first, second = rig.obj(), rig.obj()
            rig.mem.w(second, 0, 9); rig.mem.w(second, 4, 8); rig.mem.w(second, 8, 7)
            rets += [
                (child_vt[0x38], 1, "0"), (0x00683140, 1, "0"),
                (0x006A76A0, 1, at(first)), (0x005AC4F0, 2, "0"), (0x006A76A0, 1, at(second)),
            ]
        else:
            rets += [(child_vt[0x38], 1, "0"), (0x005AC4F0, 2, "0")]
        rets.append((0x006A51C0, 1, "1" if weapon else "0"))
        if weapon:
            rets += [(0x006A88C0, 1, "1"), (0x00683140, 1, "0")]
        rets.append((child_vt[0x1C], 1, "7"))
        add(0x0079FD20, rig.mem, rets)

    wide(False, False, False)
    wide(False, True, True)
    wide(True, True, False)


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
            raise RuntimeError("aistate79ccheck failed")


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
    text = f"{target:08x}:" + norm(ecx, box["bases"])
    text += ":" + "+".join(norm(arg, box["bases"]) for arg in args)
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
        return bases[int(token[1:].split("+")[0])] + (int(token.split("+")[1], 16) if "+" in token else 0)
    return int(token, 16)


def run_case(emu: Emulator, checker: Checker, case: dict, index: int) -> None:
    emu.reset_data()
    mem: Mem = case["mem"]
    bases = [emu.alloc(bytes(blob), alignment=256) for blob in mem.blobs]
    for obj, offset, target in mem.fix:
        emu.write(bases[obj] + offset, struct.pack("<I", bases[target]))
    gl = 0 if case["gl"] in ("0", "-") else resolve_token(case["gl"], bases)
    emu.write(GAME_LOGIC, struct.pack("<I", gl))
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
    thin = [f"{va:08x}:{n}" for va, n in sorted(counts.items()) if n < 2]
    if missing or extra or thin or len(counts) != 47:
        raise SystemExit(
            f"coverage {len(counts)} missing {[f'{v:08x}' for v in missing]} "
            f"extra {[f'{v:08x}' for v in extra]} thin {thin}")
    executable = Path(sys.argv[1]) if len(sys.argv) > 1 else (
        Path(__file__).resolve().parents[3] / "build/win32/bin/Release/ra3_aistate79ccheck.exe")
    emu = Emulator()
    emu.box = {}
    bind(emu)
    checker = Checker(executable)
    try:
        for index, case in enumerate(CASES):
            run_case(emu, checker, case, index)
    finally:
        checker.close()
    print(f"aistate79c oracle Release cases={len(CASES)} functions={len(counts)} mismatches=0")
    return 0


if __name__ == "__main__":
    sys.exit(main())
