# ThingTemplate accessor cluster

Twelve clean functions in `0x00780000-0x00796FFF`. Eight are direct callees of
the ThingTemplate constructor `0x00795F70` or of its callees `0x0078E0D0` and
`0x0078E1C0`. Three are slots of vtable `0x00C2927C`, which that constructor
stores at offset 0. `0x0078B6E0` is the record mover called by `0x0078DCA0`.

The implementation is `src/common` (`include/ra3/common/ThingTemplate.h`,
`src/ThingTemplate.cpp`). Field names are offsets. Method names describe the
shipped effect; they are inferences, not recovered symbols.

Generals was used only as architecture orientation:

- `work/references/generals/GeneralsMD/Code/GameEngine/Include/Common/ThingTemplate.h`
- `work/references/generals/GeneralsMD/Code/GameEngine/Source/Common/Thing/ThingTemplate.cpp`
- `work/references/generals/GeneralsMD/Code/GameEngine/Include/Common/Thing.h`

RA3's nine mask dwords, module-record stride `0x14`, and return residues are
not the Generals layout. The code is original.

## Ownership

Address adjacency is not ownership. A getter that only reads an offset is not
counted. Each counted VA has one of these edges, also written in
`thingtemplate_annotations.tsv`:

| VA | Name | Proof |
| --- | --- | --- |
| `0078e7c0` | `maskAllNine` | Target of vtable `00C2927C` slot 3. Ghidra missed the entry; `0078E800` jumps to it. |
| `0078e800` | `maskAllNineAtC0` | Vtable `00C2927C` slot 3. Adds `0xC0`, then jumps. Counted separately because that adjustment is its own behavior. |
| `0078e810` | `maskAnyNineAtC0` | Vtable `00C2927C` slot 2. |
| `0078bb10` | `findModuleRecordByKey` | Direct callee of ThingTemplate.cpp `0078DE50`. |
| `0078cd90` | `registryPrimaryValue` | Direct callee of constructor callees `0078E0D0` and `0078E1C0`. |
| `0078ce10` | `registrySecondaryValue` | Same edge. |
| `0078ce90` | `registryValuePresent` | Same edge. |
| `0078dca0` | `removeFirstMatchingModuleRecord` | Same edge, four calls from each. |
| `0078b6e0` | `moveModuleRecords` | Direct callee of `0078DCA0`. |
| `0078e0d0` | `installParsedModule` | Direct callee of constructor `00795F70` (five sites). |
| `0078e1c0` | `installParsedModuleAlternate` | Direct callee of constructor `00795F70`. |
| `0078e2a0` | `appendTemplateWordAndResetMap` | Direct callee of constructor `00795F70`. |

`0078E800` is not an alias of `0078E7C0`: the slot thunk changes `this`.
`0078E810` returns full EAX `0` or `1`. `0078E7C0` returns the query pointer
with its low byte replaced (`1` on success, `0` at the failing index).
`0078CE90` returns `0`, or the value address with its low byte replaced by `1`.
`0078DCA0` with no hit returns the incoming EAX with the low byte cleared. A
second-range hit returns that range's old end pointer with the low byte set
to `1`.

Null `this` is outside the contract. The shipped bodies dereference `ecx`.

## What was not counted

126 evaluated candidates were rejected. 119 of them are the ledger functions
in `0x0078B010-0x0078EBB0` other than the 11 accepted entries in that slice.
The other seven are outside that slice and were evaluated because they are
the constructor, its false split, its vtable stubs, or an already claimed VA.

Grouped, these partition the 119:

- Lifecycle (7): `0078b7a0`, `0078b840`, `0078d790`, `0078d830`, `0078d940`,
  `0078db60`, `0078e540`. Destructors, the Thing constructor's global counter,
  and frees. Not hooked.
- Already claimed (2): `0078d880` and `0078da40` (`recon/hookcat`).
  `00788640` is the same owner's claim and is one of the seven outside the slice.
- EASTL `allocator.h` (4): `0078cd00`, `0078ddc0`, `0078e3d0`, `0078e500`.
  `00795e50`, called by the constructor, is the fifth EASTL body and is outside
  the slice.
- Thing.cpp x87/SSE position transforms, deferred (5): `0078c080`, `0078c500`,
  `0078c7c0`, `0078cee0`, `0078d1e0`. Source-anchored, not accessors.
- SecuROM on the exercised path (2): `0078b690` calls import thunk `00499640`.
  `0078de50` is the ThingTemplate.cpp anchor; its found-key path calls
  `0040EBF0` (`jmp dword ptr [0x18ED6B4]`). Its not-found append also copies
  two uninitialized stack bytes. Deferred, not counted.
- Unfinished vtable slot (1): `0078e840`, slot 1, reads global `00DEBA80` and
  makes virtual calls.
- No vtable slot and no direct edge to `00795F70`, `0078DE50`, `0078E0D0`,
  `0078E1C0`, `0078E2A0`, or `0078DCA0` (98): `0078b010`, `0078b030`,
  `0078b050`, `0078b070`, `0078b090`, `0078b0b0`, `0078b0d0`, `0078b0f0`,
  `0078b110`, `0078b130`, `0078b150`, `0078b170`, `0078b190`, `0078b1b0`,
  `0078b1d0`, `0078b1f0`, `0078b210`, `0078b230`, `0078b250`, `0078b270`,
  `0078b290`, `0078b2b0`, `0078b2d0`, `0078b2f0`, `0078b310`, `0078b330`,
  `0078b350`, `0078b370`, `0078b390`, `0078b3b0`, `0078b3d0`, `0078b3f0`,
  `0078b410`, `0078b430`, `0078b450`, `0078b470`, `0078b490`, `0078b4b0`,
  `0078b4d0`, `0078b4f0`, `0078b510`, `0078b530`, `0078b550`, `0078b570`,
  `0078b590`, `0078b5b0`, `0078b5d0`, `0078b5f0`, `0078b610`, `0078b630`,
  `0078b650`, `0078b670`, `0078b730`, `0078b870`, `0078b8e0`, `0078b960`,
  `0078b970`, `0078b990`, `0078ba00`, `0078bae0`, `0078bb30`, `0078bb60`,
  `0078bc00`, `0078bc40`, `0078bc80`, `0078bcc0`, `0078bd00`, `0078bda0`,
  `0078bde0`, `0078bdf0`, `0078be20`, `0078be60`, `0078bea0`, `0078bee0`,
  `0078bf60`, `0078bfb0`, `0078ca30`, `0078ca80`, `0078cb00`, `0078cb30`,
  `0078cb60`, `0078cb90`, `0078cc50`, `0078d4a0`, `0078d4f0`, `0078d520`,
  `0078d560`, `0078d650`, `0078d7e0`, `0078d850`, `0078d8c0`, `0078dba0`,
  `0078e350`, `0078e470`, `0078e960`, `0078e980`, `0078eaf0`, `0078ebb0`.
  Shared field offsets and the string in `0078e980` are not an edge.
  `0078bb30` and `0078bb60` scan the same record lists `0078DE50` scans, but
  nothing anchored calls them.

Outside the slice, also rejected: `00795f70` (constructor; lifecycle and
SecuROM asserts), `0079614d` (false split of that constructor; the byte
`0x0079614D` is the ASCII of `May` and has no callers), `00795cd0` (vtable
slot 0 deleting destructor), `00549590` (slots 4 and 5, `mov eax, ecx; ret`,
outside the window, shared), `0065f220` (slots 6 and 7, `xor eax, eax; ret`,
outside the window, shared).

`0078e7c0` is the twelfth accepted function and is not in the 130-row slice.

Sibling `recon/*` annotation files do not claim any accepted VA. Main already
owns the AIStates/AITNGuard entries at `0x0079xxxx`, which is above this window.

## Oracle

`tools/analysis/ports/verify_thingtemplate.py` drives
`ra3_thingtemplatecheck` against `ra3_1.0.game` in the Unicorn emulator.
Release is the semantic run. Debug only has to compile.

Straight-line leaves have 2-6 inputs (a normal value, a boundary, and the
register residue). The registry and record walks have one case per branch,
including unsigned key order and a null value falling through to the second
map. Pointer results are checked by the relationship the instructions define:
null, an offset from the record base, or a low byte written over a pointer.
Payload dwords and the empty global map's miss sentinel `0x00CC32D4` are
compared as raw EAX.

`maskAnyNineAtC0Mutant` flips the overlap test from `jne` to `je`. The oracle
requires it to return 1 where the shipped function returns 0, and 0 where the
shipped function returns 1. A temporary edit of the real comparison made the
oracle fail (`any miss checker 00000001`); restoring it returned to zero
mismatches.

Paths not executed, because they allocate or enter SecuROM:

- `0078B6E0` copy loop (`004CA9B0` / `004CA440` allocates on first use).
- `0078DCA0` first-range hit (`004CA930` uses that lock).
- `0078E0D0` / `0078E1C0` call to `0078DE50` (found-key path enters `0040EBF0`).
- `0078E2A0` growth call `006F3E20`, and the tree free when the pointer is non-null.

The disk image of map `0x00CC32D0` has a null root, so the name lookup misses
and the virtual call is not taken. `[0x00DF9D20]` is 0 in the image; the
oracle points it at an empty registry for the install calls and restores it.

## Hooks

Five optional hooks, all `explicitOnly=true`. Stolen prefixes are whole
instructions, position-independent, and do not include a branch. Expected
bytes are the first 16 from `tools/analysis/disasm.py <VA> --hook-only`.

| Key | VA | RVA | Stolen | expected16 |
| --- | --- | --- | --- | --- |
| `thingtemplatemaskall` | `0078e7c0` | `0x0038E7C0` | 5 | `8B 44 24 04 56 8B F1 33 D2 57 2B F0 8D 64 24 00` |
| `thingtemplatemaskallatc0` | `0078e800` | `0x0038E800` | 6 | `81 C1 C0 00 00 00 E9 B5 FF FF FF CC CC CC CC CC` |
| `thingtemplatemaskany` | `0078e810` | `0x0038E810` | 6 | `8B 54 24 04 33 C0 81 C1 C0 00 00 00 56 8D 49 00` |
| `thingtemplatefindmodule` | `0078bb10` | `0x0038BB10` | 6 | `8B 41 14 8B 49 18 3B C1 74 11 8B 54 24 04 8B FF` |
| `thingtemplateregistrypresent` | `0078ce90` | `0x0038CE90` | 5 | `51 8B 44 24 08 56 8B F1 8D 4C 24 0C 51 8D 54 24` |

Wrappers are `__fastcall(this, void* edx, ...)`. Shadow mode records
`thingtemplatemaskall`, `thingtemplatemaskallatc0`, `thingtemplatemaskany`,
`thingtemplatefindmodule`, and `thingtemplateregistrypresent` through
`recordScalar` and returns the original EAX. No allocator, destructor, thread,
or global lifecycle path is hooked. No x87 return is hooked.

## Unknowns

- Original class and method names. No usable RTTI on this vtable.
- What the nine mask dwords mean. Kind-of is only an inference; Thing.cpp
  `0078D1E0` tests bit 6 of the dword at `[this+4]+0xC0`, which is the first
  of these nine, but the other eight are not named by a string.
- The full ThingTemplate size. The constructor and `0078E0D0` write at least
  through `+0x308`, and the named dword is at `+0x2F8`.
- Whether `0078E7C0` is ever entered with `ecx` not equal to `this+0xC0`.
  Its only observed edge is the slot-3 jump.
