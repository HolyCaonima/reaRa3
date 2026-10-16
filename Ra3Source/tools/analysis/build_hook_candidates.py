#!/usr/bin/env python3
"""Join Ghidra hook facts, the canonical ledger, and executable prologues.

The output contains every exported clean .text function.  ``tier`` is ``1`` or
``2`` only when the function satisfies the conservative Route-A rules; ``-``
means that it is not currently a hook candidate.
"""

from __future__ import annotations

import argparse
import collections
import csv
import re
from pathlib import Path

import capstone
from capstone import x86

from disasm import Image, analyze_hook_prologue

DEFAULT_EXE = Path(r"D:\Game\Ra3\Game\Data\ra3_1.0.game")

PURE_IMPORTS = {
    "abs", "labs", "tolower", "toupper", "strlen", "strcmp", "strncmp",
    "stricmp", "strnicmp", "wcscmp", "wcsncmp", "wcslen", "memcpy",
    "memmove", "memset", "memcmp", "bsearch", "qsort", "sqrt", "sqrtf",
    "sin", "sinf", "cos", "cosf", "tan", "tanf", "atan2", "atan2f",
    "floor", "floorf", "ceil", "ceilf", "fabs", "fabsf",
}

OUTPUT_COLUMNS = [
    "address", "name", "size", "module", "source_file", "tier", "pattern",
    "direct_callers", "parameter_count", "calling_convention", "return_forms",
    "abi", "stolen_length", "movable_prologue", "expected16",
    "instruction_count", "internal_callees", "external_callees",
    "unresolved_calls", "indirect_tail_jumps", "global_writes", "global_reads", "uses_fs",
    "uses_x87", "uses_sse", "has_loops", "strings", "op_profile",
    "existing_equivalent",
]


def read_tsv(path: Path, key: str) -> dict[str, dict[str, str]]:
    with path.open(encoding="utf-8", newline="") as handle:
        return {row[key].lower().removeprefix("0x").zfill(8): row
                for row in csv.DictReader(handle, delimiter="\t")}


def split_values(value: str) -> list[str]:
    return [item for item in value.split(",") if item]


def import_basename(value: str) -> str:
    name = value.rsplit("::", 1)[-1].rsplit("!", 1)[-1].lower()
    name = re.sub(r"^(imp_|__imp_|_+)", "", name)
    name = re.sub(r"@\d+$", "", name)
    return name


def imports_are_pure(value: str) -> bool:
    imports = split_values(value)
    return all(import_basename(item) in PURE_IMPORTS for item in imports)


def profile(row: dict[str, str]) -> collections.Counter[str]:
    result: collections.Counter[str] = collections.Counter()
    for item in row["op_profile"].split(","):
        if ":" not in item:
            continue
        mnemonic, count = item.rsplit(":", 1)
        try:
            result[mnemonic] = int(count)
        except ValueError:
            pass
    return result


def classify_pattern(row: dict[str, str]) -> str:
    """Documented, ordered heuristic classification; first match wins."""
    imports = {import_basename(value) for value in split_values(row["external_callees"])}
    ops = profile(row)
    text = " ".join((row.get("name", ""), row.get("strings", ""))).lower()
    instructions = int(row["instruction_count"])
    loops = row["has_loops"] == "true"
    bit_ops = sum(ops[name] for name in
                  ("xor", "and", "or", "shl", "shr", "sar", "rol", "ror"))

    if "crc" in text or "hash" in text or (loops and bit_ops >= 4):
        return "hash-crc"
    if imports & {"strcmp", "strncmp", "stricmp", "strnicmp", "wcscmp", "wcsncmp"}:
        return "string-compare"
    if imports & {"tolower", "toupper"}:
        return "case-conversion"
    if imports & {"memcpy", "memmove"}:
        return "memory-copy"
    if "memset" in imports:
        return "memory-fill"
    if imports & {"strlen", "wcslen"} or "string" in text or "str" in text:
        return "string-utility"
    if row["uses_x87"] == "true" or row["uses_sse"] == "true":
        return "floating-point"
    if instructions <= 6 and not loops and not split_values(row["internal_callees"]):
        return "field-accessor"
    if loops and any(token in text for token in ("list", "node", "tree", "map", "vector")):
        return "container-walk"
    if loops:
        return "loop-transform"
    if bit_ops >= 2 or any(name in ops for name in ("imul", "idiv", "div", "mul")):
        return "integer-bitmath"
    return "other"


def known_return(value: str) -> bool:
    forms = split_values(value)
    return bool(forms) and all(re.fullmatch(r"ret(?:\s+(?:0x)?[0-9a-fA-F]+)?", form)
                               for form in forms)


def abi(row: dict[str, str]) -> str:
    convention = row["calling_convention"] or "unknown"
    return f"{convention}({row['parameter_count']})/{row['return_forms'] or 'unknown-ret'}"


def count_indirect_tail_jumps(image: Image, address: int, size: int) -> int:
    """Count register/memory JMPs; Ghidra's call export handles indirect CALLs."""
    md = capstone.Cs(capstone.CS_ARCH_X86, capstone.CS_MODE_32)
    md.detail = True
    count = 0
    for insn in md.disasm(image.read(address, size), address):
        if insn.mnemonic != "jmp":
            continue
        if not insn.operands or insn.operands[0].type != x86.X86_OP_IMM:
            count += 1
    return count


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--export", required=True, type=Path)
    parser.add_argument("--ledger", required=True, type=Path)
    parser.add_argument("--exe", type=Path, default=DEFAULT_EXE)
    parser.add_argument("--out", required=True, type=Path)
    args = parser.parse_args()

    exported = read_tsv(args.export, "entry")
    ledger = read_tsv(args.ledger, "address")
    image = Image(args.exe)
    rows: dict[str, dict[str, str]] = {}

    for address in sorted(exported):
        fact = exported[address]
        meta = ledger.get(address)
        if meta is None:
            raise ValueError(f"exported function {address} is absent from ledger")
        prologue = analyze_hook_prologue(image, int(address, 16))
        indirect_tail_jumps = count_indirect_tail_jumps(
            image, int(address, 16), int(meta["size"]))
        internal = split_values(fact["internal_callees"])
        external = split_values(fact["external_callees"])
        base_ok = (
            fact["global_writes"] == "false"
            and fact["uses_fs"] == "false"
            and fact["unresolved_calls"] == "0"
            and indirect_tail_jumps == 0
            and prologue.decodable
            and prologue.position_independent
            and known_return(fact["return_forms"])
        )
        row = {
            "address": address,
            "name": meta["name"],
            "size": meta["size"],
            "module": meta["module"],
            "source_file": meta["source_file"],
            "tier": "-",
            "pattern": "",
            **{key: fact[key] for key in (
                "direct_callers", "parameter_count", "calling_convention",
                "return_forms", "instruction_count", "internal_callees",
                "external_callees", "unresolved_calls", "global_writes",
                "global_reads", "uses_fs", "uses_x87", "uses_sse", "has_loops",
                "strings", "op_profile")},
            "abi": abi(fact),
            "indirect_tail_jumps": str(indirect_tail_jumps),
            "stolen_length": str(prologue.stolen_length) if prologue.decodable else "",
            "movable_prologue": str(prologue.position_independent).lower(),
            "expected16": prologue.expected16.hex(" "),
            "existing_equivalent": meta["notes"],
        }
        if base_ok and not internal and not external:
            row["tier"] = "1"
        rows[address] = row

    tier1 = {address for address, row in rows.items() if row["tier"] == "1"}
    for address, row in rows.items():
        fact = exported[address]
        internal = {value.lower().removeprefix("0x").zfill(8)
                    for value in split_values(fact["internal_callees"])}
        external = split_values(fact["external_callees"])
        base_ok = (
            fact["global_writes"] == "false"
            and fact["uses_fs"] == "false"
            and fact["unresolved_calls"] == "0"
            and row["indirect_tail_jumps"] == "0"
            and row["movable_prologue"] == "true"
            and known_return(fact["return_forms"])
        )
        if (row["tier"] == "-" and base_ok and (internal or external)
                and internal <= tier1 and imports_are_pure(fact["external_callees"])):
            row["tier"] = "2"
        row["pattern"] = classify_pattern({**fact, "name": row["name"]})

    args.out.parent.mkdir(parents=True, exist_ok=True)
    with args.out.open("w", encoding="utf-8", newline="") as handle:
        writer = csv.DictWriter(handle, fieldnames=OUTPUT_COLUMNS, delimiter="\t",
                                lineterminator="\n")
        writer.writeheader()
        writer.writerows(rows[address] for address in sorted(rows))

    tier_counts = collections.Counter(row["tier"] for row in rows.values())
    pattern_counts = collections.Counter(
        row["pattern"] for row in rows.values() if row["tier"] in {"1", "2"})
    print(f"wrote {len(rows)} rows to {args.out}")
    print("tiers:", dict(sorted(tier_counts.items())))
    print("candidate patterns:", dict(sorted(pattern_counts.items())))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
