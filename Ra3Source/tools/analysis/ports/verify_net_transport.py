"""Differential checks for the clean RA3 network transport helpers."""

from __future__ import annotations

import argparse
import random
import struct
import subprocess
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from emulate import Emulator  # noqa: E402


CASES = 500


class Checker:
    def __init__(self, executable: Path) -> None:
        self.process = subprocess.Popen(
            [str(executable)], stdin=subprocess.PIPE, stdout=subprocess.PIPE,
            text=True, bufsize=1)

    def call(self, command: str) -> str:
        assert self.process.stdin is not None and self.process.stdout is not None
        self.process.stdin.write(command + "\n")
        self.process.stdin.flush()
        result = self.process.stdout.readline().strip()
        if not result or result == "error":
            raise RuntimeError(f"netcheck rejected {command!r}")
        return result

    def close(self) -> None:
        assert self.process.stdin is not None
        self.process.stdin.write("quit\n")
        self.process.stdin.flush()
        if self.process.wait(timeout=10) != 0:
            raise RuntimeError("netcheck failed")


def u32(data: bytes, offset: int) -> int:
    return struct.unpack_from("<I", data, offset)[0]


def cpp_memory(checker: Checker, operation: str, data: bytes, *arguments: object):
    command = " ".join([operation, data.hex() or ".", *(str(value) for value in arguments)])
    result, encoded = checker.call(command).split()
    return int(result), bytes.fromhex(encoded)


def verify(executable: Path) -> None:
    rng = random.Random(0x4E455431)
    emu = Emulator()
    checker = Checker(executable)
    totals: dict[str, int] = {}

    def memcpy_stub(machine: Emulator) -> int:
        destination = machine.stack_arg(0)
        source = machine.stack_arg(1)
        size = machine.stack_arg(2)
        machine.write(destination, machine.read(source, size))
        return destination

    emu.bind_function(0x004D2EDA, memcpy_stub)
    memmove_stub = emu.bind_stub(memcpy_stub)

    def strncpy_stub(machine: Emulator) -> int:
        destination = machine.stack_arg(0)
        source = machine.stack_arg(1)
        count = machine.stack_arg(2)
        text = bytearray()
        for index in range(count):
            value = machine.read(source + index, 1)[0]
            text.append(value)
            if value == 0:
                text.extend(b"\0" * (count - index - 1))
                break
        machine.write(destination, bytes(text))
        return destination

    strncpy_address = emu.bind_stub(strncpy_stub)

    def counted(name: str) -> None:
        totals[name] = totals.get(name, 0) + 1

    try:
        for _ in range(CASES):
            emu.reset_data()
            data = bytearray(rng.randbytes(0x80))
            pointer = emu.alloc(bytes(data))
            game = emu.call(0x005D6000, ecx=pointer)
            ours, output = cpp_memory(checker, "clear", bytes(data))
            assert (game.eax, game.stack_cleanup, emu.read(pointer, len(data))) == (ours, 0, output)
            counted("clearPairPointers")

        for _ in range(CASES):
            emu.reset_data()
            emu.write(0x00BBA544, struct.pack("<I", strncpy_address))
            source_text = rng.randbytes(rng.randrange(80)).replace(b"\0", b"\x01") + b"\0"
            destination = rng.randbytes(rng.randrange(len(source_text), 129))
            end_offset = rng.choice((-1, 0, rng.randrange(len(destination) + 1)))
            destination_pointer = emu.alloc(destination)
            source_pointer = emu.alloc(source_text)
            end_pointer = 0 if end_offset < 0 else destination_pointer + end_offset
            game = emu.call(0x005D64C0, [destination_pointer, source_pointer, end_pointer])
            ours, output = cpp_memory(
                checker, "copybounded", destination, source_text.hex(),
                (2**64 - 1) if end_offset < 0 else end_offset)
            assert (game.eax - destination_pointer, game.stack_cleanup,
                    emu.read(destination_pointer, len(destination))) == (ours, 0, output)
            counted("copyBounded")

        for _ in range(CASES):
            emu.reset_data()
            data = bytearray(rng.randbytes(0x500))
            pointer = emu.alloc(bytes(data))
            game = emu.call(0x005D6A00, ecx=pointer)
            ours, output = cpp_memory(checker, "resetmatrix", bytes(data))
            actual = emu.read(pointer, len(data))
            assert actual == output, next(
                ((i, actual[i], output[i]) for i in range(len(data)) if actual[i] != output[i]),
                None)
            assert (game.eax - pointer, game.stack_cleanup) == (ours, 0)
            counted("resetConnectionMatrix")

        for _ in range(CASES):
            emu.reset_data()
            data = rng.randbytes(0x500)
            row = rng.randrange(10)
            column = rng.randrange(10)
            pointer = emu.alloc(data)
            game = emu.call(0x005D69C5, [0, column], eax=row, ecx=pointer)
            ours, _ = cpp_memory(checker, "cell", data, row, column)
            normalized = 0xFFFFFFFF if game.eax == 0 else game.eax - pointer
            if ours == 0xFFFFFFFF:
                ours = 0
                normalized = 0 if game.eax == 0 else normalized
            assert (normalized, game.stack_cleanup) == (ours, 8)
            counted("matrixCell")

        for _ in range(CASES):
            emu.reset_data()
            data = bytearray(rng.randbytes(0x510))
            row = rng.randrange(10)
            pointer = emu.alloc(bytes(data))
            game = emu.call(0x005D6A70, [row], ecx=pointer)
            ours, output = cpp_memory(checker, "rowstate", bytes(data), row)
            assert (game.eax & 0xFF, game.stack_cleanup, emu.read(pointer, len(data))) == (
                ours, 4, output)
            counted("rowHasConnectedPeer")

        for _ in range(CASES):
            emu.reset_data()
            data = rng.randbytes(0x200)
            first = rng.randrange(10)
            second = rng.randrange(10)
            pointer = emu.alloc(data)
            game = emu.call(0x005D6C00, [first, second], ecx=pointer)
            ours, _ = cpp_memory(checker, "matrixvalue", data, first, second)
            assert (game.eax, game.stack_cleanup) == (ours, 8)
            counted("connectionValue")

        for address, active_offset, stopped_offset, start_offset, name in (
            (0x005D6E50, 0x0C, 0x270, 0x10, "peerTimeout"),
            (0x005D7250, 0x10, 0x399, 0x1C, "sessionTimeout"),
        ):
            for _ in range(CASES):
                emu.reset_data()
                data = bytearray(rng.randbytes(stopped_offset + 8))
                active = rng.randrange(2)
                stopped = rng.randrange(2)
                started = rng.randrange(0x100000000)
                now = rng.randrange(0x100000000)
                data[active_offset] = active
                data[stopped_offset] = stopped
                struct.pack_into("<I", data, start_offset, started)
                pointer = emu.alloc(bytes(data))
                emu.bind_function(0x009C9790, lambda _emu, value=now: value)
                game = emu.call(address, ecx=pointer)
                ours = int(checker.call(
                    f"timeout . {active} {stopped} {started} {now} 5000").split()[0])
                assert (game.eax, game.stack_cleanup) == (ours, 0)
                counted(name)

        for _ in range(CASES):
            emu.reset_data()
            data = bytearray(rng.randbytes(0x250))
            index = rng.randrange(128)
            sample = rng.randrange(0x100000000)
            struct.pack_into("<I", data, 0x240, index)
            pointer = emu.alloc(bytes(data))
            game = emu.call(0x005D6EA0, [sample], ecx=pointer)
            ours, output = cpp_memory(checker, "latency", bytes(data), sample)
            assert (game.eax, game.stack_cleanup, emu.read(pointer, len(data))) == (ours, 4, output)
            counted("addLatencySample")

        for address, operation in ((0x005D6FA0, "rawappend"), (0x005D70A0, "framedappend")):
            for _ in range(CASES):
                emu.reset_data()
                emu.write(0x00BBA500, struct.pack("<I", memmove_stub))
                old_size = rng.randrange(1, 300)
                old = rng.randbytes(old_size)
                read = rng.randrange(old_size + 1) if operation == "rawappend" else 0
                write = rng.randrange(read, old_size + 1)
                incoming = rng.randbytes(rng.randrange(256))
                capacity = rng.randrange(write, old_size + 1)
                old_pointer = emu.alloc(old)
                source = emu.alloc(incoming)
                replacement = emu.alloc(b"\xCC" * (write + len(incoming) + 2))
                queue = emu.alloc(struct.pack("<IIII", old_pointer, capacity, read, write))
                allocations: list[int] = []
                frees: list[int] = []

                def allocate(machine: Emulator) -> int:
                    allocations.append(machine.stack_arg(0))
                    return replacement

                def release(machine: Emulator) -> int:
                    frees.append(machine.stack_arg(0))
                    return 0

                emu.bind_function(0x00416840, allocate)
                emu.bind_function(0x004166B0, release)
                game = emu.call(address, [source, len(incoming)], ecx=queue)
                cpp = checker.call(
                    f"{operation} {old.hex()} {capacity} {read} {write} "
                    f"{incoming.hex() or '.'}").split()
                ours_capacity, ours_read, ours_write = map(int, cpp[:3])
                ours_data = bytes.fromhex(cpp[3])
                game_queue = emu.read(queue, 16)
                game_data_pointer, game_capacity, game_read, game_write = struct.unpack(
                    "<IIII", game_queue)
                game_tuple = (game.stack_cleanup, game_capacity, game_read, game_write,
                              emu.read(game_data_pointer, game_write))
                ours_tuple = (8, ours_capacity, ours_read, ours_write, ours_data)
                assert game_tuple == ours_tuple, (operation, game_tuple, ours_tuple)
                assert game.eax == game_data_pointer + (
                    game_write - len(incoming))
                needed = ((write - read if operation == "rawappend" else write)
                          + len(incoming) + (0 if operation == "rawappend"
                                             else (1 if len(incoming) < 0x80 else 2)))
                reallocated = capacity < needed
                assert allocations == ([needed] if reallocated else [])
                assert frees == ([old_pointer] if reallocated else [])
                counted(operation)

        for _ in range(CASES):
            emu.reset_data()
            count = rng.randrange(16)
            data = bytearray(rng.randbytes(10 + count * 2 + 8))
            struct.pack_into("<H", data, 8, count << 12)
            pointer = emu.alloc(bytes(data))
            game = emu.call(0x005D72A0, [pointer])
            ours, _ = cpp_memory(checker, "header", bytes(data))
            assert (game.eax, game.stack_cleanup) == (ours, 0)
            counted("packetHeaderSize")

        for _ in range(CASES):
            emu.reset_data()
            payload = rng.randbytes(rng.randrange(256))
            prefix = (bytes([len(payload)]) if len(payload) < 0x80
                      else bytes([0x80 | (len(payload) >> 8), len(payload) & 0xFF]))
            before = rng.randbytes(rng.randrange(8))
            after = rng.randbytes(rng.randrange(8))
            data = before + prefix + payload + after
            read = len(before)
            write = len(before) + len(prefix) + len(payload)
            capacity = rng.randrange(300)
            buffer_pointer = emu.alloc(data)
            queue = emu.alloc(struct.pack("<IIII", buffer_pointer, len(data), read, write))
            output_pointer = emu.alloc(b"\xA5" * max(capacity, 1))
            game = emu.call(0x005D7020, [output_pointer, capacity], ecx=queue)
            cpp = checker.call(
                f"framedpop {data.hex()} {read} {write} {capacity}").split()
            ours, ours_read = int(cpp[0]), int(cpp[1])
            ours_output = bytes.fromhex(cpp[2])
            game_queue = emu.read(queue, 16)
            game_read = u32(game_queue, 8)
            assert (game.eax, game.stack_cleanup, game_read,
                    emu.read(output_pointer, max(capacity, 1))) == (
                        ours, 8, ours_read, ours_output)
            counted("popFramed")

        for address, operation in ((0x005D7850, "bucketreset"), (0x005D7880, "bucketadd")):
            for _ in range(CASES):
                emu.reset_data()
                count = rng.randrange(1, 33)
                interval = rng.randrange(1, 5000)
                epoch = rng.randrange(0x100000000)
                now = (epoch + rng.randrange(interval * count * 2 + 1)) & 0xFFFFFFFF
                value_bits = rng.randrange(0x00800000, 0x7F000000)
                values = b"".join(struct.pack("<f", rng.uniform(-1000.0, 1000.0))
                                  for _ in range(count))
                values_pointer = emu.alloc(values)
                owner = emu.alloc(struct.pack("<IIII", values_pointer, count, interval, epoch))
                emu.bind_function(0x009C9790, lambda _emu, current=now: current)
                if address == 0x005D7850:
                    game = emu.call(address, ecx=owner)
                else:
                    game = emu.call(address, [value_bits], ecx=owner)
                cpp = checker.call(
                    f"{operation} {values.hex()} {count} {interval} {epoch} {now} {value_bits}"
                ).split()
                ours_epoch, ours_values = int(cpp[0]), bytes.fromhex(cpp[1])
                assert (game.stack_cleanup, u32(emu.read(owner, 16), 12),
                        emu.read(values_pointer, len(values))) == (
                            0 if address == 0x005D7850 else 4, ours_epoch, ours_values)
                counted(operation)

        for _ in range(CASES):
            emu.reset_data()
            interval = rng.randrange(1, 5000)
            duration = rng.randrange(interval, interval * 64)
            now = rng.randrange(0x100000000)
            count = max(duration // interval, 1)
            storage = emu.alloc(b"\x7B" * (count * 4))
            owner = emu.alloc(rng.randbytes(16))
            emu.bind_function(0x009C9790, lambda _emu, current=now: current)
            emu.bind_function(0x00416840, lambda machine, value=storage: value)
            game = emu.call(0x005D77E0, [interval, duration], ecx=owner)
            cpp = checker.call(f"bucketinit . {interval} {duration} {now}").split()
            assert (game.eax, game.stack_cleanup, emu.read(owner + 4, 12),
                    emu.read(storage, count * 4)) == (
                        owner, 8, struct.pack("<III", int(cpp[0]), int(cpp[1]), int(cpp[2])),
                        bytes.fromhex(cpp[3]))
            counted("initializeTimeBuckets")

        for _ in range(CASES):
            emu.reset_data()
            count = rng.randrange(1, 65)
            values = b"".join(struct.pack("<f", rng.uniform(-1000.0, 1000.0))
                              for _ in range(count))
            values_pointer = emu.alloc(values)
            owner = emu.alloc(struct.pack("<IIII", values_pointer, count, 1, 0))
            game, game_bits = emu.call_float32(0x005D78F0, ecx=owner)
            ours = int(checker.call(f"sum {values.hex()}").split()[0])
            assert (game_bits, game.stack_cleanup) == (ours, 0)
            counted("sumTimeBuckets")

        for _ in range(CASES):
            emu.reset_data()
            data = rng.randbytes(0x1720)
            pointer = emu.alloc(data)
            game = emu.call(0x005D7500, ecx=pointer)
            ours, _ = cpp_memory(checker, "active", data)
            assert (game.eax, game.stack_cleanup) == (ours, 0)
            counted("activePeerPendingTotal")

        for _ in range(CASES):
            emu.reset_data()
            destination = rng.randbytes(60)
            record = rng.randbytes(56)
            tag = rng.randrange(0x100000000)
            pointer = emu.alloc(destination)
            source = emu.alloc(record)
            game = emu.call(0x005D75E0, [tag, source], ecx=pointer)
            ours, output = cpp_memory(checker, "record", destination, tag, record.hex())
            assert (game.eax - pointer, game.stack_cleanup, emu.read(pointer, 60)) == (
                ours, 8, output)
            counted("copyTransportRecord")

        for _ in range(CASES):
            emu.reset_data()
            family = rng.randrange(-3, 5)
            index = rng.randrange(-20, 100)
            side = rng.randrange(-3, 5)
            game = emu.call(0x005D7640, [family, index, side])
            ours = int(checker.call(f"code . {family} {index} {side}").split()[0])
            assert (game.eax, game.stack_cleanup) == (ours, 0)
            counted("makeConnectionCode")

        for _ in range(CASES):
            emu.reset_data()
            data = rng.randbytes(16)
            value = rng.randrange(0x100000000)
            pointer = emu.alloc(data)
            game = emu.call(0x005D77D0, [value], ecx=pointer)
            ours, output = cpp_memory(checker, "opaque", data, value)
            expected = bytearray(data)
            struct.pack_into("<I", expected, 0, value)
            assert (game.eax, game.stack_cleanup, emu.read(pointer, 16), output) == (
                game.eax, 4, bytes(expected), bytes(expected))
            del ours
            counted("setOpaqueValue")

        def htonl_stub(machine: Emulator) -> int:
            value = machine.stack_arg(0)
            return struct.unpack("<I", struct.pack(">I", value))[0]

        emu.bind_function(0x00B8DBFC, htonl_stub, stack_cleanup=4)
        for _ in range(CASES):
            emu.reset_data()
            byte_count = rng.randrange(-8, 260)
            words = max((byte_count + (3 if byte_count < 0 else 0)) // 4, 0)
            data = rng.randbytes(max(words * 4, 4))
            pointer = emu.alloc(data)
            game = emu.call(0x005D7AE0, eax=byte_count, ecx=pointer)
            ours, output = cpp_memory(checker, "scramble", data, byte_count)
            assert (game.stack_cleanup, emu.read(pointer, len(data))) == (0, output)
            del ours
            counted("scramblePacketWords")

        for address, operation, cleanup in (
            (0x005D7EA0, "endpointclear", 4),
            (0x005D7FC0, "endpointupdate", 8),
        ):
            for _ in range(CASES):
                emu.reset_data()
                table = bytearray(rng.randbytes(72))
                slot = rng.randrange(10)
                if slot < 6:
                    struct.pack_into("<I", table, slot * 12, 0)
                data = bytes(0x40E00) + bytes(table)
                pointer = emu.alloc(data)
                if operation == "endpointclear":
                    game = emu.call(address, [slot], ecx=pointer)
                    ours, output = cpp_memory(checker, operation, bytes(table), slot)
                else:
                    endpoint = rng.randbytes(8)
                    source = emu.alloc(endpoint)
                    game = emu.call(address, [slot, source], ecx=pointer)
                    address_value, port, padding = struct.unpack_from("<IHH", endpoint)
                    ours, output = cpp_memory(
                        checker, operation, bytes(table), slot, 0, address_value, port, padding)
                assert (game.stack_cleanup, emu.read(pointer + 0x40E00, 72)) == (cleanup, output)
                del ours
                counted(operation)

        for address, operation, cleanup in (
            (0x005D7F20, "endpointset", 12),
            (0x005D7F70, "endpointtake", 8),
        ):
            for _ in range(CASES):
                emu.reset_data()
                table = bytearray(rng.randbytes(72))
                slot = rng.randrange(10)
                if address == 0x005D7F20 and slot < 6:
                    struct.pack_into("<I", table, slot * 12, 0)
                data = bytes(0x40E00) + bytes(table)
                pointer = emu.alloc(data)
                if operation == "endpointset":
                    handle = rng.randrange(0x100000000)
                    endpoint = rng.randbytes(8)
                    source = emu.alloc(endpoint)
                    game = emu.call(address, [handle, slot, source], ecx=pointer)
                    address_value, port, padding = struct.unpack("<IHH", endpoint)
                    _, output = cpp_memory(
                        checker, operation, bytes(table), slot, handle,
                        address_value, port, padding)
                    assert (game.stack_cleanup, emu.read(pointer + 0x40E00, 72)) == (
                        cleanup, output)
                else:
                    output_pointer = emu.alloc(b"\xA5" * 8)
                    game = emu.call(address, [slot, output_pointer], ecx=pointer)
                    cpp = checker.call(f"{operation} {bytes(table).hex()} {slot}").split()
                    ours_handle = int(cpp[0])
                    endpoint = struct.pack("<IHH", int(cpp[1]), int(cpp[2]), int(cpp[3]))
                    output = bytes.fromhex(cpp[4])
                    assert (game.eax, game.stack_cleanup, emu.read(output_pointer, 8),
                            emu.read(pointer + 0x40E00, 72)) == (
                                ours_handle, cleanup, endpoint, output)
                counted(operation)

        for _ in range(CASES):
            emu.reset_data()
            data = rng.randbytes(rng.randrange(1, 512))
            data_pointer = emu.alloc(data)
            span = emu.alloc(struct.pack("<II", data_pointer, data_pointer + len(data)))
            game = emu.call(0x005D88E0, [span])
            ours, _ = cpp_memory(checker, "countnonzero", data)
            assert (game.eax, game.stack_cleanup) == (ours, 0)
            counted("countNonzeroBytes")

        for _ in range(CASES):
            emu.reset_data()
            count = rng.randrange(1, 65)
            first_values = [rng.randrange(-100, 101) for _ in range(count)]
            second_values = [rng.randrange(-100, 101) for _ in range(count)]
            first = struct.pack(f"<{count}i", *first_values)
            second = struct.pack(f"<{count}i", *second_values)
            first_pointer = emu.alloc(first)
            second_pointer = emu.alloc(second)
            first_span = emu.alloc(struct.pack("<II", first_pointer, first_pointer + len(first)))
            second_span = emu.alloc(struct.pack("<II", second_pointer, second_pointer + len(second)))
            game = emu.call(0x005D8910, [first_span, second_span])
            ours, _ = cpp_memory(checker, "minmag", first, second.hex() or ".")
            assert (game.eax, game.stack_cleanup) == (ours, 0)
            counted("minimumCombinedMagnitude")

        print("net transport oracle:")
        for name, count in totals.items():
            print(f"  {name}: {count}/{count}, 0 mismatches")
        print(f"total: {sum(totals.values())}, 0 mismatches")
    finally:
        checker.close()


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("executable", type=Path)
    args = parser.parse_args()
    verify(args.executable)


if __name__ == "__main__":
    main()
