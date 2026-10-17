"""Differentially verify the render99 SageRender cluster against ra3_1.0.game."""

from __future__ import annotations

import struct
import subprocess
import sys
from pathlib import Path

from unicorn import x86_const

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from emulate import Emulator  # noqa: E402

ROOT = Path(__file__).resolve().parents[4]
EXE = ROOT / "Ra3Source" / "build" / "win32" / "bin" / "Release" / "ra3_render99check.exe"
NULL = 0xFFFFFFFF

COUNTED = (
    0x00990080, 0x00990100, 0x009901E0, 0x00990620,
    0x00993120, 0x00993360, 0x00993600, 0x009937E0, 0x009938A0,
    0x00995D00, 0x00997720, 0x00997EF0, 0x00997FD0, 0x009980D0,
    0x009984F0, 0x00998F40, 0x0099A790, 0x0099A850, 0x0099A8E0,
    0x0099B010, 0x0099BD10, 0x0099BD70, 0x0099BE10, 0x0099C0A0,
    0x0099C160, 0x0099C290, 0x0099C440, 0x0099F1C0, 0x0099F310,
    0x009A0500, 0x009A0640, 0x009A1990, 0x009A2540, 0x009A2DF0,
    0x009A82B0, 0x009A8EA0, 0x009AB8E0, 0x009ACB60, 0x009ACFE0,
    0x00992F70,
)

# cleanup, ecx from self, stack sources. Sources: s/0/1/2 pointers, i0/i1/i2 immediates, z zero.
ABI = {
    0x00990080: (4, True, ("0",)),
    0x00990100: (0, True, ()),
    0x009901E0: (4, True, ("0",)),
    0x00990620: (0, True, ()),
    0x00993120: (0, True, ()),
    0x00993360: (0, True, ()),
    0x00993600: (4, True, ("0",)),
    0x009937E0: (4, True, ("0",)),
    0x009938A0: (4, True, ("0",)),
    0x00995D00: (0, False, ("0", "1")),
    0x00997720: (0x10, False, ("0", "1", "z", "z")),
    0x00997EF0: (0, False, ("0", "1", "2")),
    0x00997FD0: (0, False, ("0", "1", "2")),
    0x009980D0: (0, False, ("0", "1", "2")),
    0x009984F0: (0, True, ()),
    0x00998F40: (4, True, ("0",)),
    0x0099A790: (0x0C, False, ("0", "1", "2")),
    0x0099A850: (0x10, False, ("0", "1", "2", "s")),
    0x0099A8E0: (4, True, ("0",)),
    0x0099B010: (4, True, ("0",)),
    0x0099BD10: (0, False, ("s", "0", "1", "2")),
    0x0099BD70: (0, False, ("s", "0", "1", "2")),
    0x0099BE10: (4, True, ("0",)),
    0x0099C0A0: (0, False, ("s", "0", "1", "2")),
    0x0099C160: (0, False, ("0", "1", "2")),
    0x0099C290: (0, False, ("0", "1", "2")),
    0x0099C440: (4, True, ("0",)),
    0x0099F1C0: (0, False, ("0", "1")),
    0x0099F310: (0, True, ()),
    0x009A0500: (0, False, ("0", "i1", "2")),
    0x009A0640: (0, False, ("0", "1", "2")),
    0x009A1990: (0x0C, False, ("0", "1", "2")),
    0x009A2540: (0, False, ()),
    0x009A2DF0: (4, True, ("0",)),
    0x009A82B0: (0, True, ()),
    0x009A8EA0: (0, False, ("0", "1")),
    0x009AB8E0: (4, True, ("i0",)),
    0x009ACB60: (4, True, ("0",)),
    0x009ACFE0: (4, True, ("0",)),
    0x00992F70: (4, True, ("0",)),
}

IGNORE_EAX = {0x00990620}


def u32(blob: bytes, offset: int) -> int:
    return struct.unpack_from("<I", blob, offset)[0]


def put(blob: bytearray, offset: int, value: int) -> None:
    struct.pack_into("<I", blob, offset, value & 0xFFFFFFFF)


def f32(value: float) -> int:
    return struct.unpack("<I", struct.pack("<f", value))[0]


def zeros(size: int) -> bytearray:
    return bytearray(size)


def paint(blob: bytearray, offset: int, count: int, base: int) -> None:
    for index in range(count):
        put(blob, offset + index * 4, (base + index * 0x11) & 0xFFFFFFFF)


class Checker:
    def __init__(self, executable: Path) -> None:
        self.executable = executable
        self.process = None
        self._open()

    def _open(self) -> None:
        self.process = subprocess.Popen(
            [str(self.executable), "--batch"],
            stdin=subprocess.PIPE,
            stdout=subprocess.PIPE,
            text=True,
            bufsize=1,
        )

    def call(self, va, self_off, blob, fixups, a0, a1, a2) -> tuple[int, bytes]:
        assert self.process is not None and self.process.stdin and self.process.stdout
        fix = ",".join(fixups) if fixups else "-"
        line = (
            f"0x{va:08X} 0x{self_off:X} {bytes(blob).hex()} {fix} "
            f"0x{a0 & 0xFFFFFFFF:X} 0x{a1 & 0xFFFFFFFF:X} 0x{a2 & 0xFFFFFFFF:X} 0x037F\n"
        )
        try:
            self.process.stdin.write(line)
            self.process.stdin.flush()
            out = self.process.stdout.readline().strip()
        except (BrokenPipeError, OSError):
            out = ""
        if not out.startswith("ret=0x"):
            self.close()
            self._open()
            raise RuntimeError(out or "checker died")
        parts = out.split()
        return int(parts[0].split("=")[1], 16), bytes.fromhex(parts[1].split("=")[1])

    def close(self) -> None:
        if self.process is not None and self.process.stdin is not None:
            self.process.stdin.close()
            self.process.wait(timeout=10)
        self.process = None


class Oracle:
    def __init__(self) -> None:
        self.emu = Emulator()
        self.cpp = Checker(EXE)
        self.vstubs: dict[int, int] = {}
        self.counts: dict[int, int] = {}
        self.failures: list[str] = []
        self._bind()

    def _bind(self) -> None:
        emu = self.emu

        def v0(e: Emulator) -> int:
            obj = e.stack_arg(0)
            cur = u32(e.read(obj + 0x18, 4), 0)
            e.write(obj + 0x18, struct.pack("<I", (cur + 1) & 0xFFFFFFFF))
            return 0

        def v1(e: Emulator) -> int:
            obj = e.stack_arg(0)
            cur = u32(e.read(obj + 0x1C, 4), 0)
            e.write(obj + 0x1C, struct.pack("<I", (cur + 1) & 0xFFFFFFFF))
            return 0

        def v2(e: Emulator) -> int:
            e.write(e.register(x86_const.UC_X86_REG_ECX) + 8, struct.pack("<I", e.stack_arg(0)))
            return 0

        def v3(e: Emulator) -> int:
            self_ptr = e.register(x86_const.UC_X86_REG_ECX)
            arg = e.stack_arg(0)
            e.write(self_ptr + 0x20, struct.pack("<I", arg))
            if arg == 0:
                return 0
            return u32(e.read(arg, 4), 0)

        def v4(e: Emulator) -> int:
            self_ptr = e.register(x86_const.UC_X86_REG_ECX)
            arg = e.stack_arg(0)
            e.write(self_ptr + 0x24, struct.pack("<I", arg))
            if arg == 0:
                return 0
            return e.read(arg, 1)[0]

        def v5(e: Emulator) -> int:
            self_ptr = e.register(x86_const.UC_X86_REG_ECX)
            e.write(self_ptr + 0x30, struct.pack("<I", e.stack_arg(0)))
            e.write(self_ptr + 0x34, struct.pack("<I", e.stack_arg(1)))
            e.write(self_ptr + 0x38, struct.pack("<I", e.stack_arg(2)))
            return 0

        def v6(e: Emulator) -> int:
            return u32(e.read(e.register(x86_const.UC_X86_REG_ECX) + 0x40, 4), 0)

        def v10(e: Emulator) -> int:
            self_ptr = e.register(x86_const.UC_X86_REG_ECX)
            for index in range(4):
                e.write(self_ptr + 0x50 + index * 4, struct.pack("<I", e.stack_arg(index)))
            return 0

        def lookup(e: Emulator) -> int:
            name = e.stack_arg(0)
            if name == 0x00C58C28:
                return 0x41
            if name == 0x00C58C18:
                return self.second_id
            return 0xFFFFFFFF

        def query(_e: Emulator) -> int:
            return 0

        def match_bin(e: Emulator) -> int:
            return 1 if e.read(e.register(x86_const.UC_X86_REG_ECX) + 0x70, 1)[0] else 0

        def select_pair(e: Emulator) -> int:
            obj = e.register(x86_const.UC_X86_REG_ECX)
            e.write(obj + 0x40, struct.pack("<I", e.stack_arg(0)))
            e.write(obj + 0x44, struct.pack("<I", e.stack_arg(1)))
            return 0

        def service(e: Emulator) -> int:
            return self.service_ptr

        def reserve(_e: Emulator) -> int:
            return 0

        def submit(e: Emulator) -> int:
            obj = e.register(x86_const.UC_X86_REG_ECX)
            cur = u32(e.read(obj + 0x90, 4), 0)
            e.write(obj + 0x90, struct.pack("<I", (cur + 1) & 0xFFFFFFFF))
            return 0

        self.vstubs = {
            0: emu.bind_stub(v0, 4),
            1: emu.bind_stub(v1, 4),
            2: emu.bind_stub(v2, 4),
            3: emu.bind_stub(v3, 4),
            4: emu.bind_stub(v4, 4),
            5: emu.bind_stub(v5, 12),
            6: emu.bind_stub(v6, 0),
            10: emu.bind_stub(v10, 16),
        }
        emu.bind_function(0x009C1730, lookup, 4)
        emu.bind_function(0x009C4870, query, 12)
        emu.bind_function(0x009B3240, match_bin, 12)
        emu.bind_function(0x009C6340, select_pair, 8)
        emu.bind_function(0x00416BB0, service, 0)
        emu.bind_function(0x009B93D0, reserve, 8)
        emu.bind_function(0x009B7EF0, submit, 16)
        self.second_id = 0x42
        self.service_ptr = 0

    def close(self) -> None:
        self.cpp.close()

    def _pointer(self, base: int, size: int, value: int) -> int:
        if value == NULL or value >= size:
            return 0
        return base + value

    def _relativize(self, raw: bytes, base: int) -> tuple[int, bytes]:
        out = bytearray(raw)
        kind_by_addr = {addr: kind for kind, addr in self.vstubs.items()}
        for offset in range(0, len(out) - 3, 4):
            cur = u32(out, offset)
            if base <= cur < base + len(out):
                put(out, offset, cur - base)
            elif cur in kind_by_addr:
                put(out, offset, 0xF0000000 | kind_by_addr[cur])
        return 0, bytes(out)

    def _ret(self, eax: int, base: int, size: int) -> int:
        if base <= eax < base + size:
            return eax - base
        return eax

    def check(self, va: int, blob: bytearray, fixups: list[str], self_off: int,
              a0: int = 0, a1: int = 0, a2: int = 0, name: str = "") -> None:
        self.counts[va] = self.counts.get(va, 0) + 1
        label = f"0x{va:08X} {name}"
        try:
            cpp_ret, cpp_blob = self.cpp.call(va, self_off, blob, fixups, a0, a1, a2)
        except RuntimeError as error:
            self.failures.append(f"{label} checker {error}")
            return
        try:
            emu_ret, emu_blob = self._emulate(va, blob, fixups, self_off, a0, a1, a2)
        except Exception as error:  # noqa: BLE001 - report and continue
            self.failures.append(f"{label} emu {error}")
            return
        if va in IGNORE_EAX:
            emu_ret = cpp_ret
        if emu_ret != cpp_ret or emu_blob != cpp_blob:
            detail = f"ret cpp=0x{cpp_ret:08X} emu=0x{emu_ret:08X}"
            if emu_blob != cpp_blob:
                for index, (left, right) in enumerate(zip(cpp_blob, emu_blob)):
                    if left != right:
                        detail += f" off=0x{index:X} cpp=0x{left:02X} emu=0x{right:02X}"
                        break
                if len(cpp_blob) != len(emu_blob):
                    detail += " length"
            self.failures.append(f"{label} {detail}")

    def _emulate(self, va, blob, fixups, self_off, a0, a1, a2):
        emu = self.emu
        emu.reset_data()
        emu.uc.reg_write(x86_const.UC_X86_REG_FPCW, 0x037F)
        emu.uc.reg_write(x86_const.UC_X86_REG_FPTAG, 0xFFFF)
        raw = bytearray(blob)
        base = emu.alloc(bytes(raw))
        for item in fixups:
            if item[0] in "vV" and "@" in item:
                kind_text, off_text = item[1:].split("@", 1)
                offset = int(off_text, 0)
                emu.write(base + offset, struct.pack("<I", self.vstubs[int(kind_text)]))
            else:
                offset = int(item, 0)
                rel = u32(raw, offset)
                emu.write(base + offset, struct.pack("<I", base + rel))
        self._prepare(va, base, raw, a0, a1)
        cleanup, use_ecx, sources = ABI[va]
        slots = {"s": self_off, "0": a0, "1": a1, "2": a2}
        stack = []
        for source in sources:
            if source == "z":
                stack.append(0)
            elif source.startswith("i"):
                stack.append(slots[source[1]] & 0xFFFFFFFF)
            else:
                stack.append(self._pointer(base, len(raw), slots[source]))
        ecx = self._pointer(base, len(raw), self_off) if use_ecx else 0
        result = emu.call(va, stack, ecx=ecx)
        if result.stack_cleanup != cleanup:
            raise RuntimeError(f"cleanup {result.stack_cleanup} != {cleanup}")
        produced = emu.read(base, len(raw))
        _unused, normalized = self._relativize(produced, base)
        return self._ret(result.eax, base, len(raw)), normalized

    def _prepare(self, va: int, base: int, raw: bytearray, a0: int, a1: int) -> None:
        emu = self.emu
        if va in (0x0099F1C0, 0x009A8EA0):
            begin = u32(raw, 0)
            end = u32(raw, 4)
            emu.write(0x00E1A588, struct.pack("<I", base + begin))
            emu.write(0x00E1A58C, struct.pack("<I", base + end))
            emu.write(0x00E1B314, struct.pack("<I", u32(raw, 8)))
            emu.write(0x00E1B318, bytes([raw[0x10], 0, 0, 0]))
        if va == 0x009A2540:
            emu.write(0x00E1B320, bytes([raw[0], 0, 0, 0]))
            text = u32(raw, 4)
            emu.write(0x00E1B31C, struct.pack("<I", 0 if text == 0 else base + text))
        if va == 0x0099F310:
            self.second_id = 0x42 if a0 == 0 else a0
        if va == 0x009A1990:
            service = u32(raw, 0)
            self.service_ptr = 0 if service == NULL or service >= len(raw) else base + service


def record48(seed: int) -> bytes:
    data = bytearray(0x48)
    for index in range(0x12):
        if index == 0x10:
            put(data, index * 4, f32(1.0 + seed))
        elif index < 0x0C:
            put(data, index * 4, f32(0.25 * (index + 1 + seed)))
        else:
            put(data, index * 4, 0x1000 + seed * 0x100 + index)
    return bytes(data)


def record50(seed: int) -> bytes:
    data = bytearray(0x50)
    put(data, 0, 0xA000 + seed)
    put(data, 4, 0xB000 + seed)
    for index in range(2, 14):
        put(data, index * 4, f32(0.5 * (index + seed)))
    for index in range(14, 20):
        put(data, index * 4, 0xC000 + seed * 0x20 + index)
    return bytes(data)


def matrix(values: list[float]) -> bytes:
    data = bytearray(16 * 4)
    for index, value in enumerate(values):
        put(data, index * 4, f32(value))
    return bytes(data)


def build(oracle: Oracle) -> None:
    # 0x00990080 remove from an empty quadtree cell (span is zero, so the walk stops).
    for linked in (0, 1):
        blob = zeros(0x190)
        put(blob, 0x30 + 0x14, 0x80 if linked else 0)
        put(blob, 0x30 + 0x10, 0x90)
        put(blob, 0x30 + 0x20, 0)
        put(blob, 0x30 + 0x24, 1)
        put(blob, 0x180, 2)
        fix = ["0x40", "0x44"] if linked else ["0x40"]
        oracle.check(0x00990080, blob, fix, 0, a0=0x30, name=f"link{linked}")

    # 0x00990100 empty span, with and without a side list.
    blob = zeros(0x190)
    oracle.check(0x00990100, blob, [], 0, name="empty")
    blob = zeros(0x190)
    put(blob, 0x178, 0x40)
    put(blob, 0x40 + 0x10, 0x20)
    put(blob, 0x40 + 0x0C, 0)
    put(blob, 0x20, 0x99)
    oracle.check(0x00990100, blob, ["0x178", "0x50"], 0, name="list")

    # 0x009901E0 world bounds, list empty so 0x0098FFC0 is not called.
    for width, height in ((2.0, 1.0), (1.0, 4.0), (3.0, 3.0)):
        blob = zeros(0x190)
        bounds = matrix([0, 0, 0, width, height, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0])
        blob[0x40:0x40 + 24] = bounds[:24]
        oracle.check(0x009901E0, blob, [], 0, a0=0x40, name=f"w{width}")

    # 0x00990620 resets the inner partition at +0x28.
    blob = zeros(0x1C0)
    put(blob, 0x28, 0x40)
    oracle.check(0x00990620, blob, ["0x28"], 0, name="zero")

    # 0x00993120 unlink. Owner, prev, and optional pair links live in the blob.
    blob = zeros(0x200)
    put(blob, 0x00, 0x40)  # owner
    put(blob, 0x40 + 4, 3)
    put(blob, 0x0C, 0x80)  # prev
    put(blob, 0x10, 0x90)  # next
    put(blob, 0x90 + 0x0C, 0)
    put(blob, 0x28, 0)
    put(blob, 0x14, 0)
    oracle.check(0x00993120, blob, ["0x0", "0x0C", "0x10"], 0, name="plain")
    blob = zeros(0x220)
    put(blob, 0x00, 0x40)
    put(blob, 0x40 + 4, 1)
    put(blob, 0x0C, 0x80)
    put(blob, 0x10, 0)
    put(blob, 0x28, 1)
    put(blob, 0x14, 0xA0)
    put(blob, 0x18, 0xB0)
    put(blob, 0xA0, 0)
    put(blob, 0xB0 + 0x14, 0)
    for index in range(6):
        put(blob, 0x28 + index * 0x14, 0)  # left
        put(blob, 0x2C + index * 0x14, 0)  # right
    oracle.check(0x00993120, blob, ["0x0", "0x0C", "0x14", "0x18"], 0, name="pairs")

    # 0x00993360 empty hash, then one idle node that stays linked.
    blob = zeros(0xC080)
    oracle.check(0x00993360, blob, [], 0, name="empty")
    blob = zeros(0xC080)
    put(blob, 0xAE10, 0x100)
    put(blob, 0x100, 0x140)  # primary
    put(blob, 0x104, 0x160)  # secondary
    put(blob, 0x140 + 0x14, 0)
    put(blob, 0x160 + 0x14, 0)
    put(blob, 0x100 + 0x30, 0)
    oracle.check(0x00993360, blob, ["0xAE10", "0x100", "0x104"], 0, name="idle")

    # 0x00993600 hash miss and one hit.
    blob = zeros(0x1280)
    put(blob, 0x40 + 8, 0)
    put(blob, 0x40 + 0x0C, 0)
    oracle.check(0x00993600, blob, [], 0, a0=0x40, name="miss")
    blob = zeros(0x1600)
    put(blob, 0, 0x1300)
    put(blob, 0x1300 + 8, 0)
    put(blob, 0x1300 + 0x0C, 0)
    put(blob, 0x1300 + 0x2C, 0x1400)
    put(blob, 0x1300 + 0x30, 0)
    put(blob, 0x1254, 0x1300)
    put(blob, 0x40 + 8, 0)
    put(blob, 0x40 + 0x0C, 0)
    oracle.check(0x00993600, blob, ["0x0", "0x132C", "0x1254"], 0, a0=0x40, name="hit")

    # 0x009937E0 owner with an empty wrapper list.
    blob = zeros(0xB000)
    put(blob, 0x40 + 0x24, 0)
    oracle.check(0x009937E0, blob, [], 0, a0=0x40, name="empty")

    # 0x009938A0 null drawable, null registration, and a live registration.
    blob = zeros(0x40)
    oracle.check(0x009938A0, blob, [], 0, a0=NULL, name="null")
    blob = zeros(0xC080)
    put(blob, 0x100, 0x140)  # drawable vtable
    oracle.check(0x009938A0, blob, ["0x100", "v6@0x15C"], 0, a0=0x100, name="noreg")
    blob = zeros(0xC080)
    put(blob, 0x40, 0x200)  # drawable vtable at 0x200
    put(blob, 0x40 + 0x40, 0x300)  # registration
    blob[0xC06D] = 1
    put(blob, 0x08, 0)
    put(blob, 0x300 + 0x14, 0)
    oracle.check(0x009938A0, blob, ["0x40", "0x80", "v6@0x21C"], 0, a0=0x40, name="link")

    # 0x00995D00 / compose / in-place compose share the shipped matrix math.
    ident = [1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1]
    tilted = [0, 1, 0, 0, 0, 0, 1, 0, 1, 0, 0, 0, 0.5, -1, 2, 1]
    for name, values in (("ident", ident), ("tilt", tilted)):
        blob = zeros(0x80)
        blob[0x10:0x50] = matrix(values)
        oracle.check(0x00995D00, blob, [], NULL, a0=0x00, a1=0x10, name=name)
        blob = zeros(0x90)
        blob[0x00:0x40] = matrix(ident)
        blob[0x40:0x80] = matrix(values)
        oracle.check(0x0099C290, blob, [], NULL, a0=0x00, a1=0x40, a2=0x80, name=name)
        blob = zeros(0x80)
        blob[0x00:0x40] = matrix(ident)
        blob[0x40:0x80] = matrix(values)
        oracle.check(0x0099C440, blob, [], 0, a0=0x40, name=name)

    # 0x00997720 forwards three parsed words through the console virtual.
    for words in ((1, 2, 3), (0, 0, 0), (0x10, 0x20, 0x30)):
        blob = zeros(0xC0)
        put(blob, 0x00, 0x40)
        put(blob, 0x50 + 8, words[0])
        put(blob, 0x50 + 0x0C, words[1])
        put(blob, 0x50 + 0x10, words[2])
        oracle.check(0x00997720, blob, ["0x0", "v5@0xAC"], NULL, a0=0x00, a1=0x50, name=str(words[0]))

    # Shader-parameter copies.
    one = record48(1)
    two = record48(2)
    blob = zeros(0x100)
    oracle.check(0x00997EF0, blob, [], NULL, a0=0x10, a1=0x10, a2=NULL, name="empty")
    blob = zeros(0x120)
    blob[0x80:0xC8] = one
    oracle.check(0x00997EF0, blob, [], NULL, a0=0x00, a1=0x48, a2=0x80, name="one")
    blob = zeros(0x180)
    blob[0x100:0x148] = one
    oracle.check(0x00997EF0, blob, [], NULL, a0=0x00, a1=0x90, a2=0x100, name="two")
    blob = zeros(0x100)
    blob[0x00:0x48] = one
    oracle.check(0x00997FD0, blob, [], NULL, a0=0x00, a1=0x00, a2=0x80, name="empty")
    blob = zeros(0x120)
    blob[0x00:0x48] = one
    oracle.check(0x00997FD0, blob, [], NULL, a0=0x00, a1=0x48, a2=0x60, name="one")
    blob = zeros(0x100)
    oracle.check(0x009980D0, blob, [], NULL, a0=0x48, a1=0x48, a2=0x90, name="empty")
    blob = zeros(0x120)
    blob[0x00:0x48] = two
    oracle.check(0x009980D0, blob, [], NULL, a0=0x00, a1=0x48, a2=0xD8, name="one")

    blob = zeros(0x50)
    paint(blob, 0, 0x12, 0x11111111)
    oracle.check(0x009984F0, blob, [], 0, name="fill")
    blob = zeros(0xA0)
    for index in range(0x0C):
        put(blob, 0x50 + index * 4, f32(0.1 * (index + 1)))
    for index in range(0x0C, 0x12):
        put(blob, 0x50 + index * 4, 0x22220000 + index)
    oracle.check(0x00998F40, blob, [], 0, a0=0x50, name="assign")

    # 0x0099A8E0 nested name word.
    blob = zeros(0x40)
    oracle.check(0x0099A8E0, blob, [], 0, a0=0x20, name="null")
    blob = zeros(0x40)
    put(blob, 0, 0x10)
    put(blob, 0x10 + 0x0C, 0)
    oracle.check(0x0099A8E0, blob, ["0x0"], 0, a0=0x20, name="inner0")
    blob = zeros(0x40)
    put(blob, 0, 0x10)
    put(blob, 0x1C, 0x30)
    put(blob, 0x30, 0xABC0)
    oracle.check(0x0099A8E0, blob, ["0x0", "0x1C"], 0, a0=0x20, name="word")

    # 0x0099B010 refcounted assign.
    def ref_case(name: str, same: bool, incoming: bool, previous: bool) -> None:
        blob = zeros(0xA0)
        put(blob, 0x30, 0x80)
        put(blob, 0x58, 0x80)
        if same:
            oracle.check(0x0099B010, blob, [], 0, a0=0x00, name=name)
            return
        fixups = ["0x30", "0x58", "v0@0x84", "v1@0x88"]
        if incoming:
            put(blob, 0x04, 0x30)
            fixups.append("0x4")
        if previous:
            put(blob, 0x00, 0x58)
            fixups.append("0x0")
        oracle.check(0x0099B010, blob, fixups, 0, a0=0x04, name=name)

    ref_case("same", True, False, False)
    ref_case("in", False, True, False)
    ref_case("out", False, False, True)
    ref_case("swap", False, True, True)

    # 0x0099BD10 sub-object records.
    blob = zeros(0x40)
    put(blob, 0x00, 0x99)
    oracle.check(0x0099BD10, blob, [], 0, a0=0x10, a1=0x10, a2=0x20, name="empty")
    blob = zeros(0x80)
    put(blob, 0x20, 0x111)
    put(blob, 0x24, 0x222)
    put(blob, 0x28, 0)
    put(blob, 0x2C, 0x333)
    put(blob, 0x30, 0x444)
    oracle.check(0x0099BD10, blob, [], 0, a0=0x20, a1=0x34, a2=0x40, name="noref")
    blob = zeros(0x80)
    put(blob, 0x20, 1)
    put(blob, 0x24, 2)
    put(blob, 0x28, 0x50)
    put(blob, 0x50 + 4, 7)
    put(blob, 0x2C, 3)
    put(blob, 0x30, 4)
    oracle.check(0x0099BD10, blob, ["0x28"], 0, a0=0x20, a1=0x34, a2=0x40, name="ref")

    # 0x0099BD70 extended records call the real 0x008E5F70 tail.
    blob = zeros(0x80)
    oracle.check(0x0099BD70, blob, [], 0, a0=0x10, a1=0x10, a2=0x40, name="empty")
    blob = zeros(0xE0)
    blob[0x10:0x60] = record50(1)
    oracle.check(0x0099BD70, blob, [], 0, a0=0x10, a1=0x60, a2=0x70, name="one")

    # 0x0099BE10 and the range that calls it.
    blob = zeros(0xA0)
    blob[0x50:0x98] = record48(3)
    oracle.check(0x0099BE10, blob, [], 0, a0=0x50, name="one")
    blob = zeros(0x100)
    blob[0x10:0x58] = record48(4)
    oracle.check(0x0099C0A0, blob, [], 0, a0=0x10, a1=0x58, a2=0x70, name="one")
    blob = zeros(0x40)
    oracle.check(0x0099C0A0, blob, [], 0, a0=0x10, a1=0x10, a2=NULL, name="empty")

    # 0x0099C160 backward extended copy.
    blob = zeros(0xC0)
    oracle.check(0x0099C160, blob, [], NULL, a0=0x50, a1=0x50, a2=0xA0, name="empty")
    blob = zeros(0x100)
    blob[0x00:0x50] = record50(5)
    oracle.check(0x0099C160, blob, [], NULL, a0=0x00, a1=0x50, a2=0xF0, name="one")

    # 0x0099F1C0 registry lookup. Ready bit avoids the SecuROM thunk.
    blob = zeros(0x30)
    put(blob, 0, 0x14)
    put(blob, 4, 0x14)
    blob[0x10] = 1
    put(blob, 0x18, 0)
    oracle.check(0x0099F1C0, blob, [], NULL, a0=0x18, a1=0x1C, name="zero")
    blob = zeros(0x50)
    put(blob, 0, 0x14)
    put(blob, 4, 0x24)
    blob[0x10] = 1
    put(blob, 0x14, 0x30)  # object
    put(blob, 0x30 + 0x0C, 0x40)
    put(blob, 0x40, 0x55AA)
    put(blob, 0x30 + 4, 4)
    put(blob, 0x48, 0x55AA)  # key
    oracle.check(0x0099F1C0, blob, ["0x14", "0x3C"], NULL, a0=0x4C, a1=0x48, name="hit")

    # 0x0099F310 constant ids. Query stub returns 0, so the virtual bind is skipped.
    for second, name in ((0, "default"), (7, "seven"), (0xFFFFFFFF, "copy")):
        blob = zeros(0x40)
        put(blob, 0x00, 0x10)
        put(blob, 0x10 + 0x0C, 0x20)
        put(blob, 0x10 + 0x1C, 0x30)
        put(blob, 0x30 + 4, 0x34)
        oracle.check(0x0099F310, blob, ["0x0", "0x1C", "0x2C", "0x34"], 0, a0=second, name=name)

    # 0x009A0500 replicate.
    blob = zeros(0xA0)
    blob[0x50:0x98] = record48(6)
    oracle.check(0x009A0500, blob, [], NULL, a0=0x00, a1=0, a2=0x50, name="zero")
    oracle.check(0x009A0500, bytearray(blob), [], NULL, a0=0x00, a1=1, a2=0x50, name="one")

    # 0x009A0640 backward move of records whose ref slots are null.
    blob = zeros(0x40)
    put(blob, 0x00, 1)
    put(blob, 0x04, 2)
    put(blob, 0x08, 0)
    put(blob, 0x0C, 3)
    put(blob, 0x10, 4)
    oracle.check(0x009A0640, blob, [], NULL, a0=0x00, a1=0x14, a2=0x3C, name="one")
    blob = zeros(0x20)
    oracle.check(0x009A0640, blob, [], NULL, a0=0x00, a1=0x00, a2=0x14, name="empty")

    # 0x009A1990 rejects fewer than two tokens.
    for count in (0, 1, -1):
        blob = zeros(0x20)
        put(blob, 0x10 + 0x0C, count & 0xFFFFFFFF)
        oracle.check(0x009A1990, blob, [], NULL, a0=0x00, a1=0x04, a2=0x10, name=f"n{count}")

    # 0x009A2540 fast path: ready bit and a cached string with a non-zero length prefix.
    for length in (1, 2, 9):
        blob = zeros(0x20)
        blob[0] = 1
        put(blob, 4, 0x10)
        struct.pack_into("<H", blob, 0x0C, length)
        oracle.check(0x009A2540, blob, [], NULL, name=f"len{length}")

    # 0x009A2DF0 refresh. An empty chain pointer is not safe; supply one pose and a matching matrix
    # only after the quaternion helper is consistent. The empty-flag rewrite is covered by a
    # zero pose against a non-zero translation, which always takes the rewrite.
    blob = zeros(0x140)
    blob[0x100:0x140] = matrix(ident)
    blob[0x30] = 1
    put(blob, 8, 0x40)
    put(blob, 0x0C, 0x88)
    oracle.check(0x009A2DF0, blob, ["0x8", "0xC"], 0, a0=0x100, name="flag")

    # 0x009A82B0 empty instance list still refreshes the chain at +0x74.
    blob = zeros(0x160)
    put(blob, 8, 0x40)
    put(blob, 0x0C, 0x40)
    put(blob, 0x7C, 0xC0)
    put(blob, 0x80, 0x108)
    blob[0xA4] = 1
    oracle.check(0x009A82B0, blob, ["0x8", "0xC", "0x7C", "0x80"], 0, name="empty")

    # 0x009A8EA0 failed lookup releases the asset. Key 0 never touches the registry globals.
    blob = zeros(0x20)
    blob[0x10] = 1
    put(blob, 0x18, 0)
    oracle.check(0x009A8EA0, blob, [], NULL, a0=NULL, a1=0x18, name="null")
    blob = zeros(0x30)
    blob[0x10] = 1
    put(blob, 0x00 + 4, 4)
    put(blob, 0x18, 0)
    oracle.check(0x009A8EA0, blob, [], NULL, a0=0x00, a1=0x18, name="keep")
    blob = zeros(0x40)
    blob[0x10] = 1
    put(blob, 0x00, 0x20)
    put(blob, 0x04, 1)
    put(blob, 0x18, 0)
    oracle.check(0x009A8EA0, blob, ["0x0", "v2@0x20"], NULL, a0=0x00, a1=0x18, name="drop")

    # 0x009AB8E0 truncate only. Three 0x48 records, shrink to 1 and to 0.
    def vector(count: int, keep: int) -> None:
        blob = zeros(0x120)
        put(blob, 0, 0x20)
        put(blob, 4, 0x20 + count * 0x48)
        put(blob, 8, 0xF8)
        for index in range(count):
            blob[0x20 + index * 0x48:0x20 + (index + 1) * 0x48] = record48(index + 1)
        oracle.check(0x009AB8E0, blob, ["0x0", "0x4", "0x8"], 0, a0=keep, name=f"{count}to{keep}")

    vector(2, 2)
    vector(2, 1)
    vector(1, 0)

    # 0x009ACB60 count 0 copies the 8-byte header and leaves the vector empty.
    blob = zeros(0x40)
    put(blob, 0x20, 0x30)  # *descriptor
    put(blob, 0x30, 0x111)
    put(blob, 0x34, 0x222)
    put(blob, 0x24, 0)  # count
    put(blob, 0x08, 0x10)
    put(blob, 0x0C, 0x10)
    put(blob, 0x10, 0x18)
    oracle.check(0x009ACB60, blob, ["0x20", "0x8", "0xC", "0x10"], 0, a0=0x20, name="empty")

    # 0x009ACFE0 append while capacity remains.
    blob = zeros(0x50)
    put(blob, 0, 0x20)
    put(blob, 4, 0x20)
    put(blob, 8, 0x40)
    put(blob, 0x30, 9)
    put(blob, 0x34, 8)
    put(blob, 0x38, 0)
    put(blob, 0x3C, 7)
    put(blob, 0x40 - 0x10, 6)
    oracle.check(0x009ACFE0, blob, ["0x0", "0x4", "0x8"], 0, a0=0x30, name="room")

    # 0x00992F70 wide-hash lookup. Key 0 hashes to bucket 0.
    blob = zeros(0x80)
    put(blob, 0x40 + 8, 0)
    put(blob, 0x40 + 0x0C, 0)
    oracle.check(0x00992F70, blob, [], 0, a0=0x40, name="miss")
    blob = zeros(0x80)
    put(blob, 0, 0x20)
    put(blob, 0x20 + 8, 0)
    put(blob, 0x20 + 0x0C, 1)
    put(blob, 0x20 + 0x30, 0x30)
    put(blob, 0x30 + 8, 0)
    put(blob, 0x30 + 0x0C, 0)
    put(blob, 0x30 + 0x30, 0)
    put(blob, 0x40 + 8, 0)
    put(blob, 0x40 + 0x0C, 0)
    oracle.check(0x00992F70, blob, ["0x0", "0x50"], 0, a0=0x40, name="hit")

    # 0x0099A790 '+' / '-' masks. Count <= 2 skips the prefix loop.
    blob = zeros(0xC0)
    put(blob, 0x00, 0x40)  # console vtable
    put(blob, 0x50, 0x60)  # token array
    put(blob, 0x60 + 0x0C, 0x70)  # token0
    put(blob, 0x70, 0x1234)
    put(blob, 0x50 + 0x0C, 2)
    oracle.check(0x0099A790, blob, ["0x0", "0x50", "0x6C", "v3@0xA4"], NULL, a0=0x00, a1=0x10, a2=0x50, name="two")
    blob = zeros(0xC0)
    put(blob, 0x00, 0x40)
    put(blob, 0x50, 0x60)
    put(blob, 0x6C, 0x70)
    put(blob, 0x70, 0x55)
    put(blob, 0x50 + 0x0C, 4)
    blob[0x80:0x84] = b"+AB\x00"
    blob[0x90:0x94] = b"-CD\x00"
    put(blob, 0x60 + 2 * 8 + 4, 0x80)
    put(blob, 0x60 + 3 * 8 + 4, 0x90)
    oracle.check(
        0x0099A790, blob,
        ["0x0", "0x50", "0x6C", "0x74", "0x7C", "v3@0xA4", "v4@0xA8"],
        NULL, a0=0x00, a1=0x10, a2=0x50, name="signs")

    # 0x0099A850 match failure leaves the console unchanged.
    for flag in (0, 0):
        blob = zeros(0x80)
        blob[0x70] = flag
        put(blob, 0x10, 0)  # *args
        put(blob, 0x20, 0)  # *nameSlot
        oracle.check(0x0099A850, blob, [], 0x20, a0=0x00, a1=0x08, a2=0x0C, name=f"flag{flag}")
    blob = zeros(0x80)
    blob[0x70] = 0
    put(blob, 0x0C, 0x30)
    put(blob, 0x30, 0x111)
    put(blob, 0x34, 0x222)
    put(blob, 0x20, 0x40)
    put(blob, 0x40, 0x333)
    oracle.check(0x0099A850, blob, ["0x0C", "0x20"], 0x20, a0=0x00, a1=0x08, a2=0x0C, name="deref")


def main() -> None:
    only = int(sys.argv[1], 16) if len(sys.argv) > 1 else 0
    oracle = Oracle()
    try:
        build(oracle)
    finally:
        oracle.close()
    if only:
        oracle.failures = [line for line in oracle.failures if f"0x{only:08X}" in line]
        print(f"filtered {only:08X} fails {len(oracle.failures)}")
    for line in oracle.failures[:40]:
        print("FAIL", line)
    missing = [va for va in COUNTED if oracle.counts.get(va, 0) == 0]
    total = sum(oracle.counts.values())
    print(
        f"render99 oracle: {len(oracle.counts)} entries, {total} cases, "
        f"{len(oracle.failures)} mismatches"
        + ("" if not missing else " UNTESTED " + ",".join(f"0x{va:08X}" for va in missing))
    )
    if oracle.failures or missing:
        raise SystemExit(1)


if __name__ == "__main__":
    main()
