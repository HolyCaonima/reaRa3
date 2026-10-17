"""Release oracle for module getModuleName / getModuleNameKey leaves."""

from __future__ import annotations

import argparse
import struct
import subprocess
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from unicorn import x86_const as uc
from disasm import Image  # noqa: E402
from emulate import Emulator  # noqa: E402

EXE = Path(r"D:\Game\Ra3\Game\Data\ra3_1.0.game")
NAMEKEY = 0x00986150
MANAGER = 0x00E1A078

NAMES = (
    (0x00727950, 0x00C1FC30, "OpenContain"),
    (0x00727E20, 0x00C20150, "TransportContain"),
    (0x007293F0, 0x00C2039C, "DelayedWeaponFireSpecialAbilityUpdate"),
    (0x0072AFB0, 0x00C20470, "LuaSpyData"),
    (0x0072F820, 0x00C206E4, "BoobyTrapUpdate"),
    (0x007304F0, 0x00BF8904, "GatherEjectUpdate"),
    (0x00730660, 0x00C20A5C, "LeechTargetingAttachUpdate"),
    (0x00730AF0, 0x00C20C34, "MoneyGainAttachUpdate"),
    (0x00732E30, 0x00C20E14, "StealUnitUpdate"),
    (0x0073CC60, 0x00C21B64, "HordeContain"),
    (0x0073DD70, 0x00C21F3C, "GarrisonContain"),
    (0x00740F10, 0x00C223CC, "AIUpdateInterface"),
    (0x00743760, 0x00C22658, "SqueezeFlingUpdate"),
    (0x0074E7D0, 0x00C22EEC, "SlaughterHordeContain"),
    (0x0074EA60, 0x00C232C4, "GarrisonStealthContain"),
    (0x0074EAA0, 0x00C236AC, "HealContain"),
    (0x0074FBE0, 0x00C23B5C, "TunnelContain"),
    (0x00751C00, 0x00C2400C, "AnimalAIUpdate"),
    (0x00751C70, 0x00C24434, "AssaultTransportAIUpdate"),
    (0x00751CC0, 0x00C24864, "DeployStyleAIUpdate"),
    (0x00751DD0, 0x00C24D04, "DozerAIUpdate"),
    (0x00751EA0, 0x00C25194, "HordeAIUpdate"),
    (0x00752330, 0x00C2564C, "SupplyTruckAIUpdate"),
    (0x00752400, 0x00C25A74, "TransportAIUpdate"),
    (0x00752440, 0x00C25E9C, "WanderAIUpdate"),
    (0x0075C870, 0x00C26A3C, "HordeGarrisonContain"),
)
KEYS = (
    (0x00727960, 0x00C1FC30, 0x00DF9948, 0x00DF9944),
    (0x00727E30, 0x00C20150, 0x00DF9950, 0x00DF994C),
    (0x00729400, 0x00C2039C, 0x00DF9958, 0x00DF9954),
    (0x0072F830, 0x00C206E4, 0x00DF9984, 0x00DF9980),
    (0x00730500, 0x00BF8904, 0x00DF998C, 0x00DF9988),
    (0x00730670, 0x00C20A5C, 0x00DF9994, 0x00DF9990),
    (0x00730B00, 0x00C20C34, 0x00DF999C, 0x00DF9998),
    (0x00732E40, 0x00C20E14, 0x00DF99BC, 0x00DF99B8),
    (0x0073CC70, 0x00C21B64, 0x00DF9ABC, 0x00DF9AB8),
    (0x0073DD80, 0x00C21F3C, 0x00DF9AC4, 0x00DF9AC0),
    (0x00740F20, 0x00C223CC, 0x00DF9ACC, 0x00DF9AC8),
    (0x00743770, 0x00C22658, 0x00DF9AE4, 0x00DF9AE0),
    (0x0074E7E0, 0x00C22EEC, 0x00DF9B68, 0x00DF9B64),
    (0x0074EA70, 0x00C232C4, 0x00DF9B70, 0x00DF9B6C),
    (0x0074EAB0, 0x00C236AC, 0x00DF9B78, 0x00DF9B74),
    (0x0074FBF0, 0x00C23B5C, 0x00DF9B80, 0x00DF9B7C),
    (0x00751C10, 0x00C2400C, 0x00DF9B88, 0x00DF9B84),
    (0x00751C80, 0x00C24434, 0x00DF9B90, 0x00DF9B8C),
    (0x00751CD0, 0x00C24864, 0x00DF9B98, 0x00DF9B94),
    (0x00751DE0, 0x00C24D04, 0x00DF9BA0, 0x00DF9B9C),
    (0x00751EB0, 0x00C25194, 0x00DF9BA8, 0x00DF9BA4),
    (0x00752340, 0x00C2564C, 0x00DF9BB0, 0x00DF9BAC),
    (0x00752410, 0x00C25A74, 0x00DF9BB8, 0x00DF9BB4),
    (0x00752450, 0x00C25E9C, 0x00DF9BC0, 0x00DF9BBC),
    (0x0075C880, 0x00C26A3C, 0x00DF9BF0, 0x00DF9BEC),
)

KEY_CASES = (
    (0x00000000, 0xEEEEEEEE, 0x00001000, 0x00000042, True),
    (0x00000001, 0x00000099, 0x00001000, 0x00000042, False),
    (0xFFFFFF00, 0x11111111, 0xABCDEF00, 0x80000000, True),
    (0x00000005, 0xFFFFFFFF, 0x00000000, 0x00000001, False),
)


class Checker:
    def __init__(self, executable: Path) -> None:
        self.process = subprocess.Popen(
            [str(executable)], stdin=subprocess.PIPE, stdout=subprocess.PIPE,
            text=True, bufsize=1)

    def call(self, line: str) -> str:
        assert self.process.stdin is not None and self.process.stdout is not None
        self.process.stdin.write(line + "\n")
        self.process.stdin.flush()
        result = self.process.stdout.readline().strip()
        if not result or result == "error":
            raise RuntimeError(f"gamelogic72check rejected {line}")
        return result

    def close(self) -> None:
        assert self.process.stdin is not None
        self.process.stdin.write("quit\n")
        self.process.stdin.flush()
        if self.process.wait(timeout=10) != 0:
            raise RuntimeError("gamelogic72check failed")


def parse_fields(line: str) -> dict[str, int]:
    fields = {}
    for part in line.split():
        key, value = part.split("=", 1)
        fields[key] = int(value, 16) if key != "called" else int(value, 10)
    return fields


def image_string(image: Image, va: int) -> str:
    blob = image.read(va, 96)
    return blob[:blob.index(0)].decode("ascii")


def run(checker: Checker, image: Image) -> tuple[int, int]:
    emu = Emulator(EXE)
    total = 0
    bad = 0
    seen = {"called": 0, "arg": 0, "mgr": 0, "eax": 0}

    def stub(emulator):
        seen["called"] += 1
        seen["mgr"] = emulator.register(uc.UC_X86_REG_ECX)
        seen["arg"] = emulator.stack_arg(0)
        return seen["eax"]

    emu.bind_function(NAMEKEY, stub, 4)
    for va, string_va, label in NAMES:
        if image_string(image, string_va) != label:
            raise RuntimeError(f"string at 0x{string_va:08X} is not {label}")
        raw = image.read(va, 6)
        if raw != struct.pack("<BI", 0xB8, string_va) + b"\xC3":
            raise RuntimeError(f"0x{va:08X} is not the name getter")
        port = parse_fields(checker.call(f"name {va:08X}"))
        for ecx in (0, 0xA5A5A5A5):
            total += 1
            result = emu.call(va, ecx=ecx)
            if result.eax != string_va or result.eax != port["eax"] or result.stack_cleanup != 0:
                bad += 1
                print(f"MISMATCH name {va:08X} ecx={ecx:08X} emu={result.eax:08X} port={port['eax']:08X} cleanup={result.stack_cleanup}")
    for va, string_va, guard_va, cache_va in KEYS:
        raw = image.read(va, 47)
        if struct.unpack_from("<I", raw, 26)[0] != string_va:
            raise RuntimeError(f"0x{va:08X} NAMEKEY string mismatch")
        for guard, cache, manager, stub_eax, expect_call in KEY_CASES:
            total += 1
            seen["called"] = 0
            seen["arg"] = 0
            seen["mgr"] = 0
            seen["eax"] = stub_eax
            emu.reset_data()
            emu.write(guard_va, struct.pack("<I", guard))
            emu.write(cache_va, struct.pack("<I", cache))
            emu.write(MANAGER, struct.pack("<I", manager))
            result = emu.call(va, ecx=0x22222222)
            emu_guard = struct.unpack("<I", emu.read(guard_va, 4))[0]
            emu_cache = struct.unpack("<I", emu.read(cache_va, 4))[0]
            port = parse_fields(checker.call(
                f"key {va:08X} {guard:08X} {cache:08X} {manager:08X} {stub_eax:08X}"))
            ok = (
                result.stack_cleanup == 0
                and result.eax == port["eax"]
                and emu_guard == port["guard"]
                and emu_cache == port["cache"]
                and seen["called"] == port["called"]
                and (seen["called"] == 1) == expect_call
            )
            if seen["called"]:
                ok = ok and seen["arg"] == string_va and seen["mgr"] == manager
                ok = ok and port["arg"] == string_va and port["mgr"] == manager
            if not ok:
                bad += 1
                print(
                    f"MISMATCH key {va:08X} guard={guard:08X} emu_eax={result.eax:08X} "
                    f"port_eax={port['eax']:08X} emu_guard={emu_guard:08X} port_guard={port['guard']:08X} "
                    f"emu_cache={emu_cache:08X} port_cache={port['cache']:08X} "
                    f"emu_called={seen['called']} port_called={port['called']}")
    return total, bad


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument(
        "--checker",
        type=Path,
        default=Path("Ra3Source/build/win32/bin/Release/ra3_gamelogic72check.exe"),
    )
    parser.add_argument("--mutation-checker", type=Path)
    args = parser.parse_args()
    image = Image(EXE)
    checker = Checker(args.checker)
    try:
        total, bad = run(checker, image)
    finally:
        checker.close()
    print(f"functions={len(NAMES) + len(KEYS)} scenarios={total} mismatches={bad}")
    if bad:
        return 1
    if args.mutation_checker is not None:
        mutant = Checker(args.mutation_checker)
        try:
            m_total, m_bad = run(mutant, image)
        finally:
            mutant.close()
        print(f"mutation scenarios={m_total} mismatches={m_bad} status={'PASS' if m_bad else 'FAIL'}")
        if m_bad == 0:
            return 1
    return 0


if __name__ == "__main__":
    sys.exit(main())
