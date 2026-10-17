# Protected Cave Batch 3 (`recon/protcave3`)

## Overview

`recon/protcave3` reconstructs **28** SecuROM cave-patched functions (`24` `inner-patched` and `4` `entry-recoverable`, all `resolution=cave` in `securom_sites.tsv`) into `Ra3Source/src/gamelogic/include/ra3/gamelogic/ProtectedCaveLogic3.h` and `Ra3Source/src/gamelogic/src/ProtectedCaveLogic3.cpp`. Each function is verified by differential execution against `ra3_1.0.game` (patched with the runtime `.securom` dump `0x012B6000..0x019D1000` and pointer table `0x018ED000..0x018FA000`) via `ra3_protcave3check` and `Ra3Source/tools/analysis/ports/verify_protcave3.py` (`cases 84 mismatches 0`).

Three functions previously deferred by `recon/drawable52` (`0x00523E60`, `0x005242B0`, `0x0052D6B0`) were rescued in this batch by reading their runtime cave stubs and continuations.

## Reconstructed Functions and Cave Analysis

| # | Function VA | C++ Name | Class | Site VA | Slot VA | Cave Target | Cont VA | Recovered Cave Instruction(s) | Ownership Chain |
|---|-------------|----------|-------|---------|---------|-------------|---------|-------------------------------|-----------------|
| 1 | `0x004CB260` | `runInitZeroAndCall4CB200` | `inner-patched` | `0x004CB268` | `0x018F2E38` | `0x0044DD7F` | `0x004CB26E` | `mov dword ptr [esi], 0` | callee of `0x005362E0` at `0x0053652A`; `0x005362E0` is `0x00BE0BD0` slot 5 |
| 2 | `0x004D66D0` | `runDestroyObjectBDB8E8` | `entry-recoverable` | `0x004D66D0` | `0x018F25C8` | `0x00459C86` | `0x004D66D6` | `mov dword ptr [ecx], 0xBDB8E8` | callee of `0x00785B90` at `0x00785DC4`; `0x00785B90` is `0x00C22118` slot 168 (`AIUpdateInterface`) |
| 3 | `0x004D99E0` | `runAllocateFromPowerOfTwoPool` | `inner-patched` | `0x004D99E5` | `0x018ED488` | `0x0042055A` | `0x004D99EB` | `cmp esi, dword ptr [0x018F9C38]` | callee of `0x00541370` at `0x005413A5`; `0x00541370` is `0x00BE1468` slot 27 |
| 4 | `0x004DA710` | `runConstructObjectBDC368` | `inner-patched` | `0x004DA716` | `0x018F2570` | `0x0044F45B` | `0x004DA71C` | `mov dword ptr [esi], 0xBDC368` | callee of `0x00745AF0` at `0x00745B30`; `0x00745AF0` is `0x00BF99D8` slot 1 |
| 5 | `0x004E5C90` | `runClearLists8CAnd94` | `inner-patched` | `0x004E5C95` | `0x018F2BB4` | `0x004440D2` | `0x004E5C9C` | `cmp dword ptr [esi + 0x8C], 0` | callee of `0x004F6C50` at `0x004F6E97`; `0x004F6C50` is `0x00BDCC74` slot 4 |
| 6 | `0x004EDD70` | `runDeactivateAndEnqueueObject138` | `inner-patched` | `0x004EDD7C` | `0x018EDA00` | `0x004404A6` | `0x004EDD83` | `test byte ptr [edi + 0x80], 1` | callee of `0x004F5380` at `0x004F54B7`; `0x004F5380` is `0x00BDD028` slot 7 |
| 7 | `0x00504510` | `runAssignRefObjectAtOffset94` | `inner-patched` | `0x00504522` | `0x018ED658` | `0x0040B62E` | `0x00504529` | `add dword ptr [eax + 0x98], 1` | callee of `0x0050E710` at `0x0050E7A9`; `0x0050E710` is `0x00BDF5A8` slot 99 |
| 8 | `0x0050A9C0` | `runApplyIndexedTableScaleToAudioObject` | `inner-patched` | `0x0050A9D3` | `0x018ED9F8` | `0x0044026E` | `0x0050A9D9` | `imul esi, esi, 0x1FC` | callee of `0x005138F0` at `0x0051392A`; `0x005138F0` is `0x00BDF5A8` slot 79 |
| 9 | `0x00520120` | `runSetDrawableBit13CAndNotify394` | `entry-recoverable` | `0x00520120` | `0x018F285C` | `0x00401B26` | `0x00520126` | `mov dl, byte ptr [ecx + 0x13C]` | callee of `0x00763BC0` at `0x007646C7`; `0x00763BC0` is `0x00C19644` slot 0 |
| 10 | `0x00523E60` | `runQueryDrawableModules178` | `inner-patched` | `0x00523E62` | `0x018ED3AC` | `0x0040D226` | `0x00523E68` | `mov esi, dword ptr [ecx + 0x178]` | callee of `0x006B7E30` at `0x006B7FD8`; `0x006B7E30` is `0x00BF89A0` slot 12 (`LaserUpdate`) |
| 11 | `0x005242B0` | `runMarkDrawableSlotFlags224And234` | `inner-patched` | `0x005242C3` | `0x018EDA54` | `0x004128B0` | `0x005242C9` | `mov ecx, dword ptr [esi + 0x224]` | callee of `0x008742C0` at `0x008742FB`; `0x008742C0` is `0x00C50128` slot 5 |
| 12 | `0x0052D6B0` | `runSetDrawableField384AndSyncVisibility` | `inner-patched` | `0x0052D6BA` | `0x018ED3A0` | `0x0040A37E` | `0x0052D6C0` | `mov dword ptr [esi + 0x384], eax` | callee of `0x0075D150` at `0x0075D223`; `0x0075D150` is `0x00C21BF0` slot 22 |
| 13 | `0x00535410` | `runCreateRefObjectBE03E4` | `inner-patched` | `0x00535428` | `0x018ED604` | `0x0042DBC6` | `0x0053542F` | `mov dword ptr [eax + 4], 0` | callee of `0x004D8520` at `0x004D8557`; `0x004D8520` is `0x00BDC404` slot 1 |
| 14 | `0x00537A40` | `runRemoveFlaggedDrawableFromList50` | `inner-patched` | `0x00537A48` | `0x018F29B8` | `0x00420536` | `0x00537A4F` | `test byte ptr [ebx + 0x13D], 0x40` | callee of `0x005386A0` at `0x0053873A`; `0x005386A0` is `0x00BE1468` slot 32 |
| 15 | `0x00538440` | `runConstructObjectBE082C` | `inner-patched` | `0x0053844A` | `0x018F2A04` | `0x00456F76` | `0x00538450` | `mov dword ptr [edi], 0xBE082C` | callee of `0x00540F40` at `0x00540F81`; `0x00540F40` is `0x00BE1174` slot 7 |
| 16 | `0x0055B530` | `runDestroyAudioEventBDF2C4` | `inner-patched` | `0x0055B534` | `0x018ED650` | `0x004036AA` | `0x0055B53A` | `lea ecx, [esi + 0x90]` | callee of `0x0051C890` at `0x0051CCB1`; `0x0051C890` is `0x00BDF5A8` slot 26 |
| 17 | `0x0055CC60` | `runConstructObjectBE2CD8` | `inner-patched` | `0x0055CC63` | `0x018ED450` | `0x004025EC` | `0x0055CC69` | `mov dword ptr [esi], 0xBE2A48` | callee of `0x0055DC20` at `0x0055DC42`; `0x0055DC20` is `0x00BE2BDC` slot 5 |
| 18 | `0x005838F0` | `runReleaseLockedObjects6C68And6C6C` | `inner-patched` | `0x00583900` | `0x018EDBC4` | `0x0043A04F` | `0x00583906` | `mov ecx, dword ptr [esi + 0x6C68]` | callee of `0x0058D000` at `0x0058D000`; `0x0058D000` is `0x00BE4D18` slot 4 |
| 19 | `0x005A4ED0` | `runReleaseAndClearHandleVector524` | `entry-recoverable` | `0x005A4ED0` | `0x018F27D4` | `0x0040BA01` | `0x005A4ED7` | `cmp dword ptr [0x00DF6F94], 0` | callee of `0x005A4F50` at `0x005A4F56`; `0x005A4F50` is `0x00BE5C8C` slot 0 |
| 20 | `0x005C5D70` | `runDestroyObjectBE97D4` | `inner-patched` | `0x005C5D76` | `0x018ED958` | `0x00450B64` | `0x005C5D7C` | `mov dword ptr [esi], 0xBE97D4` | callee of `0x005303D0` at `0x005303E4`; `0x005303D0` is `0x00BE0AC0` slot 0 |
| 21 | `0x005EF110` | `runConstructObjectBE2030` | `inner-patched` | `0x005EF115` | `0x018F2B10` | `0x0043A77D` | `0x005EF11B` | `mov dword ptr [esi], 0xBE2030` | callee of `0x005BEB80` at `0x005BED6A`; `0x005BEB80` is `0x00BDEDF0` slot 3 |
| 22 | `0x006232F0` | `runShutdownGlobalsDF6BC8AndDF6B34` | `entry-recoverable` | `0x006232F0` | `0x018F2E58` | `0x00455491` | `0x006232F7` | `cmp dword ptr [0x00DF6BC8], 0` | callee of `0x005D27C0` at `0x005D2D0F`; `0x005D27C0` is `0x00BDEDF0` slot 14 |
| 23 | `0x0064BD80` | `runUpdateSubObject3CAndReleaseB0` | `inner-patched` | `0x0064BD9A` | `0x018F26AC` | `0x00445F07` | `0x0064BDA0` | `add edi, dword ptr [0x018F9E50]` | callee of `0x0064E3C0` at `0x0064E43E`; `0x0064E3C0` is `0x00BF22E8` slot 10 |
| 24 | `0x00683140` | `runSetObjectField180` | `inner-patched` | `0x00683144` | `0x018F24C4` | `0x00459661` | `0x0068314A` | `mov dword ptr [ecx + 0x180], eax` | callee of `0x00797FA0` at `0x00797FC9`; `0x00797FA0` is `0x00C2AE60` slot 4 (`UnitAIEndPathMoveState`) |
| 25 | `0x00685EB0` | `runReadSubObject4Byte144` | `inner-patched` | `0x00685EB3` | `0x018ED43C` | `0x00459EF6` | `0x00685EB9` | `mov al, byte ptr [eax + 0x144]` | callee of `0x007A4FD0` at `0x007A50D4`; `0x007A4FD0` is `0x00C2C9E0` slot 5 (`AIAttackFireWeaponState`) |
| 26 | `0x006879B0` | `runDetachSubObject238` | `inner-patched` | `0x006879B2` | `0x018ED738` | `0x00459DE0` | `0x006879B8` | `mov ecx, dword ptr [eax + 0x238]` | callee of `0x007F12B0` at `0x007F136E`; `0x007F12B0` is `0x00C35158` slot 1 (`TunnelTracker`) |
| 27 | `0x00688190` | `runConstructObjectBF9CBC` | `inner-patched` | `0x00688198` | `0x018ED554` | `0x004399A6` | `0x0068819F` | `mov dword ptr [eax + 4], 0` | callee of `0x007B5E50` at `0x007B5F94`; `0x007B5E50` is `0x00C2CFC8` slot 4 (`AIMeleeReAcquireState`) |
| 28 | `0x0071C460` | `runPropagateObjectMap418ToModules310` | `inner-patched` | `0x0071C467` | `0x018ED4FC` | `0x00456E6C` | `0x0071C46D` | `mov eax, dword ptr [ebp + 0x418]` | callee of `0x007210D0` at `0x00721180`; `0x007210D0` is `0x00C14790` slot 0 |

## Deferred and Excluded Candidates

### Deferred Candidates
- `0x007461C0`: Previously deferred in `recon/protcave2` because its claimed caller `0x0074634F` is a mid-function Ghidra mis-split inside `0x00746280` (and `0x00C8CA1C` is a 1-slot `data_pointer`, not a vtable). Tracing callers of the real function `0x00746280` shows only `0x007571D0` (call site `0x007572BA`) and `0x00780ED0` (call site `0x00780F58`), neither of which has a verified vtable or string-xref ownership chain. Kept `unanalyzed` with `deferred:` note.
- `0x0052CD70`: Previously deferred in `recon/drawable52`. Disassembly shows a 6-byte `entry-recoverable` thunk (`jmp dword ptr [0x018EDC98]` -> cave `0x00436526` -> `0x0052CD76`) with no standalone closed body at `0x0052CD70`. Kept `unanalyzed` with `deferred:` note.

### Excluded Candidates
- `0x004A41E0`: CRT helper `__eh_vector_constructor_iterator` (`excluded`).
- `0x0054C650`: 9-byte compiler-generated member-wise copy helper (`excluded`).
- `0x00503310`, `0x00509FB0`: `eastl::uninitialized_copy_ptr` template instantiations (`excluded`).
- `0x00551B30`: `eastl::rbtree::operator=` template instantiation (`excluded`).
- `0x006691E0`: `eastl::hashtable::find` template instantiation (`excluded`).
- `0x006F2DA0`: `eastl::map::operator[]` template instantiation (`excluded`).
- `0x0073A190`: `eastl::vector::resize` template instantiation (`excluded`).

## Verification

- **Ownership Verification**: `python D:\Game\Ra3\RevRa3\work\probe\verify_ownership.py D:\Game\Ra3\RevRa3-protcave3\work\protcave3_claims.txt` -> `checked 56 claims, 0 failed`.
- **Differential Oracle**: `python Ra3Source/tools/analysis/ports/verify_protcave3.py` -> `cases 84 mismatches 0` (3 cases per function across all 28 functions).
- **Deliberate Mutation Check**: Mutating `runSetObjectField180` (`writeU32(self, 0x180, value ^ 1u)`) produced `cases 84 mismatches 3` (exit code 1); reverting restored `cases 84 mismatches 0`.
