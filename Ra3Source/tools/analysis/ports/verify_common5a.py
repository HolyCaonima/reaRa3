"""Release oracle for the RA3 StateMachine cluster in 0x005A0000-0x005BFFFF.

Leaves get two to four scenarios. The condition scanner, transition checker,
state installer, update loop and goal-path xfer get six to twelve. A lock-test
mutant of storeGoalScalar must disagree with the shipped bytes.
"""

from __future__ import annotations

import struct
import subprocess
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from emulate import Emulator  # noqa: E402
from unicorn import x86_const as uc  # noqa: E402

INCOMING = 0xA5A5A5A5
TRANS = 0x00DF65F0
SLEEP = 0x00DF65F4
FRAME_SLOT = 0x00DEBA64
DEBUG_SLOT = 0x00DE9B1C
MAP_FIND = 0x006C3A10
OWNER_PROBE = 0x006A51F0
OWNER_TAIL = 0x006A51C0
ASSERT = 0x0040EBF0
OWNER_QUERY = 0x006A6210
LOOKUP_BEGIN = 0x00A21BE0
LOOKUP = 0x0050B5F0
LOOKUP_END = 0x00A21C90
MOVING = 0x00686D40
UNIT_GOAL = 0x00683630
MOVE_PROBE = 0x0063AEC0
CAN_PUBLISH = 0x006A5A60
ALREADY = 0x00683600
PUBLISH = 0x0064B470
PATH_ASSIGN = 0x005BA670
XFER_RESERVE = 0x0058FC50
XFER_GROW = 0x0058FF30

LOCK = 0x005AA240
UNLOCK = 0x005AA250
SCALAR = 0x005AA280
DWORD = 0x005AA2A0
GOAL_OBJECT = 0x005AC4C0
GOAL_POS = 0x005AC4F0
GOAL_RESET = 0x005AD150
COPY = 0x005B2450
PATH = 0x005AC540
GET_ID = 0x005A82E0
IDLE = 0x005AA1A0
FORWARD = 0x005AA220
WHILE = 0x005AA260
NOTIFY = 0x005AA1E0
CONTAINS = 0x005B6240
GET_STATE = 0x005B24D0
SCAN = 0x005AA040
TRANSITIONS = 0x005AA0C0
SLEEP_FN = 0x005AA140
APPLY = 0x005A9FE0
SET_STATE = 0x005B6230
INIT = 0x005B25C0
UPDATE = 0x005AC420
FAIL = 0x005A9FA0
MODE = 0x005BA020
LIVE = 0x005BDCA0
PUBLISH_FN = 0x005BA110
ADJUST = 0x005BDCC0
XFER = 0x005BDF30


class Checker:
    def __init__(self, executable: Path) -> None:
        self.process = subprocess.Popen(
            [str(executable)], stdin=subprocess.PIPE, stdout=subprocess.PIPE, text=True, bufsize=1)

    def call(self, line: str) -> str:
        assert self.process.stdin is not None and self.process.stdout is not None
        self.process.stdin.write(line + "\n")
        self.process.stdin.flush()
        result = self.process.stdout.readline().strip()
        if not result or result == "error":
            raise RuntimeError(f"common5acheck rejected {line}")
        return result

    def close(self) -> None:
        assert self.process.stdin is not None
        self.process.stdin.write("quit\n")
        self.process.stdin.flush()
        if self.process.wait(timeout=10) != 0:
            raise RuntimeError("common5acheck failed")


def require(condition: bool, message: str) -> None:
    if not condition:
        raise RuntimeError(message)


def put(emu: Emulator, address: int, value: int) -> None:
    emu.write(address, struct.pack("<I", value & 0xFFFFFFFF))


def dword(emu: Emulator, address: int) -> int:
    return struct.unpack("<I", emu.read(address, 4))[0]


def hx(value: int) -> str:
    return f"{value & 0xFFFFFFFF:08x}"


def formatted(eax: int, words: list[int], trace: list[str]) -> str:
    text = " ".join([hx(eax), *[hx(word) for word in words]])
    if trace:
        text += " " + ",".join(trace)
    return text


box: dict = {"trace": [], "conds": [], "ci": 0}


def log(text: str) -> None:
    box["trace"].append(text)


def bind_world(emu: Emulator) -> None:
    def state_call(slot: int, cleanup: int):
        def stub(active: Emulator) -> int:
            log(f"st {slot} 0")
            state = active.register(uc.UC_X86_REG_ECX)
            if slot == 0x10 and box.get("clear"):
                machine = dword(active, state + 0x18)
                put(active, machine + 4, 0)
            if slot == 0x14:
                return box["update"]
            if slot == 0x10:
                return box["enter"]
            if slot in (0x28, 0x48, 0x3C):
                return box["state_ret"]
            return 0
        return emu.bind_stub(stub, cleanup)

    def machine_call(slot: int, cleanup: int):
        def stub(active: Emulator) -> int:
            ecx = active.register(uc.UC_X86_REG_ECX)
            log(f"mc {slot} {active.read(ecx + 0x78, 1)[0]}")
            return box["machine_ret"]
        return emu.bind_stub(stub, cleanup)

    box["state_slots"] = {
        0x10: state_call(0x10, 0),
        0x14: state_call(0x14, 0),
        0x18: state_call(0x18, 4),
        0x28: state_call(0x28, 0),
        0x3C: state_call(0x3C, 0),
        0x48: state_call(0x48, 0),
    }
    box["machine_slots"] = {
        0x18: machine_call(0x18, 0),
        0x34: machine_call(0x34, 0),
        0x38: machine_call(0x38, 4),
    }

    def debug_gate(active: Emulator) -> int:
        log("ext 4")
        return box["gate"]
    def debug_chain(active: Emulator) -> int:
        log("ext 16")
        return box["debug"]
    box["debug_slots"] = {
        0x68: emu.bind_stub(debug_gate, 0),
        0x78: emu.bind_stub(debug_chain, 16),
        0x3C: emu.bind_stub(debug_chain, 4),
        0x50: emu.bind_stub(debug_chain, 8),
    }
    box["unit_kind"] = emu.bind_stub(lambda active: log("ext 9") or box["kind"], 0)

    def condition(active: Emulator) -> int:
        value = box["conds"][box["ci"]]
        box["ci"] += 1
        log(f"cond {value & 0xFF}")
        return value
    box["condition"] = emu.bind_stub(condition, 0)

    def find(active: Emulator) -> int:
        out = active.stack_arg(0)
        key = dword(active, active.stack_arg(1))
        machine = active.register(uc.UC_X86_REG_ECX) - 8
        hit = box["hit"]
        log(f"find {key} {1 if hit else 0}")
        if hit:
            put(active, box["found"] + 4, box["find_id"])
            node = active.alloc(b"\x00" * 0x14 + struct.pack("<I", box["found"]))
            iterator = node
        else:
            iterator = machine + 0x0C
        put(active, out, iterator)
        box["find_out"] = out
        return out
    emu.bind_function(MAP_FIND, find, 8)

    emu.bind_function(OWNER_PROBE, lambda active: log("ext 1") or box["probe"], 0)
    emu.bind_function(OWNER_TAIL, lambda active: log("ext 2") or box["tail"], 4)
    emu.bind_function(ASSERT, lambda active: log("ext 3") or box["assert_al"], 0)
    emu.bind_function(OWNER_QUERY, lambda active: log("ext 5") or box["query"], 0)
    emu.bind_function(LOOKUP_BEGIN, lambda active: log("ext 14") or 0, 4)
    def lookup(active: Emulator) -> int:
        log("ext 6")
        out = active.stack_arg(0)
        if box["goal_obj"]:
            node = active.alloc(struct.pack("<II", 0, box["goal_obj"]))
            iterator = node
        else:
            iterator = box["sentinel"]
        put(active, out, iterator)
        return 0
    emu.bind_function(LOOKUP, lookup, 8)
    emu.bind_function(LOOKUP_END, lambda active: log("ext 15") or 0, 4)
    emu.bind_function(MOVING, lambda active: log("ext 7") or box["moving"], 0)
    emu.bind_function(UNIT_GOAL, lambda active: log("ext 8") or box["unit_goal"], 0)
    emu.bind_function(MOVE_PROBE, lambda active: log("ext 10") or 0, 0)
    emu.bind_function(CAN_PUBLISH, lambda active: log("ext 11") or box["can"], 4)
    emu.bind_function(ALREADY, lambda active: log("ext 12") or box["already"], 4)
    emu.bind_function(PUBLISH, lambda active: log("ext 13") or 0, 0)
    def path_assign(active: Emulator) -> int:
        log("ext 17")
        box["path_this"] = active.register(uc.UC_X86_REG_ECX)
        box["path_arg"] = active.stack_arg(0)
        return box["path_ret"]
    emu.bind_function(PATH_ASSIGN, path_assign, 4)
    emu.bind_function(XFER_RESERVE, lambda active: log("ext 23") or 0, 4)
    def xfer_grow(active: Emulator) -> int:
        log("ext 24")
        vector = active.register(uc.UC_X86_REG_ECX)
        end = dword(active, vector + 4)
        emu_zeros = b"\x00" * 12
        active.write(end, emu_zeros)
        put(active, vector + 4, end + 12)
        return 0
    emu.bind_function(XFER_GROW, xfer_grow, 8)

    def xfer_open(active: Emulator) -> int:
        log("ext 18")
        return 0
    def xfer_name(active: Emulator) -> int:
        log("ext 19")
        return box["xfer_named"]
    def xfer_count(active: Emulator) -> int:
        log("ext 20")
        put(active, active.stack_arg(0), box["xfer_count"])
        return 0
    def xfer_element(active: Emulator) -> int:
        log("ext 21")
        return 0
    def xfer_clear(active: Emulator) -> int:
        log("ext 22")
        return 0
    box["xfer_open"] = emu.bind_stub(xfer_open, 4)
    box["xfer_name"] = emu.bind_stub(xfer_name, 4)
    box["xfer_count_fn"] = emu.bind_stub(xfer_count, 4)
    box["xfer_element"] = emu.bind_stub(xfer_element, 4)
    box["xfer_clear"] = emu.bind_stub(xfer_clear, 0)


def boot(emu: Emulator, frame: int = 100) -> tuple[int, int]:
    emu.reset_data()
    box["trace"] = []
    box["conds"] = []
    box["ci"] = 0
    box["hit"] = 0
    box["find_id"] = 0
    box["enter"] = 0
    box["update"] = 0
    box["state_ret"] = 0
    box["machine_ret"] = 0
    box["clear"] = 0
    box["probe"] = 0
    box["tail"] = 0
    box["assert_al"] = 1
    box["gate"] = 1
    box["query"] = 0
    box["goal_obj"] = 0
    box["moving"] = 1
    box["unit_goal"] = 0
    box["kind"] = 0
    box["can"] = 0
    box["already"] = 1
    box["path_ret"] = 0
    box["xfer_count"] = 0
    state_vt = emu.alloc(bytes(0x50))
    for offset, address in box["state_slots"].items():
        put(emu, state_vt + offset, address)
    machine_vt = emu.alloc(bytes(0x40))
    for offset, address in box["machine_slots"].items():
        put(emu, machine_vt + offset, address)
    debug_vt = emu.alloc(bytes(0x80))
    for offset, address in box["debug_slots"].items():
        put(emu, debug_vt + offset, address)
    unit_vt = emu.alloc(bytes(0x290))
    put(emu, unit_vt + 0x288, box["unit_kind"])
    xfer_vt = emu.alloc(bytes(0x90))
    put(emu, xfer_vt + 0x18, box["xfer_open"])
    put(emu, xfer_vt + 0x1C, box["xfer_name"])
    put(emu, xfer_vt + 0x58, box["xfer_element"])
    put(emu, xfer_vt + 0x8C, box["xfer_clear"])
    named_vt = emu.alloc(bytes(0x70))
    put(emu, named_vt + 0x6C, box["xfer_count_fn"])
    frame_obj = emu.alloc(bytes(0x100))
    put(emu, frame_obj + 0x58, frame)
    sentinel = emu.alloc(bytes(4))
    array = emu.alloc(struct.pack("<I", sentinel))
    # The goal map is embedded at frame+0xC4, not stored through a pointer.
    put(emu, frame_obj + 0xC8, array)
    put(emu, frame_obj + 0xCC, 0)
    box["sentinel"] = dword(emu, array)
    put(emu, FRAME_SLOT, frame_obj)
    put(emu, TRANS, 0)
    put(emu, SLEEP, 0)
    debug = emu.alloc(bytes(0x10))
    put(emu, debug, debug_vt)
    put(emu, DEBUG_SLOT, debug)
    box["debug"] = debug
    xfer_named = emu.alloc(bytes(8))
    put(emu, xfer_named, named_vt)
    box["xfer_named"] = xfer_named
    state = emu.alloc(bytes(0x20))
    put(emu, state, state_vt)
    found = emu.alloc(bytes(0x20))
    put(emu, found, state_vt)
    machine = emu.alloc(bytes(0x80))
    put(emu, machine, machine_vt)
    put(emu, state + 0x18, machine)
    put(emu, found + 0x18, machine)
    put(emu, machine + 0x68, 0x000F423F)
    box["state_obj"] = state
    box["found"] = found
    box["machine_obj"] = machine
    box["unit_vt"] = unit_vt
    box["xfer_vt"] = xfer_vt
    return machine, state


def compare(checker: Checker, command: str, native: str) -> None:
    got = checker.call(command)
    require(got == native, f"{command}\n  checker {got}\n  native  {native}")


def call(emu: Emulator, address: int, args: list[int], ecx: int, eax: int = 0,
         cleanup: int = 0) -> int:
    result = emu.call(address, args, ecx=ecx, eax=eax)
    require(result.stack_cleanup == cleanup, f"{address:#x} cleanup {result.stack_cleanup} != {cleanup}")
    return result.eax


def verify(executable: Path) -> None:
    emu = Emulator()
    checker = Checker(executable)
    bind_world(emu)
    leaves(emu, checker)
    parsers(emu, checker)
    goals(emu, checker)
    checker.close()
    print("common5a: 30 StateMachine functions, leaf/parser/update/xfer cases;")
    print("  storeGoalScalar mutant disagreed with the shipped lock test")


def leaves(emu: Emulator, checker: Checker) -> None:
    machine, state = boot(emu)
    native = call(emu, SCALAR, [0x3F800000], machine, INCOMING, 4)
    compare(checker, f"scalar 0 {INCOMING:x} 3f800000", formatted(native, [dword(emu, machine + 0x54)], []))
    machine, _ = boot(emu)
    put(emu, machine + 0x54, 0x11111111)
    emu.write(machine + 0x78, b"\x01")
    native = call(emu, SCALAR, [0x3F800000], machine, INCOMING, 4)
    shipped = formatted(native, [dword(emu, machine + 0x54)], [])
    compare(checker, f"scalar 1 {INCOMING:x} 3f800000", shipped)
    require(checker.call(f"mutant 1 {INCOMING:x} 3f800000") != shipped, "mutant matched locked scalar")
    machine, _ = boot(emu)
    native = call(emu, SCALAR, [0x3F800000], machine, INCOMING, 4)
    unlocked = formatted(native, [dword(emu, machine + 0x54)], [])
    require(checker.call(f"mutant 0 {INCOMING:x} 3f800000") != unlocked, "mutant matched unlocked scalar")

    machine, _ = boot(emu)
    native = call(emu, DWORD, [0xDEADBEEF], machine, 0, 4)
    compare(checker, "dword deadbeef", formatted(native, [dword(emu, machine + 0x70)], []))
    machine, _ = boot(emu)
    native = call(emu, LOCK, [0], machine, INCOMING, 4)
    compare(checker, f"lock {INCOMING:x}", formatted(native, [emu.read(machine + 0x78, 1)[0]], []))
    machine, _ = boot(emu)
    emu.write(machine + 0x78, b"\x01")
    native = call(emu, UNLOCK, [], machine, INCOMING, 0)
    compare(checker, f"unlock {INCOMING:x}", formatted(native, [emu.read(machine + 0x78, 1)[0]], []))

    for locked, has, dc, label in ((1, 1, 0x12345678, "goalobj 1 a5a5a5a5 1 12345678"),
                                   (0, 0, 0, "goalobj 0 a5a5a5a5 0 0"),
                                   (0, 1, 0x12345678, "goalobj 0 a5a5a5a5 1 12345678")):
        machine, _ = boot(emu)
        emu.write(machine + 0x78, bytes([locked]))
        put(emu, machine + 0x28, 0x22222222)
        obj = 0
        if has:
            obj = emu.alloc(bytes(0xE0))
            put(emu, obj + 0xDC, dc)
        native = call(emu, GOAL_OBJECT, [obj], machine, INCOMING, 4)
        compare(checker, label, formatted(native, [dword(emu, machine + 0x28)], []))

    goal_position(emu, checker, GOAL_POS, "goalpos", True)
    goal_position(emu, checker, GOAL_RESET, "goalreset", False)
    copy_goal(emu, checker)
    for index, count, command in ((0, 2, "path 0 2"), (1, 2, "path 1 2"), (2, 2, "path 2 2"),
                                  (-1, 2, "path ffffffff 2"), (0, 0, "path 0 0")):
        machine, _ = boot(emu)
        begin = emu.alloc(bytes(64))
        put(emu, machine + 0x58, begin)
        put(emu, machine + 0x5C, begin + count * 12)
        native = call(emu, PATH, [index & 0xFFFFFFFF], machine, 0, 4)
        reported = 0xFFFFFFFF if native == 0 else (native - begin) // 12
        compare(checker, command, formatted(reported, [], []))

    machine, state = boot(emu)
    native = call(emu, GET_ID, [], machine, 0, 0)
    compare(checker, "getid 0 0", formatted(native, [], []))
    machine, state = boot(emu)
    put(emu, state + 4, 0x42)
    put(emu, machine + 4, state)
    native = call(emu, GET_ID, [], machine, 0, 0)
    compare(checker, "getid 1 42", formatted(native, [], []))

    machine, _ = boot(emu)
    native = call(emu, IDLE, [], machine, INCOMING, 0)
    compare(checker, f"idle 0 {INCOMING:x} 0", formatted(native, [], box["trace"]))
    for virt, command in ((0xA5A5A500, "idle 1 a5a5a500 a5a5a500"),
                          (0xA5A5A503, "idle 1 0 a5a5a503")):
        machine, state = boot(emu)
        put(emu, machine + 4, state)
        box["state_ret"] = virt
        native = call(emu, IDLE, [], machine, 0, 0)
        compare(checker, command, formatted(native, [], box["trace"]))
    machine, _ = boot(emu)
    native = call(emu, FORWARD, [], machine, INCOMING, 0)
    compare(checker, f"fwd 0 {INCOMING:x} 0", formatted(native, [], box["trace"]))
    machine, state = boot(emu)
    put(emu, machine + 4, state)
    box["state_ret"] = 0xDEADBEEF
    native = call(emu, FORWARD, [], machine, 0, 0)
    compare(checker, "fwd 1 0 deadbeef", formatted(native, [], box["trace"]))

    machine, _ = boot(emu)
    box["machine_ret"] = 0x1234
    emu.write(machine + 0x78, b"\xAB")
    native = call(emu, WHILE, [], machine, 0, 0)
    compare(checker, "whileu 1234", formatted(native, [emu.read(machine + 0x78, 1)[0]], box["trace"]))
    machine, _ = boot(emu)
    box["machine_ret"] = 0x99
    native = call(emu, NOTIFY, [], machine, 0, 0)
    compare(checker, "notify 0 0 99", formatted(native, [], box["trace"]))
    machine, state = boot(emu)
    put(emu, machine + 4, state)
    box["state_ret"] = 0
    box["machine_ret"] = 0x99
    native = call(emu, NOTIFY, [], machine, 0, 0)
    compare(checker, "notify 1 0 99", formatted(native, [], box["trace"]))

    for incoming, hit, command in ((INCOMING, 0, f"contains {INCOMING:x} 0"),
                                   (INCOMING, 1, f"contains {INCOMING:x} 1"),
                                   (0, 0, "contains 0 0"),
                                   (0xFF, 1, "contains ff 1")):
        machine, _ = boot(emu)
        box["hit"] = hit
        native = call(emu, CONTAINS, [7], machine, incoming, 4)
        require((native & 0xFF) == hit, f"contains flag {native:#x}")
        require((native & 0xFFFFFF00) == (box["find_out"] & 0xFFFFFF00),
                f"contains residue {native:#x} {box['find_out']:#x}")
        compare(checker, command, formatted(hit, [], box["trace"]))
    machine, state = boot(emu)
    box["hit"] = 0
    native = call(emu, GET_STATE, [7], machine, 0, 4)
    compare(checker, "getst 0 0", formatted(0 if native == 0 else dword(emu, native + 4), [], box["trace"]))
    machine, state = boot(emu)
    box["find_id"] = 0x42
    box["hit"] = 1
    native = call(emu, GET_STATE, [7], machine, 0, 4)
    compare(checker, "getst 1 42", formatted(0 if native == 0 else dword(emu, native + 4), [], box["trace"]))


def goal_position(emu: Emulator, checker: Checker, address: int, name: str, with_bits: bool) -> None:
    cases = (
        (1, INCOMING, 0, 0, 0, 0, 0x3F800000),
        (0, INCOMING, 0, 0, 0, 0, 0x3F800000),
        (0, 0, 1, 0x11, 0x22, 0x33, 0x3F800000),
    )
    for locked, incoming, has, x, y, z, bits in cases:
        machine, _ = boot(emu)
        emu.write(machine + 0x78, bytes([locked]))
        put(emu, machine + 0x54, 0x11111111)
        put(emu, machine + 0x44, 0xAB)
        position = 0
        if has:
            position = emu.alloc(bytes(12))
            put(emu, position, x)
            put(emu, position + 4, y)
            put(emu, position + 8, z)
        args = [position, bits] if with_bits else [position]
        native = call(emu, address, args, machine, incoming, 8 if with_bits else 4)
        words = [dword(emu, machine + off) for off in (0x2C, 0x30, 0x34, 0x44, 0x48, 0x4C, 0x50, 0x54)]
        tail = f" {bits:x}" if with_bits else ""
        command = f"{name} {locked:x} {incoming:x} {has:x} {x:x} {y:x} {z:x}{tail}"
        compare(checker, command, formatted(native, words, []))


def copy_goal(emu: Emulator, checker: Checker) -> None:
    for locked, command in ((0, "copy 0 10 20 30 ab 40 50 60 3f800000 7e"),
                            (1, "copy 1 10 20 30 ab 40 50 60 3f800000 7e")):
        machine, _ = boot(emu)
        emu.write(machine + 0x78, bytes([locked]))
        for offset, value in ((0x2C, 0x01010101), (0x30, 0x02020202), (0x34, 0x03030303),
                              (0x48, 0x05050505), (0x4C, 0x06060606), (0x50, 0x07070707),
                              (0x54, 0x11111111)):
            put(emu, machine + offset, value)
        emu.write(machine + 0x44, b"\x44")
        emu.write(machine + 0x7C, b"\x7C")
        source = emu.alloc(bytes(0x80))
        put(emu, source + 0x2C, 0x10)
        put(emu, source + 0x30, 0x20)
        put(emu, source + 0x34, 0x30)
        emu.write(source + 0x44, b"\xAB")
        put(emu, source + 0x48, 0x40)
        put(emu, source + 0x4C, 0x50)
        put(emu, source + 0x50, 0x60)
        put(emu, source + 0x54, 0x3F800000)
        emu.write(source + 0x7C, b"\x7E")
        native = call(emu, COPY, [source], machine, 0, 4)
        words = [dword(emu, machine + 0x2C), dword(emu, machine + 0x30), dword(emu, machine + 0x34),
                 emu.read(machine + 0x44, 1)[0], dword(emu, machine + 0x48), dword(emu, machine + 0x4C),
                 dword(emu, machine + 0x50), dword(emu, machine + 0x54), emu.read(machine + 0x7C, 1)[0]]
        compare(checker, command, formatted(native, words, []))


def arm_records(emu: Emulator, state: int, pairs: list[tuple[int, int]]) -> None:
    if not pairs:
        return
    records = emu.alloc(bytes(16 * len(pairs)))
    put(emu, state + 0x10, records)
    put(emu, state + 0x14, len(pairs))
    for index, (to_state, value) in enumerate(pairs):
        put(emu, records + index * 16, box["condition"])
        put(emu, records + index * 16 + 4, to_state)
        box["conds"].append(value)


def parsers(emu: Emulator, checker: Checker) -> None:
    scan_cases = [
        ("scan a5a5a5a5 0 0 0", INCOMING, []),
        ("scan 0 1 5 0 0 0", 0, [(5, 0)]),
        ("scan a5a5a501 1 270e a5a5a501 0 0", 0xA5A5A501, [(0x270E, 0xA5A5A501)]),
        ("scan 1 1 270f 1 0 0", 1, [(0x270F, 1)]),
        ("scan 1 1 5 1 1 5", 1, [(5, 1)]),
        ("scan 0 2 5 0 270e 1 0 0", 0, [(5, 0), (0x270E, 1)]),
    ]
    for command, incoming, pairs in scan_cases:
        _, state = boot(emu)
        box["hit"] = 1 if command.startswith("scan 1 1 5 1") else 0
        box["find_id"] = 5
        arm_records(emu, state, pairs)
        out = emu.alloc(struct.pack("<I", 0x11111111))
        native = call(emu, SCAN, [out], state, incoming, 4)
        if native == out:
            native = 1
        machine = box["machine_obj"]
        current = dword(emu, machine + 4)
        current_id = 0 if current == 0 else dword(emu, current + 4)
        compare(checker, command, formatted(native, [dword(emu, out), current_id, dword(emu, machine + 0x74)], box["trace"]))

    # depth is stored as a count. 0x13 enters the limit.
    trans_cases = [
        ("trans 13 0 0 0 0", 0x13, 0, 0, 0, []),
        ("trans 0 0 0 0 5", 0, 0, 0, 5, []),
        ("trans 0 270e 0 0 ffffffff", 0, 0x270E, 0, 0xFFFFFFFF, []),
        ("trans 0 0 270f 0 fffffffe", 0, 0, 0x270F, 0xFFFFFFFE, []),
        ("trans 0 0 0 0 0", 0, 0, 0, 0, []),
        ("trans 0 0 0 1 0 270e 1", 0, 0, 0, 0, [(0x270E, 1)]),
    ]
    for command, depth, success, failure, status, pairs in trans_cases:
        _, state = boot(emu)
        put(emu, TRANS, depth)
        put(emu, state + 8, success)
        put(emu, state + 0x0C, failure)
        arm_records(emu, state, pairs)
        native = call(emu, TRANSITIONS, [status], state, 0, 4)
        current = dword(emu, box["machine_obj"] + 4)
        current_id = 0 if current == 0 else dword(emu, current + 4)
        compare(checker, command, formatted(native, [dword(emu, TRANS), current_id], box["trace"]))

    sleep_cases = [
        ("sleep 13 0 9", 0x13, 0, 9, []),
        ("sleep 0 0 9", 0, 0, 9, []),
        ("sleep 0 1 9 270e 1", 0, 1, 9, [(0x270E, 1)]),
        ("sleep 0 1 4 5 0", 0, 1, 4, [(5, 0)]),
        ("sleep 0 0 0", 0, 0, 0, []),
        ("sleep 12 0 1", 0x12, 0, 1, []),
    ]
    for command, depth, count, frames, pairs in sleep_cases:
        _, state = boot(emu)
        put(emu, SLEEP, depth)
        arm_records(emu, state, pairs)
        native = call(emu, SLEEP_FN, [frames], state, 0, 4)
        compare(checker, command, formatted(native, [dword(emu, SLEEP)], box["trace"]))

    apply_cases = [
        ("apply 270e 0 0 0 0", 0x270E, 0, 0, 0, 0),
        ("apply 270f 0 0 0 0", 0x270F, 0, 0, 0, 0),
        ("apply f423d 123 0 0 0", 0xF423D, 0x123, 0, 0, 0),
        ("apply 5 0 1 5 0", 5, 0, 1, 5, 0),
        ("apply 5 0 0 0 0", 5, 0, 0, 0, 0),
        ("apply 5 0 1 5 4", 5, 0, 1, 5, 4),
    ]
    for command, ident, machine_ret, hit, find_id, enter in apply_cases:
        _, state = boot(emu)
        box["machine_ret"] = machine_ret
        box["hit"] = hit
        box["find_id"] = find_id
        box["enter"] = enter
        native = call(emu, APPLY, [ident], state, 0, 4)
        current = dword(emu, box["machine_obj"] + 4)
        current_id = 0 if current == 0 else dword(emu, current + 4)
        compare(checker, command, formatted(
            native, [current_id, dword(emu, box["machine_obj"] + 0x74)], box["trace"]))

    set_cases = [
        ("set 1 0 0 0 0 0 0 5", 1, 0, 0, 0, 0, 0, 0, 5),
        ("set 0 f423f 0 0 0 0 0 f423f", 0, 0xF423F, 0, 0, 0, 0, 0, 0xF423F),
        ("set 0 f423f 0 0 0 0 0 f423e", 0, 0xF423F, 0, 0, 0, 0, 0, 0xF423E),
        ("set 0 f423f 0 1 5 0 0 5", 0, 0xF423F, 0, 1, 5, 0, 0, 5),
        ("set 0 f423f 0 1 5 3 0 5", 0, 0xF423F, 0, 1, 5, 3, 0, 5),
        ("set 0 f423f 1 0 0 0 0 f423e", 0, 0xF423F, 1, 0, 0, 0, 0, 0xF423E),
        ("set 0 f423f 0 1 5 0 1 5", 0, 0xF423F, 0, 1, 5, 0, 1, 5),
    ]
    for command, locked, default, previous, hit, find_id, enter, clear, ident in set_cases:
        machine, state = boot(emu)
        emu.write(machine + 0x78, bytes([locked]))
        put(emu, machine + 0x68, default)
        put(emu, machine + 0x74, 0x33333333)
        if previous:
            other = emu.alloc(bytes(0x20))
            put(emu, other, dword(emu, state))
            put(emu, other + 4, 9)
            put(emu, machine + 4, other)
        box["hit"] = hit
        box["find_id"] = find_id
        box["enter"] = enter
        box["clear"] = clear
        native = call(emu, SET_STATE, [ident], machine, 0, 4)
        current = dword(emu, machine + 4)
        current_id = 0 if current == 0 else dword(emu, current + 4)
        compare(checker, command, formatted(
            native, [current_id, dword(emu, machine + 0x74), emu.read(machine + 0x78, 1)[0]], box["trace"]))

    init_cases = [
        ("init 1 5 0 0", 1, 5, 0, 0),
        ("init 0 f423f 0 0", 0, 0xF423F, 0, 0),
        ("init 0 5 1 5", 0, 5, 1, 5),
        ("init 0 5 0 0", 0, 5, 0, 0),
    ]
    for command, already, default, hit, find_id in init_cases:
        machine, state = boot(emu)
        emu.write(machine + 0x7A, bytes([already]))
        put(emu, machine + 0x68, default)
        box["hit"] = hit
        box["find_id"] = find_id
        native = call(emu, INIT, [], machine, 0, 0)
        current = dword(emu, machine + 4)
        current_id = 0 if current == 0 else dword(emu, current + 4)
        compare(checker, command, formatted(
            native, [emu.read(machine + 0x7A, 1)[0], current_id], box["trace"]))

    update_cases = [
        ("update 5a 10 0 0 0 0 0 0 0", 0x5A, 0x10, 0, 0, 0, 0, 0, 0, 0, []),
        ("update 5a 10 20 0 0 0 0 0 0", 0x5A, 0x10, 0x20, 0, 0, 0, 0, 0, 0, []),
        ("update 5a 10 20 1 3 0 0 0 0", 0x5A, 0x10, 0x20, 1, 3, 0, 0, 0, 0, []),
        ("update 5a 10 0 1 3 0 0 0 0", 0x5A, 0x10, 0, 1, 3, 0, 0, 0, 0, []),
        ("update 5a 10 0 1 3 0 0 0 6", 0x5A, 0x10, 0, 1, 3, 0, 0, 0, 6, []),
        ("update 5a 10 0 1 3 270e 0 0 ffffffff", 0x5A, 0x10, 0, 1, 3, 0x270E, 0, 0, 0xFFFFFFFF, []),
        ("update 5a 30 30 0 0 0 0 0 0", 0x5A, 0x30, 0x30, 0, 0, 0, 0, 0, 0, []),
        ("update 5a 10 0 1 3 0 0 1 0 270e 1", 0x5A, 0x10, 0, 1, 3, 0, 0, 1, 0, [(0x270E, 1)]),
    ]
    for command, mark, frame, sleep, has, ident, success, failure, count, updated, pairs in update_cases:
        machine, state = boot(emu, frame)
        emu.write(machine + 0x79, bytes([mark]))
        put(emu, machine + 0x74, sleep)
        if has:
            put(emu, state + 4, ident)
            put(emu, state + 8, success)
            put(emu, state + 0x0C, failure)
            put(emu, machine + 4, state)
            arm_records(emu, state, pairs)
        box["update"] = updated
        native = call(emu, UPDATE, [], machine, 0, 0)
        current = dword(emu, machine + 4)
        current_id = 0 if current == 0 else dword(emu, current + 4)
        compare(checker, command, formatted(
            native, [emu.read(machine + 0x79, 1)[0], dword(emu, machine + 0x74), current_id], box["trace"]))


def goals(emu: Emulator, checker: Checker) -> None:
    mode_cases = [
        ("mode 0 1 0 42 0 1 1", 0, 1, 0, 0x42, 0, 1, 1),
        ("mode 1 0 0 0 0 1 1", 1, 0, 0, 0, 0, 1, 1),
        ("mode 1 0 7 0 1 1 1", 1, 0, 7, 0, 1, 1, 1),
        ("mode 1 0 7 0 0 1 1", 1, 0, 7, 0, 0, 1, 1),
        ("mode 2 0 0 0 0 1 1", 2, 0, 0, 0, 0, 1, 1),
        ("mode 2 0 0 0 0 0 1", 2, 0, 0, 0, 0, 0, 1),
        ("mode 2 0 0 0 0 0 0", 2, 0, 0, 0, 0, 0, 0),
    ]
    for command, mode, has_owner, goal, query, has_goal, asserted, gate in mode_cases:
        machine, _ = boot(emu)
        put(emu, machine + 0x6C, mode)
        if has_owner:
            put(emu, machine + 0x24, emu.alloc(bytes(4)))
        put(emu, machine + 0x28, goal)
        box["query"] = query
        box["assert_al"] = asserted
        box["gate"] = gate
        goal_obj = emu.alloc(bytes(0x20)) if has_goal else 0
        box["goal_obj"] = goal_obj
        native = call(emu, MODE, [], machine, 0, 0)
        shown = 1 if has_goal and native == goal_obj else native
        compare(checker, command, formatted(shown, [], box["trace"]))

    machine, _ = boot(emu)
    native = call(emu, LIVE, [], machine, INCOMING, 0)
    compare(checker, f"live {INCOMING:x} 0 0 0", formatted(native, [], box["trace"]))
    machine, _ = boot(emu)
    put(emu, machine + 0x28, 7)
    put(emu, machine + 0x6C, 1)
    box["goal_obj"] = 0
    native = call(emu, LIVE, [], machine, 0, 0)
    compare(checker, "live 0 7 0 0", formatted(native, [], box["trace"]))
    machine, _ = boot(emu)
    put(emu, machine + 0x28, 7)
    put(emu, machine + 0x6C, 1)
    obj = emu.alloc(bytes(0x480))
    obj_bytes = bytearray(emu.read(obj, 0x480))
    obj_bytes[0x471] = 1
    emu.write(obj, bytes(obj_bytes))
    box["goal_obj"] = obj
    native = call(emu, LIVE, [], machine, 0, 0)
    require((native & 0xFFFFFF00) == (obj & 0xFFFFFF00), f"live residue {native:#x} {obj:#x}")
    compare(checker, "live 0 7 1 1", formatted(native & 0xFF, [], box["trace"]))

    _, state = boot(emu)
    native = call(emu, FAIL, [1], state, 0, 4)
    compare(checker, "fail 0 1 0 0", formatted(native, [], box["trace"]))
    _, state = boot(emu)
    owner = emu.alloc(bytes(4))
    put(emu, box["machine_obj"] + 0x24, owner)
    box["tail"] = 0x99
    native = call(emu, FAIL, [0], state, 0, 4)
    compare(checker, "fail 1 0 0 99", formatted(native, [], box["trace"]))
    _, state = boot(emu)
    put(emu, box["machine_obj"] + 0x24, emu.alloc(bytes(4)))
    box["probe"] = 0x55
    native = call(emu, FAIL, [1], state, 0, 4)
    compare(checker, "fail 1 1 55 0", formatted(native, [], box["trace"]))
    _, state = boot(emu)
    put(emu, box["machine_obj"] + 0x24, emu.alloc(bytes(4)))
    box["probe"] = 0
    box["tail"] = 0x77
    native = call(emu, FAIL, [1], state, 0, 4)
    compare(checker, "fail 1 1 0 77", formatted(native, [], box["trace"]))

    publish(emu, checker)
    machine, _ = boot(emu)
    box["path_ret"] = 0x123
    native = call(emu, ADJUST, [0x456], machine, 0, 4)
    flag = 1 if box["path_this"] == machine + 0x58 else 0
    compare(checker, "adjust 123 456", formatted(native, [flag, box["path_arg"]], box["trace"]))
    xfer(emu, checker)


def publish(emu: Emulator, checker: Checker) -> None:
    # mode, goal id, machine vec, has goal object, object vec, object id,
    # moving, has unit, unit goal, kind, can, already, command suffix
    cases = [
        (1, 0, (0x10, 0x20, 0x30), 0, (0, 0, 0, 0), 1, 0, 0, 0, 0, 1,
         "publish 0 1 0 10 20 30 0 0 0 0 0 1 0 0 0 0 1"),
        (1, 7, (0, 0, 0), 1, (0x41, 0x42, 0x43, 0x99), 1, 0, 0, 0, 0, 1,
         "publish 0 1 7 0 0 0 1 41 42 43 99 1 0 0 0 0 1"),
        (1, 7, (0, 0, 0), 1, (0x41, 0x42, 0x43, 0x99), 1, 1, 0x99, 0, 0, 1,
         "publish 1 1 7 0 0 0 1 41 42 43 99 1 99 0 0 0 1"),
        (1, 0, (0x10, 0x20, 0x30), 0, (0, 0, 0, 0), 0, 1, 0, 2, 0, 1,
         "publish 1 1 0 10 20 30 0 0 0 0 0 0 0 2 0 0 1"),
        (1, 0, (0x10, 0x20, 0x30), 0, (0, 0, 0, 0), 0, 1, 0, 1, 0, 1,
         "publish 1 1 0 10 20 30 0 0 0 0 0 0 0 1 0 0 1"),
        (1, 0, (0x10, 0x20, 0x30), 0, (0, 0, 0, 0), 0, 1, 0, 0, 1, 1,
         "publish 1 1 0 10 20 30 0 0 0 0 0 0 0 0 0 1 1"),
        (1, 0, (0x10, 0x20, 0x30), 0, (0, 0, 0, 0), 0, 1, 0, 0, 1, 0,
         "publish 1 1 0 10 20 30 0 0 0 0 0 0 0 0 0 1 0"),
    ]
    for mode, goal, vec, has_goal, goal_vec, moving, has_unit, unit_goal, kind, can, already, command in cases:
        machine, _ = boot(emu)
        owner = emu.alloc(bytes(0x380))
        put(emu, machine + 0x24, owner)
        unit = 0
        if has_unit:
            unit = emu.alloc(bytes(0x10))
            put(emu, unit, box["unit_vt"])
            put(emu, owner + 0x374, unit)
        put(emu, machine + 0x6C, mode)
        put(emu, machine + 0x28, goal)
        put(emu, machine + 0x2C, vec[0])
        put(emu, machine + 0x30, vec[1])
        put(emu, machine + 0x34, vec[2])
        if has_goal:
            goal_obj = emu.alloc(bytes(0xE0))
            put(emu, goal_obj + 0x38, goal_vec[0])
            put(emu, goal_obj + 0x3C, goal_vec[1])
            put(emu, goal_obj + 0x40, goal_vec[2])
            put(emu, goal_obj + 0xDC, goal_vec[3])
            box["goal_obj"] = goal_obj
        box["moving"] = moving
        box["unit_goal"] = unit_goal
        box["kind"] = kind
        box["can"] = can
        box["already"] = already
        raw = emu.alloc(bytes(0x100 + 12))
        out = (raw + 0xFF) & ~0xFF
        emu.write(out, struct.pack("<III", 0xAAAAAAAA, 0xBBBBBBBB, 0xCCCCCCCC))
        native = call(emu, PUBLISH_FN, [out], machine, 0, 4)
        shown = 1 if native == out else native
        words = [dword(emu, out), dword(emu, out + 4), dword(emu, out + 8)]
        compare(checker, command, formatted(shown, words, box["trace"]))


def xfer(emu: Emulator, checker: Checker) -> None:
    cases = [
        ("xfer 0 1 0 0 0", 0, 1, 0, 0, 0),
        ("xfer 2 1 0 0 4", 2, 1, 0, 0, 4),
        ("xfer 0 0 0 0 0", 0, 0, 0, 0, 0),
        ("xfer 0 0 0 2 4", 0, 0, 0, 2, 4),
        ("xfer 0 0 0 2 0", 0, 0, 0, 2, 0),
        ("xfer 0 0 1 2 4", 0, 0, 1, 2, 4),
        ("xfer 1 0 0 0 2", 1, 0, 0, 0, 2),
        ("xfer 0 0 0 1 4", 0, 0, 0, 1, 4),
    ]
    for command, count, save, byte8, post, cap in cases:
        machine, _ = boot(emu)
        begin = emu.alloc(bytes(64))
        vector = emu.alloc(struct.pack("<III", begin, begin + count * 12, begin + cap * 12))
        xfer_obj = emu.alloc(bytes(0x10))
        put(emu, xfer_obj, box["xfer_vt"])
        emu.write(xfer_obj + 4, bytes([save]))
        emu.write(xfer_obj + 8, bytes([byte8]))
        box["xfer_count"] = post
        native = call(emu, XFER, [xfer_obj, vector], machine, 0, 0)
        end = dword(emu, vector + 4)
        stored = (end - begin) // 12 if end >= begin else 0xFFFFFFFF
        shown = 1 if native == xfer_obj else native
        compare(checker, command, formatted(shown, [stored, dword(emu, begin)], box["trace"]))


if __name__ == "__main__":
    verify(Path(sys.argv[1]))
