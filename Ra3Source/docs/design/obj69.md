# Object-update 0x0069 reconstruction

## Scope and ownership

This batch keeps 41 clean, previously unanalyzed ledger entries whose starts
lie in `0x00690000`–`0x0069FFFF`. Each counted VA is a slot in a table from
`vtables_split.tsv`. The pointer was checked with
`work/probe/verify_ownership.py` (`fails=0`). Class names were read from the
binary. A slot-2 name getter is `mov eax, imm32; ret`, and the immediate is
split on NUL.

Where the counted slot's own slot 2 is not a name getter, the class is the
primary table installed by the constructor that also stores the secondary
table. Those stores were disassembled from `ra3_1.0.game`. Two slots on
`00C00F4C` have no such store; their notes say so and their C++ names do not
claim a class.

| Vtable | How the name was read |
| --- | --- |
| `00C11840` | slot 2 returns StancesBehavior |
| `00C21A98` | slot 2 returns HordeContain. `0073CB3B` stores it at `[esi]`. `0073CB72` stores `00C21784` at `[esi+0x38]`. `0073CB8A` stores `00C21520` at `[esi+0x16C]` |
| `00C22E28` | slot 2 returns SlaughterHordeContain |
| `00BFE7E8` | slot 2 returns InfiltratorContain. `006922F3` stores it at `[esi]`. `0069230E` stores `00BFE530` at `[esi+0x18]` |
| `00C1FB90` | slot 2 returns OpenContain. `0072782B` stores it at `[esi]`. `0072783F` stores `00C1F8F0` at `[esi+0x24]` |
| `00C1BA58` | slot 2 returns BeamSpecialAbilityUpdate |
| `00C0B860` | slot 2 returns SpecialPowerUpdateModule. `006B479A` is `lea edi, [esi+0x28]`. `006B479D` stores the primary at `[esi]`. `006B47B4` stores `00BF7D10` at `[esi+0x24]`. `006B47BB` stores `00BF7C68` at `[edi]` |
| `00C13640` | slot 2 returns UncombineSpecialPower. `006CB444` stores it at `[esi]`. `006CB46D` stores `00C13440` at `[esi+0x28]`. Slot 130 of `00C13440` is the same name getter |
| `00C1E918` | slot 2 returns WeaponFireSpecialAbilityUpdate |
| `00C25398` | slot 2 returns SupplyTruckAIUpdate. `007522BB` stores it at `[esi]`. `007522EB` stores `00C251A8` at `[esi+0x51C]` |
| `00C0BD58` | slot 2 returns DockUpdate. `00740541` stores it at `[esi]`. `00740555` stores `00C0BBB0` at `[esi+0x24]` |
| `00C01460` | slot 2 is `0065F220` (`xor eax, eax; ret`). Slot 78 returns DefaultProductionExitUpdate |
| `00C22118` | slot 2 returns AIUpdateInterface |
| `00C0CFC8` | slot 2 returns FireWeaponUpdate. `006CD898` stores it at `[edi]`. `006CD8A5` stores `00C0CE84` at `[edi+0x10]` |
| `00C03DC0` | slot 2 is `0065F220`. Slot 78 returns InvisibilityBroadcastUpdate |
| `00C19788` | slot 2 returns ProductionUpdate. `006E8ED7` stores it at `[esi]`. `006E8EEB` stores `00C19580` at `[esi+0x24]` |

`00C0A7D0` is not given a slot-2 name (`007265A0` is an ordinary method).
The constructor that begins after the padding at `0074D290` stores HordeContain
`00C21A98` at `[esi]` (`0074D2C6`), later stores `00C0A7D0` at `[edi]`
(`0074D555`), and publishes that object at `[esi+0x294]` (`0074D570`).

`00C00F4C` slot 2 is `0067E580`, which is not a name getter. A full-image
search found the immediate `00C00F4C` only as the absolute operand of
`call dword ptr [0x00C00F4C]` at `006CAC8B`. `006CAC5D` stores SpecialPowerModule
`00C01108`, not `00C00F4C`.

Boilerplate in the counted set: 0 of 41. Name getters, `xor eax, eax`
stubs, and `ret imm` stubs were left out.

## Inventory

| VA | Size | Class and slot | Function |
| --- | --- | --- | --- |
| `00690740` | 19 | StancesBehavior `00C11840` slot 31 | `stancesBehaviorFirstNodeValue` |
| `006911D0` | 115 | HordeContain secondary `00C21520` slot 138 | `hordeContainDecayRange` |
| `00691350` | 36 | HordeContain secondary `00C21784` slot 0 | `hordeContainInvertSlotF4` |
| `006914B0` | 45 | HordeContain secondary `00C21520` slot 8 | `hordeContainFirstPassengerId` |
| `006914E0` | 9 | HordeContain secondary `00C21520` slot 62 | `hordeContainField5cClear` |
| `006914F0` | 81 | HordeContain secondary `00C21520` slot 121 | `hordeContainNotifyPassengerList` |
| `00691550` | 79 | HordeContain secondary `00C21520` slot 122 | `hordeContainNotifyPassengerListShort` |
| `00691990` | 77 | SlaughterHordeContain `00C22E28` slot 43 | `slaughterHordeContainPasses` |
| `006919E0` | 74 | HordeContain record object `00C0A7D0` slot 12 | `hordeContainMarkRecord` |
| `00691A30` | 92 | HordeContain record object `00C0A7D0` slot 13 | `hordeContainCopyRecordVector` |
| `006923A0` | 29 | InfiltratorContain secondary `00BFE530` slot 53 | `infiltratorContainZeroSevenDwords` |
| `00692500` | 55 | OpenContain secondary `00C1F8F0` slot 42 | `openContainStorePairIds` |
| `00692580` | 112 | OpenContain secondary `00C1F8F0` slot 45 | `openContainPlaneAccepts` |
| `006925F0` | 122 | OpenContain secondary `00C1F8F0` slot 46 | `openContainPairPlaneAccepts` |
| `006926D0` | 9 | OpenContain secondary `00C1F8F0` slot 17 | `openContainHasField78` |
| `00692850` | 54 | OpenContain secondary `00C1F8F0` slot 4 | `openContainBitClear` |
| `00692890` | 26 | OpenContain secondary `00C1F8F0` slot 43 | `openContainCopySevenDwords` |
| `00694670` | 47 | BeamSpecialAbilityUpdate `00C1BA58` slot 43 | `beamSpecialAbilityAllowsTarget` |
| `00694710` | 20 | SpecialPowerUpdateModule secondary `00BF7D10` slot 5 | `specialPowerUpdateIdMatches` |
| `00694730` | 15 | SpecialPowerUpdateModule secondary `00BF7C68` slot 38 | `specialPowerUpdateReadFieldC` |
| `00695670` | 10 | `00C00F4C` slot 18, class not named by slot 2 | `table00C00F4CStoreByte39` |
| `00695690` | 15 | `00C00F4C` slot 31, class not named by slot 2 | `table00C00F4CReadField54` |
| `006966B0` | 20 | SpecialPowerUpdateModule secondary `00BF7C68` slot 0 | `specialPowerUpdateIdEquals` |
| `00696870` | 23 | SpecialPowerUpdateModule secondary `00BF7C68` slot 6 | `specialPowerUpdateReadSlot1` |
| `00697100` | 43 | UncombineSpecialPower secondary `00C13440` slot 30 | `uncombineSpecialPowerHasEntries` |
| `00697130` | 15 | WeaponFireSpecialAbilityUpdate `00C1E918` slot 42 | `weaponFireSpecialAbilityFlagBit8` |
| `00697720` | 26 | SupplyTruckAIUpdate secondary `00C251A8` slot 2 | `supplyTruckSumDwords` |
| `006978D0` | 40 | SupplyTruckAIUpdate secondary `00C251A8` slot 19 | `supplyTruckRangeScale` |
| `006979E0` | 91 | DockUpdate secondary `00C0BBB0` slot 0 | `dockUpdateIdPresentOrOpen` |
| `00697A40` | 83 | DockUpdate secondary `00C0BBB0` slot 4 | `dockUpdateSlotReady` |
| `00697AA0` | 72 | DockUpdate secondary `00C0BBB0` slot 8 | `dockUpdateMarkId` |
| `006981B0` | 264 | DefaultProductionExitUpdate `00C01460` slot 98 | `defaultProductionExitTransformPoint` |
| `00699710` | 103 | AIUpdateInterface `00C22118` slot 158 | `aiUpdatePathGoalActive` |
| `006997B0` | 38 | AIUpdateInterface `00C22118` slot 151 | `aiUpdateClearGoalFloats` |
| `006997E0` | 94 | AIUpdateInterface `00C22118` slot 153 | `aiUpdateSetGoalPosition` |
| `00699840` | 27 | AIUpdateInterface `00C22118` slot 155 | `aiUpdateSetGoalFloat` |
| `00699860` | 51 | AIUpdateInterface `00C22118` slot 157 | `aiUpdateLocomotorAllowsMove` |
| `00699CA0` | 36 | AIUpdateInterface `00C22118` slot 66 | `aiUpdatePulseModule3c` |
| `0069B5A0` | 15 | FireWeaponUpdate secondary `00C0CE84` slot 2 | `fireWeaponUpdateReadFieldC` |
| `0069C1B0` | 11 | InvisibilityBroadcastUpdate `00C03DC0` slot 92 | `invisibilityBroadcastAdjustor24` |
| `0069E5B0` | 75 | ProductionUpdate secondary `00C19580` slot 37 | `productionUpdateConsumeMatchingLink` |

## Layouts

- HordeContain decay (`006911D0`) reads the anchor through `this-0x164`. A positive difference from the anchor's `+0x120` stores `value * 0.9` (bits `0x3F666666`) at `+0x160` and returns `min(6, (1 - value * 0.01) * argument)`. The float constants live at `00BD9A34`, `00BD925C`, and `00BF2298`.
- Record marks (`006919E0`, `00691A30`) use a 60-byte stride. The count is the signed division by 60 that the bytes implement with `imul 0x88888889`.
- OpenContain plane tests (`00692580`, `006925F0`) reject a null candidate, a negative limit at `body-0x20+0x18`, or a Z separation above that limit. The dot product uses X/Y at `+0x38`/`+0x3C`.
- `00692500` reads both ids before it finishes the stores. The count is raised to 1 when it is less than or equal to 1.
- Dock slot readiness (`00697A40`) requires the visitor id, a nonzero flag, and a previous empty id. Index 0 fails the previous-empty test. The negative-index path clears only `al`.
- `006981B0` returns 0 when the matrix pointer at `+8` is null. Otherwise it writes the 3x4 product of the point at `[[this+4]+8]`.
- `006997E0` releases the held object at `+0x150` when the mode is not 4 and the decremented count is less than or equal to 0, then copies three dwords to `+0x220`.
- `0069E5B0` returns 0 immediately when a matching link has byte `+0x64` clear. It does not continue to the next link.

## Oracle and mutation

`ra3_obj69check` and `verify_obj69.py` compare the clean game bytes with the
reconstruction on one relocated blob. Each of the 41 entries has at least two
cases, including the empty, null, and boundary inputs described above.
Release result after the mutation was restored:

`obj69 oracle: 41 entries, 127 cases, 0 mismatches`

Mutation: `aiUpdateSetGoalFloat` stored the bits at `+0x224` instead of
`+0x220`. The oracle failed on `0x00699840` (`blob mismatch at +0x220`,
game `0x00`, cpp `0xA5`). The offset was restored, the checker rebuilt, and
the oracle returned to 0 mismatches. Debug `ra3_obj69check` was built after that.

## Hooks

No hook keys.

| Key | VA | Expected bytes | explicitOnly |
| --- | --- | --- | --- |
| (none) | | | |

## Deferred

Not counted:

- `00692380` is the shared `xor eax, eax` stub installed on many tables.
- `00695660` is `ret 0x14` on View.
- `00695610` is SpecialPowerModule `00C01108` slot 4. It caches the string
  pointer `00C0116C` through the globals at `00DF8ED4` and `00E1A078`.
- Slot-2 name getters in the range, including `00692330` (InfiltratorContain),
  `00695600` (SpecialPowerModule), `00698130` (DefaultProductionExitUpdate),
  and `0069C170` (InvisibilityBroadcastUpdate).

`00695670` and `00695690` are counted, with the missing class name stated
above, because the vtable slots themselves are real and the bodies are a
byte store and a field read.
