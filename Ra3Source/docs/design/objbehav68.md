# Object behavior modules `0x00680000–0x0068FFFF`

## Scope and ownership

This batch keeps 43 clean, unanalyzed ledger entries. Each counted VA is a
slot of a split vtable whose slot 2 is `mov eax, imm32; ret`, or a direct
callee of such a slot, or a direct callee of `0x0074AA60`, which pushes the
assertion string at `0x00C226A8`. Class names below were read from that slot 2
string. Slot numbers are the split-table index, and the dword at
`vtable + 4 * slot` is the counted VA.

| Vtable | Slot 2 string |
| --- | --- |
| `00BF64E0` | AreaRestrictSpecialPowerBehavior |
| `00BF7938` | SupplyCenterCreate |
| `00BF7A20` | IdleState |
| `00BF7F08` | DozerPrimaryIdleState |
| `00BF8CD8` | ProjectileUpdate |
| `00BF90F0` | StrafeAreaUpdate |
| `00BFAEC0` | EnergyDisablingBehavior |
| `00C00CF0` | MoveInStraightLineState |
| `00C00D58` | MoveToGroundState |
| `00C056A0` | RepairUpdate |
| `00C08080` | AutoAbilityBehavior |
| `00C08870` | DistributedMoneyBehavior |
| `00C08A48` | EncloseBehavior |
| `00C0C5A8` | SupplyWarehouseDockUpdate |
| `00C11420` | PlayerPowerManagerBehavior |
| `00C11840` | StancesBehavior |
| `00C13F50` | MissileUpdate |
| `00C142D8` | SpecialAbilityUpdate |
| `00C14738` | SlaveWatcherBehavior |
| `00C1AAB8` | BridgeBuilderBehavior |
| `00C1BA58` | BeamSpecialAbilityUpdate |
| `00C1C0E8` | DominateEnemySpecialPower |
| `00C1D2B8` | ReconstituteStateSpecialAbility |
| `00C1E6F0` | ToggleStatusSpecialAbilityUpdate |
| `00C1FB90` | OpenContain |
| `00C200A0` | TransportContain |
| `00C209E8` | LeechTargetingAttachUpdate |
| `00C21E90` | GarrisonContain |
| `00C22118` | AIUpdateInterface |
| `00C29FA8` | UnitAIDeliverToRefineryState |

`0x00C226A8` is
`C:\BF_QABuild_RA3_EALA-BUILD108\Ra3\code\SageEngine\source\GameEngine\gamelogic\object\Behavior\FireWeaponWhenDeadBehavior.cpp`.
`0x0074AA60` pushes it at `0x0074AB51` and calls `0x0068C520` at `0x0074AB6E`.

Five counted bodies are not themselves vtable slots. Their callers are:

| Callee | Caller | Anchor |
| --- | --- | --- |
| `00687460` | `006CA840` at `006CA87E` | ReconstituteStateSpecialAbility `00C1D2B8` slot 32 |
| `00689E80` | `00743940` at `00743B10` | StrafeAreaUpdate `00BF90F0` slot 23 |
| `0068B180` | `0071FDD0` at `0071FE18` | BridgeBuilderBehavior `00C1AAB8` slot 3 |
| `0068C520` | `0074AA60` at `0074AB6E` | `FireWeaponWhenDeadBehavior.cpp` at `00C226A8` |
| `0068D650` | `0079D030` at `0079D1B9` | UnitAIDeliverToRefineryState `00C29FA8` slot 5 |

The immediate at `0x00BD9628` is `0xBF800000` (`-1.0f`). The immediate at
`0x00BD90A4` is `0x3F800000` (`1.0f`). `0x00BF7000` is the string `StancesEnum`.

## Inventory

| VA | Size | Class and slot | Function |
| --- | --- | --- | --- |
| `00680260` | 25 | GarrisonContain `00C21E90` slot 10 | `GarrisonContain_clearReservedIfOccupied` |
| `00680690` | 10 | OpenContain `00C1FB90` slot 10 | `OpenContain_clearClosingByte` |
| `00680730` | 12 | TransportContain `00C200A0` slot 24 | `TransportContain_passengerFlag` |
| `00680B70` | 25 | IdleState `00BF7A20` slot 5 | `IdleState_busyMask` |
| `00680BB0` | 96 | MoveInStraightLineState `00C00CF0` slot 3 | `MoveInStraightLineState_xfer` |
| `00680C10` | 89 | MoveToGroundState `00C00D58` slot 3 | `MoveToGroundState_xfer` |
| `00680DC0` | 10 | BeamSpecialAbilityUpdate `00C1BA58` slot 41 | `BeamSpecialAbilityUpdate_templateFlag` |
| `00680E70` | 10 | DominateEnemySpecialPower `00C1C0E8` slot 33 | `DominateEnemySpecialPower_storeRadius80` |
| `00681670` | 26 | ToggleStatusSpecialAbilityUpdate `00C1E6F0` slot 30 | `ToggleStatusSpecialAbilityUpdate_entryPointer` |
| `00681690` | 39 | ToggleStatusSpecialAbilityUpdate `00C1E6F0` slot 31 | `ToggleStatusSpecialAbilityUpdate_nextEntryPointer` |
| `00681B50` | 81 | DozerPrimaryIdleState `00BF7F08` slot 3 | `DozerPrimaryIdleState_xfer` |
| `00682670` | 35 | SupplyWarehouseDockUpdate `00C0C5A8` slot 6 | `SupplyWarehouseDockUpdate_dispatch` |
| `00682C70` | 34 | AIUpdateInterface `00C22118` slot 6 | `AIUpdateInterface_ensureLocomotor` |
| `00682CF0` | 13 | AIUpdateInterface `00C22118` slot 121 | `AIUpdateInterface_setGoal` |
| `00682D00` | 13 | AIUpdateInterface `00C22118` slot 122 | `AIUpdateInterface_setGoalAux` |
| `00682DD0` | 28 | AIUpdateInterface `00C22118` slot 109 | `AIUpdateInterface_query34` |
| `00682EC0` | 63 | AIUpdateInterface `00C22118` slot 170 | `AIUpdateInterface_wantsAdvance` |
| `00682F70` | 22 | AIUpdateInterface `00C22118` slot 131 | `AIUpdateInterface_query24` |
| `00682FA0` | 22 | AIUpdateInterface `00C22118` slot 134 | `AIUpdateInterface_query2C` |
| `00682FC0` | 22 | AIUpdateInterface `00C22118` slot 135 | `AIUpdateInterface_query30` |
| `00683220` | 57 | AIUpdateInterface `00C22118` slot 105 | `AIUpdateInterface_enterState4D` |
| `006834E0` | 21 | AIUpdateInterface `00C22118` slot 124 | `AIUpdateInterface_forwardOrZero20` |
| `00683500` | 21 | AIUpdateInterface `00C22118` slot 125 | `AIUpdateInterface_forwardOrZero24` |
| `00683E20` | 39 | LeechTargetingAttachUpdate `00C209E8` slot 27 | `LeechTargetingAttachUpdate_notify` |
| `00683EE0` | 11 | MissileUpdate `00C13F50` slot 9 | `MissileUpdate_clearTimer` |
| `006844E0` | 127 | ProjectileUpdate `00BF8CD8` slot 23 | `ProjectileUpdate_addFrames` |
| `006846B0` | 33 | RepairUpdate `00C056A0` slot 9 | `RepairUpdate_notify` |
| `00684C90` | 32 | SpecialAbilityUpdate `00C142D8` slot 28 | `SpecialAbilityUpdate_countdown` |
| `00685940` | 18 | EncloseBehavior `00C08A48` slot 27 | `EncloseBehavior_relayIfArmed` |
| `00685960` | 77 | EncloseBehavior `00C08A48` slot 23 | `EncloseBehavior_tryOpen` |
| `00687460` | 65 | callee of `006CA840` | `findModuleByInterfaceKey` |
| `00689E80` | 111 | callee of `00743940` | `storeCoordAndSentinelSign` |
| `0068A140` | 71 | AreaRestrictSpecialPowerBehavior `00BF64E0` slot 3 | `AreaRestrictSpecialPowerBehavior_xfer` |
| `0068A680` | 47 | SupplyCenterCreate `00BF7938` slot 3 | `SupplyCenterCreate_xfer` |
| `0068A6B0` | 122 | AutoAbilityBehavior `00C08080` slot 3 | `AutoAbilityBehavior_xfer` |
| `0068B180` | 168 | callee of `0071FDD0` | `BridgeBuilderBehavior_countSpanSegments` |
| `0068BFA0` | 63 | DistributedMoneyBehavior `00C08870` slot 3 | `DistributedMoneyBehavior_xfer` |
| `0068C210` | 37 | EnergyDisablingBehavior `00BFAEC0` slot 6 | `EnergyDisablingBehavior_notifyOwner` |
| `0068C520` | 73 | callee of `0074AA60` | `copyPackedDamageBlock` |
| `0068D650` | 26 | callee of `0079D030` | `sumStride8Keys` |
| `0068DC20` | 103 | PlayerPowerManagerBehavior `00C11420` slot 3 | `PlayerPowerManagerBehavior_xfer` |
| `0068EB00` | 103 | SlaveWatcherBehavior `00C14738` slot 3 | `SlaveWatcherBehavior_xfer` |
| `0068F170` | 103 | StancesBehavior `00C11840` slot 3 | `StancesBehavior_xfer` |

Two of the 43 are boilerplate forwards. `0068A140` writes the version pair and
calls `0x008D82F0`. `0068A680` writes one version byte and calls `0x00680850`.
That is 2/43, under the 30% cap. The other xfers also transfer fields. There
are no slot-2 name getters and no static NameKey caches in the counted set.

## Layouts

- Contain flag clears: GarrisonContain writes 0 at `this+0x148` when
  `[this+4]+0x9E` is nonzero and returns that body pointer. OpenContain stores
  0 at `this+0x110`. TransportContain and BeamSpecialAbilityUpdate return the
  body pointer with AL replaced by the byte at `+0x143` and `+0x10D`.
- IdleState slot 5 loads `[[this+0x18]+0x80]+0x24`, calls slot 4, then
  `neg al; sbb eax, eax` (0 or `0xFFFFFFFF`).
- ToggleStatus entries are `table + index * 0x118 + 0x7C`, with the table at
  `[body+0xF4]` and the count at `[body+0xF0]`. Slot 31 adds 1 and wraps.
- AIUpdateInterface locomotor is `this+0x3C`. Slot 6 creates it through slot
  `0x2A4` and tail-calls the new object's slot `0x14`. Goals are dwords at
  `+0x480` and `+0x484`. Query slots 109/131/134/135 forward to `[loco+4]`
  when that pointer is nonzero; the empty path clears AL (slot 109) or sets
  AL to 1 (the other three). Slot 170 calls slot `0x274`, forces AL when the
  float at `+0x188` is greater than the `1.0f` at `0x00BD90A4` (`comiss` /
  `jbe`), and also when `[[[this+0x214]+4]+4]+0x64` is 2. Slot 105 calls
  locomotor slots `0x34` and `0x1C` with argument `0x4D`, stores the argument
  at `+0x50`, then `or eax, -1` and writes that to `+0x308`, `+0x368`, and
  `+0x3C8`.
- `00689E80` copies 12 bytes to `this+0x40`. All three components `ucomiss`
  equal to `-1.0f` and ordered stores `-1` at `+0x5C` and returns 1; every
  other pattern, including a NaN, stores `+1` and returns 0. Both paths are
  `sete` / `lea edx, [edx+edx-1]`.
- `006844E0` reads the clock object at `0x00DEBA64` and the seconds-per-frame
  float at `0x00DEEEE8`. The live path is `fld [clock+0x1C4]; fmul` global;
  `fmul` the stack argument; truncate with control word `or 0xC00`; add the
  low dword to `[clock+0x58]`; store the sum at `this+0x58`. The reconstruction
  takes the clock and the seconds-per-frame value as arguments so the checker
  and the emulator observe the same inputs. The fall-through when the global
  clock is null still executes `mov edx, [edx+0x58]`.
- `006846B0` calls slot `0x6C` on the player object at `0x00DF03FC` with
  `[this+0x30]`, then stores 1 there. A null player returns 0.
- `00684C90` returns 1 when `[this+0x3C]` is 0. Otherwise it decrements that
  dword, calls slot `0x9C`, and `sete al`.
- EncloseBehavior's `this` is the interface at `object+0x10`. Slot 27 returns
  0 when byte `+4` is 0, otherwise tail-calls slot `0x5C` on `this-0x10`.
  Slot 23 calls its own slot 8; on accept it calls slots `0x58`, `0x60`, and
  `0x5C` on the base, then slot `0x28` with 1, and forces AL to 1.
- `00687460` walks the pointer list at `[this+0x310]` until a null slot,
  comparing each module's slot `0x10` with the key.
- `0068B180` is `sqrt(dx*dx+dy*dy) - 2*width` in x87 (`fld` Y, square, `fld`
  X, square, `faddp`, `fsqrt`, `fld` width, double, `fsubp`). It clears
  `+0x34` and `+0x38`, then counts steps of `[info+0x18]` or `[info+0x20]`
  while the remainder stays non-negative. The step alternates when both
  `[info+0x1C]` and `[info+0x20]` are nonzero.
- `0068C210` follows `this+8`, `+0x418`, `+0x74` and calls `0x007F8BC0` with
  `[object+0xDC]`. Any null link returns.
- `0068C520` copies `0x2C` bytes. Offsets `+0x10`, `+0x14`, and `+0x18` go
  through `fld` / `fstp`. EAX stays the destination.
- `0068D650` sums dwords from `[this+0x24]` up to `[this+0x28]`, stride 8.
- `0068F170` pushes `StancesEnum` (`0x00BF7000`), `this+0x38`, and count 4
  into xfer slot `0x88` after the base and `+0x10` forwards.
- Dock dispatch `00682670` calls `0x0052DDE0` with `[body+0x1C]` and
  `[this+0x8C]` when `[update+0xF4]` is non-null.

## Oracle and mutation

`ra3_behaviormodules68check` (Release) and
`Ra3Source/tools/analysis/ports/verify_objbehav68.py` compare EAX, the stub
log, and the touched dwords with Unicorn on the shipped bytes. Every counted
function has at least one case; xfers, the coordinate sentinel (including
NaN), the projectile truncation, and the bridge remainder have branch cases.

Release result after restoring the mutation:

`cases=90 mismatches=0`

Mutation: `storeCoordAndSentinelSign` stored `+1` on the all-sentinel path
and `-1` on the other paths (`+0x5C`). The oracle reported
`cases=90 mismatches=5`, all five `coord` cases, with `5c:00000001` against
the shipped `5c:ffffffff` for `(-1,-1,-1)` and the opposite pair for the
other four inputs. The sign was restored, the checker rebuilt, and the oracle
returned to 0 mismatches.

## Hooks

No hook keys.

| Key | VA | Expected bytes | Stolen | explicitOnly |
| --- | --- | --- | --- | --- |
| (none) | | | | |

## Deferred

The snapshot drafted about 70 bodies in `0x00687460–0x0068FE60`. Six of those
are counted above (`00687460`, `00689E80`, `0068B180`, `0068C210`, `0068C520`,
`0068D650`). The rest were not counted.

Static NameKey caches. Each begins `mov eax, 1`, calls `0x00986150` with a
class-name immediate, and then calls the counted finder `0x00687460`:

- `0068A190` pushes `AssistedPlacementBehavior` (`0x00BF9F5C`). Direct callee
  of UnitAIDockCanGoDirect `00C2A018` slot 4 at `007C7C86`.
- `0068A320` pushes `AssistedPlacementInfoBehavior` (`0x00BFA150`). Same
  caller at `007C7C8E`.
- `0068B230` pushes `BuildingBehavior` (`0x00BFA654`).
- `0068C5E0` pushes `FXListBehavior` (`0x00BFB0DC`).
- `0068F520` pushes `StructurePlacementBehavior` (`0x00BFCDF4`).

Provable, but not matched in this batch:

- `0068E5F0` (218 bytes) is an unrolled `addss` weight sum and `fld` of the
  total. Direct callee of RestrictSpecialPowerBehavior `00BF6EF8` slot 1 at
  `006AE840`.
- `0068FE10` (77 bytes) scans the `+0x28` grid for a dword 8 with a nonzero
  following dword. Direct callee of SupplyTruckWantsToPickUpOrDeliverBoxesState
  `00BF8348` slot 5 at `006E4BA4`. The early outs leave the incoming EAX high
  bits in place.
- `0068FD40` (113 bytes) is called from `0x00721A20` at `0x00721B64`. That
  caller pushes `TiberiumFieldBehavior.cpp` at `0x00BFD4C8`.

`0068A0A0` adjusts `ecx-0xC` to `ecx+4` or returns 0. `0068C6D0` adjusts the
same base to `ecx+0x10` or returns 0. Neither is a named vtable slot and
neither has a direct `E8`/`E9` in `.text`.

The remaining snapshot addresses are clean and unanalyzed, are not slots of a
split vtable whose slot 2 returns a class name, and were not reconstructed.
A `.text` scan of `E8`/`E9` sites that land on an instruction boundary found
no named-vtable caller for them:

`00689FC0 0068A080 0068A270 0068A730 0068A810 0068A890 0068A8B0 0068AC10
0068ADF0 0068AEF0 0068AF80 0068AFB0 0068AFD0 0068B360 0068B840 0068B960
0068C7A0 0068CEF0 0068CF20 0068D3B0 0068D790 0068D860 0068D890 0068D8C0
0068DA00 0068DA20 0068DBF0 0068DC00 0068DC90 0068DCA0 0068DCD0 0068DE80
0068E410 0068E440 0068E550 0068E820 0068E880 0068EA70 0068EC50 0068EF90
0068F050 0068F0F0 0068F1E0 0068F200 0068F240 0068F2C0 0068F420 0068F790
0068F7D0 0068F810 0068FC00 0068FCE0 0068FDC0 0068FE60`
