"""Differentially verify the second render-script pass against ra3_1.0.game."""

from __future__ import annotations

import struct
import subprocess
import sys
from pathlib import Path

from unicorn import x86_const

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from emulate import Emulator  # noqa: E402

ROOT = Path(__file__).resolve().parents[4]
EXE = ROOT / "Ra3Source" / "build" / "win32" / "bin" / "Release" / "ra3_render9b2check.exe"
NULL = 0xFFFFFFFF

COUNTED = (
    0x009BCC40, 0x009BCD20, 0x009BCB10, 0x009B9D50, 0x009BBFA0, 0x009BC070, 0x009BC310,
    0x009BD1D0, 0x009BD380, 0x009BD460, 0x009BD560, 0x009BBE20, 0x009BB930, 0x009BBB30,
    0x009BBD30, 0x009B9C40, 0x009BC420, 0x009BC780, 0x009B6BD0, 0x009B6CA0, 0x009B6D70,
    0x009BACE0, 0x009BBA80, 0x009BBC80, 0x009BBDB0, 0x009B58C0, 0x009B5960, 0x009B59D0,
    0x009B5A40, 0x009B5AF0, 0x009B5B60, 0x009B3910, 0x009B5380, 0x009B5430, 0x009BB040,
    0x009B9630,
)

ABI = {va: (0x0C, False, ("p", "p", "p", "i")) for va in (
    0x009BCC40, 0x009BCD20, 0x009BCB10, 0x009B9D50, 0x009BBFA0, 0x009BC070, 0x009BC310,
    0x009BD1D0, 0x009BD380, 0x009BD460, 0x009BD560, 0x009BBE20, 0x009BB930, 0x009BBB30,
    0x009BBD30,
)}
ABI[0x009B9C40] = (0x0C, True, ("p", "p", "p", "i"))
for va in (0x009BC420, 0x009BC780, 0x009B6BD0, 0x009B6CA0, 0x009B6D70, 0x009BACE0,
           0x009BBA80, 0x009BBC80, 0x009BBDB0):
    ABI[va] = (0x10, False, ("p", "p", "p", "p"))
for va in (0x009B58C0, 0x009B5960, 0x009B59D0, 0x009B5A40, 0x009B5AF0, 0x009B5B60, 0x009B5430):
    ABI[va] = (0, True, ("i", "i", "i", "i"))
ABI[0x009B3910] = (8, True, ("i", "i", "i", "i"))
ABI[0x009B5380] = (4, False, ("p", "i", "i", "i"))
ABI[0x009BB040] = (0x0C, True, ("i", "i", "p", "i"))
ABI[0x009B9630] = (8, True, ("p", "p", "i", "i"))


def u32(blob: bytes, offset: int) -> int:
    return struct.unpack_from("<I", blob, offset)[0]


def put(blob: bytearray, offset: int, value: int) -> None:
    struct.pack_into("<I", blob, offset, value & 0xFFFFFFFF)


def zeros(size: int) -> bytearray:
    return bytearray(size)


def cstr(blob: bytearray, offset: int, text: str) -> None:
    raw = text.encode("ascii") + b"\0"
    blob[offset:offset + len(raw)] = raw


def log_append(e: Emulator, device: int, ident: int, a: int, b: int, c: int) -> None:
    count = u32(e.read(device + 0x300, 4), 0)
    e.write(device + 0x304 + count * 16, struct.pack("<IIII", ident, a, b, c))
    e.write(device + 0x300, struct.pack("<I", count + 1))


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

    def call(self, va, self_off, blob, fixups, a0, a1, a2, a3) -> tuple[int, bytes]:
        assert self.process is not None and self.process.stdin and self.process.stdout
        fix = ",".join(fixups) if fixups else "-"
        line = (
            f"0x{va:08X} 0x{self_off:X} {bytes(blob).hex()} {fix} "
            f"0x{a0 & 0xFFFFFFFF:X} 0x{a1 & 0xFFFFFFFF:X} 0x{a2 & 0xFFFFFFFF:X} 0x037F "
            f"0x{a3 & 0xFFFFFFFF:X}\n"
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


def _scan_number(text: bytes, floating: bool):
    index = 0
    while index < len(text) and text[index] in (32, 9):
        index += 1
    if index >= len(text):
        return None
    sign = 1
    if text[index] in (ord("+"), ord("-")):
        if text[index] == ord("-"):
            sign = -1
        index += 1
    if index >= len(text) or not (text[index] == ord(".") or 48 <= text[index] <= 57):
        return None
    if not floating and text[index] == ord("."):
        return None
    whole = 0
    any_digit = False
    while index < len(text) and 48 <= text[index] <= 57:
        whole = whole * 10 + text[index] - 48
        index += 1
        any_digit = True
    frac = 0
    scale = 1
    if floating and index < len(text) and text[index] == ord("."):
        index += 1
        while index < len(text) and 48 <= text[index] <= 57:
            frac = frac * 10 + text[index] - 48
            scale *= 10
            index += 1
            any_digit = True
    if not any_digit:
        return None
    if not floating:
        value = whole if sign > 0 else -whole
        return value & 0xFFFFFFFF
    magnitude = float(whole)
    if scale > 1:
        magnitude += float(frac) / float(scale)
    if sign < 0:
        magnitude = -magnitude
    return struct.unpack("<I", struct.pack("<f", magnitude))[0]


class Oracle:
    def __init__(self) -> None:
        self.emu = Emulator()
        self.cpp = Checker(EXE)
        self.vstubs: dict[int, int] = {}
        self.counts: dict[int, int] = {}
        self.failures: list[str] = []
        self.pool = 0
        self.manager = 0
        self.device = 0
        self._bind()

    def _bind(self) -> None:
        emu = self.emu

        def intern(e: Emulator) -> int:
            name = e.stack_arg(0)
            e.write(e.register(x86_const.UC_X86_REG_ECX) + 0x10, struct.pack("<I", name))
            return 0 if name == 0 else 0x42

        def release_key(e: Emulator) -> int:
            e.write(e.register(x86_const.UC_X86_REG_ECX) + 0x14, struct.pack("<I", e.stack_arg(0)))
            return 0

        def compare(e: Emulator) -> int:
            left = e.stack_arg(0)
            right = e.stack_arg(1)
            self_ptr = e.register(x86_const.UC_X86_REG_ECX)
            e.write(self_ptr + 0x18, struct.pack("<II", left, right))
            return 1 if left == (right ^ 0x01000000) else 0

        def alloc(e: Emulator) -> int:
            manager = e.register(x86_const.UC_X86_REG_ECX)
            size = e.stack_arg(0)
            nxt = u32(e.read(manager + 0x20, 4), 0)
            e.write(manager + 0x20, struct.pack("<I", (nxt + size) & 0xFFFFFFFF))
            return nxt

        def free(e: Emulator) -> int:
            e.write(e.register(x86_const.UC_X86_REG_ECX) + 0x48, struct.pack("<I", e.stack_arg(0)))
            return 0

        def query_name(e: Emulator) -> int:
            name = e.stack_arg(0)
            out = e.stack_arg(1)
            if name == 0x00C600D4:
                e.write(out, e.read(self.manager + 0x60, 4))
                return u32(e.read(self.manager + 0x64, 4), 0) & 0xFF
            e.write(out, e.read(self.manager + 0x68, 4))
            return u32(e.read(self.manager + 0x6C, 4), 0) & 0xFF

        def camera(e: Emulator) -> int:
            self_ptr = e.register(x86_const.UC_X86_REG_ECX)
            e.write(self_ptr + 0x200, struct.pack(
                "<IIIII", e.stack_arg(0), e.stack_arg(1), e.stack_arg(2), e.stack_arg(3), e.stack_arg(4)))
            return 0

        def dirty(e: Emulator) -> int:
            self_ptr = e.register(x86_const.UC_X86_REG_ECX)
            e.write(self_ptr + 0x40, struct.pack("<II", e.stack_arg(0), e.stack_arg(1)))
            return 0

        def node(e: Emulator) -> int:
            e.write(e.stack_arg(0) + 0x30, struct.pack("<III", 0xA1, e.stack_arg(1), e.stack_arg(2)))
            return 0

        def device_end(e: Emulator) -> int:
            e.write(e.stack_arg(0) + 0x90, struct.pack("<I", 1))
            return 0

        def device_set(e: Emulator) -> int:
            e.write(e.stack_arg(0) + 0x90, struct.pack("<II", 2, e.stack_arg(1)))
            return 0

        def device_same(e: Emulator) -> int:
            e.write(e.stack_arg(0) + 0x90, struct.pack("<I", 3))
            return 0

        def nested(e: Emulator) -> int:
            e.write(e.register(x86_const.UC_X86_REG_ECX) + 4, struct.pack("<I", 1))
            return 0

        def upload(e: Emulator) -> int:
            e.write(e.stack_arg(0) + 0x40, struct.pack(
                "<IIII", e.stack_arg(1), e.stack_arg(2), e.stack_arg(3), e.stack_arg(4)))
            return 0

        def stream(e: Emulator) -> int:
            return u32(e.read(e.register(x86_const.UC_X86_REG_ECX) + 0x30, 4), 0)

        def usage(e: Emulator) -> int:
            return u32(e.read(e.register(x86_const.UC_X86_REG_ECX) + 4, 4), 0)

        def obj(e: Emulator) -> int:
            return u32(e.read(e.register(x86_const.UC_X86_REG_ECX) + 8, 4), 0)

        def sscanf(e: Emulator) -> int:
            text = e.read(e.stack_arg(0), 32).split(b"\0", 1)[0]
            fmt = e.read(e.stack_arg(1), 4)
            dest = e.stack_arg(2)
            value = _scan_number(text, b"f" in fmt)
            if value is None:
                return 0
            e.write(dest, struct.pack("<I", value))
            return 1

        def pool(e: Emulator) -> int:
            del e
            return self.pool

        def manager(e: Emulator) -> int:
            del e
            return self.manager

        def raw_alloc(e: Emulator) -> int:
            size = e.stack_arg(0)
            nxt = u32(e.read(self.manager + 0x20, 4), 0)
            e.write(self.manager + 0x20, struct.pack("<I", (nxt + size) & 0xFFFFFFFF))
            return nxt

        def log(e: Emulator) -> int:
            e.write(self.manager + 0x50, struct.pack("<II", e.stack_arg(0), e.stack_arg(1)))
            return 0

        def zero(e: Emulator) -> int:
            del e
            return 0

        def equals(e: Emulator) -> int:
            entry = e.register(x86_const.UC_X86_REG_ECX)
            left = e.stack_arg(0)
            right = e.stack_arg(1)
            e.write(entry + 0x10, struct.pack("<II", left, right))
            return 1 if left == (right ^ 1) else 0

        def scripted(offset: int):
            def stub(e: Emulator) -> int:
                del e
                return u32(e_read(offset), 0)
            return stub

        def e_read(offset: int) -> bytes:
            return self.emu.read(self.manager + offset, 4)

        def name_match(e: Emulator) -> int:
            del e
            return u32(self.emu.read(self.manager + 0x30, 4), 0) & 0xFF

        def technique(e: Emulator) -> int:
            del e
            return u32(self.emu.read(self.manager + 0x34, 4), 0)

        def surface(e: Emulator) -> int:
            del e
            return u32(self.emu.read(self.manager + 0x38, 4), 0)

        def camera_index(e: Emulator) -> int:
            del e
            return u32(self.emu.read(self.manager + 0x3C, 4), 0)

        def gate(e: Emulator) -> int:
            device = e.stack_arg(0)
            self.device = device
            ok = u32(e.read(device + 0x88, 4), 0) & 0xFF
            if ok:
                log_append(e, device, 1, e.stack_arg(2), 0, 0)
            return ok

        def view(e: Emulator) -> int:
            e.write(e.stack_arg(0) + 8, e.read(e.register(x86_const.UC_X86_REG_ECX) + 0x80, 8))
            return 0

        def bind(e: Emulator) -> int:
            log_append(e, e.register(x86_const.UC_X86_REG_ECX), 2, e.stack_arg(0), e.stack_arg(1), 0)
            return 0

        def rsset(e: Emulator) -> int:
            log_append(e, self.device, 3, e.stack_arg(1), e.stack_arg(3), 0)
            return 0

        def vert(e: Emulator) -> int:
            log_append(e, self.device, 4, e.stack_arg(0), e.stack_arg(1), e.stack_arg(2))
            return 0

        def mark(e: Emulator) -> int:
            e.write(e.register(x86_const.UC_X86_REG_ECX) + 8, struct.pack("<I", 1))
            return 0

        def copy_rect(e: Emulator) -> int:
            device = e.register(x86_const.UC_X86_REG_ECX)
            e.write(device + 0x100, e.read(e.stack_arg(0), 16))
            return 0

        def memmove(e: Emulator) -> int:
            dest = e.stack_arg(0)
            source = e.stack_arg(1)
            size = e.stack_arg(2)
            e.write(dest, e.read(source, size))
            return dest

        def stricmp(e: Emulator) -> int:
            left = e.read(e.stack_arg(0), 32).split(b"\0", 1)[0].lower()
            right = e.read(e.stack_arg(1), 32).split(b"\0", 1)[0].lower()
            if left == right:
                return 0
            return -1 if left < right else 1

        self.vstubs = {
            13: emu.bind_stub(intern, 4),
            14: emu.bind_stub(release_key, 4),
            15: emu.bind_stub(compare, 8),
            16: emu.bind_stub(alloc, 12),
            17: emu.bind_stub(free, 8),
            18: emu.bind_stub(query_name, 8),
            19: emu.bind_stub(camera, 20),
            20: emu.bind_stub(dirty, 8),
            21: emu.bind_stub(node, 0),
            22: emu.bind_stub(device_end, 4),
            23: emu.bind_stub(device_set, 8),
            24: emu.bind_stub(device_same, 4),
            25: emu.bind_stub(nested, 0),
            26: emu.bind_stub(upload, 20),
            27: emu.bind_stub(stream, 4),
            28: emu.bind_stub(usage, 0),
            29: emu.bind_stub(obj, 0),
        }
        emu.bind_import(0x00BBA4E0, sscanf, 0)
        emu.bind_import(0x00BBA4D0, stricmp, 0)
        emu.bind_import(0x00BBA53C, memmove, 0)
        emu.bind_function(0x00997400, pool, 0)
        emu.bind_function(0x009973F0, manager, 0)
        emu.bind_function(0x00997380, log, 0)
        emu.bind_function(0x009976E0, raw_alloc, 0)
        emu.bind_function(0x009B93D0, zero, 8)
        emu.bind_function(0x009B9330, zero, 0)
        emu.bind_function(0x009BB8B0, zero, 0)
        emu.bind_function(0x009C13E0, equals, 8)
        emu.bind_function(0x009B2C50, name_match, 4)
        emu.bind_function(0x009B2CA0, name_match, 4)
        emu.bind_function(0x009B7670, technique, 8)
        emu.bind_function(0x009B7800, surface, 4)
        emu.bind_function(0x009B7BD0, camera_index, 4)
        emu.bind_function(0x009B3240, gate, 0x0C)
        emu.bind_function(0x009C1C70, view, 4)
        emu.bind_function(0x009C1D10, view, 4)
        emu.bind_function(0x009C6340, bind, 8)
        emu.bind_function(0x009BB420, zero, 4)
        emu.bind_function(0x009C46E0, zero, 0)
        emu.bind_function(0x009C2AD0, zero, 0)
        emu.bind_function(0x009C2AF0, rsset, 0x1C)
        emu.bind_function(0x009C2B90, zero, 0)
        emu.bind_function(0x009C64B0, zero, 4)
        emu.bind_function(0x009BB840, mark, 4)
        emu.bind_function(0x00561640, vert, 0x0C)
        emu.bind_function(0x009B98D0, zero, 0)
        emu.bind_function(0x009B2DA0, zero, 0)
        emu.bind_function(0x009C4710, zero, 0)
        emu.bind_function(0x009B97D0, zero, 0)
        emu.bind_function(0x009C1C20, copy_rect, 4)
        emu.bind_function(0x009C1CC0, copy_rect, 4)
        emu.bind_function(0x009C4B80, lambda e: (e.write(e.register(x86_const.UC_X86_REG_ECX) + 0x10, struct.pack("<I", 0xAB)) or 0), 0)
        emu.bind_function(0x009C4550, lambda e: (e.write(e.register(x86_const.UC_X86_REG_ECX) + 0x10C, struct.pack("<I", 0xCD)) or 0), 0)
        emu.bind_function(0x009C7350, zero, 0x18)
        del scripted

    def close(self) -> None:
        self.cpp.close()

    def _pointer(self, base: int, size: int, value: int) -> int:
        if value == NULL or value >= size:
            return 0
        return base + value

    def _relativize(self, raw: bytes, base: int) -> bytes:
        out = bytearray(raw)
        kind_by_addr = {addr: kind for kind, addr in self.vstubs.items()}
        for offset in range(0, len(out) - 3, 4):
            cur = u32(out, offset)
            if base <= cur < base + len(out):
                put(out, offset, cur - base)
            elif cur in kind_by_addr:
                put(out, offset, 0xF0000000 | kind_by_addr[cur])
        return bytes(out)

    def _ret(self, eax: int, base: int, size: int) -> int:
        if base <= eax < base + size:
            return eax - base
        return eax

    def check(self, va: int, blob: bytearray, fixups: list[str], self_off: int,
              a0: int = 0, a1: int = 0, a2: int = 0, a3: int = 0, name: str = "") -> None:
        self.counts[va] = self.counts.get(va, 0) + 1
        label = f"0x{va:08X} {name}"
        try:
            cpp_ret, cpp_blob = self.cpp.call(va, self_off, blob, fixups, a0, a1, a2, a3)
        except RuntimeError as error:
            self.failures.append(f"{label} checker {error}")
            return
        try:
            emu_ret, emu_blob = self._emulate(va, blob, fixups, self_off, a0, a1, a2, a3)
        except Exception as error:  # noqa: BLE001 - report and continue
            self.failures.append(f"{label} emu {error}")
            return
        if emu_ret != cpp_ret or emu_blob != cpp_blob:
            detail = f"ret cpp=0x{cpp_ret:08X} emu=0x{emu_ret:08X}"
            if emu_blob != cpp_blob:
                for index, (left, right) in enumerate(zip(cpp_blob, emu_blob)):
                    if left != right:
                        detail += f" off=0x{index:X} cpp=0x{left:02X} emu=0x{right:02X}"
                        break
            self.failures.append(f"{label} {detail}")

    def _emulate(self, va, blob, fixups, self_off, a0, a1, a2, a3):
        emu = self.emu
        emu.reset_data()
        emu.uc.reg_write(x86_const.UC_X86_REG_FPCW, 0x037F)
        emu.uc.reg_write(x86_const.UC_X86_REG_FPTAG, 0xFFFF)
        raw = bytearray(blob)
        base = emu.alloc(bytes(raw), 4096)
        self.pool = 0
        self.manager = 0
        for item in fixups:
            if item.startswith("p@"):
                offset = int(item[2:], 0)
                self.pool = 0 if offset == NULL or offset >= len(raw) else base + offset
            elif item.startswith("m@"):
                offset = int(item[2:], 0)
                self.manager = 0 if offset == NULL or offset >= len(raw) else base + offset
            elif item[0] in "vV" and "@" in item:
                kind_text, off_text = item[1:].split("@", 1)
                offset = int(off_text, 0)
                emu.write(base + offset, struct.pack("<I", self.vstubs[int(kind_text)]))
            else:
                offset = int(item, 0)
                rel = u32(raw, offset)
                emu.write(base + offset, struct.pack("<I", base + rel))
        cleanup, use_ecx, kinds = ABI[va]
        args = (a0, a1, a2, a3)
        stack = []
        for kind, value in zip(kinds, args):
            if kind == "i":
                stack.append(value & 0xFFFFFFFF)
            else:
                stack.append(self._pointer(base, len(raw), value))
        ecx = self._pointer(base, len(raw), self_off) if use_ecx else 0
        result = emu.call(va, stack, ecx=ecx)
        if result.stack_cleanup != cleanup:
            raise RuntimeError(f"cleanup {result.stack_cleanup} != {cleanup}")
        produced = self._relativize(emu.read(base, len(raw)), base)
        return self._ret(result.eax & 0xFFFFFFFF, base, len(raw)), produced


CTX = 0x100
OUT = 0x320
TOK = 0x340
ARR = 0x380


def boot() -> tuple[bytearray, list[str]]:
    blob = zeros(0xE00)
    put(blob, 0x00, 0x40)
    put(blob, 0x20, 0x700)
    put(blob, 0x80, 0xC0)
    put(blob, CTX + 0xBC, 0x500)
    put(blob, CTX + 0xD0, 0x520)
    put(blob, CTX + 0xF8, 0x540)
    put(blob, CTX + 0x12C, 0x560)
    fix = [
        "m@0", "p@0x80", "v16@0x48", "v17@0x4C", "v13@0xC4", "v14@0xC8", "v15@0xCC",
        "0x0", "0x20", "0x80", f"0x{CTX + 0xBC:X}", f"0x{CTX + 0xD0:X}", f"0x{CTX + 0xF8:X}",
        f"0x{CTX + 0x12C:X}",
    ]
    return blob, fix


def tokens(blob: bytearray, names: list[str], count: int | None = None) -> list[str]:
    put(blob, TOK, ARR)
    put(blob, TOK + 0x0C, len(names) if count is None else count)
    fix = [f"0x{TOK:X}"]
    for index, name in enumerate(names):
        at = 0x3C0 + index * 16
        cstr(blob, at, name)
        put(blob, ARR + index * 8 + 4, at)
        fix.append(f"0x{ARR + index * 8 + 4:X}")
    return fix


def shader_list(blob: bytearray, key: int) -> list[str]:
    put(blob, CTX + 8, 0x5C0)
    put(blob, CTX + 0x14, 1)
    put(blob, 0x5C0, 0x580)
    put(blob, 0x584, key)
    return [f"0x{CTX + 8:X}", "0x5C0"]


def texture_list(blob: bytearray, key: int) -> list[str]:
    put(blob, CTX + 0x1C, 0x5F0)
    put(blob, CTX + 0x28, 1)
    put(blob, 0x5F0, 0x5D0)
    put(blob, 0x5D8, key)
    return [f"0x{CTX + 0x1C:X}", "0x5F0"]


def build(oracle: Oracle) -> None:
    def parse(va: int, names: list[str], name: str, count: int | None = None, **fields: int) -> None:
        blob, fix = boot()
        fix += tokens(blob, names, count)
        for offset, value in fields.items():
            put(blob, int(offset, 16), value)
        oracle.check(va, blob, fix, NULL, a0=CTX, a1=OUT, a2=TOK, name=name)

    parse(0x009BCC40, ["setint"], "short", count=2)
    parse(0x009BCC40, ["setint", "block", "parm", "7"], "seven")
    parse(0x009BCC40, ["setint", "block", "parm", "no"], "text")

    parse(0x009BCD20, ["setbool"], "short", count=1)
    parse(0x009BCD20, ["setbool", "block", "parm", "true"], "true")
    parse(0x009BCD20, ["setbool", "block", "parm", "TRUE"], "folded")
    parse(0x009BCD20, ["setbool", "block", "parm", "yes"], "no")

    parse(0x009BCB10, ["setfloat"], "short", count=2)
    parse(0x009BCB10, ["setfloat", "block", "parm", "1.5"], "one")
    parse(0x009BCB10, ["setfloat", "block", "parm", "1", "2", "3", "4"], "four")

    blob, fix = boot()
    fix += tokens(blob, ["setdepth"], count=1)
    oracle.check(0x009B9D50, blob, fix, NULL, a0=CTX, a1=OUT, a2=TOK, name="short")
    blob, fix = boot()
    fix += tokens(blob, ["setdepth", "color"])
    put(blob, 0x30, 0)
    oracle.check(0x009B9D50, blob, fix, NULL, a0=CTX, a1=OUT, a2=TOK, name="miss")
    blob, fix = boot()
    fix += tokens(blob, ["setdepth", "color"])
    put(blob, 0x30, 1)
    put(blob, 0x38, 4)
    oracle.check(0x009B9D50, blob, fix, NULL, a0=CTX, a1=OUT, a2=TOK, name="hit")
    blob, fix = boot()
    fix += tokens(blob, ["setdepth", "color"])
    put(blob, 0x30, 1)
    put(blob, 0x38, 0xFFFFFFFF)
    oracle.check(0x009B9D50, blob, fix, NULL, a0=CTX, a1=OUT, a2=TOK, name="unknown")

    parse(0x009BBFA0, ["goto"], "short", count=1)
    parse(0x009BBFA0, ["goto", "loop"], "label")
    blob, fix = boot()
    fix += tokens(blob, ["goto", ""])
    put(blob, CTX + 0x124, 9)
    oracle.check(0x009BBFA0, blob, fix, NULL, a0=CTX, a1=OUT, a2=TOK, name="empty")

    parse(0x009BC070, ["beginevent"], "short", count=1)
    parse(0x009BC070, ["beginevent", "frame"], "named")

    blob, fix = boot()
    fix += tokens(blob, ["callproc", "draw"])
    oracle.check(0x009BC310, blob, fix, NULL, a0=CTX, a1=OUT, a2=TOK, name="named")
    blob, fix = boot()
    fix += tokens(blob, ["callproc", ""], count=1)
    oracle.check(0x009BC310, blob, fix, NULL, a0=CTX, a1=OUT, a2=TOK, name="blank")

    def shader_case(names: list[str], key: int, name: str, technique: int = 0, count: int | None = None) -> None:
        blob, fix = boot()
        fix += tokens(blob, names, count)
        fix += shader_list(blob, key)
        put(blob, 0x34, technique)
        put(blob, 0x598, 0x620)
        oracle.check(0x009BD1D0, blob, fix + ["0x598"], NULL, a0=CTX, a1=OUT, a2=TOK, name=name)

    shader_case(["setshader"], 0x42, "short", count=1)
    shader_case(["setshader", "main"], 0x11, "missing")
    shader_case(["setshader", "main"], 0x42, "name")
    shader_case(["setshader", "main", "2"], 0x42, "index")
    shader_case(["setshader", "main", "tech"], 0x42, "tech", technique=3)
    shader_case(["setshader", "main", "tech"], 0x42, "newtech", technique=0xFFFFFFFF)
    shader_case(["setshader", "main", "a", "b"], 0x42, "pair")

    def override(names: list[str], key: int, name: str, technique: int) -> None:
        blob, fix = boot()
        fix += tokens(blob, names)
        fix += shader_list(blob, key)
        put(blob, 0x34, technique)
        put(blob, 0x598, 0x620)
        oracle.check(0x009BD380, blob, fix + ["0x598"], NULL, a0=CTX, a1=OUT, a2=TOK, name=name)

    override(["over"], 0x42, "short", 0)
    override(["over", "main", "tech"], 0x11, "missing", 0)
    override(["over", "main", "tech"], 0x42, "known", 2)
    override(["over", "main", "tech"], 0x42, "created", 0xFFFFFFFF)

    def texture_case(va: int, names: list[str], key: int, name: str, surface: int = 0) -> None:
        blob, fix = boot()
        fix += tokens(blob, names)
        fix += texture_list(blob, key)
        put(blob, 0x38, surface)
        oracle.check(va, blob, fix, NULL, a0=CTX, a1=OUT, a2=TOK, name=name)

    texture_case(0x009BD460, ["settexture"], 0x42, "short")
    texture_case(0x009BD460, ["settexture", "block", "parm", "diffuse"], 0x11, "missing")
    texture_case(0x009BD460, ["settexture", "block", "parm", "diffuse"], 0x42, "hit")
    texture_case(0x009BD560, ["settosize"], 0x42, "short")
    texture_case(0x009BD560, ["settosize", "block", "parm", "diffuse"], 0x11, "neither", surface=0xFFFFFFFF)
    texture_case(0x009BD560, ["settosize", "block", "parm", "diffuse"], 0x42, "texture", surface=0xFFFFFFFF)
    texture_case(0x009BD560, ["settosize", "block", "parm", "diffuse"], 0x11, "surface", surface=6)

    def frame(blob: bytearray) -> list[str]:
        put(blob, CTX + 0x118, 0x680)
        put(blob, CTX + 0x124, 4)
        put(blob, CTX + 0x138, 1)
        put(blob, 0x560, 0x690)
        put(blob, 0x690 + 0x0C, 1)
        return [f"0x{CTX + 0x118:X}", "0x560"]

    blob, fix = boot()
    fix += tokens(blob, ["elif"], count=2)
    fix += frame(blob)
    oracle.check(0x009BBE20, blob, fix, NULL, a0=CTX, a1=OUT, a2=TOK, name="short")
    for matched, label in ((0, "else"), (1, "taken")):
        blob, fix = boot()
        fix += tokens(blob, ["elif", "block", "parm"])
        fix += frame(blob)
        put(blob, 0x30, matched)
        oracle.check(0x009BBE20, blob, fix, NULL, a0=CTX, a1=OUT, a2=TOK, name=label)

    for va, label in ((0x009BB930, "ifnot"), (0x009BBB30, "if")):
        blob, fix = boot()
        fix += tokens(blob, [label], count=1)
        oracle.check(va, blob, fix, NULL, a0=CTX, a1=OUT, a2=TOK, name="short")
        blob, fix = boot()
        fix += tokens(blob, [label, "block", "parm"])
        put(blob, CTX + 0x124, 2)
        oracle.check(va, blob, fix, NULL, a0=CTX, a1=OUT, a2=TOK, name="open")

    blob, fix = boot()
    fix += tokens(blob, ["endif", "extra"], count=2)
    oracle.check(0x009BBD30, blob, fix, NULL, a0=CTX, a1=OUT, a2=TOK, name="extra")
    blob, fix = boot()
    fix += tokens(blob, ["endif"])
    fix += frame(blob)
    oracle.check(0x009BBD30, blob, fix, NULL, a0=CTX, a1=OUT, a2=TOK, name="close")

    blob, fix = boot()
    put(blob, 0x200, 0x220)
    put(blob, 0x204, 0x1111)
    fix += ["v28@0x228", "0x200"]
    fix += tokens(blob, ["setcamera"], count=1)
    oracle.check(0x009B9C40, blob, fix, 0x200, a0=CTX, a1=OUT, a2=TOK, name="usage")
    blob, fix = boot()
    put(blob, 0x3C, 0xFFFFFFFF)
    fix += tokens(blob, ["setcamera", "main"])
    oracle.check(0x009B9C40, blob, fix, 0, a0=CTX, a1=OUT, a2=TOK, name="missing")
    blob, fix = boot()
    put(blob, 0x3C, 2)
    fix += tokens(blob, ["setcamera", "main", "1", "2", "3"])
    oracle.check(0x009B9C40, blob, fix, 0, a0=CTX, a1=OUT, a2=TOK, name="values")

    def quad(va: int, name: str, shader: int, gate: int) -> None:
        blob = zeros(0x500)
        put(blob, 0x80, 4)
        put(blob, 0x84, 2)
        put(blob, 0x88, gate)
        put(blob, 0x2F0, 0x11)
        put(blob, 0x400, 0)
        put(blob, 0x404, shader)
        put(blob, 0x408, 2)
        oracle.check(va, blob, ["0x400"], NULL, a0=0x430, a1=0x440, a2=0x400, a3=0x460, name=name)

    quad(0x009BC420, "off", 0, 1)
    quad(0x009BC420, "gated", 1, 0)
    quad(0x009BC420, "draw", 1, 1)
    quad(0x009BC780, "off", 0, 1)
    quad(0x009BC780, "gated", 1, 0)
    quad(0x009BC780, "draw", 1, 1)

    def extent(va: int, name: str, right: int, bottom: int) -> None:
        blob, fix = boot()
        put(blob, OUT + 8, 1)
        put(blob, OUT + 0x0C, 2)
        put(blob, OUT + 0x10, right)
        put(blob, OUT + 0x14, bottom)
        put(blob, 0x900, 0)
        oracle.check(va, blob, fix + ["0x900"], NULL, a0=CTX, a1=OUT, a2=0x900, name=name)

    extent(0x009B6BD0, "both", 8, 6)
    extent(0x009B6BD0, "none", 0xFFFFFFFF, 0xFFFFFFFF)
    extent(0x009B6CA0, "both", 3, 4)
    extent(0x009B6CA0, "none", 0xFFFFFFFF, 0xFFFFFFFF)

    blob, fix = boot()
    put(blob, CTX, 0x40)
    put(blob, CTX + 0x6C, 0x5C0)
    put(blob, 0x5C0, 0x580)
    put(blob, 0x58C, 9)
    put(blob, OUT + 8, 0)
    put(blob, OUT + 0x0C, 3)
    put(blob, 0x900, 0)
    put(blob, 0x980, 4)
    put(blob, 0x984, 2)
    fix += ["v19@0x68", f"0x{CTX:X}", f"0x{CTX + 0x6C:X}", "0x5C0", "0x900"]
    oracle.check(0x009B6D70, blob, fix, NULL, a0=CTX, a1=OUT, a2=0x900, name="apply")
    blob, fix = boot()
    put(blob, CTX, 0x40)
    put(blob, CTX + 0x6C, 0x5C0)
    put(blob, 0x5C0, 0x580)
    put(blob, 0x58C, 1)
    put(blob, OUT + 8, 0)
    put(blob, OUT + 0x0C, 0)
    put(blob, 0x900, 0)
    put(blob, 0x980, 8)
    put(blob, 0x984, 8)
    fix += ["v19@0x68", f"0x{CTX:X}", f"0x{CTX + 0x6C:X}", "0x5C0", "0x900"]
    oracle.check(0x009B6D70, blob, fix, NULL, a0=CTX, a1=OUT, a2=0x900, name="square")

    blob, fix = boot()
    put(blob, 0x500, 0x580)
    put(blob, 0x58C, 0)
    put(blob, OUT + 8, 0)
    oracle.check(0x009BACE0, blob, fix + ["0x500"], NULL, a0=CTX, a1=OUT, name="empty")
    blob, fix = boot()
    put(blob, 0x500, 0x580)
    put(blob, 0x58C, 0x600)
    put(blob, 0x650, 0x640)
    put(blob, 0x634, 0x660)
    put(blob, 0x640, 0x10)
    put(blob, 0x644, 0x20)
    put(blob, OUT + 8, 0)
    put(blob, OUT + 0x0C, 0)
    put(blob, CTX + 0x1C, 0x5F0)
    put(blob, 0x5F0, 0x5D0)
    put(blob, 0x5D0, 0x5C8)
    put(blob, 0x5D8, 0x6B0)
    put(blob, 0x6D4, 4)
    put(blob, 0x6D8, 2)
    fix += [
        "v29@0x5CC", "0x500", "0x58C", "0x650", "0x634",
        f"0x{CTX + 0x1C:X}", "0x5F0", "0x5D0", "0x5D8",
    ]
    oracle.check(0x009BACE0, blob, fix, NULL, a0=CTX, a1=OUT, name="texture")

    def condition(va: int, name: str, byte: int, target: int) -> None:
        blob, fix = boot()
        put(blob, 0x500, 0x580)
        put(blob, 0x58C, 0x600)
        put(blob, 0x64C, 0x6A0)
        put(blob, 0x6A0, 1 if byte else 0)
        put(blob, OUT + 0x10, 0)
        put(blob, OUT + 8, target)
        put(blob, 0x460, 0)
        oracle.check(va, blob, fix + ["0x500", "0x58C", "0x64C"], NULL, a0=CTX, a1=OUT, a3=0x460, name=name)

    condition(0x009BBA80, "take", 1, 5)
    condition(0x009BBA80, "skip", 0, 5)
    condition(0x009BBA80, "negative", 1, 0xFFFFFFFF)
    condition(0x009BBC80, "take", 0, 6)
    condition(0x009BBC80, "skip", 1, 6)

    blob, fix = boot()
    put(blob, CTX + 0x138, 1)
    put(blob, 0x560, 0x690)
    put(blob, 0x690, addr_pool())
    put(blob, 0x694, 0x42)
    oracle.check(0x009BBDB0, blob, fix + ["0x560", "0x690"], NULL, a0=CTX, name="pop")
    blob, fix = boot()
    put(blob, CTX + 0x138, 1)
    put(blob, 0x560, 0)
    oracle.check(0x009BBDB0, blob, fix, NULL, a0=CTX, name="null")

    def flush(va: int, name: str, count: int) -> None:
        blob = zeros(0x300)
        put(blob, 0x10, count)
        put(blob, 0x04, 0x80)
        put(blob, 0x08, 0xA0)
        put(blob, 0x1C, 0x40)
        put(blob, 0x4C, 4)
        put(blob, 0x24, 0xC0)
        put(blob, 0xCC, 0x100)
        put(blob, 0x100, 0x140)
        put(blob, 0x80, 0x11)
        put(blob, 0x9C, 0x22)
        oracle.check(va, blob, ["v26@0x28C", "0x4", "0x8", "0x1C", "0x24", "0xCC", "0x100"], 0, name=name)

    for va in (0x009B58C0, 0x009B5960, 0x009B59D0, 0x009B5A40, 0x009B5AF0, 0x009B5B60):
        flush(va, "zero", 0)
        flush(va, "five", 5)
        flush(va, "seven", 7)
        flush(va, "eleven", 11)

    blob = zeros(0x220)
    put(blob, 0x60, 0xE0)
    put(blob, 0x68, 7)
    put(blob, 0x6C, 7)
    put(blob, 0xE0, 0x100)
    oracle.check(0x009B3910, blob, ["v24@0x204", "0x60", "0xE0"], 0, a0=1, a1=2, name="same")
    blob = zeros(0x280)
    put(blob, 0x74, 0x80)
    put(blob, 0x80, 0x90)
    put(blob, 0xB0, 0x0F)
    put(blob, 0xB4, 0x02)
    put(blob, 0x60, 0xE0)
    put(blob, 0x68, 3)
    put(blob, 0x6C, 0xFFFFFFFF)
    put(blob, 0x24, 0x148)
    put(blob, 0x158, 1)
    put(blob, 0x15C, 0)
    put(blob, 0x168, 9)
    put(blob, 0xE0, 0x100)
    oracle.check(
        0x009B3910, blob,
        ["v20@0xA0", "v21@0x160", "v22@0x208", "v23@0x200", "0x74", "0x80", "0x60", "0x24", "0xE0"],
        0, a0=1, a1=2, name="walk")

    def kind(k: int, sub: int, sub2: int, name: str) -> None:
        blob = zeros(0x20)
        put(blob, 8, k)
        put(blob, 0x0C, sub)
        put(blob, 0x14, sub2)
        oracle.check(0x009B5380, blob, [], NULL, a0=0, name=name)

    kind(0, 1, 0, "six")
    kind(0, 9, 0, "other")
    kind(1, 3, 4, "nine")
    kind(2, 3, 0, "eleven")
    kind(4, 5, 0, "twelve")
    kind(7, 0, 0, "default")

    blob = zeros(0x1C0)
    put(blob, 0, 0x40)
    cstr(blob, 0x80, "d3d")
    put(blob, 0x60, 0x80)
    put(blob, 0x64, 1)
    put(blob, 0x68, 0x90)
    put(blob, 0x6C, 1)
    cstr(blob, 0x90, "block")
    oracle.check(0x009B5430, blob, ["m@0", "v18@0x54", "0x0", "0x60", "0x68"], 0, name="d3d")
    blob = zeros(0x1C0)
    put(blob, 0, 0x40)
    cstr(blob, 0x80, "gl")
    put(blob, 0x60, 0x80)
    put(blob, 0x64, 1)
    put(blob, 0x6C, 0)
    oracle.check(0x009B5430, blob, ["m@0", "v18@0x54", "0x0", "0x60"], 0, name="gl")
    blob = zeros(0x1C0)
    put(blob, 0, 0x40)
    cstr(blob, 0x80, "other")
    put(blob, 0x60, 0x80)
    put(blob, 0x64, 0)
    put(blob, 0x6C, 1)
    oracle.check(0x009B5430, blob, ["m@0", "v18@0x54", "0x0", "0x60"], 0, name="absent")

    def select(kind_value: int, flag: int, marker: int, name: str) -> None:
        blob = zeros(0x40)
        put(blob, 0x18, flag)
        store = blob
        store[0x190:0x191] = bytes([marker]) if False else b""
        if marker:
            grown = zeros(0x1A0)
            grown[:0x40] = blob
            grown[0x190] = marker
            blob = grown
        oracle.check(0x009BB040, blob, [], 0, a0=kind_value, a2=0, name=name)

    select(3, 2, 0, "set")
    select(3, 0, 0, "clear")
    select(5, 2, 0, "none")
    select(12, 0, 0, "only")
    select(11, 2, 1, "order")
    select(20, 0, 0, "range")

    blob = zeros(0xB0)
    put(blob, 0x14, 4)
    put(blob, 0x28, 0)
    put(blob, 0x2C, 0x40)
    put(blob, 0x40, 0x80)
    put(blob, 0x64, 0)
    put(blob, 0x9C, 4)
    oracle.check(0x009B9630, blob, ["0x2C", "0x40"], 0, a0=0x90, a1=0, name="fallback")
    blob = zeros(0xB0)
    put(blob, 0x14, 4)
    put(blob, 0x28, 2)
    put(blob, 0x2C, 0x40)
    put(blob, 0x40, 0x80)
    put(blob, 0x50, 2)
    put(blob, 0x54, 3)
    put(blob, 0x64, 1)
    put(blob, 0x70, 0x60)
    put(blob, 0x9C, 4)
    oracle.check(0x009B9630, blob, ["v27@0x8C", "0x2C", "0x40", "0x70"], 0, a0=0x90, a1=0, name="bound")


def addr_pool() -> int:
    return 0x80


def main() -> None:
    oracle = Oracle()
    try:
        build(oracle)
    finally:
        oracle.close()
    missing = [va for va in COUNTED if oracle.counts.get(va, 0) < 2]
    print(f"render9b2 oracle: {len(COUNTED)} entries, {sum(oracle.counts.values())} cases, {len(oracle.failures)} mismatches")
    for failure in oracle.failures:
        print("FAIL", failure)
    if missing:
        print("UNDERTESTED", " ".join(f"0x{va:08X}" for va in missing))
    if oracle.failures or missing:
        sys.exit(1)


if __name__ == "__main__":
    main()
