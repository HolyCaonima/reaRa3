"""Regenerate RA3 class/vtable and asset-handler analysis tables.

This is a static, read-only analysis of ra3_1.0.game.  It deliberately accepts
only .text functions marked ``securom == clean`` in function_ledger.tsv.

Outputs:
  docs/analysis/ra3_1.0/asset_handler_vtable.tsv
  docs/analysis/ra3_1.0/asset_handler_methods.tsv
  docs/analysis/ra3_1.0/vclass_annotations.tsv
  docs/analysis/ra3_1.0/classes.tsv
  docs/analysis/ra3_1.0/class_recovery_stats.tsv
"""
from __future__ import annotations

import argparse
import csv
import re
import struct
from collections import Counter, defaultdict
from dataclasses import dataclass
from pathlib import Path

import capstone
from capstone import x86


DEFAULT_EXE = Path(r"D:\Game\Ra3\Game\Data\ra3_1.0.game")
OWNER = "recon/vclass"
HANDLER_SLOT_NAMES = (
    "scalar_deleting_destructor",
    "create_instance",
    "copy_construct_in_place",
    "destroy_instance",
    "resolve_missing_prototype",
)
GHIDRA_DECOMPILED_AND_UNDERSTOOD = {
    0x00AAC690,
    0x004173B0,
    0x004D5C10,
    0x00522910,
    0x007E2E40,
    0x008D69B0,
    0x009AB7C0,
    0x0054A1C0,
    0x00A79730,
    0x00688BB0,
    0x00531250,
    0x004A6560,
    0x005AC8C0,
    0x005AC8E0,
}


def read_tsv(path: Path) -> list[dict[str, str]]:
    with path.open(encoding="utf-8", newline="") as stream:
        return list(csv.DictReader(stream, delimiter="\t"))


def safe(value: object) -> str:
    return str(value).replace("\t", " ").replace("\r", " ").replace("\n", " ")


def write_tsv(path: Path, columns: tuple[str, ...], rows: list[dict[str, object]]) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    with path.open("w", encoding="utf-8", newline="") as stream:
        writer = csv.DictWriter(stream, fieldnames=columns, delimiter="\t", lineterminator="\n")
        writer.writeheader()
        for row in rows:
            writer.writerow({column: safe(row.get(column, "")) for column in columns})


@dataclass(frozen=True)
class Section:
    name: str
    va: int
    virtual_size: int
    raw_offset: int
    raw_size: int
    executable: bool


class PeImage:
    def __init__(self, path: Path) -> None:
        self.path = path
        self.data = path.read_bytes()
        pe = struct.unpack_from("<I", self.data, 0x3C)[0]
        if self.data[pe:pe + 4] != b"PE\0\0":
            raise ValueError(f"{path}: not a PE image")
        count = struct.unpack_from("<H", self.data, pe + 6)[0]
        optional_size = struct.unpack_from("<H", self.data, pe + 20)[0]
        self.base = struct.unpack_from("<I", self.data, pe + 24 + 28)[0]
        table = pe + 24 + optional_size
        self.sections: list[Section] = []
        for index in range(count):
            entry = table + index * 40
            name = self.data[entry:entry + 8].rstrip(b"\0").decode("latin-1")
            virtual_size, rva, raw_size, raw_offset = struct.unpack_from(
                "<IIII", self.data, entry + 8
            )
            characteristics = struct.unpack_from("<I", self.data, entry + 36)[0]
            self.sections.append(
                Section(
                    name,
                    self.base + rva,
                    max(virtual_size, raw_size),
                    raw_offset,
                    raw_size,
                    bool(characteristics & 0x20000000),
                )
            )

    def section(self, va: int) -> Section | None:
        return next((s for s in self.sections if s.va <= va < s.va + s.virtual_size), None)

    def read(self, va: int, size: int) -> bytes | None:
        section = self.section(va)
        if section is None:
            return None
        relative = va - section.va
        if relative + size > section.raw_size:
            return None
        return self.data[section.raw_offset + relative:section.raw_offset + relative + size]

    def u32(self, va: int) -> int | None:
        value = self.read(va, 4)
        return struct.unpack("<I", value)[0] if value is not None else None

    def cstring(self, va: int, limit: int = 512) -> bytes | None:
        value = self.read(va, limit)
        if value is None:
            return None
        return value.split(b"\0", 1)[0]


def demangle_msvc_class(raw: bytes) -> str:
    text = raw.decode("latin-1", errors="replace")
    match = re.fullmatch(r"\.\?A[UV](.+)@@", text)
    if not match:
        return text
    # This covers ordinary namespace-qualified class names. Template names are
    # retained mangled rather than pretending to fully implement MSVC demangling.
    body = match.group(1)
    if "?$" in body or "?" in body:
        return text
    return "::".join(reversed([part for part in body.split("@") if part]))


def valid_col(image: PeImage, vtable: int) -> tuple[int, str] | None:
    """Return (COL, class name) only for a structurally valid x86 MSVC COL."""
    col = image.u32(vtable - 4)
    if col is None:
        return None
    raw = image.read(col, 20)
    if raw is None:
        return None
    signature, offset, ctor_displacement, type_descriptor, hierarchy = struct.unpack("<5I", raw)
    if signature != 0 or offset > 0x100000 or ctor_displacement > 0x100000:
        return None
    name = image.cstring(type_descriptor + 8)
    if not name or not (name.startswith(b".?AV") or name.startswith(b".?AU")):
        return None
    if image.read(hierarchy, 16) is None:
        return None
    return col, demangle_msvc_class(name)


def instructions(image: PeImage, address: int, size: int) -> list[capstone.CsInsn]:
    data = image.read(address, max(1, min(size, 0x10000)))
    if data is None:
        return []
    decoder = capstone.Cs(capstone.CS_ARCH_X86, capstone.CS_MODE_32)
    decoder.detail = True
    return list(decoder.disasm(data, address))


def function_facts(image: PeImage, address: int, size: int) -> tuple[str, str]:
    """Extract conservative layout constants; this does not name semantics."""
    insns = instructions(image, address, size)
    allocation_sizes: set[int] = set()
    field_offsets: set[int] = set()
    immediates: set[int] = set()
    for index, insn in enumerate(insns):
        for operand in insn.operands:
            if operand.type == x86.X86_OP_MEM:
                memory = operand.mem
                if (
                    memory.base not in (x86.X86_REG_ESP, x86.X86_REG_EBP)
                    and 0 < memory.disp <= 0x1000
                ):
                    field_offsets.add(memory.disp)
            elif operand.type == x86.X86_OP_IMM and 0 < operand.imm <= 0x100000:
                immediates.add(operand.imm)
        if insn.mnemonic == "push" and insn.operands and insn.operands[0].type == x86.X86_OP_IMM:
            candidate = insn.operands[0].imm
            for later in insns[index + 1:index + 6]:
                if later.mnemonic == "call" and later.operands[0].type == x86.X86_OP_IMM:
                    if later.operands[0].imm == 0x00416820:
                        allocation_sizes.add(candidate)
                    break
    constants = []
    if allocation_sizes:
        constants.append("allocation_size=" + ",".join(str(value) for value in sorted(allocation_sizes)))
    if field_offsets:
        constants.append(
            "field_offsets=" + ",".join(f"0x{value:X}" for value in sorted(field_offsets))
        )
    # Small immediates are useful evidence for specialized array/record handlers.
    useful = sorted(value for value in immediates if value not in allocation_sizes and value >= 4)
    if useful:
        constants.append("small_constants=" + ",".join(f"0x{value:X}" for value in useful[:16]))
    shape = ";".join(f"{insn.mnemonic} {insn.op_str}" for insn in insns[:12])
    return "; ".join(constants), shape


def recover_vtable_stores(
    image: PeImage,
    ledger: list[dict[str, str]],
    known_vtables: set[int],
) -> tuple[dict[int, list[int]], Counter[tuple[int, int]]]:
    """Find constructor-like ordered vtable writes in clean ledger functions."""
    stores_by_function: dict[int, list[int]] = {}
    edges: Counter[tuple[int, int]] = Counter()
    for row in ledger:
        if row["section"] != ".text" or row["securom"] != "clean":
            continue
        address = int(row["address"], 16)
        size = int(row["size"] or "0")
        if size <= 0:
            continue
        registers: dict[int, int] = {}
        stores: list[int] = []
        for insn in instructions(image, address, size):
            if insn.mnemonic == "mov" and len(insn.operands) == 2:
                destination, source = insn.operands
                if destination.type == x86.X86_OP_REG and source.type == x86.X86_OP_IMM:
                    if source.imm in known_vtables:
                        registers[destination.reg] = source.imm
                    else:
                        registers.pop(destination.reg, None)
                elif destination.type == x86.X86_OP_REG:
                    registers.pop(destination.reg, None)
                if destination.type == x86.X86_OP_MEM:
                    value = None
                    if source.type == x86.X86_OP_IMM:
                        value = source.imm
                    elif source.type == x86.X86_OP_REG:
                        value = registers.get(source.reg)
                    if value in known_vtables and (not stores or stores[-1] != value):
                        stores.append(value)
            # Calls invalidate volatile constant propagation.
            if insn.mnemonic == "call":
                for register in (x86.X86_REG_EAX, x86.X86_REG_ECX, x86.X86_REG_EDX):
                    registers.pop(register, None)
        if stores:
            stores_by_function[address] = stores
            for base, derived in zip(stores, stores[1:]):
                if base != derived:
                    edges[(base, derived)] += 1
    return stores_by_function, edges


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
    ledger = read_tsv(analysis / "function_ledger.tsv")
    ledger_by_address = {int(row["address"], 16): row for row in ledger}
    clean_sizes = {
        int(row["address"], 16): int(row["size"] or "0")
        for row in ledger
        if row["section"] == ".text" and row["securom"] == "clean"
    }
    assets = [row for row in read_tsv(analysis / "asset_types.tsv") if row["handler_vtable"]]
    original_vtables = read_tsv(analysis / "vtables.tsv")
    candidate_vtables = {int(row["vtable"], 16) for row in original_vtables}
    handler_types = {int(row["handler_vtable"], 16): row["typeName"] for row in assets}
    all_vtables = candidate_vtables | set(handler_types)

    # Handler ABI: all registrations are static descriptors whose first five
    # words are executable .text pointers. The next registered address or first
    # non-executable word bounds the table; every registration has exactly five.
    handler_targets: dict[int, tuple[int, ...]] = {}
    for vtable in handler_types:
        targets = tuple(image.u32(vtable + slot * 4) or 0 for slot in range(5))
        if any(
            image.section(target) is None
            or image.section(target).name != ".text"
            or target not in clean_sizes
            for target in targets
        ):
            raise RuntimeError(f"handler 0x{vtable:08X} has a non-clean/unlisted slot")
        handler_targets[vtable] = targets

    slot_counts = [
        Counter(targets[slot] for targets in handler_targets.values()) for slot in range(5)
    ]
    interface_rows: list[dict[str, object]] = []
    for slot, method in enumerate(HANDLER_SLOT_NAMES):
        counts = slot_counts[slot]
        shared_address, shared_count = counts.most_common(1)[0]
        overrides = len(assets) - shared_count
        evidence = {
            0: (
                "162/164 指向 00AAC690；反编译形状为调用基类清理、按 flags&1 释放 this、"
                "ret 4，符合 MSVC scalar deleting destructor"
            ),
            1: (
                "119 个处理器复用 004D5C10（尾调 this->vtable[5]）；20 个复用单槽存储实现，"
                "9 个复用 4 字节分配实现；其余为类型特化构造"
            ),
            2: (
                "119 个复用 00522910（尾调 this->vtable[7]）；21 个为 ret 0x0C 空实现，"
                "9 个复用“若目标非空则把源 4 字节写入目标”；DamageFX 特化以第二参数为 "
                "this、第一参数为复制源调用复制构造，第三参数未使用"
            ),
            3: (
                "119 个复用 007E2E40（尾调 this->vtable[6]）；20 个清空持有槽，"
                "17 个为空实现；其余执行类型特化销毁"
            ),
            4: (
                "164/164 指向 004A6560；比较 MissingPrototype 名称哈希，未命中时从 this+4 "
                "的表调用 004A62E0；推断为缺失原型解析"
            ),
        }[slot]
        interface_rows.append(
            {
                "slot": slot,
                "byte_offset": f"0x{slot * 4:02X}",
                "shared_implementation": f"{shared_address:08X}",
                "shared_handler_count": shared_count,
                "handlers_overriding": overrides,
                "unique_implementations": len(counts),
                "inferred_method_name": method,
                "confidence": "high" if slot in (0, 1, 2, 3) else "medium",
                "evidence": evidence,
            }
        )
    write_tsv(
        analysis / "asset_handler_vtable.tsv",
        (
            "slot",
            "byte_offset",
            "shared_implementation",
            "shared_handler_count",
            "handlers_overriding",
            "unique_implementations",
            "inferred_method_name",
            "confidence",
            "evidence",
        ),
        interface_rows,
    )

    uses: dict[tuple[int, int], list[str]] = defaultdict(list)
    for vtable, targets in handler_targets.items():
        for slot, target in enumerate(targets):
            uses[(slot, target)].append(handler_types[vtable])

    method_rows: list[dict[str, object]] = []
    for asset in sorted(assets, key=lambda row: int(row["typeId"], 16)):
        vtable = int(asset["handler_vtable"], 16)
        for slot, target in enumerate(handler_targets[vtable]):
            constants, shape = function_facts(image, target, clean_sizes[target])
            names = uses[(slot, target)]
            specificity = (
                "type-specific"
                if len(names) == 1
                else ("template-family" if len(names) < len(assets) else "interface-shared")
            )
            method_rows.append(
                {
                    "typeId": asset["typeId"],
                    "typeName": asset["typeName"],
                    "handler_vtable": asset["handler_vtable"],
                    "slot": slot,
                    "method": HANDLER_SLOT_NAMES[slot],
                    "function": f"{target:08X}",
                    "implementation_kind": specificity,
                    "shared_by_handlers": len(names),
                    "constants": constants,
                    "evidence": shape,
                }
            )
    write_tsv(
        analysis / "asset_handler_methods.tsv",
        (
            "typeId",
            "typeName",
            "handler_vtable",
            "slot",
            "method",
            "function",
            "implementation_kind",
            "shared_by_handlers",
            "constants",
            "evidence",
        ),
        method_rows,
    )

    annotation_rows: list[dict[str, object]] = []
    for (slot, target), names in sorted(uses.items(), key=lambda item: item[0][1]):
        method = HANDLER_SLOT_NAMES[slot]
        if len(names) == 1:
            proposed = f"AssetHandler<{names[0]}>::{method}"
        elif len(names) == len(assets):
            proposed = f"AssetHandlerBase::{method}"
        else:
            proposed = f"AssetHandlerTemplate::{method}"
        constants, _ = function_facts(image, target, clean_sizes[target])
        annotation_rows.append(
            {
                "address": f"{target:08X}",
                "status": "reversed" if target in GHIDRA_DECOMPILED_AND_UNDERSTOOD else "",
                "owner": OWNER,
                "module": "asset",
                "notes": (
                    (
                        f"name={proposed}"
                        if target in GHIDRA_DECOMPILED_AND_UNDERSTOOD
                        else f"proposed_name={proposed}"
                    )
                    + f"; slot={slot}; shared_by={len(names)} handlers"
                    + (f"; {constants}" if constants else "")
                    + (
                        "; 已由 Ghidra 反编译并结合调用约定确认语义"
                        if target in GHIDRA_DECOMPILED_AND_UNDERSTOOD
                        else "; 静态槽归类，未逐函数完成 Ghidra 语义反编译，故 status 留空"
                    )
                ),
            }
        )
    # A function can occur in more than one slot; TSV protocol requires one row
    # per address, so merge those conclusions without claiming a false owner.
    merged_annotations: dict[str, dict[str, object]] = {}
    for row in annotation_rows:
        address = str(row["address"])
        if address in merged_annotations:
            merged_annotations[address]["notes"] = (
                str(merged_annotations[address]["notes"]) + " | " + str(row["notes"])
            )
        else:
            merged_annotations[address] = row
    write_tsv(
        analysis / "vclass_annotations.tsv",
        ("address", "status", "owner", "module", "notes"),
        list(merged_annotations.values()),
    )

    stores_by_function, edge_counts = recover_vtable_stores(image, ledger, all_vtables)
    constructors: dict[int, list[int]] = defaultdict(list)
    for function, stores in stores_by_function.items():
        constructors[stores[-1]].append(function)
    incoming: dict[int, list[tuple[int, int]]] = defaultdict(list)
    for (base, derived), count in edge_counts.items():
        incoming[derived].append((base, count))

    rtti = {vtable: valid_col(image, vtable) for vtable in all_vtables}
    class_rows: list[dict[str, object]] = []
    original_by_vtable = {int(row["vtable"], 16): row for row in original_vtables}
    for vtable in sorted(all_vtables):
        original = original_by_vtable.get(vtable)
        rtti_entry = rtti[vtable]
        if vtable in handler_types:
            class_name = f"AssetHandler<{handler_types[vtable]}>"
            name_source = "asset registration"
            slot_count = 5
        elif rtti_entry:
            class_name = rtti_entry[1]
            name_source = "MSVC RTTI"
            slot_count = int(original["slots"]) if original else 0
        else:
            class_name = f"Class_{vtable:08X}"
            name_source = "synthetic"
            slot_count = int(original["slots"]) if original else 0
        bases = sorted(incoming.get(vtable, []), key=lambda item: (-item[1], item[0]))
        base = bases[0][0] if bases else None
        ctor_values = sorted(constructors.get(vtable, []))
        dtor = image.u32(vtable)
        dtor = dtor if dtor in clean_sizes else None
        source_candidates = [
            ledger_by_address[address]["source_file"]
            for address in ctor_values + ([dtor] if dtor else [])
            if ledger_by_address.get(address, {}).get("source_file")
        ]
        evidence_parts = []
        if original:
            evidence_parts.append("RecoverVtables 连续指针段")
        if vtable in handler_types:
            evidence_parts.append("00417220 注册描述符")
        if rtti_entry:
            evidence_parts.append(f"COL={rtti_entry[0]:08X}")
        if bases:
            evidence_parts.append(
                "有序 vptr 写入 "
                + ",".join(f"{candidate:08X}->{vtable:08X}({count})" for candidate, count in bases)
            )
        if len(bases) > 1:
            evidence_parts.append("多基类候选，仅最高出现次数写入 base_vtable")
        class_rows.append(
            {
                "vtable": f"{vtable:08X}",
                "class_name": class_name,
                "name_source": name_source,
                "base_vtable": f"{base:08X}" if base is not None else "",
                "slot_count": slot_count,
                "ctor_addresses": ",".join(f"{value:08X}" for value in ctor_values),
                "dtor_address": f"{dtor:08X}" if dtor is not None else "",
                "source_file": source_candidates[0] if source_candidates else "",
                "evidence": "; ".join(evidence_parts) or "仅有虚表候选地址",
            }
        )
    write_tsv(
        analysis / "classes.tsv",
        (
            "vtable",
            "class_name",
            "name_source",
            "base_vtable",
            "slot_count",
            "ctor_addresses",
            "dtor_address",
            "source_file",
            "evidence",
        ),
        class_rows,
    )

    stats = [
        {"metric": "original_vtable_candidates", "value": len(candidate_vtables)},
        {"metric": "registered_handler_boundaries", "value": len(handler_types)},
        {"metric": "class_rows", "value": len(class_rows)},
        {"metric": "rtti_complete_object_locators", "value": sum(value is not None for value in rtti.values())},
        {"metric": "named_classes", "value": sum(not row["class_name"].startswith("Class_") for row in class_rows)},
        {"metric": "synthetic_classes", "value": sum(row["class_name"].startswith("Class_") for row in class_rows)},
        {"metric": "hierarchy_edges", "value": len(edge_counts)},
        {"metric": "constructor_store_functions", "value": len(stores_by_function)},
        {"metric": "handler_method_rows", "value": len(method_rows)},
        {"metric": "annotation_rows", "value": len(merged_annotations)},
    ]
    write_tsv(analysis / "class_recovery_stats.tsv", ("metric", "value"), stats)
    print(
        f"class rows={len(class_rows)}, named={stats[4]['value']}, "
        f"edges={len(edge_counts)}, annotations={len(merged_annotations)}"
    )


if __name__ == "__main__":
    main()
