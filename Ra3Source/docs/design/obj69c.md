# Object-update 0x0069 third pass

## Scope and ownership

This batch keeps 41 clean, previously unanalyzed ledger entries in
`0x00690000`–`0x0069FFFF`. Each counted body is one slot of a split vtable.
`work/probe/verify_ownership.py` reports `fails=0` and
`work/probe/check_cited.py` reports `suspect=0` for
`work/obj69c_claims.txt`.

Slot 2 of a primary table is `mov eax, imm32; ret`. The class name is the
NUL-terminated string at that immediate. Secondary tables, whose slot 2 is a
shared `xor eax, eax; ret` or another non-name stub, are named by a
constructor that stores both the named primary table and the secondary table.
Every cited constructor is a ledger start whose bytes are preceded by `ret` or
`int3` padding, and both stores sit inside that function's ledger size:

| Constructor | Primary store | Secondary store |
| --- | --- | --- |
| `006B4770` | `006B479D` writes `SpecialPowerUpdateModule` `00C0B860` | `006B47BB` writes `00BF7C68` |
| `00751D20` | `00751D25` writes `DozerAIUpdate` `00C24A50` | `00751D5B` writes `00C24878` |
| `0074DFC0` | `0074DFD6` writes `GarrisonContain` `00C21E90` | `0074DFEA` writes `00C21BF0` at `+0x24` |
| `006E8ED0` | `006E8ED7` writes `ProductionUpdate` `00C19788` | `006E8EEB` writes `00C19580` at `+0x24` |
| `00701F40` | `00701F45` writes `SpecialAbilityUpdate` `00C142D8` | `00701F59` writes `00C14184` at `+0x24` |
| `006E2AB0` | `006E2AB3` writes `SpecialPowerDispatchSpecialPower` `00C19230` | `006E2AC0` writes `00C19058` at `+0x10` |
| `00740520` | `00740541` writes `DockUpdate` `00C0BD58` | `00740555` writes `00C0BBB0` at `+0x24` |
| `0073CB30` | `0073CB3B` writes `HordeContain` `00C21A98` | `0073CB8A` writes `00C21520` at `+0x16C` |

Bodies that sit in several vtables through ICF are counted once, under the
class cited in the claim file. No EASTL instantiation, compiler-generated
member copy, or factory/clone row is counted. Boilerplate of the capped
shapes (slot-2 getters, NAMEKEY caches, return-constant stubs, base-only
forwarders, single-field accessors) is 0 of 41. The four xfer bodies serialize
several members; `00690270` also branches on bit 1 of the xfer flags.

## Counted functions

| VA | C++ name | Ownership |
| --- | --- | --- |
| `00690240` | `unitVisionBehaviorStoreRange` | `00BFDB88` slot 23, slot 2 `UnitVisionBehavior` |
| `00690270` | `unitVisionBehaviorXfer` | `00BFDB88` slot 3, slot 2 `UnitVisionBehavior` |
| `006906E0` | `managerBodyCacheTemplate` | `00C124C8` slot 25, slot 2 `ManagerBody` |
| `006908E0` | `crateCollideAllows` | `00C0A598` slot 22, slot 2 `CrateCollide` |
| `00690A80` | `moneyCrateCollideGrant` | `00C19E10` slot 23, slot 2 `MoneyCrateCollide` |
| `006913D0` | `hordeContainSharesSide` | `00C21520` slot 27; HordeContain constructor `0073CB30` |
| `00691620` | `hordeContainPushTimer` | `00C21520` slot 126; same constructor |
| `006920B0` | `garrisonContainAccepts` | `00C21BF0` slot 37; GarrisonContain constructor `0074DFC0` |
| `00692460` | `garrisonContainAnchorOrSlot` | `00C21BF0` slot 82; same constructor |
| `00692490` | `garrisonContainLocatePoint` | `00C21BF0` slot 81; same constructor |
| `00692BA0` | `tunnelContainOpenIterator` | `00C23A98` slot 48, slot 2 `TunnelContain` |
| `00693FB0` | `detonateStateRequest` | `00BF79C0` slot 5, slot 2 `DetonateState` |
| `00695020` | `specialPowerDispatchApplyKind1` | `00C19058` slot 14; constructor `006E2AB0` |
| `006950D0` | `specialPowerDispatchApplyKind03` | `00C19058` slot 13; same constructor |
| `00695170` | `specialPowerDispatchApplyKind23` | `00C19058` slot 12; same constructor |
| `00695210` | `specialPowerDispatchApplyKind4` | `00C19058` slot 11; same constructor |
| `006953E0` | `specialPowerDispatchAdjustCount` | `00C19058` slot 10; same constructor |
| `00695720` | `specialPowerDispatchCopyKind` | `00C19058` slot 6; same constructor |
| `00695990` | `specialPowerModuleNoteReady` | `00C01108` slot 22, slot 2 `SpecialPowerModule` |
| `00696430` | `specialPowerModuleTrigger` | `00C01108` slot 10, slot 2 `SpecialPowerModule` |
| `00696740` | `specialPowerUpdateReadiness` | `00BF7C68` slot 3; constructor `006B4770` |
| `00696800` | `specialPowerUpdateNotifyOwner` | `00BF7C68` slot 5; same constructor |
| `00696BC0` | `specialPowerUpdateModuleXfer` | `00C0B860` slot 3, slot 2 `SpecialPowerUpdateModule` |
| `006975C0` | `dozerAiUpdateAcceptTask` | `00C24A50` slot 58, slot 2 `DozerAIUpdate` |
| `00697630` | `dozerAiUpdateTerrainScale` | `00C24878` slot 3; constructor `00751D20` |
| `00697B80` | `dockUpdateOccupiedScale` | `00C0BBB0` slot 23; constructor `00740520` |
| `006991B0` | `aiUpdateTouchLocomotor` | `00C22118` slot 172, slot 2 `AIUpdateInterface` |
| `00699C10` | `aiUpdateIssue61` | `00C22118` slot 56, slot 2 `AIUpdateInterface` |
| `00699C60` | `aiUpdateIssue51` | `00C22118` slot 65, slot 2 `AIUpdateInterface` |
| `00699CD0` | `aiUpdateIssue3A` | `00C22118` slot 64, slot 2 `AIUpdateInterface` |
| `00699D20` | `aiUpdateIssue32` | `00C22118` slot 63, slot 2 `AIUpdateInterface` |
| `00699D60` | `aiUpdateIssue40` | `00C22118` slot 72, slot 2 `AIUpdateInterface` |
| `00699DC0` | `aiUpdateIssue42` | `00C22118` slot 73, slot 2 `AIUpdateInterface` |
| `00699E20` | `aiUpdateIssue3F` | `00C22118` slot 75, slot 2 `AIUpdateInterface` |
| `00699E80` | `aiUpdateIssue43` | `00C22118` slot 76, slot 2 `AIUpdateInterface` |
| `00699EE0` | `aiUpdateIssue49` | `00C22118` slot 77, slot 2 `AIUpdateInterface` |
| `00699F60` | `aiUpdateIssue2A` | `00C22118` slot 98, slot 2 `AIUpdateInterface` |
| `0069E660` | `productionUpdateCancelOne` | `00C19580` slot 10; constructor `006E8ED0` |
| `0069F100` | `secondaryDamageTrackerXfer` | `00C05858` slot 3, slot 2 `SecondaryDamageTracker` |
| `0069FA90` | `specialAbilityUpdateRelay` | `00C14184` slot 6; constructor `00701F40` |
| `0069FF20` | `stealthDetectorUpdateXfer` | `00C0DFC0` slot 3, slot 2 `StealthDetectorUpdate` |

## What the bodies do

`AIUpdateInterface` keeps its state object at `this+0x3C`. The issue methods
clear that state (`virtual +0x34`), publish a goal or a packed locator, store
the stamp at `this+0x50`, and enter a fixed state id through `virtual +0x1C`.
`00699C10` uses `0x005AC4F0` with `FLT_MAX` (`0x7F7FFFFF` at `0x00BE700C`) and
`0x005AA2A0`. `00699D60` and `00699E20` do the same and then write `-1` at
`+0x308`, `+0x368`, and `+0x3C8`. `00699DC0` and `00699E80` call `0x005BDCC0`
instead of the float locator. `00699EE0` may also call `0x00844460` when the
owner's contained pointer is set, and still writes the three `-1`s.
`00699CD0` asks `0x00692390` first and leaves the state alone when that
returns zero. `00699F60` takes the clear path only when the target is null;
otherwise it calls `0x00799580` and `0x007995C0`. `006991B0` compares
`this+0x14` with the frame at `[0x00DEBA64]+0x58` and, when the gap is greater
than one and byte `+0x43B` is clear, calls `0x004ED9C0`.

`00695990` returns on the first of three gates: the embedded power at `+0x10`
answers `virtual +0x10`, byte `+0x40` is set, or bit 9 of the owner's flags is
set. The later template-bit and frame store are implemented. The offline stub
returns one slot, so a test cannot both clear the first gate and hand the
second virtual a pointer; that store is not in the oracle. `00696430` always
reads the descriptor from `virtual +0x1C`. Bit 14 clear either returns or
calls `virtual +0x44` with `1.0`. Bit 14 set calls `0x007EDDB0` (arguments
descriptor, 1) or, when byte `+0x1B9` is set, `0x007EDF50`, then may relay
through `virtual +0xA0`.

`006906E0` caches a template from `0x00795CF0` into `+0x114`/`+0x118`/`+0x3C`
and sets byte `+0x11C`. `006908E0` rejects a crate collide on a chain of
template and object bits, then returns 1 only when a dword in
`[this+0x18, this+0x1C)` equals the other's id. Bit 10 of the template word
at `+0xCC` rejects when it is set. `00690A80` always returns 1. A null wallet
still reads the crate field at `[this+4]+0x6C` and calls `0x0067F7C0`. A live
wallet also calls `0x00796EA0`, `0x007E4AB0` (seven stack arguments; the port
records the first six), and `0x007F58B0`.

`00692BA0` fills an out-iterator from the tunnel node, or from `this+0x24`
with the sentinel `0x00CC0E7C`, and calls that iterator's slot 0 with argument
1. `00690240` stores the packed range and key and the hash from `0x004A62D0`.
`00690270` versions the xfer, serializes the module and the fields at `+0x30`
and `+0x24`, and recomputes the hash when bit 1 of `[xfer+4]` is set.
`00693FB0` calls weapon-state `virtual +0x18` with 0 and returns `0xFFFFFFFF`.
`006975C0` accepts a dozer task only after `virtual +0x50`, `0x007E3720`, and
a frame/owner test against `[0x00DEBA64]+0x58`.

`00696740` returns `1.0`, the stored float at `+0x10`, or `0.0` on the three
early gates, and otherwise computes `1 - elapsed / divisor` with unsigned
`fild` and `0x007E2970`. `00696800` returns the `0x007EE070` result when bit
12 is set and an owner is attached; otherwise it returns
`frame - [this+0xC] + [this+4]`. `00697630` returns the terrain height, or
that height times the row scale when the contained node kind is 1.
`00697B80` counts occupied dock slots, adds one when `virtual +0x60` is
non-zero, and returns `(15 * rate) * count`. The 15 is the image dword at
`0x00CB0910`.

`006920B0` rejects on two anchor bits, a null other, `virtual +0xA4` of the
object at `this-0x24`, or template bit 28. A zero mode byte returns 1.
Otherwise it compares `virtual +0x10C` with `virtual +0x78`. `00692460` tail-
calls `virtual +0x144` when template bit 21 is set and otherwise returns
`anchor+0x38`. `00692490` calls `0x0063EB60` on that same bit and returns
`0x00DF8DE8` when the call answers non-zero. The one-shot writes to
`0x00DF8DF4` and `0x00DF8DE8` are image side effects; the oracle compares the
pointer result.

`0069E660` walks the production list at `+0x10` and marks one matching record.
The image dword `0x00DEEA44` is zero, so the notify arm is not taken.
`0069FA90` returns 0 when `0x0069F9E0` does, and otherwise returns whether
`[body+0xD8]` is zero when bit 21 is clear. The list-walk arm is implemented
and covered only on the early exits. The six dispatch methods walk
`[anchor+0x1C4, +count)` and call `0x00687720`. A null result aborts
`006953E0` and skips an element in the kind filters. `006953E0` then adjusts
the counter at `+0x24`. The engage-from-zero arm calls `virtual +0xC` and
`fstp`s the float; the offline stub leaves ST0 empty, so that arm is not
entered. `006913D0` returns 0 when argument 1 is non-zero, when a side bit is
set and argument 3 is zero, or when the two `0x00B11D40` results differ. A
taken `0x00B11D40` replaces `cl` with the low byte of the gate argument, which
is zero on the continuing path. `00691620` returns immediately on a null
anchor and otherwise stamps byte `+8` and calls `0x004ED9C0`.

`0069FF20`, `00696BC0`, and `0069F100` are xfer bodies. Each writes a version
pair through `virtual +0x18` and then serializes members. `0069F100`'s first
pair is bytes 1 and 2, and the later `cmp byte [esp+9], 2` therefore calls
`0x0054A090`.

## Dropped candidates

- Slot-0 deleting destructors in the range (`00690E00`, `00692FB0`, `00693260`, `00693F80`, `00693FD0`, `00694640`, `006972E0`, `00697460`, `00697900`, `00698F10`, `0069C830`, `0069CC40`, `0069E880`, `0069F750`, `0069FFD0`): free or scalar-deleting paths.
- NAMEKEY slot-4 caches (`00690090`, `00690210`, `00693070`, `00695610`, `0069F0D0` and the same shape): test a byte, call `0x00986150`, return the cached key.
- `0069A540`, `00694ED0`, `0069CCA0`, `00694330`: xfer forwarders. The first is a tail jump to `0x0043C3FC`; the others add a base helper and no local branch.
- `00692540`: falls off the end with no `ret`.
- `00694850`: `fistp` plus an unconditional use of `ecx` on the null-singleton path.
- `00694140`: `MoveToGroundState` always loads `0x00DFC190` and is x87-heavy.
- `006976B0`: the only safe exit is a null argument; the other arm loads `0x00DF03FC`.
- Source-path anchors `0069A410`, `0069A820`, `0069AD10`, `0069D000`, `0069C6F0`, `0069DBA0`, `00690B30`, `0069F180`, `00697F70`: their in-range callees are not keepable, and several of the anchors themselves have no clean vtable ownership.
- EASTL, allocator, and compiler-generated member copies: excluded by the batch rule.

## Oracle

`obj69c oracle: 41 entries, 119 cases, 0 mismatches`

Direct callees are queued on both sides. Image singletons that the body
dereferences (`0x00DEBA64`, `0x00DFBD74`, `0x00DFB9D8`) are planted with a
`g:ADDR=OFF` token after `reset_data`. `0x00CB0910` stays the image value 15.
`006913D0` is `ret 0x10`, so the checker line carries a fourth stack argument.

A one-line mutation of `aiUpdateIssue51`, state id `0x51` changed to `0x52`,
failed the oracle at blob `+0x118` (`game=0x51 cpp=0x52`). Restoring the
constant returned `0 mismatches`.

## Hooks

No hooks. The optional leaf-hook table is empty.

| Key | Target | explicitOnly |
| --- | --- | --- |
