"""Release oracle for the second 0x007B0000 AI-state pass.

Each case runs the shipped bytes in Unicorn and ra3_aistate7b2check on the same
object graph. EAX, object bytes and the call trace must match.
"""

from __future__ import annotations

import csv
import struct
import subprocess
import sys
from pathlib import Path

import capstone
from unicorn import x86_const as uc

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from emulate import STACK_BASE, STACK_SIZE, Emulator  # noqa: E402

SIZE = 0x480
GAME_LOGIC = 0x00DEBA64
LIMIT_ABS = 0x00BD95B8
NEAR_ABS = 0x00BD952C
DF03_ABS = 0x00DF03FC
DE9B_ABS = 0x00DE9B1C
DFC0_ABS = 0x00DFC058
DF6E_ABS = 0x00DF6EC0
LIMIT = 0x43C80000
NEAR = 0x42480000
EAX_IN = 0xA5A5A5A5
FLT_MAX = 0x7F7FFFFF
HORDE = 0x497423F0
NEG_ONE = 0xBF800000
F3 = 0x40400000
F4 = 0x40800000
F100 = 0x42C80000

V18 = 0x00430100
V84 = 0x00430110
V20 = 0x00430120
V6C = 0x00430130
V64 = 0x00430140
V58 = 0x00430150
V0 = 0x00430160
V38_0 = 0x00430170
V28 = 0x00430180
V34 = 0x00430190
V14_2 = 0x004301A0
V274 = 0x004301B0
V1C = 0x004301C0
V68 = 0x004301D0
V78 = 0x004301E0
V3C = 0x004301F0
V50 = 0x00430200
V38_1 = 0x00430210
WALL_UPDATE = 0x005B25C0
WALL_GOAL = 0x005AC4C0

HARD = {
    0x0040EBF0: 0, 0x00416820: 0, 0x004D2F98: 0, 0x0054A090: 0, 0x005A9F60: 8,
    0x005AA250: 0, 0x005AC4F0: 8, 0x005B6150: 0x10, 0x005B9F30: 0, 0x005BDB90: 0x14,
    0x005BDC20: 0x14, 0x005C0A40: 4, 0x00639FB0: 0, 0x0063BF20: 8, 0x00654A20: 0,
    0x00682CE0: 4, 0x00682D70: 0, 0x00682D90: 4, 0x00683140: 4, 0x00686CB0: 0,
    0x00687810: 0, 0x006888A0: 4, 0x006A4500: 4, 0x006A4F80: 8, 0x006A4FB0: 0,
    0x006A5060: 8, 0x006C0F40: 4, 0x006C3500: 0, 0x006D2020: 8, 0x006D5570: 8,
    0x006E6360: 4, 0x006F0A20: 4, 0x0078D1E0: 4, 0x0079BB20: 8, 0x007B12D0: 0,
    0x007B1810: 0, 0x007B6C60: 0xC, 0x007BEBD0: 0, 0x00804300: 0, 0x00B7AF40: 4,
    WALL_UPDATE: 0, WALL_GOAL: 4,
    V18: 4, V84: 4, V20: 4, V6C: 4, V64: 4, V58: 4, V0: 4, V38_0: 0, V28: 4, V34: 4,
    V14_2: 8, V274: 0, V1C: 4, V68: 0, V78: 16, V3C: 4, V50: 8, V38_1: 4,
}

POPS = {
    0x007B0D50: 8, 0x007B0E20: 4, 0x007B0F30: 0, 0x007B1090: 0, 0x007B1190: 4,
    0x007B36B0: 4, 0x007B47B0: 4, 0x007B4820: 4, 0x007B4BD0: 4, 0x007B4E30: 4,
    0x007B5D20: 4, 0x007B6380: 0, 0x007B6840: 0, 0x007B8E70: 0, 0x007B9710: 0,
    0x007B98D0: 0, 0x007BA190: 4, 0x007BA230: 0, 0x007BA260: 4, 0x007BA3E0: 0,
    0x007BA7F0: 4, 0x007BA940: 0, 0x007BA970: 4, 0x007BAAF0: 0, 0x007BAB20: 4,
    0x007BAC70: 0, 0x007BB2B0: 4, 0x007BB470: 0, 0x007BB500: 4, 0x007BB610: 0,
    0x007BBBD0: 0, 0x007BBC80: 0, 0x007BBD20: 4, 0x007BBE10: 0, 0x007BBE40: 4,
    0x007BBF00: 0, 0x007BBF30: 4, 0x007BC020: 0, 0x007BC050: 4, 0x007BC100: 4,
    0x007BC1C0: 0, 0x007BD6B0: 0, 0x007BD980: 4, 0x007BDDE0: 4, 0x007BDEC0: 4,
    0x007BEE80: 0,
}

FACTORIES = {
    0x007BA230: 0x007BA190, 0x007BA3E0: 0x007BA260, 0x007BA940: 0x007BA7F0,
    0x007BAAF0: 0x007BA970, 0x007BAC70: 0x007BAB20, 0x007BB470: 0x007BB2B0,
    0x007BB610: 0x007BB500, 0x007BBE10: 0x007BBD20, 0x007BBF00: 0x007BBE40,
    0x007BC020: 0x007BBF30, 0x007BC1C0: 0x007BC100,
}
CTOR1 = [
    0x007BA190, 0x007BA260, 0x007BA7F0, 0x007BA970, 0x007BAB20, 0x007BB2B0, 0x007BB500,
    0x007BBD20, 0x007BBE40, 0x007BBF30, 0x007BC100,
]
SLOT18 = [0x007B9710, 0x007B8E70, 0x007B98D0]
SPAN = {0x007B98D0: 0x007BA190 - 0x007B98D0}

ROOT = Path(__file__).resolve().parents[3]
LEDGER = ROOT / "docs/analysis/ra3_1.0/function_ledger.tsv"
GAME = Path(r"D:\Game\Ra3\Game\Data\ra3_1.0.game")


def load_binary():
    data = GAME.read_bytes()
    pe = struct.unpack_from("<I", data, 0x3C)[0]
    nsec = struct.unpack_from("<H", data, pe + 6)[0]
    opt = struct.unpack_from("<H", data, pe + 20)[0]
    spans = []
    for index in range(nsec):
        _name, vsize, rva, rawsize, raw = struct.unpack_from("<8sIIII", data, pe + 24 + opt + 40 * index)
        spans.append((rva, max(vsize, rawsize), raw))
    ledger = {}
    with LEDGER.open(encoding="utf-8", newline="") as handle:
        for row in csv.DictReader(handle, delimiter="\t"):
            ledger[int(row["address"], 16)] = int(row["size"] or 0)
    return data, spans, ledger


IMAGE, SPANS, LEDGER_SIZE = load_binary()
MD = capstone.Cs(capstone.CS_ARCH_X86, capstone.CS_MODE_32)
MD.detail = True


def read_va(va: int, size: int) -> bytes:
    rva = va - 0x400000
    for start, length, raw in SPANS:
        if start <= rva < start + length:
            off = raw + rva - start
            return IMAGE[off:off + size]
    raise RuntimeError(f"unmapped {va:08x}")


def span_of(va: int) -> int:
    return SPAN.get(va, LEDGER_SIZE.get(va, 0))


def insns_of(va: int):
    return list(MD.disasm(read_va(va, span_of(va)), va))


def callee_cleanup(target: int, depth: int = 0) -> int | None:
    if target in HARD:
        return HARD[target]
    size = LEDGER_SIZE.get(target, 0)
    if size < 1 or depth > 4:
        return None
    body = list(MD.disasm(read_va(target, size), target))
    rets = []
    jumps = []
    for ins in body:
        if ins.mnemonic == "ret":
            rets.append(int(ins.op_str, 0) if ins.op_str else 0)
        elif ins.mnemonic == "jmp" and ins.operands and ins.operands[0].type == capstone.x86.X86_OP_IMM:
            jumps.append(ins.operands[0].imm & 0xFFFFFFFF)
    if rets:
        if len(set(rets)) != 1:
            return None
        return rets[0]
    if jumps:
        return callee_cleanup(jumps[-1], depth + 1)
    return None


COUNTED = sorted(POPS)
CLEANUP: dict[int, int] = {}
SITE_ARGC: dict[int, int] = {}


def build_sites() -> None:
    cdecl = set()
    pending = []
    for va in COUNTED:
        body = insns_of(va)
        for index, ins in enumerate(body):
            if ins.mnemonic != "call" or not ins.operands or ins.operands[0].type != capstone.x86.X86_OP_IMM:
                continue
            target = ins.operands[0].imm & 0xFFFFFFFF
            if target in COUNTED:
                continue
            found = None
            for look in range(1, 6):
                if index + look >= len(body):
                    break
                nxt = body[index + look]
                if nxt.mnemonic == "add" and nxt.op_str.startswith("esp,"):
                    found = int(nxt.op_str.split(",")[1].strip(), 0)
                    break
                if nxt.mnemonic not in ("mov", "xor", "lea"):
                    break
            if found is not None:
                cdecl.add(target)
                SITE_ARGC[ins.address] = found // 4
            else:
                pending.append((ins.address, target))
    for target in {item[1] for item in pending} | cdecl | set(HARD):
        if target in COUNTED:
            continue
        if target in cdecl:
            CLEANUP[target] = 0
        else:
            cleanup = callee_cleanup(target)
            if cleanup is None:
                raise SystemExit(f"cleanup unknown {target:08x}")
            CLEANUP[target] = cleanup
    for site, target in pending:
        SITE_ARGC[site] = CLEANUP[target] // 4


def news_in(va: int) -> int:
    return sum(1 for ins in insns_of(va)
               if ins.mnemonic == "call" and ins.op_str == "0x416820")


class Mem:
    def __init__(self) -> None:
        self.blobs: list[bytearray] = []
        self.fix: list[tuple[int, int, int, int]] = []

    def add(self) -> int:
        self.blobs.append(bytearray(SIZE))
        return len(self.blobs) - 1

    def w(self, index: int, offset: int, value: int) -> None:
        struct.pack_into("<I", self.blobs[index], offset, value & 0xFFFFFFFF)

    def b(self, index: int, offset: int, value: int) -> None:
        self.blobs[index][offset] = value & 0xFF

    def link(self, index: int, offset: int, target: int, addend: int = 0) -> None:
        self.fix.append((index, offset, target, addend))


def rig(with_loco: bool = True, with_template: bool = True):
    mem = Mem()
    state, machine, unit = mem.add(), mem.add(), mem.add()
    mem.link(state, 0x18, machine)
    mem.link(machine, 0x24, unit)
    loco = None
    if with_loco:
        loco = mem.add()
        mem.link(unit, 0x374, loco)
    if with_template:
        templ = mem.add()
        mem.link(unit, 4, templ)
    return mem, state, machine, unit, loco


def plant(mem: Mem, obj: int, slots: dict[int, int]) -> int:
    table = mem.add()
    mem.link(obj, 0, table)
    for offset, fn in slots.items():
        mem.w(table, offset, fn)
    return table


CASES: list[dict] = []


def add(va: int, mem: Mem, rets: list[tuple[int, int, str]], args: list[str] | None = None,
        eax: int = EAX_IN, gl: str = "0", df03: str = "0", de9b: str = "0",
        dfc0: str = "0", df6e: str = "0") -> None:
    CASES.append({
        "va": va, "mem": mem, "rets": rets, "args": args or [], "eax": eax, "gl": gl,
        "df03": df03, "de9b": de9b, "dfc0": dfc0, "df6e": df6e,
    })


def flag_calls(status: int, ret: str) -> list[tuple[int, int, str]]:
    return [(0x006A4F80, 2, "1"), (0x006A4FB0, 0, ret)]


def build_hand() -> None:
    mem, *_ = rig(with_loco=False)
    add(0x007B1190, mem, [], ["1"])
    mem, _state, _machine, unit, _loco = rig(with_loco=False)
    mem.w(unit, 0xB0, 1 << 0x1D)
    add(0x007B1190, mem, flag_calls(0x9D, "7"), ["1"])
    mem, state, *_ = rig(with_loco=False)
    recorder = mem.add()
    plant(mem, recorder, {0x6C: V6C})
    mem.w(state, 0x44, 5)
    add(0x007B1190, mem, [(V6C, 1, "9")], ["1"], df03=f"@{recorder}")

    mem, *_ = rig(with_loco=False)
    add(0x007B5D20, mem, [(0x006D2020, 2, "1")] * 5 + [(0x006D2020, 2, "2"), (0x006D2020, 2, "3")], ["1"])
    mem, state, _machine, unit, loco = rig()
    held = mem.add()
    plant(mem, held, {0: V0})
    mem.link(state, 0x24, held)
    mem.w(unit, 0xA8, 1)
    add(0x007B5D20, mem, [(V0, 1, "1")] + [(0x006D2020, 2, "2")] * 5 + flag_calls(0x40, "3") +
        [(0x006D2020, 2, "4"), (0x006D2020, 2, "5"), (0x006C0F40, 1, "6"), (0x00682D90, 1, "7")], ["1"])

    mem, state, machine, *_ = rig(with_loco=False)
    xfer = mem.add()
    plant(mem, xfer, {0x18: V18, 0x84: V84, 0x20: V20})
    mem.link(state, 0x20, machine)
    add(0x007B0E20, mem, [(V18, 1, "1"), (V84, 1, "2"), (V20, 1, "3")], [f"@{xfer}"])
    mem, *_ = rig(with_loco=False)
    xfer = mem.add()
    plant(mem, xfer, {0x18: V18, 0x84: V84})
    add(0x007B0E20, mem, [(V18, 1, "1"), (V84, 1, "2"), (0x0040EBF0, 2, "4")], [f"@{xfer}"])

    mem, state, *_ = rig(with_loco=False)
    xfer = mem.add()
    plant(mem, xfer, {0x18: V18})
    add(0x007BDDE0, mem, [(V18, 1, "6")], [f"@{xfer}"])
    mem, state, *_ = rig(with_loco=False)
    mem.link(state, 4, state, 4)
    xfer = mem.add()
    plant(mem, xfer, {0x18: V18, 0x6C: V6C, 0x64: V64, 0x84: V84, 0x58: V58})
    mem.w(xfer, 4, 1 << 2)
    add(0x007BDDE0, mem, [(V18, 1, "1"), (V6C, 1, "2"), (V64, 1, "3"), (V84, 1, "4"),
                          (V6C, 1, "5"), (V58, 1, "6"), (V58, 1, "7")], [f"@{xfer}"])

    mem, *_ = rig(with_loco=False)
    add(0x007BD980, mem, [(0x005B9F30, 0, "0"), (0x005AA250, 0, "1"), (0x006F0A20, 1, "2"),
                          (0x00686CB0, 0, "0")], ["2"])
    mem, *_ = rig()
    goal, dock = mem.add(), mem.add()
    plant(mem, dock, {0x38: V38_0, 0x28: V28, 0x34: V34})
    add(0x007BD980, mem, [(0x005B9F30, 0, f"@{goal}"), (0x00687810, 0, f"@{dock}"), (V38_0, 0, "1"),
                          (V28, 1, "2"), (0x005AA250, 0, "3"), (0x006F0A20, 1, "4"),
                          (0x00686CB0, 0, "0"), (0x006E6360, 1, "5"), (0x00682D70, 0, "0")], ["0"])

    mem = Mem()
    state, machine = mem.add(), mem.add()
    mem.link(state, 0x18, machine)
    add(0x007B1090, mem, [])
    mem, _state, _machine, unit, _loco = rig(with_loco=False)
    calls = [(0x004D2F98, 3, "0")]
    for _ in range(6):
        calls += [(0x006C3500, 2, "10"), (0x006888A0, 1, "0")]
    calls += [(0x004D2F98, 3, "0"), (0x006A5060, 2, "0"), (0x00804300, 0, "0")]
    add(0x007B1090, mem, calls)

    mem, state, *_ = rig(with_loco=False)
    xfer, squad = mem.add(), mem.add()
    plant(mem, xfer, {0x18: V18, 0x84: V84, 0x20: V20})
    mem.link(state, 0x80, squad)
    add(0x007BC050, mem, [(V18, 1, "1"), (0x005C0A40, 1, "2"), (V84, 1, "3"), (V20, 1, "4")], [f"@{xfer}"])
    mem, *_ = rig(with_loco=False)
    xfer = mem.add()
    plant(mem, xfer, {0x18: V18, 0x84: V84})
    add(0x007BC050, mem, [(V18, 1, "0^0=0"), (0x005C0A40, 1, "8")], [f"@{xfer}"])

    mem, _state, _machine, unit, _loco = rig(with_loco=False)
    mem.b(unit, 0x80, 1)
    add(0x007B36B0, mem, [], ["1"])
    mem, state, _machine, unit, loco = rig()
    mem.w(state, 0x20, F3)
    mem.w(state, 0x24, F4)
    plant(mem, loco, {0x274: V274})
    add(0x007B36B0, mem, [(0x00682D70, 0, "0"), (0x006D5570, 2, "1"), (0x00682CE0, 1, "2"),
                          (0x006E6360, 1, "3"), (V274, 0, "1"), (0x0078D1E0, 1, "4")], ["1"])

    mem, *_ = rig(with_loco=False)
    add(0x007B4820, mem, [], ["1"])
    mem, _state, _machine, unit, loco = rig()
    templ = mem.add()
    mem.link(unit, 4, templ)
    mem.w(templ, 0xCC, 1 << 10)
    add(0x007B4820, mem, [(0x00682D70, 0, "0"), (0x00683140, 1, "1"), (0x00686CB0, 0, "0")], ["1"])

    mem, _state, _machine, unit, _loco = rig(with_loco=False)
    mem.b(unit, 0x80, 1)
    add(0x007B47B0, mem, [], ["3"], eax=0x11223344)
    mem, *_ = rig(with_loco=False)
    add(0x007B47B0, mem, [(0x006D5570, 2, "1"), (0x006D2020, 2, "2")], ["3"])

    mem, *_ = rig(with_loco=False)
    add(0x007B4BD0, mem, [], ["1"])
    mem, state, _machine, _unit, loco = rig()
    held = mem.add()
    plant(mem, held, {0x1C: V1C})
    mem.link(state, 0x60, held)
    add(0x007B4BD0, mem, [(V1C, 1, "1"), (0x00682D70, 0, "0"), (0x00682CE0, 1, "5")], ["2"])

    mem, *_ = rig(with_loco=False)
    add(0x007B4E30, mem, [(0x005AA250, 0, "1"), (0x005AC4F0, 2, "2")], ["1"])
    mem, _state, _machine, unit, _loco = rig(with_loco=False)
    mem.w(unit, 0xB0, 1 << 0x1D)
    add(0x007B4E30, mem, [(0x005AA250, 0, "1"), (0x005AC4F0, 2, "2")] + flag_calls(0x9D, "9"), ["1"])

    mem, *_ = rig(with_loco=False)
    add(0x007BDEC0, mem, [(0x005AC4F0, 2, "4")], ["1"])
    mem, state, _machine, unit, _loco = rig(with_loco=False)
    mem.w(unit, 0xB0, 1 << 0x1D)
    mem.w(state, 0x5C, F100)
    add(0x007BDEC0, mem, flag_calls(0x9D, "1") + [(0x005AC4F0, 2, "6")], ["1"])

    mem, *_ = rig()
    mem.w(mem.blobs and 0 or 0, 0, 0)
    # loco+0x150 stays 0
    add(0x007B6380, mem, [(0x007B1810, 0, "15")])
    mem, _state, _machine, _unit, loco = rig()
    body = mem.add()
    mem.link(loco, 0x150, body)
    add(0x007B6380, mem, [(0x007B1810, 0, "15"), (0x0063BF20, 2, "1"), (0x00639FB0, 5, "0")])

    goal = None
    mem, *_ = rig()
    add(0x007BD6B0, mem, [(0x005B9F30, 0, "0")])
    mem, _state, _machine, unit, _loco = rig()
    goal = mem.add()
    mem.b(goal, 0x80, 1)
    add(0x007BD6B0, mem, [(0x005B9F30, 0, f"@{goal}")])
    mem, _state, _machine, unit, _loco = rig()
    goal, dock = mem.add(), mem.add()
    plant(mem, dock, {0x14: V14_2})
    mem.w(unit, 0x38, F100)
    add(0x007BD6B0, mem, [(0x005B9F30, 0, f"@{goal}"), (0x007B12D0, 0, "ffffffff"),
                          (0x005B9F30, 0, f"@{goal}"), (0x00687810, 0, f"@{dock}"),
                          (V14_2, 2, "0^1:0=0,1:4=0")])
    mem, _state, _machine, unit, _loco = rig()
    goal, dock = mem.add(), mem.add()
    plant(mem, dock, {0x14: V14_2})
    mem.w(unit, 0x38, F3)
    mem.w(unit, 0x3C, F4)
    add(0x007BD6B0, mem, [(0x005B9F30, 0, f"@{goal}"), (0x007B12D0, 0, "ffffffff"),
                          (0x005B9F30, 0, f"@{goal}"), (0x00687810, 0, f"@{dock}"),
                          (V14_2, 2, "0^1:0=0,1:4=0")])

    mem, state, machine, *_ = rig(with_loco=False, with_template=False)
    add(0x007BBBD0, mem, [])
    mem, _state, machine, *_ = rig(with_loco=False, with_template=False)
    turret, marker, info = mem.add(), mem.add(), mem.add()
    mem.link(machine, 0x80, turret)
    mem.link(turret, 0x5C, marker)
    mem.link(turret, 0x58, info)
    mem.w(marker, 0x88, 1 << 9)
    add(0x007BBBD0, mem, [])
    mem, _state, machine, *_ = rig(with_loco=False, with_template=False)
    turret, marker, info = mem.add(), mem.add(), mem.add()
    mem.link(machine, 0x80, turret)
    mem.link(turret, 0x5C, marker)
    mem.link(turret, 0x58, info)
    mem.w(info, 0x30, F3)
    mem.w(info, 0x4C, F4)
    add(0x007BBBD0, mem, [(0x007B6C60, 3, "0"), (0x0079BB20, 2, "0")])
    add(0x007BBBD0, mem, [(0x007B6C60, 3, "1"), (0x0079BB20, 2, "1")])

    mem, state, machine, *_ = rig(with_loco=False, with_template=False)
    turret, info = mem.add(), mem.add()
    mem.link(machine, 0x80, turret)
    mem.link(turret, 0x58, info)
    mem.w(info, 0x4C, F4)
    add(0x007BBC80, mem, [(0x007B6C60, 3, "0"), (0x0079BB20, 2, "0")])
    mem, state, machine, *_ = rig(with_loco=False, with_template=False)
    turret, info = mem.add(), mem.add()
    mem.link(machine, 0x80, turret)
    mem.link(turret, 0x58, info)
    mem.b(state, 0x20, 1)
    mem.w(info, 0x30, F3)
    mem.w(info, 0x4C, F4)
    add(0x007BBC80, mem, [(0x007B6C60, 3, "1"), (0x0079BB20, 2, "1")])

    mem, state, *_ = rig(with_loco=False, with_template=False)
    logic = mem.add()
    mem.w(logic, 0x58, 1)
    mem.w(state, 0xC0, 5)
    mem.w(state, 0xC4, 0x42)
    add(0x007BEE80, mem, [], gl=f"@{logic}")
    mem, state, *_ = rig(with_loco=False, with_template=False)
    logic = mem.add()
    mem.w(logic, 0x58, 10)
    mem.w(logic, 0x1C4, F3)
    mem.w(state, 0xC0, 5)
    mem.w(state, 0xC4, 0x42)
    add(0x007BEE80, mem, [(0x007BEBD0, 0, "1")], gl=f"@{logic}")


def norm(value: int, bases: list[int], read) -> str:
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
            raise RuntimeError("aistate7b2check failed")


def resolve_value(token: str, bases: list[int]) -> int:
    if token.startswith("@"):
        plus = token.find("+")
        head = token[1:] if plus < 0 else token[1:plus]
        value = bases[int(head)]
        if plus >= 0:
            value += int(token[plus + 1:], 16)
        return value
    return int(token, 16)


def resolve_token(token: str, bases: list[int]):
    writes = []
    body = token
    if "^" in token:
        body, rest = token.split("^", 1)
        for part in rest.split(","):
            left, val = part.split("=")
            if ":" in left:
                which, off = left.split(":")
                writes.append((int(which), int(off, 16), resolve_value(val, bases)))
            else:
                writes.append((0, int(left, 16), resolve_value(val, bases)))
    return resolve_value(body, bases), writes


def line_for(case: dict) -> str:
    mem: Mem = case["mem"]
    words = [
        "run", f"{case['va']:08x}", f"{case['eax']:x}", f"{LIMIT:x}", f"{NEAR:x}",
        case["gl"], str(len(case["args"])), *case["args"], str(len(mem.blobs)),
    ]
    for blob in mem.blobs:
        words.append(blob.hex())
    words.append(str(len(mem.fix)))
    for index, offset, target, addend in mem.fix:
        words.extend((str(index), f"{offset:x}", str(target), f"{addend:x}"))
    words.append(str(len(case["rets"])))
    for _target, _argc, token in case["rets"]:
        words.append(token)
    words.extend(("0", case["df03"], case["de9b"], case["dfc0"], case["df6e"]))
    return " ".join(words)


def bind(emu: Emulator, box: dict) -> None:
    for target, cleanup in CLEANUP.items():
        def stub(active: Emulator, target=target) -> int:
            esp = active.register(uc.UC_X86_REG_ESP)
            ret_addr = struct.unpack("<I", active.read(esp, 4))[0]
            if box.get("auto"):
                argc = SITE_ARGC.get(ret_addr - 5, CLEANUP[target] // 4)
                args = [struct.unpack("<I", active.read(esp + 4 + 4 * index, 4))[0] for index in range(argc)]
                if target == 0x00416820 and box["mode"] != "zero":
                    if box["mode"] == "shell" and box["news"]:
                        value = 0
                    else:
                        value = box["bases"][box["next"]]
                        box["next"] += 1
                        box["news"] += 1
                else:
                    value = box.get("default", 0)
                text = f"{target:08x}:" + norm(active.register(uc.UC_X86_REG_ECX), box["bases"], active.read)
                text += ":" + "+".join(norm(arg, box["bases"], active.read) for arg in args)
                box["log"].append(text)
                token = canon_eax(value, box["bases"]) if value else "0"
                box["queue"].append((target, argc, token))
                return value
            if not box["pending"]:
                raise RuntimeError(f"extra call 0x{target:08x}")
            spec = box["pending"].pop(0)
            if spec["target"] != target:
                raise RuntimeError(f"expected 0x{spec['target']:08x} got 0x{target:08x}")
            if target == 0x004D2F98:
                dest = struct.unpack("<I", active.read(esp + 4, 4))[0]
                value = struct.unpack("<I", active.read(esp + 8, 4))[0]
                count = struct.unpack("<I", active.read(esp + 12, 4))[0]
                active.write(dest, bytes([value & 0xFF]) * count)
            ecx = active.register(uc.UC_X86_REG_ECX)
            args = [struct.unpack("<I", active.read(esp + 4 + 4 * index, 4))[0] for index in range(spec["argc"])]
            text = f"{target:08x}:" + norm(ecx, box["bases"], active.read)
            text += ":" + "+".join(norm(arg, box["bases"], active.read) for arg in args)
            box["log"].append(text)
            for which, offset, value in spec["writes"]:
                active.write(args[which] + offset, struct.pack("<I", value))
            return spec["ret"]
        emu.bind_function(target, stub, cleanup)


def place(emu: Emulator, case: dict) -> list[int]:
    mem: Mem = case["mem"]
    emu.write(STACK_BASE + STACK_SIZE - 0x2000, b"\x00" * 0x2000)
    bases = [emu.alloc(bytes(blob), alignment=256) for blob in mem.blobs]
    for obj, offset, target, addend in mem.fix:
        emu.write(bases[obj] + offset, struct.pack("<I", (bases[target] + addend) & 0xFFFFFFFF))
    gl = 0 if case["gl"] == "0" else resolve_value(case["gl"], bases)
    emu.write(GAME_LOGIC, struct.pack("<I", gl))
    emu.write(LIMIT_ABS, struct.pack("<I", LIMIT))
    emu.write(NEAR_ABS, struct.pack("<I", NEAR))
    for address, token in ((DF03_ABS, "df03"), (DE9B_ABS, "de9b"), (DFC0_ABS, "dfc0"), (DF6E_ABS, "df6e")):
        value = 0 if case[token] == "0" else resolve_value(case[token], bases)
        emu.write(address, struct.pack("<I", value))
    return bases


def stack_args(case: dict, bases: list[int]) -> list[int]:
    return [resolve_value(token, bases) if token.startswith("@") else int(token, 16) for token in case["args"]]


def run_case(emu: Emulator, checker: Checker, case: dict, index: int) -> None:
    bases = place(emu, case)
    box = emu.box
    box["auto"] = False
    box["bases"] = bases
    box["log"] = []
    box["pending"] = []
    for target, argc, token in case["rets"]:
        value, writes = resolve_token(token, bases)
        box["pending"].append({"target": target, "argc": argc, "ret": value, "writes": writes})
    try:
        result = emu.call(case["va"], stack_args(case, bases), eax=case["eax"], ecx=bases[0])
    except Exception as error:
        raise RuntimeError(f"case {index} 0x{case['va']:08x} emulate: {error}") from None
    if result.stack_cleanup != POPS[case["va"]]:
        raise RuntimeError(
            f"case {index} 0x{case['va']:08x} stack cleanup {result.stack_cleanup} != {POPS[case['va']]}")
    if box["pending"]:
        raise RuntimeError(f"case {index} 0x{case['va']:08x} unconsumed stubs")
    native_mem = [emu.read(base, SIZE) for base in bases]
    native_log = ";".join(box["log"]) if box["log"] else "."
    try:
        parsed = checker.call(line_for(case))
    except Exception as error:
        raise RuntimeError(f"case {index} 0x{case['va']:08x} checker: {error}") from None
    port_bases = [int(part, 16) for part in parsed["bases"].split(",") if part]
    problems = []
    if canon_eax(result.eax, bases) != canon_eax(int(parsed["eax"], 16), port_bases):
        problems.append(f"eax {canon_eax(result.eax, bases)} != {canon_eax(int(parsed['eax'], 16), port_bases)}")
    if native_log != parsed["log"]:
        problems.append(f"calls {native_log} != {parsed['log']}")
    width = SIZE * 2
    port_mem = [bytes.fromhex(parsed["mem"][i * width:(i + 1) * width]) for i in range(len(bases))]
    for obj, (native, port) in enumerate(zip(native_mem, port_mem)):
        left, right = canon(native, bases), canon(port, port_bases)
        if left != right:
            offset = next(i for i, (a, b) in enumerate(zip(left, right)) if a != b)
            problems.append(f"obj{obj}+0x{offset:x}")
            break
    if problems:
        raise RuntimeError(f"case {index} 0x{case['va']:08x} " + "; ".join(problems))


def prepare_auto(va: int, mode: str) -> tuple[Mem, list[str], int]:
    mem = Mem()
    mem.add()
    if va in FACTORIES:
        machine, unit = mem.add(), mem.add()
        mem.link(0, 0x18, machine)
        mem.link(machine, 0x24, unit)
        extra = 1 + news_in(FACTORIES[va])
        args: list[str] = []
        start = 3
    elif va == 0x007B0D50:
        mem.add()
        extra = news_in(va)
        args = ["@1", "3d424c5a"]
        start = 2
    elif va in CTOR1 or va == 0x007B0F30:
        if va != 0x007B0F30:
            mem.add()
            args = ["@1"]
            start = 2
        else:
            machine, unit = mem.add(), mem.add()
            mem.link(0, 0x18, machine)
            mem.link(machine, 0x24, unit)
            args = []
            start = 3
        extra = news_in(va) + (news_in(0x007B0D50) if va == 0x007B0F30 else 0)
    elif va == 0x007B6840 or va in SLOT18:
        extra = news_in(va)
        args = []
        start = 1
    else:
        raise RuntimeError(hex(va))
    for _ in range(extra):
        mem.add()
    return mem, args, start


def capture_auto(emu: Emulator, va: int, mode: str) -> None:
    mem, args, start = prepare_auto(va, mode)
    case = {"va": va, "mem": mem, "rets": [], "args": args, "eax": 0, "gl": "0",
            "df03": "0", "de9b": "0", "dfc0": "0", "df6e": "0"}
    bases = place(emu, case)
    box = emu.box
    box.update(auto=True, mode=mode, bases=bases, next=start, news=0, default=0, log=[], queue=[])
    result = emu.call(va, stack_args(case, bases), eax=0, ecx=bases[0])
    if result.stack_cleanup != POPS[va]:
        raise RuntimeError(f"auto 0x{va:08x} {mode} cleanup {result.stack_cleanup}")
    case["rets"] = list(box["queue"])
    CASES.append(case)


def main() -> int:
    build_sites()
    build_hand()
    executable = Path(sys.argv[1]) if len(sys.argv) > 1 else (
        ROOT / "build/win32/bin/Release/ra3_aistate7b2check.exe")
    emu = Emulator()
    emu.box = {}
    bind(emu, emu.box)
    for va in [0x007B0D50, 0x007B6840, *SLOT18, *CTOR1, *FACTORIES, 0x007B0F30]:
        modes = ("obj", "shell") if va == 0x007B0F30 else ("zero", "obj")
        for mode in modes:
            try:
                capture_auto(emu, va, mode)
            except Exception as error:
                raise SystemExit(f"capture 0x{va:08x} {mode}: {error}") from None
    counts: dict[int, int] = {}
    for case in CASES:
        counts[case["va"]] = counts.get(case["va"], 0) + 1
    missing = [f"{va:08x}" for va in COUNTED if counts.get(va, 0) < 2]
    if missing:
        raise SystemExit(f"coverage missing {missing}")
    checker = Checker(executable)
    try:
        for index, case in enumerate(CASES):
            run_case(emu, checker, case, index)
    finally:
        checker.close()
    print(f"aistate7b2 oracle Release cases={len(CASES)} functions={len(counts)} mismatches=0")
    return 0


if __name__ == "__main__":
    sys.exit(main())
