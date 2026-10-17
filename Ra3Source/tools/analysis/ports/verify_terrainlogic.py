"""Differentially verify the 30 reconstructed TerrainLogic cluster functions against ra3_1.0.game."""

from __future__ import annotations

import argparse
import math
import random
import struct
import subprocess
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from emulate import Emulator  # noqa: E402

CONTROL_WORDS = (0x007F, 0x027F, 0x037F)
GLOBAL_WAYPOINT_LIST_HEAD = 0x00DFC194
GLOBAL_THE_TERRAIN_LOGIC = 0x00DFC190
GLOBAL_THE_PARTITION_MANAGER = 0x00E07D94
GLOBAL_LOGIC_RANDOM = 0x00CB7420
GLOBAL_RANDOM_SCALE = 0x00DF65B0
IAT_CEIL = 0x00BBA4DC


def f32_to_bits(val: float) -> int:
    return struct.unpack("<I", struct.pack("<f", val))[0]


def bits_to_f32(bits: int) -> float:
    return struct.unpack("<f", struct.pack("<I", bits & 0xFFFFFFFF))[0]


def pack_coord2d(x: float, y: float) -> bytes:
    return struct.pack("<2f", x, y)


def pack_coord3d(x: float, y: float, z: float) -> bytes:
    return struct.pack("<3f", x, y, z)


def pack_region2d(lox: float, loy: float, hix: float, hiy: float) -> bytes:
    return struct.pack("<4f", lox, loy, hix, hiy)


def pack_region3d(lox: float, loy: float, loz: float, hix: float, hiy: float, hiz: float) -> bytes:
    return struct.pack("<6f", lox, loy, loz, hix, hiy, hiz)


def pack_bridge(
    fl: tuple[float, float, float],
    fr: tuple[float, float, float],
    tl: tuple[float, float, float],
    tr: tuple[float, float, float],
    bounds: tuple[float, float, float, float],
    layer: int = 2,
    poly_ptr: int = 0,
    span_begin: int = 0,
    span_end: int = 0,
    next_ptr: int = 0,
) -> bytes:
    buf = bytearray(0x60)
    struct.pack_into("<I", buf, 0x00, next_ptr & 0xFFFFFFFF)
    struct.pack_into("<3f", buf, 0x04, *fl)
    struct.pack_into("<3f", buf, 0x10, *fr)
    struct.pack_into("<3f", buf, 0x1C, *tl)
    struct.pack_into("<3f", buf, 0x28, *tr)
    struct.pack_into("<i", buf, 0x34, 77)
    struct.pack_into("<4f", buf, 0x38, *bounds)
    struct.pack_into("<i", buf, 0x48, layer)
    struct.pack_into("<I", buf, 0x4C, poly_ptr & 0xFFFFFFFF)
    struct.pack_into("<I", buf, 0x50, span_begin & 0xFFFFFFFF)
    struct.pack_into("<I", buf, 0x54, span_end & 0xFFFFFFFF)
    struct.pack_into("<I", buf, 0x58, span_end & 0xFFFFFFFF)
    return bytes(buf)


class CppOracle:
    def __init__(self, executable: Path) -> None:
        self.process = subprocess.Popen(
            [str(executable)],
            stdin=subprocess.PIPE,
            stdout=subprocess.PIPE,
            text=True,
            bufsize=1,
        )

    def call(self, line: str) -> str:
        assert self.process.stdin is not None and self.process.stdout is not None
        self.process.stdin.write(line + "\n")
        self.process.stdin.flush()
        result = self.process.stdout.readline().strip()
        if not result or result == "error":
            raise RuntimeError(f"terrainlogiccheck rejected: {line[:120]!r}")
        return result

    def set_cw(self, cw: int) -> None:
        if self.call(f"fpcw {cw}") != "ok":
            raise RuntimeError("failed to set FPU control word")

    def close(self) -> None:
        assert self.process.stdin is not None
        self.process.stdin.write("quit\n")
        self.process.stdin.flush()
        if self.process.wait(timeout=10) != 0:
            raise RuntimeError("terrainlogiccheck exited with non-zero status")


def verify_executable(executable: Path) -> dict[str, int]:
    rng = random.Random(0x8684C0)
    emu = Emulator()
    cpp = CppOracle(executable)
    counts: dict[str, int] = {}

    def bump(name: str) -> None:
        counts[name] = counts.get(name, 0) + 1

    def rand_f(lo: float = -250.0, hi: float = 250.0) -> float:
        return bits_to_f32(f32_to_bits(rng.uniform(lo, hi)))

    # 1. 0x0085C5E0: getTerrainBoundaryVector
    for idx in (-5, -1, 0, 1, 2, 7, 15, 100):
        emu.reset_data()
        base = 0x60010000
        res = emu.call(0x0085C5E0, [idx & 0xFFFFFFFF], ecx=base)
        assert res.stack_cleanup == 4
        ours = int(cpp.call(f"boundaryvec {base} {idx}"))
        assert res.eax == ours, f"boundaryvec: game={res.eax:#x} ours={ours:#x}"
        bump("0x0085C5E0_getTerrainBoundaryVector")

    # 2. 0x0085C650: setTerrainActiveBoundary
    for cur_val in (0, 1, 3, -1):
        for new_val in (0, 1, 3, -1, 7):
            emu.reset_data()
            called = False

            def on_changed(_e: Emulator) -> int:
                nonlocal called
                called = True
                return 0

            vt_stub = emu.bind_stub(on_changed, 0)
            vt = emu.alloc(b"\x00" * 0x28 + struct.pack("<I", vt_stub))
            tl_init = bytearray(0x90)
            struct.pack_into("<I", tl_init, 0x00, vt)
            struct.pack_into("<i", tl_init, 0x88, cur_val)
            tl_addr = emu.alloc(bytes(tl_init))
            res = emu.call(0x0085C650, [new_val & 0xFFFFFFFF], ecx=tl_addr)
            assert res.stack_cleanup == 4
            game_tl = emu.read(tl_addr, 0x90)
            ours_hex, ours_cb = cpp.call(f"setactiveboundary {bytes(tl_init).hex()} {new_val}").split()
            assert game_tl == bytes.fromhex(ours_hex)
            assert int(called) == int(ours_cb)
            bump("0x0085C650_setTerrainActiveBoundary")

    # 3. 0x0085CA80: getOriginGroundHeight
    for cw in CONTROL_WORDS:
        cpp.set_cw(cw)
        for has_norm in (0, 1):
            for ret_val in (0.0, 15.25, -42.5, 123.75):
                emu.reset_data()
                ret_bits = f32_to_bits(ret_val)
                norm_stub = pack_coord3d(0.0, 0.0, 1.0)
                norm_init = pack_coord3d(9.0, 8.0, 7.0)
                norm_addr = emu.alloc(norm_init) if has_norm else 0

                def stub_gh(e: Emulator) -> int:
                    n_ptr = e.stack_arg(2)
                    if n_ptr:
                        e.write(n_ptr, norm_stub)
                    # Push float onto x87 ST0 by writing to scratch and running fld
                    return 0

                # Use base getGroundHeight (0x0085DDF0) in vtable[+0x18] when ret_val == 0.0
                if ret_val == 0.0:
                    norm_stub = pack_coord3d(0.0, 0.0, 0.0)
                    vt = emu.alloc(b"\x00" * 0x18 + struct.pack("<I", 0x0085DDF0))
                    tl_addr = emu.alloc(struct.pack("<I", vt))
                    res, game_bits = emu.call_float32(
                        0x0085CA80, [0, norm_addr], ecx=tl_addr, fpu_control_word=cw
                    )
                    assert res.stack_cleanup == 8
                    game_norm = emu.read(norm_addr, 12) if has_norm else norm_init
                    ours_bits_s, ours_norm_h = cpp.call(
                        f"origingroundheight {has_norm} {norm_init.hex()} {ret_bits} {norm_stub.hex()}"
                    ).split()
                    assert game_bits == int(ours_bits_s)
                    assert game_norm == bytes.fromhex(ours_norm_h)
                    bump("0x0085CA80_getOriginGroundHeight")

    # 4. 0x0085CC70: getWaterHeightFromHandle
    for cw in CONTROL_WORDS:
        cpp.set_cw(cw)
        emu.reset_data()
        res, game_fpu80 = emu.call_float80(0x0085CC70, [0], ecx=0, fpu_control_word=cw)
        assert res.stack_cleanup == 4
        ours_fpu80 = bytes.fromhex(cpp.call("waterhandleheight 0 0"))
        assert game_fpu80 == ours_fpu80
        bump("0x0085CC70_getWaterHeightFromHandle")

        for raw_h in (-50, 0, 1, 25, 1000, -9999):
            emu.reset_data()
            stub_fn = emu.bind_stub(lambda _e, h=raw_h: h, 0)
            sub_vt = emu.alloc(struct.pack("<3I", 0, 0, stub_fn))
            vbtable = emu.alloc(struct.pack("<2i", 0, 0x10))
            handle_buf = bytearray(0x40)
            struct.pack_into("<I", handle_buf, 0x1C, vbtable)
            struct.pack_into("<I", handle_buf, 0x2C, sub_vt)
            handle_addr = emu.alloc(bytes(handle_buf))
            res, game_fpu80 = emu.call_float80(
                0x0085CC70, [handle_addr], ecx=0, fpu_control_word=cw
            )
            assert res.stack_cleanup == 4
            ours_fpu80_h, _ = cpp.call(f"waterhandleheight 1 {raw_h}").split()
            assert game_fpu80 == bytes.fromhex(ours_fpu80_h)
            bump("0x0085CC70_getWaterHeightFromHandle")

    # 5. 0x0085D110: pointInTriangle2D (normal + edge + colinear + NaN + random)
    tri_cases = [
        ((0.0, 0.0, 0.0), (10.0, 0.0, 0.0), (0.0, 10.0, 0.0), (2.5, 2.5, 0.0), 0, 1, 0),
        ((0.0, 0.0, 0.0), (0.0, 10.0, 0.0), (10.0, 0.0, 0.0), (2.5, 2.5, 0.0), 0, 1, 0),
        ((0.0, 0.0, 0.0), (10.0, 0.0, 0.0), (0.0, 10.0, 0.0), (8.0, 8.0, 0.0), 0, 1, 0),
        ((0.0, 0.0, 0.0), (10.0, 0.0, 0.0), (0.0, 10.0, 0.0), (5.0, 0.0, 0.0), 0, 1, 4),
        ((0.0, 0.0, 0.0), (10.0, 0.0, 0.0), (20.0, 0.0, 0.0), (5.0, 0.0, 0.0), 0, 1, 0),
        ((0.0, 0.0, 0.0), (10.0, 0.0, 0.0), (20.0, 0.0, 0.0), (25.0, 0.0, 0.0), 0, 1, 0),
        ((0.0, 0.0, 0.0), (0.0, 10.0, 0.0), (0.0, 20.0, 0.0), (0.0, 15.0, 0.0), 0, 1, 0),
        ((5.0, 5.0, 0.0), (5.0, 5.0, 0.0), (5.0, 5.0, 0.0), (5.0, 5.0, 0.0), 0, 1, 0),
        ((0.0, 0.0, 0.0), (10.0, 0.0, 0.0), (0.0, 10.0, 0.0), (float("nan"), 1.0, 0.0), 0, 1, 0),
    ]
    for _ in range(120):
        tri_cases.append((
            (rand_f(), rand_f(), rand_f()),
            (rand_f(), rand_f(), rand_f()),
            (rand_f(), rand_f(), rand_f()),
            (rand_f(), rand_f(), rand_f()),
            0, 1, rng.randrange(0, 16),
        ))
    for p0, p1, p2, pt, a1, a2, fl_in in tri_cases:
        emu.reset_data()
        p0_b, p1_b, p2_b, pt_b = (
            pack_coord3d(*p0),
            pack_coord3d(*p1),
            pack_coord3d(*p2),
            pack_coord3d(*pt),
        )
        a_p0 = emu.alloc(p0_b)
        a_p1 = emu.alloc(p1_b)
        a_p2 = emu.alloc(p2_b)
        a_pt = emu.alloc(pt_b)
        a_fl = emu.alloc(bytes([fl_in]))
        res = emu.call(0x0085D110, [a_p0, a_p1, a_p2, a_pt, a1, a2, a_fl])
        assert res.stack_cleanup == 0
        game_ok = res.eax & 0xFF
        game_fl = emu.read(a_fl, 1)[0]
        ours_ok_s, ours_fl_s = cpp.call(
            f"pointintri2d {p0_b.hex()} {p1_b.hex()} {p2_b.hex()} {pt_b.hex()} {a1} {a2} {fl_in}"
        ).split()
        assert game_ok == int(ours_ok_s) and game_fl == int(ours_fl_s)
        bump("0x0085D110_pointInTriangle2D")

    # 6. 0x0085D7E0: initializeFindPositionOptions
    for _ in range(25):
        emu.reset_data()
        raw = rng.randbytes(0x44)
        addr = emu.alloc(raw)
        res = emu.call(0x0085D7E0, [], ecx=addr)
        assert res.stack_cleanup == 0 and res.eax == addr
        game_b = emu.read(addr, 0x44)
        ours_b = bytes.fromhex(cpp.call(f"findposopts {raw.hex()}"))
        assert game_b == ours_b
        bump("0x0085D7E0_initializeFindPositionOptions")

    # 7. 0x0085D840: initializeBridgeInfo
    for _ in range(25):
        emu.reset_data()
        raw = rng.randbytes(0x34)
        addr = emu.alloc(raw)
        res = emu.call(0x0085D840, [], ecx=addr)
        assert res.stack_cleanup == 0 and res.eax == addr
        game_b = emu.read(addr, 0x34)
        ours_b = bytes.fromhex(cpp.call(f"bridgeinfo {raw.hex()}"))
        assert game_b == ours_b
        bump("0x0085D840_initializeBridgeInfo")

    # 8. 0x0085D890: isPointOnBridge
    for poly_ptr in (0, 0x12345678):
        for poly_ret in (0, 1):
            for _ in range(30):
                emu.reset_data()
                br_b = pack_bridge(
                    (0.0, 0.0, 20.0),
                    (0.0, 40.0, 20.0),
                    (100.0, 0.0, 20.0),
                    (100.0, 40.0, 20.0),
                    (0.0, 0.0, 100.0, 40.0),
                    layer=2,
                    poly_ptr=poly_ptr,
                )
                loc_b = pack_coord3d(rand_f(-20.0, 120.0), rand_f(-20.0, 60.0), rand_f(-10.0, 50.0))
                last_ic = b"\x00" * 12

                def stub_poly(e: Emulator, ret=poly_ret) -> int:
                    nonlocal last_ic
                    ic_ptr = e.stack_arg(0)
                    last_ic = e.read(ic_ptr, 12)
                    return ret

                emu.bind_function(0x0085CF60, stub_poly, stack_cleanup=4)
                a_br = emu.alloc(br_b)
                a_loc = emu.alloc(loc_b)
                res = emu.call(0x0085D890, [a_loc], ecx=a_br)
                assert res.stack_cleanup == 4
                game_ok = res.eax & 0xFF
                ours_ok_s, ours_ic_h = cpp.call(
                    f"pointonbridge {br_b.hex()} {loc_b.hex()} {poly_ret}"
                ).split()
                assert game_ok == int(ours_ok_s)
                assert last_ic == bytes.fromhex(ours_ic_h)
                bump("0x0085D890_isPointOnBridge")

    # 9. 0x0085DA00: hasTerrainBoundaries
    for b_ptr, e_ptr in ((0, 0), (0x1000, 0x1000), (0x1000, 0x1008), (0x2000, 0x2040)):
        emu.reset_data()
        tl_b = bytearray(0x60)
        struct.pack_into("<2I", tl_b, 0x50, b_ptr, e_ptr)
        a_tl = emu.alloc(bytes(tl_b))
        res = emu.call(0x0085DA00, [], ecx=a_tl)
        assert res.stack_cleanup == 0
        ours = int(cpp.call(f"hasboundaries {b_ptr} {e_ptr}"))
        assert (res.eax & 0xFF) == ours
        bump("0x0085DA00_hasTerrainBoundaries")

    # 10. 0x0085DA10: getTerrainBoundary2D
    elems_b = struct.pack("<6f", 10.0, 20.0, 30.5, 40.25, -5.0, 88.0)
    for idx in (0, 1, 2, 3, 10, 0xFFFFFFFF):
        emu.reset_data()
        a_elems = emu.alloc(elems_b)
        tl_b = bytearray(0x60)
        struct.pack_into("<2I", tl_b, 0x50, a_elems, a_elems + len(elems_b))
        a_tl = emu.alloc(bytes(tl_b))
        init_x = f32_to_bits(-999.0)
        init_y = f32_to_bits(-888.0)
        a_ox = emu.alloc(struct.pack("<I", init_x))
        a_oy = emu.alloc(struct.pack("<I", init_y))
        res = emu.call(0x0085DA10, [idx & 0xFFFFFFFF, a_ox, a_oy], ecx=a_tl)
        assert res.stack_cleanup == 12
        game_ok = res.eax & 0xFF
        game_x = struct.unpack("<I", emu.read(a_ox, 4))[0]
        game_y = struct.unpack("<I", emu.read(a_oy, 4))[0]
        ours_ok_s, ours_x_s, ours_y_s = cpp.call(
            f"boundary2d {elems_b.hex()} {idx & 0xFFFFFFFF} {init_x} {init_y}"
        ).split()
        assert game_ok == int(ours_ok_s)
        assert game_x == int(ours_x_s) and game_y == int(ours_y_s)
        bump("0x0085DA10_getTerrainBoundary2D")

    # 11. 0x0085DA50: lineInRegion2D
    line_cases = [
        ((-10.0, 50.0), (110.0, 50.0), (0.0, 0.0, 100.0, 100.0)),
        ((50.0, -10.0), (50.0, 110.0), (0.0, 0.0, 100.0, 100.0)),
        ((-20.0, -20.0), (120.0, 120.0), (0.0, 0.0, 100.0, 100.0)),
        ((-20.0, 90.0), (90.0, -20.0), (0.0, 0.0, 100.0, 100.0)),
        ((-20.0, -10.0), (-5.0, -2.0), (0.0, 0.0, 100.0, 100.0)),
        ((10.0, 10.0), (20.0, 20.0), (0.0, 0.0, 100.0, 100.0)),
        ((float("nan"), 10.0), (20.0, 20.0), (0.0, 0.0, 100.0, 100.0)),
    ]
    for _ in range(120):
        lx = rand_f(-100.0, 50.0)
        ly = rand_f(-100.0, 50.0)
        hx = lx + abs(rand_f(10.0, 150.0))
        hy = ly + abs(rand_f(10.0, 150.0))
        line_cases.append((
            (rand_f(-150.0, 200.0), rand_f(-150.0, 200.0)),
            (rand_f(-150.0, 200.0), rand_f(-150.0, 200.0)),
            (lx, ly, hx, hy),
        ))
    for p1, p2, reg in line_cases:
        emu.reset_data()
        p1_b = pack_coord2d(*p1)
        p2_b = pack_coord2d(*p2)
        reg_b = pack_region2d(*reg)
        a_p1 = emu.alloc(p1_b)
        a_p2 = emu.alloc(p2_b)
        a_reg = emu.alloc(reg_b)
        res = emu.call(0x0085DA50, [a_p1, a_p2, a_reg])
        assert res.stack_cleanup == 0
        game_ok = res.eax & 0xFF
        ours_ok = int(cpp.call(f"lineinregion2d {p1_b.hex()} {p2_b.hex()} {reg_b.hex()}"))
        assert game_ok == ours_ok
        bump("0x0085DA50_lineInRegion2D")

    # 12. 0x0085DD30: addWaypointLink
    for bi in (0, 1):
        for id1, id2 in ((10, 20), (10, 10), (10, 99), (20, 10)):
            emu.reset_data()
            wp1_b = bytearray(0x60)
            wp2_b = bytearray(0x60)
            a_wp1 = emu.alloc(bytes(wp1_b))
            a_wp2 = emu.alloc(bytes(wp2_b))
            struct.pack_into("<i", wp1_b, 0x04, 10)
            struct.pack_into("<I", wp1_b, 0x1C, a_wp2)
            wp1_b[0x5C] = bi
            struct.pack_into("<i", wp2_b, 0x04, 20)
            struct.pack_into("<I", wp2_b, 0x1C, 0)
            emu.write(a_wp1, bytes(wp1_b))
            emu.write(a_wp2, bytes(wp2_b))
            emu.write(GLOBAL_WAYPOINT_LIST_HEAD, struct.pack("<I", a_wp1))
            res = emu.call(0x0085DD30, [id1, id2], ecx=0)
            assert res.stack_cleanup == 8
            game_wp1 = emu.read(a_wp1, 0x60)
            game_wp2 = emu.read(a_wp2, 0x60)
            ours_wp1_h, ours_wp2_h = cpp.call(
                f"addwplink {bytes(wp1_b).hex()} {bytes(wp2_b).hex()} {id1} {id2} {a_wp1} {a_wp2}"
            ).split()
            assert game_wp1 == bytes.fromhex(ours_wp1_h)
            assert game_wp2 == bytes.fromhex(ours_wp2_h)
            bump("0x0085DD30_addWaypointLink")

    # 13 & 14. 0x0085DDF0 & 0x0085DE60: getBaseGroundHeight & getBaseLayerHeight
    for cw in CONTROL_WORDS:
        cpp.set_cw(cw)
        for has_norm in (0, 1):
            emu.reset_data()
            norm_init = pack_coord3d(3.0, 4.0, 5.0)
            a_norm = emu.alloc(norm_init) if has_norm else 0
            xb = f32_to_bits(12.5)
            yb = f32_to_bits(-7.25)
            res, game_fpu80 = emu.call_float80(
                0x0085DDF0, [xb, yb, a_norm], ecx=0x1000, fpu_control_word=cw
            )
            assert res.stack_cleanup == 12
            game_norm = emu.read(a_norm, 12) if has_norm else norm_init
            ours_fpu_h, ours_norm_h = cpp.call(
                f"basegroundheight {xb} {yb} {has_norm} {norm_init.hex()}"
            ).split()
            assert game_fpu80 == bytes.fromhex(ours_fpu_h)
            assert game_norm == bytes.fromhex(ours_norm_h)
            bump("0x0085DDF0_getBaseGroundHeight")

            for clip in (0, 1):
                emu.reset_data()
                a_norm2 = emu.alloc(norm_init) if has_norm else 0
                res2, game_fpu80_2 = emu.call_float80(
                    0x0085DE60, [xb, yb, 2, a_norm2, clip], ecx=0x1000, fpu_control_word=cw
                )
                assert res2.stack_cleanup == 20
                game_norm2 = emu.read(a_norm2, 12) if has_norm else norm_init
                ours_fpu_h2, ours_norm_h2 = cpp.call(
                    f"baselayerheight {xb} {yb} 2 {has_norm} {norm_init.hex()} {clip}"
                ).split()
                assert game_fpu80_2 == bytes.fromhex(ours_fpu_h2)
                assert game_norm2 == bytes.fromhex(ours_norm_h2)
                bump("0x0085DE60_getBaseLayerHeight")

    # Helper to build a fake TerrainLogic in Emulator with getExtent (+0x28),
    # getExtentIncludingBorder (+0x2C), getMaximumPathfindExtent (+0x38),
    # getGroundHeight (+0x18 -> 0x0085DDF0), getLayerHeight (+0x24 -> 0x0085DE60),
    # findClosestEdgePoint (+0x3C -> 0x0085DFA0).
        # Allocate once outside the loop so self._stubs doesn't grow infinitely
        pass

    ext_stub_data = bytearray(24)

    def stub_ext(e: Emulator) -> int:
        out_ptr = e.stack_arg(0)
        e.write(out_ptr, bytes(ext_stub_data))
        return out_ptr

    s_ext = emu.bind_stub(stub_ext, 4)
    s_g_or_w = emu.bind_stub(lambda e: (push_fpu_zero(e), 0)[1], 8)

    def setup_fake_terrain(ext_b: bytes, bridge_head: int = 0) -> int:
        ext_stub_data[:] = ext_b
        vt_b = bytearray(0x50)
        struct.pack_into("<I", vt_b, 0x18, 0x0085DDF0)
        struct.pack_into("<I", vt_b, 0x1C, 0x0085CA80)
        struct.pack_into("<I", vt_b, 0x20, s_g_or_w)
        struct.pack_into("<I", vt_b, 0x24, 0x0085DE60)
        struct.pack_into("<I", vt_b, 0x28, s_ext)
        struct.pack_into("<I", vt_b, 0x2C, s_ext)
        struct.pack_into("<I", vt_b, 0x38, s_ext)
        struct.pack_into("<I", vt_b, 0x3C, 0x0085DFA0)
        struct.pack_into("<I", vt_b, 0x40, 0x0085E3A0)
        struct.pack_into("<I", vt_b, 0x44, 0x0085E0B0)
        a_vt = emu.alloc(bytes(vt_b))
        tl_b = bytearray(0x90)
        struct.pack_into("<I", tl_b, 0x00, a_vt)
        struct.pack_into("<I", tl_b, 0x68, bridge_head)
        a_tl = emu.alloc(bytes(tl_b))
        emu.write(GLOBAL_THE_TERRAIN_LOGIC, struct.pack("<I", a_tl))
        return a_tl

    # 15, 16, 17, 19, 25: findClosestEdgePoint, clipLineToMapExtent,
    # findFarthestEdgePoint, clampPositionToMapExtent, getMaximumPathfindExtentCenter
    ext_b = pack_region3d(0.0, 0.0, 0.0, 200.0, 100.0, 50.0)
    for _ in range(40):
        emu.reset_data()
        a_tl = setup_fake_terrain(ext_b)
        out_init = pack_coord3d(-1.0, -2.0, -3.0)
        pt_b = pack_coord3d(rand_f(-50.0, 250.0), rand_f(-50.0, 150.0), rand_f(-10.0, 40.0))
        a_out = emu.alloc(out_init)
        a_pt = emu.alloc(pt_b)

        # 0x0085DFA0: findClosestEdgePoint
        res = emu.call(0x0085DFA0, [a_out, a_pt], ecx=a_tl)
        assert res.stack_cleanup == 8 and res.eax == a_out
        game_out = emu.read(a_out, 12)
        ours_out = bytes.fromhex(
            cpp.call(f"closestedge {out_init.hex()} {pt_b.hex()} {ext_b.hex()} 0")
        )
        assert game_out == ours_out
        bump("0x0085DFA0_findClosestEdgePoint")

        # 0x0085E3A0: findFarthestEdgePoint
        emu.write(a_out, out_init)
        res = emu.call(0x0085E3A0, [a_out, a_pt], ecx=a_tl)
        assert res.stack_cleanup == 8 and res.eax == a_out
        game_out = emu.read(a_out, 12)
        ours_out = bytes.fromhex(
            cpp.call(f"farthestedge {out_init.hex()} {pt_b.hex()} {ext_b.hex()} 0")
        )
        assert game_out == ours_out
        bump("0x0085E3A0_findFarthestEdgePoint")

        # 0x0085E840: clampPositionToMapExtent
        a_pos = emu.alloc(pt_b)
        res = emu.call(0x0085E840, [a_pos])
        assert res.stack_cleanup == 0
        game_ok = res.eax & 0xFF
        game_pos = emu.read(a_pos, 12)
        ours_ok_s, ours_pos_h = cpp.call(
            f"clamptoextent 1 {pt_b.hex()} {ext_b.hex()} 0"
        ).split()
        assert game_ok == int(ours_ok_s) and game_pos == bytes.fromhex(ours_pos_h)
        bump("0x0085E840_clampPositionToMapExtent")

        # 0x0085E0B0: clipLineToMapExtent
        p2_b = pack_coord3d(rand_f(-20.0, 220.0), rand_f(-20.0, 120.0), 0.0)
        a_p2 = emu.alloc(p2_b)
        emu.write(a_out, out_init)
        res = emu.call(0x0085E0B0, [a_out, a_pt, a_p2], ecx=a_tl)
        assert res.stack_cleanup == 12
        game_ok = res.eax & 0xFF
        game_out = emu.read(a_out, 12)
        ours_ok_s, ours_out_h = cpp.call(
            f"clipline {out_init.hex()} {pt_b.hex()} {p2_b.hex()} {ext_b.hex()} 0"
        ).split()
        assert game_ok == int(ours_ok_s) and game_out == bytes.fromhex(ours_out_h)
        bump("0x0085E0B0_clipLineToMapExtent")

    # Also test 0x0085E840 when TheTerrainLogic == nullptr
    emu.reset_data()
    emu.write(GLOBAL_THE_TERRAIN_LOGIC, struct.pack("<I", 0))
    pt_b = pack_coord3d(10.0, 20.0, 30.0)
    a_pos = emu.alloc(pt_b)
    res = emu.call(0x0085E840, [a_pos])
    assert (res.eax & 0xFF) == 0
    ours_ok_s, _ = cpp.call(f"clamptoextent 0 {pt_b.hex()} {ext_b.hex()} 0").split()
    assert int(ours_ok_s) == 0
    bump("0x0085E840_clampPositionToMapExtent")

    # 25. 0x008602F0: getMaximumPathfindExtentCenter
    for _ in range(20):
        emu.reset_data()
        r_ext = pack_region3d(
            rand_f(-100.0, 0.0), rand_f(-100.0, 0.0), rand_f(-50.0, 0.0),
            rand_f(10.0, 500.0), rand_f(10.0, 500.0), rand_f(10.0, 200.0),
        )
        a_tl = setup_fake_terrain(r_ext)
        out_init = pack_coord3d(0.0, 0.0, 0.0)
        a_out = emu.alloc(out_init)
        res = emu.call(0x008602F0, [a_out], ecx=a_tl)
        assert res.stack_cleanup == 4
        game_out = emu.read(a_out, 12)
        ours_out = bytes.fromhex(cpp.call(f"pathfindcenter {r_ext.hex()} {out_init.hex()}"))
        assert game_out == ours_out
        bump("0x008602F0_getMaximumPathfindExtentCenter")

    # 18. 0x0085E7F0: getTerrainBoundaryRegion
    iregs_b = struct.pack("<8i", 0, 0, 100, 200, -10, 25, 300, 450)
    for idx in (0, 1):
        emu.reset_data()
        a_iregs = emu.alloc(iregs_b)
        tl_b = bytearray(0x60)
        struct.pack_into("<I", tl_b, 0x54, a_iregs)
        a_tl = emu.alloc(bytes(tl_b))
        out_init = pack_region2d(-1.0, -1.0, -1.0, -1.0)
        a_out = emu.alloc(out_init)
        res = emu.call(0x0085E7F0, [a_out, idx], ecx=a_tl)
        assert res.stack_cleanup == 8
        game_out = emu.read(a_out, 16)
        ours_out = bytes.fromhex(
            cpp.call(f"boundaryregion {iregs_b.hex()} {idx} {out_init.hex()}")
        )
        assert game_out == ours_out
        bump("0x0085E7F0_getTerrainBoundaryRegion")

    # 20. 0x0085EC40: setPlaneFromPoints
    for cw in CONTROL_WORDS:
        cpp.set_cw(cw)
        plane_cases = [
            ((0.0, 0.0, 25.0), (10.0, 0.0, 25.0), (0.0, 10.0, 25.0)),
            ((0.0, 0.0, 0.0), (0.0, 0.0, 0.0), (0.0, 0.0, 0.0)),
            ((1.0, 2.0, 3.0), (2.0, 4.0, 6.0), (3.0, 6.0, 9.0)),
        ]
        for _ in range(30):
            plane_cases.append((
                (rand_f(), rand_f(), rand_f()),
                (rand_f(), rand_f(), rand_f()),
                (rand_f(), rand_f(), rand_f()),
            ))
        for v1, v2, v3 in plane_cases:
            emu.reset_data()
            pl_init = b"\xAA" * 16
            v1_b, v2_b, v3_b = pack_coord3d(*v1), pack_coord3d(*v2), pack_coord3d(*v3)
            a_pl = emu.alloc(pl_init)
            a_v1 = emu.alloc(v1_b)
            a_v2 = emu.alloc(v2_b)
            a_v3 = emu.alloc(v3_b)
            emu.uc.reg_write(unicorn_fpcw_reg(), cw)
            res = emu.call(0x0085EC40, [a_v1, a_v2, a_v3], ecx=a_pl)
            assert res.stack_cleanup == 12
            game_pl = emu.read(a_pl, 16)
            ours_pl = bytes.fromhex(
                cpp.call(f"planeset {pl_init.hex()} {v1_b.hex()} {v2_b.hex()} {v3_b.hex()}")
            )
            assert game_pl == ours_pl
            bump("0x0085EC40_setPlaneFromPoints")

    # 21, 22, 23, 24, 31, 32, 33: Bridge methods
    for cw in CONTROL_WORDS:
        cpp.set_cw(cw)
        for poly_ptr in (0, 0x11111111):
            for _ in range(20):
                emu.reset_data()
                emu.uc.reg_write(unicorn_fpcw_reg(), cw)
                br_b = pack_bridge(
                    (0.0, 0.0, 20.0),
                    (0.0, 40.0, 22.0),
                    (100.0, 0.0, 30.0),
                    (100.0, 40.0, 32.0),
                    (0.0, 0.0, 100.0, 40.0),
                    layer=2,
                    poly_ptr=poly_ptr,
                )
                a_br = emu.alloc(br_b)
                loc_b = pack_coord3d(rand_f(-20.0, 120.0), rand_f(-20.0, 60.0), rand_f(0.0, 40.0))
                a_loc = emu.alloc(loc_b)

                # 0x0085FA50: projectPointAlongBridgeSpan
                res, game_fpu80 = emu.call_float80(
                    0x0085FA50, [a_loc], ecx=a_br, fpu_control_word=cw
                )
                assert res.stack_cleanup == 4
                ours_fpu80 = bytes.fromhex(cpp.call(f"bridgeproject {br_b.hex()} {loc_b.hex()}"))
                assert game_fpu80 == ours_fpu80
                bump("0x0085FA50_projectPointAlongBridgeSpan")

                # 0x0085FAD0, 0x0085FD60, 0x00860020: cell queries
                cx = rand_f(-25.0, 125.0)
                cy = rand_f(-25.0, 65.0)
                cell_b = pack_region2d(cx - 5.0, cy - 5.0, cx + 5.0, cy + 5.0)
                a_cell = emu.alloc(cell_b)

                emu.uc.reg_write(unicorn_fpcw_reg(), cw)
                res_end = emu.call(0x0085FAD0, [a_cell], ecx=a_br)
                assert res_end.stack_cleanup == 4
                ours_end = int(cpp.call(f"bridgecellend {br_b.hex()} {cell_b.hex()}"))
                assert (res_end.eax & 0xFF) == ours_end
                bump("0x0085FAD0_isCellOnBridgeEnd")

                emu.uc.reg_write(unicorn_fpcw_reg(), cw)
                res_side = emu.call(0x0085FD60, [a_cell], ecx=a_br)
                assert res_side.stack_cleanup == 4
                ours_side = int(cpp.call(f"bridgecellside {br_b.hex()} {cell_b.hex()}"))
                assert (res_side.eax & 0xFF) == ours_side
                bump("0x0085FD60_isCellOnBridgeSide")

                init_h = f32_to_bits(-777.0)
                a_oh = emu.alloc(struct.pack("<I", init_h))
                emu.uc.reg_write(unicorn_fpcw_reg(), cw)
                res_ent = emu.call(0x00860020, [a_cell, a_oh], ecx=a_br)
                assert res_ent.stack_cleanup == 8
                game_ent_ok = res_ent.eax & 0xFF
                game_ent_h = struct.unpack("<I", emu.read(a_oh, 4))[0]
                ours_ent_ok_s, ours_ent_h_s = cpp.call(
                    f"bridgecellentry {br_b.hex()} {cell_b.hex()} {init_h}"
                ).split()
                assert game_ent_ok == int(ours_ent_ok_s)
                assert game_ent_h == int(ours_ent_h_s)
                bump("0x00860020_isCellBridgeEntryPoint")

                # 0x00861680: getBridgeHeight
                for has_norm in (0, 1):
                    norm_init = pack_coord3d(-1.0, -2.0, -3.0)
                    a_norm = emu.alloc(norm_init) if has_norm else 0
                    res_bh, game_bh80 = emu.call_float80(
                        0x00861680, [a_loc, a_norm], ecx=a_br, fpu_control_word=cw
                    )
                    assert res_bh.stack_cleanup == 8
                    game_norm = emu.read(a_norm, 12) if has_norm else norm_init
                    ours_bh80_h, ours_norm_h = cpp.call(
                        f"bridgeheight {br_b.hex()} {loc_b.hex()} {has_norm} {norm_init.hex()}"
                    ).split()
                    assert game_bh80 == bytes.fromhex(ours_bh80_h)
                    assert game_norm == bytes.fromhex(ours_norm_h)
                    bump("0x00861680_getBridgeHeight")

    # 31 & 32. 0x00861450 (isPointOnIntactBridge) & 0x008614B0 (pickBridge)
    for cw in CONTROL_WORDS:
        cpp.set_cw(cw)
        spans_b = struct.pack("<2f", 40.0, 60.0)
        for use_spans in (False, True):
            for _ in range(25):
                emu.reset_data()
                emu.uc.reg_write(unicorn_fpcw_reg(), cw)
                if use_spans:
                    a_sp = emu.alloc(spans_b)
                    sp_begin, sp_end = a_sp, a_sp + len(spans_b)
                    sp_hex = spans_b.hex()
                else:
                    sp_begin, sp_end = 0, 0
                    sp_hex = "."
                br_b = pack_bridge(
                    (0.0, 0.0, 20.0),
                    (0.0, 40.0, 20.0),
                    (100.0, 0.0, 20.0),
                    (100.0, 40.0, 20.0),
                    (0.0, 0.0, 100.0, 40.0),
                    layer=2,
                    poly_ptr=0,
                    span_begin=sp_begin,
                    span_end=sp_end,
                )
                a_br = emu.alloc(br_b)
                loc_b = pack_coord3d(rand_f(-10.0, 110.0), rand_f(-10.0, 50.0), 20.0)
                a_loc = emu.alloc(loc_b)
                res_int = emu.call(0x00861450, [a_loc], ecx=a_br)
                assert res_int.stack_cleanup == 4
                ours_int = int(
                    cpp.call(f"pointonintactbridge {br_b.hex()} {sp_hex} {loc_b.hex()}")
                )
                assert (res_int.eax & 0xFF) == ours_int
                bump("0x00861450_isPointOnIntactBridge")

                from_b = pack_coord3d(rand_f(-10.0, 110.0), rand_f(-10.0, 50.0), 60.0)
                to_b = pack_coord3d(rand_f(-10.0, 110.0), rand_f(-10.0, 50.0), -20.0)
                out_init = pack_coord3d(-9.0, -9.0, -9.0)
                a_from = emu.alloc(from_b)
                a_to = emu.alloc(to_b)
                a_out = emu.alloc(out_init)
                emu.uc.reg_write(unicorn_fpcw_reg(), cw)
                res_pk = emu.call(0x008614B0, [a_from, a_to, a_out], ecx=a_br)
                assert res_pk.stack_cleanup == 12
                game_pk_ok = res_pk.eax & 0xFF
                game_pk_out = emu.read(a_out, 12)
                ours_pk_ok_s, ours_pk_out_h = cpp.call(
                    f"pickbridge {br_b.hex()} {sp_hex} {from_b.hex()} {to_b.hex()} {out_init.hex()}"
                ).split()
                assert game_pk_ok == int(ours_pk_ok_s)
                assert game_pk_out == bytes.fromhex(ours_pk_out_h)
                bump("0x008614B0_pickBridge")

    # 26. 0x008603B0: makeAlignToNormalMatrix
    for cw in CONTROL_WORDS:
        cpp.set_cw(cw)
        for _ in range(30):
            emu.reset_data()
            emu.uc.reg_write(unicorn_fpcw_reg(), cw)
            ang = rand_f(-6.28, 6.28)
            ang_b = f32_to_bits(ang)
            pos_b = pack_coord3d(rand_f(), rand_f(), rand_f())
            nx, ny, nz = rand_f(-1.0, 1.0), rand_f(-1.0, 1.0), rand_f(0.1, 1.0)
            nlen = math.sqrt(nx * nx + ny * ny + nz * nz)
            norm_b = pack_coord3d(nx / nlen, ny / nlen, nz / nlen)
            mtx_init = b"\x55" * 48
            a_pos = emu.alloc(pos_b)
            a_norm = emu.alloc(norm_b)
            a_mtx = emu.alloc(mtx_init)
            res = emu.call(0x008603B0, [ang_b, a_pos, a_norm, a_mtx])
            assert res.stack_cleanup == 0
            game_mtx = emu.read(a_mtx, 48)
            ours_mtx = bytes.fromhex(
                cpp.call(f"alignmatrix {ang_b} {pos_b.hex()} {norm_b.hex()} {mtx_init.hex()}")
            )
            assert game_mtx == ours_mtx
            bump("0x008603B0_makeAlignToNormalMatrix")

    # 27 & 28. 0x00860630 (findBridgeAt) & 0x00860660 (findBridgeLayerAt)
    for _ in range(30):
        emu.reset_data()
        b2_b = pack_bridge(
            (100.0, 0.0, 20.0), (100.0, 40.0, 20.0),
            (200.0, 0.0, 20.0), (200.0, 40.0, 20.0),
            (100.0, 0.0, 200.0, 40.0), layer=3, next_ptr=0
        )
        a_b2 = emu.alloc(b2_b)
        b1_b = pack_bridge(
            (0.0, 0.0, 20.0), (0.0, 40.0, 20.0),
            (80.0, 0.0, 20.0), (80.0, 40.0, 20.0),
            (0.0, 0.0, 80.0, 40.0), layer=2, next_ptr=a_b2
        )
        a_b1 = emu.alloc(b1_b)
        a_tl = setup_fake_terrain(ext_b, bridge_head=a_b1)
        loc_b = pack_coord3d(rand_f(-20.0, 220.0), rand_f(-10.0, 50.0), 20.0)
        a_loc = emu.alloc(loc_b)

        res1 = emu.call(0x00860630, [a_loc], ecx=a_tl)
        assert res1.stack_cleanup == 4
        game_idx1 = 1 if res1.eax == a_b1 else (2 if res1.eax == a_b2 else 0)
        ours_idx1 = int(cpp.call(f"findbridgeat {b1_b.hex()} {b2_b.hex()} {loc_b.hex()}"))
        assert game_idx1 == ours_idx1
        bump("0x00860630_findBridgeAt")

        for layer in (1, 2, 3, 16):
            for clip in (0, 1):
                res2 = emu.call(0x00860660, [a_loc, layer, clip], ecx=a_tl)
                assert res2.stack_cleanup == 12
                game_idx2 = 1 if res2.eax == a_b1 else (2 if res2.eax == a_b2 else 0)
                ours_idx2 = int(
                    cpp.call(
                        f"findbridgelayerat {b1_b.hex()} {b2_b.hex()} {loc_b.hex()} {layer} {clip}"
                    )
                )
                assert game_idx2 == ours_idx2
                bump("0x00860660_findBridgeLayerAt")

    # 29 & 30. 0x00860A20 & 0x00860A30: Waypoint biDirectional accessors
    for bi in (0, 1):
        emu.reset_data()
        wp_raw = rng.randbytes(0x60)
        a_wp = emu.alloc(wp_raw)
        res_s = emu.call(0x00860A20, [bi], ecx=a_wp)
        assert res_s.stack_cleanup == 4
        game_wp = emu.read(a_wp, 0x60)
        ours_wp = bytes.fromhex(cpp.call(f"wpsetbi {wp_raw.hex()} {bi}"))
        assert game_wp == ours_wp
        bump("0x00860A20_setWaypointBiDirectional")

        res_g = emu.call(0x00860A30, [], ecx=a_wp)
        assert res_g.stack_cleanup == 0
        ours_g = int(cpp.call(f"wpgetbi {game_wp.hex()}"))
        assert (res_g.eax & 0xFF) == ours_g
        bump("0x00860A30_getWaypointBiDirectional")

    # The getter returns the stored byte as-is, so non-boolean bytes must survive.
    for raw_flag in (0x02, 0x80, 0xFF):
        emu.reset_data()
        wp_raw = bytearray(rng.randbytes(0x60))
        wp_raw[0x5C] = raw_flag
        a_wp = emu.alloc(bytes(wp_raw))
        res_g = emu.call(0x00860A30, [], ecx=a_wp)
        assert res_g.stack_cleanup == 0
        assert emu.read(a_wp, 0x60) == bytes(wp_raw)
        ours_g = int(cpp.call(f"wpgetbi {bytes(wp_raw).hex()}"))
        assert (res_g.eax & 0xFF) == ours_g == raw_flag
        bump("0x00860A30_getWaypointBiDirectional")

    # 34, 35, 36, 38. Layer destination queries & alignOnTerrain
    for cw in CONTROL_WORDS:
        cpp.set_cw(cw)
        spans_b = struct.pack("<2f", 40.0, 60.0)
        for _ in range(25):
            emu.reset_data()
            emu.uc.reg_write(unicorn_fpcw_reg(), cw)
            a_sp = emu.alloc(spans_b)
            br_b = pack_bridge(
                (0.0, 0.0, 20.0), (0.0, 40.0, 20.0),
                (100.0, 0.0, 20.0), (100.0, 40.0, 20.0),
                (0.0, 0.0, 100.0, 40.0),
                layer=2, poly_ptr=0, span_begin=a_sp, span_end=a_sp + len(spans_b),
            )
            a_br = emu.alloc(br_b)
            a_tl = setup_fake_terrain(ext_b, bridge_head=a_br)
            # Stub 0x00634A50 (wall check) -> returns 0, cdecl
            emu.bind_function(0x00634A50, lambda _e: 0, stack_cleanup=0)

            pos_b = pack_coord3d(rand_f(-10.0, 110.0), rand_f(-10.0, 50.0), rand_f(-5.0, 25.0))
            a_pos = emu.alloc(pos_b)

            # 0x00861CB0: getLayerForDestination
            res_l = emu.call(0x00861CB0, [0, a_pos], ecx=a_tl)
            assert res_l.stack_cleanup == 8
            ours_l = int(
                cpp.call(f"layerfordest {br_b.hex()} {spans_b.hex()} {pos_b.hex()} 0")
            )
            assert res_l.eax == ours_l
            bump("0x00861CB0_getLayerForDestination")

            # 0x00861E30 & 0x00861F40
            for only_h in (0, 1):
                res_hl = emu.call(0x00861E30, [a_pos, only_h], ecx=a_tl)
                assert res_hl.stack_cleanup == 8
                ours_hl = int(
                    cpp.call(
                        f"highestlayerfordest {br_b.hex()} {spans_b.hex()} {pos_b.hex()} {only_h} 0"
                    )
                )
                assert res_hl.eax == ours_hl
                bump("0x00861E30_getHighestLayerForDestination")

                res_hbl = emu.call(0x00861F40, [a_pos, only_h], ecx=a_tl)
                assert res_hbl.stack_cleanup == 8
                ours_hbl = int(
                    cpp.call(
                        f"highestbridgelayerfordest {br_b.hex()} {spans_b.hex()} {pos_b.hex()} {only_h} 0"
                    )
                )
                assert res_hbl.eax == ours_hbl
                bump("0x00861F40_getHighestBridgeLayerForDestination")

            # 0x00863F00: alignOnTerrain (with base getLayerHeight -> 0.0f and normal=(0,0,0))
            ang_b = f32_to_bits(0.5)
            mtx_init = b"\x33" * 48
            a_mtx = emu.alloc(mtx_init)
            norm_zero = pack_coord3d(0.0, 0.0, 0.0)
            for stick in (0, 1):
                emu.write(a_mtx, mtx_init)
                emu.uc.reg_write(unicorn_fpcw_reg(), cw)
                res_al = emu.call(0x00863F00, [ang_b, a_pos, stick, a_mtx], ecx=a_tl)
                assert res_al.stack_cleanup == 16
                game_mtx = emu.read(a_mtx, 48)
                ours_al_l_s, ours_al_mtx_h = cpp.call(
                    f"alignonterrain {br_b.hex()} {spans_b.hex()} {ang_b} {pos_b.hex()} {stick} {mtx_init.hex()} 0 0 {norm_zero.hex()}"
                ).split()
                assert res_al.eax == int(ours_al_l_s)
                assert game_mtx == bytes.fromhex(ours_al_mtx_h)
                bump("0x00863F00_alignOnTerrain")

    # 37 & 39. 0x008627F0 (getFirstWaterTableHeight) & 0x00864190 (updateCachedWaterGridHeight)
    for cw in CONTROL_WORDS:
        cpp.set_cw(cw)
        for count in (0, 1, 2):
            for raw_h in (-25, 0, 42, 300):
                emu.reset_data()
                target_b = bytearray(0x74)
                struct.pack_into("<i", target_b, 0x70, raw_h)
                a_target = emu.alloc(bytes(target_b))
                pairs_b = struct.pack("<4I", 1, a_target, 2, a_target)
                a_pairs = emu.alloc(pairs_b)
                table_b = bytearray(0x20)
                struct.pack_into("<2I", table_b, 0x18, a_pairs, a_pairs + count * 8)
                a_table = emu.alloc(bytes(table_b))
                tl_b = bytearray(0x50)
                struct.pack_into("<I", tl_b, 0x30, a_table)
                init_bits = f32_to_bits(-123.0)
                struct.pack_into("<I", tl_b, 0x3C, init_bits)
                a_tl = emu.alloc(bytes(tl_b))

                res_fw, game_fw80 = emu.call_float80(
                    0x008627F0, [], ecx=a_tl, fpu_control_word=cw
                )
                assert res_fw.stack_cleanup == 0
                ours_fw80 = bytes.fromhex(cpp.call(f"firstwaterheight 1 {count} {raw_h}"))
                assert game_fw80 == ours_fw80
                bump("0x008627F0_getFirstWaterTableHeight")

                res_uw = emu.call(0x00864190, [], ecx=a_tl)
                assert res_uw.stack_cleanup == 0
                game_uw_bits = struct.unpack("<I", emu.read(a_tl + 0x3C, 4))[0]
                ours_uw_bits = int(
                    cpp.call(f"updatewatergrid 1 {count} {raw_h} {init_bits}")
                )
                assert game_uw_bits == ours_uw_bits
                bump("0x00864190_updateCachedWaterGridHeight")

    # 40. 0x008662B0: getTerrainLogicSubsystemName
    emu.reset_data()
    res_sn = emu.call(0x008662B0, [], ecx=0)
    assert res_sn.stack_cleanup == 0
    game_sn = emu.read(res_sn.eax, 13)
    ours_sn = bytes.fromhex(cpp.call("subsystemname"))
    assert game_sn == ours_sn
    bump("0x008662B0_getTerrainLogicSubsystemName")

    # 41. 0x008683F0: resolveWaypointLinks
    for _ in range(15):
        emu.reset_data()
        base_addr = 0x60050000

        def stub_lookup(e: Emulator, b=base_addr) -> int:
            wp_id = struct.unpack("<i", struct.pack("<I", e.stack_arg(0)))[0]
            return (b + wp_id * 0x100) if wp_id > 0 else 0

        emu.bind_function(0x008663B0, stub_lookup, stack_cleanup=4)
        emu.write(GLOBAL_THE_TERRAIN_LOGIC, struct.pack("<I", 0x1000))
        wp_b = bytearray(0x60)
        for i in range(8):
            val = 0x7FFFFFFF if rng.randrange(2) == 0 else rng.randrange(1, 20)
            struct.pack_into("<I", wp_b, 0x20 + i * 4, val)
        struct.pack_into(
            "<I", wp_b, 0x40, 0x7FFFFFFF if rng.randrange(2) == 0 else rng.randrange(1, 20)
        )
        struct.pack_into(
            "<I", wp_b, 0x44, 0x7FFFFFFF if rng.randrange(2) == 0 else rng.randrange(1, 20)
        )
        a_wp = emu.alloc(bytes(wp_b))
        res_rl = emu.call(0x008683F0, [], ecx=a_wp)
        assert res_rl.stack_cleanup == 0
        game_wp = emu.read(a_wp, 0x60)
        ours_wp = bytes.fromhex(cpp.call(f"resolvewplinks {bytes(wp_b).hex()} {base_addr}"))
        assert game_wp == ours_wp
        bump("0x008683F0_resolveWaypointLinks")

    # 42. 0x00868470: adjustNewMapWaypoints
    for _ in range(15):
        emu.reset_data()
        a_tl = setup_fake_terrain(ext_b)
        post_called = False

        def stub_post(_e: Emulator) -> int:
            nonlocal post_called
            post_called = True
            return 0

        emu.bind_function(0x00866BB0, stub_post, stack_cleanup=0)
        wp1_b = bytearray(0x60)
        wp2_b = bytearray(0x60)
        a_wp2 = emu.alloc(bytes(wp2_b))
        struct.pack_into("<3f", wp1_b, 0x0C, 10.0, 20.0, rand_f(-50.0, 50.0))
        struct.pack_into("<I", wp1_b, 0x1C, a_wp2)
        struct.pack_into("<3f", wp2_b, 0x0C, 30.0, 40.0, rand_f(-50.0, 50.0))
        struct.pack_into("<I", wp2_b, 0x1C, 0)
        a_wp1 = emu.alloc(bytes(wp1_b))
        emu.write(a_wp2, bytes(wp2_b))
        emu.write(GLOBAL_WAYPOINT_LIST_HEAD, struct.pack("<I", a_wp1))

        res_nm = emu.call(0x00868470, [0], ecx=a_tl)
        assert res_nm.stack_cleanup == 4
        game_wp1 = emu.read(a_wp1, 0x60)
        game_wp2 = emu.read(a_wp2, 0x60)
        ours_wp1_h, ours_wp2_h, ours_cb_s = cpp.call(
            f"newmapwps {bytes(wp1_b).hex()} {bytes(wp2_b).hex()} 0 {a_wp2}"
        ).split()
        assert game_wp1 == bytes.fromhex(ours_wp1_h)
        assert game_wp2 == bytes.fromhex(ours_wp2_h)
        assert int(post_called) == int(ours_cb_s)
        bump("0x00868470_adjustNewMapWaypoints")

    # 43. 0x008684C0: findPositionAround (certain source anchor TerrainLogic.cpp:3158)
    def stub_ceil(e: Emulator) -> int:
        lo = e.stack_arg(0)
        hi = e.stack_arg(1)
        dval = struct.unpack("<d", struct.pack("<2I", lo, hi))[0]
        cval = float(math.ceil(dval))
        push_fpu_float(e, cval)
        return 0

    s_ceil = emu.bind_stub(stub_ceil, 0)

    for cw in CONTROL_WORDS:
        cpp.set_cw(cw)
        for accept_idx in (-1, 0, 1, 2, 5, 12):
            for start_angle in (-99999.8984375, 0.0, 1.25):
                emu.reset_data()
                emu.uc.reg_write(unicorn_fpcw_reg(), cw)
                a_tl = setup_fake_terrain(ext_b)
                emu.write(GLOBAL_THE_PARTITION_MANAGER, struct.pack("<I", 0x2000))
                emu.write(IAT_CEIL, struct.pack("<I", s_ceil))
                emu.write(GLOBAL_RANDOM_SCALE, struct.pack("<I", 0x2F800000))

                call_counter = 0

                def stub_try_pos(e: Emulator, acc=accept_idx) -> int:
                    nonlocal call_counter
                    idx = call_counter
                    call_counter += 1
                    if idx == acc:
                        c_ptr = e.stack_arg(0)
                        dist_f = bits_to_f32(e.stack_arg(1))
                        ang_f = bits_to_f32(e.stack_arg(2))
                        r_ptr = e.stack_arg(4)
                        cx, cy, cz = struct.unpack("<3f", e.read(c_ptr, 12))
                        e.write(r_ptr, pack_coord3d(cx + dist_f, cy + ang_f, cz))
                        return 1
                    return 0

                emu.bind_function(0x00866760, stub_try_pos, stack_cleanup=0)

                rng_words = [rng.getrandbits(32) for _ in range(6)]
                rng_b = struct.pack("<6I", *rng_words)
                emu.write(GLOBAL_LOGIC_RANDOM, rng_b)

                opts_b = bytearray(0x44)
                struct.pack_into("<f", opts_b, 0x28, 0.0)
                struct.pack_into("<f", opts_b, 0x2C, 15.0)
                struct.pack_into("<f", opts_b, 0x30, start_angle)
                struct.pack_into("<f", opts_b, 0x34, 1.0e10)

                center_b = pack_coord3d(50.0, 50.0, 15.0)
                res_init = pack_coord3d(-1.0, -1.0, -1.0)
                a_center = emu.alloc(center_b)
                a_opts = emu.alloc(bytes(opts_b))
                a_res = emu.alloc(res_init)

                res_fp = emu.call(0x008684C0, [a_center, a_opts, a_res])
                assert res_fp.stack_cleanup == 0
                game_ok = res_fp.eax & 0xFF
                game_res = emu.read(a_res, 12)
                game_rng = emu.read(GLOBAL_LOGIC_RANDOM, 24)

                ours_ok_s, ours_res_h, ours_rng_h, ours_cnt_s = cpp.call(
                    f"findposaround {center_b.hex()} {bytes(opts_b).hex()} {res_init.hex()} {ext_b.hex()} {rng_b.hex()} {accept_idx}"
                ).split()
                assert game_ok == int(ours_ok_s)
                assert game_res == bytes.fromhex(ours_res_h)
                assert game_rng == bytes.fromhex(ours_rng_h)
                assert call_counter == int(ours_cnt_s)
                bump("0x008684C0_findPositionAround")

    # Deliberate mutation proof: enable mutation, verify mismatch is caught, restore and re-verify
    assert cpp.call("mutate 1") == "ok"
    emu.reset_data()
    base = 0x60010000
    res_mut = emu.call(0x0085C5E0, [3], ecx=base)
    ours_mut = int(cpp.call(f"boundaryvec {base} 3"))
    mutation_detected = res_mut.eax != ours_mut
    assert mutation_detected, "deliberate mutation was not detected by differential oracle"
    assert cpp.call("mutate 0") == "ok"
    ours_restored = int(cpp.call(f"boundaryvec {base} 3"))
    assert res_mut.eax == ours_restored, "mutation restore failed"

    cpp.close()
    return counts


def unicorn_fpcw_reg() -> int:
    from unicorn import x86_const as r
    return r.UC_X86_REG_FPCW


def push_fpu_zero(emu: Emulator) -> None:
    from unicorn import x86_const as r
    fpsw = emu.uc.reg_read(r.UC_X86_REG_FPSW)
    top = (fpsw >> 11) & 7
    new_top = (top - 1) & 7
    emu.uc.reg_write(r.UC_X86_REG_FPSW, (fpsw & ~(7 << 11)) | (new_top << 11))
    from emulate import FPU_REGISTERS
    emu.uc.reg_write(FPU_REGISTERS[new_top], (0, 0))


def push_fpu_float(emu: Emulator, val: float) -> None:
    from unicorn import x86_const as r
    from emulate import FPU_REGISTERS
    fpsw = emu.uc.reg_read(r.UC_X86_REG_FPSW)
    top = (fpsw >> 11) & 7
    new_top = (top - 1) & 7
    emu.uc.reg_write(r.UC_X86_REG_FPSW, (fpsw & ~(7 << 11)) | (new_top << 11))
    if val == 0.0:
        emu.uc.reg_write(FPU_REGISTERS[new_top], (0, 0))
        return
    sign = 1 if val < 0 else 0
    aval = abs(val)
    m, e = math.frexp(aval)
    mantissa = int(round(m * (1 << 64)))
    if mantissa >= (1 << 64):
        mantissa >>= 1
        e += 1
    exp15 = (e - 1 + 16383) & 0x7FFF
    se = (sign << 15) | exp15
    emu.uc.reg_write(FPU_REGISTERS[new_top], (mantissa, se))


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "executables",
        nargs="+",
        type=Path,
        help="Path(s) to ra3_terrainlogiccheck.exe (Debug and/or Release)",
    )
    args = parser.parse_args()
    for exe in args.executables:
        counts = verify_executable(exe)
        total = sum(counts.values())
        print(
            f"Verified {exe}: {len(counts)} functions, {total} scenarios, "
            "0 mismatches (deliberate mutation detected & restored)"
        )
        for fn, cnt in sorted(counts.items()):
            print(f"  {fn}: {cnt}")


if __name__ == "__main__":
    main()
