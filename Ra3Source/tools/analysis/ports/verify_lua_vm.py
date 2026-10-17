"""Differential oracle for the Stage-1 Embedded Lua 4.0 VM/Compiler/GC batch.

Executes the 27 Stage-1 entry-lost Lua 4.0 functions in `0x00403660..0x0040D5B0`
directly in Unicorn against `snapshot_3_240s` (`Emulator(capture=...)`) by
jumping through the original `0x0040xxxx` site (`jmp dword ptr [slot]`) into
the recovered `.text` / `.securom` bodies, and compares every return value,
mutated struct field, and external call trace against `ra3_luavmcheck.exe`.
"""

from __future__ import annotations

import struct
import subprocess
import sys
from pathlib import Path
from typing import Callable

import unicorn
from unicorn import x86_const as r

TOOLS_DIR = Path(__file__).resolve().parents[1]
if str(TOOLS_DIR) not in sys.path:
    sys.path.insert(0, str(TOOLS_DIR))

from emulate import DEFAULT_EXE, Emulator, EmulationError, RETURN_SENTINEL, STACK_BASE, STACK_SIZE, entry_targets

REPO_ROOT = TOOLS_DIR.parents[2]
MAIN_REPO = Path("D:/Game/Ra3/RevRa3")
DEFAULT_CAPTURE = MAIN_REPO / "work/runtime_dump/run35/snapshot_3_240s"
DEFAULT_CHECKER = REPO_ROOT / "Ra3Source/build/win32/bin/Release/ra3_luavmcheck.exe"

ARENA_BASE = 0x22000000
ARENA_SIZE = 0x00040000

# .securom immediate slots read by the Stage-1 bodies (resolved at ms=253582..253857
# in run35/slot_changes.tsv, 13.5s after snapshot_3_240s was dumped).
SECUROM_IMMEDIATES: dict[int, int] = {
    0x018FC6E4: 0x00BD5154,  # 0x00403660: "memory allocation error: block too big"
    0x018FC93C: 0x00000004,  # 0x004036D0: MINPOWER2 = 4
    0x018FC73C: 0x00000001,  # 0x004038B0: stacksize - 1
    0x018FC47C: 0x00BD51B8,  # 0x00403EC0: "?"
    0x018FCB10: 0x00BD52AC,  # 0x00404370: "`%.20s' expected"
    0x018FCDD0: 0x0000011E,  # 0x004043F0: NO_TOKEN = 286
    0x018FC6A4: 0x00BD52C0,  # 0x00404450: "`%.20s' expected (to close `%.20s' at line %d)"
    0x018FCB60: 0x03FFFFFF,  # 0x00404550: MAXARG_U
    0x018FC4EC: 0x00000001,  # 0x004048E0: 1 (extra-- when lastisopen)
    0x018FCB24: 0xFFFFFFE0,  # 0x00406CF0: -32 (-2 * sizeof(TObject))
    0x018FC458: 0x00000001,  # 0x00406D70: 1 (luaD_checkstack(L, 1))
    0x018FC6D0: 0x00000010,  # 0x00408E30: MINSTRTABSIZE = 16
    0x018FC920: 0x00000010,  # 0x00408ED0: MINSTRTABSIZE = 16
    0x018FCA78: 0x00000004,  # 0x0040A070: LUA_TTABLE = 4
    0x018FC62C: 0x00BD5AD4,  # 0x0040A120: "%d is not a valid tag"
    0x018FC8D0: 0x00000001,  # 0x0040A340: 1 (luaD_checkstack(L, 1))
    0x018FC864: 0x00003FC0,  # 0x0040CA60: MASK_B = 0x3FC0
    0x018FCAC0: 0x00003FC0,  # 0x0040CAA0: MASK_B = 0x3FC0
    0x018FCDB0: 0x000000FF,  # 0x0040D2F0: NO_REG = 0xFF
    0x018FC924: 0x0000003F,  # 0x0040D5B0: MASK_OP = 0x3F
}


class TraceRecorder:
    def __init__(self, emu: Emulator) -> None:
        self.emu = emu
        self.reset()

    def reset(self) -> None:
        self.trace_hash = 0x811C9DC5
        self.trace_calls = 0
        self.last_arg0 = 0
        self.last_arg1 = 0
        self.last_arg2 = 0
        self.last_arg3 = 0
        self.bump_cursor = ARENA_BASE + 0x00020000
        self.stub_ret = 0
        self.lex_next_token = 0
        self.lex_next_seminfo_lo = 0
        self.lex_next_seminfo_hi = 0

    def mix_word(self, val: int) -> None:
        self.trace_hash = ((self.trace_hash ^ (val & 0xFFFFFFFF)) * 0x01000193) & 0xFFFFFFFF

    def mix_cstring(self, va: int) -> None:
        if va == 0:
            self.mix_word(0xFFFFFFFF)
            return
        raw = self.emu.read(va, 128).split(b"\x00", 1)[0]
        for b in raw:
            self.mix_word(b)

    def mix_py_str(self, s: bytes) -> None:
        for b in s:
            self.mix_word(b)


def u32(emu: Emulator, va: int) -> int:
    return struct.unpack("<I", emu.read(va, 4))[0]


def i32(emu: Emulator, va: int) -> int:
    return struct.unpack("<i", emu.read(va, 4))[0]


def i16(emu: Emulator, va: int) -> int:
    return struct.unpack("<h", emu.read(va, 2))[0]


def u64(emu: Emulator, va: int) -> int:
    return struct.unpack("<Q", emu.read(va, 8))[0]


def w32(emu: Emulator, va: int, val: int) -> None:
    emu.write(va, struct.pack("<I", val & 0xFFFFFFFF))


def wi32(emu: Emulator, va: int, val: int) -> None:
    emu.write(va, struct.pack("<i", val))


def wi16(emu: Emulator, va: int, val: int) -> None:
    emu.write(va, struct.pack("<h", val))


def wu64(emu: Emulator, va: int, val: int) -> None:
    emu.write(va, struct.pack("<Q", val & 0xFFFFFFFFFFFFFFFF))


class LuaVmOracle:
    def __init__(self, exe: Path = DEFAULT_EXE, capture: Path = DEFAULT_CAPTURE) -> None:
        self.emu = Emulator(exe=exe, capture=capture)
        self.targets = entry_targets(REPO_ROOT / "Ra3Source" / "docs" / "analysis" / "ra3_1.0" / "securom_sites.tsv")
        # Allow SecuROM execution counters in 0x01980000..0x01990000 and immediate seeding in .securom
        self.emu.uc.mem_protect(
            0x018F4000,
            0x000A0000,
            unicorn.UC_PROT_READ | unicorn.UC_PROT_WRITE | unicorn.UC_PROT_EXEC,
        )
        # Patch resolved runtime targets into .securom slots so cross-calls between entry-lost functions work
        for row_site, row_body in self.targets.items():
            # If the entry at row_site is jmp dword ptr [slot], also write row_body into slot
            op = self.emu.read(row_site, 6)
            if op[:2] == b"\xff\x25":
                slot_addr = struct.unpack("<I", op[2:6])[0]
                w32(self.emu, slot_addr, row_body)
        self.emu.uc.mem_map(
            ARENA_BASE,
            ARENA_SIZE,
            unicorn.UC_PROT_READ | unicorn.UC_PROT_WRITE,
        )
        self.emu.uc.mem_map(
            0x01132000,
            0x1000,
            unicorn.UC_PROT_READ | unicorn.UC_PROT_EXEC,
        )
        self.emu.uc.mem_write(0x01132000, b"\xC3" * 0x1000)
        # Initialize SecuROM execution counter array in 0x01980000..0x01990000 so dec dword ptr [...] never hits 0
        self.emu.uc.mem_write(0x01980000, b"\x77" * 0x10000)

        for slot_va, val in SECUROM_IMMEDIATES.items():
            w32(self.emu, slot_va, val)

        self.tr = TraceRecorder(self.emu)
        self._bind_runtime_stubs()
        # Snapshot .data after binding global function pointer stubs (0x00CA28B0/0x00CA28B4)
        self._post_stub_data = {
            start: bytes(self.emu.uc.mem_read(start, len(img)))
            for start, img in self.emu._initial.items()
        }

    def reset_arena(self) -> None:
        for start, img in self._post_stub_data.items():
            self.emu.uc.mem_write(start, img)
        self.emu.write(ARENA_BASE, b"\x00" * ARENA_SIZE)
        self.emu.uc.mem_write(0x01980000, b"\x77" * 0x10000)
        for slot_va, val in SECUROM_IMMEDIATES.items():
            w32(self.emu, slot_va, val)
        self.tr.reset()

    def invoke(
        self,
        address: int,
        *stack_args: int,
        registers: dict[str, int] | None = None,
        max_instructions: int = 5_000_000,
    ) -> int:
        target = self.targets.get(address, address)
        uc = self.emu.uc
        top = STACK_BASE + STACK_SIZE - 0x100
        frame = struct.pack(
            f"<I{len(stack_args)}I",
            RETURN_SENTINEL,
            *[a & 0xFFFFFFFF for a in stack_args],
        )
        esp = top - len(frame)
        uc.mem_write(esp, frame)
        reg_map = {
            "eax": r.UC_X86_REG_EAX,
            "ebx": r.UC_X86_REG_EBX,
            "ecx": r.UC_X86_REG_ECX,
            "edx": r.UC_X86_REG_EDX,
            "esi": r.UC_X86_REG_ESI,
            "edi": r.UC_X86_REG_EDI,
            "ebp": r.UC_X86_REG_EBP,
        }
        uc.reg_write(r.UC_X86_REG_ESP, esp)
        for name, reg_id in reg_map.items():
            val = (registers or {}).get(name, 0)
            uc.reg_write(reg_id, val & 0xFFFFFFFF)
        self.emu._fault = None
        try:
            uc.emu_start(target, RETURN_SENTINEL, count=max_instructions)
        except unicorn.UcError as err:
            raise EmulationError(self.emu._fault or f"0x{address:08X} (->0x{target:08X}): {err}") from None
        if self.emu._fault:
            raise EmulationError(self.emu._fault)
        return uc.reg_read(r.UC_X86_REG_EAX) & 0xFFFFFFFF

    def _bind_runtime_stubs(self) -> None:
        emu = self.emu
        tr = self.tr

        # CRT realloc [0x00CA28B0]
        def stub_realloc(e: Emulator) -> int:
            ptr = e.stack_arg(0)
            size = e.stack_arg(1)
            tr.trace_calls += 1
            tr.last_arg0 = ptr
            tr.last_arg1 = size
            tr.mix_word(0xCA110001)
            tr.mix_word(ptr)
            tr.mix_word(size)
            if tr.stub_ret == -999:
                return 0
            out_va = tr.bump_cursor
            aligned = (size + 15) & ~15
            tr.bump_cursor += aligned if aligned > 0 else 16
            return out_va

        # CRT free [0x00CA28B4]
        def stub_free(e: Emulator) -> int:
            ptr = e.stack_arg(0)
            tr.trace_calls += 1
            tr.last_arg0 = ptr
            tr.mix_word(0xCA110002)
            tr.mix_word(ptr)
            return 0

        # CRT sprintf [0x00BBA548] used by 0x00404370 and 0x00404450
        def stub_sprintf(e: Emulator) -> int:
            dst = e.stack_arg(0)
            fmt_va = e.stack_arg(1)
            fmt = e.read(fmt_va, 96).split(b"\x00", 1)[0].decode("latin1")
            if fmt == "`%.20s' expected":
                s0 = e.read(e.stack_arg(2), 24).split(b"\x00", 1)[0].decode("latin1")
                rendered = f"`{s0[:20]}' expected".encode("latin1") + b"\x00"
            else:
                s0 = e.read(e.stack_arg(2), 24).split(b"\x00", 1)[0].decode("latin1")
                s1 = e.read(e.stack_arg(3), 24).split(b"\x00", 1)[0].decode("latin1")
                line_no = struct.unpack("<i", struct.pack("<I", e.stack_arg(4)))[0]
                rendered = (
                    f"`{s0[:20]}' expected (to close `{s1[:20]}' at line {line_no})".encode("latin1")
                    + b"\x00"
                )
            e.write(dst, rendered)
            return len(rendered) - 1

        emu.bind_import(0x00CA28B0, stub_realloc, 0)
        emu.bind_import(0x00CA28B4, stub_free, 0)
        emu.bind_import(0x00BBA548, stub_sprintf, 0)

        # 0x004037D0 lua_error(L, msg)
        def stub_lua_error(e: Emulator) -> int:
            state_va = e.stack_arg(0)
            msg_va = e.stack_arg(1)
            tr.trace_calls += 1
            tr.last_arg0 = state_va
            tr.mix_word(0xCA110003)
            tr.mix_word(state_va)
            tr.mix_cstring(msg_va)
            return 0

        # 0x00403FF0 luaD_breakrun(L, errcode)
        def stub_breakrun(e: Emulator) -> int:
            state_va = e.stack_arg(0)
            err = e.stack_arg(1)
            tr.trace_calls += 1
            tr.last_arg0 = state_va
            tr.last_arg1 = err
            tr.mix_word(0xCA110004)
            tr.mix_word(state_va)
            tr.mix_word(err)
            return 0

        # 0x00403F80 luaD_error(L, msg)
        def stub_do_error(e: Emulator) -> int:
            state_va = e.stack_arg(0)
            msg_va = e.stack_arg(1)
            tr.trace_calls += 1
            tr.last_arg0 = state_va
            tr.mix_word(0xCA110005)
            tr.mix_word(state_va)
            tr.mix_cstring(msg_va)
            return 0

        # 0x00403800 luaD_runprotected(L, f, ud)
        def stub_runprotected(e: Emulator) -> int:
            state_va = e.stack_arg(0)
            func_va = e.stack_arg(1)
            ud_va = e.stack_arg(2)
            tr.trace_calls += 1
            tr.last_arg0 = state_va
            tr.last_arg1 = func_va
            tr.last_arg2 = u32(e, ud_va)
            tr.last_arg3 = u32(e, ud_va + 4)
            tr.mix_word(0xCA110006)
            tr.mix_word(tr.last_arg0)
            tr.mix_word(tr.last_arg1)
            tr.mix_word(tr.last_arg2)
            tr.mix_word(tr.last_arg3)
            return tr.stub_ret & 0xFFFFFFFF

        # 0x0040AF40 luaZ_mopen(z, buff, size, name)
        def stub_zmopen(e: Emulator) -> int:
            z_va = e.stack_arg(0)
            buff_va = e.stack_arg(1)
            size = e.stack_arg(2)
            name_va = e.stack_arg(3)
            tr.trace_calls += 1
            tr.last_arg0 = buff_va
            tr.last_arg1 = size
            tr.mix_word(0xCA110007)
            tr.mix_word(buff_va)
            tr.mix_word(size)
            tr.mix_cstring(name_va)
            w32(e, z_va, size)
            return z_va

        # 0x00403CF0 f_parser (EAX=z, ECX=bin, ESI=L)
        def stub_fparser(e: Emulator) -> int:
            z_va = e.register(r.UC_X86_REG_EAX)
            bin_flag = e.register(r.UC_X86_REG_ECX)
            state_va = e.register(r.UC_X86_REG_ESI)
            tr.trace_calls += 1
            tr.last_arg0 = state_va
            tr.last_arg1 = u32(e, z_va)
            tr.last_arg2 = bin_flag
            tr.mix_word(0xCA110008)
            tr.mix_word(tr.last_arg0)
            tr.mix_word(tr.last_arg1)
            tr.mix_word(tr.last_arg2)
            return (tr.stub_ret - 100) if tr.stub_ret >= 100 else 0

        # 0x0040B7B0 luaX_token2str(token, buf)
        def stub_token2str(e: Emulator) -> int:
            tok = i32(e, e.register(r.UC_X86_REG_ESP) + 4)
            buf_va = e.stack_arg(1)
            tr.trace_calls += 1
            tr.mix_word(0xCA110009)
            tr.mix_word(tok & 0xFFFFFFFF)
            e.write(buf_va, f"tok_{tok}\x00".encode("latin1"))
            return 0

        # 0x0040C900 luaX_error(ls, msg)
        def stub_lex_error(e: Emulator) -> int:
            ls_va = e.stack_arg(0)
            msg_va = e.stack_arg(1)
            tr.trace_calls += 1
            tr.last_arg0 = ls_va
            tr.mix_word(0xCA11000A)
            tr.mix_word(ls_va)
            tr.mix_cstring(msg_va)
            return 0

        # 0x0040B960 luaX_syntaxerror(ls, msg, token)
        def stub_syntax_error(e: Emulator) -> int:
            ls_va = e.stack_arg(0)
            msg_va = e.stack_arg(1)
            tok = e.stack_arg(2)
            tr.trace_calls += 1
            tr.last_arg0 = ls_va
            tr.last_arg1 = tok
            tr.mix_word(0xCA11000B)
            tr.mix_word(ls_va)
            tr.mix_word(tok)
            tr.mix_cstring(msg_va)
            return 0

        # 0x0040C380 luaX_lex(ls, seminfo)
        def stub_lex(e: Emulator) -> int:
            ls_va = e.stack_arg(0)
            seminfo_va = e.stack_arg(1)
            tr.trace_calls += 1
            tr.last_arg0 = ls_va
            tr.mix_word(0xCA11000C)
            tr.mix_word(ls_va)
            w32(e, seminfo_va, tr.lex_next_seminfo_lo)
            w32(e, seminfo_va + 4, tr.lex_next_seminfo_hi)
            return tr.lex_next_token & 0xFFFFFFFF

        # 0x00404330 error_expected(ls [in stack0], token [in ECX])
        def stub_error_expected(e: Emulator) -> int:
            ls_va = e.stack_arg(0)
            tok = e.register(r.UC_X86_REG_ECX)
            tr.trace_calls += 1
            tr.last_arg0 = ls_va
            tr.last_arg1 = tok
            tr.mix_word(0xCA11000D)
            tr.mix_word(ls_va)
            tr.mix_word(tok)
            return 0

        # 0x0040D330 luaK_kstr(ls, c)
        def stub_kstr(e: Emulator) -> int:
            ls_va = e.stack_arg(0)
            idx = e.stack_arg(1)
            tr.trace_calls += 1
            tr.last_arg0 = ls_va
            tr.last_arg1 = idx
            tr.mix_word(0xCA11000E)
            tr.mix_word(ls_va)
            tr.mix_word(idx)
            return (idx + 1000) & 0xFFFFFFFF

        # 0x0040D250 luaK_nil(ls, n)
        def stub_knil(e: Emulator) -> int:
            ls_va = e.stack_arg(0)
            n = e.stack_arg(1)
            tr.trace_calls += 1
            tr.last_arg0 = ls_va
            tr.last_arg1 = n
            tr.mix_word(0xCA11000F)
            tr.mix_word(ls_va)
            tr.mix_word(n)
            return 0

        # 0x0040D200 luaK_code0(fs, op)
        def stub_code0(e: Emulator) -> int:
            fs_va = e.stack_arg(0)
            op = e.stack_arg(1)
            tr.trace_calls += 1
            tr.last_arg0 = fs_va
            tr.last_arg1 = op
            tr.mix_word(0xCA110010)
            tr.mix_word(fs_va)
            tr.mix_word(op)
            return u32(e, fs_va + 0x10)

        # 0x00404730 rem_localvars (EAX=ls, EDX=nvars)
        def stub_rem_localvars(e: Emulator) -> int:
            ls_va = e.register(r.UC_X86_REG_EAX)
            nvars = e.register(r.UC_X86_REG_EDX)
            tr.trace_calls += 1
            tr.last_arg0 = ls_va
            tr.last_arg1 = nvars
            tr.mix_word(0xCA110011)
            tr.mix_word(ls_va)
            tr.mix_word(nvars)
            return 0

        # 0x0040E860 luaF_protook(L, f, pc)
        def stub_protook(e: Emulator) -> int:
            state_va = e.stack_arg(0)
            f_va = e.stack_arg(1)
            pc = e.stack_arg(2)
            tr.trace_calls += 1
            tr.last_arg0 = state_va
            tr.last_arg1 = f_va
            tr.last_arg2 = pc
            tr.mix_word(0xCA110012)
            tr.mix_word(state_va)
            tr.mix_word(f_va)
            tr.mix_word(pc)
            return 0

        # 0x00405260 funcname (EAX=ls, EBX=v)
        def stub_funcname(e: Emulator) -> int:
            ls_va = e.register(r.UC_X86_REG_EAX)
            v_va = e.register(r.UC_X86_REG_EBX)
            tr.trace_calls += 1
            tr.last_arg0 = ls_va
            w32(e, v_va, 1)
            w32(e, v_va + 4, 77)
            w32(e, v_va + 8, 88)
            tr.mix_word(0xCA110013)
            tr.mix_word(ls_va)
            return tr.stub_ret & 0xFFFFFFFF

        # 0x00405560 body (EBX=ls, stack0=needself, stack1=line)
        def stub_body(e: Emulator) -> int:
            ls_va = e.register(r.UC_X86_REG_EBX)
            needself = e.stack_arg(0)
            line = e.stack_arg(1)
            tr.trace_calls += 1
            tr.last_arg0 = ls_va
            tr.last_arg1 = needself
            tr.last_arg2 = line
            tr.mix_word(0xCA110014)
            tr.mix_word(ls_va)
            tr.mix_word(needself)
            tr.mix_word(line)
            return 0

        # 0x0040D990 luaK_storevar(ls, v)
        def stub_storevar(e: Emulator) -> int:
            ls_va = e.stack_arg(0)
            v_va = e.stack_arg(1)
            tr.trace_calls += 1
            tr.last_arg0 = ls_va
            tr.last_arg1 = u32(e, v_va)
            tr.last_arg2 = u32(e, v_va + 4)
            tr.last_arg3 = u32(e, v_va + 8)
            tr.mix_word(0xCA110015)
            tr.mix_word(tr.last_arg0)
            tr.mix_word(tr.last_arg1)
            tr.mix_word(tr.last_arg2)
            tr.mix_word(tr.last_arg3)
            return 0

        # 0x004093B0 luaG_getline(lineinfo, pc, refline, refi)
        def stub_getline(e: Emulator) -> int:
            lineinfo_va = e.stack_arg(0)
            pc = e.stack_arg(1)
            refline = e.stack_arg(2)
            refi_va = e.stack_arg(3)
            tr.trace_calls += 1
            tr.last_arg0 = lineinfo_va
            tr.last_arg1 = pc
            tr.last_arg2 = refline
            new_refi = (i32(e, refi_va) + 3) & 0xFFFFFFFF
            w32(e, refi_va, new_refi)
            tr.mix_word(0xCA110016)
            tr.mix_word(lineinfo_va)
            tr.mix_word(pc)
            tr.mix_word(refline)
            tr.mix_word(new_refi)
            return tr.stub_ret & 0xFFFFFFFF

        # 0x004039B0 call_linehook(L, topMinus2, newline, linehook)
        def stub_linehook(e: Emulator) -> int:
            state_va = e.stack_arg(0)
            top_m2 = e.stack_arg(1)
            newline = e.stack_arg(2)
            hook = e.stack_arg(3)
            tr.trace_calls += 1
            tr.last_arg0 = state_va
            tr.last_arg1 = top_m2
            tr.last_arg2 = newline
            tr.last_arg3 = hook
            tr.mix_word(0xCA110017)
            tr.mix_word(state_va)
            tr.mix_word(top_m2)
            tr.mix_word(newline)
            tr.mix_word(hook)
            return 0

        # 0x0040E7C0 luaF_newclosure(L, nelems)
        def stub_newclosure(e: Emulator) -> int:
            state_va = e.stack_arg(0)
            nelems = e.stack_arg(1)
            tr.trace_calls += 1
            tr.last_arg0 = state_va
            tr.last_arg1 = nelems
            tr.mix_word(0xCA110018)
            tr.mix_word(state_va)
            tr.mix_word(nelems)
            return ARENA_BASE + 0x5000

        # 0x0040E930 luaF_freeproto(L, f)
        def stub_freeproto(e: Emulator) -> int:
            state_va = e.stack_arg(0)
            f_va = e.stack_arg(1)
            tr.trace_calls += 1
            tr.last_arg0 = state_va
            tr.last_arg1 = f_va
            tr.mix_word(0xCA110019)
            tr.mix_word(state_va)
            tr.mix_word(f_va)
            return 0

        # 0x0040B470 luaH_free(L, t)
        def stub_freehash(e: Emulator) -> int:
            state_va = e.stack_arg(0)
            t_va = e.stack_arg(1)
            tr.trace_calls += 1
            tr.last_arg0 = state_va
            tr.last_arg1 = t_va
            tr.mix_word(0xCA11001A)
            tr.mix_word(state_va)
            tr.mix_word(t_va)
            return 0

        # 0x0040E3C0 luaS_resize(L, tb, newsize)
        def stub_str_resize(e: Emulator) -> int:
            state_va = e.stack_arg(0)
            tb_va = e.stack_arg(1)
            newsize = e.stack_arg(2)
            tr.trace_calls += 1
            tr.last_arg0 = state_va
            tr.last_arg1 = tb_va
            tr.last_arg2 = newsize
            w32(e, tb_va, newsize)
            tr.mix_word(0xCA11001B)
            tr.mix_word(state_va)
            tr.mix_word(tb_va)
            tr.mix_word(newsize)
            return 0

        # 0x00408BD0 marklock(L, st)
        def stub_marklock(e: Emulator) -> int:
            state_va = e.stack_arg(0)
            tr.trace_calls += 1
            tr.last_arg0 = state_va
            tr.mix_word(0xCA11001C)
            tr.mix_word(state_va)
            return 0

        # 0x0040B2C0 luaH_remove(t, n)
        def stub_table_remove(e: Emulator) -> int:
            t_va = e.stack_arg(0)
            n_va = e.stack_arg(1)
            tr.trace_calls += 1
            tr.last_arg0 = t_va
            tr.last_arg1 = n_va
            tr.mix_word(0xCA11001D)
            tr.mix_word(t_va)
            tr.mix_word(n_va)
            return 0

        # 0x00409DB0 luaT_checkevent(event [in EAX])
        def stub_checkevent(e: Emulator) -> int:
            ev_va = e.register(r.UC_X86_REG_EAX)
            tr.trace_calls += 1
            tr.mix_word(0xCA11001E)
            tr.mix_cstring(ev_va)
            return tr.stub_ret & 0xFFFFFFFF

        # 0x004041A0 luaO_verror(L, fmt, ...)
        def stub_verror(e: Emulator) -> int:
            state_va = e.stack_arg(0)
            fmt_va = e.stack_arg(1)
            tr.trace_calls += 1
            tr.last_arg0 = state_va
            tr.mix_word(0xCA11001F)
            tr.mix_word(state_va)
            tr.mix_cstring(fmt_va)
            fmt = e.read(fmt_va, 64).split(b"\x00", 1)[0]
            if b"%d" in fmt:
                tr.mix_word(e.stack_arg(2))
            else:
                tr.mix_cstring(e.stack_arg(2))
                tr.mix_cstring(e.stack_arg(3))
                tr.mix_cstring(e.stack_arg(4))
            return 0

        # 0x0040CBA0 luaK_patchlistaux(fs, list, vtarget, reg, dtarget)
        def stub_patchlistaux(e: Emulator) -> int:
            fs_va = e.stack_arg(0)
            lst = e.register(r.UC_X86_REG_ECX)
            vtarget = e.stack_arg(1)
            reg = e.stack_arg(2)
            dtarget = e.stack_arg(3)
            tr.trace_calls += 1
            tr.last_arg0 = fs_va
            tr.last_arg1 = lst
            tr.last_arg2 = vtarget
            tr.last_arg3 = ((reg & 0xFFFF) << 16) | (dtarget & 0xFFFF)
            tr.mix_word(0xCA110020)
            tr.mix_word(tr.last_arg0)
            tr.mix_word(tr.last_arg1)
            tr.mix_word(tr.last_arg2)
            tr.mix_word(tr.last_arg3)
            return 0

        # 0x0040CD80 rememberlineinfo (EDI=fs)
        def stub_rememberlineinfo(e: Emulator) -> int:
            fs_va = e.register(r.UC_X86_REG_EDI)
            tr.trace_calls += 1
            tr.last_arg0 = fs_va
            tr.mix_word(0xCA110021)
            tr.mix_word(fs_va)
            return 0

        emu.bind_function(0x004037D0, stub_lua_error, 0)
        emu.bind_function(0x00403FF0, stub_breakrun, 0)
        emu.bind_function(0x00403F80, stub_do_error, 0)
        emu.bind_function(0x00403800, stub_runprotected, 0)
        emu.bind_function(0x0040AF40, stub_zmopen, 0)
        emu.bind_function(0x00403CF0, stub_fparser, 0)
        emu.bind_function(0x0040B7B0, stub_token2str, 0)
        emu.bind_function(0x0040C900, stub_lex_error, 0)
        emu.bind_function(0x0040B960, stub_syntax_error, 0)
        emu.bind_function(0x0040C380, stub_lex, 0)
        emu.bind_function(0x00404330, stub_error_expected, 0)
        emu.bind_function(0x0040D330, stub_kstr, 0)
        emu.bind_function(0x0040D250, stub_knil, 0)
        emu.bind_function(0x0040D200, stub_code0, 0)
        emu.bind_function(0x00404730, stub_rem_localvars, 0)
        emu.bind_function(0x0040E860, stub_protook, 0)
        emu.bind_function(0x00405260, stub_funcname, 0)
        emu.bind_function(0x00405560, stub_body, 0)
        emu.bind_function(0x0040D990, stub_storevar, 0)
        emu.bind_function(0x004093B0, stub_getline, 0)
        emu.bind_function(0x004039B0, stub_linehook, 0)
        emu.bind_function(0x0040E7C0, stub_newclosure, 0)
        emu.bind_function(0x0040E930, stub_freeproto, 0)
        emu.bind_function(0x0040B470, stub_freehash, 0)
        emu.bind_function(0x0040E3C0, stub_str_resize, 0)
        emu.bind_function(0x00408BD0, stub_marklock, 0)
        emu.bind_function(0x0040B2C0, stub_table_remove, 0)
        emu.bind_function(0x00409DB0, stub_checkevent, 0)
        emu.bind_function(0x004041A0, stub_verror, 0)
        emu.bind_function(0x0040CBA0, stub_patchlistaux, 0)
        emu.bind_function(0x0040CD80, stub_rememberlineinfo, 0)


def run_checker(checker: Path, *args: str) -> str:
    proc = subprocess.run(
        [str(checker), *args],
        capture_output=True,
        text=True,
        check=True,
    )
    return proc.stdout.strip()


def build_scenarios(oracle: LuaVmOracle) -> list[tuple[str, list[str], Callable[[], str]]]:
    emu = oracle.emu
    tr = oracle.tr
    state_va = ARENA_BASE + 0x0100
    ls_va = ARENA_BASE + 0x0300
    fs_va = ARENA_BASE + 0x0400
    proto_va = ARENA_BASE + 0x0500

    scenarios: list[tuple[str, list[str], Callable[[], str]]] = []

    # 1. 0x00403660 LuaMemRealloc
    for old_va, size, stub_ret in [
        (ARENA_BASE + 0x1000, 0, 0),
        (ARENA_BASE + 0x1000, 64, 0),
        (0, 128, -999),
        (ARENA_BASE + 0x1000, 0xFFFFFFFE, 0),
    ]:
        def run_realloc(old_va=old_va, size=size, stub_ret=stub_ret) -> str:
            oracle.reset_arena()
            tr.stub_ret = stub_ret
            res = oracle.invoke(0x00403660, state_va, old_va, size)
            return (
                f"ret=0x{res:08x} calls={tr.trace_calls} hash=0x{tr.trace_hash:08x} "
                f"a0=0x{tr.last_arg0:08x} a1=0x{tr.last_arg1:08x}"
            )
        scenarios.append((
            f"0x00403660_realloc_{size}_{stub_ret}",
            ["realloc", hex(old_va), hex(size), str(stub_ret)],
            run_realloc,
        ))

    # 2. 0x004036D0 LuaMemGrowAux
    for nelems, inc, elem_size, limit in [
        (0, 1, 4, 1000),
        (3, 1, 8, 1000),
        (4, 1, 16, 1000),
        (99, 2, 4, 100),
    ]:
        def run_growaux(nelems=nelems, inc=inc, elem_size=elem_size, limit=limit) -> str:
            oracle.reset_arena()
            msg_va = ARENA_BASE + 0x0900
            emu.write(msg_va, b"growaux overflow\x00")
            block_va = ARENA_BASE + 0x1800
            res = oracle.invoke(0x004036D0, state_va, block_va, nelems, inc, elem_size, msg_va, limit)
            return (
                f"ret=0x{res:08x} calls={tr.trace_calls} hash=0x{tr.trace_hash:08x} "
                f"a1=0x{tr.last_arg1:08x}"
            )
        scenarios.append((
            f"0x004036D0_growaux_{nelems}_{inc}_{limit}",
            ["growaux", hex(ARENA_BASE + 0x1800), str(nelems), str(inc), str(elem_size), str(limit)],
            run_growaux,
        ))

    # 3. 0x00403740 LuaDoInitStack
    for nblocks, stack_size in [(100, 48), (256, 96)]:
        def run_initstack(nblocks=nblocks, stack_size=stack_size) -> str:
            oracle.reset_arena()
            w32(emu, state_va + 0x60, nblocks)
            oracle.invoke(0x00403740, state_va, stack_size)
            return (
                f"top=0x{u32(emu, state_va):08x} stack=0x{u32(emu, state_va + 4):08x} "
                f"last=0x{u32(emu, state_va + 8):08x} size={i32(emu, state_va + 0xC)} "
                f"cbase=0x{u32(emu, state_va + 0x10):08x} nblocks={u32(emu, state_va + 0x60)} "
                f"hash=0x{tr.trace_hash:08x}"
            )
        scenarios.append((
            f"0x00403740_initstack_{stack_size}",
            ["initstack", str(nblocks), str(stack_size)],
            run_initstack,
        ))

    # 4. 0x004038B0 LuaDoCheckStack
    for top_idx, last_idx, stack_size, needed in [
        (2, 10, 20, 4),
        (8, 10, 11, 5),
        (8, 15, 10, 5),
    ]:
        def run_checkstack(top_idx=top_idx, last_idx=last_idx, stack_size=stack_size, needed=needed) -> str:
            oracle.reset_arena()
            base_va = ARENA_BASE + 0x1000
            w32(emu, state_va + 0x04, base_va)
            w32(emu, state_va + 0x00, base_va + top_idx * 16)
            w32(emu, state_va + 0x08, base_va + last_idx * 16)
            wi32(emu, state_va + 0x0C, stack_size)
            oracle.invoke(0x004038B0, state_va, needed)
            return (
                f"last=0x{u32(emu, state_va + 8):08x} calls={tr.trace_calls} "
                f"hash=0x{tr.trace_hash:08x} a1=0x{tr.last_arg1:08x}"
            )
        scenarios.append((
            f"0x004038B0_checkstack_{top_idx}_{last_idx}_{needed}",
            ["checkstack", str(top_idx), str(last_idx), str(stack_size), str(needed)],
            run_checkstack,
        ))

    # 5. 0x00403CA0 LuaProtectedCall
    for nargs, nresults, stub_ret in [(2, 1, 0), (3, -1, 2)]:
        def run_call(nargs=nargs, nresults=nresults, stub_ret=stub_ret) -> str:
            oracle.reset_arena()
            tr.stub_ret = stub_ret
            base_va = ARENA_BASE + 0x1000
            w32(emu, state_va + 0x04, base_va)
            w32(emu, state_va + 0x00, base_va + 10 * 16)
            res = struct.unpack("<i", struct.pack("<I", oracle.invoke(0x00403CA0, state_va, nargs, nresults)))[0]
            return (
                f"ret={res} top=0x{u32(emu, state_va):08x} calls={tr.trace_calls} "
                f"hash=0x{tr.trace_hash:08x} a2=0x{tr.last_arg2:08x} a3=0x{tr.last_arg3:08x}"
            )
        scenarios.append((
            f"0x00403CA0_call_{nargs}_{stub_ret}",
            ["call", str(nargs), str(nresults), str(stub_ret)],
            run_call,
        ))

    # 6. 0x00403EC0 LuaDoBuffer
    for first_byte, size, has_name, stub_ret in [
        (0x61, 32, 0, 0),
        (0x1B, 64, 1, 2),
        (0x61, 16, 1, 103),
    ]:
        def run_dobuffer(first_byte=first_byte, size=size, has_name=has_name, stub_ret=stub_ret) -> str:
            oracle.reset_arena()
            tr.stub_ret = stub_ret
            base_va = ARENA_BASE + 0x1000
            w32(emu, state_va + 0x04, base_va)
            w32(emu, state_va + 0x00, base_va + 6 * 16)
            buf_va = ARENA_BASE + 0x2000
            emu.write(buf_va, bytes([first_byte, ord("x")]))
            name_va = ARENA_BASE + 0x2100
            emu.write(name_va, b"@chunk.lua\x00")
            res = struct.unpack(
                "<i",
                struct.pack("<I", oracle.invoke(0x00403EC0, state_va, buf_va, size, name_va if has_name else 0)),
            )[0]
            return (
                f"ret={res} top=0x{u32(emu, state_va):08x} calls={tr.trace_calls} "
                f"hash=0x{tr.trace_hash:08x}"
            )
        scenarios.append((
            f"0x00403EC0_dobuffer_{first_byte}_{has_name}_{stub_ret}",
            ["dobuffer", str(first_byte), str(size), str(has_name), str(stub_ret)],
            run_dobuffer,
        ))

    # 7. 0x00404370 LuaParserCheckNext
    for cur_tok, look_tok, expected, next_tok in [
        (40, 286, 40, 265),
        (40, 270, 40, 265),
        (41, 286, 40, 272),
    ]:
        def run_checknext(cur_tok=cur_tok, look_tok=look_tok, expected=expected, next_tok=next_tok) -> str:
            oracle.reset_arena()
            wi32(emu, ls_va + 0x08, cur_tok)
            wi32(emu, ls_va + 0x18, look_tok)
            w32(emu, ls_va + 0x20, ARENA_BASE + 0x3330)
            wi32(emu, ls_va + 0x34, 42)
            wi32(emu, ls_va + 0x38, 10)
            tr.lex_next_token = next_tok
            tr.lex_next_seminfo_lo = 0x12345678
            tr.lex_next_seminfo_hi = 0x9ABCDEF0
            oracle.invoke(0x00404370, registers={"esi": ls_va, "eax": expected})
            return (
                f"tok={i32(emu, ls_va + 0x08)} look={i32(emu, ls_va + 0x18)} "
                f"lastline={i32(emu, ls_va + 0x38)} calls={tr.trace_calls} hash=0x{tr.trace_hash:08x}"
            )
        scenarios.append((
            f"0x00404370_checknext_{cur_tok}_{look_tok}_{expected}",
            ["checknext", str(cur_tok), str(look_tok), str(expected), str(next_tok)],
            run_checknext,
        ))

    # 8. 0x004043F0 LuaParserOptional
    for cur_tok, look_tok, expected, next_tok in [
        (44, 286, 44, 261),
        (44, 275, 44, 261),
        (59, 286, 44, 261),
    ]:
        def run_optional(cur_tok=cur_tok, look_tok=look_tok, expected=expected, next_tok=next_tok) -> str:
            oracle.reset_arena()
            wi32(emu, ls_va + 0x08, cur_tok)
            wi32(emu, ls_va + 0x18, look_tok)
            wi32(emu, ls_va + 0x34, 55)
            wi32(emu, ls_va + 0x38, 12)
            tr.lex_next_token = next_tok
            res = oracle.invoke(0x004043F0, expected, registers={"esi": ls_va})
            return (
                f"ret={res} tok={i32(emu, ls_va + 0x08)} look={i32(emu, ls_va + 0x18)} "
                f"lastline={i32(emu, ls_va + 0x38)} calls={tr.trace_calls} hash=0x{tr.trace_hash:08x}"
            )
        scenarios.append((
            f"0x004043F0_optional_{cur_tok}_{look_tok}_{expected}",
            ["optional", str(cur_tok), str(look_tok), str(expected), str(next_tok)],
            run_optional,
        ))

    # 9. 0x00404450 LuaParserCheckMatch
    for cur_tok, look_tok, line_no, what, where in [
        (262, 286, 20, 262, 20),
        (263, 286, 20, 262, 20),
        (263, 277, 30, 262, 15),
    ]:
        def run_checkmatch(cur_tok=cur_tok, look_tok=look_tok, line_no=line_no, what=what, where=where) -> str:
            oracle.reset_arena()
            wi32(emu, ls_va + 0x08, cur_tok)
            wi32(emu, ls_va + 0x18, look_tok)
            wi32(emu, ls_va + 0x34, line_no)
            tr.lex_next_token = 285
            oracle.invoke(0x00404450, 257, registers={"esi": ls_va, "ecx": what, "edi": where})
            return (
                f"tok={i32(emu, ls_va + 0x08)} look={i32(emu, ls_va + 0x18)} "
                f"lastline={i32(emu, ls_va + 0x38)} calls={tr.trace_calls} hash=0x{tr.trace_hash:08x}"
            )
        scenarios.append((
            f"0x00404450_checkmatch_{cur_tok}_{line_no}_{where}",
            ["checkmatch", str(cur_tok), str(look_tok), str(line_no), str(what), str(where)],
            run_checkmatch,
        ))

    # 10. 0x00404550 LuaParserStringK
    for sizekstr, const_idx, match_existing in [
        (3, 1, 1),
        (2, 0, 0),
        (4, 5, 0),
    ]:
        def run_stringk(sizekstr=sizekstr, const_idx=const_idx, match_existing=match_existing) -> str:
            oracle.reset_arena()
            kstr_va = ARENA_BASE + 0x3000
            ts_va = ARENA_BASE + 0x3400
            w32(emu, ls_va + 0x28, fs_va)
            w32(emu, fs_va + 0x00, proto_va)
            w32(emu, fs_va + 0x0C, state_va)
            w32(emu, proto_va + 0x08, kstr_va)
            wi32(emu, proto_va + 0x0C, sizekstr)
            wi32(emu, ts_va + 0x04, const_idx)
            if match_existing and 0 <= const_idx < sizekstr:
                w32(emu, kstr_va + const_idx * 4, ts_va)
            res = oracle.invoke(0x00404550, registers={"ebx": ls_va, "edi": ts_va})
            return (
                f"ret={res} sizekstr={i32(emu, proto_va + 0x0C)} "
                f"constIdx={i32(emu, ts_va + 0x04)} kstr=0x{u32(emu, proto_va + 0x08):08x} "
                f"calls={tr.trace_calls} hash=0x{tr.trace_hash:08x}"
            )
        scenarios.append((
            f"0x00404550_stringk_{sizekstr}_{const_idx}_{match_existing}",
            ["stringk", str(sizekstr), str(const_idx), str(match_existing)],
            run_stringk,
        ))

    # 11. 0x004048E0 LuaParserAdjustMultAssign
    for nvars, nexps, prev_inst, pc in [
        (3, 1, 0x00003FC2, 2),
        (1, 3, 0x00003FC2, 2),
        (3, 2, 0x00000042, 2),
    ]:
        def run_adjustmult(nvars=nvars, nexps=nexps, prev_inst=prev_inst, pc=pc) -> str:
            oracle.reset_arena()
            code_va = ARENA_BASE + 0x3800
            w32(emu, ls_va + 0x28, fs_va)
            w32(emu, fs_va + 0x00, proto_va)
            w32(emu, fs_va + 0x08, ls_va)
            w32(emu, fs_va + 0x0C, state_va)
            wi32(emu, fs_va + 0x10, pc)
            wi32(emu, fs_va + 0x14, 0)
            wi16(emu, fs_va + 0x1C, 10)
            w32(emu, proto_va + 0x18, code_va)
            wi16(emu, proto_va + 0x24, 12)
            if pc > 0:
                w32(emu, code_va + (pc - 1) * 4, prev_inst)
            oracle.invoke(0x004048E0, nvars, registers={"ecx": ls_va, "eax": nexps})
            new_pc = i32(emu, fs_va + 0x10)
            new_code_va = u32(emu, proto_va + 0x18)
            return (
                f"pc={new_pc} freereg={i16(emu, fs_va + 0x1C)} "
                f"maxstack={i16(emu, proto_va + 0x24)} "
                f"code0=0x{u32(emu, new_code_va + (new_pc - 1) * 4):08x} "
                f"calls={tr.trace_calls} hash=0x{tr.trace_hash:08x}"
            )
        scenarios.append((
            f"0x004048E0_adjustmult_{nvars}_{nexps}_{hex(prev_inst)}",
            ["adjustmult", str(nvars), str(nexps), hex(prev_inst), str(pc)],
            run_adjustmult,
        ))

    # 12. 0x00404AC0 LuaParserCloseFunc
    for pc, lasttarget in [(6, 2), (4, 4)]:
        def run_closefunc(pc=pc, lasttarget=lasttarget) -> str:
            oracle.reset_arena()
            prev_fs = ARENA_BASE + 0x0480
            w32(emu, ls_va + 0x28, fs_va)
            w32(emu, ls_va + 0x2C, state_va)
            w32(emu, fs_va + 0x00, proto_va)
            w32(emu, fs_va + 0x04, prev_fs)
            wi32(emu, fs_va + 0x10, pc)
            wi32(emu, fs_va + 0x14, lasttarget)
            wi32(emu, fs_va + 0x18, 3)
            wi16(emu, fs_va + 0x1E, 4)
            wi32(emu, proto_va + 0x0C, 2)
            wi32(emu, proto_va + 0x04, 3)
            wi32(emu, proto_va + 0x14, 1)
            wi32(emu, proto_va + 0x34, 2)
            wi32(emu, proto_va + 0x30, 5)
            oracle.invoke(0x00404AC0, ls_va)
            sli = i32(emu, proto_va + 0x30)
            li_va = u32(emu, proto_va + 0x2C)
            return (
                f"fs=0x{u32(emu, ls_va + 0x28):08x} sizelineinfo={sli} "
                f"lineend=0x{u32(emu, li_va + (sli - 1) * 4):08x} "
                f"jpc={i32(emu, fs_va + 0x18)} lasttarget={i32(emu, fs_va + 0x14)} "
                f"calls={tr.trace_calls} hash=0x{tr.trace_hash:08x}"
            )
        scenarios.append((
            f"0x00404AC0_closefunc_{pc}_{lasttarget}",
            ["closefunc", str(pc), str(lasttarget)],
            run_closefunc,
        ))

    # 13. 0x00405B10 LuaParserFuncStat
    for look_tok, needself, line in [(286, 0, 17), (270, 1, 33)]:
        def run_funcstat(look_tok=look_tok, needself=needself, line=line) -> str:
            oracle.reset_arena()
            wi32(emu, ls_va + 0x18, look_tok)
            wi32(emu, ls_va + 0x34, line)
            tr.stub_ret = needself
            tr.lex_next_token = 260
            oracle.invoke(0x00405B10, line, registers={"esi": ls_va})
            return (
                f"tok={i32(emu, ls_va + 0x08)} look={i32(emu, ls_va + 0x18)} "
                f"lastline={i32(emu, ls_va + 0x38)} calls={tr.trace_calls} hash=0x{tr.trace_hash:08x}"
            )
        scenarios.append((
            f"0x00405B10_funcstat_{look_tok}_{needself}",
            ["funcstat", str(look_tok), str(needself), str(line)],
            run_funcstat,
        ))

    # 14. 0x00406CF0 LuaVmTraceExec
    for pc_off, lastpc, curline, newline in [
        (1, 5, 10, 12),
        (4, 2, 10, 10),
        (3, 5, 10, 10),
    ]:
        def run_traceexec(pc_off=pc_off, lastpc=lastpc, curline=curline, newline=newline) -> str:
            oracle.reset_arena()
            tr.stub_ret = newline
            top_va = ARENA_BASE + 0x1200
            ci_va = ARENA_BASE + 0x1300
            cl_va = ARENA_BASE + 0x1400
            pch_va = ARENA_BASE + 0x1500
            code_va = ARENA_BASE + 0x1600
            li_va = ARENA_BASE + 0x1700
            w32(emu, top_va - 8, ci_va)
            w32(emu, cl_va, proto_va)
            w32(emu, proto_va + 0x18, code_va)
            w32(emu, proto_va + 0x2C, li_va)
            w32(emu, pch_va, code_va + pc_off * 4)
            w32(emu, ci_va + 0x00, cl_va)
            w32(emu, ci_va + 0x04, pch_va)
            wi32(emu, ci_va + 0x08, lastpc)
            wi32(emu, ci_va + 0x0C, curline)
            wi32(emu, ci_va + 0x10, 2)
            oracle.invoke(0x00406CF0, state_va, top_va, 0x00409999, registers={"eax": top_va})
            return (
                f"lastpc={i32(emu, ci_va + 0x08)} line={i32(emu, ci_va + 0x0C)} "
                f"refi={i32(emu, ci_va + 0x10)} top=0x{u32(emu, state_va):08x} "
                f"calls={tr.trace_calls} hash=0x{tr.trace_hash:08x}"
            )
        scenarios.append((
            f"0x00406CF0_traceexec_{pc_off}_{lastpc}_{newline}",
            ["traceexec", str(pc_off), str(lastpc), str(curline), str(newline)],
            run_traceexec,
        ))

    # 15. 0x00406D70 LuaVmClosure
    for nelems, at_last in [(0, 0), (2, 0), (2, 1)]:
        def run_closure(nelems=nelems, at_last=at_last) -> str:
            oracle.reset_arena()
            base_va = ARENA_BASE + 0x1000
            w32(emu, state_va + 0x04, base_va)
            top_va = base_va + 4 * 16
            w32(emu, state_va + 0x00, top_va)
            for idx in range(6):
                wi32(emu, base_va + idx * 16, 2)
                wu64(emu, base_va + idx * 16 + 8, 0x100 + idx)
            last_va = (top_va - nelems * 16) if at_last else (base_va + 20 * 16)
            w32(emu, state_va + 0x08, last_va)
            wi32(emu, state_va + 0x0C, 30)
            res = oracle.invoke(0x00406D70, registers={"esi": state_va, "eax": nelems})
            new_top = u32(emu, state_va)
            return (
                f"ret=0x{res:08x} top=0x{new_top:08x} topType={i32(emu, new_top - 16)} "
                f"up0={u64(emu, res + 0x18)} calls={tr.trace_calls} hash=0x{tr.trace_hash:08x}"
            )
        scenarios.append((
            f"0x00406D70_closure_{nelems}_{at_last}",
            ["closure", str(nelems), str(at_last)],
            run_closure,
        ))

    # 16. 0x00408DB0 LuaGcCollectProto
    for mark0, mark1 in [(0, 1), (1, 0), (0, 0)]:
        def run_collectproto(mark0=mark0, mark1=mark1) -> str:
            oracle.reset_arena()
            p0 = ARENA_BASE + 0x2000
            p1 = ARENA_BASE + 0x2100
            w32(emu, p0 + 0x04, p1)
            w32(emu, p0 + 0x08, 0 if mark0 else p0)
            w32(emu, p1 + 0x04, 0)
            w32(emu, p1 + 0x08, 0 if mark1 else p1)
            w32(emu, state_va + 0x24, p0)
            oracle.invoke(0x00408DB0, registers={"edi": state_va})
            return (
                f"root=0x{u32(emu, state_va + 0x24):08x} calls={tr.trace_calls} "
                f"hash=0x{tr.trace_hash:08x}"
            )
        scenarios.append((
            f"0x00408DB0_collectproto_{mark0}_{mark1}",
            ["collectproto", str(mark0), str(mark1)],
            run_collectproto,
        ))

    # 17. 0x00408DF0 LuaGcCollectTable
    for mark0, mark1 in [(0, 1), (1, 0), (0, 0)]:
        def run_collecttable(mark0=mark0, mark1=mark1) -> str:
            oracle.reset_arena()
            h0 = ARENA_BASE + 0x2200
            h1 = ARENA_BASE + 0x2300
            w32(emu, h0 + 0x14, h1)
            w32(emu, h0 + 0x18, 0 if mark0 else h0)
            w32(emu, h1 + 0x14, 0)
            w32(emu, h1 + 0x18, 0 if mark1 else h1)
            w32(emu, state_va + 0x28, h0)
            oracle.invoke(0x00408DF0, registers={"edi": state_va})
            return (
                f"root=0x{u32(emu, state_va + 0x28):08x} calls={tr.trace_calls} "
                f"hash=0x{tr.trace_hash:08x}"
            )
        scenarios.append((
            f"0x00408DF0_collecttable_{mark0}_{mark1}",
            ["collecttable", str(mark0), str(mark1)],
            run_collecttable,
        ))

    # 18. 0x00408E30 LuaGcCollectStrings
    for mark0, all_flag, tbl_size in [(1, 0, 32), (0, 0, 32), (2, 0, 16)]:
        def run_collectstrings(mark0=mark0, all_flag=all_flag, tbl_size=tbl_size) -> str:
            oracle.reset_arena()
            buckets = ARENA_BASE + 0x2400
            s0 = ARENA_BASE + 0x2600
            w32(emu, s0 + 0x08, 7)
            wi32(emu, s0 + 0x10, mark0)
            w32(emu, s0 + 0x0C, 0)
            w32(emu, buckets, s0)
            wi32(emu, state_va + 0x2C, tbl_size)
            w32(emu, state_va + 0x30, 2)
            w32(emu, state_va + 0x34, buckets)
            w32(emu, state_va + 0x60, 500)
            oracle.invoke(0x00408E30, all_flag, registers={"esi": state_va})
            return (
                f"nuse={u32(emu, state_va + 0x30)} size={i32(emu, state_va + 0x2C)} "
                f"nblocks={u32(emu, state_va + 0x60)} s0mark={i32(emu, s0 + 0x10)} "
                f"bucket0=0x{u32(emu, buckets):08x} calls={tr.trace_calls} hash=0x{tr.trace_hash:08x}"
            )
        scenarios.append((
            f"0x00408E30_collectstrings_{mark0}_{all_flag}_{tbl_size}",
            ["collectstrings", str(mark0), str(all_flag), str(tbl_size)],
            run_collectstrings,
        ))

    # 19. 0x00408ED0 LuaGcCollectUdata
    for mark0, all_flag, tbl_size in [(1, 0, 32), (0, 0, 32), (1, 1, 16)]:
        def run_collectudata(mark0=mark0, all_flag=all_flag, tbl_size=tbl_size) -> str:
            oracle.reset_arena()
            buckets = ARENA_BASE + 0x2800
            u0 = ARENA_BASE + 0x2A00
            tm = ARENA_BASE + 0x2C00
            wi32(emu, u0 + 0x00, 1)
            w32(emu, u0 + 0x08, 12)
            wi32(emu, u0 + 0x10, mark0)
            w32(emu, u0 + 0x0C, 0)
            w32(emu, buckets, u0)
            wi32(emu, state_va + 0x38, tbl_size)
            w32(emu, state_va + 0x3C, 2)
            w32(emu, state_va + 0x40, buckets)
            w32(emu, state_va + 0x48, tm)
            w32(emu, state_va + 0x60, 600)
            oracle.invoke(0x00408ED0, all_flag, registers={"ecx": state_va})
            return (
                f"nuse={u32(emu, state_va + 0x3C)} size={i32(emu, state_va + 0x38)} "
                f"nblocks={u32(emu, state_va + 0x60)} collected=0x{u32(emu, tm + 0x40 + 0x3C):08x} "
                f"u0mark={i32(emu, u0 + 0x10)} calls={tr.trace_calls} hash=0x{tr.trace_hash:08x}"
            )
        scenarios.append((
            f"0x00408ED0_collectudata_{mark0}_{all_flag}_{tbl_size}",
            ["collectudata", str(mark0), str(all_flag), str(tbl_size)],
            run_collectudata,
        ))

    # 20. 0x00409120 LuaGcMarkAll
    for ref_status, node_val_type in [(-4, 5), (0, 1)]:
        def run_markall(ref_status=ref_status, node_val_type=node_val_type) -> str:
            oracle.reset_arena()
            gt = ARENA_BASE + 0x3000
            nodes = ARENA_BASE + 0x3100
            stack = ARENA_BASE + 0x3300
            refs = ARENA_BASE + 0x3500
            cl = ARENA_BASE + 0x3700
            s_key = ARENA_BASE + 0x3800
            s_up = ARENA_BASE + 0x3900
            w32(emu, gt + 0x00, nodes)
            wi32(emu, gt + 0x08, 1)
            w32(emu, gt + 0x18, gt)
            wi32(emu, nodes + 0x00, 3)
            w32(emu, nodes + 0x08, s_key)
            wi32(emu, nodes + 0x10, node_val_type)
            w32(emu, nodes + 0x18, cl)
            w32(emu, cl + 0x08, cl)
            wi16(emu, cl + 0x0C, 1)  # isC = 1 so markclosure skips markproto(cl->f.l)
            wi16(emu, cl + 0x0E, 1)
            wi32(emu, cl + 0x10, 3)
            w32(emu, cl + 0x18, s_up)
            wi32(emu, stack + 0x00, 5)
            w32(emu, stack + 0x08, cl)
            wi32(emu, refs + 0x00, 3)
            w32(emu, refs + 0x08, s_key)
            wi32(emu, refs + 0x10, ref_status)
            w32(emu, state_va + 0x44, gt)
            w32(emu, state_va + 0x04, stack)
            w32(emu, state_va + 0x00, stack + 16)
            w32(emu, state_va + 0x50, refs)
            wi32(emu, state_va + 0x54, 1)
            oracle.invoke(0x00409120, registers={"eax": state_va})
            return (
                f"sKeyMark={i32(emu, s_key + 0x10)} sUpMark={i32(emu, s_up + 0x10)} "
                f"clMark=0x{u32(emu, cl + 0x08):08x} calls={tr.trace_calls} hash=0x{tr.trace_hash:08x}"
            )
        scenarios.append((
            f"0x00409120_markall_{ref_status}_{node_val_type}",
            ["markall", str(ref_status), str(node_val_type)],
            run_markall,
        ))

    # 21. 0x00409E90 LuaNewTag
    for last_tag in [3, 4]:
        def run_newtag(last_tag=last_tag) -> str:
            oracle.reset_arena()
            tm = ARENA_BASE + 0x4000
            emu.write(tm, b"\x5A" * (64 * 16))
            w32(emu, state_va + 0x48, tm)
            wi32(emu, state_va + 0x4C, last_tag)
            w32(emu, state_va + 0x60, 128)
            res = oracle.invoke(0x00409E90, state_va)
            new_tm = u32(emu, state_va + 0x48)
            new_last = i32(emu, state_va + 0x4C)
            return (
                f"ret={res} lastTag={new_last} nblocks={u32(emu, state_va + 0x60)} "
                f"tm=0x{new_tm:08x} m0=0x{u32(emu, new_tm + new_last * 64):08x} "
                f"calls={tr.trace_calls} hash=0x{tr.trace_hash:08x}"
            )
        scenarios.append((
            f"0x00409E90_newtag_{last_tag}",
            ["newtag", str(last_tag)],
            run_newtag,
        ))

    # 22. 0x0040A070 LuaTagOfObject
    for ttype, inner_tag in [(0, 9), (4, 11), (3, 99)]:
        def run_tagof(ttype=ttype, inner_tag=inner_tag) -> str:
            oracle.reset_arena()
            obj = ARENA_BASE + 0x1000
            ts = ARENA_BASE + 0x1100
            ht = ARENA_BASE + 0x1200
            wi32(emu, obj, ttype)
            if ttype == 0:
                wi32(emu, ts, inner_tag)
                w32(emu, obj + 8, ts)
            elif ttype == 4:
                wi32(emu, ht + 4, inner_tag)
                w32(emu, obj + 8, ht)
            res = struct.unpack("<i", struct.pack("<I", oracle.invoke(0x0040A070, obj)))[0]
            return f"ret={res}"
        scenarios.append((
            f"0x0040A070_tagof_{ttype}_{inner_tag}",
            ["tagof", str(ttype), str(inner_tag)],
            run_tagof,
        ))

    # 23. 0x0040A120 LuaSetTagMethod
    for tag, ev_idx, top_type in [
        (0, 0, 5),
        (4, 0, 1),
        (7, 0, 2),
        (12, 0, 5),
    ]:
        def run_settagmethod(tag=tag, ev_idx=ev_idx, top_type=top_type) -> str:
            oracle.reset_arena()
            tr.stub_ret = ev_idx
            tm = ARENA_BASE + 0x4000
            stack = ARENA_BASE + 0x1000
            cl = ARENA_BASE + 0x1500
            ev_str = ARENA_BASE + 0x1600
            emu.write(ev_str, b"gettable\x00")
            w32(emu, state_va + 0x48, tm)
            wi32(emu, state_va + 0x4C, 8)
            w32(emu, state_va + 0x04, stack)
            w32(emu, state_va + 0x00, stack + 32)
            wi32(emu, stack + 16, top_type)
            w32(emu, stack + 24, cl)
            oracle.invoke(0x0040A120, state_va, tag, ev_str)
            safe_tag = tag if 0 <= tag <= 8 else 0
            return (
                f"top=0x{u32(emu, state_va):08x} "
                f"method=0x{u32(emu, tm + (safe_tag * 16 + ev_idx) * 4):08x} "
                f"calls={tr.trace_calls} hash=0x{tr.trace_hash:08x}"
            )
        scenarios.append((
            f"0x0040A120_settagmethod_{tag}_{ev_idx}_{top_type}",
            ["settagmethod", str(tag), str(ev_idx), str(top_type)],
            run_settagmethod,
        ))

    # 24. 0x0040A340 LuaPushValue
    for index, at_last in [(2, 0), (-2, 1)]:
        def run_pushvalue(index=index, at_last=at_last) -> str:
            oracle.reset_arena()
            stack = ARENA_BASE + 0x1000
            for idx in range(8):
                wi32(emu, stack + idx * 16, 2)
                wu64(emu, stack + idx * 16 + 8, 0x500 + idx)
            w32(emu, state_va + 0x04, stack)
            w32(emu, state_va + 0x10, stack + 16)
            w32(emu, state_va + 0x00, stack + 5 * 16)
            w32(emu, state_va + 0x08, (stack + 5 * 16) if at_last else (stack + 16 * 16))
            wi32(emu, state_va + 0x0C, 20)
            oracle.invoke(0x0040A340, state_va, index)
            new_top = u32(emu, state_va)
            return (
                f"top=0x{new_top:08x} pushed={u64(emu, new_top - 8)} "
                f"calls={tr.trace_calls} hash=0x{tr.trace_hash:08x}"
            )
        scenarios.append((
            f"0x0040A340_pushvalue_{index}_{at_last}",
            ["pushvalue", str(index), str(at_last)],
            run_pushvalue,
        ))

    # 25. 0x0040CA60 LuaCodeLastIsOpen
    for pc, lasttarget, inst in [
        (3, 1, 0x00003FC2),
        (3, 3, 0x00003FC2),
        (3, 1, 0x00000082),
    ]:
        def run_lastisopen(pc=pc, lasttarget=lasttarget, inst=inst) -> str:
            oracle.reset_arena()
            code = ARENA_BASE + 0x3800
            w32(emu, fs_va + 0x00, proto_va)
            wi32(emu, fs_va + 0x10, pc)
            wi32(emu, fs_va + 0x14, lasttarget)
            w32(emu, proto_va + 0x18, code)
            if pc > 0:
                w32(emu, code + (pc - 1) * 4, inst)
            res = oracle.invoke(0x0040CA60, fs_va)
            return f"ret={res}"
        scenarios.append((
            f"0x0040CA60_lastisopen_{pc}_{lasttarget}_{hex(inst)}",
            ["lastisopen", str(pc), str(lasttarget), hex(inst)],
            run_lastisopen,
        ))

    # 26. 0x0040CAA0 LuaCodeSetCallReturns
    for pc, lasttarget, inst, nresults in [
        (3, 1, 0x00003FC2, 2),
        (3, 1, 0x00003FC2, 4),
        (3, 3, 0x00003FC2, 2),
    ]:
        def run_setcallreturns(pc=pc, lasttarget=lasttarget, inst=inst, nresults=nresults) -> str:
            oracle.reset_arena()
            code = ARENA_BASE + 0x3800
            w32(emu, fs_va + 0x00, proto_va)
            w32(emu, fs_va + 0x08, ls_va)
            wi32(emu, fs_va + 0x10, pc)
            wi32(emu, fs_va + 0x14, lasttarget)
            wi16(emu, fs_va + 0x1C, 6)
            w32(emu, proto_va + 0x18, code)
            wi16(emu, proto_va + 0x24, 8)
            if pc > 0:
                w32(emu, code + (pc - 1) * 4, inst)
            oracle.invoke(0x0040CAA0, fs_va, nresults)
            return (
                f"code0=0x{u32(emu, code + (pc - 1) * 4):08x} "
                f"freereg={i16(emu, fs_va + 0x1C)} maxstack={i16(emu, proto_va + 0x24)}"
            )
        scenarios.append((
            f"0x0040CAA0_setcallreturns_{pc}_{lasttarget}_{nresults}",
            ["setcallreturns", str(pc), str(lasttarget), hex(inst), str(nresults)],
            run_setcallreturns,
        ))

    # 26. 0x0040D2F0 LuaCodeGetLabel
    for pc, lasttarget in [(7, 2), (5, 5)]:
        def run_getlabel(pc=pc, lasttarget=lasttarget) -> str:
            oracle.reset_arena()
            wi32(emu, fs_va + 0x10, pc)
            wi32(emu, fs_va + 0x14, lasttarget)
            wi32(emu, fs_va + 0x18, 9)
            res = oracle.invoke(0x0040D2F0, fs_va)
            return (
                f"ret={res} lasttarget={i32(emu, fs_va + 0x14)} "
                f"jpc={i32(emu, fs_va + 0x18)} calls={tr.trace_calls} hash=0x{tr.trace_hash:08x}"
            )
        scenarios.append((
            f"0x0040D2F0_getlabel_{pc}_{lasttarget}",
            ["getlabel", str(pc), str(lasttarget)],
            run_getlabel,
        ))

    # 27. 0x0040D5B0 LuaCodeAdjustStack
    for n, prev_inst, freereg, maxstack in [
        (3, 0x00000095, 10, 8),
        (2, 0x00000001, 10, 12),
        (-3, 0x00000084, 5, 6),
        (-2, 0x00000001, 250, 250),
    ]:
        def run_adjuststack(n=n, prev_inst=prev_inst, freereg=freereg, maxstack=maxstack) -> str:
            oracle.reset_arena()
            code = ARENA_BASE + 0x3800
            w32(emu, fs_va + 0x00, proto_va)
            w32(emu, fs_va + 0x08, ls_va)
            w32(emu, fs_va + 0x0C, state_va)
            wi32(emu, ls_va + 0x08, 280)
            wi32(emu, fs_va + 0x10, 2)
            wi32(emu, fs_va + 0x14, 0)
            wi16(emu, fs_va + 0x1C, freereg)
            w32(emu, proto_va + 0x18, code)
            wi16(emu, proto_va + 0x24, maxstack)
            w32(emu, code + 4, prev_inst)
            oracle.invoke(0x0040D5B0, fs_va, n)
            new_pc = i32(emu, fs_va + 0x10)
            new_code = u32(emu, proto_va + 0x18)
            return (
                f"pc={new_pc} freereg={i16(emu, fs_va + 0x1C)} "
                f"maxstack={i16(emu, proto_va + 0x24)} "
                f"lastInst=0x{u32(emu, new_code + (new_pc - 1) * 4):08x} "
                f"calls={tr.trace_calls} hash=0x{tr.trace_hash:08x}"
            )
        scenarios.append((
            f"0x0040D5B0_adjuststack_{n}_{hex(prev_inst)}_{freereg}",
            ["adjuststack", str(n), hex(prev_inst), str(freereg), str(maxstack)],
            run_adjuststack,
        ))

    return scenarios


def main() -> int:
    oracle = LuaVmOracle()
    scenarios = build_scenarios(oracle)
    mismatches = 0
    for name, cli_args, emu_fn in scenarios:
        emu_out = emu_fn()
        cpp_out = run_checker(DEFAULT_CHECKER, *cli_args)
        if emu_out != cpp_out:
            mismatches += 1
            print(f"MISMATCH {name}:")
            print(f"  emu: {emu_out}")
            print(f"  cpp: {cpp_out}")
    print(f"Scenarios: {len(scenarios)}, Mismatches: {mismatches}")
    return 1 if mismatches else 0


if __name__ == "__main__":
    raise SystemExit(main())
