"""Static acceptance checks for one reconstruction branch before it is merged.

Reads everything through git (never the working tree) and the executable
read-only, then reports what a controller would otherwise check by hand:

- shared files the branch must not touch, and files it adds that main already has;
- new hook targets: unique keys, explicitOnly count, expected bytes against the
  binary, stolen prefix made of whole movable instructions, no branch back into
  the stolen range, a shadow record call per key, and hook tables in design docs
  that quote bytes different from the code;
- branch annotations: format, owner, ledger membership, protection class, and
  claims that overlap main or another open branch.

    python accept_batch.py recon/net5d [--base REF] [--main main] [--owner NAME] [--exe PATH]

Exit status 1 when any error is reported.
"""

from __future__ import annotations

import argparse
import collections
import csv
import io
import re
import subprocess
import sys
from pathlib import Path

import capstone

sys.path.insert(0, str(Path(__file__).resolve().parent))
from disasm import DEFAULT_EXE, Image, position_dependent  # noqa: E402

DOCS = "Ra3Source/docs/analysis/ra3_1.0"
LEDGER = f"{DOCS}/function_ledger.tsv"
MAIN_ANNOTATIONS = f"{DOCS}/ledger_annotations.tsv"
ADDITIONS = f"{DOCS}/function_additions.tsv"
TARGETS_DIR = "Ra3Source/src/hook/dll/targets/"
CORE_TARGETS = "Ra3Source/src/hook/dll/Targets.cpp"
SHARED_FILES = {
    LEDGER,
    MAIN_ANNOTATIONS,
    f"{DOCS}/securom_sites.tsv",
    "Ra3Source/docs/HANDOFF.md",
    "Ra3Source/docs/RECONSTRUCTION.md",
    "Ra3Source/docs/design/injection.md",
}
ANNOTATION_COLUMNS = ["address", "status", "owner", "module", "notes"]
IMAGE_BASE = 0x400000

ENTRY = re.compile(
    r'\{\s*"([^"]+)"\s*,\s*0x([0-9A-Fa-f]+)U?\s*,\s*\{([^{}]*)\}\s*,\s*(\d+)U?\s*,'
    r'\s*([^,]+?)\s*,\s*&?([\w:]+)\s*(?:,\s*(true|false))?\s*(?:,\s*\{([^{}]*)\})?\s*,?\s*\}',
    re.S,
)
RECORD = re.compile(r'\brecord\w*\(\s*"([^"]+)"')


class Report:
    def __init__(self) -> None:
        self.errors: list[str] = []
        self.warnings: list[str] = []

    def error(self, text: str) -> None:
        self.errors.append(text)

    def warn(self, text: str) -> None:
        self.warnings.append(text)


def git(*args: str) -> str:
    return subprocess.run(["git", *args], check=True, capture_output=True,
                          encoding="utf-8", errors="replace").stdout


def show(ref: str, path: str) -> str | None:
    result = subprocess.run(["git", "show", f"{ref}:{path}"], capture_output=True,
                            encoding="utf-8", errors="replace")
    return result.stdout if result.returncode == 0 else None


def tree_files(ref: str, prefix: str = "") -> list[str]:
    args = ["ls-tree", "-r", "--name-only", ref]
    if prefix:
        args.append(prefix)
    return git(*args).splitlines()


def parse_targets(text: str) -> list[dict]:
    entries = []
    for m in ENTRY.finditer(text):
        rel32 = m.group(8)
        entries.append({
            "key": m.group(1),
            "rva": int(m.group(2), 16),
            "expected": bytes(int(b, 16) for b in re.findall(r"0x([0-9A-Fa-f]{1,2})", m.group(3))),
            "stolen": int(m.group(4)),
            "state": m.group(6),
            "explicit": m.group(7) == "true",
            "rel32": [int(x, 0) for x in re.findall(r"0x[0-9A-Fa-f]+|\d+", rel32)] if rel32 else [],
        })
    return entries


def all_targets(ref: str) -> dict[str, list[dict]]:
    paths = [CORE_TARGETS] + [p for p in tree_files(ref, TARGETS_DIR) if p.endswith(".cpp")]
    return {path: parse_targets(show(ref, path) or "") for path in paths}


def read_tsv(text: str) -> list[dict]:
    return list(csv.DictReader(io.StringIO(text), delimiter="\t"))


def check_prologue(image: Image, entry: dict, size: int, report: Report) -> None:
    key, va = entry["key"], entry["rva"] + IMAGE_BASE
    actual = image.read(va, len(entry["expected"]))
    if actual != entry["expected"]:
        report.error(f"hook {key}: expected bytes differ from binary at 0x{va:08X} "
                     f"(code {entry['expected'].hex(' ')}, binary {actual.hex(' ')})")
        return
    md = capstone.Cs(capstone.CS_ARCH_X86, capstone.CS_MODE_32)
    md.detail = True
    offset = 0
    for insn in md.disasm(image.read(va, 32), va):
        if offset >= entry["stolen"]:
            break
        allowed_rel32 = insn.size == 5 and insn.bytes[0] in (0xE8, 0xE9) and offset in entry["rel32"]
        terminal = insn.group(capstone.CS_GRP_RET) or insn.mnemonic == "int3"
        if position_dependent(insn) and not allowed_rel32 and not terminal:
            report.error(f"hook {key}: stolen prefix has position-dependent "
                         f"'{insn.mnemonic} {insn.op_str}' at +{offset}")
        offset += insn.size
    if offset != entry["stolen"] or entry["stolen"] < 5:
        report.error(f"hook {key}: stolen length {entry['stolen']} is not a whole-instruction "
                     f"prefix of at least 5 bytes (boundary {offset})")
    if size <= 0:
        return
    for insn in md.disasm(image.read(va, size), va):
        if not (insn.group(capstone.CS_GRP_JUMP) or insn.group(capstone.CS_GRP_CALL)):
            continue
        for op in insn.operands:
            if op.type == capstone.x86.X86_OP_IMM and va < op.imm < va + entry["stolen"]:
                report.error(f"hook {key}: 0x{insn.address:08X} branches into the stolen "
                             f"range (0x{op.imm:08X})")


def check_doc_bytes(head: str, docs: list[str], entries: dict[str, dict], report: Report) -> None:
    """Hook table rows in design docs must quote the code's bytes, VA and explicitOnly."""
    mentioned: set[str] = set()
    for path in docs:
        for line in (show(head, path) or "").splitlines():
            if not line.lstrip().startswith("|"):
                continue
            cells = [re.sub(r"[`\s,]", "", c) for c in line.strip().strip("|").split("|")]
            for key, entry in entries.items():
                if key not in cells:
                    continue
                mentioned.add(key)
                va = entry["rva"] + IMAGE_BASE
                addresses = {int(c, 16) for c in cells if re.fullmatch(r"(?:0x)?[0-9A-Fa-f]{8}", c)}
                if addresses and va not in addresses and entry["rva"] not in addresses:
                    report.error(f"doc {path}: row for {key} quotes "
                                 f"{', '.join(f'0x{a:08X}' for a in sorted(addresses))}, code has 0x{va:08X}")
                for cell in cells:
                    hexits = cell[2:] if cell.lower().startswith("0x") else cell
                    if len(hexits) >= 16 and len(hexits) % 2 == 0 and re.fullmatch(r"[0-9A-Fa-f]+", hexits):
                        quoted = bytes.fromhex(hexits)
                        if quoted[:len(entry["expected"])] != entry["expected"][:len(quoted)]:
                            report.error(f"doc {path}: bytes quoted for {key} differ from code "
                                         f"({quoted.hex(' ')} vs {entry['expected'].hex(' ')})")
                flags = [c for c in cells if c in ("true", "false")]
                if len(flags) == 1 and (flags[0] == "true") != entry["explicit"]:
                    report.error(f"doc {path}: row for {key} says explicitOnly={flags[0]}, "
                                 f"code has {'true' if entry['explicit'] else 'false'}")
    for key in sorted(set(entries) - mentioned):
        report.error(f"hook {key} is not listed in any changed design doc table")


def open_branches(main: str, head: str) -> list[str]:
    branches = []
    for line in git("worktree", "list", "--porcelain").splitlines():
        if line.startswith("branch refs/heads/"):
            name = line[len("branch refs/heads/"):]
            if name not in (main, head) and git("rev-list", "--count", f"{main}..{name}").strip() != "0":
                branches.append(name)
    return branches


def claimed_rows(ref: str, base: str, head: str) -> dict[str, str]:
    """Rows a sibling claims in annotation files that differ from the checked branch."""
    claims = {}
    for path, status in changed_files(base, ref):
        if status != "D" and path.startswith(DOCS) and path.endswith("_annotations.tsv") \
                and path != MAIN_ANNOTATIONS:
            text = show(ref, path) or ""
            if text == show(head, path) or text == show("main", path):
                continue
            for row in read_tsv(text):
                if (row.get("status") or "").strip():
                    claims[row["address"].strip().lower()] = f"{ref}:{Path(path).name}"
    return claims


LIFTED_MARK = re.compile(r"stk\[--sp\]|wr\(env, frame|doTest\(|\bFlags fl\b|goto L_00[0-9a-fA-F]{6}"
                         r"|std::uint32_t ecx = addressOf\(self\)")
LIFTED_HDR = re.compile(r"^\s*//\s*0x([0-9A-Fa-f]{8})\b")
LIFTED_INS = re.compile(r"^\s*//\s*([0-9a-f]{8})\s+[a-z]")
CONTAINER = re.compile(r"^\s*(namespace\b|extern\s+\"C\"|(struct|class|union|enum)\b[^;(]*$)")


def lifted_bodies(text: str) -> list[str]:
    """VAs (or line tags) of function bodies that emulate registers/stack/flags instead of reconstructing."""
    found: list[str] = []
    stack: list[str] = []
    start = None
    lifted, first_ins, last_hdr, prev = False, None, None, ""
    for i, line in enumerate(text.splitlines()):
        code = line.split("//")[0]
        m = LIFTED_HDR.match(line)
        if m and start is None:
            last_hdr = m.group(1).lower()
        if start is not None:
            lifted = lifted or bool(LIFTED_MARK.search(line))
            mi = LIFTED_INS.match(line)
            if mi and first_ins is None:
                first_ins = mi.group(1)
        for ch in code:
            if ch == "{":
                head = code if code.strip() != "{" else prev
                kind = "c" if CONTAINER.match(head) else "b"
                if start is None and kind == "b" and all(k == "c" for k in stack):
                    start, lifted, first_ins = i, bool(LIFTED_MARK.search(line)), None
                    stack.append("f")
                else:
                    stack.append(kind)
            elif ch == "}" and stack and stack.pop() == "f":
                if lifted:
                    found.append(first_ins or last_hdr or f"line{start + 1}")
                start, last_hdr = None, None
        if code.strip():
            prev = code
    return found


def changed_files(base: str, head: str) -> list[tuple[str, str]]:
    out = []
    for line in git("diff", "--name-status", "--no-renames", base, head).splitlines():
        status, path = line.split("\t", 1)
        out.append((path, status[0]))
    return out


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("head")
    parser.add_argument("--base")
    parser.add_argument("--main", default="main")
    parser.add_argument("--exe", type=Path, default=DEFAULT_EXE)
    parser.add_argument("--owner", help="annotation owner (default recon/<branch suffix>)")
    args = parser.parse_args()

    head, main_ref = args.head, args.main
    base = args.base or git("merge-base", head, main_ref).strip()
    report = Report()
    changes = changed_files(base, head)
    changed = {path: status for path, status in changes}
    image = Image(args.exe)

    for path, status in changes:
        if path in SHARED_FILES:
            report.error(f"branch edits shared file {path}")
        if status == "A" and show(main_ref, path) is not None and show(main_ref, path) != show(head, path):
            report.error(f"branch adds {path}, which main also has with different content")

    ledger = {row["address"]: row for row in read_tsv(show(main_ref, LEDGER) or "")}
    for ref in (main_ref, head):
        for row in read_tsv(show(ref, ADDITIONS) or ""):
            ledger.setdefault(row["entry"].strip().lower(),
                              {"size": row["size"], "securom": "clean (function_additions)"})

    head_targets = all_targets(head)
    base_targets = all_targets(base)
    main_targets = all_targets(main_ref)
    base_keys = {e["key"] for entries in base_targets.values() for e in entries}
    new_entries: dict[str, dict] = {}
    for path, entries in head_targets.items():
        if path not in changed:
            continue
        text = show(head, path) or ""
        records = set(RECORD.findall(text))
        for entry in entries:
            if entry["key"] in base_keys:
                continue
            entry["file"] = path
            new_entries[entry["key"]] = entry
            if entry["key"] not in records:
                report.error(f"hook {entry['key']}: no record*(\"{entry['key']}\") call in {path}")
        for key in sorted(records - {e["key"] for e in entries} - base_keys):
            report.error(f"{path}: record key '{key}' has no target table entry")

    merged_keys = collections.Counter(e["key"] for entries in main_targets.values() for e in entries)
    merged_rvas = {e["rva"]: e["key"] for entries in main_targets.values() for e in entries}
    for key, entry in new_entries.items():
        if merged_keys[key]:
            report.error(f"hook {key}: key already used on {main_ref}")
        if entry["rva"] in merged_rvas:
            report.error(f"hook {key}: RVA 0x{entry['rva']:08X} already hooked on {main_ref} "
                         f"as {merged_rvas[entry['rva']]}")
        size = int(ledger.get(f"{entry['rva'] + IMAGE_BASE:08x}", {}).get("size") or 0)
        check_prologue(image, entry, size, report)
    head_keys = collections.Counter(e["key"] for entries in head_targets.values() for e in entries)
    for key, n in head_keys.items():
        if n > 1:
            report.error(f"hook {key}: defined {n} times on the branch")

    docs = [p for p, s in changes if s != "D" and p.startswith("Ra3Source/docs/design/") and p.endswith(".md")]
    if new_entries:
        check_doc_bytes(head, docs, new_entries, report)

    owner = args.owner or f"recon/{head.split('/', 1)[-1]}"
    main_claims = {row["address"]: row["owner"] for row in read_tsv(show(main_ref, MAIN_ANNOTATIONS) or "")
                   if (row.get("status") or "").strip()}
    sibling_claims: dict[str, str] = {}
    for branch in open_branches(main_ref, head):
        sibling_claims.update(claimed_rows(branch, git("merge-base", branch, main_ref).strip(), head))

    statuses = collections.Counter()
    protection = collections.Counter()
    seen: set[str] = set()
    counted: set[str] = set()
    annotation_files = [p for p, s in changes if s != "D" and p.startswith(DOCS)
                        and p.endswith("_annotations.tsv") and p != MAIN_ANNOTATIONS]
    for path in annotation_files:
        text = show(head, path) or ""
        header = text.splitlines()[0].split("\t") if text else []
        if header != ANNOTATION_COLUMNS:
            report.error(f"{path}: header {header} is not {ANNOTATION_COLUMNS}")
            continue
        for row in read_tsv(text):
            addr = (row["address"] or "").strip()
            status = (row["status"] or "").strip()
            if not re.fullmatch(r"[0-9a-f]{8}", addr):
                report.error(f"{path}: address '{addr}' is not lowercase 8-digit hex")
                continue
            if addr in seen:
                report.error(f"{path}: {addr} listed twice")
            seen.add(addr)
            if status not in ("", "reversed", "verified"):
                report.error(f"{path}: {addr} has status '{status}'")
            if (row["owner"] or "").strip() != owner:
                report.error(f"{path}: {addr} owner '{row['owner']}' is not {owner}")
            statuses[status or "(deferred)"] += 1
            if addr not in ledger:
                (report.error if status else report.warn)(
                    f"{path}: {addr} is not a ledger function entry")
                continue
            if not status:
                continue
            counted.add(addr)
            protection[ledger[addr]["securom"] or "(none)"] += 1
            if not (row["notes"] or "").strip():
                report.error(f"{path}: {addr} has no notes")
            if addr in main_claims and main_claims[addr] != owner:
                report.error(f"{path}: {addr} already claimed on {main_ref} by {main_claims[addr]}")
            if addr in sibling_claims:
                report.error(f"{path}: {addr} also claimed by {sibling_claims[addr]}")

    for path, status in changes:
        if status == "D" or not path.startswith("Ra3Source/src/") or Path(path).suffix not in (".cpp", ".h", ".inc"):
            continue
        for tag in lifted_bodies(show(head, path) or ""):
            if tag in counted:
                report.error(f"{path}: {tag} is register/stack-emulating lifted code, not a reconstruction")
            else:
                report.warn(f"{path}: lifted body {tag} (not a counted row)")

    explicit = sum(1 for e in new_entries.values() if e["explicit"])
    print(f"branch {head}  base {base[:10]}  changed files {len(changes)}")
    print(f"annotations: {dict(statuses)}  protection of claimed rows: {dict(protection)}")
    print(f"new hooks: {len(new_entries)} ({explicit} explicitOnly, "
          f"{len(new_entries) - explicit} default)")
    for key, entry in sorted(new_entries.items(), key=lambda kv: kv[1]["rva"]):
        print(f"  {key:<32} 0x{entry['rva'] + IMAGE_BASE:08X} stolen={entry['stolen']:<2} "
              f"explicitOnly={'true' if entry['explicit'] else 'false'}")
    total = sum(len(v) for v in main_targets.values()) + len(new_entries)
    print(f"hook table after merge: {total} targets")
    for text in report.warnings:
        print(f"WARN  {text}")
    for text in report.errors:
        print(f"ERROR {text}")
    print("RESULT " + ("FAIL" if report.errors else "PASS"))
    return 1 if report.errors else 0


if __name__ == "__main__":
    sys.exit(main())
