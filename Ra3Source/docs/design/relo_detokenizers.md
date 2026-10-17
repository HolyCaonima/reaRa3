# Relo detokenizers (protected entry-lost batch)

First batch rebuilt from protected code. All 46 functions are `entry-lost`: the
ledger entry holds a 6-byte `jmp [slot]` whose slot lies in `.securom`. The
run35 capture (`work/runtime_dump/run35/snapshot_3_240s`) resolved every slot to
a plaintext function body already present in the shipped image: 36 bodies in
`.text` gaps that Ghidra did not define, 10 in `.securom`. None of the `.text`
bodies falls inside another ledger function, so each behaviour is counted once,
at its entry. `securom_sites.tsv` rows for all 46 entries read
`resolution=body`, `function_securom=entry-lost`.

Code: `src/asset/include/ra3/asset/ReloDetokenizers.h`,
`src/asset/src/ReloDetokenizers.cpp`. Annotations:
`docs/analysis/ra3_1.0/relodetok_annotations.tsv` (entry, slot, body, section
per row).

## Observed

- 21 `reloDetokenize<Type>` field walkers. Each walks 12-byte
  `{fieldId, typeHash, offset}` descriptors terminated by `(0, 0)` and is reached
  by direct calls from other code (for example `0x0048A520` from `0x0048A6F4`,
  `0x0048A7B7`, `0x0048D6F9`). No data table points at the entries. The type
  names are inferred from the descriptors: `typeHash` values are
  `asset_name_hash.type_id` of field types, e.g. the HordeGarrison walker hands
  `fieldId 1 / typeHash 0xBAE3BC7D` (`type_id("GarrisonContainModuleData")`) to
  the Garrison walker. A walker's own type id does not appear in its body, so
  each name is an inference from its field set.
- 20 `reloClone*` copy helpers called by those walkers (fixed-size structs,
  strings, `Coord3D`, status bit flags). Names carrying an address suffix are
  placeholders; the original names are unknown.
- Five primitives: `assetMemoryCompare`, `assetByteSpanLess`,
  `reloApplyPointerOffsets` (the `.relo` fixup loop in `asset_layout.md` §2.1),
  `ReloScratchBuffer::ensureCapacity`, `ReloStreamHeader::clear`.
- Ownership: the `RtsXmlPipeline/ReloDetokenizers.h` assertion path at
  `0x00BD9420` (referenced at `0x0043C633`, `0x0045E9A3`, `0x00477513`,
  `0x00477A53`), `assetmanager/resourcemanager.cpp:1041` at `0x00BD9298`
  (referenced at `0x00417253`, `0x53` bytes past the `0x00417200` entry), and
  call edges inside the cluster (for example body `0x00491B00` of `0x00447740`
  calls entry `0x00416ED0` through `push 0x491B40; push 0x416ED0; ret`).

## Protection artefacts in the bodies

The recovered bodies still carry SecuROM rewrites; the C++ implements the
original semantics:

- calls written as `push <return>; push <target>; ret`;
- immediates replaced by loads from `.securom` data. `assetByteSpanLess`
  (`0x00491B00`) computes `or eax, [0x018FCD20]` where the original had a
  negative constant; the oracle reads the runtime value from the capture.

SecuROM's own code is never mapped or analysed.

## Verification

`python Ra3Source/tools/analysis/ports/verify_relo_detokenizers.py` runs each
body in Unicorn over the capture (allocator and `memset` stubbed) against
`ra3_relodetokcheck.exe`: 166 scenarios, 0 mismatches in the integration build.
Mutations `lhsLen < rhsLen` to `<=` in `assetByteSpanLess` and output slot
`+0xA4` to `+0xA8` in `reloDetokenizeHordeGarrisonContainModuleData` were
detected (3 mismatches) and reverted.

## Hooks

None. Candidates once live behaviour of patched slots is validated: the pure
primitives with `.text` bodies (`0x004416E0`, `0x0043A1D0`, `0x0043B110`,
`0x00491B00`, `0x0046DC80`), hooked at the body rather than the entry thunk.

## Deferred

Thirteen empty-status rows: five functions whose callees are still
`unresolved` in run35 (`0x00417700`, `0x00445B10`, `0x00461860`,
`0x004618B0`, `0x0048F7C0`) and eight further entry-lost walkers outside this
batch (`0x0043D6D0`, `0x0043D780`, `0x00440580`, `0x00443A50`, `0x004459B0`,
`0x00447870`, `0x00454F10`, `0x0045F830`).
