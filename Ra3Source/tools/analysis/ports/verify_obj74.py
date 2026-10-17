"""Differential oracle for the 0x0074 object-module batch.

The Release checker prints eax/log/mem tokens. Unicorn runs the shipped bytes
with the same inputs and the same stub side effects. 0x0076FF30 stays live:
Heal and Tunnel call that Horde body directly.
"""

from __future__ import annotations

import struct
import subprocess
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from emulate import Emulator  # noqa: E402

EXE = Path(__file__).resolve().parents[3] / "build" / "win32" / "bin" / "Release" / "ra3_obj74check.exe"
GAME_LOGIC = 0x00DEBA64
RAW_SLOT = 0x00BBA500
POP_SLOTS = {0x00, 0xB0, 0x10C}
POP_TAGS = {"6b0db0", "6b0bd0", "707050", "6fd820", "6b3200", "6a5900", "694000", "77ce10"}

# address, tag, stack args, cleanup, pop, produce
DIRECTS = [
    (0x00740690, "740690", 1, 4, False, False),
    (0x0074F0E0, "74f0e0", 1, 4, False, False),
    (0x0075D630, "75d630", 1, 4, False, False),
    (0x008D82F0, "8d82f0", 1, 4, False, False),
    (0x00685080, "685080", 1, 4, False, False),
    (0x006858E0, "6858e0", 1, 4, False, False),
    (0x007321F0, "7321f0", 1, 4, False, False),
    (0x007302E0, "7302e0", 1, 4, False, False),
    (0x0073F600, "73f600", 1, 4, False, False),
    (0x0072E810, "72e810", 1, 4, False, False),
    (0x0071D5A0, "71d5a0", 1, 4, False, False),
    (0x0073A190, "73a190", 1, 4, False, False),
    (0x0073AA60, "73aa60", 1, 4, False, False),
    (0x00741D00, "741d00", 0, 0, False, False),
    (0x006EBB60, "6ebb60", 0, 0, False, True),
    (0x00752C00, "752c00", 1, 4, False, False),
    (0x0075DF80, "75df80", 1, 4, False, False),
    (0x006DE680, "6de680", 1, 4, False, False),
    (0x0078D1E0, "78d1e0", 1, 4, False, False),
    (0x00691AB0, "691ab0", 1, 4, False, False),
    (0x006B0DB0, "6b0db0", 2, 8, True, False),
    (0x006B0BD0, "6b0bd0", 2, 8, True, False),
    (0x007704B0, "7704b0", 2, 8, False, False),
    (0x0074C7A0, "74c7a0", 1, 4, False, False),
    (0x006D1ED0, "6d1ed0", 2, 8, False, False),
    (0x005B9F30, "5b9f30", 0, 0, False, True),
    (0x007483A0, "7483a0", 3, 12, False, False),
    (0x00755FA0, "755fa0", 1, 4, False, False),
    (0x00755E80, "755e80", 1, 4, False, False),
    (0x0075A8F0, "75a8f0", 2, 8, False, False),
    (0x0073B550, "73b550", 0, 0, False, False),
    (0x007653F0, "7653f0", 0, 0, False, False),
    (0x007432E0, "7432e0", 0, 0, False, False),
    (0x00707050, "707050", 1, 4, True, False),
    (0x006FD820, "6fd820", 2, 8, True, False),
    (0x0071C890, "71c890", 2, 8, False, False),
    (0x00718B80, "718b80", 2, 8, False, False),
    (0x006D2090, "6d2090", 2, 8, False, False),
    (0x00775EA0, "775ea0", 0, 0, False, False),
    (0x006C74D0, "6c74d0", 0, 0, False, False),
    (0x0073A270, "73a270", 1, 4, False, False),
    (0x006A4FB0, "6a4fb0", 0, 0, False, False),
    (0x00701770, "701770", 0, 0, False, False),
    (0x006B3200, "6b3200", 0, 0, True, False),
    (0x006A5900, "6a5900", 2, 8, True, False),
    (0x00694000, "694000", 0, 0, True, False),
    (0x0077CE10, "77ce10", 0, 0, True, False),
    (0x006941F0, "6941f0", 0, 0, False, False),
    (0x0075EB00, "75eb00", 0, 0, False, False),
    (0x004D9AB0, "4d9ab0", 1, 4, False, False),
    (0x004D9AF0, "4d9af0", 1, 4, False, False),
    (0x00757200, "757200", 1, 4, False, False),
    (0x0072DC90, "72dc90", 3, 12, False, False),
    (0x0072DE40, "72de40", 3, 12, False, False),
    (0x0072E080, "72e080", 3, 12, False, False),
    (0x006C5680, "6c5680", 3, 12, False, False),
    (0x006C5710, "6c5710", 3, 12, False, False),
    (0x006C57A0, "6c57a0", 3, 12, False, False),
    (0x0054A090, "54a090", 2, 0, False, False),
    (0x0071D140, "71d140", 2, 0, False, False),
    (0x0073A470, "73a470", 2, 0, False, False),
    (0x004ED9C0, "4ed9c0", 3, 12, False, False),
]


def u32(value: int) -> int:
    return value & 0xFFFFFFFF


def p32(value: int) -> bytes:
    return struct.pack("<I", u32(value))


def parse(command: str) -> tuple[str, list[int], list[int], list[int]]:
    cmd, *tokens = command.split()
    phase = 0
    args: list[int] = []
    queue: list[int] = []
    mem: list[int] = []
    for token in tokens:
        if token == "q":
            phase = 1
            continue
        if token == "mem":
            phase = 2
            continue
        value = int(token, 0)
        if phase == 2:
            mem.append(value)
        elif phase == 1:
            queue.append(u32(value))
        else:
            args.append(u32(value))
    return cmd, args, queue, mem


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
        self.fill = False
        self.zero84 = False
        self.set84 = False
        self.out0 = 0
        self.out1 = 0
        self.produced = 0
        self.clock = 0
        self.obj = 0
        self.fn0: dict[int, int] = {}
        self.fn1: dict[int, int] = {}
        self._bind()

    def _bind(self) -> None:
        def virtual(slot: int, arity: int):
            def stub(emu: Emulator, slot: int = slot, arity: int = arity) -> int:
                return self.on_virtual(emu, slot, arity)
            return stub

        for slot in (0x00, 0x08, 0x0C, 0x10, 0x64, 0x78, 0xAC, 0xB0):
            self.fn0[slot] = self.emu.bind_stub(virtual(slot, 0), 0)
        for slot in (0x00, 0x04, 0x0C, 0x18, 0x20, 0x58, 0x5C, 0x60, 0x64, 0x6C, 0x70, 0x7C,
                     0x84, 0xAC, 0xB0, 0x10C, 0x110, 0x128):
            self.fn1[slot] = self.emu.bind_stub(virtual(slot, 1), 4)

        def direct(tag: str, nargs: int, pop: bool, produce: bool):
            def stub(emu: Emulator, tag: str = tag, nargs: int = nargs, pop: bool = pop,
                     produce: bool = produce) -> int:
                args = [emu.stack_arg(i) for i in range(nargs)]
                return self.on_direct(tag, args, pop, produce)
            return stub

        for address, tag, nargs, cleanup, pop, produce in DIRECTS:
            self.emu.bind_function(address, direct(tag, nargs, pop, produce), cleanup)

        def raw(emu: Emulator) -> int:
            args = [emu.stack_arg(i) for i in range(3)]
            return self.on_direct("rawmove", args, False, False)

        self.raw = self.emu.bind_stub(raw, 0)

    def on_direct(self, tag: str, args: list[int], pop: bool, produce: bool) -> int:
        text = "c" + tag
        for arg in args:
            text += self.arg_text(arg)
        self.add(text)
        if produce:
            return self.produced
        return self.popq() if pop or tag in POP_TAGS else 0

    def on_virtual(self, emu: Emulator, slot: int, arity: int) -> int:
        arg = emu.stack_arg(0) if arity else 0
        text = f"v{slot:x}"
        if arity:
            if slot == 0x18 and arg >= 0x10000:
                blob = emu.read(arg, 2)
                if blob[0] == 1 and blob[1] in (1, 2):
                    self.add(f"v18:m{blob[0]:02x}{blob[1]:02x}")
                    return 0
            if slot == 0x110 and self.fill and arg >= 0x10000:
                emu.write(arg, p32(self.out0) + p32(self.out1))
            if slot == 0x84 and arg >= 0x10000:
                if self.zero84:
                    emu.write(arg, b"\x00")
                elif self.set84:
                    emu.write(arg, b"\x01")
            text += self.arg_text(arg)
        self.add(text)
        if slot == 0x128:
            return self.produced
        return self.popq() if slot in POP_SLOTS else 0

    def arg_text(self, arg: int) -> str:
        arg = u32(arg)
        if arg < 0x10000:
            return f":{arg:x}"
        if arg == 0x00C2240C:
            return ":c2240c"
        return ":p"

    def add(self, text: str) -> None:
        self.log.append(text)

    def popq(self) -> int:
        if self.qi >= len(self.queue):
            return 0
        value = self.queue[self.qi]
        self.qi += 1
        return value

    def alloc(self, size: int) -> int:
        return self.emu.alloc(b"\x11" * size)

    def put(self, address: int, value: int) -> None:
        self.emu.write(address, p32(value))

    def get(self, address: int) -> int:
        return struct.unpack("<I", self.emu.read(address, 4))[0]

    def begin(self, queue: list[int]) -> None:
        self.emu.reset_data()
        self.log = []
        self.queue = list(queue)
        self.qi = 0
        self.fill = False
        self.zero84 = False
        self.set84 = False
        self.out0 = 0
        self.out1 = 0
        self.produced = 0
        self.emu.write(RAW_SLOT, p32(self.raw))
        self.clock = self.alloc(0x80)
        self.put(self.clock + 0x58, 0x3C)
        self.emu.write(GAME_LOGIC, p32(self.clock))
        self.obj = self.alloc(0xA00)

    def vt(self, slots: list[tuple[int, int]], size: int = 0x140) -> int:
        table = self.alloc(size)
        for offset, fn in slots:
            self.put(table + offset, fn)
        return table

    def xfer(self, flags: int, extra: list[tuple[int, int]]) -> int:
        table = self.vt([(0x18, self.fn1[0x18]), *extra])
        block = self.alloc(0x20)
        self.put(block, table)
        self.put(block + 4, flags)
        return block

    def emit(self, address: int, ecx: int, stack: list[int], base: int, mem: list[int],
             mix) -> dict[str, str]:
        eax = u32(self.emu.call(address, stack, ecx=ecx).eax)
        out = {
            "eax": f"{eax:08x}",
            "log": ",".join(self.log) if self.log else ".",
            "mem": self.mem_text(base, mem),
        }
        if mix is not None:
            out["mix"] = mix(eax)
        return out

    def mem_text(self, base: int, offsets: list[int]) -> str:
        if not offsets:
            return "."
        parts = [f"{offset:x}:{self.get(base + offset):08x}" for offset in offsets]
        return ",".join(parts)

    def passenger(self, flags: int) -> tuple[int, int]:
        body = self.alloc(0x20)
        tmpl = self.alloc(0x100)
        self.put(tmpl + 0xCC, flags)
        self.put(body + 4, tmpl)
        self.put(self.obj + 8, self.alloc(0x40))
        return body, tmpl

    def weapon_target(self, key: int) -> tuple[int, int]:
        target = self.alloc(0x430)
        holder = self.alloc(0x20)
        weapon = self.alloc(0x10)
        self.put(weapon, self.vt([(0xB0, self.fn1[0xB0])]))
        self.put(holder + 0x14, weapon)
        self.put(target + 0x310, 1)
        self.put(target + 0x3C0, 1)
        self.put(target + 0x3B4, holder)
        record = self.alloc(0x30)
        self.put(record + 0x20, key)
        self.put(target + 0x418, record)
        return target, record

    def run(self, command: str) -> dict[str, str]:
        cmd, args, queue, mem = parse(command)
        self.begin(queue)
        s = self.obj
        if cmd == "tcontain":
            self.put(s + 0x148, args[0])
            return self.emit(0x0074FB70, s, [self.xfer(0, [(0x70, self.fn1[0x70]), (0x6C, self.fn1[0x6C]),
                                                           (0x84, self.fn1[0x84])])], s, mem, None)
        if cmd == "hadd":
            body, tmpl = self.passenger(args[0])

            def mix(eax: int, bit: int = args[0] & 0x400, tmpl: int = tmpl) -> str:
                return "1" if (eax == 0 if bit else eax == tmpl) else "0"

            return self.emit(0x0076FE20, s, [body], s, mem, mix)
        if cmd == "hgxfer":
            return self.emit(0x0076FF30, s, [self.xfer(0, [(0x6C, self.fn1[0x6C])])], s, mem, None)
        if cmd == "sadd":
            body, tmpl = self.passenger(args[0])

            def mix(eax: int, bit: int = args[0] & 0x400, tmpl: int = tmpl) -> str:
                return "1" if (eax == 0 if bit else eax == tmpl) else "0"

            return self.emit(0x0076FF90, s, [body], s, mem, mix)
        if cmd == "gsxfer":
            return self.emit(0x007701E0, s, [self.xfer(0, [])], s, mem, None)
        if cmd == "hxfer":
            return self.emit(0x00770660, s, [self.xfer(0, [(0x6C, self.fn1[0x6C])])], s, mem, None)
        if cmd == "txfer":
            return self.emit(0x00770E20, s, [self.xfer(args[0], [(0x6C, self.fn1[0x6C]), (0x84, self.fn1[0x84])])],
                             s, mem, None)
        if cmd == "hrem":
            self.put(s, self.vt([(0x78, self.fn0[0x78])]))
            return self.emit(0x0077C910, s, [self.alloc(0x10)], s, mem, None)
        if cmd == "srem":
            self.put(s, self.vt([(0x78, self.fn0[0x78])]))
            return self.emit(0x0077C950, s, [self.alloc(0x10)], s, mem, None)
        if cmd == "rail":
            return self.emit(0x00740990, s, [self.xfer(0, [(0x64, self.fn1[0x64]), (0x70, self.fn1[0x70])])], s, mem, None)
        if cmd == "repair":
            return self.emit(0x00740A60, s, [self.xfer(args[0], [(0x64, self.fn1[0x64])])], s, mem, None)
        if cmd == "scenter":
            return self.emit(0x00740B00, s, [self.xfer(0, [])], s, mem, None)
        if cmd == "swh":
            return self.emit(0x00740B80, s, [self.xfer(0, [(0x70, self.fn1[0x70])])], s, mem, None)
        if cmd == "adep":
            self.put(s + 0x24, 0x1000)
            self.put(s + 0x28, 0x1008 if args[0] == 2 else 0x1000)
            flags = 0x8 if args[0] == 1 else (0x2 if args[0] == 2 else 0)
            return self.emit(0x00742030, s, [self.xfer(flags, [(0x6C, self.fn1[0x6C]), (0x84, self.fn1[0x84])])],
                             s, mem, None)
        if cmd == "cluster":
            self.put(s + 0x24, 0x11)
            self.put(s + 0x28, 0 if args[0] == 3 else 7)
            self.put(s + 0x30, 0x2000)
            self.put(s + 0x34, 0x2008 if args[0] == 4 else 0x2000)
            self.zero84 = args[0] == 1
            self.set84 = args[0] == 3
            flags = 0x2 if args[0] == 4 else 0
            extra = [(0x04, self.fn1[0x04]), (0x08, self.fn0[0x08]), (0x0C, self.fn1[0x0C]),
                     (0x20, self.fn1[0x20]), (0x84, self.fn1[0x84])]
            return self.emit(0x00742220, s, [self.xfer(flags, extra)], s, mem, None)
        if cmd == "dmg":
            return self.emit(0x00742370, s, [self.xfer(0, [])], s, mem, None)
        if cmd == "struck":
            data = self.alloc(0x10)
            self.put(data, 0x41)
            self.put(data + 4, 0x42)
            self.put(s + 0x54C, data)
            self.put(s + 0x550, data + args[1] * 4)
            self.put(s + 0x520, 0x19)
            extra = [(0x20, self.fn1[0x20]), (0x58, self.fn1[0x58]), (0x6C, self.fn1[0x6C]), (0x84, self.fn1[0x84])]
            return self.emit(0x00740170, s, [self.xfer(args[0], extra)], s, mem, None)
        if cmd == "osxfer":
            return self.emit(0x00742DC0, s, [self.xfer(0, [(0x84, self.fn1[0x84])])], s, mem, None)
        if cmd == "ocl":
            data = self.alloc(0x10)
            self.put(data, 1)
            self.put(data + 4, 2)
            self.put(s + 0xB8, data)
            self.put(s + 0xBC, data + args[1] * 4)
            return self.emit(0x00742E00, s, [self.xfer(args[0], [(0x6C, self.fn1[0x6C])])], s, mem, None)
        if cmd == "repxfer":
            return self.emit(0x00743140, s, [self.xfer(args[0], [])], s, mem, None)
        if cmd == "subxfer":
            data = self.alloc(0x10)
            self.put(s + 0x30, data)
            self.put(s + 0x34, data + args[1] * 4)
            return self.emit(0x007450B0, s, [self.xfer(args[0], [(0x7C, self.fn1[0x7C]), (0x84, self.fn1[0x84])])],
                             s, mem, None)
        if cmd == "delay":
            return self.emit(0x00751280, s, [self.xfer(0, [(0x6C, self.fn1[0x6C])])], s, mem, None)
        if cmd == "goal":
            coord = self.alloc(0x10)
            self.put(coord, args[0])
            self.put(coord + 4, args[1])
            self.put(coord + 8, args[2])
            return self.emit(0x00740F60, s, [coord], s, mem, None)
        if cmd == "missile":
            self.put(self.clock + 0x58, args[1])
            self.put(s + 0xE0, args[0])
            self.put(s + 0xE4, 0x55)
            body = self.alloc(0x50)
            self.put(body + 0x38, 0x10)
            self.put(body + 0x3C, 0x20)
            self.put(body + 0x40, 0x30)
            self.put(s + 8, body)
            return self.emit(0x007428E0, s, [0x6], s, mem, None)
        if cmd == "latch":
            record = 0
            if args[0] == 0:
                self.emu.write(s + 0x50, b"\x01")
            elif args[0] == 2:
                target, record = self.weapon_target(0x77)
                body = self.alloc(0x430)
                owned = self.alloc(0x30)
                self.put(owned + 0x20, 0x77)
                self.put(body + 0x418, owned)
                self.put(s + 8, body)
                self.produced = target
            eax_box = {"record": record, "mode": args[0]}

            def mix(eax: int, box: dict = eax_box) -> str:
                if box["mode"] == 2:
                    return "1" if eax == box["record"] else "0"
                return "1" if eax == 0 else "0"

            return self.emit(0x007437E0, s, [], s, mem, mix)
        if cmd == "spawn":
            tmpl = self.alloc(0x60)
            self.emu.write(tmpl + 0x58, bytes([args[0] & 0xFF]))
            self.put(s + 4, tmpl)
            self.put(s + 0x6C, 0x2A)

            def mix(eax: int, tmpl: int = tmpl, mode: int = args[0]) -> str:
                return "1" if (eax == tmpl if mode == 0 else eax == 0) else "0"

            return self.emit(0x0074C870, s, [0, 0], s, mem, mix)
        if cmd == "mounted":
            obj = s + 0x100
            self.put(s + 8, obj)
            self.put(obj + 0x88, args[1])
            self.put(obj + 0xF4, s + 0x300 if args[0] else 0)
            return self.emit(0x00751A30, s, [], s, mem, None)
        if cmd == "dozer":
            if args[0] == 0:
                self.put(s + 0x5DC, 1)
                self.put(s + 0x51C, self.vt([(0x64, self.fn0[0x64])], 0x80))
            else:
                self.put(s + 0x5DC, 0)
                self.put(s + 0x538, self.alloc(0x10))
                self.produced = 0 if args[0] == 1 else self.alloc(0x10)
            return self.emit(0x00751E60, s, [], s, mem, None)
        if cmd == "srelay":
            if args[0] == 0:
                self.produced = 0
            else:
                target, _record = self.weapon_target(0x21)
                self.produced = target
            return self.emit(0x00755190, s, [0x9], s, mem, None)
        if cmd == "tech9":
            self.put(s + 0x14, self.vt([(0x00, self.fn0[0x00])]))
            obj = self.alloc(0x430)
            self.put(obj + 0x418, 0x42)
            self.put(s + 0x0C, obj)
            return self.emit(0x007671B0, s + 4, [], s, mem, None)
        if cmd == "tech10":
            self.put(s + 0x14, self.vt([(0x00, self.fn0[0x00])]))
            obj = self.alloc(0x280)
            self.put(obj + 0x26C, args[0])
            self.put(s + 0x0C, obj)
            return self.emit(0x007671E0, s + 4, [args[1], args[2]], s, mem, None)
        if cmd == "laser":
            self.put(self.clock + 0x58, 0x10)
            self.put(s + 8, self.alloc(0x10))
            tmpl = self.alloc(0x30)
            self.put(tmpl + 0x14, args[1])
            self.put(tmpl + 0x1C, args[2])
            self.put(s + 4, tmpl)
            return self.emit(0x0076CFE0, s, [args[0], 0], s, mem, None)
        if cmd == "grant":
            obj = 0 if args[0] == 0 else self.alloc(0x40)
            self.put(s + 8, obj)
            tmpl = self.alloc(0x100)
            self.put(tmpl + 0xF8, 0 if args[0] == 1 else 0x15)
            self.put(s + 4, tmpl)

            def mix(eax: int, mode: int = args[0], tmpl: int = tmpl) -> str:
                if mode == 0:
                    ok = eax == 0
                elif mode == 1:
                    ok = eax == tmpl
                elif mode in (2, 3):
                    ok = eax == 1
                else:
                    ok = eax == 0
                return "1" if ok else "0"

            return self.emit(0x00772430, s, [], s, mem, mix)
        if cmd == "loco":
            obj = s + 0x100
            tmpl = self.alloc(0xD0)
            self.put(s + 8, obj)
            self.put(s + 4, tmpl)
            self.put(obj + 0x88, args[1])
            for slot in range(7):
                self.put(tmpl + 0xA8 + slot * 4, 0)
            self.put(tmpl + 0xA8, args[2])
            return self.emit(0x00774BB0, s, [0x3, args[0]], s, mem, None)
        if cmd == "deploy":
            obj = self.alloc(0xD0)
            self.put(s + 8, obj)
            self.put(s, self.vt([(0xAC, self.fn1[0xAC]), (0xB0, self.fn1[0xB0])]))
            self.put(s + 0x2C, args[0])
            self.put(obj + 0xC0, args[1])

            def mix(eax: int, obj: int = obj, mode: int = args[0]) -> str:
                return "1" if (eax == obj if mode != 1 else eax == 0) else "0"

            return self.emit(0x00775CB0, s, [], s, mem, mix)
        if cmd == "hidden":
            obj = self.alloc(0x90)
            self.put(s + 8, obj)
            self.put(s, self.vt([(0xAC, self.fn0[0xAC]), (0xB0, self.fn0[0xB0])]))
            self.put(s + 0x2C, args[0])
            self.put(obj + 0x80, args[1])
            return self.emit(0x00775CF0, s, [], s, mem, None)
        if cmd == "tclear":
            self.put(s + 0xBC, args[0])
            return self.emit(0x00776280, s, [], s, mem, None)
        if cmd == "ostatus":
            self.emu.write(s + 0xB8, bytes([args[0] & 0xFF]))
            self.put(s + 4, self.alloc(0x120))
            self.put(s + 8, self.alloc(0x10))
            return self.emit(0x007784C0, s, [], s, mem, None)
        if cmd == "mground":
            link = self.alloc(0x30)
            self.put(s + 0x18, link)
            self.put(link + 0x24, self.alloc(0x10))
            self.produced = 0 if args[0] == 1 else self.alloc(0x10)
            return self.emit(0x0077CE90, s, [], s, mem, None)
        if cmd in ("t45", "t49", "t52"):
            obj = self.alloc(0x340)
            self.put(s + 8, obj)
            contain = self.alloc(0x20)
            self.put(contain, self.vt([(0xB0, self.fn0[0xB0]), (0x110, self.fn1[0x110])]))
            if args[0] == 0:
                self.put(obj + 0x328, 0)
                self.put(obj + 0x32C, 0)
                self.put(obj + 0x330, 0)
            elif args[0] == 1:
                data = self.alloc(0x10)
                self.put(data, contain)
                self.put(obj + 0x328, 0)
                self.put(obj + 0x32C, data)
                self.put(obj + 0x330, data + 4)
            else:
                self.put(obj + 0x328, contain)
            head = self.alloc(0x10)
            node = self.alloc(0x10)
            rider = self.alloc(0x380)
            module = self.alloc(0x30)
            self.put(head, head if args[0] == 4 else node)
            self.put(node, head)
            self.put(node + 8, rider)
            self.put(rider + 0x374, 0 if args[0] == 5 else module)
            first = self.alloc(0x10)
            self.put(first, self.vt([(0x00, self.fn1[0x00])]))
            self.fill = args[0] >= 3
            self.out1 = head
            self.out0 = first if args[0] == 6 else 0
            va = {"t45": 0x007402E0, "t49": 0x007403A0, "t52": 0x00740460}[cmd]
            return self.emit(va, s, [0x1, 0x2, 2 if args[0] == 2 else 0], s, mem, None)
        if cmd == "ppow":
            power = 0
            if args[0] == 0:
                self.put(s + 8, 0)
            else:
                obj = self.alloc(0x430)
                self.put(s + 8, obj)
                if args[0] == 1:
                    self.put(obj + 0x418, 0)
                else:
                    record = self.alloc(0x1330)
                    self.put(obj + 0x418, record)
                    if args[0] == 2:
                        self.put(record + 0x1320, 0)
                    else:
                        power = self.alloc(0x20)
                        self.put(record + 0x1320, power)

            def mix(eax: int, mode: int = args[0], power: int = power, base: int = s) -> str:
                ok = eax == 0
                stored = self.get(base + 0x40)
                if mode == 2:
                    ok = ok and stored == 0
                elif mode == 3:
                    ok = ok and stored == power
                else:
                    ok = ok and stored == 0x11111111
                return "1" if ok else "0"

            return self.emit(0x0074BE70, s, [], s, mem, mix)
        if cmd == "fling":
            obj = self.alloc(0x340)
            contain = self.alloc(0x20)
            self.put(s + 8, obj)
            self.put(obj + 0x328, contain)
            self.put(contain, self.vt([(0x10C, self.fn1[0x10C]), (0x128, self.fn1[0x128])]))
            unit = s + 0x200
            self.produced = unit
            self.emu.write(unit + 0x471, bytes([0 if args[0] in (2, 3) else 1]))
            self.emu.write(unit + 0xB2, bytes([1 if args[0] == 1 else 0]))
            self.put(unit + 0xB0, 0)
            self.put(unit + 0xBC, 0x80 if args[0] == 4 else 0)
            self.put(unit + 0x378, self.alloc(0x10) if args[0] == 2 else 0)
            mix = None
            if args[0] == 3:
                def mix(eax: int, unit: int = unit) -> str:
                    return "1" if eax == unit else "0"
            return self.emit(0x00772390, s, [], s, mem, mix)
        if cmd == "scatter":
            face = s + 0x20
            if args[0] == 0:
                self.put(face - 8, 0)
            else:
                obj = self.alloc(0x350)
                record = self.alloc(0x30)
                self.put(face - 8, obj)
                self.put(face - 12, record)
                self.emu.write(record + 0x29, bytes([1 if args[0] == 1 else 0]))
                self.emu.write(record + 0x28, bytes([1 if args[0] == 3 else 0]))
                self.put(record + 0x24, 7 if args[0] == 3 else 1)
                if args[0] == 4:
                    self.put(obj + 0x33C, 0)
                else:
                    weapon = self.alloc(0x10)
                    self.put(weapon, self.vt([(0x5C, self.fn1[0x5C]), (0x60, self.fn1[0x60])]))
                    self.put(obj + 0x33C, weapon)
                box = self.alloc(8)
                self.put(box, 0xFFFFFFFF if args[0] == 5 else 0x22)
                self.put(record + 8, 0 if args[0] == 6 else box)
            return self.emit(0x007446B0, face, [], face, mem, None)
        if cmd == "proj":
            self.put(self.clock + 0x58, 0x8)
            obj = self.alloc(0x50)
            self.put(obj + 0x38, 1)
            self.put(obj + 0x3C, 2)
            self.put(obj + 0x40, 3)
            self.put(s + 8, obj)
            shot = self.alloc(0x10)
            self.put(shot, self.vt([(0x10, self.fn0[0x10])]))
            self.put(s + 0x50, shot)
            source = self.alloc(0xE0)
            self.put(source + 0xDC, 0x44)
            self.put(source + 0x38, 5)
            self.put(source + 0x3C, 6)
            self.put(source + 0x40, 7)
            coord = self.alloc(0x10)
            self.put(coord, 0xA)
            self.put(coord + 4, 0xB)
            self.put(coord + 8, 0xC)
            return self.emit(0x00764BF0, s, [source if args[0] else 0, coord, args[1]], s, mem, None)
        raise RuntimeError(f"unknown command {cmd}")


CASES = [
    "tcontain 0x22 mem 0x148",
    "tcontain 0 mem 0x148",
    "hadd 0",
    "hadd 0x400",
    "hadd 0x800",
    "hgxfer",
    "hgxfer mem 0x770",
    "sadd 0",
    "sadd 0x400",
    "gsxfer",
    "gsxfer",
    "hxfer",
    "hxfer",
    "txfer 0",
    "txfer 8",
    "hrem q 0",
    "hrem q 1",
    "hrem q 0x100",
    "srem q 0",
    "srem q 1",
    "srem q 0x100",
    "rail",
    "rail",
    "repair 0",
    "repair 8",
    "scenter",
    "scenter",
    "swh",
    "swh",
    "adep 0 mem 0x28",
    "adep 1 mem 0x28",
    "adep 2 mem 0x28",
    "cluster 0 mem 0x34",
    "cluster 1 mem 0x34",
    "cluster 3 mem 0x34",
    "cluster 4 mem 0x34",
    "dmg",
    "dmg",
    "struck 8 0",
    "struck 0 0",
    "struck 2 2",
    "osxfer",
    "osxfer",
    "ocl 0 0",
    "ocl 0 2",
    "ocl 2 0",
    "repxfer 0",
    "repxfer 8",
    "subxfer 0 0",
    "subxfer 0 2",
    "subxfer 2 2",
    "delay",
    "delay",
    "goal 1 2 3 mem 0x60 0x64 0x68",
    "goal 0 0xffffffff 0x80000000 mem 0x60 0x64 0x68",
    "missile 0 0x3c mem 0xe0 0xe4 0x104 0x108 0x10c 0x120",
    "missile 1 0x3c mem 0xe0 0xe4 0x104 0x108 0x10c 0x120",
    "latch 0 mem 0x50",
    "latch 1 mem 0x50",
    "latch 2 mem 0x50 0x54",
    "spawn 0 mem 0x68",
    "spawn 1 mem 0x68",
    "mounted 0 0 mem 0x188 0x4bc",
    "mounted 1 0x2000 mem 0x188 0x4bc",
    "mounted 0 0x200000 mem 0x188 0x4bc",
    "dozer 0",
    "dozer 1",
    "dozer 2",
    "srelay 0",
    "srelay 1",
    "tech9 q 0x100",
    "tech9 q 1",
    "tech10 1 3 5 q 1",
    "tech10 0 3 5 q 1",
    "tech10 0 0 0 q 1",
    "tech10 0 3 0 q 0x100",
    "laser 4 0 0 mem 0x58",
    "laser 0 1 0 mem 0x58",
    "laser 1 2 3 mem 0x58",
    "grant 0",
    "grant 1",
    "grant 2 q 1",
    "grant 3 q 0 1",
    "grant 4 q 0 0",
    "loco 0 0 0 mem 0x188",
    "loco 0 0x40000000 0 mem 0x188",
    "loco 1 0 0 mem 0x188",
    "loco 1 0 1 mem 0x188",
    "deploy 0 0",
    "deploy 1 0x200",
    "deploy 1 0",
    "hidden 0 0",
    "hidden 1 0x40000",
    "hidden 1 0",
    "tclear 0 mem 0xbc",
    "tclear 5 mem 0xbc",
    "ostatus 0 mem 0xb8",
    "ostatus 1 mem 0xb8",
    "mground 0 q 1",
    "mground 1 q 0 1",
    "mground 2 q 0 1",
    "mground 3 q 0 0 1",
    "mground 4 q 0 0 0 1",
    "mground 5 q 0 0 0 0",
    "t45 0",
    "t45 1 q 0",
    "t45 2 q 1",
    "t45 3 q 1",
    "t45 4 q 1",
    "t45 5 q 1",
    "t45 6 q 1",
    "t49 0",
    "t49 1 q 0",
    "t49 2 q 1",
    "t49 3 q 1",
    "t49 5 q 1",
    "t49 6 q 1",
    "t52 0",
    "t52 2 q 1",
    "t52 4 q 1",
    "t52 5 q 1",
    "t52 6 q 1",
    "ppow 0",
    "ppow 1",
    "ppow 2",
    "ppow 3",
    "fling 0 q 0 mem 0x2b0 0x2bc",
    "fling 1 q 1 mem 0x2b0 0x2bc",
    "fling 2 q 1 mem 0x2b0 0x2bc",
    "fling 3 q 1 mem 0x2b0 0x2bc",
    "fling 4 q 1 mem 0x2b0 0x2bc",
    "scatter 0",
    "scatter 1",
    "scatter 2",
    "scatter 3",
    "scatter 4",
    "scatter 5",
    "scatter 6",
    "proj 0 0 mem 0x28 0x2c 0x30 0x34 0x38 0x3c 0x40 0x44 0x54",
    "proj 1 0 mem 0x28 0x2c 0x30 0x34 0x38 0x3c 0x40 0x44 0x54",
    "proj 0 7 mem 0x28 0x2c 0x30 0x34 0x38 0x3c 0x40 0x44 0x54",
]


def main() -> int:
    checker = Checker(EXE)
    oracle = Oracle()
    failed = 0
    try:
        for command in CASES:
            try:
                got = oracle.run(command)
            except Exception as exc:
                print(f"FAIL {command}\n  emu {exc}")
                failed += 1
                continue
            exp = checker.call(command)
            keys = ["log", "mem"]
            if "mix" in exp or "mix" in got:
                keys.append("mix")
            else:
                keys = ["eax", *keys]
            bad = [key for key in keys if exp.get(key) != got.get(key)]
            if bad or exp.get("log") == "ERR":
                failed += 1
                print(f"FAIL {command}")
                for key in ["eax", "log", "mem", "mix"]:
                    if exp.get(key) != got.get(key):
                        print(f"  {key}\n    cpp {exp.get(key)}\n    emu {got.get(key)}")
    finally:
        checker.close()
    print(f"mismatches={failed} cases={len(CASES)}")
    return 1 if failed else 0


if __name__ == "__main__":
    sys.exit(main())
