"""Release oracle for the ThingTemplate accessor cluster against ra3_1.0.game.

Straight-line leaves get a handful of distinguishing inputs. The registry and
module-record walks get one case per branch. Pointer results are compared by
the relationship the shipped instructions define (null, an offset from the
record base, or a low byte written over a pointer), because the checker and
the emulator allocate in different address spaces. Payload dwords and the
fixed map-miss sentinel 0x00CC32D4 are compared as raw EAX.
"""

from __future__ import annotations

import struct
import subprocess
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from emulate import Emulator  # noqa: E402


ALLNINE = 0x0078E7C0
ALLC0 = 0x0078E800
ANY = 0x0078E810
FIND = 0x0078BB10
PRIMARY = 0x0078CD90
SECONDARY = 0x0078CE10
PRESENT = 0x0078CE90
REMOVE = 0x0078DCA0
MOVE = 0x0078B6E0
INSTALL = 0x0078E0D0
INSTALLB = 0x0078E1C0
APPEND = 0x0078E2A0
REGISTRY_GLOBAL = 0x00DF9D20
GLOBAL_MAP = 0x00CC32D0
MASK_BYTES = 0xE8
TEMPLATE_BYTES = 0x310
NAMED_KEY = 0x977F8391


class Checker:
    def __init__(self, executable: Path) -> None:
        self.process = subprocess.Popen(
            [str(executable)], stdin=subprocess.PIPE, stdout=subprocess.PIPE,
            text=True, bufsize=1)

    def call(self, line: str) -> str:
        assert self.process.stdin is not None and self.process.stdout is not None
        self.process.stdin.write(line + "\n")
        self.process.stdin.flush()
        result = self.process.stdout.readline().strip()
        if not result or result == "error":
            raise RuntimeError(f"thingtemplatecheck rejected {line[:120]!r} -> {result!r}")
        return result

    def close(self) -> None:
        assert self.process.stdin is not None
        self.process.stdin.write("quit\n")
        self.process.stdin.flush()
        if self.process.wait(timeout=10) != 0:
            raise RuntimeError("thingtemplatecheck failed")


def require(condition: bool, message: str) -> None:
    if not condition:
        raise RuntimeError(message)


def words(*values: int) -> str:
    return " ".join(f"0x{value & 0xFFFFFFFF:x}" for value in values)


def pack_words(values: list[int]) -> bytes:
    return b"".join(struct.pack("<I", value & 0xFFFFFFFF) for value in values)


def dword(data: bytes, offset: int) -> int:
    return struct.unpack_from("<I", data, offset)[0]


def expected_allnine(query: int, object_words: list[int], query_words: list[int]) -> int:
    eax = query & 0xFFFFFFFF
    for index in range(9):
        if (object_words[index] & query_words[index]) != (query_words[index] & 0xFFFFFFFF):
            return eax & 0xFFFFFF00
        eax = (eax + 4) & 0xFFFFFFFF
    return (eax & 0xFFFFFF00) | 1


def expected_any(object_words: list[int], query_words: list[int]) -> int:
    for index in range(9):
        if (object_words[index] & query_words[index]) != 0:
            return 1
    return 0


def nine(fill: int, **replacements: int) -> list[int]:
    words_out = [fill & 0xFFFFFFFF] * 9
    for key, value in replacements.items():
        words_out[int(key)] = value & 0xFFFFFFFF
    return words_out


def verify(executable: Path) -> None:
    emu = Emulator()
    checker = Checker(executable)
    cases = 0

    def native(address: int, args: list[int], ecx: int = 0, eax: int = 0, cleanup: int = 4):
        result = emu.call(address, args, ecx=ecx, eax=eax)
        require(result.stack_cleanup == cleanup, f"{address:#x} cleanup {result.stack_cleanup}")
        return result

    def check_allnine(object_words: list[int], query_words: list[int], label: str) -> None:
        nonlocal cases
        line = f"allnine {words(*object_words)} {words(*query_words)}"
        eax_text, query_text = checker.call(line).split()
        query_host = int(query_text, 16)
        require(
            int(eax_text, 16) == expected_allnine(query_host, object_words, query_words),
            f"{label} checker")
        query_native = emu.alloc(pack_words(query_words))
        object_native = emu.alloc(pack_words(object_words))
        result = native(ALLNINE, [query_native], ecx=object_native)
        require(
            result.eax == expected_allnine(query_native, object_words, query_words),
            f"{label} native {result.eax:#x}")
        cases += 1

    def check_allc0(object_words: list[int], query_words: list[int], label: str) -> None:
        nonlocal cases
        text = checker.call(f"allc0 {words(*object_words)} {words(*query_words)}")
        eax_text, query_text, image_text = text.split()
        image = bytes.fromhex(image_text)
        require(len(image) == MASK_BYTES, label)
        require(image[:0xC0] == bytes([0xA5]) * 0xC0, f"{label} prefix")
        require(image[0xE4:] == bytes([0xA5]) * 4, f"{label} suffix")
        require(image[0xC0:0xE4] == pack_words(object_words), f"{label} mask")
        query_host = int(query_text, 16)
        require(
            int(eax_text, 16) == expected_allnine(query_host, object_words, query_words),
            f"{label} checker eax")
        native_object = bytearray([0xA5]) * MASK_BYTES
        native_object[0xC0:0xE4] = pack_words(object_words)
        object_native = emu.alloc(bytes(native_object))
        query_native = emu.alloc(pack_words(query_words))
        result = native(ALLC0, [query_native], ecx=object_native)
        require(
            result.eax == expected_allnine(query_native, object_words, query_words),
            f"{label} native eax")
        require(emu.read(object_native, MASK_BYTES) == bytes(native_object), f"{label} native image")
        cases += 1

    def check_any(object_words: list[int], query_words: list[int], label: str) -> None:
        nonlocal cases
        expected = expected_any(object_words, query_words)
        eax_text, image_text = checker.call(
            f"any {words(*object_words)} {words(*query_words)}").split()
        require(int(eax_text, 16) == expected, f"{label} checker {eax_text}")
        image = bytes.fromhex(image_text)
        require(image[:0xC0] == bytes([0xA5]) * 0xC0 and image[0xE4:] == bytes([0xA5]) * 4, label)
        native_object = bytearray([0xA5]) * MASK_BYTES
        native_object[0xC0:0xE4] = pack_words(object_words)
        object_native = emu.alloc(bytes(native_object))
        query_native = emu.alloc(pack_words(query_words))
        result = native(ANY, [query_native], ecx=object_native, eax=0xAABBCCDD)
        require(result.eax == expected, f"{label} native {result.eax:#x}")
        require(emu.read(object_native, MASK_BYTES) == bytes(native_object), f"{label} write")
        cases += 1

    zeros = nine(0)
    ones = nine(0xFFFFFFFF)
    check_allnine(ones, ones, "all nine")
    check_allnine(ones, zeros, "all zero query")
    check_allnine(nine(0xFF), nine(0x100, **{"0": 0x01}), "fail index 0")
    check_allnine(nine(0xFF, **{"4": 0x00}), nine(0x0F), "fail index 4")
    check_allnine(nine(0xFF, **{"8": 0x01}), nine(0x0F, **{"8": 0x02}), "fail index 8")
    check_allc0(ones, ones, "slot3 all")
    check_allc0(nine(0xFF, **{"0": 0}), nine(1), "slot3 fail0")
    check_allc0(zeros, zeros, "slot3 zeros")
    check_any(zeros, ones, "any miss")
    check_any(nine(0, **{"0": 0x80000000}), nine(0, **{"0": 0x80000000}), "any index 0")
    check_any(nine(0, **{"8": 0x1}), nine(0, **{"8": 0x1}), "any index 8")
    check_any(nine(0, **{"3": 0x1}), nine(0, **{"3": 0x2}), "any disjoint")
    check_any(ones, zeros, "any zero query")

    def check_find(key: int, records: list[int], label: str) -> None:
        nonlocal cases
        count = len(records) // 5
        text = checker.call(f"find {words(key)} {count} {words(*records)}")
        eax_text, begin_text = text.split()
        begin = int(begin_text, 16)
        native_records = emu.alloc(pack_words(records) + bytes(4)) if count else 0
        container = bytearray([0xA5]) * 0x20
        native_begin = native_records if count else 0
        native_end = native_begin + count * 0x14 if count else 0
        struct.pack_into("<II", container, 0x14, native_begin, native_end)
        container_native = emu.alloc(bytes(container))
        result = native(FIND, [key], ecx=container_native)
        if key in [records[index] for index in range(0, len(records), 5)]:
            hit = next(index for index in range(0, len(records), 5) if records[index] == key)
            require(int(eax_text, 16) == (begin + hit * 4) & 0xFFFFFFFF, f"{label} checker")
            require(result.eax == native_begin + hit * 4, f"{label} native")
        else:
            require(int(eax_text, 16) == 0 and result.eax == 0, label)
        require(emu.read(container_native, 0x20) == bytes(container), f"{label} container")
        cases += 1

    check_find(1, [], "find empty")
    check_find(0x10, [0x10, 1, 2, 3, 4], "find first")
    check_find(0x20, [0x10, 0, 0, 0, 0, 0x20, 5, 6, 7, 8, 0x30, 0, 0, 0, 0], "find second")
    check_find(0x99, [0x10, 0, 0, 0, 0, 0x20, 0, 0, 0, 0], "find miss")
    check_find(0, [0, 9, 8, 7, 6], "find zero key")
    check_find(0xFFFFFFFF, [1, 0, 0, 0, 0, 0xFFFFFFFF, 2, 3, 4, 5], "find high key")

    def plant_maps(memory: bytearray, base: int, mode: str, primary: list[int], secondary: list[int],
                   primary_value: int, secondary_value: int) -> None:
        if mode in {"P", "B", "Z", "N"}:
            node = base + 0x80
            value = 0 if mode == "Z" else primary_value
            struct.pack_into("<IIIIII", memory, 0x80, 0, 0, 0, 0, primary[0], value)
            struct.pack_into("<I", memory, 0x4C, node)
        if mode in {"S", "B", "Z", "N"}:
            node = base + 0xA0
            struct.pack_into("<IIIIII", memory, 0xA0, 0, 0, 0, 0, secondary[0], secondary_value)
            struct.pack_into("<I", memory, 0x30, node)

    def check_registry(kind: str, key: int, mode: str, primary: list[int], secondary: list[int],
                       label: str) -> None:
        nonlocal cases
        line = (
            f"{kind} {words(key)} {mode} {words(*primary)} {words(*secondary)}")
        checker_eax = int(checker.call(line), 16)
        blob = bytearray(0x100)
        # Registry is 0x00..0x7F, nodes are 0x80 and 0xA0, value blocks are
        # 0xC0 and 0xE0 so a node cannot overwrite a payload dword.
        struct.pack_into("<IIIII", blob, 0xC0, primary[1], primary[2], primary[3], 0, primary[4])
        struct.pack_into("<IIIII", blob, 0xE0, secondary[1], secondary[2], secondary[3], 0, secondary[4])
        address = emu.alloc(bytes(blob))
        plant_maps(blob, address, mode, primary, secondary, address + 0xC0, address + 0xE0)
        emu.write(address, bytes(blob))
        entry = {"primary": PRIMARY, "secondary": SECONDARY}[kind]
        result = native(entry, [key], ecx=address)
        require(result.eax == checker_eax, f"{label} {result.eax:#x} != {checker_eax:#x}")
        cases += 1

    empty = [0, 0, 0, 0, 0]
    primary_hit = [0x20, 0x11111111, 0x22222222, 0x33333333, 0x44444444]
    secondary_hit = [0x20, 0x55555555, 0x66666666, 0x77777777, 0x88888888]
    check_registry("primary", 0, "P", [0, 0xAB, 0, 0, 0], empty, "primary key 0")
    check_registry("primary", 7, "E", empty, empty, "primary empty")
    check_registry("primary", 0x20, "P", primary_hit, secondary_hit, "primary hit")
    check_registry("primary", 0x21, "P", primary_hit, empty, "primary greater miss")
    check_registry("primary", 0x1F, "P", primary_hit, empty, "primary lower miss")
    check_registry("primary", 0x80000000, "P", [0x7FFFFFFF, 1, 2, 3, 4], empty, "primary unsigned greater")
    check_registry("primary", 0x7FFFFFFF, "P", [0x80000000, 1, 2, 3, 4], empty, "primary unsigned lower")
    check_registry("primary", 0x20, "Z", primary_hit, secondary_hit, "primary null falls through")
    check_registry("primary", 0x20, "N", [0x20, 0, 2, 3, 4], secondary_hit, "primary zero payload")
    check_registry("primary", 0x20, "S", empty, secondary_hit, "primary secondary only")
    check_registry("primary", 0x20, "B", primary_hit, secondary_hit, "primary wins")
    check_registry("secondary", 0, "P", primary_hit, secondary_hit, "secondary key 0")
    check_registry("secondary", 4, "E", empty, empty, "secondary empty")
    check_registry("secondary", 0x20, "P", primary_hit, secondary_hit, "secondary primary field")
    check_registry("secondary", 0x20, "Z", primary_hit, secondary_hit, "secondary fall through")
    check_registry("secondary", 0x20, "S", empty, secondary_hit, "secondary only")
    check_registry("secondary", 0x19, "S", empty, secondary_hit, "secondary miss")

    def check_present(key: int, mode: str, node_key: int, residue: int, label: str) -> None:
        nonlocal cases
        checker_eax = int(checker.call(
            f"present {words(key)} {mode} {words(node_key)} {words(residue)}"), 16)
        blob = bytearray(0xA0)
        if mode != "E":
            struct.pack_into("<IIIIII", blob, 0x80, 0, 0, 0, 0, node_key, 0 if mode == "Z" else residue)
        address = emu.alloc(bytes(blob))
        if mode != "E":
            struct.pack_into("<I", blob, 0x30, address + 0x80)
            emu.write(address, bytes(blob))
        result = native(PRESENT, [key], ecx=address)
        require(result.eax == checker_eax, f"{label} {result.eax:#x} != {checker_eax:#x}")
        cases += 1

    check_present(1, "E", 0, 0, "present empty")
    check_present(0, "H", 0, 0x11223344, "present key 0")
    check_present(5, "H", 5, 0xA1B2C3D4, "present residue")
    check_present(5, "H", 5, 0x00000080, "present low byte")
    check_present(5, "H", 5, 0x00000001, "present only low")
    check_present(5, "Z", 5, 0xFFFFFFFF, "present null value")
    check_present(5, "M", 6, 0x11223344, "present miss")
    check_present(0xFFFFFFFF, "H", 0xFFFFFFFF, 0x01020304, "present high key")

    def check_remove(incoming: int, mask: int, first: list[int], second: list[int], label: str,
                     second_hit: int | None = None) -> None:
        nonlocal cases
        command = (
            f"remove {words(incoming)} {words(mask)} {len(first) // 5} {words(*first)} "
            f"{len(second) // 5} {words(*second)}")
        parts = checker.call(command).split()
        checker_eax = int(parts[0], 16)
        checker_first_begin = int(parts[1], 16)
        checker_first_end = int(parts[2], 16)
        checker_second_begin = int(parts[3], 16)
        checker_second_end = int(parts[4], 16)
        first_image = bytes.fromhex(parts[5])
        second_image = bytes.fromhex(parts[6])
        container_image = bytes.fromhex(parts[7])

        native_first = emu.alloc(pack_words(first) + bytes(4)) if first else 0
        native_second_words = list(second)
        dummy = emu.alloc(struct.pack("<I", 0x22222222))
        for index in range(1, len(native_second_words), 5):
            if native_second_words[index] == 1:
                native_second_words[index] = dummy
        native_second = emu.alloc(pack_words(native_second_words) + bytes(4)) if second else 0
        first_begin = native_first if first else 0
        first_end = first_begin + (len(first) // 5) * 0x14 if first else 0
        second_begin = native_second if second else 0
        second_end = second_begin + (len(second) // 5) * 0x14 if second else 0
        container = bytearray([0xA5]) * 0x20
        struct.pack_into("<IIII", container, 4, first_begin, first_end, 0, 0)
        struct.pack_into("<II", container, 0x14, second_begin, second_end)
        # Offset 0x0C was packed as 0 above; restore the sentinel the checker uses.
        container[0x0C:0x10] = bytes([0xA5]) * 4
        container[0x10:0x14] = bytes([0xA5]) * 4
        name_map = emu.alloc(bytes(0x20))
        container_address = emu.alloc(bytes(container))
        result = native(REMOVE, [mask], ecx=container_address, eax=incoming)
        native_container = emu.read(container_address, 0x20)
        native_first_image = emu.read(native_first, len(first_image)) if first else bytes(4)
        native_second_image = emu.read(native_second, len(second_image)) if second else bytes(4)

        if second_hit is None:
            require(checker_eax == (incoming & 0xFFFFFF00), f"{label} checker eax")
            require(result.eax == (incoming & 0xFFFFFF00), f"{label} native eax {result.eax:#x}")
            require(checker_first_end == checker_first_begin + (len(first) // 5) * 0x14
                    or (not first and checker_first_end == 0), label)
            require(dword(native_container, 8) == first_end, f"{label} native first end")
            require(dword(native_container, 0x18) == second_end, f"{label} native second end")
            first_bytes = (len(first) // 5) * 0x14
            second_bytes = (len(second) // 5) * 0x14
            require(not first or first_image[:first_bytes] == pack_words(first), f"{label} checker records")
            require(not first or native_first_image[:first_bytes] == pack_words(first),
                    f"{label} native first records")
            require(not second or native_second_image[:second_bytes] == second_image[:second_bytes],
                    f"{label} native second records")
        else:
            old_end = second_begin + (len(second) // 5) * 0x14
            require(result.eax == ((old_end & 0xFFFFFF00) | 1), f"{label} native eax {result.eax:#x}")
            require(
                checker_eax == ((checker_second_begin + (len(second) // 5) * 0x14) & 0xFFFFFF00) | 1,
                f"{label} checker eax")
            require(dword(native_container, 0x18) == old_end - 0x14, f"{label} end")
            require(checker_second_end == checker_second_begin + (len(second) // 5) * 0x14 - 0x14, label)
            require(native_second_image[:0x14] == emu.read(native_second, 0x14), label)
        require(native_container[0] == 0xA5 and native_container[0x1C] == 0xA5, f"{label} sentinels")
        require(container_image[0] == 0xA5 and container_image[0x1C] == 0xA5, f"{label} checker sentinel")
        cases += 1

    check_remove(0xAABBCCDD, 1, [], [], "remove empty")
    check_remove(0, 1, [], [], "remove empty zero eax")
    check_remove(0x000000FF, 0xFFFFFFFF, [], [], "remove empty low eax")
    # Each record is five dwords: +0, +4, +8, +C, and the byte pair in the low
    # half of the fifth dword (bytes +0x10 and +0x11).
    def rec(mask_dword: int, byte10: int, byte11: int, pointer: int = 0) -> list[int]:
        return [0x10, pointer, 0, mask_dword, byte10 | (byte11 << 8)]

    def rec2(mask_dword: int, byte_c: int, byte_d: int, pointer: int = 0) -> list[int]:
        return [0x20, pointer, mask_dword, byte_c | (byte_d << 8), 0x30]

    check_remove(0xAABBCCDD, 0x4, [
        *rec(0x1, 1, 0),
        *rec(0x4, 0, 0),
        *rec(0x4, 1, 1),
    ], [], "list1 each predicate fails")
    check_remove(0x11223300, 0x4, [], [
        *rec2(0x1, 1, 0),
        *rec2(0x4, 0, 0),
        *rec2(0x4, 1, 1),
    ], "list2 each predicate fails")
    check_remove(0x01020304, 0, [
        *rec(0x1, 1, 0),
    ], [
        *rec2(0x1, 1, 0, 0),
    ], "mask zero misses")
    check_remove(0xAABBCCDD, 0x4, [
        *rec(0x1, 1, 0),
    ], [
        *rec2(0x4, 1, 0, 1),
    ], "list2 only hit", second_hit=0)
    check_remove(0xAABBCCDD, 0x4, [], [
        *rec2(0x1, 1, 0, 1),
        *rec2(0x4, 1, 0, 1),
    ], "list2 second hits", second_hit=1)

    def check_move(mode: str, label: str) -> None:
        nonlocal cases
        if mode == "null":
            checker_eax = int(checker.call("move null"), 16)
            result = native(MOVE, [0, 0, 0], cleanup=0)
            require(checker_eax == 0 and result.eax == 0, label)
        else:
            text = checker.call("move equal")
            eax_text, dest_text, image_text = text.split()
            require(int(eax_text, 16) == int(dest_text, 16), label)
            require(bytes.fromhex(image_text) == bytes([0xA5]) * 0x20, f"{label} image")
            destination = emu.alloc(bytes([0xA5]) * 0x20)
            begin = emu.alloc(bytes([0x11, 0x22, 0x33, 0x44]))
            result = native(MOVE, [begin, begin, destination], cleanup=0, eax=0xAABBCCDD)
            require(result.eax == destination, f"{label} native")
            require(emu.read(destination, 0x20) == bytes([0xA5]) * 0x20, f"{label} native image")
        cases += 1

    check_move("null", "move null")
    check_move("equal", "move empty")

    def install_image(flag: int, key: int, float_bits: int, alternate: bool) -> bytes:
        image = bytearray([0xA5]) * TEMPLATE_BYTES
        for base in (0x194, 0x1B8, 0x1DC, 0x200):
            struct.pack_into("<II", image, base + 4, 0, 0)
            struct.pack_into("<II", image, base + 0x14, 0, 0)
        image[0x308] = flag
        if not alternate and key == NAMED_KEY:
            struct.pack_into("<I", image, 0x2F8, float_bits)
        return bytes(image)

    def check_install(alternate: bool, key: int, float_bits: int, flag: int, label: str) -> None:
        nonlocal cases
        command = "installb" if alternate else "install"
        eax_text, pair_text, image_text = checker.call(
            f"{command} {words(key)} {words(float_bits)} {words(flag)}").split()
        expected = install_image(flag, key, float_bits, alternate)
        require(bytes.fromhex(image_text) == expected, f"{label} checker image")
        registry = emu.alloc(bytes(0x80))
        previous = emu.read(REGISTRY_GLOBAL, 4)
        emu.write(REGISTRY_GLOBAL, struct.pack("<I", registry))
        data = emu.alloc(struct.pack("<III", 0, 0, float_bits))
        pair = emu.alloc(struct.pack("<III", key, data, float_bits))
        image = bytearray([0xA5]) * TEMPLATE_BYTES
        for base in (0x194, 0x1B8, 0x1DC, 0x200):
            struct.pack_into("<II", image, base + 4, 0, 0)
            struct.pack_into("<II", image, base + 0x14, 0, 0)
        image[0x308] = flag
        object_address = emu.alloc(bytes(image))
        entry = INSTALLB if alternate else INSTALL
        result = native(entry, [pair, 0], ecx=object_address, cleanup=8)
        if key == 0:
            require(int(eax_text, 16) == int(pair_text, 16), f"{label} checker eax")
            require(result.eax == pair, f"{label} native eax {result.eax:#x}")
        else:
            require(int(eax_text, 16) == GLOBAL_MAP + 4, f"{label} checker sentinel")
            require(result.eax == GLOBAL_MAP + 4, f"{label} native sentinel {result.eax:#x}")
        require(emu.read(object_address, TEMPLATE_BYTES) == expected, f"{label} native image")
        emu.write(REGISTRY_GLOBAL, previous)
        cases += 1

    check_install(False, 0, 0, 2, "install key 0")
    check_install(False, 1, 0, 0, "install plain")
    check_install(False, 1, 0, 2, "install flag without present")
    check_install(False, NAMED_KEY, 0x3F800000, 0, "install float")
    check_install(False, NAMED_KEY, 0x80000000, 2, "install negative zero")
    check_install(True, 0, 0x3F800000, 2, "installb key 0")
    check_install(True, 1, 0, 2, "installb plain")
    check_install(True, NAMED_KEY, 0x3F800000, 2, "installb ignores float")

    def check_append(flag: int, word: int, filled: int, mode: str, label: str) -> None:
        nonlocal cases
        text = checker.call(f"append {words(flag)} {words(word)} {words(filled)} {mode}")
        eax_text, object_text, vector_text, image_text, vector_image_text = text.split()
        object_host = int(object_text, 16)
        vector_host = int(vector_text, 16)
        image = bytes.fromhex(image_text)
        vector_image = bytes.fromhex(vector_image_text)
        native_vector = struct.pack("<8I", 0x11111111, 0x22222222, 0x33333333, 0x44444444,
                                    0x55555555, 0x66666666, 0x77777777, 0x88888888)
        vector_address = emu.alloc(native_vector)
        native_object = bytearray([0x5A]) * TEMPLATE_BYTES
        native_object[0x306] = flag
        if mode == "nullend":
            struct.pack_into("<III", native_object, 0x254, 0, 0, 0x100)
        else:
            struct.pack_into(
                "<III", native_object, 0x254, vector_address,
                vector_address + filled * 4, vector_address + 0x20)
        struct.pack_into("<I", native_object, 0x270, 0)
        object_address = emu.alloc(bytes(native_object))
        result = native(APPEND, [word], ecx=object_address)
        native_image = emu.read(object_address, TEMPLATE_BYTES)
        native_vector_image = emu.read(vector_address, 0x20)

        def expect(base: int, vec: int) -> tuple[int, int, int, int]:
            begin = 0 if mode == "nullend" else vec
            end = 0 if mode == "nullend" else vec + filled * 4
            if flag == 1 and mode != "nullend":
                end = begin
            if mode == "nullend":
                stored_end = 4
            else:
                stored_end = end + 4
            return begin, stored_end, base + 0x268, vec

        checker_begin, checker_end, checker_header, _ = expect(object_host, vector_host)
        native_begin, native_end, native_header, _ = expect(object_address, vector_address)
        require(int(eax_text, 16) == checker_header, f"{label} checker eax")
        require(result.eax == native_header, f"{label} native eax {result.eax:#x}")
        require(dword(image, 0x254) == checker_begin, f"{label} checker begin")
        require(dword(image, 0x258) == checker_end, f"{label} checker end")
        require(dword(image, 0x268) == checker_header, f"{label} checker header")
        require(dword(image, 0x26C) == checker_header, f"{label} checker link")
        require(dword(image, 0x270) == 0 and image[0x274] == 0, f"{label} checker cleared")
        require(dword(image, 0x278) == 0, f"{label} checker root")
        require(dword(native_image, 0x254) == native_begin, f"{label} native begin")
        require(dword(native_image, 0x258) == native_end, f"{label} native end")
        require(dword(native_image, 0x268) == native_header, f"{label} native header")
        require(dword(native_image, 0x26C) == native_header, f"{label} native link")
        require(dword(native_image, 0x270) == 0 and native_image[0x274] == 0, f"{label} native cleared")
        require(dword(native_image, 0x278) == 0, f"{label} native root")
        require(image[0x306] == (0 if flag == 1 else flag), f"{label} flag")
        require(native_image[0x306] == image[0x306], f"{label} native flag")
        require(image[0] == 0x5A and native_image[0] == 0x5A, f"{label} sentinel")
        if mode == "nullend":
            require(vector_image == native_vector and native_vector_image == native_vector, label)
        elif flag == 1:
            require(dword(vector_image, 0) == word, f"{label} collapsed word")
            require(dword(native_vector_image, 0) == word, f"{label} native word")
        else:
            require(dword(vector_image, filled * 4) == word, f"{label} appended")
            require(dword(native_vector_image, filled * 4) == word, f"{label} native appended")
        cases += 1

    check_append(0, 0xABCDEF01, 1, "room", "append room")
    check_append(2, 0, 0, "room", "append flag stays")
    check_append(1, 0x100, 3, "room", "append collapses")
    check_append(1, 0xFFFFFFFF, 0, "room", "append empty collapse")
    check_append(0, 0x55, 0, "nullend", "append null end")
    check_append(0, 0, 2, "room", "append zero word")

    mutant_words_object = zeros
    mutant_words_query = ones
    mutant = int(checker.call(
        f"anymutant {words(*mutant_words_object)} {words(*mutant_words_query)}").split()[0], 16)
    shipped = expected_any(mutant_words_object, mutant_words_query)
    require(shipped == 0 and mutant == 1 and mutant != shipped, "mutant did not disagree")
    overlap_object = ones
    overlap_query = ones
    mutant_overlap = int(checker.call(
        f"anymutant {words(*overlap_object)} {words(*overlap_query)}").split()[0], 16)
    require(expected_any(overlap_object, overlap_query) == 1 and mutant_overlap == 0,
            "mutant overlap")

    checker.close()
    print(f"thingtemplate: {cases} cases, 12 functions;")
    print("  deliberate semantic mutant: detected against shipped behavior")


if __name__ == "__main__":
    if len(sys.argv) != 2:
        raise SystemExit(f"usage: {sys.argv[0]} ra3_thingtemplatecheck.exe")
    verify(Path(sys.argv[1]))
