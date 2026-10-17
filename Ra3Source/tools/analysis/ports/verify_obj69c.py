"""Differentially verify the third 0x0069 update cluster against ra3_1.0.game."""

from __future__ import annotations

import struct
import subprocess
import sys
from pathlib import Path

from unicorn import x86_const

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from emulate import Emulator  # noqa: E402

ACCEPTED_ENTRIES = (
    0x00699C10, 0x00699C60, 0x00699CD0, 0x00699D20, 0x00699D60, 0x00699DC0,
    0x00699E20, 0x00699E80, 0x00699EE0, 0x00699F60, 0x006991B0, 0x00695990,
    0x00696430, 0x006906E0, 0x00690A80, 0x00692BA0, 0x00690240, 0x00690270,
    0x00693FB0, 0x006975C0, 0x006908E0, 0x0069FF20, 0x00696BC0, 0x0069F100,
    0x00696740, 0x00696800, 0x00697630, 0x006920B0, 0x00692460, 0x00692490,
    0x0069E660, 0x0069FA90, 0x00695720, 0x006953E0, 0x00695210, 0x00695170,
    0x006950D0, 0x00695020, 0x00697B80, 0x006913D0, 0x00691620,
)

DIRECTS = (
    (0x005AC4F0, 8, False), (0x005AA2A0, 4, False), (0x005BDCC0, 4, False),
    (0x00692390, 12, False), (0x00799580, 0, False), (0x007995C0, 8, False),
    (0x00844460, 8, False), (0x004ED9C0, 12, False), (0x00795CF0, 4, True),
    (0x007886E0, 0, False), (0x00A7C770, 0, False), (0x00796EA0, 4, False),
    (0x007E4AB0, 28, False), (0x007EDDB0, 8, False), (0x007EDF50, 12, False),
    (0x007F58B0, 0, False), (0x0086C630, 0, False),
    (0x0067F7C0, 4, False), (0x004A62D0, 0, False), (0x007E3720, 12, False),
    (0x004D5900, 0, False), (0x0078B960, 8, False), (0x007E4990, 0, False),
    (0x0067D3C0, 0, False), (0x004D3C00, 0, False), (0x007EE070, 4, False),
    (0x007E2970, 0, False), (0x00687720, 4, True), (0x0069F9E0, 4, False),
    (0x0078D880, 4, False), (0x00B11D40, 0, False), (0x0078C080, 4, False),
    (0x0063EB60, 0, False), (0x008D82F0, 4, False), (0x00685080, 4, False),
    (0x006858E0, 4, False), (0x00980820, 0, False), (0x0054A090, 0, False),
)


def u32(data: bytes, offset: int) -> int:
    return struct.unpack_from("<I", data, offset)[0]


def put_u32(data: bytearray, offset: int, value: int) -> None:
    struct.pack_into("<I", data, offset, value & 0xFFFFFFFF)


def put_f32(data: bytearray, offset: int, value: float) -> None:
    struct.pack_into("<f", data, offset, value)


def fill(size: int) -> bytearray:
    return bytearray(size)


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
             a0: int, a1: int, a2: int, a3: int, stubs: str) -> tuple[int, bytes]:
        assert self.process.stdin is not None and self.process.stdout is not None
        fix_str = ",".join(fixups) if fixups else "-"
        line = (
            f"0x{va:08X} 0x{self_offset:X} {blob.hex()} {fix_str} "
            f"0x{a0 & 0xFFFFFFFF:X} 0x{a1 & 0xFFFFFFFF:X} 0x{a2 & 0xFFFFFFFF:X} "
            f"0x{a3 & 0xFFFFFFFF:X} 0x037F {stubs}\n"
        )
        self.process.stdin.write(line)
        self.process.stdin.flush()
        out = self.process.stdout.readline().strip()
        if not out.startswith("ret=0x"):
            raise RuntimeError(f"ra3_obj69ccheck failed on 0x{va:08X}: {out!r}")
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
        self.pointer_targets: set[int] = set()
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

        for kind in range(5):
            self.stubs[kind] = self.emu.bind_stub(make_stub(kind, 8, 0x0C), kind * 4)
        self.stubs[5] = self.emu.bind_stub(make_stub(1, 0x10, 0x0C), 4)

        def stub6(emu: Emulator) -> int:
            self_ptr = emu.register(x86_const.UC_X86_REG_ECX)
            result = u32(emu.read(self_ptr + 0x20, 4), 0)
            for index in range(4):
                arg = emu.stack_arg(index)
                emu.write(self_ptr + 0x40 + 4 * index, struct.pack("<I", arg & 0xFFFFFFFF))
            count = u32(emu.read(self_ptr + 0x0C, 4), 0)
            emu.write(self_ptr + 0x0C, struct.pack("<I", (count + 1) & 0xFFFFFFFF))
            return result

        self.stubs[6] = self.emu.bind_stub(stub6, 16)

        for va, cleanup, pointer in DIRECTS:
            if pointer:
                self.pointer_targets.add(va)

            def direct(emu: Emulator, target: int = va, is_pointer: bool = pointer) -> int:
                queued = self.queues.get(target)
                value = queued.pop(0) if queued else 0
                if not is_pointer or value == 0:
                    return value & 0xFFFFFFFF
                return (self.base + value) & 0xFFFFFFFF
            self.emu.bind_function(va, direct, cleanup)

    def _load_queues(self, stubs: str) -> None:
        self.queues = {}
        self.pointer_targets = {va for va, _cleanup, pointer in DIRECTS if pointer}
        if stubs in ("", "-"):
            return
        for item in stubs.split(","):
            if len(item) < 3 or item[1] != ":" or item[0] in "gnGN":
                continue
            target_s, values_s = item[2:].split("=", 1)
            target = int(target_s, 16)
            self.queues[target] = [int(part, 16) for part in values_s.split("|")]
            if item[0] in "pP":
                self.pointer_targets.add(target)
            elif item[0] in "rR":
                self.pointer_targets.discard(target)

    def _apply_plants(self, stubs: str, base: int) -> None:
        if stubs in ("", "-"):
            return
        for item in stubs.split(","):
            if item.startswith(("g:", "G:")):
                addr_s, off_s = item[2:].split("=", 1)
                self.emu.write(int(addr_s, 16), struct.pack("<I", (base + int(off_s, 16)) & 0xFFFFFFFF))
            elif item.startswith(("n:", "N:")):
                addr_s, val_s = item[2:].split("=", 1)
                self.emu.write(int(addr_s, 16), struct.pack("<I", int(val_s, 16) & 0xFFFFFFFF))

    def check(self, va: int, self_offset: int, blob: bytes, fixups: list[str],
              invoke, cleanup: int, *, a0: int, a1: int, a2: int, a3: int, stubs: str,
              compare_ret: str) -> None:
        self._load_queues(stubs)
        # Rebuild binds so pointer-ness follows this case's r:/p: tokens.
        self._rebind_pointers()
        cpp_ret, cpp_blob = self.cpp.call(va, self_offset, bytes(blob), fixups, a0, a1, a2, a3, stubs)
        self._load_queues(stubs)
        self.emu.reset_data()
        self.emu.uc.reg_write(x86_const.UC_X86_REG_FPCW, 0x037F)
        base = self.emu.alloc(bytes(blob))
        self.base = base
        self._apply_plants(stubs, base)
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

    def _rebind_pointers(self) -> None:
        # Pointer adjustment is captured per direct() closure from DIRECTS.
        # r: tokens for a pointer-typed callee must stay zero; non-zero pointer
        # results use p: and the DIRECTS flag. Nothing to rebuild.
        return

    def close(self) -> None:
        self.cpp.close()


def verify(executable: Path) -> None:
    oracle = Verifier(executable)

    def run(va: int, blob: bytearray, stack, cleanup: int, *, self_off: int = 0,
            a0: int = 0, a1: int = 0, a2: int = 0, a3: int = 0,
            fixups: list[str] | None = None, stubs: str = "-", compare: str = "none",
            float_ret: bool = False, tag: str = "") -> None:
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
                         a0=a0, a1=a1, a2=a2, a3=a3, stubs=stubs, compare_ret=compare)
        except AssertionError as exc:
            raise AssertionError(f"{tag} {exc}") from None

    def ai_fix(state: int = 0x100, vt: int = 0x300) -> list[str]:
        return [f"0x3C", f"0x{state:X}", f"v0@0x{vt + 0x34:X}", f"v1@0x{vt + 0x38:X}", f"v1@0x{vt + 0x1C:X}"]

    def ai_blob() -> bytearray:
        blob = fill(0x600)
        put_u32(blob, 0x3C, 0x100)
        put_u32(blob, 0x100, 0x300)
        return blob

    # AIUpdateInterface issue helpers.
    run(0x00699C10, ai_blob(), [0x11, 0x22, 0x33], 0xC, a0=0x11, a1=0x22, a2=0x33,
        fixups=ai_fix(), tag="issue61")
    run(0x00699C10, ai_blob(), [0x90, 0x91, 0x92], 0xC, a0=0x90, a1=0x91, a2=0x92,
        fixups=ai_fix(), tag="issue61-b")
    run(0x00699C60, ai_blob(), [0x44, 0x55], 8, a0=0x44, a1=0x55, fixups=ai_fix(), tag="issue51")
    run(0x00699C60, ai_blob(), [0x1, 0x2], 8, a0=0x1, a1=0x2, fixups=ai_fix(), tag="issue51-b")
    run(0x00699D20, ai_blob(), [0x66, 0x77], 8, a0=0x66, a1=0x77, fixups=ai_fix(), tag="issue32")
    run(0x00699D20, ai_blob(), [0x3, 0x4], 8, a0=0x3, a1=0x4, fixups=ai_fix(), tag="issue32-b")
    blocked = ai_blob()
    run(0x00699CD0, blocked, [0x10, 0x20], 8, a0=0x10, a1=0x20, fixups=ai_fix(),
        stubs="r:00692390=0", tag="issue3A-block")
    allowed = ai_blob()
    run(0x00699CD0, allowed, [0x10, 0x20], 8, a0=0x10, a1=0x20, fixups=ai_fix(),
        stubs="r:00692390=1", tag="issue3A-go")
    run(0x00699D60, ai_blob(), [1, 2, 3], 0xC, a0=1, a1=2, a2=3, fixups=ai_fix(), tag="issue40")
    run(0x00699D60, ai_blob(), [8, 7, 6], 0xC, a0=8, a1=7, a2=6, fixups=ai_fix(), tag="issue40-b")
    run(0x00699DC0, ai_blob(), [4, 5, 6], 0xC, a0=4, a1=5, a2=6, fixups=ai_fix(), tag="issue42")
    run(0x00699DC0, ai_blob(), [1, 0, 2], 0xC, a0=1, a1=0, a2=2, fixups=ai_fix(), tag="issue42-b")
    run(0x00699E20, ai_blob(), [7, 8, 9], 0xC, a0=7, a1=8, a2=9, fixups=ai_fix(), tag="issue3F")
    run(0x00699E20, ai_blob(), [2, 2, 2], 0xC, a0=2, a1=2, a2=2, fixups=ai_fix(), tag="issue3F-b")
    run(0x00699E80, ai_blob(), [3, 2, 1], 0xC, a0=3, a1=2, a2=1, fixups=ai_fix(), tag="issue43")
    run(0x00699E80, ai_blob(), [9, 8, 7], 0xC, a0=9, a1=8, a2=7, fixups=ai_fix(), tag="issue43-b")
    issue49 = ai_blob()
    put_u32(issue49, 8, 0x180)
    run(0x00699EE0, issue49, [9, 8, 7], 0xC, a0=9, a1=8, a2=7,
        fixups=ai_fix() + ["0x8"], tag="issue49-empty")
    issue49b = ai_blob()
    put_u32(issue49b, 8, 0x180)
    put_u32(issue49b, 0x180 + 0x418, 0x1C0)
    run(0x00699EE0, issue49b, [9, 8, 7], 0xC, a0=9, a1=8, a2=7,
        fixups=ai_fix() + ["0x8", "0x598"], tag="issue49-contained")
    quiet = ai_blob()
    run(0x00699F60, quiet, [0, 0x99], 8, a0=0, a1=0x99, fixups=ai_fix(), tag="issue2A-clear")
    aimed = ai_blob()
    run(0x00699F60, aimed, [0x42, 0x99], 8, a0=0x42, a1=0x99, fixups=ai_fix(), tag="issue2A-aim")

    touch = fill(0x480)
    put_u32(touch, 0x14, 5)
    put_u32(touch, 0x100 + 0x58, 10)
    run(0x006991B0, touch, [], 0, stubs="g:00DEBA64=100", tag="touch-early")
    touch2 = fill(0x480)
    put_u32(touch2, 0x14, 11)
    put_u32(touch2, 0x100 + 0x58, 10)
    run(0x006991B0, touch2, [], 0, stubs="g:00DEBA64=100", tag="touch-one")
    touch3 = fill(0x480)
    put_u32(touch3, 0x14, 30)
    touch3[0x43B] = 1
    put_u32(touch3, 0x100 + 0x58, 10)
    run(0x006991B0, touch3, [], 0, stubs="g:00DEBA64=100", tag="touch-busy")
    touch4 = fill(0x480)
    put_u32(touch4, 0x14, 30)
    put_u32(touch4, 8, 0x21)
    put_u32(touch4, 0x100 + 0x58, 10)
    run(0x006991B0, touch4, [], 0, stubs="g:00DEBA64=100", tag="touch-arm")

    # SpecialPowerModule.
    ready = fill(0x80)
    put_u32(ready, 0x10, 0x40)
    put_u32(ready, 0x18, 1)
    run(0x00695990, ready, [], 0, fixups=["0x10", "v0@0x50"], tag="note-active")
    held = fill(0x80)
    put_u32(held, 0x10, 0x40)
    held[0x40] = 1
    run(0x00695990, held, [], 0, fixups=["0x10", "v0@0x50"], tag="note-held")
    flagged = fill(0x120)
    put_u32(flagged, 0x10, 0x40)
    put_u32(flagged, 8, 0x60)
    put_u32(flagged, 0x60 + 0x88, 1 << 9)
    run(0x00695990, flagged, [], 0, fixups=["0x10", "v0@0x50", "0x8"], tag="note-flag")

    def trigger_blob(bits: int, body_byte: int, owner: int) -> tuple[bytearray, list[str]]:
        blob = fill(0x400)
        put_u32(blob, 0x10, 0x300)
        put_u32(blob, 0x18, 0x100)
        put_u32(blob, 0x100 + 8, 0x140)
        put_u32(blob, 0x140 + 0x1C, bits)
        put_u32(blob, 4, 0x180)
        blob[0x180 + 0x1B9] = body_byte
        if owner:
            put_u32(blob, 8, owner)
            put_u32(blob, owner + 4, 0x1A0)
            put_u32(blob, 0x1A0 + 0xCC, 1 << 25)
        fix = ["0x10", "0x18", "0x108", "0x4", "v0@0x31C", "v1@0x344"]
        if owner:
            fix += ["0x8", "0x184"]
        return blob, fix

    blob, fix = trigger_blob(0, 0, 0)
    run(0x00696430, blob, [0x1, 0x2], 8, a0=1, a1=2, fixups=fix, tag="trigger-scale")
    blob, fix = trigger_blob(0, 1, 0)
    run(0x00696430, blob, [0x1, 0x2], 8, a0=1, a1=2, fixups=fix, tag="trigger-skip")
    blob, fix = trigger_blob(1 << 14, 0, 0)
    run(0x00696430, blob, [0x1, 0x2], 8, a0=1, a1=2, fixups=fix, tag="trigger-direct")
    blob, fix = trigger_blob(1 << 14, 1, 0)
    put_u32(blob, 0x220 + 0x58, 40)
    run(0x00696430, blob, [0x1, 0x2], 8, a0=1, a1=2, fixups=fix,
        stubs="g:00DEBA64=220", tag="trigger-frame")

    cached = fill(0x140)
    cached[0x11C] = 1
    run(0x006906E0, cached, [], 0, tag="manager-set")
    missing = fill(0x140)
    put_u32(missing, 8, 0x80)
    run(0x006906E0, missing, [], 0, fixups=["0x8"], stubs="p:00795CF0=0", tag="manager-none")
    fresh = fill(0x160)
    put_u32(fresh, 8, 0x80)
    put_u32(fresh, 0x80 + 4, 0x90)
    run(0x006906E0, fresh, [], 0, fixups=["0x8", "0x84"],
        stubs="p:00795CF0=100,r:007886E0=11,r:00A7C770=22", tag="manager-fill")
    same = fill(0x160)
    put_u32(same, 8, 0x80)
    put_u32(same, 0x114, 0x100)
    run(0x006906E0, same, [], 0, fixups=["0x8"], stubs="p:00795CF0=100", tag="manager-same")

    money = fill(0x500)
    put_u32(money, 4, 0x40)
    run(0x00690A80, money, lambda b: [b + 0x80], 4, a0=0x80, fixups=["0x4"], compare="u8", tag="money-empty")
    granted = fill(0x600)
    put_u32(granted, 4, 0x40)
    put_u32(granted, 8, 0x77)
    put_u32(granted, 0x40 + 0x6C, 0x1234)
    put_u32(granted, 0x100 + 0x418, 0x180)
    put_u32(granted, 0x100 + 0xDC, 9)
    put_u32(granted, 0x180 + 0x1F0, 0x200)
    run(0x00690A80, granted, lambda b: [b + 0x100], 4, a0=0x100,
        fixups=["0x4", "0x518", "0x370"], stubs="r:00796EA0=8,r:007E4AB0=0,r:007F58B0=0",
        compare="u8", tag="money-quiet")
    live = fill(0x600)
    put_u32(live, 4, 0x40)
    put_u32(live, 8, 0x77)
    put_u32(live, 0x40 + 0x6C, 3)
    put_u32(live, 0x100 + 0x418, 0x180)
    put_u32(live, 0x180 + 0x1F0, 0x200)
    run(0x00690A80, live, lambda b: [b + 0x100], 4, a0=0x100,
        fixups=["0x4", "0x518", "0x370"], stubs="r:00796EA0=8,r:007E4AB0=1,r:007F58B0=5",
        compare="u8", tag="money-live")

    def tunnel(node: int) -> tuple[bytearray, list[str]]:
        blob = fill(0x600)
        put_u32(blob, 8, 0x100)
        put_u32(blob, 0x100 + 0x418, 0x180)
        put_u32(blob, 0x180 + 0xA4, node)
        fix = ["0x8", "0x518"]
        if node:
            put_u32(blob, node + 4, 0x280)
            fix += ["0x284", "v1@0x280"]
        else:
            put_u32(blob, 0x24, 0x2C0)
            fix += ["v1@0x2C0"]
        if node:
            return blob, ["0x8", "0x518", "0x224", "0x204", "v1@0x280"]
        return blob, ["0x8", "0x518", "0x24", "v1@0x2C0"]

    blob, fix = tunnel(0x200)
    run(0x00692BA0, blob, lambda b: [b + 0x300], 4, a0=0x300, fixups=fix, compare="ptr", tag="tunnel-node")
    blob, fix = tunnel(0)
    run(0x00692BA0, blob, lambda b: [b + 0x300], 4, a0=0x300, fixups=fix, compare="ptr", tag="tunnel-fallback")

    vision = fill(0x40)
    run(0x00690240, vision, [0x10, 0x20], 8, a0=0x10, a1=0x20, stubs="r:004A62D0=0xABC", tag="vision-store")
    vision_b = fill(0x40)
    run(0x00690240, vision_b, [0, 1], 8, a0=0, a1=1, stubs="r:004A62D0=0", tag="vision-zero")
    xfer = fill(0x300)
    put_u32(xfer, 0x200, 0x240)
    put_u32(xfer, 0x2C, 0x55)
    run(0x00690270, xfer, lambda b: [b + 0x200], 4, a0=0x200,
        fixups=["0x200", "v1@0x258", "v1@0x2AC"], tag="vision-xfer-clear")
    xfer2 = fill(0x300)
    put_u32(xfer2, 0x200, 0x240)
    put_u32(xfer2, 0x200 + 4, 2)
    put_u32(xfer2, 0x2C, 0x55)
    run(0x00690270, xfer2, lambda b: [b + 0x200], 4, a0=0x200,
        fixups=["0x200", "v1@0x258", "v1@0x2AC"], stubs="r:004A62D0=0x99", tag="vision-xfer-bit")

    detonate = fill(0x240)
    put_u32(detonate, 0x18, 0x40)
    put_u32(detonate, 0x40 + 0x80, 0x80)
    put_u32(detonate, 0x80 + 0x24, 0x200)
    run(0x00693FB0, detonate, [], 0, fixups=["0x18", "0xC0", "0xA4", "v1@0x218"],
        compare="u32", tag="detonate")
    detonate_b = fill(0x240)
    put_u32(detonate_b, 0x18, 0x40)
    put_u32(detonate_b, 0xC0, 0x80)
    put_u32(detonate_b, 0xA4, 0x200)
    run(0x00693FB0, detonate_b, [], 0, fixups=["0x18", "0xC0", "0xA4", "v1@0x218"],
        compare="u32", tag="detonate-b")

    def dozer(ret_byte: int, gate: int, frame: int, ready: int, owner: int, mine: int) -> bytearray:
        blob = fill(0x780)
        put_u32(blob, 0x51C, 0x700)
        put_u32(blob, 0x51C + 8, ret_byte)
        put_u32(blob, 8, 0x100)
        put_u32(blob, 0x100 + 0xDC, mine)
        put_u32(blob, 0x200 + 0x44C, ready)
        put_u32(blob, 0x200 + 0x448, owner)
        put_u32(blob, 0x300 + 0x58, frame)
        return blob

    dozer_fix = ["0x51C", "0x8", "v1@0x750", "v2@0x730"]
    run(0x006975C0, dozer(0, 0, 0, 0, 0, 0), lambda b: [b + 0x200, 3], 8, a0=0x200, a1=3,
        fixups=dozer_fix, tag="dozer-reject")
    run(0x006975C0, dozer(1, 0, 0, 0, 0, 0), lambda b: [b + 0x200, 3], 8, a0=0x200, a1=3,
        fixups=dozer_fix, stubs="r:007E3720=0", tag="dozer-gate")
    run(0x006975C0, dozer(1, 1, 10, 20, 5, 9), lambda b: [b + 0x200, 3], 8, a0=0x200, a1=3,
        fixups=dozer_fix, stubs="r:007E3720=1,g:00DEBA64=300", tag="dozer-foreign")
    run(0x006975C0, dozer(1, 1, 30, 20, 5, 9), lambda b: [b + 0x200, 3], 8, a0=0x200, a1=3,
        fixups=dozer_fix, stubs="r:007E3720=1,g:00DEBA64=300", tag="dozer-accept")

    def crate(other_flags: int, template_cc: int, listed: int, match: int) -> tuple[bytearray, list[str]]:
        blob = fill(0x600)
        put_u32(blob, 4, 0x200)
        put_u32(blob, 8, 0x280)
        put_u32(blob, 0x18, 0x400)
        put_u32(blob, 0x1C, 0x404 if listed else 0x400)
        put_u32(blob, 0x400, match)
        put_u32(blob, 0x100 + 4, 0x300)
        put_u32(blob, 0x100 + 0x88, other_flags)
        put_u32(blob, 0x100 + 0x374, 1)
        put_u32(blob, 0x100 + 0xDC, match if listed else 0x55)
        put_u32(blob, 0x300 + 0xCC, template_cc)
        put_u32(blob, 0x480 + 0x34, 1)
        fix = ["0x4", "0x8", "0x18", "0x1C", "0x104", "0x304"]
        return blob, fix

    run(0x006908E0, fill(0x20), [0], 4, a0=0xFFFFFFFF, compare="u8", tag="crate-null")
    marked = fill(0x20)
    marked[0x14] = 1
    run(0x006908E0, marked, lambda b: [b + 0x10], 4, a0=0x10, compare="u8", tag="crate-used")
    blob, fix = crate(1 << 12, 0, 0, 0)
    run(0x006908E0, blob, lambda b: [b + 0x100], 4, a0=0x100, fixups=fix,
        stubs="g:00DFBD74=480", compare="u8", tag="crate-side")
    blob, fix = crate(0, 1 << 10, 0, 0)
    run(0x006908E0, blob, lambda b: [b + 0x100], 4, a0=0x100, fixups=fix,
        stubs="g:00DFBD74=480", compare="u8", tag="crate-template")
    blob, fix = crate(0, 0, 0, 0)
    run(0x006908E0, blob, lambda b: [b + 0x100], 4, a0=0x100, fixups=fix,
        stubs="g:00DFBD74=480,r:0067D3C0=0", compare="u8", tag="crate-empty")
    blob, fix = crate(0, 0, 1, 0x42)
    run(0x006908E0, blob, lambda b: [b + 0x100], 4, a0=0x100, fixups=fix,
        stubs="g:00DFBD74=480,r:0067D3C0=0", compare="u8", tag="crate-hit")
    blob, fix = crate(0, 0, 1, 0x99)
    put_u32(blob, 0x100 + 0xDC, 0x42)
    run(0x006908E0, blob, lambda b: [b + 0x100], 4, a0=0x100, fixups=fix,
        stubs="g:00DFBD74=480,r:0067D3C0=1", compare="u8", tag="crate-blocked")

    def xfer_blob() -> bytearray:
        blob = fill(0x300)
        put_u32(blob, 0x200, 0x240)
        return blob

    xfer_fix = ["0x200", "v1@0x258", "v1@0x2AC", "v1@0x2B0", "v1@0x2C4", "v1@0x2A4"]
    run(0x0069FF20, xfer_blob(), lambda b: [b + 0x200], 4, a0=0x200, fixups=xfer_fix, tag="stealth-xfer")
    stealth_b = xfer_blob()
    put_u32(stealth_b, 0x40, 0x1234)
    run(0x0069FF20, stealth_b, lambda b: [b + 0x200], 4, a0=0x200, fixups=xfer_fix, tag="stealth-xfer-b")
    run(0x00696BC0, xfer_blob(), lambda b: [b + 0x200], 4, a0=0x200, fixups=xfer_fix, tag="power-xfer")
    run(0x00696BC0, xfer_blob(), lambda b: [b + 0x200], 4, a0=0x200, fixups=xfer_fix, tag="power-xfer-b")
    run(0x0069F100, xfer_blob(), lambda b: [b + 0x200], 4, a0=0x200, fixups=xfer_fix, tag="damage-xfer")
    run(0x0069F100, xfer_blob(), lambda b: [b + 0x200], 4, a0=0x200, fixups=xfer_fix, tag="damage-xfer-b")

    def readiness(field: int, stored: float, module: int) -> bytearray:
        blob = fill(0x180)
        put_u32(blob, 0, 0x100)
        put_u32(blob, 8, field)
        put_f32(blob, 0x10, stored)
        put_u32(blob, 0x40, 0x60)  # anchor lives at self-0x24 when self is 0x40
        return blob

    # self at 0x40 so [self-0x24] is in range.
    early = fill(0x160)
    put_u32(early, 0x40, 0x100)
    put_u32(early, 0x48, 1)
    run(0x00696740, early, [], 0, self_off=0x40, fixups=["0x40", "v0@0x108"],
        compare="u32", float_ret=True, tag="ready-one")
    stored = fill(0x160)
    put_u32(stored, 0x40, 0x100)
    put_u32(stored, 0x48, 0x100)
    put_f32(stored, 0x50, 0.25)
    run(0x00696740, stored, [], 0, self_off=0x40, fixups=["0x40", "v0@0x108"],
        compare="u32", float_ret=True, tag="ready-stored")
    zeroed = fill(0x160)
    put_u32(zeroed, 0x40, 0x100)
    put_u32(zeroed, 0x1C, 0x80)  # [self-0x24] at 0x40-0x24=0x1C
    run(0x00696740, zeroed, [], 0, self_off=0x40, fixups=["0x40", "0x1C", "v0@0x108"],
        compare="u32", float_ret=True, tag="ready-zero")
    scaled = fill(0x160)
    put_u32(scaled, 0x40, 0x100)
    put_u32(scaled, 4 + 0x40, 20)  # [self+4]
    put_u32(scaled, 0x1C, 0x80)
    put_u32(scaled, 0x80 + 8, 0xA0)
    put_u32(scaled, 0xC0 + 0x58, 10)
    run(0x00696740, scaled, [], 0, self_off=0x40,
        fixups=["0x40", "0x1C", "0x88", "v0@0x108"],
        stubs="g:00DEBA64=C0,r:007E2970=10", compare="u32", float_ret=True, tag="ready-ratio")

    notify = fill(0x520)
    put_u32(notify, 0x40, 0x120)
    put_u32(notify, 0x48, 0x80)
    put_u32(notify, 0x80 + 8, 0xA0)
    put_u32(notify, 0xA0 + 0x1C, 1 << 12)
    put_u32(notify, 0x20, 0xC0)  # [self-0x20]
    put_u32(notify, 0xC0 + 0x418, 0xE0)
    run(0x00696800, notify, [], 0, self_off=0x40,
        fixups=["0x40", "0x48", "0x88", "0x20", "0x4D8", "v0@0x13C"],
        stubs="r:007EE070=0x42", compare="u32", tag="notify-owner")
    late = fill(0x180)  # descriptor chain stays inside this blob
    put_u32(late, 0x40, 0x120)
    put_u32(late, 0x48, 0x80)
    put_u32(late, 0x80 + 8, 0xA0)
    put_u32(late, 0x44, 7)
    put_u32(late, 0x4C, 30)
    put_u32(late, 0x100 + 0x58, 100)
    run(0x00696800, late, [], 0, self_off=0x40,
        fixups=["0x40", "0x48", "0x88", "v0@0x13C"],
        stubs="g:00DEBA64=100", compare="u32", tag="notify-late")

    height = fill(0x560)
    put_u32(height, 0x520 - 0x514, 0x80)  # self 0x520, object
    put_u32(height, 0x520 - 0x518, 0x40)
    put_f32(height, 0x40 + 0x54, 3.5)
    run(0x00697630, height, [], 0, self_off=0x520, fixups=["0xC", "0x8"],
        compare="u32", float_ret=True, tag="terrain-plain")
    # 0x520-0x514 = 0xC, 0x520-0x518 = 0x8. Yes.
    scaled_h = fill(0x560)
    put_u32(scaled_h, 0xC, 0x80)
    put_u32(scaled_h, 8, 0x40)
    put_u32(scaled_h, 0x80 + 0x418, 0x100)
    put_u32(scaled_h, 0x100 + 0x2C, 1)
    put_u32(scaled_h, 0x140 + 0x38, 0x160)
    put_u32(scaled_h, 0x160, 0x180)
    put_f32(scaled_h, 0x180 + 0x54, 0.5)
    put_f32(scaled_h, 0x40 + 0x54, 4.0)
    run(0x00697630, scaled_h, [], 0, self_off=0x520,
        fixups=["0xC", "0x8", "0x498", "0x178", "0x160"],
        stubs="g:00DFB9D8=140", compare="u32", float_ret=True, tag="terrain-scale")

    def garrison(bits94: int, bits26c: int) -> bytearray:
        blob = fill(0x520)
        put_u32(blob, 0x80 - 0x1C, 0x100)
        put_u32(blob, 0x100 + 0x94, bits94)
        put_u32(blob, 0x100 + 0x26C, bits26c)
        return blob

    run(0x006920B0, garrison(4, 0), [0, 0, 0], 0xC, self_off=0x80, a0=0xFFFFFFFF,
        fixups=["0x64"], compare="u8", tag="garrison-bit")
    run(0x006920B0, garrison(0, 2), [0, 0, 0], 0xC, self_off=0x80, a0=0xFFFFFFFF,
        fixups=["0x64"], compare="u8", tag="garrison-mask")
    clear = garrison(0, 0)
    run(0x006920B0, clear, [0, 0, 0], 0xC, self_off=0x80, a0=0xFFFFFFFF, fixups=["0x64"],
        compare="u8", tag="garrison-null")
    rejected = garrison(0, 0)
    put_u32(rejected, 0x5C, 0x300)
    run(0x006920B0, rejected, lambda b: [b + 0x180, 0, 0], 0xC, self_off=0x80,
        a0=0x180, fixups=["0x64", "0x5C", "v6@0x3A4"], compare="u8", tag="garrison-virtual")
    accepted = garrison(0, 0)
    put_u32(accepted, 0x7C, 1)
    put_u32(accepted, 0x5C, 0x300)
    put_u32(accepted, 0x180 + 4, 0x1C0)
    run(0x006920B0, accepted, lambda b: [b + 0x180, 0, 9], 0xC, self_off=0x80,
        a0=0x180, a2=9, fixups=["0x64", "0x5C", "0x184", "v6@0x3A4"],
        compare="u8", tag="garrison-open")
    full = garrison(0, 0)
    put_u32(full, 0x7C, 1)
    put_u32(full, 0x5C, 0x300)
    put_u32(full, 0x180 + 4, 0x1C0)
    put_u32(full, 0x1C0 + 0xC0, 1 << 28)
    run(0x006920B0, full, lambda b: [b + 0x180, 1, 0], 0xC, self_off=0x80,
        a0=0x180, a1=1, fixups=["0x64", "0x5C", "0x184", "v6@0x3A4"],
        compare="u8", tag="garrison-kind")
    roomy = garrison(0, 0)
    put_u32(roomy, 0x7C, 1)
    put_u32(roomy, 0x5C, 0x300)
    put_u32(roomy, 0x80, 0x400)
    put_u32(roomy, 0x88, 10)
    put_u32(roomy, 0x90, 3)
    put_u32(roomy, 0x180 + 4, 0x1C0)
    run(0x006920B0, roomy, lambda b: [b + 0x180, 1, 0], 0xC, self_off=0x80,
        a0=0x180, a1=1, fixups=["0x64", "0x5C", "0x80", "0x184", "v6@0x3A4", "v0@0x478", "v5@0x50C"],
        compare="u8", tag="garrison-room")
    tight = garrison(0, 0)
    put_u32(tight, 0x7C, 1)
    put_u32(tight, 0x5C, 0x300)
    put_u32(tight, 0x80, 0x400)
    put_u32(tight, 0x88, 3)
    put_u32(tight, 0x90, 3)
    put_u32(tight, 0x180 + 4, 0x1C0)
    run(0x006920B0, tight, lambda b: [b + 0x180, 1, 0], 0xC, self_off=0x80,
        a0=0x180, a1=1, fixups=["0x64", "0x5C", "0x80", "0x184", "v6@0x3A4", "v0@0x478", "v5@0x50C"],
        compare="u8", tag="garrison-full")

    slot = fill(0x180)
    put_u32(slot, 0x40 - 0x1C, 0x80)
    put_u32(slot, 0x80 + 4, 0xA0)
    run(0x00692460, slot, [], 0, self_off=0x40, fixups=["0x24", "0x84"],
        compare="ptr", tag="garrison-anchor")
    jumped = fill(0x2C0)
    put_u32(jumped, 0x24, 0x80)
    put_u32(jumped, 0x80 + 4, 0xA0)
    put_u32(jumped, 0xA0 + 0xD4, 1 << 0x15)
    put_u32(jumped, 0x40, 0x140)
    put_u32(jumped, 0x48, 0x50)
    run(0x00692460, jumped, [], 0, self_off=0x40,
        fixups=["0x24", "0x84", "0x40", "v0@0x284"], compare="ptr", tag="garrison-jump")

    point = fill(0x180)
    put_u32(point, 0x24, 0x80)
    put_u32(point, 0x80 + 4, 0xA0)
    run(0x00692490, point, [], 0, self_off=0x40, fixups=["0x24", "0x84"],
        compare="ptr", tag="locate-plain")
    missed = fill(0x180)
    put_u32(missed, 0x24, 0x80)
    put_u32(missed, 0x84, 0xA0)
    put_u32(missed, 0xA0 + 0xD4, 1 << 0x15)
    run(0x00692490, missed, [], 0, self_off=0x40, fixups=["0x24", "0x84"],
        stubs="r:0063EB60=0", compare="ptr", tag="locate-miss")
    found = fill(0x180)
    put_u32(found, 0x24, 0x80)
    put_u32(found, 0x84, 0xA0)
    put_u32(found, 0xA0 + 0xD4, 1 << 0x15)
    run(0x00692490, found, [], 0, self_off=0x40, fixups=["0x24", "0x84"],
        stubs="r:0063EB60=1", compare="ptr", tag="locate-hit")

    miss = fill(0x100)
    put_u32(miss, 0x10, 0x40)
    put_u32(miss, 0x40 + 0xC, 1)
    put_u32(miss, 0x40 + 0x18, 0x99)
    run(0x0069E660, miss, [0x10], 4, a0=0x10, fixups=["0x10"], compare="u8", tag="cancel-miss")
    hit = fill(0x100)
    put_u32(hit, 0x10, 0x40)
    put_u32(hit, 0x40 + 0xC, 1)
    put_u32(hit, 0x40 + 0x18, 0x10)
    run(0x0069E660, hit, [0x10], 4, a0=0x10, fixups=["0x10"], compare="u8", tag="cancel-hit")
    done = fill(0x100)
    put_u32(done, 0x10, 0x40)
    put_u32(done, 0x40 + 0xC, 1)
    put_u32(done, 0x40 + 0x18, 0x10)
    done[0x40 + 0x64] = 1
    run(0x0069E660, done, [0x10], 4, a0=0x10, fixups=["0x10"], compare="u8", tag="cancel-done")

    relay = fill(0x80)
    run(0x0069FA90, relay, [0x5, 0], 8, self_off=0x40, a0=0x5, stubs="r:0069F9E0=0",
        compare="u8", tag="relay-stop")
    idle = fill(0x160)
    put_u32(idle, 0x40 - 0x20, 0x80)
    run(0x0069FA90, idle, [0x5, 0], 8, self_off=0x40, a0=0x5, fixups=["0x20"],
        stubs="r:0069F9E0=1", compare="u8", tag="relay-idle")
    busy = fill(0x160)
    put_u32(busy, 0x20, 0x80)
    put_u32(busy, 0x80 + 0xD8, 4)
    run(0x0069FA90, busy, [0x5, 0], 8, self_off=0x40, a0=0x5, fixups=["0x20"],
        stubs="r:0069F9E0=1", compare="u8", tag="relay-busy")

    copied = fill(0x160)
    put_u32(copied, 0, 0x100)
    put_u32(copied, 8, 0x40)
    put_u32(copied, 0x40 + 8, 0x60)
    put_u32(copied, 0x60, 0x80)
    put_u32(copied, 0x80 + 4, 0x2A)
    run(0x00695720, copied, lambda b: [b + 0xC0], 4, a0=0xC0,
        fixups=["0x0", "0x8", "0x48", "0x60", "v0@0x11C"], compare="ptr", tag="copy-kind")
    copied_b = fill(0x160)
    put_u32(copied_b, 0, 0x100)
    put_u32(copied_b, 8, 0x40)
    put_u32(copied_b, 0x48, 0x60)
    put_u32(copied_b, 0x60, 0x80)
    put_u32(copied_b, 0x84, 7)
    run(0x00695720, copied_b, lambda b: [b + 0xC0], 4, a0=0xC0,
        fixups=["0x0", "0x8", "0x48", "0x60", "v0@0x11C"], compare="ptr", tag="copy-kind-b")

    def span(count: int, ids: list[int], engage_count: int) -> tuple[bytearray, list[str]]:
        blob = fill(0x220)
        put_u32(blob, 0x20 - 0xC, 0x40)
        put_u32(blob, 0x20 - 8, 0x80)
        put_u32(blob, 0x40 + 0x1C0, count)
        put_u32(blob, 0x40 + 0x1C4, 0xC0)
        put_u32(blob, 0x20 + 0x24, engage_count)
        for index, value in enumerate(ids):
            put_u32(blob, 0xC0 + 4 * index, value)
        return blob, ["0x14", "0x18", "0x204"]

    blob, fix = span(0, [], 0)
    run(0x006953E0, blob, [0], 4, self_off=0x20, a0=0, fixups=fix, tag="adjust-idle")
    blob, fix = span(0, [], 5)
    run(0x006953E0, blob, [1], 4, self_off=0x20, a0=1, fixups=fix, tag="adjust-inc")
    blob, fix = span(1, [0x11], 4)
    run(0x006953E0, blob, [0], 4, self_off=0x20, a0=0, fixups=fix, stubs="p:00687720=0", tag="adjust-abort")
    blob, fix = span(1, [0x11], 1)
    put_u32(blob, 0x48, 10)
    put_u32(blob, 0x40, 3)
    put_u32(blob, 0x90 + 0x58, 16)
    run(0x006953E0, blob, [0], 4, self_off=0x20, a0=0, fixups=fix,
        stubs="p:00687720=0,g:00DEBA64=90", tag="adjust-zero")
    blob, fix = span(1, [0x180], 2)
    put_u32(blob, 0x180, 0x140)
    run(0x006953E0, blob, [1], 4, self_off=0x20, a0=1, fixups=fix + ["0x180", "v1@0x168"],
        stubs="p:00687720=180", tag="adjust-visit")

    def kind_blob(kind: int) -> tuple[bytearray, list[str]]:
        blob = fill(0x380)
        put_u32(blob, 0x20 - 0xC, 0x40)
        put_u32(blob, 0x18, 0x80)
        put_u32(blob, 0x40 + 0x1C0, 1)
        put_u32(blob, 0x40 + 0x1C4, 0xC0)
        put_u32(blob, 0xC0, 0x100)
        put_u32(blob, 0x100 + 8, 0x120)
        put_u32(blob, 0x120 + 0x14, kind)
        put_u32(blob, 0x280, 0x2E0)
        return blob, ["0x14", "0x18", "0x204", "0xC0", "0x108", "0x280"]

    blob, fix = kind_blob(9)
    run(0x00695210, blob, [0x70], 4, self_off=0x20, a0=0x70, fixups=fix, tag="kind4-skip")
    blob, fix = kind_blob(4)
    put_u32(blob, 0x288, 0)
    run(0x00695210, blob, [0x70], 4, self_off=0x20, a0=0x70, fixups=fix + ["v1@0x358", "v1@0x30C"],
        stubs="p:00687720=280", tag="kind4-quiet")
    blob, fix = kind_blob(4)
    put_u32(blob, 0x288, 1)
    run(0x00695210, blob, [0x70], 4, self_off=0x20, a0=0x70, fixups=fix + ["v1@0x358", "v1@0x30C"],
        stubs="p:00687720=280", tag="kind4-apply")

    blob, fix = kind_blob(2)
    run(0x00695170, blob, [0x31, 0x32], 8, self_off=0x20, a0=0x31, a1=0x32,
        fixups=fix + ["v2@0x354", "v2@0x310"], stubs="p:00687720=280", tag="kind23-stop")
    blob, fix = kind_blob(2)
    put_u32(blob, 0x288, 1)
    run(0x00695170, blob, [0x31, 0x32], 8, self_off=0x20, a0=0x31, a1=0x32,
        fixups=fix + ["v2@0x354", "v2@0x310"], stubs="p:00687720=280", tag="kind23-go")
    blob, fix = kind_blob(1)
    run(0x00695170, blob, [0x31, 0x32], 8, self_off=0x20, a0=0x31, a1=0x32, fixups=fix, tag="kind23-other")

    blob, fix = kind_blob(0)
    run(0x006950D0, blob, [0x41, 0x42], 8, self_off=0x20, a0=0x41, a1=0x42,
        fixups=fix + ["v3@0x34C", "v2@0x314"], stubs="p:00687720=280", tag="kind03-stop")
    blob, fix = kind_blob(3)
    put_u32(blob, 0x288, 1)
    run(0x006950D0, blob, [0x41, 0x42], 8, self_off=0x20, a0=0x41, a1=0x42,
        fixups=fix + ["v3@0x34C", "v2@0x314"], stubs="p:00687720=280", tag="kind03-go")

    blob, fix = kind_blob(1)
    run(0x00695020, blob, [0x51, 0x3F800000, 0x52], 0xC, self_off=0x20, a0=0x51, a1=0x3F800000, a2=0x52,
        fixups=fix + ["v4@0x350", "v3@0x318"], stubs="p:00687720=280", tag="kind1-stop")
    blob, fix = kind_blob(1)
    put_u32(blob, 0x288, 1)
    run(0x00695020, blob, [0x51, 0x3F800000, 0x52], 0xC, self_off=0x20, a0=0x51, a1=0x3F800000, a2=0x52,
        fixups=fix + ["v4@0x350", "v3@0x318"], stubs="p:00687720=280", tag="kind1-go")

    dock = fill(0x180)
    put_u32(dock, 0x40, 0x100)
    put_u32(dock, 0x20, 0x60)
    put_f32(dock, 0x60 + 0x10, 2.0)
    put_u32(dock, 0x40 + 0x40, 0xC0)
    put_u32(dock, 0x40 + 0x44, 0xC8)
    put_u32(dock, 0xC0, 7)
    run(0x00697B80, dock, [], 0, self_off=0x40,
        fixups=["0x40", "0x20", "0x80", "0x84", "v0@0x160"],
        compare="u32", float_ret=True, tag="dock-one")
    empty_dock = fill(0x180)
    put_u32(empty_dock, 0x40, 0x100)
    put_u32(empty_dock, 0x48, 1)
    put_u32(empty_dock, 0x20, 0x60)
    put_f32(empty_dock, 0x70, 1.0)
    put_u32(empty_dock, 0x80, 0xC0)
    put_u32(empty_dock, 0x84, 0xC0)
    run(0x00697B80, empty_dock, [], 0, self_off=0x40,
        fixups=["0x40", "0x20", "0x80", "0x84", "v0@0x160"],
        compare="u32", float_ret=True, tag="dock-virtual")
    none_dock = fill(0x180)
    put_u32(none_dock, 0x40, 0x100)
    put_u32(none_dock, 0x20, 0x60)
    put_f32(none_dock, 0x70, 4.0)
    put_u32(none_dock, 0x80, 0xC0)
    put_u32(none_dock, 0x84, 0xC0)
    run(0x00697B80, none_dock, [], 0, self_off=0x40,
        fixups=["0x40", "0x20", "0x80", "0x84", "v0@0x160"],
        compare="u32", float_ret=True, tag="dock-zero")

    def horde(anchor_bits: int, other_bits: int, count: int) -> bytearray:
        blob = fill(0x600)
        put_u32(blob, 0x200 - 0x164, 0x40)
        put_u32(blob, 0x40 + 0x88, anchor_bits)
        put_u32(blob, 0x100 + 0x88, other_bits)
        put_u32(blob, 0x100 + 4, 0x140)
        put_u32(blob, 0x140 + 0x14, 0x160)
        put_u32(blob, 0x160, 0x180)
        put_u32(blob, 0x184, 0x99)
        put_u32(blob, 0x1C0, count)
        return blob

    horde_fix = ["0x9C", "0x104", "0x154", "0x160"]
    run(0x006913D0, fill(0x20), [0, 1, 0, 0], 0x10, a0=0xFFFFFFFF, a1=1, compare="u8", tag="horde-gate")
    run(0x006913D0, horde(0, 0, 0), lambda b: [b + 0x100, 0, b + 0x1C0, 0], 0x10,
        self_off=0x200, a0=0x100, a2=0x1C0, fixups=horde_fix, compare="u8", tag="horde-empty")
    run(0x006913D0, horde(0, 0, 2), lambda b: [b + 0x100, 0, b + 0x1C0, 1], 0x10,
        self_off=0x200, a0=0x100, a2=0x1C0, a3=1, fixups=horde_fix,
        stubs="r:0078D880=0|0", compare="u8", tag="horde-miss")
    run(0x006913D0, horde(0, 0, 2), lambda b: [b + 0x100, 0, b + 0x1C0, 1], 0x10,
        self_off=0x200, a0=0x100, a2=0x1C0, a3=1, fixups=horde_fix,
        stubs="r:0078D880=0|1", compare="u8", tag="horde-second")
    run(0x006913D0, horde(0, 2, 3), lambda b: [b + 0x100, 0, b + 0x1C0, 0], 0x10,
        self_off=0x200, a0=0x100, a3=0, fixups=horde_fix, compare="u8", tag="horde-closed")
    both = horde(2, 2, 1)
    run(0x006913D0, both, lambda b: [b + 0x100, 0, b + 0x1C0, 1], 0x10,
        self_off=0x200, a0=0x100, a2=0x1C0, a3=1, fixups=horde_fix,
        stubs="r:0078D880=1", compare="u8", tag="horde-same-side")
    players = horde(2, 2, 1)
    put_u32(players, 0x100 + 0x414, 0x1A0)
    put_u32(players, 0x40 + 0x414, 0x1B0)
    put_u32(players, 0x100 + 0x418, 0x1A8)
    put_u32(players, 0x40 + 0x418, 0x1B8)
    run(0x006913D0, players, lambda b: [b + 0x100, 0, b + 0x1C0, 1], 0x10,
        self_off=0x200, a0=0x100, a2=0x1C0, a3=1,
        fixups=horde_fix + ["0x514", "0x454", "0x518", "0x458"],
        stubs="r:00B11D40=7|7", compare="u8", tag="horde-players")

    quiet_timer = fill(0x180)
    run(0x00691620, quiet_timer, [0x3F800000], 4, self_off=0x164, a0=0x3F800000, tag="timer-null")
    armed_timer = fill(0x200)
    put_u32(armed_timer, 0, 0x40)
    put_u32(armed_timer, 0x80 + 0x58, 15)
    run(0x00691620, armed_timer, [0x3F800000], 4, self_off=0x164, a0=0x3F800000,
        fixups=["0x0"], stubs="g:00DEBA64=80", tag="timer-arm")

    oracle.close()
    missing = [hex(va) for va in ACCEPTED_ENTRIES if oracle.counts.get(va, 0) < 2]
    if missing:
        raise AssertionError(f"fewer than 2 cases: {', '.join(missing)}")
    cases = sum(oracle.counts.values())
    print(f"obj69c oracle: {len(ACCEPTED_ENTRIES)} entries, {cases} cases, 0 mismatches")


if __name__ == "__main__":
    if len(sys.argv) != 2:
        raise SystemExit("usage: verify_obj69c.py <ra3_obj69ccheck>")
    verify(Path(sys.argv[1]))
