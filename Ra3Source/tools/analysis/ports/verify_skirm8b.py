"""Differentially verify the skirm8b skirmish-AI cluster against ra3_1.0.game."""

from __future__ import annotations

import argparse
import struct
import subprocess
import sys
from pathlib import Path

from unicorn import x86_const

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from emulate import HEAP_BASE, STACK_BASE, STACK_SIZE, CallResult, Emulator  # noqa: E402

ACCEPTED = (
    0x008B85F0, 0x008B8600, 0x008B8610, 0x008B8FE0, 0x008B7860, 0x008BDC40, 0x008BFF40,
    0x008C0100, 0x008C1340, 0x008C1370, 0x008C3040, 0x008C48E0, 0x008C4B10, 0x008C4D40,
    0x008C5140, 0x008C58F0, 0x008C8DA0, 0x008C8DE0, 0x008C96F0, 0x008C9950, 0x008C9A70,
    0x008C9AA0, 0x008CCA40, 0x008CCAA0, 0x008CCB40, 0x008CCB90, 0x008CCC10, 0x008CCEB0,
    0x008CCF60, 0x008CD020, 0x008CD110, 0x008CD3F0, 0x008CD4B0, 0x008CD6E0, 0x008CD760,
    0x008CD7B0, 0x008CD840, 0x008CD880, 0x008CD8D0, 0x008CD950, 0x008CD9D0, 0x008CD9F0,
)

BLOB = 0x1300
LOG = 0xC00
HEAP = 0xA80
HEAP_END = 0xC00
XFER = 0x200
VTAB = 0x280
XFER_SLOTS = (0x18, 0x20, 0x6C, 0x70, 0x84, 0x88)
LOGIC_GLOBAL = 0x00DEBA64
CATALOG_GLOBAL = 0x00DF9D28
MAX_LOG = 24

CLEANUP = {
    0x00416820: 0, 0x008CC9B0: 12, 0x008CCBC0: 12, 0x008CCC40: 12, 0x008CCEE0: 12,
    0x008CCF90: 12, 0x008CD050: 12, 0x008CD440: 12, 0x008CD4E0: 12, 0x008CD710: 12,
    0x008CD7E0: 12, 0x008CD900: 12, 0x008CD140: 16, 0x008ADD50: 8, 0x00885BB0: 4,
    0x008B5370: 4, 0x008818E0: 4, 0x008C16A0: 8, 0x008A8B20: 0, 0x0089A640: 0,
    0x0089A740: 0, 0x008B6260: 4, 0x007E5660: 4, 0x008B4560: 24, 0x008908C0: 8,
    0x00890400: 0, 0x00625090: 4, 0x008CB3B0: 4, 0x0089A6D0: 0, 0x008C1950: 8,
    0x007E6850: 0, 0x0088C0E0: 8, 0x008B5050: 0, 0x008A88C0: 0, 0x008A8A90: 4,
    0x0078D880: 4, 0x008878C0: 8, 0x008BC5B0: 0, 0x004CBFF0: 4, 0x008B4900: 20,
    0x004CA930: 0, 0x008BC0C0: 0, 0x008BC310: 0, 0x008BBCE0: 0, 0x008A5020: 0,
    0x0082D820: 0, 0x0073A470: 0, 0x007A7BA0: 8, 0x0087DF00: 8, 0x008A51A0: 0,
    0x008AD720: 0, 0x00887BE0: 4, 0x008C4990: 4, 0x0087F630: 4, 0x008C5060: 4,
    0x008884E0: 4,
}


def u32(data: bytes | bytearray, offset: int = 0) -> int:
    return struct.unpack_from("<I", data, offset)[0]


def put_u32(data: bytearray, offset: int, value: int) -> None:
    struct.pack_into("<I", data, offset, value & 0xFFFFFFFF)


def put_f32(data: bytearray, offset: int, value: float) -> None:
    struct.pack_into("<f", data, offset, value)


def key_slot(key: int) -> int:
    return {
        0x00DFC6D8: 0,
        0x00DFC680: 4,
        0x00DFC6D4: 8,
        0x00DFC67C: 12,
    }.get(key, 0)


class Verifier:
    def __init__(self, executable: Path) -> None:
        self.emu = Emulator()
        self.cpp = subprocess.Popen(
            [str(executable), "--batch"],
            stdin=subprocess.PIPE,
            stdout=subprocess.PIPE,
            text=True,
            bufsize=1,
        )
        self.counts: dict[int, int] = {}
        self.base = 0
        self.blob_len = 0
        self.heap = HEAP
        self.fail_alloc = False
        self.vstubs: dict[int, int] = {}
        self._bind()

    def _bind(self) -> None:
        def version_stub(emu: Emulator) -> int:
            xfer = emu.register(x86_const.UC_X86_REG_ECX)
            pair = emu.stack_arg(0)
            if emu.read(xfer + 0x20, 1)[0] != 0:
                emu.write(pair + 1, emu.read(xfer + 0x21, 1))
            count = u32(emu.read(xfer + 0x24, 4))
            emu.write(xfer + 0x24, struct.pack("<I", (count + 1) & 0xFFFFFFFF))
            return 0

        def field_stub(salt: int):
            def stub(emu: Emulator) -> int:
                xfer = emu.register(x86_const.UC_X86_REG_ECX)
                field = emu.stack_arg(0)
                if field != 0:
                    cur = u32(emu.read(field, 4))
                    emu.write(field, struct.pack("<I", (cur ^ salt) & 0xFFFFFFFF))
                count = u32(emu.read(xfer + 0x28, 4))
                emu.write(xfer + 0x28, struct.pack("<I", (count + 1) & 0xFFFFFFFF))
                return 0
            return stub

        def named_stub(emu: Emulator) -> int:
            xfer = emu.register(x86_const.UC_X86_REG_ECX)
            name = emu.stack_arg(0)
            field = emu.stack_arg(1)
            count = emu.stack_arg(2)
            salt = (0x77770000 ^ (name & 0xFF) ^ count) & 0xFFFFFFFF
            if field != 0:
                cur = u32(emu.read(field, 4))
                emu.write(field, struct.pack("<I", (cur ^ salt) & 0xFFFFFFFF))
            seen = u32(emu.read(xfer + 0x28, 4))
            emu.write(xfer + 0x28, struct.pack("<I", (seen + 1) & 0xFFFFFFFF))
            return 0

        salts = {0x20: 0x11110001, 0x6C: 0x44440004, 0x70: 0x55550005, 0x84: 0x66660006}
        self.vstubs[0x18] = self.emu.bind_stub(version_stub, 4)
        for slot, salt in salts.items():
            self.vstubs[slot] = self.emu.bind_stub(field_stub(salt), 4)
        self.vstubs[0x88] = self.emu.bind_stub(named_stub, 12)

        for address, cleanup in CLEANUP.items():
            self.emu.bind_function(address, self._dispatch(address), cleanup)

    def enc(self, value: int) -> int:
        value &= 0xFFFFFFFF
        if value == 0:
            return 0
        if self.base <= value < self.base + self.blob_len:
            return (value - self.base) | 0x80000000
        if STACK_BASE <= value < STACK_BASE + STACK_SIZE:
            return 0x40000000
        return value

    def _log(self, fn: int, ecx: int, args: list[int], extra: list[int] | None = None) -> None:
        raw = bytearray(self.emu.read(self.base + LOG, 4))
        count = u32(raw, 0)
        if count >= MAX_LOG:
            return
        at = self.base + LOG + 4 + count * 64
        words = [fn & 0xFFFFFFFF, self.enc(ecx)]
        for index in range(6):
            words.append(self.enc(args[index]) if index < len(args) else 0)
        for index in range(8):
            words.append(extra[index] if extra and index < len(extra) else 0)
        self.emu.write(at, struct.pack("<16I", *words))
        self.emu.write(self.base + LOG, struct.pack("<I", count + 1))

    def _read(self, address: int) -> int:
        if address == 0:
            return 0
        return u32(self.emu.read(address, 4), 0)

    def _dispatch(self, fn: int):
        ctor3 = (0x008CC9B0, 0x008CCBC0, 0x008CCC40, 0x008CCEE0, 0x008CCF90, 0x008CD050,
                 0x008CD440, 0x008CD4E0, 0x008CD710, 0x008CD7E0, 0x008CD900)
        cdecl = (0x008BC0C0, 0x008BC310, 0x008BBCE0, 0x008A5020, 0x0082D820, 0x0073A470,
                 0x008A51A0, 0x008AD720, 0x008BC5B0)
        call2 = (0x008ADD50, 0x008C16A0, 0x008C1950, 0x0088C0E0, 0x008908C0, 0x008878C0,
                 0x007A7BA0, 0x0087DF00)
        call1 = (0x00885BB0, 0x008B5370, 0x008818E0, 0x008B6260, 0x007E5660, 0x00625090,
                 0x008CB3B0, 0x008A8A90, 0x0078D880, 0x008884E0, 0x004CBFF0, 0x00887BE0,
                 0x008C4990, 0x0087F630, 0x008C5060)

        def stub(emu: Emulator) -> int:
            ecx = emu.register(x86_const.UC_X86_REG_ECX) & 0xFFFFFFFF
            args = [emu.stack_arg(i) & 0xFFFFFFFF for i in range(6)]
            if fn == 0x00416820:
                result = self._alloc(args[0])
                extra = [self.enc(result)] + [0] * 7 if result else None
                self._log(fn, 0, args[:1], extra)
                return result
            if fn in ctor3 or fn in (0x008CD140, 0x008B4900):
                nargs = 5 if fn == 0x008B4900 else 4 if fn == 0x008CD140 else 3
                self._log(fn, ecx, args[:nargs], None)
                if ecx == 0:
                    return 0
                words = [fn] + [self.enc(args[index]) for index in range(nargs)]
                emu.write(ecx, struct.pack(f"<{nargs + 1}I", *words))
                return ecx
            if fn in cdecl:
                extra = [0] * 8
                if fn == 0x008BC5B0 and args[1] != 0:
                    extra[0] = self.enc(self._read(args[1]))
                elif self.base <= args[1] < self.base + self.blob_len:
                    salt = (0x51000000 ^ (fn & 0xFFFF)) & 0xFFFFFFFF
                    cur = self._read(args[1])
                    self.emu.write(args[1], struct.pack("<I", (cur ^ salt) & 0xFFFFFFFF))
                self._log(fn, 0, args[:2], extra if fn == 0x008BC5B0 else None)
                return 0
            if fn == 0x008ADD50:
                extra = [0] * 8
                if args[1] != 0:
                    extra[0] = self.enc(self._read(args[1]))
                self._log(fn, ecx, args[:2], extra)
                return 0
            if fn == 0x008B4560:
                extra = [0] * 8
                if args[4] != 0:
                    extra[0], extra[1], extra[2] = struct.unpack("<III", emu.read(args[4], 12))
                if args[3] != 0:
                    extra[3], extra[4], extra[5] = struct.unpack("<III", emu.read(args[3], 12))
                self._log(fn, ecx, args[:6], extra)
                return 0
            if fn == 0x007E5660 and args[0] != 0:
                emu.write(args[0], struct.pack("<III", 0x3F800000, 0x40000000, 0x40400000))
            nargs = 2 if fn in call2 else 1 if fn in call1 else 0
            self._log(fn, ecx, args[:nargs], None)
            return self._result(fn, ecx, args)
        return stub

    def _alloc(self, size: int) -> int:
        if self.fail_alloc:
            return 0
        bytes_ = (size + 3) & ~3
        if self.heap + bytes_ > HEAP_END:
            return 0
        result = self.base + self.heap
        self.heap += bytes_
        return result

    def _result(self, fn: int, ecx: int, args: list[int]) -> int:
        if fn == 0x00625090:
            return ecx
        if ecx == 0:
            return 0
        if fn in (0x0089A740, 0x0089A6D0, 0x004CBFF0):
            return ecx
        if fn == 0x007E6850:
            return self._read(ecx)
        if fn == 0x0089A640:
            return self.emu.read(ecx, 1)[0]
        if fn in (0x008A8B20, 0x008B5050):
            return self.emu.read(ecx + 1, 1)[0]
        if fn == 0x008A88C0:
            return self.emu.read(ecx + 2, 1)[0]
        if fn == 0x00890400:
            return self._read(ecx + 8)
        if fn == 0x00885BB0:
            return self._read(ecx)
        if fn == 0x008818E0:
            return self._read(ecx + 4 + (args[0] & 1) * 4)
        if fn == 0x008A8A90:
            return self.emu.read(ecx + 3, 1)[0]
        if fn == 0x008CB3B0:
            return self._read(ecx + key_slot(args[0]))
        if fn == 0x0078D880:
            return self._read(ecx)
        if fn == 0x008884E0:
            return (ecx + 0x20) & 0xFFFFFFFF
        if fn in (0x008878C0, 0x008908C0):
            return self._read(ecx)
        return 0

    def close(self) -> None:
        if self.cpp.stdin is not None:
            self.cpp.stdin.close()
        self.cpp.wait(timeout=10)

    def check(self, va: int, self_off: int, blob: bytes, fixups: list[str], stack: list[int],
              cleanup: int, *, a0: int = 0, a1: int = 0, a2: int = 0, frame: int = 0,
              catalog: int | None = None, compare: str = "none") -> None:
        assert self.cpp.stdin is not None and self.cpp.stdout is not None
        fix_str = ",".join(fixups) if fixups else "-"
        line = (
            f"0x{va:08X} 0x{self_off:X} {blob.hex()} {fix_str} "
            f"0x{a0 & 0xFFFFFFFF:X} 0x{a1 & 0xFFFFFFFF:X} 0x{a2 & 0xFFFFFFFF:X}\n"
        )
        self.cpp.stdin.write(line)
        self.cpp.stdin.flush()
        out = self.cpp.stdout.readline().strip()
        if not out.startswith("ret=0x"):
            raise RuntimeError(f"ra3_skirm8bcheck failed on 0x{va:08X}: {out}")
        parts = out.split()
        cpp_ret = int(parts[0].split("=")[1], 16)
        cpp_blob = bytes.fromhex(parts[1].split("=")[1])

        self.emu.reset_data()
        self.fail_alloc = a2 == 1
        self.heap = HEAP
        self._alloc_result = 0
        base = self.emu.alloc(blob, alignment=256)
        self.base = base
        self.blob_len = len(blob)
        logic = self.emu.alloc(b"\x00" * 0x60)
        self.emu.write(logic + 0x58, struct.pack("<I", frame & 0xFFFFFFFF))
        self.emu.write(LOGIC_GLOBAL, struct.pack("<I", logic))
        if catalog is None:
            self.emu.write(CATALOG_GLOBAL, b"\x00\x00\x00\x00")
        else:
            self.emu.write(CATALOG_GLOBAL, struct.pack("<I", base + catalog))
        reverse = {addr: kind for kind, addr in self.vstubs.items()}
        for item in fixups:
            if item.startswith("v") and "@" in item:
                kind_text, off_text = item[1:].split("@", 1)
                kind = int(kind_text, 16)
                off = int(off_text, 0)
                self.emu.write(base + off, struct.pack("<I", self.vstubs[kind]))
            else:
                off = int(item, 0)
                rel = u32(blob, off)
                self.emu.write(base + off, struct.pack("<I", (base + rel) & 0xFFFFFFFF))
        resolved = [(base + arg) if isinstance(arg, str) else arg for arg in stack]
        # stack entries that are blob offsets are ints already absolute or offsets.
        absolute = []
        for arg in stack:
            if isinstance(arg, tuple):
                absolute.append((base + arg[0]) & 0xFFFFFFFF)
            else:
                absolute.append(arg & 0xFFFFFFFF)
        ecx = 0 if self_off >= len(blob) else base + self_off
        result = self.emu.call(va, absolute, ecx=ecx)
        if result.stack_cleanup != cleanup:
            raise AssertionError(
                f"0x{va:08X}: stack cleanup {result.stack_cleanup}, expected {cleanup}")
        game_blob = bytearray(self.emu.read(base, len(blob)))
        for off in range(0, len(game_blob) - 3, 4):
            cur = u32(game_blob, off)
            if base <= cur < base + len(blob):
                put_u32(game_blob, off, cur - base)
            elif cur in reverse:
                put_u32(game_blob, off, 0xF0000000 | reverse[cur])
        if bytes(game_blob) != cpp_blob:
            for idx, (game_byte, cpp_byte) in enumerate(zip(game_blob, cpp_blob)):
                if game_byte != cpp_byte:
                    start = max(0, idx - 4)
                    end = min(len(game_blob), idx + 8)
                    raise AssertionError(
                        f"0x{va:08X}: blob mismatch at +0x{idx:X}: "
                        f"game=0x{game_byte:02X} cpp=0x{cpp_byte:02X} "
                        f"(game={game_blob[start:end].hex()} cpp={cpp_blob[start:end].hex()})")
            raise AssertionError(f"0x{va:08X}: blob length {len(game_blob)} vs {len(cpp_blob)}")
        if compare != "none":
            game_ret = result.eax & 0xFFFFFFFF
            if compare == "ptr":
                if game_ret == 0:
                    game_val = 0
                elif base <= game_ret < base + len(blob):
                    game_val = game_ret - base
                else:
                    game_val = game_ret
            else:
                game_val = game_ret
            if game_val != (cpp_ret & 0xFFFFFFFF):
                raise AssertionError(
                    f"0x{va:08X}: return mismatch ({compare}): "
                    f"game=0x{game_val:08X} cpp=0x{cpp_ret & 0xFFFFFFFF:08X}")
        self.counts[va] = self.counts.get(va, 0) + 1


def fresh(fill: int = 0xA5) -> bytearray:
    blob = bytearray([fill & 0xFF]) * BLOB
    blob[LOG:] = b"\x00" * (BLOB - LOG)
    return blob


def xfer_fixes() -> list[str]:
    fixes = ["0x200"]
    for slot in XFER_SLOTS:
        fixes.append(f"v{slot:X}@0x{VTAB + slot:X}")
    return fixes


def make_xfer(flags: int = 0, override: int | None = None, fill: int = 0xA5) -> bytearray:
    blob = fresh(fill)
    put_u32(blob, XFER, VTAB)
    put_u32(blob, XFER + 4, flags)
    blob[XFER + 0x20] = 0 if override is None else 1
    blob[XFER + 0x21] = 0 if override is None else override & 0xFF
    return blob


def verify(executable: Path) -> None:
    oracle = Verifier(executable)
    creates = (
        0x008CCB90, 0x008CCC10, 0x008CCEB0, 0x008CCF60, 0x008CD020, 0x008CD110,
        0x008CD4B0, 0x008CD6E0, 0x008CD7B0, 0x008CD8D0, 0x008CD950,
    )

    def go(va: int, blob: bytearray, fixups: list[str], stack: list[int], cleanup: int,
           **kwargs) -> None:
        oracle.check(va, kwargs.pop("self_off", 0), bytes(blob), fixups, stack, cleanup, **kwargs)

    for phase in (0, 1, 2, 0xFFFFFFFF):
        blob = fresh()
        put_u32(blob, 0x18, phase)
        go(0x008B85F0, blob, [], [], 0, compare="u32")
        go(0x008B8600, blob, [], [], 0, compare="u32")

    for values in ((1.0, -2.0, 0.5), (0.0, 0.0, 0.0), (-8.0, 0.125, 3.0)):
        blob = fresh()
        for index, value in enumerate(values):
            put_f32(blob, 0x40 + index * 4, value)
        go(0x008B8610, blob, [], [(0x80,)], 4, a0=0x80, compare="ptr")

    def push_cases() -> None:
        # Room in the 0x618 vector, clear the selector, null end pointer.
        blob = fresh()
        put_u32(blob, 0x2C, 0x111)
        put_u32(blob, 0x14, 7)
        put_u32(blob, 0x100, 0x200)          # arg object -> player
        put_u32(blob, 0x290, 0)              # player+0x90 not used; +0x90 of arg is 0x190
        put_u32(blob, 0x190, 0x200)
        put_u32(blob, 0x200 + 0x618 + 4, 0)  # end null
        put_u32(blob, 0x200 + 0x618 + 8, 0xA20)
        put_u32(blob, 0x200 + 0x640 + 4, 0xA00)
        put_u32(blob, 0x200 + 0x640 + 8, 0xA00)
        fixes = ["0x100", "0x190", f"0x{0x200 + 0x618 + 8:X}"]
        go(0x008B8FE0, blob, fixes, [(0x100,), 0], 8, a0=0x100, a1=0)
        # Keep selector 1, room in the 0x640 vector, write through a real end pointer.
        blob = fresh()
        put_u32(blob, 0x2C, 0x222)
        put_u32(blob, 0x14, 1)
        put_u32(blob, 0x100, 0x200)
        put_u32(blob, 0x190, 0x200)
        put_u32(blob, 0x200 + 0x640 + 4, 0xA00)
        put_u32(blob, 0x200 + 0x640 + 8, 0xA20)
        put_u32(blob, 0x200 + 0x618 + 4, 0xA00)
        put_u32(blob, 0x200 + 0x618 + 8, 0xA00)
        fixes = ["0x100", "0x190", f"0x{0x200 + 0x640 + 4:X}", f"0x{0x200 + 0x640 + 8:X}",
                 f"0x{0x200 + 0x618 + 4:X}", f"0x{0x200 + 0x618 + 8:X}"]
        go(0x008B8FE0, blob, fixes, [(0x100,), 1], 8, a0=0x100, a1=1)
        # Equal end/cap takes the insert helper.
        blob = fresh()
        put_u32(blob, 0x2C, 0x333)
        put_u32(blob, 0x14, 0)
        put_u32(blob, 0x100, 0x200)
        put_u32(blob, 0x190, 0x200)
        put_u32(blob, 0x200 + 0x618 + 4, 0xA00)
        put_u32(blob, 0x200 + 0x618 + 8, 0xA00)
        fixes = ["0x100", "0x190", f"0x{0x200 + 0x618 + 4:X}", f"0x{0x200 + 0x618 + 8:X}"]
        go(0x008B8FE0, blob, fixes, [(0x100,), 5], 8, a0=0x100, a1=5)

    push_cases()

    for ctor in creates:
        for ai, player, fail in ((0x222, 0x111, 0), (0, 0xFFFFFFFF, 0), (0x5, 0x6, 1)):
            blob = fresh()
            put_u32(blob, 0x50, ai)
            put_u32(blob, 0x54, player)
            go(ctor, blob, [], [], 0, a2=fail, compare="ptr")

    def plant_table(blob: bytearray, ai: int, table: int, player: int, add: int, word: int) -> list[str]:
        inner = ai + 0x40
        put_u32(blob, ai + 0x10, inner)
        put_u32(blob, inner + 0x3C, table)
        put_u32(blob, table + player * 68 + add, word)
        return [f"0x{ai + 0x10:X}", f"0x{inner + 0x3C:X}"]

    for word, fail in ((2, 0), (0, 0), (2, 1)):
        blob = fresh()
        put_u32(blob, 0x50, 0x480)
        put_u32(blob, 0x54, 0)
        fixes = plant_table(blob, 0x480, 0x500, 0, 4, word)
        fixes.append("0x50")
        go(0x008CD3F0, blob, fixes, [], 0, a2=fail, compare="ptr")

    for ai, player, fail in ((0x10, 0x20, 0), (0, 1, 0), (4, 5, 1)):
        blob = fresh()
        put_u32(blob, 0x50, ai)
        put_u32(blob, 0x54, player)
        go(0x008CD9F0, blob, [], [], 0, a2=fail, compare="ptr")

    def owner_fixes(blob: bytearray, owner: int = 0x200) -> list[str]:
        put_u32(blob, 0x2C, owner)
        put_u32(blob, owner + 0x90, owner)
        return ["0x2C", f"0x{owner + 0x90:X}"]

    # Simple defense contact: subject present, subject absent, host byte overwritten.
    for subject, prior in ((0x300, 0), (0, 0x7F), (0x300, 0xA5)):
        blob = fresh(0x11)
        blob[LOG:] = b"\x00" * (BLOB - LOG)
        put_u32(blob, 0x28, subject)
        put_u32(blob, 0x2C, 0x200)
        blob[0x200 + 0x10A] = prior & 0xFF
        if subject:
            put_u32(blob, subject, 0x3F800000)
            put_u32(blob, subject + 4, 0x40000000)
            put_u32(blob, subject + 8, 0x40400000)
        fixes = ["0x2C"] + (["0x28"] if subject else [])
        go(0x008C1340, blob, fixes, [], 0)

    for found in (0, 0x99, 0xABCDEF01):
        blob = fresh()
        put_u32(blob, 0x58, found)
        go(0x008C1370, blob, [], [], 0)

    for live, subject in ((0, 0), (0, 0x10), (1, 0), (0x200, 0x10)):
        blob = fresh()
        put_u32(blob, 4, live)
        put_u32(blob, 0x28, subject)
        fixes = ["0x4"] if live >= 0x100 else []
        go(0x008C9A70, blob, fixes, [], 0)

    # try simple defense: empty, byte clear, direct ready, held, rejected.
    for byte14, ready, held in ((0, 1, 1), (1, 1, 0), (1, 0, 1), (1, 0, 0)):
        blob = fresh()
        put_u32(blob, 0x28, 0x300)
        blob[0x14] = byte14
        blob[1] = ready
        blob[0x300] = held
        go(0x008C8DA0, blob, ["0x28"], [], 0)
    blob = fresh()
    put_u32(blob, 0x28, 0)
    blob[0x14] = 1
    go(0x008C8DA0, blob, [], [], 0)

    for live, subject in ((0, 0x300), (0x10, 0), (0x300, 0x340)):
        blob = fresh()
        put_u32(blob, 4, live)
        put_u32(blob, 0x28, subject)
        if subject:
            put_f32(blob, subject, 1.5)
            put_f32(blob, subject + 4, -0.25)
            put_f32(blob, subject + 8, 0.0)
        fixes = []
        if live >= 0x100:
            fixes.append("0x4")
        if subject:
            fixes.append("0x28")
        go(0x008C8DE0, blob, fixes, [], 0)

    def flank(live: int, subject: int, word: int) -> None:
        blob = fresh()
        put_u32(blob, 4, live)
        put_u32(blob, 0x28, subject)
        put_u32(blob, 0x2C, 0x180)
        put_u32(blob, 0x50, 0x480)
        put_u32(blob, 0x54, 1)
        put_u32(blob, 0xA0, 0x42)
        fixes = ["0x2C", "0x50"] + plant_table(blob, 0x480, 0x500, 1, 4, word)
        if live >= 0x100:
            fixes.append("0x4")
            put_u32(blob, live, 0x11)
        if subject:
            fixes.append("0x28")
            put_f32(blob, subject, 4.0)
            put_f32(blob, subject + 4, 5.0)
            put_f32(blob, subject + 8, 6.0)
        go(0x008C96F0, blob, fixes, [], 0)

    flank(0, 0x300, 2)
    flank(0x160, 0, 2)
    flank(0x160, 0x300, 2)
    flank(0x160, 0x300, 0)

    def reactive_update(live: int, found_ptr: int, float_bits: int) -> None:
        blob = fresh()
        put_u32(blob, 4, live)
        put_u32(blob, 0x2C, 0x200)
        put_u32(blob, 0x200 + 0x90, 0x200)
        put_u32(blob, 0x50, 0x480)
        put_u32(blob, 0x54, 0)
        fixes = ["0x2C", "0x290", "0x50"] + plant_table(blob, 0x480, 0x500, 0, 0x30, 0x77)
        if live >= 0x100:
            fixes.append("0x4")
        put_u32(blob, 0x200 + 0x454, found_ptr)
        if found_ptr:
            fixes.append(f"0x{0x200 + 0x454:X}")
            put_u32(blob, found_ptr + 4, 0x64)
            put_u32(blob, found_ptr + 8, 0x99)
        put_u32(blob, 0x200 + 0x7DC, 0x120)
        put_u32(blob, 0x120, 0x140)
        put_u32(blob, 0x140 + 0x60, float_bits)
        fixes += [f"0x{0x200 + 0x7DC:X}", "0x120"]
        go(0x008CCAA0, blob, fixes, [], 0)

    reactive_update(0, 0x160, 0x3F800000)
    reactive_update(0x300, 0, 0xBF800000)
    reactive_update(0x300, 0x160, 0x00000000)

    for frame in (0, 5, 0xFFFFFFFF):
        blob = fresh()
        fixes = owner_fixes(blob)
        go(0x008CCB40, blob, fixes, [], 0, a0=frame, frame=frame)
        blob = fresh()
        blob[0xA0] = 1
        fixes = owner_fixes(blob)
        go(0x008CD880, blob, fixes, [], 0, a0=frame, frame=frame)
    blob = fresh()
    blob[0xA0] = 0
    fixes = owner_fixes(blob)
    go(0x008CD880, blob, fixes, [], 0, a0=9, frame=9)

    for first, second, frame in ((1, 0, 0), (0, 5, 4), (0, 4, 4), (0, 0xFFFFFFFF, 0)):
        blob = fresh()
        fixes = owner_fixes(blob)
        put_u32(blob, 0x200 + 8, first)    # key D4
        put_u32(blob, 0x200 + 12, second)  # key 7C
        go(0x008CCA40, blob, fixes, [0], 4, a0=frame, frame=frame, compare="ptr")
        blob = fresh()
        fixes = owner_fixes(blob)
        put_u32(blob, 0x200 + 0, first)    # key D8
        put_u32(blob, 0x200 + 4, second)   # key 80
        go(0x008CD840, blob, fixes, [0], 4, a0=frame, frame=frame, compare="u32")

    # continue simple attack: null live pointer, live object, subject present.
    for live, subject in ((0, 0), (0x180, 0), (0x180, 0x300)):
        blob = fresh()
        put_u32(blob, 4, live)
        put_u32(blob, 0x28, subject)
        if live:
            put_u32(blob, live, 0x55)
        if subject:
            put_u32(blob, subject, 0x21)
        fixes = []
        if live:
            fixes.append("0x4")
        if subject:
            fixes.append("0x28")
        go(0x008C9950, blob, fixes, [], 0, compare="u32")

    def mode_case(mode: int, pred1: int, pred2: int) -> None:
        blob = fresh()
        put_u32(blob, 0x50, 0x480)
        put_u32(blob, 0x54, 0)
        blob[1] = pred1
        blob[2] = pred2
        fixes = ["0x50"] + plant_table(blob, 0x480, 0x500, 0, 8, mode)
        go(0x008C9AA0, blob, fixes, [], 0, compare="u32")

    mode_case(1, 0, 0)
    mode_case(0, 1, 0)
    mode_case(0, 0, 0)
    mode_case(2, 0, 0)
    mode_case(2, 0, 1)
    mode_case(5, 0, 0)

    for byte14, live1, live0, pred in ((0, 1, 1, 1), (1, 0, 1, 1), (1, 0x180, 0, 1),
                                       (1, 0x180, 0x160, 0), (1, 0x180, 0x160, 1)):
        blob = fresh()
        blob[0x14] = byte14
        blob[3] = pred
        put_u32(blob, 8, live1)   # arg 1
        put_u32(blob, 4, live0)   # arg 0
        put_u32(blob, 0x28, 0)
        fixes = []
        if live1 >= 0x100:
            fixes.append("0x8")
            put_u32(blob, live1, 0x12)
        if live0 >= 0x100:
            fixes.append("0x4")
            put_u32(blob, live0, 0x34)
        go(0x008CD760, blob, fixes, [], 0, compare="u32")

    for subject_byte, mode in ((0, 1), (1, 0), (1, 5)):
        blob = fresh()
        put_u32(blob, 0x28, 0x300 if subject_byte or True else 0)
        blob[0x300] = subject_byte
        put_u32(blob, 0x50, 0x480)
        put_u32(blob, 0x54, 0)
        blob[1] = 0
        blob[2] = 0
        fixes = ["0x28", "0x50"] + plant_table(blob, 0x480, 0x500, 0, 8, mode)
        go(0x008CD9D0, blob, fixes, [], 0, compare="u32")
    blob = fresh()
    put_u32(blob, 0x28, 0)
    put_u32(blob, 0x50, 0x480)
    put_u32(blob, 0x54, 0)
    fixes = ["0x50"] + plant_table(blob, 0x480, 0x500, 0, 8, 1)
    go(0x008CD9D0, blob, fixes, [], 0, compare="u32")

    def bind_parent(blob: bytearray, player: int) -> list[str]:
        put_u32(blob, 4, 0x180)
        put_u32(blob, 0x180 + 0x90, player)
        fixes = ["0x4"]
        if player:
            fixes.append(f"0x{0x180 + 0x90:X}")
        return fixes

    for player, found in ((0, 0), (0x220, 0), (0x220, 0x100)):
        blob = fresh()
        fixes = bind_parent(blob, player)
        put_u32(blob, 0x0C, 0x41)
        put_u32(blob, 0x18, 0x42)
        if player and found:
            put_u32(blob, player + 0x394, found)
            fixes.append(f"0x{player + 0x394:X}")
        elif player:
            put_u32(blob, player + 0x394, 0)
        go(0x008C4D40, blob, fixes, [], 0)

    # catalog miss, list miss, list hit.
    for kind in ("miss", "nomatch", "match"):
        blob = fresh()
        fixes = bind_parent(blob, 0)
        put_u32(blob, 0x10, 0x1234)
        put_u32(blob, 0x0C, 1)
        put_u32(blob, 0x18, 2)
        catalog = 0x5C0
        if kind == "miss":
            put_u32(blob, catalog, 0)
        else:
            put_u32(blob, catalog, 0x600)
            fixes.append(f"0x{catalog:X}")
            put_u32(blob, 0x600 + 0x1A8, 0x640)
            put_u32(blob, 0x600 + 0x1AC, 0x668 if kind == "match" else 0x654)
            fixes += ["0x7A8", "0x7AC"]
            put_u32(blob, 0x640 + 4, 0x690)
            fixes.append("0x644")
            put_u32(blob, 0x690, 0 if kind == "nomatch" else 0xA112A404)
            if kind == "match":
                put_u32(blob, 0x654 + 4, 0x6A0)
                fixes.append("0x658")
                put_u32(blob, 0x6A0, 0xA112A404)
        go(0x008C5140, blob, fixes, [], 0, a1=catalog, catalog=catalog)

    for override in (None, 1, 2):
        go(0x008C0100, make_xfer(override=override), xfer_fixes(), [(XFER,)], 4, compare="none")
    for fill in (0xA5, 0x00, 0x3C):
        go(0x008BDC40, make_xfer(fill=fill), xfer_fixes(), [(XFER,)], 4)
        go(0x008C3040, make_xfer(fill=fill), xfer_fixes(), [(XFER,)], 4)
        go(0x008C48E0, make_xfer(fill=fill), xfer_fixes(), [(XFER,)], 4)
    for flags in (0, 2, 0x80000002):
        blob = make_xfer(flags=flags)
        put_u32(blob, 8, 0x10)
        put_u32(blob, 0x0C, 0x20)
        put_u32(blob, 0x28, 0x30)
        put_u32(blob, 0x2C, 0x40)
        go(0x008B7860, blob, xfer_fixes(), [(XFER,)], 4)
    for flags in (0, 2, 1):
        blob = make_xfer(flags=flags, fill=0x11)
        blob[LOG:] = b"\x00" * (BLOB - LOG)
        put_u32(blob, 0x54, 0x70)
        put_u32(blob, 0x58, 0x71)
        go(0x008C4B10, blob, xfer_fixes(), [(XFER,)], 4)
    for override, flags in ((None, 0), (2, 0), (0, 2)):
        blob = make_xfer(flags=flags, override=override)
        go(0x008BFF40, blob, xfer_fixes(), [(XFER,)], 4, self_off=0x40)
    for flags, current, ident in ((0, 0, 0), (0, 0x160, 0x20), (2, 0x160, 0x20)):
        blob = make_xfer(flags=flags)
        put_u32(blob, 0x70, current)
        put_u32(blob, 0x6C, 0x180)
        fixes = xfer_fixes() + ["0x6C"]
        if current:
            fixes.append("0x70")
            put_u32(blob, current + 4, ident)
        go(0x008C58F0, blob, fixes, [(XFER,)], 4)

    missing = [va for va in ACCEPTED if oracle.counts.get(va, 0) < 3]
    if missing:
        raise AssertionError(
            "entries with fewer than 3 cases: "
            + ", ".join(f"0x{va:08X}={oracle.counts.get(va, 0)}" for va in missing))
    total = sum(oracle.counts.values())
    print(f"skirm8b oracle: {len(oracle.counts)} entries, {total} cases, 0 mismatches")
    oracle.close()


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument(
        "--skirm8bcheck", type=Path,
        default=Path(__file__).resolve().parents[3] / "build/win32/bin/Release/ra3_skirm8bcheck.exe")
    args = parser.parse_args()
    verify(args.skirm8bcheck.resolve())


if __name__ == "__main__":
    main()
