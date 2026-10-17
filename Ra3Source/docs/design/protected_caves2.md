# Protected caves 2

Thirty game functions (`0x004D5550`..`0x007461C0`) whose ledger class is
`inner-patched` or `entry-recoverable`, including the thirteen functions
deferred from `recon/protcave` plus seventeen additional cave functions from the
`0x00400000`..`0x0074FFFF` range. Each has one `resolution=cave` row in
`securom_sites.tsv`: a `jmp [slot]` inside the function, a replaced instruction
at `runtime_target`, and a continuation in the clean image. The reconstruction
uses those replaced bytes plus the clean file bytes of the rest of the function.
Entry-lost bodies are out of scope. SecuROM's VM is not mapped or analysed. No
hooks.

Code: `src/gamelogic/include/ra3/gamelogic/ProtectedCaveLogic2.h`,
`src/gamelogic/src/ProtectedCaveLogic2.cpp`. Checker: `tools/protcave2check`.
Annotations: `docs/analysis/ra3_1.0/protcave2_annotations.tsv`.

## Evidence

Runtime capture `work/runtime_dump/run35/snapshot_3_240s`. The oracle loads
`.text`, `.rdata`, and `.data` from that capture and arms each cave slot to
the row's `runtime_target`, so the stub executes the replaced instruction and
returns to the continuation. Slot values that point at a VM trampoline are
overwritten and never executed.

Ownership for every counted function is either a `vtables_split.tsv` slot on
the function itself (`0x005E6B30` at `0x00BE2030` slot 4, `0x006CACE0` at
`0x00C00F4C` slot 13), or a direct `E8` call from a caller that is itself a
`vtables_split.tsv` slot. None of these thirty functions is a constructor.

## Inventory

Ledger size is the Ghidra span, which stops at the cave jump. Behaviour
continues at the continuation.

| VA | size | class | site | function | cases |
| --- | --- | --- | --- | --- | --- |
| 0x004D5550 | 49 | inner-patched | 0x004D5577 | runNotifySpecialPowerReady | 3 |
| 0x00502700 | 9 | inner-patched | 0x00502703 | runStopSubChannelStream | 4 |
| 0x00505060 | 6 | entry-recoverable | 0x00505060 | runUpdateLinearEnvelopeValue | 5 |
| 0x00513340 | 44 | inner-patched | 0x00513353 | runCreateWrappedRefSubObject | 2 |
| 0x0051F780 | 11 | inner-patched | 0x0051F785 | runComputeTransformedViewVector | 2 |
| 0x00521050 | 35 | inner-patched | 0x00521053 | runGetOrCreatePlayerTeamEntry | 3 |
| 0x005259E0 | 9 | inner-patched | 0x005259E3 | runFlushDirtySubObjects | 3 |
| 0x0054DF20 | 25 | inner-patched | 0x0054DF33 | runEnsureTerrainLogicSubHelper | 2 |
| 0x0057F540 | 14 | inner-patched | 0x0057F548 | runAppendFormattedProjectionStrings | 2 |
| 0x00598630 | 13 | inner-patched | 0x00598637 | runDestroySubObjectArrayAndReset | 2 |
| 0x005B5180 | 18 | inner-patched | 0x005B518C | runIsIndexedTableEntryEmpty | 3 |
| 0x005E65B0 | 10 | inner-patched | 0x005E65B4 | runDestroyGameInfoBase | 2 |
| 0x005E6B30 | 19 | inner-patched | 0x005E6B3D | runResetGameSlot | 2 |
| 0x00674640 | 32 | inner-patched | 0x0067465A | runRegisterEntryInTaggedMap | 2 |
| 0x0067E600 | 27 | inner-patched | 0x0067E60F | runFindModuleMatchingSubObject | 3 |
| 0x006885D0 | 23 | inner-patched | 0x006885E1 | runInitKindOfFilterPredicate | 3 |
| 0x0068C150 | 10 | inner-patched | 0x0068C154 | runHaveDifferentContainerStatuses | 3 |
| 0x006A8400 | 6 | entry-recoverable | 0x006A8400 | runTriggerWeaponInitAudioEvent | 2 |
| 0x006C0620 | 9 | inner-patched | 0x006C0623 | runResolveObjectIndicatorColor | 3 |
| 0x006C2C30 | 9 | inner-patched | 0x006C2C33 | runUpdateObjectPowerRadarStatusBit | 3 |
| 0x006C4F10 | 73 | inner-patched | 0x006C4F53 | runDispatchVec3CommandMessage | 2 |
| 0x006C5480 | 57 | inner-patched | 0x006C54B3 | runDispatchTargetCommandMessage3D | 2 |
| 0x006C8380 | 10 | inner-patched | 0x006C8384 | runDestroyActiveBodyBase | 2 |
| 0x006CACE0 | 12 | inner-patched | 0x006CACE6 | runTryExecuteSpecialPowerOnTarget | 3 |
| 0x006D1DD0 | 10 | inner-patched | 0x006D1DD4 | runRefreshObjectModulesAndDrawableState | 2 |
| 0x006E4760 | 217 | inner-patched | 0x006E476E | runFindSelectableObjectAndQueryModule44 | 2 |
| 0x006EFE40 | 9 | inner-patched | 0x006EFE43 | runResolveObjectByProducerId | 2 |
| 0x006F1350 | 22 | inner-patched | 0x006F1356 | runIsObjectMobileAndControllable | 3 |
| 0x0072CF30 | 13 | inner-patched | 0x0072CF37 | runApplyDrawableTintEnvelope | 2 |
| 0x007461C0 | 153 | inner-patched | 0x007461CD | runCopyWeaponAimTransformAndUpdate | 3 |

## Oracle

`python Ra3Source/tools/analysis/ports/verify_protcave2.py` verifies all 30
proof records (`proofs ok (30 functions)`) and builds 77 scenarios comparing
full-width EAX, EDX (stack residue excepted), stack cleanup, callbacks,
globals, and the pinned image against `ra3_protcave2check.exe`. Release build,
0 mismatches (`cases 77 mismatches 0`). Debug compiles.

Mutation: `runTriggerWeaponInitAudioEvent` (`0x006A8400`) flipped `idx < thresh`
to `idx >= thresh` when selecting between template sound references `+0xBC` and
`+0xC0`. The oracle reported 2 mismatches (`wpnaudio #47` and `wpnaudio #48`,
callback argument to `0x0055A0B0` swapped `0x1111` and `0x2222`). The comparison
was restored and a subsequent run reported `cases 77 mismatches 0`.

## Deferred

All thirteen functions deferred in `recon/protcave` (`0x006A8400`, `0x006C0620`,
`0x006C2C30`, `0x006C4F10`, `0x006C5480`, `0x006C8380`, `0x006CACE0`,
`0x006D1DD0`, `0x006E4760`, `0x006EFE40`, `0x006F1350`, `0x0072CF30`,
`0x007461C0`) are closed and counted in this batch. No functions from the
selected thirty were deferred or dropped.
