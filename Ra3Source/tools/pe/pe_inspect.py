#!/usr/bin/env python3
"""Static, read-only inspection of a 32-bit PE image.

Writes a JSON summary (headers, sections with entropy, imports, TLS) and
plain-text inventories (MSVC RTTI type names, embedded source paths,
windowed entropy of executable sections) into an output directory.

The input file is only ever opened for reading.
"""

from __future__ import annotations

import argparse
import hashlib
import json
import math
import re
import struct
import sys
from collections import Counter
from datetime import datetime, timezone
from pathlib import Path

SECTION_FLAGS = {
    0x00000020: "code",
    0x00000040: "initialized_data",
    0x00000080: "uninitialized_data",
    0x02000000: "discardable",
    0x10000000: "shared",
    0x20000000: "execute",
    0x40000000: "read",
    0x80000000: "write",
}

DATA_DIRECTORY_NAMES = [
    "export", "import", "resource", "exception", "security", "basereloc",
    "debug", "architecture", "globalptr", "tls", "load_config", "bound_import",
    "iat", "delay_import", "clr", "reserved",
]

ENTROPY_WINDOW = 64 * 1024


def entropy(data: bytes) -> float:
    if not data:
        return 0.0
    counts = Counter(data)
    total = len(data)
    return -sum(c / total * math.log2(c / total) for c in counts.values())


def c_string(data: bytes, offset: int, limit: int = 512) -> str:
    end = data.find(b"\0", offset, offset + limit)
    if end < 0:
        end = offset + limit
    return data[offset:end].decode("latin-1")


class PeImage:
    def __init__(self, data: bytes):
        self.data = data
        if data[:2] != b"MZ":
            raise ValueError("missing MZ signature")
        self.pe_offset = struct.unpack_from("<I", data, 0x3C)[0]
        if data[self.pe_offset:self.pe_offset + 4] != b"PE\0\0":
            raise ValueError("missing PE signature")
        coff = self.pe_offset + 4
        (self.machine, self.section_count, self.timestamp, _, _,
         self.optional_size, self.characteristics) = struct.unpack_from("<HHIIIHH", data, coff)
        opt = coff + 20
        self.optional_magic = struct.unpack_from("<H", data, opt)[0]
        if self.optional_magic != 0x10B:
            raise ValueError(f"not a PE32 image (optional header magic 0x{self.optional_magic:X})")
        self.entry_rva = struct.unpack_from("<I", data, opt + 16)[0]
        self.image_base = struct.unpack_from("<I", data, opt + 28)[0]
        self.section_alignment, self.file_alignment = struct.unpack_from("<II", data, opt + 32)
        self.linker_version = f"{data[opt + 2]}.{data[opt + 3]}"
        self.size_of_image, self.size_of_headers = struct.unpack_from("<II", data, opt + 56)
        self.subsystem, self.dll_characteristics = struct.unpack_from("<HH", data, opt + 68)
        rva_count = struct.unpack_from("<I", data, opt + 92)[0]
        self.directories = []
        for i in range(min(rva_count, 16)):
            rva, size = struct.unpack_from("<II", data, opt + 96 + 8 * i)
            self.directories.append((DATA_DIRECTORY_NAMES[i], rva, size))
        self.sections = []
        table = opt + self.optional_size
        for i in range(self.section_count):
            o = table + 40 * i
            name = data[o:o + 8].split(b"\0", 1)[0].decode("latin-1")
            vsize, va, raw_size, raw_ptr = struct.unpack_from("<IIII", data, o + 8)
            flags = struct.unpack_from("<I", data, o + 36)[0]
            self.sections.append({
                "name": name, "rva": va, "virtual_size": vsize,
                "raw_offset": raw_ptr, "raw_size": raw_size, "flags": flags,
            })

    def section_for_rva(self, rva: int):
        for s in self.sections:
            if s["rva"] <= rva < s["rva"] + max(s["virtual_size"], s["raw_size"]):
                return s
        return None

    def rva_to_offset(self, rva: int):
        s = self.section_for_rva(rva)
        if s is None:
            return rva if rva < self.size_of_headers else None
        delta = rva - s["rva"]
        if delta >= s["raw_size"]:
            return None
        return s["raw_offset"] + delta

    def directory(self, name: str):
        for n, rva, size in self.directories:
            if n == name:
                return rva, size
        return 0, 0

    def imports(self):
        rva, size = self.directory("import")
        if not rva:
            return [], "no import directory"
        base = self.rva_to_offset(rva)
        if base is None:
            return [], f"import directory RVA 0x{rva:X} has no file backing"
        result = []
        for i in range(4096):
            o = base + 20 * i
            oft, _, _, name_rva, ft = struct.unpack_from("<IIIII", self.data, o)
            if not (oft or name_rva or ft):
                break
            name_off = self.rva_to_offset(name_rva)
            dll = c_string(self.data, name_off) if name_off is not None else f"<rva 0x{name_rva:X}>"
            thunk_off = self.rva_to_offset(oft or ft)
            functions = []
            if thunk_off is not None:
                for j in range(65536):
                    value = struct.unpack_from("<I", self.data, thunk_off + 4 * j)[0]
                    if value == 0:
                        break
                    if value & 0x80000000:
                        functions.append(f"#ordinal {value & 0xFFFF}")
                    else:
                        hint_off = self.rva_to_offset(value)
                        functions.append(c_string(self.data, hint_off + 2) if hint_off is not None
                                         else f"<rva 0x{value:X}>")
            result.append({"dll": dll, "iat_rva": ft, "functions": functions})
        return result, None

    def exports(self):
        rva, _ = self.directory("export")
        if not rva:
            return None
        o = self.rva_to_offset(rva)
        if o is None:
            return {"error": f"export directory RVA 0x{rva:X} has no file backing"}
        (_, timestamp, _, _, name_rva, ordinal_base, function_count, name_count,
         functions_rva, names_rva, ordinals_rva) = struct.unpack_from("<IIHHIIIIIII", self.data, o)
        name_off = self.rva_to_offset(name_rva)
        functions_off = self.rva_to_offset(functions_rva)
        names_off = self.rva_to_offset(names_rva)
        ordinals_off = self.rva_to_offset(ordinals_rva)
        names_by_index = {}
        if names_off is not None and ordinals_off is not None:
            for i in range(name_count):
                n_rva = struct.unpack_from("<I", self.data, names_off + 4 * i)[0]
                index = struct.unpack_from("<H", self.data, ordinals_off + 2 * i)[0]
                n_off = self.rva_to_offset(n_rva)
                names_by_index[index] = c_string(self.data, n_off) if n_off is not None else f"<rva 0x{n_rva:X}>"
        entries = []
        if functions_off is not None:
            for i in range(function_count):
                f_rva = struct.unpack_from("<I", self.data, functions_off + 4 * i)[0]
                if f_rva == 0:
                    continue
                section = self.section_for_rva(f_rva)
                entries.append({
                    "ordinal": ordinal_base + i,
                    "name": names_by_index.get(i),
                    "va": f"0x{self.image_base + f_rva:08X}",
                    "section": section["name"] if section else None,
                })
        return {
            "dll_name": c_string(self.data, name_off) if name_off is not None else None,
            "timestamp": timestamp, "function_count": function_count, "name_count": name_count,
            "entries": entries,
        }

    def debug_entries(self):
        rva, size = self.directory("debug")
        o = self.rva_to_offset(rva) if rva else None
        if o is None:
            return []
        result = []
        for i in range(size // 28):
            (_, timestamp, major, minor, kind, data_size, data_rva,
             data_ptr) = struct.unpack_from("<IIHHIIII", self.data, o + 28 * i)
            entry = {"type": kind, "timestamp": timestamp, "size": data_size,
                     "version": f"{major}.{minor}"}
            if kind == 2 and data_ptr and self.data[data_ptr:data_ptr + 4] == b"RSDS":
                guid = self.data[data_ptr + 4:data_ptr + 20]
                age = struct.unpack_from("<I", self.data, data_ptr + 20)[0]
                entry["codeview"] = {
                    "format": "RSDS",
                    "guid": "{%08X-%04X-%04X-%s-%s}" % (
                        struct.unpack_from("<I", guid, 0)[0], struct.unpack_from("<H", guid, 4)[0],
                        struct.unpack_from("<H", guid, 6)[0], guid[8:10].hex().upper(), guid[10:].hex().upper()),
                    "age": age,
                    "pdb_path": c_string(self.data, data_ptr + 24),
                }
            result.append(entry)
        return result

    def tls(self):
        rva, _ = self.directory("tls")
        if not rva:
            return None
        o = self.rva_to_offset(rva)
        if o is None:
            return {"rva": rva, "error": "no file backing"}
        start, end, index_va, callbacks_va = struct.unpack_from("<IIII", self.data, o)
        callbacks = []
        cb_off = self.rva_to_offset(callbacks_va - self.image_base) if callbacks_va else None
        if cb_off is not None:
            for j in range(64):
                va = struct.unpack_from("<I", self.data, cb_off + 4 * j)[0]
                if va == 0:
                    break
                callbacks.append(f"0x{va:08X}")
        return {"rva": rva, "raw_data_va": [f"0x{start:08X}", f"0x{end:08X}"],
                "callbacks_va": f"0x{callbacks_va:08X}", "callbacks": callbacks}


def demangle_type_name(mangled: str) -> str:
    """Best-effort decoding of an MSVC RTTI type name such as '.?AVFoo@Bar@@'.

    Template and other special encodings are returned unchanged.
    """
    body = mangled[4:]
    if "?" in body or not body.endswith("@@"):
        return mangled
    parts = [p for p in body[:-2].split("@") if p]
    return "::".join(reversed(parts))


def scan_rtti(data: bytes):
    names = set()
    for m in re.finditer(rb"\.\?A[VUTW][\x21-\x7e]{1,1024}?@@", data):
        names.add(m.group(0).decode("latin-1"))
    return sorted(names)


SOURCE_PATH = re.compile(rb"[\x20-\x7e]{3,260}?\.(?:cpp|cxx|cc|c|hpp|hxx|h|inl)(?![\w])", re.IGNORECASE)


def scan_source_paths(data: bytes):
    paths = set()
    for m in SOURCE_PATH.finditer(data):
        text = m.group(0).decode("latin-1")
        if "\\" in text or "/" in text:
            start = max(text.rfind("\""), text.rfind("'"), text.rfind(" ") if ":" not in text else -1)
            paths.add(text[start + 1:].strip())
    return sorted(paths, key=str.lower)


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("image", type=Path)
    parser.add_argument("--out", type=Path, required=True, help="output directory")
    args = parser.parse_args()

    data = args.image.read_bytes()
    pe = PeImage(data)
    args.out.mkdir(parents=True, exist_ok=True)

    sections = []
    windowed = {}
    for s in pe.sections:
        raw = data[s["raw_offset"]:s["raw_offset"] + s["raw_size"]]
        flags = [label for bit, label in SECTION_FLAGS.items() if s["flags"] & bit]
        sections.append({
            "name": s["name"],
            "va": f"0x{pe.image_base + s['rva']:08X}",
            "virtual_size": s["virtual_size"],
            "raw_offset": f"0x{s['raw_offset']:X}",
            "raw_size": s["raw_size"],
            "flags": flags,
            "entropy": round(entropy(raw), 3),
        })
        if s["flags"] & 0x20000000 and raw:
            windowed[s["name"]] = [
                (pe.image_base + s["rva"] + i, entropy(raw[i:i + ENTROPY_WINDOW]))
                for i in range(0, len(raw), ENTROPY_WINDOW)
            ]

    imports, import_error = pe.imports()
    entry_section = pe.section_for_rva(pe.entry_rva)
    summary = {
        "file": str(args.image),
        "file_size": len(data),
        "sha256": hashlib.sha256(data).hexdigest(),
        "machine": f"0x{pe.machine:04X}",
        "timestamp_utc": datetime.fromtimestamp(pe.timestamp, timezone.utc).isoformat(),
        "linker_version": pe.linker_version,
        "image_base": f"0x{pe.image_base:08X}",
        "entry_point": f"0x{pe.image_base + pe.entry_rva:08X}",
        "entry_section": entry_section["name"] if entry_section else None,
        "subsystem": pe.subsystem,
        "dll_characteristics": f"0x{pe.dll_characteristics:04X}",
        "size_of_image": pe.size_of_image,
        "data_directories": [
            {"name": n, "va": f"0x{pe.image_base + rva:08X}" if rva else None, "size": size,
             "section": (pe.section_for_rva(rva) or {}).get("name") if rva else None}
            for n, rva, size in pe.directories if rva or size
        ],
        "sections": sections,
        "imports": imports,
        "import_error": import_error,
        "exports": pe.exports(),
        "debug": pe.debug_entries(),
        "tls": pe.tls(),
    }
    (args.out / "pe_summary.json").write_text(json.dumps(summary, indent=2), encoding="utf-8")

    with (args.out / "entropy_windows.txt").open("w", encoding="utf-8") as f:
        f.write(f"# Shannon entropy (bits/byte) per {ENTROPY_WINDOW // 1024} KiB window of executable sections\n")
        for name, rows in windowed.items():
            f.write(f"\n[{name}]\n")
            for va, e in rows:
                f.write(f"0x{va:08X}  {e:5.3f}\n")

    rtti = scan_rtti(data)
    with (args.out / "rtti_types.txt").open("w", encoding="utf-8") as f:
        f.write("# MSVC RTTI type names found in the image: decoded<TAB>mangled\n")
        for mangled in rtti:
            f.write(f"{demangle_type_name(mangled)}\t{mangled}\n")

    paths = scan_source_paths(data)
    with (args.out / "source_paths.txt").open("w", encoding="utf-8") as f:
        f.write("# Strings in the image that look like source file paths\n")
        f.writelines(p + "\n" for p in paths)

    print(json.dumps({
        "sections": [(s["name"], s["entropy"], s["raw_size"]) for s in sections],
        "entry_point": summary["entry_point"], "entry_section": summary["entry_section"],
        "import_dlls": len(imports), "import_functions": sum(len(i["functions"]) for i in imports),
        "import_error": import_error, "rtti_types": len(rtti), "source_paths": len(paths),
    }, indent=1))
    return 0


if __name__ == "__main__":
    sys.exit(main())
