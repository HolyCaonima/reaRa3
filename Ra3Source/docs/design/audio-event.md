# Audio event reconstruction

This branch reconstructs a framework-facing slice of RA3 1.0 audio state. It does
not implement an audio device. `ra3::audio` owns no game pointer: its views borrow
caller storage, and random generation, player-envelope work, channel initialization
and backend refresh remain explicit boundaries.

## Ownership evidence

The accepted set is the 50 rows in
`docs/analysis/ra3_1.0/audioevent_annotations.tsv`. The `0x00502xxx` through
`0x0050xxxx` methods are entries of the `SageAudioManager` vtable at
`0x00BDF5A8`, or direct callees with the same recovered player-state layout.
The `0x00559xxx`/`0x0055Bxxx` methods close to the `AudioEventRTS` vtable
`0x00BDF2C4`, constructor writes, source-path anchors and mutual calls. Address
proximity alone was not accepted.

Examples of rejected neighbours include `0x00504620`--`0x005049E0` math,
mouse/GameClient routines, generic EASTL helpers, and `0x0055A9B0` (already owned
by the math reconstruction). The table is branch-local evidence and is not folded
into the main reconstruction ledger.

## Non-owning layouts

`AudioEventView::minimumSize` is `0x94`. Recovered fields used here are:

| Offset | Meaning |
|---:|---|
| `0x0C` | borrowed `AudioEventInfo` pointer |
| `0x24`, `0x28` | minimum/maximum range override |
| `0x2C` | volume |
| `0x30` | player/category index |
| `0x34`, `0x38` | owner ID and owner kind (`position`, drawable, object, invalid) |
| `0x3C`--`0x48` | XYZ and position-present byte |
| `0x49` | should-fade byte |
| `0x4C`, `0x4D`, `0x50`, `0x54` | generation/control bytes |
| `0x58`, `0x5C`, `0x60`, `0x64` | pitch/volume random factors |
| `0x68` | delay |
| `0x70`, `0x74` | generated range |
| `0x7C` | attack/sound/decay/done portion |
| `0x84`, `0x88` | loop/count and saved playing handle |

`AudioEventInfoView::minimumSize` is `0x124`. The port uses sample ranges at
`0x7C/0x80`, `0x90/0x94`, `0xA4/0xA8`, the indexed range at `0xB8/0xBC`,
type at `0xE8`, cached priority at `0xEC`, group flags at `0x100`, and the
32-byte group-record range at `0x104/0x108`.

`SageAudioManagerView::minimumSize` is `0x35C`. This slice uses handles and
counters at `0xB4` and `0x140`, enable bytes at `0x136`--`0x13A`, the six
player handles at `0x2F0`, and a borrowed listener pointer at `0x350`.
Player state is an external three-element array with stride `0x1FC`; the manager
has a separate six-entry handle table.

All integer and floating fields are loaded with byte copies. This avoids adding
alignment, ownership or C++ object-lifetime claims to the recovered layout.

## Differential oracle

`tools/analysis/ports/verify_audio_event.py` executes every accepted shipped x86
body and compares it with `ra3_audioeventcheck`. Each function receives 64
seeded edge/random cases (3,200 total). The comparison includes semantic EAX,
full 80-bit x87 ST0 plus caller-rounded results, callee stack cleanup, exact
modified bytes, pointer alias normalization, callback order and callback
arguments. X87 entries rotate through control words `0x007F`, `0x027F` and
`0x037F`.

Only demonstrated boundaries are stubbed:

- `0x005A95F0`/`0x005A9520`: deterministic random real/integer;
- `0x0055A7A0`: deterministic generation while checking `0x0055B6F0`'s
  save/restore behaviour;
- `0x006C3A10`: playback-count map lookup, returning either a node or the
  container sentinel;
- `0x00504C80`, `0x00505150`, `0x00504DC0`: channel, envelope and backend-refresh
  callbacks whose order/arguments or isolation are compared.

The checker has an oracle-only deliberate mutation switch. The oracle enables it
across a real original-x86/checker comparison, requires the mismatch to be
detected, disables it, and requires the same comparison to match again.

## Hooks

`AudioTargets.cpp` has 15 targets. Every target is `explicitOnly=true`. The
hooks cover only isolated layout reads/writes and predicates; none performs
audio I/O, allocation, refcount destruction, threading, playlist mutation or a
backend call. No x87-returning function is hooked, so this target table does not
need a naked ST0 bridge. Shadow setters operate on a copy before invoking the
original and compare only the proven write span.

## Architecture references

The following GPL Generals sources were used only to guide class naming and
architecture; this module is an original implementation derived from RA3 bytes
and differential behaviour:

- `work/references/generals/GeneralsMD/Code/GameEngine/Include/Common/AudioEventRTS.h`
- `work/references/generals/GeneralsMD/Code/GameEngine/Source/Common/Audio/AudioEventRTS.cpp`
- `work/references/generals/GeneralsMD/Code/GameEngine/Include/Common/AudioEventInfo.h`
- `work/references/generals/GeneralsMD/Code/GameEngine/Include/Common/GameAudio.h`
- `work/references/generals/GeneralsMD/Code/GameEngine/Source/Common/Audio/GameAudio.cpp`
- `work/references/generals/GeneralsMD/Code/GameEngineDevice/Include/MilesAudioDevice/MilesAudioManager.h`
- `work/references/generals/GeneralsMD/Code/GameEngineDevice/Source/MilesAudioDevice/MilesAudioManager.cpp`

## Unknown boundary

Real backend/device calls, Miles objects, playlist tree ownership, asynchronous
work, destruction/refcount paths, `AudioEventRTS`'s owning string/shared-pointer
members, and unresolved SecuROM bodies are intentionally absent. Vtable methods
that merely proved manager ownership but crossed those boundaries remain
deferred, including the large playback/playlist methods around `0x005086E0`,
`0x0050ACF0`, `0x0050C940` and `0x0050CE80`.
