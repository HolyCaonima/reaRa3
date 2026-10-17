"""Differentially verify the reconstructed RA3 weapon/object logic cluster.

The oracle executes only clean shipped .text in Unicorn. Protected entries in
the same address interval are deliberately absent. Every accepted entry checks
its callee cleanup and full EAX where it is a return value; stateful entries
also compare the entire prefilled object so writes outside the recovered range
are detected. ChargeWeaponUpdate compares all ten x87 ST0 bytes.
"""

from __future__ import annotations

import argparse
import random
import struct
import subprocess
import sys
from pathlib import Path

import unicorn
from unicorn import x86_const

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from emulate import Emulator, RETURN_SENTINEL  # noqa: E402


METADATA = (
    ("AssistedTargetingUpdate", 0x0069A250, 0x0069A260, 0x00C01CE0, 0x00DF8F2C, 0x00DF8F28),
    ("AttachUpdate", 0x0069A3C0, 0x0069A3D0, 0x00C01EB4, 0x00DF8F34, 0x00DF8F30),
    ("BoredUpdate", 0x0069AA10, 0x0069AA20, 0x00C021B8, 0x00DF8F3C, 0x00DF8F38),
    ("ChargeWeaponUpdate", 0x0069AB60, 0x0069AB70, 0x00C021C4, 0x00DF8F4C, 0x00DF8F48),
    ("DeletionUpdate", 0x0069AE80, 0x0069AE90, 0x00C02580, 0x00DF8F54, 0x00DF8F50),
    ("DemoTrapUpdate", 0x0069AF60, 0x0069AF70, 0x00C02728, 0x00DF8F5C, 0x00DF8F58),
    ("DestroyObjectsUpdate", 0x0069B040, 0x0069B050, 0x00C028D0, 0x00DF8F64, 0x00DF8F60),
    ("DestructibleEnvironmentUpdate", 0x0069B0C0, 0x0069B0D0, 0x00C02A80, 0x00DF8F6C, 0x00DF8F68),
    ("DieOnProducerDeathUpdate", 0x0069B2E0, 0x0069B2F0, 0x00C02C38, 0x00DF8F74, 0x00DF8F70),
    ("EMPUpdate", 0x0069B480, 0x0069B490, 0x00C02E60, 0x00DF8F7C, 0x00DF8F78),
    ("FireSpreadUpdate", 0x0069B560, 0x0069B570, 0x00C03018, 0x00DF8F8C, 0x00DF8F88),
    ("FlammableUpdate", 0x0069B630, 0x0069B640, 0x00C02E6C, 0x00DF8F94, 0x00DF8F90),
    ("FreefallUpdate", 0x0069B830, 0x0069B840, 0x00C03368, 0x00DF8F9C, 0x00DF8F98),
    ("GatherSlavesUpdate", 0x0069B9E0, 0x0069B9F0, 0x00C03510, 0x00DF8FAC, 0x00DF8FA8),
    ("GrapplingHookUpdate", 0x0069BAF0, 0x0069BB00, 0x00C03524, 0x00DF8FBC, 0x00DF8FB8),
    ("HeightDieUpdate", 0x0069BC80, 0x0069BC90, 0x00C03868, 0x00DF8FC4, 0x00DF8FC0),
    ("HijackerUpdate", 0x0069BDC0, 0x0069BDD0, 0x00C03A10, 0x00DF8FCC, 0x00DF8FC8),
    ("HordeNotifyTargetsOfImminentProbableCrushingUpdate", 0x0069BEE0, 0x0069BEF0, 0x00C03BB8, 0x00DF8FD4, 0x00DF8FD0),
    ("InitiateRepairDie", 0x0069BF90, 0x0069BFA0, 0x00C03D90, 0x00DF8FDC, 0x00DF8FD8),
    ("InvisibilityBroadcastUpdate", 0x0069C170, 0x0069C180, 0x00C03F48, 0x00DF8FE4, 0x00DF8FE0),
    ("InvisibilityUpdate", 0x0069C4B0, 0x0069C4C0, 0x00C04108, 0x00DF8FEC, 0x00DF8FE8),
    ("LargeGroupBonusUpdate", 0x0069C7F0, 0x0069C800, 0x00C04330, 0x00DF8FF4, 0x00DF8FF0),
    ("LaserScatterUpdate", 0x0069CA10, 0x0069CA20, 0x00BF8988, 0x00DF8FFC, 0x00DF8FF8),
    ("LoseHealthUpdate", 0x0069D1D0, 0x0069D1E0, 0x00C04768, 0x00DF9004, 0x00DF9000),
    ("PowerUpdate", 0x0069E2D0, 0x0069E2E0, 0x00BF8A94, 0x00DF903C, 0x00DF9038),
    ("ProneUpdate", 0x0069EA50, 0x0069EA60, 0x00C053B8, 0x00DF9044, 0x00DF9040),
    ("RadarUpdate", 0x0069EB30, 0x0069EB40, 0x00C05560, 0x00DF904C, 0x00DF9048),
    ("RepairUpdate", 0x0069ECE0, 0x0069ECF0, 0x00C05708, 0x00DF9054, 0x00DF9050),
    ("SetLocomotorSpeedUpdate", 0x0069F270, 0x0069F280, 0x00C05AE0, 0x00DF9064, 0x00DF9060),
    ("SlavedUpdate", 0x0069F840, 0x0069F850, 0x00C05E88, 0x00DF9074, 0x00DF9070),
)

FPS_GLOBAL = 0x00DEEEE8
GAME_GLOBAL = 0x00DEBA64
SCHEDULE = 0x004ED9C0
RANDOM_INTEGER = 0x005A94C0
CEIL_IAT = 0x00BBA4DC
CONTROL_WORDS = (0x007F, 0x027F, 0x037F)


class Checker:
    def __init__(self, executable: Path, mutated: bool) -> None:
        arguments = [str(executable)]
        if mutated:
            arguments.append("--mutated")
        self.process = subprocess.Popen(
            arguments, stdin=subprocess.PIPE, stdout=subprocess.PIPE,
            text=True, bufsize=1)

    def call(self, command: str) -> str:
        assert self.process.stdin is not None and self.process.stdout is not None
        self.process.stdin.write(command + "\n")
        self.process.stdin.flush()
        result = self.process.stdout.readline().strip()
        if not result or result == "error":
            raise RuntimeError(f"weaponlogiccheck rejected {command!r}")
        return result

    def close(self) -> None:
        assert self.process.stdin is not None
        self.process.stdin.write("quit\n")
        self.process.stdin.flush()
        if self.process.wait(timeout=10) != 0:
            raise RuntimeError("weaponlogiccheck failed")


def u32(value: int) -> int:
    return value & 0xFFFFFFFF


def p32(*values: int) -> bytes:
    return struct.pack("<" + "I" * len(values), *(u32(value) for value in values))


def bits(value: float) -> int:
    return struct.unpack("<I", struct.pack("<f", value))[0]


def f32(value: int) -> float:
    return struct.unpack("<f", struct.pack("<I", value))[0]


def x87_roundtrip_bits(value: int) -> int:
    if (value & 0x7F800000) == 0x7F800000 and (value & 0x007FFFFF) != 0:
        return value | 0x00400000
    return value


def random_blob(rng: random.Random, size: int) -> bytes:
    return bytes(rng.randrange(256) for _ in range(size))


def c_string(emu: Emulator, address: int) -> str:
    result = bytearray()
    while True:
        byte = emu.read(address + len(result), 1)[0]
        if byte == 0:
            return result.decode("ascii")
        result.append(byte)


def scalar(cpp: Checker, command: str) -> int:
    return int(cpp.call(command).split()[0])


def assert_call(name: str, game, cleanup: int, expected_eax: int | None = None) -> None:
    if game.stack_cleanup != cleanup:
        raise AssertionError(
            f"{name}: stack cleanup {game.stack_cleanup}, expected {cleanup}")
    if expected_eax is not None and game.eax != u32(expected_eax):
        raise AssertionError(
            f"{name}: EAX {game.eax:08x}, expected {u32(expected_eax):08x}")


def install_ceil_stub(emu: Emulator) -> None:
    # The positive LoseHealth cases use integral quotients, so FRNDINT and ceil
    # agree while still exercising the imported-call/x87 return path.
    address = 0x5F000000
    emu.uc.mem_map(address, 0x1000, unicorn.UC_PROT_ALL)
    emu.uc.mem_write(address, b"\xDD\x44\x24\x04\xD9\xFC\xC3")  # fld qword; frndint; ret
    emu.write(CEIL_IAT, p32(address))


def verify(executable: Path, cases: int, mutated: bool) -> dict[str, int]:
    rng = random.Random(0x57454150)
    emu = Emulator()
    install_ceil_stub(emu)
    cpp = Checker(executable, mutated)
    totals: dict[str, int] = {}

    def counted(name: str) -> None:
        totals[name] = totals.get(name, 0) + 1

    game_state = bytearray(0x1C8)
    game = emu.alloc(bytes(game_state))
    emu.write(GAME_GLOBAL, p32(game))
    emu.write(FPS_GLOBAL, p32(bits(30.0)))

    resolver_result = 0
    resolver_calls: list[tuple[int, str]] = []

    def resolver_stub(machine: Emulator) -> int:
        resolver_calls.append((machine.stack_arg(0), c_string(machine, machine.stack_arg(0))))
        return resolver_result

    emu.bind_function(0x00986150, resolver_stub, stack_cleanup=4)

    schedules: list[tuple[int, int, int]] = []

    def schedule_stub(machine: Emulator) -> int:
        schedules.append((
            machine.stack_arg(0), machine.stack_arg(1), machine.stack_arg(2)))
        return 0xA1B2C3D4

    emu.bind_function(SCHEDULE, schedule_stub, stack_cleanup=12)

    sampled_lifetime = 1

    def random_stub(machine: Emulator) -> int:
        low = machine.stack_arg(0)
        high = machine.stack_arg(1)
        if not (low <= sampled_lifetime <= high):
            raise AssertionError(
                f"LifetimeUpdate random range {low}..{high} excludes {sampled_lifetime}")
        return sampled_lifetime

    emu.bind_function(RANDOM_INTEGER, random_stub)

    try:
        # Sixty metadata entries: 30 exact name pointers and 30 guarded key
        # resolvers. The second key scenario proves no resolver call or write.
        for index, (name, name_va, key_va, name_address, guard_address, cache_address) in enumerate(METADATA):
            initial_edx = rng.getrandbits(32)
            game_result = emu.call(name_va, eax=rng.getrandbits(32), edx=initial_edx)
            ours = scalar(cpp, f"metadata_name {index}")
            assert_call(f"{name}.name", game_result, 0, ours)
            if game_result.edx != initial_edx or game_result.eax != name_address:
                raise AssertionError(f"{name}.name register contract differs")
            if c_string(emu, game_result.eax) != name:
                raise AssertionError(f"{name}.name returned wrong string")
            counted(f"{name}.name")

            for initialized in (False, True):
                guard = rng.getrandbits(32)
                if initialized:
                    guard |= 1
                else:
                    guard &= ~1
                cached = rng.getrandbits(32)
                resolver_result = rng.getrandbits(32)
                emu.write(cache_address, p32(cached))
                emu.write(guard_address, p32(guard))
                before_calls = len(resolver_calls)
                game_result = emu.call(
                    key_va, eax=rng.getrandbits(32), edx=initial_edx)
                ours = tuple(map(int, cpp.call(
                    f"metadata_key {index} {guard} {cached} {resolver_result}").split()))
                game_state_pair = (
                    struct.unpack("<I", emu.read(guard_address, 4))[0],
                    struct.unpack("<I", emu.read(cache_address, 4))[0])
                expected_calls = 0 if initialized else 1
                assert_call(f"{name}.key", game_result, 0, ours[0])
                if game_state_pair != (ours[1], ours[2]):
                    raise AssertionError(
                        f"{name}.key exact writes differ {game_state_pair}/{ours[1:3]}")
                if len(resolver_calls) - before_calls != expected_calls or ours[3] != expected_calls:
                    raise AssertionError(f"{name}.key resolver call count differs")
                if not initialized and resolver_calls[-1][1] != name:
                    raise AssertionError(f"{name}.key resolver argument differs")
                counted(f"{name}.key")

        for _ in range(cases):
            # 0x0069A610
            template_value = emu.alloc(random_blob(rng, 0x108))
            source_value = emu.alloc(random_blob(rng, 0x41C))
            candidate = emu.alloc(random_blob(rng, 0x41C))
            context = emu.alloc(random_blob(rng, 12))
            flags = rng.getrandbits(32)
            source_player = rng.getrandbits(32)
            candidate_player = rng.choice((source_player, rng.getrandbits(32)))
            emu.write(template_value + 0x104, p32(flags))
            emu.write(source_value + 0x418, p32(source_player))
            emu.write(candidate + 0x418, p32(candidate_player))
            emu.write(context + 4, p32(template_value, source_value))
            ours = scalar(cpp, " ".join(map(str, (
                "target_player", template_value, flags, source_value,
                source_player, candidate_player))))
            result = emu.call(0x0069A610, [candidate], ecx=context)
            assert_call("0069a610", result, 4, ours)
            counted("0069a610")

            # 0x0069A640
            config = emu.alloc(random_blob(rng, 0xD0))
            source = emu.alloc(random_blob(rng, 0xA0))
            target = emu.alloc(random_blob(rng, 0xA0))
            context = emu.alloc(random_blob(rng, 12))
            exclusions = [rng.getrandbits(32) for _ in range(7)]
            source_kinds = [rng.getrandbits(32) for _ in range(7)]
            requirements = [rng.getrandbits(32) for _ in range(7)]
            target_kinds = [rng.getrandbits(32) for _ in range(7)]
            emu.write(config + 0xB4, p32(*exclusions))
            emu.write(config + 0x98, p32(*requirements))
            emu.write(source + 0x80, p32(*source_kinds))
            emu.write(target + 0x80, p32(*target_kinds))
            emu.write(context + 4, p32(config, source))
            ours = scalar(cpp, "kind_mask "
                + " ".join(p32(*values).hex() for values in (
                    exclusions, source_kinds, requirements, target_kinds)))
            result = emu.call(0x0069A640, [target], ecx=context)
            assert_call("0069a640", result, 4, ours)
            counted("0069a640")

            # 0x0069A710
            flags = rng.getrandbits(32)
            emu.write(config + 0x104, p32(flags))
            emu.write(context + 4, p32(config))
            ours = scalar(cpp, f"los {flags}")
            result = emu.call(0x0069A710, ecx=context, edx=0x81726354)
            assert_call("0069a710", result, 0, ours)
            if result.edx != 0x81726354:
                raise AssertionError("0069a710 unexpectedly clobbered EDX")
            counted("0069a710")

            # 0x0069ABA0, all 80 x87 result bits and three control words.
            current_frame = rng.getrandbits(32)
            start_frame = rng.getrandbits(32)
            charge_frames = bits(rng.uniform(-10.0, 300.0))
            emu.write(game + 0x58, p32(current_frame))
            charge = emu.alloc(random_blob(rng, 0x30))
            emu.write(charge + 0x2C, p32(start_frame))
            for control_word in CONTROL_WORDS:
                if cpp.call(f"fpcw {control_word}") != "ok":
                    raise RuntimeError("checker rejected FPU control word")
                game_result, game_extended = emu.call_float80(
                    0x0069ABA0, [charge_frames], ecx=charge,
                    fpu_control_word=control_word)
                ours_words = cpp.call(
                    f"charge {current_frame} {start_frame} {charge_frames}").split()
                assert_call("0069aba0", game_result, 4)
                if game_extended != bytes.fromhex(ours_words[1]):
                    raise AssertionError(
                        f"0069aba0 x87 mismatch {game_extended.hex()}/{ours_words[1]}")
                counted("0069aba0")

            # 0x0069AC90
            used_ids = [rng.getrandbits(32) for _ in range(rng.randrange(5))]
            target_id = rng.choice(used_ids) if used_ids and rng.randrange(2) else rng.getrandbits(32)
            target_flags = rng.getrandbits(32)
            has_position = bool(rng.randrange(2))
            position_allowed = bool(rng.randrange(2))
            ids = emu.alloc(p32(*used_ids) if used_ids else b"\0")
            descriptor = emu.alloc(p32(ids, ids + len(used_ids) * 4))
            holder = emu.alloc(p32(descriptor, int(has_position)))
            cluster = emu.alloc(random_blob(rng, 12))
            cluster_target = emu.alloc(random_blob(rng, 0xE0))
            emu.write(cluster + 8, p32(holder))
            emu.write(cluster_target + 0x80, p32(target_flags))
            emu.write(cluster_target + 0xDC, p32(target_id))
            emu.bind_function(
                0x0085CBC0, lambda machine: int(position_allowed),
                stack_cleanup=16)
            ids_hex = p32(*used_ids).hex() if used_ids else ""
            # Empty vectors use a one-dword textual sentinel and count zero in
            # the checker; avoid ambiguity by exercising at least one ID.
            if not used_ids:
                used_ids = [rng.getrandbits(32)]
                emu.write(ids, p32(used_ids[0]))
                emu.write(descriptor, p32(ids, ids + 4))
                ids_hex = p32(*used_ids).hex()
            ours = scalar(cpp, f"cluster {target_flags} {ids_hex} {ids} {target_id} "
                               f"{int(has_position)} {int(position_allowed)}")
            result = emu.call(0x0069AC90, [cluster_target], ecx=cluster)
            assert_call("0069ac90", result, 4, ours)
            counted("0069ac90")

            # 0x0069B100
            local = [bits(rng.uniform(-20.0, 20.0)) for _ in range(6)]
            matrix = [bits(rng.uniform(-5.0, 5.0)) for _ in range(12)]
            local_address = emu.alloc(p32(0, 0, 0, *local))
            matrix_address = emu.alloc(p32(0, 0, *matrix))
            storage = emu.alloc(random_blob(rng, 0x50))
            callback = storage + 0x10
            enabled = rng.randrange(2)
            emu.write(storage + 4, p32(local_address, matrix_address))
            emu.write(callback + 0x2C, bytes([enabled]))
            before = emu.read(storage, 0x50)
            output_seed = before[0x24:0x3C]
            ours = bytes.fromhex(cpp.call(
                f"transform {enabled} {p32(*local).hex()} {p32(*matrix).hex()} "
                f"{output_seed.hex()}"))
            result = emu.call(0x0069B100, ecx=callback)
            after = emu.read(storage, 0x50)
            expected = bytearray(before)
            expected[0x24:0x3C] = ours
            assert_call("0069b100", result, 0, 1)
            if after != bytes(expected):
                raise AssertionError(
                    f"0069b100 exact writes differ game={after.hex()} "
                    f"ours={bytes(expected).hex()}")
            counted("0069b100")

            # The output fields can alias the local-point storage. The native
            # body finishes loading each point before writing that point.
            alias_storage = emu.alloc(random_blob(rng, 0x50))
            alias_callback = alias_storage + 0x10
            emu.write(alias_callback - 0x0C, p32(alias_callback + 8))
            emu.write(alias_callback - 0x08, p32(matrix_address))
            emu.write(alias_callback + 0x14, p32(*local))
            emu.write(alias_callback + 0x2C, b"\x01")
            alias_before = emu.read(alias_storage, 0x50)
            alias_ours = bytes.fromhex(cpp.call(
                f"transform_alias {p32(*local).hex()} {p32(*matrix).hex()}"))
            result = emu.call(0x0069B100, ecx=alias_callback)
            alias_expected = bytearray(alias_before)
            alias_expected[0x24:0x3C] = alias_ours
            assert_call("0069b100 alias", result, 0, 1)
            alias_after = emu.read(alias_storage, 0x50)
            if alias_after != bytes(alias_expected):
                raise AssertionError(
                    f"0069b100 aliased exact writes differ "
                    f"game={alias_after.hex()} ours={bytes(alias_expected).hex()}")
            counted("0069b100")

            # 0x0069B930, one outer range containing one inner range.
            object_values = []
            target_ids = []
            records = emu.alloc(random_blob(rng, 0x58 * 4))
            record_count = rng.randrange(1, 5)
            for index in range(record_count):
                object_values.append(records + index * 0x58)
                target_ids.append(rng.getrandbits(32))
                emu.write(records + index * 0x58 + 0x4C, p32(target_ids[-1]))
            inner = emu.alloc(p32(records, records + record_count * 0x58, 0, 0))
            outer = emu.alloc(p32(inner, inner + 16, 0, 0))
            freefall = emu.alloc(random_blob(rng, 0x2C))
            emu.write(freefall + 0x24, p32(outer, outer + 16))
            candidate_id = rng.choice(target_ids + [rng.getrandbits(32)])
            candidate = emu.alloc(random_blob(rng, 0xE0))
            emu.write(candidate + 0xDC, p32(candidate_id))
            ours = scalar(cpp, f"freefall {p32(*object_values).hex()} "
                               f"{p32(*target_ids).hex()} "
                               f"{records + record_count * 0x58} {candidate_id}")
            result = emu.call(0x0069B930, [candidate], eax=0, ecx=freefall)
            assert_call("0069b930", result, 4, ours)
            counted("0069b930")

            # LifetimeUpdate 0x0069CD30.
            current_frame = rng.randrange(1000, 50000)
            final_frame = u32(current_frame + rng.randrange(1, 10000))
            speed = rng.choice((0.5, 1.0, 2.0))
            delay = rng.uniform(0.0, 5.0)
            tick = rng.uniform(0.01, 2.0)
            minimum = rng.randrange(2)
            maximum = rng.randrange(2)
            has_limit = rng.randrange(2)
            limit = rng.randrange(0, 100)
            config_blob = bytearray(random_blob(rng, 0x28))
            struct.pack_into("<IIff", config_blob, 0x14, minimum, maximum, delay, tick)
            limit_address = emu.alloc(p32(limit))
            struct.pack_into("<I", config_blob, 0x24, limit_address if has_limit else 0)
            lifetime_config = emu.alloc(bytes(config_blob))
            lifetime_blob = bytearray(random_blob(rng, 0x34))
            struct.pack_into("<I", lifetime_blob, 4, lifetime_config)
            struct.pack_into("<I", lifetime_blob, 0x28, final_frame)
            lifetime = emu.alloc(bytes(lifetime_blob))
            emu.write(game + 0x58, p32(current_frame))
            emu.write(game + 0x1C4, p32(bits(speed)))
            ours = scalar(cpp, " ".join(map(str, (
                "lifetime_secondary", minimum, maximum, bits(delay), bits(tick),
                has_limit, limit, current_frame, final_frame, bits(30.0), bits(speed)))))
            result = emu.call(0x0069CD30, ecx=lifetime)
            assert_call("0069cd30", result, 0)
            after = emu.read(lifetime, 0x34)
            expected = bytearray(lifetime_blob)
            struct.pack_into("<I", expected, 0x30, ours)
            if after != bytes(expected):
                raise AssertionError(
                    f"0069cd30 exact writes differ current={current_frame} "
                    f"final={final_frame} speed={speed} delay={delay} tick={tick} "
                    f"min={minimum} max={maximum} limit={has_limit}/{limit} "
                    f"game={after.hex()} ours={bytes(expected).hex()}")
            counted("0069cd30")

            # 0x0069CE80 enters the native body at 0x0069CE8A after its clean
            # continuation shim, then performs random sampling and CD30.
            low = rng.randrange(1, 500)
            high = low + rng.randrange(0, 500)
            # The native continuation passes the second entry argument as
            # both range endpoints; the first stack argument is ABI padding.
            sampled_lifetime = high
            # Directly enter the native continuation with the helper-line and
            # saved-ESI words normally supplied by the clean entry shim. This
            # avoids executing or modeling the SecuROM continuation dispatcher.
            result = emu.call(
                0x0069CE8A, [0, RETURN_SENTINEL, low, high],
                ecx=lifetime, eax=high, _return_address=0)
            secondary = struct.unpack_from(
                "<I", emu.read(lifetime, 0x34), 0x30)[0]
            ours_delay = scalar(
                cpp, f"lifetime_start {current_frame} {sampled_lifetime} {secondary}")
            assert_call("0069ce80", result, 16, ours_delay)
            if struct.unpack_from("<I", emu.read(lifetime, 0x34), 0x2C)[0] != current_frame:
                raise AssertionError("0069ce80 start-frame write differs")
            counted("0069ce80")

            # 0x0069CEF0/0x0069CF20 schedule through 0x004ED9C0.
            timer_storage = emu.alloc(random_blob(rng, 0x40))
            lifetime_subobject = timer_storage + 0x1C
            owner_value = rng.getrandbits(32)
            emu.write(timer_storage, p32(owner_value))
            emu.write(game + 0x58, p32(current_frame))
            schedules.clear()
            pause_words = tuple(map(int, cpp.call(
                f"lifetime_pause {emu.read(lifetime_subobject + 0x11, 1)[0]} "
                f"{current_frame}").split()))
            result = emu.call(0x0069CEF0, [rng.getrandbits(32)], ecx=lifetime_subobject)
            assert_call("0069cef0", result, 4)
            if emu.read(lifetime_subobject + 0x11, 1)[0] != pause_words[1]:
                raise AssertionError("0069cef0 paused-byte write differs")
            if len(schedules) != 1 or schedules[0] != (
                    owner_value, lifetime_subobject - 0x14, pause_words[0]):
                raise AssertionError(f"0069cef0 scheduler args differ {schedules}")
            counted("0069cef0")

            final_frame = u32(current_frame + rng.randrange(1, 500))
            secondary = rng.choice((0, u32(current_frame + rng.randrange(1, 500))))
            emu.write(lifetime_subobject + 4, p32(final_frame))
            emu.write(lifetime_subobject + 0x0C, p32(secondary))
            schedules.clear()
            resume_words = tuple(map(int, cpp.call(
                f"lifetime_resume 1 {current_frame} {final_frame} {secondary}").split()))
            result = emu.call(0x0069CF20, [rng.getrandbits(32)], ecx=lifetime_subobject)
            assert_call("0069cf20", result, 4)
            if emu.read(lifetime_subobject + 0x11, 1)[0] != resume_words[1]:
                raise AssertionError("0069cf20 paused-byte write differs")
            if len(schedules) != 1 or schedules[0][2] != resume_words[0]:
                raise AssertionError("0069cf20 scheduler deadline differs")
            counted("0069cf20")

            # 0x0069D0F0. Integral positive ratios make the local FRNDINT stub
            # exactly equivalent to the imported ceil; zero/negative paths skip it.
            pulse = rng.choice((1.0, 2.0, 4.0, -1.0))
            multiplier = rng.randrange(0, 20)
            accumulated = pulse * multiplier if pulse > 0 else rng.uniform(-10.0, 0.0)
            requested = rng.randrange(0, 30)
            lose_config = emu.alloc(random_blob(rng, 0x20))
            lose = emu.alloc(random_blob(rng, 0x54))
            emu.write(lose_config + 0x1C, p32(bits(pulse)))
            emu.write(lose + 4, p32(lose_config))
            emu.write(lose + 0x50, p32(bits(accumulated)))
            ours = scalar(cpp, f"lose_health {bits(accumulated)} {bits(pulse)} {requested}")
            result = emu.call(0x0069D0F0, [requested], ecx=lose)
            assert_call("0069d0f0", result, 4, ours)
            counted("0069d0f0")

            # Physics collision callbacks 0x0069E0D0/0x0069E120.
            object_mask = rng.getrandbits(32)
            accepted_mask = rng.getrandbits(32)
            physics_object = emu.alloc(random_blob(rng, 0x270))
            emu.write(physics_object + 0x26C, p32(object_mask))
            accepted_mask_address = emu.alloc(p32(accepted_mask))
            mask_vtable = emu.alloc(p32(0, 0, emu.bind_stub(
                lambda machine: accepted_mask_address, stack_cleanup=4)))
            collision_storage = emu.alloc(random_blob(rng, 0xF0))
            collision = collision_storage + 0x1C
            emu.write(collision_storage, p32(physics_object))
            emu.write(collision_storage + 8, p32(mask_vtable))
            initial_flag = rng.randrange(256)
            emu.write(collision + 0xCC, bytes([initial_flag]))
            ours = scalar(
                cpp, f"collision_set {initial_flag} {object_mask} {accepted_mask}")
            result = emu.call(0x0069E0D0, [rng.getrandbits(32)], ecx=collision)
            assert_call("0069e0d0", result, 4)
            if emu.read(collision + 0xCC, 1)[0] != ours:
                raise AssertionError("0069e0d0 flag write differs")
            counted("0069e0d0")
            initial_flag = rng.randrange(256)
            emu.write(collision + 0xCC, bytes([initial_flag]))
            ours = scalar(
                cpp, f"collision_clear {initial_flag} {object_mask} {accepted_mask}")
            result = emu.call(0x0069E120, [rng.getrandbits(32)], ecx=collision)
            assert_call("0069e120", result, 4)
            if emu.read(collision + 0xCC, 1)[0] != ours:
                raise AssertionError("0069e120 flag write differs")
            counted("0069e120")

            # 0x0069E190, including exact quieting of occasional signaling NaNs.
            transform_words = [rng.getrandbits(32) for _ in range(12)]
            contact_words = [rng.getrandbits(32) for _ in range(12)]
            physics_owner = emu.alloc(p32(0, 0, *transform_words))
            contact = emu.alloc(p32(*contact_words))
            physics_blob = bytearray(random_blob(rng, 0xF4))
            struct.pack_into("<I", physics_blob, 8, physics_owner)
            physics = emu.alloc(bytes(physics_blob))
            ours_words = cpp.call(
                f"capture_transforms {p32(*transform_words).hex()} "
                f"{p32(*contact_words).hex()}").split()
            result = emu.call(0x0069E190, [contact], ecx=physics)
            assert_call("0069e190", result, 4)
            expected = bytearray(physics_blob)
            hardware_object = p32(*(x87_roundtrip_bits(value) for value in transform_words))
            hardware_contact = p32(*(x87_roundtrip_bits(value) for value in contact_words))
            if (bytes.fromhex(ours_words[0]) != hardware_object
                    or bytes.fromhex(ours_words[1]) != hardware_contact):
                raise AssertionError("0069e190 native x87 quieting differs")
            expected[0x68:0x98] = hardware_object
            expected[0x98:0xC8] = hardware_contact
            expected[0xF1] = int(ours_words[2])
            # Unicorn does not quiet signaling NaNs on fld/fstp. Normalize only
            # those 24 written dwords to the native-x87 result before the exact
            # whole-object comparison.
            game_after = bytearray(emu.read(physics, len(expected)))
            game_after[0x68:0x98] = hardware_object
            game_after[0x98:0xC8] = hardware_contact
            if bytes(game_after) != bytes(expected):
                raise AssertionError("0069e190 exact writes/x87 quieting differ")
            counted("0069e190")

            # 0x0069E3C0
            count = rng.randrange(-5, 20)
            source_value = rng.randrange(-10000, 10000)
            scale_blob = bytearray(random_blob(rng, 0x70))
            struct.pack_into("<i", scale_blob, 0x30, source_value)
            scale = emu.alloc(bytes(scale_blob))
            inverse_bits, scaled_bits = map(
                int, cpp.call(f"scale_group {count} {source_value}").split())
            result = emu.call(0x0069E3C0, [u32(count)], ecx=scale)
            assert_call("0069e3c0", result, 4)
            expected = bytearray(scale_blob)
            struct.pack_into("<I", expected, 0x20, inverse_bits)
            struct.pack_into("<I", expected, 0x6C, scaled_bits)
            if emu.read(scale, len(expected)) != bytes(expected):
                raise AssertionError("0069e3c0 exact writes differ")
            counted("0069e3c0")

            # 0x0069E400
            status = rng.getrandbits(32)
            status_object = emu.alloc(random_blob(rng, 0xC4))
            emu.write(status_object + 0xC0, p32(status))
            ours = scalar(cpp, f"status9 {status}")
            result = emu.call(0x0069E400, [status_object], edx=0xCAFEBABE)
            assert_call("0069e400", result, 0, ours)
            if result.edx != 0xCAFEBABE:
                raise AssertionError("0069e400 unexpectedly clobbered EDX")
            counted("0069e400")

            # 0x0069E420
            player_id = rng.getrandbits(32)
            controller = emu.alloc(random_blob(rng, 0x24))
            owner = emu.alloc(random_blob(rng, 0x41C))
            physics_blob = bytearray(random_blob(rng, 0x130))
            physics = emu.alloc(bytes(physics_blob))
            emu.write(controller + 0x20, p32(player_id))
            emu.write(owner + 0x418, p32(controller))
            emu.write(physics + 8, p32(owner))
            checker_seed = bytearray(physics_blob)
            struct.pack_into("<I", checker_seed, 8, 0)
            ours = bytes.fromhex(cpp.call(
                f"copy_player {player_id} {bytes(checker_seed).hex()}"))
            result = emu.call(0x0069E420, ecx=physics)
            assert_call("0069e420", result, 0)
            expected = bytearray(physics_blob)
            struct.pack_into("<I", expected, 8, owner)
            struct.pack_into("<I", expected, 0x12C, player_id)
            if emu.read(physics, len(expected)) != bytes(expected):
                raise AssertionError("0069e420 exact writes differ")
            # The checker uses host pointers at +8; normalize that untouched field.
            ours = bytearray(ours)
            ours[8:12] = p32(owner)
            if bytes(ours) != bytes(expected):
                raise AssertionError("0069e420 checker writes differ")
            counted("0069e420")

            # 0x0069E820
            overlap_owner = rng.getrandbits(32)
            overlap_values = [rng.getrandbits(32) for _ in range(rng.randrange(0, 8))]
            ignored = [rng.randrange(2) for _ in overlap_values]
            nodes = [
                emu.alloc(random_blob(rng, 0x68))
                for _ in overlap_values
            ]
            for index, node in enumerate(nodes):
                emu.write(node + 0x38, p32(overlap_values[index]))
                emu.write(node + 0x54, p32(
                    nodes[index + 1] if index + 1 < len(nodes) else 0))
                emu.write(node + 0x65, bytes([ignored[index]]))
            physics_list = emu.alloc(random_blob(rng, 0x14))
            emu.write(physics_list + 0x10, p32(nodes[0] if nodes else 0))
            owners_hex = p32(*overlap_values).hex() if overlap_values else ""
            ignored_hex = bytes(ignored).hex() if ignored else ""
            if not overlap_values:
                # The line protocol has no empty-token representation.
                overlap_values = [rng.getrandbits(32)]
                ignored = [1]
                node = emu.alloc(random_blob(rng, 0x68))
                emu.write(node + 0x38, p32(overlap_values[0]))
                emu.write(node + 0x54, p32(0))
                emu.write(node + 0x65, b"\x01")
                emu.write(physics_list + 0x10, p32(node))
                owners_hex = p32(*overlap_values).hex()
                ignored_hex = bytes(ignored).hex()
            ours = scalar(cpp, f"count_overlaps {overlap_owner} "
                               f"{owners_hex} {ignored_hex}")
            result = emu.call(0x0069E820, [overlap_owner], ecx=physics_list)
            assert_call("0069e820", result, 4, ours)
            counted("0069e820")

            # 0x0069E850
            has_object = rng.randrange(2)
            has_controller = rng.randrange(2)
            height = rng.uniform(-200.0, 300.0)
            physics_predicate = emu.alloc(random_blob(rng, 12))
            height_object = emu.alloc(random_blob(rng, 0x20))
            predicate_owner = emu.alloc(random_blob(rng, 0x41C))
            if has_object:
                emu.write(physics_predicate + 8, p32(predicate_owner))
                emu.write(predicate_owner + 0x418, p32(
                    rng.getrandbits(32) | 1 if has_controller else 0))
            else:
                emu.write(physics_predicate + 8, p32(0))
            emu.write(height_object + 0x1C, p32(bits(height)))
            ours = scalar(cpp, f"controller_height {has_object} "
                               f"{has_controller} {predicate_owner if has_object else 0} "
                               f"{height_object} {bits(height)}")
            result = emu.call(0x0069E850, [height_object], ecx=physics_predicate)
            assert_call("0069e850", result, 4, ours)
            counted("0069e850")

            # 0x0069E8E0
            powered = rng.randrange(2)
            power_storage = emu.alloc(random_blob(rng, 0x50))
            power = power_storage + 0x1C
            owner_value = rng.getrandbits(32)
            emu.write(power_storage, p32(owner_value))
            emu.write(game + 0x58, p32(current_frame))
            schedules.clear()
            ours = scalar(cpp, f"power_wake {powered} {current_frame}")
            result = emu.call(0x0069E8E0, [powered], ecx=power)
            assert_call("0069e8e0", result, 4)
            if emu.read(power + 0x30, 1)[0] != powered:
                raise AssertionError("0069e8e0 powered-byte write differs")
            if len(schedules) != 1 or schedules[0][2] != ours:
                raise AssertionError("0069e8e0 scheduler deadline differs")
            counted("0069e8e0")

            # 0x0069CA50
            delay = rng.uniform(0.0, 5.0)
            speed = rng.choice((0.5, 1.0, 2.0))
            laser_config = emu.alloc(random_blob(rng, 12))
            laser_owner = emu.alloc(random_blob(rng, 0xF8))
            drawable = emu.alloc(random_blob(rng, 0x140))
            laser_blob = bytearray(random_blob(rng, 0x3C))
            laser = emu.alloc(bytes(laser_blob))
            emu.write(laser_config + 8, p32(bits(delay)))
            emu.write(laser_owner + 0xF4, p32(drawable))
            emu.write(laser + 4, p32(laser_config))
            emu.write(laser + 8, p32(laser_owner))
            emu.write(game + 0x58, p32(current_frame))
            emu.write(game + 0x1C4, p32(bits(speed)))
            schedules.clear()
            ours_words = tuple(map(int, cpp.call(
                f"laser {bits(delay)} {bits(30.0)} {bits(speed)} {current_frame} "
                f"{laser_blob[0x38]} {laser_blob[0x39]}").split()))
            result = emu.call(0x0069CA50, ecx=laser)
            assert_call("0069ca50", result, 0)
            after = emu.read(laser, len(laser_blob))
            if after[0x38] != ours_words[1] or after[0x39] != ours_words[2]:
                raise AssertionError("0069ca50 state-byte writes differ")
            if len(schedules) != 1 or schedules[0][2] != ours_words[0]:
                raise AssertionError("0069ca50 scheduler deadline differs")
            if emu.read(drawable + 0x13E, 1)[0] & 0x80 == 0:
                raise AssertionError("0069ca50 drawable status write missing")
            counted("0069ca50")

            # 0x0069F450, exact output bytes and no-write miss path.
            sphere_words = [
                bits(rng.uniform(-10.0, 10.0)),
                bits(rng.uniform(-10.0, 10.0)),
                bits(rng.uniform(-10.0, 10.0)),
                bits(rng.uniform(0.1, 10.0)),
            ]
            start_words = [bits(rng.uniform(-20.0, 20.0)) for _ in range(3)]
            end_words = [bits(rng.uniform(-20.0, 20.0)) for _ in range(3)]
            output_seed = random_blob(rng, 12)
            sphere = emu.alloc(p32(*sphere_words))
            start = emu.alloc(p32(*start_words))
            end = emu.alloc(p32(*end_words))
            output = emu.alloc(output_seed)
            ours_words = cpp.call(
                f"shield {p32(*sphere_words).hex()} {p32(*start_words).hex()} "
                f"{p32(*end_words).hex()} {output_seed.hex()} {start} {output}").split()
            result = emu.call(0x0069F450, [start, end, output], ecx=sphere)
            assert_call("0069f450", result, 12, int(ours_words[0]))
            if emu.read(output, 12) != bytes.fromhex(ours_words[1]):
                raise AssertionError(
                    f"0069f450 output differs {emu.read(output,12).hex()}/{ours_words[1]}")
            counted("0069f450")

            # The hit output is permitted to alias the segment start.
            alias_ours = cpp.call(
                f"shield_alias_start {p32(*sphere_words).hex()} "
                f"{p32(*start_words).hex()} {p32(*end_words).hex()} {start}").split()
            emu.write(start, p32(*start_words))
            result = emu.call(0x0069F450, [start, end, start], ecx=sphere)
            assert_call("0069f450 alias", result, 12, int(alias_ours[0]))
            if emu.read(start, 12) != bytes.fromhex(alias_ours[1]):
                raise AssertionError("0069f450 aliased output differs")
            counted("0069f450")

            # 0x0069F760
            object_blob = random_blob(rng, 0x12F8)
            slave_object = emu.alloc(object_blob)
            ours = bytes.fromhex(cpp.call(f"clear_links {object_blob.hex()}"))
            result = emu.call(0x0069F760, ecx=slave_object, edx=0x11223344)
            assert_call("0069f760", result, 0, 0)
            if result.edx != 0x11223344 or emu.read(slave_object, len(object_blob)) != ours:
                raise AssertionError("0069f760 register or exact writes differ")
            counted("0069f760")

            # 0x0069F770
            master_words = [rng.getrandbits(32) for _ in range(3)]
            offset_words = [rng.getrandbits(32) for _ in range(3)]
            has_offset = rng.randrange(2)
            master = emu.alloc(random_blob(rng, 0x44))
            slaved_config = emu.alloc(random_blob(rng, 0x90))
            offset = emu.alloc(p32(*offset_words))
            slaved = emu.alloc(random_blob(rng, 12))
            output_seed = random_blob(rng, 12)
            output = emu.alloc(output_seed)
            emu.write(master + 0x38, p32(*master_words))
            emu.write(slaved_config + 0x8C, p32(offset if has_offset else 0))
            emu.write(slaved + 4, p32(slaved_config, master))
            ours = bytes.fromhex(cpp.call(
                f"pinned {p32(*master_words).hex()} {p32(*offset_words).hex()} "
                f"{output_seed.hex()} {has_offset}"))
            result = emu.call(0x0069F770, [output], ecx=slaved)
            assert_call("0069f770", result, 4)
            if emu.read(output, 12) != ours:
                raise AssertionError("0069f770 output differs")
            counted("0069f770")

            # Verify both meaningful output aliases. Native writes the master
            # first, then reloads the configured offset.
            master_alias_ours = bytes.fromhex(cpp.call(
                f"pinned_alias_master {p32(*master_words).hex()} "
                f"{p32(*offset_words).hex()}"))
            emu.write(master + 0x38, p32(*master_words))
            emu.write(offset, p32(*offset_words))
            emu.write(slaved_config + 0x8C, p32(offset))
            result = emu.call(0x0069F770, [master + 0x38], ecx=slaved)
            assert_call("0069f770 master alias", result, 4)
            if emu.read(master + 0x38, 12) != master_alias_ours:
                raise AssertionError("0069f770 master-aliased output differs")
            counted("0069f770")

            offset_alias_ours = bytes.fromhex(cpp.call(
                f"pinned_alias_offset {p32(*master_words).hex()} "
                f"{p32(*offset_words).hex()}"))
            emu.write(master + 0x38, p32(*master_words))
            emu.write(offset, p32(*offset_words))
            result = emu.call(0x0069F770, [offset], ecx=slaved)
            assert_call("0069f770 offset alias", result, 4)
            if emu.read(offset, 12) != offset_alias_ours:
                raise AssertionError("0069f770 offset-aliased output differs")
            counted("0069f770")

            # 0x0069F8D0
            points_words = [
                rng.getrandbits(32) for _ in range(rng.randrange(1, 6) * 3)]
            position_words = [rng.getrandbits(32) for _ in range(3)]
            radius = bits(rng.uniform(0.0, 20.0))
            points = emu.alloc(p32(*points_words))
            position = emu.alloc(p32(*position_words))
            guard_config = emu.alloc(random_blob(rng, 0x20))
            guard = emu.alloc(random_blob(rng, 0x2C))
            emu.write(guard_config + 0x1C, p32(radius))
            emu.write(guard + 4, p32(guard_config))
            emu.write(guard + 0x24, p32(points, points + len(points_words) * 4))
            ours = scalar(cpp, f"guard_area {p32(*points_words).hex()} "
                               f"{p32(*position_words).hex()} {radius} {points}")
            result = emu.call(0x0069F8D0, [position], ecx=guard)
            assert_call("0069f8d0", result, 4, ours)
            counted("0069f8d0")

            # 0x0069F9E0
            configuration = rng.getrandbits(32)
            status_a = rng.getrandbits(32)
            status_b = rng.getrandbits(32)
            status_c = rng.getrandbits(32)
            slaved_config = emu.alloc(random_blob(rng, 0x88))
            slaved_object = emu.alloc(random_blob(rng, 0xC4))
            slaved = emu.alloc(random_blob(rng, 12))
            emu.write(slaved_config + 0x84, p32(configuration))
            emu.write(slaved_object + 0xA0, p32(status_a))
            emu.write(slaved_object + 0xB0, p32(status_b))
            emu.write(slaved_object + 0xC0, p32(status_c))
            emu.write(slaved + 4, p32(slaved_config, slaved_object))
            ours = scalar(cpp, f"slaved_state {configuration} {status_a} "
                               f"{status_b} {status_c}")
            result = emu.call(0x0069F9E0, [rng.getrandbits(32)], ecx=slaved)
            if result.stack_cleanup != 4 or result.eax != ours:
                raise AssertionError(
                    f"0069f9e0 cfg={configuration:08x} a={status_a:08x} "
                    f"b={status_b:08x} c={status_c:08x} "
                    f"game={result.eax:08x} ours={ours:08x}")
            counted("0069f9e0")

            # 0x0069FC50
            configuration = rng.getrandbits(32)
            slaved_config = emu.alloc(random_blob(rng, 0x30))
            slaved = emu.alloc(random_blob(rng, 8))
            emu.write(slaved_config + 0x2C, p32(configuration))
            emu.write(slaved + 4, p32(slaved_config))
            ours = scalar(cpp, f"config22 {configuration}")
            result = emu.call(0x0069FC50, ecx=slaved, edx=0x55667788)
            assert_call("0069fc50", result, 0, ours)
            if result.edx != 0x55667788:
                raise AssertionError("0069fc50 unexpectedly clobbered EDX")
            counted("0069fc50")

            # 0x0069FC60
            has_module = rng.randrange(2)
            configured_value = rng.randrange(256)
            configuration = rng.getrandbits(32)
            object_value = emu.alloc(random_blob(rng, 0x378))
            config_value = emu.alloc(random_blob(rng, 0xF0))
            slaved = emu.alloc(random_blob(rng, 12))
            emu.write(object_value + 0x374, p32(rng.getrandbits(32) | 1 if has_module else 0))
            emu.write(config_value + 0x2C, p32(configuration))
            emu.write(config_value + 0xEE, bytes([configured_value]))
            emu.write(slaved + 4, p32(config_value, object_value))
            ours = scalar(cpp, f"rider {object_value} {has_module} {config_value} "
                               f"{configuration} {configured_value}")
            result = emu.call(0x0069FC60, ecx=slaved)
            assert_call("0069fc60", result, 0, ours)
            counted("0069fc60")

            # 0x0069FD30
            template_flags = rng.getrandbits(32)
            template = emu.alloc(random_blob(rng, 0xE4))
            slaved = emu.alloc(random_blob(rng, 8))
            output_seed = rng.getrandbits(32)
            output = emu.alloc(p32(output_seed))
            emu.write(template + 0xE0, p32(template_flags))
            emu.write(slaved + 4, p32(template))
            ours = scalar(cpp, f"copy_flags {template_flags} {output_seed}")
            result = emu.call(0x0069FD30, [output], ecx=slaved, edx=0x99AABBCC)
            assert_call("0069fd30", result, 4)
            if emu.read(output, 4) != p32(ours) or result.edx != 0x99AABBCC:
                raise AssertionError("0069fd30 register or exact write differs")
            counted("0069fd30")

        return totals
    finally:
        cpp.close()


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("checker", type=Path)
    parser.add_argument("--cases", type=int, default=50)
    parser.add_argument("--expect-mutation", action="store_true")
    args = parser.parse_args()
    if args.cases <= 0:
        parser.error("--cases must be positive")

    if args.expect_mutation:
        try:
            verify(args.checker, min(args.cases, 2), True)
        except AssertionError as error:
            print(f"weapon logic mutation rejected: {error}")
            return 0
        print("weapon logic mutation was not detected", file=sys.stderr)
        return 1

    totals = verify(args.checker, args.cases, False)
    accepted = len(totals)
    comparisons = sum(totals.values())
    print(
        f"weapon logic oracle: {accepted} accepted entries, "
        f"{comparisons} comparisons, 0 mismatches")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
