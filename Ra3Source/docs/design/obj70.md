# Object modules `0x00700000–0x0071FFFF`

## Scope and ownership

This batch keeps 44 clean, unanalyzed ledger entries. Every counted VA is a
slot of a split vtable. Where that table's slot 2 is `mov eax, imm32; ret`,
the class name is the C string at the immediate. Secondary tables have no
slot-2 getter; the class is the slot-2 string of the primary table stored by
the same constructor, and `work/obj70_claims.txt` cites both stores with `wr`.
The dword at `vtable + 4 * slot` is the counted VA. `verify_ownership.py`
reports `fails=0`.

Slot-2 strings read from the image:

| Vtable | Slot 2 string | How the counted function is attached |
| --- | --- | --- |
| `00C1BA58` | BeamSpecialAbilityUpdate | constructor `00728950` also stores `00C1B918`, `00C1B928`, `00C1B8D4` |
| `00C1EE08` | OCLMonitorSpecialAbilityUpdate | primary; constructor `00730BB0` also stores `00C1ECA4` |
| `00C1E918` | WeaponFireSpecialAbilityUpdate | constructor `00712DC0` also stores `00C1E7A0` |
| `00C0D4E0` | LinearDamageUpdate | constructor `006B8580` also stores `00C0D538` |
| `00C108B0` | SubStructureBuildUpgrade | constructor `00703690` also stores `00C10754` |
| `00C142D8` | SpecialAbilityUpdate | constructor `00701F40` also stores `00C14184` |
| `00C09B20` | StoreTemplateIDBehavior | constructor `006C7BE0` also stores `00C099E8` |
| `00C11840` | StancesBehavior | primary |
| `00C1D960` | SwitchLocomotorsSpecialAbilityUpdate | primary |
| `00BDBC08` | BehaviorModule | constructor `0069F890` also stores `00BF8DA8` |
| `00BF9708` | WeaponReadyState | primary |
| `00C19788` | ProductionUpdate | constructor `006E8ED0` also stores `00C19580` |
| `00C20660` | BoobyTrapUpdate | primary |
| `00C19510` | MagneticEffectUpdate | primary; constructor `007783E0` also stores `00C193AC` |
| `00C21A98` | HordeContain | primary; constructor `0073CB30` also stores `00C21520` |
| `00C1F584` | GhostObject | constructor `0071A870` also stores `00C1F574` |
| `00C1D738` | SpecialDisguiseUpdate | constructor `00729CF0` also stores `00C1D5F8` |
| `00C27548` | Object | constructor `0076B240` also stores `00C27530` |
| `00C185C8` | RecallUnitsSpecialPower | constructor `006FDDE0` also stores `00C183F0` |
| `00C1C9D8` | GiveOrRestoreUpgradeSpecialPower | constructor `00729540` also stores `00C1C874` |
| `00C1AAB8` | BridgeBuilderBehavior | primary |
| `00C08A48` | EncloseBehavior | primary |
| `00C14120` | ShieldSphereUpdate | primary |
| `00BF97D8` | WeaponFireState | primary |
| `00BF98A0` | WeaponOutOfAmmoState | primary |
| `00C0CAD0` | BombardAreaUpdate | primary |
| `00C08480` | BeamTargetModule | primary |
| `00C0D338` | LiftObjectUpdate | primary |
| `00C13BB8` | EmotionTrackerUpdate | constructor `006CDA30` also stores `00C13A70` |
| `00C08DF8` | NotifyWhenAttackingBehavior | constructor `006F6DE0` also stores `00C08C94` |
| `00C01108` | SpecialPowerModule | primary |
| `00C22118` | AIUpdateInterface | primary |
| `00C11090` | BezierProjectileBehavior | constructor `006C6370` also stores `00C10F20` |
| `00C1F4E8` | SubObjectsUpgrade | constructor `00719880` also stores `00C1F388` |
| `00C05CA4` | ShieldSphereManager | constructor `006B9980` also stores `00C05CB4` |

Immediate read from the image: `0x00C053B8` is the C string `ProneUpdate`.
`AIUpdateInterface_applyProne` passes it to the name-key call when the ready
byte at `0x00DF98F4` is clear, and stores the key at `0x00DF98F0`.

Shared bodies are counted once. `00712EF0` is also slot 2 of `00C20174`.
`0071FA60` is also slot 7 of `00C13DE0`. Both are named from the constructor
cited in the claims file.

## Inventory

Boilerplate counted: 0. Same-shape factories counted: 0. Hooks: none.

| VA | Size | Class and slot | Function |
| --- | --- | --- | --- |
| `007006C0` | 157 | BoobyTrapUpdate `00C20660` slot 25 | `BoobyTrapUpdate_capture` |
| `00700F30` | 77 | MagneticEffectUpdate `00C193AC` slot 1 | `MagneticEffectUpdate_onBit` |
| `007011C0` | 55 | OCLMonitorSpecialAbilityUpdate `00C1ECA4` slot 0 | `OCLMonitorSpecialAbilityUpdate_forward` |
| `00701410` | 20 | LinearDamageUpdate `00C0D538` slot 7 | `LinearDamageUpdate_relayIfArmed` |
| `00702360` | 35 | SpecialAbilityUpdate `00C14184` slot 7 | `SpecialAbilityUpdate_pointInside` |
| `007035C0` | 22 | SubObjectsUpgrade `00C1F388` slot 2 | `SubObjectsUpgrade_relay` |
| `007037F0` | 28 | SubStructureBuildUpgrade `00C10754` slot 2 | `SubStructureBuildUpgrade_maskIf` |
| `00703F70` | 96 | WeaponReadyState `00BF9708` slot 4 | `WeaponReadyState_poll` |
| `00703FD0` | 125 | WeaponFireState `00BF97D8` slot 6 | `WeaponFireState_enter` |
| `00704050` | 143 | WeaponOutOfAmmoState `00BF98A0` slot 4 | `WeaponOutOfAmmoState_enter` |
| `00706190` | 80 | GhostObject `00C1F574` slot 1 | `GhostObject_linkWatcher` |
| `0070AFD0` | 47 | NotifyWhenAttackingBehavior `00C08C94` slot 0 | `NotifyWhenAttackingBehavior_clearIds` |
| `0070C900` | 44 | StoreTemplateIDBehavior `00C099E8` slot 0 | `StoreTemplateIDBehavior_pushId` |
| `0070D490` | 213 | StancesBehavior `00C11840` slot 58 | `StancesBehavior_accumulate` |
| `0070D570` | 48 | StancesBehavior `00C11840` slot 33 | `StancesBehavior_clearRecords` |
| `0070EAB0` | 123 | HordeContain `00C21520` slot 10 | `HordeContain_collectIds` |
| `0070EB30` | 77 | HordeContain `00C21A98` slot 1 | `HordeContain_refresh` |
| `00710BF0` | 17 | BeamSpecialAbilityUpdate `00C1B928` slot 7 | `BeamSpecialAbilityUpdate_adjustB4` |
| `00710C20` | 17 | BeamSpecialAbilityUpdate `00C1B928` slot 16 | `BeamSpecialAbilityUpdate_adjustB0` |
| `00710C50` | 37 | BeamSpecialAbilityUpdate `00C1B8D4` slot 2 | `BeamSpecialAbilityUpdate_sameOwner` |
| `00710C80` | 16 | BeamSpecialAbilityUpdate `00C1B918` slot 2 | `BeamSpecialAbilityUpdate_setMask46` |
| `00711A40` | 72 | GiveOrRestoreUpgradeSpecialPower `00C1C874` slot 6 | `GiveOrRestoreUpgradeSpecialPower_try` |
| `00711F10` | 68 | RecallUnitsSpecialPower `00C183F0` slot 20 | `RecallUnitsSpecialPower_filter` |
| `00712370` | 26 | SpecialDisguiseUpdate `00C1D5F8` slot 2 | `SpecialDisguiseUpdate_seed` |
| `00712730` | 181 | SpecialPowerModule `00C01108` slot 23 | `SpecialPowerModule_trigger` |
| `007128C0` | 61 | SwitchLocomotorsSpecialAbilityUpdate `00C1D960` slot 22 | `SwitchLocomotorsSpecialAbilityUpdate_latch` |
| `00712EF0` | 37 | WeaponFireSpecialAbilityUpdate `00C1E7A0` slot 2 | `WeaponFireSpecialAbilityUpdate_sameOwner` |
| `00714240` | 127 | AIUpdateInterface `00C22118` slot 79 | `AIUpdateInterface_applyProne` |
| `00714D10` | 131 | BombardAreaUpdate `00C0CAD0` slot 3 | `BombardAreaUpdate_xfer` |
| `00714DC0` | 102 | EmotionTrackerUpdate `00C13A70` slot 0 | `EmotionTrackerUpdate_release` |
| `007155F0` | 192 | LiftObjectUpdate `00C0D338` slot 3 | `LiftObjectUpdate_xfer` |
| `00715C20` | 129 | MagneticEffectUpdate `00C19510` slot 3 | `MagneticEffectUpdate_xfer` |
| `00715EE0` | 20 | OCLMonitorSpecialAbilityUpdate `00C1EE08` slot 42 | `OCLMonitorSpecialAbilityUpdate_spanOpen` |
| `00715FE0` | 59 | LinearDamageUpdate `00C0D538` slot 5 | `LinearDamageUpdate_clearIfUnset` |
| `00716A30` | 111 | ProductionUpdate `00C19580` slot 1 | `ProductionUpdate_doorCode` |
| `00716AA0` | 160 | ProductionUpdate `00C19580` slot 13 | `ProductionUpdate_scanOrPush` |
| `00717D30` | 25 | ShieldSphereManager `00C05CB4` slot 5 | `ShieldSphereManager_retarget` |
| `00718120` | 209 | ShieldSphereUpdate `00C14120` slot 3 | `ShieldSphereUpdate_xfer` |
| `00718200` | 85 | BehaviorModule `00BF8DA8` slot 2 | `BehaviorModule_clearMotion` |
| `0071C9F0` | 43 | Object `00C27530` slot 2 | `Object_maskedFind` |
| `0071F060` | 133 | BeamTargetModule `00C08480` slot 3 | `BeamTargetModule_xfer` |
| `0071FA60` | 76 | BezierProjectileBehavior `00C10F20` slot 7 | `BezierProjectileBehavior_copyAim` |
| `0071FDD0` | 99 | BridgeBuilderBehavior `00C1AAB8` slot 3 | `BridgeBuilderBehavior_xfer` |
| `0071FE60` | 100 | EncloseBehavior `00C08A48` slot 3 | `EncloseBehavior_xfer` |

## Behaviour

`BeamSpecialAbilityUpdate_setMask46` writes `*arg = 0x46` and returns the
pointer. `adjustB4` and `adjustB0` return `self + 0xB4` or `self + 0xB0`, and
return 0 when `this` is the adjustand `0x0C`. `sameOwner` (and the
WeaponFire twin at `this - 0xB4`) compares `[mine+4]` with `[theirs+4]`,
where mine is `[[[[self-off]+8]+8]]` and theirs is `[[other+8]]`.

`OCLMonitorSpecialAbilityUpdate_spanOpen` compares the dword at `this+0xB8`
with the other object's dword. EAX keeps the loaded high bytes and replaces
AL with 1 when the dwords differ. `forward` passes six stack arguments to
`006EA540` and then calls `007010D0` on `this-0x24`; the return is the second
call.

`LinearDamageUpdate_relayIfArmed` tests bit 0 of `[[self-0x20]+8]`. If set it
subtracts `0x24` and tails into `00701330`; otherwise EAX is the loaded
pointer. `clearIfUnset` returns the virtual pointer when the bit is set, and
otherwise the clear call.

`SubStructureBuildUpgrade_maskIf` returns the relay EAX when AL is 0, and
otherwise the byte at `this+0x30` in AL. `SubObjectsUpgrade_relay` returns the
object pointer with AL cleared when `[[self-8]+0xF4]` is 0, and otherwise
tails into `006ECE70`. `SpecialAbilityUpdate_pointInside` calls `006EA9D0`
with the point at `[self-0x1C]+0x38` and inverts AL.

`StoreTemplateIDBehavior_pushId` pushes one dword into the vector at
`this+4`. The grow source is the address of the stack id. A null old end is
returned and not written. `HordeContain_collectIds` uses the same fast path
but the grow source and the stored dword are the record pointer and its first
dword. `seta` after `sar eax, 2` is folded into AL of the count.
`StancesBehavior_clearRecords` calls `009279F0` and rewinds the end by the
16-byte span. `NotifyWhenAttackingBehavior_clearIds` and
`EmotionTrackerUpdate_release` rewind a dword vector after the import clear;
the size argument of that import is 0.

`StancesBehavior_accumulate` pushes a negative ordered delta, clamps the
current value to the cap, zeroes it when the floor is above it, and calls
the parent slot `0x6C`. The final flag is `mov al, 1` when `0 >= current` is
ordered, and `xor al, al` when current is positive or unordered. That EAX is
passed to `006D23F0`.

`SwitchLocomotorsSpecialAbilityUpdate_latch` returns 0 when bit 30 of the
special-ready result is clear. Otherwise it returns the special-ready EAX,
or the pointer at `this+4` when byte `[that+0x101]` is 0.
`BehaviorModule_clearMotion` returns the gate pointer when byte `[gate+0x6D]`
is set, and otherwise the `006F0A20` EAX. The status call runs only when bit
3 was set.

`WeaponReadyState_poll` returns 0 when byte `[edx+0x60]` is 0 and
`0xFFFFFFFF` otherwise. `WeaponFireState_enter` clears bit 14, optionally
binds a kind-5 weapon, and returns `006BCF30(self, 3)`.
`WeaponOutOfAmmoState_enter` returns `0xFFFFFFFE` when the template count is
negative or the weapon count is still positive, `0xFFFFFFFF` when the
template link is null, and otherwise tails into slot `0x14`.

`ProductionUpdate_doorCode` returns 4, 3, `0xE`, or 0 from the index, the
object bits, and the two door tests. `scanOrPush` pushes eight bytes when the
byte argument is 0. A node whose byte at `+0x65` is nonzero is not advanced;
the port only walks nodes whose byte is 0. A node is visited when the
production limit is greater than `[node+0x1C]` and `[node+0xC]` is 1.

`BoobyTrapUpdate_capture` returns the template pointer when the bit is clear,
and the incoming EAX when the byte is already set. Equal coordinates return
the project EAX with AH set to `0x42` (`lahf` after an ordered `ucomiss`
equal, including `+0 == -0`). A differing coordinate returns the set-position
EAX.

`MagneticEffectUpdate_onBit` returns the bit-array pointer when the bit is
clear. `HordeContain_refresh` compares the inverted subobject slot `0xF4`
with byte `this+0x291` and tails into slot `0x60` when they differ.
`GhostObject_linkWatcher` inserts a node on the list at `[ghostRoot]+0x14`
and returns the node. `SpecialDisguiseUpdate_seed` calls `00506C20` and
returns the argument pointer. `Object_maskedFind` returns 0 when bit 4 is
clear, and otherwise `0071ABE0(...) + 4`.

`RecallUnitsSpecialPower_filter` returns the full EAX of `006CACE0` when AL
is 0. Otherwise it scans with callback `00711ED0` and replaces AL via
`setne` against 1. `GiveOrRestoreUpgradeSpecialPower_try` returns the full
EAX of `0069FA90` when AL is 0, substitutes `[self-0x1C]+0x38` for a null
target, and inverts the AL of `006FD820`.

`SpecialPowerModule_trigger` fires once per outer count. After the loops,
`mov eax, [template+0x10]` discards the fire EAX. A null template and a null
extra therefore return 0. A template calls `006C1C50`. An extra calls
`00538530` and, when AL is 0, `0052EFC0`, unless bit 10 of
`[[object+4]+0xCC]` is set.

`AIUpdateInterface_applyProne` returns the module pointer unless the old
counter is not positive and the truncated sum is positive, in which case it
returns `006E96E0`. `BezierProjectileBehavior_copyAim` copies three dwords
from the argument and three from `[owner+0x38]`, clears `this+0x60`, and
tails into `0071F0F0` with the constant 1 in place of the first argument.
`ShieldSphereManager_retarget` tails into `00717B40` or `007176B0` from the
AL of `007174C0`.

The xfer bodies pass a two-byte version local to slot `0x18`. Enclose and
Magnetic compare the first local's second byte with 3. Shield compares a
second local `{1,2}` with 2 before slot `0x84`. On the bit-3 early path,
Shield and Lift execute `mov eax, [xfer+4]; shr eax, 3` and return that
shifted flags dword, not the preceding call. Bridge stores the place call at
`this+0x4C` when bit 1 is set. Lift walks the circular list at `this+0x2C`.
Beam calls `0054A090` three times around slot `0x84`.

## Dropped

These clean entries in the window were not counted:

- `0071A9F0`: `sub ecx, [ecx-4]; jmp 0071A9A0`. Thunk.
- `0070EB80`: loads a float, then `xor al, al; ret 4`. Constant false.
- `00710510`: calls `0070F2E0` and jumps to `006B2A30`. Thunk.
- `00701DC0`: `add ecx, -0x24; jmp 006EA3F0`. Thunk. The recorded size covers the padding after the jump.
- `007168B0` and `00716970`: same-shaped walks. Both load `0x00BD90A4` and step a stride-`0x7C` array. Counted at most as one shape, and left out.
- `00712F80`: the recorded size ends inside the jump table.
- `0070E350`: the recorded size ends mid-instruction.
- `0070F290`: begins with `call [0x00BBA1E4]` and a circular-list walk. Left as an import-led cleanup path.

EASTL `009279F0` and vector grow `006F3E20`, and the import at `0x00BBA500`, are callees. They are not counted. Deleting destructors, slot-4 name-key caches, and hash-map probes in the window were not selected; none of them is in the counted 44.

## Oracle

`python Ra3Source/tools/analysis/ports/verify_obj70.py` against the Release
checker prints `mismatches=0 cases=121`.

Mutation: `OCLMonitorSpecialAbilityUpdate_spanOpen` compared with `==` instead
of `!=`. The oracle reported `mismatches=3 cases=121`, all three `span`
cases, with AL inverted (`11223301` against `11223300`, and the two unequal
pairs swapped). The comparison was restored. The same command then printed
`mismatches=0 cases=121`.

No hook key is registered.
