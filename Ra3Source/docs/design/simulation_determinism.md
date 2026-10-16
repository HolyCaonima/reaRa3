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
address. They calculate the delta with SSE, reject a non-positive delta, then
convert the unsigned random dword on x87 and evaluate
`random * factor * delta + low` in extended precision. Their result remains
in ST0 and depends on the caller's x87 control word. They are mapped but not
hooked: a normal C++ `float` hook would round the result and violate
determinism. A future hook must use a naked assembly return path.

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

The five registered functions (`randomstep`, `randominit`,
`logicrandomint`, `clientrandomint`, and `audiorandomint`) are clean `.text`
functions with position-independent stolen prefixes. They are offline
verified and require an explicit key so ordinary hook configurations and the
refpack-only integration test remain unchanged.
