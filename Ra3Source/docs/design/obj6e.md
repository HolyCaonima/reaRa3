# Object modules `0x006E0000–0x006FFFFF`

## Scope

This batch keeps 51 clean, unanalyzed ledger entries whose starts lie in
`0x006E0000–0x006FFFFF`. Each one is a non-getter slot of a split vtable.
Where that table's slot 2 is `mov eax, imm32; ret`, the class name is the C
string it returns. Secondary tables have no slot-2 getter; the class name is
the slot-2 string of the primary table stored by the same constructor. Both
stores are `wr` lines in `work/obj6e_claims.txt`, and
`verify_ownership.py` reports `fails=0`.

Boilerplate counted: 0. No slot-2 getters, namekey caches, constant stubs, or
base-only xfers are in the set. No factory or clone methods. Hooks: none.

## Classes

| Vtable | How the name was read |
| --- | --- |
| `00C01108` | slot 2 returns SpecialPowerModule |
| `00C00F18` | ctor `006CAC50` also stores `00C01108` |
| `00C08A48` | slot 2 returns EncloseBehavior |
| `00BF8DA8` | ctor `0069F890` also stores `00BDBC08` (BehaviorModule) |
| `00BF8F40` | slot 2 returns StoreCurrentStateUpdate |
| `00BF96A8` | slot 2 returns WeaponInitState |
| `00BF9770` | slot 2 returns WeaponPreFireState |
| `00BF97D8` | slot 2 returns WeaponFireState |
| `00BFB418` | ctor `0068CFA0` also stores `00BFB3C0` (GateProxyBehavior) |
| `00BFD820` | slot 2 returns UnifiedControlBehavior |
| `00C114E8` | ctor `006C7A20` also stores `00C11688` (SpawnBehavior) |
| `00C142D8` | slot 2 returns SpecialAbilityUpdate |
| `00C16930` | ctor `006FDBA0` also stores `00C16A60` (MovementSpecialPower) |
| `00C16B10` | ctor `006FDC00` also stores `00C16CE8` (MoveToPositionAndEvacuateSpecialPower) |
| `00C16D98` | ctor `006FDC50` also stores `00C16F70` (MoveToPositionAndReplaceSelfSpecialPower) |
| `00C17028` | ctors `006E1E60` and `006E1F10` also store `00C17200` (OCLSpecialPower) |
| `00C19580` | ctor `006E8ED0` also stores `00C19788` (ProductionUpdate) |
| `00C19788` | slot 2 returns ProductionUpdate |
| `00C1B5D8` | slot 2 returns ActivateModuleSpecialAbilityUpdate |
| `00C1CC18` | slot 2 returns GiveUpgradeUpdate |
| `00C1E258` | slot 2 returns ToggleHiddenSpecialAbilityUpdate |
| `00C1E498` | slot 2 returns ToggleMountedSpecialAbilityUpdate |
| `00C1F8F0` | `0x00727820` stores `00C1FB90` (OpenContain) and `00C1F8F0`; the stores sit after its cave jump at `0x00727826`, in the continuation Ghidra split as `0x0072782B` (controller correction) |
| `00C20BC0` | slot 2 returns MoneyGainAttachUpdate |
| `00C20D90` | slot 2 returns StealUnitUpdate |
| `00C21520` / `00C217F8` | ctor `0073CB30` also stores `00C21A98` (HordeContain) |
| `00C22118` | slot 2 returns AIUpdateInterface |
| `00C23350` | ctor `0075D9D0` also stores `00C235F0` (HealContain) |
| `00C24878` | ctor `00751D20` also stores `00C24A50` (DozerAIUpdate) |
| `00C24D18` | ctor `0075FCD0` also stores `00C24EE0` (HordeAIUpdate) |
| `00C251A8` | ctor `007522B0` also stores `00C25398` (SupplyTruckAIUpdate) |
| `00C25398` | slot 2 returns SupplyTruckAIUpdate |

## Counted functions

| VA | C++ | Ownership |
| --- | --- | --- |
| `006E1C80` | `MovementSpecialPower_adjustedField` | `00C16930` slot 11 |
| `006E1D30` | `MoveToPositionAndEvacuateSpecialPower_dispatch` | `00C16B10` slot 20 |
| `006E1E40` | `MoveToPositionAndReplaceSelfSpecialPower_dispatch` | `00C16D98` slot 20 |
| `006E1F70` | `OCLSpecialPower_dispatch` | `00C17028` slot 20 |
| `006E2F30` | `SpecialPowerModule_dispatchIfBit25` | `00C01108` slot 6 |
| `006E2F70` | `SpecialPowerModule_dispatchUnlessBit6` | `00C00F18` slot 3 |
| `006E3220` | `ToggleHiddenSpecialAbilityUpdate_clearHidden` | `00C1E258` slot 43 |
| `006E3290` | `ToggleHiddenSpecialAbilityUpdate_applyHidden` | `00C1E258` slot 44 |
| `006E3E10` | `DozerAIUpdate_acceptsObject` | `00C24878` slot 20 |
| `006E4220` | `DozerAIUpdate_resetAction` | `00C24878` slot 13 |
| `006E44D0` | `DozerAIUpdate_workerFromId` | `00C24878` slot 26 |
| `006E46E0` | `HordeAIUpdate_flagPassenger` | `00C24D18` slot 12 |
| `006E48F0` | `SupplyTruckAIUpdate_acceptTarget` | `00C25398` slot 35 |
| `006E4920` | `SupplyTruckAIUpdate_deliverBoxes` | `00C25398` slot 67 |
| `006E4980` | `SupplyTruckAIUpdate_deliverPosition` | `00C25398` slot 69 |
| `006E4A10` | `SupplyTruckAIUpdate_recordCargo` | `00C25398` slot 68 |
| `006E8750` | `MoneyGainAttachUpdate_maybeGain` | `00C20BC0` slot 26 |
| `006E8FD0` | `ProductionUpdate_producerField` | `00C19580` slot 27 |
| `006E9010` | `ProductionUpdate_incrementQueued` | `00C19788` slot 22 |
| `006E9040` | `ProductionUpdate_objectFromId` | `00C19580` slot 47 |
| `006E91F0` | `ProductionUpdate_clearLink` | `00C19580` slot 36 |
| `006E9390` | `ProductionUpdate_bindProducer` | `00C19580` slot 20 |
| `006E9EE0` | `BehaviorModule_objectOrigin` | `00BF8DA8` slot 7 |
| `006EB4E0` | `SpecialAbilityUpdate_isReady` | `00C142D8` slot 39 |
| `006EBAC0` | `StealUnitUpdate_syncTarget` | `00C20D90` slot 25 |
| `006EBBD0` | `StoreCurrentStateUpdate_xfer` | `00BF8F40` slot 3 |
| `006ECE70` | `EncloseBehavior_offerToContainer` | `00C08A48` slot 24 |
| `006ED440` | `WeaponInitState_enter` | `00BF96A8` slot 4 |
| `006ED4A0` | `WeaponPreFireState_clearFlag` | `00BF9770` slot 6 |
| `006ED4E0` | `WeaponFireState_advance` | `00BF97D8` slot 5 |
| `006F6C10` | `GateProxyBehavior_call14` | `00BFB418` slot 5 |
| `006F6C30` | `GateProxyBehavior_call18` | `00BFB418` slot 6 |
| `006F6C50` | `GateProxyBehavior_call1C` | `00BFB418` slot 7 |
| `006F6C70` | `GateProxyBehavior_call10` | `00BFB418` slot 4 |
| `006F6C90` | `GateProxyBehavior_call20` | `00BFB418` slot 8 |
| `006F7DB0` | `SpawnBehavior_anyLiving` | `00C114E8` slot 7 |
| `006F8A60` | `UnifiedControlBehavior_onRadius` | `00BFD820` slot 23 |
| `006F94B0` | `HordeContain_passengerSlotB0` | `00C217F8` slot 44 |
| `006F94F0` | `HordeContain_passengerSlot28` | `00C217F8` slot 10 |
| `006F9710` | `HordeContain_knowsObject` | `00C21520` slot 5 |
| `006F9B00` | `HordeContain_testTriple` | `00C217F8` slot 73 |
| `006F9B40` | `HordeContain_passengerSlot70` | `00C217F8` slot 28 |
| `006FA9E0` | `HealContain_noteFrame` | `00C23350` slot 38 |
| `006FBB50` | `OpenContain_passengerFlag` | `00C1F8F0` slot 44 |
| `006FCAD0` | `ActivateModuleSpecialAbilityUpdate_enable` | `00C1B5D8` slot 6 |
| `006FDA30` | `GiveUpgradeUpdate_runGrant` | `00C1CC18` slot 38 |
| `006FE070` | `ToggleMountedSpecialAbilityUpdate_mountedReady` | `00C1E498` slot 39 |
| `006FF060` | `SupplyTruckAIUpdate_popPending` | `00C251A8` slot 7 |
| `006FF9F0` | `AIUpdateInterface_allowsMove` | `00C22118` slot 167 |
| `006FFAD0` | `AIUpdateInterface_locomotorReady` | `00C22118` slot 130 |
| `006FFD50` | `AIUpdateInterface_orderState33` | `00C22118` slot 74 |

Object lookups go through the global at `0x00DEBA64`: lock `0x00A21BE0`, find
`0x0050B5F0` on the map at clock+`0xC4`, unlock `0x00A21C90`. The port returns
the mapped object. `UnifiedControlBehavior_onRadius` uses the MSVC ordered
float compare (`ucomiss` / `lahf` / `test ah, 0x44` / `jnp`) against the 1.0
constant at `0x00BD90A4`. `ToggleMountedSpecialAbilityUpdate_mountedReady` uses
`comiss`/`jbe` against the threshold at `0x00BD9F94` (`0x4B189680`).

`SupplyTruckAIUpdate_popPending` inlines a one-element vector pop and, when the
dock pointer is set, calls `0x0052DDE0` with the player field and the new
count. That call is why the body is kept; a bare erase is not.

## Dropped

- `006F6E60`, `006F7600`, `006F8B00`, `006F8C80`, `006F8D90`: begin/end pointer
  compare and slide, the EASTL vector-erase shape, with no other call.
- `006FDDB0`: tests a pointer and calls `0x004166B0` to release it. Destructor
  path, not counted.
- `006E4D40`, `006E4E30`, `006E6240`, `006EEAB0`: larger update and snapshot
  bodies (200-plus bytes, several calls). Left out so the counted set stays
  inside 40–60.
- `006E79D0`, `006E81B0`, `006EDE00`, `006EE1E0`, `006E9F60`, `006EC360`: the
  source-path anchors named in the brief. Their bodies are the large nugget
  and update functions; this batch uses the smaller vtable methods above.
- In-range constructors such as `006E1F10`, `006E2AB0` and `006E8ED0` are `wr`
  anchors only. They write vtables and are not counted.

## Oracle

`python Ra3Source/tools/analysis/ports/verify_obj6e.py` against the Release
checker:

```
cases=127 mismatches=0
```

Mutation: `WeaponFireState_advance` compares `sum > frame` (`ja` on the
unsigned frame sum). Changing that compare to `>=` makes the equal-frame case
return 0 and leave state 5, while the native returns `0xFFFFFFFF` and stores
state 4. The oracle reported `cases=127 mismatches=1` on `run 0x6ed4e0`.
The compare was restored and the oracle returned to `mismatches=0`.

## Hooks

None.

主控验收口径（controller）：5 个 GateProxyBehavior 守卫转发（`006F6C10`–`006F6C90`，同形）、2 个纯转发（`006E1E40`、`006E91F0`）与 3 个单字段读取/自增或调整后指针（`006E1C80`、`006E8FD0`、`006E9010`）按样板计，共 10/51，未超 30%。
