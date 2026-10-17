#pragma once

#include <cstddef>
#include <cstdint>

namespace ra3::core {

struct LuaState;
struct LuaTString;
struct LuaHash;
struct LuaProto;
struct LuaClosure;
struct LuaNode;
struct LuaCallInfo;
struct LuaFuncState;
struct LuaLexState;
struct LuaZio;

// Lua 4.0 basic value types (ttype codes observed at 0x0043B020, 0x0049F650,
// 0x0041AA80, 0x00426E30, 0x004461A0, 0x0191B790, 0x0045EDC0, 0x00408C40).
enum LuaTagType : int32_t {
    kLuaTagDefault = -1,
    kLuaTypeUserdata = 0,
    kLuaTypeNil = 1,
    kLuaTypeNumber = 2,
    kLuaTypeString = 3,
    kLuaTypeTable = 4,
    kLuaTypeClosure = 5,
    kLuaTypeCMark = 6,
    kLuaNumTags = 6,
};

// 16-byte Lua 4.0 TObject layout:
//   +0x00 int32_t ttype
//   +0x04 int32_t pad
//   +0x08 union { double n; LuaTString* ts; LuaClosure* cl; LuaHash* ht; LuaCallInfo* i; void* p; }
struct LuaTObject {
    int32_t ttype = kLuaTypeNil;
    int32_t pad04 = 0;
    union ValueUnion {
        double n;
        LuaTString* ts;
        LuaClosure* cl;
        LuaHash* ht;
        LuaCallInfo* ci;
        void* ptr;
        uint64_t raw;
    } value{};
};
static_assert(sizeof(LuaTObject) == 0x10, "LuaTObject must be 16 bytes");

// 20-byte header + inline character bytes (sizeof header = 0x14, +1 NUL = 0x15):
//   +0x00 union { struct { int32_t tag; void* value; } d; struct { uint32_t hash; int32_t constIndex; } s; }
//   +0x08 uint32_t len
//   +0x0C LuaTString* nexthash
//   +0x10 int32_t marked
//   +0x14 char str[4]
struct LuaTString {
    union {
        struct {
            int32_t tag;
            void* value;
        } d;
        struct {
            uint32_t hash;
            int32_t constIndex;
        } s;
    } u{};
    uint32_t len = 0;
    LuaTString* nexthash = nullptr;
    int32_t marked = 0;
    char str[4]{};
};
static_assert(offsetof(LuaTString, len) == 0x08, "LuaTString::len offset");
static_assert(offsetof(LuaTString, nexthash) == 0x0C, "LuaTString::nexthash offset");
static_assert(offsetof(LuaTString, marked) == 0x10, "LuaTString::marked offset");
static_assert(offsetof(LuaTString, str) == 0x14, "LuaTString::str offset");

// 40-byte Lua 4.0 hash node (0x28 bytes, observed at 0x0049F4FE and 0x019069F0):
//   +0x00 LuaTObject key
//   +0x10 LuaTObject val
//   +0x20 LuaNode* next
//   +0x24 uint32_t pad24
struct LuaNode {
    LuaTObject key{};
    LuaTObject val{};
    LuaNode* next = nullptr;
    uint32_t pad24 = 0;
};
static_assert(sizeof(LuaNode) == 0x28, "LuaNode must be 40 bytes");

// 28-byte Lua 4.0 Hash table (0x1C bytes, observed at 0x00470720, 0x00404950, 0x019069F0):
//   +0x00 LuaNode* node
//   +0x04 int32_t htag
//   +0x08 int32_t size
//   +0x0C uint32_t pad0C
//   +0x10 LuaNode* firstfree
//   +0x14 LuaHash* next
//   +0x18 LuaHash* marked
struct LuaHash {
    LuaNode* node = nullptr;
    int32_t htag = kLuaTypeTable;
    int32_t size = 0;
    uint32_t pad0C = 0;
    LuaNode* firstfree = nullptr;
    LuaHash* next = nullptr;
    LuaHash* marked = nullptr;
};
static_assert(sizeof(LuaHash) == 0x1C, "LuaHash must be 28 bytes");

struct LuaLocVar {
    LuaTString* varname = nullptr;
    int32_t startpc = 0;
    int32_t endpc = 0;
};
static_assert(sizeof(LuaLocVar) == 0x0C, "LuaLocVar must be 12 bytes");

// Lua 4.0 Proto layout (observed at 0x019026A0, 0x00474880, 0x00467B40, 0x0047AA50):
//   +0x00 double* knum
//   +0x04 int32_t sizeknum
//   +0x08 LuaTString** kstr
//   +0x0C int32_t sizekstr
//   +0x10 LuaProto** kproto
//   +0x14 int32_t sizekproto
//   +0x18 uint32_t* code
//   +0x1C int32_t sizecode
//   +0x20 int16_t numparams
//   +0x22 int16_t is_vararg
//   +0x24 int16_t maxstacksize
//   +0x26 int16_t marked_short
//   +0x28 LuaProto* next
//   +0x2C int32_t* lineinfo
//   +0x30 int32_t sizelineinfo
//   +0x34 int32_t sizelocvars
//   +0x38 LuaLocVar* locvars
//   +0x3C int32_t lineDefined
//   +0x40 LuaTString* source
struct LuaProto {
    double* knum = nullptr;
    int32_t sizeknum = 0;
    LuaTString** kstr = nullptr;
    int32_t sizekstr = 0;
    LuaProto** kproto = nullptr;
    int32_t sizekproto = 0;
    uint32_t* code = nullptr;
    int32_t sizecode = 0;
    int16_t numparams = 0;
    int16_t is_vararg = 0;
    int16_t maxstacksize = 0;
    int16_t pad26 = 0;
    LuaProto* next = nullptr;
    int32_t* lineinfo = nullptr;
    int32_t sizelineinfo = 0;
    int32_t sizelocvars = 0;
    LuaLocVar* locvars = nullptr;
    int32_t lineDefined = 0;
    LuaTString* source = nullptr;
};
static_assert(offsetof(LuaProto, kstr) == 0x08, "LuaProto::kstr offset");
static_assert(offsetof(LuaProto, code) == 0x18, "LuaProto::code offset");
static_assert(offsetof(LuaProto, maxstacksize) == 0x24, "LuaProto::maxstacksize offset");
static_assert(offsetof(LuaProto, lineinfo) == 0x2C, "LuaProto::lineinfo offset");
static_assert(offsetof(LuaProto, locvars) == 0x38, "LuaProto::locvars offset");

// Lua 4.0 Closure layout (observed at 0x00404ED0, 0x0049F310, 0x00408C40):
//   +0x00 union { int32_t (*c)(LuaState*); LuaProto* l; } f
//   +0x04 LuaClosure* next
//   +0x08 LuaClosure* mark
//   +0x0C int16_t isC
//   +0x0E int16_t nelems
//   +0x10 LuaTObject upvalue[1]
struct LuaClosure {
    void* f = nullptr;
    LuaClosure* next = nullptr;
    LuaClosure* mark = nullptr;
    int16_t isC = 0;
    int16_t nelems = 0;
    LuaTObject upvalue[1]{};
};
static_assert(offsetof(LuaClosure, mark) == 0x08, "LuaClosure::mark offset");
static_assert(offsetof(LuaClosure, nelems) == 0x0E, "LuaClosure::nelems offset");
static_assert(offsetof(LuaClosure, upvalue) == 0x10, "LuaClosure::upvalue offset");

// Lua 4.0 CallInfo layout (observed at 0x01915010):
//   +0x00 LuaClosure* func (where *func is LuaProto* tf)
//   +0x04 const uint32_t** pc
//   +0x08 int32_t lastpc
//   +0x0C int32_t line
//   +0x10 int32_t refi
struct LuaCallInfo {
    LuaClosure* func = nullptr;
    const uint32_t** pc = nullptr;
    int32_t lastpc = 0;
    int32_t line = 0;
    int32_t refi = 0;
};
static_assert(sizeof(LuaCallInfo) == 0x14, "LuaCallInfo must be 20 bytes");

// 12-byte Lua 4.0 StringTable (observed at 0x004A2490, 0x01906B30, 0x0044DE30):
//   +0x00 int32_t size
//   +0x04 uint32_t nuse
//   +0x08 LuaTString** hash
struct LuaStringTable {
    int32_t size = 0;
    uint32_t nuse = 0;
    LuaTString** hash = nullptr;
};
static_assert(sizeof(LuaStringTable) == 0x0C, "LuaStringTable must be 12 bytes");

// 64-byte Lua 4.0 TM entry (observed at 0x00443DC0, 0x01906B30, 0x0049F650):
//   +0x00 LuaClosure* method[15]
//   +0x3C LuaTString* collected
struct LuaTM {
    LuaClosure* method[15]{};
    LuaTString* collected = nullptr;
};
static_assert(sizeof(LuaTM) == 0x40, "LuaTM must be 64 bytes");

// 24-byte Lua 4.0 Ref entry (observed at 0x0049F3A8):
//   +0x00 LuaTObject o
//   +0x10 int32_t status (-4 = LOCK)
//   +0x14 int32_t pad14
struct LuaRef {
    LuaTObject o{};
    int32_t status = 0;
    int32_t pad14 = 0;
};
static_assert(sizeof(LuaRef) == 0x18, "LuaRef must be 24 bytes");

struct LuaStkMark {
    LuaHash* tmark = nullptr;
    LuaClosure* cmark = nullptr;
};
static_assert(sizeof(LuaStkMark) == 0x08, "LuaStkMark must be 8 bytes");

struct LuaCallS {
    LuaTObject* func = nullptr;
    int32_t nresults = 0;
};
static_assert(sizeof(LuaCallS) == 0x08, "LuaCallS must be 8 bytes");

// 276-byte LuaZio buffer used in 0x00403EC0 (stack frame reserves 0x11C bytes).
struct LuaZio {
    uint32_t words[68]{};
};

// Lua 4.0 parser expression descriptor (12 bytes, observed at 0x01933830, 0x004A2BD0):
//   +0x00 int32_t k
//   +0x04 int32_t info_or_t
//   +0x08 int32_t aux_or_f
struct LuaExpDesc {
    int32_t k = 0;
    int32_t t = -1;
    int32_t f = -1;
};
static_assert(sizeof(LuaExpDesc) == 0x0C, "LuaExpDesc must be 12 bytes");

union LuaSemInfo {
    double r;
    LuaTString* ts;
};
static_assert(sizeof(LuaSemInfo) == 0x08, "LuaSemInfo must be 8 bytes");

struct LuaToken {
    int32_t token = 0;
    int32_t pad04 = 0;
    LuaSemInfo seminfo{};
};
static_assert(sizeof(LuaToken) == 0x10, "LuaToken must be 16 bytes");

// Lua 4.0 FuncState layout (observed at 0x019026A0, 0x00474880, 0x0047AA50,
// 0x00435790, 0x01913030, 0x004975D0, 0x01907050):
//   +0x00 LuaProto* f
//   +0x04 LuaFuncState* prev
//   +0x08 LuaLexState* ls
//   +0x0C LuaState* L
//   +0x10 int32_t pc
//   +0x14 int32_t lasttarget
//   +0x18 int32_t jpc
//   +0x1C int16_t freereg
//   +0x1E int16_t nactvar
//   +0x20 int32_t upvalues[32] (or jump/break lists; +0x24 is lastline in 0x0040CD80)
struct LuaFuncState {
    LuaProto* f = nullptr;
    LuaFuncState* prev = nullptr;
    LuaLexState* ls = nullptr;
    LuaState* L = nullptr;
    int32_t pc = 0;
    int32_t lasttarget = 0;
    int32_t jpc = -1;
    int16_t freereg = 0;
    int16_t nactvar = 0;
    int32_t unk20 = 0;
    int32_t lastline = 0;
};
static_assert(offsetof(LuaFuncState, pc) == 0x10, "LuaFuncState::pc offset");
static_assert(offsetof(LuaFuncState, lasttarget) == 0x14, "LuaFuncState::lasttarget offset");
static_assert(offsetof(LuaFuncState, jpc) == 0x18, "LuaFuncState::jpc offset");
static_assert(offsetof(LuaFuncState, freereg) == 0x1C, "LuaFuncState::freereg offset");
static_assert(offsetof(LuaFuncState, nactvar) == 0x1E, "LuaFuncState::nactvar offset");
static_assert(offsetof(LuaFuncState, lastline) == 0x24, "LuaFuncState::lastline offset");

// Lua 4.0 LexState layout (observed at 0x019149F0, 0x004A58A0, 0x004A1440,
// 0x00474880, 0x00439CA0, 0x019026A0, 0x01933830):
//   +0x00 int32_t current
//   +0x04 int32_t pad04
//   +0x08 LuaToken t (16 bytes: +0x08 token, +0x10 seminfo)
//   +0x18 LuaToken lookahead (16 bytes: +0x18 token, +0x20 seminfo)
//   +0x28 LuaFuncState* fs
//   +0x2C LuaState* L
//   +0x30 LuaZio* z
//   +0x34 int32_t linenumber
//   +0x38 int32_t lastline
//   +0x3C LuaTString* source
struct LuaLexState {
    int32_t current = 0;
    int32_t pad04 = 0;
    LuaToken t{};
    LuaToken lookahead{};
    LuaFuncState* fs = nullptr;
    LuaState* L = nullptr;
    LuaZio* z = nullptr;
    int32_t linenumber = 0;
    int32_t lastline = 0;
    LuaTString* source = nullptr;
};
static_assert(offsetof(LuaLexState, t) == 0x08, "LuaLexState::t offset");
static_assert(offsetof(LuaLexState, lookahead) == 0x18, "LuaLexState::lookahead offset");
static_assert(offsetof(LuaLexState, fs) == 0x28, "LuaLexState::fs offset");
static_assert(offsetof(LuaLexState, L) == 0x2C, "LuaLexState::L offset");
static_assert(offsetof(LuaLexState, linenumber) == 0x34, "LuaLexState::linenumber offset");
static_assert(offsetof(LuaLexState, lastline) == 0x38, "LuaLexState::lastline offset");

// Lua 4.0 lua_State layout (observed across all 44 functions):
//   +0x00 LuaTObject* top
//   +0x04 LuaTObject* stack
//   +0x08 LuaTObject* stackLast
//   +0x0C int32_t stackSize
//   +0x10 LuaTObject* Cbase
//   +0x14 uint32_t pad14[4]
//   +0x24 LuaProto* rootproto
//   +0x28 LuaHash* roottable
//   +0x2C LuaStringTable strt (size +0x2C, nuse +0x30, hash +0x34)
//   +0x38 LuaStringTable udt  (size +0x38, nuse +0x3C, hash +0x40)
//   +0x44 LuaHash* gt
//   +0x48 LuaTM* TMtable
//   +0x4C int32_t lastTag
//   +0x50 LuaRef* refArray
//   +0x54 int32_t refSize
//   +0x58 uint32_t pad58
//   +0x5C uint32_t GCthreshold
//   +0x60 uint32_t nblocks
//   +0x64 uint32_t pad64[2]
//   +0x6C int32_t allowhooks
struct LuaState {
    LuaTObject* top = nullptr;
    LuaTObject* stack = nullptr;
    LuaTObject* stackLast = nullptr;
    int32_t stackSize = 0;
    LuaTObject* Cbase = nullptr;
    uint32_t pad14[4]{};
    LuaProto* rootproto = nullptr;
    LuaHash* roottable = nullptr;
    LuaStringTable strt{};
    LuaStringTable udt{};
    LuaHash* gt = nullptr;
    LuaTM* TMtable = nullptr;
    int32_t lastTag = 0;
    LuaRef* refArray = nullptr;
    int32_t refSize = 0;
    uint32_t pad58 = 0;
    uint32_t GCthreshold = 0;
    uint32_t nblocks = 0;
    uint32_t pad64[2]{};
    int32_t allowhooks = 0;
};
static_assert(offsetof(LuaState, top) == 0x00, "LuaState::top offset");
static_assert(offsetof(LuaState, stack) == 0x04, "LuaState::stack offset");
static_assert(offsetof(LuaState, stackLast) == 0x08, "LuaState::stackLast offset");
static_assert(offsetof(LuaState, stackSize) == 0x0C, "LuaState::stackSize offset");
static_assert(offsetof(LuaState, Cbase) == 0x10, "LuaState::Cbase offset");
static_assert(offsetof(LuaState, rootproto) == 0x24, "LuaState::rootproto offset");
static_assert(offsetof(LuaState, roottable) == 0x28, "LuaState::roottable offset");
static_assert(offsetof(LuaState, strt) == 0x2C, "LuaState::strt offset");
static_assert(offsetof(LuaState, udt) == 0x38, "LuaState::udt offset");
static_assert(offsetof(LuaState, gt) == 0x44, "LuaState::gt offset");
static_assert(offsetof(LuaState, TMtable) == 0x48, "LuaState::TMtable offset");
static_assert(offsetof(LuaState, lastTag) == 0x4C, "LuaState::lastTag offset");
static_assert(offsetof(LuaState, refArray) == 0x50, "LuaState::refArray offset");
static_assert(offsetof(LuaState, refSize) == 0x54, "LuaState::refSize offset");
static_assert(offsetof(LuaState, GCthreshold) == 0x5C, "LuaState::GCthreshold offset");
static_assert(offsetof(LuaState, nblocks) == 0x60, "LuaState::nblocks offset");
static_assert(offsetof(LuaState, allowhooks) == 0x6C, "LuaState::allowhooks offset");

// External runtime callbacks for callees outside the 44-function entry-lost batch.
struct LuaVmRuntimeHooks {
    void* (*crtRealloc)(void* ptr, uint32_t size) = nullptr;
    void (*crtFree)(void* ptr) = nullptr;
    void (*luaError)(LuaState* state, const char* msg) = nullptr;
    void (*luaBreakRun)(LuaState* state, int32_t errcode) = nullptr;
    void (*luaDoError)(LuaState* state, const char* msg) = nullptr;
    int32_t (*luaRunProtected)(LuaState* state, uint32_t funcVa, LuaCallS* ud) = nullptr;
    LuaZio* (*luaZioMemOpen)(LuaZio* z, const char* buff, uint32_t size, const char* name) = nullptr;
    int32_t (*luaProtectedParser)(LuaState* state, LuaZio* z, int32_t bin) = nullptr;
    void (*luaTokenToString)(int32_t token, char* outBuf) = nullptr;
    void (*luaLexError)(LuaLexState* ls, const char* msg) = nullptr;
    void (*luaSyntaxError)(LuaLexState* ls, const char* msg, const char* token) = nullptr;
    int32_t (*luaLex)(LuaLexState* ls, LuaSemInfo* seminfo) = nullptr;
    void (*luaErrorExpected)(LuaLexState* ls, int32_t token) = nullptr;
    int32_t (*luaCodeKstr)(LuaLexState* ls, int32_t index) = nullptr;
    void (*luaCodeNil)(LuaLexState* ls, int32_t n) = nullptr;
    int32_t (*luaCode0)(LuaFuncState* fs, int32_t op) = nullptr;
    void (*luaRemoveLocalVars)(LuaLexState* ls, int32_t nvars) = nullptr;
    void (*luaProtoOk)(LuaState* state, LuaProto* f, int32_t pc) = nullptr;
    int32_t (*luaFuncName)(LuaLexState* ls, LuaExpDesc* v) = nullptr;
    void (*luaBody)(LuaLexState* ls, int32_t needself, int32_t line) = nullptr;
    void (*luaCodeStoreVar)(LuaLexState* ls, LuaExpDesc* v) = nullptr;
    int32_t (*luaDebugGetLine)(const int32_t* lineinfo, int32_t pc, int32_t refline, int32_t* refi) = nullptr;
    void (*luaCallLineHook)(LuaState* state, LuaTObject* topMinus2, int32_t newline, uint32_t linehook) = nullptr;
    LuaClosure* (*luaFuncNewClosure)(LuaState* state, int32_t nelems) = nullptr;
    void (*luaFuncFreeProto)(LuaState* state, LuaProto* f) = nullptr;
    void (*luaTableFree)(LuaState* state, LuaHash* t) = nullptr;
    void (*luaStringResize)(LuaState* state, LuaStringTable* tb, int32_t newsize) = nullptr;
    void (*luaGcMarkLock)(LuaState* state, LuaStkMark* st) = nullptr;
    void (*luaTableRemove)(LuaHash* t, LuaNode* n) = nullptr;
    int32_t (*luaCheckEvent)(const char* event) = nullptr;
    void (*luaVError)(LuaState* state, const char* fmt, const char* a0, const char* a1, const char* a2, int32_t i0) = nullptr;
    int32_t (*luaVmToString)(LuaState* state, LuaTObject* obj) = nullptr;
    LuaTString* (*luaStringCreateUdata)(LuaState* state, void* udata, int32_t tag) = nullptr;
    LuaTString* (*luaStringNew)(LuaState* state, const char* str) = nullptr;
    const LuaTObject* (*luaVmGetGlobal)(LuaState* state, LuaTString* name) = nullptr;
    LuaTString* (*luaStringNewUdata)(LuaState* state, uint32_t size, void* udata) = nullptr;
    void (*luaTableSetNodeVector)(LuaState* state, LuaHash* t, uint32_t size) = nullptr;
    LuaNode* (*luaTableMainPosition)(const LuaHash* t, const LuaTObject* key) = nullptr;
    void (*luaTableRehash)(LuaState* state, LuaHash* t) = nullptr;
    int32_t (*luaCode2)(LuaFuncState* fs, int32_t op, int32_t arg1, int32_t arg2) = nullptr;
    void (*luaCodePatchListAux)(LuaFuncState* fs, int32_t list, int32_t vtarget, int32_t reg, int32_t dtarget) = nullptr;
    void (*luaCodeRememberLineInfo)(LuaFuncState* fs) = nullptr;
    void (*luaCodeArith)(LuaFuncState* fs, LuaExpDesc* v, int32_t arg, int32_t op) = nullptr;
    int32_t (*luaCodeExp2Val)(LuaFuncState* fs, LuaExpDesc* v) = nullptr;
    void (*luaCodeConcat)(LuaFuncState* fs, int32_t* l1, int32_t l2) = nullptr;
    void (*luaCodePrefix)(LuaLexState* ls, LuaExpDesc* v, int32_t n) = nullptr;
};

void SetLuaVmRuntimeHooks(const LuaVmRuntimeHooks& hooks);
const LuaVmRuntimeHooks& GetLuaVmRuntimeHooks();

// Clean .text helper 0x00404070 (luaO_power2) used by 0x004036D0 and 0x0040B410.
uint32_t LuaPower2(uint32_t n);

// Clean .text helper 0x00408C40 (markobject) used by 0x00409120.
void LuaGcMarkObject(LuaStkMark* st, LuaTObject* o);

// Clean .text helper 0x00404090 (luaO_equalObj) used by 0x0040B570.
int32_t LuaEqualObj(const LuaTObject* t1, const LuaTObject* t2);

// [1/44] site=0x00403660 slot=0x018FB434 body=0x004A3130 section=.text
void* LuaMemRealloc(LuaState* state, void* oldBlock, uint32_t size);

// [2/44] site=0x004036D0 slot=0x018F504C body=0x019082A0 section=.securom
void* LuaMemGrowAux(
    LuaState* state,
    void* block,
    uint32_t nelems,
    int32_t inc,
    uint32_t elementSize,
    const char* errormsg,
    uint32_t limit);

// [3/44] site=0x00403740 slot=0x018F640C body=0x00466F20 section=.text
void LuaDoInitStack(LuaState* state, int32_t stackSize);

// [4/44] site=0x004038B0 slot=0x018FB698 body=0x004A4010 section=.text
void LuaDoCheckStack(LuaState* state, int32_t needed);

// [5/44] site=0x00403CA0 slot=0x018FB0A4 body=0x004252F0 section=.text
int32_t LuaProtectedCall(LuaState* state, int32_t nargs, int32_t nresults);

// [6/44] site=0x00403EC0 slot=0x018FA148 body=0x00406550 section=.text
int32_t LuaDoBuffer(LuaState* state, const char* buff, uint32_t size, const char* name);

// [7/44] site=0x00404370 slot=0x018F5E88 body=0x019149F0 section=.securom
void LuaParserCheckNext(LuaLexState* ls, int32_t token);

// [8/44] site=0x004043F0 slot=0x018F7340 body=0x004A58A0 section=.text
int32_t LuaParserOptional(LuaLexState* ls, int32_t token);

// [9/44] site=0x00404450 slot=0x018FB1B8 body=0x004A1440 section=.text
void LuaParserCheckMatch(LuaLexState* ls, int32_t what, int32_t who, int32_t where);

// [10/44] site=0x00404550 slot=0x018F6120 body=0x00474880 section=.text
int32_t LuaParserStringK(LuaLexState* ls, LuaTString* str);

// [11/44] site=0x004048E0 slot=0x018FA584 body=0x00439CA0 section=.text
void LuaParserAdjustMultAssign(LuaLexState* ls, int32_t nvars, int32_t nexps);

// [12/44] site=0x00404AC0 slot=0x018FBEAC body=0x019026A0 section=.securom
void LuaParserCloseFunc(LuaLexState* ls);

// [13/44] site=0x00405B10 slot=0x018F71A0 body=0x01933830 section=.securom
void LuaParserFuncStat(LuaLexState* ls, int32_t line);

// [14/44] site=0x00406CF0 slot=0x018F5EE8 body=0x01915010 section=.securom
void LuaVmTraceExec(LuaState* state, LuaTObject* top, uint32_t linehook);

// [15/44] site=0x00406D70 slot=0x018F9FF4 body=0x00404ED0 section=.text
LuaClosure* LuaVmClosure(LuaState* state, int32_t nelems);

// [16/44] site=0x00408DB0 slot=0x018F6040 body=0x00467B40 section=.text
void LuaGcCollectProto(LuaState* state);

// [17/44] site=0x00408DF0 slot=0x018FA02C body=0x00404950 section=.text
void LuaGcCollectTable(LuaState* state);

// [18/44] site=0x00408E30 slot=0x018FB34C body=0x004A2490 section=.text
void LuaGcCollectStrings(LuaState* state, int32_t all);

// [19/44] site=0x00408ED0 slot=0x018FC3F0 body=0x01906B30 section=.securom
void LuaGcCollectUdata(LuaState* state, int32_t all);

// [20/44] site=0x00409120 slot=0x018FAE84 body=0x0049F310 section=.text
void LuaGcMarkAll(LuaState* state);

// [21/44] site=0x00409E90 slot=0x018FC3D0 body=0x00443DC0 section=.text
int32_t LuaNewTag(LuaState* state);

// [22/44] site=0x0040A070 slot=0x018F5B20 body=0x0043B020 section=.text
int32_t LuaTagOfObject(const LuaTObject* o);

// [23/44] site=0x0040A120 slot=0x018FAEB8 body=0x0049F650 section=.text
void LuaSetTagMethod(LuaState* state, int32_t tag, const char* event);

// [24/44] site=0x0040A340 slot=0x018FC148 body=0x004431E0 section=.text
void LuaPushValue(LuaState* state, int32_t index);

// [25/44] site=0x0040A440 slot=0x018FB260 body=0x0041AA80 section=.text
int32_t LuaGetN(LuaState* state, int32_t index);

// [26/44] site=0x0040A5C0 slot=0x018FB380 body=0x00426E30 section=.text
int32_t LuaIsFunction(LuaState* state, int32_t index);

// [27/44] site=0x0040A600 slot=0x018F50B8 body=0x004461A0 section=.text
const char* LuaToString(LuaState* state, int32_t index);

// [28/44] site=0x0040A860 slot=0x018F6608 body=0x0191B790 section=.securom
void LuaPushUserTag(LuaState* state, void* udata, int32_t tag);

// [29/44] site=0x0040A8C0 slot=0x018FB6F4 body=0x0043E3B0 section=.text
void LuaGetGlobal(LuaState* state, const char* name);

// [30/44] site=0x0040AAD0 slot=0x018F5C50 body=0x0045EDC0 section=.text
void LuaNewTable(LuaState* state, uint32_t aux = 0);

// [31/44] site=0x0040AEF0 slot=0x018FB394 body=0x0043D6E0 section=.text
void* LuaNewUserData(LuaState* state, uint32_t size);

// [32/44] site=0x0040B410 slot=0x018F603C body=0x00470720 section=.text
LuaHash* LuaTableNew(LuaState* state, int32_t nhash, uint32_t aux = 0);

// [33/44] site=0x0040B570 slot=0x018FC3E8 body=0x019069F0 section=.securom
LuaTObject* LuaTableSet(LuaState* state, LuaHash* t, const LuaTObject* key);

// [34/44] site=0x0040C920 slot=0x018F5474 body=0x0045A730 section=.text
void LuaCodeFixJump(LuaFuncState* fs, int32_t pc, int32_t dest);

// [35/44] site=0x0040C980 slot=0x018F6788 body=0x0047AA50 section=.text
void LuaCodeCheckStack(LuaFuncState* fs, int32_t n);

// [36/44] site=0x0040CA60 slot=0x018FBEFC body=0x00435790 section=.text
int32_t LuaCodeLastIsOpen(const LuaFuncState* fs);

// [37/44] site=0x0040CAA0 slot=0x018F5C84 body=0x01913030 section=.securom
void LuaCodeSetCallReturns(LuaFuncState* fs, int32_t nresults);

// [38/44] site=0x0040D220 slot=0x018FC078 body=0x0042CE50 section=.text
int32_t LuaCode1(LuaFuncState* fs, int32_t op, int32_t arg1);

// [39/44] site=0x0040D2F0 slot=0x018F725C body=0x004975D0 section=.text
int32_t LuaCodeGetLabel(LuaFuncState* fs);

// [40/44] site=0x0040D5B0 slot=0x018FC418 body=0x01907050 section=.securom
void LuaCodeAdjustStack(LuaFuncState* fs, int32_t n);

// [41/44] site=0x0040DC40 slot=0x018FB15C body=0x0040B6E0 section=.text
void LuaCodeUnary(LuaFuncState* fs, LuaExpDesc* v, int32_t op);

// [42/44] site=0x0040E1D0 slot=0x018F5C74 body=0x0045EF60 section=.text
void LuaCodeInfix(LuaLexState* ls, int32_t op, LuaExpDesc* v);

// [43/44] site=0x0040E220 slot=0x018FB3A0 body=0x004A2BD0 section=.text
void LuaCodePosfix(LuaLexState* ls, int32_t op, LuaExpDesc* v1, LuaExpDesc* v2);

// [44/44] site=0x0040E2F0 slot=0x018F4FDC body=0x0044DE30 section=.text
void LuaStringInit(LuaState* state);

}  // namespace ra3::core
