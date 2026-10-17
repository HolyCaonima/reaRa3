# Weapon and object update logic cluster

This reconstruction covers 90 clean GameLogic entries in
`ra3_1.0.game` `0x0069A250-0x0069FD30`. It consists of 30 stateful
module-key routines, their 30 exact name getters, and 30 weapon/object update
behaviors. The nontrivial count is 56: 30 guarded key resolvers plus 26
substantive target, transform, lifetime, physics, shield, and slaved behaviors.
The other 34 entries are complete-class companions: 30 name getters and four
pure accessors (`0069A710`, `0069E400`, `0069FC50`, and `0069FD30`).

The implementation is in `src/gamelogic` (`ra3/gamelogic/UpdateModules.h`,
`src/UpdateModules.cpp`); its raw-layout edges use `memcpy`
loads/stores so the framework API remains alignment and strict-aliasing safe.
Names follow RA3 strings and, where noted in source comments, the structural
concepts in these GPL reference files:

- `work/references/generals/GeneralsMD/Code/GameEngine/Source/GameLogic/Object/Weapon.cpp`
- `work/references/generals/GeneralsMD/Code/GameEngine/Source/GameLogic/Object/Update/LifetimeUpdate.cpp`
- `work/references/generals/GeneralsMD/Code/GameEngine/Source/GameLogic/Object/Update/PhysicsUpdate.cpp`
- `work/references/generals/GeneralsMD/Code/GameEngine/Source/GameLogic/Object/Update/SlavedUpdate.cpp`

The code is an original RA3 implementation. Generals supplies names and
structural expectations only; field offsets, arithmetic order, upper-register
residue, and control flow come from the shipped RA3 code and differential
execution.

## Ownership evidence

Address adjacency was not used as ownership evidence.

- The 30 metadata pairs return the listed literal and call the common
  `0x00986150` resolver under a dedicated guard/cache pair. Assertion strings
  independently contain the same update-module class names.
- `AttachUpdate` constructor `0x0069A290` installs vtable `0x00C01E40`;
  `LargeGroupBonusUpdate` constructor `0x0069C6F0` installs `0x00C042D8`;
  `SetLocomotorSpeedUpdate` constructor `0x0069F180` installs `0x00C05A88`.
- `LifetimeUpdate` table `0x00BF89A0` owns `0x0069CC40`,
  `0x0069CEF0`, and `0x0069CF20`. The scheduling entry calls
  `0x0069CD30` directly and carries the exact
  `.../object/Update/LifetimeUpdate.cpp` assertion path.
- The PhysicsUpdate family is tied by the
  `.../object/Update/PhysicsUpdate.cpp` assertion path, direct calls from
  `0x00714E30`, `0x007311D0`, `0x00762080`, and `0x00763BC0`, and
  vtable `0x00C19580` (including overlap count at slot 23).
- The weapon filters at `0x0069A610/40` share direct callers
  `0x00741720` and `0x00752D30`; `0x0069A710` is called by the
  weapon path at `0x007C9730`.
- Shield intersection `0x0069F450` is reached by `0x007178D0` and
  `0x00717D50` and is tied to the `ShieldBodyUpdate` metadata pair.
- SlavedUpdate ownership combines its metadata pair, the direct
  `0x0069F9E0 -> 0x0069FA90` edge, direct callers at `0x006E9B80`,
  `0x00718620`, `0x005252B0`, and `0x007AA550`, and object vtable
  `0x00C05CB4`.

## Accepted metadata pairs

Each row represents two accepted entries: an exact name getter and a guarded
module-key resolver.

| Class | Name VA | Key VA |
|---|---:|---:|
| AssistedTargetingUpdate | `0069A250` | `0069A260` |
| AttachUpdate | `0069A3C0` | `0069A3D0` |
| BoredUpdate | `0069AA10` | `0069AA20` |
| ChargeWeaponUpdate | `0069AB60` | `0069AB70` |
| DeletionUpdate | `0069AE80` | `0069AE90` |
| DemoTrapUpdate | `0069AF60` | `0069AF70` |
| DestroyObjectsUpdate | `0069B040` | `0069B050` |
| DestructibleEnvironmentUpdate | `0069B0C0` | `0069B0D0` |
| DieOnProducerDeathUpdate | `0069B2E0` | `0069B2F0` |
| EMPUpdate | `0069B480` | `0069B490` |
| FireSpreadUpdate | `0069B560` | `0069B570` |
| FlammableUpdate | `0069B630` | `0069B640` |
| FreefallUpdate | `0069B830` | `0069B840` |
| GatherSlavesUpdate | `0069B9E0` | `0069B9F0` |
| GrapplingHookUpdate | `0069BAF0` | `0069BB00` |
| HeightDieUpdate | `0069BC80` | `0069BC90` |
| HijackerUpdate | `0069BDC0` | `0069BDD0` |
| HordeNotifyTargetsOfImminentProbableCrushingUpdate | `0069BEE0` | `0069BEF0` |
| InitiateRepairDie | `0069BF90` | `0069BFA0` |
| InvisibilityBroadcastUpdate | `0069C170` | `0069C180` |
| InvisibilityUpdate | `0069C4B0` | `0069C4C0` |
| LargeGroupBonusUpdate | `0069C7F0` | `0069C800` |
| LaserScatterUpdate | `0069CA10` | `0069CA20` |
| LoseHealthUpdate | `0069D1D0` | `0069D1E0` |
| PowerUpdate | `0069E2D0` | `0069E2E0` |
| ProneUpdate | `0069EA50` | `0069EA60` |
| RadarUpdate | `0069EB30` | `0069EB40` |
| RepairUpdate | `0069ECE0` | `0069ECF0` |
| SetLocomotorSpeedUpdate | `0069F270` | `0069F280` |
| SlavedUpdate | `0069F840` | `0069F850` |

## Accepted behavior entries

| VA | Framework function | Recovered behavior |
|---:|---|---|
| `0069A610` | `rejectDifferentControllingPlayer` | weapon controlling-player filter, including upper EAX |
| `0069A640` | `weaponKindMaskMatches` | seven-word source exclusion and target requirement masks |
| `0069A710` | `weaponUsesLineOfSight` | weapon-template flag 17 |
| `0069ABA0` | `chargeWeaponProgress` | unsigned elapsed-frame ratio, clamped to `[0,1]`, x87 return |
| `0069AC90` | `clusterBombAcceptsTarget` | prior-target, flag, and position filter |
| `0069B100` | `transformDestructiblePoints` | two points transformed by a 3x4 matrix in shipped SSE order |
| `0069B930` | `freefallContainsTarget` | nested range/object-ID search |
| `0069CA50` | `laserScatterInitialize` | flags, wake deadline, and drawable-status initialization |
| `0069CD30` | `lifetimeSecondaryDeadline` | delayed/tick deadline with x87 truncation and tick cap |
| `0069CE80` | `lifetimeStart` | range scheduling entry; native continuation starts at `0069CE8A` |
| `0069CEF0` | `lifetimePause` | pause flag and next-frame wake |
| `0069CF20` | `lifetimeResume` | earliest remaining deadline and wake |
| `0069D0F0` | `loseHealthPulseCount` | positive ratio ceil capped by requested pulses |
| `0069E0D0` | `physicsSetCollisionFlag` | collision mask callback/set |
| `0069E120` | `physicsClearCollisionFlag` | collision mask callback/clear |
| `0069E190` | `physicsCaptureTransforms` | two 48-byte x87 transform copies and valid flag |
| `0069E3C0` | `physicsScaleForGroup` | clamped count, inverse mass, and scaled frames |
| `0069E400` | `objectStatusBit9` | object status flag |
| `0069E420` | `physicsCopyControllingPlayerId` | nested controller field copy |
| `0069E820` | `physicsCountOverlaps` | nonignored owner-value list count |
| `0069E850` | `physicsHasControllerAtHeight` | object/controller existence and height threshold |
| `0069E8E0` | `powerUpdateWakeFrame` | powered/idle wake deadline |
| `0069F450` | `shieldSegmentIntersection` | sphere/segment intersection and exact hit point |
| `0069F760` | `clearObjectSlaveLinks` | clears object fields `+0x12F4/+0x34C` |
| `0069F770` | `slavedPinnedPosition` | master position plus optional configured offset |
| `0069F8D0` | `slavedPointOutsideGuardArea` | point-list/radius proximity scan |
| `0069F9E0` | `slavedStateAllowed` | multi-status predicate with path-dependent upper EAX |
| `0069FC50` | `slavedConfigurationBit22` | configuration flag |
| `0069FC60` | `slavedHasRider` | rider-module/configuration predicate |
| `0069FD30` | `slavedCopyTemplateFlags` | template field copy |

## Verification contract

`tools/weaponlogiccheck` is the native side of
`tools/analysis/ports/verify_weapon_logic.py`. A normal 50-case run performs
1,890 comparisons over all 90 accepted entries:

- exact callee cleanup and full EAX, including path-dependent upper 24 bits;
- EDX preservation for leaf entries where the shipped body preserves it;
- whole prefilled-object comparison for exact write ranges;
- overlapping input/output cases for destructible transforms, shield
  intersections, and both SlavedUpdate position sources;
- pointer relocation normalization for linked/range return residue;
- native x87 SNaN quieting for transform copies;
- all ten ST0 bytes for ChargeWeaponUpdate under control words
  `007F`, `027F`, and `037F`;
- shipped SSE operation order for transforms, scaling, proximity, and shield
  geometry;
- resolver call count/name and exact guard/cache writes for every key routine.

The checker exposes `--mutated`. The oracle's `--expect-mutation` run must fail
the differential comparison; this proves that the harness detects a deliberate
wrong result rather than merely exercising both paths.

## Hooks

`WeaponLogicTargets.cpp` contains 11 hooks, all `explicitOnly=true`:

`weapontargetplayer`, `weaponkindmask`, `weaponlineofsight`,
`objectstatusbit9`, `physicsoverlapcount`, `physicscontrollerheight`,
`objectclearslavelinks`, `slavedguardarea`, `slavedstateallowed`,
`slavedconfigurationbit22`, and `slavedhasrider`.

They are allocation-free, do not destroy objects, do not write globals, and do
not touch threads. The x87-returning `0x0069ABA0` is intentionally not hooked:
a normal C++ wrapper cannot preserve all ten ST0 bytes, and adding a naked
entry without live-process validation would not meet the replacement bar.

## Deferred entries

No VM/DRM implementation was inferred. The following protected entries in the
interval remain unknown:

- unresolved bodies: `00690660`, `00691D30`, `00697BF0`, `006992C0`,
  `0069A030`, `0069C530`, `0069E170`;
- resolved cave/site fragments that are not complete standalone bodies:
  `00695550` (site `00695566`), `00699780`, and `0069E780`
  (site `0069E78E`).

The `0x0069CE80` continuation dispatcher is not analyzed. Verification enters
the already-native `0x0069CE8A` continuation with the two stack words supplied
by the clean entry shim; no SecuROM section or VM code is mapped or interpreted.
