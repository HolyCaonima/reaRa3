# Render 0x008E cluster reconstruction

## Scope and ownership

This batch reconstructs 53 clean native entries, all in
`0x008E4610–0x008EFE00`. Ownership is based on class and call evidence rather
than address proximity:

- The assertion paths at `0x008E52B0` and `0x008E78B0` name
  `SageEngine\source\GameEngineDevice\W3DDevice\GameClient\Drawable\W3DDynamicsDraw.cpp`.
  The paths at `0x008E5890` and accepted entry `0x008E8A00` name
  `SageEngine\source\GameEngineDevice\W3DDevice\GameClient\Drawable\W3DScriptedModelDraw.cpp`.
- `0x0090CD10` and `0x0090CE30`, both independently attributed to
  `W3DScriptedModelDraw.cpp`, call accepted helpers `0x008E87D0`,
  `0x008E89D0`, `0x008E8A00`, `0x008E8B20`, `0x008E8C10`, and
  `0x008ED5B0`. The last is called twice by `0x0090CE30`.
- W3D drawable vtable families occupy `0x00C555F0–0x00C55D2C`;
  `0x008E78B0` is present at `0x00C55A74`. These tables tie the state and
  model-layout helpers back to the same drawable hierarchy.
- Math behavior was cross-referenced against
  `WWVegas/WWMath/matrix3d.cpp`, `WWVegas/WWMath/quat.cpp`, and
  `WWVegas/WWMath/hermitespline.cpp`; W3D animation record behavior was
  checked against the Generals W3D animation sources (`hrawanim.cpp` and
  `hcanim.cpp`). These references supply semantics and names, not copied code.

The accepted VAs are:

- drawable/state: `4610 4620 4680 46A0 4820 4840 4910 4950 49E0 4A40
  4AA0 4AE0 4B30 56B0 5710 5740`;
- material/math/container: `5550 5B00 5B80 5F70 62E0 6330 6620 6770
  6800 87D0 88A0 8AB0 ADD0 AE00 AF70 B160 B2C0 B620 B6D0 D5B0 E0B0
  FE00`;
- terrain/model/animation: `6DD0 7000 70F0 71E0 89D0 8A00 8B20 8C10
  92A0 9350 95F0 96B0 98A0 9980 9BE0`.

Each suffix above has the `0x008E` prefix. The batch deliberately excludes
the already merged W3DView/Rain implementation. It also excludes
`0x008E5EC0/0x008E5EF0`, which are owned by `recon/vclass` as asset handler
functions. No function was accepted merely because it lies inside the VA
window.

## Implementation boundaries

`W3DDrawable` contains sparse drawable state, pass-mask, viewport and cached
property operations. `W3DMaterial` contains packed colors, x87 copy helpers,
matrix/quaternion operations, tree bounds, render records and geometry.
`W3DAnimation` contains terrain sampling, scripted-model selection and
animation interpolation.

Game pointers remain 32-bit and are decoded from raw layouts. Public types are
limited to stable value layouts such as the 16-byte pass mask; speculative
full game classes are not exposed. The segment-quad implementation preserves
the shipped SSE operation order and its x87 extended reciprocal square root.
Float-copy helpers intentionally use `fld m32`/`fstp m32` on Win32.

Every implementation carries its exact `ra3_1.0.game` VA. Source-family
comments are only present where the assertion/call or Generals reference
actually establishes one.

## Differential oracle

`ra3_render8echeck` uses one relocated blob protocol for all 53 entries.
`verify_render8e.py` executes the clean game bytes in Unicorn and compares:

- calling convention and stack cleanup;
- return values and relocated pointer returns;
- every byte in the caller-owned blob, including bytes that must remain
  untouched;
- self and overlapping aliases for drawable state, pass masks, float/record
  copies and descriptor copies;
- SSE truncation special values and exact segment-quad arithmetic;
- x87 m32 copies with zero, signed zero, infinities, quiet NaN and subnormals;
- full 80-bit ST0 for `0x008E6DD0` and `0x008E7000` at control words
  `007F`, `027F`, and `037F`.

The deterministic suite has 88 accepted cases and reports 53/53 entries with
zero mismatches. A deliberate candidate-output mutation at
`0x008E4610 + 0x104` is required to be detected before the oracle succeeds.
The pre-existing W3DView/Rain oracle remains independent.

## Hooks

`Render8eTargets.cpp` defines 15 safe `explicitOnly=true` targets:

`4610 4620 4680 4820 4910 4AA0 4AE0 4B30 5B80 87D0 89D0 8B20 8C10
96B0 D5B0`.

They are limited to scalar/sparse state operations and side-effect-free model
queries. Shadow wrappers compare only original writes or return values. There
are no D3D calls, buffer locks, allocation, refcount destruction, render-thread
transitions, or x87-return hooks. Consequently this batch does not need a
naked ten-byte x87 wrapper.

No game process was launched or injected while producing this batch. All game
and shared-work inputs were read-only; verification used only the on-disk
clean native image.
