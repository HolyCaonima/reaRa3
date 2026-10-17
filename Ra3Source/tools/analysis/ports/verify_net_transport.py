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
