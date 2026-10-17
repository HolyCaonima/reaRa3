# Object modules `0x00720000–0x0073FFFF`

## Scope and ownership

This batch keeps 41 clean ledger entries whose starts lie in `0x00720000–0x0073FFFF`. Every counted body is one slot of a split vtable. The class name is the C string returned by slot 2 (`mov eax, imm32; ret`). A body that also sits in a secondary table is counted once, under the class cited below. Slot numbers are decimal. `work/obj72_claims.txt` records one `vt` line per function; `verify_ownership.py` reports `fails=0`.

| Vtable | Slot 2 string | Counted slots |
| --- | --- | --- |
| `00C22118` | AIUpdateInterface | 22, 25, 29–32, 34, 37–40, 46, 49, 50, 54, 55, 86, 100 |
| `00C11840` | StancesBehavior | 22, 29 |
| `00C1E918` | WeaponFireSpecialAbilityUpdate | 23, 26 |
| `00C1E498` | ToggleMountedSpecialAbilityUpdate | 36 |
| `00C1BA58` | BeamSpecialAbilityUpdate | 36 |
| `00C1B5D8` | ActivateModuleSpecialAbilityUpdate | 36 |
| `00C1CC18` | GiveUpgradeUpdate | 3, 36 |
| `00C1EE08` | OCLMonitorSpecialAbilityUpdate | 36 |
| `00C05CA4` | ShieldSphereManager | 3 |
| `00C1D078` | RadarScanMapSpecialAbilityUpdate | 3 |
| `00C1BC90` | CaptureSpecialAbilityUpdate | 3 |
| `00C0AE88` | CompositeStructureInfoModule | 3 |
| `00C12ED0` | StoreObjectsSpecialPower | 3 |
| `00C245B0` | DeployStyleAIUpdate | 3 |
| `00C24EE0` | HordeAIUpdate | 3 |
| `00C12238` | ManagedActiveBody | 3 |
| `00C0DBC8` | SpawnCrateUpdate | 3 |
| `00C13840` | BroadcastStealthUpdate | 3 |
| `00C09CD8` | StrafeAreaTargetChooser | 3 |
| `00C09EE0` | SubStructureStorageBehavior | 3 |
| `00BF69B8` | FireWeaponWhenDamagedBehavior | 25 |

`WeaponFireSpecialAbilityUpdate` slots 23 and 26 are the same bytes as the matching slots of DelayedWeaponFire (`00C202F0`). `StancesBehavior` slots 22 and 29 are also installed on secondary tables. `HordeAIUpdate_xfer` is shared by more than one AI table. Each of those bodies is counted once.

The dword at `0x00BE700C` is `0x7F7FFFFF`. The move-to-constant commands load it with `fld` and pass it to the steer helper. The dword at `0x00BD90A4` is `0x3F800000` (`1.0f`); ToggleMounted stores it at `mounted+0x1BC`.

There are no new hooks.

## Inventory

| VA | Size | Slot | Function |
| --- | --- | --- | --- |
| `007216e0` | 210 | StrafeAreaTargetChooser 3 | `StrafeAreaTargetChooser_xfer` |
| `00721820` | 213 | SubStructureStorageBehavior 3 | `SubStructureStorageBehavior_xfer` |
| `00722660` | 92 | StancesBehavior 22 | `StancesBehavior_addStanceId` |
| `007226c0` | 132 | StancesBehavior 29 | `StancesBehavior_linkStance` |
| `00722970` | 63 | ManagedActiveBody 3 | `ManagedActiveBody_xfer` |
| `007284c0` | 52 | CompositeStructureInfoModule 3 | `CompositeStructureInfoModule_xfer` |
| `007287e0` | 33 | ActivateModuleSpecialAbilityUpdate 36 | `ActivateModuleSpecialAbilityUpdate_xferAndActivate` |
| `007289d0` | 116 | BeamSpecialAbilityUpdate 36 | `BeamSpecialAbilityUpdate_resetBeam` |
| `007295c0` | 29 | GiveUpgradeUpdate 36 | `GiveUpgradeUpdate_clearThenXfer` |
| `0072a580` | 106 | StoreObjectsSpecialPower 3 | `StoreObjectsSpecialPower_xfer` |
| `0072a8d0` | 87 | ToggleMountedSpecialAbilityUpdate 36 | `ToggleMountedSpecialAbilityUpdate_clearMountedBit` |
| `0072cfd0` | 182 | AIUpdateInterface 22 | `AIUpdateInterface_moveWithVector` |
| `0072d1a0` | 179 | AIUpdateInterface 25 | `AIUpdateInterface_moveToObject` |
| `0072d260` | 173 | AIUpdateInterface 29 | `AIUpdateInterface_moveClear1C` |
| `0072d310` | 173 | AIUpdateInterface 30 | `AIUpdateInterface_moveClear1D` |
| `0072d3c0` | 148 | AIUpdateInterface 31 | `AIUpdateInterface_moveState1E` |
| `0072d460` | 148 | AIUpdateInterface 32 | `AIUpdateInterface_moveState35` |
| `0072d500` | 140 | AIUpdateInterface 34 | `AIUpdateInterface_faceObject` |
| `0072d590` | 135 | AIUpdateInterface 100 | `AIUpdateInterface_command2F` |
| `0072d620` | 165 | AIUpdateInterface 37 | `AIUpdateInterface_aimState3` |
| `0072d6d0` | 97 | AIUpdateInterface 39 | `AIUpdateInterface_aimState5` |
| `0072d740` | 85 | AIUpdateInterface 38 | `AIUpdateInterface_aimState2` |
| `0072d7a0` | 85 | AIUpdateInterface 40 | `AIUpdateInterface_aimState4` |
| `0072ddb0` | 139 | AIUpdateInterface 46 | `AIUpdateInterface_order2C` |
| `0072de40` | 125 | AIUpdateInterface 49 | `AIUpdateInterface_order0B` |
| `0072dec0` | 129 | AIUpdateInterface 50 | `AIUpdateInterface_order16` |
| `0072e560` | 158 | AIUpdateInterface 54 | `AIUpdateInterface_aimFlagged` |
| `0072e600` | 117 | AIUpdateInterface 86 | `AIUpdateInterface_guardPoint` |
| `0072e680` | 146 | AIUpdateInterface 55 | `AIUpdateInterface_attach60` |
| `0072fa80` | 266 | BroadcastStealthUpdate 3 | `BroadcastStealthUpdate_xfer` |
| `00730d90` | 29 | OCLMonitorSpecialAbilityUpdate 36 | `OCLMonitorSpecialAbilityUpdate_stopThenXfer` |
| `00731900` | 79 | ShieldSphereManager 3 | `ShieldSphereManager_xfer` |
| `00731b70` | 175 | SpawnCrateUpdate 3 | `SpawnCrateUpdate_xfer` |
| `0073b200` | 161 | FireWeaponWhenDamagedBehavior 25 | `FireWeaponWhenDamagedBehavior_onDamage` |
| `0073e780` | 63 | CaptureSpecialAbilityUpdate 3 | `CaptureSpecialAbilityUpdate_xfer` |
| `0073ed30` | 107 | GiveUpgradeUpdate 3 | `GiveUpgradeUpdate_xfer` |
| `0073ee80` | 72 | RadarScanMapSpecialAbilityUpdate 3 | `RadarScanMapSpecialAbilityUpdate_xfer` |
| `0073f590` | 39 | WeaponFireSpecialAbilityUpdate 26 | `WeaponFireSpecialAbilityUpdate_armAndTrigger` |
| `0073f5c0` | 25 | WeaponFireSpecialAbilityUpdate 23 | `WeaponFireSpecialAbilityUpdate_templateBit8` |
| `0073faa0` | 65 | DeployStyleAIUpdate 3 | `DeployStyleAIUpdate_xfer` |
| `0073fff0` | 47 | HordeAIUpdate 3 | `HordeAIUpdate_xfer` |

## What the bodies do

AIUpdateInterface commands share one skeleton. `usable` (`0x006C0ED0`) is tested on `al`; a zero low byte returns that EAX. Mode 0 and mode 1 then call a follow helper. The move commands copy the object position through the pointer at `self+0x214` (not the locomotor at `self+0x3C`), call locomotor slot `0x34`, steer or place, clear `self+0x190` and byte `self+0x432`, store the mode at `self+0x50`, and call locomotor slot `0x1C` with a state id.

- `moveWithVector` (`0072cfd0`) steers with the caller's float bits. A non-null vector is copied to locomotor `+0x48` and byte `+0x44` is set. State id is 1. Follow argument is the position.
- `moveToObject` (`0072d1a0`) returns immediately when byte `self+0x436` is set (incoming EAX is 0 in the harness). It calls locomotor slot `0x38` with the target and places at `target+0x38` using `0x7F7FFFFF`. State id is 1.
- `moveClear1C` / `moveClear1D` (`0072d260`, `0072d310`) steer with `0x7F7FFFFF`, use state `0x1C` or `0x1D`, then `or eax, -1` and store that in `self+0x308`, `+0x368`, and `+0x3C8`.
- `moveState1E` / `moveState35` (`0072d3c0`, `0072d460`) are the same steer skeleton with state `0x1E` or `0x35` and no forced EAX.
- `faceObject` (`0072d500`) faces `target` and follows `object+0x38`. State id is `0x37`.
- `command2F` (`0072d590`) also bails on byte `0x436`. A non-zero `currentTarget` (`0x006EFE40`) is returned as-is. Otherwise it queries the locomotor (`0x005B9F30`), and when the mode is not 2 marks the object with that query result. State id is `0x2F`. Follow argument is `target+0x38`.
- `aimState5` / `aimState2` / `aimState4` (`0072d6d0`, `0072d740`, `0072d7a0`) aim at the target. State ids are 5, 2, and 4. Only state 5 zeroes `object+0x458`. The `push 2` at `0x0072D777` is `kAimState2`.
- `aimState3` (`0072d620`) also zeroes the timer. If template `+0xD4` bit 21 is set, a non-null chain at `object+0x328` (or the first dword of the vector at `+0x32C` when the chain is null) gates a walk from the target along `+0x20`. The walk result is discarded. State id is 3.
- `order2C` / `order0B` (`0072ddb0`, `0072de40`) return 0 for a null target. They write weapon aux 1, or the caller's aux, through `object+0x37C`. A null weapon set leaves EAX equal to the object, because the search begins with `mov eax, [esi+8]`. A failed search leaves EAX 0. Mode 0/1 then replaces EAX with the follow call. State ids are `0x2C` and `0x0B`. `order2C` marks the target when the mode is not 2.
- `order16` (`0072dec0`) prepares a key on the locomotor, writes the aux, then returns the locomotor query, or the follow of that query when the query is non-zero and the mode is 0 or 1. State id is `0x16`.
- `aimFlagged` (`0072e560`) bails on byte `0x436`. The state id is `0x22` plus 1 when the flag byte is non-zero. Follow uses `0x0072CE90` and `target+0xC`.
- `guardPoint` (`0072e600`) returns the template pointer when template `+0xC0` bit 26 is set. Otherwise it stores the point at `self+0x74`, state `0x1F`, and for mode 0/1 reads the point into a 12-byte local (`0x0085CEE0`) and follows that local (`0x0072CDF0`).
- `attach60` (`0072e680`) resolves a partner from the drawable at `object+0x374` (`0x007D2120`) or the weapon set at `object+0x37C` (`0x006A18A0`), unless `object+0x471` bit 0 is set. It binds `self+0x3C8` with `self+0x368`, notes the argument, and sets state `0x60`.

StancesBehavior is called with `this` eight bytes above the interface base, so the object is `[this-8]`.

- `addStanceId` (`00722660`) returns the object when the id equals `object+0xDC`. Otherwise it scans the dword vector at `this+0xE0`. A hit returns that slot. A full vector calls `0x008ADD50`. An empty end pointer stores `end+4` and returns 0. Otherwise it appends and returns the old end.
- `linkStance` (`007226c0`) returns the existing node when the id is already in the list at `this+0xD4`. Otherwise it allocates a 12-byte node (`0x004D99E0`), links it, and ORs `object+0x94` with `0x4000`. `shr eax, 14; not al; and al, 1` decides whether to notify `0xAE`.

WeaponFire:

- `templateBit8` (`0073f5c0`) returns `(object & ~0xFF) | 1` when `object+0xBC` bit 8 is set. Otherwise it jumps to `0x00731C20`.
- `armAndTrigger` (`0073f590`) applies template `+0x104` when it is not `-1`, then triggers the ability with 1. The slot pops one unused argument.

Other slot-36 updates:

- ToggleMounted (`0072a8d0`) writes `1.0f` through `object+0xF4` when that pointer is set, clears bit `0x2000` of `object+0x88`, notifies `0x4D` when that bit was set, then forwards to `0x00718B80`.
- Beam (`007289d0`) forwards, pulses, and returns 0 when byte `self+0xFA` is 0. Otherwise it zeroes the beam fields and applies the drawable's slot `0x288` result.
- Activate (`007287e0`), GiveUpgrade clear (`007295c0`), and OCL monitor (`00730d90`) are a base transfer wrapped around one helper. They are the boilerplate rows.

Xfer slots:

- Shield (`00731900`) versions `{1,2}` and transfers three subobjects; the third runs only when the version byte stays at least 2.
- Radar (`0073ee80`) transfers the ability, versions `{1,1}`, then two cdecl field writers (`0x0071D020`, `0x006F3160`). The `add esp, 0x10` cleans both.
- Capture (`0073e780`) versions, transfers the ability, then xfer slot `0x64` at `self+0xB8`.
- Composite (`007284c0`) versions and writes `self+0x18`.
- StoreObjects (`0072a580`) versions `{1,2}`. The pair at `self+0x70` runs only when the version byte stays at least 2.
- GiveUpgrade xfer (`0073ed30`) and DeployStyle (`0073faa0`) return `(previousEAX & ~0xFF) | 1` when bit 3 of `xfer+4` is set. Otherwise they version and transfer their fields.
- Horde (`0073fff0`) versions `{1,1}` and forwards to the AI xfer. The ledger entry is 47 bytes starting at `0073fff0`; `ret 4` is at `0074001C`. The entry is in this range. This forward is boilerplate.
- ManagedActiveBody (`00722970`) versions, transfers the body, and writes `self+0x124`.
- SpawnCrate (`00731b70`) versions twice, then walks an 8-byte vector. After the count transfer, EAX becomes `xfer+4 >> 1` unless bit 1 requests a resize. An empty vector leaves that value; a non-empty vector leaves the last element transfer.
- BroadcastStealth (`0072fa80`) returns `xfer+4 >> 3` when bit 3 is set. Bit 0 saves the circular list at `self+0x2C`. The other path loads a count and allocates nodes.
- Strafe (`007216e0`) and SubStructure (`00721820`) version twice, transfer a vector (the storage count is the low byte of an arithmetic `>> 3`), and finish with slot `0x84`.

`FireWeaponWhenDamagedBehavior_onDamage` (`0073b200`) calls slot 0 of the embedded object at `self+0x10` and returns that EAX when `al` is 0. The damage type is `1 << (damage+0x1C & 31)`. A mask miss or `comiss` above returns the template pointer, which is what EAX holds after the mask is loaded. Otherwise the selector at `object+0x33C` slot `0x54` picks a module (`3` at `self+0x38`, `1` at `+0x30`, `2` at `+0x34`, anything else at `+0x2C`). A null module returns the kind. A busy weapon returns the idle call. Otherwise `0x00739E60` fires at `object+0x38` and pops 8 bytes.

## Boilerplate

Four of the 41 counted functions only forward:

| VA | Body |
| --- | --- |
| `007287e0` | base xfer, then activate |
| `007295c0` | clear, then base xfer |
| `00730d90` | stop, then base xfer |
| `0073fff0` | version bytes, then the AI xfer |

That is 4/41, under the 30% cap. No two counted functions are the same shape with different immediates. The four steer commands differ by state id, and two of them also force EAX to `-1` and clear three fields. Same-shape factories counted: 0.

## Dropped

- `00722bd0` is only the EASTL dword-vector append (`call 0x008ADD50` on the full path). EASTL growth is not counted. `addStanceId` calls that helper and is counted as the stance method.
- `0073f5e0` returns `fldz` or leaves an x87 result. It is outside the integer/SSE oracle.
- `00723370` tail-jumps `0x0070F2E0` after two virtual calls. The body is the callee.
- `00733000` has ledger size 50, which ends in the middle of the version setup. The entry is not a complete function.
- `00731530` is already `reversed` by `recon/protcave`.
- Deleting destructors (the stack-byte test followed by the free helper) and pure `jmp` thunks were not counted.
- Slot-2 name getters and NAMEKEY caches in this range are already taken by the earlier gamelogic72 pass.
- Further xfer clones that only forward a version byte were left out so the boilerplate share stays low.

## Oracle

`ra3_obj72check` and `tools/analysis/ports/verify_obj72.py` run the same cases. The checker calls the C++ port. Unicorn runs the shipped bytes. Both sides install the same stubs, including the in-range follow helpers and the securom jumps those helpers would otherwise enter. Pointer returns are compared by tag (`obj`, `tmpl`, `mix`, `wpn`, `hit`, `slot`, `elem`, `ptr`), not by heap address.

After `kAimState2` was restored to 2:

```
cases=113 mismatches=0
```

Semantic mutation: `kAimState2` was changed from 2 to 9 and the Release checker was rebuilt. The oracle caught it on the state pushed at `0x0072D777`:

```
aim2 0x20 0 log
  cpp u:1,v34,aim:20,v1c:9,cf:2c
  emu u:1,v34,aim:20,v1c:2,cf:2c
aim2 0x20 2 log
  cpp u:1,v34,aim:20,v1c:9
  emu u:1,v34,aim:20,v1c:2
```

The constant was restored to 2, the checker was rebuilt, and the oracle returned `cases=113 mismatches=0`. The tree does not contain the mutant.

## Hooks

None.
