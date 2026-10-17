"""Release oracle for the named module-slot bodies in 0x006A0000-0x006BFFFF.

The checker is the reconstruction. This script is the shipped image. Callback
traces name the call and the module offset of any pointer it receives. EAX is
compared at full width, including AL-only residues. A watched span that changes
is a write-set failure.
"""

from __future__ import annotations

import struct
import subprocess
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from emulate import Emulator  # noqa: E402
from unicorn import x86_const as uc  # noqa: E402

ROOT = Path(__file__).resolve().parents[3]
EXE = Path(sys.argv[1]) if len(sys.argv) > 1 else ROOT / "build" / "win32" / "bin" / "Release" / "ra3_obj6acheck.exe"
GLOBAL_AUDIO = 0x00DF03FC

SLOTS = (
    (0x00C08A48, 30, 0x006A23A0, "EncloseBehavior"),
    (0x00C1F320, 1, 0x006A16A0, "WeaponSetUpdate"),
    (0x00C22E28, 32, 0x006B13F0, "SlaughterHordeContain"),
    (0x00C1AC90, 3, 0x006AED50, "ClearanceTestingSlowDeathBehavior"),
    (0x00C10AA0, 3, 0x006A1C80, "TechUpgrade"),
    (0x00BF7DD0, 3, 0x006B50A0, "DozerActionMoveToActionPosState"),
    (0x00BF7EA8, 3, 0x006B5130, "DozerActionStateMachine"),
    (0x00BF7D60, 3, 0x006B5050, "DozerActionPickActionPosState"),
    (0x00BF7E40, 3, 0x006B50E0, "DozerActionDoActionState"),
    (0x00BF7F70, 3, 0x006B5170, "DozerActionState"),
    (0x00C1F4E8, 3, 0x006BC4A0, "SubObjectsUpgrade"),
    (0x00C07BA4, 3, 0x006A3E90, "ExperienceTrackerObject"),
    (0x00BF9318, 3, 0x006A1AA0, "AudioLoopUpgrade"),
    (0x00C1A7D8, 3, 0x006B37C0, "CreateAndEnterObjectSpecialPower"),
    (0x00C1A4E0, 22, 0x006AFCB0, "VeterancyCrateCollide"),
    (0x00BF9580, 3, 0x006A2220, "TechUpgradeReceiver"),
)


def u32(value: int) -> bytes:
    return struct.pack("<I", value & 0xFFFFFFFF)


def dword(data: bytes, offset: int = 0) -> int:
    return struct.unpack_from("<I", data, offset)[0]


class Checker:
    def __init__(self, executable: Path) -> None:
        self.process = subprocess.Popen(
            [str(executable)], stdin=subprocess.PIPE, stdout=subprocess.PIPE, text=True, bufsize=1)

    def call(self, line: str) -> str:
        assert self.process.stdin is not None and self.process.stdout is not None
        self.process.stdin.write(line + "\n")
        self.process.stdin.flush()
        result = self.process.stdout.readline().strip()
        if not result or result.startswith("error"):
            raise RuntimeError(f"obj6acheck rejected {line!r}: {result!r}")
        return result

    def close(self) -> None:
        assert self.process.stdin is not None
        self.process.stdin.write("quit\n")
        self.process.stdin.flush()
        if self.process.wait(timeout=20) != 0:
            raise RuntimeError("obj6acheck failed")


class World:
    def __init__(self) -> None:
        self.queue: list[int] = []
        self.trace = ""
        self.bad = ""
        self.module = 0
        self.xfer = 0
        self.user = 0
        self.object = 0
        self.global_obj = 0
        self.weapons: list[int] = []

    def take(self) -> int:
        if not self.queue:
            self.bad = "return queue empty"
            return 0
        return self.queue.pop(0)

    def add(self, text: str) -> None:
        self.trace += text


def cstring(image, address: int) -> str:
    raw = image.read(address, 80)
    return raw[:raw.find(b"\x00")].decode("ascii")


def assert_slots(emu: Emulator) -> None:
    for vtable, slot, function, class_name in SLOTS:
        pointer = dword(emu.image.read(vtable + slot * 4, 4))
        if pointer != function:
            raise SystemExit(f"{vtable:08X} slot {slot} is {pointer:08X}, not {function:08X}")
        name_fn = dword(emu.image.read(vtable + 8, 4))
        code = emu.image.read(name_fn, 6)
        if code[0] != 0xB8 or code[5] != 0xC3:
            raise SystemExit(f"{vtable:08X} slot 2 is not a class-name return")
        if cstring(emu.image, dword(code, 1)) != class_name:
            raise SystemExit(f"{vtable:08X} slot 2 name is not {class_name}")


def bind_direct(emu: Emulator, world: World) -> None:
    def slaughter(machine: Emulator) -> int:
        ecx = machine.register(uc.UC_X86_REG_ECX)
        if ecx != world.module + 0x760 or machine.stack_arg(0) != world.module or machine.stack_arg(1) != world.user:
            world.bad = f"slaughter ecx={ecx:08X}"
        world.add("h6b0bd0;")
        return world.take()

    def helper_this(token: str):
        def stub(machine: Emulator) -> int:
            if machine.register(uc.UC_X86_REG_ECX) != world.module or machine.stack_arg(0) != world.xfer:
                world.bad = token.strip(";") + " args"
            world.add(token)
            return world.take()

        return stub

    def sub_local(machine: Emulator) -> int:
        if machine.register(uc.UC_X86_REG_ECX) != world.module or machine.stack_arg(0) != world.xfer:
            world.bad = "c6a1ea0 args"
        if bytes(machine.read(machine.stack_arg(1), 2)) != b"\x01\x01":
            world.bad = "c6a1ea0 version"
        world.add("c6a1ea0;")
        return world.take()

    def cdecl_ptr(machine: Emulator) -> int:
        offset = machine.stack_arg(1) - world.module
        if machine.stack_arg(0) != world.xfer:
            world.bad = "c54a090 xfer"
        world.add(f"c54a090:{offset:x};")
        return world.take()

    def sub_object(machine: Emulator) -> int:
        if machine.register(uc.UC_X86_REG_ECX) != world.module + 0x18 or machine.stack_arg(0) != world.xfer:
            world.bad = "c685080 args"
        world.add("c685080:18;")
        return world.take()

    def vet_gate(machine: Emulator) -> int:
        if machine.register(uc.UC_X86_REG_ECX) != world.module or machine.stack_arg(0) != world.object:
            world.bad = "h6908e0 args"
        world.add("h6908e0;")
        return world.take()

    def vet_team(machine: Emulator) -> int:
        if machine.register(uc.UC_X86_REG_ECX) != world.object:
            world.bad = "h6875a0 this"
        world.add("h6875a0;")
        return world.take()

    emu.bind_function(0x006B0BD0, slaughter, stack_cleanup=8)
    emu.bind_function(0x0068ECD0, helper_this("c68ecd0;"), stack_cleanup=4)
    emu.bind_function(0x006858E0, helper_this("c6858e0;"), stack_cleanup=4)
    emu.bind_function(0x006A3D10, helper_this("c6a3d10;"), stack_cleanup=4)
    emu.bind_function(0x00681220, helper_this("c681220;"), stack_cleanup=4)
    emu.bind_function(0x006A1EA0, sub_local, stack_cleanup=8)
    emu.bind_function(0x0054A090, cdecl_ptr, stack_cleanup=0)
    emu.bind_function(0x00685080, sub_object, stack_cleanup=4)
    emu.bind_function(0x006908E0, vet_gate, stack_cleanup=4)
    emu.bind_function(0x006875A0, vet_team, stack_cleanup=0)


def plant(emu: Emulator, world: World, slots: dict[int, tuple[str, int]]) -> int:
    table = bytearray(max(slots) + 4)

    def make(name: str, cleanup: int):
        def stub(machine: Emulator) -> int:
            if name == "v20":
                world.add(f"v20:{machine.stack_arg(0) & 0xFFFFFFFF:08x};")
            elif name == "g158":
                if machine.register(uc.UC_X86_REG_ECX) != world.global_obj:
                    world.bad = "g158 this"
                if machine.stack_arg(0) != world.xfer:
                    world.bad = "g158 xfer"
                world.add(f"g158:{(machine.stack_arg(1) - world.module) & 0xFFFFFFFF:x};")
            elif name == "v18":
                if bytes(machine.read(machine.stack_arg(0), 2)) != b"\x01\x01":
                    world.bad = "version bytes"
                world.add("v18;")
            elif name == "v14":
                if machine.stack_arg(1) != 4:
                    world.bad = "v14 size"
                world.add(f"v14:{(machine.stack_arg(0) - world.module) & 0xFFFFFFFF:x};")
            elif name == "s1c":
                ecx = machine.register(uc.UC_X86_REG_ECX)
                index = world.weapons.index(ecx) if ecx in world.weapons else -1
                world.add(f"s1c:{index};")
            else:
                world.add(f"{name}:{(machine.stack_arg(0) - world.module) & 0xFFFFFFFF:x};")
            return world.take()

        return stub

    for offset, (name, cleanup) in slots.items():
        struct.pack_into("<I", table, offset, emu.bind_stub(make(name, cleanup), cleanup))
    return emu.alloc(bytes(table))


class Watch:
    def __init__(self, emu: Emulator) -> None:
        self.emu = emu
        self.spans: list[tuple[int, int, bytes]] = []

    def add(self, address: int, size: int) -> None:
        self.spans.append((address, size, bytes(self.emu.read(address, size))))

    def intact(self) -> bool:
        return all(bytes(self.emu.read(address, size)) == before for address, size, before in self.spans)


def finish(label: str, emu: Emulator, checker: Checker, world: World, watch: Watch,
           va: int, command: str, args: list[int], ecx: int, incoming: int, cleanup: int,
           missed: dict[str, int]) -> None:
    world.trace = ""
    world.bad = ""
    result = emu.call(va, args, ecx=ecx, eax=incoming)
    native_trace = world.trace if world.trace else "-"
    line = checker.call(command)
    eax_text, trace, mem = line.split(" ", 2)
    native_eax = f"{result.eax & 0xFFFFFFFF:08x}"
    if (eax_text != native_eax or trace != native_trace or mem != "-"
            or result.stack_cleanup != cleanup or world.bad or not watch.intact() or world.queue):
        missed["n"] += 1
        print(label)
        print(f"  native eax={native_eax} trace={native_trace} clean={result.stack_cleanup} bad={world.bad!r} "
              f"writes={not watch.intact()} left={len(world.queue)}")
        print(f"  checker {line} expect_clean={cleanup}")


def pattern(seed: int, size: int) -> bytes:
    return bytes((seed + index) & 0xFF for index in range(size))


def run(emu: Emulator, checker: Checker, world: World, missed: dict[str, int]) -> None:
    def enclose(incoming: int, ids: list[int], needle: int) -> None:
        owner = emu.alloc(b"\x11" * 0x20)
        blob = emu.alloc(struct.pack("<" + "I" * len(ids), *ids) if ids else b"\x00\x00\x00\x00")
        emu.write(owner + 0x0C, u32(len(ids)))
        emu.write(owner + 0x10, u32(blob if ids else 0))
        holder = emu.alloc(b"\x22" * 0x10)
        emu.write(holder, u32(owner))
        watch = Watch(emu)
        watch.add(owner, 0x20)
        watch.add(blob, max(4, 4 * len(ids)))
        begin = blob if ids else 0
        ids_text = " ".join(str(item) for item in ids)
        finish(
            f"enclose {ids} {needle:08x}", emu, checker, world, watch, 0x006A23A0,
            f"enclose {begin} {len(ids)} {needle} {ids_text}".rstrip(),
            [needle], holder + 0x0C, incoming, 4, missed)

    enclose(0xA5A5A500, [], 1)
    enclose(0xA5A5A500, [7], 7)
    enclose(0xA5A5A5FF, [7], 8)
    enclose(0x11111110, [1, 2, 3, 4], 1)
    enclose(0x11111110, [1, 2, 3, 4], 4)
    enclose(0x11111110, [1, 2, 3, 4, 5, 6, 7, 8], 5)
    enclose(0x11111110, [9, 9, 4], 9)
    enclose(0xABCDEF00, [0, 2, 0], 0)
    enclose(0xABCDEF00, [1, 2, 3, 4, 5, 6], 0)
    enclose(0x00000000, [0xFFFFFFFF, 0], 0xFFFFFFFF)

    def notify(incoming: int, returns: list[int], equal_empty: bool = False) -> None:
        world.weapons = []
        vtable = plant(emu, world, {0x1C: ("s1c", 0)})
        count = len(returns)
        if count == 0 and not equal_empty:
            array = 0
            end = 0
            watched = []
        else:
            weapons = [emu.alloc(u32(vtable) + b"\x00" * 12) for _ in range(count or 1)]
            world.weapons = weapons[:count]
            array = emu.alloc(struct.pack("<" + "I" * max(count, 1), *weapons))
            end = array if count == 0 else array + count * 4
            watched = [(array, max(4, count * 4))] + [(weapon, 16) for weapon in weapons]
        module = emu.alloc(b"\x33" * 0x40)
        emu.write(module + 0x24, u32(array))
        emu.write(module + 0x28, u32(end))
        world.queue = list(returns)
        watch = Watch(emu)
        watch.add(module, 0x40)
        for address, size in watched:
            watch.add(address, size)
        text = " ".join(str(item) for item in returns)
        finish(
            f"notify {returns}", emu, checker, world, watch, 0x006A16A0,
            f"notify {incoming} {count} {text}".rstrip(), [], module, incoming, 0, missed)

    notify(0xA5A5A501, [])
    notify(0xA5A5A501, [], equal_empty=True)
    notify(0x11111111, [0])
    notify(0x11111111, [9])
    notify(0x22222222, [1, 0, 4, 8, 2, 7])
    notify(0, [0xFFFFFFFF, 2, 3])

    def slaughter(helper: int) -> None:
        module = emu.alloc(b"\x44" * 0x780)
        world.module = module
        world.user = 0x01020304
        world.queue = [helper]
        watch = Watch(emu)
        watch.add(module, 0x780)
        finish(
            f"slaughter {helper:08x}", emu, checker, world, watch, 0x006B13F0,
            f"slaughter {helper}", [world.user], module, 0, 4, missed)

    slaughter(0)
    slaughter(1)
    slaughter(0x80000000)
    slaughter(0x7FFFFFFF)

    def vet(helper1: int, flag: int, status: int, byte74: int, byte75: int, same: int, helper2: int) -> None:
        template = emu.alloc(b"\x00" * 0x80)
        emu.write(template + 0x74, bytes((byte74, byte75)))
        other = emu.alloc(b"\x00" * 0x420)
        obj = emu.alloc(b"\x00" * 0x480)
        team_obj = 0x1111
        team_other = 0x1111 if same else 0x2222
        emu.write(other + 0x418, u32(team_other))
        emu.write(obj + 0x471, bytes((flag,)))
        emu.write(obj + 0x80, u32(status))
        emu.write(obj + 0x418, u32(team_obj))
        module = emu.alloc(b"\x00" * 0x10)
        emu.write(module + 4, u32(template))
        emu.write(module + 8, u32(other))
        world.module = module
        world.object = obj
        calls_second = (helper1 & 0xFF) != 0 and (flag & 1) == 0 and ((status >> 7) & 1) == 0 and byte74 == 0 and byte75 != 0 and same
        world.queue = [helper1] + ([helper2] if calls_second else [])
        watch = Watch(emu)
        for address, size in ((template, 0x80), (other, 0x420), (obj, 0x480), (module, 0x10)):
            watch.add(address, size)
        finish(
            f"vet {helper1:08x} {flag} {status:08x} {byte74} {byte75} {same} {helper2:08x}",
            emu, checker, world, watch, 0x006AFCB0,
            f"vet {helper1} {flag} {status} {byte74} {byte75} {same} {helper2} {team_obj}",
            [obj], module, 0xA5A5A500, 4, missed)

    vet(0xA5A5A500, 0, 0, 0, 0, 1, 0)
    vet(0xA5A5A501, 1, 0, 0, 0, 1, 0)
    vet(0xA5A5A501, 0, 1 << 7, 0, 0, 1, 0)
    vet(0xA5A5A501, 0, 0, 1, 0, 1, 0)
    vet(0xA5A5A501, 0, 0, 0, 0, 1, 0)
    vet(0xA5A5A501, 0, 0, 0, 1, 0, 0)
    vet(0xA5A5A501, 0, 0, 0, 1, 1, 0x01020300)
    vet(0xA5A5A501, 0, 0, 0, 1, 1, 0x01020304)
    vet(0x00000001, 0, 0xFFFFFF7F, 0, 2, 1, 0xFFFFFF00)

    def xfer_case(label: str, va: int, command: str, returns: list[int], slots: dict[int, tuple[str, int]],
                  plus4: int = 0, audio: bool = False) -> None:
        vtable = plant(emu, world, slots)
        xfer = emu.alloc(b"\x00" * 0x10)
        emu.write(xfer, u32(vtable))
        emu.write(xfer + 4, u32(plus4))
        module = emu.alloc(pattern(0xA5, 0x100))
        world.module = module
        world.xfer = xfer
        world.queue = list(returns)
        watch = Watch(emu)
        watch.add(module, 0x100)
        watch.add(xfer, 0x10)
        if audio:
            gtable = plant(emu, world, {0x158: ("g158", 8)})
            gobj = emu.alloc(u32(gtable) + b"\x00" * 12)
            world.global_obj = gobj
            emu.write(GLOBAL_AUDIO, u32(gobj))
            watch.add(GLOBAL_AUDIO, 4)
            watch.add(gobj, 16)
        finish(label, emu, checker, world, watch, va, command, [xfer], module, 0, 4, missed)

    xfer_case("clear a", 0x006AED50, "x_clear 11 22", [11, 22], {0x18: ("v18", 4)})
    xfer_case("clear b", 0x006AED50, "x_clear 1 0", [1, 0], {0x18: ("v18", 4)})
    xfer_case("tech a", 0x006A1C80, "x_tech 3 4", [3, 4], {0x18: ("v18", 4)})
    xfer_case("tech b", 0x006A1C80, "x_tech 0 4294967295", [0, 0xFFFFFFFF], {0x18: ("v18", 4)})
    xfer_case("dmove a", 0x006B50A0, "x_dmove 5 6", [5, 6], {0x18: ("v18", 4), 0x14: ("v14", 8)})
    xfer_case("dmove b", 0x006B50A0, "x_dmove 0 0", [0, 0], {0x18: ("v18", 4), 0x14: ("v14", 8)})
    xfer_case("dsm a", 0x006B5130, "x_dsm 7 8", [7, 8], {0x18: ("v18", 4), 0x14: ("v14", 8)})
    xfer_case("dsm b", 0x006B5130, "x_dsm 9 1", [9, 1], {0x18: ("v18", 4), 0x14: ("v14", 8)})
    xfer_case("dpick a", 0x006B5050, "x_dpick 1 2 3", [1, 2, 3],
              {0x18: ("v18", 4), 0x14: ("v14", 8), 0x70: ("v70", 4)})
    xfer_case("dpick b", 0x006B5050, "x_dpick 4 5 0", [4, 5, 0],
              {0x18: ("v18", 4), 0x14: ("v14", 8), 0x70: ("v70", 4)})
    xfer_case("ddo a", 0x006B50E0, "x_ddo 1 2 9", [1, 2, 9],
              {0x18: ("v18", 4), 0x14: ("v14", 8), 0x6C: ("v6c", 4)})
    xfer_case("ddo b", 0x006B50E0, "x_ddo 0 0 256", [0, 0, 0x100],
              {0x18: ("v18", 4), 0x14: ("v14", 8), 0x6C: ("v6c", 4)})

    def dozer_action(seed: int) -> None:
        vtable = plant(emu, world, {0x18: ("v18", 4), 0x14: ("v14", 8), 0x20: ("v20", 4)})
        xfer = emu.alloc(b"\x00" * 0x10)
        emu.write(xfer, u32(vtable))
        module = emu.alloc(pattern(seed, 0x100))
        world.module = module
        world.xfer = xfer
        world.queue = [1, 2, 3]
        watch = Watch(emu)
        watch.add(module, 0x100)
        watch.add(xfer, 0x10)
        finish(
            f"dact {seed}", emu, checker, world, watch, 0x006B5170,
            f"x_dact {seed} 1 2 3", [xfer], module, 0, 4, missed)

    dozer_action(0xA5)
    dozer_action(0x10)
    dozer_action(0x00)

    xfer_case("sub a", 0x006BC4A0, "x_sub 1 2 3 4", [1, 2, 3, 4], {0x18: ("v18", 4), 0x6C: ("v6c", 4)})
    xfer_case("sub b", 0x006BC4A0, "x_sub 8 7 6 5", [8, 7, 6, 5], {0x18: ("v18", 4), 0x6C: ("v6c", 4)})
    xfer_case("exp a", 0x006A3E90, "x_exp 1 2 3 4", [1, 2, 3, 4], {0x18: ("v18", 4), 0x84: ("v84", 4)})
    xfer_case("exp b", 0x006A3E90, "x_exp 0 0 0 9", [0, 0, 0, 9], {0x18: ("v18", 4), 0x84: ("v84", 4)})
    xfer_case("audio a", 0x006A1AA0, "x_audio 1 2 3 4", [1, 2, 3, 4], {0x18: ("v18", 4)}, audio=True)
    xfer_case("audio b", 0x006A1AA0, "x_audio 0 0 0 85", [0, 0, 0, 0x55], {0x18: ("v18", 4)}, audio=True)
    xfer_case("enter a", 0x006B37C0, "x_enter 1 2 3 4", [1, 2, 3, 4], {0x18: ("v18", 4), 0x6C: ("v6c", 4)})
    xfer_case("enter b", 0x006B37C0, "x_enter 9 8 7 6", [9, 8, 7, 6], {0x18: ("v18", 4), 0x6C: ("v6c", 4)})
    xfer_case("recv short", 0x006A2220, "x_recv 4 1 2 3 4 5 6", [1, 2, 3], {0x18: ("v18", 4), 0x64: ("v64", 4), 0x70: ("v70", 4)}, plus4=4)
    xfer_case("recv long", 0x006A2220, "x_recv 0 1 2 3 4 5 6", [1, 2, 3, 4, 5, 6],
              {0x18: ("v18", 4), 0x64: ("v64", 4), 0x70: ("v70", 4)}, plus4=0)
    xfer_case("recv other", 0x006A2220, "x_recv 1 9 8 7 6 5 4", [9, 8, 7, 6, 5, 4],
              {0x18: ("v18", 4), 0x64: ("v64", 4), 0x70: ("v70", 4)}, plus4=1)


def main() -> None:
    if not EXE.is_file():
        raise SystemExit(f"missing checker: {EXE}")
    emu = Emulator()
    assert_slots(emu)
    world = World()
    bind_direct(emu, world)
    checker = Checker(EXE)
    missed = {"n": 0}
    try:
        run(emu, checker, world, missed)
    finally:
        checker.close()
    if missed["n"]:
        raise SystemExit(f"mismatches {missed['n']}")
    print(f"ok functions={len(SLOTS)} mismatches=0")


if __name__ == "__main__":
    main()
