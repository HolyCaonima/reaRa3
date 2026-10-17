"""Differential oracle for the 0x006C0000 object-module batch.

The Release checker prints log/mem/nax/ext. Unicorn runs the shipped bytes
with the same inputs and the same stub side effects. Raw eax is ignored
because heap pointers differ between the checker process and the emulator.
"""

from __future__ import annotations

import struct
import subprocess
import sys
from pathlib import Path

from unicorn import x86_const

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from emulate import Emulator  # noqa: E402

EXE = Path(__file__).resolve().parents[3] / "build" / "win32" / "bin" / "Release" / "ra3_obj6ccheck.exe"

CLOCK = 0x00DEBA64
REGISTRY = 0x00CC561C
WORLD = 0x00DFC190
CATALOG = 0x00DEBA80
IAT_ENTER = 0x00BBA1E4
IAT_LEAVE = 0x00BBA1E8


def u32(value: int) -> int:
    return value & 0xFFFFFFFF


def p32(value: int) -> bytes:
    return struct.pack("<I", u32(value))


def hx(value: int) -> str:
    return f"{u32(value):x}"


def hex32(value: int) -> str:
    return f"{u32(value):08x}"


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
        if line.startswith("err="):
            raise RuntimeError(f"checker {line} on: {command}")
        out = {}
        for token in line.split():
            key, value = token.split("=", 1)
            out[key] = value
        return out

    def close(self) -> None:
        try:
            if self.process.stdin is not None:
                self.process.stdin.write("quit\n")
                self.process.stdin.flush()
            self.process.wait(timeout=10)
        except OSError:
            self.process.kill()


class Oracle:
    def __init__(self) -> None:
        self.emu = Emulator()
        self.log: list[str] = []
        self.seq = 0
        self.usable = 1
        self.pair = 1
        self.disp = 1
        self.accept = 1
        self.delay = 3
        self.ready = 0
        self.embed = 1
        self.arm = 1
        self.magic = 0x55
        self.hit = 0
        self.list_count = 0
        self.match_index = 0xFFFFFFFF
        self.match_object = 0
        self.rect = [0, 0, 0, 100, 100]
        self.repl = [0, 0, 0]
        self.child = 0
        self.node = 0
        self.header = 0
        self.weapon = 0
        self.weapon_alt = 0
        self.template = 0
        self.object = 0
        self.sentinel = 0
        self.module = 0
        self.clock = 0
        self.registry = 0
        self.world = 0
        self.catalog = 0
        self.ext = "."
        self.mem_obj = 0
        self.repl_addr = 0
        self._bind()

    def _bind(self) -> None:
        e = self.emu
        self.fn = {
            "v1c": e.bind_stub(self.on_v1c, 4),
            "v34": e.bind_stub(self.on_v34, 0),
            "v38": e.bind_stub(self.on_v38, 4),
            "v18": e.bind_stub(self.on_v18, 8),
            "v28": e.bind_stub(self.on_v28, 4),
            "v3c": e.bind_stub(self.on_v3c, 8),
            "v5c": e.bind_stub(self.on_v5c, 4),
            "v80": e.bind_stub(self.on_v80, 8),
            "v84": e.bind_stub(self.on_v84, 0),
            "v88": e.bind_stub(self.on_v88, 4),
            "v90": e.bind_stub(self.on_v90, 0),
            "v98": e.bind_stub(self.on_v98, 4),
            "v154": e.bind_stub(self.on_v154, 8),
            "v20c": e.bind_stub(self.on_v20c, 0),
            "ln": e.bind_stub(self.on_count, 0),
            "la": e.bind_stub(self.on_at, 4),
        }
        pairs = (
            (0x006C0ED0, self.on_usable, 0),
            (0x006B67A0, self.on_steer, 12),
            (0x005AC4F0, self.on_place, 8),
            (0x007997B0, self.on_aim, 4),
            (0x007E3640, self.on_pair_a, 12),
            (0x006C59E0, self.on_apply59, 8),
            (0x007E3110, self.on_pair_b, 12),
            (0x006C5AE0, self.on_apply5a, 8),
            (0x007E3900, self.on_dispatch, 20),
            (0x0054F490, self.on_state, 4),
            (0x006A1770, self.on_weapon, 4),
            (0x006A1720, self.on_weapon_alt, 8),
            (0x006A6770, self.on_read_id, 4),
            (0x004DEE90, self.on_lookup, 4),
            (0x006C1C50, self.on_neg, 8),
            (0x006C1D60, self.on_one, 4),
            (0x0067F7C0, self.on_base, 4),
            (0x006D1ED0, self.on_bit, 8),
            (0x006D2020, self.on_alt, 8),
            (0x00AAFE10, self.on_find, 8),
            (0x009C9980, self.on_unlink_a, 0),
            (0x00B66F80, self.on_unlink_b, 0),
            (0x004166B0, self.on_release, 0),
            (0x004D36C0, self.on_alloc, 4),
            (0x00A21BE0, self.on_lock, 4),
            (0x0050B5F0, self.on_map, 8),
            (0x00A21C90, self.on_unlock, 4),
            (0x005B7DE0, self.on_accept, 12),
            (0x005A8230, self.on_delay, 4),
            (0x007E16A0, self.on_hide1, 4),
            (0x007E1670, self.on_hide2, 4),
            (0x004EDD70, self.on_kill, 4),
            (0x006C8020, self.on_tail, 0),
            (0x004D6960, self.on_schedule, 8),
            (0x00538530, self.on_ready, 0),
            (0x0052EFC0, self.on_fire, 8),
            (0x0085CEE0, self.on_point, 4),
        )
        for address, stub, cleanup in pairs:
            e.bind_function(address, stub, cleanup)
        self.enter_stub = e.bind_stub(self.on_enter, 4)
        self.leave_stub = e.bind_stub(self.on_leave, 4)

    def add(self, text: str) -> None:
        self.log.append(text)

    def next_seq(self) -> int:
        self.seq += 1
        return 0xA1000000 + self.seq

    def reset(self) -> None:
        self.emu.reset_data()
        self.emu.uc.reg_write(x86_const.UC_X86_REG_FPCW, 0x037F)
        self.log = []
        self.seq = 0
        self.ext = "."
        self.template = 0
        self.object = 0
        self.sentinel = 0
        self.module = 0
        self.weapon = 0
        self.weapon_alt = 0
        self.child = 0
        self.node = 0
        self.header = 0
        self.match_object = 0
        self.match_index = 0xFFFFFFFF
        self.hit = 0
        self.list_count = 0
        self.clock = 0
        self.registry = 0
        self.world = 0
        self.catalog = 0
        self.mem_obj = 0
        self.repl_addr = self.alloc(12)
        self.emu.write(IAT_ENTER, p32(self.enter_stub))
        self.emu.write(IAT_LEAVE, p32(self.leave_stub))

    def alloc(self, size: int) -> int:
        return self.emu.alloc(b"\x11" * size)

    def put(self, address: int, offset: int, value: int | None = None) -> None:
        if value is None:
            self.emu.write(address, p32(offset))
        else:
            self.emu.write(address + offset, p32(value))

    def get(self, address: int) -> int:
        return struct.unpack("<I", self.emu.read(address, 4))[0]

    def publish(self) -> None:
        self.put(CLOCK, self.clock)
        self.put(REGISTRY, self.registry)
        self.put(WORLD, self.world)
        self.put(CATALOG, self.catalog)

    def call(self, address: int, this: int, args: tuple[int, ...] = ()) -> int:
        self.publish()
        result = self.emu.call(address, args, ecx=this)
        return u32(result.eax)

    def classify(self, eax: int) -> str:
        if self.object and eax == self.object:
            return "O"
        if self.template and eax == self.template:
            return "T"
        if self.sentinel and eax == self.sentinel:
            return "S"
        if self.weapon and eax == self.weapon:
            return "W"
        if self.weapon_alt and eax == self.weapon_alt:
            return "A"
        if self.module and eax == self.module:
            return "M"
        return hex32(eax)

    def tokens(self, eax: int, offsets: list[int]) -> dict[str, str]:
        if offsets:
            mem = ",".join(f"{off:x}:{self.get(self.mem_obj + off):08x}" for off in offsets)
        else:
            mem = "."
        return {
            "log": ",".join(self.log) if self.log else ".",
            "mem": mem,
            "nax": self.classify(eax),
            "ext": self.ext,
        }

    def make_loco(self) -> int:
        vt = self.alloc(0x40)
        self.put(vt, 0x1C, self.fn["v1c"])
        self.put(vt, 0x34, self.fn["v34"])
        self.put(vt, 0x38, self.fn["v38"])
        loco = self.alloc(0x20)
        self.put(loco, 0, vt)
        return loco

    def make_chain(self) -> int:
        vt = self.alloc(0xA0)
        self.put(vt, 0x80, self.fn["v80"])
        self.put(vt, 0x84, self.fn["v84"])
        self.put(vt, 0x88, self.fn["v88"])
        self.put(vt, 0x90, self.fn["v90"])
        chain = self.alloc(0x20)
        self.put(chain, 0, vt)
        return chain

    def make_ai(self, flags: int) -> tuple[int, int, int, int, int]:
        self_ = self.alloc(0x500)
        obj = self.alloc(0x400)
        templ = self.alloc(0x100)
        loco = self.make_loco()
        dest = self.alloc(0x30)
        self.put(self_, 8, obj)
        self.put(self_, 0x3C, loco)
        self.put(self_, 0x214, dest)
        self.put(obj, 4, templ)
        self.put(obj, 0x328, 0)
        self.put(obj, 0x32C, 0)
        self.put(obj, 0x330, 0)
        self.put(obj, 0x374, 0)
        self.put(obj, 0x37C, 0)
        self.put(self_, 0x47C, 0)
        self.emu.write(self_ + 0x436, b"\x00")
        self.put(templ, 0xC0, flags)
        self.template = templ
        self.object = obj
        return self_, obj, templ, loco, dest

    def link_circular(self, sentinel: int, keys: list[int]) -> None:
        self.put(sentinel, 0, sentinel)
        self.put(sentinel, 4, sentinel)
        prev = sentinel
        for key in keys:
            node = self.alloc(0x10)
            self.put(node, 8, key)
            self.put(node, 0, sentinel)
            self.put(node, 4, prev)
            self.put(prev, 0, node)
            self.put(sentinel, 4, node)
            prev = node

    def keys_of(self, sentinel: int) -> str:
        node = self.get(sentinel)
        parts: list[str] = []
        guard = 0
        while node != sentinel and guard < 8:
            parts.append(hx(self.get(node + 8)))
            node = self.get(node)
            guard += 1
        return ".".join(parts) if parts else "."

    def on_usable(self, emu: Emulator) -> int:
        self.next_seq()
        self.add("use")
        return self.usable

    def on_steer(self, emu: Emulator) -> int:
        ret = self.next_seq()
        self.add(f"steer:{hx(emu.stack_arg(0))}:{hx(emu.stack_arg(1))}:{hx(emu.stack_arg(2))}")
        return ret

    def on_place(self, emu: Emulator) -> int:
        ret = self.next_seq()
        raw = u32(emu.stack_arg(0))
        scale = u32(emu.stack_arg(1))
        if raw < 0x10000:
            self.add(f"placei:{hx(raw)}:{hx(scale)}")
        else:
            self.add(f"placev:{hx(self.get(raw))}:{hx(self.get(raw + 4))}:{hx(self.get(raw + 8))}:{hx(scale)}")
        return ret

    def on_aim(self, emu: Emulator) -> int:
        ret = self.next_seq()
        self.add(f"aim:{hx(emu.stack_arg(0))}")
        return ret

    def on_pair_a(self, emu: Emulator) -> int:
        self.next_seq()
        self.add(f"pairA:{hx(emu.stack_arg(1))}:{hx(emu.stack_arg(2))}")
        return self.pair

    def on_pair_b(self, emu: Emulator) -> int:
        self.next_seq()
        self.add(f"pairB:{hx(emu.stack_arg(1))}:{hx(emu.stack_arg(2))}")
        return self.pair

    def on_apply59(self, emu: Emulator) -> int:
        ret = self.next_seq()
        self.add(f"a59:{hx(emu.stack_arg(0))}:{hx(emu.stack_arg(1))}")
        return ret

    def on_apply5a(self, emu: Emulator) -> int:
        ret = self.next_seq()
        self.add(f"a5a:{hx(emu.stack_arg(0))}:{hx(emu.stack_arg(1))}")
        return ret

    def on_dispatch(self, emu: Emulator) -> int:
        self.next_seq()
        record = u32(emu.stack_arg(1))
        self.add(
            f"disp:{hx(self.get(record))}:{hx(self.get(record + 4))}:"
            f"{hx(emu.stack_arg(2))}:{hx(emu.stack_arg(3))}:{hx(emu.stack_arg(4))}"
        )
        return self.disp

    def on_state(self, emu: Emulator) -> int:
        ret = self.next_seq()
        self.add(f"st:{hx(emu.stack_arg(0))}")
        return ret

    def on_weapon(self, emu: Emulator) -> int:
        self.next_seq()
        self.add(f"wep:{hx(emu.stack_arg(0))}")
        return self.weapon

    def on_weapon_alt(self, emu: Emulator) -> int:
        self.next_seq()
        self.add(f"wep2:{hx(emu.stack_arg(0))}:{hx(emu.stack_arg(1))}")
        return self.weapon_alt

    def on_read_id(self, emu: Emulator) -> int:
        self.next_seq()
        self.add("rid")
        slot = u32(emu.stack_arg(0))
        self.put(slot, 0x42)
        return slot

    def on_lookup(self, emu: Emulator) -> int:
        self.next_seq()
        self.add(f"look:{hx(emu.stack_arg(0))}")
        return self.header

    def on_neg(self, emu: Emulator) -> int:
        ret = self.next_seq()
        self.add(f"neg:{hx(emu.stack_arg(0))}:{hx(emu.stack_arg(1))}")
        return ret

    def on_one(self, emu: Emulator) -> int:
        ret = self.next_seq()
        self.add(f"one:{hx(emu.stack_arg(0))}")
        return ret

    def on_base(self, emu: Emulator) -> int:
        ret = self.next_seq()
        self.add("base")
        return ret

    def on_bit(self, emu: Emulator) -> int:
        ret = self.next_seq()
        self.add(f"bit:{hx(emu.stack_arg(0))}:{hx(emu.stack_arg(1))}")
        return ret

    def on_alt(self, emu: Emulator) -> int:
        ret = self.next_seq()
        self.add(f"alt:{hx(emu.stack_arg(0))}:{hx(emu.stack_arg(1))}")
        return ret

    def on_find(self, emu: Emulator) -> int:
        self.next_seq()
        ecx = u32(emu.register(x86_const.UC_X86_REG_ECX))
        out_b = u32(emu.stack_arg(0))
        self.add("find:1" if self.hit else "find:0")
        self.put(out_b, self.node if self.hit and self.node else ecx + 4)
        return 0

    def _node_key(self, emu: Emulator) -> int:
        return self.get(u32(emu.stack_arg(0)) + 8)

    def on_unlink_a(self, emu: Emulator) -> int:
        self.next_seq()
        self.add(f"ua:{hx(self._node_key(emu))}")
        return 0

    def on_unlink_b(self, emu: Emulator) -> int:
        self.next_seq()
        self.add(f"ub:{hx(self._node_key(emu))}")
        return 0

    def on_release(self, emu: Emulator) -> int:
        self.next_seq()
        self.add(f"rel:{hx(self._node_key(emu))}")
        return 0

    def on_alloc(self, emu: Emulator) -> int:
        self.next_seq()
        self.add(f"alloc:{hx(emu.stack_arg(0))}")
        return self.header

    def on_lock(self, emu: Emulator) -> int:
        ret = self.next_seq()
        self.add(f"lk:{hx(emu.stack_arg(0))}")
        return ret

    def on_map(self, emu: Emulator) -> int:
        self.next_seq()
        ecx = u32(emu.register(x86_const.UC_X86_REG_ECX))
        begin = self.get(ecx + 4)
        count = self.get(ecx + 8)
        end_value = self.get(begin + count * 4)
        key = self.get(u32(emu.stack_arg(1)))
        hit = key == self.magic and self.node != 0
        self.put(u32(emu.stack_arg(0)), self.node if hit else end_value)
        self.add(f"map:{hx(key)}:{1 if hit else 0}")
        return 0

    def on_unlock(self, emu: Emulator) -> int:
        ret = self.next_seq()
        self.add(f"ulk:{hx(emu.stack_arg(0))}")
        return ret

    def on_accept(self, emu: Emulator) -> int:
        self.next_seq()
        self.add(f"hit:{hx(emu.stack_arg(1))}:{hx(emu.stack_arg(2))}")
        return self.accept

    def on_delay(self, emu: Emulator) -> int:
        self.next_seq()
        self.add(f"dly:{hx(emu.stack_arg(0))}")
        return self.delay

    def on_hide1(self, emu: Emulator) -> int:
        ret = self.next_seq()
        ecx = u32(emu.register(x86_const.UC_X86_REG_ECX))
        obj = u32(emu.stack_arg(0))
        self.add(f"h1:{hx(self.get(ecx + 4))}:{hx(self.get(obj + 0x10))}")
        return ret

    def on_hide2(self, emu: Emulator) -> int:
        ret = self.next_seq()
        ecx = u32(emu.register(x86_const.UC_X86_REG_ECX))
        obj = u32(emu.stack_arg(0))
        self.add(f"h2:{hx(self.get(ecx + 4))}:{hx(self.get(obj + 0x10))}")
        return ret

    def on_kill(self, emu: Emulator) -> int:
        ret = self.next_seq()
        obj = u32(emu.stack_arg(0))
        self.add(f"kill:{hx(self.get(obj + 0x10))}")
        return ret

    def on_tail(self, emu: Emulator) -> int:
        ret = self.next_seq()
        self.add("tail")
        return ret

    def on_schedule(self, emu: Emulator) -> int:
        ret = self.next_seq()
        ecx = u32(emu.register(x86_const.UC_X86_REG_ECX))
        at = u32(emu.stack_arg(0))
        when = u32(emu.stack_arg(1))
        self.add(f"sch:{hx(self.get(ecx + 4))}:{hx(when)}:{hx(self.get(at - 0x20))}")
        return ret

    def on_ready(self, emu: Emulator) -> int:
        self.next_seq()
        ecx = u32(emu.register(x86_const.UC_X86_REG_ECX))
        self.add(f"rdy:{hx(self.get(ecx + 4))}")
        return self.ready

    def on_fire(self, emu: Emulator) -> int:
        ret = self.next_seq()
        ecx = u32(emu.register(x86_const.UC_X86_REG_ECX))
        target = u32(emu.stack_arg(0))
        self.add(f"fire:{hx(self.get(ecx + 4))}:{hx(self.get(target))}:{hx(emu.stack_arg(1))}")
        return ret

    def on_enter(self, emu: Emulator) -> int:
        self.next_seq()
        self.add(f"cs:{hx(emu.stack_arg(0))}")
        return 0

    def on_leave(self, emu: Emulator) -> int:
        ret = self.next_seq()
        self.add(f"cso:{hx(emu.stack_arg(0))}")
        return ret

    def on_point(self, emu: Emulator) -> int:
        ret = self.next_seq()
        self.add("pt")
        out = u32(emu.stack_arg(0))
        for index, value in enumerate(self.repl):
            self.put(out + index * 4, value)
        return ret

    def on_v1c(self, emu: Emulator) -> int:
        ret = self.next_seq()
        self.add(f"v1c:{hx(emu.stack_arg(0))}")
        return ret

    def on_v34(self, emu: Emulator) -> int:
        ret = self.next_seq()
        self.add("v34")
        return ret

    def on_v38(self, emu: Emulator) -> int:
        ret = self.next_seq()
        self.add(f"v38:{hx(emu.stack_arg(0))}")
        return ret

    def on_v18(self, emu: Emulator) -> int:
        self.next_seq()
        self.add(f"v18:{hx(emu.stack_arg(0))}:{hx(emu.stack_arg(1))}")
        return self.embed

    def on_v28(self, emu: Emulator) -> int:
        ret = self.next_seq()
        self.add("v28")
        out = u32(emu.stack_arg(0))
        for index, value in enumerate(self.rect):
            self.put(out + index * 4, value)
        return ret

    def on_v3c(self, emu: Emulator) -> int:
        self.next_seq()
        self.add("v3c")
        for index, value in enumerate(self.repl):
            self.put(self.repl_addr + index * 4, value)
        return self.repl_addr

    def on_v5c(self, emu: Emulator) -> int:
        ret = self.next_seq()
        self.add(f"v5c:{hx(emu.stack_arg(0))}")
        return ret

    def on_v80(self, emu: Emulator) -> int:
        ret = self.next_seq()
        self.add(f"v80:{hx(emu.stack_arg(0))}:{hx(emu.stack_arg(1))}")
        return ret

    def on_v84(self, emu: Emulator) -> int:
        self.next_seq()
        self.add("v84:1" if self.child else "v84:0")
        return self.child

    def on_v88(self, emu: Emulator) -> int:
        ret = self.next_seq()
        self.add(f"v88:{hx(emu.stack_arg(0))}")
        return ret

    def on_v90(self, emu: Emulator) -> int:
        ret = self.next_seq()
        self.add("v90")
        return ret

    def on_v98(self, emu: Emulator) -> int:
        ret = self.next_seq()
        arg = u32(emu.stack_arg(0))
        key = arg if arg < 0x10000 else self.get(arg + 0xDC)
        self.add(f"v98:{hx(key)}")
        return ret

    def on_v154(self, emu: Emulator) -> int:
        ret = self.next_seq()
        interior = u32(emu.stack_arg(0))
        self.add(f"v154:{hx(self.get(interior - 0x3F8))}:{hx(emu.stack_arg(1))}")
        return ret

    def on_v20c(self, emu: Emulator) -> int:
        self.next_seq()
        self.add("v20c")
        return self.arm

    def on_count(self, emu: Emulator) -> int:
        self.next_seq()
        self.add("ln")
        return self.list_count

    def on_at(self, emu: Emulator) -> int:
        self.next_seq()
        index = u32(emu.stack_arg(0))
        self.add(f"la:{hx(index)}")
        return self.match_object if index == self.match_index else 0

    def run(self, command: str) -> dict[str, str]:
        parts = command.split()
        mem_at = parts.index("mem") if "mem" in parts else len(parts)
        cmd = parts[0]
        args = [int(token, 0) for token in parts[1:mem_at]]
        offsets = [int(token, 0) for token in parts[mem_at + 1 :]]

        def need(count: int) -> bool:
            return len(args) >= count

        def arg(index: int) -> int:
            return args[index]

        self.reset()
        eax = 0
        if cmd == "latch" and need(2):
            self_ = self.alloc(0x120)
            self.emu.write(self_ + 0x10F, bytes([arg(0) & 0xFF]))
            self.put(self_, 0x40, arg(1))
            eax = self.call(0x006C8450, self_)
            self.mem_obj = self_
        elif cmd == "setbit" and need(2):
            self_ = self.alloc(0x120)
            index = arg(0)
            self.put(self_, ((index >> 5) * 4) + 0x100, arg(1))
            self.module = self_
            eax = self.call(0x006C8500, self_, (index,))
            self.mem_obj = self_
        elif cmd == "clrbit" and need(2):
            self_ = self.alloc(0x120)
            index = arg(0)
            self.put(self_, ((index >> 5) * 4) + 0x100, arg(1))
            self.module = self_
            eax = self.call(0x006C8530, self_, (index,))
            self.mem_obj = self_
        elif cmd == "linkflag" and need(2):
            block = self.alloc(0x40)
            self_ = block + 16
            primary = self.alloc(0xA0)
            self.put(self_ - 8, 0, primary)
            self.put(primary, 0x98, arg(1))
            eax = self.call(0x006D9A40, self_, (arg(0),))
            self.mem_obj = primary
        elif cmd == "linkbyte" and need(1):
            block = self.alloc(0x40)
            self_ = block + 16
            primary = self.alloc(0x40)
            self.put(self_ - 8, 0, primary)
            eax = self.call(0x006D9AA0, self_, (arg(0),))
            self.ext = "v" + hex32(self.get(self_ + 0x28))
            self.mem_obj = primary
        elif cmd == "release" and need(2):
            block = self.alloc(0x120)
            self_ = block + 16
            primary = self.alloc(0xA0)
            self.put(self_ - 8, 0, primary)
            self.put(primary, 0x94, arg(0))
            self.link_circular(self_ + 0xD4, args[2:])
            self.header = self.alloc(0x20)
            self.put(self.header, 0x10, 0)
            self.sentinel = self_ + 0xD4
            eax = self.call(0x006D9B80, self_, (arg(1),))
            freed = self.get(self.header + 0x10)
            freed_key = 0 if freed == 0 else self.get(freed + 8)
            self.ext = self.keys_of(self_ + 0xD4) + ":" + hex32(freed_key)
            self.mem_obj = primary
        elif cmd == "readbody" and need(2):
            self_ = self.alloc(0x130)
            self.emu.write(self_ + 0x11F, bytes([arg(0) & 0xFF]))
            self.put(self_, 0x50, arg(1))
            eax = self.call(0x006C84E0, self_)
            self.mem_obj = self_
        elif cmd == "applybody" and need(3):
            self_ = self.alloc(0x50)
            templ = self.alloc(0x40)
            host = self.alloc(0x10)
            self.put(self_, 4, templ)
            self.put(self_, 8, host)
            self.put(self_, 0x44, arg(0))
            self.put(templ, 0x30, arg(1))
            self.put(templ, 0x34, arg(2))
            eax = self.call(0x006C85E0, self_)
            self.mem_obj = self_
        elif cmd == "crate" and need(1):
            self_ = self.alloc(0x20)
            templ = self.alloc(0x80)
            self.put(self_, 4, templ)
            self.put(templ, 0x6C, arg(0))
            other = 0 if arg(0) == 0xFFFFFFFF else self.alloc(0x10)
            eax = self.call(0x006C89B0, self_, (other,))
            self.mem_obj = self_
        elif cmd == "followpos" and need(6):
            self.usable = arg(0)
            ai, obj, _templ, _loco, dest = self.make_ai(0)
            self.put(obj, 0x38, arg(1))
            self.put(obj, 0x3C, arg(2))
            self.put(obj, 0x40, arg(3))
            if arg(4) == 0:
                self.put(ai, 0x214, 0)
            eax = self.call(0x006CC0C0, ai, (arg(5), 7))
            self.ext = hex32(self.get(dest + 0x0C)) + hex32(self.get(dest + 0x10)) + hex32(self.get(dest + 0x14))
            self.mem_obj = ai
        elif cmd == "followobj" and need(3):
            self.usable = arg(0)
            ai, obj, _templ, _loco, dest = self.make_ai(0)
            self.emu.write(ai + 0x436, bytes([arg(1) & 0xFF]))
            self.put(obj, 0x38, 1)
            self.put(obj, 0x3C, 2)
            self.put(obj, 0x40, 3)
            eax = self.call(0x006CC150, ai, (arg(2), 8))
            self.ext = hex32(self.get(dest + 0x0C))
            self.mem_obj = ai
        elif cmd == "steer1b" and need(2):
            self.usable = arg(0)
            ai, *_rest = self.make_ai(0)
            eax = self.call(0x006CC4D0, ai, (arg(1), 4))
            self.mem_obj = ai
        elif cmd == "pair59" and need(3):
            self.pair = arg(0)
            ai, *_rest = self.make_ai(0)
            self.registry = self.alloc(0x10)
            eax = self.call(0x006CC530, ai, (arg(1), arg(2)))
            self.mem_obj = ai
        elif cmd == "pair5a" and need(3):
            self.pair = arg(0)
            ai, *_rest = self.make_ai(0)
            self.registry = self.alloc(0x10)
            eax = self.call(0x006CC570, ai, (arg(1), arg(2)))
            self.mem_obj = ai
        elif cmd == "weaponor" and need(4):
            self.usable = arg(0)
            self.disp = arg(1)
            ai, obj, *_rest = self.make_ai(0)
            self.registry = self.alloc(0x10)
            if arg(2) != 0:
                self.put(obj, 0x328, self.make_chain())
                if arg(3) != 0:
                    self.child = self.make_chain()
            eax = self.call(0x006CC5B0, ai, (0x21, 0x22))
            self.mem_obj = ai
        elif cmd == "loco" and need(3):
            self.usable = arg(0)
            ai, *_rest = self.make_ai(0)
            which = arg(1)
            table = {
                0x0E: 0x006CC670,
                0x2D: 0x006CC700,
                0x69: 0x006CC740,
                0x13: 0x006CC8F0,
                0x14: 0x006CC930,
            }
            eax = self.call(table.get(which, 0x006CCE40), ai, (arg(2), 9))
            self.mem_obj = ai
        elif cmd == "place2d" and need(2):
            self.usable = arg(0)
            ai, *_rest = self.make_ai(0)
            eax = self.call(0x006CC6B0, ai, (arg(1), 6))
            self.mem_obj = ai
        elif cmd == "aim" and need(3):
            self.usable = arg(0)
            ai, *_rest = self.make_ai(0)
            address = 0x006CC7F0 if arg(1) == 0x11 else 0x006CC8B0
            eax = self.call(address, ai, (arg(2), 3))
            self.mem_obj = ai
        elif cmd == "tail" and need(3):
            self.usable = arg(0)
            ai, *_rest = self.make_ai(0)
            address = 0x006CC830 if arg(1) == 0x29 else 0x006CC870
            eax = self.call(address, ai, (arg(2),))
            self.mem_obj = ai
        elif cmd == "notify" and need(4):
            ai, obj, *_rest = self.make_ai(0)
            if arg(0) != 0:
                self.put(obj, 0x328, self.make_chain())
            if arg(1) != 0:
                self.put(obj, 0x374, self.alloc(0x40))
            eax = self.call(0x006CC780, ai, (arg(2), arg(3)))
            self.mem_obj = ai
        elif cmd == "guard" and need(8):
            self.usable = arg(0)
            ai, *_rest = self.make_ai(arg(1))
            self.put(ai, 0x5C, arg(2))
            point = self.alloc(0x10)
            self.put(point, 0, arg(3))
            self.put(point, 4, arg(4))
            self.put(point, 8, arg(5))
            self.rect = [0, 0, 0, 100, 100]
            self.repl = [9, 8, 7]
            world = self.alloc(0x40)
            vt = self.alloc(0x40)
            self.put(vt, 0x28, self.fn["v28"])
            self.put(vt, 0x3C, self.fn["v3c"])
            self.put(world, 0, vt)
            self.world = world
            eax = self.call(0x006CC970, ai, (point, arg(7), arg(6)))
            self.mem_obj = ai
        elif cmd == "state18" and need(3):
            self.usable = arg(0)
            ai, *_rest = self.make_ai(arg(1))
            eax = self.call(0x006CCA90, ai, (arg(2), 5))
            self.mem_obj = ai
        elif cmd == "actor50" and need(4):
            self.usable = arg(0)
            ai, *_rest = self.make_ai(arg(1))
            self.put(ai, 0x5C, arg(2))
            actor = self.alloc(0xE0)
            self.put(actor, 0xDC, arg(3))
            eax = self.call(0x006CCAE0, ai, (actor, 4, 5))
            self.mem_obj = ai
        elif cmd == "point50" and need(4):
            self.usable = arg(0)
            ai, *_rest = self.make_ai(arg(1))
            self.put(ai, 0x5C, arg(2))
            point = self.alloc(0x10)
            self.put(point, 8, arg(3))
            eax = self.call(0x006CCB50, ai, (point, 4, 5))
            self.mem_obj = ai
        elif cmd == "point4f" and need(4):
            self.usable = arg(0)
            ai, *_rest = self.make_ai(arg(1))
            self.put(ai, 0x5C, 4)
            extra = 0
            if arg(2) != 0:
                extra = self.alloc(0xB0)
                self.put(ai, 0x47C, extra)
            self.repl = [3, 4, 5]
            point = 0
            if arg(3) != 0:
                point = self.alloc(0x10)
                self.put(point, 0, 11)
                self.put(point, 4, 12)
                self.put(point, 8, 13)
            eax = self.call(0x006CCBD0, ai, (0x70, 0x71, 0x72, point))
            if extra != 0:
                self.ext = hex32(self.get(extra + 0xA8))
            self.mem_obj = ai
        elif cmd == "move3c" and need(3):
            ai, obj, *_rest = self.make_ai(0)
            if arg(0) != 0:
                self.put(obj, 0x37C, self.alloc(0x10))
                if arg(1) != 0:
                    self.weapon = self.alloc(0x30)
                elif arg(2) != 0:
                    self.weapon_alt = self.alloc(0x30)
            target = 0 if arg(0) == 2 else 0x15
            eax = self.call(0x006CCCC0, ai, (target, 0x16, 0x17, 0x18))
            self.mem_obj = ai
        elif cmd == "roster" and need(4):
            ai, obj, *_rest = self.make_ai(arg(0))
            actor = self.alloc(0x20)
            actor_templ = self.alloc(0x10)
            self.put(actor, 4, actor_templ)
            self.put(actor_templ, 4, arg(1))
            if arg(2) != 0:
                self.put(obj, 0x374, self.alloc(0x30))
            self.list_count = arg(3)
            self.match_index = 0 if arg(3) > 0 else 0xFFFFFFFF
            self.match_object = actor
            self.header = self.alloc(0x20)
            list_vt = self.alloc(0x20)
            self.put(list_vt, 0, self.fn["ln"])
            self.put(list_vt, 0x0C, self.fn["la"])
            self.put(self.header, 0, list_vt)
            self.catalog = self.alloc(0x10)
            eax = self.call(0x006CCD40, ai, (actor, 0x2A))
            self.mem_obj = ai
        elif cmd == "walk" and need(3):
            ai, obj, *_rest = self.make_ai(arg(0))
            if arg(1) != 0:
                self.put(obj, 0x374, self.alloc(0x10))
            self.list_count = arg(2)
            self.match_index = 0xFFFFFFFF
            self.header = self.alloc(0x20)
            list_vt = self.alloc(0x20)
            self.put(list_vt, 0, self.fn["ln"])
            self.put(list_vt, 0x0C, self.fn["la"])
            self.put(self.header, 0, list_vt)
            self.catalog = self.alloc(0x10)
            actor = 0 if arg(1) == 2 else self.alloc(0x10)
            eax = self.call(0x006CCDD0, ai, (actor, 0, 0))
            self.mem_obj = ai
        elif cmd == "deploy" and need(4):
            self_ = self.alloc(0x40)
            vt = self.alloc(0x20)
            self.put(vt, 0x18, self.fn["v18"])
            self.put(self_, 0x24, vt)
            self.embed = arg(0)
            obj = self.alloc(0x380)
            self.object = obj
            passenger = self.alloc(0x220)
            pvt = self.alloc(0x220)
            self.put(pvt, 0x20C, self.fn["v20c"])
            self.put(passenger, 0, pvt)
            self.arm = arg(1)
            self.put(obj, 0x374, 0)
            if arg(2) != 0:
                self.put(obj, 0x374, passenger)
                component = self.alloc(0x40)
                self.put(passenger, 0x214, component)
                self.put(component, 0x38, arg(3))
            eax = self.call(0x006CB3C0, self_, (obj,))
            self.mem_obj = self_
        elif cmd == "horde" and need(2):
            self_ = self.alloc(0x200)
            face = self.alloc(0x160)
            self.put(face, 0x98, self.fn["v98"])
            self.put(face, 0x154, self.fn["v154"])
            self.put(self_, 0x24, face)
            obj = self.alloc(0x400)
            self.put(obj, 0, 0x51)
            self.put(self_, 8, obj)
            member = self.alloc(0xE0)
            self.put(member, 0xDC, arg(0))
            self.hit = arg(1)
            if self.hit:
                self.node = self.alloc(0x10)
                self.put(self.node, 0, 0x77)
                self.put(self.node, 8, 0x77)
            self.put(self_, 0x1C8, 3)
            eax = self.call(0x006C8E90, self_, (member,))
            self.mem_obj = self_
        elif cmd == "rmkey" and need(1):
            self_ = self.alloc(0x90)
            self.link_circular(self_ + 0x78, args[1:])
            self.put(self_, 0x84, len(args) - 1)
            eax = self.call(0x006C9CC0, self_, (arg(0),))
            self.ext = self.keys_of(self_ + 0x78)
            self.mem_obj = self_
        elif cmd == "passive" and need(4):
            self_ = self.alloc(0x20)
            templ = self.alloc(0x40)
            self.put(templ, 0, 0x61)
            self.put(self_, 4, templ)
            self.put(templ, 0x30, arg(0))
            listing = self.alloc(0x10)
            self.put(listing, 0, arg(1))
            self.put(listing, 4, arg(1) ^ 1)
            self.put(templ, 0x34, listing)
            self.put(templ, 0x18, arg(2))
            self.put(templ, 0x24, 0)
            clock = self.alloc(0x1E0)
            self.put(clock, 0x58, 10)
            self.put(clock, 0x1C4, arg(3))
            self.clock = clock
            target = self.alloc(0x380)
            self.put(target, 0, 0x44)
            self.put(target, 0x36C, 0)
            mode = arg(4) if need(5) else 0
            if mode != 0:
                actor = self.alloc(0x20)
                self.put(actor, 4, 0x64)
                self.put(target, 0x36C, actor)
                if mode == 2:
                    effect = self.alloc(0x20)
                    self.put(effect, 4, 0x65)
                    self.put(templ, 0x24, effect)
                    self.ready = arg(5) if need(6) else 0
            eax = self.call(0x006C72E0, self_, (0 if mode == 3 else target,))
            self.mem_obj = self_
        elif cmd == "relay" and need(1):
            self_ = self.alloc(0x20)
            vt = self.alloc(0x60)
            self.put(vt, 0x5C, self.fn["v5c"])
            self.put(self_, 0, vt)
            obj = self.alloc(0x420)
            self.put(self_, 8, obj)
            self.put(obj, 0x418, arg(0))
            eax = self.call(0x006D9540, self_)
            self.mem_obj = self_
        elif cmd == "spawn" and need(3):
            self_ = self.alloc(0x80)
            templ = self.alloc(0x60)
            self.put(self_, 4, templ)
            self.emu.write(templ + 0x50, bytes([arg(0) & 0xFF]))
            self.template = templ
            self.link_circular(self_ + 0x5C, [arg(1)] if arg(1) else [])
            clock = self.alloc(0x200)
            self.clock = clock
            buckets = self.alloc(0x10)
            end_node = self.alloc(0x10)
            self.put(buckets, 0, end_node)
            self.put(clock, 0xC8, buckets)
            self.put(clock, 0xCC, 0)
            self.magic = arg(1)
            if arg(2) != 0:
                self.node = self.alloc(0x20)
                actor = self.alloc(0x500)
                self.put(actor, 0x10, 0x81)
                actor_templ = self.alloc(0xD0)
                self.put(actor, 4, actor_templ)
                self.put(actor_templ, 0xC8, 0x02000000 if arg(2) == 2 else 0)
                comp = self.alloc(0x10)
                self.put(comp, 4, 0x82)
                self.put(actor, 0x418, comp)
                self.emu.write(actor + 0x4D7, bytes([0 if arg(2) == 3 else 1]))
                self.emu.write(actor + 0x471, bytes([1 if arg(2) == 4 else 0]))
                self.put(self.node, 4, actor)
            eax = self.call(0x006D7CB0, self_)
            self.mem_obj = self_
        elif cmd == "notehit" and need(4):
            self_ = self.alloc(0x50)
            info = self.alloc(0x30)
            self.put(info, 0x20, arg(0))
            self.put(info, 8, arg(1))
            if arg(2) != 0:
                self.put(self_, 0x3C, self.alloc(0x10))
            else:
                self.put(self_, 0x3C, 0)
            self.put(self_, 0x40, arg(3))
            self.put(self_, 0x28, 100)
            self.put(self_, 8, 0x19)
            clock = self.alloc(0x200)
            self.put(clock, 0x58, 50)
            self.clock = clock
            buckets = self.alloc(0x10)
            self.put(buckets, 0, self.alloc(0x8))
            self.put(clock, 0xC8, buckets)
            self.put(clock, 0xCC, 0)
            self.magic = arg(1)
            if arg(1) != 0:
                self.node = self.alloc(0x10)
                self.put(self.node, 4, 0x33)
            self.accept = arg(4) if need(5) else 1
            self.delay = 4
            eax = self.call(0x006D9AC0, self_, (info,))
            self.mem_obj = self_
        else:
            raise RuntimeError(f"unhandled command: {command}")
        return self.tokens(eax, offsets)


CASES = [
    "latch 1 0x55 mem 0x40 0x10f",
    "latch 0 0x55 mem 0x40 0x10f",
    "latch 0 0 mem 0x40",
    "setbit 0 0 mem 0x100 0x10c",
    "setbit 31 0x80000000 mem 0x100",
    "setbit 32 0 mem 0x104",
    "clrbit 0 1 mem 0x100 0x10c",
    "clrbit 31 0xffffffff mem 0x100",
    "clrbit 32 7 mem 0x104",
    "linkflag 0 0x100000 mem 0x98",
    "linkflag 0 0 mem 0x98",
    "linkflag 0 0x200000 mem 0x98",
    "linkflag 1 0 mem 0x98",
    "linkflag 1 0x100000 mem 0x98",
    "linkflag 0x100 0x100000 mem 0x98",
    "linkbyte 0",
    "linkbyte 1",
    "linkbyte 0x100",
    "release 0 1 1 2 mem 0x94",
    "release 0 9 1 2 mem 0x94",
    "release 0 1 mem 0x94",
    "release 0x4000 1 mem 0x94",
    "release 0x4000 9 3 mem 0x94",
    "readbody 1 0x42 mem 0x50",
    "readbody 0 0x42 mem 0x50",
    "readbody 0 0",
    "applybody 0 0 0",
    "applybody 0 5 0",
    "applybody 0 5 6",
    "applybody 0 0 6",
    "applybody 1 0 0",
    "applybody 1 5 0",
    "applybody 1 5 6",
    "applybody 2 0 0",
    "applybody 2 5 0",
    "applybody 2 0 6",
    "applybody 2 5 6",
    "crate 0xffffffff",
    "crate 0",
    "crate 0x70",
    "followpos 0 1 2 3 1 4 mem 0x50",
    "followpos 0x100 1 2 3 1 4 mem 0x50",
    "followpos 1 1 2 3 1 9 mem 0x50 0x190 0x430",
    "followpos 1 8 9 10 0 4",
    "followobj 0 0 0x11 mem 0x50",
    "followobj 1 1 0x11",
    "followobj 1 0 0x11 mem 0x50 0x190 0x430",
    "steer1b 0 3 mem 0x50",
    "steer1b 0x100 3",
    "steer1b 1 3 mem 0x50",
    "pair59 0 1 2",
    "pair59 0x100 1 2",
    "pair59 1 1 2",
    "pair5a 0 3 4",
    "pair5a 1 3 4",
    "weaponor 0 1 0 0",
    "weaponor 1 0 0 0 mem 0x50",
    "weaponor 1 1 1 0",
    "weaponor 1 1 1 1",
    "loco 0 0x0e 1",
    "loco 1 0x0e 1 mem 0x50",
    "loco 1 0x2d 2 mem 0x50",
    "loco 1 0x69 3 mem 0x50",
    "loco 0x100 0x13 4",
    "loco 1 0x13 4 mem 0x50",
    "loco 1 0x14 5 mem 0x50",
    "loco 1 0x38 6 mem 0x50",
    "place2d 0 5",
    "place2d 1 5 mem 0x50",
    "aim 0 0x11 7",
    "aim 1 0x11 7 mem 0x50",
    "aim 1 0x12 8 mem 0x50",
    "tail 0 0x29 1",
    "tail 0x100 0x29 1",
    "tail 1 0x29 1 mem 0x50",
    "tail 1 0x48 2 mem 0x50",
    "notify 0 0 1 2",
    "notify 1 0 0 3",
    "notify 1 0 1 3",
    "notify 1 1 1 4",
    "notify 1 1 0 4",
    "guard 0 0 0 50 50 1 0 2",
    "guard 1 0x04000000 0 50 50 1 0 2",
    "guard 1 0 0 50 50 1 0 2 mem 0x50 0x54 0x60 0x64",
    "guard 1 0 4 50 50 1 0 2 mem 0x5c 0x58",
    "guard 1 0 1 0 50 1 0 2 mem 0x60 0x64 0x68",
    "guard 1 0 1 150 50 1 0 2 mem 0x60",
    "guard 1 0 1 50 50 1 1 2 mem 0x60 0x50",
    "state18 0 9 0 mem 0x54",
    "state18 1 0x04000000 9",
    "state18 1 0 9 mem 0x50 0x54",
    "actor50 0 0 0 0xabc",
    "actor50 1 0x04000000 0 0xabc",
    "actor50 1 0 4 0xabc mem 0x5c 0x54 0x6c 0x50",
    "actor50 1 0 1 0xabc mem 0x58 0x6c",
    "point50 0 0 0 6",
    "point50 1 0x04000000 0 6",
    "point50 1 0 4 6 mem 0x5c 0x6c 0x70 0x50",
    "point50 1 0 0 6 mem 0x58",
    "point4f 0 0 0 0",
    "point4f 1 0x04000000 0 0",
    "point4f 1 0 0 0 mem 0x60 0x64 0x68 0x74",
    "point4f 1 0 1 0 mem 0x74",
    "point4f 1 0 1 1 mem 0x60 0x64 0x68",
    "point4f 1 0 0 1 mem 0x60",
    "move3c 2 0 0",
    "move3c 0 0 0 mem 0x50",
    "move3c 1 1 0 mem 0x50",
    "move3c 1 0 1",
    "move3c 1 0 0",
    "roster 0x04000000 2 1 1",
    "roster 0 2 0 1",
    "roster 0 2 1 0",
    "roster 0 2 1 1",
    "roster 0 0 1 2",
    "walk 0 2 0",
    "walk 0x04000000 1 1",
    "walk 0 0 1",
    "walk 0 1 0",
    "walk 0 1 3",
    "deploy 0 1 1 0x3e4ccccd",
    "deploy 0x100 1 1 0x3e4ccccd",
    "deploy 1 1 0 0",
    "deploy 1 1 1 0x3e4ccccd",
    "deploy 1 0 1 0",
    "deploy 1 0 1 0x3d4ccccd",
    "deploy 1 1 1 0x3d4ccccd",
    "deploy 1 1 1 0",
    "horde 0x21 0 mem 0x1c8",
    "horde 0x21 1 mem 0x1c8",
    "rmkey 5 1 2 mem 0x84",
    "rmkey 1 1 2 mem 0x84",
    "rmkey 1 1 1 mem 0x84",
    "rmkey 2",
    "passive 0 1 5 0x40000000 3",
    "passive 0 1 5 0x40000000",
    "passive 1 7 5 0x40000000",
    "passive 2 7 5 0x40000000",
    "passive 1 7 5 0x40000000 1",
    "passive 1 7 5 0x3f800000 2 1",
    "passive 1 7 -3 0x3f800000 2 0",
    "passive 1 7 1 0x3f000000 1",
    "relay 0x123",
    "spawn 0 1 1",
    "spawn 1 0 0",
    "spawn 1 4 0",
    "spawn 1 4 1",
    "spawn 1 4 2",
    "spawn 1 4 3",
    "spawn 1 4 4",
    "notehit 7 0 0 1 mem 0x28 0x40",
    "notehit 7 1 1 7 mem 0x28 0x40",
    "notehit 7 0 1 1 mem 0x28 0x40",
    "notehit 7 8 1 1 mem 0x28 0x40",
    "notehit 7 8 1 1 0 mem 0x40",
    "notehit 7 8 1 1 0x100 mem 0x40",
]


def main() -> int:
    checker = Checker(EXE)
    oracle = Oracle()
    mismatches = 0
    try:
        for command in CASES:
            try:
                left = checker.call(command)
                right = oracle.run(command)
            except Exception as error:
                print(f"FAIL {command}")
                print(f"  error {error}")
                mismatches += 1
                continue
            bad = [key for key in ("log", "mem", "nax", "ext") if left.get(key) != right.get(key)]
            if bad:
                mismatches += 1
                print(f"FAIL {command}")
                for key in bad:
                    print(f"  {key} checker={left.get(key)} oracle={right.get(key)}")
    finally:
        checker.close()
    print(f"obj6c oracle mismatches={mismatches} cases={len(CASES)}")
    return 1 if mismatches else 0


if __name__ == "__main__":
    sys.exit(main())
