"""Differential oracle for the W3D 0x0056-0x0057 layer.

Release checker versus Unicorn. Floating-point returns are the fstp m32 bits of
ST0 (call_float32), not EAX.
"""

from __future__ import annotations

import struct
import subprocess
import sys
from pathlib import Path

from unicorn import x86_const

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from emulate import Emulator  # noqa: E402

EXE = Path(__file__).resolve().parents[3] / "build" / "win32" / "bin" / "Release" / "ra3_w3d56check.exe"
FRAME = 0x00DF4854
PAUSE = 0x00DF445D
LATCH = 0x00DF4460
FOLLOW = 0x00DF4450
LISTENER = 0x00DF4918
LFLAG = 0x00DF4924
TERRAIN = 0x00DFEA10
OWNER = 0x00DEEE78
HALFPI = 0x00CB4EB8

FN = {
    "track": (0x00562320, 0x14, False),
    "trackheld": (0x005623E0, 0x18, False),
    "uzoom": (0x00562650, 0, False),
    "upitch": (0x005626D0, 0, False),
    "uheight": (0x00562770, 0, False),
    "uangle": (0x005627F0, 0, False),
    "pause": (0x00562890, 4, False),
    "sphere": (0x0056B9E0, 8, False),
    "listen": (0x0056BA70, 0, False),
    "follow": (0x0056BCE0, 4, True),
    "bzoom": (0x0056BED0, 0x10, False),
    "bheight": (0x0056C030, 0x10, False),
    "bangle": (0x0056C0C0, 0x10, False),
    "samples": (0x0056C2B0, 4, False),
    "clearmotion": (0x0057C860, 0, False),
    "efloat": (0x00572120, 0, True),
    "eset4": (0x00572130, 4, False),
    "eset8": (0x00572140, 4, False),
    "eset12": (0x00572150, 4, False),
    "eset16": (0x00572160, 4, False),
    "bcopy": (0x005676A0, 4, False),
    "bassign": (0x005676D0, 8, False),
    "binter": (0x00567700, 4, False),
    "extent": (0x0056EE80, 4, False),
    "height": (0x005633E0, 8, True),
    "synccell": (0x005633A0, 4, False),
    "txfer": (0x005634C0, 4, False),
    "fwda": (0x00561EA0, 0x0C, False),
    "fwdb": (0x00561ED0, 0x0C, False),
    "fwdc": (0x00561F00, 8, False),
    "sample": (0x0056B2C0, 8, False),
    "latch": (0x005609F0, 8, False),
    "setflag": (0x00567550, 4, False),
    "wvec3": (0x005637D0, 8, False),
    "weq": (0x00563870, 8, False),
    "wvec4": (0x0056F2A0, 8, False),
    "wpair": (0x0056F2F0, 8, False),
    "transpose": (0x0056F540, 0, False),
    "pose": (0x00565D30, 4, False),
    "scorch": (0x005615C0, 4, False),
    "easereset": (0x0056C390, 0x18, False),
}


def u32(value: int) -> int:
    return value & 0xFFFFFFFF


def p32(value: int) -> bytes:
    return struct.pack("<I", u32(value))


def f32(value: float) -> int:
    return struct.unpack("<I", struct.pack("<f", value))[0]


def hx(data: bytes) -> str:
    return data.hex() or "."


def set_st0(emu: Emulator, bits: int) -> None:
    emu.uc.reg_write(x86_const.UC_X86_REG_FPSW, 0)
    tag = getattr(x86_const, "UC_X86_REG_FPTAG", None)
    if tag is not None:
        emu.uc.reg_write(tag, 0xFFFC)
    value = struct.unpack("<f", p32(bits))[0]
    raw = struct.unpack("<Q", struct.pack("<d", float(value)))[0]
    sign = raw >> 63
    exp = (raw >> 52) & 0x7FF
    frac = raw & ((1 << 52) - 1)
    if exp == 0 and frac == 0:
        emu.uc.reg_write(x86_const.UC_X86_REG_FP0, (0, 0x8000 if sign else 0))
        return
    exp80 = (exp - 1023 + 16383) & 0x7FFF
    mant = (1 << 63) | (frac << 11)
    emu.uc.reg_write(x86_const.UC_X86_REG_FP0, (mant, exp80 | (sign << 15)))


class Checker:
    def __init__(self) -> None:
        self.process = subprocess.Popen(
            [str(EXE)], stdin=subprocess.PIPE, stdout=subprocess.PIPE, text=True, bufsize=1
        )

    def command(self, line: str) -> dict[str, str]:
        assert self.process.stdin is not None and self.process.stdout is not None
        self.process.stdin.write(line + "\n")
        self.process.stdin.flush()
        reply = self.process.stdout.readline().strip()
        if not reply:
            raise RuntimeError(f"checker closed on {line}")
        out = {}
        for token in reply.split():
            key, value = token.split("=", 1)
            out[key] = value
        return out

    def close(self) -> None:
        if self.process.stdin is not None:
            self.process.stdin.write("quit\n")
            self.process.stdin.flush()
        self.process.wait(timeout=10)


class Side:
    def __init__(self, oracle: "Oracle", native: bool) -> None:
        self.oracle = oracle
        self.native = native
        self.slots: list[tuple[int, bytes]] = []

    def slot(self, data: bytes) -> int:
        if self.native:
            address = self.oracle.emu.alloc(data)
        else:
            reply = self.oracle.checker.command("slot " + hx(data))
            address = int(reply["ptr"], 16)
        self.slots.append((address, data))
        return address

    def xfer(self, version: int) -> int:
        if self.native:
            address = self.oracle.emu.alloc(b"\x00" * 16)
            self.oracle.emu.write(address, p32(self.oracle.xfer_vt) + p32(version))
        else:
            reply = self.oracle.checker.command(f"xfer {version:x}")
            address = int(reply["ptr"], 16)
        self.slots.append((address, p32(0) + p32(version) + b"\x00" * 8))
        return address


class Oracle:
    def __init__(self, checker: Checker) -> None:
        self.checker = checker
        self.emu = Emulator()
        self.log: list[str] = []
        self.query = 0
        self.finish = 0
        self.height_bits = 0
        self.xfer_vt = 0
        self._bind()

    def _bind(self) -> None:
        def slot(name: str, write: bool):
            def stub(emu: Emulator, name: str = name, write: bool = write) -> int:
                arg = u32(emu.stack_arg(0))
                if write:
                    emu.write(arg, b"\xA1\xA1\xA1\xA1")
                    self.log.append(name)
                else:
                    raw = emu.read(arg, 2)
                    self.log.append(f"s18:{raw[0]:x}:{raw[1]:x}")
                return 0

            return self.emu.bind_stub(stub, 4)

        table = bytearray(40 * 4)
        for offset, name, write in (
            (0x18, "s18", False),
            (0x58, "s58", True),
            (0x64, "s64", True),
            (0x70, "s70", True),
            (0x84, "s84", True),
        ):
            struct.pack_into("<I", table, offset, slot(name, write))
        self.xfer_bytes = bytes(table)
        self.xfer_vt = self.emu.alloc(self.xfer_bytes)
        self._heap_floor = self.emu._heap

        def marker(emu: Emulator, ecx: int) -> int:
            return struct.unpack("<I", emu.read(ecx, 4))[0] if ecx else 0

        def sync_a(emu: Emulator) -> int:
            self.log.append(f"a:{marker(emu, emu.register(x86_const.UC_X86_REG_ECX)):x}:{u32(emu.stack_arg(0)):x}")
            return 0

        def sync_b(emu: Emulator) -> int:
            self.log.append(f"b:{marker(emu, emu.register(x86_const.UC_X86_REG_ECX)):x}:{u32(emu.stack_arg(0)):x}")
            return 0

        def body(emu: Emulator) -> int:
            self.log.append(f"body:{marker(emu, emu.register(x86_const.UC_X86_REG_ECX)):x}")
            return 0

        def forward(tag: str, count: int):
            def stub(emu: Emulator, tag: str = tag, count: int = count) -> int:
                parts = [f"{tag}:{marker(emu, emu.register(x86_const.UC_X86_REG_ECX)):x}"]
                for index in range(count):
                    parts.append(f"{u32(emu.stack_arg(index)):x}")
                self.log.append(":".join(parts))
                return 0

            return stub

        def query(emu: Emulator) -> int:
            ecx = emu.register(x86_const.UC_X86_REG_ECX)
            self.log.append(f"q:{marker(emu, ecx):x}:{u32(emu.stack_arg(0)):x}:{u32(emu.stack_arg(1)):x}")
            return self.query

        def finish(emu: Emulator) -> int:
            self.log.append(f"fin:{marker(emu, emu.register(x86_const.UC_X86_REG_ECX)):x}:{u32(emu.stack_arg(0)):x}")
            return self.finish

        def height(emu: Emulator) -> int:
            ecx = emu.register(x86_const.UC_X86_REG_ECX)
            self.log.append(f"ht:{marker(emu, ecx):x}:{u32(emu.stack_arg(0)):x}:{u32(emu.stack_arg(1)):x}")
            set_st0(emu, self.height_bits)
            return 0

        self.emu.bind_function(0x008E4F30, sync_a, 4)
        self.emu.bind_function(0x00868470, sync_b, 4)
        self.emu.bind_function(0x00868DC0, body, 4)
        self.emu.bind_function(0x008E50B0, forward("fa", 3), 12)
        self.emu.bind_function(0x008E50D0, forward("fb", 3), 12)
        self.emu.bind_function(0x008E51A0, forward("fc", 2), 8)
        self.emu.bind_function(0x005630C0, query, 8)
        self.emu.bind_function(0x00553250, finish, 4)
        self.emu.bind_function(0x008E6DD0, height, 8)

    def reset(self) -> None:
        self.emu.reset_data()
        self.emu.write(self.xfer_vt, self.xfer_bytes)
        self.emu._heap = self._heap_floor
        self.log = []
        self.checker.command("reset")

    def set_u32(self, address: int, value: int) -> None:
        self.emu.write(address, p32(value))

    def run(self, name: str, this: bytes, args: list[int], watch: list[int], mask: list[int],
            checks: set[str], native_slots: list[tuple[int, int]], checker_slots: list[int]) -> list[str]:
        address, cleanup, want_st = FN[name]
        self.emu.uc.reg_write(x86_const.UC_X86_REG_FPCW, 0x037F)
        this_addr = self.emu.alloc(this) if name != "transpose" else 0
        if name != "transpose":
            native_args = args
            native = self.emu.call_float32(address, native_args, ecx=this_addr) if want_st else None
            if want_st:
                result, st_bits = native
            else:
                result = self.emu.call(address, native_args, ecx=this_addr)
                st_bits = 0
        else:
            result = self.emu.call(address, args, ecx=0)
            st_bits = 0
        if result.stack_cleanup != cleanup:
            return [f"{name}: cleanup {result.stack_cleanup} != {cleanup}"]
        arg_text = " ".join(f"{u32(a):x}" for a in args)
        watch_text = (" watch " + " ".join(str(i) for i in watch)) if watch else ""
        reply = self.checker.command(f"call {name} {hx(this)} {arg_text}{watch_text}".rstrip())
        problems = []
        native_log = ",".join(self.log) or "."
        if reply.get("log", ".") != native_log:
            problems.append(f"log checker={reply.get('log')} native={native_log}")
        if "st" in checks and int(reply["st"], 16) != u32(st_bits):
            problems.append(f"st checker={reply['st']} native={st_bits:x}")
        if "eax" in checks and int(reply["eax"], 16) != u32(result.eax):
            problems.append(f"eax checker={reply['eax']} native={result.eax:x}")
        if "al" in checks and int(reply["al"], 16) != (result.eax & 0xFF):
            problems.append(f"al checker={reply['al']} native={result.eax & 0xFF:x}")
        if name != "transpose":
            native_mem = bytearray(self.emu.read(this_addr, len(this)))
            checker_mem = bytearray.fromhex(reply["mem"]) if reply["mem"] != "." else bytearray()
            for offset in mask:
                for index in range(4):
                    if offset + index < len(native_mem):
                        native_mem[offset + index] = 0
                    if offset + index < len(checker_mem):
                        checker_mem[offset + index] = 0
            if native_mem != checker_mem:
                shown = next((i for i, (a, b) in enumerate(zip(native_mem, checker_mem)) if a != b), -1)
                problems.append(
                    f"mem@{shown:x} native={native_mem[shown:shown+8].hex()} checker={checker_mem[shown:shown+8].hex()}"
                    if shown >= 0 else f"memlen {len(native_mem)}!={len(checker_mem)}"
                )
        for index, slot in enumerate(watch):
            native_aux = self.emu.read(native_slots[slot][0], native_slots[slot][1])
            checker_aux = bytes.fromhex(reply[f"a{index}"]) if reply[f"a{index}"] != "." else b""
            if native_aux != checker_aux:
                problems.append(f"aux{index} mismatch")
        for key, address, size in (
            ("pause", PAUSE, 1),
            ("latch", LATCH, 4),
            ("eflag", LFLAG, 4),
            ("ear0", LISTENER, 4),
            ("ear1", LISTENER + 4, 4),
            ("ear2", LISTENER + 8, 4),
        ):
            raw = self.emu.read(address, size)
            native_value = raw[0] if size == 1 else struct.unpack("<I", raw)[0]
            if int(reply[key], 16) != native_value:
                problems.append(f"{key} checker={reply[key]} native={native_value:x}")
        return problems


def main() -> int:
    if not EXE.exists():
        print(f"missing {EXE}")
        return 1
    checker = Checker()
    oracle = Oracle(checker)
    failures = 0
    covered: dict[str, int] = {name: 0 for name in FN}

    def case(name: str, build, checks: set[str] | None = None, mask: list[int] | None = None) -> None:
        nonlocal failures
        oracle.reset()
        native = Side(oracle, True)
        port = Side(oracle, False)
        this_n, args_n, watch, frame, halfpi, pause, latch, eflag, query, finish, height = build(native)
        this_c, args_c, _, _, _, _, _, _, _, _, _ = build(port)
        oracle.checker.command(f"frame {frame}")
        oracle.set_u32(FRAME, u32(frame))
        oracle.checker.command(f"halfpi {halfpi:x}")
        oracle.set_u32(HALFPI, u32(halfpi))
        oracle.checker.command(f"pause {pause:x}")
        oracle.emu.write(PAUSE, bytes([pause & 0xFF]))
        oracle.checker.command(f"latch {latch:x}")
        oracle.set_u32(LATCH, u32(latch))
        oracle.checker.command(f"eflag {eflag:x}")
        oracle.set_u32(LFLAG, u32(eflag))
        oracle.checker.command(f"queryret {query:x}")
        oracle.checker.command(f"finret {finish:x}")
        oracle.checker.command(f"htail {height:x}")
        oracle.query = query
        oracle.finish = finish
        oracle.height_bits = height
        for kind, slot in getattr(native, "binds", []):
            oracle.set_u32({"follow": FOLLOW, "terrain": TERRAIN, "owner": OWNER}[kind], slot)
        for kind, slot in getattr(port, "binds", []):
            oracle.checker.command(f"bind {kind} {slot}")
        # Native call must use native this/args. Patch run() by doing it inline.
        address, cleanup, want_st = FN[name]
        oracle.emu.uc.reg_write(x86_const.UC_X86_REG_FPCW, 0x037F)
        this_addr = oracle.emu.alloc(this_n) if name != "transpose" else 0
        if want_st:
            result, st_bits = oracle.emu.call_float32(address, args_n, ecx=this_addr)
        else:
            result = oracle.emu.call(address, args_n, ecx=0 if name == "transpose" else this_addr)
            st_bits = 0
        problems = []
        if result.stack_cleanup != cleanup:
            problems.append(f"cleanup {result.stack_cleanup} != {cleanup}")
        arg_text = " ".join(f"{u32(a):x}" for a in args_c)
        watch_text = (" watch " + " ".join(str(i) for i in watch)) if watch else ""
        reply = oracle.checker.command(f"call {name} {hx(this_c)} {arg_text}{watch_text}".rstrip())
        native_log = ",".join(oracle.log) or "."
        if reply.get("log", ".") != native_log:
            problems.append(f"log checker={reply.get('log')} native={native_log}")
        if "st" in (checks or set()) and int(reply["st"], 16) != u32(st_bits):
            problems.append(f"st checker={reply['st']} native={st_bits:x}")
        if "eax" in (checks or set()) and int(reply["eax"], 16) != u32(result.eax):
            problems.append(f"eax checker={reply['eax']} native={result.eax:x}")
        if "al" in (checks or set()) and int(reply["al"], 16) != (result.eax & 0xFF):
            problems.append(f"al checker={reply['al']} native={result.eax & 0xFF:x}")
        if name != "transpose":
            native_mem = bytearray(oracle.emu.read(this_addr, len(this_n)))
            checker_mem = bytearray.fromhex(reply["mem"]) if reply["mem"] != "." else bytearray()
            for offset in mask or []:
                for index in range(4):
                    if offset + index < len(native_mem):
                        native_mem[offset + index] = 0
                    if offset + index < len(checker_mem):
                        checker_mem[offset + index] = 0
            if native_mem != checker_mem:
                shown = next((i for i, (a, b) in enumerate(zip(native_mem, checker_mem)) if a != b), -1)
                if shown < 0 and len(native_mem) != len(checker_mem):
                    problems.append(f"memlen {len(native_mem)}!={len(checker_mem)}")
                elif shown >= 0:
                    problems.append(
                        f"mem@{shown:x} native={native_mem[shown:shown+8].hex()} checker={checker_mem[shown:shown+8].hex()}"
                    )
        for index, slot in enumerate(watch):
            address_n, data_n = native.slots[slot]
            native_aux = oracle.emu.read(address_n, len(data_n))
            token = reply[f"a{index}"]
            checker_aux = bytes.fromhex(token) if token != "." else b""
            if native_aux != checker_aux:
                problems.append(f"aux{index} mismatch native={native_aux[:16].hex()} checker={checker_aux[:16].hex()}")
        for key, address_g, size in (
            ("pause", PAUSE, 1),
            ("latch", LATCH, 4),
            ("eflag", LFLAG, 4),
            ("ear0", LISTENER, 4),
            ("ear1", LISTENER + 4, 4),
            ("ear2", LISTENER + 8, 4),
        ):
            raw = oracle.emu.read(address_g, size)
            native_value = raw[0] if size == 1 else struct.unpack("<I", raw)[0]
            if int(reply[key], 16) != native_value:
                problems.append(f"{key} checker={reply[key]} native={native_value:x}")
        covered[name] += 1
        if problems:
            failures += 1
            print(f"FAIL {name} #{covered[name]} " + "; ".join(problems))
        else:
            print(f"ok {name} #{covered[name]}")

    def bind(side: Side, kind: str, slot_index: int) -> None:
        binds = getattr(side, "binds", [])
        binds.append((kind, side.slots[slot_index][0] if side.native else slot_index))
        side.binds = binds

    def plain(size: int, fill: int = 0x11) -> bytes:
        return bytes([fill]) * size

    def put(buf: bytearray, offset: int, value: int) -> None:
        struct.pack_into("<I", buf, offset, u32(value))

    # Isolated ease bits, not a counted entry.
    oracle.reset()
    ease = struct.pack("<ff", 0.25, 0.75)
    for position in (0.0, 0.1, 0.25, 0.5, 0.75, 1.0, -0.5, 1.5):
        bits = f32(position)
        reply = oracle.checker.command(f"ease {ease.hex()} {bits:x}")
        oracle.emu.uc.reg_write(x86_const.UC_X86_REG_FPCW, 0x037F)
        ease_addr = oracle.emu.alloc(ease)
        _result, native_bits = oracle.emu.call_float32(0x005ABCE0, [bits], ecx=ease_addr)
        if int(reply["st"], 16) != u32(native_bits):
            failures += 1
            print(f"FAIL ease pos={position} checker={reply['st']} native={native_bits:x}")
        else:
            print(f"ok ease {position}")

    def setup_numbers(frame=5, halfpi=0x3FC90FDB, pause=0, latch=0, eflag=0, query=0x11, finish=0x22, height=0x3F800000):
        return frame, halfpi, pause, latch, eflag, query, finish, height

    def view_case(name, mutate, checks=None, mask=None, size=0x2700):
        def build(side: Side):
            buf = bytearray(plain(size))
            mutate(side, buf)
            return bytes(buf), getattr(side, "args"), getattr(side, "watch", []), *setup_numbers(**getattr(side, "nums", {}))
        case(name, build, checks, mask)

    # The builders above got tangled. Use explicit builders from here.
    def run_build(name, build, checks=None, mask=None):
        case(name, build, checks, mask)

    # Redefine case body was duplicated. The function `case` already does the
    # real comparison on the second half. Callers below only use `case`.

    def make(size=0x80, fill=0x11):
        return bytearray(plain(size, fill))

    def std(side, **nums):
        side.nums = nums
        side.args = getattr(side, "args", [])
        side.watch = getattr(side, "watch", [])
        return (
            bytes(side.this), side.args, side.watch,
            *setup_numbers(**nums),
        )

    # Replace the unused view_case path with direct case() calls.
    def go(name, this, args, watch=None, mask=None, checks=None, nums=None, binds=None):
        def build(side: Side):
            side.binds = []
            local = bytearray(this)
            local_args = list(args)
            local_watch = list(watch or [])
            if binds:
                binds(side, local, local_args, local_watch)
            return bytes(local), local_args, local_watch, *setup_numbers(**(nums or {}))
        case(name, build, checks, mask)

    # ---- cases ----
    base = make(0x2700)
    put(base, 0x28, f32(2.0))
    put(base, 0x2618, 9)
    go("track", base, [f32(0.5), 10, 1, f32(0.0), f32(0.0)])
    go("track", base, [f32(-1.0), 0, 0, f32(0.2), f32(0.3)], nums={"frame": 1})
    held = make(0x2700)
    put(held, 0x2618, 4)
    go("trackheld", held, [f32(3.0), 8, 0, f32(0.1), f32(0.2), f32(6.0)])
    go("trackheld", held, [0xBF800000, 3, 6, 0, 0x3F800000, 0x40000000], nums={"frame": 2})

    zoom = make(0x500)
    put(zoom, 0x40, f32(1.5))
    put(zoom, 0x444, 4)
    put(zoom, 0x448, 0)
    put(zoom, 0x44C, f32(1.0))
    put(zoom, 0x450, f32(3.0))
    put(zoom, 0x45C, f32(0.0))
    put(zoom, 0x460, f32(1.0))
    put(zoom, 0x464, 1)
    go("uzoom", zoom, [])
    put(zoom, 0x448, 3)
    go("uzoom", zoom, [])

    pitch = make(0x500)
    put(pitch, 0x468, 2)
    put(pitch, 0x46C, 0)
    put(pitch, 0x478, f32(0.0))
    put(pitch, 0x47C, f32(90.0))
    put(pitch, 0x488, f32(0.0))
    put(pitch, 0x48C, f32(1.0))
    go("upitch", pitch, [])
    put(pitch, 0x46C, 2)
    go("upitch", pitch, [])

    height = make(0x500)
    put(height, 0x78, f32(10.0))
    put(height, 0x41C, 3)
    put(height, 0x420, 1)
    put(height, 0x428, f32(10.0))
    put(height, 0x42C, f32(40.0))
    put(height, 0x438, f32(0.25))
    put(height, 0x43C, f32(0.75))
    go("uheight", height, [])
    put(height, 0x420, 5)
    go("uheight", height, [])

    angle = make(0x500)
    put(angle, 0x30, f32(0.25))
    put(angle, 0x494, 1)
    put(angle, 0x498, 0)
    put(angle, 0x4A0, f32(0.25))
    put(angle, 0x4A4, f32(1.25))
    put(angle, 0x4B0, 0)
    put(angle, 0x4B4, 0x3F800000)
    go("uangle", angle, [])
    put(angle, 0x498, 5)
    go("uangle", angle, [])

    go("pause", make(4), [0], nums={"pause": 1})
    go("pause", make(4), [1], nums={"pause": 0})
    go("pause", make(4), [0], nums={"pause": 0})

    def sphere_bind(side, local, args, watch):
        planes = side.slot(struct.pack("<ffff", 1.0, 0.0, 0.0, 0.0) + struct.pack("<ffff", 0.0, 1.0, 0.0, 100.0))
        end = planes + 32
        put(local, 0x308, planes)
        put(local, 0x30C, end)
        center = side.slot(struct.pack("<fff", 0.5, 0.0, 0.0))
        args[:] = [center, f32(1.0)]
        watch[:] = [0, 1]
    go("sphere", make(0x320), [0, 0], mask=[0x308, 0x30C], checks={"al"}, binds=sphere_bind)

    def sphere_reject(side, local, args, watch):
        planes = side.slot(struct.pack("<ffff", 1.0, 0.0, 0.0, 0.0))
        put(local, 0x308, planes)
        put(local, 0x30C, planes + 16)
        put(local, 0x310, planes + 16)
        center = side.slot(struct.pack("<fff", 5.0, 0.0, 0.0))
        args[:] = [center, f32(1.0)]
        watch[:] = [0, 1]
    go("sphere", make(0x320), [0, 0], mask=[0x308, 0x30C, 0x310], checks={"al"}, binds=sphere_reject)

    def sphere_empty(side, local, args, watch):
        planes = side.slot(b"\x00" * 16)
        put(local, 0x308, planes)
        put(local, 0x30C, planes)
        center = side.slot(struct.pack("<fff", 1.0, 2.0, 3.0))
        args[:] = [center, f32(0.0)]
        watch[:] = [1]
    go("sphere", make(0x320), [0, 0], mask=[0x308, 0x30C], checks={"al"}, binds=sphere_empty)

    listener = make(0x180)
    put(listener, 0x15C, f32(1.0))
    put(listener, 0x16C, f32(-2.0))
    put(listener, 0x17C, f32(3.5))
    go("listen", listener, [], checks={"eax"}, nums={"eflag": 0})
    go("listen", listener, [], checks={"eax"}, nums={"eflag": 1})

    def follow_bind(side, local, args, watch):
        follow = side.slot(struct.pack("<fff", 0.0, 0.0, 2.0))
        bind(side, "follow", len(side.slots) - 1)
        put(local, 0x2624, f32(4.0))
        args[:] = [f32(8.0)]
        watch[:] = [0]
    go("follow", make(0x2630), [0], checks={"st"}, binds=follow_bind)
    def follow_one(side, local, args, watch):
        follow = side.slot(struct.pack("<fff", 0.0, 0.0, 1.0))
        bind(side, "follow", len(side.slots) - 1)
        put(local, 0x2624, f32(4.0))
        args[:] = [f32(8.0)]
        watch[:] = [0]
    go("follow", make(0x2630), [0], checks={"st"}, binds=follow_one)

    motion = make(0x500)
    put(motion, 0x40, f32(2.0))
    go("bzoom", motion, [f32(6.0), 10, f32(0.0), f32(0.0)])
    go("bzoom", motion, [f32(6.0), 1, f32(0.25), f32(0.25)], nums={"frame": 1})
    put(motion, 0x78, f32(5.0))
    go("bheight", motion, [f32(9.0), 0, 0, 0x3F800000])
    go("bheight", motion, [f32(9.0), 15, f32(0.5), f32(0.5)], nums={"frame": 5})
    put(motion, 0x30, f32(0.5))
    go("bangle", motion, [f32(1.5), 1, 0, 0], nums={"frame": 1})
    go("bangle", motion, [f32(2.5), 4, f32(0.0), f32(1.0)], nums={"frame": 4})

    def sample_bind(side, local, args, watch, current, flag, mode):
        put(local, 0x418, flag)
        put(local, 0x2594, mode)
        put(local, 0x252C, current)
        for index in range(0, 5):
            put(local, 0x4E8 + index * 0x14, f32(float(index)))
            put(local, 0x4EC + index * 0x14, f32(float(index) + 0.5))
            put(local, 0x4F0 + index * 0x14, f32(10.0))
        pos = side.slot(struct.pack("<fff", 4.0, 5.0, 6.0))
        args[:] = [pos]
        watch[:] = [0]
    go("samples", make(0x2600), [0], binds=lambda s, l, a, w: sample_bind(s, l, a, w, 3, 0, 1))
    go("samples", make(0x2600), [0], binds=lambda s, l, a, w: sample_bind(s, l, a, w, 1, 0, 1))
    go("samples", make(0x2600), [0], binds=lambda s, l, a, w: sample_bind(s, l, a, w, 3, 1, 1))
    go("samples", make(0x2600), [0], binds=lambda s, l, a, w: sample_bind(s, l, a, w, 4, 0, 0))

    dirty = bytearray(b"\xFF" * 0x2700)
    go("clearmotion", dirty, [])
    go("clearmotion", make(0x2700, 0), [])

    embedded = make(0x20)
    embedded[0x18] = 0
    go("efloat", embedded, [], checks={"st"})
    embedded[0x18] = 255
    go("efloat", embedded, [], checks={"st"})
    embedded[0x18] = 3
    go("efloat", embedded, [], checks={"st"})
    for name, bits in (("eset4", 0x3F800000), ("eset8", 0xBF800000), ("eset12", 0), ("eset16", 0x7FC00000)):
        go(name, make(0x20), [bits])
        go(name, make(0x20, 0), [0x40490FDB])

    rect = make(0x20)
    rect[8] = 7
    put(rect, 0x0C, f32(1.0))
    put(rect, 0x10, f32(2.0))
    put(rect, 0x14, f32(3.0))
    put(rect, 0x18, f32(4.0))

    def copy_bind(side, local, args, watch):
        out = side.slot(b"\x00" * 16)
        args[:] = [out]
        watch[:] = [0]
    go("bcopy", rect, [0], checks={"al"}, binds=copy_bind)
    rect0 = bytearray(rect)
    rect0[8] = 0
    go("bcopy", rect0, [0], checks={"al"}, binds=copy_bind)

    def assign_bind(side, local, args, watch, enabled):
        src = side.slot(struct.pack("<ffff", -1.0, -2.0, 5.0, 6.0))
        args[:] = [enabled, src]
        watch[:] = [0]
    go("bassign", make(0x20), [0, 0], binds=lambda s, l, a, w: assign_bind(s, l, a, w, 0))
    go("bassign", make(0x20), [0, 0], binds=lambda s, l, a, w: assign_bind(s, l, a, w, 1))

    def inter_bind(side, local, args, watch, payload):
        src = side.slot(payload)
        args[:] = [src]
        watch[:] = [0]
    empty = make(0x20)
    empty[8] = 0
    box = struct.pack("<ffff", 0.0, 0.0, 2.0, 2.0)
    go("binter", empty, [0], binds=lambda s, l, a, w: inter_bind(s, l, a, w, box))
    overlap = make(0x20)
    overlap[8] = 1
    put(overlap, 0x0C, f32(0.0))
    put(overlap, 0x10, f32(0.0))
    put(overlap, 0x14, f32(4.0))
    put(overlap, 0x18, f32(4.0))
    go("binter", overlap, [0], binds=lambda s, l, a, w: inter_bind(s, l, a, w, struct.pack("<ffff", 1.0, 1.0, 3.0, 5.0)))
    go("binter", overlap, [0], binds=lambda s, l, a, w: inter_bind(s, l, a, w, struct.pack("<ffff", 5.0, 5.0, 6.0, 6.0)))
    touch = bytearray(overlap)
    go("binter", touch, [0], binds=lambda s, l, a, w: inter_bind(s, l, a, w, struct.pack("<ffff", 4.0, 0.0, 6.0, 1.0)))

    def extent_bind(side, local, args, watch, border, span):
        child = bytearray(0x40)
        put(child, 0x2C, border)
        child_ptr = side.slot(bytes(child))
        host = bytearray(0x20)
        put(host, 0, 0x51)
        put(host, 0x14, child_ptr)
        side.slot(bytes(host))
        bind(side, "terrain", len(side.slots) - 1)
        out = side.slot(b"\x00" * 24)
        put(local, 0x40, span)
        put(local, 0x44, span + 3)
        put(local, 0x1438, f32(1.25))
        put(local, 0x143C, f32(-4.5))
        args[:] = [out]
        watch[:] = [0, 2]
    go("extent", make(0x1440), [0], binds=lambda s, l, a, w: extent_bind(s, l, a, w, 2, 10))
    go("extent", make(0x1440), [0], binds=lambda s, l, a, w: extent_bind(s, l, a, w, 0, 0))
    go("extent", make(0x1440), [0], binds=lambda s, l, a, w: extent_bind(s, l, a, w, -2, 1))

    def height_bind(side, local, args, watch, present):
        if present:
            host = bytearray(4)
            put(host, 0, 0x77)
            side.slot(bytes(host))
            bind(side, "terrain", 0)
        args[:] = [f32(1.5), f32(-2.0)]
    go("height", make(4), [0, 0], checks={"st"}, nums={"height": 0x40000000}, binds=lambda s, l, a, w: height_bind(s, l, a, w, False))
    go("height", make(4), [0, 0], checks={"st"}, nums={"height": 0x3FC00000}, binds=lambda s, l, a, w: height_bind(s, l, a, w, True))

    def sync_bind(side, local, args, watch):
        host = bytearray(4)
        put(host, 0, 0x42)
        side.slot(bytes(host))
        bind(side, "terrain", 0)
        put(local, 0, 0x43)
        args[:] = [0x1234]
    go("synccell", make(8), [0], binds=sync_bind)
    go("synccell", make(8), [0xFFFFFFFF], binds=sync_bind)

    def txfer_bind(side, local, args, watch):
        put(local, 0, 0x91)
        xfer = side.xfer(1)
        args[:] = [xfer]
    go("txfer", make(8), [0], binds=txfer_bind)
    go("txfer", make(8), [0], binds=txfer_bind)

    def fwd_bind(side, local, args, watch, present, values):
        if present:
            child = bytearray(4)
            put(child, 0, 0x55)
            ptr = side.slot(bytes(child))
            put(local, 0x28, ptr)
            watch[:] = [0]
        else:
            put(local, 0x28, 0)
        args[:] = list(values)
    go("fwda", make(0x30), [0, 0, 0], mask=[0x28], binds=lambda s, l, a, w: fwd_bind(s, l, a, w, False, [1, f32(2.0), 3]))
    go("fwda", make(0x30), [0, 0, 0], mask=[0x28], binds=lambda s, l, a, w: fwd_bind(s, l, a, w, True, [1, f32(2.0), 3]))
    go("fwdb", make(0x30), [0, 0, 0], mask=[0x28], binds=lambda s, l, a, w: fwd_bind(s, l, a, w, True, [4, 5, f32(-1.0)]))
    go("fwdb", make(0x30), [0, 0, 0], mask=[0x28], binds=lambda s, l, a, w: fwd_bind(s, l, a, w, False, [4, 5, 0]))
    go("fwdc", make(0x30), [0, 0], mask=[0x28], binds=lambda s, l, a, w: fwd_bind(s, l, a, w, True, [8, f32(0.25)]))
    go("fwdc", make(0x30), [0, 0], mask=[0x28], binds=lambda s, l, a, w: fwd_bind(s, l, a, w, False, [8, 0]))
    def sample_idx(side, local, args, watch, present):
        if present:
            terrain = bytearray(4)
            put(terrain, 0, 0x61)
            ptr = side.slot(bytes(terrain))
            put(local, 0x30, ptr)
            owner = bytearray(4)
            put(owner, 0, 0x62)
            side.slot(bytes(owner))
            bind(side, "owner", 1)
            watch[:] = [0, 1]
        else:
            put(local, 0x30, 0)
        args[:] = [f32(3.0), f32(4.0)]
    go("sample", make(0x40), [0, 0], checks={"eax"}, mask=[0x30], nums={"query": 0x10, "finish": 0x99},
       binds=lambda s, l, a, w: sample_idx(s, l, a, w, False))
    go("sample", make(0x40), [0, 0], checks={"eax"}, mask=[0x30], nums={"query": 0x10, "finish": 0x99},
       binds=lambda s, l, a, w: sample_idx(s, l, a, w, True))
    go("sample", make(0x40), [0, 0], checks={"eax"}, mask=[0x30], nums={"query": 0, "finish": 0},
       binds=lambda s, l, a, w: sample_idx(s, l, a, w, True))

    go("latch", make(4), [0x11223344, 0], checks={"eax"}, nums={"latch": 0})
    go("latch", make(4), [0x0000007F, 1], checks={"eax"}, nums={"latch": 5})
    go("latch", make(4), [0xAABBCCDD, 0], checks={"eax"}, nums={"latch": 0xFFFFFFFF})

    def flag_bind(side, local, args, watch, with_flag):
        flag = bytearray(0x40)
        flag_ptr = side.slot(bytes(flag)) if with_flag else 0
        host = bytearray(0xB0)
        put(host, 0, 1)
        put(host, 0xA8, flag_ptr)
        side.slot(bytes(host))
        bind(side, "terrain", len(side.slots) - 1)
        args[:] = [0xAB]
        watch[:] = ([0] if with_flag else [])
    go("setflag", make(4), [0], binds=lambda s, l, a, w: flag_bind(s, l, a, w, False))
    go("setflag", make(4), [0], binds=lambda s, l, a, w: flag_bind(s, l, a, w, True))
    host_only = make(4)
    def no_terrain(side, local, args, watch):
        args[:] = [1]
    go("setflag", host_only, [0], binds=no_terrain)

    def record_bind(side, local, args, watch, src, index, base_bytes=None):
        target = side.slot(b"\x00" * 0x110)
        base = side.slot(base_bytes if base_bytes is not None else b"\x22" * 64)
        put(local, 0x34, target)
        put(local, 0x40, 0x3)
        put(local, 0x44, 0xC)
        put(local, 0x4C, base)
        put(local, 0x50, base)
        src_ptr = side.slot(src)
        args[:] = [src_ptr, index]
        watch[:] = [0, 1, 2]
    go("wvec3", make(0x60), [0, 0], mask=[0x34, 0x4C, 0x50],
       binds=lambda s, l, a, w: record_bind(s, l, a, w, struct.pack("<fff", 1.0, 2.0, 3.0), 1))
    go("wvec3", make(0x60), [0, 0], mask=[0x34, 0x4C, 0x50],
       binds=lambda s, l, a, w: record_bind(s, l, a, w, struct.pack("<III", 0, 0x80000000, 0x7FC00000), 0))
    same = struct.pack("<f", 1.0)
    equal_base = bytearray(b"\x22" * 64)
    equal_base[0:4] = same
    go("weq", make(0x60), [0, 0], mask=[0x34, 0x4C, 0x50],
       binds=lambda s, l, a, w: record_bind(s, l, a, w, same, 0, bytes(equal_base)))
    other = struct.pack("<f", 2.0)
    go("weq", make(0x60), [0, 0], mask=[0x34, 0x4C, 0x50],
       binds=lambda s, l, a, w: record_bind(s, l, a, w, other, 1))
    negzero = struct.pack("<I", 0x80000000)
    signed_base = bytearray(b"\x22" * 64)
    signed_base[0:4] = b"\x00\x00\x00\x00"
    go("weq", make(0x60), [0, 0], mask=[0x34, 0x4C, 0x50],
       binds=lambda s, l, a, w: record_bind(s, l, a, w, negzero, 0, bytes(signed_base)))
    go("wvec4", make(0x60), [0, 0], mask=[0x34, 0x4C, 0x50],
       binds=lambda s, l, a, w: record_bind(s, l, a, w, struct.pack("<ffff", 1, 2, 3, 4), 1))
    go("wvec4", make(0x60), [0, 0], mask=[0x34, 0x4C, 0x50],
       binds=lambda s, l, a, w: record_bind(s, l, a, w, struct.pack("<IIII", 0, 1, 2, 3), 0))
    go("wpair", make(0x60), [0, 0], mask=[0x34, 0x4C, 0x50],
       binds=lambda s, l, a, w: record_bind(s, l, a, w, struct.pack("<ff", 8.0, 9.0), 3))
    go("wpair", make(0x60), [0, 0], mask=[0x34, 0x4C, 0x50],
       binds=lambda s, l, a, w: record_bind(s, l, a, w, struct.pack("<II", 0xFFFFFFFF, 0), 0))

    def transpose_bind(side, local, args, watch):
        dst = side.slot(b"\x00" * 64)
        src = side.slot(struct.pack("<16I", *range(16)))
        args[:] = [dst, src]
        watch[:] = [0, 1]
    go("transpose", b"\x00", [0, 0], binds=transpose_bind)
    def transpose_neg(side, local, args, watch):
        dst = side.slot(b"\xFF" * 64)
        src = side.slot(struct.pack("<16I", *([0x80000000, 0x3F800000, 0x7FC00000, 0] * 4)))
        args[:] = [dst, src]
        watch[:] = [0, 1]
    go("transpose", b"\x00", [0, 0], binds=transpose_neg)

    pose = make(0x60)
    put(pose, 0x50, f32(1.0))
    put(pose, 0x54, f32(-0.0))
    put(pose, 0x58, 0x7FC00000)

    def pose_bind(side, local, args, watch):
        out = side.slot(b"\x00" * 12)
        args[:] = [out]
        watch[:] = [0]
    go("pose", pose, [0], binds=pose_bind)
    go("pose", make(0x60, 0), [0], binds=pose_bind)

    def scorch_bind(side, local, args, watch, version):
        put(local, 4, 0x11111111)
        put(local, 0x10, 0x22222222)
        xfer = side.xfer(version)
        args[:] = [xfer]
    go("scorch", make(0x30), [0], binds=lambda s, l, a, w: scorch_bind(s, l, a, w, 1))
    go("scorch", make(0x30), [0], binds=lambda s, l, a, w: scorch_bind(s, l, a, w, 8))
    go("scorch", make(0x30), [0], binds=lambda s, l, a, w: scorch_bind(s, l, a, w, 0))

    track = make(0x2080)
    put(track, 0x2070, 0)
    go("easereset", track, [0xAABBCCDD, 1, 0, 0x3F800000, 7, 0], nums={"frame": 1})
    put(track, 0x2070, 3)
    go("easereset", track, [0x10, 0, f32(0.2), f32(0.4), 0x99, 1])
    put(track, 0x2070, -1)
    go("easereset", track, [0x10, 4, 0, 0, 3, 0], nums={"frame": 2})

    missing = [name for name, count in covered.items() if count < 2]
    print(f"cases={sum(covered.values())} failures={failures} uncovered={missing}")
    checker.close()
    return 1 if failures or missing else 0


if __name__ == "__main__":
    sys.exit(main())
