"""Inventory and Unicorn differential oracle for the RA3 1.0 Scripts.cpp cluster.

Sixty-four SecuROM-clean native bodies in the Scripts.cpp / ScriptEngine cluster
(centered on 0x0081F710..0x00821080 and anchored by source row 0x00820780 =
SageEngine\\source\\GameEngine\\gamelogic\\scriptengine\\Scripts.cpp) are inventoried
by address, size, and full-body CRC32. Only the 36 functions with complete C++
reconstructions in ra3::gamelogic are marked ``reversed``; the remaining 28
entries are kept as ``inventory-only``.

Every one of the 36 reconstructed functions is executed from the shipped
ra3_1.0.game bytes in Unicorn across adversarial edge cases and deterministic
random cases and compared against ra3_scripts82check. A deliberate semantic
mutation on 0x00804590 is verified to be detected and restored.
"""

from __future__ import annotations

import argparse
import csv
import random
import struct
import subprocess
import sys
import zlib
from pathlib import Path

from unicorn import x86_const as r

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from disasm import DEFAULT_EXE, Image  # noqa: E402
from emulate import Emulator  # noqa: E402


SOURCE = Path(__file__).resolve().parents[3]
LEDGER = SOURCE / "docs" / "analysis" / "ra3_1.0" / "function_ledger.tsv"
ANNOTATIONS = SOURCE / "docs" / "analysis" / "ra3_1.0" / "scripts82_annotations.tsv"
INVENTORY = SOURCE / "docs" / "analysis" / "ra3_1.0" / "scripts82_inventory.tsv"

ENTRIES: dict[int, tuple[int, int, bool, bool]] = {
    0x00804590: (68, 0xA6C4617A, True, True),
    0x00804780: (104, 0x33A1ABE4, True, True),
    0x008049A0: (73, 0x4B81251D, True, True),
    0x0081A3A0: (31, 0xAC6E416D, False, False),
    0x0081A3C0: (53, 0xA3A5158E, False, False),
    0x0081A400: (18, 0x6F2ED002, False, False),
    0x0081A420: (106, 0x2E18B822, True, False),
    0x0081A4A0: (45, 0xEB204A9A, True, False),
    0x0081A4D0: (65, 0x014F6440, True, False),
    0x0081A520: (52, 0x98879165, True, False),
    0x0081A560: (75, 0x9E634368, False, False),
    0x0081A5B0: (82, 0xB0153D66, True, False),
    0x0081A610: (82, 0x0FD1A43F, False, False),
    0x0081A700: (73, 0x805ABF6C, True, False),
    0x0081A750: (46, 0x7EBBBA12, True, True),
    0x0081AA40: (26, 0xE4E98D4D, True, True),
    0x0081AA60: (63, 0xA0E768F1, True, True),
    0x0081AAA0: (184, 0x18BBC6C7, True, True),
    0x0081AB60: (103, 0xEB16C86E, True, True),
    0x0081B130: (70, 0x0C9B3BE8, True, False),
    0x0081B600: (58, 0x4C8CE17C, False, False),
    0x0081BE00: (44, 0xB73D4987, True, False),
    0x0081BEC0: (37, 0x64E98E78, False, False),
    0x0081BEF0: (42, 0x6AC7114E, False, False),
    0x0081BF20: (88, 0xE6CC11E9, False, False),
    0x0081BF90: (82, 0x57EC88C4, True, False),
    0x0081BFF0: (97, 0xE1C7C578, True, False),
    0x0081C3E0: (95, 0x7DFF4396, True, False),
    0x0081C530: (134, 0x8E6EBBD5, False, False),
    0x0081DD80: (51, 0xB2FC04AD, False, False),
    0x0081DE20: (76, 0xF187D6FD, True, True),
    0x0081DE70: (204, 0xA8E2794A, True, False),
    0x0081DF50: (97, 0xF2232F06, True, True),
    0x0081DFC0: (61, 0xD097AEFE, True, False),
    0x0081E030: (57, 0xB7976532, False, False),
    0x0081F710: (156, 0x99894E3F, True, False),
    0x0081F7B0: (111, 0x6F859EB2, True, False),
    0x0081F820: (114, 0x64436FAD, False, False),
    0x0081F8A0: (91, 0x5F7520BE, True, False),
    0x0081F900: (242, 0x325F230D, False, False),
    0x0081FA00: (209, 0x7E52E431, True, False),
    0x0081FAE0: (90, 0x785D6B4D, True, False),
    0x0081FB40: (247, 0x04B1F285, True, False),
    0x0081FC40: (107, 0xF47B2AA5, True, False),
    0x0081FCB0: (47, 0x44AB8722, False, False),
    0x0081FCE0: (117, 0x0DB1E2AE, False, False),
    0x0081FD60: (43, 0x22AD90C5, True, False),
    0x0081FD90: (236, 0x7EB6247D, False, False),
    0x0081FE80: (616, 0x354E3B67, False, False),
    0x008200F0: (183, 0xB0FFA8DC, False, False),
    0x008201B0: (197, 0xABD47C69, True, False),
    0x00820280: (140, 0x1BE2C6B1, True, False),
    0x00820370: (337, 0xFDAB65A5, False, False),
    0x008204D0: (91, 0x97F8D437, False, False),
    0x00820530: (30, 0xC166B1B7, False, False),
    0x00820550: (102, 0x026CD74E, False, False),
    0x008205C0: (111, 0x1D9AC021, False, False),
    0x00820630: (317, 0xD32BF855, True, False),
    0x00820780: (992, 0xC9DBE9E5, False, False),
    0x00820B70: (239, 0x24AAE396, True, False),
    0x00820C60: (102, 0x2BE7CED8, False, False),
    0x00820CD0: (289, 0xBCCF4D3E, False, False),
    0x00820E00: (175, 0xBCE7AF8F, True, False),
    0x00821080: (1942, 0x26A4140C, False, False),
}


def encode_tok(s: str) -> str:
    return "<EMPTY>" if not s else s.replace(" ", "|")


def f2b(val: float) -> int:
    return struct.unpack("<I", struct.pack("<f", val))[0]


def u32(val: int) -> int:
    return val & 0xFFFFFFFF


def s32(val: int) -> int:
    return struct.unpack("<i", struct.pack("<I", val & 0xFFFFFFFF))[0]


def fnv1a32(text: bytes) -> int:
    h = 0x811C9DC5
    for ch in text:
        h = (((h * 0x01000193) & 0xFFFFFFFF) ^ ch) & 0xFFFFFFFF
    return h


class Checker:
    def __init__(self, executable: Path) -> None:
        self.process = subprocess.Popen(
            [str(executable), "--stream"],
            stdin=subprocess.PIPE,
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
            text=True,
            encoding="utf-8",
        )

    def ask(self, command: str) -> str:
        assert self.process.stdin is not None and self.process.stdout is not None
        self.process.stdin.write(command + "\n")
        self.process.stdin.flush()
        line = self.process.stdout.readline()
        if not line:
            err = self.process.stderr.read() if self.process.stderr else ""
            raise RuntimeError(f"checker terminated on {command!r}: {err}")
        out = line.strip()
        if out == "error":
            raise RuntimeError(f"checker reported error on {command!r}")
        return out

    def close(self) -> None:
        if self.process.poll() is None and self.process.stdin is not None:
            self.process.stdin.write("quit\n")
            self.process.stdin.flush()
            self.process.wait(timeout=5)


class ScriptsHarness:
    STATIC_EMPTY = 0x00BD282C

    def __init__(self, exe: Path) -> None:
        self.emu = Emulator(exe)
        self.last_freed: list[int] = []
        self.team_returned: str = ""
        self.waypoints: list[tuple[str, int]] = []
        self.ready_pairs: list[tuple[int, int]] = []
        self.reg_first: int = 0
        self._bind_services()

    def _read_cstr(self, addr: int) -> bytes:
        if addr == 0:
            return b""
        out = bytearray()
        while True:
            b = self.emu.read(addr, 1)[0]
            if b == 0:
                return bytes(out)
            out.append(b)
            addr += 1

    def alloc_ascii_block(self, text: bytes) -> int:
        if not text:
            return 0
        raw = struct.pack("<iHH", 1, len(text), len(text) + 1) + text + b"\x00"
        base = self.emu.alloc(raw)
        return base + 8

    def alloc_ascii_obj(self, text: bytes) -> int:
        data_ptr = self.alloc_ascii_block(text)
        return self.emu.alloc(struct.pack("<I", data_ptr))

    def read_ascii_obj(self, slot: int) -> str:
        data_ptr = struct.unpack("<I", self.emu.read(slot, 4))[0]
        if data_ptr == 0:
            return ""
        return self._read_cstr(data_ptr).decode("latin1")

    def _bind_services(self) -> None:
        emu = self.emu

        def mem_alloc(m: Emulator) -> int:
            size = m.stack_arg(0)
            return m.alloc(b"\x00" * max(size, 16))

        def mem_free(m: Emulator) -> int:
            self.last_freed.append(m.stack_arg(0))
            return 0

        def ascii_copy_ctor(m: Emulator) -> int:
            this_ptr = m.register(r.UC_X86_REG_ECX)
            other = m.stack_arg(0)
            s = self.read_ascii_obj(other).encode("latin1") if other else b""
            m.write(this_ptr, struct.pack("<I", self.alloc_ascii_block(s)))
            return this_ptr

        def ascii_release(m: Emulator) -> int:
            this_ptr = m.register(r.UC_X86_REG_ECX)
            m.write(this_ptr, struct.pack("<I", 0))
            return 0

        def ascii_assign_obj(m: Emulator) -> int:
            this_ptr = m.register(r.UC_X86_REG_ECX)
            other = m.stack_arg(0)
            s = self.read_ascii_obj(other).encode("latin1") if other else b""
            m.write(this_ptr, struct.pack("<I", self.alloc_ascii_block(s)))
            return this_ptr

        def ascii_set_cstr_len(m: Emulator) -> int:
            this_ptr = m.register(r.UC_X86_REG_ECX)
            cstr = m.stack_arg(0)
            length = s32(m.stack_arg(1))
            s = m.read(cstr, max(0, length)) if (cstr and length > 0) else b""
            m.write(this_ptr, struct.pack("<I", self.alloc_ascii_block(s)))
            return this_ptr

        def ascii_append_cstr_len(m: Emulator) -> int:
            this_ptr = m.register(r.UC_X86_REG_ECX)
            cstr = m.stack_arg(0)
            length = s32(m.stack_arg(1))
            cur = self.read_ascii_obj(this_ptr).encode("latin1")
            add = m.read(cstr, max(0, length)) if (cstr and length > 0) else b""
            m.write(this_ptr, struct.pack("<I", self.alloc_ascii_block(cur + add)))
            return this_ptr

        def ascii_substring_copy(m: Emulator) -> int:
            this_ptr = m.register(r.UC_X86_REG_ECX)
            src_obj = m.stack_arg(0)
            start = s32(m.stack_arg(1))
            count = s32(m.stack_arg(2))
            s = self.read_ascii_obj(src_obj).encode("latin1")
            sub = s[max(0, start) : max(0, start + count)]
            m.write(this_ptr, struct.pack("<I", self.alloc_ascii_block(sub)))
            return this_ptr

        def get_team_named(m: Emulator) -> int:
            esp = m.register(r.UC_X86_REG_ESP)
            self.team_returned = self.read_ascii_obj(esp + 4)
            return 0x70001234 if self.team_returned else 0

        def register_cmd_ready(m: Emulator) -> int:
            gl = struct.unpack("<I", m.read(0x00DEBA64, 4))[0]
            flag_a8 = m.read(gl + 0xA8, 1)[0] if gl else 0
            if flag_a8 == 0:
                self.reg_first = 1
            return 0

        def query_cmd_ready(m: Emulator) -> int:
            a = m.stack_arg(0)
            b = m.stack_arg(1)
            return 1 if (a, b) in self.ready_pairs else 0

        def find_waypoint(m: Emulator) -> int:
            name_slot = m.stack_arg(0)
            nm = self.read_ascii_obj(name_slot)
            for wp_name, wp_id in self.waypoints:
                if wp_name == nm:
                    return m.alloc(struct.pack("<II", 0, u32(wp_id)))
            return 0

        def fast_hash_cstr(m: Emulator) -> int:
            this_ptr = m.register(r.UC_X86_REG_ECX)
            cstr = m.stack_arg(0)
            s = self._read_cstr(cstr)
            h = fnv1a32(s)
            m.write(this_ptr, struct.pack("<II", h, h))
            return this_ptr

        def trigger_inside(m: Emulator) -> int:
            this_ptr = m.register(r.UC_X86_REG_ECX)
            return m.read(this_ptr, 1)[0]

        for addr, fn, cleanup in (
            (0x004165A0, mem_alloc, 0),
            (0x004166B0, mem_free, 0),
            (0x004CA8F0, ascii_copy_ctor, 4),
            (0x004CA930, ascii_release, 0),
            (0x004CA9B0, ascii_assign_obj, 4),
            (0x004CB690, ascii_set_cstr_len, 8),
            (0x004CB6E0, ascii_append_cstr_len, 8),
            (0x004CC8C0, ascii_substring_copy, 12),
            (0x004CC0B0, ascii_substring_copy, 12),
            (0x0081FE80, get_team_named, 8),
            (0x005BA210, register_cmd_ready, 12),
            (0x005BA2F0, query_cmd_ready, 8),
            (0x00861A70, find_waypoint, 4),
            (0x00482300, fast_hash_cstr, 4),
            (0x0085CFA0, trigger_inside, 4),
        ):
            stub_addr = emu.bind_stub(fn, cleanup)
            emu.write(addr, b"\xE9" + struct.pack("<i", stub_addr - (addr + 5)))

    def reset(self) -> None:
        self.emu.reset_data()
        self.last_freed.clear()
        self.team_returned = ""
        self.waypoints.clear()
        self.ready_pairs.clear()
        self.reg_first = 0


def verify_inventory(exe_path: Path, checker: Checker) -> None:
    image = Image(exe_path)
    with LEDGER.open(encoding="utf-8") as f:
        ledger = {int(r["address"], 16): r for r in csv.DictReader(f, delimiter="\t")}
    with ANNOTATIONS.open(encoding="utf-8") as f:
        ann_rows = list(csv.DictReader(f, delimiter="\t"))
    with INVENTORY.open(encoding="utf-8") as f:
        inv_rows = list(csv.DictReader(f, delimiter="\t"))

    if len(ann_rows) != 36:
        raise RuntimeError(f"expected 36 rows in scripts82_annotations.tsv, got {len(ann_rows)}")
    if len(inv_rows) != 64:
        raise RuntimeError(f"expected 64 rows in scripts82_inventory.tsv, got {len(inv_rows)}")

    for va, (size, crc, _is_rev, _) in ENTRIES.items():
        row = ledger.get(va)
        if row is None:
            raise RuntimeError(f"missing ledger entry 0x{va:08X}")
        if int(row["size"]) != size:
            raise RuntimeError(f"size mismatch at 0x{va:08X}")
        if row["securom"] != "clean":
            raise RuntimeError(f"non-clean entry 0x{va:08X}: {row['securom']}")
        raw = image.read(va, size)
        actual_crc = zlib.crc32(raw) & 0xFFFFFFFF
        if actual_crc != crc:
            raise RuntimeError(f"CRC mismatch at 0x{va:08X}: {actual_crc:#x} != {crc:#x}")

    manifest_lines = subprocess.check_output(
        [checker.process.args[0], "inventory"], text=True, encoding="utf-8"
    ).strip().splitlines()
    if len(manifest_lines) != 64:
        raise RuntimeError(f"checker inventory count {len(manifest_lines)} != 64")


def run_differential_oracle(exe_path: Path, checker: Checker) -> int:
    h = ScriptsHarness(exe_path)
    emu = h.emu
    rng = random.Random(0x00820780)
    scenarios = 0

    def check(cmd: str, actual: str, label: str) -> None:
        nonlocal scenarios
        expected = checker.ask(cmd)
        if expected != actual:
            raise RuntimeError(
                f"mismatch in {label}: cmd={cmd!r} checker={expected!r} unicorn={actual!r}"
            )
        scenarios += 1

    # 1..3 & 36: 0x00804590, 0x00804780, 0x008049A0, 0x00820E00
    action_types = (
        list(range(-2, 420))
        + [0x1FFF, -100, 500, 0x7FFFFFFF, -0x80000000]
    )
    for t in action_types:
        h.reset()
        act = emu.alloc(struct.pack("<Ii", 0x00C35D44, t) + bytes(0x40))
        r1 = emu.call(0x00804590, ecx=act, eax=0xDEADBEEF)
        assert r1.stack_cleanup == 0
        check(f"unitaction {u32(t)}", str(r1.eax), "0x00804590")

        r2 = emu.call(0x00804780, ecx=act, eax=0xDEADBEEF)
        assert r2.stack_cleanup == 0
        check(f"teamaction {u32(t)}", str(r2.eax), "0x00804780")

        r3 = emu.call(0x008049A0, ecx=act, eax=0xDEADBEEF)
        assert r3.stack_cleanup == 0
        check(f"teamorunitaction {u32(t)}", str(r3.eax), "0x008049A0")

    for t in [-1, 0, 12, 33, 61, 62, 65, 146, 152, 210, 224, 282, 286, 352, 378, 385, 386, 400]:
        h.reset()
        eng = emu.alloc(bytes(0x100))
        emu.write(0x00DFC058, struct.pack("<I", eng))
        p0 = emu.alloc(bytes(0x10) + struct.pack("<I", h.alloc_ascii_block(b"TeamAlpha")) + bytes(0x1C))
        p1 = emu.alloc(bytes(0x10) + struct.pack("<I", h.alloc_ascii_block(b"TeamBeta")) + bytes(0x1C))
        act_buf = bytearray(0x48)
        struct.pack_into("<IiiII", act_buf, 0, 0x00C35D44, t, 2, p0, p1)
        act = emu.alloc(bytes(act_buf))
        res = emu.call(0x00820E00, ecx=act)
        assert res.stack_cleanup == 0
        inv = 1 if res.eax != 0 else 0
        idx = -1 if not inv else (1 if h.team_returned == "TeamBeta" else 0)
        check(
            f"teaminvolved {u32(t)} TeamAlpha TeamBeta",
            f"{idx} {inv} {encode_tok(h.team_returned)}",
            "0x00820E00",
        )

    # 4: 0x0081A420 ScriptEngine::canEvaluateScript
    for _ in range(40):
        h.reset()
        cur_bits = f2b(rng.choice([0.0, 1.5, -42.25, 100.0]))
        active = rng.randint(0, 1)
        easy, normal, hard = rng.randint(0, 1), rng.randint(0, 1), rng.randint(0, 1)
        eval_frame = rng.choice([0, 10, 50, 100])
        has_cp = rng.randint(0, 1)
        has_ai = rng.randint(0, 1) if has_cp else 0
        ai_diff = rng.choice([0, 1, 2, 3])
        eng_diff = rng.choice([0, 1, 2, 3])
        logic_frame = rng.choice([0, 9, 10, 50, 99, 100])

        s_buf = bytearray(0x50)
        s_buf[0x27] = easy
        s_buf[0x28] = normal
        s_buf[0x29] = hard
        struct.pack_into("<I", s_buf, 0x38, eval_frame)
        s_buf[0x3C] = active
        struct.pack_into("<I", s_buf, 0x48, cur_bits)
        s_ptr = emu.alloc(bytes(s_buf))

        eng_buf = bytearray(0x18400)
        if has_cp:
            ai_ptr = 0
            if has_ai:
                ai_b = bytearray(0x80)
                struct.pack_into("<i", ai_b, 0x3C, ai_diff)
                ai_ptr = emu.alloc(bytes(ai_b))
            pl_b = bytearray(0x100)
            struct.pack_into("<I", pl_b, 0x8C, ai_ptr)
            pl_ptr = emu.alloc(bytes(pl_b))
            struct.pack_into("<I", eng_buf, 0x17D88, pl_ptr)
        struct.pack_into("<i", eng_buf, 0x182D4, eng_diff)
        eng_ptr = emu.alloc(bytes(eng_buf))
        emu.write(0x00DFC058, struct.pack("<I", eng_ptr))

        gl_b = bytearray(0x80)
        struct.pack_into("<I", gl_b, 0x58, logic_frame)
        gl_ptr = emu.alloc(bytes(gl_b))
        emu.write(0x00DEBA64, struct.pack("<I", gl_ptr))

        res = emu.call(0x0081A420, [s_ptr], ecx=eng_ptr)
        assert res.stack_cleanup == 4
        out_cur = struct.unpack("<I", emu.read(s_ptr + 0x48, 4))[0]
        check(
            f"caneval {cur_bits} {active} {easy} {normal} {hard} {eval_frame} "
            f"{has_cp} {has_ai} {u32(ai_diff)} {u32(eng_diff)} {logic_frame}",
            f"{res.eax & 0xFF} {out_cur}",
            "0x0081A420",
        )

    # 5 & 8: 0x0081A4A0 clearTrackedObjectPointer & 0x0081A5B0 notifyOfObjectDestruction
    for _ in range(25):
        h.reset()
        cnt = rng.randint(0, 6)
        target = rng.choice([0, 0x1000, 0x2000, 0x3000])
        ptrs = [rng.choice([0, 0x1000, 0x2000, 0x3000]) for _ in range(cnt)]
        hashes = [rng.getrandbits(32) for _ in range(cnt)]
        arr_ptr = emu.alloc(
            b"".join(struct.pack("<II", hashes[i], ptrs[i]) for i in range(cnt))
            if cnt
            else b"\x00" * 8
        )
        eng_buf = bytearray(0x17E00)
        struct.pack_into("<II", eng_buf, 0x17D74, arr_ptr, arr_ptr + cnt * 8)
        eng_ptr = emu.alloc(bytes(eng_buf))
        res = emu.call(0x0081A4A0, [target], ecx=eng_ptr)
        assert res.stack_cleanup == 4
        out_ptrs = [
            str(struct.unpack("<I", emu.read(arr_ptr + i * 8 + 4, 4))[0])
            for i in range(cnt)
        ]
        args = " ".join(f"{ptrs[i]} {hashes[i]}" for i in range(cnt))
        check(
            f"cleartracked {target} {cnt} {args}".strip(),
            " ".join(out_ptrs),
            "0x0081A4A0",
        )

        h.reset()
        c70 = rng.choice([0, 0x1000, 0x2000])
        c68 = rng.choice([0, 0x1000, 0x2000])
        arr_ptr2 = emu.alloc(
            b"".join(struct.pack("<II", hashes[i], ptrs[i]) for i in range(cnt))
            if cnt
            else b"\x00" * 8
        )
        eng_buf2 = bytearray(0x17E00)
        struct.pack_into("<I", eng_buf2, 0x17D68, c68)
        struct.pack_into("<I", eng_buf2, 0x17D70, c70)
        struct.pack_into("<II", eng_buf2, 0x17D74, arr_ptr2, arr_ptr2 + cnt * 8)
        eng_ptr2 = emu.alloc(bytes(eng_buf2))
        res2 = emu.call(0x0081A5B0, [target], ecx=eng_ptr2)
        assert res2.stack_cleanup == 4
        o70 = struct.unpack("<I", emu.read(eng_ptr2 + 0x17D70, 4))[0]
        o68 = struct.unpack("<I", emu.read(eng_ptr2 + 0x17D68, 4))[0]
        out_p2 = [
            str(struct.unpack("<I", emu.read(arr_ptr2 + i * 8 + 4, 4))[0])
            for i in range(cnt)
        ]
        p_args = " ".join(str(p) for p in ptrs)
        check(
            f"notifydestroy {target} {c70} {c68} {cnt} {p_args}".strip(),
            " ".join([str(o70), str(o68)] + out_p2),
            "0x0081A5B0",
        )

    # 6 & 7: 0x0081A4D0 & 0x0081A520 setSequentialTimerForObject / Team
    for _ in range(25):
        h.reset()
        cnt = rng.randint(0, 5)
        obj_present = rng.randint(0, 1)
        obj_id = rng.choice([0, 7, 15, 42])
        frames = rng.choice([-1, 0, 15, 120])
        ids = [rng.choice([7, 15, 42, 99]) for _ in range(cnt)]
        waits = [rng.choice([-1, 5, 10]) for _ in range(cnt)]
        ptrs: list[int] = []
        for i in range(cnt):
            sb = bytearray(0x2C)
            struct.pack_into("<I", sb, 0x04, ids[i])
            struct.pack_into("<I", sb, 0x08, ids[i])
            struct.pack_into("<i", sb, 0x20, waits[i])
            ptrs.append(emu.alloc(bytes(sb)))
        arr_p = emu.alloc(b"".join(struct.pack("<I", p) for p in ptrs)) if cnt else 0
        eng_b = bytearray(0x80)
        struct.pack_into("<II", eng_b, 0x34, arr_p, arr_p + cnt * 4)
        eng_p = emu.alloc(bytes(eng_b))
        obj_p = 0
        if obj_present:
            ob = bytearray(0xE8)
            struct.pack_into("<I", ob, 0xDC, obj_id)
            obj_p = emu.alloc(bytes(ob))
        r_obj = emu.call(0x0081A4D0, [obj_p, u32(frames)], ecx=eng_p)
        assert r_obj.stack_cleanup == 8
        out_w = [str(struct.unpack("<i", emu.read(p + 0x20, 4))[0]) for p in ptrs]
        s_args = " ".join(f"{ids[i]} {u32(waits[i])}" for i in range(cnt))
        check(
            f"seqtimerobj {obj_present} {obj_id} {u32(frames)} {cnt} {s_args}".strip(),
            " ".join(out_w),
            "0x0081A4D0",
        )

        for i in range(cnt):
            emu.write(ptrs[i] + 0x20, struct.pack("<i", waits[i]))
        team_p = obj_id if obj_present else 0
        r_tm = emu.call(0x0081A520, [team_p, u32(frames)], ecx=eng_p)
        assert r_tm.stack_cleanup == 8
        out_w2 = [str(struct.unpack("<i", emu.read(p + 0x20, 4))[0]) for p in ptrs]
        check(
            f"seqtimerteam {team_p} {u32(frames)} {cnt} {s_args}".strip(),
            " ".join(out_w2),
            "0x0081A520",
        )

    # 9: 0x0081A700 ScriptList::markAllScriptsAndGroupsActive
    for _ in range(20):
        h.reset()
        sc = rng.randint(0, 4)
        gc = rng.randint(0, 4)
        s_nodes = [(i + 1 if i + 1 < sc else -1, rng.randint(0, 1)) for i in range(sc)]
        g_nodes = [(i + 1 if i + 1 < gc else -1, rng.randint(0, 1)) for i in range(gc)]
        first_s = 0 if sc > 0 else -1
        first_g = 0 if gc > 0 else -1
        s_arr = emu.alloc(
            b"".join(
                struct.pack("<iII", nxt, 0, 0) + bytes([flg, 0, 0, 0]) + bytes(4)
                for nxt, flg in s_nodes
            )
            if sc
            else bytes(20)
        )
        g_arr = emu.alloc(
            b"".join(
                struct.pack("<iII", nxt, 0, 0) + bytes([flg, 0, 0, 0]) + bytes(4)
                for nxt, flg in g_nodes
            )
            if gc
            else bytes(20)
        )
        sl_b = bytearray(0x64)
        struct.pack_into("<I", sl_b, 0x1C, s_arr)
        struct.pack_into("<i", sl_b, 0x30, first_s)
        struct.pack_into("<I", sl_b, 0x44, g_arr)
        struct.pack_into("<i", sl_b, 0x58, first_g)
        sl_p = emu.alloc(bytes(sl_b))
        r9 = emu.call(0x0081A700, ecx=sl_p)
        assert r9.stack_cleanup == 0
        out_flags = [str(emu.read(s_arr + i * 20 + 0x0C, 1)[0]) for i in range(sc)] + [
            str(emu.read(g_arr + i * 20 + 0x0C, 1)[0]) for i in range(gc)
        ]
        s_part = " ".join(f"{u32(n)} {f}" for n, f in s_nodes)
        g_part = " ".join(f"{u32(n)} {f}" for n, f in g_nodes)
        cmd = f"markactive {u32(first_s)} {sc} {s_part} {u32(first_g)} {gc} {g_part}"
        check(" ".join(cmd.split()), " ".join(out_flags), "0x0081A700")

    # 10: 0x0081A750 Parameter::getCoord3D
    for pt in [0, 0x0B, 0x10, 0x11, -1]:
        for coords in [(0.0, 0.0, 0.0), (12.5, -3.25, 100.0), (-0.0, 42.0, -99.5)]:
            h.reset()
            xb, yb, zb = f2b(coords[0]), f2b(coords[1]), f2b(coords[2])
            pb = bytearray(0x30)
            struct.pack_into("<i", pb, 0x00, pt)
            struct.pack_into("<III", pb, 0x14, xb, yb, zb)
            p_ptr = emu.alloc(bytes(pb))
            out_ptr = emu.alloc(struct.pack("<fff", 99.0, 99.0, 99.0))
            r10 = emu.call(0x0081A750, [out_ptr], ecx=p_ptr)
            assert r10.stack_cleanup == 4 and r10.eax == out_ptr
            ox, oy, oz = struct.unpack("<III", emu.read(out_ptr, 12))
            check(f"paramcoord3d {u32(pt)} {xb} {yb} {zb}", f"{ox} {oy} {oz}", "0x0081A750")

    # 11: 0x0081AA40 IntVector::sumElements
    for _ in range(25):
        h.reset()
        cnt = rng.randint(0, 8)
        vals = [rng.randint(-100000, 100000) for _ in range(cnt)]
        buf = emu.alloc(b"".join(struct.pack("<i", v) for v in vals) if cnt else b"\x00\x00\x00\x00")
        vec = emu.alloc(struct.pack("<II", buf, buf + cnt * 4))
        r11 = emu.call(0x0081AA40, [vec])
        assert r11.stack_cleanup == 0
        v_str = " ".join(str(u32(v)) for v in vals)
        check(f"intvecsum {cnt} {v_str}".strip(), str(r11.eax), "0x0081AA40")

    # 12, 13, 14, 31: 0x0081AA60, 0x0081AAA0, 0x0081AB60, 0x0081FD60 (SightingKey & SightingTree)
    sample_keys = [
        (0, 0, 0, 0),
        (1, 0, 1, 0),
        (1, 0, 2, 0),
        (1, 2, 3, 1),
        (2, 1, 3, 1),
        (5, 5, 0xFF, 0xFF),
        (5, 6, 0xFF, 0xFF),
    ]
    for a in sample_keys:
        for b in sample_keys:
            h.reset()
            pa = emu.alloc(struct.pack("<IIII", *a))
            pb = emu.alloc(struct.pack("<IIII", *b))
            r12 = emu.call(0x0081AA60, [pa, pb])
            assert r12.stack_cleanup == 8
            check(
                f"sightingless {a[0]} {a[1]} {a[2]} {a[3]} {b[0]} {b[1]} {b[2]} {b[3]}",
                str(r12.eax & 0xFF),
                "0x0081AA60",
            )

    for q in sample_keys:
        h.reset()
        cnt = len(sample_keys)
        node_ptrs = [
            emu.alloc(struct.pack("<IIIIIIII", 0, 0, 0, 0, *k)) for k in sample_keys
        ]
        for i in range(1, cnt):
            cur = 0
            while True:
                pa = emu.alloc(struct.pack("<IIII", *sample_keys[i]))
                pb = emu.alloc(struct.pack("<IIII", *sample_keys[cur]))
                less = emu.call(0x0081AA60, [pa, pb]).eax & 0xFF
                off = 0 if less else 8
                child = struct.unpack("<I", emu.read(node_ptrs[cur] + off, 4))[0]
                if child == 0:
                    emu.write(node_ptrs[cur] + off, struct.pack("<I", node_ptrs[i]))
                    break
                cur = node_ptrs.index(child)
        tree_ptr = emu.alloc(struct.pack("<IIII", 0, 0, 0, node_ptrs[0]))
        out_it = emu.alloc(bytes(4))
        q_ptr = emu.alloc(struct.pack("<IIII", *q))

        rf = emu.call(0x0081AAA0, [out_it, q_ptr], ecx=tree_ptr)
        assert rf.stack_cleanup == 8 and rf.eax == out_it
        found_node = struct.unpack("<I", emu.read(out_it, 4))[0]
        f_idx = -1 if found_node == tree_ptr + 4 else node_ptrs.index(found_node)

        rlb = emu.call(0x0081AB60, [out_it, q_ptr], ecx=tree_ptr)
        assert rlb.stack_cleanup == 8 and rlb.eax == out_it
        lb_node = struct.unpack("<I", emu.read(out_it, 4))[0]
        lb_idx = -1 if lb_node == tree_ptr + 4 else node_ptrs.index(lb_node)

        eng_s = bytearray(0x18350)
        struct.pack_into("<I", eng_s, 0x1833C, node_ptrs[0])
        eng_sp = emu.alloc(bytes(eng_s))
        rhs = emu.call(0x0081FD60, [q_ptr], ecx=eng_sp)
        assert rhs.stack_cleanup == 4

        k_args = " ".join(f"{k[0]} {k[1]} {k[2]} {k[3]}" for k in sample_keys)
        q_args = f"{q[0]} {q[1]} {q[2]} {q[3]}"
        check(f"sightingfind {q_args} {cnt} {k_args}", str(f_idx), "0x0081AAA0")
        check(f"sightinglb {q_args} {cnt} {k_args}", str(lb_idx), "0x0081AB60")
        check(f"hassighting {q_args} {cnt} {k_args}", str(rhs.eax & 0xFF), "0x0081FD60")

    # 15, 16, 19: Constructors 0x0081B130, 0x0081BE00, 0x0081C3E0
    for pt, iv in [(0, 0), (0x10, 42), (0x0B, -7)]:
        h.reset()
        p_mem = emu.alloc(b"\xAA" * 0x30)
        r15 = emu.call(0x0081B130, [u32(pt), u32(iv)], ecx=p_mem)
        assert r15.stack_cleanup == 8 and r15.eax == p_mem
        raw = emu.read(p_mem, 0x30)
        t_out, init_b = struct.unpack_from("<iB", raw, 0)
        iv_out, rv_out, str_p, cx, cy, cz, o0, o1, h0, h1 = struct.unpack_from(
            "<IIIIIIIIII", raw, 8
        )
        check(
            f"initparam {u32(pt)} {u32(iv)}",
            f"{t_out} {init_b} {s32(iv_out)} {rv_out} {1 if str_p == 0 else 0} "
            f"{cx} {cy} {cz} {o0} {o1} {h0} {h1}",
            "0x0081B130",
        )

    h.reset()
    seq_mem = emu.alloc(b"\xAA" * 0x2C)
    r16 = emu.call(0x0081BE00, ecx=seq_mem)
    assert r16.stack_cleanup == 0 and r16.eax == seq_mem
    s_raw = emu.read(seq_mem, 0x2C)
    vt, tm, oid, sn, f10, sseq, ci, ttl, ftw = struct.unpack_from("<IIIIIIiii", s_raw, 0)
    da = s_raw[0x24]
    nxt_s = struct.unpack_from("<I", s_raw, 0x28)[0]
    check(
        "initseq",
        f"{vt} {tm} {oid} {sn} {f10} {sseq} {ci} {ttl} {ftw} {da} {nxt_s}",
        "0x0081BE00",
    )

    h.reset()
    scr_mem = emu.alloc(b"\xAA" * 0x50)
    r19 = emu.call(0x0081C3E0, ecx=scr_mem)
    assert r19.stack_cleanup == 0 and r19.eax == scr_mem
    sc_raw = emu.read(scr_mem, 0x50)
    vt_s = struct.unpack_from("<I", sc_raw, 0x00)[0]
    ia = sc_raw[0x18]
    ios, isub, f26, ez, nm, hd = sc_raw[0x24:0x2A]
    fte = struct.unpack_from("<I", sc_raw, 0x38)[0]
    isar, f3d = sc_raw[0x3C], sc_raw[0x3D]
    ct, curt = struct.unpack_from("<II", sc_raw, 0x44)
    check(
        "initscript",
        f"{vt_s} {ia} {ios} {isub} {f26} {ez} {nm} {hd} {fte} {isar} {f3d} {ct} {curt}",
        "0x0081C3E0",
    )

    # 17: 0x0081BF90 ScriptCounterDefaultMap::lookupOrDefault
    # Note: 0x006C3A10 uses left=+0x00, right=+0x04, key=+0x10, val=+0x14!
    for has_tree in [0, 1]:
        for qk in [5, 10, 15, 20, 25]:
            h.reset()
            root_p = 0
            if has_tree:
                n1 = emu.alloc(struct.pack("<IIIIII", 0, 0, 0, 0, 20, 1234))
                n0 = emu.alloc(struct.pack("<IIIIiI", n1, 0, 0, 0, 10, u32(-50)))
                root_p = n0
            tree_obj = emu.alloc(struct.pack("<IIIIII", 0, 0, 0, root_p, 0, 2)) if has_tree else 0
            this_p = emu.alloc(struct.pack("<IIiI", 0, 0, -99, tree_obj))
            kh = emu.alloc(struct.pack("<II", 0, qk))
            r17, f80 = emu.call_float80(0x0081BF90, [0, kh, 0], ecx=this_p)
            assert r17.stack_cleanup == 12
            mant, exp_sign = struct.unpack("<QH", f80)
            if mant == 0 and exp_sign == 0:
                d_val = 0.0
            else:
                sign = -1.0 if (exp_sign & 0x8000) else 1.0
                exp = (exp_sign & 0x7FFF) - 16383
                d_val = sign * (mant / (1 << 63)) * (2.0**exp)
            d_bits = struct.unpack("<Q", struct.pack("<d", d_val))[0]
            check(
                f"counterdefault {u32(-99)} {has_tree} {qk} 2 10 {u32(-50)} 20 1234",
                str(d_bits),
                "0x0081BF90",
            )

    # 18: 0x0081BFF0 ScriptEngine::isTrackedObjectDestroyedByName
    for q_name in ["HeroTanya", "Tank01", "MissingUnit"]:
        for ptr_val in [0, 0x12345678]:
            h.reset()
            entries = [
                (fnv1a32(b"Tank01"), ptr_val),
                (fnv1a32(b"HeroTanya"), 0),
            ]
            arr_p = emu.alloc(b"".join(struct.pack("<II", hsh, p) for hsh, p in entries))
            eng_b = bytearray(0x17E00)
            struct.pack_into("<II", eng_b, 0x17D74, arr_p, arr_p + len(entries) * 8)
            eng_p = emu.alloc(bytes(eng_b))
            str_slot = h.alloc_ascii_obj(q_name.encode("latin1"))
            r18 = emu.call(0x0081BFF0, [str_slot], ecx=eng_p)
            assert r18.stack_cleanup == 4
            e_args = " ".join(f"{hsh} {p}" for hsh, p in entries)
            check(
                f"istrackeddestroyed {q_name} {len(entries)} {e_args}",
                str(r18.eax & 0xFF),
                "0x0081BFF0",
            )

    # 20: 0x0081DE20 ScriptIndexedEntry::lowerBoundById
    for qid in [0, 5, 10, 15, 20, 25]:
        h.reset()
        items = [(5, 100, 1), (10, 200, 2), (20, 300, 3)]
        arr_p = emu.alloc(b"".join(struct.pack("<iII", *it) for it in items))
        key_p = emu.alloc(struct.pack("<i", qid))
        r20 = emu.call(0x0081DE20, [arr_p, arr_p + len(items) * 12, key_p])
        assert r20.stack_cleanup == 0
        idx = (r20.eax - arr_p) // 12
        i_args = " ".join(f"{it[0]} {it[1]} {it[2]}" for it in items)
        check(f"indexedlb {qid} {len(items)} {i_args}", str(idx), "0x0081DE20")

    # 21: 0x0081DE70 ScriptListIndex::lowerBoundByName
    names = ["Alpha", "Bravo", "Charlie", "Delta"]
    for qn in ["AAA", "Alpha", "Beta", "Charlie", "Delta", "Zulu"]:
        h.reset()
        ent_buf = bytearray()
        for nm in names:
            s_ptr = h.alloc_ascii_block(nm.encode("latin1"))
            ent_buf += struct.pack("<IIIII", 0, 0, s_ptr, 1, 0)
        ent_arr = emu.alloc(bytes(ent_buf))
        idx_arr = emu.alloc(struct.pack("<iiii", 0, 1, 2, 3))
        this_p = emu.alloc(struct.pack("<IIIII", idx_arr, idx_arr + 16, 0, 0, ent_arr))
        q_obj = h.alloc_ascii_obj(qn.encode("latin1"))
        r21 = emu.call(0x0081DE70, [q_obj], ecx=this_p)
        assert r21.stack_cleanup == 4
        n_args = " ".join(f"{i} {names[i]}" for i in range(len(names)))
        check(f"listindexlb {qn} {len(names)} {n_args}", str(s32(r21.eax)), "0x0081DE70")

    # 22: 0x0081DF50 DualHashTree::find
    dh_keys = [(10, 100), (5, 200), (20, 50), (20, 150)]
    for qk in [(10, 100), (5, 200), (20, 150), (10, 999), (99, 100)]:
        h.reset()
        n_ptrs = [emu.alloc(struct.pack("<IIIIII", 0, 0, 0, 0, *k)) for k in dh_keys]
        for i in range(1, len(dh_keys)):
            cur = 0
            while True:
                less = dh_keys[i] < dh_keys[cur]
                off = 0 if less else 8
                child = struct.unpack("<I", emu.read(n_ptrs[cur] + off, 4))[0]
                if child == 0:
                    emu.write(n_ptrs[cur] + off, struct.pack("<I", n_ptrs[i]))
                    break
                cur = n_ptrs.index(child)
        tree_p = emu.alloc(struct.pack("<IIII", 0, 0, 0, n_ptrs[0]))
        out_it = emu.alloc(bytes(4))
        q_p = emu.alloc(struct.pack("<II", *qk))
        r22 = emu.call(0x0081DF50, [out_it, q_p], ecx=tree_p)
        assert r22.stack_cleanup == 8 and r22.eax == out_it
        fn = struct.unpack("<I", emu.read(out_it, 4))[0]
        f_idx = -1 if fn == tree_p + 4 else n_ptrs.index(fn)
        k_args = " ".join(f"{k[0]} {k[1]}" for k in dh_keys)
        check(f"dualhashfind {qk[0]} {qk[1]} {len(dh_keys)} {k_args}", str(f_idx), "0x0081DF50")

    # 23: 0x0081DFC0 ScriptWaypointInfo::copyFrom
    h.reset()
    s_str = h.alloc_ascii_block(b"WP_Start")
    src_b = struct.pack(
        "<IIIBxxxIfffI", 10, 20, 30, 1, s_str, 1.5, -2.5, 3.5, 99
    )
    src_p = emu.alloc(src_b)
    dst_p = emu.alloc(bytes(0x24))
    r23 = emu.call(0x0081DFC0, [src_p], ecx=dst_p)
    assert r23.stack_cleanup == 4 and r23.eax == dst_p
    w0, w4, w8, bc, w10, f14, f18, f1c, w20 = struct.unpack(
        "<IIIBxxxIIIII", emu.read(dst_p, 0x24)
    )
    check(
        f"waypointcopy 10 20 30 1 {s_str} {f2b(1.5)} {f2b(-2.5)} {f2b(3.5)} 99",
        f"{w0} {w4} {w8} {bc} {w10} {f14} {f18} {f1c} {w20}",
        "0x0081DFC0",
    )

    # 24: 0x0081F710 ScriptConditions::evaluateSkirmishCommandButtonIsReady
    btn_crc = zlib.crc32(b"Command_ConstructSovietPowerPlant") & 0xFFFFFFFF
    for cd_in in [0, 1]:
        for flag_a8 in [0, 1]:
            for has_match in [0, 1]:
                h.reset()
                cond_b = bytearray(0x50)
                struct.pack_into("<I", cond_b, 0x44, cd_in)
                cond_p = emu.alloc(bytes(cond_b))
                parm_p = emu.alloc(
                    bytes(0x10)
                    + struct.pack("<I", h.alloc_ascii_block(b"Command_ConstructSovietPowerPlant"))
                    + bytes(0x1C)
                )
                pairs = [(0x01D2592F, btn_crc if has_match else 0x11111111)]
                h.ready_pairs = list(pairs)
                gl_b = bytearray(0x100)
                gl_b[0xA8] = flag_a8
                gl_p = emu.alloc(bytes(gl_b))
                emu.write(0x00DEBA64, struct.pack("<I", gl_p))
                emu.write(0x00DF4C18, struct.pack("<I", emu.alloc(bytes(0x60))))
                r24 = emu.call(0x0081F710, [cond_p, parm_p])
                assert r24.stack_cleanup == 8
                cd_out = struct.unpack("<I", emu.read(cond_p + 0x44, 4))[0]
                p_args = " ".join(f"{a} {b}" for a, b in pairs)
                check(
                    f"skirmishcmdready {cd_in} Command_ConstructSovietPowerPlant "
                    f"{flag_a8} {len(pairs)} {p_args}",
                    f"{r24.eax & 0xFF} {cd_out} {h.reg_first}",
                    "0x0081F710",
                )

    # 25: 0x0081F7B0 ScriptConditions::evaluateBridgeBroken
    for q_wp in ["BridgeWP_1", "BridgeWP_2", "MissingWP"]:
        h.reset()
        h.waypoints = [("BridgeWP_1", 10), ("BridgeWP_2", 20)]
        emu.write(0x00DFC190, struct.pack("<I", emu.alloc(bytes(0x30))))
        # BridgeManager (0x00DFC040) RB-tree at +0x38 (root at +0x44, left=+0x00, right=+0x04, key=+0x10, state=+0x14)
        bn2 = emu.alloc(struct.pack("<IIIIii", 0, 0, 0, 0, 20, 2))
        bn1 = emu.alloc(struct.pack("<IIIIii", bn2, 0, 0, 0, 10, 1))
        bm_b = bytearray(0x50)
        struct.pack_into("<I", bm_b, 0x44, bn1)
        bm_p = emu.alloc(bytes(bm_b))
        emu.write(0x00DFC040, struct.pack("<I", bm_p))

        parm_p = emu.alloc(
            bytes(0x10)
            + struct.pack("<I", h.alloc_ascii_block(q_wp.encode("latin1")))
            + bytes(0x1C)
        )
        r25 = emu.call(0x0081F7B0, [parm_p])
        assert r25.stack_cleanup == 4
        check(
            f"bridgebroken {q_wp} 2 10 BridgeWP_1 20 BridgeWP_2 2 10 1 20 2",
            str(r25.eax & 0xFF),
            "0x0081F7B0",
        )

    # 26: 0x0081F8A0 ScriptConditions::evaluatePlatform
    for plat in ["PC", "pc", "360", "PS3", "Xbox", ""]:
        h.reset()
        parm_p = emu.alloc(
            bytes(0x10)
            + struct.pack("<I", h.alloc_ascii_block(plat.encode("latin1")))
            + bytes(0x1C)
        )
        r26 = emu.call(0x0081F8A0, [parm_p])
        assert r26.stack_cleanup == 4
        check(f"platform {encode_tok(plat)}", str(r26.eax & 0xFF), "0x0081F8A0")

    # 27 & 28: 0x0081FA00 findFlagByName & 0x0081FAE0 qualifyWithCurrentSidePrefix
    for cur_p in ["PlyrAllies", "PlyrSoviet", ""]:
        for raw_n in ["AttackFlag", "PlyrEmpire/DefenseFlag", "/RootFlag"]:
            h.reset()
            eng_b = bytearray(0x17E00)
            struct.pack_into(
                "<I", eng_b, 0x17D60, h.alloc_ascii_block(cur_p.encode("latin1"))
            )
            k_line = checker.ask(f"findflag {encode_tok(cur_p)} {encode_tok(raw_n)}")
            kh_p, kh_n = [int(x) for x in k_line.split()]
            node_p = emu.alloc(struct.pack("<IIIIIII", 0, 0, 0, 0, kh_p, kh_n, 1))
            struct.pack_into("<I", eng_b, 0x16CF0, node_p)
            eng_p = emu.alloc(bytes(eng_b))

            out_slot = h.alloc_ascii_obj(b"")
            raw_data_ptr = h.alloc_ascii_block(raw_n.encode("latin1"))
            r28 = emu.call(0x0081FAE0, [out_slot, raw_data_ptr], ecx=eng_p)
            assert r28.stack_cleanup == 8 and r28.eax == out_slot
            qual_out = h.read_ascii_obj(out_slot)
            check(
                f"qualifyside {encode_tok(cur_p)} {encode_tok(raw_n)}",
                encode_tok(qual_out),
                "0x0081FAE0",
            )

            raw_data_ptr2 = h.alloc_ascii_block(raw_n.encode("latin1"))
            r27 = emu.call(0x0081FA00, [raw_data_ptr2], ecx=eng_p)
            assert r27.stack_cleanup == 4 and r27.eax == node_p + 0x18
            scenarios += 1

    # 29: 0x0081FB40 ScriptEngine::findAttackInfo
    for cur_cnt, q_name, add_if in [
        (1, "Alpha", 0),
        (1, "Alpha", 1),
        (3, "Bravo", 0),
        (3, "Charlie", 1),
        (0x100, "Overflow", 1),
    ]:
        h.reset()
        eng_b = bytearray(0x17E00)
        existing = ["Alpha", "Bravo"]
        for idx, nm in enumerate(existing, start=1):
            if idx < cur_cnt:
                struct.pack_into(
                    "<I",
                    eng_b,
                    0x16D54 + idx * 16 + 4,
                    h.alloc_ascii_block(nm.encode("latin1")),
                )
        struct.pack_into("<i", eng_b, 0x17D54, cur_cnt)
        eng_p = emu.alloc(bytes(eng_b))
        q_slot = h.alloc_ascii_obj(q_name.encode("latin1"))
        r29 = emu.call(0x0081FB40, [q_slot, add_if], ecx=eng_p)
        assert r29.stack_cleanup == 8
        new_cnt = struct.unpack("<i", emu.read(eng_p + 0x17D54, 4))[0]
        idx_out = 0 if r29.eax == 0 else (r29.eax - (eng_p + 0x16D54)) // 16
        ex_count = max(0, min(len(existing), cur_cnt - 1))
        ex_args = " ".join(existing[:ex_count])
        check(
            f"findattack {cur_cnt} {q_name} {add_if} {ex_count} {ex_args}".strip(),
            f"{idx_out} {new_cnt}",
            "0x0081FB40",
        )

    # 30: 0x0081FC40 ScriptEngine::getObjectCount
    for p_valid in [0, 1]:
        for t_name in ["SovietHammerTank", "AlliedVindicator"]:
            h.reset()
            th = zlib.crc32(b"SovietHammerTank") & 0xFFFFFFFF
            n0 = emu.alloc(struct.pack("<IIIIII", 0, 0, 0, 0, th, 17))
            eng_b = bytearray(0x19000)
            base_off = (2 + 0xDA2) * 28
            struct.pack_into("<I", eng_b, base_off + 0x0C, n0)
            eng_p = emu.alloc(bytes(eng_b))

            pl_obj = bytearray(0x1300)
            if not p_valid:
                pl_obj[0x106] = 1
            pl_list = bytearray(0x60)
            struct.pack_into("<I", pl_list, 0x34 + 2 * 4, emu.alloc(bytes(pl_obj)))
            emu.write(0x00DFBD74, struct.pack("<I", emu.alloc(bytes(pl_list))))

            t_slot = h.alloc_ascii_obj(t_name.encode("latin1"))
            r30 = emu.call(0x0081FC40, [2, t_slot], ecx=eng_p)
            assert r30.stack_cleanup == 8
            check(
                f"objcount {p_valid} {t_name} 1 {th} 17",
                str(s32(r30.eax)),
                "0x0081FC40",
            )

    # 32 & 33: 0x008201B0 & 0x00820280 SpecialPower / ScienceOrUpgrade event lists
    for p_idx in [0, 2, 20]:
        for erase in [0, 1]:
            for match in [0, 1]:
                h.reset()
                ev_str = "SpecialPower_Orbital" if match else "OtherPower"
                ev_hash = fnv1a32(ev_str.encode("latin1"))
                q_hash = fnv1a32(b"SpecialPower_Orbital")
                sentinel = emu.alloc(struct.pack("<IIIIIIIBxxx", 0, 0, 0, 0, 0, 0, 0, 0))
                node1 = emu.alloc(
                    struct.pack(
                        "<IIIIfffBxxx",
                        sentinel,
                        sentinel,
                        ev_hash,
                        55,
                        0.0,
                        0.0,
                        0.0,
                        0,
                    )
                )
                emu.write(sentinel, struct.pack("<II", node1, node1))
                eng_b = bytearray(0x18400)
                if 0 <= p_idx <= 0x13:
                    off32 = (p_idx * 3 + 0x5FFA) * 4
                    off33 = (p_idx * 3 + 0x6036) * 4
                    struct.pack_into("<II", eng_b, off32, node1, node1)
                    struct.pack_into("<II", eng_b, off33, node1, node1)
                eng_p = emu.alloc(bytes(eng_b))
                if 0 <= p_idx <= 0x13:
                    # Point node1->next and prev to the embedded list head
                    head32 = eng_p + (p_idx * 3 + 0x5FFA) * 4
                    emu.write(node1, struct.pack("<II", head32, head32))

                q_slot = h.alloc_ascii_obj(b"SpecialPower_Orbital")
                r32 = emu.call(0x008201B0, [p_idx, q_slot, erase, 55, 0], ecx=eng_p)
                assert r32.stack_cleanup == 20
                erased32 = 1 if h.last_freed else 0
                check(
                    f"specialpower {p_idx} {q_hash} {erase} 55 0 1 "
                    f"{ev_hash} 55 0 0",
                    f"{r32.eax & 0xFF} {erased32}",
                    "0x008201B0",
                )

                h.last_freed.clear()
                if 0 <= p_idx <= 0x13:
                    head33 = eng_p + (p_idx * 3 + 0x6036) * 4
                    emu.write(head33, struct.pack("<II", node1, node1))
                    emu.write(node1, struct.pack("<II", head33, head33))
                r33 = emu.call(0x00820280, [p_idx, q_slot, erase], ecx=eng_p)
                assert r33.stack_cleanup == 12
                erased33 = 1 if h.last_freed else 0
                check(
                    f"scienceupgrade {p_idx} {q_hash} {erase} 1 {ev_hash}",
                    f"{r33.eax & 0xFF} {erased33}",
                    "0x00820280",
                )

    # 34: 0x00820630 Parameter::qualifyPlayerSide
    for pt in [0x0B, 0x10]:
        for s_in in [
            "<Local Player>",
            "<Local Player's Allies>",
            "<Local Player's Enemies>",
            "<Local Player's Neutral>",
            "<Local Player's Friendly>",
            "PlyrAllies",
        ]:
            h.reset()
            p_b = bytearray(0x30)
            struct.pack_into("<i", p_b, 0x00, pt)
            struct.pack_into("<I", p_b, 0x10, h.alloc_ascii_block(s_in.encode("latin1")))
            p_p = emu.alloc(bytes(p_b))
            r34 = emu.call(0x00820630, ecx=p_p)
            assert r34.stack_cleanup == 0
            s_out = h.read_ascii_obj(p_p + 0x10)
            check(
                f"qualifyplayerside {pt} {encode_tok(s_in)}",
                encode_tok(s_out),
                "0x00820630",
            )

    # 35: 0x00820B70 ScriptAction::setActionType
    for act_t in [-5, 0, 12, 0x23C, 0x300]:
        h.reset()
        eng_b = bytearray(0x17000)
        struct.pack_into("<iii", eng_b, 0x48 + 0 * 0x80 + 0x48, 1, 0x0B, 0)
        struct.pack_into("<iiii", eng_b, 0x48 + 12 * 0x80 + 0x48, 2, 0x10, 0x0B, 0)
        struct.pack_into("<ii", eng_b, 0x48 + 0x23C * 0x80 + 0x48, 0, 0)
        eng_p = emu.alloc(bytes(eng_b))
        emu.write(0x00DFC058, struct.pack("<I", eng_p))

        act_p = emu.alloc(struct.pack("<Iii", 0x00C35D44, 0, 0) + bytes(0x3C))
        r35 = emu.call(0x00820B70, [u32(act_t)], ecx=act_p)
        assert r35.stack_cleanup == 4
        out_t, out_np = struct.unpack("<ii", emu.read(act_p + 4, 8))
        p_types = []
        for i in range(out_np):
            pp = struct.unpack("<I", emu.read(act_p + 0x0C + i * 4, 4))[0]
            pt_val = struct.unpack("<i", emu.read(pp, 4))[0]
            p_types.append(str(pt_val))
        actual_str = " ".join([str(out_t), str(out_np)] + p_types)
        check(
            f"setactiontype {u32(act_t)} 3 0 1 11 12 2 16 11 572 0",
            actual_str,
            "0x00820B70",
        )

    # Mutation sensitivity check on 0x00804590
    h.reset()
    act_m = emu.alloc(struct.pack("<Ii", 0x00C35D44, 0x129) + bytes(0x40))
    shipped_m = str(emu.call(0x00804590, ecx=act_m).eax)
    mutant_m = checker.ask(f"unitactionmutant {0x129}")
    restored_m = checker.ask(f"unitaction {0x129}")
    if mutant_m == shipped_m:
        raise RuntimeError("semantic mutant was NOT detected by differential oracle")
    if restored_m != shipped_m:
        raise RuntimeError("restored implementation did not match shipped oracle")

    return scenarios


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--exe", type=Path, default=DEFAULT_EXE)
    parser.add_argument(
        "--checker",
        type=Path,
        default=SOURCE / "build" / "win32" / "bin" / "Release" / "ra3_scripts82check.exe",
    )
    args = parser.parse_args()

    checker = Checker(args.checker)
    try:
        verify_inventory(args.exe, checker)
        total = run_differential_oracle(args.exe, checker)
    finally:
        checker.close()

    print(
        f"Scripts82 verified: inventory=64, reversed=36, hooks=10, "
        f"scenarios={total}, 0 semantic mismatches; deliberate mutant detected and restored"
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
