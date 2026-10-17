"""Differentially verify the skirmish AI base-build cluster against ra3_1.0.game.

The Release checker is the C++ oracle. Each case builds the same inputs in
Unicorn, runs the shipped function, and compares the checker's printed result.
"""

from __future__ import annotations

import argparse
import struct
import subprocess
import sys
from pathlib import Path

from unicorn import x86_const

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from emulate import Emulator  # noqa: E402

ROOT = Path(__file__).resolve().parents[4]
CHECK_DEFAULT = ROOT / "Ra3Source/build/win32/bin/Release/ra3_skirmishaibasebuildcheck.exe"
MUTANT_DEFAULT = ROOT / "Ra3Source/build/win32/bin/Release/ra3_skirmishaibasebuildcheck_mutant.exe"

GAME_LOGIC = 0x00DEBA64
THING_FACTORY = 0x00DF9D28
TERRAIN_LOGIC = 0x00DFC190
POWER_MGR = 0x00DFC624
FRAMES_PER_SECOND = 0x00DEEEE8


def hex32(value: int) -> str:
    return f"0x{value & 0xFFFFFFFF:08x}"


def fhex(value: float) -> str:
    return hex32(struct.unpack("<I", struct.pack("<f", float(value)))[0])


def fbits(text: str) -> float:
    if text.startswith(("0x", "0X")):
        raw = struct.pack("<I", int(text, 16))
        return struct.unpack("<f", raw)[0]
    return float(text)


def set_st0(emu: Emulator, value: float) -> None:
    # Physical FP0 is ST0 only when the x87 top field is 0 and that tag is valid.
    emu.uc.reg_write(x86_const.UC_X86_REG_FPSW, 0)
    tag = getattr(x86_const, "UC_X86_REG_FPTAG", None)
    if tag is not None:
        emu.uc.reg_write(tag, 0xFFFC)
    raw = struct.pack("<d", float(value))
    bits = struct.unpack("<Q", raw)[0]
    sign = bits >> 63
    exp = (bits >> 52) & 0x7FF
    frac = bits & ((1 << 52) - 1)
    if exp == 0 and frac == 0:
        emu.uc.reg_write(x86_const.UC_X86_REG_FP0, (0, 0x8000 if sign else 0))
        return
    exp80 = (exp - 1023 + 16383) & 0x7FFF
    mant = (1 << 63) | (frac << 11)
    emu.uc.reg_write(x86_const.UC_X86_REG_FP0, (mant, exp80 | (sign << 15)))


def pu32(emu: Emulator, addr: int, value: int) -> None:
    emu.write(addr, struct.pack("<I", value & 0xFFFFFFFF))


def pf32(emu: Emulator, addr: int, value: float) -> None:
    emu.write(addr, struct.pack("<f", float(value)))


def ru32(emu: Emulator, addr: int) -> int:
    return struct.unpack("<I", emu.read(addr, 4))[0]


class Verifier:
    def __init__(self, executable: Path) -> None:
        self.executable = executable
        self.emu = Emulator()
        self.mismatches: list[str] = []
        self.counts: dict[int, int] = {}
        self._bind_memmove()

    def _bind_memmove(self) -> None:
        def memmove(emu: Emulator) -> int:
            dst = emu.stack_arg(0)
            src = emu.stack_arg(1)
            size = emu.stack_arg(2)
            if size and dst and src:
                emu.write(dst, emu.read(src, size))
            return dst

        # Bound import used as memmove/memcpy (cdecl, caller pops 12).
        self.emu.bind_import(0x00BBA500, memmove, 0)

        def ret0(_emu: Emulator) -> int:
            return 0

        # 0x00A21BE0 / 0x00A21C90 bracket the object-hash lookup. Callers ignore the result.
        self.emu.bind_import(0x00BBA26C, ret0, 8)
        self.emu.bind_import(0x00BBA180, ret0, 4)
        self.emu.bind_import(0x00BBA110, ret0, 8)
        self.emu.bind_import(0x00BBA2DC, ret0, 12)

    def cpp(self, args: list[str]) -> str:
        done = subprocess.run(
            [str(self.executable), *args],
            capture_output=True,
            text=True,
            check=False,
        )
        if done.returncode != 0:
            return f"CHECK_EXIT {done.returncode}"
        return done.stdout.strip()

    def case(self, va: int, args: list[str], native) -> None:
        expected = self.cpp(args)
        self.emu.reset_data()
        try:
            got = native(self.emu)
        except Exception as error:  # noqa: BLE001 - oracle reports the case
            got = f"EXC {error}"
        self.counts[va] = self.counts.get(va, 0) + 1
        if got != expected:
            self.mismatches.append(f"0x{va:08X} {args[0]} {args[1:]}: cpp [{expected}] nat [{got}]")


def run_leaves(v: Verifier) -> None:
    # 0x00881130 normalize scores. args: has s has s has s
    def norm(scores):
        def native(emu: Emulator) -> str:
            entries = emu.alloc(bytes(0x24))
            for i, (present, score) in enumerate(scores):
                pu32(emu, entries + i * 0x0C, 1 if present else 0)
                pf32(emu, entries + i * 0x0C + 4, score)
            mgr = emu.alloc(bytes(0x20))
            pu32(emu, mgr + 4, entries)
            pu32(emu, mgr + 8, entries + 0x24)
            pu32(emu, mgr + 0x0C, entries + 0x24)
            emu.write(mgr + 0x14, b"\xAA")
            res = emu.call(0x00881130, [], ecx=mgr)
            if res.stack_cleanup != 0:
                return f"cleanup {res.stack_cleanup}"
            parts = [str(emu.read(mgr + 0x14, 1)[0])]
            for i in range(3):
                parts.append(hex32(ru32(emu, entries + i * 0x0C + 4)))
            return "pos=" + parts[0] + " s0=" + parts[1] + " s1=" + parts[2] + " s2=" + parts[3]
        return native

    for args in (
        ["1", "1.0", "1", "3.0", "0", "9.0"],
        ["1", "0.0", "0", "4.0", "0", "1.0"],
        ["1", "-2.0", "1", "2.0", "1", "0.0"],
        ["0", "5.0", "0", "5.0", "0", "5.0"],
    ):
        triples = [(int(args[i]), fbits(args[i + 1])) for i in range(0, 6, 2)]
        v.case(0x00881130, ["normalize_budget_881130", *args], norm(triples))

    # 0x00882190 zero a Coord3D. AIUpgrade slot 19.
    def reset(x, y, z):
        def native(emu: Emulator) -> str:
            pos = emu.alloc(struct.pack("<fff", x, y, z))
            res = emu.call(0x00882190, [pos], ecx=0)
            if res.stack_cleanup != 4:
                return f"cleanup {res.stack_cleanup}"
            a, b, c = struct.unpack("<III", emu.read(pos, 12))
            return f"x={hex32(a)} y={hex32(b)} z={hex32(c)}"
        return native

    for xyz in (("1.5", "2.5", "3.5"), ("0", "0", "0"), ("-1", "0x7fc00000", "1")):
        v.case(0x00882190, ["reset_build_pos_882190", *xyz], reset(*(fbits(t) for t in xyz)))

    # 0x00882400 copy position then replace z with terrain height.
    def place(x, y, z, base):
        def native(emu: Emulator) -> str:
            def ground(emu_: Emulator) -> int:
                esp = emu_.register(x86_const.UC_X86_REG_ESP)
                fx = struct.unpack("<f", emu_.read(esp + 4, 4))[0]
                fy = struct.unpack("<f", emu_.read(esp + 8, 4))[0]
                set_st0(emu_, base + fx * 0.5 - fy * 0.25)
                return 0
            fn = emu.bind_stub(ground, 12)
            vt = emu.alloc(struct.pack("<8I", 0, 0, 0, 0, 0, 0, fn, 0))
            obj = emu.alloc(struct.pack("<I", vt))
            pu32(emu, TERRAIN_LOGIC, obj)
            dst = emu.alloc(struct.pack("<fff", -1.0, -1.0, -1.0))
            src = emu.alloc(struct.pack("<fff", x, y, z))
            selfb = emu.alloc(bytes(0x70))
            pu32(emu, selfb + 0x44, dst)
            res = emu.call(0x00882400, [src], ecx=selfb)
            if res.stack_cleanup != 4:
                return f"cleanup {res.stack_cleanup}"
            flag = emu.read(selfb + 0x64, 1)[0]
            a, b, c = struct.unpack("<III", emu.read(dst, 12))
            return f"flag={flag} x={hex32(a)} y={hex32(b)} z={hex32(c)}"
        return native

    for args in (("2", "4", "9", "10"), ("0", "0", "1", "0"), ("-2", "8", "3", "-4")):
        v.case(
            0x00882400,
            ["set_build_pos_terrain_882400", *args],
            place(*(fbits(t) for t in args)),
        )

    # 0x00882A90 / 0x00882AE0 / 0x00882C30 bitmask scans.
    def bitscan(va: int, kind: str, bit_index: int, count: int, masks, values):
        def native(emu: Emulator) -> str:
            if kind == "entry":
                entries = emu.alloc(bytes(3 * 20))
                for i in range(2):
                    pu32(emu, entries + i * 20, 100 + i)
                    pu32(emu, entries + i * 20 + 0x10, masks[i])
                    pu32(emu, entries + i * 20 + 0x14, int(values[i]) & 0xFFFFFFFF)
                inner = emu.alloc(bytes(0x150))
                pu32(emu, inner + 0x10C, count)
                pu32(emu, inner + 0x110, entries)
                wrap = emu.alloc(struct.pack("<I", inner))
                res = emu.call(va, [bit_index], ecx=wrap)
                if res.stack_cleanup != 4:
                    return f"cleanup {res.stack_cleanup}"
                if res.eax == 0:
                    return "idx=-1"
                idx = (res.eax - entries) // 20
                return f"idx={idx}"
            entries = emu.alloc(bytes(3 * 12))
            for i in range(2):
                pf32(emu, entries + i * 8, values[i])
                pu32(emu, entries + i * 8 + 4, masks[i])
            inner = emu.alloc(bytes(0x150))
            pu32(emu, inner + 0x104, count)
            pu32(emu, inner + 0x108, entries)
            pu32(emu, inner + 0x12C, count)
            pu32(emu, inner + 0x130, entries)
            wrap = emu.alloc(struct.pack("<I", inner))
            res, bits = emu.call_float32(va, [bit_index], ecx=wrap)
            if res.stack_cleanup != 4:
                return f"cleanup {res.stack_cleanup}"
            return f"ret={hex32(bits)}"
        return native

    specs = (
        (0x00882A90, "find_bitmask_zero_882a90", "float"),
        (0x00882AE0, "find_entry20_bit_882ae0", "entry"),
        (0x00882C30, "find_bitmask_one_882c30", "float"),
    )
    float_rows = (
        ("0", "2", "1", "1.5", "0", "2.5"),
        ("3", "2", "0", "1.5", "8", "4.25"),
        ("0", "0", "1", "1.5", "1", "2.5"),
        ("31", "2", "0x80000000", "7", "1", "8"),
    )
    entry_rows = (
        ("0", "2", "1", "11", "0", "22"),
        ("4", "2", "0", "11", "0x10", "22"),
        ("0", "0", "1", "11", "1", "22"),
    )
    for va, op, kind in specs:
        rows = entry_rows if kind == "entry" else float_rows
        for row in rows:
            masks = (int(row[2], 0), int(row[4], 0))
            vals = (fbits(row[3]), fbits(row[5])) if kind == "float" else (int(row[3], 0), int(row[5], 0))
            v.case(va, [op, *row], bitscan(va, kind, int(row[0], 0), int(row[1], 0), masks, vals))

    # 0x00884550 priority flags on up to 3 bases.
    def flags(count, triples):
        def native(emu: Emulator) -> str:
            bases = [emu.alloc(bytes(0x44)) for _ in range(3)]
            for i, (ident, fl) in enumerate(triples):
                struct.pack_into  # keep import used
                pu32(emu, bases[i] + 4, ident & 0xFFFFFFFF)
                pu32(emu, bases[i] + 0x2C, fl)
            ptrs = emu.alloc(struct.pack("<3I", *bases))
            selfb = emu.alloc(bytes(0x40))
            pu32(emu, selfb + 0x28, ptrs)
            pu32(emu, selfb + 0x2C, ptrs + count * 4)
            res = emu.call(0x00884550, [], ecx=selfb)
            if res.stack_cleanup != 0:
                return f"cleanup {res.stack_cleanup}"
            got = [hex32(ru32(emu, b + 0x2C)) for b in bases]
            return f"f0={got[0]} f1={got[1]} f2={got[2]}"
        return native

    for row in (
        ("3", "1", "0x1", "2", "0x2", "3", "0x4"),
        ("2", "5", "0x81", "4", "0x101", "9", "0x1"),
        ("3", "3", "0x181", "1", "0x81", "2", "0x101"),
        ("0", "1", "1", "2", "2", "3", "3"),
    ):
        triples = [(int(row[i], 0), int(row[i + 1], 0)) for i in range(1, 7, 2)]
        v.case(0x00884550, ["update_base_priority_884550", *row], flags(int(row[0]), triples))

    # 0x00886320 product of heuristic slot-1 floats.
    def product(count, v0, v1):
        def native(emu: Emulator) -> str:
            returns = {id(v0): v0}

            def eval0(emu_: Emulator) -> int:
                set_st0(emu_, v0)
                return 0

            def eval1(emu_: Emulator) -> int:
                set_st0(emu_, v1)
                return 0

            s0 = emu.bind_stub(eval0, 8)
            s1 = emu.bind_stub(eval1, 8)
            vt0 = emu.alloc(struct.pack("<II", 0, s0))
            vt1 = emu.alloc(struct.pack("<II", 0, s1))
            h0 = emu.alloc(struct.pack("<I", vt0))
            h1 = emu.alloc(struct.pack("<I", vt1))
            # nodes: next, prev, heuristic. sentinel is state+4.
            state = emu.alloc(bytes(0x20))
            n0 = emu.alloc(bytes(0x10))
            n1 = emu.alloc(bytes(0x10))
            sentinel = state + 4
            if count == 0:
                pu32(emu, state + 4, sentinel)
                pu32(emu, state + 8, sentinel)
            elif count == 1:
                pu32(emu, state + 4, n0)
                pu32(emu, n0, sentinel)
                pu32(emu, n0 + 8, h0)
            else:
                pu32(emu, state + 4, n0)
                pu32(emu, n0, n1)
                pu32(emu, n0 + 8, h0)
                pu32(emu, n1, sentinel)
                pu32(emu, n1 + 8, h1)
            res, bits = emu.call_float32(0x00886320, [0, 0], ecx=state)
            if res.stack_cleanup != 8:
                return f"cleanup {res.stack_cleanup}"
            del returns
            return f"ret={hex32(bits)}"
        return native

    for row in (("0", "1", "1"), ("1", "0.5", "4"), ("2", "2", "3"), ("2", "-1", "4"), ("1", "0", "9")):
        v.case(0x00886320, ["eval_heuristic_prod_886320", *row], product(int(row[0]), fbits(row[1]), fbits(row[2])))

    # 0x00887170 pop a template id off a pair cursor.
    def pop(enabled, exhausted, cursor, pairs):
        def native(emu: Emulator) -> str:
            buf = emu.alloc(struct.pack("<4I", *pairs))
            q = emu.alloc(bytes(0x10))
            pu32(emu, q, enabled)
            pu32(emu, q + 4, buf + cursor * 8)
            pu32(emu, q + 8, buf)
            emu.write(q + 0x0C, bytes([exhausted & 0xFF]))
            res = emu.call(0x00887170, [], ecx=q)
            if res.stack_cleanup != 0:
                return f"cleanup {res.stack_cleanup}"
            new_idx = (ru32(emu, q + 4) - buf) // 8
            exh = emu.read(q + 0x0C, 1)[0]
            return f"ret={hex32(res.eax)} idx={new_idx} exh={exh}"
        return native

    for row in (
        ("1", "0", "2", "1", "10", "1", "11"),
        ("0", "0", "2", "1", "10", "1", "11"),
        ("1", "1", "2", "1", "10", "1", "11"),
        ("1", "0", "1", "7", "8", "7", "9"),
        ("1", "0", "1", "3", "4", "5", "6"),
    ):
        pairs = (int(row[3], 0), int(row[4], 0), int(row[5], 0), int(row[6], 0))
        v.case(
            0x00887170,
            ["pop_next_tid_887170", *row],
            pop(int(row[0], 0), int(row[1], 0), int(row[2], 0), pairs),
        )

    # 0x00887300 linear scan of 0x2CC entries.
    def find_tid(count, target, ids):
        def native(emu: Emulator) -> str:
            entries = emu.alloc(bytes(3 * 0x2CC))
            for i, ident in enumerate(ids):
                pu32(emu, entries + i * 0x2CC + 0x24, ident)
            selfb = emu.alloc(bytes(0x30))
            pu32(emu, selfb + 0x1C, entries)
            pu32(emu, selfb + 0x20, count)
            res = emu.call(0x00887300, [target], ecx=selfb)
            if res.stack_cleanup != 4:
                return f"cleanup {res.stack_cleanup}"
            if res.eax == 0:
                return "idx=-1"
            return f"idx={(res.eax - entries) // 0x2CC}"
        return native

    for row in (("3", "20", "10", "20", "30"), ("2", "99", "1", "2", "3"), ("0", "1", "1", "1", "1"), ("1", "7", "7", "8", "9")):
        ids = tuple(int(x, 0) for x in row[2:])
        v.case(0x00887300, ["find_entry_tid_887300", *row], find_tid(int(row[0]), int(row[1], 0), ids))

    # 0x00887D60 constructor stores the buildable-entry vtable.
    def init_entry(owner, template):
        def native(emu: Emulator) -> str:
            selfb = emu.alloc(b"\xCC" * 0x60)
            res = emu.call(0x00887D60, [owner, template], ecx=selfb)
            if res.stack_cleanup != 8:
                return f"cleanup {res.stack_cleanup}"
            eq = 1 if res.eax == selfb else 0
            return (
                f"ret_eq={eq} vft={hex32(ru32(emu, selfb))} f2c={hex32(ru32(emu, selfb + 0x2C))}"
                f" f38={emu.read(selfb + 0x38, 1)[0]} f40={ru32(emu, selfb + 0x40)}"
                f" f4c={emu.read(selfb + 0x4C, 1)[0]} f4d={emu.read(selfb + 0x4D, 1)[0]}"
                f" f50={ru32(emu, selfb + 0x50)} f54={ru32(emu, selfb + 0x54)}"
            )
        return native

    for row in (("0", "0"), ("5", "9"), ("0xffffffff", "1")):
        v.case(0x00887D60, ["init_buildable_entry_887d60", *row], init_entry(int(row[0], 0), int(row[1], 0)))

    # 0x0089AF50 small vtable init.
    def init_heur(emu: Emulator) -> str:
        selfb = emu.alloc(b"\xCC" * 0x30)
        res = emu.call(0x0089AF50, [], ecx=selfb)
        if res.stack_cleanup != 0:
            return f"cleanup {res.stack_cleanup}"
        eq = 1 if res.eax == selfb else 0
        return (
            f"ret_eq={eq} vft={hex32(ru32(emu, selfb))} f04={ru32(emu, selfb + 4)}"
            f" f08={ru32(emu, selfb + 8)} f0c={ru32(emu, selfb + 0x0C)}"
            f" f14={ru32(emu, selfb + 0x14)} f18={ru32(emu, selfb + 0x18)}"
            f" f1c={ru32(emu, selfb + 0x1C)} f24={emu.read(selfb + 0x24, 1)[0]}"
        )

    v.case(0x0089AF50, ["init_heur_base_89af50"], init_heur)

    # 0x008AAB70 effective priority. x87 return, no args.
    def prio(base, override):
        def native(emu: Emulator) -> str:
            selfb = emu.alloc(bytes(0x30))
            pf32(emu, selfb + 8, base)
            pf32(emu, selfb + 0x24, override)
            res, bits = emu.call_float32(0x008AAB70, [], ecx=selfb)
            if res.stack_cleanup != 0:
                return f"cleanup {res.stack_cleanup}"
            return f"ret={hex32(bits)}"
        return native

    for row in (("1.0", "2.0"), ("-1.0", "4.5"), ("-1.0", "-2.0"), ("3.0", "3.0")):
        v.case(0x008AAB70, ["get_eff_prio_8aab70", *row], prio(fbits(row[0]), fbits(row[1])))


def run_bank(v: Verifier) -> None:
    # Shared account layout for 0x0088A0E0 and 0x008900B0.
    # args: count minPrio inOutMax cost rateScale w0C spent avail p0 c0 p1 c1
    def bank(va: int, kind: str, count, min_prio, in_max, cost, rate, weight, spent, avail, p0, c0, p1, c1):
        def native(emu: Emulator) -> str:
            acc = emu.alloc(bytes(0x30))
            pf32(emu, acc + 0x0C, weight)
            pf32(emu, acc + 0x1C, spent)
            pf32(emu, acc + 0x20, avail)
            n0 = emu.alloc(bytes(0x1C))
            n1 = emu.alloc(bytes(0x1C))
            pf32(emu, n0 + 0x14, p0)
            pf32(emu, n0 + 0x18, c0)
            pf32(emu, n1 + 0x14, p1)
            pf32(emu, n1 + 0x18, c1)
            sentinel = acc + 0x10
            if count == 0:
                pu32(emu, acc + 0x10, sentinel)
                pu32(emu, acc + 0x14, sentinel)
            elif count == 1:
                pu32(emu, acc + 0x10, n0)
                pu32(emu, acc + 0x14, n0)
                pu32(emu, n0, sentinel)
                pu32(emu, n0 + 4, sentinel)
            else:
                pu32(emu, acc + 0x10, n0)
                pu32(emu, acc + 0x14, n1)
                pu32(emu, n0, n1)
                pu32(emu, n0 + 4, sentinel)
                pu32(emu, n1, sentinel)
                pu32(emu, n1 + 4, n0)
            if kind == "sum":
                slot = emu.alloc(struct.pack("<f", in_max))
                res, bits = emu.call_float32(va, [struct.unpack("<I", struct.pack("<f", min_prio))[0], slot], ecx=acc)
                if res.stack_cleanup != 8:
                    return f"cleanup {res.stack_cleanup}"
                return f"ret={hex32(bits)} max={hex32(ru32(emu, slot))}"
            args = [
                struct.unpack("<I", struct.pack("<f", cost))[0],
                struct.unpack("<I", struct.pack("<f", min_prio))[0],
                struct.unpack("<I", struct.pack("<f", rate))[0],
            ]
            res = emu.call(va, args, ecx=acc)
            if res.stack_cleanup != 12:
                return f"cleanup {res.stack_cleanup}"
            return f"ret={res.eax & 0xFF}"
        return native

    rows = (
        ("0", "1", "0", "5", "1", "1", "0", "10", "2", "3", "1", "4"),
        ("1", "1.5", "0", "2", "1", "2", "0", "100", "1", "4", "0", "0"),
        ("2", "1", "0.5", "3", "2", "1", "1", "20", "0.5", "2", "3", "6"),
        ("2", "2", "2", "1", "1", "1", "0", "50", "2", "5", "2", "7"),
        ("2", "2", "1", "1", "1", "1", "0", "0", "2", "4", "0", "1"),
        ("2", "0", "10", "1", "1", "1", "0", "0", "5", "1", "4", "1"),
    )
    for row in rows:
        nums = [int(row[0])] + [fbits(x) for x in row[1:]]
        v.case(0x0088A0E0, ["sum_weighted_88a0e0", *row], bank(0x0088A0E0, "sum", *nums))
        v.case(0x008900B0, ["can_afford_8900b0", *row], bank(0x008900B0, "afford", *nums))


def econ_fields(emu: Emulator, selfb: int, eax: int) -> str:
    def rel(off: int, target: int) -> int:
        return 1 if ru32(emu, selfb + off) == (selfb + target) & 0xFFFFFFFF else 0

    return (
        f"ret_eq={1 if eax == selfb else 0} vft={hex32(ru32(emu, selfb))}"
        f" f04={hex32(ru32(emu, selfb + 4))} f08={hex32(ru32(emu, selfb + 8))}"
        f" f0c={ru32(emu, selfb + 0x0C)} self1c={rel(0x1C, 0x1C)} self20={rel(0x20, 0x1C)}"
        f" f24={ru32(emu, selfb + 0x24)} self48={rel(0x48, 0x48)} self4c={rel(0x4C, 0x48)}"
        f" f58={ru32(emu, selfb + 0x58)}"
    )


def run_more(v: Verifier) -> None:
    def init_owner(owner8: int, owner4: int):
        def native(emu: Emulator) -> str:
            selfb = emu.alloc(b"\xCC" * 0x60)
            res = emu.call(0x0088BC10, [owner8, owner4], ecx=selfb)
            if res.stack_cleanup != 8:
                return f"cleanup {res.stack_cleanup}"
            return econ_fields(emu, selfb, res.eax)
        return native

    for row in (("0", "0"), ("0x10", "0x20"), ("1", "0")):
        v.case(0x0088BC10, ["init_econ_owner_88bc10", *row], init_owner(int(row[0], 0), int(row[1], 0)))

    def init_default(emu: Emulator) -> str:
        selfb = emu.alloc(b"\xCC" * 0x60)
        res = emu.call(0x00897BB0, [], ecx=selfb)
        if res.stack_cleanup != 0:
            return f"cleanup {res.stack_cleanup}"
        return econ_fields(emu, selfb, res.eax)

    v.case(0x00897BB0, ["init_econ_default_897bb0", "0", "0"], init_default)
    v.case(0x00897BB0, ["init_econ_default_897bb0", "9", "8"], init_default)

    def samples(emu: Emulator, base: int, frames, values) -> tuple[int, int]:
        block = emu.alloc(bytes(0x100))
        for i, (frame, value) in enumerate(frames):
            pu32(emu, block + i * 8, frame)
            pf32(emu, block + i * 8 + 4, value)
        mapping = emu.alloc(struct.pack("<II", block, block))
        pu32(emu, base + 8, block)
        pu32(emu, base + 0x0C, block)
        pu32(emu, base + 0x10, block + 0x100)
        pu32(emu, base + 0x14, mapping)
        pu32(emu, base + 0x18, block + len(frames) * 8)
        pf32(emu, base + 0x2C, values)
        return block, mapping

    def prune(cur, window, count, init_sum, f0, v0, f1, v1):
        def native(emu: Emulator) -> str:
            gl = emu.alloc(bytes(0x80))
            pu32(emu, gl + 0x58, cur)
            pu32(emu, GAME_LOGIC, gl)
            win = emu.alloc(bytes(0x40))
            frames = [(f0, v0), (f1, v1)][:count]
            samples(emu, win, frames, init_sum)
            pu32(emu, win + 0x34, window)
            res = emu.call(0x0088BE90, [], ecx=win)
            if res.stack_cleanup != 0:
                return f"cleanup {res.stack_cleanup}"
            rem = (ru32(emu, win + 0x18) - ru32(emu, win + 8)) // 8
            return f"rem={rem} sum={hex32(ru32(emu, win + 0x2C))}"
        return native

    def update_win(cur, window, count, init_sum, f0, v0, f1, v1):
        def native(emu: Emulator) -> str:
            gl = emu.alloc(bytes(0x80))
            pu32(emu, gl + 0x58, cur)
            pu32(emu, GAME_LOGIC, gl)
            selfb = emu.alloc(bytes(0x80))
            frames = [(f0, v0), (f1, v1)][:count]
            samples(emu, selfb + 8, frames, init_sum)
            samples(emu, selfb + 0x40, frames, init_sum)
            pu32(emu, selfb + 8 + 0x34, window)
            pu32(emu, selfb + 0x40 + 0x34, window)
            res = emu.call(0x00897ED0, [], ecx=selfb)
            if res.stack_cleanup != 0:
                return f"cleanup {res.stack_cleanup}"
            rem0 = (ru32(emu, selfb + 8 + 0x18) - ru32(emu, selfb + 8 + 8)) // 8
            rem1 = (ru32(emu, selfb + 0x40 + 0x18) - ru32(emu, selfb + 0x40 + 8)) // 8
            return (
                f"rem0={rem0} sum0={hex32(ru32(emu, selfb + 8 + 0x2C))}"
                f" rem1={rem1} sum1={hex32(ru32(emu, selfb + 0x40 + 0x2C))}"
            )
        return native

    sample_rows = (
        ("100", "10", "2", "5", "80", "1.5", "95", "2.5"),
        ("100", "50", "2", "5", "40", "1", "60", "4"),
        ("10", "100", "2", "3", "1", "9", "2", "1"),
        ("30", "5", "1", "0", "10", "2", "0", "0"),
        ("20", "0", "0", "1.25", "0", "0", "0", "0"),
    )
    for row in sample_rows:
        nums = [int(row[0], 0), int(row[1], 0), int(row[2], 0), fbits(row[3]),
                int(row[4], 0), fbits(row[5]), int(row[6], 0), fbits(row[7])]
        v.case(0x0088BE90, ["prune_samples_88be90", *row], prune(*nums))
        v.case(0x00897ED0, ["update_econ_window_897ed0", *row], update_win(*nums))

    def erase(total, begin, end):
        def native(emu: Emulator) -> str:
            buf = emu.alloc(bytes(6 * 0x44))
            for i in range(total):
                pu32(emu, buf + i * 0x44, 10 + i)
                pu32(emu, buf + i * 0x44 + 0x40, 100 + i)
            selfb = emu.alloc(bytes(0x10))
            pu32(emu, selfb, buf)
            pu32(emu, selfb + 4, buf + total * 0x44)
            res = emu.call(0x0089DD90, [buf + begin * 0x44, buf + end * 0x44], ecx=selfb)
            if res.stack_cleanup != 8:
                return f"cleanup {res.stack_cleanup}"
            new_total = (ru32(emu, selfb + 4) - buf) // 0x44
            ret_idx = (res.eax - buf) // 0x44
            return (
                f"retIdx={ret_idx} newTotal={new_total} v0={ru32(emu, buf)}"
                f" v1={ru32(emu, buf + 0x44)} v2={ru32(emu, buf + 2 * 0x44)}"
            )
        return native

    for row in (("4", "1", "3"), ("3", "0", "1"), ("3", "2", "2"), ("1", "0", "1")):
        v.case(0x0089DD90, ["erase_cand_range_89dd90", *row], erase(int(row[0]), int(row[1]), int(row[2])))

    def delay(has_gl, scale, fps):
        def native(emu: Emulator) -> str:
            if has_gl:
                gl = emu.alloc(bytes(0x1D0))
                pf32(emu, gl + 0x1C4, scale)
                pu32(emu, GAME_LOGIC, gl)
            else:
                pu32(emu, GAME_LOGIC, 0)
            pf32(emu, FRAMES_PER_SECOND, fps)
            res, bits = emu.call_float32(0x008AAC20, [0], ecx=0)
            if res.stack_cleanup != 4:
                return f"cleanup {res.stack_cleanup}"
            return f"ret={hex32(bits)}"
        return native

    for row in (("1", "2", "30"), ("1", "2", "15"), ("1", "1.5", "10"), ("1", "0", "30")):
        v.case(0x008AAC20, ["get_scaled_delay_8aac20", *row], delay(int(row[0]), fbits(row[1]), fbits(row[2])))

    def vpc(va: int, value, cost14, cost15):
        def native(emu: Emulator) -> str:
            def ret_float(number: float):
                def stub(emu_: Emulator) -> int:
                    set_st0(emu_, number)
                    return 0
                return stub
            s13 = emu.bind_stub(ret_float(value), 4)
            s14 = emu.bind_stub(ret_float(cost14), 4)
            s15 = emu.bind_stub(ret_float(cost15), 4)
            vt = emu.alloc(struct.pack("<16I", *([0] * 13), s13, s14, s15))
            obj = emu.alloc(struct.pack("<I", vt))
            res, bits = emu.call_float32(va, [0], ecx=obj)
            if res.stack_cleanup != 4:
                return f"cleanup {res.stack_cleanup}"
            return f"ret={hex32(bits)}"
        return native

    for row in (("8", "2", "4"), ("9", "3", "3"), ("1", "1", "1"), ("7.5", "2.5", "5")):
        nums = tuple(fbits(x) for x in row)
        v.case(0x008AABB0, ["get_struct_vpc_8aabb0", *row], vpc(0x008AABB0, *nums))
        v.case(0x008AABE0, ["get_unit_vpc_8aabe0", *row], vpc(0x008AABE0, *nums))

    def move(va: int, obj, act_count, a0, a1, cmp_count, c0, c1):
        def native(emu: Emulator) -> str:
            act = emu.alloc(struct.pack("<4I", a0, a1, 0, 0))
            cmpb = emu.alloc(struct.pack("<4I", c0, c1, 0, 0))
            selfb = emu.alloc(bytes(0x80))
            pu32(emu, selfb + 0x5C, act)
            pu32(emu, selfb + 0x60, act + act_count * 4)
            pu32(emu, selfb + 0x64, act + 16)
            pu32(emu, selfb + 0x6C, cmpb)
            pu32(emu, selfb + 0x70, cmpb + cmp_count * 4)
            pu32(emu, selfb + 0x74, cmpb + 16)
            res = emu.call(va, [obj], ecx=selfb)
            if res.stack_cleanup != 4:
                return f"cleanup {res.stack_cleanup}"
            new_act = (ru32(emu, selfb + 0x60) - act) // 4
            new_cmp = (ru32(emu, selfb + 0x70) - cmpb) // 4
            return (
                f"actCnt={new_act} a0={ru32(emu, act)} a1={ru32(emu, act + 4)} a2={ru32(emu, act + 8)}"
                f" cmpCnt={new_cmp} c0={ru32(emu, cmpb)} c1={ru32(emu, cmpb + 4)} c2={ru32(emu, cmpb + 8)}"
            )
        return native

    for row in (
        ("5", "1", "1", "2", "0", "0", "0"),
        ("7", "2", "3", "7", "1", "7", "8"),
        ("4", "0", "0", "0", "2", "4", "9"),
        ("9", "2", "9", "1", "2", "2", "3"),
    ):
        nums = tuple(int(x, 0) for x in row)
        v.case(0x008AEEB0, ["move_to_under_const_8aeeb0", *row], move(0x008AEEB0, *nums))
        v.case(0x008AEF30, ["move_to_completed_8aef30", *row], move(0x008AEF30, *nums))


def run_rest(v: Verifier) -> None:
    # 0x0088BE50. args: useTarget8 queryKey k0 v0 k1 v1
    def find_stat(use_target: int, query: int, k0: int, v0: int, k1: int, v1: int):
        def native(emu: Emulator) -> str:
            n0 = emu.alloc(bytes(0x30))
            n1 = emu.alloc(bytes(0x30))
            pu32(emu, n0 + 0x10, k0)
            pu32(emu, n0 + 0x14, v0)
            pu32(emu, n1 + 0x10, k1)
            pu32(emu, n1 + 0x14, v1)
            pu32(emu, n1 + 4, n0)
            target = emu.alloc(bytes(0x60))
            selfb = emu.alloc(bytes(0x60))
            pu32(emu, target + 0x50, n1)
            pu32(emu, selfb + 0x50, n1)
            pu32(emu, selfb + 8, target if use_target else 0)
            res = emu.call(0x0088BE50, [query], ecx=selfb)
            if res.stack_cleanup != 4:
                return f"cleanup {res.stack_cleanup}"
            return f"ret={res.eax & 0xFFFFFFFF}"
        return native

    for row in (
        ("0", "10", "10", "3", "20", "9"),
        ("1", "20", "10", "3", "20", "9"),
        ("0", "15", "10", "3", "20", "9"),
        ("1", "5", "10", "3", "20", "9"),
    ):
        nums = tuple(int(x, 0) for x in row)
        v.case(0x0088BE50, ["find_stat_val_88be50", *row], find_stat(*nums))

    # 0x0088D120. args: count targetTid t0 t1
    def group(count: int, target: int, t0: int, t1: int):
        def native(emu: Emulator) -> str:
            e0 = emu.alloc(bytes(0x20))
            e1 = emu.alloc(bytes(0x20))
            pu32(emu, e0 + 0x10, t0)
            pu32(emu, e1 + 0x10, t1)
            n0 = emu.alloc(bytes(0x30))
            n1 = emu.alloc(bytes(0x30))
            pu32(emu, n0 + 0x10, 1)
            pu32(emu, n0 + 0x14, e0)
            pu32(emu, n1 + 0x10, 2)
            pu32(emu, n1 + 0x14, e1)
            pu32(emu, n1 + 4, n0)
            owner = emu.alloc(bytes(0x30))
            pu32(emu, owner + 0x14, n1)
            keys = emu.alloc(struct.pack("<II", 1, 2))
            grp = emu.alloc(bytes(0x20))
            pu32(emu, grp + 0x0C, owner)
            pu32(emu, grp + 0x10, keys)
            pu32(emu, grp + 0x14, keys + count * 4)
            res = emu.call(0x0088D120, [target], ecx=grp)
            if res.stack_cleanup != 4:
                return f"cleanup {res.stack_cleanup}"
            return f"ret={res.eax & 0xFF}"
        return native

    for row in (("0", "7", "7", "8"), ("2", "7", "7", "8"), ("2", "9", "7", "8"), ("1", "8", "7", "8"), ("1", "7", "7", "8")):
        nums = tuple(int(x, 0) for x in row)
        v.case(0x0088D120, ["group_contains_88d120", *row], group(*nums))

    # 0x00890600. args: req forb pref disl mode id0 fl0 cnt0 m0 x0 id1 fl1 cnt1 m1 x1 refX
    def better(args: tuple[str, ...]):
        def native(emu: Emulator) -> str:
            structs = emu.alloc(bytes(16))
            ref = emu.alloc(struct.pack("<fff", fbits(args[15]), 0.0, 0.0))

            def one(ident: int, flags: int, count: int, metric: int, x: float) -> int:
                base = emu.alloc(bytes(0x44))
                emu.write(base + 4, struct.pack("<i", ident))
                pf32(emu, base + 0x10, x)
                pu32(emu, base + 0x2C, flags)
                pu32(emu, base + 0x30, structs)
                pu32(emu, base + 0x34, structs + count * 4)
                pu32(emu, base + 0x40, metric)
                return base

            b0 = one(int(args[5], 0), int(args[6], 0), int(args[7], 0), int(args[8], 0), fbits(args[9]))
            b1 = one(int(args[10], 0), int(args[11], 0), int(args[12], 0), int(args[13], 0), fbits(args[14]))
            crit = emu.alloc(bytes(0x18))
            for off, text in ((0, args[0]), (4, args[1]), (8, args[2]), (0xC, args[3]), (0x10, args[4])):
                pu32(emu, crit + off, int(text, 0))
            pu32(emu, crit + 0x14, ref)
            res = emu.call(0x00890600, [b0, b1], ecx=crit)
            if res.stack_cleanup != 8:
                return f"cleanup {res.stack_cleanup}"
            return f"ret={res.eax & 0xFF}"
        return native

    better_rows = (
        ("0", "0", "0", "0", "0", "1", "0", "0", "0", "0", "2", "0", "0", "0", "0", "0"),
        ("0", "0", "0", "0", "0", "5", "0", "0", "0", "0", "2", "0", "0", "0", "0", "0"),
        ("0", "0", "0", "0", "1", "5", "0", "0", "0", "0", "2", "0", "0", "0", "0", "0"),
        ("0", "0", "0", "0", "2", "1", "0", "1", "0", "0", "2", "0", "3", "0", "0", "0"),
        ("0", "0", "1", "0", "0", "9", "1", "0", "0", "0", "1", "0", "0", "0", "0", "0"),
        ("0", "0", "0", "0", "5", "3", "0", "0", "4", "0", "8", "0", "0", "9", "0", "0"),
    )
    for row in better_rows:
        v.case(0x00890600, ["is_candidate_better_890600", *row], better(row))

    # 0x008908C0. Same args. required mask 0xFFFFFFFF selects the null config.
    def find_best(args: tuple[str, ...]):
        def native(emu: Emulator) -> str:
            structs = emu.alloc(bytes(16))
            ref = emu.alloc(struct.pack("<fff", fbits(args[15]), 0.0, 0.0))

            def one(ident: int, flags: int, count: int, metric: int, x: float) -> int:
                base = emu.alloc(bytes(0x44))
                emu.write(base + 4, struct.pack("<i", ident))
                pf32(emu, base + 0x10, x)
                pu32(emu, base + 0x2C, flags)
                pu32(emu, base + 0x30, structs)
                pu32(emu, base + 0x34, structs + count * 4)
                pu32(emu, base + 0x40, metric)
                return base

            b0 = one(int(args[5], 0), int(args[6], 0), int(args[7], 0), int(args[8], 0), fbits(args[9]))
            b1 = one(int(args[10], 0), int(args[11], 0), int(args[12], 0), int(args[13], 0), fbits(args[14]))
            arr = emu.alloc(struct.pack("<II", b0, b1))
            selfb = emu.alloc(bytes(0x34))
            pu32(emu, selfb + 0x28, arr)
            pu32(emu, selfb + 0x2C, arr + 8)
            req = int(args[0], 0)
            cfg = 0
            if req != 0xFFFFFFFF:
                cfg = emu.alloc(bytes(0x14))
                for off, text in ((0, args[0]), (4, args[1]), (8, args[2]), (0xC, args[3]), (0x10, args[4])):
                    pu32(emu, cfg + off, int(text, 0))
            res = emu.call(0x008908C0, [cfg, ref], ecx=selfb)
            if res.stack_cleanup != 8:
                return f"cleanup {res.stack_cleanup}"
            idx = 0 if res.eax == b0 else 1 if res.eax == b1 else -1
            return f"best={idx}"
        return native

    best_rows = (
        ("4294967295", "0", "0", "0", "1", "1", "0", "0", "0", "0", "5", "0", "0", "0", "0", "0"),
        ("4294967295", "0", "0", "0", "1", "1", "4", "0", "0", "0", "5", "0", "0", "0", "0", "0"),
        ("4294967295", "0", "0", "0", "1", "1", "4", "0", "0", "0", "5", "4", "0", "0", "0", "0"),
        ("1", "0", "0", "0", "0", "3", "1", "0", "0", "0", "2", "0", "0", "0", "0", "0"),
    )
    for row in best_rows:
        v.case(0x008908C0, ["find_best_base_8908c0", *row], find_best(row))

    # 0x00890400 / 0x0089A6D0 / 0x0089A740. args: objId inHash dead ox oy oz fx fy fz
    def placed(op: str, obj_id: int, in_hash: int, dead: int, ox, oy, oz, fx, fy, fz):
        def native(emu: Emulator) -> str:
            obj = emu.alloc(bytes(0x480))
            pf32(emu, obj + 0x38, ox)
            pf32(emu, obj + 0x3C, oy)
            pf32(emu, obj + 0x40, oz)
            emu.write(obj + 0x471, bytes([dead & 0xFF]))
            logic = emu.alloc(bytes(0x200))
            sentinel = emu.alloc(bytes(0x10))
            buckets = emu.alloc(struct.pack("<5I", 0, 0, 0, 0, sentinel))
            pu32(emu, logic + 0xC8, buckets)
            pu32(emu, logic + 0xCC, 4)
            if in_hash and obj_id:
                node = emu.alloc(bytes(0x10))
                pu32(emu, node, obj_id)
                pu32(emu, node + 4, obj)
                slot = obj_id % 4
                pu32(emu, buckets + slot * 4, node)
            pu32(emu, GAME_LOGIC, logic)
            if op == "get_base_pos_890400":
                base = emu.alloc(bytes(0x44))
                pu32(emu, base + 0x0C, obj_id)
                pf32(emu, base + 0x10, fx)
                pf32(emu, base + 0x14, fy)
                pf32(emu, base + 0x18, fz)
                res = emu.call(0x00890400, [], ecx=base)
                if res.stack_cleanup != 0 or res.eax == 0:
                    return f"cleanup {res.stack_cleanup} eax {res.eax:x}"
                a, b, c = struct.unpack("<III", emu.read(res.eax, 12))
                return f"x={hex32(a)} y={hex32(b)} z={hex32(c)}"
            entry = emu.alloc(bytes(0x30))
            pf32(emu, entry + 0x10, fx)
            pf32(emu, entry + 0x14, fy)
            pf32(emu, entry + 0x18, fz)
            pu32(emu, entry + 0x20, obj_id)
            if op == "resolve_struct_obj_89a6d0":
                res = emu.call(0x0089A6D0, [], ecx=entry)
                if res.stack_cleanup != 0:
                    return f"cleanup {res.stack_cleanup}"
                return f"found={1 if res.eax == obj else 0}"
            res = emu.call(0x0089A740, [], ecx=entry)
            if res.stack_cleanup != 0 or res.eax == 0:
                return f"cleanup {res.stack_cleanup} eax {res.eax:x}"
            a, b, c = struct.unpack("<III", emu.read(res.eax, 12))
            return f"x={hex32(a)} y={hex32(b)} z={hex32(c)}"
        return native

    place_rows = (
        ("0", "0", "0", "1", "2", "3", "4", "5", "6"),
        ("7", "1", "0", "1", "2", "3", "4", "5", "6"),
        ("7", "1", "1", "1", "2", "3", "4", "5", "6"),
        ("7", "0", "0", "1", "2", "3", "4", "5", "6"),
    )
    for op, va in (
        ("get_base_pos_890400", 0x00890400),
        ("resolve_struct_obj_89a6d0", 0x0089A6D0),
        ("get_struct_pos_89a740", 0x0089A740),
    ):
        for row in place_rows:
            nums = (int(row[0], 0), int(row[1], 0), int(row[2], 0), *(fbits(t) for t in row[3:]))
            v.case(va, [op, *row], placed(op, *nums))



def run_tail(v: Verifier) -> None:
    # 0x008878C0. args: outerKey innerKey kOuter kInner0 vInner0 kInner1 vInner1
    def nested(outer_key: int, inner_key: int, k_outer: int, k0: int, v0: int, k1: int, v1: int):
        def native(emu: Emulator) -> str:
            n0 = emu.alloc(bytes(0x30))
            n1 = emu.alloc(bytes(0x30))
            emu.write(n0 + 0x10, struct.pack("<i", k0))
            pu32(emu, n0 + 0x14, v0)
            pu32(emu, n1 + 4, n0)
            emu.write(n1 + 0x10, struct.pack("<i", k1))
            pu32(emu, n1 + 0x14, v1)
            outer = emu.alloc(bytes(0x40))
            emu.write(outer + 0x10, struct.pack("<i", k_outer))
            pu32(emu, outer + 0x20, n1)
            selfb = emu.alloc(bytes(0x30))
            pu32(emu, selfb + 0x10, outer)
            res = emu.call(0x008878C0, [outer_key & 0xFFFFFFFF, inner_key & 0xFFFFFFFF], ecx=selfb)
            if res.stack_cleanup != 8:
                return f"cleanup {res.stack_cleanup}"
            return f"ret={res.eax & 0xFFFFFFFF}"
        return native

    for row in (
        ("1", "10", "1", "10", "4", "20", "8"),
        ("1", "20", "1", "10", "4", "20", "8"),
        ("1", "15", "1", "10", "4", "20", "8"),
        ("2", "10", "1", "10", "4", "20", "8"),
    ):
        nums = tuple(int(x, 0) for x in row)
        v.case(0x008878C0, ["lookup_nested_8878c0", *row], nested(*nums))

    # 0x0088D1A0 reuses the group layout, then one outer/inner node.
    def catmap(count: int, target: int, t0: int, t1: int):
        def native(emu: Emulator) -> str:
            e0 = emu.alloc(bytes(0x20))
            e1 = emu.alloc(bytes(0x20))
            pu32(emu, e0 + 0x10, t0)
            pu32(emu, e1 + 0x10, t1)
            rn0 = emu.alloc(bytes(0x30))
            rn1 = emu.alloc(bytes(0x30))
            pu32(emu, rn0 + 0x10, 1)
            pu32(emu, rn0 + 0x14, e0)
            pu32(emu, rn1 + 0x10, 2)
            pu32(emu, rn1 + 0x14, e1)
            pu32(emu, rn1 + 4, rn0)
            owner = emu.alloc(bytes(0x30))
            pu32(emu, owner + 0x14, rn1)
            keys = emu.alloc(struct.pack("<II", 1, 2))
            grp = emu.alloc(bytes(0x20))
            pu32(emu, grp + 0x0C, owner)
            pu32(emu, grp + 0x10, keys)
            pu32(emu, grp + 0x14, keys + count * 4)
            selfb = emu.alloc(bytes(0x30))
            outer = emu.alloc(bytes(0x40))
            inner = emu.alloc(bytes(0x30))
            emu.write(selfb + 8 + 0x2D, b"\x01")
            # Real increment follows a non-null right child. End nodes have null children
            # and a parent equal to the end sentinel, which is what the walk compares against.
            pu32(emu, inner + 8, outer + 0x18)
            pu32(emu, inner + 0x14, grp)
            pu32(emu, outer + 8, selfb + 8)
            pu32(emu, outer + 0x1C, inner if count else outer + 0x18)
            pu32(emu, selfb + 0x0C, outer if count else selfb + 8)
            res = emu.call(0x0088D1A0, [target], ecx=selfb)
            if res.stack_cleanup != 4:
                return f"cleanup {res.stack_cleanup}"
            return f"ret={res.eax & 0xFF}"
        return native

    for row in (("0", "7", "7", "8"), ("2", "7", "7", "8"), ("2", "9", "7", "8"), ("1", "8", "7", "8")):
        nums = tuple(int(x, 0) for x in row)
        v.case(0x0088D1A0, ["catmap_contains_88d1a0", *row], catmap(*nums))

    # 0x0088DE50 / 0x0088DEC0. args: queryKey k0 blk0 k1 blk1
    def walk(op: str, query: int, k0: int, b0: int, k1: int, b1: int):
        def native(emu: Emulator) -> str:
            selfb = emu.alloc(bytes(0x40))
            n0 = emu.alloc(bytes(0x30))
            n1 = emu.alloc(bytes(0x30))
            sent = selfb + 8
            emu.write(sent + 0x2D, b"\x01")
            pu32(emu, n0 + 0x10, k0)
            pu32(emu, n0 + 0x28, b0)
            pu32(emu, n0, n1)
            pu32(emu, n0 + 8, sent)
            pu32(emu, n1 + 0x10, k1)
            pu32(emu, n1 + 0x28, b1)
            pu32(emu, n1 + 8, n0)
            pu32(emu, sent + 8, n0)
            pu32(emu, selfb + 0x10, n0)
            it = emu.alloc(bytes(0x0C))
            if op == "advance_next_active_88de50":
                pu32(emu, it, n0)
                pu32(emu, it + 4, query)
                pu32(emu, it + 8, selfb)
                res = emu.call(0x0088DE50, [], ecx=it)
                if res.stack_cleanup != 0:
                    return f"cleanup {res.stack_cleanup}"
            else:
                res = emu.call(0x0088DEC0, [it, query], ecx=selfb)
                if res.stack_cleanup != 8:
                    return f"cleanup {res.stack_cleanup}"
            node = ru32(emu, it)
            which = 0 if node == n0 else 1 if node == n1 else 2 if node == sent else -1
            owner = ru32(emu, it + 8)
            return f"node={which} key={ru32(emu, it + 4)} hasOwner={1 if owner else 0}"
        return native

    for op, va in (("advance_next_active_88de50", 0x0088DE50), ("find_first_active_88dec0", 0x0088DEC0)):
        for row in (("10", "10", "0", "20", "0"), ("10", "10", "1", "20", "0"), ("15", "10", "0", "20", "1"), ("25", "10", "0", "20", "0")):
            nums = tuple(int(x, 0) for x in row)
            v.case(va, [op, *row], walk(op, *nums))

    def install_objects(emu: Emulator, pairs: list[tuple[int, int]]) -> None:
        logic = emu.alloc(bytes(0x200))
        sentinel = emu.alloc(bytes(0x10))
        slots = [0, 0, 0, 0, sentinel]
        buckets = emu.alloc(struct.pack("<5I", *slots))
        pu32(emu, logic + 0xC8, buckets)
        pu32(emu, logic + 0xCC, 4)
        for key, obj in pairs:
            if not key or not obj:
                continue
            node = emu.alloc(bytes(0x10))
            pu32(emu, node, key)
            pu32(emu, node + 4, obj)
            slot = key % 4
            prev = ru32(emu, buckets + slot * 4)
            pu32(emu, node + 8, prev)
            pu32(emu, buckets + slot * 4, node)
        pu32(emu, GAME_LOGIC, logic)

    # 0x0089D070 / 0x0089D0D0.
    def active(op: str, count: int, query: int, qx, qy, qz, e0, e1):
        def native(emu: Emulator) -> str:
            objs = []
            pairs = []
            for ident, _flag, in_hash, x in (e0, e1):
                obj = emu.alloc(bytes(0x480))
                pf32(emu, obj + 0x38, x)
                pf32(emu, obj + 0x3C, qy)
                pf32(emu, obj + 0x40, qz)
                objs.append(obj)
                if in_hash and ident:
                    pairs.append((ident, obj))
            install_objects(emu, pairs)
            entries = emu.alloc(bytes(2 * 0x2CC))
            for i, (ident, flag, _in_hash, x) in enumerate((e0, e1)):
                base = entries + i * 0x2CC
                pf32(emu, base + 0x10, x)
                pf32(emu, base + 0x14, qy)
                pf32(emu, base + 0x18, qz)
                pu32(emu, base + 0x20, ident)
                emu.write(base + 0x74, bytes([flag & 0xFF]))
            reg = emu.alloc(bytes(0x50))
            pu32(emu, reg + 0x38, entries)
            pu32(emu, reg + 0x3C, count)
            selfb = emu.alloc(bytes(0x700))
            pu32(emu, selfb + 0x6F4, reg)
            if op == "has_active_entry_id_89d070":
                res = emu.call(0x0089D070, [query], ecx=selfb)
                if res.stack_cleanup != 4:
                    return f"cleanup {res.stack_cleanup}"
            else:
                res = emu.call(
                    0x0089D0D0,
                    [struct.unpack("<I", struct.pack("<f", qx))[0],
                     struct.unpack("<I", struct.pack("<f", qy))[0],
                     struct.unpack("<I", struct.pack("<f", qz))[0]],
                    ecx=selfb,
                )
                if res.stack_cleanup != 12:
                    return f"cleanup {res.stack_cleanup}"
            return f"ret={res.eax & 0xFF}"
        return native

    active_rows = (
        ("1", "5", "1", "2", "3", "5", "0", "1", "1", "9", "0", "0", "8"),
        ("1", "5", "1", "2", "3", "5", "0", "0", "1", "9", "0", "0", "8"),
        ("2", "9", "8", "2", "3", "5", "0", "0", "1", "9", "1", "1", "8"),
        ("0", "5", "1", "2", "3", "5", "0", "1", "1", "9", "0", "1", "8"),
    )
    for op, va in (("has_active_entry_id_89d070", 0x0089D070), ("has_active_entry_pos_89d0d0", 0x0089D0D0)):
        for row in active_rows:
            e0 = (int(row[5], 0), int(row[6], 0), int(row[7], 0), fbits(row[8]))
            e1 = (int(row[9], 0), int(row[10], 0), int(row[11], 0), fbits(row[12]))
            v.case(
                va,
                [op, *row],
                active(op, int(row[0], 0), int(row[1], 0), fbits(row[2]), fbits(row[3]), fbits(row[4]), e0, e1),
            )

    # 0x00893880 / 0x00893920 / 0x00893A00. args: queryKey nodeKey objId auxId inHash dead
    def slots(op: str, query: int, node_key: int, obj_id: int, aux: int, in_hash: int, dead: int):
        def native(emu: Emulator) -> str:
            obj = emu.alloc(bytes(0x480))
            emu.write(obj + 0x471, bytes([dead & 0xFF]))
            install_objects(emu, [(obj_id, obj)] if in_hash and obj_id else [])
            node = emu.alloc(bytes(0x30))
            emu.write(node + 0x10, struct.pack("<i", node_key))
            pu32(emu, node + 0x14, obj_id)
            pu32(emu, node + 0x18, aux)
            selfb = emu.alloc(bytes(0xB0))
            pu32(emu, selfb + 0xA4, node)
            pu32(emu, selfb + 0x88, node)
            if op == "find_active_slot_893880":
                res = emu.call(0x00893880, [query & 0xFFFFFFFF], ecx=selfb)
                if res.stack_cleanup != 4:
                    return f"cleanup {res.stack_cleanup}"
                return f"found={1 if res.eax == obj else 0}"
            if op == "get_slot_aux_893920":
                res = emu.call(0x00893920, [query & 0xFFFFFFFF], ecx=selfb)
                if res.stack_cleanup != 4:
                    return f"cleanup {res.stack_cleanup}"
                return f"ret={res.eax & 0xFFFFFFFF}"
            cand = emu.alloc(bytes(0xE8))
            emu.write(cand + 0xDC, struct.pack("<i", query))
            res = emu.call(0x00893A00, [cand], ecx=selfb)
            if res.stack_cleanup != 4:
                return f"cleanup {res.stack_cleanup}"
            return f"ret={res.eax & 0xFF}"
        return native

    slot_rows = (
        ("3", "3", "11", "70", "1", "0"),
        ("3", "3", "11", "70", "1", "1"),
        ("4", "3", "11", "70", "1", "0"),
        ("3", "3", "11", "70", "0", "0"),
    )
    for op, va in (
        ("find_active_slot_893880", 0x00893880),
        ("get_slot_aux_893920", 0x00893920),
        ("has_alive_tracked_893a00", 0x00893A00),
    ):
        for row in slot_rows:
            nums = tuple(int(x, 0) for x in row)
            v.case(va, [op, *row], slots(op, *nums))

    # 0x0089FE10. args: actCount a0 a1 a2 b0 b1
    def prune(count: int, ids: tuple[int, int, int], structures: tuple[int, int]):
        def native(emu: Emulator) -> str:
            act = emu.alloc(struct.pack("<4I", ids[0], ids[1], ids[2], 0))
            bs = emu.alloc(struct.pack("<2I", *structures))
            base = emu.alloc(bytes(0x44))
            pu32(emu, base + 0x30, bs)
            pu32(emu, base + 0x34, bs + 8)
            arr = emu.alloc(struct.pack("<I", base))
            selfb = emu.alloc(bytes(0x70))
            pu32(emu, selfb + 0x28, arr)
            pu32(emu, selfb + 0x2C, arr + 4)
            pu32(emu, selfb + 0x5C, act)
            pu32(emu, selfb + 0x60, act + count * 4)
            res = emu.call(0x0089FE10, [], ecx=selfb)
            if res.stack_cleanup != 0:
                return f"cleanup {res.stack_cleanup}"
            rem = (ru32(emu, selfb + 0x60) - act) // 4
            return f"rem={rem} a0={ru32(emu, act)} a1={ru32(emu, act + 4)} a2={ru32(emu, act + 8)}"
        return native

    for row in (("3", "1", "2", "3", "2", "9"), ("2", "4", "5", "6", "1", "2"), ("0", "1", "2", "3", "1", "2"), ("3", "7", "7", "8", "7", "8")):
        nums = tuple(int(x, 0) for x in row)
        v.case(0x0089FE10, ["prune_assigned_89fe10", *row], prune(nums[0], nums[1:4], nums[4:6]))

    # Sorts. The criteria object is the stack frame starting at the return address,
    # so sort mode lives in the third argument after first/last.
    def bases(emu: Emulator, ids: tuple[int, int, int, int]) -> tuple[int, int]:
        objs = []
        for ident in ids:
            base = emu.alloc(bytes(0x44))
            emu.write(base + 4, struct.pack("<i", ident))
            objs.append(base)
        ptrs = emu.alloc(struct.pack("<4I", *objs))
        return ptrs, objs[0]

    def sort_bounded(mode: int, ids: tuple[int, int, int, int]):
        def native(emu: Emulator) -> str:
            ptrs, _ = bases(emu, ids)
            res = emu.call(0x0089E700, [ptrs, ptrs + 16, 0, 0, 0, 0, mode, 0])
            if res.stack_cleanup != 0:
                return f"cleanup {res.stack_cleanup}"
            order = []
            for i in range(4):
                order.append(str(struct.unpack("<i", emu.read(ru32(emu, ptrs + i * 4) + 4, 4))[0]))
            return "o=" + ",".join(order)
        return native

    def sort_open(mode: int, ids: tuple[int, int, int, int]):
        def native(emu: Emulator) -> str:
            sentinel = -999 if mode == 0 else 999
            ptrs, _ = bases(emu, (sentinel, *ids[1:]))
            res = emu.call(0x0089E750, [ptrs + 4, ptrs + 16, 0, 0, 0, 0, mode, 0])
            if res.stack_cleanup != 0:
                return f"cleanup {res.stack_cleanup}"
            order = []
            for i in range(4):
                order.append(str(struct.unpack("<i", emu.read(ru32(emu, ptrs + i * 4) + 4, 4))[0]))
            return "o=" + ",".join(order)
        return native

    for row in (("0", "3", "1", "4", "2"), ("1", "3", "1", "4", "2"), ("0", "5", "5", "1", "9"), ("1", "8", "2", "2", "7")):
        ids = tuple(int(x, 0) for x in row[1:])
        mode = int(row[0], 0)
        v.case(0x0089E700, ["sort_bounded_89e700", *row], sort_bounded(mode, ids))
        v.case(0x0089E750, ["sort_unguarded_89e750", *row], sort_open(mode, ids))

    def median(mode: int, ids: tuple[int, int, int, int]):
        def native(emu: Emulator) -> str:
            ptrs, _ = bases(emu, ids)
            res = emu.call(0x0089F140, [ptrs, ptrs + 4, ptrs + 8, 0, 0, 0, 0, mode, 0])
            if res.stack_cleanup != 0:
                return f"cleanup {res.stack_cleanup}"
            if res.eax < ptrs or (res.eax - ptrs) % 4:
                return f"eax {res.eax:x}"
            idx = (res.eax - ptrs) // 4
            ident = struct.unpack("<i", emu.read(ru32(emu, res.eax) + 4, 4))[0]
            return f"idx={idx} id={ident}"
        return native

    def partition(mode: int, ids: tuple[int, int, int, int]):
        def native(emu: Emulator) -> str:
            ptrs, _ = bases(emu, ids)
            res = emu.call(0x0089F1D0, [ptrs, ptrs + 16, ptrs + 4, 0, 0, 0, 0, mode, 0])
            if res.stack_cleanup != 0:
                return f"cleanup {res.stack_cleanup}"
            if res.eax < ptrs or (res.eax - ptrs) % 4:
                return f"eax {res.eax:x}"
            idx = (res.eax - ptrs) // 4
            order = [str(struct.unpack("<i", emu.read(ru32(emu, ptrs + i * 4) + 4, 4))[0]) for i in range(4)]
            return f"cut={idx} o=" + ",".join(order)
        return native

    for row in (("0", "3", "1", "4", "2"), ("1", "3", "1", "4", "2"), ("0", "5", "5", "1", "9"), ("1", "9", "2", "8", "2")):
        ids = tuple(int(x, 0) for x in row[1:])
        mode = int(row[0], 0)
        v.case(0x0089F140, ["median3_89f140", *row], median(mode, ids))
        v.case(0x0089F1D0, ["partition_89f1d0", *row], partition(mode, ids))



def run_queue(v: Verifier) -> None:
    # Shared argv: count targetTid builderId e0c e0tid e0state e0pwr e1c e1tid e1state e1pwr
    def queued(op: str, count: int, target: int, builder: int, e0, e1):
        def native(emu: Emulator) -> str:
            entries = []
            notified = {"value": 0}

            def on_notify(emu_: Emulator) -> int:
                ecx = emu_.register(x86_const.UC_X86_REG_ECX)
                notified["value"] = 10 if ecx == entries[0] else 20 if ecx == entries[1] else -1
                return 0

            fn = emu.bind_stub(on_notify, 4)
            vt = emu.alloc(struct.pack("<18I", *([0] * 17), fn))
            for spec in (e0, e1):
                entry = emu.alloc(bytes(0x30))
                pu32(emu, entry, vt)
                pu32(emu, entry + 0x0C, spec[0])
                pu32(emu, entry + 0x10, spec[1])
                emu.write(entry + 0x14, struct.pack("<i", spec[2]))
                entries.append(entry)
            n0 = emu.alloc(bytes(0x30))
            n1 = emu.alloc(bytes(0x30))
            pu32(emu, n0 + 0x10, 10)
            pu32(emu, n0 + 0x14, entries[0])
            pu32(emu, n1 + 0x10, 20)
            pu32(emu, n1 + 0x14, entries[1])
            pu32(emu, n1 + 4, n0)
            reg = emu.alloc(bytes(0x30))
            pu32(emu, reg + 0x14, n1)
            keys = emu.alloc(struct.pack("<II", 10, 20))
            selfb = emu.alloc(bytes(0x550))
            ctx = emu.alloc(bytes(4))
            pu32(emu, selfb + 0x264, ctx)
            pu32(emu, selfb + 0x538, reg)
            pu32(emu, selfb + 0x53C, keys)
            pu32(emu, selfb + 0x540, keys + count * 4)
            sentinel = emu.alloc(bytes(0x10))
            buckets = emu.alloc(struct.pack("<5I", 0, 0, 0, 0, sentinel))
            factory = emu.alloc(bytes(0x60))
            pu32(emu, factory + 0x2C, buckets)
            pu32(emu, factory + 0x30, 4)
            for spec in (e0, e1):
                tid = spec[1]
                if not tid:
                    continue
                data = emu.alloc(bytes(0xA0))
                emu.write(data + 0x8C, struct.pack("<i", spec[3]))
                tmpl = emu.alloc(bytes(0x20))
                pu32(emu, tmpl + 0x14, data)
                node = emu.alloc(bytes(0x10))
                pu32(emu, node, tid)
                pu32(emu, node + 4, tmpl)
                slot = tid % 4
                pu32(emu, node + 8, ru32(emu, buckets + slot * 4))
                pu32(emu, buckets + slot * 4, node)
            pu32(emu, THING_FACTORY, factory)
            if op == "sum_queued_power_88ad80":
                res = emu.call(0x0088AD80, [], ecx=selfb)
                if res.stack_cleanup != 0:
                    return f"cleanup {res.stack_cleanup}"
                return f"ret={struct.unpack('<i', struct.pack('<I', res.eax & 0xFFFFFFFF))[0]}"
            tid_ptr = emu.alloc(struct.pack("<I", target))
            res = emu.call(0x0088AFC0, [builder, tid_ptr], ecx=selfb)
            if res.stack_cleanup != 8:
                return f"cleanup {res.stack_cleanup}"
            return f"notified={notified['value']}"
        return native

    rows = (
        ("0", "6", "9", "9", "6", "0", "5", "8", "8", "0", "-2"),
        ("1", "6", "9", "9", "6", "0", "5", "8", "8", "0", "-2"),
        ("2", "6", "9", "9", "6", "0", "5", "8", "8", "0", "-3"),
        ("1", "6", "9", "9", "99", "0", "5", "8", "8", "0", "-2"),
        ("2", "8", "8", "9", "6", "0", "5", "8", "8", "0", "-2"),
        ("1", "6", "9", "9", "6", "5", "5", "8", "8", "0", "-2"),
    )
    for row in rows[:4]:
        nums = [int(x, 0) for x in row]
        e0 = (nums[3], nums[4], nums[5], nums[6])
        e1 = (nums[7], nums[8], nums[9], nums[10])
        v.case(0x0088AD80, ["sum_queued_power_88ad80", *row], queued("sum_queued_power_88ad80", nums[0], nums[1], nums[2], e0, e1))
    for row in rows[1:]:
        nums = [int(x, 0) for x in row]
        e0 = (nums[3], nums[4], nums[5], nums[6])
        e1 = (nums[7], nums[8], nums[9], nums[10])
        v.case(0x0088AFC0, ["notify_constructed_88afc0", *row], queued("notify_constructed_88afc0", nums[0], nums[1], nums[2], e0, e1))


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--check", type=Path, default=CHECK_DEFAULT)
    parser.add_argument("--mutant", type=Path, default=MUTANT_DEFAULT)
    parser.add_argument("--skip-mutant", action="store_true")
    args = parser.parse_args()

    v = Verifier(args.check)
    run_leaves(v)
    run_bank(v)
    run_more(v)
    run_rest(v)
    run_tail(v)
    run_queue(v)
    total = sum(v.counts.values())
    print(f"skirmishai89 oracle: {len(v.counts)} entries, {total} cases, mismatches={len(v.mismatches)}")
    for line in v.mismatches[:30]:
        print(line)
    if v.mismatches:
        return 1
    if args.skip_mutant:
        return 0
    mutant = Verifier(args.mutant)
    run_leaves(mutant)
    run_bank(mutant)
    run_more(mutant)
    run_rest(mutant)
    run_tail(mutant)
    run_queue(mutant)
    print(f"mutant mismatches={len(mutant.mismatches)}")
    if not mutant.mismatches:
        print("mutant was not detected")
        return 1
    # The planted change is the priority compare inside 0x0088A0E0.
    if not any("0x0088A0E0" in line or "0x008900B0" in line for line in mutant.mismatches):
        print("mutant changed an unrelated function")
        for line in mutant.mismatches[:8]:
            print(line)
        return 1
    return 0


if __name__ == "__main__":
    sys.exit(main())
