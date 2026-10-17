#include "ra3/core/LuaVmPorts.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>

namespace ra3::core {
namespace {

LuaVmRuntimeHooks g_hooks{};

// Observed at 0x00BD5230..0x00BD5247 (6 entries: userdata, nil, number, string, table, function).
constexpr const char* kLuaTypeNames[7] = {
    "userdata",
    "nil",
    "number",
    "string",
    "table",
    "function",
    "",
};

// Observed at 0x00BD59A0..0x00BD59DF (16 entries).
constexpr const char* kLuaEventNames[16] = {
    "gettable",
    "settable",
    "index",
    "getglobal",
    "setglobal",
    "add",
    "sub",
    "mul",
    "div",
    "pow",
    "unm",
    "lt",
    "concat",
    "gc",
    "function",
    "le",
};

// Observed at 0x00BD59F0..0x00BD5A58 (7 tags x 15 events byte matrix).
constexpr int8_t kLuaValidEvents[7][15] = {
    {1, 1, 0, 0, 0, 1, 1, 1, 1, 1, 1, 1, 1, 0, 1},  // 0: userdata
    {1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1},  // 1: nil
    {1, 1, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1},  // 2: number
    {1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1},  // 3: string
    {0, 0, 1, 0, 0, 1, 1, 1, 1, 1, 1, 1, 1, 0, 1},  // 4: table
    {1, 1, 0, 0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0},  // 5: function
    {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},  // 6: cmark
};

constexpr int32_t kLuaNoToken = 0x11E;
constexpr uint32_t kMaxIntMinus2 = 0x7FFFFFFDu;
constexpr uint32_t kMaxArgU = 0x03FFFFFFu;

void AdvanceLexerToken(LuaLexState* ls) {
    ls->lastline = ls->linenumber;
    if (ls->lookahead.token != kLuaNoToken) {
        ls->t = ls->lookahead;
        ls->lookahead.token = kLuaNoToken;
    } else if (g_hooks.luaLex != nullptr) {
        ls->t.token = g_hooks.luaLex(ls, &ls->t.seminfo);
    }
}

}  // namespace

void SetLuaVmRuntimeHooks(const LuaVmRuntimeHooks& hooks) {
    g_hooks = hooks;
}

const LuaVmRuntimeHooks& GetLuaVmRuntimeHooks() {
    return g_hooks;
}

// Clean .text helper at 0x00404070 (luaO_power2).
uint32_t LuaPower2(uint32_t n) {
    uint32_t p = 4u;
    if (n < p) {
        return p;
    }
    do {
        p += p;
    } while (p <= n);
    return p;
}

// Clean .text helper at 0x00408C40 (markobject).
void LuaGcMarkObject(LuaStkMark* st, LuaTObject* o) {
    if (st == nullptr || o == nullptr) {
        return;
    }
    switch (o->ttype) {
        case kLuaTypeUserdata:
        case kLuaTypeString: {
            LuaTString* ts = o->value.ts;
            if (ts != nullptr && ts->marked == 0) {
                ts->marked = 1;
            }
            break;
        }
        case kLuaTypeTable: {
            LuaHash* h = o->value.ht;
            if (h != nullptr && h->marked == h) {
                h->marked = st->tmark;
                st->tmark = h;
            }
            break;
        }
        case kLuaTypeClosure: {
            LuaClosure* cl = o->value.cl;
            if (cl != nullptr && cl->mark == cl) {
                cl->mark = st->cmark;
                st->cmark = cl;
            }
            break;
        }
        case 7: {
            LuaCallInfo* ci = o->value.ci;
            if (ci != nullptr && ci->func != nullptr) {
                LuaClosure* cl = ci->func;
                if (cl->mark == cl) {
                    cl->mark = st->cmark;
                    st->cmark = cl;
                }
            }
            break;
        }
        default:
            break;
    }
}

// Clean .text helper at 0x00404090 (luaO_equalObj).
int32_t LuaEqualObj(const LuaTObject* t1, const LuaTObject* t2) {
    if (t1 == nullptr || t2 == nullptr || t1->ttype != t2->ttype) {
        return 0;
    }
    if (t1->ttype == kLuaTypeNil || static_cast<uint32_t>(t1->ttype) > 6u) {
        return 1;
    }
    if (t1->ttype == kLuaTypeNumber) {
        return (t1->value.n == t2->value.n) ? 1 : 0;
    }
    return (t1->value.ptr == t2->value.ptr) ? 1 : 0;
}

// [1/27] site=0x00403660 slot=0x018FB434 body=0x004A3130 section=.text
// Reallocates or frees a Lua heap block (luaM_realloc).
// SecuROM rewrite: loads 0x00BD5154 ("memory allocation error: block too big") from [0x018FC6E4].
void* LuaMemRealloc(LuaState* state, void* oldBlock, uint32_t size) {
    if (size == 0u) {
        if (g_hooks.crtFree != nullptr) {
            g_hooks.crtFree(oldBlock);
        } else {
            std::free(oldBlock);
        }
        return nullptr;
    }
    if (size >= 0xFFFFFFFDu) {
        if (g_hooks.luaError != nullptr) {
            g_hooks.luaError(state, "memory allocation error: block too big");
        }
    }
    void* block = (g_hooks.crtRealloc != nullptr)
        ? g_hooks.crtRealloc(oldBlock, size)
        : std::realloc(oldBlock, size);
    if (block == nullptr && state != nullptr) {
        if (g_hooks.luaBreakRun != nullptr) {
            g_hooks.luaBreakRun(state, 4);
        }
    }
    return block;
}

// [2/27] site=0x004036D0 slot=0x018F504C body=0x019082A0 section=.securom
// Grows a power-of-two array when crossing a power-of-two boundary (luaM_growaux).
// SecuROM rewrite: loads MINPOWER2 = 4 from [0x018FC93C].
void* LuaMemGrowAux(
    LuaState* state,
    void* block,
    uint32_t nelems,
    int32_t inc,
    uint32_t elementSize,
    const char* errormsg,
    uint32_t limit) {
    const uint32_t uinc = static_cast<uint32_t>(inc);
    const uint32_t newn = nelems + uinc;
    if (nelems >= limit - uinc) {
        if (g_hooks.luaError != nullptr) {
            g_hooks.luaError(state, errormsg);
        }
    }
    if ((newn ^ nelems) <= nelems || (nelems > 0u && newn < 4u)) {
        return block;
    }
    const uint32_t allocBytes = LuaPower2(newn) * elementSize;
    return LuaMemRealloc(state, block, allocBytes);
}

// [3/27] site=0x00403740 slot=0x018F640C body=0x00466F20 section=.text
// Allocates and initializes the Lua value stack with 40 extra TObject slots (luaD_init).
void LuaDoInitStack(LuaState* state, int32_t stackSize) {
    const uint32_t allocBytes = static_cast<uint32_t>(stackSize + 40) << 4u;
    auto* stack = static_cast<LuaTObject*>(LuaMemRealloc(state, nullptr, allocBytes));
    const uint32_t usableBytes = static_cast<uint32_t>(stackSize) << 4u;
    state->nblocks += usableBytes;
    state->stackSize = stackSize;
    state->stack = stack;
    state->stackLast = reinterpret_cast<LuaTObject*>(
        reinterpret_cast<uint8_t*>(stack) + usableBytes - sizeof(LuaTObject));
    state->top = stack;
    state->Cbase = stack;
}

// [4/27] site=0x004038B0 slot=0x018FB698 body=0x004A4010 section=.text
// Checks stack capacity and triggers stack overflow handling when exceeded (luaD_checkstack).
// SecuROM rewrite: loads 1 from [0x018FC73C] when computing (stackSize - 1).
void LuaDoCheckStack(LuaState* state, int32_t needed) {
    const int32_t avail = static_cast<int32_t>(state->stackLast - state->top);
    if (avail > needed) {
        return;
    }
    const int32_t span = static_cast<int32_t>(state->stackLast - state->stack);
    if (span > state->stackSize - 1) {
        if (g_hooks.luaBreakRun != nullptr) {
            g_hooks.luaBreakRun(state, 5);
        }
        return;
    }
    state->stackLast = reinterpret_cast<LuaTObject*>(
        reinterpret_cast<uint8_t*>(state->stackLast) + 0x280);
    if (g_hooks.luaDoError != nullptr) {
        g_hooks.luaDoError(state, "stack overflow");
    }
    if (g_hooks.luaBreakRun != nullptr) {
        g_hooks.luaBreakRun(state, 1);
    }
}

// [5/27] site=0x00403CA0 slot=0x018FB0A4 body=0x004252F0 section=.text
// Protected function call wrapper (lua_call).
int32_t LuaProtectedCall(LuaState* state, int32_t nargs, int32_t nresults) {
    LuaTObject* func = state->top - (nargs + 1);
    LuaCallS callArgs{func, nresults};
    const int32_t status = (g_hooks.luaRunProtected != nullptr)
        ? g_hooks.luaRunProtected(state, 0x00403C80u, &callArgs)
        : 0;
    if (status != 0) {
        state->top = func;
    }
    return status;
}

// [6/27] site=0x00403EC0 slot=0x018FA148 body=0x00406550 section=.text
// Parses and executes an in-memory Lua chunk or precompiled bytecode buffer (lua_dobuffer).
// SecuROM rewrite: loads 0x00BD51B8 ("?") from [0x018FC47C] when name is null.
int32_t LuaDoBuffer(LuaState* state, const char* buff, uint32_t size, const char* name) {
    if (name == nullptr) {
        name = "?";
    }
    LuaZio z{};
    if (g_hooks.luaZioMemOpen != nullptr) {
        g_hooks.luaZioMemOpen(&z, buff, size, name);
    }
    const int32_t bin = (buff != nullptr && static_cast<uint8_t>(buff[0]) == 0x1Bu) ? 1 : 0;
    int32_t status = (g_hooks.luaProtectedParser != nullptr)
        ? g_hooks.luaProtectedParser(state, &z, bin)
        : 0;
    if (status == 0) {
        LuaTObject* func = state->top - 1;
        LuaCallS callArgs{func, -1};
        status = (g_hooks.luaRunProtected != nullptr)
            ? g_hooks.luaRunProtected(state, 0x00403C80u, &callArgs)
            : 0;
        if (status != 0) {
            state->top = func;
        }
    }
    return status;
}

// [7/27] site=0x00404370 slot=0x018F5E88 body=0x019149F0 section=.securom
// Verifies the current lexer token and advances to the next token (checknext).
// SecuROM rewrite: loads 0x00BD52AC ("`%.20s' expected") from [0x018FCB10] and resolves
// 0x0040B7B0 via inline arithmetic thunk at 0x01914A0C.
void LuaParserCheckNext(LuaLexState* ls, int32_t token) {
    if (ls->t.token != token) {
        char tokenBuf[32]{};
        char msgBuf[88]{};
        if (g_hooks.luaTokenToString != nullptr) {
            g_hooks.luaTokenToString(token, tokenBuf);
        }
        std::snprintf(msgBuf, sizeof(msgBuf), "`%.20s' expected", tokenBuf);
        if (g_hooks.luaLexError != nullptr) {
            g_hooks.luaLexError(ls, msgBuf);
        }
    }
    AdvanceLexerToken(ls);
}

// [8/27] site=0x004043F0 slot=0x018F7340 body=0x004A58A0 section=.text
// Consumes the current token if it matches `token` and returns 1, else 0 (optional).
// SecuROM rewrite: loads NO_TOKEN (0x11E) from [0x018FCDD0].
int32_t LuaParserOptional(LuaLexState* ls, int32_t token) {
    if (ls->t.token != token) {
        return 0;
    }
    AdvanceLexerToken(ls);
    return 1;
}

// [9/27] site=0x00404450 slot=0x018FB1B8 body=0x004A1440 section=.text
// Checks a closing delimiter token against its opening delimiter and line (check_match).
// SecuROM rewrite: loads 0x00BD52C0 ("`%.20s' expected (to close `%.20s' at line %d)") from [0x018FC6A4].
void LuaParserCheckMatch(LuaLexState* ls, int32_t what, int32_t who, int32_t where) {
    if (ls->t.token != what) {
        if (where == ls->linenumber) {
            if (g_hooks.luaErrorExpected != nullptr) {
                g_hooks.luaErrorExpected(ls, what);
            }
        } else {
            char whatBuf[24]{};
            char whoBuf[24]{};
            char msgBuf[96]{};
            if (g_hooks.luaTokenToString != nullptr) {
                g_hooks.luaTokenToString(what, whatBuf);
                g_hooks.luaTokenToString(who, whoBuf);
            }
            std::snprintf(
                msgBuf,
                sizeof(msgBuf),
                "`%.20s' expected (to close `%.20s' at line %d)",
                whatBuf,
                whoBuf,
                where);
            if (g_hooks.luaLexError != nullptr) {
                g_hooks.luaLexError(ls, msgBuf);
            }
        }
    }
    AdvanceLexerToken(ls);
}

// [10/27] site=0x00404550 slot=0x018F6120 body=0x00474880 section=.text
// Interns a string constant in the current function prototype's kstr table (stringK).
// SecuROM rewrite: loads MAXARG_U (0x03FFFFFF) from [0x018FCB60].
int32_t LuaParserStringK(LuaLexState* ls, LuaTString* str) {
    LuaFuncState* fs = ls->fs;
    LuaProto* f = fs->f;
    int32_t idx = str->u.s.constIndex;
    if (idx >= f->sizekstr || f->kstr[idx] != str) {
        f->kstr = static_cast<LuaTString**>(LuaMemGrowAux(
            fs->L,
            f->kstr,
            static_cast<uint32_t>(f->sizekstr),
            1,
            sizeof(LuaTString*),
            "constant table overflow",
            kMaxArgU));
        idx = f->sizekstr;
        f->sizekstr = idx + 1;
        f->kstr[idx] = str;
        str->u.s.constIndex = idx;
    }
    return (g_hooks.luaCodeKstr != nullptr) ? g_hooks.luaCodeKstr(ls, idx) : idx;
}

// [11/28] site=0x004048E0 slot=0x018FA584 body=0x00439CA0 section=.text
// Balances stack values for multiple assignment statements (adjust_mult_assign).
// SecuROM rewrite: loads 1 from [0x018FC4EC] and resolves 0x0040CA60 via 0x00439CD7.
void LuaParserAdjustMultAssign(LuaLexState* ls, int32_t nvars, int32_t nexps) {
    LuaFuncState* fs = ls->fs;
    int32_t diff = nexps - nvars;
    if (nexps > 0 && LuaCodeLastIsOpen(fs) != 0) {
        diff -= 1;
        if (diff <= 0) {
            LuaCodeSetCallReturns(fs, -diff);
            diff = 0;
        } else {
            LuaCodeSetCallReturns(fs, 0);
        }
    }
    LuaCodeAdjustStack(fs, diff);
}

// [12/27] site=0x00404AC0 slot=0x018FBEAC body=0x019026A0 section=.securom
// Finalizes a compiled function prototype, shrinking its arrays to exact size (close_func).
void LuaParserCloseFunc(LuaLexState* ls) {
    LuaFuncState* fs = ls->fs;
    LuaProto* f = fs->f;
    LuaState* L = ls->L;
    if (g_hooks.luaCode0 != nullptr) {
        g_hooks.luaCode0(fs, 0);
    }
    LuaCodeGetLabel(fs);
    f->code = static_cast<uint32_t*>(
        LuaMemRealloc(L, f->code, static_cast<uint32_t>(fs->pc) * sizeof(uint32_t)));
    f->kstr = static_cast<LuaTString**>(
        LuaMemRealloc(L, f->kstr, static_cast<uint32_t>(f->sizekstr) * sizeof(LuaTString*)));
    f->knum = static_cast<double*>(
        LuaMemRealloc(L, f->knum, static_cast<uint32_t>(f->sizeknum) * sizeof(double)));
    f->kproto = static_cast<LuaProto**>(
        LuaMemRealloc(L, f->kproto, static_cast<uint32_t>(f->sizekproto) * sizeof(LuaProto*)));
    if (g_hooks.luaRemoveLocalVars != nullptr) {
        g_hooks.luaRemoveLocalVars(ls, fs->nactvar);
    }
    f->locvars = static_cast<LuaLocVar*>(
        LuaMemRealloc(L, f->locvars, static_cast<uint32_t>(f->sizelocvars) * sizeof(LuaLocVar)));
    f->lineinfo = static_cast<int32_t*>(LuaMemRealloc(
        L,
        f->lineinfo,
        static_cast<uint32_t>(f->sizelineinfo + 1) * sizeof(int32_t)));
    f->lineinfo[f->sizelineinfo] = static_cast<int32_t>(kMaxIntMinus2);
    f->sizelineinfo += 1;
    if (g_hooks.luaProtoOk != nullptr) {
        g_hooks.luaProtoOk(L, f, fs->pc);
    }
    ls->fs = fs->prev;
}

// [13/27] site=0x00405B10 slot=0x018F71A0 body=0x01933830 section=.securom
// Parses a `function` declaration statement (funcstat).
// SecuROM rewrite: resolves 0x0040C380 via inline xor thunk at 0x0193388C.
void LuaParserFuncStat(LuaLexState* ls, int32_t line) {
    AdvanceLexerToken(ls);
    LuaExpDesc v{};
    const int32_t needself = (g_hooks.luaFuncName != nullptr) ? g_hooks.luaFuncName(ls, &v) : 0;
    if (g_hooks.luaBody != nullptr) {
        g_hooks.luaBody(ls, needself, line);
    }
    if (g_hooks.luaCodeStoreVar != nullptr) {
        g_hooks.luaCodeStoreVar(ls, &v);
    }
}

// [14/27] site=0x00406CF0 slot=0x018F5EE8 body=0x01915010 section=.securom
// Executes the VM line hook when stepping into a new source line or backward jump (traceexec).
// SecuROM rewrite: loads -32 (0xFFFFFFE0 = -2 * sizeof(LuaTObject)) from [0x018FCB24] and
// resolves 0x004039B0 via inline not/xor thunk at 0x019150CC.
void LuaVmTraceExec(LuaState* state, LuaTObject* top, uint32_t linehook) {
    LuaCallInfo* ci = (top - 1)->value.ci;
    auto* tf = *reinterpret_cast<LuaProto**>(ci->func);
    const int32_t pc = static_cast<int32_t>((*ci->pc - tf->code) - 1);
    if (pc == 0) {
        ci->line = 1;
        ci->refi = 0;
        ci->lastpc = 1;
    }
    const int32_t newline = (g_hooks.luaDebugGetLine != nullptr)
        ? g_hooks.luaDebugGetLine(tf->lineinfo, pc, ci->line, &ci->refi)
        : ci->line;
    if (newline != ci->line || pc <= ci->lastpc) {
        ci->line = newline;
        state->top = top;
        if (g_hooks.luaCallLineHook != nullptr) {
            g_hooks.luaCallLineHook(state, top - 2, newline, linehook);
        }
    }
    ci->lastpc = pc;
}

// [15/27] site=0x00406D70 slot=0x018F9FF4 body=0x00404ED0 section=.text
// Creates a closure with `nelems` upvalues popped from the top of the stack (luaV_closure).
// SecuROM rewrite: loads 1 from [0x018FC458] for luaD_checkstack(L, 1).
LuaClosure* LuaVmClosure(LuaState* state, int32_t nelems) {
    LuaClosure* c = (g_hooks.luaFuncNewClosure != nullptr)
        ? g_hooks.luaFuncNewClosure(state, nelems)
        : nullptr;
    state->top -= nelems;
    for (int32_t i = nelems - 1; i >= 0; --i) {
        c->upvalue[i] = state->top[i];
    }
    state->top->value.cl = c;
    state->top->ttype = kLuaTypeClosure;
    if (state->top == state->stackLast) {
        LuaDoCheckStack(state, 1);
    }
    state->top += 1;
    return c;
}

// [16/27] site=0x00408DB0 slot=0x018F6040 body=0x00467B40 section=.text
// Sweeps unmarked function prototypes from state->rootproto (collectproto).
void LuaGcCollectProto(LuaState* state) {
    LuaProto** p = &state->rootproto;
    while (LuaProto* curr = *p) {
        auto** markedSlot = reinterpret_cast<LuaProto**>(reinterpret_cast<uint8_t*>(curr) + 0x08);
        auto** nextSlot = reinterpret_cast<LuaProto**>(reinterpret_cast<uint8_t*>(curr) + 0x04);
        if (*markedSlot != curr) {
            *markedSlot = curr;
            p = nextSlot;
        } else {
            *p = *nextSlot;
            if (g_hooks.luaFuncFreeProto != nullptr) {
                g_hooks.luaFuncFreeProto(state, curr);
            }
        }
    }
}

// [17/27] site=0x00408DF0 slot=0x018FA02C body=0x00404950 section=.text
// Sweeps unmarked hash tables from state->roottable (collecttable).
void LuaGcCollectTable(LuaState* state) {
    LuaHash** p = &state->roottable;
    while (LuaHash* curr = *p) {
        if (curr->marked != curr) {
            curr->marked = curr;
            p = &curr->next;
        } else {
            *p = curr->next;
            if (g_hooks.luaTableFree != nullptr) {
                g_hooks.luaTableFree(state, curr);
            }
        }
    }
}

// [18/27] site=0x00408E30 slot=0x018FB34C body=0x004A2490 section=.text
// Sweeps unmarked strings from state->strt and shrinks the hash table if sparse (collectstrings).
// SecuROM rewrite: loads MINSTRTABSIZE (16) from [0x018FC6D0] and resolves 0x0040E3C0 via 0x004A2549.
void LuaGcCollectStrings(LuaState* state, int32_t all) {
    for (int32_t i = 0; i < state->strt.size; ++i) {
        LuaTString** p = &state->strt.hash[i];
        while (LuaTString* next = *p) {
            if (next->marked != 0 && all == 0) {
                if (next->marked < 2) {
                    next->marked = 0;
                }
                p = &next->nexthash;
            } else {
                *p = next->nexthash;
                state->strt.nuse -= 1u;
                state->nblocks -= (next->len + 21u);
                LuaMemRealloc(state, next, 0u);
            }
        }
    }
    if (state->strt.nuse < static_cast<uint32_t>(state->strt.size / 4) && state->strt.size > 16) {
        if (g_hooks.luaStringResize != nullptr) {
            g_hooks.luaStringResize(state, &state->strt, state->strt.size / 2);
        }
    }
}

// [19/27] site=0x00408ED0 slot=0x018FC3F0 body=0x01906B30 section=.securom
// Sweeps unmarked userdata from state->udt into per-tag collected lists (collectudata).
// SecuROM rewrite: loads MINSTRTABSIZE (16) from [0x018FC920].
void LuaGcCollectUdata(LuaState* state, int32_t all) {
    for (int32_t i = 0; i < state->udt.size; ++i) {
        LuaTString** p = &state->udt.hash[i];
        while (LuaTString* next = *p) {
            if (next->marked != 0 && all == 0) {
                next->marked = 0;
                p = &next->nexthash;
            } else {
                const int32_t tag = next->u.d.tag;
                *p = next->nexthash;
                next->nexthash = state->TMtable[tag].collected;
                state->TMtable[tag].collected = next;
                state->nblocks -= (next->len + 21u);
                state->udt.nuse -= 1u;
            }
        }
    }
    if (state->udt.nuse < static_cast<uint32_t>(state->udt.size / 4) && state->udt.size > 16) {
        if (g_hooks.luaStringResize != nullptr) {
            g_hooks.luaStringResize(state, &state->udt, state->udt.size / 2);
        }
    }
}

// [20/27] site=0x00409120 slot=0x018FAE84 body=0x0049F310 section=.text
// Marks all reachable objects starting from globals, stack, locked refs, closures, and tables (markall).
void LuaGcMarkAll(LuaState* state) {
    LuaStkMark st{};
    st.tmark = state->gt;
    state->gt->marked = nullptr;
    st.cmark = nullptr;
    if (g_hooks.luaGcMarkLock != nullptr) {
        g_hooks.luaGcMarkLock(state, &st);
    }
    for (LuaTObject* o = state->stack; o < state->top; ++o) {
        LuaGcMarkObject(&st, o);
    }
    for (int32_t i = 0; i < state->refSize; ++i) {
        if (state->refArray[i].status == -4) {
            LuaGcMarkObject(&st, &state->refArray[i].o);
        }
    }
    for (;;) {
        while (st.cmark != nullptr) {
            LuaClosure* f = st.cmark;
            st.cmark = f->mark;
            for (int32_t i = 0; i < f->nelems; ++i) {
                LuaGcMarkObject(&st, &f->upvalue[i]);
            }
        }
        if (st.tmark == nullptr) {
            break;
        }
        LuaHash* h = st.tmark;
        st.tmark = h->marked;
        for (int32_t i = 0; i < h->size; ++i) {
            LuaNode* n = &h->node[i];
            if (n->key.ttype != kLuaTypeNil) {
                if (n->val.ttype == kLuaTypeNil) {
                    if (g_hooks.luaTableRemove != nullptr) {
                        g_hooks.luaTableRemove(h, n);
                    }
                }
                LuaGcMarkObject(&st, &n->key);
                LuaGcMarkObject(&st, &n->val);
            }
        }
    }
}

// [21/27] site=0x00409E90 slot=0x018FC3D0 body=0x00443DC0 section=.text
// Allocates and zeroes a new tag method table entry and increments state->lastTag (lua_newtag).
int32_t LuaNewTag(LuaState* state) {
    state->TMtable = static_cast<LuaTM*>(LuaMemGrowAux(
        state,
        state->TMtable,
        static_cast<uint32_t>(state->lastTag),
        1,
        sizeof(LuaTM),
        "tag table overflow",
        kMaxIntMinus2));
    state->lastTag += 1;
    state->nblocks += sizeof(LuaTM);
    std::memset(&state->TMtable[state->lastTag], 0, sizeof(LuaTM));
    return state->lastTag;
}

// [22/27] site=0x0040A070 slot=0x018F5B20 body=0x0043B020 section=.text
// Returns the effective tag of a Lua TObject (luaT_tag).
// SecuROM rewrite: loads LUA_TTABLE (4) from [0x018FCA78].
int32_t LuaTagOfObject(const LuaTObject* o) {
    const int32_t t = o->ttype;
    if (t == kLuaTypeUserdata) {
        return o->value.ts->u.d.tag;
    }
    if (t == kLuaTypeTable) {
        return o->value.ht->htag;
    }
    return t;
}

// [23/27] site=0x0040A120 slot=0x018FAEB8 body=0x0049F650 section=.text
// Validates and installs a tag method from the top of the Lua stack (lua_settagmethod).
// SecuROM rewrite: loads 0x00BD5AD4 ("%d is not a valid tag") from [0x018FC62C].
void LuaSetTagMethod(LuaState* state, int32_t tag, const char* event) {
    const int32_t e = (g_hooks.luaCheckEvent != nullptr) ? g_hooks.luaCheckEvent(event) : 0;
    if (tag < 0 || tag > state->lastTag) {
        if (g_hooks.luaVError != nullptr) {
            g_hooks.luaVError(state, "%d is not a valid tag", nullptr, nullptr, nullptr, tag);
        }
    }
    if (tag < 7 && kLuaValidEvents[tag][e] == 0) {
        const char* suffix = (tag == kLuaTypeTable || tag == kLuaTypeUserdata)
            ? " with default tag"
            : "";
        if (g_hooks.luaVError != nullptr) {
            g_hooks.luaVError(
                state,
                "cannot change `%.20s' tag method for type `%.20s'%.20s",
                kLuaEventNames[e],
                kLuaTypeNames[tag],
                suffix,
                0);
        }
    }
    LuaTObject* topVal = state->top - 1;
    if (topVal->ttype == kLuaTypeNil) {
        state->TMtable[tag].method[e] = nullptr;
    } else if (topVal->ttype == kLuaTypeClosure) {
        state->TMtable[tag].method[e] = topVal->value.cl;
    } else {
        if (g_hooks.luaError != nullptr) {
            g_hooks.luaError(state, "tag method must be a function (or nil)");
        }
    }
    state->top -= 1;
}

// [24/44] site=0x0040A340 slot=0x018FC148 body=0x004431E0 section=.text
// Pushes a copy of the stack element at `index` onto the top of the stack (lua_pushvalue).
// SecuROM rewrite: loads 1 from [0x018FC8D0] for luaD_checkstack(L, 1).
void LuaPushValue(LuaState* state, int32_t index) {
    const LuaTObject* src = (index >= 0) ? (state->Cbase + (index - 1)) : (state->top + index);
    *state->top = *src;
    if (state->top == state->stackLast) {
        LuaDoCheckStack(state, 1);
    }
    state->top += 1;
}

namespace {

// Resolves a 1-based positive or top-relative negative stack index with bounds check (luaA_indexAcceptable).
LuaTObject* ResolveAcceptableIndex(LuaState* state, int32_t index) {
    if (index >= 0) {
        LuaTObject* o = state->Cbase + (index - 1);
        if (o >= state->top) {
            return nullptr;
        }
        return o;
    }
    return state->top + index;
}

struct BinOprCodeEntry {
    int32_t opcode;
    int32_t arg;
};

// Binary operator descriptor table at 0x00BD5EE8 (observed in 0x0040E220 body 0x004A2BD0).
constexpr BinOprCodeEntry kBinOprTable[12] = {
    {24, 0},   // OPR_ADD -> OP_ADD
    {26, 0},   // OPR_SUB -> OP_SUB
    {27, 0},   // OPR_MULT -> OP_MULT
    {28, 0},   // OPR_DIV -> OP_DIV
    {29, 0},   // OPR_POW -> OP_POW
    {30, 2},   // OPR_CONCAT -> OP_CONCAT (arg=2)
    {33, -1},  // OPR_NE -> OP_JMPNE (NO_JUMP=-1)
    {34, -1},  // OPR_EQ -> OP_JMPEQ (NO_JUMP=-1)
    {35, -1},  // OPR_LT -> OP_JMPLT (NO_JUMP=-1)
    {36, -1},  // OPR_LE -> OP_JMPLE (NO_JUMP=-1)
    {37, -1},  // OPR_GT -> OP_JMPGT (NO_JUMP=-1)
    {38, -1},  // OPR_GE -> OP_JMPGE (NO_JUMP=-1)
};

}  // namespace

// [25/44] site=0x0040A440 slot=0x018FB260 body=0x0041AA80 section=.text
// Returns the custom array/table count or hash aux word at +0x0C if stack[index] is a table (lua_getn / table aux).
int32_t LuaGetN(LuaState* state, int32_t index) {
    const LuaTObject* o = ResolveAcceptableIndex(state, index);
    if (o == nullptr || o->ttype != kLuaTypeTable) {
        return 0;
    }
    return static_cast<int32_t>(o->value.ht->pad0C);
}

// [26/44] site=0x0040A5C0 slot=0x018FB380 body=0x00426E30 section=.text
// Converts stack[index] to a boolean truth value (0 for nil/out-of-bounds, raw value for tag 6, 1 otherwise).
int32_t LuaIsFunction(LuaState* state, int32_t index) {
    const LuaTObject* o = ResolveAcceptableIndex(state, index);
    if (o == nullptr || o->ttype == kLuaTypeNil) {
        return 0;
    }
    if (o->ttype == 6) {
        return static_cast<int32_t>(o->value.raw);
    }
    return 1;
}

// [27/44] site=0x0040A600 slot=0x018F50B8 body=0x004461A0 section=.text
// Returns the C string pointer for stack[index], converting numbers in place via luaV_tostring (lua_tostring).
const char* LuaToString(LuaState* state, int32_t index) {
    LuaTObject* o = ResolveAcceptableIndex(state, index);
    if (o == nullptr) {
        return nullptr;
    }
    if (o->ttype != kLuaTypeString) {
        if (g_hooks.luaVmToString == nullptr || g_hooks.luaVmToString(state, o) != 0) {
            return nullptr;
        }
    }
    return o->value.ts->str;
}

// [28/44] site=0x0040A860 slot=0x018F6608 body=0x0191B790 section=.securom
// Pushes a userdata object with explicit tag onto the Lua stack (lua_pushusertag).
// SecuROM rewrite: loads LUA_NTAGS (6) from [0x018FCBF8].
void LuaPushUserTag(LuaState* state, void* udata, int32_t tag) {
    if (tag != -1 && tag != 0 && (tag < 6 || tag > state->lastTag)) {
        if (g_hooks.luaVError != nullptr) {
            g_hooks.luaVError(
                state,
                "invalid tag for a userdata (%d)",
                nullptr,
                nullptr,
                nullptr,
                tag);
        }
    }
    state->top->value.ts = (g_hooks.luaStringCreateUdata != nullptr)
        ? g_hooks.luaStringCreateUdata(state, udata, tag)
        : nullptr;
    state->top->ttype = kLuaTypeUserdata;
    if (state->top == state->stackLast) {
        LuaDoCheckStack(state, 1);
    }
    state->top += 1;
}

// [29/44] site=0x0040A8C0 slot=0x018FB6F4 body=0x0043E3B0 section=.text
// Pushes the value of global variable `name` onto the Lua stack (lua_getglobal).
// SecuROM rewrite: loads 1 from [0x018FC748] for luaD_checkstack(L, 1).
void LuaGetGlobal(LuaState* state, const char* name) {
    LuaTObject* topSlot = state->top;
    LuaTString* str = (g_hooks.luaStringNew != nullptr) ? g_hooks.luaStringNew(state, name) : nullptr;
    const LuaTObject* val = (g_hooks.luaVmGetGlobal != nullptr) ? g_hooks.luaVmGetGlobal(state, str) : nullptr;
    if (val != nullptr) {
        *topSlot = *val;
    }
    state->top = topSlot;
    if (topSlot == state->stackLast) {
        LuaDoCheckStack(state, 1);
    }
    state->top += 1;
}

// [30/44] site=0x0040AAD0 slot=0x018F5C50 body=0x0045EDC0 section=.text
// Creates an empty table via LuaTableNew(L, 0, aux) and pushes it onto the stack (lua_newtable).
// SecuROM rewrite: loads 1 from [0x018FCAB8] for luaD_checkstack(L, 1).
void LuaNewTable(LuaState* state, uint32_t aux) {
    LuaHash* h = LuaTableNew(state, 0, aux);
    state->top->value.ht = h;
    state->top->ttype = kLuaTypeTable;
    if (state->top == state->stackLast) {
        LuaDoCheckStack(state, 1);
    }
    state->top += 1;
}

// [31/44] site=0x0040AEF0 slot=0x018FB394 body=0x0043D6E0 section=.text
// Allocates a managed userdata block of `size` bytes, pushes it, and returns its payload pointer (lua_newuserdata).
// SecuROM rewrite: loads 0 from [0x018FC6D8].
void* LuaNewUserData(LuaState* state, uint32_t size) {
    LuaTString* ts = (g_hooks.luaStringNewUdata != nullptr)
        ? g_hooks.luaStringNewUdata(state, size, nullptr)
        : nullptr;
    state->top->value.ts = ts;
    state->top->ttype = kLuaTypeUserdata;
    if (state->top == state->stackLast) {
        LuaDoCheckStack(state, 1);
    }
    state->top += 1;
    return (ts != nullptr) ? ts->u.d.value : nullptr;
}

// [32/44] site=0x0040B410 slot=0x018F603C body=0x00470720 section=.text
// Allocates and initializes a new LuaHash table with `nhash` initial capacity (luaH_new).
LuaHash* LuaTableNew(LuaState* state, int32_t nhash, uint32_t aux) {
    auto* t = static_cast<LuaHash*>(LuaMemRealloc(state, nullptr, sizeof(LuaHash)));
    t->htag = kLuaTypeTable;
    t->next = state->roottable;
    state->roottable = t;
    t->marked = t;
    t->size = 0;
    t->pad0C = aux;
    state->nblocks += sizeof(LuaHash);
    t->node = nullptr;
    const uint32_t powSize = LuaPower2(static_cast<uint32_t>(nhash));
    if (g_hooks.luaTableSetNodeVector != nullptr) {
        g_hooks.luaTableSetNodeVector(state, t, powSize);
    }
    return t;
}

// [33/44] site=0x0040B570 slot=0x018FC3E8 body=0x019069F0 section=.securom
// Finds or inserts `key` in table `t` and returns a pointer to its value slot (luaH_set).
LuaTObject* LuaTableSet(LuaState* state, LuaHash* t, const LuaTObject* key) {
    for (;;) {
        LuaNode* mp = (g_hooks.luaTableMainPosition != nullptr)
            ? g_hooks.luaTableMainPosition(t, key)
            : nullptr;
        if (mp == nullptr) {
            if (g_hooks.luaError != nullptr) {
                g_hooks.luaError(state, "table index is nil");
            }
            return nullptr;
        }
        LuaNode* n = mp;
        do {
            if (LuaEqualObj(key, &n->key) != 0) {
                return &n->val;
            }
            n = n->next;
        } while (n != nullptr);

        if (mp->key.ttype != kLuaTypeNil) {
            LuaNode* freepos = t->firstfree;
            LuaNode* other = (mp > freepos && g_hooks.luaTableMainPosition != nullptr)
                ? g_hooks.luaTableMainPosition(t, &mp->key)
                : mp;
            if (other != mp) {
                while (other->next != mp) {
                    other = other->next;
                }
                other->next = freepos;
                *freepos = *mp;
                mp->next = nullptr;
            } else {
                freepos->next = mp->next;
                mp->next = freepos;
                mp = freepos;
            }
        }
        mp->key = *key;
        for (;;) {
            if (t->firstfree->key.ttype == kLuaTypeNil) {
                return &mp->val;
            }
            if (t->firstfree == t->node) {
                break;
            }
            t->firstfree -= 1;
        }
        if (g_hooks.luaTableRehash != nullptr) {
            g_hooks.luaTableRehash(state, t);
        }
    }
}

// [34/44] site=0x0040C920 slot=0x018F5474 body=0x0045A730 section=.text
// Patches the signed jump displacement of instruction `pc` to target `dest` (luaK_fixjump).
// SecuROM rewrite: loads 0x3F from [0x018FC988] and rotates [0x018F76A8] by [0x0052EF45] to 0x00BD5FE0.
void LuaCodeFixJump(LuaFuncState* fs, int32_t pc, int32_t dest) {
    uint32_t& jmp = fs->f->code[pc];
    if (dest == -1) {
        jmp = (jmp & 0x3Fu) | 0x7FFFFF80u;
        return;
    }
    const int32_t offset = dest - (pc + 1);
    const int32_t absOffset = (offset < 0) ? -offset : offset;
    if (absOffset > 0x01FFFFFF) {
        if (g_hooks.luaSyntaxError != nullptr && fs->ls != nullptr) {
            g_hooks.luaSyntaxError(
                fs->ls,
                "control structure too long",
                reinterpret_cast<const char*>(static_cast<uintptr_t>(fs->ls->t.token)));
        }
    }
    jmp = (jmp & 0x3Fu) | (static_cast<uint32_t>(offset + 0x01FFFFFF) << 6u);
}

// [35/44] site=0x0040C980 slot=0x018F6788 body=0x0047AA50 section=.text
// Advances `fs->freereg` by `n` and updates `fs->f->maxstacksize` (luaK_deltastack).
// SecuROM rewrite: loads 0x00BD5FFC ("function or expression too complex") from [0x018FCC14].
void LuaCodeCheckStack(LuaFuncState* fs, int32_t n) {
    fs->freereg = static_cast<int16_t>(fs->freereg + n);
    const uint16_t cur = static_cast<uint16_t>(fs->freereg);
    if (static_cast<int16_t>(cur) > fs->f->maxstacksize) {
        if (static_cast<int16_t>(cur) > 250) {
            if (g_hooks.luaSyntaxError != nullptr && fs->ls != nullptr) {
                g_hooks.luaSyntaxError(
                    fs->ls,
                    "function or expression too complex",
                    reinterpret_cast<const char*>(static_cast<uintptr_t>(fs->ls->t.token)));
            }
        }
        fs->f->maxstacksize = static_cast<int16_t>(cur);
    }
}

// [36/44] site=0x0040CA60 slot=0x018FBEFC body=0x00435790 section=.text
// Checks whether the previous instruction is an open OP_CALL with MULT_RET (luaK_lastisopen).
// SecuROM rewrite: loads MASK_B (0x00003FC0) from [0x018FC864].
int32_t LuaCodeLastIsOpen(const LuaFuncState* fs) {
    const uint32_t inst = (fs->pc > fs->lasttarget) ? fs->f->code[fs->pc - 1] : 0u;
    if ((inst & 0x3Fu) == 2u && (inst & 0x3FC0u) == 0x3FC0u) {
        return 1;
    }
    return 0;
}

// [37/44] site=0x0040CAA0 slot=0x018F5C84 body=0x01913030 section=.securom
// Patches the B-argument of an open OP_CALL instruction to `nresults` and updates freereg (luaK_setcallreturns).
// SecuROM rewrite: loads MASK_B (0x00003FC0) from [0x018FCAC0].
void LuaCodeSetCallReturns(LuaFuncState* fs, int32_t nresults) {
    const uint32_t inst = (fs->pc > fs->lasttarget) ? fs->f->code[fs->pc - 1] : 0u;
    if ((inst & 0x3Fu) == 2u && (inst & 0x3FC0u) == 0x3FC0u) {
        uint32_t& slot = fs->f->code[fs->pc - 1];
        slot = (slot & 0xFFFF803Fu) | (static_cast<uint32_t>(nresults) << 6u);
        LuaCodeCheckStack(fs, nresults);
    }
}

// [38/44] site=0x0040D220 slot=0x018FC078 body=0x0042CE50 section=.text
// Emits a 1-argument instruction via luaK_code2(fs, op, arg1, 0) (luaK_code1).
int32_t LuaCode1(LuaFuncState* fs, int32_t op, int32_t arg1) {
    return (g_hooks.luaCode2 != nullptr) ? g_hooks.luaCode2(fs, op, arg1, 0) : 0;
}

// [39/44] site=0x0040D2F0 slot=0x018F725C body=0x004975D0 section=.text
// Marks the current pc as a jump target and patches pending jumps in fs->jpc (luaK_getlabel).
// SecuROM rewrite: loads NO_REG (0xFF) from [0x018FCDB0].
int32_t LuaCodeGetLabel(LuaFuncState* fs) {
    if (fs->pc != fs->lasttarget) {
        const int32_t oldTarget = fs->lasttarget;
        const int32_t jpc = fs->jpc;
        fs->lasttarget = fs->pc;
        if (g_hooks.luaCodePatchListAux != nullptr) {
            g_hooks.luaCodePatchListAux(fs, jpc, oldTarget, 0, 0xFF);
        }
        fs->jpc = -1;
    }
    return fs->pc;
}

// [40/44] site=0x0040D5B0 slot=0x018FC418 body=0x01907050 section=.securom
// Emits or coalesces OP_POP / OP_PUSHNIL to adjust the compile-time stack by `n` (luaK_adjuststack).
// SecuROM rewrite: loads MASK_OP (0x3F) from [0x018FC924].
void LuaCodeAdjustStack(LuaFuncState* fs, int32_t n) {
    if (n > 0) {
        uint32_t inst = (fs->pc > fs->lasttarget) ? fs->f->code[fs->pc - 1] : 0u;
        bool coalesced = false;
        if ((inst & 0x3Fu) == 0x15u) {
            const uint32_t arg = ((inst >> 6u) & 0x1FFu) + static_cast<uint32_t>(n);
            inst = (inst & 0xFFFF803Fu) | (arg << 6u);
            coalesced = true;
        }
        fs->freereg = static_cast<int16_t>(fs->freereg - n);
        const uint16_t cur = static_cast<uint16_t>(fs->freereg);
        if (static_cast<int16_t>(cur) > fs->f->maxstacksize) {
            if (static_cast<int16_t>(cur) > 250) {
                if (g_hooks.luaSyntaxError != nullptr && fs->ls != nullptr) {
                    g_hooks.luaSyntaxError(
                        fs->ls,
                        "function or expression too complex",
                        reinterpret_cast<const char*>(static_cast<uintptr_t>(fs->ls->t.token)));
                }
            }
            fs->f->maxstacksize = static_cast<int16_t>(cur);
        }
        if (coalesced) {
            fs->f->code[fs->pc - 1] = inst;
            return;
        }
        const uint32_t popInst = (static_cast<uint32_t>(n) << 6u) | 5u;
        if (g_hooks.luaCodeRememberLineInfo != nullptr) {
            g_hooks.luaCodeRememberLineInfo(fs);
        }
        fs->f->code = static_cast<uint32_t*>(LuaMemGrowAux(
            fs->L,
            fs->f->code,
            static_cast<uint32_t>(fs->pc),
            1,
            sizeof(uint32_t),
            "code size overflow",
            kMaxIntMinus2));
        fs->f->code[fs->pc] = popInst;
        fs->pc += 1;
    } else {
        const int32_t pushCount = -n;
        uint32_t inst = (fs->pc > fs->lasttarget) ? fs->f->code[fs->pc - 1] : 0u;
        bool coalesced = false;
        if (pushCount == 0) {
            return;
        }
        if ((inst & 0x3Fu) == 4u) {
            inst = (((inst >> 6u) + static_cast<uint32_t>(pushCount)) << 6u) | 4u;
            coalesced = true;
        }
        fs->freereg = static_cast<int16_t>(fs->freereg + pushCount);
        const uint16_t cur = static_cast<uint16_t>(fs->freereg);
        if (static_cast<int16_t>(cur) > fs->f->maxstacksize) {
            if (static_cast<int16_t>(cur) > 250) {
                if (g_hooks.luaSyntaxError != nullptr && fs->ls != nullptr) {
                    g_hooks.luaSyntaxError(
                        fs->ls,
                        "function or expression too complex",
                        reinterpret_cast<const char*>(static_cast<uintptr_t>(fs->ls->t.token)));
                }
            }
            fs->f->maxstacksize = static_cast<int16_t>(cur);
        }
        if (coalesced) {
            fs->f->code[fs->pc - 1] = inst;
            return;
        }
        const uint32_t pushNilInst = (static_cast<uint32_t>(pushCount) << 6u) | 4u;
        if (g_hooks.luaCodeRememberLineInfo != nullptr) {
            g_hooks.luaCodeRememberLineInfo(fs);
        }
        fs->f->code = static_cast<uint32_t*>(LuaMemGrowAux(
            fs->L,
            fs->f->code,
            static_cast<uint32_t>(fs->pc),
            1,
            sizeof(uint32_t),
            "code size overflow",
            kMaxIntMinus2));
        fs->f->code[fs->pc] = pushNilInst;
        fs->pc += 1;
    }
}

// [41/44] site=0x0040DC40 slot=0x018FB15C body=0x0040B6E0 section=.text
// Emits a unary jump-on-false or jump-on-true via codearith(fs, v, 1, op ? 42 : 40).
void LuaCodeUnary(LuaFuncState* fs, LuaExpDesc* v, int32_t op) {
    const int32_t opcode = (op != 0) ? 42 : 40;
    if (g_hooks.luaCodeArith != nullptr) {
        g_hooks.luaCodeArith(fs, v, 1, opcode);
    }
}

// [42/44] site=0x0040E1D0 slot=0x018F5C74 body=0x0045EF60 section=.text
// Emits the LHS jump or value evaluation before a binary operator's RHS is parsed (luaK_infix).
// SecuROM rewrite: loads 1 from [0x018FCABC] to distinguish OPR_OR (13) from OPR_AND (12).
void LuaCodeInfix(LuaLexState* ls, int32_t op, LuaExpDesc* v) {
    if (op == 12) {
        if (g_hooks.luaCodeArith != nullptr) {
            g_hooks.luaCodeArith(ls->fs, v, 1, 42);
        }
    } else if (op == 13) {
        if (g_hooks.luaCodeArith != nullptr) {
            g_hooks.luaCodeArith(ls->fs, v, 0, 41);
        }
    } else {
        if (g_hooks.luaCodePrefix != nullptr) {
            g_hooks.luaCodePrefix(ls, v, 1);
        }
    }
}

// [43/44] site=0x0040E220 slot=0x018FB3A0 body=0x004A2BD0 section=.text
// Finalizes a binary operator expression after its RHS `v2` is parsed (luaK_posfix).
// SecuROM rewrite: loads 1 from [0x018FC6DC] to distinguish OPR_OR (13) from OPR_AND (12).
void LuaCodePosfix(LuaLexState* ls, int32_t op, LuaExpDesc* v1, LuaExpDesc* v2) {
    LuaFuncState* fs = ls->fs;
    if (op == 12) {
        if (g_hooks.luaCodeExp2Val != nullptr) {
            g_hooks.luaCodeExp2Val(fs, v2);
        }
        if (v2->t == -1 && v2->f == -1) {
            LuaCodeSetCallReturns(fs, 1);
        }
        v1->t = v2->t;
        if (g_hooks.luaCodeConcat != nullptr) {
            g_hooks.luaCodeConcat(fs, &v1->f, v2->f);
        }
    } else if (op == 13) {
        if (g_hooks.luaCodeExp2Val != nullptr) {
            g_hooks.luaCodeExp2Val(fs, v2);
        }
        if (v2->t == -1 && v2->f == -1) {
            LuaCodeSetCallReturns(fs, 1);
        }
        v1->f = v2->f;
        if (g_hooks.luaCodeConcat != nullptr) {
            g_hooks.luaCodeConcat(fs, &v1->t, v2->t);
        }
    } else {
        if (g_hooks.luaCodePrefix != nullptr) {
            g_hooks.luaCodePrefix(ls, v2, 1);
        }
        const BinOprCodeEntry& entry = kBinOprTable[op];
        if (g_hooks.luaCode2 != nullptr) {
            g_hooks.luaCode2(fs, entry.opcode, entry.arg, 0);
        }
    }
}

// [44/44] site=0x0040E2F0 slot=0x018F4FDC body=0x0044DE30 section=.text
// Allocates initial 1-bucket hash tables for `L->strt` and `L->udt` (luaS_init).
void LuaStringInit(LuaState* state) {
    state->strt.hash = static_cast<LuaTString**>(LuaMemRealloc(state, nullptr, sizeof(LuaTString*)));
    state->udt.hash = static_cast<LuaTString**>(LuaMemRealloc(state, nullptr, sizeof(LuaTString*)));
    state->nblocks += 2 * sizeof(LuaTString*);
    state->udt.size = 1;
    state->strt.size = 1;
    state->udt.nuse = 0;
    state->strt.nuse = 0;
    state->udt.hash[0] = nullptr;
    state->strt.hash[0] = nullptr;
}

}  // namespace ra3::core
