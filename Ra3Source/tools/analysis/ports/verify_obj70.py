"""Differential oracle for the 0x00700000–0x0071FFFF object-module batch.

The Release checker and Unicorn share the arena at 0x60000000. Stub side
effects (vector growth, coordinate nudge, stance poke, version byte) are the
same on both sides. EAX, the call log and the dumped dwords must match.
"""

from __future__ import annotations

import struct
import subprocess
import sys
from pathlib import Path

import unicorn.x86_const as ux

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from emulate import Emulator  # noqa: E402

EXE = Path(__file__).resolve().parents[3] / "build" / "win32" / "bin" / "Release" / "ra3_obj70check.exe"
BASE = 0x60000000
SELF = BASE + 0x200
P = SELF + 0x400
Q = SELF + 0x600
R = SELF + 0x800
S = SELF + 0xA00
T = SELF + 0xC00
U = SELF + 0xE00
VEC = BASE + 0x1C00
XFER = BASE + 0x2800
NODE = BASE + 0x2C00
VT = BASE + 0x3000
VT3 = BASE + 0x3400
VT4 = BASE + 0x3600
RAW = 0x00BBA500
KNOWN = {
    0x00C053B8, 0x00BC8E74, 0x00BC8E80, 0x00711ED0, 0x7FFFFFFF,
    0x3F800000, 0x40000000, 0x3F000000, 0xBF800000, 0xC0000000,
    0x40400000, 0x40800000, 0x41200000, 0x7FC00000, 0x80000000,
    0x3FC00000, 0x40A00000,
}

# address, tag, stack args, cleanup
DIRECTS = [
    (0x004166B0, "4166b0", 1, 0),
    (0x00416860, "416860", 6, 0),
    (0x004DA760, "4da760", 0, 0),
    (0x004E2140, "4e2140", 1, 4),
    (0x004E41B0, "4e41b0", 2, 8),
    (0x004ED380, "4ed380", 0, 0),
    (0x004F6850, "4f6850", 2, 8),
    (0x00506C20, "506c20", 4, 0x10),
    (0x0052EFC0, "52efc0", 2, 8),
    (0x00538530, "538530", 0, 0),
    (0x00539DF0, "539df0", 2, 0),
    (0x0054A090, "54a090", 2, 0),
    (0x00683E70, "683e70", 1, 4),
    (0x00685080, "685080", 1, 4),
    (0x00687460, "687460", 1, 4),
    (0x0068B180, "68b180", 0, 0),
    (0x0069D000, "69d000", 0, 0),
    (0x0069FA90, "69fa90", 2, 8),
    (0x006A56A0, "6a56a0", 0, 0),
    (0x006A8400, "6a8400", 0, 0),
    (0x006BCF30, "6bcf30", 1, 4),
    (0x006C1C50, "6c1c50", 2, 8),
    (0x006C8560, "6c8560", 0, 0),
    (0x006CACE0, "6cace0", 3, 0xC),
    (0x006D1ED0, "6d1ed0", 2, 8),
    (0x006D23F0, "6d23f0", 1, 4),
    (0x006E96E0, "6e96e0", 0, 0),
    (0x006EA540, "6ea540", 6, 0x18),
    (0x006EA9D0, "6ea9d0", 3, 0xC),
    (0x006ECE70, "6ece70", 1, 4),
    (0x006EE8F0, "6ee8f0", 5, 0x14),
    (0x006F0A20, "6f0a20", 1, 4),
    (0x006F2050, "6f2050", 0, 0),
    (0x006F2730, "6f2730", 2, 8),
    (0x006F42D0, "6f42d0", 2, 8),
    (0x006F8B80, "6f8b80", 2, 8),
    (0x006FD820, "6fd820", 2, 8),
    (0x006FBBD0, "6fbbd0", 0, 0),
    (0x00702070, "702070", 0, 0),
    (0x00707050, "707050", 1, 4),
    (0x00708190, "708190", 2, 0),
    (0x007087D0, "7087d0", 2, 0),
    (0x00700E50, "700e50", 0, 0),
    (0x007010D0, "7010d0", 0, 0),
    (0x00701330, "701330", 0, 0),
    (0x007174C0, "7174c0", 0, 0),
    (0x007176B0, "7176b0", 0, 0),
    (0x00717B40, "717b40", 0, 0),
    (0x0071ABE0, "71abe0", 1, 4),
    (0x0071D020, "71d020", 2, 0),
    (0x0071F0F0, "71f0f0", 2, 8),
    (0x0078DA40, "78da40", 1, 4),
    (0x007ED750, "7ed750", 2, 8),
    (0x007F3120, "7f3120", 1, 4),
    (0x00860630, "860630", 1, 4),
    (0x008D82F0, "8d82f0", 1, 4),
    (0x009279F0, "9279f0", 3, 0),
    (0x00986150, "986150", 1, 4),
]


def u32(value: int) -> int:
    return value & 0xFFFFFFFF


def p32(value: int) -> bytes:
    return struct.pack("<I", u32(value))


def as_signed(value: int) -> int:
    value = u32(value)
    return value - 0x100000000 if value >= 0x80000000 else value


def parse(command: str) -> tuple[str, list[int], list[int]]:
    cmd, *tokens = command.split()
    phase = 0
    args: list[int] = []
    queue: list[int] = []
    for token in tokens:
        if token == "q":
            phase = 1
            continue
        if token == "mem":
            phase = 2
            continue
        value = int(token, 0)
        if phase == 1:
            queue.append(u32(value))
        elif phase == 0:
            args.append(u32(value))
    return cmd, args, queue


class Checker:
    def __init__(self, executable: Path) -> None:
        self.process = subprocess.Popen(
            [str(executable)],
            stdin=subprocess.PIPE,
            stdout=subprocess.PIPE,
            text=True,
            bufsize=1,
        )

    def call(self, command: str) -> dict[str, str]:
        assert self.process.stdin is not None and self.process.stdout is not None
        self.process.stdin.write(command + "\n")
        self.process.stdin.flush()
        line = self.process.stdout.readline().strip()
        if not line:
            raise RuntimeError(f"checker closed on: {command}")
        out = {}
        for token in line.split():
            key, value = token.split("=", 1)
            out[key] = value
        return out

    def close(self) -> None:
        if self.process.stdin is not None:
            self.process.stdin.write("quit\n")
            self.process.stdin.flush()
        self.process.wait(timeout=10)


class Oracle:
    def __init__(self) -> None:
        self.emu = Emulator()
        self.log: list[str] = []
        self.queue: list[int] = []
        self.qi = 0
        self.force_minor = -1
        self.nudge = False
        self.touch34 = False
        self.fn0: dict[int, int] = {}
        self.fn1: dict[int, int] = {}
        self.fn2: dict[int, int] = {}
        self._bind()

    def _bind(self) -> None:
        def virtual(slot: int, arity: int, touch: bool = False):
            def stub(emu: Emulator, slot: int = slot, arity: int = arity, touch: bool = touch) -> int:
                return self.on_virtual(emu, slot, arity, touch)
            return stub

        for slot in (0x14, 0x60, 0xAC, 0xF4):
            self.fn0[slot] = self.emu.bind_stub(virtual(slot, 0), 0)
        self.fn0[0x6C] = self.emu.bind_stub(virtual(0x6C, 0, True), 0)
        for slot in (0x08, 0x18, 0x2C, 0x64, 0x6C, 0x70, 0x84, 0xB4):
            cleanup = 4
            self.fn1[slot] = self.emu.bind_stub(virtual(slot, 1), cleanup)
        self.fn2[0x58] = self.emu.bind_stub(virtual(0x58, 2), 4)

        def direct(tag: str, nargs: int):
            def stub(emu: Emulator, tag: str = tag, nargs: int = nargs) -> int:
                args = [emu.stack_arg(i) for i in range(nargs)]
                return self.on_direct(tag, args)
            return stub

        for address, tag, nargs, cleanup in DIRECTS:
            self.emu.bind_function(address, direct(tag, nargs), cleanup)

        def note(tag: str, args: list[int]) -> None:
            self.log.append("c" + tag + "".join(self.arg_text(arg) for arg in args))

        def grow(emu: Emulator) -> int:
            end = emu.stack_arg(0)
            src = emu.stack_arg(1)
            vec = emu.register(ux.UC_X86_REG_ECX)
            note("6f3e20", [end, src])
            if BASE <= end < BASE + 0x4000 and src >= 0x10000:
                emu.write(end, emu.read(src, 4))
            emu.write(vec + 4, p32(end + 4))
            return self.popq()

        def push8(emu: Emulator) -> int:
            end = emu.stack_arg(0)
            src = emu.stack_arg(1)
            vec = emu.register(ux.UC_X86_REG_ECX)
            note("7f17c0", [end, src])
            if BASE <= end < BASE + 0x4000 and src >= 0x10000:
                emu.write(end, emu.read(src, 8))
            emu.write(vec + 4, p32(end + 8))
            return self.popq()

        def project(emu: Emulator) -> int:
            ptr = emu.stack_arg(0)
            note("63ed00", [ptr])
            if self.nudge and ptr >= 0x10000:
                emu.write(ptr, p32(struct.unpack("<I", emu.read(ptr, 4))[0] + 1))
            return self.popq()

        def place(emu: Emulator) -> int:
            coords = emu.stack_arg(0)
            flag = emu.stack_arg(1)
            this = emu.register(ux.UC_X86_REG_ECX)
            note("6f0910", [coords, flag])
            if coords >= 0x10000:
                emu.write(this + 0x38, emu.read(coords, 12))
            return self.popq()

        def raw(emu: Emulator) -> int:
            args = [emu.stack_arg(i) for i in range(3)]
            return self.on_direct("bba500", args)

        self.emu.bind_function(0x006F3E20, grow, 8)
        self.emu.bind_function(0x007F17C0, push8, 8)
        self.emu.bind_function(0x0063ED00, project, 0)
        self.emu.bind_function(0x006F0910, place, 8)
        self.raw = self.emu.bind_stub(raw, 0)

    def on_direct(self, tag: str, args: list[int]) -> int:
        text = "c" + tag + "".join(self.arg_text(arg) for arg in args)
        self.log.append(text)
        return self.popq()

    def on_virtual(self, emu: Emulator, slot: int, arity: int, touch: bool) -> int:
        args = [emu.stack_arg(i) for i in range(arity)]
        if slot == 0x18 and arity == 1 and args[0] >= 0x10000:
            blob = emu.read(args[0], 2)
            self.log.append(f"v18:{blob[0]:02x}{blob[1]:02x}")
            if self.force_minor >= 0:
                emu.write(args[0] + 1, bytes([self.force_minor & 0xFF]))
            return self.popq()
        if touch and self.touch34:
            this = emu.register(ux.UC_X86_REG_ECX)
            current = struct.unpack("<I", emu.read(this + 0x44, 4))[0]
            emu.write(this + 0x44, p32(current + 1))
        text = f"v{slot:x}" + "".join(self.arg_text(arg) for arg in args)
        self.log.append(text)
        return self.popq()

    def arg_text(self, arg: int) -> str:
        arg = u32(arg)
        if arg < 0x10000 or BASE <= arg < BASE + 0x4000 or arg in KNOWN or arg >= 0xFFFF0000:
            return f":{arg:x}"
        return ":s"

    def popq(self) -> int:
        if self.qi >= len(self.queue):
            return 0
        value = self.queue[self.qi]
        self.qi += 1
        return value

    def put(self, address: int, value: int) -> None:
        self.emu.write(address, p32(value))

    def get(self, address: int) -> int:
        return struct.unpack("<I", self.emu.read(address, 4))[0]

    def plant(self) -> None:
        self.emu.write(BASE, b"\x00" * 0x4000)
        slots = {
            0x08: self.fn1[0x08], 0x14: self.fn0[0x14], 0x18: self.fn1[0x18],
            0x2C: self.fn1[0x2C], 0x58: self.fn2[0x58], 0x60: self.fn0[0x60],
            0x64: self.fn1[0x64], 0x6C: self.fn1[0x6C], 0x70: self.fn1[0x70],
            0x84: self.fn1[0x84], 0xAC: self.fn0[0xAC], 0xB4: self.fn1[0xB4],
            0xF4: self.fn0[0xF4],
        }
        for offset, fn in slots.items():
            self.put(VT + offset, fn)
        self.put(VT3 + 0x6C, self.fn0[0x6C])
        self.put(VT4 + 0xF4, self.fn0[0xF4])
        self.put(VT4 + 0x60, self.fn0[0x60])
        self.put(XFER, VT)
        self.put(SELF, VT)
        self.put(SELF - 0x18, VT)
        self.put(SELF - 0x10, VT3)
        self.put(SELF + 0x16C, VT4)
        self.put(P + 0x14, P + 0x14)
        self.put(P + 0x18, P + 0x14)

    def globals(self, ready: int = 0, value: int = 0, floor: int = 0x3F000000,
                limit: int = 0x3F800000, ghost: int = P, player: int = R) -> None:
        self.put(0x00DF8918, ghost)
        self.put(0x00DF98F4, ready)
        self.put(0x00DF98F0, value)
        self.put(0x00E1A078, VT)
        self.put(0x00DFC190, Q)
        self.put(0x00BE700C, 0x3F800000)
        self.put(0x00BF6060, floor)
        self.put(0x00BD94EC, limit)
        self.put(0x00DEBA5C, player)
        self.emu.write(RAW, p32(self.raw))

    def chain(self, slot: int, ident: int) -> None:
        self.put(slot, P)
        self.put(P + 8, Q)
        self.put(Q + 8, R)
        self.put(R, S)
        self.put(S + 4, ident)

    def other(self, ident: int) -> None:
        self.put(T + 8, U)
        self.put(U, S + 0x20)
        self.put(S + 0x24, ident)

    def vector(self, vec: int, begin: int, end: int, cap: int) -> None:
        self.put(vec, begin)
        self.put(vec + 4, end)
        self.put(vec + 8, cap)

    def begin(self, queue: list[int]) -> None:
        self.emu.reset_data()
        arena = self.emu.alloc(b"\x00" * 0x4000)
        if arena != BASE:
            raise RuntimeError(f"arena {arena:#x}")
        self.log = []
        self.queue = list(queue)
        self.qi = 0
        self.force_minor = -1
        self.nudge = False
        self.touch34 = False
        self.plant()
        self.globals()

    def emit(self, address: int, ecx: int, stack: list[int], mem: list[int]) -> dict[str, str]:
        eax = u32(self.emu.call(address, stack, ecx=ecx).eax)
        return {
            "eax": f"{eax:08x}",
            "log": ",".join(self.log) if self.log else ".",
            "mem": self.mem_text(mem),
        }

    def mem_text(self, offsets: list[int]) -> str:
        if not offsets:
            return "."
        return ",".join(f"{offset:x}:{self.get(SELF + offset):08x}" for offset in offsets)

    def run(self, command: str) -> dict[str, str]:
        cmd, args, queue = parse(command)
        self.begin(queue)
        s = SELF
        if cmd == "mask":
            self.put(VEC, 0)
            return self.emit(0x00710C80, 0, [VEC], [0x1A00])
        if cmd == "adj4":
            return self.emit(0x00710BF0, args[0], [], [])
        if cmd == "adj0":
            return self.emit(0x00710C20, args[0], [], [])
        if cmd == "sameb":
            self.chain(s - 0xB8, args[0])
            self.other(args[1])
            return self.emit(0x00710C50, s, [T], [])
        if cmd == "samew":
            self.chain(s - 0xB4, args[0])
            self.other(args[1])
            return self.emit(0x00712EF0, s, [T], [])
        if cmd == "span":
            self.put(s + 0xB8, args[0])
            self.put(s + 0xBC, args[1])
            return self.emit(0x00715EE0, s, [], [])
        if cmd == "fwd":
            return self.emit(0x007011C0, s, args[:6], [])
        if cmd == "armed":
            self.put(s - 0x20, P)
            self.emu.write(P + 8, bytes([args[0] & 0xFF]))
            return self.emit(0x00701410, s, [1], [])
        if cmd == "unset":
            self.put(P, args[1])
            return self.emit(0x00715FE0, s, [args[0]], [])
        if cmd == "maskif":
            self.emu.write(s + 0x30, bytes([args[0] & 0xFF]))
            return self.emit(0x007037F0, s, [7], [])
        if cmd == "relay":
            self.put(s - 8, P)
            self.put(P + 0xF4, args[0])
            return self.emit(0x007035C0, s, [9], [])
        if cmd == "inside":
            self.put(s - 0x1C, P)
            return self.emit(0x00702360, s, [args[0], args[1]], [])
        if cmd == "pushid":
            self.vector(s + 4, args[0], args[1], args[2])
            return self.emit(0x0070C900, s, [args[3]], [0x8, 0x1A00])
        if cmd == "clearrec":
            self.put(s + 0xF0, args[0])
            self.put(s + 0xF4, args[1])
            return self.emit(0x0070D570, s, [], [0xF4])
        if cmd == "stance":
            self.globals(floor=args[0])
            self.put(s + 4, args[1])
            self.put(s + 0x0C, args[2])
            self.put(s + 0x34, args[3])
            self.put(s - 8, P)
            self.put(P + 0x80, args[4])
            self.touch34 = len(args) > 8 and args[8] != 0
            if args[5] != 0:
                self.put(Q + 8, args[6])
            return self.emit(0x0070D490, s, [args[7], Q if args[5] else 0], [0x4, 0x8, 0x34])
        if cmd == "latch":
            self.put(s + 4, P)
            self.emu.write(P + 0x101, bytes([args[0] & 0xFF]))
            self.put(s + 8, Q)
            self.put(Q + 0x88, args[1])
            return self.emit(0x007128C0, s, [], [0x688])
        if cmd == "motion":
            self.put(s - 0x20, P)
            self.emu.write(P + 0x6D, bytes([args[0] & 0xFF]))
            self.put(s - 0x1C, Q)
            self.put(Q + 0x80, args[1])
            return self.emit(0x00718200, s, [0], [0x4, 0x8, 0x680])
        if cmd == "poll":
            self.put(s + 0x20, P)
            self.put(P + 0x3C, args[0])
            self.put(P + 0x5C, args[1])
            self.emu.write(P + 0x60, bytes([args[2] & 0xFF]))
            self.put(s + 0x18, Q)
            self.put(Q + 0x24, R)
            self.emu.write(R + 0x9B, bytes([args[3] & 0xFF]))
            self.put(S + 0x94, args[4])
            return self.emit(0x00703F70, s, [], [0x43C])
        if cmd == "fire":
            self.put(s + 0x18, Q)
            self.put(Q + 0x24, R)
            self.put(R + 0x80, args[0])
            self.put(s + 0x20, P)
            self.put(P + 4, S)
            self.put(S + 4, T)
            self.put(T + 0xB4, args[1])
            self.put(P + 0x5C, args[2])
            self.put(P + 0x24, args[3])
            self.emu.write(P + 0x60, b"\x01")
            return self.emit(0x00703FD0, s, [0], [0x28, 0x460, 0x880])
        if cmd == "ammo":
            self.put(s + 0x20, P)
            self.put(P + 4, S)
            self.put(S + 4, T)
            self.put(T + 0xB4, args[0])
            self.put(T + 0x70, args[1])
            self.put(T + 0xB8, args[2])
            self.put(P + 0x1C, args[3])
            self.put(s + 0x18, Q)
            self.put(Q + 0x24, R)
            return self.emit(0x00704050, s, [], [0x41C])
        if cmd == "door":
            self.put(s - 0x20, P)
            self.put(P + 8, args[0])
            self.put(s + 0x1C, args[1])
            self.put(s - 0x1C, Q)
            self.put(Q + 0x88, args[2])
            self.put(Q + 0x94, args[3])
            self.put(Q + 0x418, args[4])
            return self.emit(0x00716A30, s, [3], [])
        if cmd == "scan":
            self.put(s + 0xFC, args[0])
            self.put(s + 0x100, args[1])
            self.put(T + 4, 0x11)
            self.put(T + 8, 0x22)
            if len(args) > 2 and args[2] != 0:
                self.globals(limit=args[3])
                self.put(s + 0x10, U)
                self.emu.write(U + 0x65, bytes([args[4] & 0xFF]))
                self.put(U + 0x1C, args[5])
                self.put(U + 0x0C, args[6])
                self.put(U + 0x10, args[7])
                self.put(U + 0x54, (U + 0x80) if args[8] else 0)
                if args[8]:
                    self.emu.write(U + 0x80 + 0x65, b"\x00")
                return self.emit(0x00716AA0, s, [T, 1], [])
            return self.emit(0x00716AA0, s, [T, 0], [0xFC, 0x1A00, 0x1A04])
        if cmd == "trap":
            self.emu.write(s + 0x54, bytes([args[0] & 0xFF]))
            self.put(s + 4, P)
            self.put(P + 0x104, args[1])
            self.put(s + 8, Q)
            self.put(Q + 0x38, args[2])
            self.put(Q + 0x3C, args[3])
            self.put(Q + 0x40, args[4])
            self.nudge = len(args) > 5 and args[5] != 0
            return self.emit(0x007006C0, s, [0], [0x54, 0x638])
        if cmd == "onbit":
            self.emu.write(s + 0x45, bytes([args[0] & 0xFF]))
            self.put(P, args[2])
            self.put(s - 0x20, Q)
            return self.emit(0x00700F30, s, [args[1]], [])
        if cmd == "refresh":
            self.put(s, VT)
            self.emu.write(s + 0x291, bytes([args[0] & 0xFF]))
            return self.emit(0x0070EB30, s, [], [])
        if cmd == "ids":
            self.put(s - 0x168, P)
            self.put(P + 0x128, args[0])
            self.put(P + 0x12C, Q)
            self.put(Q, args[1])
            self.put(Q + 4, args[2])
            self.vector(VEC, args[3], args[4], args[5])
            return self.emit(0x0070EAB0, s, [VEC], [0x1A04, 0x1A20])
        if cmd == "ghost":
            self.globals(ghost=P)
            return self.emit(0x00706190, s, [], [0x18, 0x418, 0x2A00, 0x2A04, 0x2A08])
        if cmd == "seed":
            return self.emit(0x00712370, s, [args[0]], [])
        if cmd == "find":
            self.put(s - 0x60, P)
            self.put(P + 0xC0, args[0])
            self.globals(ghost=Q)
            return self.emit(0x0071C9F0, s, [], [])
        if cmd == "filter":
            self.put(s - 8, P)
            self.put(P + 0x418, Q)
            return self.emit(0x00711F10, s, [args[0], args[1], 0], [])
        if cmd == "upgrade":
            self.put(s - 0x20, P)
            self.put(P + 0xF8, args[0])
            self.put(s - 0x1C, Q)
            return self.emit(0x00711A40, s, [args[1], args[2]], [])
        if cmd == "bridge":
            self.put(XFER + 4, args[0])
            return self.emit(0x0071FDD0, s, [XFER], [0x4C])
        if cmd == "enclose":
            self.force_minor = as_signed(args[0])
            return self.emit(0x0071FE60, s, [XFER], [])
        if cmd == "shield":
            self.put(XFER + 4, args[0])
            self.force_minor = as_signed(args[1])
            return self.emit(0x00718120, s, [XFER], [])
        if cmd == "bombard":
            self.put(XFER + 4, args[0])
            self.put(s + 0x2C, args[1])
            self.put(s + 0x30, args[2])
            return self.emit(0x00714D10, s, [XFER], [])
        if cmd == "mxfer":
            self.force_minor = as_signed(args[0])
            return self.emit(0x00715C20, s, [XFER], [])
        if cmd == "beamx":
            return self.emit(0x0071F060, s, [XFER], [])
        if cmd == "lift":
            self.put(XFER + 4, args[0])
            self.put(s + 0x2C, U if args[1] else s + 0x2C)
            if args[1]:
                self.put(U, T if args[2] else s + 0x2C)
                if args[2]:
                    self.put(T, s + 0x2C)
            return self.emit(0x007155F0, s, [XFER], [])
        if cmd == "emotion":
            self.put(s + 0x78, args[0])
            self.put(s + 0x68, VEC)
            self.put(s + 0x6C, VEC + args[1])
            self.put(VEC, args[2])
            self.put(VEC + 4, args[3])
            return self.emit(0x00714DC0, s, [], [0x6C, 0x78])
        if cmd == "clearid":
            self.put(s + 8, args[0])
            self.put(s + 0x0C, args[1])
            return self.emit(0x0070AFD0, s, [0], [0xC])
        if cmd == "power":
            self.put(s + 4, P)
            self.put(P + 0x38, args[0])
            self.put(P + 0x10, args[1])
            self.put(P + 0x18, args[2])
            self.put(Q + 0x3CC, args[3])
            self.put(Q + 4, R)
            self.put(R + 0xCC, args[4])
            self.globals(player=S)
            return self.emit(0x00712730, s, [Q], [])
        if cmd == "prone":
            self.globals(ready=args[0], value=args[1])
            self.put(s + 8, P)
            self.put(T + 0x8C, args[2])
            self.put(U + 0x24, args[3])
            self.put(U + 4, S)
            self.put(S + 8, args[4])
            result = self.emit(0x00714240, s, [T, 0], [])
            self.put(s + 0x3F0, self.get(0x00DF98F4))
            self.put(s + 0x3F4, self.get(0x00DF98F0))
            result["mem"] = self.mem_text([0x3F0, 0x3F4, 0xE24])
            return result
        if cmd == "aim":
            self.put(s - 0x1C, P if args[0] else 0)
            self.put(Q, args[1])
            self.put(Q + 4, args[2])
            self.put(Q + 8, args[3])
            self.put(P + 0x38, 0x10)
            self.put(P + 0x3C, 0x20)
            self.put(P + 0x40, 0x30)
            return self.emit(0x0071FA60, s, [Q, args[4]], [0x34, 0x40, 0x60])
        if cmd == "sphere":
            return self.emit(0x00717D30, s, [], [])
        raise RuntimeError(cmd)


def cases() -> list[str]:
    v = f"{VEC:#x}"
    node = f"{NODE:#x}"
    p = f"{P:#x}"
    q = f"{Q:#x}"
    out: list[str] = []
    out += [
        f"mask",
        f"adj4 {SELF:#x}",
        "adj4 0xc",
        f"adj0 {SELF:#x}",
        "adj0 0xc",
        "sameb 4 4",
        "sameb 4 5",
        "samew 7 7",
        "samew 7 8",
        "span 0x11223344 0x11223344",
        "span 0x11223344 0x11223345",
        "span 0 1",
        f"fwd 1 2 3 4 5 0x3f800000 q 0x41 0x42",
        "armed 0",
        "armed 1 q 0x43",
        f"unset 0 0 q {p} 0x44",
        f"unset 0 1 q {p}",
        "unset 5 0 q " + p + " 0x45",
        "maskif 0x2a q 0",
        "maskif 0x2a q 0x101",
        "relay 0",
        "relay 1 q 0x46",
        "inside 3 4 q 0",
        "inside 3 4 q 1",
        f"pushid {v} {v} {int(VEC + 0x10):#x} 0x55",
        f"pushid {v} {v} {v} 0x56 q 0x57",
        f"pushid {v} 0 4 0x58",
        f"clearrec {v} {v}",
        f"clearrec {v} {int(VEC + 0x20):#x} q 0x59",
        "stance 0x3f000000 0x3f800000 0x40000000 1 0 0 0 0x3f800000 q 0x10 0x11 0x12",
        "stance 0x3f800000 0x3f000000 0x40000000 1 0 0 0 0xbf800000 q 0x10 0x11 0x12",
        f"stance 0x3f000000 0x3fc00000 0x3f800000 7 4 1 9 0xbf800000 1 q 0x10 0x21 0x13 0x14 0x15",
        "stance 0x3f000000 0x3f800000 0x40000000 1 4 0 0 0x7fc00000 q 0x10 0x11 0x12",
        "latch 0 0 q 0x60",
        "latch 1 0x40000000 q 0x61",
        "latch 1 0 q 0x62 0x63",
        "motion 1 0",
        "motion 0 0 q 0x64",
        "motion 0 8 q 0x65 0x66",
        "poll 0 0 0 0 0 q 0x70",
        "poll 1 1 1 0 0 q 0x71",
        f"poll 1 0 0 0 0 q 0x72 {S:#x}",
        f"poll 1 0 2 1 0x4000 q 0x73 {S:#x}",
        "fire 0 1 0 1 q 0x74",
        "fire 0x4000 5 0 0 q 0x75 0x76 0x77 0x78",
        "fire 0 5 1 0xffffffff q 0x79",
        "ammo 2 1 0 0 q 0x80",
        "ammo 0 0 0 0 q 0x81 0x82",
        "ammo 0 0xffffffff 0 0 q 0x83",
        "ammo 0 5 0 0 q 0x84",
        "ammo 0 5 1 0 q 0x85 0x86",
        "door 0 0 0 0 0",
        "door 5 9 0 0 0",
        "door 5 1 2 0 0",
        "door 5 1 4 0 0",
        "door 5 1 0 0 0 q 1",
        f"door 5 1 0 0 {R:#x} q 0 1",
        f"door 5 1 0 0 {R:#x} q 0 0",
        f"scan {v} {int(VEC + 0x20):#x} 0",
        f"scan {v} {v} 0 q 0x87",
        f"scan 0 0 1 0x3f800000 0 0x3f000000 1 4 0 q 0",
        f"scan 0 0 1 0x40000000 0 0x3f800000 1 4 0 q 1 0x88",
        f"scan 0 0 1 0x3f000000 0 0x3f800000 1 4 1 q 0",
        "trap 1 0 0 0 0",
        "trap 0 0 0x3f800000 0 0",
        "trap 0 0x10 0x3f800000 0 0 q 0x90",
        "trap 0 0x10 0x3f800000 0 0 1 q 0x91 0x92",
        "trap 0 0x10 0x80000000 0 0 q 0x93",
        "onbit 0 0 0",
        f"onbit 1 0 0 q {p}",
        f"onbit 1 0 1 q {p} 1",
        f"onbit 1 0 1 q {p} 0 0x94",
        "refresh 1 q 0x1 0x2 0",
        "refresh 0 q 0x1 0x2 1 0x95",
        f"ids 0 0 0 {int(VEC + 0x20):#x} {int(VEC + 0x20):#x} {int(VEC + 0x40):#x}",
        f"ids 1 0x21 1 {int(VEC + 0x20):#x} {int(VEC + 0x20):#x} {int(VEC + 0x40):#x}",
        f"ids 1 0x22 2 {int(VEC + 0x20):#x} {int(VEC + 0x20):#x} {int(VEC + 0x20):#x} q 0x96 0x97",
        f"ids 0 0 0 {v} {int(VEC + 0x404):#x} {int(VEC + 0x800):#x}",
        f"ghost q {node}",
        f"seed {q}",
        "find 0",
        f"find 0x10 q 0",
        f"find 0x10 q {p}",
        "filter 1 2 q 0",
        "filter 1 2 q 1 1",
        "filter 1 2 q 1 0x105",
        "upgrade 0x33 0 0x3f800000 q 0",
        f"upgrade 0x33 0 0x3f800000 q 1 0",
        f"upgrade 0x34 {q} 0x40000000 q 2 1",
        "bridge 0 q 0x1 0x2",
        "bridge 2 q 0x1 0x3 0x4 0x5",
        "enclose 0xffffffff q 0x1 0x2 0x3 0x4",
        "enclose 2 q 0x1 0x2 0x3 0x4",
        "shield 8 0xffffffff q 0x1 0x2 0x3",
        "shield 0 0xffffffff q 0x1 0x2 0x3 0x4 0x5 0x6 0x7 0x8 0x9 0xa 0xb",
        "shield 0 1 q 0x1 0x2 0x3 0x4 0x5 0x6 0x7 0x8 0x9 0xa",
        "bombard 0 0x10 0x20 q 0x1 0x2 0x3 0x4 0x5 0x6",
        "bombard 2 0x10 0x20 q 0x1 0x2 0x3 0x4 0x7 0x5 0x6",
        "mxfer 0xffffffff q 0x1 0x2 0x3 0x4 0x5 0x6 0x7",
        "mxfer 2 q 0x1 0x2 0x3 0x4 0x5",
        "beamx q 0x1 0x2 0x3 0x4 0x5 0x6 0x7 0x8",
        "lift 8 0 0 q 0x1 0x2 0x3 0x4",
        "lift 0 1 0 q 0x1 0x2 0x3 0x4 0x5",
        "lift 2 1 1 q 0x1 0x2 0x3 0x4 0x5 0x6 0x7 0x8",
        f"emotion 0 0 0 0 q 0xaa",
        f"emotion {p} 8 {q} 0 q 0xab 0xac 0xad 0xae",
        f"clearid {v} {v}",
        f"clearid {v} {int(VEC + 8):#x} q 0xaf",
        f"power 0 {T:#x} 0 0 0 q 0xb0",
        f"power 1 0 0 {U:#x} 0 q 1 2 0xb1",
        f"power 1 0 0 {U:#x} 0 q 1 0",
        f"power 0 0 {R:#x} 0 0 q 0 0xb2",
        f"power 0 0 {R:#x} 0 0x400 q 1",
        f"prone 0 0 0x40000000 0 0x3f800000 q 0x11 {U:#x}",
        f"prone 1 0x22 0x3f800000 5 0x3f800000 q {U:#x}",
        f"prone 0 0 0x3f800000 0 0x40000000 q 0x12 0",
        f"prone 0 0 0 1 0x3f800000 q 0x13 {U:#x}",
        "aim 0 1 2 3 4",
        "aim 1 0x100 0x200 0x300 9 q 0xb3",
        "sphere q 0 0xb4",
        "sphere q 1 0xb5",
    ]
    return out


def main() -> int:
    checker = Checker(EXE)
    oracle = Oracle()
    failed = 0
    try:
        for command in cases():
            try:
                left = checker.call(command)
                right = oracle.run(command)
            except Exception as error:  # noqa: BLE001
                print(f"FAIL {command}\n  error {error}")
                failed += 1
                continue
            if left != right:
                failed += 1
                print(f"FAIL {command}")
                print(f"  checker {left}")
                print(f"  oracle  {right}")
    finally:
        checker.close()
    print(f"mismatches={failed} cases={len(cases())}")
    return 1 if failed else 0


if __name__ == "__main__":
    sys.exit(main())
