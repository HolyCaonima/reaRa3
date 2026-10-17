# Weapon object-logic reconstruction

This reconstruction is a closed `WeaponSet` / `WeaponStateMachine` /
`WeaponTemplate` / `Weapon` cluster, not a range-based sweep. The accepted 29
entries are listed in `docs/analysis/ra3_1.0/objectlogic_annotations.tsv`.

## Evidence boundary

The ownership chain is supported by:

* the `WeaponStateMachine.cpp` assertion source path at `006A2AB0`;
* the `WeaponTemplate.cpp` assertion source path at `006A3020`;
* the `"Weapon"` string reached by the `006A82F0` constructor family;
* the state-machine constructor at `006A2850`, which installs the seven state
  vtables `00BF9648`, `00BF96A8`, `00BF9708`, `00BF9770`, `00BF97D8`,
  `00BF9838`, `00BF98A0`, and the machine vtable `00BF9908`;
* direct calls between the state methods, the frame conversion helpers, and
  the `Weapon` object methods.

The implementation was written from the resolved native instructions in the
immutable run35 capture. No Generals implementation was copied or translated.
The nearby factory at `006A2CF0`, allocator-heavy constructors, type-name
getters, and unrelated behavior/module functions are intentionally outside
the boundary. `006A2F30` and `006A3020` remain deferred because the former has
an unresolved inner-protected site in run35.

The audit also excludes all 17 entries owned by `recon/aistates`, generic
helper `006A2EE0`, and unproven owner-chain accessor `006A2480`. The releasing
destructor at `006A2A70` and target-refresh method `006A2B40` were removed
after stronger scenarios showed that their earlier projections covered only
cleanup or early-exit fragments, not the complete shipped behavior.

## Layout and API

`WeaponRuntimeData` names the complete observed `0x78`-byte object layout.
The public API separates template calculations, frame conversion, state
predicates, and cached-target transforms from the raw layout. The
address-indexed `evaluateWeaponEntry` adapter is only the stable protocol used
by the differential checker; callers should prefer the semantic functions.

Frame endpoints deliberately use single-precision products followed by
`ceil`, matching the native SSE/x87 path. Floating-point observations are
transported as bits. The oracle additionally checks full 80-bit ST0 values for
native x87 entries before reducing them to the public `float` result.
The constructor oracle compares all 120 bytes, preserves the three bytes the
native body leaves untouched, and separately checks its `ret 8` cleanup.

## Verification

`verify_objectlogic_weapon.py` uses `Emulator(capture=...)` with the absolute
run35 snapshot path. Six simple entries have 32 deterministic scenarios each;
the other 23 have 64 each, for 1,664 shipped-code scenarios plus the dedicated
in-place alias observation. Fixtures cover null and invalid state chains,
all slot limits, state transitions, filter outcomes, range/pitch boundaries,
cooldown/reload sentinels, exact writes, and three x87 precision-control
settings. Results include stack cleanup, EAX/EDX or AL, explicit writes,
untouched bytes, float bits, and full 80-bit x87 values as applicable.

`--mutation-proof` runs a named semantic mutant for every accepted entry and
requires a shipped-behavior mismatch followed by a clean baseline restoration.
The targeted high-risk mutants include a wrong constructor sentinel, a
first-slot rather than aggregate range scan, a changed x87 readiness
denominator, and an alias-unsafe cached-target transform.

The hook table is a smaller safety subset: `006A2430`, `006A2840`, and
`006BD310`. They are pure pointer/integer queries, all marked `explicitOnly`;
no constructor, allocator, destructor, state transition, global write, or
thread operation is hookable from this module.
