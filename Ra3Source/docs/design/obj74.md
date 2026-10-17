# Object modules `0x00740000–0x0077FFFF`

## Scope and ownership

This batch keeps 46 clean, unanalyzed ledger entries in the contain, dock, and
special-ability families. Every counted VA is a slot of a split vtable. Slot 2
of that vtable is `mov eax, imm32; ret`. The class name is the C string at
that immediate. The dword at `vtable + 4 * slot` is the counted VA.
`work/obj74_claims.txt` lists the 46 `vt` lines; `verify_ownership.py` reports
`fails=0`.

| Vtable | Slot 2 string |
| --- | --- |
| `00BF8CD8` | ProjectileUpdate |
| `00C00D58` | MoveToGroundState |
| `00C0BF70` | RailedTransportDockUpdate |
| `00C0C190` | RepairDockUpdate |
| `00C0C3A8` | SupplyCenterDockUpdate |
| `00C0C5A8` | SupplyWarehouseDockUpdate |
| `00C0C920` | AutoDepositUpdate |
| `00C0CE18` | ClusterBombUpdate |
| `00C0E5D0` | WeaponScatterEffectUpdate |
| `00C108B0` | SubStructureBuildUpgrade |
| `00C10AA0` | TechUpgrade |
| `00C11420` | PlayerPowerManagerBehavior |
| `00C11688` | SpawnBehavior |
| `00C13A00` | DamageFieldUpdate |
| `00C13F50` | MissileUpdate |
| `00C1AE58` | LaserStateModule |
| `00C1C798` | FlingPassengerSpecialAbilityUpdate |
| `00C1C9D8` | GiveOrRestoreUpgradeSpecialPower |
| `00C1D960` | SwitchLocomotorsSpecialAbilityUpdate |
| `00C1E010` | ToggleDeploySpecialAbilityUpdate |
| `00C1E258` | ToggleHiddenSpecialAbilityUpdate |
| `00C1E498` | ToggleMountedSpecialAbilityUpdate |
| `00C1E6F0` | ToggleStatusSpecialAbilityUpdate |
| `00C1EBC8` | ObjectStatusSpecialAbilityUpdate |
| `00C1EE08` | OCLMonitorSpecialAbilityUpdate |
| `00C1F0A8` | ReplaceObjectUpdate |
| `00C200A0` | TransportContain |
| `00C202F0` | DelayedWeaponFireSpecialAbilityUpdate |
| `00C20D90` | StealUnitUpdate |
| `00C22118` | AIUpdateInterface |
| `00C22E28` | SlaughterHordeContain |
| `00C23218` | GarrisonStealthContain |
| `00C235F0` | HealContain |
| `00C23A98` | TunnelContain |
| `00C24A50` | DozerAIUpdate |
| `00C25398` | SupplyTruckAIUpdate |
| `00C257C0` | TransportAIUpdate |
| `00C26980` | HordeGarrisonContain |

Immediates read from the image:

- `0x00C2240C` is the C string `ClusterBombUpdateInitialWeapon`. ClusterBomb
  pushes it before the armed-weapon branch, so both the slot-4 arm and the
  slot-`0x0C` arm receive it.
- `0x00BD90A4` is the dword `0x3F800000`. ToggleMounted stores it with `movss`
  at rider `+0x1BC`.
- `0x00CC0D58` is eight dwords, `0x155 0x156 0x157 0x15B 0x158 0x159 0x15A 0x15C`.
  WeaponScatter indexes it. The oracle log for index 1 is `c4d9ab0:156` and
  for index 7 is `c4d9af0:15c`.
- `0x00BBA500` is an `.rdata` dword `0x78145320`. AutoDeposit at `0x00742099`
  and ClusterBomb at `0x007422F3` call through it, then `add esp, 0xc`. The
  byte count is `sub reg, reg`, so it is always 0, and the end pointer is
  folded back onto begin.
- `0x00DEBA64` is the game-logic object. Missile, Laser, and Projectile read
  the frame at `+0x58`.

## Inventory

| VA | Size | Class and slot | Function |
| --- | --- | --- | --- |
| `00740170` | 353 | SupplyTruckAIUpdate `00C25398` slot 3 | `SupplyTruckAIUpdate_xfer` |
| `007402E0` | 186 | TransportAIUpdate `00C257C0` slot 45 | `TransportAIUpdate_notifyPassengers45` |
| `007403A0` | 186 | TransportAIUpdate `00C257C0` slot 49 | `TransportAIUpdate_notifyPassengers49` |
| `00740460` | 186 | TransportAIUpdate `00C257C0` slot 52 | `TransportAIUpdate_notifyPassengers52` |
| `00740990` | 127 | RailedTransportDockUpdate `00C0BF70` slot 3 | `RailedTransportDockUpdate_xfer` |
| `00740A60` | 85 | RepairDockUpdate `00C0C190` slot 3 | `RepairDockUpdate_xfer` |
| `00740B00` | 47 | SupplyCenterDockUpdate `00C0C3A8` slot 3 | `SupplyCenterDockUpdate_xfer` |
| `00740B80` | 63 | SupplyWarehouseDockUpdate `00C0C5A8` slot 3 | `SupplyWarehouseDockUpdate_xfer` |
| `00740F60` | 24 | AIUpdateInterface `00C22118` slot 138 | `AIUpdateInterface_storeGoalPosition` |
| `00742030` | 194 | AutoDepositUpdate `00C0C920` slot 3 | `AutoDepositUpdate_xfer` |
| `00742220` | 257 | ClusterBombUpdate `00C0CE18` slot 3 | `ClusterBombUpdate_xfer` |
| `00742370` | 60 | DamageFieldUpdate `00C13A00` slot 3 | `DamageFieldUpdate_xfer` |
| `007428E0` | 87 | MissileUpdate `00C13F50` slot 22 | `MissileUpdate_arm` |
| `00742DC0` | 62 | ObjectStatusSpecialAbilityUpdate `00C1EBC8` slot 3 | `ObjectStatusSpecialAbilityUpdate_xfer` |
| `00742E00` | 134 | OCLMonitorSpecialAbilityUpdate `00C1EE08` slot 3 | `OCLMonitorSpecialAbilityUpdate_xfer` |
| `00743140` | 49 | ReplaceObjectUpdate `00C1F0A8` slot 3 | `ReplaceObjectUpdate_xfer` |
| `007437E0` | 101 | StealUnitUpdate `00C20D90` slot 24 | `StealUnitUpdate_latch` |
| `007446B0` | 125 | WeaponScatterEffectUpdate `00C0E5D0` slot 31 | `WeaponScatterEffectUpdate_apply` |
| `007450B0` | 163 | SubStructureBuildUpgrade `00C108B0` slot 3 | `SubStructureBuildUpgrade_xfer` |
| `0074BE70` | 55 | PlayerPowerManagerBehavior `00C11420` slot 1 | `PlayerPowerManagerBehavior_refresh` |
| `0074C870` | 31 | SpawnBehavior `00C11688` slot 10 | `SpawnBehavior_prime` |
| `0074FB70` | 98 | TransportContain `00C200A0` slot 3 | `TransportContain_xfer` |
| `00751280` | 63 | DelayedWeaponFireSpecialAbilityUpdate `00C202F0` slot 3 | `DelayedWeaponFireSpecialAbilityUpdate_xfer` |
| `00751A30` | 76 | ToggleMountedSpecialAbilityUpdate `00C1E498` slot 29 | `ToggleMountedSpecialAbilityUpdate_clear` |
| `00751E60` | 55 | DozerAIUpdate `00C24A50` slot 166 | `DozerAIUpdate_routeBuild` |
| `00755190` | 85 | StealUnitUpdate `00C20D90` slot 23 | `StealUnitUpdate_relay` |
| `00764BF0` | 147 | ProjectileUpdate `00BF8CD8` slot 22 | `ProjectileUpdate_launch` |
| `007671B0` | 37 | TechUpgrade `00C10AA0` slot 9 | `TechUpgrade_finish` |
| `007671E0` | 87 | TechUpgrade `00C10AA0` slot 10 | `TechUpgrade_grant` |
| `0076CFE0` | 104 | LaserStateModule `00C1AE58` slot 22 | `LaserStateModule_schedule` |
| `0076FE20` | 65 | HealContain `00C235F0` slot 44 | `HealContain_receivePassenger` |
| `0076FF30` | 87 | HordeGarrisonContain `00C26980` slot 3 | `HordeGarrisonContain_xfer` |
| `0076FF90` | 71 | SlaughterHordeContain `00C22E28` slot 44 | `SlaughterHordeContain_receivePassenger` |
| `007701E0` | 47 | GarrisonStealthContain `00C23218` slot 3 | `GarrisonStealthContain_xfer` |
| `00770660` | 63 | HealContain `00C235F0` slot 3 | `HealContain_xfer` |
| `00770E20` | 107 | TunnelContain `00C23A98` slot 3 | `TunnelContain_xfer` |
| `00772390` | 157 | FlingPassengerSpecialAbilityUpdate `00C1C798` slot 27 | `FlingPassengerSpecialAbilityUpdate_fling` |
| `00772430` | 72 | GiveOrRestoreUpgradeSpecialPower `00C1C9D8` slot 27 | `GiveOrRestoreUpgradeSpecialPower_apply` |
| `00774BB0` | 118 | SwitchLocomotorsSpecialAbilityUpdate `00C1D960` slot 36 | `SwitchLocomotorsSpecialAbilityUpdate_commit` |
| `00775CB0` | 56 | ToggleDeploySpecialAbilityUpdate `00C1E010` slot 27 | `ToggleDeploySpecialAbilityUpdate_flip` |
| `00775CF0` | 54 | ToggleHiddenSpecialAbilityUpdate `00C1E258` slot 27 | `ToggleHiddenSpecialAbilityUpdate_flip` |
| `00776280` | 25 | ToggleStatusSpecialAbilityUpdate `00C1E6F0` slot 43 | `ToggleStatusSpecialAbilityUpdate_rewind` |
| `007784C0` | 98 | ObjectStatusSpecialAbilityUpdate `00C1EBC8` slot 27 | `ObjectStatusSpecialAbilityUpdate_swap` |
| `0077C910` | 50 | HealContain `00C235F0` slot 45 | `HealContain_removePassenger` |
| `0077C950` | 50 | SlaughterHordeContain `00C22E28` slot 46 | `SlaughterHordeContain_removePassenger` |
| `0077CE90` | 89 | MoveToGroundState `00C00D58` slot 5 | `MoveToGroundState_update` |

Two of the 46 are boilerplate: a version probe (`{1,1}` through slot `0x18`)
and one base xfer. That is `GarrisonStealthContain_xfer` and
`SupplyCenterDockUpdate_xfer`, 2/46. The other xfers store fields, walk a
vector, or branch on a flag bit.

## Behaviour the oracle locks

Heal (`0x00770660`) and Tunnel (`0x00770E20`) call Horde (`0x0076FF30`)
directly. The oracle leaves that body live and stubs everything else those
three call.

`0x0076FE20` loads the template pointer into EAX before testing flag bit 10
(`shr ecx, 0xA`). A clear bit returns that pointer. A set bit calls
`0x0078D1E0` and returns that call. `0x0076FF90` is the same body with
`0x00691AB0` first. Heal remove (`0x0077C910`) tests AL; Slaughter remove
(`0x0077C950`) tests EAX, and its slot `0x78` runs before `0x007704B0`.

Repair, Replace, and Tunnel set AL to 1 and return that EAX when xfer flag
bit 3 is set. SupplyTruck shifts the flags by 3 and returns the shifted
value. AutoDeposit reloads the flags into EAX and returns them on that bit.

ToggleMounted calls `0x007432E0` (not `0x007653F0`), then `movss` from
`0x00BD90A4`, then `and dword [object+0x88], 0xFFFFDFFF`. `and al, 1` keeps
the high bits, so a lone bit 21 returns `0x100`.

Fling's null-alternate path returns the unit pointer left by slot `0x128`.
Scatter's `id == -1` path returns `0xFFFFFFFF`. Spawn's zero template byte
returns the template pointer. Give's zero upgrade id returns the template
pointer. Deploy's state other than 1 returns the object pointer loaded before
the `jne`.

Transport notify (`0x007402E0`, `0x007403A0`, `0x00740460`) shares one walk.
Contain is object `+0x328`, or the first vector element at `+0x32C` when the
range is non-empty. Slot `0xB0` must return a nonzero low byte and the third
argument must be 0 or 1. Slot `0x110` fills an 8-byte out struct. The list is
circular. A null module is skipped after the node advances. A nonzero
`out.first` is a 1-arg slot-0 call.

## Oracle

`Ra3Source/tools/analysis/ports/verify_obj74.py` runs the Release checker
`ra3_obj74check` against Unicorn on the shipped bytes. 141 cases, several per
counted function, including the flag, null, and empty-list edges.

`cases=141 mismatches=0`

Mutation: `HealContain_receivePassenger` tested template flag bit 11 instead
of bit 10. The oracle reported `cases=141 mismatches=3`: `hadd 0x400`,
`hadd 0x800`, and `sadd 0x400`. On `hadd 0x400` the port log stopped before
`c78d1e0` and the shipped log included it. The shift was restored, the
checker rebuilt, and the oracle returned to 0 mismatches.

## Hooks

No hook keys.

| Key | VA | Expected bytes | Stolen | explicitOnly |
| --- | --- | --- | --- | --- |
| (none) | | | | |

## Dropped

Deleting destructors. Each calls `0x004166B0` after writing a vtable or
testing the delete flag:

- `0074D590` (30), `0074E470` (30), `0075C8B0` (30)
- `0075CCF0`, `0075CE00`, `0075D9D0`, `0075E990` (99)
- AIUpdate slot-0 bodies `0075FB90`, `0075FBF0`, `0075FC50`, `0075FCD0` (88 or
  91) and `0075FD50`, `0075FDA0` (78). `0075FB90` writes `0x00C23D58` at
  `0075FB93` and calls `0x004166B0` at `0075FBDA`.

`00740690` is 28 bytes and `jmp 0x4163B1` at `007406A7`. Dock xfers call it;
the oracle stubs it. It is not counted.

Shared bodies left as stubs, not counted:

- `0074F0E0` (1483), `0075D630` (498, calls `0074F0E0`), `0075C0A0` (1522,
  calls `0074FB70`), `0075E110` (1753, `sub esp, 0x724`), `0074D620` (966),
  `0074FA20` (325), `007432E0` (1038, mounted's prepare), `007653F0` (603),
  `0077CE10` (115, ground-near, stubbed for `0077CE90`).

x87 bodies, not counted: `00743850` (228, `fld`/`fsub` at `0074385A`),
`00753740` (381, `fld` at `00753797`), `00771E60` (287, `fld` at `00771EB1`).

Object-id lookup loops that call `0x00A21BE0`, `0x0050B5F0`, and `0x00A21C90`,
not counted: `007512C0` (196), `00755DA0` (220), `0075ACD0` (191),
`0075FDF0` (147), `00761010` (216), `007670D0` (214). `0075A780` (190) and
its caller `0075B670` (53, call at `0075B67F`) are the same family without
that trio.

Also left out, clean and larger than this batch's field methods:
`00744FE0` (199) and `00756410` (101).
