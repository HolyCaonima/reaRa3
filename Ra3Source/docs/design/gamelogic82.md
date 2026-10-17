# gamelogic82 named-object script queries

51 clean functions in `0x00838D80`-`0x0083FF90`. Each one looks a named object up through `0x00837420` with `ecx` loaded from the dword at `0x00DFC058`, then branches on the object's fields, writes a flag or tracker slot, or tail-calls a helper with arguments it just built. None of them is a `getModuleName` or NAMEKEY slot. Boilerplate count is 0. Semantic count is 51.

`0x00837420` itself is stubbed by the oracle and is not counted. It is the direct callee of the anchor at `0x0083E52C`.

## Anchor

`0x0083E520` is `ScriptConditions::evaluateNamedUnitRankLevel`. It is not a constructor: it never writes a vtable. The assertion text at `0x00C4E740`, read from the binary, is `ScriptConditions::evaluateNamedUnitRankLevel: Unit not found: `. The function pushes that address at `0x0083E53D` and calls `0x00837420` at `0x0083E52C` with `ecx = [0x00DFC058]`.

On a hit it reads the tracker at object `+0x3CC`, compares the signed rank at tracker `+0x24` with the signed limit at `[arg1+8]`, and returns 0 or 1 from `xor eax, eax` / `setge al`. On a miss it either returns immediately when byte `0x00DFC164` is already set, or builds the debug line and clears that byte's once-flag.

Every other counted function is tied to this anchor by `calls 00837420 at <site> with the same this`: the lookup's `ecx` is the same engine pointer. `0083CDD0` is a thiscall; it saves the incoming `this` and still performs the lookup on `[0x00DFC058]`.

Ownership edges (controller, checked with `work/probe/verify_ownership.py`, `fails=0`): sharing a callee is not an ownership edge, so each counted function is additionally tied to a vtable method that calls it directly. All 51 are direct callees of one of two script dispatchers:

- `0x0084E110` (vtable `0x00C35F30` slot 14): the action dispatcher.
- `0x008477A0`, called from `0x0084C460` at `0x0084C466`, which is vtable `0x00C48898` slot 14: the condition dispatcher. `0x008477A0` also calls the `ScriptConditions::evaluateNamedUnitRankLevel` anchor at `0x008478E5`, so `0x00C48898` is the ScriptConditions table.

Both tables' slot 2 is the shared base getter `0x004D6430`, not a class-name getter. The per-function call site is in the `dispatcher edge` part of each row in `gamelogic82_annotations.tsv`.

Generals `ScriptConditions.cpp` and `ScriptActions.cpp` (`work/references/generals/GeneralsMD/Code/GameEngine/Source/GameLogic/ScriptEngine/`) supply vocabulary only. The bodies follow the RA3 instructions. `0x004CA930` is inner-patched, so the oracle stubs it and does not execute it.

## Counted functions

| VA | C++ name | Ownership |
| --- | --- | --- |
| `00838d80` | `drawableForward` to `0x0052DB30` | calls 00837420 at 00838d8b with the same this |
| `00839ea0` | `namedAiBranch` | calls 00837420 at 00839eab with the same this |
| `0083a570` | `namedClearRepulsor` | calls 00837420 at 0083a57b with the same this |
| `0083a990` | `namedSellAndTransfer` | calls 00837420 at 0083a99c with the same this |
| `0083abd0` | `namedHandToLogic` | calls 00837420 at 0083abdb with the same this |
| `0083ac60` | `namedSetStance` | calls 00837420 at 0083ac6b with the same this |
| `0083ac90` | `namedSetExperience` | calls 00837420 at 0083ac9b with the same this |
| `0083afc0` | `namedCopyPosition` | calls 00837420 at 0083afcb with the same this |
| `0083b260` | `drawableForward` to `0x00524530` | calls 00837420 at 0083b26b with the same this |
| `0083b290` | `namedRelationTail` | calls 00837420 at 0083b29b with the same this |
| `0083b2d0` | `namedFixedRelationTail` | calls 00837420 at 0083b2db with the same this |
| `0083b300` | `namedProductionBitCall` | calls 00837420 at 0083b30b with the same this |
| `0083b350` | `namedRecruitTail` | calls 00837420 at 0083b35b with the same this |
| `0083b390` | `namedAiFloatFloor` | calls 00837420 at 0083b39b with the same this |
| `0083b3d0` | `namedUnheldSelect` | calls 00837420 at 0083b3db with the same this |
| `0083b410` | `namedWeaponOnSlot` | calls 00837420 at 0083b41c with the same this |
| `0083c430` | `namedAiOneTail` | calls 00837420 at 0083c43b with the same this |
| `0083c640` | `namedPassTwoInts` | calls 00837420 at 0083c64c with the same this |
| `0083c8d0` | `namedClassifyText` | calls 00837420 at 0083c8dc with the same this |
| `0083c920` | `namedLinkPair` | calls 00837420 at 0083c92b with the same this |
| `0083c950` | `namedEnableProduced` | calls 00837420 at 0083c95b with the same this |
| `0083cb30` | `namedAttackIfFree` to `0x0071C890` | calls 00837420 at 0083cb3c with the same this |
| `0083cb80` | `namedAttackIfFree` to `0x007070A0` | calls 00837420 at 0083cb8c with the same this |
| `0083cc30` | `namedExperienceScaled` to `0x006EE8F0` | calls 00837420 at 0083cc3b with the same this |
| `0083cc70` | `namedExperienceScaled` to `0x006EE9E0` | calls 00837420 at 0083cc7b with the same this |
| `0083ccb0` | `namedWriteRank` | calls 00837420 at 0083ccbb with the same this |
| `0083cdd0` | `namedForwardOnThis` | calls 00837420 at 0083cdde with the same this |
| `0083da80` | `namedFlaggedCommand` | calls 00837420 at 0083da8b with the same this |
| `0083dac0` | `namedAttachUpgrade` | calls 00837420 at 0083dacc with the same this |
| `0083dc70` | `namedExperienceArg` | calls 00837420 at 0083dc7b with the same this |
| `0083dd90` | `namedTwoArgCommand` | calls 00837420 at 0083dd9b with the same this |
| `0083ddc0` | `namedBindPair` | calls 00837420 at 0083ddcc with the same this |
| `0083e220` | `namedVirtualAttach` | calls 00837420 at 0083e23f with the same this |
| `0083e270` | `namedPlayOrStop` | calls 00837420 at 0083e27c with the same this |
| `0083e350` | `namedPublishDrawable` | calls 00837420 at 0083e35b with the same this |
| `0083e380` | `namedGuardBoth` | calls 00837420 at 0083e38c with the same this |
| `0083e3d0` | `namedGuardUpgrade` | calls 00837420 at 0083e3dc with the same this |
| `0083e520` | `evaluateNamedUnitRankLevel` | assertion string 0x00c4e740; calls 00837420 at 0083e52c with the same this |
| `0083e5d0` | `namedStatusBit0` | calls 00837420 at 0083e5dc with the same this |
| `0083e630` | `namedStatusBit0Clear` | calls 00837420 at 0083e63b with the same this |
| `0083e660` | `namedStatusBit0Keep` | calls 00837420 at 0083e66d with the same this |
| `0083e6c0` | `namedFoundFalseOrDestroyed` | calls 00837420 at 0083e6cc with the same this |
| `0083eba0` | `namedObjectPresent` | calls 00837420 at 0083ebab with the same this |
| `0083ed80` | `namedProducedCount` | calls 00837420 at 0083ed8b with the same this |
| `0083f6a0` | `namedTeamMemberBit` | calls 00837420 at 0083f6ac with the same this |
| `0083f700` | `namedTeamMemberOther` | calls 00837420 at 0083f70c with the same this |
| `0083f7e0` | `namedKindofBit` | calls 00837420 at 0083f7ec with the same this |
| `0083fa40` | `namedRankAboveFrame` | calls 00837420 at 0083fa4f with the same this |
| `0083fa80` | `namedExperienceCountEquals` | calls 00837420 at 0083fa9b with the same this |
| `0083fdb0` | `namedContainCompare` | calls 00837420 at 0083fdbc with the same this |
| `0083ff90` | `namedProducedPositive` | calls 00837420 at 0083ff9c with the same this |

`00837420` is the direct callee of `0083e520` at `0083e52c`. `0083e520` is anchored by assertion string `0x00c4e740`.

`0083DDC0`, `0083E380` and `0083FDB0` call the lookup a second time (`0083dde2`, `0083e39e`, `0083fdd8`) with the same engine `this`.

## Oracle

`tools/analysis/ports/verify_gamelogic82.py` runs the shipped bytes under Unicorn and `ra3_gamelogic82check` runs `runScriptNamedQuery`. Both see the same addresses, the same stub queue and the same AsciiString stand-ins (`0x004CA8F0`, `0x004CA930`, `0x004CBFF0`, `0x004CB6E0`, `0x0081FCE0`). The comparison is full EAX, the call trace `(target, ecx, eax-in, stack args)`, watched writes, and the callee's stack cleanup. 178 cases, 0 mismatches on the unmutated Release build.

`RA3_GAMELOGIC82_ORACLE_MUTANT` compiles a second checker that changes the rank test in `evaluateNamedUnitRankLevel` from signed `>=` to signed `<`. The five found-path rank cases change EAX and nothing else. The miss paths and the other 50 functions still match (`mutant_detected 5`, `mutant_unexpected 0`).

## Hooks

No hook keys. Every counted function calls `0x00837420`, and that lookup copies an AsciiString, so a shadow wrapper would re-enter the allocator. `ModuleTargets.cpp` is unchanged. There is no explicitOnly entry.

## Deferred

EASTL `allocator.h` instantiations in this window (23, `module_estimates` certain) are skipped. `0x0083AC00` and `0x0083DC30` move float arguments with `fld`/`fstp` and stay out of the integer oracle. Team-priority bodies `0x00826970`, `0x008269F0`, `0x00826A70` and `0x00826AF0` have no assertion and are not direct callees of `0x0083E520`. `0x00820780` and `0x00821080` are large script bodies. `0x0082B120` is outside this cluster. `0x00837420` is the stubbed lookup.
