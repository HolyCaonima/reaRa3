# Object-update 0x0069 second pass

## Scope and ownership

This batch keeps 40 clean, previously unanalyzed ledger entries whose starts
lie in `0x00690000`–`0x0069FFFF`. Each counted VA is a slot in a table from
`vtables_split.tsv`. The pointer was checked with
`work/probe/verify_ownership.py` (`fails=0`). A slot-2 name getter is
`mov eax, imm32; ret`, and the immediate is split on NUL.

Where the counted slot's own slot 2 is not a name getter, the class is the
primary table installed by the constructor that also stores the secondary
table. Those stores were disassembled from `ra3_1.0.game`. One slot,
`0069c350` on `00C03DA4`, has no constructor store; the only store is the
destructor `0069c1c0` writing the table at `this+0x24`, and its slot 2
(`006cdb90`) is not a name getter. The C++ name does not invent a class.

No EASTL instantiation, compiler-generated member copy, or same-shape factory
is counted. The five fire packers and the five dispatch walks differ by
argument layout or by the virtual slot they call. Boilerplate counted: 0.
Slot-2 name getters and the size-47 static NAMEKEY caches in this range were
left out.

Direct callees that are not part of this batch are bound by the oracle to the
same return sequence on both sides. `0x004D5BD0` and `0x004D5B10` are the
already reconstructed mask helpers and run as real code. `0x007E1930` is the
17-byte `movss [ecx+0xF8], arg; ret 4` and is executed as-is.

## Counted functions

| VA | C++ name | Ownership |
| --- | --- | --- |
| `00690AF0` | `salvageCrateCollideAccepts` | `00C1A178` slot 22, slot 2 `SalvageCrateCollide` |
| `00690E60` | `fireWeaponCollideMasksAllow` | `00BF7488` slot 22, slot 2 `FireWeaponCollide` |
| `00691250` | `hordeContainAnchorRejected` | `00C21520` slot 150; `0073CB3B` stores `HordeContain` `00C21A98`; `0073CB8A` stores `00C21520` at `+0x16C` |
| `006912E0` | `hordeContainForwardId` | `00C21520` slot 45; same constructor |
| `00691660` | `hordeContainRelayAnchor` | `00C21520` slot 135; same constructor |
| `00691C30` | `hordeContainRecordSpanPointer` | `00C0A7D0` slot 107; HordeContain constructor `0074D290` stores `00C21A98` at `[esi]` (`0074D2C6`), then allocates a 0x7C-byte helper whose vtable `00C0A7D0` it stores at `0074D555` (controller: `0074D2C6` is mid-function; the helper is owned by HordeContain, not HordeContain itself) |
| `00691CB0` | `hordeContainSelectRecordId` | `00C0A7D0` slot 104; same record constructor |
| `00691E20` | `garrisonContainSlotPointer` | `00C21BF0` slot 62; `0074DFD6` stores `GarrisonContain` `00C21E90`; `0074DFEA` stores `00C21BF0` at `+0x24` |
| `00691FC0` | `garrisonContainCopySlotTriple` | `00C21BF0` slot 71; same constructor |
| `00692270` | `garrisonContainReleaseIfCount` | `00C21BC8` slot 2; `0074DFFF` stores `00C21BC8` at `+0x30` in that constructor |
| `00692670` | `openContainRefreshMask` | `00C1FB90` slot 29, slot 2 `OpenContain` |
| `00692AC0` | `transportContainRedirect` | `00C200A0` slot 10, slot 2 `TransportContain` |
| `00694F00` | `scavengerSpecialPowerPushScale` | `00C18DE0` slot 10; `006E2954` stores `ScavengerSpecialPower` `00C18FB8`; `006E2961` stores `00C18DE0` at `+0x10` |
| `00695290` | `specialPowerDispatchAnyReady` | `00C19058` slot 1; `006E2A34` stores `SpecialPowerDispatchSpecialPower` `00C19230`; `006E2A41` stores `00C19058` at `+0x10` |
| `00695300` | `specialPowerDispatchAnyOrPending` | `00C19058` slot 4; same constructor |
| `00695370` | `specialPowerDispatchStoreArg` | `00C19058` slot 9; same constructor |
| `00695490` | `specialPowerDispatchNotifyClear4C` | `00C19058` slot 19; same constructor |
| `006954F0` | `specialPowerDispatchNotifyClear54` | `00C19058` slot 21; same constructor |
| `006963F0` | `specialPowerModuleScaleIfFlag` | `00C00FD8` slot 73; `006955C2` stores `SpecialPowerModule` `00C01108`; `006955C8` stores `00C00FD8` at `+0xC` |
| `006968A0` | `specialPowerUpdateFirePacked` | `00BF7C68` slot 11; `006B479D` stores `SpecialPowerUpdateModule` `00C0B860`; `006B47BB` stores `00BF7C68` |
| `006968F0` | `specialPowerUpdateFirePair` | `00BF7C68` slot 12; same constructor |
| `00696940` | `specialPowerUpdateFireInsert` | `00BF7C68` slot 13; same constructor |
| `00696990` | `specialPowerUpdateFireFloat` | `00BF7C68` slot 14; same constructor |
| `006969E0` | `specialPowerUpdateFireSpread` | `00BF7C68` slot 15; same constructor |
| `00696C50` | `specialPowerUpdateScaleIfFlag` | `00C0B730` slot 73; `006B47A3` stores `00C0B730` at `+0xC` in that constructor |
| `00697680` | `dozerAiUpdateIssueOrder` | `00C24878` slot 29; `00751D25` stores `DozerAIUpdate` `00C24A50`; `00751D5B` stores `00C24878` |
| `00697B50` | `dockUpdateForwardVisitor` | `00C0BBB0` slot 22; `00740541` stores `DockUpdate` `00C0BD58`; `00740555` stores `00C0BBB0` at `+0x24` |
| `006983B0` | `queueProductionExitProject` | `00BF851C` slot 11; `006982E3` stores `QueueProductionExitUpdate` `00C01758`; `006982F7` stores `00BF851C` at `+0x24` |
| `006984E0` | `queueProductionExitIdle` | `00BF851C` slot 1; same constructor |
| `00699BC0` | `aiUpdateStampGoal` | `00C22118` slot 23, slot 2 `AIUpdateInterface` |
| `00699FB0` | `aiUpdateGoalKindAllowed` | `00C22118` slot 147, slot 2 `AIUpdateInterface` |
| `0069A040` | `aiUpdateApplyFacing` | `00C22118` slot 104, slot 2 `AIUpdateInterface` |
| `0069C350` | `invisibilityMemberScale` | `00C03DA4` slot 0; slot 2 is not a name getter; `0069C1C3` stores the table at `+0x24` |
| `0069C6B0` | `invisibilityUpdateArgsDiffer` | `00C03F64` slot 2; `0069C3C0` stores `InvisibilityUpdate` `00C040B0`; `0069C3D3` stores `00C03F64` at `+0x24` |
| `0069CCD0` | `leechTargetingAttachForward` | `00C209E8` slot 28, slot 2 `LeechTargetingAttachUpdate` |
| `0069E6F0` | `productionUpdateNextReadyLink` | `00C19580` slot 30; `006E8ED7` stores `ProductionUpdate` `00C19788`; `006E8EEB` stores `00C19580` at `+0x24` |
| `0069FB60` | `specialAbilityUpdateKindMatch` | `00C14184` slot 3; `006CF1A0` stores `SpecialAbilityUpdate` `00C142D8`; `006CF1AD` stores `00C14184` at `+0x24` |
| `0069FBC0` | `specialAbilityUpdateIsReady` | `00C142D8` slot 25, slot 2 `SpecialAbilityUpdate` |
| `0069FCD0` | `specialAbilityUpdateResolveOut` | `00C14384` slot 2; `006CF199` stores `00C14384` at `+0x10` in that constructor |
| `0069FEC0` | `stealthDetectorUpdateTouch` | `00C0DFC0` slot 23, slot 2 `StealthDetectorUpdate` |

`00691E20` with an index above `0x13` returns the global `0x00DF8DD0`.
`00691C30` returns `record+0x38` when the signed span divided by 40 is not 1,
and returns the global `0x00DF8DC0` when the count is 1. The calls those paths
make into the global publisher are stubbed; the returned address matches.

`0069FEC0` returns `this` when the body byte at `+0x98` is clear. The armed
path loads the singleton at `0x00DEBA64` and calls `0x004ED9C0`. That global is
zero in the on-disk image, so the oracle does not enter it and the port
returns `this` there as well.

`0069FBC0` decides the final float with `ucomiss` / `lahf` / `test ah, 0x44` /
`jp`, which on a real CPU is ordered `== 0.0f` (NaN is not equal). This
Unicorn build leaves PF clear after `test`, so the oracle covers the virtual
hit and the blocked-byte paths and does not claim the equal-float edge.

## Dropped candidates

Deleting destructors that store vtables and call `0x788640`, often then
`0x4166B0`: `00690DC0`, `00692FB0`, `00693400`, `00693770`, `00693850`,
`00698170`, `00698370`, `006987C0`, `00698E00`, `0069BB30`, `0069C1C0`,
`0069C4F0`, `0069C830`, `0069CCF0`, `0069D210`, `0069F890`.

Size-36 slot-0 deleting wrappers that call a base and then optionally
`0x4166B0`: `00693260`, `00693F80`, `00693FD0`, `00694640`, `00697460`,
`00697900`, `00698F10`, `00691A90`.

`00692540` returns 0 or falls off the end. `006946E0` is a one-iteration
fallthrough. `00694E80` and `0069D490` have no proven constructor store of
their tables. `006966D0`, `00691620`, `006991B0`, `00694850`, `00697020`, and
`00691E90` read a `.data` global or a static-init guard on the path that
completes. `00697F70` (`SupplyWarehouseDockUpdate`) is a 325-byte body whose
only early test is `[this+0x68]==0`; the rest was not reconstructed.

## Oracle and mutation

`obj69b oracle: 40 entries, 126 cases, 0 mismatches`

Mutation: `queueProductionExitIdle` read `this+0x1C` changed to `this+0x20`.
The oracle failed with
`idle 0x006984E0: return mismatch (u32): game=0xFFFFFFFF cpp=0x00000000`.
The offset was restored and the oracle returned to 0 mismatches. Release and
Debug `ra3_obj69bcheck` both build.

## Hooks

| key | VA | expected bytes | stolen | explicitOnly |
| --- | --- | --- | --- | --- |
| (none) | | | | |

Controller note: the constructor addresses `006955C2` and `006CF197` cited above are not function starts. They are the code right after the SecuROM cave jumps inside the inner-patched constructors `0x00695550` (SpecialPowerModule) and `0x006CF170` (SpecialAbilityUpdate); the vtable stores they contain are real. `0074D2C6` is likewise inside the HordeContain constructor `0x0074D290`, which has no ledger entry.
