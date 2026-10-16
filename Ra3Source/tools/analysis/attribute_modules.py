#!/usr/bin/env python3
"""Estimate RA3 function modules from ledger, vtables, and local anchors.

The estimator deliberately uses only strong, reproducible evidence:

* a ledger module (source path or manual annotation);
* agreement among the ledger-anchored methods of a vtable;
* the nearest ledger anchor in .text, inside a data-selected window.

The executable scan only reads bytes in clean ledger function ranges in
.text.  It recognizes E8 rel32 call sites; it is not a disassembler.

Usage (from the repository root):
  python Ra3Source/tools/analysis/attribute_modules.py \
    --inputs D:/Game/Ra3/RevRa3/work/analysis/ghidra \
    --exe D:/Game/Ra3/Game/Data/ra3_1.0.game
"""
from __future__ import annotations

import argparse
import bisect
import collections
import csv
import pathlib
import random
import struct
import sys
from dataclasses import dataclass


PROJECT_MODULES = (
    "asset",
    "audio",
    "common",
    "core",
    "gameclient",
    "gamelogic",
    "io",
    "net",
    "render",
    "ui",
)
SPECIAL_MODULES = ("thirdparty", "securom")
SECUROM_SECTIONS = {"Stext", "Sitext", "Srdata", "Sdata", "Sidata", ".securom"}
WINDOW_CANDIDATES = tuple(1 << shift for shift in range(10, 17))  # 1--64 KiB
MIN_LOCALITY_PAIRS = 20
MIN_LOCALITY_AGREEMENT = 0.90
HOLDOUT_FRACTION = 0.20
HOLDOUT_SEED = 0x524133  # ASCII "RA3"


@dataclass(frozen=True)
class Function:
    address: int
    name: str
    section: str
    size: int
    source_file: str
    securom: str
    module: str


@dataclass(frozen=True)
class Estimate:
    module: str
    method: str
    anchor: int | None = None
    source_file: str = ""
    distance: int | None = None
    confidence: str = ""


def read_ledger(path: pathlib.Path) -> list[Function]:
    functions: list[Function] = []
    with path.open(encoding="utf-8") as handle:
        for row in csv.DictReader(handle, delimiter="\t"):
            functions.append(
                Function(
                    int(row["address"], 16),
                    row["name"],
                    row["section"],
                    int(row["size"]),
                    row["source_file"],
                    row["securom"],
                    row["module"],
                )
            )
    if not functions:
        raise ValueError(f"{path}: empty ledger")
    return functions


def read_vtables(path: pathlib.Path, known: set[int]) -> tuple[dict[int, set[int]], int]:
    """Return function -> vtables and the number of distinct vtables."""
    memberships: dict[int, set[int]] = collections.defaultdict(set)
    vtables: set[int] = set()
    with path.open(encoding="utf-8") as handle:
        for row in csv.DictReader(handle, delimiter="\t"):
            target = int(row["target"], 16)
            if target not in known:
                continue
            vtable = int(row["vtable"], 16)
            memberships[target].add(vtable)
            vtables.add(vtable)
    return memberships, len(vtables)


def anchored(function: Function) -> bool:
    return function.module in PROJECT_MODULES or function.module == "thirdparty"


def text_anchors(
    functions: list[Function], excluded: set[int] | None = None
) -> list[Function]:
    excluded = excluded or set()
    return sorted(
        (
            function
            for function in functions
            if function.address not in excluded
            and function.section == ".text"
            and anchored(function)
        ),
        key=lambda function: function.address,
    )


def adjacent_locality(anchors: list[Function], window: int) -> tuple[int, int]:
    """Project-anchor adjacent pairs at most window apart: (agree, total).

    Third-party anchors are barriers and are intentionally excluded from the
    agreement denominator.
    """
    agree = total = 0
    for left, right in zip(anchors, anchors[1:]):
        if right.address - left.address > window:
            continue
        if left.module not in PROJECT_MODULES or right.module not in PROJECT_MODULES:
            continue
        total += 1
        agree += left.module == right.module
    return agree, total


def select_window(anchors: list[Function]) -> tuple[int, list[tuple[int, int, int]]]:
    measurements: list[tuple[int, int, int]] = []
    selected = WINDOW_CANDIDATES[0]
    for window in WINDOW_CANDIDATES:
        agree, total = adjacent_locality(anchors, window)
        measurements.append((window, agree, total))
        if total >= MIN_LOCALITY_PAIRS and agree / total >= MIN_LOCALITY_AGREEMENT:
            selected = window
    return selected, measurements


def build_vtable_evidence(
    functions: list[Function],
    memberships: dict[int, set[int]],
    excluded: set[int] | None = None,
) -> tuple[dict[int, set[str]], dict[int, list[Function]]]:
    excluded = excluded or set()
    by_address = {function.address: function for function in functions}
    table_modules: dict[int, set[str]] = collections.defaultdict(set)
    table_anchors_by_address: dict[int, dict[int, Function]] = collections.defaultdict(dict)
    for address, tables in memberships.items():
        function = by_address[address]
        if function.address in excluded or not anchored(function):
            continue
        for table in tables:
            table_modules[table].add(function.module)
            table_anchors_by_address[table][function.address] = function
    table_anchors = {
        table: list(anchors.values())
        for table, anchors in table_anchors_by_address.items()
    }
    return table_modules, table_anchors


def vtable_estimate(
    function: Function,
    memberships: dict[int, set[int]],
    table_modules: dict[int, set[str]],
    table_anchors: dict[int, list[Function]],
) -> tuple[Estimate | None, set[str]]:
    modules: set[str] = set()
    supports: list[Function] = []
    for table in memberships.get(function.address, ()):
        evidence = table_modules.get(table, set())
        # One recovered method is too weak for the noisy candidate-vtable
        # inventory.  Require two distinct agreeing ledger anchors.
        if len(evidence) == 1 and len(table_anchors.get(table, ())) >= 2:
            modules.update(evidence)
            supports.extend(table_anchors[table])
        elif len(evidence) > 1:
            modules.update(evidence)
    if not modules:
        return None, set()
    if modules == {"thirdparty"}:
        return None, set()
    if len(modules) != 1:
        return None, modules
    module = next(iter(modules))
    support = min(
        (item for item in supports if item.module == module),
        key=lambda item: (abs(item.address - function.address), item.address),
    )
    return (
        Estimate(
            module,
            "vtable",
            support.address,
            support.source_file,
            abs(support.address - function.address),
            "high",
        ),
        set(),
    )


def neighbor_estimate(
    function: Function, anchors: list[Function], addresses: list[int], window: int
) -> tuple[Estimate | None, set[str]]:
    if function.section != ".text" or function.section in SECUROM_SECTIONS:
        return None, set()
    index = bisect.bisect_left(addresses, function.address)
    candidates: list[Function] = []
    if index:
        candidates.append(anchors[index - 1])
    if index < len(anchors):
        candidates.append(anchors[index])
    if not candidates:
        return None, set()
    distance = min(abs(item.address - function.address) for item in candidates)
    nearest = [item for item in candidates if abs(item.address - function.address) == distance]
    modules = {item.module for item in nearest}
    if distance > window or len(modules) != 1 or "thirdparty" in modules:
        return None, modules if distance <= window else set()
    anchor = min(nearest, key=lambda item: item.address)
    if distance <= 0x1000:
        confidence = "high"
    elif distance <= 0x4000:
        confidence = "medium"
    else:
        confidence = "low"
    return (
        Estimate(
            anchor.module,
            "neighbor",
            anchor.address,
            anchor.source_file,
            distance,
            confidence,
        ),
        set(),
    )


def estimate_all(
    functions: list[Function],
    memberships: dict[int, set[int]],
    window: int,
    excluded: set[int] | None = None,
) -> tuple[dict[int, Estimate], dict[str, object]]:
    excluded = excluded or set()
    anchors = text_anchors(functions, excluded)
    addresses = [function.address for function in anchors]
    table_modules, table_anchors = build_vtable_evidence(functions, memberships, excluded)
    estimates: dict[int, Estimate] = {}
    vtable_conflicts: dict[tuple[str, ...], int] = collections.Counter()
    neighbor_ties = 0
    thirdparty_barriers = 0
    for function in functions:
        if function.address not in excluded and function.module != "unknown":
            estimates[function.address] = Estimate(
                function.module,
                "ledger",
                function.address,
                function.source_file,
                0,
                "certain",
            )
            continue
        if function.section in SECUROM_SECTIONS:
            estimates[function.address] = Estimate("unknown", "unknown")
            continue
        estimate, conflict = vtable_estimate(
            function, memberships, table_modules, table_anchors
        )
        if estimate:
            estimates[function.address] = estimate
            continue
        if conflict:
            vtable_conflicts[tuple(sorted(conflict))] += 1
            estimates[function.address] = Estimate(
                "unknown", "unknown", confidence="vtable-conflict"
            )
            continue
        estimate, conflict = neighbor_estimate(function, anchors, addresses, window)
        if estimate:
            estimates[function.address] = estimate
        else:
            if conflict:
                if conflict == {"thirdparty"}:
                    thirdparty_barriers += 1
                else:
                    neighbor_ties += 1
            estimates[function.address] = Estimate(
                "unknown",
                "unknown",
                confidence=(
                    "thirdparty-barrier"
                    if conflict == {"thirdparty"}
                    else "neighbor-tie" if conflict else ""
                ),
            )
    conflicts: dict[str, object] = {
        "vtable": vtable_conflicts,
        "neighbor_ties": neighbor_ties,
        "thirdparty_barriers": thirdparty_barriers,
    }
    return estimates, conflicts


def holdout_addresses(functions: list[Function]) -> set[int]:
    rng = random.Random(HOLDOUT_SEED)
    by_module: dict[str, list[int]] = collections.defaultdict(list)
    for function in functions:
        if function.section == ".text" and function.module in PROJECT_MODULES:
            by_module[function.module].append(function.address)
    result: set[int] = set()
    for module in PROJECT_MODULES:
        addresses = sorted(by_module[module])
        if len(addresses) < 2:
            continue
        count = max(1, round(len(addresses) * HOLDOUT_FRACTION))
        result.update(rng.sample(addresses, count))
    return result


def evaluate_holdout(
    functions: list[Function], memberships: dict[int, set[int]]
) -> tuple[int, int, dict[tuple[str, str], list[int]], int]:
    held = holdout_addresses(functions)
    training_anchors = text_anchors(functions, held)
    window, _ = select_window(training_anchors)
    estimates, _ = estimate_all(functions, memberships, window, held)
    truth = {function.address: function.module for function in functions}
    scores: dict[tuple[str, str], list[int]] = collections.defaultdict(
        lambda: [0, 0, 0]
    )
    for address in sorted(held):
        estimate = estimates[address]
        key = (truth[address], estimate.method)
        score = scores[key]
        score[2] += 1  # held out
        if estimate.method != "unknown":
            score[1] += 1  # predicted
            score[0] += estimate.module == truth[address]
    project_training = sum(
        anchor.module in PROJECT_MODULES for anchor in training_anchors
    )
    return len(held), window, scores, project_training


def pe_sections(data: bytes) -> list[tuple[int, int, int, int]]:
    if data[:2] != b"MZ":
        raise ValueError("executable has no MZ header")
    pe_offset = struct.unpack_from("<I", data, 0x3C)[0]
    if data[pe_offset : pe_offset + 4] != b"PE\0\0":
        raise ValueError("executable has no PE header")
    section_count = struct.unpack_from("<H", data, pe_offset + 6)[0]
    optional_size = struct.unpack_from("<H", data, pe_offset + 20)[0]
    table = pe_offset + 24 + optional_size
    sections = []
    for index in range(section_count):
        offset = table + index * 40
        virtual_size, virtual_address, raw_size, raw_offset = struct.unpack_from(
            "<IIII", data, offset + 8
        )
        sections.append((virtual_address, virtual_size, raw_offset, raw_size))
    return sections


def address_to_offset(
    address: int, size: int, sections: list[tuple[int, int, int, int]]
) -> int | None:
    rva = address - 0x00400000
    for virtual_address, virtual_size, raw_offset, raw_size in sections:
        extent = max(virtual_size, raw_size)
        if virtual_address <= rva and rva + size <= virtual_address + extent:
            relative = rva - virtual_address
            if relative + size <= raw_size:
                return raw_offset + relative
    return None


def direct_call_counts(
    functions: list[Function], executable: pathlib.Path
) -> collections.Counter[int]:
    data = executable.read_bytes()
    sections = pe_sections(data)
    targets = {function.address for function in functions}
    calls: collections.Counter[int] = collections.Counter()
    for function in functions:
        if (
            function.section != ".text"
            or function.securom != "clean"
            or function.size < 5
        ):
            continue
        offset = address_to_offset(function.address, function.size, sections)
        if offset is None:
            continue
        body = data[offset : offset + function.size]
        for index in range(len(body) - 4):
            if body[index] != 0xE8:
                continue
            displacement = struct.unpack_from("<i", body, index + 1)[0]
            target = function.address + index + 5 + displacement
            if target in targets:
                calls[target] += 1
    return calls


def pct(correct: int, predicted: int) -> str:
    return "n/a" if not predicted else f"{100.0 * correct / predicted:.1f}%"


def write_report(
    path: pathlib.Path,
    functions: list[Function],
    estimates: dict[int, Estimate],
    vtable_inventory_count: int,
    vtable_method_count: int,
    source_file_count: int,
    window: int,
    measurements: list[tuple[int, int, int]],
    conflicts: dict[str, object],
    held_count: int,
    holdout_window: int,
    holdout_scores: dict[tuple[str, str], list[int]],
    training_count: int,
    call_counts: collections.Counter[int],
) -> None:
    summary: dict[tuple[str, str], int] = collections.Counter(
        (estimate.module, estimate.method) for estimate in estimates.values()
    )
    input_modules = collections.Counter(function.module for function in functions)
    securom: dict[tuple[str, str], int] = collections.Counter(
        (estimates[function.address].module, function.securom)
        for function in functions
        if function.securom != "clean"
    )
    anchors = text_anchors(functions)
    project_anchor_count = sum(anchor.module in PROJECT_MODULES for anchor in anchors)
    lines = [
        "RA3 module attribution report",
        "=============================",
        f"ledger_rows\t{len(functions)}",
        f"source_file_rows\t{source_file_count}",
        f"vtable_candidates\t{vtable_inventory_count}",
        f"vtables_with_ledger_methods\t{vtable_method_count}",
        f"project_text_anchors\t{project_anchor_count}",
        f"selected_neighbor_window\t0x{window:X}",
        "window_rule\tlargest candidate with "
        f">= {MIN_LOCALITY_PAIRS} adjacent project-anchor pairs and "
        f">= {MIN_LOCALITY_AGREEMENT:.0%} agreement",
        "",
        "adjacent_anchor_locality",
        "window\tagree\ttotal\trate",
    ]
    for candidate, agree, total in measurements:
        rate = "n/a" if not total else f"{100.0 * agree / total:.1f}%"
        lines.append(f"0x{candidate:X}\t{agree}\t{total}\t{rate}")
    lines += [
        "",
        "ledger_input_modules",
        "module\trows",
    ]
    for module, count in sorted(input_modules.items()):
        lines.append(f"{module}\t{count}")
    lines += [
        "",
        "module_summary",
        "module\tledger\tvtable\tneighbor\ttotal",
    ]
    modules = PROJECT_MODULES + ("thirdparty", "securom", "unknown")
    for module in modules:
        ledger = summary[module, "ledger"]
        vtable = summary[module, "vtable"]
        neighbor = summary[module, "neighbor"]
        unknown = summary[module, "unknown"]
        total = ledger + vtable + neighbor + unknown
        lines.append(f"{module}\t{ledger}\t{vtable}\t{neighbor}\t{total}")
    lines += [
        f"reconciled_total\t{sum(summary.values())}",
        "",
        "securom_by_estimated_module",
        "module\tentry-lost\tinner-patched\tentry-recoverable\tpatched\ttotal_affected",
    ]
    for module in modules:
        values = [securom[module, status] for status in (
            "entry-lost", "inner-patched", "entry-recoverable", "patched"
        )]
        lines.append(f"{module}\t" + "\t".join(map(str, values + [sum(values)])))
    totals = [
        sum(securom[module, status] for module in modules)
        for status in ("entry-lost", "inner-patched", "entry-recoverable", "patched")
    ]
    lines.append("all\t" + "\t".join(map(str, totals + [sum(totals)])))
    lines += [
        "",
        "holdout",
        f"seed\t0x{HOLDOUT_SEED:X}",
        f"fraction_per_module\t{HOLDOUT_FRACTION:.0%}",
        f"held_out\t{held_count}",
        f"training_project_anchors\t{training_count}",
        f"evaluation_window\t0x{holdout_window:X}",
        "module\tmethod\tcorrect\tpredicted\theld_out\tprecision\tcoverage",
    ]
    for module in PROJECT_MODULES:
        module_held = sum(
            values[2]
            for (score_module, _), values in holdout_scores.items()
            if score_module == module
        )
        for method in ("vtable", "neighbor", "unknown"):
            correct, predicted, held = holdout_scores[module, method]
            if not held:
                continue
            coverage = (
                "n/a" if not module_held else f"{100.0 * predicted / module_held:.1f}%"
            )
            lines.append(
                f"{module}\t{method}\t{correct}\t{predicted}\t{held}\t"
                f"{pct(correct, predicted)}\t{coverage}"
            )
    for method in ("vtable", "neighbor", "unknown"):
        values = [
            value
            for (module, score_method), value in holdout_scores.items()
            if score_method == method
        ]
        correct = sum(value[0] for value in values)
        predicted = sum(value[1] for value in values)
        held = sum(value[2] for value in values)
        if held:
            lines.append(
                f"all\t{method}\t{correct}\t{predicted}\t{held}\t"
                f"{pct(correct, predicted)}\t{100.0 * predicted / held_count:.1f}%"
            )
    vtable_conflicts = conflicts["vtable"]
    assert isinstance(vtable_conflicts, collections.Counter)
    lines += [
        "",
        "conflicts",
        f"vtable_conflict_functions\t{sum(vtable_conflicts.values())}",
        f"neighbor_tie_functions\t{conflicts['neighbor_ties']}",
        f"thirdparty_barrier_functions\t{conflicts['thirdparty_barriers']}",
    ]
    for modules_key, count in sorted(vtable_conflicts.items()):
        lines.append(f"vtable_modules:{','.join(modules_key)}\t{count}")
    lines += [
        "",
        "direct_calls",
        f"recognized_E8_sites\t{sum(call_counts.values())}",
        f"targets_with_callers\t{len(call_counts)}",
        "note\tE8 bytes are scanned in clean .text ledger ranges; "
        "counts are call sites, not disassembled instructions.",
    ]
    path.write_text("\n".join(lines) + "\n", encoding="utf-8")


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument(
        "--ledger",
        type=pathlib.Path,
        default=pathlib.Path("Ra3Source/docs/analysis/ra3_1.0/function_ledger.tsv"),
    )
    parser.add_argument(
        "--inputs",
        type=pathlib.Path,
        default=pathlib.Path("work/analysis/ghidra"),
        help="Ghidra export directory (only vtables/vtable_methods.tsv is read)",
    )
    parser.add_argument("--vtable-methods", type=pathlib.Path)
    parser.add_argument("--vtables", type=pathlib.Path)
    parser.add_argument("--source-functions", type=pathlib.Path)
    parser.add_argument("--exe", required=True, type=pathlib.Path)
    parser.add_argument(
        "--out",
        type=pathlib.Path,
        default=pathlib.Path("Ra3Source/docs/analysis/ra3_1.0/module_estimates.tsv"),
    )
    parser.add_argument(
        "--report",
        type=pathlib.Path,
        default=pathlib.Path("Ra3Source/docs/analysis/ra3_1.0/module_estimates_report.txt"),
    )
    args = parser.parse_args()
    vtable_path = args.vtable_methods or args.inputs / "vtables" / "vtable_methods.tsv"
    vtable_inventory = args.vtables or args.ledger.parent / "vtables.tsv"
    source_functions = (
        args.source_functions or args.ledger.parent / "source_file_functions.tsv"
    )
    for required in (
        args.ledger,
        vtable_path,
        vtable_inventory,
        source_functions,
        args.exe,
    ):
        if not required.exists():
            print(f"missing {required}", file=sys.stderr)
            return 1

    functions = read_ledger(args.ledger)
    memberships, vtable_method_count = read_vtables(
        vtable_path, {function.address for function in functions}
    )
    with vtable_inventory.open(encoding="utf-8") as handle:
        vtable_inventory_count = sum(1 for _ in csv.DictReader(handle, delimiter="\t"))
    with source_functions.open(encoding="utf-8") as handle:
        source_file_count = sum(1 for _ in csv.DictReader(handle, delimiter="\t"))
    anchors = text_anchors(functions)
    window, measurements = select_window(anchors)
    estimates, conflicts = estimate_all(functions, memberships, window)
    held_count, holdout_window, holdout_scores, training_count = evaluate_holdout(
        functions, memberships
    )
    call_counts = direct_call_counts(functions, args.exe)

    args.out.parent.mkdir(parents=True, exist_ok=True)
    with args.out.open("w", encoding="utf-8", newline="") as handle:
        writer = csv.writer(handle, delimiter="\t", lineterminator="\n")
        writer.writerow(
            [
                "address",
                "module_estimate",
                "method",
                "anchor_address",
                "anchor_source_file",
                "distance",
                "confidence",
                "direct_callers",
            ]
        )
        for function in functions:
            estimate = estimates[function.address]
            writer.writerow(
                [
                    f"{function.address:08x}",
                    estimate.module,
                    estimate.method,
                    "" if estimate.anchor is None else f"{estimate.anchor:08x}",
                    estimate.source_file,
                    "" if estimate.distance is None else estimate.distance,
                    estimate.confidence,
                    call_counts[function.address],
                ]
            )
    write_report(
        args.report,
        functions,
        estimates,
        vtable_inventory_count,
        vtable_method_count,
        source_file_count,
        window,
        measurements,
        conflicts,
        held_count,
        holdout_window,
        holdout_scores,
        training_count,
        call_counts,
    )
    print(args.report.read_text(encoding="utf-8"), end="")
    print(f"wrote {len(functions)} rows to {args.out}")
    print(f"wrote report to {args.report}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
