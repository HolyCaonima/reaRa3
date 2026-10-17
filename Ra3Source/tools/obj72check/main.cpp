#include "ra3/gamelogic/ObjectModules72.h"

#include <cstdint>
#include <cstring>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

// Line protocol shared with tools/analysis/ports/verify_obj72.py.
// Ordinary stubs log a name then return seq*0x10 (seq starts at 1).
// Forced stubs log a value and do not consume seq.
// Pointers in the log are "p"; values below 0x10000 or at/above 0xF0000000
// are bare hex. Field writers stamp 0xA1000000+seq. Pointer EAX is reduced
// to a tag so the two heaps can be compared.

namespace {

std::string g_log;
int g_seq = 0;
std::uint32_t g_usable = 1;
std::uint32_t g_cur = 0;
std::uint32_t g_query = 0;
std::uint32_t g_d2120 = 0;
std::uint32_t g_a18a0 = 0;
std::uint32_t g_ready = 1;
std::uint32_t g_kind = 0;
std::uint32_t g_idle = 0;
int g_verOut = -1;
int g_stream = -1;
bool g_findMiss = false;
std::uint8_t* g_weapon = nullptr;
std::uint32_t g_fbits = 0;

struct Arena {
    std::vector<std::vector<std::uint8_t>> blocks;
    std::uint8_t* alloc(std::size_t size) {
        blocks.emplace_back(size + 0x100, static_cast<std::uint8_t>(0));
        const auto raw = reinterpret_cast<std::uintptr_t>(blocks.back().data());
        const auto aligned = (raw + 0xFFu) & ~std::uintptr_t{0xFFu};
        return reinterpret_cast<std::uint8_t*>(aligned);
    }
};

Arena* g_arena = nullptr;

void resetProbe() {
    g_log.clear();
    g_seq = 0;
    g_usable = 1;
    g_cur = 0;
    g_query = 0;
    g_d2120 = 0;
    g_a18a0 = 0;
    g_ready = 1;
    g_kind = 0;
    g_idle = 0;
    g_verOut = -1;
    g_stream = -1;
    g_findMiss = false;
    g_fbits = 0;
}

void addLog(const std::string& text) {
    if (!g_log.empty()) {
        g_log += ",";
    }
    g_log += text;
}

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

std::uint32_t take(const std::string& name) {
    ++g_seq;
    addLog(name);
    return static_cast<std::uint32_t>(g_seq) * 0x10u;
}

std::uint32_t stampRet(const char* name, void* field) {
    ++g_seq;
    storeU32(field, 0xA1000000u + static_cast<std::uint32_t>(g_seq));
    addLog(std::string(name) + ":" + std::to_string(g_seq));
    return static_cast<std::uint32_t>(g_seq) * 0x10u;
}

bool isVersion(std::uint32_t address) {
    if (address < 0x10000u || address >= 0xF0000000u) {
        return false;
    }
    const auto* bytes = reinterpret_cast<const std::uint8_t*>(address);
    return bytes[0] == 1 && (bytes[1] == 1 || bytes[1] == 2);
}

std::uint32_t onPtr(int slot, void* arg) {
    const auto address = static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(arg));
    if (slot == 0x18 && isVersion(address)) {
        const auto* bytes = reinterpret_cast<std::uint8_t*>(arg);
        std::ostringstream text;
        text << std::hex << std::nouppercase << "v" << slot << ":" << std::setw(2) << std::setfill('0')
             << static_cast<int>(bytes[0]) << std::setw(2) << std::setfill('0') << static_cast<int>(bytes[1]);
        if (g_verOut >= 0) {
            reinterpret_cast<std::uint8_t*>(arg)[1] = static_cast<std::uint8_t>(g_verOut);
        }
        return take(text.str());
    }
    if (slot == 0x6C && g_stream >= 0 && address >= 0x10000u && address < 0xF0000000u) {
        storeU32(arg, static_cast<std::uint32_t>(g_stream));
        g_stream = -1;
    }
    std::ostringstream text;
    text << "v" << std::hex << slot << ":" << fmt(address);
    return take(text.str());
}

std::uint32_t onImm(int slot, std::uint32_t arg) {
    std::ostringstream text;
    text << "v" << std::hex << slot << ":" << fmt(arg);
    return take(text.str());
}

std::uint32_t __fastcall v0(void*, void*) {
    addLog("v0:" + fmt(g_ready));
    return g_ready;
}
std::uint32_t __fastcall v18(void*, void*, void* arg) { return onPtr(0x18, arg); }
std::uint32_t __fastcall v1c(void*, void*, std::uint32_t arg) { return onImm(0x1C, arg); }
std::uint32_t __fastcall v34(void*, void*) { return take("v34"); }
std::uint32_t __fastcall v38(void*, void*, std::uint32_t arg) { return onImm(0x38, arg); }
std::uint32_t __fastcall v54(void*, void*) {
    addLog("v54:" + fmt(g_kind));
    return g_kind;
}
std::uint32_t __fastcall v58(void*, void*, void* arg) { return onPtr(0x58, arg); }
std::uint32_t __fastcall v64(void*, void*, void* arg) { return onPtr(0x64, arg); }
std::uint32_t __fastcall v6c(void*, void*, void* arg) { return onPtr(0x6C, arg); }
std::uint32_t __fastcall v7c(void*, void*, void* arg) { return onPtr(0x7C, arg); }
std::uint32_t __fastcall v84(void*, void*, void* arg) { return onPtr(0x84, arg); }
std::uint32_t __fastcall v288(void*, void*) { return take("v288"); }

std::uint32_t __fastcall usable(void*, void*) {
    addLog("u:" + fmt(g_usable));
    return g_usable;
}
std::uint32_t __fastcall aimAt(void*, void*, std::uint32_t target) { return take("aim:" + fmt(target)); }
std::uint32_t __fastcall findWeapon(void*, void*, std::uint32_t) {
    if (g_weapon == nullptr || g_findMiss) {
        addLog("fw0");
        return 0;
    }
    addLog("fw");
    return static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(g_weapon));
}
std::uint32_t __fastcall findWeaponAlt(void*, void*, std::uint32_t, std::uint32_t) {
    if (g_weapon == nullptr) {
        addLog("fa0");
        return 0;
    }
    addLog("fa");
    return static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(g_weapon));
}
std::uint32_t __fastcall followCd10(void*, void*, std::uint32_t arg) { return take("cd:" + fmt(arg)); }
std::uint32_t __fastcall followCe90(void*, void*, std::uint32_t arg) { return take("ce:" + fmt(arg)); }
std::uint32_t __fastcall followCf30(void*, void*, std::uint32_t arg) { return take("cf:" + fmt(arg)); }
std::uint32_t __fastcall followCdf0(void*, void*, void*) { return take("cdf"); }
std::uint32_t __fastcall markObject(void*, void*, std::uint32_t arg) { return take("mk:" + fmt(arg)); }
std::uint32_t __fastcall prepareDed70(void*, void*, std::uint32_t arg) { return take("ded:" + fmt(arg)); }
std::uint32_t __fastcall queryLoco(void*, void*) {
    addLog("q:" + fmt(g_query));
    return g_query;
}
std::uint32_t __fastcall currentTarget(void*, void*) {
    addLog("cur:" + fmt(g_cur));
    return g_cur;
}
std::uint32_t __fastcall readPoint(void*, void*, void*) { return take("rpt"); }
std::uint32_t __fastcall queryD2120(void*, void*) {
    addLog("qd:" + fmt(g_d2120));
    return g_d2120;
}
std::uint32_t __fastcall queryA18a0(void*, void*) {
    addLog("qa:" + fmt(g_a18a0));
    return g_a18a0;
}
std::uint32_t __fastcall bindPair(void*, void*, void*) { return take("bind"); }
std::uint32_t __fastcall noteAa2a0(void*, void*, std::uint32_t arg) { return take("note:" + fmt(arg)); }
std::uint32_t __fastcall placeAc4f0(void*, void*, void* pos, std::uint32_t bits) {
    g_fbits = bits;
    return take("place:" + fmt(static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(pos))) + ":" + hex8(bits));
}
std::uint32_t __fastcall steerB67a0(void*, void*, void* pos, std::uint32_t bits, std::uint32_t mode) {
    g_fbits = bits;
    return take("steer:" + fmt(static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(pos))) + ":" + hex8(bits) +
                ":" + fmt(mode));
}
std::uint32_t __fastcall applyKey(void*, void*, std::uint32_t key) { return take("key:" + fmt(key)); }
std::uint32_t __fastcall triggerAbility(void*, void*, std::uint32_t flag) { return take("trig:" + fmt(flag)); }
std::uint32_t __fastcall abilityReady(void*, void*) { return take("rdy"); }
std::uint32_t __fastcall xferBase88(void*, void*, std::uint32_t a, std::uint32_t b) {
    return take("x88:" + fmt(a) + ":" + fmt(b));
}
std::uint32_t __fastcall beamPulse(void*, void*) { return take("pulse"); }
std::uint32_t __fastcall applyBeam(void*, void*, void*, std::uint32_t value) { return take("beam:" + fmt(value)); }
std::uint32_t __fastcall notifyBit(void*, void*, std::uint32_t code, std::uint32_t flag) {
    return take("ntf:" + fmt(code) + ":" + fmt(flag));
}
std::uint32_t __fastcall allocNode(void*, void*, std::uint32_t, std::uint32_t) {
    addLog("alloc");
    return static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(g_arena->alloc(0x20)));
}
std::uint32_t __fastcall growVector(void* vec, void*, std::uint32_t end, std::uint32_t valuePtr) {
    const std::uint32_t id = loadU32(reinterpret_cast<void*>(valuePtr));
    if (end != 0) {
        storeU32(reinterpret_cast<void*>(end), id);
    }
    storeU32(static_cast<std::uint8_t*>(vec) + 4, end + 4);
    return take("grow:" + fmt(id));
}
std::uint32_t __fastcall xferRoot(void*, void*, void*) { return take("root"); }
std::uint32_t __fastcall xferAt10(void*, void*, void*) { return take("at10"); }
std::uint32_t __fastcall xferStealth(void*, void*, void*) { return take("stl"); }
std::uint32_t __fastcall resizeCrate(void*, void*, std::uint32_t n) { return take("rsz:" + fmt(n)); }
std::uint32_t __fastcall resizeStrafe(void*, void*, std::uint32_t n) { return take("rsz:" + fmt(n)); }
std::uint32_t __fastcall resizeStorage(void*, void*, std::uint32_t n) { return take("rsz:" + fmt(n)); }
std::uint32_t __fastcall xferShieldA(void*, void*, void*) { return take("sa"); }
std::uint32_t __fastcall xferShieldB(void*, void*, void*) { return take("sb"); }
std::uint32_t __fastcall xferShieldC(void*, void*, void*) { return take("sc"); }
std::uint32_t __fastcall xferPower(void*, void*, void*) { return take("pwr"); }
std::uint32_t __fastcall stopMonitor(void*, void*) { return take("stop"); }
std::uint32_t __fastcall clearUpgrade(void*, void*) { return take("clr"); }
std::uint32_t __fastcall activateOne(void*, void*, std::uint32_t flag) { return take("act:" + fmt(flag)); }
std::uint32_t __fastcall weaponIdle(void*, void*) {
    addLog("idle:" + fmt(g_idle));
    return g_idle;
}
std::uint32_t __stdcall fireAt(void*, void*) { return take("fire"); }
std::uint32_t __fastcall xferDeploy(void*, void*, void*) { return take("dep"); }
std::uint32_t __fastcall xferAbility(void*, void*, void*) { return take("abil"); }
std::uint32_t __fastcall xferAi(void*, void*, void*) { return take("ai"); }
std::uint32_t __fastcall xferBody(void*, void*, void*) { return take("body"); }
std::uint32_t __cdecl xferField(void*, void* field) { return stampRet("fld", field); }
std::uint32_t __cdecl xferPair(void*, void* field) { return stampRet("pair", field); }
std::uint32_t __cdecl xferRadarTail(void*, void* field) { return stampRet("rad", field); }

void putPtr(std::uint8_t* object, int offset, const void* value) {
    storeU32(object + offset, static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(value)));
}

void putU32(std::uint8_t* object, int offset, std::uint32_t value) { storeU32(object + offset, value); }

void putFn(std::uint8_t* vt, int slot, const void* fn) { putPtr(vt, slot, fn); }

std::uint32_t parseU32(const std::string& token) {
    return static_cast<std::uint32_t>(std::strtoul(token.c_str(), nullptr, 0));
}

struct Hints {
    std::uint32_t weapon = 0;
    std::uint32_t templ = 0;
    std::uint32_t object = 0;
    std::uint32_t planted = 0;
    std::uint32_t oldEnd = 0;
    std::uint32_t begin = 0;
    std::uint32_t end = 0;
};

void classify(std::uint32_t eax, const Hints& hints, std::uint32_t& shown, std::string& tag) {
    if (hints.weapon != 0 && eax == hints.weapon) {
        shown = 0;
        tag = "wpn";
        return;
    }
    if (hints.templ != 0 && eax == hints.templ) {
        shown = 0;
        tag = "tmpl";
        return;
    }
    if (hints.object != 0 && eax == hints.object) {
        shown = 0;
        tag = "obj";
        return;
    }
    if (hints.object != 0 && (eax & 0xFFFFFF00u) == (hints.object & 0xFFFFFF00u) && (eax & 0xFFu) == 1u) {
        shown = 1;
        tag = "mix";
        return;
    }
    if (hints.planted != 0 && eax == hints.planted) {
        shown = 0;
        tag = "hit";
        return;
    }
    if (hints.oldEnd != 0 && eax == hints.oldEnd) {
        shown = 0;
        tag = "slot";
        return;
    }
    if (hints.begin != 0 && eax >= hints.begin && eax < hints.end) {
        shown = 0;
        tag = "elem";
        return;
    }
    if (eax >= 0x10000u && eax < 0xF0000000u) {
        shown = 0;
        tag = "ptr";
        return;
    }
    shown = eax;
    tag = "raw";
}

struct Ai {
    std::uint8_t* self = nullptr;
    std::uint8_t* obj = nullptr;
    std::uint8_t* loco = nullptr;
    std::uint8_t* templ = nullptr;
    std::uint8_t* weapon = nullptr;
    std::uint8_t* pose = nullptr;
    Hints hints;
};

Ai makeAi(Arena& arena) {
    Ai ai;
    ai.self = arena.alloc(0x800);
    ai.obj = arena.alloc(0x500);
    ai.loco = arena.alloc(0x80);
    ai.templ = arena.alloc(0x120);
    ai.weapon = arena.alloc(0x40);
    ai.pose = arena.alloc(0x40);
    auto* locoVt = arena.alloc(0x40);
    putFn(locoVt, 0x1C, &v1c);
    putFn(locoVt, 0x34, &v34);
    putFn(locoVt, 0x38, &v38);
    putPtr(ai.loco, 0, locoVt);
    putPtr(ai.self, 8, ai.obj);
    putPtr(ai.self, 4, ai.templ);
    putPtr(ai.self, 0x3C, ai.loco);
    putPtr(ai.self, 0x214, ai.pose);
    putPtr(ai.obj, 4, ai.templ);
    putPtr(ai.obj, 0x37C, ai.weapon);
    putU32(ai.self, 0x50, 0x99);
    putU32(ai.self, 0x190, 0x55);
    putU32(ai.self, 0x308, 0x77);
    putU32(ai.self, 0x368, 0x77);
    putU32(ai.self, 0x3C8, 0x77);
    ai.self[0x432] = 7;
    putU32(ai.obj, 0x458, 0x99);
    putU32(ai.obj, 0x38, 0x111);
    putU32(ai.obj, 0x3C, 0x222);
    putU32(ai.obj, 0x40, 0x333);
    putU32(ai.templ, 0x104, 0xFFFFFFFFu);
    g_weapon = ai.weapon;
    ai.hints.weapon = static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(ai.weapon));
    ai.hints.templ = static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(ai.templ));
    ai.hints.object = static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(ai.obj));
    return ai;
}

std::uint8_t* makeXfer(Arena& arena, std::uint32_t flags) {
    auto* vt = arena.alloc(0x90);
    putFn(vt, 0x18, &v18);
    putFn(vt, 0x58, &v58);
    putFn(vt, 0x64, &v64);
    putFn(vt, 0x6C, &v6c);
    putFn(vt, 0x7C, &v7c);
    putFn(vt, 0x84, &v84);
    auto* xfer = arena.alloc(0x20);
    putPtr(xfer, 0, vt);
    putU32(xfer, 4, flags);
    return xfer;
}

struct Reply {
    std::uint32_t eax = 0;
    std::string extra;
};

Reply fromAi(const Ai& ai, std::uint32_t eax) {
    std::uint32_t shown = 0;
    std::string tag;
    classify(eax, ai.hints, shown, tag);
    Reply reply;
    reply.eax = shown;
    std::ostringstream text;
    text << "tag=" << tag << " b432=" << hex8(ai.self[0x432]) << " t458=" << hex8(loadU32(ai.obj + 0x458))
         << " p0=" << hex8(loadU32(ai.pose + 0xC)) << " p1=" << hex8(loadU32(ai.pose + 0x10))
         << " p2=" << hex8(loadU32(ai.pose + 0x14)) << " v44=" << hex8(ai.loco[0x44])
         << " v0=" << hex8(loadU32(ai.loco + 0x48)) << " v1=" << hex8(loadU32(ai.loco + 0x4C))
         << " v2=" << hex8(loadU32(ai.loco + 0x50)) << " w24=" << hex8(loadU32(ai.weapon + 0x24))
         << " fbits=" << hex8(g_fbits);
    reply.extra = text.str();
    return reply;
}

std::string memList(std::uint8_t* object, const std::vector<int>& offsets) {
    if (offsets.empty() || object == nullptr) {
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

void emit(const Reply& reply, std::uint8_t* object, const std::vector<int>& offsets) {
    std::cout << "eax=" << hex8(reply.eax) << " log=" << (g_log.empty() ? "." : g_log)
              << " mem=" << memList(object, offsets);
    if (!reply.extra.empty()) {
        std::cout << " " << reply.extra;
    }
    std::cout << "\n" << std::flush;
}

Reply tagged(std::uint32_t eax, const Hints& hints) {
    std::uint32_t shown = 0;
    std::string tag;
    classify(eax, hints, shown, tag);
    Reply reply;
    reply.eax = shown;
    reply.extra = "tag=" + tag;
    return reply;
}

void fldcw(unsigned short control) { __asm { fldcw control } }

const std::vector<int> kAiMem = {0x50, 0x190, 0x308, 0x368, 0x3C8};

}  // namespace

int main() {
    using namespace ra3::gamelogic;
    setObject72Calls(Object72Calls{
        usable, aimAt, findWeapon, findWeaponAlt, followCd10, followCe90, followCf30, followCdf0, markObject,
        prepareDed70, queryLoco, currentTarget, readPoint, queryD2120, queryA18a0, bindPair, noteAa2a0, placeAc4f0,
        steerB67a0, applyKey, triggerAbility, abilityReady, xferBase88, beamPulse, applyBeam, notifyBit, allocNode,
        growVector, xferRoot, xferAt10, xferStealth, resizeCrate, resizeStrafe, resizeStorage, xferShieldA,
        xferShieldB, xferShieldC, xferPower, stopMonitor, clearUpgrade, activateOne, weaponIdle, fireAt, xferDeploy,
        xferAbility, xferAi, xferBody, xferField, xferPair, xferRadarTail});
    fldcw(0x037F);

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
        auto arg = [&](std::size_t index) { return parseU32(args[index]); };
        Arena arena;
        g_arena = &arena;
        resetProbe();
        Reply reply;
        std::uint8_t* base = nullptr;
        if (mem.empty() && (cmd.rfind("aim", 0) == 0 || cmd.rfind("move", 0) == 0 || cmd.rfind("face", 0) == 0 ||
                            cmd.rfind("cmd", 0) == 0 || cmd.rfind("ord", 0) == 0 || cmd.rfind("guard", 0) == 0 ||
                            cmd.rfind("att", 0) == 0 || cmd.rfind("wf", 0) == 0 || cmd == "act" || cmd == "giveC" ||
                            cmd == "ocl")) {
            mem = kAiMem;
        }

        if (cmd == "aim2" || cmd == "aim4" || cmd == "aim5" || cmd == "aim3" || cmd == "aim2u" || cmd == "aim5u" ||
            cmd == "aim3w") {
            Ai ai = makeAi(arena);
            base = ai.self;
            std::uint32_t target = args.size() > 0 ? arg(0) : 0x20;
            std::uint32_t mode = args.size() > 1 ? arg(1) : 0;
            if (cmd == "aim2u") {
                g_usable = 0;
            }
            if (cmd == "aim5u") {
                g_usable = 0x100;
            }
            if (cmd == "aim3w") {
                putU32(ai.templ, 0xD4, 1u << 21);
                putU32(ai.obj, 0x328, 1);
                auto* node = arena.alloc(0x30);
                target = static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(node));
            }
            std::uint32_t eax = 0;
            if (cmd == "aim2" || cmd == "aim2u") {
                eax = AIUpdateInterface_aimState2(ai.self, target, mode);
            } else if (cmd == "aim4") {
                eax = AIUpdateInterface_aimState4(ai.self, target, mode);
            } else if (cmd == "aim5" || cmd == "aim5u") {
                eax = AIUpdateInterface_aimState5(ai.self, target, mode);
            } else {
                eax = AIUpdateInterface_aimState3(ai.self, target, mode);
            }
            reply = fromAi(ai, eax);
        } else if (cmd == "move1e" || cmd == "move35" || cmd == "move1c" || cmd == "move1d" || cmd == "move1eu" ||
                   cmd == "movevec") {
            Ai ai = makeAi(arena);
            base = ai.self;
            std::uint32_t pos = args.size() > 0 ? arg(0) : 0x1000;
            std::uint32_t mode = args.size() > 1 ? arg(1) : 0;
            std::uint32_t eax = 0;
            if (cmd == "move1eu") {
                g_usable = 0;
                eax = AIUpdateInterface_moveState1E(ai.self, pos, mode);
            } else if (cmd == "move35") {
                eax = AIUpdateInterface_moveState35(ai.self, pos, mode);
            } else if (cmd == "move1c") {
                eax = AIUpdateInterface_moveClear1C(ai.self, pos, mode);
            } else if (cmd == "move1d") {
                eax = AIUpdateInterface_moveClear1D(ai.self, pos, mode);
            } else if (cmd == "movevec") {
                std::uint32_t useVec = args.size() > 1 ? arg(1) : 0;
                std::uint32_t bits = args.size() > 2 ? arg(2) : 0x3F800000u;
                mode = args.size() > 3 ? arg(3) : 0;
                std::uint32_t vec = 0;
                if (useVec != 0) {
                    auto* bytes = arena.alloc(12);
                    putU32(bytes, 0, 0x44);
                    putU32(bytes, 4, 0x55);
                    putU32(bytes, 8, 0x66);
                    vec = static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(bytes));
                }
                eax = AIUpdateInterface_moveWithVector(ai.self, pos, vec, bits, mode);
            } else {
                eax = AIUpdateInterface_moveState1E(ai.self, pos, mode);
            }
            reply = fromAi(ai, eax);
        } else if (cmd == "moveto" || cmd == "movetoB" || cmd == "movetoU") {
            Ai ai = makeAi(arena);
            base = ai.self;
            if (cmd == "movetoB") {
                ai.self[0x436] = 1;
            }
            if (cmd == "movetoU") {
                g_usable = 0;
            }
            const std::uint32_t target = args.empty() ? 0x40 : arg(0);
            const std::uint32_t mode = args.size() > 1 ? arg(1) : 0;
            reply = fromAi(ai, AIUpdateInterface_moveToObject(ai.self, target, mode));
        } else if (cmd == "face") {
            Ai ai = makeAi(arena);
            base = ai.self;
            reply = fromAi(ai, AIUpdateInterface_faceObject(ai.self, arg(0), arg(1)));
        } else if (cmd == "cmd2f" || cmd == "cmd2fC" || cmd == "cmd2fB") {
            Ai ai = makeAi(arena);
            base = ai.self;
            g_query = 0x21;
            if (cmd == "cmd2fC") {
                g_cur = 0x55;
            }
            if (cmd == "cmd2fB") {
                ai.self[0x436] = 1;
            }
            const std::uint32_t target = args.empty() ? 0x40 : arg(0);
            const std::uint32_t mode = args.size() > 1 ? arg(1) : 0;
            reply = fromAi(ai, AIUpdateInterface_command2F(ai.self, target, mode));
        } else if (cmd == "ord2c" || cmd == "ord2c0" || cmd == "ord2cN" || cmd == "ord2cM") {
            Ai ai = makeAi(arena);
            base = ai.self;
            if (cmd == "ord2cN") {
                putPtr(ai.obj, 0x37C, nullptr);
            }
            if (cmd == "ord2cM") {
                g_findMiss = true;
            }
            const std::uint32_t target = cmd == "ord2c0" ? 0 : arg(0);
            const std::uint32_t mode = cmd == "ord2c0" ? 0 : arg(1);
            reply = fromAi(ai, AIUpdateInterface_order2C(ai.self, target, mode));
        } else if (cmd == "ord0b" || cmd == "ord0b0") {
            Ai ai = makeAi(arena);
            base = ai.self;
            const std::uint32_t target = cmd == "ord0b0" ? 0 : arg(0);
            const std::uint32_t aux = cmd == "ord0b0" ? 0 : arg(1);
            const std::uint32_t mode = cmd == "ord0b0" ? 0 : arg(2);
            reply = fromAi(ai, AIUpdateInterface_order0B(ai.self, target, aux, mode));
        } else if (cmd == "ord16") {
            Ai ai = makeAi(arena);
            base = ai.self;
            g_query = arg(3);
            reply = fromAi(ai, AIUpdateInterface_order16(ai.self, arg(0), arg(1), arg(2)));
        } else if (cmd == "aimf" || cmd == "aimfB") {
            Ai ai = makeAi(arena);
            base = ai.self;
            if (cmd == "aimfB") {
                ai.self[0x436] = 1;
            }
            const std::uint32_t target = args.size() > 0 ? arg(0) : 0x20;
            const std::uint32_t aux = args.size() > 1 ? arg(1) : 4;
            const std::uint32_t flag = args.size() > 2 ? arg(2) : 0;
            const std::uint32_t mode = args.size() > 3 ? arg(3) : 0;
            reply = fromAi(ai, AIUpdateInterface_aimFlagged(ai.self, target, aux, flag, mode));
        } else if (cmd == "guard" || cmd == "guardB" || cmd == "guardU") {
            Ai ai = makeAi(arena);
            base = ai.self;
            if (cmd == "guardB") {
                putU32(ai.templ, 0xC0, 1u << 26);
            }
            if (cmd == "guardU") {
                g_usable = 0;
            }
            const std::uint32_t point = args.empty() ? 0x30 : arg(0);
            const std::uint32_t mode = args.size() > 1 ? arg(1) : 0;
            reply = fromAi(ai, AIUpdateInterface_guardPoint(ai.self, point, mode));
        } else if (cmd == "att") {
            Ai ai = makeAi(arena);
            base = ai.self;
            const int path = args.size() > 2 ? static_cast<int>(arg(2)) : 0;
            if (path == 1) {
                auto* draw = arena.alloc(0x480);
                auto* nested = arena.alloc(0x10);
                putPtr(draw, 0x47C, nested);
                putPtr(ai.obj, 0x374, draw);
                g_d2120 = 0x61;
            } else if (path == 2) {
                g_a18a0 = 0x62;
            } else if (path == 3) {
                ai.obj[0x471] = 1;
            } else if (path == 4) {
                auto* draw = arena.alloc(0x480);
                putPtr(ai.obj, 0x374, draw);
                g_d2120 = 0;
                g_a18a0 = 0x62;
            }
            reply = fromAi(ai, AIUpdateInterface_attach60(ai.self, arg(0), arg(1)));
        } else if (cmd == "stSame" || cmd == "stFound" || cmd == "stApp" || cmd == "stZero" || cmd == "stGrow") {
            auto* slab = arena.alloc(0x200);
            auto* self = slab + 8;
            auto* object = arena.alloc(0x100);
            putPtr(slab, 0, object);
            base = self;
            Hints hints;
            hints.object = static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(object));
            const std::uint32_t id = arg(0);
            if (cmd == "stSame") {
                putU32(object, 0xDC, id);
            } else if (cmd == "stFound" || cmd == "stApp" || cmd == "stGrow") {
                auto* buf = arena.alloc(0x20);
                const auto begin = static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(buf));
                putU32(buf, 0, cmd == "stFound" ? id : 0x11);
                putPtr(self, 0xE0, buf);
                putU32(self, 0xE4, (cmd == "stFound" || cmd == "stApp") ? begin + 4 : begin);
                putU32(self, 0xE8, cmd == "stGrow" ? begin : begin + 16);
                hints.begin = begin;
                hints.end = loadU32(self + 0xE4);
                hints.oldEnd = hints.end;
            }
            reply = tagged(StancesBehavior_addStanceId(self, id), hints);
            const std::uint32_t begin = loadU32(self + 0xE0);
            const std::uint32_t end = loadU32(self + 0xE4);
            std::uint32_t count = 0;
            std::uint32_t first = 0;
            std::uint32_t endv = end < 0x10000u ? end : 0;
            if (begin >= 0x10000u && end >= begin && ((end - begin) % 4u) == 0 && (end - begin) / 4u <= 8u) {
                count = (end - begin) / 4u;
                if (count != 0) {
                    first = loadU32(reinterpret_cast<void*>(begin));
                }
            }
            reply.extra += " n=" + hex8(count) + " e0=" + hex8(first) + " endv=" + hex8(endv);
        } else if (cmd == "stHit" || cmd == "stNew") {
            auto* slab = arena.alloc(0x200);
            auto* self = slab + 8;
            auto* object = arena.alloc(0x100);
            putPtr(slab, 0, object);
            auto* head = self + 0xD4;
            putPtr(head, 0, head);
            putPtr(head, 4, head);
            base = self;
            const std::uint32_t id = arg(0);
            Hints hints;
            hints.object = static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(object));
            if (cmd == "stHit") {
                auto* node = arena.alloc(0x20);
                putPtr(node, 0, head);
                putPtr(node, 4, head);
                putU32(node, 8, id);
                putPtr(head, 0, node);
                putPtr(head, 4, node);
                hints.planted = static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(node));
            } else {
                putU32(object, 0x94, arg(1));
            }
            reply = tagged(StancesBehavior_linkStance(self, id), hints);
            reply.extra += " fl=" + hex8(loadU32(object + 0x94));
        } else if (cmd == "wfSet" || cmd == "wfClr") {
            Ai ai = makeAi(arena);
            base = ai.self;
            if (cmd == "wfSet") {
                putU32(ai.obj, 0xBC, 0x100);
            }
            reply = fromAi(ai, WeaponFireSpecialAbilityUpdate_templateBit8(ai.self));
            mem = kAiMem;
        } else if (cmd == "wfArm") {
            Ai ai = makeAi(arena);
            base = ai.self;
            putU32(ai.templ, 0x104, arg(0));
            reply = fromAi(ai, WeaponFireSpecialAbilityUpdate_armAndTrigger(ai.self));
        } else if (cmd == "mount") {
            Ai ai = makeAi(arena);
            base = ai.self;
            std::uint8_t* mounted = nullptr;
            if (arg(0) != 0) {
                mounted = arena.alloc(0x1C0);
                putPtr(ai.obj, 0xF4, mounted);
            }
            putU32(ai.obj, 0x88, arg(1));
            reply = tagged(ToggleMountedSpecialAbilityUpdate_clearMountedBit(ai.self, 1, 2), ai.hints);
            const std::uint32_t ubits = mounted != nullptr ? loadU32(mounted + 0x1BC) : 0;
            reply.extra += " m88=" + hex8(loadU32(ai.obj + 0x88)) + " ubits=" + hex8(ubits);
            mem = {0x88};
        } else if (cmd == "beam") {
            Ai ai = makeAi(arena);
            base = ai.self;
            ai.self[0xFA] = static_cast<std::uint8_t>(arg(0));
            putU32(ai.self, 0xFC, 0x99);
            putU32(ai.self, 0x100, 0x99);
            putU32(ai.self, 0x104, 0x99);
            putU32(ai.self, 0x108, 0x99);
            if (arg(0) != 0) {
                auto* draw = arena.alloc(0x40);
                auto* vt = arena.alloc(0x290);
                putFn(vt, 0x288, &v288);
                putPtr(draw, 0, vt);
                putPtr(ai.obj, 0x374, draw);
            }
            reply = tagged(BeamSpecialAbilityUpdate_resetBeam(ai.self, 1, 2), ai.hints);
            reply.extra += " fa=" + hex8(ai.self[0xFA]) + " b100=" + hex8(loadU32(ai.self + 0x100));
            mem = {0xFC, 0x104, 0x108};
        } else if (cmd == "act") {
            Ai ai = makeAi(arena);
            base = ai.self;
            reply = fromAi(ai, ActivateModuleSpecialAbilityUpdate_xferAndActivate(ai.self, arg(0), arg(1)));
        } else if (cmd == "giveC") {
            Ai ai = makeAi(arena);
            base = ai.self;
            reply = fromAi(ai, GiveUpgradeUpdate_clearThenXfer(ai.self, arg(0), arg(1)));
        } else if (cmd == "ocl") {
            Ai ai = makeAi(arena);
            base = ai.self;
            reply = fromAi(ai, OCLMonitorSpecialAbilityUpdate_stopThenXfer(ai.self, arg(0), arg(1)));
        } else if (cmd == "shield" || cmd == "radar" || cmd == "cap" || cmd == "comp" || cmd == "store" ||
                   cmd == "givex" || cmd == "deploy" || cmd == "horde" || cmd == "body" || cmd == "spawn" ||
                   cmd == "strafe" || cmd == "subst" || cmd == "stealth") {
            auto* self = arena.alloc(0x800);
            base = self;
            const std::uint32_t flags = args.empty() ? 0 : arg(0);
            if ((cmd == "shield" || cmd == "store") && flags == 1) {
                g_verOut = 1;
            }
            auto* xfer = makeXfer(arena, cmd == "shield" || cmd == "store" ? 0 : flags);
            Hints hints;
            std::uint32_t eax = 0;
            if (cmd == "shield") {
                eax = ShieldSphereManager_xfer(self, xfer);
                mem = {0x8};
            } else if (cmd == "radar") {
                eax = RadarScanMapSpecialAbilityUpdate_xfer(self, xfer);
                mem = {0xC8, 0xD8};
            } else if (cmd == "cap") {
                eax = CaptureSpecialAbilityUpdate_xfer(self, xfer);
            } else if (cmd == "comp") {
                eax = CompositeStructureInfoModule_xfer(self, xfer);
                mem = {0x18};
            } else if (cmd == "store") {
                putU32(self, 0x70, 0x99);
                eax = StoreObjectsSpecialPower_xfer(self, xfer);
                mem = {0x50, 0x70};
            } else if (cmd == "givex") {
                eax = GiveUpgradeUpdate_xfer(self, xfer);
            } else if (cmd == "deploy") {
                eax = DeployStyleAIUpdate_xfer(self, xfer);
            } else if (cmd == "horde") {
                eax = HordeAIUpdate_xfer(self, xfer);
            } else if (cmd == "body") {
                eax = ManagedActiveBody_xfer(self, xfer);
                mem = {0x124};
            } else if (cmd == "spawn" || cmd == "strafe" || cmd == "subst") {
                const std::uint32_t count = arg(1);
                std::uint8_t* buf = nullptr;
                if (count != 0) {
                    buf = arena.alloc(0x10);
                    putU32(buf, 0, 0x22);
                    putU32(buf, 4, 0x33);
                }
                const int beginOff = cmd == "spawn" ? 0x34 : cmd == "strafe" ? 0x14 : 0x10;
                if (buf != nullptr) {
                    putPtr(self, beginOff, buf);
                    putU32(self, beginOff + 4, static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(buf)) + 8);
                }
                if (cmd == "spawn") {
                    eax = SpawnCrateUpdate_xfer(self, xfer);
                } else if (cmd == "strafe") {
                    eax = StrafeAreaTargetChooser_xfer(self, xfer);
                } else {
                    eax = SubStructureStorageBehavior_xfer(self, xfer);
                }
                const std::uint32_t begin = loadU32(self + beginOff);
                const std::uint32_t el0 = begin >= 0x10000u ? loadU32(reinterpret_cast<void*>(begin)) : 0;
                reply = tagged(eax, hints);
                reply.extra += " el0=" + hex8(el0);
            } else {
                auto* head = self + 0x2C;
                putPtr(head, 0, head);
                putPtr(head, 4, head);
                if ((flags & 1u) != 0) {
                    auto* node = arena.alloc(0x20);
                    putPtr(node, 0, head);
                    putPtr(node, 4, head);
                    putU32(node, 8, 0x42);
                    putPtr(head, 0, node);
                    putPtr(head, 4, node);
                } else {
                    g_stream = static_cast<int>(arg(1));
                }
                eax = BroadcastStealthUpdate_xfer(self, xfer);
                std::uint32_t nlink = 0;
                for (auto* node = *reinterpret_cast<std::uint8_t**>(head); node != head && nlink < 8;
                     node = *reinterpret_cast<std::uint8_t**>(node)) {
                    ++nlink;
                }
                const std::uint32_t val = nlink != 0 ? loadU32(*reinterpret_cast<std::uint8_t**>(head + 4) + 8) : 0;
                reply = tagged(eax, hints);
                reply.extra += " val=" + hex8(val) + " nlink=" + hex8(nlink);
            }
            if (reply.extra.empty()) {
                reply = tagged(eax, hints);
            }
        } else if (cmd.rfind("dmg", 0) == 0) {
            auto* self = arena.alloc(0x80);
            auto* obj = arena.alloc(0x400);
            auto* templ = arena.alloc(0x40);
            auto* damage = arena.alloc(0xA0);
            auto* selector = arena.alloc(0x10);
            auto* readyVt = arena.alloc(0x10);
            auto* kindVt = arena.alloc(0x60);
            putFn(readyVt, 0, &v0);
            putFn(kindVt, 0x54, &v54);
            putPtr(self, 0x10, readyVt);
            putPtr(selector, 0, kindVt);
            putPtr(self, 8, obj);
            putPtr(self, 4, templ);
            putPtr(obj, 0x33C, selector);
            base = self;
            Hints hints;
            hints.templ = static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(templ));
            hints.object = static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(obj));
            putU32(templ, 0x24, 1);
            putU32(templ, 0x28, 0x3F800000u);
            putU32(damage, 0x8C, 0x40000000u);
            if (cmd == "dmg0") {
                g_ready = 0;
            } else if (cmd == "dmgHi") {
                g_ready = 0x100;
            } else if (cmd == "dmgMask") {
                putU32(templ, 0x24, 0);
            } else if (cmd == "dmgAbove") {
                putU32(templ, 0x28, 0x40000000u);
                putU32(damage, 0x8C, 0x3F800000u);
            } else if (cmd == "dmgNan") {
                putU32(templ, 0x28, 0x7FC00001u);
            } else if (cmd == "dmgIdle") {
                g_kind = 1;
                g_idle = 5;
                putPtr(self, 0x30, arena.alloc(0x10));
            } else if (cmd == "dmgFire") {
                g_kind = 2;
                putPtr(self, 0x34, arena.alloc(0x10));
            } else if (cmd == "dmgK3") {
                g_kind = 3;
                putPtr(self, 0x38, arena.alloc(0x10));
            } else if (cmd == "dmgDef") {
                g_kind = 4;
            } else if (cmd == "dmgGo") {
                g_kind = 4;
                putPtr(self, 0x2C, arena.alloc(0x10));
            }
            reply = tagged(FireWeaponWhenDamagedBehavior_onDamage(self, damage), hints);
        } else {
            reply.extra = "tag=err";
        }
        emit(reply, base, mem);
    }
    return 0;
}
