#!/usr/bin/env python3
"""Differential oracle for RA3 inner-patched / entry-recoverable cave functions.

The run35 slot often still points at a SecuROM VM trampoline. This oracle
rewrites that slot to the native cave recorded in securom_sites.tsv
(runtime_target: the replaced instruction plus a jump to the continuation)
and executes the rest of the function from the clean image.
"""

from __future__ import annotations

import argparse
import csv
import io
import struct
import subprocess
import sys
from pathlib import Path

import unicorn
from unicorn import x86_const as ucr

SCRIPT_DIR = Path(__file__).resolve().parent
ANALYSIS_DIR = SCRIPT_DIR.parent
REPO = ANALYSIS_DIR.parents[1]
sys.path.insert(0, str(ANALYSIS_DIR))

from disasm import Image  # noqa: E402
from emulate import Emulator  # noqa: E402

BASE = 0x21000000
SIZE = 0x1800
CAP = Path(r"D:\Game\Ra3\RevRa3\work\runtime_dump\run35\snapshot_3_240s")
SITES = REPO / "docs" / "analysis" / "ra3_1.0" / "securom_sites.tsv"
LEDGER = "Ra3Source/docs/analysis/ra3_1.0/function_ledger.tsv"
VTABLES = REPO / "docs" / "analysis" / "ra3_1.0" / "vtables_split.tsv"
EXE = Path(r"D:\Game\Ra3\Game\Data\ra3_1.0.game")

# name, va, class, site, slot, continuation, ownership text
FUNCS = [
    ("clear26c", 0x006E5CE0, "inner-patched", 0x006E5CE2, 0x018F2AA0, 0x006E5CE8,
     "direct callee of 0079b720 at 0079b72f", 0x0079B720, 0x0079B72F, 0x00C30160, 4),
    ("player", 0x006A7600, "entry-recoverable", 0x006A7600, 0x018F2708, 0x006A7606,
     "direct callee of 007688e0 at 0076985c", 0x007688E0, 0x0076985C, None, None),
    ("query", 0x006A51A0, "entry-recoverable", 0x006A51A0, 0x018F2B00, 0x006A51A6,
     "direct callee of 00750c70 at 0075117c", 0x00750C70, 0x0075117C, None, None),
    ("substate", 0x00699780, "entry-recoverable", 0x00699780, 0x018F2C84, 0x00699786,
     "direct callee of 007b24e0 at 007b266c", 0x007B24E0, 0x007B266C, 0x00C2F718, 19),
    ("classify", 0x006BB360, "inner-patched", 0x006BB369, 0x018F2C98, 0x006BB36F,
     "direct callee of 00745250 at 0074534e", 0x00745250, 0x0074534E, None, None),
    ("filter", 0x006BD510, "inner-patched", 0x006BD51E, 0x018F2F18, 0x006BD526,
     "direct callee of 00745d40 at 00745dc1", 0x00745D40, 0x00745DC1, 0x00C07788, 1),
    ("findkey", 0x006C1500, "inner-patched", 0x006C1504, 0x018EDDA0, 0x006C150B,
     "direct callee of 006ebea0 at 006ebf35", 0x006EBEA0, 0x006EBF35, 0x00C06274, 1),
    ("unreg", 0x006A6460, "inner-patched", 0x006A6463, 0x018ED9EC, 0x006A646A,
     "direct callee of 006b0f80 at 006b0fc5", 0x006B0F80, 0x006B0FC5, 0x00C22B88, 29),
    ("step", 0x006A6DC0, "inner-patched", 0x006A6DC1, 0x018F2560, 0x006A6DC8,
     "direct callee of 00742940 at 00742a40", 0x00742940, 0x00742A40, None, None),
    ("idle", 0x00747F10, "inner-patched", 0x00747F13, 0x018F2E68, 0x00747F19,
     "direct callee of 004f7320 at 004f7a59", 0x004F7320, 0x004F7A59, 0x00BDCC74, 13),
    ("reassign", 0x006C1D60, "inner-patched", 0x006C1D63, 0x018F29F0, 0x006C1D69,
     "direct callee of 006c85e0 at 006c85ff", 0x006C85E0, 0x006C85FF, 0x00C11970, 98),
    ("listen", 0x00731530, "inner-patched", 0x00731534, 0x018ED904, 0x0073153B,
     "direct callee of 00742e90 at 00742f46", 0x00742E90, 0x00742F46, 0x00C19580, 4),
    ("buckets", 0x007451E0, "inner-patched", 0x007451F5, 0x018ED724, 0x007451FD,
     "direct callee of 007564a0 at 007564a3", 0x007564A0, 0x007564A3, 0x00C143B0, 1),
    ("part", 0x0069E780, "inner-patched", 0x0069E78E, 0x018F2568, 0x0069E794,
     "direct callee of 006ce860 at 006ceb5d", 0x006CE860, 0x006CEB5D, 0x00C19580, 9),
    ("aitarget", 0x006B7040, "inner-patched", 0x006B7045, 0x018F27DC, 0x006B704B,
     "direct callee of 0079ff10 at 0079ff61", 0x0079FF10, 0x0079FF61, 0x00C2EF40, 6),
]

THISCALL = {
    "clear26c": 0x006E5CE0,
    "player": 0x006A7600,
    "query": 0x006A51A0,
    "substate": 0x00699780,
    "filter": 0x006BD510,
    "findkey": 0x006C1500,
    "unreg": 0x006A6460,
    "step": 0x006A6DC0,
    "idle": 0x00747F10,
    "reassign": 0x006C1D60,
    "listen": 0x00731530,
    "buckets": 0x007451E0,
    "part": 0x0069E780,
    "aitarget": 0x006B7040,
}
CDECL = {"classify": 0x006BB360}

CLEANUP = {
    0x006A1720: 8, 0x00631C20: 0, 0x00AAFE10: 8, 0x004F9950: 4, 0x0098C370: 0,
    0x00990690: 4, 0x009939C0: 4, 0x0081B300: 4, 0x007E16A0: 4, 0x00688B40: 4,
    0x006A6070: 0, 0x006875D0: 0, 0x0085CBC0: 12, 0x00638B70: 0, 0x00747C10: 0,
    0x00687B80: 4, 0x0054B680: 12, 0x0078BDA0: 16, 0x004D9CE0: 12, 0x00734380: 0,
    0x006A8490: 4, 0x008ADD50: 8, 0x006B6900: 12, 0x22220001: 4, 0x22220002: 0,
    0x22220003: 0, 0x22220004: 8, 0x22220005: 0, 0x22220006: 4,
}


def u32(buf, off, val):
    struct.pack_into("<I", buf, off, val & 0xFFFFFFFF)


def u8(buf, off, val):
    buf[off] = val & 0xFF


def f32(buf, off, val):
    struct.pack_into("<f", buf, off, val)


def blank():
    return bytearray(SIZE)


def p(off):
    return BASE + off


def check_proofs() -> None:
    image = Image(EXE)
    text = subprocess.run(["git", "show", f"main:{LEDGER}"], check=True, capture_output=True,
                          encoding="utf-8", errors="replace", cwd=REPO.parents[0]).stdout
    ledger = {int(row["address"], 16): row for row in csv.DictReader(io.StringIO(text), delimiter="\t")}
    ranges = sorted((int(k), int(k) + max(int(v["size"]), 1)) for k, v in ledger.items())

    def containing(va):
        lo, hi = 0, len(ranges)
        while lo < hi:
            mid = (lo + hi) // 2
            if ranges[mid][0] <= va:
                lo = mid + 1
            else:
                hi = mid
        if lo == 0:
            return None
        start, end = ranges[lo - 1]
        return start if start <= va < end else None

    sites = {}
    with SITES.open(encoding="utf-8") as handle:
        for row in csv.DictReader(handle, delimiter="\t"):
            if row["resolution"] == "cave" and row["function"]:
                sites[int(row["function"], 16)] = row
    vt_ranges = []
    with VTABLES.open(encoding="utf-8") as handle:
        for row in csv.DictReader(handle, delimiter="\t"):
            vt_ranges.append((int(row["start"], 16), int(row["end"], 16)))
    errors = []
    for name, va, klass, site, slot, cont, _own, caller, call_site, vt, slot_i in FUNCS:
        row = ledger.get(va)
        if row is None or row["securom"] != klass:
            errors.append(f"{va:08X} ledger class {None if row is None else row['securom']} != {klass}")
        srow = sites.get(va)
        if srow is None:
            errors.append(f"{va:08X} no cave row")
            continue
        if int(srow["site"], 16) != site or int(srow["slot"], 16) != slot or int(srow["continuation"], 16) != cont:
            errors.append(f"{va:08X} site row {srow['site']} {srow['slot']} {srow['continuation']}")
        if srow["function_securom"] != klass:
            errors.append(f"{va:08X} site class {srow['function_securom']}")
        raw = image.read(call_site, 5)
        if raw[0] != 0xE8:
            errors.append(f"{name} call site {call_site:08X} is not E8")
        else:
            rel = struct.unpack_from("<i", raw, 1)[0]
            tgt = (call_site + 5 + rel) & 0xFFFFFFFF
            if tgt != va:
                errors.append(f"{name} call {call_site:08X} targets {tgt:08X}")
        if containing(call_site) != caller:
            errors.append(f"{name} call {call_site:08X} is in {containing(call_site)} not {caller:08X}")
        if vt is not None:
            if not any(start == vt and vt + slot_i * 4 < end for start, end in vt_ranges):
                errors.append(f"{name} vtable {vt:08X} slot {slot_i} is not in vtables_split")
            ptr = struct.unpack("<I", image.read(vt + slot_i * 4, 4))[0]
            if ptr != caller:
                errors.append(f"{name} {vt:08X} slot {slot_i} is {ptr:08X} not {caller:08X}")
        else:
            src = (ledger[caller].get("source_file") or "")
            if "SageEngine" not in src.replace("/", "\\"):
                errors.append(f"{name} caller {caller:08X} source {src!r} is not a SageEngine anchor")
    if errors:
        for err in errors:
            print("PROOF", err)
        raise SystemExit(1)
    print(f"proofs ok ({len(FUNCS)} functions)")


class Oracle:
    def __init__(self) -> None:
        self.emu = Emulator(capture=CAP)
        self.emu.uc.mem_map(BASE, 0x10000)
        self.emu.uc.mem_map(0x22220000, 0x1000, unicorn.UC_PROT_ALL)
        self.emu.write(0x22220000, b"\xC3" * 0x1000)
        self.pending: list[int] = []
        self.log: list[tuple] = []
        sites = {}
        with SITES.open(encoding="utf-8") as handle:
            for row in csv.DictReader(handle, delimiter="\t"):
                if row["resolution"] == "cave" and row["function"] and row["runtime_target"]:
                    sites[int(row["function"], 16)] = row
        for _n, va, *_rest in FUNCS:
            row = sites[va]
            slot = int(row["slot"], 16)
            target = int(row["runtime_target"], 16)
            self.emu.uc.mem_protect(slot & ~0xFFF, 0x1000, unicorn.UC_PROT_ALL)
            self.emu.write(slot, struct.pack("<I", target))
        for target, cleanup in CLEANUP.items():
            self.emu.bind_function(target, self._stub(target, cleanup), cleanup)

    def _stub(self, target, cleanup):
        def fn(emu: Emulator):
            result = self.pending.pop(0) if self.pending else 0
            esp = emu.register(ucr.UC_X86_REG_ESP)
            ecx = emu.register(ucr.UC_X86_REG_ECX)
            args = [struct.unpack("<I", emu.read(esp + 4 + 4 * i, 4))[0] for i in range(4)]
            n = cleanup // 4
            if target == 0x00AAFE10:
                emu.write(args[0], struct.pack("<I", result))
                key = struct.unpack("<I", emu.read(args[1], 4))[0]
                self.log.append((target, ecx, key, 0, 0, 2, result))
            elif target == 0x00688B40:
                mask = struct.unpack("<I", emu.read(args[0], 4))[0]
                self.log.append((target, ecx, mask, 0, 0, 1, result))
            elif target == 0x00638B70:
                self.log.append((target, ecx, args[0], 0, 0, 1, result))
            elif target == 0x004D9CE0:
                self.log.append((target, ecx, args[0], 0, args[2] if n > 2 else 0, n, result))
            elif target == 0x0085CBC0:
                self.log.append((target, ecx, args[0], args[1], args[2], n, result))
            else:
                self.log.append((
                    target, ecx,
                    args[0] if n > 0 else 0,
                    args[1] if n > 1 else 0,
                    args[2] if n > 2 else 0,
                    n, result,
                ))
            return result
        return fn

    def run(self, name, blob, args, rets, globs):
        self.emu.reset_data()
        self.pending = list(rets)
        self.log = []
        for addr, val in globs:
            self.emu.write(addr, struct.pack("<I", val))
        self.emu.write(BASE, bytes(blob))
        if name in THISCALL:
            result = self.emu.call(THISCALL[name], args, ecx=BASE)
        else:
            result = self.emu.call(CDECL[name], args, ecx=0)
        image = bytes(self.emu.read(BASE, SIZE))
        globals_out = [(addr, struct.unpack("<I", self.emu.read(addr, 4))[0]) for addr, _ in globs]
        return result, list(self.log), image, globals_out


def scenarios():
    out = []

    def add(name, blob, args=(), rets=(), globs=()):
        out.append((name, blob, list(args), list(rets), list(globs)))

    for value in (0, 1, 0xFFFFFFFF, 0x80000000):
        b = blank()
        u32(b, 0x26C, value)
        b[0x100] = 0x5A
        add("clear26c", b)

    b = blank(); u32(b, 0x328, 0x111); u32(b, 0x32C, p(0x180)); u32(b, 0x330, p(0x184)); u32(b, 0x180, 0xABCDE0); add("player", b)
    b = blank(); u32(b, 0x32C, p(0x180)); u32(b, 0x330, p(0x180)); u32(b, 0x180, 0xABCDE0); add("player", b)
    b = blank(); u32(b, 0x32C, p(0x180)); u32(b, 0x330, p(0x184)); u32(b, 0x180, 0xABCDE0); add("player", b)
    b = blank(); u32(b, 0x32C, p(0x180)); u32(b, 0x330, p(0x184)); u32(b, 0x180, 0); add("player", b)

    b = blank(); add("query", b, [0x55])
    b = blank(); u32(b, 0x37C, 0x333); add("query", b, [0x55], [0x11])
    b = blank(); u32(b, 0x37C, 0x444); add("query", b, [0xFFFFFFFF], [0])
    b = blank(); u32(b, 0x37C, 0x1); add("query", b, [7], [0xABCDEF])

    for state, sub, ret in ((0, 0x444, None), (1, 0x444, 0x11), (2, 0x444, None), (4, 0x444, 0xAB000000),
                            (5, 0x444, None), (1, 0, None), (4, 0, None), (7, 0x444, None)):
        b = blank(); u32(b, 0x21C, state); u32(b, 0x150, sub)
        add("substate", b, rets=[] if ret is None else [ret])

    def target(bits_c4=0, bits_c0=0, bits_c8=0, bits_d8=0, flags80=0, flags98=0):
        b = blank()
        u32(b, 4, p(0x100))
        u32(b, 0x1C0, bits_c0)
        u32(b, 0x1C4, bits_c4)
        u32(b, 0x1C8, bits_c8)
        u32(b, 0x1D8, bits_d8)
        u32(b, 0x80, flags80)
        u32(b, 0x98, flags98)
        return b

    add("classify", target(bits_c4=1 << 24), [BASE, p(0x700)])
    add("classify", target(bits_c4=1 << 21), [BASE, p(0x700)])
    add("classify", target(bits_c8=1 << 11), [BASE, p(0x700)])
    add("classify", target(bits_c0=1 << 26), [BASE, p(0x700)])
    add("classify", target(flags80=1 << 7, flags98=1 << 19, bits_d8=1 << 12), [BASE, p(0x700)])
    add("classify", target(flags80=1 << 7, bits_c0=1 << 10), [BASE, p(0x700)])
    add("classify", target(flags98=1, bits_c0=1 << 10), [BASE, p(0x700)])
    add("classify", target(bits_c0=1 << 10), [BASE, p(0x700)], rets=[1])
    add("classify", target(bits_c0=0, bits_d8=1 << 12), [BASE, p(0x700)], rets=[0, 0])

    def filt(c4, helper, forbidden):
        b = blank()
        u32(b, 4, p(0x300))
        u32(b, 0x3D4, forbidden)
        u32(b, 0x404, p(0x480))
        u32(b, 0x480 + 0xC4, c4)
        return b
    add("filter", filt(1 << 24, 0, 0), [p(0x400)], [0])
    add("filter", filt(1 << 24, 0x15, 0), [p(0x400)], [0x15])
    add("filter", filt(1 << 24, 0x11000001, 0), [p(0x400)], [0x11000001])
    add("filter", filt(1 << 24, 0, 0x201), [p(0x400)], [0])
    add("filter", filt(1 << 21, 0, 0x201), [p(0x400)], [0])

    b = blank(); add("findkey", b, [3])
    b = blank(); u32(b, 0x310, 1); add("findkey", b, [3])
    b = blank(); u32(b, 0x310, 1); u32(b, 0x3C0, 1); u32(b, 0x3B4, p(0x180)); u32(b, 0x194, 0x55AA)
    add("findkey", b, [0xFFFFFFFF])
    b = blank(); u32(b, 0x310, 1); u32(b, 0x3C0, 4); u32(b, 0x214, 0x99)
    add("findkey", b, [7], [p(0x200)])
    b = blank(); u32(b, 0x310, 1); u32(b, 0x3C0, 4)
    add("findkey", b, [7], [p(0x3B0)])

    def un(flag8, flag7, skip, **kw):
        b = blank()
        u8(b, 0x4D8, flag8)
        u8(b, 0x4D7, flag7)
        u32(b, 4, p(0x300))
        u32(b, 0x3C8, kw.get("c8", 0))
        u32(b, 0x9C, kw.get("s9c", 0))
        u32(b, 0x4E4, kw.get("e4", 0))
        u32(b, 0x4E8, kw.get("e8", 0))
        u32(b, 0x418, kw.get("pl", 0))
        globs = [(0x00DEBA88, 0x11), (0x00DF609C, 0x22), (0x00DF60A4, 0x33), (0x00DFC044, kw.get("g", 0x44))]
        return b, globs
    b, g = un(0, 0, 0); add("unreg", b, [0], globs=g)
    b, g = un(1, 0, 0); add("unreg", b, [0], [0x1], globs=g)
    b, g = un(1, 0, 1); add("unreg", b, [1], globs=g)
    b, g = un(0, 1, 0, s9c=0x55, e4=1, e8=1, pl=0x66); add("unreg", b, [0], [1, 2, 3, 4, 5], globs=g)
    b, g = un(0, 1, 0, c8=1 << 25, s9c=0x55, e4=1, pl=0x66); add("unreg", b, [0], [1, 2], globs=g)
    b, g = un(1, 1, 2, g=0, e4=1); add("unreg", b, [2], [1, 2], globs=g)

    outp = p(0x600)
    b = blank(); u8(b, 0x236, 1); f32(b, 0x38, 1); f32(b, 0x3C, 2); f32(b, 0x40, 3)
    f32(b, 0x228, 4); f32(b, 0x22C, 6); f32(b, 0x230, 8); add("step", b, [outp])
    b = blank(); u8(b, 0x236, 1); u8(b, 0x234, 1); f32(b, 0x38, 9); f32(b, 0x228, 1)
    add("step", b, [outp])
    b = blank(); u8(b, 0x234, 1); u32(b, 0x218, 10); u32(b, 0x758, 14)
    f32(b, 0x38, 5); f32(b, 0x1F4, 3); f32(b, 0x204, 0); f32(b, 0x214, 6)
    add("step", b, [outp], globs=[(0x00DEBA64, p(0x700))])
    b = blank(); u8(b, 0x234, 1); u32(b, 0x218, 10); u32(b, 0x758, 13)
    f32(b, 0x38, 1); f32(b, 0x3C, 2); f32(b, 0x40, 4); f32(b, 0x1F4, 1); f32(b, 0x204, 1); f32(b, 0x214, 1)
    add("step", b, [outp], globs=[(0x00DEBA64, p(0x700))])
    b = blank(); u8(b, 0x234, 1); u32(b, 0x218, 20); u32(b, 0x758, 20)
    add("step", b, [outp], globs=[(0x00DEBA64, p(0x700))])
    b = blank(); u8(b, 0x235, 1); u32(b, 0x218, 8); u32(b, 0x758, 10)
    f32(b, 0x38, 9); f32(b, 0x3C, 0); f32(b, 0x40, 3); f32(b, 0x21C, 3); f32(b, 0x220, 0); f32(b, 0x224, 0)
    add("step", b, [outp], globs=[(0x00DEBA64, p(0x700))])
    b = blank(); add("step", b, [outp], globs=[(0x00DEBA64, 0)])

    def idle(ai=0, fn=0, busy=0, flag=0, f80=0, allow=0, sub=0, sube=0, ok=None, tail=None):
        b = blank()
        u32(b, 0x374, ai)
        if ai:
            u32(b, 0x20, p(0x300))
            # ai object is the pointer itself if we don't deref except vtable. Put vtable in image.
        if ai == p(0x100):
            u32(b, 0x100, p(0x300))
            u32(b, 0x300 + 0x214, fn)
        u8(b, 0x471, flag)
        u32(b, 0x80, f80)
        u32(b, 0x378, sub)
        if sub == p(0x180):
            u8(b, 0x180 + 0xE0, sube)
        rets = []
        if ai:
            rets.append(busy)
        if ok is not None:
            rets.append(ok)
        if tail is not None:
            rets.append(tail)
        return b, rets
    b, r = idle(); add("idle", b, [1])
    b, r = idle(ai=p(0x100), fn=0x22220005, busy=1); add("idle", b, [1], r)
    b, r = idle(ai=p(0x100), fn=0x22220005, busy=0, ok=0); add("idle", b, [1], r)
    b, r = idle(flag=1); add("idle", b, [1])
    b, r = idle(f80=1 << 30, allow=0); add("idle", b, [0])
    b, r = idle(f80=1 << 30, allow=1, ok=0); add("idle", b, [1], r)
    b, r = idle(sub=p(0x180), sube=1); add("idle", b, [1])
    b, r = idle(ok=1, tail=0x42); add("idle", b, [0], r)
    b, r = idle(ai=p(0x100), fn=0x22220005, busy=0, ok=1, tail=0x77); add("idle", b, [1], r)

    def re(direct=0, begin=0, end=0, first=0, child_vt=0, child=0):
        b = blank()
        u32(b, 0x328, direct)
        u32(b, 0x32C, begin)
        u32(b, 0x330, end)
        if first:
            u32(b, begin - BASE, first)
            if child_vt:
                u32(b, first - BASE, p(0x500))
                u32(b, 0x500 + 0x84, 0x22220003)
        if direct and child_vt:
            u32(b, direct - BASE, p(0x500))
            u32(b, 0x500 + 0x84, 0x22220003)
        return b
    add("reassign", re(), [0x999], [0x1])
    b = re(direct=p(0x100), child_vt=1)
    u32(b, 0x100, p(0x500))
    u32(b, 0x500 + 0x84, 0x22220003)
    add("reassign", b, [0x999], [0])
    # child object returned by the virtual call must have a vtable
    b = re(direct=p(0x100))
    u32(b, 0x100, p(0x500))
    u32(b, 0x500 + 0x84, 0x22220003)
    u32(b, 0x180, p(0x580))
    u32(b, 0x580 + 0x1D8, 0x22220004)
    add("reassign", b, [0x999], [p(0x180), 0x55])
    b = blank(); u32(b, 0x32C, p(0x160)); u32(b, 0x330, p(0x164)); u32(b, 0x160, p(0x100))
    u32(b, 0x100, p(0x500)); u32(b, 0x500 + 0x84, 0x22220003)
    add("reassign", b, [0x42], [0])

    def listen(registered, local_end, local_cap, kind=0, bit=0):
        b = blank()
        u8(b, 0x138, registered)
        u32(b, 0x140, local_end)
        u32(b, 0x144, local_cap)
        u32(b, 0x20C, kind)
        if bit:
            u32(b, 0x210, p(0x300))
            u32(b, 0x300 + 0xE0, 1 << 10)
            u32(b, 8, p(0x400))
            u32(b, 0x400 + 0x418, p(0x480))
            u32(b, 0x480 + 0x1318, 5)
        return b
    add("listen", listen(1, p(0x940), p(0x950)), [p(0x200)],
        globs=[(0x00DF8A54, p(0x900)), (0x00DF8A58, p(0x910))])
    add("listen", listen(0, p(0x940), p(0x950)), [p(0x200)],
        globs=[(0x00DF8A54, p(0x900)), (0x00DF8A58, p(0x910))])
    add("listen", listen(1, p(0x940), p(0x950), kind=1, bit=1), [p(0x200)],
        globs=[(0x00DF8A54, p(0x900)), (0x00DF8A58, p(0x910))])
    add("listen", listen(1, 0, p(0x950)), [p(0x200)],
        globs=[(0x00DF8A54, p(0x900)), (0x00DF8A58, p(0x908))])

    b = blank(); u32(b, 0, p(0x200)); u32(b, 0x10, 0x1234); u32(b, 0x248, 0x22220006)
    for i in range(6):
        u32(b, 0x34 + i * 0x10, 0)
        u32(b, 0x34 + i * 0x10 + 4, 0)
    add("buckets", b, rets=[0x77])
    b = blank(); u32(b, 0, p(0x200)); u32(b, 0x10, 0x1234); u32(b, 0x248, 0x22220006)
    for i in range(6):
        u32(b, 0x34 + i * 0x10, p(0x800))
        u32(b, 0x34 + i * 0x10 + 4, p(0x800))
    u32(b, 0x34, p(0x800)); u32(b, 0x38, p(0x808)); u32(b, 0x800, 0x111); u32(b, 0x804, 0x222)
    add("buckets", b, rets=[0x10, 0x21, 0x31, 0x22, 0x32])

    b = blank(); add("part", b, [0])
    b = blank(); u32(b, 8, p(0x100)); add("part", b, [0])
    b = blank(); u32(b, 8, p(0x100)); u32(b, 0x100 + 0x418, 0); add("part", b, [p(0x500)])
    b = blank(); u32(b, 8, p(0x100)); u32(b, 0x100 + 0x418, 0x777); u32(b, 0x50C, 9)
    add("part", b, [p(0x500)])
    b = blank(); u32(b, 8, p(0x100)); u32(b, 0x100 + 0x418, 0x777); u32(b, 0x50C, 2); u32(b, 0x514, 0x888)
    add("part", b, [p(0x500)], [9])
    b = blank(); u32(b, 8, p(0x100)); u32(b, 0x100 + 0x418, 0x777); u32(b, 0x50C, 1); u32(b, 0x510, 0x666)
    add("part", b, [p(0x500)], [4])

    def ai(old=0, ref=0, new=0, newref=0, flags=(0xAB, 0xCD)):
        b = blank()
        u32(b, 0, p(0x500))
        u32(b, 0x500 + 0x270, 0x22220002)
        u8(b, 0x428, flags[0])
        u8(b, 0x432, flags[1])
        if old:
            u32(b, 0x150, old)
            u32(b, old - BASE, p(0x880))
            u32(b, 0x880, 0x22220001)
            u32(b, old - BASE + 4, ref)
        if new:
            u32(b, new - BASE, p(0x980))
            u32(b, 0x980, 0x22220001)
            u32(b, new - BASE + 4, newref)
        return b
    add("aitarget", ai(), [0, 0], [0x10, 0x20])
    add("aitarget", ai(old=p(0x800), ref=1), [0, 0], [0x31, 0x10, 0x20])
    add("aitarget", ai(old=p(0x800), ref=5, new=p(0x900), newref=5), [1, p(0x900)], [0x10, 0x20])
    add("aitarget", ai(new=p(0x900), newref=0xFFFFFFFF), [0, p(0x900)], [0x10, 0x20, 0x41])
    return out


def parse_checker(line):
    parts = line.split()
    fields = {}
    obj = ""
    for part in parts:
        if part.startswith("obj="):
            obj = part[4:]
        elif "=" in part:
            k, v = part.split("=", 1)
            fields[k] = v
    cbs = []
    if fields.get("cbs") not in (None, "."):
        for item in fields["cbs"].split(";"):
            bits = item.split(":")
            cbs.append(tuple(int(x, 16) if i != 5 else int(x, 10) for i, x in enumerate(bits)))
    globs = []
    if fields.get("glob") not in (None, "."):
        for item in fields["glob"].split(","):
            a, v = item.split("=")
            globs.append((int(a, 16), int(v, 16)))
    return int(fields["eax"], 16), int(fields["edx"], 16), int(fields["clean"], 10), cbs, globs, obj


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--checker", type=Path)
    parser.add_argument("--proofs-only", action="store_true")
    args = parser.parse_args()
    check_proofs()
    if args.proofs_only:
        return 0
    checker = args.checker
    if checker is None:
        candidate = REPO / "build" / "win32" / "bin" / "Release" / "ra3_protcavecheck.exe"
        checker = candidate
    if not checker.is_file():
        print(f"checker missing: {checker}")
        return 2
    oracle = Oracle()
    proc = subprocess.Popen([str(checker)], stdin=subprocess.PIPE, stdout=subprocess.PIPE,
                            stderr=subprocess.PIPE, text=True)
    mismatches = 0
    count = 0
    try:
        for name, blob, args, rets, globs in scenarios():
            count += 1
            native, nlog, nimage, nglobs = oracle.run(name, blob, args, rets, globs)
            arg_txt = "." if not args else ",".join(f"{a:08X}" for a in args)
            ret_txt = "." if not rets else ",".join(f"{a:08X}" for a in rets)
            glob_txt = "." if not globs else ",".join(f"{a:08X}={v:08X}" for a, v in globs)
            line = f"{name} {bytes(blob).hex()} {arg_txt} {ret_txt} {glob_txt}\n"
            assert proc.stdin is not None and proc.stdout is not None
            proc.stdin.write(line)
            proc.stdin.flush()
            reply = proc.stdout.readline()
            if not reply:
                err = proc.stderr.read() if proc.stderr else ""
                print(f"checker died on {name}: {err}")
                return 3
            eax, edx, clean, cbs, cglobs, obj = parse_checker(reply.strip())
            problems = []
            if eax != native.eax:
                problems.append(f"eax cpp={eax:08X} native={native.eax:08X}")
            stack = 0x7F000000 <= native.edx < 0x7F100000
            if not (stack and edx == 0) and edx != native.edx:
                problems.append(f"edx cpp={edx:08X} native={native.edx:08X}")
            if clean != native.stack_cleanup:
                problems.append(f"clean cpp={clean} native={native.stack_cleanup}")
            if cbs != nlog:
                problems.append(f"cbs cpp={cbs} native={nlog}")
            if cglobs != nglobs:
                problems.append(f"glob cpp={cglobs} native={nglobs}")
            if obj != nimage.hex():
                for i, (a, bch) in enumerate(zip(bytes.fromhex(obj), nimage)):
                    if a != bch:
                        problems.append(f"image +{i:03X} cpp={a:02X} native={bch:02X}")
                        break
            if problems:
                mismatches += 1
                print(f"MISMATCH {name} #{count}")
                for item in problems[:6]:
                    print(" ", item)
    finally:
        if proc.stdin:
            proc.stdin.write("quit\n")
            proc.stdin.flush()
        proc.wait(timeout=10)
    print(f"cases {count} mismatches {mismatches}")
    return 1 if mismatches else 0


if __name__ == "__main__":
    sys.exit(main())
