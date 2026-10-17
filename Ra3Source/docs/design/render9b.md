# Render script interpreter, 0x009B0000–0x009BFFFF

## What this range is

The ledger has 341 clean, unanalyzed functions whose starts lie in `0x009B0000–0x009BFFFF`. The middle of that range is the SageRender script interpreter: one small class per command. This batch counts 48 of those functions. Boilerplate counted: 0.

Each command vtable has five slots.

| Slot | Role |
| --- | --- |
| 0 | Shared destructor `009B9AB0`. It writes `00C5EEB4` and may free the object. Not counted. |
| 1 | Six-byte keyword getter, `mov eax, imm32; ret`. Not counted. |
| 2 | Usage string, the same six-byte form. The class is named from the first word of that string. |
| 3 | Parse, `ret 0xC`. Stack arguments are context, out record, token list. |
| 4 | Execute, `ret 0x10`. Stack arguments are context, parsed record, a state or out pointer, and the instruction pointer. |

A token record is 8 bytes. The string pointer sits at +4. The token count is at tokens+0xC and the array pointer is at tokens+0. `mov al, imm` / `setge al` keep the high bytes of EAX, so a success return is the pointer with its low byte replaced. The checker allocates its blob on a 4K boundary, matching the Unicorn heap, so those low-byte results relativize to the same offset.

Direct3D and the other unported callees are fake method tables. The checker and the oracle install the same stubs and compare the arguments those stubs observe.

No hooks were registered.

## Command classes

Slot 2 was read from the image. The string below is the start of that C string.

| Vtable | Command | Counted slots |
| --- | --- | --- |
| `00C60A50` | clear - clears the current target (color, depth, and stencil) | parse `009B7050` (slot 3), exec `009B3100` (slot 4) |
| `00C60AA4` | clearcolor - clears the color channels of the current target | exec `009B3130` (slot 4); parse is the shared `009B7050` |
| `00C60BF8` | clearz - clear the depth and stencil values on current target | exec `009B3180` (slot 4); parse is the shared `009B7050` |
| `00C60B04` | cleartarget [index] - set the surface for the specified target (0-3 for render targets, 4 for depth) to NULL | parse `009B70A0`, exec `009B3160` |
| `00C60C54` | setclearcolor [red] [green] [blue] [alpha] | parse `009B9CC0`, exec `009B7150` |
| `00C60CF0` | setcleardepth [depth] [(optional) stencil] | parse `009B9CF0`, exec `009B31B0` |
| `00C6136C` | renderquad [(optional) red] | parse `009B9FC0` |
| `00C61450` | renderfullquad [(optional) red] | parse is the shared `009B9FC0` |
| `00C5F010` | setshader [name] [(optional) technique] | exec `009B8070` |
| `00C5F164` | setshaderoverride [name] [technique] | exec `009B8110` |
| `00C5F1E4` | clearshaderoverride [name] - resets the technique for the shader to use back to default. | parse `009B8150`, exec `009B81A0` |
| `00C5F26C` | settexture [scopeBlock] [scopeParm] [name] | exec `009B81D0` |
| `00C5F448` | casttexture [name] [format] | parse `009B8230`, exec `009B82A0` |
| `00C6162C` | setint [scopeblock] [parmName] [value] | exec `009B75B0` |
| `00C616A0` | setbool [scopeblock] [parmName] [value] | exec `009B7620` |
| `00C60DB8` | setdepth [name] | exec `009B71C0` |
| `00C609EC` | goto [name] - jump to label (set with : Label) and continue execution | exec `009B30E0` |
| `00C60984` | else - opposing conditional execution block | parse `009B6F70`, exec `009B6FE0` |
| `00C60860` | elif [scopeBlock] [scopeParm] | exec `009B6EA0` |
| `00C608F0` | elifnot [scopeBlock] [scopeParm] | exec is the shared `009B6EA0` |
| `00C61050` | beginevent [name] - tags an event for profiling system (PIX) | exec `009B73C0` |
| `00C610B0` | endevent [name] - marks the end of an event for profiling system (PIX) | exec `009B7410` |
| `00C61118` | setmarker [name] - sets a marker for profiling system (PIX) | exec `009B7460` |
| `00C611DC` | endproc - marks the end of a procedure defintion | parse `009B31E0`, exec `009BC200` |
| `00C61310` | return - returns from the current procedure to the parent caller | parse `009B31F0`; exec is the shared `009BC200` |
| `00C612C4` | callproc [name] - calls the named procedure | exec `009BC3D0` |

Four tables have no usage getter. The names below are inference, not strings from the image.

| Vtable | Why the name is inferred | Counted slots |
| --- | --- | --- |
| `00C5DB18` | Slot 2 is `009A8750`, outside this range, and its first bytes are not `mov eax, imm32; ret`. | 4 `009B87A0`, 5 `009B5550`, 6 `009B5510`, 7 `009B54D0`, 9 `009B8800`, 10 `009B8740`, 11 `009B5330` |
| `00C5EC94` | Slot 2 is `009B2E30` itself. First bytes `33 C0 89 41 1C` are a clear, not a string getter. | 1 `009B6B80`, 2 `009B2E30` |
| `00C600F0` | Slot 2 is `009B58A0` itself. First bytes `56 8B F1 8B 06` are a method prologue, not a string getter. | 2 `009B58A0` |
| `00C5DB10` | Two slots only. There is no slot 2. | 0 `009B5860`, 1 `009B5880` |

## Helpers reached by a call

These four are not vtable slots. Each edge is an `E8` on an instruction boundary inside a slot-3 function listed above.

| VA | Function | Edge |
| --- | --- | --- |
| `009B7E50` | `parseFloatTokens` | `009B9CC0` at `009B9CD5` (`E8 76 E1 FF FF`), and `009B9FC0` at `009B9FF6` (`E8 55 DE FF FF`) |
| `009B7EF0` | `parseIntTokens` | `009B9CF0` at `009B9D24` (`E8 C7 E1 FF FF`) |
| `009B7740` | `findShaderByName` | `009B8150` at `009B8165` (`E8 D6 F5 FF FF`) |
| `009B7980` | `findTextureByName` | `009B8230` at `009B824E` (`E8 2D F7 FF FF`) |

## Per-function evidence

Shared bodies are counted once.

| VA | C++ | Evidence |
| --- | --- | --- |
| `009B7050` | `parseClearCommand` | Slot 3 of clear, clearcolor, and clearz. Succeeds when the token count is at least 1 (`setge`). |
| `009B70A0` | `parseClearTargetCommand` | Slot 3 of cleartarget. Parses token 1 as an integer and rejects an unsigned index above 3. |
| `009B9CC0` | `parseSetClearColorCommand` | Slot 3 of setclearcolor. Parses four floats from token 1 through `009B7E50`. |
| `009B9CF0` | `parseSetClearDepthCommand` | Slot 3 of setcleardepth. One float, and one integer stencil only when the token count is 3. |
| `009B9FC0` | `parseRenderQuadCommand` | Slot 3 of renderquad and renderfullquad. Stores 1.0 for all four channels, then four floats when the count is 5. |
| `009B8150` | `parseClearShaderOverrideCommand` | Slot 3 of clearshaderoverride. Looks the name up with `009B7740`. |
| `009B8230` | `parseCastTextureCommand` | Slot 3 of casttexture. Texture lookup through `009B7980`, then a format lookup. |
| `009B31E0` | `parseEndProcCommand` | Slot 3 of endproc. Writes a zero byte at context+0x140. |
| `009B31F0` | `parseReturnCommand` | Slot 3 of return. Writes -1 at out+8. |
| `009B6F70` | `parseElseCommand` | Slot 3 of else. Requires one token and patches the open frame. |
| `009B7150` | `execSetClearColorCommand` | Slot 4 of setclearcolor. Copies the parsed color into the clear state. |
| `009B31B0` | `execSetClearDepthCommand` | Slot 4 of setcleardepth. Copies depth and stencil. |
| `009B3100` | `execClearCommand` | Slot 4 of clear. Clear mask `0xF`. |
| `009B3130` | `execClearColorCommand` | Slot 4 of clearcolor. Clear mask 3. |
| `009B3180` | `execClearZCommand` | Slot 4 of clearz. Clear mask `0xC`. |
| `009B3160` | `execClearTargetCommand` | Slot 4 of cleartarget. |
| `009B30E0` | `execGotoCommand` | Slot 4 of goto. A negative index returns failure; otherwise the instruction pointer becomes index-1. |
| `009B6FE0` | `execElseCommand` | Slot 4 of else. If the frame is not taken, the instruction pointer jumps to the else target. |
| `009B6EA0` | `execElifCommand` | Slot 4 of elif and elifnot. `parsed+0xC == 3` flips the tested byte. |
| `009B8110` | `execSetShaderOverrideCommand` | Slot 4 of setshaderoverride. Writes the chosen technique into the shader. |
| `009B81A0` | `execClearShaderOverrideCommand` | Slot 4 of clearshaderoverride. Writes `0xFFFFFFFF` over the override. |
| `009B8070` | `execSetShaderCommand` | Slot 4 of setshader. Modes 0, 1, and 2 select the technique, then bind the shader. |
| `009B81D0` | `execSetTextureCommand` | Slot 4 of settexture. On a change, marks the parameter dirty and returns the block at shader+0x34. |
| `009B82A0` | `execCastTextureCommand` | Slot 4 of casttexture. Always clears AL on the way out. |
| `009B75B0` | `execSetIntCommand` | Slot 4 of setint. Compares the dword and ors the dirty mask at param+0x44. |
| `009B7620` | `execSetBoolCommand` | Slot 4 of setbool. Same path, comparing one byte. |
| `009B71C0` | `execSetDepthCommand` | Slot 4 of setdepth. Binds depth slot 4. |
| `009B73C0` | `execBeginEventCommand` | Slot 4 of beginevent. Virtual call at vtable+0x70. |
| `009B7410` | `execEndEventCommand` | Slot 4 of endevent. Virtual call at vtable+0x74. |
| `009B7460` | `execSetMarkerCommand` | Slot 4 of setmarker. Virtual call at vtable+0x78. |
| `009BC200` | `execEndProcCommand` | Slot 4 of endproc and return. Pops the call stack at context+0x144, or restores context+0x124 when the depth is 0. |
| `009BC3D0` | `execCallProcCommand` | Slot 4 of callproc. Pushes the current instruction pointer and jumps to target-1. |
| `009B7E50` | `parseFloatTokens` | Called from the setclearcolor and renderquad parsers. `$` names go through context vtable+0x80. |
| `009B7EF0` | `parseIntTokens` | Called from the setcleardepth parser. `$` names go through context vtable+0x7C. |
| `009B7740` | `findShaderByName` | Called from the clearshaderoverride parser. Count at +0x14, array at +8. |
| `009B7980` | `findTextureByName` | Called from the casttexture parser. Count at +0x28, array at +0x1C. |
| `009B2E30` | `clearStreamBindings` | `00C5EC94` slot 2. Zeros +0x1C and +0x20. |
| `009B6B80` | `assignStreamSource` | `00C5EC94` slot 1. Returns stride times source+0xC. A null callback stores `009B62B0` at +0x20. |
| `009B58A0` | `resetPassOverrides` | `00C600F0` slot 2. Calls vtable+0xC, then zeros the override fields. |
| `009B5510` | `applyNamedState` | `00C5DB18` slot 6. Two virtual calls, slots +0x24 and +0x6C. |
| `009B54D0` | `applyNamedSampler` | `00C5DB18` slot 7. Same shape, second slot +0x7C. |
| `009B5550` | `applyNamedStage` | `00C5DB18` slot 5. Same shape, second slot +0xCC. |
| `009B87A0` | `querySurfaceExtent` | `00C5DB18` slot 4. Queries by name, then copies two dwords and one name byte. |
| `009B5330` | `flushActiveTarget` | `00C5DB18` slot 11. Flushes when self+0x38 is set. |
| `009B8800` | `selectPassConstant` | `00C5DB18` slot 9. Selects the row for the pass at self+0x3C. |
| `009B8740` | `bindTechniquePass` | `00C5DB18` slot 10. Ensures the pass, binds constants, then selects the row. |
| `009B5860` | `releasePrimarySurface` | `00C5DB10` slot 0. Virtual call at vtable+0x114. |
| `009B5880` | `releaseSecondarySurface` | `00C5DB10` slot 1. Virtual call at vtable+0x118. |

## Dropped candidates

293 of the 341 candidates are not counted.

- Slot 1 and slot 2 of the command tables are `mov eax, imm32; ret` getters. They are boilerplate and were left out so the counted set stays at a boilerplate share of 0.
- `009B9AB0`, slot 0 of those tables, writes vtable `00C5EEB4` and is the shared destructor.
- `009B31D0` is `B0 01 C2 10 00` (`mov al, 1; ret 0x10`), a return-constant stub.
- `009B2E40` is `8B 01 8B 50 0C FF E2` (`mov eax, [ecx]; mov edx, [eax+0xC]; jmp edx`), a forwarding thunk. It also sits in `00C5EC94` slot 4 and `00C600F0` slot 4.
- `009B9330` and `009B93D0` are the reserve and shrink callees of the procedure stack. They are stubbed so the execute bodies can run, and they are not counted.
- The wide command bodies were left unported: the parse slots of setshader (`009BD1D0`), setshaderoverride (`009BD380`), settexture (`009BD460`), setint (`009BCC40`), setbool (`009BCD20`), setdepth (`009B9D50`), goto (`009BBFA0`), elif (`009BBE20`), the event commands (`009BC070`), and callproc (`009BC310`), plus the execute slots of renderquad (`009BC420`) and renderfullquad (`009BC780`).
- The remaining clean unanalyzed functions in the range have no cited slot and no `E8`/`E9` edge from a counted function.

## Oracle

`python Ra3Source/tools/analysis/ports/verify_render9b.py` after the comparison was restored:

`render9b oracle: 48 entries, 111 cases, 0 mismatches`

Mutation: `parseClearCommand` compared the token count with `>= 2` instead of `>= 1`. The oracle reported

`FAIL 0x009B7050 one ret cpp=0x00000000 emu=0x00000001`

and `1 mismatches`. The comparison was restored and the oracle returned to 0 mismatches.

## Hooks

None.
