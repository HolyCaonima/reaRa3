#!/usr/bin/env python3
"""Differential oracle for RA3 batch 3 inner-patched / entry-recoverable cave functions.

Arms each function's securom_sites.tsv slot to its runtime_target so the native
cave instruction and jump back to the continuation execute in Unicorn, then
compares full-width return values, memory mutations, and callback traces against
ra3_protcave3check.exe across all 28 reconstructed functions (84 cases).
"""

from __future__ import annotations

import argparse
import struct
import subprocess
import sys
from pathlib import Path
from typing import Callable

from unicorn import x86_const as r

SCRIPT_DIR = Path(__file__).resolve().parent
ANALYSIS_DIR = SCRIPT_DIR.parent
REPO = ANALYSIS_DIR.parents[1]
sys.path.insert(0, str(ANALYSIS_DIR))

from emulate import Emulator  # noqa: E402

CAP = Path(r"D:\Game\Ra3\RevRa3\work\runtime_dump\run35\snapshot_3_240s")

# (site_va, slot_va, runtime_target_va) for all 28 functions
CAVE_SITES = [
    (0x004CB268, 0x018F2E38, 0x0044DD7F),
    (0x004D66D0, 0x018F25C8, 0x00459C86),
    (0x004D99E5, 0x018ED488, 0x0042055A),
    (0x004DA716, 0x018F2570, 0x0044F45B),
    (0x004E5C95, 0x018F2BB4, 0x004440D2),
    (0x004EDD7C, 0x018EDA00, 0x004404A6),
    (0x00504522, 0x018ED658, 0x0040B62E),
    (0x0050A9D3, 0x018ED9F8, 0x0044026E),
    (0x00520120, 0x018F285C, 0x00401B26),
    (0x00523E62, 0x018ED3AC, 0x0040D226),
    (0x005242C3, 0x018EDA54, 0x004128B0),
    (0x0052D6BA, 0x018ED3A0, 0x0040A37E),
    (0x00535428, 0x018ED604, 0x0042DBC6),
    (0x00537A48, 0x018F29B8, 0x00420536),
    (0x0053844A, 0x018F2A04, 0x00456F76),
    (0x0055B534, 0x018ED650, 0x004036AA),
    (0x0055CC63, 0x018ED450, 0x004025EC),
    (0x00583900, 0x018EDBC4, 0x0043A04F),
    (0x005A4ED0, 0x018F27D4, 0x0040BA01),
    (0x005C5D76, 0x018ED958, 0x00450B64),
    (0x005EF115, 0x018F2B10, 0x0043A77D),
    (0x006232F0, 0x018F2E58, 0x00455491),
    (0x0064BD9A, 0x018F26AC, 0x00445F07),
    (0x00683144, 0x018F24C4, 0x00459661),
    (0x00685EB3, 0x018ED43C, 0x00459EF6),
    (0x006879B2, 0x018ED738, 0x00459DE0),
    (0x00688198, 0x018ED554, 0x004399A6),
    (0x0071C467, 0x018ED4FC, 0x00456E6C),
]


def u32(val: int) -> bytes:
    return struct.pack("<I", val & 0xFFFFFFFF)


def f32(val: float) -> bytes:
    return struct.pack("<f", val)


def read_u8(emu: Emulator, addr: int) -> int:
    return emu.read(addr, 1)[0]


def read_u16(emu: Emulator, addr: int) -> int:
    return struct.unpack("<H", emu.read(addr, 2))[0]


def read_u32(emu: Emulator, addr: int) -> int:
    return struct.unpack("<I", emu.read(addr, 4))[0]


def write_u8(emu: Emulator, addr: int, val: int) -> None:
    emu.write(addr, bytes([val & 0xFF]))


def write_u32(emu: Emulator, addr: int, val: int) -> None:
    emu.write(addr, u32(val))


def write_f32(emu: Emulator, addr: int, val: float) -> None:
    emu.write(addr, f32(val))


class CaveOracle:
    def __init__(self) -> None:
        self.emu = Emulator(capture=CAP)
        self.calls: list[tuple[int, int, int, int, int, int]] = []

    def reset(self) -> None:
        self.emu.reset_data()
        self.emu._function_stubs.clear()
        self.calls.clear()
        for _, slot_va, runtime_target_va in CAVE_SITES:
            write_u32(self.emu, slot_va, runtime_target_va)

    def hook_call(
        self,
        target_va: int,
        nargs: int,
        cleanup: int,
        ret_val: int = 0,
        side_effect: Callable[[Emulator], int | None] | None = None,
        zero_ecx: bool = False,
    ) -> None:
        def stub(e: Emulator) -> int:
            ecx = 0 if zero_ecx else e.register(r.UC_X86_REG_ECX)
            a0 = e.stack_arg(0) if nargs >= 1 else 0
            a1 = e.stack_arg(1) if nargs >= 2 else 0
            a2 = e.stack_arg(2) if nargs >= 3 else 0
            self.calls.append((target_va, ecx, nargs, a0, a1, a2))
            if side_effect is not None:
                custom = side_effect(e)
                if custom is not None:
                    return custom
            return ret_val

        self.emu.bind_function(target_va, stub, stack_cleanup=cleanup)

    def format_line(self, case_id: str, ret_val: int, m0: int, m1: int, m2: int) -> str:
        parts = [
            case_id,
            f"ret={ret_val & 0xFFFFFFFF}",
            f"m0={m0 & 0xFFFFFFFF}",
            f"m1={m1 & 0xFFFFFFFF}",
            f"m2={m2 & 0xFFFFFFFF}",
            f"count={len(self.calls)}",
        ]
        for idx, (tva, ecx, ac, a0, a1, a2) in enumerate(self.calls):
            parts.append(f"c{idx}={tva}:{ecx}:{ac}:{a0}:{a1}:{a2}")
        return " ".join(parts)


def run_all_native(oracle: CaveOracle) -> list[str]:
    out: list[str] = []
    emu = oracle.emu

    # 1. 0x004CB260
    for cid, self_va, init_d0, arg0 in [
        ("init4cb260_a", 0x60010000, 0xDEADBEEF, 0x60010100),
        ("init4cb260_b", 0x60010200, 0x12345678, 0),
        ("init4cb260_c", 0x60010400, 0xFFFFFFFF, 0x60010500),
    ]:
        oracle.reset()
        write_u32(emu, self_va, init_d0)
        oracle.hook_call(0x004CB200, 1, 4, ret_val=0)
        res = emu.call(0x004CB260, [arg0], ecx=self_va)
        assert res.stack_cleanup == 4
        out.append(oracle.format_line(cid, res.eax, read_u32(emu, self_va), 0, 0))

    # 2. 0x004D66D0
    for cid, child_va, init_ref, dtor_va in [
        ("dtor4d66d0_null", 0, 0, 0x00601000),
        ("dtor4d66d0_keep", 0x60020100, 3, 0x00601000),
        ("dtor4d66d0_free", 0x60020200, 1, 0x00601000),
    ]:
        oracle.reset()
        self_va = 0x60020000
        write_u32(emu, self_va + 0x00, 0x11111111)
        write_u32(emu, self_va + 0x04, child_va)
        if child_va:
            vt_va = 0x60020900
            write_u32(emu, vt_va + 0x00, dtor_va)
            write_u32(emu, child_va + 0x00, vt_va)
            write_u32(emu, child_va + 0x04, init_ref)
        oracle.hook_call(dtor_va, 1, 4, ret_val=0)
        res = emu.call(0x004D66D0, [], ecx=self_va)
        assert res.stack_cleanup == 0
        cref = read_u32(emu, child_va + 0x04) if child_va else 0
        out.append(oracle.format_line(cid, 0, read_u32(emu, self_va), cref, 0))

    # 3. 0x004D99E0
    for cid, self_va, req_size, max_pool, large_ret, pool_va, init_head, grow_ret, grown_head, next_node in [
        ("alloc4d99e0_large", 0x60030000, 0x200, 0x100, 0x60039900, 0x60030100, 0, 0, 0, 0),
        ("alloc4d99e0_ready", 0x60030000, 0x0C, 0x100, 0, 0x60030100, 0x60030200, 1, 0x60030200, 0x60030300),
        ("alloc4d99e0_grow", 0x60030000, 0x20, 0x100, 0, 0x60030100, 0, 1, 0x60030400, 0x60030500),
    ]:
        oracle.reset()
        write_u32(emu, 0x018F9C38, max_pool)
        write_u32(emu, pool_va + 0x10, init_head)
        active_head = init_head if init_head else grown_head
        if active_head:
            write_u32(emu, active_head, next_node)
        oracle.hook_call(0x004165A0, 3, 0, ret_val=large_ret, zero_ecx=True)
        oracle.hook_call(0x004D61B0, 0, 0, ret_val=0)
        oracle.hook_call(0x004D36C0, 1, 4, ret_val=pool_va)

        def on_grow(e: Emulator, pv: int = pool_va, gh: int = grown_head, gr: int = grow_ret) -> int:
            write_u32(e, pv + 0x10, gh)
            return gr

        oracle.hook_call(0x00A662A0, 2, 8, ret_val=grow_ret, side_effect=on_grow)
        res = emu.call(0x004D99E0, [req_size, 0], ecx=self_va)
        assert res.stack_cleanup == 8
        out.append(oracle.format_line(cid, res.eax, read_u32(emu, pool_va + 0x10), 0, 0))

    # 4. 0x004DA710
    for cid, self_va, dirty in [
        ("ctor4da710_a", 0x60040000, 0xCAFEBABE),
        ("ctor4da710_b", 0x60041000, 0xFFFFFFFF),
        ("ctor4da710_c", 0x60042000, 0x13572468),
    ]:
        oracle.reset()
        emu.write(self_va, bytes([0x5A]) * 0xA0)
        write_u32(emu, self_va + 0x8C, dirty)
        write_u32(emu, self_va + 0x90, dirty)
        write_u32(emu, self_va + 0x98, dirty)
        oracle.hook_call(0x004D6550, 0, 0, ret_val=self_va + 4)
        res = emu.call(0x004DA710, [], ecx=self_va)
        assert res.stack_cleanup == 0
        m0 = read_u32(emu, self_va + 0x00)
        m1 = read_u32(emu, self_va + 0x88)
        m2 = (
            read_u32(emu, self_va + 0x8C)
            | read_u32(emu, self_va + 0x90)
            | read_u8(emu, self_va + 0x94)
            | read_u32(emu, self_va + 0x98)
        )
        out.append(oracle.format_line(cid, res.eax, m0, m1, m2))

    # 5. 0x004E5C90
    # Case 1: empty
    oracle.reset()
    self_va = 0x60050000
    emu.write(self_va, b"\x00" * 0xB0)
    write_u32(emu, self_va + 0x8C, 0)
    write_u32(emu, self_va + 0x94, self_va + 0x94)
    write_u32(emu, self_va + 0x98, self_va + 0x94)
    write_u8(emu, self_va + 0xAA, 1)
    res = emu.call(0x004E5C90, [], ecx=self_va)
    assert res.stack_cleanup == 0
    out.append(
        oracle.format_line(
            "clear4e5c90_empty",
            0,
            read_u32(emu, self_va + 0x8C),
            read_u32(emu, self_va + 0x94),
            read_u8(emu, self_va + 0xAA),
        )
    )

    # Case 2: 2 nodes in list 8C
    oracle.reset()
    self_va = 0x60051000
    emu.write(self_va, b"\x00" * 0xB0)
    write_u32(emu, 0x60051100 + 0x14, 0x60051200)
    write_u32(emu, 0x60051200 + 0x14, 0)
    write_u32(emu, self_va + 0x8C, 0x60051100)
    write_u32(emu, self_va + 0x94, self_va + 0x94)
    write_u32(emu, self_va + 0x98, self_va + 0x94)
    write_u8(emu, self_va + 0xAA, 1)
    oracle.hook_call(0x004166B0, 1, 0, zero_ecx=True)
    res = emu.call(0x004E5C90, [], ecx=self_va)
    assert res.stack_cleanup == 0
    out.append(
        oracle.format_line(
            "clear4e5c90_list8c",
            0,
            read_u32(emu, self_va + 0x8C),
            read_u32(emu, self_va + 0x94),
            read_u8(emu, self_va + 0xAA),
        )
    )

    # Case 3: both lists populated
    oracle.reset()
    self_va = 0x60052000
    sent_va = self_va + 0x94
    emu.write(self_va, b"\x00" * 0xB0)
    write_u32(emu, 0x60052100 + 0x14, 0)
    write_u32(emu, 0x60052200 + 0x00, 0x60052300)
    write_u32(emu, 0x60052200 + 0x04, sent_va)
    write_u32(emu, 0x60052200 + 0x08, 0x60052900)
    write_u32(emu, 0x60052300 + 0x00, sent_va)
    write_u32(emu, 0x60052300 + 0x04, 0x60052200)
    write_u32(emu, 0x60052300 + 0x08, 0)
    write_u32(emu, self_va + 0x8C, 0x60052100)
    write_u32(emu, self_va + 0x94, 0x60052200)
    write_u32(emu, self_va + 0x98, 0x60052300)
    write_u8(emu, self_va + 0xAA, 7)
    oracle.hook_call(0x004166B0, 1, 0, zero_ecx=True)

    def on_4ce3d0(e: Emulator) -> int:
        out_ptr = e.stack_arg(0)
        write_u32(e, out_ptr, 0x88)
        return 0x77

    oracle.hook_call(0x004CE3D0, 1, 4, side_effect=on_4ce3d0)
    oracle.hook_call(0x004D4EA0, 1, 4)
    oracle.hook_call(0x004E29F0, 1, 4)
    res = emu.call(0x004E5C90, [], ecx=self_va)
    assert res.stack_cleanup == 0
    out.append(
        oracle.format_line(
            "clear4e5c90_both",
            0,
            read_u32(emu, self_va + 0x8C),
            read_u32(emu, self_va + 0x94),
            read_u8(emu, self_va + 0xAA),
        )
    )

    # 6. 0x004EDD70
    # Case 1: null target
    oracle.reset()
    self_va = 0x60060000
    write_u32(emu, self_va + 0x138, 0x60060138)
    write_u32(emu, self_va + 0x13C, 0x60060138)
    res = emu.call(0x004EDD70, [0], ecx=self_va)
    assert res.stack_cleanup == 4
    out.append(
        oracle.format_line(
            "deact4edd70_null",
            0,
            read_u32(emu, self_va + 0x138),
            read_u32(emu, self_va + 0x13C),
            0,
        )
    )

    # Case 2: flagged target
    oracle.reset()
    self_va = 0x60060000
    target_va = 0x60061000
    write_u32(emu, self_va + 0x138, 0x60060138)
    write_u32(emu, self_va + 0x13C, 0x60060138)
    write_u8(emu, target_va + 0x80, 0x01)
    res = emu.call(0x004EDD70, [target_va], ecx=self_va)
    assert res.stack_cleanup == 4
    out.append(
        oracle.format_line(
            "deact4edd70_flagged",
            0,
            read_u32(emu, self_va + 0x138),
            read_u32(emu, self_va + 0x13C),
            0,
        )
    )

    # Case 3: active target with 2 modules
    oracle.reset()
    self_va = 0x60060000
    target_va = 0x60061000
    alloc_node_va = 0x60062000
    arr_va = 0x60062500
    write_u32(emu, self_va + 0x138, self_va + 0x138)
    write_u32(emu, self_va + 0x13C, self_va + 0x138)
    write_u8(emu, target_va + 0x80, 0x02)
    write_u32(emu, target_va + 0x310, arr_va)
    write_u32(emu, arr_va + 0x00, 0x60063000)
    write_u32(emu, arr_va + 0x04, 0x60063200)
    write_u32(emu, arr_va + 0x08, 0)
    # mod 0: [mod+0x0C] -> vt+0x18 = 0x00603010 returns 0x60063100; sub->vt+0x00 = 0x00603020
    write_u32(emu, 0x60063000 + 0x0C, 0x60063800)
    write_u32(emu, 0x60063800 + 0x18, 0x00603010)
    write_u32(emu, 0x60063100 + 0x00, 0x60063880)
    write_u32(emu, 0x60063880 + 0x00, 0x00603020)
    # mod 1: [mod+0x0C] -> vt+0x18 = 0x00603030 returns 0
    write_u32(emu, 0x60063200 + 0x0C, 0x60063900)
    write_u32(emu, 0x60063900 + 0x18, 0x00603030)
    oracle.hook_call(0x00603010, 0, 0, ret_val=0x60063100)
    oracle.hook_call(0x00603020, 0, 0)
    oracle.hook_call(0x00603030, 0, 0, ret_val=0)
    oracle.hook_call(0x006D2020, 2, 8)
    oracle.hook_call(0x006D23F0, 1, 4)
    oracle.hook_call(0x004D99E0, 2, 8, ret_val=alloc_node_va)
    oracle.hook_call(0x006F01A0, 0, 0)
    res = emu.call(0x004EDD70, [target_va], ecx=self_va)
    assert res.stack_cleanup == 4
    out.append(
        oracle.format_line(
            "deact4edd70_active",
            0,
            read_u32(emu, self_va + 0x138),
            read_u32(emu, self_va + 0x13C),
            read_u32(emu, alloc_node_va + 0x08),
        )
    )

    # 7. 0x00504510
    # Case 1: self == other
    oracle.reset()
    self_va = 0x60070000
    write_u32(emu, self_va, 0x60070100)
    res = emu.call(0x00504510, [self_va], ecx=self_va)
    assert res.stack_cleanup == 4
    out.append(oracle.format_line("assign504510_same", res.eax, read_u32(emu, self_va), 0, 0))

    # Cases 2 & 3: keep vs free
    for cid, new_ref, old_ref in [
        ("assign504510_keep", 2, 2),
        ("assign504510_free", 4, 1),
    ]:
        oracle.reset()
        self_va = 0x60070000
        other_va = 0x60070010
        new_va = 0x60071000
        old_va = 0x60072000
        dtor_va = 0x00607010
        vt_va = 0x60072900
        write_u32(emu, self_va, old_va)
        write_u32(emu, other_va, new_va)
        write_u32(emu, new_va + 0x98, new_ref)
        write_u32(emu, old_va + 0x94, vt_va)
        write_u32(emu, old_va + 0x98, old_ref)
        write_u32(emu, vt_va + 0x00, dtor_va)
        oracle.hook_call(dtor_va, 1, 4)
        res = emu.call(0x00504510, [other_va], ecx=self_va)
        assert res.stack_cleanup == 4
        out.append(
            oracle.format_line(
                cid,
                res.eax,
                read_u32(emu, self_va),
                read_u32(emu, new_va + 0x98),
                read_u32(emu, old_va + 0x98),
            )
        )

    # 8. 0x0050A9C0
    for cid, idx30, found, scale_val, denom10, factor1c in [
        ("scale50a9c0_found_pos", 2, True, 0.75, 1.5, 4.0),
        ("scale50a9c0_found_zero", 1, True, 0.50, 0.0, 3.0),
        ("scale50a9c0_fallback", 3, False, 0.0, 2.25, 5.5),
    ]:
        oracle.reset()
        target_va = 0x60080000
        sub0c_va = 0x60080100
        vt08_va = 0x00608010
        key_va = 0x60080200
        entry_base = 0x00DEE290 + idx30 * 0x1FC
        found_node_va = 0x60080300 if found else (entry_base + 0x1E4)
        vt_va = 0x60080900
        write_u32(emu, target_va + 0x0C, sub0c_va)
        write_u32(emu, target_va + 0x30, idx30)
        write_u32(emu, sub0c_va + 0x00, vt_va)
        write_u32(emu, vt_va + 0x08, vt08_va)
        write_f32(emu, sub0c_va + 0x10, denom10)
        write_f32(emu, sub0c_va + 0x1C, factor1c)
        if found:
            write_f32(emu, found_node_va + 0x18, scale_val)
        oracle.hook_call(vt08_va, 0, 0, ret_val=key_va)

        def on_506df0(e: Emulator, fnv: int = found_node_va) -> int:
            out_ptr = e.stack_arg(0)
            write_u32(e, out_ptr, fnv)
            return out_ptr

        oracle.hook_call(0x00506DF0, 2, 8, side_effect=on_506df0)
        oracle.hook_call(0x00559D10, 1, 4)
        oracle.hook_call(0x00559D20, 1, 4)
        res = emu.call(0x0050A9C0, [target_va], ecx=0x6008F000)
        assert res.stack_cleanup == 4
        out.append(oracle.format_line(cid, 0, idx30, found_node_va, 0))

    # 9. 0x00520120
    for cid, init13c, arg0, listener_va, vt04_va in [
        ("bit520120_same", 0x15, 1, 0x60090200, 0x00609010),
        ("bit520120_set_notify", 0x05, 1, 0x60090200, 0x00609010),
        ("bit520120_clear_nolistener", 0x15, 0, 0, 0x00609010),
    ]:
        oracle.reset()
        self_va = 0x60090000
        write_u8(emu, self_va + 0x13C, init13c)
        write_u32(emu, self_va + 0x394, listener_va)
        if listener_va:
            vt_va = 0x60090900
            write_u32(emu, listener_va, vt_va)
            write_u32(emu, vt_va + 0x04, vt04_va)
        oracle.hook_call(vt04_va, 1, 4)
        res = emu.call(0x00520120, [arg0], ecx=self_va)
        assert res.stack_cleanup == 4
        out.append(oracle.format_line(cid, 0, read_u8(emu, self_va + 0x13C), listener_va, 0))

    # 10. 0x00523E60
    # Case 1: empty
    oracle.reset()
    self_va = 0x600A0000
    arr_va = 0x600A0100
    write_u32(emu, self_va + 0x178, arr_va)
    write_u32(emu, arr_va + 0x00, 0)
    res = emu.call(0x00523E60, [0x111, 0x222], ecx=self_va)
    assert res.stack_cleanup == 8
    out.append(oracle.format_line("query523e60_empty", res.eax & 0xFF, 0, 0, 0))

    # Case 2: miss
    oracle.reset()
    self_va = 0x600A0000
    arr_va = 0x600A0100
    write_u32(emu, self_va + 0x178, arr_va)
    write_u32(emu, arr_va + 0x00, 0x600A0200)
    write_u32(emu, arr_va + 0x04, 0x600A0300)
    write_u32(emu, arr_va + 0x08, 0)
    write_u32(emu, 0x600A0200, 0x600A0700)
    write_u32(emu, 0x600A0700 + 0xB0, 0x0060A010)
    write_u32(emu, 0x600A0300, 0x600A0800)
    write_u32(emu, 0x600A0800 + 0xB0, 0x0060A020)
    write_u32(emu, 0x600A0400, 0x600A0900)
    write_u32(emu, 0x600A0900 + 0x0C, 0x0060A030)
    oracle.hook_call(0x0060A010, 0, 0, ret_val=0)
    oracle.hook_call(0x0060A020, 0, 0, ret_val=0x600A0400)
    oracle.hook_call(0x0060A030, 2, 8, ret_val=0)
    res = emu.call(0x00523E60, [0x111, 0x222], ecx=self_va)
    assert res.stack_cleanup == 8
    out.append(oracle.format_line("query523e60_miss", res.eax & 0xFF, 0, 0, 0))

    # Case 3: hit
    oracle.reset()
    self_va = 0x600A0000
    arr_va = 0x600A0100
    write_u32(emu, self_va + 0x178, arr_va)
    write_u32(emu, arr_va + 0x00, 0x600A0200)
    write_u32(emu, arr_va + 0x04, 0x600A0300)
    write_u32(emu, arr_va + 0x08, 0)
    write_u32(emu, 0x600A0200, 0x600A0700)
    write_u32(emu, 0x600A0700 + 0xB0, 0x0060A010)
    write_u32(emu, 0x600A0400, 0x600A0900)
    write_u32(emu, 0x600A0900 + 0x0C, 0x0060A030)
    oracle.hook_call(0x0060A010, 0, 0, ret_val=0x600A0400)
    oracle.hook_call(0x0060A030, 2, 8, ret_val=1)
    res = emu.call(0x00523E60, [0x333, 0x444], ecx=self_va)
    assert res.stack_cleanup == 8
    out.append(oracle.format_line("query523e60_hit", res.eax & 0xFF, 0, 0, 0))

    # 11. 0x005242B0
    for cid, arg0, idx_ret in [
        ("mark5242b0_neg", 0x10, -1),
        ("mark5242b0_zero", 0x20, 0),
        ("mark5242b0_three", 0x30, 3),
    ]:
        oracle.reset()
        self_va = 0x600B0000
        arr224_va = 0x600B0300
        arr234_va = 0x600B0400
        emu.write(arr224_va, b"\x00" * 8)
        emu.write(arr234_va, b"\x00" * 8)
        write_u32(emu, self_va + 0x224, arr224_va)
        write_u32(emu, self_va + 0x234, arr234_va)
        oracle.hook_call(0x00524230, 2, 8, ret_val=idx_ret & 0xFFFFFFFF)
        res = emu.call(0x005242B0, [arg0], ecx=self_va)
        assert res.stack_cleanup == 4
        pidx = idx_ret if idx_ret >= 0 else 0
        out.append(
            oracle.format_line(
                cid,
                0,
                read_u8(emu, arr224_va + pidx),
                read_u8(emu, arr234_va + pidx),
                0,
            )
        )

    # 12. 0x0052D6B0
    for cid, arg0, g13f, obj138_va, h7886b0_ret in [
        ("vis52d6b0_global_on", 0x12345600, 1, 0x600C0200, 0),
        ("vis52d6b0_helper_false", 0xABCDEF00, 0, 0x600C0200, 0),
        ("vis52d6b0_helper_true", 0xABCDEF00, 0, 0x600C0200, 1),
    ]:
        oracle.reset()
        self_va = 0x600C0000
        g_deea50 = 0x600C0800
        write_u32(emu, 0x00DEEA50, g_deea50)
        write_u8(emu, g_deea50 + 0x13F, g13f)
        write_u32(emu, self_va + 0x138, obj138_va)
        oracle.hook_call(0x00529FF0, 2, 8, ret_val=0x600C0900)
        oracle.hook_call(0x007886B0, 1, 4, ret_val=h7886b0_ret)
        oracle.hook_call(0x005208B0, 1, 4)
        res = emu.call(0x0052D6B0, [arg0], ecx=self_va)
        assert res.stack_cleanup == 4
        out.append(oracle.format_line(cid, 0, read_u32(emu, self_va + 0x384), 0, 0))

    # 13. 0x00535410
    for cid, out_ptr_va, arg1, alloc_va in [
        ("create535410_ok1", 0x600D0000, 0x11223344, 0x600D0100),
        ("create535410_ok2", 0x600D0000, 0x99887766, 0x600D0200),
        ("create535410_null", 0x600D0000, 0x55555555, 0),
    ]:
        oracle.reset()
        emu.write(out_ptr_va, b"\xFF" * 8)
        if alloc_va:
            emu.write(alloc_va, b"\xAA" * 0x20)
        oracle.hook_call(0x00416820, 1, 0, ret_val=alloc_va, zero_ecx=True)
        res = emu.call(0x00535410, [out_ptr_va, arg1], ecx=0)
        assert res.stack_cleanup == 0
        m0 = read_u32(emu, out_ptr_va)
        m1 = read_u32(emu, alloc_va + 0x00) if alloc_va else 0
        m2 = (
            (read_u32(emu, alloc_va + 0x04) ^ read_u32(emu, alloc_va + 0x08) ^ read_u32(emu, alloc_va + 0x0C))
            if alloc_va
            else 0
        )
        out.append(oracle.format_line(cid, res.eax, m0, m1, m2))

    # 14. 0x00537A40
    for cid, init_flags, obj138_va in [
        ("rem537a40_unflagged", 0x20, 0x600E0500),
        ("rem537a40_with138", 0x65, 0x600E0500),
        ("rem537a40_no138", 0x41, 0),
    ]:
        oracle.reset()
        self_va = 0x600E0000
        target_va = 0x600E0100
        node_va = 0x600E0200
        g_deea50 = 0x600E0300
        pool_va = 0x600E0400
        g_deba64 = 0x600E0600
        write_u32(emu, 0x00DEBA64, g_deba64)
        write_u32(emu, g_deba64 + 0x58, 0x7788)
        write_u32(emu, 0x00DEEA50, g_deea50)
        write_u32(emu, g_deea50 + 0xBC, 0x99)
        write_u32(emu, self_va + 0x50, node_va)
        write_u32(emu, self_va + 0x54, node_va)
        write_u32(emu, self_va + 0x5C, 4)
        write_u8(emu, target_va + 0x13D, init_flags)
        write_u32(emu, target_va + 0x138, obj138_va)
        write_u32(emu, node_va + 0x00, self_va + 0x50)
        write_u32(emu, node_va + 0x04, self_va + 0x50)
        write_u32(emu, node_va + 0x08, target_va)
        write_u32(emu, pool_va + 0x10, 0x600E0900)
        oracle.hook_call(0x005240C0, 0, 0)
        oracle.hook_call(0x00524050, 0, 0)
        oracle.hook_call(0x004D36C0, 1, 4, ret_val=pool_va)
        oracle.hook_call(0x0085E950, 1, 4)
        res = emu.call(0x00537A40, [target_va], ecx=self_va)
        assert res.stack_cleanup == 4
        out.append(
            oracle.format_line(
                cid,
                0,
                read_u8(emu, target_va + 0x13D),
                read_u32(emu, self_va + 0x5C),
                read_u32(emu, pool_va + 0x10),
            )
        )

    # 15. 0x00538440
    # Case 1: empty
    oracle.reset()
    self_va = 0x600F0000
    source_va = 0x600F0100
    write_u32(emu, source_va + 0x10, 0)
    write_u32(emu, source_va + 0x14, 0x600F0180)
    res = emu.call(0x00538440, [source_va], ecx=self_va)
    assert res.stack_cleanup == 4
    out.append(
        oracle.format_line(
            "ctor538440_empty",
            res.eax,
            read_u32(emu, self_va + 0x00),
            read_u32(emu, self_va + 0x04),
            read_u32(emu, self_va + 0x08),
        )
    )

    # Case 2: 1 matching item
    oracle.reset()
    self_va = 0x600F0000
    source_va = 0x600F0100
    arr_va = 0x600F0180
    tree_node = 0x600F0600
    write_u32(emu, source_va + 0x10, 1)
    write_u32(emu, source_va + 0x14, arr_va)
    write_u32(emu, arr_va + 0x00, 0x600F0200)
    write_u32(emu, 0x600F0200 + 0x00, 10)
    write_u32(emu, 0x00DEEBBC, tree_node)
    write_u32(emu, tree_node + 0x00, 0)
    write_u32(emu, tree_node + 0x04, 0)
    write_u32(emu, tree_node + 0x10, 10)
    write_u32(emu, tree_node + 0x14, 0x600F0300)
    write_u32(emu, 0x600F0300, 0x600F0800)
    write_u32(emu, 0x600F0800 + 0x00, 0x0060F010)
    oracle.hook_call(0x0060F010, 1, 4, ret_val=0x600F0400)
    oracle.hook_call(0x00416860, 3, 0, ret_val=0x600F0500, zero_ecx=True)
    res = emu.call(0x00538440, [source_va], ecx=self_va)
    assert res.stack_cleanup == 4
    out.append(
        oracle.format_line(
            "ctor538440_one",
            res.eax,
            read_u32(emu, self_va + 0x04),
            read_u32(emu, self_va + 0x08),
            read_u32(emu, 0x600F0500 + 0x08),
        )
    )

    # Case 3: 2 items (first unmatched, second matched)
    oracle.reset()
    self_va = 0x600F0000
    source_va = 0x600F0100
    arr_va = 0x600F0180
    tree_node = 0x600F0600
    write_u32(emu, source_va + 0x10, 2)
    write_u32(emu, source_va + 0x14, arr_va)
    write_u32(emu, arr_va + 0x00, 0x600F0200)
    write_u32(emu, arr_va + 0x04, 0x600F0210)
    write_u32(emu, 0x600F0200 + 0x00, 5)
    write_u32(emu, 0x600F0210 + 0x00, 10)
    write_u32(emu, 0x00DEEBBC, tree_node)
    write_u32(emu, tree_node + 0x00, 0)
    write_u32(emu, tree_node + 0x04, 0)
    write_u32(emu, tree_node + 0x10, 10)
    write_u32(emu, tree_node + 0x14, 0x600F0300)
    write_u32(emu, 0x600F0300, 0x600F0800)
    write_u32(emu, 0x600F0800 + 0x00, 0x0060F010)
    oracle.hook_call(0x0060F010, 1, 4, ret_val=0x600F0410)
    oracle.hook_call(0x00416860, 3, 0, ret_val=0x600F0510, zero_ecx=True)
    res = emu.call(0x00538440, [source_va], ecx=self_va)
    assert res.stack_cleanup == 4
    out.append(
        oracle.format_line(
            "ctor538440_mixed",
            res.eax,
            read_u32(emu, self_va + 0x04),
            read_u32(emu, self_va + 0x08),
            read_u32(emu, 0x600F0510 + 0x08),
        )
    )

    # 16. 0x0055B530
    for cid, init_ref in [
        ("dtor55b530_allnull", 0),
        ("dtor55b530_keep", 2),
        ("dtor55b530_release", 1),
    ]:
        oracle.reset()
        self_va = 0x60100000
        emu.write(self_va, b"\x00" * 0xA0)
        has_objs = init_ref > 0
        v20 = 0x60100100 if has_objs else 0
        v1c = 0x60100200 if has_objs else 0
        v14 = 0x60100300 if has_objs else 0
        v0c = 0x60100400 if has_objs else 0
        v04 = 0x60100500 if has_objs else 0
        write_u32(emu, self_va + 0x20, v20)
        write_u32(emu, self_va + 0x1C, v1c)
        write_u32(emu, self_va + 0x14, v14)
        write_u32(emu, self_va + 0x0C, v0c)
        write_u32(emu, self_va + 0x04, v04)
        if has_objs:
            for obj_va, ref_off, vt_off, dtor_va, vt_va in [
                (v20, 0x04, 0x00, 0x00610010, 0x60100800),
                (v1c, 0x04, 0x00, 0x00610020, 0x60100820),
                (v14, 0x98, 0x94, 0x00610030, 0x60100840),
                (v0c, 0x04, 0x00, 0x00610040, 0x60100860),
                (v04, 0x04, 0x00, 0x00610050, 0x60100880),
            ]:
                write_u32(emu, obj_va + vt_off, vt_va)
                write_u32(emu, obj_va + ref_off, init_ref)
                write_u32(emu, vt_va, dtor_va)
                oracle.hook_call(dtor_va, 1, 4)
        oracle.hook_call(0x004CA930, 0, 0)
        res = emu.call(0x0055B530, [], ecx=self_va)
        assert res.stack_cleanup == 0
        m1 = read_u32(emu, v14 + 0x98) if has_objs else 0
        m2 = read_u32(emu, v0c + 0x04) if has_objs else 0
        out.append(oracle.format_line(cid, 0, read_u32(emu, self_va), m1, m2))

    # 17. 0x0055CC60
    for cid, self_va, fill_byte in [
        ("ctor55cc60_a", 0x60110000, 0xAA),
        ("ctor55cc60_b", 0x60111000, 0x55),
        ("ctor55cc60_c", 0x60112000, 0xFF),
    ]:
        oracle.reset()
        emu.write(self_va, bytes([fill_byte]) * 0x120)
        oracle.hook_call(0x00417070, 0, 0)
        res = emu.call(0x0055CC60, [], ecx=self_va)
        assert res.stack_cleanup == 0
        m0 = read_u32(emu, self_va + 0x00) ^ read_u32(emu, self_va + 0x04)
        m1 = read_u32(emu, self_va + 0x10) ^ read_u32(emu, self_va + 0x54) ^ read_u32(emu, self_va + 0x70)
        m2 = (
            read_u32(emu, self_va + 0xCC)
            ^ read_u32(emu, self_va + 0xD0)
            ^ read_u32(emu, self_va + 0xE8)
            ^ read_u32(emu, self_va + 0x114)
        )
        out.append(oracle.format_line(cid, res.eax, m0, m1, m2))

    # 18. 0x005838F0
    for cid, init_ref in [
        ("rel5838f0_null", 0),
        ("rel5838f0_keep", 3),
        ("rel5838f0_free", 1),
    ]:
        oracle.reset()
        self_va = 0x60120000
        emu.write(self_va, b"\x00" * 0x6C80)
        has_objs = init_ref > 0
        v68 = 0x60127100 if has_objs else 0
        v6c = 0x60127200 if has_objs else 0
        write_u32(emu, self_va + 0x6C68, v68)
        write_u32(emu, self_va + 0x6C6C, v6c)
        write_u8(emu, self_va + 0x2C, 1)
        if has_objs:
            write_u32(emu, v68 + 0x00, 0x60127820)
            write_u32(emu, v68 + 0x04, init_ref)
            write_u32(emu, 0x60127820, 0x00612010)
            write_u32(emu, v6c + 0x00, 0x60127840)
            write_u32(emu, v6c + 0x04, init_ref)
            write_u32(emu, 0x60127840 + 0x00, 0x00612020)
            write_u32(emu, 0x60127840 + 0x14, 0x00612030)
            oracle.hook_call(0x00612010, 1, 4)
            oracle.hook_call(0x00612020, 1, 4)
            oracle.hook_call(0x00612030, 0, 0)
        oracle.hook_call(0x00997410, 0, 0)
        oracle.hook_call(0x00997460, 0, 0)
        res = emu.call(0x005838F0, [], ecx=self_va)
        assert res.stack_cleanup == 0
        m0 = read_u32(emu, self_va + 0x6C68) | read_u32(emu, self_va + 0x6C6C)
        m1 = read_u8(emu, self_va + 0x2C)
        m2 = read_u32(emu, v68 + 0x04) if has_objs else 0
        out.append(oracle.format_line(cid, 0, m0, m1, m2))

    # 19. 0x005A4ED0
    # Case 1: global [0x00DF6F94] == 0
    oracle.reset()
    self_va = 0x60130000
    write_u32(emu, 0x00DF6F94, 0)
    write_u32(emu, self_va + 0x524, 0x60130100)
    write_u32(emu, self_va + 0x528, 0x60130108)
    res = emu.call(0x005A4ED0, [], ecx=self_va)
    assert res.stack_cleanup == 0
    out.append(
        oracle.format_line(
            "clear5a4ed0_disabled",
            0,
            read_u32(emu, self_va + 0x524),
            read_u32(emu, self_va + 0x528),
            0,
        )
    )

    # Case 2: empty vector
    oracle.reset()
    self_va = 0x60130000
    write_u32(emu, 0x00DF6F94, 0x60139000)
    write_u32(emu, self_va + 0x524, 0x60130100)
    write_u32(emu, self_va + 0x528, 0x60130100)
    oracle.hook_call(0x00591C30, 3, 0, ret_val=0x60130100, zero_ecx=True)
    oracle.hook_call(0x005901C0, 2, 8)
    res = emu.call(0x005A4ED0, [], ecx=self_va)
    assert res.stack_cleanup == 0
    out.append(
        oracle.format_line(
            "clear5a4ed0_empty",
            0,
            read_u32(emu, self_va + 0x524),
            read_u32(emu, self_va + 0x528),
            0,
        )
    )

    # Case 3: 3 entries
    oracle.reset()
    self_va = 0x60130000
    vec_va = 0x60130100
    h0_ptr = 0x60130180
    h2_ptr = 0x60130190
    write_u32(emu, 0x00DF6F94, 0x60139000)
    write_u32(emu, self_va + 0x524, vec_va)
    write_u32(emu, self_va + 0x528, vec_va + 12)
    write_u32(emu, h0_ptr, 0x60130200)
    write_u32(emu, h2_ptr, 0x60130300)
    write_u32(emu, vec_va + 0x00, h0_ptr)
    write_u32(emu, vec_va + 0x04, 0x00DF88F8)
    write_u32(emu, vec_va + 0x08, h2_ptr)
    oracle.hook_call(0x0067ACA0, 0, 0)
    oracle.hook_call(0x00591C30, 3, 0, ret_val=vec_va + 12, zero_ecx=True)
    oracle.hook_call(0x005901C0, 2, 8)
    res = emu.call(0x005A4ED0, [], ecx=self_va)
    assert res.stack_cleanup == 0
    out.append(
        oracle.format_line(
            "clear5a4ed0_three",
            0,
            read_u32(emu, self_va + 0x524),
            read_u32(emu, self_va + 0x528),
            0,
        )
    )

    # 20. 0x005C5D70
    for cid, self_va, field10 in [
        ("dtor5c5d70_a", 0x60140000, 0x60140100),
        ("dtor5c5d70_b", 0x60141000, 0),
        ("dtor5c5d70_c", 0x60142000, 0x60142500),
    ]:
        oracle.reset()
        write_u32(emu, self_va + 0x00, 0)
        write_u32(emu, self_va + 0x10, field10)
        oracle.hook_call(0x004CAB60, 0, 0)
        oracle.hook_call(0x005BE310, 1, 4)
        res = emu.call(0x005C5D70, [], ecx=self_va)
        assert res.stack_cleanup == 0
        out.append(oracle.format_line(cid, 0, read_u32(emu, self_va + 0x00), read_u32(emu, self_va + 0x10), 0))

    # 21. 0x005EF110
    for cid, self_va, fill_byte in [
        ("ctor5ef110_a", 0x60150000, 0xCC),
        ("ctor5ef110_b", 0x60151000, 0x77),
        ("ctor5ef110_c", 0x60152000, 0xFF),
    ]:
        oracle.reset()
        emu.write(self_va, bytes([fill_byte]) * 0x60)
        oracle.hook_call(0x005E6B30, 0, 0)
        res = emu.call(0x005EF110, [], ecx=self_va)
        assert res.stack_cleanup == 0
        m0 = read_u32(emu, self_va + 0x00)
        m1 = read_u32(emu, self_va + 0x30) | read_u32(emu, self_va + 0x34) | read_u32(emu, self_va + 0x38)
        m2 = read_u16(emu, self_va + 0x3C) | read_u32(emu, self_va + 0x58)
        out.append(oracle.format_line(cid, res.eax, m0, m1, m2))

    # 22. 0x006232F0
    for cid, init_bc8, init_b34 in [
        ("shut6232f0_already_null", 0, 0x60160100),
        ("shut6232f0_both_active", 0x60160200, 0x60160100),
        ("shut6232f0_only_bc8", 0x60160300, 0),
    ]:
        oracle.reset()
        write_u32(emu, 0x00DF6BC8, init_bc8)
        write_u32(emu, 0x00DF6B34, init_b34)
        oracle.hook_call(0x005F13A0, 0, 0)
        oracle.hook_call(0x005EACE0, 0, 0)
        oracle.hook_call(0x004166B0, 1, 0, zero_ecx=True)
        oracle.hook_call(0x00622D30, 0, 0, zero_ecx=True)
        res = emu.call(0x006232F0, [])
        assert res.stack_cleanup == 0
        out.append(oracle.format_line(cid, 0, read_u32(emu, 0x00DF6BC8), 0, 0))

    # 23. 0x0064BD80
    for cid, arg0, sec_off, ref_va, init_ref in [
        ("upd64bd80_nullref", 0x60170200, 0xA0, 0, 0),
        ("upd64bd80_keepref", 0x60170200, 0xA0, 0x60170300, 3),
        ("upd64bd80_freeref", 0x60170200, 0xA0, 0x60170300, 1),
    ]:
        oracle.reset()
        self_va = 0x60170000
        sub3c_va = 0x60170100
        write_u32(emu, 0x018F9E50, sec_off)
        write_u32(emu, self_va + 0x3C, sub3c_va)
        write_u32(emu, sub3c_va + 0xB0, ref_va)
        if ref_va:
            write_u32(emu, ref_va + 0x00, 0x60170800)
            write_u32(emu, ref_va + 0x04, init_ref)
            write_u32(emu, 0x60170800, 0x00617010)
            oracle.hook_call(0x00617010, 1, 4)
        oracle.hook_call(0x00648310, 1, 4)
        oracle.hook_call(0x00646A70, 1, 4)
        res = emu.call(0x0064BD80, [arg0], ecx=self_va)
        assert res.stack_cleanup == 4
        m1 = read_u32(emu, ref_va + 0x04) if ref_va else 0
        out.append(oracle.format_line(cid, 0, read_u32(emu, sub3c_va + 0xB0), m1, 0))

    # 24. 0x00683140
    for cid, arg0 in [
        ("set683140_a", 0x11223344),
        ("set683140_b", 0),
        ("set683140_c", 0xAABBCCDD),
    ]:
        oracle.reset()
        self_va = 0x60180000
        emu.write(self_va, bytes([0xEE]) * 0x190)
        res = emu.call(0x00683140, [arg0], ecx=self_va)
        assert res.stack_cleanup == 4
        out.append(oracle.format_line(cid, 0, read_u32(emu, self_va + 0x180), 0, 0))

    # 25. 0x00685EB0
    for cid, val144 in [
        ("read685eb0_zero", 0),
        ("read685eb0_one", 1),
        ("read685eb0_byte", 0x7A),
    ]:
        oracle.reset()
        self_va = 0x60190000
        sub4_va = 0x60190100
        write_u32(emu, self_va + 0x04, sub4_va)
        write_u8(emu, sub4_va + 0x144, val144)
        res = emu.call(0x00685EB0, [], ecx=self_va)
        assert res.stack_cleanup == 0
        out.append(oracle.format_line(cid, res.eax & 0xFF, val144, 0, 0))

    # 26. 0x006879B0
    for cid, self_va, sub238_va in [
        ("detach6879b0_null", 0x601A0000, 0),
        ("detach6879b0_a", 0x601A0000, 0x601A0100),
        ("detach6879b0_b", 0x601A1000, 0x601A1200),
    ]:
        oracle.reset()
        write_u32(emu, self_va + 0x238, sub238_va)
        oracle.hook_call(0x007BDC20, 1, 4)
        res = emu.call(0x006879B0, [], ecx=self_va)
        assert res.stack_cleanup == 0
        out.append(oracle.format_line(cid, 0, read_u32(emu, self_va + 0x238), 0, 0))

    # 27. 0x00688190
    for cid, has_owner, owner418, flag_byte in [
        ("ctor688190_with_owner", True, 0x601B0500, 1),
        ("ctor688190_null_owner", False, 0, 0),
        ("ctor688190_custom_flag", True, 0x601B0900, 5),
    ]:
        oracle.reset()
        self_va = 0x601B0000
        owner_va = 0x601B0100 if has_owner else 0
        emu.write(self_va, b"\xFF" * 0x20)
        if has_owner:
            write_u32(emu, owner_va + 0x418, owner418)
        res = emu.call(0x00688190, [owner_va, flag_byte], ecx=self_va)
        assert res.stack_cleanup == 8
        m0 = read_u32(emu, self_va + 0x00)
        m1 = read_u32(emu, self_va + 0x04) ^ read_u32(emu, self_va + 0x08)
        m2 = read_u8(emu, self_va + 0x0C)
        out.append(oracle.format_line(cid, 0, m0, m1, m2))

    # 28. 0x0071C460
    # Case 1: [self+0x418] == 0
    oracle.reset()
    self_va = 0x601C0000
    write_u32(emu, self_va + 0x418, 0)
    res = emu.call(0x0071C460, [], ecx=self_va)
    assert res.stack_cleanup == 0
    out.append(oracle.format_line("prop71c460_no418", 0, 0, 0, 0))

    # Cases 2 & 3: skip vs copy
    for cid, skip_ret in [
        ("prop71c460_skip", 1),
        ("prop71c460_copy", 0),
    ]:
        oracle.reset()
        self_va = 0x601C0000
        arr_va = 0x601C0500
        write_u32(emu, self_va + 0x418, 0x601C0100)
        write_u32(emu, self_va + 0x310, arr_va)
        write_u32(emu, arr_va + 0x00, 0x601C0200)
        write_u32(emu, arr_va + 0x04, 0)
        write_u32(emu, 0x601C0200 + 0x0C, 0x601C0800)
        write_u32(emu, 0x601C0800 + 0x30, 0x0061C010)
        write_u32(emu, 0x601C0300, 0x601C0880)
        write_u32(emu, 0x601C0880 + 0x00, 0x0061C020)
        write_u32(emu, 0x601C0880 + 0x04, 0x0061C030)
        write_u32(emu, 0x601C0880 + 0x18, 0x0061C040)
        oracle.hook_call(0x0061C010, 0, 0, ret_val=0x601C0300)
        oracle.hook_call(0x0061C020, 0, 0, ret_val=skip_ret)
        oracle.hook_call(0x006F2DA0, 1, 4)
        oracle.hook_call(0x004E3D00, 1, 4)
        oracle.hook_call(0x0061C030, 1, 4)
        oracle.hook_call(0x00636FA0, 1, 4)
        oracle.hook_call(0x0061C040, 0, 0)
        res = emu.call(0x0071C460, [], ecx=self_va)
        assert res.stack_cleanup == 0
        if skip_ret == 0:
            local_map_va = 0x7F0FFEE0
            m0 = read_u32(emu, local_map_va + 0x04)
            m1 = read_u32(emu, local_map_va + 0x08)
        else:
            m0 = 0
            m1 = 0
        out.append(oracle.format_line(cid, 0, m0, m1, 0))

    return out


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument(
        "--checker",
        type=Path,
        default=REPO / "build" / "win32" / "bin" / "Release" / "ra3_protcave3check.exe",
    )
    args = parser.parse_args()
    if not args.checker.is_file():
        print(f"checker missing: {args.checker}")
        return 2

    cpp_proc = subprocess.run(
        [str(args.checker)],
        check=True,
        capture_output=True,
        text=True,
    )
    cpp_lines = [line.strip() for line in cpp_proc.stdout.splitlines() if line.strip()]

    oracle = CaveOracle()
    native_lines = run_all_native(oracle)

    if len(cpp_lines) != len(native_lines):
        print(f"line count mismatch: cpp={len(cpp_lines)} native={len(native_lines)}")
        return 1

    mismatches = 0
    for idx, (cline, nline) in enumerate(zip(cpp_lines, native_lines), 1):
        if cline != nline:
            mismatches += 1
            print(f"MISMATCH #{idx}:")
            print(f"  cpp   : {cline}")
            print(f"  native: {nline}")

    print(f"cases {len(native_lines)} mismatches {mismatches}")
    return 1 if mismatches else 0


if __name__ == "__main__":
    sys.exit(main())
