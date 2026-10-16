#!/usr/bin/env python3
"""Builds the canonical function ledger for the RA3 reconstruction.

The ledger is the single source of truth for parallel work: every function in
ra3_1.0.game with its address, section, size, recovered source file, SecuROM
status, guessed target module, and reconstruction state. Parallel agents claim
functions by setting `owner` and update `status`; they must not renumber rows.

Inputs (produced by the Ghidra scripts in tools/ghidra, kept under work/):
  functions.tsv               entry, size, block, source, name
  source_file_functions.tsv   source_path, function_count, functions
  securom_flow_sites.tsv      site, function, function_entry, ...
  securom_thunk_bodies.tsv    site, function, at_entry, class, linear_instructions, ...

Usage:
  python tools/analysis/build_function_ledger.py \
      --inputs work/analysis/ghidra \
      --out Ra3Source/docs/analysis/ra3_1.0/function_ledger.tsv
"""
from __future__ import annotations

import argparse
import collections
import csv
import pathlib
import re
import sys

# Functions already reverse-engineered this far (the filesystem / BigHandler /
# RefpackInflate cluster, plus the SkuDef directive handlers). Keyed by address.
REVERSED = {
    "009f5af0", "009f5a80", "009f5db0", "009f5e00", "009f5e90", "009f5ed0",
    "009f5f40", "009f5f80", "009f63b0", "009f65e0", "009f6670", "009f6810",
    "009f6910", "009f6990", "009f6a20", "009f6a50", "009f6aa0", "009f6b00",
    "009f6b90", "009f6d00", "009f7130", "009f72e0", "009f74f0", "009f7620",
    "009f3650", "004ce600", "004d2490", "004d18f6", "004d1b60",
}

SECUROM_BLOCKS = {"Stext", "Sitext", "Srdata", "Sdata", "Sidata", ".securom"}

# source-file substring -> target module. First match wins; order matters.
MODULE_RULES = [
    (r"external\\eastl", "thirdparty"),
    (r"external\\", "thirdparty"),
    (r"openssl", "thirdparty"),
    (r"libraries\\source\\assetmanager", "asset"),
    (r"libraries\\source\\(ini|string|debug|memorypool)", "core"),
    (r"lib-packages\\rtsbase", "io"),
    (r"rtsxmlpipeline", "asset"),
    (r"gameenginedevice\\w3ddevice", "render"),
    (r"gameenginedevice\\sageaudiodevice", "audio"),
    (r"sagerender", "render"),
    (r"sageingameui", "ui"),
    (r"gameengine\\gamelogic", "gamelogic"),
    (r"gameengine\\gameclient", "gameclient"),
    (r"gameengine\\gamenetwork", "net"),
    (r"gameengine\\common", "common"),
    (r"gameengine\\", "common"),
]


def guess_module(source_file: str, section: str) -> str:
    low = source_file.lower()
    for pattern, module in MODULE_RULES:
        if re.search(pattern, low):
            return module
    if section in SECUROM_BLOCKS:
        return "securom"
    return "unknown"


def load_source_map(path: pathlib.Path) -> dict[str, str]:
    """addr(hex, no 0x, lower) -> best recovered source file.

    Prefers a specific game source over the generic EASTL allocator, and a
    longer (more specific) path over a shorter one.
    """
    ref = re.compile(r"@([0-9a-fA-F]+)\[")
    best: dict[str, str] = {}
    if not path.exists():
        return best
    with path.open(encoding="utf-8") as handle:
        reader = csv.DictReader(handle, delimiter="\t")
        for row in reader:
            source = row["source_path"]
            is_external = source.lower().startswith("external\\")
            for token in row["functions"].split():
                m = ref.search(token)
                if not m:
                    continue
                addr = m.group(1).lower()
                current = best.get(addr)
                if current is None:
                    best[addr] = source
                    continue
                cur_external = current.lower().startswith("external\\")
                # Prefer a game source over external, then the longer path.
                if (cur_external and not is_external) or (
                    cur_external == is_external and len(source) > len(current)
                ):
                    best[addr] = source
    return best


def load_securom_status(flow: pathlib.Path, bodies: pathlib.Path):
    """name -> (status, detail). Classifies how a SecuROM patch affects a function."""
    patched = collections.defaultdict(lambda: {"entry": [], "inner": False})
    if bodies.exists():
        with bodies.open(encoding="utf-8") as handle:
            for row in csv.DictReader(handle, delimiter="\t"):
                name = row["function"]
                if name == "<none>":
                    continue
                if row["at_entry"].lower() == "true":
                    try:
                        linear = int(row["linear_instructions"])
                    except ValueError:
                        linear = 0
                    patched[name]["entry"].append((row["class"], linear))
                else:
                    patched[name]["inner"] = True

    status: dict[str, str] = {}
    for name, info in patched.items():
        if info["entry"]:
            recoverable = any(
                cls in {"reaches-ret", "ends-in-jmp", "too-long"} and linear >= 6
                for cls, linear in info["entry"]
            )
            status[name] = "entry-recoverable" if recoverable else "entry-lost"
        elif info["inner"]:
            status[name] = "inner-patched"

    # Any function with a jump into SecuROM but no body classification.
    if flow.exists():
        with flow.open(encoding="utf-8") as handle:
            for row in csv.DictReader(handle, delimiter="\t"):
                name = row["function"]
                if name != "<none>" and name not in status:
                    status[name] = "patched"
    return status


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--inputs", default="work/analysis/ghidra", type=pathlib.Path)
    parser.add_argument(
        "--out",
        default="Ra3Source/docs/analysis/ra3_1.0/function_ledger.tsv",
        type=pathlib.Path,
    )
    args = parser.parse_args()

    functions = args.inputs / "functions.tsv"
    if not functions.exists():
        print(f"missing {functions}", file=sys.stderr)
        return 1

    source_map = load_source_map(args.inputs / "source_file_functions.tsv")
    securom = load_securom_status(
        args.inputs / "securom_flow_sites.tsv",
        args.inputs / "securom_thunk_bodies.tsv",
    )

    args.out.parent.mkdir(parents=True, exist_ok=True)
    counts = collections.Counter()
    module_counts = collections.Counter()
    securom_counts = collections.Counter()
    rows = 0

    with functions.open(encoding="utf-8") as src, args.out.open(
        "w", encoding="utf-8", newline=""
    ) as dst:
        reader = csv.DictReader(src, delimiter="\t")
        writer = csv.writer(dst, delimiter="\t", lineterminator="\n")
        writer.writerow(
            ["address", "name", "section", "size", "source_file",
             "securom", "module", "owner", "status", "notes"]
        )
        for row in reader:
            addr = row["entry"].lower()
            section = row["block"]
            source_file = source_map.get(addr, "")
            sec = securom.get(row["name"], "clean")
            module = guess_module(source_file, section)
            status = "reversed" if addr in REVERSED else "unanalyzed"
            writer.writerow(
                [row["entry"], row["name"], section, row["size"], source_file,
                 sec, module, "", status, ""]
            )
            rows += 1
            counts[status] += 1
            module_counts[module] += 1
            securom_counts[sec] += 1

    print(f"wrote {rows} rows to {args.out}")
    print("status  :", dict(counts))
    print("securom :", dict(sorted(securom_counts.items(), key=lambda x: -x[1])))
    print("modules :")
    for module, count in module_counts.most_common():
        print(f"  {count:6} {module}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
