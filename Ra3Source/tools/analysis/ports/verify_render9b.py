"""Differentially verify the render-script command cluster against ra3_1.0.game."""

from __future__ import annotations

import struct
import subprocess
import sys
from pathlib import Path

from unicorn import x86_const

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from emulate import Emulator  # noqa: E402

ROOT = Path(__file__).resolve().parents[4]
EXE = ROOT / "Ra3Source" / "build" / "win32" / "bin" / "Release" / "ra3_render9bcheck.exe"
NULL = 0xFFFFFFFF

COUNTED = (
    0x009B7050, 0x009B70A0, 0x009B9CC0, 0x009B9CF0, 0x009B9FC0, 0x009B8150, 0x009B8230,
    0x009B31E0, 0x009B31F0, 0x009B6F70, 0x009B7150, 0x009B31B0, 0x009B3100, 0x009B3130,
    0x009B3180, 0x009B3160, 0x009B30E0, 0x009B6FE0, 0x009B6EA0, 0x009B8110, 0x009B81A0,
    0x009B8070, 0x009B81D0, 0x009B82A0, 0x009B75B0, 0x009B7620, 0x009B71C0, 0x009B73C0,
    0x009B7410, 0x009B7460, 0x009BC200, 0x009BC3D0, 0x009B7E50, 0x009B7EF0, 0x009B7740,
    0x009B7980, 0x009B2E30, 0x009B6B80, 0x009B58A0, 0x009B5510, 0x009B54D0, 0x009B5550,
    0x009B87A0, 0x009B5330, 0x009B8800, 0x009B5860, 0x009B5880, 0x009B8740,
)

# cleanup, ecx used, argument kinds for a0,a1,a2,a3. p=pointer i=immediate.
ABI = {
    0x009B7E50: (0x10, True, ("p", "i", "p", "i")),
    0x009B7EF0: (0x10, True, ("p", "i", "p", "i")),
    0x009B7740: (4, True, ("p", "i", "i", "i")),
    0x009B7980: (4, True, ("p", "i", "i", "i")),
    0x009B7050: (0x0C, False, ("p", "p", "p", "i")),
    0x009B70A0: (0x0C, False, ("p", "p", "p", "i")),
    0x009B9CC0: (0x0C, False, ("p", "p", "p", "i")),
    0x009B9CF0: (0x0C, False, ("p", "p", "p", "i")),
    0x009B9FC0: (0x0C, False, ("p", "p", "p", "i")),
    0x009B8150: (0x0C, False, ("p", "p", "p", "i")),
    0x009B8230: (0x0C, False, ("p", "p", "p", "i")),
    0x009B31E0: (0x0C, False, ("p", "p", "p", "i")),
    0x009B31F0: (0x0C, False, ("p", "p", "p", "i")),
    0x009B6F70: (0x0C, False, ("p", "p", "p", "i")),
    0x009B7150: (0x10, False, ("p", "p", "p", "p")),
    0x009B31B0: (0x10, False, ("p", "p", "p", "p")),
    0x009B3100: (0x10, False, ("p", "p", "p", "p")),
    0x009B3130: (0x10, False, ("p", "p", "p", "p")),
    0x009B3180: (0x10, False, ("p", "p", "p", "p")),
    0x009B3160: (0x10, False, ("p", "p", "p", "p")),
    0x009B30E0: (0x10, False, ("p", "p", "p", "p")),
    0x009B6FE0: (0x10, False, ("p", "p", "p", "p")),
    0x009B6EA0: (0x10, False, ("p", "p", "p", "p")),
    0x009B8110: (0x10, False, ("p", "p", "p", "p")),
    0x009B81A0: (0x10, False, ("p", "p", "p", "p")),
    0x009B8070: (0x10, False, ("p", "p", "p", "p")),
    0x009B81D0: (0x10, False, ("p", "p", "p", "p")),
    0x009B82A0: (0x10, False, ("p", "p", "p", "p")),
    0x009B75B0: (0x10, False, ("p", "p", "p", "p")),
    0x009B7620: (0x10, False, ("p", "p", "p", "p")),
    0x009B71C0: (0x10, False, ("p", "p", "p", "p")),
    0x009B73C0: (0x10, False, ("p", "p", "p", "p")),
    0x009B7410: (0x10, False, ("p", "p", "p", "p")),
    0x009B7460: (0x10, False, ("p", "p", "p", "p")),
    0x009BC200: (0x10, False, ("p", "p", "p", "p")),
    0x009BC3D0: (0x10, False, ("p", "p", "p", "p")),
    0x009B2E30: (0, True, ("i", "i", "i", "i")),
    0x009B6B80: (8, True, ("p", "p", "i", "i")),
    0x009B58A0: (0, True, ("i", "i", "i", "i")),
    0x009B5510: (8, True, ("i", "i", "i", "i")),
    0x009B54D0: (8, True, ("i", "i", "i", "i")),
    0x009B5550: (8, True, ("i", "i", "i", "i")),
    0x009B87A0: (8, True, ("p", "p", "i", "i")),
    0x009B5330: (0, True, ("i", "i", "i", "i")),
    0x009B8800: (4, True, ("i", "i", "i", "i")),
    0x009B5860: (0, True, ("i", "i", "i", "i")),
    0x009B5880: (0, True, ("i", "i", "i", "i")),
    0x009B8740: (8, True, ("i", "i", "i", "i")),
}


def u32(blob: bytes, offset: int) -> int:
    return struct.unpack_from("<I", blob, offset)[0]


def put(blob: bytearray, offset: int, value: int) -> None:
    struct.pack_into("<I", blob, offset, value & 0xFFFFFFFF)


def zeros(size: int) -> bytearray:
    return bytearray(size)


def cstr(blob: bytearray, offset: int, text: str) -> None:
    raw = text.encode("ascii") + b"\0"
    blob[offset:offset + len(raw)] = raw


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
        self._bind()

    def _bind(self) -> None:
        emu = self.emu

        def begin(e: Emulator) -> int:
            self_ptr = e.register(x86_const.UC_X86_REG_ECX)
            e.write(self_ptr + 0x90, struct.pack("<II", e.stack_arg(0), e.stack_arg(1)))
            return 0

        def end(e: Emulator) -> int:
            e.write(e.register(x86_const.UC_X86_REG_ECX) + 0x98, struct.pack("<I", e.stack_arg(0)))
            return 0

        def marker(e: Emulator) -> int:
            self_ptr = e.register(x86_const.UC_X86_REG_ECX)
            e.write(self_ptr + 0x9C, struct.pack("<II", e.stack_arg(0), e.stack_arg(1)))
            return 0

        def query(e: Emulator) -> int:
            return u32(e.read(e.register(x86_const.UC_X86_REG_ECX) + 0x10, 4), 0)

        def cast(e: Emulator) -> int:
            e.write(e.register(x86_const.UC_X86_REG_ECX) + 0x64, struct.pack("<I", e.stack_arg(0)))
            return 0

        def state_first(e: Emulator) -> int:
            device = e.stack_arg(0)
            e.write(device + 0x2C, struct.pack("<I", e.stack_arg(2)))
            return u32(e.read(device + 0x10, 4), 0)

        def state_second(e: Emulator) -> int:
            device = e.stack_arg(0)
            e.write(device + 0x18, struct.pack("<I", e.stack_arg(1)))
            e.write(device + 0x1C, struct.pack("<I", e.stack_arg(2)))
            return u32(e.read(device + 0x14, 4), 0)

        def size_first(e: Emulator) -> int:
            device = e.stack_arg(0)
            e.write(device + 0x2C, struct.pack("<I", e.stack_arg(1)))
            return u32(e.read(device + 0x28, 4), 0)

        def size_second(e: Emulator) -> int:
            device = e.stack_arg(0)
            buffer = e.stack_arg(2)
            e.write(buffer, e.read(device + 0x20, 8))
            return 0

        def release(e: Emulator) -> int:
            e.write(e.stack_arg(0) + 0x20, struct.pack("<I", 0x11))
            return 0x11

        def reset(e: Emulator) -> int:
            e.write(e.register(x86_const.UC_X86_REG_ECX) + 0x28, struct.pack("<I", 1))
            return 0

        def var_float(e: Emulator) -> int:
            name = e.stack_arg(0)
            if e.read(name, 3) == b"ok\0":
                e.write(e.stack_arg(1), struct.pack("<I", 0x3F800000))
                return 1
            return 0

        def var_int(e: Emulator) -> int:
            name = e.stack_arg(0)
            if e.read(name, 3) == b"ok\0":
                e.write(e.stack_arg(1), struct.pack("<I", 7))
                return 1
            return 0

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

        def sscanf(e: Emulator) -> int:
            text = e.read(e.stack_arg(0), 32).split(b"\0", 1)[0]
            fmt = e.read(e.stack_arg(1), 4)
            dest = e.stack_arg(2)
            floating = b"f" in fmt
            value = _scan_number(text, floating)
            if value is None:
                return 0
            e.write(dest, struct.pack("<I", value))
            return 1

        def pool(e: Emulator) -> int:
            del e
            return self.pool

        def log(e: Emulator) -> int:
            del e
            return 0

        def clear_surface(e: Emulator) -> int:
            device = e.register(x86_const.UC_X86_REG_ECX)
            color = e.stack_arg(1)
            bits = 0 if color == 0 else u32(e.read(color, 4), 0)
            e.write(device + 0xB0, struct.pack("<IIII", e.stack_arg(0), bits, e.stack_arg(2), e.stack_arg(3)))
            return 0

        def set_target(e: Emulator) -> int:
            device = e.register(x86_const.UC_X86_REG_ECX)
            e.write(device + 0xC0, struct.pack("<II", e.stack_arg(0), e.stack_arg(1)))
            return 0

        def bind_shader(e: Emulator) -> int:
            e.write(e.register(x86_const.UC_X86_REG_ECX) + 8, struct.pack("<I", e.stack_arg(0)))
            return 0

        def zero(e: Emulator) -> int:
            del e
            return 0

        def lookup(e: Emulator) -> int:
            text = e.stack_arg(0)
            if text == 0:
                return 0
            return 0x21 if e.read(text, 1) == b"D" else 0

        def equals(e: Emulator) -> int:
            entry = e.register(x86_const.UC_X86_REG_ECX)
            left = e.stack_arg(0)
            right = e.stack_arg(1)
            e.write(entry + 0x10, struct.pack("<II", left, right))
            return 1 if left == (right ^ 1) else 0

        def ensure(e: Emulator) -> int:
            e.write(e.register(x86_const.UC_X86_REG_ECX) + 0x40, struct.pack("<I", e.stack_arg(0)))
            return 0

        def bind_constants(e: Emulator) -> int:
            e.write(e.stack_arg(0) + 0x70, struct.pack("<I", 0xAB))
            return 0

        def refresh(e: Emulator) -> int:
            e.write(e.stack_arg(0) + 0x6C, struct.pack("<I", 0x11))
            return 0

        def flush(e: Emulator) -> int:
            e.write(e.stack_arg(0) + 0x70, struct.pack("<I", 0xCD))
            return 0

        self.vstubs = {
            0: emu.bind_stub(begin, 8),
            1: emu.bind_stub(end, 4),
            2: emu.bind_stub(marker, 8),
            3: emu.bind_stub(query, 0),
            4: emu.bind_stub(cast, 4),
            5: emu.bind_stub(state_first, 12),
            6: emu.bind_stub(state_second, 12),
            7: emu.bind_stub(size_first, 8),
            8: emu.bind_stub(size_second, 12),
            9: emu.bind_stub(release, 4),
            10: emu.bind_stub(reset, 0),
            11: emu.bind_stub(var_float, 8),
            12: emu.bind_stub(var_int, 8),
            13: emu.bind_stub(intern, 4),
            14: emu.bind_stub(release_key, 4),
            15: emu.bind_stub(compare, 8),
        }
        emu.bind_import(0x00BBA4E0, sscanf, 0)
        emu.bind_function(0x00997400, pool, 0)
        emu.bind_function(0x00997380, log, 0)
        emu.bind_function(0x009C3E40, clear_surface, 16)
        emu.bind_function(0x009C5440, set_target, 8)
        emu.bind_function(0x009C1660, bind_shader, 4)
        emu.bind_function(0x009B9330, zero, 0)
        emu.bind_function(0x009B93D0, zero, 8)
        emu.bind_function(0x009B33C0, lookup, 4)
        emu.bind_function(0x009C13E0, equals, 8)
        emu.bind_function(0x009B52C0, ensure, 4)
        emu.bind_function(0x009C4A70, bind_constants, 4)
        emu.bind_function(0x009C4B20, refresh, 4)
        emu.bind_function(0x009C4AE0, flush, 4)

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
                if len(cpp_blob) != len(emu_blob):
                    detail += " length"
            self.failures.append(f"{label} {detail}")

    def _emulate(self, va, blob, fixups, self_off, a0, a1, a2, a3):
        emu = self.emu
        emu.reset_data()
        emu.uc.reg_write(x86_const.UC_X86_REG_FPCW, 0x037F)
        emu.uc.reg_write(x86_const.UC_X86_REG_FPTAG, 0xFFFF)
        raw = bytearray(blob)
        base = emu.alloc(bytes(raw))
        self.pool = 0
        for item in fixups:
            if item.startswith("p@"):
                offset = int(item[2:], 0)
                self.pool = 0 if offset == NULL or offset >= len(raw) else base + offset
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
        return self._ret(result.eax, base, len(raw)), produced


def build(oracle: Oracle) -> None:
    # 0x009B7050 token count. Shared by clear / clearcolor / clearz.
    for count, name in ((0, "zero"), (1, "one"), (4, "four")):
        blob = zeros(0x20)
        put(blob, 0x0C, count)
        oracle.check(0x009B7050, blob, [], NULL, a2=0x00, name=name)

    # 0x009B70A0 cleartarget index.
    def target_tokens(text: str, count: int) -> bytearray:
        blob = zeros(0x40)
        put(blob, 0x00, 0x10)
        put(blob, 0x0C, count)
        put(blob, 0x1C, 0x28)
        cstr(blob, 0x28, text)
        return blob

    oracle.check(0x009B70A0, target_tokens("0", 1), ["0x0", "0x1C"], NULL, a1=0x30, a2=0x00, name="short")
    oracle.check(0x009B70A0, target_tokens("2", 2), ["0x0", "0x1C"], NULL, a1=0x30, a2=0x00, name="two")
    oracle.check(0x009B70A0, target_tokens("4", 2), ["0x0", "0x1C"], NULL, a1=0x30, a2=0x00, name="range")
    oracle.check(0x009B70A0, target_tokens("no", 2), ["0x0", "0x1C"], NULL, a1=0x30, a2=0x00, name="text")

    # 0x009B7E50 / 0x009B7EF0 numeric conversion, including a $-variable and a short list.
    def token_blob(words: list[str], dest: int) -> tuple[bytearray, list[str]]:
        blob = zeros(0x200)
        put(blob, 0x00, 0x160)
        put(blob, 0x60, 0x90)
        put(blob, 0x6C, len(words))
        fix = ["0x0", "0x60", "v11@0x1E0", "v12@0x1DC"]
        cursor = 0x100
        for index, word in enumerate(words):
            put(blob, 0x90 + index * 8 + 4, cursor)
            fix.append(f"0x{0x90 + index * 8 + 4:X}")
            cstr(blob, cursor, word)
            cursor += len(word) + 1
        return blob, fix

    for start, count, words, name in (
        (1, 0, ["clear", "1"], "none"),
        (1, 2, ["clear", "1", "0.5"], "two"),
        (1, 2, ["clear", "1"], "short"),
        (1, 1, ["clear", "zz"], "bad"),
        (1, 1, ["clear", "$ok"], "var"),
        (1, 1, ["clear", "$no"], "miss"),
    ):
        blob, fix = token_blob(words, 0xE0)
        oracle.check(0x009B7E50, blob, fix, 0x00, a0=0x60, a1=start, a2=0xE0, a3=count, name=name)
        oracle.check(0x009B7EF0, blob, fix, 0x00, a0=0x60, a1=start, a2=0xE0, a3=count, name=name)

    # 0x009B9CC0 / 0x009B9CF0 / 0x009B9FC0 parse wrappers.
    def wrapped(words: list[str]) -> tuple[bytearray, list[str]]:
        blob, fix = token_blob(words, 0xE0)
        return blob, fix

    for words, name in ((["setclearcolor", "1", "0", "0.5", "1"], "rgba"), (["setclearcolor", "1"], "short"), (["setclearcolor", "1", "zz", "0", "1"], "bad")):
        blob, fix = wrapped(words)
        oracle.check(0x009B9CC0, blob, fix, NULL, a0=0x00, a1=0xD8, a2=0x60, name=name)
    for words, name in ((["setcleardepth", "0.5"], "depth"), (["setcleardepth", "0.5", "3"], "stencil"), (["setcleardepth"], "short")):
        blob, fix = wrapped(words)
        oracle.check(0x009B9CF0, blob, fix, NULL, a0=0x00, a1=0xD8, a2=0x60, name=name)
    for words, name in ((["renderquad"], "default"), (["renderquad", "1", "0.5", "0", "1"], "color"), (["renderquad", "zz", "0", "0", "1"], "bad")):
        blob, fix = wrapped(words)
        oracle.check(0x009B9FC0, blob, fix, NULL, a0=0x00, a1=0xD8, a2=0x60, name=name)

    # 0x009B7740 / 0x009B7980 name lookup, then the parses that call them.
    def named(count_off: int, array_off: int, key_off: int, key: int, count: int) -> tuple[bytearray, list[str]]:
        blob = zeros(0x100)
        put(blob, array_off, 0x40)
        put(blob, count_off, count)
        put(blob, 0x40, 0x60)
        put(blob, 0x60 + key_off, key)
        cstr(blob, 0x80, "shader")
        put(blob, 0x90, 0xB0)
        fix = [f"0x{array_off:X}", "0x40", "0x90", "v13@0xB4", "v14@0xB8", "p@0x90"]
        return blob, fix

    blob, fix = named(0x14, 8, 4, 0, 0)
    oracle.check(0x009B7740, blob, fix, 0x00, a0=0x80, name="empty")
    blob, fix = named(0x14, 8, 4, 0x42, 1)
    oracle.check(0x009B7740, blob, fix, 0x00, a0=0x80, name="hit")
    blob, fix = named(0x14, 8, 4, 0x99, 2)
    put(blob, 0x44, 0x70)
    put(blob, 0x74, 0x42)
    fix.append("0x44")
    oracle.check(0x009B7740, blob, fix, 0x00, a0=0x80, name="second")
    blob, fix = named(0x28, 0x1C, 8, 0x42, 1)
    oracle.check(0x009B7980, blob, fix, 0x00, a0=0x80, name="hit")
    blob, fix = named(0x28, 0x1C, 8, 7, 1)
    oracle.check(0x009B7980, blob, fix, 0x00, a0=NULL, name="miss")

    def override_parse(words: list[str], key: int) -> tuple[bytearray, list[str]]:
        blob = zeros(0x120)
        put(blob, 0x14, 1)
        put(blob, 8, 0x40)
        put(blob, 0x40, 0x60)
        put(blob, 0x64, key)
        put(blob, 0x90, 0xB0)
        put(blob, 0xC0, 0xD0)
        put(blob, 0xCC, len(words))
        fix = ["0x8", "0x40", "0x90", "v13@0xB4", "v14@0xB8", "p@0x90", "0xC0"]
        cursor = 0xE0
        for index, word in enumerate(words):
            put(blob, 0xD0 + index * 8 + 4, cursor)
            fix.append(f"0x{0xD0 + index * 8 + 4:X}")
            cstr(blob, cursor, word)
            cursor += 8
        return blob, fix

    blob, fix = override_parse(["clearshaderoverride"], 0x42)
    oracle.check(0x009B8150, blob, fix, NULL, a0=0x00, a1=0x30, a2=0xC0, name="short")
    blob, fix = override_parse(["clearshaderoverride", "shader"], 0x42)
    oracle.check(0x009B8150, blob, fix, NULL, a0=0x00, a1=0x30, a2=0xC0, name="hit")
    blob, fix = override_parse(["clearshaderoverride", "shader"], 0x7)
    oracle.check(0x009B8150, blob, fix, NULL, a0=0x00, a1=0x30, a2=0xC0, name="miss")

    def cast_parse(words: list[str], key: int) -> tuple[bytearray, list[str]]:
        blob, fix = override_parse(words, key)
        # Texture records keep the key at +8 and the list at +0x1C / count +0x28.
        put(blob, 0x14, 0)
        put(blob, 0x28, 1)
        put(blob, 0x1C, 0x40)
        put(blob, 0x68, key)
        fix.extend(["0x1C"])
        return blob, fix

    blob, fix = cast_parse(["casttexture", "shader", "DXT"], 0x42)
    oracle.check(0x009B8230, blob, fix, NULL, a0=0x00, a1=0x30, a2=0xC0, name="hit")
    blob, fix = cast_parse(["casttexture", "shader", "ARGB"], 0x42)
    oracle.check(0x009B8230, blob, fix, NULL, a0=0x00, a1=0x30, a2=0xC0, name="format")
    blob, fix = cast_parse(["casttexture"], 0x42)
    oracle.check(0x009B8230, blob, fix, NULL, a0=0x00, a1=0x30, a2=0xC0, name="short")

    # Bookkeeping parses and executes that only touch the script record.
    blob = zeros(0x160)
    oracle.check(0x009B31E0, blob, [], NULL, a0=0x00, name="clear")
    blob = zeros(0x20)
    put(blob, 8, 0x11)
    oracle.check(0x009B31F0, blob, [], NULL, a1=0x00, name="mark")
    blob = zeros(0x20)
    put(blob, 0x18, 0x22)
    oracle.check(0x009B31F0, blob, [], NULL, a1=0x10, name="other")

    def frame_blob(active: int, field: int, value: int, tokens: int) -> tuple[bytearray, list[str]]:
        blob = zeros(0x160)
        put(blob, 0x118, 0x40)
        put(blob, 0x124, value)
        put(blob, 0x12C, 0x80)
        put(blob, 0x138, 1)
        put(blob, 0x80, 0xA0)
        put(blob, 0xAC, field)
        put(blob, 0xC0, 0xD0)
        put(blob, 0xCC, tokens)
        return blob, ["0x118", "0x12C", "0x80", "0xC0"]

    blob, fix = frame_blob(1, 0, 0x20, 1)
    oracle.check(0x009B6F70, blob, fix, NULL, a0=0x00, a1=0x100, a2=0xC0, name="ok")
    blob, fix = frame_blob(1, 0, 0x20, 2)
    oracle.check(0x009B6F70, blob, fix, NULL, a0=0x00, a1=0x100, a2=0xC0, name="extra")

    blob = zeros(0x40)
    put(blob, 8, 0x3F800000)
    put(blob, 0x0C, 0)
    put(blob, 0x10, 0x3E800000)
    put(blob, 0x14, 0x3F000000)
    oracle.check(0x009B7150, blob, [], NULL, a1=0x00, a2=0x20, name="copy")
    blob = zeros(0x30)
    put(blob, 4, 7)
    put(blob, 8, 0x3F000000)
    oracle.check(0x009B31B0, blob, [], NULL, a1=0x00, a2=0x18, name="copy")

    def state_blob() -> bytearray:
        blob = zeros(0x180)
        put(blob, 0x20, 0x40)
        put(blob, 0x30, 0x11111111)
        put(blob, 0x40, 0x3F800000)
        put(blob, 0x44, 2)
        return blob

    for va, name in ((0x009B3100, "all"), (0x009B3130, "color"), (0x009B3180, "z")):
        blob = state_blob()
        oracle.check(va, blob, ["0x20"], NULL, a2=0x20, name=name)
    blob = zeros(0x40)
    put(blob, 0x08, 2)
    put(blob, 0x20, 0x28)
    oracle.check(0x009B3160, blob, ["0x20"], NULL, a1=0x00, a2=0x20, name="index")

    for target, name in ((0, "zero"), (5, "five"), (0xFFFFFFFF, "neg")):
        blob = zeros(0x20)
        put(blob, 8, target)
        oracle.check(0x009B30E0, blob, [], NULL, a1=0x00, a3=0x10, name=name)

    def else_exec(active: int, target: int) -> tuple[bytearray, list[str]]:
        blob = zeros(0x160)
        put(blob, 0x12C, 0x40)
        put(blob, 0x138, 1)
        put(blob, 0x40, 0x60)
        put(blob, 0x6C, active)
        put(blob, 0x80, target)
        return blob, ["0x12C", "0x40"]

    blob, fix = else_exec(1, 6)
    oracle.check(0x009B6FE0, blob, fix, NULL, a0=0x00, a1=0x80, a3=0x90, name="taken")
    blob, fix = else_exec(0, 6)
    oracle.check(0x009B6FE0, blob, fix, NULL, a0=0x00, a1=0x80, a3=0x90, name="skip")

    def elif_exec(active: int, flag: int, byte: int, target: int) -> tuple[bytearray, list[str]]:
        blob = zeros(0x180)
        put(blob, 0xBC, 0x40)
        put(blob, 0x12C, 0x60)
        put(blob, 0x138, 1)
        put(blob, 0x40, 0x80)
        put(blob, 0x60, 0xA0)
        put(blob, 0xAC, active)
        put(blob, 0x8C, 0xC0)
        put(blob, 0xC0 + 0x4C, 0xD0)
        blob[0xD0] = byte
        put(blob, 0x148, target)
        put(blob, 0x14C, flag)
        put(blob, 0x150, 0)
        return blob, ["0xBC", "0x12C", "0x40", "0x60", "0x8C", "0x10C"]

    # param is at record+0xC. record is at 0x80, so param pointer lives at 0x8C.
    for active, flag, byte, target, name in (
        (0, 0, 0, 4, "inactive"),
        (1, 0, 0, 4, "true"),
        (1, 0, 1, 4, "false"),
        (1, 3, 1, 4, "not"),
        (1, 0, 0, 0xFFFFFFFF, "neg"),
    ):
        blob, fix = elif_exec(active, flag, byte, target)
        oracle.check(0x009B6EA0, blob, fix, NULL, a0=0x00, a1=0x140, a3=0x160, name=name)

    for tech, name in ((-1, "neg"), (0, "zero"), (3, "high")):
        blob = zeros(0x80)
        put(blob, 8, 0x20)
        put(blob, 0x20, 0x30)
        put(blob, 0x3C, 0x40 if name != "high" else 0)
        put(blob, 0x48, 0x50)
        put(blob, 0x54, 1)
        put(blob, 0x50, 0x58)
        put(blob, 0x64, 0x77)
        put(blob, 0x18, 0)
        put(blob, 0x1C, tech & 0xFFFFFFFF)
        fix = ["0x8", "0x20", "0x48", "0x50"]
        if name != "high":
            fix.append("0x3C")
        oracle.check(0x009B8110, blob, fix, NULL, a0=0x00, a1=0x10, name=name)
    blob = zeros(0x50)
    put(blob, 8, 0x18)
    put(blob, 0x18, 0x20)
    put(blob, 0x2C, 0)
    put(blob, 0x38, 0)
    oracle.check(0x009B81A0, blob, ["0x8", "0x18"], NULL, a0=0x00, a1=0x30, name="null")
    blob = zeros(0x50)
    put(blob, 8, 0x18)
    put(blob, 0x18, 0x20)
    put(blob, 0x2C, 0x30)
    put(blob, 0x38, 0)
    oracle.check(0x009B81A0, blob, ["0x8", "0x18", "0x2C"], NULL, a0=0x00, a1=0x30, name="reset")

    for mode, name in ((0, "raw"), (1, "index"), (2, "current"), (9, "other")):
        blob = zeros(0x180)
        put(blob, 8, 0x40)
        put(blob, 0x40, 0x60)
        put(blob, 0x6C, 0x90)
        put(blob, 0x78, 0xA0)
        put(blob, 0x84, 2)
        put(blob, 0xA0, 0xB0)
        put(blob, 0xBC, 0x55)
        put(blob, 0xC8, 0)
        put(blob, 0xCC, mode)
        put(blob, 0xD0, 0)
        put(blob, 0xE0, 0xF0)
        fix = ["0x8", "0x40", "0x78", "0xA0", "0xE0"]
        if mode == 2:
            put(blob, 0xBC, 0x100)
            put(blob, 0x100, 0x110)
            put(blob, 0x11C, 0x130)
            put(blob, 0x17C, 0x140)
            put(blob, 0x140, 0x61)
            fix.extend(["0xBC", "0x100", "0x11C", "0x17C"])
        oracle.check(0x009B8070, blob, fix, NULL, a0=0x00, a1=0xC0, a2=0xE0, name=name)

    def texture_exec(match: bool) -> tuple[bytearray, list[str]]:
        blob = zeros(0x180)
        put(blob, 0x1C, 0x40)
        put(blob, 0xBC, 0x50)
        put(blob, 0x40, 0x70)
        put(blob, 0x50, 0x90)
        put(blob, 0x70, 0xB0)
        put(blob, 0x80, 0x21 if match else 0x22)
        put(blob, 0x9C, 0xC0)
        put(blob, 0xC0 + 0x4C, 0xD0 if match else 0)
        if match:
            put(blob, 0xD0, 0x21)
        put(blob, 0xC0 + 0x50, 0xE0)
        put(blob, 0xC0 + 0x34, 0xF0)
        put(blob, 0xC0 + 0x40, 0x3)
        put(blob, 0xC0 + 0x44, 0x5)
        put(blob, 0x08, 0)
        put(blob, 0x0C, 0)
        fix = ["0x1C", "0xBC", "0x40", "0x50", "0x70", "0x9C", "0x110", "0xF4", "v3@0xB4"]
        if match:
            fix.append("0x10C")
        return blob, fix

    oracle.check(0x009B81D0, texture_exec(True)[0], texture_exec(True)[1], NULL, a0=0x00, a1=0x00, name="same")
    oracle.check(0x009B81D0, texture_exec(False)[0], texture_exec(False)[1], NULL, a0=0x00, a1=0x00, name="dirty")
    blob = zeros(0xC0)
    put(blob, 0x1C, 0x20)
    put(blob, 0x20, 0x30)
    put(blob, 0x30, 0x70)
    put(blob, 0x40, 0x50)
    put(blob, 0x50, 0x90)
    put(blob, 0x0C, 0x15)
    oracle.check(0x009B82A0, blob, ["0x1C", "0x20", "0x30", "0x40", "0x50", "v3@0x74", "v4@0xAC"], NULL, a0=0x00, a1=0x00, name="cast")

    def param_exec(kind: str, same: bool) -> tuple[bytearray, list[str]]:
        blob = zeros(0x160)
        put(blob, 0xBC, 0x20)
        put(blob, 0x20, 0x40)
        put(blob, 0x4C, 0x60)
        put(blob, 0x60 + 0x4C, 0x80 if same else 0)
        put(blob, 0x60 + 0x50, 0x90)
        put(blob, 0x60 + 0x34, 0xA0)
        put(blob, 0x60 + 0x40, 0x7)
        put(blob, 0x60 + 0x44, 0x9)
        if kind == "int":
            put(blob, 0x80, 4 if same else 1)
            put(blob, 0x08, 4)
        else:
            blob[0x80] = 1 if same else 0
            blob[0x08] = 1
        put(blob, 0x04, 0)
        fix = ["0xBC", "0x20", "0x4C", "0xB0", "0x94"]
        if same:
            fix.append("0xAC")
        return blob, fix

    oracle.check(0x009B75B0, param_exec("int", True)[0], param_exec("int", True)[1], NULL, a0=0x00, a1=0x00, name="same")
    oracle.check(0x009B75B0, param_exec("int", False)[0], param_exec("int", False)[1], NULL, a0=0x00, a1=0x00, name="dirty")
    oracle.check(0x009B7620, param_exec("bool", True)[0], param_exec("bool", True)[1], NULL, a0=0x00, a1=0x00, name="same")
    oracle.check(0x009B7620, param_exec("bool", False)[0], param_exec("bool", False)[1], NULL, a0=0x00, a1=0x00, name="dirty")

    blob = zeros(0x120)
    put(blob, 0x30, 0x40)
    put(blob, 0x40, 0x50)
    put(blob, 0x5C, 0x70)
    put(blob, 0x0C, 0)
    put(blob, 0x80, 0x90)
    oracle.check(0x009B71C0, blob, ["0x30", "0x40", "0x5C", "0x80"], NULL, a0=0x00, a1=0x00, a2=0x80, name="set")
    blob = zeros(0x40)
    put(blob, 0x0C, 0xFFFFFFFF)
    put(blob, 0x20, 0x28)
    oracle.check(0x009B71C0, blob, ["0x20"], NULL, a0=0x00, a1=0x00, a2=0x20, name="none")

    def event_blob() -> tuple[bytearray, list[str]]:
        blob = zeros(0x80)
        put(blob, 0x00, 0x30)
        put(blob, 0xD0 if False else 0x40, 0)  # placeholder replaced below
        return blob, []

    blob = zeros(0x120)
    put(blob, 0x00, 0x40)
    put(blob, 0xD0, 0x60)
    put(blob, 0x60, 0x70)
    put(blob, 0x74, 0x90)
    cstr(blob, 0x90, "pix")
    put(blob, 0x08, 3)
    put(blob, 0x0C, 0)
    fix = ["0x0", "0xD0", "0x60", "0x74", "v0@0xB0", "v1@0xB4", "v2@0xB8"]
    oracle.check(0x009B73C0, blob, fix, NULL, a0=0x00, a1=0x00, name="begin")
    oracle.check(0x009B7410, blob, fix, NULL, a0=0x00, a1=0x00, name="end")
    oracle.check(0x009B7460, blob, fix, NULL, a0=0x00, a1=0x00, name="marker")

    blob = zeros(0x160)
    put(blob, 0x124, 9)
    put(blob, 0x150, 0)
    oracle.check(0x009BC200, blob, [], NULL, a0=0x00, a3=0x140, name="root")
    blob = zeros(0x180)
    put(blob, 0x144, 0x40)
    put(blob, 0x150, 1)
    put(blob, 0x40, 4)
    oracle.check(0x009BC200, blob, ["0x144"], NULL, a0=0x00, a3=0x160, name="pop")
    blob = zeros(0x180)
    put(blob, 0x08, 0xFFFFFFFF)
    oracle.check(0x009BC3D0, blob, [], NULL, a0=0x00, a1=0x00, a3=0x20, name="neg")
    blob = zeros(0x180)
    put(blob, 0x08, 6)
    put(blob, 0x144, 0x40)
    put(blob, 0x150, 0)
    put(blob, 0x20, 3)
    oracle.check(0x009BC3D0, blob, ["0x144"], NULL, a0=0x00, a1=0x00, a3=0x20, name="push")

    blob = zeros(0x30)
    put(blob, 0x1C, 0x11)
    put(blob, 0x20, 0x22)
    oracle.check(0x009B2E30, blob, [], 0x00, name="clear")
    blob = zeros(0x40)
    put(blob, 0x14, 3)
    put(blob, 0x20, 0x0C)
    oracle.check(0x009B6B80, blob, ["0x20"], 0x00, a0=0x20, a1=NULL, name="default")
    blob = zeros(0x40)
    put(blob, 0x14, 2)
    put(blob, 0x20, 0x0C)
    put(blob, 0x28, 0x30)
    oracle.check(0x009B6B80, blob, ["0x20", "0x28"], 0x00, a0=0x20, a1=0x28, name="callback")

    blob = zeros(0x50)
    put(blob, 0x00, 0x30)
    put(blob, 4, 9)
    oracle.check(0x009B58A0, blob, ["0x0", "v10@0x3C"], 0x00, name="reset")

    def named_state(second: str, found: int, result: int) -> tuple[bytearray, list[str]]:
        blob = zeros(0x120)
        put(blob, 0x38, 0x20)
        put(blob, 0x20, 0x40)
        put(blob, 0x30, found)
        put(blob, 0x34, result)
        return blob, ["0x38", "0x20", "v5@0x64", second]

    for va, slot, name in ((0x009B5510, "v6@0xAC", "state"), (0x009B54D0, "v6@0xBC", "sampler"), (0x009B5550, "v6@0x10C", "stage")):
        blob, fix = named_state(slot, 0, 0)
        oracle.check(va, blob, fix, 0x00, a0=0x1234, a1=0x56, name="miss")
        blob, fix = named_state(slot, 0x55, 0)
        oracle.check(va, blob, fix, 0x00, a0=0x1234, a1=0x56, name="hit")
        blob, fix = named_state(slot, 0x55, 0xFFFFFFFF)
        oracle.check(va, blob, fix, 0x00, a0=0x1234, a1=0x56, name="neg")

    blob = zeros(0x180)
    put(blob, 0x38, 0x20)
    put(blob, 0x20, 0x80)
    put(blob, 0x160, 0)
    cstr(blob, 0xC0, "A")
    oracle.check(0x009B87A0, blob, ["0x38", "0x20", "v7@0xB0"], 0x00, a0=0xC0, a1=0xD0, name="miss")
    blob = zeros(0x180)
    put(blob, 0x38, 0x20)
    put(blob, 0x20, 0x80)
    put(blob, 0x48, 0x77)
    put(blob, 0x40, 0x11)
    put(blob, 0x44, 0x22)
    put(blob, 0x160, 0)
    blob[0xC0] = 0x5A
    oracle.check(0x009B87A0, blob, ["0x38", "0x20", "v7@0xB0", "v8@0x94"], 0x00, a0=0xC0, a1=0xD0, name="hit")

    blob = zeros(0x100)
    oracle.check(0x009B5330, blob, [], 0x00, name="null")
    blob = zeros(0x100)
    put(blob, 0x38, 1)
    put(blob, 0x10, 0x20)
    oracle.check(0x009B5330, blob, ["0x10"], 0x00, name="flush")

    blob = zeros(0x180)
    put(blob, 0x3C, 0)
    put(blob, 0x174, 0x20)
    put(blob, 0x20, 0x40)
    put(blob, 0x40, 0x99)
    put(blob, 0xB0, 0)
    oracle.check(0x009B8800, blob, ["0x174", "0x20"], 0x00, a0=0, name="same")
    blob = zeros(0x180)
    put(blob, 0x3C, 0)
    put(blob, 0x174, 0x20)
    put(blob, 0x20, 0x40)
    put(blob, 0x40, 0x99)
    put(blob, 0xB0, 1)
    put(blob, 0x58, 0x30)
    oracle.check(0x009B8800, blob, ["0x174", "0x20", "0x58"], 0x00, a0=0, name="refresh")

    blob = zeros(0x40)
    oracle.check(0x009B5860, blob, [], 0x00, name="null")
    oracle.check(0x009B5880, blob, [], 0x00, name="null")
    blob = zeros(0x180)
    put(blob, 8, 0x20)
    put(blob, 0x20, 0x40)
    oracle.check(0x009B5860, blob, ["0x8", "0x20", "v9@0x154"], 0x00, name="drop")
    oracle.check(0x009B5880, blob, ["0x8", "0x20", "v9@0x158"], 0x00, name="drop")

    blob = zeros(0x180)
    oracle.check(0x009B8740, blob, [], 0x00, a0=1, a1=0, name="null")
    blob = zeros(0x180)
    put(blob, 0x38, 1)
    put(blob, 0x3C, 0)
    put(blob, 0x10, 0x20)
    put(blob, 0x174, 0x30)
    put(blob, 0x30, 0x50)
    put(blob, 0x50, 0x66)
    put(blob, 0xB0, 0)
    oracle.check(0x009B8740, blob, ["0x10", "0x174", "0x30"], 0x00, a0=0, a1=0, name="same")
    blob = zeros(0x180)
    put(blob, 0x38, 1)
    put(blob, 0x3C, 2)
    put(blob, 0x10, 0x20)
    put(blob, 0x174, 0x100)
    put(blob, 0x128, 0x140)
    put(blob, 0x140, 0x66)
    put(blob, 0xB0, 1)
    put(blob, 0x58, 0x28)
    oracle.check(0x009B8740, blob, ["0x10", "0x174", "0x128", "0x58"], 0x00, a0=1, a1=0, name="switch")


def main() -> None:
    only = int(sys.argv[1], 16) if len(sys.argv) > 1 else 0
    oracle = Oracle()
    try:
        build(oracle)
    finally:
        oracle.close()
    if only:
        oracle.failures = [line for line in oracle.failures if f"0x{only:08X}" in line]
    for line in oracle.failures[:40]:
        print("FAIL", line)
    missing = [va for va in COUNTED if oracle.counts.get(va, 0) == 0]
    total = sum(oracle.counts.values())
    print(
        f"render9b oracle: {len(oracle.counts)} entries, {total} cases, "
        f"{len(oracle.failures)} mismatches"
        + ("" if not missing else " UNTESTED " + ",".join(f"0x{va:08X}" for va in missing))
    )
    if oracle.failures or missing:
        raise SystemExit(1)


if __name__ == "__main__":
    main()
