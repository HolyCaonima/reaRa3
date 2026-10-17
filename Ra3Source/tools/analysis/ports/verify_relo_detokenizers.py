#!/usr/bin/env python3
"""Differential oracle for RA3 entry-lost .relo relocation & detokenizer functions.

Executes the 46 recovered native x86 function bodies (35 in .text, 11 in .securom)
from the run35 runtime capture via Unicorn against ra3_relodetokcheck.exe.
"""

from __future__ import annotations

import argparse
from pathlib import Path
import struct
import subprocess
import sys

import unicorn

SCRIPT_DIR = Path(__file__).resolve().parent
ANALYSIS_DIR = SCRIPT_DIR.parent
REPO_ROOT = ANALYSIS_DIR.parents[2]
sys.path.insert(0, str(ANALYSIS_DIR))

from emulate import Emulator, entry_targets  # noqa: E402

ARENA_BASE = 0x60000000
ARENA_SIZE = 0x00020000
INSTANCE_ADDR = ARENA_BASE + 0x1000
SRC_ADDR = ARENA_BASE + 0x2000
DST_ADDR = ARENA_BASE + 0x3000
HEAP_START_ADDR = ARENA_BASE + 0x4000

EXPECTED_TARGETS: dict[int, tuple[int, str]] = {
    # 5 Core .relo & Asset Primitives
    0x00416ED0: (0x004416E0, "assetMemoryCompare"),
    0x00417200: (0x0043A1D0, "reloApplyPointerOffsets"),
    0x00417380: (0x0043E450, "ReloScratchBuffer::ensureCapacity"),
    0x00418090: (0x0043B110, "ReloStreamHeader::clear"),
    0x00447740: (0x00491B00, "assetByteSpanLess"),
    # 20 ReloClone_<Type> Nullable Struct/Field Cloners
    0x00417550: (0x0047A780, "reloCloneCoord3D"),
    0x004176B0: (0x00424730, "reloCloneString"),
    0x0043BAD0: (0x0043FBC0, "reloCloneValue8"),
    0x0043BF60: (0x00455D70, "reloCloneObjectStatusBitFlags"),
    0x0043BFB0: (0x0048F3F0, "reloCloneValue60"),
    0x0043D830: (0x0045EA50, "reloCloneStruct60_419580"),
    0x0043FC60: (0x00490490, "reloCloneStruct20_423860"),
    0x004404A0: (0x0047BB20, "reloCloneScalar32"),
    0x004428C0: (0x0046DF20, "reloCloneStruct12_41D1E0"),
    0x004430E0: (0x0045CCA0, "reloCloneObjectStatusValidationDataType"),
    0x00445C10: (0x00490690, "reloCloneStruct36_4323B0"),
    0x0045D9F0: (0x00450950, "reloCloneSoundOrEvaEvent"),
    0x0045E570: (0x0040B970, "reloCloneStruct44_44A230"),
    0x0045F460: (0x004724A0, "reloCloneStruct8_455B10"),
    0x0045F5D0: (0x00446480, "reloCloneStruct8_448070"),
    0x004617B0: (0x0047A6F0, "reloCloneStruct8_45B590"),
    0x00478BA0: (0x0047AC00, "reloCloneStruct24_467520"),
    0x004812D0: (0x0043BD10, "reloCloneStruct8_47EF30"),
    0x00495180: (0x004290B0, "reloCloneStruct88_4944E0"),
    0x004A9AB0: (0x0048FE70, "reloCloneStruct40_4A7120"),
    # 21 ReloDetokenizeFields_<Type> Field-Descriptor Walkers
    0x0041CF20: (0x0190C480, "reloDetokenizeModuleData"),
    0x0041D0B0: (0x0046DC80, "reloDetokenizeObjectStatusValidationDataType"),
    0x0042C120: (0x01918630, "reloDetokenizeAttackAssistResponseBehaviorModuleData"),
    0x0042C8B0: (0x00492CA0, "reloDetokenizeQueueDockModuleData"),
    0x0042CDC0: (0x0191BDE0, "reloDetokenizeStructurePlacementBehaviorModuleData"),
    0x00448170: (0x0045CF90, "reloDetokenizeAudioVoiceEntry"),
    0x00454BF0: (0x004AF880, "reloDetokenizeHighlanderBodyModuleData"),
    0x00459520: (0x01907EB0, "reloDetokenizeLaserStateModuleData"),
    0x00472180: (0x01908CA0, "reloDetokenizeSubObjectsUpgradeModuleData"),
    0x00473100: (0x00446370, "reloDetokenizeShipSlowDeathBehaviorModuleData"),
    0x0047C190: (0x01909340, "reloDetokenizeFXShaderMaterial"),
    0x0048A430: (0x0044F800, "reloDetokenizeGarrisonContainModuleData"),
    0x0048A520: (0x018FE520, "reloDetokenizeHordeGarrisonContainModuleData"),
    0x0048B940: (0x00487B30, "reloDetokenizeCaptureSpecialAbilityUpdateModuleData"),
    0x00493870: (0x01903AA0, "reloDetokenizeW3DTankDrawModuleData"),
    0x00494430: (0x019104B0, "reloDetokenizeToggleStatusSpecialAbilityUpdateModuleData"),
    0x004A8280: (0x0044D880, "reloDetokenizeRecallUnitsSpecialPowerModuleData"),
    0x004A8A50: (0x00440590, "reloDetokenizeAttackTargetSpecialPowerModuleData"),
    0x004A8EA0: (0x01912C20, "reloDetokenizeActivateModuleSpecialPower"),
    0x004A94E0: (0x0045DC60, "reloDetokenizeMoneyCrateCollideModuleData"),
    0x004A9600: (0x0049FDA0, "reloDetokenizeVeterancyCrateCollideModuleData"),
}

# (entry_va, alloc_size)
CLONE_SPECS: list[tuple[int, int]] = [
    (0x00417550, 12),
    (0x004176B0, 8),
    (0x0043BAD0, 8),
    (0x0043BF60, 28),
    (0x0043BFB0, 60),
    (0x0043D830, 60),
    (0x0043FC60, 20),
    (0x004404A0, 4),
    (0x004428C0, 12),
    (0x004430E0, 56),
    (0x00445C10, 36),
    (0x0045D9F0, 8),
    (0x0045E570, 44),
    (0x0045F460, 8),
    (0x0045F5D0, 8),
    (0x004617B0, 8),
    (0x00478BA0, 24),
    (0x004812D0, 8),
    (0x00495180, 88),
    (0x004A9AB0, 40),
]

# (entry_va, dst_size, list of (field_id, type_hash))
WALKER_SPECS: list[tuple[int, int, list[tuple[int, int]]]] = [
    (
        0x0041CF20,
        0x08,
        [
            (0x8CDC9898, 0xBDF07965),
            (0x19B042A0, 0x23E650B8),
        ],
    ),
    (
        0x0041D0B0,
        0x38,
        [
            (0x239EB201, 0xA7BD811E),
            (0x312281FD, 0xA7BD811E),
        ],
    ),
    (
        0x0042C120,
        0x30,
        [
            (0x00000001, 0xA50D729A),
            (0xEC430B3A, 0xA7BD811E),
            (0x1848C549, 0x1848C549),
            (0x5E259CB2, 0x763177A4),
        ],
    ),
    (
        0x0042C8B0,
        0x34,
        [
            (0x00000001, 0x30A36B8D),
            (0xBC613612, 0x1A2933AA),
            (0x3EFFF3BD, 0x1A2933AA),
            (0xC17D6193, 0x1A2933AA),
            (0x7D16400E, 0x1A2933AA),
            (0xD4A2D1AD, 0x1A2933AA),
            (0x4BB8F560, 0x15723E0B),
        ],
    ),
    (
        0x0042CDC0,
        0x10,
        [
            (0x00000001, 0xA50D729A),
            (0xCAFDC008, 0xE7210555),
        ],
    ),
    (
        0x00448170,
        0x10,
        [
            (0x00000001, 0x1AE6B6B7),
            (0x88F12FFB, 0x4DA708B0),
        ],
    ),
    (
        0x00454BF0,
        0x10,
        [
            (0x00000001, 0x5A0E3D9B),
        ],
    ),
    (
        0x00459520,
        0x2C,
        [
            (0x00000001, 0xF1002505),
            (0xC48AD38E, 0x1A2933AA),
            (0xC58EA518, 0x29368077),
            (0xEA73EC5F, 0x3E8F938C),
            (0x42A4EC4F, 0x23E650B8),
            (0x7120BE4E, 0x43F1CE36),
            (0x7E5D901A, 0x43F1CE36),
        ],
    ),
    (
        0x00472180,
        0x2C,
        [
            (0x00000001, 0xAE60AE98),
            (0xBE41637D, 0x7F67C232),
        ],
    ),
    (
        0x00473100,
        0x10,
        [
            (0x00000001, 0x98769794),
        ],
    ),
    (
        0x0047C190,
        0x1C,
        [
            (0xDB4C0154, 0x1A2933AA),
            (0x4CC1CA67, 0x1A2933AA),
            (0x66B329AD, 0x19842B2F),
            (0x7A39D303, 0x30916B96),
        ],
    ),
    (
        0x0048A430,
        0xA0,
        [
            (0x00000001, 0xD7CC2E35),
            (0x201C212B, 0x201C212B),
            (0x5B9A8331, 0x77E3F4F7),
            (0xB6CAC298, 0x77E3F4F7),
            (0xFA3BFE5C, 0x77E3F4F7),
        ],
    ),
    (
        0x0048A520,
        0xB0,
        [
            (0x00000001, 0xBAE3BC7D),
            (0xF3D211A1, 0x763177A4),
            (0x88D1B434, 0x29368077),
            (0x9019CE65, 0x29368077),
            (0x294B04AB, 0x29368077),
        ],
    ),
    (
        0x0048B940,
        0x10,
        [
            (0x00000001, 0xAC598F82),
        ],
    ),
    (
        0x00493870,
        0xE4,
        [
            (0x00000001, 0x4ECF2A4B),
            (0x2A350DD4, 0xA6FF4551),
            (0xC4037FCB, 0xC4037FCB),
        ],
    ),
    (
        0x00494430,
        0xFC,
        [
            (0x00000001, 0xAC598F82),
            (0x07DAB6F7, 0xFC294B1D),
            (0xB3C22701, 0x77E3F4F7),
        ],
    ),
    (
        0x004A8280,
        0x1C8,
        [
            (0x00000001, 0xCEFE1113),
            (0xC1BAA310, 0x23E650B8),
            (0x995C7C88, 0x77E3F4F7),
        ],
    ),
    (
        0x004A8A50,
        0x10,
        [
            (0x00000001, 0xCEFE1113),
        ],
    ),
    (
        0x004A8EA0,
        0x1C8,
        [
            (0x00000001, 0xCEFE1113),
            (0x6C48FC6E, 0x58F6065C),
        ],
    ),
    (
        0x004A94E0,
        0x70,
        [
            (0x00000001, 0x2027962A),
            (0xAB6ABBF0, 0x763177A4),
        ],
    ),
    (
        0x004A9600,
        0x78,
        [
            (0x00000001, 0x2027962A),
            (0xA249AAF4, 0x77E3F4F7),
            (0xA50E58A0, 0x77E3F4F7),
            (0x62D48EA3, 0x763177A4),
            (0x3E4F1C74, 0x23E650B8),
        ],
    ),
]

EXTERNAL_DETOKENIZER_VAS: list[int] = [
    0x004174E0,
    0x00417B70,
    0x00418D60,
    0x00419580,
    0x0041D1E0,
    0x0041D270,
    0x00423860,
    0x0042A360,
    0x0042B1A0,
    0x00431F70,
    0x004323B0,
    0x00442ED0,
    0x00442F20,
    0x004475F0,
    0x00447E30,
    0x00448070,
    0x0044A230,
    0x00453ED0,
    0x00454690,
    0x00455B10,
    0x0045B590,
    0x0045F4B0,
    0x00467520,
    0x0046FFE0,
    0x00476A50,
    0x0047EF30,
    0x004890D0,
    0x0048AB00,
    0x00490830,
    0x00492EA0,
    0x004944E0,
    0x004A7120,
    0x004A71D0,
    0x004A8F30,
]


def encode_hex(data: bytes) -> str:
    return "-" if not data else data.hex()


class EmulatedArena:
    def __init__(self, emu: Emulator) -> None:
        self.emu = emu
        self.heap_cursor = HEAP_START_ADDR
        self.events: list[str] = []
        self._install_hooks()

    def reset(self) -> None:
        self.emu.write(ARENA_BASE, b"\x00" * ARENA_SIZE)
        self.heap_cursor = HEAP_START_ADDR
        self.events.clear()

    def alloc_aligned(self, size: int) -> int:
        aligned = (self.heap_cursor + 15) & ~15
        step = 16 if size == 0 else ((size + 15) & ~15)
        self.emu.write(aligned, b"\xcc" * step)
        self.heap_cursor = aligned + step
        return aligned

    def join_events(self) -> str:
        return "-" if not self.events else ",".join(self.events)

    def read_u32(self, addr: int) -> int:
        return struct.unpack("<I", self.emu.read(addr, 4))[0]

    def write_u32(self, addr: int, val: int) -> None:
        self.emu.write(addr, struct.pack("<I", val & 0xFFFFFFFF))

    def _install_hooks(self) -> None:
        # Allow .securom runtime bodies to execute dec dword ptr [0x0198xxxx]
        self.emu.uc.mem_protect(
            0x01980000,
            0x10000,
            unicorn.UC_PROT_READ | unicorn.UC_PROT_WRITE | unicorn.UC_PROT_EXEC,
        )

        def stub_alloc_tagged(em: Emulator) -> int:
            size = em.stack_arg(0)
            ptr = self.alloc_aligned(size)
            self.events.append(f"alloc:{size:x}@{ptr:x}")
            return ptr

        def stub_alloc_raw(em: Emulator) -> int:
            size = em.stack_arg(0)
            ptr = self.alloc_aligned(size)
            self.events.append(f"alloc_raw:{size:x}@{ptr:x}")
            return ptr

        def stub_free_raw(em: Emulator) -> int:
            ptr = em.stack_arg(0)
            self.events.append(f"free_raw:{ptr:x}")
            return 0

        def stub_memset(em: Emulator) -> int:
            dst = em.stack_arg(0)
            val = em.stack_arg(1) & 0xFF
            count = em.stack_arg(2)
            if count > 0:
                em.write(dst, bytes([val]) * count)
            return dst

        self.emu.bind_function(0x004165A0, stub_alloc_tagged)
        self.emu.bind_function(0x004166B0, stub_free_raw)
        self.emu.bind_function(0x00416840, stub_alloc_raw)
        self.emu.bind_function(0x004D2F98, stub_memset)
        self.emu.bind_import(0x00BBA50C, stub_memset)

        for ext_va in EXTERNAL_DETOKENIZER_VAS:
            def make_ext_stub(va: int):
                def _stub(em: Emulator) -> int:
                    inst_base = em.stack_arg(0)
                    src_field = em.stack_arg(1)
                    dst_field = em.stack_arg(2)
                    ctx_val = em.stack_arg(3)
                    src_word = 0x55555555 if src_field == 0 else self.read_u32(src_field)
                    tag = (va ^ ((src_word * 0x9E3779B1) & 0xFFFFFFFF) ^ ctx_val) & 0xFFFFFFFF
                    self.write_u32(dst_field, tag)
                    self.events.append(
                        f"call:{va:x}:{inst_base:x}:{src_field:x}:{dst_field:x}:{ctx_val:x}"
                    )
                    return 0

                return _stub

            self.emu.bind_function(ext_va, make_ext_stub(ext_va))


def build_scenarios() -> list[tuple[int, str, str]]:
    """Returns list of (entry_va, scenario_label, checker_line)."""
    scenarios: list[tuple[int, str, str]] = []

    # 1. 0x00416ED0 assetMemoryCompare (6 scenarios)
    memcmp_cases = [
        ("zero_len", b"ABCD", b"WXYZ", 0),
        ("equal_11b", b"RedAlert3!!", b"RedAlert3!!", 11),
        ("diff_dword0_lt", b"\x10\x20\x30\x40\xaa", b"\x10\x20\x31\x40\xaa", 5),
        ("diff_dword1_gt", b"ABCD\x99\x00\x00\x00", b"ABCD\x11\x00\x00\x00", 8),
        ("diff_tail_unsigned_lt", b"ABCDEFGH\x7f", b"ABCDEFGH\x80", 9),
        ("diff_tail_unsigned_gt", b"ABCDEFGH\xfe\x01", b"ABCDEFGH\x7f\x01", 10),
    ]
    for label, lhs, rhs, count in memcmp_cases:
        line = f"memcmp {encode_hex(lhs)} {encode_hex(rhs)} {count:x}"
        scenarios.append((0x00416ED0, label, line))

    # 2. 0x00417200 reloApplyPointerOffsets (4 scenarios)
    relofixup_cases = [
        ("empty_table", [0xFFFFFFFF], [0x10, 0x20, 0x30, 0x40]),
        ("single_slot0", [0, 0xFFFFFFFF], [0x40, 0x11111111, 0x22222222, 0x33333333]),
        ("multi_slots", [0, 8, 20, 0xFFFFFFFF], [0x04, 0xAA, 0x18, 0xBB, 0xCC, 0x30]),
        ("odd_slots", [4, 16, 28, 0xFFFFFFFF], [0x00, 0x08, 0x00, 0x00, 0x24, 0x00, 0x00, 0x4C]),
    ]
    for label, table_words, inst_words in relofixup_cases:
        table_bytes = b"".join(struct.pack("<I", w) for w in table_words)
        inst_bytes = b"".join(struct.pack("<I", w) for w in inst_words)
        line = f"relofixup {encode_hex(table_bytes)} {encode_hex(inst_bytes)}"
        scenarios.append((0x00417200, label, line))

    # 3. 0x00417380 ReloScratchBuffer::ensureCapacity (4 scenarios)
    scratch_cases = [
        ("reuse_sufficient", 0x60009000, 0x80, 0x40),
        ("reuse_exact", 0x60009000, 0x40, 0x40),
        ("alloc_from_null", 0x0, 0x10, 0x30),
        ("realloc_free_old", 0x60009000, 0x20, 0x48),
    ]
    for label, data_ptr, cap, req in scratch_cases:
        line = f"scratch {data_ptr:x} {cap:x} {req:x}"
        scenarios.append((0x00417380, label, line))

    # 4. 0x00418090 ReloStreamHeader::clear (2 scenarios)
    scenarios.append((0x00418090, "all_ff", f"clearhdr {'ff' * 32}"))
    scenarios.append(
        (0x00418090, "patterned", f"clearhdr {bytes(range(1, 33)).hex()}")
    )

    # 5. 0x00447740 assetByteSpanLess (6 scenarios)
    spanless_cases = [
        ("prefix_shorter_true", b"Allied", b"AlliedPrism", 6, 11),
        ("prefix_longer_false", b"AlliedPrism", b"Allied", 11, 6),
        ("equal_false", b"SovietTesla", b"SovietTesla", 11, 11),
        ("content_lt_longer", b"Empire\x10XYZ", b"Empire\x20", 10, 7),
        ("content_gt_shorter", b"Empire\x90", b"Empire\x20XYZ", 7, 10),
        ("unsigned_high_bit", b"Test\x7f", b"Test\x80", 5, 5),
    ]
    for label, lhs, rhs, lhs_len, rhs_len in spanless_cases:
        line = f"spanless {encode_hex(lhs)} {encode_hex(rhs)} {lhs_len:x} {rhs_len:x}"
        scenarios.append((0x00447740, label, line))

    # 6. 20 ReloClone_<Type> functions (3 scenarios each)
    for va, size in CLONE_SPECS:
        scenarios.append(
            (va, "null_src", f"clone {va:x} 1 - deadbeef 11223344")
        )
        if va == 0x004430E0:
            # reloCloneObjectStatusValidationDataType calls intra-cluster 0x0041D0B0!
            # src is a 12-byte ReloFieldDescriptor array at SRC_ADDR (0x60002000);
            # offset 0x1040 -> INSTANCE_ADDR + 0x1040 == SRC_ADDR + 0x40
            desc_a = struct.pack(
                "<IIIIIIIII",
                0x239EB201, 0xA7BD811E, 0x1030,
                0x312281FD, 0xA7BD811E, 0x1050,
                0x00000000, 0x00000000, 0x0000,
            ) + bytes((i * 7 + 3) & 0xFF for i in range(96))
            desc_b = struct.pack(
                "<IIIIII",
                0x312281FD, 0xA7BD811E, 0x1020,
                0x00000000, 0x00000000, 0x0000,
            ) + bytes((i * 13 + 9) & 0xFF for i in range(96))
            scenarios.append(
                (va, "nonnull_a", f"clone {va:x} 0 {encode_hex(desc_a)} 0 11223344")
            )
            scenarios.append(
                (va, "nonnull_b", f"clone {va:x} 0 {encode_hex(desc_b)} deadbeef cafebabe")
            )
        else:
            src_a = bytes(((i * 17 + (va & 0xFF)) & 0xFF) for i in range(size))
            src_b = bytes(((i * 29 + ((va >> 8) & 0xFF) + 0x55) & 0xFF) for i in range(size))
            scenarios.append(
                (va, "nonnull_a", f"clone {va:x} 0 {encode_hex(src_a)} 0 11223344")
            )
            scenarios.append(
                (va, "nonnull_b", f"clone {va:x} 0 {encode_hex(src_b)} deadbeef cafebabe")
            )

    # 7. 21 ReloDetokenizeFields_<Type> functions (4 scenarios each)
    for va, dst_size, field_specs in WALKER_SPECS:
        inst_a = bytearray(((i * 23 + 0x11) & 0xFF) for i in range(0x240))
        inst_b = bytearray(((i * 37 + 0x83) & 0xFF) for i in range(0x240))
        if va == 0x00459520:
            # Nested ObjectStatusValidationDataType descriptor table at offset 0x100
            sub_desc = struct.pack(
                "<IIIIIIIII",
                0x239EB201, 0xA7BD811E, 0x150,
                0x312281FD, 0xA7BD811E, 0x170,
                0x00000000, 0x00000000, 0x000,
            )
            inst_a[0x100 : 0x100 + len(sub_desc)] = sub_desc
            inst_b[0x100 : 0x100 + len(sub_desc)] = sub_desc
        elif va == 0x0048A520:
            # Nested GarrisonContainModuleData descriptor table at offset 0x100
            sub_desc = struct.pack(
                "<IIIIIIIIIIII",
                0x00000001, 0xD7CC2E35, 0x150,
                0x5B9A8331, 0x77E3F4F7, 0x160,
                0xFA3BFE5C, 0x77E3F4F7, 0x170,
                0x00000000, 0x00000000, 0x000,
            )
            inst_a[0x100 : 0x100 + len(sub_desc)] = sub_desc
            inst_b[0x100 : 0x100 + len(sub_desc)] = sub_desc

        dst_init_a = bytes([0xAA] * dst_size)
        dst_init_b = bytes([0x55] * dst_size)

        # Scenario 1: fields == nullptr
        scenarios.append(
            (va, "null_fields", f"walk {va:x} 1 {encode_hex(bytes(inst_a))} - {encode_hex(dst_init_a)} 10203040")
        )

        # Scenario 2: empty descriptor table ({0, 0, 0})
        empty_fields = struct.pack("<III", 0, 0, 0)
        scenarios.append(
            (va, "empty_fields", f"walk {va:x} 0 {encode_hex(bytes(inst_a))} {encode_hex(empty_fields)} {encode_hex(dst_init_a)} 10203040")
        )

        # Scenario 3: all (field_id, type_hash) pairs + wrong type_hash + unknown field_id in forward order
        entries_fwd: list[tuple[int, int, int]] = []
        for idx, (fid, thash) in enumerate(field_specs):
            if (va == 0x00459520 and fid == 0xEA73EC5F) or (va == 0x0048A520 and fid == 0x00000001):
                offset = 0x100
            else:
                offset = 0x20 + idx * 0x1C
            entries_fwd.append((fid, thash, offset))
            if idx == 0:
                entries_fwd.append((fid, thash ^ 0xFFFFFFFF, offset + 4))
            if idx == len(field_specs) // 2:
                entries_fwd.append((0x7FFFFFFF, 0x12345678, 0x10))
        entries_fwd.append((0, 0, 0))
        fields_fwd = b"".join(struct.pack("<III", fid, thash, off) for fid, thash, off in entries_fwd)
        scenarios.append(
            (va, "all_fields_fwd", f"walk {va:x} 0 {encode_hex(bytes(inst_a))} {encode_hex(fields_fwd)} {encode_hex(dst_init_a)} 22334455")
        )

        # Scenario 4: reverse order with alternate instance payload & context
        entries_rev: list[tuple[int, int, int]] = []
        for idx, (fid, thash) in enumerate(reversed(field_specs)):
            if (va == 0x00459520 and fid == 0xEA73EC5F) or (va == 0x0048A520 and fid == 0x00000001):
                offset = 0x100
            else:
                offset = 0x28 + idx * 0x1C
            entries_rev.append((fid, thash, offset))
        entries_rev.append((0x01020304, 0x05060708, 0))
        entries_rev.append((0, 0, 0))
        fields_rev = b"".join(struct.pack("<III", fid, thash, off) for fid, thash, off in entries_rev)
        scenarios.append(
            (va, "all_fields_rev", f"walk {va:x} 0 {encode_hex(bytes(inst_b))} {encode_hex(fields_rev)} {encode_hex(dst_init_b)} 99aabbcc")
        )

    return scenarios


def run_emulated_scenario(
    arena: EmulatedArena,
    targets: dict[int, int],
    line: str,
) -> str:
    arena.reset()
    emu = arena.emu
    parts = line.split()
    kind = parts[0]

    if kind == "memcmp":
        lhs = bytes.fromhex(parts[1]) if parts[1] != "-" else b""
        rhs = bytes.fromhex(parts[2]) if parts[2] != "-" else b""
        count = int(parts[3], 16)
        emu.write(INSTANCE_ADDR, lhs)
        emu.write(SRC_ADDR, rhs)
        res = emu.call(targets[0x00416ED0], (INSTANCE_ADDR, SRC_ADDR, count))
        return f"{res.eax & 0xFFFFFFFF:x}"

    if kind == "relofixup":
        table_bytes = bytes.fromhex(parts[1])
        inst_bytes = bytes.fromhex(parts[2])
        emu.write(SRC_ADDR, table_bytes)
        emu.write(INSTANCE_ADDR, inst_bytes)
        emu.call(targets[0x00417200], (SRC_ADDR, INSTANCE_ADDR))
        out_bytes = emu.read(INSTANCE_ADDR, len(inst_bytes))
        return encode_hex(out_bytes)

    if kind == "scratch":
        data_ptr = int(parts[1], 16)
        cap = int(parts[2], 16)
        req = int(parts[3], 16)
        arena.write_u32(DST_ADDR + 0, data_ptr)
        arena.write_u32(DST_ADDR + 4, cap)
        res = emu.call(targets[0x00417380], (req,), ecx=DST_ADDR)
        struct_bytes = emu.read(DST_ADDR, 8)
        return f"{res.eax:x} {encode_hex(struct_bytes)} {arena.join_events()}"

    if kind == "clearhdr":
        hdr_bytes = bytes.fromhex(parts[1])
        emu.write(DST_ADDR, hdr_bytes)
        res = emu.call(targets[0x00418090], (), ecx=DST_ADDR)
        out_bytes = emu.read(DST_ADDR, 32)
        return f"{res.eax:x} {encode_hex(out_bytes)}"

    if kind == "spanless":
        lhs = bytes.fromhex(parts[1])
        rhs = bytes.fromhex(parts[2])
        lhs_len = int(parts[3], 16)
        rhs_len = int(parts[4], 16)
        emu.write(INSTANCE_ADDR, lhs)
        emu.write(SRC_ADDR, rhs)
        arena.write_u32(DST_ADDR + 0, INSTANCE_ADDR)
        arena.write_u32(DST_ADDR + 4, INSTANCE_ADDR + lhs_len)
        arena.write_u32(DST_ADDR + 8, SRC_ADDR)
        arena.write_u32(DST_ADDR + 12, SRC_ADDR + rhs_len)
        res = emu.call(targets[0x00447740], (DST_ADDR, DST_ADDR + 8))
        return f"{res.eax & 0xFF:x}"

    if kind == "clone":
        va = int(parts[1], 16)
        is_null = parts[2] == "1"
        src_bytes = b"" if parts[3] == "-" else bytes.fromhex(parts[3])
        init_dst_ptr = int(parts[4], 16)
        ctx = int(parts[5], 16)
        src_ptr = 0
        if not is_null:
            src_ptr = SRC_ADDR
            emu.write(SRC_ADDR, src_bytes)
        arena.write_u32(DST_ADDR, init_dst_ptr)
        emu.call(targets[va], (INSTANCE_ADDR, src_ptr, DST_ADDR, ctx))
        dst_ptr_after = arena.read_u32(DST_ADDR)
        heap_used = arena.heap_cursor - HEAP_START_ADDR
        heap_bytes = emu.read(HEAP_START_ADDR, heap_used) if heap_used > 0 else b""
        return f"{dst_ptr_after:x} {encode_hex(heap_bytes)} {arena.join_events()}"

    if kind == "walk":
        va = int(parts[1], 16)
        is_null = parts[2] == "1"
        inst_bytes = bytes.fromhex(parts[3])
        fields_bytes = b"" if parts[4] == "-" else bytes.fromhex(parts[4])
        dst_bytes = bytes.fromhex(parts[5])
        ctx = int(parts[6], 16)

        emu.write(INSTANCE_ADDR, inst_bytes)
        fields_ptr = 0
        if not is_null:
            fields_ptr = SRC_ADDR
            emu.write(SRC_ADDR, fields_bytes)
        emu.write(DST_ADDR, dst_bytes)
        emu.call(targets[va], (INSTANCE_ADDR, fields_ptr, DST_ADDR, ctx))
        out_dst = emu.read(DST_ADDR, len(dst_bytes))
        heap_used = arena.heap_cursor - HEAP_START_ADDR
        heap_bytes = emu.read(HEAP_START_ADDR, heap_used) if heap_used > 0 else b""
        return f"{encode_hex(out_dst)} {encode_hex(heap_bytes)} {arena.join_events()}"

    raise ValueError(f"Unknown scenario kind: {kind}")


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "--capture",
        type=Path,
        default=Path("D:/Game/Ra3/RevRa3/work/runtime_dump/run35/snapshot_3_240s"),
        help="Path to run35 runtime capture snapshot",
    )
    parser.add_argument(
        "--sites",
        type=Path,
        default=REPO_ROOT / "Ra3Source/docs/analysis/ra3_1.0/securom_sites.tsv",
        help="Path to securom_sites.tsv",
    )
    parser.add_argument(
        "--checker",
        type=Path,
        default=REPO_ROOT / "Ra3Source/build/win32/bin/Release/ra3_relodetokcheck.exe",
        help="Path to ra3_relodetokcheck.exe",
    )
    args = parser.parse_args()

    targets = entry_targets(args.sites)
    for va, (expected_body, name) in EXPECTED_TARGETS.items():
        actual_body = targets.get(va)
        if actual_body != expected_body:
            print(
                f"TARGET MISMATCH for 0x{va:08X} ({name}): "
                f"expected 0x{expected_body:08X}, got {actual_body!r}",
                file=sys.stderr,
            )
            return 1

    emu = Emulator(capture=args.capture)
    arena = EmulatedArena(emu)

    scenarios = build_scenarios()
    input_payload = "\n".join(line for _, _, line in scenarios) + "\n"
    proc = subprocess.run(
        [str(args.checker)],
        input=input_payload,
        text=True,
        capture_output=True,
        check=True,
    )
    cpp_outputs = proc.stdout.splitlines()
    if len(cpp_outputs) != len(scenarios):
        print(
            f"Checker output line count mismatch: expected {len(scenarios)}, got {len(cpp_outputs)}",
            file=sys.stderr,
        )
        return 1

    mismatches = 0
    per_va_counts: dict[int, int] = {va: 0 for va in EXPECTED_TARGETS}

    for (va, label, line), cpp_out in zip(scenarios, cpp_outputs):
        emu_out = run_emulated_scenario(arena, targets, line)
        per_va_counts[va] += 1
        if emu_out != cpp_out:
            mismatches += 1
            _, fn_name = EXPECTED_TARGETS[va]
            print(
                f"MISMATCH 0x{va:08X} -> 0x{targets[va]:08X} {fn_name} [{label}]:\n"
                f"  emu: {emu_out}\n"
                f"  cpp: {cpp_out}"
            )

    print(
        f"Verified {len(EXPECTED_TARGETS)} entry-lost functions across "
        f"{len(scenarios)} scenarios: {mismatches} mismatches."
    )
    return 0 if mismatches == 0 else 1


if __name__ == "__main__":
    raise SystemExit(main())
