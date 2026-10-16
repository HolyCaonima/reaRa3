"""Generate C++17 compiled-asset overlays and reflection from validated XSD layouts."""

from __future__ import annotations

import argparse
import csv
import keyword
import re
from pathlib import Path

from xsd_layout import Layout, LayoutField, SchemaModel


CPP_KEYWORDS = {
    "alignas", "alignof", "and", "and_eq", "asm", "atomic_cancel",
    "atomic_commit", "atomic_noexcept", "auto", "bitand", "bitor", "bool",
    "break", "case", "catch", "char", "char8_t", "char16_t", "char32_t",
    "class", "compl", "concept", "const", "consteval", "constexpr",
    "constinit", "const_cast", "continue", "co_await", "co_return", "co_yield",
    "decltype", "default", "delete", "do", "double", "dynamic_cast", "else",
    "enum", "explicit", "export", "extern", "false", "float", "for",
    "friend", "goto", "if", "inline", "int", "long", "mutable", "namespace",
    "new", "noexcept", "not", "not_eq", "nullptr", "operator", "or",
    "or_eq", "private", "protected", "public", "reflexpr", "register",
    "reinterpret_cast", "requires", "return", "short", "signed", "sizeof",
    "static", "static_assert", "static_cast", "struct", "switch",
    "synchronized", "template", "this", "thread_local", "throw", "true",
    "try", "typedef", "typeid", "typename", "union", "unsigned", "using",
    "virtual", "void", "volatile", "wchar_t", "while", "xor", "xor_eq",
}


def identifier(value: str, fallback: str = "Value") -> str:
    result = re.sub(r"[^A-Za-z0-9_]", "_", value)
    if not result:
        result = fallback
    if result[0].isdigit():
        result = "_" + result
    if result in CPP_KEYWORDS or keyword.iskeyword(result):
        result += "_"
    return result


def unique_identifier(value: str, used: set[str]) -> str:
    base = identifier(value)
    result = base
    suffix = 2
    while result in used:
        result = f"{base}_{suffix}"
        suffix += 1
    used.add(result)
    return result


def validated_roots(path: Path) -> list[tuple[int, str]]:
    result: list[tuple[int, str]] = []
    with path.open(encoding="utf-8", newline="") as stream:
        for row in csv.DictReader(stream, delimiter="\t"):
            if row["fully_validated"] == "yes":
                result.append((int(row["typeId"], 16), row["typeName"]))
    return sorted(result, key=lambda item: item[1])


def field_kind(model: SchemaModel, item: LayoutField) -> str:
    if item.kind == "scalar":
        if item.type_name in ("boolean", "SageBool"):
            return "Boolean"
        if item.type_name in ("float", "SageReal"):
            return "Float"
        if item.type_name in ("byte", "short", "int", "integer", "SageInt"):
            return "Signed"
        return "Unsigned"
    return {
        "scalar64": "Unsigned",
        "enum": "Enum",
        "string": "String",
        "cstring": "FileReference",
        "blob": "Blob",
        "reference": "AssetReference",
        "struct": "Struct",
        "list": "List",
        "list-simple": "SimpleList",
    }.get(item.kind, "Unsigned")


class Generator:
    def __init__(self, model: SchemaModel, roots: list[tuple[int, str]]):
        self.model = model
        self.roots = roots
        self.layouts: dict[tuple[bool, str], Layout] = {}
        self.cpp_names: dict[tuple[bool, str], str] = {}
        self.enums: set[str] = set()
        used: set[str] = set()
        for _, name in roots:
            self.cpp_names[(True, name)] = unique_identifier(name, used)
        for _, name in roots:
            layout = model.layout(name, True)
            self._apply_verified_runtime_order(layout)
            self._collect(layout, True)
        for key in sorted(self.layouts, key=lambda value: (not value[0], value[1])):
            if key not in self.cpp_names:
                suffix = "Value" if not key[0] else ""
                self.cpp_names[key] = unique_identifier(key[1] + suffix, used)

    @staticmethod
    def _apply_verified_runtime_order(layout: Layout) -> None:
        trailing_bool = {
            "OnDemandTextureImage": "Rotated",
            "PackedTextureImage": "Rotated",
            "RadarTerrainImage": "Rotated",
            "W3DCollisionBox": "JoypadPickingOnly",
        }.get(layout.name)
        if trailing_bool is None:
            return
        field = next(item for item in layout.fields if item.path == trailing_bool)
        old_offset = field.offset
        for item in layout.fields:
            if item.offset > old_offset:
                item.offset -= field.size
        field.offset = layout.size - field.size
        layout.fields.remove(field)
        layout.fields.append(field)

    def _collect(self, layout: Layout, root: bool) -> None:
        key = (root, layout.name)
        if key in self.layouts:
            return
        self.layouts[key] = layout
        for item in layout.fields:
            type_def = self.model.types.get(item.type_name)
            if item.kind == "enum":
                self.enums.add(item.type_name)
            elif item.kind == "list-simple" and type_def is not None:
                listed = self.model.types.get(type_def.list_item)
                if listed is not None and listed.enum:
                    self.enums.add(type_def.list_item)
            if item.kind == "struct" and item.child is not None:
                self._collect(item.child, False)
            if item.kind == "list" and item.item is not None:
                if type_def is not None and type_def.enum:
                    self.enums.add(item.type_name)
                self._collect(item.item, False)

    def enum_name(self, name: str) -> str:
        return identifier(name)

    def cpp_type(self, item: LayoutField) -> str:
        if item.kind == "string":
            return "CompiledString"
        if item.kind == "cstring":
            return "FileReference"
        if item.kind == "blob":
            return "DataBlob"
        if item.kind == "reference":
            return "AssetReference"
        if item.kind == "enum":
            return self.enum_name(item.type_name)
        if item.kind == "struct":
            return self.cpp_names[(False, item.child.name)]
        if item.kind in ("list", "list-simple"):
            element = "std::uint8_t"
            if item.kind == "list" and item.item is not None:
                if (False, item.item.name) in self.layouts:
                    element = self.cpp_names[(False, item.item.name)]
            elif item.kind == "list-simple":
                target = self.model.types.get(item.type_name)
                listed = self.model.types.get(target.list_item) if target else None
                if listed is not None and listed.enum:
                    element = self.enum_name(target.list_item)
                elif target is not None:
                    element = "std::uint32_t"
            return f"CompiledList<{element}>"
        if item.kind == "scalar" and item.type_name in ("float", "SageReal"):
            return "float"
        if item.kind == "scalar64" and item.type_name in ("double",):
            return "double"
        signed = item.type_name in ("byte", "short", "int", "integer", "SageInt")
        return ("std::int" if signed else "std::uint") + str(item.size * 8) + "_t"

    def element_info(self, item: LayoutField) -> tuple[str, int]:
        if item.kind == "struct" and item.child is not None:
            return item.child.name, item.child.size
        if item.kind == "list" and item.item is not None:
            return item.item.name, item.item.size
        if item.kind == "list-simple":
            target = self.model.types.get(item.type_name)
            name = target.list_item if target is not None else item.type_name
            return name, 4
        if item.kind == "enum":
            return item.type_name, item.size
        return item.type_name, item.size

    def ordered_structs(self) -> list[tuple[tuple[bool, str], Layout]]:
        result: list[tuple[tuple[bool, str], Layout]] = []
        visited: set[tuple[bool, str]] = set()

        def visit(key: tuple[bool, str]) -> None:
            if key in visited:
                return
            visited.add(key)
            layout = self.layouts[key]
            for item in layout.fields:
                if item.kind == "struct" and item.child is not None:
                    visit((False, item.child.name))
                if item.kind == "list" and item.item is not None:
                    child_key = (False, item.item.name)
                    if child_key in self.layouts:
                        visit(child_key)
            result.append((key, layout))

        for _, name in self.roots:
            visit((True, name))
        return result

    def emit_enum(self, name: str) -> list[str]:
        item = self.model.types[name]
        used: set[str] = set()
        values = [
            (unique_identifier(value_name, used), value)
            for value_name, value in item.enum_values
        ]
        lines = [f"enum class {self.enum_name(name)} : std::uint32_t {{"]
        for value_name, value in values:
            lines.append(f"    {value_name} = 0x{value & 0xFFFFFFFF:08X}u,")
        lines.append("};")
        lines.append(
            f"static_assert(sizeof({self.enum_name(name)}) == 4, "
            f"\"{name} must use the compiled u32 encoding\");")
        lines.append("")
        return lines

    def emit_struct(self, key: tuple[bool, str], layout: Layout) -> list[str]:
        root, _ = key
        cpp = self.cpp_names[key]
        lines = [f"struct {cpp} {{"]
        cursor = 0
        member_names: list[tuple[LayoutField | None, str, int]] = []
        if root:
            lines.append("    RuntimeToken runtimeType;")
            member_names.append((None, "runtimeType", 0))
            cursor = 4
        used = {"runtimeType"} if root else set()
        padding = 0
        for item in layout.fields:
            if item.offset > cursor:
                lines.append(
                    f"    std::uint8_t padding_{padding}[{item.offset - cursor}]{{}};")
                padding += 1
            member = unique_identifier(item.path.replace(".", "_"), used)
            lines.append(f"    {self.cpp_type(item)} {member};")
            member_names.append((item, member, item.offset))
            cursor = item.offset + item.size
        if layout.size > cursor:
            lines.append(f"    std::uint8_t padding_{padding}[{layout.size - cursor}]{{}};")
        lines.append("};")
        lines.append(f"static_assert(sizeof({cpp}) == {layout.size}, \"compiled size mismatch\");")
        for _, member, offset in member_names:
            lines.append(
                f"static_assert(offsetof({cpp}, {member}) == {offset}, "
                f"\"compiled offset mismatch\");")
        lines.append("")
        return lines

    def emit(self) -> str:
        sources = sorted({
            item.source for item in self.model.types.values()
            if item.source and (
                item.name in {name for _, name in self.layouts} or
                item.name in self.enums)
        })
        lines = [
            "// Generated by tools/analysis/gen_asset_types.py from " +
            ", ".join(sources) + "; do not edit.",
            "#pragma once",
            "",
            "#include <ra3/asset/TypedAsset.h>",
            "",
            "#include <array>",
            "#include <cstddef>",
            "#include <cstdint>",
            "",
            "namespace ra3::asset::types {",
            "",
        ]
        for name in sorted(self.enums):
            lines.extend(self.emit_enum(name))
        lines.append("#pragma pack(push, 1)")
        for key, layout in self.ordered_structs():
            if not layout.supported or layout.size == 0:
                raise ValueError(f"validated type {layout.name} has no supported layout")
            lines.extend(self.emit_struct(key, layout))
        lines.extend(["#pragma pack(pop)", "", "}  // namespace ra3::asset::types", ""])

        lines.append("namespace ra3::asset {")
        for type_id, name in self.roots:
            cpp = self.cpp_names[(True, name)]
            lines.extend([
                f"template <> struct AssetTypeTraits<types::{cpp}> {{",
                f"    static constexpr std::uint32_t typeId = 0x{type_id:08X}u;",
                f'    static constexpr std::string_view name = "{name}";',
                "};",
            ])
        lines.extend(["", "namespace generated {", ""])

        for name in sorted(self.enums):
            item = self.model.types[name]
            array_name = "kEnumValues_" + identifier(name)
            lines.append(
                f"inline constexpr std::array<EnumValueInfo, {len(item.enum_values)}> "
                f"{array_name} = {{{{")
            for value_name, value in item.enum_values:
                escaped = value_name.replace("\\", "\\\\").replace('"', '\\"')
                lines.append(f'    {{"{escaped}", 0x{value & 0xFFFFFFFF:08X}u}},')
            lines.extend(["}};", ""])

        metadata_names: dict[tuple[bool, str], str] = {}
        for index, (key, layout) in enumerate(self.ordered_structs()):
            array_name = f"kFields_{index}_{identifier(layout.name)}"
            metadata_names[key] = array_name
            count = len(layout.fields) + (1 if key[0] else 0)
            lines.append(
                f"inline constexpr std::array<FieldInfo, {count}> {array_name} = {{{{")
            if key[0]:
                lines.append(
                    '    {"$runtimeType", 0, 4, FieldKind::RuntimeToken, "", 4},')
            for field_item in layout.fields:
                element_name, element_size = self.element_info(field_item)
                escaped_name = field_item.path.replace("\\", "\\\\").replace('"', '\\"')
                escaped_type = element_name.replace("\\", "\\\\").replace('"', '\\"')
                lines.append(
                    f'    {{"{escaped_name}", {field_item.offset}, {field_item.size}, '
                    f'FieldKind::{field_kind(self.model, field_item)}, '
                    f'"{escaped_type}", {element_size}}},')
            lines.extend(["}};", ""])

        ordered = self.ordered_structs()
        root_ids = {name: type_id for type_id, name in self.roots}
        lines.append(
            f"inline constexpr std::array<TypeInfo, {len(ordered)}> kAllTypes = {{{{")
        for key, layout in ordered:
            type_id = root_ids.get(layout.name, 0) if key[0] else 0
            array_name = metadata_names[key]
            lines.append(
                f'    {{"{layout.name}", 0x{type_id:08X}u, {layout.size}, '
                f"{array_name}.data(), {array_name}.size()}},")
        lines.extend(["}};", ""])

        root_indexes = [
            next(index for index, (key, _) in enumerate(ordered)
                 if key == (True, name))
            for _, name in self.roots
        ]
        lines.append(
            f"inline constexpr std::array<TypeInfo, {len(self.roots)}> kTypes = {{{{")
        for index in root_indexes:
            lines.append(f"    kAllTypes[{index}],")
        lines.extend(["}};", ""])

        lines.append(
            f"inline constexpr std::array<EnumInfo, {len(self.enums)}> kEnums = {{{{")
        for name in sorted(self.enums):
            array_name = "kEnumValues_" + identifier(name)
            lines.append(
                f'    {{"{name}", {array_name}.data(), {array_name}.size()}},')
        lines.extend([
            "}};",
            "",
            "}  // namespace generated",
            "}  // namespace ra3::asset",
            "",
        ])
        return "\n".join(lines)


def default_path(relative: str) -> Path:
    return Path(__file__).resolve().parents[2] / relative


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--schemas", type=Path, required=True)
    parser.add_argument(
        "--coverage", type=Path,
        default=default_path("docs/analysis/ra3_1.0/asset_layout_coverage.tsv"))
    parser.add_argument(
        "--output", type=Path,
        default=default_path(
            "src/asset/include/ra3/asset/types/GeneratedTypes.h"))
    parser.add_argument("--check", action="store_true")
    args = parser.parse_args()

    model = SchemaModel(args.schemas)
    roots = validated_roots(args.coverage)
    generator = Generator(model, roots)
    text = generator.emit()
    if args.check:
        if not args.output.exists() or args.output.read_text(encoding="utf-8") != text:
            raise SystemExit(f"generated file is stale: {args.output}")
    else:
        args.output.parent.mkdir(parents=True, exist_ok=True)
        args.output.write_text(text, encoding="utf-8", newline="\n")
    print(
        f"types={len(roots)} structs={len(generator.layouts)} "
        f"enums={len(generator.enums)} bytes={len(text.encode('utf-8'))}")


if __name__ == "__main__":
    main()
