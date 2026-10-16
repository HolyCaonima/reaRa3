"""Disassemble one ra3_1.0.game function and derive its hook parameters.

Reads the executable read-only. For a hook target it reports the first 16 bytes
(the expected-bytes gate), the shortest whole-instruction prefix of at least 5
bytes (the stolen length) and whether that prefix is position independent, and
the return form (ret / ret N), which separates cdecl from stdcall/thiscall.

    python disasm.py 0x00417DB0 [--exe PATH] [--max-bytes N] [--hook-only]
"""

from __future__ import annotations

import argparse
import struct
from pathlib import Path

import capstone
from capstone import x86

DEFAULT_EXE = Path(r"D:\Game\Ra3\Game\Data\ra3_1.0.game")
JUMP_SIZE = 5


class Image:
    def __init__(self, path: Path) -> None:
        self.data = path.read_bytes()
        pe = struct.unpack_from("<I", self.data, 0x3C)[0]
        if self.data[pe:pe + 4] != b"PE\0\0":
            raise ValueError(f"{path}: not a PE file")
        sections = struct.unpack_from("<H", self.data, pe + 6)[0]
        optional_size = struct.unpack_from("<H", self.data, pe + 20)[0]
        self.base = struct.unpack_from("<I", self.data, pe + 24 + 28)[0]
        table = pe + 24 + optional_size
        self.sections = []
        for i in range(sections):
            entry = table + i * 40
            name = self.data[entry:entry + 8].rstrip(b"\0").decode("latin-1")
            vsize, rva, raw_size, raw = struct.unpack_from("<IIII", self.data, entry + 8)
            self.sections.append((name, rva, max(vsize, raw_size), raw, raw_size))

    def section_of(self, va: int) -> tuple[str, int]:
        rva = va - self.base
        for name, start, size, raw, raw_size in self.sections:
            if start <= rva < start + size:
                if rva - start >= raw_size:
                    raise ValueError(f"0x{va:08X} is in uninitialised data of {name}")
                return name, raw + (rva - start)
        raise ValueError(f"0x{va:08X} is outside every section")

    def read(self, va: int, size: int) -> bytes:
        _, offset = self.section_of(va)
        return self.data[offset:offset + size]


def position_dependent(insn) -> bool:
    if insn.group(capstone.CS_GRP_JUMP) or insn.group(capstone.CS_GRP_CALL):
        return any(op.type == x86.X86_OP_IMM for op in insn.operands)
    if insn.group(capstone.CS_GRP_RET) or insn.group(capstone.CS_GRP_INT):
        return True
    return False


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("address", type=lambda s: int(s, 0))
    parser.add_argument("--exe", type=Path, default=DEFAULT_EXE)
    parser.add_argument("--max-bytes", type=int, default=0x400)
    parser.add_argument("--hook-only", action="store_true")
    args = parser.parse_args()

    image = Image(args.exe)
    section, offset = image.section_of(args.address)
    code = image.read(args.address, args.max_bytes)
    md = capstone.Cs(capstone.CS_ARCH_X86, capstone.CS_MODE_32)
    md.detail = True

    print(f"function 0x{args.address:08X}  section={section}  file_offset=0x{offset:X}  "
          f"rva=0x{args.address - image.base:08X}")
    print("expected16 " + " ".join(f"{b:02X}" for b in code[:16]))

    stolen = 0
    movable = True
    returns: set[str] = set()
    lines = []
    for insn in md.disasm(code, args.address):
        if stolen < JUMP_SIZE:
            stolen += insn.size
            movable = movable and not position_dependent(insn)
        lines.append(f"  {insn.address:08X}  {insn.bytes.hex(' '):<24} {insn.mnemonic} {insn.op_str}")
        if insn.group(capstone.CS_GRP_RET):
            returns.add(f"{insn.mnemonic} {insn.op_str}".strip())
            if args.hook_only:
                break
        if insn.mnemonic == "int3" and lines and stolen >= JUMP_SIZE:
            break
    if stolen < JUMP_SIZE:
        print("stolen     undecodable prefix")
    else:
        print(f"stolen     {stolen} bytes, {'position independent' if movable else 'POSITION DEPENDENT'}")
    print("returns    " + (", ".join(sorted(returns)) or "none seen"))
    if not args.hook_only:
        print("\n".join(lines))


if __name__ == "__main__":
    main()
