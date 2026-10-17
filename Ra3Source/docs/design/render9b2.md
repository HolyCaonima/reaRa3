# Render script interpreter, second pass

## What this pass adds

`recon/render9b` counted 48 command methods and left the wide parse and execute bodies. This pass counts 36 more functions whose starts lie in `0x009B0000–0x009BFFFF`. Boilerplate counted: 0. No hooks.

The new code is `RenderScript9B2.h` / `RenderScript9B2.cpp`. It includes `RenderScript9B.h` and stays in `ra3::render`, so `parseIntTokens`, `parseFloatTokens`, `findShaderByName` and `findTextureByName` are the functions the first pass already matched. A `ScriptHost` carries the memory manager, the string pool, and the log callback. The pool pointer is the same object as `ScriptServices::stringPool`.

Command tables still use the five-slot layout from the first pass: slot 3 parses (`ret 0xC`; context, out record, tokens) and slot 4 executes (`ret 0x10`). `mov al, 1` and `xor al, al` keep the high bytes of EAX. The checker blob is 4K-aligned, matching the Unicorn allocation, so those low-byte results relativize to the same offset.

Direct3D and the other unported callees are fake method tables. The checker and the oracle install the same stubs and compare the arguments those stubs record. The 71-byte string assign at `009B6080` runs on the emulator and is mirrored in C++; it is not counted. The vector growth at `009B93D0` and the shrink at `009B9330` are stubbed and not counted.

## Command classes

Slot 2 was read from the image. The text is the start of that C string.

| Vtable | Slot 2 string | Counted |
| --- | --- | --- |
| `00C6162C` | setint [scopeblock] [parmName] [value] | parse `009BCC40` |
| `00C616A0` | setbool [scopeblock] [parmName] [value] | parse `009BCD20` |
| `00C61534` | setfloat [scopeblock] [parmName] [value] ... | parse `009BCB10` |
| `00C60DB8` | setdepth [name] | parse `009B9D50` |
| `00C609EC` | goto [name] | parse `009BBFA0` |
| `00C61050` | beginevent [name] | parse `009BC070` |
| `00C612C4` | callproc [name] | parse `009BC310` |
| `00C5F010` | setshader [name] [(optional) technique] | parse `009BD1D0` |
| `00C5F164` | setshaderoverride [name] [technique] | parse `009BD380` |
| `00C5F26C` | settexture [scopeBlock] [scopeParm] [name] | parse `009BD460` |
| `00C5F310` | settosize [scopeBlock] [scopeParm] [texture or surface name] | parse `009BD560`, exec `009BACE0` |
| `00C60860` | elif [scopeBlock] [scopeParm] | parse `009BBE20` |
| `00C606F0` | ifnot [scopeBlock] [scopeParm] | parse `009BB930`, exec `009BBA80` |
| `00C60778` | if [scopeBlock] [scopeParm] | parse `009BBB30`, exec `009BBC80` |
| `00C607F4` | endif | parse `009BBD30`, exec `009BBDB0` |
| `00C6065C` | setcamera [name] | parse `009B9C40`, exec `009B6D70` |
| `00C60368` | setviewport [value] [value] [value] [value] | exec `009B6BD0` |
| `00C604E4` | setscissor [value] [value] [value] [value] | exec `009B6CA0` |
| `00C6136C` | renderquad [(optional) red] ... | exec `009BC420` |
| `00C61450` | renderfullquad [(optional) red] ... | exec `009BC780` |

## Tables without a usage getter

These names are inference. Slot 2 is not `mov eax, imm32; ret`.

| Vtable | Slot 2 | Counted |
| --- | --- | --- |
| `00C600F0`, `00C60108`, `00C60120`, `00C60138`, `00C60150`, `00C60168` | `009B58A0` | the six DrawPrimitiveUP vertex-batch flushes, each at slot 3 |
| `00C5F4C8` | `009B38A0` | dirty dispatch `009B3910` at slot 4 |
| `00C5DB18` | `009A8750` | classify `009B5380` slot 17, matrix order `009B5430` slot 16, constant-routine select `009BB040` slot 18 |
| `00C60338` | `009B60D0` | dynamic stream bind `009B9630` at slot 1 |

## How the new methods fit

Parse bodies allocate a `Scrape::Variable` (`00C5DA48`) through the manager, intern the token text through the pool, and push the object on the context vector at +0xBC. The stored index is the new size minus one, which lives at context+0xC8. Execute bodies read that object back and write device or scope state.

`setint` requires four tokens, stores type 4, then parses one integer. `setbool` stores type 6 and sets the out byte when token 3 compares equal to `true` (`00BD5558`), including a folded `TRUE`. `setfloat` requires at least three tokens and rewrites the variable type from the float count: 3, 7, 8 or 9 for one through four values. `setdepth` interns `NULL` (`00BD12DC`), asks whether the name matches, and either logs `00C61AF0` (`ERROR: SCRAPE Unknown surface %s`) or stores the surface index.

`setshader` looks the name up first. Four tokens push a type-4 variable and finish with out kind 2. Three tokens parse an integer into the slot that started as -1; a successful parse finishes with out kind 0 and that integer (`009BD334` stores 0, not 2). A failed parse resolves a technique, or allocates a name and pushes it on the shader record. Two tokens store the shader and leave the technique at -1. The return is `mov al, 1` on the out pointer. `setshaderoverride` resolves the technique the same way and returns 1 while EAX is still that small index.

`settexture` is 17 bytes at `009BD460`. Its `jge 009BD471` falls into the 230-byte body, which is not a separate counted function. The body builds a type-0xC variable and looks the texture up; a miss logs `00C6173C`. `settosize` accepts either a texture or a surface. On success `009BD668` does `mov al, 1` while EAX is still the surface index, so a surface of -1 returns `0xFFFFFF01`. The execute at `009BACE0` reads width and height from the texture query or the surface record, writes both floats through the parameter slot, and ORs the block masks. A null parameter returns `mov al, 1` on the variable pointer (`009BAD92`).

`if` / `ifnot` allocate a frame, intern the token array, push a type-6 variable, and push the frame. `elif` patches the open frame, interns `elif` (`00C60874`), and stores kind 2 or 3 from the name match. `endif` parses only when the token count is 1; a longer line does `xor al, al` while EAX is still the token pointer (`009BBD3A`), which is why a token block at 0x340 returns 0x300. Execute of `if` / `ifnot` reads the bool at `[[variable+0xC]+0x4C]` (`009BBA99`) and, when the branch is taken and the target is non-negative, rewrites the instruction pointer. Execute of `endif` releases and frees the frame, then shrinks the frame vector.

`goto`, `beginevent` and `callproc` push a small record on the label, event, or procedure vector. `setcamera` logs the usage string `00BF47B4` when the line is too short, and `00C61ACC` when the camera name is unknown. Its execute calls context vtable+0x28 with the device, the camera field, and the view size. `setviewport` and `setscissor` copy the four extents onto the device. Both missing extents take `xor al, al` while EAX is still -1 (`009B6C71`, `009B6D41`), returning `0xFFFFFF00`.

`renderquad` and `renderfullquad` return 0 when the state shader is 0 or the gate byte is 0. Otherwise they log the command name (`00C61380` / `00C61464`), bind, set three render states, emit the viewport-sized positions, bind clear, and restore the saved pass.

The six vertex-batch flushes share the prologue read at `009B58C0`: call `009C4B80` on pass+0xF8, optionally the nested pass virtual, then `009C4550`. Half (`009B58C0`) and thirds (`009B5A40`) upload, then `memmove` the remainder via `004D2EDA` and return that destination pointer. The other four upload a different count and then zero the field; their returns stayed 0 in the cases the oracle ran. The six bodies end in device vtable +0x14C, `IDirect3DDevice9::DrawPrimitiveUP`, with primitive type 1-6 and the matching primitive count (point list n, line list n/2, line strip n-1, triangle list n/3, strip and fan n-2), so they are named by D3DPRIMITIVETYPE. List types keep the leftover vertices.

`dispatchDirtyState` tests the primary object, walks the node list at +0x24 (each node is the stored pointer minus 8) and calls the cdecl function at node+0x20, then updates the device through vtable+0x108/+0x100 or +0x104. `classifyPrimitive` and `selectConstantRoutine` are the kind switches on `00C5DB18`. `readMatrixOrder` queries `MatrixOrder` (`00C600D4`), compares the result with `d3d` / `gl`, logs `00C60094` on anything else, then queries `DefaultParameterScopeBlock` (`00C60078`). `bindDynamicStream` multiplies the count by the source stride. When the object flag and the local flag are both zero it copies the globals at `00E1B418` and `00E1B41C` (both 0 in this image). Otherwise it returns the virtual base pointer.

## Dropped candidates

- Every slot-1 line in the candidate list is a keyword getter. `009B3750` is the shape: 6 bytes, `mov eax, 0xc5f024`.
- `009B31D0` is 5 bytes, `mov al, 1; ret 0x10`.
- `009B2E40` is 7 bytes and jumps through `[vtable+0xC]`.
- `009B9AB0` writes vtable `00C5EEB4` and optionally frees the object. Shared destructor, not counted.
- `009BD471` is only entered by `jge` from `009BD460`. It is the rest of `parseSetTextureCommand`.
- `009B6080`, `009B93D0` and `009B9330` are the string assign and the vector growth/shrink. Not counted.
- Left uncounted because the set was already 36 real bodies: settarget parse `009B9E10` (429 bytes) and exec `009B7210` (386), setviewport parse `009B9AD0` (367), stretchrect `009B8F40` (511) and `009B9140` (335), setfloat exec `009BA010` (232), proc parse `009BC130` (207), label parse `009BC250` (184), and the larger non-command slots `009B55F0` (612), `009B8450` (588), `009BA8C0` (696), `009BE370` (810). `009B38E0` (46 bytes, slot 3 of `00C5F4C8`) was left with them.

## Oracle

`python Ra3Source/tools/analysis/ports/verify_render9b2.py`

```
render9b2 oracle: 36 entries, 103 cases, 0 mismatches
```

Mutation: `parseSetIntCommand` compared the token count with 5 instead of 4. The oracle reported:

```
FAIL 0x009BCC40 seven ret cpp=0x00000000 emu=0x00000001 off=0x20 cpp=0x00 emu=0x24
FAIL 0x009BCC40 text ret cpp=0x00000000 emu=0x00000000 off=0x20 cpp=0x00 emu=0x24
```

The comparison was restored. The same command then printed `0 mismatches` again.

Hooks: none.
