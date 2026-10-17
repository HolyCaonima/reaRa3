# Object module slots (`recon/obj6a`)

## Scope

Sixteen clean functions in `0x006A0000`–`0x006BFFFF` from `ra3_1.0.game`. Each counted function is a slot of a vtable whose slot 2 is `mov eax, imm32; ret` and whose immediate is the class-name string. The implementation is `ra3::gamelogic` in `ObjectCore6A.h` / `ObjectCore6A.cpp`. The oracle is `tools/analysis/ports/verify_obj6a.py` against `ra3_obj6acheck`. There are no hooks.

The earlier draft of Object, WeaponSet, and experience helpers in this window was not a set of class-name slots and did not match full-width EAX. Those addresses are deferred in `obj6a_annotations.tsv`. `0x006A16A0` is the one draft address that is a real `WeaponSetUpdate` slot and is counted.

Generals headers were not used to name these methods. The class name is the slot-2 string. Slot 3 of these module vtables is the xfer body: it passes a two-byte version `{1, 1}` to `vtable+0x18` and then xfers fields. Slot 1 of `WeaponSetUpdate` calls `vtable+0x1C` on each weapon pointer.

## Ownership

Every counted row cites one vtable from `vtables_split.tsv`. The anchor for that vtable is its slot-2 class-name string, read from the bytes at the immediate in the slot-2 function. `WeaponSetUpdate`'s vtable is also stored by `00743F66` (`mov dword ptr [edi], 0x00C1F320`) after the base call `004D6790`.

| VA | Size | Class | Slot | Function | Cases |
| --- | --- | --- | --- | --- | --- |
| `0x006A23A0` | 56 | `EncloseBehavior` | `00C08A48` slot 30 | `encloseContainsId` | 10 |
| `0x006A16A0` | 35 | `WeaponSetUpdate` | `00C1F320` slot 1 | `weaponSetCallSlot1C` | 6 |
| `0x006B13F0` | 26 | `SlaughterHordeContain` | `00C22E28` slot 32 | `slaughterContainedFlag` | 4 |
| `0x006AED50` | 47 | `ClearanceTestingSlowDeathBehavior` | `00C1AC90` slot 3 | `clearanceSlowDeathXfer` | 2 |
| `0x006A1C80` | 47 | `TechUpgrade` | `00C10AA0` slot 3 | `techUpgradeXfer` | 2 |
| `0x006B50A0` | 54 | `DozerActionMoveToActionPosState` | `00BF7DD0` slot 3 | `dozerMoveToXfer` | 2 |
| `0x006B5130` | 57 | `DozerActionStateMachine` | `00BF7EA8` slot 3 | `dozerStateMachineXfer` | 2 |
| `0x006B5050` | 67 | `DozerActionPickActionPosState` | `00BF7D60` slot 3 | `dozerPickXfer` | 2 |
| `0x006B50E0` | 67 | `DozerActionDoActionState` | `00BF7E40` slot 3 | `dozerDoXfer` | 2 |
| `0x006B5170` | 67 | `DozerActionState` | `00BF7F70` slot 3 | `dozerActionXfer` | 3 |
| `0x006BC4A0` | 73 | `SubObjectsUpgrade` | `00C1F4E8` slot 3 | `subObjectsUpgradeXfer` | 2 |
| `0x006A3E90` | 76 | `ExperienceTrackerObject` | `00C07BA4` slot 3 | `experienceTrackerObjectXfer` | 2 |
| `0x006A1AA0` | 77 | `AudioLoopUpgrade` | `00BF9318` slot 3 | `audioLoopUpgradeXfer` | 2 |
| `0x006B37C0` | 84 | `CreateAndEnterObjectSpecialPower` | `00C1A7D8` slot 3 | `createAndEnterXfer` | 2 |
| `0x006AFCB0` | 103 | `VeterancyCrateCollide` | `00C1A4E0` slot 22 | `veterancyCrateCanPass` | 9 |
| `0x006A2220` | 110 | `TechUpgradeReceiver` | `00BF9580` slot 3 | `techUpgradeReceiverXfer` | 3 |

`0x006A23A0` is also installed on other named vtables, including `SupplyWarehouseDockUpdate` `00C0C5A8` slot 30 and `WeaponScatterEffectUpdate` `00C0E5D0` slot 30. It is one body and is counted once, under the `EncloseBehavior` slot.

NameKey caches (`mov eax, 1; test byte ptr [flag], al; call 00986150`) and deleting destructors (`call 004166B0`) are 0 of the 16. That is under the 30% boilerplate cap.

## Layouts

`encloseContainsId` reads `owner = *(iface - 0x0C)`, then the dword count at `owner+0x0C` and the pointer at `owner+0x10`. EAX is that pointer with AL replaced by 1 when the needle is present. The list is not written.

`weaponSetCallSlot1C` walks `[this+0x24, this+0x28)` as pointers and calls `vtable+0x1C` with no stack arguments. An empty range leaves the incoming EAX unchanged. Otherwise EAX is the last call's EAX.

`slaughterContainedFlag` calls `006B0BD0(this+0x760, originalThis, argument)` and folds EAX with `neg / sbb / neg` to 0 or 1.

Xfer bodies pass `{1, 1}` to `vtable+0x18`. Later calls, and the EAX of the last one:

| Function | After the version call |
| --- | --- |
| `clearanceSlowDeathXfer` | `0068ECD0(this, xfer)` |
| `techUpgradeXfer` | `006858E0(this, xfer)` |
| `dozerMoveToXfer` | `vtable+0x14(this+0x20, 4)` |
| `dozerStateMachineXfer` | `vtable+0x14(this+0x80, 4)` |
| `dozerPickXfer` | `vtable+0x14(this+0x20, 4)`, then `vtable+0x70(this+0x24)` |
| `dozerDoXfer` | `vtable+0x14(this+0x20, 4)`, then `vtable+0x6C(this+0x24)` |
| `dozerActionXfer` | `vtable+0x14(this+0x20, 4)`, then `vtable+0x20(*(this+0x24))` |
| `subObjectsUpgradeXfer` | `vtable+0x6C(this+0x28)`, `006A1EA0(this, xfer, &version)`, `006858E0(this, xfer)` |
| `experienceTrackerObjectXfer` | `006A3D10(this, xfer)`, cdecl `0054A090(xfer, this+0x3C)`, `vtable+0x84(this+0x40)` |
| `audioLoopUpgradeXfer` | `(*[0x00DF03FC] vtable+0x158)(xfer, this+0x30)`, `006858E0(this, xfer)`, `00685080(this+0x18, xfer)` |
| `createAndEnterXfer` | a second version call, `vtable+0x6C(this+0x50)`, `00681220(this, xfer)` |
| `techUpgradeReceiverXfer` | `006858E0`, cdecl `0054A090(xfer, this+0x30)`. If bit 2 of `*(xfer+4)` is clear, also `vtable+0x64(this+0x38)`, `vtable+0x64(this+0x34)`, `vtable+0x70(this+0x3C)` |

`veterancyCrateCanPass` calls `006908E0(this, object)`. A zero AL returns that EAX with AL cleared. Flag `object+0x471` bit 0 does the same. Otherwise EAX becomes `*(object+0x80) >> 7`; bit 0 of that shift, or a non-zero byte at `template+0x74`, clears AL. A zero byte at `template+0x75` sets AL. Otherwise a different team id at `object+0x418` and `[this+8]+0x418` returns the object team with AL cleared. The same team calls `006875A0(object)` and sets AL from that result. None of these functions write the module image. The oracle watches the module, the xfer object, and the list or weapon array.

## Oracle and mutation

`verify_obj6a.py` emulates each body with Unicorn, stubs the callees, and compares full EAX, stack cleanup, the callback trace, and an empty write set against `ra3_obj6acheck`. Loops use 6 to 10 cases. The veterancy predicate uses 9 branch cases. Straight xfer bodies use 2 or 3 return values. Release only.

Changing `dozerMoveToXfer` so its trace recorded `this+0x24` instead of `this+0x20` made the Release oracle report 2 mismatches (`dmove a` and `dmove b`: native `v14:20`, checker `v14:24`). Restoring the offset returned `mismatches=0`.

## Hooks

No hook keys. The xfer bodies pass an xfer object and call other module code, so they are not stolen as leaf hooks.

## Deferred

The vtables installed at object offsets `0x00`, `0x60`, `0x64`, `0x6C`, `0x74`, and `0x78` by `0076B240` (`00C27558`, `00C27548`, `00C27530`, `00C27514`, `00C274EC`, `00C274B0`) do not have a slot-2 class-name string, and `0076B240` has no assertion string. Their slots in this window stay deferred. `00DFC190` terrain reads (`006A5BC0`, `006AC110`) and the null-dereference path in `StrafeAreaUpdate` slot 25 (`006BAB00`) were not matched. NameKey caches and deleting destructors were not counted. SecuROM bodies were not analyzed.
