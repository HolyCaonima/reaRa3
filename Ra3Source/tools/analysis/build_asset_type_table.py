"""Build the RA3 asset-type catalog from manifests and Ghidra exports.

This script only reads BIG archives.  It intentionally contains its own small
BIG4/RefPack reader so the generated table does not depend on a build tree.
"""

from __future__ import annotations

import argparse
import csv
import struct
from collections import defaultdict
from dataclasses import dataclass, field
from pathlib import Path


ARCHIVES = (
    "English.big", "EnglishAudio.big", "EnglishMovieAudio.big", "Apt.big",
    "GlobalStream.big", "Libraries.big", "MapsCampaign.big",
    "MapsMultiplayer.big", "MapsTutorial.big", "Misc.big", "Shaders.big",
    "StaticStream.big", "Terrain.big", "WBData.big",
)
HEADER = struct.Struct("<BBHIIIIIIIIIII")
ENTRY = struct.Struct("<12I")


def refpack(src: bytes) -> bytes:
    if len(src) < 5 or (((src[0] << 8) | src[1]) & 0x3EFF) != 0x10FB:
        return src
    pos = 2
    width = 4 if src[0] & 0x80 else 3
    if src[0] & 1:
        pos += width
    expected = int.from_bytes(src[pos:pos + width], "big")
    pos += width
    out = bytearray()
    while pos < len(src):
        c0 = src[pos]
        if c0 < 0x80:
            c1 = src[pos + 1]
            pos += 2
            plain, count = c0 & 3, ((c0 >> 2) & 7) + 3
            distance = ((c0 & 0x60) << 3) + c1 + 1
        elif c0 < 0xC0:
            c1, c2 = src[pos + 1:pos + 3]
            pos += 3
            plain, count = (c1 >> 6) & 3, (c0 & 0x3F) + 4
            distance = ((c1 & 0x3F) << 8) + c2 + 1
        elif c0 < 0xE0:
            c1, c2, c3 = src[pos + 1:pos + 4]
            pos += 4
            plain, count = c0 & 3, ((c0 & 0x0C) << 6) + c3 + 5
            distance = ((c0 & 0x10) << 12) + (c1 << 8) + c2 + 1
        elif c0 < 0xFC:
            pos += 1
            plain, count, distance = ((c0 & 0x1F) << 2) + 4, 0, 0
        else:
            pos += 1
            plain = c0 & 3
            out += src[pos:pos + plain]
            break
        out += src[pos:pos + plain]
        pos += plain
        start = len(out) - distance
        for i in range(count):
            out.append(out[start + i])
    if len(out) != expected:
        raise ValueError(f"RefPack size mismatch: {len(out)} != {expected}")
    return bytes(out)


def big_entries(path: Path) -> dict[str, tuple[int, int]]:
    with path.open("rb") as stream:
        header = stream.read(16)
        if header[:4] not in (b"BIG4", b"BIGF"):
            raise ValueError(f"{path}: unsupported BIG signature {header[:4]!r}")
        count, directory_end = struct.unpack(">II", header[8:16])
        directory = stream.read(directory_end - 16)
    result: dict[str, tuple[int, int]] = {}
    pos = 0
    for _ in range(count):
        offset, size = struct.unpack_from(">II", directory, pos)
        pos += 8
        end = directory.index(0, pos)
        name = directory[pos:end].decode("latin-1").replace("\\", "/").lower()
        result[name] = (offset, size)
        pos = end + 1
    return result


def read_member(path: Path, location: tuple[int, int]) -> bytes:
    offset, size = location
    with path.open("rb") as stream:
        stream.seek(offset)
        raw = stream.read(size)
    return refpack(raw)


@dataclass
class DataType:
    names: set[str] = field(default_factory=set)
    hashes: set[int] = field(default_factory=set)
    count: int = 0
    streams: set[str] = field(default_factory=set)
    min_size: int | None = None
    max_size: int | None = None


def collect_data(data_dir: Path, pairs_path: Path | None) -> dict[int, DataType]:
    types: dict[int, DataType] = defaultdict(DataType)
    pair_file = None
    pair_writer = None
    if pairs_path:
        pairs_path.parent.mkdir(parents=True, exist_ok=True)
        pair_file = pairs_path.open("w", encoding="utf-8", newline="")
        pair_writer = csv.writer(pair_file, delimiter="\t", lineterminator="\n")
        pair_writer.writerow(("typeId", "typeName", "instanceId", "instanceName",
                              "instanceHash", "stream"))
    try:
        for archive_name in ARCHIVES:
            archive = data_dir / archive_name
            entries = big_entries(archive)
            for member_name in sorted(n for n in entries if n.endswith(".manifest")):
                blob = read_member(archive, entries[member_name])
                header = HEADER.unpack_from(blob)
                count = header[5]
                refs_size, refman_size, names_size = header[10:13]
                names_start = (HEADER.size + count * ENTRY.size + refs_size +
                               refman_size)
                names_end = names_start + names_size
                if names_end > len(blob):
                    raise ValueError(f"{archive_name}:{member_name}: names out of range")
                stream_name = f"{archive_name}:{member_name[:-9]}"
                for index in range(count):
                    record = ENTRY.unpack_from(blob, HEADER.size + index * ENTRY.size)
                    type_id, instance_id, type_hash, instance_hash = record[:4]
                    name_offset, instance_size = record[6], record[8]
                    absolute = names_start + name_offset
                    end = blob.index(0, absolute, names_end)
                    full_name = blob[absolute:end].decode("utf-8")
                    type_name, separator, instance_name = full_name.partition(":")
                    if not separator:
                        raise ValueError(f"{stream_name}: malformed asset name {full_name!r}")
                    item = types[type_id]
                    item.names.add(type_name)
                    item.hashes.add(type_hash)
                    item.count += 1
                    item.streams.add(stream_name)
                    item.min_size = instance_size if item.min_size is None else min(
                        item.min_size, instance_size)
                    item.max_size = instance_size if item.max_size is None else max(
                        item.max_size, instance_size)
                    if pair_writer:
                        pair_writer.writerow((f"{type_id:08X}", type_name,
                                              f"{instance_id:08X}", instance_name,
                                              f"{instance_hash:08X}", stream_name))
    finally:
        if pair_file:
            pair_file.close()
    return types


def read_tsv(path: Path) -> list[dict[str, str]]:
    if not path.exists():
        return []
    with path.open(encoding="utf-8", newline="") as stream:
        return list(csv.DictReader(stream, delimiter="\t"))


def one(values: set[str]) -> str:
    return ";".join(sorted(v for v in values if v))


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("--data-dir", type=Path, required=True)
    parser.add_argument("--program", type=Path, required=True)
    parser.add_argument("--factory", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--pairs", type=Path,
                        help="optional full name/id verification corpus")
    args = parser.parse_args()

    data = collect_data(args.data_dir, args.pairs)
    program: dict[int, list[dict[str, str]]] = defaultdict(list)
    for row in read_tsv(args.program):
        program[int(row["typeId"], 16)].append(row)
    factory: dict[int, set[str]] = defaultdict(set)
    for row in read_tsv(args.factory):
        factory[int(row["typeId"], 16)].add(row["alloc_size"])

    args.output.parent.mkdir(parents=True, exist_ok=True)
    fields = ("typeId", "typeName", "typeHash_data", "typeHash_program",
              "handler_vtable", "registrar_address", "factory_alloc_size",
              "asset_count", "streams", "min_instance_size", "max_instance_size")
    with args.output.open("w", encoding="utf-8", newline="") as stream:
        writer = csv.DictWriter(stream, fields, delimiter="\t", lineterminator="\n")
        writer.writeheader()
        for type_id in sorted(set(data) | set(program) | set(factory)):
            datum = data.get(type_id)
            registrations = program.get(type_id, [])
            writer.writerow({
                "typeId": f"{type_id:08X}",
                "typeName": one(datum.names) if datum else "",
                "typeHash_data": one({f"{v:08X}" for v in datum.hashes}) if datum else "",
                "typeHash_program": one({r["typeHash"] for r in registrations}),
                "handler_vtable": one({r["handler_vtable"] for r in registrations}),
                "registrar_address": one({r["registrar_address"] for r in registrations}),
                "factory_alloc_size": one(factory.get(type_id, set())),
                "asset_count": str(datum.count) if datum else "",
                "streams": str(len(datum.streams)) if datum else "",
                "min_instance_size": str(datum.min_size) if datum else "",
                "max_instance_size": str(datum.max_size) if datum else "",
            })


if __name__ == "__main__":
    main()
