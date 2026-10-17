// Differential oracle harness for the Stage-1 Embedded Lua 4.0 VM/Compiler/GC
// entry-lost batch (27 functions in 0x00403660..0x0040D5B0).

#include "ra3/core/LuaVmPorts.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>

namespace {

using namespace ra3::core;

constexpr uint32_t kArenaBase = 0x22000000u;

struct TraceState {
    uint32_t traceHash = 0x811C9DC5u;
    uint32_t traceCalls = 0;
    uint32_t lastArg0 = 0;
    uint32_t lastArg1 = 0;
    uint32_t lastArg2 = 0;
    uint32_t lastArg3 = 0;
    char lastMsg[128]{};
    uint32_t bumpCursor = kArenaBase + 0x00020000u;
    uint8_t* arenaBase = nullptr;
    int32_t stubRet = 0;
    int32_t lexNextToken = 0;
    uint32_t lexNextSeminfoLo = 0;
    uint32_t lexNextSeminfoHi = 0;
};

TraceState g_trace{};

void MixWord(uint32_t v) {
    g_trace.traceHash ^= v;
    g_trace.traceHash *= 0x01000193u;
}

void MixString(const char* s) {
    if (s == nullptr) {
        MixWord(0xFFFFFFFFu);
        return;
    }
    std::snprintf(g_trace.lastMsg, sizeof(g_trace.lastMsg), "%s", s);
    for (const uint8_t* p = reinterpret_cast<const uint8_t*>(s); *p != 0; ++p) {
        MixWord(*p);
    }
}

uint32_t ToSimVa(const void* ptr) {
    if (ptr == nullptr || g_trace.arenaBase == nullptr) {
        return 0u;
    }
    const uintptr_t p = reinterpret_cast<uintptr_t>(ptr);
    const uintptr_t base = reinterpret_cast<uintptr_t>(g_trace.arenaBase);
    if (p >= base && p < base + 0x00040000u) {
        return kArenaBase + static_cast<uint32_t>(p - base);
    }
    return static_cast<uint32_t>(p);
}

void* FromSimVa(uint32_t va) {
    if (va == 0u || g_trace.arenaBase == nullptr) {
        return nullptr;
    }
    if (va >= kArenaBase && va < kArenaBase + 0x00040000u) {
        return g_trace.arenaBase + (va - kArenaBase);
    }
    return reinterpret_cast<void*>(static_cast<uintptr_t>(va));
}

void* StubRealloc(void* ptr, uint32_t size) {
    g_trace.traceCalls += 1;
    g_trace.lastArg0 = ToSimVa(ptr);
    g_trace.lastArg1 = size;
    MixWord(0xCA110001u);
    MixWord(g_trace.lastArg0);
    MixWord(size);
    if (g_trace.stubRet == -999) {
        return nullptr;
    }
    const uint32_t outVa = g_trace.bumpCursor;
    const uint32_t aligned = (size + 15u) & ~15u;
    g_trace.bumpCursor += (aligned > 0u ? aligned : 16u);
    return FromSimVa(outVa);
}

void StubFree(void* ptr) {
    g_trace.traceCalls += 1;
    g_trace.lastArg0 = ToSimVa(ptr);
    MixWord(0xCA110002u);
    MixWord(g_trace.lastArg0);
}

void StubLuaError(LuaState* state, const char* msg) {
    g_trace.traceCalls += 1;
    g_trace.lastArg0 = ToSimVa(state);
    MixWord(0xCA110003u);
    MixWord(g_trace.lastArg0);
    MixString(msg);
}

void StubBreakRun(LuaState* state, int32_t errcode) {
    g_trace.traceCalls += 1;
    g_trace.lastArg0 = ToSimVa(state);
    g_trace.lastArg1 = static_cast<uint32_t>(errcode);
    MixWord(0xCA110004u);
    MixWord(g_trace.lastArg0);
    MixWord(g_trace.lastArg1);
}

void StubDoError(LuaState* state, const char* msg) {
    g_trace.traceCalls += 1;
    g_trace.lastArg0 = ToSimVa(state);
    MixWord(0xCA110005u);
    MixWord(g_trace.lastArg0);
    MixString(msg);
}

int32_t StubRunProtected(LuaState* state, uint32_t funcVa, LuaCallS* ud) {
    g_trace.traceCalls += 1;
    g_trace.lastArg0 = ToSimVa(state);
    g_trace.lastArg1 = funcVa;
    g_trace.lastArg2 = ToSimVa(ud->func);
    g_trace.lastArg3 = static_cast<uint32_t>(ud->nresults);
    MixWord(0xCA110006u);
    MixWord(g_trace.lastArg0);
    MixWord(g_trace.lastArg1);
    MixWord(g_trace.lastArg2);
    MixWord(g_trace.lastArg3);
    return g_trace.stubRet;
}

LuaZio* StubZioMemOpen(LuaZio* z, const char* buff, uint32_t size, const char* name) {
    g_trace.traceCalls += 1;
    g_trace.lastArg0 = ToSimVa(buff);
    g_trace.lastArg1 = size;
    MixWord(0xCA110007u);
    MixWord(g_trace.lastArg0);
    MixWord(size);
    MixString(name);
    z->words[0] = size;
    return z;
}

int32_t StubProtectedParser(LuaState* state, LuaZio* z, int32_t bin) {
    g_trace.traceCalls += 1;
    g_trace.lastArg0 = ToSimVa(state);
    g_trace.lastArg1 = z->words[0];
    g_trace.lastArg2 = static_cast<uint32_t>(bin);
    MixWord(0xCA110008u);
    MixWord(g_trace.lastArg0);
    MixWord(g_trace.lastArg1);
    MixWord(g_trace.lastArg2);
    return (g_trace.stubRet >= 100) ? (g_trace.stubRet - 100) : 0;
}

void StubTokenToString(int32_t token, char* outBuf) {
    g_trace.traceCalls += 1;
    MixWord(0xCA110009u);
    MixWord(static_cast<uint32_t>(token));
    std::snprintf(outBuf, 20, "tok_%d", token);
}

void StubLexError(LuaLexState* ls, const char* msg) {
    g_trace.traceCalls += 1;
    g_trace.lastArg0 = ToSimVa(ls);
    MixWord(0xCA11000Au);
    MixWord(g_trace.lastArg0);
    MixString(msg);
}

void StubSyntaxError(LuaLexState* ls, const char* msg, const char* token) {
    g_trace.traceCalls += 1;
    g_trace.lastArg0 = ToSimVa(ls);
    g_trace.lastArg1 = static_cast<uint32_t>(reinterpret_cast<uintptr_t>(token));
    MixWord(0xCA11000Bu);
    MixWord(g_trace.lastArg0);
    MixWord(g_trace.lastArg1);
    MixString(msg);
}

int32_t StubLex(LuaLexState* ls, LuaSemInfo* seminfo) {
    g_trace.traceCalls += 1;
    g_trace.lastArg0 = ToSimVa(ls);
    MixWord(0xCA11000Cu);
    MixWord(g_trace.lastArg0);
    auto* words = reinterpret_cast<uint32_t*>(seminfo);
    words[0] = g_trace.lexNextSeminfoLo;
    words[1] = g_trace.lexNextSeminfoHi;
    return g_trace.lexNextToken;
}

void StubErrorExpected(LuaLexState* ls, int32_t token) {
    g_trace.traceCalls += 1;
    g_trace.lastArg0 = ToSimVa(ls);
    g_trace.lastArg1 = static_cast<uint32_t>(token);
    MixWord(0xCA11000Du);
    MixWord(g_trace.lastArg0);
    MixWord(g_trace.lastArg1);
}

int32_t StubCodeKstr(LuaLexState* ls, int32_t idx) {
    g_trace.traceCalls += 1;
    g_trace.lastArg0 = ToSimVa(ls);
    g_trace.lastArg1 = static_cast<uint32_t>(idx);
    MixWord(0xCA11000Eu);
    MixWord(g_trace.lastArg0);
    MixWord(g_trace.lastArg1);
    return idx + 1000;
}

void StubCodeNil(LuaLexState* ls, int32_t n) {
    g_trace.traceCalls += 1;
    g_trace.lastArg0 = ToSimVa(ls);
    g_trace.lastArg1 = static_cast<uint32_t>(n);
    MixWord(0xCA11000Fu);
    MixWord(g_trace.lastArg0);
    MixWord(g_trace.lastArg1);
}

int32_t StubCode0(LuaFuncState* fs, int32_t op) {
    g_trace.traceCalls += 1;
    g_trace.lastArg0 = ToSimVa(fs);
    g_trace.lastArg1 = static_cast<uint32_t>(op);
    MixWord(0xCA110010u);
    MixWord(g_trace.lastArg0);
    MixWord(g_trace.lastArg1);
    return fs->pc;
}

void StubRemoveLocalVars(LuaLexState* ls, int32_t nvars) {
    g_trace.traceCalls += 1;
    g_trace.lastArg0 = ToSimVa(ls);
    g_trace.lastArg1 = static_cast<uint32_t>(nvars);
    MixWord(0xCA110011u);
    MixWord(g_trace.lastArg0);
    MixWord(g_trace.lastArg1);
}

void StubProtoOk(LuaState* state, LuaProto* f, int32_t pc) {
    g_trace.traceCalls += 1;
    g_trace.lastArg0 = ToSimVa(state);
    g_trace.lastArg1 = ToSimVa(f);
    g_trace.lastArg2 = static_cast<uint32_t>(pc);
    MixWord(0xCA110012u);
    MixWord(g_trace.lastArg0);
    MixWord(g_trace.lastArg1);
    MixWord(g_trace.lastArg2);
}

int32_t StubFuncName(LuaLexState* ls, LuaExpDesc* v) {
    g_trace.traceCalls += 1;
    g_trace.lastArg0 = ToSimVa(ls);
    v->k = 1;
    v->t = 77;
    v->f = 88;
    MixWord(0xCA110013u);
    MixWord(g_trace.lastArg0);
    return g_trace.stubRet;
}

void StubBody(LuaLexState* ls, int32_t needself, int32_t line) {
    g_trace.traceCalls += 1;
    g_trace.lastArg0 = ToSimVa(ls);
    g_trace.lastArg1 = static_cast<uint32_t>(needself);
    g_trace.lastArg2 = static_cast<uint32_t>(line);
    MixWord(0xCA110014u);
    MixWord(g_trace.lastArg0);
    MixWord(g_trace.lastArg1);
    MixWord(g_trace.lastArg2);
}

void StubCodeStoreVar(LuaLexState* ls, LuaExpDesc* v) {
    g_trace.traceCalls += 1;
    g_trace.lastArg0 = ToSimVa(ls);
    g_trace.lastArg1 = static_cast<uint32_t>(v->k);
    g_trace.lastArg2 = static_cast<uint32_t>(v->t);
    g_trace.lastArg3 = static_cast<uint32_t>(v->f);
    MixWord(0xCA110015u);
    MixWord(g_trace.lastArg0);
    MixWord(g_trace.lastArg1);
    MixWord(g_trace.lastArg2);
    MixWord(g_trace.lastArg3);
}

int32_t StubDebugGetLine(const int32_t* lineinfo, int32_t pc, int32_t refline, int32_t* refi) {
    g_trace.traceCalls += 1;
    g_trace.lastArg0 = ToSimVa(lineinfo);
    g_trace.lastArg1 = static_cast<uint32_t>(pc);
    g_trace.lastArg2 = static_cast<uint32_t>(refline);
    *refi += 3;
    MixWord(0xCA110016u);
    MixWord(g_trace.lastArg0);
    MixWord(g_trace.lastArg1);
    MixWord(g_trace.lastArg2);
    MixWord(static_cast<uint32_t>(*refi));
    return g_trace.stubRet;
}

void StubCallLineHook(LuaState* state, LuaTObject* topMinus2, int32_t newline, uint32_t linehook) {
    g_trace.traceCalls += 1;
    g_trace.lastArg0 = ToSimVa(state);
    g_trace.lastArg1 = ToSimVa(topMinus2);
    g_trace.lastArg2 = static_cast<uint32_t>(newline);
    g_trace.lastArg3 = linehook;
    MixWord(0xCA110017u);
    MixWord(g_trace.lastArg0);
    MixWord(g_trace.lastArg1);
    MixWord(g_trace.lastArg2);
    MixWord(g_trace.lastArg3);
}

LuaClosure* StubFuncNewClosure(LuaState* state, int32_t nelems) {
    g_trace.traceCalls += 1;
    g_trace.lastArg0 = ToSimVa(state);
    g_trace.lastArg1 = static_cast<uint32_t>(nelems);
    MixWord(0xCA110018u);
    MixWord(g_trace.lastArg0);
    MixWord(g_trace.lastArg1);
    return reinterpret_cast<LuaClosure*>(FromSimVa(kArenaBase + 0x5000u));
}

void StubFuncFreeProto(LuaState* state, LuaProto* f) {
    g_trace.traceCalls += 1;
    g_trace.lastArg0 = ToSimVa(state);
    g_trace.lastArg1 = ToSimVa(f);
    MixWord(0xCA110019u);
    MixWord(g_trace.lastArg0);
    MixWord(g_trace.lastArg1);
}

void StubTableFree(LuaState* state, LuaHash* t) {
    g_trace.traceCalls += 1;
    g_trace.lastArg0 = ToSimVa(state);
    g_trace.lastArg1 = ToSimVa(t);
    MixWord(0xCA11001Au);
    MixWord(g_trace.lastArg0);
    MixWord(g_trace.lastArg1);
}

void StubStringResize(LuaState* state, LuaStringTable* tb, int32_t newsize) {
    g_trace.traceCalls += 1;
    g_trace.lastArg0 = ToSimVa(state);
    g_trace.lastArg1 = ToSimVa(tb);
    g_trace.lastArg2 = static_cast<uint32_t>(newsize);
    tb->size = newsize;
    MixWord(0xCA11001Bu);
    MixWord(g_trace.lastArg0);
    MixWord(g_trace.lastArg1);
    MixWord(g_trace.lastArg2);
}

void StubGcMarkLock(LuaState* state, LuaStkMark* st) {
    g_trace.traceCalls += 1;
    g_trace.lastArg0 = ToSimVa(state);
    MixWord(0xCA11001Cu);
    MixWord(g_trace.lastArg0);
    (void)st;
}

void StubTableRemove(LuaHash* t, LuaNode* n) {
    g_trace.traceCalls += 1;
    g_trace.lastArg0 = ToSimVa(t);
    g_trace.lastArg1 = ToSimVa(n);
    MixWord(0xCA11001Du);
    MixWord(g_trace.lastArg0);
    MixWord(g_trace.lastArg1);
}

int32_t StubCheckEvent(const char* event) {
    g_trace.traceCalls += 1;
    MixWord(0xCA11001Eu);
    MixString(event);
    return g_trace.stubRet;
}

void StubVError(
    LuaState* state,
    const char* fmt,
    const char* a0,
    const char* a1,
    const char* a2,
    int32_t i0) {
    g_trace.traceCalls += 1;
    g_trace.lastArg0 = ToSimVa(state);
    MixWord(0xCA11001Fu);
    MixWord(g_trace.lastArg0);
    MixString(fmt);
    if (a0 != nullptr) {
        MixString(a0);
        MixString(a1);
        MixString(a2);
    } else {
        MixWord(static_cast<uint32_t>(i0));
    }
}

void StubCodePatchListAux(
    LuaFuncState* fs,
    int32_t list,
    int32_t vtarget,
    int32_t reg,
    int32_t dtarget) {
    g_trace.traceCalls += 1;
    g_trace.lastArg0 = ToSimVa(fs);
    g_trace.lastArg1 = static_cast<uint32_t>(list);
    g_trace.lastArg2 = static_cast<uint32_t>(vtarget);
    g_trace.lastArg3 = static_cast<uint32_t>((reg << 16) | (dtarget & 0xFFFF));
    MixWord(0xCA110020u);
    MixWord(g_trace.lastArg0);
    MixWord(g_trace.lastArg1);
    MixWord(g_trace.lastArg2);
    MixWord(g_trace.lastArg3);
}

void StubCodeRememberLineInfo(LuaFuncState* fs) {
    g_trace.traceCalls += 1;
    g_trace.lastArg0 = ToSimVa(fs);
    MixWord(0xCA110021u);
    MixWord(g_trace.lastArg0);
}

int32_t StubVmToString(LuaState* state, LuaTObject* obj) {
    g_trace.traceCalls += 1;
    g_trace.lastArg0 = ToSimVa(state);
    g_trace.lastArg1 = ToSimVa(obj);
    MixWord(0xCA110022u);
    MixWord(g_trace.lastArg0);
    MixWord(g_trace.lastArg1);
    if (g_trace.stubRet != 0) {
        return g_trace.stubRet;
    }
    auto* ts = reinterpret_cast<LuaTString*>(FromSimVa(kArenaBase + 0x5100u));
    ts->len = 5;
    std::strcpy(reinterpret_cast<char*>(ts) + sizeof(LuaTString), "hello");
    obj->ttype = kLuaTypeString;
    obj->value.ts = ts;
    return 0;
}

LuaTString* StubStringCreateUdata(LuaState* state, void* udata, int32_t tag) {
    g_trace.traceCalls += 1;
    g_trace.lastArg0 = ToSimVa(state);
    g_trace.lastArg1 = ToSimVa(udata);
    g_trace.lastArg2 = static_cast<uint32_t>(tag);
    MixWord(0xCA110023u);
    MixWord(g_trace.lastArg0);
    MixWord(g_trace.lastArg1);
    MixWord(g_trace.lastArg2);
    return reinterpret_cast<LuaTString*>(FromSimVa(kArenaBase + 0x5200u));
}

LuaTString* StubStringNew(LuaState* state, const char* str) {
    g_trace.traceCalls += 1;
    g_trace.lastArg0 = ToSimVa(state);
    MixWord(0xCA110024u);
    MixWord(g_trace.lastArg0);
    MixString(str);
    return reinterpret_cast<LuaTString*>(FromSimVa(kArenaBase + 0x5300u));
}

const LuaTObject* StubVmGetGlobal(LuaState* state, LuaTString* name) {
    g_trace.traceCalls += 1;
    g_trace.lastArg0 = ToSimVa(state);
    g_trace.lastArg1 = ToSimVa(name);
    MixWord(0xCA110025u);
    MixWord(g_trace.lastArg0);
    MixWord(g_trace.lastArg1);
    auto* val = reinterpret_cast<LuaTObject*>(FromSimVa(kArenaBase + 0x5400u));
    val->ttype = kLuaTypeNumber;
    val->value.raw = 0x1122334455667788ULL;
    return val;
}

LuaTString* StubStringNewUdata(LuaState* state, uint32_t size, void* udata) {
    g_trace.traceCalls += 1;
    g_trace.lastArg0 = ToSimVa(state);
    g_trace.lastArg1 = size;
    g_trace.lastArg2 = ToSimVa(udata);
    MixWord(0xCA110026u);
    MixWord(g_trace.lastArg0);
    MixWord(size);
    MixWord(g_trace.lastArg2);
    auto* ts = reinterpret_cast<LuaTString*>(FromSimVa(kArenaBase + 0x5500u));
    ts->u.d.value = FromSimVa(kArenaBase + 0x5600u);
    return ts;
}

void StubTableSetNodeVector(LuaState* state, LuaHash* t, uint32_t size) {
    g_trace.traceCalls += 1;
    g_trace.lastArg0 = ToSimVa(state);
    g_trace.lastArg1 = ToSimVa(t);
    g_trace.lastArg2 = size;
    t->size = static_cast<int32_t>(size);
    MixWord(0xCA110027u);
    MixWord(g_trace.lastArg0);
    MixWord(g_trace.lastArg1);
    MixWord(size);
}

LuaNode* StubTableMainPosition(const LuaHash* t, const LuaTObject* key) {
    g_trace.traceCalls += 1;
    g_trace.lastArg0 = ToSimVa(t);
    g_trace.lastArg1 = ToSimVa(key);
    MixWord(0xCA110028u);
    MixWord(g_trace.lastArg0);
    MixWord(g_trace.lastArg1);
    return &t->node[g_trace.stubRet];
}

void StubTableRehash(LuaState* state, LuaHash* t) {
    g_trace.traceCalls += 1;
    g_trace.lastArg0 = ToSimVa(state);
    g_trace.lastArg1 = ToSimVa(t);
    MixWord(0xCA110029u);
    MixWord(g_trace.lastArg0);
    MixWord(g_trace.lastArg1);
    t->node[1].key.ttype = kLuaTypeNil;
    t->firstfree = &t->node[3];
    t->node[3].key.ttype = kLuaTypeNil;
}

int32_t StubCode2(LuaFuncState* fs, int32_t op, int32_t arg1, int32_t arg2) {
    g_trace.traceCalls += 1;
    g_trace.lastArg0 = ToSimVa(fs);
    g_trace.lastArg1 = static_cast<uint32_t>(op);
    g_trace.lastArg2 = static_cast<uint32_t>(arg1);
    g_trace.lastArg3 = static_cast<uint32_t>(arg2);
    MixWord(0xCA11002Au);
    MixWord(g_trace.lastArg0);
    MixWord(g_trace.lastArg1);
    MixWord(g_trace.lastArg2);
    MixWord(g_trace.lastArg3);
    const int32_t pc = fs->pc;
    fs->pc += 1;
    return pc;
}

void StubCodeArith(LuaFuncState* fs, LuaExpDesc* v, int32_t arg, int32_t op) {
    g_trace.traceCalls += 1;
    g_trace.lastArg0 = ToSimVa(fs);
    g_trace.lastArg1 = ToSimVa(v);
    g_trace.lastArg2 = static_cast<uint32_t>(arg);
    g_trace.lastArg3 = static_cast<uint32_t>(op);
    MixWord(0xCA11002Bu);
    MixWord(g_trace.lastArg0);
    MixWord(g_trace.lastArg1);
    MixWord(g_trace.lastArg2);
    MixWord(g_trace.lastArg3);
}

int32_t StubCodeExp2Val(LuaFuncState* fs, LuaExpDesc* v) {
    g_trace.traceCalls += 1;
    g_trace.lastArg0 = ToSimVa(fs);
    g_trace.lastArg1 = ToSimVa(v);
    MixWord(0xCA11002Cu);
    MixWord(g_trace.lastArg0);
    MixWord(g_trace.lastArg1);
    return 0;
}

void StubCodeConcat(LuaFuncState* fs, int32_t* l1, int32_t l2) {
    g_trace.traceCalls += 1;
    g_trace.lastArg0 = ToSimVa(fs);
    g_trace.lastArg1 = static_cast<uint32_t>(*l1);
    g_trace.lastArg2 = static_cast<uint32_t>(l2);
    *l1 = (*l1 * 10) + l2;
    MixWord(0xCA11002Du);
    MixWord(g_trace.lastArg0);
    MixWord(g_trace.lastArg1);
    MixWord(g_trace.lastArg2);
}

void StubCodePrefix(LuaLexState* ls, LuaExpDesc* v, int32_t n) {
    g_trace.traceCalls += 1;
    g_trace.lastArg0 = ToSimVa(ls);
    g_trace.lastArg1 = ToSimVa(v);
    g_trace.lastArg2 = static_cast<uint32_t>(n);
    MixWord(0xCA11002Eu);
    MixWord(g_trace.lastArg0);
    MixWord(g_trace.lastArg1);
    MixWord(g_trace.lastArg2);
}

void InitHooks(uint8_t* arena) {
    g_trace = TraceState{};
    g_trace.arenaBase = arena;
    LuaVmRuntimeHooks hooks{};
    hooks.crtRealloc = &StubRealloc;
    hooks.crtFree = &StubFree;
    hooks.luaError = &StubLuaError;
    hooks.luaBreakRun = &StubBreakRun;
    hooks.luaDoError = &StubDoError;
    hooks.luaRunProtected = &StubRunProtected;
    hooks.luaZioMemOpen = &StubZioMemOpen;
    hooks.luaProtectedParser = &StubProtectedParser;
    hooks.luaTokenToString = &StubTokenToString;
    hooks.luaLexError = &StubLexError;
    hooks.luaSyntaxError = &StubSyntaxError;
    hooks.luaLex = &StubLex;
    hooks.luaErrorExpected = &StubErrorExpected;
    hooks.luaCodeKstr = &StubCodeKstr;
    hooks.luaCodeNil = &StubCodeNil;
    hooks.luaCode0 = &StubCode0;
    hooks.luaRemoveLocalVars = &StubRemoveLocalVars;
    hooks.luaProtoOk = &StubProtoOk;
    hooks.luaFuncName = &StubFuncName;
    hooks.luaBody = &StubBody;
    hooks.luaCodeStoreVar = &StubCodeStoreVar;
    hooks.luaDebugGetLine = &StubDebugGetLine;
    hooks.luaCallLineHook = &StubCallLineHook;
    hooks.luaFuncNewClosure = &StubFuncNewClosure;
    hooks.luaFuncFreeProto = &StubFuncFreeProto;
    hooks.luaTableFree = &StubTableFree;
    hooks.luaStringResize = &StubStringResize;
    hooks.luaGcMarkLock = &StubGcMarkLock;
    hooks.luaTableRemove = &StubTableRemove;
    hooks.luaCheckEvent = &StubCheckEvent;
    hooks.luaVError = &StubVError;
    hooks.luaCodePatchListAux = &StubCodePatchListAux;
    hooks.luaCodeRememberLineInfo = &StubCodeRememberLineInfo;
    hooks.luaVmToString = &StubVmToString;
    hooks.luaStringCreateUdata = &StubStringCreateUdata;
    hooks.luaStringNew = &StubStringNew;
    hooks.luaVmGetGlobal = &StubVmGetGlobal;
    hooks.luaStringNewUdata = &StubStringNewUdata;
    hooks.luaTableSetNodeVector = &StubTableSetNodeVector;
    hooks.luaTableMainPosition = &StubTableMainPosition;
    hooks.luaTableRehash = &StubTableRehash;
    hooks.luaCode2 = &StubCode2;
    hooks.luaCodeArith = &StubCodeArith;
    hooks.luaCodeExp2Val = &StubCodeExp2Val;
    hooks.luaCodeConcat = &StubCodeConcat;
    hooks.luaCodePrefix = &StubCodePrefix;
    SetLuaVmRuntimeHooks(hooks);
}

}  // namespace

int main(int argc, char** argv) {
    if (argc < 2) {
        std::fprintf(stderr, "usage: ra3_luavmcheck <op> [args...]\n");
        return 2;
    }

    static uint8_t arena[0x00040000];
    std::memset(arena, 0, sizeof(arena));
    InitHooks(arena);

    const std::string op = argv[1];
    auto* state = reinterpret_cast<LuaState*>(FromSimVa(kArenaBase + 0x0100u));
    auto* ls = reinterpret_cast<LuaLexState*>(FromSimVa(kArenaBase + 0x0300u));
    auto* fs = reinterpret_cast<LuaFuncState*>(FromSimVa(kArenaBase + 0x0400u));
    auto* proto = reinterpret_cast<LuaProto*>(FromSimVa(kArenaBase + 0x0500u));

    if (op == "realloc") {
        if (argc != 5) {
            return 2;
        }
        const uint32_t oldVa = static_cast<uint32_t>(std::strtoul(argv[2], nullptr, 0));
        const uint32_t size = static_cast<uint32_t>(std::strtoul(argv[3], nullptr, 0));
        g_trace.stubRet = static_cast<int32_t>(std::strtol(argv[4], nullptr, 0));
        void* res = LuaMemRealloc(state, FromSimVa(oldVa), size);
        std::printf(
            "ret=0x%08x calls=%u hash=0x%08x a0=0x%08x a1=0x%08x\n",
            ToSimVa(res),
            g_trace.traceCalls,
            g_trace.traceHash,
            g_trace.lastArg0,
            g_trace.lastArg1);
        return 0;
    }

    if (op == "growaux") {
        if (argc != 7) {
            return 2;
        }
        const uint32_t blockVa = static_cast<uint32_t>(std::strtoul(argv[2], nullptr, 0));
        const uint32_t nelems = static_cast<uint32_t>(std::strtoul(argv[3], nullptr, 0));
        const int32_t inc = static_cast<int32_t>(std::strtol(argv[4], nullptr, 0));
        const uint32_t elemSize = static_cast<uint32_t>(std::strtoul(argv[5], nullptr, 0));
        const uint32_t limit = static_cast<uint32_t>(std::strtoul(argv[6], nullptr, 0));
        void* res = LuaMemGrowAux(
            state,
            FromSimVa(blockVa),
            nelems,
            inc,
            elemSize,
            "growaux overflow",
            limit);
        std::printf(
            "ret=0x%08x calls=%u hash=0x%08x a1=0x%08x\n",
            ToSimVa(res),
            g_trace.traceCalls,
            g_trace.traceHash,
            g_trace.lastArg1);
        return 0;
    }

    if (op == "initstack") {
        if (argc != 4) {
            return 2;
        }
        state->nblocks = static_cast<uint32_t>(std::strtoul(argv[2], nullptr, 0));
        const int32_t stackSize = static_cast<int32_t>(std::strtol(argv[3], nullptr, 0));
        LuaDoInitStack(state, stackSize);
        std::printf(
            "top=0x%08x stack=0x%08x last=0x%08x size=%d cbase=0x%08x nblocks=%u hash=0x%08x\n",
            ToSimVa(state->top),
            ToSimVa(state->stack),
            ToSimVa(state->stackLast),
            state->stackSize,
            ToSimVa(state->Cbase),
            state->nblocks,
            g_trace.traceHash);
        return 0;
    }

    if (op == "checkstack") {
        if (argc != 6) {
            return 2;
        }
        const int32_t topIdx = static_cast<int32_t>(std::strtol(argv[2], nullptr, 0));
        const int32_t lastIdx = static_cast<int32_t>(std::strtol(argv[3], nullptr, 0));
        const int32_t stackSize = static_cast<int32_t>(std::strtol(argv[4], nullptr, 0));
        const int32_t needed = static_cast<int32_t>(std::strtol(argv[5], nullptr, 0));
        auto* base = reinterpret_cast<LuaTObject*>(FromSimVa(kArenaBase + 0x1000u));
        state->stack = base;
        state->top = base + topIdx;
        state->stackLast = base + lastIdx;
        state->stackSize = stackSize;
        LuaDoCheckStack(state, needed);
        std::printf(
            "last=0x%08x calls=%u hash=0x%08x a1=0x%08x\n",
            ToSimVa(state->stackLast),
            g_trace.traceCalls,
            g_trace.traceHash,
            g_trace.lastArg1);
        return 0;
    }

    if (op == "call") {
        if (argc != 5) {
            return 2;
        }
        const int32_t nargs = static_cast<int32_t>(std::strtol(argv[2], nullptr, 0));
        const int32_t nresults = static_cast<int32_t>(std::strtol(argv[3], nullptr, 0));
        g_trace.stubRet = static_cast<int32_t>(std::strtol(argv[4], nullptr, 0));
        auto* base = reinterpret_cast<LuaTObject*>(FromSimVa(kArenaBase + 0x1000u));
        state->stack = base;
        state->top = base + 10;
        const int32_t res = LuaProtectedCall(state, nargs, nresults);
        std::printf(
            "ret=%d top=0x%08x calls=%u hash=0x%08x a2=0x%08x a3=0x%08x\n",
            res,
            ToSimVa(state->top),
            g_trace.traceCalls,
            g_trace.traceHash,
            g_trace.lastArg2,
            g_trace.lastArg3);
        return 0;
    }

    if (op == "dobuffer") {
        if (argc != 6) {
            return 2;
        }
        const uint32_t firstByte = static_cast<uint32_t>(std::strtoul(argv[2], nullptr, 0));
        const uint32_t size = static_cast<uint32_t>(std::strtoul(argv[3], nullptr, 0));
        const int32_t hasName = static_cast<int32_t>(std::strtol(argv[4], nullptr, 0));
        g_trace.stubRet = static_cast<int32_t>(std::strtol(argv[5], nullptr, 0));
        auto* base = reinterpret_cast<LuaTObject*>(FromSimVa(kArenaBase + 0x1000u));
        state->stack = base;
        state->top = base + 6;
        auto* buf = reinterpret_cast<char*>(FromSimVa(kArenaBase + 0x2000u));
        buf[0] = static_cast<char>(firstByte);
        buf[1] = 'x';
        auto* name = reinterpret_cast<char*>(FromSimVa(kArenaBase + 0x2100u));
        std::strcpy(name, "@chunk.lua");
        const int32_t res = LuaDoBuffer(state, buf, size, hasName != 0 ? name : nullptr);
        std::printf(
            "ret=%d top=0x%08x calls=%u hash=0x%08x\n",
            res,
            ToSimVa(state->top),
            g_trace.traceCalls,
            g_trace.traceHash);
        return 0;
    }

    if (op == "checknext") {
        if (argc != 6) {
            return 2;
        }
        ls->t.token = static_cast<int32_t>(std::strtol(argv[2], nullptr, 0));
        ls->lookahead.token = static_cast<int32_t>(std::strtol(argv[3], nullptr, 0));
        ls->lookahead.seminfo.ts = reinterpret_cast<LuaTString*>(FromSimVa(kArenaBase + 0x3330u));
        ls->linenumber = 42;
        ls->lastline = 10;
        const int32_t expected = static_cast<int32_t>(std::strtol(argv[4], nullptr, 0));
        g_trace.lexNextToken = static_cast<int32_t>(std::strtol(argv[5], nullptr, 0));
        g_trace.lexNextSeminfoLo = 0x12345678u;
        g_trace.lexNextSeminfoHi = 0x9ABCDEF0u;
        LuaParserCheckNext(ls, expected);
        std::printf(
            "tok=%d look=%d lastline=%d calls=%u hash=0x%08x\n",
            ls->t.token,
            ls->lookahead.token,
            ls->lastline,
            g_trace.traceCalls,
            g_trace.traceHash);
        return 0;
    }

    if (op == "optional") {
        if (argc != 6) {
            return 2;
        }
        ls->t.token = static_cast<int32_t>(std::strtol(argv[2], nullptr, 0));
        ls->lookahead.token = static_cast<int32_t>(std::strtol(argv[3], nullptr, 0));
        ls->linenumber = 55;
        ls->lastline = 12;
        const int32_t expected = static_cast<int32_t>(std::strtol(argv[4], nullptr, 0));
        g_trace.lexNextToken = static_cast<int32_t>(std::strtol(argv[5], nullptr, 0));
        const int32_t res = LuaParserOptional(ls, expected);
        std::printf(
            "ret=%d tok=%d look=%d lastline=%d calls=%u hash=0x%08x\n",
            res,
            ls->t.token,
            ls->lookahead.token,
            ls->lastline,
            g_trace.traceCalls,
            g_trace.traceHash);
        return 0;
    }

    if (op == "checkmatch") {
        if (argc != 7) {
            return 2;
        }
        ls->t.token = static_cast<int32_t>(std::strtol(argv[2], nullptr, 0));
        ls->lookahead.token = static_cast<int32_t>(std::strtol(argv[3], nullptr, 0));
        ls->linenumber = static_cast<int32_t>(std::strtol(argv[4], nullptr, 0));
        const int32_t what = static_cast<int32_t>(std::strtol(argv[5], nullptr, 0));
        const int32_t where = static_cast<int32_t>(std::strtol(argv[6], nullptr, 0));
        g_trace.lexNextToken = 285;
        LuaParserCheckMatch(ls, what, 257, where);
        std::printf(
            "tok=%d look=%d lastline=%d calls=%u hash=0x%08x\n",
            ls->t.token,
            ls->lookahead.token,
            ls->lastline,
            g_trace.traceCalls,
            g_trace.traceHash);
        return 0;
    }

    if (op == "stringk") {
        if (argc != 5) {
            return 2;
        }
        const int32_t sizekstr = static_cast<int32_t>(std::strtol(argv[2], nullptr, 0));
        const int32_t constIndex = static_cast<int32_t>(std::strtol(argv[3], nullptr, 0));
        const int32_t matchExisting = static_cast<int32_t>(std::strtol(argv[4], nullptr, 0));
        ls->fs = fs;
        fs->f = proto;
        fs->L = state;
        auto* kstrArr = reinterpret_cast<LuaTString**>(FromSimVa(kArenaBase + 0x3000u));
        auto* ts = reinterpret_cast<LuaTString*>(FromSimVa(kArenaBase + 0x3400u));
        proto->kstr = kstrArr;
        proto->sizekstr = sizekstr;
        ts->u.s.constIndex = constIndex;
        if (matchExisting != 0 && constIndex >= 0 && constIndex < sizekstr) {
            kstrArr[constIndex] = ts;
        }
        const int32_t res = LuaParserStringK(ls, ts);
        std::printf(
            "ret=%d sizekstr=%d constIdx=%d kstr=0x%08x calls=%u hash=0x%08x\n",
            res,
            proto->sizekstr,
            ts->u.s.constIndex,
            ToSimVa(proto->kstr),
            g_trace.traceCalls,
            g_trace.traceHash);
        return 0;
    }

    if (op == "adjustmult") {
        if (argc != 6) {
            return 2;
        }
        const int32_t nvars = static_cast<int32_t>(std::strtol(argv[2], nullptr, 0));
        const int32_t nexps = static_cast<int32_t>(std::strtol(argv[3], nullptr, 0));
        const uint32_t prevInst = static_cast<uint32_t>(std::strtoul(argv[4], nullptr, 0));
        const int32_t pc = static_cast<int32_t>(std::strtol(argv[5], nullptr, 0));
        ls->fs = fs;
        fs->f = proto;
        fs->L = state;
        fs->ls = ls;
        fs->pc = pc;
        fs->lasttarget = 0;
        fs->freereg = 10;
        auto* codeArr = reinterpret_cast<uint32_t*>(FromSimVa(kArenaBase + 0x3800u));
        proto->code = codeArr;
        proto->maxstacksize = 12;
        if (pc > 0) {
            codeArr[pc - 1] = prevInst;
        }
        LuaParserAdjustMultAssign(ls, nvars, nexps);
        std::printf(
            "pc=%d freereg=%d maxstack=%d code0=0x%08x calls=%u hash=0x%08x\n",
            fs->pc,
            fs->freereg,
            proto->maxstacksize,
            proto->code[fs->pc - 1],
            g_trace.traceCalls,
            g_trace.traceHash);
        return 0;
    }

    if (op == "closefunc") {
        if (argc != 4) {
            return 2;
        }
        const int32_t pc = static_cast<int32_t>(std::strtol(argv[2], nullptr, 0));
        const int32_t lasttarget = static_cast<int32_t>(std::strtol(argv[3], nullptr, 0));
        auto* prevFs = reinterpret_cast<LuaFuncState*>(FromSimVa(kArenaBase + 0x0480u));
        ls->fs = fs;
        ls->L = state;
        fs->f = proto;
        fs->prev = prevFs;
        fs->pc = pc;
        fs->lasttarget = lasttarget;
        fs->jpc = 3;
        fs->nactvar = 4;
        proto->sizekstr = 2;
        proto->sizeknum = 3;
        proto->sizekproto = 1;
        proto->sizelocvars = 2;
        proto->sizelineinfo = 5;
        LuaParserCloseFunc(ls);
        std::printf(
            "fs=0x%08x sizelineinfo=%d lineend=0x%08x jpc=%d lasttarget=%d calls=%u hash=0x%08x\n",
            ToSimVa(ls->fs),
            proto->sizelineinfo,
            static_cast<uint32_t>(proto->lineinfo[proto->sizelineinfo - 1]),
            fs->jpc,
            fs->lasttarget,
            g_trace.traceCalls,
            g_trace.traceHash);
        return 0;
    }

    if (op == "funcstat") {
        if (argc != 5) {
            return 2;
        }
        ls->lookahead.token = static_cast<int32_t>(std::strtol(argv[2], nullptr, 0));
        g_trace.stubRet = static_cast<int32_t>(std::strtol(argv[3], nullptr, 0));
        const int32_t line = static_cast<int32_t>(std::strtol(argv[4], nullptr, 0));
        ls->linenumber = line;
        g_trace.lexNextToken = 260;
        LuaParserFuncStat(ls, line);
        std::printf(
            "tok=%d look=%d lastline=%d calls=%u hash=0x%08x\n",
            ls->t.token,
            ls->lookahead.token,
            ls->lastline,
            g_trace.traceCalls,
            g_trace.traceHash);
        return 0;
    }

    if (op == "traceexec") {
        if (argc != 6) {
            return 2;
        }
        const int32_t pcOffset = static_cast<int32_t>(std::strtol(argv[2], nullptr, 0));
        const int32_t lastpc = static_cast<int32_t>(std::strtol(argv[3], nullptr, 0));
        const int32_t curline = static_cast<int32_t>(std::strtol(argv[4], nullptr, 0));
        g_trace.stubRet = static_cast<int32_t>(std::strtol(argv[5], nullptr, 0));
        auto* top = reinterpret_cast<LuaTObject*>(FromSimVa(kArenaBase + 0x1200u));
        auto* ci = reinterpret_cast<LuaCallInfo*>(FromSimVa(kArenaBase + 0x1300u));
        auto* cl = reinterpret_cast<LuaClosure*>(FromSimVa(kArenaBase + 0x1400u));
        auto* pcHolder = reinterpret_cast<const uint32_t**>(FromSimVa(kArenaBase + 0x1500u));
        auto* codeBase = reinterpret_cast<uint32_t*>(FromSimVa(kArenaBase + 0x1600u));
        auto* lineinfo = reinterpret_cast<int32_t*>(FromSimVa(kArenaBase + 0x1700u));
        (top - 1)->value.ci = ci;
        cl->f = proto;
        proto->code = codeBase;
        proto->lineinfo = lineinfo;
        *pcHolder = codeBase + pcOffset;
        ci->func = cl;
        ci->pc = pcHolder;
        ci->lastpc = lastpc;
        ci->line = curline;
        ci->refi = 2;
        LuaVmTraceExec(state, top, 0x00409999u);
        std::printf(
            "lastpc=%d line=%d refi=%d top=0x%08x calls=%u hash=0x%08x\n",
            ci->lastpc,
            ci->line,
            ci->refi,
            ToSimVa(state->top),
            g_trace.traceCalls,
            g_trace.traceHash);
        return 0;
    }

    if (op == "closure") {
        if (argc != 4) {
            return 2;
        }
        const int32_t nelems = static_cast<int32_t>(std::strtol(argv[2], nullptr, 0));
        const int32_t atLast = static_cast<int32_t>(std::strtol(argv[3], nullptr, 0));
        auto* base = reinterpret_cast<LuaTObject*>(FromSimVa(kArenaBase + 0x1000u));
        state->stack = base;
        state->top = base + 4;
        for (int32_t i = 0; i < 6; ++i) {
            base[i].ttype = kLuaTypeNumber;
            base[i].value.raw = 0x100u + static_cast<uint64_t>(i);
        }
        state->stackLast = (atLast != 0) ? (state->top - nelems) : (base + 20);
        state->stackSize = 30;
        LuaClosure* c = LuaVmClosure(state, nelems);
        std::printf(
            "ret=0x%08x top=0x%08x topType=%d up0=%llu calls=%u hash=0x%08x\n",
            ToSimVa(c),
            ToSimVa(state->top),
            (state->top - 1)->ttype,
            static_cast<unsigned long long>(c->upvalue[0].value.raw),
            g_trace.traceCalls,
            g_trace.traceHash);
        return 0;
    }

    if (op == "collectproto") {
        if (argc != 4) {
            return 2;
        }
        const int32_t mark0 = static_cast<int32_t>(std::strtol(argv[2], nullptr, 0));
        const int32_t mark1 = static_cast<int32_t>(std::strtol(argv[3], nullptr, 0));
        auto* p0 = reinterpret_cast<LuaProto*>(FromSimVa(kArenaBase + 0x2000u));
        auto* p1 = reinterpret_cast<LuaProto*>(FromSimVa(kArenaBase + 0x2100u));
        *reinterpret_cast<LuaProto**>(reinterpret_cast<uint8_t*>(p0) + 0x04) = p1;
        *reinterpret_cast<LuaProto**>(reinterpret_cast<uint8_t*>(p0) + 0x08) = (mark0 != 0) ? nullptr : p0;
        *reinterpret_cast<LuaProto**>(reinterpret_cast<uint8_t*>(p1) + 0x04) = nullptr;
        *reinterpret_cast<LuaProto**>(reinterpret_cast<uint8_t*>(p1) + 0x08) = (mark1 != 0) ? nullptr : p1;
        state->rootproto = p0;
        LuaGcCollectProto(state);
        std::printf(
            "root=0x%08x calls=%u hash=0x%08x\n",
            ToSimVa(state->rootproto),
            g_trace.traceCalls,
            g_trace.traceHash);
        return 0;
    }

    if (op == "collecttable") {
        if (argc != 4) {
            return 2;
        }
        const int32_t mark0 = static_cast<int32_t>(std::strtol(argv[2], nullptr, 0));
        const int32_t mark1 = static_cast<int32_t>(std::strtol(argv[3], nullptr, 0));
        auto* h0 = reinterpret_cast<LuaHash*>(FromSimVa(kArenaBase + 0x2200u));
        auto* h1 = reinterpret_cast<LuaHash*>(FromSimVa(kArenaBase + 0x2300u));
        h0->next = h1;
        h0->marked = (mark0 != 0) ? nullptr : h0;
        h1->next = nullptr;
        h1->marked = (mark1 != 0) ? nullptr : h1;
        state->roottable = h0;
        LuaGcCollectTable(state);
        std::printf(
            "root=0x%08x calls=%u hash=0x%08x\n",
            ToSimVa(state->roottable),
            g_trace.traceCalls,
            g_trace.traceHash);
        return 0;
    }

    if (op == "collectstrings") {
        if (argc != 5) {
            return 2;
        }
        const int32_t mark0 = static_cast<int32_t>(std::strtol(argv[2], nullptr, 0));
        const int32_t all = static_cast<int32_t>(std::strtol(argv[3], nullptr, 0));
        const int32_t tableSize = static_cast<int32_t>(std::strtol(argv[4], nullptr, 0));
        auto* hashBuckets = reinterpret_cast<LuaTString**>(FromSimVa(kArenaBase + 0x2400u));
        auto* s0 = reinterpret_cast<LuaTString*>(FromSimVa(kArenaBase + 0x2600u));
        s0->len = 7;
        s0->marked = mark0;
        s0->nexthash = nullptr;
        hashBuckets[0] = s0;
        state->strt.size = tableSize;
        state->strt.nuse = 2;
        state->strt.hash = hashBuckets;
        state->nblocks = 500;
        LuaGcCollectStrings(state, all);
        std::printf(
            "nuse=%u size=%d nblocks=%u s0mark=%d bucket0=0x%08x calls=%u hash=0x%08x\n",
            state->strt.nuse,
            state->strt.size,
            state->nblocks,
            s0->marked,
            ToSimVa(hashBuckets[0]),
            g_trace.traceCalls,
            g_trace.traceHash);
        return 0;
    }

    if (op == "collectudata") {
        if (argc != 5) {
            return 2;
        }
        const int32_t mark0 = static_cast<int32_t>(std::strtol(argv[2], nullptr, 0));
        const int32_t all = static_cast<int32_t>(std::strtol(argv[3], nullptr, 0));
        const int32_t tableSize = static_cast<int32_t>(std::strtol(argv[4], nullptr, 0));
        auto* hashBuckets = reinterpret_cast<LuaTString**>(FromSimVa(kArenaBase + 0x2800u));
        auto* u0 = reinterpret_cast<LuaTString*>(FromSimVa(kArenaBase + 0x2A00u));
        auto* tm = reinterpret_cast<LuaTM*>(FromSimVa(kArenaBase + 0x2C00u));
        u0->u.d.tag = 1;
        u0->len = 12;
        u0->marked = mark0;
        u0->nexthash = nullptr;
        hashBuckets[0] = u0;
        state->udt.size = tableSize;
        state->udt.nuse = 2;
        state->udt.hash = hashBuckets;
        state->TMtable = tm;
        state->nblocks = 600;
        LuaGcCollectUdata(state, all);
        std::printf(
            "nuse=%u size=%d nblocks=%u collected=0x%08x u0mark=%d calls=%u hash=0x%08x\n",
            state->udt.nuse,
            state->udt.size,
            state->nblocks,
            ToSimVa(tm[1].collected),
            u0->marked,
            g_trace.traceCalls,
            g_trace.traceHash);
        return 0;
    }

    if (op == "markall") {
        if (argc != 4) {
            return 2;
        }
        const int32_t refStatus = static_cast<int32_t>(std::strtol(argv[2], nullptr, 0));
        const int32_t nodeValType = static_cast<int32_t>(std::strtol(argv[3], nullptr, 0));
        auto* gt = reinterpret_cast<LuaHash*>(FromSimVa(kArenaBase + 0x3000u));
        auto* nodes = reinterpret_cast<LuaNode*>(FromSimVa(kArenaBase + 0x3100u));
        auto* stack = reinterpret_cast<LuaTObject*>(FromSimVa(kArenaBase + 0x3300u));
        auto* refs = reinterpret_cast<LuaRef*>(FromSimVa(kArenaBase + 0x3500u));
        auto* cl = reinterpret_cast<LuaClosure*>(FromSimVa(kArenaBase + 0x3700u));
        auto* sKey = reinterpret_cast<LuaTString*>(FromSimVa(kArenaBase + 0x3800u));
        auto* sUp = reinterpret_cast<LuaTString*>(FromSimVa(kArenaBase + 0x3900u));
        gt->node = nodes;
        gt->size = 1;
        gt->marked = gt;
        nodes[0].key.ttype = kLuaTypeString;
        nodes[0].key.value.ts = sKey;
        nodes[0].val.ttype = nodeValType;
        nodes[0].val.value.cl = cl;
        cl->mark = cl;
        cl->isC = 1;
        cl->nelems = 1;
        cl->upvalue[0].ttype = kLuaTypeString;
        cl->upvalue[0].value.ts = sUp;
        stack[0].ttype = kLuaTypeClosure;
        stack[0].value.cl = cl;
        refs[0].status = refStatus;
        refs[0].o.ttype = kLuaTypeString;
        refs[0].o.value.ts = sKey;
        state->gt = gt;
        state->stack = stack;
        state->top = stack + 1;
        state->refArray = refs;
        state->refSize = 1;
        LuaGcMarkAll(state);
        std::printf(
            "sKeyMark=%d sUpMark=%d clMark=0x%08x calls=%u hash=0x%08x\n",
            sKey->marked,
            sUp->marked,
            ToSimVa(cl->mark),
            g_trace.traceCalls,
            g_trace.traceHash);
        return 0;
    }

    if (op == "newtag") {
        if (argc != 3) {
            return 2;
        }
        const int32_t lastTag = static_cast<int32_t>(std::strtol(argv[2], nullptr, 0));
        auto* tm = reinterpret_cast<LuaTM*>(FromSimVa(kArenaBase + 0x4000u));
        std::memset(tm, 0x5A, sizeof(LuaTM) * 16);
        state->TMtable = tm;
        state->lastTag = lastTag;
        state->nblocks = 128;
        const int32_t res = LuaNewTag(state);
        std::printf(
            "ret=%d lastTag=%d nblocks=%u tm=0x%08x m0=0x%08x calls=%u hash=0x%08x\n",
            res,
            state->lastTag,
            state->nblocks,
            ToSimVa(state->TMtable),
            ToSimVa(state->TMtable[state->lastTag].method[0]),
            g_trace.traceCalls,
            g_trace.traceHash);
        return 0;
    }

    if (op == "tagof") {
        if (argc != 4) {
            return 2;
        }
        const int32_t ttype = static_cast<int32_t>(std::strtol(argv[2], nullptr, 0));
        const int32_t innerTag = static_cast<int32_t>(std::strtol(argv[3], nullptr, 0));
        auto* obj = reinterpret_cast<LuaTObject*>(FromSimVa(kArenaBase + 0x1000u));
        auto* ts = reinterpret_cast<LuaTString*>(FromSimVa(kArenaBase + 0x1100u));
        auto* ht = reinterpret_cast<LuaHash*>(FromSimVa(kArenaBase + 0x1200u));
        obj->ttype = ttype;
        if (ttype == kLuaTypeUserdata) {
            ts->u.d.tag = innerTag;
            obj->value.ts = ts;
        } else if (ttype == kLuaTypeTable) {
            ht->htag = innerTag;
            obj->value.ht = ht;
        }
        const int32_t res = LuaTagOfObject(obj);
        std::printf("ret=%d\n", res);
        return 0;
    }

    if (op == "settagmethod") {
        if (argc != 5) {
            return 2;
        }
        const int32_t tag = static_cast<int32_t>(std::strtol(argv[2], nullptr, 0));
        const int32_t eventIdx = static_cast<int32_t>(std::strtol(argv[3], nullptr, 0));
        const int32_t topType = static_cast<int32_t>(std::strtol(argv[4], nullptr, 0));
        g_trace.stubRet = eventIdx;
        auto* tm = reinterpret_cast<LuaTM*>(FromSimVa(kArenaBase + 0x4000u));
        auto* stack = reinterpret_cast<LuaTObject*>(FromSimVa(kArenaBase + 0x1000u));
        auto* cl = reinterpret_cast<LuaClosure*>(FromSimVa(kArenaBase + 0x1500u));
        state->TMtable = tm;
        state->lastTag = 8;
        state->stack = stack;
        state->top = stack + 2;
        stack[1].ttype = topType;
        stack[1].value.cl = cl;
        auto* evStr = reinterpret_cast<char*>(FromSimVa(kArenaBase + 0x1600u));
        std::strcpy(evStr, "gettable");
        LuaSetTagMethod(state, tag, evStr);
        const int32_t safeTag = (tag >= 0 && tag <= 8) ? tag : 0;
        std::printf(
            "top=0x%08x method=0x%08x calls=%u hash=0x%08x\n",
            ToSimVa(state->top),
            ToSimVa(tm[safeTag].method[eventIdx]),
            g_trace.traceCalls,
            g_trace.traceHash);
        return 0;
    }

    if (op == "pushvalue") {
        if (argc != 4) {
            return 2;
        }
        const int32_t index = static_cast<int32_t>(std::strtol(argv[2], nullptr, 0));
        const int32_t atLast = static_cast<int32_t>(std::strtol(argv[3], nullptr, 0));
        auto* stack = reinterpret_cast<LuaTObject*>(FromSimVa(kArenaBase + 0x1000u));
        for (int32_t i = 0; i < 8; ++i) {
            stack[i].ttype = kLuaTypeNumber;
            stack[i].value.raw = 0x500u + static_cast<uint64_t>(i);
        }
        state->stack = stack;
        state->Cbase = stack + 1;
        state->top = stack + 5;
        state->stackLast = (atLast != 0) ? state->top : (stack + 16);
        state->stackSize = 20;
        LuaPushValue(state, index);
        std::printf(
            "top=0x%08x pushed=%llu calls=%u hash=0x%08x\n",
            ToSimVa(state->top),
            static_cast<unsigned long long>((state->top - 1)->value.raw),
            g_trace.traceCalls,
            g_trace.traceHash);
        return 0;
    }

    if (op == "lastisopen") {
        if (argc != 5) {
            return 2;
        }
        const int32_t pc = static_cast<int32_t>(std::strtol(argv[2], nullptr, 0));
        const int32_t lasttarget = static_cast<int32_t>(std::strtol(argv[3], nullptr, 0));
        const uint32_t inst = static_cast<uint32_t>(std::strtoul(argv[4], nullptr, 0));
        auto* code = reinterpret_cast<uint32_t*>(FromSimVa(kArenaBase + 0x3800u));
        fs->f = proto;
        fs->pc = pc;
        fs->lasttarget = lasttarget;
        proto->code = code;
        if (pc > 0) {
            code[pc - 1] = inst;
        }
        const int32_t res = LuaCodeLastIsOpen(fs);
        std::printf("ret=%d\n", res);
        return 0;
    }

    if (op == "setcallreturns") {
        if (argc != 6) {
            return 2;
        }
        const int32_t pc = static_cast<int32_t>(std::strtol(argv[2], nullptr, 0));
        const int32_t lasttarget = static_cast<int32_t>(std::strtol(argv[3], nullptr, 0));
        const uint32_t inst = static_cast<uint32_t>(std::strtoul(argv[4], nullptr, 0));
        const int32_t nresults = static_cast<int32_t>(std::strtol(argv[5], nullptr, 0));
        auto* code = reinterpret_cast<uint32_t*>(FromSimVa(kArenaBase + 0x3800u));
        fs->f = proto;
        fs->ls = ls;
        fs->pc = pc;
        fs->lasttarget = lasttarget;
        fs->freereg = 6;
        proto->code = code;
        proto->maxstacksize = 8;
        if (pc > 0) {
            code[pc - 1] = inst;
        }
        LuaCodeSetCallReturns(fs, nresults);
        std::printf(
            "code0=0x%08x freereg=%d maxstack=%d\n",
            code[pc - 1],
            fs->freereg,
            proto->maxstacksize);
        return 0;
    }

    if (op == "getlabel") {
        if (argc != 4) {
            return 2;
        }
        fs->pc = static_cast<int32_t>(std::strtol(argv[2], nullptr, 0));
        fs->lasttarget = static_cast<int32_t>(std::strtol(argv[3], nullptr, 0));
        fs->jpc = 9;
        const int32_t res = LuaCodeGetLabel(fs);
        std::printf(
            "ret=%d lasttarget=%d jpc=%d calls=%u hash=0x%08x\n",
            res,
            fs->lasttarget,
            fs->jpc,
            g_trace.traceCalls,
            g_trace.traceHash);
        return 0;
    }

    if (op == "adjuststack") {
        if (argc != 6) {
            return 2;
        }
        const int32_t n = static_cast<int32_t>(std::strtol(argv[2], nullptr, 0));
        const uint32_t prevInst = static_cast<uint32_t>(std::strtoul(argv[3], nullptr, 0));
        const int32_t freereg = static_cast<int32_t>(std::strtol(argv[4], nullptr, 0));
        const int32_t maxstack = static_cast<int32_t>(std::strtol(argv[5], nullptr, 0));
        auto* code = reinterpret_cast<uint32_t*>(FromSimVa(kArenaBase + 0x3800u));
        fs->f = proto;
        fs->L = state;
        fs->ls = ls;
        ls->t.token = 280;
        fs->pc = 2;
        fs->lasttarget = 0;
        fs->freereg = static_cast<int16_t>(freereg);
        proto->code = code;
        proto->maxstacksize = static_cast<int16_t>(maxstack);
        code[1] = prevInst;
        LuaCodeAdjustStack(fs, n);
        std::printf(
            "pc=%d freereg=%d maxstack=%d lastInst=0x%08x calls=%u hash=0x%08x\n",
            fs->pc,
            fs->freereg,
            proto->maxstacksize,
            proto->code[fs->pc - 1],
            g_trace.traceCalls,
            g_trace.traceHash);
        return 0;
    }

    if (op == "getn") {
        if (argc != 5) {
            return 2;
        }
        const int32_t index = static_cast<int32_t>(std::strtol(argv[2], nullptr, 0));
        const int32_t ttype = static_cast<int32_t>(std::strtol(argv[3], nullptr, 0));
        const uint32_t auxVal = static_cast<uint32_t>(std::strtoul(argv[4], nullptr, 0));
        auto* stack = reinterpret_cast<LuaTObject*>(FromSimVa(kArenaBase + 0x1000u));
        auto* ht = reinterpret_cast<LuaHash*>(FromSimVa(kArenaBase + 0x2000u));
        ht->pad0C = auxVal;
        stack[1].ttype = ttype;
        stack[1].value.ht = ht;
        state->stack = stack;
        state->Cbase = stack + 1;
        state->top = stack + 2;
        const int32_t res = LuaGetN(state, index);
        std::printf("ret=%d\n", res);
        return 0;
    }

    if (op == "isfunction") {
        if (argc != 5) {
            return 2;
        }
        const int32_t index = static_cast<int32_t>(std::strtol(argv[2], nullptr, 0));
        const int32_t ttype = static_cast<int32_t>(std::strtol(argv[3], nullptr, 0));
        const uint32_t rawVal = static_cast<uint32_t>(std::strtoul(argv[4], nullptr, 0));
        auto* stack = reinterpret_cast<LuaTObject*>(FromSimVa(kArenaBase + 0x1000u));
        stack[1].ttype = ttype;
        stack[1].value.raw = rawVal;
        state->stack = stack;
        state->Cbase = stack + 1;
        state->top = stack + 2;
        const int32_t res = LuaIsFunction(state, index);
        std::printf("ret=%d\n", res);
        return 0;
    }

    if (op == "tostring") {
        if (argc != 4) {
            return 2;
        }
        const int32_t ttype = static_cast<int32_t>(std::strtol(argv[2], nullptr, 0));
        g_trace.stubRet = static_cast<int32_t>(std::strtol(argv[3], nullptr, 0));
        auto* stack = reinterpret_cast<LuaTObject*>(FromSimVa(kArenaBase + 0x1000u));
        auto* ts = reinterpret_cast<LuaTString*>(FromSimVa(kArenaBase + 0x2000u));
        std::strcpy(reinterpret_cast<char*>(ts) + sizeof(LuaTString), "direct");
        stack[1].ttype = ttype;
        stack[1].value.ts = ts;
        state->stack = stack;
        state->Cbase = stack + 1;
        state->top = stack + 2;
        const char* res = LuaToString(state, 1);
        std::printf(
            "ret=0x%08x calls=%u hash=0x%08x\n",
            ToSimVa(res),
            g_trace.traceCalls,
            g_trace.traceHash);
        return 0;
    }

    if (op == "newtable") {
        if (argc != 3) {
            return 2;
        }
        const int32_t atLast = static_cast<int32_t>(std::strtol(argv[2], nullptr, 0));
        auto* stack = reinterpret_cast<LuaTObject*>(FromSimVa(kArenaBase + 0x1000u));
        state->stack = stack;
        state->top = stack + 2;
        state->stackLast = (atLast != 0) ? state->top : (stack + 10);
        state->stackSize = 12;
        state->roottable = reinterpret_cast<LuaHash*>(FromSimVa(kArenaBase + 0x2000u));
        state->nblocks = 40;
        LuaNewTable(state, 99);
        std::printf(
            "top=0x%08x ttype=%d ht=0x%08x aux=%u nblocks=%u calls=%u hash=0x%08x\n",
            ToSimVa(state->top),
            stack[2].ttype,
            ToSimVa(stack[2].value.ht),
            stack[2].value.ht->pad0C,
            state->nblocks,
            g_trace.traceCalls,
            g_trace.traceHash);
        return 0;
    }

    if (op == "pushusertag") {
        if (argc != 5) {
            return 2;
        }
        const uint32_t uVa = static_cast<uint32_t>(std::strtoul(argv[2], nullptr, 0));
        const int32_t tag = static_cast<int32_t>(std::strtol(argv[3], nullptr, 0));
        const int32_t atLast = static_cast<int32_t>(std::strtol(argv[4], nullptr, 0));
        auto* stack = reinterpret_cast<LuaTObject*>(FromSimVa(kArenaBase + 0x1000u));
        state->stack = stack;
        state->top = stack + 2;
        state->stackLast = (atLast != 0) ? state->top : (stack + 10);
        state->stackSize = 12;
        state->lastTag = 5;
        LuaPushUserTag(state, FromSimVa(uVa), tag);
        std::printf(
            "top=0x%08x ttype=%d val=0x%08x calls=%u hash=0x%08x\n",
            ToSimVa(state->top),
            stack[2].ttype,
            ToSimVa(stack[2].value.ts),
            g_trace.traceCalls,
            g_trace.traceHash);
        return 0;
    }

    if (op == "getglobal") {
        if (argc != 3) {
            return 2;
        }
        const int32_t atLast = static_cast<int32_t>(std::strtol(argv[2], nullptr, 0));
        auto* stack = reinterpret_cast<LuaTObject*>(FromSimVa(kArenaBase + 0x1000u));
        state->stack = stack;
        state->top = stack + 2;
        state->stackLast = (atLast != 0) ? state->top : (stack + 10);
        state->stackSize = 12;
        auto* name = reinterpret_cast<char*>(FromSimVa(kArenaBase + 0x2000u));
        std::strcpy(name, "myGlobal");
        LuaGetGlobal(state, name);
        std::printf(
            "top=0x%08x ttype=%d raw=0x%llx calls=%u hash=0x%08x\n",
            ToSimVa(state->top),
            stack[2].ttype,
            static_cast<unsigned long long>(stack[2].value.raw),
            g_trace.traceCalls,
            g_trace.traceHash);
        return 0;
    }

    if (op == "newuserdata") {
        if (argc != 4) {
            return 2;
        }
        const uint32_t size = static_cast<uint32_t>(std::strtoul(argv[2], nullptr, 0));
        const int32_t atLast = static_cast<int32_t>(std::strtol(argv[3], nullptr, 0));
        auto* stack = reinterpret_cast<LuaTObject*>(FromSimVa(kArenaBase + 0x1000u));
        state->stack = stack;
        state->top = stack + 2;
        state->stackLast = (atLast != 0) ? state->top : (stack + 10);
        state->stackSize = 12;
        void* res = LuaNewUserData(state, size);
        std::printf(
            "ret=0x%08x top=0x%08x ttype=%d ts=0x%08x calls=%u hash=0x%08x\n",
            ToSimVa(res),
            ToSimVa(state->top),
            stack[2].ttype,
            ToSimVa(stack[2].value.ts),
            g_trace.traceCalls,
            g_trace.traceHash);
        return 0;
    }

    if (op == "tablenew") {
        if (argc != 3) {
            return 2;
        }
        const int32_t size = static_cast<int32_t>(std::strtol(argv[2], nullptr, 0));
        state->roottable = reinterpret_cast<LuaHash*>(FromSimVa(kArenaBase + 0x2000u));
        state->nblocks = 40;
        LuaHash* res = LuaTableNew(state, size);
        std::printf(
            "ret=0x%08x htag=%d next=0x%08x size=%d nblocks=%u calls=%u hash=0x%08x\n",
            ToSimVa(res),
            res->htag,
            ToSimVa(res->next),
            res->size,
            state->nblocks,
            g_trace.traceCalls,
            g_trace.traceHash);
        return 0;
    }

    if (op == "tableset") {
        if (argc != 5) {
            return 2;
        }
        const int32_t keyType = static_cast<int32_t>(std::strtol(argv[2], nullptr, 0));
        const int32_t mpKeyType = static_cast<int32_t>(std::strtol(argv[3], nullptr, 0));
        const int32_t collideMode = static_cast<int32_t>(std::strtol(argv[4], nullptr, 0));
        auto* ht = reinterpret_cast<LuaHash*>(FromSimVa(kArenaBase + 0x2000u));
        auto* nodes = reinterpret_cast<LuaNode*>(FromSimVa(kArenaBase + 0x2100u));
        auto* key = reinterpret_cast<LuaTObject*>(FromSimVa(kArenaBase + 0x2300u));
        ht->node = nodes;
        ht->size = 4;
        ht->firstfree = &nodes[3];
        nodes[3].key.ttype = kLuaTypeNil;
        g_trace.stubRet = 1;
        key->ttype = keyType;
        key->value.n = 10.0;
        nodes[1].key.ttype = mpKeyType;
        nodes[1].key.value.n = (collideMode == 0) ? 10.0 : 99.0;
        nodes[1].val.ttype = kLuaTypeNumber;
        if (collideMode == 2) {
            nodes[2].key.ttype = kLuaTypeNumber;
            nodes[2].key.value.n = 20.0;
            nodes[1].key = nodes[2].key;
            // make othern (&nodes[2]) != mp (&nodes[1])
            g_trace.stubRet = 1;
        }
        if (collideMode == 3) {
            ht->firstfree = &nodes[0];
            nodes[0].key.ttype = kLuaTypeNumber;
        }
        LuaTObject* res = LuaTableSet(state, ht, key);
        std::printf(
            "ret=0x%08x ff=0x%08x n1key=%d calls=%u hash=0x%08x\n",
            ToSimVa(res),
            ToSimVa(ht->firstfree),
            nodes[1].key.ttype,
            g_trace.traceCalls,
            g_trace.traceHash);
        return 0;
    }

    if (op == "code1") {
        if (argc != 4) {
            return 2;
        }
        const int32_t opcode = static_cast<int32_t>(std::strtol(argv[2], nullptr, 0));
        const int32_t arg1 = static_cast<int32_t>(std::strtol(argv[3], nullptr, 0));
        fs->pc = 7;
        const int32_t res = LuaCode1(fs, opcode, arg1);
        std::printf(
            "ret=%d pc=%d calls=%u hash=0x%08x\n",
            res,
            fs->pc,
            g_trace.traceCalls,
            g_trace.traceHash);
        return 0;
    }

    if (op == "fixjump") {
        if (argc != 5) {
            return 2;
        }
        const int32_t pc = static_cast<int32_t>(std::strtol(argv[2], nullptr, 0));
        const int32_t dest = static_cast<int32_t>(std::strtol(argv[3], nullptr, 0));
        const uint32_t initInst = static_cast<uint32_t>(std::strtoul(argv[4], nullptr, 0));
        auto* code = reinterpret_cast<uint32_t*>(FromSimVa(kArenaBase + 0x3800u));
        fs->f = proto;
        fs->ls = ls;
        ls->t.token = 285;
        proto->code = code;
        code[pc] = initInst;
        LuaCodeFixJump(fs, pc, dest);
        std::printf(
            "inst=0x%08x calls=%u hash=0x%08x\n",
            code[pc],
            g_trace.traceCalls,
            g_trace.traceHash);
        return 0;
    }

    if (op == "codecheckstack") {
        if (argc != 5) {
            return 2;
        }
        const int32_t freereg = static_cast<int32_t>(std::strtol(argv[2], nullptr, 0));
        const int32_t maxstack = static_cast<int32_t>(std::strtol(argv[3], nullptr, 0));
        const int32_t n = static_cast<int32_t>(std::strtol(argv[4], nullptr, 0));
        fs->f = proto;
        fs->ls = ls;
        ls->t.token = 286;
        fs->freereg = static_cast<int16_t>(freereg);
        proto->maxstacksize = static_cast<int16_t>(maxstack);
        LuaCodeCheckStack(fs, n);
        std::printf(
            "maxstack=%d calls=%u hash=0x%08x\n",
            proto->maxstacksize,
            g_trace.traceCalls,
            g_trace.traceHash);
        return 0;
    }

    if (op == "codeunary") {
        if (argc != 3) {
            return 2;
        }
        const int32_t unaryOp = static_cast<int32_t>(std::strtol(argv[2], nullptr, 0));
        auto* v = reinterpret_cast<LuaExpDesc*>(FromSimVa(kArenaBase + 0x2500u));
        LuaCodeUnary(fs, v, unaryOp);
        std::printf(
            "calls=%u hash=0x%08x a2=0x%08x a3=0x%08x\n",
            g_trace.traceCalls,
            g_trace.traceHash,
            g_trace.lastArg2,
            g_trace.lastArg3);
        return 0;
    }

    if (op == "codeinfix") {
        if (argc != 3) {
            return 2;
        }
        const int32_t binOp = static_cast<int32_t>(std::strtol(argv[2], nullptr, 0));
        ls->fs = fs;
        auto* v = reinterpret_cast<LuaExpDesc*>(FromSimVa(kArenaBase + 0x2500u));
        LuaCodeInfix(ls, binOp, v);
        std::printf(
            "calls=%u hash=0x%08x a2=0x%08x a3=0x%08x\n",
            g_trace.traceCalls,
            g_trace.traceHash,
            g_trace.lastArg2,
            g_trace.lastArg3);
        return 0;
    }

    if (op == "codeposfix") {
        if (argc != 5) {
            return 2;
        }
        const int32_t binOp = static_cast<int32_t>(std::strtol(argv[2], nullptr, 0));
        const int32_t v2t = static_cast<int32_t>(std::strtol(argv[3], nullptr, 0));
        const int32_t v2f = static_cast<int32_t>(std::strtol(argv[4], nullptr, 0));
        auto* code = reinterpret_cast<uint32_t*>(FromSimVa(kArenaBase + 0x3800u));
        ls->fs = fs;
        fs->f = proto;
        fs->ls = ls;
        fs->pc = 2;
        fs->lasttarget = 0;
        fs->freereg = 4;
        proto->code = code;
        proto->maxstacksize = 6;
        code[1] = 0x0000109Cu;  // OP_CALL (28)
        auto* v1 = reinterpret_cast<LuaExpDesc*>(FromSimVa(kArenaBase + 0x2500u));
        auto* v2 = reinterpret_cast<LuaExpDesc*>(FromSimVa(kArenaBase + 0x2600u));
        v1->t = 3;
        v1->f = 4;
        v2->t = v2t;
        v2->f = v2f;
        LuaCodePosfix(ls, binOp, v1, v2);
        std::printf(
            "v1t=%d v1f=%d code1=0x%08x freereg=%d calls=%u hash=0x%08x\n",
            v1->t,
            v1->f,
            code[1],
            fs->freereg,
            g_trace.traceCalls,
            g_trace.traceHash);
        return 0;
    }

    if (op == "stringinit") {
        if (argc != 2) {
            return 2;
        }
        state->nblocks = 16;
        LuaStringInit(state);
        std::printf(
            "strt=0x%08x udt=0x%08x ss=%d us=%d sn=%u un=%u nblocks=%u calls=%u hash=0x%08x\n",
            ToSimVa(state->strt.hash),
            ToSimVa(state->udt.hash),
            state->strt.size,
            state->udt.size,
            state->strt.nuse,
            state->udt.nuse,
            state->nblocks,
            g_trace.traceCalls,
            g_trace.traceHash);
        return 0;
    }

    return 2;
}
