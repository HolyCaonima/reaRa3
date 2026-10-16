# Simulation determinism

## Random streams

RA3 retains the six-dword add-with-carry generator from Generals
`Common/RandomValue.cpp`. The shipped `ra3_1.0.game` evidence is the clean
cluster at `0x005A9310..0x005A963A`; the ancestor source was used only to name
the recovered operations.

| Stream | State address | Integer helper | Real helper |
|---|---:|---:|---:|
| game logic | `0x00CB7420` | `0x005A94C0` | `0x005A9550` |
| game client | `0x00CB73F0` | `0x005A94F0` | `0x005A95A0` |
| game audio | `0x00CB7408` | `0x005A9520` | `0x005A95F0` |

Each state is six contiguous little-endian `uint32_t` values. The initial
words are `F22D0E56 883126E9 C624DD2F 0702C49C 9E353F7D 6FDF3B64`.
`0x00DF60CC` stores the game-logic base seed.

`0x005A9310` adds words 5 through 0 with carry, stores each intermediate sum
in words 4 through 0, then increments the six-word state from word 5 toward
word 0. It returns the final word 0. `0x005A93B0` adds one caller seed to each
initial word, writes all three streams, and writes the base seed.

The logic integer helper has changed since Generals: when `low >= high`, RA3
returns `low` without advancing the state. Client and audio retain the older
unsigned-width behavior, including advancing for `low == high`. All three
return `high` without advancing when the unsigned inclusive width wraps to
zero. These distinctions cover reversed, singleton, negative, and full
32-bit ranges.

The three real helpers are instruction-identical except for their state
address. They calculate the single-precision delta `high - low` with SSE. A
zero or negative delta returns `high` from x87 without advancing; a positive
or NaN delta advances, because `comiss` reports an unordered comparison as
below. They then convert the random dword on x87, add `2^32` when its sign
bit is set, and evaluate `random * factor * delta + low`. `factor` is the
float global at `0x00DF65B0`. The static initializer at `0x00BA7EC0` computes
`1.0 / (pow(2.0, 32.0) - 1.0)` on x87 through MSVCR80 `_CIpow` and stores it
as a float, which rounds to exactly `2^-32` (`0x2F800000`). The result
remains in ST0, so it is rounded only by the caller's x87 control word.

`ra3::sim::randomRealExtended` is a naked MSVC x86 port with the same x87
instruction order, so it leaves the same 80-bit ST0 under every control word.
`ra3::sim::randomReal` is the portable form. It matches the original under
the game's usual control word `0x007F` (24-bit precision) while every
intermediate stays in the normal single-precision range. Denormal or
overflowing operands differ because x87 keeps its wider exponent. The Unicorn
oracle compares the full 80-bit ST0 and post-state for 2,000 random and
special-value cases per helper under control words `0x007F`, `0x027F`, and
`0x037F`. When both results are NaN it skips the payload comparison, because
the emulator's softfloat picks NaN operands differently from hardware. The
live shadow run compares payloads too, because there both sides execute on
the real FPU.

All RNG hooks are `explicitOnly`. Shadow snapshots the six state words before
calling the original, applies the Ra3Source transition to the snapshot, and
compares both the return value and complete post-state. `randominit` compares
all three post-states and the base seed. Replace mode updates the game's state
in place. The logic RNG is expected to run on the simulation thread; hook
temporary state is stack-local.

## CRC investigation

RA3 does not retain an identifiable Generals-style `XferCRC::addCRC` in the
clean image. A complete scan found no clean length-based IEEE CRC-32 routine;
`0x0092A0D0` is only a null-terminated string CRC helper. The string
`CRCBuffer::PREALLOCATE_SIZE must be increased` leads to `0x004D4EA0`, but
that function merely records a high-water allocation size for desync report
generation. It does not accumulate or finalize a game-state checksum.

Consequently no stateful CRC target is registered in this round. Treating
`0x004D4EA0` or the string CRC as the per-frame simulation checksum would be
incorrect. The per-frame producer remains an open call-graph problem; work
must stop if its path crosses a non-clean function. The Generals
`XferCRC.cpp` rotate/add algorithm remains only a hypothesis and is not
presented as an RA3 implementation.

## Hookability

The eight registered functions (`randomstep`, `randominit`,
`logicrandomint`, `clientrandomint`, `audiorandomint`, `logicrandomreal`,
`clientrandomreal`, and `audiorandomreal`) are clean `.text` functions with
position-independent stolen prefixes. They are offline verified and require
an explicit key so ordinary hook configurations and the refpack-only
integration test remain unchanged.

Each real-helper hook is a naked cdecl entry that reserves a 10-byte buffer
and calls the C++ `runRandomReal`. Replace mode runs the port on the game
stream and stores ST0 with `fstp tbyte`. Shadow mode calls the original,
then runs the port on a state snapshot, and compares the 10 result bytes and
the six-dword post-state. The entry reloads the buffer with `fld tbyte`, so
the caller receives the unrounded extended result either way. In a live
skirmish the client-stream callers changed the x87 control word on 18,372 of
18,437 calls, while logic callers stayed at `0x007F`. A float-returning hook
would be wrong whenever such a caller runs above 24-bit precision.
