"""Differentially verify the second 0x0069 update cluster against ra3_1.0.game."""

from __future__ import annotations

import struct
import subprocess
import sys
from pathlib import Path

from unicorn import x86_const

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from emulate import Emulator  # noqa: E402

ACCEPTED_ENTRIES = (
    0x00690AF0, 0x00690E60, 0x00691250, 0x006912E0, 0x00691660, 0x00691C30,
    0x00691CB0, 0x00691E20, 0x00691FC0, 0x00692270, 0x00692670, 0x00692AC0,
    0x00694F00, 0x00695290, 0x00695300, 0x00695370, 0x00695490, 0x006954F0,
    0x006963F0, 0x006968A0, 0x006968F0, 0x00696940, 0x00696990, 0x006969E0,
    0x00696C50, 0x00697680, 0x00697B50, 0x006983B0, 0x006984E0, 0x00699BC0,
    0x00699FB0, 0x0069A040, 0x0069C350, 0x0069C6B0, 0x0069CCD0, 0x0069E6F0,
    0x0069FB60, 0x0069FBC0, 0x0069FCD0, 0x0069FEC0,
)

FLT_MAX = 0x7F7FFFFF
DIRECTS = (
    (0x006908E0, 4, False), (0x007F2710, 4, False), (0x00680180, 0, False),
    (0x00691D30, 8, False), (0x0088E060, 8, False), (0x006811C0, 4, False),
    (0x00687720, 4, True), (0x0088AEB0, 12, False), (0x004E30B0, 8, False),
    (0x005BA020, 0, False), (0x005AC4F0, 8, False), (0x0069C530, 4, False),
    (0x004E21F0, 4, False), (0x007E2970, 0, False), (0x00981280, 4, False),
    (0x00982B00, 8, False),
)


def u32(data: bytes, offset: int) -> int:
    return struct.unpack_from("<I", data, offset)[0]


def put_u32(data: bytearray, offset: int, value: int) -> None:
    struct.pack_into("<I", data, offset, value & 0xFFFFFFFF)


def put_f32(data: bytearray, offset: int, value: float) -> None:
    struct.pack_into("<f", data, offset, value)


def fill(size: int, value: int = 0xA5) -> bytearray:
    return bytearray([value]) * size


def zero(data: bytearray, offset: int, size: int) -> None:
    data[offset:offset + size] = b"\x00" * size


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
             a0: int, a1: int, a2: int, stubs: str) -> tuple[int, bytes]:
        assert self.process.stdin is not None and self.process.stdout is not None
        fix_str = ",".join(fixups) if fixups else "-"
        line = (
            f"0x{va:08X} 0x{self_offset:X} {blob.hex()} {fix_str} "
            f"0x{a0 & 0xFFFFFFFF:X} 0x{a1 & 0xFFFFFFFF:X} 0x{a2 & 0xFFFFFFFF:X} "
            f"0x037F {stubs}\n"
        )
        self.process.stdin.write(line)
        self.process.stdin.flush()
        out = self.process.stdout.readline().strip()
        if not out.startswith("ret=0x"):
            raise RuntimeError(f"ra3_obj69bcheck failed on 0x{va:08X}: {out!r}")
        parts = out.split()
        return int(parts[0].split("=")[1], 16), bytes.fromhex(parts[1].split("=")[1])

    def close(self) -> None:
        if self.process.stdin is not None:
            self.process.stdin.close()
        self.process.wait(timeout=20)


class Verifier:
    def __init__(self, executable: Path) -> None:
        self.emu = Emulator()
        self.cpp = CppOracle(executable)
        self.counts: dict[int, int] = {}
        self.stubs: dict[int, int] = {}
        self.queues: dict[int, list[int]] = {}
        self.base = 0
        self._bind()

    def _bind(self) -> None:
        def make_stub(argc: int, result_off: int, count_off: int):
            def stub(emu: Emulator) -> int:
                self_ptr = emu.register(x86_const.UC_X86_REG_ECX)
                result = u32(emu.read(self_ptr + result_off, 4), 0)
                for index in range(argc):
                    arg = emu.stack_arg(index)
                    emu.write(self_ptr + 0x18 + 4 * index, struct.pack("<I", arg & 0xFFFFFFFF))
                count = u32(emu.read(self_ptr + count_off, 4), 0)
                emu.write(self_ptr + count_off, struct.pack("<I", (count + 1) & 0xFFFFFFFF))
                return result
            return stub

        for kind in range(7):
            self.stubs[kind] = self.emu.bind_stub(make_stub(kind, 8, 0x0C), kind * 4)
        self.stubs[7] = self.emu.bind_stub(make_stub(0, 0x10, 0x14), 0)

        for va, cleanup, pointer in DIRECTS:
            def direct(emu: Emulator, target: int = va, is_pointer: bool = pointer) -> int:
                queued = self.queues.get(target)
                value = queued.pop(0) if queued else 0
                if not is_pointer or value == 0:
                    return value & 0xFFFFFFFF
                return (self.base + value) & 0xFFFFFFFF
            self.emu.bind_function(va, direct, cleanup)

    def check(self, va: int, self_offset: int, blob: bytes, fixups: list[str],
              invoke, cleanup: int, *, a0: int, a1: int, a2: int, stubs: str,
              compare_ret: str) -> None:
        self.queues = {}
        if stubs not in ("", "-"):
            for item in stubs.split(","):
                target_s, values_s = item[2:].split("=", 1)
                self.queues[int(target_s, 16)] = [int(part, 16) for part in values_s.split("|")]
        cpp_ret, cpp_blob = self.cpp.call(va, self_offset, bytes(blob), fixups, a0, a1, a2, stubs)
        self.queues = {}
        if stubs not in ("", "-"):
            for item in stubs.split(","):
                target_s, values_s = item[2:].split("=", 1)
                self.queues[int(target_s, 16)] = [int(part, 16) for part in values_s.split("|")]
        self.emu.reset_data()
        self.emu.uc.reg_write(x86_const.UC_X86_REG_FPCW, 0x037F)
        base = self.emu.alloc(bytes(blob))
        self.base = base
        for item in fixups:
            if item.startswith("v") and "@" in item:
                kind_str, off_str = item[1:].split("@", 1)
                off = int(off_str, 0)
                self.emu.write(base + off, struct.pack("<I", self.stubs[int(kind_str, 0)]))
            else:
                off = int(item, 0)
                self.emu.write(base + off, struct.pack("<I", (base + u32(blob, off)) & 0xFFFFFFFF))
        res, game_ret = invoke(self.emu, base)
        if res.stack_cleanup != cleanup:
            raise AssertionError(
                f"0x{va:08X}: stack cleanup {res.stack_cleanup}, expected {cleanup}")
        game_blob = bytearray(self.emu.read(base, len(blob)))
        stub_rev = {addr: kind for kind, addr in self.stubs.items()}
        for off in range(0, len(game_blob) - 3, 4):
            cur = u32(game_blob, off)
            if base <= cur < base + len(game_blob):
                put_u32(game_blob, off, cur - base)
            elif cur in stub_rev:
                put_u32(game_blob, off, 0xF0000000 | stub_rev[cur])
        if bytes(game_blob) != cpp_blob:
            for idx, (g, c) in enumerate(zip(game_blob, cpp_blob)):
                if g != c:
                    raise AssertionError(
                        f"0x{va:08X}: blob mismatch at +0x{idx:X}: "
                        f"game=0x{g:02X} cpp=0x{c:02X}")
        if compare_ret != "none":
            if compare_ret == "u8":
                g_val, c_val = game_ret & 0xFF, cpp_ret & 0xFF
            elif compare_ret == "ptr":
                if game_ret == 0:
                    g_val = 0
                elif base <= game_ret < base + len(blob):
                    g_val = game_ret - base
                else:
                    g_val = game_ret & 0xFFFFFFFF
                c_val = cpp_ret & 0xFFFFFFFF
            else:
                g_val, c_val = game_ret & 0xFFFFFFFF, cpp_ret & 0xFFFFFFFF
            if g_val != c_val:
                raise AssertionError(
                    f"0x{va:08X}: return mismatch ({compare_ret}): "
                    f"game=0x{g_val:08X} cpp=0x{c_val:08X}")
        self.counts[va] = self.counts.get(va, 0) + 1

    def close(self) -> None:
        self.cpp.close()


def verify(executable: Path) -> None:
    oracle = Verifier(executable)

    def run(va: int, blob: bytearray, stack, cleanup: int, *, self_off: int = 0,
            a0: int = 0, a1: int = 0, a2: int = 0, fixups: list[str] | None = None,
            stubs: str = "-", compare: str = "none", float_ret: bool = False,
            tag: str = "") -> None:
        if va not in ACCEPTED_ENTRIES:
            raise AssertionError(hex(va))

        def invoke(machine: Emulator, base: int):
            ecx = 0 if self_off >= len(blob) else base + self_off
            args = stack(base) if callable(stack) else stack
            if float_ret:
                result, bits = machine.call_float32(va, args, ecx=ecx)
                return result, bits
            result = machine.call(va, args, ecx=ecx)
            return result, result.eax

        try:
            oracle.check(va, self_off, blob, fixups or [], invoke, cleanup,
                         a0=a0, a1=a1, a2=a2, stubs=stubs, compare_ret=compare)
        except AssertionError as exc:
            raise AssertionError(f"{tag} {exc}") from None

    def salvage(probed: int, field: int, flags: int, other_null: bool = False) -> None:
        blob = fill(0x500)
        if other_null:
            other = 0xFFFFFFFF
        else:
            other = 0x100
            put_u32(blob, 0x100 + 0x374, field)
            put_u32(blob, 0x104, 0x200)
            put_u32(blob, 0x200 + 0xC0, flags)
        run(0x00690AF0, blob, lambda b: [0 if other_null else b + other], 4,
            self_off=0x20, a0=other, fixups=[] if other_null else ["0x104"],
            stubs=f"r:006908E0={probed:X}", compare="u8", tag="salvage")

    salvage(0, 0, 0, other_null=True)
    salvage(1, 0, 0)
    salvage(1, 1, 0)
    salvage(1, 1, 0x10)

    def masks(primary: int, secondary: int, byte_self: int, byte_body: int, words: int) -> None:
        blob = fill(0x200)
        put_u32(blob, 0x24, 0x40)
        put_u32(blob, 0x28, 0x80)
        for index in range(7):
            put_u32(blob, 0x100 + index * 4, words)
            put_u32(blob, 0x6C + index * 4, primary if index == 0 else 0)
            put_u32(blob, 0x50 + index * 4, secondary if index == 0 else 0)
        blob[0x38] = byte_self
        blob[0x8C] = byte_body
        run(0x00690E60, blob, [], 0, self_off=0x20, fixups=["0x24", "0x28"],
            compare="u8", tag="masks")

    masks(0, 0, 0, 0, 0)
    masks(1, 0, 0, 0, 0xFFFFFFFF)
    masks(0, 1, 0, 0, 0)
    masks(0, 0, 1, 1, 0)
    masks(0, 0, 1, 0, 0)

    def anchor(mode: str) -> None:
        blob = fill(0x680)
        self_off = 0x200
        put_u32(blob, self_off, 0x100)
        put_u32(blob, self_off + 8, 1 if mode == "virtual" else 0)
        put_u32(blob, self_off - 0x164, 0 if mode == "null" else 0x300)
        if mode != "null":
            put_u32(blob, 0x300 + 0x90, 0x200000 if mode == "flag" else 0)
        if mode in ("clear", "hit"):
            put_u32(blob, self_off - 0x168, 0x480)
            zero(blob, 0x300 + 0x80, 28)
            zero(blob, 0x480 + 0x194, 28)
            if mode == "hit":
                put_u32(blob, 0x300 + 0x80, 1)
                put_u32(blob, 0x480 + 0x194, 1)
                put_u32(blob, 0x300 + 0x90, 0)
        fixups = ["0x200", "v0@0x1A0"]
        if mode != "null":
            fixups.append("0x9C")
        if mode in ("clear", "hit"):
            fixups.append("0x98")
        run(0x00691250, blob, [], 0, self_off=self_off, fixups=fixups,
            compare="u8", tag=f"anchor-{mode}")

    anchor("null")
    anchor("flag")
    anchor("virtual")
    anchor("clear")
    anchor("hit")

    def forward(ident: int) -> None:
        blob = fill(0x200)
        put_u32(blob, 0x20, 0x100)
        put_u32(blob, 0x28, 0x55)
        put_u32(blob, 0x8C, 0xA0)
        put_u32(blob, 0xA0, 0xC0)
        put_u32(blob, 0xC4, ident)
        run(0x006912E0, blob, lambda b: [b + 0x80], 4, self_off=0x20, a0=0x80,
            fixups=["0x20", "0x8C", "0xA0", "v1@0x1B0"], compare="u32", tag="forward")

    forward(0x42)
    forward(0)

    def relay(update: int, module: int) -> None:
        blob = fill(0x700)
        put_u32(blob, 0x1C, 0x200)
        put_u32(blob, 0x200 + 0x418, update)
        if update:
            put_u32(blob, update + 0x90, module)
        fixups = ["0x1C"]
        if update:
            fixups.append(hex(0x200 + 0x418))
        if module:
            fixups.append(hex(update + 0x90))
        run(0x00691660, blob, [0x77], 4, self_off=0x180, a0=0x77, fixups=fixups,
            tag=f"relay-{update:X}")

    relay(0, 0)
    relay(0x400, 0)
    relay(0x400, 0x500)

    def span(begin: int, end: int) -> None:
        blob = fill(0x280)
        put_u32(blob, 0x28, 0x80)
        put_u32(blob, 0x80 + 0x13C, begin)
        put_u32(blob, 0x80 + 0x140, end)
        run(0x00691C30, blob, [], 0, self_off=0x30, fixups=["0x28"],
            compare="ptr", tag=f"span-{end - begin}")

    span(0, 0)
    span(0, 80)
    span(0, 40)

    def select(other_null: bool, key: int, kind: int, link_field: int) -> None:
        blob = fill(0x520)
        put_u32(blob, 0x38, key)
        put_u32(blob, 0x34, 0x80)
        put_u32(blob, 0x80 + 0x0C, 0 if kind == 2 and link_field == 0 else 0xC0)
        put_u32(blob, 0x80 + 0x10, 0 if kind != 2 and link_field == 0 else 0xD0)
        put_u32(blob, 0xC0, 0x111)
        put_u32(blob, 0xD0, 0x222)
        put_u32(blob, 0x100 + 0x418, 0)
        other = 0xFFFFFFFF if other_null else 0x100
        fixups = ["0x34"]
        if u32(blob, 0x8C):
            fixups.append("0x8C")
        if u32(blob, 0x90):
            fixups.append("0x90")
        run(0x00691CB0, blob, lambda b: [0 if other_null else b + 0x100, b + 0xE0], 8,
            self_off=0x40, a0=other, a1=0xE0, fixups=fixups,
            stubs=f"r:007F2710={kind:X}", compare="u8", tag="select")

    select(True, 1, 0, 0)
    select(False, 0, 0, 0)
    select(False, 5, 2, 0xC0)
    select(False, 5, 1, 0xD0)
    select(False, 5, 1, 0)

    def slot(index: int, slot_value: int) -> None:
        blob = fill(0x700)
        stubs = "-" if index > 0x13 else f"r:00680180={slot_value:X}"
        run(0x00691E20, blob, [index], 4, self_off=0x40, a0=index, stubs=stubs,
            compare="ptr", tag=f"slot-{index}-{slot_value}")

    slot(0, 0)
    slot(1, 2)
    slot(0x14, 0)

    def copy_triple(dest_null: bool, key: int, located: int) -> None:
        blob = fill(0x340)
        put_u32(blob, 0x1C0, 0x11)
        put_u32(blob, 0x1C4, 0x22)
        put_u32(blob, 0x1C8, 0x33)
        dest = 0xFFFFFFFF if dest_null else 0x300
        stubs = "-" if dest_null or key == 0 else f"r:00680180=0,r:00691D30={located & 0xFFFFFFFF:X}"
        run(0x00691FC0, blob, lambda b: [0 if dest_null else b + dest, key], 8,
            self_off=0x40, a0=dest, a1=key, stubs=stubs, compare="u8", tag="copy")

    copy_triple(True, 1, 0)
    copy_triple(False, 0, 0)
    copy_triple(False, 4, 0xFFFFFFFF)
    copy_triple(False, 4, 0)

    def release(kind: int, flags: int, ready: int) -> None:
        blob = fill(0x360)
        put_u32(blob, 0x18, 0x80)
        put_u32(blob, 0x84, 0xE0)
        put_u32(blob, 0x1A8, flags)
        put_u32(blob, 0x34, 0x220)
        put_u32(blob, 0x44, ready)
        fixups = ["0x18", "0x84", "0x34", "v1@0x32C", "v1@0x2A8"]
        run(0x00692270, blob, [0, 0, kind], 0xC, self_off=0x40, a2=kind,
            fixups=fixups, tag=f"release-{kind}-{ready}")

    release(0, 0, 0)
    release(2, 0x8000, 0)
    release(2, 0, 0)
    release(2, 0, 1)

    def refresh(same: bool) -> None:
        blob = fill(0x500)
        put_u32(blob, 0x28, 0x80)
        put_u32(blob, 0x80 + 0xF4, 0x100)
        put_u32(blob, 0x20, 0x400)
        if not same:
            blob[0x100 + 0x264] = 0x11
        run(0x00692670, blob, [], 0, self_off=0x20,
            fixups=["0x28", "0x174", "0x20", "v0@0x478"], tag=f"refresh-{same}")

    refresh(True)
    refresh(False)

    def redirect(left: int, right: int, flags: int, byte: int) -> None:
        blob = fill(0x320)
        put_u32(blob, 0x28, 0x80)
        put_u32(blob, 0x80 + 0x26C, flags)
        blob[0x80 + 0x8E] = byte
        put_u32(blob, 0x44, 0x200)
        run(0x00692AC0, blob, [left, right], 8, self_off=0x20, a0=left, a1=right,
            fixups=["0x28", "0x44", "v1@0x2A4", "v1@0x288"], tag="redirect")

    redirect(3, 3, 0, 0)
    redirect(1, 2, 0x20, 0)
    redirect(1, 2, 0, 1)
    redirect(1, 2, 0, 0)

    def scavenger(enabled: int, flag: int, bits: int) -> None:
        blob = fill(0x800)
        blob[0x40 + 0x3C] = enabled
        put_u32(blob, 0x34, 0x80)
        put_u32(blob, 0x80 + 0x1C0, bits)
        put_u32(blob, 0x38, 0x300)
        put_u32(blob, 0x300 + 0x418, 0x600)
        fixups = ["0x34", "0x38", "0x718"] if enabled else []
        run(0x00694F00, blob, [flag], 4, self_off=0x40, a0=flag, fixups=fixups,
            tag=f"scavenger-{enabled}")

    scavenger(0, 0, 0x3F800000)
    scavenger(1, 0, 0x3F800000)
    scavenger(1, 1, 0x3F800000)

    def dispatch(va: int, slot: int, kind: int, count: int, returns: list[int],
                 pending: int = 0, arg: int = 0, stop: bool = False) -> None:
        blob = fill(0x400)
        put_u32(blob, 0x34, 0x80)
        put_u32(blob, 0x38, 0)
        put_u32(blob, 0x80 + 0x1C0, count)
        put_u32(blob, 0x80 + 0x1C4, 0x280 if count else 0)
        put_u32(blob, 0x40 + 0x24, pending)
        put_u32(blob, 0x40 + 0x20, 0xA5A5A5A5)
        blob[0x40 + 0x30] = 1
        items = (0x100, 0x180)
        for index, ret in enumerate(returns):
            put_u32(blob, 0x280 + index * 4, 0x10 + index)
            put_u32(blob, items[index], 0x300)
            put_u32(blob, items[index] + 8, ret)
        fixups = ["0x34", "0x244"] if count else ["0x34"]
        for index in range(len(returns)):
            fixups.append(hex(items[index]))
        if count and slot:
            fixups.append(f"v{kind}@0x{0x300 + slot:X}")
        queued = "|".join(f"{items[index]:X}" for index in range(len(returns)))
        stubs = f"p:00687720={queued}" if returns else "-"
        cleanup = 4 if va in (0x00695370,) else 0
        stack = [arg] if cleanup else []
        run(va, blob, stack, cleanup, self_off=0x40, a0=arg, fixups=fixups,
            stubs=stubs, compare="u8" if va in (0x00695290, 0x00695300) else "none",
            tag=f"dispatch-{va:X}-{stop}")

    dispatch(0x00695290, 4, 0, 0, [])
    dispatch(0x00695290, 4, 0, 1, [0])
    dispatch(0x00695290, 4, 0, 2, [0, 1], stop=True)
    dispatch(0x00695290, 4, 0, 1, [0], stop=False)
    # The zero found value is a separate queue entry, not an object return.
    blob = fill(0x400)
    put_u32(blob, 0x34, 0x80)
    put_u32(blob, 0x80 + 0x1C0, 1)
    put_u32(blob, 0x80 + 0x1C4, 0x280)
    put_u32(blob, 0x280, 7)
    run(0x00695290, blob, [], 0, self_off=0x40, fixups=["0x34", "0x244"],
        stubs="p:00687720=0", compare="u8", tag="dispatch-null")
    dispatch(0x00695300, 0x10, 0, 0, [], pending=0)
    dispatch(0x00695300, 0x10, 0, 0, [], pending=3)
    dispatch(0x00695300, 0x10, 0, 1, [1], pending=0)
    dispatch(0x00695370, 0x24, 1, 0, [], arg=0x64)
    dispatch(0x00695370, 0x24, 1, 1, [0], arg=0x64)
    dispatch(0x00695490, 0x4C, 0, 0, [])
    dispatch(0x00695490, 0x4C, 0, 1, [0])
    dispatch(0x006954F0, 0x54, 0, 0, [])
    dispatch(0x006954F0, 0x54, 0, 1, [0])

    def scale(va: int, sub_off: int, queried: int, bit: int, flag: int) -> None:
        blob = fill(0x200)
        self_off = 0x40
        put_u32(blob, self_off + sub_off, 0x100)
        put_u32(blob, self_off + sub_off + 8, queried)
        if queried:
            put_u32(blob, queried + 8, 0x180)
            put_u32(blob, 0x180 + 0x1C, bit)
        fixups = [hex(self_off + sub_off), "v0@0x11C", "v1@0x144"]
        if queried:
            fixups += [hex(self_off + sub_off + 8), hex(queried + 8)]
        run(va, blob, [0x3F800000, flag], 8, self_off=self_off, a0=0x3F800000, a1=flag,
            fixups=fixups, tag=f"scale-{va:X}")

    scale(0x006963F0, 4, 0, 0, 1)
    scale(0x006963F0, 4, 0x140, 0, 1)
    scale(0x006963F0, 4, 0x140, 1 << 14, 1)
    scale(0x00696C50, 0x1C, 0, 0, 0)
    scale(0x00696C50, 0x1C, 0x140, 1 << 14, 1)
    scale(0x00696C50, 0x1C, 0x140, 1 << 14, 0)

    def fire(va: int, count: int, gate: int, kind: int, args: list[int]) -> None:
        blob = fill(0x200)
        put_u32(blob, 0x88, count)
        put_u32(blob, 0x90, gate)
        put_u32(blob, 0x80, 0x100)
        put_u32(blob, 0x7C, 0x140)
        put_u32(blob, 0x5C, 0x40)
        put_u32(blob, 0x48, 0x111)
        fixups = ["0x80", "0x7C", "0x5C", f"v7@0x19C", f"v{kind}@0x140"]
        run(va, blob, args, 4 * len(args), self_off=0x80, a0=args[0] if args else 0,
            a1=args[1] if len(args) > 1 else 0, a2=args[2] if len(args) > 2 else 0,
            fixups=fixups, tag=f"fire-{va:X}-{count:X}-{gate}")

    fire(0x006968A0, 1, 1, 6, [0x99])
    fire(0x006968A0, 0, 0, 6, [0x99])
    fire(0x006968A0, 0, 1, 6, [0x99])
    fire(0x006968F0, 1, 1, 6, [0x31, 0x32])
    fire(0x006968F0, 0, 1, 6, [0x31, 0x32])
    fire(0x00696940, 5, 1, 6, [0x41, 0x42])
    fire(0x00696940, 0, 1, 6, [0x41, 0x42])
    fire(0x00696990, 0, 0, 6, [0x51, 0x3F000000, 0x52])
    fire(0x00696990, 0, 1, 6, [0x51, 0x3F000000, 0x52])
    fire(0x006969E0, 0xFFFFFFFF, 1, 6, [0x61, 0x62, 0x63])
    fire(0x006969E0, 0, 1, 6, [0x61, 0x62, 0x63])

    def dozer(module: int) -> None:
        blob = fill(0x620)
        put_u32(blob, 0x6C, 0x100)
        put_u32(blob, 0x100 + 0xDC, 0x45)
        put_u32(blob, 0x100 + 0x418, 0x200)
        put_u32(blob, 0x200 + 0x90, module)
        fixups = ["0x6C", "0x518"]
        if module:
            fixups.append("0x290")
        run(0x00697680, blob, [], 0, self_off=0x580, fixups=fixups, tag=f"dozer-{module}")

    dozer(0)
    dozer(0x300)

    def dock(target: int, result: int) -> None:
        blob = fill(0x520)
        put_u32(blob, 0x20, 0x80)
        put_u32(blob, 0x94, target)
        put_u32(blob, 0x24, 0xC0)
        put_u32(blob, 0xC0 + 0x418, 0x99)
        fixups = ["0x20", "0x24"]
        if target:
            fixups.append("0x94")
        stubs = f"r:004E30B0={result:X}" if target else "-"
        run(0x00697B50, blob, [0x17], 4, self_off=0x40, a0=0x17, fixups=fixups,
            stubs=stubs, compare="u8", tag="dock")

    dock(0, 0)
    dock(0xA0, 0x11)
    dock(0xA0, 0)

    def project(has_matrix: bool) -> None:
        blob = fill(0x340)
        put_u32(blob, 0x60, 0x180)
        put_u32(blob, 0x64, 0x100 if has_matrix else 0)
        put_f32(blob, 0x180 + 0x14, 1.0)
        put_f32(blob, 0x180 + 0x18, 0.0)
        put_f32(blob, 0x180 + 0x1C, 0.0)
        put_f32(blob, 0x180 + 8, 3.0)
        put_f32(blob, 0x100 + 0x08, 2.0)
        put_f32(blob, 0x100 + 0x44, 4.0)
        for off in (0x0C, 0x10, 0x14, 0x18, 0x1C, 0x20, 0x24, 0x28, 0x2C, 0x30, 0x34):
            put_f32(blob, 0x100 + off, 0.0)
        fixups = ["0x60"]
        if has_matrix:
            fixups.append("0x64")
        run(0x006983B0, blob, lambda b: [b + 0x300, b + 0x310], 8, self_off=0x80,
            a0=0x300, a1=0x310, fixups=fixups, compare="u8", tag=f"project-{has_matrix}")

    project(False)
    project(True)
    blob = fill(0x340)
    put_u32(blob, 0x60, 0x180)
    put_u32(blob, 0x64, 0x100)
    put_f32(blob, 0x180 + 0x14, 1.0)
    put_f32(blob, 0x180 + 0x18, 2.0)
    put_f32(blob, 0x180 + 0x1C, 4.0)
    put_f32(blob, 0x180 + 8, 0.5)
    put_f32(blob, 0x100 + 0x08, 1.0)
    put_f32(blob, 0x100 + 0x0C, 1.0)
    put_f32(blob, 0x100 + 0x10, 1.0)
    put_f32(blob, 0x100 + 0x14, 0.25)
    put_f32(blob, 0x100 + 0x18, 1.0)
    put_f32(blob, 0x100 + 0x1C, 1.0)
    put_f32(blob, 0x100 + 0x20, 1.0)
    put_f32(blob, 0x100 + 0x24, 0.5)
    put_f32(blob, 0x100 + 0x28, 1.0)
    put_f32(blob, 0x100 + 0x2C, 1.0)
    put_f32(blob, 0x100 + 0x30, 1.0)
    put_f32(blob, 0x100 + 0x34, 0.125)
    put_f32(blob, 0x100 + 0x44, 1.5)
    run(0x006983B0, blob, lambda b: [b + 0x300, b + 0x310], 8, self_off=0x80,
        a0=0x300, a1=0x310, fixups=["0x60", "0x64"], compare="u8", tag="project-full")

    def idle(field: int, gate: int) -> None:
        blob = fill(0x40)
        put_u32(blob, 0x10 + 0x1C, field)
        put_u32(blob, 0x14, gate)
        run(0x006984E0, blob, [0, 0], 8, self_off=0x10, compare="u32", tag="idle")

    idle(1, 0)
    idle(0, 0)
    idle(0, 5)

    def stamp(packed: int, stamp_value: int) -> None:
        blob = fill(0x480)
        put_u32(blob, 0x5C, 0x100)
        put_u32(blob, 0x100, 0x300)
        put_u32(blob, 0x108, 0)
        run(0x00699BC0, blob, [packed, stamp_value], 8, self_off=0x20,
            a0=packed, a1=stamp_value, fixups=["0x5C", "0x100", "v0@0x334", "v1@0x31C"],
            tag=f"stamp-{stamp_value}")

    stamp(0x10, 1)
    stamp(0x20, 0x99)

    def goal(state: int, gate: int, kind: int, flags: int) -> None:
        blob = fill(0x1C0)
        put_u32(blob, 0x5C, state)
        put_u32(blob, 0x28, 0x80)
        put_u32(blob, 0x80 + 0x8C, flags)
        if state:
            put_u32(blob, state + 0x88, 0x140)
            put_u32(blob, 0x144, kind)
            put_u32(blob, state + 4, 0)
        fixups = ["0x28"]
        if state:
            fixups += ["0x5C", hex(state + 0x88)]
        run(0x00699FB0, blob, [], 0, self_off=0x20, fixups=fixups,
            stubs=f"r:005BA020={gate:X}", compare="u32", tag=f"goal-{kind:X}")

    goal(0, 0, 0, 0)
    goal(0, 4, 0, 0)
    goal(0, 4, 0, 2)
    goal(0x100, 9, 0x0F, 0)
    goal(0x100, 3, 0x70, 0)
    goal(0x100, 3, 0x70, 2)

    def facing(limit_bits: int, stamp_value: int) -> None:
        blob = fill(0x220)
        put_u32(blob, 0x28, 0x80)
        put_u32(blob, 0x84, 0xA0)
        put_u32(blob, 0xB4, 0xC0)
        put_u32(blob, 0xC0 + 0x120, limit_bits)
        put_u32(blob, 0x5C, 0x100)
        put_u32(blob, 0x100, 0x180)
        put_u32(blob, 0x108, 0)
        run(0x0069A040, blob, [0x3F800000, stamp_value], 8, self_off=0x20,
            a0=0x3F800000, a1=stamp_value,
            fixups=["0x28", "0x84", "0xB4", "0x5C", "0x100", "v0@0x1B4", "v1@0x19C"],
            tag=f"facing-{limit_bits:X}")

    facing(0x43B40000, 1)
    facing(0, 7)
    facing(0x43B38000, 8)

    def member_scale(value: float) -> None:
        blob = fill(0x420)
        put_u32(blob, 0x20, 0x80)
        put_f32(blob, 0x90, value)
        put_u32(blob, 0x24, 0xA0)
        put_u32(blob, 0xA0 + 0x36C, 0)
        run(0x0069C350, blob, [], 0, self_off=0x40, fixups=["0x20", "0x24"],
            compare="u32", float_ret=True, tag="member")

    member_scale(1.0)
    member_scale(0.0)
    member_scale(-2.5)

    def differ(gate: int, first: int, second: int) -> None:
        blob = fill(0xA0)
        put_u32(blob, 0x20, 0x80)
        put_u32(blob, 0x88, gate)
        stubs = "-" if gate == 0 else f"r:0069C530={first:X}|{second:X}"
        run(0x0069C6B0, blob, [0, 0x11, 0x22], 0xC, self_off=0x40, a1=0x11, a2=0x22,
            fixups=["0x20"], stubs=stubs, compare="u8", tag="differ")

    differ(0, 0, 0)
    differ(1, 0, 1)
    differ(1, 1, 0)
    differ(1, 1, 1)

    def leech(target: int) -> None:
        blob = fill(0x400)
        put_u32(blob, 0x37C, target)
        fixups = []
        if target:
            put_u32(blob, target, target + 0x20)
            put_u32(blob, target + 8, 0x22)
            fixups = ["0x37C", hex(target), f"v1@0x{target + 0x28:X}"]
        run(0x0069CCD0, blob, lambda b: [b + 0x40], 4, self_off=0, a0=0x40,
            fixups=fixups, compare="ptr", tag="leech")

    leech(0)
    leech(0x3A0)

    def production(head: int, skip: int) -> None:
        blob = fill(0x1A0)
        put_u32(blob, 0x30, head)
        put_u32(blob, 0x20, 0x100)
        put_u32(blob, 0x28, 0)
        if head:
            blob[head + 0x65] = skip
            blob[head + 0x64] = 0
            put_u32(blob, head + 0x10, 0)
        fixups = ["0x20", "v1@0x170"]
        if head:
            fixups.append("0x30")
        run(0x0069E6F0, blob, [], 0, self_off=0x20, fixups=fixups, compare="ptr",
            tag=f"production-{head}")

    production(0, 0)
    production(0x80, 0)
    production(0x80, 1)

    def kind_match(mode: int, call_ret: int, node: int, kind: int, object_ptr: int) -> None:
        blob = fill(0x120)
        put_u32(blob, 0x40 + 0x14, mode)
        put_u32(blob, 0x20, 0x80)
        put_u32(blob, 0x88, object_ptr)
        put_u32(blob, 0x5C, 0xC0)
        put_u32(blob, 0xC4, node)
        if node:
            put_u32(blob, node + 4, kind)
        fixups = ["0x20", "0x5C"]
        if object_ptr:
            fixups.append("0x88")
        if node:
            fixups.append("0xC4")
        stubs = f"r:007E2970={call_ret:X}" if mode in (1, 3) and object_ptr else "-"
        run(0x0069FB60, blob, [], 0, self_off=0x40, fixups=fixups, stubs=stubs,
            compare="u8", tag=f"kind-{mode}-{kind:X}")

    kind_match(0, 0, 0, 0, 0)
    kind_match(2, 0, 0xE0, 0x63, 0)
    kind_match(1, 0, 0, 0, 0xA0)
    kind_match(1, 1, 0, 0, 0xA0)
    kind_match(3, 1, 0xE0, 0x66, 0xA0)
    kind_match(2, 0, 0, 0, 0)

    def ready(state: int, virtual_ret: int, flags: int, byte: int, float_bits: int) -> None:
        blob = fill(0x1C0)
        put_u32(blob, 0x20 + 0x38, state)
        put_u32(blob, 0x24, 0x80 if state == 4 else 0)
        put_u32(blob, 0x28, virtual_ret)
        put_u32(blob, 0x20, 0x100)
        put_u32(blob, 0x80 + 0x2C, flags)
        put_u32(blob, 0x80 + 0x20, float_bits)
        blob[0x20 + 0x9D] = byte
        fixups = ["0x20", "v0@0x1A8"]
        if state == 4:
            fixups.append("0x24")
        run(0x0069FBC0, blob, [], 0, self_off=0x20, fixups=fixups, compare="u8",
            tag=f"ready-{state}")

    ready(0, 0, 0, 0, 0)
    ready(4, 1, 0, 0, 0)
    ready(4, 0, 2, 1, 0)
    ready(4, 0, 6, 2, 0)

    def resolve(stored: int) -> None:
        blob = fill(0x320)
        put_u32(blob, 0x24, 0x80)
        put_u32(blob, 0x80 + 0xDC, stored)
        put_u32(blob, 0x88, 0x15)
        put_u32(blob, 0x28, 0)
        put_u32(blob, 0x100, 0x200)
        put_u32(blob, 0x108, 0)
        fixups = ["0x24"]
        stubs = "-"
        if stored == 0:
            fixups += ["0x100", "v1@0x298"]
            stubs = "p:00687720=100"
        run(0x0069FCD0, blob, lambda b: [b + 0x180], 4, self_off=0x30, a0=0x180,
            fixups=fixups, stubs=stubs, compare="ptr", tag=f"resolve-{stored:X}")

    resolve(0x99)
    resolve(0)

    def touch(self_off: int) -> None:
        blob = fill(0x140)
        put_u32(blob, self_off + 4, 0x80)
        blob[0x80 + 0x98] = 0
        run(0x0069FEC0, blob, [], 0, self_off=self_off, fixups=[hex(self_off + 4)],
            compare="ptr", tag="touch")

    touch(0x20)
    touch(0x30)

    missing = [va for va in ACCEPTED_ENTRIES if oracle.counts.get(va, 0) < 2]
    if missing:
        raise AssertionError("fewer than 2 cases: " + ", ".join(f"0x{va:08X}" for va in missing))
    cases = sum(oracle.counts.values())
    print(f"obj69b oracle: {len(ACCEPTED_ENTRIES)} entries, {cases} cases, 0 mismatches")
    oracle.close()


if __name__ == "__main__":
    exe = Path(sys.argv[1]) if len(sys.argv) > 1 else Path(
        "Ra3Source/build/win32/bin/Release/ra3_obj69bcheck.exe")
    verify(exe)
