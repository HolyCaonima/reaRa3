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

Manual state lives in ledger_annotations.tsv (address, status, owner, module,
notes) next to the ledger, so regenerating never loses claims or findings.
Non-empty annotation fields override the generated ones.

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

ANNOTATION_FIELDS = ("status", "owner", "module", "notes")

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


def load_annotations(path: pathlib.Path) -> dict[str, dict[str, str]]:
    """addr(hex, no 0x, lower) -> non-empty annotation fields."""
    annotations: dict[str, dict[str, str]] = {}
    if not path.exists():
        return annotations
    with path.open(encoding="utf-8") as handle:
        for row in csv.DictReader(handle, delimiter="\t"):
            addr = row["address"].strip().lower().removeprefix("0x").zfill(8)
            if addr in annotations:
                raise ValueError(f"{path}: duplicate annotation for {addr}")
            annotations[addr] = {
                key: row[key].strip() for key in ANNOTATION_FIELDS if (row.get(key) or "").strip()
            }
    return annotations


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
    parser.add_argument(
        "--annotations",
        type=pathlib.Path,
        help="manual ledger state (default: ledger_annotations.tsv next to --out)",
    )
    args = parser.parse_args()

    functions = args.inputs / "functions.tsv"
    if not functions.exists():
        print(f"missing {functions}", file=sys.stderr)
        return 1

    annotations = load_annotations(args.annotations or args.out.parent / "ledger_annotations.tsv")
    unused = set(annotations)
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
            manual = annotations.get(addr, {})
            unused.discard(addr)
            module = manual.get("module") or guess_module(source_file, section)
            status = manual.get("status", "unanalyzed")
            writer.writerow(
                [row["entry"], row["name"], section, row["size"], source_file,
                 sec, module, manual.get("owner", ""), status, manual.get("notes", "")]
            )
            rows += 1
            counts[status] += 1
            module_counts[module] += 1
            securom_counts[sec] += 1

    print(f"wrote {rows} rows to {args.out}")
    if unused:
        print("annotations without a function:", " ".join(sorted(unused)))
    print("status  :", dict(counts))
    print("securom :", dict(sorted(securom_counts.items(), key=lambda x: -x[1])))
    print("modules :")
    for module, count in module_counts.most_common():
        print(f"  {count:6} {module}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
