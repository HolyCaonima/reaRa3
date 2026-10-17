"""Release oracle for the lobby-session field leaves.

Each accepted address is executed from the on-disk ra3_1.0.game image and
compared with ra3_lobbysessioncheck.  Setters are judged by the bytes they
change and by stack cleanup.  Their EAX result is not part of the C++
contract: a dword store leaves the argument in EAX, and a byte store writes
only AL.
"""

from __future__ import annotations

import argparse
import struct
import subprocess
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from emulate import Emulator  # noqa: E402

from lobby_session_table import ENTRIES  # noqa: E402

SIZE = 0x1ED8
PREDICATE = 0x00622830
BYTE_LOAD = 0x00622820


class Checker:
    def __init__(self, executable: Path) -> None:
        self.process = subprocess.Popen(
            [str(executable)], stdin=subprocess.PIPE, stdout=subprocess.PIPE,
            text=True, bufsize=1)

    def call(self, address: int, blob: bytes, argument: int | None = None) -> tuple[int, bytes]:
        assert self.process.stdin is not None and self.process.stdout is not None
        command = f"{address:x} {blob.hex()}"
        if argument is not None:
            command += f" {argument:x}"
        self.process.stdin.write(command + "\n")
        self.process.stdin.flush()
        line = self.process.stdout.readline().strip()
        if not line or line == "error":
            raise RuntimeError(f"lobbysessioncheck rejected {address:#x}")
        result, encoded = line.split()
        return int(result, 16), bytes.fromhex(encoded)

    def close(self) -> None:
        assert self.process.stdin is not None
        self.process.stdin.write("quit\n")
        self.process.stdin.flush()
        if self.process.wait(timeout=10) != 0:
            raise RuntimeError("lobbysessioncheck failed")


def pattern(seed: int) -> bytes:
    return bytes((index * 17 + seed) & 0xFF for index in range(SIZE))


def dword(blob: bytes, offset: int) -> int:
    return struct.unpack_from("<I", blob, offset)[0]


def stored(blob: bytes, kind: str, offset: int, value: int) -> bytes:
    output = bytearray(blob)
    if kind == "u8set":
        output[offset] = value & 0xFF
    elif kind == "u32set":
        struct.pack_into("<I", output, offset, value & 0xFFFFFFFF)
    else:
        raise AssertionError(kind)
    return bytes(output)


def semantic(kind: str, blob: bytes, offset: int) -> int:
    if kind == "u8get":
        return blob[offset]
    if kind == "u32get":
        return dword(blob, offset)
    if kind == "addr":
        return offset
    if kind == "nonzero":
        return int(dword(blob, offset) != 0)
    raise AssertionError(kind)


def first_difference(left: bytes, right: bytes) -> int | None:
    for index, (a, b) in enumerate(zip(left, right)):
        if a != b:
            return index
    if len(left) != len(right):
        return min(len(left), len(right))
    return None


def require(condition: bool, message: str) -> None:
    if not condition:
        raise AssertionError(message)


def require_bytes(actual: bytes, expected: bytes, message: str) -> None:
    if actual != expected:
        raise AssertionError(f"{message} +{first_difference(actual, expected):#x}")


def verify(executable: Path) -> None:
    emu = Emulator()
    checker = Checker(executable)
    obj = emu.alloc(bytes(SIZE))
    reads = [entry for entry in ENTRIES if not entry[1].endswith("set")]
    writes = [entry for entry in ENTRIES if entry[1].endswith("set")]
    getters = {(kind, offset): address
               for address, kind, offset, _cleanup in ENTRIES
               if kind in {"u8get", "u32get"}}

    def native(address: int, blob: bytes, argument: int | None = None, eax: int = 0):
        emu.write(obj, blob)
        result = emu.call(address, [] if argument is None else [argument], ecx=obj, eax=eax)
        return result, emu.read(obj, SIZE)

    def check_read(address: int, kind: str, offset: int, cleanup: int, blob: bytes, label: str) -> None:
        result, memory = native(address, blob)
        cpp, cpp_memory = checker.call(address, blob)
        expected = semantic(kind, blob, offset)
        observed = result.eax if kind != "addr" else (result.eax - obj) & 0xFFFFFFFF
        require(result.stack_cleanup == cleanup, f"{label} {address:#x} cleanup {result.stack_cleanup}")
        require(observed == expected, f"{label} {address:#x} native {observed:#x} expected {expected:#x}")
        require(cpp == expected, f"{label} {address:#x} cpp {cpp:#x} expected {expected:#x}")
        require_bytes(memory, blob, f"{label} {address:#x} native wrote")
        require_bytes(cpp_memory, blob, f"{label} {address:#x} cpp wrote")

    def check_write(address: int, kind: str, offset: int, cleanup: int, blob: bytes,
                    value: int, label: str) -> bytes:
        result, memory = native(address, blob, value)
        _cpp, cpp_memory = checker.call(address, blob, value)
        expected = stored(blob, kind, offset, value)
        require(result.stack_cleanup == cleanup, f"{label} {address:#x} cleanup {result.stack_cleanup}")
        require_bytes(memory, expected, f"{label} {address:#x} native write")
        require_bytes(cpp_memory, expected, f"{label} {address:#x} cpp write")
        return expected

    # Two distinguishing object images for every read.
    for label, blob in (("pattern", pattern(3)), ("boundary", bytes([0xFF]) * SIZE)):
        for address, kind, offset, cleanup in reads:
            check_read(address, kind, offset, cleanup, blob, label)

    # mov al preserves EAX[31:8].  The C++ method returns the byte only.
    incoming = 0xA5A5A500
    preserved, preserved_memory = native(BYTE_LOAD, pattern(3), eax=incoming)
    require(preserved.eax == incoming | pattern(3)[0xFC],
            f"byte load clobbered EAX {preserved.eax:#x}")
    require(preserved_memory == pattern(3), "preservation call wrote memory")

    # Stores of zero and of a mixed dword.  A byte store must keep the other
    # three bytes of that argument out of the object.
    for label, value in (("clear", 0), ("mixed", 0xA5A5A5A5)):
        blob = pattern(9)
        for address, kind, offset, cleanup in writes:
            written = check_write(address, kind, offset, cleanup, blob, value, label)
            getter_kind = "u8get" if kind == "u8set" else "u32get"
            getter = getters.get((getter_kind, offset))
            if getter is None:
                continue
            check_read(getter, getter_kind, offset, 0, written, f"{label}-readback")

    # Eight predicate branches, including both sides and the high bit.
    predicate = next(entry for entry in ENTRIES if entry[0] == PREDICATE)
    for value in (0, 1, 2, 0x100, 0x7FFFFFFF, 0x80000000, 0xFFFFFFFE, 0xFFFFFFFF):
        blob = bytearray(pattern(1))
        struct.pack_into("<I", blob, predicate[2], value)
        check_read(*predicate, bytes(blob), f"predicate {value:#x}")

    # One semantic mutation: invert the predicate.  It must miss both a zero
    # field and a nonzero field, while the real method still matches.
    for value in (0, 1):
        blob = bytearray(SIZE)
        struct.pack_into("<I", blob, predicate[2], value)
        result, _memory = native(PREDICATE, bytes(blob))
        mutated = int(value == 0)
        require(result.eax != mutated, f"mutation was not detected for {value:#x}")
        require(result.eax == int(value != 0), f"predicate native {result.eax:#x} for {value:#x}")
        cpp, _cpp_memory = checker.call(PREDICATE, bytes(blob))
        require(cpp == result.eax, f"mutation replacement diverged for {value:#x}")

    checker.close()
    print(f"lobby session oracle: {len(ENTRIES)} functions, "
          f"{len(reads)} reads, {len(writes)} writes")


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument(
        "--checker", type=Path,
        default=Path(__file__).resolve().parents[3] / "build/win32/bin/Release/ra3_lobbysessioncheck.exe")
    args = parser.parse_args()
    verify(args.checker)


if __name__ == "__main__":
    main()
