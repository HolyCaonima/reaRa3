"""Differentially verify the render56 W3D terrain & environment cluster against ra3_1.0.game."""

from __future__ import annotations

import argparse
import struct
import subprocess
import sys
from pathlib import Path
from typing import Callable

from unicorn import x86_const

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from emulate import CallResult, Emulator  # noqa: E402

ACCEPTED_ENTRIES = (
    0x005612F0, 0x00561340, 0x005615A0, 0x00561D70,
    0x00561DB0, 0x00561E10, 0x00561E70, 0x00561E80,
    0x00562BB0, 0x00562D30, 0x00562DF0, 0x00562F10,
    0x00563520, 0x005635F0, 0x005636E0, 0x00563980,
    0x005642B0, 0x00564300, 0x00564E70, 0x00564F20,
    0x00564F80, 0x00565130, 0x00565250, 0x00565700,
    0x00568EA0, 0x00569480, 0x00569570, 0x005695E0,
    0x00569750, 0x0056A8A0, 0x0056AB00, 0x0056B290,
    0x0056B2F0, 0x0056B380, 0x0056D970, 0x0056D9D0,
    0x0056DA40, 0x0056DAB0, 0x0056DB20, 0x0056DB80,
    0x0056DC30, 0x0056DD70, 0x0056DE70, 0x0056DFB0,
    0x0056E0A0, 0x0056EC10, 0x0056ECC0, 0x0056EDB0,
    0x00570D50, 0x00572070, 0x0057A140, 0x0057DB70,
    0x0057E4E0, 0x00580690, 0x005882C0, 0x0058D1B0,
)


class CppOracle:
    def __init__(self, executable: Path) -> None:
        self.process = subprocess.Popen(
            [str(executable)], stdin=subprocess.PIPE, stdout=subprocess.PIPE,
            text=True, bufsize=1)

    def call(
        self, address: int, blob: bytes, fixups: list[int], arguments: list[int]
    ) -> tuple[int, bytes]:
        assert self.process.stdin is not None and self.process.stdout is not None
        fixup_text = ",".join(f"{offset:x}" for offset in fixups) or "."
        line = " ".join([
            f"{address:08x}", blob.hex(), fixup_text,
            *(f"{value & 0xFFFFFFFF:x}" for value in arguments),
        ])
        self.process.stdin.write(line + "\n")
        self.process.stdin.flush()
        output = self.process.stdout.readline().strip()
        if not output or output == "error":
            raise RuntimeError(f"render56check rejected 0x{address:08X}")
        result, encoded = output.split()
        return int(result, 16), bytes.fromhex(encoded)

    def set_control_word(self, value: int) -> None:
        assert self.process.stdin is not None and self.process.stdout is not None
        self.process.stdin.write(f"fpcw {value:04x}\n")
        self.process.stdin.flush()
        if self.process.stdout.readline().strip() != "ok":
            raise RuntimeError("render56check rejected FPU control word")

    def close(self) -> None:
        assert self.process.stdin is not None
        self.process.stdin.write("quit\n")
        self.process.stdin.flush()
        if self.process.wait(timeout=10) != 0:
            raise RuntimeError("render56check failed")


def u32(data: bytearray | bytes, offset: int) -> int:
    return struct.unpack_from("<I", data, offset)[0]


def i32(data: bytearray | bytes, offset: int) -> int:
    return struct.unpack_from("<i", data, offset)[0]


def put_u32(data: bytearray, offset: int, value: int) -> None:
    struct.pack_into("<I", data, offset, value & 0xFFFFFFFF)


def put_i32(data: bytearray, offset: int, value: int) -> None:
    struct.pack_into("<i", data, offset, value)


def put_u16(data: bytearray, offset: int, value: int) -> None:
    struct.pack_into("<H", data, offset, value & 0xFFFF)


def put_i16(data: bytearray, offset: int, value: int) -> None:
    struct.pack_into("<h", data, offset, value)


def put_f32(data: bytearray, offset: int, value: float) -> None:
    struct.pack_into("<f", data, offset, value)


def fbits(value: float) -> int:
    return struct.unpack("<I", struct.pack("<f", value))[0]


def as_float(bits: int) -> float:
    return struct.unpack("<f", struct.pack("<I", bits & 0xFFFFFFFF))[0]


Invoke = Callable[[Emulator, int], tuple[CallResult, int | None]]


class Verifier:
    def __init__(self, executable: Path) -> None:
        self.emu = Emulator()
        self.cpp = CppOracle(executable)
        self.counts: dict[int, int] = {}
        self.control_word: int | None = None
        self._init_stubs()

    def _init_stubs(self) -> None:
        # 1. Stub for 0x00561d70: W3DTerrainVisual::vtable[10](float x, float y) -> subobj*
        def stub_tv_slot10(emu: Emulator) -> int:
            self_ptr = emu.register(x86_const.UC_X86_REG_ECX)
            x_bits = emu.stack_arg(0)
            y_bits = emu.stack_arg(1)
            emu.write(self_ptr + 0x08, struct.pack("<II", x_bits, y_bits))
            if as_float(x_bits) >= 0.0:
                return u32(emu.read(self_ptr + 0x04, 4), 0)
            return 0

        self.stub_tv_slot10 = self.emu.bind_stub(stub_tv_slot10, stack_cleanup=8)

        # 2. Stub for 0x00561e10: renderObj::vtable[10](const int32_t* region)
        def stub_ro_slot10_update(emu: Emulator) -> int:
            self_ptr = emu.register(x86_const.UC_X86_REG_ECX)
            region_ptr = emu.stack_arg(0)
            cnt = u32(emu.read(self_ptr + 0x04, 4), 0) + 1
            emu.write(self_ptr + 0x04, struct.pack("<I", cnt))
            if region_ptr != 0:
                reg_bytes = emu.read(region_ptr, 16)
                emu.write(self_ptr + 0x08, struct.pack("<I", 1) + reg_bytes)
            else:
                emu.write(self_ptr + 0x08, struct.pack("<I", 0))
            return 0

        self.stub_ro_slot10_update = self.emu.bind_stub(stub_ro_slot10_update, stack_cleanup=4)

        # 3. Stub for 0x00561e80: renderObj::vtable[19]()
        def stub_ro_slot19_remove_bibs(emu: Emulator) -> int:
            self_ptr = emu.register(x86_const.UC_X86_REG_ECX)
            cnt = u32(emu.read(self_ptr + 0x04, 4), 0) + 0x100
            emu.write(self_ptr + 0x04, struct.pack("<I", cnt))
            return 0

        self.stub_ro_slot19_remove_bibs = self.emu.bind_stub(stub_ro_slot19_remove_bibs, stack_cleanup=0)

        # 4. Stub for 0x005882c0: renderObj::vtable[2](void* rayCollisionTest)
        def stub_ro_slot11_cast_ray(emu: Emulator) -> int:
            self_ptr = emu.register(x86_const.UC_X86_REG_ECX)
            ray_test_ptr = emu.stack_arg(0)
            cast_res_ptr = u32(emu.read(ray_test_ptr + 0x00, 4), 0)
            ray_len_bits = u32(emu.read(ray_test_ptr + 0x38, 4), 0)
            col_type = u32(emu.read(ray_test_ptr + 0x04, 4), 0)
            emu.write(self_ptr + 0x14, struct.pack("<II", ray_len_bits, col_type))
            contact = emu.read(self_ptr + 0x08, 12)
            emu.write(cast_res_ptr + 0x18, contact)
            return u32(emu.read(self_ptr + 0x04, 4), 0)

        self.stub_ro_slot11_cast_ray = self.emu.bind_stub(stub_ro_slot11_cast_ray, stack_cleanup=4)

        # 5. Stub for 0x00562d30: InputStream::vtable[1](void* dst, int32_t size)
        def stub_stream_read(emu: Emulator) -> int:
            self_ptr = emu.register(x86_const.UC_X86_REG_ECX)
            dst_ptr = emu.stack_arg(0)
            req_size = struct.unpack("<i", struct.pack("<I", emu.stack_arg(1)))[0]
            state = emu.read(self_ptr + 0x04, 12)
            data_ptr, total, pos = struct.unpack("<Iii", state)
            avail = total - pos
            to_read = min(req_size, max(avail, 0))
            if to_read > 0:
                chunk = emu.read(data_ptr + pos, to_read)
                emu.write(dst_ptr, chunk)
                emu.write(self_ptr + 0x0C, struct.pack("<i", pos + to_read))
            return to_read

        self.stub_stream_read = self.emu.bind_stub(stub_stream_read, stack_cleanup=8)

        # 6. Stub for 0x00563520: StreamObj::vtable[3](uint32_t lockArg)
        def stub_stream_obj_slot3(emu: Emulator) -> int:
            self_ptr = emu.register(x86_const.UC_X86_REG_ECX)
            lock_arg = emu.stack_arg(0)
            emu.write(self_ptr + 0x08, struct.pack("<I", lock_arg))
            return (u32(emu.read(self_ptr + 0x04, 4), 0) + lock_arg) & 0xFFFFFFFF

        self.stub_stream_obj_slot3 = self.emu.bind_stub(stub_stream_obj_slot3, stack_cleanup=4)

    def select_control_word(self, control_word: int) -> None:
        if self.control_word == control_word:
            return
        self.cpp.set_control_word(control_word)
        self.emu.uc.reg_write(x86_const.UC_X86_REG_FPCW, control_word)
        self.control_word = control_word

    def case(
        self, address: int, blob: bytes, fixups: list[int], arguments: list[int],
        invoke: Invoke, cleanup: int, *, compare_result: bool = False,
        control_word: int = 0x037F,
        emu_patches: list[tuple[int, int]] | None = None,
    ) -> None:
        self.select_control_word(control_word)
        cpp_result, cpp_blob = self.cpp.call(address, blob, fixups, arguments)
        self.emu.reset_data()
        base = self.emu.alloc(blob)
        for offset in fixups:
            relative = u32(blob, offset)
            self.emu.write(
                base + offset, struct.pack("<I", 0 if relative == 0 else base + relative))
        if emu_patches:
            for patch_off, patch_val in emu_patches:
                self.emu.write(base + patch_off, struct.pack("<I", patch_val))
        result, game_value = invoke(self.emu, base)
        if result.stack_cleanup != cleanup:
            raise AssertionError(
                f"0x{address:08X}: cleanup {result.stack_cleanup}, expected {cleanup}")
        if emu_patches:
            for patch_off, _ in emu_patches:
                self.emu.write(base + patch_off, struct.pack("<I", 0))
        game_mutated = bytearray(self.emu.read(base, len(blob)))
        for offset in fixups:
            absolute = u32(game_mutated, offset)
            if absolute == 0:
                relative = 0
            elif base <= absolute <= base + len(blob):
                relative = absolute - base
            else:
                relative = 0xFFFFFFFF
            put_u32(game_mutated, offset, relative)
        if bytes(game_mutated) != cpp_blob:
            for idx, (gb, cb) in enumerate(zip(game_mutated, cpp_blob)):
                if gb != cb:
                    raise AssertionError(
                        f"0x{address:08X}: memory mismatch at +0x{idx:X}: "
                        f"game=0x{gb:02X} cpp=0x{cb:02X}")
        if compare_result and game_value != cpp_result:
            raise AssertionError(
                f"0x{address:08X}: result game=0x{game_value:08X} cpp=0x{cpp_result:08X}")
        self.counts[address] = self.counts.get(address, 0) + 1

    def close(self) -> None:
        self.cpp.close()


def verify(executable: Path) -> None:
    v = Verifier(executable)

    def call_this(
        address: int, blob: bytearray | bytes, fixups: list[int],
        cpp_args: list[int], emu_stack_fn: Callable[[int], list[int]], *,
        ecx_off: int = 0, cleanup: int, compare_result: bool = False,
        result_fn: Callable[[CallResult, int], int] | None = None,
        emu_patches: list[tuple[int, int]] | None = None,
    ) -> None:
        def invoke(emu: Emulator, base: int) -> tuple[CallResult, int | None]:
            out = emu.call(address, emu_stack_fn(base), ecx=base + ecx_off)
            if result_fn is not None:
                return out, result_fn(out, base)
            return out, (out.eax if compare_result else None)

        v.case(
            address, bytes(blob), fixups, cpp_args, invoke, cleanup,
            compare_result=compare_result or (result_fn is not None),
            emu_patches=emu_patches)

    # =========================================================================
    # 1. W3DTerrainVisual & ray/water helpers
    # =========================================================================

    # 0x00561D70: W3DTerrainVisual::getBridgeRenderObjectIDAt (0x00be38e8 slot 9)
    for x_val, y_val, sub_id, has_sub in (
        (10.0, 20.0, 0x12345678, True),
        (-5.0, 15.0, 0xCAFEBABE, True),
        (3.5, -2.0, 0x00000000, False),
    ):
        blob = bytearray(0x100)
        put_u32(blob, 0x00, 0x40)  # vtable at +0x40
        put_u32(blob, 0x04, 0x80 if has_sub else 0)  # subobj at +0x80
        put_u32(blob, 0x80, 0xA0)  # subobj vtable at +0xA0
        put_u32(blob, 0xA0 + 0x18, sub_id)
        xb, yb = fbits(x_val), fbits(y_val)
        call_this(
            0x00561D70, blob, [0x00, 0x04, 0x80], [0, xb, yb],
            lambda _, x=xb, y=yb: [x, y],
            cleanup=8, compare_result=True,
            emu_patches=[(0x40 + 0x28, v.stub_tv_slot10)])

    # 0x00561DB0: W3DTerrainVisual::getRawMapHeight (0x00be38e8 slot 20)
    for null_hm, null_data, x_q, y_q, sample_val in (
        (True, False, 1, 1, 256),
        (False, True, 1, 1, 256),
        (False, False, -5, 0, 500),
        (False, False, 0, -5, 500),
        (False, False, 10, 0, 500),
        (False, False, 0, 10, 500),
        (False, False, 1, 2, 0),
        (False, False, 1, 2, 256),
        (False, False, 2, 1, 1280),
        (False, False, 0, 0, 65535),
    ):
        blob = bytearray(0x200)
        hm = 0x40
        samples = 0x100
        fixups: list[int] = []
        if not null_hm:
            put_u32(blob, 0x30, hm)
            fixups.append(0x30)
            put_i32(blob, hm + 0x24, 4)   # width = 4
            put_i32(blob, hm + 0x28, 4)   # height = 4
            put_i32(blob, hm + 0x2C, 1)   # border = 1
            put_i32(blob, hm + 0x40, 16)  # dataSize = 16
            if not null_data:
                put_u32(blob, hm + 0x44, samples)
                fixups.append(hm + 0x44)
                for i in range(16):
                    put_u16(blob, samples + i * 2, (sample_val + i * 17) & 0xFFFF)
        call_this(
            0x00561DB0, blob, fixups, [0, x_q, y_q],
            lambda _, x=x_q, y=y_q: [x, y],
            cleanup=8, compare_result=True)

    # 0x00561E10: W3DTerrainVisual::partialMapUpdate (0x00be38e8 slot 21)
    for null_reg, coords in (
        (True, (0, 0, 0, 0)),
        (False, (10, 20, 30, 40)),
        (False, (-5, -10, 0, 5)),
        (False, (0, 1, 100, 200)),
    ):
        blob = bytearray(0x100)
        ro = 0x40
        vt = 0x80
        reg = 0xC0
        put_u32(blob, 0x28, ro)
        put_u32(blob, ro, vt)
        fixups = [0x28, ro]
        patches = [(vt + 0x28, v.stub_ro_slot10_update)]
        for idx, val in enumerate(coords):
            put_i32(blob, reg + idx * 4, val)
        reg_arg = 0xFFFFFFFF if null_reg else reg
        call_this(
            0x00561E10, blob, fixups, [0, reg_arg],
            lambda base, nr=null_reg, r=reg: [0 if nr else base + r],
            cleanup=4, emu_patches=patches)

    # 0x00561E70: W3DTerrainVisual::getBorderSize (0x00be38e8 slot 23)
    for null_hm, border_val in ((True, 0), (False, 0), (False, 5), (False, 12)):
        blob = bytearray(0x80)
        fixups = []
        if not null_hm:
            put_u32(blob, 0x30, 0x40)
            put_i32(blob, 0x40 + 0x2C, border_val)
            fixups.append(0x30)
        call_this(
            0x00561E70, blob, fixups, [0], lambda _: [],
            cleanup=0, compare_result=True)

    # 0x00561E80: W3DTerrainVisual::removeAllBibs (0x00be38e8 slot 18)
    for null_ro, init_cnt in ((True, 3), (False, 0), (False, 7)):
        blob = bytearray(0x100)
        fixups = []
        patches = []
        put_u32(blob, 0x40 + 0x04, init_cnt)
        if not null_ro:
            put_u32(blob, 0x28, 0x40)
            put_u32(blob, 0x40, 0x80)
            fixups.extend([0x28, 0x40])
            patches.append((0x80 + 0x4C, v.stub_ro_slot19_remove_bibs))
        call_this(
            0x00561E80, blob, fixups, [0], lambda _: [],
            cleanup=0, emu_patches=patches)

    # 0x0056B290 (W3DTerrainVisual::getTerrainColorAt) & 0x00562DF0 (WorldHeightMap::getTerrainColorAt)
    for x_f, y_f, border, rgb555, null_hm, null_tile in (
        (15.0, 25.0, 1, 0x7E10, False, False),
        (-50.0, 500.0, 2, 0x1234, False, False),
        (0.0, 0.0, 0, 0x7FFF, False, True),
        (10.0, 10.0, 1, 0x03E0, True, False),
    ):
        blob = bytearray(0x3200)
        hm = 0x40
        tiles_ndx = 0x200
        tile_obj = 0x400
        out_col = 0x30
        put_f32(blob, out_col + 0, -1.0)
        put_f32(blob, out_col + 4, -1.0)
        put_f32(blob, out_col + 8, -1.0)
        fixups = []
        if not null_hm:
            put_u32(blob, 0x30, hm)
            fixups.append(0x30)
        put_i32(blob, hm + 0x24, 4)
        put_i32(blob, hm + 0x28, 4)
        put_i32(blob, hm + 0x2C, border)
        put_i32(blob, hm + 0x40, 16)
        put_u32(blob, hm + 0xF8, tiles_ndx)
        fixups.append(hm + 0xF8)
        for i in range(16):
            put_i16(blob, tiles_ndx + i * 2, (i % 3) * 4)
        if not null_tile:
            for t in range(3):
                put_u32(blob, hm + 0x110 + t * 4, tile_obj)
                fixups.append(hm + 0x110 + t * 4)
            put_u16(blob, tile_obj + 0x2AB0, rgb555)
        xb, yb = fbits(x_f), fbits(y_f)
        # Test 0x00562DF0 directly on WorldHeightMap
        call_this(
            0x00562DF0, blob, [f for f in fixups if f != 0x30],
            [hm, xb, yb, out_col],
            lambda base, x=xb, y=yb, oc=out_col: [x, y, base + oc],
            ecx_off=hm, cleanup=12)
        # Test 0x0056B290 on W3DTerrainVisual
        out_tv = 0x10
        put_f32(blob, out_tv + 0, -2.0)
        put_f32(blob, out_tv + 4, -2.0)
        put_f32(blob, out_tv + 8, -2.0)
        if null_hm:
            put_u32(blob, 0x30, 0)
        call_this(
            0x0056B290, blob, fixups,
            [0, xb, yb, out_tv],
            lambda base, x=xb, y=yb, ot=out_tv: [x, y, base + ot],
            ecx_off=0, cleanup=12)

    # 0x0056B2F0: W3DTerrainVisual::setRawMapHeightAndFlags (0x00be38e8 slot 19)
    # 0x0056B380: W3DTerrainVisual::getCellFlags (0x00be38e8 slot 22)
    for null_hm, null_arr, x_q, y_q, h_val, f_word in (
        (True, False, 1, 1, 10, 0x000000AB),
        (False, True, 1, 1, 10, 0x000000AB),
        (False, False, -5, 1, 10, 0x000000AB),
        (False, False, 1, 10, 10, 0x000000AB),
        (False, False, 0, 0, 12, 0x12345601),
        (False, False, 2, 1, 45, 0x0000FE00),   # flagByte == 0, curSample(500) > 45 -> updates
        (False, False, 2, 1, 800, 0x0000FE00),  # flagByte == 0, curSample(500) <= 800 -> skips
        (False, False, 1, 2, 0, 0x00000105),
    ):
        blob = bytearray(0x200)
        hm = 0x40
        heights = 0x100
        flags = 0x140
        fixups = []
        if not null_hm:
            put_u32(blob, 0x30, hm)
            fixups.append(0x30)
            put_i32(blob, hm + 0x24, 4)
            put_i32(blob, hm + 0x28, 4)
            put_i32(blob, hm + 0x2C, 1)
            put_i32(blob, hm + 0x40, 16)
            if not null_arr:
                put_u32(blob, hm + 0x44, heights)
                put_u32(blob, hm + 0x48, flags)
                fixups.extend([hm + 0x44, hm + 0x48])
                for i in range(16):
                    put_u16(blob, heights + i * 2, 500)
                    blob[flags + i] = (0x80 + i) & 0xFF
        if not (null_arr and (f_word & 0xFF) == 0):
            call_this(
                0x0056B2F0, blob, fixups, [0, x_q, y_q, h_val, f_word],
                lambda _, x=x_q, y=y_q, h=h_val, fw=f_word: [x, y, h, fw],
                cleanup=16, compare_result=True)
        if not null_hm:
            call_this(
                0x0056B380, blob, fixups, [0, x_q, y_q],
                lambda _, x=x_q, y=y_q: [x, y],
                cleanup=8, compare_result=True)

    # 0x00570D50 (LineSegClass::recalculate) & 0x00580690 (LineSegClass::set)
    for p0, p1 in (
        ((0.0, 0.0, 0.0), (3.0, 4.0, 12.0)),
        ((1.0, 2.0, 3.0), (1.0, 2.0, 3.0)),          # zero length (<= 1e-4)
        ((1.0, 2.0, 3.0), (1.00005, 2.0, 3.0)),      # length < 1e-4
        ((-10.5, 20.25, -5.0), (14.5, -4.75, 7.0)),
    ):
        blob = bytearray(0x80)
        for i in range(3):
            put_f32(blob, 0x00 + i * 4, p0[i])
            put_f32(blob, 0x0C + i * 4, p1[i])
            put_f32(blob, 0x40 + i * 4, p0[i])
            put_f32(blob, 0x50 + i * 4, p1[i])
        call_this(0x00570D50, blob, [], [0], lambda _: [], cleanup=0)
        call_this(
            0x00580690, blob, [], [0, 0x40, 0x50],
            lambda base: [base + 0x40, base + 0x50],
            cleanup=8, result_fn=lambda out, base: out.eax - base)

    # 0x00572070: RayCollisionTestClass::init
    for col_type, f3c, f3d in ((1, 0, 0), (0x42, 1, 0), (7, 1, 1)):
        blob = bytearray(0x100)
        for i in range(13):
            put_f32(blob, 0x50 + i * 4, float(i + 1) * 1.25)
        call_this(
            0x00572070, blob, [0x00], [0, 0x50, 0x90, col_type, f3c, f3d],
            lambda base, ct=col_type, a=f3c, b=f3d: [base + 0x50, base + 0x90, ct, a, b],
            cleanup=20, result_fn=lambda out, base: out.eax - base)

    # 0x005882C0: W3DTerrainVisual::intersectTerrain (0x00be38e8 slot 11)
    for null_ro, null_out, ret_word in (
        (True, False, 0x00000001),
        (False, True, 0xABCD0101),
        (False, False, 0xABCD0101),
        (False, False, 0xABCD0100),
    ):
        blob = bytearray(0x140)
        ro = 0x40
        vt = 0x80
        p_start = 0xC0
        p_end = 0xD0
        p_out = 0xE0
        for i, val in enumerate((10.0, 20.0, 100.0)):
            put_f32(blob, p_start + i * 4, val)
        for i, val in enumerate((13.0, 24.0, 88.0)):
            put_f32(blob, p_end + i * 4, val)
        for i, val in enumerate((-1.0, -1.0, -1.0)):
            put_f32(blob, p_out + i * 4, val)
        put_u32(blob, ro + 0x04, ret_word)
        put_f32(blob, ro + 0x08, 11.5)
        put_f32(blob, ro + 0x0C, 22.5)
        put_f32(blob, ro + 0x10, 5.25)
        fixups = []
        patches = []
        if not null_ro:
            put_u32(blob, 0x28, ro)
            put_u32(blob, ro, vt)
            fixups.extend([0x28, ro])
            patches.append((vt + 0x08, v.stub_ro_slot11_cast_ray))
        o_arg = 0xFFFFFFFF if null_out else p_out
        call_this(
            0x005882C0, blob, fixups, [0, p_start, p_end, o_arg],
            lambda base, no=null_out: [
                base + p_start,
                base + p_end,
                0 if no else base + p_out,
            ],
            cleanup=12,
            result_fn=(lambda out, _: out.eax & 0xFF) if null_ro else None,
            compare_result=not null_ro,
            emu_patches=patches)

    # 0x0057A140: initWaterWaveGridState
    for fill_byte in (0x00, 0xFF, 0x5A):
        blob = bytearray([fill_byte] * 0x1340)
        call_this(0x0057A140, blob, [], [0], lambda _: [], cleanup=0)

    # =========================================================================
    # 2. W3DTerrainLogic & helpers
    # =========================================================================

    # 0x0056EC10, 0x0056ECC0, 0x0056EDB0
    for empty_vec, sel_idx, min_z, max_z in (
        (True, 0, -5.0, 120.0),
        (False, 0, 0.0, 50.5),
        (False, 1, -12.25, 250.75),
        (False, 2, -100.0, 300.0),
    ):
        blob = bytearray(0x1600)
        entries_off = 0x100
        out_ext = 0x1500
        fixups = []
        if empty_vec:
            put_u32(blob, 0x54, entries_off)
            put_u32(blob, 0x58, entries_off)
        else:
            put_u32(blob, 0x54, entries_off)
            put_u32(blob, 0x58, entries_off + 3 * 16)
        fixups.extend([0x54, 0x58])
        put_i32(blob, 0x64, sel_idx)
        put_f32(blob, 0x1438, min_z)
        put_f32(blob, 0x143C, max_z)
        for i, rect in enumerate((
            (-5, 10, 40, 80),
            (-20, -15, 30, 120),
            (5, -30, 90, 60),
        )):
            for c_idx, c_val in enumerate(rect):
                put_i32(blob, entries_off + i * 16 + c_idx * 4, c_val)
        for addr in (0x0056EC10, 0x0056ECC0, 0x0056EDB0):
            call_this(
                addr, blob, fixups, [0, out_ext],
                lambda base, oe=out_ext: [base + oe], cleanup=4)

    # 0x00563980: orderCoord3DPair (__cdecl, 8 stack args)
    for a_pt, b_pt in (
        ((1.0, 2.0, 3.0), (4.0, 5.0, 6.0)),   # bx > ax -> a first
        ((4.0, 5.0, 6.0), (1.0, 2.0, 3.0)),   # bx < ax -> b first
        ((2.0, 1.0, 9.0), (2.0, 5.0, 8.0)),   # ax == bx, by > ay -> a first
        ((2.0, 5.0, 9.0), (2.0, 1.0, 8.0)),   # ax == bx, by < ay -> b first
        ((2.0, 5.0, 9.0), (2.0, 5.0, 8.0)),   # ax == bx, by == ay -> b first
    ):
        blob = bytearray(0x40)
        args_bits = [fbits(v) for v in (*a_pt, *b_pt)]
        v.case(
            0x00563980, bytes(blob), [], [*args_bits, 0x00, 0x10],
            lambda emu, base, ab=args_bits: (
                emu.call(0x00563980, [*ab, base + 0x00, base + 0x10]), None),
            cleanup=0)

    # =========================================================================
    # 3. WorldHeightMap
    # =========================================================================

    # 0x00562BB0 (setFlipState) & 0x0056DC30 (setCliffState)
    # and bit-array getters: 0x005642B0, 0x00564300, 0x0056D970, 0x0056D9D0,
    # 0x0056DA40, 0x0056DAB0, 0x0056DB20, 0x0056DB80
    for x_q, y_q, bit_val, init_byte, span_len in (
        (-1, 2, 1, 0x00, 8),
        (2, -1, 1, 0xFF, 8),
        (100, 100, 1, 0x55, 8),
        (3, 1, 1, 0x00, 8),
        (3, 1, 0, 0xFF, 8),
        (6, 2, 1, 0xAA, 8),
        (7, 0, 0, 0x80, 8),
        (2, 1, 1, 0xFF, 0),   # empty span (byteIndex >= spanSize)
    ):
        blob = bytearray(0x200)
        data_off = 0x120
        for i in range(16):
            blob[data_off + i] = init_byte
        put_i32(blob, 0x24, 16)  # width = 16
        put_i32(blob, 0x28, 4)   # height = 4
        put_i32(blob, 0x54, 2)   # byte stride per row = 2
        put_u32(blob, 0x4C, data_off)
        put_u32(blob, 0x50, span_len)
        fixups = [0x4C]
        # Populate vector<uint8_t> bit-array spans (begin at off, end at off+4)
        for begin_off in (0x58, 0x78, 0x88, 0x98, 0xA8, 0xB8, 0xC8, 0xD8):
            put_u32(blob, begin_off, data_off)
            put_u32(blob, begin_off + 4, data_off + span_len)
            fixups.extend([begin_off, begin_off + 4])

        for setter in (0x00562BB0, 0x0056DC30):
            call_this(
                setter, blob, fixups, [0, x_q, y_q, bit_val],
                lambda _, x=x_q, y=y_q, bv=bit_val: [x, y, bv],
                cleanup=12)

        for getter in (
            0x005642B0, 0x00564300, 0x0056D970, 0x0056D9D0,
            0x0056DA40, 0x0056DAB0, 0x0056DB20, 0x0056DB80,
        ):
            call_this(
                getter, blob, fixups, [0, x_q, y_q],
                lambda _, x=x_q, y=y_q: [x, y],
                cleanup=8, compare_result=True)

    # 0x00562D30: WorldHeightMap::countTiles
    for w, h, bpp, img_type, cmap_type, short_hdr, null_half in (
        (64, 64, 24, 2, 0, False, False),       # 1x1 normal 24-bit tile -> 1
        (192, 128, 32, 10, 0, False, False),    # 3x2 RLE 32-bit -> min(3,2)^2 = 4
        (32, 32, 32, 2, 0, False, False),       # 32x32 halfTile=1 -> 1
        (32, 32, 32, 2, 0, False, True),        # 32x32 with null halfTileOut -> 0
        (64, 64, 16, 2, 0, False, False),       # unsupported bpp=16 -> 0
        (64, 64, 24, 1, 0, False, False),       # unsupported img_type=1 -> 0
        (64, 64, 24, 2, 1, False, False),       # cmap_type != 0 -> 0
        (2048, 64, 24, 2, 0, False, False),     # tileWidth = 32 > 16 -> 0
        (64, 64, 24, 2, 0, True, False),        # short header (< 18 bytes) -> 0
    ):
        total_len = 8 if short_hdr else 18
        blob = bytearray(0x100)
        vt = 0x20
        buf = 0x60
        put_u32(blob, 0x00, vt)
        put_u32(blob, 0x04, buf)
        put_i32(blob, 0x08, total_len)
        put_i32(blob, 0x0C, 0)
        blob[buf + 0x01] = cmap_type
        blob[buf + 0x02] = img_type
        put_u16(blob, buf + 0x0C, w)
        put_u16(blob, buf + 0x0E, h)
        blob[buf + 0x10] = bpp
        half_arg = 0xFFFFFFFF if null_half else 0x50
        v.case(
            0x00562D30, bytes(blob), [0x00, 0x04], [0, half_arg],
            lambda emu, base, nh=null_half: (
                (out := emu.call(0x00562D30, [base + 0x00, 0 if nh else base + 0x50])),
                out.eax),
            cleanup=0, compare_result=True,
            emu_patches=[(vt + 0x00, v.stub_stream_read)])

    # 0x00562F10: WorldHeightMap::evaluateBlendTileCoeffs
    for null_info, b4, b5, b6, b7, b8, b9, d_0c in (
        (True, 0, 0, 0, 0, 0, 0, -1),
        (False, 1, 0, 0, 0, 0, 0, -1),
        (False, 1, 0, 0, 0, 1, 0, -1),
        (False, 0, 1, 0, 0, 0, 0, -1),
        (False, 0, 1, 0, 0, 1, 0, -1),
        (False, 0, 0, 1, 0, 0, 0, -1),
        (False, 0, 0, 1, 0, 0, 1, -1),
        (False, 0, 0, 1, 0, 1, 0, -1),
        (False, 0, 0, 1, 0, 1, 1, -1),
        (False, 0, 0, 0, 1, 0, 0, -1),
        (False, 0, 0, 0, 1, 0, 1, -1),
        (False, 0, 0, 0, 1, 1, 0, -1),
        (False, 0, 0, 0, 1, 1, 1, -1),
        (False, 0, 0, 1, 0, 1, 1, 0),   # d_0c != -1 clears coeffs
    ):
        blob = bytearray(0x80)
        info = 0x20
        out_c = 0x40
        blob[info + 4] = b4
        blob[info + 5] = b5
        blob[info + 6] = b6
        blob[info + 7] = b7
        blob[info + 8] = b8
        blob[info + 9] = b9
        put_i32(blob, info + 0x0C, d_0c)
        for i in range(4):
            put_f32(blob, out_c + i * 4, -9.0)
        info_arg = 0xFFFFFFFF if null_info else info
        call_this(
            0x00562F10, blob, [], [0, info_arg, out_c],
            lambda base, ni=null_info: [0 if ni else base + info, base + out_c],
            cleanup=8)

    # 0x0056DFB0 (setCellCliffFlagFromHeights) & 0x0057E4E0 (initCliffFlagsFromHeights)
    for h_samples in (
        (100, 200, 300, 400, 100, 3000, 100, 100, 100),  # cell (0,0) diff=2900 > 0x9cc
        (100, 100, 100, 100, 100, 2608, 100, 100, 100),  # diff=2508 == 0x9cc -> not cliff
        (0, 5000, 0, 5000, 0, 5000, 0, 5000, 0),
    ):
        blob = bytearray(0x200)
        heights = 0x100
        cliff_bits = 0x140
        put_i32(blob, 0x24, 3)   # width = 3
        put_i32(blob, 0x28, 3)   # height = 3
        put_i32(blob, 0x40, 9)   # dataSize = 9
        put_u32(blob, 0x44, heights)
        put_i32(blob, 0x54, 1)   # byte stride per row = 1
        put_u32(blob, 0x58, cliff_bits)
        put_u32(blob, 0x5C, cliff_bits + 4)
        for idx, h_val in enumerate(h_samples):
            put_u16(blob, heights + idx * 2, h_val)
        call_this(
            0x0056DFB0, blob, [0x44, 0x58, 0x5C], [0, 0, 0],
            lambda _: [0, 0], cleanup=8)
        call_this(
            0x0057E4E0, blob, [0x44, 0x58, 0x5C], [0],
            lambda _: [], cleanup=0)

    # 0x0056E0A0 (getCellBlendClassesAndCoeffs), 0x0056DD70 (isCliffMappedTextureCheck),
    # 0x0057DB70 (shouldFlipCellQuad), 0x0056DE70 (getBlendVertexAlphaMask)
    for scenario in range(8):
        blob = bytearray(0x17800)
        heights = 0x17200
        tile_ndxes = 0x17240
        blend_ndxes = 0x17280
        cliff_ndxes = 0x172C0
        extra_ndxes = 0x17300
        blend_descs = 0x17340
        cliff_table = 0x17440
        out_classes = 0x17540
        out_coeffs = 0x17560
        out_alphas = 0x175A0

        put_i32(blob, 0x24, 4)
        put_i32(blob, 0x28, 4)
        put_i32(blob, 0x40, 16)
        put_u32(blob, 0x44, heights)
        put_u32(blob, 0xF8, tile_ndxes)
        put_u32(blob, 0xFC, blend_ndxes)
        put_u32(blob, 0x100, cliff_ndxes)
        put_u32(blob, 0x104, extra_ndxes)
        put_u32(blob, 0x8110, blend_descs)
        put_u32(blob, 0x8114, blend_descs + 4 * 16)
        put_u32(blob, 0x8120, cliff_table)
        put_i32(blob, 0x8130, 2)  # 2 texture classes
        put_i32(blob, 0x17148, 0)
        put_i32(blob, 0x1714C, 0)

        # Texture classes at +0x8138 (stride 0x3C)
        put_i32(blob, 0x8138 + 0x00, 0)
        put_i32(blob, 0x8138 + 0x04, 2)
        put_i32(blob, 0x8138 + 0x3C + 0x00, 2)
        put_i32(blob, 0x8138 + 0x3C + 0x04, 3)

        for i in range(16):
            h_val = (i * 200) if scenario % 2 == 0 else ((i % 2) * 600 + (i // 4) * 100)
            put_u16(blob, heights + i * 2, h_val)
            put_i16(blob, tile_ndxes + i * 2, (i % 4) * 4)
            put_u16(blob, blend_ndxes + i * 2, (scenario + i) % 4)
            put_u16(blob, cliff_ndxes + i * 2, 1 if (scenario % 3 == 1 and i == 5) else 0)
            put_u16(blob, extra_ndxes + i * 2, (scenario + i + 1) % 4)

        for d in range(4):
            desc = blend_descs + d * 16
            put_i32(blob, desc + 0x00, (d * 4))
            blob[desc + 0x04 + (d % 4)] = 1
            blob[desc + 0x08] = d & 1
            blob[desc + 0x09] = (d >> 1) & 1
            put_i32(blob, desc + 0x0C, -1 if scenario < 6 else 0)

        blob[cliff_table + 36 + 3] = scenario & 1

        fixups = [0x44, 0xF8, 0xFC, 0x100, 0x104, 0x8110, 0x8114, 0x8120]
        xq = -1 if scenario == 7 else (scenario % 3)
        yq = scenario % 2

        call_this(
            0x0056E0A0, blob, fixups, [0, xq, yq, out_classes, out_coeffs],
            lambda base, x=xq, y=yq: [x, y, base + out_classes, base + out_coeffs],
            cleanup=16)
        call_this(
            0x0056DD70, blob, fixups, [0, xq, yq],
            lambda _, x=xq, y=yq: [x, y],
            cleanup=8, compare_result=True)
        call_this(
            0x0057DB70, blob, fixups, [0, xq, yq],
            lambda _, x=xq, y=yq: [x, y],
            cleanup=8, compare_result=True)
        if xq >= 0:
            call_this(
                0x0056DE70, blob, fixups, [0, xq, yq, out_alphas, 0],
                lambda base, x=xq, y=yq: [x, y, base + out_alphas, 0],
                cleanup=16)

    # =========================================================================
    # 4. BaseHeightMapRenderObjClass, Scorch, Road & Prop Buffers
    # =========================================================================

    # 0x00565250: BaseHeightMapRenderObjClass::clearPatchVisibilityAndBounds
    for cx, cy in ((0, 0), (2, 3), (4, 2)):
        blob = bytearray(0x400)
        patches = 0x100
        for i in range(0x280):
            blob[patches + i] = 0xFF
        put_u32(blob, 0xB4, patches)
        put_i32(blob, 0xC0, cx)
        put_i32(blob, 0xC4, cy)
        put_u32(blob, 0xC8, 99)
        call_this(0x00565250, blob, [0xB4], [0], lambda _: [], cleanup=0)

    # 0x00563520: BaseHeightMapRenderObjClass::initStreamLockHelper
    for ret_elem, lock_arg in ((0x11223344, 0), (0x00000000, 5), (0xDEADBEEF, 0x10)):
        blob = bytearray(0x100)
        stream_obj = 0x20
        vt = 0x60
        put_u32(blob, stream_obj + 0x00, vt)
        put_u32(blob, stream_obj + 0x04, ret_elem)
        call_this(
            0x00563520, blob, [0x00, stream_obj], [0, stream_obj, lock_arg],
            lambda base, la=lock_arg: [base + stream_obj, la],
            cleanup=8, result_fn=lambda out, base: out.eax - base,
            emu_patches=[(vt + 0x0C, v.stub_stream_obj_slot3)])

    # 0x005635F0 (vec3SignMaskMin), 0x005636E0 (vec3SignMaskMax),
    # 0x00564E70 (aaboxFromMinMax), 0x00564F20 (minMaxAABoxMergePoint),
    # 0x00564F80 (minMaxAABoxMergeCenterExtentBox)
    for a_vec, b_vec in (
        ((-1.0, 5.0, -3.5), (2.0, -4.0, -3.5)),
        ((10.0, 20.0, 30.0), (5.0, 25.0, 15.0)),
        ((0.0, -0.0, 1.5), (-0.0, 0.0, -1.5)),
    ):
        blob = bytearray(0x100)
        for i in range(3):
            put_f32(blob, 0x20 + i * 4, a_vec[i])
            put_f32(blob, 0x30 + i * 4, b_vec[i])
            put_f32(blob, 0x40 + i * 4, a_vec[i])
            put_f32(blob, 0x4C + i * 4, b_vec[i])
        v.case(
            0x005635F0, bytes(blob), [], [0x00, 0x20, 0x30],
            lambda emu, base: (
                (out := emu.call(0x005635F0, [base + 0x00, base + 0x20, base + 0x30])),
                out.eax - base),
            cleanup=0, compare_result=True)
        v.case(
            0x005636E0, bytes(blob), [], [0x00, 0x20, 0x30],
            lambda emu, base: (
                (out := emu.call(0x005636E0, [base + 0x00, base + 0x20, base + 0x30])),
                out.eax - base),
            cleanup=0, compare_result=True)
        call_this(
            0x00564E70, blob, [], [0x00, 0x40],
            lambda base: [base + 0x40], cleanup=4)
        call_this(
            0x00564F20, blob, [], [0x40, 0x20],
            lambda base: [base + 0x20], ecx_off=0x40, cleanup=4)

    for extent in ((0.0, 0.0, 0.0), (2.0, 3.0, 4.0), (0.0, 5.0, 0.0)):
        blob = bytearray(0x80)
        for i, val in enumerate((-2.0, -3.0, -4.0, 2.0, 3.0, 4.0)):
            put_f32(blob, 0x00 + i * 4, val)
        for i, val in enumerate((1.0, -1.0, 2.0, *extent)):
            put_f32(blob, 0x20 + i * 4, val)
        call_this(
            0x00564F80, blob, [], [0x00, 0x20],
            lambda base: [base + 0x20], ecx_off=0x00, cleanup=4)

    # 0x00565130: packCompressedTerrainVertex
    for pos, norm, tp, bp in (
        ((120.0, 340.0, 15.5), (0.0, 0.5, 0.866), (0.1, 0.2, 0.25, -0.5, 0.75, -0.25), (3.0, 1.0)),
        ((0.0, 50.0, -3.25), (-1.0, 1.0, 0.0), (0.0, 0.5, 1.0, 0.0, -1.0, 0.125), (0.0, 2.0)),
        ((640.0, 1280.0, 100.0), (0.25, -0.75, 0.5), (0.33, 0.5, -0.25, 0.75, 0.6, -0.8), (7.0, 4.0)),
    ):
        blob = bytearray(0x200)
        out_v = 0x120
        pos_off = 0x140
        norm_off = 0x150
        tp_off = 0x160
        bp_off = 0x180
        for i in range(3):
            put_f32(blob, pos_off + i * 4, pos[i])
            put_f32(blob, norm_off + i * 4, norm[i])
        for i in range(6):
            put_f32(blob, tp_off + i * 4, tp[i])
        for i in range(2):
            put_f32(blob, bp_off + i * 4, bp[i])
        call_this(
            0x00565130, blob, [], [0, out_v, pos_off, norm_off, tp_off, bp_off],
            lambda base: [base + out_v, base + pos_off, base + norm_off, base + tp_off, base + bp_off],
            cleanup=20)

    # 0x00565700: copyTerrainRenderState52
    for seed in (1, 2, 3):
        blob = bytearray(0x100)
        for i in range(13):
            put_f32(blob, 0x40 + i * 4, float(i * seed) * 0.75 - 3.0)
        call_this(
            0x00565700, blob, [], [0x00, 0x40],
            lambda base: [base + 0x40], cleanup=4)

    # 0x005615A0 (clearAllScorches) & 0x0056AB00 (invalidateScorchesInRadius)
    for count, radius in ((0, 10.0), (3, 5.0), (3, 50.0)):
        blob = bytearray(0x4690)
        put_i32(blob, 0x4660, count)
        put_u32(blob, 0x4664, 22)
        put_u32(blob, 0x4668, 33)
        for i in range(count):
            off = 0x14 + i * 0x24
            put_f32(blob, off + 0x00, float(i * 10))
            put_f32(blob, off + 0x04, float(i * 10))
            put_f32(blob, off + 0x08, 0.0)
            put_f32(blob, off + 0x0C, 3.0)
        center_off = 0x4680
        put_f32(blob, center_off + 0, 2.0)
        put_f32(blob, center_off + 4, 2.0)
        put_f32(blob, center_off + 8, 0.0)
        rad_bits = fbits(radius)
        call_this(
            0x0056AB00, blob, [], [0, center_off, rad_bits],
            lambda base, rb=rad_bits: [base + center_off, rb],
            cleanup=8)
        call_this(0x005615A0, blob, [], [0], lambda _: [], cleanup=0)

    # 0x005612F0, 0x00561340, 0x00569570, 0x005695E0, 0x00569750, 0x0056A8A0, 0x00568EA0, 0x00569480
    for scenario in range(6):
        blob = bytearray(0x600)
        types_off = 0x80
        roads_off = 0x300
        put_u32(blob, 0x00, types_off)
        put_u32(blob, 0x04, roads_off)
        put_i32(blob, 0x08, 3)   # 3 road segments
        put_i32(blob, 0x34, 3)   # 3 road types
        blob[0x40] = 0 if scenario == 5 else 1
        type_fixups = []
        for j in range(3):
            toff = types_off + j * 0xA0
            put_u32(blob, toff + 0x04, roads_off)
            type_fixups.append(toff + 0x04)
            put_i32(blob, toff + 0x14, 100 + j)
            put_i32(blob, toff + 0x1C, j * 2)
        road_fixups = []
        for i in range(3):
            roff = roads_off + i * 0x7C
            # Make endpoints match across segments according to scenario
            p0x = 10.0 if (i == 0 or (i == 1 and scenario in (0, 2))) else float(i * 20)
            p0y = 20.0 if (i == 0 or (i == 1 and scenario in (0, 2))) else float(i * 20)
            p1x = 10.0 if (i == 2 and scenario in (1, 2)) else 50.0
            p1y = 20.0 if (i == 2 and scenario in (1, 2)) else 60.0
            put_f32(blob, roff + 0x00, p0x)
            put_f32(blob, roff + 0x04, p0y)
            put_f32(blob, roff + 0x20, p1x)
            put_f32(blob, roff + 0x24, p1y)
            put_u32(blob, roff + 0x44, roads_off)
            road_fixups.append(roff + 0x44)
            put_i32(blob, roff + 0x54, 100 + (0 if scenario < 4 else i))

        all_road_fixups = [0x00, 0x04, *type_fixups, *road_fixups]
        if scenario < 3:
            call_this(0x005612F0, blob, all_road_fixups, [0], lambda _: [], cleanup=0)
            call_this(0x00569570, blob, all_road_fixups, [0], lambda _: [], cleanup=0)
        call_this(
            0x005695E0, blob, [0x00, 0x04], [0, roads_off],
            lambda base: [base + roads_off], cleanup=4)
        call_this(0x00569750, blob, [0x00, 0x04], [0], lambda _: [], cleanup=0)

    for active, top_id, bot_id in (
        (1, 100, 101),
        (1, 102, 100),
        (1, 999, 100),
        (1, 100, 999),
        (0, 100, 101),
    ):
        blob = bytearray(0x300)
        types_off = 0x60
        put_u32(blob, 0x00, types_off)
        put_i32(blob, 0x34, 3)
        blob[0x40] = active
        for j in range(3):
            toff = types_off + j * 0xA0
            put_i32(blob, toff + 0x14, 100 + j)
            put_i32(blob, toff + 0x1C, j)
        call_this(
            0x00561340, blob, [0x00], [0, top_id, bot_id],
            lambda _, t=top_id, b=bot_id: [t, b], cleanup=8)

    for pt, center, angle in (
        ((10.0, 0.0), (0.0, 0.0), 0.0),
        ((12.0, 5.0), (2.0, 5.0), 1.5707963),
        ((-3.5, 4.25), (1.0, -2.0), -0.7853982),
    ):
        blob = bytearray(0x40)
        put_f32(blob, 0x10, pt[0])
        put_f32(blob, 0x14, pt[1])
        cx, cy, ang = fbits(center[0]), fbits(center[1]), fbits(angle)
        call_this(
            0x0056A8A0, blob, [], [0, 0x10, cx, cy, ang],
            lambda base, x=cx, y=cy, a=ang: [base + 0x10, x, y, a],
            cleanup=16)

    for fill_b in (0x00, 0xAA, 0xFF):
        blob = bytearray([fill_b] * 0x100)
        call_this(
            0x00568EA0, blob, [], [0], lambda _: [],
            cleanup=0, result_fn=lambda out, base: out.eax - base)

    for seed in (1, 2, 3):
        blob = bytearray(0x100)
        for i in range(30):
            put_f32(blob, 0x80 + i * 4, float(i + seed) * 0.5)
        blob[0x80 + 0x78] = seed & 1
        call_this(
            0x00569480, blob, [], [0x00, 0x80],
            lambda base: [base + 0x80],
            cleanup=4, result_fn=lambda out, base: out.eax - base)

    # 0x0058D1B0: W3DPropBuffer::init
    for fill_b in (0x00, 0x55, 0xFF):
        blob = bytearray([fill_b] * 0x40)
        call_this(
            0x0058D1B0, blob, [], [0], lambda _: [],
            cleanup=0, result_fn=lambda out, base: out.eax - base)

    missing = set(ACCEPTED_ENTRIES) - set(v.counts)
    if missing:
        raise AssertionError(
            "entries without oracle coverage: "
            + ", ".join(f"0x{address:08X}" for address in sorted(missing)))

    v.close()
    for address, count in sorted(v.counts.items()):
        print(f"0x{address:08X}: {count}/{count} matched, mismatches=0")
    print(f"total: {sum(v.counts.values())} cases, "
          f"{len(v.counts)}/{len(ACCEPTED_ENTRIES)} entries, mismatches=0")
    verify_deliberate_mutations(executable)


def verify_deliberate_mutations(executable: Path) -> None:
    """Prove the differential oracle catches semantic mutations."""
    v = Verifier(executable)
    caught = 0

    # Mutation 1: Perturb height sample scaling in 0x00561DB0
    blob = bytearray(0x200)
    put_u32(blob, 0x30, 0x40)
    put_i32(blob, 0x40 + 0x24, 4)
    put_i32(blob, 0x40 + 0x28, 4)
    put_i32(blob, 0x40 + 0x2C, 1)
    put_i32(blob, 0x40 + 0x40, 16)
    put_u32(blob, 0x40 + 0x44, 0x100)
    put_u16(blob, 0x100 + 5 * 2, 256)
    try:
        v.case(
            0x00561DB0, bytes(blob), [0x30, 0x40 + 0x44], [0, 0, 0],
            lambda emu, base: (
                (out := emu.call(0x00561DB0, [0, 0], ecx=base)),
                out.eax ^ 1),
            8, compare_result=True)
    except AssertionError:
        caught += 1

    # Mutation 2: Perturb written extent float in 0x0056EC10
    blob2 = bytearray(0x1600)
    put_u32(blob2, 0x54, 0x100)
    put_u32(blob2, 0x58, 0x100 + 16)
    put_i32(blob2, 0x64, 0)
    put_i32(blob2, 0x100 + 8, 40)
    try:
        def bad_extent(emu: Emulator, base: int) -> tuple[CallResult, None]:
            out = emu.call(0x0056EC10, [base + 0x1500], ecx=base)
            emu.write(base + 0x1500 + 0x0C, struct.pack("<f", 1.0))
            return out, None

        v.case(0x0056EC10, bytes(blob2), [0x54, 0x58], [0, 0x1500], bad_extent, 4)
    except AssertionError:
        caught += 1

    v.close()
    if caught != 2:
        raise AssertionError(f"expected 2 deliberate mutations caught, got {caught}")
    print(f"deliberate_mutations: {caught}/2 caught as expected")


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument(
        "--render56check", type=Path,
        default=Path(__file__).resolve().parents[3]
        / "build/win32/bin/Release/ra3_render56check.exe")
    args = parser.parse_args()
    verify(args.render56check.resolve())


if __name__ == "__main__":
    main()
