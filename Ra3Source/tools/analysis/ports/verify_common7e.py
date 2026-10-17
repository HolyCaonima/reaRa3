"""Release oracle for the common subsystem name slots and their sibling leaves.

Name slots are straight-line and are each called twice (ecx 0 / incoming 0,
and ecx 1 / incoming 0xA5A5A5A5).  Length, adjust, copy, forward, flag and
availability paths use the branch counts below.  One inverted +0xBA test must
disagree with the shipped predicate.
"""

from __future__ import annotations

import struct
import subprocess
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from emulate import Emulator  # noqa: E402
from unicorn import x86_const  # noqa: E402

NAMES = (
    0x007E0BA0, 0x007E1470, 0x007E24B0, 0x007E25F0, 0x007E72B0, 0x007E72C0,
    0x007E7C00, 0x007E9BC0, 0x007E9FF0, 0x007EAE70, 0x007EB200, 0x007EB480,
    0x007EBFC0, 0x007EE9B0, 0x007F0F90, 0x007F5FA0, 0x007F74A0, 0x007F7850,
    0x007F9F70, 0x007FC3A0, 0x008028F0, 0x00802900, 0x00802910,
)
TEXT_LEN = 0x007E96C0
TEXT_PLUS = 0x007E9710
ADJUST = 0x007E2DD0
COPY = 0x007E1710
FWD34 = 0x007E16F0
FWD38 = 0x007E1740
STORE_CF = 0x008052D0
BYTE_40 = 0x00805850
BYTE_D0 = 0x00805840
BYTE_PRED = 0x00805760
AVAIL = 0x00805720
INCOMING = 0xA5A5A5A5


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
            raise RuntimeError(f"common7echeck rejected {line}")
        return result

    def close(self) -> None:
        assert self.process.stdin is not None
        self.process.stdin.write("quit\n")
        self.process.stdin.flush()
        if self.process.wait(timeout=10) != 0:
            raise RuntimeError("common7echeck failed")


def require(condition: bool, message: str) -> None:
    if not condition:
        raise RuntimeError(message)


def put_u32(data: bytearray, offset: int, value: int) -> None:
    data[offset:offset + 4] = struct.pack("<I", value & 0xFFFFFFFF)


def verify(executable: Path) -> None:
    emu = Emulator()
    checker = Checker(executable)

    for index, address in enumerate(NAMES):
        for ecx, eax in ((0, 0), (1, INCOMING)):
            result = emu.call(address, [], ecx=ecx, eax=eax)
            require(result.stack_cleanup == 0, f"name cleanup {address:#x}")
            require(result.eax == int(checker.call(f"name {index}"), 16), f"name {address:#x} eax={eax:#x}")

    for length in (-1, 0, 1, 0xFFFF):
        for incoming in (0, INCOMING):
            native = text_len(emu, length, incoming)
            got = int(checker.call(f"textlen {incoming:#x} {length}"), 16)
            require(native == got, f"textlen {length} {incoming:#x}")

    for extra, length in ((0, -1), (0x80000000, -1), (3, 4), (0, 0), (1, 0xFFFF), (0xFFFFFFFF, 1)):
        native = text_plus(emu, extra, length, INCOMING)
        got = int(checker.call(f"textplus {INCOMING:#x} {extra:#x} {length}"), 16)
        require(native == got, f"textplus {extra:#x} {length}")

    for flag, start, incoming in ((1, 0, INCOMING), (0, 0, 0), (1, 0x7FFFFFFF, INCOMING), (0, 0x80000000, 0x11223344)):
        native_eax, native_image = adjust(emu, flag, start, incoming)
        eax_text, image_text = checker.call(f"adjust {flag:#x} {incoming:#x} {start:#x}").split()
        require(int(eax_text, 16) == native_eax and image_text == native_image.hex(),
                f"adjust {flag} {start:#x}")

    native = copy_null(emu, INCOMING)
    require(checker.call(f"copy null {INCOMING:#x}") == native, "copy null")

    for values in ((0x11111111, 0x22222222, 0x33333333, 0xAB), (0, 0xFFFFFFFF, 0x80000000, 0x00)):
        native = copy_live(emu, values, INCOMING)
        got = checker.call(
            f"copy live {INCOMING:#x} {values[0]:#x} {values[1]:#x} {values[2]:#x} {values[3]:#x}")
        require(got == native, f"copy live {values}")

    native = copy_alias(emu, (0x11111111, 0x22222222, 0x33333333, 0x7F), 0)
    require(checker.call("copy alias 0 0x11111111 0x22222222 0x33333333 0x7f") == native, "copy alias")

    for incoming in (0, INCOMING):
        native = forward(emu, FWD34, True, None, incoming, 0x10, 0x20, 0)
        got = checker.call(f"fwd34 null {incoming:#x} 0x10 0x20 0")
        require(got == native, f"fwd34 null {incoming:#x}")
        native = forward(emu, FWD34, True, 0xAABBCCDD, incoming, 0x10, 0x20, 0xAABBCCDD)
        got = checker.call(f"fwd34 stub {incoming:#x} 0x10 0x20 0xAABBCCDD")
        require(got == native, "fwd34 stub")

    native = forward(emu, FWD38, False, None, INCOMING, 0x55, 0, 0)
    require(checker.call(f"fwd38 null {INCOMING:#x} 0x55 0") == native, "fwd38 null residue")
    native = forward(emu, FWD38, False, 0, 0, 0x66, 0, 0)
    require(checker.call("fwd38 stub 0 0x66 0") == native, "fwd38 stub zero")
    native = forward(emu, FWD38, False, 0xFFFFFFFF, 0x01020304, 0x77, 0, 0xFFFFFFFF)
    require(checker.call("fwd38 stub 0x01020304 0x77 0xffffffff") == native, "fwd38 stub")

    for incoming, value in ((INCOMING, 0xFFFFFF3C), (0, 0), (0x11223300, 0xFF), (INCOMING, 0x100)):
        native_eax, native_image = store_cf(emu, incoming, value)
        eax_text, image_text = checker.call(f"storecf {incoming:#x} {value:#x}").split()
        require(int(eax_text, 16) == native_eax and image_text == native_image.hex(),
                f"storecf {value:#x}")

    for incoming, b8, ba, at40, atd0 in (
        (INCOMING, 0x3C, 0x00, 0x7F, 0x00),
        (INCOMING, 0x3C, 0x01, 0x00, 0xFF),
        (0, 0x00, 0x00, 0xFF, 0x80),
        (INCOMING, 0x80, 0xFF, 0x01, 0x02),
    ):
        native = bytes_case(emu, incoming, b8, ba, at40, atd0)
        got = checker.call(f"bytes {incoming:#x} {b8:#x} {ba:#x} {at40:#x} {atd0:#x}")
        require(got == native, f"bytes ba={ba:#x}")

    # ba != 0 and bc > 0 returns +0xB9.  bc <= 0 and an out-of-range index clear AL.
    # An in-range index calls vtable +0x44 with the indexed dword.
    avail_cases = (
        (1, 0x5A, 1, 0, 0, 0, "busy"),
        (1, 0x5A, 0, 0, 0, 0, "busy zero"),
        (1, 0x5A, 0x80000000, 0, 0, 0, "busy negative"),
        (0, 0, 0, 0xFFFFFFFF, 0, 0, "index negative"),
        (0, 0, 0, 0x14, 0x99, 0, "index 0x14"),
        (0, 0, 0, 0, 0x99, 0x11111111, "index 0"),
        (0, 0, 0, 0x13, 0x42, 0, "index last"),
    )
    for ba, b9, bc, index, slot, stubret, label in avail_cases:
        native = avail(emu, INCOMING, ba, b9, bc, index, slot, stubret)
        got = checker.call(
            f"avail {INCOMING:#x} {ba:#x} {b9:#x} {bc:#x} {index:#x} {slot:#x} {stubret:#x}")
        require(got == native, f"avail {label}")

    shipped = bytes_case(emu, INCOMING, 0x3C, 0, 0, 0).split()[2]
    mutant = checker.call(f"mutant {INCOMING:#x} 0x3c 0")
    require(int(shipped, 16) == 0xA5A5A53C and int(mutant, 16) == 0xA5A5A500 and mutant != shipped,
            "mutant did not disagree on ba=0")
    shipped_busy = bytes_case(emu, INCOMING, 0x3C, 1, 0, 0).split()[2]
    mutant_busy = checker.call(f"mutant {INCOMING:#x} 0x3c 1")
    require(int(shipped_busy, 16) == 0xA5A5A500 and int(mutant_busy, 16) == 0xA5A5A53C,
            "mutant did not disagree on ba!=0")

    checker.close()
    print("common7e: 34 functions, name slots x2, length/adjust/copy/forward/flag/availability cases;")
    print("  +0xBA predicate mutant disagreed with the shipped bytes")


def text_len(emu: Emulator, length: int, incoming: int) -> int:
    header = bytearray(8)
    if length >= 0:
        struct.pack_into("<H", header, 0, length & 0xFFFF)
    header_addr = emu.alloc(bytes(header))
    node_b = struct.pack("<I", 0 if length < 0 else header_addr + 4)
    node_b_addr = emu.alloc(node_b)
    node_a = struct.pack("<I", node_b_addr)
    node_a_addr = emu.alloc(node_a)
    obj = bytearray(8)
    put_u32(obj, 4, node_a_addr)
    address = emu.alloc(bytes(obj))
    result = emu.call(TEXT_LEN, [], ecx=address, eax=incoming)
    require(result.stack_cleanup == 0, "textlen cleanup")
    return result.eax


def text_plus(emu: Emulator, extra: int, length: int, incoming: int) -> int:
    header = bytearray(8)
    if length >= 0:
        struct.pack_into("<H", header, 0, length & 0xFFFF)
    header_addr = emu.alloc(bytes(header))
    link = struct.pack("<I", 0 if length < 0 else header_addr + 4)
    link_addr = emu.alloc(link)
    node = bytearray(12)
    put_u32(node, 4, extra)
    put_u32(node, 8, link_addr)
    node_addr = emu.alloc(bytes(node))
    obj = bytearray(8)
    put_u32(obj, 4, node_addr)
    address = emu.alloc(bytes(obj))
    result = emu.call(TEXT_PLUS, [], ecx=address, eax=incoming)
    require(result.stack_cleanup == 0, "textplus cleanup")
    return result.eax


def adjust(emu: Emulator, flag: int, start: int, incoming: int) -> tuple[int, bytes]:
    obj = bytearray(b"\xA5" * 0x20)
    put_u32(obj, 0x10, start)
    address = emu.alloc(bytes(obj))
    result = emu.call(ADJUST, [flag], ecx=address, eax=incoming)
    require(result.stack_cleanup == 4, "adjust cleanup")
    return result.eax, emu.read(address, 0x20)


def copy_report(eax: int, incoming: int, nested: int | None, image: bytes) -> str:
    if nested is None:
        upper = 1 if (eax & 0xFFFFFF00) == (incoming & 0xFFFFFF00) else 0
    else:
        upper = 1 if (eax & 0xFFFFFF00) == (nested & 0xFFFFFF00) else 0
    return f"{eax & 0xFF:08x} {upper} {image.hex()}"


def copy_null(emu: Emulator, incoming: int) -> str:
    obj = bytearray(b"\x11" * 0x90)
    put_u32(obj, 0x8C, 0)
    out = emu.alloc(b"\xA5" * 12)
    address = emu.alloc(bytes(obj))
    result = emu.call(COPY, [out], ecx=address, eax=incoming)
    require(result.stack_cleanup == 4, "copy null cleanup")
    return copy_report(result.eax, incoming, None, emu.read(out, 12))


def copy_live(emu: Emulator, values: tuple[int, int, int, int], incoming: int) -> str:
    nested = bytearray(b"\x22" * 0x54)
    put_u32(nested, 0x44, values[0])
    put_u32(nested, 0x48, values[1])
    put_u32(nested, 0x4C, values[2])
    nested[0x50] = values[3]
    nested_addr = emu.alloc(bytes(nested))
    obj = bytearray(b"\x11" * 0x90)
    put_u32(obj, 0x8C, nested_addr)
    out = emu.alloc(b"\xA5" * 12)
    address = emu.alloc(bytes(obj))
    result = emu.call(COPY, [out], ecx=address, eax=incoming)
    require(result.stack_cleanup == 4, "copy live cleanup")
    return copy_report(result.eax, incoming, nested_addr, emu.read(out, 12))


def copy_alias(emu: Emulator, values: tuple[int, int, int, int], incoming: int) -> str:
    nested = bytearray(b"\x22" * 0x54)
    put_u32(nested, 0x44, values[0])
    put_u32(nested, 0x48, values[1])
    put_u32(nested, 0x4C, values[2])
    nested[0x50] = values[3]
    nested_addr = emu.alloc(bytes(nested))
    obj = bytearray(b"\x11" * 0x90)
    put_u32(obj, 0x8C, nested_addr)
    address = emu.alloc(bytes(obj))
    result = emu.call(COPY, [nested_addr + 0x48], ecx=address, eax=incoming)
    require(result.stack_cleanup == 4, "copy alias cleanup")
    return copy_report(result.eax, incoming, nested_addr, emu.read(nested_addr, 0x54))


def forward(emu: Emulator, address: int, two: bool, stub_ret: int | None, incoming: int,
            arg0: int, arg1: int, expected_ret: int) -> str:
    obj = bytearray(b"\x33" * 0x90)
    calls = 0
    ecx_ok = 0
    seen_arg0 = 0
    seen_arg1 = 0
    if stub_ret is None:
        put_u32(obj, 0x8C, 0)
        this = emu.alloc(bytes(obj))
    else:
        seen: dict[str, int] = {}

        def stub(em: Emulator) -> int:
            seen["ecx"] = em.register(x86_const.UC_X86_REG_ECX)
            seen["arg0"] = em.stack_arg(0)
            if two:
                seen["arg1"] = em.stack_arg(1)
            seen["calls"] = seen.get("calls", 0) + 1
            return expected_ret

        stub_addr = emu.bind_stub(stub, 8 if two else 4)
        vtable = bytearray(16 * 4)
        slot = 0x34 if two else 0x38
        put_u32(vtable, slot, stub_addr)
        vtable_addr = emu.alloc(bytes(vtable))
        nested = struct.pack("<I", vtable_addr)
        nested_addr = emu.alloc(nested)
        put_u32(obj, 0x8C, nested_addr)
        this = emu.alloc(bytes(obj))
        # filled after the call
        seen["nested"] = nested_addr
    args = [arg0, arg1] if two else [arg0]
    result = emu.call(address, args, ecx=this, eax=incoming)
    require(result.stack_cleanup == (8 if two else 4), f"forward cleanup {address:#x}")
    if stub_ret is not None:
        calls = seen.get("calls", 0)
        seen_arg0 = seen.get("arg0", 0)
        seen_arg1 = seen.get("arg1", 0)
        ecx_ok = 1 if seen.get("ecx") == seen.get("nested") else 0
    return f"{result.eax:08x} {calls} {seen_arg0:08x} {seen_arg1:08x} {ecx_ok}"


def store_cf(emu: Emulator, incoming: int, value: int) -> tuple[int, bytes]:
    obj = bytearray(b"\x5A" * 0xD4)
    address = emu.alloc(bytes(obj))
    result = emu.call(STORE_CF, [value], ecx=address, eax=incoming)
    require(result.stack_cleanup == 4, "storecf cleanup")
    return result.eax, emu.read(address, 0xD4)


def bytes_case(emu: Emulator, incoming: int, b8: int, ba: int, at40: int, atd0: int) -> str:
    obj = bytearray(b"\x11" * 0xD8)
    obj[0xB8] = b8 & 0xFF
    obj[0xBA] = ba & 0xFF
    obj[0x40] = at40 & 0xFF
    obj[0xD0] = atd0 & 0xFF
    address = emu.alloc(bytes(obj))
    values = []
    for function in (BYTE_40, BYTE_D0, BYTE_PRED):
        result = emu.call(function, [], ecx=address, eax=incoming)
        require(result.stack_cleanup == 0, f"byte cleanup {function:#x}")
        values.append(f"{result.eax:08x}")
    return " ".join(values)


def avail(emu: Emulator, incoming: int, ba: int, b9: int, bc: int, index: int, slot: int,
          stubret: int) -> str:
    seen: dict[str, int] = {}

    def stub(em: Emulator) -> int:
        seen["ecx"] = em.register(x86_const.UC_X86_REG_ECX)
        seen["arg0"] = em.stack_arg(0)
        seen["calls"] = seen.get("calls", 0) + 1
        return stubret

    stub_addr = emu.bind_stub(stub, 4)
    vtable = bytearray(0x48)
    put_u32(vtable, 0x44, stub_addr)
    vtable_addr = emu.alloc(bytes(vtable))
    obj = bytearray(b"\xA5" * 0xC0)
    put_u32(obj, 0, vtable_addr)
    obj[0xBA] = ba & 0xFF
    obj[0xB9] = b9 & 0xFF
    put_u32(obj, 0xBC, bc)
    put_u32(obj, 0x9C, index)
    signed = struct.unpack("<i", struct.pack("<I", index & 0xFFFFFFFF))[0]
    if 0 <= signed < 0x14:
        put_u32(obj, 0x4C + signed * 4, slot)
    address = emu.alloc(bytes(obj))
    seen["this"] = address
    result = emu.call(AVAIL, [], ecx=address, eax=incoming)
    require(result.stack_cleanup == 0, "avail cleanup")
    calls = seen.get("calls", 0)
    ecx_ok = 1 if calls and seen.get("ecx") == address else 0
    arg = seen.get("arg0", 0)
    return f"{result.eax:08x} {calls} {arg:08x} {ecx_ok}"


if __name__ == "__main__":
    if len(sys.argv) != 2:
        raise SystemExit(f"usage: {sys.argv[0]} ra3_common7echeck.exe")
    verify(Path(sys.argv[1]))
