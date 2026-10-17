"""Run one ra3_1.0.game function in a CPU emulator.

Differential oracle for C++ ports: the function's own shipped bytes are executed
on caller-chosen inputs, outside the game process. Only the on-disk images of
.text, .rdata and .data are mapped; SecuROM sections are never mapped, so any
access to them (or to anything else unmapped) stops the run with an error.
.data holds load-time values only: globals initialised at runtime read as their
on-disk contents, so functions depending on such state are not comparable.

Imported functions are not mapped. A caller may bind an IAT slot to a Python
stub (cdecl, sees the emulator and returns EAX) with ``bind_import``.

    from emulate import Emulator
    emu = Emulator()
    a = emu.alloc(b"Data\\Foo.big\\0")
    b = emu.alloc(b"data\\foo.BIG\\0")
    print(emu.call(0x009F3650, [a, b]).eax)

SecuROM-protected functions (ledger ``securom`` != clean) are not plaintext on
disk: an entry-lost function's entry is a ``jmp [slot]`` whose slot points at a VM
stub, an inner-patched one has single instructions replaced the same way, and the
real target is only written into the slot the first time the site executes in the
running game. ``securom_sites.py`` captures those resolved slots and the sections
they live in from a read-only runtime dump (``ra3hook`` dump mode). Passing such a
dump snapshot as ``capture`` overlays its ``.securom`` image, which carries both
the resolved slot table and the decrypted bodies, so a protected function runs
from the same resolved code the process would, SecuROM's own sections aside.

    emu = Emulator(capture="work/runtime_dump/run35/snapshot_3_240s")
    thunks = entry_targets("Ra3Source/docs/analysis/ra3_1.0/securom_sites.tsv")
    print(emu.call(thunks[0x00401AD0]).eax)  # emulate the real body, not the thunk
"""

from __future__ import annotations

import csv
import struct
from dataclasses import dataclass
from pathlib import Path
from typing import Callable, Optional, Sequence

import unicorn
from unicorn import x86_const as r

from disasm import DEFAULT_EXE, Image

MAPPED_SECTIONS = (".text", ".rdata", ".data")
SECUROM_SECTION = ".securom"
SECUROM_OVERLAY = "_securom.bin"
PAGE = 0x1000
STACK_BASE = 0x7F000000
STACK_SIZE = 0x100000
HEAP_BASE = 0x60000000
HEAP_SIZE = 0x1000000
STUB_BASE = 0x7FE00000
RETURN_SENTINEL = STUB_BASE + 0xFF0
FLOAT_RETURN_STUB = STUB_BASE + 0xFE0
FPU_POP_STUB = STUB_BASE + 0xFD0
FLOAT_RESULT = HEAP_BASE + HEAP_SIZE - 0x10
FPU_REGISTERS = (
    r.UC_X86_REG_FP0, r.UC_X86_REG_FP1, r.UC_X86_REG_FP2, r.UC_X86_REG_FP3,
    r.UC_X86_REG_FP4, r.UC_X86_REG_FP5, r.UC_X86_REG_FP6, r.UC_X86_REG_FP7,
)


@dataclass(frozen=True)
class CallResult:
    eax: int
    edx: int
    stack_cleanup: int


class EmulationError(RuntimeError):
    pass


def _align(value: int, alignment: int = PAGE) -> int:
    return (value + alignment - 1) & ~(alignment - 1)


def read_securom_sites(path: Path) -> list[dict]:
    """Rows of ``securom_sites.py`` output with the address columns parsed as ints."""
    rows = []
    with Path(path).open(encoding="utf-8") as handle:
        for row in csv.DictReader(handle, delimiter="\t"):
            for column in ("site", "slot", "file_target", "runtime_target", "continuation",
                           "function"):
                row[column] = int(row[column], 16) if row.get(column) else None
            rows.append(row)
    return rows


def entry_targets(path: Path) -> dict[int, int]:
    """Entry-site thunk VA -> resolved body VA, for functions whose entry was lost.

    These are the real functions to emulate and port: the ledger address is a thunk
    that jumps through a resolved slot at the returned body.
    """
    return {row["site"]: row["runtime_target"] for row in read_securom_sites(path)
            if row["runtime_target"] is not None and row["resolution"] == "body"
            and row["function"] == row["site"]}


class Emulator:
    def __init__(self, exe: Path = DEFAULT_EXE, capture: Optional[Path] = None) -> None:
        self.image = Image(exe)
        self.uc = unicorn.Uc(unicorn.UC_ARCH_X86, unicorn.UC_MODE_32)
        for name, rva, size, raw, raw_size in self.image.sections:
            if name.split("\0")[0] not in MAPPED_SECTIONS:
                continue
            start = self.image.base + rva
            self.uc.mem_map(start, _align(size), unicorn.UC_PROT_READ | unicorn.UC_PROT_WRITE
                            | unicorn.UC_PROT_EXEC)
            self.uc.mem_write(start, self.image.data[raw:raw + raw_size])
        if capture is not None:
            self._map_securom_overlay(Path(capture))
        self._initial = {
            self.image.base + rva: self.image.data[raw:raw + raw_size]
            for name, rva, _, raw, raw_size in self.image.sections
            if name.split("\0")[0] == ".data"
        }
        self.uc.mem_map(STACK_BASE, STACK_SIZE)
        self.uc.mem_map(HEAP_BASE, HEAP_SIZE)
        self.uc.mem_map(STUB_BASE, PAGE, unicorn.UC_PROT_READ | unicorn.UC_PROT_EXEC)
        self.uc.mem_write(STUB_BASE, b"\xC3" * PAGE)
        relative = RETURN_SENTINEL - (FLOAT_RETURN_STUB + 11)
        self.uc.mem_write(
            FLOAT_RETURN_STUB,
            b"\xD9\x1D" + struct.pack("<I", FLOAT_RESULT)
            + b"\xE9" + struct.pack("<i", relative))
        pop_relative = RETURN_SENTINEL - (FPU_POP_STUB + 7)
        self.uc.mem_write(
            FPU_POP_STUB,
            b"\xDD\xD8\xE9" + struct.pack("<i", pop_relative))
        self._heap = HEAP_BASE
        self._stubs: dict[int, tuple[Callable[["Emulator"], int], int]] = {}
        self._next_stub = STUB_BASE
        self._function_stubs: dict[int, tuple[Callable[["Emulator"], int], int]] = {}
        self._fault: Optional[str] = None
        self.uc.hook_add(unicorn.UC_HOOK_MEM_UNMAPPED, self._on_unmapped)
        self.uc.hook_add(unicorn.UC_HOOK_CODE, self._on_stub, begin=STUB_BASE,
                         end=STUB_BASE + PAGE - 1)
        self.uc.hook_add(unicorn.UC_HOOK_CODE, self._on_function_stub)

    def _map_securom_overlay(self, capture: Path) -> None:
        """Map .securom from a runtime dump: resolved slot table and decrypted bodies.

        The shipped .securom is encrypted and holds VM stubs, so it is never mapped
        from the file. The dump's image of it is what the process ran: slots resolved
        to real targets, and the relocated/decrypted bodies those slots point at.
        """
        overlay = capture / SECUROM_OVERLAY
        if not overlay.is_file():
            raise EmulationError(f"capture has no {SECUROM_OVERLAY}: {capture}")
        data = overlay.read_bytes()
        start = next(self.image.base + rva for name, rva, *_ in self.image.sections
                     if name.split("\0")[0] == SECUROM_SECTION)
        self.uc.mem_map(start, _align(len(data)),
                        unicorn.UC_PROT_READ | unicorn.UC_PROT_EXEC)
        self.uc.mem_write(start, data)

    def _on_unmapped(self, uc, access, address, size, value, data) -> bool:
        eip = uc.reg_read(r.UC_X86_REG_EIP)
        self._fault = f"unmapped access 0x{address:08X} (size {size}) at eip 0x{eip:08X}"
        return False

    def _on_stub(self, uc, address, size, data) -> None:
        if address == RETURN_SENTINEL:
            uc.emu_stop()
            return
        if (
            FLOAT_RETURN_STUB <= address < FLOAT_RETURN_STUB + 11
            or FPU_POP_STUB <= address < FPU_POP_STUB + 7
        ):
            return
        binding = self._stubs.get(address)
        if binding is None:
            self._fault = f"call into unbound stub 0x{address:08X}"
            uc.emu_stop()
            return
        self._return_from_stub(*binding)

    def _on_function_stub(self, uc, address, size, data) -> None:
        binding = self._function_stubs.get(address)
        if binding is not None:
            self._return_from_stub(*binding)

    def _return_from_stub(
        self, stub: Callable[["Emulator"], int], stack_cleanup: int
    ) -> None:
        esp = self.uc.reg_read(r.UC_X86_REG_ESP)
        return_address = struct.unpack("<I", self.uc.mem_read(esp, 4))[0]
        self.uc.reg_write(r.UC_X86_REG_EAX, stub(self) & 0xFFFFFFFF)
        self.uc.reg_write(r.UC_X86_REG_ESP, esp + 4 + stack_cleanup)
        self.uc.reg_write(r.UC_X86_REG_EIP, return_address)

    def bind_stub(
        self, stub: Callable[["Emulator"], int], stack_cleanup: int = 0
    ) -> int:
        address = self._next_stub
        self._next_stub += 0x10
        self._stubs[address] = (stub, stack_cleanup)
        return address

    def bind_import(
        self,
        iat_slot: int,
        stub: Callable[["Emulator"], int],
        stack_cleanup: int = 0,
    ) -> None:
        address = self.bind_stub(stub, stack_cleanup)
        self.uc.mem_write(iat_slot, struct.pack("<I", address))

    def bind_function(
        self,
        address: int,
        stub: Callable[["Emulator"], int],
        stack_cleanup: int = 0,
    ) -> None:
        self._function_stubs[address] = (stub, stack_cleanup)

    def stack_arg(self, index: int) -> int:
        """Argument ``index`` of a cdecl call as seen from inside an import stub."""
        esp = self.uc.reg_read(r.UC_X86_REG_ESP)
        return struct.unpack("<I", self.uc.mem_read(esp + 4 + 4 * index, 4))[0]

    def register(self, register: int) -> int:
        return self.uc.reg_read(register)

    def reset_data(self) -> None:
        for start, image in self._initial.items():
            self.uc.mem_write(start, image)
        self._heap = HEAP_BASE

    def alloc(self, data: bytes, alignment: int = 16) -> int:
        address = _align(self._heap, alignment)
        if address + max(len(data), 1) > HEAP_BASE + HEAP_SIZE:
            raise EmulationError("emulator heap exhausted")
        self.uc.mem_write(address, data)
        self._heap = address + max(len(data), 1)
        return address

    def read(self, address: int, size: int) -> bytes:
        return bytes(self.uc.mem_read(address, size))

    def write(self, address: int, data: bytes) -> None:
        self.uc.mem_write(address, data)

    def call(self, address: int, stack_args: Sequence[int] = (), *, eax: int = 0,
             ecx: int = 0, edx: int = 0,
             max_instructions: int = 5_000_000,
             _return_address: int = RETURN_SENTINEL) -> CallResult:
        """Call ``address`` with ``stack_args`` pushed right to left.

        ``stack_cleanup`` is the number of argument bytes the callee popped:
        0 for cdecl, 4 * stack arguments for stdcall/thiscall.
        """
        top = STACK_BASE + STACK_SIZE - 0x100
        frame = struct.pack(f"<I{len(stack_args)}I", _return_address,
                            *[a & 0xFFFFFFFF for a in stack_args])
        esp = top - len(frame)
        self.uc.mem_write(esp, frame)
        for reg, value in ((r.UC_X86_REG_ESP, esp), (r.UC_X86_REG_EBP, 0),
                           (r.UC_X86_REG_EAX, eax), (r.UC_X86_REG_EBX, 0),
                           (r.UC_X86_REG_ECX, ecx), (r.UC_X86_REG_EDX, edx),
                           (r.UC_X86_REG_ESI, 0), (r.UC_X86_REG_EDI, 0)):
            self.uc.reg_write(reg, value & 0xFFFFFFFF)
        self._fault = None
        try:
            self.uc.emu_start(address, RETURN_SENTINEL, count=max_instructions)
        except unicorn.UcError as error:
            raise EmulationError(self._fault or f"0x{address:08X}: {error}") from None
        if self._fault:
            raise EmulationError(self._fault)
        eip = self.uc.reg_read(r.UC_X86_REG_EIP)
        if eip != RETURN_SENTINEL:
            raise EmulationError(f"0x{address:08X}: no return within {max_instructions} "
                                 f"instructions (eip 0x{eip:08X})")
        cleanup = self.uc.reg_read(r.UC_X86_REG_ESP) - (esp + 4)
        return CallResult(self.uc.reg_read(r.UC_X86_REG_EAX), self.uc.reg_read(r.UC_X86_REG_EDX),
                          cleanup)

    def call_float32(self, address: int, stack_args: Sequence[int] = (), *,
                     ecx: int = 0, edx: int = 0, fpu_control_word: int = 0x037F,
                     max_instructions: int = 5_000_000) -> tuple[CallResult, int]:
        """Call an x87-float-returning function and capture the caller-rounded bits."""
        self.uc.reg_write(r.UC_X86_REG_FPCW, fpu_control_word)
        self.uc.mem_write(FLOAT_RESULT, b"\xA5\xA5\xA5\xA5")
        result = self.call(
            address, stack_args, ecx=ecx, edx=edx,
            max_instructions=max_instructions, _return_address=FLOAT_RETURN_STUB)
        bits = struct.unpack("<I", self.uc.mem_read(FLOAT_RESULT, 4))[0]
        return result, bits

    def call_float80(self, address: int, stack_args: Sequence[int] = (), *,
                     ecx: int = 0, edx: int = 0, fpu_control_word: int = 0x037F,
                     max_instructions: int = 5_000_000) -> tuple[CallResult, bytes]:
        """Call an x87-returning function and capture and pop its full 80-bit ST0."""
        self.uc.reg_write(r.UC_X86_REG_FPCW, fpu_control_word)
        result = self.call(
            address, stack_args, ecx=ecx, edx=edx,
            max_instructions=max_instructions)
        top = (self.uc.reg_read(r.UC_X86_REG_FPSW) >> 11) & 7
        mantissa, exponent = self.uc.reg_read(FPU_REGISTERS[top])
        value = struct.pack("<QH", mantissa, exponent)
        self.uc.emu_start(FPU_POP_STUB, RETURN_SENTINEL, count=3)
        return result, value
