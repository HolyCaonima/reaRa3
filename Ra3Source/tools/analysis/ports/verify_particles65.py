"""Differentially verify the 53 clean RA3 FX particle-module functions in 0x0065E1E0-0x0066CE40."""

from __future__ import annotations

import argparse
import random
import struct
import subprocess
import sys
from pathlib import Path

from unicorn import x86_const

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from emulate import Emulator, RETURN_SENTINEL, STACK_BASE, STACK_SIZE, CallResult  # noqa: E402

CLIENT_STATE = 0x00CB73F0
REAL_FACTOR = 0x00DF65B0
TWO_PI_GLOBAL = 0x00CBE96C
PARTICLE_MGR_PTR = 0x00DF6F90
PARTICLE_MGR_PTR_2 = 0x00DF6F94
GAME_CLIENT_PTR = 0x00DF6594
CLIENT_FRAME_GLOBAL = 0x00E0484C
FRAME_DUR_GLOBAL = 0x00DEEEEC

CONTROL_WORDS = (0x007F, 0x027F)


class CppOracle:
    def __init__(self, executable: Path) -> None:
        self.process = subprocess.Popen(
            [str(executable)],
            stdin=subprocess.PIPE,
            stdout=subprocess.PIPE,
            text=True,
            bufsize=1,
        )

    def call(self, command: str) -> str:
        assert self.process.stdin is not None and self.process.stdout is not None
        self.process.stdin.write(command + "\n")
        self.process.stdin.flush()
        result = self.process.stdout.readline().strip()
        if not result or result == "error":
            raise RuntimeError(f"particles65check rejected {command!r}")
        return result

    def close(self) -> None:
        assert self.process.stdin is not None
        self.process.stdin.write("quit\n")
        self.process.stdin.flush()
        if self.process.wait(timeout=10) != 0:
            raise RuntimeError("particles65check failed")


def bits(value: float) -> int:
    return struct.unpack("<I", struct.pack("<f", value))[0]


def finite_float(rng: random.Random, lo: float = -25.0, hi: float = 25.0) -> int:
    return bits(rng.uniform(lo, hi))


def pos_float(rng: random.Random, lo: float = 0.05, hi: float = 25.0) -> int:
    return bits(rng.uniform(lo, hi))


def rand_rv_bytes(rng: random.Random, lo: float = -20.0, hi: float = 20.0) -> bytes:
    dist = rng.choice((0, 1, 2))
    a = rng.uniform(lo, hi)
    b = a if (dist == 0 and rng.randrange(2)) else rng.uniform(lo, hi)
    if b < a:
        a, b = b, a
    return struct.pack("<iII", dist, bits(a), bits(b))


def state_bytes(rng: random.Random) -> bytes:
    return bytes(rng.randrange(256) for _ in range(24))


def set_globals(emu: Emulator, state: bytes) -> None:
    emu.write(CLIENT_STATE, state)
    emu.write(REAL_FACTOR, struct.pack("<I", 0x2F800000))
    emu.write(TWO_PI_GLOBAL, struct.pack("<I", 0x40C90FDB))


def call_with_regs(
    emu: Emulator,
    address: int,
    stack_args: list[int],
    *,
    ecx: int = 0,
    edx: int = 0,
    esi: int = 0,
    edi: int = 0,
) -> CallResult:
    top = STACK_BASE + STACK_SIZE - 0x100
    frame = struct.pack(f"<I{len(stack_args)}I", RETURN_SENTINEL, *[a & 0xFFFFFFFF for a in stack_args])
    esp = top - len(frame)
    emu.uc.mem_write(esp, frame)
    for reg, val in (
        (x86_const.UC_X86_REG_ESP, esp),
        (x86_const.UC_X86_REG_EBP, 0),
        (x86_const.UC_X86_REG_EAX, 0),
        (x86_const.UC_X86_REG_EBX, 0),
        (x86_const.UC_X86_REG_ECX, ecx),
        (x86_const.UC_X86_REG_EDX, edx),
        (x86_const.UC_X86_REG_ESI, esi),
        (x86_const.UC_X86_REG_EDI, edi),
    ):
        emu.uc.reg_write(reg, val & 0xFFFFFFFF)
    emu.uc.emu_start(address, RETURN_SENTINEL, count=5_000_000)
    cleanup = emu.uc.reg_read(x86_const.UC_X86_REG_ESP) - (esp + 4)
    return CallResult(emu.uc.reg_read(x86_const.UC_X86_REG_EAX), emu.uc.reg_read(x86_const.UC_X86_REG_EDX), cleanup)


def verify(executable: Path, cases: int) -> None:
    rng = random.Random(0x65664658)
    emu = Emulator()
    cpp = CppOracle(executable)

    scratch_a = emu.alloc(bytes(0x400))
    scratch_b = emu.alloc(bytes(0x400))
    scratch_c = emu.alloc(bytes(0x400))
    scratch_d = emu.alloc(bytes(0x400))
    scratch_e = emu.alloc(bytes(0x400))
    scratch_f = emu.alloc(bytes(0x400))
    mgr_addr = emu.alloc(bytes(0x80))
    gc_addr = emu.alloc(bytes(0x40))
    gc_vt_addr = emu.alloc(bytes(0x40))

    client_frame_holder = [0]
    get_frame_stub = emu.bind_stub(lambda _e: client_frame_holder[0], 0)
    emu.write(gc_vt_addr + 0x1C, struct.pack("<I", get_frame_stub))
    emu.write(gc_addr, struct.pack("<I", gc_vt_addr))
    emu.write(GAME_CLIENT_PTR, struct.pack("<I", gc_addr))
    emu.write(PARTICLE_MGR_PTR, struct.pack("<I", mgr_addr))
    emu.write(PARTICLE_MGR_PTR_2, struct.pack("<I", mgr_addr))

    totals: dict[str, int] = {}

    def bump(key: str) -> None:
        totals[key] = totals.get(key, 0) + 1

    try:
        for cw in CONTROL_WORDS:
            if cpp.call(f"fpcw {cw}") != "ok":
                raise RuntimeError("could not set native FPU control word")
            emu.uc.reg_write(x86_const.UC_X86_REG_FPCW, cw)

            for case_idx in range(cases):
                state = state_bytes(rng)

                # 1-3: DefaultAlphaParticleModule (0x0065E1E0, 0x006650E0, 0x00661370)
                alpha_tmpl = bytearray(0x84)
                struct.pack_into("<I", alpha_tmpl, 0, 0x00BF2A58)
                t_cursor = 0.0
                for i in range(8):
                    rv = rand_rv_bytes(rng, 0.0, 1.0)
                    alpha_tmpl[0x04 + i * 0x10 : 0x10 + i * 0x10] = rv
                    if i == 0:
                        tf = 0.0
                    elif i < 5:
                        t_cursor += rng.uniform(0.1, 0.22)
                        tf = min(t_cursor, 1.0)
                    else:
                        tf = 0.0
                    struct.pack_into("<f", alpha_tmpl, 0x10 + i * 0x10, tf)

                init_life = rng.randint(10, 200)
                rem_life = rng.randint(1, init_life)
                part_blob = bytearray(0x80)
                struct.pack_into("<I", part_blob, 0x28, init_life)
                struct.pack_into("<I", part_blob, 0x38, rem_life)

                emu.write(scratch_a, bytes(alpha_tmpl))
                emu.write(scratch_b, bytes(part_blob))
                emu.write(scratch_c, struct.pack("<III", 0, 0, scratch_a))
                emu.write(scratch_d, b"\xCC" * 0x54)
                set_globals(emu, state)
                res = emu.call(0x00661370, [scratch_b, scratch_c], ecx=scratch_d)
                game_mod = bytearray(emu.read(scratch_d, 0x54))
                struct.pack_into("<I", game_mod, 4, 0)
                game_state = emu.read(CLIENT_STATE, 24)
                ours_mod_hex, ours_state_hex = cpp.call(
                    f"alpha_init {alpha_tmpl.hex()} {part_blob.hex()} {state.hex()}"
                ).split()
                if (
                    res.stack_cleanup != 8
                    or res.eax != scratch_d
                    or bytes(game_mod).hex() != ours_mod_hex
                    or game_state.hex() != ours_state_hex
                ):
                    raise AssertionError(f"0x00661370 alpha_init mismatch: {bytes(game_mod).hex()} vs {ours_mod_hex}")
                bump("0x00661370")

                # 0x0065E1E0 computeDefaultAlphaRate directly
                mod_in = bytearray(game_mod)
                struct.pack_into("<i", mod_in, 0x50, rng.choice((0, 1, 2, 3, 8)))
                struct.pack_into("<I", mod_in, 4, scratch_b)
                emu.write(scratch_d, bytes(mod_in))
                res = emu.call(0x0065E1E0, [], ecx=scratch_d)
                game_mod2 = bytearray(emu.read(scratch_d, 0x54))
                struct.pack_into("<I", game_mod2, 4, 0)
                struct.pack_into("<I", mod_in, 4, 0)
                ours_rate_hex = cpp.call(f"alpha_rate {mod_in.hex()} {part_blob.hex()}")
                if res.stack_cleanup != 0 or bytes(game_mod2).hex() != ours_rate_hex:
                    raise AssertionError("0x0065E1E0 alpha_rate mismatch")
                bump("0x0065E1E0")

                # 0x006650E0 updateDefaultAlphaParticle
                shader_type = rng.choice((0, 1, 2, 3, 4))
                sys_tmpl = bytearray(0x8C)
                struct.pack_into("<I", sys_tmpl, 0x4C, shader_type)
                emu.write(scratch_e, bytes(sys_tmpl))
                sys_stub = bytearray(0x130)
                struct.pack_into("<I", sys_stub, 0x08, scratch_e)
                emu.write(scratch_f, bytes(sys_stub))
                emu.write(scratch_c, struct.pack("<II", scratch_f, 1))
                part_upd = bytearray(part_blob)
                struct.pack_into("<I", part_upd, 0x34, scratch_c)
                emu.write(scratch_b, bytes(part_upd))
                mod_upd = bytearray(game_mod)
                struct.pack_into("<I", mod_upd, 4, scratch_b)
                emu.write(scratch_d, bytes(mod_upd))
                res = emu.call(0x006650E0, [], ecx=scratch_d)
                game_mod3 = bytearray(emu.read(scratch_d, 0x54))
                struct.pack_into("<I", game_mod3, 4, 0)
                struct.pack_into("<I", part_upd, 0x34, 0)
                ours_upd_hex = cpp.call(f"alpha_update {game_mod.hex()} {part_upd.hex()} {shader_type}")
                if res.stack_cleanup != 0 or bytes(game_mod3).hex() != ours_upd_hex:
                    raise AssertionError("0x006650E0 alpha_update mismatch")
                bump("0x006650E0")

                # 4-9: DefaultColorModule & DefaultColorParticleModule
                color_tmpl = bytearray(0x98)
                struct.pack_into("<I", color_tmpl, 0, 0x00BF2AC0)
                t_cursor = 0.0
                for i in range(8):
                    r_v = rng.uniform(0.0, 2.0) if i < 5 else 0.0
                    g_v = rng.uniform(0.0, 2.0) if i < 5 else 0.0
                    b_v = rng.uniform(0.0, 2.0) if i < 5 else 0.0
                    if i == 0:
                        tf = 0.0
                    elif i < 5:
                        t_cursor += rng.uniform(0.1, 0.22)
                        tf = min(t_cursor, 1.0)
                    else:
                        tf = 0.0
                    struct.pack_into("<ffff", color_tmpl, 0x04 + i * 0x10, r_v, g_v, b_v, tf)
                if case_idx % 2 == 0:
                    color_tmpl[0x84:0x90] = struct.pack("<iff", 0, 1.0, 1.0)
                else:
                    color_tmpl[0x84:0x90] = rand_rv_bytes(rng, 0.2, 2.5)
                color_tmpl[0x90] = rng.choice((0, 1))
                struct.pack_into("<f", color_tmpl, 0x94, rng.uniform(-0.2, 1.0))

                ref_cnt = rng.randint(1, 50)
                sys_stub = bytearray(0x130)
                struct.pack_into("<i", sys_stub, 0x04, ref_cnt)
                emu.write(scratch_a, bytes(color_tmpl))
                emu.write(scratch_b, bytes(sys_stub))
                emu.write(scratch_c, struct.pack("<I", scratch_b))
                emu.write(scratch_d, bytes(0xA0))
                res = emu.call(0x00669AB0, [scratch_c, scratch_a], ecx=scratch_d)
                game_cmod = bytearray(emu.read(scratch_d, 0xA0))
                struct.pack_into("<II", game_cmod, 4, 0, 0)
                game_ref = struct.unpack("<i", emu.read(scratch_b + 4, 4))[0]
                ours_cmod_hex, ours_ref_str = cpp.call(f"color_mod_init {color_tmpl.hex()} {ref_cnt}").split()
                if (
                    res.stack_cleanup != 8
                    or res.eax != scratch_d
                    or bytes(game_cmod).hex() != ours_cmod_hex
                    or game_ref != int(ours_ref_str)
                ):
                    raise AssertionError("0x00669AB0 color_mod_init mismatch")
                bump("0x00669AB0")

                # 0x00669CA0 getDefaultColorScaleRange
                emu.write(scratch_d, bytes(game_cmod))
                emu.write(scratch_e, bytes(8))
                res = emu.call(0x00669CA0, [scratch_e], ecx=scratch_d)
                game_range = emu.read(scratch_e, 8)
                ours_range_hex = cpp.call(f"color_scale_range {game_cmod.hex()}")
                if res.stack_cleanup != 4 or game_range.hex() != ours_range_hex:
                    raise AssertionError("0x00669CA0 color_scale_range mismatch")
                bump("0x00669CA0")

                # 0x0065E490 applyDefaultColorTint
                packed_rgb = rng.choice((0, 0x00FF8040, 0x00123456, 0x00FFFFFF))
                emu.write(scratch_d, bytes(game_cmod))
                res = emu.call(0x0065E490, [packed_rgb], ecx=scratch_d)
                game_tinted = emu.read(scratch_d, 0xA0)
                ours_tinted_hex = cpp.call(f"color_tint {game_cmod.hex()} {packed_rgb}")
                if res.stack_cleanup != 4 or game_tinted.hex() != ours_tinted_hex:
                    raise AssertionError("0x0065E490 color_tint mismatch")
                bump("0x0065E490")

                # 0x00661590 initializeDefaultColorParticle
                emu.write(scratch_b, bytes(part_blob))
                emu.write(scratch_d, bytes(game_cmod))
                emu.write(scratch_e, bytes(0xA4))
                set_globals(emu, state)
                res = emu.call(0x00661590, [scratch_b, scratch_d], ecx=scratch_e)
                game_cpmod = bytearray(emu.read(scratch_e, 0xA4))
                struct.pack_into("<I", game_cpmod, 4, 0)
                game_state = emu.read(CLIENT_STATE, 24)
                ours_cpmod_hex, ours_state_hex = cpp.call(
                    f"color_part_init {game_cmod.hex()} {part_blob.hex()} {state.hex()}"
                ).split()
                if (
                    res.stack_cleanup != 8
                    or res.eax != scratch_e
                    or bytes(game_cpmod).hex() != ours_cpmod_hex
                    or game_state.hex() != ours_state_hex
                ):
                    raise AssertionError("0x00661590 color_part_init mismatch")
                bump("0x00661590")

                # 0x0065E710 computeDefaultColorRate
                cp_in = bytearray(game_cpmod)
                struct.pack_into("<i", cp_in, 0xA0, rng.choice((0, 1, 2, 3, 8)))
                struct.pack_into("<I", cp_in, 4, scratch_b)
                emu.write(scratch_e, bytes(cp_in))
                res = emu.call(0x0065E710, [], ecx=scratch_e)
                game_cpmod2 = bytearray(emu.read(scratch_e, 0xA4))
                struct.pack_into("<I", game_cpmod2, 4, 0)
                struct.pack_into("<I", cp_in, 4, 0)
                ours_cprate_hex = cpp.call(f"color_rate {cp_in.hex()} {part_blob.hex()}")
                if res.stack_cleanup != 0 or bytes(game_cpmod2).hex() != ours_cprate_hex:
                    raise AssertionError("0x0065E710 color_rate mismatch")
                bump("0x0065E710")

                # 0x0065E8B0 updateDefaultColorParticle
                cp_upd = bytearray(game_cpmod)
                struct.pack_into("<I", cp_upd, 4, scratch_b)
                emu.write(scratch_e, bytes(cp_upd))
                res = emu.call(0x0065E8B0, [], ecx=scratch_e)
                game_cpmod3 = bytearray(emu.read(scratch_e, 0xA4))
                struct.pack_into("<I", game_cpmod3, 4, 0)
                ours_cpupd_hex = cpp.call(f"color_update {game_cpmod.hex()} {part_blob.hex()}")
                if res.stack_cleanup != 0 or bytes(game_cpmod3).hex() != ours_cpupd_hex:
                    raise AssertionError("0x0065E8B0 color_update mismatch")
                bump("0x0065E8B0")

                # 10-15: WindModule family & getParticleSize
                wind_asset = struct.pack(
                    "<Ifffffffffff",
                    rng.choice((1, 2, 3)),
                    rng.uniform(0.1, 10.0),
                    rng.uniform(1.0, 20.0),
                    rng.uniform(25.0, 100.0),
                    rng.uniform(0.01, 0.2),
                    rng.uniform(0.2, 0.5),
                    rng.uniform(-1.5, -0.2),
                    rng.uniform(-0.2, 0.0),
                    rng.uniform(0.1, 0.5),
                    rng.uniform(0.5, 1.5),
                    rng.choice((0.0, rng.uniform(0.1, 5.0))),
                    rng.uniform(0.1, 3.0),
                )
                emu.write(scratch_a, wind_asset)
                emu.write(scratch_b, bytes(0x40))
                res = emu.call(0x0065FC70, [scratch_a], ecx=scratch_b)
                game_wtmpl = emu.read(scratch_b, 0x40)
                ours_wtmpl_hex = cpp.call(f"wind_tmpl_init {wind_asset.hex()}")
                if res.stack_cleanup != 4 or res.eax != scratch_b or game_wtmpl.hex() != ours_wtmpl_hex:
                    raise AssertionError("0x0065FC70 wind_tmpl_init mismatch")
                bump("0x0065FC70")

                emu.write(scratch_c, bytes(0x40))
                res = emu.call(0x0065FCF0, [scratch_b], ecx=scratch_c)
                game_wtmpl_copy = emu.read(scratch_c, 0x40)
                ours_wtmpl_copy_hex = cpp.call(f"wind_tmpl_copy {game_wtmpl.hex()}")
                if res.stack_cleanup != 4 or res.eax != scratch_c or game_wtmpl_copy.hex() != ours_wtmpl_copy_hex:
                    raise AssertionError("0x0065FCF0 wind_tmpl_copy mismatch")
                bump("0x0065FCF0")

                sys_stub = bytearray(0x130)
                struct.pack_into("<i", sys_stub, 0x04, ref_cnt)
                emu.write(scratch_d, bytes(sys_stub))
                emu.write(scratch_e, struct.pack("<I", scratch_d))
                emu.write(scratch_f, bytes(0x20))
                set_globals(emu, state)
                res = emu.call(0x0066CD60, [scratch_e, scratch_b], ecx=scratch_f)
                game_wmod = bytearray(emu.read(scratch_f, 0x20))
                struct.pack_into("<II", game_wmod, 4, 0, 0)
                game_wref = struct.unpack("<i", emu.read(scratch_d + 4, 4))[0]
                game_state = emu.read(CLIENT_STATE, 24)
                ours_wmod_hex, ours_wref_str, ours_state_hex = cpp.call(
                    f"wind_mod_init {game_wtmpl.hex()} {ref_cnt} {state.hex()}"
                ).split()
                if (
                    res.stack_cleanup != 8
                    or res.eax != scratch_f
                    or bytes(game_wmod).hex() != ours_wmod_hex
                    or game_wref != int(ours_wref_str)
                    or game_state.hex() != ours_state_hex
                ):
                    raise AssertionError("0x0066CD60 wind_mod_init mismatch")
                bump("0x0066CD60")

                # 0x0065FD60 updateWindModuleMotion
                wmod_in = bytearray(game_wmod)
                if case_idx % 3 == 1:
                    struct.pack_into("<f", wmod_in, 0x10, 10.0)  # trigger ping-pong / wrap
                elif case_idx % 3 == 2:
                    wmod_in[0x1C] = 0
                    struct.pack_into("<f", wmod_in, 0x10, -10.0)
                wmod_emu = bytearray(wmod_in)
                struct.pack_into("<I", wmod_emu, 8, scratch_b)
                emu.write(scratch_f, bytes(wmod_emu))
                set_globals(emu, state)
                res = emu.call(0x0065FD60, [], ecx=scratch_f)
                game_wmod2 = bytearray(emu.read(scratch_f, 0x20))
                struct.pack_into("<II", game_wmod2, 4, 0, 0)
                game_state = emu.read(CLIENT_STATE, 24)
                ours_wmod2_hex, ours_state_hex = cpp.call(
                    f"wind_mod_update {wmod_in.hex()} {game_wtmpl.hex()} {state.hex()}"
                ).split()
                if (
                    res.stack_cleanup != 0
                    or bytes(game_wmod2).hex() != ours_wmod2_hex
                    or game_state.hex() != ours_state_hex
                ):
                    raise AssertionError("0x0065FD60 wind_mod_update mismatch")
                bump("0x0065FD60")

                # 0x00660530 getParticleSize (10-byte ST0 check)
                size_kind = case_idx % 3
                size_bits = pos_float(rng, 0.5, 25.0)
                part_sz = bytearray(0x80)
                if size_kind == 1:
                    upd_sz = bytearray(0x30)
                    struct.pack_into("<I", upd_sz, 0, 0x00BF3F9C)
                    struct.pack_into("<I", upd_sz, 0x10, size_bits)
                    emu.write(scratch_c, bytes(upd_sz))
                    struct.pack_into("<I", part_sz, 0x54, scratch_c)
                elif size_kind == 2:
                    upd_sz = bytearray(0x4C)
                    struct.pack_into("<I", upd_sz, 0, 0x00BF4418)
                    struct.pack_into("<I", upd_sz, 0x38, size_bits)
                    emu.write(scratch_c, bytes(upd_sz))
                    struct.pack_into("<I", part_sz, 0x54, scratch_c)
                emu.write(scratch_a, bytes(part_sz))
                res, game_st0 = emu.call_float80(0x00660530, [], ecx=scratch_a, fpu_control_word=cw)
                ours_st0_hex = cpp.call(f"particle_size {size_kind} {size_bits}")
                if res.stack_cleanup != 0 or game_st0.hex() != ours_st0_hex:
                    raise AssertionError(f"0x00660530 particle_size ST0 mismatch: {game_st0.hex()} vs {ours_st0_hex}")
                bump("0x00660530")

                # 0x0066CE40 updateWindParticle
                emu.write(scratch_b, game_wtmpl)
                wmod_emu = bytearray(game_wmod2)
                struct.pack_into("<I", wmod_emu, 8, scratch_b)
                emu.write(scratch_f, bytes(wmod_emu))
                sx, sy, sz = (finite_float(rng, -5.0, 5.0) for _ in range(3))
                sys_stub = bytearray(0x130)
                struct.pack_into("<I", sys_stub, 0x18, scratch_f)
                struct.pack_into("<I", sys_stub, 0xE0, sx)
                struct.pack_into("<I", sys_stub, 0xF0, sy)
                struct.pack_into("<I", sys_stub, 0x100, sz)
                emu.write(scratch_d, bytes(sys_stub))
                emu.write(scratch_e, struct.pack("<II", scratch_d, 1))
                part_wind = bytearray(part_sz)
                for off in (0x10, 0x14, 0x18):
                    struct.pack_into("<I", part_wind, off, finite_float(rng, -15.0, 15.0))
                struct.pack_into("<I", part_wind, 0x34, scratch_e)
                emu.write(scratch_a, bytes(part_wind))
                wp_mod = bytearray(0x0C)
                struct.pack_into("<IIf", wp_mod, 0, 0x00BF31D4, scratch_a, rng.uniform(0.2, 1.5))
                wp_addr = scratch_e + 0x20
                emu.write(wp_addr, bytes(wp_mod))
                res = emu.call(0x0066CE40, [], ecx=wp_addr)
                game_wpos = emu.read(scratch_a + 0x10, 12)
                struct.pack_into("<I", wp_mod, 4, 0)
                struct.pack_into("<I", part_wind, 0x34, 0)
                struct.pack_into("<I", part_wind, 0x54, 0)
                ours_wpos_hex = cpp.call(
                    f"wind_part_update {wp_mod.hex()} {part_wind.hex()} {game_wmod2.hex()} "
                    f"{game_wtmpl.hex()} {sx} {sy} {sz} {size_kind} {size_bits}"
                )
                if res.stack_cleanup != 0 or game_wpos.hex() != ours_wpos_hex:
                    raise AssertionError(f"0x0066CE40 wind_part_update mismatch: {game_wpos.hex()} vs {ours_wpos_hex}")
                bump("0x0066CE40")

                # 16-24: UpdateModuleTemplate, DefaultUpdate, RenderObjectUpdate
                def_upd_asset = bytearray(0x68)
                struct.pack_into("<II", def_upd_asset, 0, 0, rng.randint(0, 3))
                for i in range(8):
                    def_upd_asset[0x08 + i * 0x0C : 0x14 + i * 0x0C] = rand_rv_bytes(rng, -5.0, 5.0)
                emu.write(scratch_a, bytes(def_upd_asset))
                emu.write(scratch_b, bytes(0x68))
                res = emu.call(0x0065EB10, [scratch_a], ecx=scratch_b)
                game_du_tmpl = emu.read(scratch_b, 0x68)
                ours_du_tmpl_hex = cpp.call(f"def_upd_tmpl_init {def_upd_asset.hex()}")
                if res.stack_cleanup != 4 or res.eax != scratch_b or game_du_tmpl.hex() != ours_du_tmpl_hex:
                    raise AssertionError("0x0065EB10 def_upd_tmpl_init mismatch")
                bump("0x0065EB10")

                # 0x0065EC20 copyUpdateModuleTemplateBase
                emu.write(scratch_c, bytes(0x44))
                res = emu.call(0x0065EC20, [scratch_b], ecx=scratch_c)
                game_ub_copy = emu.read(scratch_c, 0x44)
                ours_ub_copy_hex = cpp.call(f"upd_base_copy {game_du_tmpl[:0x44].hex()}")
                if res.stack_cleanup != 4 or res.eax != scratch_c or game_ub_copy.hex() != ours_ub_copy_hex:
                    raise AssertionError("0x0065EC20 upd_base_copy mismatch")
                bump("0x0065EC20")

                # 0x00661B20 copyDefaultUpdateModuleTemplate
                emu.write(scratch_c, bytes(0x68))
                res = emu.call(0x00661B20, [scratch_b], ecx=scratch_c)
                game_du_copy = emu.read(scratch_c, 0x68)
                ours_du_copy_hex = cpp.call(f"def_upd_tmpl_copy {game_du_tmpl.hex()}")
                if res.stack_cleanup != 4 or res.eax != scratch_c or game_du_copy.hex() != ours_du_copy_hex:
                    raise AssertionError("0x00661B20 def_upd_tmpl_copy mismatch")
                bump("0x00661B20")

                # 0x0066A740 initializeDefaultUpdateParticle & 0x0065EDF0 updateDefaultUpdateParticle
                sys_mul = pos_float(rng, 0.5, 3.0)
                sys_bias = finite_float(rng, -2.0, 2.0)
                global_mul = pos_float(rng, 0.5, 2.0)
                sys_rv = rand_rv_bytes(rng, 0.5, 2.0)
                emu.write(mgr_addr + 4, struct.pack("<I", global_mul))
                sys_tmpl = bytearray(0x8C)
                sys_tmpl[0x80:0x8C] = sys_rv
                emu.write(scratch_d, bytes(sys_tmpl))
                sys_stub = bytearray(0x130)
                struct.pack_into("<I", sys_stub, 0x08, scratch_d)
                struct.pack_into("<II", sys_stub, 0x124, sys_mul, sys_bias)
                emu.write(scratch_e, bytes(sys_stub))
                emu.write(scratch_f, struct.pack("<II", scratch_e, 1))
                upd_view_addr = scratch_f + 0x20
                emu.write(upd_view_addr, struct.pack("<III", 0, scratch_f, scratch_b))
                part_addr = scratch_f + 0x40
                emu.write(part_addr, bytes(0x80))
                du_part_addr = scratch_f + 0x100
                emu.write(du_part_addr, bytes(0x30))
                set_globals(emu, state)
                res = emu.call(0x0066A740, [part_addr, upd_view_addr], ecx=du_part_addr)
                game_dup = bytearray(emu.read(du_part_addr, 0x30))
                struct.pack_into("<I", game_dup, 4, 0)
                game_state = emu.read(CLIENT_STATE, 24)
                ours_dup_hex, ours_state_hex = cpp.call(
                    f"def_upd_part_init {game_du_tmpl.hex()} {sys_mul} {sys_bias} "
                    f"{sys_rv.hex()} {global_mul} {state.hex()}"
                ).split()
                if (
                    res.stack_cleanup != 8
                    or res.eax != du_part_addr
                    or bytes(game_dup).hex() != ours_dup_hex
                    or game_state.hex() != ours_state_hex
                ):
                    raise AssertionError("0x0066A740 def_upd_part_init mismatch")
                bump("0x0066A740")

                emu.write(du_part_addr, bytes(game_dup))
                res = emu.call(0x0065EDF0, [], ecx=du_part_addr)
                game_dup2 = emu.read(du_part_addr, 0x30)
                ours_dup2_hex = cpp.call(f"def_upd_part_update {game_dup.hex()}")
                if res.stack_cleanup != 0 or game_dup2.hex() != ours_dup2_hex:
                    raise AssertionError("0x0065EDF0 def_upd_part_update mismatch")
                bump("0x0065EDF0")

                # RenderObjectUpdate (0x0065F530, 0x0065F690, 0x0066BAE0, 0x0065F900)
                ro_upd_asset = bytearray(0xC8)
                struct.pack_into("<II", ro_upd_asset, 0, 0, rng.randint(0, 3))
                for i in range(16):
                    ro_upd_asset[0x08 + i * 0x0C : 0x14 + i * 0x0C] = rand_rv_bytes(rng, -5.0, 5.0)
                emu.write(scratch_a, bytes(ro_upd_asset))
                emu.write(scratch_b, bytes(0xC8))
                res = emu.call(0x0065F530, [scratch_a], ecx=scratch_b)
                game_rou_tmpl = emu.read(scratch_b, 0xC8)
                ours_rou_tmpl_hex = cpp.call(f"ro_upd_tmpl_init {ro_upd_asset.hex()}")
                if res.stack_cleanup != 4 or res.eax != scratch_b or game_rou_tmpl.hex() != ours_rou_tmpl_hex:
                    raise AssertionError("0x0065F530 ro_upd_tmpl_init mismatch")
                bump("0x0065F530")

                emu.write(scratch_c, bytes(0xC8))
                res = emu.call(0x0065F690, [scratch_b], ecx=scratch_c)
                game_rou_copy = emu.read(scratch_c, 0xC8)
                ours_rou_copy_hex = cpp.call(f"ro_upd_tmpl_copy {game_rou_tmpl.hex()}")
                if res.stack_cleanup != 4 or res.eax != scratch_c or game_rou_copy.hex() != ours_rou_copy_hex:
                    raise AssertionError("0x0065F690 ro_upd_tmpl_copy mismatch")
                bump("0x0065F690")

                sys_ymul = pos_float(rng, 0.5, 3.0)
                struct.pack_into("<I", sys_stub, 0x12C, sys_ymul)
                emu.write(scratch_e, bytes(sys_stub))
                emu.write(upd_view_addr, struct.pack("<III", 0, scratch_f, scratch_b))
                ro_part_addr = scratch_f + 0x140
                emu.write(ro_part_addr, bytes(0x4C))
                set_globals(emu, state)
                res = emu.call(0x0066BAE0, [part_addr, upd_view_addr], ecx=ro_part_addr)
                game_rop = bytearray(emu.read(ro_part_addr, 0x4C))
                struct.pack_into("<I", game_rop, 4, 0)
                game_state = emu.read(CLIENT_STATE, 24)
                ours_rop_hex, ours_state_hex = cpp.call(
                    f"ro_upd_part_init {game_rou_tmpl.hex()} {sys_ymul} {sys_bias} {sys_rv.hex()} {state.hex()}"
                ).split()
                if (
                    res.stack_cleanup != 8
                    or res.eax != ro_part_addr
                    or bytes(game_rop).hex() != ours_rop_hex
                    or game_state.hex() != ours_state_hex
                ):
                    raise AssertionError("0x0066BAE0 ro_upd_part_init mismatch")
                bump("0x0066BAE0")

                emu.write(ro_part_addr, bytes(game_rop))
                res = emu.call(0x0065F900, [], ecx=ro_part_addr)
                game_rop2 = emu.read(ro_part_addr, 0x4C)
                ours_rop2_hex = cpp.call(f"ro_upd_part_update {game_rop.hex()}")
                if res.stack_cleanup != 0 or game_rop2.hex() != ours_rop2_hex:
                    raise AssertionError("0x0065F900 ro_upd_part_update mismatch")
                bump("0x0065F900")

                # 25: 0x00666D70 SwarmPhysicsParticleModule::updatePhysics
                swarm_part = bytearray(0x80)
                for off in (0x04, 0x08, 0x0C, 0x10, 0x14, 0x18):
                    struct.pack_into("<I", swarm_part, off, finite_float(rng, -20.0, 20.0))
                sx_s = finite_float(rng, -50.0, 50.0)
                sy_s = finite_float(rng, -50.0, 50.0)
                sz_s = finite_float(rng, -50.0, 50.0)
                swarm_sys = bytearray(0x130)
                struct.pack_into("<I", swarm_sys, 0xE0, sx_s)
                struct.pack_into("<I", swarm_sys, 0xF0, sy_s)
                struct.pack_into("<I", swarm_sys, 0x100, sz_s)
                emu.write(scratch_d, bytes(swarm_sys))
                emu.write(scratch_e, struct.pack("<II", scratch_d, 1))
                struct.pack_into("<I", swarm_part, 0x34, scratch_e)
                emu.write(scratch_a, bytes(swarm_part))
                swarm_mod = bytearray(0x0C)
                struct.pack_into("<IIf", swarm_mod, 0, 0x00BF3154, scratch_a, rng.uniform(0.1, 20.0))
                emu.write(scratch_b, bytes(swarm_mod))
                res = emu.call(0x00666D70, [], ecx=scratch_b)
                game_svelpos = emu.read(scratch_a + 4, 24)
                struct.pack_into("<I", swarm_mod, 4, 0)
                struct.pack_into("<I", swarm_part, 0x34, 0)
                ours_svelpos_hex = cpp.call(f"swarm_update {swarm_mod.hex()} {swarm_part.hex()} {sx_s} {sy_s} {sz_s}")
                if res.stack_cleanup != 0 or game_svelpos.hex() != ours_svelpos_hex:
                    raise AssertionError("0x00666D70 swarm_update mismatch")
                bump("0x00666D70")

                # 26-28: WavefrontEmissionVelocity (0x0065F100, 0x0065F160, 0x0065F1E0)
                wave_asset = bytearray(0x44)
                struct.pack_into("<II", wave_asset, 0, 0, rng.randint(0, 7))
                for i in range(5):
                    wave_asset[0x08 + i * 0x0C : 0x14 + i * 0x0C] = rand_rv_bytes(rng, -10.0, 10.0)
                emu.write(scratch_a, bytes(wave_asset))
                emu.write(scratch_b, bytes(0x48))
                res = emu.call(0x0065F100, [scratch_a], ecx=scratch_b)
                game_wt = emu.read(scratch_b, 0x48)
                ours_wt_hex = cpp.call(f"wave_tmpl_init {wave_asset.hex()}")
                if res.stack_cleanup != 4 or res.eax != scratch_b or game_wt.hex() != ours_wt_hex:
                    raise AssertionError("0x0065F100 wave_tmpl_init mismatch")
                bump("0x0065F100")

                emu.write(scratch_c, bytes(0x48))
                res = emu.call(0x0065F160, [scratch_b], ecx=scratch_c)
                game_wtc = emu.read(scratch_c, 0x48)
                ours_wtc_hex = cpp.call(f"wave_tmpl_copy {game_wt.hex()}")
                if res.stack_cleanup != 4 or res.eax != scratch_c or game_wtc.hex() != ours_wtc_hex:
                    raise AssertionError("0x0065F160 wave_tmpl_copy mismatch")
                bump("0x0065F160")

                emu.write(scratch_d, struct.pack("<III", 0, 0, scratch_b))
                emu.write(scratch_e, bytes(16))
                set_globals(emu, state)
                res = emu.call(
                    0x0065F1E0,
                    [scratch_e, scratch_e + 4, scratch_e + 8, scratch_e + 12],
                    ecx=scratch_d,
                )
                game_wsamp = emu.read(scratch_e, 16)
                game_state = emu.read(CLIENT_STATE, 24)
                ours_wsamp_hex, ours_state_hex = cpp.call(
                    f"wave_sample {game_wt.hex()} {state.hex()}"
                ).split()
                if (
                    res.stack_cleanup != 16
                    or game_wsamp.hex() != ours_wsamp_hex
                    or game_state.hex() != ours_state_hex
                ):
                    raise AssertionError("0x0065F1E0 wave_sample mismatch")
                bump("0x0065F1E0")

                # 29-31: TerrainFireEmission (0x00662A00, 0x00662990, 0x00662A60)
                tfire_asset = bytearray(0x30)
                tfire_asset[4] = rng.choice((0, 1))
                struct.pack_into("<I", tfire_asset, 8, finite_float(rng, 0.0, 20.0))
                for i in range(3):
                    tfire_asset[0x0C + i * 0x0C : 0x18 + i * 0x0C] = rand_rv_bytes(rng, -15.0, 15.0)
                emu.write(scratch_a, bytes(tfire_asset))
                emu.write(scratch_b, bytes(0x34))
                res = emu.call(0x00662A00, [scratch_a], ecx=scratch_b)
                game_tft = emu.read(scratch_b, 0x34)
                ours_tft_hex = cpp.call(f"tfire_tmpl_init {tfire_asset.hex()}")
                if res.stack_cleanup != 4 or res.eax != scratch_b or game_tft.hex() != ours_tft_hex:
                    raise AssertionError("0x00662A00 tfire_tmpl_init mismatch")
                bump("0x00662A00")

                emu.write(scratch_c, bytes(0x34))
                res = emu.call(0x00662990, [scratch_b], ecx=scratch_c)
                game_tftc = emu.read(scratch_c, 0x34)
                ours_tftc_hex = cpp.call(f"tfire_tmpl_copy {game_tft.hex()}")
                if res.stack_cleanup != 4 or res.eax != scratch_c or game_tftc.hex() != ours_tftc_hex:
                    raise AssertionError("0x00662990 tfire_tmpl_copy mismatch")
                bump("0x00662990")

                emu.write(scratch_d, struct.pack("<III", 0x00BF4BC0, 0, scratch_b))
                emu.write(scratch_e, bytes(12))
                set_globals(emu, state)
                res = emu.call(0x00662A60, [scratch_e, 0, 0, 0, 0], ecx=scratch_d)
                game_tfpt = emu.read(scratch_e, 12)
                game_state = emu.read(CLIENT_STATE, 24)
                ours_tfpt_hex, ours_state_hex = cpp.call(
                    f"tfire_sample {game_tft.hex()} {state.hex()}"
                ).split()
                if (
                    res.stack_cleanup != 20
                    or game_tfpt.hex() != ours_tfpt_hex
                    or game_state.hex() != ours_state_hex
                ):
                    raise AssertionError("0x00662A60 tfire_sample mismatch")
                bump("0x00662A60")

                # 32: 0x0065EF80 OrthoEmissionVelocityModuleTemplate::copyFrom
                ortho_src = bytearray(0x2C)
                struct.pack_into("<II", ortho_src, 0, 0x00BF2CFC, 1)
                for i in range(3):
                    ortho_src[0x08 + i * 0x0C : 0x14 + i * 0x0C] = rand_rv_bytes(rng, -10.0, 10.0)
                emu.write(scratch_a, bytes(ortho_src))
                emu.write(scratch_b, bytes(0x2C))
                res = emu.call(0x0065EF80, [scratch_a], ecx=scratch_b)
                game_ortho = emu.read(scratch_b, 0x2C)
                ours_ortho_hex = cpp.call(f"ortho_tmpl_copy {ortho_src.hex()}")
                if res.stack_cleanup != 4 or res.eax != scratch_b or game_ortho.hex() != ours_ortho_hex:
                    raise AssertionError("0x0065EF80 ortho_tmpl_copy mismatch")
                bump("0x0065EF80")

                # 33-38: OutwardEmissionVelocity & Cylinder/Line/Spline/Radial volume outward velocities
                rate_bits = finite_float(rng, -2.0, 2.0)
                off_bits = finite_float(rng, -0.5, 2.0)
                cyl_tmpl_blob = bytearray(0x24)
                struct.pack_into("<I", cyl_tmpl_blob, 0x10, rate_bits)
                emu.write(scratch_a, bytes(cyl_tmpl_blob))
                emu.write(scratch_b, struct.pack("<IIII", 0x00BF49DC, 0, scratch_a, off_bits))
                res = emu.call(0x0065EF50, [], ecx=scratch_b)
                game_rad_off = emu.read(scratch_b + 0x0C, 4)
                ours_rad_off_hex = cpp.call(f"cyl_step_radius {rate_bits} {off_bits}")
                if res.stack_cleanup != 0 or game_rad_off.hex() != ours_rad_off_hex:
                    raise AssertionError("0x0065EF50 cyl_step_radius mismatch")
                bump("0x0065EF50")

                pos_vec = struct.pack(
                    "<III",
                    pos_float(rng, 0.5, 10.0),
                    pos_float(rng, 0.5, 10.0),
                    pos_float(rng, 0.5, 10.0),
                )
                norm_vec = struct.pack("<fff", 0.0, 0.0, 1.0)
                rad_spd = finite_float(rng, -10.0, 10.0)
                norm_spd = finite_float(rng, -10.0, 10.0)

                # 0x00665420 computeCylinderOutwardVelocity
                cyl_tmpl_blob[0x18:0x24] = norm_vec
                emu.write(scratch_a, bytes(cyl_tmpl_blob))
                emu.write(scratch_b, struct.pack("<IIII", 0x00BF49DC, 0, scratch_a, bits(1.0)))
                emu.write(scratch_c, pos_vec)
                emu.write(scratch_d, bytes(12))
                res = emu.call(0x00665420, [scratch_d, scratch_c, rad_spd, norm_spd], ecx=scratch_b)
                game_cvel = emu.read(scratch_d, 12)
                ours_cvel_hex = cpp.call(f"cyl_outward {norm_vec.hex()} {pos_vec.hex()} {rad_spd} {norm_spd}")
                if res.stack_cleanup != 16 or game_cvel.hex() != ours_cvel_hex:
                    raise AssertionError("0x00665420 cyl_outward mismatch")
                bump("0x00665420")

                # 0x00665540 computeLineOutwardVelocity
                if case_idx % 3 == 0:
                    pts_vec = struct.pack("<ffffff", 0.0, 0.0, 0.0, 0.0, 0.0, 5.0)
                else:
                    pts_vec = struct.pack(
                        "<ffffff",
                        rng.uniform(-5.0, -1.0),
                        rng.uniform(-5.0, -1.0),
                        rng.uniform(-5.0, -1.0),
                        rng.uniform(1.0, 5.0),
                        rng.uniform(1.0, 5.0),
                        rng.uniform(1.0, 5.0),
                    )
                line_tmpl_blob = bytearray(0x24)
                line_tmpl_blob[0x0C:0x24] = pts_vec
                emu.write(scratch_a, bytes(line_tmpl_blob))
                emu.write(scratch_b, struct.pack("<III", 0x00BF4A2C, 0, scratch_a))
                emu.write(scratch_d, bytes(12))
                res = emu.call(0x00665540, [scratch_d, scratch_c, rad_spd, norm_spd], ecx=scratch_b)
                game_lvel = emu.read(scratch_d, 12)
                ours_lvel_hex = cpp.call(f"line_outward {pts_vec.hex()} {pos_vec.hex()} {rad_spd} {norm_spd}")
                if res.stack_cleanup != 16 or game_lvel.hex() != ours_lvel_hex:
                    raise AssertionError("0x00665540 line_outward mismatch")
                bump("0x00665540")

                # 0x006657D0 computeSplineOutwardVelocity
                tan_vec = struct.pack(
                    "<III",
                    pos_float(rng, 0.5, 5.0),
                    pos_float(rng, 0.5, 5.0),
                    pos_float(rng, 0.5, 5.0),
                )
                spline_mod_blob = bytearray(0x2C)
                struct.pack_into("<I", spline_mod_blob, 0, 0x00BF4CC4)
                spline_mod_blob[0x20:0x2C] = tan_vec
                emu.write(scratch_b, bytes(spline_mod_blob))
                emu.write(scratch_d, bytes(12))
                res = emu.call(0x006657D0, [scratch_d, scratch_c, rad_spd, norm_spd], ecx=scratch_b)
                game_spvel = emu.read(scratch_d, 12)
                ours_spvel_hex = cpp.call(f"spline_outward {tan_vec.hex()} {pos_vec.hex()} {rad_spd} {norm_spd}")
                if res.stack_cleanup != 16 or res.eax != scratch_d or game_spvel.hex() != ours_spvel_hex:
                    raise AssertionError("0x006657D0 spline_outward mismatch")
                bump("0x006657D0")

                # 0x00665B60 computeRadialOutwardVelocity
                rad_pos = struct.pack("<fff", 0.0, 0.0, 0.0) if case_idx == 0 else pos_vec
                emu.write(scratch_b, struct.pack("<III", 0x00BF4954, 0, 0))
                emu.write(scratch_c, rad_pos)
                emu.write(scratch_d, bytes(12))
                res = emu.call(0x00665B60, [scratch_d, scratch_c, rad_spd, norm_spd], ecx=scratch_b)
                game_rvel = emu.read(scratch_d, 12)
                ours_rvel_hex = cpp.call(f"radial_outward {rad_pos.hex()} {rad_spd} {norm_spd}")
                if res.stack_cleanup != 16 or res.eax != scratch_d or game_rvel.hex() != ours_rvel_hex:
                    raise AssertionError("0x00665B60 radial_outward mismatch")
                bump("0x00665B60")

                # 0x0065F040 OutwardEmissionVelocityModule::getVelocity
                outward_tmpl = bytearray(0x20)
                struct.pack_into("<II", outward_tmpl, 0, 0x00BF2CE4, 2)
                outward_tmpl[0x08:0x14] = rand_rv_bytes(rng, -5.0, 5.0)
                outward_tmpl[0x14:0x20] = rand_rv_bytes(rng, -5.0, 5.0)
                emu.write(scratch_a, bytes(outward_tmpl))
                emu.write(scratch_e, struct.pack("<III", 0x00BF4AB0, 0, scratch_a))
                vol_vt = (0x00BF49DC, 0x00BF4A2C, 0x00BF4CC4, 0x00BF4954)[case_idx % 4]
                if vol_vt == 0x00BF49DC:
                    emu.write(scratch_f, bytes(cyl_tmpl_blob))
                    emu.write(scratch_b, struct.pack("<IIII", vol_vt, 0, scratch_f, bits(1.0)))
                    vol_data_hex = norm_vec.hex()
                elif vol_vt == 0x00BF4A2C:
                    emu.write(scratch_f, bytes(line_tmpl_blob))
                    emu.write(scratch_b, struct.pack("<III", vol_vt, 0, scratch_f))
                    vol_data_hex = pts_vec.hex()
                elif vol_vt == 0x00BF4CC4:
                    emu.write(scratch_b, bytes(spline_mod_blob))
                    vol_data_hex = tan_vec.hex()
                else:
                    emu.write(scratch_b, struct.pack("<III", vol_vt, 0, 0))
                    vol_data_hex = "00"
                emu.write(scratch_c, pos_vec)
                emu.write(scratch_d, bytes(12))
                set_globals(emu, state)
                res = emu.call(0x0065F040, [scratch_d, scratch_c, scratch_b], ecx=scratch_e)
                game_ovel = emu.read(scratch_d, 12)
                game_state = emu.read(CLIENT_STATE, 24)
                ours_ovel_hex, ours_state_hex = cpp.call(
                    f"outward_vel {outward_tmpl.hex()} {pos_vec.hex()} {vol_vt} {vol_data_hex} {state.hex()}"
                ).split()
                if (
                    res.stack_cleanup != 12
                    or res.eax != scratch_d
                    or game_ovel.hex() != ours_ovel_hex
                    or game_state.hex() != ours_state_hex
                ):
                    raise AssertionError("0x0065F040 outward_vel mismatch")
                bump("0x0065F040")

                # 39-43: LightningDrawModule family (0x00662CA0, 0x00662E10, 0x00662F50, 0x0066AF10, 0x00662B60)
                gar_blob = bytes(rng.randrange(256) for _ in range(0xAC))
                emu.write(scratch_a, gar_blob)
                res = emu.call(0x00662F50, [], ecx=scratch_a)
                game_lgt_def = emu.read(scratch_a, 0xAC)
                ours_lgt_def_hex = cpp.call(f"lgt_tmpl_default {gar_blob.hex()}")
                if res.stack_cleanup != 0 or game_lgt_def.hex() != ours_lgt_def_hex:
                    raise AssertionError("0x00662F50 lgt_tmpl_default mismatch")
                bump("0x00662F50")

                lgt_hdr = struct.pack("<I6I", 0, *(finite_float(rng, -10.0, 10.0) for _ in range(6)))
                lgt_pairs = b"".join(rand_rv_bytes(rng, -5.0, 5.0) for _ in range(12))
                emu.write(scratch_c, lgt_pairs)
                emu.write(scratch_a, lgt_hdr + struct.pack("<II", 0, scratch_c))
                emu.write(scratch_b, bytes(0xAC))
                res = emu.call(0x00662CA0, [scratch_a], ecx=scratch_b)
                game_lgt_tmpl = emu.read(scratch_b, 0xAC)
                ours_lgt_tmpl_hex = cpp.call(f"lgt_tmpl_init {lgt_hdr.hex()} {lgt_pairs.hex()}")
                if res.stack_cleanup != 4 or res.eax != scratch_b or game_lgt_tmpl.hex() != ours_lgt_tmpl_hex:
                    raise AssertionError("0x00662CA0 lgt_tmpl_init mismatch")
                bump("0x00662CA0")

                emu.write(scratch_d, bytes(0xAC))
                res = emu.call(0x00662E10, [scratch_b], ecx=scratch_d)
                game_lgt_copy = emu.read(scratch_d, 0xAC)
                ours_lgt_copy_hex = cpp.call(f"lgt_tmpl_copy {game_lgt_tmpl.hex()}")
                if res.stack_cleanup != 4 or res.eax != scratch_d or game_lgt_copy.hex() != ours_lgt_copy_hex:
                    raise AssertionError("0x00662E10 lgt_tmpl_copy mismatch")
                bump("0x00662E10")

                lgt_shader = rng.choice((0, 1, 2, 3, 4, 5, 6, 7, 8))
                lgt_sys_tmpl = bytearray(0x8C)
                struct.pack_into("<I", lgt_sys_tmpl, 0x4C, lgt_shader)
                emu.write(scratch_a, bytes(lgt_sys_tmpl))
                lgt_real_sys = bytearray(0x20)
                struct.pack_into("<I", lgt_real_sys, 0x08, scratch_a)
                emu.write(scratch_d, bytes(lgt_real_sys))
                sys_ref = struct.pack("<Ii", scratch_d, ref_cnt)
                emu.write(scratch_c, sys_ref)
                emu.write(scratch_e, struct.pack("<I", scratch_c))
                emu.write(scratch_f, bytes(0x40))
                set_globals(emu, state)
                res = emu.call(0x0066AF10, [scratch_e, scratch_b], ecx=scratch_f)
                game_lgt_mod = bytearray(emu.read(scratch_f, 0x40))
                struct.pack_into("<II", game_lgt_mod, 4, 0, 0)
                game_lref = struct.unpack("<i", emu.read(scratch_c + 4, 4))[0]
                game_state = emu.read(CLIENT_STATE, 24)
                ours_lgt_mod_hex, ours_lref_str, ours_state_hex = cpp.call(
                    f"lgt_mod_init {game_lgt_tmpl.hex()} {ref_cnt} {lgt_shader} {state.hex()}"
                ).split()
                if (
                    res.stack_cleanup != 8
                    or res.eax != scratch_f
                    or bytes(game_lgt_mod).hex() != ours_lgt_mod_hex
                    or game_lref != int(ours_lref_str)
                    or game_state.hex() != ours_state_hex
                ):
                    raise AssertionError("0x0066AF10 lgt_mod_init mismatch")
                bump("0x0066AF10")

                seg_cnt = rng.choice((0, 1, 3, 5))
                lgt_packer = bytearray(0x28)
                struct.pack_into("<i", lgt_packer, 0x24, seg_cnt)
                part_lgt = bytearray(0x80)
                for off in (0x04, 0x08, 0x0C, 0x10, 0x14, 0x18, 0x44, 0x48):
                    struct.pack_into("<I", part_lgt, off, finite_float(rng, -20.0, 200.0))
                struct.pack_into("<I", part_lgt, 0x28, init_life)
                struct.pack_into("<I", part_lgt, 0x38, rem_life)
                emu.write(scratch_a, bytes(lgt_packer))
                emu.write(scratch_b, bytes(part_lgt))
                n_verts = max(seg_cnt, 1)
                emu.write(scratch_c, bytes(n_verts * 0x28))
                res = emu.call(0x00662B60, [scratch_b, scratch_c], ecx=scratch_a)
                game_verts = emu.read(scratch_c, n_verts * 0x28)
                ours_verts_hex = cpp.call(f"lgt_pack_verts {seg_cnt} {part_lgt.hex()}")
                if res.stack_cleanup != 8 or game_verts.hex() != ours_verts_hex:
                    raise AssertionError("0x00662B60 lgt_pack_verts mismatch")
                bump("0x00662B60")

                # 44: 0x0065F460 RenderObjectDrawModuleTemplate::copyFrom
                rod_src = bytearray(0x40)
                struct.pack_into("<I", rod_src, 0, 0x00BF30E0)
                rod_src[4] = rng.choice((0, 1))
                struct.pack_into("<I", rod_src, 8, finite_float(rng, -5.0, 5.0))
                rod_src[0x0C] = rng.choice((0, 1))
                for base_off in (0x10, 0x20, 0x30):
                    struct.pack_into(
                        "<IIII",
                        rod_src,
                        base_off,
                        rng.randrange(1000),
                        rng.randrange(1000),
                        rng.randrange(1000),
                        finite_float(rng, 0.1, 10.0),
                    )
                emu.write(scratch_a, bytes(rod_src))
                emu.write(scratch_b, bytes(0x40))
                res = emu.call(0x0065F460, [scratch_a], ecx=scratch_b)
                game_rod = emu.read(scratch_b, 0x40)
                ours_rod_hex = cpp.call(f"ro_draw_tmpl_copy {rod_src.hex()}")
                if res.stack_cleanup != 4 or res.eax != scratch_b or game_rod.hex() != ours_rod_hex:
                    raise AssertionError("0x0065F460 ro_draw_tmpl_copy mismatch")
                bump("0x0065F460")

                # 45: 0x0066AE60 LifeEventModule::initialize
                life_rv = rand_rv_bytes(rng, 1.0, 100.0)
                life_tmpl = bytearray(0x20)
                life_tmpl[0x14:0x20] = life_rv
                sys_stub = bytearray(0x130)
                struct.pack_into("<i", sys_stub, 0x04, ref_cnt)
                emu.write(scratch_a, bytes(life_tmpl))
                emu.write(scratch_b, bytes(sys_stub))
                emu.write(scratch_c, struct.pack("<I", scratch_b))
                emu.write(scratch_d, bytes(0x14))
                set_globals(emu, state)
                res = emu.call(0x0066AE60, [scratch_c, scratch_a], ecx=scratch_d)
                game_life = bytearray(emu.read(scratch_d, 0x14))
                struct.pack_into("<II", game_life, 4, 0, 0)
                game_lref = struct.unpack("<i", emu.read(scratch_b + 4, 4))[0]
                game_state = emu.read(CLIENT_STATE, 24)
                ours_life_hex, ours_lref_str, ours_state_hex = cpp.call(
                    f"life_evt_init {life_rv.hex()} {ref_cnt} {state.hex()}"
                ).split()
                if (
                    res.stack_cleanup != 8
                    or res.eax != scratch_d
                    or bytes(game_life).hex() != ours_life_hex
                    or game_lref != int(ours_lref_str)
                    or game_state.hex() != ours_state_hex
                ):
                    raise AssertionError("0x0066AE60 life_evt_init mismatch")
                bump("0x0066AE60")

                # 46-47: 0x006699D0 FXParticleBase::initialize & 0x0066ABF0 FXParticle::initialize
                spawn_blob = bytearray(0x30)
                for off in range(0, 0x24, 4):
                    struct.pack_into("<I", spawn_blob, off, finite_float(rng, -20.0, 20.0))
                struct.pack_into("<II", spawn_blob, 0x24, init_life, rng.randint(1, 5000))
                spawn_blob[0x2C] = rng.choice((0, 1))
                act_cnt = rng.randint(0, 500)
                emu.write(mgr_addr + 0x78, struct.pack("<I", act_cnt))
                emu.write(scratch_a, bytes(spawn_blob))
                emu.write(scratch_b, struct.pack("<Ii", 0, ref_cnt))
                emu.write(scratch_c, struct.pack("<I", scratch_b))
                emu.write(scratch_d, bytes(0x80))
                res = emu.call(0x006699D0, [scratch_c, scratch_a], ecx=scratch_d)
                game_pbase = bytearray(emu.read(scratch_d, 0x80))
                struct.pack_into("<I", game_pbase, 0x34, 0)
                game_pref = struct.unpack("<i", emu.read(scratch_b + 4, 4))[0]
                game_act = struct.unpack("<I", emu.read(mgr_addr + 0x78, 4))[0]
                ours_pbase_hex, ours_pref_str, ours_act_str = cpp.call(
                    f"part_base_init {spawn_blob.hex()} {ref_cnt} {act_cnt}"
                ).split()
                if (
                    res.stack_cleanup != 8
                    or bytes(game_pbase).hex() != ours_pbase_hex
                    or game_pref != int(ours_pref_str)
                    or game_act != int(ours_act_str)
                ):
                    raise AssertionError("0x006699D0 part_base_init mismatch")
                bump("0x006699D0")

                c_frame = rng.randint(100, 10000)
                f_dur = pos_float(rng, 0.01, 0.1)
                emu.write(CLIENT_FRAME_GLOBAL, struct.pack("<I", c_frame))
                emu.write(FRAME_DUR_GLOBAL, struct.pack("<I", f_dur))
                emu.write(mgr_addr + 0x78, struct.pack("<I", act_cnt))
                emu.write(scratch_b, struct.pack("<Ii", 0, ref_cnt))
                emu.write(scratch_d, b"\xCC" * 0x80)
                set_globals(emu, state)
                res = emu.call(0x0066ABF0, [scratch_c, scratch_a], ecx=scratch_d)
                game_pfull = bytearray(emu.read(scratch_d, 0x80))
                struct.pack_into("<I", game_pfull, 0x34, 0)
                game_pref = struct.unpack("<i", emu.read(scratch_b + 4, 4))[0]
                game_act = struct.unpack("<I", emu.read(mgr_addr + 0x78, 4))[0]
                game_state = emu.read(CLIENT_STATE, 24)
                ours_pfull_hex, ours_pref_str, ours_act_str, ours_state_hex = cpp.call(
                    f"part_full_init {spawn_blob.hex()} {ref_cnt} {act_cnt} {c_frame} {f_dur} {state.hex()}"
                ).split()
                if (
                    res.stack_cleanup != 8
                    or res.eax != scratch_d
                    or bytes(game_pfull).hex() != ours_pfull_hex
                    or game_pref != int(ours_pref_str)
                    or game_act != int(ours_act_str)
                    or game_state.hex() != ours_state_hex
                ):
                    raise AssertionError("0x0066ABF0 part_full_init mismatch")
                bump("0x0066ABF0")

                # 48-53: Spline & Matrix34/33 helpers
                t_bits = finite_float(rng, 0.0, 1.0)
                vecs4 = struct.pack("<12I", *(finite_float(rng, -20.0, 20.0) for _ in range(12)))
                emu.write(scratch_a, vecs4)
                emu.write(scratch_b, bytes(12))
                res = call_with_regs(
                    emu,
                    0x00661030,
                    [scratch_b, t_bits],
                    ecx=scratch_a + 36,
                    edx=scratch_a + 12,
                    esi=scratch_a + 24,
                    edi=scratch_a,
                )
                game_herm = emu.read(scratch_b, 12)
                ours_herm_hex = cpp.call(f"hermite {t_bits} {vecs4.hex()}")
                if res.stack_cleanup != 0 or game_herm.hex() != ours_herm_hex:
                    raise AssertionError("0x00661030 hermite mismatch")
                bump("0x00661030")

                mat_a = struct.pack("<12I", *(finite_float(rng, -10.0, 10.0) for _ in range(12)))
                mat_b = struct.pack("<12I", *(finite_float(rng, -10.0, 10.0) for _ in range(12)))
                pt_in = struct.pack("<3I", *(finite_float(rng, -20.0, 20.0) for _ in range(3)))
                emu.write(scratch_a, mat_a)
                emu.write(scratch_b, pt_in)
                emu.write(scratch_c, bytes(12))
                res = emu.call(0x00664FA0, [scratch_a, scratch_b, scratch_c])
                game_invpt = emu.read(scratch_c, 12)
                ours_invpt_hex = cpp.call(f"inv_mat34 {mat_a.hex()} {pt_in.hex()}")
                if res.stack_cleanup != 0 or game_invpt.hex() != ours_invpt_hex:
                    raise AssertionError("0x00664FA0 inv_mat34 mismatch")
                bump("0x00664FA0")

                emu.write(scratch_b, mat_b)
                emu.write(scratch_c, bytes(48))
                res = emu.call(0x00660D50, [scratch_a, scratch_b], ecx=scratch_c)
                game_mul34 = emu.read(scratch_c, 48)
                ours_mul34_hex = cpp.call(f"mul_mat34 {mat_a.hex()} {mat_b.hex()}")
                if res.stack_cleanup != 8 or game_mul34.hex() != ours_mul34_hex:
                    raise AssertionError("0x00660D50 mul_mat34 mismatch")
                bump("0x00660D50")

                emu.write(scratch_c, bytes(36))
                res = emu.call(0x00660F80, [scratch_c, scratch_a])
                game_ext33 = emu.read(scratch_c, 36)
                ours_ext33_hex = cpp.call(f"ext_rot33 {mat_a.hex()}")
                if res.stack_cleanup != 0 or game_ext33.hex() != ours_ext33_hex:
                    raise AssertionError("0x00660F80 ext_rot33 mismatch")
                bump("0x00660F80")

                euler_in = struct.pack("<3I", *(finite_float(rng, -3.14, 3.14) for _ in range(3)))
                emu.write(scratch_b, euler_in)
                emu.write(scratch_c, bytes(36))
                res = emu.call(0x00669060, [scratch_c, scratch_b])
                game_eul33 = emu.read(scratch_c, 36)
                ours_eul33_hex = cpp.call(f"euler_mat33 {euler_in.hex()}")
                if res.stack_cleanup != 0 or game_eul33.hex() != ours_eul33_hex:
                    raise AssertionError("0x00669060 euler_mat33 mismatch")
                bump("0x00669060")

                emu.write(scratch_d, bytes(36))
                res = emu.call(0x00669150, [scratch_d, scratch_c])
                game_tr33 = emu.read(scratch_d, 36)
                ours_tr33_hex = cpp.call(f"trans_mat33 {game_eul33.hex()}")
                if res.stack_cleanup != 0 or game_tr33.hex() != ours_tr33_hex:
                    raise AssertionError("0x00669150 trans_mat33 mismatch")
                bump("0x00669150")

        total_checks = sum(totals.values())
        print(
            f"verified {len(totals)} clean particle65 functions across {total_checks} differential checks: 0 mismatches"
        )
    finally:
        cpp.close()


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "--exe",
        type=Path,
        default=Path("Ra3Source/build/win32/bin/Release/ra3_particles65check.exe"),
    )
    parser.add_argument("--cases", type=int, default=12)
    args = parser.parse_args()
    verify(args.exe, args.cases)


if __name__ == "__main__":
    main()
