"""Differentially verify the audio50 SageAudio ports against ra3_1.0.game."""

from __future__ import annotations

import struct
import subprocess
import sys
from pathlib import Path

from unicorn import x86_const

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from emulate import Emulator  # noqa: E402

COUNTED = (
    0x00502270, 0x00502540, 0x00502FB0, 0x00502FF0, 0x00503290, 0x00503300, 0x00503400,
    0x005044F0, 0x00504560, 0x005045C0, 0x005045E0, 0x00504AC0, 0x00504AF0, 0x00504C80,
    0x00504DF0, 0x00504EC0, 0x00505340, 0x005053B0, 0x00506430, 0x00506670, 0x00506680,
    0x00506C20, 0x00506C90, 0x00507720, 0x00508640, 0x0050B090, 0x0050DF80, 0x0050E510,
    0x0050FA80, 0x0050FD50,
)
NULL = 0xFFFFFFFF
VT_MARKER = 0xF1000000
CLEANUP = {
    0x00502270: 0, 0x00502540: 4, 0x00502FB0: 0, 0x00502FF0: 0, 0x00503290: 0,
    0x00503300: 0, 0x00503400: 0, 0x005044F0: 4, 0x00504560: 0, 0x005045C0: 0,
    0x005045E0: 0, 0x00504AC0: 4, 0x00504AF0: 4, 0x00504C80: 8, 0x00504DF0: 4,
    0x00504EC0: 0, 0x00505340: 0, 0x005053B0: 0, 0x00506430: 4, 0x00506670: 4,
    0x00506680: 4, 0x00506C20: 0x10, 0x00506C90: 8, 0x00507720: 0, 0x00508640: 4,
    0x0050B090: 4, 0x0050DF80: 4, 0x0050E510: 4, 0x0050FA80: 8, 0x0050FD50: 0,
}
GLOBAL_LAYOUT = (
    (0x00DEE1C8, 4),
    (0x00CA7D7C, 4),
    (0x00CA7E7C, 4),
    (0x00DF03F8, 4),
    (0x00DF6098, 4),
    (0x00DEE290, 0x5F4),
)


def le(value: int) -> str:
    return struct.pack("<I", value & 0xFFFFFFFF).hex()


def fhex(value: float) -> str:
    return struct.pack("<f", value).hex()


def put(blob: bytearray, offset: int, data: bytes) -> None:
    end = offset + len(data)
    if offset < 0 or end > len(blob):
        raise AssertionError(f"put out of range +0x{offset:X} size 0x{len(blob):X}")
    blob[offset:end] = data


def u32(data: bytes, offset: int) -> int:
    return struct.unpack_from("<I", data, offset)[0]


class Oracle:
    def __init__(self, executable: Path) -> None:
        self.cpp = subprocess.Popen(
            [str(executable), "--batch"],
            stdin=subprocess.PIPE,
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
            text=True,
        )
        self.emu = Emulator()
        self.vt = 0
        self.stubs: dict[int, int] = {}
        self.counts = {va: 0 for va in COUNTED}
        self._bind()

    def _bind(self) -> None:
        def release(machine: Emulator) -> int:
            this = machine.register(x86_const.UC_X86_REG_ECX)
            machine.write(this + 8, struct.pack("<I", machine.stack_arg(0)))
            return 0x42

        def stop(machine: Emulator) -> int:
            this = machine.register(x86_const.UC_X86_REG_ECX)
            machine.write(this + 4, struct.pack("<I", machine.stack_arg(0)))
            return 0x55

        def head(machine: Emulator) -> int:
            this = machine.register(x86_const.UC_X86_REG_ECX)
            slot = machine.stack_arg(0)
            raw = machine.read(slot, 2)
            machine.write(this + 0x10, struct.pack("<I", raw[0] | (raw[1] << 8)))
            return 0x11

        def body(machine: Emulator) -> int:
            this = machine.register(x86_const.UC_X86_REG_ECX)
            machine.write(this + 0x14, struct.pack("<I", machine.stack_arg(0)))
            return 0x77

        self.stubs = {
            self.emu.bind_stub(release, 4): 0,
            self.emu.bind_stub(stop, 4): 1,
            self.emu.bind_stub(head, 4): 2,
            self.emu.bind_stub(body, 4): 3,
        }
        self.stub_by_slot = {marker: addr for addr, marker in self.stubs.items()}

    def close(self) -> None:
        if self.cpp.stdin:
            self.cpp.stdin.close()
        self.cpp.wait(timeout=10)

    def _baseline(self) -> None:
        self.emu.reset_data()
        self.emu.write(0x00DEE1C8, b"\0" * 4)
        self.emu.write(0x00CA7D7C, b"\0" * 4)
        self.emu.write(0x00CA7E7C, b"\0" * 4)
        self.emu.write(0x00DF03F8, b"\0" * 4)
        self.emu.write(0x00DF6098, b"\0" * 4)
        self.emu.write(0x00DEE290, b"\0" * 0x5F4)
        self.emu.write(0x00BD90A4, struct.pack("<f", 1.0))
        self.emu.write(0x00CA9F04, struct.pack("<8f", *([1.0] * 8)))
        slots = [0] * 32
        slots[0] = self.stub_by_slot[0]
        slots[6] = self.stub_by_slot[2]
        slots[14] = self.stub_by_slot[1]
        slots[27] = self.stub_by_slot[3]
        self.vt = self.emu.alloc(struct.pack("<32I", *slots))

    def _apply_patches(self, patches: str) -> None:
        if patches in ("-", "."):
            return
        for item in patches.split(","):
            addr_s, hex_bytes = item.split("=", 1)
            self.emu.write(int(addr_s, 0), bytes.fromhex(hex_bytes))

    def _normalize(self, value: int, base: int, size: int) -> int:
        if size and base <= value < base + size:
            return value - base
        if value == self.vt:
            return VT_MARKER
        if value in self.stubs:
            return 0xF0000000 | self.stubs[value]
        if not size:
            return value
        high = value & 0xFFFFFF00
        lo = base & 0xFFFFFF00
        hi = (base + size - 1) & 0xFFFFFF00
        if lo <= high <= hi:
            return (value - lo) & 0xFFFFFFFF
        return value

    def _normalize_bytes(self, data: bytearray, base: int, size: int) -> None:
        for off in range(0, len(data) - 3, 4):
            cur = u32(data, off)
            nxt = self._normalize(cur, base, size)
            if nxt != cur:
                struct.pack_into("<I", data, off, nxt)

    def _globals(self, base: int, size: int) -> str:
        raw = bytearray()
        for addr, length in GLOBAL_LAYOUT:
            raw += self.emu.read(addr, length)
        self._normalize_bytes(raw, base, size)
        return raw.hex()

    def check(
        self,
        va: int,
        blob: bytearray,
        *,
        self_off: str = "0x0",
        fixups: str = "-",
        a0: int = 0,
        a1: int = 0,
        a2: int = 0,
        a3: int = 0,
        eax: int = 0xA5A5A5A5,
        patches: str = "-",
        compare: str = "u32",
        fpcw: int = 0x037F,
    ) -> None:
        blob_hex = blob.hex() if blob else "-"
        line = (
            f"0x{va:08X} {self_off} {blob_hex} {fixups} "
            f"0x{a0:X} 0x{a1:X} 0x{a2:X} 0x{a3:X} 0x{eax:X} 0x{fpcw:X} {patches}"
        )
        assert self.cpp.stdin is not None and self.cpp.stdout is not None
        self.cpp.stdin.write(line + "\n")
        self.cpp.stdin.flush()
        reply = self.cpp.stdout.readline().strip()
        if not reply:
            err = self.cpp.stderr.read() if self.cpp.stderr else ""
            raise AssertionError(f"0x{va:08X}: checker closed: {err}")
        try:
            ret_s, blob_s, g_s = reply.split()
        except ValueError as error:
            raise AssertionError(f"0x{va:08X}: bad reply {reply}") from error
        cpp_ret = int(ret_s.split("=", 1)[1], 16)
        cpp_blob = blob_s.split("=", 1)[1]
        cpp_g = g_s.split("=", 1)[1]

        self._baseline()
        self._apply_patches(patches)
        base = self.emu.alloc(bytes(blob) if blob else b"")
        for item in [] if fixups in ("-", ".") else fixups.split(","):
            if item.startswith("t@"):
                off = int(item[2:], 0)
                self.emu.write(base + off, struct.pack("<I", self.vt))
            elif item.startswith("g"):
                addr_s, off_s = item[1:].split("@", 1)
                self.emu.write(int(addr_s, 0), struct.pack("<I", (base + int(off_s, 0)) & 0xFFFFFFFF))
            else:
                off = int(item, 0)
                rel = u32(blob, off)
                self.emu.write(base + off, struct.pack("<I", (base + rel) & 0xFFFFFFFF))

        if self_off[:1] in ("A", "a"):
            ecx = int(self_off[1:], 0)
        else:
            off = int(self_off, 0)
            ecx = 0 if off == NULL or off >= len(blob) else base + off
        self.emu.uc.reg_write(x86_const.UC_X86_REG_FPCW, fpcw)
        result = self.emu.call(va, (a0 if a0 < len(blob) or a0 == 0 else a0,), ecx=ecx, eax=eax)
        # The call above mis-handles pointer arguments. Re-issue with decoded args.
        del result
        args = []
        for arg in (a0, a1, a2, a3):
            args.append(arg)
        # Pointer-shaped arguments are decoded per function below; this placeholder
        # is replaced by the real invocation in _invoke.
        raise AssertionError("internal")

    def invoke(
        self,
        va: int,
        blob: bytearray,
        stack: list[int],
        *,
        self_off: str = "0x0",
        fixups: str = "-",
        eax: int = 0xA5A5A5A5,
        patches: str = "-",
        compare: str = "u32",
        fpcw: int = 0x037F,
        pointer_args: tuple[int, ...] = (),
    ) -> None:
        raw = bytes(blob)
        a = [0, 0, 0, 0]
        for index, value in enumerate(stack):
            a[index] = value
        blob_hex = raw.hex() if raw else "-"
        line = (
            f"0x{va:08X} {self_off} {blob_hex} {fixups} "
            f"0x{a[0]:X} 0x{a[1]:X} 0x{a[2]:X} 0x{a[3]:X} 0x{eax:X} 0x{fpcw:X} {patches}"
        )
        assert self.cpp.stdin is not None and self.cpp.stdout is not None
        self.cpp.stdin.write(line + "\n")
        self.cpp.stdin.flush()
        reply = self.cpp.stdout.readline().strip()
        if not reply:
            err = ""
            if self.cpp.stderr:
                err = self.cpp.stderr.read()
            raise AssertionError(f"0x{va:08X}: checker closed: {err}\nline={line}")
        parts = reply.split()
        if len(parts) != 3:
            raise AssertionError(f"0x{va:08X}: bad reply {reply}")
        cpp_ret = int(parts[0].split("=", 1)[1], 16)
        cpp_blob = parts[1].split("=", 1)[1]
        cpp_g = parts[2].split("=", 1)[1]

        self._baseline()
        self._apply_patches(patches)
        base = self.emu.alloc(raw if raw else b"\0", alignment=256)
        size = len(raw)
        for item in [] if fixups in ("-", ".") else fixups.split(","):
            if item.startswith("t@"):
                self.emu.write(base + int(item[2:], 0), struct.pack("<I", self.vt))
            elif item.startswith("g"):
                addr_s, off_s = item[1:].split("@", 1)
                self.emu.write(int(addr_s, 0), struct.pack("<I", base + int(off_s, 0)))
            else:
                off = int(item, 0)
                rel = u32(raw, off)
                self.emu.write(base + off, struct.pack("<I", base + rel))

        if self_off[:1] in ("A", "a"):
            ecx = int(self_off[1:], 0)
        else:
            off = int(self_off, 0)
            ecx = 0 if off == NULL or not size or off >= size else base + off

        decoded = []
        for index, value in enumerate(stack):
            if value == NULL:
                decoded.append(0)
            elif index in pointer_args and size and value < size:
                decoded.append(base + value)
            else:
                decoded.append(value)
        while len(decoded) < 4 and False:
            decoded.append(0)
        self.emu.uc.reg_write(x86_const.UC_X86_REG_FPCW, fpcw)
        result = self.emu.call(va, decoded, ecx=ecx, eax=eax)
        if result.stack_cleanup != CLEANUP[va]:
            raise AssertionError(
                f"0x{va:08X}: cleanup {result.stack_cleanup}, expected {CLEANUP[va]}"
            )
        game_blob = bytearray(self.emu.read(base, size)) if size else bytearray()
        self._normalize_bytes(game_blob, base, size)
        game_g = self._globals(base, size)
        game_ret = self._normalize(result.eax & 0xFFFFFFFF, base, size)
        if game_blob.hex() != cpp_blob:
            self._diff_bytes(va, "blob", game_blob.hex(), cpp_blob)
        if game_g != cpp_g:
            self._diff_bytes(va, "globals", game_g, cpp_g)
        if compare == "u8":
            game_ret &= 0xFF
            cpp_ret &= 0xFF
        if compare != "none" and game_ret != cpp_ret:
            raise AssertionError(
                f"0x{va:08X}: return mismatch game=0x{game_ret:08X} cpp=0x{cpp_ret:08X} "
                f"raw=0x{result.eax & 0xFFFFFFFF:08X} base=0x{base:08X} size=0x{size:X}"
            )
        self.counts[va] += 1

    @staticmethod
    def _diff_bytes(va: int, label: str, game: str, cpp: str) -> None:
        game_b = bytes.fromhex(game) if game else b""
        cpp_b = bytes.fromhex(cpp) if cpp else b""
        limit = min(len(game_b), len(cpp_b))
        for index in range(limit):
            if game_b[index] != cpp_b[index]:
                start = max(0, index - 8)
                raise AssertionError(
                    f"0x{va:08X}: {label} mismatch at +0x{index:X} "
                    f"game={game_b[start:index+8].hex()} cpp={cpp_b[start:index+8].hex()}"
                )
        raise AssertionError(
            f"0x{va:08X}: {label} length game={len(game_b)} cpp={len(cpp_b)}"
        )


def zeros(size: int) -> bytearray:
    return bytearray(size)


def verify(executable: Path) -> None:
    oracle = Oracle(executable)
    try:
        _cases(oracle)
    finally:
        oracle.close()
    missing = [f"0x{va:08X}={n}" for va, n in oracle.counts.items() if n < 3]
    if missing:
        raise AssertionError("fewer than 3 cases: " + ", ".join(missing))
    print(f"audio50 oracle mismatches=0 cases={sum(oracle.counts.values())}")


def _cases(o: Oracle) -> None:
    # 00502270 speaker mode.
    for mode, _name in ((0, "zero"), (1, "one"), (2, "other"), (0x101, "high")):
        o.invoke(0x00502270, zeros(0), [], patches=f"0x00CA7E7C={le(mode)}")
    # Channel sits 0x100 past the system so the preserved system high bits
    # differ from the channel pointer's high bits.
    system = zeros(0x300)
    put(system, 0x2E4, struct.pack("<I", 0))
    put(system, 0x54, struct.pack("<I", 0x100))
    system[0x129] = 0xAB
    o.invoke(
        0x00502270, system, [], fixups="0x54,g0x00DF6098@0",
        patches="0x00CA7E7C=" + le(0xFFFFFFFF),
    )
    system[0x129] = 0x00
    o.invoke(
        0x00502270, system, [], fixups="0x54,g0x00DF6098@0",
        patches="0x00CA7E7C=" + le(0xFFFFFFFF),
    )

    # 00502540 rejects before the external name call.
    provider = zeros(0x20)
    o.invoke(0x00502540, provider, [NULL], pointer_args=(0,))
    put(provider, 4, struct.pack("<I", 1))
    o.invoke(0x00502540, provider, [NULL], pointer_args=(0,))
    put(provider, 0x0C, struct.pack("<I", 1))
    put(provider, 0, struct.pack("<I", 0))
    o.invoke(0x00502540, provider, [0x10], pointer_args=(0,))
    put(provider, 0, struct.pack("<I", 1))
    o.invoke(0x00502540, provider, [NULL], pointer_args=(0,))

    for eax in (0, 0x11111111, 0xA5A5A5A5):
        o.invoke(0x00502FB0, zeros(0), [], eax=eax)
        o.invoke(0x00503300, zeros(0), [], eax=eax)
        o.invoke(0x00503400, zeros(0), [], eax=eax)

    manager = zeros(0x20)
    o.invoke(0x00502FF0, manager, [], fixups="t@0")
    o.invoke(0x00502FF0, manager, [], fixups="t@0", eax=0)
    o.invoke(0x00502FF0, manager, [], fixups="t@0", eax=0x99)

    for count in (0, 1, 7, 0x100):
        o.invoke(0x00503290, zeros(0), [], patches=f"0x00CA7D7C={le(count)}")

    # 005044F0 copies the dword at the slot (caller passes &event) into the handle.
    slot = zeros(0xC0)
    o.invoke(0x005044F0, slot, [0], self_off="0x10", pointer_args=(0,))
    put(slot, 0, struct.pack("<I", 0x20))
    put(slot, 0xB8, struct.pack("<I", 4))
    o.invoke(0x005044F0, slot, [0], self_off="0x10", fixups="0", pointer_args=(0,))
    put(slot, 0xB8, struct.pack("<I", 0))
    o.invoke(0x005044F0, slot, [0], self_off="0x10", fixups="0", pointer_args=(0,))

    # 00504560
    obj = zeros(0xB0)
    o.invoke(0x00504560, obj, [])
    put(obj, 0, struct.pack("<I", 0x10))
    put(obj, 0x10 + 0x98, struct.pack("<I", 2))
    o.invoke(0x00504560, obj, [], fixups="0")
    put(obj, 0x10 + 0x98, struct.pack("<I", 1))
    put(obj, 0x10 + 0x94, struct.pack("<I", 0))
    o.invoke(0x00504560, obj, [], fixups="0,t@0xA4")
    put(obj, 0x10 + 0x98, struct.pack("<i", 0))
    o.invoke(0x00504560, obj, [], fixups="0,t@0xA4")

    # 005045C0. The handle's first dword is the object.
    play = zeros(0x20)
    o.invoke(0x005045C0, play, [], eax=0xA5A5A5A5)
    put(play, 0, struct.pack("<I", 0x10))
    put(play, 0x14, struct.pack("<I", 3))
    o.invoke(0x005045C0, play, [], eax=0xA5A5A5A5, fixups="0,t@0x10")
    put(play, 0x14, struct.pack("<I", 1))
    o.invoke(0x005045C0, play, [], eax=0xA5A5A5A5, fixups="0,t@0x10")
    o.invoke(0x005045C0, play, [], eax=0, fixups="0,t@0x10")

    # 005045E0 node at 0, prev at 0x20, next at 0x40, owner at 0x60.
    def node_blob(prev: int | None, nxt: int | None) -> bytearray:
        raw = zeros(0x140)
        put(raw, 0, struct.pack("<I", 0x60))
        put(raw, 4, struct.pack("<I", 0 if prev is None else 0x20))
        put(raw, 8, struct.pack("<I", 0 if nxt is None else 0x40))
        return raw

    o.invoke(0x005045E0, node_blob(0x20, 0x40), [], fixups="0,4,8", compare="ptr")
    o.invoke(0x005045E0, node_blob(None, 0x40), [], fixups="0,8", compare="ptr")
    o.invoke(0x005045E0, node_blob(0x20, None), [], fixups="0,4", compare="ptr")
    o.invoke(0x005045E0, node_blob(None, None), [], fixups="0", compare="ptr")

    # 00504AC0 / 00504AF0
    info = zeros(0x90)
    put(info, 0x78, struct.pack("<I", 0x80000001))
    put(info, 0x7C, struct.pack("<I", 0x2))
    for bit in (0, 1, 31, 32, 33, 63):
        o.invoke(0x00504AC0, info, [bit])
    mask = zeros(8)
    put(mask, 0, struct.pack("<I", 0x80000001))
    both = info + mask
    o.invoke(0x00504AF0, both, [0x90], pointer_args=(0,))
    put(mask, 0, struct.pack("<I", 0))
    both = info + mask
    o.invoke(0x00504AF0, both, [0x90], pointer_args=(0,))
    put(mask, 0, struct.pack("<I", 0x2))
    both = info + mask
    o.invoke(0x00504AF0, both, [0x90], pointer_args=(0,))

    # 00504C80 volume. Player is the blob.
    def volume_player() -> bytearray:
        raw = zeros(0x200)
        put(raw, 4, struct.pack("<f", 2.0))
        put(raw, 8, struct.pack("<f", 4.0))
        put(raw, 0x34, struct.pack("<f", 0.5))
        put(raw, 0xAC, struct.pack("<f", 0.25))
        put(raw, 0xB0, struct.pack("<f", 0.5))
        put(raw, 0xB4, struct.pack("<f", 0.5))
        return raw

    o.invoke(0x00504C80, volume_player(), [5, 0], compare="none")
    o.invoke(0x00504C80, volume_player(), [5, 1], compare="none")
    o.invoke(0x00504C80, volume_player(), [0, 0], compare="none")
    settings = zeros(0x80)
    put(settings, 0x74, struct.pack("<f", 0.5))
    put(settings, 0x78, struct.pack("<f", 2.0))
    o.invoke(
        0x00504C80, volume_player() + settings, [0, 1],
        fixups="g0x00DF03F8@0x200", compare="none",
    )
    o.invoke(
        0x00504C80, volume_player(), [0, 0], compare="none",
        patches="0x00CA9F04=" + fhex(2.0) + ",0x00BD90A4=" + fhex(0.25),
    )
    nan_player = volume_player()
    put(nan_player, 0xF0, struct.pack("<I", 0x7FC00001))
    o.invoke(0x00504C80, nan_player, [5, 0], compare="none")

    # 00504DF0 product.
    def product_case(record: bytes, lock: int, with_settings: bool) -> None:
        raw = zeros(0x240)
        put(raw, 0x4C, struct.pack("<I", 0x40))
        put(raw, 0x50, struct.pack("<I", 0x40 + len(record)))
        if record:
            put(raw, 0x40, record)
        fix = "0x4C,0x50"
        if with_settings:
            put(raw, 0x100 + 0xD0, struct.pack("<I", 2))
            put(raw, 0x100 + 0xD4, struct.pack("<I", 0x200))
            put(raw, 0x200, struct.pack("<II", 1, struct.unpack("<I", struct.pack("<f", 0.5))[0]))
            put(raw, 0x208, struct.pack("<II", 0, struct.unpack("<I", struct.pack("<f", 0.25))[0]))
            fix += ",0x1D4,g0x00DF03F8@0x100"
        o.invoke(
            0x00504DF0, raw, [0], fixups=fix, compare="ptr",
            patches=f"0x00DEE1C8={le(lock)}",
        )

    product_case(b"", 0, False)
    product_case(struct.pack("<fI", 0.5, 0) + struct.pack("<fI", 0.5, 0), 0, False)
    product_case(struct.pack("<fI", -2.0, 0), 0, False)
    product_case(struct.pack("<fI", 4.0, 0), 0, False)
    product_case(struct.pack("<II", 0x7FC00001, 0), 0, False)
    product_case(struct.pack("<fI", 0.5, 0), 1, True)
    product_case(struct.pack("<fI", 0.5, 0), 1, False)
    o.invoke(
        0x00504DF0, zeros(0x80), [0], compare="ptr",
        patches="0x00BD90A4=" + fhex(0.25),
    )

    # 00504EC0 counters.
    counters = zeros(0x200)
    for index in range(0x1B0, 0x1E0):
        counters[index] = (index * 17) & 0xFF
    o.invoke(0x00504EC0, counters, [], compare="ptr")
    counters = zeros(0x200)
    o.invoke(0x00504EC0, counters, [], compare="ptr")
    counters = zeros(0x200)
    for index in range(0x1B0, 0x1E0):
        counters[index] = 0x80 if index % 2 == 0 else 0x01
    o.invoke(0x00504EC0, counters, [], compare="ptr")

    # 00505340 / 005053B0 and the vtable wrappers.
    guard = zeros(0x20)
    put(guard, 0x18, struct.pack("<I", 1))
    o.invoke(0x00505340, guard, [0], pointer_args=(0,), compare="ptr")
    o.invoke(0x005053B0, guard, [0], pointer_args=(0,), compare="ptr")
    o.invoke(0x00506670, guard, [0], pointer_args=(0,), compare="ptr")
    o.invoke(0x00506680, guard, [0], pointer_args=(0,), compare="ptr")
    o.invoke(0x00505340, zeros(4), [NULL], pointer_args=(0,), patches="0x00DEE1C8=" + le(4))
    o.invoke(0x00505340, zeros(4), [NULL], pointer_args=(0,), compare="none")
    o.invoke(0x00506670, zeros(4), [NULL], pointer_args=(0,), patches="0x00DEE1C8=" + le(2))
    o.invoke(0x005053B0, zeros(4), [NULL], pointer_args=(0,), patches="0x00DEE1C8=" + le(3))
    o.invoke(0x005053B0, zeros(4), [NULL], pointer_args=(0,), patches="0x00DEE1C8=" + le(0))
    o.invoke(0x005053B0, zeros(4), [NULL], pointer_args=(0,), patches="0x00DEE1C8=" + le(1), compare="none")
    o.invoke(0x00506680, zeros(4), [NULL], pointer_args=(0,), patches="0x00DEE1C8=" + le(5))
    o.invoke(0x00506680, zeros(4), [NULL], pointer_args=(0,), patches="0x00DEE1C8=" + le(0))
    empty_guard = zeros(0x20)
    o.invoke(0x00505340, empty_guard, [0], pointer_args=(0,), patches="0x00DEE1C8=" + le(8))
    o.invoke(0x00506670, empty_guard, [0], pointer_args=(0,), compare="none")

    # 00506430 early returns only.
    request = zeros(0xA0)
    put(request, 0x30, struct.pack("<I", 1))
    o.invoke(0x00506430, request, [0], eax=0xA5A5A511, pointer_args=(0,))
    o.invoke(0x00506430, request, [0], eax=0x10, pointer_args=(0,))
    put(request, 0x30, struct.pack("<I", 0))
    put(request, 0x0C, struct.pack("<I", 0x20))
    put(request, 0x20 + 0x74, struct.pack("<I", 0))
    o.invoke(0x00506430, request, [0], eax=0xA5A5A5A5, pointer_args=(0,), fixups="0x0C")
    put(request, 0x20 + 0x74, struct.pack("<I", 0x2))
    o.invoke(0x00506430, request, [0], pointer_args=(0,), fixups="0x0C")

    # 00506C20 / 00506C90. Pre-fill so uncleared words stay visible.
    bits = bytearray(b"\xFF" * 16)
    o.invoke(0x00506C20, bits, [0, 1, 31, 32], compare="ptr")
    o.invoke(0x00506C20, bytearray(b"\xFF" * 16), [5, 5, 5, 5], compare="ptr")
    o.invoke(0x00506C20, bytearray(b"\xFF" * 16), [63, 0, 33, 1], compare="ptr")
    o.invoke(0x00506C90, bytearray(b"\xFF" * 16), [0, 31], compare="ptr")
    o.invoke(0x00506C90, bytearray(b"\xFF" * 16), [32, 33], compare="ptr")
    o.invoke(0x00506C90, bytearray(b"\xFF" * 16), [4, 4], compare="ptr")

    # 00507720. Object at 0x10, inner object at 0x40.
    cursor = zeros(0x80)
    o.invoke(0x00507720, cursor, [])
    put(cursor, 0, struct.pack("<I", 0x10))
    put(cursor, 0x20, struct.pack("<I", 0x40))
    put(cursor, 0x60, struct.pack("<I", 0x70))
    o.invoke(0x00507720, cursor, [], fixups="0,0x20,0x60")
    put(cursor, 0x60, struct.pack("<I", 0))
    put(cursor, 0x2C, struct.pack("<I", 0x70))
    o.invoke(0x00507720, cursor, [], fixups="0,0x20,0x2C")
    put(cursor, 0x2C, struct.pack("<I", 0))
    o.invoke(0x00507720, cursor, [], fixups="0,0x20")

    # 00508640
    node = zeros(0x20)
    o.invoke(0x00508640, node, [NULL], pointer_args=(0,), compare="ptr")
    owner = zeros(0xE0)
    blob = node + owner
    o.invoke(0x00508640, blob, [0x20], pointer_args=(0,), compare="ptr")
    put(owner, 0xC4, struct.pack("<I", 0x100))
    prev = zeros(0x20)
    blob = node + owner + prev
    o.invoke(0x00508640, blob, [0x20], fixups="0xE4", pointer_args=(0,), compare="ptr")

    # 0050B090
    manager = zeros(0x60)
    stream = zeros(0x20)
    put(stream, 4, struct.pack("<I", 4))
    o.invoke(0x0050B090, manager + stream, [0x60], fixups="t@0x60", pointer_args=(0,))
    put(stream, 4, struct.pack("<I", 0x14))
    o.invoke(0x0050B090, manager + stream, [0x60], fixups="t@0x60", pointer_args=(0,))
    put(stream, 4, struct.pack("<I", 0))
    o.invoke(0x0050B090, manager + stream, [0x60], fixups="t@0x60", pointer_args=(0,))
    put(stream, 4, struct.pack("<I", 8))
    o.invoke(0x0050B090, manager + stream, [0x60], fixups="t@0x60", pointer_args=(0,))

    # 0050DF80
    def cache_with(keys: list[int], target: int) -> None:
        raw = zeros(0x90 + 8 * max(len(keys), 1))
        begin = 0x90
        end = 0x90 + 8 * len(keys)
        put(raw, 0x60, struct.pack("<I", begin))
        put(raw, 0x64, struct.pack("<I", end))
        for index, key in enumerate(keys):
            put(raw, begin + index * 8, struct.pack("<II", key, 0x1000 + index))
        o.invoke(0x0050DF80, raw, [target], fixups="0x60,0x64", compare="ptr")

    cache_with([], 1)
    cache_with([1, 2, 3], 9)
    cache_with([1, 2, 3], 1)
    cache_with([1, 2, 3], 2)
    cache_with([1, 2, 3], 3)

    manager = zeros(0x50)
    o.invoke(0x0050E510, manager, [1], eax=0xA5A5A5A5)
    o.invoke(0x0050E510, manager, [1], eax=0)
    cache = zeros(0xB0)
    put(cache, 0x60, struct.pack("<I", 0xE0))
    put(cache, 0x64, struct.pack("<I", 0xF0))
    put(cache, 0x90, struct.pack("<II", 4, 1))
    put(cache, 0x98, struct.pack("<II", 5, 2))
    blob = manager + cache
    put(blob, 0x40, struct.pack("<I", 0x50))
    o.invoke(0x0050E510, blob, [4], fixups="0x40,0xB0,0xB4", eax=0x11, compare="ptr")
    o.invoke(0x0050E510, blob, [9], fixups="0x40,0xB0,0xB4", compare="ptr")

    # 0050FA80
    def listed(records: list[tuple[int, float]]) -> bytearray:
        raw = zeros(0x260)
        put(raw, 0x4C, struct.pack("<I", 0x80))
        put(raw, 0x50, struct.pack("<I", 0x80))
        put(raw, 0x54, struct.pack("<I", 0xA0))
        put(raw, 0x34, struct.pack("<f", 1.0))
        put(raw, 0xB0, struct.pack("<f", 1.0))
        put(raw, 0xB4, struct.pack("<f", 1.0))
        put(raw, 0xAC, struct.pack("<f", 1.0))
        put(raw, 4, struct.pack("<f", 1.0))
        put(raw, 8, struct.pack("<f", 1.0))
        put(raw, 0x200, struct.pack("<I", 0x100))
        begin = 0x220
        end = 0x220 + 8 * len(records)
        put(raw, 0x100 + 0xF0, struct.pack("<I", begin))
        put(raw, 0x100 + 0xF4, struct.pack("<I", end))
        for index, (category, sample) in enumerate(records):
            put(raw, begin + index * 8, struct.pack("<if", category, sample))
        return raw

    fix = "0x4C,0x50,0x54,0x200,0x1F0,0x1F4"
    o.invoke(0x0050FA80, listed([]), [0x200, 3], fixups=fix, pointer_args=(0,), compare="ptr")
    o.invoke(
        0x0050FA80, listed([(-1, 0.0)]), [0x200, 3],
        fixups=fix, pointer_args=(0,), compare="ptr",
    )
    o.invoke(
        0x0050FA80, listed([(0, 0.5), (-1, 1.0), (0, 0.25)]), [0x200, 7],
        fixups=fix, pointer_args=(0,), compare="ptr",
    )

    # 0050FD50. Destination vectors live at the absolute player addresses.
    parts = []
    for index in range(3):
        base = 0x00DEE290 + index * 0x1FC
        begin = base + 0x80
        parts.append(f"0x{base + 0x4C:08X}={le(begin)}")
        parts.append(f"0x{base + 0x50:08X}={le(begin)}")
        parts.append(f"0x{base + 0x54:08X}={le(begin + 0x20)}")
        parts.append(f"0x{base + 4:08X}={fhex(1.0)}")
        parts.append(f"0x{base + 8:08X}={fhex(1.0)}")
        parts.append(f"0x{base + 0x34:08X}={fhex(1.0)}")
        parts.append(f"0x{base + 0xAC:08X}={fhex(1.0)}")
        parts.append(f"0x{base + 0xB0:08X}={fhex(1.0)}")
        parts.append(f"0x{base + 0xB4:08X}={fhex(1.0)}")
    player_patches = ",".join(parts)

    def rendering(index: int, records: list[tuple[int, float]]) -> bytearray:
        raw = zeros(0x220)
        put(raw, 4, struct.pack("<I", 0x40))
        put(raw, 0x40 + 0x30, struct.pack("<I", index))
        put(raw, 0x40 + 0x0C, struct.pack("<I", 0x80))
        put(raw, 0x80, struct.pack("<I", 0xC0))
        begin = 0x1C0
        end = begin + 8 * len(records)
        put(raw, 0xC0 + 0xF0, struct.pack("<I", begin))
        put(raw, 0xC0 + 0xF4, struct.pack("<I", end))
        for slot, (category, sample) in enumerate(records):
            put(raw, begin + slot * 8, struct.pack("<if", category, sample))
        return raw

    render_fix = "0x4,0x4C,0x80,0x1B0,0x1B4"
    o.invoke(
        0x0050FD50, rendering(0, []), [], fixups=render_fix, patches=player_patches, compare="ptr",
    )
    o.invoke(
        0x0050FD50, rendering(1, [(0, 0.5), (-1, 0.0)]), [],
        fixups=render_fix, patches=player_patches, compare="ptr",
    )
    o.invoke(
        0x0050FD50, rendering(2, [(0, 0.25)]), [],
        fixups=render_fix, patches=player_patches, compare="ptr",
    )
    o.invoke(
        0x0050FD50, rendering(2, []), [], fixups=render_fix, patches=player_patches, compare="ptr",
    )


def main() -> None:
    if len(sys.argv) != 2:
        raise SystemExit("usage: verify_audio50.py <ra3_audio50check.exe>")
    verify(Path(sys.argv[1]))


if __name__ == "__main__":
    main()
