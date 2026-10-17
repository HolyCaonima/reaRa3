"""Differentially verify the fx66 particle-system ports against ra3_1.0.game."""

from __future__ import annotations

import struct
import subprocess
import sys
from pathlib import Path

from unicorn import x86_const

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from emulate import Emulator  # noqa: E402

CLIENT_STATE = 0x00CB73F0
EXE = Path(__file__).resolve().parents[3] / "build" / "win32" / "bin" / "Release" / "ra3_fx66check.exe"

NAMES = (
    (0x00663400, 0x00DF87DC, 0x00DF87D8, 0x00BF3048),
    (0x00663430, 0x00DF87E4, 0x00DF87E0, 0x00BF3050),
    (0x00663460, 0x00DF881C, 0x00DF8818, 0x00BF3088),
    (0x00663490, 0x00DF87F4, 0x00DF87F0, 0x00BF3060),
    (0x006634C0, 0x00DF880C, 0x00DF8808, 0x00BF3078),
    (0x00663520, 0x00DF8824, 0x00DF8820, 0x00BF3090),
    (0x00663550, 0x00DF8864, 0x00DF8860, 0x00BF30D0),
    (0x00663580, 0x00DF8834, 0x00DF8830, 0x00BF30A0),
    (0x006635B0, 0x00DF882C, 0x00DF8828, 0x00BF3098),
    (0x00663670, 0x00DF884C, 0x00DF8848, 0x00BF30B8),
)


def uhex(value: int) -> str:
    return f"{value & 0xFFFFFFFF:08x}"


def fhex(value: float) -> str:
    return uhex(struct.unpack("<I", struct.pack("<f", value))[0])


def rec_pack(mode: int, a: bytes, b: bytes, color: int, a3: int, a4: int) -> bytes:
    return struct.pack("<I", mode) + a + b + struct.pack("<III", color, a3, a4)


class Cpp:
    def __init__(self, path: Path) -> None:
        self.process = subprocess.Popen(
            [str(path)], stdin=subprocess.PIPE, stdout=subprocess.PIPE, text=True, bufsize=1
        )

    def call(self, command: str) -> str:
        assert self.process.stdin is not None and self.process.stdout is not None
        self.process.stdin.write(command + "\n")
        self.process.stdin.flush()
        result = self.process.stdout.readline().strip()
        if not result or result == "error":
            raise RuntimeError(f"checker rejected {command!r} -> {result!r}")
        return result

    def close(self) -> None:
        assert self.process.stdin is not None
        self.process.stdin.write("quit\n")
        self.process.stdin.flush()
        self.process.wait(timeout=10)


def main() -> int:
    emu = Emulator()
    cpp = Cpp(EXE)
    fails: list[str] = []

    draws: list[bytes] = []

    def draw_stub(e: Emulator) -> int:
        a0, a1, color, a3, a4 = (e.stack_arg(i) for i in range(5))
        try:
            second = e.read(a1, 12)
            mode = 0
        except Exception:
            mode = 1
            second = struct.pack("<III", a1 & 0xFFFFFFFF, 0, 0)
        draws.append(rec_pack(mode, e.read(a0, 12), second, color, a3, a4))
        return 0

    draw_fn = emu.bind_stub(draw_stub, 0x14)
    draw_vt = emu.alloc(b"\x00" * 0x80)
    for off in (0x30, 0x3C, 0x44):
        emu.write(draw_vt + off, struct.pack("<I", draw_fn))
    drawer = emu.alloc(struct.pack("<I", draw_vt) + b"\x00" * 16)
    root = emu.alloc(b"\x00" * 0x80)
    emu.write(root + 0x70, struct.pack("<I", drawer))
    emu.write(0x00DEEA1C, struct.pack("<I", root))

    freed = {"tag": 0}
    alloc_fail = {"v": False}

    def alloc_stub(e: Emulator) -> int:
        if alloc_fail["v"]:
            return 0
        return emu.alloc(b"\xCC" * e.stack_arg(0))

    def free_stub(e: Emulator) -> int:
        ptr = e.stack_arg(0)
        freed["tag"] = struct.unpack_from("<I", e.read(ptr + 8, 4))[0] if ptr else 0
        return 0

    def dtor_stub(e: Emulator) -> int:
        ecx = e.register(x86_const.UC_X86_REG_ECX)
        e.write(ecx + 0x24, b"\xD1")
        return 0

    member_hit = {"v": 0}

    def member_stub(e: Emulator) -> int:
        member_hit["v"] = 0x5A
        return 0

    def prep_stub(e: Emulator) -> int:
        ecx = e.register(x86_const.UC_X86_REG_ECX)
        e.write(ecx + 0x19, b"\x5A")
        return 0

    def scene_stub(e: Emulator) -> int:
        mesh = e.stack_arg(2)
        e.write(mesh + 0x3C, b"\xA5")
        return 0

    emu.bind_function(0x00416820, alloc_stub, 0)
    emu.bind_function(0x004166B0, free_stub, 0)
    emu.bind_function(0x00666AB0, prep_stub, 0)
    emu.bind_function(0x0066D0A0, lambda e: 0, 0)
    emu.bind_function(0x009C7E10, lambda e: 0, 4)
    emu.bind_function(0x0057F4E0, lambda e: 0, 4)
    dtor = emu.bind_stub(dtor_stub, 4)
    dtor_vt = emu.alloc(struct.pack("<I", dtor) + b"\x00" * 16)
    member_fn = emu.bind_stub(member_stub, 0)
    scene_fn = emu.bind_stub(scene_stub, 0)

    def check(label: str, command: str, expected: str) -> None:
        got = cpp.call(command)
        if got != expected:
            fails.append(f"{label}\n  cmd {command}\n  got {got}\n  exp {expected}")

    def draw_blob() -> str:
        raw = b"".join(draws)
        return uhex(len(draws)) + raw.hex()

    def run_draw(addr: int, this: int, point: int) -> None:
        draws.clear()
        emu.call(addr, [point], ecx=this)

    try:
        for cw in (0x007F, 0x027F):
            if cpp.call(f"fpcw {cw:x}") != "ok":
                raise RuntimeError("fpcw")
            emu.uc.reg_write(x86_const.UC_X86_REG_FPCW, cw)
            cases = [
                ("box", 0x00661B90, (2.0, 3.0, 4.0, 0, 0, 0), (10, 20, 30), False),
                ("box0", 0x00661B90, (0, 0, 0, 0, 0, 0), (1, -2, 3), False),
                ("boxn", 0x00661B90, (-1.5, 0.25, 8, 0, 0, 0), (0, 0, 0), False),
                ("cyl", 0x006620F0, (0, 0, 5.0, 1.5, 0, 0), (1, 2, 3), True),
                ("cyl0", 0x006620F0, (0, 0, 0.0, 0.0, 0, 0), (4, 5, 6), True),
                ("line", 0x00662390, (1, 2, 3, 4, 5, 6), (10, 20, 30), False),
                ("sphere", 0x00662710, (2.5, 0, 0, 0, 0, 0), (8, 9, 10), False),
                ("sphere0", 0x00662710, (0, 0, 0, 0, 0, 0), (1, 1, 1), False),
                ("spline0", 0x00662820, (1, 0, 0, 0, 1, 0), (5, 6, 7), False),
            ]
            for name, addr, tmpl_f, point_f, cyl in cases:
                tmpl = emu.alloc(b"\x00" * 0x40)
                module = emu.alloc(b"\x00" * 0x20)
                emu.write(module + 8, struct.pack("<I", tmpl))
                vals = [struct.pack("<f", float(v)) for v in tmpl_f]
                emu.write(tmpl + 0x0C, b"".join(vals[:3]))
                emu.write(tmpl + 0x18, b"".join(vals[3:]))
                if cyl:
                    emu.write(module + 0x0C, struct.pack("<f", float(tmpl_f[3])))
                point = emu.alloc(struct.pack("<3f", *map(float, point_f)))
                run_draw(addr, module, point)
                nums = " ".join(fhex(float(v)) for v in (*tmpl_f, *point_f))
                cmd = "cyl" if name.startswith("cyl") else name
                if name.startswith("box"):
                    cmd = "box"
                elif name.startswith("sphere"):
                    cmd = "sphere"
                check(f"{name} cw={cw:x}", f"{cmd} {nums}", draw_blob())

            for label, point_f, addr, cmd in (
                ("point", (8, 9, 10), 0x00662680, "point"),
                ("pointn", (-3, 0, -1), 0x00662680, "point"),
                ("bolt", (1, 2, 3), 0x00663180, "lightning"),
                ("bolt0", (0, 0, 0), 0x00663180, "lightning"),
            ):
                point = emu.alloc(struct.pack("<3f", *map(float, point_f)))
                run_draw(addr, 0, point)
                nums = " ".join(fhex(float(v)) for v in point_f)
                check(f"{label} cw={cw:x}", f"{cmd} {nums}", draw_blob())

            for count, pts in (
                (0, []),
                (1, [(1, 2, 3)]),
                (2, [(0, 0, 0), (2, 0, 0)]),
                (3, [(0, 0, 0), (2, 0, 0), (4, 1, 0)]),
            ):
                raw = b"".join(struct.pack("<6f", *p, 0, 0, 0) for p in pts)
                buf = emu.alloc(raw or b"\x00")
                module = emu.alloc(b"\x00" * 0x20)
                emu.write(module + 0x0C, b"\x01")
                emu.write(module + 0x10, struct.pack("<II", buf, buf + len(raw)))
                point = emu.alloc(struct.pack("<3f", 9, 9, 9))
                run_draw(0x00662820, module, point)
                body = " ".join(fhex(c) for p in pts for c in p)
                cmd = f"splinen {count:x} {fhex(9)} {fhex(9)} {fhex(9)}"
                if body:
                    cmd += " " + body
                check(f"splinen{count} cw={cw:x}", cmd, draw_blob())

        def swarm_case(mode: int, rate: float, target, records: list[bytes]) -> None:
            blob = b"".join(records)
            sysb = emu.alloc(b"\x00" * 0x110)
            emu.write(sysb + 0xE0, struct.pack("<f", target[0]))
            emu.write(sysb + 0xF0, struct.pack("<f", target[1]))
            emu.write(sysb + 0x100, struct.pack("<f", target[2]))
            rate_data = emu.alloc(b"\x00" * 0x14)
            emu.write(rate_data + 0x10, struct.pack("<f", rate))
            view = emu.alloc(b"\x00" * 0x10)
            emu.write(view + 8, struct.pack("<I", rate_data))
            view_fn = emu.bind_stub(lambda e, view=view: view, 0)
            null_fn = emu.bind_stub(lambda e: 0, 0)
            rate_vt = emu.alloc(b"\x00" * 0x60)
            emu.write(rate_vt + 0x54, struct.pack("<I", null_fn if mode == 2 else view_fn))
            rate_obj = emu.alloc(struct.pack("<II", rate_vt, 0) + struct.pack("<I", view))
            if mode != 1:
                emu.write(sysb + 0x20, struct.pack("<I", rate_obj))
            slot = emu.alloc(struct.pack("<I", sysb))
            rec = emu.alloc(blob or b"\x00" * 4)
            module = emu.alloc(b"\x00" * 0x14)
            emu.write(module + 4, struct.pack("<I", slot))
            emu.write(module + 8, struct.pack("<I", rec))
            emu.write(module + 0x0C, struct.pack("<I", rec + len(blob)))
            emu.call(0x006721B0, [], ecx=module)
            end = struct.unpack_from("<I", emu.read(module + 0x0C, 4))[0]
            begin = struct.unpack_from("<I", emu.read(module + 8, 4))[0]
            count = (end - begin) // 0x1C
            expected = uhex(count) + emu.read(rec, count * 0x1C).hex()
            nums = " ".join(
                [f"{mode:x}", fhex(rate), fhex(target[0]), fhex(target[1]), fhex(target[2]), f"{len(records):x}"]
            )
            check(f"swarm m{mode} n{len(records)}", f"swarm {nums} {blob.hex() or '00'}", expected)

        def rec(x, y, z, vx, vy, vz, life) -> bytes:
            return struct.pack("<6fi", x, y, z, vx, vy, vz, life)

        swarm_case(0, 0.5, (10, 0, -2), [])
        swarm_case(0, 0.25, (3, 4, 5), [rec(0, 0, 0, 1, 0, 0, 4)])
        swarm_case(0, 1.0, (0, 0, 0), [rec(1, 2, 3, 0, 1, 0, 1)])
        swarm_case(0, 0.5, (8, 8, 8), [rec(0, 0, 0, 0, 0, 0, 1), rec(1, 1, 1, 0.5, 0, 0, 3)])
        swarm_case(0, -0.5, (1, 2, 3), [rec(4, 5, 6, 0, 0, 1, 0)])
        swarm_case(1, 1.0, (1, 1, 1), [rec(0, 0, 0, 0, 0, 0, 2)])
        swarm_case(2, 1.0, (1, 1, 1), [rec(0, 0, 0, 0, 0, 0, 2), rec(1, 0, 0, 0, 0, 0, 2)])

        for nbytes in (0, 28, 56, 0x1C * 3, -28 & 0xFFFFFFFF, 4):
            begin = emu.alloc(b"\x00" * 4)
            module = emu.alloc(b"\x00" * 0x10)
            emu.write(module + 8, struct.pack("<I", begin))
            emu.write(module + 0x0C, struct.pack("<I", (begin + nbytes) & 0xFFFFFFFF))
            result = emu.call(0x006637C0, [], ecx=module)
            check(f"count {nbytes}", f"count {nbytes & 0xFFFFFFFF:x}", uhex(result.eax))

        def name_obj(tag: int, refs: int) -> int:
            raw = bytearray(0x28)
            struct.pack_into("<III", raw, 0, dtor_vt, refs, tag)
            return emu.alloc(bytes(raw))

        for which, present, refs in (
            (0, 1, 2),
            (0, 0, 2),
            (1, 1, 5),
            (1, 0, 1),
            (2, 1, 3),
            (2, 0, 3),
            (3, 1, 4),
            (3, 0, 4),
        ):
            obj = name_obj(0x11111111, refs)
            tmpl = emu.alloc(b"\x00" * 0x50)
            module = emu.alloc(b"\x00" * 0x10)
            emu.write(module + 8, struct.pack("<I", tmpl))
            if present:
                off = 0x48 if which == 1 else 0x1C
                emu.write(tmpl + off, struct.pack("<I", obj))
            if which == 2:
                emu.write(0x00E1A5AC, struct.pack("<I", tmpl))
            if which == 3:
                emu.write(0x00DF7090, struct.pack("<I", tmpl))
            out = emu.alloc(struct.pack("<I", 1))
            addr = (0x00666A30, 0x00665FB0, 0x00666390, 0x006663C0)[which]
            emu.call(addr, [out], ecx=module)
            stored = struct.unpack_from("<I", emu.read(out, 4))[0]
            if stored == 0:
                expected = uhex(0) + uhex(0)
            else:
                tag, got = struct.unpack_from("<II", emu.read(stored + 8, 4) + emu.read(stored + 4, 4))
                expected = uhex(tag) + uhex(got)
            check(f"addref {which} {present}", f"addref {which:x} {refs:x} {present:x}", expected)

        for prev_refs, prev_tag, in_tag, in_refs in ((2, 0x22222222, 0x89ABCDEF, 4), (1, 0x22222222, 0x1000, 7)):
            freed["tag"] = 0
            previous = name_obj(prev_tag, prev_refs)
            incoming = name_obj(in_tag, in_refs)
            particle = emu.alloc(b"\x00" * 0x50)
            emu.write(particle + 0x44, struct.pack("<I", previous))
            slot = emu.alloc(struct.pack("<I", incoming))
            emu.call(0x00676080, [slot], ecx=particle)
            stored = struct.unpack_from("<I", emu.read(particle + 0x44, 4))[0]
            tag = struct.unpack_from("<I", emu.read(stored + 8, 4))[0]
            srefs = struct.unpack_from("<I", emu.read(stored + 4, 4))[0]
            prefs = struct.unpack_from("<I", emu.read(previous + 4, 4))[0]
            mark = emu.read(previous + 0x24, 1)[0]
            expected = uhex(tag) + uhex(srefs) + uhex(prefs) + uhex(freed["tag"]) + uhex(mark)
            check(
                f"assign {prev_refs}",
                f"assign {prev_refs:x} {prev_tag:x} {in_tag:x} {in_refs:x}",
                expected,
            )

        for mode, refs, sentinel in ((0, 3, 9), (1, 3, 4), (2, 3, 1), (0, 1, 2)):
            freed["tag"] = 0
            previous = name_obj(0x42424242, refs)
            if mode == 2:
                emu.write(previous, struct.pack("<I", 0))
            particle = emu.alloc(b"\x00" * 0x50)
            emu.write(particle + 0x44, struct.pack("<I", 0x00DF88F8 if mode == 1 else previous))
            emu.write(0x00DF88FC, struct.pack("<i", sentinel))
            emu.call(0x006760C0, [], ecx=particle)
            field = struct.unpack_from("<I", emu.read(particle + 0x44, 4))[0]
            shown = field if field == 0x00DF88F8 else 1
            ref = struct.unpack_from("<i", emu.read(0x00DF88FC, 4))[0]
            prefs = struct.unpack_from("<I", emu.read(previous + 4, 4))[0]
            expected = uhex(shown) + uhex(ref) + uhex(prefs) + uhex(freed["tag"])
            check(f"reset {mode} {refs}", f"resetaux {refs:x} 42424242 {mode:x} {sentinel:x}", expected)

        for armed, present, refs in ((0, 1, 4), (1, 0, 4), (1, 1, 3), (1, 1, 1)):
            held = name_obj(0x33333333, refs)
            emu.write(0x00DF6F80, bytes([armed]))
            emu.write(0x00DF7158, struct.pack("<I", held if present else 0))
            emu.call(0x00666A60, [], ecx=0)
            flag = emu.read(0x00DF6F80, 1)[0]
            now = struct.unpack_from("<I", emu.read(0x00DF7158, 4))[0]
            hrefs = struct.unpack_from("<I", emu.read(held + 4, 4))[0]
            mark = emu.read(held + 0x24, 1)[0]
            expected = uhex(flag) + uhex(0 if now == 0 else 1) + uhex(hrefs) + uhex(mark)
            check(f"release {armed} {present} {refs}", f"release {armed:x} {present:x} {refs:x}", expected)

        for present, refs, name_present, name_refs in ((0, 2, 1, 2), (1, 2, 1, 5), (1, 2, 0, 1), (1, 1, 1, 4)):
            name = name_obj(0x44444444, name_refs)
            holder = bytearray(0x28)
            struct.pack_into("<II", holder, 0, dtor_vt, refs)
            if name_present:
                struct.pack_into("<I", holder, 0x1C, name)
            holder_addr = emu.alloc(bytes(holder))
            tmpl = emu.alloc(b"\x00" * 0x48)
            if present:
                emu.write(tmpl + 0x44, struct.pack("<I", holder_addr))
            out = emu.alloc(struct.pack("<I", 1))
            emu.call(0x00665F50, [out], ecx=tmpl)
            stored = struct.unpack_from("<I", emu.read(out, 4))[0]
            if stored == 0:
                tag, nrefs = 0, 0
            else:
                tag = struct.unpack_from("<I", emu.read(stored + 8, 4))[0]
                nrefs = struct.unpack_from("<I", emu.read(stored + 4, 4))[0]
            hrefs = struct.unpack_from("<I", emu.read(holder_addr + 4, 4))[0]
            mark = emu.read(holder_addr + 0x24, 1)[0]
            expected = uhex(tag) + uhex(nrefs) + uhex(hrefs) + uhex(mark)
            check(
                f"exchange {present} {refs} {name_present}",
                f"exchange {present:x} {refs:x} {name_present:x} {name_refs:x}",
                expected,
            )

        for bits in ((0x3F800000, 0x40000000, 0x40400000), (0x7FC00001, 0x7F800000, 0xBF800000)):
            module = emu.alloc(b"\x00" * 0x30)
            emu.write(module + 0x20, struct.pack("<III", *bits))
            out = emu.alloc(b"\x00" * 12)
            emu.call(0x00662960, [out], ecx=module)
            expected = emu.read(out, 12).hex()
            check("tangent", f"tangent {bits[0]:x} {bits[1]:x} {bits[2]:x}", expected)

        for a, b, c in ((0x11, 0x22, 0x33), (0xFFFFFFFF, 0, 0x7FFFFFFF)):
            inner = emu.alloc(b"\x00" * 0x68)
            emu.write(inner + 0x4C, struct.pack("<I", a))
            emu.write(inner + 0x54, struct.pack("<I", b))
            emu.write(inner + 0x5C, struct.pack("<I", c))
            emu.write(inner + 0x60, struct.pack("<I", a))
            second = emu.alloc(b"\x00" * 0x10)
            emu.write(second + 8, struct.pack("<I", inner))
            first = emu.alloc(struct.pack("<I", second))
            module = emu.alloc(b"\x00" * 8)
            emu.write(module + 4, struct.pack("<I", first))
            pair = emu.alloc(b"\x00" * 8)
            emu.call(0x00665350, [pair], ecx=module)
            lo, hi = struct.unpack_from("<II", emu.read(pair, 8))
            check("shader", f"shader {a:x} {b:x} {c:x}", uhex(lo) + uhex(hi))
            got = emu.call(0x00665330, [], ecx=module)
            dword = struct.unpack_from("<I", emu.read(got.eax, 4))[0]
            check("payload", f"payload {a:x} {b:x} {c:x}", uhex(dword) + uhex(1 if got.eax == inner + 0x54 else 0))
            word = emu.call(0x00666A50, [], ecx=module)
            check("sword", f"sword {a:x} {b:x} {c:x}", uhex(word.eax))

        def xfer_case(ext: bool, version: int, modules: int, aux_mode: int, aux_word: int) -> None:
            stream = bytes((i * 17 + 3) & 0xFF for i in range(220))
            particle = emu.alloc(b"\xAB" * 0x90)
            for off in (0x44, 0x4C, 0x50, 0x54, 0x58, 0x5C, 0x60, 0x64):
                emu.write(particle + off, b"\x00\x00\x00\x00")
            slots = emu.alloc(b"\x00" * 0xA0)

            cursor = {"n": 0}

            def take(size: int, dest: int) -> None:
                emu.write(dest, stream[cursor["n"] : cursor["n"] + size])
                cursor["n"] += size

            def xfer_stub(size):
                def inner(e: Emulator) -> int:
                    take(size, e.stack_arg(0))
                    return 0
                return inner

            captured = {"v": 0}

            def named(e: Emulator) -> int:
                dest = e.stack_arg(1)
                captured["v"] = struct.unpack_from("<I", e.read(dest, 4))[0]
                take(4, dest)
                return 0

            emu.write(slots + 0x18, struct.pack("<I", emu.bind_stub(xfer_stub(2), 4)))
            emu.write(slots + 0x58, struct.pack("<I", emu.bind_stub(xfer_stub(12), 4)))
            emu.write(slots + 0x6C, struct.pack("<I", emu.bind_stub(xfer_stub(4), 4)))
            emu.write(slots + 0x84, struct.pack("<I", emu.bind_stub(xfer_stub(1), 4)))
            emu.write(slots + 0x88, struct.pack("<I", emu.bind_stub(named, 12)))
            xfer = emu.alloc(struct.pack("<II", slots, version) + b"\x00" * 16)
            mod_words = []
            if ext:
                mod_vt = emu.alloc(b"\x00" * 0x20)

                def mod_stub(e: Emulator) -> int:
                    ecx = e.register(x86_const.UC_X86_REG_ECX)
                    take(4, ecx + 4)
                    return 0

                emu.write(mod_vt + 0x0C, struct.pack("<I", emu.bind_stub(mod_stub, 4)))
                mods = []
                for _ in range(6):
                    mods.append(emu.alloc(struct.pack("<II", mod_vt, 0)))
                offs = (0x4C, 0x50, 0x54, 0x5C, 0x58)
                for i in range(min(modules, 5)):
                    emu.write(particle + offs[i], struct.pack("<I", mods[i]))
                if modules >= 6:
                    listing = emu.alloc(struct.pack("<I", mods[5]))
                    emu.write(particle + 0x60, struct.pack("<I", listing))
                    emu.write(particle + 0x64, struct.pack("<I", listing + 4))
                aux_vt = emu.alloc(b"\x00" * 0x98)
                emu.write(aux_vt + 0x94, struct.pack("<I", aux_word))
                aux = emu.alloc(struct.pack("<I", 0 if aux_mode == 2 else aux_vt))
                if aux_mode == 1:
                    emu.write(particle + 0x44, struct.pack("<I", 0x00DF88F8))
                else:
                    emu.write(particle + 0x44, struct.pack("<I", aux))
                emu.call(0x00667EA0, [xfer], ecx=particle)
                for item in mods:
                    mod_words.append(struct.unpack_from("<I", emu.read(item + 4, 4))[0])
            else:
                emu.call(0x006604B0, [xfer], ecx=particle)
                mod_words = [0] * 6
            raw = bytearray(emu.read(particle, 0x90))
            for off in (0x44, 0x4C, 0x50, 0x54, 0x58, 0x5C, 0x60, 0x64):
                raw[off : off + 4] = b"\xAB\xAB\xAB\xAB"
            expected = uhex(cursor["n"]) + uhex(captured["v"]) + raw.hex() + "".join(uhex(w) for w in mod_words)
            cmd = (
                f"{'xferext' if ext else 'xfer'} {version:x} {stream.hex()} {modules:x} {aux_mode:x} {aux_word:x}"
            )
            got = cpp.call(cmd)
            # Mask the same pointer slots in the checker's particle image.
            consumed = got[:8]
            cap = got[8:16]
            particle_hex = got[16 : 16 + 0x90 * 2]
            tail = got[16 + 0x90 * 2 :]
            blob = bytearray(bytes.fromhex(particle_hex))
            for off in (0x44, 0x4C, 0x50, 0x54, 0x58, 0x5C, 0x60, 0x64):
                blob[off : off + 4] = b"\xAB\xAB\xAB\xAB"
            masked = consumed + cap + blob.hex() + tail
            if masked != expected:
                fails.append(f"xfer ext={ext} ver={version:#x} mods={modules} aux={aux_mode}\n  got {masked}\n  exp {expected}")

        xfer_case(False, 0, 0, 0, 0)
        xfer_case(False, 0x8, 0, 0, 0)
        xfer_case(False, 0x7, 0, 0, 0)
        xfer_case(True, 0x8, 0, 0, 0x1234)
        xfer_case(True, 0, 0, 1, 0x1234)
        xfer_case(True, 0, 0, 2, 0x1234)
        xfer_case(True, 0, 1, 0, 0x55AA)
        xfer_case(True, 0, 6, 0, 0x01020304)

        state = bytes(range(24))
        for fail, kind in ((1, "mkphys"), (0, "mkphys"), (0, "mkterr"), (1, "mkterr"), (0, "clbox"), (1, "clbox"), (0, "clphys"), (1, "clphys"), (0, "cllife"), (1, "cllife")):
            alloc_fail["v"] = bool(fail)
            emu.write(CLIENT_STATE, state)
            cpp.call(f"fpcw 7f")
            emu.uc.reg_write(x86_const.UC_X86_REG_FPCW, 0x007F)
            tmpl = emu.alloc(b"\x11" * 0x40)
            emu.write(tmpl + 4, struct.pack("<I", 0xAABBCCDD))
            emu.write(tmpl + 8, b"\x3C")
            emu.write(tmpl + 9, b"\x3D")
            emu.write(tmpl + 0x0C, struct.pack("<ff", 1.5, -2.0))
            emu.write(tmpl + 0x14, struct.pack("<Iff", 0, 1.25, 1.25))
            emu.write(tmpl + 0x20, b"\x41")
            emu.write(tmpl + 0x21, b"\x42")
            system = name_obj(0x66666666, 6)
            module = emu.alloc(b"\x00" * 0x10)
            emu.write(module + 8, struct.pack("<I", tmpl))
            slot = emu.alloc(struct.pack("<I", system))
            addr = {"mkphys": 0x0066A490, "mkterr": 0x006705A0, "clbox": 0x0066A880, "clphys": 0x006652B0, "cllife": 0x00666050}[kind]
            arg = system if kind == "mkphys" else slot
            result = emu.call(addr, [] if kind.startswith("cl") else [arg], ecx=tmpl if kind != "mkphys" else module)
            new_state = emu.read(CLIENT_STATE, 24)
            refs = struct.unpack_from("<I", emu.read(system + 4, 4))[0]
            if result.eax == 0:
                blob = "00"
            else:
                size = {"mkphys": 0x10, "mkterr": 0x14, "clbox": 0x18, "clphys": 0x24, "cllife": 0x20}[kind]
                raw = bytearray(emu.read(result.eax, size))
                if kind == "mkphys":
                    raw[4:8] = b"\x00" * 4
                elif kind == "mkterr":
                    raw[4:12] = b"\x00" * 8
                blob = "01" + raw.hex()
            fields = "4 14 " + uhex(struct.unpack_from("<I", struct.pack("<f", 1.25))[0])
            fields += " 18 " + uhex(struct.unpack_from("<I", struct.pack("<f", 1.25))[0])
            fields += " c " + fhex(1.5) + " 10 " + fhex(-2.0) + " 14wait"
            # The checker overlays floats by offset. Build that explicitly.
            spec = [
                (0x14, 0),
                (0x18, struct.unpack_from("<I", struct.pack("<f", 1.25))[0]),
                (0x1C, struct.unpack_from("<I", struct.pack("<f", 1.25))[0]),
                (0x04, 0xAABBCCDD),
                (0x0C, struct.unpack_from("<I", struct.pack("<f", 1.5))[0]),
                (0x10, struct.unpack_from("<I", struct.pack("<f", -2.0))[0]),
            ]
            # distribution dword is the 0 at +0x14; the two floats follow.
            cmd = (
                f"{kind} {fail:x} {state.hex()} 6 "
                f"14 {spec[0][1]:x} 18 {spec[1][1]:x} 1c {spec[2][1]:x} "
                f"4 {spec[3][1]:x} c {spec[4][1]:x} 10 {spec[5][1]:x} "
                f"4 8 3c 9 3d 20 41 21 42 6"
            )
            got = cpp.call(cmd)
            prefix = new_state.hex() + uhex(refs)
            body = got[len(prefix) :]
            if kind in ("mkphys", "mkterr") and body.startswith("01"):
                raw = bytearray(bytes.fromhex(body[2:]))
                if kind == "mkphys":
                    raw[4:8] = b"\x00" * 4
                else:
                    raw[4:12] = b"\x00" * 8
                body = "01" + raw.hex()
            if prefix + body != prefix + blob and body != blob:
                fails.append(f"{kind} fail={fail}\n  got {got}\n  exp {prefix + blob}")
            elif body != blob:
                fails.append(f"{kind} fail={fail} body\n  got {body}\n  exp {blob}")

        for gate, armed, query in ((0, 0, 1), (1, 0, 0), (1, 0, 1), (1, 1, 1)):
            emu.write(0x00DF7090, struct.pack("<I", 0 if gate == 0 else root))
            emu.write(0x00DF6F81, bytes([armed]))
            qfn = emu.bind_stub(lambda e, query=query: root if query else 0, 0)
            qvt = emu.alloc(b"\x00" * 0x40)
            emu.write(qvt + 0x2C, struct.pack("<I", qfn))
            attachment = emu.alloc(struct.pack("<I", qvt))
            system = emu.alloc(b"\x00" * 0x98)
            emu.write(system + 0x90, struct.pack("<I", attachment))
            slot = emu.alloc(struct.pack("<I", system))
            module = emu.alloc(b"\x00" * 8)
            emu.write(module + 4, struct.pack("<I", slot))
            emu.call(0x00670930, [0, 0, 0], ecx=module)
            flag = emu.read(0x00DF6F81, 1)[0]
            check(f"trail {gate} {query}", f"trail {gate:x} {armed:x} {query:x}", uhex(flag))

        for kind, shader, gate, refs in ((0, 1, 1, 3), (1, 5, 1, 3), (1, 6, 1, 4), (1, 1, 0, 3), (1, 2, 1, 5), (2, 1, 1, 3)):
            name = name_obj(0x77777777, refs)
            tmpl = emu.alloc(b"\x00" * 0x28)
            if kind != 2:
                emu.write(tmpl + 0x20, struct.pack("<I", name))
            st = emu.alloc(b"\x00" * 0x54)
            emu.write(st + 0x50, struct.pack("<I", shader))
            system = emu.alloc(b"\x00" * 0x10)
            emu.write(system + 8, struct.pack("<I", st))
            slot = emu.alloc(struct.pack("<I", system))
            module = emu.alloc(b"\x00" * 0x10)
            emu.write(module + 4, struct.pack("<I", slot))
            emu.write(module + 8, struct.pack("<I", tmpl))
            arg = emu.alloc(b"\x00" * 0x10)
            emu.write(arg + 8, struct.pack("<I", 1 if kind == 0 else 3))
            emu.write(0x00DF6F80, b"\x00")
            emu.write(0x00DF6F84, struct.pack("<I", gate))
            emu.call(0x006707C0, [arg, 0, 0], ecx=module)
            flag = emu.read(0x00DF6F80, 1)[0]
            nrefs = struct.unpack_from("<I", emu.read(name + 4, 4))[0]
            mark = emu.read(name + 0x24, 1)[0]
            expected = uhex(flag) + uhex(nrefs) + uhex(mark)
            check(f"pass {kind} {shader} {gate}", f"swarmpass {kind:x} {shader:x} {gate:x} {refs:x}", expected)

        for armed, primed, has_list, count in ((0, 0, 1, 4), (1, 0, 0, 4), (1, 1, 1, 0), (1, 0, 1, 4), (1, 1, 1, 3)):
            member_hit["v"] = 0
            member = emu.alloc(struct.pack("<II", 0, member_fn))
            mesh = emu.alloc(b"\x11" * 0x40)
            scene = emu.alloc(b"\x00" * 0x60)
            emu.write(scene + 0x18, struct.pack("<I", scene_fn))
            emu.write(scene + 0x58, struct.pack("<I", root))
            emu.write(0x00E1A2FC, struct.pack("<I", scene))
            emu.write(0x00DF6F80, bytes([armed]))
            emu.write(0x00DF7158, struct.pack("<I", root))
            module = emu.alloc(b"\x00" * 0x20)
            emu.write(module + 0x10, bytes([primed]))
            emu.write(module + 0x0C, struct.pack("<I", member))
            emu.write(module + 0x18, struct.pack("<I", has_list))
            emu.write(module + 0x14, struct.pack("<I", count))
            emu.write(module + 0x1C, struct.pack("<I", mesh))
            result = emu.call(0x0066C1E0, [0, 0, 0], ecx=module)
            flag10 = emu.read(module + 0x10, 1)[0]
            flag19 = emu.read(module + 0x19, 1)[0]
            mark = emu.read(mesh + 0x3C, 1)[0]
            fields = emu.read(mesh + 0x28, 20)
            expected = uhex(result.eax) + uhex(flag10) + uhex(flag19) + uhex(member_hit["v"]) + uhex(mark) + fields.hex()
            check(
                f"submit {armed} {primed} {has_list} {count}",
                f"submit {armed:x} {primed:x} {has_list:x} {count:x}",
                expected,
            )

        for index, (addr, guard, slot, pointer) in enumerate(NAMES):
            for initial_guard, initial_slot, label in ((0, 0, "cold"), (1, 0xABCDEF, "hot"), (0x100, 0x111, "bit")):
                emu.write(guard, struct.pack("<I", initial_guard))
                emu.write(slot, struct.pack("<I", initial_slot))
                emu.call(addr, [], ecx=0)
                got_guard = struct.unpack_from("<I", emu.read(guard, 4))[0]
                got_slot = struct.unpack_from("<I", emu.read(slot, 4))[0]
                expected = uhex(got_guard) + uhex(got_slot) + uhex(1)
                check(f"name {index} {label}", f"name {index:x} {initial_guard:x} {initial_slot:x}", expected)
                if got_slot not in (pointer, initial_slot):
                    fails.append(f"name {index} stored {got_slot:#x} not {pointer:#x}")

    finally:
        cpp.close()

    print(f"mismatches={len(fails)}")
    for item in fails[:12]:
        print(item)
    return 1 if fails else 0


if __name__ == "__main__":
    sys.exit(main())
