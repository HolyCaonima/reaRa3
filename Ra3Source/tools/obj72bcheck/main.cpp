#include "ra3/gamelogic/ObjectModules72B.h"

#include <cstdint>
#include <cstring>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

// Line protocol shared with tools/analysis/ports/verify_obj72b.py.
// Every direct or virtual call notes a token and advances seq, even when the
// value returned to the body is a planted pointer rather than seq*0x10.

namespace {

std::string g_log;
int g_seq = 0;
std::uint32_t g_usable = 1;
std::uint32_t g_ready = 1;
std::uint32_t g_query = 0;
std::uint32_t g_cur = 0;
std::uint32_t g_near = 1;
std::uint32_t g_module = 0;
std::uint32_t g_chain = 0;
std::uint32_t g_count = 0;
std::uint32_t g_whit = 0;
std::uint32_t g_whit2 = 0;
int g_verOut = -1;
int g_v70arm = 0;
bool g_poke = false;
bool g_findHit = false;
std::uint32_t g_pred = 0x110;
bool g_mapHit = false;
std::uint32_t g_mapValue = 0;
std::uint32_t g_hitNode = 0;
std::uint32_t g_endNode = 0;
std::uint32_t g_node = 0;
std::uint32_t g_listVt = 0;

struct Base {
    char tag;
    const std::uint8_t* address;
    std::size_t size;
};

std::vector<Base> g_bases;

struct Arena {
    std::vector<std::vector<std::uint8_t>> blocks;
    std::uint8_t* alloc(std::size_t size) {
        blocks.emplace_back(size + 0x40, static_cast<std::uint8_t>(0));
        return blocks.back().data();
    }
};

Arena g_arena;
ra3::gamelogic::Object72BEnv g_env;

std::uint32_t loadU32(const void* address) {
    std::uint32_t value = 0;
    std::memcpy(&value, address, 4);
    return value;
}

void storeU32(void* address, std::uint32_t value) {
    std::memcpy(address, &value, 4);
}

std::string hex8(std::uint32_t value) {
    std::ostringstream text;
    text << std::hex << std::nouppercase << std::setw(8) << std::setfill('0') << value;
    return text.str();
}

std::string fmt(std::uint32_t value) {
    if (value < 0x10000u || value >= 0xF0000000u) {
        std::ostringstream text;
        text << std::hex << std::nouppercase << value;
        return text.str();
    }
    return "p";
}

void addLog(const std::string& text) {
    if (!g_log.empty()) {
        g_log += ",";
    }
    g_log += text;
}

void note(const std::string& text) {
    ++g_seq;
    addLog(text);
}

std::uint32_t take(const std::string& text) {
    note(text);
    return static_cast<std::uint32_t>(g_seq) * 0x10u;
}

std::string rel(const void* pointer) {
    const auto address = reinterpret_cast<std::uintptr_t>(pointer);
    for (const Base& base : g_bases) {
        const auto start = reinterpret_cast<std::uintptr_t>(base.address);
        if (address >= start && address - start < base.size) {
            std::ostringstream text;
            text << base.tag << std::hex << std::nouppercase << (address - start);
            return text.str();
        }
    }
    return fmt(static_cast<std::uint32_t>(address));
}

std::string floats3(const void* pointer) {
    const auto* bytes = static_cast<const std::uint8_t*>(pointer);
    return hex8(loadU32(bytes)) + ":" + hex8(loadU32(bytes + 4)) + ":" + hex8(loadU32(bytes + 8));
}

void watch(char tag, const std::uint8_t* address, std::size_t size) {
    g_bases.push_back({tag, address, size});
}

std::uint32_t __fastcall v0(void*, void*) { return take("v34"); }
std::uint32_t __fastcall v1c(void*, void*, std::uint32_t arg) { return take("v1c:" + fmt(arg)); }
std::uint32_t __fastcall v38(void*, void*, std::uint32_t arg) { return take("v38:" + fmt(arg)); }
std::uint32_t __fastcall vDel(void*, void*, std::uint32_t arg) { return take("del:" + fmt(arg)); }
std::uint32_t __fastcall vCount(void*, void*, std::uint32_t) {
    note("n:" + fmt(g_count));
    return g_count;
}
std::uint32_t __fastcall vPass(void*, void*, std::uint32_t) {
    note("ps");
    return g_node;
}
std::uint32_t __fastcall vW24(void*, void*) {
    note("w24");
    return g_whit;
}
std::uint32_t __fastcall vW20(void*, void*, std::uint32_t arg) {
    note("w20:" + fmt(arg));
    return g_whit2;
}
std::uint32_t __fastcall vChain(void*, void*) {
    note("ch");
    return g_chain;
}
std::uint32_t __fastcall vHand(void*, void*, std::uint32_t a, std::uint32_t b) {
    return take("hv:" + fmt(a) + ":" + fmt(b));
}

std::uint32_t onPtr(int slot, void* arg) {
    auto* bytes = static_cast<std::uint8_t*>(arg);
    if (slot == 0x18) {
        std::ostringstream text;
        text << std::hex << std::nouppercase << "v18:" << std::setw(2) << std::setfill('0')
             << static_cast<int>(bytes[0]) << std::setw(2) << std::setfill('0') << static_cast<int>(bytes[1]);
        if (g_verOut >= 0) {
            bytes[1] = static_cast<std::uint8_t>(g_verOut);
        }
        return take(text.str());
    }
    if (slot == 0x7C) {
        return take("v7c:" + rel(arg) + ":" + fmt(bytes[0]));
    }
    if (slot == 0x70 && g_v70arm > 0) {
        if (g_v70arm == 1) {
            storeU32(arg, 0x55);
            g_v70arm = 0;
        } else {
            --g_v70arm;
        }
    }
    if (slot == 0x84 && g_poke) {
        const auto token = take("v84:" + rel(arg) + ":" + fmt(bytes[0]));
        bytes[0] = 1;
        g_poke = false;
        return token;
    }
    if (slot == 0x84) {
        return take("v84:" + rel(arg) + ":" + fmt(bytes[0]));
    }
    std::ostringstream text;
    text << "v" << std::hex << slot << ":" << rel(arg);
    return take(text.str());
}

std::uint32_t __fastcall v18(void*, void*, void* arg) { return onPtr(0x18, arg); }
std::uint32_t __fastcall v58(void*, void*, void* arg) { return onPtr(0x58, arg); }
std::uint32_t __fastcall v64(void*, void*, void* arg) { return onPtr(0x64, arg); }
std::uint32_t __fastcall v6c(void*, void*, void* arg) { return onPtr(0x6C, arg); }
std::uint32_t __fastcall v70(void*, void*, void* arg) { return onPtr(0x70, arg); }
std::uint32_t __fastcall v7c(void*, void*, void* arg) { return onPtr(0x7C, arg); }
std::uint32_t __fastcall v84(void*, void*, void* arg) { return onPtr(0x84, arg); }
std::uint32_t __fastcall v04(void*, void*, std::uint32_t arg) { return take("v4:" + hex8(arg)); }
std::uint32_t __fastcall v08(void*, void*) { return take("v8"); }
std::uint32_t __fastcall v0c(void*, void*, std::uint32_t arg) { return take("vc:" + hex8(arg)); }
std::uint32_t __fastcall v14(void*, void*, std::uint32_t field, std::uint32_t imm) {
    return take("v14:" + rel(reinterpret_cast<void*>(field)) + ":" + fmt(imm));
}
std::uint32_t __fastcall v20(void*, void*, std::uint32_t arg) { return take("v20:" + fmt(arg)); }

std::uint32_t __fastcall usable(void*, void*) {
    note("u:" + fmt(g_usable));
    return g_usable;
}
std::uint32_t __cdecl nearCheck(void*, void*, void*) {
    note("near:" + fmt(g_near));
    return g_near;
}
std::uint32_t __fastcall steer(void*, void*, void* pos, std::uint32_t bits, std::uint32_t mode) {
    return take("st:" + floats3(pos) + ":" + hex8(bits) + ":" + fmt(mode));
}
std::uint32_t __fastcall place(void*, void*, void* pos, std::uint32_t bits) {
    return take("pl:" + floats3(pos) + ":" + hex8(bits));
}
std::uint32_t __fastcall followCf(void*, void*, void* pos) { return take("cf:" + floats3(pos)); }
std::uint32_t __fastcall followCe(void*, void*, void* pos) { return take("ce:" + floats3(pos)); }
std::uint32_t __fastcall followCd(void*, void*, std::uint32_t arg) { return take("cd:" + fmt(arg)); }
std::uint32_t __fastcall bindPath(void*, void*, void*) { return take("bp"); }
std::uint32_t __fastcall findWeapon(void*, void*, std::uint32_t) {
    note(g_findHit ? "fw" : "fw0");
    return g_findHit ? g_whit : 0u;
}
std::uint32_t __fastcall findWeaponAlt(void*, void*, std::uint32_t, std::uint32_t) {
    note(g_findHit ? "fa" : "fa0");
    return g_findHit ? g_whit : 0u;
}
std::uint32_t __fastcall markObject(void*, void*, std::uint32_t arg) { return take("mk:" + fmt(arg)); }
std::uint32_t __fastcall nameKey(void*, void*, std::uint32_t text) { return take("nk:" + hex8(text)); }
std::uint32_t __fastcall findModule(void*, void*, std::uint32_t key) {
    note("fm:" + fmt(key));
    return g_module;
}
std::uint32_t __fastcall pokeModule(void*, void*) { return take("pk"); }
std::uint32_t __fastcall disguiseSlot(void*, void*, std::uint32_t zero) { return take("dg:" + fmt(zero)); }
std::uint32_t __fastcall visitHeld(void*, void*, std::uint32_t id) { return take("vh:" + fmt(id)); }
std::uint32_t __fastcall queryLoco(void*, void*) {
    note("q:" + fmt(g_query));
    return g_query;
}
std::uint32_t __fastcall currentTarget(void*, void*) {
    note("cur:" + fmt(g_cur));
    return g_cur;
}
std::uint32_t __fastcall notePose(void*, void*, void*) { return take("np"); }
std::uint32_t __fastcall scriptCall(void*, void*, std::uint32_t obj, std::uint32_t zero) {
    return take("sc:" + fmt(obj) + ":" + fmt(zero));
}
std::uint32_t __fastcall encloseTest(void*, void*, void*) {
    note("et:" + fmt(g_pred));
    return g_pred;
}
std::uint32_t __fastcall applyKey(void*, void*, std::uint32_t id) { return take("key:" + fmt(id)); }
std::uint32_t __fastcall xferBase(void*, void*, std::uint32_t a, std::uint32_t b) {
    return take("xb:" + fmt(a) + ":" + fmt(b));
}
std::uint32_t __fastcall lockMap(void*, void*, std::uint32_t) { return take("lk"); }
std::uint32_t __fastcall findMap(void*, void*, std::uint32_t* out, std::uint32_t*) {
    note("fmap");
    *out = g_mapHit ? g_hitNode : g_endNode;
    return 0;
}
std::uint32_t __fastcall unlockMap(void*, void*, std::uint32_t) { return take("ul"); }
std::uint32_t __fastcall floatCall(void*, void*, std::uint32_t bits, std::uint32_t id) {
    return take("fc:" + hex8(bits) + ":" + fmt(id));
}
std::uint32_t __fastcall ready(void*, void*) {
    note("rdy:" + fmt(g_ready));
    return g_ready;
}
std::uint32_t __fastcall xferAbility(void*, void*, void*) { return take("ab"); }
std::uint32_t __fastcall xferRoot(void*, void*, void*) { return take("rt"); }
std::uint32_t __fastcall xferAt10Real(void* self, void*, void*) { return take("a10:" + rel(self)); }
std::uint32_t __fastcall xferStealth(void*, void*, void*) { return take("sth"); }
std::uint32_t __fastcall xferAi(void*, void*, void*) { return take("ai"); }
std::uint32_t __fastcall xferMissile(void*, void*, void*) { return take("ms"); }
std::uint32_t __cdecl xferField(void*, void* field) { return take("fd:" + rel(field)); }
std::uint32_t __cdecl xferFieldB(void*, void* field) { return take("fb:" + rel(field)); }
std::uint32_t __cdecl xferPair(void*, void* field) { return take("pr:" + rel(field)); }
std::uint32_t __cdecl xferPairB(void*, void* field) { return take("pb:" + rel(field)); }
std::uint32_t __cdecl xferExtra(void*, void* field) { return take("ex:" + rel(field)); }
std::uint32_t __cdecl xferString(void*, void* field) {
    return take("xs:" + hex8(loadU32(field)) + ":" + hex8(loadU32(static_cast<std::uint8_t*>(field) + 4)));
}
std::uint32_t __cdecl xferList(void*, void* field) { return take("ls:" + rel(field)); }
std::uint32_t __fastcall allocBlock(void*, void*, std::uint32_t) { return 0; }
std::uint32_t __cdecl allocBytes(std::uint32_t size) {
    note("al:" + fmt(size));
    return static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(g_arena.alloc(size)));
}
std::uint32_t __fastcall ctorList(void* self, void*) {
    note("ct");
    storeU32(self, g_listVt);
    return static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(self));
}
std::uint32_t __fastcall pushItem(void*, void*, std::uint32_t value) { return take("pu:" + fmt(value)); }
std::uint32_t __fastcall setList(void*, void*, void*) { return take("sl"); }
std::uint32_t __cdecl freePtr(void*) { return take("fr"); }
std::uint32_t __fastcall weaponPair(void*, void*, std::uint32_t a, std::uint32_t b) {
    note("pair:" + fmt(a) + ":" + fmt(b));
    return 0;
}
std::uint32_t __fastcall testPair(void*, void*, void*, void*) { return take("tp"); }
std::uint32_t __fastcall floatAdjust(void*, void*, std::uint32_t) { return take("adj"); }
std::uint32_t __fastcall laserApply(void*, void*, std::uint32_t, void*, void*) { return take("lap"); }
std::uint32_t __fastcall stringCtor(void* self, void*) {
    note("str");
    storeU32(self, 0);
    storeU32(static_cast<std::uint8_t*>(self) + 4, 0);
    return static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(self));
}
std::uint32_t __fastcall clearList(void*, void*) { return take("cl"); }
std::uint32_t __fastcall beginRecords(void*, void*) { return take("br"); }
std::uint32_t __fastcall applyRecord(void*, void*, std::uint32_t id) { return take("ar:" + fmt(id)); }
std::uint32_t __fastcall trigger(void*, void*, std::uint32_t flag) { return take("tg:" + fmt(flag)); }
std::uint32_t __fastcall allocNode(void*, void*, std::uint32_t, std::uint32_t) {
    note("an");
    return g_node;
}
std::uint32_t __fastcall linkNode(void*, void*) { return take("ln"); }
std::uint32_t __cdecl postList(void*, std::uint32_t n, void*) { return take("po:" + fmt(n)); }
std::uint32_t __fastcall wipeList(void*, void*) { return take("wp"); }
std::uint32_t __fastcall disguiseFlag(void*, void*, std::uint32_t flag) { return take("df:" + fmt(flag & 0xFFu)); }
std::uint32_t __fastcall moveVec(void*, void*, std::uint32_t a, std::uint32_t b, std::uint32_t bits,
                                 std::uint32_t mode) {
    return take("mv:" + fmt(a) + ":" + fmt(b) + ":" + hex8(bits) + ":" + fmt(mode));
}
std::uint32_t __cdecl memMove(void*, void*, std::uint32_t n) { return take("mm:" + fmt(n)); }
std::uint32_t __fastcall scanPulse(void*, void*, void*, std::uint32_t bits, std::uint32_t mask) {
    return take("sp:" + hex8(bits) + ":" + fmt(mask));
}
std::uint32_t __fastcall resize16(void* self, void*, std::uint32_t n) {
    return take("rz:" + rel(self) + ":" + fmt(n));
}
std::uint32_t __fastcall projectPassengers(void*, void*, void*, void* transformed, void*, std::uint32_t extra,
                                           std::uint32_t) {
    return take("pj:" + floats3(transformed) + ":" + fmt(extra));
}
std::uint32_t __fastcall touchPassenger(void*, void*, std::uint32_t flag) { return take("th:" + fmt(flag)); }
std::uint32_t __fastcall markPassenger(void*, void*) { return take("mp"); }

using Slot = std::uint32_t;

void plant(Slot* table, int offset, void* fn) {
    table[offset / 4] = static_cast<Slot>(reinterpret_cast<std::uintptr_t>(fn));
}

struct World {
    std::uint8_t* self = nullptr;
    std::uint8_t* obj = nullptr;
    std::uint8_t* loco = nullptr;
    std::uint8_t* xfer = nullptr;
    std::uint8_t* pose = nullptr;
    std::uint8_t* point = nullptr;
    std::uint8_t* path = nullptr;
    std::uint8_t* pts = nullptr;
    std::uint8_t* weapon = nullptr;
    std::uint8_t* set = nullptr;
    std::uint8_t* draw = nullptr;
    std::uint8_t* mounted = nullptr;
    std::uint8_t* out = nullptr;
    std::uint8_t* hit = nullptr;
    std::uint8_t* held = nullptr;
    std::uint8_t* chain = nullptr;
    std::uint8_t* handler = nullptr;
    std::uint8_t* node = nullptr;
    std::uint8_t* map = nullptr;
    std::uint8_t* passenger = nullptr;
    std::uint8_t* face = nullptr;
    std::uint8_t* templ = nullptr;
    std::uint8_t* matrix = nullptr;
    std::uint8_t* vec = nullptr;
    std::uint8_t* rec = nullptr;
    std::uint8_t* named = nullptr;
    std::uint8_t* player = nullptr;
    std::uint8_t* info = nullptr;
    std::uint8_t* sub = nullptr;
    std::uint8_t* items = nullptr;
    Slot* locoVt = nullptr;
    Slot* xferVt = nullptr;
    Slot* listVt = nullptr;
    Slot* faceVt = nullptr;
    Slot* weaponVt = nullptr;
    Slot* chainVt = nullptr;
    Slot* handVt = nullptr;
};

World g_w;

void resetState() {
    g_log.clear();
    g_seq = 0;
    g_usable = 1;
    g_ready = 1;
    g_query = 0;
    g_cur = 0;
    g_near = 1;
    g_module = 0;
    g_chain = 0;
    g_count = 0;
    g_whit = 0;
    g_whit2 = 0;
    g_verOut = -1;
    g_v70arm = 0;
    g_poke = false;
    g_findHit = false;
    g_pred = 0x110;
    g_mapHit = false;
    g_mapValue = 0;
    g_bases.clear();
    g_arena = Arena{};
    g_env = {};
    ra3::gamelogic::setObject72BEnv(&g_env);
}

void buildWorld() {
    g_w = {};
    g_w.self = g_arena.alloc(0x800);
    g_w.obj = g_arena.alloc(0x500);
    g_w.loco = g_arena.alloc(0x80);
    g_w.xfer = g_arena.alloc(0x20);
    g_w.pose = g_arena.alloc(0x40);
    g_w.point = g_arena.alloc(0x10);
    g_w.path = g_arena.alloc(0x10);
    g_w.pts = g_arena.alloc(0x30);
    g_w.weapon = g_arena.alloc(0x40);
    g_w.set = g_arena.alloc(0x40);
    g_w.draw = g_arena.alloc(0x80);
    g_w.mounted = g_arena.alloc(0x200);
    g_w.out = g_arena.alloc(0x10);
    g_w.hit = g_arena.alloc(0x40);
    g_w.held = g_arena.alloc(0x100);
    g_w.chain = g_arena.alloc(0x10);
    g_w.handler = g_arena.alloc(0x10);
    g_w.node = g_arena.alloc(0x20);
    g_w.map = g_arena.alloc(0x200);
    g_w.passenger = g_arena.alloc(0x400);
    g_w.face = g_arena.alloc(0x40);
    g_w.templ = g_arena.alloc(0x140);
    g_w.matrix = g_arena.alloc(0x40);
    g_w.vec = g_arena.alloc(0x10);
    g_w.rec = g_arena.alloc(0x20);
    g_w.named = g_arena.alloc(0x20);
    g_w.player = g_arena.alloc(0x40);
    g_w.info = g_arena.alloc(0x40);
    g_w.sub = g_arena.alloc(0x10);
    g_w.items = g_arena.alloc(0x10);
    g_w.locoVt = reinterpret_cast<Slot*>(g_arena.alloc(0x100));
    g_w.xferVt = reinterpret_cast<Slot*>(g_arena.alloc(0x100));
    g_w.listVt = reinterpret_cast<Slot*>(g_arena.alloc(0x20));
    g_w.faceVt = reinterpret_cast<Slot*>(g_arena.alloc(0x140));
    g_w.weaponVt = reinterpret_cast<Slot*>(g_arena.alloc(0x40));
    g_w.chainVt = reinterpret_cast<Slot*>(g_arena.alloc(0x90));
    g_w.handVt = reinterpret_cast<Slot*>(g_arena.alloc(0x90));
    plant(g_w.locoVt, 0x1C, reinterpret_cast<void*>(&v1c));
    plant(g_w.locoVt, 0x34, reinterpret_cast<void*>(&v0));
    plant(g_w.locoVt, 0x38, reinterpret_cast<void*>(&v38));
    plant(g_w.xferVt, 0x04, reinterpret_cast<void*>(&v04));
    plant(g_w.xferVt, 0x08, reinterpret_cast<void*>(&v08));
    plant(g_w.xferVt, 0x0C, reinterpret_cast<void*>(&v0c));
    plant(g_w.xferVt, 0x14, reinterpret_cast<void*>(&v14));
    plant(g_w.xferVt, 0x18, reinterpret_cast<void*>(&v18));
    plant(g_w.xferVt, 0x20, reinterpret_cast<void*>(&v20));
    plant(g_w.xferVt, 0x58, reinterpret_cast<void*>(&v58));
    plant(g_w.xferVt, 0x64, reinterpret_cast<void*>(&v64));
    plant(g_w.xferVt, 0x6C, reinterpret_cast<void*>(&v6c));
    plant(g_w.xferVt, 0x70, reinterpret_cast<void*>(&v70));
    plant(g_w.xferVt, 0x7C, reinterpret_cast<void*>(&v7c));
    plant(g_w.xferVt, 0x84, reinterpret_cast<void*>(&v84));
    plant(g_w.listVt, 0x00, reinterpret_cast<void*>(&vDel));
    plant(g_w.faceVt, 0x10C, reinterpret_cast<void*>(&vCount));
    plant(g_w.faceVt, 0x128, reinterpret_cast<void*>(&vPass));
    plant(g_w.weaponVt, 0x20, reinterpret_cast<void*>(&vW20));
    plant(g_w.weaponVt, 0x24, reinterpret_cast<void*>(&vW24));
    plant(g_w.chainVt, 0x84, reinterpret_cast<void*>(&vChain));
    plant(g_w.handVt, 0x84, reinterpret_cast<void*>(&vHand));
    storeU32(g_w.loco, reinterpret_cast<std::uintptr_t>(g_w.locoVt));
    storeU32(g_w.xfer, reinterpret_cast<std::uintptr_t>(g_w.xferVt));
    storeU32(g_w.weapon, reinterpret_cast<std::uintptr_t>(g_w.weaponVt));
    storeU32(g_w.chain, reinterpret_cast<std::uintptr_t>(g_w.chainVt));
    storeU32(g_w.handler, reinterpret_cast<std::uintptr_t>(g_w.handVt));
    storeU32(g_w.weapon + 0xC, reinterpret_cast<std::uintptr_t>(g_w.sub));
    storeU32(g_w.self + 8, reinterpret_cast<std::uintptr_t>(g_w.obj));
    storeU32(g_w.self + 0x3C, reinterpret_cast<std::uintptr_t>(g_w.loco));
    storeU32(g_w.self + 4, reinterpret_cast<std::uintptr_t>(g_w.templ));
    g_endNode = static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(g_w.map + 0x190));
    g_hitNode = static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(g_w.map + 0x1A0));
    g_node = static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(g_w.node));
    storeU32(g_w.map + 0xC4 + 4, reinterpret_cast<std::uintptr_t>(g_w.map + 0x180));
    storeU32(g_w.map + 0xC4 + 8, 0);
    storeU32(g_w.map + 0x180, g_endNode);
    g_listVt = static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(g_w.listVt));
    watch('s', g_w.self, 0x800);
    watch('o', g_w.obj, 0x500);
    watch('x', g_w.xfer, 0x20);
    watch('p', g_w.point, 0x10);
    watch('a', g_w.pts, 0x30);
    watch('u', g_w.out, 0x10);
    watch('m', g_w.mounted, 0x200);
    watch('r', g_w.rec, 0x20);
    watch('h', g_w.hit, 0x40);
    watch('d', g_w.draw, 0x80);
    watch('n', g_w.node, 0x20);
    watch('e', g_w.held, 0x100);
    watch('q', g_w.pose, 0x40);
    watch('w', g_w.weapon, 0x40);
    watch('b', g_w.passenger, 0x400);
}

void plantMap(std::uint32_t value) {
    g_mapHit = true;
    g_mapValue = value;
    storeU32(g_w.map + 0x1A0 + 4, value);
    g_env.mapRoot = static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(g_w.map));
}

void onePoint(std::uint32_t x, std::uint32_t y, std::uint32_t z) {
    storeU32(g_w.pts, x);
    storeU32(g_w.pts + 4, y);
    storeU32(g_w.pts + 8, z);
    storeU32(g_w.path, reinterpret_cast<std::uintptr_t>(g_w.pts));
    storeU32(g_w.path + 4, reinterpret_cast<std::uintptr_t>(g_w.pts + 12));
}

void emptyPath() {
    storeU32(g_w.path, 0);
    storeU32(g_w.path + 4, 0);
}

std::string dword(const char* name, const void* address) {
    return std::string(name) + "=" + hex8(loadU32(address));
}

std::string snapshot(std::uint32_t eax) {
    const bool collapsed = loadU32(g_w.self + 0xC8) == loadU32(g_w.self + 0xCC);
    const bool head = loadU32(g_w.self + 0xD8) ==
                      static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(g_w.self + 0xD8));
    std::ostringstream text;
    text << "eax=" << fmt(eax) << " log=" << g_log << " mem=" << dword("s50", g_w.self + 0x50) << ","
         << dword("s180", g_w.self + 0x180) << "," << dword("s190", g_w.self + 0x190) << ","
         << dword("s1f0", g_w.self + 0x1F0) << "," << dword("s308", g_w.self + 0x308) << ","
         << dword("s4c", g_w.self + 0x4C) << "," << dword("sbc", g_w.self + 0xBC) << ","
         << dword("s56c", g_w.self + 0x56C) << "," << dword("s110", g_w.self + 0x110) << ","
         << dword("s114", g_w.self + 0x114) << "," << dword("s118", g_w.self + 0x118) << ","
         << "b432=" << std::hex << static_cast<int>(g_w.self[0x432]) << ","
         << dword("o490", g_w.obj + 0x490) << "," << dword("w24", g_w.weapon + 0x24) << ","
         << dword("h24", g_w.hit + 0x24) << "," << dword("pbc", g_w.passenger + 0xBC) << ","
         << dword("m1bc", g_w.mounted + 0x1BC) << "," << dword("u0", g_w.out) << ","
         << dword("u4", g_w.out + 4) << "," << dword("u8", g_w.out + 8) << ","
         << dword("hec", g_w.held + 0xEC) << "," << dword("n8", g_w.node + 8) << ","
         << dword("qc", g_w.pose + 0xC) << ","
         << "cc=" << (collapsed ? "1" : "0") << ",hd=" << (head ? "1" : "0");
    return text.str();
}

std::string tagEax(std::uint32_t eax) {
    const auto raw = static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(g_w.obj));
    const auto out = static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(g_w.out));
    const auto draw = static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(g_w.draw));
    const auto mounted = static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(g_w.mounted));
    if (eax == raw) {
        return "obj";
    }
    if (eax == out) {
        return "out";
    }
    if (eax == draw) {
        return "draw";
    }
    if (eax == mounted) {
        return "mnt";
    }
    if ((eax & 0xFFFFFF00u) == (mounted & 0xFFFFFF00u) && (eax & 0xFFu) == (g_ready & 0xFFu)) {
        return "mix" + fmt(eax & 0xFFu);
    }
    return fmt(eax);
}

void emit(std::uint32_t eax, const char* stk) {
    std::string line = snapshot(eax);
    const auto at = line.find("eax=");
    const auto end = line.find(' ', at);
    line.replace(at, end - at, "eax=" + tagEax(eax));
    std::cout << line << " stk=" << stk << "\n";
}

void bindCalls() {
    ra3::gamelogic::Object72BCalls calls{};
    calls.usable = usable;
    calls.nearCheck = nearCheck;
    calls.steer = steer;
    calls.place = place;
    calls.followCf = followCf;
    calls.followCe = followCe;
    calls.followCd = followCd;
    calls.bindPath = bindPath;
    calls.findWeapon = findWeapon;
    calls.findWeaponAlt = findWeaponAlt;
    calls.markObject = markObject;
    calls.nameKey = nameKey;
    calls.findModule = findModule;
    calls.pokeModule = pokeModule;
    calls.disguiseSlot = disguiseSlot;
    calls.visitHeld = visitHeld;
    calls.queryLoco = queryLoco;
    calls.currentTarget = currentTarget;
    calls.notePose = notePose;
    calls.scriptCall = scriptCall;
    calls.encloseTest = encloseTest;
    calls.applyKey = applyKey;
    calls.xferBase = xferBase;
    calls.lockMap = lockMap;
    calls.findMap = findMap;
    calls.unlockMap = unlockMap;
    calls.floatCall = floatCall;
    calls.ready = ready;
    calls.xferAbility = xferAbility;
    calls.xferRoot = xferRoot;
    calls.xferAt10 = xferAt10Real;
    calls.xferStealth = xferStealth;
    calls.xferAi = xferAi;
    calls.xferMissile = xferMissile;
    calls.xferField = xferField;
    calls.xferFieldB = xferFieldB;
    calls.xferPair = xferPair;
    calls.xferPairB = xferPairB;
    calls.xferExtra = xferExtra;
    calls.xferString = xferString;
    calls.xferList = xferList;
    calls.allocBlock = allocBlock;
    calls.allocBytes = allocBytes;
    calls.ctorList = ctorList;
    calls.pushItem = pushItem;
    calls.setList = setList;
    calls.freePtr = freePtr;
    calls.weaponPair = weaponPair;
    calls.testPair = testPair;
    calls.floatAdjust = floatAdjust;
    calls.laserApply = laserApply;
    calls.stringCtor = stringCtor;
    calls.clearList = clearList;
    calls.beginRecords = beginRecords;
    calls.applyRecord = applyRecord;
    calls.trigger = trigger;
    calls.allocNode = allocNode;
    calls.linkNode = linkNode;
    calls.postList = postList;
    calls.wipeList = wipeList;
    calls.disguiseFlag = disguiseFlag;
    calls.moveVec = moveVec;
    calls.memMove = memMove;
    calls.scanPulse = scanPulse;
    calls.resize16 = resize16;
    calls.projectPassengers = projectPassengers;
    calls.touchPassenger = touchPassenger;
    calls.markPassenger = markPassenger;
    ra3::gamelogic::setObject72BCalls(calls);
}

void prepare() {
    resetState();
    buildWorld();
    bindCalls();
    storeU32(g_w.obj + 0xDC, 0x61);
}

void putF(std::uint8_t* address, std::uint32_t bits) { storeU32(address, bits); }

bool handled(const std::string& cmd, std::uint32_t& eax);

std::uint32_t run(const std::string& cmd) {
    using namespace ra3::gamelogic;
    if (cmd == "snapFar") {
        g_near = 1;
        putF(g_w.obj + 0x38, 0);
        putF(g_w.point, 0x43480000u);
        storeU32(g_w.self + 0x214, reinterpret_cast<std::uintptr_t>(g_w.pose));
        return AIUpdateInterface_snapNear(g_w.self, g_w.point, 0, 0);
    }
    if (cmd == "snapNear") {
        putF(g_w.obj + 0x38, 0);
        putF(g_w.point, 0x3F800000u);
        return AIUpdateInterface_snapNear(g_w.self, g_w.point, 1, 2);
    }
    if (cmd == "snapOff") {
        g_near = 0;
        putF(g_w.point, 0x3F800000u);
        return AIUpdateInterface_snapNear(g_w.self, g_w.point, 0, 3);
    }
    if (cmd == "flagBad") {
        g_usable = 0x100;
        emptyPath();
        return AIUpdateInterface_followPathFlag(g_w.self, g_w.path, g_w.obj, 0, 0);
    }
    if (cmd == "flagEmpty") {
        emptyPath();
        return AIUpdateInterface_followPathFlag(g_w.self, g_w.path, g_w.obj, 1, 0);
    }
    if (cmd == "flagGo") {
        onePoint(0x3F800000u, 0x40000000u, 0x40400000u);
        return AIUpdateInterface_followPathFlag(g_w.self, g_w.path, g_w.obj, 0, 0);
    }
    if (cmd == "flagBit") {
        onePoint(0x3F800000u, 0, 0);
        return AIUpdateInterface_followPathFlag(g_w.self, g_w.path, nullptr, 2, 1);
    }
    if (cmd == "p34Empty") {
        emptyPath();
        return AIUpdateInterface_followPath34(g_w.self, g_w.path, g_w.obj, 0x3F800000u, 0);
    }
    if (cmd == "p34Go") {
        onePoint(0x3F800000u, 0x40000000u, 0x40400000u);
        return AIUpdateInterface_followPath34(g_w.self, g_w.path, g_w.obj, 0x3F800000u, 0);
    }
    if (cmd == "p3bGo") {
        onePoint(0x3F800000u, 0, 0);
        return AIUpdateInterface_followPath3B(g_w.self, g_w.path, g_w.obj, 0x40000000u, 2);
    }
    if (cmd == "p34Bad") {
        g_usable = 0x100;
        return AIUpdateInterface_followPath34(g_w.self, g_w.path, g_w.obj, 0, 0);
    }
    if (cmd == "ord0") {
        return AIUpdateInterface_orderWeapon0A(g_w.self, 0, 0x11, 0);
    }
    if (cmd == "ordBit") {
        g_w.obj[0x471] = 8;
        return AIUpdateInterface_orderWeapon0A(g_w.self, 0x21, 0x11, 0);
    }
    if (cmd == "ordDraw") {
        storeU32(g_w.obj + 0x374, reinterpret_cast<std::uintptr_t>(g_w.draw));
        storeU32(g_w.draw + 0x40, 1);
        storeU32(g_w.obj + 0x37C, reinterpret_cast<std::uintptr_t>(g_w.set));
        g_findHit = true;
        g_whit = static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(g_w.weapon));
        return AIUpdateInterface_orderWeapon0A(g_w.self, 0x21, 0x11, 0);
    }
    if (cmd == "ordGo" || cmd == "ordTwice" || cmd == "ordMode2") {
        storeU32(g_w.obj + 0x37C, reinterpret_cast<std::uintptr_t>(g_w.set));
        g_findHit = true;
        g_whit = static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(g_w.weapon));
        const std::uint32_t mode = cmd == "ordMode2" ? 2u : 0u;
        if (cmd == "ordMode2") {
            storeU32(g_w.self + 0x508, 0);
        }
        auto eax = AIUpdateInterface_orderWeapon0A(g_w.self, 0x21, 0x11, mode);
        if (cmd == "ordTwice") {
            eax = AIUpdateInterface_orderWeapon0A(g_w.self, 0x21, 0x12, 0);
        }
        return eax;
    }
    if (cmd == "ordMod") {
        g_module = 0x30;
        return AIUpdateInterface_orderWeapon0A(g_w.self, 0x21, 0x11, 1);
    }
    if (cmd == "bld0") {
        return AIUpdateInterface_buildList16(g_w.self, 0, 0, 0, 0, 0x11, 2);
    }
    if (cmd == "bldQ") {
        g_query = 0x70;
        return AIUpdateInterface_buildList16(g_w.self, 0, 0, 0, 0, 0x11, 0);
    }
    if (cmd == "bldItem") {
        storeU32(g_w.items, 0x55);
        return AIUpdateInterface_buildList16(g_w.self,
                                              static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(g_w.items)),
                                              static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(g_w.items + 4)),
                                              0, 0, 0x11, 2);
    }
    if (cmd == "bldWep") {
        storeU32(g_w.obj + 0x37C, reinterpret_cast<std::uintptr_t>(g_w.set));
        storeU32(g_w.set + 0x24, reinterpret_cast<std::uintptr_t>(g_w.items));
        storeU32(g_w.set + 0x28, reinterpret_cast<std::uintptr_t>(g_w.items + 4));
        storeU32(g_w.items, reinterpret_cast<std::uintptr_t>(g_w.weapon));
        g_whit = 0;
        return AIUpdateInterface_buildList16(g_w.self, 0, 0, 0, 0, 0x11, 2);
    }
    if (cmd == "bldHit") {
        storeU32(g_w.obj + 0x37C, reinterpret_cast<std::uintptr_t>(g_w.set));
        storeU32(g_w.set + 0x24, reinterpret_cast<std::uintptr_t>(g_w.items));
        storeU32(g_w.set + 0x28, reinterpret_cast<std::uintptr_t>(g_w.items + 4));
        storeU32(g_w.items, reinterpret_cast<std::uintptr_t>(g_w.weapon));
        g_whit = static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(g_w.hit));
        return AIUpdateInterface_buildList16(g_w.self, 0, 0, 0, 0, 0x11, 2);
    }
    if (cmd == "stByte") {
        g_w.self[0x436] = 1;
        return AIUpdateInterface_steerFloat21(g_w.self, g_w.point, 0x40000000u, 0, 0);
    }
    if (cmd == "stBad") {
        g_usable = 0x100;
        return AIUpdateInterface_steerFloat21(g_w.self, g_w.point, 0x40000000u, 0, 0);
    }
    if (cmd == "stGo") {
        putF(g_w.point, 0x3F800000u);
        return AIUpdateInterface_steerFloat21(g_w.self, g_w.point, 0x40000000u, 0x11, 0);
    }
    if (cmd == "stScript") {
        storeU32(g_w.obj + 0x418, 0x42);
        return AIUpdateInterface_steerFloat21(g_w.self, g_w.point, 0x40000000u, 0, 2);
    }
    if (cmd == "pl0") {
        return AIUpdateInterface_placeTarget21(g_w.self, nullptr, 0);
    }
    if (cmd == "plBad") {
        g_usable = 0x100;
        return AIUpdateInterface_placeTarget21(g_w.self, g_w.obj, 0);
    }
    if (cmd == "plGo") {
        putF(g_w.obj + 0x38, 0x3F800000u);
        storeU32(g_w.self + 0x214, reinterpret_cast<std::uintptr_t>(g_w.pose));
        return AIUpdateInterface_placeTarget21(g_w.self, g_w.obj, 0);
    }
    if (cmd == "plScr") {
        putF(g_w.obj + 0x38, 0);
        storeU32(g_w.obj + 0x418, 0x42);
        return AIUpdateInterface_placeTarget21(g_w.self, g_w.obj, 2);
    }
    if (cmd == "at0") {
        g_cur = 0;
        return AIUpdateInterface_attackOrState26(g_w.self, 0, 0);
    }
    if (cmd == "atCur") {
        g_cur = 5;
        return AIUpdateInterface_attackOrState26(g_w.self, 0, 0);
    }
    if (cmd == "atChain") {
        storeU32(g_w.obj + 0x328, reinterpret_cast<std::uintptr_t>(g_w.chain));
        g_chain = static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(g_w.handler));
        return AIUpdateInterface_attackOrState26(g_w.self, 0x21, 3);
    }
    if (cmd == "atMod") {
        g_module = 0x30;
        return AIUpdateInterface_attackOrState26(g_w.self, 0x21, 1);
    }
    if (cmd == "en0") {
        g_pred = 0x200;
        auto* face = g_w.face + 0x20;
        return EncloseBehavior_tryApplyKey(face, 0x12);
    }
    if (cmd == "enByte") {
        auto* face = g_w.face + 0x20;
        face[4] = 0;
        return EncloseBehavior_tryApplyKey(face, 0x12);
    }
    if (cmd == "enOpen") {
        auto* face = g_w.face + 0x20;
        face[4] = 1;
        storeU32(face - 0xC, reinterpret_cast<std::uintptr_t>(g_w.obj));
        return EncloseBehavior_tryApplyKey(face, 0x12);
    }
    if (cmd == "enKey") {
        auto* face = g_w.face + 0x20;
        face[4] = 1;
        storeU32(face - 8, reinterpret_cast<std::uintptr_t>(g_w.held));
        storeU32(face - 0xC, reinterpret_cast<std::uintptr_t>(g_w.obj));
        storeU32(g_w.obj + 8, reinterpret_cast<std::uintptr_t>(g_w.info));
        storeU32(g_w.info, 7);
        return EncloseBehavior_tryApplyKey(face, 0x12);
    }
    if (cmd == "rel0") {
        return CaptureSpecialAbilityUpdate_releaseHeld(g_w.self, 1, 2);
    }
    if (cmd == "relHit") {
        storeU32(g_w.self + 0x4C, 0x21);
        storeU32(g_w.held + 0xEC, 0x99);
        plantMap(static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(g_w.held)));
        return CaptureSpecialAbilityUpdate_releaseHeld(g_w.self, 1, 2);
    }
    if (cmd == "bd0") {
        g_ready = 0;
        return SpecialDisguiseUpdate_blendScale(g_w.self);
    }
    if (cmd == "bdA" || cmd == "bd2" || cmd == "bdBig") {
        g_ready = 1;
        storeU32(g_w.self + 0x30, cmd == "bd2" ? 1u : 2u);
        storeU32(g_w.self + 0x38, cmd == "bd2" ? 2u : 0u);
        putF(g_w.templ + 0x20, cmd == "bdBig" ? 0x43960000u : 0x40800000u);
        putF(g_w.templ + 0x24, 0x40800000u);
        putF(g_w.templ + 0xF0, 0x3E800000u);
        if (cmd == "bdBig") {
            storeU32(g_w.self + 0x30, 2);
        }
        return SpecialDisguiseUpdate_blendScale(g_w.self);
    }
    if (cmd == "mb0") {
        g_ready = 0;
        return ToggleMountedSpecialAbilityUpdate_blendMounted(g_w.self);
    }
    if (cmd == "mbNull" || cmd == "mbOn") {
        g_ready = 1;
        storeU32(g_w.self + 0x30, 2);
        putF(g_w.templ + 0x20, 0x40800000u);
        putF(g_w.templ + 0xF0, 0x3E800000u);
        if (cmd == "mbOn") {
            storeU32(g_w.obj + 0xF4, reinterpret_cast<std::uintptr_t>(g_w.mounted));
        }
        return ToggleMountedSpecialAbilityUpdate_blendMounted(g_w.self);
    }
    if (cmd == "hoEmpty") {
        storeU32(g_w.obj + 0x32C, 1);
        storeU32(g_w.obj + 0x330, 1);
        return HordeAIUpdate_forwardMove(g_w.self, 1, 2, 0x3F800000u, 3);
    }
    if (cmd == "hoZero") {
        storeU32(g_w.obj + 0x328, reinterpret_cast<std::uintptr_t>(g_w.chain));
        g_chain = 0;
        return HordeAIUpdate_forwardMove(g_w.self, 1, 2, 0x3F800000u, 3);
    }
    if (cmd == "hoGo") {
        storeU32(g_w.obj + 0x328, reinterpret_cast<std::uintptr_t>(g_w.chain));
        g_chain = 1;
        return HordeAIUpdate_forwardMove(g_w.self, 1, 2, 0x3F800000u, 3);
    }
    if (cmd == "rec0") {
        return CompositeStructureInfoModule_applyRecords(g_w.self);
    }
    if (cmd == "recZ") {
        storeU32(g_w.self + 0x18, reinterpret_cast<std::uintptr_t>(g_w.items));
        storeU32(g_w.self + 0x1C, reinterpret_cast<std::uintptr_t>(g_w.items + 4));
        storeU32(g_w.items, 0);
        return CompositeStructureInfoModule_applyRecords(g_w.self);
    }
    if (cmd == "recHit") {
        storeU32(g_w.self + 0x18, reinterpret_cast<std::uintptr_t>(g_w.items));
        storeU32(g_w.self + 0x1C, reinterpret_cast<std::uintptr_t>(g_w.items + 4));
        storeU32(g_w.items, 0x21);
        plantMap(0x44);
        return CompositeStructureInfoModule_applyRecords(g_w.self);
    }
    if (cmd == "tg0") {
        return CaptureSpecialAbilityUpdate_onTrigger(g_w.self, 0);
    }
    if (cmd == "tg1") {
        storeU32(g_w.obj + 0xF4, 0x77);
        return CaptureSpecialAbilityUpdate_onTrigger(g_w.self, 1);
    }
    return 0;
}

bool handled(const std::string& cmd, std::uint32_t& eax) {
    static const char* names[] = {
        "snapFar", "snapNear", "snapOff", "flagBad", "flagEmpty", "flagGo", "flagBit", "p34Empty", "p34Go",
        "p3bGo",     "p34Bad",  "ord0",    "ordBit",  "ordDraw",   "ordGo",   "ordTwice", "ordMode2", "ordMod",
        "bld0",     "bldQ",    "bldItem", "bldWep",  "bldHit",    "stByte",  "stBad",    "stGo",     "stScript",
        "pl0",      "plBad",   "plGo",    "plScr",   "at0",       "atCur",   "atChain",  "atMod",    "en0",
        "enByte",   "enOpen",  "enKey",   "rel0",    "relHit",    "bd0",     "bdA",      "bd2",      "bdBig",
        "mb0",      "mbNull",  "mbOn",    "hoEmpty", "hoZero",    "hoGo",    "rec0",     "recZ",     "recHit",
        "tg0",      "tg1"};
    for (const char* name : names) {
        if (cmd == name) {
            eax = run(cmd);
            return true;
        }
    }
    return false;
}

using XferFn = std::uint32_t (*)(std::uint8_t*, std::uint8_t*);

std::uint32_t runXfer(const std::string& cmd) {
    using namespace ra3::gamelogic;
    struct Row {
        const char* name;
        XferFn fn;
        std::uint32_t flags;
        int ver;
    };
    const Row rows[] = {
        {"notify", NotifyWhenAttackingBehavior_xfer, 0, -1},
        {"passive", PassiveAreaEffectBehavior_xfer, 0, -1},
        {"manager", ManagerBody_xfer, 0, -1},
        {"crate", CrateCollide_xfer, 0, -1},
        {"squish", SquishCollide_xfer, 0, -1},
        {"squish2", SquishCollide_xfer, 0, 2},
        {"squish1", SquishCollide_xfer, 0, 1},
        {"circ", CircularDamageUpdate_xfer, 0, -1},
        {"circ1", CircularDamageUpdate_xfer, 0, 1},
        {"invalid", InvalidTargetsDeathUpdate_xfer, 0, -1},
        {"linear", LinearDamageUpdate_xfer, 0, -1},
        {"miss0", MissileUpdate_xfer, 0, -1},
        {"missZ", MissileUpdate_xfer, 2, -1},
        {"beam", BeamSpecialAbilityUpdate_xfer, 0, -1},
        {"clone", CloneStoredObjectsSpecialPowerUpdate_xfer, 0, -1},
        {"eject", EjectPassengersSpecialAbilityUpdate_xfer, 0, -1},
        {"dis0", SpecialDisguiseUpdate_xfer, 0, -1},
        {"dis1", SpecialDisguiseUpdate_xfer, 2, -1},
        {"hidden", ToggleHiddenSpecialAbilityUpdate_xfer, 0, -1},
        {"mountx", ToggleMountedSpecialAbilityUpdate_xfer, 0, -1},
        {"status", ToggleStatusSpecialAbilityUpdate_xfer, 0, -1},
        {"fire2", WeaponFireSpecialAbilityUpdate_xfer, 4, -1},
        {"fire0", WeaponFireSpecialAbilityUpdate_xfer, 0, -1},
        {"fireP", WeaponFireSpecialAbilityUpdate_xfer, 0, -1},
        {"fireF", WeaponFireSpecialAbilityUpdate_xfer, 1, -1},
        {"animal", AnimalAIUpdate_xfer, 0, -1},
        {"assE", AssaultTransportAIUpdate_xfer, 8, -1},
        {"ass0", AssaultTransportAIUpdate_xfer, 0, -1},
        {"ass2", AssaultTransportAIUpdate_xfer, 0, -1},
        {"spE", SpawnBehavior_xfer, 8, -1},
        {"sp0", SpawnBehavior_xfer, 0, -1},
        {"sp1", SpawnBehavior_xfer, 2, -1},
        {"sc0", WeaponScatterEffectUpdate_xfer, 0, -1},
        {"sc1", WeaponScatterEffectUpdate_xfer, 0, -1},
        {"scR", WeaponScatterEffectUpdate_xfer, 2, -1},
    };
    for (const Row& row : rows) {
        if (cmd != row.name) {
            continue;
        }
        g_verOut = row.ver;
        storeU32(g_w.xfer + 4, row.flags);
        if (cmd == "miss0" || cmd == "missZ") {
            storeU32(g_w.self + 0x110, 0x11111111u);
            storeU32(g_w.self + 0x114, 0x22222222u);
            storeU32(g_w.self + 0x118, 0x33333333u);
        }
        if (cmd == "dis1") {
            storeU32(g_w.obj + 0xC8, 0x200);
            storeU32(g_w.self + 0xC0, 1);
            storeU32(g_w.self + 0xD0, 1);
            storeU32(g_w.obj + 0x418, 5);
            storeU32(g_w.player + 0x2C, 5);
            g_env.player = static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(g_w.player));
        }
        if (cmd == "fireP") {
            g_poke = true;
        }
        if (cmd == "fireF") {
            storeU32(g_w.self + 0xBC, 0x42);
        }
        if (cmd == "ass2") {
            storeU32(g_w.self + 0x574, 2);
            g_v70arm = 2;
        }
        if (cmd == "sp0" || cmd == "sp1") {
            storeU32(g_w.self + 0x40, reinterpret_cast<std::uintptr_t>(g_w.named));
            storeU32(g_w.named + 4, 0x10);
            storeU32(g_w.named + 8, cmd == "sp1" ? 0u : 0x20u);
        }
        if (cmd == "sc0" || cmd == "sc1" || cmd == "scR") {
            storeU32(g_w.self + 0x3C, 0);
            storeU32(g_w.self + 0x40, 0);
        }
        if (cmd == "sc1") {
            storeU32(g_w.self + 0x2C, reinterpret_cast<std::uintptr_t>(g_w.rec));
            storeU32(g_w.self + 0x30, reinterpret_cast<std::uintptr_t>(g_w.rec + 16));
        }
        return row.fn(g_w.self, g_w.xfer);
    }
    return 0;
}

const char* stackOf(const std::string& cmd) {
    if (cmd.rfind("snap", 0) == 0 || cmd.rfind("ord", 0) == 0) {
        return "c";
    }
    if (cmd == "stByte" || cmd == "stBad" || cmd == "stGo" || cmd == "stScript" || cmd.rfind("flag", 0) == 0 ||
        cmd.rfind("p34", 0) == 0 || cmd.rfind("p3b", 0) == 0 || cmd.rfind("ho", 0) == 0) {
        return "10";
    }
    if (cmd.rfind("bld", 0) == 0) {
        return "18";
    }
    if (cmd.rfind("pl", 0) == 0 || cmd.rfind("at", 0) == 0 || cmd.rfind("rel", 0) == 0) {
        return "8";
    }
    if (cmd.rfind("en", 0) == 0 || cmd.rfind("tg", 0) == 0) {
        return "4";
    }
    if (cmd.rfind("bd", 0) == 0 || cmd.rfind("mb", 0) == 0 || cmd.rfind("rec", 0) == 0 || cmd.rfind("tr", 0) == 0) {
        return "0";
    }
    if (cmd == "laserM" || cmd == "laserH" || cmd.rfind("sp", 0) == 0 || cmd.rfind("sc", 0) == 0) {
        return "4";
    }
    if (cmd.rfind("rad", 0) == 0) {
        return "8";
    }
    if (cmd.rfind("trn", 0) == 0) {
        return "0";
    }
    return "4";
}

std::uint32_t runRest(const std::string& cmd) {
    using namespace ra3::gamelogic;
    if (cmd == "laserM") {
        putF(g_w.self + 0x3C, 0x100);
        putF(g_w.self + 0x40, 0x200);
        putF(g_w.self + 0x44, 0x300);
        return LaserStateModule_fillOutput(g_w.self, g_w.out);
    }
    if (cmd == "laserH") {
        storeU32(g_w.self + 0x54, 0x21);
        putF(g_w.held + 0x38, 0x11);
        putF(g_w.held + 0x3C, 0x22);
        putF(g_w.held + 0x40, 0x33);
        plantMap(static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(g_w.held)));
        return LaserStateModule_fillOutput(g_w.self, g_w.out);
    }
    if (cmd == "rad0") {
        const auto head = static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(g_w.self + 0xD8));
        storeU32(g_w.self + 0xC8, head);
        storeU32(g_w.self + 0xCC, head);
        storeU32(g_w.self + 0xD8, head);
        storeU32(g_w.self + 0xDC, head);
        return RadarScanMapSpecialAbilityUpdate_clearScan(g_w.self, 1, 2);
    }
    if (cmd == "radZ") {
        storeU32(g_w.items, 0);
        storeU32(g_w.self + 0xC8, reinterpret_cast<std::uintptr_t>(g_w.items));
        storeU32(g_w.self + 0xCC, reinterpret_cast<std::uintptr_t>(g_w.items + 4));
        const auto head = static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(g_w.self + 0xD8));
        storeU32(g_w.self + 0xD8, head);
        storeU32(g_w.self + 0xDC, head);
        return RadarScanMapSpecialAbilityUpdate_clearScan(g_w.self, 1, 2);
    }
    if (cmd == "radHit") {
        storeU32(g_w.items, 0x21);
        storeU32(g_w.self + 0xC8, reinterpret_cast<std::uintptr_t>(g_w.items));
        storeU32(g_w.self + 0xCC, reinterpret_cast<std::uintptr_t>(g_w.items + 4));
        storeU32(g_w.held + 0xF4, 0x15);
        plantMap(static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(g_w.held)));
        storeU32(g_w.node, reinterpret_cast<std::uintptr_t>(g_w.self + 0xD8));
        storeU32(g_w.self + 0xD8, reinterpret_cast<std::uintptr_t>(g_w.node));
        storeU32(g_w.obj + 0x418, reinterpret_cast<std::uintptr_t>(g_w.info));
        storeU32(g_w.info + 0x20, 3);
        g_env.scanGlobal = 1;
        return RadarScanMapSpecialAbilityUpdate_clearScan(g_w.self, 1, 2);
    }
    if (cmd == "trn0") {
        storeU32(g_w.self + 0x24, reinterpret_cast<std::uintptr_t>(g_w.faceVt));
        g_count = 0;
        return TransportContain_refreshPassengers(g_w.self);
    }
    if (cmd == "trn1") {
        storeU32(g_w.self + 0x24, reinterpret_cast<std::uintptr_t>(g_w.faceVt));
        storeU32(g_w.self + 4, reinterpret_cast<std::uintptr_t>(g_w.templ));
        storeU32(g_w.self + 8, reinterpret_cast<std::uintptr_t>(g_w.matrix));
        storeU32(g_w.templ + 0x130, reinterpret_cast<std::uintptr_t>(g_w.vec));
        storeU32(g_w.templ + 0x118, 0x99);
        putF(g_w.vec, 0x3F800000u);
        putF(g_w.matrix + 8, 0x3F800000u);
        putF(g_w.matrix + 0x18, 0x40000000u);
        putF(g_w.matrix + 0x28, 0x40800000u);
        storeU32(g_w.passenger + 0x378, reinterpret_cast<std::uintptr_t>(g_w.draw));
        g_node = static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(g_w.passenger));
        g_count = 1;
        return TransportContain_refreshPassengers(g_w.self);
    }
    return runXfer(cmd);
}

}  // namespace

int main() {
    std::string line;
    while (std::getline(std::cin, line)) {
        if (line == "quit") {
            break;
        }
        if (line.empty()) {
            continue;
        }
        prepare();
        std::uint32_t eax = 0;
        if (!handled(line, eax)) {
            eax = runRest(line);
        }
        emit(eax, stackOf(line));
    }
    return 0;
}
