# Object modules `0x006C0000–0x006DFFFF`

42 clean ledger functions. Each one is a slot of a split vtable whose slot 2
is `mov eax, imm32; ret`. Class names were read from that string.
`verify_ownership.py` printed `fails=0` for `work/obj6c_claims.txt`. Shared
bodies are counted once and named after HighlanderBody, the first table cited.
No hooks.

## Classes

| Vtable | Slot 2 string | String VA |
| --- | --- | --- |
| `00C22118` | AIUpdateInterface | `00C223CC` |
| `00C11840` | StancesBehavior | `00BF700C` |
| `00C11D28` | HighlanderBody | `00C11D98` |
| `00C11FB0` | ImmortalBody | `00C12020` |
| `00C12238` | ManagedActiveBody | `00C122A8` |
| `00C124C8` | ManagerBody | `00C12538` |
| `00C1B3F8` | AttributeCrateCollide | `00BF7210` |
| `00C1E010` | ToggleDeploySpecialAbilityUpdate | `00C1E0C4` |
| `00C21A98` | HordeContain | `00C21B64` |
| `00C1FB90` | OpenContain | `00C1FC30` |
| `00C11230` | PassiveAreaEffectBehavior | `00C11298` |
| `00BFD820` | UnifiedControlBehavior | `00BFD880` |
| `00C11688` | SpawnBehavior | `00C116EC` |

`006C84E0`, `006C85E0` and `006D9AC0` also sit at the same slot on ImmortalBody,
ManagedActiveBody and ManagerBody. `006C9CC0` is cited only on OpenContain
`00C1FB90` slot 23.

## Inventory

| VA | Size | Slot | Function |
| --- | --- | --- | --- |
| `006C72E0` | 175 | PassiveAreaEffectBehavior 25 | `PassiveAreaEffectBehavior_apply` |
| `006C8450` | 23 | StancesBehavior 50 | `StancesBehavior_latchGoal` |
| `006C84E0` | 16 | HighlanderBody 23 | `HighlanderBody_readIfUnset` |
| `006C8500` | 42 | StancesBehavior 45 | `StancesBehavior_setBit` |
| `006C8530` | 44 | StancesBehavior 46 | `StancesBehavior_clearBit` |
| `006C85E0` | 109 | HighlanderBody 22 | `HighlanderBody_applyTemplate` |
| `006C89B0` | 48 | AttributeCrateCollide 23 | `AttributeCrateCollide_onCollide` |
| `006C8E90` | 138 | HordeContain 50 | `HordeContain_removeMember` |
| `006C9CC0` | 104 | OpenContain 23 | `OpenContain_removeByKey` |
| `006CB3C0` | 105 | ToggleDeploySpecialAbilityUpdate 44 | `ToggleDeploySpecialAbilityUpdate_tryDeploy` |
| `006CC0C0` | 129 | AIUpdateInterface 26 | `AIUpdateInterface_followPosition` |
| `006CC150` | 130 | AIUpdateInterface 28 | `AIUpdateInterface_followObject` |
| `006CC4D0` | 85 | AIUpdateInterface 36 | `AIUpdateInterface_steerState1B` |
| `006CC530` | 50 | AIUpdateInterface 60 | `AIUpdateInterface_tryCommand59` |
| `006CC570` | 50 | AIUpdateInterface 61 | `AIUpdateInterface_tryCommand5A` |
| `006CC5B0` | 178 | AIUpdateInterface 62 | `AIUpdateInterface_weaponOrFollow` |
| `006CC670` | 63 | AIUpdateInterface 67 | `AIUpdateInterface_locoState0E` |
| `006CC6B0` | 71 | AIUpdateInterface 69 | `AIUpdateInterface_placeState2D` |
| `006CC700` | 63 | AIUpdateInterface 68 | `AIUpdateInterface_locoState2D` |
| `006CC740` | 63 | AIUpdateInterface 70 | `AIUpdateInterface_locoState69` |
| `006CC780` | 106 | AIUpdateInterface 78 | `AIUpdateInterface_notifyPassenger` |
| `006CC7F0` | 61 | AIUpdateInterface 93 | `AIUpdateInterface_aimState11` |
| `006CC830` | 55 | AIUpdateInterface 94 | `AIUpdateInterface_tailState29` |
| `006CC870` | 55 | AIUpdateInterface 95 | `AIUpdateInterface_tailState48` |
| `006CC8B0` | 61 | AIUpdateInterface 96 | `AIUpdateInterface_aimState12` |
| `006CC8F0` | 63 | AIUpdateInterface 97 | `AIUpdateInterface_locoState13` |
| `006CC930` | 63 | AIUpdateInterface 101 | `AIUpdateInterface_locoState14` |
| `006CC970` | 282 | AIUpdateInterface 81 | `AIUpdateInterface_guardRect` |
| `006CCA90` | 76 | AIUpdateInterface 80 | `AIUpdateInterface_state18` |
| `006CCAE0` | 107 | AIUpdateInterface 82 | `AIUpdateInterface_followActor50` |
| `006CCB50` | 115 | AIUpdateInterface 83 | `AIUpdateInterface_followPoint50` |
| `006CCBD0` | 229 | AIUpdateInterface 85 | `AIUpdateInterface_guardPoint4F` |
| `006CCCC0` | 127 | AIUpdateInterface 84 | `AIUpdateInterface_weaponMove3C` |
| `006CCD40` | 144 | AIUpdateInterface 90 | `AIUpdateInterface_matchRoster` |
| `006CCDD0` | 112 | AIUpdateInterface 91 | `AIUpdateInterface_walkRoster` |
| `006CCE40` | 63 | AIUpdateInterface 102 | `AIUpdateInterface_locoState38` |
| `006D7CB0` | 269 | SpawnBehavior 9 | `SpawnBehavior_update` |
| `006D9540` | 30 | UnifiedControlBehavior 9 | `UnifiedControlBehavior_relay` |
| `006D9A40` | 90 | StancesBehavior 74 | `StancesBehavior_setLinkedFlag` |
| `006D9AA0` | 29 | StancesBehavior 24 | `StancesBehavior_setLinkedByte` |
| `006D9AC0` | 185 | HighlanderBody 26 | `HighlanderBody_noteHit` |
| `006D9B80` | 155 | StancesBehavior 30 | `StancesBehavior_releaseRecord` |

Boilerplate: 0 of 42. Same-shape factories: 0. The bit setters shift an index,
the latch and the unset-read test a flag byte, and the linked-flag writer
updates a primary-object bit and sometimes notifies. None of those is a
single-field accessor.

## What the bodies do

AIUpdateInterface command wrappers share one locomotor at `self+0x3C`. A
usable check on the object (`006C0ED0`, low byte) returns that full EAX when
it is zero. Bit 26 of `[template+0xC0]` blocks several commands; most of those
exits return the template pointer, while `followActor50` returns 1 because it
sets EAX to 1 before the test. `guardRect` probes the world object at
`[0x00DFC190]` and replaces the point only when the strict `comiss`/`ja` test
fails. `weaponOrFollow` tail-calls chain slot `0x80` when slot `0x84` returns
a child, otherwise it builds a two-dword record whose first dword is the
immediate `0x00BFDC10`.

`setLinkedFlag` shifts the primary object's flags at `+0x98` by 20
(`shr eax, 0x14`, mask `0x00100000`). `and al, 1` and `not al; and al, 1`
leave the upper bits of that shift in EAX. `setLinkedByte` uses `setne al`,
which also leaves the upper bits, and always notifies. `releaseRecord` unlinks
a circular node at `self+0xD4` and, when the list is empty, clears bit 14 of
`[primary+0x94]`.

`applyTemplate` branches on the state at `self+0x44` (1 applies the key
negative then the extra once; 2 does the reverse; any other state applies both
once). `noteHit` returns the stamp when there is no body, or when the stamp
matches and the stored time is still above the clock. `tryDeploy` pushes the
constant at `0x00BE700C` as a float, and a null passenger returns the object
pointer that was reloaded from the argument, not the ready call. `removeByKey`
locks the immediate `0x00DF8A30` and tail-jumps LeaveCriticalSection.
`PassiveAreaEffectBehavior_apply` multiplies with the x87 chop control
(`or cw, 0xC00`) before scheduling. `SpawnBehavior_update` walks `self+0x5C`
and looks the key up in the clock map at `+0xC4`.

## Dropped

- `006C8470`, `006C8480`, `006C8490`, `006C84A0`, `006C84B0`, `006C84C0`,
  `006C84D0`: one byte load or one byte store.
- `006C84F0`: stores 1 at `+0x11F` and returns.
- `006CE720`: `xor al, al; ret 8`. `006CF350`: `fldz; ret 4`.
  `006CF360`: stores 0 at `+0x34`.
- `006CB200` and `006CE140`: `lea` this-adjust thunks with no other behaviour.
- `006CC1E0` and `006CC260`: `fpatan`. Unicorn and the CPU do not agree on the
  80-bit result, so they are not in the oracle.
- `006C89E0` (520), `006C9DB0` (405), `006CC2E0` (482), `006DA7D0` (541),
  `006DEC60` (419): larger than this batch's checked bodies.
- `006CA840`: NAMEKEY call `00986150` plus further x87, left for a later pass.
- Secondary-interface slots whose slot 2 is the shared `ret` were not named.
  No constructor that stores both tables was cited.

## Oracle

`tools/obj6ccheck` and `tools/analysis/ports/verify_obj6c.py` compare log,
memory, a normalized return tag, and an extra text field. Raw EAX is not
compared. Release result after restoring the mutation:

`obj6c oracle mismatches=0 cases=156`

Semantic mutation: `kStanceFlagShift` in `ObjectModules6C.h` was changed from
20 to 21. The five `linkflag` cases mismatched on the flag dword at `+0x98`,
the notify log, and the return tag (`mismatches=5`). The constant was restored
and the oracle returned to 0 mismatches. Debug `ra3_obj6ccheck` builds with no
new warnings. Hooks: none.
