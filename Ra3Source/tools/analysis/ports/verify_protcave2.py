#!/usr/bin/env python3
"""Differential oracle for RA3 batch 2 inner-patched / entry-recoverable cave functions.

Arms each function's securom_sites.tsv slot to its runtime_target so the native
cave instruction and jump back to the continuation execute in Unicorn, then
compares full-width EAX, EDX, stack cleanup, callback traces, globals, and
scratch memory (0x21000000) against ra3_protcave2check.exe.
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
from emulate import Emulator, STACK_BASE, STACK_SIZE  # noqa: E402

BASE = 0x21000000
SIZE = 0x1800
CAP = Path(r"D:\Game\Ra3\RevRa3\work\runtime_dump\run35\snapshot_3_240s")
SITES = REPO / "docs" / "analysis" / "ra3_1.0" / "securom_sites.tsv"
LEDGER = "Ra3Source/docs/analysis/ra3_1.0/function_ledger.tsv"
VTABLES = REPO / "docs" / "analysis" / "ra3_1.0" / "vtables_split.tsv"
EXE = Path(r"D:\Game\Ra3\Game\Data\ra3_1.0.game")

# (name, va, class, site, slot, continuation, caller, call_site, vt, vt_slot)
FUNCS = [
    ("spready", 0x004D5550, "inner-patched", 0x004D5577, 0x018ED888, 0x004D557D,
     0x0071C540, 0x0071C639, 0x00C27548, 1),
    ("stopstream", 0x00502700, "inner-patched", 0x00502703, 0x018EDC04, 0x0050270A,
     0x005064A0, 0x005064B1, 0x00BDF5A8, 89),
    ("envelope", 0x00505060, "entry-recoverable", 0x00505060, 0x018ED768, 0x00505067,
     0x0051EFF0, 0x0051F0E7, 0x00BDF5A8, 5),
    ("createref", 0x00513340, "inner-patched", 0x00513353, 0x018F2750, 0x00513359,
     0x005134D0, 0x005134E2, 0x00BDF5A8, 36),
    ("xformview", 0x0051F780, "inner-patched", 0x0051F785, 0x018F25B8, 0x0051F78B,
     0x0052F5A0, 0x0052F77D, 0x00BDFADC, 2),
    ("teamentry", 0x00521050, "inner-patched", 0x00521053, 0x018EDDEC, 0x00521059,
     0x00579700, 0x00579744, 0x00BE5580, 1),
    ("flushdirty", 0x005259E0, "inner-patched", 0x005259E3, 0x018F2DD0, 0x005259EA,
     0x006FE4A0, 0x006FEA1A, 0x00BF7E40, 5),
    ("terrhelper", 0x0054DF20, "inner-patched", 0x0054DF33, 0x018ED9F0, 0x0054DF3A,
     0x00864630, 0x0086467E, 0x00C4F778, 4),
    ("projstr", 0x0057F540, "inner-patched", 0x0057F548, 0x018F293C, 0x0057F54E,
     0x0071FAB0, 0x0071FD38, 0x00C11090, 3),
    ("destroyarr", 0x00598630, "inner-patched", 0x00598637, 0x018EDCE0, 0x0059863D,
     0x005986B0, 0x00598739, 0x00BE4290, 6),
    ("tblentry", 0x005B5180, "inner-patched", 0x005B518C, 0x018F290C, 0x005B5193,
     0x005E8B00, 0x005E8F12, 0x00BEF40C, 0),
    ("dtorgameinfo", 0x005E65B0, "inner-patched", 0x005E65B4, 0x018ED564, 0x005E65BA,
     0x005EECE0, 0x005EECE3, 0x00BEF5C8, 0),
    ("resetgameslot", 0x005E6B30, "inner-patched", 0x005E6B3D, 0x018EDC50, 0x005E6B44,
     None, None, 0x00BE2030, 4),
    ("regmap", 0x00674640, "inner-patched", 0x0067465A, 0x018F2CC8, 0x00674662,
     0x00674690, 0x006746B4, 0x00BF3AE0, 5),
    ("findmodsub", 0x0067E600, "inner-patched", 0x0067E60F, 0x018F251C, 0x0067E615,
     0x006812F0, 0x0068131D, 0x00C00F4C, 33),
    ("kindpred", 0x006885D0, "inner-patched", 0x006885E1, 0x018ED534, 0x006885E8,
     0x006C89E0, 0x006C8A9E, 0x00C19C68, 23),
    ("diffcontain", 0x0068C150, "inner-patched", 0x0068C154, 0x018F2A58, 0x0068C15A,
     0x006C9DB0, 0x006C9EF4, 0x00C1FB90, 22),
    ("wpnaudio", 0x006A8400, "entry-recoverable", 0x006A8400, 0x018F28C4, 0x006A8406,
     0x006ED440, 0x006ED471, 0x00BF96A8, 4),
    ("indcolor", 0x006C0620, "inner-patched", 0x006C0623, 0x018F2B84, 0x006C0629,
     0x006FD280, 0x006FD3D6, 0x00C1BC90, 39),
    ("powerradar", 0x006C2C30, "inner-patched", 0x006C2C33, 0x018ED5E0, 0x006C2C39,
     0x007028B0, 0x0070295D, 0x00C06420, 0),
    ("vec3cmd", 0x006C4F10, "inner-patched", 0x006C4F53, 0x018F2C0C, 0x006C4F5B,
     0x006E6830, 0x006E6B21, 0x00C01E40, 25),
    ("tgtcmd3d", 0x006C5480, "inner-patched", 0x006C54B3, 0x018ED740, 0x006C54BB,
     0x007512C0, 0x00751377, 0x00C1C560, 29),
    ("dtorbody", 0x006C8380, "inner-patched", 0x006C8384, 0x018F2B94, 0x006C838A,
     0x006D9C20, 0x006D9C37, 0x00C11D28, 0),
    ("execsp", 0x006CACE0, "inner-patched", 0x006CACE6, 0x018F28A8, 0x006CACEC,
     None, None, 0x00C00F4C, 13),
    ("refreshdraw", 0x006D1DD0, "inner-patched", 0x006D1DD4, 0x018EDA4C, 0x006D1DDA,
     0x006FE4A0, 0x006FEA0D, 0x00BF7E40, 5),
    ("findsel44", 0x006E4760, "inner-patched", 0x006E476E, 0x018ED40C, 0x006E4774,
     0x0079CD20, 0x0079CD3C, 0x00C29F40, 4),
    ("producer", 0x006EFE40, "inner-patched", 0x006EFE43, 0x018F2654, 0x006EFE49,
     0x006FFC00, 0x006FFC7D, 0x00C22118, 99),
    ("mobile", 0x006F1350, "inner-patched", 0x006F1356, 0x018F2574, 0x006F135C,
     0x007CA9E0, 0x007CAA89, 0x00C31DC8, 5),
    ("tintenv", 0x0072CF30, "inner-patched", 0x0072CF37, 0x018EDA38, 0x0072CF3D,
     0x0072CFD0, 0x0072D07B, 0x00C22118, 22),
    ("wpnaim", 0x007461C0, "inner-patched", 0x007461CD, 0x018EDD24, 0x007461D4,
     0x0074634F, 0x007465A4, 0x00C8CA1C, 0),
]

FUNC_BY_NAME = {row[0]: row[1] for row in FUNCS}

CDECL = {
    "spready": 0x004D5550,
    "findmodsub": 0x0067E600,
    "diffcontain": 0x0068C150,
    "findsel44": 0x006E4760,
}

# External / virtual callees -> stack cleanup in bytes
CLEANUP = {
    0x006A77F0: 0,
    0x00687D60: 4,
    0x006A7600: 0,
    0x009CCF80: 0,
    0x009CFEC0: 8,
    0x009CCFA0: 0,
    0x00504DC0: 0,
    0x00416820: 0,
    0x007E20F0: 0,
    0x006A43A0: 4,
    0x00522880: 8,
    0x0054DDC0: 4,
    0x00499640: 0,
    0x004CB690: 8,
    0x004CAA30: 0,
    0x004CBCA0: 0,
    0x00997410: 0,
    0x0058BA70: 0,
    0x004166B0: 0,
    0x00997460: 0,
    0x004CBFF0: 4,
    0x004CA930: 0,
    0x0054CB10: 0,
    0x004CAB60: 0,
    0x004CA9B0: 4,
    0x00673220: 4,
    0x004D88C0: 4,
    0x00B23C00: 0,
    0x0055A0B0: 4,
    0x0055A0F0: 4,
    0x0055A140: 0,
    0x0055D130: 8,
    0x0055B530: 0,
    0x00B11D40: 0,
    0x007F2690: 4,
    0x00986150: 4,
    0x00687460: 4,
    0x00694FF0: 0,
    0x0067E3C0: 0,
    0x007F55B0: 4,
    0x007F5620: 4,
    0x007D2CB0: 12,
    0x006B4220: 4,
    0x006B44E0: 4,
    0x007F03B0: 8,
    0x006A4F20: 4,
    0x00687780: 4,
    0x006C2C30: 4,
    0x006A4D20: 0,
    0x00A21BE0: 4,
    0x0050B5F0: 8,
    0x00A21C90: 4,
    0x006EFE40: 0,
    0x006A6210: 0,
    0x006A19E0: 0,
    0x00687880: 0,
    0x004D99E0: 8,
    0x008758C0: 0,
    0x00879050: 0,
    0x00550290: 0,
    0x00735DB0: 16,
    # Synthetic virtual call targets in 0x22220000:
    0x22220001: 0,   # 0 stack args (__thiscall)
    0x22220002: 4,   # 1 stack arg (__thiscall)
    0x22220003: 8,   # 2 stack args (__thiscall)
    0x22220004: 12,  # 3 stack args (__thiscall)
    0x22220005: 20,  # 5 stack args (__thiscall)
    0x22220006: 0,   # _controlfp (__cdecl, 2 stack args, cleanup=0)
    0x22220007: 12,  # tblentry vt+0x20 (__thiscall, 3 stack args, writes *arg0 = result)
    0x22220008: 4,   # vec3cmd vt+0 (__thiscall, 1 stack arg &cmd, reads cmd+8..+10)
    0x22220009: 4,   # tgtcmd3d vt+0 (__thiscall, 1 stack arg &cmd, reads cmd+14)
    0x2222000A: 4,   # vec3cmd/tgtcmd3d refObj dtor vt+0 (__thiscall, 1 stack arg, reads cmd+4c)
}


def u32(buf, off, val):
    struct.pack_into("<I", buf, off, val & 0xFFFFFFFF)


def u16(buf, off, val):
    struct.pack_into("<H", buf, off, val & 0xFFFF)


def u8(buf, off, val):
    buf[off] = val & 0xFF


def f32(buf, off, val):
    struct.pack_into("<f", buf, off, val)


def fbits(val: float) -> int:
    return struct.unpack("<I", struct.pack("<f", val))[0]


def blank():
    return bytearray(SIZE)


def p(off):
    return BASE + off


def is_stack(addr: int) -> bool:
    return STACK_BASE <= addr < STACK_BASE + STACK_SIZE


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
    for name, va, klass, site, slot, cont, caller, call_site, vt, slot_i in FUNCS:
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
        if caller is not None:
            raw = image.read(call_site, 5)
            if raw[0] not in (0xE8, 0xE9):
                errors.append(f"{name} call site {call_site:08X} is not E8/E9")
            else:
                rel = struct.unpack_from("<i", raw, 1)[0]
                tgt = (call_site + 5 + rel) & 0xFFFFFFFF
                if tgt != va:
                    errors.append(f"{name} call {call_site:08X} targets {tgt:08X}")
            if containing(call_site) != caller:
                errors.append(f"{name} call {call_site:08X} is in {containing(call_site)} not {caller:08X}")
            if not any(start == vt and vt + slot_i * 4 < end for start, end in vt_ranges):
                errors.append(f"{name} vtable {vt:08X} slot {slot_i} is not in vtables_split")
            ptr = struct.unpack("<I", image.read(vt + slot_i * 4, 4))[0]
            if ptr != caller:
                errors.append(f"{name} {vt:08X} slot {slot_i} is {ptr:08X} not {caller:08X}")
        else:
            if not any(start == vt and vt + slot_i * 4 < end for start, end in vt_ranges):
                errors.append(f"{name} vtable {vt:08X} slot {slot_i} is not in vtables_split")
            ptr = struct.unpack("<I", image.read(vt + slot_i * 4, 4))[0]
            if ptr != va:
                errors.append(f"{name} {vt:08X} slot {slot_i} is {ptr:08X} not {va:08X}")
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
        # At 0x006A4540, write: fld dword ptr [0x22220FF0]; ret 4 (D9 05 F0 0F 22 22 C2 04 00)
        self.emu.write(0x006A4540, b"\xD9\x05\xF0\x0F\x22\x22\xC2\x04\x00")
        self.emu.uc.hook_add(unicorn.UC_HOOK_CODE, self._on_float_ret_callee,
                             begin=0x006A4540, end=0x006A4540)
        self.pending: list[int] = []
        self.log: list[tuple] = []
        self.active_entry: int = 0
        self.active_name: str = ""
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

    def _on_float_ret_callee(self, uc, address, size, data) -> None:
        result = self.pending.pop(0) if self.pending else 0
        esp = uc.reg_read(ucr.UC_X86_REG_ESP)
        ecx = uc.reg_read(ucr.UC_X86_REG_ECX)
        arg0 = struct.unpack("<I", uc.mem_read(esp + 4, 4))[0]
        uc.mem_write(0x22220FF0, struct.pack("<I", result & 0xFFFFFFFF))
        uc.reg_write(ucr.UC_X86_REG_EAX, result & 0xFFFFFFFF)
        self.log.append((0x006A4540, ecx, arg0, 0, 0, 1, result))

    def _stub(self, target, cleanup):
        def fn(emu: Emulator):
            result = self.pending.pop(0) if self.pending else 0
            esp = emu.register(ucr.UC_X86_REG_ESP)
            ecx = emu.register(ucr.UC_X86_REG_ECX)
            if is_stack(ecx):
                raw_ecx = ecx
                ecx = 0
            else:
                raw_ecx = ecx
            args = [struct.unpack("<I", emu.read(esp + 4 + 4 * i, 4))[0] for i in range(6)]
            n = cleanup // 4

            if target == 0x22220006:
                # _controlfp (__cdecl, 2 stack args)
                self.log.append((target, ecx, args[0], args[1], 0, 2, result))
            elif target == 0x00416820:
                # __cdecl(size)
                self.log.append((target, ecx, args[0], 0, 0, 1, result))
            elif target == 0x00499640:
                # __cdecl(str, 0)
                self.log.append((target, ecx, args[0], args[1], 0, 2, result))
            elif target == 0x004CBCA0:
                # __cdecl(self, fmt, str)
                self.log.append((target, ecx, args[0], args[1], args[2], 3, result))
            elif target == 0x004166B0:
                # __cdecl(ptr)
                if self.active_name in ("vec3cmd", "tgtcmd3d"):
                    # Verify cave wrote 0x00BDB8E8 to cmd + 0x4C ([esp + 8 + 0x4C])
                    cmd4c = struct.unpack("<I", emu.read(esp + 8 + 0x4C, 4))[0]
                    self.log.append((target, 0, args[0], cmd4c, 0, 1, result))
                else:
                    self.log.append((target, ecx, args[0], 0, 0, 1, result))
            elif target == 0x22220007:
                # tblentry vt+0x20(self, &valStr, &keyStr, 0xDEAD04)
                emu.write(args[0], struct.pack("<I", result))
                self.log.append((target, ecx, 0, 0, args[2], 3, result))
            elif target == 0x00673220:
                # regmap: args[0] is &keyPair on stack
                k0, k1 = struct.unpack("<II", emu.read(args[0], 8))
                self.log.append((target, ecx, k0, k1, 0, 1, result))
            elif target == 0x007D2CB0:
                # vec3cmd / tgtcmd3d: populate stack cmd + 0x50, + 0x2C, + 0x1C from BASE + 0x700..0x708
                ref_obj, ptr2c, ptr1c = struct.unpack("<III", emu.read(BASE + 0x700, 12))
                emu.write(raw_ecx + 0x50, struct.pack("<I", ref_obj))
                emu.write(raw_ecx + 0x2C, struct.pack("<I", ptr2c))
                emu.write(raw_ecx + 0x1C, struct.pack("<I", ptr1c))
                self.log.append((target, 0, args[0], args[1], args[2], 3, result))
            elif target == 0x22220008:
                # vec3cmd vt+0(self, &cmd): read cmd+8, cmd+0xC, cmd+0x10
                v0, v1, v2 = struct.unpack("<III", emu.read(args[0] + 8, 12))
                self.log.append((target, ecx, v0, v1, v2, 1, result))
            elif target == 0x22220009:
                # tgtcmd3d vt+0(self, &cmd): read cmd+0x14
                v0 = struct.unpack("<I", emu.read(args[0] + 0x14, 4))[0]
                self.log.append((target, ecx, v0, 0, 0, 1, result))
            elif target == 0x2222000A:
                # refObj dtor vt+0(refObj, 1) in vec3cmd/tgtcmd3d: verify cmd+0x4C ([esp + 8 + 0x4C])
                cmd4c = struct.unpack("<I", emu.read(esp + 8 + 0x4C, 4))[0]
                self.log.append((target, ecx, args[0], cmd4c, 0, 1, result))
            elif target == 0x0050B5F0:
                # eastl::hashtable::find(table, &iter, &key)
                emu.write(args[0], struct.pack("<I", result))
                key = struct.unpack("<I", emu.read(args[1], 4))[0]
                self.log.append((target, ecx, key, 0, 0, 2, result))
            elif target == 0x00879050:
                # __cdecl(&listHead, 0x1B, &env): read env+0x14, +0x18, +0x1C
                t0, t1, t2 = struct.unpack("<III", emu.read(args[2] + 0x14, 12))
                self.log.append((target, args[1], t0, t1, t2, 3, result))
            elif target == 0x00550290:
                # tintenv list dtor: clear stack pointers written into node[0..1] in BASE
                node_next, node_prev = struct.unpack("<II", emu.read(raw_ecx, 8))
                if BASE <= node_next < BASE + SIZE and node_next == node_prev:
                    emu.write(node_next, struct.pack("<II", 0, 0))
                self.log.append((target, 0, 0, 0, 0, 0, result))
            elif target == 0x00735DB0:
                # __thiscall(self, targetObj, arg1, float_r, arg2)
                self.log.append((target, ecx, args[0], args[1], args[2], 4, result))
            else:
                a0 = args[0] if n > 0 else 0
                a1 = args[1] if n > 1 else 0
                a2 = args[2] if n > 2 else 0
                if is_stack(a0):
                    a0 = 0
                if is_stack(a1):
                    # In spready vt+0x108(sub, 0x4D5480, &val, 1), dereference &val
                    if self.active_name == "spready" and n == 3:
                        a1 = struct.unpack("<I", emu.read(args[1], 4))[0]
                    else:
                        a1 = 0
                if is_stack(a2):
                    a2 = 0
                self.log.append((target, ecx, a0, a1, a2, n, result))
            return result
        return fn

    def run(self, name, blob, args, rets, globs):
        self.emu.reset_data()
        self.pending = list(rets)
        self.log = []
        va = FUNC_BY_NAME[name]
        self.active_entry = va
        self.active_name = name
        # Temporarily unbind the function under test if it is also a callee of another function
        saved_stub = self.emu._function_stubs.pop(va, None)
        try:
            # Zero stack frame region so any subword stack reads see deterministic 0
            top = STACK_BASE + STACK_SIZE - 0x100
            self.emu.write(top - 0x200, b"\x00" * 0x200)
            for addr, val in globs:
                self.emu.write(addr, struct.pack("<I", val))
            self.emu.write(BASE, bytes(blob))
            if name in CDECL:
                result = self.emu.call(va, args, ecx=0)
            elif name == "execsp":
                result = self.emu.call(va, args, ecx=BASE + 0x20)
            else:
                result = self.emu.call(va, args, ecx=BASE)
            image = bytes(self.emu.read(BASE, SIZE))
            globals_out = [(addr, struct.unpack("<I", self.emu.read(addr, 4))[0]) for addr, _ in globs]
            return result, list(self.log), image, globals_out
        finally:
            if saved_stub is not None:
                self.emu._function_stubs[va] = saved_stub


def scenarios():
    out = []

    def add(name, blob, args=(), rets=(), globs=()):
        out.append((name, blob, list(args), list(rets), list(globs)))

    # 1. spready (0x004D5550)
    b = blank(); add("spready", b, [BASE], rets=[0x42, 0, 0])
    b = blank(); u32(b, 0x200, p(0x300)); u32(b, 0x384, 0x22220001)
    add("spready", b, [BASE], rets=[0x42, 0, p(0x200), 0])
    b = blank(); u32(b, 0x200, p(0x300)); u32(b, 0x384, 0x22220001); u32(b, 0x300 + 0x108, 0x22220004)
    add("spready", b, [BASE], rets=[0x99, 0, p(0x200), 1, 0x77])

    # 2. stopstream (0x00502700)
    b = blank(); add("stopstream", b)
    b = blank(); u8(b, 0x80, 1); add("stopstream", b)
    b = blank(); u8(b, 0x80, 1); u32(b, 0xAC, p(0x200))
    add("stopstream", b, rets=[0x1234, 0, 0, 0x5678], globs=[(0x00BBA3A8, 0x22220006)])
    b = blank(); u8(b, 0x80, 1); u32(b, 0x88, p(0x180)); u32(b, 0xAC, p(0x200))
    add("stopstream", b, rets=[0x1234, 0, 0, 0, 0, 0x9ABC], globs=[(0x00BBA3A8, 0x22220006)])

    # 3. envelope (0x00505060)
    b = blank(); add("envelope", b, [fbits(100.0)])
    # piece 0 (d0 >= t)
    b = blank(); u8(b, 0xEC, 1); f32(b, 0xE8, 0.1)
    f32(b, 0xD4, 0.2); f32(b, 0xD8, 0.8); f32(b, 0xDC, 1.0); f32(b, 0xE0, 1.0); f32(b, 0xE4, 1.0)
    add("envelope", b, [fbits(400.0)], rets=[0x11], globs=[(0x00BD94BC, fbits(0.001)), (0x00BD90A4, fbits(1.0))])
    # piece 1 (d1 >= t - d0) and clamp > 1.0
    b = blank(); u8(b, 0xEC, 1); f32(b, 0xE8, 0.9)
    f32(b, 0xD4, 0.2); f32(b, 0xD8, 1.5); f32(b, 0xDC, 0.5); f32(b, 0xE0, 1.0); f32(b, 0xE4, 1.0)
    add("envelope", b, [fbits(200.0)], rets=[0x22], globs=[(0x00BD94BC, fbits(0.001)), (0x00BD90A4, fbits(1.0))])
    # piece 2 (d2 >= t - d0 - d1) and clamp < 0.0
    b = blank(); u8(b, 0xEC, 1); f32(b, 0xE8, 1.6)
    f32(b, 0xD4, -0.5); f32(b, 0xD8, -0.2); f32(b, 0xDC, 0.5); f32(b, 0xE0, 1.0); f32(b, 0xE4, 1.0)
    add("envelope", b, [fbits(200.0)], rets=[0x33], globs=[(0x00BD94BC, fbits(0.001)), (0x00BD90A4, fbits(1.0))])
    # piece 3 (past d2 -> clears +0xEC)
    b = blank(); u8(b, 0xEC, 1); f32(b, 0xE8, 2.6)
    f32(b, 0xD4, 0.4); f32(b, 0xD8, 0.8); f32(b, 0xDC, 0.5); f32(b, 0xE0, 1.0); f32(b, 0xE4, 1.0)
    add("envelope", b, [fbits(200.0)], rets=[0x44], globs=[(0x00BD94BC, fbits(0.001)), (0x00BD90A4, fbits(1.0))])

    # 4. createref (0x00513340)
    b = blank(); u32(b, 0x500, 0xAAAAAAAA); add("createref", b, [p(0x500)], rets=[0])
    b = blank()
    for i in range(0x48):
        u8(b, 0x200 + i, 0xFF)
    add("createref", b, [p(0x500)], rets=[p(0x200)])

    # 5. xformview (0x0051F780)
    b = blank(); u32(b, 0, p(0x200))
    u32(b, 0x200 + 0x98, 0x22220001); u32(b, 0x200 + 0xAC, 0x22220005); u32(b, 0x200 + 0xC4, 0x22220001)
    add("xformview", b, [fbits(1.0), fbits(2.0), fbits(3.0), fbits(4.0), 7], rets=[1, 2, 0x88])
    add("xformview", b, [fbits(-1.0), fbits(0.5), fbits(9.0), fbits(0.0), 0], rets=[0, 0, 0x99])

    # 6. teamentry (0x00521050)
    b = blank(); u32(b, 0x138, p(0x300)); add("teamentry", b)
    b = blank(); add("teamentry", b)
    b = blank(); u32(b, 0x32C, p(0x200)); u32(b, 0x420, 5)
    add("teamentry", b, rets=[p(0x400), p(0x555)], globs=[(0x00DFBD74, p(0x100))])

    # 7. flushdirty (0x005259E0)
    b = blank(); add("flushdirty", b, [0])
    b = blank(); u8(b, 0x13F, 0x08); u32(b, 0x214, p(0x400)); u32(b, 0x218, p(0x400))
    add("flushdirty", b, [0], rets=[0x11])
    b = blank(); u8(b, 0x13F, 0x0F); u32(b, 0x214, p(0x400)); u32(b, 0x218, p(0x408))
    u32(b, 0x400, p(0x500)); u32(b, 0x404, p(0x540))
    u32(b, 0x500, 0x22220001); u32(b, 0x540, 0x22220001)
    u32(b, 0x22220001 - BASE if False else 0x500, p(0x600)); u32(b, 0x540, p(0x600))
    u32(b, 0x600, 0x22220004)
    add("flushdirty", b, [1], rets=[0x10, 0x20, 0x30])

    # 8. terrhelper (0x0054DF20)
    for val in (0x12345678, 0xABCDEF01):
        b = blank()
        for off in range(0x28):
            u8(b, off, 0xEE)
        u32(b, 0x300, p(0x400)); u32(b, 0x404, 0x22220001)
        add("terrhelper", b, [p(0x300)], rets=[0, val])

    # 9. projstr (0x0057F540)
    b = blank(); u32(b, 0, 0xFFFFFFFF); u32(b, 0x200, p(0x300))
    b[0x300:0x306] = b"HELLO\x00"
    add("projstr", b, [p(0x200)], rets=[p(0x300), 0, 0])
    b = blank(); u32(b, 0, 0xFFFFFFFF); u32(b, 0x200, p(0x300))
    add("projstr", b, [p(0x200)], rets=[0, 0, 0])

    # 10. destroyarr (0x00598630)
    b = blank(); u32(b, 0xBC, 9); u32(b, 0xC0, 8); u32(b, 0xC4, 7); add("destroyarr", b)
    b = blank(); u32(b, 0xB4, p(0x300)); u32(b, 0x2F0, 2); u32(b, 0xBC, 9); u32(b, 0xC0, 8); u32(b, 0xC4, 7)
    add("destroyarr", b, rets=[0, 0, 0, 0, 0x55])

    # 11. tblentry (0x005B5180)
    b = blank(); u32(b, 0, p(0x200)); u32(b, 0x220, 0x22220007)
    add("tblentry", b, [2], rets=[0, 0, 0, 0], globs=[(0x00CB7378 + 24, p(0x400))])
    b = blank(); u32(b, 0, p(0x200)); u32(b, 0x220, 0x22220007); u16(b, 0x500 - 4, 0)
    add("tblentry", b, [1], rets=[0, p(0x500), 0, 0], globs=[(0x00CB7378 + 12, p(0x400))])
    b = blank(); u32(b, 0, p(0x200)); u32(b, 0x220, 0x22220007); u16(b, 0x500 - 4, 5)
    add("tblentry", b, [1], rets=[0, p(0x500), 0, 0], globs=[(0x00CB7378 + 12, p(0x400))])

    # 12. dtorgameinfo (0x005E65B0)
    b = blank(); add("dtorgameinfo", b, rets=[0] * 10 + [0x77])
    b = blank(); u32(b, 0xD4, p(0x300)); add("dtorgameinfo", b, rets=[0] * 13 + [0x88])

    # 13. resetgameslot (0x005E6B30)
    b = blank(); add("resetgameslot", b, rets=[0, 0, 0x33], globs=[(0x00DFBD78, 0)])
    b = blank(); u32(b, 0x490, 0xCAFEBABE)
    add("resetgameslot", b, rets=[0, 0, 0x44], globs=[(0x00DFBD78, p(0x400))])

    # 14. regmap (0x00674640)
    b = blank(); u32(b, 0x208, 0); u32(b, 0x210, 0x11223344)
    add("regmap", b, [p(0x200)], rets=[p(0x500)])
    b = blank(); u32(b, 0x208, 1); u32(b, 0x210, 0x99887766)
    add("regmap", b, [p(0x200)], rets=[p(0x504)])

    # 15. findmodsub (0x0067E600)
    b = blank(); add("findmodsub", b, [p(0x100), 0])
    b = blank(); u32(b, 0x100 + 0x310, p(0x200)); u32(b, 0x200, 0)
    add("findmodsub", b, [p(0x100), p(0x50)])
    b = blank(); u32(b, 0x100 + 0x310, p(0x200)); u32(b, 0x200, p(0x300)); u32(b, 0x204, p(0x340)); u32(b, 0x208, 0)
    u32(b, 0x30C, p(0x400)); u32(b, 0x450, 0x22220001)
    u32(b, 0x34C, p(0x400)); u32(b, 0x500, p(0x600)); u32(b, 0x620, 0x22220002)
    add("findmodsub", b, [p(0x100), p(0x50)], rets=[0, p(0x500), 1])

    # 16. kindpred (0x006885D0)
    b = blank(); add("kindpred", b, [0, 0x123, 1])
    b = blank(); add("kindpred", b, [p(0x200), 0x123, 1], rets=[0x00FF00FF])
    b = blank(); add("kindpred", b, [p(0x200), 0x123, 0], rets=[0x00FF00FF])

    # 17. diffcontain (0x0068C150)
    b = blank(); add("diffcontain", b, [p(0x100), p(0x200)])
    b = blank(); u32(b, 0x100 + 0x33C, p(0x300)); u32(b, 0x200 + 0x33C, p(0x300))
    u32(b, 0x300, p(0x400)); u32(b, 0x4CC, 0x22220001)
    add("diffcontain", b, [p(0x100), p(0x200)], rets=[0x12340005, 0x99880005])
    add("diffcontain", b, [p(0x100), p(0x200)], rets=[0x00000005, 0x00000005])

    # 18. wpnaudio (0x006A8400)
    def wpnaudio_blob(idx, thresh):
        b = blank()
        u32(b, 0x04, p(0x100)); u32(b, 0x104, p(0x200))
        u32(b, 0x0C, 0x77); u32(b, 0x1C, idx)
        u32(b, 0x270, thresh); u32(b, 0x2BC, 0x1111); u32(b, 0x2C0, 0x2222)
        u32(b, 0x300, p(0x400)); u32(b, 0x464, 0x22220002)
        return b
    add("wpnaudio", wpnaudio_blob(1, 3), rets=[0, 0, 0, 0, 0, 0, 0, 0x55], globs=[(0x00DF03FC, p(0x300))])
    add("wpnaudio", wpnaudio_blob(5, 3), rets=[0, 0, 0, 0, 0, 0, 0, 0x66], globs=[(0x00DF03FC, p(0x300))])

    # 19. indcolor (0x006C0620)
    b = blank(); u32(b, 0x424, 0xAABBCCDD); add("indcolor", b)
    b = blank(); u32(b, 0x04, p(0x500)); u32(b, 0x414, p(0x200)); u32(b, 0x200 + 0x1F8, 1); u32(b, 0x680, 0x11223344)
    add("indcolor", b, rets=[p(0x600)])
    b = blank(); u32(b, 0x04, p(0x500)); u8(b, 0x5CB, 1); u32(b, 0xC8, 1 << 9)
    u32(b, 0x414, p(0x200)); u32(b, 0x418, p(0x280)); u32(b, 0x780, 0x55667788)
    add("indcolor", b, rets=[p(0x240), p(0x300), 1, 0x999, p(0x600), p(0x700)],
        globs=[(0x00DFBD74, p(0x100)), (0x00DF9470, 0), (0x00DF946C, 0), (0x00E1A078, p(0x180))])

    # 20. powerradar (0x006C2C30)
    b = blank(); add("powerradar", b, [1])
    b = blank(); u32(b, 0x418, p(0x500)); u32(b, 0x574, p(0x600)); u8(b, 0x471, 0x20)
    add("powerradar", b, [0], rets=[0x77])
    b = blank(); u32(b, 0x418, p(0x500)); u32(b, 0x574, p(0x600))
    u32(b, 0x04, p(0x700)); u32(b, 0x714, p(0x800)); u32(b, 0x88C, 5)
    add("powerradar", b, [1], rets=[0x88])

    # 21. vec3cmd (0x006C4F10)
    b = blank(); u32(b, 0, p(0x200)); u32(b, 0x200, 0x22220008)
    u32(b, 0x300, 10); u32(b, 0x304, 20); u32(b, 0x308, 30)
    add("vec3cmd", b, [p(0x300), 0x42], rets=[0, 0])
    b = blank(); u32(b, 0, p(0x200)); u32(b, 0x200, 0x22220008)
    u32(b, 0x300, 11); u32(b, 0x304, 22); u32(b, 0x308, 33)
    u32(b, 0x700, p(0x400)); u32(b, 0x704, p(0x500)); u32(b, 0x708, p(0x600))
    u32(b, 0x400, p(0x480)); u32(b, 0x404, 1); u32(b, 0x480, 0x2222000A)
    add("vec3cmd", b, [p(0x300), 0x99], rets=[0, 0, 0, 0, 0])

    # 22. tgtcmd3d (0x006C5480)
    b = blank(); u32(b, 0, p(0x200)); u32(b, 0x200, 0x22220009)
    add("tgtcmd3d", b, [0x1234, 0x42], rets=[0, 0])
    b = blank(); u32(b, 0, p(0x200)); u32(b, 0x200, 0x22220009)
    u32(b, 0x700, p(0x400)); u32(b, 0x704, p(0x500)); u32(b, 0x708, p(0x600))
    u32(b, 0x400, p(0x480)); u32(b, 0x404, 1); u32(b, 0x480, 0x2222000A)
    add("tgtcmd3d", b, [0x5678, 0x99], rets=[0, 0, 0, 0, 0])

    # 23. dtorbody (0x006C8380)
    b = blank(); u32(b, 0xE4, p(0xE4)); add("dtorbody", b)
    b = blank(); u32(b, 0x100, p(0x300)); u32(b, 0xF0, p(0x340))
    u32(b, 0xE4, p(0x400)); u32(b, 0x400, p(0x420)); u32(b, 0x420, p(0xE4))
    u32(b, 0x510, 0x9999)
    add("dtorbody", b, rets=[0, 0], globs=[(0x00DEA3E0, p(0x500))])

    # 24. execsp (0x006CACE0, self = BASE + 0x20)
    def execsp_blob(f88=0, f84=0):
        b = blank()
        u32(b, 0x14, p(0x300))  # [self - 0x0C]
        u32(b, 0x18, p(0x400))  # [self - 0x08] = ebp
        u32(b, 0x20, p(0x200))  # [self] = vt
        u32(b, 0x29C, 0x22220001)
        u32(b, 0x21C, 0x22220001)
        u8(b, 0x300 + 0x1BB, 1)
        u32(b, 0x488, f88)
        u32(b, 0x484, f84)
        u32(b, 0x600, p(0x680))
        u32(b, 0x698, 0x22220003)
        return b
    add("execsp", execsp_blob(), [p(0x80), p(0x90), fbits(1.5)], rets=[0])
    add("execsp", execsp_blob(), [p(0x80), p(0x90), fbits(1.5)],
        rets=[1, 1, 1, p(0x500), 0, p(0x500), 0, p(0x500), p(0x600), 1],
        globs=[(0x00CC569C, p(0x700))])
    add("execsp", execsp_blob(), [p(0x80), 0, fbits(1.5)],
        rets=[1, 1, 0, p(0x500), 0, p(0x500), 0])

    # 25. refreshdraw (0x006D1DD0)
    b = blank(); u32(b, 0x310, p(0x400)); u32(b, 0x400, 0)
    u32(b, 0x500, p(0x580)); u32(b, 0x580 + 0x9C, 0x22220001)
    u32(b, 0x04, p(0x600)); u32(b, 0x6C0, 1 << 9); u32(b, 0x94, 1 << 5)
    add("refreshdraw", b, rets=[1], globs=[(0x00DEEE54, p(0x500))])
    b = blank(); u32(b, 0x310, p(0x400)); u32(b, 0x400, p(0x420)); u32(b, 0x404, 0)
    u32(b, 0x42C, p(0x480)); u32(b, 0x48C, 0x22220001)
    u32(b, 0x440, p(0x4A0)); u32(b, 0x4A8, 0x22220001)
    u32(b, 0x500, p(0x580)); u32(b, 0x580 + 0x9C, 0x22220001)
    u32(b, 0x90, 1 << 6)
    add("refreshdraw", b, rets=[p(0x440), 0, 0, 0, 0x55], globs=[(0x00DEEE54, p(0x500))])

    # 26. findsel44 (0x006E4760)
    b = blank(); u32(b, 0x50, 0x9999); add("findsel44", b, [0, p(0x50)], globs=[(0x00DEBA64, p(0x100))])
    b = blank(); u32(b, 0x100 + 0xC8, p(0x200)); u32(b, 0x100 + 0xCC, 2); u32(b, 0x208, p(0x2F0))
    u32(b, 0x304, p(0x400)); u32(b, 0x404, p(0x500)); u32(b, 0x5C4, 1 << 4)
    u32(b, 0x400 + 0x310, p(0x600)); u32(b, 0x600, p(0x620)); u32(b, 0x604, 0)
    u32(b, 0x62C, p(0x700)); u32(b, 0x744, 0x22220001)
    add("findsel44", b, [7, p(0x50)], rets=[0, p(0x300), 0, p(0x888)], globs=[(0x00DEBA64, p(0x100))])

    # 27. producer (0x006EFE40)
    b = blank(); add("producer", b, globs=[(0x00DEBA64, p(0x100))])
    b = blank(); u32(b, 0x3EC, 9); u32(b, 0x400 + 0xC8, p(0x500)); u32(b, 0x400 + 0xCC, 1); u32(b, 0x504, p(0x5F0))
    u32(b, 0x604, p(0x777))
    add("producer", b, rets=[0, p(0x600), 0], globs=[(0x00DEBA64, p(0x400))])

    # 28. mobile (0x006F1350)
    b = blank(); u32(b, 0x80, 1 << 5); add("mobile", b)
    b = blank(); u32(b, 0x04, p(0x200)); u32(b, 0x2C0, 1 << 5); add("mobile", b, rets=[0])
    b = blank(); u32(b, 0x04, p(0x200)); u32(b, 0x500, p(0x580)); u32(b, 0x59C, 0x22220001)
    add("mobile", b, rets=[0, 0, p(0x500), 1])

    # 29. tintenv (0x0072CF30)
    b = blank(); u32(b, 0x08, p(0x100)); add("tintenv", b, [p(0x200)])
    b = blank(); u32(b, 0x08, p(0x100)); u32(b, 0x1F4, p(0x300))
    u32(b, 0x200, 0x11); u32(b, 0x204, 0x22); u32(b, 0x208, 0x33)
    add("tintenv", b, [p(0x200)], rets=[p(0x600), 0, 0, 0x77])

    # 30. wpnaim (0x007461C0)
    b = blank(); u32(b, 0x40, 1 << 7); add("wpnaim", b, [p(0x200), 1, 2])
    b = blank(); u8(b, 0xA2, 1); f32(b, 0x38, 1.0)
    add("wpnaim", b, [p(0x200), 1, 2], globs=[(0x00BDB9A0, fbits(0.0))])
    b = blank(); u8(b, 0xA2, 1); f32(b, 0x38, 0.0)
    for i in range(11):
        u32(b, 0x208 + i * 4, 0x100 + i)
    add("wpnaim", b, [p(0x200), 5, 6], rets=[fbits(2.5), 0x88], globs=[(0x00BDB9A0, fbits(0.0))])

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
        checker = REPO / "build" / "win32" / "bin" / "Release" / "ra3_protcave2check.exe"
    if not checker.is_file():
        print(f"checker missing: {checker}")
        return 2
    oracle = Oracle()
    proc = subprocess.Popen([str(checker)], stdin=subprocess.PIPE, stdout=subprocess.PIPE,
                            stderr=subprocess.PIPE, text=True)
    mismatches = 0
    count = 0
    try:
        for name, blob, args_list, rets, globs in scenarios():
            count += 1
            native, nlog, nimage, nglobs = oracle.run(name, blob, args_list, rets, globs)
            arg_txt = "." if not args_list else ",".join(f"{a:08X}" for a in args_list)
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
