"""Differential oracle for the View cluster in 0x00520000-0x0053FFFF.

The Release checker prints eax/log/mem/st0/o0/o1/out. Unicorn runs the shipped
bytes with the same inputs. Floating results are the full 80-bit ST0.
"""

from __future__ import annotations

import struct
import subprocess
import sys
from pathlib import Path

from unicorn import x86_const

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from emulate import Emulator  # noqa: E402

EXE = Path(__file__).resolve().parents[3] / "build" / "win32" / "bin" / "Release" / "ra3_client52check.exe"

RAW_EAX = {
    "setlock", "setwide", "settall", "setorigin", "getorigin", "screen", "addxy",
    "screenif", "setb7d", "clearf", "setf", "getmode", "setmode", "clearz", "setcam",
    "getflag", "getgid", "setgid", "setg", "clearg", "hasg",
}
PTR_THIS = {"setid"}
PTR_OUT = {"copyg", "copypos", "capture"}
FLOAT_GET = {
    "getangle": 0x005298E0,
    "getpitch": 0x005298F0,
    "getzoom": 0x00529920,
    "getfov": 0x00529930,
    "getheight": 0x00529960,
    "getmax": 0x00529980,
    "getterr": 0x005299B0,
    "getzcur": 0x00529A10,
    "getextra": 0x00529A70,
    "getshake": 0x00529B90,
    "getnear": 0x00529BC0,
    "getfar": 0x00529BD0,
}
FLOAT_SET = {
    "setfov": 0x00529940,
    "setheight": 0x00529970,
    "setmax": 0x00529990,
    "setterr": 0x005299A0,
    "setextra": 0x00529A80,
    "setnear": 0x00529BE0,
    "setfar": 0x00529C00,
}
DWORD_SET = {
    "setlock": 0x00529820,
    "setwide": 0x00529830,
    "settall": 0x00529840,
    "setb7d": 0x00529950,
    "setmode": 0x00529A30,
    "setcam": 0x00529A60,
    "setgid": 0x00529AB0,
    "setid": 0x00522570,
}
DIST = {
    "distfov": 0x00529E90,
    "disthalf": 0x00522750,
}


def u32(value: int) -> int:
    return value & 0xFFFFFFFF


def p32(value: int) -> bytes:
    return struct.pack("<I", u32(value))


def hx(value: int) -> str:
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
            raise RuntimeError(line)
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
        self.notify = self.emu.bind_stub(self.on_notify, 0)

    def on_notify(self, _emu: Emulator) -> int:
        self.log.append("n16")
        return 0

    def reset(self) -> None:
        self.emu.reset_data()
        self.log = []
        self.emu.uc.reg_write(x86_const.UC_X86_REG_FPCW, 0x037F)

    def alloc(self, size: int, fill: int) -> int:
        return self.emu.alloc(bytes([fill]) * size)

    def put(self, address: int, value: int) -> None:
        self.emu.write(address, p32(value))

    def get(self, address: int) -> int:
        return struct.unpack("<I", self.emu.read(address, 4))[0]

    def install(self, obj: int) -> None:
        table = self.alloc(0x220, 0x11)
        slots = {
            0x40: self.notify,
            0x48: 0x00529D40,
            0xC0: 0x00559D20,
            0xC4: 0x00A247B0,
            0xC8: 0x005225F0,
            0xCC: 0x005298E0,
            0xD0: 0x0067F120,
            0xD4: 0x005298F0,
            0xF4: 0x00529930,
            0xF8: 0x00529940,
        }
        for offset, target in slots.items():
            self.put(table + offset, target)
        self.put(obj, table)

    def call(self, address: int, args: list[int], obj: int, floating: bool) -> tuple[int, str]:
        self.emu.uc.reg_write(x86_const.UC_X86_REG_FPCW, 0x037F)
        if floating:
            result, raw = self.emu.call_float80(address, args, ecx=obj)
            return result.eax, raw.hex()
        result = self.emu.call(address, args, ecx=obj)
        return result.eax, "."


def parse(command: str) -> tuple[list[tuple[int, int]], str, list[int], list[int]]:
    tokens = command.split()
    words = {"fill", "u", "call", "mem"}
    pokes: list[tuple[int, int]] = []
    name = ""
    args: list[int] = []
    mem: list[int] = []
    i = 0
    while i < len(tokens):
        word = tokens[i]
        if word == "fill":
            i += 1
        elif word == "u":
            pokes.append((int(tokens[i + 1], 16), int(tokens[i + 2], 16)))
            i += 3
        elif word == "call":
            name = tokens[i + 1]
            i += 2
            while i < len(tokens) and tokens[i] not in words:
                args.append(int(tokens[i], 16))
                i += 1
        elif word == "mem":
            i += 1
            while i < len(tokens) and tokens[i] not in words:
                mem.append(int(tokens[i], 16))
                i += 1
        else:
            raise RuntimeError(f"bad token {word}")
    return pokes, name, args, mem


def execute(oracle: Oracle, command: str) -> dict[str, str]:
    pokes, name, args, mem = parse(command)
    oracle.reset()
    obj = oracle.alloc(0xC0, 0xA5)
    oracle.install(obj)
    for offset, value in pokes:
        oracle.put(obj + offset, value)
    eax = 0
    st0 = "."
    o0 = "."
    o1 = "."
    out = "."
    floating = name in FLOAT_GET or name in DIST
    if name in FLOAT_SET:
        eax, st0 = oracle.call(FLOAT_SET[name], [args[0]], obj, False)
        st0 = "."
        eax = 0
    elif name in FLOAT_GET:
        _eax, st0 = oracle.call(FLOAT_GET[name], [], obj, True)
        eax = 0
    elif name == "setshake":
        oracle.call(0x00529BA0, [args[0], args[1]], obj, False)
    elif name in DWORD_SET and name != "setid":
        eax, _st0 = oracle.call(DWORD_SET[name], [args[0]], obj, False)
    elif name == "setid":
        eax, _st0 = oracle.call(0x00522570, [args[0]], obj, False)
        eax = 1 if eax == obj else 0
    elif name == "setorigin":
        eax, _st0 = oracle.call(0x00529850, [args[0], args[1]], obj, False)
    elif name == "getorigin":
        dest = oracle.alloc(8, 0)
        eax, _st0 = oracle.call(0x00529870, [dest, dest + 4], obj, False)
        o0 = hx(oracle.get(dest))
        o1 = hx(oracle.get(dest + 4))
    elif name == "screen":
        point = oracle.alloc(8, 0)
        oracle.put(point, args[0])
        oracle.put(point + 4, args[1])
        eax, _st0 = oracle.call(0x00529D40, [point], obj, False)
    elif name == "addxy":
        point = oracle.alloc(8, 0)
        oracle.put(point, args[0])
        oracle.put(point + 4, args[1])
        eax, _st0 = oracle.call(0x00529DB0, [point], obj, False)
    elif name == "screenif":
        if len(args) >= 2:
            point = oracle.alloc(8, 0)
            oracle.put(point, args[0])
            oracle.put(point + 4, args[1])
            stack = [point, 0, 0, 0, 0, 0]
        else:
            stack = [0, 0, 0, 0, 0, 0]
        eax, _st0 = oracle.call(0x00529890, stack, obj, False)
    elif name == "clamp":
        oracle.call(0x005225F0, [args[0]], obj, False)
    elif name == "push":
        oracle.call(0x00522530, [args[0]], obj, False)
    elif name == "shake":
        oracle.call(0x00529B40, [args[0]], obj, False)
    elif name == "copydef":
        oracle.call(0x00522620, [], obj, False)
    elif name == "init":
        oracle.call(0x00529C60, [], obj, False)
    elif name == "viewport":
        oracle.call(0x00531330, [], obj, False)
    elif name == "scaledown":
        oracle.call(0x00522580, [], obj, False)
    elif name == "scaleup":
        oracle.call(0x005225B0, [], obj, False)
    elif name == "clearf":
        eax, _st0 = oracle.call(0x005299C0, [args[0]], obj, False)
    elif name == "ready":
        oracle.call(0x005299E0, [], obj, False)
    elif name == "setf":
        eax, _st0 = oracle.call(0x005299F0, [args[0], args[1]], obj, False)
    elif name == "getmode":
        eax, _st0 = oracle.call(0x00529A20, [], obj, False)
    elif name == "clearz":
        eax, _st0 = oracle.call(0x00529A40, [], obj, False)
    elif name == "getflag":
        eax, _st0 = oracle.call(0x00529A90, [], obj, False)
    elif name == "getgid":
        eax, _st0 = oracle.call(0x00529AA0, [], obj, False)
    elif name == "setg":
        eax, _st0 = oracle.call(0x00529AC0, [args[0], args[1]], obj, False)
    elif name == "clearg":
        eax, _st0 = oracle.call(0x00529AE0, [], obj, False)
    elif name == "hasg":
        eax, _st0 = oracle.call(0x00529AF0, [], obj, False)
    elif name in ("copyg", "copypos", "capture"):
        dest = oracle.alloc(32, 0x5A)
        address = {"copyg": 0x00529B10, "copypos": 0x00529900, "capture": 0x00529DE0}[name]
        eax, _st0 = oracle.call(address, [dest], obj, False)
        eax = 1 if eax == dest else 0
        out = oracle.emu.read(dest, 32).hex()
    elif name == "apply":
        state = oracle.alloc(0x20, 0)
        oracle.emu.write(state, bytes([args[0] & 0xFF]))
        oracle.put(state + 4, args[1])
        oracle.put(state + 8, args[2])
        oracle.put(state + 0x10, args[3])
        oracle.put(state + 0x14, args[4])
        oracle.put(state + 0x18, args[5])
        oracle.put(state + 0x1C, args[6])
        oracle.call(0x00522630, [state], obj, False)
    elif name in DIST:
        _eax, st0 = oracle.call(DIST[name], [args[0]], obj, True)
        eax = 0
    else:
        raise RuntimeError(f"unknown {name}")
    if name not in RAW_EAX and name not in PTR_THIS and name not in PTR_OUT:
        eax = 0
    mem_text = "." if not mem else ",".join(f"{offset:x}:{oracle.get(obj + offset):08x}" for offset in mem)
    return {
        "eax": hx(eax),
        "log": ",".join(oracle.log) if oracle.log else ".",
        "mem": mem_text,
        "st0": st0,
        "o0": o0,
        "o1": o1,
        "out": out,
    }


def cases() -> list[str]:
    floats = [0, 0x3F800000, 0xBF800000, 0x7F800000, 0xFF800000, 0x7FC00000, 0x80000000, 0x4B000000]
    ints = [0, 1, 0xFFFFFFFF, 0x1AB, 0x80000000, 0x7FFFFFFF]
    out: list[str] = []
    for name, _address in FLOAT_SET.items():
        offset = {
            "setfov": "44", "setheight": "58", "setmax": "54", "setterr": "74",
            "setextra": "78", "setnear": "9c", "setfar": "a4",
        }[name]
        for bits in floats:
            out.append(f"fill call {name} {bits:x} mem {offset}")
    for name, _address in FLOAT_GET.items():
        offset = {
            "getangle": "2c", "getpitch": "30", "getzoom": "40", "getfov": "44",
            "getheight": "58", "getmax": "54", "getterr": "74", "getzcur": "6c",
            "getextra": "78", "getshake": "98", "getnear": "9c", "getfar": "a4",
        }[name]
        for bits in floats:
            out.append(f"fill u {offset} {bits:x} call {name} mem {offset}")
    for name, offset in (("setwide", "18"), ("settall", "1c"), ("setmode", "60"), ("setgid", "88")):
        for bits in ints:
            out.append(f"fill call {name} {bits:x} mem {offset}")
    for name, offset in (("setlock", 0x48), ("setb7d", 0x7C), ("setcam", 0x7C)):
        for bits in (0, 1, 0x1AB, 0xFF, 0xA5, 0x100):
            out.append(f"fill call {name} {bits:x} mem {offset:x}")
    for bits in ints:
        out.append(f"fill call setid {bits:x} mem 4")
        out.append(f"fill call setorigin {bits:x} {(bits ^ 0x5A5A5A5A) & 0xFFFFFFFF:x} mem 20 24")
        out.append(f"fill call setg {bits:x} {(bits + 3) & 0xFFFFFFFF:x} mem 8c 90")
    for a, b in ((0, 0), (0x11, 0x22), (0xFFFFFFFF, 1), (5, 0xFFFFFFFF), (0xFFFFFFFF, 0xFFFFFFFF), (0x80000000, 0)):
        out.append(f"fill u 20 {a:x} u 24 {b:x} call getorigin mem 20 24")
        out.append(f"fill u 8c {a:x} u 90 {b:x} call hasg mem 8c 90")
    for bits in (0, 0x3F000000, 0x3F800000, 0xBF800000, 0xBF20D97C, 0x3F20D97C, 0x7FC00000, 0x7F800000, 0xFF800000, 0x80000000):
        out.append(f"fill call clamp {bits:x} mem 2c")
    for current, fov, arg in (
        (0, 0x40000000, 0x40400000),
        (0x80000000, 0x40000000, 0x3F800000),
        (0x3F800000, 0x40000000, 0x40400000),
        (0x7FC00000, 0x3F800000, 0),
        (0x7F800000, 0x3F800000, 0xBF800000),
        (0x3F800000, 0x7FC00000, 0xBF800000),
    ):
        out.append(f"fill u 6c {current:x} u 44 {fov:x} call push {arg:x} mem 6c 70 44")
    for current, delta in (
        (0, 0x3F800000), (0x3F800000, 0xBF800000), (0x40000000, 0),
        (0x3F800000, 0xBF000000), (0, 0x7F800000), (0x3F800000, 0xFF800000),
        (0x7FC00000, 0x3F800000), (0x3F800000, 0x7FC00000),
    ):
        out.append(f"fill u 98 {current:x} call shake {delta:x} mem 98")
    for x, y, width, height, z in (
        (0x3F800000, 0x40000000, 0x280, 0x1E0, 0x40400000),
        (0, 0, 0, 0, 0),
        (0x3F800000, 0x3F800000, 0xFFFFFFFF, 0xFFFFFFFF, 0x7FC00000),
        (0xBF800000, 0x7F800000, 0x80000000, 1, 0x80000000),
        (0x7FC00000, 0, 0x100, 0x100, 0x3F800000),
        (0x42C80000, 0x42C80000, 0x280, 0x1E0, 0),
    ):
        out.append(f"fill u 18 {width:x} u 1c {height:x} u 14 {z:x} call screen {x:x} {y:x} mem c 10 14")
        out.append(f"fill u 18 {width:x} u 1c {height:x} u 14 {z:x} call screenif {x:x} {y:x} mem c 10 14")
    out.append("fill u c 11111111 u 10 22222222 u 14 33333333 call screenif 0 mem c 10 14")
    for px, py, x, y in (
        (0, 0, 0x3F800000, 0xBF800000),
        (0x3F800000, 0x3F800000, 0x3F800000, 0x3F800000),
        (0x7FC00000, 0x3F800000, 0x7F800000, 0),
        (0x80000000, 0xFF800000, 0x3F800000, 0x7FC00000),
    ):
        out.append(f"fill u c {px:x} u 10 {py:x} call addxy {x:x} {y:x} mem c 10 14")
    for low, high in ((0, 0x3F800000), (0xBF800000, 0x7FC00000), (0x7F800000, 0xFF800000)):
        out.append(f"fill call setshake {low:x} {high:x} mem ac b0")
    for a, b in ((0x3F800000, 0x40000000), (0x7FC00000, 0x80000000), (0, 0x7F800000)):
        out.append(f"fill u 4c {a:x} u 50 {b:x} call copydef mem 28 2c 4c 50")
    for fov in (0, 0x3F800000, 0x40000000, 0xBF800000, 0x3F000000, 0x42B40000):
        out.append(f"fill u 44 {fov:x} call scaledown mem 44")
        out.append(f"fill u 44 {fov:x} call scaleup mem 44")
    init_mem = "4 8 18 1c 20 24 c 10 14 28 2c 30 34 38 3c 40 44 48 4c 50 54 58 5c 60 64 68 6c 70 74 78 7c 80 84 88 8c 90 94 98 9c a0 a4 a8 ac b0 b4"
    out.append(f"fill call init mem {init_mem}")
    out.append(f"fill u 44 aabbccdd u 30 01020304 call viewport mem {init_mem}")
    for ident, value in ((0, 0), (0x11, 0x3F800000), (0xFFFFFFFF, 0x7FC00000)):
        out.append(f"fill call clearf {ident:x} mem 5c 64 68")
        out.append(f"fill call setf {ident:x} {value:x} mem 5c 64 68")
    out.append("fill call ready mem 7c")
    out.append("fill u 5c 1 u 60 2 u 6c 3f800000 u 70 40000000 call clearz mem 5c 60 6c 70")
    out.append("fill call clearg mem 8c 90 94")
    for a, b, c in ((1, 2, 3), (0xFFFFFFFF, 0, 0x80000000), (0, 0, 0x7FC00000)):
        out.append(f"fill u 8c {a:x} u 90 {b:x} u 94 {c:x} call copyg")
        out.append(f"fill u c {a:x} u 10 {b:x} u 14 {c:x} call copypos")
        out.append(f"fill u c {a:x} u 10 {b:x} u 14 {c:x} u 28 {c:x} u 2c {a:x} u 30 {b:x} u 44 {a:x} call capture")
    for flag in (0, 1):
        out.append(
            "fill u 18 280 u 1c 1e0 u 14 3f800000 "
            f"call apply {flag:x} 42c80000 42c80000 3f000000 3f800000 bf000000 40000000 "
            "mem c 10 14 28 2c 30 44"
        )
        out.append(
            "fill u 18 0 u 1c 0 u 14 0 "
            f"call apply {flag:x} 0 0 0 7fc00000 7f800000 80000000 "
            "mem c 10 14 28 2c 30 44"
        )
    for bits in (0, 0x3F800000, 0x3F000000, 0xBF800000, 0x3E4CCCCD, 0x42B40000, 0x3FC00000, 0x3727C5AC):
        out.append(f"fill call distfov {bits:x}")
        out.append(f"fill call disthalf {bits:x}")
    for bits in (0x11, 0xFFFFFFFF, 0):
        out.append(f"fill u 60 {bits:x} call getmode mem 60")
        out.append(f"fill u 88 {bits:x} call getgid mem 88")
        out.append(f"fill u 84 {bits:x} call getflag mem 84")
    return out


def main() -> int:
    checker = Checker(EXE)
    oracle = Oracle()
    failures: list[str] = []
    try:
        for command in cases():
            try:
                got = checker.call(command)
                exp = execute(oracle, command)
            except Exception as error:  # noqa: BLE001 - surface the failing command
                failures.append(f"{command}\n  {error}")
                continue
            for key in ("eax", "log", "mem", "st0", "o0", "o1", "out"):
                if got.get(key) != exp.get(key):
                    failures.append(
                        f"{command} {key}\n  cpp {got.get(key)}\n  emu {exp.get(key)}"
                    )
                    break
    finally:
        checker.close()
    print(f"oracle mismatches={len(failures)} compared={len(cases())}")
    for failure in failures[:40]:
        print(failure)
    return 1 if failures else 0


if __name__ == "__main__":
    sys.exit(main())
