# Embedded Lua 4.0 VM, Parser, Garbage Collector, Tag-Method, Table & Code-Generator Reconstruction (`0x00403660`–`0x0040E480`)

## 1. Observed Binary Evidence & Ownership

All 56 `entry-lost` (`resolution=body`) functions in `0x00403660`–`0x0040E480` belong to the embedded **Lua 4.0** runtime in `ra3_1.0.game`. Their ownership and exact Lua 4.0 subsystem boundaries are anchored by `.rdata` diagnostic and event strings read directly from `ra3_1.0.game`:

- `0x00BD5150`: `""` (empty string fallback in `luaM_realloc`)
- `0x00BD5154`: `"memory allocation error: block too big"` (referenced by `0x00403660` body `0x004A3130` via `.securom` immediate slot `0x018FC6E4`, resolved at `ms=253582` in `run35/slot_changes.tsv`)
- `0x00BD517C`: `"stack overflow"` (referenced by `0x004038B0` body `0x004A4010` at `0x004A4024`)
- `0x00BD51B8`: `"?"` (default chunk name referenced by `0x00403EC0` body `0x00406550` via `.securom` immediate slot `0x018FC47C`, resolved at `ms=253582`)
- `0x00BD5230`: `luaO_typenames` table (`"userdata"`, `"nil"`, `"number"`, `"string"`, `"table"`, `"function"`) read at `0x0049F6D9` (`0x0040A120`)
- `0x00BD52AC`: ``"`%.20s' expected"`` (referenced by `0x00404370` body `0x019149F0` via `.securom` immediate slot `0x018FCB10`, resolved at `ms=253857`)
- `0x00BD52C0`: ``"`%.20s' expected (to close `%.20s' at line %d)"`` (referenced by `0x00404450` body `0x004A1440` via `.securom` immediate slot `0x018FC6A4`, resolved at `ms=253857`)
- `0x00BD52F0`: `"constant table overflow"` (pushed at `0x004748A4` in `0x00404550` body `0x00474880`)
- `0x00BD59A0`: `luaT_eventname` table (`"gettable"`, `"settable"`, `"index"`, `"getglobal"`, `"setglobal"`, `"add"`, `"sub"`, `"mul"`, `"div"`, `"pow"`, `"unm"`, `"lt"`, `"concat"`, `"gc"`, `"function"`, `NULL`) read at `0x0049F6E0` (`0x0040A120`)
- `0x00BD59F0`: `luaT_validevents[7][15]` validity matrix read at `0x0049F6BA` (`0x0040A120`)
- `0x00BD5AC0`: `"tag table overflow"` (pushed at `0x00443DD0` in `0x00409E90` body `0x00443DC0`)
- `0x00BD5AD4`: `"%d is not a valid tag"` (referenced by `0x0040A120` body `0x0049F650` via `.securom` immediate slot `0x018FC62C`)
- `0x00BD5AF0`: `"cannot change `%.20s' tag method for type `%.20s'%.20s"` (pushed at `0x0049F6EB` in `0x0040A120` body `0x0049F650`)
- `0x00BD5B28`: `"tag method must be a function (or nil)"` (pushed at `0x0049F710` in `0x0040A120` body `0x0049F650`)
- `0x00BD5B70`: `" with default tag"` / `0x00BBED30`: `""` (selected at `0x0049F6CD`–`0x0049F6D4` in `0x0040A120`)
- `0x00BD5CC8`: `"table overflow"` (referenced by `0x0040B410` body `0x01929B70` via `.securom` immediate slot `0x018FCAE8`, resolved at `ms=253582`)
- `0x00BD5CDC`: `"table index is nil"` (pushed at `0x01929B8B` in `0x0040B410` body `0x01929B70`)
- `0x00BD5FFC`: `"function or expression too complex"` (pushed at `0x0047AA83` via `[0x018FCC14]` in `0x0040C980`, `0x019130B1` in `0x0040CAA0`, and `0x019070EC`/`0x0190720A` in `0x0040D5B0`)
- `0x00BD6034`: `"code size overflow"` (pushed at `0x01907154`/`0x0190726B` in `0x0040D5B0` body `0x01907050`)

## 2. Protection Artefacts Observed in Recovered Bodies

Across the `.text` and `.securom` bodies resolved in `run35/snapshot_3_240s`:

1. **Entry-site trampoline**: Each function's original 6-byte entry at `site` was overwritten with `jmp dword ptr [slot]` (`FF 25 <slot>`), where `slot` in `.securom` (`0x018F5000`–`0x018FCFFF`) points at the relocated/decrypted `body` (`0x00404950`–`0x004A58A0` in `.text` or `0x019026A0`–`0x01933830` in `.securom`).
2. **SecuROM call-site rewrites**: Direct `call <callee>` instructions inside relocated bodies were rewritten to `push <ret_addr>; jmp <callee>` or `push <ret_addr>; push <callee>; ret`, followed by junk dead bytes between `<ret_addr - N>` and `<ret_addr>`.
3. **Inline arithmetic target thunks**: Several call sites compute their callee target inline from `.securom` constants and `.text` words without calling any SecuROM VM helper:
   - `0x00404370` (`0x01914A0C`): `mov ebp, [0x018F76F0]; add ebp, [0x0059BD14]` -> `0xFDFA8E5B + 0x02462955 = 0x0040B7B0` (`luaX_token2str`)
   - `0x004048E0` (`0x00439CD7`): `mov ebx, [0x018F7474]; mov ecx, [0x004049D1]; ror ebx, cl` -> `ror(0x08194C00, 5) = 0x0040CA60` (`luaK_lastisopen`)
   - `0x00405B10` (`0x0193388C`): `mov edx, [0x0065EBF2]; xor edx, [0x018F746C]` -> `0x8BE8BFFE ^ 0x8BA87C7E = 0x0040C380` (`luaX_lex`)
   - `0x00406CF0` (`0x019150CC`): `mov eax, [0x018F7700]; not eax; xor eax, [0x0069CE86]` -> `~0x00675713 ^ 0xFFD8915C = 0x004039B0` (`call_linehook`)
4. **SecuROM execution counters**: `dec dword ptr [0x0198xxxx]; je <re-encrypt_stub>` instructions decrement a nonzero counter in `.securom` data on each call.
5. **Relocated immediates in `.securom`**: Immediate constants (`MINPOWER2 = 4`, `NO_TOKEN = 286`, `MAXARG_U = 0x03FFFFFF`, `MASK_B = 0x00003FC0`, `NO_REG = 0xFF`, `MASK_OP = 0x3F`, `MINSTRTABSIZE = 16`, and `.rdata` string pointers) were replaced by `dword ptr [0x018FCxxx]` loads resolved on first execution (`run35/slot_changes.tsv` at `ms=253582..253857`).

## 3. Reconstructed Functions (Stage 1: 28 Functions)

| # | Entry VA | Slot VA | Body VA | Section | Name | Lua 4.0 Role |
|---|----------|---------|---------|---------|------|--------------|
| 1 | `0x00403660` | `0x018FB434` | `0x004A3130` | `.text` | `LuaMemRealloc` | `luaM_realloc` |
| 2 | `0x004036D0` | `0x018F504C` | `0x019082A0` | `.securom` | `LuaMemGrowAux` | `luaM_growaux` |
| 3 | `0x00403740` | `0x018F640C` | `0x00466F20` | `.text` | `LuaDoInitStack` | `luaD_init` |
| 4 | `0x004038B0` | `0x018FB698` | `0x004A4010` | `.text` | `LuaDoCheckStack` | `luaD_checkstack` |
| 5 | `0x00403CA0` | `0x018FB0A4` | `0x004252F0` | `.text` | `LuaProtectedCall` | `lua_call` |
| 6 | `0x00403EC0` | `0x018FA148` | `0x00406550` | `.text` | `LuaDoBuffer` | `lua_dobuffer` |
| 7 | `0x00404370` | `0x018F5E88` | `0x019149F0` | `.securom` | `LuaParserCheckNext` | `checknext` |
| 8 | `0x004043F0` | `0x018F7340` | `0x004A58A0` | `.text` | `LuaParserOptional` | `optional` |
| 9 | `0x00404450` | `0x018FB1B8` | `0x004A1440` | `.text` | `LuaParserCheckMatch` | `check_match` |
| 10 | `0x00404550` | `0x018F6120` | `0x00474880` | `.text` | `LuaParserStringK` | `stringK` |
| 11 | `0x004048E0` | `0x018FA584` | `0x00439CA0` | `.text` | `LuaParserAdjustMultAssign` | `adjust_mult_assign` |
| 12 | `0x00404AC0` | `0x018FBEAC` | `0x019026A0` | `.securom` | `LuaParserCloseFunc` | `close_func` |
| 13 | `0x00405B10` | `0x018F71A0` | `0x01933830` | `.securom` | `LuaParserFuncStat` | `funcstat` |
| 14 | `0x00406CF0` | `0x018F5EE8` | `0x01915010` | `.securom` | `LuaVmTraceExec` | `traceexec` |
| 15 | `0x00406D70` | `0x018F9FF4` | `0x00404ED0` | `.text` | `LuaVmClosure` | `luaV_closure` |
| 16 | `0x00408DB0` | `0x018F6040` | `0x00467B40` | `.text` | `LuaGcCollectProto` | `collectproto` |
| 17 | `0x00408DF0` | `0x018FA02C` | `0x00404950` | `.text` | `LuaGcCollectTable` | `collecttable` |
| 18 | `0x00408E30` | `0x018FB34C` | `0x004A2490` | `.text` | `LuaGcCollectStrings` | `collectstrings` |
| 19 | `0x00408ED0` | `0x018FC3F0` | `0x01906B30` | `.securom` | `LuaGcCollectUdata` | `collectudata` |
| 20 | `0x00409120` | `0x018FAE84` | `0x0049F310` | `.text` | `LuaGcMarkAll` | `markall` |
| 21 | `0x00409E90` | `0x018FC3D0` | `0x00443DC0` | `.text` | `LuaNewTag` | `lua_newtag` |
| 22 | `0x0040A070` | `0x018F5B20` | `0x0043B020` | `.text` | `LuaTagOfObject` | `luaT_tag` |
| 23 | `0x0040A120` | `0x018FAEB8` | `0x0049F650` | `.text` | `LuaSetTagMethod` | `lua_settagmethod` |
| 24 | `0x0040A340` | `0x018FC148` | `0x004431E0` | `.text` | `LuaPushValue` | `lua_pushvalue` |
| 25 | `0x0040CA60` | `0x018FBEFC` | `0x00435790` | `.text` | `LuaCodeLastIsOpen` | `luaK_lastisopen` |
| 26 | `0x0040CAA0` | `0x018F5C84` | `0x01913030` | `.securom` | `LuaCodeSetCallReturns` | `luaK_setcallreturns` |
| 27 | `0x0040D2F0` | `0x018F725C` | `0x004975D0` | `.text` | `LuaCodeGetLabel` | `luaK_getlabel` |
| 28 | `0x0040D5B0` | `0x018FC418` | `0x01907050` | `.securom` | `LuaCodeAdjustStack` | `luaK_adjuststack` |

## 4. Deferred Entries (12 Functions)

- **11 entries deferred because their `.securom` body calls a SecuROM VM obfuscation helper (`0x00E61000`, `0x00E610C0`, `0x00E60CC0`, `0x00E60CE0`, `0x00E62160`)**:
  - `0x00404BA0` (`slot=0x018F5D68`, `body=0x01913770`, `section=.securom`)
  - `0x00405560` (`slot=0x018FC2E4`, `body=0x019061B0`, `section=.securom`)
  - `0x00405F10` (`slot=0x018FA964`, `body=0x01931370`, `section=.securom`)
  - `0x00407020` (`slot=0x018FBA24`, `body=0x018FEE50`, `section=.securom`)
  - `0x004070C0` (`slot=0x018F6C6C`, `body=0x01925BF0`, `section=.securom`)
  - `0x00408F70` (`slot=0x018FBD7C`, `body=0x01901590`, `section=.securom`)
  - `0x0040D240` (`slot=0x018F5DCC`, `body=0x019142B0`, `section=.securom`)
  - `0x0040D990` (`slot=0x018F52E0`, `body=0x0190B810`, `section=.securom`)
  - `0x0040DC70` (`slot=0x018FC16C`, `body=0x01904560`, `section=.securom`)
  - `0x0040DFF0` (`slot=0x018F5C98`, `body=0x019130F0`, `section=.securom`)
  - `0x0040E480` (`slot=0x018F52A0`, `body=0x0190A290`, `section=.securom`)
- **1 entry deferred due to `function_ledger.tsv` `.text` body overlap**:
  - `0x00408FE0` (`slot=0x018FA648`, `body=0x00422AC0`, `section=.text`): `.text` body `0x00422AC0` falls inside ledger entry `0x0042295e` (`size=1538`, `0x0042295E`–`0x00422F60`).

## 5. Verification

- Checker executable: `Ra3Source/build/win32/bin/Release/ra3_luavmcheck.exe`
- Differential oracle: `Ra3Source/tools/analysis/ports/verify_lua_vm.py`
- Result: `Scenarios: 80, Mismatches: 0`
