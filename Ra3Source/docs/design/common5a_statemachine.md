# StateMachine cluster (common5a)

RA3's `StateMachine` lives at vtable `0x00BE8178` (18 slots). Slot 2, `0x005B2440`, is `mov eax, 0x00BE81C0; ret` and that address holds the ASCII name `StateMachine`. `FailureState` is the sibling table `0x00BE6CB0`; its slot 2, `0x005AA2E0`, returns `FailureState` at `0x00BE6CFC`. The same base method set is copied onto the derived state-machine tables. Counted methods are either a slot of `0x00BE8178` / `0x00BE6CB0`, a direct callee of one of those methods, or a direct callee of a named vtable slot that passes the machine as `this`.

`0x005BA020` contains the assertion string `0x00BE90A8` (`StateMachine.cpp:1075`). `0x005C0A6A` (outside this batch's address window) contains `0x00BE95D8` (`StateMachine.cpp:1379`) and calls `0x005B24D0` and `0x005BDF30`. The vtable itself is installed by `mov dword ptr [eax], 0x00BE8178` at `0x005B6150` (already owned by `recon/hookcat`, not counted here). `0x00C114E8` is installed by `mov dword ptr [esi+0x18], 0x00C114E8` at `0x006C7840`.

Controller note: calling the anchored `0x005BA020` is not an ownership edge, so the two goal helpers that only did that now cite a vtable caller (checked with `work/probe/verify_ownership.py`, `fails=0`): `0x005BA110` is a direct callee of `0x0079E0C0` at `0x0079E0D2` (vtable `0x00C2AA80` slot 4, UnitAIAttemptSkipMoveState); `0x005BDCA0` is a direct callee of `0x007A2E50` at `0x007A2E61` (vtable `0x00C2C7D0` slot 4, AIAttackMeleeHordeWaitState).

Field picture used by the reconstruction: current state `+0x04`, state id `+0x04` on the state, success `+0x08`, failure `+0x0C`, condition records `+0x10`/`+0x14` (16-byte stride, cdecl predicate), owning machine `+0x18`, goal id `+0x28`, goal vec3 `+0x2C` and `+0x48`, scalar `+0x54`, path records `+0x58`/`+0x5C`, default id `+0x68`, query mode `+0x6C`, sleep frame `+0x74`, lock `+0x78`, re-entry `+0x79`, inited `+0x7A`. Immediates in the bytes are continue `0`, success `0xFFFFFFFF`, failure `0xFFFFFFFE`, invalid `0x000F423F`, machine-done `0x000F423E`, exit ids `0x270E`/`0x270F`, special id `0x000F423D`, recursion cap `0x14`. The goal-reset constant is the dword at `0x00BE700C` (`0x7F7FFFFF`).

Generals `StateMachine.h` / `StateMachine.cpp` supplied role names only. No Generals body is copied.

```
work/references/generals/GeneralsMD/Code/GameEngine/Include/Common/StateMachine.h
work/references/generals/GeneralsMD/Code/GameEngine/Source/Common/StateMachine.cpp
```

## Hook descriptors

Registered in `Ra3Source/src/hook/dll/targets/StateMachineTargets.cpp`. `expected16` and the stolen length come from `tools/analysis/disasm.py <VA> --hook-only`. Shadow compares the returned dword and returns the original. All three write either nothing or one idempotent dword.

| Hook Name | Address | Stolen | `expected16` | ABI / Return | `explicitOnly` | Notes |
|---|---|---|---|---|---|---|
| `smgetid` | `0x005A82E0` | 5 | `8B410485C074048B4004C3B83F420F00` | `__fastcall(this, edx) -> uint32` | `true` | current state id, or `0x000F423F` |
| `smgoaldword` | `0x005AA2A0` | 7 | `8B442404894170C20400CCCCCCCCCCCC` | `__fastcall(this, edx, uint32) -> uint32` | `true` | stores the argument at `+0x70` and returns it |
| `smpathat` | `0x005AC540` | 5 | `568B74240885F67C278B515C2B5158B8` | `__fastcall(this, edx, int32) -> uint32` | `true` | pointer to a 12-byte path record, or 0 |

All 3 targets are `explicitOnly=true`.

Functions that call a virtual, tail-jump `0x005B2500`, adjust `this` and jump, call `0x004166B0`, or return through x87 are reconstructed without a hook. `0x005AA190` (`fld dword ptr [0x00BE68E8]; ret`) is the x87 constant and is not counted.

`storeGoalScalarMutant` inverts the lock test of `0x005AA280`. It is not a shipped function. The oracle requires it to disagree with the shipped bytes on both lock states.
