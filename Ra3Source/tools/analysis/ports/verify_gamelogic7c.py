"""Differential oracle for the clean GameLogic 0x007C AIStateMachine & AI State cluster."""

from __future__ import annotations

import argparse
import random
import struct
import subprocess
import sys
from dataclasses import dataclass
from pathlib import Path

from unicorn import x86_const as x86

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from emulate import Emulator  # noqa: E402


ARENA_SIZE = 0x2000


def u32(value: int) -> int:
    return value & 0xFFFFFFFF


def p32(value: int) -> bytes:
    return struct.pack("<I", u32(value))


def fbits(value: float) -> int:
    return struct.unpack("<I", struct.pack("<f", value))[0]


def load32(data: bytes | bytearray, offset: int) -> int:
    return struct.unpack_from("<I", data, offset)[0]


def put32(data: bytearray, offset: int, value: int) -> None:
    data[offset : offset + 4] = p32(value)


def put8(data: bytearray, offset: int, value: int) -> None:
    data[offset] = value & 0xFF


@dataclass(frozen=True)
class Spec:
    va: int
    name: str
    cleanup: int = 0
    result_kind: str = "u32"  # u32, void, ptr
    cases: int = 8


SPECS: list[Spec] = [
    Spec(0x007C0AD0, "AIMoveToState::onEnter", 0, "u32", 12),
    Spec(0x007C0BD0, "AIMoveToStateSA::onEnter", 0, "u32", 8),
    Spec(0x007C0C80, "AIMoveOutOfTheWayState::onEnter", 0, "u32", 8),
    Spec(0x007C0D00, "AIMoveAndTightenState::onEnter", 0, "u32", 6),
    Spec(0x007C0D70, "AIMoveAwayPanicState::onEnter", 0, "u32", 8),
    Spec(0x007C0DF0, "AIMoveAwayFromRepulsorsState::onEnter", 0, "u32", 8),
    Spec(0x007C0E80, "AIBackAwayAndCowerState::onEnter", 0, "u32", 8),
    Spec(0x007C0EE0, "AIMoveAwayAndCowerState::onEnter", 0, "u32", 8),
    Spec(0x007C2C80, "AIPickUpCrateState::update", 0, "u32", 6),
    Spec(0x007C3520, "AIMoveAndEvacuateState::onEnter", 0, "u32", 6),
    Spec(0x007C3680, "AIMoveAndDeleteState::onEnter", 0, "u32", 10),
    Spec(0x007C39C0, "AIFollowWaypointPathStateAndEvacuate::onEnter", 0, "u32", 4),
    Spec(0x007C3B50, "AIAttackFollowWaypointPathState::onEnter", 0, "u32", 4),
    Spec(0x007C42E0, "AIDockState::xfer", 4, "void", 10),
    Spec(0x007C43A0, "AIDockState::onEnter", 0, "u32", 10),
    Spec(0x007C4450, "AIHarvestState::xfer", 4, "void", 10),
    Spec(0x007C4510, "AIHarvestState::onEnter", 0, "u32", 10),
    Spec(0x007C4820, "AIGuardRetaliateState::xfer", 4, "void", 10),
    Spec(0x007C48C0, "AIGuardRetaliateState::onEnter", 0, "u32", 8),
    Spec(0x007C4BE0, "AIMoveToPositionAndDieState::onEnter", 0, "u32", 6),
    Spec(0x007C4D30, "AIMoveToAndEvacuateState::onEnter", 0, "u32", 12),
    Spec(0x007C4DC0, "AIFollowPathAndEvacuateState::onEnter", 0, "u32", 4),
    Spec(0x007C4DE0, "AIMoveForBoarding::onEnter", 0, "u32", 8),
    Spec(0x007C6B80, "AIStateMachineDerived6B80::createSubMachine19", 12, "ptr", 6),
    Spec(0x007C6BB0, "AIStateMachineDerived6B80::createSubMachine20", 8, "ptr", 6),
    Spec(0x007C6BE0, "AIStateMachineDerived6B80::createSubMachine21", 4, "ptr", 6),
    Spec(0x007C6C10, "AIStateMachineDerived6C10::createSubMachine19", 12, "ptr", 8),
    Spec(0x007C6C80, "AIStateMachineDerived6C10::createSubMachine20", 8, "ptr", 8),
    Spec(0x007C6E90, "AIStateMachineDerived6E90::initDefaultState", 0, "u32", 12),
    Spec(0x007C86E0, "UnitAIAttackBlockingObjectState::onExit", 4, "void", 8),
    Spec(0x007C8C90, "UnitAIEnterAndDieState::onEnter", 0, "u32", 8),
    Spec(0x007C8D40, "UnitAIFindReplacementState::onEnter", 0, "u32", 8),
    Spec(0x007C8DE0, "UnitAIFollowWaypointPathState::queryWeaponClipOffset", 4, "ptr", 8),
    Spec(0x007C9150, "UnitAIGarrisonedState::createSubMachine", 0, "ptr", 6),
    Spec(0x007C9280, "UnitAIHoldGroundState::createSubMachine", 0, "ptr", 6),
    Spec(0x007CC8A0, "AIStateMachine::createSubMachine19", 12, "ptr", 6),
    Spec(0x007CC8D0, "AIStateMachine::createSubMachine20", 8, "ptr", 6),
    Spec(0x007CC920, "AIStateMachine::updateStateMachine", 0, "u32", 14),
    Spec(0x007CCA30, "AIStateMachine::exitTemporaryState", 0, "void", 10),
    Spec(0x007CCB00, "AIStateMachine::clear", 0, "void", 10),
    Spec(0x007CCB70, "AIStateMachine::resetToDefaultState", 0, "u32", 8),
    Spec(0x007CCBC0, "AIStateMachine::setState", 4, "u32", 10),
    Spec(0x007CE280, "AIEnterState::onExit", 4, "void", 10),
    Spec(0x007CE460, "AIExitState::onExit", 4, "void", 8),
    Spec(0x007CE500, "AIExitSiegeEngineState::onExit", 4, "void", 8),
]


class NativeStubRuntime:
    def __init__(self, emu: Emulator) -> None:
        self.emu = emu
        self.base = 0
        self.event_hash = 0x811C9DC5
        self.xfer_byte_idx = 0

        # Synthetic vtable stubs
        self.vt_stubs = {
            (0x1900, 0x00): emu.bind_stub(self._vt_submach_dtor, 4),
            (0x1900, 0x14): emu.bind_stub(self._vt_submach_init_default, 0),
            (0x1900, 0x1C): emu.bind_stub(self._vt_submach_set_state, 4),
            (0x1900, 0x34): emu.bind_stub(self._vt_submach_clear, 0),
            (0x1900, 0x38): emu.bind_stub(self._vt_submach_set_goal_object, 4),
            (0x1980, 0x14): emu.bind_stub(self._vt_state_update, 0),
            (0x1980, 0x18): emu.bind_stub(self._vt_state_on_exit, 4),
            (0x1980, 0x1C): emu.bind_stub(self._vt_state_resume, 0),
            (0x1A00, 0x1C4): emu.bind_stub(self._vt_aiupdate_get_submod, 0),
            (0x1A00, 0x270): emu.bind_stub(self._vt_aiupdate_270, 0),
            (0x1A00, 0x2B0): emu.bind_stub(self._vt_aiupdate_2b0, 0),
            (0x1D00, 0x84): emu.bind_stub(self._vt_submod_check84, 0),
            (0x1DA0, 0x18): emu.bind_stub(self._vt_xfer_version, 4),
            (0x1DA0, 0x20): emu.bind_stub(self._vt_xfer_object, 4),
            (0x1DA0, 0x84): emu.bind_stub(self._vt_xfer_byte, 4),
            (0x1E40, 0x40): emu.bind_stub(self._vt_contain_40, 8),
            (0x1E40, 0x98): emu.bind_stub(self._vt_contain_98, 4),
            (0x1F00, 0x40): emu.bind_stub(self._vt_weapon_target_40, 4),
        }

        # Direct call bindings
        emu.bind_function(0x00416820, self._fn_operator_new, 0)
        emu.bind_function(0x005B9F30, self._fn_get_goal_object, 0)
        emu.bind_function(0x00683630, self._fn_get_target_id, 0)
        self.height_stub = emu.bind_stub(self._fn_query_height, 0)
        emu.write(
            0x00981070,
            b"\xE8"
            + struct.pack("<i", self.height_stub - (0x00981070 + 5))
            + b"\xD9\x00\xC2\x04\x00",
        )
        emu.bind_function(0x007C08A0, self._fn_start_move, 0)
        emu.bind_function(0x006FFB00, self._fn_check_move_out, 4)
        emu.bind_function(0x006A4F80, self._fn_set_status_helper, 8)
        emu.bind_function(0x006A4FB0, self._fn_post_status_helper, 0)
        emu.bind_function(0x00686E00, self._fn_owner_helper, 0)
        emu.bind_function(0x007526C0, self._fn_repulsor_helper, 8)
        emu.bind_function(0x007527C0, self._fn_aiupdate_mode, 4)
        emu.bind_function(0x004D9AF0, self._fn_owner_action, 4)
        emu.bind_function(0x00752720, self._fn_aiupdate_set_target_id, 4)
        self.range_stub = emu.bind_stub(self._fn_query_owner_range, 0)
        emu.write(
            0x006A6510,
            b"\xE8"
            + struct.pack("<i", self.range_stub - (0x006A6510 + 5))
            + b"\xD9\x00\xC3",
        )
        emu.bind_function(0x00798BE0, self._fn_find_target_in_range, 8)
        emu.bind_function(0x007B1E90, self._fn_ctor_0e80, 12)
        emu.bind_function(0x007B12D0, self._fn_delayed_fallback, 0)
        emu.bind_function(0x005AA240, self._fn_lock_machine, 4)
        emu.bind_function(0x007C3740, self._fn_base_move_on_enter, 0)
        emu.bind_function(0x007BD4D0, self._fn_ctor_7bd4d0, 4)
        emu.bind_function(0x00687810, self._fn_check_guard_target, 0)
        emu.bind_function(0x007BE020, self._fn_ctor_7be020, 4)
        emu.bind_function(0x005AC4F0, self._fn_set_goal_position, 8)
        emu.bind_function(0x007BDF30, self._fn_ctor_7bdf30, 4)
        emu.bind_function(0x007C2CA0, self._fn_follow_path_on_enter, 0)
        emu.bind_function(0x007BA5E0, self._fn_ctor_7ba5e0, 8)
        emu.bind_function(0x007BADC0, self._fn_ctor_7badc0, 8)
        emu.bind_function(0x007BACA0, self._fn_ctor_7baca0, 8)
        emu.bind_function(0x007BB180, self._fn_ctor_7bb180, 4)
        emu.bind_function(0x007BB070, self._fn_ctor_7bb070, 4)
        emu.bind_function(0x00A21BE0, self._fn_lock_acquire_noop, 4)
        emu.bind_function(0x00A21C90, self._fn_lock_release_noop, 4)
        emu.bind_function(0x0050B5F0, self._fn_find_object_by_id, 8)
        emu.bind_function(0x004DF7C0, self._fn_lookup_entry, 8)
        emu.bind_function(0x00A78070, self._fn_notify_default_state, 4)
        emu.bind_function(0x005B25C0, self._fn_base_init_default, 0)
        emu.bind_function(0x006C0F40, self._fn_owner_target_cleanup, 4)
        emu.bind_function(0x006A7620, self._fn_get_contain_module, 8)
        emu.bind_function(0x004DE030, self._fn_resolve_object_id, 4)
        emu.bind_function(0x0069A030, self._fn_query_aiupdate_sub, 0)
        emu.bind_function(0x007ADAB0, self._fn_recompute_sub, 0)
        emu.bind_function(0x007C9070, self._fn_ctor_7c9070, 4)
        emu.bind_function(0x007C91E0, self._fn_ctor_7c91e0, 4)
        emu.bind_function(0x007C8780, self._fn_ctor_7c8780, 8)
        emu.bind_function(0x007C8E40, self._fn_ctor_7c8e40, 4)
        emu.bind_function(0x005AC4C0, self._fn_set_goal_object, 4)
        emu.bind_function(0x005AC420, self._fn_base_update_machine, 0)
        emu.bind_function(0x006838B0, self._fn_notify_temp_exit, 0)
        emu.bind_function(0x005C0840, self._fn_base_clear, 0)
        emu.bind_function(0x005C08C0, self._fn_base_reset_default, 0)
        emu.bind_function(0x005B6230, self._fn_base_set_state, 4)
        emu.bind_function(0x007B1190, self._fn_base_state_on_exit, 4)
        emu.bind_function(0x006E6360, self._fn_aiupdate_helper, 4)
        emu.bind_function(0x006A7600, self._fn_get_open_contain_module, 0)

    def reset(self, base: int) -> None:
        self.base = base
        self.event_hash = 0x811C9DC5
        self.xfer_byte_idx = 0
        for (vt_off, slot_off), stub_addr in self.vt_stubs.items():
            self.emu.write(base + vt_off + slot_off, p32(stub_addr))

    def rel(self, addr: int) -> int:
        addr = u32(addr)
        if self.base <= addr < self.base + ARENA_SIZE:
            return addr - self.base
        return addr

    def mix(self, tag: int, a: int = 0, b: int = 0, c: int = 0) -> None:
        for v in (tag, a, b, c):
            self.event_hash = u32((self.event_hash ^ u32(v)) * 16777619)

    def load_u32(self, off: int) -> int:
        return struct.unpack("<I", self.emu.read(self.base + off, 4))[0]

    def load_f32(self, off: int) -> float:
        return struct.unpack("<f", self.emu.read(self.base + off, 4))[0]

    def ptr_at(self, off: int) -> int:
        v = self.load_u32(off)
        return 0 if v == 0 else self.base + v

    # Synthetic vtable handlers
    def _vt_submach_dtor(self, emu: Emulator) -> int:
        self.mix(0x90000000, self.rel(emu.register(x86.UC_X86_REG_ECX)), emu.stack_arg(0))
        return 0

    def _vt_submach_init_default(self, emu: Emulator) -> int:
        self.mix(0x90000014, self.rel(emu.register(x86.UC_X86_REG_ECX)))
        return self.load_u32(0x0038)

    def _vt_submach_set_state(self, emu: Emulator) -> int:
        self.mix(0x9000001C, self.rel(emu.register(x86.UC_X86_REG_ECX)), emu.stack_arg(0))
        return self.load_u32(0x0038)

    def _vt_submach_clear(self, emu: Emulator) -> int:
        self.mix(0x90000034, self.rel(emu.register(x86.UC_X86_REG_ECX)))
        return 0

    def _vt_submach_set_goal_object(self, emu: Emulator) -> int:
        self.mix(
            0x90000038,
            self.rel(emu.register(x86.UC_X86_REG_ECX)),
            self.rel(emu.stack_arg(0)),
        )
        return 0

    def _vt_state_update(self, emu: Emulator) -> int:
        self.mix(0x91000014, self.rel(emu.register(x86.UC_X86_REG_ECX)))
        return self.load_u32(0x0038)

    def _vt_state_on_exit(self, emu: Emulator) -> int:
        self.mix(0x91000018, self.rel(emu.register(x86.UC_X86_REG_ECX)), emu.stack_arg(0))
        return 0

    def _vt_state_resume(self, emu: Emulator) -> int:
        self.mix(0x9100001C, self.rel(emu.register(x86.UC_X86_REG_ECX)))
        return 0

    def _vt_aiupdate_get_submod(self, emu: Emulator) -> int:
        self.mix(0x920001C4, self.rel(emu.register(x86.UC_X86_REG_ECX)))
        return self.ptr_at(0x003C)

    def _vt_aiupdate_270(self, emu: Emulator) -> int:
        self.mix(0x92000270, self.rel(emu.register(x86.UC_X86_REG_ECX)))
        return 0

    def _vt_aiupdate_2b0(self, emu: Emulator) -> int:
        self.mix(0x920002B0, self.rel(emu.register(x86.UC_X86_REG_ECX)))
        return 0

    def _vt_submod_check84(self, emu: Emulator) -> int:
        self.mix(0x93000084, self.rel(emu.register(x86.UC_X86_REG_ECX)))
        return self.load_u32(0x0040)

    def _vt_xfer_version(self, emu: Emulator) -> int:
        ver_ptr = emu.stack_arg(0)
        b0, b1 = self.emu.read(ver_ptr, 2)
        packed = b0 | (b1 << 8)
        self.mix(0x94000018, self.rel(emu.register(x86.UC_X86_REG_ECX)), packed)
        return 0

    def _vt_xfer_object(self, emu: Emulator) -> int:
        self.mix(
            0x94000020,
            self.rel(emu.register(x86.UC_X86_REG_ECX)),
            self.rel(emu.stack_arg(0)),
        )
        return 0

    def _vt_xfer_byte(self, emu: Emulator) -> int:
        byte_ptr = emu.stack_arg(0)
        idx = self.xfer_byte_idx
        self.xfer_byte_idx += 1
        if idx == 0:
            override = self.load_u32(0x0044)
            if override != 0xFFFFFFFF:
                self.emu.write(byte_ptr, bytes([override & 0xFF]))
        val = self.emu.read(byte_ptr, 1)[0]
        self.mix(0x94000084, self.rel(emu.register(x86.UC_X86_REG_ECX)), val, idx)
        return 0

    def _vt_contain_40(self, emu: Emulator) -> int:
        self.mix(
            0x95000040,
            self.rel(emu.register(x86.UC_X86_REG_ECX)),
            self.rel(emu.stack_arg(0)),
            emu.stack_arg(1),
        )
        return 0

    def _vt_contain_98(self, emu: Emulator) -> int:
        self.mix(
            0x95000098,
            self.rel(emu.register(x86.UC_X86_REG_ECX)),
            self.rel(emu.stack_arg(0)),
        )
        return 0

    def _vt_weapon_target_40(self, emu: Emulator) -> int:
        self.mix(
            0x96000040,
            self.rel(emu.register(x86.UC_X86_REG_ECX)),
            emu.stack_arg(0),
        )
        return 0

    # Direct function handlers
    def _fn_operator_new(self, emu: Emulator) -> int:
        self.mix(0x00416820, emu.stack_arg(0))
        return self.ptr_at(0x0000)

    def _fn_get_goal_object(self, emu: Emulator) -> int:
        self.mix(0x005B9F30, self.rel(emu.register(x86.UC_X86_REG_ECX)))
        return self.ptr_at(0x000C)

    def _fn_get_target_id(self, emu: Emulator) -> int:
        self.mix(0x00683630, self.rel(emu.register(x86.UC_X86_REG_ECX)))
        return self.load_u32(0x0010)

    def _fn_query_height(self, emu: Emulator) -> int:
        # ESP+0 is return address 0x00981076; ESP+4 is caller's return address; ESP+8 is caller's arg0
        arg0 = emu.stack_arg(1)
        self.mix(0x00981070, self.rel(emu.register(x86.UC_X86_REG_ECX)), arg0)
        return self.base + 0x0014

    def _fn_start_move(self, emu: Emulator) -> int:
        self.mix(0x007C08A0, self.rel(emu.register(x86.UC_X86_REG_ECX)))
        return self.load_u32(0x0004)

    def _fn_check_move_out(self, emu: Emulator) -> int:
        self.mix(0x006FFB00, self.rel(emu.register(x86.UC_X86_REG_ECX)), emu.stack_arg(0))
        return 1 if self.load_u32(0x0008) != 0 else 0

    def _fn_set_status_helper(self, emu: Emulator) -> int:
        self.mix(
            0x006A4F80,
            self.rel(emu.register(x86.UC_X86_REG_ECX)),
            emu.stack_arg(0),
            emu.stack_arg(1),
        )
        return 0

    def _fn_post_status_helper(self, emu: Emulator) -> int:
        self.mix(0x006A4FB0, self.rel(emu.register(x86.UC_X86_REG_ECX)))
        return 0

    def _fn_owner_helper(self, emu: Emulator) -> int:
        self.mix(0x00686E00, self.rel(emu.register(x86.UC_X86_REG_ECX)))
        return 0

    def _fn_repulsor_helper(self, emu: Emulator) -> int:
        self.mix(
            0x007526C0,
            self.rel(emu.register(x86.UC_X86_REG_ECX)),
            self.rel(emu.stack_arg(0)),
            self.rel(emu.stack_arg(1)),
        )
        return 0

    def _fn_aiupdate_mode(self, emu: Emulator) -> int:
        self.mix(0x007527C0, self.rel(emu.register(x86.UC_X86_REG_ECX)), emu.stack_arg(0))
        return 1

    def _fn_owner_action(self, emu: Emulator) -> int:
        self.mix(0x004D9AF0, self.rel(emu.register(x86.UC_X86_REG_ECX)), emu.stack_arg(0))
        return 0

    def _fn_aiupdate_set_target_id(self, emu: Emulator) -> int:
        self.mix(0x00752720, self.rel(emu.register(x86.UC_X86_REG_ECX)), emu.stack_arg(0))
        return 0

    def _fn_query_owner_range(self, emu: Emulator) -> int:
        self.mix(0x006A6510, self.rel(emu.register(x86.UC_X86_REG_ECX)))
        return self.base + 0x0018

    def _fn_find_target_in_range(self, emu: Emulator) -> int:
        self.mix(
            0x00798BE0,
            self.rel(emu.register(x86.UC_X86_REG_ECX)),
            self.rel(emu.stack_arg(0)),
            emu.stack_arg(1),
        )
        return self.ptr_at(0x000C)

    def _fn_ctor_0e80(self, emu: Emulator) -> int:
        mem_ptr = emu.register(x86.UC_X86_REG_ECX)
        self.mix(
            0x007B1E90,
            self.rel(mem_ptr),
            self.rel(emu.stack_arg(0)),
            self.rel(emu.stack_arg(1)),
        )
        self.mix(emu.stack_arg(2))
        return mem_ptr

    def _fn_delayed_fallback(self, emu: Emulator) -> int:
        self.mix(0x007B12D0, self.rel(emu.register(x86.UC_X86_REG_ECX)))
        return self.load_u32(0x0048)

    def _fn_lock_machine(self, emu: Emulator) -> int:
        mach = emu.register(x86.UC_X86_REG_ECX)
        self.mix(0x005AA240, self.rel(mach), emu.stack_arg(0))
        if mach != 0:
            self.emu.write(mach + 0x78, b"\x01")
        return 0

    def _fn_base_move_on_enter(self, emu: Emulator) -> int:
        self.mix(0x007C3740, self.rel(emu.register(x86.UC_X86_REG_ECX)))
        return self.load_u32(0x0048)

    def _fn_ctor_7bd4d0(self, emu: Emulator) -> int:
        mem_ptr = emu.register(x86.UC_X86_REG_ECX)
        self.mix(0x007BD4D0, self.rel(mem_ptr), self.rel(emu.stack_arg(0)))
        return mem_ptr

    def _fn_check_guard_target(self, emu: Emulator) -> int:
        self.mix(0x00687810, self.rel(emu.register(x86.UC_X86_REG_ECX)))
        return self.ptr_at(0x001C)

    def _fn_ctor_7be020(self, emu: Emulator) -> int:
        mem_ptr = emu.register(x86.UC_X86_REG_ECX)
        self.mix(0x007BE020, self.rel(mem_ptr), self.rel(emu.stack_arg(0)))
        return mem_ptr

    def _fn_set_goal_position(self, emu: Emulator) -> int:
        mach = emu.register(x86.UC_X86_REG_ECX)
        pos_ptr = emu.stack_arg(0)
        rad_bits = emu.stack_arg(1)
        self.mix(0x005AC4F0, self.rel(mach), self.rel(pos_ptr), rad_bits)
        if mach != 0 and self.emu.read(mach + 0x78, 1)[0] == 0:
            if pos_ptr != 0:
                coord = self.emu.read(pos_ptr, 12)
                self.emu.write(mach + 0x2C, coord)
                self.emu.write(mach + 0x48, coord)
                self.emu.write(mach + 0x44, b"\x00")
            self.emu.write(mach + 0x54, p32(rad_bits))
        return 0

    def _fn_ctor_7bdf30(self, emu: Emulator) -> int:
        mem_ptr = emu.register(x86.UC_X86_REG_ECX)
        self.mix(0x007BDF30, self.rel(mem_ptr), self.rel(emu.stack_arg(0)))
        return mem_ptr

    def _fn_follow_path_on_enter(self, emu: Emulator) -> int:
        self.mix(0x007C2CA0, self.rel(emu.register(x86.UC_X86_REG_ECX)))
        return self.load_u32(0x0048)

    def _fn_ctor_7ba5e0(self, emu: Emulator) -> int:
        mem_ptr = emu.register(x86.UC_X86_REG_ECX)
        self.mix(
            0x007BA5E0,
            self.rel(mem_ptr),
            self.rel(emu.stack_arg(0)),
            self.rel(emu.stack_arg(1)),
        )
        return mem_ptr

    def _fn_ctor_7badc0(self, emu: Emulator) -> int:
        mem_ptr = emu.register(x86.UC_X86_REG_ECX)
        self.mix(
            0x007BADC0,
            self.rel(mem_ptr),
            self.rel(emu.stack_arg(0)),
            self.rel(emu.stack_arg(1)),
        )
        return mem_ptr

    def _fn_ctor_7baca0(self, emu: Emulator) -> int:
        mem_ptr = emu.register(x86.UC_X86_REG_ECX)
        self.mix(
            0x007BACA0,
            self.rel(mem_ptr),
            self.rel(emu.stack_arg(0)),
            self.rel(emu.stack_arg(1)),
        )
        return mem_ptr

    def _fn_ctor_7bb180(self, emu: Emulator) -> int:
        mem_ptr = emu.register(x86.UC_X86_REG_ECX)
        self.mix(0x007BB180, self.rel(mem_ptr), self.rel(emu.stack_arg(0)))
        return mem_ptr

    def _fn_ctor_7bb070(self, emu: Emulator) -> int:
        mem_ptr = emu.register(x86.UC_X86_REG_ECX)
        self.mix(0x007BB070, self.rel(mem_ptr), self.rel(emu.stack_arg(0)))
        return mem_ptr

    def _fn_lock_acquire_noop(self, emu: Emulator) -> int:
        return 0

    def _fn_lock_release_noop(self, emu: Emulator) -> int:
        return 0

    def _fn_find_object_by_id(self, emu: Emulator) -> int:
        # Native passes ecx = theGameLogic + 0xC4, arg0 = outIterPtr, arg1 = &objId
        ecx = emu.register(x86.UC_X86_REG_ECX)
        out_iter_ptr = emu.stack_arg(0)
        key_ptr = emu.stack_arg(1)
        obj_id = struct.unpack("<I", self.emu.read(key_ptr, 4))[0]
        game_logic_ptr = ecx - 0xC4
        self.mix(0x0050B5F0, self.rel(game_logic_ptr), obj_id)

        found_obj = self.ptr_at(0x0020)
        # Set up hash table sentinel at [ecx + 4] and [ecx + 8] and iterator at [out_iter_ptr]
        # Native checks: edx=[ecx+4], k=[ecx+8], eax=[out_iter_ptr]; if eax != [edx + k*4], obj = [eax + 4]
        sentinel_node = self.base + 0x18D0
        found_node = self.base + 0x18E0
        buckets_arr = self.base + 0x18C0
        self.emu.write(ecx + 4, p32(buckets_arr))
        self.emu.write(ecx + 8, p32(0))
        self.emu.write(buckets_arr, p32(sentinel_node))
        if found_obj != 0:
            self.emu.write(found_node + 4, p32(found_obj))
            self.emu.write(out_iter_ptr, p32(found_node))
        else:
            self.emu.write(out_iter_ptr, p32(sentinel_node))
        return out_iter_ptr

    def _fn_lookup_entry(self, emu: Emulator) -> int:
        table_ptr = emu.register(x86.UC_X86_REG_ECX)
        entry_id = emu.stack_arg(0)
        kind = emu.stack_arg(1)
        self.mix(0x004DF7C0, self.rel(table_ptr), entry_id, kind)
        slot = 0x0024 if kind == 0x14 else 0x0028
        return self.ptr_at(slot)

    def _fn_notify_default_state(self, emu: Emulator) -> int:
        self.mix(0x00A78070, self.rel(emu.register(x86.UC_X86_REG_ECX)), emu.stack_arg(0))
        return 0

    def _fn_base_init_default(self, emu: Emulator) -> int:
        self.mix(0x005B25C0, self.rel(emu.register(x86.UC_X86_REG_ECX)))
        return self.load_u32(0x0048)

    def _fn_owner_target_cleanup(self, emu: Emulator) -> int:
        self.mix(
            0x006C0F40,
            self.rel(emu.register(x86.UC_X86_REG_ECX)),
            self.rel(emu.stack_arg(0)),
        )
        return 0

    def _fn_get_contain_module(self, emu: Emulator) -> int:
        self.mix(
            0x006A7620,
            self.rel(emu.register(x86.UC_X86_REG_ECX)),
            self.rel(emu.stack_arg(0)),
            emu.stack_arg(1),
        )
        return self.ptr_at(0x002C)

    def _fn_resolve_object_id(self, emu: Emulator) -> int:
        self.mix(0x004DE030, self.rel(emu.register(x86.UC_X86_REG_ECX)), emu.stack_arg(0))
        return self.load_u32(0x0030)

    def _fn_query_aiupdate_sub(self, emu: Emulator) -> int:
        self.mix(0x0069A030, self.rel(emu.register(x86.UC_X86_REG_ECX)))
        return self.ptr_at(0x0034)

    def _fn_recompute_sub(self, emu: Emulator) -> int:
        self.mix(0x007ADAB0, self.rel(emu.register(x86.UC_X86_REG_ECX)))
        return 0

    def _fn_ctor_7c9070(self, emu: Emulator) -> int:
        mem_ptr = emu.register(x86.UC_X86_REG_ECX)
        self.mix(0x007C9070, self.rel(mem_ptr), self.rel(emu.stack_arg(0)))
        return mem_ptr

    def _fn_ctor_7c91e0(self, emu: Emulator) -> int:
        mem_ptr = emu.register(x86.UC_X86_REG_ECX)
        self.mix(0x007C91E0, self.rel(mem_ptr), self.rel(emu.stack_arg(0)))
        return mem_ptr

    def _fn_ctor_7c8780(self, emu: Emulator) -> int:
        mem_ptr = emu.register(x86.UC_X86_REG_ECX)
        self.mix(
            0x007C8780,
            self.rel(mem_ptr),
            self.rel(emu.stack_arg(0)),
            self.rel(emu.stack_arg(1)),
        )
        return mem_ptr

    def _fn_ctor_7c8e40(self, emu: Emulator) -> int:
        mem_ptr = emu.register(x86.UC_X86_REG_ECX)
        self.mix(0x007C8E40, self.rel(mem_ptr), self.rel(emu.stack_arg(0)))
        return mem_ptr

    def _fn_set_goal_object(self, emu: Emulator) -> int:
        mach = emu.register(x86.UC_X86_REG_ECX)
        goal_obj = emu.stack_arg(0)
        self.mix(0x005AC4C0, self.rel(mach), self.rel(goal_obj))
        if mach != 0 and self.emu.read(mach + 0x78, 1)[0] == 0:
            obj_id = 0
            if goal_obj != 0:
                obj_id = struct.unpack("<I", self.emu.read(goal_obj + 0xDC, 4))[0]
            self.emu.write(mach + 0x28, p32(obj_id))
        return 0

    def _fn_base_update_machine(self, emu: Emulator) -> int:
        self.mix(0x005AC420, self.rel(emu.register(x86.UC_X86_REG_ECX)))
        return self.load_u32(0x0048)

    def _fn_notify_temp_exit(self, emu: Emulator) -> int:
        self.mix(0x006838B0, self.rel(emu.register(x86.UC_X86_REG_ECX)))
        return 0

    def _fn_base_clear(self, emu: Emulator) -> int:
        self.mix(0x005C0840, self.rel(emu.register(x86.UC_X86_REG_ECX)))
        return 0

    def _fn_base_reset_default(self, emu: Emulator) -> int:
        self.mix(0x005C08C0, self.rel(emu.register(x86.UC_X86_REG_ECX)))
        return self.load_u32(0x0048)

    def _fn_base_set_state(self, emu: Emulator) -> int:
        self.mix(0x005B6230, self.rel(emu.register(x86.UC_X86_REG_ECX)), emu.stack_arg(0))
        return self.load_u32(0x0048)

    def _fn_base_state_on_exit(self, emu: Emulator) -> int:
        self.mix(0x007B1190, self.rel(emu.register(x86.UC_X86_REG_ECX)), emu.stack_arg(0))
        return 0

    def _fn_aiupdate_helper(self, emu: Emulator) -> int:
        self.mix(0x006E6360, self.rel(emu.register(x86.UC_X86_REG_ECX)), emu.stack_arg(0))
        return 0

    def _fn_get_open_contain_module(self, emu: Emulator) -> int:
        self.mix(0x006A7600, self.rel(emu.register(x86.UC_X86_REG_ECX)))
        return self.ptr_at(0x002C)


def build_scenario(spec: Spec, idx: int, rng: random.Random) -> tuple[bytearray, list[int], list[int]]:
    arena = bytearray(ARENA_SIZE)
    relocs: list[int] = []

    def set_ptr(off: int, target_off: int) -> None:
        put32(arena, off, target_off)
        if off not in relocs:
            relocs.append(off)

    # Arena layout:
    # 0x0000..0x0064: control block
    # 0x0100: self (State or AIStateMachine)
    # 0x0200: machine (when self is State) or tempState (when self is AIStateMachine)
    # 0x0300: owner Object (size ~0x480)
    # 0x0800: AIUpdateInterface (size ~0x460)
    # 0x0D00: goalObject / targetObject / containerObject / producerObject
    # 0x0E80: ThingTemplate for owner
    # 0x0F80: ThingTemplate for producer
    # 0x1080: allocated subMachine / existing subMachine (vtable 0x1900)
    # 0x1180: currentState (vtable 0x1980)
    # 0x1200: Xfer object (vtable 0x1DA0)
    # 0x1280: SubModule from AIUpdate+0x1C4 (vtable 0x1D00)
    # 0x1300: ContainModule (vtable 0x1E40)
    # 0x1380: WeaponSubObject / AIUpdate sub +0x150 / +0x214 / +0x418
    # 0x1400: inner object / lookup entry 1
    # 0x1480: lookup entry 2 / WeaponTarget (vtable 0x1F00)
    # 0x1500: TheGameLogic
    # 0x1680: TheWeaponStore
    # 0x1700: ThePartitionMgr
    # 0x1780: OwnerComponent324 / outPair buffer

    self_off = 0x0100
    mach_off = 0x0200
    owner_off = 0x0300
    ai_off = 0x0800
    goal_off = 0x0D00
    tmpl_off = 0x0E80
    prod_tmpl_off = 0x0F80
    submach_off = 0x1080
    curstate_off = 0x1180
    xfer_off = 0x1200
    submod_off = 0x1280
    contain_off = 0x1300
    wsub_off = 0x1380
    inner_off = 0x1400
    entry2_off = 0x1480
    gl_off = 0x1500
    ws_off = 0x1680
    pm_off = 0x1700
    comp_off = 0x1780

    # Global singletons in control block
    put32(arena, 0x0058, gl_off)
    put32(arena, 0x005C, ws_off)
    put32(arena, 0x0060, pm_off)

    # Default control block returns
    put32(arena, 0x0000, submach_off if idx % 5 != 4 else 0)  # operatorNew
    put32(arena, 0x0004, rng.choice([0, 1, 0xFFFFFFFF, 0xFFFFFFFE]))  # startMove return
    put32(arena, 0x0008, idx & 1)  # checkMoveOut
    put32(arena, 0x000C, goal_off if idx % 3 != 0 else 0)  # getGoalObject
    put32(arena, 0x0010, 0x11223344 if idx % 2 == 0 else 0x55667788)  # AIUpdate targetId
    put32(arena, 0x0014, fbits(rng.uniform(2.0, 20.0)))  # queryHeight
    put32(arena, 0x0018, fbits(rng.uniform(50.0, 300.0)))  # queryOwnerRange
    put32(arena, 0x001C, 0x1280 if idx % 4 != 1 else 0)  # checkGuardTarget
    put32(arena, 0x0020, goal_off if idx % 3 != 2 else 0)  # findObjectById
    put32(arena, 0x0024, inner_off)  # lookupEntry(0x14)
    put32(arena, 0x0028, entry2_off)  # lookupEntry(0x27)
    put32(arena, 0x002C, contain_off if idx % 4 != 2 else 0)  # getContainModule
    put32(arena, 0x0030, 0x99887766 if idx % 3 != 0 else 0)  # resolveObjectId
    put32(arena, 0x0034, wsub_off if idx % 3 != 0 else 0)  # queryAIUpdateSub
    put32(arena, 0x0038, rng.choice([0, 1, 0xFFFFFFFF, 0xFFFFFFFE]))  # vtable status return
    put32(arena, 0x003C, submod_off if idx % 3 != 1 else 0)  # AIUpdate getSubMod
    put32(arena, 0x0040, idx & 1)  # subModCheck84
    put32(arena, 0x0044, 0xFFFFFFFF)  # xfer byte override
    put32(arena, 0x0048, rng.choice([0, 1, 2, 0xFFFFFFFF]))  # base fallback return

    # Vtable pointers for synthetic objects
    set_ptr(self_off + 0x00, 0x1900)
    set_ptr(mach_off + 0x00, 0x1900)
    set_ptr(submach_off + 0x00, 0x1900)
    set_ptr(curstate_off + 0x00, 0x1980)
    set_ptr(ai_off + 0x00, 0x1A00)
    set_ptr(submod_off + 0x00, 0x1D00)
    set_ptr(xfer_off + 0x00, 0x1DA0)
    set_ptr(contain_off + 0x00, 0x1E40)
    set_ptr(entry2_off + 0x00, 0x1F00)

    # Common links for State -> Machine -> Owner -> AIUpdate
    set_ptr(self_off + 0x18, mach_off)
    set_ptr(mach_off + 0x24, owner_off)
    set_ptr(owner_off + 0x04, tmpl_off)
    set_ptr(owner_off + 0x374, ai_off if idx % 6 != 5 else 0)
    set_ptr(owner_off + 0x324, comp_off if idx % 3 != 0 else 0)
    set_ptr(owner_off + 0x418, wsub_off)
    set_ptr(ai_off + 0x3C, mach_off)

    # Common coordinates & IDs
    for c_off in (self_off + 0x20, mach_off + 0x2C, mach_off + 0x48, owner_off + 0x38, goal_off + 0x38, inner_off + 0x0C):
        put32(arena, c_off + 0x00, fbits(rng.uniform(-200.0, 200.0)))
        put32(arena, c_off + 0x04, fbits(rng.uniform(-200.0, 200.0)))
        put32(arena, c_off + 0x08, fbits(rng.uniform(-50.0, 100.0)))

    put32(arena, goal_off + 0xDC, 0x11223344)
    set_ptr(goal_off + 0x04, prod_tmpl_off)
    put32(arena, owner_off + 0xB0, rng.choice([0, 0x00400000, 0x00008000, 0x00800000, 0xFFFFFFFF]))
    put32(arena, owner_off + 0xE4, 0xABCDEF01 if idx % 4 != 0 else 0)
    put32(arena, owner_off + 0x45C, rng.getrandbits(32))
    put32(arena, owner_off + 0x460, rng.getrandbits(32))
    put8(arena, ai_off + 0x434, idx & 1)
    put8(arena, mach_off + 0x44, idx & 1)
    put8(arena, mach_off + 0x78, 0)
    put32(arena, mach_off + 0x28, rng.choice([0, 0x12345678]))
    put8(arena, self_off + 0x54, idx & 1)
    put32(arena, self_off + 0x50, rng.choice([0, 1, 2, 5]))
    put32(arena, tmpl_off + 0xC0, (1 << 26) if (idx & 1) else 0)
    put32(arena, tmpl_off + 0xDC, (1 << 20) if (idx % 3 != 0) else 0)
    put32(arena, prod_tmpl_off + 0xD0, (1 << 3) if (idx & 1) else 0)
    put32(arena, wsub_off + 0x20, 0x42)
    put32(arena, inner_off + 0x14, rng.choice([10, 20, 30]))
    put32(arena, entry2_off + 0x14, rng.choice([10, 20, 30]))

    stack_args: list[int] = []
    va = spec.va

    if va == 0x007C0AD0 or va == 0x007C4DE0:
        # Ensure owner->aiUpdate can be null or non-null, and goalZ comparison both ways
        if idx % 2 == 0:
            put32(arena, goal_off + 0x40, fbits(-100.0))
        else:
            put32(arena, goal_off + 0x40, fbits(500.0))
        if va == 0x007C4DE0:
            set_ptr(owner_off + 0x374, ai_off)

    elif va == 0x007C0C80:
        set_ptr(owner_off + 0x374, ai_off)
        if idx % 3 != 0:
            set_ptr(ai_off + 0x150, wsub_off)
            set_ptr(wsub_off + 0x14, inner_off)
        else:
            put32(arena, ai_off + 0x150, 0)

    elif va == 0x007C0D00:
        set_ptr(owner_off + 0x374, ai_off)

    elif va in (0x007C0D70, 0x007C0DF0, 0x007C0EE0):
        # Owner is always non-null on the parent state machine; vary aiUpdate and goalObject
        set_ptr(mach_off + 0x24, owner_off)

    elif va == 0x007C0E80:
        if idx % 3 == 0:
            set_ptr(self_off + 0x20, submach_off)
        else:
            put32(arena, self_off + 0x20, 0)

    elif va == 0x007C2C80:
        put32(arena, self_off + 0x50, idx % 3)

    elif va == 0x007C3680:
        if idx % 4 == 3:
            put32(arena, mach_off + 0x24, 0)

    elif va in (0x007C39C0, 0x007C4DC0):
        set_ptr(owner_off + 0x374, ai_off)

    elif va == 0x007C3B50:
        set_ptr(self_off + 0x6C, submach_off)

    elif va in (0x007C42E0, 0x007C4450, 0x007C4820):
        put32(arena, 0x0000, submach_off)
        put32(arena, xfer_off + 0x04, (1 << 3) if idx == 0 else 0)
        if idx % 3 == 1:
            set_ptr(self_off + 0x20, submach_off)
        else:
            put32(arena, self_off + 0x20, 0)
        if idx % 2 == 0:
            put32(arena, 0x0044, 1)  # force hasSub = 1 during load
        elif idx % 4 == 3:
            put32(arena, 0x0044, 0)
        put8(arena, self_off + 0x24, rng.randrange(256))
        put8(arena, self_off + 0x25, rng.randrange(256))
        put32(arena, 0x004C, xfer_off)
        stack_args = [xfer_off]

    elif va in (0x007C43A0, 0x007C4510, 0x007C48C0):
        put32(arena, 0x0000, submach_off)
        if va == 0x007C48C0:
            set_ptr(owner_off + 0x374, ai_off)

    elif va == 0x007C4D30:
        set_ptr(owner_off + 0x374, ai_off)
        if idx % 2 == 0:
            # Close distance <= 0.1f
            put32(arena, self_off + 0x20, fbits(10.0))
            put32(arena, self_off + 0x24, fbits(10.0))
            put32(arena, self_off + 0x28, fbits(10.0))
            put32(arena, owner_off + 0x38, fbits(10.02))
            put32(arena, owner_off + 0x3C, fbits(10.02))
            put32(arena, owner_off + 0x40, fbits(10.02))
        else:
            # Far distance > 0.1f
            put32(arena, self_off + 0x20, fbits(10.0))
            put32(arena, self_off + 0x24, fbits(10.0))
            put32(arena, self_off + 0x28, fbits(10.0))
            put32(arena, owner_off + 0x38, fbits(50.0))
            put32(arena, owner_off + 0x3C, fbits(50.0))
            put32(arena, owner_off + 0x40, fbits(10.0))

    elif va in (0x007C6B80, 0x007CC8A0):
        a0, a1, a2 = rng.getrandbits(32), rng.getrandbits(32), rng.getrandbits(32)
        # Ensure args don't accidentally fall into [0, ARENA_SIZE)
        a0 |= 0x10000
        a1 |= 0x10000
        a2 |= 0x10000
        put32(arena, 0x004C, a0)
        put32(arena, 0x0050, a1)
        put32(arena, 0x0054, a2)
        stack_args = [a0, a1, a2]

    elif va in (0x007C6BB0, 0x007CC8D0):
        a0, a1 = rng.getrandbits(32) | 0x10000, rng.getrandbits(32) | 0x10000
        put32(arena, 0x004C, a0)
        put32(arena, 0x0050, a1)
        stack_args = [a0, a1]

    elif va == 0x007C6BE0:
        a0 = rng.getrandbits(32) | 0x10000
        put32(arena, 0x004C, a0)
        stack_args = [a0]

    elif va == 0x007C6C10:
        a0, a1 = rng.getrandbits(32) | 0x10000, rng.getrandbits(32) | 0x10000
        a2_off = goal_off if idx % 3 != 0 else 0
        put32(arena, goal_off + 0x80, (1 << 7) if (idx & 1) else 0)
        put32(arena, 0x004C, a0)
        put32(arena, 0x0050, a1)
        put32(arena, 0x0054, a2_off)
        stack_args = [a0, a1, a2_off]

    elif va == 0x007C6C80:
        a0 = rng.getrandbits(32) | 0x10000
        a1_off = goal_off if idx % 3 != 0 else 0
        put32(arena, goal_off + 0x80, (1 << 7) if (idx & 1) else 0)
        put32(arena, 0x004C, a0)
        put32(arena, 0x0050, a1_off)
        stack_args = [a0, a1_off]

    elif va == 0x007C6E90:
        # self_off is AIStateMachineDerived6E90, so self+0x24 is owner Object
        set_ptr(self_off + 0x24, owner_off)

    elif va == 0x007C86E0:
        put32(arena, self_off + 0x2C, 0x22334455 if idx % 2 == 0 else 0)
        if idx % 3 != 0:
            set_ptr(self_off + 0x20, submach_off)
        else:
            put32(arena, self_off + 0x20, 0)
        a0 = idx & 1
        put32(arena, 0x004C, a0)
        stack_args = [a0]

    elif va == 0x007C8DE0:
        set_ptr(owner_off + 0x374, ai_off)
        set_ptr(self_off + 0x20, entry2_off)
        put8(arena, wsub_off + 0x14, idx & 1)
        put32(arena, wsub_off + 0x10, fbits(rng.uniform(1.0, 50.0)))
        put32(arena, 0x004C, comp_off)
        stack_args = [comp_off]

    elif va in (0x007CC920, 0x007CCA30, 0x007CCB00, 0x007CCB70, 0x007CCBC0):
        # self_off is AIStateMachine (must precede self_off + 0x20 float writes)
        set_ptr(self_off + 0x24, owner_off if idx % 7 != 6 else 0)
        if idx % 4 != 0:
            set_ptr(self_off + 0x88, curstate_off)
        else:
            put32(arena, self_off + 0x88, 0)
        if idx % 3 == 0:
            set_ptr(self_off + 0x04, mach_off)
            set_ptr(mach_off + 0x00, 0x1980)
            put32(arena, mach_off + 0x04, rng.choice([0, 2, 5]))
        else:
            put32(arena, self_off + 0x04, 0)
        if idx % 3 != 1:
            set_ptr(self_off + 0x84, submach_off)
        else:
            put32(arena, self_off + 0x84, 0)

        frame_end = rng.choice([10, 50, 100, 0xFFFFFFFF])
        cur_frame = rng.choice([20, 50, 80])
        put32(arena, self_off + 0x8C, frame_end)
        put32(arena, gl_off + 0x58, cur_frame)
        put32(arena, self_off + 0x90, 0x44556677 if idx % 2 == 0 else 0)
        put32(arena, self_off + 0x94, fbits(12.5))
        put32(arena, self_off + 0x98, fbits(-34.0))
        put32(arena, self_off + 0x9C, fbits(5.0))
        put8(arena, self_off + 0x78, 0)

        if va in (0x007CCB00, 0x007CCB70, 0x007CCBC0) and frame_end != 0xFFFFFFFF:
            set_ptr(self_off + 0x24, owner_off)

        if va == 0x007CCBC0:
            new_state = rng.choice([0, 2, 5, 9])
            put32(arena, 0x004C, new_state)
            stack_args = [new_state]

    elif va == 0x007CE280:
        if idx % 2 == 0:
            set_ptr(ai_off + 0x214, wsub_off)
            put32(arena, wsub_off + 0x40, 0xFFFFFFFF)
        else:
            put32(arena, ai_off + 0x214, 0)
        put32(arena, self_off + 0x50, 0x778899AA if idx % 3 != 0 else 0)
        a0 = idx & 1
        put32(arena, 0x004C, a0)
        stack_args = [a0]

    elif va in (0x007CE460, 0x007CE500):
        put32(arena, self_off + 0x20, 0x778899AA if idx % 2 == 0 else 0)
        a0 = idx & 1
        put32(arena, 0x004C, a0)
        stack_args = [a0]

    return arena, relocs, stack_args


def run_native(
    emu: Emulator,
    rt: NativeStubRuntime,
    spec: Spec,
    arena: bytearray,
    relocs: list[int],
    stack_args: list[int],
) -> tuple[int, int, bytes, int]:
    emu.reset_data()
    base = emu.alloc(bytes(arena), alignment=0x1000)

    for off in relocs:
        val = load32(arena, off)
        if 0 < val < ARENA_SIZE:
            emu.write(base + off, p32(base + val))

    rt.reset(base)

    gl_off = load32(arena, 0x0058)
    ws_off = load32(arena, 0x005C)
    pm_off = load32(arena, 0x0060)
    emu.write(0x00DEBA64, p32(base + gl_off if gl_off else 0))
    emu.write(0x00DEBA88, p32(base + ws_off if ws_off else 0))
    emu.write(0x00DFB9D8, p32(base + pm_off if pm_off else 0))

    native_args: list[int] = []
    for i, a in enumerate(stack_args):
        if spec.va in (0x007C42E0, 0x007C4450, 0x007C4820, 0x007C8DE0) and i == 0:
            native_args.append(base + a)
        elif spec.va == 0x007C6C10 and i == 2 and a != 0:
            native_args.append(base + a)
        elif spec.va == 0x007C6C80 and i == 1 and a != 0:
            native_args.append(base + a)
        else:
            native_args.append(a)

    res = emu.call(spec.va, native_args, ecx=base + 0x0100)
    out_arena = bytearray(emu.read(base, ARENA_SIZE))

    for off in relocs:
        val = load32(out_arena, off)
        if base <= val < base + ARENA_SIZE:
            put32(out_arena, off, val - base)

    #Also normalize statePtr+0x20 if subMachine was written there
    for dynamic_ptr_off in (0x0100 + 0x20, 0x0100 + 0x84, 0x0100 + 0x88):
        val = load32(out_arena, dynamic_ptr_off)
        if base <= val < base + ARENA_SIZE:
            put32(out_arena, dynamic_ptr_off, val - base)

    # Clear scratch hash-map sentinel, scratch nodes, and synthetic vtables
    out_arena[gl_off + 0xC4 : gl_off + 0xD0] = b"\x00" * 12
    out_arena[0x18C0:ARENA_SIZE] = b"\x00" * (ARENA_SIZE - 0x18C0)

    eax = u32(res.eax)
    if spec.result_kind == "ptr" and base <= eax < base + ARENA_SIZE:
        eax -= base
    elif spec.result_kind == "void":
        eax = 0

    return eax, rt.event_hash, bytes(out_arena), res.stack_cleanup


def run_checker(
    proc: subprocess.Popen[str],
    spec: Spec,
    arena: bytearray,
    relocs: list[int],
) -> tuple[int, int, bytes]:
    # Ensure dynamic pointer slots are in relocs so checker un-relocates them on output
    full_relocs = list(relocs)
    for dyn_off in (0x0100 + 0x20, 0x0100 + 0x84, 0x0100 + 0x88):
        if dyn_off not in full_relocs:
            full_relocs.append(dyn_off)
    reloc_str = "-" if not full_relocs else ",".join(f"{r:x}" for r in full_relocs)
    line = f"{spec.va:08x} {reloc_str} {arena.hex()}\n"
    assert proc.stdin is not None and proc.stdout is not None
    proc.stdin.write(line)
    proc.stdin.flush()
    resp = proc.stdout.readline().strip()
    if not resp or resp == "error":
        raise RuntimeError(f"checker error on 0x{spec.va:08X}")
    parts = resp.split()
    eax = int(parts[0], 16)
    ev = int(parts[1], 16)
    out_arena = bytearray.fromhex(parts[2])
    out_arena[0x18C0:ARENA_SIZE] = b"\x00" * (ARENA_SIZE - 0x18C0)
    if spec.result_kind == "void":
        eax = 0
    return eax, ev, bytes(out_arena)


def run_suite(checker_path: Path, specs: list[Spec], seed: int, quiet: bool = False) -> tuple[int, int]:
    rng = random.Random(seed)
    emu = Emulator()
    rt = NativeStubRuntime(emu)
    proc = subprocess.Popen(
        [str(checker_path.resolve())],
        stdin=subprocess.PIPE,
        stdout=subprocess.PIPE,
        text=True,
        encoding="ascii",
    )

    total = 0
    mismatches = 0
    try:
        for spec in specs:
            fn_bad = 0
            for idx in range(spec.cases):
                arena, relocs, stack_args = build_scenario(spec, idx, rng)
                g_eax, g_ev, g_mem, g_clean = run_native(emu, rt, spec, arena, relocs, stack_args)
                c_eax, c_ev, c_mem = run_checker(proc, spec, arena, relocs)

                bad = (
                    g_eax != c_eax
                    or g_ev != c_ev
                    or g_mem != c_mem
                    or g_clean != spec.cleanup
                )
                if bad:
                    fn_bad += 1
                    if fn_bad <= 2 and not quiet:
                        diff_offs = [
                            f"0x{o:04x}:{g_mem[o]:02x}!={c_mem[o]:02x}"
                            for o in range(ARENA_SIZE)
                            if g_mem[o] != c_mem[o]
                        ]
                        print(
                            f"  MISMATCH 0x{spec.va:08X} ({spec.name}) case={idx}: "
                            f"eax={g_eax:08x}/{c_eax:08x} ev={g_ev:08x}/{c_ev:08x} "
                            f"clean={g_clean}/{spec.cleanup} diffs={diff_offs[:8]}"
                        )
                total += 1
            mismatches += fn_bad
            if not quiet:
                print(f"0x{spec.va:08X} {spec.name:50s} cases={spec.cases:2d} mismatches={fn_bad}")
    finally:
        if proc.stdin:
            proc.stdin.close()
        proc.wait(timeout=15)

    return total, mismatches


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "--checker",
        type=Path,
        default=Path("Ra3Source/build/win32/bin/Release/ra3_gamelogic7ccheck.exe"),
    )
    parser.add_argument(
        "--mutation-checker",
        type=Path,
        default=None,
    )
    parser.add_argument("--seed", type=int, default=0x007C2026)
    args = parser.parse_args()

    if args.mutation_checker is not None:
        mut_specs = [s for s in SPECS if s.va == 0x007CC920]
        m_total, m_bad = run_suite(args.mutation_checker, mut_specs, args.seed, quiet=True)
        print(
            f"mutation check (0x007CC920): cases={m_total} detected={m_bad} "
            f"status={'PASS' if m_bad > 0 else 'FAIL'}"
        )
        if m_bad == 0:
            return 1

    total, bad = run_suite(args.checker, SPECS, args.seed)
    print(f"gamelogic7c oracle: functions={len(SPECS)} cases={total} mismatches={bad}")
    return 1 if bad else 0


if __name__ == "__main__":
    raise SystemExit(main())
