#include "ra3/gamelogic/ObjectModules6C.h"

#include <cstdint>
#include <cstring>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

namespace {

std::string g_log;
int g_seq = 0;
std::uint32_t g_usable = 1;
std::uint32_t g_pair = 1;
std::uint32_t g_disp = 1;
std::uint32_t g_accept = 1;
std::uint32_t g_delay = 3;
std::uint32_t g_ready = 0;
std::uint32_t g_embed = 1;
std::uint32_t g_arm = 1;
std::uint32_t g_magic = 0x55;
std::uint32_t g_hit = 1;
std::uint32_t g_listCount = 0;
std::uint32_t g_matchIndex = 0xFFFFFFFF;
std::uint32_t g_matchObject = 0;
std::uint32_t g_idCell = 0x42;
std::uint32_t g_rect[5] = {};
std::uint32_t g_repl[3] = {};
std::uint8_t* g_child = nullptr;
std::uint8_t* g_node = nullptr;
std::uint8_t* g_header = nullptr;
std::uint8_t* g_weapon = nullptr;
std::uint8_t* g_weaponAlt = nullptr;
std::uint32_t g_template = 0;
std::uint32_t g_objectBits = 0;
std::uint32_t g_sentinel = 0;
std::uint32_t g_weaponBits = 0;
std::uint32_t g_weaponAltBits = 0;
std::uint32_t g_moduleBits = 0;
std::uint32_t g_clockStore = 0;
std::uint32_t g_registryStore = 0;
std::uint32_t g_worldStore = 0;
std::uint32_t g_catalogStore = 0;
std::string g_ext = ".";

std::uint32_t loadU32(const void* address) {
    std::uint32_t value = 0;
    std::memcpy(&value, address, 4);
    return value;
}

void storeU32(void* address, std::uint32_t value) { std::memcpy(address, &value, 4); }

std::uint32_t bits(const void* pointer) {
    return static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(pointer));
}

void addLog(const std::string& text) {
    if (!g_log.empty()) {
        g_log += ",";
    }
    g_log += text;
}

std::uint32_t nextSeq() {
    ++g_seq;
    return 0xA1000000u + static_cast<std::uint32_t>(g_seq);
}

std::string hex32(std::uint32_t value) {
    std::ostringstream text;
    text << std::hex << std::nouppercase << std::setw(8) << std::setfill('0') << value;
    return text.str();
}

std::string hexList(std::uint32_t value) {
    std::ostringstream text;
    text << std::hex << std::nouppercase << value;
    return text.str();
}

void resetProbe() {
    g_log.clear();
    g_seq = 0;
    g_ext = ".";
    g_template = 0;
    g_objectBits = 0;
    g_sentinel = 0;
    g_weaponBits = 0;
    g_weaponAltBits = 0;
    g_moduleBits = 0;
    g_child = nullptr;
    g_node = nullptr;
    g_header = nullptr;
    g_weapon = nullptr;
    g_weaponAlt = nullptr;
    g_matchObject = 0;
    g_matchIndex = 0xFFFFFFFF;
    g_hit = 0;
    g_listCount = 0;
}

std::uint32_t __fastcall onUsable(void*, void*) {
    nextSeq();
    addLog("use");
    return g_usable;
}

std::uint32_t __fastcall onSteer(void*, void*, std::uint32_t pos, std::uint32_t bits, std::uint32_t mode) {
    const auto ret = nextSeq();
    addLog("steer:" + hexList(pos) + ":" + hexList(bits) + ":" + hexList(mode));
    return ret;
}

std::uint32_t __fastcall onPlace(void*, void*, void* pos, std::uint32_t scale) {
    const auto ret = nextSeq();
    const auto raw = bits(pos);
    if (raw < 0x10000u) {
        addLog("placei:" + hexList(raw) + ":" + hexList(scale));
    } else {
        addLog("placev:" + hexList(loadU32(pos)) + ":" + hexList(loadU32(static_cast<std::uint8_t*>(pos) + 4)) +
               ":" + hexList(loadU32(static_cast<std::uint8_t*>(pos) + 8)) + ":" + hexList(scale));
    }
    return ret;
}

std::uint32_t __fastcall onAim(void*, void*, std::uint32_t target) {
    const auto ret = nextSeq();
    addLog("aim:" + hexList(target));
    return ret;
}

std::uint32_t __fastcall onPairA(void*, void*, std::uint32_t, std::uint32_t first, std::uint32_t second) {
    nextSeq();
    addLog("pairA:" + hexList(first) + ":" + hexList(second));
    return g_pair;
}

std::uint32_t __fastcall onPairB(void*, void*, std::uint32_t, std::uint32_t first, std::uint32_t second) {
    nextSeq();
    addLog("pairB:" + hexList(first) + ":" + hexList(second));
    return g_pair;
}

std::uint32_t __fastcall onApply59(void*, void*, std::uint32_t first, std::uint32_t second) {
    const auto ret = nextSeq();
    addLog("a59:" + hexList(first) + ":" + hexList(second));
    return ret;
}

std::uint32_t __fastcall onApply5A(void*, void*, std::uint32_t first, std::uint32_t second) {
    const auto ret = nextSeq();
    addLog("a5a:" + hexList(first) + ":" + hexList(second));
    return ret;
}

std::uint32_t __fastcall onDispatch(void*, void*, std::uint32_t, void* record, std::uint32_t mode,
                                   std::uint32_t one, std::uint32_t zero) {
    nextSeq();
    auto* bytes = static_cast<std::uint8_t*>(record);
    addLog("disp:" + hexList(loadU32(bytes)) + ":" + hexList(loadU32(bytes + 4)) + ":" + hexList(mode) + ":" +
           hexList(one) + ":" + hexList(zero));
    return g_disp;
}

std::uint32_t __fastcall onState(void*, void*, std::uint32_t code) {
    const auto ret = nextSeq();
    addLog("st:" + hexList(code));
    return ret;
}

std::uint32_t __fastcall onWeapon(void*, void*, std::uint32_t id) {
    nextSeq();
    addLog("wep:" + hexList(id));
    return bits(g_weapon);
}

std::uint32_t __fastcall onWeaponAlt(void*, void*, std::uint32_t found, std::uint32_t id) {
    nextSeq();
    addLog("wep2:" + hexList(found) + ":" + hexList(id));
    return bits(g_weaponAlt);
}

std::uint32_t __fastcall onReadId(void*, void*, void*) {
    nextSeq();
    addLog("rid");
    g_idCell = 0x42;
    return bits(&g_idCell);
}

std::uint32_t __fastcall onLookup(void*, void*, std::uint32_t id) {
    nextSeq();
    addLog("look:" + hexList(id));
    return bits(g_header);
}

std::uint32_t __fastcall onNeg(void*, void*, std::uint32_t key, std::uint32_t flag) {
    const auto ret = nextSeq();
    addLog("neg:" + hexList(key) + ":" + hexList(flag));
    return ret;
}

std::uint32_t __fastcall onOne(void*, void*, std::uint32_t key) {
    const auto ret = nextSeq();
    addLog("one:" + hexList(key));
    return ret;
}

std::uint32_t __fastcall onBase(void*, void*, void*) {
    const auto ret = nextSeq();
    addLog("base");
    return ret;
}

std::uint32_t __fastcall onBit(void*, void*, std::uint32_t code, std::uint32_t flag) {
    const auto ret = nextSeq();
    addLog("bit:" + hexList(code) + ":" + hexList(flag));
    return ret;
}

std::uint32_t __fastcall onAlt(void*, void*, std::uint32_t code, std::uint32_t flag) {
    const auto ret = nextSeq();
    addLog("alt:" + hexList(code) + ":" + hexList(flag));
    return ret;
}

std::uint32_t __fastcall onFind(void* self, void*, void* outB, void*) {
    nextSeq();
    addLog(std::string("find:") + (g_hit ? "1" : "0"));
    if (g_hit && g_node != nullptr) {
        storeU32(outB, bits(g_node));
    } else {
        storeU32(outB, bits(static_cast<std::uint8_t*>(self) + 4));
    }
    return 0;
}

void __cdecl onUnlinkA(void* node) {
    nextSeq();
    addLog("ua:" + hexList(loadU32(static_cast<std::uint8_t*>(node) + 8)));
}
void __cdecl onUnlinkB(void* node, void*) {
    nextSeq();
    addLog("ub:" + hexList(loadU32(static_cast<std::uint8_t*>(node) + 8)));
}
void __cdecl onRelease(void* node) {
    nextSeq();
    addLog("rel:" + hexList(loadU32(static_cast<std::uint8_t*>(node) + 8)));
}

std::uint32_t __fastcall onAlloc(void*, void*, std::uint32_t size) {
    nextSeq();
    addLog("alloc:" + hexList(size));
    return bits(g_header);
}

std::uint32_t __fastcall onLock(void*, void*, std::uint32_t key) {
    const auto ret = nextSeq();
    addLog("lk:" + hexList(key));
    return ret;
}

std::uint32_t __fastcall onMap(void* self, void*, void* out, void* keyPtr) {
    nextSeq();
    auto* map = static_cast<std::uint8_t*>(self);
    const std::uint32_t begin = loadU32(map + 4);
    const std::uint32_t count = loadU32(map + 8);
    const std::uint32_t endValue = loadU32(reinterpret_cast<void*>(static_cast<std::uintptr_t>(begin + count * 4)));
    const std::uint32_t key = loadU32(keyPtr);
    const bool hit = key == g_magic && g_node != nullptr;
    storeU32(out, hit ? bits(g_node) : endValue);
    addLog(std::string("map:") + hexList(key) + ":" + (hit ? "1" : "0"));
    return 0;
}

std::uint32_t __fastcall onUnlock(void*, void*, std::uint32_t flag) {
    const auto ret = nextSeq();
    addLog("ulk:" + hexList(flag));
    return ret;
}

std::uint32_t __fastcall onAccept(void*, void*, void*, std::uint32_t who, std::uint32_t src) {
    nextSeq();
    addLog("hit:" + hexList(who) + ":" + hexList(src));
    return g_accept;
}

std::uint32_t __fastcall onDelay(void*, void*, std::uint32_t key) {
    nextSeq();
    addLog("dly:" + hexList(key));
    return g_delay;
}

std::uint32_t __fastcall onHide1(void* self, void*, void* obj) {
    const auto ret = nextSeq();
    addLog("h1:" + hexList(loadU32(static_cast<std::uint8_t*>(self) + 4)) + ":" +
           hexList(loadU32(static_cast<std::uint8_t*>(obj) + 0x10)));
    return ret;
}

std::uint32_t __fastcall onHide2(void* self, void*, void* obj) {
    const auto ret = nextSeq();
    addLog("h2:" + hexList(loadU32(static_cast<std::uint8_t*>(self) + 4)) + ":" +
           hexList(loadU32(static_cast<std::uint8_t*>(obj) + 0x10)));
    return ret;
}

std::uint32_t __fastcall onKill(void*, void*, void* obj) {
    const auto ret = nextSeq();
    addLog("kill:" + hexList(loadU32(static_cast<std::uint8_t*>(obj) + 0x10)));
    return ret;
}

std::uint32_t __fastcall onTail(void*, void*) {
    const auto ret = nextSeq();
    addLog("tail");
    return ret;
}

std::uint32_t __fastcall onSchedule(void* self, void*, void* at, std::uint32_t when) {
    const auto ret = nextSeq();
    const auto marker = loadU32(static_cast<std::uint8_t*>(at) - 0x20);
    addLog("sch:" + hexList(loadU32(static_cast<std::uint8_t*>(self) + 4)) + ":" + hexList(when) + ":" +
           hexList(marker));
    return ret;
}

std::uint32_t __fastcall onReady(void* self, void*) {
    nextSeq();
    addLog("rdy:" + hexList(loadU32(static_cast<std::uint8_t*>(self) + 4)));
    return g_ready;
}

std::uint32_t __fastcall onFire(void* self, void*, void* target, std::uint32_t zero) {
    const auto ret = nextSeq();
    addLog("fire:" + hexList(loadU32(static_cast<std::uint8_t*>(self) + 4)) + ":" +
           hexList(loadU32(static_cast<std::uint8_t*>(target))) + ":" + hexList(zero));
    return ret;
}

void __stdcall onEnter(std::uint32_t cs) {
    nextSeq();
    addLog("cs:" + hexList(cs));
}

std::uint32_t __stdcall onLeave(std::uint32_t cs) {
    const auto ret = nextSeq();
    addLog("cso:" + hexList(cs));
    return ret;
}

std::uint32_t __fastcall onPoint(void*, void*, void* out) {
    const auto ret = nextSeq();
    addLog("pt");
    storeU32(out, g_repl[0]);
    storeU32(static_cast<std::uint8_t*>(out) + 4, g_repl[1]);
    storeU32(static_cast<std::uint8_t*>(out) + 8, g_repl[2]);
    return ret;
}

std::uint32_t __fastcall vt1c(void*, void*, std::uint32_t arg) {
    const auto ret = nextSeq();
    addLog("v1c:" + hexList(arg));
    return ret;
}
std::uint32_t __fastcall vt34(void*, void*) {
    const auto ret = nextSeq();
    addLog("v34");
    return ret;
}
std::uint32_t __fastcall vt38(void*, void*, std::uint32_t arg) {
    const auto ret = nextSeq();
    addLog("v38:" + hexList(arg));
    return ret;
}
std::uint32_t __fastcall vt18(void*, void*, std::uint32_t first, std::uint32_t second) {
    nextSeq();
    addLog("v18:" + hexList(first) + ":" + hexList(second));
    return g_embed;
}
std::uint32_t __fastcall vt28(void*, void*, std::uint32_t out) {
    const auto ret = nextSeq();
    addLog("v28");
    auto* dst = reinterpret_cast<std::uint8_t*>(static_cast<std::uintptr_t>(out));
    for (int i = 0; i < 5; ++i) {
        storeU32(dst + i * 4, g_rect[i]);
    }
    return ret;
}
std::uint32_t __fastcall vt3c(void*, void*, std::uint32_t, std::uint32_t) {
    const auto ret = nextSeq();
    addLog("v3c");
    return bits(g_repl);
}
std::uint32_t __fastcall vt5c(void*, void*, std::uint32_t arg) {
    const auto ret = nextSeq();
    addLog("v5c:" + hexList(arg));
    return ret;
}
std::uint32_t __fastcall vt80(void*, void*, std::uint32_t first, std::uint32_t second) {
    const auto ret = nextSeq();
    addLog("v80:" + hexList(first) + ":" + hexList(second));
    return ret;
}
std::uint32_t __fastcall vt84(void*, void*) {
    nextSeq();
    addLog(g_child != nullptr ? "v84:1" : "v84:0");
    return bits(g_child);
}
std::uint32_t __fastcall vt88(void*, void*, std::uint32_t arg) {
    const auto ret = nextSeq();
    addLog("v88:" + hexList(arg));
    return ret;
}
std::uint32_t __fastcall vt90(void*, void*) {
    const auto ret = nextSeq();
    addLog("v90");
    return ret;
}
std::uint32_t __fastcall vt98(void*, void*, std::uint32_t arg) {
    const auto ret = nextSeq();
    const auto key = arg < 0x10000u ? arg : loadU32(reinterpret_cast<void*>(static_cast<std::uintptr_t>(arg + 0xDC)));
    addLog("v98:" + hexList(key));
    return ret;
}
std::uint32_t __fastcall vt154(void*, void*, std::uint32_t interior, std::uint32_t zero) {
    const auto ret = nextSeq();
    const auto marker = loadU32(reinterpret_cast<void*>(static_cast<std::uintptr_t>(interior - 0x3F8)));
    addLog("v154:" + hexList(marker) + ":" + hexList(zero));
    return ret;
}
std::uint32_t __fastcall vt20c(void*, void*) {
    nextSeq();
    addLog("v20c");
    return g_arm;
}
std::uint32_t __fastcall vtCount(void*, void*) {
    nextSeq();
    addLog("ln");
    return g_listCount;
}
std::uint32_t __fastcall vtAt(void*, void*, std::uint32_t index) {
    nextSeq();
    addLog("la:" + hexList(index));
    return index == g_matchIndex ? g_matchObject : 0;
}

struct Arena {
    std::vector<std::vector<std::uint8_t>> blocks;
    std::uint8_t* alloc(std::size_t size) {
        blocks.emplace_back(size, static_cast<std::uint8_t>(0x11));
        return blocks.back().data();
    }
};

void putPtr(std::uint8_t* object, int offset, const void* value) { storeU32(object + offset, bits(value)); }
void putU32(std::uint8_t* object, int offset, std::uint32_t value) { storeU32(object + offset, value); }
void putFn(std::uint8_t* vt, int slot, const void* fn) { putPtr(vt, slot, fn); }

std::uint8_t* makeLoco(Arena& arena) {
    auto* vt = arena.alloc(0x40);
    putFn(vt, 0x1C, &vt1c);
    putFn(vt, 0x34, &vt34);
    putFn(vt, 0x38, &vt38);
    auto* loco = arena.alloc(0x20);
    putPtr(loco, 0, vt);
    return loco;
}

std::uint8_t* makeChain(Arena& arena) {
    auto* vt = arena.alloc(0xA0);
    putFn(vt, 0x80, &vt80);
    putFn(vt, 0x84, &vt84);
    putFn(vt, 0x88, &vt88);
    putFn(vt, 0x90, &vt90);
    auto* chain = arena.alloc(0x20);
    putPtr(chain, 0, vt);
    return chain;
}

struct Ai {
    std::uint8_t* self;
    std::uint8_t* object;
    std::uint8_t* templ;
    std::uint8_t* loco;
    std::uint8_t* dest;
};

Ai makeAi(Arena& arena, std::uint32_t flags) {
    Ai ai{};
    ai.self = arena.alloc(0x500);
    ai.object = arena.alloc(0x400);
    ai.templ = arena.alloc(0x100);
    ai.loco = makeLoco(arena);
    ai.dest = arena.alloc(0x30);
    putPtr(ai.self, 8, ai.object);
    putPtr(ai.self, 0x3C, ai.loco);
    putPtr(ai.self, 0x214, ai.dest);
    putPtr(ai.object, 4, ai.templ);
    putU32(ai.object, 0x328, 0);
    putU32(ai.object, 0x32C, 0);
    putU32(ai.object, 0x330, 0);
    putU32(ai.object, 0x374, 0);
    putU32(ai.object, 0x37C, 0);
    putU32(ai.self, 0x47C, 0);
    ai.self[0x436] = 0;
    putU32(ai.templ, 0xC0, flags);
    g_template = bits(ai.templ);
    g_objectBits = bits(ai.object);
    return ai;
}

std::string classify(std::uint32_t eax) {
    if (g_objectBits != 0 && eax == g_objectBits) {
        return "O";
    }
    if (g_template != 0 && eax == g_template) {
        return "T";
    }
    if (g_sentinel != 0 && eax == g_sentinel) {
        return "S";
    }
    if (g_weaponBits != 0 && eax == g_weaponBits) {
        return "W";
    }
    if (g_weaponAltBits != 0 && eax == g_weaponAltBits) {
        return "A";
    }
    if (g_moduleBits != 0 && eax == g_moduleBits) {
        return "M";
    }
    return hex32(eax);
}

std::string memList(std::uint8_t* object, const std::vector<int>& offsets) {
    if (offsets.empty()) {
        return ".";
    }
    std::ostringstream text;
    text << std::hex << std::nouppercase;
    for (std::size_t i = 0; i < offsets.size(); ++i) {
        if (i != 0) {
            text << ",";
        }
        text << offsets[i] << ":" << std::setw(8) << std::setfill('0') << loadU32(object + offsets[i]);
    }
    return text.str();
}

void emit(std::uint32_t eax, std::uint8_t* object, const std::vector<int>& offsets) {
    std::cout << "eax=" << hex32(eax) << " log=" << (g_log.empty() ? "." : g_log)
              << " mem=" << memList(object, offsets) << " nax=" << classify(eax) << " ext=" << g_ext << "\n";
}

std::uint32_t parseU32(const std::string& token) {
    return static_cast<std::uint32_t>(std::strtoul(token.c_str(), nullptr, 0));
}

void bindCalls() {
    using namespace ra3::gamelogic;
    setObject6CCalls(Object6CCalls{
        onUsable, onSteer, onPlace, onAim, onPairA, onApply59, onPairB, onApply5A, onDispatch, onState,
        onWeapon, onWeaponAlt, onReadId, onLookup, onNeg, onOne, onBase, onBit, onAlt, onFind, onUnlinkA,
        onUnlinkB, onRelease, onAlloc, onLock, onMap, onUnlock, onAccept, onDelay, onHide1, onHide2, onKill,
        onTail, onSchedule, onReady, onFire, onEnter, onLeave, onPoint});
    setObject6CEnv(Object6CEnv{&g_clockStore, &g_registryStore, &g_worldStore, &g_catalogStore, nullptr});
}

std::uint8_t* linkCircular(Arena& arena, std::uint8_t* sentinel, const std::vector<std::uint32_t>& keys) {
    putPtr(sentinel, 0, sentinel);
    putPtr(sentinel, 4, sentinel);
    std::uint8_t* prev = sentinel;
    for (std::uint32_t key : keys) {
        auto* node = arena.alloc(0x10);
        putU32(node, 8, key);
        putPtr(node, 0, sentinel);
        putPtr(node, 4, prev);
        putPtr(prev, 0, node);
        putPtr(sentinel, 4, node);
        prev = node;
    }
    return sentinel;
}

std::string keysOf(std::uint8_t* sentinel) {
    std::ostringstream text;
    text << std::hex << std::nouppercase;
    auto* node = reinterpret_cast<std::uint8_t*>(static_cast<std::uintptr_t>(loadU32(sentinel)));
    bool any = false;
    int guard = 0;
    while (node != sentinel && guard < 8) {
        if (any) {
            text << ".";
        }
        any = true;
        text << loadU32(node + 8);
        node = reinterpret_cast<std::uint8_t*>(static_cast<std::uintptr_t>(loadU32(node)));
        ++guard;
    }
    return any ? text.str() : ".";
}

}  // namespace

int main() {
    using namespace ra3::gamelogic;
    bindCalls();
    std::string line;
    while (std::getline(std::cin, line)) {
        if (line.empty()) {
            continue;
        }
        std::istringstream input(line);
        std::string cmd;
        input >> cmd;
        if (cmd == "quit") {
            break;
        }
        std::vector<std::string> args;
        std::vector<int> mem;
        bool inMem = false;
        std::string token;
        while (input >> token) {
            if (token == "mem") {
                inMem = true;
                continue;
            }
            if (inMem) {
                mem.push_back(static_cast<int>(parseU32(token)));
            } else {
                args.push_back(token);
            }
        }
        auto need = [&](std::size_t n) { return args.size() >= n; };
        auto arg = [&](std::size_t index) { return parseU32(args[index]); };
        Arena arena;
        resetProbe();
        std::uint8_t* nameLock = arena.alloc(0x20);
        setObject6CEnv(Object6CEnv{&g_clockStore, &g_registryStore, &g_worldStore, &g_catalogStore, nameLock});
        std::uint32_t eax = 0;
        std::uint8_t* self = arena.alloc(0x20);
        if (!need(1) && cmd != "quit") {
            std::cout << "err=args\n";
            continue;
        }
        if (cmd == "latch" && need(2)) {
            self = arena.alloc(0x120);
            self[0x10F] = static_cast<std::uint8_t>(arg(0));
            putU32(self, 0x40, arg(1));
            eax = StancesBehavior_latchGoal(self);
        } else if (cmd == "setbit" && need(2)) {
            self = arena.alloc(0x120);
            const std::uint32_t index = arg(0);
            putU32(self, static_cast<int>(((index >> 5) * 4u) + 0x100u), arg(1));
            g_moduleBits = bits(self);
            eax = StancesBehavior_setBit(self, index);
        } else if (cmd == "clrbit" && need(2)) {
            self = arena.alloc(0x120);
            const std::uint32_t index = arg(0);
            putU32(self, static_cast<int>(((index >> 5) * 4u) + 0x100u), arg(1));
            g_moduleBits = bits(self);
            eax = StancesBehavior_clearBit(self, index);
        } else if (cmd == "linkflag" && need(2)) {
            auto* block = arena.alloc(0x40);
            self = block + 16;
            auto* primary = arena.alloc(0xA0);
            putPtr(self, -8, primary);
            putU32(primary, 0x98, arg(1));
            eax = StancesBehavior_setLinkedFlag(self, arg(0));
            self = primary;
        } else if (cmd == "linkbyte" && need(1)) {
            auto* block = arena.alloc(0x40);
            self = block + 16;
            auto* primary = arena.alloc(0x40);
            putPtr(self, -8, primary);
            eax = StancesBehavior_setLinkedByte(self, arg(0));
            g_ext = "v" + hex32(loadU32(self + 0x28));
            self = primary;
        } else if (cmd == "release" && need(2)) {
            auto* block = arena.alloc(0x120);
            self = block + 16;
            auto* primary = arena.alloc(0xA0);
            putPtr(self, -8, primary);
            putU32(primary, 0x94, arg(0));
            std::vector<std::uint32_t> keys;
            for (std::size_t i = 2; i < args.size(); ++i) {
                keys.push_back(arg(i));
            }
            linkCircular(arena, self + 0xD4, keys);
            g_header = arena.alloc(0x20);
            putU32(g_header, 0x10, 0);
            g_sentinel = bits(self + 0xD4);
            eax = StancesBehavior_releaseRecord(self, arg(1));
            const std::uint32_t freed = loadU32(g_header + 0x10);
            const std::uint32_t freedKey =
                freed == 0 ? 0 : loadU32(reinterpret_cast<void*>(static_cast<std::uintptr_t>(freed + 8)));
            g_ext = keysOf(self + 0xD4) + ":" + hex32(freedKey);
            self = primary;
        } else if (cmd == "readbody" && need(2)) {
            self = arena.alloc(0x130);
            self[0x11F] = static_cast<std::uint8_t>(arg(0));
            putU32(self, 0x50, arg(1));
            eax = HighlanderBody_readIfUnset(self);
        } else if (cmd == "applybody" && need(3)) {
            self = arena.alloc(0x50);
            auto* templ = arena.alloc(0x40);
            auto* host = arena.alloc(0x10);
            putPtr(self, 4, templ);
            putPtr(self, 8, host);
            putU32(self, 0x44, arg(0));
            putU32(templ, 0x30, arg(1));
            putU32(templ, 0x34, arg(2));
            eax = HighlanderBody_applyTemplate(self);
        } else if (cmd == "crate" && need(1)) {
            self = arena.alloc(0x20);
            auto* templ = arena.alloc(0x80);
            putPtr(self, 4, templ);
            putU32(templ, 0x6C, arg(0));
            std::uint8_t* other = nullptr;
            if (arg(0) != 0xFFFFFFFF) {
                other = arena.alloc(0x10);
            }
            eax = AttributeCrateCollide_onCollide(self, other);
        } else if (cmd == "followpos" && need(6)) {
            g_usable = arg(0);
            auto ai = makeAi(arena, 0);
            putU32(ai.object, 0x38, arg(1));
            putU32(ai.object, 0x3C, arg(2));
            putU32(ai.object, 0x40, arg(3));
            if (arg(4) == 0) {
                putU32(ai.self, 0x214, 0);
            }
            eax = AIUpdateInterface_followPosition(ai.self, arg(5), 7);
            g_ext = hex32(loadU32(ai.dest + 0x0C)) + hex32(loadU32(ai.dest + 0x10)) + hex32(loadU32(ai.dest + 0x14));
            self = ai.self;
        } else if (cmd == "followobj" && need(3)) {
            g_usable = arg(0);
            auto ai = makeAi(arena, 0);
            ai.self[0x436] = static_cast<std::uint8_t>(arg(1));
            putU32(ai.object, 0x38, 1);
            putU32(ai.object, 0x3C, 2);
            putU32(ai.object, 0x40, 3);
            eax = AIUpdateInterface_followObject(ai.self, arg(2), 8);
            g_ext = hex32(loadU32(ai.dest + 0x0C));
            self = ai.self;
        } else if (cmd == "steer1b" && need(2)) {
            g_usable = arg(0);
            auto ai = makeAi(arena, 0);
            eax = AIUpdateInterface_steerState1B(ai.self, arg(1), 4);
            self = ai.self;
        } else if (cmd == "pair59" && need(3)) {
            g_pair = arg(0);
            auto ai = makeAi(arena, 0);
            g_registryStore = bits(arena.alloc(0x10));
            eax = AIUpdateInterface_tryCommand59(ai.self, arg(1), arg(2));
            self = ai.self;
        } else if (cmd == "pair5a" && need(3)) {
            g_pair = arg(0);
            auto ai = makeAi(arena, 0);
            g_registryStore = bits(arena.alloc(0x10));
            eax = AIUpdateInterface_tryCommand5A(ai.self, arg(1), arg(2));
            self = ai.self;
        } else if (cmd == "weaponor" && need(4)) {
            g_usable = arg(0);
            g_disp = arg(1);
            auto ai = makeAi(arena, 0);
            g_registryStore = bits(arena.alloc(0x10));
            if (arg(2) != 0) {
                auto* chain = makeChain(arena);
                putPtr(ai.object, 0x328, chain);
                if (arg(3) != 0) {
                    g_child = makeChain(arena);
                }
            }
            eax = AIUpdateInterface_weaponOrFollow(ai.self, 0x21, 0x22);
            self = ai.self;
        } else if (cmd == "loco" && need(3)) {
            g_usable = arg(0);
            auto ai = makeAi(arena, 0);
            const std::uint32_t which = arg(1);
            if (which == 0x0E) {
                eax = AIUpdateInterface_locoState0E(ai.self, arg(2), 9);
            } else if (which == 0x2D) {
                eax = AIUpdateInterface_locoState2D(ai.self, arg(2), 9);
            } else if (which == 0x69) {
                eax = AIUpdateInterface_locoState69(ai.self, arg(2), 9);
            } else if (which == 0x13) {
                eax = AIUpdateInterface_locoState13(ai.self, arg(2), 9);
            } else if (which == 0x14) {
                eax = AIUpdateInterface_locoState14(ai.self, arg(2), 9);
            } else {
                eax = AIUpdateInterface_locoState38(ai.self, arg(2), 9);
            }
            self = ai.self;
        } else if (cmd == "place2d" && need(2)) {
            g_usable = arg(0);
            auto ai = makeAi(arena, 0);
            eax = AIUpdateInterface_placeState2D(ai.self, arg(1), 6);
            self = ai.self;
        } else if (cmd == "aim" && need(3)) {
            g_usable = arg(0);
            auto ai = makeAi(arena, 0);
            eax = arg(1) == 0x11 ? AIUpdateInterface_aimState11(ai.self, arg(2), 3)
                                 : AIUpdateInterface_aimState12(ai.self, arg(2), 3);
            self = ai.self;
        } else if (cmd == "tail" && need(3)) {
            g_usable = arg(0);
            auto ai = makeAi(arena, 0);
            eax = arg(1) == 0x29 ? AIUpdateInterface_tailState29(ai.self, arg(2))
                                 : AIUpdateInterface_tailState48(ai.self, arg(2));
            self = ai.self;
        } else if (cmd == "notify" && need(4)) {
            auto ai = makeAi(arena, 0);
            if (arg(0) != 0) {
                auto* chain = makeChain(arena);
                putPtr(ai.object, 0x328, chain);
            }
            if (arg(1) != 0) {
                auto* passenger = arena.alloc(0x40);
                putPtr(ai.object, 0x374, passenger);
            }
            eax = AIUpdateInterface_notifyPassenger(ai.self, arg(2), arg(3));
            self = ai.self;
        } else if (cmd == "guard" && need(8)) {
            g_usable = arg(0);
            auto ai = makeAi(arena, arg(1));
            putU32(ai.self, 0x5C, arg(2));
            auto* point = arena.alloc(0x10);
            putU32(point, 0, arg(3));
            putU32(point, 4, arg(4));
            putU32(point, 8, arg(5));
            g_rect[0] = 0;
            g_rect[1] = 0;
            g_rect[2] = 0;
            g_rect[3] = 100;
            g_rect[4] = 100;
            g_repl[0] = 9;
            g_repl[1] = 8;
            g_repl[2] = 7;
            auto* world = arena.alloc(0x40);
            auto* vt = arena.alloc(0x40);
            putFn(vt, 0x28, &vt28);
            putFn(vt, 0x3C, &vt3c);
            putPtr(world, 0, vt);
            g_worldStore = bits(world);
            eax = AIUpdateInterface_guardRect(ai.self, bits(point), arg(7), arg(6));
            self = ai.self;
        } else if (cmd == "state18" && need(3)) {
            g_usable = arg(0);
            auto ai = makeAi(arena, arg(1));
            eax = AIUpdateInterface_state18(ai.self, arg(2), 5);
            self = ai.self;
        } else if (cmd == "actor50" && need(4)) {
            g_usable = arg(0);
            auto ai = makeAi(arena, arg(1));
            putU32(ai.self, 0x5C, arg(2));
            auto* actor = arena.alloc(0xE0);
            putU32(actor, 0xDC, arg(3));
            eax = AIUpdateInterface_followActor50(ai.self, bits(actor), 4, 5);
            self = ai.self;
        } else if (cmd == "point50" && need(4)) {
            g_usable = arg(0);
            auto ai = makeAi(arena, arg(1));
            putU32(ai.self, 0x5C, arg(2));
            auto* point = arena.alloc(0x10);
            putU32(point, 8, arg(3));
            eax = AIUpdateInterface_followPoint50(ai.self, bits(point), 4, 5);
            self = ai.self;
        } else if (cmd == "point4f" && need(4)) {
            g_usable = arg(0);
            auto ai = makeAi(arena, arg(1));
            putU32(ai.self, 0x5C, 4);
            if (arg(2) != 0) {
                auto* extra = arena.alloc(0xB0);
                putPtr(ai.self, 0x47C, extra);
            }
            g_repl[0] = 3;
            g_repl[1] = 4;
            g_repl[2] = 5;
            std::uint32_t point = 0;
            if (arg(3) != 0) {
                auto* bytes = arena.alloc(0x10);
                putU32(bytes, 0, 11);
                putU32(bytes, 4, 12);
                putU32(bytes, 8, 13);
                point = bits(bytes);
            }
            eax = AIUpdateInterface_guardPoint4F(ai.self, 0x70, 0x71, 0x72, point);
            if (arg(2) != 0) {
                auto* extra = *reinterpret_cast<std::uint8_t**>(ai.self + 0x47C);
                g_ext = hex32(loadU32(extra + 0xA8));
            }
            self = ai.self;
        } else if (cmd == "move3c" && need(3)) {
            auto ai = makeAi(arena, 0);
            if (arg(0) != 0) {
                auto* weapons = arena.alloc(0x10);
                putPtr(ai.object, 0x37C, weapons);
                if (arg(1) != 0) {
                    g_weapon = arena.alloc(0x30);
                    g_weaponBits = bits(g_weapon);
                } else if (arg(2) != 0) {
                    g_weaponAlt = arena.alloc(0x30);
                    g_weaponAltBits = bits(g_weaponAlt);
                }
            }
            eax = AIUpdateInterface_weaponMove3C(ai.self, arg(0) == 2 ? 0 : 0x15, 0x16, 0x17, 0x18);
            self = ai.self;
        } else if (cmd == "roster" && need(4)) {
            g_usable = 1;
            auto ai = makeAi(arena, arg(0));
            auto* actor = arena.alloc(0x20);
            auto* actorTempl = arena.alloc(0x10);
            putPtr(actor, 4, actorTempl);
            putU32(actorTempl, 4, arg(1));
            if (arg(2) != 0) {
                auto* passenger = arena.alloc(0x30);
                putPtr(ai.object, 0x374, passenger);
            }
            g_listCount = arg(3);
            g_matchIndex = arg(3) > 0 ? 0 : 0xFFFFFFFF;
            g_matchObject = bits(actor);
            g_header = arena.alloc(0x20);
            auto* listVt = arena.alloc(0x20);
            putFn(listVt, 0, &vtCount);
            putFn(listVt, 0x0C, &vtAt);
            putPtr(g_header, 0, listVt);
            g_catalogStore = bits(arena.alloc(0x10));
            eax = AIUpdateInterface_matchRoster(ai.self, bits(actor), 0x2A);
            self = ai.self;
        } else if (cmd == "walk" && need(3)) {
            auto ai = makeAi(arena, arg(0));
            if (arg(1) != 0) {
                putPtr(ai.object, 0x374, arena.alloc(0x10));
            }
            g_listCount = arg(2);
            g_matchIndex = 0xFFFFFFFF;
            g_header = arena.alloc(0x20);
            auto* listVt = arena.alloc(0x20);
            putFn(listVt, 0, &vtCount);
            putFn(listVt, 0x0C, &vtAt);
            putPtr(g_header, 0, listVt);
            g_catalogStore = bits(arena.alloc(0x10));
            auto* actor = arena.alloc(0x10);
            eax = AIUpdateInterface_walkRoster(ai.self, arg(1) == 2 ? 0 : bits(actor), 0, 0);
            self = ai.self;
        } else if (cmd == "deploy" && need(4)) {
            self = arena.alloc(0x40);
            auto* vt = arena.alloc(0x20);
            putFn(vt, 0x18, &vt18);
            putPtr(self, 0x24, vt);
            g_embed = arg(0);
            auto* object = arena.alloc(0x380);
            g_objectBits = bits(object);
            auto* passenger = arena.alloc(0x220);
            auto* pvt = arena.alloc(0x220);
            putFn(pvt, 0x20C, &vt20c);
            putPtr(passenger, 0, pvt);
            g_arm = arg(1);
            putU32(object, 0x374, 0);
            if (arg(2) != 0) {
                putPtr(object, 0x374, passenger);
                auto* component = arena.alloc(0x40);
                putPtr(passenger, 0x214, component);
                putU32(component, 0x38, arg(3));
            }
            eax = ToggleDeploySpecialAbilityUpdate_tryDeploy(self, object);
        } else if (cmd == "horde" && need(2)) {
            self = arena.alloc(0x200);
            auto* faceVt = arena.alloc(0x160);
            putFn(faceVt, 0x98, &vt98);
            putFn(faceVt, 0x154, &vt154);
            putPtr(self, 0x24, faceVt);
            auto* object = arena.alloc(0x400);
            putU32(object, 0, 0x51);
            putPtr(self, 8, object);
            auto* member = arena.alloc(0xE0);
            putU32(member, 0xDC, arg(0));
            g_hit = arg(1);
            if (g_hit) {
                g_node = arena.alloc(0x10);
                putU32(g_node, 0, 0x77);
                putU32(g_node, 8, 0x77);
            }
            putU32(self, 0x1C8, 3);
            eax = HordeContain_removeMember(self, member);
        } else if (cmd == "rmkey" && need(1)) {
            self = arena.alloc(0x90);
            std::vector<std::uint32_t> keys;
            for (std::size_t i = 1; i < args.size(); ++i) {
                keys.push_back(arg(i));
            }
            linkCircular(arena, self + 0x78, keys);
            putU32(self, 0x84, static_cast<std::uint32_t>(keys.size()));
            eax = OpenContain_removeByKey(self, arg(0));
            g_ext = keysOf(self + 0x78);
        } else if (cmd == "passive" && need(4)) {
            self = arena.alloc(0x20);
            auto* templ = arena.alloc(0x40);
            putU32(templ, 0, 0x61);
            putPtr(self, 4, templ);
            putU32(templ, 0x30, arg(0));
            auto* list = arena.alloc(0x10);
            putU32(list, 0, arg(1));
            putU32(list, 4, arg(1) ^ 1u);
            putPtr(templ, 0x34, list);
            putU32(templ, 0x18, arg(2));
            putU32(templ, 0x24, 0);
            auto* clock = arena.alloc(0x1E0);
            putU32(clock, 0x58, 10);
            putU32(clock, 0x1C4, arg(3));
            g_clockStore = bits(clock);
            auto* target = arena.alloc(0x380);
            putU32(target, 0, 0x44);
            putU32(target, 0x36C, 0);
            if (need(5) && arg(4) != 0) {
                auto* actor = arena.alloc(0x20);
                putU32(actor, 4, 0x64);
                putPtr(target, 0x36C, actor);
                if (arg(4) == 2) {
                    auto* effect = arena.alloc(0x20);
                    putU32(effect, 4, 0x65);
                    putPtr(templ, 0x24, effect);
                    g_ready = need(6) ? arg(5) : 0;
                }
            }
            eax = PassiveAreaEffectBehavior_apply(self, need(5) && arg(4) == 3 ? nullptr : target);
        } else if (cmd == "relay" && need(1)) {
            self = arena.alloc(0x20);
            auto* vt = arena.alloc(0x60);
            putFn(vt, 0x5C, &vt5c);
            putPtr(self, 0, vt);
            auto* object = arena.alloc(0x420);
            putPtr(self, 8, object);
            putU32(object, 0x418, arg(0));
            eax = UnifiedControlBehavior_relay(self);
        } else if (cmd == "spawn" && need(3)) {
            self = arena.alloc(0x80);
            auto* templ = arena.alloc(0x60);
            putPtr(self, 4, templ);
            templ[0x50] = static_cast<std::uint8_t>(arg(0));
            g_template = bits(templ);
            std::vector<std::uint32_t> keys;
            if (arg(1) != 0) {
                keys.push_back(arg(1));
            }
            linkCircular(arena, self + 0x5C, keys);
            auto* clock = arena.alloc(0x200);
            g_clockStore = bits(clock);
            auto* buckets = arena.alloc(0x10);
            auto* endNode = arena.alloc(0x10);
            putPtr(buckets, 0, endNode);
            putPtr(clock, 0xC8, buckets);
            putU32(clock, 0xCC, 0);
            g_magic = arg(1);
            if (arg(2) != 0) {
                g_node = arena.alloc(0x20);
                auto* actor = arena.alloc(0x500);
                putU32(actor, 0x10, 0x81);
                putPtr(actor, 4, arena.alloc(0xD0));
                putU32(*reinterpret_cast<std::uint8_t**>(actor + 4), 0xC8, arg(2) == 2 ? 0x02000000u : 0);
                auto* comp = arena.alloc(0x10);
                putU32(comp, 4, 0x82);
                putPtr(actor, 0x418, comp);
                actor[0x4D7] = arg(2) == 3 ? 0 : 1;
                actor[0x471] = arg(2) == 4 ? 1 : 0;
                putPtr(g_node, 4, actor);
            }
            eax = SpawnBehavior_update(self);
        } else if (cmd == "notehit" && need(4)) {
            self = arena.alloc(0x50);
            auto* info = arena.alloc(0x30);
            putU32(info, 0x20, arg(0));
            putU32(info, 8, arg(1));
            if (arg(2) != 0) {
                putPtr(self, 0x3C, arena.alloc(0x10));
            } else {
                putU32(self, 0x3C, 0);
            }
            putU32(self, 0x40, arg(3));
            putU32(self, 0x28, 100);
            putU32(self, 8, 0x19);
            auto* clock = arena.alloc(0x200);
            putU32(clock, 0x58, 50);
            g_clockStore = bits(clock);
            auto* buckets = arena.alloc(0x10);
            putPtr(buckets, 0, arena.alloc(0x8));
            putPtr(clock, 0xC8, buckets);
            putU32(clock, 0xCC, 0);
            g_magic = arg(1);
            if (arg(1) != 0) {
                g_node = arena.alloc(0x10);
                putU32(g_node, 4, 0x33);
            }
            g_accept = need(5) ? arg(4) : 1;
            g_delay = 4;
            eax = HighlanderBody_noteHit(self, info);
        } else {
            std::cout << "err=unknown\n";
            continue;
        }
        emit(eax, self, mem);
    }
    return 0;
}
