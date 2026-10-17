# Protected caves

Fifteen game functions whose ledger class is `inner-patched` or `entry-recoverable`.
Each has one `resolution=cave` row in `securom_sites.tsv`: a `jmp [slot]` inside
the function, a replaced instruction at `runtime_target`, and a continuation in
the clean image. The reconstruction uses those replaced bytes plus the clean
file bytes of the rest of the function. Entry-lost bodies are out of scope.
SecuROM's VM is not mapped or analysed. No hooks.

Code: `src/gamelogic/include/ra3/gamelogic/ProtectedCaveLogic.h`,
`src/gamelogic/src/ProtectedCaveLogic.cpp`. Checker: `tools/protcavecheck`.
Annotations: `docs/analysis/ra3_1.0/protcave_annotations.tsv`.

## Evidence

Runtime capture `work/runtime_dump/run35/snapshot_3_240s`. The oracle loads
`.text`, `.rdata`, and `.data` from that capture and arms each cave slot to
the row's `runtime_target`, so the stub executes the replaced instruction and
returns to the continuation. Slot values that point at a VM trampoline are
overwritten and never executed.

Ownership for every counted function is a direct `E8` from a caller that is
itself a `vtables_split.tsv` slot, or from a caller whose ledger `source_file`
is under SageEngine. The call site is inside that caller. None of these
functions is a constructor: none writes a vtable.

## Inventory

Ledger size is the Ghidra span, which stops at the cave jump. Behaviour
continues at the continuation.

| VA | size | class | site | function | cases |
| --- | --- | --- | --- | --- | --- |
| 0x00699780 | 6 | entry-recoverable | 0x00699780 | runSubControllerStateActive | 8 |
| 0x0069E780 | 29 | inner-patched | 0x0069E78E | runPartitionModifierCount | 6 |
| 0x006A51A0 | 16 | entry-recoverable | 0x006A51A0 | runQueryContainedModule | 4 |
| 0x006A6460 | 35 | inner-patched | 0x006A6463 | runUnregisterObjectSubsystems | 6 |
| 0x006A6DC0 | 7 | inner-patched | 0x006A6DC1 | runObjectPositionStep | 7 |
| 0x006A7600 | 6 | entry-recoverable | 0x006A7600 | runObjectControllingPlayer | 4 |
| 0x006B7040 | 11 | inner-patched | 0x006B7045 | runAssignAiTargetRef | 4 |
| 0x006BB360 | 15 | inner-patched | 0x006BB369 | runClassifyObjectKindMask | 9 |
| 0x006BD510 | 20 | inner-patched | 0x006BD51E | runObjectPassesKindFilter | 5 |
| 0x006C1500 | 10 | inner-patched | 0x006C1504 | runFindBehaviorModuleByKey | 5 |
| 0x006C1D60 | 9 | inner-patched | 0x006C1D63 | runReassignControllingPlayer | 4 |
| 0x006E5CE0 | 8 | inner-patched | 0x006E5CE2 | runClearNonZeroStatusWord | 4 |
| 0x00731530 | 10 | inner-patched | 0x00731534 | runRegisterActiveModuleListener | 4 |
| 0x007451E0 | 37 | inner-patched | 0x007451F5 | runNotifySixBucketListeners | 2 |
| 0x00747F10 | 9 | inner-patched | 0x00747F13 | runConsiderObjectIdle | 9 |

## Oracle

`python Ra3Source/tools/analysis/ports/verify_protcave.py` builds 81 scenarios
and compares full-width EAX, EDX (stack residue excepted), stack cleanup,
callbacks, globals, and the pinned image against `ra3_protcavecheck.exe`.
Release build, 0 mismatches. Debug compiles.

Mutation: `runClearNonZeroStatusWord` compared `field != 0` as `field == 0`.
The oracle reported 3 mismatches (`clear26c` cases 2-4, image byte at `+0x26C`).
The comparison was restored. A following run reported `cases 81 mismatches 0`.

## Deferred

Ten functions were dropped because no caller is a vtable slot or a SageEngine
function, or because the continuation is not a closed body: `0x00695550`,
`0x006A5FA0`, `0x006C0B60`, `0x006C6310`, `0x006D0330`, `0x006EE7B0`,
`0x00734490`, `0x0073C740`, `0x00741140`, `0x00747920`.

Thirteen anchored or draft functions stay empty because their bodies were not
matched here: `0x006A8400`, `0x006C0620`, `0x006C2C30`, `0x006C4F10`,
`0x006C5480`, `0x006C8380`, `0x006CACE0`, `0x006D1DD0`, `0x006E4760`,
`0x006EFE40`, `0x006F1350`, `0x0072CF30`, `0x007461C0`. Sites and reasons are
in `protcave_annotations.tsv`.
