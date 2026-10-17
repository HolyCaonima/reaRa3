# Remaining RA3 1.0 StringBase cluster

This note covers 50 reconstructed `StringBase<char>` / `StringBase<wchar_t>`
entries on branch `recon/stringbase2`. It extends the framework described in
`game_string.md`; addresses are VAs for the English RA3 1.0 executable.

## Layout and ABI

The object remains one 32-bit data pointer. A non-null data pointer has:

| data offset | field |
|---:|---|
| -8 | signed 32-bit reference count |
| -4 | unsigned 16-bit recorded length |
| -2 | unsigned 16-bit capacity, including the terminator |

Null is the normal empty representation, but a non-null block with recorded
length zero is also empty. Read/search methods use the recorded length rather
than rescanning for NUL. Static empty character arrays are `0x00BD282C`
(ASCII) and `0x00BDAAD0` (Unicode).

The reconstructed code uses `char16_t` for the game's two-byte `wchar_t`.
Member entries receive `this` in ECX. Hook replacements therefore use
`__fastcall(this, edx, ...)`; source `copyRange` entries pop 12 bytes,
predicates with pointer/length pop 8, one-argument members pop 4, and
`0x004CA510` remains cdecl. No function in this set returns through x87 ST0,
so the x87 naked-entry rule is not applicable.

Text copy sources are `{vtable, text, int32 length}`. Their vtables are
`0x00BDAB6C` / `0x00BDAB7C`; abstract-source base vtables are
`0x00BDAAB0` / `0x00BDAAC0`. Source slots observed here are deleting
destructor, length, `copyRange`, and `copyTo`.

## Accepted entries

Names below describe recovered effects. Except for the template/class identity
from the shipped debug path, they are inferred names, not observed symbols.

| VA | reconstructed effect |
|---:|---|
| 004C9F80 | fixed-length UTF-16 no-case comparison |
| 004CA010 | ASCII abstract-source deleting destructor |
| 004CA030 | ASCII `isEmpty` |
| 004CA050 | ASCII recorded-length character search |
| 004CA0D0 | ASCII starts-with explicit range |
| 004CA170 | ASCII ends-with explicit range |
| 004CA1D0 | ASCII ends-with-no-case explicit range |
| 004CA250 | Unicode data/static-empty fallback |
| 004CA260 | Unicode recorded-length character search |
| 004CA330 | Unicode starts-with-no-case explicit range |
| 004CA380 | Unicode ends-with explicit range |
| 004CA4E0 | skip leading UTF-16 whitespace |
| 004CA510 | length-aware UTF-16 comparison |
| 004CA930 | ASCII release |
| 004CAAD0 | ASCII remove-last |
| 004CAB10 | ASCII remove-first |
| 004CADD0 | Unicode writable-buffer preparation |
| 004CAE10 | Unicode assignment |
| 004CAE90 | Unicode set from abstract source |
| 004CAF00 | Unicode append abstract source |
| 004CB020 | ASCII source `copyRange` |
| 004CB060 | ASCII skip separators |
| 004CB0A0 | ASCII skip non-separators |
| 004CB0E0 | Unicode source `copyRange` |
| 004CB140 | Unicode abstract-source deleting destructor |
| 004CB160 | Unicode skip separators |
| 004CB1B0 | Unicode skip non-separators |
| 004CB200 | Unicode-object to ASCII translation |
| 004CB2A0 | ASCII-text to Unicode translation |
| 004CB460 | ASCII no-case compare to C string |
| 004CB4C0 | ASCII no-case compare to object |
| 004CB520 | Unicode compare to C string |
| 004CB630 | Unicode no-case compare to object |
| 004CB730 | ASCII trim |
| 004CB8E0 | ASCII `nextToken` |
| 004CB990 | ASCII case-insensitive `None` predicate |
| 004CBA50 | Unicode trim |
| 004CBB40 | Unicode `format_va` |
| 004CBBE0 | Unicode `nextToken` |
| 004CBCE0 | Unicode translation object wrapper |
| 004CBD00 | Unicode constructor from ASCII object |
| 004CBE80 | Unicode append C string |
| 004CC010 | ASCII substring from C string |
| 004CC0B0 | ASCII substring from object |
| 004CC160 | ASCII replace-all |
| 004CC3B0 | Unicode one-unit constructor |
| 004CC3F0 | Unicode pointer-length constructor |
| 004CC460 | Unicode substring from C string |
| 004CC5B0 | Unicode replace-all |
| 004CC820 | Unicode replace-all object wrapper |

The ASCII/Unicode trim and token control flow agrees with these exact GPL
reference files:

- `work/references/generals/GeneralsMD/Code/GameEngine/Source/Common/System/AsciiString.cpp`
- `work/references/generals/GeneralsMD/Code/GameEngine/Source/Common/System/UnicodeString.cpp`

Those files guide names and intent only. Allocation, recorded-length behavior,
copy-source calls, lock traces, bounds, and implementations were independently
derived from RA3 evidence.

## Evidence

Clean shipped `.text` closes the ordinary entries. The run35 capture supplies
these recovered native bodies:

| entry | captured body |
|---:|---:|
| 004C9F80 | 004A9570 |
| 004CA0D0 | 0047B550 |
| 004CA170 | 00440BF0 |
| 004CA1D0 | 00451BD0 |
| 004CA330 | 0047F7C0 |
| 004CA380 | 00489CC0 |
| 004CADD0 | 0044A780 |
| 004CB460 | 0045BB30 |
| 004CB4C0 | 00494440 |
| 004CB520 | 00494DA0 |
| 004CB630 | 0045F8F0 |
| 004CBB40 | 01900970 |
| 004CBBE0 | 01930820 |
| 004CBE80 | 004DB160 |

The comparison predicates return arithmetic unit/length differences, not just
-1/0/1. Case folding is delegated to the game's CRT callbacks. Translation
first asks the opaque converter for a size including NUL, ensures that many
units, converts, then stores `size - 1` as the recorded length.

Substring bounds use signed 32-bit `start + count`: a negative start is clamped
to zero while preserving the computed end, an end beyond the input is clamped,
and an invalid start/end releases the destination. Replace-all follows the
original repeated substring/assignment/append sequence, preserving aliases,
reference-count changes, and allocation order.

`0x004CBB40` uses a 0x2000-unit local buffer and passes 0x1FFF to
`_vsnwprintf`. The framework isolates CRT and `va_list` behavior behind a
callback and then applies the observed StringBase set/release semantics.

## Offline differential

`tools/analysis/ports/verify_game_string2.py` uses:

`Emulator(capture=D:\Game\Ra3\RevRa3\work\runtime_dump\run35\snapshot_3_240s)`

and `ra3_stringbase2check`. For each of the 50 entries, 64 edge/random cases
compare return values, callee cleanup, aliases, full live block headers and
areas, and ordered lock/allocation/free writes: 3,200 scenarios per
configuration, zero mismatches. There are no floating or x87 results in this
cluster. A checker-only deliberate mutation changes the no-case comparator;
the oracle requires that mismatch, removes the mutation, and rechecks the
restored result.

Unresolved dependencies are modeled only at their observed game-level
contracts: ASCII set `0x004CB690`, ASCII writable buffer `0x004CA970`, and
conversion entries `0x004C9FE0` / `0x004CD890`. The accepted callers still
execute their original shipped/captured bytes. For `0x004CA930`, the unresolved
six-byte site is replaced only with the observed `self->data = 0` effect; lock,
decrement, free, and unlock execute from clean shipped bytes.

## Hook boundary

`StringBase2Targets.cpp` contains only six explicit targets:

- `stringbase2asciifind`
- `stringbase2unicodefind`
- `stringbase2widecompare`
- `stringbase2asciisourcecopyrange`
- `stringbase2unicodesourcecopyrange`
- `stringbase2asciiisnone`

All are `explicitOnly=true`. Read-only shadows call the original first and
return its value. `copyRange` snapshots the source, calls the original, runs
the reconstruction on the snapshot, and compares exactly `count * sizeof(Char)`
destination bytes. Entry thunks and entries whose steal window contains a
short relative branch are intentionally not hooked.

## Deferred boundaries

- Protected entries with unresolved run35 slots remain unclaimed, including
  `0x004CA970`, `0x004CBD30`, `0x004CBDA0`, `0x004CBE10`,
  `0x004CBEF0`, `0x004CBF60`, and `0x004CBFA0`.
- `0x004CB690` is available as framework set support but is not an accepted
  entry because its source-construction slot `0x004CB6B7` is unresolved.
- The protected default-delimiter construction branch inside Unicode
  `0x004CBBE0` leaves the captured native body. Explicit delimiter behavior is
  closed and differential-tested; null/default delimiters remain deferred.
- No SecuROM VM body or conversion-thunk internals are inferred.
