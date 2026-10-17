# GameLogic 0x0072 Module Identity (`recon/gamelogic72`)

## Scope and Ownership Evidence

This batch reconstructs **51 clean (`securom=clean`) functions** in `0x00720000..0x0075FFFF`. Each one is a primary-vtable leaf of a module or AI update class. Slot 2 is `mov eax, imm32; ret` and the immediate is the class-name string (read from the image). Slot 4 is the 47-byte MSVC static initializer for `getModuleNameKey`: test the guard byte's bit 0, otherwise load the name-key generator from `[0x00E1A078]`, set the guard with `or dword ptr [guard], 1`, call `0x00986150` (thiscall, `ret 4`) with the same class-name string, and cache EAX. Generals uses the same static `NAMEKEY(#cls)` in `work/references/generals/GeneralsMD/Code/GameEngine/Include/Common/Module.h` (`MAKE_STANDARD_MODULE_MACRO`).

Ownership for every counted VA is the vtable slot below. The vtable start is a row in `vtables_split.tsv`, and `slot = (pointer address - start) / 4`. Class names are the slot 2 string, not a neighbouring table.

| VA | Class | Slot | Proof |
|---|---|---:|---|
| `0x00727950` | `OpenContain::getModuleName` | 2 | `00C1FB90 slot 2`; slot2 name "OpenContain" |
| `0x00727960` | `OpenContain::getModuleNameKey` | 4 | `00C1FB90 slot 4`; slot2 name "OpenContain" |
| `0x00727E20` | `TransportContain::getModuleName` | 2 | `00C200A0 slot 2`; slot2 name "TransportContain" |
| `0x00727E30` | `TransportContain::getModuleNameKey` | 4 | `00C200A0 slot 4`; slot2 name "TransportContain" |
| `0x007293F0` | `DelayedWeaponFireSpecialAbilityUpdate::getModuleName` | 2 | `00C202F0 slot 2`; slot2 name "DelayedWeaponFireSpecialAbilityUpdate" |
| `0x00729400` | `DelayedWeaponFireSpecialAbilityUpdate::getModuleNameKey` | 4 | `00C202F0 slot 4`; slot2 name "DelayedWeaponFireSpecialAbilityUpdate" |
| `0x0072AFB0` | `LuaSpyData::getModuleName` | 2 | `00C20460 slot 2`; slot2 name "LuaSpyData" |
| `0x0072F820` | `BoobyTrapUpdate::getModuleName` | 2 | `00C20660 slot 2`; slot2 name "BoobyTrapUpdate" |
| `0x0072F830` | `BoobyTrapUpdate::getModuleNameKey` | 4 | `00C20660 slot 4`; slot2 name "BoobyTrapUpdate" |
| `0x007304F0` | `GatherEjectUpdate::getModuleName` | 2 | `00C20848 slot 2`; slot2 name "GatherEjectUpdate" |
| `0x00730500` | `GatherEjectUpdate::getModuleNameKey` | 4 | `00C20848 slot 4`; slot2 name "GatherEjectUpdate" |
| `0x00730660` | `LeechTargetingAttachUpdate::getModuleName` | 2 | `00C209E8 slot 2`; slot2 name "LeechTargetingAttachUpdate" |
| `0x00730670` | `LeechTargetingAttachUpdate::getModuleNameKey` | 4 | `00C209E8 slot 4`; slot2 name "LeechTargetingAttachUpdate" |
| `0x00730AF0` | `MoneyGainAttachUpdate::getModuleName` | 2 | `00C20BC0 slot 2`; slot2 name "MoneyGainAttachUpdate" |
| `0x00730B00` | `MoneyGainAttachUpdate::getModuleNameKey` | 4 | `00C20BC0 slot 4`; slot2 name "MoneyGainAttachUpdate" |
| `0x00732E30` | `StealUnitUpdate::getModuleName` | 2 | `00C20D90 slot 2`; slot2 name "StealUnitUpdate" |
| `0x00732E40` | `StealUnitUpdate::getModuleNameKey` | 4 | `00C20D90 slot 4`; slot2 name "StealUnitUpdate" |
| `0x0073CC60` | `HordeContain::getModuleName` | 2 | `00C21A98 slot 2`; slot2 name "HordeContain" |
| `0x0073CC70` | `HordeContain::getModuleNameKey` | 4 | `00C21A98 slot 4`; slot2 name "HordeContain" |
| `0x0073DD70` | `GarrisonContain::getModuleName` | 2 | `00C21E90 slot 2`; slot2 name "GarrisonContain" |
| `0x0073DD80` | `GarrisonContain::getModuleNameKey` | 4 | `00C21E90 slot 4`; slot2 name "GarrisonContain" |
| `0x00740F10` | `AIUpdateInterface::getModuleName` | 2 | `00C22118 slot 2`; slot2 name "AIUpdateInterface" |
| `0x00740F20` | `AIUpdateInterface::getModuleNameKey` | 4 | `00C22118 slot 4`; slot2 name "AIUpdateInterface" |
| `0x00743760` | `SqueezeFlingUpdate::getModuleName` | 2 | `00C225F0 slot 2`; slot2 name "SqueezeFlingUpdate" |
| `0x00743770` | `SqueezeFlingUpdate::getModuleNameKey` | 4 | `00C225F0 slot 4`; slot2 name "SqueezeFlingUpdate" |
| `0x0074E7D0` | `SlaughterHordeContain::getModuleName` | 2 | `00C22E28 slot 2`; slot2 name "SlaughterHordeContain" |
| `0x0074E7E0` | `SlaughterHordeContain::getModuleNameKey` | 4 | `00C22E28 slot 4`; slot2 name "SlaughterHordeContain" |
| `0x0074EA60` | `GarrisonStealthContain::getModuleName` | 2 | `00C23218 slot 2`; slot2 name "GarrisonStealthContain" |
| `0x0074EA70` | `GarrisonStealthContain::getModuleNameKey` | 4 | `00C23218 slot 4`; slot2 name "GarrisonStealthContain" |
| `0x0074EAA0` | `HealContain::getModuleName` | 2 | `00C235F0 slot 2`; slot2 name "HealContain" |
| `0x0074EAB0` | `HealContain::getModuleNameKey` | 4 | `00C235F0 slot 4`; slot2 name "HealContain" |
| `0x0074FBE0` | `TunnelContain::getModuleName` | 2 | `00C23A98 slot 2`; slot2 name "TunnelContain" |
| `0x0074FBF0` | `TunnelContain::getModuleNameKey` | 4 | `00C23A98 slot 4`; slot2 name "TunnelContain" |
| `0x00751C00` | `AnimalAIUpdate::getModuleName` | 2 | `00C23D58 slot 2`; slot2 name "AnimalAIUpdate" |
| `0x00751C10` | `AnimalAIUpdate::getModuleNameKey` | 4 | `00C23D58 slot 4`; slot2 name "AnimalAIUpdate" |
| `0x00751C70` | `AssaultTransportAIUpdate::getModuleName` | 2 | `00C24180 slot 2`; slot2 name "AssaultTransportAIUpdate" |
| `0x00751C80` | `AssaultTransportAIUpdate::getModuleNameKey` | 4 | `00C24180 slot 4`; slot2 name "AssaultTransportAIUpdate" |
| `0x00751CC0` | `DeployStyleAIUpdate::getModuleName` | 2 | `00C245B0 slot 2`; slot2 name "DeployStyleAIUpdate" |
| `0x00751CD0` | `DeployStyleAIUpdate::getModuleNameKey` | 4 | `00C245B0 slot 4`; slot2 name "DeployStyleAIUpdate" |
| `0x00751DD0` | `DozerAIUpdate::getModuleName` | 2 | `00C24A50 slot 2`; slot2 name "DozerAIUpdate" |
| `0x00751DE0` | `DozerAIUpdate::getModuleNameKey` | 4 | `00C24A50 slot 4`; slot2 name "DozerAIUpdate" |
| `0x00751EA0` | `HordeAIUpdate::getModuleName` | 2 | `00C24EE0 slot 2`; slot2 name "HordeAIUpdate" |
| `0x00751EB0` | `HordeAIUpdate::getModuleNameKey` | 4 | `00C24EE0 slot 4`; slot2 name "HordeAIUpdate" |
| `0x00752330` | `SupplyTruckAIUpdate::getModuleName` | 2 | `00C25398 slot 2`; slot2 name "SupplyTruckAIUpdate" |
| `0x00752340` | `SupplyTruckAIUpdate::getModuleNameKey` | 4 | `00C25398 slot 4`; slot2 name "SupplyTruckAIUpdate" |
| `0x00752400` | `TransportAIUpdate::getModuleName` | 2 | `00C257C0 slot 2`; slot2 name "TransportAIUpdate" |
| `0x00752410` | `TransportAIUpdate::getModuleNameKey` | 4 | `00C257C0 slot 4`; slot2 name "TransportAIUpdate" |
| `0x00752440` | `WanderAIUpdate::getModuleName` | 2 | `00C25BE8 slot 2`; slot2 name "WanderAIUpdate" |
| `0x00752450` | `WanderAIUpdate::getModuleNameKey` | 4 | `00C25BE8 slot 4`; slot2 name "WanderAIUpdate" |
| `0x0075C870` | `HordeGarrisonContain::getModuleName` | 2 | `00C26980 slot 2`; slot2 name "HordeGarrisonContain" |
| `0x0075C880` | `HordeGarrisonContain::getModuleNameKey` | 4 | `00C26980 slot 4`; slot2 name "HordeGarrisonContain" |

## Differential Oracle and Semantic Mutation Check

- **Oracle script**: `Ra3Source/tools/analysis/ports/verify_gamelogic72.py`
- **Checker executable**: `ra3_gamelogic72check` (`Ra3Source/tools/gamelogic72check/main.cpp`)
- Name leaves: two calls with different `ecx` values; EAX is the shipped string address and the image string matches the slot 2 name.
- Name-key leaves: four cases (uninitialized, bit0 already set, upper guard bits preserved, full-width cached EAX). The checker supplies the `0x00986150` result; Unicorn binds that callee with `ret 4`.
- **Semantic mutation**: `ra3_gamelogic72check_mutant` compiles `ModuleIdentity72.cpp` with `RA3_GAMELOGIC72_ORACLE_MUTANT`, treating the guard as ready when bit 1 is set instead of bit 0. Already-initialized cases must disagree; the unmutated checker returns to 0 mismatches.

## Hook Targets (`GameLogic72Targets.cpp`)

The four hooks are `getModuleName` leaves. They ignore `this`, write nothing, and return the class-name pointer in EAX. `explicitOnly` is true. Stolen length is the 5-byte `mov eax, imm32`.

| Key | VA | Expected 16 Bytes | Stolen | explicitOnly |
|---|---|---|---:|---|
| `opencontainname` | `0x00727950` | `B8 30 FC C1 00 C3 CC CC CC CC CC CC CC CC CC CC` | 5 | `true` |
| `hordecontainname` | `0x0073CC60` | `B8 64 1B C2 00 C3 CC CC CC CC CC CC CC CC CC CC` | 5 | `true` |
| `aiupdateinterfacename` | `0x00740F10` | `B8 CC 23 C2 00 C3 CC CC CC CC CC CC CC CC CC CC` | 5 | `true` |
| `dozeraiupdatename` | `0x00751DD0` | `B8 04 4D C2 00 C3 CC CC CC CC CC CC CC CC CC CC` | 5 | `true` |

## Rejected Candidates

- **241** other clean methods on these same slot-2-named vtables, inside the address window: slot 0 deleting destructors, xfer/update bodies, and calls into pathing, contain, or AI helpers. They are not name or name-key leaves.
- **10** name-shaped or NAMEKEY-shaped bodies whose only `vtables_split.tsv` hit is not slot 2 or slot 4 of a vtable whose slot 2 returns that string (merged tables at slot 78/80; their slot 2 is `0x0065F220` or `0x006CE140`). Naming them from the merged table would attach the wrong class.
- **18** `EASTL` allocator instantiations in the window were skipped.
- Protected (`securom != clean`) bodies were not analyzed.

## Deferred Boundaries

- `0x00986150` (name-to-key) and the generator object at `[0x00E1A078]` stay outside this batch.
- Large `update` / `xfer` overrides on these vtables (for example `0x007432E0`, `0x0075E110`) call engine helpers and are left for a later behavior batch.
- The ten merged-slot name/key bodies (`0x0073C990`, `0x0073C9A0`, `0x0073CAD0`, `0x0073CAE0`, `0x0074E420`, `0x0074E430`, `0x0075CDC0`, `0x0075CDD0`, `0x0075EAC0`, `0x0075EAD0`) need a vtable split before they can be named.
