"""从 RA3 Mod SDK XSD 推导 BinaryAssetBuilder 布局，并用发行资源流验证。

参考（EA GPL v3 Mod SDK，未复制 schema 内容）：
  Schemas/Base/AssetBase.xsd
  Schemas/Includes/Base.xsd
  Schemas/Includes/Ref.xsd
  Schemas/AssetType*.xsd 及其 include

脚本只读 BIG。推导规则和证据见 docs/design/asset_layout.md。
"""

from __future__ import annotations

import argparse
import csv
import struct
import sys
import xml.etree.ElementTree as ET
from collections import Counter, defaultdict
from dataclasses import dataclass, field
from pathlib import Path
from typing import BinaryIO, Iterable

from build_asset_type_table import (
    ARCHIVES, ENTRY, HEADER, big_entries, read_member, refpack,
)
from asset_name_hash import type_id


XS = "{http://www.w3.org/2001/XMLSchema}"
XAS = "{uri:ea.com:eala:asset:schema}"
U32 = struct.Struct("<I")
MAX_MATERIALIZE = 400 * 1024 * 1024


def local(name: str | None) -> str:
    return (name or "").split(":")[-1]


def align(value: int, boundary: int = 4) -> int:
    return (value + boundary - 1) & ~(boundary - 1)


def occurs(node: ET.Element, key: str, default: int) -> int | None:
    value = node.get(key)
    if value is None:
        return default
    return None if value == "unbounded" else int(value)


@dataclass
class SourceField:
    name: str
    type_name: str = ""
    is_element: bool = False
    min_occurs: int = 1
    max_occurs: int | None = 1
    anonymous: "TypeDef | None" = None
    pipeline_only: bool = False
    by_value: bool = False
    choice: bool = False
    choice_group: int = 0


@dataclass
class TypeDef:
    name: str
    simple: bool = False
    base: str = ""
    tag: str = ""
    enum: bool = False
    list_item: str = ""
    fields: list[SourceField] = field(default_factory=list)
    type_group: str = ""
    runtime_wrapper: str = ""
    is_ref: bool = False
    is_weak_ref: bool = False
    source: str = ""


@dataclass
class LayoutField:
    path: str
    offset: int
    size: int
    kind: str
    type_name: str
    child: "Layout | None" = None
    item: "Layout | None" = None
    alternatives: list["Layout"] = field(default_factory=list)


@dataclass
class Layout:
    name: str
    size: int
    fields: list[LayoutField]
    supported: bool = True
    reason: str = ""


class SchemaModel:
    """宽容地加载 SDK 的全局同命名空间声明。

    SDK 的 include 图包含少量重复声明；BAB 按名字解析，故这里也以首次定义为准，
    同名但结构不同的声明会记录为歧义而不静默覆盖。
    """

    def __init__(self, root: Path):
        self.root = root
        self.types: dict[str, TypeDef] = {}
        self.elements: dict[str, str] = {}
        self.substitutions: dict[str, list[str]] = defaultdict(list)
        self.duplicates: set[str] = set()
        self._anonymous_id = 0
        self._choice_id = 0
        self._layout_cache: dict[tuple[str, bool, bool], Layout] = {}
        self._loading: set[tuple[str, bool, bool]] = set()
        self._load()

    def _load(self) -> None:
        parsed: list[tuple[Path, ET.Element]] = []
        for path in sorted(self.root.rglob("*.xsd")):
            try:
                parsed.append((path, ET.parse(path).getroot()))
            except ET.ParseError as error:
                raise ValueError(f"{path}: XSD 解析失败: {error}") from error

        for path, root in parsed:
            for node in root:
                if node.tag == XS + "simpleType" and node.get("name"):
                    self._add_type(self._parse_simple(node, path))
                elif node.tag == XS + "complexType" and node.get("name"):
                    self._add_type(self._parse_complex(node, path))
                elif node.tag == XS + "element" and node.get("name"):
                    name = node.get("name", "")
                    type_name = local(node.get("type"))
                    if type_name:
                        self.elements.setdefault(name, type_name)
                    substitution = local(node.get("substitutionGroup"))
                    if substitution:
                        self.substitutions[substitution].append(name)

    def _add_type(self, item: TypeDef) -> None:
        old = self.types.get(item.name)
        if old is None:
            self.types[item.name] = item
        elif self._signature(old) != self._signature(item):
            self.duplicates.add(item.name)

    @staticmethod
    def _signature(item: TypeDef) -> tuple:
        return (
            item.simple, item.base, item.tag, item.enum, item.list_item,
            tuple((f.name, f.type_name, f.is_element, f.min_occurs,
                   f.max_occurs, f.pipeline_only, f.by_value, f.choice)
                  for f in item.fields),
        )

    def _parse_simple(self, node: ET.Element, path: Path) -> TypeDef:
        item = TypeDef(
            name=node.get("name", ""), simple=True,
            is_ref=node.get(XAS + "isRef") == "true",
            is_weak_ref=node.get(XAS + "isWeakRef") == "true",
            source=str(path.relative_to(self.root)).replace("\\", "/"),
        )
        restriction = node.find(XS + "restriction")
        listing = node.find(XS + "list")
        if restriction is not None:
            item.base = local(restriction.get("base"))
            item.tag = local(restriction.get(XAS + "tag"))
            item.enum = restriction.find(XS + "enumeration") is not None
            item.is_ref |= restriction.get(XAS + "isRef") == "true"
            item.is_weak_ref |= restriction.get(XAS + "isWeakRef") == "true"
        elif listing is not None:
            item.list_item = local(listing.get("itemType"))
        return item

    def _parse_complex(self, node: ET.Element, path: Path,
                       forced_name: str = "") -> TypeDef:
        name = forced_name or node.get("name", "")
        item = TypeDef(
            name=name,
            type_group=node.get(XAS + "typeGroup", ""),
            runtime_wrapper=node.get(XAS + "runtimeWrapper", ""),
            source=str(path.relative_to(self.root)).replace("\\", "/"),
        )
        content = node.find(XS + "complexContent")
        owner = node
        if content is not None:
            extension = content.find(XS + "extension")
            restriction = content.find(XS + "restriction")
            owner = extension if extension is not None else restriction
            if owner is not None:
                item.base = local(owner.get("base"))
        if owner is None:
            return item

        # BAB 实测顺序是 base -> attributes -> particles，与 XSD 文本中 sequence
        # 位于 attributes 前无关。
        for attribute in owner.findall(XS + "attribute"):
            field = self._parse_field(attribute, path, False)
            if field:
                item.fields.append(field)
        for group_tag in ("sequence", "all", "choice"):
            for particle in owner.findall(XS + group_tag):
                item.fields.extend(self._parse_particle(
                    particle, path, group_tag == "choice"))
        return item

    def _parse_particle(self, node: ET.Element, path: Path,
                        in_choice: bool = False, choice_group: int = 0
                        ) -> list[SourceField]:
        result: list[SourceField] = []
        if node.tag == XS + "choice":
            self._choice_id += 1
            choice_group = self._choice_id
            in_choice = True
        group_min = occurs(node, "minOccurs", 1)
        group_max = occurs(node, "maxOccurs", 1)
        for child in node:
            if child.tag == XS + "element":
                field = self._parse_field(child, path, True)
                if field:
                    field.choice = in_choice
                    field.choice_group = choice_group
                    if group_min == 0:
                        field.min_occurs = 0
                    if group_max != 1:
                        field.max_occurs = group_max
                    result.append(field)
            elif child.tag in (XS + "sequence", XS + "choice", XS + "all"):
                result.extend(self._parse_particle(
                    child, path, in_choice, choice_group))
        return result

    def _parse_field(self, node: ET.Element, path: Path,
                     is_element: bool) -> SourceField | None:
        name = node.get("name") or local(node.get("ref"))
        if not name:
            return None
        type_name = local(node.get("type"))
        if not type_name and node.get("ref"):
            type_name = self.elements.get(local(node.get("ref")), local(node.get("ref")))
        anonymous = None
        complex_node = node.find(XS + "complexType")
        simple_node = node.find(XS + "simpleType")
        if complex_node is not None:
            self._anonymous_id += 1
            anon_name = f"__anonymous_{self._anonymous_id}_{name}"
            anonymous = self._parse_complex(complex_node, path, anon_name)
            self.types[anon_name] = anonymous
            type_name = anon_name
        elif simple_node is not None:
            self._anonymous_id += 1
            anon_name = f"__anonymous_{self._anonymous_id}_{name}"
            simple_node.set("name", anon_name)
            anonymous = self._parse_simple(simple_node, path)
            self.types[anon_name] = anonymous
            type_name = anon_name
        return SourceField(
            name=name,
            type_name=type_name,
            is_element=is_element,
            min_occurs=occurs(node, "minOccurs", 1) or 0,
            max_occurs=occurs(node, "maxOccurs", 1),
            anonymous=anonymous,
            pipeline_only=node.get(XAS + "pipelineOnly") == "true",
            by_value=node.get(XAS + "byValue") == "true",
        )

    def asset_names(self) -> set[str]:
        return {name for name, item in self.types.items()
                if item.type_group and not name.startswith("__")}

    def _simple_kind(self, name: str, seen: set[str] | None = None,
                     tokenized: bool = False) -> tuple[str, int]:
        seen = set() if seen is None else seen
        if name in seen:
            return "unsupported", 0
        seen.add(name)
        # 这两类虽然在 XSD 中最终 restriction 到字符串，BAB 有专用编译器：
        # FileReference 是一个 NUL 结尾字符串指针；DataBlob 是 {pointer,size}。
        if name == "FileReference":
            return "cstring", 4
        if name == "DataBlob":
            return "blob", 8
        # Poid 标识同一编译对象图中的对象。它保留编译期 token；原值不是
        # 实例相对指针，也不列入普通 relo（W3DHierarchy 全量样本只有数组指针）。
        if name == "Poid":
            return "token", 4
        if tokenized and name in ("string", "anyURI"):
            return "token", 4
        builtins = {
            "string": ("string", 8), "anyURI": ("string", 8),
            "boolean": ("scalar", 4), "float": ("scalar", 4),
            "double": ("scalar64", 8), "decimal": ("scalar", 4),
            "byte": ("scalar", 1), "unsignedByte": ("scalar", 1),
            "short": ("scalar", 2), "unsignedShort": ("scalar", 2),
            "int": ("scalar", 4), "unsignedInt": ("scalar", 4),
            "integer": ("scalar", 4), "long": ("scalar64", 8),
            "unsignedLong": ("scalar64", 8),
            "SageBool": ("scalar", 4), "SageReal": ("scalar", 4),
            "SageInt": ("scalar", 4), "SageUnsignedInt": ("scalar", 4),
            "SageUnsignedShort": ("scalar", 2),
            "SageDurationUnsignedInt": ("scalar", 4),
        }
        if name in builtins:
            return builtins[name]
        item = self.types.get(name)
        if item is None:
            # SDK 中常见的位掩码/枚举来自外部基础 schema，发行数据均按 u32。
            return "scalar", 4
        if item.is_ref or item.is_weak_ref or name in (
                "AssetReference", "WeakReference", "Poid"):
            return "reference", 4
        if item.list_item:
            list_item = self.types.get(item.list_item)
            if list_item is not None and list_item.enum:
                # 枚举列表由 BAB 折叠为 32 位 flags，而不是动态数组。
                return "scalar", 4
            return "list-simple", 8
        if item.enum:
            return "enum", 4
        # 这些 tag 的 XML 载体是字符串，但 BAB 在落盘前转换为运行时 32 位值。
        # Angle/Percentage/Time 分别对应弧度、比例和毫秒；StickyString/StringHash
        # 对应运行时 token/hash。不能沿 restriction base 把它们当字符串指针。
        if item.tag in (
                "AngleTag", "PercentageTag", "TimeTag",
                "PositiveNonZeroRealTag", "DurationTag", "VelocityTag",
                "StickyStringTag", "StringHashTag"):
            return "scalar", 4
        if item.base:
            return self._simple_kind(item.base, seen, tokenized)
        # 无 restriction base 的简单类型在 BAB 中是 32 位 token。
        return "scalar", 4

    def layout(self, name: str, root: bool = False,
               tokenized: bool = False) -> Layout:
        key = (name, root, tokenized)
        cached = self._layout_cache.get(key)
        if cached:
            return cached
        if key in self._loading:
            return Layout(name, 0, [], False, "递归内联类型")
        self._loading.add(key)
        try:
            result = self._layout_impl(name, root, tokenized)
            self._layout_cache[key] = result
            return result
        finally:
            self._loading.remove(key)

    def _layout_impl(self, name: str, root: bool,
                     tokenized: bool) -> Layout:
        if name == "FXParticleSystemTemplate" and root:
            # 0x00673A50 直接按这些偏移读取构建块：六个
            # ClientRandomVariable 内联，模块槽则保留指针/多态列表。
            scalar_fields = [
                ("Priority", 4, "enum"), ("IsOneShot", 8, "scalar"),
                ("Shader", 12, "enum"),
                ("SystemLifetime", 32, "scalar"), ("SortLevel", 36, "scalar"),
                ("IsGroundAligned", 40, "scalar"),
                ("IsEmitAboveGroundOnly", 44, "scalar"),
                ("IsParticleUpTowardsEmitter", 48, "scalar"),
                ("UseMaximumHeight", 52, "scalar"),
            ]
            fields = [
                LayoutField(field_name, field_offset, 4, kind, kind)
                for field_name, field_offset, kind in scalar_fields
            ]
            for field_name, field_offset in (
                    ("ParticleTexture", 16), ("Drawable", 20),
                    ("SlaveSystem", 24), ("PerParticleAttachedSystem", 28)):
                fields.append(LayoutField(
                    field_name, field_offset, 4, "reference", "AssetReference"))
            random_layout = self.layout("ClientRandomVariable", False)
            for index, field_name in enumerate((
                    "Lifetime", "Size", "StartSizeRate", "BurstDelay",
                    "BurstCount", "InitialDelay")):
                fields.append(LayoutField(
                    field_name, 0x38 + index * random_layout.size,
                    random_layout.size, "struct", "ClientRandomVariable",
                    child=random_layout))
            for field_name, field_offset, type_name in (
                    ("Alphas", 0x80, "FXParticleAlpha"),
                    ("Colors", 0x84, "FXParticleColor"),
                    ("Wind", 0x88, "FXParticleWind")):
                child = self.layout(type_name, False)
                fields.append(LayoutField(
                    field_name, field_offset, 4, "pointer-struct",
                    type_name, child=child))
            for field_name, field_offset, type_name in (
                    ("Physics", 0x8C, "FXParticlePhysicsModule"),
                    ("Draw", 0x90, "FXParticleDrawModule"),
                    ("Volume", 0x94, "FXParticleVolumeModule"),
                    ("Velocity", 0x98, "FXParticleVelocityModule"),
                    ("Event", 0x9C, "FXParticleEventModule"),
                    ("Update", 0xA4, "FXParticleUpdateModule")):
                child = self.layout(type_name, False)
                fields.append(LayoutField(
                    field_name, field_offset, child.size, "struct",
                    type_name, child=child))
            return Layout(name, 0xA8, fields)
        if name == "W3DAnimation" and root:
            # W3D 管线把源标量/四元数帧压缩为 runtime 的 Timecoded/Delta 通道。
            runtime = self.layout("W3DAnimationRuntime", True)
            return Layout(name, runtime.size, runtime.fields,
                          runtime.supported, runtime.reason)
        if name == "RenderSubObjectReference":
            # 两个 choice 分支都是 AssetReference，BAB 合并为同一个引用槽；
            # 目标 typeId 由该资源的引用目录携带，不另存 discriminant。
            return Layout(name, 4, [
                LayoutField("RenderObject", 0, 4, "reference", "AssetReference"),
            ])
        # AssetTypeAudio.xsd 同时给出源类型 AudioFile 与构建后
        # AudioFileRuntime；注释明确 HeaderData 经 xsd2cpp 配置转换。
        # 发行样本进一步证明运行时字段顺序为下列 32 字节形式。
        if name == "AudioFile" and root:
            return Layout(name, 32, [
                LayoutField("SubtitleStringName", 4, 8, "string", "string"),
                LayoutField("NumberOfSamples", 12, 4, "scalar", "SageInt"),
                LayoutField("SampleRate", 16, 4, "scalar", "SageInt"),
                LayoutField("HeaderData", 20, 8, "blob", "DataBlob"),
                LayoutField("NumberOfChannels", 28, 4, "scalar", "unsignedByte"),
            ])
        if name == "Texture" and root:
            # Texture typeGroup 把源 FileReference 编译为内嵌 DDS blob。
            return Layout(name, 12, [
                LayoutField("File", 4, 8, "blob", "DataBlob"),
            ])
        item = self.types.get(name)
        if item is None:
            kind, size = self._simple_kind(name, tokenized=tokenized)
            return Layout(name, size, [LayoutField(name, 0, size, kind, name)],
                          kind != "unsupported", "" if kind != "unsupported"
                          else "未知类型")
        if item.simple:
            kind, size = self._simple_kind(name, tokenized=tokenized)
            return Layout(name, size, [LayoutField(name, 0, size, kind, name)])

        tokenized = tokenized or (root and item.type_group == "Tokenized")
        fields: list[LayoutField] = []
        offset = 4 if root else 0
        # BaseAssetType 的 XML 标识字段不落盘；根对象的 4 字节槽由运行时类型提供。
        if item.base and item.base not in ("BaseAssetType", "BaseInheritableAsset"):
            base_layout = self.layout(item.base, False, tokenized)
            if not base_layout.supported:
                return Layout(name, 0, [], False, f"基类 {item.base}: {base_layout.reason}")
            for field_item in base_layout.fields:
                fields.append(LayoutField(
                    f"base.{field_item.path}", offset + field_item.offset,
                    field_item.size, field_item.kind, field_item.type_name,
                    field_item.child, field_item.item))
            offset += base_layout.size

        index = 0
        while index < len(item.fields):
            source = item.fields[index]
            if source.pipeline_only or source.name in (
                    "id", "typeHashCode", "buildRule", "inheritFrom",
                    # AudioEventInfo 运行时没有源 schema 的 SubmixSlider 槽；
                    # 后续可选 Range 指针从发行块 +0x50 开始。
                    "SubmixSlider"):
                index += 1
                continue
            offset = align(offset)
            if source.choice:
                group = [source]
                index += 1
                while (index < len(item.fields)
                       and item.fields[index].choice_group == source.choice_group):
                    group.append(item.fields[index])
                    index += 1
                alternatives = [self.layout(choice.type_name, False, tokenized)
                                for choice in group]
                if any(not choice.supported for choice in alternatives):
                    return Layout(name, 0, [], False,
                                  f"choice {source.name}: 未支持分支")
                repeated_choice = (
                    source.max_occurs is None or source.max_occurs != 1)
                if repeated_choice:
                    fields.append(LayoutField(
                        source.name, offset, 8, "polymorphic-list",
                        "|".join(choice.type_name for choice in group),
                        alternatives=alternatives))
                    offset += 8
                else:
                    kinds = [self._simple_kind(
                                choice.type_name, tokenized=tokenized)
                             for choice in group]
                    if all(kind == kinds[0] for kind in kinds):
                        fields.append(LayoutField(
                            source.name, offset, kinds[0][1], kinds[0][0],
                            "|".join(choice.type_name for choice in group)))
                        offset += kinds[0][1]
                    else:
                        fields.append(LayoutField(
                            source.name, offset, 4, "polymorphic-pointer",
                            "|".join(choice.type_name for choice in group),
                            alternatives=alternatives))
                        offset += 4
                continue
            repeated = source.max_occurs is None or source.max_occurs != 1
            if repeated:
                child = self.layout(source.type_name, False, tokenized)
                fields.append(LayoutField(
                    source.name, offset, 8, "list", source.type_name, item=child))
                offset += 8
                index += 1
                continue

            target = self.types.get(source.type_name)
            if target is not None and not target.simple:
                child = self.layout(source.type_name, False, tokenized)
                if not child.supported:
                    return Layout(name, 0, [], False,
                                  f"字段 {source.name}: {child.reason}")
                if (source.is_element and source.min_occurs == 0
                        and not source.by_value):
                    # 可选复合元素由空/非空指针表达；xas:byValue 才强制内联。
                    fields.append(LayoutField(
                        source.name, offset, 4, "pointer-struct",
                        source.type_name, child=child))
                    offset += 4
                else:
                    fields.append(LayoutField(
                        source.name, offset, child.size,
                        "struct", source.type_name, child=child))
                    offset += child.size
            else:
                kind, size = self._simple_kind(
                    source.type_name, tokenized=tokenized)
                fields.append(LayoutField(source.name, offset, size, kind,
                                          source.type_name))
                offset += size
            index += 1
        return Layout(name, align(offset), fields)


@dataclass
class AssetRecord:
    type_name: str
    instance_name: str
    type_id: int
    instance_size: int
    relo_size: int
    imp_size: int
    ref_count: int


class MemberReader:
    """BIG 成员的只读视图；未压缩成员不物化，压缩成员按需解压一次。"""

    def __init__(self, archive: Path, location: tuple[int, int]):
        self.archive = archive
        self.offset, self.stored_size = location
        self._blob: bytes | None = None
        self._stream: BinaryIO = archive.open("rb")
        self._stream.seek(self.offset)
        head = self._stream.read(10)
        self.compressed = (
            len(head) >= 5 and (((head[0] << 8) | head[1]) & 0x3EFF) == 0x10FB)
        if self.compressed:
            pos = 2
            width = 4 if head[0] & 0x80 else 3
            if head[0] & 1:
                pos += width
            self.size = int.from_bytes(head[pos:pos + width], "big")
        else:
            self.size = self.stored_size

    def materialize(self) -> bytes:
        if self._blob is None:
            if self.size > MAX_MATERIALIZE:
                raise MemoryError(
                    f"{self.archive.name}: 解压后 {self.size} 字节，超过内存验证上限")
            self._stream.seek(self.offset)
            raw = self._stream.read(self.stored_size)
            self._blob = refpack(raw)
        return self._blob

    def read_at(self, offset: int, size: int) -> bytes:
        if offset < 0 or size < 0 or offset + size > self.size:
            raise ValueError(f"成员读取越界: {offset}+{size}>{self.size}")
        if self.compressed:
            return self.materialize()[offset:offset + size]
        self._stream.seek(self.offset + offset)
        data = self._stream.read(size)
        if len(data) != size:
            raise ValueError("BIG 成员截断")
        return data

    def close(self) -> None:
        self._stream.close()


def table_values(chunk: bytes, label: str) -> list[int]:
    if not chunk:
        return []
    if len(chunk) % 4:
        raise ValueError(f"{label} 长度不是 4 的倍数")
    values = list(struct.unpack(f"<{len(chunk) // 4}I", chunk))
    if not values or values[-1] != 0xFFFFFFFF:
        raise ValueError(f"{label} 缺少 FFFFFFFF 终止符")
    return values[:-1]


@dataclass
class InstanceResult:
    ok: bool
    reasons: list[str]
    predicted_relo: set[int] = field(default_factory=set)
    predicted_import: set[int] = field(default_factory=set)


class InstanceValidator:
    def __init__(self, data: bytes, layout: Layout,
                 actual_relo: list[int], actual_import: list[int],
                 reference_count: int):
        self.data = data
        self.layout = layout
        self.actual_relo = set(actual_relo)
        self.actual_import = set(actual_import)
        self.reference_count = reference_count
        self.relo: set[int] = set()
        self.imports: set[int] = set()
        self.errors: list[str] = []
        self.visits: set[tuple[str, int]] = set()

    def u32(self, offset: int) -> int:
        if offset < 0 or offset + 4 > len(self.data):
            self.errors.append(f"读取越界@{offset}")
            return 0
        return U32.unpack_from(self.data, offset)[0]

    def pointer(self, field_offset: int, count: int, extent: int,
                path: str) -> int | None:
        target = self.u32(field_offset)
        if target == 0:
            if count:
                self.errors.append(f"{path}: 非零数量使用空指针")
            return None
        self.relo.add(field_offset)
        if target >= len(self.data):
            self.errors.append(f"{path}: 指针 {target} 越界")
            return None
        total = count * extent
        if count and (extent <= 0 or target + total > len(self.data)):
            self.errors.append(f"{path}: 指向数据 {target}+{total} 越界")
            return None
        return target

    def walk(self, layout: Layout, base: int, path: str) -> None:
        visit = (layout.name, base)
        if visit in self.visits:
            return
        self.visits.add(visit)
        if base < 0 or base + layout.size > len(self.data):
            self.errors.append(f"{path}: 对象 {base}+{layout.size} 越界")
            return
        for field_item in layout.fields:
            absolute = base + field_item.offset
            field_path = f"{path}.{field_item.path}" if path else field_item.path
            if field_item.kind == "string":
                length = self.u32(absolute)
                target = self.pointer(absolute + 4, length, 1, field_path)
                if target is not None and target + length < len(self.data):
                    # BAB 字符串长度不含 NUL；空字符串允许共享单个 NUL。
                    if self.data[target + length] != 0:
                        self.errors.append(f"{field_path}: 字符串无 NUL")
            elif field_item.kind == "cstring":
                target = self.pointer(absolute, 0, 1, field_path)
                if target is not None and self.data.find(b"\0", target) < 0:
                    self.errors.append(f"{field_path}: C 字符串无 NUL")
            elif field_item.kind == "blob":
                size = self.u32(absolute + 4)
                self.pointer(absolute, size, 1, field_path)
            elif field_item.kind == "reference":
                ordinal = self.u32(absolute)
                if ordinal:
                    self.imports.add(absolute)
                    if ordinal > self.reference_count:
                        self.errors.append(
                            f"{field_path}: 导入序号 {ordinal}>{self.reference_count}")
            elif field_item.kind == "struct" and field_item.child:
                self.walk(field_item.child, absolute, field_path)
            elif field_item.kind == "pointer-struct" and field_item.child:
                # 空值是 minOccurs=0 的正常表示；非空后由 walk 检查完整子对象范围。
                target = self.pointer(absolute, 0, 1, field_path)
                if target is not None:
                    self.walk(field_item.child, target, field_path)
            elif field_item.kind == "list" and field_item.item:
                count = self.u32(absolute)
                target = self.pointer(
                    absolute + 4, count, field_item.item.size, field_path)
                if target is not None:
                    for index in range(count):
                        self.walk(field_item.item,
                                  target + index * field_item.item.size,
                                  f"{field_path}[{index}]")
            elif field_item.kind == "list-simple":
                # simpleType xs:list 的元素类型由 TypeDef 持有；当前静态字段只需
                # 验证 count/pointer。元素按其 primitive 大小连续存放。
                count = self.u32(absolute)
                self.pointer(absolute + 4, count, 4, field_path)
            elif field_item.kind == "polymorphic-pointer":
                target = self.pointer(absolute, 0, 1, field_path)
                if target is not None:
                    self.walk_polymorphic(field_item, target, field_path)
            elif field_item.kind == "polymorphic-list":
                count = self.u32(absolute)
                pointers = self.pointer(absolute + 4, count, 4, field_path)
                if pointers is not None:
                    for index in range(count):
                        target = self.pointer(
                            pointers + index * 4, 1, 4, f"{field_path}[{index}]")
                        if target is not None:
                            self.walk_polymorphic(
                                field_item, target, f"{field_path}[{index}]")

    def walk_polymorphic(self, field_item: LayoutField, target: int,
                         path: str) -> None:
        token = self.u32(target)
        matches = [layout for layout in field_item.alternatives
                   if type_id(layout.name) == token]
        if len(matches) != 1:
            self.errors.append(f"{path}: 未知多态 token {token:08X}")
            return
        self.walk(matches[0], target, path)

    def run(self) -> InstanceResult:
        # 发行流用零长记录声明“此资源由依赖流提供”；它没有可解释的实例对象，
        # 但仍是合法 manifest 记录。其 relo/imp 必须也为空。
        if not self.data:
            if self.actual_relo or self.actual_import:
                return InstanceResult(False, ["零长外部记录带 relo/imp"])
            return InstanceResult(True, [])
        if not self.layout.supported:
            return InstanceResult(False, [self.layout.reason])
        if len(self.data) < self.layout.size:
            return InstanceResult(
                False, [f"实例 {len(self.data)} 小于根布局 {self.layout.size}"])
        self.walk(self.layout, 0, "")
        missing_relo = self.relo - self.actual_relo
        extra_relo = self.actual_relo - self.relo
        missing_imp = self.imports - self.actual_import
        extra_imp = self.actual_import - self.imports
        if missing_relo:
            self.errors.append(
                "非空指针缺重定位:" + ",".join(map(str, sorted(missing_relo)[:8])))
        if extra_relo:
            self.errors.append(
                "重定位未命中布局:" + ",".join(map(str, sorted(extra_relo)[:8])))
        if missing_imp:
            self.errors.append(
                "引用字段缺导入:" + ",".join(map(str, sorted(missing_imp)[:8])))
        if extra_imp:
            self.errors.append(
                "导入未命中布局:" + ",".join(map(str, sorted(extra_imp)[:8])))
        return InstanceResult(not self.errors, self.errors, self.relo, self.imports)


@dataclass
class Coverage:
    type_id: int
    type_name: str
    total: int = 0
    passed: int = 0
    root_size: int = 0
    min_instance: int | None = None
    max_instance: int = 0
    reasons: Counter[str] = field(default_factory=Counter)


def manifest_records(blob: bytes) -> tuple[list[AssetRecord], list[tuple[int, ...]]]:
    header = HEADER.unpack_from(blob)
    count = header[5]
    refs_size, refman_size, names_size = header[10:13]
    names_start = HEADER.size + count * ENTRY.size + refs_size + refman_size
    names_end = names_start + names_size
    if names_end > len(blob):
        raise ValueError("manifest 名称缓冲越界")
    result: list[AssetRecord] = []
    raw_records: list[tuple[int, ...]] = []
    for index in range(count):
        raw = ENTRY.unpack_from(blob, HEADER.size + index * ENTRY.size)
        name_pos = names_start + raw[6]
        name_end = blob.index(0, name_pos, names_end)
        full_name = blob[name_pos:name_end].decode("utf-8")
        type_name, separator, instance_name = full_name.partition(":")
        if not separator:
            raise ValueError(f"非法资源名 {full_name!r}")
        result.append(AssetRecord(
            type_name, instance_name, raw[0], raw[8], raw[9], raw[10], raw[5]))
        raw_records.append(raw)
    return result, raw_records


def stream_quartets(data_dir: Path) -> Iterable[
        tuple[Path, str, dict[str, tuple[int, int]]]]:
    for archive_name in ARCHIVES:
        archive = data_dir / archive_name
        entries = big_entries(archive)
        for manifest_name in sorted(n for n in entries if n.endswith(".manifest")):
            stem = manifest_name[:-9]
            quartet = {}
            for suffix in (".manifest", ".bin", ".relo", ".imp"):
                name = stem + suffix
                if name not in entries:
                    raise ValueError(f"{archive_name}:{stem} 缺少 {suffix}")
                quartet[suffix] = entries[name]
            yield archive, stem, quartet


def flatten_layout(layout: Layout, prefix: str = "", base: int = 0
                   ) -> Iterable[tuple[str, int, int, str]]:
    for item in layout.fields:
        path = f"{prefix}.{item.path}" if prefix else item.path
        offset = base + item.offset
        yield path, offset, item.size, item.kind
        if item.kind == "struct" and item.child:
            yield from flatten_layout(item.child, path, offset)


def validate_all(args: argparse.Namespace, model: SchemaModel) -> tuple[int, int, int, int]:
    coverage: dict[int, Coverage] = {}
    total_instances = passed_instances = 0
    stream_count = 0
    for archive, stem, quartet in stream_quartets(args.data_dir):
        stream_count += 1
        manifest = read_member(archive, quartet[".manifest"])
        relo_blob = read_member(archive, quartet[".relo"])
        imp_blob = read_member(archive, quartet[".imp"])
        records, _ = manifest_records(manifest)
        bin_reader = MemberReader(archive, quartet[".bin"])
        bin_pos = relo_pos = imp_pos = 4
        for record in records:
            data = bin_reader.read_at(bin_pos, record.instance_size)
            relo_chunk = relo_blob[relo_pos:relo_pos + record.relo_size]
            imp_chunk = imp_blob[imp_pos:imp_pos + record.imp_size]
            item = coverage.setdefault(
                record.type_id, Coverage(record.type_id, record.type_name))
            layout = model.layout(record.type_name, True)
            item.root_size = layout.size
            item.total += 1
            item.min_instance = (
                record.instance_size if item.min_instance is None
                else min(item.min_instance, record.instance_size))
            item.max_instance = max(item.max_instance, record.instance_size)
            try:
                validator = InstanceValidator(
                    data, layout, table_values(relo_chunk, "relo"),
                    table_values(imp_chunk, "imp"), record.ref_count)
                result = validator.run()
            except (ValueError, struct.error) as error:
                result = InstanceResult(False, [str(error)])
            if result.ok:
                item.passed += 1
                passed_instances += 1
            else:
                for reason in result.reasons:
                    # 稳定报告：去掉具体 offset 尾部，保留首个示例及次数。
                    item.reasons[reason] += 1
            total_instances += 1
            bin_pos += record.instance_size
            relo_pos += record.relo_size
            imp_pos += record.imp_size
        if bin_pos != bin_reader.size or relo_pos != len(relo_blob) or imp_pos != len(imp_blob):
            raise ValueError(
                f"{archive.name}:{stem}: 四件套分块总长不一致 "
                f"bin={bin_pos}/{bin_reader.size} relo={relo_pos}/{len(relo_blob)} "
                f"imp={imp_pos}/{len(imp_blob)}")
        bin_reader.close()
        if args.progress:
            print(f"[{stream_count:3d}] {archive.name}:{stem} "
                  f"{total_instances} instances", file=sys.stderr)

    args.coverage.parent.mkdir(parents=True, exist_ok=True)
    with args.coverage.open("w", encoding="utf-8", newline="") as stream:
        fields = ("typeId", "typeName", "instances", "passed", "failed",
                  "fully_validated", "root_size", "min_instance_size",
                  "max_instance_size", "failure_summary")
        writer = csv.DictWriter(stream, fields, delimiter="\t", lineterminator="\n")
        writer.writeheader()
        for type_id in sorted(coverage):
            item = coverage[type_id]
            summary = "; ".join(
                f"{reason} ({count})" for reason, count in item.reasons.most_common(3))
            writer.writerow({
                "typeId": f"{type_id:08X}", "typeName": item.type_name,
                "instances": item.total, "passed": item.passed,
                "failed": item.total - item.passed,
                "fully_validated": "yes" if item.passed == item.total else "no",
                "root_size": item.root_size,
                "min_instance_size": item.min_instance,
                "max_instance_size": item.max_instance,
                "failure_summary": summary,
            })

    args.layouts.parent.mkdir(parents=True, exist_ok=True)
    with args.layouts.open("w", encoding="utf-8", newline="") as stream:
        writer = csv.writer(stream, delimiter="\t", lineterminator="\n")
        writer.writerow(("type", "field_path", "offset", "size", "kind"))
        for item in sorted(coverage.values(), key=lambda value: value.type_name):
            layout = model.layout(item.type_name, True)
            writer.writerow((item.type_name, "$runtimeType", 0, 4, "runtime-token"))
            for path, offset, size, kind in flatten_layout(layout):
                writer.writerow((item.type_name, path, offset, size, kind))

    full_types = sum(item.passed == item.total for item in coverage.values())
    return full_types, len(coverage), passed_instances, total_instances


def default_path(relative: str) -> Path:
    return Path(__file__).resolve().parents[2] / relative


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--schemas", type=Path, required=True,
                        help="RA3 Mod SDK Schemas 目录")
    parser.add_argument("--validate", action="store_true", help="验证全部发行资源")
    parser.add_argument("--data-dir", type=Path,
                        default=Path(r"D:\Game\Ra3\Game\Data"))
    parser.add_argument("--coverage", type=Path,
                        default=default_path(
                            "docs/analysis/ra3_1.0/asset_layout_coverage.tsv"))
    parser.add_argument("--layouts", type=Path,
                        default=default_path(
                            "docs/analysis/ra3_1.0/asset_layouts.tsv"))
    parser.add_argument("--progress", action="store_true")
    args = parser.parse_args()

    model = SchemaModel(args.schemas)
    print(f"schemas={len(model.types)} assetTypes={len(model.asset_names())} "
          f"ambiguous={len(model.duplicates)}")
    if args.validate:
        full, types, passed, total = validate_all(args, model)
        print(f"fully_validated_types={full}/{types} "
              f"validated_instances={passed}/{total}")
    else:
        for name in sorted(model.asset_names()):
            layout = model.layout(name, True)
            print(f"{name}\t{layout.size}\t"
                  f"{'ok' if layout.supported else layout.reason}")


if __name__ == "__main__":
    main()
