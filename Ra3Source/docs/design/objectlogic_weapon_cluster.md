# Weapon object-logic reconstruction

This reconstruction is a closed `WeaponSet` / `WeaponStateMachine` /
`WeaponTemplate` / `Weapon` cluster, not a range-based sweep. The accepted 50
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

## Verification

`verify_objectlogic_weapon.py` uses `Emulator(capture=...)` with the absolute
run35 snapshot path. Each accepted entry has a deterministic scenario and an
untouched-sentinel check. Results include stack cleanup, EAX/EDX or AL,
explicit writes, alias cases, float bits, and x87 values as applicable.
`--mutation-proof` asks the native checker to perturb every entry and requires
all 50 perturbations to be detected.

The hook table is a smaller safety subset. It contains only pure frame
conversions and pointer/integer queries, all marked `explicitOnly`; no
constructor, allocator, destructor, state transition, global write, or thread
operation is hookable from this module.
