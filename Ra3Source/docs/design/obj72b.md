# Object modules `0x00720000–0x0073FFFF`, second pass

## Scope and ownership

This batch keeps 40 clean `unanalyzed` ledger entries whose starts lie in `0x00720000–0x0073FFFF`. Every counted body is one slot of a primary split vtable. The class name is the C string returned by slot 2 (`mov eax, imm32; ret`), read from `ra3_1.0.game`. Slot numbers are decimal. `work/obj72b_claims.txt` records one `vt` line per function; `verify_ownership.py` reports `fails=0`.

| Vtable | Slot 2 string | Counted slots |
| --- | --- | --- |
| `00C22118` | AIUpdateInterface | 24, 27, 41–43, 45, 51, 53, 71 |
| `00C08A48` | EncloseBehavior | 25 |
| `00C1BC90` | CaptureSpecialAbilityUpdate | 26, 36 |
| `00C1D738` | SpecialDisguiseUpdate | 3, 23 |
| `00C1E498` | ToggleMountedSpecialAbilityUpdate | 3, 23 |
| `00C24EE0` | HordeAIUpdate | 22 |
| `00C0AE88` | CompositeStructureInfoModule | 1 |
| `00C1AE58` | LaserStateModule | 23 |
| `00C1D078` | RadarScanMapSpecialAbilityUpdate | 36 |
| `00C200A0` | TransportContain | 42 |
| `00C08DF8` | NotifyWhenAttackingBehavior | 3 |
| `00C11230` | PassiveAreaEffectBehavior | 3 |
| `00C124C8` | ManagerBody | 3 |
| `00C0A598` | CrateCollide | 3 |
| `00C0A748` | SquishCollide | 3 |
| `00C0CC70` | CircularDamageUpdate | 3 |
| `00C0D158` | InvalidTargetsDeathUpdate | 3 |
| `00C0D4E0` | LinearDamageUpdate | 3 |
| `00C13F50` | MissileUpdate | 3 |
| `00C1BA58` | BeamSpecialAbilityUpdate | 3 |
| `00C1BEA8` | CloneStoredObjectsSpecialPowerUpdate | 3 |
| `00C1C320` | EjectPassengersSpecialAbilityUpdate | 3 |
| `00C1E258` | ToggleHiddenSpecialAbilityUpdate | 3 |
| `00C1E6F0` | ToggleStatusSpecialAbilityUpdate | 3 |
| `00C1E918` | WeaponFireSpecialAbilityUpdate | 3 |
| `00C23D58` | AnimalAIUpdate | 3 |
| `00C24180` | AssaultTransportAIUpdate | 3 |
| `00C11688` | SpawnBehavior | 3 |
| `00C0E5D0` | WeaponScatterEffectUpdate | 3 |

The dword at `0x00BE700C` is `0x7F7FFFFF`. The dword at `0x00BD90A4` is `0x3F800000` (`1.0f`). The dword at `0x00BD94EC` is `0x42C80000` (`100.0f`), the snap distance in `0x0072D090`. Slot 2 of `00C08DF8` returns `0x00C08E50` (`NotifyWhenAttackingBehavior`); `orderWeapon0A` passes that immediate to the name-key helper. Slot 2 of `00C1D738` returns `0x00C07C88` (`SpecialDisguiseUpdate`); `attackOrState26` passes that immediate on the fallback path.

There are no new hooks.

## Inventory

| VA | Size | Slot | Function |
| --- | --- | --- | --- |
| `00720200` | 371 | LaserStateModule 23 | `LaserStateModule_fillOutput` |
| `00720380` | 164 | NotifyWhenAttackingBehavior 3 | `NotifyWhenAttackingBehavior_xfer` |
| `00720df0` | 106 | PassiveAreaEffectBehavior 3 | `PassiveAreaEffectBehavior_xfer` |
| `007212b0` | 371 | SpawnBehavior 3 | `SpawnBehavior_xfer` |
| `00722ab0` | 122 | ManagerBody 3 | `ManagerBody_xfer` |
| `00722c00` | 122 | CrateCollide 3 | `CrateCollide_xfer` |
| `007232c0` | 164 | SquishCollide 3 | `SquishCollide_xfer` |
| `007280f0` | 344 | TransportContain 42 | `TransportContain_refreshPassengers` |
| `00728500` | 132 | CompositeStructureInfoModule 1 | `CompositeStructureInfoModule_applyRecords` |
| `00728f80` | 156 | CaptureSpecialAbilityUpdate 36 | `CaptureSpecialAbilityUpdate_releaseHeld` |
| `00729980` | 318 | RadarScanMapSpecialAbilityUpdate 36 | `RadarScanMapSpecialAbilityUpdate_clearScan` |
| `0072d090` | 264 | AIUpdateInterface 27 | `AIUpdateInterface_snapNear` |
| `0072d9c0` | 238 | AIUpdateInterface 41 | `AIUpdateInterface_followPathFlag` |
| `0072dab0` | 231 | AIUpdateInterface 42 | `AIUpdateInterface_followPath34` |
| `0072dba0` | 231 | AIUpdateInterface 43 | `AIUpdateInterface_followPath3B` |
| `0072dc90` | 273 | AIUpdateInterface 45 | `AIUpdateInterface_orderWeapon0A` |
| `0072df50` | 293 | AIUpdateInterface 51 | `AIUpdateInterface_buildList16` |
| `0072e3d0` | 183 | AIUpdateInterface 24 | `AIUpdateInterface_steerFloat21` |
| `0072e490` | 194 | AIUpdateInterface 53 | `AIUpdateInterface_placeTarget21` |
| `0072e720` | 236 | AIUpdateInterface 71 | `AIUpdateInterface_attackOrState26` |
| `0072fb90` | 204 | CircularDamageUpdate 3 | `CircularDamageUpdate_xfer` |
| `007305c0` | 93 | InvalidTargetsDeathUpdate 3 | `InvalidTargetsDeathUpdate_xfer` |
| `00730750` | 226 | LinearDamageUpdate 3 | `LinearDamageUpdate_xfer` |
| `00730980` | 294 | MissileUpdate 3 | `MissileUpdate_xfer` |
| `007331a0` | 372 | WeaponScatterEffectUpdate 3 | `WeaponScatterEffectUpdate_xfer` |
| `007340f0` | 71 | EncloseBehavior 25 | `EncloseBehavior_tryApplyKey` |
| `0073e380` | 289 | BeamSpecialAbilityUpdate 3 | `BeamSpecialAbilityUpdate_xfer` |
| `0073e6e0` | 156 | CaptureSpecialAbilityUpdate 26 | `CaptureSpecialAbilityUpdate_onTrigger` |
| `0073ea60` | 95 | CloneStoredObjectsSpecialPowerUpdate 3 | `CloneStoredObjectsSpecialPowerUpdate_xfer` |
| `0073eb10` | 133 | EjectPassengersSpecialAbilityUpdate 3 | `EjectPassengersSpecialAbilityUpdate_xfer` |
| `0073efa0` | 181 | SpecialDisguiseUpdate 23 | `SpecialDisguiseUpdate_blendScale` |
| `0073f060` | 222 | SpecialDisguiseUpdate 3 | `SpecialDisguiseUpdate_xfer` |
| `0073f3f0` | 63 | ToggleHiddenSpecialAbilityUpdate 3 | `ToggleHiddenSpecialAbilityUpdate_xfer` |
| `0073f430` | 197 | ToggleMountedSpecialAbilityUpdate 23 | `ToggleMountedSpecialAbilityUpdate_blendMounted` |
| `0073f500` | 66 | ToggleMountedSpecialAbilityUpdate 3 | `ToggleMountedSpecialAbilityUpdate_xfer` |
| `0073f550` | 63 | ToggleStatusSpecialAbilityUpdate 3 | `ToggleStatusSpecialAbilityUpdate_xfer` |
| `0073f600` | 201 | WeaponFireSpecialAbilityUpdate 3 | `WeaponFireSpecialAbilityUpdate_xfer` |
| `0073f6d0` | 181 | AnimalAIUpdate 3 | `AnimalAIUpdate_xfer` |
| `0073f8a0` | 280 | AssaultTransportAIUpdate 3 | `AssaultTransportAIUpdate_xfer` |
| `0073fe20` | 118 | HordeAIUpdate 22 | `HordeAIUpdate_forwardMove` |

Ledger size 318 for `00729980` ends at `00729ABE`, which is `ret 8`. Ledger size 344 for `007280F0` ends at `00728248`, which is `add esp, 0x20; ret`. The oracle maps `.text` and executes those epilogues.

## What the bodies do

AIUpdateInterface commands sit on `00C22118`. Several of them test `usable` (`al == 0` returns that EAX) or byte `self+0x436`.

- `snapNear` (`0072d090`) calls the near check on `object+0x38`. When that low byte is set, it compares the x87 distance with `100.0f` at `0x00BD94EC` (`fcomip` of the constant against the distance; `jbe` keeps the caller point). It copies the object position through `self+0x214`, calls locomotor slot `0x34`, steers with `0x7F7FFFFF`, clears `self+0x190` and byte `self+0x432`, stores the mode at `self+0x50`, and calls slot `0x1C` with state `0x3E` or `0x4B`. Mode 0/1 then replaces EAX with the follow call.
- `followPathFlag` (`0072d9c0`) places the last 12-byte path point when the vector is non-empty. A non-zero flag byte selects state 7 and skips follow. An empty path still binds the path and uses state 6 or 7. The return is the state call.
- `followPath34` / `followPath3B` (`0072dab0`, `0072dba0`) share one body with state `0x34` or `0x3B`. An empty path returns the end pointer: the compare is `mov eax, [path+4]` and the equal branch returns that EAX, so the slot `0x34` return is discarded. A non-empty path places the last point, follows only for mode 0/1, binds the path, stores the mode, `[object+0xDC]` or 0 at `self+0x180`, and the caller bits at `self+0x1F0`. The return is the state call.
- `orderWeapon0A` (`0072dc90`) returns 0 for a null target or when `object+0x471` bit 3 is set. It clears `object+0x490`. If the drawable at `object+0x374` has a non-zero `+0x40`, it returns that drawable pointer. Otherwise it marks the object unless the mode is 2 and `self+0x508` is 0, calls locomotor slots `0x34`, `0x38`, and `0x1C`. The state immediate at `0072DD38` is `push 0xa`. A resolved weapon receives the aux at `+0x24`. Mode 0/1 follows the target. The name key is `0x00C08E50`.
- `buildList16` (`0072df50`) allocates 0x30 bytes and constructs a list, then walks the caller's range and the weapon set at `object+0x37C`. The two weapon loops are not the same shape: slot `0x24` with no argument, then slot `0x20` with a pushed 0, and the `-1` compare at `weapon+0xC` takes a different branch in each loop. The function return is `free(begin)` when `begin` is non-zero. When `begin` is 0, EAX is that 0: `mov eax, [esp+0xc]` overwrites the query and follow returns before the test.
- `steerFloat21` (`0072e3d0`) returns before the body when byte `self+0x436` is set or `usable`'s low byte is 0. It stores the caller bits at `self+0x1F0` and uses state `0x21`. Follow runs only for mode 0/1. EAX is then `[object+0x418]`. A zero script object returns 0. A non-zero script object is passed to `0x00844460`.
- `placeTarget21` (`0072e490`) is the same tail with a place at `target+0x38` using `0x7F7FFFFF`. The script call is a tail jump in the binary; the port calls the same stub.
- `attackOrState26` (`0072e720`) returns 0 for a null target. A non-zero current target is returned as-is. A chain at `object+0x328` whose slot `0x84` is non-zero is called as `slot 0x84(target, mode)` and that EAX is returned. The fallback name key is `0x00C07C88`. It finishes with state `0x26` and `or eax, -1`, stored at `self+0x308`, `+0x368`, and `+0x3C8`.

Other methods:

- `tryApplyKey` (`007340f0`) is called with `this` 0x20 bytes into the interface block. A failed predicate (`al == 0`) or a zero byte at `self+4` returns the predicate EAX with the low byte cleared. Success, including the no-key and id `-1` paths, falls into `mov al, 1`, so the return is `(previous & ~0xFF) | 1`.
- `releaseHeld` (`00728f80`) looks up `self+0x4C`. A hit calls the float helper with `1.0f`, clears `found+0xEC` and the key, then forwards to the base update.
- `blendScale` (`0073efa0`) returns the ready call when its low byte is 0. Otherwise it truncates template `+0x24` (state at `self+0x38` equals 2) or `+0x20` toward zero, blends with template `+0xF0` and `1.0f`, and stores the float at `self+0xBC`. EAX is `(trunc_low32 & 0xFFFFFF00) | (ready & 0xFF)`.
- `blendMounted` (`0073f430`) runs the same truncation before the null check. A null mounted object at `object+0xF4` returns `ready & 0xFF`. A mounted object stores the blend at `mounted+0x1BC` and returns `(mounted & 0xFFFFFF00) | (ready & 0xFF)`.
- `forwardMove` (`0073fe20`) returns the object pointer when the chain at `object+0x328` is null and the vector at `+0x32C` is empty or its first element is null. Slot `0x84` returning 0 returns 0. Success calls `0x0072CFD0` (the move-with-vector body from the previous pass) and returns that EAX.
- `applyRecords` (`00728500`) returns the begin-records EAX for an empty vector. A non-empty vector returns the last apply-record EAX. A zero id skips the map lookup.
- `onTrigger` (`0073e6e0`) returns the trigger EAX when the flag low byte is 0. Otherwise it allocates a node, stores `object+0xF4` at `node+8`, links it, posts the list, and returns the wipe-list EAX.
- `fillOutput` (`00720200`) copies `self+0x3C/+0x40/+0x44` to the output and returns the output pointer when the key is 0 or the lookup misses. A hit whose pair is 0 copies `found+0x38/+0x3C/+0x40` and returns the Y bits at `found+0x3C`.
- `clearScan` (`00729980`) walks the id vector at `self+0xC8`. A non-zero id that resolves to an object with a non-zero `+0xF4` calls `0x00681030`. It then collapses the id vector (`memmove` length is 0) and, when the circular list at `self+0xD8` is non-empty, pulses the scan global at `0x00DF60A0` with the shift at `[object+0x418]+0x20`.
- `refreshPassengers` (`007280f0`) transforms the passenger point. The translation terms at matrix `+0x14/+0x24/+0x34` are added and then subtracted, so they cancel. `r1` is `(m1c*vy) + (m20*vz) + (m18*vx) + m24`. The project call is `0x00716020`. Slot `0x128` must return a passenger; slot `0x10C` returning 0 leaves early (`test eax` / `jbe`, and `test` clears CF).

Xfer slot 3:

- Notify, Passive, Manager, Crate, Invalid, Linear, Beam, Clone, Eject, and Animal version, transfer a base or ability, then a fixed list of fields. They return the last transfer.
- Squish versions `{1,3}`. Version 2 transfers a stack pair. Version 3 or higher transfers `self+0x34`. Version 1 skips both. Two more `{1,1}` versions and the root follow.
- Circular versions `{1,2}`. The extra slot `0x64` at `self+0x58` runs only when the version byte stays at least 2.
- Missile clears `self+0x110/+0x114/+0x118` when bit 1 of `xfer+4` is set, then transfers the rest.
- Disguise transfers six fields. The following `mov eax, [xfer+4]; shr eax, 1` replaces the last transfer. Bit 0 of that shifted value, `object+0xC8` bit 9, and non-zero `self+0xC0` and `self+0xD0` call the disguise-flag helper. Otherwise EAX stays the shifted flags.
- Hidden versions, transfers `self+0xB8`, then the ability. Mounted and Status version, transfer the ability, then one field (`+0xBC` via slot `0x84` or `0x6C`).
- WeaponFire returns `flags >> 2` when bit 2 is set. Otherwise a present byte at `self+0xBC` selects slot `0x0C` or the `0x04` / `0x20` / `0x08` path. The immediate is `0x00C21F4C`.
- Assault returns `flags >> 3` when bit 3 is set. Otherwise it versions and walks `self+0x574` records of 4 bytes from `self+0x520`. The count transfer is a different slot `0x70` from the stack local copied back to `self+0x56C`.
- Spawn returns `flags >> 3` when bit 3 is set. Bit 1 tests the second dword copied from the string at `self+0x40`, not the first. A zero second dword skips the name lookup.
- Scatter versions twice. The record count is the low byte of `(end-begin) >> 4` for the vectors at `self+0x2C` and `self+0x3C`. Bit 1 resizes both. The count byte is not overwritten by the element transfers.

## Boilerplate

None of the 40 counted functions is a slot-2 getter, a NAMEKEY cache, a return-constant stub, a base-only xfer, or a single-field accessor. The three short toggle xfers still version and transfer one extra field plus the ability xfer. `buildList16` allocates 0x30 bytes and then walks weapons; it is not a same-shape factory. Same-shape factories counted: 0. Boilerplate counted: 0.

## Dropped

These starts were read and left out. They are not counted.

- Slot 0 of these tables is the scalar deleting destructor. None of those are counted.
- `00722bd0` reaches `call 0x008ADD50` and returns. That is the EASTL append helper, not a module method.
- `00723370` makes two virtual calls on `this+0x16C` and tail-jumps `0x0070F2E0`.
- `00722c80`, `007228f0`, and `0073eed0` version `{1,1}` and forward to `0x00722C00`, `0x00722750`, and `0x007321F0`.
- `0072e810` is the AI xfer stub used by this batch. Its body jumps to `0x0042F84D` at `0072E829`. It is not counted.
- `007321f0` versions and jumps to `0x00416886`. It is stubbed, not counted.
- `0073f5e0` executes `fldz; ret` on the null path.
- `0073f3d0` is `jmp 0x00731E00`. `0073f3e0` writes 1 and jumps to the same address. `0072a700` is `jmp 0x00718B80`. `00721580` is `call 0x0070C890; ret 8`.
- `00722cb0` is a real SalvageCrate body. It loads `fld1`, pushes `0x41`, and pushes the table at `0x00BF72C0`. The x87 switch was left out of this oracle.
- `0072fec0`, `0072e080`, `00731e00`, `0073aa60`, `00738140`, `00739f50`, `007365f0`, `00723a10`, `00723d20`, `007221c0`, `00721c40`, `00720a70`, `0073e4b0`, `0072aa50`, `0073fc90`, `007302e0`, `00732490`, and `0072d800` open with large frames (`sub esp` from 8 to `0x120`, or an x87/SSE prologue). They were left for a later pass rather than counted from a partial reading.

## Oracle

`ra3_obj72bcheck` and `tools/analysis/ports/verify_obj72b.py` run the same 99 cases. The checker calls the C++ port. Unicorn runs the shipped bytes with the x87 control word set to `0x037F`. Both sides install the same stubs. Pointer returns are compared by tag. Field dumps use the relative tag plus the hex offset.

After `kOrderState0A` was restored to `0x0A`:

```
mismatches=0
```

Semantic mutation: `kOrderState0A` was changed from `0x0A` to `0x0B` and the Release checker was rebuilt. The oracle caught it on the state pushed at `0x0072DD38`:

```
FAIL ordGo
  cpp ... v1c:b ...
  emu ... v1c:a ...
FAIL ordTwice
  cpp ... v1c:b ... v1c:b ...
  emu ... v1c:a ... v1c:a ...
FAIL ordMode2
  cpp ... v1c:b ...
  emu ... v1c:a ...
FAIL ordMod
  cpp ... v1c:b ...
  emu ... v1c:a ...
mismatches=4
```

The early-out cases `ord0`, `ordBit`, and `ordDraw` stayed green because they return before the `push 0xa`. The constant was restored to `0x0A`, the checker was rebuilt, and the oracle returned `mismatches=0`. The tree does not contain the mutant.

## Hooks

None.
