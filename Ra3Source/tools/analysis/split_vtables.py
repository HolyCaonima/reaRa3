"""Split contiguous code-pointer runs into evidence-backed RA3 vtables.

Only bytes belonging to ``.text`` functions marked ``securom == clean`` are
decoded.  The executable is opened read-only.  Data scans are restricted to
.rdata and .data.
"""
from __future__ import annotations

import argparse
import bisect
import csv
import struct
from collections import Counter, defaultdict
from dataclasses import dataclass, field
from pathlib import Path

import capstone
from capstone import x86

from recover_classes import DEFAULT_EXE, PeImage, instructions, read_tsv, write_tsv


@dataclass
class Boundary:
    kinds: set[str] = field(default_factory=set)
    functions: set[int] = field(default_factory=set)
    details: set[str] = field(default_factory=set)


def containing_run(starts: list[int], runs: dict[int, tuple[int, int]], value: int) -> int | None:
    index = bisect.bisect_right(starts, value) - 1
    if index < 0:
        return None
    start = starts[index]
    _, end = runs[start]
    return start if start <= value < end and value % 4 == 0 else None


def add_boundary(
    boundaries: dict[int, Boundary],
    address: int,
    kind: str,
    function: int | None = None,
    detail: str = "",
) -> None:
    boundary = boundaries[address]
    boundary.kinds.add(kind)
    if function is not None:
        boundary.functions.add(function)
    if detail:
        boundary.details.add(detail)


def scan_clean_text(
    image: PeImage,
    ledger: list[dict[str, str]],
    run_starts: list[int],
    runs: dict[int, tuple[int, int]],
    boundaries: dict[int, Boundary],
) -> tuple[int, int]:
    clean_count = 0
    skipped_count = 0
    for row in ledger:
        if row["section"] != ".text":
            continue
        if row["securom"] != "clean":
            skipped_count += 1
            continue
        address = int(row["address"], 16)
        size = int(row["size"] or "0")
        if size <= 0:
            continue
        clean_count += 1
        insns = instructions(image, address, size)
        pending: dict[int, tuple[int, int]] = {}
        for index, insn in enumerate(insns):
            if insn.mnemonic == "mov" and len(insn.operands) == 2:
                destination, source = insn.operands
                if destination.type == x86.X86_OP_REG:
                    pending.pop(destination.reg, None)
                    if source.type == x86.X86_OP_IMM:
                        value = source.imm & 0xFFFFFFFF
                        if containing_run(run_starts, runs, value) is not None:
                            pending[destination.reg] = (value, index)
                            add_boundary(
                                boundaries, value, "text_imm", address,
                                f"{insn.address:08X}:mov-reg-imm",
                            )
                if destination.type == x86.X86_OP_MEM:
                    value = None
                    propagated = False
                    if source.type == x86.X86_OP_IMM:
                        value = source.imm & 0xFFFFFFFF
                    elif source.type == x86.X86_OP_REG and source.reg in pending:
                        candidate, origin = pending[source.reg]
                        if index - origin <= 12:
                            value = candidate
                            propagated = True
                    memory = destination.mem
                    object_store = (
                        memory.base not in (x86.X86_REG_ESP, x86.X86_REG_EBP)
                        and memory.index == 0
                        and 0 <= memory.disp <= 0x1000
                    )
                    if (
                        object_store
                        and value is not None
                        and containing_run(run_starts, runs, value) is not None
                    ):
                        displacement = destination.mem.disp
                        kind = "vptr_store_secondary" if displacement else "vptr_store_primary"
                        add_boundary(
                            boundaries, value, kind, address,
                            f"{insn.address:08X}:store"
                            + ("-propagated" if propagated else "-immediate"),
                        )
            if insn.mnemonic == "call":
                pending.clear()

            for operand in insn.operands:
                value = None
                if operand.type == x86.X86_OP_IMM:
                    value = operand.imm & 0xFFFFFFFF
                elif operand.type == x86.X86_OP_MEM and operand.mem.base == 0:
                    value = operand.mem.disp & 0xFFFFFFFF
                if value is None or containing_run(run_starts, runs, value) is None:
                    continue
                direct_slot = operand.type == x86.X86_OP_MEM
                add_boundary(
                    boundaries,
                    value,
                    "direct_slot_ref" if direct_slot else "text_imm",
                    address,
                    f"{insn.address:08X}:{insn.mnemonic}",
                )
    return clean_count, skipped_count


def scan_data_references(
    image: PeImage,
    run_starts: list[int],
    runs: dict[int, tuple[int, int]],
    boundaries: dict[int, Boundary],
) -> int:
    count = 0
    for section in image.sections:
        if section.name not in (".rdata", ".data"):
            continue
        size = section.raw_size - (section.raw_size % 4)
        raw = image.read(section.va, size)
        if raw is None:
            continue
        for offset in range(0, len(raw), 4):
            value = struct.unpack_from("<I", raw, offset)[0]
            if containing_run(run_starts, runs, value) is None:
                continue
            location = section.va + offset
            # Do not mistake a vtable slot for a reference merely because a
            # function VA happens to overlap the vtable-address interval.
            if containing_run(run_starts, runs, location) is not None:
                continue
            add_boundary(boundaries, value, "data_pointer", detail=f"{location:08X}")
            count += 1
    return count


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("--exe", type=Path, default=DEFAULT_EXE)
    parser.add_argument(
        "--source-root",
        type=Path,
        default=Path(__file__).resolve().parents[2],
        help="Ra3Source directory",
    )
    args = parser.parse_args()
    root = args.source_root.resolve()
    analysis = root / "docs" / "analysis" / "ra3_1.0"
    image = PeImage(args.exe)
    original = read_tsv(analysis / "vtables.tsv")
    ledger = read_tsv(analysis / "function_ledger.tsv")
    assets = [row for row in read_tsv(analysis / "asset_types.tsv") if row["handler_vtable"]]

    runs: dict[int, tuple[int, int]] = {}
    run_rows: dict[int, dict[str, str]] = {}
    for row in original:
        start = int(row["vtable"], 16)
        end = start + int(row["slots"]) * 4
        runs[start] = (int(row["slots"]), end)
        run_rows[start] = row
    run_starts = sorted(runs)
    boundaries: dict[int, Boundary] = defaultdict(Boundary)
    for start in run_starts:
        add_boundary(boundaries, start, "original_run_start")

    clean_count, skipped_count = scan_clean_text(
        image, ledger, run_starts, runs, boundaries
    )
    data_reference_count = scan_data_references(image, run_starts, runs, boundaries)

    handler_starts = {int(row["handler_vtable"], 16) for row in assets}
    for start in handler_starts:
        run = containing_run(run_starts, runs, start)
        if run is None:
            raise RuntimeError(f"handler 0x{start:08X} is outside original vtable runs")
        add_boundary(boundaries, start, "asset_handler_registration")
        end = start + 5 * 4
        _, run_end = runs[run]
        if end <= run_end:
            add_boundary(boundaries, end, "asset_handler_5slot_end")

    # Independently documented constructor evidence.
    known_starts = {0x00C69460: "BigHandler base", 0x00BDAF98: "BigHandler derived"}
    for start, label in known_starts.items():
        if containing_run(run_starts, runs, start) is not None:
            add_boundary(boundaries, start, "documented_vtable", detail=label)

    output_rows: list[dict[str, object]] = []
    split_runs = 0
    unreferenced_segments = 0
    slot_histogram: Counter[int] = Counter()
    kind_counts: Counter[str] = Counter()
    for run_id, start in enumerate(run_starts):
        slots, end = runs[start]
        cuts = sorted(address for address in boundaries if start <= address < end)
        if len(cuts) > 1:
            split_runs += 1
        for segment_index, segment_start in enumerate(cuts):
            segment_end = cuts[segment_index + 1] if segment_index + 1 < len(cuts) else end
            if segment_end <= segment_start:
                continue
            boundary = boundaries[segment_start]
            segment_slots = (segment_end - segment_start) // 4
            slot_histogram[segment_slots] += 1
            for kind in boundary.kinds:
                kind_counts[kind] += 1
            substantive = boundary.kinds - {"original_run_start", "asset_handler_5slot_end"}
            if not substantive:
                unreferenced_segments += 1
            output_rows.append(
                {
                    "start": f"{segment_start:08X}",
                    "slots": segment_slots,
                    "end": f"{segment_end:08X}",
                    "evidence_kinds": ",".join(sorted(boundary.kinds)),
                    "referencing_functions": ",".join(
                        f"{value:08X}" for value in sorted(boundary.functions)
                    ),
                    "evidence_details": ",".join(sorted(boundary.details)),
                    "original_run_id": run_id,
                    "original_run_start": f"{start:08X}",
                    "original_run_slots": slots,
                    "block": run_rows[start]["block"],
                }
            )

    write_tsv(
        analysis / "vtables_split.tsv",
        (
            "start", "slots", "end", "evidence_kinds", "referencing_functions",
            "evidence_details", "original_run_id", "original_run_start",
            "original_run_slots", "block",
        ),
        output_rows,
    )

    output_by_start = {int(row["start"], 16): row for row in output_rows}
    handler_failures = []
    for handler in sorted(handler_starts):
        row = output_by_start.get(handler)
        if row is None or int(row["slots"]) != 5:
            handler_failures.append(f"{handler:08X}")
    known_failures = []
    for known in known_starts:
        if known not in output_by_start:
            known_failures.append(f"{known:08X}")
    validation_failures = handler_failures + known_failures

    stats: list[dict[str, object]] = [
        {"metric": "original_runs", "value": len(original)},
        {"metric": "runs_split", "value": split_runs},
        {"metric": "resulting_vtables", "value": len(output_rows)},
        {"metric": "unreferenced_segments", "value": unreferenced_segments},
        {"metric": "clean_text_functions_scanned", "value": clean_count},
        {"metric": "nonclean_text_functions_skipped", "value": skipped_count},
        {"metric": "data_pointer_references", "value": data_reference_count},
        {"metric": "asset_handlers_checked", "value": len(handler_starts)},
        {"metric": "asset_handlers_exactly_5_slots", "value": len(handler_starts) - len(handler_failures)},
        {
            "metric": "bighandler_base_boundary",
            "value": int(0x00C69460 in output_by_start),
        },
        {
            "metric": "bighandler_base_slots_to_next_boundary",
            "value": output_by_start.get(0x00C69460, {}).get("slots", ""),
        },
        {
            "metric": "bighandler_derived_boundary",
            "value": int(0x00BDAF98 in output_by_start),
        },
        {
            "metric": "bighandler_derived_slots_to_next_boundary",
            "value": output_by_start.get(0x00BDAF98, {}).get("slots", ""),
        },
        {"metric": "ground_truth_failures", "value": ",".join(validation_failures) or "none"},
    ]
    stats.extend(
        {"metric": f"evidence_{kind}", "value": count}
        for kind, count in sorted(kind_counts.items())
    )
    stats.extend(
        {"metric": f"slot_histogram_{slots}", "value": count}
        for slots, count in sorted(slot_histogram.items())
    )
    write_tsv(analysis / "vtable_split_stats.tsv", ("metric", "value"), stats)
    print(
        f"runs={len(original)}, split={split_runs}, vtables={len(output_rows)}, "
        f"handlers_5slot={len(handler_starts) - len(handler_failures)}/{len(handler_starts)}, "
        f"skipped_nonclean={skipped_count}"
    )
    if validation_failures:
        raise RuntimeError("ground-truth validation failed: " + ",".join(validation_failures))


if __name__ == "__main__":
    main()
