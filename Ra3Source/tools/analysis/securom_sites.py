#!/usr/bin/env python3
"""Merges runtime captures of SecuROM jump sites into securom_sites.tsv.

ra3hook.dll in dump mode (`dump=DIR`, docs/design/injection.md) lists every
`jmp [slot]` site in .text whose slot lies in .securom and logs each slot value
change while the game runs. A slot initially points at a VM stub; the first time
its site executes, SecuROM writes the real target into the slot. Every target is
plaintext already present in the shipped image (.text or .securom; runtime .text
equals the file), so a resolved slot recovers the site completely:

  body   the slot points at a function body (the site was a whole function entry)
  cave   the slot points at a short sequence that runs the replaced instruction(s)
         and transfers back behind the site (`continuation`)

A slot still holding its file value was never executed in the captured sessions
or stays virtualised; captures cannot distinguish the two (`unresolved`).

Each resolved target is traced in Unicorn with the image's .text/.rdata/.data and
.securom mapped from the file and the resolved slots applied; SecuROM's own code
sections are never mapped.

    python securom_sites.py work/runtime_dump/run35 [more runs ...] \
        --out Ra3Source/docs/analysis/ra3_1.0/securom_sites.tsv
"""
from __future__ import annotations

import argparse
import bisect
import collections
import csv
import re
import struct
import sys
from pathlib import Path
from typing import Callable

import capstone
import unicorn
from unicorn import x86_const as r

from disasm import DEFAULT_EXE, Image

LEDGER = Path(__file__).resolve().parents[2] / "docs" / "analysis" / "ra3_1.0" / "function_ledger.tsv"
MAPPED = (".text", ".rdata", ".data", ".securom")
VM_ENTRY = 0x01132580
PAGE = 0x1000
STACK = 0x7F000000
SCRATCH = 0x60000000
SCRATCH_SIZE = 0x01000000
TRACE_LIMIT = 48
CONTINUATION_WINDOW = 16


def load_runs(runs: list[Path]):
    """site -> (slot, kind); slot -> {run: resolved value}; slot -> file value."""
    sites: dict[int, tuple[int, str]] = {}
    resolved: dict[int, dict[str, int]] = collections.defaultdict(dict)
    for run in runs:
        with (run / "sites.tsv").open(encoding="utf-8") as handle:
            for row in csv.DictReader(handle, delimiter="\t"):
                site = int(row["site"], 16)
                entry = (int(row["slot"], 16), row["kind"])
                if sites.setdefault(site, entry) != entry:
                    raise ValueError(f"{run}: site {site:08x} differs between runs")
        with (run / "slot_changes.tsv").open(encoding="utf-8") as handle:
            for row in csv.DictReader(handle, delimiter="\t"):
                resolved[int(row["slot"], 16)][run.name] = int(row["new"], 16)
    return sites, resolved


class Tracer:
    def __init__(self, image: Image, slot_values: dict[int, int]) -> None:
        self.image = image
        self.md = capstone.Cs(capstone.CS_ARCH_X86, capstone.CS_MODE_32)
        self.uc = unicorn.Uc(unicorn.UC_ARCH_X86, unicorn.UC_MODE_32)
        # Caves read constants from the PE header as well as from .securom.
        self.uc.mem_map(image.base, PAGE)
        self.uc.mem_write(image.base, image.data[:PAGE])
        self.sections = [("header", image.base, PAGE)]
        for name, rva, size, raw, raw_size in image.sections:
            if name in MAPPED:
                start = image.base + rva
                self.uc.mem_map(start, (size + PAGE - 1) & ~(PAGE - 1))
                self.uc.mem_write(start, image.data[raw:raw + raw_size])
                self.sections.append((name, start, size))
        for slot, value in slot_values.items():
            self.uc.mem_write(slot, struct.pack("<I", value))
        self.uc.mem_map(STACK, 0x100000)
        self.uc.mem_map(SCRATCH, SCRATCH_SIZE)
        self.uc.hook_add(unicorn.UC_HOOK_CODE, self._on_code)
        self.uc.hook_add(unicorn.UC_HOOK_MEM_UNMAPPED, self._on_unmapped)
        self._trace: list[int] = []
        self._stop: tuple[int, int] = (0, 0)
        self._scratch_pages: list[int] = []
        self.continuation = 0

    def _on_unmapped(self, uc, access, address, size, value, data) -> bool:
        """Data accesses through the arbitrary register values get zero pages."""
        if access == unicorn.UC_MEM_FETCH_UNMAPPED or len(self._scratch_pages) >= 256:
            return False
        for start in sorted({address & ~(PAGE - 1), (address + size - 1) & ~(PAGE - 1)}):
            try:
                uc.mem_map(start, PAGE)
            except unicorn.UcError:
                continue
            self._scratch_pages.append(start)
        return True

    def section_of(self, address: int) -> str:
        for name, start, size in self.sections:
            if start <= address < start + size:
                return name
        return ""

    def _on_code(self, uc, address, size, data) -> None:
        low, high = self._stop
        if low <= address < high:
            self.continuation = address
            uc.emu_stop()
            return
        self._trace.append(address)
        if len(self._trace) >= TRACE_LIMIT:
            uc.emu_stop()

    def trace(self, target: int, site: int) -> list[capstone.CsInsn]:
        """Instructions executed from `target` until control reaches just behind `site`."""
        self._trace = []
        self.continuation = 0
        self._stop = (site + 6, site + CONTINUATION_WINDOW)
        for page in self._scratch_pages:
            self.uc.mem_unmap(page, PAGE)
        self._scratch_pages = []
        self.uc.mem_write(STACK, bytes(0x100000))
        middle = SCRATCH + SCRATCH_SIZE // 2
        for register in (r.UC_X86_REG_EAX, r.UC_X86_REG_EBX, r.UC_X86_REG_ECX, r.UC_X86_REG_EDX,
                         r.UC_X86_REG_ESI, r.UC_X86_REG_EDI, r.UC_X86_REG_EBP):
            self.uc.reg_write(register, middle)
        self.uc.reg_write(r.UC_X86_REG_ESP, STACK + 0x80000)
        self.uc.reg_write(r.UC_X86_REG_EFLAGS, 0x202)
        try:
            self.uc.emu_start(target, 0xFFFFFFFF, count=TRACE_LIMIT + 1)
        except unicorn.UcError:
            pass
        executed = []
        for address in self._trace:
            code = bytes(self.uc.mem_read(address, 16))
            insn = next(self.md.disasm(code, address), None)
            if insn is not None:
                executed.append(insn)
        return executed


ABSOLUTE = re.compile(r"\[0x([0-9a-f]+)\]")


def text_of(insn: capstone.CsInsn, constants: Callable[[int], int | None]) -> str:
    """Instruction text; absolute operands in SecuROM-owned constants get their file value."""
    text = f"{insn.mnemonic} {insn.op_str}".strip()
    for match in ABSOLUTE.finditer(insn.op_str):
        value = constants(int(match.group(1), 16))
        if value is not None:
            text += f" {{[0x{match.group(1)}]=0x{value:08x}}}"
    return text


def replaced_instructions(executed: list[capstone.CsInsn],
                          constants: Callable[[int], int | None]) -> str:
    """Instructions of a cave before its transfer back (`jmp rel` or `push r32; pushfd`)."""
    kept = []
    for index, insn in enumerate(executed):
        if insn.mnemonic == "jmp" and insn.op_str.startswith("0x"):
            break
        following = executed[index + 1] if index + 1 < len(executed) else None
        if insn.mnemonic == "push" and insn.op_str in {"eax", "ebx", "ecx", "edx", "esi", "edi", "ebp"} \
                and following is not None and following.mnemonic == "pushfd":
            break
        kept.append(text_of(insn, constants))
    return "; ".join(kept)


def stub_shape(image: Image, md: capstone.Cs, target: int) -> str:
    try:
        code = image.read(target, 32)
    except ValueError:
        return "unreadable"
    insns = list(md.disasm(code, target))[:5]
    names = [insn.mnemonic for insn in insns]
    if names[:2] == ["push", "call"] and insns[1].op_str == f"0x{VM_ENTRY:x}":
        return "vm"
    if names[:4] == ["push", "push", "push", "pushfd"]:
        return "vm-indirect"
    return "native"


def load_ledger():
    starts, rows = [], {}
    with LEDGER.open(encoding="utf-8") as handle:
        for row in csv.DictReader(handle, delimiter="\t"):
            address = int(row["address"], 16)
            starts.append(address)
            rows[address] = (int(row["size"]), row["securom"])
    starts.sort()

    def containing(address: int):
        index = bisect.bisect_right(starts, address) - 1
        if index >= 0:
            start = starts[index]
            size, securom = rows[start]
            if address < start + max(size, 1):
                return start, securom
        return None, ""

    return containing


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("runs", nargs="+", type=Path, help="dump directories (ra3hook dump=DIR)")
    parser.add_argument("--out", type=Path, required=True)
    parser.add_argument("--exe", type=Path, default=DEFAULT_EXE)
    args = parser.parse_args()

    image = Image(args.exe)
    sites, resolved = load_runs(args.runs)
    final: dict[int, int] = {}
    conflicts = 0
    for slot, values in resolved.items():
        if len(set(values.values())) != 1:
            conflicts += 1
            print(f"slot {slot:08x} resolves differently: {values}", file=sys.stderr)
            continue
        final[slot] = next(iter(values.values()))
    tracer = Tracer(image, final)
    containing = load_ledger()
    md = capstone.Cs(capstone.CS_ARCH_X86, capstone.CS_MODE_32)

    def resolved_constant(address: int) -> int | None:
        """Runtime value of a .securom slot a cave reads indirectly, else None."""
        return final.get(address)

    counts = collections.Counter()
    args.out.parent.mkdir(parents=True, exist_ok=True)
    with args.out.open("w", encoding="utf-8", newline="") as handle:
        writer = csv.writer(handle, delimiter="\t", lineterminator="\n")
        writer.writerow(["site", "slot", "file_target", "stub", "runtime_target", "target_section",
                         "resolution", "continuation", "replaced", "function", "function_securom",
                         "runs"])
        for site in sorted(sites):
            slot, _ = sites[site]
            file_target = struct.unpack("<I", image.read(slot, 4))[0]
            function, securom = containing(site)
            target = final.get(slot)
            continuation, replaced, section = "", "", ""
            if target is None:
                resolution = "conflict" if slot in resolved else "unresolved"
            else:
                section = tracer.section_of(target)
                executed = tracer.trace(target, site)
                if tracer.continuation:
                    resolution = "cave"
                    continuation = f"{tracer.continuation:08x}"
                    replaced = replaced_instructions(executed, resolved_constant)
                else:
                    resolution = "body"
            seen = len(resolved.get(slot, {}))
            writer.writerow([
                f"{site:08x}", f"{slot:08x}", f"{file_target:08x}", stub_shape(image, md, file_target),
                f"{target:08x}" if target is not None else "", section, resolution, continuation,
                replaced, f"{function:08x}" if function is not None else "", securom,
                f"{seen}/{len(args.runs)}"])
            counts[(resolution, securom or "-", "entry" if function == site else "inner")] += 1

    print(f"{len(sites)} sites, {len(final)} resolved slots, {conflicts} conflicting -> {args.out}")
    for key, count in sorted(counts.items(), key=lambda item: -item[1]):
        print(f"  {count:5}  {' / '.join(key)}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
