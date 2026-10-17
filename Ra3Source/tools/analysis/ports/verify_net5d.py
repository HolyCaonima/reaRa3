"""Release oracle for the 0x005D command-message cluster.

Each accepted address is executed from the on-disk ra3_1.0.game image and
compared with ra3_net5dcheck.  Printers are judged by the text-sink call
trace and by leaving the command object unchanged.  Field transfers are
judged by the stream-slot trace and by the bytes the transfer writes back.
The callee stubs return a stable receiver (text T/T2, xfer A/B/C) so a body
that follows the wrong pointer fails the return id.
"""

from __future__ import annotations

import argparse
import struct
import subprocess
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from emulate import Emulator  # noqa: E402
from unicorn import x86_const as uc  # noqa: E402

SIZE = 0x48
FRAME = "<frame=%d, player=%d, type=%d>"
PRINTERS = [
    0x005D67E0, 0x005D9DF0, 0x005D9EB0, 0x005D9F80, 0x005DA100, 0x005DA200,
    0x005DA2E0, 0x005DA430, 0x005DA550, 0x005DA660, 0x005DA6E0, 0x005DA790,
    0x005DA8F0, 0x005DA970, 0x005DA9F0, 0x005DAAA0, 0x005DAB30, 0x005DABC0,
    0x005DAC50, 0x005DACE0, 0x005DAD70, 0x005DAE00, 0x005DAF30, 0x005DB000,
    0x005DB190, 0x005DB270,
]
XFERS = [
    0x005D9E40, 0x005D9F00, 0x005DA030, 0x005DA160, 0x005DA520, 0x005DA5A0,
    0x005DA740, 0x005DA810, 0x005DA950, 0x005DAA50, 0x005DAED0,
]
FORWARD = 0x005DB0B0
TEXT_SLOTS = {0x14: "w", 0x20: "r", 0x24: "y", 0x30: "i", 0x34: "b", 0x3C: "t"}
XFER_SLOTS = (0x54, 0x58, 0x60, 0x6C, 0x70, 0x74, 0x7C, 0x84)


def signed(value: int) -> int:
    value &= 0xFFFFFFFF
    return value - 0x100000000 if value >= 0x80000000 else value


def hex32(value: int) -> str:
    return f"{value & 0xFFFFFFFF:08x}"


class Checker:
    def __init__(self, executable: Path) -> None:
        self.process = subprocess.Popen(
            [str(executable)], stdin=subprocess.PIPE, stdout=subprocess.PIPE,
            text=True, bufsize=1)

    def call(self, address: int, blob: bytes, flags: int = 0, extra: str = "-") -> tuple[str, str, str]:
        assert self.process.stdin is not None and self.process.stdout is not None
        self.process.stdin.write(f"{address:x} {blob.hex()} {flags:x} {extra}\n")
        self.process.stdin.flush()
        line = self.process.stdout.readline().rstrip("\r\n")
        if not line or line == "error":
            raise RuntimeError(f"net5dcheck rejected {address:#x}: {line!r}")
        parts = line.split(" ", 2)
        if len(parts) != 3:
            raise RuntimeError(f"net5dcheck bad line for {address:#x}: {line!r}")
        ret, encoded, trace = parts
        return ret, encoded, trace

    def close(self) -> None:
        assert self.process.stdin is not None
        self.process.stdin.write("quit\n")
        self.process.stdin.flush()
        if self.process.wait(timeout=10) != 0:
            raise RuntimeError("net5dcheck failed")


class Native:
    def __init__(self) -> None:
        self.emu = Emulator()
        self.trace: list[str] = []
        self.split = False
        self.text = 0
        self.text2 = 0
        self.xfer = {"A": 0, "B": 0, "C": 0}
        self.command = 0
        self._bind()

    def _bind(self) -> None:
        self.emu.bind_function(0x0040EB30, self._sprintf, 0)
        self.text_stubs = {slot: self.emu.bind_stub(self._text_stub(slot), 4) for slot in TEXT_SLOTS}
        self.xfer_stubs = {slot: self.emu.bind_stub(self._xfer_stub(slot), 4) for slot in XFER_SLOTS}
        self.ui_stub = self.emu.bind_stub(self._ui, 4)
        self.forward_stub = self.emu.bind_stub(self._forward, 4)

    def _sprintf(self, emu: Emulator) -> int:
        destination = emu.stack_arg(0)
        text = FRAME % (
            signed(emu.stack_arg(2)), signed(emu.stack_arg(3)), signed(emu.stack_arg(4)))
        emu.write(destination, text.encode("ascii") + b"\0")
        return destination

    def _text_id(self, receiver: int) -> str:
        if receiver == self.text:
            return "T"
        if receiver == self.text2:
            return "T2"
        return "?"

    def _text_stub(self, slot: int):
        kind = TEXT_SLOTS[slot]

        def stub(emu: Emulator) -> int:
            receiver = emu.register(uc.UC_X86_REG_ECX)
            argument = emu.stack_arg(0)
            if kind == "t":
                payload = emu.read(argument, 256).split(b"\0", 1)[0].hex()
            else:
                payload = hex32(argument)
            self.trace.append(f"{self._text_id(receiver)}:{kind}:{payload}")
            if kind == "t" and self.split and receiver == self.text:
                return self.text2
            return receiver

        return stub

    def _xfer_id(self, receiver: int) -> str:
        for name, address in self.xfer.items():
            if receiver == address:
                return name
        return "?"

    def _xfer_stub(self, slot: int):
        def stub(emu: Emulator) -> int:
            receiver = emu.register(uc.UC_X86_REG_ECX)
            field = emu.stack_arg(0)
            if self.command <= field < self.command + SIZE:
                where = hex32(field - self.command)
            else:
                where = "S"
            self.trace.append(f"{self._xfer_id(receiver)}:x:{slot:02x}:{where}")
            current = emu.read(field, 1)[0] ^ 0x01
            emu.write(field, bytes((current,)))
            if slot == 0x6C:
                return self.xfer["B"]
            if slot == 0x7C:
                return self.xfer["C"]
            return receiver

        return stub

    def _ui(self, _emu: Emulator) -> int:
        self.trace.append("U:u:")
        return 0

    def _forward(self, emu: Emulator) -> int:
        self.trace.append("N:n:" + hex32(emu.stack_arg(0)))
        return 0x4B

    def _vtable(self, mapping: dict[int, int]) -> int:
        table = bytearray(0x100)
        for slot, target in mapping.items():
            struct.pack_into("<I", table, slot, target)
        return self.emu.alloc(bytes(table))

    def call(self, address: int, blob: bytes, flags: int = 0, extra: str = "-") -> tuple[str, bytes, str, int]:
        self.trace = []
        self.split = (flags & 1) != 0
        image = bytearray(blob)
        self.command = self.emu.alloc(bytes(image))
        self.text = self.emu.alloc(b"\0" * 16)
        self.text2 = self.emu.alloc(b"\0" * 16)
        text_vtable = self._vtable(self.text_stubs)
        self.emu.write(self.text, struct.pack("<I", text_vtable))
        self.emu.write(self.text2, struct.pack("<I", text_vtable))
        for name in ("A", "B", "C"):
            self.xfer[name] = self.emu.alloc(b"\0" * 16)
        xfer_vtable = self._vtable(self.xfer_stubs)
        for address_ in self.xfer.values():
            self.emu.write(address_, struct.pack("<I", xfer_vtable))
        if address == 0x005DB000:
            if extra == "-":
                self.emu.write(self.command + 0x14, b"\0\0\0\0")
            else:
                caption = b"" if extra == "." else bytes.fromhex(extra)
                text = self.emu.alloc(caption + b"\0")
                block = self.emu.alloc(struct.pack("<III", 0, 0, text))
                ui_vtable = self._vtable({0x0C: self.ui_stub})
                node = self.emu.alloc(struct.pack("<III", ui_vtable, block, 0))
                self.emu.write(self.command + 0x14, struct.pack("<I", node))
        elif address == FORWARD:
            if extra == "-":
                self.emu.write(self.command + 0x14, b"\0\0\0\0")
            else:
                ui_vtable = self._vtable({0x08: self.forward_stub})
                node = self.emu.alloc(struct.pack("<III", ui_vtable, 0, 0))
                self.emu.write(self.command + 0x14, struct.pack("<I", node))
        ecx = self.command
        stack = [self.text] if address in PRINTERS else [self.xfer["A"]] if address in XFERS else []
        result = self.emu.call(address, stack, ecx=ecx)
        eax = result.eax
        if eax == self.text:
            ret = "T"
        elif eax == self.text2:
            ret = "T2"
        elif eax == self.xfer["A"]:
            ret = "A"
        elif eax == self.xfer["B"]:
            ret = "B"
        elif eax == self.xfer["C"]:
            ret = "C"
        elif eax == 0:
            ret = "0"
        elif eax == 0x4B:
            ret = "K"
        else:
            ret = f"?{eax:x}"
        return ret, self.emu.read(self.command, SIZE), ",".join(self.trace), result.stack_cleanup


def pattern(seed: int) -> bytearray:
    return bytearray((index * 17 + seed) & 0xFF for index in range(SIZE))


def put_u32(blob: bytearray, offset: int, value: int) -> None:
    struct.pack_into("<I", blob, offset, value & 0xFFFFFFFF)


def scenario_blob(seed: int, **fields: int) -> bytes:
    blob = pattern(seed)
    for offset, value in fields.items():
        put_u32(blob, int(offset, 16), value)
    return bytes(blob)


def cases() -> list[tuple[int, bytes, int, str]]:
    rows: list[tuple[int, bytes, int, str]] = []
    plain = [
        {"04": 1, "08": 2, "0c": 3, "14": 4, "18": 5},
        {"04": 0xFFFFFFFF, "08": 0, "0c": 0x7FFFFFFF, "14": 0x10, "18": 0x20},
        {"04": 0x80000000, "08": 0x80000000, "0c": 1, "14": 0xAABBCCDD, "18": 0x01020304},
    ]
    for address in PRINTERS:
        if address == 0x005D9F80:
            for flags, bits in ((0, 0x3F800000), (1, 0x40000000), (2, 0xBF800000), (3, 0x3F000000)):
                rows.append((address, scenario_blob(
                    flags + 1, **{"04": 9, "08": 8, "0c": 7, "14": 6, "18": flags,
                                  "1c": bits, "20": bits ^ 0x1000, "24": bits ^ 0x2000,
                                  "28": bits, "2c": bits + 1, "30": bits + 2, "34": bits + 3}), 0, "-"))
            rows.append((address, scenario_blob(
                9, **{"04": 1, "08": 2, "0c": 3, "14": 4, "18": 3,
                      "1c": 0x3F800000, "20": 0x40000000, "24": 0x40400000,
                      "28": 0x3F800000, "2c": 0, "30": 0x3F000000, "34": 0xBF800000}), 1, "-"))
            continue
        if address == 0x005DB000:
            rows.append((address, scenario_blob(1, **{"04": 1, "08": 2, "0c": 3}), 0, "-"))
            rows.append((address, scenario_blob(2, **{"04": 4, "08": 5, "0c": 6}), 0, b"Cap".hex()))
            rows.append((address, scenario_blob(3, **{"04": 0x80000000, "08": 1, "0c": 2}), 0, "."))
            rows.append((address, scenario_blob(4, **{"04": 1, "08": 2, "0c": 3}), 1, b"Cap".hex()))
            continue
        if address == 0x005DB190:
            for seed, frame, word in ((1, 1, 0x1234), (2, 0xFFFFFFFF, 0xFFFF), (3, 0x80000000, 0)):
                blob = pattern(seed)
                put_u32(blob, 0x04, frame)
                put_u32(blob, 0x08, 2)
                put_u32(blob, 0x0C, 3)
                put_u32(blob, 0x14, 9)
                struct.pack_into("<HH", blob, 0x18, word, (word * 3) & 0xFFFF)
                rows.append((address, bytes(blob), 0, "-"))
            rows.append((address, rows[-1][1], 1, "-"))
            continue
        for index, fields in enumerate(plain):
            rows.append((address, scenario_blob(index + 1, **fields), 0, "-"))
        rows.append((address, scenario_blob(4, **plain[0]), 1, "-"))
    for address in XFERS:
        seeds = (1, 2, 3, 4, 5, 6) if address in (0x005D9F00, 0x005DA030, 0x005DA160, 0x005DA5A0) else (1, 2, 3, 4)
        for seed in seeds:
            blob = pattern(seed * 3)
            if address == 0x005DA030:
                blob[0x18] = (0, 1, 2, 3, 0xFE, 0x7F)[seed - 1]
            if address == 0x005D9F00:
                put_u32(blob, 0x18, (0, 1, 0xAABBCCDD, 0x0000FFFF, 0x10, 0xFFFFFFFE)[seed - 1])
            rows.append((address, bytes(blob), 0, "-"))
    for player in (0, 1, 0xFFFFFFFF, 0x80000000):
        rows.append((FORWARD, scenario_blob(player & 0xFF, **{"08": player}), 0, "-"))
        rows.append((FORWARD, scenario_blob((player + 1) & 0xFF, **{"08": player, "14": 0x11111111}), 0, "1"))
    return rows


def masked(blob: bytes, address: int) -> bytes:
    if address not in (0x005DB000, FORWARD):
        return blob
    data = bytearray(blob)
    data[0x14:0x18] = b"\0\0\0\0"
    return bytes(data)


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("checker", type=Path)
    args = parser.parse_args()
    checker = Checker(args.checker)
    native = Native()
    mismatches = 0
    try:
        for address, blob, flags, extra in cases():
            cpp_ret, cpp_obj, cpp_trace = checker.call(address, blob, flags, extra)
            nat_ret, nat_obj, nat_trace, cleanup = native.call(address, blob, flags, extra)
            expected_cleanup = 0 if address == FORWARD else 4
            if cpp_ret != nat_ret or cpp_trace != nat_trace or masked(bytes.fromhex(cpp_obj), address) != masked(nat_obj, address) or cleanup != expected_cleanup:
                mismatches += 1
                print(
                    f"MISMATCH {address:08x} flags={flags} extra={extra}\n"
                    f"  cpp {cpp_ret} {cpp_trace}\n"
                    f"  nat {nat_ret} {nat_trace} cleanup={cleanup}\n"
                    f"  cppobj {cpp_obj}\n"
                    f"  natobj {nat_obj.hex()}")
                if mismatches >= 8:
                    break
            elif address in PRINTERS and address != 0x005DB000 and bytes.fromhex(cpp_obj) != blob:
                mismatches += 1
                print(f"MISMATCH {address:08x} printer wrote the command")
                if mismatches >= 8:
                    break
    finally:
        checker.close()
    print(f"compared {len(cases())} scenarios, mismatches {mismatches}")
    if mismatches:
        sys.exit(1)


if __name__ == "__main__":
    main()
