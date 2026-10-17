"""Differentially verify the net5d2 lobby and transport leaves against ra3_1.0.game."""

from __future__ import annotations

import struct
import subprocess
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from emulate import Emulator  # noqa: E402

GAMEINFO = 0x00DF6B1C
LOGIC = 0x00DEBA64
NETWORK = 0x00DF6B2C
NULL = 0xFFFFFFFF
ECHO = 0xF00

CLEANUP = {
    0x005D3770: 0, 0x005D3790: 0, 0x005D37B0: 0, 0x005D37D0: 0, 0x005D3C50: 0,
    0x005D3C70: 0, 0x005D4010: 0, 0x005D4920: 8, 0x005D4960: 0, 0x005D5450: 8,
    0x005D5540: 8, 0x005D5BD0: 0, 0x005D5C40: 0, 0x005DC180: 0, 0x005DC1C0: 0,
    0x005DC1F0: 0, 0x005DBFF0: 0, 0x005DCF10: 0, 0x005D68B0: 0, 0x005D68C0: 0,
    0x005D68D0: 8, 0x005D68F0: 4, 0x005DD100: 4, 0x005D6010: 4, 0x005D60B0: 4,
    0x005D6140: 0, 0x005D3260: 4, 0x005D3290: 4, 0x005D32C0: 4, 0x005D6430: 4,
    0x005DB640: 4, 0x005DB670: 8, 0x005D6040: 4, 0x005D60F0: 4, 0x005D4DF0: 4,
    0x005D5C00: 0, 0x005D6840: 4, 0x005DB600: 0, 0x005D5B50: 8, 0x005D5F80: 0,
    0x005D8480: 0, 0x005D84A0: 0,
}


def u32(data: bytes | bytearray, offset: int) -> int:
    return struct.unpack_from("<I", data, offset)[0]


def put(data: bytearray, offset: int, value: int) -> None:
    struct.pack_into("<I", data, offset, value & 0xFFFFFFFF)


def put16(data: bytearray, offset: int, value: int) -> None:
    struct.pack_into("<H", data, offset, value & 0xFFFF)


def fill(size: int, value: int = 0xA5) -> bytearray:
    return bytearray([value & 0xFF]) * size


class CppOracle:
    def __init__(self, executable: Path) -> None:
        self.process = subprocess.Popen(
            [str(executable), "--batch"],
            stdin=subprocess.PIPE,
            stdout=subprocess.PIPE,
            text=True,
            bufsize=1,
        )

    def call(self, va: int, self_offset: int, blob: bytes, fixups: list[str],
             a0: int, a1: int, a2: int) -> tuple[int, bytes]:
        assert self.process.stdin is not None and self.process.stdout is not None
        fix = ",".join(fixups) if fixups else "-"
        line = (
            f"0x{va:08X} 0x{self_offset:X} {blob.hex()} {fix} "
            f"0x{a0 & 0xFFFFFFFF:X} 0x{a1 & 0xFFFFFFFF:X} 0x{a2 & 0xFFFFFFFF:X}\n"
        )
        self.process.stdin.write(line)
        self.process.stdin.flush()
        out = self.process.stdout.readline().strip()
        if not out.startswith("ret=0x"):
            raise RuntimeError(f"ra3_net5d2check failed on 0x{va:08X}: {out}")
        parts = out.split()
        return int(parts[0].split("=")[1], 16), bytes.fromhex(parts[1].split("=")[1])

    def close(self) -> None:
        if self.process.stdin is not None:
            self.process.stdin.close()
        self.process.wait(timeout=10)


class Verifier:
    def __init__(self, executable: Path) -> None:
        self.emu = Emulator()
        self.cpp = CppOracle(executable)
        self.counts: dict[int, int] = {}
        self.endpoint = 0

        def memcpy_stub(emu: Emulator) -> int:
            dest = emu.stack_arg(0)
            src = emu.stack_arg(1)
            count = emu.stack_arg(2)
            if count:
                emu.write(dest, emu.read(src, count))
            return dest

        def zero_stub(emu: Emulator) -> int:
            return 0

        def endpoint_stub(emu: Emulator) -> int:
            return self.endpoint

        def network_stub(emu: Emulator) -> int:
            from unicorn import x86_const
            net = emu.register(x86_const.UC_X86_REG_ECX)
            cur = u32(emu.read(net + 4, 4), 0)
            emu.write(net + 4, struct.pack("<I", (cur + 1) & 0xFFFFFFFF))
            return 0

        self.emu.bind_function(0x004D2EDA, memcpy_stub, 0)
        self.vstubs = {
            0x10: self.emu.bind_stub(zero_stub, 0),
            0x20: self.emu.bind_stub(endpoint_stub, 0),
            0x7C: self.emu.bind_stub(network_stub, 0),
        }

    def check(self, va: int, blob: bytearray, fixups: list[str], raw: tuple[int, int, int],
              stack: list[tuple[str, int]], mode: str, globals_: dict[int, int | None] | None = None,
              self_off: int = 0) -> None:
        cpp_ret, cpp_blob = self.cpp.call(va, self_off, bytes(blob), fixups, raw[0], raw[1], raw[2])
        self.emu.reset_data()
        base = self.emu.alloc(bytes(blob))
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
        if globals_:
            for address, offset in globals_.items():
                value = 0 if offset is None else (base + offset) & 0xFFFFFFFF
                self.emu.write(address, struct.pack("<I", value))
        self.endpoint = (base + 0x500) & 0xFFFFFFFF
        resolved = []
        for kind, value in stack:
            if kind == "ptr":
                resolved.append(0 if value == NULL else (base + value) & 0xFFFFFFFF)
            else:
                resolved.append(value & 0xFFFFFFFF)
        res = self.emu.call(va, resolved, ecx=(base + self_off) & 0xFFFFFFFF)
        if res.stack_cleanup != CLEANUP[va]:
            raise AssertionError(f"0x{va:08X}: cleanup {res.stack_cleanup} expected {CLEANUP[va]}")
        game_blob = bytearray(self.emu.read(base, len(blob)))
        reverse = {addr: kind for kind, addr in self.vstubs.items()}
        for off in range(0, len(game_blob) - 3, 4):
            cur = u32(game_blob, off)
            if base <= cur < base + len(game_blob):
                put(game_blob, off, cur - base)
            elif cur in reverse:
                put(game_blob, off, 0xF0000000 | reverse[cur])
        if va == 0x005D6040 and len(game_blob) >= ECHO + 4:
            global_value = u32(self.emu.read(GAMEINFO, 4), 0)
            if base <= global_value < base + len(game_blob):
                global_value -= base
            elif global_value != 0:
                global_value &= 0xFFFFFFFF
            put(game_blob, ECHO, global_value)
        if bytes(game_blob) != cpp_blob:
            for idx, (game_byte, cpp_byte) in enumerate(zip(game_blob, cpp_blob)):
                if game_byte != cpp_byte:
                    raise AssertionError(
                        f"0x{va:08X}: blob mismatch at +0x{idx:X}: "
                        f"game=0x{game_byte:02X} cpp=0x{cpp_byte:02X}"
                    )
            raise AssertionError(f"0x{va:08X}: blob length {len(game_blob)} vs {len(cpp_blob)}")
        game_ret = res.eax & 0xFFFFFFFF
        if mode == "ptr":
            if game_ret == 0:
                game_val = 0
            elif base <= game_ret < base + len(blob):
                game_val = game_ret - base
            else:
                game_val = game_ret
        elif mode == "al":
            game_val = game_ret & 0xFF
            cpp_ret &= 0xFF
        else:
            game_val = game_ret
        if game_val != (cpp_ret & 0xFFFFFFFF):
            raise AssertionError(
                f"0x{va:08X}: return ({mode}) game=0x{game_val:08X} cpp=0x{cpp_ret & 0xFFFFFFFF:08X}"
            )
        self.counts[va] = self.counts.get(va, 0) + 1

    def close(self) -> None:
        self.cpp.close()


def slot(blob: bytearray, base: int, index: int, state: int, local: int = 0, ip: int = 0,
         ident: int = 0, accepted: int = 0) -> str:
    off = base + index * 0x60
    put(blob, off + 4, state)
    put(blob, off + 0x38, local)
    put(blob, off + 0x10, ip)
    put(blob, off + 0xC, ident)
    blob[off + 0x54] = accepted & 0xFF
    put(blob, 4 + index * 4, off)
    return f"0x{4 + index * 4:X}"


def child(blob: bytearray, off: int = 0x40) -> str:
    put(blob, 0xC0, off)
    return "0xC0"


def run(verifier: Verifier) -> None:
    # Child field relays.
    for va, offset, mode in (
        (0x005D3770, 0x2E, "al"), (0x005D3790, 0x2D, "al"),
        (0x005D3C50, 0x29, "al"), (0x005D3C70, 0x28, "al"),
    ):
        empty = fill(0x100)
        put(empty, 0xC0, 0)
        verifier.check(va, empty, [], (0, 0, 0), [], mode)
        present = fill(0x100)
        present[0x40 + offset] = 0x5A
        fix = [child(present)]
        verifier.check(va, present, fix, (0, 0, 0), [], mode)
    for va, offset in ((0x005D8480, 0x6C), (0x005D84A0, 0x7C)):
        empty = fill(0x100)
        put(empty, 0xC0, 0)
        verifier.check(va, empty, [], (0, 0, 0), [], "full")
        present = fill(0x100)
        put(present, 0x40 + offset, 0x11223344)
        verifier.check(va, present, [child(present)], (0, 0, 0), [], "full")
    dword = fill(0x100)
    put(dword, 0xC0, 0)
    verifier.check(0x005D37B0, dword, [], (0, 0, 0), [], "full")
    put(dword, 0x40 + 0x30, 0x01020304)
    verifier.check(0x005D37B0, dword, [child(dword)], (0, 0, 0), [], "full")
    field = fill(0x100)
    put(field, 0xC0, 0)
    verifier.check(0x005D37D0, field, [], (0, 0, 0), [], "ptr")
    verifier.check(0x005D37D0, field, [child(field, 0x20)], (0, 0, 0), [], "ptr")

    differ = fill(0x100)
    put(differ, 0xF0, 0x12345600)
    put(differ, 0xF4, 0x12345600)
    verifier.check(0x005D4010, differ, [], (0, 0, 0), [], "full")
    put(differ, 0xF4, 0x00000001)
    verifier.check(0x005D4010, differ, [], (0, 0, 0), [], "full")
    put(differ, 0xF0, 0x0000007F)
    put(differ, 0xF4, 0x0000007F)
    verifier.check(0x005D4010, differ, [], (0, 0, 0), [], "full")

    pair = fill(0x140)
    verifier.check(0x005D4920, pair, [], (0x111, 0x222, 0), [("imm", 0x111), ("imm", 0x222)], "full")
    verifier.check(0x005D4920, pair, [], (0, 0xFFFFFFFF, 0), [("imm", 0), ("imm", 0xFFFFFFFF)], "full")
    flag = fill(0x140)
    put(flag, 0x130, 0)
    flag[0x134] = 0
    verifier.check(0x005D4960, flag, [], (0, 0, 0), [], "full")
    flag[0x134] = 1
    verifier.check(0x005D4960, flag, [], (0, 0, 0), [], "full")
    flag[0x134] = 0
    put(flag, 0x130, 1)
    verifier.check(0x005D4960, flag, [], (0, 0, 0), [], "full")

    # NAT buffer read / seek.
    buf = fill(0xC0)
    put(buf, 0x14, 8)
    put(buf, 0x1C, 2)
    put(buf, 0x20, 0x40)
    buf[0x40:0x48] = bytes(range(8))
    verifier.check(0x005D5450, buf, ["0x20"], (0x80, 3, 0), [("ptr", 0x80), ("imm", 3)], "full")
    verifier.check(0x005D5450, buf, ["0x20"], (NULL, 4, 0), [("ptr", NULL), ("imm", 4)], "full")
    verifier.check(0x005D5450, buf, ["0x20"], (0x80, 0, 0), [("ptr", 0x80), ("imm", 0)], "full")
    verifier.check(0x005D5450, buf, ["0x20"], (0x80, 0xFFFFFFFF, 0), [("ptr", 0x80), ("imm", 0xFFFFFFFF)], "full")
    put(buf, 0x1C, 8)
    verifier.check(0x005D5450, buf, ["0x20"], (0x80, 2, 0), [("ptr", 0x80), ("imm", 2)], "full")

    seek = fill(0x30)
    put(seek, 0x14, 10)
    put(seek, 0x1C, 4)
    verifier.check(0x005D5540, seek, [], (0, 0, 0), [("imm", 0), ("imm", 0)], "full")
    put(seek, 0x1C, 4)
    verifier.check(0x005D5540, seek, [], (3, 1, 0), [("imm", 3), ("imm", 1)], "full")
    put(seek, 0x1C, 4)
    verifier.check(0x005D5540, seek, [], (0, 2, 0), [("imm", 0), ("imm", 2)], "full")
    put(seek, 0x1C, 1)
    verifier.check(0x005D5540, seek, [], (11, 0, 0), [("imm", 11), ("imm", 0)], "full")
    verifier.check(0x005D5540, seek, [], (0, 3, 0), [("imm", 0), ("imm", 3)], "full")
    verifier.check(0x005D5540, seek, [], (10, 0, 0), [("imm", 10), ("imm", 0)], "full")

    # GameInfo slots.
    info = fill(0x280)
    fixes = []
    for index, state, local, accepted in (
        (0, 6, 0x42, 1), (1, 2, 0, 1), (2, 1, 0, 1), (3, 6, 0x42, 0), (4, 5, 0, 0), (5, 4, 9, 1),
    ):
        fixes.append(slot(info, 0x40, index, state, local, ip=0x10 + index, ident=0x100 + index, accepted=accepted))
    info[0x2C] = 1
    put(info, 0x30, 0x42)
    verifier.check(0x005D5BD0, info, fixes, (0, 0, 0), [], "full")
    cleared = fill(0x20)
    for index in range(6):
        put(cleared, 4 + index * 4, 0)
    verifier.check(0x005D5BD0, cleared, [], (0, 0, 0), [], "full")
    info[0x2C] = 1
    verifier.check(0x005DC180, info, fixes, (0, 0, 0), [], "full")
    info[0x2C] = 0
    verifier.check(0x005DC180, info, fixes, (0, 0, 0), [], "full")
    info[0x2C] = 1
    put(info, 0x30, 0x99)
    verifier.check(0x005DC180, info, fixes, (0, 0, 0), [], "full")
    put(info, 0x30, 0x42)
    info[0x2C] = 1
    last = fill(0x280)
    last_fixes = []
    for index in range(6):
        last_fixes.append(slot(last, 0x40, index, 6 if index == 5 else 1,
                               0x77 if index == 5 else 0))
    last[0x2C] = 1
    put(last, 0x30, 0x77)
    verifier.check(0x005DC180, last, last_fixes, (0, 0, 0), [], "full")
    put(last, 0, 0x00BE8018)
    verifier.check(0x005DC1F0, last, last_fixes, (0, 0, 0), [], "al")
    verifier.check(0x005DC1C0, info, fixes, (0, 0, 0), [], "full")
    info[0x2C] = 0
    verifier.check(0x005DC1C0, info, fixes, (0, 0, 0), [], "full")
    info[0x2C] = 1
    put(info, 0, 0x00BE8018)
    verifier.check(0x005DC1F0, info, fixes, (0, 0, 0), [], "al")
    put(info, 0x30, 0x42)
    # Slot 0 is the only human; the others are state 1.
    quiet = fill(0x280)
    qfix = []
    for index in range(6):
        qfix.append(slot(quiet, 0x40, index, 6 if index == 0 else 1, 0x42 if index == 0 else 0))
    quiet[0x2C] = 1
    put(quiet, 0x30, 0x42)
    put(quiet, 0, 0x00BE8018)
    verifier.check(0x005DC1F0, quiet, qfix, (0, 0, 0), [], "al")
    accepted = fill(0x280)
    afix = []
    for index, state, byte in ((0, 6, 1), (1, 6, 0), (2, 2, 1), (3, 0, 1), (4, 0, 0), (5, 5, 1)):
        afix.append(slot(accepted, 0x40, index, state, accepted=byte))
    verifier.check(0x005DBFF0, accepted, afix, (0, 0, 0), [], "full")
    blank_slots = fill(0x40)
    for index in range(6):
        put(blank_slots, 4 + index * 4, 0)
    verifier.check(0x005DBFF0, blank_slots, [], (0, 0, 0), [], "full")

    share = fill(0x280)
    sfix = []
    for index, ident in enumerate((7, 8, 7, 0, 0, 9)):
        sfix.append(slot(share, 0x40, index, 0, ident=ident))
    verifier.check(0x005D5B50, share, sfix, (7, 0, 0), [("imm", 7), ("imm", 0)], "full")
    verifier.check(0x005D5B50, share, sfix, (7, 2, 0), [("imm", 7), ("imm", 2)], "full")
    verifier.check(0x005D5B50, share, sfix, (7, NULL, 0), [("imm", 7), ("imm", NULL)], "full")
    verifier.check(0x005D5B50, share, sfix, (4, 0, 0), [("imm", 4), ("imm", 0)], "full")

    ai = fill(0x10)
    for state in (0, 1, 2, 5, 6):
        put(ai, 4, state)
        verifier.check(0x005D5F80, ai, [], (0, 0, 0), [], "full")

    # Dismiss indexed slot.
    dismiss = fill(0x200)
    dismiss[0xFA] = 0
    verifier.check(0x005D5C40, dismiss, [], (0, 0, 0), [], "full")
    dismiss[0xFA] = 6
    verifier.check(0x005D5C40, dismiss, [], (0, 0, 0), [], "full")
    dismiss[0xFA] = 0xFF
    verifier.check(0x005D5C40, dismiss, [], (0, 0, 0), [], "full")
    dismiss[0xFA] = 1
    put(dismiss, 8, 0x100)
    put(dismiss, 0x100, 0x180)
    verifier.check(0x005D5C40, dismiss, ["0x8", "0x100", "v10@0x190"], (0, 0, 0), [], "full")

    # LAN ready bytes.
    lan = fill(0x400)
    for offset, state in ((0x100, 6), (0x180, 1), (0x200, 6), (0x280, 6), (0x300, 0), (0x380, 6)):
        put(lan, offset, state)
        lan[offset + 4] = 0xAB
    verifier.check(0x005DCF10, lan, [], (NULL, 0, 0), [], "full", {NETWORK: None})
    put(lan, 0x400 - 4, 0)  # keep size; network lives in a larger blob
    lan = fill(0x520)
    for offset, state in ((0x100, 6), (0x180, 5), (0x200, 6), (0x280, 6), (0x300, 6), (0x380, 2)):
        put(lan, offset, state)
        lan[offset + 4] = 0xCD
    put(lan, 0x400, 0x480)
    put(lan, 0x404, 3)
    verifier.check(0x005DCF10, lan, ["0x400", "v7C@0x4FC"], (0x400, 0, 0), [], "full", {NETWORK: 0x400})

    # Link / channel object 00BEEDB0.
    link = fill(0x40)
    put(link, 0x24, 1)
    verifier.check(0x005D68B0, link, [], (0, 0, 0), [], "full")
    put(link, 0x24, 2)
    verifier.check(0x005D68B0, link, [], (0, 0, 0), [], "full")
    put(link, 0x24, 0)
    verifier.check(0x005D68B0, link, [], (0, 0, 0), [], "full")
    child_link = fill(0x40)
    put(child_link, 0x28, 0)
    verifier.check(0x005D68C0, child_link, [], (0, 0, 0), [], "ptr")
    put(child_link, 0x28, 0x10)
    put(child_link, 0x18, 0x20)
    verifier.check(0x005D68C0, child_link, ["0x28", "0x18"], (0, 0, 0), [], "ptr")

    stamp = fill(0x400)
    put(stamp, 0x28, 0x40)
    put(stamp, 0x40 + 0x4C, 0x100)
    put(stamp, 0x40 + 0x188 + 0x4C, 5)
    verifier.check(0x005D68D0, stamp, ["0x28"], (0, 0x50, 0), [("imm", 0), ("imm", 0x50)], "ptr")
    verifier.check(0x005D68D0, stamp, ["0x28"], (0, 0x200, 0), [("imm", 0), ("imm", 0x200)], "ptr")
    verifier.check(0x005D68D0, stamp, ["0x28"], (1, 0, 0), [("imm", 1), ("imm", 0)], "ptr")
    verifier.check(0x005D68D0, stamp, ["0x28"], (1, 0xFFFFFFFF, 0), [("imm", 1), ("imm", 0xFFFFFFFF)], "ptr")
    bit = fill(0x40)
    put(bit, 0x2C, 0)
    verifier.check(0x005D68F0, bit, [], (0, 0, 0), [("imm", 0)], "ptr")
    verifier.check(0x005D68F0, bit, [], (31, 0, 0), [("imm", 31)], "ptr")
    verifier.check(0x005D68F0, bit, [], (32, 0, 0), [("imm", 32)], "ptr")
    verifier.check(0x005D68F0, bit, [], (3, 0, 0), [("imm", 3)], "ptr")

    frame = fill(0x400)
    put(frame, 0x28, 0x80)
    put(frame, 0x80 + 0x4C, 10)
    put(frame, 0x80 + 0x188 + 0x4C, 50)
    put(frame, 0x200 + 4, 0x300)
    put(frame, 0x300 + 4, 6)
    put(frame, 0x1D8, 10)
    verifier.check(0x005DD100, frame, ["0x28", "0x204"], (0, 0x200, 0x180), [("imm", 0)], "al",
                   {GAMEINFO: 0x200, LOGIC: 0x180})
    put(frame, 0x80 + 0x4C, 11)
    verifier.check(0x005DD100, frame, ["0x28", "0x204"], (0, 0x200, 0x180), [("imm", 0)], "al",
                   {GAMEINFO: 0x200, LOGIC: 0x180})
    verifier.check(0x005DD100, frame, ["0x28"], (6, NULL, 0x180), [("imm", 6)], "al",
                   {GAMEINFO: None, LOGIC: 0x180})
    verifier.check(0x005DD100, frame, ["0x28"], (0, NULL, 0x180), [("imm", 0)], "al",
                   {GAMEINFO: None, LOGIC: 0x180})
    put(frame, 0x300 + 4, 5)
    verifier.check(0x005DD100, frame, ["0x28", "0x204"], (0, 0x200, 0x180), [("imm", 0)], "al",
                   {GAMEINFO: 0x200, LOGIC: 0x180})
    put(frame, 0x204, 0)
    put(frame, 0x208, 0)
    verifier.check(0x005DD100, frame, ["0x28"], (1, 0x200, 0x180), [("imm", 1)], "al",
                   {GAMEINFO: 0x200, LOGIC: 0x180})

    # Wide chain and endpoint list on 00BEF6E8.
    chain = fill(0x900)
    put(chain, 0x28, 0x100)
    put(chain, 0x100 + 0x3FC, 0x500)
    put(chain, 0x500 + 0x3FC, 0)
    verifier.check(0x005D6010, chain, ["0x28", "0x4FC"], (0, 0, 0), [("imm", 0)], "ptr")
    verifier.check(0x005D6010, chain, ["0x28", "0x4FC"], (1, 0, 0), [("imm", 1)], "ptr")
    verifier.check(0x005D6010, chain, ["0x28", "0x4FC"], (2, 0, 0), [("imm", 2)], "ptr")
    verifier.check(0x005D6010, chain, ["0x28", "0x4FC"], (NULL, 0, 0), [("imm", NULL)], "ptr")
    empty_chain = fill(0x40)
    put(empty_chain, 0x28, 0)
    verifier.check(0x005D6010, empty_chain, [], (1, 0, 0), [("imm", 1)], "ptr")

    ends = fill(0x80)
    put(ends, 0, 0x0A000001)
    put16(ends, 4, 27015)
    put(ends, 0x10, 0x0A000001)
    put16(ends, 0x14, 27015)
    verifier.check(0x005D3260, ends, [], (0x10, 0, 0), [("ptr", 0x10)], "full")
    put16(ends, 0x14, 1)
    verifier.check(0x005D3260, ends, [], (0x10, 0, 0), [("ptr", 0x10)], "full")
    verifier.check(0x005D3290, ends, [], (0x10, 0, 0), [("ptr", 0x10)], "full")
    put16(ends, 0x14, 27015)
    verifier.check(0x005D3290, ends, [], (0x10, 0, 0), [("ptr", 0x10)], "full")

    tiny = fill(0x20)
    for value in (0, 2, NULL, 3, 4):
        verifier.check(0x005D32C0, tiny, [], (value, 0, 0), [("imm", value)], "full")

    records = fill(0x600)
    verifier.check(0x005D6430, records, [], (0, 0, 0), [("imm", 0)], "ptr")
    verifier.check(0x005D6430, records, [], (5, 0, 0), [("imm", 5)], "ptr")
    verifier.check(0x005D6430, records, [], (6, 0, 0), [("imm", 6)], "ptr")
    put(records, 0x100, 6)
    put(records, 0x178, 0x22222222)
    put(records, 0x180, 1)
    verifier.check(0x005DB640, records, [], (0, 0, 0), [("imm", 0)], "full")
    verifier.check(0x005DB640, records, [], (1, 0, 0), [("imm", 1)], "full")
    verifier.check(0x005DB670, records, [], (0, 0x33333333, 0), [("imm", 0), ("imm", 0x33333333)], "full")
    verifier.check(0x005DB670, records, [], (1, 0x44444444, 0), [("imm", 1), ("imm", 0x44444444)], "full")

    wide = fill(0x1000)
    put(wide, 0x28, 0x100)
    put(wide, 0x100 + 0x3FC, 0x500)
    put(wide, 0x500 + 0x3FC, 0x900)
    put(wide, 0x900 + 0x3FC, 0)
    verifier.check(0x005D6040, wide, ["0x28", "0x4FC", "0x8FC"], (0x500, 0x40, 0), [("ptr", 0x500)], "ptr",
                   {GAMEINFO: 0x40})
    verifier.check(0x005D6040, wide, ["0x28", "0x8FC"], (0x100, 0x100, 0), [("ptr", 0x100)], "ptr",
                   {GAMEINFO: 0x100})
    verifier.check(0x005D6040, wide, ["0x28"], (NULL, NULL, 0), [("ptr", NULL)], "ptr", {GAMEINFO: None})
    verifier.check(0x005D6040, wide, ["0x28", "0x4FC", "0x8FC"], (0x40, 0x40, 0), [("ptr", 0x40)], "ptr",
                   {GAMEINFO: 0x40})

    short = fill(0x80)
    put(short, 0x24, 0x30)
    put(short, 0x30 + 0x10, 0x50)
    put(short, 0x50 + 0x10, 0)
    verifier.check(0x005D60F0, short, ["0x24", "0x40"], (0x50, 0, 0), [("ptr", 0x50)], "ptr")
    verifier.check(0x005D60F0, short, ["0x24"], (0x30, 0, 0), [("ptr", 0x30)], "ptr")
    verifier.check(0x005D60F0, short, [], (NULL, 0, 0), [("ptr", NULL)], "ptr")

    found = fill(0x90)
    put(found, 0x24, 0x20)
    put(found, 0x20 + 0x14, 0x0A000002)
    put16(found, 0x20 + 0x18, 80)
    put(found, 0x20 + 0x10, 0x50)
    put(found, 0x50 + 0x14, 0x0A000009)
    put16(found, 0x50 + 0x18, 90)
    put(found, 0x50 + 0x10, 0)
    put(found, 0x70, 0x0A000009)
    put16(found, 0x74, 90)
    verifier.check(0x005D60B0, found, ["0x24", "0x30"], (0x70, 0, 0), [("ptr", 0x70)], "ptr")
    put16(found, 0x74, 0)
    put(found, 0x70, 0x0A000002)
    verifier.check(0x005D60B0, found, ["0x24", "0x30"], (0x70, 0, 0), [("ptr", 0x70)], "ptr")
    put(found, 0x70, 0x01020304)
    put16(found, 0x74, 1)
    verifier.check(0x005D60B0, found, ["0x24", "0x30"], (0x70, 0, 0), [("ptr", 0x70)], "ptr")
    no_head = fill(0x30)
    put(no_head, 0x24, 0)
    verifier.check(0x005D60B0, no_head, [], (0x10, 0, 0), [("ptr", 0x10)], "ptr")

    match = fill(0x520)
    put(match, 0, 0x200)
    put(match, 0x5C, 0x300)
    put(match, 0x300 + 0x134, 0xAABBCCDD)
    put16(match, 0x300 + 0x138, 4000)
    put(match, 0x500, 0xAABBCCDD)
    put16(match, 0x504, 4000)
    verifier.check(0x005D6140, match, ["0x0", "0x5C", "v20@0x2F8"], (0, 0, 0), [], "full")
    put16(match, 0x504, 1)
    verifier.check(0x005D6140, match, ["0x0", "0x5C", "v20@0x2F8"], (0, 0, 0), [], "full")
    no_match_head = fill(0x80)
    put(no_match_head, 0x5C, 0)
    verifier.check(0x005D6140, no_match_head, [], (0, 0, 0), [], "full")

    tagged = fill(0x20)
    verifier.check(0x005D4DF0, tagged, [], (0x10, 0, 0), [("ptr", 0x10)], "ptr")
    verifier.check(0x005D4DF0, tagged, [], (NULL, 0, 0), [("ptr", NULL)], "ptr")
    state = fill(0x60)
    for value in (0, 5, 6, 7, 8):
        put(state, 0x5C, value)
        verifier.check(0x005D5C00, state, [], (0, 0, 0), [], "full")
    retain = fill(0x40)
    put(retain, 0x28, 0x08)
    put(retain, 0x18, 4)
    verifier.check(0x005D6840, retain, ["0x28"], (0x28, 0, 0), [("ptr", 0x28)], "ptr")
    retain_zero = fill(0x40)
    put(retain_zero, 0x28, 0x08)
    put(retain_zero, 0x18, 0)
    verifier.check(0x005D6840, retain_zero, ["0x28"], (0x28, 0, 0), [("ptr", 0x28)], "ptr")
    extents = fill(0x30)
    verifier.check(0x005DB600, extents, [], (0, 0, 0), [], "full")
    extents_zero = bytearray(0x30)
    verifier.check(0x005DB600, extents_zero, [], (0, 0, 0), [], "full")


def main() -> None:
    root = Path(__file__).resolve().parents[3]
    executable = root / "build" / "win32" / "bin" / "Release" / "ra3_net5d2check.exe"
    if not executable.is_file():
        executable = root / "Ra3Source" / "build" / "win32" / "bin" / "Release" / "ra3_net5d2check.exe"
    verifier = Verifier(executable)
    try:
        run(verifier)
    finally:
        verifier.close()
    missing = [f"0x{va:08X}" for va in CLEANUP if verifier.counts.get(va, 0) < 2]
    if missing:
        raise SystemExit("functions with fewer than 2 cases: " + ", ".join(missing))
    print(f"net5d2 oracle: 0 mismatches, {sum(verifier.counts.values())} cases, {len(verifier.counts)} functions")


if __name__ == "__main__":
    main()
